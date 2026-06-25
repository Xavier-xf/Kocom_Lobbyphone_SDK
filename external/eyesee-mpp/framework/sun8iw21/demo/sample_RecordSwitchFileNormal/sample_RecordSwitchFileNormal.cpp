//#define LOG_NDEBUG 0
#define LOG_TAG "sample_RecordSwitchFileNormal"
#include <utils/plat_log.h>

#include <iostream>
#include <csignal>

#include <hwdisplay.h>
#include <record_writer.h>
#include <confparser.h>
#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>

#include "sample_RecordSwitchFileNormal_config.h"
#include "sample_RecordSwitchFileNormal.h"

using namespace std;
using namespace EyeseeLinux;

SampleRecordSwitchFileNormalContext *pSampleRecordSwitchFileNormalContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if (pSampleRecordSwitchFileNormalContext!=NULL)
    {
        cdx_sem_up(&pSampleRecordSwitchFileNormalContext->mSemExit);
    }
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId == mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1])
            {
                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("fatal error! channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
            }
            break;
        }
        default:
        {
            aloge("fatal error! unknown info[0x%x] from channel[%d]", info, chnId);
            bHandleInfoFlag = false;
            break;
        }
    }
    return bHandleInfoFlag;
}

void EyeseeCameraListener::onPictureTaken(int chnId, const void *data, int size, EyeseeCamera *pCamera)
{
    aloge("fatal error! Do not test CallbackOut!");
}

EyeseeCameraListener::EyeseeCameraListener(SampleRecordSwitchFileNormalContext *pContext)
    : mpContext(pContext)
{
}

void EyeseeRecorderListener::onError(EyeseeRecorder *pMr, int what, int extra)
{
    alogd("receive onError message![%d][%d], recorderId[%d]", what, extra, pMr->mRecorderId);
    switch (what)
    {
        case EyeseeRecorder::MEDIA_ERROR_SERVER_DIED:
            break;
        default:
            break;
    }
}

void EyeseeRecorderListener::onInfo(EyeseeRecorder *pMr, int what, int extra)
{
    switch (what)
    {
        case EyeseeRecorder::MEDIA_RECORDER_INFO_NEED_SET_NEXT_FD:
        {
            alogd("receive onInfo message: need_set_next_fd, muxer_id[%d]", extra);
            int nMuxerId = extra;
            if (mpOwner->mpMainRecorder != pMr)
            {
                aloge("fatal error! why is not main recorder[%p]!=[%p]?", mpOwner->mpMainRecorder, pMr);
            }
            std::string strSegmentFilePath = mpOwner->mConfigPara.mSegmentFolderPath + '/' + mpOwner->MakeSegmentFileName(true);
            mpOwner->mpMainRecorder->setOutputFileSync((char*)strSegmentFilePath.c_str(), 0, nMuxerId);
            alogd("add file[%s] success", strSegmentFilePath.c_str());
            mpOwner->mMainSegmentFiles.push_back(strSegmentFilePath);
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            alogd("receive onInfo message: record_file_done, muxer_id[%d]", extra);
            int nMuxerId = extra;
            int ret;
            if (mpOwner->mpMainRecorder == pMr)
            {
                if (mpOwner->bSubEncodeEnable)
                {
                    //let subRecorder switch file normal!
                    std::string strSegmentFilePath = mpOwner->mConfigPara.mSegmentFolderPath + '/' + mpOwner->MakeSegmentFileName(false);
                    ret = mpOwner->mpSubRecorder->switchFileNormal((char*)strSegmentFilePath.c_str(), 0, mpOwner->mSubMuxerId);
                    if (NO_ERROR == ret)
                    {
                        AutoMutex autoLock(mpOwner->mFilesLock);
                        alogd("add file[%s] success", strSegmentFilePath.c_str());
                        mpOwner->mSubSegmentFiles.push_back(strSegmentFilePath);
                    }
                    else
                    {
                        aloge("fatal error! why switch sub file normal fail?[0x%x]", ret);
                    }
                }
                while (mpOwner->mMainSegmentFiles.size() > mpOwner->mConfigPara.mSegmentCount)
                {
                    if ((ret = remove(mpOwner->mMainSegmentFiles[0].c_str())) < 0)
                    {
                        aloge("fatal error! delete file[%s] failed:%s", mpOwner->mMainSegmentFiles[0].c_str(), strerror(errno));
                    }
                    else
                    {
                        alogd("delete file[%s] success, ret[0x%x]", mpOwner->mMainSegmentFiles[0].c_str(), ret);
                    }
                    mpOwner->mMainSegmentFiles.pop_front();
                }
            }
            else if (mpOwner->mpSubRecorder == pMr)
            {
                int ret;
                AutoMutex autoLock(mpOwner->mFilesLock);
                while (mpOwner->mSubSegmentFiles.size() > mpOwner->mConfigPara.mSegmentCount)
                {
                    if ((ret = remove(mpOwner->mSubSegmentFiles[0].c_str())) < 0)
                    {
                        aloge("fatal error! delete file[%s] failed:%s", mpOwner->mSubSegmentFiles[0].c_str(), strerror(errno));
                    }
                    else
                    {
                        alogd("delete file[%s] success, ret[0x%x]", mpOwner->mSubSegmentFiles[0].c_str(), ret);
                    }
                    mpOwner->mSubSegmentFiles.pop_front();
                }
            }
            else
            {
                aloge("fatal error! pMr[%p] is wrong!", pMr);
            }
            break;
        }
        default:
        {
            alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);
            break;
        }
    }
}

void EyeseeRecorderListener::onData(EyeseeRecorder *pMr, int what, int extra)
{
    aloge("fatal error! Do not test CallbackOut!");
}

EyeseeRecorderListener::EyeseeRecorderListener(SampleRecordSwitchFileNormalContext *pOwner)
    : mpOwner(pOwner)
{
}
SampleRecordSwitchFileNormalContext::SampleRecordSwitchFileNormalContext()
    :mCameraListener(this)
    ,mMainRecorderListener(this)
    ,mSubRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mpCamera = NULL;
    mpMainRecorder = NULL;
    mpSubRecorder = NULL;
    mMainFileNum = 0;
    mSubFileNum = 0;
    bzero(&mConfigPara,sizeof(mConfigPara));
    mMainMuxerId = -1;
    mMainVencId = -1;
    mSubMuxerId = -1;
    mSubVencId = -1;
}

SampleRecordSwitchFileNormalContext::~SampleRecordSwitchFileNormalContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    if(mpCamera!=NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
    if(mpMainRecorder!=NULL)
    {
        aloge("fatal error! main EyeseeRecorder is not destruct!");
    }
    if(mpSubRecorder!=NULL)
    {
        aloge("fatal error! sub EyeseeRecorder is not destruct!");
    }
}

status_t SampleRecordSwitchFileNormalContext::ParseCmdLine(int argc, char *argv[])
{
    alogd("this program path:[%s], arg number is [%d]", argv[0], argc);
    status_t ret = NO_ERROR;
    int i=1;
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                std::string errorString;
                errorString = "fatal error! use -h to learn how to set parameter!!!";
                cout<<errorString<<endl;
                ret = -1;
                break;
            }
            mCmdLinePara.mConfigFilePath = argv[i];
        }
        else if(!strcmp(argv[i], "-h"))
        {
            std::string helpString;
            helpString += "CmdLine param example:\n";
            helpString += "\t run -path /home/sample_RecordSwitchFileNormal.conf\n";
            cout<<helpString<<endl;
            ret = 1;
            break;
        }
        else
        {
            std::string ignoreString;
            ignoreString += "ignore invalid CmdLine param:[";
            ignoreString += argv[i];
            ignoreString += "], type -h to get how to set parameter!";
            cout<<ignoreString<<endl;
        }
        i++;
    }
    return ret;
}

status_t SampleRecordSwitchFileNormalContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mMainVippWidth = 1920;
        mConfigPara.mMainVippHeight = 1080;
        mConfigPara.mSubVippWidth = 640;
        mConfigPara.mSubVippHeight = 360;
        bSubVippEnable = true;
        mConfigPara.eMainPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.eSubPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.nDisplayWidth = 640;
        mConfigPara.nDisplayHeight = 360;
        bDisplayEnable = true;
        mConfigPara.mFrameRate = 30;
        mConfigPara.mEncodeType = PT_H264;
        mConfigPara.mAudioEncodeType = PT_MAX;
        mConfigPara.mMainEncodeWidth = 1920;
        mConfigPara.mMainEncodeHeight = 1080;
        mConfigPara.mMainEncodeBitrate = 6*1024*1024;
        mConfigPara.bMainEncodeOnline = false;
        mConfigPara.nMainEncodeOnlineSharebufnum = 2;
        mConfigPara.mSubEncodeWidth = 640;
        mConfigPara.mSubEncodeHeight = 360;
        mConfigPara.mSubEncodeBitrate = 2*1024*1024;
        bSubEncodeEnable = true;
        mConfigPara.mSegmentFolderPath = "/home/sample_RecordSwitchFileNormal_Files";
        mConfigPara.mSegmentCount = 3;
        mConfigPara.mSegmentMethod = SegmentByTime;
        mConfigPara.mSegmentDuration = 60;
        mConfigPara.mSegmentSize = 100*1024*1024;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mMainVippWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_VIPP_WIDTH, 0);
    mConfigPara.mMainVippHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_VIPP_HEIGHT, 0);
    mConfigPara.mSubVippWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_VIPP_WIDTH, 0);
    mConfigPara.mSubVippHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_VIPP_HEIGHT, 0);
    if ((mConfigPara.mSubVippWidth > 0) && (mConfigPara.mSubVippHeight > 0))
    {
        bSubVippEnable = true;
    }
    else
    {
        bSubVippEnable = false;
    }
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_PIC_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.eMainPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.eMainPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.eMainPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.eMainPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "lbc25x"))
    {
        mConfigPara.eMainPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if(!strcmp(pStrPixelFormat, "lbc20x"))
    {
        mConfigPara.eMainPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else
    {
        aloge("fatal error! conf file main pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.eMainPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_PIC_FORMAT, NULL);
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.eSubPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.eSubPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.eSubPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.eSubPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "lbc25x"))
    {
        mConfigPara.eSubPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if(!strcmp(pStrPixelFormat, "lbc20x"))
    {
        mConfigPara.eSubPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else
    {
        aloge("fatal error! conf file sub pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.eSubPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.nDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_DISPLAY_WIDTH, 0);
    mConfigPara.nDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_DISPLAY_HEIGHT, 0);
    if ((mConfigPara.nDisplayWidth > 0) && (mConfigPara.nDisplayHeight > 0))
    {
        bDisplayEnable = true;
    }
    else
    {
        bDisplayEnable = false;
    }
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_FRAME_RATE, 0);
    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_ENCODE_TYPE, NULL);
    if(!strcmp(pStrEncodeType, "h264"))
    {
        mConfigPara.mEncodeType = PT_H264;
    }
    else if(!strcmp(pStrEncodeType, "h265"))
    {
        mConfigPara.mEncodeType = PT_H265;
    }
    else if(!strcmp(pStrEncodeType, "mjpeg"))
    {
        mConfigPara.mEncodeType = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encode_type is [%s]?", pStrEncodeType);
        mConfigPara.mEncodeType = PT_H264;
    }
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_AUDIO_ENCODE_TYPE, NULL);
    if(pStrEncodeType!=NULL)
    {
        if(!strcmp(pStrEncodeType, "aac"))
        {
            mConfigPara.mAudioEncodeType = PT_AAC;
        }
        else if(!strcmp(pStrEncodeType, "mp3"))
        {
            mConfigPara.mAudioEncodeType = PT_MP3;
        }
        else if(!strcmp(pStrEncodeType, ""))
        {
            alogd("user set no audio.");
            mConfigPara.mAudioEncodeType = PT_MAX;
        }
        else
        {
            aloge("fatal error! conf file audio encode type is [%s]?", pStrEncodeType);
            mConfigPara.mAudioEncodeType = PT_MAX;
        }
    }
    else
    {
        aloge("fatal error! not find key audio_encode_type?");
        mConfigPara.mAudioEncodeType = PT_MAX;
    }
    mConfigPara.mMainEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_ENCODE_WIDTH, 0);
    mConfigPara.mMainEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_ENCODE_HEIGHT, 0);
    mConfigPara.mMainEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_ENCODE_BITRATE, 0)*1024*1024;
    mConfigPara.bMainEncodeOnline = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_ENCODE_ONLINE, 0);
    mConfigPara.nMainEncodeOnlineSharebufnum = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_MAIN_ENCODE_ONLINE_SHAREBUFNUM, 0);
    mConfigPara.mSubEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_ENCODE_WIDTH, 0);
    mConfigPara.mSubEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_ENCODE_HEIGHT, 0);
    if ((mConfigPara.mSubEncodeWidth > 0) && (mConfigPara.mSubEncodeHeight > 0))
    {
        bSubEncodeEnable = true;
    }
    else
    {
        bSubEncodeEnable = false;
    }
    mConfigPara.mSubEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SUB_ENCODE_BITRATE, 0)*1024*1024;

    mConfigPara.mSegmentFolderPath = GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_FOLDER, NULL);
    mConfigPara.mSegmentCount = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_COUNT, 0);
    char *pStrSegmentMethod = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_METHOD, NULL);
    if(!strcmp(pStrSegmentMethod, "time"))
    {
        mConfigPara.mSegmentMethod = SegmentByTime;
    }
    else if(!strcmp(pStrSegmentMethod, "size"))
    {
        mConfigPara.mSegmentMethod = SegmentBySize;
    }
    else
    {
        aloge("fatal error! conf file segment method is [%s]?", pStrSegmentMethod);
        mConfigPara.mSegmentMethod = SegmentByTime;
    }
    mConfigPara.mSegmentDuration = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_DURATION, 0);
    mConfigPara.mSegmentSize = GetConfParaInt(&stConfParser, SAMPLE_RECORDSWITCHFILENORMAL_KEY_SEGMENT_SIZE, 0)*1024*1024;

    destroyConfParser(&stConfParser);
    if (false == bSubVippEnable)
    {
        bDisplayEnable = false;
        bSubEncodeEnable = false;
    }
    return SUCCESS;
}

status_t SampleRecordSwitchFileNormalContext::CreateFolder(const std::string& strFolderPath)
{
    if(strFolderPath.empty())
    {
        aloge("jpeg path is not set!");
        return UNKNOWN_ERROR;
    }
    const char* pFolderPath = strFolderPath.c_str();
    //check folder existence
    struct stat sb;
    if (stat(pFolderPath, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pFolderPath, sb.st_mode);
            return UNKNOWN_ERROR;
        }
    }
    //create folder if necessary
    int ret = mkdir(pFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pFolderPath);
        return UNKNOWN_ERROR;
    }
}

std::string SampleRecordSwitchFileNormalContext::MakeSegmentFileName(bool bMainFile)
{
    char fileName[64];
    if (bMainFile)
    {
        sprintf(fileName, "File%s[%04d]_main.mp4", mConfigPara.mSegmentMethod==SegmentByTime?"ByTime":"BySize", mMainFileNum++);
    }
    else
    {
        sprintf(fileName, "File%s[%04d]_sub.mp4", mConfigPara.mSegmentMethod==SegmentByTime?"ByTime":"BySize", mSubFileNum++);
    }
    return std::string(fileName);
}

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_RecordSwitchFileNormal!"<<endl;
    GLogConfig stGLogConfig =
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_WARN,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);
    SampleRecordSwitchFileNormalContext stContext;
    pSampleRecordSwitchFileNormalContext = &stContext;  //global var

    // parse command line param
    if (stContext.ParseCmdLine(argc, argv) != NO_ERROR)
    {
        aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        return result;
    }

    // parse config file.
    if (stContext.loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }

    // check folder existence, create folder if necessary
    if (SUCCESS != stContext.CreateFolder(stContext.mConfigPara.mSegmentFolderPath))
    {
        aloge("fatal error! Create output path %s fail", stContext.mConfigPara.mSegmentFolderPath);
        return -1;
    }

    // register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    // init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //config camera.
    {
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
        CameraInfo cameraInfo;
        ISPGeometry mISPGeometry;
        VI_CHN chn;
        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 1;
        mISPGeometry.mISPDev = 0;
        mISPGeometry.mScalerOutChns.push_back(HVIDEO(0,0));
        if (stContext.bSubVippEnable)
        {
            mISPGeometry.mScalerOutChns.push_back(HVIDEO(1,0));
        }
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(mISPGeometry);
        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

    int cameraId;
    CameraInfo& cameraInfo = stContext.mCameraInfo;
    cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &cameraInfo);
    stContext.mpCamera = EyeseeCamera::open(cameraId);
    stContext.mpCamera->setInfoCallback(&stContext.mCameraListener);
    stContext.mpCamera->prepareDevice();
    stContext.mpCamera->startDevice();

    stContext.mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], true);
    CameraParameters cameraParam;
    stContext.mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    SIZE_S captureSize;
    captureSize.Width = stContext.mConfigPara.mMainVippWidth;
    captureSize.Height = stContext.mConfigPara.mMainVippHeight;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.eMainPicFormat);
    if (false == stContext.mConfigPara.bMainEncodeOnline)
    {
        cameraParam.setVideoBufferNumber(5);
    }
    else
    {
        cameraParam.setOnlineEnable(true);
        cameraParam.setOnlineShareBufNum(stContext.mConfigPara.nMainEncodeOnlineSharebufnum);
    }
    stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

    if (stContext.bSubVippEnable)
    {
        stContext.mpCamera->openChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], false);
        stContext.mpCamera->getParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
        captureSize.Width = stContext.mConfigPara.mSubVippWidth;
        captureSize.Height = stContext.mConfigPara.mSubVippHeight;
        cameraParam.setVideoSize(captureSize);
        cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
        cameraParam.setPreviewFormat(stContext.mConfigPara.eSubPicFormat);
        cameraParam.setVideoBufferNumber(5);
        stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], cameraParam);
        stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    }

    if (stContext.bDisplayEnable)
    {
        stContext.mVoDev = 0;
        AW_MPI_VO_Enable(stContext.mVoDev);
        AW_MPI_VO_AddOutsideVideoLayer(stContext.mUILayer);
        AW_MPI_VO_CloseVideoLayer(stContext.mUILayer);//close ui layer.
        VO_PUB_ATTR_S spPubAttr;
        AW_MPI_VO_GetPubAttr(stContext.mVoDev, &spPubAttr);
        spPubAttr.enIntfType = VO_INTF_LCD;
        spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
        AW_MPI_VO_SetPubAttr(stContext.mVoDev, &spPubAttr);
        VO_LAYER hlay = 0;
        while(hlay < VO_MAX_LAYER_NUM)
        {
            if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay))
            {
                break;
            }
            hlay++;
        }
        if(hlay >= VO_MAX_LAYER_NUM)
        {
            aloge("fatal error! enable video layer fail!");
        }
        AW_MPI_VO_GetVideoLayerAttr(hlay, &stContext.mLayerAttr);
        stContext.mLayerAttr.stDispRect.X      = 0;
        stContext.mLayerAttr.stDispRect.Y      = 0;
        stContext.mLayerAttr.stDispRect.Width  = stContext.mConfigPara.nDisplayWidth;
        stContext.mLayerAttr.stDispRect.Height = stContext.mConfigPara.nDisplayHeight;
        AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
        stContext.mVoLayer = hlay;
        //camera preview test
        alogd("prepare setPreviewDisplay(), hlay=%d", stContext.mVoLayer);
        stContext.mpCamera->setChannelDisplay(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1], stContext.mVoLayer);
    }

    if (stContext.bSubVippEnable)
    {
        stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    }
    stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    if (stContext.bDisplayEnable)
    {
        int ret = cdx_sem_down_timedwait(&stContext.mSemRenderStart, 5000);
        if (0 == ret)
        {
            alogd("app receive message that camera start render!");
        }
        else if(ETIMEDOUT == ret)
        {
            aloge("fatal error! wait render start timeout");
        }
        else
        {
            aloge("fatal error! other error[0x%x]", ret);
        }
    }

    // prepare to record main file.
    stContext.mpMainRecorder = new EyeseeRecorder();
    stContext.mpMainRecorder->setOnInfoListener(&stContext.mMainRecorderListener);
    stContext.mpMainRecorder->setOnDataListener(&stContext.mMainRecorderListener);
    stContext.mpMainRecorder->setOnErrorListener(&stContext.mMainRecorderListener);
    stContext.mpMainRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    stContext.mpMainRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);

    SinkParam stSinkParam;
    //memset(&stSinkParam, 0, sizeof(SinkParam));
    stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_MP4;
    stSinkParam.mOutputFd = -1;
    std::string strSegmentFilePath = stContext.mConfigPara.mSegmentFolderPath + '/' + stContext.MakeSegmentFileName(true);
    stSinkParam.mOutputPath = (char*)strSegmentFilePath.c_str();
    stSinkParam.mFallocateLen = 0;
    stSinkParam.mMaxDurationMs = 0;
    stSinkParam.bCallbackOutFlag  = false;
    stSinkParam.bBufFromCacheFlag = false;
    stContext.mMainMuxerId = stContext.mpMainRecorder->addOutputSink(&stSinkParam);
    stContext.mMainSegmentFiles.push_back(strSegmentFilePath);
    VencParameters stVencParam;

    VENC_CHN_ATTR_S stVencChnAttr;
    memset(&stVencChnAttr, 0, sizeof(stVencChnAttr));
    VENC_RC_PARAM_S stVencRCParam;
    memset(&stVencRCParam, 0, sizeof(stVencRCParam));
    stVencChnAttr.VeAttr.Type = stContext.mConfigPara.mEncodeType;
    switch(stVencChnAttr.VeAttr.Type)
    {
        case PT_H264:
        {
            stVencChnAttr.VeAttr.AttrH264e.Profile = 1;
            stVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
            stVencChnAttr.VeAttr.AttrH264e.PicWidth = stContext.mConfigPara.mMainEncodeWidth;
            stVencChnAttr.VeAttr.AttrH264e.PicHeight = stContext.mConfigPara.mMainEncodeHeight;
            stVencChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
            stVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = FALSE;
            stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            stVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = stContext.mConfigPara.mMainEncodeBitrate;
            //stVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = ;
            stVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = stContext.mConfigPara.mFrameRate;
            stVencRCParam.ParamH264Cbr.mMaxQp = 51;
            stVencRCParam.ParamH264Cbr.mMinQp = 1;
            stVencRCParam.ParamH264Cbr.mMaxPqp = 50;
            stVencRCParam.ParamH264Cbr.mMinPqp = 10;
            stVencRCParam.ParamH264Cbr.mQpInit = 30;
            stVencRCParam.ParamH264Cbr.mbEnMbQpLimit = 0;
            break;
        }
        case PT_H265:
        {
            stVencChnAttr.VeAttr.AttrH265e.mProfile = 0;
            stVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
            stVencChnAttr.VeAttr.AttrH265e.mPicWidth = stContext.mConfigPara.mMainEncodeWidth;
            stVencChnAttr.VeAttr.AttrH265e.mPicHeight = stContext.mConfigPara.mMainEncodeHeight;
            stVencChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
            stVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = FALSE;
            stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            stVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = stContext.mConfigPara.mMainEncodeBitrate;
            //stVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = ;
            stVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = stContext.mConfigPara.mFrameRate;
            stVencRCParam.ParamH265Cbr.mMaxQp = 51;
            stVencRCParam.ParamH265Cbr.mMinQp = 1;
            stVencRCParam.ParamH265Cbr.mMaxPqp = 50;
            stVencRCParam.ParamH265Cbr.mMinPqp = 10;
            stVencRCParam.ParamH265Cbr.mQpInit = 30;
            stVencRCParam.ParamH265Cbr.mbEnMbQpLimit = 0;
            break;
        }
        case PT_MJPEG:
        {
            stVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
            stVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = stContext.mConfigPara.mMainEncodeWidth;
            stVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = stContext.mConfigPara.mMainEncodeHeight;
            stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = stContext.mConfigPara.mMainEncodeBitrate;
            //stVencChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = ;
            stVencChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = stContext.mConfigPara.mFrameRate;
            break;
        }
        default:
        {
            aloge("fatal error! wrong video encode type:%d", stVencChnAttr.VeAttr.Type);
            break;
        }
    }
    stVencChnAttr.VeAttr.mOnlineEnable = stContext.mConfigPara.bMainEncodeOnline;
    stVencChnAttr.VeAttr.mOnlineShareBufNum = stContext.mConfigPara.nMainEncodeOnlineSharebufnum;
    stVencChnAttr.VeAttr.mVeRecRefBufReduceEnable=0;
    stVencChnAttr.VeAttr.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_2_5X;
    stVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    stVencParam.setVencChnAttr(stVencChnAttr);
    stVencParam.setVencRcParam(stVencRCParam);
    stContext.mMainVencId = 0;
    stContext.mpMainRecorder->setVencParameters(stContext.mMainVencId, &stVencParam);
    alogd("CameraSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpMainRecorder->setCameraProxy(stContext.mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpMainRecorder->bindVeVipp(stContext.mMainVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    if (stContext.mConfigPara.mAudioEncodeType != PT_MAX)
    {
        stContext.mpMainRecorder->setAudioSamplingRate(8000);
        stContext.mpMainRecorder->setAudioChannels(1);
        stContext.mpMainRecorder->setAudioEncodingBitRate(12200);
        stContext.mpMainRecorder->setAudioEncoder(PT_AAC);
    }
    if (SegmentByTime == stContext.mConfigPara.mSegmentMethod)
    {
        stContext.mpMainRecorder->setMaxDuration(stContext.mConfigPara.mSegmentDuration*1000);
    }
    else
    {
        stContext.mpMainRecorder->setMaxFileSize(stContext.mConfigPara.mSegmentSize);
    }
    alogd("prepare()!");
    stContext.mpMainRecorder->prepare();

    if (stContext.bSubEncodeEnable)
    {
        //prepare to record sub file.
        stContext.mpSubRecorder = new EyeseeRecorder();
        stContext.mpSubRecorder->setOnInfoListener(&stContext.mSubRecorderListener);
        stContext.mpSubRecorder->setOnDataListener(&stContext.mSubRecorderListener);
        stContext.mpSubRecorder->setOnErrorListener(&stContext.mSubRecorderListener);
        stContext.mpSubRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
        stContext.mpSubRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
        //memset(&stSinkParam, 0, sizeof(SinkParam));
        stSinkParam.Reset();
        stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_MP4;
        stSinkParam.mOutputFd = -1;
        strSegmentFilePath = stContext.mConfigPara.mSegmentFolderPath + '/' + stContext.MakeSegmentFileName(false);
        stSinkParam.mOutputPath = (char*)strSegmentFilePath.c_str();
        stSinkParam.mFallocateLen = 0;
        stSinkParam.mMaxDurationMs = 0;
        stSinkParam.bCallbackOutFlag  = false;
        stSinkParam.bBufFromCacheFlag = false;
        stContext.mSubMuxerId = stContext.mpSubRecorder->addOutputSink(&stSinkParam);
        stContext.mSubSegmentFiles.push_back(strSegmentFilePath);
        alogd("setSubEncodeVideoSize=[%dx%d]", stContext.mConfigPara.mSubEncodeWidth, stContext.mConfigPara.mSubEncodeHeight);
        SIZE_S stVideoSize;
        stVideoSize.Width = stContext.mConfigPara.mSubEncodeWidth;
        stVideoSize.Height = stContext.mConfigPara.mSubEncodeHeight;
        VENC_CHN_ATTR_S stSubVencChnAttr = stVencParam.getVencChnAttr();
        stSubVencChnAttr.VeAttr.mOnlineEnable = 0;
        stVencParam.setVencChnAttr(stSubVencChnAttr);
        stVencParam.setVideoSize(stVideoSize);
        stVencParam.setVideoEncodingBitRate(stContext.mConfigPara.mSubEncodeBitrate);
        stContext.mSubVencId = 0;
        stContext.mpSubRecorder->setVencParameters(stContext.mSubVencId, &stVencParam);
        alogd("CameraSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        stContext.mpSubRecorder->setCameraProxy(stContext.mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        stContext.mpSubRecorder->bindVeVipp(stContext.mSubVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        if (stContext.mConfigPara.mAudioEncodeType != PT_MAX)
        {
            stContext.mpSubRecorder->setAudioSamplingRate(8000);
            stContext.mpSubRecorder->setAudioChannels(1);
            stContext.mpSubRecorder->setAudioEncodingBitRate(12200);
            stContext.mpSubRecorder->setAudioEncoder(PT_AAC);
        }
        alogd("prepare()!");
        stContext.mpSubRecorder->prepare();
    }

    alogd("start()!");
    stContext.mpMainRecorder->start();
    if (stContext.bSubEncodeEnable)
    {
        stContext.mpSubRecorder->start();
    }

    cdx_sem_down(&stContext.mSemExit);
    alogd("record done! stop()!");

    //stop recorder
    stContext.mpMainRecorder->stop();
    if (stContext.bSubEncodeEnable)
    {
        stContext.mpSubRecorder->stop();
    }

    delete stContext.mpMainRecorder;
    stContext.mpMainRecorder = NULL;
    if (stContext.bSubEncodeEnable)
    {
        delete stContext.mpSubRecorder;
        stContext.mpSubRecorder = NULL;
    }

    // close camera
    alogd("EyeseeCamera::release()");
    alogd("EyeseeCamera stopPreview()");
    stContext.mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    if (stContext.bSubVippEnable)
    {
        stContext.mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        stContext.mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
        stContext.mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[1]);
    }
    stContext.mpCamera->stopDevice();
    stContext.mpCamera->releaseDevice();
    EyeseeCamera::close(stContext.mpCamera);
    stContext.mpCamera = NULL;

    if (stContext.bDisplayEnable)
    {
        // close vo
        AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
        stContext.mVoLayer = -1;
        AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
        AW_MPI_VO_Disable(stContext.mVoDev);
        stContext.mVoDev = -1;
    }

    // exit mpp system
    AW_MPI_SYS_Exit();

    log_quit();
    cout<<"bye, sample_RecordSwitchFileNormal!"<<endl;
    return result;
}
