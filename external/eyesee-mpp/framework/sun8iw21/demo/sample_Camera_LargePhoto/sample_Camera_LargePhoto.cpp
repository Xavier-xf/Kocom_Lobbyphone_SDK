//#define LOG_NDEBUG 0
#define LOG_TAG "sample_Camera_LargePhoto"
#include <utils/plat_log.h>

#include <sys/stat.h>
#include <cstdio>
#include <csignal>
#include <iostream>

#include <hwdisplay.h>
#include <confparser.h>
#include <mpi_sys.h>
#include <mpi_vi.h>
#include <mpi_vo.h>
#include <BITMAP_S.h>

#include "sample_Camera_LargePhoto_config.h"
#include "sample_Camera_LargePhoto.h"

//#define TEST_CHANGE_ISP_PARAMETER

using namespace std;
using namespace EyeseeLinux;

static SampleCameraLargePhotoContext *gpSampleCameraLargePhotoContext = NULL;

static void handle_exit(int signo)
{
    alogd("user wants to exit!");
    if(gpSampleCameraLargePhotoContext!=NULL)
    {
        cdx_sem_up(&gpSampleCameraLargePhotoContext->mSemExit);
    }
}

bool EyeseeCameraCallback::onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeCamera *pCamera)
{
    bool bHandleInfoFlag = true;
    switch(info)
    {
        case CAMERA_INFO_RENDERING_START:
        {
            if(chnId == mpContext->mScalerOutChnForPreview)
            {
                alogd("scalerOutChn[%d] notify render start!", chnId);
                cdx_sem_up(&mpContext->mSemRenderStart);
            }
            else
            {
                aloge("fatal error! scalerOutChn[%d] notify render start, but scalerOutChn[%d] wait render start!", chnId, mpContext->mScalerOutChnForPreview);
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

void EyeseeCameraCallback::onError(int chnId, int error, EyeseeLinux::EyeseeCamera *pCamera)
{
    switch(error)
    {
        case CAMERA_ERROR_SELECT_TIMEOUT:
        {
            alogw("fatal error! scalerChn[%d] select frame fail", chnId);
            break;
        }
        case CAMERA_ERROR_TAKE_PIC_FAIL:
        {
            alogw("fatal error! scalerChn[%d] take picture fail", chnId);
            break;
        }
        default:
        {
            aloge("fatal error! scalerChn[%d] unknown error!", chnId);
            break;
        }
    }
}

void EyeseeCameraCallback::onPictureTaken(int chnId, const void *data, int size, EyeseeCamera *pCamera)
{
    int ret = -1;
    alogd("channel %d picture data size %d", chnId, size);
    if(chnId != mpContext->mScalerOutChnForLargePhoto)
    {
        aloge("fatal error! channel[%d] is not match current channel[%d]", chnId, mpContext->mScalerOutChnForLargePhoto);
    }
    char picName[64];
    sprintf(picName, "pic[%02d].jpg", mpContext->mPicNum++);
    std::string PicFullPath = mpContext->mConfigPara.strJpegFolderPath + '/' + picName;
    FILE *fp = fopen(PicFullPath.c_str(), "wb");
    fwrite(data, 1, size, fp);
    fclose(fp);
    mpContext->mPicNumInOneTakePicture++;
    if(mpContext->mPicNumInOneTakePicture == mpContext->mConfigPara.nJpegNum)
    {
        alogd("scalerOutChn[%d] reach picture number[%d]", chnId, mpContext->mPicNumInOneTakePicture);
        cdx_sem_up_unique(&mpContext->mSemTakePictureDone);
    }
    else
    {
        alogd("scalerOutChn[%d] take picture number[%d/%d] in current takingPicture process", chnId,
            mpContext->mPicNumInOneTakePicture, mpContext->mConfigPara.nJpegNum);
    }
}

EyeseeCameraCallback::EyeseeCameraCallback(SampleCameraLargePhotoContext *pContext)
    : mpContext(pContext)
{
}

SampleCameraLargePhotoContext::SampleCameraLargePhotoContext()
    :mCameraCallbacks(this)
{
	alogd("---------->SampleCameraLargePhotoContext construction!!");
    cdx_sem_init(&mSemExit, 0);
    cdx_sem_init(&mSemRenderStart, 0);
    cdx_sem_init(&mSemTakePictureDone, 0);
    mUILayer = HLAY(2, 0);
    mVoDev = -1;
    mVoLayer = -1;
    mpCamera = NULL;
    mPicNum = 0;
    mPicNumInOneTakePicture = 0;
    mScalerOutChnForPreview = -1;
    mScalerOutChnForLargePhoto = -1;
    mIspDevForPreview = MM_INVALID_DEV;
    mIspDevForLargePhoto = MM_INVALID_DEV;
}

SampleCameraLargePhotoContext::~SampleCameraLargePhotoContext()
{
    cdx_sem_deinit(&mSemExit);
    cdx_sem_deinit(&mSemRenderStart);
    cdx_sem_deinit(&mSemTakePictureDone);
    if(mpCamera!=NULL)
    {
        aloge("fatal error! EyeseeCamera is not destruct!");
    }
}

status_t SampleCameraLargePhotoContext::ParseCmdLine(int argc, char *argv[])
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
            helpString += "\t run -path /mnt/extsd/sample_Camera_LargePhoto.conf\n";
            cout<<helpString<<endl;
            ret = 1;
            break;
        }
        else
        {
            std::string ignoreString;
            ignoreString += "ignore invalid CmdLine param:[";
            ignoreString += argv[i];
            ignoreString += ", type -h to get how to set parameter!";
            cout<<ignoreString<<endl;
        }
        i++;
    }
    return ret;
}

static PIXEL_FORMAT_E convertPixelFormatString2PIXEL_FORMAT_E(char *pStrPixelFormat)
{
    PIXEL_FORMAT_E ePixelFormat;
    if(!strcmp(pStrPixelFormat, "yu12"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "yv12"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv21"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv12"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if(!strcmp(pStrPixelFormat, "nv61"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_422;
    }
    else if(!strcmp(pStrPixelFormat, "nv16"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_422;
    }
    else if(!strcmp(pStrPixelFormat, "lbc1.0"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
    }
    else if(!strcmp(pStrPixelFormat, "lbc1.5"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
    }
    else if(!strcmp(pStrPixelFormat, "lbc2.0"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if(!strcmp(pStrPixelFormat, "lbc2.5"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        ePixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    return ePixelFormat;
}

status_t SampleCameraLargePhotoContext::loadConfig()
{
    int ret;
    char *ptr;
    std::string& ConfigFilePath = mCmdLinePara.mConfigFilePath;
    if(ConfigFilePath.empty())
    {
        alogd("user not set config file. use default test parameter!");
        mConfigPara.nPreviewWidth = 640;
        mConfigPara.nPreviewHeight = 320;
        mConfigPara.nPreviewFrameRate = 20;
        mConfigPara.nPreviewFrameNum = 5;
        mConfigPara.ePreviewPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        mConfigPara.nPreviewRotation = 0;
        mConfigPara.nDisplayFrameRate = 0;
        mConfigPara.nDisplayWidth = 640;
        mConfigPara.nDisplayHeight = 360;
        mConfigPara.bKeepRender = false;
    
        mConfigPara.nCaptureWidth = 1920;
        mConfigPara.nCaptureHeight = 1080;
        mConfigPara.nCaptureFrameRate = 20;
        mConfigPara.nCaptureFrameNum = 3;
        mConfigPara.eCapturePicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;

        mConfigPara.nDigitalZoom = 0;
    
        mConfigPara.nTakePhotoTimes = 1;
        mConfigPara.bKeepJpegEncoder = false;
        mConfigPara.nJpegWidth = 1920;
        mConfigPara.nJpegHeight = 1080;
        mConfigPara.nJpegQuality = 90;
        mConfigPara.nJpegThumbWidth = 480;
        mConfigPara.nJpegThumbHeight = 270;
        mConfigPara.nJpegThumbQuality = 80;
        mConfigPara.nJpegNum = 1;
        mConfigPara.nJpegInterval = 0;
        mConfigPara.strJpegFolderPath = "/mnt/extsd/sample_Camera_LargePhoto_Files";

        mConfigPara.nTestDuration = 30;
        return SUCCESS;
    }
    CONFPARSER_S stConfParser;
    ret = createConfParser(ConfigFilePath.c_str(), &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return UNKNOWN_ERROR;
    }
    mConfigPara.nPreviewWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_PREVIEW_WIDTH, 0);
    mConfigPara.nPreviewHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_PREVIEW_HEIGHT, 0);
    mConfigPara.nPreviewFrameRate = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_PREVIEW_FRAME_RATE, 0);
    mConfigPara.nPreviewFrameNum = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_PREVIEW_FRAME_NUM, 0);
    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_PREVIEW_PIC_FORMAT, NULL);
    mConfigPara.ePreviewPicFormat = convertPixelFormatString2PIXEL_FORMAT_E(pStrPixelFormat);
    int nPreviewMirror = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_PREVIEW_MIRROR, 0);
    int nPreviewRotation = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_PREVIEW_ROTATION, 0);
    mConfigPara.nPreviewRotation = (nPreviewMirror << 16) | nPreviewRotation;
    mConfigPara.nDisplayFrameRate = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_DISPLAY_FRAME_RATE, 0);
    mConfigPara.nDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_DISP_WIDTH, 0);
    mConfigPara.nDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_DISP_HEIGHT, 0);
    mConfigPara.bKeepRender = (bool)GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_KEEP_RENDER, 0);

    mConfigPara.nCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_CAPTURE_WIDTH, 0);
    mConfigPara.nCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_CAPTURE_HEIGHT, 0);
    mConfigPara.nCaptureFrameRate = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_CAPTURE_FRAME_RATE, 0);
    mConfigPara.nCaptureFrameNum = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_CAPTURE_FRAME_NUM, 0);
    mConfigPara.capture_lost_frame_cnt = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_CAPTURE_LOST_FRAME_CNT, 0);
    pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_CAPTURE_PIC_FORMAT, NULL);
    mConfigPara.eCapturePicFormat = convertPixelFormatString2PIXEL_FORMAT_E(pStrPixelFormat);

    mConfigPara.nDigitalZoom = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_DIGITAL_ZOOM, 0);

    mConfigPara.nTakePhotoTimes = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_TAKE_PHOTO_TIMES, 0);
    mConfigPara.bKeepJpegEncoder = (bool)GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_KEEP_JPEG_ENCODER, 0);
    mConfigPara.nJpegWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_JPEG_WIDTH, 0);
    mConfigPara.nJpegHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_JPEG_HEIGHT, 0);
    mConfigPara.nJpegQuality = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_JPEG_QUALITY, 0);
    mConfigPara.nJpegThumbWidth = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_JPEG_THUMB_WIDTH, 0);
    mConfigPara.nJpegThumbHeight = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_JPEG_THUMB_HEIGHT, 0);
    mConfigPara.nJpegThumbQuality = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_JPEG_THUMB_QUALITY, 0);
    mConfigPara.nJpegNum = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_JPEG_NUM, 0);
    mConfigPara.nJpegInterval = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_JPEG_INTERVAL, 0);
    mConfigPara.strJpegFolderPath = GetConfParaString(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_JPEG_FOLDER, NULL);

    mConfigPara.nTestDuration = GetConfParaInt(&stConfParser, SAMPLE_CAMERA_LARGEPHOTO_KEY_TEST_DURATION, 0);
    
    destroyConfParser(&stConfParser);
    return SUCCESS;
}

status_t SampleCameraLargePhotoContext::CreateFolder(const std::string& strFolderPath)
{
    if(strFolderPath.empty())
    {
        aloge("jpeg path is not set!");
        return UNKNOWN_ERROR;
    }
    const char* pJpegFolderPath = strFolderPath.c_str();
    //check folder existence
    struct stat sb;
    if (stat(pJpegFolderPath, &sb) == 0)
    {
        if(S_ISDIR(sb.st_mode))
        {
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! [%s] is exist, but mode[0x%x] is not directory!", pJpegFolderPath, sb.st_mode);
            return UNKNOWN_ERROR;
        }
    }
    //create folder if necessary
    int ret = mkdir(pJpegFolderPath, S_IRWXU | S_IRWXG | S_IRWXO);
    if(!ret)
    {
        alogd("create folder[%s] success", pJpegFolderPath);
        return SUCCESS;
    }
    else
    {
        aloge("fatal error! create folder[%s] failed!", pJpegFolderPath);
        return UNKNOWN_ERROR;
    }
}

void SampleCameraLargePhotoPictureRegionCallback::addPictureRegion(std::list<PictureRegionType> &rPictureRegionList)
{
    rPictureRegionList.clear();
}

//#define Region_debug
int main(int argc, char *argv[])
{
    int result = 0;
    int ret;
    status_t rc;
    cout<<"hello, sample_Camera_LargePhoto!"<<endl;
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
    
    SampleCameraLargePhotoContext *pContext = new SampleCameraLargePhotoContext;
    gpSampleCameraLargePhotoContext = pContext;
    //parse command line param
    if(pContext->ParseCmdLine(argc, argv) != NO_ERROR)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        return result;
    }
    //parse config file.
    if(pContext->loadConfig() != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        return result;
    }
    //check folder existence, create folder if necessary
    if(pContext->mConfigPara.nJpegNum > 0)
    {
        if(SUCCESS != pContext->CreateFolder(pContext->mConfigPara.strJpegFolderPath))
        {
            return -1;
        }
    }
    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        perror("can't catch SIGSEGV");
    }
    //init mpp system
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    pContext->mVoDev = 0;
    AW_MPI_VO_Enable(pContext->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pContext->mUILayer);//close ui layer.
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(pContext->mVoDev, &spPubAttr);
    spPubAttr.enIntfType = VO_INTF_LCD;
    spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(pContext->mVoDev, &spPubAttr);

    //config camera.
    {
        EyeseeCamera::clearCamerasConfiguration();
        int cameraId;
        CameraInfo cameraInfo;
        VI_CHN chn;
        ISPGeometry tmpISPGeometry;
        cameraId = 0;
        cameraInfo.facing = CAMERA_FACING_BACK;
        cameraInfo.orientation = 0;
        cameraInfo.canDisableShutterSound = true;
        cameraInfo.mCameraDeviceType = CameraInfo::CAMERA_CSI;
        cameraInfo.mStitchMode = 1;
        cameraInfo.mMPPGeometry.mCSIChn = 0;
        tmpISPGeometry.mISPDev = 0;
        tmpISPGeometry.mScalerOutChns.clear();
        tmpISPGeometry.mScalerOutChns.push_back(HVIDEO(0,0));
        // tmpISPGeometry.mScalerOutChns.push_back(HVIDEO(0,1));
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(tmpISPGeometry);

        tmpISPGeometry.mISPDev = 1;
        tmpISPGeometry.mScalerOutChns.clear();
        // tmpISPGeometry.mScalerOutChns.push_back(HVIDEO(0,0));
        tmpISPGeometry.mScalerOutChns.push_back(HVIDEO(0,1));
        cameraInfo.mMPPGeometry.mISPGeometrys.push_back(tmpISPGeometry);

        EyeseeCamera::configCameraWithMPPModules(cameraId, &cameraInfo);
    }

    CameraParameters cameraParam;
    CameraParameters stISPParams;
    int awbMode; //ref to enum v4l2_auto_n_preset_white_balance, but not same in 0 and 1.
    scene_mode_t eSpecialScene;
    enum colorfx eColorEffect;
    int nISOSensitiveIndex;
    int nExposureBiasIndex;
    int cameraId = 0;
    EyeseeCamera::getCameraInfo(cameraId, &pContext->mCameraInfo);
    alogd("---EyeseeCamera::open->\n\n");
    pContext->mpCamera = EyeseeCamera::open(cameraId);//EyeseeCamera 构造函数
    pContext->mpCamera->setInfoCallback(&pContext->mCameraCallbacks);
    pContext->mpCamera->prepareDevice(); 
    pContext->mpCamera->startDevice();
    pContext->mScalerOutChnForPreview = pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mScalerOutChns[0];
    pContext->mScalerOutChnForLargePhoto = pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[1].mScalerOutChns[0];
    pContext->mIspDevForPreview = pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[0].mISPDev;
    pContext->mIspDevForLargePhoto = pContext->mCameraInfo.mMPPGeometry.mISPGeometrys[1].mISPDev;
    alogd("mScalerOutChnForLargePhoto = %d-%d\n", pContext->mIspDevForLargePhoto, pContext->mScalerOutChnForLargePhoto);

    pContext->mpCamera->openChannel(pContext->mScalerOutChnForPreview, true);//VIChannel 构造函数 -->各种thread构造函数
    pContext->mpCamera->openChannel(pContext->mScalerOutChnForLargePhoto, false);//VIChannel 构造函数 -->各种thread构造函数

    pContext->mpCamera->KeepPictureEncoder(pContext->mScalerOutChnForLargePhoto, pContext->mConfigPara.bKeepJpegEncoder);

    pContext->mpCamera->getParameters(pContext->mScalerOutChnForPreview, cameraParam);
    SIZE_S previewSize={(unsigned int)pContext->mConfigPara.nPreviewWidth, (unsigned int)pContext->mConfigPara.nPreviewHeight};
    cameraParam.setVideoSize(previewSize);
    cameraParam.setPreviewFrameRate(pContext->mConfigPara.nPreviewFrameRate);
    cameraParam.setDisplayFrameRate(pContext->mConfigPara.nDisplayFrameRate);
    cameraParam.setPreviewFormat(pContext->mConfigPara.ePreviewPicFormat);
    cameraParam.setVideoBufferNumber(pContext->mConfigPara.nPreviewFrameNum); 
    cameraParam.setPreviewRotation(pContext->mConfigPara.nPreviewRotation);
    cameraParam.setLostFrameNumber(pContext->mConfigPara.capture_lost_frame_cnt);
    pContext->mpCamera->setParameters(pContext->mScalerOutChnForPreview, cameraParam);//AW_MPI_VI_SetVippAttr

    pContext->mpCamera->prepareChannel(pContext->mScalerOutChnForPreview);//AW_MPI_VI_CreateVipp

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
    AW_MPI_VO_GetVideoLayerAttr(hlay, &pContext->mLayerAttr);
    pContext->mLayerAttr.stDispRect.X = 0;
    pContext->mLayerAttr.stDispRect.Y = 0;
    pContext->mLayerAttr.stDispRect.Width = pContext->mConfigPara.nDisplayWidth;
    pContext->mLayerAttr.stDispRect.Height = pContext->mConfigPara.nDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(hlay, &pContext->mLayerAttr);
    pContext->mVoLayer = hlay;
    //camera preview test
    alogd("prepare setPreviewDisplay(), hlay=%d", pContext->mVoLayer);
    pContext->mpCamera->setChannelDisplay(pContext->mScalerOutChnForPreview, pContext->mVoLayer);
    //pContext->mpCamera->startRender(pContext->mScalerOutChn);

#ifdef TEST_CHANGE_ISP_PARAMETER
    pContext->mpCamera->getISPParameters(pContext->mIspDevForPreview, stISPParams);
//    awbMode = (int)V4L2_WHITE_BALANCE_FLUORESCENT;
//    stISPParams.ChnIspAwb_SetMode(awbMode);
//    eColorEffect = ISP_COLORFX_NEGATIVE;
//    stISPParams.ChnIsp_SetColorEffect(eColorEffect);
//    eSpecialScene = GREEN_PLANTS;
//    stISPParams.ChnIsp_SetSpecialScene(eSpecialScene);
//    nISOSensitiveIndex = 1;
//    stISPParams.ChnIspAe_SetISOSensitive(nISOSensitiveIndex);
    nExposureBiasIndex = 1;
    stISPParams.ChnIspAe_SetExposureBias(nExposureBiasIndex);
    pContext->mpCamera->setISPParameters(pContext->mIspDevForPreview, stISPParams);
    alogd("preview change awbmode:%d colorEffect:%d specialScene:%d before startChannel!", awbMode, eColorEffect, eSpecialScene);
#endif

    pContext->mpCamera->startChannel(pContext->mScalerOutChnForPreview);
	/*
	AW_MPI_ISP_Run
	AW_MPI_VI_EnableVipp 
	AW_MPI_VI_CreateVirChn  
	AW_MPI_VI_EnableVirChn 
	AW_MPI_VO_CreateChn  
	AW_MPI_VO_StartChn
	*/
    ret = cdx_sem_down_timedwait(&pContext->mSemRenderStart, 5000);
    if(0 == ret)
    {
        alogd("app receive message that camera start render!");
        alogd("---------------------------------------------");
    }
    else if(ETIMEDOUT == ret)
    {
        aloge("fatal error! wait render start timeout");
    }
    else
    {
        aloge("fatal error! other error[0x%x]", ret);
    }

    //test zoom
    if(pContext->mConfigPara.nDigitalZoom > 0)
    {
        ret = cdx_sem_down_timedwait(&pContext->mSemExit, 5*1000);
        if(0 == ret)
        {
            alogd("user want to exit!");
            goto _exit0;
        }
        pContext->mpCamera->getParameters(pContext->mScalerOutChnForPreview, cameraParam);
        int oldZoom = cameraParam.getZoom();
        cameraParam.setZoom(pContext->mConfigPara.nDigitalZoom);
        pContext->mpCamera->setParameters(pContext->mScalerOutChnForPreview, cameraParam);
        alogd("change digital zoom[%d]->[%d]", oldZoom, pContext->mConfigPara.nDigitalZoom);
    }
    // display some time to let user watch.
    ret = cdx_sem_down_timedwait(&pContext->mSemExit, 5*1000);
    if(0 == ret)
    {
        alogd("user want to exit!");
        goto _exit0;
    }

    //test vipp0 AWB mode
#ifdef TEST_CHANGE_ISP_PARAMETER
    pContext->mpCamera->getISPParameters(pContext->mIspDevForPreview, stISPParams);
    //awbMode = (int)V4L2_WHITE_BALANCE_CLOUDY;
    //stISPParams.ChnIspAwb_SetMode(awbMode);
    //nISOSensitiveIndex = 3;
    //stISPParams.ChnIspAe_SetISOSensitive(nISOSensitiveIndex);
    nExposureBiasIndex = 8;
    stISPParams.ChnIspAe_SetExposureBias(nExposureBiasIndex);
    pContext->mpCamera->setISPParameters(pContext->mIspDevForPreview, stISPParams);
    //alogd("preview change awbmode to %d!", awbMode);
    sleep(5);

    pContext->mpCamera->getISPParameters(pContext->mIspDevForPreview, stISPParams);
//    awbMode = 0; //auto mode
//    stISPParams.ChnIspAwb_SetMode(awbMode);
//    eColorEffect = ISP_COLORFX_NONE;
//    stISPParams.ChnIsp_SetColorEffect(eColorEffect);
//    eSpecialScene = NORMAL;
//    stISPParams.ChnIsp_SetSpecialScene(eSpecialScene);
//    nISOSensitiveIndex = 6;
//    stISPParams.ChnIspAe_SetISOSensitive(nISOSensitiveIndex);
    nExposureBiasIndex = 1;
    stISPParams.ChnIspAe_SetExposureBias(nExposureBiasIndex);
    pContext->mpCamera->setISPParameters(pContext->mIspDevForPreview, stISPParams);
    alogd("preview change awbmode to %d, colorEffect to %d, specialScene to %d!", awbMode, eColorEffect, eSpecialScene);
    sleep(5);
#endif

    //test take picture
    for(int i=0; i<pContext->mConfigPara.nTakePhotoTimes; i++)
    {
        alogd("take photo times: [%d]", i);
        //1. re-config scalerOutChn params.
        rc = pContext->mpCamera->stopChannel(pContext->mScalerOutChnForPreview, pContext->mConfigPara.bKeepRender);
        if(rc != NO_ERROR)
        {
            aloge("fatal error! stop scalerOutChn[%d] fail[0x%x]", pContext->mScalerOutChnForPreview, rc);
        }
        rc = pContext->mpCamera->releaseChannel(pContext->mScalerOutChnForPreview);
        if(rc != NO_ERROR)
        {
            aloge("fatal error! release scalerOutChn[%d] fail[0x%x]", pContext->mScalerOutChnForPreview, rc);
        }
        pContext->mpCamera->getParameters(pContext->mScalerOutChnForLargePhoto, cameraParam);
        SIZE_S captureSize={(unsigned int)pContext->mConfigPara.nCaptureWidth, (unsigned int)pContext->mConfigPara.nCaptureHeight};
        cameraParam.setVideoSize(captureSize);
        cameraParam.setPreviewFrameRate(pContext->mConfigPara.nCaptureFrameRate);
        cameraParam.setDisplayFrameRate(pContext->mConfigPara.nDisplayFrameRate);
        cameraParam.setPreviewFormat(pContext->mConfigPara.eCapturePicFormat);
        cameraParam.setVideoBufferNumber(pContext->mConfigPara.nCaptureFrameNum);
        cameraParam.setLostFrameNumber(pContext->mConfigPara.capture_lost_frame_cnt);
        // cameraParam.setLostFrameNumber(0);
        pContext->mpCamera->setParameters(pContext->mScalerOutChnForLargePhoto, cameraParam);//AW_MPI_VI_SetVippAttr
        pContext->mpCamera->prepareChannel(pContext->mScalerOutChnForLargePhoto);//AW_MPI_VI_CreateVipp
        pContext->mpCamera->stopRender(pContext->mScalerOutChnForLargePhoto);
        //pContext->mpCamera->setChannelDisplay(pContext->mScalerOutChnForLargePhoto, pContext->mVoLayer);
#ifdef TEST_CHANGE_ISP_PARAMETER
        pContext->mpCamera->getISPParameters(pContext->mIspDevForLargePhoto, stISPParams);
        awbMode = (int)V4L2_WHITE_BALANCE_INCANDESCENT;
        stISPParams.ChnIspAwb_SetMode(awbMode);
        eColorEffect = ISP_COLORFX_GRAY;
        stISPParams.ChnIsp_SetColorEffect(eColorEffect);
        pContext->mpCamera->setISPParameters(pContext->mIspDevForLargePhoto, stISPParams);
        alogd("capture change awbmode to %d, colorEffect to %d before startChannel", awbMode, eColorEffect);
#endif
        pContext->mpCamera->startChannel(pContext->mScalerOutChnForLargePhoto);

        //2.take picture
        pContext->mPicNumInOneTakePicture = 0;
        if(1 == pContext->mConfigPara.nJpegNum)
        {
            pContext->mpCamera->getParameters(pContext->mScalerOutChnForLargePhoto, cameraParam);
            cameraParam.setPictureMode(TAKE_PICTURE_MODE_FAST);
            cameraParam.setPictureSize(SIZE_S{(unsigned int)pContext->mConfigPara.nJpegWidth, (unsigned int)pContext->mConfigPara.nJpegHeight});
            cameraParam.setJpegQuality(pContext->mConfigPara.nJpegQuality);
            cameraParam.setJpegThumbnailSize(SIZE_S{(unsigned int)pContext->mConfigPara.nJpegThumbWidth, (unsigned int)pContext->mConfigPara.nJpegThumbHeight});
            cameraParam.setJpegThumbnailQuality(pContext->mConfigPara.nJpegThumbQuality);
            pContext->mpCamera->setParameters(pContext->mScalerOutChnForLargePhoto, cameraParam);
            rc = pContext->mpCamera->takePicture(pContext->mScalerOutChnForLargePhoto, NULL, NULL, NULL, &pContext->mCameraCallbacks, &pContext->mPictureRegionCallback);
            if(rc != NO_ERROR)
            {
                aloge("fatal error! scalerOutChn[%d] take picture fail[0x%x]", pContext->mScalerOutChnForLargePhoto, rc);
            }
        }
        else if(pContext->mConfigPara.nJpegNum > 1)
        {
            pContext->mpCamera->getParameters(pContext->mScalerOutChnForLargePhoto, cameraParam);
            cameraParam.setPictureMode(TAKE_PICTURE_MODE_CONTINUOUS);
            cameraParam.setContinuousPictureNumber(pContext->mConfigPara.nJpegNum);
            cameraParam.setContinuousPictureIntervalMs(pContext->mConfigPara.nJpegInterval);
            cameraParam.setPictureSize(SIZE_S{(unsigned int)pContext->mConfigPara.nJpegWidth, (unsigned int)pContext->mConfigPara.nJpegHeight});
            cameraParam.setJpegQuality(pContext->mConfigPara.nJpegQuality);
            cameraParam.setJpegThumbnailSize(SIZE_S{(unsigned int)pContext->mConfigPara.nJpegThumbWidth, (unsigned int)pContext->mConfigPara.nJpegThumbHeight});
            cameraParam.setJpegThumbnailQuality(pContext->mConfigPara.nJpegThumbQuality);
            pContext->mpCamera->setParameters(pContext->mScalerOutChnForLargePhoto, cameraParam);

            rc = pContext->mpCamera->takePicture(pContext->mScalerOutChnForLargePhoto, NULL, NULL, NULL, &pContext->mCameraCallbacks, &pContext->mPictureRegionCallback);
            if(rc != NO_ERROR)
            {
                aloge("fatal error! scalerOutChn[%d] take picture continuous fail[0x%x]", pContext->mScalerOutChnForLargePhoto, rc);
            }
        }
        ret = cdx_sem_down_timedwait(&pContext->mSemTakePictureDone, 10*1000);
        if(0 == ret)
        {
            alogd("takingPicture done in [%d] time", i);
        }
        else if(ETIMEDOUT == ret)
        {
            aloge("fatal error! takingPicture timeout in [%d] time, jpegNum:%d", i, pContext->mConfigPara.nJpegNum);
        }
        else
        {
            aloge("fatal error! takingPicture other error[0x%x] in [%d] time", ret, i);
        }

        //3. restore preview
        rc = pContext->mpCamera->stopChannel(pContext->mScalerOutChnForLargePhoto);
        if(rc != NO_ERROR)
        {
            aloge("fatal error! stop scalerOutChn[%d] fail[0x%x]", pContext->mScalerOutChnForLargePhoto, rc);
        }
        rc = pContext->mpCamera->releaseChannel(pContext->mScalerOutChnForLargePhoto);
        if(rc != NO_ERROR)
        {
            aloge("fatal error! release scalerOutChn[%d] fail[0x%x]", pContext->mScalerOutChnForLargePhoto, rc);
        }
        pContext->mpCamera->getParameters(pContext->mScalerOutChnForPreview, cameraParam);
        cameraParam.setVideoSize(previewSize);
        cameraParam.setPreviewFrameRate(pContext->mConfigPara.nPreviewFrameRate);
        cameraParam.setDisplayFrameRate(pContext->mConfigPara.nDisplayFrameRate);
        cameraParam.setPreviewFormat(pContext->mConfigPara.ePreviewPicFormat);
        cameraParam.setVideoBufferNumber(pContext->mConfigPara.nPreviewFrameNum); 
        cameraParam.setPreviewRotation(pContext->mConfigPara.nPreviewRotation);
        cameraParam.setLostFrameNumber(pContext->mConfigPara.capture_lost_frame_cnt);
        pContext->mpCamera->setParameters(pContext->mScalerOutChnForPreview, cameraParam);//AW_MPI_VI_SetVippAttr
        pContext->mpCamera->prepareChannel(pContext->mScalerOutChnForPreview);//AW_MPI_VI_CreateVipp
        pContext->mpCamera->startRender(pContext->mScalerOutChnForPreview);
        pContext->mpCamera->startChannel(pContext->mScalerOutChnForPreview);
        ret = cdx_sem_down_timedwait(&pContext->mSemExit, 5*1000);
        if(0 == ret)
        {
            alogd("user want to exit!");
            goto _exit0;
        }
    }

    if(pContext->mConfigPara.nTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.nTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }
_exit0:
    //close camera
    alogd("EyeseeCamera::release()");
    alogd("EyeseeCamera stopPreview()");
    //pContext->mpCamera->stopRender(pContext->mScalerOutChn);
    pContext->mpCamera->stopChannel(pContext->mScalerOutChnForPreview);
    pContext->mpCamera->releaseChannel(pContext->mScalerOutChnForPreview);
    pContext->mpCamera->closeChannel(pContext->mScalerOutChnForPreview);
    pContext->mpCamera->stopChannel(pContext->mScalerOutChnForLargePhoto);
    pContext->mpCamera->releaseChannel(pContext->mScalerOutChnForLargePhoto);
    pContext->mpCamera->closeChannel(pContext->mScalerOutChnForLargePhoto);
    pContext->mpCamera->stopDevice(); 
    pContext->mpCamera->releaseDevice(); 
    EyeseeCamera::close(pContext->mpCamera);
    pContext->mpCamera = NULL;
    //close vo
    AW_MPI_VO_DisableVideoLayer(pContext->mVoLayer);
    pContext->mVoLayer = -1;
    AW_MPI_VO_RemoveOutsideVideoLayer(pContext->mUILayer);
    AW_MPI_VO_Disable(pContext->mVoDev);
    pContext->mVoDev = -1;

	//exit mpp system
    AW_MPI_SYS_Exit(); 

    delete pContext;
    pContext = NULL;
    log_quit();
    cout<<"bye, sample_Camera_LargePhoto!"<<endl;
    return result;
}

