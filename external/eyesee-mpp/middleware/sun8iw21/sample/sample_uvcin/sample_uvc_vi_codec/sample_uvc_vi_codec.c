//#define LOG_NDEBUG 0
#define LOG_TAG "sample_uvc_vi_codec"
#include <utils/plat_log.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/prctl.h>

#include <alsa/asoundlib.h>

#include <base_list.h>

#include "vo/hwdisplay.h"
#include <confparser.h>
#include <sunxi_camera_v2.h>
#include <media_common_aio.h>
#include <SystemBase.h>
#include <g2d_scale.h>
#include <plat_math.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_sys.h>
#include <mpi_uvc.h>
#include <mpi_vdec.h>
#include <mpi_vi.h>
#include <mpi_isp.h>
#include <mpi_venc.h>
#include <mpi_vo.h>
#include <mpi_ao.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>

#include "sample_uvc_vi_codec.h"
#include "sample_uvc_vi_codec_config.h"

//#define SUPPORT_RTSP_TEST

#ifdef SUPPORT_RTSP_TEST
#include <rtsp_server.h>
#endif

#define SOUND_CARD_UAC "hw:UAC1Gadget" //"hw:UAC1Gadget", "hw:Camera", "hw:UVCUAC1"
#define PERIOD_SIZE (960)
#define PERIOD_COUNT (8)
//#define DEBUG_SAVE_UAC_CAPTURE_PCM
//#define DEBUG_SAVE_UAC_CAPTURE_PCM_FILE     "/mnt/extsd/save_uac_capture.pcm"

#define SampleUvcViCodec_UVCCHN (0)
#define SampleUvcViCodec_VDECCHN (0)
#define SampleUvcViCodec_ISPDEV (0)
//#define SampleUvcViCodec_VIDEV (0)
#define SampleUvcViCodec_VICHN (0)
//#define SampleUvcViCodec_SUBVIDEV (4)
#define SampleUvcViCodec_SUBVICHN (0)
#define SampleUvcViCodec_VODEV (0)
#define SampleUvcViCodec_VOLAYER (0)
#define SampleUvcViCodec_VOCHN (0)
#define SampleUvcViCodec_VENCCHN (0)
#define SampleUvcViCodec_AIODev (0)
#define SampleUvcViCodec_AICHN (0)
#define SampleUvcViCodec_AENCCHN (0)

#define G2DFRAME_PREFIX (0x40000000)

static SampleUvcViCodecContext *gpSampleUvcViCodecContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleUvcViCodecContext != NULL)
    {
        cdx_sem_up(&gpSampleUvcViCodecContext->stSemExit);
    }
}

static int kernel_fwrite(const char *fmt, ...)
{
    FILE * stream = NULL;
    va_list ap;

    stream = fopen("/dev/kmsg", "w");
    if (!stream) {
        fprintf(stderr, "Cannot open:/dev/kmsg\n");
        return -EINVAL;
    }

    va_start(ap, fmt);
    if (vfprintf(stream, fmt, ap) == -1) {
        fprintf(stderr, "[err] vfprintf %s, ret = %d\n", fmt, errno);
        fclose(stream);
        return -EINVAL;
    }
    va_end(ap);

    fclose(stream);

    return 0;
}

int initSampleUvcViCodecContext(SampleUvcViCodecContext *pContext)
{
    int ret;
    int i;
    memset(pContext, 0, sizeof *pContext);
    ret = cdx_sem_init(&pContext->stSemExit, 0);
    if (ret != 0)
    {
        aloge("fatal error! cdx sem init fail:%d", ret);
    }
    ret = pthread_mutex_init(&pContext->stVdecFrameLock, NULL);
    if (ret!=0)
    {
        aloge("fatal error! pthread mutex init fail:%d!", ret);
    }
    INIT_LIST_HEAD(&pContext->mIdleVdecFramePairList);
    for (i=0; i<MAX_VDEC_FRAMEPAIR_NUM; i++)
    {
        VdecDoubleFrameInfoNode *pNode = (VdecDoubleFrameInfoNode *)malloc(sizeof(VdecDoubleFrameInfoNode));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
        }
        memset(pNode, 0, sizeof(VdecDoubleFrameInfoNode));
        list_add_tail(&pNode->mList, &pContext->mIdleVdecFramePairList);
    }
    INIT_LIST_HEAD(&pContext->mUsingVdecFramePairList);
    ret = message_create(&pContext->stGetUvcVdecFrameMessageQueue);
    if (ret!=0)
    {
        aloge("fatal error! message queue init fail:%d!", ret);
    }
    ret = message_create(&pContext->stGetVippFrameMessageQueue);
    if (ret!=0)
    {
        aloge("fatal error! message queue init fail:%d!", ret);
    }
    ret = message_create(&pContext->stPreviewSwitchMessageQueue);
    if (ret!=0)
    {
        aloge("fatal error! message queue init fail:%d!", ret);
    }
    ret = pthread_mutex_init(&pContext->stDisplayFrameLock, NULL);
    if (ret != 0)
    {
        aloge("fatal error! pthread mutex init fail:%d!", ret);
    }
    INIT_LIST_HEAD(&pContext->mIdleDisplayFrameList);
    INIT_LIST_HEAD(&pContext->mUsingDisplayFrameList);
    ret = message_create(&pContext->stGetStreamMessageQueue);
    if (ret!=0)
    {
        aloge("fatal error! message queue init fail:%d!", ret);
    }
    INIT_LIST_HEAD(&pContext->VencFileList);
    INIT_LIST_HEAD(&pContext->AencFileList);

    pContext->nUvcChn = MM_INVALID_CHN;
    pContext->nVdecChn = MM_INVALID_CHN;
    pContext->nIspDev = MM_INVALID_DEV;
    //pContext->nViDev = MM_INVALID_DEV;
    pContext->nViChn = MM_INVALID_CHN;
    //pContext->nSubViDev = MM_INVALID_DEV;
    pContext->nSubViChn = MM_INVALID_CHN;
    pContext->nVoDev = MM_INVALID_DEV;
    pContext->nVoLayer = MM_INVALID_LAYER;
    pContext->nVOChn = MM_INVALID_CHN;
    pContext->nVencChn = MM_INVALID_CHN;
    pContext->nAIODev = MM_INVALID_DEV;
    pContext->nAiChn = MM_INVALID_CHN;
    pContext->nAEncChn = MM_INVALID_CHN;

    pContext->nG2dDevFd = -1;
    return 0;
}

int destroySampleUvcViCodecContext(SampleUvcViCodecContext *pContext)
{
    int ret;
    cdx_sem_deinit(&pContext->stSemExit);
    ret = pthread_mutex_destroy(&pContext->stVdecFrameLock);
    if (ret != 0)
    {
        aloge("fatal error! pthread mutex destroy fail:%d", ret);
    }
    int cnt = list_count_nodes(&pContext->mUsingVdecFramePairList);
    if (cnt > 0)
    {
        aloge("fatal error! using vdec frame_pair list has [%d]nodes!", cnt);
        VdecDoubleFrameInfoNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mUsingVdecFramePairList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    if (!list_empty(&pContext->mIdleVdecFramePairList))
    {
        VdecDoubleFrameInfoNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mIdleVdecFramePairList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    message_destroy(&pContext->stGetUvcVdecFrameMessageQueue);
    message_destroy(&pContext->stGetVippFrameMessageQueue);
    message_destroy(&pContext->stPreviewSwitchMessageQueue);
    ret = pthread_mutex_destroy(&pContext->stDisplayFrameLock);
    if (ret != 0)
    {
        aloge("fatal error! pthread mutex destroy fail:%d", ret);
    }
    cnt = list_count_nodes(&pContext->mUsingDisplayFrameList);
    if (cnt > 0)
    {
        aloge("fatal error! using display frame list has [%d]nodes!", cnt);
    }
    cnt = list_count_nodes(&pContext->mIdleDisplayFrameList);
    if (cnt > 0)
    {
        aloge("fatal error! idle display frame list has [%d]nodes!", cnt);
    }
    message_destroy(&pContext->stGetStreamMessageQueue);
    cnt = list_count_nodes(&pContext->VencFileList);
    if (cnt > 0)
    {
        aloge("fatal error! venc file list has [%d]nodes!", cnt);
    }
    cnt = list_count_nodes(&pContext->AencFileList);
    if (cnt > 0)
    {
        aloge("fatal error! aenc file list has [%d]nodes!", cnt);
    }
    
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleUvcViCodecCmdLineParam *pCmdLinePara)
{
    //alogd("sample_region input path is : %s", argv[0]);
    int ret = 0;
    int i = 1;
    memset(pCmdLinePara, 0, sizeof *pCmdLinePara);
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if((++i) >= argc)
            {
                aloge("fatal error!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error!");
            }
            strncpy(pCmdLinePara->strConfigFilePath, argv[i], strlen(argv[i]));
            pCmdLinePara->strConfigFilePath[strlen(argv[i])] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
             alogd("CmdLine param example:\n"
                "\t run -path /mnt/extsd/sample_uvc_vi_codec.conf\n");
             ret = 1;
             break;
        }
        else
        {
            alogd("CmdLine param example:\n"
                "\t run -path /mnt/extsd/sample_uvc_vi_codec.conf\n");
        }
        ++i;
    }
    return ret;
}

static enum v4l2_colorspace convertString2ColorSpace(char *ptr)
{
    enum v4l2_colorspace eColorSpace;
    if (!strcmp(ptr, "jpeg"))
    {
        eColorSpace = V4L2_COLORSPACE_JPEG;
    }
    else if (!strcmp(ptr, "rec709"))
    {
        eColorSpace = V4L2_COLORSPACE_REC709;
    }
    else if (!strcmp(ptr, "rec709_part_range"))
    {
        eColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
    }
    else
    {
        aloge("fatal error! unknown color space:%s", ptr);
        eColorSpace = V4L2_COLORSPACE_REC709;
    }
    return eColorSpace;
}

static PIXEL_FORMAT_E convertString2PixelFormat(char *ptr)
{
    PIXEL_FORMAT_E ePixelFormat;
    if (!strcmp(ptr, "nv12"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(ptr, "nv21"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(ptr, "lbc20x"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if (!strcmp(ptr, "lbc25x"))
    {
        ePixelFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else
    {
        aloge("fatal error! unknown pixel format:%s", ptr);
        ePixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    return ePixelFormat;
}

static PAYLOAD_TYPE_E convertString2PAYLOAD_TYPE_E(char *ptr)
{
    PAYLOAD_TYPE_E ePayloadType;
    if (!strcmp(ptr, "h264"))
    {
        ePayloadType = PT_H264;
    }
    else if (!strcmp(ptr, "h265"))
    {
        ePayloadType = PT_H265;
    }
    else if (!strcmp(ptr, "mjpeg"))
    {
        ePayloadType = PT_MJPEG;
    }
    else if (!strcmp(ptr, "aac"))
    {
        ePayloadType = PT_AAC;
    }
    else if (!strcmp(ptr, "pcm"))
    {
        ePayloadType = PT_PCM_AUDIO;
    }
    else
    {
        ePayloadType = PT_MAX;
    }
    return ePayloadType;
}

static ROTATE_E convertRotateDegree2ROTATE_E(int rotateDegree)
{
    ROTATE_E eRotate;
    switch (rotateDegree)
    {
    case 0:
    {
        eRotate = ROTATE_NONE;
        break;
    }
    case 90:
    {
        eRotate = ROTATE_90;
        break;
    }
    case 180:
    {
        eRotate = ROTATE_180;
        break;
    }
    case 270:
    {
        eRotate = ROTATE_270;
        break;
    }
    default:
    {
        eRotate = ROTATE_NONE;
        break;
    }
    }
    return eRotate;
}

static int LoadSampleUvcViCodecConfig(SampleUvcViCodecConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;
    memset(pConfig, 0, sizeof(SampleUvcViCodecConfig));
    strcpy(pConfig->strUvcDevName, "/dev/video1");
    pConfig->eUvcColorSpace = V4L2_COLORSPACE_REC709;
    pConfig->nUvcCaptureFrameRate = 20;
    pConfig->nUvcCaptureWidth = 1280;
    pConfig->nUvcCaptureHeight = 720;
    pConfig->nUvcCaptureVideoBufCnt = 2;
    pConfig->fUvcCaptureMaxFramesizeRatio = 0.2;
    pConfig->nVdecExtraFrameNum = 1;
    pConfig->eVdecPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pConfig->nVdecSubRatio = 1;
    pConfig->nUvcDisplayRotate = 90;
    pConfig->nUvcDisplayX = 0;
    pConfig->nUvcDisplayY = 0;
    pConfig->nUvcDisplayWidth = 480;
    pConfig->nUvcDisplayHeight = 854;

    pConfig->eIspColorSpace = V4L2_COLORSPACE_REC709;
    pConfig->nIspCaptureFrameRate = 30;
    pConfig->nVippDev = 0;
    pConfig->eVippPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pConfig->nVippCaptureWidth = 1280;
    pConfig->nVippCaptureHeight = 720;
    pConfig->nVippBufNum = 3;
    pConfig->nSubVippDev = 4;
    pConfig->eSubVippPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pConfig->nSubVippCaptureWidth = 960;
    pConfig->nSubVippCaptureHeight = 540;
    pConfig->nSubVippBufNum = 3;
    pConfig->nSubVippDisplayRotate = 90;
    pConfig->nSubVippDisplayX = 0;
    pConfig->nSubVippDisplayY = 0;
    pConfig->nSubVippDisplayWidth = 480;
    pConfig->nSubVippDisplayHeight = 854;

    pConfig->ePreviewSource = PreviewSource_UVC;
    pConfig->nPreviewSwitchInterval = 10;

    pConfig->eVencType = PT_H264;
    pConfig->RcMode = 1;
    pConfig->vbrOptEn = 1;
    pConfig->eVbrOptRcPriority = VENC_RC_RT_BIT_RATE_FIRST;
    pConfig->eVbrOptRcQualityLevel = VENC_QUALITY_LOW_LEVEL;
    strcpy(pConfig->strVencFilePath, "/mnt/extsd/video.h264");
    pConfig->nVencFileDuration = 60;
    pConfig->nVencFileNum = 3;
    pConfig->nRtspNetType = 3;
    pConfig->nRtspId = -1;

    pConfig->nUvcKeyFrameInterval = 60;
    pConfig->nUvcVideoBitrate = 1048576;
    pConfig->nUvcEncodeRotate = 90;

    pConfig->nVippKeyFrameInterval = 90;
    pConfig->nVippVideoBitrate = 2097152;
    pConfig->nVippEncodeRotate = 90;
    pConfig->nVippCropX = 0;
    pConfig->nVippCropY = 0;
    pConfig->nVippCropWidth = 0;
    pConfig->nVippCropHeight = 0;
    pConfig->nVippEncodeWidth = 1280;
    pConfig->nVippEncodeHeight = 720;
    pConfig->bVippEncodeSharpEn = true;
    pConfig->bVippIsp2VeLinkEn = true;
    pConfig->bVippVe2IspLinkEn = true;
    pConfig->bVippRegionLinkEnable = true;
    pConfig->bVippRegionLinkTexDetectEnable = true;
    pConfig->bVippRegionLinkMotionDetectEnable = true;
    pConfig->nVippRegionLinkMotionDetectInterval = 0;

    pConfig->nSampleRate = 16000;
    pConfig->nAiVolume = 100;
    pConfig->nMicNum = 1;
    pConfig->bAiAec = true;
    pConfig->bAiAns = true;
    pConfig->bAiAgc = true;

    pConfig->eAencType = PT_AAC;
    pConfig->bAencAttachAACHeader = true;
    strcpy(pConfig->strAencFilePath, "/mnt/extsd/audio.aac");
    pConfig->nAencFileDuration = 60;
    pConfig->nAencFileNum = 3;

    pConfig->nTestDuration = 90;

    if(!conf_path)
    {
        alogd("user not set config file. use default test parameter!");
    }
    else
    {
        CONFPARSER_S stConfParser;
        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail!");
            return FAILURE;
        }

        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_DEV, NULL);
        if(ptr)
        {
            strcpy(pConfig->strUvcDevName, ptr);
        }
        else
        {
            aloge("fatal error! the uvc dev name is error!");
        }
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_CAPTURE_COLORSPACE, NULL);
        pConfig->eUvcColorSpace = convertString2ColorSpace(ptr);
        pConfig->nUvcCaptureFrameRate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_CAPTURE_FRAMERATE, 0);
        pConfig->nUvcCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_CAPTURE_WIDTH, 0);
        pConfig->nUvcCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_CAPTURE_HEIGHT, 0);
        pConfig->nUvcCaptureVideoBufCnt = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_CAPTURE_VIDEOBUFCNT, 0);
        pConfig->fUvcCaptureMaxFramesizeRatio = GetConfParaDouble(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_CAPTURE_MAXFRAMESIZE_RATIO, 1.0);
        pConfig->nVdecExtraFrameNum = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VDEC_EXTRA_FRAME_NUM, 0);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VDEC_PIXEL_FORMAT, NULL);
        pConfig->eVdecPixelFormat = convertString2PixelFormat(ptr);
        pConfig->nVdecSubRatio = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VDEC_SUB_RATIO, 0);
        pConfig->nUvcDisplayRotate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_DISPLAY_ROTATE, 0);
        pConfig->nUvcDisplayX = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_DISPLAY_X, 0);
        pConfig->nUvcDisplayX = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_DISPLAY_Y, 0);
        pConfig->nUvcDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_DISPLAY_WIDTH, 0);
        pConfig->nUvcDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_DISPLAY_HEIGHT, 0);

        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_ISP_COLOR_SPACE, NULL);
        pConfig->eIspColorSpace = convertString2ColorSpace(ptr);
        pConfig->nIspCaptureFrameRate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_ISP_CAPTURE_FRAMERATE, 0);
        pConfig->nVippDev = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_DEV, 0);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_PIXEL_FORMAT, NULL);
        pConfig->eVippPixelFormat = convertString2PixelFormat(ptr);
        pConfig->nVippCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_CAPTURE_WIDTH, 0);
        pConfig->nVippCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_CAPTURE_HEIGHT, 0);
        pConfig->nVippBufNum = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_BUF_NUM, 0);
        pConfig->nSubVippDev = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_DEV, 0);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_PIXEL_FORMAT, NULL);
        pConfig->eSubVippPixelFormat = convertString2PixelFormat(ptr);
        pConfig->nSubVippCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_CAPTURE_WIDTH, 0);
        pConfig->nSubVippCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_CAPTURE_HEIGHT, 0);
        pConfig->nSubVippBufNum = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_BUF_NUM, 0);
        pConfig->nSubVippDisplayRotate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_DISPLAY_ROTATE, 0);
        pConfig->nSubVippDisplayX = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_DISPLAY_X, 0);
        pConfig->nSubVippDisplayY = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_DISPLAY_Y, 0);
        pConfig->nSubVippDisplayWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_DISPLAY_WIDTH, 0);
        pConfig->nSubVippDisplayHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SUB_VIPP_DISPLAY_HEIGHT, 0);

        pConfig->ePreviewSource = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_PREVIEW_SOURCE, 0);
        pConfig->nPreviewSwitchInterval = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_PREVIEW_SWITCH_INTERVAL, 0);

        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VENC_TYPE, NULL);
        pConfig->eVencType = convertString2PAYLOAD_TYPE_E(ptr);
        pConfig->RcMode = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_RC_MODE, 0);
        pConfig->vbrOptEn = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VBR_OPT_EN, 0);
        pConfig->eVbrOptRcPriority = (VENC_RC_PRIORITY)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VBR_OPT_RC_PRIORITY, 0);
        pConfig->eVbrOptRcQualityLevel = (VENC_QUALITY_LEVEL)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VBR_OPT_RC_QUALITYLEVEL, 0);

        pConfig->nUvcKeyFrameInterval = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_KEY_FRAME_INTERVAL, 0);
        pConfig->nUvcVideoBitrate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_VIDEO_BITRATE, 0);
        pConfig->nUvcEncodeRotate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_UVC_ENCODE_ROTATE, 0);

        pConfig->nVippKeyFrameInterval = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_KEY_FRAME_INTERVAL, 0);
        pConfig->nVippVideoBitrate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_VIDEO_BITRATE, 0);
        pConfig->nVippEncodeRotate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_ENCODE_ROTATE, 0);
        pConfig->nVippCropX = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_CROP_X, 0);
        pConfig->nVippCropY = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_CROP_Y, 0);
        pConfig->nVippCropWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_CROP_WIDTH, 0);
        pConfig->nVippCropHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_CROP_HEIGHT, 0);
        pConfig->nVippEncodeWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_ENCODE_WIDTH, 0);
        pConfig->nVippEncodeHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_ENCODE_HEIGHT, 0);
        pConfig->bVippEncodeSharpEn = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_ENCODE_SHARP_EN, 0);
        pConfig->bVippIsp2VeLinkEn = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_ISP2VE_LINK_EN, 0);
        pConfig->bVippVe2IspLinkEn = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_VE2ISP_LINK_EN, 0);
        pConfig->bVippRegionLinkEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_REGION_LINK_ENABLE, 0);
        pConfig->bVippRegionLinkTexDetectEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_REGION_LINK_TEX_DETECT_ENABLE, 0);
        pConfig->bVippRegionLinkMotionDetectEnable = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_REGION_LINK_MOTION_DETECT_ENABLE, 0);
        pConfig->nVippRegionLinkMotionDetectInterval = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VIPP_REGION_LINK_MOTION_DETECT_INTERVAL, 0);

        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VENC_FILE_PATH, NULL);
        if (ptr)
        {
            strcpy(pConfig->strVencFilePath, ptr);
        }
        else
        {
            aloge("fatal error! venc file path is NULL!");
        }
        pConfig->nVencFileDuration = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VENC_FILE_DURATION, 0);
        pConfig->nVencFileNum = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_VENC_FILE_NUM, 0);
        pConfig->nRtspNetType = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_RTSP_NET_TYPE, 0);
        pConfig->nRtspId = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_RTSP_ID, 0);

        pConfig->nSampleRate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_SAMPLE_RATE, 0);
        pConfig->nAiVolume = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_AI_VOLUME, 0);
        pConfig->nMicNum = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_MIC_NUM, 0);
        pConfig->bAiAec = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_AEC_EN, 0);
        pConfig->bAiAns = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_ANS_EN, 0);
        pConfig->bAiAgc = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_AGC_EN, 0);

        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_AENC_TYPE, NULL);
        pConfig->eAencType = convertString2PAYLOAD_TYPE_E(ptr);
        pConfig->bAencAttachAACHeader = (bool)GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_AENC_ATTACHAACHEADER, 0);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_AENC_FILE_PATH, NULL);
        if (ptr)
        {
            strcpy(pConfig->strAencFilePath, ptr);
        }
        else
        {
            aloge("fatal error! aenc file path is NULL!");
        }
        pConfig->nAencFileDuration = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_AENC_FILE_DURATION, 0);
        pConfig->nAencFileNum = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_AENC_FILE_NUM, 0);

        pConfig->nTestDuration = GetConfParaInt(&stConfParser, SAMPLE_UVC_VI_CODEC_KEY_TEST_DURATION, 0);

        destroyConfParser(&stConfParser);
    }
    alogd("uvcDev[%s] captureParam:[%d-%d-%dx%d-%d-%lf],vdec[%d-%d-%d],uvcDisplay[%d-%d-%d-%dx%d],isp[%d-%d],"
        "vipp[%d][%d-%dx%d-%d],vipp[%d][%d-%dx%d-%d],vippDisplay[%d-%d-%d-%dx%d],previewMethod[%d-%d],venc[%d-%d-%d-%d-%d],"
        "uvcVenc[%d-%d-%d],vippVenc[%d-%d-%d,%d-%d-%dx%d,%dx%d,%d-%d-%d-%d-%d-%d-%d],vencFile[%s-%d-%d-%d-%d],"
        "ai[%d-%d-%d-%d-%d-%d], aenc[%d-%d], aencFile[%s-%d-%d], testDuration[%d]", pConfig->strUvcDevName,
        pConfig->eUvcColorSpace, pConfig->nUvcCaptureFrameRate, pConfig->nUvcCaptureWidth, pConfig->nUvcCaptureHeight,
        pConfig->nUvcCaptureVideoBufCnt, pConfig->fUvcCaptureMaxFramesizeRatio, pConfig->nVdecExtraFrameNum,
        pConfig->eVdecPixelFormat, pConfig->nVdecSubRatio, pConfig->nUvcDisplayRotate, pConfig->nUvcDisplayX,
        pConfig->nUvcDisplayY, pConfig->nUvcDisplayWidth, pConfig->nUvcDisplayHeight, pConfig->eIspColorSpace,
        pConfig->nIspCaptureFrameRate, pConfig->nVippDev, pConfig->eVippPixelFormat, pConfig->nVippCaptureWidth,
        pConfig->nVippCaptureHeight, pConfig->nVippBufNum, pConfig->nSubVippDev, pConfig->eSubVippPixelFormat,
        pConfig->nSubVippCaptureWidth, pConfig->nSubVippCaptureHeight, pConfig->nSubVippBufNum, pConfig->nSubVippDisplayRotate,
        pConfig->nSubVippDisplayX, pConfig->nSubVippDisplayY, pConfig->nSubVippDisplayWidth, pConfig->nSubVippDisplayHeight,
        pConfig->ePreviewSource, pConfig->nPreviewSwitchInterval, pConfig->eVencType, pConfig->RcMode, pConfig->vbrOptEn,
        pConfig->eVbrOptRcPriority, pConfig->eVbrOptRcQualityLevel, pConfig->nUvcKeyFrameInterval, pConfig->nUvcVideoBitrate,
        pConfig->nUvcEncodeRotate, pConfig->nVippKeyFrameInterval, pConfig->nVippVideoBitrate, pConfig->nVippEncodeRotate,
        pConfig->nVippCropX, pConfig->nVippCropY, pConfig->nVippCropWidth, pConfig->nVippCropHeight, pConfig->nVippEncodeWidth,
        pConfig->nVippEncodeHeight, pConfig->bVippEncodeSharpEn, pConfig->bVippIsp2VeLinkEn, pConfig->bVippVe2IspLinkEn,
        pConfig->bVippRegionLinkEnable, pConfig->bVippRegionLinkTexDetectEnable, pConfig->bVippRegionLinkMotionDetectEnable,
        pConfig->nVippRegionLinkMotionDetectInterval, pConfig->strVencFilePath, pConfig->nVencFileDuration,
        pConfig->nVencFileNum, pConfig->nRtspNetType, pConfig->nRtspId, pConfig->nSampleRate, pConfig->nAiVolume,
        pConfig->nMicNum, pConfig->bAiAec, pConfig->bAiAns, pConfig->bAiAgc, pConfig->eAencType, pConfig->bAencAttachAACHeader,
        pConfig->strAencFilePath, pConfig->nAencFileDuration, pConfig->nAencFileNum, pConfig->nTestDuration);
    return 0;
}

static int ReleaseG2dVideoFrame(SampleUvcViCodecContext *pContext, unsigned int nFrameId)
{
    DisplayFrameInfoNode *pNode = NULL;
    pthread_mutex_lock(&pContext->stDisplayFrameLock);
    DisplayFrameInfoNode *pEntry;
    list_for_each_entry(pEntry, &pContext->mUsingDisplayFrameList, mList)
    {
        if (pEntry->stFrame.mId == nFrameId)
        {
            pNode = pEntry;
            break;
        }
    }
    if (NULL == pNode)
    {
        aloge("fatal error! why not find frameId[%d]?", nFrameId);
        pthread_mutex_unlock(&pContext->stDisplayFrameLock);
        return -1;
    }
    pNode->nRefCnt--;
    if (pNode->nRefCnt < 0)
    {
        aloge("fatal error! RefCnt[%d] wrong!", pNode->nRefCnt);
    }
    if (0 == pNode->nRefCnt)
    {
        list_move_tail(&pNode->mList, &pContext->mIdleDisplayFrameList);
    }
    pthread_mutex_unlock(&pContext->stDisplayFrameLock);
    return 0;
    
}

static int ReleaseVdecSubVideoFrame(SampleUvcViCodecContext *pContext, unsigned int nFrameId)
{
    ERRORTYPE ret;
    VdecDoubleFrameInfoNode *pNode = NULL;
    pthread_mutex_lock(&pContext->stVdecFrameLock);
    VdecDoubleFrameInfoNode *pEntry = NULL;
    list_for_each_entry(pEntry, &pContext->mUsingVdecFramePairList, mList)
    {
        if (pEntry->stSubFrame.mId == nFrameId)
        {
            pNode = pEntry;
            break;
        }
    }
    if (NULL == pNode)
    {
        aloge("fatal error! why not find frameId[%d]?", nFrameId);
        pthread_mutex_unlock(&pContext->stVdecFrameLock);
        return -1;
    }
    pNode->nSubRefCnt--;
    if(pNode->nSubRefCnt < 0)
    {
        aloge("fatal error! subRefCnt[%d] wrong!", pNode->nSubRefCnt);
    }
    if((0 == pNode->nMainRefCnt) && (0 == pNode->nSubRefCnt))
    {
        ret = AW_MPI_VDEC_ReleaseDoubleImage(pContext->nVdecChn, &pNode->stMainFrame, &pNode->stSubFrame);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why release frame to vdecVhn[%d] fail[0x%x]?", pContext->nVdecChn, ret);
        }
        list_move_tail(&pNode->mList, &pContext->mIdleVdecFramePairList);
    }
    pthread_mutex_unlock(&pContext->stVdecFrameLock);
    return 0;
}

static int ReleaseVdecMainVideoFrame(SampleUvcViCodecContext *pContext, unsigned int nFrameId)
{
    ERRORTYPE ret;
    VdecDoubleFrameInfoNode *pNode = NULL;
    pthread_mutex_lock(&pContext->stVdecFrameLock);
    VdecDoubleFrameInfoNode *pEntry = NULL;
    list_for_each_entry(pEntry, &pContext->mUsingVdecFramePairList, mList)
    {
        if (pEntry->stMainFrame.mId == nFrameId)
        {
            pNode = pEntry;
            break;
        }
    }
    if (NULL == pNode)
    {
        aloge("fatal error! why not find frameId[%d]?", nFrameId);
        pthread_mutex_unlock(&pContext->stVdecFrameLock);
        return -1;
    }
    pNode->nMainRefCnt--;
    if(pNode->nMainRefCnt < 0)
    {
        aloge("fatal error! mainRefCnt[%d] wrong!", pNode->nMainRefCnt);
    }
    if((0 == pNode->nMainRefCnt) && (0 == pNode->nSubRefCnt))
    {
        ret = AW_MPI_VDEC_ReleaseDoubleImage(pContext->nVdecChn, &pNode->stMainFrame, &pNode->stSubFrame);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why release frame to vdecVhn[%d] fail[0x%x]?", pContext->nVdecChn, ret);
        }
        list_move_tail(&pNode->mList, &pContext->mIdleVdecFramePairList);
    }
    pthread_mutex_unlock(&pContext->stVdecFrameLock);
    return 0;
}

static ERRORTYPE SampleUvcViCodec_MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event,
    void *pEventData)
{
    int result;
    ERRORTYPE ret = SUCCESS;
    SampleUvcViCodecContext *pContext = (SampleUvcViCodecContext*)cookie;
    if (MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
        case MPP_EVENT_VI_TIMEOUT:
        {
            aloge("viChn[%d-%d] receive vi timeout.", pChn->mDevId, pChn->mChnId);
            break;
        }
        default:
            aloge("fatal error! unknow vi event type[0x%x]", event);
            break;
        }
    }
    else if(MOD_ID_VOU == pChn->mModId)
    {
        if(!(pChn->mDevId == pContext->nVoLayer && pChn->mChnId == pContext->nVOChn))
        {
            aloge("fatal error! voChn[%d-%d] != [%d-%d]", pChn->mDevId, pChn->mChnId, pContext->nVoLayer, pContext->nVOChn);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                if (pVideoFrameInfo->mId & G2DFRAME_PREFIX) //g2d display frame.
                {
                    result = ReleaseG2dVideoFrame(pContext, pVideoFrameInfo->mId);
                    if (result != 0)
                    {
                        aloge("fatal error! vo release g2d frameId[%d] fail:%d", pVideoFrameInfo->mId, result);
                    }
                }
                else
                {
                    MOD_ID_E eModId = (pVideoFrameInfo->VFrame.mWhoSetFlag>>16) & 0xFF;
                    if (MOD_ID_VIU == eModId)
                    {
                        aloge("fatal error! impossible because vi->vo will binding if not rotate.");
                        result = -1;
                    }
                    else //vdec main frame.
                    {
                        result = ReleaseVdecMainVideoFrame(pContext, pVideoFrameInfo->mId);
                        if (result != 0)
                        {
                            aloge("fatal error! vo release vdec main frameId[%d] fail:%d", pVideoFrameInfo->mId, result);
                        }
                    }
                }
                if (result != 0)
                {
                    aloge("fatal error! voChn[%d-%d] release frameId[%d] fail:%d", pChn->mDevId, pChn->mChnId, pVideoFrameInfo->mId,
                        result);
                    ret = FAILURE;
                }
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("voChn[%d-%d] report video display size[%dx%d]", pChn->mDevId, pChn->mChnId, pDisplaySize->Width,
                    pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                aloge("voChn[%d-%d] report rendering start", pChn->mDevId, pChn->mChnId);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[%d-%d-%d]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN nVEncChn = pChn->mChnId;
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                if(pVideoFrameInfo != NULL)
                {
                    result = ReleaseVdecMainVideoFrame(pContext, pVideoFrameInfo->mId);
                    if (result != 0)
                    {
                        aloge("fatal error! vencChn[%d] release frameId[%d] fail:%d", pChn->mChnId, pVideoFrameInfo->mId, result);
                        ret = FAILURE;
                    }
                }
                break;
            }
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM_EXTRA:
            {
                VENC_Isp2VeExtraParam *pParam = (VENC_Isp2VeExtraParam *)pEventData;
                ret = FAILURE;
                break;
            }
            default:
            {
                aloge("fatal error! vencChn[%d] receive unknown event:%d", pChn->mChnId, event);
                ret = ERR_VENC_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else if (MOD_ID_AI == pChn->mModId)
    {
        if(pChn->mChnId != pContext->nAiChn)
        {
            aloge("fatal error! AI chnId[%d]!=[%d]", pChn->mChnId, pContext->nAiChn);
        }
        switch(event)
        {
            case MPP_EVENT_CAPTURE_AUDIO_DATA:
            {
                AISendDataInfo *pDataInfo = (AISendDataInfo*)pEventData;
                //alogd("AIChannel transport pcm:%d bytes, ignore:%d, pts:%lldms", pDataInfo->mLen, pDataInfo->mbIgnore, pDataInfo->mPts/1000);
                break;
            }
            default:
            {
                aloge("fatal error! aiChn[%d] receive unknown event[0x%x]!", pChn->mChnId, event);
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! why modId[0x%x]?", pChn->mModId);
        ret = FAILURE;
    }
    return ret;
}

static ERRORTYPE configVdecChnAttr(SampleUvcViCodecContext *pContext)
{
    memset(&pContext->stVdecChnAttr, 0, sizeof(pContext->stVdecChnAttr));
    pContext->stVdecChnAttr.mType = PT_MJPEG;
    pContext->stVdecChnAttr.mBufSize = AWALIGN(pContext->stConfig.nUvcCaptureWidth*pContext->stConfig.nUvcCaptureHeight*3/2,
        1024);
    pContext->stVdecChnAttr.mPicWidth = 0;
    pContext->stVdecChnAttr.mPicHeight = 0;
    pContext->stVdecChnAttr.mInitRotation = ROTATE_NONE;
    pContext->stVdecChnAttr.mOutputPixelFormat = pContext->stConfig.eVdecPixelFormat;
    if (pContext->stConfig.nVdecSubRatio > 0)
    {
        pContext->stVdecChnAttr.mSubPicEnable = TRUE;
        pContext->stVdecChnAttr.mSubPicWidthRatio = pContext->stConfig.nVdecSubRatio;
        pContext->stVdecChnAttr.mSubPicHeightRatio = pContext->stConfig.nVdecSubRatio;
        pContext->stVdecChnAttr.mSubOutputPixelFormat = pContext->stConfig.eVdecPixelFormat;
    }
    pContext->stVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
    pContext->stVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 1;
    if (pContext->stConfig.nVdecExtraFrameNum >= 0)
    {
        pContext->stVdecChnAttr.bEnableExtraFrameNum = TRUE;
        pContext->stVdecChnAttr.mExtraFrameNum = pContext->stConfig.nVdecExtraFrameNum;
    }
    return SUCCESS;
}

static ERRORTYPE configUvcVencChnAttr(SampleUvcViCodecContext *pContext)
{
    memset(&pContext->stUvcVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    pContext->stUvcVencChnAttr.VeAttr.Type = pContext->stConfig.eVencType;
    if (PT_H264 == pContext->stUvcVencChnAttr.VeAttr.Type)
    {
        pContext->stUvcVencChnAttr.VeAttr.AttrH264e.BufSize = 0;
        pContext->stUvcVencChnAttr.VeAttr.AttrH264e.mThreshSize = pContext->stConfig.nUvcVideoBitrate/8
            /pContext->stConfig.nUvcCaptureFrameRate*15;
        pContext->stUvcVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->stUvcVencChnAttr.VeAttr.AttrH264e.Profile = 2;
        pContext->stUvcVencChnAttr.VeAttr.AttrH264e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stUvcVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->stConfig.nUvcCaptureWidth;
        pContext->stUvcVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->stConfig.nUvcCaptureHeight;
        pContext->stUvcVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
    }
    else if (PT_H265 == pContext->stUvcVencChnAttr.VeAttr.Type)
    {
        pContext->stUvcVencChnAttr.VeAttr.AttrH265e.mBufSize = 0;
        pContext->stUvcVencChnAttr.VeAttr.AttrH265e.mThreshSize = pContext->stConfig.nUvcVideoBitrate/8
            /pContext->stConfig.nUvcCaptureFrameRate*15;
        pContext->stUvcVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->stUvcVencChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->stUvcVencChnAttr.VeAttr.AttrH265e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stUvcVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->stConfig.nUvcCaptureWidth;
        pContext->stUvcVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->stConfig.nUvcCaptureHeight;
        pContext->stUvcVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
    }
    else if (PT_MJPEG == pContext->stUvcVencChnAttr.VeAttr.Type)
    {
        pContext->stUvcVencChnAttr.VeAttr.AttrMjpeg.mBufSize = 0;
        pContext->stUvcVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = 0;
        pContext->stUvcVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->stUvcVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->stConfig.nUvcCaptureWidth;
        pContext->stUvcVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->stConfig.nUvcCaptureHeight;
    }
    pContext->stUvcVencChnAttr.VeAttr.MaxKeyInterval = pContext->stConfig.nUvcKeyFrameInterval;
    //mpi_vdec output width and height are all 32 align, so mpi_venc must config to 32 align
    pContext->stUvcVencChnAttr.VeAttr.SrcPicWidth  = AWALIGN(pContext->stConfig.nUvcCaptureWidth, 32);
    pContext->stUvcVencChnAttr.VeAttr.SrcPicHeight = AWALIGN(pContext->stConfig.nUvcCaptureHeight, 32);
    pContext->stUvcVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->stUvcVencChnAttr.VeAttr.PixelFormat = pContext->stConfig.eVdecPixelFormat;
    pContext->stUvcVencChnAttr.VeAttr.mColorSpace = pContext->stConfig.eUvcColorSpace;
    pContext->stUvcVencChnAttr.VeAttr.Rotate = convertRotateDegree2ROTATE_E(pContext->stConfig.nUvcEncodeRotate);
    pContext->stUvcVencChnAttr.VeAttr.mDropFrameNum = 0;
    pContext->stUvcVencChnAttr.VeAttr.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT;
    pContext->stUvcVencChnAttr.VeAttr.mVeRecRefBufReduceEnable = 0;
    pContext->stUvcVencChnAttr.VeAttr.mVbrOptEnable = pContext->stConfig.vbrOptEn;
    alogd("pixfmt:0x%x, colorSpace:0x%x, VeRefFrameLbcMode:%d, VeRecRefBufReduceEnable:%d, VbrOptEnable:%d",
        pContext->stUvcVencChnAttr.VeAttr.PixelFormat, pContext->stUvcVencChnAttr.VeAttr.mColorSpace,
        pContext->stUvcVencChnAttr.VeAttr.mVeRefFrameLbcMode, pContext->stUvcVencChnAttr.VeAttr.mVeRecRefBufReduceEnable,
        pContext->stUvcVencChnAttr.VeAttr.mVbrOptEnable);
    if (PT_H264 == pContext->stUvcVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfig.RcMode)
        {
        case 1:
            pContext->stUvcVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->stConfig.nUvcVideoBitrate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            break;
        case 2:
            pContext->stUvcVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 35;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 35;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264FixQp.mSrcFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264FixQp.mDstFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            break;
        case 0:
        default:
            pContext->stUvcVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->stConfig.nUvcVideoBitrate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            break;
        }
    }
    else if (PT_H265 == pContext->stUvcVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfig.RcMode)
        {
        case 1:
            pContext->stUvcVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->stConfig.nUvcVideoBitrate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            break;
        case 2:
            pContext->stUvcVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 35;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 35;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265FixQp.mSrcFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265FixQp.mDstFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            break;
        case 0:
        default:
            pContext->stUvcVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->stConfig.nUvcVideoBitrate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            break;
        }
    }
    else if (PT_MJPEG == pContext->stUvcVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfig.RcMode)
        {
        case 2:
            pContext->stUvcVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            break;
        case 0:
        default:
            pContext->stUvcVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->stConfig.nUvcVideoBitrate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = pContext->stConfig.nUvcCaptureFrameRate;
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.bitRateMax = (int)((float)pContext->stConfig.nUvcVideoBitrate*1.2);
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.bitRateMin = (int)((float)pContext->stConfig.nUvcVideoBitrate*0.8);
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.fRangeRatioTh = 0.05;
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nQualityTh = 85;
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nMinQuality = 10;
            pContext->stUvcVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nMaxQuality = 100;
            break;
        }
    }
    pContext->stUvcVencChnAttr.RcAttr.mProductMode = PRODUCT_STATIC_IPC;
    pContext->stUvcVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->stUvcVencChnAttr.GopAttr.mGopSize = 2;
    pContext->stUvcVencChnAttr.EncppAttr.eEncppSharpSetting = VencEncppSharp_Disable;

    memset(&pContext->stUvcVencRcParam, 0, sizeof(VENC_RC_PARAM_S));
    if (VENC_RC_MODE_H264CBR == pContext->stUvcVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stUvcVencRcParam.ParamH264Cbr.mMaxQp = 45;
        pContext->stUvcVencRcParam.ParamH264Cbr.mMinQp = 25;
        pContext->stUvcVencRcParam.ParamH264Cbr.mMaxPqp = 45;
        pContext->stUvcVencRcParam.ParamH264Cbr.mMinPqp = 25;
        pContext->stUvcVencRcParam.ParamH264Cbr.mQpInit = 37;
        pContext->stUvcVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 1;
    }
    else if (VENC_RC_MODE_H264VBR == pContext->stUvcVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stUvcVencRcParam.ParamH264Vbr.mMaxQp = 45;
        pContext->stUvcVencRcParam.ParamH264Vbr.mMinQp = 25;
        pContext->stUvcVencRcParam.ParamH264Vbr.mMaxPqp = 45;
        pContext->stUvcVencRcParam.ParamH264Vbr.mMinPqp = 25;
        pContext->stUvcVencRcParam.ParamH264Vbr.mQpInit = 37;
        pContext->stUvcVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 1;
        pContext->stUvcVencRcParam.ParamH264Vbr.mMovingTh = 20;
        pContext->stUvcVencRcParam.ParamH264Vbr.mQuality = 5;
        pContext->stUvcVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 15;
        pContext->stUvcVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    }
    else if (VENC_RC_MODE_H265CBR == pContext->stUvcVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stUvcVencRcParam.ParamH265Cbr.mMaxQp = 45;
        pContext->stUvcVencRcParam.ParamH265Cbr.mMinQp = 25;
        pContext->stUvcVencRcParam.ParamH265Cbr.mMaxPqp = 45;
        pContext->stUvcVencRcParam.ParamH265Cbr.mMinPqp = 25;
        pContext->stUvcVencRcParam.ParamH265Cbr.mQpInit = 37;
        pContext->stUvcVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 1;
    }
    else if (VENC_RC_MODE_H265VBR == pContext->stUvcVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stUvcVencRcParam.ParamH265Vbr.mMaxQp = 45;
        pContext->stUvcVencRcParam.ParamH265Vbr.mMinQp = 25;
        pContext->stUvcVencRcParam.ParamH265Vbr.mMaxPqp = 45;
        pContext->stUvcVencRcParam.ParamH265Vbr.mMinPqp = 25;
        pContext->stUvcVencRcParam.ParamH265Vbr.mQpInit = 37;
        pContext->stUvcVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 1;
        pContext->stUvcVencRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->stUvcVencRcParam.ParamH265Vbr.mQuality = 5;
        pContext->stUvcVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 15;
        pContext->stUvcVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }

    return SUCCESS;
}

static ERRORTYPE configVippVencChnAttr(SampleUvcViCodecContext *pContext)
{
    memset(&pContext->stVippVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    pContext->stVippVencChnAttr.VeAttr.Type = pContext->stConfig.eVencType;
    if (PT_H264 == pContext->stVippVencChnAttr.VeAttr.Type)
    {
        pContext->stVippVencChnAttr.VeAttr.AttrH264e.BufSize = 0;
        pContext->stVippVencChnAttr.VeAttr.AttrH264e.mThreshSize = pContext->stConfig.nVippVideoBitrate/8
            /pContext->stConfig.nIspCaptureFrameRate*15;
        pContext->stVippVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->stVippVencChnAttr.VeAttr.AttrH264e.Profile = 2;
        pContext->stVippVencChnAttr.VeAttr.AttrH264e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stVippVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->stConfig.nVippEncodeWidth;
        pContext->stVippVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->stConfig.nVippEncodeHeight;
        pContext->stVippVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
    }
    else if (PT_H265 == pContext->stVippVencChnAttr.VeAttr.Type)
    {
        pContext->stVippVencChnAttr.VeAttr.AttrH265e.mBufSize = 0;
        pContext->stVippVencChnAttr.VeAttr.AttrH265e.mThreshSize = pContext->stConfig.nVippVideoBitrate/8
            /pContext->stConfig.nIspCaptureFrameRate*15;
        pContext->stVippVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->stVippVencChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->stVippVencChnAttr.VeAttr.AttrH265e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stVippVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->stConfig.nVippEncodeWidth;
        pContext->stVippVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->stConfig.nVippEncodeHeight;
        pContext->stVippVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
    }
    else if (PT_MJPEG == pContext->stVippVencChnAttr.VeAttr.Type)
    {
        pContext->stVippVencChnAttr.VeAttr.AttrMjpeg.mBufSize = 0;
        pContext->stVippVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = 0;
        pContext->stVippVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->stVippVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->stConfig.nVippEncodeWidth;
        pContext->stVippVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->stConfig.nVippEncodeHeight;
    }
    pContext->stVippVencChnAttr.VeAttr.MaxKeyInterval = pContext->stConfig.nVippKeyFrameInterval;
    pContext->stVippVencChnAttr.VeAttr.SrcPicWidth  = AWALIGN(pContext->stConfig.nVippCaptureWidth,16);
    pContext->stVippVencChnAttr.VeAttr.SrcPicHeight = pContext->stConfig.nVippCaptureHeight;
    pContext->stVippVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->stVippVencChnAttr.VeAttr.PixelFormat = pContext->stConfig.eVippPixelFormat;
    pContext->stVippVencChnAttr.VeAttr.mColorSpace = pContext->stConfig.eIspColorSpace;
    pContext->stVippVencChnAttr.VeAttr.Rotate = convertRotateDegree2ROTATE_E(pContext->stConfig.nVippEncodeRotate);
    pContext->stVippVencChnAttr.VeAttr.mDropFrameNum = 0;
    pContext->stVippVencChnAttr.VeAttr.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT;
    pContext->stVippVencChnAttr.VeAttr.mVeRecRefBufReduceEnable = 0;
    pContext->stVippVencChnAttr.VeAttr.mVbrOptEnable = pContext->stConfig.vbrOptEn;
    alogd("pixfmt:0x%x, colorSpace:0x%x, VeRefFrameLbcMode:%d, VeRecRefBufReduceEnable:%d, VbrOptEnable:%d",
        pContext->stVippVencChnAttr.VeAttr.PixelFormat, pContext->stVippVencChnAttr.VeAttr.mColorSpace,
        pContext->stVippVencChnAttr.VeAttr.mVeRefFrameLbcMode, pContext->stVippVencChnAttr.VeAttr.mVeRecRefBufReduceEnable,
        pContext->stVippVencChnAttr.VeAttr.mVbrOptEnable);
    if (PT_H264 == pContext->stVippVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfig.RcMode)
        {
        case 1:
            pContext->stVippVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->stConfig.nVippVideoBitrate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            break;
        case 2:
            pContext->stVippVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 35;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 35;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264FixQp.mSrcFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264FixQp.mDstFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            break;
        case 0:
        default:
            pContext->stVippVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->stConfig.nVippVideoBitrate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            break;
        }
    }
    else if (PT_H265 == pContext->stVippVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfig.RcMode)
        {
        case 1:
            pContext->stVippVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->stConfig.nVippVideoBitrate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            break;
        case 2:
            pContext->stVippVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 35;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 35;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265FixQp.mSrcFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265FixQp.mDstFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            break;
        case 0:
        default:
            pContext->stVippVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->stConfig.nVippVideoBitrate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            pContext->stVippVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            break;
        }
    }
    else if (PT_MJPEG == pContext->stVippVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfig.RcMode)
        {
        case 2:
            pContext->stVippVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            break;
        case 0:
        default:
            pContext->stVippVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->stConfig.nVippVideoBitrate;
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = pContext->stConfig.nIspCaptureFrameRate;
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.bitRateMax = (int)((float)pContext->stConfig.nVippVideoBitrate*1.2);
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.bitRateMin = (int)((float)pContext->stConfig.nVippVideoBitrate*0.8);
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.fRangeRatioTh = 0.05;
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nQualityTh = 85;
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nMinQuality = 10;
            pContext->stVippVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nMaxQuality = 100;
            break;
        }
    }
    pContext->stVippVencChnAttr.RcAttr.mProductMode = PRODUCT_STATIC_IPC;
    pContext->stVippVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->stVippVencChnAttr.GopAttr.mGopSize = 2;
    if (pContext->stConfig.bVippEncodeSharpEn)
    {
        pContext->stVippVencChnAttr.EncppAttr.eEncppSharpSetting = VencEncppSharp_FollowISPConfig;
    }
    else
    {
        pContext->stVippVencChnAttr.EncppAttr.eEncppSharpSetting = VencEncppSharp_Disable;
    }

    memset(&pContext->stVippVencRcParam, 0, sizeof(VENC_RC_PARAM_S));
    if (VENC_RC_MODE_H264CBR == pContext->stVippVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stVippVencRcParam.ParamH264Cbr.mMaxQp = 45;
        pContext->stVippVencRcParam.ParamH264Cbr.mMinQp = 25;
        pContext->stVippVencRcParam.ParamH264Cbr.mMaxPqp = 45;
        pContext->stVippVencRcParam.ParamH264Cbr.mMinPqp = 25;
        pContext->stVippVencRcParam.ParamH264Cbr.mQpInit = 37;
        pContext->stVippVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 1;
    }
    else if (VENC_RC_MODE_H264VBR == pContext->stVippVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stVippVencRcParam.ParamH264Vbr.mMaxQp = 45;
        pContext->stVippVencRcParam.ParamH264Vbr.mMinQp = 25;
        pContext->stVippVencRcParam.ParamH264Vbr.mMaxPqp = 45;
        pContext->stVippVencRcParam.ParamH264Vbr.mMinPqp = 25;
        pContext->stVippVencRcParam.ParamH264Vbr.mQpInit = 37;
        pContext->stVippVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 1;
        pContext->stVippVencRcParam.ParamH264Vbr.mMovingTh = 20;
        pContext->stVippVencRcParam.ParamH264Vbr.mQuality = 5;
        pContext->stVippVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 15;
        pContext->stVippVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    }
    else if (VENC_RC_MODE_H265CBR == pContext->stVippVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stVippVencRcParam.ParamH265Cbr.mMaxQp = 45;
        pContext->stVippVencRcParam.ParamH265Cbr.mMinQp = 25;
        pContext->stVippVencRcParam.ParamH265Cbr.mMaxPqp = 45;
        pContext->stVippVencRcParam.ParamH265Cbr.mMinPqp = 25;
        pContext->stVippVencRcParam.ParamH265Cbr.mQpInit = 37;
        pContext->stVippVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 1;
    }
    else if (VENC_RC_MODE_H265VBR == pContext->stVippVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stVippVencRcParam.ParamH265Vbr.mMaxQp = 45;
        pContext->stVippVencRcParam.ParamH265Vbr.mMinQp = 25;
        pContext->stVippVencRcParam.ParamH265Vbr.mMaxPqp = 45;
        pContext->stVippVencRcParam.ParamH265Vbr.mMinPqp = 25;
        pContext->stVippVencRcParam.ParamH265Vbr.mQpInit = 37;
        pContext->stVippVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 1;
        pContext->stVippVencRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->stVippVencRcParam.ParamH265Vbr.mQuality = 5;
        pContext->stVippVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 15;
        pContext->stVippVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }

    if (pContext->stConfig.nVippCropWidth > 0 && pContext->stConfig.nVippCropHeight > 0)
    {
        pContext->stVippVencCropCfg.bEnable = TRUE;
        pContext->stVippVencCropCfg.Rect.X = pContext->stConfig.nVippCropX;
        pContext->stVippVencCropCfg.Rect.Y = pContext->stConfig.nVippCropY;
        pContext->stVippVencCropCfg.Rect.Width = pContext->stConfig.nVippCropWidth;
        pContext->stVippVencCropCfg.Rect.Height = pContext->stConfig.nVippCropHeight;
    }
    else
    {
        pContext->stVippVencCropCfg.bEnable = FALSE;
    }
    return SUCCESS;
}

void config_AIO_ATTR_S_for_AI(AIO_ATTR_S *pAiAttr, SampleUvcViCodecConfig *pConfig)
{
    memset(pAiAttr, 0, sizeof(AIO_ATTR_S));
    pAiAttr->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(pConfig->nSampleRate);
    pAiAttr->enBitwidth = AUDIO_BIT_WIDTH_16;
    if(pConfig->bAiAec)
    {
        pAiAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
    }
    else
    {
        if(1 == pConfig->nMicNum)
        {
            pAiAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
        }
        else if(2 == pConfig->nMicNum)
        {
            pAiAttr->enSoundmode = AUDIO_SOUND_MODE_STEREO;
        }
        else
        {
            aloge("fatal error! mic num[%d] wrong", pConfig->nMicNum);
            pAiAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
        }
    }
    pAiAttr->mPtNumPerFrm = 0;
    pAiAttr->mChnCnt = pConfig->nMicNum;
    pAiAttr->mMicNum = pConfig->nMicNum;
    pAiAttr->ai_aec_en = pConfig->bAiAec;
    pAiAttr->aec_delay_ms = 0;
    pAiAttr->mAecNlpMode = 0;
    pAiAttr->ai_ans_en = pConfig->bAiAns;
    pAiAttr->ai_ans_mode = 0;
    pAiAttr->ai_agc_en = pConfig->bAiAgc;
    if (pAiAttr->ai_agc_en)
    {
        pAiAttr->ai_agc_float_cfg.fTargetDb = 0;
        pAiAttr->ai_agc_float_cfg.fMaxGainDb = 30;
    }
}

static void configAEncAttr(SampleUvcViCodecContext *pContext)
{
    memset(&pContext->stAEncAttr, 0, sizeof(pContext->stAEncAttr));
    pContext->stAEncAttr.AeAttr.Type = pContext->stConfig.eAencType;
    pContext->stAEncAttr.AeAttr.channels = judgeAudioChnNumBySoundMode(pContext->stAiAttr.enSoundmode, NULL, NULL);
    pContext->stAEncAttr.AeAttr.bitsPerSample = 16;
    pContext->stAEncAttr.AeAttr.sampleRate = pContext->stConfig.nSampleRate;
    pContext->stAEncAttr.AeAttr.bitRate = 32000;
    pContext->stAEncAttr.AeAttr.attachAACHeader = (int)pContext->stConfig.bAencAttachAACHeader;
    pContext->stAEncAttr.AeAttr.mInBufSize = 0;
    pContext->stAEncAttr.AeAttr.mOutBufCnt = 0;
}

/**
  need malloc frame buffer for display frame, consider display rotate. So width and height are all 16 align.

  @param nDisplayRotate
    value is 0, 90, 180, 270.
  @return
    0:success
    -1:fail
*/
static int initDisplayFrame(VIDEO_FRAME_INFO_S *pDisplayFrame, VIDEO_FRAME_INFO_S *pSrcFrame, int nDisplayRotate,
    int index)
{
    int result = 0;
    ERRORTYPE eRet;
    if ((pSrcFrame->VFrame.mPixelFormat != MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420)
        && (pSrcFrame->VFrame.mPixelFormat != MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420))
    {
        aloge("fatal error! we only support nv12 or nv21!");
        return -1;
    }
    int nBufWidth = AWALIGN(pSrcFrame->VFrame.mWidth, 16);
    int nBufHeight = AWALIGN(pSrcFrame->VFrame.mHeight, 16);
    int nBufLen = nBufWidth*nBufHeight*3/2;
    unsigned int PhyAddr;
    void *pVirtAddr;
    eRet = AW_MPI_SYS_MmzAlloc_Cached(&PhyAddr, &pVirtAddr, nBufLen);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! mmzAlloc fail:0x%x", eRet);
        return -1;
    }
    if ((0 == nDisplayRotate) || (180 == nDisplayRotate))
    {
        pDisplayFrame->VFrame.mWidth = nBufWidth;
        pDisplayFrame->VFrame.mHeight = nBufHeight;
    }
    else
    {
        pDisplayFrame->VFrame.mWidth = nBufHeight;
        pDisplayFrame->VFrame.mHeight = nBufWidth;
    }
    pDisplayFrame->VFrame.mPixelFormat = pSrcFrame->VFrame.mPixelFormat;
    pDisplayFrame->VFrame.mPhyAddr[0] = PhyAddr;
    pDisplayFrame->VFrame.mPhyAddr[1] = PhyAddr + nBufWidth*nBufHeight;
    pDisplayFrame->VFrame.mpVirAddr[0] = pVirtAddr;
    pDisplayFrame->VFrame.mpVirAddr[1] = pVirtAddr + nBufWidth*nBufHeight;
    if ((0 == nDisplayRotate) || (180 == nDisplayRotate))
    {
        pDisplayFrame->VFrame.mStride[0] = nBufWidth;
        pDisplayFrame->VFrame.mStride[1] = nBufWidth;
    }
    else
    {
        pDisplayFrame->VFrame.mStride[0] = nBufHeight;
        pDisplayFrame->VFrame.mStride[1] = nBufHeight;
    }
    pDisplayFrame->mId = (G2DFRAME_PREFIX | index);
    return result;
}

static int destroyDisplayFrame(VIDEO_FRAME_INFO_S *pDisplayFrame)
{
    int result = 0;
    ERRORTYPE eRet = AW_MPI_SYS_MmzFree(pDisplayFrame->VFrame.mPhyAddr[0], pDisplayFrame->VFrame.mpVirAddr[0]);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! mmz free fail:0x%x", eRet);
        result = -1;
    }
    pDisplayFrame->VFrame.mPhyAddr[0] = 0;
    pDisplayFrame->VFrame.mPhyAddr[1] = 0;
    pDisplayFrame->VFrame.mpVirAddr[0] = NULL;
    pDisplayFrame->VFrame.mpVirAddr[1] = NULL;
    return result;
}

static int generateVencFilePath(char *pFilePath, int nSize, SampleUvcViCodecContext *pContext)
{
    char *dotPos = strrchr(pContext->stConfig.strVencFilePath, '.');
    if (dotPos)
    {
        int nPrefixLen = dotPos - pContext->stConfig.strVencFilePath;
        snprintf(pFilePath, nSize, "%.*s%d%s", (int)nPrefixLen, pContext->stConfig.strVencFilePath, pContext->nVencFileIndex, 
            dotPos);
    }
    else
    {
        snprintf(pFilePath, nSize, "%s%d", pContext->stConfig.strVencFilePath, pContext->nVencFileIndex);
    }
    pContext->nVencFileIndex++;
    return 0;
}

static int generateAencFilePath(char *pFilePath, int nSize, SampleUvcViCodecContext *pContext)
{
    char *dotPos = strrchr(pContext->stConfig.strAencFilePath, '.');
    if (dotPos)
    {
        int nPrefixLen = dotPos - pContext->stConfig.strAencFilePath;
        snprintf(pFilePath, nSize, "%.*s%d%s", (int)nPrefixLen, pContext->stConfig.strAencFilePath, pContext->nAencFileIndex, 
            dotPos);
    }
    else
    {
        snprintf(pFilePath, nSize, "%s%d", pContext->stConfig.strAencFilePath, pContext->nAencFileIndex);
    }
    pContext->nAencFileIndex++;
    return 0;
}

static bool IsKeyFrame(VENC_STREAM_S *pVencStream, SampleUvcViCodecContext *pContext)
{
    bool bKeyFrame = false;
    if (PT_H264 == pContext->stConfig.eVencType)
    {
        if(H264E_NALU_ISLICE == pVencStream->mpPack[0].mDataType.enH264EType)
        {
            bKeyFrame = true;
        }
    }
    else if(PT_H265 == pContext->stConfig.eVencType)
    {
        if(H265E_NALU_ISLICE == pVencStream->mpPack[0].mDataType.enH265EType)
        {
            bKeyFrame = true;
        }
    }
    return bKeyFrame;
}

/**
  get video frame from uvcVdecChannel, send to mpi_venc, send to mpi_vo.
*/
static void *GetUvcVdecFrameThread(void *pThreadData)
{
    int result;
    int ret;
    ERRORTYPE eRet;
    message_t stMsg;
    SampleUvcViCodecContext *pContext = (SampleUvcViCodecContext *)pThreadData;

    alogd("GetUvcVdecFrame thread start...");
    char strThreadName[32];
    sprintf(strThreadName, "uvcVdecFrameThd");
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    while (1)
    {
PROCESS_MESSAGE:
        if (get_message(&pContext->stGetUvcVdecFrameMessageQueue, &stMsg) == 0)
        {
            // State transition command
            if (SampleMsgType_SetState == stMsg.command)
            {
                if (pContext->eGetUvcVdecFrameThreadState == (SampleState)stMsg.para0)
                {
                    alogd("same state:%d", stMsg.para0);
                    if (stMsg.pReply)
                    {
                        stMsg.pReply->ReplyResult = 0;
                        cdx_sem_up(&stMsg.pReply->ReplySem);
                    }
                }
                else
                {
                    switch ((SampleState)stMsg.para0)
                    {
                    case SampleState_Idle:
                    {
                        if(SampleState_Executing == pContext->eGetUvcVdecFrameThreadState)
                        {
                            //reset voChn, free g2d frame buffer, reset vencChn, uvcChn and vdecChn all turn to idle.
                            alogd("executing->idle");
                            if (pContext->bEnableUvcDisplay)
                            {
                                eRet = AW_MPI_VO_StopChn(pContext->nVoLayer, pContext->nVOChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! voChn[%d-%d] stop fail:0x%x", pContext->nVoLayer, pContext->nVOChn, eRet);
                                }
                                pthread_mutex_lock(&pContext->stDisplayFrameLock);
                                int cnt = list_count_nodes(&pContext->mUsingDisplayFrameList);
                                if (cnt > 0)
                                {
                                    aloge("fatal error! still has [%d] using display frame?", cnt);
                                }
                                cnt = list_count_nodes(&pContext->mIdleDisplayFrameList);
                                if (cnt > 0)
                                {
                                    DisplayFrameInfoNode *pEntry, *pTmp;
                                    list_for_each_entry_safe(pEntry, pTmp, &pContext->mIdleDisplayFrameList, mList)
                                    {
                                        if (pEntry->nRefCnt != 0)
                                        {
                                            aloge("fatal error! check display frame refCnt:%d", pEntry->nRefCnt);
                                        }
                                        destroyDisplayFrame(&pEntry->stFrame);
                                        list_del(&pEntry->mList);
                                        free(pEntry);
                                    }
                                    alogd("uvcFrameThread free [%d]displayFrame nodes", cnt);
                                }
                                pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                            }
                            if (pContext->bEnableVideoEncode)
                            {
                                if (pContext->eCurPreviewSource != PreviewSource_UVC)
                                {
                                    aloge("fatal error! current preview source[%d] must be 0!", pContext->eCurPreviewSource);
                                }
                                eRet = AW_MPI_VENC_StopRecvPic(pContext->nVencChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! vencChn[%d] stop fail:0x%x", pContext->nVencChn, eRet);
                                }
                                //must return all outFrames before destroy encLib.
                                eRet = AW_MPI_VENC_ResetChn(pContext->nVencChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! vencChn[%d] resetChn fail:0x%x", pContext->nVencChn, eRet);
                                }
                                eRet = AW_MPI_VENC_DestroyEncoder(pContext->nVencChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! vencChn[%d] destroy encoder fail:0x%x", pContext->nVencChn, eRet);
                                }
                                pContext->bCreateEncLibFlag = false;
                            }
                            eRet = AW_MPI_UVC_StopRecvPic(pContext->stConfig.strUvcDevName, pContext->nUvcChn);
                            if (eRet != SUCCESS)
                            {
                                aloge("fatal error! uvcChn[%s-%d] stop fail:0x%x", pContext->stConfig.strUvcDevName, pContext->nUvcChn, eRet);
                            }
                            eRet = AW_MPI_VDEC_StopRecvStream(pContext->nVdecChn);
                            if (eRet != SUCCESS)
                            {
                                aloge("fatal error! vdecChn[%d] stop fail:0x%x", pContext->nVdecChn, eRet);
                            }
                            //AW_MPI_VDEC_Seek(pContext->nVdecChn); //to reset video decoder, clear input vbv of vdeclib.
                            pContext->eGetUvcVdecFrameThreadState = SampleState_Idle;
                            if (stMsg.pReply)
                            {
                                stMsg.pReply->ReplyResult = 0;
                                cdx_sem_up(&stMsg.pReply->ReplySem);
                            }
                        }
                        else
                        {
                            aloge("fatal error! current state[%d] can't turn to idle!", pContext->eGetUvcVdecFrameThreadState);
                        }
                        break;
                    }
                    case SampleState_Executing:
                    {
                        //uvcChn and vdecChn all turn to executing, start voChn, start vencChn.
                        //when get first frame, alloc g2d frame buffer then.
                        if (SampleState_Idle == pContext->eGetUvcVdecFrameThreadState)
                        {
                            alogd("idle->executing");
                            eRet = AW_MPI_UVC_StartRecvPic(pContext->stConfig.strUvcDevName, pContext->nUvcChn);
                            if (eRet != SUCCESS)
                            {
                                aloge("fatal error! uvcChn[%s-%d] start fail:0x%x", pContext->stConfig.strUvcDevName, pContext->nUvcChn, eRet);
                            }
                            eRet = AW_MPI_VDEC_StartRecvStream(pContext->nVdecChn);
                            if (eRet != SUCCESS)
                            {
                                aloge("fatal error! vdecChn[%d] start fail:0x%x", pContext->nVdecChn, eRet);
                            }
                            if (pContext->bEnableUvcDisplay)
                            {
                                AW_MPI_VO_GetVideoLayerAttr(pContext->nVoLayer, &pContext->stUvcLayerAttr);
                                pContext->stUvcLayerAttr.stDispRect.X = pContext->stConfig.nUvcDisplayX;
                                pContext->stUvcLayerAttr.stDispRect.Y = pContext->stConfig.nUvcDisplayY;
                                pContext->stUvcLayerAttr.stDispRect.Width = pContext->stConfig.nUvcDisplayWidth;
                                pContext->stUvcLayerAttr.stDispRect.Height = pContext->stConfig.nUvcDisplayHeight;
                                AW_MPI_VO_SetVideoLayerAttr(pContext->nVoLayer, &pContext->stUvcLayerAttr);
                                eRet = AW_MPI_VO_StartChn(pContext->nVoLayer, pContext->nVOChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! voChn[%d-%d] start fail:0x%x", pContext->nVoLayer, pContext->nVOChn, eRet);
                                }
                                pthread_mutex_lock(&pContext->stDisplayFrameLock);
                                int cnt = list_count_nodes(&pContext->mUsingDisplayFrameList);
                                if (cnt > 0)
                                {
                                    aloge("fatal error! still has [%d] using display frames?", cnt);
                                }
                                cnt = list_count_nodes(&pContext->mIdleDisplayFrameList);
                                if (cnt > 0)
                                {
                                    aloge("fatal error! still has [%d] idle display frames?", cnt);
                                }
                                pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                            }
                            if (pContext->bEnableVideoEncode)
                            {
                                if (pContext->eCurPreviewSource != PreviewSource_UVC)
                                {
                                    aloge("fatal error! current preview source[%d] must be 0!", pContext->eCurPreviewSource);
                                }
                                if (false == pContext->bCreateEncLibFlag) //it means encLib is destroyed, need recreate it.
                                {
                                    eRet = AW_MPI_VENC_SetChnAttr(pContext->nVencChn, &pContext->stUvcVencChnAttr);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] set chnAttr fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    eRet = AW_MPI_VENC_CreateEncoder(pContext->nVencChn);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] create encoder fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    eRet = AW_MPI_VENC_SetRcParam(pContext->nVencChn, &pContext->stUvcVencRcParam);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] set rc param fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    VencVbrOptParam stVencVbrOptParam;
                                    eRet = AW_MPI_VENC_GetVbrOptParam(pContext->nVencChn, &stVencVbrOptParam);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] get vbr opt param fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    stVencVbrOptParam.sRcPriority = pContext->stConfig.eVbrOptRcPriority;
                                    stVencVbrOptParam.eQualityLevel = pContext->stConfig.eVbrOptRcQualityLevel;
                                    eRet = AW_MPI_VENC_SetVbrOptParam(pContext->nVencChn, &stVencVbrOptParam);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] set vbr opt param fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    VENC_IspVeLinkAttr stIspVeLink;
                                    memset(&stIspVeLink, 0, sizeof(stIspVeLink));
                                    stIspVeLink.bEnableIsp2Ve = FALSE;
                                    stIspVeLink.bEnableVe2Isp = FALSE;
                                    AW_MPI_VENC_EnableIspVeLink(pContext->nVencChn, &stIspVeLink);
                                    pContext->bCreateEncLibFlag = true;
                                }
                                eRet = AW_MPI_VENC_StartRecvPic(pContext->nVencChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! vencChn[%d] start fail:0x%x", pContext->nVencChn, eRet);
                                }
                            }
                            pContext->eGetUvcVdecFrameThreadState = SampleState_Executing;
                            if (stMsg.pReply)
                            {
                                stMsg.pReply->ReplyResult = 0;
                                cdx_sem_up(&stMsg.pReply->ReplySem);
                            }
                        }
                        else
                        {
                            aloge("fatal error! current state[%d] can't turn to executing!", pContext->eGetUvcVdecFrameThreadState);
                        }
                        break;
                    }
                    default:
                    {
                        aloge("fatal error! unknown dst state:%d", stMsg.para0);
                        break;
                    }
                    }
                }
            }
            else if (SampleMsgType_Stop == stMsg.command)
            {
                // Kill thread
                if (pContext->eGetUvcVdecFrameThreadState != SampleState_Idle)
                {
                    aloge("fatal error! state[%d] is not idle!", pContext->eGetUvcVdecFrameThreadState);
                }
                goto EXIT;
            }
            else
            {
                aloge("fatal error! unknown command:%d", stMsg.command);
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (SampleState_Executing == pContext->eGetUvcVdecFrameThreadState)
        {
            int cnt;
            int i;
            VIDEO_FRAME_INFO_S uvcFrameInfo;
            VDEC_STREAM_S stVdecStream;
            memset(&stVdecStream, 0, sizeof(stVdecStream));
            //get uvc stream, send to vdecChn, get vdec frame, send to vencChn and voChn.
            eRet = AW_MPI_UVC_GetFrame(pContext->stConfig.strUvcDevName, pContext->nUvcChn, &uvcFrameInfo, 200);
            if (SUCCESS == eRet)
            {
                stVdecStream.pAddr = uvcFrameInfo.VFrame.mpVirAddr[0];
                stVdecStream.mLen = uvcFrameInfo.VFrame.mStride[0];
                stVdecStream.mPTS = uvcFrameInfo.VFrame.mpts;
                stVdecStream.mbEndOfFrame = TRUE;
                stVdecStream.mbEndOfStream = FALSE;
                eRet = AW_MPI_VDEC_SendStream(pContext->nVdecChn, &stVdecStream, 200);
                if (eRet != SUCCESS)
                {
                    aloge("fatal error! vdecChn[%d] send stream fail:0x%x?", pContext->nVdecChn, eRet);
                }
                eRet = AW_MPI_UVC_ReleaseFrame(pContext->stConfig.strUvcDevName, pContext->nUvcChn, &uvcFrameInfo);
                if (eRet != SUCCESS)
                {
                    aloge("fatal error! uvcChn[%s-%d] release frame fail:0x%x?", pContext->stConfig.strUvcDevName, pContext->nUvcChn, eRet);
                }
            }
            else
            {
                aloge("fatal error! uvcChn[%s-%d] get frame fail:0x%x", pContext->stConfig.strUvcDevName, pContext->nUvcChn, eRet);
                goto PROCESS_MESSAGE;
            }

            VdecDoubleFrameInfoNode *pFramePairNode = list_first_entry_or_null(&pContext->mIdleVdecFramePairList,
                VdecDoubleFrameInfoNode, mList);
            if (NULL == pFramePairNode)
            {
                aloge("fatal error! uvcChn[%s-%d] idle doubleFrame list empty.", pContext->stConfig.strUvcDevName, pContext->nUvcChn);
            }
            if ((pFramePairNode->nMainRefCnt != 0) || (pFramePairNode->nSubRefCnt != 0))
            {
                aloge("fatal error! uvcChn[%s-%d] idle doubleFrame refCnt[%d-%d] wrong.", pContext->stConfig.strUvcDevName,
                    pContext->nUvcChn, pFramePairNode->nMainRefCnt, pFramePairNode->nSubRefCnt);
            }
            eRet = AW_MPI_VDEC_GetDoubleImage(pContext->nVdecChn, &pFramePairNode->stMainFrame, &pFramePairNode->stSubFrame, 500);
            if(SUCCESS == eRet)
            {
                pthread_mutex_lock(&pContext->stVdecFrameLock);
                pFramePairNode->nMainRefCnt = 1;
                if (pContext->stConfig.nVdecSubRatio > 0)
                {
                    pFramePairNode->nSubRefCnt = 1;
                }
                list_move_tail(&pFramePairNode->mList, &pContext->mUsingVdecFramePairList);
                pthread_mutex_unlock(&pContext->stVdecFrameLock);
            }
            else
            {
                aloge("fatal error! why not get frame from vdec for too long? ret=0x%x", eRet);
                goto PROCESS_MESSAGE;
            }
            if (pContext->bEnableVideoEncode)
            {
                pthread_mutex_lock(&pContext->stVdecFrameLock);
                pFramePairNode->nMainRefCnt++;
                pthread_mutex_unlock(&pContext->stVdecFrameLock);
                eRet = AW_MPI_VENC_SendFrame(pContext->nVencChn, &pFramePairNode->stMainFrame, 0);
                if (eRet != SUCCESS)
                {
                    aloge("fatal error! vencChn[%d] send frame fail:0x%x", pContext->nVencChn, eRet);
                    pthread_mutex_lock(&pContext->stVdecFrameLock);
                    pFramePairNode->nMainRefCnt--;
                    pthread_mutex_unlock(&pContext->stVdecFrameLock);
                }
            }
            if (pContext->bEnableUvcDisplay)
            {
                if ((0 == pContext->stConfig.nVdecSubRatio) && (0==pContext->stConfig.nUvcDisplayRotate))
                {
                    pthread_mutex_lock(&pContext->stVdecFrameLock);
                    pFramePairNode->nMainRefCnt++;
                    pthread_mutex_unlock(&pContext->stVdecFrameLock);
                    eRet = AW_MPI_VO_SendFrame(pContext->nVoLayer, pContext->nVOChn, &pFramePairNode->stMainFrame, 0);
                    if(eRet != SUCCESS)
                    {
                        aloge("fatal error! why send frame to vo fail? voChn[%d-%d], ret[0x%x]", pContext->nVoLayer, pContext->nVOChn, eRet);
                        pthread_mutex_lock(&pContext->stVdecFrameLock);
                        pFramePairNode->nMainRefCnt--;
                        pthread_mutex_unlock(&pContext->stVdecFrameLock);
                    }
                }
                else
                {
                    DisplayFrameInfoNode *pDisplayFrameNode = NULL;
                    VIDEO_FRAME_INFO_S *pFrameInfo = NULL;
                    if(pContext->stConfig.nVdecSubRatio > 0)
                    {
                        pFrameInfo = &pFramePairNode->stSubFrame;
                    }
                    else
                    {
                        pFrameInfo = &pFramePairNode->stMainFrame;
                    }
                    //must use g2d frame buffer, because vdec frame buffer can't malloc too many. we wan't to reduce memory.
                    pthread_mutex_lock(&pContext->stDisplayFrameLock);
                    cnt = list_count_nodes(&pContext->mIdleDisplayFrameList) + list_count_nodes(&pContext->mUsingDisplayFrameList);
                    if (0 == cnt)
                    {
                        for (i=0; i<MAX_DISPLAY_FRAME_NUM; i++)
                        {
                            DisplayFrameInfoNode *pNode = (DisplayFrameInfoNode *)calloc(1, sizeof(DisplayFrameInfoNode));
                            if (NULL == pNode)
                            {
                                aloge("fatal error! malloc fail");
                            }
                            ret = initDisplayFrame(&pNode->stFrame, pFrameInfo, pContext->stConfig.nUvcDisplayRotate, i);
                            if (ret != 0)
                            {
                                aloge("fatal error! init display frame fail:%d", ret);
                            }
                            list_add_tail(&pNode->mList, &pContext->mIdleDisplayFrameList);
                        }
                    }
                    pDisplayFrameNode = list_first_entry_or_null(&pContext->mIdleDisplayFrameList, DisplayFrameInfoNode, mList);
                    pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                    if (pDisplayFrameNode)
                    {
                        //verify.
                        if (pDisplayFrameNode->nRefCnt != 0)
                        {
                            aloge("fatal error! check refCnt[%d]", pDisplayFrameNode->nRefCnt);
                        }
                        if (pDisplayFrameNode->stFrame.VFrame.mPixelFormat != pFrameInfo->VFrame.mPixelFormat)
                        {
                            aloge("fatal error! check frame pixel format[%d!=%d]", pDisplayFrameNode->stFrame.VFrame.mPixelFormat,
                                pFrameInfo->VFrame.mPixelFormat);
                        }
                        if ((0==pContext->stConfig.nUvcDisplayRotate) || (180==pContext->stConfig.nUvcDisplayRotate))
                        {
                            if ((pDisplayFrameNode->stFrame.VFrame.mWidth != AWALIGN(pFrameInfo->VFrame.mWidth, 16))
                                || (pDisplayFrameNode->stFrame.VFrame.mHeight != AWALIGN(pFrameInfo->VFrame.mHeight, 16)))
                            {
                                aloge("fatal error! check frame width and height[%dx%d,%dx%d]", pDisplayFrameNode->stFrame.VFrame.mWidth,
                                    pDisplayFrameNode->stFrame.VFrame.mHeight, pFrameInfo->VFrame.mWidth, pFrameInfo->VFrame.mHeight);
                            }
                        }
                        else
                        {
                            if ((pDisplayFrameNode->stFrame.VFrame.mWidth != AWALIGN(pFrameInfo->VFrame.mHeight, 16))
                                || (pDisplayFrameNode->stFrame.VFrame.mHeight != AWALIGN(pFrameInfo->VFrame.mWidth, 16)))
                            {
                                aloge("fatal error! check frame width and height[%dx%d,%dx%d]", pDisplayFrameNode->stFrame.VFrame.mWidth,
                                    pDisplayFrameNode->stFrame.VFrame.mHeight, pFrameInfo->VFrame.mWidth, pFrameInfo->VFrame.mHeight);
                            }
                        }
                        //set display frame info. note: we move valid area to left-top of display frame.
                        pDisplayFrameNode->stFrame.VFrame.mOffsetTop = 0;
                        pDisplayFrameNode->stFrame.VFrame.mOffsetLeft = 0;
                        if ((0 == pContext->stConfig.nUvcDisplayRotate) || (180 == pContext->stConfig.nUvcDisplayRotate))
                        {
                            pDisplayFrameNode->stFrame.VFrame.mOffsetBottom = pFrameInfo->VFrame.mOffsetBottom - pFrameInfo->VFrame.mOffsetTop;
                            pDisplayFrameNode->stFrame.VFrame.mOffsetRight = pFrameInfo->VFrame.mOffsetRight - pFrameInfo->VFrame.mOffsetLeft;
                        }
                        else
                        {
                            pDisplayFrameNode->stFrame.VFrame.mOffsetBottom = pFrameInfo->VFrame.mOffsetRight - pFrameInfo->VFrame.mOffsetLeft;
                            pDisplayFrameNode->stFrame.VFrame.mOffsetRight = pFrameInfo->VFrame.mOffsetBottom - pFrameInfo->VFrame.mOffsetTop;
                        }
                        pDisplayFrameNode->stFrame.VFrame.mpts = pFrameInfo->VFrame.mpts;
                        pDisplayFrameNode->stFrame.VFrame.mExposureTime = pFrameInfo->VFrame.mExposureTime;
                        pDisplayFrameNode->stFrame.VFrame.mFramecnt = pFrameInfo->VFrame.mFramecnt;
                        pDisplayFrameNode->stFrame.VFrame.mEnvLV = pFrameInfo->VFrame.mEnvLV;
                        pDisplayFrameNode->stFrame.VFrame.mEnvLVAdj = pFrameInfo->VFrame.mEnvLVAdj;
                        pDisplayFrameNode->stFrame.VFrame.mWhoSetFlag = pFrameInfo->VFrame.mWhoSetFlag;
                        //g2d rotate
                        ScalePictureParam stScaleParam;
                        memset(&stScaleParam, 0, sizeof(stScaleParam));
                        stScaleParam.eSrcPixFormat = pFrameInfo->VFrame.mPixelFormat;
                        stScaleParam.eSrcColorSpace = pContext->stConfig.eUvcColorSpace;
                        stScaleParam.mSrcPhyAddrs[0] = pFrameInfo->VFrame.mPhyAddr[0];
                        stScaleParam.mSrcPhyAddrs[1] = pFrameInfo->VFrame.mPhyAddr[1];
                        stScaleParam.mSrcPhyAddrs[2] = pFrameInfo->VFrame.mPhyAddr[2];
                        stScaleParam.mSrcPicSize.Width = pFrameInfo->VFrame.mWidth;
                        stScaleParam.mSrcPicSize.Height = pFrameInfo->VFrame.mHeight;
                        stScaleParam.mSrcValidRect.X = pFrameInfo->VFrame.mOffsetLeft;
                        stScaleParam.mSrcValidRect.Y = pFrameInfo->VFrame.mOffsetTop;
                        stScaleParam.mSrcValidRect.Width = pFrameInfo->VFrame.mOffsetRight - pFrameInfo->VFrame.mOffsetLeft;
                        stScaleParam.mSrcValidRect.Height = pFrameInfo->VFrame.mOffsetBottom - pFrameInfo->VFrame.mOffsetTop;
                        stScaleParam.eDstPixFormat = pFrameInfo->VFrame.mPixelFormat;
                        stScaleParam.eDstColorSpace = pContext->stConfig.eUvcColorSpace;
                        stScaleParam.mDstPhyAddrs[0] = pDisplayFrameNode->stFrame.VFrame.mPhyAddr[0];
                        stScaleParam.mDstPhyAddrs[1] = pDisplayFrameNode->stFrame.VFrame.mPhyAddr[1];
                        stScaleParam.mDstPhyAddrs[2] = pDisplayFrameNode->stFrame.VFrame.mPhyAddr[2];
                        stScaleParam.mDstPicSize.Width = pDisplayFrameNode->stFrame.VFrame.mWidth;
                        stScaleParam.mDstPicSize.Height = pDisplayFrameNode->stFrame.VFrame.mHeight;
                        stScaleParam.mDstValidRect.X = pDisplayFrameNode->stFrame.VFrame.mOffsetLeft;
                        stScaleParam.mDstValidRect.Y = pDisplayFrameNode->stFrame.VFrame.mOffsetTop;
                        stScaleParam.mDstValidRect.Width = pDisplayFrameNode->stFrame.VFrame.mOffsetRight
                            - pDisplayFrameNode->stFrame.VFrame.mOffsetLeft;
                        stScaleParam.mDstValidRect.Height = pDisplayFrameNode->stFrame.VFrame.mOffsetBottom
                            - pDisplayFrameNode->stFrame.VFrame.mOffsetTop;
                        stScaleParam.nRotate = pContext->stConfig.nUvcDisplayRotate;
                        ret = ScalePictureByG2d(&stScaleParam, pContext->nG2dDevFd);
                        if (ret != 0)
                        {
                            aloge("fatal error! g2d rotate fail:%d", ret);
                        }
                        pDisplayFrameNode->nRefCnt = 1;
                        pthread_mutex_lock(&pContext->stDisplayFrameLock);
                        list_move_tail(&pDisplayFrameNode->mList, &pContext->mUsingDisplayFrameList);
                        pDisplayFrameNode->nRefCnt++;
                        pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                        eRet = AW_MPI_VO_SendFrame(pContext->nVoLayer, pContext->nVOChn, &pDisplayFrameNode->stFrame, 0);
                        if(eRet != SUCCESS)
                        {
                            aloge("fatal error! why send frame to vo fail? voChn[%d-%d], ret[0x%x]", pContext->nVoLayer, pContext->nVOChn, eRet);
                            pthread_mutex_lock(&pContext->stDisplayFrameLock);
                            pDisplayFrameNode->nRefCnt--;
                            pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                        }
                        ret = ReleaseG2dVideoFrame(pContext, pDisplayFrameNode->stFrame.mId);
                        if (ret != 0)
                        {
                            aloge("fatal error! release g2d frameId[0x%x] fail:%d", pDisplayFrameNode->stFrame.mId, ret);
                        }
                    }
                    else
                    {
                        alogd("no idle g2d DisplayFrame node, ignore this frame");
                    }
                }
            }
            if (pContext->stConfig.nVdecSubRatio > 0)
            {
                ret = ReleaseVdecSubVideoFrame(pContext, pFramePairNode->stSubFrame.mId);
                if (ret != 0)
                {
                    aloge("fatal error! release vdec sub frameId[%d] fail:%d", pFramePairNode->stSubFrame.mId, ret);
                }
            }
            ret = ReleaseVdecMainVideoFrame(pContext, pFramePairNode->stMainFrame.mId);
            if (ret != 0)
            {
                aloge("fatal error! release vdec main frameId[%d] fail:%d", pFramePairNode->stMainFrame.mId, ret);
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pContext->stGetUvcVdecFrameMessageQueue, 10*1000);
        }
    }
EXIT:
    alogd("GetUvcVdecFrame thread exit");
    return (void *)SUCCESS;
}

static void *GetVippFrameThread(void *pThreadData)
{
    int result;
    int ret;
    ERRORTYPE eRet;
    message_t stMsg;
    SampleUvcViCodecContext *pContext = (SampleUvcViCodecContext *)pThreadData;

    alogd("GetVippFrame thread start...");
    char strThreadName[32];
    sprintf(strThreadName, "getVippFrameThd");
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    while (1)
    {
PROCESS_MESSAGE:
        if (get_message(&pContext->stGetVippFrameMessageQueue, &stMsg) == 0)
        {
            // State transition command
            if (SampleMsgType_SetState == stMsg.command)
            {
                if (pContext->eGetVippFrameThreadState == (SampleState)stMsg.para0)
                {
                    alogd("same state:%d", stMsg.para0);
                    if (stMsg.pReply)
                    {
                        stMsg.pReply->ReplyResult = 0;
                        cdx_sem_up(&stMsg.pReply->ReplySem);
                    }
                }
                else
                {
                    switch ((SampleState)stMsg.para0)
                    {
                    case SampleState_Idle:
                    {
                        if(SampleState_Executing == pContext->eGetVippFrameThreadState)
                        {
                            //reset voChn, free g2d frame buffer, reset vencChn and destroy tunnel,viChns all turn to idle, 
                            alogd("executing->idle");
                            if (pContext->bEnableSubVippDisplay)
                            {
                                eRet = AW_MPI_VI_DisableVirChn(pContext->stConfig.nSubVippDev, pContext->nSubViChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! subViChn[%d-%d] disable fail:0x%x", pContext->stConfig.nSubVippDev, pContext->nSubViChn, eRet);
                                }
                                eRet = AW_MPI_VO_StopChn(pContext->nVoLayer, pContext->nVOChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! voChn[%d-%d] stop fail:0x%x", pContext->nVoLayer, pContext->nVOChn, eRet);
                                }
                                if (0 == pContext->stConfig.nSubVippDisplayRotate) //virvi-vo unbind
                                {
                                    MPP_CHN_S stMppSubViChn = {MOD_ID_VIU, pContext->stConfig.nSubVippDev, pContext->nSubViChn};
                                    MPP_CHN_S stMppVoChn = {MOD_ID_VOU, pContext->nVoLayer, pContext->nVOChn};
                                    eRet = AW_MPI_SYS_UnBind(&stMppSubViChn, &stMppVoChn);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! unbind vi and venc fail:0x%x", eRet);
                                    }
                                }
                                pthread_mutex_lock(&pContext->stDisplayFrameLock);
                                int cnt = list_count_nodes(&pContext->mUsingDisplayFrameList);
                                if (cnt > 0)
                                {
                                    aloge("fatal error! still has [%d] using display frame?", cnt);
                                }
                                cnt = list_count_nodes(&pContext->mIdleDisplayFrameList);
                                if (cnt > 0)
                                {
                                    DisplayFrameInfoNode *pEntry, *pTmp;
                                    list_for_each_entry_safe(pEntry, pTmp, &pContext->mIdleDisplayFrameList, mList)
                                    {
                                        if (pEntry->nRefCnt != 0)
                                        {
                                            aloge("fatal error! check display frame refCnt:%d", pEntry->nRefCnt);
                                        }
                                        destroyDisplayFrame(&pEntry->stFrame);
                                        list_del(&pEntry->mList);
                                        free(pEntry);
                                    }
                                    alogd("getVippFrameThread free [%d]displayFrame nodes", cnt);
                                }
                                pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                            }
                            if (pContext->bEnableVideoEncode)
                            {
                                if (pContext->eCurPreviewSource != PreviewSource_VIPP)
                                {
                                    aloge("fatal error! current preview source[%d] must be 1, vippPreview!", pContext->eCurPreviewSource);
                                }
                                eRet = AW_MPI_VI_DisableVirChn(pContext->stConfig.nVippDev, pContext->nViChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! viChn[%d-%d] disable fail:0x%x", pContext->stConfig.nVippDev, pContext->nViChn, eRet);
                                }
                                eRet = AW_MPI_VENC_StopRecvPic(pContext->nVencChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! vencChn[%d] stop fail:0x%x", pContext->nVencChn, eRet);
                                }
                                //must return all outFrames before destroy encLib.
                                eRet = AW_MPI_VENC_ResetChn(pContext->nVencChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! vencChn[%d] resetChn fail:0x%x", pContext->nVencChn, eRet);
                                }
                                MPP_CHN_S stMppViChn = {MOD_ID_VIU, pContext->stConfig.nVippDev, pContext->nViChn};
                                MPP_CHN_S stMppVeChn = {MOD_ID_VENC, 0, pContext->nVencChn};
                                eRet = AW_MPI_SYS_UnBind(&stMppViChn, &stMppVeChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! unbind vi and venc fail:0x%x", eRet);
                                }
                                eRet = AW_MPI_VENC_DestroyEncoder(pContext->nVencChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! vencChn[%d] destroy encoder fail:0x%x", pContext->nVencChn, eRet);
                                }
                                pContext->bCreateEncLibFlag = false;
                            }
                            //AW_MPI_VDEC_Seek(pContext->nVdecChn); //to reset video decoder, clear input vbv of vdeclib.
                            pContext->eGetVippFrameThreadState = SampleState_Idle;
                            if (stMsg.pReply)
                            {
                                stMsg.pReply->ReplyResult = 0;
                                cdx_sem_up(&stMsg.pReply->ReplySem);
                            }
                        }
                        else
                        {
                            aloge("fatal error! current state[%d] can't turn to idle!", pContext->eGetVippFrameThreadState);
                        }
                        break;
                    }
                    case SampleState_Executing:
                    {
                        //uvcChn and vdecChn all turn to executing, start voChn, start vencChn.
                        //when get first frame, alloc g2d frame buffer then.
                        if (SampleState_Idle == pContext->eGetVippFrameThreadState)
                        {
                            alogd("idle->executing");
                            if (pContext->bEnableSubVippDisplay)
                            {
                                pthread_mutex_lock(&pContext->stDisplayFrameLock);
                                int cnt = list_count_nodes(&pContext->mUsingDisplayFrameList);
                                if (cnt > 0)
                                {
                                    aloge("fatal error! still has [%d] using display frames?", cnt);
                                }
                                cnt = list_count_nodes(&pContext->mIdleDisplayFrameList);
                                if (cnt > 0)
                                {
                                    aloge("fatal error! still has [%d] idle display frames?", cnt);
                                }
                                pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                                AW_MPI_VO_GetVideoLayerAttr(pContext->nVoLayer, &pContext->stVippLayerAttr);
                                pContext->stVippLayerAttr.stDispRect.X = pContext->stConfig.nSubVippDisplayX;
                                pContext->stVippLayerAttr.stDispRect.Y = pContext->stConfig.nSubVippDisplayY;
                                pContext->stVippLayerAttr.stDispRect.Width = pContext->stConfig.nSubVippDisplayWidth;
                                pContext->stVippLayerAttr.stDispRect.Height = pContext->stConfig.nSubVippDisplayHeight;
                                AW_MPI_VO_SetVideoLayerAttr(pContext->nVoLayer, &pContext->stVippLayerAttr);
                                if (0 == pContext->stConfig.nSubVippDisplayRotate) //virvi-vo bind
                                {
                                    MPP_CHN_S stMppSubViChn = {MOD_ID_VIU, pContext->stConfig.nSubVippDev, pContext->nSubViChn};
                                    MPP_CHN_S stMppVoChn = {MOD_ID_VOU, pContext->nVoLayer, pContext->nVOChn};
                                    eRet = AW_MPI_SYS_Bind(&stMppSubViChn, &stMppVoChn);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! bind vi and venc fail:0x%x", eRet);
                                    }
                                }
                                eRet = AW_MPI_VI_EnableVirChn(pContext->stConfig.nSubVippDev, pContext->nSubViChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! subViChn[%d-%d] enable fail:0x%x", pContext->stConfig.nSubVippDev, pContext->nSubViChn, eRet);
                                }
                                eRet = AW_MPI_VO_StartChn(pContext->nVoLayer, pContext->nVOChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! voChn[%d-%d] start fail:0x%x", pContext->nVoLayer, pContext->nVOChn, eRet);
                                }
                            }
                            if (pContext->bEnableVideoEncode)
                            {
                                if (pContext->eCurPreviewSource != PreviewSource_VIPP)
                                {
                                    aloge("fatal error! current preview source[%d] must be 1, vippPreview!", pContext->eCurPreviewSource);
                                }
                                if (false == pContext->bCreateEncLibFlag) //it means encLib is destroyed, need recreate it.
                                {
                                    eRet = AW_MPI_VENC_SetChnAttr(pContext->nVencChn, &pContext->stVippVencChnAttr);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] set chnAttr fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    eRet = AW_MPI_VENC_CreateEncoder(pContext->nVencChn);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] create encoder fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    eRet = AW_MPI_VENC_SetRcParam(pContext->nVencChn, &pContext->stVippVencRcParam);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] set rc param fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    VencVbrOptParam stVencVbrOptParam;
                                    eRet = AW_MPI_VENC_GetVbrOptParam(pContext->nVencChn, &stVencVbrOptParam);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] get vbr opt param fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    stVencVbrOptParam.sRcPriority = pContext->stConfig.eVbrOptRcPriority;
                                    stVencVbrOptParam.eQualityLevel = pContext->stConfig.eVbrOptRcQualityLevel;
                                    eRet = AW_MPI_VENC_SetVbrOptParam(pContext->nVencChn, &stVencVbrOptParam);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] set vbr opt param fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    eRet = AW_MPI_VENC_SetCrop(pContext->nVencChn, &pContext->stVippVencCropCfg);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] set crop fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    VENC_IspVeLinkAttr stIspVeLink;
                                    memset(&stIspVeLink, 0, sizeof(stIspVeLink));
                                    stIspVeLink.bEnableIsp2Ve = (BOOL)pContext->stConfig.bVippIsp2VeLinkEn;
                                    stIspVeLink.bEnableVe2Isp = (BOOL)pContext->stConfig.bVippVe2IspLinkEn;
                                    stIspVeLink.nVipp = pContext->stConfig.nVippDev;
                                    eRet = AW_MPI_VENC_EnableIspVeLink(pContext->nVencChn, &stIspVeLink);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] enable ispVeLink fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    sRegionLinkParam stRegionDetectLink;
                                    eRet = AW_MPI_VENC_GetRegionDetectLink(pContext->nVencChn, &stRegionDetectLink);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] get region detect link fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    stRegionDetectLink.mStaticParam.region_link_en = (unsigned char)pContext->stConfig.bVippRegionLinkEnable;
                                    stRegionDetectLink.mStaticParam.tex_detect_en = (unsigned char)pContext->stConfig.bVippRegionLinkTexDetectEnable;
                                    stRegionDetectLink.mStaticParam.motion_detect_en = (unsigned char)pContext->stConfig.bVippRegionLinkMotionDetectEnable;
                                    //stRegionDetectLink.mStaticParam.updateInterVal = (unsigned char)pContext->stConfig.nVippRegionLinkMotionDetectInterval;
                                    eRet = AW_MPI_VENC_SetRegionDetectLink(pContext->nVencChn, &stRegionDetectLink);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] set region detect link fail:0x%x", pContext->nVencChn, eRet);
                                    }
                                    pContext->bCreateEncLibFlag = true;
                                }
                                MPP_CHN_S stMppViChn = {MOD_ID_VIU, pContext->stConfig.nVippDev, pContext->nViChn};
                                MPP_CHN_S stMppVeChn = {MOD_ID_VENC, 0, pContext->nVencChn};
                                eRet = AW_MPI_SYS_Bind(&stMppViChn, &stMppVeChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! bind vi and venc fail:0x%x", eRet);
                                }
                                eRet = AW_MPI_VI_EnableVirChn(pContext->stConfig.nVippDev, pContext->nViChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! ViChn[%d-%d] enable fail:0x%x", pContext->stConfig.nVippDev, pContext->nViChn, eRet);
                                }
                                eRet = AW_MPI_VENC_StartRecvPic(pContext->nVencChn);
                                if (eRet != SUCCESS)
                                {
                                    aloge("fatal error! vencChn[%d] start fail:0x%x", pContext->nVencChn, eRet);
                                }
                            }
                            pContext->eGetVippFrameThreadState = SampleState_Executing;
                            if (stMsg.pReply)
                            {
                                stMsg.pReply->ReplyResult = 0;
                                cdx_sem_up(&stMsg.pReply->ReplySem);
                            }
                        }
                        else
                        {
                            aloge("fatal error! current state[%d] can't turn to executing!", pContext->eGetVippFrameThreadState);
                        }
                        break;
                    }
                    default:
                    {
                        aloge("fatal error! unknown dst state:%d", stMsg.para0);
                        break;
                    }
                    }
                }
            }
            else if (SampleMsgType_Stop == stMsg.command)
            {
                // Kill thread
                if (pContext->eGetVippFrameThreadState != SampleState_Idle)
                {
                    aloge("fatal error! state[%d] is not idle!", pContext->eGetVippFrameThreadState);
                }
                goto EXIT;
            }
            else
            {
                aloge("fatal error! unknown command:%d", stMsg.command);
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (SampleState_Executing == pContext->eGetVippFrameThreadState)
        {
            int cnt;
            int i;
            if (pContext->bEnableSubVippDisplay && (pContext->stConfig.nSubVippDisplayRotate != 0))
            {
                VIDEO_FRAME_INFO_S stFrameInfo;
                eRet = AW_MPI_VI_GetFrame(pContext->stConfig.nSubVippDev, pContext->nSubViChn, &stFrameInfo, 200);
                if (SUCCESS == eRet)
                {
                    DisplayFrameInfoNode *pDisplayFrameNode = NULL;
                    pthread_mutex_lock(&pContext->stDisplayFrameLock);
                    cnt = list_count_nodes(&pContext->mIdleDisplayFrameList) + list_count_nodes(&pContext->mUsingDisplayFrameList);
                    if (0 == cnt)
                    {
                        for (i=0; i<MAX_DISPLAY_FRAME_NUM; i++)
                        {
                            DisplayFrameInfoNode *pNode = (DisplayFrameInfoNode *)calloc(1, sizeof(DisplayFrameInfoNode));
                            if (NULL == pNode)
                            {
                                aloge("fatal error! malloc fail");
                            }
                            ret = initDisplayFrame(&pNode->stFrame, &stFrameInfo, pContext->stConfig.nSubVippDisplayRotate, i);
                            if (ret != 0)
                            {
                                aloge("fatal error! init display frame fail:%d", ret);
                            }
                            list_add_tail(&pNode->mList, &pContext->mIdleDisplayFrameList);
                        }
                    }
                    pDisplayFrameNode = list_first_entry_or_null(&pContext->mIdleDisplayFrameList, DisplayFrameInfoNode, mList);
                    pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                    if (pDisplayFrameNode)
                    {
                        //verify.
                        if (pDisplayFrameNode->nRefCnt != 0)
                        {
                            aloge("fatal error! check refCnt[%d]", pDisplayFrameNode->nRefCnt);
                        }
                        if (pDisplayFrameNode->stFrame.VFrame.mPixelFormat != stFrameInfo.VFrame.mPixelFormat)
                        {
                            aloge("fatal error! check frame pixel format[%d!=%d]", pDisplayFrameNode->stFrame.VFrame.mPixelFormat,
                                stFrameInfo.VFrame.mPixelFormat);
                        }
                        if ((0==pContext->stConfig.nSubVippDisplayRotate) || (180==pContext->stConfig.nSubVippDisplayRotate))
                        {
                            if ((pDisplayFrameNode->stFrame.VFrame.mWidth != AWALIGN(stFrameInfo.VFrame.mWidth, 16))
                                || (pDisplayFrameNode->stFrame.VFrame.mHeight != AWALIGN(stFrameInfo.VFrame.mHeight, 16)))
                            {
                                aloge("fatal error! check frame width and height[%dx%d,%dx%d]", pDisplayFrameNode->stFrame.VFrame.mWidth,
                                    pDisplayFrameNode->stFrame.VFrame.mHeight, stFrameInfo.VFrame.mWidth, stFrameInfo.VFrame.mHeight);
                            }
                        }
                        else
                        {
                            if ((pDisplayFrameNode->stFrame.VFrame.mWidth != AWALIGN(stFrameInfo.VFrame.mHeight, 16))
                                || (pDisplayFrameNode->stFrame.VFrame.mHeight != AWALIGN(stFrameInfo.VFrame.mWidth, 16)))
                            {
                                aloge("fatal error! check frame width and height[%dx%d,%dx%d]", pDisplayFrameNode->stFrame.VFrame.mWidth,
                                    pDisplayFrameNode->stFrame.VFrame.mHeight, stFrameInfo.VFrame.mWidth, stFrameInfo.VFrame.mHeight);
                            }
                        }
                        //set display frame info. note: we move valid area to left-top of display frame.
                        pDisplayFrameNode->stFrame.VFrame.mOffsetTop = 0;
                        pDisplayFrameNode->stFrame.VFrame.mOffsetLeft = 0;
                        if ((0 == pContext->stConfig.nSubVippDisplayRotate) || (180 == pContext->stConfig.nSubVippDisplayRotate))
                        {
                            pDisplayFrameNode->stFrame.VFrame.mOffsetBottom = stFrameInfo.VFrame.mOffsetBottom - stFrameInfo.VFrame.mOffsetTop;
                            pDisplayFrameNode->stFrame.VFrame.mOffsetRight = stFrameInfo.VFrame.mOffsetRight - stFrameInfo.VFrame.mOffsetLeft;
                        }
                        else
                        {
                            pDisplayFrameNode->stFrame.VFrame.mOffsetBottom = stFrameInfo.VFrame.mOffsetRight - stFrameInfo.VFrame.mOffsetLeft;
                            pDisplayFrameNode->stFrame.VFrame.mOffsetRight = stFrameInfo.VFrame.mOffsetBottom - stFrameInfo.VFrame.mOffsetTop;
                        }
                        pDisplayFrameNode->stFrame.VFrame.mpts = stFrameInfo.VFrame.mpts;
                        pDisplayFrameNode->stFrame.VFrame.mExposureTime = stFrameInfo.VFrame.mExposureTime;
                        pDisplayFrameNode->stFrame.VFrame.mFramecnt = stFrameInfo.VFrame.mFramecnt;
                        pDisplayFrameNode->stFrame.VFrame.mEnvLV = stFrameInfo.VFrame.mEnvLV;
                        pDisplayFrameNode->stFrame.VFrame.mEnvLVAdj = stFrameInfo.VFrame.mEnvLVAdj;
                        pDisplayFrameNode->stFrame.VFrame.mWhoSetFlag = stFrameInfo.VFrame.mWhoSetFlag;
                        //g2d rotate
                        ScalePictureParam stScaleParam;
                        memset(&stScaleParam, 0, sizeof(stScaleParam));
                        stScaleParam.eSrcPixFormat = stFrameInfo.VFrame.mPixelFormat;
                        stScaleParam.eSrcColorSpace = pContext->stConfig.eIspColorSpace;
                        stScaleParam.mSrcPhyAddrs[0] = stFrameInfo.VFrame.mPhyAddr[0];
                        stScaleParam.mSrcPhyAddrs[1] = stFrameInfo.VFrame.mPhyAddr[1];
                        stScaleParam.mSrcPhyAddrs[2] = stFrameInfo.VFrame.mPhyAddr[2];
                        stScaleParam.mSrcPicSize.Width = stFrameInfo.VFrame.mWidth;
                        stScaleParam.mSrcPicSize.Height = stFrameInfo.VFrame.mHeight;
                        stScaleParam.mSrcValidRect.X = stFrameInfo.VFrame.mOffsetLeft;
                        stScaleParam.mSrcValidRect.Y = stFrameInfo.VFrame.mOffsetTop;
                        stScaleParam.mSrcValidRect.Width = stFrameInfo.VFrame.mOffsetRight - stFrameInfo.VFrame.mOffsetLeft;
                        stScaleParam.mSrcValidRect.Height = stFrameInfo.VFrame.mOffsetBottom - stFrameInfo.VFrame.mOffsetTop;
                        stScaleParam.eDstPixFormat = stFrameInfo.VFrame.mPixelFormat;
                        stScaleParam.eDstColorSpace = pContext->stConfig.eIspColorSpace;
                        stScaleParam.mDstPhyAddrs[0] = pDisplayFrameNode->stFrame.VFrame.mPhyAddr[0];
                        stScaleParam.mDstPhyAddrs[1] = pDisplayFrameNode->stFrame.VFrame.mPhyAddr[1];
                        stScaleParam.mDstPhyAddrs[2] = pDisplayFrameNode->stFrame.VFrame.mPhyAddr[2];
                        stScaleParam.mDstPicSize.Width = pDisplayFrameNode->stFrame.VFrame.mWidth;
                        stScaleParam.mDstPicSize.Height = pDisplayFrameNode->stFrame.VFrame.mHeight;
                        stScaleParam.mDstValidRect.X = pDisplayFrameNode->stFrame.VFrame.mOffsetLeft;
                        stScaleParam.mDstValidRect.Y = pDisplayFrameNode->stFrame.VFrame.mOffsetTop;
                        stScaleParam.mDstValidRect.Width = pDisplayFrameNode->stFrame.VFrame.mOffsetRight
                            - pDisplayFrameNode->stFrame.VFrame.mOffsetLeft;
                        stScaleParam.mDstValidRect.Height = pDisplayFrameNode->stFrame.VFrame.mOffsetBottom
                            - pDisplayFrameNode->stFrame.VFrame.mOffsetTop;
                        stScaleParam.nRotate = pContext->stConfig.nSubVippDisplayRotate;
                        ret = ScalePictureByG2d(&stScaleParam, pContext->nG2dDevFd);
                        if (ret != 0)
                        {
                            aloge("fatal error! g2d rotate fail:%d", ret);
                        }
                        pDisplayFrameNode->nRefCnt = 1;
                        pthread_mutex_lock(&pContext->stDisplayFrameLock);
                        list_move_tail(&pDisplayFrameNode->mList, &pContext->mUsingDisplayFrameList);
                        pDisplayFrameNode->nRefCnt++;
                        pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                        eRet = AW_MPI_VO_SendFrame(pContext->nVoLayer, pContext->nVOChn, &pDisplayFrameNode->stFrame, 0);
                        if(eRet != SUCCESS)
                        {
                            aloge("fatal error! why send frame to vo fail? voChn[%d-%d], ret[0x%x]", pContext->nVoLayer, pContext->nVOChn, eRet);
                            pthread_mutex_lock(&pContext->stDisplayFrameLock);
                            pDisplayFrameNode->nRefCnt--;
                            pthread_mutex_unlock(&pContext->stDisplayFrameLock);
                        }
                        ret = ReleaseG2dVideoFrame(pContext, pDisplayFrameNode->stFrame.mId);
                        if (ret != 0)
                        {
                            aloge("fatal error! release g2d frameId[0x%x] fail:%d", pDisplayFrameNode->stFrame.mId, ret);
                        }
                    }
                    else
                    {
                        alogd("no idle g2d DisplayFrame node, ignore this frame");
                    }
                    eRet = AW_MPI_VI_ReleaseFrame(pContext->stConfig.nSubVippDev, pContext->nSubViChn, &stFrameInfo);
                    if (eRet != SUCCESS)
                    {
                        aloge("fatal error! why viChn[%d] release frame fail:0x%x?", pContext->stConfig.nSubVippDev, pContext->nSubViChn, eRet);
                    }
                }
                else
                {
                    aloge("fatal error! why viChn[%d] get frame fail:0x%x?", pContext->stConfig.nSubVippDev, pContext->nSubViChn, eRet);
                }
            }
            else
            {
                TMessage_WaitQueueNotEmpty(&pContext->stGetVippFrameMessageQueue, 10*1000);
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pContext->stGetVippFrameMessageQueue, 10*1000);
        }
    }
EXIT:
    alogd("GetVippFrame thread exit");
    return (void *)SUCCESS;
}

static void *PreviewSwitchThread(void *pThreadData)
{
    int result;
    int ret;
    ERRORTYPE eRet;
    message_t stMsg;
    SampleUvcViCodecContext *pContext = (SampleUvcViCodecContext *)pThreadData;

    alogd("previewSwitch thread start...");
    char strThreadName[32];
    sprintf(strThreadName, "previewSwitchThd");
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    while (1)
    {
PROCESS_MESSAGE:
        if (get_message(&pContext->stPreviewSwitchMessageQueue, &stMsg) == 0)
        {
            // State transition command
            if (SampleMsgType_SetState == stMsg.command)
            {
                if (pContext->ePreviewSwitchThreadState == (SampleState)stMsg.para0)
                {
                    alogd("same state:%d", stMsg.para0);
                    if (stMsg.pReply)
                    {
                        stMsg.pReply->ReplyResult = 0;
                        cdx_sem_up(&stMsg.pReply->ReplySem);
                    }
                }
                else
                {
                    switch ((SampleState)stMsg.para0)
                    {
                    case SampleState_Idle:
                    {
                        if(SampleState_Executing == pContext->ePreviewSwitchThreadState)
                        {
                            //reset voChn, free g2d frame buffer, reset vencChn, uvcChn and vdecChn all turn to idle.
                            alogd("executing->idle");
                            pContext->ePreviewSwitchThreadState = SampleState_Idle;
                            if (stMsg.pReply)
                            {
                                stMsg.pReply->ReplyResult = 0;
                                cdx_sem_up(&stMsg.pReply->ReplySem);
                            }
                        }
                        else
                        {
                            aloge("fatal error! current state[%d] can't turn to idle!", pContext->ePreviewSwitchThreadState);
                        }
                        break;
                    }
                    case SampleState_Executing:
                    {
                        //uvcChn and vdecChn all turn to executing, start voChn, start vencChn.
                        //when get first frame, alloc g2d frame buffer then.
                        if (SampleState_Idle == pContext->ePreviewSwitchThreadState)
                        {
                            alogd("idle->executing");
                            pContext->ePreviewSwitchThreadState = SampleState_Executing;
                            if (stMsg.pReply)
                            {
                                stMsg.pReply->ReplyResult = 0;
                                cdx_sem_up(&stMsg.pReply->ReplySem);
                            }
                        }
                        else
                        {
                            aloge("fatal error! current state[%d] can't turn to executing!", pContext->ePreviewSwitchThreadState);
                        }
                        break;
                    }
                    default:
                    {
                        aloge("fatal error! unknown dst state:%d", stMsg.para0);
                        break;
                    }
                    }
                }
            }
            else if (SampleMsgType_Stop == stMsg.command)
            {
                // Kill thread
                if (pContext->ePreviewSwitchThreadState != SampleState_Idle)
                {
                    aloge("fatal error! state[%d] is not idle!", pContext->ePreviewSwitchThreadState);
                }
                goto EXIT;
            }
            else
            {
                aloge("fatal error! unknown command:%d", stMsg.command);
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (SampleState_Executing == pContext->ePreviewSwitchThreadState)
        {
            if (pContext->bEnableUvc || pContext->bEnableIspVipp)
            {
                ret = TMessage_WaitQueueNotEmpty(&pContext->stPreviewSwitchMessageQueue,
                    pContext->stConfig.nPreviewSwitchInterval*1000);
                if (0 == ret)
                {
                    message_t stMsg;
                    if (PreviewSource_UVC == pContext->eCurPreviewSource)
                    {
                        alogd("switch preview source uvc -> vipp");
                        if (pContext->bEnableVideoEncode || pContext->bEnableAudioEncode)
                        {
                            //GetStream thread turn to idle
                            InitMessage(&stMsg);
                            stMsg.command = SampleMsgType_SetState;
                            stMsg.para0 = SampleState_Idle;
                            stMsg.pReply = ConstructMessageReply();
                            putMessageWithData(&pContext->stGetStreamMessageQueue, &stMsg);
                            while(1)
                            {
                                ret = cdx_sem_down_timedwait(&stMsg.pReply->ReplySem, 5000);
                                if(ret != 0)
                                {
                                    aloge("fatal error! wait state to idle fail[%d]", ret);
                                }
                                else
                                {
                                    break;
                                }
                            }
                            alogd("receive state to idle reply: %d!", stMsg.pReply->ReplyResult);
                            DestructMessageReply(stMsg.pReply);
                            stMsg.pReply = NULL;
                        }
                        //GetUvcDecFrame thread turn to idle
                        InitMessage(&stMsg);
                        stMsg.command = SampleMsgType_SetState;
                        stMsg.para0 = SampleState_Idle;
                        stMsg.pReply = ConstructMessageReply();
                        putMessageWithData(&pContext->stGetUvcVdecFrameMessageQueue, &stMsg);
                        while(1)
                        {
                            ret = cdx_sem_down_timedwait(&stMsg.pReply->ReplySem, 5000);
                            if(ret != 0)
                            {
                                aloge("fatal error! wait state to idle fail[%d]", ret);
                            }
                            else
                            {
                                break;
                            }
                        }
                        alogd("receive state to idle reply: %d!", stMsg.pReply->ReplyResult);
                        DestructMessageReply(stMsg.pReply);
                        stMsg.pReply = NULL;

                        if (pContext->bEnableIspVipp)
                        {
                            pContext->eCurPreviewSource = PreviewSource_VIPP;
                        }
                        else
                        {
                            alogw("Be careful! only uvc enable, so preview switch to uvc again!");
                            pContext->eCurPreviewSource = PreviewSource_UVC;
                        }

                        //GetVippFrame thread turn to executing.
                        InitMessage(&stMsg);
                        stMsg.command = SampleMsgType_SetState;
                        stMsg.para0 = SampleState_Executing;
                        stMsg.pReply = ConstructMessageReply();
                        if (PreviewSource_VIPP == pContext->eCurPreviewSource)
                        {
                            putMessageWithData(&pContext->stGetVippFrameMessageQueue, &stMsg);
                        }
                        else if (PreviewSource_UVC == pContext->eCurPreviewSource)
                        {
                            putMessageWithData(&pContext->stGetUvcVdecFrameMessageQueue, &stMsg);
                        }
                        while(1)
                        {
                            ret = cdx_sem_down_timedwait(&stMsg.pReply->ReplySem, 5000);
                            if(ret != 0)
                            {
                                aloge("fatal error! wait state to executing fail[%d]", ret);
                            }
                            else
                            {
                                break;
                            }
                        }
                        alogd("receive state to executing reply: %d!", stMsg.pReply->ReplyResult);
                        DestructMessageReply(stMsg.pReply);
                        stMsg.pReply = NULL;

                        if (pContext->bEnableVideoEncode || pContext->bEnableAudioEncode)
                        {
                            //GetStream thread turn to executing
                            InitMessage(&stMsg);
                            stMsg.command = SampleMsgType_SetState;
                            stMsg.para0 = SampleState_Executing;
                            putMessageWithData(&pContext->stGetStreamMessageQueue, &stMsg);
                        }
                    }
                    else
                    {
                        alogd("switch preview source vipp -> uvc");
                        if (pContext->bEnableVideoEncode || pContext->bEnableAudioEncode)
                        {
                            //GetStream thread turn to idle
                            InitMessage(&stMsg);
                            stMsg.command = SampleMsgType_SetState;
                            stMsg.para0 = SampleState_Idle;
                            stMsg.pReply = ConstructMessageReply();
                            putMessageWithData(&pContext->stGetStreamMessageQueue, &stMsg);
                            while(1)
                            {
                                ret = cdx_sem_down_timedwait(&stMsg.pReply->ReplySem, 5000);
                                if(ret != 0)
                                {
                                    aloge("fatal error! wait state to idle fail[%d]", ret);
                                }
                                else
                                {
                                    break;
                                }
                            }
                            alogd("receive state to idle reply: %d!", stMsg.pReply->ReplyResult);
                            DestructMessageReply(stMsg.pReply);
                            stMsg.pReply = NULL;
                        }
                        //GetVippFrame thread turn to idle.
                        InitMessage(&stMsg);
                        stMsg.command = SampleMsgType_SetState;
                        stMsg.para0 = SampleState_Idle;
                        stMsg.pReply = ConstructMessageReply();
                        putMessageWithData(&pContext->stGetVippFrameMessageQueue, &stMsg);
                        while(1)
                        {
                            ret = cdx_sem_down_timedwait(&stMsg.pReply->ReplySem, 5000);
                            if(ret != 0)
                            {
                                aloge("fatal error! wait state to idle fail[%d]", ret);
                            }
                            else
                            {
                                break;
                            }
                        }
                        alogd("receive state to idle reply: %d!", stMsg.pReply->ReplyResult);
                        DestructMessageReply(stMsg.pReply);
                        stMsg.pReply = NULL;

                        if (pContext->bEnableUvc)
                        {
                            pContext->eCurPreviewSource = PreviewSource_UVC;
                        }
                        else
                        {
                            alogw("Be careful! only vipp enable, so preview switch to vipp again!");
                            pContext->eCurPreviewSource = PreviewSource_VIPP;
                        }

                        //GetUvcDecFrame thread turn to executing
                        InitMessage(&stMsg);
                        stMsg.command = SampleMsgType_SetState;
                        stMsg.para0 = SampleState_Executing;
                        stMsg.pReply = ConstructMessageReply();
                        if (PreviewSource_UVC == pContext->eCurPreviewSource)
                        {
                            putMessageWithData(&pContext->stGetUvcVdecFrameMessageQueue, &stMsg);
                        }
                        else if (PreviewSource_VIPP == pContext->eCurPreviewSource)
                        {
                            putMessageWithData(&pContext->stGetVippFrameMessageQueue, &stMsg);
                        }
                        while(1)
                        {
                            ret = cdx_sem_down_timedwait(&stMsg.pReply->ReplySem, 5000);
                            if(ret != 0)
                            {
                                aloge("fatal error! wait state to idle fail[%d]", ret);
                            }
                            else
                            {
                                break;
                            }
                        }
                        alogd("receive state to executing reply: %d!", stMsg.pReply->ReplyResult);
                        DestructMessageReply(stMsg.pReply);
                        stMsg.pReply = NULL;

                        if (pContext->bEnableVideoEncode || pContext->bEnableAudioEncode)
                        {
                            //GetStream thread turn to executing
                            InitMessage(&stMsg);
                            stMsg.command = SampleMsgType_SetState;
                            stMsg.para0 = SampleState_Executing;
                            putMessageWithData(&pContext->stGetStreamMessageQueue, &stMsg);
                        }
                    }
                }
            }
            else
            {
                aloge("fatal error! uvc and vipp not all enable, so preview switch is impossbile!");
                TMessage_WaitQueueNotEmpty(&pContext->stPreviewSwitchMessageQueue, 0);
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pContext->stPreviewSwitchMessageQueue, 10*1000);
        }
    }
EXIT:
    alogd("previewSwitch thread exit");
    return (void *)SUCCESS;
}

static void *GetStreamThread(void *pThreadData)
{
    int result;
    int ret;
    ERRORTYPE eRet;
    int cnt;
    message_t stMsg;
    SampleUvcViCodecContext *pContext = (SampleUvcViCodecContext *)pThreadData;

    alogd("GetStream thread start...");
    char strThreadName[32];
    sprintf(strThreadName, "GetStreamThd");
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    int nVeChnFd = -1;
    int nAeChnFd = -1;
    int nMaxFd = -1;
    if(pContext->bEnableVideoEncode)
    {
        nVeChnFd = AW_MPI_VENC_GetHandle(pContext->nVencChn);
        if (nVeChnFd < 0)
        {
            aloge("fatal error! venc handle[%d] wrong", nVeChnFd);
        }
    }
    if(pContext->bEnableAudioEncode)
    {
        nAeChnFd = AW_MPI_AENC_GetHandle(pContext->nAEncChn);
        if (nAeChnFd < 0)
        {
            aloge("fatal error! aenc handle[%d] wrong", nAeChnFd);
        }
    }
    nMaxFd = (nVeChnFd > nAeChnFd) ? nVeChnFd : nAeChnFd;
    alogd("VeFd:%d, AeFd:%d, maxFd:%d", nVeChnFd, nAeChnFd, nMaxFd);

#ifdef SUPPORT_RTSP_TEST
    if (pContext->bEnableRtsp && pContext->bEnableVideoEncode)
    {
        RtspServerAttr rtsp_attr;
        memset(&rtsp_attr, 0, sizeof(RtspServerAttr));
        rtsp_attr.net_type = pContext->stConfig.nRtspNetType;

        if (PT_H264 == pContext->stConfig.eVencType)
            rtsp_attr.video_type = RTSP_VIDEO_TYPE_H264;
        else if (PT_H265 == pContext->stConfig.eVencType)
            rtsp_attr.video_type = RTSP_VIDEO_TYPE_H265;
        else
            rtsp_attr.video_type = RTSP_VIDEO_TYPE_LAST;

        rtsp_attr.frame_rate = pContext->stConfig.nUvcCaptureFrameRate;

        ret = rtsp_open(pContext->stConfig.nRtspId, &rtsp_attr);
        if (ret)
        {
            aloge("fatal error! Do rtsp_open fail! ret:%d", ret);
        }
        rtsp_start(pContext->stConfig.nRtspId);
    }
#endif

    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    memset(&stVencStream, 0, sizeof(stVencStream));
    memset(&stVencPack, 0, sizeof(stVencPack));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;

    AUDIO_STREAM_S stAencStream;
    memset(&stAencStream, 0, sizeof(stAencStream));

    while (1)
    {
PROCESS_MESSAGE:
        if (get_message(&pContext->stGetStreamMessageQueue, &stMsg) == 0)
        {
            // State transition command
            if (SampleMsgType_SetState == stMsg.command)
            {
                if (pContext->eGetStreamThreadState == (SampleState)stMsg.para0)
                {
                    alogd("same state:%d", stMsg.para0);
                    if (stMsg.pReply)
                    {
                        stMsg.pReply->ReplyResult = 0;
                        cdx_sem_up(&stMsg.pReply->ReplySem);
                    }
                }
                else
                {
                    switch ((SampleState)stMsg.para0)
                    {
                    case SampleState_Idle:
                    {
                        if(SampleState_Executing == pContext->eGetStreamThreadState)
                        {
                            //close file.
                            alogd("executing->idle");
                            if (pContext->bEnableWriteVideoFile)
                            {
                                if (pContext->pVencFileFp)
                                {
                                    fclose(pContext->pVencFileFp);
                                    pContext->pVencFileFp = NULL;
                                }
                            }
                            if (pContext->bEnableWriteAudioFile)
                            {
                                if (pContext->pAencFileFp)
                                {
                                    fclose(pContext->pAencFileFp);
                                    pContext->pAencFileFp = NULL;
                                }
                            }
                            pContext->eGetStreamThreadState = SampleState_Idle;
                            if (stMsg.pReply)
                            {
                                stMsg.pReply->ReplyResult = 0;
                                cdx_sem_up(&stMsg.pReply->ReplySem);
                            }
                        }
                        else
                        {
                            aloge("fatal error! current state[%d] can't turn to idle!", pContext->eGetStreamThreadState);
                        }
                        break;
                    }
                    case SampleState_Executing:
                    {
                        if (SampleState_Idle == pContext->eGetStreamThreadState)
                        {
                            alogd("idle->executing");
                            if (pContext->bEnableVideoEncode && pContext->bEnableWriteVideoFile)
                            {
                                if (pContext->pVencFileFp != NULL)
                                {
                                    aloge("fatal error! why venc file fp is not NULL?");
                                    fclose(pContext->pVencFileFp);
                                    pContext->pVencFileFp = NULL;
                                }
                                char strVencFilePath[256];
                                generateVencFilePath(strVencFilePath, sizeof(strVencFilePath), pContext);
                                pContext->pVencFileFp = fopen(strVencFilePath, "wb");
                                if (NULL == pContext->pVencFileFp)
                                {
                                    aloge("fatal error! create file[%s] fail.", strVencFilePath);
                                }
                                pContext->VencFileStartPts = -1;

                                //add to fileList.
                                FilePathNode *pFileNode = (FilePathNode *)calloc(1, sizeof(FilePathNode));
                                if (NULL == pFileNode)
                                {
                                    aloge("fatal error! malloc fail");
                                }
                                strcpy(pFileNode->strFilePath, strVencFilePath);
                                list_add_tail(&pFileNode->mList, &pContext->VencFileList);

                                //get spspps
                                if (PT_H264 == pContext->stConfig.eVencType)
                                {
                                    eRet = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->nVencChn, &pContext->stSpsPpsInfo);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] get H264SpsPpsInfo failed[0x%x]!", pContext->nVencChn, eRet);
                                    }
                                }
                                else if (PT_H265 == pContext->stConfig.eVencType)
                                {
                                    eRet = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->nVencChn, &pContext->stSpsPpsInfo);
                                    if (eRet != SUCCESS)
                                    {
                                        aloge("fatal error! vencChn[%d] get H265SpsPpsInfo failed[0x%x]!", pContext->nVencChn, eRet);
                                    }
                                }
                            }
                            if (pContext->bEnableAudioEncode && pContext->bEnableWriteAudioFile)
                            {
                                if (pContext->pAencFileFp != NULL)
                                {
                                    aloge("fatal error! why aenc file fp is not NULL?");
                                    fclose(pContext->pAencFileFp);
                                    pContext->pAencFileFp = NULL;
                                }
                                char strAencFilePath[256];
                                generateAencFilePath(strAencFilePath, sizeof(strAencFilePath), pContext);
                                pContext->pAencFileFp = fopen(strAencFilePath, "wb");
                                if (NULL == pContext->pAencFileFp)
                                {
                                    aloge("fatal error! create file[%s] fail.", strAencFilePath);
                                }
                                pContext->AencFileStartPts = -1;

                                //add to fileList.
                                FilePathNode *pFileNode = (FilePathNode *)calloc(1, sizeof(FilePathNode));
                                if (NULL == pFileNode)
                                {
                                    aloge("fatal error! malloc fail");
                                }
                                strcpy(pFileNode->strFilePath, strAencFilePath);
                                list_add_tail(&pFileNode->mList, &pContext->AencFileList);
                            }
                            pContext->eGetStreamThreadState = SampleState_Executing;
                            if (stMsg.pReply)
                            {
                                stMsg.pReply->ReplyResult = 0;
                                cdx_sem_up(&stMsg.pReply->ReplySem);
                            }
                        }
                        else
                        {
                            aloge("fatal error! current state[%d] can't turn to executing!", pContext->eGetStreamThreadState);
                        }
                        break;
                    }
                    default:
                    {
                        aloge("fatal error! unknown dst state:%d", stMsg.para0);
                        break;
                    }
                    }
                }
            }
            else if (SampleMsgType_Stop == stMsg.command)
            {
                // Kill thread
                if (pContext->eGetStreamThreadState != SampleState_Idle)
                {
                    aloge("fatal error! state[%d] is not idle!", pContext->eGetStreamThreadState);
                }
                goto EXIT;
            }
            else
            {
                aloge("fatal error! unknown command:%d", stMsg.command);
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (SampleState_Executing == pContext->eGetStreamThreadState)
        {
            handle_set rdFds;
            AW_MPI_SYS_HANDLE_ZERO(&rdFds);
            if(pContext->bEnableVideoEncode)
            {
                AW_MPI_SYS_HANDLE_SET(nVeChnFd, &rdFds);
            }
            if(pContext->bEnableAudioEncode)
            {
                AW_MPI_SYS_HANDLE_SET(nAeChnFd, &rdFds);
            }
            int nReadyCnt = AW_MPI_SYS_HANDLE_Select(nMaxFd+1, &rdFds, 200);
            if(nReadyCnt > 0)
            {
                if(pContext->bEnableVideoEncode)
                {
                    if(AW_MPI_SYS_HANDLE_ISSET(nVeChnFd, &rdFds))
                    {
                        while(1)
                        {
                            eRet = AW_MPI_VENC_GetStream(pContext->nVencChn, &stVencStream, 0);
                            if (SUCCESS == eRet)
                            {
                                alogv("Venc stream addr %px-%px-%px, Len %d-%d-%d", stVencStream.mpPack->mpAddr0, stVencStream.mpPack->mpAddr1,
                                    stVencStream.mpPack->mpAddr2, stVencStream.mpPack->mLen0, stVencStream.mpPack->mLen1, stVencStream.mpPack->mLen2);
                                if (-1 == pContext->VencFileStartPts)
                                {
                                    pContext->VencFileStartPts = stVencStream.mpPack[0].mPTS;
                                }
                                bool bKeyFrameFlag = IsKeyFrame(&stVencStream, pContext);
                                if (pContext->bEnableWriteVideoFile)
                                {
                                    //switch file if reach file duration.
                                    if (bKeyFrameFlag && (pContext->stConfig.nVencFileDuration > 0))
                                    {
                                        if ((stVencStream.mpPack[0].mPTS - pContext->VencFileStartPts)/1000000 >= pContext->stConfig.nVencFileDuration)
                                        {
                                            fclose(pContext->pVencFileFp);
                                            pContext->pVencFileFp = NULL;
                                            char strVencFilePath[256];
                                            generateVencFilePath(strVencFilePath, sizeof(strVencFilePath), pContext);
                                            pContext->pVencFileFp = fopen(strVencFilePath, "wb");
                                            if (NULL == pContext->pVencFileFp)
                                            {
                                                aloge("fatal error! create file[%s] fail.", strVencFilePath);
                                            }
                                            pContext->VencFileStartPts = stVencStream.mpPack[0].mPTS;

                                            //add to fileList.
                                            FilePathNode *pFileNode = (FilePathNode *)calloc(1, sizeof(FilePathNode));
                                            if (NULL == pFileNode)
                                            {
                                                aloge("fatal error! malloc fail");
                                            }
                                            strcpy(pFileNode->strFilePath, strVencFilePath);
                                            list_add_tail(&pFileNode->mList, &pContext->VencFileList);
                                            cnt = list_count_nodes(&pContext->VencFileList);
                                            while (cnt > pContext->stConfig.nVencFileNum)
                                            {
                                                pFileNode = list_first_entry(&pContext->VencFileList, FilePathNode, mList);
                                                ret = remove(pFileNode->strFilePath);
                                                if (ret != 0)
                                                {
                                                    aloge("fatal error! delete file[%s] failed:%s", pFileNode->strFilePath, strerror(errno));
                                                }
                                                list_del(&pFileNode->mList);
                                                free(pFileNode);
                                                cnt = list_count_nodes(&pContext->VencFileList);
                                            }
                                        }
                                    }
                                    if (bKeyFrameFlag)
                                    {
                                        ret = fwrite(pContext->stSpsPpsInfo.pBuffer, 1, pContext->stSpsPpsInfo.nLength, pContext->pVencFileFp);
                                        if (ret != pContext->stSpsPpsInfo.nLength)
                                        {
                                            aloge("fatal error! write fail:%d!=%d", ret, pContext->stSpsPpsInfo.nLength);
                                        }
                                    }
                                    if(stVencStream.mpPack[0].mLen0 > 0)
                                    {
                                        ret = fwrite(stVencStream.mpPack[0].mpAddr0, 1, stVencStream.mpPack[0].mLen0, pContext->pVencFileFp);
                                        if (ret != stVencStream.mpPack[0].mLen0)
                                        {
                                            aloge("fatal error! write fail:%d!=%d", ret, stVencStream.mpPack[0].mLen0);
                                        }
                                    }
                                    if(stVencStream.mpPack[0].mLen1 > 0)
                                    {
                                        ret = fwrite(stVencStream.mpPack[0].mpAddr1, 1, stVencStream.mpPack[0].mLen1, pContext->pVencFileFp);
                                        if (ret != stVencStream.mpPack[0].mLen1)
                                        {
                                            aloge("fatal error! write fail:%d!=%d", ret, stVencStream.mpPack[0].mLen1);
                                        }
                                    }
                                    if(stVencStream.mpPack[0].mLen2 > 0)
                                    {
                                        ret = fwrite(stVencStream.mpPack[0].mpAddr2, 1, stVencStream.mpPack[0].mLen2, pContext->pVencFileFp);
                                        if (ret != stVencStream.mpPack[0].mLen2)
                                        {
                                            aloge("fatal error! write fail:%d!=%d", ret, stVencStream.mpPack[0].mLen2);
                                        }
                                    }
                                }
                            #ifdef SUPPORT_RTSP_TEST
                                if (pContext->bEnableRtsp)
                                {
                                    char *pRtspStreamBuf = NULL;
                                    int nStreamSize = 0;
                                    if (bKeyFrameFlag || (stVencStream.mpPack[0].mLen1 > 0)) //will use pRtspStreamBuf to copy
                                    {
                                        if (bKeyFrameFlag)
                                        {
                                            nStreamSize += pContext->stSpsPpsInfo.nLength;
                                        }
                                        nStreamSize += (stVencStream.mpPack[0].mLen0 + stVencStream.mpPack[0].mLen1 + stVencStream.mpPack[0].mLen2);
                                        if (pContext->nRtspStreamBufSize < nStreamSize)
                                        {
                                            if (pContext->pRtspStreamBuf != NULL)
                                            {
                                                free(pContext->pRtspStreamBuf);
                                                pContext->pRtspStreamBuf = NULL;
                                            }
                                            pContext->pRtspStreamBuf = (char *)malloc(nStreamSize);
                                            if (NULL == pContext->pRtspStreamBuf)
                                            {
                                                aloge("fatal error! malloc fail");
                                            }
                                            pContext->nRtspStreamBufSize = nStreamSize;
                                        }
                                        pRtspStreamBuf = pContext->pRtspStreamBuf;
                                        if (bKeyFrameFlag)
                                        {
                                            memcpy(pRtspStreamBuf, pContext->stSpsPpsInfo.pBuffer, pContext->stSpsPpsInfo.nLength);
                                            pRtspStreamBuf += pContext->stSpsPpsInfo.nLength;
                                        }
                                        if(stVencStream.mpPack[0].mLen0 > 0)
                                        {
                                            memcpy(pRtspStreamBuf, stVencStream.mpPack[0].mpAddr0, stVencStream.mpPack[0].mLen0);
                                            pRtspStreamBuf += stVencStream.mpPack[0].mLen0;
                                        }
                                        if(stVencStream.mpPack[0].mLen1 > 0)
                                        {
                                            memcpy(pRtspStreamBuf, stVencStream.mpPack[0].mpAddr1, stVencStream.mpPack[0].mLen1);
                                            pRtspStreamBuf += stVencStream.mpPack[0].mLen1;
                                        }
                                        if(stVencStream.mpPack[0].mLen2 > 0)
                                        {
                                            memcpy(pRtspStreamBuf, stVencStream.mpPack[0].mpAddr2, stVencStream.mpPack[0].mLen2);
                                            pRtspStreamBuf += stVencStream.mpPack[0].mLen2;
                                        }
                                        pRtspStreamBuf = pContext->pRtspStreamBuf;
                                    }
                                    else
                                    {
                                        pRtspStreamBuf = stVencStream.mpPack[0].mpAddr0;
                                        nStreamSize = stVencStream.mpPack[0].mLen0;
                                    }
                                        
                                    /* get current fps and set to rtsp to avoid rtsp checkDurationTime warn */
                                    int nFps = 0;
                                    //AW_MPI_ISP_GetSensorFps(pStreamContext->mIsp, &sensor_fps);
                                    if (PreviewSource_UVC == pContext->eCurPreviewSource)
                                    {
                                        nFps = pContext->stConfig.nUvcCaptureFrameRate;
                                    }
                                    else if (PreviewSource_VIPP == pContext->eCurPreviewSource)
                                    {
                                        nFps = pContext->stConfig.nIspCaptureFrameRate;
                                    }

                                    RtspSendDataParam stRtspParam;
                                    memset(&stRtspParam, 0, sizeof(RtspSendDataParam));
                                    stRtspParam.buf = pRtspStreamBuf;
                                    stRtspParam.size = nStreamSize;
                                    stRtspParam.frame_type = (bKeyFrameFlag) ? RTSP_FRAME_DATA_TYPE_I : RTSP_FRAME_DATA_TYPE_P;
                                    stRtspParam.pts = stVencStream.mpPack[0].mPTS;
                                    stRtspParam.frame_rate = nFps;
                                    alogv("vencChn[%d] rtsp id[%d] RtspParam %p %d %d %lld, %dfps", pContext->nVencChn, pContext->stConfig.nRtspId,
                                        stRtspParam.buf, stRtspParam.size, stRtspParam.frame_type, stRtspParam.pts, stRtspParam.frame_rate);
                                    int64_t start_time = CDX_GetSysTimeUsMonotonic()/1000;
                                    rtsp_sendData(pContext->stConfig.nRtspId, &stRtspParam);
                                    int64_t end_time = CDX_GetSysTimeUsMonotonic()/1000;
                                    if (100 < end_time - start_time)
                                    {
                                        alogw("Be careful! vencChn[%d] rtsp_sendData cost %dms > 100ms", pContext->nVencChn, end_time - start_time);
                                    }
                                }
                            #endif
                                eRet = AW_MPI_VENC_ReleaseStream(pContext->nVencChn, &stVencStream);
                                if(eRet != SUCCESS)
                                {
                                    aloge("fatal error! vencChn[%d] releaseFrame failed:0x%x", pContext->nVencChn, eRet);
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
                if(pContext->bEnableAudioEncode)
                {
                    if(AW_MPI_SYS_HANDLE_ISSET(nAeChnFd, &rdFds))
                    {
                        while(1)
                        {
                            eRet = AW_MPI_AENC_GetStream(pContext->nAEncChn, &stAencStream, 0);
                            if (SUCCESS == eRet)
                            {
                                alogv("Aenc stream addr %px-%px, Len %d-%d", stAencStream.pStream, stAencStream.pStreamExtra, stAencStream.mLen,
                                    stAencStream.mExtraLen);
                                if (-1 == pContext->AencFileStartPts)
                                {
                                    pContext->AencFileStartPts = stAencStream.mTimeStamp;
                                }
                                if (pContext->bEnableWriteAudioFile)
                                {
                                    //switch file if reach file duration.
                                    if (pContext->stConfig.nAencFileDuration > 0)
                                    {
                                        if ((stAencStream.mTimeStamp - pContext->AencFileStartPts)/1000000 >= pContext->stConfig.nAencFileDuration)
                                        {
                                            fclose(pContext->pAencFileFp);
                                            pContext->pAencFileFp = NULL;
                                            char strAencFilePath[256];
                                            generateAencFilePath(strAencFilePath, sizeof(strAencFilePath), pContext);
                                            pContext->pAencFileFp = fopen(strAencFilePath, "wb");
                                            if (NULL == pContext->pAencFileFp)
                                            {
                                                aloge("fatal error! create file[%s] fail.", strAencFilePath);
                                            }
                                            pContext->AencFileStartPts = stAencStream.mTimeStamp;

                                            //add to fileList.
                                            FilePathNode *pFileNode = (FilePathNode *)calloc(1, sizeof(FilePathNode));
                                            if (NULL == pFileNode)
                                            {
                                                aloge("fatal error! malloc fail");
                                            }
                                            strcpy(pFileNode->strFilePath, strAencFilePath);
                                            list_add_tail(&pFileNode->mList, &pContext->AencFileList);
                                            cnt = list_count_nodes(&pContext->AencFileList);
                                            while (cnt > pContext->stConfig.nAencFileNum)
                                            {
                                                pFileNode = list_first_entry(&pContext->AencFileList, FilePathNode, mList);
                                                ret = remove(pFileNode->strFilePath);
                                                if (ret != 0)
                                                {
                                                    aloge("fatal error! delete file[%s] failed:%s", pFileNode->strFilePath, strerror(errno));
                                                }
                                                list_del(&pFileNode->mList);
                                                free(pFileNode);
                                                cnt = list_count_nodes(&pContext->AencFileList);
                                            }
                                        }
                                    }
                                    if(stAencStream.mLen > 0)
                                    {
                                        ret = fwrite(stAencStream.pStream, 1, stAencStream.mLen, pContext->pAencFileFp);
                                        if (ret != stAencStream.mLen)
                                        {
                                            aloge("fatal error! write fail:%d!=%d", ret, stAencStream.mLen);
                                        }
                                    }
                                    if(stAencStream.mExtraLen > 0)
                                    {
                                        ret = fwrite(stAencStream.pStreamExtra, 1, stAencStream.mExtraLen, pContext->pAencFileFp);
                                        if (ret != stAencStream.mExtraLen)
                                        {
                                            aloge("fatal error! write fail:%d!=%d", ret, stAencStream.mExtraLen);
                                        }
                                    }
                                }
                                eRet = AW_MPI_AENC_ReleaseStream(pContext->nAEncChn, &stAencStream);
                                if(eRet != SUCCESS)
                                {
                                    aloge("fatal error! aencChn[%d] releaseFrame failed:0x%x", pContext->nAEncChn, eRet);
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pContext->stGetStreamMessageQueue, 10*1000);
        }
    }
EXIT:
    cnt = list_count_nodes(&pContext->VencFileList);
    if (cnt > 0)
    {
        FilePathNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pContext->VencFileList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    cnt = list_count_nodes(&pContext->AencFileList);
    if (cnt > 0)
    {
        FilePathNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pContext->AencFileList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    
#ifdef SUPPORT_RTSP_TEST
    if (pContext->bEnableRtsp && pContext->bEnableVideoEncode)
    {
        rtsp_stop(pContext->stConfig.nRtspId);
        rtsp_close(pContext->stConfig.nRtspId);
    }
#endif
    alogd("GetStream thread exit");
    return (void *)SUCCESS;
}

int main(int argc, char **argv)
{
    //kernel_fwrite("dmesg: app[%s] begin\n", argv[0]);
    int result = 0;
    int ret;
    ERRORTYPE eRet;
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
    aloge("log_init done");

    SampleUvcViCodecContext *pContext = (SampleUvcViCodecContext *)malloc(sizeof(SampleUvcViCodecContext));
    initSampleUvcViCodecContext(pContext);
    gpSampleUvcViCodecContext = pContext;

    if(ParseCmdLine(argc, argv, &pContext->stCmdLineParam) != 0)
    {
        result = -1;
        goto _exit;
    }

    char *pConfigFilePath = NULL;
    if(strlen(pContext->stCmdLineParam.strConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->stCmdLineParam.strConfigFilePath;
    }
    if(LoadSampleUvcViCodecConfig(&pContext->stConfig, pConfigFilePath) != 0)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    if (strlen(pContext->stConfig.strUvcDevName) > 0)
    {
        pContext->bEnableUvc = true;
    }
    if ((pContext->stConfig.nUvcDisplayWidth > 0) && (pContext->stConfig.nUvcDisplayHeight > 0) && pContext->bEnableUvc)
    {
        pContext->bEnableUvcDisplay = true;
    }
    if (pContext->stConfig.nVippDev >= 0)
    {
        pContext->bEnableIspVipp = true;
    }
    if ((pContext->stConfig.nSubVippDisplayWidth > 0) && (pContext->stConfig.nSubVippDisplayHeight > 0)
        && pContext->bEnableIspVipp)
    {
        pContext->bEnableSubVippDisplay = true;
    }
    if (pContext->stConfig.eVencType != PT_MAX)
    {
        pContext->bEnableVideoEncode = true;
    }
    if (strlen(pContext->stConfig.strVencFilePath) > 0)
    {
        pContext->bEnableWriteVideoFile = true;
    }
    if (pContext->stConfig.nRtspId >= 0)
    {
        pContext->bEnableRtsp = true;
    }
    if (pContext->stConfig.nMicNum > 0)
    {
        pContext->bEnableAudio = true;
    }
    if ((pContext->stConfig.eAencType != PT_MAX) && pContext->bEnableAudio)
    {
        pContext->bEnableAudioEncode = true;
    }
    if (strlen(pContext->stConfig.strAencFilePath) > 0)
    {
        pContext->bEnableWriteAudioFile = true;
    }

    if ((false == pContext->bEnableUvc) && (false == pContext->bEnableIspVipp))
    {
        aloge("fatal error! config wrong, uvc and ispVipp at lease enable one!");
        result = -1;
        goto _exit;
    }
    if (false == pContext->bEnableUvc)
    {
        if (PreviewSource_UVC == pContext->stConfig.ePreviewSource)
        {
            aloge("fatal error! config wrong, force change to preview vipp");
            pContext->stConfig.ePreviewSource = PreviewSource_VIPP;
            pContext->stConfig.nPreviewSwitchInterval = 0;
        }
    }
    if (false == pContext->bEnableIspVipp)
    {
        if (PreviewSource_VIPP == pContext->stConfig.ePreviewSource)
        {
            aloge("fatal error! config wrong, force change to preview uvc");
            pContext->stConfig.ePreviewSource = PreviewSource_UVC;
            pContext->stConfig.nPreviewSwitchInterval = 0;
        }
    }
    pContext->eCurPreviewSource = pContext->stConfig.ePreviewSource;
    
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }

    //1. prepare mpp channles.
    memset(&pContext->stSysconf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->stSysconf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->stSysconf);
    AW_MPI_SYS_Init();
    //AW_MPI_VDEC_SetVEFreq(MM_INVALID_CHN, 648);

    if (pContext->bEnableUvc)
    {
        eRet = AW_MPI_UVC_CreateDevice(pContext->stConfig.strUvcDevName);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! uvcDev[%s] can not create, ret:0x%x!", pContext->stConfig.strUvcDevName, eRet);
        }
        memset(&pContext->stUvcAttr, 0, sizeof(pContext->stUvcAttr));
        pContext->stUvcAttr.mPixelformat = UVC_MJPEG;
        pContext->stUvcAttr.mUvcVideo_BufCnt = pContext->stConfig.nUvcCaptureVideoBufCnt;
        pContext->stUvcAttr.mUvcVideo_Width = pContext->stConfig.nUvcCaptureWidth;
        pContext->stUvcAttr.mUvcVideo_Height = pContext->stConfig.nUvcCaptureHeight;
        pContext->stUvcAttr.mUvcVideo_Fps = pContext->stConfig.nUvcCaptureFrameRate;
        pContext->stUvcAttr.nMaxVideoFrameSize = AWALIGN((unsigned int)(pContext->stConfig.nUvcCaptureWidth
            * pContext->stConfig.nUvcCaptureHeight * 3 / 2 * pContext->stConfig.fUvcCaptureMaxFramesizeRatio), 1024);
        eRet = AW_MPI_UVC_SetDeviceAttr(pContext->stConfig.strUvcDevName, &pContext->stUvcAttr);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! UVC[%s] can not set device attr, ret:0x%x", pContext->stConfig.strUvcDevName, eRet);
        }
        eRet = AW_MPI_UVC_GetDeviceAttr(pContext->stConfig.strUvcDevName, &pContext->stUvcAttr);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! UVC[%s] can not get device attr", pContext->stConfig.strUvcDevName);
        }
        eRet = AW_MPI_UVC_EnableDevice(pContext->stConfig.strUvcDevName);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! UVC[%s] device can not start", pContext->stConfig.strUvcDevName);
        }
        pContext->nUvcChn = SampleUvcViCodec_UVCCHN;
        eRet = AW_MPI_UVC_CreateVirChn(pContext->stConfig.strUvcDevName, pContext->nUvcChn);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! UVC[%s] can not create virchannel[%d]", pContext->stConfig.strUvcDevName, pContext->nUvcChn);
            pContext->nUvcChn = MM_INVALID_CHN;
        }
        configVdecChnAttr(pContext);
        pContext->nVdecChn = SampleUvcViCodec_VDECCHN;
        eRet = AW_MPI_VDEC_CreateChn(pContext->nVdecChn, &pContext->stVdecChnAttr);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! create vdecChn[%d] fail:0x%x", pContext->nVdecChn, eRet);
            pContext->nVdecChn = MM_INVALID_CHN;
        }
    }

    if (pContext->bEnableIspVipp)
    {
        pContext->nIspDev = SampleUvcViCodec_ISPDEV;
        pContext->nViChn = SampleUvcViCodec_VICHN;
        eRet = AW_MPI_VI_CreateVipp(pContext->stConfig.nVippDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! AW_MPI_VI CreateVipp[%d] failed", pContext->stConfig.nVippDev);
        }
        memset(&pContext->stViAttr, 0, sizeof(VI_ATTR_S));
        pContext->stViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pContext->stViAttr.memtype = V4L2_MEMORY_MMAP;
        pContext->stViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->stConfig.eVippPixelFormat);
        pContext->stViAttr.format.field = V4L2_FIELD_NONE;
        pContext->stViAttr.format.colorspace = pContext->stConfig.eIspColorSpace;
        pContext->stViAttr.format.width = pContext->stConfig.nVippCaptureWidth;
        pContext->stViAttr.format.height = pContext->stConfig.nVippCaptureHeight;
        pContext->stViAttr.nbufs =  pContext->stConfig.nVippBufNum;
        pContext->stViAttr.nplanes = 2;
        pContext->stViAttr.wdr_mode = 0;
        pContext->stViAttr.fps = pContext->stConfig.nIspCaptureFrameRate;
        pContext->stViAttr.drop_frame_num = 0;
        alogd("vipp[%d] use %d v4l2 buffers, colorspace: 0x%x, wdr_mode:%d", pContext->stConfig.nVippDev,
            pContext->stViAttr.nbufs, pContext->stViAttr.format.colorspace, pContext->stViAttr.wdr_mode);
        eRet = AW_MPI_VI_SetVippAttr(pContext->stConfig.nVippDev, &pContext->stViAttr);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vipp[%d] SetVippAttr failed:0x%x", pContext->stConfig.nVippDev, eRet);
        }
        eRet = AW_MPI_ISP_Run(pContext->nIspDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! isp[%d] run fail:0x%x", pContext->nIspDev, eRet);
        }
        eRet = AW_MPI_VI_EnableVipp(pContext->stConfig.nVippDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vipp[%d] enable failed:0x%x", pContext->stConfig.nVippDev, eRet);
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleUvcViCodec_MPPCallbackWrapper;
        AW_MPI_VI_RegisterCallback(pContext->stConfig.nVippDev, &cbInfo);
        eRet = AW_MPI_VI_CreateVirChn(pContext->stConfig.nVippDev, pContext->nViChn, NULL);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! createVirChn[%d-%d] fail:0x%x!", pContext->stConfig.nVippDev, pContext->nViChn, eRet);
            pContext->nViChn = MM_INVALID_CHN;
        }

        pContext->nSubViChn = SampleUvcViCodec_SUBVICHN;
        eRet = AW_MPI_VI_CreateVipp(pContext->stConfig.nSubVippDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! AW_MPI_VI CreateVipp[%d] failed", pContext->stConfig.nSubVippDev);
        }
        memset(&pContext->stSubViAttr, 0, sizeof(VI_ATTR_S));
        pContext->stSubViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pContext->stSubViAttr.memtype = V4L2_MEMORY_MMAP;
        pContext->stSubViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->stConfig.eSubVippPixelFormat);
        pContext->stSubViAttr.format.field = V4L2_FIELD_NONE;
        pContext->stSubViAttr.format.colorspace = pContext->stConfig.eIspColorSpace;
        pContext->stSubViAttr.format.width = pContext->stConfig.nSubVippCaptureWidth;
        pContext->stSubViAttr.format.height = pContext->stConfig.nSubVippCaptureHeight;
        pContext->stSubViAttr.nbufs =  pContext->stConfig.nSubVippBufNum;
        pContext->stSubViAttr.nplanes = 2;
        pContext->stSubViAttr.wdr_mode = 0;
        pContext->stSubViAttr.fps = pContext->stConfig.nIspCaptureFrameRate;
        pContext->stSubViAttr.drop_frame_num = 0;
        alogd("vipp[%d] use %d v4l2 buffers, colorspace: 0x%x, wdr_mode:%d", pContext->stConfig.nSubVippDev,
            pContext->stSubViAttr.nbufs, pContext->stSubViAttr.format.colorspace, pContext->stSubViAttr.wdr_mode);
        eRet = AW_MPI_VI_SetVippAttr(pContext->stConfig.nSubVippDev, &pContext->stSubViAttr);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vipp[%d] SetVippAttr failed:0x%x", pContext->stConfig.nSubVippDev, eRet);
        }
        eRet = AW_MPI_ISP_Run(pContext->nIspDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! isp[%d] run fail:0x%x", pContext->nIspDev, eRet);
        }
        eRet = AW_MPI_VI_EnableVipp(pContext->stConfig.nSubVippDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! subVipp[%d] enable failed:0x%x", pContext->stConfig.nSubVippDev, eRet);
        }
        AW_MPI_VI_RegisterCallback(pContext->stConfig.nSubVippDev, &cbInfo);
        eRet = AW_MPI_VI_CreateVirChn(pContext->stConfig.nSubVippDev, pContext->nSubViChn, NULL);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! createVirChn[%d-%d] fail:0x%x!", pContext->stConfig.nSubVippDev, pContext->nSubViChn, eRet);
            pContext->nSubViChn = MM_INVALID_CHN;
        }
    }

    if(pContext->bEnableUvcDisplay || pContext->bEnableSubVippDisplay)
    {
        pContext->nVoDev = SampleUvcViCodec_VODEV;
        pContext->nVoLayer = SampleUvcViCodec_VOLAYER;
        pContext->nVOChn = SampleUvcViCodec_VOCHN;
        eRet = AW_MPI_VO_Enable(pContext->nVoDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! VODev[%d] enable fail:0x%x!", pContext->nVoDev, eRet);
        }
        memset(&pContext->stVoPubAttr, 0, sizeof(VO_PUB_ATTR_S));
        AW_MPI_VO_GetPubAttr(pContext->nVoDev, &pContext->stVoPubAttr);
        pContext->stVoPubAttr.enIntfType = VO_INTF_LCD;
        pContext->stVoPubAttr.enIntfSync = VO_OUTPUT_NTSC;
        eRet = AW_MPI_VO_SetPubAttr(pContext->nVoDev, &pContext->stVoPubAttr);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! VODev[%d] set pubAttr fail:0x%x!", pContext->nVoDev, eRet);
        }

        eRet = AW_MPI_VO_EnableVideoLayer(pContext->nVoLayer);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! enable video layer[%d] failed:0x%x", pContext->nVoLayer, eRet);
            pContext->nVoLayer = MM_INVALID_LAYER;
        }
        if (PreviewSource_UVC == pContext->stConfig.ePreviewSource) //uvc
        {
            AW_MPI_VO_GetVideoLayerAttr(pContext->nVoLayer, &pContext->stUvcLayerAttr);
            pContext->stUvcLayerAttr.stDispRect.X = pContext->stConfig.nUvcDisplayX;
            pContext->stUvcLayerAttr.stDispRect.Y = pContext->stConfig.nUvcDisplayY;
            pContext->stUvcLayerAttr.stDispRect.Width = pContext->stConfig.nUvcDisplayWidth;
            pContext->stUvcLayerAttr.stDispRect.Height = pContext->stConfig.nUvcDisplayHeight;
            AW_MPI_VO_SetVideoLayerAttr(pContext->nVoLayer, &pContext->stUvcLayerAttr);
        }
        else if (PreviewSource_VIPP == pContext->stConfig.ePreviewSource) //vipp
        {
            AW_MPI_VO_GetVideoLayerAttr(pContext->nVoLayer, &pContext->stVippLayerAttr);
            pContext->stVippLayerAttr.stDispRect.X = pContext->stConfig.nSubVippDisplayX;
            pContext->stVippLayerAttr.stDispRect.Y = pContext->stConfig.nSubVippDisplayY;
            pContext->stVippLayerAttr.stDispRect.Width = pContext->stConfig.nSubVippDisplayWidth;
            pContext->stVippLayerAttr.stDispRect.Height = pContext->stConfig.nSubVippDisplayHeight;
            AW_MPI_VO_SetVideoLayerAttr(pContext->nVoLayer, &pContext->stVippLayerAttr);
        }

        eRet = AW_MPI_VO_CreateChn(pContext->nVoLayer, pContext->nVOChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! create vo channel[%d-%d] failed", pContext->nVoLayer, pContext->nVOChn);
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void *)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleUvcViCodec_MPPCallbackWrapper;
        AW_MPI_VO_RegisterCallback(pContext->nVoLayer, pContext->nVOChn, &cbInfo);
        AW_MPI_VO_SetChnDispBufNum(pContext->nVoLayer, pContext->nVOChn, 2);

        pContext->nG2dDevFd = open("/dev/g2d", O_RDWR, 0);
        if (pContext->nG2dDevFd < 0)
        {
            aloge("fatal error! open g2d fail, %d", pContext->nG2dDevFd);
        }
    }
    if (pContext->bEnableVideoEncode)
    {
        if (pContext->bEnableUvc)
        {
            configUvcVencChnAttr(pContext);
        }
        if (pContext->bEnableIspVipp)
        {
            configVippVencChnAttr(pContext);
        }
        VENC_CHN_ATTR_S *pVencChnAttr = NULL;
        VENC_RC_PARAM_S *pVencRcParam = NULL;
        if (PreviewSource_UVC == pContext->stConfig.ePreviewSource) //uvc
        {
            pVencChnAttr = &pContext->stUvcVencChnAttr;
            pVencRcParam = &pContext->stUvcVencRcParam;
        }
        else if (PreviewSource_VIPP == pContext->stConfig.ePreviewSource) //vipp
        {
            pVencChnAttr = &pContext->stVippVencChnAttr;
            pVencRcParam = &pContext->stVippVencRcParam;
        }
        pContext->nVencChn = SampleUvcViCodec_VENCCHN;
        eRet = AW_MPI_VENC_CreateChn(pContext->nVencChn, pVencChnAttr);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] create fail:0x%x", pContext->nVencChn, eRet);
            pContext->nVencChn = MM_INVALID_CHN;
        }
        eRet = AW_MPI_VENC_SetRcParam(pContext->nVencChn, pVencRcParam);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] set rc param fail:0x%x", pContext->nVencChn, eRet);
        }
        VencVbrOptParam stVencVbrOptParam;
        eRet = AW_MPI_VENC_GetVbrOptParam(pContext->nVencChn, &stVencVbrOptParam);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] get vbr opt param fail:0x%x", pContext->nVencChn, eRet);
        }
        stVencVbrOptParam.sRcPriority = pContext->stConfig.eVbrOptRcPriority;
        stVencVbrOptParam.eQualityLevel = pContext->stConfig.eVbrOptRcQualityLevel;
        eRet = AW_MPI_VENC_SetVbrOptParam(pContext->nVencChn, &stVencVbrOptParam);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] set vbr opt param fail:0x%x", pContext->nVencChn, eRet);
        }
        if (PreviewSource_UVC == pContext->stConfig.ePreviewSource) //uvc encode don't need ispVe link.
        {
            VENC_IspVeLinkAttr stIspVeLink;
            memset(&stIspVeLink, 0, sizeof(stIspVeLink));
            stIspVeLink.bEnableIsp2Ve = FALSE;
            stIspVeLink.bEnableVe2Isp = FALSE;
            AW_MPI_VENC_EnableIspVeLink(pContext->nVencChn, &stIspVeLink);
        }
        else //vipp
        {
            eRet = AW_MPI_VENC_SetCrop(pContext->nVencChn, &pContext->stVippVencCropCfg);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! vencChn[%d] set crop fail:0x%x", pContext->nVencChn, eRet);
            }
            VENC_IspVeLinkAttr stIspVeLink;
            memset(&stIspVeLink, 0, sizeof(stIspVeLink));
            stIspVeLink.bEnableIsp2Ve = (BOOL)pContext->stConfig.bVippIsp2VeLinkEn;
            stIspVeLink.bEnableVe2Isp = (BOOL)pContext->stConfig.bVippVe2IspLinkEn;
            stIspVeLink.nVipp = pContext->stConfig.nVippDev;
            eRet = AW_MPI_VENC_EnableIspVeLink(pContext->nVencChn, &stIspVeLink);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! vencChn[%d] enable ispVeLink fail:0x%x", pContext->nVencChn, eRet);
            }
            sRegionLinkParam stRegionDetectLink;
            eRet = AW_MPI_VENC_GetRegionDetectLink(pContext->nVencChn, &stRegionDetectLink);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! vencChn[%d] get region detect link fail:0x%x", pContext->nVencChn, eRet);
            }
            stRegionDetectLink.mStaticParam.region_link_en = (unsigned char)pContext->stConfig.bVippRegionLinkEnable;
            stRegionDetectLink.mStaticParam.tex_detect_en = (unsigned char)pContext->stConfig.bVippRegionLinkTexDetectEnable;
            stRegionDetectLink.mStaticParam.motion_detect_en = (unsigned char)pContext->stConfig.bVippRegionLinkMotionDetectEnable;
            //stRegionDetectLink.mStaticParam.updateInterVal = (unsigned char)pContext->stConfig.nVippRegionLinkMotionDetectInterval;
            eRet = AW_MPI_VENC_SetRegionDetectLink(pContext->nVencChn, &stRegionDetectLink);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! vencChn[%d] set region detect link fail:0x%x", pContext->nVencChn, eRet);
            }
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void *)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleUvcViCodec_MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->nVencChn, &cbInfo);

        pContext->bCreateEncLibFlag = true;
    }
    if (pContext->bEnableAudio)
    {
        pContext->nAIODev = SampleUvcViCodec_AIODev;
        pContext->nAiChn = SampleUvcViCodec_AICHN;
        config_AIO_ATTR_S_for_AI(&pContext->stAiAttr, &pContext->stConfig);
        eRet = AW_MPI_AI_SetPubAttr(pContext->nAIODev, &pContext->stAiAttr);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! AiDev[%d] set pubAttr fail:0x%x", pContext->nAIODev, eRet);
        }
        eRet = AW_MPI_AI_Enable(pContext->nAIODev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! AiDev[%d] enable fail:0x%x", pContext->nAIODev, eRet);
        }
        AW_MPI_AI_SetDevVolume(pContext->nAIODev, pContext->stConfig.nAiVolume);
        eRet = AW_MPI_AI_CreateChn(pContext->nAIODev, pContext->nAiChn, NULL);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! create ai channel[%d-%d] fail:0x%x!", pContext->nAIODev, pContext->nAiChn, eRet);
            pContext->nAiChn = MM_INVALID_CHN;
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void *)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleUvcViCodec_MPPCallbackWrapper;
        AW_MPI_AI_RegisterCallback(pContext->nAIODev, pContext->nAiChn, &cbInfo);
    }
    if (pContext->bEnableAudioEncode)
    {
        pContext->nAEncChn = SampleUvcViCodec_AENCCHN;
        configAEncAttr(pContext);
        eRet = AW_MPI_AENC_CreateChn(pContext->nAEncChn, &pContext->stAEncAttr);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! create aencChn[%d] fail:0x%x", pContext->nAEncChn, eRet);
            pContext->nAEncChn = MM_INVALID_CHN;
        }
        MPP_CHN_S stMppAiChn = {MOD_ID_AI, pContext->nAIODev, pContext->nAiChn};
        MPP_CHN_S stMppAencChn = {MOD_ID_AENC, 0, pContext->nAEncChn};
        eRet = AW_MPI_SYS_Bind(&stMppAiChn, &stMppAencChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! aiChn bind aencChn fail:0x%x", eRet);
        }
        //start audio encode.
        eRet = AW_MPI_AI_EnableChn(pContext->nAIODev, pContext->nAiChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! aiChn[%d-%d] enable fail:0x%x", pContext->nAIODev, pContext->nAiChn, eRet);
        }
        eRet = AW_MPI_AENC_StartRecvPcm(pContext->nAEncChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! aencChn[%d] start fail:0x%x", pContext->nAEncChn, eRet);
        }
    }
    
    //2. prepare threads.
    if (pContext->bEnableUvc)
    {
        pContext->eGetUvcVdecFrameThreadState = SampleState_Idle;
        ret = pthread_create(&pContext->GetUvcVdecFrameThreadId, NULL, GetUvcVdecFrameThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create GetUvcVdecFrameThread fail:%d", ret);
        }
    }
    if (pContext->bEnableIspVipp)
    {
        pContext->eGetVippFrameThreadState = SampleState_Idle;
        ret = pthread_create(&pContext->GetVippFrameThreadId, NULL, GetVippFrameThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create GetVippFrameThread fail:%d", ret);
        }
    }
    if (pContext->stConfig.nPreviewSwitchInterval > 0)
    {
        pContext->ePreviewSwitchThreadState = SampleState_Idle;
        ret = pthread_create(&pContext->PreviewSwitchThreadId, NULL, PreviewSwitchThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create PreviewSwitchThread fail:%d", ret);
        }
    }
    if (pContext->bEnableVideoEncode || pContext->bEnableAudioEncode)
    {
        pContext->eGetStreamThreadState = SampleState_Idle;
        ret = pthread_create(&pContext->GetStreamThreadId, NULL, GetStreamThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create GetStreamThread fail:%d", ret);
        }
    }

    //3. start
    if (PreviewSource_UVC == pContext->stConfig.ePreviewSource) //uvc
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_SetState;
        stMsg.para0 = SampleState_Executing;
        stMsg.pReply = ConstructMessageReply();
        putMessageWithData(&pContext->stGetUvcVdecFrameMessageQueue, &stMsg);
        while(1)
        {
            ret = cdx_sem_down_timedwait(&stMsg.pReply->ReplySem, 5000);
            if(ret != 0)
            {
                aloge("fatal error! wait state to executing fail[0x%x]", ret);
            }
            else
            {
                break;
            }
        }
        alogd("receive state to executing reply: %d!", stMsg.pReply->ReplyResult);
        DestructMessageReply(stMsg.pReply);
        stMsg.pReply = NULL;
    }
    else //vipp
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_SetState;
        stMsg.para0 = SampleState_Executing;
        stMsg.pReply = ConstructMessageReply();
        putMessageWithData(&pContext->stGetVippFrameMessageQueue, &stMsg);
        while(1)
        {
            ret = cdx_sem_down_timedwait(&stMsg.pReply->ReplySem, 5000);
            if(ret != 0)
            {
                aloge("fatal error! wait state to executing fail[0x%x]", ret);
            }
            else
            {
                break;
            }
        }
        alogd("receive state to executing reply: %d!", stMsg.pReply->ReplyResult);
        DestructMessageReply(stMsg.pReply);
        stMsg.pReply = NULL;
    }
    if (pContext->bEnableVideoEncode || pContext->bEnableAudioEncode)
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_SetState;
        stMsg.para0 = SampleState_Executing;
        put_message(&pContext->stGetStreamMessageQueue, &stMsg);
    }
    if (pContext->stConfig.nPreviewSwitchInterval > 0)
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_SetState;
        stMsg.para0 = SampleState_Executing;
        put_message(&pContext->stPreviewSwitchMessageQueue, &stMsg);
    }

    //4. wait exit
    if (pContext->stConfig.nTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->stSemExit, pContext->stConfig.nTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->stSemExit);
    }

    //5. destroy threads and mpp channels.
    if (pContext->stConfig.nPreviewSwitchInterval > 0)
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_SetState;
        stMsg.para0 = SampleState_Idle;
        putMessageWithData(&pContext->stPreviewSwitchMessageQueue, &stMsg);

        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_Stop;
        putMessageWithData(&pContext->stPreviewSwitchMessageQueue, &stMsg);
        int eError = 0;
        ret = pthread_join(pContext->PreviewSwitchThreadId, (void **)&eError);
        if (ret != 0)
        {
            aloge("fatal error! pthread join fail:%d", ret);
        }
    }
    if (pContext->bEnableVideoEncode || pContext->bEnableAudioEncode)
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_SetState;
        stMsg.para0 = SampleState_Idle;
        putMessageWithData(&pContext->stGetStreamMessageQueue, &stMsg);

        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_Stop;
        putMessageWithData(&pContext->stGetStreamMessageQueue, &stMsg);
        int eError = 0;
        ret = pthread_join(pContext->GetStreamThreadId, (void **)&eError);
        if (ret != 0)
        {
            aloge("fatal error! pthread join fail:%d", ret);
        }
    }
    if (pContext->bEnableUvc)
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_SetState;
        stMsg.para0 = SampleState_Idle;
        putMessageWithData(&pContext->stGetUvcVdecFrameMessageQueue, &stMsg);

        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_Stop;
        putMessageWithData(&pContext->stGetUvcVdecFrameMessageQueue, &stMsg);
        int eError = 0;
        ret = pthread_join(pContext->GetUvcVdecFrameThreadId, (void **)&eError);
        if (ret != 0)
        {
            aloge("fatal error! pthread join fail:%d", ret);
        }
    }
    if (pContext->bEnableIspVipp)
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_SetState;
        stMsg.para0 = SampleState_Idle;
        putMessageWithData(&pContext->stGetVippFrameMessageQueue, &stMsg);

        InitMessage(&stMsg);
        stMsg.command = SampleMsgType_Stop;
        putMessageWithData(&pContext->stGetVippFrameMessageQueue, &stMsg);
        int eError = 0;
        ret = pthread_join(pContext->GetVippFrameThreadId, (void **)&eError);
        if (ret != 0)
        {
            aloge("fatal error! pthread join fail:%d", ret);
        }
    }
    if (pContext->nAiChn >= 0)
    {
        eRet = AW_MPI_AI_DisableChn(pContext->nAIODev, pContext->nAiChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! aiChn[%d-%d] disable fail:0x%x", pContext->nAIODev, pContext->nAiChn, eRet);
        }
    }
    if (pContext->nAEncChn >= 0)
    {
        eRet = AW_MPI_AENC_StopRecvPcm(pContext->nAEncChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! aencChn[%d] stop fail:0x%x", pContext->nAEncChn, eRet);
        }
        eRet = AW_MPI_AENC_DestroyChn(pContext->nAEncChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! destroy aencChn[%d] fail:0x%x", pContext->nAEncChn, eRet);
        }
        pContext->nAEncChn = MM_INVALID_CHN;
    }
    if (pContext->nAiChn >= 0)
    {
        eRet = AW_MPI_AI_DestroyChn(pContext->nAIODev, pContext->nAiChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! destroy aiChannel[%d-%d] fail:0x%x!", pContext->nAIODev, pContext->nAiChn, eRet);
        }
        pContext->nAiChn = MM_INVALID_CHN;
    }
    if (pContext->nAIODev >= 0)
    {
        eRet = AW_MPI_AI_ClrPubAttr(pContext->nAIODev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! AiDev[%d] clear pubAttr fail:0x%x", pContext->nAIODev, eRet);
        }
        pContext->nAIODev = MM_INVALID_DEV;
    }
    if (pContext->nVencChn >= 0)
    {
        eRet = AW_MPI_VENC_DestroyChn(pContext->nVencChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] destroy fail:0x%x", pContext->nVencChn, eRet);
        }
        pContext->nVencChn = MM_INVALID_CHN;
    }
    if (pContext->nVOChn >= 0)
    {
        eRet = AW_MPI_VO_DestroyChn(pContext->nVoLayer, pContext->nVOChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! destroy vo channel[%d-%d] failed:0x%x", pContext->nVoLayer, pContext->nVOChn, eRet);
        }
        pContext->nVOChn = MM_INVALID_CHN;
    }
    if (pContext->nVoLayer >= 0)
    {
        eRet = AW_MPI_VO_DisableVideoLayer(pContext->nVoLayer);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! disable video layer[%d] failed:0x%x", pContext->nVoLayer, eRet);
        }
        pContext->nVoLayer = MM_INVALID_LAYER;
    }
    if (pContext->nVoDev >= 0)
    {
        eRet = AW_MPI_VO_Disable(pContext->nVoDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! VODev[%d] enable fail:0x%x!", pContext->nVoDev, eRet);
        }
        pContext->nVoDev = MM_INVALID_DEV;
    }
    if (pContext->bEnableIspVipp)
    {
        if (pContext->nSubViChn >= 0)
        {
            eRet = AW_MPI_VI_DestroyVirChn(pContext->stConfig.nSubVippDev, pContext->nSubViChn);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! destroy VirChn[%d-%d] fail:0x%x!", pContext->stConfig.nSubVippDev, pContext->nSubViChn, eRet);
            }
            pContext->nSubViChn = MM_INVALID_CHN;
        }
        eRet = AW_MPI_VI_DisableVipp(pContext->stConfig.nSubVippDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! subVipp[%d] disable failed:0x%x", pContext->stConfig.nSubVippDev, eRet);
        }
        eRet = AW_MPI_VI_DestroyVipp(pContext->stConfig.nSubVippDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! destroy vipp[%d] failed:0x%x", pContext->stConfig.nSubVippDev, eRet);
        }
        eRet = AW_MPI_ISP_Stop(pContext->nIspDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! isp[%d] stop fail:0x%x", pContext->nIspDev, eRet);
        }

        if (pContext->nViChn >= 0)
        {
            eRet = AW_MPI_VI_DestroyVirChn(pContext->stConfig.nVippDev, pContext->nViChn);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! destroy VirChn[%d-%d] fail:0x%x!", pContext->stConfig.nVippDev, pContext->nViChn, eRet);
            }
            pContext->nViChn = MM_INVALID_CHN;
        }
        eRet = AW_MPI_VI_DisableVipp(pContext->stConfig.nVippDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vipp[%d] disable failed:0x%x", pContext->stConfig.nVippDev, eRet);
        }
        eRet = AW_MPI_VI_DestroyVipp(pContext->stConfig.nVippDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! destroy vipp[%d] failed:0x%x", pContext->stConfig.nVippDev, eRet);
        }
        eRet = AW_MPI_ISP_Stop(pContext->nIspDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! isp[%d] stop fail:0x%x", pContext->nIspDev, eRet);
        }
        pContext->nIspDev = MM_INVALID_DEV;
    }
    if (pContext->bEnableUvc)
    {
        if (pContext->nVdecChn >= 0)
        {
            eRet = AW_MPI_VDEC_DestroyChn(pContext->nVdecChn);
            if(eRet != SUCCESS)
            {
                aloge("fatal error! destroy vdecChn[%d] fail:0x%x", pContext->nVdecChn, eRet);
            }
            pContext->nVdecChn = MM_INVALID_CHN;
        }
        if (pContext->nUvcChn >= 0)
        {
            eRet = AW_MPI_UVC_DestroyVirChn(pContext->stConfig.strUvcDevName, pContext->nUvcChn);
            if(eRet != SUCCESS)
            {
                aloge("fatal error! UVC[%s] destroy virchannel[%d] fail:0x%x", pContext->stConfig.strUvcDevName, pContext->nUvcChn,
                    eRet);
            }
            pContext->nUvcChn = MM_INVALID_CHN;
        }
        eRet = AW_MPI_UVC_DisableDevice(pContext->stConfig.strUvcDevName);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! UVC[%s] disable device fail:0x%x", pContext->stConfig.strUvcDevName, eRet);
        }
        eRet = AW_MPI_UVC_DestroyDevice(pContext->stConfig.strUvcDevName);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! uvcDev[%s] destroy fail:0x%x!", pContext->stConfig.strUvcDevName, eRet);
        }
    }
    if (pContext->nG2dDevFd >= 0)
    {
        close(pContext->nG2dDevFd);
        pContext->nG2dDevFd = -1;
    }
    AW_MPI_SYS_Exit();

    if (pContext->pRtspStreamBuf)
    {
        free(pContext->pRtspStreamBuf);
        pContext->pRtspStreamBuf = NULL;
        pContext->nRtspStreamBufSize = 0;
    }
    destroySampleUvcViCodecContext(pContext);
    free(pContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;

_exit:
    destroySampleUvcViCodecContext(pContext);
    free(pContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}

