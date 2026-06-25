/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/11/4
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleAisr"

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/prctl.h>
#include <mm_common.h>
#include <cdx_list.h>
#include <awaisr.h>

#include "plat_log.h"
#include <mpi_videoformat_conversion.h>
#include "sample_aisr.h"
#include "sample_aisr_config.h"

#define ISP_RUN (1)

static SAMPLE_AISR_S *gpAisrData = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpAisrData)
    {
        cdx_sem_up(&gpAisrData->mSemExit);
    }
}

static ERRORTYPE InitAisrData(SAMPLE_AISR_S *pContext)
{
    memset(pContext, 0, sizeof(SAMPLE_AISR_S));
    pContext->mVeChn = MM_INVALID_CHN;
    pContext->mViChn = MM_INVALID_CHN;
    pContext->mViDev = MM_INVALID_DEV;

    return SUCCESS;
}

static unsigned long long GetTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (unsigned long long)(time.tv_usec + time.tv_sec * 1000000LL);
}

static ERRORTYPE parseCmdLine(SAMPLE_AISR_S *pContext, int argc, char **argv)
{
    ERRORTYPE ret = FAILURE;

    if (argc <= 1)
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

              strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN - 1);
              pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if (!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_aisr.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_AISR_S *pContext, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;

    if (conf_path != NULL)
    {
        CONFPARSER_S mConf;
        memset(&mConf, 0, sizeof(CONFPARSER_S));
        ret = createConfParser(conf_path, &mConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }

        pContext->mConfigPara.mVippDev = GetConfParaInt(&mConf, CFG_VIPP_DEV_ID, 0);
        pContext->mConfigPara.mVeChn = GetConfParaInt(&mConf, CFG_VENC_CH_ID, 0);
        alogd("vippDev: %d, veChn: %d", pContext->mConfigPara.mVippDev, pContext->mConfigPara.mVeChn);

        pContext->mConfigPara.srcWidth = GetConfParaInt(&mConf, CFG_SRC_WIDTH, 0);
        pContext->mConfigPara.srcHeight = GetConfParaInt(&mConf, CFG_SRC_HEIGHT, 0);
        alogd("srcWidth: %d, srcHeight: %d", pContext->mConfigPara.srcWidth, pContext->mConfigPara.srcHeight);

        pContext->mConfigPara.mSrcFrameRate = GetConfParaInt(&mConf, CFG_SRC_FRAMERATE, 0);

        ptr = (char *)GetConfParaString(&mConf, CFG_SRC_PIXFMT, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "yv12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            }
            else if (!strcmp(ptr, "yu12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_PLANAR_420;
            }
            else
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_PLANAR_420;
                aloge("fatal error! wrong src pixfmt:%s", ptr);
                alogw("use the default pixfmt %d", pContext->mConfigPara.srcPixFmt);
            }
        }
        alogd("srcPixFmt=%d", pContext->mConfigPara.srcPixFmt);

        pContext->mConfigPara.mAisrCropWidth = GetConfParaInt(&mConf, CFG_AISR_CROPWIDTH, 0);
        pContext->mConfigPara.mAisrCropHeight = GetConfParaInt(&mConf, CFG_AISR_CROPHEIGHT, 0);
        pContext->mConfigPara.mAisrPx = GetConfParaInt(&mConf, CFG_AISR_PX, 0);
        pContext->mConfigPara.mAisrWidthScale = GetConfParaInt(&mConf, CFG_AISR_WIDTH_SCALE, 0);
        pContext->mConfigPara.mAisrHeightScale = GetConfParaInt(&mConf, CFG_AISR_HEIGHT_SCALE, 0);
        pContext->mConfigPara.mAisrOutputBufNum = GetConfParaInt(&mConf, CFG_AISR_OUTPUT_BUFNUM, 0);
        alogd("aisrcropwidth = %d, aisrcropHeight = %d, aisrpx = %d,"
            "aisrwidthscale = %d, aisrHeightscale = %d, output buf num = %d",
            pContext->mConfigPara.mAisrCropWidth, pContext->mConfigPara.mAisrCropHeight,
            pContext->mConfigPara.mAisrPx, pContext->mConfigPara.mAisrWidthScale,
            pContext->mConfigPara.mAisrHeightScale, pContext->mConfigPara.mAisrOutputBufNum);

        pContext->mConfigPara.mAisr1200WTest = GetConfParaInt(&mConf, CFG_AISR_1200W_TEST, 0);
        ptr = (char *)GetConfParaString(&mConf, CFG_AISR_SRCYUV_FILE, NULL);
        if (ptr != NULL)
        {
            strcpy(pContext->mConfigPara.srcYuvFile, ptr);
        }

        ptr = (char *)GetConfParaString(&mConf, CFG_AISR_DSTYUV_FILE, NULL);
        if (ptr != NULL)
        {
            strcpy(pContext->mConfigPara.dstYuvFile, ptr);
        }

        pContext->mConfigPara.mVencsrcWidth = pContext->mConfigPara.srcWidth * pContext->mConfigPara.mAisrWidthScale;
        pContext->mConfigPara.mVencsrcHeight = pContext->mConfigPara.srcHeight * pContext->mConfigPara.mAisrHeightScale;
        pContext->mConfigPara.mVencdstWidth = pContext->mConfigPara.mVencsrcWidth;
        pContext->mConfigPara.mVencdstHeight = pContext->mConfigPara.mVencsrcHeight;
        alogd("venc src widthxHeight = %dx%d, dst widthxHeight = %dx%d",
            pContext->mConfigPara.mVencsrcWidth, pContext->mConfigPara.mVencsrcHeight,
            pContext->mConfigPara.mVencdstWidth, pContext->mConfigPara.mVencdstHeight);

        ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_FILE_STR, NULL);
        if (ptr != NULL)
        {
            strcpy(pContext->mConfigPara.dstVideoFile, ptr);
        }

        pContext->mConfigPara.mVbrOptEnable = GetConfParaInt(&mConf, CFG_VBR_OPT_ENABLE, 0);

        pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_FRAMERATE, 0);
        pContext->mConfigPara.mViBufferNum = GetConfParaInt(&mConf, CFG_DST_VI_BUFFER_NUM, 0);
        pContext->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_BITRATE, 0);

        pContext->mConfigPara.mProductMode = GetConfParaInt(&mConf, CFG_PRODUCT_MODE, 0);
        //pContext->mConfigPara.mSensorType = GetConfParaInt(&mConf, CFG_SENSOR_TYPE, 0);
        pContext->mConfigPara.mKeyFrameInterval = GetConfParaInt(&mConf, CFG_KEY_FRAME_INTERVAL, 0);
        pContext->mConfigPara.mRcMode = GetConfParaInt(&mConf, CFG_RC_MODE, 0);

        ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_ENCODER, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "H.264"))
            {
                pContext->mConfigPara.mVideoEncoderFmt = PT_H264;
                alogd("H.264");
            }
            else if (!strcmp(ptr, "H.265"))
            {
                pContext->mConfigPara.mVideoEncoderFmt = PT_H265;
                alogd("H.265");
            }
            else if (!strcmp(ptr, "MJPEG"))
            {
                pContext->mConfigPara.mVideoEncoderFmt = PT_MJPEG;
                alogd("MJPEG");
            }
            else
            {
                aloge("error conf encoder type, default use H264");
                pContext->mConfigPara.mVideoEncoderFmt = PT_H264;
            }
        }

        pContext->mConfigPara.mTestDuration = GetConfParaInt(&mConf, CFG_TEST_DURATION, 0);
        pContext->mConfigPara.mEncUseProfile = GetConfParaInt(&mConf, CFG_DST_ENCODE_PROFILE, 0);

        alogd("vipp:%d, SrcFrameRate:%d, VideoFrameRate:%d, bitrate:%d, test_time=%d, profile=%d", pContext->mConfigPara.mVippDev,\
            pContext->mConfigPara.mSrcFrameRate, pContext->mConfigPara.mVideoFrameRate, pContext->mConfigPara.mVideoBitRate,\
            pContext->mConfigPara.mTestDuration,\
            pContext->mConfigPara.mEncUseProfile);

        pContext->mConfigPara.mVbvBufferSize = GetConfParaInt(&mConf, CFG_vbvBufferSize, 0);
        pContext->mConfigPara.mVbvThreshSize = GetConfParaInt(&mConf, CFG_vbvThreshSize, 0);
        alogd("VbvBufferSize:%d, VbvThreshSize:%d",
            pContext->mConfigPara.mVbvBufferSize,
            pContext->mConfigPara.mVbvThreshSize);

        pContext->mConfigPara.mEncppEnable = GetConfParaInt(&mConf, CFG_EncppEnable, 0);
        alogd("EncppEnable: %d", pContext->mConfigPara.mEncppEnable);

        ptr = (char *)GetConfParaString(&mConf, CFG_VeRefFrameLbcMode, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "aw_lbc_2_5x"))
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_2_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_2_0x"))
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_2_0X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_5x"))
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_1_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_no_lossy"))
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_NO_LOSSY;
            }
            else
            {
                pContext->mConfigPara.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT; /* default: 1.5x. */
                aloge("fatal error! wrong src pixfmt:%s", ptr);
                alogw("use the default pixfmt %d", pContext->mConfigPara.mVeRefFrameLbcMode);
            }
        }

        destroyConfParser(&mConf);
    }

    return SUCCESS;
}

static int AISRCallbackWrapper(void *cookie, AwaisrImgBufInfo InputBufInfo, AwaisrImgBufInfo OutputBufInfo)
{
    SAMPLE_AISR_S *pContext = (SAMPLE_AISR_S *)cookie;
    CsiSaveBufMgr *pBufMgr = pContext->mpSaveBufMgr;

    if (!list_empty(&pBufMgr->mFrameAisrUsingList))
    {
        pthread_mutex_lock(&pBufMgr->mFrameAisrUsingListLock);
        SampleAisrSaveBufNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pBufMgr->mFrameAisrUsingList, mList)
        {
            if (pEntry->mDataPhyAddr == OutputBufInfo.mImgPhyBuf[0])
            {
                if (!pContext->mOpt1200wYuv && pEntry->mSrcFrameInfo.VFrame.mPhyAddr[0] == InputBufInfo.mImgPhyBuf[0])
                    AW_MPI_VI_ReleaseFrame(pContext->mViDev, pContext->mViChn, &pEntry->mSrcFrameInfo);
                list_del(&pEntry->mList);
                pthread_mutex_lock(&pBufMgr->mFrameReadyListLock);
                list_add_tail(&pEntry->mList, &pBufMgr->mFrameReadyList);
                pthread_mutex_unlock(&pBufMgr->mFrameReadyListLock);
                break;
            }
        }
        pthread_mutex_unlock(&pBufMgr->mFrameAisrUsingListLock);
    }
    else
    {
        aloge("fatal error!!! mFrameAisrUsingList empty, please check if aisr call callback again");
    }

    if (pContext->mOpt1200wYuv)
    {
        if (!list_empty(&pBufMgr->mInputYuvUsingList))
        {
            pthread_mutex_lock(&pBufMgr->mInputYuvUsingListLock);
            SampleAisrSaveBufNode *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pBufMgr->mInputYuvUsingList, mList)
            {
                if (pEntry->mDataPhyAddr == InputBufInfo.mImgPhyBuf[0])
                {
                    list_del(&pEntry->mList);
                    pthread_mutex_lock(&pBufMgr->mInputYuvIdleListLock);
                    list_add_tail(&pEntry->mList, &pBufMgr->mInputYuvIdleList);
                    pthread_mutex_unlock(&pBufMgr->mInputYuvIdleListLock);
                    break;
                }
            }
            pthread_mutex_unlock(&pBufMgr->mInputYuvUsingListLock);
        }
        else
        {
            aloge("fatal error!!! mInputYuvUsingList empty, please check if aisr call callback again");
        }
    }

    return 0;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_AISR_S *pContext = (SAMPLE_AISR_S *)cookie;
    CsiSaveBufMgr *pBufMgr = pContext->mpSaveBufMgr;
    VIDEO_FRAME_INFO_S *pFrame = (VIDEO_FRAME_INFO_S *)pEventData;
    ERRORTYPE ret = 0;

    if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
                if (pFrame != NULL)
                {
                    if (!list_empty(&pBufMgr->mFrameVeUsingList))
                    {
                        pthread_mutex_lock(&pBufMgr->mFrameVeUsingListLock);
                        SampleAisrSaveBufNode *pEntry, *pTmp;
                        list_for_each_entry_safe(pEntry, pTmp, &pBufMgr->mFrameVeUsingList, mList)
                        {
                            if (pEntry->mDstFrameInfo.mId == pFrame->mId)
                            {
                                list_del(&pEntry->mList);
                                pthread_mutex_lock(&pBufMgr->mFrameIdleListLock);
                                list_add_tail(&pEntry->mList, &pBufMgr->mFrameIdleList);
                                pthread_mutex_unlock(&pBufMgr->mFrameIdleListLock);
                                break;
                            }
                        }
                        pthread_mutex_unlock(&pBufMgr->mFrameVeUsingListLock);
                    }
                }
                break;
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

    return SUCCESS;
}

static inline unsigned int map_H264_UserSet2Profile(int val)
{
    unsigned int profile = (unsigned int)H264_PROFILE_HIGH;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H264_PROFILE_BASE;
        break;
    case 1:
        profile = (unsigned int)H264_PROFILE_MAIN;
        break;
    case 2:
        profile = (unsigned int)H264_PROFILE_HIGH;
        break;
    default:
        break;
    }

    return profile;
}

static inline unsigned int map_H265_UserSet2Profile(int val)
{
    unsigned int profile = H265_PROFILE_MAIN;
    switch (val)
    {
    case 0:
        profile = (unsigned int)H265_PROFILE_MAIN;
        break;
    case 1:
        profile = (unsigned int)H265_PROFILE_MAIN10;
        break;
    case 2:
        profile = (unsigned int)H265_PROFILE_STI11;
        break;
    default:
        break;
    }
    return profile;
}

static ERRORTYPE configVencChnAttr(SAMPLE_AISR_S *pContext)
{
    memset(&pContext->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    pContext->mVencChnAttr.VeAttr.Type = pContext->mConfigPara.mVideoEncoderFmt;
    pContext->mVencChnAttr.VeAttr.MaxKeyInterval = pContext->mConfigPara.mKeyFrameInterval;
    pContext->mVencChnAttr.VeAttr.SrcPicWidth  = pContext->mConfigPara.mVencsrcWidth;
    pContext->mVencChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.mVencsrcHeight;
    pContext->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mVencChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.srcPixFmt;
    pContext->mVencChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
    alogd("pixfmt:0x%x, colorSpace:0x%x", pContext->mVencChnAttr.VeAttr.PixelFormat, pContext->mVencChnAttr.VeAttr.mColorSpace);
    pContext->mVencChnAttr.VeAttr.mVeRefFrameLbcMode = pContext->mConfigPara.mVeRefFrameLbcMode;
    alogd("VeRefFrameLbcMode:%d", pContext->mVencChnAttr.VeAttr.mVeRefFrameLbcMode);
    pContext->mVencChnAttr.VeAttr.mVbrOptEnable = pContext->mConfigPara.mVbrOptEnable;
    alogd("VbrOptEnable:%d", pContext->mVencChnAttr.VeAttr.mVbrOptEnable);
    pContext->mVencChnAttr.EncppAttr.eEncppSharpSetting = pContext->mConfigPara.mEncppEnable ? \
        VencEncppSharp_FollowISPConfig : VencEncppSharp_Disable;

    pContext->mVencChnAttr.RcAttr.mProductMode = pContext->mConfigPara.mProductMode;

    if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH264e.BufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH264e.Profile = map_H264_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH264e.mLevel = VENC_H264LevelDefault; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->mConfigPara.mVencsrcWidth;
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.mVencsrcHeight;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mVencRcParam.mBitsRatioEnable = 0;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH264Vbr.mMinQp = 25;
            pContext->mVencRcParam.ParamH264Vbr.mMaxQp = 45;
            pContext->mVencRcParam.ParamH264Vbr.mMaxPqp = 45;
            pContext->mVencRcParam.ParamH264Vbr.mMinPqp = 25;
            pContext->mVencRcParam.ParamH264Vbr.mQpInit = 37;
            pContext->mVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 1;
            pContext->mVencRcParam.ParamH264Vbr.mMovingTh = 20;
            pContext->mVencRcParam.ParamH264Vbr.mQuality = 1;
            pContext->mVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
            pContext->mVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 25;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 25;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            break;
        case 3:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mRatioChangeQp = 85;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mQuality = 8;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMinIQp = 20;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMinQp = 25;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxQp = 45;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH264Cbr.mMaxQp = 45;
            pContext->mVencRcParam.ParamH264Cbr.mMinQp = 25;
            pContext->mVencRcParam.ParamH264Cbr.mMaxPqp = 45;
            pContext->mVencRcParam.ParamH264Cbr.mMinPqp = 25;
            pContext->mVencRcParam.ParamH264Cbr.mQpInit = 37;
            pContext->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 1;
            break;
        }
    }
    else if (PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH265e.mBufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mProfile = map_H265_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH265e.mLevel = VENC_H265LevelDefault; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.mVencsrcWidth;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.mVencsrcHeight;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mVencRcParam.mBitsRatioEnable = 0;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH265Vbr.mMinQp = 25;
            pContext->mVencRcParam.ParamH265Vbr.mMaxQp = 45;
            pContext->mVencRcParam.ParamH265Vbr.mMaxPqp = 45;
            pContext->mVencRcParam.ParamH265Vbr.mMinPqp = 25;
            pContext->mVencRcParam.ParamH265Vbr.mQpInit = 37;
            pContext->mVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 1;
            pContext->mVencRcParam.ParamH265Vbr.mMovingTh = 20;
            pContext->mVencRcParam.ParamH265Vbr.mQuality = 1;
            pContext->mVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
            pContext->mVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 25;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 25;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            break;
        case 3:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mRatioChangeQp = 85;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mQuality = 1;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMinIQp = 25;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMinQp = 25;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxQp = 45;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH265Cbr.mMaxQp = 45;
            pContext->mVencRcParam.ParamH265Cbr.mMinQp = 25;
            pContext->mVencRcParam.ParamH265Cbr.mMaxPqp = 45;
            pContext->mVencRcParam.ParamH265Cbr.mMinPqp = 25;
            pContext->mVencRcParam.ParamH265Cbr.mQpInit = 37;
            pContext->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 1;
            break;
        }
    }
    else if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mBufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->mConfigPara.mVencsrcWidth;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.mVencsrcHeight;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 0:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            break;
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            break;
        case 2:
        case 3:
        default:
            aloge("not support! use default cbr mode");
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            break;
        }
    }

    alogd("venc set Rcmode=%d", pContext->mVencChnAttr.RcAttr.mRcMode);

    pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->mVencChnAttr.GopAttr.mGopSize = 2;

    return SUCCESS;
}

static ERRORTYPE createVencChn(SAMPLE_AISR_S *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pContext);

    pContext->mVeChn = pContext->mConfigPara.mVeChn;
    while (pContext->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pContext->mVeChn, &pContext->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pContext->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pContext->mVeChn);
            pContext->mVeChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", pContext->mVeChn, ret);
            pContext->mVeChn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pContext->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }
    else
    {
        AW_MPI_VENC_SetRcParam(pContext->mVeChn, &pContext->mVencRcParam);
        /** must be call it before AW_MPI_VENC_GetH264SpsPpsInfo(unbind) and AW_MPI_VENC_StartRecvPic. */
        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
        {
            VENC_PARAM_H264_VUI_S H264Vui;
            memset(&H264Vui, 0, sizeof(VENC_PARAM_H264_VUI_S));
            H264Vui.VuiBitstreamRestric.bitstream_restriction_flag = 1;
            AW_MPI_VENC_SetH264Vui(pContext->mVeChn, &H264Vui);
            AW_MPI_VENC_SetH264NalRefIdcNoneZeroValue(pContext->mVeChn, 3);
        }
        else if (PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            VENC_PARAM_H265_VUI_S H265Vui;
            memset(&H265Vui, 0, sizeof(VENC_PARAM_H265_VUI_S));
            H265Vui.VuiBitstreamRestric.bitstream_restriction_flag = 1;
            AW_MPI_VENC_SetH265Vui(pContext->mVeChn, &H265Vui);
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type || PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            int dstWidthAlign = AWALIGN(pContext->mConfigPara.mVencdstWidth, 16);
            int dstHeightAlign = AWALIGN(pContext->mConfigPara.mVencdstHeight, 16);
            if (dstWidthAlign != pContext->mConfigPara.mVencdstWidth || dstHeightAlign != pContext->mConfigPara.mVencdstHeight)
            {
                VencForceConfWin stConfWin;
                memset(&stConfWin, 0, sizeof(VencForceConfWin));
                stConfWin.en_force_conf = 1;
                stConfWin.left_offset = 0;
                stConfWin.right_offset = dstWidthAlign - pContext->mConfigPara.mVencdstWidth;
                stConfWin.top_offset = 0;
                stConfWin.bottom_offset = dstHeightAlign - pContext->mConfigPara.mVencdstHeight;
                alogd("set ForceConfWin en %d, left %d right %d top %d bottom %d", stConfWin.en_force_conf, stConfWin.left_offset,
                    stConfWin.right_offset, stConfWin.top_offset, stConfWin.bottom_offset);
                AW_MPI_VENC_SetForceConfWin(pContext->mVeChn, &stConfWin);
            }
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type || PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            if (pContext->mConfigPara.mVbrOptEnable)
            {
                VencVbrOptParam stVbrOptParam;
                memset(&stVbrOptParam, 0, sizeof(VencVbrOptParam));
                AW_MPI_VENC_GetVbrOptParam(pContext->mVeChn, &stVbrOptParam);
                AW_MPI_VENC_SetVbrOptParam(pContext->mVeChn, &stVbrOptParam);
            }
        }

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void *)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mVeChn, &cbInfo);

        VENC_IspVeLinkAttr stIspVeLinkAttr;
        memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
        stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
        stIspVeLinkAttr.bEnableVe2Isp = FALSE;
        stIspVeLinkAttr.nVipp = pContext->mViDev;
        AW_MPI_VENC_EnableIspVeLink(pContext->mVeChn, &stIspVeLinkAttr);
        alogd("VencChn[%d] ispVeLink:%d-%d-%d", pContext->mVeChn, stIspVeLinkAttr.bEnableIsp2Ve,
            stIspVeLinkAttr.bEnableVe2Isp, stIspVeLinkAttr.nVipp);

        return SUCCESS;
    }
}

static ERRORTYPE createViChn(SAMPLE_AISR_S *pContext)
{
    ERRORTYPE ret;

    //create vi channel
    pContext->mViDev = pContext->mConfigPara.mVippDev;
    pContext->mIspDev = 0;
    pContext->mViChn = 0;

    ret = AW_MPI_VI_CreateVipp(pContext->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
        return ret;
    }

    memset(&pContext->mViAttr, 0, sizeof(VI_ATTR_S));
    pContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.srcPixFmt);
    pContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709_PART_RANGE;
    pContext->mViAttr.format.width = pContext->mConfigPara.srcWidth;
    pContext->mViAttr.format.height = pContext->mConfigPara.srcHeight;
    pContext->mViAttr.nbufs =  pContext->mConfigPara.mViBufferNum;
    alogd("vipp use %d v4l2 buffers, colorspace: 0x%x", pContext->mViAttr.nbufs, pContext->mViAttr.format.colorspace);
    pContext->mViAttr.nplanes = 2;
    alogd("wdr_mode %d", pContext->mViAttr.wdr_mode);
    pContext->mViAttr.fps = pContext->mConfigPara.mSrcFrameRate;
    pContext->mViAttr.mbEncppEnable = pContext->mConfigPara.mEncppEnable;
    ret = AW_MPI_VI_SetVippAttr(pContext->mViDev, &pContext->mViAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }
#if ISP_RUN
    AW_MPI_ISP_Run(pContext->mIspDev);
#endif

    ret = AW_MPI_VI_CreateVirChn(pContext->mViDev, pContext->mViChn, NULL);
    if (ret != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pContext->mViChn);
    }
    ret = AW_MPI_VI_EnableVipp(pContext->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
    }
    return ret;
}

static ERRORTYPE createAisr(SAMPLE_AISR_S *pContext)
{
    int ret = SUCCESS;
    AwaisrConfigParam awaisrconfigparam;
    memset(&awaisrconfigparam, 0 , sizeof(AwaisrConfigParam));
    awaisrconfigparam.mWidth = pContext->mConfigPara.srcWidth;
    awaisrconfigparam.mHeight = pContext->mConfigPara.srcHeight;
    awaisrconfigparam.mCropWidth = pContext->mConfigPara.mAisrCropWidth;
    awaisrconfigparam.mCropHeight = pContext->mConfigPara.mAisrCropHeight;
    awaisrconfigparam.mPx = pContext->mConfigPara.mAisrPx;
    awaisrconfigparam.mScaleX = pContext->mConfigPara.mAisrWidthScale;
    awaisrconfigparam.mScaleY = pContext->mConfigPara.mAisrHeightScale;

    if (0 == AwaisrOpen(awaisrconfigparam))
    {
        AiSrCallbackInfo awaisrcallbackinfo;
        memset(&awaisrcallbackinfo, 0, sizeof(AiSrCallbackInfo));
        awaisrcallbackinfo.cookie = (void *)pContext;
        awaisrcallbackinfo.AwaisrFrameDone = AISRCallbackWrapper;
        AwaisrRegisterCallback(awaisrcallbackinfo);
    }
    else
    {
        aloge("fatal error! sr_init fail");
        ret = FAILURE;
    }
    pContext->mOpt1200wYuv = pContext->mConfigPara.mAisr1200WTest;

    return ret;
}

static ERRORTYPE prepare(SAMPLE_AISR_S *pContext)
{
    ERRORTYPE result = FAILURE;

    if (pContext->mOpt1200wYuv)
        return SUCCESS;

    if (createViChn(pContext) != SUCCESS)
    {
        aloge("create vi chn fail");
        return result;
    }

    if (createVencChn(pContext) != SUCCESS)
    {
        aloge("create venc chn fail");
        return result;
    }

    pContext->mOutputFileFp = fopen(pContext->mConfigPara.dstVideoFile, "wb+");
    if (NULL == pContext->mOutputFileFp)
    {
        aloge("fatal error! can't open file[%s]", pContext->mConfigPara.dstVideoFile);
        return result;
    }
    else
    {
        alogd("open %s success", pContext->mConfigPara.dstVideoFile);
    }

    return 0;
}

static void *GetCsiFrameThread(void *pThreadData)
{
    SAMPLE_AISR_S *pContext = (SAMPLE_AISR_S *)pThreadData;
    CsiSaveBufMgr *pBufMgr = pContext->mpSaveBufMgr;
    struct list_head tmpList;
    int frm_cnt = 0;
    int ret = 0;

    char strThreadName[32] = "GetCsiFrameThread";
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    INIT_LIST_HEAD(&tmpList);
    VIDEO_FRAME_INFO_S FrmInfo;
    while (!pContext->mExitFlag)
    {
        if ((ret = AW_MPI_VI_GetFrame(pContext->mViDev, pContext->mViChn, &FrmInfo, 2000)) != 0)
        {
            continue;
        }

        if (pBufMgr)
        {
            pthread_mutex_lock(&pBufMgr->mFrameIdleListLock);
            SampleAisrSaveBufNode *pOutputEntry = \
                list_first_entry_or_null(&pBufMgr->mFrameIdleList, SampleAisrSaveBufNode, mList);
            if (NULL == pOutputEntry)
            {
                pthread_mutex_unlock(&pBufMgr->mFrameIdleListLock);
                AW_MPI_VI_ReleaseFrame(pContext->mViDev, pContext->mViChn, &FrmInfo);
                usleep(100);
                continue;
            }
            else
            {
                list_move_tail(&pOutputEntry->mList, &tmpList);
                pthread_mutex_unlock(&pBufMgr->mFrameIdleListLock);
            }

            memcpy(&pOutputEntry->mSrcFrameInfo, &FrmInfo, sizeof(VIDEO_FRAME_INFO_S));
            pthread_mutex_lock(&pBufMgr->mFrameAisrUsingListLock);
            list_move_tail(&pOutputEntry->mList, &pBufMgr->mFrameAisrUsingList);
            pthread_mutex_unlock(&pBufMgr->mFrameAisrUsingListLock);

            AwaisrImgBufInfo nInputBufInfo;
            memset(&nInputBufInfo, 0, sizeof(AwaisrImgBufInfo));
            AwaisrImgBufInfo nOutputBufInfo;
            memset(&nOutputBufInfo, 0, sizeof(AwaisrImgBufInfo));
            nInputBufInfo.mImgPhyBuf[0] = FrmInfo.VFrame.mPhyAddr[0];
            nInputBufInfo.mImgVirBuf[0] = FrmInfo.VFrame.mpVirAddr[0];
            if (FrmInfo.VFrame.mPhyAddr[1])
                nInputBufInfo.mImgPhyBuf[1] = FrmInfo.VFrame.mPhyAddr[1];
            else
                nInputBufInfo.mImgPhyBuf[1] = nInputBufInfo.mImgPhyBuf[0] + pContext->mConfigPara.srcWidth * pContext->mConfigPara.srcHeight;
            if (FrmInfo.VFrame.mpVirAddr[1])
                nInputBufInfo.mImgVirBuf[1] = FrmInfo.VFrame.mpVirAddr[1];
            else
                nInputBufInfo.mImgVirBuf[1] = nInputBufInfo.mImgVirBuf[0] + pContext->mConfigPara.srcWidth * pContext->mConfigPara.srcHeight;

            nOutputBufInfo.mImgPhyBuf[0] = pOutputEntry->mDataPhyAddr;
            nOutputBufInfo.mImgPhyBuf[1] = pOutputEntry->mDataPhyAddr + pContext->mConfigPara.mVencsrcWidth * pContext->mConfigPara.mVencsrcHeight;
            nOutputBufInfo.mImgVirBuf[0] = pOutputEntry->mpDataVirAddr;
            nOutputBufInfo.mImgVirBuf[1] = pOutputEntry->mpDataVirAddr + pContext->mConfigPara.mVencsrcWidth * pContext->mConfigPara.mVencsrcHeight;
            AwaisrSendFrame(nInputBufInfo, nOutputBufInfo);
        }
    }

    return NULL;
}

static void *SendEncoderFrameThread(void *pThreadData)
{
    SAMPLE_AISR_S *pContext = (SAMPLE_AISR_S*)pThreadData;
    CsiSaveBufMgr *pBufMgr = pContext->mpSaveBufMgr;
    int ret = 0;
    int count = 0;
    struct list_head tmpList;
    char strThreadName[32] = "SendEncoderFrameThread";
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    INIT_LIST_HEAD(&tmpList);
    int lastIndex = -1;
    while (!pContext->mExitFlag)
    {
        pthread_mutex_lock(&pBufMgr->mFrameReadyListLock);
        SampleAisrSaveBufNode *pOutputEntry = \
                list_first_entry_or_null(&pBufMgr->mFrameReadyList, SampleAisrSaveBufNode, mList);
        if (NULL == pOutputEntry)
        {
            pthread_mutex_unlock(&pBufMgr->mFrameReadyListLock);
            usleep(1000);
            continue;
        }

        list_move_tail(&pOutputEntry->mList, &tmpList);
        pthread_mutex_unlock(&pBufMgr->mFrameReadyListLock);

        memcpy(&pOutputEntry->mDstFrameInfo, &pOutputEntry->mSrcFrameInfo, sizeof(VIDEO_FRAME_INFO_S));

        pOutputEntry->mDstFrameInfo.VFrame.mWidth = pContext->mConfigPara.mVencsrcWidth;
        pOutputEntry->mDstFrameInfo.VFrame.mHeight = pContext->mConfigPara.mVencsrcHeight;
        pOutputEntry->mDstFrameInfo.VFrame.mOffsetLeft = 0;
        pOutputEntry->mDstFrameInfo.VFrame.mOffsetTop = 0;
        pOutputEntry->mDstFrameInfo.VFrame.mOffsetRight = pOutputEntry->mDstFrameInfo.VFrame.mOffsetLeft + pOutputEntry->mDstFrameInfo.VFrame.mWidth;
        pOutputEntry->mDstFrameInfo.VFrame.mOffsetBottom = pOutputEntry->mDstFrameInfo.VFrame.mOffsetTop + pOutputEntry->mDstFrameInfo.VFrame.mHeight;
        pOutputEntry->mDstFrameInfo.VFrame.mpVirAddr[0] = pOutputEntry->mpDataVirAddr;
        pOutputEntry->mDstFrameInfo.VFrame.mpVirAddr[1] = pOutputEntry->mpDataVirAddr + pOutputEntry->mDstFrameInfo.VFrame.mWidth * pOutputEntry->mDstFrameInfo.VFrame.mHeight;
        pOutputEntry->mDstFrameInfo.VFrame.mpVirAddr[2] = pOutputEntry->mpDataVirAddr + pOutputEntry->mDstFrameInfo.VFrame.mWidth * pOutputEntry->mDstFrameInfo.VFrame.mHeight * 5 / 4;
        pOutputEntry->mDstFrameInfo.VFrame.mPhyAddr[0] = pOutputEntry->mDataPhyAddr;
        pOutputEntry->mDstFrameInfo.VFrame.mPhyAddr[1] = pOutputEntry->mDataPhyAddr + pOutputEntry->mDstFrameInfo.VFrame.mWidth * pOutputEntry->mDstFrameInfo.VFrame.mHeight;
        pOutputEntry->mDstFrameInfo.VFrame.mPhyAddr[2] = pOutputEntry->mDataPhyAddr + pOutputEntry->mDstFrameInfo.VFrame.mWidth * pOutputEntry->mDstFrameInfo.VFrame.mHeight * 5 / 4;

        pthread_mutex_lock(&pBufMgr->mFrameVeUsingListLock);
        list_move_tail(&pOutputEntry->mList, &pBufMgr->mFrameVeUsingList);
        pthread_mutex_unlock(&pBufMgr->mFrameVeUsingListLock);

        ret = AW_MPI_VENC_SendFrame(pContext->mVeChn, &pOutputEntry->mDstFrameInfo, 0);
        if (ret != SUCCESS)
        {
            pthread_mutex_lock(&pBufMgr->mFrameVeUsingListLock);
            pthread_mutex_lock(&pBufMgr->mFrameReadyListLock);
            list_move_tail(&pOutputEntry->mList, &pBufMgr->mFrameReadyList);
            pthread_mutex_unlock(&pBufMgr->mFrameReadyListLock);
            pthread_mutex_unlock(&pBufMgr->mFrameVeUsingListLock);
            aloge("send frame[%d] to venc fail", pOutputEntry->mId);
        }

    }

    return NULL;
}

static void *GetEncoderFrameThread(void *pThreadData)
{
    SAMPLE_AISR_S *pContext = (SAMPLE_AISR_S*)pThreadData;
    int ret = 0;
    int count = 0;
    char strThreadName[32] = "GetEncoderFrameThread";
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    VENC_CHN nVencChn = pContext->mVeChn;

    //set spspps
    VencHeaderData SpsPpsInfo;
    if (pContext->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVeChn, &SpsPpsInfo);
        if (SUCCESS == ret)
        {
            if (SpsPpsInfo.nLength)
            {
                fwrite(SpsPpsInfo.pBuffer, 1, SpsPpsInfo.nLength, pContext->mOutputFileFp);
            }
        }
        else
        {
            alogd("AW_MPI_VENC_GetH264SpsPpsInfo failed!\n");
            ret = -1;
        }
    }
    else if (pContext->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVeChn, &SpsPpsInfo);
        if (SUCCESS == ret)
        {
            if (SpsPpsInfo.nLength)
            {
                fwrite(SpsPpsInfo.pBuffer, 1, SpsPpsInfo.nLength, pContext->mOutputFileFp);
            }
        }
        else
        {
            alogd("AW_MPI_VENC_GetH265SpsPpsInfo failed!\n");
            ret = -1;
        }
    }

    VENC_STREAM_S VencFrame;
    VENC_PACK_S venc_pack;
    VencFrame.mPackCount = 1;
    VencFrame.mpPack = &venc_pack;

    while (!pContext->mExitFlag)
    {
        if ((ret = AW_MPI_VENC_GetStream(nVencChn, &VencFrame, 4000)) < 0) //6000(25fps) 4000(30fps)
        {
            continue;
        }
        else
        {
            if (VencFrame.mpPack != NULL && VencFrame.mpPack->mLen0)
            {
                fwrite(VencFrame.mpPack->mpAddr0, 1, VencFrame.mpPack->mLen0, pContext->mOutputFileFp);
            }
            if (VencFrame.mpPack != NULL && VencFrame.mpPack->mLen1)
            {
                fwrite(VencFrame.mpPack->mpAddr1, 1, VencFrame.mpPack->mLen1, pContext->mOutputFileFp);
            }
            ret = AW_MPI_VENC_ReleaseStream(nVencChn, &VencFrame);
            if (ret < 0)
            {
                alogd("falied error, release failed!!!\n");
            }
        }

        count++;
    }

    return NULL;
}

static void *ReadYuvThread(void *pThreadData)
{
    SAMPLE_AISR_S *pContext = (SAMPLE_AISR_S*)pThreadData;
    CsiSaveBufMgr *pBufMgr = pContext->mpSaveBufMgr;
    int ret = 0;
    int size = 0;
    struct list_head tmpList;
    char strThreadName[32] = "ReadYuvThread";
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    INIT_LIST_HEAD(&tmpList);

    pContext->mInputFileFp = fopen(pContext->mConfigPara.srcYuvFile, "rb");
    if (!pContext->mInputFileFp)
    {
        aloge("fatal error! open %s fail", pContext->mConfigPara.srcYuvFile);
        return NULL;
    }

    while (!pContext->mExitFlag)
    {
        pthread_mutex_lock(&pBufMgr->mInputYuvIdleListLock);
        SampleAisrSaveBufNode *pEntry = \
                list_first_entry_or_null(&pBufMgr->mInputYuvIdleList, SampleAisrSaveBufNode, mList);
        if (NULL == pEntry)
        {
            pthread_mutex_unlock(&pBufMgr->mInputYuvIdleListLock);
            usleep(1000);
            continue;
        }
        list_move_tail(&pEntry->mList, &tmpList);
        pthread_mutex_unlock(&pBufMgr->mInputYuvIdleListLock);

        size = fread(pEntry->mpDataVirAddr, 1, pEntry->mDataLen, pContext->mInputFileFp);
        AW_MPI_SYS_MmzFlushCache(pEntry->mDataPhyAddr, pEntry->mpDataVirAddr, pEntry->mDataLen);
        if (size != pEntry->mDataLen)
            break;

        pthread_mutex_lock(&pBufMgr->mInputYuvReadyListLock);
        list_move_tail(&pEntry->mList, &pBufMgr->mInputYuvReadyList);
        pthread_mutex_unlock(&pBufMgr->mInputYuvReadyListLock);
    }

    pContext->mInputRead2Eof = 1;
    fclose(pContext->mInputFileFp);
    return NULL;
}

static void *DoAisrThread(void *pThreadData)
{
    SAMPLE_AISR_S *pContext = (SAMPLE_AISR_S *)pThreadData;
    CsiSaveBufMgr *pBufMgr = pContext->mpSaveBufMgr;
    struct list_head tmpInputList;
    struct list_head tmpOutputList;
    int frm_cnt = 0;
    int ret = 0;

    char strThreadName[32] = "DoAisrThread";
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    INIT_LIST_HEAD(&tmpInputList);
    INIT_LIST_HEAD(&tmpOutputList);
    VIDEO_FRAME_INFO_S FrmInfo;

    while (!pContext->mExitFlag)
    {
        if ((list_empty(&pBufMgr->mInputYuvReadyList)) || (list_empty(&pBufMgr->mFrameIdleList)))
        {
            usleep(1000);
            continue;
        }

        pthread_mutex_lock(&pBufMgr->mInputYuvReadyListLock);
        SampleAisrSaveBufNode *pInputEntry = \
            list_first_entry(&pBufMgr->mInputYuvReadyList, SampleAisrSaveBufNode, mList);
        list_move_tail(&pInputEntry->mList, &tmpInputList);
        pthread_mutex_unlock(&pBufMgr->mInputYuvReadyListLock);

        pthread_mutex_lock(&pBufMgr->mFrameIdleListLock);
        SampleAisrSaveBufNode *pOutputEntry = \
            list_first_entry(&pBufMgr->mFrameIdleList, SampleAisrSaveBufNode, mList);
        list_move_tail(&pOutputEntry->mList, &tmpOutputList);
        pthread_mutex_unlock(&pBufMgr->mFrameIdleListLock);


        pthread_mutex_lock(&pBufMgr->mInputYuvUsingListLock);
        list_move_tail(&pInputEntry->mList, &pBufMgr->mInputYuvUsingList);
        pthread_mutex_unlock(&pBufMgr->mInputYuvUsingListLock);

        pthread_mutex_lock(&pBufMgr->mFrameAisrUsingListLock);
        list_move_tail(&pOutputEntry->mList, &pBufMgr->mFrameAisrUsingList);
        pthread_mutex_unlock(&pBufMgr->mFrameAisrUsingListLock);


        AwaisrImgBufInfo nInputBufInfo;
        memset(&nInputBufInfo, 0, sizeof(AwaisrImgBufInfo));
        AwaisrImgBufInfo nOutputBufInfo;
        memset(&nOutputBufInfo, 0, sizeof(AwaisrImgBufInfo));

        nInputBufInfo.mImgPhyBuf[0] = pInputEntry->mDataPhyAddr;
        nInputBufInfo.mImgPhyBuf[1] = pInputEntry->mDataPhyAddr + pContext->mConfigPara.srcWidth * pContext->mConfigPara.srcHeight;
        nInputBufInfo.mImgVirBuf[0] = pInputEntry->mpDataVirAddr;
        nInputBufInfo.mImgVirBuf[1] = pInputEntry->mpDataVirAddr + pContext->mConfigPara.srcWidth * pContext->mConfigPara.srcHeight;

        nOutputBufInfo.mImgPhyBuf[0] = pOutputEntry->mDataPhyAddr;
        nOutputBufInfo.mImgPhyBuf[1] = pOutputEntry->mDataPhyAddr + pContext->mConfigPara.mVencsrcWidth * pContext->mConfigPara.mVencsrcHeight;
        nOutputBufInfo.mImgVirBuf[0] = pOutputEntry->mpDataVirAddr;
        nOutputBufInfo.mImgVirBuf[1] = pOutputEntry->mpDataVirAddr + pContext->mConfigPara.mVencsrcWidth * pContext->mConfigPara.mVencsrcHeight;
        AwaisrSendFrame(nInputBufInfo, nOutputBufInfo);
    }

    return NULL;
}

static void *OutputYuvThread(void *pThreadData)
{
    struct list_head tmpList;
    SAMPLE_AISR_S *pContext = (SAMPLE_AISR_S*)pThreadData;
    CsiSaveBufMgr *pBufMgr = pContext->mpSaveBufMgr;
    int ret = 0;

    char strThreadName[32] = "OutputYuvThread";
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    pContext->mOutputFileFp = fopen(pContext->mConfigPara.dstYuvFile, "wb");
    if (!pContext->mOutputFileFp)
    {
        aloge("fatal error! open %s fail", pContext->mConfigPara.dstYuvFile);
        return NULL;
    }

    INIT_LIST_HEAD(&tmpList);
    while (!pContext->mExitFlag)
    {
        if (list_empty(&pBufMgr->mInputYuvReadyList) && list_empty(&pBufMgr->mInputYuvUsingList) &&
            pContext->mInputRead2Eof && list_empty(&pBufMgr->mFrameReadyList))
            cdx_sem_up(&gpAisrData->mSemExit);

        pthread_mutex_lock(&pBufMgr->mFrameReadyListLock);
        SampleAisrSaveBufNode *pEntry = \
                list_first_entry_or_null(&pBufMgr->mFrameReadyList, SampleAisrSaveBufNode, mList);
        if (NULL == pEntry)
        {
            pthread_mutex_unlock(&pBufMgr->mFrameReadyListLock);
            usleep(1000);
            continue;
        }
        list_move_tail(&pEntry->mList, &tmpList);
        pthread_mutex_unlock(&pBufMgr->mFrameReadyListLock);

        fwrite(pEntry->mpDataVirAddr, 1, pEntry->mDataLen, pContext->mOutputFileFp);

        pthread_mutex_lock(&pBufMgr->mFrameIdleListLock);
        list_move_tail(&pEntry->mList, &pBufMgr->mFrameIdleList);
        pthread_mutex_unlock(&pBufMgr->mFrameIdleListLock);
    }

    fclose(pContext->mOutputFileFp);
    return NULL;
}

static int initInputYuvBuf(SAMPLE_AISR_S *pContext)
{
    int ret = SUCCESS;
    CsiSaveBufMgr *pSaveBufMgr = pContext->mpSaveBufMgr;
    SampleAisrSaveBufNode *pEntry = NULL, *pTmp = NULL;
    INIT_LIST_HEAD(&pSaveBufMgr->mInputYuvIdleList);
    INIT_LIST_HEAD(&pSaveBufMgr->mInputYuvReadyList);
    INIT_LIST_HEAD(&pSaveBufMgr->mInputYuvUsingList);
    pthread_mutex_init(&pSaveBufMgr->mInputYuvIdleListLock, NULL);
    pthread_mutex_init(&pSaveBufMgr->mInputYuvReadyListLock, NULL);
    pthread_mutex_init(&pSaveBufMgr->mInputYuvUsingListLock, NULL);

    for (int i = 0; i < pContext->mConfigPara.mViBufferNum; i++)
    {
        SampleAisrSaveBufNode *pNode = malloc(sizeof(SampleAisrSaveBufNode));
        if (NULL == pNode)
        {
            aloge("fatal error! malloc save buf node fail!");
            ret = FAILURE;
            goto err1;
        }
        memset(pNode, 0, sizeof(SampleAisrSaveBufNode));
        pNode->mId = i;
        pNode->mDataLen = pContext->mConfigPara.srcWidth * pContext->mConfigPara.srcHeight * 3 / 2;
        AW_MPI_SYS_MmzAlloc_Cached(&pNode->mDataPhyAddr, &pNode->mpDataVirAddr, pNode->mDataLen);
        if ((0 == pNode->mDataPhyAddr) || (NULL == pNode->mpDataVirAddr))
        {
            aloge("fatal error! alloc buf[%d] fail!", i);
            ret = FAILURE;
            goto err1;
        }
        memset(pNode->mpDataVirAddr, 0, pNode->mDataLen);
        alogd("node[%d] alloc data len[%d] phy addr[%x] vir addr[%p]", \
            i, pNode->mDataLen, pNode->mDataPhyAddr, pNode->mpDataVirAddr);
        pthread_mutex_lock(&pSaveBufMgr->mInputYuvIdleListLock);
        list_add_tail(&pNode->mList, &pSaveBufMgr->mInputYuvIdleList);
        pthread_mutex_unlock(&pSaveBufMgr->mInputYuvIdleListLock);

        AwaisrImgBufInfo inputbufinfo;
        memset(&inputbufinfo, 0, sizeof(AwaisrImgBufInfo));
        inputbufinfo.mImgPhyBuf[0] = pNode->mDataPhyAddr;
        AwaisrVipBufferCreate(&inputbufinfo, NULL);
    }

    return ret;

err1:
    pthread_mutex_lock(&pSaveBufMgr->mInputYuvIdleListLock);
    list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mInputYuvIdleList, mList)
    {
        if (NULL == pEntry)
            continue;

        if (0 != pEntry->mDataPhyAddr && NULL != pEntry->mpDataVirAddr)
        {
            alogd("node[%d] free data len[%d] phy addr[%d] vir addr[%p]", \
                pEntry->mId, pEntry->mDataLen, pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            AW_MPI_SYS_MmzFree(pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            pEntry->mDataPhyAddr = 0;
            pEntry->mpDataVirAddr = NULL;
        }
        list_del(&pEntry->mList);
        free(pEntry);
        pEntry = NULL;
    }
    pthread_mutex_unlock(&pSaveBufMgr->mInputYuvIdleListLock);

    pthread_mutex_destroy(&pSaveBufMgr->mInputYuvIdleListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mInputYuvReadyListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mInputYuvUsingListLock);

    return ret;
}

static int deinitInputYuvBuf(SAMPLE_AISR_S *pContext)
{
    CsiSaveBufMgr *pSaveBufMgr = pContext->mpSaveBufMgr;
    if (NULL == pSaveBufMgr)
    {
        alogw("why save buf mgr is null");
        return -1;
    }

    SampleAisrSaveBufNode *pEntry = NULL, *pTmp = NULL;
    struct list_head tmpList;
    INIT_LIST_HEAD(&tmpList);

    if (!list_empty(&pSaveBufMgr->mInputYuvReadyList))
    {
        pthread_mutex_lock(&pSaveBufMgr->mInputYuvReadyListLock);
        list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mInputYuvReadyList, mList)
        {
            list_move_tail(&pEntry->mList, &tmpList);
        }
        pthread_mutex_unlock(&pSaveBufMgr->mInputYuvReadyListLock);
    }

    if (!list_empty(&pSaveBufMgr->mInputYuvUsingList))
    {
        pthread_mutex_lock(&pSaveBufMgr->mInputYuvUsingListLock);
        list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mInputYuvUsingList, mList)
        {
            list_move_tail(&pEntry->mList, &tmpList);
        }
        pthread_mutex_unlock(&pSaveBufMgr->mInputYuvUsingListLock);
    }

    pthread_mutex_lock(&pSaveBufMgr->mInputYuvIdleListLock);

    if (!list_empty(&tmpList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &tmpList, mList)
        {
            list_move_tail(&pEntry->mList, &pSaveBufMgr->mInputYuvIdleList);
        }
    }
    list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mInputYuvIdleList, mList)
    {
        if (NULL == pEntry)
            continue;

        if (0 != pEntry->mDataPhyAddr && NULL != pEntry->mpDataVirAddr)
        {
            alogd("node[%d] free data len[%d] phy addr[%d] vir addr[%p]", \
                pEntry->mId, pEntry->mDataLen, pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            AW_MPI_SYS_MmzFree(pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            pEntry->mDataPhyAddr = 0;
            pEntry->mpDataVirAddr = NULL;
        }
        list_del(&pEntry->mList);
        free(pEntry);
        pEntry = NULL;
    }
    pthread_mutex_unlock(&pSaveBufMgr->mInputYuvIdleListLock);

    pthread_mutex_destroy(&pSaveBufMgr->mInputYuvIdleListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mInputYuvReadyListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mInputYuvUsingListLock);

    return 0;
}

static int initSaveBufMgr(SAMPLE_AISR_S *pContext)
{
    CsiSaveBufMgr *pSaveBufMgr = NULL;
    SampleAisrSaveBufNode *pEntry = NULL, *pTmp = NULL;

    pSaveBufMgr = malloc(sizeof(CsiSaveBufMgr));
    if (NULL == pSaveBufMgr)
    {
        aloge("fatal error! save buffer mgr malloc fail!");
        return FAILURE;
    }
    memset(pSaveBufMgr, 0, sizeof(CsiSaveBufMgr));

    INIT_LIST_HEAD(&pSaveBufMgr->mFrameIdleList);
    INIT_LIST_HEAD(&pSaveBufMgr->mFrameAisrUsingList);
    INIT_LIST_HEAD(&pSaveBufMgr->mFrameReadyList);
    INIT_LIST_HEAD(&pSaveBufMgr->mFrameVeUsingList);
    pthread_mutex_init(&pSaveBufMgr->mFrameIdleListLock, NULL);
    pthread_mutex_init(&pSaveBufMgr->mFrameAisrUsingListLock, NULL);
    pthread_mutex_init(&pSaveBufMgr->mFrameReadyListLock, NULL);
    pthread_mutex_init(&pSaveBufMgr->mFrameVeUsingListLock, NULL);

    for (int i = 0; i < pContext->mConfigPara.mAisrOutputBufNum; i++)
    {
        SampleAisrSaveBufNode *pNode = malloc(sizeof(SampleAisrSaveBufNode));
        if (NULL == pNode)
        {
            aloge("fatal error! malloc save buf node fail!");
            goto err1;
        }
        memset(pNode, 0, sizeof(SampleAisrSaveBufNode));
        pNode->mId = i;
        pNode->mDataLen = pContext->mConfigPara.mVencsrcWidth * pContext->mConfigPara.mVencsrcHeight * 3 / 2;
        AW_MPI_SYS_MmzAlloc_Cached(&pNode->mDataPhyAddr, &pNode->mpDataVirAddr, pNode->mDataLen);
        if ((0 == pNode->mDataPhyAddr) || (NULL == pNode->mpDataVirAddr))
        {
            aloge("fatal error! alloc buf[%d] fail!", i);
            goto err1;
        }
        memset(pNode->mpDataVirAddr, 0, pNode->mDataLen);
        alogd("node[%d] alloc data len[%d] phy addr[%x] vir addr[%p]", \
            i, pNode->mDataLen, pNode->mDataPhyAddr, pNode->mpDataVirAddr);
        pthread_mutex_lock(&pSaveBufMgr->mFrameIdleListLock);
        list_add_tail(&pNode->mList, &pSaveBufMgr->mFrameIdleList);
        pthread_mutex_unlock(&pSaveBufMgr->mFrameIdleListLock);

        AwaisrImgBufInfo outputbufinfo;
        memset(&outputbufinfo, 0, sizeof(AwaisrImgBufInfo));
        outputbufinfo.mImgPhyBuf[0] = pNode->mDataPhyAddr;
        AwaisrVipBufferCreate(NULL, &outputbufinfo);
    }

    pContext->mpSaveBufMgr = pSaveBufMgr;

    if (pContext->mOpt1200wYuv)
    {
        if (initInputYuvBuf(pContext) != SUCCESS)
        {
            aloge("fatal error! init intput yuv buffer fail!");
            goto err1;
        }
    }

    return SUCCESS;

err1:
    pthread_mutex_lock(&pSaveBufMgr->mFrameIdleListLock);
    list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mFrameIdleList, mList)
    {
        if (NULL == pEntry)
            continue;

        if (0 != pEntry->mDataPhyAddr && NULL != pEntry->mpDataVirAddr)
        {
            alogd("node[%d] free data len[%d] phy addr[%d] vir addr[%p]", \
                pEntry->mId, pEntry->mDataLen, pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            AW_MPI_SYS_MmzFree(pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            pEntry->mDataPhyAddr = 0;
            pEntry->mpDataVirAddr = NULL;
        }
        list_del(&pEntry->mList);
        free(pEntry);
        pEntry = NULL;
    }
    pthread_mutex_unlock(&pSaveBufMgr->mFrameIdleListLock);

    pthread_mutex_destroy(&pSaveBufMgr->mFrameIdleListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mFrameAisrUsingListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mFrameReadyListLock);

    if (pSaveBufMgr)
        free(pSaveBufMgr);
    pContext->mpSaveBufMgr = NULL;

    return FAILURE;
}

static int deinitSaveBufMgr(SAMPLE_AISR_S *pContext)
{
    CsiSaveBufMgr *pSaveBufMgr = pContext->mpSaveBufMgr;

    if (NULL == pSaveBufMgr)
    {
        alogw("why save buf mgr is null");
        return -1;
    }

    SampleAisrSaveBufNode *pEntry = NULL, *pTmp = NULL;
    struct list_head tmpList;
    INIT_LIST_HEAD(&tmpList);

    if (!list_empty(&pSaveBufMgr->mFrameReadyList))
    {
        pthread_mutex_lock(&pSaveBufMgr->mFrameReadyListLock);
        list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mFrameReadyList, mList)
        {
            list_move_tail(&pEntry->mList, &tmpList);
        }
        pthread_mutex_unlock(&pSaveBufMgr->mFrameReadyListLock);
    }

    if (!list_empty(&pSaveBufMgr->mFrameAisrUsingList))
    {
        pthread_mutex_lock(&pSaveBufMgr->mFrameAisrUsingListLock);
        list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mFrameAisrUsingList, mList)
        {
            list_move_tail(&pEntry->mList, &tmpList);
        }
        pthread_mutex_unlock(&pSaveBufMgr->mFrameAisrUsingListLock);
    }

    if (!list_empty(&pSaveBufMgr->mFrameVeUsingList))
    {
        pthread_mutex_lock(&pSaveBufMgr->mFrameVeUsingListLock);
        list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mFrameVeUsingList, mList)
        {
            list_move_tail(&pEntry->mList, &tmpList);
        }
        pthread_mutex_unlock(&pSaveBufMgr->mFrameVeUsingListLock);
    }

    pthread_mutex_lock(&pSaveBufMgr->mFrameIdleListLock);

    if (!list_empty(&tmpList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &tmpList, mList)
        {
            list_move_tail(&pEntry->mList, &pSaveBufMgr->mFrameIdleList);
        }
    }
    list_for_each_entry_safe(pEntry, pTmp, &pSaveBufMgr->mFrameIdleList, mList)
    {
        if (NULL == pEntry)
            continue;

        if (0 != pEntry->mDataPhyAddr && NULL != pEntry->mpDataVirAddr)
        {
            alogd("node[%d] free data len[%d] phy addr[%d] vir addr[%p]", \
                pEntry->mId, pEntry->mDataLen, pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            AW_MPI_SYS_MmzFree(pEntry->mDataPhyAddr, pEntry->mpDataVirAddr);
            pEntry->mDataPhyAddr = 0;
            pEntry->mpDataVirAddr = NULL;
        }
        list_del(&pEntry->mList);
        free(pEntry);
        pEntry = NULL;
    }
    pthread_mutex_unlock(&pSaveBufMgr->mFrameIdleListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mFrameIdleListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mFrameAisrUsingListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mFrameReadyListLock);
    pthread_mutex_destroy(&pSaveBufMgr->mFrameVeUsingListLock);

    if (pContext->mOpt1200wYuv)
        deinitInputYuvBuf(pContext);

    free(pSaveBufMgr);
    pContext->mpSaveBufMgr = NULL;

    return 0;
}

static ERRORTYPE start(SAMPLE_AISR_S *pContext)
{
    ERRORTYPE ret = SUCCESS;

    alogd("start");

    if (pContext->mOpt1200wYuv)
    {
        ret = pthread_create(&pContext->mInputYUVThreadId, NULL, ReadYuvThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create Get Csi Frame Thread fail[%d]", ret);
            return ret;
        }

        ret = pthread_create(&pContext->mDoAisrThreadId, NULL, DoAisrThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create Msg Queue Thread fail[%d]", ret);
            return ret;
        }

        ret = pthread_create(&pContext->mOutputYUVThreadId, NULL, OutputYuvThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create Msg Queue Thread fail[%d]", ret);
            return ret;
        }
    }
    else
    {
        VIDEO_FRAME_INFO_S stFrameInfo[pContext->mViAttr.nbufs];
        memset(stFrameInfo, 0 , sizeof(VIDEO_FRAME_INFO_S) * pContext->mViAttr.nbufs);
        if (0 != AW_MPI_VI_GetVippBuffer(pContext->mViDev, stFrameInfo, pContext->mViAttr.nbufs))
        {
            aloge("fatal error! get vi buffer fail");
            return FAILURE;
        }

        AwaisrImgBufInfo inputbufinfo;
        memset(&inputbufinfo, 0, sizeof(AwaisrImgBufInfo));
        for (int i = 0; i < pContext->mViAttr.nbufs; i++)
        {
            inputbufinfo.mImgPhyBuf[0] = stFrameInfo[i].VFrame.mPhyAddr[0];
            AwaisrVipBufferCreate(&inputbufinfo, NULL);
        }

        ret = pthread_create(&pContext->mCSIFrameThreadId, NULL, GetCsiFrameThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create Get Csi Frame Thread fail[%d]", ret);
            return ret;
        }

        ret = pthread_create(&pContext->mGetStreamThreadId, NULL, GetEncoderFrameThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create Msg Queue Thread fail[%d]", ret);
            return ret;
        }

        ret = pthread_create(&pContext->mSendEncFrameThreadId, NULL, SendEncoderFrameThread, pContext);
        if (ret != 0)
        {
            aloge("fatal error! create Msg Queue Thread fail[%d]", ret);
            return ret;
        }

        ret = AW_MPI_VI_EnableVirChn(pContext->mViDev, pContext->mViChn);
        if (ret != SUCCESS)
        {
            alogd("VI enable error!");
            return FAILURE;
        }

        if (pContext->mVeChn >= 0)
        {
            AW_MPI_VENC_StartRecvPic(pContext->mVeChn);
        }
    }

    return ret;
}

static ERRORTYPE stop(SAMPLE_AISR_S *pContext)
{
    ERRORTYPE ret = SUCCESS;

    alogd("stop");

    if (pContext->mOpt1200wYuv)
        return SUCCESS;

    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pContext->mViDev, pContext->mViChn);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pContext->mVeChn);
    }

    pthread_join(pContext->mCSIFrameThreadId, NULL);
    pthread_join(pContext->mGetStreamThreadId, NULL);
    pthread_join(pContext->mSendEncFrameThreadId, NULL);

    return SUCCESS;
}

static ERRORTYPE destroy(SAMPLE_AISR_S *pContext)
{
    ERRORTYPE ret = SUCCESS;

    alogd("destroy");

    if (pContext->mOpt1200wYuv)
    {
        pthread_join(pContext->mInputYUVThreadId, NULL);
        pthread_join(pContext->mDoAisrThreadId, NULL);
        pthread_join(pContext->mOutputYUVThreadId, NULL);
    }
    else
    {
        if (pContext->mVeChn >= 0)
        {
            alogd("destory venc");
            AW_MPI_VENC_DestroyChn(pContext->mVeChn);
            pContext->mVeChn = MM_INVALID_CHN;
        }
        if (pContext->mViChn >= 0)
        {
            AW_MPI_VI_DestroyVirChn(pContext->mViDev, pContext->mViChn);
            AW_MPI_VI_DisableVipp(pContext->mViDev);
        #if ISP_RUN
            AW_MPI_ISP_Stop(pContext->mIspDev);
        #endif
            AW_MPI_VI_DestroyVipp(pContext->mViDev);
        }

        if (pContext->mOutputFileFp)
        {
            fclose(pContext->mOutputFileFp);
            pContext->mOutputFileFp = NULL;
        }
    }

    alogd("here");
    return SUCCESS;
}

int main(int argc, char **argv)
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

    alogd("sample_aisr demo running!\n");
    SAMPLE_AISR_S *pContext = (SAMPLE_AISR_S *)malloc(sizeof(SAMPLE_AISR_S));
    if (NULL == pContext)
    {
        aloge("malloc struct fail");
        result = FAILURE;
        goto _err0;
    }
    InitAisrData(pContext);
    gpAisrData = pContext;
    cdx_sem_init(&pContext->mSemExit, 0);

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
    if (argc > 1)
    {
        pConfPath = pContext->mCmdLinePara.mConfigFilePath;
    }

    if (loadConfigPara(pContext, pConfPath) != SUCCESS)
    {
        aloge("load config file fail");
        result = FAILURE;
        goto err_out_0;
    }

    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    if (0 != createAisr(pContext))
        goto err_out_1;

    if (initSaveBufMgr(pContext) != SUCCESS)
    {
        aloge("fatal error! init save csi frame mgr fail!");
        goto err_out_2;
    }

    if (prepare(pContext) != SUCCESS)
    {
        aloge("prepare fail!");
        goto err_out_2;
    }

    if(SUCCESS != start(pContext))
    {
        aloge("start fail!");
        goto err_out_3;
    }

    if (pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration * 1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    result = SUCCESS;
    alogd("sample_aisr demo start to exit");

    pContext->mExitFlag = 1;
    stop(pContext);
err_out_3:
    destroy(pContext);
err_out_2:
    deinitSaveBufMgr(pContext);
    AwaisrVipBufferDestroyAll();
    AwaisrClose();
err_out_1:
    AW_MPI_SYS_Exit();
err_out_0:
    cdx_sem_deinit(&pContext->mSemExit);
    free(pContext);
    gpAisrData = pContext = NULL;
_err0:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
