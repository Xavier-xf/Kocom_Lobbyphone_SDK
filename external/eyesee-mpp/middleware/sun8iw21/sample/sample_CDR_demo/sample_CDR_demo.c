/******************************************************************************
  Copyright (C), 2020-2030, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2020/5/15
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_CDR_demo"
#include "plat_log.h"

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>

#include <confparser.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_sys.h>
#include <mpi_vi.h>
#include <mpi_venc.h>
#include <mpi_mux.h>
#include <mpi_isp.h>
#include <vo/hwdisplay.h>
#include <mpi_vo.h>
#include <cdx_list.h>
#include <mpi_ai.h>
#include <mpi_ao.h>
#include <mpi_aenc.h>
#include "mm_comm_aenc.h"
#include <media_common_aio.h>
#include <aenc_sw_lib.h>
#include <media_common_vcodec.h>

#include "sample_CDR_demo.h"
#include "sample_CDR_demo_conf.h"
#include "../common/sample_common_venc.h"
#include "../common/rtsp_server.h"

#define FILE_EXIST(PATH)   (access(PATH, F_OK) == 0)
#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData);
static void *takePictureThread(void *pThreadData);

static SampleCDRDemoContext *gpSampleCDRDemoContext = NULL;
static void handle_exit()
{
    alogd("user want to exit!");
    if (NULL != gpSampleCDRDemoContext)
    {
        cdx_sem_up(&gpSampleCDRDemoContext->mSemExit);
    }
}

static unsigned int getSysTickMs()
{
    unsigned int ms = 0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ms = tv.tv_sec*1000 + tv.tv_usec / 1000;
    return ms;
}

static int ParseCmdLine(SampleCDRDemoContext *pContext, int argc, char **argv)
{
    alogd("sample_CDRRecorder:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i = 1;

    memset(&pContext->mCmdLinePara, 0, sizeof(pContext->mCmdLinePara));
    while (i < argc)
    {
        if (!strcmp(argv[i], "-path"))
        {
            if ( ++i >= argc)
            {
                aloge("fatal error! use -h to learn how to set parameter!!!");
                ret = -1;
                break;
            }
            if (strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error! file path[%s] too long: [%d]>=[%d]!", argv[i], strlen(argv[i]), MAX_FILE_PATH_SIZE);
            }
            else
            {
                strcpy(pContext->mCmdLinePara.mConfigFilePath, argv[i]);
            }
        }
        else if (!strcmp(argv[i], "-h"))
        {
            alogd("CmdLine param:\n"
                "\t-path /mnt/extsd/sample_multi_vi2venc2muxer.conf");
            ret = 1;
            break;
        }
        else
        {
            alogd("ignore invalid CmdLine param:[%s], type -h to get how to set parameter!", argv[i]);
        }
        i++;
    }

    return ret;
}

static void judgeCaptureFormat(char *pFormatConf, PIXEL_FORMAT_E *pCapFromat)
{
    if (!pFormatConf)
        aloge("fatal error! params is null!");
    if (!strcmp(pFormatConf, "nv21"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(pFormatConf, "yv12"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if (!strcmp(pFormatConf, "nv12"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(pFormatConf, "yu12"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcmp(pFormatConf, "aw_afbc"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_2_0x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_2_5x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_1_5x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_1_0x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
    }
    else
    {
        *pCapFromat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        aloge("fatal error! wrong src pixfmt:%s use default nv21", pFormatConf);
    }
}

static void judgeVencoderType(char *pFormatConf, PAYLOAD_TYPE_E *pVencoderType)
{
    if (!strcmp(pFormatConf, "H.264"))
    {
        *pVencoderType = PT_H264;
    }
    else if (!strcmp(pFormatConf, "H.265"))
    {
        *pVencoderType = PT_H265;
    }
    else if (!strcmp(pFormatConf, "MJPEG"))
    {
        *pVencoderType = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! unsupport Vencoder type[%s] use default H.264", pFormatConf);
        *pVencoderType = PT_H264;
    }
}

static void judgeAencoderType(char *pFormatConf, PAYLOAD_TYPE_E *pAencoderType)
{
    if (!strcmp(pFormatConf, "aac"))
    {
        *pAencoderType = PT_AAC;
    }
    else if (!strcmp(pFormatConf, "mp3"))
    {
        *pAencoderType = PT_MP3;
    }
    else if (!strcmp(pFormatConf, "pcm"))
    {
        *pAencoderType = PT_PCM_AUDIO;
    }
    else if (!strcmp(pFormatConf, "g711a"))
    {
        *pAencoderType = PT_G711A;
    }
    else if (!strcmp(pFormatConf, "g711u"))
    {
        *pAencoderType = PT_G711U;
    }
    else
    {
        alogw("fatal error! unsupport audio codec type[%s]! Set to default [aac]", pFormatConf);
        *pAencoderType = PT_AAC;
    }
}

static int loadSampleCDRConfig(SampleCDRDemoContext *pContext, const char *conf_path)
{
    SampleCDRDemoConfig *pConfigPara = &pContext->mConfigPara;
    int ret = 0;
    char *pTmp = NULL;
    CONFPARSER_S stConf;

    if (conf_path != NULL)
    {
        ret = createConfParser(conf_path, &stConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return ret;
        }

        // common param
        pConfigPara->mEncRecRefBufReduceEnable = GetConfParaInt(&stConf, CFG_EncRecRefBufReduceEnable, 0);
        pConfigPara->mVeRxInputBufmultiplexEnable = GetConfParaInt(&stConf, CFG_VeRxInputBufmultiplexEnable, 0);
        pConfigPara->mProductMode = GetConfParaInt(&stConf, CFG_PRODUCTMODE, 0);
        pConfigPara->mEncRcMode = GetConfParaInt(&stConf, CFG_ENC_RCMODE, 0);
        pConfigPara->mTestDuration = GetConfParaInt(&stConf, CFG_TEST_DURATION, 0);
        pConfigPara->mRegionLinkEnable = GetConfParaInt(&stConf, CFG_RegionLinkEnable, 0);
        pConfigPara->mRegionLinkTexDetectEnable = GetConfParaInt(&stConf, CFG_RegionLinkTexDetectEnable, 0);
        pConfigPara->mRegionLinkMotionDetectEnable = GetConfParaInt(&stConf, CFG_RegionLinkMotionDetectEnable, 0);
        pConfigPara->mRegionLinkMotionDetectInv = GetConfParaInt(&stConf, CFG_RegionLinkMotionDetectInv, 0);

        // config audio param
        AudioConfig *pAudioConfig = &pContext->mConfigPara.mAudioConfig;
        pAudioConfig->mCaptureSampleRate = GetConfParaInt(&stConf, CFG_CAPTURE_SAMPLE_RATE, 0);
        pAudioConfig->mCaptureBitWitdh = GetConfParaInt(&stConf, CFG_CAPTURE_BIT_WIDTH, 0);
        pAudioConfig->mCaptureChannelCnt = GetConfParaInt(&stConf, CFG_CAPTURE_CHANNEL_CNT, 0);
        pAudioConfig->mCaptureAnsEn = GetConfParaInt(&stConf, CFG_CAPTURE_ANS_EN, 0);
        pAudioConfig->mCaptureAgcEn = GetConfParaInt(&stConf, CFG_CAPTURE_AGC_EN, 0);
        pAudioConfig->mCaptureAecEn = GetConfParaInt(&stConf, CFG_CAPTURE_AEC_EN, 0);

        pAudioConfig->mAencBitRate = GetConfParaInt(&stConf, CFG_AENC_BITRATE, 0);
        pTmp = (char *)GetConfParaString(&stConf, CFG_AENC_TYPE, NULL);
        judgeAencoderType(pTmp, &pAudioConfig->mAencType);

        // config recorder param
        StreamConfig *pRecorderConfig = &pConfigPara->mRecorderConfig;
        pRecorderConfig->mVIDev = GetConfParaInt(&stConf, CFG_RECORDER_VIPP_DEV, 0);
        pRecorderConfig->mViVirChn = GetConfParaInt(&stConf, CFG_RECORDER_VI_VIRCHN, 0);
        pRecorderConfig->mIspDev = GetConfParaInt(&stConf, CFG_RECORDER_ISP_DEV, 0);
        pRecorderConfig->mCapWidth = GetConfParaInt(&stConf, CFG_RECORDER_CAP_WIDTH, 0);
        pRecorderConfig->mCapHeight = GetConfParaInt(&stConf, CFG_RECORDER_CAP_HEIGHT, 0);
        pRecorderConfig->mCapFrmRate = GetConfParaInt(&stConf, CFG_RECORDER_CAP_FRAMERATE, 0);
        pTmp = (char *) GetConfParaString(&stConf, CFG_RECORDER_CAP_FORMAT, NULL);
        judgeCaptureFormat(pTmp, &pRecorderConfig->mCapFormat);
        pRecorderConfig->mVIBufNum = GetConfParaInt(&stConf, CFG_RECORDER_VI_BUFNUM, 0);
        pRecorderConfig->mEnableWDR = GetConfParaInt(&stConf, CFG_RECORDER_ENABLEWDR, 0);
        pRecorderConfig->mViStitchMode = GetConfParaInt(&stConf, CFG_RECORDER_VI_STITCH_MODE, 0);
        pRecorderConfig->mEncChn = GetConfParaInt(&stConf, CFG_RECORDER_ENC_CHN, 0);
        pRecorderConfig->mEncOnlineEnable = GetConfParaInt(&stConf, CFG_RECORDER_ENC_ONLINE, 0);
        pRecorderConfig->mEncOnlineShareBufNum = GetConfParaInt(&stConf, CFG_RECORDER_ENC_ONLINE_SHARE_BFUNUM, 0);
        pRecorderConfig->bEncIsp2VeEnable = (bool)GetConfParaInt(&stConf, CFG_RECORDER_ENC_ISP2VE, 1);
        pRecorderConfig->bEncVe2IspEnable = (bool)GetConfParaInt(&stConf, CFG_RECORDER_ENC_VE2ISP, 1);
        pTmp = (char *)GetConfParaString(&stConf, CFG_RECORDER_ENC_TYPE, NULL);
        judgeVencoderType(pTmp, &pRecorderConfig->mEncType);
        pRecorderConfig->mEncWidth = GetConfParaInt(&stConf, CFG_RECORDER_ENC_WIDTH, 0);
        pRecorderConfig->mEncHeight = GetConfParaInt(&stConf, CFG_RECORDER_ENC_HEIGHT, 0);
        pRecorderConfig->mEncFrmRate = GetConfParaInt(&stConf, CFG_RECORDER_ENC_FRAMERATE, 0);
        pRecorderConfig->mEncBitRate = GetConfParaInt(&stConf, CFG_RECORDER_ENC_BITRATE, 0);
        pRecorderConfig->mEncRefFrameLbcMode = GetConfParaInt(&stConf, CFG_RECORDER_VEREFFRAMELBCMODE, 0);
        pConfigPara->mRecorderRecDuration = GetConfParaInt(&stConf, CFG_RECORDER_REC_DURATION, 0);
        pConfigPara->mRecorderRecFileCnt = GetConfParaInt(&stConf, CFG_RECORDER_REC_FILECNT, 0);
        pTmp = (char *)GetConfParaString(&stConf, CFG_RECORDER_REC_FILE_FORMART, NULL);
        if (!pTmp)
        {
            alogw("recorder file format is NULL! default output mp4 format");
            pTmp = "mp4";
        }
        strncpy(pConfigPara->mRecorderRecFileFormat, pTmp, MAX_FILE_FORMAT_SIZE - 1);

        pTmp = (char *)GetConfParaString(&stConf, CFG_RECORDER_REC_FILE, NULL);
        if (pTmp)
        {
            strncpy(pConfigPara->mRecorderOutFilePath, pTmp, MAX_FILE_PATH_SIZE - 1);
        }
        else
        {
            aloge("fatal error! recorder output file path is NULL");
        }

        // config take picture param
        pConfigPara->mTakePicture = GetConfParaInt(&stConf, CFG_TAKEPIC_ENABLE, 0);
        pConfigPara->mTakePictureOnline = GetConfParaInt(&stConf, CFG_TAKEPIC_ONLINE, 0);
        pConfigPara->mTakePictureInterval = GetConfParaInt(&stConf, CFG_TAKEPIC_INTERVAL, 5);
        pConfigPara->mTakePictureThumbEnable = GetConfParaInt(&stConf, CFG_TAKEPIC_THUMB_ENABLE, 0);
        pConfigPara->mTakePicTureVIDev = GetConfParaInt(&stConf, CFG_TAKEPIC_VIPP_DEV, 0);
        pConfigPara->mTakePictureViChn = GetConfParaInt(&stConf, CFG_TAKEPIC_VI_VIRCHN, 0);
        pConfigPara->mTakePictureEncChn = GetConfParaInt(&stConf, CFG_TAKEPIC_ENC_CHN, 0);
        pConfigPara->nTakePictureFileCnt = GetConfParaInt(&stConf, CFG_TAKEPIC_FILE_CNT, 0);
        pTmp = (char *)GetConfParaString(&stConf, CFG_TAKEPIC_FILE, NULL);
        if (pTmp)
        {
            strncpy(pConfigPara->mTakePictureFile, pTmp, MAX_FILE_PATH_SIZE - 1);
        }
        else
        {
            aloge("fatal error! take pic file path is NULL");
        }

        // config preview param
        StreamConfig *pPreviewConfig = &pConfigPara->mPreviewConfig;
        pConfigPara->mPreviewEnable = GetConfParaInt(&stConf, CFG_PREVIEW_ENABLE, 0);
        pPreviewConfig->mVIDev = GetConfParaInt(&stConf, CFG_PREVIEW_VIPP_DEV, 4);
        pPreviewConfig->mViVirChn = GetConfParaInt(&stConf, CFG_PREVIEW_VI_VIRCHN, 0);
        pPreviewConfig->mIspDev = GetConfParaInt(&stConf, CFG_PREVIEW_ISP_DEV, 0);
        pPreviewConfig->mCapWidth = GetConfParaInt(&stConf, CFG_PREVIEW_CAP_WIDTH, 0);
        pPreviewConfig->mCapHeight = GetConfParaInt(&stConf, CFG_PREVIEW_CAP_HEIGHT, 0);
        pPreviewConfig->mCapFrmRate = GetConfParaInt(&stConf, CFG_PREVIEW_CAP_FRAMERATE, 0);
        pTmp = (char *)GetConfParaString(&stConf, CFG_PREVIEW_CAP_FORMAT, NULL);
        judgeCaptureFormat(pTmp, &pPreviewConfig->mCapFormat);
        pPreviewConfig->mVIBufNum = GetConfParaInt(&stConf, CFG_PREVIEW_VI_BUFNUM, 0);
        pPreviewConfig->mEnableWDR = GetConfParaInt(&stConf, CFG_PREVIEW_ENABLEWDR, 0);
        pPreviewConfig->mViStitchMode = GetConfParaInt(&stConf, CFG_PREVIEW_VI_STITCH_MODE, 0);
        pPreviewConfig->mEncChn = GetConfParaInt(&stConf, CFG_PREVIEW_ENC_CHN, 0);
        pPreviewConfig->mEncOnlineEnable = GetConfParaInt(&stConf, CFG_PREVIEW_ENC_ONLINE, 0);
        pPreviewConfig->mEncOnlineShareBufNum = GetConfParaInt(&stConf, CFG_PREVIEW_ENC_ONLINE_SHARE_BFUNUM, 0);
        pPreviewConfig->bEncIsp2VeEnable = (bool)GetConfParaInt(&stConf, CFG_PREVIEW_ENC_ISP2VE, 1);
        pPreviewConfig->bEncVe2IspEnable = (bool)GetConfParaInt(&stConf, CFG_PREVIEW_ENC_VE2ISP, 0);
        pTmp = (char *)GetConfParaString(&stConf, CFG_PREVIEW_ENC_TYPE, NULL);
        judgeVencoderType(pTmp, &pPreviewConfig->mEncType);
        pPreviewConfig->mEncWidth = GetConfParaInt(&stConf, CFG_PREVIEW_ENC_WIDTH, 0);
        pPreviewConfig->mEncHeight = GetConfParaInt(&stConf, CFG_PREVIEW_ENC_HEIGHT, 0);
        pPreviewConfig->mEncFrmRate = GetConfParaInt(&stConf, CFG_PREVIEW_ENC_FRAMERATE, 0);
        pPreviewConfig->mEncBitRate = GetConfParaInt(&stConf, CFG_PREVIEW_ENC_BITRATE, 0);
        pPreviewConfig->mEncRefFrameLbcMode = GetConfParaInt(&stConf, CFG_PREVIEW_VEREFFRAMELBCMODE, 0);
        pConfigPara->mPreviewMode = GetConfParaInt(&stConf, CFG_PREVIEW_MODE, 0);
        pConfigPara->mPreviewRtspId = GetConfParaInt(&stConf, CFG_PREVIEW_RTSP_ID, 0);
        pConfigPara->mPreviewRtspNetType = GetConfParaInt(&stConf, CFG_PREVIEW_RTSP_Net_TYPE, 0);
        pConfigPara->mPreviewDispX = GetConfParaInt(&stConf, CFG_PREVIEW_DISP_X, 0);
        pConfigPara->mPreviewDispY = GetConfParaInt(&stConf, CFG_PREVIEW_DISP_Y, 0);
        pConfigPara->mPreviewDispWidth = GetConfParaInt(&stConf, CFG_PREVIEW_DISP_WIDTH, 0);
        pConfigPara->mPreviewDispHeight = GetConfParaInt(&stConf, CFG_PREVIEW_DISP_HEIGHT, 0);
        pTmp = (char *)GetConfParaString(&stConf, CFG_PREVIEW_DISP_DEV, NULL);
        if (!strcmp(pTmp, "lcd"))
        {
            pConfigPara->mPreviewDispDev = VO_INTF_LCD;
        }
        else
        {
            aloge("fatal error! unsupport display device[%s] use dafault lcd", pTmp);
            pConfigPara->mPreviewDispDev = VO_INTF_LCD;
        }

        pConfigPara->mRecorderConfig.mProductMode = pConfigPara->mProductMode;
        pConfigPara->mRecorderConfig.mEncRcMode = pConfigPara->mEncRcMode;
        pConfigPara->mPreviewConfig.mProductMode = pConfigPara->mProductMode;
        pConfigPara->mPreviewConfig.mEncRcMode = pConfigPara->mEncRcMode;

        // param check and update
        if (0 == pConfigPara->mTakePictureOnline)
        {
            if (pRecorderConfig->mEncOnlineEnable)
            {
                pRecorderConfig->mEncOnlineEnable = 0;
                alogw("user set take picture offline, force set offline encoder when usr set online encoder !");
            }
        }
        else
        {
            if (0 == pRecorderConfig->mEncOnlineEnable)
            {
                alogw("user set take picture online, but recoder not set online vencoder !");
                goto exit;
            }
            if (pConfigPara->mTakePicTureVIDev != pRecorderConfig->mVIDev)
            {
                alogw("user set take picture online, force set TakePicTureVIDev = recorder VIDev !");
                pConfigPara->mTakePicTureVIDev = pRecorderConfig->mVIDev;
            }
            if (pConfigPara->mTakePictureThumbEnable)
            {
                alogw("user set take picture online, force set disable TakePicture Thumb and Exif !");
                pConfigPara->mTakePictureThumbEnable = 0;
            }
        }

        if (pConfigPara->mTakePicture)
        {
            if (0 == pConfigPara->mTakePictureInterval)
            {
                pConfigPara->mTakePictureInterval = 5;
                alogw("usr set take picture interval is 0, force set 5 !");
            }
        }

        if (pRecorderConfig->mViStitchMode > DMA_STITCH_NONE)
        {
            /* mViStitchIspChannelId is ISP0, it will be run when mViStitchMode > DMA_STITCH_NONE */
            if (pRecorderConfig->mIspDev != 1)
            {
                aloge("when Recorder mViStitchMode = %d, it must be set mIsp to 1, please check!!!\n", pRecorderConfig->mViStitchMode);
                return -1;
            }
            pRecorderConfig->mViStitchIspChannelId = 0;
            pPreviewConfig->mViStitchIspChannelId = -1;
        }

        destroyConfParser(&stConf);
    }

    alogd("sample record test duration[%d]", pConfigPara->mTestDuration);
    return SUCCESS;

exit:
    destroyConfParser(&stConf);
    aloge("fatal error! wrong config params!");
    return FAILURE;
}

static SampleCDRDemoContext *constructSampleCDRDemoContext()
{
    SampleCDRDemoContext *pContext = (SampleCDRDemoContext *)malloc(sizeof(SampleCDRDemoContext));
    if (pContext != NULL)
    {
        memset(pContext, 0, sizeof(SampleCDRDemoContext));
        int ret = cdx_sem_init(&pContext->mSemExit, 0);
        if (ret != 0)
        {
            aloge("fatal error! cdx sem init fail[%d]", ret);
            free(pContext);
            pContext = NULL;
        }
    }
    else
    {
        aloge("fatal error! malloc fail!");
    }

    return pContext;
}

static int initRecorder(StreamContext *pRecorderContext)
{
    int ret = 0;

    pthread_mutex_init(&pRecorderContext->mFilePathListLock, NULL);
    INIT_LIST_HEAD(&pRecorderContext->mFilePathList);

    return ret;
}

static int deinitRecorder(StreamContext *pRecorderContext)
{
    FilePathNode *pEntry, *pTmp;
    int ret = 0;

    pthread_mutex_lock(&pRecorderContext->mFilePathListLock);
    list_for_each_entry_safe(pEntry, pTmp, &pRecorderContext->mFilePathList, mList)
    {
        if (pEntry)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            pEntry = NULL;
        }
    }
    pthread_mutex_unlock(&pRecorderContext->mFilePathListLock);
    pthread_mutex_destroy(&pRecorderContext->mFilePathListLock);

    return 0;
}

static int initPicFilePathList(SampleCDRDemoContext *pContext)
{
    INIT_LIST_HEAD(&pContext->PicFilePathList);
    return 0;
}

static int destroyPicFilePathList(SampleCDRDemoContext *pContext)
{
    FilePathNode *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pContext->PicFilePathList, mList)
    {
        if (pEntry)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            pEntry = NULL;
        }
    }
    return 0;
}

static MEDIA_FILE_FORMAT_E getFileFormat(char *pFileFormat)
{
    MEDIA_FILE_FORMAT_E eFileFormat = MEDIA_FILE_FORMAT_MP4;

    if (pFileFormat)
    {
        if (!strcmp(pFileFormat, "mp4"))
        {
            eFileFormat = MEDIA_FILE_FORMAT_MP4;
        }
        else if (!strcmp(pFileFormat, "ts"))
        {
            eFileFormat = MEDIA_FILE_FORMAT_TS;
        }
        else
        {
            alogw("Be careful! unknown file format:%d, default to mp4", eFileFormat);
            eFileFormat = MEDIA_FILE_FORMAT_MP4;
        }
    }
    else
    {
        alogw("Be careful! pFileFormat is not config, default to mp4");
        eFileFormat = MEDIA_FILE_FORMAT_MP4;
    }

    return eFileFormat;
}

static int generateNextFileFd(SampleCDRDemoConfig *pConfigPara)
{
    unsigned int rec_time = getSysTickMs();

    memset(pConfigPara->mRecorderCurFileName, 0, MAX_FILE_PATH_SIZE - 1);

    sprintf(pConfigPara->mRecorderCurFileName, "%s_%dx%d_%ums.%s",
        pConfigPara->mRecorderOutFilePath, pConfigPara->mRecorderConfig.mCapWidth,
        pConfigPara->mRecorderConfig.mCapHeight, rec_time, pConfigPara->mRecorderRecFileFormat);

    alogd("recorder output file path[%s]", pConfigPara->mRecorderCurFileName);

    int nFd = open(pConfigPara->mRecorderCurFileName, O_RDWR | O_CREAT | O_TRUNC, 0666);

    return nFd;
}

static int setNextFileToMuxer(SampleCDRDemoContext *pContext, int64_t fallocateLength, int muxChn)
{
    SampleCDRDemoConfig *pConfigPara = &pContext->mConfigPara;
    int result = 0;
    ERRORTYPE ret;

    int fd = generateNextFileFd(pConfigPara);
    if (fd < 0)
    {
        aloge("fatal error! fail to open %s", pConfigPara->mRecorderCurFileName);
        return -1;
    }

    if (pContext->mRecorderContext.mMuxChn == muxChn)
    {
        ret = AW_MPI_MUX_SwitchFd(muxChn, fd, (int)fallocateLength);
        if(ret != SUCCESS)
        {
            aloge("fatal error! muxChn[%d] switch fd[%d] fail[0x%x]!", muxChn, fd, ret);
            result = -1;
        }
    }
    else
    {
        aloge("fatal error! muxChn is not match:[0x%x!=0x%x]", pContext->mRecorderContext.mMuxChn, muxChn);
        result = -1;
    }

    close(fd);
    return result;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    int ret;
    ERRORTYPE eRet = SUCCESS;
    StreamContext *pStreamContext = (StreamContext *)cookie;
    SampleCDRDemoContext *pContext = (SampleCDRDemoContext *)pStreamContext->priv;

    if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                break;
            }
            case MPP_EVENT_VENC_TIMEOUT:
            {
                uint64_t framePts = *(uint64_t*)pEventData;
                alogw("Be careful! detect encode timeout, pts[%lld]us", framePts);
                break;
            }
            case MPP_EVENT_VENC_BUFFER_FULL:
            {
                alogw("Be careful! detect venc buffer full");
                break;
            }
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM_EXTRA:
            {
                VENC_Isp2VeExtraParam *pExtraParam = (VENC_Isp2VeExtraParam *)pEventData;
                pExtraParam->eEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                break;
            }
            default:
            {
                alogv("fatal error! unknown event[%d]", event);
                break;
            }
        }
    }
    else if(MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                SampleCDRDemo_MessageData stMsgData;

                alogd("MuxChn[%d] record file done. stream:%p", *(int *)pEventData, cookie);
                stMsgData.pStreamContext = (StreamContext *)cookie;
                stCmdMsg.command = Rec_FileDone;
                stCmdMsg.para0 = *(int *)pEventData;
                stCmdMsg.mDataSize = sizeof(SampleCDRDemo_MessageData);
                stCmdMsg.mpData = &stMsgData;

                putMessageWithData(&pContext->mMsgQueue, &stCmdMsg);
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                message_t stMsgCmd;
                InitMessage(&stMsgCmd);
                SampleCDRDemo_MessageData stMsgData;

                alogd("MuxChn[%d] need next fd. stream:%p", *(int *)pEventData, cookie);
                stMsgData.pStreamContext = (StreamContext *)cookie;
                stMsgCmd.command = Rec_NeedSetNextFd;
                stMsgCmd.para0 = *(int *)pEventData;
                stMsgCmd.mDataSize = sizeof(SampleCDRDemo_MessageData);
                stMsgCmd.mpData = &stMsgData;

                putMessageWithData(&pContext->mMsgQueue, &stMsgCmd);
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x]", event);
                break;
            }
        }
    }
    else if (MOD_ID_VOU == pChn->mModId)
    {
        switch (event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("vo report video display size[%dx%d]", pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                alogd("vo report rendering start");
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else
    {
         aloge("fatal error! unknown chn[%d,%d,%d]", pChn->mModId, pChn->mDevId, pChn->mChnId);
    }

    return eRet;
}

static void setRegionDetectLink(int nVEncChn, int nRegionLinkEn, int nRegionLinkTexDetectEn, int nRegionLinkMotionDetectEn, int nRegionLinkMotionDetectInv)
{
    sRegionLinkParam stRegionLinkParam;
    memset(&stRegionLinkParam, 0, sizeof(sRegionLinkParam));
    AW_MPI_VENC_GetRegionDetectLink(nVEncChn, &stRegionLinkParam);
    if (stRegionLinkParam.mStaticParam.region_link_en != nRegionLinkEn)
    {
        alogd("VencChn[%d] region_link_en: %d -> %d", nVEncChn, stRegionLinkParam.mStaticParam.region_link_en, nRegionLinkEn);
        stRegionLinkParam.mStaticParam.region_link_en = nRegionLinkEn;
    }
    if (stRegionLinkParam.mStaticParam.tex_detect_en != nRegionLinkTexDetectEn)
    {
        alogd("VencChn[%d] tex_detect_en: %d -> %d", nVEncChn, stRegionLinkParam.mStaticParam.tex_detect_en, nRegionLinkTexDetectEn);
        stRegionLinkParam.mStaticParam.tex_detect_en = nRegionLinkTexDetectEn;
    }
    if (stRegionLinkParam.mStaticParam.motion_detect_en != nRegionLinkMotionDetectEn)
    {
        alogd("VencChn[%d] motion_detect_en: %d -> %d", nVEncChn, stRegionLinkParam.mStaticParam.motion_detect_en, nRegionLinkMotionDetectEn);
        stRegionLinkParam.mStaticParam.motion_detect_en = nRegionLinkMotionDetectEn;
    }
    //if (stRegionLinkParam.mStaticParam.updateInterVal != nRegionLinkMotionDetectInv)
    //{
    //    alogd("VencChn[%d] updateInterVal: %d -> %d", nVEncChn, stRegionLinkParam.mStaticParam.updateInterVal, nRegionLinkMotionDetectInv);
    //    stRegionLinkParam.mStaticParam.updateInterVal = nRegionLinkMotionDetectInv;
    //}
    AW_MPI_VENC_SetRegionDetectLink(nVEncChn, &stRegionLinkParam);
}

static int createVODev(StreamContext *pPreviewContext)
{
    int ret = 0;

    pPreviewContext->mVODev = 0;
    ret = AW_MPI_VO_Enable(pPreviewContext->mVODev);
    if (SUCCESS != ret)
    {
        aloge("fatal error! vo dev[%d] enable fail!", pPreviewContext->mVODev);
        return -1;
    }
    //pPreviewContext->mUILayer = HLAY(2, 0);
    //AW_MPI_VO_AddOutsideVideoLayer(pPreviewContext->mUILayer);
    //AW_MPI_VO_CloseVideoLayer(pPreviewContext->mUILayer); /* close ui layer. */
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(pPreviewContext->mVODev, &spPubAttr);
    spPubAttr.enIntfType = VO_INTF_LCD;
    spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(pPreviewContext->mVODev, &spPubAttr);

    return ret;
}

static int createVOChn(StreamContext *pPreviewContext)
{
    int ret = 0;

    ret = AW_MPI_VO_EnableVideoLayer(pPreviewContext->mVOLayer);
    if (0 != ret)
    {
        aloge("fatal error! enable video layer fail! ret[0x%x]", ret);
        return -1;
    }

    AW_MPI_VO_SetVideoLayerAttr(pPreviewContext->mVOLayer, &pPreviewContext->mVOLayerAttr);

    ret = AW_MPI_VO_CreateChn(pPreviewContext->mVOLayer, pPreviewContext->mVOChn);
    if (SUCCESS != ret)
    {
        aloge("fatal error! create vo channel[%d] ret[0x%x]!", pPreviewContext->mVOChn, ret);
        return -1;
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&pPreviewContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VO_RegisterCallback(pPreviewContext->mVOLayer, pPreviewContext->mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(pPreviewContext->mVOLayer, pPreviewContext->mVOChn, 2);

    alogd("create vo layer[%d] create vo chn[%d] success", pPreviewContext->mVOLayer, pPreviewContext->mVOChn);

    return 0;
}

static int PrepareTakePicture(SampleCDRDemoContext *pContext)
{
    SampleCDRDemoConfig *pConfigPara = &pContext->mConfigPara;
    ERRORTYPE ret = SUCCESS;
    int result = SUCCESS;
    unsigned int nVbvBufSize = 0;
    unsigned int vbvThreshSize = 0;
    unsigned int src_picWidth = 0;
    unsigned int src_picHeight = 0;
    unsigned int dst_picWidth = 0;
    unsigned int dst_picHeight = 0;
    unsigned int mPictureNum = 1;

    StreamContext *pTakePicContext = NULL;
    if (pConfigPara->mTakePicTureVIDev == pConfigPara->mRecorderConfig.mVIDev)
    {
        pTakePicContext = &pContext->mRecorderContext;
    }
    else
    {
        aloge("fatal error! take picture vipp[%d] unexist", pConfigPara->mTakePicTureVIDev);
        pConfigPara->mTakePicture = 0;
        return -1;
    }

    src_picWidth = pTakePicContext->mVIAttr.format.width;
    src_picHeight = pTakePicContext->mVIAttr.format.height;
    dst_picWidth = pConfigPara->mRecorderConfig.mEncWidth;
    dst_picHeight = pConfigPara->mRecorderConfig.mEncHeight;

    ViVirChnAttrS nViChnAttr;
    memset(&nViChnAttr, 0, sizeof(ViVirChnAttrS));
    nViChnAttr.mCacheFrameNum = 1;
    AW_MPI_VI_CreateVirChn(pConfigPara->mTakePicTureVIDev, pConfigPara->mTakePictureViChn, &nViChnAttr);

    // calc vbv buf size and vbv threshold size
    unsigned int minVbvBufSize = dst_picWidth * dst_picHeight * 3/2;
    vbvThreshSize = dst_picWidth * dst_picHeight;
    nVbvBufSize = (dst_picWidth * dst_picHeight * 3/2 /10 * mPictureNum) + vbvThreshSize;
    if(nVbvBufSize < minVbvBufSize)
    {
        nVbvBufSize = minVbvBufSize;
    }
    if(nVbvBufSize > 16*1024*1024)
    {
        alogd("Be careful! vbvSize[%d]MB is too large, decrease to threshSize[%d]MB + 1MB", nVbvBufSize/(1024*1024), vbvThreshSize/(1024*1024));
        nVbvBufSize = vbvThreshSize + 1*1024*1024;
    }
    nVbvBufSize = AWALIGN(nVbvBufSize, 1024);

    VENC_CHN_ATTR_S venc_chn_attr;
    memset(&venc_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));
    venc_chn_attr.VeAttr.Type = PT_JPEG;
    //venc_chn_attr.VeAttr.mVippID = pConfigPara->mTakePicTureVIDev;
    if (pConfigPara->mTakePictureOnline)
    {
        venc_chn_attr.VeAttr.mOnlineEnable = pConfigPara->mRecorderConfig.mEncOnlineEnable;
        venc_chn_attr.VeAttr.mOnlineShareBufNum = pConfigPara->mRecorderConfig.mEncOnlineShareBufNum;
        alogd("takepic Online, vencChn%d online %d bufnum %d", pConfigPara->mTakePictureEncChn, venc_chn_attr.VeAttr.mOnlineEnable, venc_chn_attr.VeAttr.mOnlineShareBufNum);
    }
    venc_chn_attr.VeAttr.AttrJpeg.MaxPicWidth = 0;
    venc_chn_attr.VeAttr.AttrJpeg.MaxPicHeight = 0;
    venc_chn_attr.VeAttr.AttrJpeg.BufSize = nVbvBufSize;
    venc_chn_attr.VeAttr.AttrJpeg.mThreshSize = vbvThreshSize;
    venc_chn_attr.VeAttr.AttrJpeg.bByFrame = TRUE;
    venc_chn_attr.VeAttr.AttrJpeg.PicWidth = dst_picWidth;
    venc_chn_attr.VeAttr.AttrJpeg.PicHeight = dst_picHeight;
    venc_chn_attr.VeAttr.AttrJpeg.bSupportDCF = FALSE;
    venc_chn_attr.VeAttr.MaxKeyInterval = 1;
    venc_chn_attr.VeAttr.SrcPicWidth = src_picWidth;
    venc_chn_attr.VeAttr.SrcPicHeight = src_picHeight;
    venc_chn_attr.VeAttr.Field = VIDEO_FIELD_FRAME;
    venc_chn_attr.VeAttr.PixelFormat = map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(pTakePicContext->mVIAttr.format.pixelformat);
    venc_chn_attr.VeAttr.OutputFormat = VENC_OUTPUT_SAME_AS_INPUT;
    venc_chn_attr.VeAttr.mColorSpace = V4L2_COLORSPACE_JPEG;
    alogd("pixfmt:0x%x, output_format:0x%x, colorSpace:0x%x", venc_chn_attr.VeAttr.PixelFormat, venc_chn_attr.VeAttr.OutputFormat, venc_chn_attr.VeAttr.mColorSpace);

    ret = AW_MPI_VENC_CreateChn(pConfigPara->mTakePictureEncChn, &venc_chn_attr);
    if (SUCCESS != ret)
    {
        aloge("fatal error! create venc channel[%d] ret[0x%x]!", pConfigPara->mTakePictureEncChn, ret);
        result = FAILURE;
        goto _out0;
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void *)pTakePicContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VENC_RegisterCallback(pConfigPara->mTakePictureEncChn, &cbInfo);

    VENC_PARAM_JPEG_S jpeg_param;
    memset(&jpeg_param, 0, sizeof(VENC_PARAM_JPEG_S));
    jpeg_param.Qfactor = 20;
    AW_MPI_VENC_SetJpegParam(pConfigPara->mTakePictureEncChn, &jpeg_param);

    if (pConfigPara->mTakePictureOnline)
    {
        alogd("takepic Online, no need set ForbidDiscardingFrame");
    }
    else
    {
        AW_MPI_VENC_ForbidDiscardingFrame(pConfigPara->mTakePictureEncChn, TRUE);
    }

    if (pConfigPara->mTakePictureOnline)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pConfigPara->mTakePicTureVIDev, pConfigPara->mTakePictureViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pConfigPara->mTakePictureEncChn};
        ret = AW_MPI_SYS_Bind(&ViChn, &VeChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind vi[%d-%d] ve chn[%d] fail! ret[0x%x]", \
                pConfigPara->mTakePicTureVIDev, pConfigPara->mTakePictureViChn, pConfigPara->mTakePictureEncChn, ret);
            return -1;
        }
        alogd("takepic Online, bind vi[%d-%d] ve chn[%d]", pConfigPara->mTakePicTureVIDev, pConfigPara->mTakePictureViChn, pConfigPara->mTakePictureEncChn);
    }

    result = pthread_create(&pContext->mCSIFrameThreadId, NULL, takePictureThread, (void*)pContext);
    if (result != 0)
    {
        aloge("fatal error! pthread create fail[%d]", result);
    }

_out0:
    return result;
}

static int startTakePicture(SampleCDRDemoContext *pContext)
{
    //call mpi_venc to encode jpeg.
    int result = SUCCESS;
    ERRORTYPE ret = 0;

    AW_MPI_VI_EnableVirChn(pContext->mConfigPara.mTakePicTureVIDev, pContext->mConfigPara.mTakePictureViChn);

    ret = AW_MPI_VENC_StartRecvPic(pContext->mConfigPara.mTakePictureEncChn);
    if (SUCCESS != ret)
    {
        aloge("fatal error:%x jpegEnc AW_MPI_VENC_StartRecvPic",ret);
        result = FAILURE;
    }

    VENC_EXIFINFO_S exif_info;
    memset(&exif_info, 0, sizeof(VENC_EXIFINFO_S));
    time_t t;
    struct tm *tm_t;
    time(&t);
    tm_t = localtime(&t);
    snprintf((char *)exif_info.DateTime, MM_DATA_TIME_LENGTH, "%4d:%02d:%02d %02d:%02d:%02d",
        tm_t->tm_year + 1900, tm_t->tm_mon + 1, tm_t->tm_mday,
        tm_t->tm_hour, tm_t->tm_min, tm_t->tm_sec);

    if (pContext->mConfigPara.mTakePictureThumbEnable)
    {
        exif_info.ThumbWidth = 320;
        exif_info.ThumbHeight = 240;
        exif_info.thumb_quality = 60;
        exif_info.Orientation = 0;
        exif_info.FNumber.num = 26;
        exif_info.FNumber.den = 10;
        exif_info.MeteringMode = METERING_MODE_AVERAGE;
        exif_info.FocalLength.num = 228;
        exif_info.FocalLength.den = 100;
        exif_info.WhiteBalance = 0;
        exif_info.FocalLengthIn35mmFilm = 18;
        strcpy((char *)exif_info.ImageName, "aw-photo");
        AW_MPI_VENC_SetJpegExifInfo(pContext->mConfigPara.mTakePictureEncChn, &exif_info);
    }
    else
    {
        alogd("VeChn[%d] usr set jpeg thumb disable!", pContext->mConfigPara.mTakePictureEncChn);
    }

    alogd("VeChn[%d] start jpeg take picture, thumb_en: %d", pContext->mConfigPara.mTakePictureEncChn, pContext->mConfigPara.mTakePictureThumbEnable);

    return result;
}

static int stopTakePicture(SampleCDRDemoContext *pContext)
{
    int result = SUCCESS;

    AW_MPI_VENC_StopRecvPic(pContext->mConfigPara.mTakePictureEncChn);
    AW_MPI_VENC_DestroyChn(pContext->mConfigPara.mTakePictureEncChn);

    alogd("VeChn[%d] stop jpeg take picture", pContext->mConfigPara.mTakePictureEncChn);

    return result;
}

static int destroyJpegPicture(SampleCDRDemoContext *pContext)
{
    int result = SUCCESS;
    void *pRetVal = NULL;

    pthread_join(pContext->mCSIFrameThreadId, &pRetVal);

    AW_MPI_VI_DisableVirChn(pContext->mConfigPara.mTakePicTureVIDev, pContext->mConfigPara.mTakePictureViChn);
    AW_MPI_VI_DestroyVirChn(pContext->mConfigPara.mTakePicTureVIDev, pContext->mConfigPara.mTakePictureViChn);

    return result;
}

static void *takePictureThread(void *pThreadData)
{
    SampleCDRDemoContext *pContext = (SampleCDRDemoContext*)pThreadData;
    SampleCDRDemoConfig *pConfigPara = &pContext->mConfigPara;
    ERRORTYPE ret = 0;
    static int count = 0;
    unsigned int cur_time = 0;
    char strThreadName[32];
    sprintf(strThreadName, "takepic");
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    if (SUCCESS != startTakePicture(pContext))
    {
        aloge("fatal error! VeChn%d startTakePicture fail!", pConfigPara->mTakePictureEncChn);
        return NULL;
    }

    pConfigPara->mTakePictureInterval = 5;

    cur_time = getSysTickMs();

    while (!pContext->mbExitFlag)
    {
        if (getSysTickMs() >= cur_time + pConfigPara->mTakePictureInterval * 1000)
        {
            cur_time = getSysTickMs();
            if (pConfigPara->mTakePictureOnline)
            {
                alogd("VeChn[%d] enable Online TakePicture not support now !", pConfigPara->mTakePictureEncChn);
                //AW_MPI_VENC_EnableOnlineTakePicture(pConfigPara->mTakePictureEncChn, TRUE);
            }
            else
            {
                VIDEO_FRAME_INFO_S video_frame;
                memset(&video_frame, 0, sizeof(video_frame));

                ret = AW_MPI_VI_GetFrame(pConfigPara->mTakePicTureVIDev, pConfigPara->mTakePictureViChn, &video_frame, 2000);
                if (ret != 0)
                {
                    aloge("fatal error, vi[%d %d] get frame fail", pConfigPara->mTakePicTureVIDev, pConfigPara->mTakePictureViChn);
                    return NULL;
                }

                ret = AW_MPI_VENC_SendFrameSync(pConfigPara->mTakePictureEncChn, &video_frame, 10 * 1000);
                if (ret != 0)
                {
                    aloge("fatal error! VeChn%d send frame sync failed! ret[0x%x]", pConfigPara->mTakePictureEncChn, ret);
                    AW_MPI_VI_ReleaseFrame(pConfigPara->mTakePicTureVIDev, pConfigPara->mTakePictureViChn, &video_frame);
                    return NULL;
                }
                alogd("VeChn[%d] send frame sync done", pConfigPara->mTakePictureEncChn);
                AW_MPI_VI_ReleaseFrame(pConfigPara->mTakePicTureVIDev, pConfigPara->mTakePictureViChn, &video_frame);
            }

            VENC_STREAM_S venc_stream;
            VENC_PACK_S pack;
            memset(&venc_stream, 0, sizeof(venc_stream));
            memset(&pack, 0, sizeof(pack));
            venc_stream.mPackCount = 1;
            venc_stream.mpPack = &pack;

            ret = AW_MPI_VENC_GetStream(pConfigPara->mTakePictureEncChn, &venc_stream, 1000);
            if (ret != SUCCESS)
            {
                aloge("fatal error! VeChn%d why get stream fail?", pConfigPara->mTakePictureEncChn);
            }
            else
            {
                char strFilePath[MAX_FILE_PATH_SIZE];
                snprintf(strFilePath, MAX_FILE_PATH_SIZE, "%s_%d.jpg", pConfigPara->mTakePictureFile, count);
                FILE *fpPic = fopen(strFilePath, "wb");
                if(fpPic != NULL)
                {
                    if(venc_stream.mpPack[0].mpAddr0 != NULL && venc_stream.mpPack[0].mLen0 > 0)
                        fwrite(venc_stream.mpPack[0].mpAddr0, 1, venc_stream.mpPack[0].mLen0, fpPic);
                    if(venc_stream.mpPack[0].mpAddr1 != NULL && venc_stream.mpPack[0].mLen1 > 0)
                        fwrite(venc_stream.mpPack[0].mpAddr1, 1, venc_stream.mpPack[0].mLen1, fpPic);
                    if(venc_stream.mpPack[0].mpAddr2 != NULL && venc_stream.mpPack[0].mLen2 > 0)
                        fwrite(venc_stream.mpPack[0].mpAddr2, 1, venc_stream.mpPack[0].mLen2, fpPic);
                    fclose(fpPic);
                    alogd("VeChn%d store jpeg in file[%s]", pConfigPara->mTakePictureEncChn, strFilePath);
                    count++;
                    FilePathNode *pNode = (FilePathNode *)malloc(sizeof(FilePathNode));
                    if (pNode)
                    {
                        memset(pNode, 0, sizeof(FilePathNode));
                        strncpy(pNode->strFilePath, strFilePath, MAX_FILE_PATH_SIZE - 1);
                        list_add_tail(&pNode->mList, &pContext->PicFilePathList);
                    }
                    else
                    {
                        aloge("fatal error! pNode malloc fail!");
                    }
                    int cnt = list_count_nodes(&pContext->PicFilePathList);
                    while (cnt > pContext->mConfigPara.nTakePictureFileCnt && pContext->mConfigPara.nTakePictureFileCnt > 0)
                    {
                        FilePathNode *pNode = list_first_entry(&pContext->PicFilePathList, FilePathNode, mList);
                        list_del(&pNode->mList);
                        cnt--;
                        if ((ret = remove(pNode->strFilePath)) != 0)
                        {
                            aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
                        }
                        else
                        {
                            alogd("delete file[%s] success", pNode->strFilePath);
                        }
                        free(pNode);
                    }
                }
                else
                {
                    aloge("fatal error! VeChn%d open file[%s] fail! errno(%d)", pConfigPara->mTakePictureEncChn, strFilePath, errno);
                }

                ret = AW_MPI_VENC_ReleaseStream(pConfigPara->mTakePictureEncChn, &venc_stream);
                if (ret != SUCCESS)
                {
                    aloge("fatal error! VeChn%d why release stream fail(0x%x)?", pConfigPara->mTakePictureEncChn, ret);
                }
            }
        }
        else
        {
            usleep(200*1000);
        }
    }

    stopTakePicture(pContext);

    return NULL;
}


static void *previewGetStreamThread(void *pThreadData)
{
    SampleCDRDemoContext *pContext = (SampleCDRDemoContext *)pThreadData;
    StreamConfig *pPreviewConfigPara = &pContext->mConfigPara.mPreviewConfig;
    StreamContext *pPreviewContext = &pContext->mPreviewContext;

    char strThreadName[32];
    sprintf(strThreadName, "venc%d-stream", pPreviewContext->mVeChn);
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    ERRORTYPE ret = SUCCESS;
    int result = 0;
    unsigned int nStreamLen = 0;

    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    memset(&stVencStream, 0, sizeof(stVencStream));
    memset(&stVencPack, 0, sizeof(stVencPack));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;

    unsigned int buf_size = 0;
    unsigned char *stream_buf = NULL;
    int rtsp_test_enable = 0;

    buf_size = pPreviewConfigPara->mEncWidth * pPreviewConfigPara->mEncHeight * 3 / 2 / 10;

    if (0 >= buf_size)
    {
        aloge("fatal error! VencChn[%d] buf_size %d is invalid param!", pPreviewContext->mVeChn, buf_size);
        return NULL;
    }
    stream_buf = (unsigned char *)malloc(buf_size);
    if (NULL == stream_buf)
    {
        aloge("malloc stream_buf failed, size=%d", buf_size);
        return NULL;
    }
    memset(stream_buf, 0, buf_size);
    alogd("VencChn[%d] stream_buf:%p, size=%d", pPreviewContext->mVeChn, stream_buf, buf_size);

    if (pContext->mConfigPara.mPreviewRtspId >= 0)
    {
        rtsp_start(pContext->mConfigPara.mPreviewRtspId);
    }

    int mActualFrameRate = 0;
    int mTempFrameCnt = 0;
    int mTempCurrentTime = 0;
    int mTempLastTime = 0;
    uint64_t mLastPts = 0;

    //set spspps
    VencHeaderData mSpsPpsInfo;
    memset(&mSpsPpsInfo, 0, sizeof(VencHeaderData));
    if (pPreviewContext->mVeChnAttr.VeAttr.Type == PT_H264)
    {
        AW_MPI_VENC_GetH264SpsPpsInfo(pPreviewContext->mVeChn, &mSpsPpsInfo);
    }
    else if(pPreviewContext->mVeChnAttr.VeAttr.Type == PT_H265)
    {
        AW_MPI_VENC_GetH265SpsPpsInfo(pPreviewContext->mVeChn, &mSpsPpsInfo);
    }

    while (!pContext->mbExitFlag)
    {
        memset(stVencStream.mpPack, 0, sizeof(VENC_PACK_S));
        ret = AW_MPI_VENC_GetStream(pPreviewContext->mVeChn, &stVencStream, 4000);
        if(SUCCESS == ret)
        {
            nStreamLen = stVencStream.mpPack[0].mLen0 + stVencStream.mpPack[0].mLen1;
            if (nStreamLen <= 0 || nStreamLen > buf_size)
            {
                aloge("fatal error! VencStream length error, drop it, StreamLen:%d, buf_size:%d, [%d,%d]!",
                    nStreamLen, buf_size, stVencStream.mpPack[0].mLen0, stVencStream.mpPack[0].mLen1);
                AW_MPI_VENC_ReleaseStream(pPreviewContext->mVeChn, &stVencStream);
                continue;
            }

            // check actual framerate.
            mTempFrameCnt++;
            if (0 == mTempLastTime)
            {
                mTempLastTime = getSysTickMs();
            }
            else
            {
                mTempCurrentTime = getSysTickMs();
                if (mTempCurrentTime >= mTempLastTime + 1000)
                {
                    mActualFrameRate = mTempFrameCnt;
                    if (mActualFrameRate < pPreviewConfigPara->mEncFrmRate)
                    {
                        int sensor_fps = 0;
                        AW_MPI_ISP_GetSensorFps(pPreviewConfigPara->mIspDev, &sensor_fps);
                        alogv("sensor_fps:%d, venc fps:%d", sensor_fps, pPreviewConfigPara->mEncFrmRate);
                        if (sensor_fps == pPreviewConfigPara->mEncFrmRate)
                        {
                            alogw("VencChn[%d], actualFrameRate %d < dstFrmRate %d", pPreviewContext->mVeChn, mActualFrameRate, pPreviewConfigPara->mEncFrmRate);
                        }
                    }
                    mTempLastTime = mTempCurrentTime;
                    mTempFrameCnt = 0;
                }
            }

            uint64_t pts = stVencStream.mpPack->mPTS;
            int len = 0;
            char keyframe = 0;
            char stream_buf_empty = 0;
            if (stVencStream.mpPack != NULL && stVencStream.mpPack->mLen0 > 0)
            {
                if (PT_H264 == pPreviewContext->mVeChnAttr.VeAttr.Type)
                {
                    if (H264E_NALU_ISLICE == stVencStream.mpPack->mDataType.enH264EType)
                    {
                        if (NULL == mSpsPpsInfo.pBuffer)
                        {
                            alogd("SpsPpsInfo.pBuffer = NULL!!\n");
                        }
                        if (0 < mSpsPpsInfo.nLength && buf_size >= mSpsPpsInfo.nLength)
                        {
                            /* Get sps/pps first */
                            memcpy(stream_buf, mSpsPpsInfo.pBuffer, mSpsPpsInfo.nLength);
                            len += mSpsPpsInfo.nLength;
                        }
                        else
                        {
                            stream_buf_empty = 1;
                            aloge("fatal error! VeChn[%d] stream buf size %d is too small, h264 spspps len=%d !", pPreviewContext->mVeChn, buf_size, mSpsPpsInfo.nLength);
                        }
                        keyframe = 1;
                        alogv("***** VeChn[%d] H264 got I frame, Seq %d *****", pPreviewContext->mVeChn, stVencStream.mSeq);
                    }
                    else
                    {
                        alogv("VeChn[%d] H264 got P frame, Seq %d", pPreviewContext->mVeChn, stVencStream.mSeq);
                    }
                }
                else if (PT_H265 == pPreviewContext->mVeChnAttr.VeAttr.Type)
                {
                    if (H265E_NALU_ISLICE == stVencStream.mpPack->mDataType.enH265EType)
                    {
                        if (NULL == mSpsPpsInfo.pBuffer)
                        {
                            alogd("SpsPpsInfo.pBuffer = NULL!!\n");
                        }
                        if (0 < mSpsPpsInfo.nLength && buf_size >= mSpsPpsInfo.nLength)
                        {
                            /* Get sps/pps first */
                            memcpy(stream_buf, mSpsPpsInfo.pBuffer, mSpsPpsInfo.nLength);
                            len += mSpsPpsInfo.nLength;
                        }
                        else
                        {
                            stream_buf_empty = 1;
                            aloge("fatal error! VeChn[%d] stream buf size %d is too small, h265 spspps len=%d !", pPreviewContext->mVeChn, buf_size, mSpsPpsInfo.nLength);
                        }
                        keyframe = 1;
                        alogv("***** VeChn[%d] H265 got I frame, Seq %d *****", pPreviewContext->mVeChn, stVencStream.mSeq);
                    }
                    else
                    {
                        alogv("VeChn[%d] H265 got P frame, Seq %d", pPreviewContext->mVeChn, stVencStream.mSeq);
                    }
                }
                else
                {
                    aloge("fatal error! vencType:0x%x is wrong!", pPreviewContext->mVeChnAttr.VeAttr.Type);
                }

                if (stVencStream.mpPack->mLen0 > 0)
                {
                    if (buf_size >= (len + stVencStream.mpPack->mLen0))
                    {
                        memcpy(stream_buf + len, stVencStream.mpPack->mpAddr0, stVencStream.mpPack->mLen0);
                        len += stVencStream.mpPack->mLen0;
                    }
                    else
                    {
                        stream_buf_empty = 1;
                        aloge("fatal error! VeChn[%d] stream buf size %d is too small, len=%d !", pPreviewContext->mVeChn, buf_size, len + stVencStream.mpPack->mLen0);
                    }
                }
                if (stVencStream.mpPack->mLen1 > 0)
                {
                    if (buf_size >= (len + stVencStream.mpPack->mLen1))
                    {
                        memcpy(stream_buf + len, stVencStream.mpPack->mpAddr1, stVencStream.mpPack->mLen1);
                        len += stVencStream.mpPack->mLen1;
                    }
                    else
                    {
                        stream_buf_empty = 1;
                        aloge("fatal error! VeChn[%d] stream buf size %d is too small, len=%d !", pPreviewContext->mVeChn, buf_size, len + stVencStream.mpPack->mLen1);
                    }
                }
            }
            ret = AW_MPI_VENC_ReleaseStream(pPreviewContext->mVeChn, &stVencStream);
            if (ret != SUCCESS)
            {
                aloge("fatal error! venc_chn[%d] releaseStream fail", pPreviewContext->mVeChn);
            }

            if ((stream_buf != NULL) && len > 0 && (0 == stream_buf_empty))
            {
                /* get current fps and set to rtsp to avoid rtsp checkDurationTime warn */
                int sensor_fps = 0;
                AW_MPI_ISP_GetSensorFps(pPreviewContext->mIspDev, &sensor_fps);

                RtspSendDataParam stRtspParam;
                memset(&stRtspParam, 0, sizeof(RtspSendDataParam));
                stRtspParam.buf = stream_buf;
                stRtspParam.size = len;
                stRtspParam.frame_type = (1 == keyframe) ? RTSP_FRAME_DATA_TYPE_I : RTSP_FRAME_DATA_TYPE_P;
                stRtspParam.pts = pts;
                stRtspParam.frame_rate = sensor_fps;
                alogv("VencChn[%d] rtsp id[%d] RtspParam %p %d %d %lld, %dfps",
                    pPreviewContext->mVeChn, pContext->mConfigPara.mMainRtspID,
                    stRtspParam.buf, stRtspParam.size, stRtspParam.frame_type, stRtspParam.pts, stRtspParam.frame_rate);
                int start_time = getSysTickMs();
                if (pContext->mConfigPara.mPreviewRtspId >= 0)
                {
                    rtsp_sendData(pContext->mConfigPara.mPreviewRtspId, &stRtspParam);
                }
                int end_time = getSysTickMs();
                if (100 < end_time - start_time)
                {
                    alogw("venc_chn[%d] rtsp_sendData cost %dms > 100ms", pPreviewContext->mVeChn, end_time - start_time);
                }
            }
        }
        else
        {
            alogw("fatal error! vencChn[%d] getStream failed! check code! ret=0x%x", pPreviewContext->mVeChn, ret);
            continue;
        }
    }

    if (stream_buf)
    {
        free(stream_buf);
        stream_buf = NULL;
    }

    alogd("exit");

    return (void *)result;
}

static void configViAttr(StreamConfig *pStreamConfigPara, VI_ATTR_S *pViAttr)
{
    if (pStreamConfigPara->mEncOnlineEnable)
    {
        pViAttr->mOnlineEnable = 1;
        pViAttr->mOnlineShareBufNum = pStreamConfigPara->mEncOnlineShareBufNum;
    }
    pViAttr->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pViAttr->memtype = V4L2_MEMORY_MMAP;
    pViAttr->format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pStreamConfigPara->mCapFormat);
    pViAttr->format.field = V4L2_FIELD_NONE;
    pViAttr->format.colorspace = V4L2_COLORSPACE_REC709;
    pViAttr->format.width = pStreamConfigPara->mCapWidth;
    pViAttr->format.height = pStreamConfigPara->mCapHeight;
    pViAttr->nbufs = pStreamConfigPara->mVIBufNum;
    pViAttr->nplanes = 2;
    pViAttr->fps = pStreamConfigPara->mCapFrmRate;
    pViAttr->use_current_win = 0;
    pViAttr->wdr_mode = pStreamConfigPara->mEnableWDR;
    pViAttr->capturemode = V4L2_MODE_VIDEO;
    pViAttr->drop_frame_num = 0;
    if (pStreamConfigPara->mViStitchMode < DMA_STITCH_NONE || pStreamConfigPara->mViStitchMode > DMA_STITCH_MODE_MAX) {
        aloge("stitch_mode =%d, but it should be set to [0, 3], it will set it by default:0\n", pStreamConfigPara->mViStitchMode);
        pStreamConfigPara->mViStitchMode = DMA_STITCH_NONE;
    }
    pViAttr->large_dma_merge_en = pStreamConfigPara->mViStitchMode;
    pViAttr->mbEncppEnable = TRUE;
}

static int configVencChnAttr(StreamConfig *pStreamConfigPara, VENC_CHN_ATTR_S *pVencChnAttr, VENC_RC_PARAM_S *pVencRcParam)
{
    pVencChnAttr->VeAttr.Type = pStreamConfigPara->mEncType;
    //pVencChnAttr->VeAttr.mVippID = pStreamConfigPara->mVIDev;
    pVencChnAttr->VeAttr.mVeRecRefBufReduceEnable = pStreamConfigPara->mVeRecRefBufReduceEnable;
    pVencChnAttr->VeAttr.mVeRefFrameLbcMode = pStreamConfigPara->mEncRefFrameLbcMode;
    if (pStreamConfigPara->mEncOnlineEnable)
    {
        pVencChnAttr->VeAttr.mOnlineEnable = pStreamConfigPara->mEncOnlineEnable;
        pVencChnAttr->VeAttr.mOnlineShareBufNum = pStreamConfigPara->mEncOnlineShareBufNum;
        //if (1 == pStreamConfigPara->mViStitchMode)
        //{
        //    pVencChnAttr->VeAttr.mMultiSensorOnlineEnable = 1;
        //    pVencChnAttr->VeAttr.mImageStitchingEnable = 1;
        //    alogd("VencChn[%d] MultiSensorOnlineEnable:%d ImageStitchingEnable:%d", pStreamConfigPara->mEncChn,
        //        pVencChnAttr->VeAttr.mMultiSensorOnlineEnable,
        //        pVencChnAttr->VeAttr.mImageStitchingEnable);
        //}
    }
    //pVencChnAttr->VeAttr.mVeRxInputBufmultiplexEnable = pStreamConfigPara->mVeRxInputBufmultiplexEnable;

    unsigned int mThreshSize = AWALIGN((pStreamConfigPara->mEncWidth*pStreamConfigPara->mEncHeight*3/2)/3, 1024);
    unsigned int mVbvBufSize = AWALIGN(pStreamConfigPara->mEncBitRate * 2 / 8, 1024);
    if (pStreamConfigPara->mCapFrmRate)
    {
        mThreshSize = pStreamConfigPara->mEncBitRate/8/pStreamConfigPara->mCapFrmRate*15;
    }
    mVbvBufSize = pStreamConfigPara->mEncBitRate/8*4 + mThreshSize;
    alogd("vbvThreshSize: %d, vbvBufSize: %d", mThreshSize, mVbvBufSize);

    switch(pVencChnAttr->VeAttr.Type)
    {
        case PT_H264:
        {
            pVencChnAttr->VeAttr.AttrH264e.mThreshSize = mThreshSize;
            pVencChnAttr->VeAttr.AttrH264e.BufSize = mVbvBufSize;
            pVencChnAttr->VeAttr.AttrH264e.Profile = 2;//0:base 1:main 2:high
            pVencChnAttr->VeAttr.AttrH264e.bByFrame = TRUE;
            pVencChnAttr->VeAttr.AttrH264e.PicWidth  = pStreamConfigPara->mEncWidth;
            pVencChnAttr->VeAttr.AttrH264e.PicHeight = pStreamConfigPara->mEncHeight;
            pVencChnAttr->VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
            pVencChnAttr->VeAttr.AttrH264e.FastEncFlag = FALSE;
            pVencChnAttr->VeAttr.AttrH264e.IQpOffset = 0;
            pVencChnAttr->VeAttr.AttrH264e.mbPIntraEnable = TRUE;
            break;
        }
        case PT_H265:
        {
            pVencChnAttr->VeAttr.AttrH265e.mThreshSize = mThreshSize;
            pVencChnAttr->VeAttr.AttrH265e.mBufSize = mVbvBufSize;
            pVencChnAttr->VeAttr.AttrH265e.mProfile = 0; //0:main 1:main10 2:sti11
            pVencChnAttr->VeAttr.AttrH265e.mbByFrame = TRUE;
            pVencChnAttr->VeAttr.AttrH265e.mPicWidth = pStreamConfigPara->mEncWidth;
            pVencChnAttr->VeAttr.AttrH265e.mPicHeight = pStreamConfigPara->mEncHeight;
            pVencChnAttr->VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
            pVencChnAttr->VeAttr.AttrH265e.mFastEncFlag = FALSE;
            pVencChnAttr->VeAttr.AttrH265e.IQpOffset = 0;
            pVencChnAttr->VeAttr.AttrH265e.mbPIntraEnable = TRUE;
            break;
        }
        case PT_MJPEG:
        {
            pVencChnAttr->VeAttr.AttrMjpeg.mbByFrame = TRUE;
            pVencChnAttr->VeAttr.AttrMjpeg.mPicWidth = pStreamConfigPara->mEncWidth;
            pVencChnAttr->VeAttr.AttrMjpeg.mPicHeight = pStreamConfigPara->mEncHeight;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", pVencChnAttr->VeAttr.Type);
            break;
        }
    }
    pVencChnAttr->VeAttr.SrcPicWidth = pStreamConfigPara->mCapWidth;
    pVencChnAttr->VeAttr.SrcPicHeight = pStreamConfigPara->mCapHeight;
    pVencChnAttr->VeAttr.Field = VIDEO_FIELD_FRAME;
    pVencChnAttr->VeAttr.PixelFormat = pStreamConfigPara->mCapFormat;
    pVencChnAttr->VeAttr.mColorSpace = V4L2_COLORSPACE_REC709;
    pVencChnAttr->VeAttr.Rotate = ROTATE_NONE;
    pVencChnAttr->RcAttr.mProductMode = pStreamConfigPara->mProductMode;

    pVencChnAttr->EncppAttr.eEncppSharpSetting = VencEncppSharp_FollowISPConfig;

    switch(pVencChnAttr->VeAttr.Type)
    {
        case PT_H264:
        {
            switch (pStreamConfigPara->mEncRcMode)
            {
                case 1:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
                    pVencChnAttr->RcAttr.mAttrH264Vbr.mMaxBitRate = pStreamConfigPara->mEncBitRate;
                    pVencChnAttr->RcAttr.mAttrH264Vbr.mSrcFrmRate = pStreamConfigPara->mCapFrmRate;
                    pVencChnAttr->RcAttr.mAttrH264Vbr.mDstFrmRate = pStreamConfigPara->mEncFrmRate;
                    pVencRcParam->ParamH264Vbr.mMaxQp = 50;
                    pVencRcParam->ParamH264Vbr.mMinQp = 10;
                    pVencRcParam->ParamH264Vbr.mMaxPqp = 50;
                    pVencRcParam->ParamH264Vbr.mMinPqp = 10;
                    pVencRcParam->ParamH264Vbr.mQpInit = 38;
                    pVencRcParam->ParamH264Vbr.mMovingTh = 20;
                    pVencRcParam->ParamH264Vbr.mQuality = 10;
                    break;
                }
                case 0:
                default:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                    pVencChnAttr->RcAttr.mAttrH264Cbr.mBitRate = pStreamConfigPara->mEncBitRate;
                    pVencChnAttr->RcAttr.mAttrH264Cbr.mSrcFrmRate = pStreamConfigPara->mCapFrmRate;
                    pVencChnAttr->RcAttr.mAttrH264Cbr.mDstFrmRate = pStreamConfigPara->mEncFrmRate;
                    pVencRcParam->ParamH264Cbr.mMaxQp = 50;
                    pVencRcParam->ParamH264Cbr.mMinQp = 10;
                    pVencRcParam->ParamH264Cbr.mMaxPqp = 50;
                    pVencRcParam->ParamH264Cbr.mMinPqp = 10;
                    pVencRcParam->ParamH264Cbr.mQpInit = 38;
                    pVencRcParam->ParamH264Cbr.mbEnMbQpLimit = 1;
                    break;
                }
            }
            break;
        }
        case PT_H265:
        {
            switch (pStreamConfigPara->mEncRcMode)
            {
                case 1:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
                    pVencChnAttr->RcAttr.mAttrH265Vbr.mMaxBitRate = pStreamConfigPara->mEncBitRate;
                    pVencChnAttr->RcAttr.mAttrH265Vbr.mSrcFrmRate = pStreamConfigPara->mCapFrmRate;
                    pVencChnAttr->RcAttr.mAttrH265Vbr.mDstFrmRate = pStreamConfigPara->mEncFrmRate;
                    pVencRcParam->ParamH265Vbr.mMaxQp = 51;
                    pVencRcParam->ParamH265Vbr.mMinQp = 10;
                    pVencRcParam->ParamH265Vbr.mMaxPqp = 50;
                    pVencRcParam->ParamH265Vbr.mMinPqp = 10;
                    pVencRcParam->ParamH265Vbr.mQpInit = 38;
                    pVencRcParam->ParamH265Vbr.mMovingTh = 20;
                    pVencRcParam->ParamH265Vbr.mQuality = 10;
                    break;
                }
                case 0:
                default:
                {
                    pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                    pVencChnAttr->RcAttr.mAttrH265Cbr.mBitRate = pStreamConfigPara->mEncBitRate;
                    pVencChnAttr->RcAttr.mAttrH265Cbr.mSrcFrmRate = pStreamConfigPara->mCapFrmRate;
                    pVencChnAttr->RcAttr.mAttrH265Cbr.mDstFrmRate = pStreamConfigPara->mEncFrmRate;
                    pVencRcParam->ParamH265Cbr.mMaxQp = 51;
                    pVencRcParam->ParamH265Cbr.mMinQp = 1;
                    pVencRcParam->ParamH265Cbr.mMaxPqp = 50;
                    pVencRcParam->ParamH265Cbr.mMinPqp = 10;
                    pVencRcParam->ParamH265Cbr.mQpInit = 38;
                    break;
                }
            }
            break;
        }
        case PT_MJPEG:
        {
            if(pStreamConfigPara->mEncRcMode != 0)
            {
                aloge("fatal error! mjpeg don't support rcMode[%d]!", pStreamConfigPara->mEncBitRate);
            }
            pVencChnAttr->RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pVencChnAttr->RcAttr.mAttrMjpegeCbr.mBitRate = pStreamConfigPara->mEncBitRate;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", pVencChnAttr->VeAttr.Type);
            break;
        }
    }
    pVencChnAttr->GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    alogd("venc ste Rcmode=%d", pVencChnAttr->RcAttr.mRcMode);

    return 0;
}

static void configAiChnAttr(SampleCDRDemoConfig *pConfigPara, AIO_ATTR_S *pAiChnAttr)
{
    AudioConfig *pAudioConfig = &pConfigPara->mAudioConfig;
    pAiChnAttr->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(pAudioConfig->mCaptureSampleRate);
    pAiChnAttr->enBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(pAudioConfig->mCaptureBitWitdh);
    pAiChnAttr->enSoundmode = (pAudioConfig->mCaptureChannelCnt == 1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    pAiChnAttr->mMicNum = 1;
    pAiChnAttr->mChnCnt = pAudioConfig->mCaptureChannelCnt;
    pAiChnAttr->ai_ans_en = pAudioConfig->mCaptureAnsEn;
    if (pAiChnAttr->ai_ans_en)
        pAiChnAttr->ai_ans_mode = 3;
    pAiChnAttr->ai_agc_en = pAudioConfig->mCaptureAgcEn;
    if (pAiChnAttr->ai_agc_en) {
        pAiChnAttr->ai_agc_float_cfg.fTargetDb = 0;
        pAiChnAttr->ai_agc_float_cfg.fMaxGainDb = 30;
    }
    pAiChnAttr->ai_aec_en = pAudioConfig->mCaptureAecEn;
}

static void configAencChnAttr(SampleCDRDemoConfig *pConfigPara, AENC_CHN_ATTR_S *pAencChnAttr)
{
    AudioConfig *pAudioConfig = &pConfigPara->mAudioConfig;
    pAencChnAttr->AeAttr.Type = pAudioConfig->mAencType;
    pAencChnAttr->AeAttr.channels = pAudioConfig->mCaptureChannelCnt;
    pAencChnAttr->AeAttr.bitsPerSample = pAudioConfig->mCaptureBitWitdh;
    pAencChnAttr->AeAttr.sampleRate = pAudioConfig->mCaptureSampleRate;
    pAencChnAttr->AeAttr.bitRate = pAudioConfig->mAencBitRate;
    pAencChnAttr->AeAttr.attachAACHeader = 0; //aacMuxer will add adts header, so aac encoder need not attach aac header.
    pAencChnAttr->AeAttr.mInBufSize = 0;
    pAencChnAttr->AeAttr.mOutBufCnt = 0;
}

static void configMuxChnAttr(SampleCDRDemoConfig *pConfigPara, MUX_CHN_ATTR_S *pMuxChnAttr)
{
    pMuxChnAttr->mVideoAttrValidNum = 1;
    pMuxChnAttr->mVideoAttr[0].mWidth = pConfigPara->mRecorderConfig.mEncWidth;
    pMuxChnAttr->mVideoAttr[0].mHeight = pConfigPara->mRecorderConfig.mEncHeight;
    pMuxChnAttr->mVideoAttr[0].mVideoFrmRate = pConfigPara->mRecorderConfig.mEncFrmRate*1000;
    pMuxChnAttr->mVideoAttr[0].mVideoEncodeType = pConfigPara->mRecorderConfig.mEncType;
    pMuxChnAttr->mVideoAttr[0].mVeChn = pConfigPara->mRecorderConfig.mEncChn;

    pMuxChnAttr->mChannels = pConfigPara->mAudioConfig.mCaptureChannelCnt;
    pMuxChnAttr->mBitsPerSample = pConfigPara->mAudioConfig.mCaptureBitWitdh;
    pMuxChnAttr->mSamplesPerFrame = MAXDECODESAMPLE;
    pMuxChnAttr->mSampleRate = pConfigPara->mAudioConfig.mCaptureSampleRate;
    pMuxChnAttr->mAudioEncodeType = pConfigPara->mAudioConfig.mAencType;
    pMuxChnAttr->mTextEncodeType = PT_MAX;

    pMuxChnAttr->mMediaFileFormat = getFileFormat(pConfigPara->mRecorderRecFileFormat);
    pMuxChnAttr->mMaxFileDuration = pConfigPara->mRecorderRecDuration * 1000;
    pMuxChnAttr->mMaxFileSizeBytes = 0;
    pMuxChnAttr->mCallbackOutFlag = FALSE;
    pMuxChnAttr->mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pMuxChnAttr->mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
}

static void configVOChnAttr(SampleCDRDemoConfig *pConfigPara, StreamContext *pPreviewContext)
{
    pPreviewContext->mVOLayer = 0;
    pPreviewContext->mVOChn = 0;
    memset(&pPreviewContext->mVOLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));
    pPreviewContext->mVOLayerAttr.stDispRect.X = pConfigPara->mPreviewDispX;
    pPreviewContext->mVOLayerAttr.stDispRect.Y = pConfigPara->mPreviewDispY;
    pPreviewContext->mVOLayerAttr.stDispRect.Width = pConfigPara->mPreviewDispWidth;
    pPreviewContext->mVOLayerAttr.stDispRect.Height = pConfigPara->mPreviewDispHeight;
}

static void configRecorder(SampleCDRDemoContext *pContext)
{
    SampleCDRDemoConfig *pConfigPara = &pContext->mConfigPara;
    StreamContext *pRecorderContext = &pContext->mRecorderContext;

    pRecorderContext->mVIDev = pConfigPara->mRecorderConfig.mVIDev;
    pRecorderContext->mVIChn = pConfigPara->mRecorderConfig.mViVirChn;
    pRecorderContext->mIspDev = pConfigPara->mRecorderConfig.mIspDev;
    pRecorderContext->mViStitchIspChannelId = pConfigPara->mRecorderConfig.mViStitchIspChannelId;
    pRecorderContext->mCapFrmRate = pConfigPara->mRecorderConfig.mCapFrmRate;
    configViAttr(&pConfigPara->mRecorderConfig, &pRecorderContext->mVIAttr);

    pConfigPara->mRecorderConfig.mVeRecRefBufReduceEnable = pConfigPara->mEncRecRefBufReduceEnable;
    pConfigPara->mRecorderConfig.mVeRxInputBufmultiplexEnable = pConfigPara->mVeRxInputBufmultiplexEnable;
    pRecorderContext->mVeChn = pConfigPara->mRecorderConfig.mEncChn;
    pRecorderContext->bVencIsp2VeEnable = (BOOL)pConfigPara->mRecorderConfig.bEncIsp2VeEnable;
    pRecorderContext->bVencVe2IspEnable = (BOOL)pConfigPara->mRecorderConfig.bEncVe2IspEnable;
    pRecorderContext->mRegionLinkEnable = pConfigPara->mRegionLinkEnable;
    pRecorderContext->mRegionLinkTexDetectEnable = pConfigPara->mRegionLinkTexDetectEnable;
    pRecorderContext->mRegionLinkMotionDetectEnable = pConfigPara->mRegionLinkMotionDetectEnable;
    pRecorderContext->mRegionLinkMotionDetectInv = pConfigPara->mRegionLinkMotionDetectInv;
    pRecorderContext->mEncFrmRate = pConfigPara->mRecorderConfig.mEncFrmRate;
    configVencChnAttr(&pConfigPara->mRecorderConfig, &pRecorderContext->mVeChnAttr, &pRecorderContext->mVencRcParam);

    pRecorderContext->mAiChn = 0;
    pRecorderContext->mAIDevId = 0;
    configAiChnAttr(pConfigPara, &pRecorderContext->mAiChnAttr);

    pRecorderContext->mAencChn = 0;
    configAencChnAttr(pConfigPara, &pRecorderContext->mAencChnAttr);

    pRecorderContext->mMuxChn = 0;
    memset(&pRecorderContext->mMuxChnAttr, 0, sizeof(MUX_CHN_ATTR_S));
    configMuxChnAttr(pConfigPara, &pRecorderContext->mMuxChnAttr);
}

static void configPreview(SampleCDRDemoContext *pContext)
{
    SampleCDRDemoConfig *pConfigPara = &pContext->mConfigPara;
    StreamContext *pPreviewContext = &pContext->mPreviewContext;

    pPreviewContext->mVIDev = pConfigPara->mPreviewConfig.mVIDev;
    pPreviewContext->mVIChn = pConfigPara->mPreviewConfig.mViVirChn;
    pPreviewContext->mIspDev = pConfigPara->mPreviewConfig.mIspDev;
    pPreviewContext->mViStitchIspChannelId = pConfigPara->mPreviewConfig.mViStitchIspChannelId;
    pPreviewContext->mCapFrmRate = pConfigPara->mPreviewConfig.mCapFrmRate;
    configViAttr(&pConfigPara->mPreviewConfig, &pPreviewContext->mVIAttr);

    pConfigPara->mPreviewConfig.mVeRecRefBufReduceEnable = pConfigPara->mEncRecRefBufReduceEnable;
    pConfigPara->mPreviewConfig.mVeRxInputBufmultiplexEnable = pConfigPara->mVeRxInputBufmultiplexEnable;
    pPreviewContext->mVeChn = pConfigPara->mPreviewConfig.mEncChn;
    pPreviewContext->bVencIsp2VeEnable = (BOOL)pConfigPara->mPreviewConfig.bEncIsp2VeEnable;
    pPreviewContext->bVencVe2IspEnable = (BOOL)pConfigPara->mPreviewConfig.bEncVe2IspEnable;
    pPreviewContext->mRegionLinkEnable = pConfigPara->mRegionLinkEnable;
    pPreviewContext->mRegionLinkTexDetectEnable = pConfigPara->mRegionLinkTexDetectEnable;
    pPreviewContext->mRegionLinkMotionDetectEnable = pConfigPara->mRegionLinkMotionDetectEnable;
    pPreviewContext->mRegionLinkMotionDetectInv = pConfigPara->mRegionLinkMotionDetectInv;
    pPreviewContext->mEncFrmRate = pConfigPara->mPreviewConfig.mEncFrmRate;
    configVencChnAttr(&pConfigPara->mPreviewConfig, &pPreviewContext->mVeChnAttr, &pPreviewContext->mVencRcParam);

    if (1 == pConfigPara->mPreviewMode)
    {
        configVOChnAttr(pConfigPara, pPreviewContext);
    }
}

static int createVipp(StreamContext *pStreamContext)
{
    ERRORTYPE ret = SUCCESS;

    ret = AW_MPI_VI_CreateVipp(pStreamContext->mVIDev);
    if (SUCCESS != ret)
    {
        aloge("fatal error! vi dev[%d] create fail!", pStreamContext->mVIDev);
        return -1;
    }

    /* open video0/4 when stitch_mode >= DMA_STITCH_HORIZONTAL for set some ioctl to video0/4 */
    if (pStreamContext->mVIAttr.large_dma_merge_en >= DMA_STITCH_HORIZONTAL) {
        alogd("It will open video%d when stitch_mode >= DMA_STITCH_HORIZONTAL\n", pStreamContext->mVIDev - 1);
        AW_MPI_VI_CreateVipp(pStreamContext->mVIDev - 1);
    }

    ret = AW_MPI_VI_SetVippAttr(pStreamContext->mVIDev, &pStreamContext->mVIAttr);
    if (SUCCESS != ret)
    {
        aloge("fatal error! vi dev[%d] set attr fail!", pStreamContext->mVIDev);
        return -1;
    }

    if (pStreamContext->mViStitchIspChannelId >= 0)
    {
        if (pStreamContext->mVIAttr.large_dma_merge_en > DMA_STITCH_NONE)
        {
            if (pStreamContext->mVIAttr.large_dma_merge_en == DMA_STITCH_2IN1_LINNER)
            {
                /* It must be set StitchMode and StatsSyncMode for DMA_STITCH_2IN1_LINNER */
                AW_MPI_ISP_SetStitchMode(pStreamContext->mIspDev, STITCH_2IN1_LINNER);
                AW_MPI_ISP_AWB_SetStatsSyncMode(pStreamContext->mIspDev, ISP0_ISP1_COMBINE);
            }
            AW_MPI_ISP_Run(pStreamContext->mViStitchIspChannelId);
        }
    }

    if (pStreamContext->mIspDev >= 0)
    {
        ret = AW_MPI_ISP_Run(pStreamContext->mIspDev);
        if (SUCCESS != ret)
        {
            aloge("fatal error! ISP[%d] init fail!", pStreamContext->mIspDev);
            return -1;
        }
    }

    ret = AW_MPI_VI_EnableVipp(pStreamContext->mVIDev);
    if (SUCCESS != ret)
    {
        aloge("fatal error! vi dev[%d] enable fail!", pStreamContext->mVIDev);
        return -1;
    }

    ret = AW_MPI_VI_CreateVirChn(pStreamContext->mVIDev, pStreamContext->mVIChn, NULL);
    if (SUCCESS != ret)
    {
        aloge("fatal error! vi chn[%d] create fail!", pStreamContext->mVIChn);
        return -1;
    }

    alogd("create vi dev[%d] vi chn[%d] success!", pStreamContext->mVIDev, pStreamContext->mVIChn);
    return 0;
}

static int createVencChn(StreamContext *pStreamContext)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    ret = AW_MPI_VENC_CreateChn(pStreamContext->mVeChn, &pStreamContext->mVeChnAttr);
    if (SUCCESS != ret)
    {
        alogd("fatal error! create venc channel[%d] fail! ret[%d]", pStreamContext->mVeChn, ret);
        return -1;
    }

    AW_MPI_VENC_SetRcParam(pStreamContext->mVeChn, &pStreamContext->mVencRcParam);
    VENC_FRAME_RATE_S stFrameRate;
    stFrameRate.SrcFrmRate = pStreamContext->mCapFrmRate;
    stFrameRate.DstFrmRate = pStreamContext->mEncFrmRate;
    alogd("VencChn[%d] set srcFrameRate:%d, venc framerate:%d", pStreamContext->mVeChn, stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
    ret = AW_MPI_VENC_SetFrameRate(pStreamContext->mVeChn, &stFrameRate);
    if (ret != SUCCESS)
    {
        aloge("fatal error! VencChn[%d] set framerate fail[0x%x]!", pStreamContext->mVeChn, ret);
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pStreamContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VENC_RegisterCallback(pStreamContext->mVeChn, &cbInfo);

    VENC_IspVeLinkAttr stIspVeLinkAttr;
    memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
    stIspVeLinkAttr.bEnableIsp2Ve = pStreamContext->bVencIsp2VeEnable;
    stIspVeLinkAttr.bEnableVe2Isp = pStreamContext->bVencVe2IspEnable;
    stIspVeLinkAttr.nVipp = pStreamContext->mVIDev;
    AW_MPI_VENC_EnableIspVeLink(pStreamContext->mVeChn, &stIspVeLinkAttr);
    alogd("VencChn[%d] ispVeLink:%d-%d-%d", pStreamContext->mVeChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
        stIspVeLinkAttr.nVipp);

    if (stIspVeLinkAttr.bEnableVe2Isp == TRUE)
    {
        setRegionDetectLink(pStreamContext->mVeChn, pStreamContext->mRegionLinkEnable,
            pStreamContext->mRegionLinkTexDetectEnable, pStreamContext->mRegionLinkMotionDetectEnable,
            pStreamContext->mRegionLinkMotionDetectInv);
    }

    alogd("create venc chn[%d] success!", pStreamContext->mVeChn);
    return result;
}

static int CreateAiChn(StreamContext *pStreamContext)
{
    AW_MPI_AI_SetPubAttr(pStreamContext->mAIDevId, &pStreamContext->mAiChnAttr);

    AI_CHN_ATTR_S ai_chn_attr;
    memset(&ai_chn_attr, 0, sizeof(ai_chn_attr));
    ai_chn_attr.nFrameSize = 1024;
    AW_MPI_AI_CreateChn(pStreamContext->mAIDevId, pStreamContext->mAiChn, &ai_chn_attr);

    return 0;
}

static int CreateAencChn(StreamContext *pStreamContext)
{
    int  ret = 0;
    ret = AW_MPI_AENC_CreateChn(pStreamContext->mAencChn, &pStreamContext->mAencChnAttr);
    if (0 != ret)
    {
        aloge("fatal error! create aenc channel[%d] fail!", pStreamContext->mAencChn);
        return -1;
    }

    return ret;
}

static int createMuxChn(StreamContext *pRecorderContext, int fd)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    if (fd < 0)
    {
        aloge("fatal error! invalid fd %d", fd);
        return -1;
    }

    ret = AW_MPI_MUX_CreateChn(pRecorderContext->mMuxChn, &pRecorderContext->mMuxChnAttr, fd, 0);
    if (SUCCESS != ret)
    {
        alogd("create muxChn[%d] fail! ret[%d]", pRecorderContext->mMuxChn, ret);
        return -1;
    }

    alogd("create mux chn[%d] file format[%d]", pRecorderContext->mMuxChn, pRecorderContext->mMuxChnAttr.mMediaFileFormat);
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pRecorderContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_MUX_RegisterCallback(pRecorderContext->mMuxChn, &cbInfo);

    RecordFileDurationPolicy mFileDurationPolicy;
    mFileDurationPolicy = RecordFileDurationPolicy_AverageDuration;
    ret = AW_MPI_MUX_SetSwitchFileDurationPolicy(pRecorderContext->mMuxChn, mFileDurationPolicy);
    if(ret != SUCCESS)
    {
        aloge("fatal error! RecorderId[%d] set switchFileDuration Policy fail[0x%x]", pRecorderContext->mMuxChn, ret);
    }

    return result;
}

static int prepareRecorder(SampleCDRDemoContext *pContext)
{
    SampleCDRDemoConfig *pConfigPara = &pContext->mConfigPara;
    StreamContext *pRecorderContext = &pContext->mRecorderContext;
    AudioContext *pAudioContext = &pContext->mAudioContext;
    ERRORTYPE ret = SUCCESS;

    pRecorderContext->priv = (void *)pContext;
    configRecorder(pContext);

    if (pRecorderContext->mVIDev < 0)
    {
        alogd("recorder vi dev[%d], not open recorder");
        return 0;
    }

    if (0 != createVipp(pRecorderContext))
    {
        aloge("fatal eorror! create vipp fail!");
        return -1;
    }

    if (createVencChn(pRecorderContext) != 0)
    {
        aloge("fatal error! create venc fail!");
        return -1;
    }

    if (CreateAiChn(pRecorderContext) != 0)
    {
        aloge("fatal error! create ai fail!");
        return -1;
    }

    if (CreateAencChn(pRecorderContext) != 0)
    {
        aloge("fatal error! create aenc fail!");
        return -1;
    }

    int nFd = generateNextFileFd(pConfigPara);
    if (createMuxChn(pRecorderContext, nFd) != 0)
    {
        aloge("fatal error! create mux chn fail!");
        return -1;
    }

    if (nFd >= 0)
    {
        close(nFd);
    }

    //set spspps
    if (pRecorderContext->mVeChnAttr.VeAttr.Type == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        AW_MPI_VENC_GetH264SpsPpsInfo(pRecorderContext->mVeChn, &H264SpsPpsInfo);
        AW_MPI_MUX_SetH264SpsPpsInfo(pRecorderContext->mMuxChn, pRecorderContext->mVeChn, &H264SpsPpsInfo);
    }
    else if(pRecorderContext->mVeChnAttr.VeAttr.Type == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        AW_MPI_VENC_GetH265SpsPpsInfo(pRecorderContext->mVeChn, &H265SpsPpsInfo);
        AW_MPI_MUX_SetH265SpsPpsInfo(pRecorderContext->mMuxChn, pRecorderContext->mVeChn, &H265SpsPpsInfo);
    }
    else
    {
        alogd("don't need set spspps for encodeType[%d]", pRecorderContext->mVeChnAttr.VeAttr.Type);
    }

    FilePathNode *pNode = (FilePathNode *)malloc(sizeof(FilePathNode));
    memset(pNode, 0, sizeof(FilePathNode));
    strncpy(pNode->strFilePath, pConfigPara->mRecorderCurFileName, MAX_FILE_PATH_SIZE - 1);
    pthread_mutex_lock(&pRecorderContext->mFilePathListLock);
    list_add_tail(&pNode->mList, &pRecorderContext->mFilePathList);
    pthread_mutex_unlock(&pRecorderContext->mFilePathListLock);

    MPP_CHN_S ViChn = {MOD_ID_VIU, pRecorderContext->mVIDev, pRecorderContext->mVIChn};
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pRecorderContext->mVeChn};
    ret = AW_MPI_SYS_Bind(&ViChn, &VeChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! bind vi[%d-%d] ve chn[%d] fail! ret[0x%x]", \
            pRecorderContext->mVIDev, pRecorderContext->mVIChn, pRecorderContext->mVeChn, ret);
        return -1;
    }

    MPP_CHN_S AiChn = {MOD_ID_AI, pRecorderContext->mAIDevId, pRecorderContext->mAiChn};
    MPP_CHN_S AencChn = {MOD_ID_AENC, 0, pRecorderContext->mAencChn};
    ret = AW_MPI_SYS_Bind(&AiChn, &AencChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! bind ai[%d-%d] aenc chn[%d] fail! ret[0x%x]", \
            pRecorderContext->mAIDevId, pRecorderContext->mAiChn, pRecorderContext->mAencChn, ret);
        return -1;
    }

    MPP_CHN_S MuxChn = {MOD_ID_MUX, 0, pRecorderContext->mMuxChn};
    ret = AW_MPI_SYS_Bind(&VeChn, &MuxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! bind ve chn[%d] mux chn[%d] fail! ret[0x%x]", pRecorderContext->mVeChn, pRecorderContext->mMuxChn, ret);
        return -1;
    }

    ret = AW_MPI_SYS_Bind(&AencChn, &MuxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! bind ve chn[%d] mux chn[%d] fail! ret[0x%x]", pRecorderContext->mAencChn, pRecorderContext->mMuxChn, ret);
        return -1;
    }

    return ret;
}

static int preparePreview(SampleCDRDemoContext *pContext)
{
    SampleCDRDemoConfig *pConfigPara = &pContext->mConfigPara;
    StreamContext *pPreviewContext = &pContext->mPreviewContext;
    ERRORTYPE ret = SUCCESS;

    if (0 == pContext->mConfigPara.mPreviewEnable)
    {
        alogd("preview is not enable!");
        return 0;
    }

    pPreviewContext->priv = (void *)pContext;
    configPreview(pContext);

    if (pPreviewContext->mVIDev < 0)
    {
        alogd("recorder vi dev[%d], not open preview");
        return 0;
    }

    if (0 != createVipp(pPreviewContext))
    {
        aloge("fatal eorror! create vipp fail!");
        return -1;
    }

    if (0 == pConfigPara->mPreviewMode)
    {
        if (createVencChn(pPreviewContext) != 0)
        {
            aloge("fatal error! create venc fail!");
            return -1;
        }

        MPP_CHN_S ViChn = {MOD_ID_VIU, pPreviewContext->mVIDev, pPreviewContext->mVIChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pPreviewContext->mVeChn};
        ret = AW_MPI_SYS_Bind(&ViChn, &VeChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind vi[%d-%d] ve chn[%d] fail!", \
                pPreviewContext->mVIDev, pPreviewContext->mVIChn, pPreviewContext->mVeChn);
            return -1;
        }
    }

    if (1 == pConfigPara->mPreviewMode)
    {
        if (0 != createVODev(pPreviewContext))
        {
            aloge("fatal error! create vo dev fail");
            return -1;
        }

        if (0 != createVOChn(pPreviewContext))
        {
            aloge("fatal error! create vo chn fail");
            return -1;
        }
        MPP_CHN_S VIChn = {MOD_ID_VIU, pPreviewContext->mVIDev, pPreviewContext->mVIChn};
        MPP_CHN_S VOChn = {MOD_ID_VOU, pPreviewContext->mVOLayer, pPreviewContext->mVOChn};
        ret = AW_MPI_SYS_Bind(&VIChn, &VOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind vi [%d-%d] vo[%d-%d] fail!", \
                pPreviewContext->mVIDev, pPreviewContext->mVIChn, pPreviewContext->mVOLayer, pPreviewContext->mVOChn);
            return -1;
        }
    }
    else
    {
        if (pContext->mConfigPara.mPreviewRtspId >= 0)
        {
            RtspServerAttr rtsp_attr;
            memset(&rtsp_attr, 0, sizeof(RtspServerAttr));
            rtsp_attr.net_type = pConfigPara->mPreviewRtspNetType;

            if (PT_H264 == pPreviewContext->mVeChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H264;
            else if (PT_H265 == pPreviewContext->mVeChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H265;
            else
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_LAST;

            rtsp_attr.frame_rate = pPreviewContext->mEncFrmRate;

            ret = rtsp_open(pConfigPara->mPreviewRtspId, &rtsp_attr);
            if (ret)
            {
                aloge("Do rtsp_open fail! ret:%d \n", ret);
                return -1;
            }
        }
    }

    return ret;
}

static int prepare(SampleCDRDemoContext *pContext)
{
    int ret = 0;

    ret = prepareRecorder(pContext);
    if (ret != 0)
    {
        aloge("fatal error! prepare Recorder fail!");
        return -1;
    }

    ret = preparePreview(pContext);
    if (ret != 0)
    {
        aloge("fatal error! prepare Preview fail!");
        return -1;
    }

    if (pContext->mConfigPara.mTakePicture)
    {
        ret = PrepareTakePicture(pContext);
        if (ret != 0)
        {
            aloge("fatal error! prepare take picture fail!");
            return -1;
        }
    }

    return ret;
}

static int startRecorder(SampleCDRDemoContext *pContext)
{
    StreamContext *pRecorderContext = &pContext->mRecorderContext;
    int ret = 0;

    if (pRecorderContext->mVIDev < 0)
        return 0;

    ret = AW_MPI_VI_EnableVirChn(pRecorderContext->mVIDev, pRecorderContext->mVIChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! viChn[%d,%d] enable error[0x%x]!", pRecorderContext->mVIDev, pRecorderContext->mVIChn, ret);
    }

    if (pRecorderContext->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_StartRecvPic(pRecorderContext->mVeChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] start error[0x%x]!", pRecorderContext->mVeChn, ret);
        }
    }

    ret = AW_MPI_AI_EnableChn(pRecorderContext->mAIDevId, pRecorderContext->mAiChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! aiChn[%d,%d] enable error[0x%x]!", pRecorderContext->mAIDevId, pRecorderContext->mAiChn, ret);
    }

    ret = AW_MPI_AENC_StartRecvPcm(pRecorderContext->mAencChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AencChn[%d] start error[0x%x]!", pRecorderContext->mAencChn, ret);
    }

    if (pRecorderContext->mMuxChn >= 0)
    {
        ret = AW_MPI_MUX_StartChn(pRecorderContext->mMuxChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! muxChn[%d] start error[0x%x]!", pRecorderContext->mMuxChn, ret);
        }
    }

    return ret;
}

static int startPreview(SampleCDRDemoContext *pContext)
{
    StreamContext *pPreviewContext = &pContext->mPreviewContext;
    int ret = 0;

    if (0 == pContext->mConfigPara.mPreviewEnable)
    {
        return 0;
    }

    ret = AW_MPI_VI_EnableVirChn(pPreviewContext->mVIDev, pPreviewContext->mVIChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! viChn[%d,%d] enable error[0x%x]!", pPreviewContext->mVIDev, pPreviewContext->mVIChn, ret);
    }

    if (0 == pContext->mConfigPara.mPreviewMode)
    {
        if (pPreviewContext->mVeChn >= 0)
        {
            ret = AW_MPI_VENC_StartRecvPic(pPreviewContext->mVeChn);
            if (ret != SUCCESS)
            {
                aloge("fatal error! veChn[%d] start error[0x%x]!", pPreviewContext->mVeChn, ret);
            }
        }
    }

    if (1 == pContext->mConfigPara.mPreviewMode)
    {
        if (pPreviewContext->mVOLayer >= 0 && pPreviewContext->mVOChn >= 0)
        {
            ret = AW_MPI_VO_StartChn(pPreviewContext->mVOLayer, pPreviewContext->mVOChn);
            if (ret != SUCCESS)
            {
                aloge("fatal error! vo layer[%d] vo chn[%d] start fail!", pPreviewContext->mVOLayer, pPreviewContext->mVOChn);
            }
        }
    }
    else
    {
        ret = pthread_create(&pContext->mPreviewGetStreamThreadId, NULL, previewGetStreamThread, (void *)pContext);
        if (ret != 0)
        {
            aloge("fatal error! pthread create fail[%d]", ret);
        }
    }

    return ret;
}

static int start(SampleCDRDemoContext *pContext)
{
    int ret = 0;

    ret = startRecorder(pContext);
    if (0 != ret)
    {
        aloge("fatal error! start recorder fail");
        return -1;
    }

    ret = startPreview(pContext);
    if (0 != ret)
    {
        aloge("fatal error! start preview fail");
        return -1;
    }

    return ret;
}

static int stopRecorder(SampleCDRDemoContext *pContext)
{
    StreamContext *pRecorderContext = &pContext->mRecorderContext;
    int ret = 0;

    if (pRecorderContext->mVIChn >= 0)
    {
        ret = AW_MPI_VI_DisableVirChn(pRecorderContext->mVIDev, pRecorderContext->mVIChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d]chn[%d] disabled fail[0x%x]", pRecorderContext->mVIDev, pRecorderContext->mVIChn, ret);
        }
    }
    else
    {
        return 0;
    }

    if (pRecorderContext->mVeChn >= 0)
    {
        ret = AW_MPI_VENC_StopRecvPic(pRecorderContext->mVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] stop fail[0x%x]", pRecorderContext->mVeChn, ret);
        }
    }

    ret = AW_MPI_AI_DisableChn(pRecorderContext->mAIDevId, pRecorderContext->mAiChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! ai[%d]chn[%d] disabled fail[0x%x]", pRecorderContext->mAIDevId, pRecorderContext->mAiChn, ret);
    }

    ret = AW_MPI_AENC_StopRecvPcm(pRecorderContext->mAencChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! AencChn[%d] stop fail[0x%x]", pRecorderContext->mAencChn, ret);
    }

    if (pRecorderContext->mMuxChn >= 0)
    {
        ret = AW_MPI_MUX_StopChn(pRecorderContext->mMuxChn, FALSE);
        if(ret != SUCCESS)
        {
            aloge("fatal error! muxChn[%d] stop fail[0x%x]", pRecorderContext->mMuxChn, ret);
        }
    }

    return ret;
}

static int stopPreview(SampleCDRDemoContext *pContext)
{
    StreamContext *pPreviewContext = &pContext->mPreviewContext;
    int ret = 0;

    if (0 == pContext->mConfigPara.mPreviewEnable)
    {
        return 0;
    }

    if (pPreviewContext->mVIChn >= 0)
    {
        ret = AW_MPI_VI_DisableVirChn(pPreviewContext->mVIDev, pPreviewContext->mVIChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d]chn[%d] disabled fail[0x%x]", pPreviewContext->mVIDev, pPreviewContext->mVIChn, ret);
        }
    }

    if (0 == pContext->mConfigPara.mPreviewMode)
    {
        if (pPreviewContext->mVeChn >= 0)
        {
            ret = AW_MPI_VENC_StopRecvPic(pPreviewContext->mVeChn);
            if(ret != SUCCESS)
            {
                aloge("fatal error! veChn[%d] stop fail[0x%x]", pPreviewContext->mVeChn, ret);
            }
        }
    }

    if (1 == pContext->mConfigPara.mPreviewMode)
    {
        if (pPreviewContext->mVOLayer >= 0 && pPreviewContext->mVOChn >= 0)
        {
            ret = AW_MPI_VO_StopChn(pPreviewContext->mVOLayer, pPreviewContext->mVOChn);
            if (ret != SUCCESS)
            {
                aloge("fatal error! vo layer[%d] vo chn[%d] stop fail!", pPreviewContext->mVOLayer, pPreviewContext->mVOChn);
            }
        }
    }
    else
    {
        if (pContext->mPreviewGetStreamThreadId > 0)
        {
            pthread_join(pContext->mPreviewGetStreamThreadId , (void*)&ret);
            alogd("PreviewGetStreamThreadId ret val=%p", ret);
        }
    }

    return ret;
}

static int stop(SampleCDRDemoContext *pContext)
{
    int ret = 0;

    //v821 online encode must stop bk1 then stop bk0, because bk1 use frame counter of bk0.
    ret = stopPreview(pContext);
    if (0 != ret)
    {
        aloge("fatal error! stop recorder fail");
    }

    ret = stopRecorder(pContext);
    if (0 != ret)
    {
        aloge("fatal error! stop recorder fail");
    }

    return 0;
}

static int destroyRecorder(SampleCDRDemoContext *pContext)
{
    StreamContext *pRecorderContext = &pContext->mRecorderContext;
    int ret = 0;

    if (pRecorderContext->mVIChn < 0)
        return 0;

    ret = AW_MPI_MUX_DestroyChn(pRecorderContext->mMuxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! muxChn[%d] destroy fail[0x%x]", pRecorderContext->mMuxChn, ret);
    }
    pRecorderContext->mMuxChn = MM_INVALID_CHN;

    ret = AW_MPI_VENC_ResetChn(pRecorderContext->mVeChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! veChn[%d] stop fail[0x%x]", pRecorderContext->mVeChn, ret);
    }
    ret = AW_MPI_VENC_DestroyChn(pRecorderContext->mVeChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! veChn[%d] destroy fail[0x%x]", pRecorderContext->mVeChn, ret);
    }
    pRecorderContext->mVeChn = MM_INVALID_CHN;

    ret = AW_MPI_AENC_DestroyChn(pRecorderContext->mAencChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! AencChn[%d] destroy fail[0x%x]", pRecorderContext->mAencChn, ret);
    }
    pRecorderContext->mAencChn = MM_INVALID_CHN;


    ret = AW_MPI_VI_DestroyVirChn(pRecorderContext->mVIDev, pRecorderContext->mVIChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vipp[%d]Chn[%d] stop fail[0x%x]", pRecorderContext->mVIDev, pRecorderContext->mVIChn, ret);
    }
    pRecorderContext->mVeChn = MM_INVALID_CHN;

    if (pRecorderContext->mVIAttr.large_dma_merge_en >= DMA_STITCH_HORIZONTAL) {
        alogd("It will close video%d when stitch_mode >= DMA_STITCH_HORIZONTAL\n", pRecorderContext->mVIDev - 1);
        AW_MPI_VI_DisableVipp(pRecorderContext->mVIDev - 1);
    }
    ret = AW_MPI_VI_DisableVipp(pRecorderContext->mVIDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vi dev[%d] disable fail!", pRecorderContext->mVIDev);
    }

    ret = AW_MPI_ISP_Stop(pRecorderContext->mIspDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! isp[%d] stop fail!", pRecorderContext->mIspDev);
    }

    if (pRecorderContext->mVIAttr.large_dma_merge_en > DMA_STITCH_NONE)
        AW_MPI_ISP_Stop(pRecorderContext->mViStitchIspChannelId);

    ret = AW_MPI_VI_DestroyVipp(pRecorderContext->mVIDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vi dev[%d] destroy fail!", pRecorderContext->mVIDev);
    }

    ret = AW_MPI_AI_DestroyChn(pRecorderContext->mAIDevId, pRecorderContext->mAiChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! Ai[%d]Chn[%d] stop fail[0x%x]", pRecorderContext->mAIDevId, pRecorderContext->mAiChn, ret);
    }
    pRecorderContext->mAiChn = MM_INVALID_CHN;

    return ret;
}

static int destroyPreview(SampleCDRDemoContext *pContext)
{
    StreamContext *pPreviewContext = &pContext->mPreviewContext;
    int ret = 0;

    if (0 == pContext->mConfigPara.mPreviewEnable)
    {
        return 0;
    }

    if (0 == pContext->mConfigPara.mPreviewMode)
    {
        if (pPreviewContext->mVeChn >= 0)
        {
            ret = AW_MPI_VENC_ResetChn(pPreviewContext->mVeChn);
            if(ret != SUCCESS)
            {
                aloge("fatal error! veChn[%d] stop fail[0x%x]", pPreviewContext->mVeChn, ret);
            }
            ret = AW_MPI_VENC_DestroyChn(pPreviewContext->mVeChn);
            if(ret != SUCCESS)
            {
                aloge("fatal error! veChn[%d] destroy fail[0x%x]", pPreviewContext->mVeChn, ret);
            }
            pPreviewContext->mVeChn = MM_INVALID_CHN;
        }
    }
    if (1 == pContext->mConfigPara.mPreviewMode)
    {
        if (pPreviewContext->mVOLayer >= 0 && pPreviewContext->mVOChn >= 0)
        {
            ret = AW_MPI_VO_DestroyChn(pPreviewContext->mVOLayer, pPreviewContext->mVOChn);
            if (ret != SUCCESS)
            {
                aloge("fatal error! vo layer[%d] vo chn[%d] destroy fail!", pPreviewContext->mVOLayer, pPreviewContext->mVOChn);
            }
            ret = AW_MPI_VO_DisableVideoLayer(pPreviewContext->mVOLayer);
            if (ret != SUCCESS)
            {
                aloge("fatal error! vo layer[%d] disable fail!", pPreviewContext->mVOLayer);
            }
            pPreviewContext->mVOChn = MM_INVALID_CHN;
            pPreviewContext->mVOLayer = MM_INVALID_LAYER;
        }
        if (pPreviewContext->mVODev >= 0)
        {
            //AW_MPI_VO_CloseVideoLayer(pPreviewContext->mUILayer);
            //AW_MPI_VO_RemoveOutsideVideoLayer(pPreviewContext->mUILayer);
            AW_MPI_VO_Disable(pPreviewContext->mVODev);
            //pPreviewContext->mUILayer = MM_INVALID_LAYER;
            pPreviewContext->mVODev = MM_INVALID_DEV;
        }
    }
    else
    {
        if (pContext->mConfigPara.mPreviewRtspId >= 0)
        {
            rtsp_stop(pContext->mConfigPara.mPreviewRtspId);
            rtsp_close(pContext->mConfigPara.mPreviewRtspId);
        }
    }
    if (pPreviewContext->mVIChn >= 0)
    {
        ret = AW_MPI_VI_DestroyVirChn(pPreviewContext->mVIDev, pPreviewContext->mVIChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d]Chn[%d] stop fail[0x%x]", pPreviewContext->mVIDev, pPreviewContext->mVIChn, ret);
        }
        pPreviewContext->mVIChn = MM_INVALID_CHN;
    }
    if (pPreviewContext->mVIAttr.large_dma_merge_en >= DMA_STITCH_HORIZONTAL) {
        alogd("It will close video%d when stitch_mode >= DMA_STITCH_HORIZONTAL\n", pPreviewContext->mVIDev - 1);
        AW_MPI_VI_DisableVipp(pPreviewContext->mVIDev - 1);
    }
    if (pPreviewContext->mVIDev >= 0)
    {
        ret = AW_MPI_VI_DisableVipp(pPreviewContext->mVIDev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vi dev[%d] disable fail!", pPreviewContext->mVIDev);
        }
    }
    if (pPreviewContext->mIspDev >= 0)
    {
        ret = AW_MPI_ISP_Stop(pPreviewContext->mIspDev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! isp[%d] stop fail!", pPreviewContext->mIspDev);
        }
    }
    if (pPreviewContext->mViStitchIspChannelId >= 0)
    {
        if (pPreviewContext->mVIAttr.large_dma_merge_en > DMA_STITCH_NONE)
            AW_MPI_ISP_Stop(pPreviewContext->mViStitchIspChannelId);
    }
    if (pPreviewContext->mVIDev >= 0)
    {
        ret = AW_MPI_VI_DestroyVipp(pPreviewContext->mVIDev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vi dev[%d] destroy fail!", pPreviewContext->mVIDev);
        }
    }

    return ret;
}

static int destroy(SampleCDRDemoContext *pContext)
{
    int ret = 0;

    if (pContext->mConfigPara.mTakePicture)
    {
        ret = destroyJpegPicture(pContext);
        if (0 != ret)
        {
            aloge("fatal error! destroy take picture fail");
        }
    }

    ret = destroyRecorder(pContext);
    if (0 != ret)
    {
        aloge("fatal error! destroy recorder fail");
    }

    ret = destroyPreview(pContext);
    if (0 != ret)
    {
        aloge("fatal error! destroy preview fail");
    }

    return ret;
}

void *MsgQueueThread(void *pThreadData)
{
    SampleCDRDemoContext *pContext = (SampleCDRDemoContext *)pThreadData;
    message_t stCmdMsg;
    SampleCDRDemoMsgType cmd;
    int nCmdPara;
    char strThreadName[32];
    sprintf(strThreadName, "MsgQueueThread");
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    alogd("message queue thread start.");
    while (!pContext->mbExitFlag)
    {
        if (0 == get_message(&pContext->mMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case Rec_NeedSetNextFd:
                {
                    SampleCDRDemo_MessageData *pMsgData = (SampleCDRDemo_MessageData *)stCmdMsg.mpData;
                    StreamContext *pStreamContext = pMsgData->pStreamContext;
                    int muxChn = nCmdPara;
                    int ret;

                    ret = setNextFileToMuxer(pContext, 0, muxChn);
                    if (0 == ret)
                    {
                        FilePathNode *pNode = (FilePathNode *)malloc(sizeof(FilePathNode));
                        if (pNode)
                        {
                            memset(pNode, 0, sizeof(FilePathNode));
                            strncpy(pNode->strFilePath, pContext->mConfigPara.mRecorderCurFileName, MAX_FILE_PATH_SIZE - 1);
                            pthread_mutex_lock(&pStreamContext->mFilePathListLock);
                            list_add_tail(&pNode->mList, &pStreamContext->mFilePathList);
                            pthread_mutex_unlock(&pStreamContext->mFilePathListLock);
                        }
                        else
                            aloge("fatal error! pNode malloc fail!");
                    }

                    //free message mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Rec_FileDone:
                {
                    SampleCDRDemo_MessageData *pMsgData = (SampleCDRDemo_MessageData *)stCmdMsg.mpData;
                    StreamContext *pStreamContext = pMsgData->pStreamContext;
                    //int muxerId = nCmdPara;
                    int ret;

                    pthread_mutex_lock(&pStreamContext->mFilePathListLock);
                    int cnt = 0;
                    struct list_head *pList;
                    list_for_each(pList, &pStreamContext->mFilePathList)
                    {
                        cnt++;
                    }
                    while (cnt > pContext->mConfigPara.mRecorderRecFileCnt && pContext->mConfigPara.mRecorderRecFileCnt > 0)
                    {
                        FilePathNode *pNode = list_first_entry(&pStreamContext->mFilePathList, FilePathNode, mList);
                        list_del(&pNode->mList);
                        cnt--;
                        if ((ret = remove(pNode->strFilePath)) != 0)
                        {
                            aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
                        }
                        else
                        {
                            alogd("delete file[%s] success", pNode->strFilePath);
                        }
                        free(pNode);
                    }
                    pthread_mutex_unlock(&pStreamContext->mFilePathListLock);
                    //free message mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case MsgQueue_Stop:
                {
                    goto _Exit;
                    break;
                }
                default :
                {
                    break;
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pContext->mMsgQueue, 0);
        }
    }

_Exit:
    alogd("message queue thread stop.");
    return NULL;
}

int main(int argc, char **argv)
{
    int result = 0;
    ERRORTYPE eRet = SUCCESS;
//    void *pValue = NULL;
    message_t stCmdMsg;
    GLogConfig stGLogConfig =
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
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

    SampleCDRDemoContext *pContext = constructSampleCDRDemoContext();
    if (NULL == pContext)
    {
        result = -1;
        goto _exit;
    }
    gpSampleCDRDemoContext = pContext;

    char *pConfigFilePath;
    if (ParseCmdLine(pContext, argc, argv) != 0)
    {
        result = -1;
        goto _destroy_context;
    }
    if (strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }
    /* parse config file. */
    if (loadSampleCDRConfig(pContext, pConfigFilePath) != 0)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _destroy_context;
    }

    // create msg queue
    if (message_create(&pContext->mMsgQueue) < 0)
    {
        aloge("fatal error! create message queue fail!");
        goto _destroy_context;
    }

    /* register process function for SIGINT, to exit program. */
    if (SIG_ERR == signal(SIGINT, handle_exit))
    {
        aloge("fatal error! can't catch SIGSEGV");
    }

    initRecorder(&pContext->mRecorderContext);
    initPicFilePathList(pContext);

    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    eRet = AW_MPI_SYS_Init();
    if (eRet < 0)
    {
        aloge("sys Init failed!");
        result = -1;
        goto _destroy_msgque;
    }

    //create message queue thread
    result = pthread_create(&pContext->mMsgQueueThreadId, NULL, MsgQueueThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create message queue thread fail[%d]!", result);
        goto _destroy_msgque;
    }
    else
    {
        alogd("create message queue success threadId[0x%x].", &pContext->mMsgQueueThreadId);
    }

    result = prepare(pContext);
    if (0 != result)
    {
        aloge("fatal errpr! cdr recorder prepare fail!");
        goto _stop_msgque;
    }

    result = start(pContext);
    if (0 != result)
    {
        aloge("fatal errpr! cdr recorder prepare fail!");
        goto _destroy_recorder;
    }

    //wait exit.
    if (pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration * 1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    pContext->mbExitFlag = 1;

_stop_recorder:
    //stop and release record.
    result = stop(pContext);
    if (0 != result)
    {
        aloge("fatal errpr! cdr recorder prepare fail!");
    }
_destroy_recorder:
    result = destroy(pContext);
    if (0 != result)
    {
        aloge("fatal errpr! cdr recorder destroy fail!");
    }
_stop_msgque:
    memset(&stCmdMsg, 0, sizeof(message_t));
    stCmdMsg.command = MsgQueue_Stop;
    put_message(&pContext->mMsgQueue, &stCmdMsg);
    int ret;
    pthread_join(pContext->mMsgQueueThreadId, (void*)&ret);
_mpp_exit:
    AW_MPI_SYS_Exit();
_destroy_msgque:
    destroyPicFilePathList(pContext);
    deinitRecorder(&pContext->mRecorderContext);
    message_destroy(&pContext->mMsgQueue);
_destroy_context:
    cdx_sem_deinit(&pContext->mSemExit);
    if (pContext != NULL)
    {
        free(pContext);
        pContext = NULL;
        gpSampleCDRDemoContext = NULL;
    }
_exit:
    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
