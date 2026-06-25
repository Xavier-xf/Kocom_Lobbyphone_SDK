//#define LOG_NDEBUG 0
#define LOG_TAG "sample_CodecParallel"
#include <utils/plat_log.h>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
//#include <sys/ioctl.h>

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"

#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <confparser.h>

#include "mpi_isp.h"
#include <cdx_list.h>

#include "sample_CodecParallel_config.h"
#include "sample_CodecParallel.h"
#include "../common/sample_common_venc.h"

/* for DEBUG */
#define SAMPLE_RECORD_TEST
#define SAMPLE_RECORD_SUB_TEST
#define SAMPLE_PREVIEW_TEST
#define SAMPLE_PLAY_TEST

/* used to control isp run */
#define ISP_RUN
#define ISP_DEV_ID  (0)

#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)
#define VE_VBV_CACHE_TIME                   (4)  // unit:seconds, exp:1,2,3,4...

static SampleCodecParallelContext *gpCodecParallelContext = NULL;

static int parseCmdLine(SampleCodecParallelContext *pContext, int argc, char** argv)
{
    int ret = -1;

    alogd("argc=%d", argc);

    if (argc != 3)
    {
        printf("CmdLine param:\n"
                "\t-path ./sample_CodecParallel.conf\n");
        return -1;
    }

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
                ret = 0;
                if (strlen(*argv) >= MAX_FILE_PATH_SIZE)
                {
                    aloge("fatal error! file path[%s] too long:!", *argv);
                }

                if (pContext)
                {
                    strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_SIZE-1);
                    pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
                }
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path ./sample_CodecParallel.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static PIXEL_FORMAT_E getPicFormatFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PIXEL_FORMAT_E PicFormat = MM_PIXEL_FORMAT_BUTT;
    char *pStrPixelFormat = (char*)GetConfParaString(pConfParser, key, NULL);

    if (!strcmp(pStrPixelFormat, "yu12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "yv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv21"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv21s"))
    {
        PicFormat = MM_PIXEL_FORMAT_AW_NV21S;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_2_0x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_2_5x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_1_5x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_1_0x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }

    return PicFormat;
}

static PAYLOAD_TYPE_E getEncoderTypeFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PAYLOAD_TYPE_E EncType = PT_BUTT;
    char *ptr = (char *)GetConfParaString(pConfParser, key, NULL);
    if(!strcmp(ptr, "H.264"))
    {
        EncType = PT_H264;
    }
    else if(!strcmp(ptr, "H.265"))
    {
        EncType = PT_H265;
    }
    else if(!strcmp(ptr, "MJPEG"))
    {
        EncType = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
        EncType = PT_H264;
    }

    return EncType;
}

static void convertStrDispType(char *pStrDispType, int DisplayWidth, VO_INTF_TYPE_E *pDispType, VO_INTF_SYNC_E *pDispSync)
{
    if (!strcmp(pStrDispType, "hdmi"))
    {
        *pDispType = VO_INTF_HDMI;
        if (DisplayWidth > 1920)
            *pDispSync = VO_OUTPUT_3840x2160_30;
        else if (DisplayWidth > 1280)
            *pDispSync = VO_OUTPUT_1080P30;
        else
            *pDispSync = VO_OUTPUT_720P60;
    }
    else if (!strcmp(pStrDispType, "lcd"))
    {
        *pDispType = VO_INTF_LCD;
        *pDispSync = VO_OUTPUT_NTSC;
    }
    else if (!strcmp(pStrDispType, "cvbs"))
    {
        *pDispType = VO_INTF_CVBS;
        *pDispSync = VO_OUTPUT_NTSC;
    }
}

static PIXEL_FORMAT_E getVideoFileFormatFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    MEDIA_FILE_FORMAT_E VideoFileFormat = MEDIA_FILE_FORMAT_UNKNOWN;
    char *pStrPixelFormat = (char*)GetConfParaString(pConfParser, key, NULL);

    if (!strcmp(pStrPixelFormat, "mp4"))
    {
        VideoFileFormat = MEDIA_FILE_FORMAT_MP4;
    }
    else if (!strcmp(pStrPixelFormat, "ts"))
    {
        VideoFileFormat = MEDIA_FILE_FORMAT_TS;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        VideoFileFormat = MEDIA_FILE_FORMAT_DEFAULT;
    }

    return VideoFileFormat;
}

static void loadRecordConfig(RecordContext *pRec, CONFPARSER_S *pConfParser)
{
    pRec->mEnable = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_ENABLE, 0);

    /* vi */
    pRec->mViCfg.mVippID = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIPP_ID, 0);
    pRec->mViCfg.mOnlineEnable = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_ONLINE_EN, 0);
    pRec->mViCfg.mOnlineShareBufNum = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_ONLINE_SHARE_BUF_NUM, 0);
    pRec->mViCfg.mVippBufNum = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIPP_BUF_NUM, 0);
    pRec->mViCfg.mPicFormat = getPicFormatFromConfig(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIDEO_PIXFORMAT);
    pRec->mViCfg.mCaptureWidth = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIPP_CAPTURE_WIDTH, 0);
    pRec->mViCfg.mCaptureHeight = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIPP_CAPTURE_HEIGHT, 0);
    pRec->mViCfg.mFrameRate = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIPP_FRAME_RATE, 0);

    /* venc */
    pRec->mVencCfg.mVencChn = 0;
    pRec->mVencCfg.mOnlineEnable = pRec->mViCfg.mOnlineEnable;
    pRec->mVencCfg.mOnlineShareBufNum = pRec->mViCfg.mOnlineShareBufNum;
    pRec->mVencCfg.mVideoFrameRate = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIDEO_FRAME_RATE, 0);
    pRec->mVencCfg.mVideoBitrate = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIDEO_BITRATE, 0);
    pRec->mVencCfg.mVideoWidth = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIDEO_WIDTH, 0);
    pRec->mVencCfg.mVideoHeight = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIDEO_HEIGHT, 0);
    pRec->mVencCfg.mVideoEncoderType = getEncoderTypeFromConfig(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIDEO_ENCODER_TYPE);
    pRec->mVencCfg.mVideoRcMode = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIDEO_RATE_CTRL_MODE, 0);

    /* mux */
    pRec->mMuxCfg.mMuxChn = 0;
    pRec->mMuxCfg.mVideoFileFormat = getVideoFileFormatFromConfig(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_VIDEO_FILE_FORMAT);
    pRec->mMuxCfg.mVideoDuration = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_FILE_DURATION, 0);
    char *ptr = (char *)GetConfParaString(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_DEST_FILE_STR, NULL);
    strncpy(pRec->mMuxCfg.mDestVideoFile, ptr, MAX_FILE_PATH_SIZE);
    pRec->mMuxCfg.nMaxFileNum = 3;
}

static void loadSubRecordConfig(RecordContext *pRec, CONFPARSER_S *pConfParser)
{
    pRec->mEnable = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_ENABLE, 0);

    /* vi */
    pRec->mViCfg.mVippID = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIPP_ID, 0);
    pRec->mViCfg.mVippBufNum = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIPP_BUF_NUM, 0);
    pRec->mViCfg.mPicFormat = getPicFormatFromConfig(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIDEO_PIXFORMAT);
    pRec->mViCfg.mCaptureWidth = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIPP_CAPTURE_WIDTH, 0);
    pRec->mViCfg.mCaptureHeight = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIPP_CAPTURE_HEIGHT, 0);
    pRec->mViCfg.mFrameRate = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIPP_FRAME_RATE, 0);

    /* venc */
    pRec->mVencCfg.mVencChn = 1;
    pRec->mVencCfg.mVideoFrameRate = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIDEO_FRAME_RATE, 0);
    pRec->mVencCfg.mVideoBitrate = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIDEO_BITRATE, 0);
    pRec->mVencCfg.mVideoWidth = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIDEO_WIDTH, 0);
    pRec->mVencCfg.mVideoHeight = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIDEO_HEIGHT, 0);
    pRec->mVencCfg.mVideoEncoderType = getEncoderTypeFromConfig(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIDEO_ENCODER_TYPE);
    pRec->mVencCfg.mVideoRcMode = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIDEO_RATE_CTRL_MODE, 0);

    /* mux */
    pRec->mMuxCfg.mMuxChn = 1;
    pRec->mMuxCfg.mVideoFileFormat = getVideoFileFormatFromConfig(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_VIDEO_FILE_FORMAT);
    pRec->mMuxCfg.mVideoDuration = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_FILE_DURATION, 0);
    char *ptr = (char *)GetConfParaString(pConfParser, SAMPLE_CODECPARALLEL_KEY_RECORD_SUB_DEST_FILE_STR, NULL);
    strncpy(pRec->mMuxCfg.mDestVideoFile, ptr, MAX_FILE_PATH_SIZE);
    pRec->mMuxCfg.nMaxFileNum = 3;
}

static void loadPreviewConfig(PreviewContext *pPrew, CONFPARSER_S *pConfParser)
{
    pPrew->mEnable = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_ENABLE, 0);

    /* vi */
    pPrew->mViCfg.mVippID = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VIPP_ID, 0);
    pPrew->mViCfg.mPicFormat = getPicFormatFromConfig(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VIPP_FORMAT);
    pPrew->mViCfg.mCaptureWidth = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VIPP_CAPTURE_WIDTH, 0);
    pPrew->mViCfg.mCaptureHeight = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VIPP_CAPTURE_HEIGHT, 0);
    pPrew->mViCfg.mFrameRate = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VIPP_FRAME_RATE, 0);

    /* vo */
    pPrew->mVoCfg.mVoDev = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VO_DEV_ID, 0);
    pPrew->mVoCfg.mDisplayX = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VO_DISPLAY_X, 0);
    pPrew->mVoCfg.mDisplayY = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VO_DISPLAY_Y, 0);
    pPrew->mVoCfg.mDisplayWidth = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VO_DISPLAY_WIDTH, 0);
    pPrew->mVoCfg.mDisplayHeight = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VO_DISPLAY_HEIGHT, 0);
    pPrew->mVoCfg.mVoLayer = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VO_LAYER_NUM, 0);
    pPrew->mVoCfg.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    /* Note: Because of dependencies, pStrDispType must be obtained after mPrewVoDisplayWidth */
    char *pStrDispType = (char*)GetConfParaString(pConfParser, SAMPLE_CODECPARALLEL_KEY_PREVIEW_VO_DISP_TYPE, NULL);
    convertStrDispType(pStrDispType, pPrew->mVoCfg.mDisplayWidth, &pPrew->mVoCfg.mDispType, &pPrew->mVoCfg.mDispSync);
}

static void loadPlayConfig(PlayContext *pPlay, CONFPARSER_S *pConfParser)
{
    pPlay->mEnable = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_ENABLE, 0);

    /* demux */
    char *ptr = (char *)GetConfParaString(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_SRC_FILE_STR, NULL);
    strncpy(pPlay->mDmxCfg.mSrcVideoFile, ptr, MAX_FILE_PATH_SIZE);

    /* vo */
    pPlay->mVoCfg.mVoDev = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_VO_DEV_ID, 0);
    pPlay->mVoCfg.mDisplayX = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_VO_DISPLAY_X, 0);
    pPlay->mVoCfg.mDisplayY = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_VO_DISPLAY_Y, 0);
    pPlay->mVoCfg.mDisplayWidth = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_VO_DISPLAY_WIDTH, 0);
    pPlay->mVoCfg.mDisplayHeight = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_VO_DISPLAY_HEIGHT, 0);
    pPlay->mVoCfg.mVoLayer = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_VO_LAYER_NUM, 0);
    pPlay->mVoCfg.mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    /* Note: Because of dependencies, pStrDispType must be obtained after mPrewVoDisplayWidth */
    char *pStrDispType = (char*)GetConfParaString(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_VO_DISP_TYPE, NULL);
    convertStrDispType(pStrDispType, pPlay->mVoCfg.mDisplayWidth, &pPlay->mVoCfg.mDispType, &pPlay->mVoCfg.mDispSync);

    pPlay->mVdecCfg.mVdecBufSize = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_VDEC_BUF_SIZE, 0);

    pPlay->mLoopEnable = GetConfParaInt(pConfParser, SAMPLE_CODECPARALLEL_KEY_PLAY_LOOP_ENABLE, 0);
    alogd("play en:%d loop_en:%d", pPlay->mEnable, pPlay->mLoopEnable);
}

static ERRORTYPE loadConfigPara(SampleCodecParallelContext *pContext, const char *conf_path)
{
    if (NULL == pContext)
    {
        aloge("pContext is NULL!");
        return FAILURE;
    }

    if (NULL == conf_path)
    {
        aloge("user not set config file!");
        return FAILURE;
    }

    CONFPARSER_S stConfParser;
    if(0 > createConfParser(conf_path, &stConfParser))
    {
        aloge("load conf fail!");
        return FAILURE;
    }

    /* common configure */
    pContext->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_CODECPARALLEL_KEY_TEST_DURATION, 0);

    alogd("testDuration[%d]", pContext->mTestDuration);

    /* record configure */
    loadRecordConfig(&pContext->mRec, &stConfParser);

    /* record sub configure */
    loadSubRecordConfig(&pContext->mSubRec, &stConfParser);

    /* preview configure */
    loadPreviewConfig(&pContext->mPrew, &stConfParser);

    /* play configure */
    loadPlayConfig(&pContext->mPlay, &stConfParser);

    destroyConfParser(&stConfParser);

    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = 0;
    SampleCodecParallelContext *pContext = (SampleCodecParallelContext *)cookie;
    if (MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                alogd("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if (MOD_ID_VOU == pChn->mModId)
    {
        alogd("VO callback: VO Layer[%d] chn[%d] event:%d", pChn->mDevId, pChn->mChnId, event);
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                aloge("vo layer[%d] release frame id[0x%x]!", pChn->mDevId, pFrameInfo->mId);
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("vo layer[%d] report video display size[%dx%d]", pChn->mDevId, pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("vo to the end of file");
                if (pContext && pChn->mDevId == pContext->mPlay.mVoPara.mVoLayer)
                {
                    //cdx_sem_up(&pContext->mSemExit);
                    message_t stCmdMsg;
                    InitMessage(&stCmdMsg);
                    stCmdMsg.command = MSGQUEUE_MEDIA_NOTIFY_EOF;
                    putMessageWithData(&pContext->mPlay.mMsgQueue, &stCmdMsg);
                }
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                alogd("vo layer[%d] report rendering start", pChn->mDevId);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if (MOD_ID_VDEC == pChn->mModId)
    {
        switch (event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("vdec to the end of file");
                if ((pContext) && (pContext->mPlay.mVoPara.mVoChn >= 0))
                {
                    AW_MPI_VO_SetStreamEof(pContext->mPlay.mVoPara.mVoLayer, pContext->mPlay.mVoPara.mVoChn, 1);
                }
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if (MOD_ID_DEMUX == pChn->mModId)
    {
        switch (event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("demux to the end of file");
                if ((pContext) && (pContext->mPlay.mVdecPara.mVdecChn >= 0))
                {
                    AW_MPI_VDEC_SetStreamEof(pContext->mPlay.mVdecPara.mVdecChn, 1);
                }
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if (MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                alogd("MuxChn[%d] record file done.", *(int*)pEventData);
                message_queue_t *pDstMsgQueue = NULL;
                if(pChn->mChnId == pContext->mRec.mMuxPara.mMuxChn)
                {
                    pDstMsgQueue = &pContext->mRec.stMsgQueue;
                }
                else if(pChn->mChnId == pContext->mSubRec.mMuxPara.mMuxChn)
                {
                    pDstMsgQueue = &pContext->mSubRec.stMsgQueue;
                }
                else
                {
                    aloge("fatal error! muxChn[%d] wrong!", pChn->mChnId);
                }
                message_t stMsg;
                InitMessage(&stMsg);
                stMsg.command = MSGQUEUE_Rec_FileDone;
                stMsg.para0 = *(int*)pEventData;
                putMessageWithData(pDstMsgQueue, &stMsg);
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                alogd("MuxChn[%d] need next fd.", *(int*)pEventData);
                message_queue_t *pDstMsgQueue = NULL;
                if(pChn->mChnId == pContext->mRec.mMuxPara.mMuxChn)
                {
                    pDstMsgQueue = &pContext->mRec.stMsgQueue;
                }
                else if(pChn->mChnId == pContext->mSubRec.mMuxPara.mMuxChn)
                {
                    pDstMsgQueue = &pContext->mSubRec.stMsgQueue;
                }
                else
                {
                    aloge("fatal error! muxChn[%d] wrong!", pChn->mChnId);
                }
                message_t stMsg;
                InitMessage(&stMsg);
                stMsg.command = MSGQUEUE_Rec_NeedSetNextFd;
                stMsg.para0 = *(int*)pEventData;
                putMessageWithData(pDstMsgQueue, &stMsg);
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        RecordContext *pRec = NULL;
        if (mVEncChn == pContext->mRec.mVencPara.mVencChn)
        {
            pRec = &pContext->mRec;
        }
        else if (mVEncChn == pContext->mSubRec.mVencPara.mVencChn)
        {
            pRec = &pContext->mSubRec;
        }
        else
        {
            aloge("fatal error! invalid VEncChn %d", mVEncChn);
        }
        switch(event)
        {
            /*case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                Isp2VeLinkageParam stIsp2Ve;
                memset(&stIsp2Ve, 0, sizeof(Isp2VeLinkageParam));
                stIsp2Ve.mIspAndVeLinkageEnable = 1;
                stIsp2Ve.mCameraAdaptiveMovingAndStaticEnable = 0;
                stIsp2Ve.mVEncChn = mVEncChn;
                stIsp2Ve.mVipp = pRec->mViCfg.mVippID;
                stIsp2Ve.pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                stIsp2Ve.nEncppSharpAttenCoefPer = 100;
                int ret = setIsp2VeLinkageParam(&stIsp2Ve);
                if (ret)
                {
                    aloge("fatal error, VEncChn[%d] set Isp2VeLinkageParam failed! ret=%d", mVEncChn, ret);
                    return -1;
                }
                break;
            }
            case MPP_EVENT_LINKAGE_VE2ISP_PARAM:
            {
                Ve2IspLinkageParam stVe2Isp;
                memset(&stVe2Isp, 0, sizeof(Ve2IspLinkageParam));
                stVe2Isp.mIspAndVeLinkageEnable = 1;
                stVe2Isp.mVEncChn = mVEncChn;
                stVe2Isp.mVipp = pRec->mViCfg.mVippID;
                stVe2Isp.p2Ve2IspParam = (VencVe2IspParam *)pEventData;
                int ret = setVe2IspLinkageParam(&stVe2Isp);
                if (ret)
                {
                    aloge("fatal error, VEncChn[%d] set Ve2IspLinkageParam failed! ret=%d", mVEncChn, ret);
                    return -1;
                }
                break;
            }*/
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM_EXTRA:
            {
                VENC_Isp2VeExtraParam *pExtraParam = (VENC_Isp2VeExtraParam *)pEventData;
                pExtraParam->eEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
    }

    return SUCCESS;
}

static void configViChn(ViParam *pPara, ViConfig *pCfg, void *priv)
{
    pPara->priv = priv;
    pPara->mpViCfg = pCfg;
}

static int createViChn(ViParam *pViPara, int nVippBufNum)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    pViPara->mIspDev = ISP_DEV_ID;

    /* get vipp id from user config */
    pViPara->mViDev = pViPara->mpViCfg->mVippID;

    ret = AW_MPI_VI_CreateVipp(pViPara->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! create Vipp:%d failed, ret=%d", pViPara->mViDev, ret);
        return -1;
    }
    alogd("create vipp %d success", pViPara->mViDev);

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = pViPara->priv;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    ret = AW_MPI_VI_RegisterCallback(pViPara->mViDev, &cbInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vipp[%d] register callback failed, ret=%d", pViPara->mViDev, ret);
        return -1;
    }

    VI_ATTR_S stAttr;
    memset(&stAttr, 0, sizeof(VI_ATTR_S));
    ret = AW_MPI_VI_GetVippAttr(pViPara->mViDev, &stAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! Get Vipp[%d] Attr failed, ret=%d", pViPara->mViDev, ret);
        return -1;
    }

    stAttr.mOnlineEnable = pViPara->mpViCfg->mOnlineEnable;
    stAttr.mOnlineShareBufNum = pViPara->mpViCfg->mOnlineShareBufNum;
    alogd("vipp%d OnlineEnable:%d, OnlineShareBufNum:%d, VippBufNum:%d", pViPara->mViDev, stAttr.mOnlineEnable, stAttr.mOnlineShareBufNum, nVippBufNum);
    stAttr.mbEncppEnable = TRUE;
    stAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    stAttr.memtype = V4L2_MEMORY_MMAP;
    stAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pViPara->mpViCfg->mPicFormat);
    stAttr.format.field = V4L2_FIELD_NONE;
    stAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    stAttr.format.width  = pViPara->mpViCfg->mCaptureWidth;
    stAttr.format.height = pViPara->mpViCfg->mCaptureHeight;
    stAttr.nbufs = nVippBufNum;
    stAttr.nplanes = 2;
    stAttr.drop_frame_num = 0;
    stAttr.use_current_win = 0;
    stAttr.fps = pViPara->mpViCfg->mFrameRate;
    ret = AW_MPI_VI_SetVippAttr(pViPara->mViDev, &stAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! Set Vipp[%d] Attr failed, ret=%d", pViPara->mViDev, ret);
        return -1;
    }

#ifdef ISP_RUN
    ret = AW_MPI_ISP_Run(pViPara->mIspDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! Set Vipp[%d] Attr failed, ret=%d", pViPara->mViDev, ret);
        return -1;
    }
#endif

    ViVirChnAttrS stVirChnAttr;
    memset(&stVirChnAttr, 0, sizeof(ViVirChnAttrS));
    stVirChnAttr.mbRecvInIdleState = TRUE;

    pViPara->mViChn = 0;

    while(1)
    {
        ret = AW_MPI_VI_CreateVirChn(pViPara->mViDev, pViPara->mViChn, &stVirChnAttr);
        if (SUCCESS == ret)
        {
            alogd("vipp[%d] create VirChn[%d] success", pViPara->mViDev, pViPara->mViChn);
            break;
        }
        else if (ERR_VI_EXIST == ret)
        {
            pViPara->mViChn++;
            continue;
        }
        else
        {
            aloge("fatal error! create VirChn[%d] fail, ret=%d", pViPara->mViChn, ret);
            return -1;
        }
    }
    return 0;
}

static int destroyViChn(ViParam *pViPara)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    ret = AW_MPI_VI_DestroyVirChn(pViPara->mViDev, pViPara->mViChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! mpi vi destroy vir chn fail, ret=%d", ret);
        result = -1;
    }

#ifdef ISP_RUN
    ret = AW_MPI_ISP_Stop(pViPara->mIspDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! mpi isp stop fail, ret=%d", ret);
        result = -1;
    }
#endif

    ret = AW_MPI_VI_DisableVipp(pViPara->mViDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! mpi vi disable vipp fail, ret=%d", ret);
        result = -1;
    }

    ret = AW_MPI_VI_DestroyVipp(pViPara->mViDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! mpi vi destory vipp fail, ret=%d", ret);
        result = -1;
    }

    return result;
}

static void configVoChn(VoParam *pPara, VoConfig *pVoCfg, void *priv)
{
    pPara->priv = priv;
    pPara->mpVoCfg = pVoCfg;
}

static int createVoChn(VoParam *pVoPara)
{
    ERRORTYPE ret = SUCCESS;

    /* get vo dev id from user config */
    pVoPara->mVoDev = pVoPara->mpVoCfg->mVoDev;
    alogd("mVoDev=%d", pVoPara->mVoDev);

    /* Explanation:
     * video layer: layer 0~7, and the attr of 0~3 and 4~7 layers
     *              need to be consistent respectively.
     * UI layer: layer 8~15.
     */
    pVoPara->mUILayer = HLAY(2, 0);

    /* close ui player for display video */
    AW_MPI_VO_Enable(pVoPara->mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(pVoPara->mUILayer);
    AW_MPI_VO_CloseVideoLayer(pVoPara->mUILayer);

    /* Set the background color for the VO display device */
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(pVoPara->mVoDev, &spPubAttr);
    spPubAttr.enIntfType = pVoPara->mpVoCfg->mDispType;
    spPubAttr.enIntfSync = pVoPara->mpVoCfg->mDispSync;
    AW_MPI_VO_SetPubAttr(pVoPara->mVoDev, &spPubAttr);

    // enable vo layer
    pVoPara->mVoLayer = pVoPara->mpVoCfg->mVoLayer;
    while (pVoPara->mVoLayer < VO_MAX_LAYER_NUM)
    {
        if (SUCCESS == AW_MPI_VO_EnableVideoLayer(pVoPara->mVoLayer))
        {
            alogd("mpi Vo enable video Layer %d success", pVoPara->mVoLayer);
            break;
        }
        pVoPara->mVoLayer++;
    }

    if (pVoPara->mVoLayer >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
        pVoPara->mVoLayer = MM_INVALID_DEV;
        AW_MPI_VO_RemoveOutsideVideoLayer(pVoPara->mUILayer);
        AW_MPI_VO_Disable(pVoPara->mVoDev);
        return -1;
    }

    VO_VIDEO_LAYER_ATTR_S VoLayerAttr;
    memset(&VoLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));
    AW_MPI_VO_GetVideoLayerAttr(pVoPara->mVoLayer, &VoLayerAttr);
    VoLayerAttr.stDispRect.X = pVoPara->mpVoCfg->mDisplayX;
    VoLayerAttr.stDispRect.Y = pVoPara->mpVoCfg->mDisplayY;
    VoLayerAttr.stDispRect.Width = pVoPara->mpVoCfg->mDisplayWidth;
    VoLayerAttr.stDispRect.Height = pVoPara->mpVoCfg->mDisplayHeight;
    VoLayerAttr.enPixFormat = pVoPara->mpVoCfg->mPicFormat;
    AW_MPI_VO_SetVideoLayerAttr(pVoPara->mVoLayer, &VoLayerAttr);

    BOOL nSuccessFlag = FALSE;
    pVoPara->mVoChn = 0;

    while (pVoPara->mVoChn < VO_MAX_CHN_NUM)
    {
        ret = AW_MPI_VO_CreateChn(pVoPara->mVoLayer, pVoPara->mVoChn);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create vo channel[%d] success!", pVoPara->mVoChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == ret)
        {
            alogd("vo channel[%d] is exist, find next!", pVoPara->mVoChn);
            pVoPara->mVoChn++;
        }
        else
        {
            alogd("create vo channel[%d] ret[0x%x]!", pVoPara->mVoChn, ret);
            break;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pVoPara->mVoChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        return -1;
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = pVoPara->priv;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    ret = AW_MPI_VO_RegisterCallback(pVoPara->mVoLayer, pVoPara->mVoChn, &cbInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vo layer[%d] chn[%d] register callback failed, ret=%d", pVoPara->mVoLayer, pVoPara->mVoChn, ret);
        return -1;
    }
    AW_MPI_VO_SetChnDispBufNum(pVoPara->mVoLayer, pVoPara->mVoChn, 2);

    return 0;
}

static int destroyVoChn(VoParam *pVoPara)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    ret = AW_MPI_VO_DestroyChn(pVoPara->mVoLayer, pVoPara->mVoChn);
    if (ret != SUCCESS)
    {
        aloge("mpi vo destroy chn fail, ret=%d", ret);
        result = -1;
    }

    ret = AW_MPI_VO_DisableVideoLayer(pVoPara->mVoLayer);
    if (ret != SUCCESS)
    {
        aloge("mpi vo destroy video layer fail, ret=%d", ret);
        result = -1;
    }

    ret = AW_MPI_VO_RemoveOutsideVideoLayer(pVoPara->mUILayer);
    if (ret != SUCCESS)
    {
        aloge("mpi vo destroy video layer fail, ret=%d", ret);
        result = -1;
    }

    ret = AW_MPI_VO_Disable(pVoPara->mVoDev);
    if (ret != SUCCESS)
    {
        aloge("mpi vo destroy video layer fail, ret=%d", ret);
        result = -1;
    }

    return result;
}

static void configVencChn(VencParam *pPara, VencConfig *pVencCfg, ViConfig *pViCfg, void *priv)
{
    pPara->priv = priv;
    pPara->mpVencCfg = pVencCfg;
    pPara->mpViCfg = pViCfg;
}

static int configVencChnAttr(VencParam *pVencPara)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;

    memset(&pVencPara->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));

    pVencPara->mVencChnAttr.VeAttr.mOnlineEnable = pVencPara->mpVencCfg->mOnlineEnable;
    pVencPara->mVencChnAttr.VeAttr.mOnlineShareBufNum = pVencPara->mpVencCfg->mOnlineShareBufNum;
    alogd("vencChn%d OnlineEnable:%d, OnlineShareBufNum:%d", pVencPara->mVencChn, pVencPara->mVencChnAttr.VeAttr.mOnlineEnable, pVencPara->mVencChnAttr.VeAttr.mOnlineShareBufNum);
    pVencPara->mVencChnAttr.EncppAttr.eEncppSharpSetting = VencEncppSharp_FollowISPConfig;
    pVencPara->mVencChnAttr.VeAttr.MaxKeyInterval = 100;
    pVencPara->mVencChnAttr.VeAttr.Type = pVencPara->mpVencCfg->mVideoEncoderType;

    if (pVencPara->mpVencCfg->mVideoFrameRate)
    {
        vbvThreshSize = pVencPara->mpVencCfg->mVideoBitrate/8/pVencPara->mpVencCfg->mVideoFrameRate*15;
    }
    vbvBufSize = pVencPara->mpVencCfg->mVideoBitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    switch(pVencPara->mVencChnAttr.VeAttr.Type)
    {
        case PT_H264:
        {
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = AWALIGN((pVencPara->mpVencCfg->mVideoWidth*pVencPara->mpVencCfg->mVideoHeight*3/2)/3, 1024);
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.BufSize = AWALIGN(pVencPara->mpVencCfg->mVideoBitrate*4/8 + pVencPara->mVencChnAttr.VeAttr.AttrH264e.mThreshSize, 1024);
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.Profile = 2; //0:base 1:main 2:high
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pVencPara->mpVencCfg->mVideoWidth;
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pVencPara->mpVencCfg->mVideoHeight;
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.FastEncFlag = FALSE;
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.IQpOffset = 0;
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = vbvThreshSize;
            pVencPara->mVencChnAttr.VeAttr.AttrH264e.BufSize = vbvBufSize;
            break;
        }
        case PT_H265:
        {
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = AWALIGN((pVencPara->mpVencCfg->mVideoWidth*pVencPara->mpVencCfg->mVideoHeight*3/2)/3, 1024);
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mBufSize = AWALIGN(pVencPara->mpVencCfg->mVideoBitrate*4/8 + pVencPara->mVencChnAttr.VeAttr.AttrH264e.mThreshSize, 1024);
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mProfile = 0; //0:main 1:main10 2:sti11
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pVencPara->mpVencCfg->mVideoWidth;
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pVencPara->mpVencCfg->mVideoHeight;
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mFastEncFlag = FALSE;
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.IQpOffset = 0;
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = vbvThreshSize;
            pVencPara->mVencChnAttr.VeAttr.AttrH265e.mBufSize = vbvBufSize;
            break;
        }
        case PT_MJPEG:
        {
            pVencPara->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
            pVencPara->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pVencPara->mpVencCfg->mVideoWidth;
            pVencPara->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pVencPara->mpVencCfg->mVideoHeight;
            pVencPara->mVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = vbvThreshSize;
            pVencPara->mVencChnAttr.VeAttr.AttrMjpeg.mBufSize = vbvBufSize;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", pVencPara->mVencChnAttr.VeAttr.Type);
            break;
        }
    }
    pVencPara->mVencChnAttr.VeAttr.SrcPicWidth = pVencPara->mpViCfg->mCaptureWidth;
    pVencPara->mVencChnAttr.VeAttr.SrcPicHeight = pVencPara->mpViCfg->mCaptureHeight;
    pVencPara->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pVencPara->mVencChnAttr.VeAttr.PixelFormat = pVencPara->mpViCfg->mPicFormat;
    pVencPara->mVencChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_JPEG;
    pVencPara->mVencChnAttr.VeAttr.Rotate = ROTATE_NONE;

    switch(pVencPara->mVencChnAttr.VeAttr.Type)
    {
        case PT_H264:
        {
            switch (pVencPara->mpVencCfg->mVideoRcMode)
            {
                case 1:
                {
                    pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pVencPara->mpVencCfg->mVideoBitrate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pVencPara->mpViCfg->mFrameRate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = pVencPara->mpVencCfg->mVideoFrameRate;
                    pVencPara->mVencRcParam.ParamH264Vbr.mMinQp = 25;
                    pVencPara->mVencRcParam.ParamH264Vbr.mMaxQp = 45;
                    pVencPara->mVencRcParam.ParamH264Vbr.mMinPqp = 25;
                    pVencPara->mVencRcParam.ParamH264Vbr.mMaxPqp = 45;
                    pVencPara->mVencRcParam.ParamH264Vbr.mQpInit = 37;
                    pVencPara->mVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 1;
                    pVencPara->mVencRcParam.ParamH264Vbr.mMovingTh = 20;
                    pVencPara->mVencRcParam.ParamH264Vbr.mQuality = 1;
                    pVencPara->mVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
                    pVencPara->mVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
                    break;
                }
                case 2:
                {
                    pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 28;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 28;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264FixQp.mSrcFrmRate = pVencPara->mpViCfg->mFrameRate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264FixQp.mDstFrmRate = pVencPara->mpVencCfg->mVideoFrameRate;
                    break;
                }
                case 3:
                {
                    pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxBitRate = pVencPara->mpVencCfg->mVideoBitrate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Abr.mRatioChangeQp = 85;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Abr.mQuality = 8;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Abr.mMinIQp = 20;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxQp = 51;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Abr.mMinQp = 10;
                    break;
                }
                case 0:
                default:
                {
                    pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pVencPara->mpVencCfg->mVideoBitrate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pVencPara->mpViCfg->mFrameRate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = pVencPara->mpVencCfg->mVideoFrameRate;
                    pVencPara->mVencRcParam.ParamH264Cbr.mMaxQp = 50;
                    pVencPara->mVencRcParam.ParamH264Cbr.mMinQp = 20;
                    pVencPara->mVencRcParam.ParamH264Cbr.mMaxPqp = 50;
                    pVencPara->mVencRcParam.ParamH264Cbr.mMinPqp = 20;
                    pVencPara->mVencRcParam.ParamH264Cbr.mQpInit = 30;
                    pVencPara->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 1;
                    break;
                }
            }
            break;
        }
        case PT_H265:
        {
            switch (pVencPara->mpVencCfg->mVideoRcMode)
            {
                case 1:
                {
                    pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pVencPara->mpVencCfg->mVideoBitrate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pVencPara->mpViCfg->mFrameRate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = pVencPara->mpVencCfg->mVideoFrameRate;
                    pVencPara->mVencRcParam.ParamH265Vbr.mMinQp = 25;
                    pVencPara->mVencRcParam.ParamH265Vbr.mMaxQp = 45;
                    pVencPara->mVencRcParam.ParamH265Vbr.mMinPqp = 25;
                    pVencPara->mVencRcParam.ParamH265Vbr.mMaxPqp = 45;
                    pVencPara->mVencRcParam.ParamH265Vbr.mQpInit = 37;
                    pVencPara->mVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 1;
                    pVencPara->mVencRcParam.ParamH265Vbr.mMovingTh = 20;
                    pVencPara->mVencRcParam.ParamH265Vbr.mQuality = 1;
                    pVencPara->mVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
                    pVencPara->mVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
                    break;
                }
                case 2:
                {
                    pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 28;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 28;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265FixQp.mSrcFrmRate = pVencPara->mpViCfg->mFrameRate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265FixQp.mDstFrmRate = pVencPara->mpVencCfg->mVideoFrameRate;
                    break;
                }
                case 3:
                {
                    pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxBitRate = pVencPara->mpVencCfg->mVideoBitrate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Abr.mRatioChangeQp = 85;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Abr.mQuality = 8;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Abr.mMinIQp = 20;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxQp = 51;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Abr.mMinQp = 10;
                    break;
                }
                case 0:
                default:
                {
                    pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pVencPara->mpVencCfg->mVideoBitrate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pVencPara->mpViCfg->mFrameRate;
                    pVencPara->mVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = pVencPara->mpVencCfg->mVideoFrameRate;
                    pVencPara->mVencRcParam.ParamH265Cbr.mMaxQp = 50;
                    pVencPara->mVencRcParam.ParamH265Cbr.mMinQp = 20;
                    pVencPara->mVencRcParam.ParamH265Cbr.mMaxPqp = 50;
                    pVencPara->mVencRcParam.ParamH265Cbr.mMinPqp = 20;
                    pVencPara->mVencRcParam.ParamH265Cbr.mQpInit = 30;
                    pVencPara->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 1;
                    break;
                }
            }
            break;
        }
        case PT_MJPEG:
        {
            if(pVencPara->mpVencCfg->mVideoRcMode != 0)
            {
                aloge("fatal error! mjpeg don't support rcMode[%d]!", pVencPara->mpVencCfg->mVideoRcMode);
            }
            pVencPara->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pVencPara->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pVencPara->mpVencCfg->mVideoBitrate;
            pVencPara->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pVencPara->mpViCfg->mFrameRate;
            pVencPara->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = pVencPara->mpVencCfg->mVideoFrameRate;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", pVencPara->mVencChnAttr.VeAttr.Type);
            break;
        }
    }
    pVencPara->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pVencPara->mVencChnAttr.GopAttr.mGopSize = 2;
    alogd("venc set Rcmode=%d", pVencPara->mVencChnAttr.RcAttr.mRcMode);

    return 0;
}

static int createVencChn(VencParam *pVencPara)
{
    ERRORTYPE ret = SUCCESS;
    BOOL nSuccessFlag = FALSE;

    pVencPara->mVencChn = pVencPara->mpVencCfg->mVencChn;
    alogd("VencChn %d", pVencPara->mVencChn);

    configVencChnAttr(pVencPara);

    while (pVencPara->mVencChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pVencPara->mVencChn, &pVencPara->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pVencPara->mVencChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pVencPara->mVencChn);
            pVencPara->mVencChn++;
        }
        else
        {
            aloge("fatal error! create venc channel[%d] fail, ret[0x%x]", pVencPara->mVencChn, ret);
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pVencPara->mVencChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return -1;
    }

    ret = AW_MPI_VENC_SetRcParam(pVencPara->mVencChn, &pVencPara->mVencRcParam);
    if (ret != SUCCESS)
    {
        aloge("fatal error! venc chn[%d] set rc param fail[0x%x]!", pVencPara->mVencChn, ret);
        return -1;
    }

//    VENC_FRAME_RATE_S stFrameRate;
//    stFrameRate.SrcFrmRate = pVencPara->mpViCfg->mFrameRate;
//    stFrameRate.DstFrmRate = pVencPara->mpVencCfg->mVideoFrameRate;
//    alogd("set srcFrameRate:%d, venc framerate:%d", stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
//    ret = AW_MPI_VENC_SetFrameRate(pVencPara->mVencChn, &stFrameRate);
//    if (ret != SUCCESS)
//    {
//        aloge("fatal error! venc chn[%d] set framerate fail[0x%x]!", pVencPara->mVencChn, ret);
//        return -1;
//    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = pVencPara->priv;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    ret = AW_MPI_VENC_RegisterCallback(pVencPara->mVencChn, &cbInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! venc chn[%d] register callback failed, ret=%d", pVencPara->mVencChn, ret);
        return -1;
    }

    // only for code parallel, only set online channel
    SampleCodecParallelContext *pContext = (SampleCodecParallelContext *)(pVencPara->priv);
    if (pContext->mPlay.mEnable && (pContext->mRec.mEnable || pContext->mSubRec.mEnable) &&
        (pVencPara->mpVencCfg->mOnlineEnable))
    {
        ret = AW_MPI_VENC_SetEncAndDecCase(pVencPara->mVencChn, TRUE);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] set EncAndDecCase[0x%x]!", pVencPara->mVencChn, ret);
            return -1;
        }
    }

    VENC_IspVeLinkAttr stIspVeLinkAttr;
    memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
    stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
    stIspVeLinkAttr.bEnableVe2Isp = FALSE;
    stIspVeLinkAttr.nVipp = pVencPara->mpViCfg->mVippID;
    AW_MPI_VENC_EnableIspVeLink(pVencPara->mVencChn, &stIspVeLinkAttr);
    alogd("VencChn[%d] ispVeLink:%d-%d-%d", pVencPara->mVencChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
        stIspVeLinkAttr.nVipp);

    return 0;
}

static int destroyVencChn(VencParam *pVencPara)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_VENC_ResetChn(pVencPara->mVencChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vencChn[%d] reset fail[0x%x]", pVencPara->mVencChn, ret);
        result = -1;
    }
    ret = AW_MPI_VENC_DestroyChn(pVencPara->mVencChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vencChn[%d] destroy fail[0x%x]", pVencPara->mVencChn, ret);
        result = -1;
    }
    return result;
}

static int configVdecChn(VdecParam *pVdecPara, DemuxParam *pDmxPara, VdecConfig *pVdecCfg, VoConfig *pVoCfg, void *priv)
{
    ERRORTYPE ret = SUCCESS;
    DEMUX_MEDIA_INFO_S DemuxMediaInfo = {0};

    ret = AW_MPI_DEMUX_GetMediaInfo(pDmxPara->mDmxChn, &DemuxMediaInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! get media info fail, ret=%d", ret);
        return -1;
    }

    if ((DemuxMediaInfo.mVideoNum > 0 && DemuxMediaInfo.mVideoIndex >= DemuxMediaInfo.mVideoNum)
        || (DemuxMediaInfo.mAudioNum > 0 && DemuxMediaInfo.mAudioIndex >= DemuxMediaInfo.mAudioNum)
        || (DemuxMediaInfo.mSubtitleNum > 0 && DemuxMediaInfo.mSubtitleIndex >= DemuxMediaInfo.mSubtitleNum))
    {
        alogd("fatal error, trackIndex wrong! [%d][%d],[%d][%d],[%d][%d]",
               DemuxMediaInfo.mVideoNum, DemuxMediaInfo.mVideoIndex, DemuxMediaInfo.mAudioNum,
               DemuxMediaInfo.mAudioIndex, DemuxMediaInfo.mSubtitleNum, DemuxMediaInfo.mSubtitleIndex);
        return -1;
    }

    /* Pass information from dmx to vdec */
    pVdecPara->mMaxVdecOutputWidth = DemuxMediaInfo.mVideoStreamInfo[0].mWidth;
    pVdecPara->mMaxVdecOutputHeight = DemuxMediaInfo.mVideoStreamInfo[0].mHeight;
    pVdecPara->mDemuxDisableTrack = pDmxPara->mDemuxDisableTrack;
    pVdecPara->mVideoNum = DemuxMediaInfo.mVideoNum;
    pVdecPara->mCodecType = DemuxMediaInfo.mVideoStreamInfo[DemuxMediaInfo.mVideoIndex].mCodecType;
    pVdecPara->mVdecBufSize = pVdecCfg->mVdecBufSize;

    if (DemuxMediaInfo.mVideoNum <= 0)
    {
        aloge("invalid video num %d", DemuxMediaInfo.mVideoNum);
        return -1;
    }

    pVdecPara->priv = priv;
    pVdecPara->mPicFormat = pVoCfg->mPicFormat;  // get PicFormat from vo config

    return 0;
}

static int createVdecChn(VdecParam *pVdecPara)
{
    ERRORTYPE ret = SUCCESS;
    BOOL nSuccessFlag = FALSE;

    /* vdec params from dmx media info */
    if (!((pVdecPara->mVideoNum > 0) && !(pVdecPara->mDemuxDisableTrack & DEMUX_DISABLE_VIDEO_TRACK)))
    {
        aloge("invalid param! mVideoNum=%d, mDemuxDisableTrack=%d ", pVdecPara->mVideoNum, pVdecPara->mDemuxDisableTrack);
        return -1;
    }

    VDEC_CHN_ATTR_S VdecChnAttr;
    memset(&VdecChnAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    VdecChnAttr.mPicWidth = pVdecPara->mMaxVdecOutputWidth;
    VdecChnAttr.mPicHeight = pVdecPara->mMaxVdecOutputHeight;
    VdecChnAttr.mInitRotation = 0;
    VdecChnAttr.mOutputPixelFormat = pVdecPara->mPicFormat;
    VdecChnAttr.mType = pVdecPara->mCodecType;
    VdecChnAttr.mVdecVideoAttr.mSupportBFrame = 0;
    VdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
    VdecChnAttr.mBufSize = pVdecPara->mVdecBufSize;

    pVdecPara->mVdecChn = 0;

    while (pVdecPara->mVdecChn < VDEC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VDEC_CreateChn(pVdecPara->mVdecChn, &VdecChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create vdec channel[%d] success!", pVdecPara->mVdecChn);
            break;
        }
        else if (ERR_VDEC_EXIST == ret)
        {
            alogd("vdec channel[%d] is exist, find next!", pVdecPara->mVdecChn);
            pVdecPara->mVdecChn++;
        }
        else
        {
            alogd("create vdec channel[%d] fail, ret[0x%x]!", pVdecPara->mVdecChn, ret);
            break;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pVdecPara->mVdecChn = MM_INVALID_CHN;
        aloge("fatal error! create vdec channel fail!");
        return -1;
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = pVdecPara->priv;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    ret = AW_MPI_VDEC_RegisterCallback(pVdecPara->mVdecChn, &cbInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vdec chn[%d] register callback fail, ret=%d", pVdecPara->mVdecChn, ret);
        return -1;
    }

    return 0;
}

static ERRORTYPE destroyVdecChn(VdecParam *pVdecPara)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_VDEC_DestroyChn(pVdecPara->mVdecChn);
    if (ret != SUCCESS)
    {
        aloge("mpi vdec destroy chn fail, ret=%d", ret);
        result = -1;
    }
    return result;
}

static void configMuxChn(MuxParam *pPara, MuxConfig *pMuxCfg, VencConfig *pVencCfg, void *priv)
{
    pPara->priv = priv;
    pPara->mpMuxCfg = pMuxCfg;
    pPara->mpVencCfg = pVencCfg;
}

static int createMuxChn(MuxParam *pMuxPara)
{
    ERRORTYPE ret = SUCCESS;
    BOOL nSuccessFlag = FALSE;
    MUX_CHN_ATTR_S MuxChnAttr;

    memset(&MuxChnAttr, 0, sizeof(MUX_CHN_ATTR_S));
    MuxChnAttr.mVideoAttrValidNum = 1;
    MuxChnAttr.mVideoAttr[0].mHeight = pMuxPara->mpVencCfg->mVideoHeight;
    MuxChnAttr.mVideoAttr[0].mWidth = pMuxPara->mpVencCfg->mVideoWidth;
    MuxChnAttr.mVideoAttr[0].mVideoFrmRate = pMuxPara->mpVencCfg->mVideoFrameRate * 1000;
    MuxChnAttr.mVideoAttr[0].mVideoEncodeType = pMuxPara->mpVencCfg->mVideoEncoderType;
    MuxChnAttr.mVideoAttr[0].mVeChn = pMuxPara->mpVencCfg->mVencChn;
    MuxChnAttr.mAudioEncodeType = PT_MAX;
    MuxChnAttr.mTextEncodeType = PT_MAX;

    //MuxChnAttr.mMuxerId = pMuxPara->mpMuxCfg->mMuxChn;
    MuxChnAttr.mMediaFileFormat = pMuxPara->mpMuxCfg->mVideoFileFormat;
    MuxChnAttr.mMaxFileDuration = pMuxPara->mpMuxCfg->mVideoDuration * 1000;
    MuxChnAttr.mMaxFileSizeBytes = 0;
    MuxChnAttr.mCallbackOutFlag = FALSE;
    MuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    MuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;

    int DestFd = open(pMuxPara->mpMuxCfg->mDestVideoFile, O_RDWR | O_CREAT, 0666);
    if (0 > DestFd)
    {
        aloge("Error %d: Failed to open file %s, DestFd=%d, errno=%d", errno, pMuxPara->mpMuxCfg->mDestVideoFile, DestFd);
        return -1;
    }
    pMuxPara->mMuxChn = pMuxPara->mpMuxCfg->mMuxChn;
    alogd("MuxChn %d", pMuxPara->mMuxChn);

    while (pMuxPara->mMuxChn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(pMuxPara->mMuxChn, &MuxChnAttr, DestFd, 0);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create muxChn[%d] success!", pMuxPara->mMuxChn);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("muxChn[%d] is exist, find next!", pMuxPara->mMuxChn);
            pMuxPara->mMuxChn++;
        }
        else
        {
            alogd("create muxChn[%d] fail, ret[0x%x]", pMuxPara->mMuxChn, ret);
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pMuxPara->mMuxChn = MM_INVALID_CHN;
        aloge("fatal error! create muxChn fail!");
        return -1;
    }
    if (DestFd >= 0)
    {
        alogd("close DestFd %d \n", DestFd);
        close(DestFd);
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = pMuxPara->priv;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    ret = AW_MPI_MUX_RegisterCallback(pMuxPara->mMuxChn, &cbInfo);
    if(ret != SUCCESS)
    {
        aloge("fatal error! muxChn[%d] register callback fail, ret=%d", pMuxPara->mMuxChn, ret);
        return -1;
    }
    return 0;
}

static ERRORTYPE destroyMuxChn(MuxParam *pMuxPara)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_MUX_DestroyChn(pMuxPara->mMuxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! muxChn[%d] destroy fail, ret=%d", pMuxPara->mMuxChn, ret);
        result = -1;
    }
    return result;
}

static void configDemuxChn(DemuxParam *pPara, DemuxConfig *pCfg, void *priv)
{
    pPara->priv = priv;
    pPara->mpDmxCfg = pCfg;
}

static int createDemuxChn(DemuxParam *pDemuxPara)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    BOOL nSuccessFlag = FALSE;

    int SrcFd = open(pDemuxPara->mpDmxCfg->mSrcVideoFile, O_RDONLY);
    if (SrcFd < 0)
    {
        aloge("ERROR: cannot open video src file");
        return -1;
    }
    alogd("open %s success, SrcFd %d", pDemuxPara->mpDmxCfg->mSrcVideoFile, SrcFd);

    DEMUX_CHN_ATTR_S DmxChnAttr;
    memset(&DmxChnAttr, 0, sizeof(DEMUX_CHN_ATTR_S));
    //DmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    DmxChnAttr.mSourceType = SOURCETYPE_FD;
    DmxChnAttr.mSourceUrl = NULL;
    DmxChnAttr.mFd = SrcFd;
    DmxChnAttr.mDemuxDisableTrack = DEMUX_DISABLE_SUBTITLE_TRACK | DEMUX_DISABLE_AUDIO_TRACK;

    /* save it for createVdec */
    pDemuxPara->mDemuxDisableTrack = DmxChnAttr.mDemuxDisableTrack;

    pDemuxPara->mDmxChn = 0;

    while (pDemuxPara->mDmxChn < DEMUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_DEMUX_CreateChn(pDemuxPara->mDmxChn, &DmxChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create demux channel[%d] success!", pDemuxPara->mDmxChn);
            break;
        }
        else if (ERR_DEMUX_EXIST == ret)
        {
            alogd("demux channel[%d] is exist, find next!", pDemuxPara->mDmxChn);
            pDemuxPara->mDmxChn++;
        }
        else
        {
            alogd("create demux channel[%d] fail, ret[0x%x]!", pDemuxPara->mDmxChn, ret);
            break;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pDemuxPara->mDmxChn = MM_INVALID_CHN;
        aloge("fatal error! create demux channel fail!");
        result = -1;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = pDemuxPara->priv;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        ret = AW_MPI_DEMUX_RegisterCallback(pDemuxPara->mDmxChn, &cbInfo);
        if (ret != SUCCESS)
        {
            aloge("fatal error! dmx chn[%d] Register Callback failed, ret=%d", pDemuxPara->mDmxChn, ret);
            result = -1;
        }
    }

    if (SrcFd >= 0)
    {
        alogd("close SrcFd %d", SrcFd);
        close(SrcFd);
    }

    return result;
}

static ERRORTYPE destroyDemuxChn(DemuxParam *pDemuxPara)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_DEMUX_DestroyChn(pDemuxPara->mDmxChn);
    if (ret != SUCCESS)
    {
        aloge("mpi demux destroy chn fail, ret=%d", ret);
        result = -1;
    }
    return result;
}

static void configClockChn(ClockParam *pPara, void *pCfg, void *priv)
{
    pPara->priv = priv;
    (void)pCfg;
}

static int createClockChn(ClockParam *pClkPara)
{
    ERRORTYPE ret = SUCCESS;
    BOOL bSuccessFlag = FALSE;
    CLOCK_CHN_ATTR_S ClockChnAttr;

    pClkPara->mClockChn = 0;

    while (pClkPara->mClockChn < CLOCK_MAX_CHN_NUM)
    {
        memset(&ClockChnAttr, 0, sizeof(CLOCK_CHN_ATTR_S));
        ClockChnAttr.nWaitMask = 1 << 1; //CLOCK_PORT_VIDEO; //becareful this is too important!!!
        ret = AW_MPI_CLOCK_CreateChn(pClkPara->mClockChn, &ClockChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", pClkPara->mClockChn);
            break;
        }
        else if (ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", pClkPara->mClockChn);
            pClkPara->mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", pClkPara->mClockChn, ret);
            break;
        }
    }

    if (FALSE == bSuccessFlag)
    {
        pClkPara->mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
        return -1;
    }
    else
    {
        return 0;
    }
}

static ERRORTYPE destroyClockChn(ClockParam *pClkPara)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_CLOCK_DestroyChn(pClkPara->mClockChn);
    if (ret != SUCCESS)
    {
        aloge("mpi clock destroy chn fail, ret=%d", ret);
        result = -1;
    }

    return result;
}

/*
 * [record]  VI(VIPP.0) -> VENC -> MUX
 */
static int bindRecordComponent(RecordContext *pRec)
{
    ERRORTYPE ret = SUCCESS;
    MPP_CHN_S ViChn = {MOD_ID_VIU, pRec->mViPara.mViDev, pRec->mViPara.mViChn};
    MPP_CHN_S VencChn = {MOD_ID_VENC, 0, pRec->mVencPara.mVencChn};
    MPP_CHN_S MuxChn = {MOD_ID_MUX, 0, pRec->mMuxPara.mMuxChn};

    ret = AW_MPI_SYS_Bind(&ViChn, &VencChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vi dev[%d] chn[%d] & venc chn[%d] bind fail", pRec->mViPara.mViDev, pRec->mViPara.mViChn, pRec->mVencPara.mVencChn);
        return -1;
    }
    alogd("vi dev[%d] chn[%d] & venc chn[%d] bind success", pRec->mViPara.mViDev, pRec->mViPara.mViChn, pRec->mVencPara.mVencChn);

    ret = AW_MPI_SYS_Bind(&VencChn, &MuxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! venc chn[%d] & mux chn[%d] bind fail", pRec->mVencPara.mVencChn, pRec->mMuxPara.mMuxChn);
        return -1;
    }
    alogd("venc chn[%d] & mux chn[%d] bind success", pRec->mVencPara.mVencChn, pRec->mMuxPara.mMuxChn);

    return 0;
}

static int unbindRecordComponent(RecordContext *pRec)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    MPP_CHN_S ViChn = {MOD_ID_VIU, pRec->mViPara.mViDev, pRec->mViPara.mViChn};
    MPP_CHN_S VencChn = {MOD_ID_VENC, 0, pRec->mVencPara.mVencChn};
    MPP_CHN_S MuxChn = {MOD_ID_MUX, 0, pRec->mMuxPara.mMuxChn};

    ret = AW_MPI_SYS_UnBind(&ViChn, &VencChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vi dev[%d] chn[%d] & venc chn[%d] unbind fail", pRec->mViPara.mViDev, pRec->mViPara.mViChn, pRec->mVencPara.mVencChn);
        result = -1;
    }
    alogd("vi dev[%d] chn[%d] & venc chn[%d] unbind success", pRec->mViPara.mViDev, pRec->mViPara.mViChn, pRec->mVencPara.mVencChn);

    ret = AW_MPI_SYS_UnBind(&VencChn, &MuxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! venc chn[%d] & mux chn[%d] unbind fail", pRec->mVencPara.mVencChn, pRec->mMuxPara.mMuxChn);
        result = -1;
    }
    alogd("venc chn[%d] & mux chn[%d] unbind success", pRec->mVencPara.mVencChn, pRec->mMuxPara.mMuxChn);

    return result;
}

static int generateFileName(RecordContext *pContext, char *pNameBuf)
{
    char strStemPath[MAX_FILE_PATH_SIZE] = {0};

    char *pDot = strrchr(pContext->mMuxCfg.mDestVideoFile, '.');
    if(pDot)
    {
        strncpy(strStemPath, pContext->mMuxCfg.mDestVideoFile, pDot-pContext->mMuxCfg.mDestVideoFile);
    }
    else
    {
        strcpy(strStemPath, pContext->mMuxCfg.mDestVideoFile);
    }
    pContext->nFileNum++;
    sprintf(pNameBuf, "%s_%d.%s", strStemPath, pContext->nFileNum, (MEDIA_FILE_FORMAT_TS==pContext->mMuxCfg.mVideoFileFormat)?"ts":"mp4");
    return 0;
}
static void *RecordMsgQueueThread(void *pThreadData)
{
    ERRORTYPE ret;
    RecordContext *pRecCtx = (RecordContext*)pThreadData;
    message_t stCmdMsg;
    CodecParallelMsgType cmd;
    int nCmdPara;

    alogd("msg queue thread start run!");
    while (1)
    {
        if (0 == get_message(&pRecCtx->stMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case MSGQUEUE_Rec_NeedSetNextFd:
                {
                    int muxChn = nCmdPara;
                    char fileName[MAX_FILE_PATH_SIZE] = {0};
                    generateFileName(pRecCtx, fileName);
                    FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                    memset(pFilePathNode, 0, sizeof(FilePathNode));
                    strncpy(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_SIZE-1);
                    list_add_tail(&pFilePathNode->mList, &pRecCtx->stFileList);
                    alogd("muxChn[%d] set next fd, filepath=%s", muxChn, fileName);

                    int fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0666);
                    if (fd < 0)
                    {
                        aloge("fatal error! fail to open %s", fileName);
                    }
                    ret = AW_MPI_MUX_SwitchFd(pRecCtx->mMuxPara.mMuxChn, fd, 0);
                    if(ret != SUCCESS)
                    {
                        aloge("fatal error! muxChn[%d] switch fd fail.", pRecCtx->mMuxPara.mMuxChn);
                    }
                    close(fd);
                    break;
                }
                case MSGQUEUE_Rec_FileDone:
                {
                    int res;
                    int muxChn = nCmdPara;
                    int cnt = 0;
                    struct list_head *pList;
                    list_for_each(pList, &pRecCtx->stFileList){cnt++;}
                    FilePathNode *pNode = NULL;
                    while (cnt > pRecCtx->mMuxCfg.nMaxFileNum)
                    {
                        pNode = list_first_entry(&pRecCtx->stFileList, FilePathNode, mList);
                        if ((res = remove(pNode->strFilePath)) != 0)
                        {
                            aloge("fatal error! delete file[%s] failed:%d-%s", pNode->strFilePath, res, strerror(errno));
                        }
                        else
                        {
                            alogd("delete file[%s] success", pNode->strFilePath);
                        }
                        cnt--;
                        list_del(&pNode->mList);
                        free(pNode);
                    }
                    break;
                }
                case MSGQUEUE_STOP:
                {
                    goto _Exit;
                }
                default:
                {
                    break;
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pRecCtx->stMsgQueue, 0);
        }
    }
_Exit:
    alogd("RecordCtx[(%d-%d-%d)-%d-%d] msg queue thread exit!", pRecCtx->mViPara.mIspDev, pRecCtx->mViPara.mViDev, pRecCtx->mViPara.mViChn,
        pRecCtx->mVencPara.mVencChn, pRecCtx->mMuxPara.mMuxChn);
    return NULL;
}

static int prepareRecord(RecordContext *pRec)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    configViChn(&pRec->mViPara, &pRec->mViCfg, pRec->priv);
    result = createViChn(&pRec->mViPara, pRec->mViCfg.mVippBufNum);
    if (result != 0)
    {
        aloge("create vi chn fail, result=%d", result);
        return -1;
    }
    alogd("create vi chn success");

    configVencChn(&pRec->mVencPara, &pRec->mVencCfg, &pRec->mViCfg, pRec->priv);
    result = createVencChn(&pRec->mVencPara);
    if (result != 0)
    {
        aloge("create venc ch fail, result=%d", result);
        return -1;
    }
    alogd("create venc chn success");

    configMuxChn(&pRec->mMuxPara, &pRec->mMuxCfg, &pRec->mVencCfg, pRec->priv);
    result = createMuxChn(&pRec->mMuxPara);
    if (result != 0)
    {
        aloge("create mux grp fail, result=%d", result);
        return -1;
    }
    alogd("create mux chn success");

    alogd("mVideoEncoderType=%d", pRec->mVencCfg.mVideoEncoderType);

    /* set sps pps */
    if (pRec->mVencCfg.mVideoEncoderType == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        memset(&H264SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pRec->mVencPara.mVencChn, &H264SpsPpsInfo);
        if (ret != SUCCESS)
        {
            aloge("mpi venc get H264 sps pps info fail, ret=%d", ret);
            return -1;
        }
        ret = AW_MPI_MUX_SetH264SpsPpsInfo(pRec->mMuxPara.mMuxChn, pRec->mVencPara.mVencChn, &H264SpsPpsInfo);
        if (ret != SUCCESS)
        {
            aloge("mpi venc set H264 sps pps info fail, ret=%d", ret);
            return -1;
        }
    }
    else if (pRec->mVencCfg.mVideoEncoderType == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        memset(&H265SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pRec->mVencPara.mVencChn, &H265SpsPpsInfo);
        if (ret != SUCCESS)
        {
            aloge("mpi venc get H265 sps pps info fail, ret=%d", ret);
            return -1;
        }
        ret = AW_MPI_MUX_SetH265SpsPpsInfo(pRec->mMuxPara.mMuxChn, pRec->mVencPara.mVencChn, &H265SpsPpsInfo);
        if (ret != SUCCESS)
        {
            aloge("mpi venc set H265 sps pps info fail, ret=%d", ret);
            return -1;
        }
    }
    else
    {
        alogd("don't need set spspps for encodeType[%d]", pRec->mVencCfg.mVideoEncoderType);
    }

    result = bindRecordComponent(pRec);
    if (result != 0)
    {
        alogd("bind Record Component fail, result=%d", result);
        return -1;
    }

    INIT_LIST_HEAD(&pRec->stFileList);
    result = message_create(&pRec->stMsgQueue);
    if(result < 0)
    {
        aloge("fatal error! message create fail!");
    }
    result = pthread_create(&pRec->nMsgQueueThreadId, NULL, RecordMsgQueueThread, pRec);
    if (result != 0)
    {
        aloge("fatal error! pthread create fail[%d]", result);
    }

    alogd("leave, result=%d", result);
    return result;
}

static int startRecord(RecordContext *pRec)
{
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    ret = AW_MPI_VI_EnableVipp(pRec->mViPara.mViDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! enable Vipp[%d] fail, ret=%d", pRec->mViPara.mViDev, ret);
        return -1;
    }

#ifdef ISP_RUN
    /* Note:
     * if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations
     * must be after AW_MPI_VI_EnableVipp.
     */
    ret = AW_MPI_VI_SetVippMirror(pRec->mViPara.mViDev, 0);
    if (ret != SUCCESS)
    {
        aloge("fatal error! mpi vi set vipp Mirror fail, ret=%d", ret);
        return -1;
    }
    ret = AW_MPI_VI_SetVippFlip(pRec->mViPara.mViDev, 0);
    if (ret != SUCCESS)
    {
        aloge("fatal error! mpi vi set vipp Flip fail, ret=%d", ret);
        return -1;
    }
#endif

    ret = AW_MPI_VI_EnableVirChn(pRec->mViPara.mViDev, pRec->mViPara.mViChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! viChn[%d,%d] enable error[0x%x]!", pRec->mViPara.mViDev, pRec->mViPara.mViChn, ret);
        return -1;
    }

    ret = AW_MPI_VENC_StartRecvPic(pRec->mVencPara.mVencChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vencChn[%d] start error[0x%x]!", pRec->mVencPara.mVencChn, ret);
        return -1;
    }

    ret = AW_MPI_MUX_StartChn(pRec->mMuxPara.mMuxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! muxChn[%d] start error[0x%x]!", pRec->mMuxPara.mMuxChn, ret);
        return -1;
    }

#ifdef ISP_RUN
    ret = AW_MPI_ISP_SwitchIspConfig(pRec->mViPara.mIspDev, NORMAL_CFG);
    if (ret != SUCCESS)
    {
        aloge("fatal error! mpi isp switch isp config fail, ret=%d", ret);
        return -1;
    }
#endif

    alogd("success");
    return 0;
}

static int stopRecord(RecordContext *pRec)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    ret = AW_MPI_VI_DisableVirChn(pRec->mViPara.mViDev, pRec->mViPara.mViChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vipp[%d]chn[%d] disable fail[0x%x]", pRec->mViPara.mViDev, pRec->mViPara.mViChn, ret);
        result = -1;
    }

    ret = AW_MPI_MUX_StopChn(pRec->mMuxPara.mMuxChn, FALSE);
    if(ret != SUCCESS)
    {
        aloge("fatal error! muxChn[%d] stop fail[0x%x]", pRec->mMuxPara.mMuxChn, ret);
        result = -1;
    }

    ret = AW_MPI_VENC_StopRecvPic(pRec->mVencPara.mVencChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vencChn[%d] stop fail[0x%x]", pRec->mVencPara.mVencChn, ret);
        result = -1;
    }

    alogd("%s", ((0==result) ? "success" : "fail"));
    return 0;
}

static int releaseRecord(RecordContext *pRec)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    /* Note:
     * Because the component has been destroyed after the program ends,
     * it is not necessary to unbind Record Components.
     */
    // result = unbindRecordComponent(pRec);
    // if (result != 0)
    // {
    //     alogd("unbind Record Component fail, result=%d", result);
    // }

    result = destroyMuxChn(&pRec->mMuxPara);
    if (result != 0)
    {
        aloge("destroy Mux Chn fail, result=%d", result);
    }

    result = destroyVencChn(&pRec->mVencPara);
    if (result != 0)
    {
        aloge("destroy Venc Chn fail, result=%d", result);
    }

    result = destroyViChn(&pRec->mViPara);
    if (result != 0)
    {
        aloge("destroy Vi Chn fail, result=%d", result);
    }

    message_t stCmdMsg;
    InitMessage(&stCmdMsg);
    stCmdMsg.command = MSGQUEUE_STOP;
    putMessageWithData(&pRec->stMsgQueue, &stCmdMsg);
    void *pError = NULL;
    pthread_join(pRec->nMsgQueueThreadId, (void**)pError);
    message_destroy(&pRec->stMsgQueue);

    if(!list_empty(&pRec->stFileList))
    {
        FilePathNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pRec->stFileList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }

    alogd("%s", ((0==result) ? "success" : "fail"));
    return result;
}

/*
 * [preview] VI(VIPP.1) -> VO
 */
static int bindPreviewComponent(PreviewContext *pPrew)
{
    ERRORTYPE ret = SUCCESS;
    MPP_CHN_S ViChn = {MOD_ID_VIU, pPrew->mViPara.mViDev, pPrew->mViPara.mViChn};
    MPP_CHN_S VoChn = {MOD_ID_VOU, pPrew->mVoPara.mVoLayer, pPrew->mVoPara.mVoChn};

    ret = AW_MPI_SYS_Bind(&ViChn, &VoChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vi dev%d chn%d & vo layer%d chn%d bind fail", pPrew->mViPara.mViDev, pPrew->mViPara.mViChn, pPrew->mVoPara.mVoLayer, pPrew->mVoPara.mVoChn);
        return -1;
    }
    alogd("vi dev%d chn%d & vo layer%d chn%d bind success", pPrew->mViPara.mViDev, pPrew->mViPara.mViChn, pPrew->mVoPara.mVoLayer, pPrew->mVoPara.mVoChn);

    return 0;
}

static int unbindPreviewComponent(PreviewContext *pPrew)
{
    ERRORTYPE ret = SUCCESS;
    MPP_CHN_S ViChn = {MOD_ID_VIU, pPrew->mViPara.mViDev, pPrew->mViPara.mViChn};
    MPP_CHN_S VoChn = {MOD_ID_VOU, pPrew->mVoPara.mVoLayer, pPrew->mVoPara.mVoChn};

    ret = AW_MPI_SYS_UnBind(&ViChn, &VoChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vi dev%d chn%d & vo layer%d chn%d unbind fail", pPrew->mViPara.mViDev, pPrew->mViPara.mViChn, pPrew->mVoPara.mVoLayer, pPrew->mVoPara.mVoChn);
        return -1;
    }
    alogd("vi dev%d chn%d & vo layer%d chn%d unbind success", pPrew->mViPara.mViDev, pPrew->mViPara.mViChn, pPrew->mVoPara.mVoLayer, pPrew->mVoPara.mVoChn);

    return 0;
}

static int preparePreview(PreviewContext *pPrew)
{
    int result = 0;

    alogd("enter");

    configViChn(&pPrew->mViPara, &pPrew->mViCfg, pPrew->priv);
    result = createViChn(&pPrew->mViPara, 5);
    if (result != 0)
    {
        aloge("fatal error! create vi chn fail, result=%d", result);
        return -1;
    }

    configVoChn(&pPrew->mVoPara, &pPrew->mVoCfg, pPrew->priv);
    result = createVoChn(&pPrew->mVoPara);
	if (result != 0)
    {
		alogd("create vo ch fail, result=%d", result);
        return -1;
	}

    result = bindPreviewComponent(pPrew);
    if (result != 0)
    {
        aloge("bind Preview Component fail, result=%d", result);
		return -1;
    }

    alogd("success");
    return 0;
}

static int startPreview(PreviewContext *pPrew)
{
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    ret = AW_MPI_VI_EnableVipp(pPrew->mViPara.mViDev);
    if(ret != SUCCESS)
    {
        aloge("fatal error! enable Vipp[%d] fail, ret=%d", pPrew->mViPara.mViDev, ret);
        return -1;
    }

#ifdef ISP_RUN
    /* Note:
     * if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations
     * must be after AW_MPI_VI_EnableVipp.
     */
    ret = AW_MPI_VI_SetVippMirror(pPrew->mViPara.mViDev, 0);
    if (ret != SUCCESS)
    {
        aloge("fatal error! mpi vi set vipp Mirror fail, ret=%d", ret);
        return -1;
    }
    ret = AW_MPI_VI_SetVippFlip(pPrew->mViPara.mViDev, 0);
    if (ret != SUCCESS)
    {
        aloge("fatal error! mpi vi set vipp Flip fail, ret=%d", ret);
        return -1;
    }
#endif

    ret = AW_MPI_VI_EnableVirChn(pPrew->mViPara.mViDev, pPrew->mViPara.mViChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! mpi vi enable vir chn fail, ret=%d", ret);
        return -1;
    }

    ret = AW_MPI_VO_StartChn(pPrew->mVoPara.mVoLayer, pPrew->mVoPara.mVoChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! mpi vo start Chn fail, ret=%d", ret);
        return -1;
    }

#ifdef ISP_RUN
    ret = AW_MPI_ISP_SwitchIspConfig(pPrew->mViPara.mIspDev, NORMAL_CFG);
    if (ret != SUCCESS)
    {
        aloge("fatal error! mpi isp switch isp config fail, ret=%d", ret);
        return -1;
    }
#endif

    alogd("success");
    return 0;
}

static int stopPreview(PreviewContext *pPrew)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    ret = AW_MPI_VO_StopChn(pPrew->mVoPara.mVoLayer, pPrew->mVoPara.mVoChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! VO Stop Chn fail!");
        result = -1;
    }

    ret = AW_MPI_VI_DisableVirChn(pPrew->mViPara.mViDev, pPrew->mViPara.mViChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vipp[%d]chn[%d] disable fail[0x%x]", pPrew->mViPara.mViDev, pPrew->mViPara.mViChn, ret);
        result = -1;
    }

    alogd("%s", ((0==result) ? "success" : "fail"));
    return result;
}

static int releasePreview(PreviewContext *pPrew)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    /* Note:
     * Because the component has been destroyed after the program ends,
     * it is not necessary to unbind Preview Components.
     */
    // result = unbindPreviewComponent(pPrew);
    // if (result != 0)
    // {
    //     aloge("unbind Preview Component fail, result=%d", result);
    // }

    result = destroyVoChn(&pPrew->mVoPara);
    if (result != 0)
    {
        aloge("destroy Vo Chn fail, result=%d", result);
    }

    result = destroyViChn(&pPrew->mViPara);
    if (result != 0)
    {
        aloge("destroy Vi Chn fail, result=%d", result);
    }

    alogd("%s", ((0==result) ? "success" : "fail"));
    return result;
}

/*
 * [play] DEMUX -> VDEC -> VO
 */
static int bindPlayComponent(PlayContext *pPlay)
{
    ERRORTYPE ret = SUCCESS;
    MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pPlay->mDmxPara.mDmxChn};
    MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pPlay->mVdecPara.mVdecChn};
    MPP_CHN_S VoChn = {MOD_ID_VOU, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn};
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pPlay->mClkPara.mClockChn};

    ret = AW_MPI_SYS_Bind(&DmxChn, &VdecChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! demux chn%d & vdec chn%d bind fail", pPlay->mDmxPara.mDmxChn, pPlay->mVdecPara.mVdecChn);
        return -1;
    }
    alogd("demux chn%d & vdec chn%d bind success", pPlay->mDmxPara.mDmxChn, pPlay->mVdecPara.mVdecChn);

    ret = AW_MPI_SYS_Bind(&VdecChn, &VoChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vdec chn%d & vo layer%d chn%d bind fail", pPlay->mVdecPara.mVdecChn, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
        return -1;
    }
    alogd("vdec chn%d & vo layer%d chn%d bind success", pPlay->mVdecPara.mVdecChn, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);

    ret = AW_MPI_SYS_Bind(&ClockChn, &DmxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! clock chn%d & demux chn%d bind fail", pPlay->mClkPara.mClockChn, pPlay->mDmxPara.mDmxChn);
        return -1;
    }
    alogd("clock chn%d & demux chn%d bind success", pPlay->mClkPara.mClockChn, pPlay->mDmxPara.mDmxChn);

    ret = AW_MPI_SYS_Bind(&ClockChn, &VoChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! clock chn%d & vo layer%d chn%d bind fail", pPlay->mClkPara.mClockChn, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
        return -1;
    }
    alogd("clock chn%d & vo layer%d chn%d bind success", pPlay->mClkPara.mClockChn, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);

    return 0;
}

static int unbindPlayComponent(PlayContext *pPlay)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;
    MPP_CHN_S DmxChn = {MOD_ID_DEMUX, 0, pPlay->mDmxPara.mDmxChn};
    MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pPlay->mVdecPara.mVdecChn};
    MPP_CHN_S VoChn = {MOD_ID_VOU, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn};
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, pPlay->mClkPara.mClockChn};

    ret = AW_MPI_SYS_UnBind(&DmxChn, &VdecChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! demux chn%d & vdec chn%d unbind fail", pPlay->mDmxPara.mDmxChn, pPlay->mVdecPara.mVdecChn);
        result = -1;
    }
    alogd("demux chn%d & vdec chn%d unbind success", pPlay->mDmxPara.mDmxChn, pPlay->mVdecPara.mVdecChn);

    ret = AW_MPI_SYS_UnBind(&VdecChn, &VoChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! vdec chn%d & vo layer%d chn%d unbind fail", pPlay->mVdecPara.mVdecChn, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
        result = -1;
    }
    alogd("vdec chn%d & vo layer%d chn%d unbind success", pPlay->mVdecPara.mVdecChn, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);

    ret = AW_MPI_SYS_UnBind(&ClockChn, &DmxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! clock chn%d & demux chn%d unbind fail", pPlay->mClkPara.mClockChn, pPlay->mDmxPara.mDmxChn);
        result = -1;
    }
    alogd("clock chn%d & demux chn%d unbind success", pPlay->mClkPara.mClockChn, pPlay->mDmxPara.mDmxChn);

    ret = AW_MPI_SYS_UnBind(&ClockChn, &VoChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! clock chn%d & vo layer%d chn%d unbind fail", pPlay->mClkPara.mClockChn, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
        result = -1;
    }
    alogd("clock chn%d & vo layer%d chn%d unbind success", pPlay->mClkPara.mClockChn, pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);

    return result;
}

static void _pause(PlayContext *pPlay)
{
    ERRORTYPE ret;
    ret = AW_MPI_DEMUX_Pause(pPlay->mDmxPara.mDmxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! demux pause fail[0x%x]", ret);
    }
    ret = AW_MPI_VO_PauseChn(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo pause fail[0x%x]", ret);
    }
    ret = AW_MPI_CLOCK_Pause(pPlay->mClkPara.mClockChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! clock pause fail[0x%x]", ret);
    }
    ret = AW_MPI_VDEC_Pause(pPlay->mVdecPara.mVdecChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vdec pause fail[0x%x]", ret);
    }
}

static void _start(PlayContext *pPlay)
{
    ERRORTYPE ret;
    ret = AW_MPI_CLOCK_Start(pPlay->mClkPara.mClockChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! clock start fail[0x%x]", ret);
    }
    ret = AW_MPI_VDEC_StartRecvStream(pPlay->mVdecPara.mVdecChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vdec start fail[0x%x]", ret);
    }
    ret = AW_MPI_VO_StartChn(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo start fail[0x%x]", ret);
    }
    ret = AW_MPI_DEMUX_Start(pPlay->mDmxPara.mDmxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! demux start fail[0x%x]", ret);
    }
}

static void _seek_to(PlayContext *pPlay, int msec)
{
    ERRORTYPE ret;
	_pause(pPlay);

    ret = AW_MPI_DEMUX_Seek(pPlay->mDmxPara.mDmxChn, msec);
    if(ret != SUCCESS)
    {
        aloge("fatal error! demux seek fail[0x%x]", ret);
    }
    ret = AW_MPI_VDEC_SetStreamEof(pPlay->mVdecPara.mVdecChn, FALSE);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vdec set eof fail[0x%x]", ret);
    }
    ret = AW_MPI_VO_SetStreamEof(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn, FALSE);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo set eof fail[0x%x]", ret);
    }
    ret = AW_MPI_VO_Seek(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vo seek fail[0x%x]", ret);
    }
    ret = AW_MPI_VDEC_Seek(pPlay->mVdecPara.mVdecChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vdec seek fail[0x%x]", ret);
    }
    ret = AW_MPI_CLOCK_Seek(pPlay->mClkPara.mClockChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! clock seek fail[0x%x]", ret);
    }

	_start(pPlay);

    alogd("seek to %dms done", msec);
}

static void *PlayMsgQueueThread(void *pThreadData)
{
    PlayContext *pPlay = (PlayContext*)pThreadData;
    message_t stCmdMsg;
    CodecParallelMsgType cmd;
    int nCmdPara;

    alogd("msg queue thread start run!");
    while (1)
    {
        if (0 == get_message(&pPlay->mMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case MSGQUEUE_MEDIA_NOTIFY_EOF:
                {
                    if (pPlay->mLoopEnable)
                    {
                        alogd("vo notify eof, loop again");
                        // stop
                        AW_MPI_VO_StopChn(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
                        AW_MPI_VDEC_StopRecvStream(pPlay->mVdecPara.mVdecChn);
                        AW_MPI_DEMUX_Stop(pPlay->mDmxPara.mDmxChn);
                        AW_MPI_CLOCK_Stop(pPlay->mClkPara.mClockChn);
                        // seek 0
                        AW_MPI_DEMUX_Seek(pPlay->mDmxPara.mDmxChn, 0);
                        AW_MPI_VDEC_SetStreamEof(pPlay->mVdecPara.mVdecChn, 0);
                        AW_MPI_VO_SetStreamEof(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn, 0);
                        AW_MPI_VO_Seek(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
                        AW_MPI_VDEC_Seek(pPlay->mVdecPara.mVdecChn);
                        AW_MPI_CLOCK_Seek(pPlay->mClkPara.mClockChn);
                        // start
                        AW_MPI_CLOCK_Start(pPlay->mClkPara.mClockChn);
                        AW_MPI_VDEC_StartRecvStream(pPlay->mVdecPara.mVdecChn);
                        AW_MPI_VO_StartChn(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
                        AW_MPI_DEMUX_Start(pPlay->mDmxPara.mDmxChn);
                    }
                    break;
                }
                case MSGQUEUE_STOP:
                {
                    goto _Exit;
                }
                default:
                {
                    break;
                }
            }
        }
        else
        {
#if 1

            TMessage_WaitQueueNotEmpty(&pPlay->mMsgQueue, 0);
#else
            TMessage_WaitQueueNotEmpty(&pPlay->mMsgQueue, 500);
            static int msec = 2000;
            _seek_to(pPlay, msec);
            msec += 300;
            if(msec > 180*1000)
            {
                msec = 2000;
            }
#endif
        }
    }
_Exit:
    alogd("msg queue thread exit!");
    return NULL;
}

static int preparePlay(PlayContext *pPlay)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    if (message_create(&pPlay->mMsgQueue) < 0)
    {
        aloge("message create fail!");
        return -1;
    }

    configDemuxChn(&pPlay->mDmxPara, &pPlay->mDmxCfg, pPlay->priv);
    result = createDemuxChn(&pPlay->mDmxPara);
    if (result != 0)
    {
        aloge("create demux ch fail, result=%d", result);
        return -1;
    }

    result = configVdecChn(&pPlay->mVdecPara, &pPlay->mDmxPara, &pPlay->mVdecCfg, &pPlay->mVoCfg, pPlay->priv);
    if (result != 0)
	{
		alogd("config vdec ch fail, result=%d", result);
		return -1;
	}
	result = createVdecChn(&pPlay->mVdecPara);
	if (result != 0)
	{
		alogd("create vdec ch fail, result=%d", result);
		return -1;
	}

    configVoChn(&pPlay->mVoPara, &pPlay->mVoCfg, pPlay->priv);
	result = createVoChn(&pPlay->mVoPara);
	if (result != 0)
    {
		alogd("create vo ch fail, result=%d", result);
		return -1;
	}

    configClockChn(&pPlay->mClkPara, NULL, pPlay->priv);
	result = createClockChn(&pPlay->mClkPara);
	if (result != 0)
	{
		alogd("create clock ch fail, result=%d", result);
		return -1;
	}

    result = bindPlayComponent(pPlay);
    if (result != 0)
    {
        alogd("bind Play Component fail, result=%d", result);
		return -1;
    }

    result = pthread_create(&pPlay->mMsgQueueThreadId, NULL, PlayMsgQueueThread, pPlay);
    if (result != 0)
    {
        aloge("fatal error! pthread create fail[%d]", result);
    }

    alogd("success");
	return 0;
}

static int startPlay(PlayContext *pPlay)
{
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    ret = AW_MPI_CLOCK_Start(pPlay->mClkPara.mClockChn);
    if (ret != SUCCESS)
    {
        aloge("mpi clock start fail, ret=%d\n", ret);
        return -1;
    }

    ret = AW_MPI_VDEC_StartRecvStream(pPlay->mVdecPara.mVdecChn);
    if (ret != SUCCESS)
    {
        aloge("mpi vdec start recv stream fail, ret=%d\n", ret);
        return -1;
    }

    AW_MPI_VO_StartChn(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
    if (ret != SUCCESS)
    {
        aloge("mpi vo start chn fail, ret=%d\n", ret);
        return -1;
    }

    ret = AW_MPI_DEMUX_Start(pPlay->mDmxPara.mDmxChn);
    if (ret != SUCCESS)
    {
        aloge("mpi demux start fail, ret=%d\n", ret);
        return -1;
    }

    alogd("success");
    return 0;
}

static int stopPlay(PlayContext *pPlay)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    /* Note:
     * vo stop must before vdec stop.
     * when vo stop, will return buffer to vdec, just all buffer sync.
     */
    ret = AW_MPI_VO_StopChn(pPlay->mVoPara.mVoLayer, pPlay->mVoPara.mVoChn);
    if (ret != SUCCESS)
    {
        aloge("mpi vo stop chn fail, ret=%d\n", ret);
        result = -1;
    }

    ret = AW_MPI_VDEC_StopRecvStream(pPlay->mVdecPara.mVdecChn);
    if (ret != SUCCESS)
    {
        aloge("mpi vdec stop recv stream fail, ret=%d\n", ret);
        result = -1;
    }

    ret = AW_MPI_DEMUX_Stop(pPlay->mDmxPara.mDmxChn);
    if (ret != SUCCESS)
    {
        aloge("mpi demux stop fail, ret=%d\n", ret);
        result = -1;
    }

    ret = AW_MPI_CLOCK_Stop(pPlay->mClkPara.mClockChn);
    if (ret != SUCCESS)
    {
        aloge("mpi clock stop fail, ret=%d\n", ret);
        result = -1;
    }

    alogd("%s", ((0==result) ? "success" : "fail"));
    return result;
}

static int releasePlay(PlayContext *pPlay)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("enter");

    /* Note:
     * Because the component has been destroyed after the program ends,
     * it is not necessary to unbind Play Components.
     */
    // result = bindPlayComponent(pPlay);
    // if (result != 0)
    // {
    //     alogd("bind Play Component fail, result=%d", result);
    // }

    result = destroyVoChn(&pPlay->mVoPara);
    if (result != 0)
    {
        aloge("destroy Vo Chn fail, result=%d", result);
    }

    /* Note:
    * wait hwdisplay kernel driver processing frame buffer, must guarantee this!
    * Then vdec can free frame buffer.
    */
    usleep(50*1000);

    result = destroyVdecChn(&pPlay->mVdecPara);
    if (result != 0)
    {
        aloge("destroy Vdec Chn fail, result=%d", result);
    }

    result = destroyDemuxChn(&pPlay->mDmxPara);
    if (result != 0)
    {
        aloge("destroy Demux Chn fail, result=%d", result);
    }

    result = destroyClockChn(&pPlay->mClkPara);
    if (result != 0)
    {
        aloge("destroy Clock Chn fail, result=%d", result);
    }

    message_t stCmdMsg;
    InitMessage(&stCmdMsg);
    stCmdMsg.command = MSGQUEUE_STOP;
    putMessageWithData(&pPlay->mMsgQueue, &stCmdMsg);
    void *pError = NULL;
    pthread_join(pPlay->mMsgQueueThreadId, (void**)pError);
    message_destroy(&pPlay->mMsgQueue);

    alogd("%s", ((0==result) ? "success" : "fail"));
    return result;
}

static int prepare(SampleCodecParallelContext* pContext)
{
    int ret = 0;

    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

    alogd("enter");

#ifdef SAMPLE_RECORD_TEST
    if (pContext->mRec.mEnable)
    {
        pContext->mRec.priv = pContext;
        ret = prepareRecord(&pContext->mRec);
        if (ret != 0)
        {
            aloge("prepare Record fail, ret=%d", ret);
            return -1;
        }
    }
#endif /* SAMPLE_RECORD_TEST */

#ifdef SAMPLE_RECORD_SUB_TEST
    if (pContext->mSubRec.mEnable)
    {
        pContext->mSubRec.priv = pContext;
        ret = prepareRecord(&pContext->mSubRec);
        if (ret != 0)
        {
            aloge("prepare Record fail, ret=%d", ret);
            return -1;
        }
        //AW_MPI_VENC_EnableIspVeLink(pContext->mSubRec.mVencPara.mVencChn, FALSE);
    }
#endif /* SAMPLE_RECORD_SUB_TEST */

#ifdef SAMPLE_PREVIEW_TEST
    if (pContext->mPrew.mEnable)
    {
        pContext->mPrew.priv = pContext;
        ret = preparePreview(&pContext->mPrew);
        if (ret != 0)
        {
            aloge("prepare Preview fail, ret=%d", ret);
            return -1;
        }
    }
#endif /* SAMPLE_PREVIEW_TEST */

#ifdef SAMPLE_PLAY_TEST
    if (pContext->mPlay.mEnable)
    {
        pContext->mPlay.priv = pContext;
        ret = preparePlay(&pContext->mPlay);
        if (ret != 0)
        {
            aloge("prepare Play fail, ret=%d", ret);
            return -1;
        }
    }
#endif /* SAMPLE_PLAY_TEST */

    alogd("success");
    return 0;
}

static int start(SampleCodecParallelContext* pContext)
{
    int ret = 0;

    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

    alogd("enter");

#ifdef SAMPLE_RECORD_TEST
    if (pContext->mRec.mEnable)
    {
        ret = startRecord(&pContext->mRec);
        if (ret != 0)
        {
            aloge("start record fail, ret=%d", ret);
            return -1;
        }
    }
#endif /* SAMPLE_RECORD_TEST */

#ifdef SAMPLE_RECORD_SUB_TEST
    if (pContext->mSubRec.mEnable)
    {
        ret = startRecord(&pContext->mSubRec);
        if (ret != 0)
        {
            aloge("start record fail, ret=%d", ret);
            return -1;
        }
    }
#endif /* SAMPLE_RECORD_SUB_TEST */

#ifdef SAMPLE_PREVIEW_TEST
    if (pContext->mPrew.mEnable)
    {
        ret = startPreview(&pContext->mPrew);
        if (ret != 0)
        {
            aloge("start preview fail, ret=%d", ret);
            return -1;
        }
    }
#endif /* SAMPLE_PREVIEW_TEST */

#ifdef SAMPLE_PLAY_TEST
    if (pContext->mPlay.mEnable)
    {
        ret = startPlay(&pContext->mPlay);
        if (ret != 0)
        {
            aloge("start Play fail, ret=%d", ret);
            return -1;
        }
    }
#endif /* SAMPLE_PLAY_TEST */

    alogd("success");
    return 0;
}

static int stop(SampleCodecParallelContext* pContext)
{
    int ret = 0;

    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

#ifdef SAMPLE_RECORD_TEST
    if (pContext->mRec.mEnable)
    {
        ret += stopRecord(&pContext->mRec);
    }
#endif /* SAMPLE_RECORD_TEST */

#ifdef SAMPLE_RECORD_SUB_TEST
    if (pContext->mSubRec.mEnable)
    {
        ret += stopRecord(&pContext->mSubRec);
    }
#endif /* SAMPLE_RECORD_SUB_TEST */

#ifdef SAMPLE_PREVIEW_TEST
    if (pContext->mPrew.mEnable)
    {
        ret += stopPreview(&pContext->mPrew);
    }
#endif /* SAMPLE_PREVIEW_TEST */

#ifdef SAMPLE_PLAY_TEST
    if (pContext->mPlay.mEnable)
    {
        ret += stopPlay(&pContext->mPlay);
    }
#endif /* SAMPLE_PLAY_TEST */

    alogd("%s \n", ((0==ret) ? "success" : "fail"));
    return ret;
}

static int release(SampleCodecParallelContext *pContext)
{
    int ret = 0;

    if (NULL == pContext)
    {
        aloge("invalid input param!");
        return -1;
    }

#ifdef SAMPLE_RECORD_TEST
    if (pContext->mRec.mEnable)
    {
        ret += releaseRecord(&pContext->mRec);
    }
#endif /* SAMPLE_RECORD_TEST */

#ifdef SAMPLE_RECORD_SUB_TEST
    if (pContext->mSubRec.mEnable)
    {
        ret += releaseRecord(&pContext->mSubRec);
    }
#endif /* SAMPLE_RECORD_SUB_TEST */

#ifdef SAMPLE_PREVIEW_TEST
    if (pContext->mPrew.mEnable)
    {
        ret += releasePreview(&pContext->mPrew);
    }
#endif /* SAMPLE_PREVIEW_TEST */

#ifdef SAMPLE_PLAY_TEST
    if (pContext->mPlay.mEnable)
    {
        ret += releasePlay(&pContext->mPlay);
    }
#endif /* SAMPLE_PLAY_TEST */

    alogd("%s \n", ((0==ret) ? "success" : "fail"));
    return ret;
}

void handle_exit(int signo)
{
    alogd("user want to exit!");

    if(NULL != gpCodecParallelContext)
    {
        cdx_sem_up(&gpCodecParallelContext->mSemExit);
    }
}

/*
 * Test path
 * [record]  VI(VIPP.0) -> VENC -> MUX
 * [preview] VI(VIPP.1) -> VO
 * [play]    DEMUX -> VDEC -> VO
 */
int main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__)))
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("sample_CodecParallel running!");

    /* malloc CodecParallel Context */
    SampleCodecParallelContext *pContext = (SampleCodecParallelContext *)malloc(sizeof(SampleCodecParallelContext));
    if (NULL == pContext)
    {
        aloge("malloc SampleCodecParallelContext fail, size=%d", sizeof(SampleCodecParallelContext));
        return -1;
    }
    memset(pContext, 0, sizeof(SampleCodecParallelContext));
    gpCodecParallelContext = pContext; // for handle_exit()

    /* init sem */
    cdx_sem_init(&pContext->mSemExit, 0);

    /* parse command line param */
    char *pConfigFilePath = NULL;
    if (parseCmdLine(pContext, argc, argv) != SUCCESS)
    {
        aloge("fatal error! parse cmd line fail");
        result = -1;
        goto err_out_0;
    }
    pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;

    /* parse config file */
    if (loadConfigPara(pContext, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto err_out_0;
    }

    /* register process function for SIGINT, to exit program */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
        perror("can't catch SIGSEGV");

    /* init mpp system */
    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    ret = AW_MPI_SYS_Init();
    if (SUCCESS != ret)
    {
        aloge("fatal error! mpi sys init failed, ret=%d", ret);
        result = -1;
        goto err_out_0;
    }

    if (SUCCESS != prepare(pContext))
    {
        aloge("prepare failed");
        goto err_out_1;
    }

    if (SUCCESS != start(pContext))
    {
        aloge("start fail");
        result = -1;
        goto err_out_2;
    }

    /* wait for test done */
    if (pContext->mTestDuration > 0)
    {
        alogd("The test time is %d s, continues ...", pContext->mTestDuration);
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mTestDuration*1000);
        alogd("The test time is up, end the test.");
    }
    else
    {
        alogd("No test time is specified, you need to pass 'ctrl+c' to exit the test.");
        cdx_sem_down(&pContext->mSemExit);
    }

err_out_2:
    if (SUCCESS != stop(pContext))
    {
        aloge("stop fail");
        result = -1;
    }

err_out_1:
    release(pContext);
    AW_MPI_SYS_Exit();

err_out_0:
    if (pContext)
    {
        cdx_sem_deinit(&pContext->mSemExit);
        free(pContext);
        pContext = NULL;
    }

    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

