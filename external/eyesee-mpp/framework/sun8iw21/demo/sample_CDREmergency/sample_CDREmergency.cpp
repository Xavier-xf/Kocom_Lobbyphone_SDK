//#define LOG_NDEBUG 0
#define LOG_TAG "sample_CDREmergency"
#include <utils/plat_log.h>

#include <iostream>
#include <csignal>

#include <hwdisplay.h>
#include <record_writer.h>
#include <confparser.h>
#include <SystemBase.h>

#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>

#include "sample_CDREmergency_config.h"
#include "sample_CDREmergency.h"

using namespace std;
using namespace EyeseeLinux;

#define SAMPLE_RECODER_1_MODE  (1)
//#define SAMPLE_RECODER_2_MODE  (2)
//#define SAMPLE_RECODER_3_MODE  (3)

#define SAMPLE_RECODER_MODE   SAMPLE_RECODER_1_MODE   // set test mode

static SampleCDREmergencyContext *pSampleCDREmergencyContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(pSampleCDREmergencyContext!=NULL)
    {
        cdx_sem_up(&pSampleCDREmergencyContext->mSemExit);
    }
}

bool EyeseeCameraListener::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId == mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0])
            {
                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("fatal error! channel[%d] notify render start, but channel[%d] wait render start!", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
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
    int ret = -1;
    alogd("channel %d picture data size %d", chnId, size);
    if(chnId != mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0])
    {
        aloge("fatal error! channel[%d] is not match current channel[%d]", chnId, mpContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    }
    char picName[64];
    sprintf(picName, "pic[%02d].jpg", mpContext->mPicNum++);
    std::string PicFullPath = mpContext->mConfigPara.mFolderPath + '/' + picName;
    FILE *fp = fopen(PicFullPath.c_str(), "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
}

EyeseeCameraListener::EyeseeCameraListener(SampleCDREmergencyContext *pContext)
    : mpContext(pContext)
{
}

void EyeseeRecorderListener::onError(EyeseeRecorder *pMr, int what, int extra)
{
    alogd("receive onError message![%d][%d]", what, extra);
    switch(what)
    {
        case EyeseeRecorder::MEDIA_ERROR_SERVER_DIED:
            break;
        default:
            break;
    }
}

void EyeseeRecorderListener::onInfo(EyeseeRecorder *pMr, int what, int extra)
{
    switch(what)
    {
        case EyeseeRecorder::MEDIA_RECORDER_INFO_NEED_SET_NEXT_FD:
        {
            alogd("receive onInfo message: need_set_next_fd, muxer_id[%d]", extra);
            int nMuxerId = extra;
            if(mpOwner->mMuxerId == nMuxerId)
            {
                std::string strFilePath = mpOwner->mConfigPara.mFolderPath + '/' + mpOwner->MakeFileName();
                mpOwner->mpRecorder->setOutputFileSync((char*)strFilePath.c_str(), 0, nMuxerId);
                mpOwner->mFiles.push_back(strFilePath);
            }
            else
            {
                alogd("onInfo: need_set_next_fd muxerId is %d, maybe impact muxerId[%d], don't set next fd to it", nMuxerId, mpOwner->mMuxerIdEmergency);
            }
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE:
        {
            int ret;
            int nMuxerId = extra;
            alogd("receive onInfo message: record_file_done,  muxer_id[%d]", nMuxerId);
            bool bCommonMuxerFlag = true;
            if(ImpactStyle::AddFile == mpOwner->mConfigPara.mImpactStyle)
            {
                std::lock_guard<std::mutex> autoLock(mpOwner->mSwitchImpactLock);
                if(mpOwner->mMuxerIdEmergency == nMuxerId)
                {
                    alogd("remove Emergency_MuxerId[%d]",mpOwner->mMuxerIdEmergency);
                    ret = mpOwner->mpRecorder->removeOutputSink(mpOwner->mMuxerIdEmergency);
                    //alogd("remove Emergency_MuxerId[%d] done, ret:0x%x",mpOwner->mMuxerIdEmergency, ret);
                    if(ret != NO_ERROR)
                    {
                        aloge("fatal error! why impact muxerId[%d] is removed fail[0x%x]?", mpOwner->mMuxerIdEmergency, ret);
                    }
                    mpOwner->mMuxerIdEmergency = -1;
                    if(true == mpOwner->mbMuxImpactFlag)
                    {
                        mpOwner->mbMuxImpactFlag = false;
                    }
                    bCommonMuxerFlag = false;
                }
            }
            else if(ImpactStyle::SwitchFile == mpOwner->mConfigPara.mImpactStyle)
            {
                std::lock_guard<std::mutex> autoLock(mpOwner->mSwitchImpactLock);
                if(nMuxerId != mpOwner->mMuxerId)
                {
                    aloge("fatal error! recorder[%d] file done sync, muxerId[%d!=%d]", mpOwner->mpRecorder->mRecorderId, nMuxerId,
                        mpOwner->mMuxerId);
                }
                if(true == mpOwner->mbMuxImpactFlag)
                {
                    alogd("recorder[%d] impact switch_file style: muxerId[%d] impact file is done, restore file duration to %ds",
                        mpOwner->mpRecorder->mRecorderId, mpOwner->mMuxerId, mpOwner->mConfigPara.mDuration);
                    ret = mpOwner->mpRecorder->setMaxDuration(nMuxerId, mpOwner->mConfigPara.mDuration*1000);
                    if (ret != NO_ERROR)
                    {
                        aloge("fatal error! recorder[%d] muxerId[%d] impact done, restore duration fail:0x%x", mpOwner->mpRecorder->mRecorderId,
                            nMuxerId, ret);
                    }
                    mpOwner->mbMuxImpactFlag = false;
                }
            }
            if(bCommonMuxerFlag)
            {
                if(mpOwner->mMuxerId != nMuxerId)
                {
                    aloge("fatal error! common muxerId[%d!=%d]", mpOwner->mMuxerId, nMuxerId);
                }
                while(mpOwner->mFiles.size() > mpOwner->mConfigPara.mCount)
                {
                    if ((ret = remove(mpOwner->mFiles[0].c_str())) < 0) 
                    {
                        aloge("fatal error! delete file[%s] failed:%s", mpOwner->mFiles[0].c_str(), strerror(errno));
                    }
                    else
                    {
                        alogd("delete file[%s] success, ret[0x%x]", mpOwner->mFiles[0].c_str(), ret);
                    }
                    mpOwner->mFiles.pop_front();
                }
            }
            break;
        }
        case EyeseeRecorder::MEDIA_RECORDER_INFO_RECORD_FILE_DONE_SYNC:
        {
            int nMuxerId = extra;
            break;
        }
        default:
            alogd("receive onInfo message! media_info_type[%d] extra[%d]", what, extra);
            break;
    }
}

void EyeseeRecorderListener::onData(EyeseeRecorder *pMr, int what, int extra)
{
    aloge("fatal error! Do not test CallbackOut!");
}

EyeseeRecorderListener::EyeseeRecorderListener(SampleCDREmergencyContext *pOwner)
    : mpOwner(pOwner)
{
}
SampleCDREmergencyContext::SampleCDREmergencyContext()
    :mCameraListener(this)
    ,mRecorderListener(this)
{
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    mUILayer = HLAY(2, 0);
    mpCamera = NULL;
    mpRecorder = NULL;
    mFileNum = 0;
    mPicNum = 0;
    mMuxerIdEmergency = -1;
    bzero(&mConfigPara,sizeof(mConfigPara));
    mMuxerId = -1;
    mbMuxImpactFlag = false;
    mImpactTm = -1;
}

SampleCDREmergencyContext::~SampleCDREmergencyContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    if(mpCamera!=NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
    if(mpRecorder!=NULL)
    {
        aloge("fatal error! EyeseeRecorder is not destruct!");
    }
}

status_t SampleCDREmergencyContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /home/sample_Recorder.conf\n";
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
status_t SampleCDREmergencyContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.mCaptureWidth = 1920;
        mConfigPara.mCaptureHeight = 1080;
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        mConfigPara.mFrameRate = 25;
        mConfigPara.mEncodeType = PT_H264;
        mConfigPara.mAudioEncodeType = PT_MAX;
        mConfigPara.mEncodeWidth = 1920;
        mConfigPara.mEncodeHeight = 1080;
        mConfigPara.mEncodeBitrate = 14*1024*1024;
        mConfigPara.mFolderPath = "/mnt/extsd/sample_CDREmergency_Files";
        mConfigPara.mCount = 5;
        mConfigPara.mMethod = ByTime;
        mConfigPara.mDuration = 10;
        mConfigPara.mSize = 100*1024*1024;
        mConfigPara.mFirstEmergencyTime = 10;
        mConfigPara.mSecondEmergencyTime = 0;
        mConfigPara.mImpactDuration = 15;
        mConfigPara.mImpactStyle = ImpactStyle::AddFile;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_CAPTURE_WIDTH, 0); 
    mConfigPara.mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_CAPTURE_HEIGHT, 0); 
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDER_KEY_PIC_FORMAT, NULL); 
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        mConfigPara.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    mConfigPara.mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_FRAME_RATE, 0);
    mConfigPara.mDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_DISPLAY_WIDTH, 0);
    mConfigPara.mDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_DISPLAY_HEIGHT, 0);
    char *pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDER_KEY_ENCODE_TYPE, NULL); 
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
    pStrEncodeType = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDER_KEY_AUDIO_ENCODE_TYPE, NULL); 
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
    mConfigPara.mEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_ENCODE_WIDTH, 0);
    mConfigPara.mEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_ENCODE_HEIGHT, 0);
    mConfigPara.mEncodeBitrate = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_ENCODE_BITRATE, 0)*1024*1024;
    mConfigPara.mEncodeFrameRate = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_ENCODE_FRAME_RATE, 0);

    mConfigPara.mFolderPath = GetConfParaString(&stConfParser, SAMPLE_RECORDER_KEY_FOLDER, NULL);
    mConfigPara.mCount = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_COUNT, 0);
    char *pStrMethod = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDER_KEY_METHOD, NULL); 
    if(!strcmp(pStrMethod, "time"))
    {
        mConfigPara.mMethod = ByTime;
    }
    else if(!strcmp(pStrMethod, "size"))
    {
        mConfigPara.mMethod = BySize;
    }
    else
    {
        aloge("fatal error! conf file  method is [%s]?", pStrMethod);
        mConfigPara.mMethod = ByTime;
    }
    mConfigPara.mDuration = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_DURATION, 0);
    mConfigPara.mSize = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_SIZE, 0)*1024*1024;
    mConfigPara.mMuxCacheDuration  = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_MUX_CACHE_DURATION, 0);
    mConfigPara.mFirstEmergencyTime  = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_FIRST_EMERGENCY_TIME, 0);
    mConfigPara.mSecondEmergencyTime = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_SECOND_EMERGENCY_TIME, 0);
    mConfigPara.mImpactDuration = GetConfParaInt(&stConfParser, SAMPLE_RECORDER_KEY_IMPACT_DURATION, 0);
    char *pStrImpactStyle = (char*)GetConfParaString(&stConfParser, SAMPLE_RECORDER_KEY_IMPACT_STYLE, NULL); 
    if(!strcmp(pStrImpactStyle, "add_file"))
    {
        mConfigPara.mImpactStyle = ImpactStyle::AddFile;
    }
    else if(!strcmp(pStrImpactStyle, "switch_file"))
    {
        mConfigPara.mImpactStyle = ImpactStyle::SwitchFile;
    }
    else
    {
        aloge("fatal error! impact style[%s] wrong?", pStrImpactStyle);
        mConfigPara.mImpactStyle = ImpactStyle::AddFile;
    }

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleCDREmergencyContext::CreateFolder(const std::string& strFolderPath)
{
    if(strFolderPath.empty())
    {
        aloge("path is not set!");
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

std::string SampleCDREmergencyContext::MakeFileName()
{
    char fileName[64];
    sprintf(fileName, "File%s[%04d].mp4", mConfigPara.mMethod==ByTime?"ByTime":"BySize", mFileNum++);
    return std::string(fileName);
}

/*status_t SampleCDREmergencyContext::setVideoFrameRateAndIFramesNumberInterval(SampleRecorderContext *stContext, int fps)
{
    if (fps < 1)
    {
        aloge("fatal error! the fps %d is invalid!",fps);
    }
    alogd("setVideoFrameRateAndIFramesNumberInterval %d",fps);
    stContext->mpRecorder->setVideoFrameRate(fps);
    stContext->mpRecorder->setVideoEncodingIFramesNumberInterval(fps);
    stContext->mpRecorder->reencodeIFrame();
    return 0;
}*/

int main(int argc, char *argv[])
{
    int result = 0;
    cout<<"hello, sample_CDREmergency!"<<endl;
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

    SampleCDREmergencyContext stContext;
    pSampleCDREmergencyContext = &stContext;
    //parse command line param
    if(stContext.ParseCmdLine(argc, argv) != NO_ERROR)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        return result;
    }
    //parse config file.
    if(stContext.loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }
    //check folder existence, create folder if necessary
    if(SUCCESS != stContext.CreateFolder(stContext.mConfigPara.mFolderPath))
    {
        return -1;
    }
    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");
    //init mpp system
    memset(&stContext.mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //config camera.
    {
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
        CameraInfo cameraInfo;
        ISPGeometry tmpISPGeometry;
        VI_CHN chn;
        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mMPPGeometry.mCSIChn = 1;
        tmpISPGeometry.mISPDev = 0;
        tmpISPGeometry.mScalerOutChns.clear();
        tmpISPGeometry.mScalerOutChns.push_back(HVIDEO(0,0));
        tmpISPGeometry.mScalerOutChns.push_back(HVIDEO(1,0));
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(tmpISPGeometry);
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
    captureSize.Width = stContext.mConfigPara.mCaptureWidth;
    captureSize.Height = stContext.mConfigPara.mCaptureHeight;
    cameraParam.setVideoSize(captureSize);
    cameraParam.setPreviewFrameRate(stContext.mConfigPara.mFrameRate);
    cameraParam.setPreviewFormat(stContext.mConfigPara.mPicFormat);
    cameraParam.setVideoBufferNumber(5); 
    stContext.mpCamera->setParameters(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], cameraParam);
    stContext.mpCamera->prepareChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

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
    stContext.mLayerAttr.stDispRect.X = 0;
    stContext.mLayerAttr.stDispRect.Y = 0;
    stContext.mLayerAttr.stDispRect.Width = stContext.mConfigPara.mDisplayWidth;
    stContext.mLayerAttr.stDispRect.Height = stContext.mConfigPara.mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &stContext.mLayerAttr);
    stContext.mVoLayer = hlay;
    //camera preview test
    alogd("prepare setPreviewDisplay(), hlay=%d", stContext.mVoLayer);
    stContext.mpCamera->setChannelDisplay(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0], stContext.mVoLayer);
    stContext.mpCamera->startChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    int ret = cdx_sem_down_timedwait(&stContext.mSemRenderStart, 5000);
    if(0 == ret)
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

    //record first file.
    stContext.mpRecorder = new EyeseeRecorder();
    stContext.mpRecorder->setOnInfoListener(&stContext.mRecorderListener);
    stContext.mpRecorder->setOnDataListener(&stContext.mRecorderListener);
    stContext.mpRecorder->setOnErrorListener(&stContext.mRecorderListener);
    //stContext.mpRecorder->setCamera(stContext.mpCamera);
    stContext.mpRecorder->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    stContext.mpRecorder->setAudioSource(EyeseeRecorder::AudioSource::MIC);
    std::string strFilePath = stContext.mConfigPara.mFolderPath + '/' + stContext.MakeFileName();
    SinkParam stSinkParam;
    //memset(&stSinkParam, 0, sizeof(SinkParam));
    stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_MP4;
    stSinkParam.mOutputFd = -1;
    stSinkParam.mOutputPath = (char*)strFilePath.c_str();
    stSinkParam.mFallocateLen = 0;
    stSinkParam.mMaxDurationMs = stContext.mConfigPara.mDuration*1000;
    stSinkParam.bCallbackOutFlag = false;
    stSinkParam.bBufFromCacheFlag = false;
    //stSinkParam.mFileDurationPolicy = RecordFileDurationPolicy_AverageDuration;
    stContext.mMuxerId = stContext.mpRecorder->addOutputSink(&stSinkParam);
    alogd("common muxerId=[%d]", stContext.mMuxerId);
    stContext.mFiles.push_back(strFilePath);

    //VencParameters
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
            stVencChnAttr.VeAttr.AttrH264e.PicWidth = stContext.mConfigPara.mEncodeWidth;
            stVencChnAttr.VeAttr.AttrH264e.PicHeight = stContext.mConfigPara.mEncodeHeight;
            stVencChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
            stVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = FALSE;
            stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            stVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = stContext.mConfigPara.mEncodeBitrate;
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
            stVencChnAttr.VeAttr.AttrH265e.mPicWidth = stContext.mConfigPara.mEncodeWidth;
            stVencChnAttr.VeAttr.AttrH265e.mPicHeight = stContext.mConfigPara.mEncodeHeight;
            stVencChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
            stVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = FALSE;
            stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            stVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = stContext.mConfigPara.mEncodeBitrate;
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
            stVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = stContext.mConfigPara.mEncodeWidth;
            stVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = stContext.mConfigPara.mEncodeHeight;
            stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = stContext.mConfigPara.mEncodeBitrate;
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
    stVencChnAttr.VeAttr.MaxKeyInterval = stContext.mConfigPara.mEncodeFrameRate;
    stVencParam.setVencChnAttr(stVencChnAttr);
    alogd("setVideoFrameRate=[%d]", stContext.mConfigPara.mEncodeFrameRate);
    stVencParam.setVideoFrameRate(stContext.mConfigPara.mEncodeFrameRate);
    //stVencParam.setVideoEncodingIFramesNumberInterVal(stContext.mConfigPara.mEncodeFrameRate);
    stVencParam.setVencRcParam(stVencRCParam);
    stContext.mVencId = 0;
    stContext.mpRecorder->setVencParameters(stContext.mVencId, &stVencParam);
    //bind Venc and Vipp
    stContext.mpRecorder->bindVeVipp(stContext.mVencId, cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    
    alogd("setSourceChannel=[%d]", cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    //stContext.mpRecorder->setSourceChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpRecorder->setCameraProxy(stContext.mpCamera->getRecordingProxy(), cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);

#if (SAMPLE_RECODER_MODE == SAMPLE_RECODER_1_MODE)
    if(stContext.mConfigPara.mAudioEncodeType != PT_MAX)
    {
        stContext.mpRecorder->setAudioSamplingRate(16000);
        stContext.mpRecorder->setAudioChannels(1);
        stContext.mpRecorder->setAudioEncodingBitRate(36000);
        stContext.mpRecorder->setAudioEncoder(PT_AAC);
    }
#endif

    if(ByTime == stContext.mConfigPara.mMethod)
    {
        stContext.mpRecorder->setMaxDuration(stContext.mConfigPara.mDuration*1000);
    }
    else
    {
        stContext.mpRecorder->setMaxFileSize(stContext.mConfigPara.mSize);
    }

    alogd("setImpactFileDuration:%dms!", stContext.mConfigPara.mMuxCacheDuration);
    stContext.mpRecorder->setMuxCacheDuration(stContext.mConfigPara.mMuxCacheDuration);
    
    alogd("prepare()!");
    stContext.mpRecorder->prepare();
    alogd("start()!");
    stContext.mpRecorder->start();

    if (stContext.mConfigPara.mFirstEmergencyTime > 0)
    {
        // The first Emergency Record.
        sleep(stContext.mConfigPara.mFirstEmergencyTime);
        alogd("@@@=== The first Emergency happened!");
        if(ImpactStyle::AddFile == stContext.mConfigPara.mImpactStyle)
        {
            bool bNewImpactFlag = false;
            stContext.mSwitchImpactLock.lock();
            if(false == stContext.mbMuxImpactFlag) //impact file is done. so switch impact file again!
            {
                stContext.mImpactTm = CDX_GetSysTimeUsMonotonic()/1000;
                bNewImpactFlag = true;
            }
            else //impact file is not done. so try to extend impact file duration.
            {
                alogd("Impact Style:%d muxerId[%d] impact file is still going on, extend duration!", stContext.mConfigPara.mImpactStyle, stContext.mMuxerIdEmergency);
                if(stContext.mImpactTm >= 0)
                {
                    int64_t nCurImpactTm = CDX_GetSysTimeUsMonotonic()/1000;
                    int newFileDuration = stContext.mConfigPara.mImpactDuration*1000 + (nCurImpactTm-stContext.mImpactTm);
                    result = stContext.mpRecorder->setMaxDuration(stContext.mMuxerIdEmergency, newFileDuration);
                    if(result != NO_ERROR)
                    {
                        alogw("Be careful! muxerId[%d] can't change duration,reason:0x%x", stContext.mMuxerId, result);
                    }
                    stContext.mImpactTm = nCurImpactTm;
                }
                else
                {
                    aloge("fatal error! why muxerId[%d] not record previous impact time?", stContext.mMuxerId);
                }
            }
            stContext.mSwitchImpactLock.unlock();

            if(bNewImpactFlag)
            {
                SinkParam sinkParam;
                //memset(&sinkParam, 0, sizeof(SinkParam));
                sinkParam.mOutputFd         = -1;
                std::string strFilePath = stContext.mConfigPara.mFolderPath + '/' + "emergency_file.mp4";
                sinkParam.mOutputPath       = (char*)strFilePath.c_str();
                sinkParam.mFallocateLen     = 0;
                sinkParam.mMaxDurationMs    = stContext.mConfigPara.mImpactDuration*1000;
                sinkParam.bCallbackOutFlag  = false;
                sinkParam.bBufFromCacheFlag = true;
                sinkParam.mFileDurationPolicy = RecordFileDurationPolicy_AccurateDuration;
                stContext.mMuxerIdEmergency = stContext.mpRecorder->addOutputSink(&sinkParam);
                if (stContext.mMuxerIdEmergency >= 0)
                {
                    stContext.mSwitchImpactLock.lock();
                    stContext.mbMuxImpactFlag = true;
                    stContext.mSwitchImpactLock.unlock();
                }
                else
                {
                    aloge("fatal error! Start emergency 1 record fail:%d!", stContext.mMuxerIdEmergency);
                    exit(-1);
                }
            }
        }
        else if(ImpactStyle::SwitchFile == stContext.mConfigPara.mImpactStyle)
        {
            bool bNewImpactFlag = false;
            stContext.mSwitchImpactLock.lock();
            if(false == stContext.mbMuxImpactFlag) //impact file is done. so switch impact file again!
            {
                stContext.mImpactTm = CDX_GetSysTimeUsMonotonic()/1000;
                bNewImpactFlag = true;
            }
            else //impact file is not done. so try to extend impact file duration.
            {
                alogd("Impact Style:%d muxerId[%d] impact file is still going on, extend duration!", stContext.mConfigPara.mImpactStyle, stContext.mMuxerId);
                if(stContext.mImpactTm >= 0)
                {
                    int64_t nCurImpactTm = CDX_GetSysTimeUsMonotonic()/1000;
                    int newFileDuration = stContext.mConfigPara.mImpactDuration*1000 + (nCurImpactTm-stContext.mImpactTm);
                    result = stContext.mpRecorder->setMaxDuration(stContext.mMuxerId, newFileDuration);
                    if(result != NO_ERROR)
                    {
                        alogw("Be careful! muxerId[%d] can't change duration,reason:0x%x", stContext.mMuxerId, result);
                    }
                    stContext.mImpactTm = nCurImpactTm;
                }
                else
                {
                    aloge("fatal error! why muxerId[%d] not record previous impact time?", stContext.mMuxerId);
                }
            }
            stContext.mSwitchImpactLock.unlock();

            if(bNewImpactFlag)
            {
                std::string strFilePath = stContext.mConfigPara.mFolderPath + '/' + "emergency_file.mp4";
                result = stContext.mpRecorder->switchImpactFile((char*)strFilePath.c_str(), 0, stContext.mMuxerId);
                if(result != NO_ERROR)
                {
                    aloge("fatal error! why muxerId[%d] switch impact file fail[0x%x]?", stContext.mMuxerId, result);
                }
                //change file duration to impace file duration, when switch to normal file, we need change it back.
                result = stContext.mpRecorder->setMaxDuration(stContext.mMuxerId, stContext.mConfigPara.mImpactDuration*1000);
                if(result != NO_ERROR)
                {
                    aloge("fatal error! why change muxerId duration fail[0x%x]?", stContext.mMuxerId, result);
                }
                stContext.mSwitchImpactLock.lock();
                stContext.mbMuxImpactFlag = true;
                stContext.mSwitchImpactLock.unlock();
            }
        }
        else
        {
            aloge("fatal error! check code!");
        }
    }

    if (stContext.mConfigPara.mSecondEmergencyTime > stContext.mConfigPara.mFirstEmergencyTime)
    {
        int nImpactInterval = stContext.mConfigPara.mSecondEmergencyTime - stContext.mConfigPara.mFirstEmergencyTime;
        sleep(nImpactInterval);
        alogd("@@@=== The second Emergency happened!");
        if(ImpactStyle::AddFile == stContext.mConfigPara.mImpactStyle)
        {
            bool bNewImpactFlag = false;
            stContext.mSwitchImpactLock.lock();
            if(false == stContext.mbMuxImpactFlag) //impact file is done. so switch impact file again!
            {
                alogd("second impact file is new!");
                stContext.mImpactTm = CDX_GetSysTimeUsMonotonic()/1000;
                bNewImpactFlag = true;
            }
            else //impact file is not done. so try to extend impact file duration.
            {
                alogd("Impact Style:%d muxerId[%d] second impact file is replaced as old first impact file extension!",
                    stContext.mConfigPara.mImpactStyle, stContext.mMuxerIdEmergency);
                if(stContext.mImpactTm >= 0)
                {
                    int64_t nCurImpactTm = CDX_GetSysTimeUsMonotonic()/1000;
                    int newFileDuration = stContext.mConfigPara.mImpactDuration*1000 + (nCurImpactTm-stContext.mImpactTm);
                    result = stContext.mpRecorder->setMaxDuration(stContext.mMuxerIdEmergency, newFileDuration);
                    if(result != NO_ERROR)
                    {
                        alogw("Be careful! muxerId[%d] can't change duration,reason:0x%x", stContext.mMuxerId, result);
                    }
                    stContext.mImpactTm = nCurImpactTm;
                }
                else
                {
                    aloge("fatal error! why muxerId[%d] not record previous impact time?", stContext.mMuxerId);
                }
            }
            stContext.mSwitchImpactLock.unlock();

            if(bNewImpactFlag)
            {
                SinkParam sinkParam;
                //memset(&sinkParam, 0, sizeof(SinkParam));
                sinkParam.mOutputFd         = -1;
                std::string strFilePath = stContext.mConfigPara.mFolderPath + '/' + "emergency_file2.mp4";
                sinkParam.mOutputPath       = (char*)strFilePath.c_str();
                sinkParam.mFallocateLen     = 0;
                sinkParam.mMaxDurationMs    = stContext.mConfigPara.mImpactDuration*1000;
                sinkParam.bCallbackOutFlag  = false;
                sinkParam.bBufFromCacheFlag = true;
                sinkParam.mFileDurationPolicy = RecordFileDurationPolicy_AccurateDuration;
                if(stContext.mMuxerIdEmergency != -1)
                {
                    aloge("fatal error! first impact file muxerId[%d] is not record done?", stContext.mMuxerIdEmergency);
                }
                stContext.mMuxerIdEmergency = stContext.mpRecorder->addOutputSink(&sinkParam);
                if (stContext.mMuxerIdEmergency >= 0)
                {
                    stContext.mSwitchImpactLock.lock();
                    stContext.mbMuxImpactFlag = true;
                    stContext.mSwitchImpactLock.unlock();
                }
                else
                {
                    aloge("fatal error! wrong impact muxerId[%d]", stContext.mMuxerIdEmergency);
                }
            }
        }
        else if(ImpactStyle::SwitchFile == stContext.mConfigPara.mImpactStyle)
        {
            bool bNewImpactFlag = false;
            stContext.mSwitchImpactLock.lock();
            if(false == stContext.mbMuxImpactFlag) //impact file is done. so switch impact file again!
            {
                alogd("second impact file is new!");
                stContext.mImpactTm = CDX_GetSysTimeUsMonotonic()/1000;
                bNewImpactFlag = true;
            }
            else //impact file is not done. so try to extend impact file duration.
            {
                alogd("Impact Style:%d muxerId[%d] second impact file is replaced as old first impact file extension!",
                    stContext.mConfigPara.mImpactStyle, stContext.mMuxerId);
                if(stContext.mImpactTm >= 0)
                {
                    int64_t nCurImpactTm = CDX_GetSysTimeUsMonotonic()/1000;
                    int newFileDuration = stContext.mConfigPara.mImpactDuration*1000 + (nCurImpactTm-stContext.mImpactTm);
                    result = stContext.mpRecorder->setMaxDuration(stContext.mMuxerId, newFileDuration);
                    if(result != NO_ERROR)
                    {
                        alogw("Be careful! muxerId[%d] can't change duration,reason:0x%x", stContext.mMuxerId, result);
                    }
                    stContext.mImpactTm = nCurImpactTm;
                }
                else
                {
                    aloge("fatal error! why muxerId[%d] not record previous impact time?", stContext.mMuxerId);
                }
            }
            stContext.mSwitchImpactLock.unlock();

            if(bNewImpactFlag)
            {
                std::string strFilePath = stContext.mConfigPara.mFolderPath + '/' + "emergency_file2.mp4";
                result = stContext.mpRecorder->switchImpactFile((char*)strFilePath.c_str(), 0, stContext.mMuxerId);
                if(result != NO_ERROR)
                {
                    aloge("fatal error! why muxerId[%d] switch impact file fail[0x%x]?", stContext.mMuxerId, result);
                }
                //change file duration to impace file duration, when switch to normal file, we need change it back.
                result = stContext.mpRecorder->setMaxDuration(stContext.mMuxerId, stContext.mConfigPara.mImpactDuration*1000);
                if(result != NO_ERROR)
                {
                    aloge("fatal error! why change muxerId duration fail[0x%x]?", stContext.mMuxerId, result);
                }
                stContext.mSwitchImpactLock.lock();
                stContext.mbMuxImpactFlag = true;
                stContext.mSwitchImpactLock.unlock();
            }
        }
        else
        {
            aloge("fatal error! check code!");
        }
    }

    cdx_sem_down(&stContext.mSemExit);
    alogd("record done! stop()!");
    stContext.mpRecorder->stop();
    delete stContext.mpRecorder;
    stContext.mpRecorder = NULL;
    //close camera
    alogd("Camera close!");
    stContext.mpCamera->stopChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->releaseChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]);
    stContext.mpCamera->closeChannel(cameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0]); 
    stContext.mpCamera->stopDevice(); 
    stContext.mpCamera->releaseDevice(); 
    EyeseeCamera::close(stContext.mpCamera);
    stContext.mpCamera = NULL;
    //close vo
    AW_MPI_VO_DisableVideoLayer(stContext.mVoLayer);
    stContext.mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(stContext.mUILayer);
    AW_MPI_VO_Disable(stContext.mVoDev);
    stContext.mVoDev = -1;
    //exit mpp system
    AW_MPI_SYS_Exit();
    log_quit();
    cout<<"bye, sample_CDREmergency!"<<endl;
    return result;
}

