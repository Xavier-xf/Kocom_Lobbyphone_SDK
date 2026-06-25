//#define LOG_NDEBUG 0
#define LOG_TAG "SampleVirvi2VencSync"

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "plat_log.h"
#include <mm_common.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_vi_private.h>
#include "sample_virvi2vencSync.h"
#include "sample_virvi2vencSync_config.h"
#include "../common/sample_common_venc.h"

#define ISP_RUN (1)

static SAMPLE_VIRVI2VENCSYNC_S *gpVirvi2VencSyncData = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpVirvi2VencSyncData)
    {
        cdx_sem_up(&gpVirvi2VencSyncData->mSemExit);
    }
}

static ERRORTYPE InitVirvi2VencSyncData(SAMPLE_VIRVI2VENCSYNC_S *pContext)
{
    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }
    memset(pContext, 0, sizeof(SAMPLE_VIRVI2VENCSYNC_S));
    pContext->mVeChn = MM_INVALID_CHN;
    pContext->mViChn = MM_INVALID_CHN;
    pContext->mViDev = MM_INVALID_DEV;

    return SUCCESS;
}

static ERRORTYPE parseCmdLine(SAMPLE_VIRVI2VENCSYNC_S *pContext, int argc, char** argv)
{
    ERRORTYPE ret = FAILURE;

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

              strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
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

static ERRORTYPE loadConfigPara(SAMPLE_VIRVI2VENCSYNC_S *pContext, const char *conf_path)
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

        pContext->mConfigPara.dstWidth = GetConfParaInt(&mConf, CFG_DST_VIDEO_WIDTH, 0);
        pContext->mConfigPara.dstHeight = GetConfParaInt(&mConf, CFG_DST_VIDEO_HEIGHT, 0);
        alogd("dstWidth: %d, dstHeight: %d", pContext->mConfigPara.dstWidth, pContext->mConfigPara.dstHeight);

        ptr = (char *)GetConfParaString(&mConf, CFG_SRC_PIXFMT, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "nv21"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yv12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_PLANAR_420;
            }
            else if (!strcmp(ptr, "nv12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            }
            else if (!strcmp(ptr, "yu12"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_PLANAR_420;
            }
            else if (!strcmp(ptr, "aw_lbc_2_0x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
            }
            else if (!strcmp(ptr, "aw_lbc_2_5x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_5x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_1_0x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
            }
            else
            {
                aloge("fatal error! wrong src pixfmt:%s", ptr);
                alogw("use the default pixfmt %d", pContext->mConfigPara.srcPixFmt);
            }
        }

        ptr = (char *)GetConfParaString(&mConf, CFG_COLOR_SPACE, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "jpeg"))
            {
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
            }
            else if (!strcmp(ptr, "rec709"))
            {
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709;
            }
            else if (!strcmp(ptr, "rec709_part_range"))
            {
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
            }
            else
            {
                aloge("fatal error! wrong color space:%s", ptr);
                pContext->mConfigPara.mColorSpace = V4L2_COLORSPACE_JPEG;
            }
        }

        alogd("srcPixFmt=%d, ColorSpace=%d", pContext->mConfigPara.srcPixFmt, pContext->mConfigPara.mColorSpace);

        ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_FILE_STR, NULL);
        if (ptr != NULL)
        {
            strcpy(pContext->mConfigPara.dstVideoFile, ptr);
        }

        pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_FRAMERATE, 0);
        pContext->mConfigPara.mViBufferNum = GetConfParaInt(&mConf, CFG_DST_VI_BUFFER_NUM, 0);
        pContext->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_BITRATE, 0);

        pContext->mConfigPara.mProductMode = GetConfParaInt(&mConf, CFG_PRODUCT_MODE, 0);
        //pContext->mConfigPara.mSensorType = GetConfParaInt(&mConf, CFG_SENSOR_TYPE, 0);
        pContext->mConfigPara.mKeyFrameInterval = GetConfParaInt(&mConf, CFG_KEY_FRAME_INTERVAL, 0);

        pContext->mConfigPara.mRcMode = GetConfParaInt(&mConf, CFG_RC_MODE, 0);
        pContext->mConfigPara.mInitQp = GetConfParaInt(&mConf, CFG_INIT_QP, 0);
        pContext->mConfigPara.mMinIQp = GetConfParaInt(&mConf, CFG_MIN_I_QP, 0);
        pContext->mConfigPara.mMaxIQp = GetConfParaInt(&mConf, CFG_MAX_I_QP, 0);
        pContext->mConfigPara.mMinPQp = GetConfParaInt(&mConf, CFG_MIN_P_QP, 0);
        pContext->mConfigPara.mMaxPQp = GetConfParaInt(&mConf, CFG_MAX_P_QP, 0);
        pContext->mConfigPara.mEnMbQpLimit = GetConfParaInt(&mConf, CFG_MB_QP_LIMIT, 0);
        pContext->mConfigPara.mMovingTh = GetConfParaInt(&mConf, CFG_MOVING_TH, 0);
        pContext->mConfigPara.mQuality = GetConfParaInt(&mConf, CFG_QUALITY, 0);
        pContext->mConfigPara.mPBitsCoef = GetConfParaInt(&mConf, CFG_P_BITS_COEF, 0);
        pContext->mConfigPara.mIBitsCoef = GetConfParaInt(&mConf, CFG_I_BITS_COEF, 0);

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
                aloge("error conf encoder type");
            }
        }

        pContext->mConfigPara.mTestDuration = GetConfParaInt(&mConf, CFG_TEST_DURATION, 0);

        pContext->mConfigPara.mEncUseProfile = GetConfParaInt(&mConf, CFG_DST_ENCODE_PROFILE, 0);

        alogd("vipp:%d, frame rate:%d, bitrate:%d, test_time=%d, profile=%d", pContext->mConfigPara.mVippDev,\
            pContext->mConfigPara.mVideoFrameRate, pContext->mConfigPara.mVideoBitRate,\
            pContext->mConfigPara.mTestDuration,\
            pContext->mConfigPara.mEncUseProfile);

        pContext->mConfigPara.mOnlineEnable = GetConfParaInt(&mConf, CFG_online_en, 0);
        pContext->mConfigPara.mOnlineShareBufNum = GetConfParaInt(&mConf, CFG_online_share_buf_num, 0);
        alogd("OnlineEnable: %d, OnlineShareBufNum: %d", pContext->mConfigPara.mOnlineEnable,
            pContext->mConfigPara.mOnlineShareBufNum);

        if (0 == pContext->mConfigPara.mOnlineEnable)
        {
            // vi drop frame only support offline.
            pContext->mConfigPara.mViDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("ViDropFrameNum: %d", pContext->mConfigPara.mViDropFrameNum);
        }
        else
        {
            // venc drop frame support online and offline.
            pContext->mConfigPara.mVencDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("VencDropFrameNum: %d", pContext->mConfigPara.mVencDropFrameNum);
        }

        pContext->mConfigPara.mEncppEnable = GetConfParaInt(&mConf, CFG_EncppEnable, 0);
        pContext->mConfigPara.mIspAndVeLinkageEnable = GetConfParaInt(&mConf, CFG_IspAndVeLinkageEnable, 0);
        alogd("EncppEnable: %d, IspAndVeLinkageEnable: %d", pContext->mConfigPara.mEncppEnable, pContext->mConfigPara.mIspAndVeLinkageEnable);

        destroyConfParser(&mConf);
    }

    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_VIRVI2VENCSYNC_S *pContext = (SAMPLE_VIRVI2VENCSYNC_S *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        switch(event)
        {
            /*case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                Isp2VeLinkageParam stIsp2Ve;
                memset(&stIsp2Ve, 0, sizeof(Isp2VeLinkageParam));
                stIsp2Ve.mIspAndVeLinkageEnable = pContext->mConfigPara.mIspAndVeLinkageEnable;
                stIsp2Ve.mCameraAdaptiveMovingAndStaticEnable = 0;
                stIsp2Ve.mVEncChn = mVEncChn;
                stIsp2Ve.mVipp = pContext->mConfigPara.mVippDev;
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
                stVe2Isp.mIspAndVeLinkageEnable = pContext->mConfigPara.mIspAndVeLinkageEnable;
                stVe2Isp.mVEncChn = mVEncChn;
                stVe2Isp.mVipp = pContext->mConfigPara.mVippDev;
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

static ERRORTYPE configVencChnAttr(SAMPLE_VIRVI2VENCSYNC_S *pContext)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;

    memset(&pContext->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mVencChnAttr.VeAttr.mOnlineEnable = 1;
        pContext->mVencChnAttr.VeAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    }
    pContext->mVencChnAttr.VeAttr.Type = pContext->mConfigPara.mVideoEncoderFmt;
    pContext->mVencChnAttr.VeAttr.MaxKeyInterval = pContext->mConfigPara.mKeyFrameInterval;
    pContext->mVencChnAttr.VeAttr.SrcPicWidth  = pContext->mConfigPara.srcWidth;
    pContext->mVencChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.srcHeight;
    pContext->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mVencChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.srcPixFmt;
    pContext->mVencChnAttr.VeAttr.mColorSpace = pContext->mConfigPara.mColorSpace;
    alogd("pixfmt:0x%x, colorSpace:0x%x", pContext->mVencChnAttr.VeAttr.PixelFormat, pContext->mVencChnAttr.VeAttr.mColorSpace);
    pContext->mVencChnAttr.VeAttr.mDropFrameNum = pContext->mConfigPara.mVencDropFrameNum;
    alogd("DropFrameNum:%d", pContext->mVencChnAttr.VeAttr.mDropFrameNum);
    pContext->mVencChnAttr.EncppAttr.eEncppSharpSetting = pContext->mConfigPara.mEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    if (pContext->mConfigPara.mVideoFrameRate)
    {
        vbvThreshSize = pContext->mConfigPara.mVideoBitRate/8/pContext->mConfigPara.mVideoFrameRate*15;
    }
    vbvBufSize = pContext->mConfigPara.mVideoBitRate/8*4 + vbvThreshSize;
    alogd("vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH264e.BufSize = vbvBufSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = vbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH264e.Profile = map_H264_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH264e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH264Vbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH264Vbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH264Vbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH264Vbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH264Vbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH264Vbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            pContext->mVencRcParam.ParamH264Vbr.mMovingTh = pContext->mConfigPara.mMovingTh;
            pContext->mVencRcParam.ParamH264Vbr.mQuality = pContext->mConfigPara.mQuality;
            pContext->mVencRcParam.ParamH264Vbr.mIFrmBitsCoef = pContext->mConfigPara.mIBitsCoef;
            pContext->mVencRcParam.ParamH264Vbr.mPFrmBitsCoef = pContext->mConfigPara.mPBitsCoef;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = pContext->mConfigPara.mMinPQp;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            break;
        case 3:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mRatioChangeQp = 85;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mQuality = 8;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMinIQp = 20;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH264Abr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH264Cbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH264Cbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH264Cbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH264Cbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH264Cbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            break;
        }
    }
    else if (PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH265e.mBufSize = vbvBufSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = vbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mProfile = map_H265_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH265e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH265Vbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH265Vbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH265Vbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH265Vbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH265Vbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH265Vbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            pContext->mVencRcParam.ParamH265Vbr.mMovingTh = pContext->mConfigPara.mMovingTh;
            pContext->mVencRcParam.ParamH265Vbr.mQuality = pContext->mConfigPara.mQuality;
            pContext->mVencRcParam.ParamH265Vbr.mIFrmBitsCoef = pContext->mConfigPara.mIBitsCoef;
            pContext->mVencRcParam.ParamH265Vbr.mPFrmBitsCoef = pContext->mConfigPara.mPBitsCoef;
            break;
        case 2:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = pContext->mConfigPara.mMinPQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            break;
        case 3:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mRatioChangeQp = 85;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mQuality = pContext->mConfigPara.mQuality;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMinIQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencChnAttr.RcAttr.mAttrH265Abr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            break;
        case 0:
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH265Cbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH265Cbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH265Cbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH265Cbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH265Cbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            break;
        }
    }
    else if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mBufSize = vbvBufSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.dstHeight;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 0:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            break;
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            pContext->mVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            break;
        case 2:
        case 3:
            aloge("not support! use default cbr mode");
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        default:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        }
    }

    alogd("venc set Rcmode=%d", pContext->mVencChnAttr.RcAttr.mRcMode);

    pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->mVencChnAttr.GopAttr.mGopSize = 2;

    return SUCCESS;
}

static ERRORTYPE createVencChn(SAMPLE_VIRVI2VENCSYNC_S *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configVencChnAttr(pContext);
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mVeChn = 0;
        alogd("online: only vipp0 & Vechn0 support online.");
    }
    else
    {
        pContext->mVeChn = pContext->mConfigPara.mVeChn;
    }

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

        /* set framerate in AW_MPI_VENC_CreateChn */
        /*VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
        stFrameRate.DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
        alogd("set venc framerate: src %dfps, dst %dfps", stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
        AW_MPI_VENC_SetFrameRate(pContext->mVeChn, &stFrameRate);*/

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mVeChn, &cbInfo);

        VENC_IspVeLinkAttr stIspVeLinkAttr;
        memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
        stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
        stIspVeLinkAttr.bEnableVe2Isp = FALSE;
        stIspVeLinkAttr.nVipp = pContext->mViDev;
        AW_MPI_VENC_EnableIspVeLink(pContext->mVeChn, &stIspVeLinkAttr);
        alogd("VencChn[%d] ispVeLink:%d-%d-%d", pContext->mVeChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
            stIspVeLinkAttr.nVipp);

        return SUCCESS;
    }
}

static ERRORTYPE createViChn(SAMPLE_VIRVI2VENCSYNC_S *pContext)
{
    ERRORTYPE ret;

    //create vi channel
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mViDev = 0;
        alogd("online: only vipp0 & Vechn0 support online.");
    }
    else
    {
        pContext->mViDev = pContext->mConfigPara.mVippDev;
    }
    pContext->mIspDev = 0;
    pContext->mViChn = 0;

    ret = AW_MPI_VI_CreateVipp(pContext->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
    }

    memset(&pContext->mViAttr, 0, sizeof(VI_ATTR_S));
    if (pContext->mConfigPara.mOnlineEnable)
    {
        pContext->mViAttr.mOnlineEnable = 1;
        pContext->mViAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    }
    pContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.srcPixFmt);
    pContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mViAttr.format.colorspace = pContext->mConfigPara.mColorSpace;
    pContext->mViAttr.format.width = pContext->mConfigPara.srcWidth;
    pContext->mViAttr.format.height = pContext->mConfigPara.srcHeight;
    pContext->mViAttr.nbufs =  pContext->mConfigPara.mViBufferNum;
    alogd("vipp use %d v4l2 buffers, colorspace: 0x%x", pContext->mViAttr.nbufs, pContext->mViAttr.format.colorspace);
    pContext->mViAttr.nplanes = 2;
    pContext->mViAttr.wdr_mode = 0;
    alogd("wdr_mode %d", pContext->mViAttr.wdr_mode);
    pContext->mViAttr.fps = pContext->mConfigPara.mVideoFrameRate;
    pContext->mViAttr.drop_frame_num = pContext->mConfigPara.mViDropFrameNum;
    pContext->mViAttr.mbEncppEnable = pContext->mConfigPara.mEncppEnable;

    ret = AW_MPI_VI_SetVippAttr(pContext->mViDev, &pContext->mViAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }
#if ISP_RUN
    AW_MPI_ISP_Run(pContext->mIspDev);
#endif

    ViVirChnAttrS stVirChnAttr;
    memset(&stVirChnAttr, 0, sizeof(ViVirChnAttrS));
    stVirChnAttr.mbRecvInIdleState = TRUE;
    ret = AW_MPI_VI_CreateVirChn(pContext->mViDev, pContext->mViChn, &stVirChnAttr);
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

static ERRORTYPE prepare(SAMPLE_VIRVI2VENCSYNC_S *pContext)
{
    ERRORTYPE result = FAILURE;

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

static ERRORTYPE start(SAMPLE_VIRVI2VENCSYNC_S *pContext)
{
    ERRORTYPE ret = SUCCESS;

    alogd("start");

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

    return ret;
}

static ERRORTYPE stop(SAMPLE_VIRVI2VENCSYNC_S *pContext)
{
    ERRORTYPE ret = SUCCESS;

    alogd("stop");

    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pContext->mViDev, pContext->mViChn);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pContext->mVeChn);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("destory venc");
        //AW_MPI_VENC_ResetChn(pContext->mVeChn);
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

    return SUCCESS;
}

static void *GetCSIFrameThread(void *pThreadData)
{
    SAMPLE_VIRVI2VENCSYNC_S *pContext = (SAMPLE_VIRVI2VENCSYNC_S*)pThreadData;
    int ret = 0;
    VI_DEV nViDev = pContext->mViDev;
    VI_CHN nViChn = pContext->mViChn;
    VENC_CHN nVencChn = pContext->mVeChn;

    VIDEO_FRAME_INFO_S stFrameInfo;
    memset(&stFrameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));

    while (0 == pContext->mExitFlag)
    {
        if ((ret = AW_MPI_VI_GetFrame(nViDev, nViChn, &stFrameInfo, 1000)) < 0)
        {
            aloge("fatal error, vi get frame failed!");
            continue;
        }

        ret = AW_MPI_VENC_SendFrameSync(nVencChn, &stFrameInfo, 0);
        if (ret < 0)
        {
            aloge("fatal error, venc send frame sync failed!");
        }

        ret = AW_MPI_VI_ReleaseFrame(nViDev, nViChn, &stFrameInfo);
        if (ret < 0)
        {
            aloge("fatal error, vi release frame failed!");
        }
    }

    return NULL;
}

static void *GetEncoderStreamThread(void *pThreadData)
{
    SAMPLE_VIRVI2VENCSYNC_S *pContext = (SAMPLE_VIRVI2VENCSYNC_S*)pThreadData;
    int ret = 0;
    VENC_CHN nVencChn = pContext->mVeChn;

    VencHeaderData SpsPpsInfo;
    if (pContext->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        if ((ret = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVeChn, &SpsPpsInfo)) == SUCCESS)
        {
            if (SpsPpsInfo.nLength)
                fwrite(SpsPpsInfo.pBuffer, 1, SpsPpsInfo.nLength, pContext->mOutputFileFp);
        }
        else
            aloge("fatal error, venc get H264SpsPpsInfo failed!");
    }
    else if (pContext->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        if ((ret = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVeChn, &SpsPpsInfo)) == SUCCESS)
        {
            if (SpsPpsInfo.nLength)
                fwrite(SpsPpsInfo.pBuffer, 1, SpsPpsInfo.nLength, pContext->mOutputFileFp);
        }
        else
            aloge("fatal error, venc get H265SpsPpsInfo failed!");
    }

    VENC_STREAM_S VencFrame;
    memset(&VencFrame, 0, sizeof(VENC_STREAM_S));
    VENC_PACK_S venc_pack;
    memset(&venc_pack, 0, sizeof(VENC_PACK_S));
    VencFrame.mPackCount = 1;
    VencFrame.mpPack = &venc_pack;

    while (0 == pContext->mExitFlag)
    {
        if ((ret = AW_MPI_VENC_GetStream(nVencChn, &VencFrame, 4000)) < 0)
        {
            aloge("fatal error, venc get stream failed!");
            continue;
        }

        if (VencFrame.mpPack != NULL && VencFrame.mpPack->mLen0)
        {
            fwrite(VencFrame.mpPack->mpAddr0,1,VencFrame.mpPack->mLen0, pContext->mOutputFileFp);
        }
        if (VencFrame.mpPack != NULL && VencFrame.mpPack->mLen1)
        {
            fwrite(VencFrame.mpPack->mpAddr1,1,VencFrame.mpPack->mLen1, pContext->mOutputFileFp);
        }

        ret = AW_MPI_VENC_ReleaseStream(nVencChn, &VencFrame);
        if (ret < 0)
        {
            aloge("fatal error, venc release stream failed!");
        }
    }

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

    printf("sample_virvi2vencSync running!\n");
    SAMPLE_VIRVI2VENCSYNC_S *pContext = (SAMPLE_VIRVI2VENCSYNC_S* )malloc(sizeof(SAMPLE_VIRVI2VENCSYNC_S));
    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        result = FAILURE;
        goto _err0;
    }
    if (InitVirvi2VencSyncData(pContext) != SUCCESS)
    {
        return -1;
    }
    gpVirvi2VencSyncData = pContext;
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
    if(argc > 1)
    {
        pConfPath = pContext->mCmdLinePara.mConfigFilePath;
    }
        
    if (loadConfigPara(pContext, pConfPath) != SUCCESS)
    {
        aloge("load config file fail");
        result = FAILURE;
        goto err_out_0;
    }
    alogd("ViDropFrameNum=%d", pContext->mConfigPara.mViDropFrameNum);

    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    if (prepare(pContext) != SUCCESS)
    {
        aloge("prepare fail!");
        goto err_out_3;
    }

    result = pthread_create(&pContext->mCSIFrameThreadId, NULL, GetCSIFrameThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create GetCSIFrame Thread fail[%d]", result);
        goto err_out_3;
    }
    else
    {
        alogd("create GetCSIFrame Thread success! threadId[0x%x]", &pContext->mCSIFrameThreadId);
    }

    result = pthread_create(&pContext->mThreadId, NULL, GetEncoderStreamThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create GetEncoderStream Thread fail[%d]", result);
        goto err_out_3;
    }
    else
    {
        alogd("create GetEncoderStream Thread success! threadId[0x%x]", &pContext->mThreadId);
    }

    start(pContext);

    if (pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    pContext->mExitFlag = 1;
    pthread_join(pContext->mThreadId, NULL);
    pthread_join(pContext->mCSIFrameThreadId, NULL);
    alogd("start to free res");
err_out_3:
    stop(pContext);
    result = 0;
err_out_1:
    AW_MPI_SYS_Exit();

err_out_0:
    cdx_sem_deinit(&pContext->mSemExit);
    free(pContext);
    gpVirvi2VencSyncData = pContext = NULL;
_err0:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
