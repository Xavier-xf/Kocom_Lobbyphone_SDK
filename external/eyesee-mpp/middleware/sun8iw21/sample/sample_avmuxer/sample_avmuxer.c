/** @file
  vi -> venc -> muxer
  ai -> aenc ->
  
  @author eric_wang in PDC-PD5
  @date 2024-06-14
*/

//#define LOG_NDEBUG 0
//#define LOG_TAG "SampleAVMuxer"
#include "plat_log.h"

#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <mm_common.h>
#include <media_common_aio.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_region.h>
#include <aenc_sw_lib.h>
#include <confparser.h>

#include "sample_common_venc.h"
#include <file_common.h>
#include "sample_avmuxer.h"
#include "sample_avmuxer_conf.h"

#include <cdx_list.h>

#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)
#define ISP_RUN (1)

static SampleAVMuxerContext *gpSampleAVMuxerContext;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleAVMuxerContext)
    {
        cdx_sem_up(&gpSampleAVMuxerContext->stSemExit);
    }
}

static int InitSampleAVMuxerContext(SampleAVMuxerContext *pContext)
{
    int ret;
    memset(pContext, 0, sizeof(SampleAVMuxerContext));
    pContext->nMuxChn = MM_INVALID_CHN;
    pContext->nVeChn = MM_INVALID_CHN;
    pContext->nViChn = MM_INVALID_CHN;
    pContext->nViDev = MM_INVALID_DEV;
    pContext->nAudioDevId = MM_INVALID_DEV;
    pContext->nAiChn= MM_INVALID_CHN;
    pContext->nAEncChn = MM_INVALID_CHN;

    INIT_LIST_HEAD(&pContext->stMuxerFileList);

    ret = cdx_sem_init(&pContext->stSemExit, 0);
    if(ret != 0)
    {
        aloge("fatal error! cdx sem init fail:%d", ret);
    }
    ret = message_create(&pContext->stMsgQueue);
    if(ret != 0)
    {
        aloge("fatal error! message create fail:%d!", ret);
    }

    return SUCCESS;
}

static int parseCmdLine(SampleAVMuxerContext *pContext, int argc, char** argv)
{
    int ret = FAILURE;

    if(argc <= 1)
    {
        alogd("use default config.");
        return SUCCESS;
    }
    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = SUCCESS;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pContext->stCmdLinePara.strConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pContext->stCmdLinePara.strConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_vi2venc2muxer.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static PIXEL_FORMAT_E convertPixelFormatStringToPIXEL_FORMAT_E(char *pStrPixelFormat)
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

static enum v4l2_colorspace convertColorSpaceStringTov4l2_colorspace(char *pStrColorSpace)
{
    enum v4l2_colorspace eColorSpace;
    if (!strcmp(pStrColorSpace, "jpeg"))
    {
        eColorSpace = V4L2_COLORSPACE_JPEG;
    }
    else if (!strcmp(pStrColorSpace, "rec709"))
    {
        eColorSpace = V4L2_COLORSPACE_REC709;
    }
    else if (!strcmp(pStrColorSpace, "rec709_part_range"))
    {
        eColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
    }
    else
    {
        aloge("fatal error! wrong color space:%s", pStrColorSpace);
        eColorSpace = V4L2_COLORSPACE_JPEG;
    }
    return eColorSpace;
}

static PAYLOAD_TYPE_E convertCodecTypeStringToPAYLOAD_TYPE_E(char *pStrCodecType)
{
    PAYLOAD_TYPE_E ePayloadType;
    if (!strcmp(pStrCodecType, "H.264"))
    {
        ePayloadType = PT_H264;
    }
    else if (!strcmp(pStrCodecType, "H.265"))
    {
        ePayloadType = PT_H265;
    }
    else if (!strcmp(pStrCodecType, "MJPEG"))
    {
        ePayloadType = PT_MJPEG;
    }
    else if (!strcmp(pStrCodecType, "aac"))
    {
        ePayloadType = PT_AAC;
    }
    else if (!strcmp(pStrCodecType, "mp3"))
    {
        ePayloadType = PT_MP3;
    }
    else if (!strcmp(pStrCodecType, "pcm"))
    {
        ePayloadType = PT_PCM_AUDIO;
    }
    else if (!strcmp(pStrCodecType, "g711a"))
    {
        ePayloadType = PT_G711A;
    }
    else if (!strcmp(pStrCodecType, "g711u"))
    {
        ePayloadType = PT_G711U;
    }
    else
    {
        aloge("fatal error! unknown codec type:%s", pStrCodecType);
        ePayloadType = PT_BUTT;
    }
    return ePayloadType;
}

static VENC_REF_FRAME_LBC_MODE_E convertLBCStringToVENC_REF_FRAME_LBC_MODE_E(char *pStrLBC)
{
    VENC_REF_FRAME_LBC_MODE_E eVencRefFrameLbcMode;
    if(!strcmp(pStrLBC, "aw_lbc_1_5x"))
    {
        eVencRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_1_5X;
    }
    else if(!strcmp(pStrLBC, "aw_lbc_2_0x"))
    {
        eVencRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_2_0X;
    }
    else if(!strcmp(pStrLBC, "aw_lbc_2_5x"))
    {
        eVencRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_2_5X;
    }
    else if(!strcmp(pStrLBC, "aw_lbc_no_lossy"))
    {
        eVencRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_NO_LOSSY;
    }
    else
    {
        aloge("fatal error! unknown Lbc string:%s", pStrLBC);
        eVencRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT;
    }
    return eVencRefFrameLbcMode;
}        

static MEDIA_FILE_FORMAT_E convertSuffixStringToMEDIA_FILE_FORMAT_E(char *pStrSuffix)
{
    MEDIA_FILE_FORMAT_E eFileFormat;
    if(!strcmp(".mp4", pStrSuffix))
    {
        eFileFormat = MEDIA_FILE_FORMAT_MP4;
    }
    else if(!strcmp(".ts", pStrSuffix))
    {
        eFileFormat = MEDIA_FILE_FORMAT_TS;
    }
    else
    {
        alogd("unknown file suffix:[%s], use raw file format", pStrSuffix);
        eFileFormat = MEDIA_FILE_FORMAT_RAW;
    }
    return eFileFormat;
}

static int loadConfigPara(SampleAVMuxerContext *pContext, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;

    if (conf_path != NULL)
    {
        CONFPARSER_S stConf;
        memset(&stConf, 0, sizeof(CONFPARSER_S));
        ret = createConfParser(conf_path, &stConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }

        pContext->stConfigPara.bOnlineEnable = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_ONLINE_EN, 0);
        pContext->stConfigPara.nOnlineShareBufNum = GetConfParaInt(&stConf, SAMPLE_AVMUXER_ONLINE_SHARE_BUF_NUM, 0);
        pContext->stConfigPara.nVippDev = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VIPP_ID, 0);
        pContext->stConfigPara.wdr_en = GetConfParaInt(&stConf, SAMPLE_AVMUXER_WDR_EN, 0);
        pContext->stConfigPara.nViDropFrameNum = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VI_DROP_FRM_NUM, 0);
        pContext->stConfigPara.nVencDropFrameNum = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VENC_DROP_FRM_NUM, 0);
        pContext->stConfigPara.srcWidth = GetConfParaInt(&stConf, SAMPLE_AVMUXER_SRC_WIDTH, 0);
        pContext->stConfigPara.srcHeight = GetConfParaInt(&stConf, SAMPLE_AVMUXER_SRC_HEIGHT, 0);
        pContext->stConfigPara.nSrcFrameRate = GetConfParaInt(&stConf, SAMPLE_AVMUXER_SRC_FRAMERATE, 0);
        pContext->stConfigPara.nViBufferNum = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VI_BUFFER_NUM, 0);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_AVMUXER_SRC_PIXFMT, NULL);
        pContext->stConfigPara.eSrcPixFmt = convertPixelFormatStringToPIXEL_FORMAT_E(ptr);
        ptr = (char*)GetConfParaString(&stConf, SAMPLE_AVMUXER_COLOR_SPACE, NULL);
        pContext->stConfigPara.eColorSpace = convertColorSpaceStringTov4l2_colorspace(ptr);
        pContext->stConfigPara.nVeChn = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VENC_CH_ID, 0);
        ptr = (char *)GetConfParaString(&stConf, SAMPLE_AVMUXER_VIDEO_DST_FILE, NULL);
        if (ptr != NULL)
        {
            strcpy(pContext->stConfigPara.dstVideoFile, ptr);
        }
        pContext->stConfigPara.bAddRepairInfo = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_ADD_REPAIR_INFO, 0);
        pContext->stConfigPara.nMaxFrmsTagInterval = GetConfParaInt(&stConf, SAMPLE_AVMUXER_FRMSTAG_BACKUP_INTERVAL, 0);
        pContext->stConfigPara.nDstFileMaxCnt = GetConfParaInt(&stConf, SAMPLE_AVMUXER_DST_FILE_MAX_CNT, 0);
        pContext->stConfigPara.nMaxFileDuration = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VIDEO_DURATION, 0);
        pContext->stConfigPara.nVideoFrameRate = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VIDEO_FRAMERATE, 0);
        pContext->stConfigPara.nVideoBitRate = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VIDEO_BITRATE, 0);
        pContext->stConfigPara.dstWidth = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VIDEO_WIDTH, 0);
        pContext->stConfigPara.dstHeight = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VIDEO_HEIGHT, 0);
        ptr = (char *)GetConfParaString(&stConf, SAMPLE_AVMUXER_VIDEO_ENCODER, NULL);
        pContext->stConfigPara.eVideoEncoderFmt = convertCodecTypeStringToPAYLOAD_TYPE_E(ptr);
        pContext->stConfigPara.nEncUseProfile = GetConfParaInt(&stConf, SAMPLE_AVMUXER_PROFILE, 0);
        ptr = (char *)GetConfParaString(&stConf, SAMPLE_AVMUXER_VE_REF_LBC_MODE, NULL);
        pContext->stConfigPara.eVeRefFrameLbcMode = convertLBCStringToVENC_REF_FRAME_LBC_MODE_E(ptr);
        pContext->stConfigPara.eProductMode = (eVencProductMode)GetConfParaInt(&stConf, SAMPLE_AVMUXER_PRODUCT_MODE, 0);
        pContext->stConfigPara.nKeyFrameInterval = GetConfParaInt(&stConf, SAMPLE_AVMUXER_KEY_FRAME_INTERVAL, 0);
        pContext->stConfigPara.bVbrOptEnable = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_VBR_OPT_EN, 0);
        pContext->stConfigPara.nRcMode = GetConfParaInt(&stConf, SAMPLE_AVMUXER_RC_MODE, 0);
        pContext->stConfigPara.nGopMode = GetConfParaInt(&stConf, SAMPLE_AVMUXER_GOP_MODE, 0);
        pContext->stConfigPara.nGopSize = GetConfParaInt(&stConf, SAMPLE_AVMUXER_GOP_SIZE, 0);
        pContext->stConfigPara.nVbvBufferSize = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VBVBUFFERSIZE, 0);
        pContext->stConfigPara.nVbvThreshSize = GetConfParaInt(&stConf, SAMPLE_AVMUXER_VBVTHRESHSIZE, 0);
        pContext->stConfigPara.bCropEnable = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_CROP_EN, 0);
        pContext->stConfigPara.nCropRectX = GetConfParaInt(&stConf, SAMPLE_AVMUXER_CROP_RECT_X, 0);
        pContext->stConfigPara.nCropRectY = GetConfParaInt(&stConf, SAMPLE_AVMUXER_CROP_RECT_Y, 0);
        pContext->stConfigPara.nCropRectWidth = GetConfParaInt(&stConf, SAMPLE_AVMUXER_CROP_RECT_W, 0);
        pContext->stConfigPara.nCropRectHeight = GetConfParaInt(&stConf, SAMPLE_AVMUXER_CROP_RECT_H, 0);
        pContext->stConfigPara.bVuiTimingInfoPresentFlag = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_VUI_TIMING_INFO_PRESENT_FLAG, 0);
        pContext->stConfigPara.bEncppEnable = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_ENCPP_ENABLE, 0);
        pContext->stConfigPara.bIspAndVeLinkageEnable = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_ISP_VE_LINKAGE_ENABLE, 0);
        pContext->stConfigPara.bVeRecRefBufReduceEnable = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_VE_REC_REF_BUF_REDUCE_ENABLE, 0);
        pContext->stConfigPara.eSeiEnable = (VencSeiEnableSettingE)GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_ENABLE, 0);
        pContext->stConfigPara.bSeiDataIsp = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_DATA_ISP, 0);
        pContext->stConfigPara.bSeiDataVipp = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_DATA_VIPP, 0);
        pContext->stConfigPara.bSeiDataVenc = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_DATA_VENC, 0);
        pContext->stConfigPara.nSeiFrameIntervalIspLevel1 = GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_FRAME_INTERVAL_ISPLEVEL1, 0);
        pContext->stConfigPara.nSeiFrameIntervalIspLevel2 = GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_FRAME_INTERVAL_ISPLEVEL2, 0);
        pContext->stConfigPara.nSeiFrameIntervalIspLevel3 = GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_FRAME_INTERVAL_ISPLEVEL3, 0);
        pContext->stConfigPara.nSeiFrameIntervalVipp = GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_FRAME_INTERVAL_VIPP, 0);
        pContext->stConfigPara.nSeiFrameIntervalVencLevel1 = GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_FRAME_INTERVAL_VENCLEVEL1, 0);
        pContext->stConfigPara.nSeiFrameIntervalVencLevel2 = GetConfParaInt(&stConf, SAMPLE_AVMUXER_SEI_FRAME_INTERVAL_VENCLEVEL2, 0);
        pContext->stConfigPara.nPcmChnCnt = GetConfParaInt(&stConf, SAMPLE_AVMUXER_PCM_CHN_CNT, 0);
        pContext->stConfigPara.nPcmBitWidth = GetConfParaInt(&stConf, SAMPLE_AVMUXER_PCM_BIT_WIDTH, 0);
        pContext->stConfigPara.nPcmSampleRate = GetConfParaInt(&stConf, SAMPLE_AVMUXER_PCM_SAMPLE_RATE, 0);
        pContext->stConfigPara.nAiVolume = GetConfParaInt(&stConf, SAMPLE_AVMUXER_AI_VOLUME, 0);
        pContext->stConfigPara.bAecEn = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_AEC_EN, 0);
        pContext->stConfigPara.nAecNlpMode = GetConfParaInt(&stConf, SAMPLE_AVMUXER_AEC_NLP_MODE, 0);
        pContext->stConfigPara.bAnsEn = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_ANS_EN, 0);
        pContext->stConfigPara.nAnsMode = GetConfParaInt(&stConf, SAMPLE_AVMUXER_ANS_MODE, 0);
        pContext->stConfigPara.bAgcEn = (bool)GetConfParaInt(&stConf, SAMPLE_AVMUXER_AGC_EN, 0);
        pContext->stConfigPara.nAgcTargetDb = GetConfParaInt(&stConf, SAMPLE_AVMUXER_AGC_TARGET_DB, 0);
        pContext->stConfigPara.nAgcMaxGainDb = GetConfParaInt(&stConf, SAMPLE_AVMUXER_AGC_MAX_GAIN_DB, 0);
        ptr = (char *)GetConfParaString(&stConf, SAMPLE_AVMUXER_AUDIO_CODEC_TYPE, NULL);
        pContext->stConfigPara.eAudioCodecType = convertCodecTypeStringToPAYLOAD_TYPE_E(ptr);
        pContext->stConfigPara.nBitrate = GetConfParaInt(&stConf, SAMPLE_AVMUXER_BITRATE, 0);
        pContext->stConfigPara.nTestDuration = GetConfParaInt(&stConf, SAMPLE_AVMUXER_TEST_DURATION, 0);

        destroyConfParser(&stConf);
    }

    //parse dst directory from dst file path.
    char *pLastSlash = strrchr(pContext->stConfigPara.dstVideoFile, '/');
    if(pLastSlash != NULL)
    {
        int dirLen = pLastSlash-pContext->stConfigPara.dstVideoFile;
        strncpy(pContext->strDstDir, pContext->stConfigPara.dstVideoFile, dirLen);
        pContext->strDstDir[dirLen] = '\0';
        
        char *pFileName = pLastSlash+1;
        strcpy(pContext->strFirstFileName, pFileName);
    }
    else
    {
        strcpy(pContext->strDstDir, "");
        strcpy(pContext->strFirstFileName, pContext->stConfigPara.dstVideoFile);
    }

    //get file format from suffix of file name.
    char *pLastDot = strrchr(pContext->stConfigPara.dstVideoFile, '.');
    if(pLastDot != NULL)
    {
        pContext->eFileFormat = convertSuffixStringToMEDIA_FILE_FORMAT_E(pLastDot);
    }
    else
    {
        pContext->eFileFormat = MEDIA_FILE_FORMAT_RAW;
    }
    return SUCCESS;
}

static int getFileNameByFileCnt(SampleAVMuxerContext *pContext, char *pNameBuf)
{
    static int file_cnt = 0;
    char strStemPath[MAX_FILE_PATH_LEN] = {0};
    int len = strlen(pContext->stConfigPara.dstVideoFile);
    char *ptr = pContext->stConfigPara.dstVideoFile;
    while (*(ptr+len-1) != '.')
    {
        len--;
    }

    ++file_cnt;
    strncpy(strStemPath, pContext->stConfigPara.dstVideoFile, len-1);
    sprintf(pNameBuf, "%s_%d.%s", strStemPath, file_cnt, (MEDIA_FILE_FORMAT_TS==pContext->eFileFormat)?"ts":"mp4");
    return 0;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleAVMuxerContext *pContext = (SampleAVMuxerContext *)cookie;
    ERRORTYPE ret = SUCCESS;
    int rc = 0;

    if (MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                aloge("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                stCmdMsg.command = Vi_Timeout;
                stCmdMsg.mDataSize = 0;
                stCmdMsg.mpData = NULL;
                putMessageWithData(&pContext->stMsgQueue, &stCmdMsg);
                break;
            }
    	    default:
    	    {
    		    aloge("fatal error! unknow event type[0x%x]", event);
    		    break;
    	    }
        }
    }
    else if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN nVEncChn = pChn->mChnId;
        switch(event)
        {
            /*case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                Isp2VeLinkageParam stIsp2Ve;
                memset(&stIsp2Ve, 0, sizeof(Isp2VeLinkageParam));
                stIsp2Ve.mIspAndVeLinkageEnable = pContext->stConfigPara.bIspAndVeLinkageEnable;
                stIsp2Ve.mCameraAdaptiveMovingAndStaticEnable = FALSE;
                stIsp2Ve.mVEncChn = nVEncChn;
                stIsp2Ve.mVipp = pContext->nViDev;
                stIsp2Ve.pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                stIsp2Ve.nEncppSharpAttenCoefPer = 100;
                rc = setIsp2VeLinkageParam(&stIsp2Ve);
                if (rc)
                {
                    aloge("fatal error, VeChn[%d] set Isp2VeLinkageParam failed! ret=%d", nVEncChn, rc);
                    ret = FAILURE;
                }
                break;
            }
            case MPP_EVENT_LINKAGE_VE2ISP_PARAM:
            {
                Ve2IspLinkageParam stVe2Isp;
                memset(&stVe2Isp, 0, sizeof(Ve2IspLinkageParam));
                stVe2Isp.mIspAndVeLinkageEnable = pContext->stConfigPara.bIspAndVeLinkageEnable;
                stVe2Isp.mVEncChn = nVEncChn;
                stVe2Isp.mVipp = pContext->nViDev;
                stVe2Isp.p2Ve2IspParam = (VencVe2IspParam *)pEventData;
                int ret = setVe2IspLinkageParam(&stVe2Isp);
                if (ret)
                {
                    aloge("fatal error! VeChn[%d] set Ve2IspLinkageParam failed! ret=%d", nVEncChn, ret);
                    ret = FAILURE;
                }
                break;
            }*/
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM_EXTRA:
            {
                VENC_Isp2VeExtraParam *pExtraParam = (VENC_Isp2VeExtraParam *)pEventData;
                pExtraParam->eEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                break;
            }
            case MPP_EVENT_VENC_BUFFER_FULL:
            {
                alogw("VeChn[%d] vbv buffer full", pChn->mChnId);
                break;
            }
            case MPP_EVENT_DROP_FRAME:
            {
                alogd("VeChn[%d] receive dropFrame message", pChn->mChnId);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if (MOD_ID_AI == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_CAPTURE_AUDIO_DATA:
            {
                AISendDataInfo *pAiDataInfo = (AISendDataInfo*)pEventData;
                //alogd("aiChn[%d] capture audio data:%lldus-%d-%d", pChn->mChnId, pAiDataInfo->mPts, pAiDataInfo->mLen, pAiDataInfo->mbIgnore);
                break;
            }
            default:
            {
                aloge("fatal error! receive aiChn[%d] event[%d]", pChn->mChnId, event);
                break;
            }
        }
    }
    else if (MOD_ID_AENC == pChn->mModId)
    {
        aloge("fatal error! receive aencChn[%d] event[%d]", pChn->mChnId, event);
    }
    else if (MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                MUX_CHN nMuxChn = (MUX_CHN)(*(int*)pEventData);
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                alogd("MuxChn[%d] record file done.", nMuxChn);
                stCmdMsg.command = Rec_FileDone;
                stCmdMsg.para0 = nMuxChn;
                stCmdMsg.mDataSize = 0;
                stCmdMsg.mpData = NULL;
                putMessageWithData(&pContext->stMsgQueue, &stCmdMsg);  
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                MUX_CHN nMuxChn = (MUX_CHN)*(int*)pEventData;
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                alogd("MuxChn[%d] need next fd.", nMuxChn);
                stCmdMsg.command = Rec_NeedSetNextFd;
                stCmdMsg.para0 = nMuxChn;
                stCmdMsg.mDataSize = 0;
                stCmdMsg.mpData = NULL;
                putMessageWithData(&pContext->stMsgQueue, &stCmdMsg);  
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("muxChn[%d] bs frame available", pChn->mChnId);
                break;
            }
            default:
            {
                aloge("fatal error! muxChn[%d] receive mux known event:%d", pChn->mChnId, event);
                break;
            }
        }
    }
    return ret;
}

static int configMuxChnAttr(SampleAVMuxerContext *pContext)
{
    memset(&pContext->stMuxChnAttr, 0, sizeof(MUX_CHN_ATTR_S));

    pContext->stMuxChnAttr.mVideoAttrValidNum = 1;
    pContext->stMuxChnAttr.mVideoAttr[0].mWidth = pContext->stConfigPara.dstWidth;
    pContext->stMuxChnAttr.mVideoAttr[0].mHeight = pContext->stConfigPara.dstHeight;
    pContext->stMuxChnAttr.mVideoAttr[0].mVideoFrmRate = pContext->stConfigPara.nVideoFrameRate*1000;
    pContext->stMuxChnAttr.mVideoAttr[0].mVideoEncodeType = pContext->stConfigPara.eVideoEncoderFmt;
    pContext->stMuxChnAttr.mVideoAttr[0].mVeChn = pContext->nVeChn;
    
    pContext->stMuxChnAttr.mChannels = pContext->stConfigPara.nPcmChnCnt;
    pContext->stMuxChnAttr.mBitsPerSample = pContext->stConfigPara.nPcmBitWidth;
    pContext->stMuxChnAttr.mSamplesPerFrame = MAXDECODESAMPLE;
    pContext->stMuxChnAttr.mSampleRate = pContext->stConfigPara.nPcmSampleRate;
    pContext->stMuxChnAttr.mAudioEncodeType = pContext->stConfigPara.eAudioCodecType;

    pContext->stMuxChnAttr.mTextEncodeType = PT_MAX;

    pContext->stMuxChnAttr.mMediaFileFormat = pContext->eFileFormat;
    pContext->stMuxChnAttr.mMaxFileDuration = pContext->stConfigPara.nMaxFileDuration*1000;
    pContext->stMuxChnAttr.mCallbackOutFlag = FALSE;
    pContext->stMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pContext->stMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
    pContext->stMuxChnAttr.mAddRepairInfo = (int)pContext->stConfigPara.bAddRepairInfo;
    pContext->stMuxChnAttr.mMaxFrmsTagInterval = pContext->stConfigPara.nMaxFrmsTagInterval;

    return SUCCESS;
}

static int createMuxChn(SampleAVMuxerContext *pContext)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;

    configMuxChnAttr(pContext);
    int nFd = open(pContext->stConfigPara.dstVideoFile, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (nFd < 0)
    {
        aloge("fatal error! Failed to open %s", pContext->stConfigPara.dstVideoFile);
        return -1;
    }
    int nFallocateLen = 0;
    pContext->nMuxChn = 0;
    while (pContext->nMuxChn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(pContext->nMuxChn, &pContext->stMuxChnAttr, nFd, nFallocateLen);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create muxChn[%d] success!", pContext->nMuxChn);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("muxChn[%d] is exist, find next!", pContext->nMuxChn);
            pContext->nMuxChn++;
        }
        else
        {
            aloge("fatal error! create muxChn[%d] ret[0x%x], find next!", pContext->nMuxChn, ret);
            break;
        }
    }

    if(nFd >= 0)
    {
        close(nFd);
        nFd = -1;
    }
    if (FALSE == bSuccessFlag)
    {
        pContext->nMuxChn = MM_INVALID_CHN;
        aloge("fatal error! create mux channel fail!");
        return FAILURE;
    }
    else
    {
        RecordFileDurationPolicy ePolicy = RecordFileDurationPolicy_MinDuration;
        ret = AW_MPI_MUX_SetSwitchFileDurationPolicy(pContext->nMuxChn, ePolicy);
        if(ret != SUCCESS)
        {
            aloge("fatal error! muxChn[%d] set file policy[%d]", pContext->nMuxChn, ePolicy);
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pContext->nMuxChn, &cbInfo);
        return SUCCESS;
    }
}

void configAioAttrForAI(SampleAVMuxerContext *pContext)
{
    pContext->stAioAttr.enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(pContext->stConfigPara.nPcmSampleRate);
    pContext->stAioAttr.enBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(pContext->stConfigPara.nPcmBitWidth);
    pContext->stAioAttr.enSoundmode = AUDIO_SOUND_MODE_MONO;
    pContext->stAioAttr.mChnCnt = pContext->stConfigPara.nPcmChnCnt;
    pContext->stAioAttr.mMicNum = 1;

    pContext->stAioAttr.ai_aec_en = (int)pContext->stConfigPara.bAecEn;
    pContext->stAioAttr.aec_delay_ms = 0;
    pContext->stAioAttr.mAecNlpMode = pContext->stConfigPara.nAecNlpMode;
    pContext->stAioAttr.mbBypassAec = 0;
    pContext->stAioAttr.ai_ans_en = (int)pContext->stConfigPara.bAnsEn;
    pContext->stAioAttr.ai_ans_mode = pContext->stConfigPara.nAnsMode;
    pContext->stAioAttr.ai_agc_en = (int)pContext->stConfigPara.bAgcEn;
    pContext->stAioAttr.ai_agc_float_cfg.fTargetDb = pContext->stConfigPara.nAgcTargetDb;
    pContext->stAioAttr.ai_agc_float_cfg.fMaxGainDb = pContext->stConfigPara.nAgcMaxGainDb;
}

static int createAIChn(SampleAVMuxerContext *pContext)
{
    int result = 0;
    configAioAttrForAI(pContext);
    //enable audio_hw_ai
    AW_MPI_AI_SetPubAttr(pContext->nAudioDevId, &pContext->stAioAttr);
    AW_MPI_AI_Enable(pContext->nAudioDevId);
    AW_MPI_AI_SetDevVolume(pContext->nAudioDevId, pContext->stConfigPara.nAiVolume);

    BOOL bSuccessFlag = FALSE;
    ERRORTYPE ret;
    pContext->nAiChn = 0;
    while (pContext->nAiChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(pContext->nAudioDevId, pContext->nAiChn, NULL);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", pContext->nAiChn);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogw("ai channel[%d] exist, try next", pContext->nAiChn);
            pContext->nAiChn++;
        }
        else if (ERR_AI_NOT_ENABLED == ret)
        {
            aloge("fatal error! audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("fatal error! create ai channel[%d] fail! ret[0x%x]!", pContext->nAiChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        pContext->nAiChn = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        result = -1;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_AI_RegisterCallback(pContext->nAudioDevId, pContext->nAiChn, &cbInfo);
    }
    return result;
}

static void configAEncAttr(SampleAVMuxerContext *pContext)
{
    pContext->stAEncChnAttr.AeAttr.Type = pContext->stConfigPara.eAudioCodecType;
    pContext->stAEncChnAttr.AeAttr.sampleRate = pContext->stConfigPara.nPcmSampleRate;
    pContext->stAEncChnAttr.AeAttr.channels = pContext->stConfigPara.nPcmChnCnt;
    pContext->stAEncChnAttr.AeAttr.bitRate = pContext->stConfigPara.nBitrate;
    pContext->stAEncChnAttr.AeAttr.bitsPerSample = pContext->stConfigPara.nPcmBitWidth;
    pContext->stAEncChnAttr.AeAttr.attachAACHeader = 0; //aacMuxer will add adts header, so aac encoder need not attach aac header.
    pContext->stAEncChnAttr.AeAttr.mInBufSize = 0;
    pContext->stAEncChnAttr.AeAttr.mOutBufCnt = 0;
}

static int createAEncChn(SampleAVMuxerContext *pContext)
{
    int result = 0;
    configAEncAttr(pContext);
    BOOL bSuccessFlag = FALSE;
    ERRORTYPE ret;
    pContext->nAEncChn = 0;
    while (pContext->nAEncChn < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(pContext->nAEncChn, &pContext->stAEncChnAttr);
        if (SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", pContext->nAEncChn);
            break;
        }
        else if (ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", pContext->nAEncChn);
            pContext->nAEncChn++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[0x%x], find next!", pContext->nAEncChn, ret);
            break;
        }
    }
    if (FALSE == bSuccessFlag)
    {
        pContext->nAEncChn = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        result = -1;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_AENC_RegisterCallback(pContext->nAEncChn, &cbInfo);
    }
    return result;
}

static ERRORTYPE configVencChnAttr(SampleAVMuxerContext *pContext)
{
    memset(&pContext->stVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    if (pContext->stConfigPara.bOnlineEnable)
    {
        pContext->stVencChnAttr.VeAttr.mOnlineEnable = 1;
        pContext->stVencChnAttr.VeAttr.mOnlineShareBufNum = pContext->stConfigPara.nOnlineShareBufNum;
    }
    pContext->stVencChnAttr.VeAttr.Type = pContext->stConfigPara.eVideoEncoderFmt;
    pContext->stVencChnAttr.VeAttr.MaxKeyInterval = pContext->stConfigPara.nKeyFrameInterval;
    pContext->stVencChnAttr.VeAttr.SrcPicWidth  = pContext->stConfigPara.srcWidth;
    pContext->stVencChnAttr.VeAttr.SrcPicHeight = pContext->stConfigPara.srcHeight;
    pContext->stVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->stVencChnAttr.VeAttr.PixelFormat = pContext->stConfigPara.eSrcPixFmt;
    pContext->stVencChnAttr.VeAttr.mColorSpace = pContext->stConfigPara.eColorSpace;
    alogd("pixfmt:0x%x, colorSpace:0x%x", pContext->stVencChnAttr.VeAttr.PixelFormat, pContext->stVencChnAttr.VeAttr.mColorSpace);
    pContext->stVencChnAttr.VeAttr.mDropFrameNum = pContext->stConfigPara.nVencDropFrameNum;
    alogd("DropFrameNum:%d", pContext->stVencChnAttr.VeAttr.mDropFrameNum);
    pContext->stVencChnAttr.VeAttr.mVeRefFrameLbcMode = pContext->stConfigPara.eVeRefFrameLbcMode;
    alogd("VeRefFrameLbcMode:%d", pContext->stVencChnAttr.VeAttr.mVeRefFrameLbcMode);
    pContext->stVencChnAttr.VeAttr.mVeRecRefBufReduceEnable = pContext->stConfigPara.bVeRecRefBufReduceEnable;
    alogd("VeRecRefBufReduceEnable:%d", pContext->stVencChnAttr.VeAttr.mVeRecRefBufReduceEnable);
    pContext->stVencChnAttr.VeAttr.mVbrOptEnable = pContext->stConfigPara.bVbrOptEnable;
    alogd("VbrOptEnable:%d", pContext->stVencChnAttr.VeAttr.mVbrOptEnable);
    if (PT_H264 == pContext->stVencChnAttr.VeAttr.Type)
    {
        pContext->stVencChnAttr.VeAttr.AttrH264e.BufSize = pContext->stConfigPara.nVbvBufferSize;
        pContext->stVencChnAttr.VeAttr.AttrH264e.mThreshSize = pContext->stConfigPara.nVbvThreshSize;
        pContext->stVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->stVencChnAttr.VeAttr.AttrH264e.Profile = pContext->stConfigPara.nEncUseProfile;
        pContext->stVencChnAttr.VeAttr.AttrH264e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->stConfigPara.dstWidth;
        pContext->stVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->stConfigPara.dstHeight;
        pContext->stVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
    }
    else if (PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
    {
        pContext->stVencChnAttr.VeAttr.AttrH265e.mBufSize = pContext->stConfigPara.nVbvBufferSize;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mThreshSize = pContext->stConfigPara.nVbvThreshSize;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mProfile = pContext->stConfigPara.nEncUseProfile;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->stConfigPara.dstWidth;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->stConfigPara.dstHeight;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
    }
    else if (PT_MJPEG == pContext->stVencChnAttr.VeAttr.Type)
    {
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mBufSize = pContext->stConfigPara.nVbvBufferSize;
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = pContext->stConfigPara.nVbvThreshSize;
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->stConfigPara.dstWidth;
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->stConfigPara.dstHeight;
    }

    pContext->stVencChnAttr.RcAttr.mProductMode = pContext->stConfigPara.eProductMode;
    if (PT_H264 == pContext->stVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfigPara.nRcMode)
        {
        case 1:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pContext->stVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->stConfigPara.nVideoBitRate;
            pContext->stVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pContext->stConfigPara.nSrcFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = pContext->stConfigPara.nVideoFrameRate;
            break;
        case 0:
        default:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->stVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->stConfigPara.nVideoBitRate;
            pContext->stVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pContext->stConfigPara.nSrcFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = pContext->stConfigPara.nVideoFrameRate;
            break;
        }
    }
    else if (PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfigPara.nRcMode)
        {
        case 1:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->stVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->stConfigPara.nVideoBitRate;
            pContext->stVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pContext->stConfigPara.nSrcFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = pContext->stConfigPara.nVideoFrameRate;
            break;
        case 0:
        default:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->stVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->stConfigPara.nVideoBitRate;
            pContext->stVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pContext->stConfigPara.nSrcFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = pContext->stConfigPara.nVideoFrameRate;
            break;
        }
    }
    else if (PT_MJPEG == pContext->stVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfigPara.nRcMode)
        {
        case 2:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            break;
        case 0:
        default:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->stConfigPara.nVideoBitRate;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pContext->stConfigPara.nSrcFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = pContext->stConfigPara.nVideoFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.bitRateMax = (int)(pContext->stConfigPara.nVideoBitRate*1.2);
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.bitRateMin = (int)(pContext->stConfigPara.nVideoBitRate*0.8);
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.fRangeRatioTh = 0.05;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nQualityTh = 85;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nMinQuality = 10;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nMaxQuality = 100;
            break;
        }
    }
    alogd("venc set Rcmode=%d", pContext->stVencChnAttr.RcAttr.mRcMode);

    if(0 == pContext->stConfigPara.nGopMode)
    {
        pContext->stVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    }
    else if(1 == pContext->stConfigPara.nGopMode)
    {
        pContext->stVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_DUALP;
    }
    else if(2 == pContext->stConfigPara.nGopMode)
    {
        pContext->stVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_SMARTP;
        pContext->stVencChnAttr.GopAttr.stSmartP.mVirtualIFrameInterval = 15;
    }
    pContext->stVencChnAttr.GopAttr.mGopSize = pContext->stConfigPara.nGopSize;

    pContext->stVencChnAttr.EncppAttr.eEncppSharpSetting = pContext->stConfigPara.bEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    pContext->stVencRcParam.EnIFrmMbRcMoveStatusEnable = 0;
    pContext->stVencRcParam.EnIFrmMbRcMoveStatus = 0;
    pContext->stVencRcParam.mBitsRatioEnable = 0;
    pContext->stVencRcParam.mWeakTextureThEnable = 0;
    pContext->stVencRcParam.mWeakTextureTh = 0;
    if (PT_H264 == pContext->stVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfigPara.nRcMode)
        {
        case 1:
            pContext->stVencRcParam.ParamH264Vbr.mMinQp = 25;
            pContext->stVencRcParam.ParamH264Vbr.mMaxQp = 45;
            pContext->stVencRcParam.ParamH264Vbr.mMinPqp = 25;
            pContext->stVencRcParam.ParamH264Vbr.mMaxPqp = 45;
            pContext->stVencRcParam.ParamH264Vbr.mQpInit = 37;
            pContext->stVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 1;
            pContext->stVencRcParam.ParamH264Vbr.mMovingTh = 20;
            pContext->stVencRcParam.ParamH264Vbr.mQuality = 5;
            pContext->stVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
            pContext->stVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
            break;
        case 0:
        default:
            pContext->stVencRcParam.ParamH264Cbr.mMinQp = 25;
            pContext->stVencRcParam.ParamH264Cbr.mMaxQp = 45;
            pContext->stVencRcParam.ParamH264Cbr.mMaxPqp = 25;
            pContext->stVencRcParam.ParamH264Cbr.mMinPqp = 45;
            pContext->stVencRcParam.ParamH264Cbr.mQpInit = 37;
            pContext->stVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 0;
            break;
        }
    }
    else if (PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfigPara.nRcMode)
        {
        case 1:
            pContext->stVencRcParam.ParamH265Vbr.mMinQp = 25;
            pContext->stVencRcParam.ParamH265Vbr.mMaxQp = 45;
            pContext->stVencRcParam.ParamH265Vbr.mMinPqp = 25;
            pContext->stVencRcParam.ParamH265Vbr.mMaxPqp = 45;
            pContext->stVencRcParam.ParamH265Vbr.mQpInit = 37;
            pContext->stVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 1;
            pContext->stVencRcParam.ParamH265Vbr.mMovingTh = 20;
            pContext->stVencRcParam.ParamH265Vbr.mQuality = 5;
            pContext->stVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
            pContext->stVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
            break;
        case 0:
        default:
            pContext->stVencRcParam.ParamH265Cbr.mMinQp = 25;
            pContext->stVencRcParam.ParamH265Cbr.mMaxQp = 45;
            pContext->stVencRcParam.ParamH265Cbr.mMinPqp = 25;
            pContext->stVencRcParam.ParamH265Cbr.mMaxPqp = 45;
            pContext->stVencRcParam.ParamH265Cbr.mQpInit = 37;
            pContext->stVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 0;
            break;
        }
    }

    return SUCCESS;
}

static ERRORTYPE createVencChn(SampleAVMuxerContext *pContext)
{
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;

    configVencChnAttr(pContext);
    pContext->nVeChn = pContext->stConfigPara.nVeChn;

    ret = AW_MPI_VENC_CreateChn(pContext->nVeChn, &pContext->stVencChnAttr);
    if (SUCCESS == ret)
    {
        bSuccessFlag = TRUE;
        alogd("create venc channel[%d] success!", pContext->nVeChn);
    }
    else if (ERR_VENC_EXIST == ret)
    {
        aloge("fatal error! venc channel[%d] is exist!", pContext->nVeChn);
    }
    else
    {
        aloge("fatal error! create venc channel[%d] ret[0x%x], find next!", pContext->nVeChn, ret);
    }

    if (bSuccessFlag == FALSE)
    {
        pContext->nVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }
    else
    {
        ret = AW_MPI_VENC_SetRcParam(pContext->nVeChn, &pContext->stVencRcParam);
        if(ret != SUCCESS)
        {
            aloge("fatal error! VencChn[%d] set rc param fail", pContext->nVeChn);
        }
        
        /* set framerate in AW_MPI_VENC_CreateChn(), so no need set here */
        VENC_FRAME_RATE_S stFrameRate;
        //stFrameRate.SrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
        //stFrameRate.DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
        //alogd("set venc framerate: src %dfps, dst %dfps", stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
        //AW_MPI_VENC_SetFrameRate(pContext->mVeChn, &stFrameRate);

        if (pContext->stConfigPara.bCropEnable)
        {
            VENC_CROP_CFG_S stCropCfg;
            memset(&stCropCfg, 0, sizeof(VENC_CROP_CFG_S));
            stCropCfg.bEnable = (BOOL)pContext->stConfigPara.bCropEnable;
            stCropCfg.Rect.X = pContext->stConfigPara.nCropRectX;
            stCropCfg.Rect.Y = pContext->stConfigPara.nCropRectY;
            stCropCfg.Rect.Width = pContext->stConfigPara.nCropRectWidth;
            stCropCfg.Rect.Height = pContext->stConfigPara.nCropRectHeight;
            AW_MPI_VENC_SetCrop(pContext->nVeChn, &stCropCfg);
            alogd("set Crop %d, [%d-%d-%dx%d]", stCropCfg.bEnable, stCropCfg.Rect.X, stCropCfg.Rect.Y, stCropCfg.Rect.Width, stCropCfg.Rect.Height);
        }

        if (pContext->stConfigPara.bVuiTimingInfoPresentFlag)
        {
            /** must be call it before AW_MPI_VENC_GetH264SpsPpsInfo(unbind) and AW_MPI_VENC_StartRecvPic. */
            if(PT_H264 == pContext->stVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H264_VUI_S H264Vui;
                memset(&H264Vui, 0, sizeof(VENC_PARAM_H264_VUI_S));
                AW_MPI_VENC_GetH264Vui(pContext->nVeChn, &H264Vui);
                H264Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H264Vui.VuiTimeInfo.fixed_frame_rate_flag = 1;
                H264Vui.VuiTimeInfo.num_units_in_tick = 1000;
                H264Vui.VuiTimeInfo.time_scale = H264Vui.VuiTimeInfo.num_units_in_tick * pContext->stConfigPara.nVideoFrameRate * 2;
                H264Vui.VuiBitstreamRestric.bitstream_restriction_flag = 1;
                AW_MPI_VENC_SetH264Vui(pContext->nVeChn, &H264Vui);
                alogd("VencChn[%d] fill framerate %d to H264VUI", pContext->nVeChn, pContext->stConfigPara.nVideoFrameRate);
            }
            else if(PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H265_VUI_S H265Vui;
                memset(&H265Vui, 0, sizeof(VENC_PARAM_H265_VUI_S));
                AW_MPI_VENC_GetH265Vui(pContext->nVeChn, &H265Vui);
                H265Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H265Vui.VuiTimeInfo.num_units_in_tick = 1000;
                /* Notices: the protocol syntax states that h265 does not need to be multiplied by 2. */
                H265Vui.VuiTimeInfo.time_scale = H265Vui.VuiTimeInfo.num_units_in_tick * pContext->stConfigPara.nVideoFrameRate;
                H265Vui.VuiTimeInfo.num_ticks_poc_diff_one_minus1 = H265Vui.VuiTimeInfo.num_units_in_tick;
                H265Vui.VuiBitstreamRestric.bitstream_restriction_flag = 1;
                AW_MPI_VENC_SetH265Vui(pContext->nVeChn, &H265Vui);
                alogd("VencChn[%d] fill framerate %d to H265VUI", pContext->nVeChn, pContext->stConfigPara.nVideoFrameRate);
            }
        }

        if (PT_H264 == pContext->stVencChnAttr.VeAttr.Type || PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
        {
            int dstWidthAlign = AWALIGN(pContext->stConfigPara.dstWidth, 16);
            int dstHeightAlign = AWALIGN(pContext->stConfigPara.dstHeight, 16);
            if (dstWidthAlign != pContext->stConfigPara.dstWidth || dstHeightAlign != pContext->stConfigPara.dstHeight)
            {
                VencForceConfWin stConfWin;
                memset(&stConfWin, 0, sizeof(VencForceConfWin));
                stConfWin.en_force_conf = 1;
                stConfWin.left_offset = 0;
                stConfWin.right_offset = dstWidthAlign - pContext->stConfigPara.dstWidth;
                stConfWin.top_offset = 0;
                stConfWin.bottom_offset = dstHeightAlign - pContext->stConfigPara.dstHeight;
                alogd("set ForceConfWin en %d, left %d right %d top %d bottom %d", stConfWin.en_force_conf, stConfWin.left_offset,
                    stConfWin.right_offset, stConfWin.top_offset, stConfWin.bottom_offset);
                AW_MPI_VENC_SetForceConfWin(pContext->nVeChn, &stConfWin);
            }
        }

        if (PT_MJPEG == pContext->stVencChnAttr.VeAttr.Type)
        {
            VENC_PARAM_JPEG_S stJpegParam;
            memset(&stJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
            stJpegParam.Qfactor = 90; //init qfactor, first jpeg quality.
            AW_MPI_VENC_SetJpegParam(pContext->nVeChn, &stJpegParam);
        }

        if (PT_H264 == pContext->stVencChnAttr.VeAttr.Type || PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
        {
            if (pContext->stConfigPara.bVbrOptEnable)
            {
                VencVbrOptParam stVbrOptParam;
                memset(&stVbrOptParam, 0, sizeof(VencVbrOptParam));
                AW_MPI_VENC_GetVbrOptParam(pContext->nVeChn, &stVbrOptParam);
#if 0
                stVbrOptParam.enable_instaneousBR = 1;
                stVbrOptParam.recodeIsliceQpEn = 1;
                stVbrOptParam.uMosAicOptEn = 1;
                stVbrOptParam.uSaveBitRateEn = 1;
                stVbrOptParam.uClassifyMadThAdjustEn = 1;
                stVbrOptParam.uMoveToStaticOptEn = 1;

                stVbrOptParam.nIPratio = 25;
                stVbrOptParam.nIntraPeriodNumInVbv = 4;
                stVbrOptParam.max_instaneousBR = 0.9f;
                stVbrOptParam.peroid_instaneousBR = 40;

                alogd("update VbrOptParam");
#endif
                AW_MPI_VENC_SetVbrOptParam(pContext->nVeChn, &stVbrOptParam);
            }
        }

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->nVeChn, &cbInfo);

        VENC_IspVeLinkAttr stIspVeLinkAttr;
        memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
        stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
        stIspVeLinkAttr.bEnableVe2Isp = FALSE;
        stIspVeLinkAttr.nVipp = pContext->nViDev;
        AW_MPI_VENC_EnableIspVeLink(pContext->nVeChn, &stIspVeLinkAttr);
        alogd("VencChn[%d] isp2VeLink:%d-%d-%d", pContext->nVeChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
            stIspVeLinkAttr.nVipp);

        return SUCCESS;
    }
}

static ERRORTYPE createViChn(SampleAVMuxerContext *pContext)
{
    ERRORTYPE ret;

    //create vi channel
    pContext->nViDev = pContext->stConfigPara.nVippDev;
    if (pContext->stConfigPara.bOnlineEnable)
    {
        if(pContext->nViDev != 0)
        {
            aloge("fatal error! only vipp0 support online encode.");
        }
        pContext->nViDev = 0;
    }
    pContext->nIspDev = 0;
    pContext->nViChn = 0;

    ret = AW_MPI_VI_CreateVipp(pContext->nViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
    }

    memset(&pContext->stViAttr, 0, sizeof(VI_ATTR_S));
    if (pContext->stConfigPara.bOnlineEnable)
    {
        pContext->stViAttr.mOnlineEnable = 1;
        pContext->stViAttr.mOnlineShareBufNum = pContext->stConfigPara.nOnlineShareBufNum;
    }
    pContext->stViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->stViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->stViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->stConfigPara.eSrcPixFmt);
    pContext->stViAttr.format.field = V4L2_FIELD_NONE;
    pContext->stViAttr.format.colorspace = pContext->stConfigPara.eColorSpace;
    pContext->stViAttr.format.width = pContext->stConfigPara.srcWidth;
    pContext->stViAttr.format.height = pContext->stConfigPara.srcHeight;
    pContext->stViAttr.nbufs =  pContext->stConfigPara.nViBufferNum;
    alogd("vipp use %d v4l2 buffers, colorspace: 0x%x", pContext->stViAttr.nbufs, pContext->stViAttr.format.colorspace);
    pContext->stViAttr.nplanes = 2;
    pContext->stViAttr.wdr_mode = pContext->stConfigPara.wdr_en;
    alogd("wdr_mode %d", pContext->stViAttr.wdr_mode);
    pContext->stViAttr.fps = pContext->stConfigPara.nSrcFrameRate;
    pContext->stViAttr.drop_frame_num = pContext->stConfigPara.nViDropFrameNum;
    pContext->stViAttr.mbEncppEnable = pContext->stConfigPara.bEncppEnable;
    ret = AW_MPI_VI_SetVippAttr(pContext->nViDev, &pContext->stViAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr[%d] failed", pContext->nViDev);
    }
#if ISP_RUN
    AW_MPI_ISP_Run(pContext->nIspDev);
#endif

    ViVirChnAttrS stVirChnAttr;
    memset(&stVirChnAttr, 0, sizeof(ViVirChnAttrS));
    stVirChnAttr.mbRecvInIdleState = FALSE;
    ret = AW_MPI_VI_CreateVirChn(pContext->nViDev, pContext->nViChn, &stVirChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pContext->nViChn);
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VI_RegisterCallback(pContext->nViDev, &cbInfo);

    ret = AW_MPI_VI_EnableVipp(pContext->nViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! enableVipp[%d] fail!", pContext->nViDev);
    }
    return ret;
}

static int prepare(SampleAVMuxerContext *pContext)
{
    MUX_CHN nMuxChn;
    ERRORTYPE ret;
    int result = FAILURE;

    if (createViChn(pContext) != SUCCESS)
    {
        aloge("fatal error! create vi chn fail");
        return result;
    }

    if (createVencChn(pContext) != SUCCESS)
    {
        aloge("fatal error! create venc chn fail");
        return result;
    }

    // config ai & aenc chn id
    pContext->nAudioDevId = 0;

    // create ai & aenc chn
    if (createAIChn(pContext) != SUCCESS)
    {
        aloge("fatal error! create ai chn fail!");
        return result;
    }
    if (createAEncChn(pContext) != SUCCESS)
    {
        aloge("fatal error! create aenc chn fail!");
        return result;
    }

    if (createMuxChn(pContext) != SUCCESS)
    {
        aloge("fatal error! create mux chn fail");
        return result;
    }

    //set spspps
    if (pContext->stConfigPara.eVideoEncoderFmt == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        memset(&H264SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->nVeChn, &H264SpsPpsInfo);
        if (ret != SUCCESS)
        {
            aloge("fatal error! venc GetH264SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        ret = AW_MPI_MUX_SetH264SpsPpsInfo(pContext->nMuxChn, pContext->nVeChn, &H264SpsPpsInfo);
        if (ret != SUCCESS)
        {
            aloge("fatal error! mux SetH264SpsPpsInfo failed! ret=%d", ret);
        }
    }
    else if(pContext->stConfigPara.eVideoEncoderFmt == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        memset(&H265SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->nVeChn, &H265SpsPpsInfo);
        if (ret != SUCCESS)
        {
            aloge("fatal error! venc GetH265SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        ret = AW_MPI_MUX_SetH265SpsPpsInfo(pContext->nMuxChn, pContext->nVeChn, &H265SpsPpsInfo);
        if (ret != SUCCESS)
        {
            aloge("fatal error! venc SetH265SpsPpsInfo failed! ret=%d", ret);
        }
    }

    MPP_CHN_S stViChn = {MOD_ID_VIU, pContext->nViDev, pContext->nViChn};
    MPP_CHN_S stVeChn = {MOD_ID_VENC, 0, pContext->nVeChn};
    MPP_CHN_S stAiChn = {MOD_ID_AI, pContext->nAudioDevId, pContext->nAiChn};
    MPP_CHN_S stAeChn = {MOD_ID_AENC, 0, pContext->nAEncChn};
    MPP_CHN_S stMuxChn = {MOD_ID_MUX, 0, pContext->nMuxChn};
    if ((pContext->nViDev >= 0 && pContext->nViChn >= 0) && pContext->nVeChn >= 0)
    {
        ret = AW_MPI_SYS_Bind(&stViChn, &stVeChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind ViChn-VeChn fail:0x%x", ret);
        }
    }

    if(pContext->nAiChn >=0 && pContext->nAEncChn >=0)
    {
        ret = AW_MPI_SYS_Bind(&stAiChn, &stAeChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind AiChn-AeChn fail:0x%x", ret);
        }
    }

    if (pContext->nVeChn >= 0 && pContext->nMuxChn >= 0)
    {
        ret = AW_MPI_SYS_Bind(&stVeChn, &stMuxChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind VeChn-MuxChn fail:0x%x", ret);
        }
    }

    if (pContext->nAEncChn >= 0 && pContext->nMuxChn >= 0)
    {
        ret = AW_MPI_SYS_Bind(&stAeChn, &stMuxChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind AeChn-MuxChn fail:0x%x", ret);
        }
    }
    result = SUCCESS;
    return result;
}

static int start(SampleAVMuxerContext *pContext)
{
    int result = 0;
    ERRORTYPE ret = SUCCESS;

    alogd("start");

    ret = AW_MPI_VI_EnableVirChn(pContext->nViDev, pContext->nViChn);
    if (ret != SUCCESS)
    {
        alogd("VI enable error!");
        return FAILURE;
    }
    if (pContext->nVeChn >= 0)
    {
        AW_MPI_VENC_StartRecvPic(pContext->nVeChn);
    }

    if (pContext->nAiChn >= 0)
    {
        ret = AW_MPI_AI_EnableChn(pContext->nAudioDevId, pContext->nAiChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! ai enable chn[%d-%d] fail[0x%x]", pContext->nAudioDevId, pContext->nAiChn, ret);
        }
    }
    if (pContext->nAEncChn >= 0)
    {
        ret = AW_MPI_AENC_StartRecvPcm(pContext->nAEncChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! aenc enable chn[%d] fail[0x%x]", pContext->nAEncChn, ret);
        }
    }

    if (pContext->nMuxChn >= 0)
    {
        ret = AW_MPI_MUX_StartChn(pContext->nMuxChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! mux start chn[%d] fail[0x%x]", pContext->nMuxChn, ret);
        }
    }

    return result;
}

static ERRORTYPE stop(SampleAVMuxerContext *pContext)
{
    ERRORTYPE ret = SUCCESS;

    alogd("stop");

    if (pContext->nViChn >= 0)
    {
        ret = AW_MPI_VI_DisableVirChn(pContext->nViDev, pContext->nViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! viChn[%d-%d] disable fail[0x%x]", pContext->nViDev, pContext->nViChn, ret);
        }
    }

    if (pContext->nVeChn >= 0)
    {
        alogd("stop venc");
        ret = AW_MPI_VENC_StopRecvPic(pContext->nVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! VeChn[%d] stop fail[0x%x]", pContext->nVeChn, ret);
        }
    }
    // stop ai & aenc
    if(pContext->nAiChn >= 0)
    {
        ret = AW_MPI_AI_DisableChn(pContext->nAudioDevId, pContext->nAiChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! AiChn[%d-%d] disable fail[0x%x]", pContext->nAudioDevId, pContext->nAiChn, ret);
        }
    }
    if(pContext->nAEncChn >= 0)
    {
        ret = AW_MPI_AENC_StopRecvPcm(pContext->nAEncChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! AencChn[%d] stop fail[0x%x]", pContext->nAEncChn, ret);
        }
    }
    if(pContext->nMuxChn >= 0)
    {
        ret = AW_MPI_MUX_StopChn(pContext->nMuxChn, FALSE);
        if(ret != SUCCESS)
        {
            aloge("fatal error! muxChn[%d] stop fail[0x%x]", pContext->nMuxChn, ret);
        }
    }

    //destroy components
    if (pContext->nMuxChn >= 0)
    {
        alogd("destory mux chn");
        ret = AW_MPI_MUX_DestroyChn(pContext->nMuxChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! muxChn[%d] destroy fail[0x%x]", pContext->nMuxChn, ret);
        }
        pContext->nMuxChn = MM_INVALID_CHN;
    }
    if (pContext->nVeChn >= 0)
    {
        alogd("destory venc");
        //AW_MPI_VENC_ResetChn(pContext->mVeChn);
        ret = AW_MPI_VENC_DestroyChn(pContext->nVeChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! VeChn[%d] destroy fail[0x%x]", pContext->nVeChn, ret);
        }
        pContext->nVeChn = MM_INVALID_CHN;
    }
    if (pContext->nViChn >= 0)
    {
        ret = AW_MPI_VI_DestroyVirChn(pContext->nViDev, pContext->nViChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! ViChn[%d-%d] destroy fail[0x%x]", pContext->nViDev, pContext->nViChn, ret);
        }
        pContext->nViChn = MM_INVALID_CHN;
        ret = AW_MPI_VI_DisableVipp(pContext->nViDev);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d] disable fail[0x%x]", pContext->nViDev, ret);
        }
    #if ISP_RUN
        ret = AW_MPI_ISP_Stop(pContext->nIspDev);
        if(ret != SUCCESS)
        {
            aloge("fatal error! isp[%d] stop fail[0x%x]", pContext->nIspDev, ret);
        }
    #endif
        ret = AW_MPI_VI_DestroyVipp(pContext->nViDev);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d] destroy fail[0x%x]", pContext->nViDev, ret);
        }
        pContext->nViDev = MM_INVALID_DEV;
    }
    if(pContext->nAEncChn >= 0)
    {
        //AW_MPI_AENC_ResetChn(pContext->nAEncChn);
        ret = AW_MPI_AENC_DestroyChn(pContext->nAEncChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! AeChn[%d] destroy fail[0x%x]", pContext->nAEncChn, ret);
        }
        pContext->nAEncChn = MM_INVALID_CHN;
    }
    if(pContext->nAiChn >= 0)
    {
        //AW_MPI_AI_ResetChn(pContext->nAudioDevId, pContext->nAiChn);
        ret = AW_MPI_AI_DestroyChn(pContext->nAudioDevId, pContext->nAiChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! AiChn[%d-%d] destroy fail[0x%x]", pContext->nAudioDevId, pContext->nAiChn, ret);
        }
        pContext->nAiChn = MM_INVALID_CHN;
    }

    return SUCCESS;
}

static int resetCamera(SampleAVMuxerContext *pContext)
{
    alogd("reset camera [%d]", pContext->nResetCameraCnt);

    pContext->nResetCameraCnt++;

    if (pContext->nViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pContext->nViDev, pContext->nViChn);
    }

    if (pContext->nVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pContext->nVeChn);
    }

    if ((pContext->nViDev >= 0 && pContext->nViChn >= 0) && pContext->nVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->nViDev, pContext->nViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->nVeChn};
        alogd("UnBind vi ve");
        AW_MPI_SYS_UnBind(&ViChn, &VeChn);
    }

    if (pContext->nViChn >= 0)
    {
        alogd("DestroyVirChn");
        AW_MPI_VI_DestroyVirChn(pContext->nViDev, pContext->nViChn);
        alogd("DisableVipp");
        AW_MPI_VI_DisableVipp(pContext->nViDev);
#if ISP_RUN
        alogd("ISP_Stop");
        AW_MPI_ISP_Stop(pContext->nIspDev);
#endif
        alogd("DestroyVipp");
        AW_MPI_VI_DestroyVipp(pContext->nViDev);
    }

    alogd("createViChn");
    createViChn(pContext);

    if ((pContext->nViDev >= 0 && pContext->nViChn >= 0) && pContext->nVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->nViDev, pContext->nViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->nVeChn};
        alogd("Bind vi ve");
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    alogd("start");

    if (pContext->nViDev >= 0 && pContext->nViChn >= 0)
    {
        alogd("VI_EnableVirChn");
        AW_MPI_VI_EnableVirChn(pContext->nViDev, pContext->nViChn);
    }

    if (pContext->nVeChn >= 0)
    {
        alogd("VENC_StartRecvPic");
        AW_MPI_VENC_StartRecvPic(pContext->nVeChn);
    }

    return SUCCESS;
}

static void *MsgQueueThread(void *pThreadData)
{
    SampleAVMuxerContext *pContext = (SampleAVMuxerContext*)pThreadData;
    message_t stCmdMsg;
    SampleAVMuxerMsgType cmd;
    int nCmdPara;

    alogd("msg queue thread start run!");
    while (1)
    {
        if (0 == get_message(&pContext->stMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case Rec_NeedSetNextFd:
                {
                    ERRORTYPE ret;
                    int muxChn = nCmdPara;
                    int nFallocateLength = 0;
                    char fileName[MAX_FILE_PATH_LEN] = {0};
                    if(pContext->nMuxChn != muxChn)
                    {
                        aloge("fatal error! check muxChn[%d!=%d]", pContext->nMuxChn, muxChn);
                    }
                    getFileNameByFileCnt(pContext, fileName);
                    FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                    memset(pFilePathNode, 0, sizeof(FilePathNode));
                    strncpy(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                    list_add_tail(&pFilePathNode->mList, &pContext->stMuxerFileList);
                    alogd("muxChn[%d] set next fd, filepath=%s", muxChn, fileName);
                    int fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0666);
                    if (fd < 0)
                    {
                        aloge("fail to open %s", fileName);
                    }
                    ret = AW_MPI_MUX_SwitchFd(muxChn, fd, nFallocateLength);
                    if(ret != SUCCESS)
                    {
                        aloge("fatal error! muxChn[%d] switchFd[%d] fail[0x%x]", muxChn, fd, ret);
                    }
                    close(fd);
                    break;
                }
                case Rec_FileDone:
                {
                    int ret;
                    int muxChn = nCmdPara;
                    if(muxChn != pContext->nMuxChn)
                    {
                        aloge("fatal error! muxChn[%d!=%d]", muxChn, pContext->nMuxChn);
                    }
                    int cnt = 0;
                    struct list_head *pList;
                    list_for_each(pList, &pContext->stMuxerFileList){cnt++;}
                    FilePathNode *pNode = NULL;
                    while (cnt > pContext->stConfigPara.nDstFileMaxCnt)
                    {
                        pNode = list_first_entry(&pContext->stMuxerFileList, FilePathNode, mList);
                        if ((ret = remove(pNode->strFilePath)) != 0)
                        {
                            aloge("fatal error! delete file[%s] failed:%s", pNode->strFilePath, strerror(errno));
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
                case Vi_Timeout:
                {
                    int ret = resetCamera(pContext);
                    break;
                }
                case MsgQueue_Stop:
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
            TMessage_WaitQueueNotEmpty(&pContext->stMsgQueue, 0);
        }
    }
_Exit:
    alogd("msg queue thread exit!");
    return NULL;
}

int main(int argc, char** argv)
{
    int result = -1;
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

    alogd("sample_avmuxer running!");
    SampleAVMuxerContext *pContext = (SampleAVMuxerContext*)malloc(sizeof(SampleAVMuxerContext));
    if(pContext == NULL)
    {
        aloge("fatal error! malloc fail");
        result = FAILURE;
        goto _err0;
    }
    InitSampleAVMuxerContext(pContext);
    gpSampleAVMuxerContext = pContext;

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    if (parseCmdLine(pContext, argc, argv) != SUCCESS)
    {
        aloge("parse cmdline fail");
        result = FAILURE;
        goto err_out_0;
    }
    char *pConfPath = NULL;
    if(argc > 1)
    {
        pConfPath = pContext->stCmdLinePara.strConfigFilePath;
    }
        
    if (loadConfigPara(pContext, pConfPath) != SUCCESS)
    {
        aloge("load config file fail");
        result = FAILURE;
        goto err_out_0;
    }
    alogd("ViDropFrameNum=%d", pContext->stConfigPara.nViDropFrameNum);
    CreateFolder(pContext->strDstDir);

    pContext->stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->stSysConf);
    AW_MPI_SYS_Init();

    FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
    memset(pFilePathNode, 0, sizeof(FilePathNode));
    strncpy(pFilePathNode->strFilePath, pContext->stConfigPara.dstVideoFile, MAX_FILE_PATH_LEN-1);
    list_add_tail(&pFilePathNode->mList, &pContext->stMuxerFileList);

    if (prepare(pContext) != SUCCESS)
    {
        aloge("prepare fail!");
        goto err_out_2;
    }

    //create msg queue thread
    result = pthread_create(&pContext->nMsgQueueThreadId, NULL, MsgQueueThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create Msg Queue Thread fail[%d]", result);
        goto err_out_3;
    }
    else
    {
        alogd("create Msg Queue Thread success! threadId[%d]", pContext->nMsgQueueThreadId);
    }

    start(pContext);

    //test SEI
    if (pContext->stConfigPara.eSeiEnable <= VencSei_Enable)
    {
        VENC_SEI_ATTR stVencSeiAttr;
        memset(&stVencSeiAttr, 0, sizeof(stVencSeiAttr));
        stVencSeiAttr.eSeiEnableSetting = pContext->stConfigPara.eSeiEnable;
        if(pContext->stConfigPara.bSeiDataIsp)
        {
            stVencSeiAttr.nSeiDataTypeFlags |= SEIDataType_ISP;
        }
        if(pContext->stConfigPara.bSeiDataVipp)
        {
            stVencSeiAttr.nSeiDataTypeFlags |= SEIDataType_VIPP;
        }
        if(pContext->stConfigPara.bSeiDataVenc)
        {
            stVencSeiAttr.nSeiDataTypeFlags |= SEIDataType_VENC;
        }
        stVencSeiAttr.nIspDev = pContext->nIspDev;
        stVencSeiAttr.nVipp = pContext->nViDev;
        stVencSeiAttr.nFrameIntervalForISPLevel1 = pContext->stConfigPara.nSeiFrameIntervalIspLevel1;
        stVencSeiAttr.nFrameIntervalForISPLevel2 = pContext->stConfigPara.nSeiFrameIntervalIspLevel2;
        stVencSeiAttr.nFrameIntervalForISPLevel3 = pContext->stConfigPara.nSeiFrameIntervalIspLevel3;
        stVencSeiAttr.nFrameIntervalForVIPP = pContext->stConfigPara.nSeiFrameIntervalVipp;
        stVencSeiAttr.nFrameIntervalForVencLevel1 = pContext->stConfigPara.nSeiFrameIntervalVencLevel1;
        stVencSeiAttr.nFrameIntervalForVencLevel2 = pContext->stConfigPara.nSeiFrameIntervalVencLevel2;
        AW_MPI_VENC_ConfigSEI(pContext->nVeChn, &stVencSeiAttr);
    }
    else
    {
        alogd("ignore to config sei!");
    }

    alogd("wait for test time %ds ...", pContext->stConfigPara.nTestDuration);
    if (pContext->stConfigPara.nTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->stSemExit, pContext->stConfigPara.nTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->stSemExit);
    }

    alogd("test time %ds is up, stop test", pContext->stConfigPara.nTestDuration);

    result = 0;

    //check result
    if (0 < pContext->nResetCameraCnt)
    {
        result = FAILURE;
        aloge("fatal error! resetCameraCnt=%d", pContext->nResetCameraCnt);
    }

    alogd("start to free resource");
    stop(pContext);
    if(pContext->nMsgQueueThreadId != 0)
    {
        //stop msg queue thread
        message_t stMsgCmd;
        stMsgCmd.command = MsgQueue_Stop;
        put_message(&pContext->stMsgQueue, &stMsgCmd);
        pthread_join(pContext->nMsgQueueThreadId, NULL);
        pContext->nMsgQueueThreadId = 0;
    }
    AW_MPI_SYS_Exit();

    cdx_sem_deinit(&pContext->stSemExit);
    message_destroy(&pContext->stMsgQueue);
    free(pContext);
    gpSampleAVMuxerContext = pContext = NULL;
	alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
    
err_out_3:
    stop(pContext);
err_out_2:
    if(!list_empty(&pContext->stMuxerFileList))
    {
        FilePathNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pContext->stMuxerFileList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
err_out_1:
    AW_MPI_SYS_Exit();

err_out_0:
    cdx_sem_deinit(&pContext->stSemExit);
    message_destroy(&pContext->stMsgQueue);
    free(pContext);
    gpSampleAVMuxerContext = pContext = NULL;
_err0:
	alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();

    return result;
}
