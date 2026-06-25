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
#define LOG_TAG "SampleVirVi2Venc2Muxer"

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "plat_log.h"
#include <mm_common.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_region.h>
#include <mpi_vi_private.h>
#include "sample_vi2venc2muxer.h"
#include "sample_vi2venc2muxer_conf.h"
#include <file_common.h>
#include <sample_common_venc.h>
#include <cdx_list.h>

//#define TEST_LEAKTRACER

#ifdef TEST_LEAKTRACER
#include <LeakTracer/leaktracer.h>
#endif

#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)
//#define DOUBLE_ENCODER_FILE_OUT
#define ISP_RUN (1)
#define TEST_DROP_FRAME (0)

static SAMPLE_VI2VENC2MUXER_S *gpVi2Venc2MuxerData;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpVi2Venc2MuxerData)
    {
        cdx_sem_up(&gpVi2Venc2MuxerData->mSemExit);
    }
}

static int setOutputFileSync(SAMPLE_VI2VENC2MUXER_S *pContext, char* path, int64_t fallocateLength, int muxChn);


static ERRORTYPE InitVi2Venc2MuxerData(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }
    memset(pContext, 0, sizeof(SAMPLE_VI2VENC2MUXER_S));
    pContext->mMuxChn = MM_INVALID_CHN;
    pContext->mVeChn = MM_INVALID_CHN;
    pContext->mViChn = MM_INVALID_CHN;
    pContext->mViDev = MM_INVALID_DEV;

    INIT_LIST_HEAD(&pContext->mMuxerFileListArray);

    pContext->mCurrentState = REC_NOT_PREPARED;

    if (message_create(&pContext->mMsgQueue) < 0)
    {
        aloge("message create fail!");
        return FAILURE;
    }

    return SUCCESS;
}

static eGdcWarpType parserGdcWarpMode(char *pStr)
{
    if (!strcmp(pStr, "LDC"))
    {
        return Gdc_Warp_LDC;
    }
    else if (!strcmp(pStr, "LDC_Pro"))
    {
        return Gdc_Warp_LDC_Pro;
    }
    else
    {
        aloge("unsupport gdc warp mode[%s]", pStr);
    }
    return -1;
}

static ERRORTYPE parseCmdLine(SAMPLE_VI2VENC2MUXER_S *pContext, int argc, char** argv)
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

static ERRORTYPE loadConfigPara(SAMPLE_VI2VENC2MUXER_S *pContext, const char *conf_path)
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
            else if (!strcmp(ptr, "aw_lbc_2_5x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            }
            else if (!strcmp(ptr, "aw_lbc_2_0x"))
            {
                pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
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

        pContext->mConfigPara.mSaturationChange = GetConfParaInt(&mConf, CFG_SATURATION_CHANGE, 0);
        alogd("SaturationChange=%d", pContext->mConfigPara.mSaturationChange);
        
        ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_FILE_STR, NULL);
        if (ptr != NULL)
        {
            strcpy(pContext->mConfigPara.dstVideoFile, ptr);
        }

        pContext->mConfigPara.mVbrOptEnable = GetConfParaInt(&mConf, CFG_VBR_OPT_ENABLE, 0);

        pContext->mConfigPara.mbAddRepairInfo = GetConfParaInt(&mConf, CFG_ADD_REPAIR_INFO, 0);
        pContext->mConfigPara.mMaxFrmsTagInterval = GetConfParaInt(&mConf, CFG_FRMSTAG_BACKUP_INTERVAL, 0);
        pContext->mConfigPara.mDstFileMaxCnt = GetConfParaInt(&mConf, CFG_DST_FILE_MAX_CNT, 0);
        pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_FRAMERATE, 0);
        pContext->mConfigPara.mViBufferNum = GetConfParaInt(&mConf, CFG_DST_VI_BUFFER_NUM, 0);
        pContext->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_BITRATE, 0);
        pContext->mConfigPara.mMaxFileDuration = GetConfParaInt(&mConf, CFG_DST_VIDEO_DURATION, 0);
        pContext->mConfigPara.mVideoSkipFrameMode = GetConfParaInt(&mConf, CFG_DST_VIDEO_SKIP_FRAME_MODE, 0);

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
        pContext->mConfigPara.mGopMode = GetConfParaInt(&mConf, CFG_GOP_MODE, 0);
        pContext->mConfigPara.mGopSize = GetConfParaInt(&mConf, CFG_GOP_SIZE, 0);
        pContext->mConfigPara.mAdvancedRef_Base = GetConfParaInt(&mConf, CFG_AdvancedRef_Base, 0);
        pContext->mConfigPara.mAdvancedRef_Enhance = GetConfParaInt(&mConf, CFG_AdvancedRef_Enhance, 0);
        pContext->mConfigPara.mAdvancedRef_RefBaseEn = GetConfParaInt(&mConf, CFG_AdvancedRef_RefBaseEn, 0);
        pContext->mConfigPara.mEnableFastEnc = GetConfParaInt(&mConf, CFG_FAST_ENC, 0);
        pContext->mConfigPara.mbEnableSmart = GetConfParaBoolean(&mConf, CFG_ENABLE_SMART, 0);
        pContext->mConfigPara.mSVCLayer = GetConfParaInt(&mConf, CFG_SVC_LAYER, 0);
        pContext->mConfigPara.mEncodeRotate = GetConfParaInt(&mConf, CFG_ENCODE_ROTATE, 0);

        pContext->mConfigPara.m2DnrPara.enable_2d_filter = GetConfParaInt(&mConf, CFG_2DNR_EN, 0);
        pContext->mConfigPara.m2DnrPara.filter_strength_y = GetConfParaInt(&mConf, CFG_2DNR_STRENGTH_Y, 0);
        pContext->mConfigPara.m2DnrPara.filter_strength_uv = GetConfParaInt(&mConf, CFG_2DNR_STRENGTH_C, 0);
        pContext->mConfigPara.m2DnrPara.filter_th_y = GetConfParaInt(&mConf, CFG_2DNR_THRESHOLD_Y, 0);
        pContext->mConfigPara.m2DnrPara.filter_th_uv = GetConfParaInt(&mConf, CFG_2DNR_THRESHOLD_C, 0);

        pContext->mConfigPara.m3DnrPara.enable_3d_filter = GetConfParaInt(&mConf, CFG_3DNR_EN, 0);
        pContext->mConfigPara.m3DnrPara.adjust_pix_level_enable = GetConfParaInt(&mConf, CFG_3DNR_PIX_LEVEL_EN, 0);
        pContext->mConfigPara.m3DnrPara.smooth_filter_enable = GetConfParaInt(&mConf, CFG_3DNR_SMOOTH_EN, 0);
        pContext->mConfigPara.m3DnrPara.max_pix_diff_th = GetConfParaInt(&mConf, CFG_3DNR_PIX_DIFF_TH, 0);
        pContext->mConfigPara.m3DnrPara.max_mv_th = GetConfParaInt(&mConf, CFG_3DNR_MAX_MV_TH, 0);
        pContext->mConfigPara.m3DnrPara.max_mad_th = GetConfParaInt(&mConf, CFG_3DNR_MAX_MAD_TH, 0);
        pContext->mConfigPara.m3DnrPara.min_coef = GetConfParaInt(&mConf, CFG_3DNR_MIN_COEF, 0);
        pContext->mConfigPara.m3DnrPara.max_coef = GetConfParaInt(&mConf, CFG_3DNR_MAX_COEF, 0);

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

        alogd("vipp:%d, SrcFrameRate:%d, VideoFrameRate:%d, bitrate:%d, video_duration=%d, test_time=%d, profile=%d", pContext->mConfigPara.mVippDev,\
            pContext->mConfigPara.mSrcFrameRate, pContext->mConfigPara.mVideoFrameRate, pContext->mConfigPara.mVideoBitRate,\
            pContext->mConfigPara.mMaxFileDuration, pContext->mConfigPara.mTestDuration,\
            pContext->mConfigPara.mEncUseProfile);

        pContext->mConfigPara.mHorizonFlipFlag = GetConfParaInt(&mConf, CFG_MIRROR, 0);

        ptr = (char *)GetConfParaString(&mConf, CFG_COLOR2GREY, NULL);
        if (ptr != NULL)
        {
            if(!strcmp(ptr, "yes"))
            {
                pContext->mConfigPara.mColor2Grey = TRUE;
            }
            else
            {
                pContext->mConfigPara.mColor2Grey = FALSE;
            }
        }

        pContext->mConfigPara.mRoiNum = GetConfParaInt(&mConf, CFG_ROI_NUM, 0);
        pContext->mConfigPara.mRoiQp = GetConfParaInt(&mConf, CFG_ROI_QP, 0);
        pContext->mConfigPara.mRoiBgFrameRateEnable = GetConfParaBoolean(&mConf, CFG_ROI_BgFrameRateEnable, 0);
        pContext->mConfigPara.mRoiBgFrameRateAttenuation = GetConfParaInt(&mConf, CFG_ROI_BgFrameRateAttenuation, 0);
        pContext->mConfigPara.mIntraRefreshBlockNum = GetConfParaInt(&mConf, CFG_IntraRefresh_BlockNum, 0);
        pContext->mConfigPara.mOrlNum = GetConfParaInt(&mConf, CFG_ORL_NUM, 0);
        pContext->mConfigPara.mVbvBufferSize = GetConfParaInt(&mConf, CFG_vbvBufferSize, 0);
        pContext->mConfigPara.mVbvThreshSize = GetConfParaInt(&mConf, CFG_vbvThreshSize, 0);

        alogd("mirror:%d, Color2Grey:%d, RoiNum:%d, RoiQp:%d, RoiBgFrameRate Enable:%d Attenuation:%d, IntraRefreshBlockNum:%d, OrlNum:%d"
            "VbvBufferSize:%d, VbvThreshSize:%d",
            pContext->mConfigPara.mHorizonFlipFlag, pContext->mConfigPara.mColor2Grey,
            pContext->mConfigPara.mRoiNum, pContext->mConfigPara.mRoiQp,
            pContext->mConfigPara.mRoiBgFrameRateEnable, pContext->mConfigPara.mRoiBgFrameRateAttenuation,
            pContext->mConfigPara.mIntraRefreshBlockNum,
            pContext->mConfigPara.mOrlNum, pContext->mConfigPara.mVbvBufferSize,
            pContext->mConfigPara.mVbvThreshSize);

        pContext->mConfigPara.mCropEnable = GetConfParaInt(&mConf, CFG_CROP_ENABLE, 0);
        pContext->mConfigPara.mCropRectX = GetConfParaInt(&mConf, CFG_CROP_RECT_X, 0);
        pContext->mConfigPara.mCropRectY = GetConfParaInt(&mConf, CFG_CROP_RECT_Y, 0);
        pContext->mConfigPara.mCropRectWidth = GetConfParaInt(&mConf, CFG_CROP_RECT_WIDTH, 0);
        pContext->mConfigPara.mCropRectHeight = GetConfParaInt(&mConf, CFG_CROP_RECT_HEIGHT, 0);

        alogd("venc crop enable:%d, X:%d, Y:%d, Width:%d, Height:%d",
            pContext->mConfigPara.mCropEnable, pContext->mConfigPara.mCropRectX,
            pContext->mConfigPara.mCropRectY, pContext->mConfigPara.mCropRectWidth,
            pContext->mConfigPara.mCropRectHeight);

        pContext->mConfigPara.mVuiTimingInfoPresentFlag = GetConfParaInt(&mConf, CFG_vui_timing_info_present_flag, 0);
        alogd("VuiTimingInfoPresentFlag:%d", pContext->mConfigPara.mVuiTimingInfoPresentFlag);

        //pContext->mConfigPara.mVeFreq = GetConfParaInt(&mConf, CFG_Ve_Freq, 0);
        //alogd("mVeFreq:%d MHz", pContext->mConfigPara.mVeFreq);

        pContext->mConfigPara.mOnlineEnable = GetConfParaInt(&mConf, CFG_online_en, 0);
        pContext->mConfigPara.mOnlineShareBufNum = GetConfParaInt(&mConf, CFG_online_share_buf_num, 0);
        alogd("OnlineEnable: %d, OnlineShareBufNum: %d", pContext->mConfigPara.mOnlineEnable,
            pContext->mConfigPara.mOnlineShareBufNum);

        if (0 == pContext->mConfigPara.mOnlineEnable)
        {
            // venc drop frame only support offline.
            pContext->mConfigPara.mViDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("ViDropFrameNum: %d", pContext->mConfigPara.mViDropFrameNum);
        }
        else
        {
            // venc drop frame support online and offline.
            pContext->mConfigPara.mVencDropFrameNum = GetConfParaInt(&mConf, CFG_DROP_FRAME_NUM, 0);
            alogd("VencDropFrameNum: %d", pContext->mConfigPara.mVencDropFrameNum);
        }

        pContext->mConfigPara.wdr_en = GetConfParaInt(&mConf, CFG_WDR_EN, 0);
        alogd("wdr_en: %d", pContext->mConfigPara.wdr_en);

        pContext->mConfigPara.mEnableGdc = GetConfParaInt(&mConf, CFG_EnableGdc, 0);
        pContext->mConfigPara.mGdcWarpMode = parserGdcWarpMode((char *)GetConfParaString(&mConf, CFG_GDC_WARP_MODE, NULL));
        if (Gdc_Warp_LDC_Pro == pContext->mConfigPara.mGdcWarpMode)
        {
            ptr = (char *)GetConfParaString(&mConf, CFG_GDC_LDC_Pro_Lut_Bin, NULL);
            strcpy(pContext->mConfigPara.mGdcLdcProLutBin, ptr);
        }
        alogd("EnableGdc: %d warp mode: %d gdc ldc pro lut bin[%s]", \
            pContext->mConfigPara.mEnableGdc, pContext->mConfigPara.mGdcWarpMode, pContext->mConfigPara.mGdcLdcProLutBin);

        pContext->mConfigPara.mEncppEnable = GetConfParaInt(&mConf, CFG_EncppEnable, 0);
        alogd("EncppEnable: %d", pContext->mConfigPara.mEncppEnable);
        pContext->mConfigPara.mIspAndVeLinkageEnable = GetConfParaInt(&mConf, CFG_IspAndVeLinkageEnable, 0);
        pContext->mConfigPara.mCameraAdaptiveMovingAndStaticEnable = GetConfParaInt(&mConf, CFG_CameraAdaptiveMovingAndStaticEnable, 0);
        pContext->mConfigPara.mVencLensMovingMaxQp = GetConfParaInt(&mConf, CFG_VencLensMovingMaxQp, 0);
        alogd("IspAndVeLinkageEn:%d, AdaptEn:%d, LensMoveMaxQp:%d", pContext->mConfigPara.mIspAndVeLinkageEnable,
            pContext->mConfigPara.mCameraAdaptiveMovingAndStaticEnable, pContext->mConfigPara.mVencLensMovingMaxQp);

        pContext->mConfigPara.eSeiEnable = (VencSeiEnableSettingE)GetConfParaInt(&mConf, CFG_SeiEnable, 0);
        pContext->mConfigPara.bSeiDataIsp = (BOOL)GetConfParaInt(&mConf, CFG_SeiDataIsp, 0);
        pContext->mConfigPara.bSeiDataVipp = (BOOL)GetConfParaInt(&mConf, CFG_SeiDataVipp, 0);
        pContext->mConfigPara.bSeiDataVenc = (BOOL)GetConfParaInt(&mConf, CFG_SeiDataVenc, 0);
        pContext->mConfigPara.nSeiFrameIntervalIspLevel1 = GetConfParaInt(&mConf, CFG_SeiFrameIntervalIspLevel1, 0);
        pContext->mConfigPara.nSeiFrameIntervalIspLevel2 = GetConfParaInt(&mConf, CFG_SeiFrameIntervalIspLevel2, 0);
        pContext->mConfigPara.nSeiFrameIntervalIspLevel3 = GetConfParaInt(&mConf, CFG_SeiFrameIntervalIspLevel3, 0);
        pContext->mConfigPara.nSeiFrameIntervalVipp = GetConfParaInt(&mConf, CFG_SeiFrameIntervalVipp, 0);
        pContext->mConfigPara.nSeiFrameIntervalVencLevel1 = GetConfParaInt(&mConf, CFG_SeiFrameIntervalVencLevel1, 0);
        pContext->mConfigPara.nSeiFrameIntervalVencLevel2 = GetConfParaInt(&mConf, CFG_SeiFrameIntervalVencLevel2, 0);

        pContext->mConfigPara.mSuperFrmMode = GetConfParaInt(&mConf, CFG_SuperFrmMode, 0);
        pContext->mConfigPara.mSuperMaxRencodeTimes = GetConfParaInt(&mConf, CFG_SuperMaxRencodeTimes, 0);
        pContext->mConfigPara.mSuperMaxP2IFrameBitsRatio = (float)GetConfParaDouble(&mConf, CFG_SuperMaxP2IFrameBitsRatio, 0);
        pContext->mConfigPara.mSuperIFrmBitsThr = GetConfParaInt(&mConf, CFG_SuperIFrmBitsThr, 0);
        pContext->mConfigPara.mSuperPFrmBitsThr = GetConfParaInt(&mConf, CFG_SuperPFrmBitsThr, 0);
        alogd("SuperFrm Mode: %d, MaxRencodeTimes: %d, MaxP2IFrameBitsRatio: %.2f, IBitsThr: %d, PBitsThr: %d", pContext->mConfigPara.mSuperFrmMode,
            pContext->mConfigPara.mSuperMaxRencodeTimes, pContext->mConfigPara.mSuperMaxP2IFrameBitsRatio,
            pContext->mConfigPara.mSuperIFrmBitsThr, pContext->mConfigPara.mSuperPFrmBitsThr);

        pContext->mConfigPara.mBitsClipParam.dis_default_para = GetConfParaBoolean(&mConf, CFG_BitsClipDisDefault, 0);
        pContext->mConfigPara.mBitsClipParam.mode = GetConfParaInt(&mConf, CFG_BitsClipMode, 0);
        pContext->mConfigPara.mBitsClipParam.en_gop_clip = GetConfParaInt(&mConf, CFG_BitsClipEnableGopClip, 0);
        pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipGopBitRatioTh0, 0);
        pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipGopBitRatioTh1, 1);
        pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[2] = (float)GetConfParaDouble(&mConf, CFG_BitsClipGopBitRatioTh2, 2);
        pContext->mConfigPara.mBitsClipParam.coef_th[0][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef00, -0.5);
        pContext->mConfigPara.mBitsClipParam.coef_th[0][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef01, 0.2);
        pContext->mConfigPara.mBitsClipParam.coef_th[1][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef10, -0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[1][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef11, 0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[2][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef20, -0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[2][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef21, 0.3);
        pContext->mConfigPara.mBitsClipParam.coef_th[3][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef30, -0.5);
        pContext->mConfigPara.mBitsClipParam.coef_th[3][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef31, 0.5);
        pContext->mConfigPara.mBitsClipParam.coef_th[4][0] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef40, 0.4);
        pContext->mConfigPara.mBitsClipParam.coef_th[4][1] = (float)GetConfParaDouble(&mConf, CFG_BitsClipCoef41, 0.7);

        alogd("BitsClipParam: %d %d %d {%.2f,%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}",
            pContext->mConfigPara.mBitsClipParam.dis_default_para,
            pContext->mConfigPara.mBitsClipParam.mode,
            pContext->mConfigPara.mBitsClipParam.en_gop_clip,
            pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[0],
            pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[1],
            pContext->mConfigPara.mBitsClipParam.gop_bit_ratio_th[2],
            pContext->mConfigPara.mBitsClipParam.coef_th[0][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[0][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[1][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[1][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[2][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[2][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[3][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[3][1],
            pContext->mConfigPara.mBitsClipParam.coef_th[4][0],
            pContext->mConfigPara.mBitsClipParam.coef_th[4][1]);

        pContext->mConfigPara.EnIFrmMbRcMoveStatusEnable = GetConfParaInt(&mConf, CFG_EnIFrmMbRcMoveStatusEnable, 0);
        pContext->mConfigPara.EnIFrmMbRcMoveStatus = GetConfParaInt(&mConf, CFG_EnIFrmMbRcMoveStatus, 3);
        alogd("EnIFrmMbRcMoveStatus: en %d, %d", pContext->mConfigPara.EnIFrmMbRcMoveStatusEnable, pContext->mConfigPara.EnIFrmMbRcMoveStatus);

        pContext->mConfigPara.mBitsRatioEnable = GetConfParaInt(&mConf, CFG_IPTargetBitsRatioEnable, 0);
        pContext->mConfigPara.mBitsRatio.nSceneCoef[0] = (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioSceneCoef0, 20);
        pContext->mConfigPara.mBitsRatio.nSceneCoef[1] = (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioSceneCoef1, 17);
        pContext->mConfigPara.mBitsRatio.nSceneCoef[2] = (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioSceneCoef2, 15);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[0] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef0, 1);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[1] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef1, 0.75);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[2] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef2, 0.5);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[3] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef3, 0.25);
        pContext->mConfigPara.mBitsRatio.nMoveCoef[4] =  (float)GetConfParaDouble(&mConf, CFG_IPTargetBitsRatioMoveCoef4, 0.25);

        alogd("BitsRatio: en %d, SceneCoef[%.2f,%.2f,%.2f] MoveCoef[%.2f,%.2f,%.2f,%.2f,%.2f]",
            pContext->mConfigPara.mBitsRatioEnable,
            pContext->mConfigPara.mBitsRatio.nSceneCoef[0],
            pContext->mConfigPara.mBitsRatio.nSceneCoef[1],
            pContext->mConfigPara.mBitsRatio.nSceneCoef[2],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[0],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[1],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[2],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[3],
            pContext->mConfigPara.mBitsRatio.nMoveCoef[4]);

        pContext->mConfigPara.mWeakTextureThEnable = GetConfParaInt(&mConf, CFG_WeakTextureThEnable, 0);
        pContext->mConfigPara.mWeakTextureTh =  (float)GetConfParaDouble(&mConf, CFG_WeakTextureTh, 0);
        alogd("WeakTextureTh: %.2f", pContext->mConfigPara.mWeakTextureTh);

        pContext->mConfigPara.mChromaQPOffsetEnable = GetConfParaInt(&mConf, CFG_ChromaQPOffsetEnable, 0);
        pContext->mConfigPara.mChromaQPOffset = GetConfParaInt(&mConf, CFG_ChromaQPOffset, 0);

        pContext->mConfigPara.mH264ConstraintFlagEnable = GetConfParaInt(&mConf, CFG_H264ConstraintFlagEnable, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_0 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit0, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_1 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit1, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_2 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit2, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_3 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit3, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_4 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit4, 0);
        pContext->mConfigPara.mH264ConstraintFlag.constraint_5 = GetConfParaInt(&mConf, CFG_H264ConstraintFlagBit5, 0);

        pContext->mConfigPara.mVe2IspD2DLimit.en_d2d_limit = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitEnable, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[0] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel0, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[1] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel1, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[2] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel2, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[3] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel3, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[4] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel4, 0);
        pContext->mConfigPara.mVe2IspD2DLimit.d2d_level[5] = GetConfParaInt(&mConf, CFG_Ve2IspD2DLimitD2DLevel5, 0);

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
        pContext->mConfigPara.mVeRecRefBufReduceEnable = GetConfParaInt(&mConf, CFG_VeRecRefBufReduceEnable, 0);

        destroyConfParser(&mConf);
    }

    //parse dst directory form dst file path.
    char *pLastSlash = strrchr(pContext->mConfigPara.dstVideoFile, '/');
    if(pLastSlash != NULL)
    {
        int dirLen = pLastSlash-pContext->mConfigPara.dstVideoFile;
        strncpy(pContext->mDstDir, pContext->mConfigPara.dstVideoFile, dirLen);
        pContext->mDstDir[dirLen] = '\0';
        
        char *pFileName = pLastSlash+1;
        strcpy(pContext->mFirstFileName, pFileName);
    }
    else
    {
        strcpy(pContext->mDstDir, "");
        strcpy(pContext->mFirstFileName, pContext->mConfigPara.dstVideoFile);
    }

    //get file format from suffix of file name.
    char *pLastDot = strrchr(pContext->mConfigPara.dstVideoFile, '.');
    if(pLastDot != NULL)
    {
        if(!strcmp(".mp4", pLastDot))
        {
            pContext->eFileFormat = MEDIA_FILE_FORMAT_MP4;
        }
        else if(!strcmp(".ts", pLastDot))
        {
            pContext->eFileFormat = MEDIA_FILE_FORMAT_TS;
        }
        else
        {
            alogd("unknown file suffix:[%s], use raw file format", pLastDot);
            pContext->eFileFormat = MEDIA_FILE_FORMAT_RAW;
        }
    }
    else
    {
        pContext->eFileFormat = MEDIA_FILE_FORMAT_RAW;
    }
    return SUCCESS;
}

static unsigned long long GetNowTimeUs(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

static int getFileNameByCurTime(SAMPLE_VI2VENC2MUXER_S *pContext, char *pNameBuf)
{
#if 0
    sprintf(pNameBuf, "%s", "/mnt/extsd/sample_mux/");
    sprintf(pNameBuf, "%s%llud.mp4", pNameBuf, GetNowTimeUs());
#else
    static int file_cnt = 0;
    char strStemPath[MAX_FILE_PATH_LEN] = {0};
    int len = strlen(pContext->mConfigPara.dstVideoFile);
    char *ptr = pContext->mConfigPara.dstVideoFile;
    while (*(ptr+len-1) != '.')
    {
        len--;
    }

    ++file_cnt;
    strncpy(strStemPath, pContext->mConfigPara.dstVideoFile, len-1);
    sprintf(pNameBuf, "%s_%d.%s", strStemPath, file_cnt, (MEDIA_FILE_FORMAT_TS==pContext->eFileFormat)?"ts":"mp4");
#endif
    return 0;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_VI2VENC2MUXER_S *pContext = (SAMPLE_VI2VENC2MUXER_S *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                aloge("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Vi2Venc2Muxer_MessageData stMsgData;
                stMsgData.mpVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S*)cookie;
                stCmdMsg.command = Vi_Timeout;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&pContext->mMsgQueue, &stCmdMsg);
                break;
            }
	    default:
		aloge("fatal error! unknow event type[0x%x]", event);
		break;
        }
    }
    else if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;
        switch(event)
        {
            /*case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                Isp2VeLinkageParam stIsp2Ve;
                memset(&stIsp2Ve, 0, sizeof(Isp2VeLinkageParam));
                stIsp2Ve.mIspAndVeLinkageEnable = pContext->mConfigPara.mIspAndVeLinkageEnable;
                stIsp2Ve.mCameraAdaptiveMovingAndStaticEnable = pContext->mConfigPara.mCameraAdaptiveMovingAndStaticEnable;
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
                if (pContext->mConfigPara.mCameraAdaptiveMovingAndStaticEnable)
                {
                    pExtraParam->eEnCameraMove = CAMERA_ADAPTIVE_MOVING_AND_STATIC;
                }
                else
                {
                    /**
                    Example code:
                    static int lens_start_moving_flag = 1; // obtain motor motion status from motor driver.
                    if (lens_start_moving_flag)
                    {
                        pIsp2VeParam->mEnCameraMove = CAMERA_FORCE_MOVING;
                        lens_start_moving_flag = 0;
                    }
                    else
                    {
                        pIsp2VeParam->mEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                    }
                    */
                    pExtraParam->eEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                }
                break;
            }
            case MPP_EVENT_VENC_BUFFER_FULL:
            {
                alogw("vencChn[%d] vbv buffer full", pChn->mChnId);
                break;
            }
            case MPP_EVENT_DROP_FRAME:
            {
                alogd("vencChn[%d] receive dropFrame message", pChn->mChnId);
                break;
            }
            default:
            {
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
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Vi2Venc2Muxer_MessageData stMsgData;
                alogd("MuxChn[%d] record file done.", *(int*)pEventData);
                stMsgData.mpVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S*)cookie;
                stCmdMsg.command = Rec_FileDone;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&gpVi2Venc2MuxerData->mMsgQueue, &stCmdMsg);  
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                Vi2Venc2Muxer_MessageData stMsgData;
                alogd("MuxChn[%d] need next fd.", *(int*)pEventData);
                stMsgData.mpVi2Venc2MuxerData = (SAMPLE_VI2VENC2MUXER_S*)cookie;
                stCmdMsg.command = Rec_NeedSetNextFd;
                stCmdMsg.para0 = *(int*)pEventData;
                stCmdMsg.mDataSize = sizeof(Vi2Venc2Muxer_MessageData);
                stCmdMsg.mpData = &stMsgData;
                putMessageWithData(&gpVi2Venc2MuxerData->mMsgQueue, &stCmdMsg);  
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
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

static ERRORTYPE configMuxChnAttr(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    MUX_CHN_INFO_S *pOutSinkEntry;
    memset(&pContext->mMuxChnAttr, 0, sizeof(MUX_CHN_ATTR_S));

    pContext->mMuxChnAttr.mVideoAttrValidNum = 1;
    pContext->mMuxChnAttr.mVideoAttr[0].mVideoEncodeType = pContext->mConfigPara.mVideoEncoderFmt;
    pContext->mMuxChnAttr.mVideoAttr[0].mWidth = pContext->mConfigPara.dstWidth;
    pContext->mMuxChnAttr.mVideoAttr[0].mHeight = pContext->mConfigPara.dstHeight;
    pContext->mMuxChnAttr.mVideoAttr[0].mVideoFrmRate = pContext->mConfigPara.mVideoFrameRate*1000;
    pContext->mMuxChnAttr.mVideoAttr[0].mVeChn = pContext->mVeChn;
    pContext->mMuxChnAttr.mAudioEncodeType = PT_MAX;
    pContext->mMuxChnAttr.mTextEncodeType = PT_MAX;

    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        pOutSinkEntry = list_first_entry(&pContext->mMuxChnList, MUX_CHN_INFO_S, mList);
        //pContext->mMuxChnAttr.mMuxerId = pOutSinkEntry->mSinkInfo.mMuxerId;
        pContext->mMuxChnAttr.mMediaFileFormat = pOutSinkEntry->mSinkInfo.mOutputFormat;
        pContext->mMuxChnAttr.mMaxFileDuration = pContext->mConfigPara.mMaxFileDuration *1000;
        pContext->mMuxChnAttr.mCallbackOutFlag = pOutSinkEntry->mSinkInfo.mCallbackOutFlag;
        pContext->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
        pContext->mMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
        pContext->mMuxChnAttr.mAddRepairInfo = pContext->mConfigPara.mbAddRepairInfo;
        pContext->mMuxChnAttr.mMaxFrmsTagInterval = pContext->mConfigPara.mMaxFrmsTagInterval;
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    return SUCCESS;
}

static ERRORTYPE createMuxChn(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;
    MUX_CHN_INFO_S *pOutSinkEntry = NULL;

    configMuxChnAttr(pContext);
    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        pOutSinkEntry = list_first_entry(&pContext->mMuxChnList, MUX_CHN_INFO_S, mList);
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);
    pContext->mMuxChn = 0;
    while (pContext->mMuxChn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(pContext->mMuxChn, &pContext->mMuxChnAttr, pOutSinkEntry->mSinkInfo.mOutputFd, pOutSinkEntry->mSinkInfo.mFallocateLen);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create muxChn[%d] success!", pContext->mMuxChn);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("muxChn[%d] is exist, find next!", pContext->mMuxChn);
            pContext->mMuxChn++;
        }
        else
        {
            alogd("create muxChn[%d] ret[0x%x], find next!", pContext->mMuxChn, ret);
            pContext->mMuxChn++;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pContext->mMuxChn = MM_INVALID_CHN;
        aloge("fatal error! create mux channel fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pContext->mMuxChn, &cbInfo);
        return SUCCESS;
    }
}

static int addOutputFormatAndOutputSink_l(SAMPLE_VI2VENC2MUXER_S *pContext, OUTSINKINFO_S *pSinkInfo)
{
    int ret = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;

    alogd("fmt:0x%x, fd:%d, FallocateLen:%d, callback_out_flag:%d", pSinkInfo->mOutputFormat, pSinkInfo->mOutputFd, pSinkInfo->mFallocateLen, pSinkInfo->mCallbackOutFlag);
    if(pSinkInfo->mOutputFd >= 0 && TRUE == pSinkInfo->mCallbackOutFlag)
    {
        aloge("fatal error! one muxer cannot support two sink methods!");
        return -1;
    }

    //find if the same output_format sinkInfo exist or callback out stream is exist.
    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFormat == pSinkInfo->mOutputFormat)
            {
                alogd("Be careful! same outputForamt[0x%x] exist in array", pSinkInfo->mOutputFormat);
            }
//            if (pEntry->mSinkInfo.mCallbackOutFlag == pSinkInfo->mCallbackOutFlag)
//            {
//                aloge("fatal error! only support one callback out stream");
//            }
        }
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    MUX_CHN_INFO_S *p_node = (MUX_CHN_INFO_S *)malloc(sizeof(MUX_CHN_INFO_S));
    if (p_node == NULL)
    {
        aloge("alloc mux chn info node fail");
        return -1;
    }

    memset(p_node, 0, sizeof(MUX_CHN_INFO_S));
    //p_node->mSinkInfo.mMuxerId = pContext->mMuxerIdCounter;
    p_node->mSinkInfo.mOutputFormat = pSinkInfo->mOutputFormat;
    if (pSinkInfo->mOutputFd > 0)
    {
        p_node->mSinkInfo.mOutputFd = dup(pSinkInfo->mOutputFd);
    }
    else
    {
        p_node->mSinkInfo.mOutputFd = -1;
    }
    p_node->mSinkInfo.mFallocateLen = pSinkInfo->mFallocateLen;
    p_node->mSinkInfo.mCallbackOutFlag = pSinkInfo->mCallbackOutFlag;

//    p_node->mMuxChnAttr.mMuxerId = p_node->mSinkInfo.mMuxerId;
//    p_node->mMuxChnAttr.mMediaFileFormat = p_node->mSinkInfo.mOutputFormat;
//    p_node->mMuxChnAttr.mMaxFileDuration = pContext->mConfigPara.mMaxFileDuration *1000;
//    p_node->mMuxChnAttr.mFallocateLen = p_node->mSinkInfo.mFallocateLen;
//    p_node->mMuxChnAttr.mCallbackOutFlag = p_node->mSinkInfo.mCallbackOutFlag;
//    p_node->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
//    p_node->mMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
//    p_node->mMuxChnAttr.mAddRepairInfo = pContext->mConfigPara.mbAddRepairInfo;
//    p_node->mMuxChnAttr.mMaxFrmsTagInterval = pContext->mConfigPara.mMaxFrmsTagInterval;

//    p_node->mMuxChn = MM_INVALID_CHN;

    if ((pContext->mCurrentState == REC_PREPARED) || (pContext->mCurrentState == REC_RECORDING))
    {
        aloge("fatal error! mpi_mux tunnel-mode must create muxChn in init state.");
        #if 0
        ERRORTYPE ret;
        BOOL nSuccessFlag = FALSE;
        MUX_CHN nMuxChn = 0;
        while (nMuxChn < MUX_MAX_CHN_NUM)
        {
            ret = AW_MPI_MUX_CreateChn(pContext->mMuxGrp, nMuxChn, &p_node->mMuxChnAttr, p_node->mSinkInfo.mOutputFd);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", pContext->mMuxGrp, nMuxChn, p_node->mMuxChnAttr.mMuxerId);
                break;
            }
            else if (ERR_MUX_EXIST == ret)
            {
                alogd("mux group[%d] channel[%d] is exist, find next!", pContext->mMuxGrp, nMuxChn);
                nMuxChn++;
            }
            else
            {
                aloge("fatal error! create mux group[%d] channel[%d] fail ret[0x%x], find next!", pContext->mMuxGrp, nMuxChn, ret);
                nMuxChn++;
            }
        }

        if (nSuccessFlag)
        {
            retMuxerId = p_node->mSinkInfo.mMuxerId;
            p_node->mMuxChn = nMuxChn;
            pContext->mMuxerIdCounter++;
        }
        else
        {
            aloge("fatal error! create mux group[%d] channel fail!", pContext->mMuxGrp);
            if (p_node->mSinkInfo.mOutputFd >= 0)
            {
                close(p_node->mSinkInfo.mOutputFd);
                p_node->mSinkInfo.mOutputFd = -1;
            }

            retMuxerId = -1;
        }

        pthread_mutex_lock(&pContext->mMuxChnListLock);
        list_add_tail(&p_node->mList, &pContext->mMuxChnList);
        pthread_mutex_unlock(&pContext->mMuxChnListLock);
        #endif
    }
    else
    {
        //retMuxerId = p_node->mSinkInfo.mMuxerId;
        //pContext->mMuxerIdCounter++;
        pthread_mutex_lock(&pContext->mMuxChnListLock);
        list_add_tail(&p_node->mList, &pContext->mMuxChnList);
        pthread_mutex_unlock(&pContext->mMuxChnListLock);
        ret = 0;
    }

    return ret;
}

static int addOutputFormatAndOutputSink(SAMPLE_VI2VENC2MUXER_S *pContext, char* path, MEDIA_FILE_FORMAT_E format)
{
    int ret = -1;
    OUTSINKINFO_S sinkInfo = {0};

    if (path != NULL)
    {
        sinkInfo.mFallocateLen = 0;
        sinkInfo.mCallbackOutFlag = FALSE;
        sinkInfo.mOutputFormat = format;
        sinkInfo.mOutputFd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (sinkInfo.mOutputFd < 0)
        {
            aloge("Failed to open %s", path);
            return -1;
        }

        ret = addOutputFormatAndOutputSink_l(pContext, &sinkInfo);
        close(sinkInfo.mOutputFd);
    }

    return ret;
}

static int setOutputFileSync_l(SAMPLE_VI2VENC2MUXER_S *pContext, int fd, int64_t fallocateLength, int muxChn)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;

    if (pContext->mCurrentState != REC_RECORDING)
    {
        aloge("must be in recording state");
        return -1;
    }

    alogv("setOutputFileSync fd=%d", fd);
    if (fd < 0)
    {
        aloge("Invalid parameter");
        return -1;
    }

    if(pContext->mMuxChn != muxChn)
    {
        aloge("fatal error! why muxChn is not match?[%d!=%d]", pContext->mMuxChn, muxChn);
    }

    if (muxChn != MM_INVALID_CHN)
    {
        alogd("switch fd");
        AW_MPI_MUX_SwitchFd(pContext->mMuxChn, fd, fallocateLength);
        return 0;
    }
    else
    {
        aloge("fatal error! can't find muxChn[%d]", muxChn);
        return -1;
    }
}

static int setOutputFileSync(SAMPLE_VI2VENC2MUXER_S *pContext, char* path, int64_t fallocateLength, int muxChn)
{
    int ret;

    if (pContext->mCurrentState != REC_RECORDING)
    {
        aloge("not in recording state");
        return -1;
    }

    if(path != NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
        {
            aloge("fail to open %s", path);
            return -1;
        }
        ret = setOutputFileSync_l(pContext, fd, fallocateLength, muxChn);
        close(fd);

        return ret;
    }
    else
    {
        return -1;
    }
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

/* GC4663 2560x1440 parameter */
static void configLdcParam(SAMPLE_VI2VENC2MUXER_S *pContext, sGdcParam *pGdcParam)
{
    pGdcParam->bGDC_en = 1;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
    pGdcParam->bMirror = 0;
    pGdcParam->calib_widht = 2560;
    pGdcParam->calib_height = 1440;

    pGdcParam->eWarpMode = Gdc_Warp_LDC;

    pGdcParam->fx = 1706.57f;
    pGdcParam->fy = 1713.46f;
    pGdcParam->cx = 1279.50f;
    pGdcParam->cy = 719.50f;
    pGdcParam->fx_scale = 1595.14f;
    pGdcParam->fy_scale = 1601.57f;
    pGdcParam->cx_scale = 1279.50f;
    pGdcParam->cy_scale = 719.50f;

    pGdcParam->distCoef_wide_ra[0] = -0.428043f;
    pGdcParam->distCoef_wide_ra[1] = 0.227647f;
    pGdcParam->distCoef_wide_ra[2] = 0.000000f;
    pGdcParam->distCoef_wide_ta[0] = 0.009734f;
    pGdcParam->distCoef_wide_ta[1] = -0.001312f;

    pGdcParam->distCoef_fish_k[0] = 0.00;
    pGdcParam->distCoef_fish_k[1] = 0.00;
    pGdcParam->distCoef_fish_k[2] = 0.00;
    pGdcParam->distCoef_fish_k[3] = 0.00;

    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->centerOffsetX = 0;
    pGdcParam->centerOffsetY = 0;
    pGdcParam->rotateAngle = 0;
    pGdcParam->radialDistortCoef = 0;
    pGdcParam->trapezoidDistortCoef = 0;

    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;
}

static void configLdcProParam(SAMPLE_VI2VENC2MUXER_S *pContext, sGdcParam *pGdcParam)
{
    pGdcParam->bGDC_en = 1;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
    pGdcParam->bMirror = 0;
    pGdcParam->calib_widht = pContext->mConfigPara.dstWidth;
    pGdcParam->calib_height = pContext->mConfigPara.dstHeight;
    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpMode;
    pGdcParam->lut_data_buf = (unsigned int *)pContext->mConfigPara.mpGdcLdcProLutBinData;
    pGdcParam->lut_data_size = pContext->mConfigPara.mGdcLdcProLutBinDataLen;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;
}

static void configGdcParam(SAMPLE_VI2VENC2MUXER_S *pContext, sGdcParam *pGdcParam)
{
    switch (pGdcParam->eWarpMode)
    {
        case Gdc_Warp_LDC:
            configLdcParam(pContext, pGdcParam);
            break;
        case Gdc_Warp_LDC_Pro:
            configLdcProParam(pContext, pGdcParam);
            break;
        default:
            aloge("unsupport warp mode[%d]", pGdcParam->eWarpMode);
            break;
    }
}

static ERRORTYPE configVencChnAttr(SAMPLE_VI2VENC2MUXER_S *pContext)
{
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
    pContext->mVencChnAttr.VeAttr.mVeRefFrameLbcMode = pContext->mConfigPara.mVeRefFrameLbcMode;
    alogd("VeRefFrameLbcMode:%d", pContext->mVencChnAttr.VeAttr.mVeRefFrameLbcMode);
    pContext->mVencChnAttr.VeAttr.mVeRecRefBufReduceEnable = pContext->mConfigPara.mVeRecRefBufReduceEnable;
    alogd("VeRecRefBufReduceEnable:%d", pContext->mVencChnAttr.VeAttr.mVeRecRefBufReduceEnable);
    pContext->mVencChnAttr.VeAttr.mVbrOptEnable = pContext->mConfigPara.mVbrOptEnable;
    alogd("VbrOptEnable:%d", pContext->mVencChnAttr.VeAttr.mVbrOptEnable);
    pContext->mVencChnAttr.EncppAttr.eEncppSharpSetting = pContext->mConfigPara.mEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;
    switch(pContext->mConfigPara.mEncodeRotate)
    {
        case 90:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_90;
            break;
        case 180:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_180;
            break;
        case 270:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_270;
            break;
        default:
            pContext->mVencChnAttr.VeAttr.Rotate = ROTATE_NONE;
            break;
    }

    pContext->mVencChnAttr.RcAttr.mProductMode = pContext->mConfigPara.mProductMode;
    //pContext->mVencRcParam.sensor_type = pContext->mConfigPara.mSensorType;

    if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH264e.BufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH264e.Profile = map_H264_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH264e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mVencRcParam.EnIFrmMbRcMoveStatusEnable = pContext->mConfigPara.EnIFrmMbRcMoveStatusEnable;
        pContext->mVencRcParam.EnIFrmMbRcMoveStatus = pContext->mConfigPara.EnIFrmMbRcMoveStatus;
        pContext->mVencRcParam.mBitsRatioEnable = pContext->mConfigPara.mBitsRatioEnable;
        memcpy(&pContext->mVencRcParam.mBitsRatio, &pContext->mConfigPara.mBitsRatio, sizeof(VencIPTargetBitsRatio));
        pContext->mVencRcParam.mWeakTextureThEnable = pContext->mConfigPara.mWeakTextureThEnable;
        pContext->mVencRcParam.mWeakTextureTh = pContext->mConfigPara.mWeakTextureTh;
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
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
            pContext->mVencChnAttr.RcAttr.mAttrH264FixQp.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
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
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH264Cbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH264Cbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH264Cbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH264Cbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH264Cbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            break;
        }
        if (pContext->mConfigPara.mEnableFastEnc)
        {
            pContext->mVencChnAttr.VeAttr.AttrH264e.FastEncFlag = TRUE;
        }
    }
    else if (PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH265e.mBufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mProfile = map_H265_UserSet2Profile(pContext->mConfigPara.mEncUseProfile);
        pContext->mVencChnAttr.VeAttr.AttrH265e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.dstHeight;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mVencRcParam.EnIFrmMbRcMoveStatusEnable = pContext->mConfigPara.EnIFrmMbRcMoveStatusEnable;
        pContext->mVencRcParam.EnIFrmMbRcMoveStatus = pContext->mConfigPara.EnIFrmMbRcMoveStatus;
        pContext->mVencRcParam.mBitsRatioEnable = pContext->mConfigPara.mBitsRatioEnable;
        memcpy(&pContext->mVencRcParam.mBitsRatio, &pContext->mConfigPara.mBitsRatio, sizeof(VencIPTargetBitsRatio));
        switch (pContext->mConfigPara.mRcMode)
        {
        case 1:
            pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mVideoBitRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
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
            pContext->mVencChnAttr.RcAttr.mAttrH265FixQp.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
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
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
            pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
            pContext->mVencRcParam.ParamH265Cbr.mMaxQp = pContext->mConfigPara.mMaxIQp;
            pContext->mVencRcParam.ParamH265Cbr.mMinQp = pContext->mConfigPara.mMinIQp;
            pContext->mVencRcParam.ParamH265Cbr.mMaxPqp = pContext->mConfigPara.mMaxPQp;
            pContext->mVencRcParam.ParamH265Cbr.mMinPqp = pContext->mConfigPara.mMinPQp;
            pContext->mVencRcParam.ParamH265Cbr.mQpInit = pContext->mConfigPara.mInitQp;
            pContext->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = pContext->mConfigPara.mEnMbQpLimit;
            break;
        }
        if (pContext->mConfigPara.mEnableFastEnc)
        {
            pContext->mVencChnAttr.VeAttr.AttrH265e.mFastEncFlag = TRUE;
        }
    }
    else if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mBufSize = pContext->mConfigPara.mVbvBufferSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = pContext->mConfigPara.mVbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->mConfigPara.dstWidth;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.dstHeight;
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

    if(0 == pContext->mConfigPara.mGopMode)
    {
        pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    }
    else if(1 == pContext->mConfigPara.mGopMode)
    {
        pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_DUALP;
    }
    else if(2 == pContext->mConfigPara.mGopMode)
    {
        pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_SMARTP;
        pContext->mVencChnAttr.GopAttr.stSmartP.mVirtualIFrameInterval = 15;
    }
    pContext->mVencChnAttr.GopAttr.mGopSize = pContext->mConfigPara.mGopSize;

    if (pContext->mConfigPara.mEnableGdc)
    {
        alogd("enable GDC and init GDC params");
        configGdcParam(pContext, &pContext->mVencChnAttr.GdcAttr);
    }

    memcpy(&pContext->mVencRcParam.mBitsClipParam, &pContext->mConfigPara.mBitsClipParam, sizeof(VencTargetBitsClipParam));

    return SUCCESS;
}

static ERRORTYPE createVencChn(SAMPLE_VI2VENC2MUXER_S *pContext)
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
        //if (0 < pContext->mConfigPara.mVeFreq)
        //{
        //    AW_MPI_VENC_SetVEFreq(pContext->mVeChn, pContext->mConfigPara.mVeFreq);
        //    alogd("set VE freq %d MHz", pContext->mConfigPara.mVeFreq);
        //}

        AW_MPI_VENC_SetRcParam(pContext->mVeChn, &pContext->mVencRcParam);

        /* set framerate in AW_MPI_VENC_CreateChn */
        /*VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = pContext->mConfigPara.mSrcFrameRate;
        stFrameRate.DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
        alogd("set venc framerate: src %dfps, dst %dfps", stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
        AW_MPI_VENC_SetFrameRate(pContext->mVeChn, &stFrameRate);*/

        if (pContext->mConfigPara.mAdvancedRef_Base)
        {
            VENC_PARAM_REF_S stRefParam;
            memset(&stRefParam, 0, sizeof(VENC_PARAM_REF_S));
            stRefParam.Base = pContext->mConfigPara.mAdvancedRef_Base;
            stRefParam.Enhance = pContext->mConfigPara.mAdvancedRef_Enhance;
            stRefParam.bEnablePred = pContext->mConfigPara.mAdvancedRef_RefBaseEn;
            alogd("set RefParam %d %d %d", stRefParam.Base, stRefParam.Enhance, stRefParam.bEnablePred);
            ret = AW_MPI_VENC_SetRefParam(pContext->mVeChn, &stRefParam);
            if (ret)
            {
                aloge("fatal error! VencChn[%d] set RefParam fail! ret=%d", pContext->mVeChn, ret);
                return FAILURE;
            }
        }

        //if (pContext->mConfigPara.m2DnrPara.enable_2d_filter)
        {
            AW_MPI_VENC_Set2DFilter(pContext->mVeChn, &pContext->mConfigPara.m2DnrPara);
            alogd("set 2DFilter param");
        }

        //if (pContext->mConfigPara.m3DnrPara.enable_3d_filter)
        {
            AW_MPI_VENC_Set3DFilter(pContext->mVeChn, &pContext->mConfigPara.m3DnrPara);
            alogd("set 3DFilter param");
        }

        if (pContext->mConfigPara.mColor2Grey)
        {
            VENC_COLOR2GREY_S bColor2Grey;
            memset(&bColor2Grey, 0, sizeof(VENC_COLOR2GREY_S));
            bColor2Grey.bColor2Grey = pContext->mConfigPara.mColor2Grey;
            AW_MPI_VENC_SetColor2Grey(pContext->mVeChn, &bColor2Grey);
            alogd("set Color2Grey %d", pContext->mConfigPara.mColor2Grey);
        }

        if (pContext->mConfigPara.mHorizonFlipFlag)
        {
            AW_MPI_VENC_SetHorizonFlip(pContext->mVeChn, pContext->mConfigPara.mHorizonFlipFlag);
            alogd("set HorizonFlip %d", pContext->mConfigPara.mHorizonFlipFlag);
        }

        if (pContext->mConfigPara.mCropEnable)
        {
            VENC_CROP_CFG_S stCropCfg;
            memset(&stCropCfg, 0, sizeof(VENC_CROP_CFG_S));
            stCropCfg.bEnable = pContext->mConfigPara.mCropEnable;
            stCropCfg.Rect.X = pContext->mConfigPara.mCropRectX;
            stCropCfg.Rect.Y = pContext->mConfigPara.mCropRectY;
            stCropCfg.Rect.Width = pContext->mConfigPara.mCropRectWidth;
            stCropCfg.Rect.Height = pContext->mConfigPara.mCropRectHeight;
            AW_MPI_VENC_SetCrop(pContext->mVeChn, &stCropCfg);
            alogd("set Crop %d, [%d][%d][%d][%d]", stCropCfg.bEnable, stCropCfg.Rect.X, stCropCfg.Rect.Y, stCropCfg.Rect.Width, stCropCfg.Rect.Height);
        }

        //test PIntraRefresh
        if(pContext->mConfigPara.mIntraRefreshBlockNum > 0)
        {
            VencCyclicIntraRefresh stIntraRefresh;
            memset(&stIntraRefresh, 0, sizeof(VencCyclicIntraRefresh));
            stIntraRefresh.bEnable = 1;
            stIntraRefresh.nBlockNumber = pContext->mConfigPara.mIntraRefreshBlockNum;
            ret = AW_MPI_VENC_SetIntraRefresh(pContext->mVeChn, &stIntraRefresh);
            if(ret != SUCCESS)
            {
                aloge("fatal error! set roiBgFrameRate fail[0x%x]!", ret);
            }
            else
            {
                alogd("set intra refresh:%d", stIntraRefresh.nBlockNumber);
            }
        }

        if(pContext->mConfigPara.mbEnableSmart)
        {
            VencSmartFun smartParam;
            memset(&smartParam, 0, sizeof(VencSmartFun));
            smartParam.smart_fun_en = 1;
            smartParam.img_bin_en = 1;
            smartParam.img_bin_th = 0;
            smartParam.shift_bits = 2;
            AW_MPI_VENC_SetSmartP(pContext->mVeChn, &smartParam);
        }

        if(pContext->mConfigPara.mSVCLayer > 0)
        {
            VencH264SVCSkip stSVCSkip;
            memset(&stSVCSkip, 0, sizeof(VencH264SVCSkip));
            stSVCSkip.nTemporalSVC = pContext->mConfigPara.mSVCLayer;
            AW_MPI_VENC_SetH264SVCSkip(pContext->mVeChn, &stSVCSkip);
        }

        if (pContext->mConfigPara.mVuiTimingInfoPresentFlag)
        {
            /** must be call it before AW_MPI_VENC_GetH264SpsPpsInfo(unbind) and AW_MPI_VENC_StartRecvPic. */
            if(PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H264_VUI_S H264Vui;
                memset(&H264Vui, 0, sizeof(VENC_PARAM_H264_VUI_S));
                AW_MPI_VENC_GetH264Vui(pContext->mVeChn, &H264Vui);
                H264Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H264Vui.VuiTimeInfo.fixed_frame_rate_flag = 1;
                H264Vui.VuiTimeInfo.num_units_in_tick = 1000;
                H264Vui.VuiTimeInfo.time_scale = H264Vui.VuiTimeInfo.num_units_in_tick * pContext->mConfigPara.mVideoFrameRate * 2;
                AW_MPI_VENC_SetH264Vui(pContext->mVeChn, &H264Vui);
                alogd("VencChn[%d] fill framerate %d to H264VUI", pContext->mVeChn, pContext->mConfigPara.mVideoFrameRate);
            }
            else if(PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
            {
                VENC_PARAM_H265_VUI_S H265Vui;
                memset(&H265Vui, 0, sizeof(VENC_PARAM_H265_VUI_S));
                AW_MPI_VENC_GetH265Vui(pContext->mVeChn, &H265Vui);
                H265Vui.VuiTimeInfo.timing_info_present_flag = 1;
                H265Vui.VuiTimeInfo.num_units_in_tick = 1000;
                /* Notices: the protocol syntax states that h265 does not need to be multiplied by 2. */
                H265Vui.VuiTimeInfo.time_scale = H265Vui.VuiTimeInfo.num_units_in_tick * pContext->mConfigPara.mVideoFrameRate;
                H265Vui.VuiTimeInfo.num_ticks_poc_diff_one_minus1 = H265Vui.VuiTimeInfo.num_units_in_tick;
                AW_MPI_VENC_SetH265Vui(pContext->mVeChn, &H265Vui);
                alogd("VencChn[%d] fill framerate %d to H265VUI", pContext->mVeChn, pContext->mConfigPara.mVideoFrameRate);
            }
        }

        if (0 <= pContext->mConfigPara.mSuperFrmMode)
        {
            VENC_SUPERFRAME_CFG_S mSuperFrmParam;
            memset(&mSuperFrmParam, 0, sizeof(VENC_SUPERFRAME_CFG_S));
            mSuperFrmParam.enSuperFrmMode = pContext->mConfigPara.mSuperFrmMode;
            mSuperFrmParam.MaxRencodeTimes = pContext->mConfigPara.mSuperMaxRencodeTimes;
            mSuperFrmParam.MaxP2IFrameBitsRatio = pContext->mConfigPara.mSuperMaxP2IFrameBitsRatio;
            if (0 == pContext->mConfigPara.mSuperIFrmBitsThr || 0 == pContext->mConfigPara.mSuperPFrmBitsThr)
            {
                float cmp_bits = 1.5*1024*1024 / 20;
                float dst_bits = (float)pContext->mConfigPara.mVideoBitRate / pContext->mConfigPara.mVideoFrameRate;
                float bits_ratio = dst_bits / cmp_bits;
                mSuperFrmParam.SuperIFrmBitsThr = (unsigned int)((8.0*200*1024) * bits_ratio);
                mSuperFrmParam.SuperPFrmBitsThr = mSuperFrmParam.SuperIFrmBitsThr / 3;
            }
            else
            {
                mSuperFrmParam.SuperIFrmBitsThr = pContext->mConfigPara.mSuperIFrmBitsThr;
                mSuperFrmParam.SuperPFrmBitsThr = pContext->mConfigPara.mSuperPFrmBitsThr;
            }
            alogd("SuperFrm Mode:%d, MaxRencodeTimes:%d, MaxP2IFrameBitsRatio:%.2f, IBitsThr:%d, PBitsThr:%d",
                mSuperFrmParam.enSuperFrmMode, mSuperFrmParam.MaxRencodeTimes, mSuperFrmParam.MaxP2IFrameBitsRatio,
                mSuperFrmParam.SuperIFrmBitsThr, mSuperFrmParam.SuperPFrmBitsThr);
            AW_MPI_VENC_SetSuperFrameCfg(pContext->mVeChn, &mSuperFrmParam);
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type || PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            if (pContext->mConfigPara.mChromaQPOffsetEnable)
            {
                AW_MPI_VENC_SetChromaQPOffset(pContext->mVeChn, pContext->mConfigPara.mChromaQPOffset);
            }
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
        {
            if (pContext->mConfigPara.mH264ConstraintFlagEnable)
            {
                AW_MPI_VENC_SetH264ConstraintFlag(pContext->mVeChn, &pContext->mConfigPara.mH264ConstraintFlag);
            }
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type || PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            AW_MPI_VENC_SetVe2IspD2DLimit(pContext->mVeChn, &pContext->mConfigPara.mVe2IspD2DLimit);
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type || PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            int dstWidthAlign = AWALIGN(pContext->mConfigPara.dstWidth, 16);
            int dstHeightAlign = AWALIGN(pContext->mConfigPara.dstHeight, 16);
            if (dstWidthAlign != pContext->mConfigPara.dstWidth || dstHeightAlign != pContext->mConfigPara.dstHeight)
            {
                VencForceConfWin stConfWin;
                memset(&stConfWin, 0, sizeof(VencForceConfWin));
                stConfWin.en_force_conf = 1;
                stConfWin.left_offset = 0;
                stConfWin.right_offset = dstWidthAlign - pContext->mConfigPara.dstWidth;
                stConfWin.top_offset = 0;
                stConfWin.bottom_offset = dstHeightAlign - pContext->mConfigPara.dstHeight;
                alogd("set ForceConfWin en %d, left %d right %d top %d bottom %d", stConfWin.en_force_conf, stConfWin.left_offset,
                    stConfWin.right_offset, stConfWin.top_offset, stConfWin.bottom_offset);
                AW_MPI_VENC_SetForceConfWin(pContext->mVeChn, &stConfWin);
            }
        }

        if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
        {
            VENC_PARAM_JPEG_S stJpegParam;
            memset(&stJpegParam, 0, sizeof(VENC_PARAM_JPEG_S));
            stJpegParam.Qfactor = pContext->mConfigPara.mQuality;
            AW_MPI_VENC_SetJpegParam(pContext->mVeChn, &stJpegParam);
        }

        if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type || PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
        {
            if (pContext->mConfigPara.mVbrOptEnable)
            {
                VencVbrOptParam stVbrOptParam;
                memset(&stVbrOptParam, 0, sizeof(VencVbrOptParam));
                AW_MPI_VENC_GetVbrOptParam(pContext->mVeChn, &stVbrOptParam);
# if 0
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
                AW_MPI_VENC_SetVbrOptParam(pContext->mVeChn, &stVbrOptParam);
            }
        }

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mVeChn, &cbInfo);

        switch (pContext->mConfigPara.mVideoSkipFrameMode)
        {
        case 1:
        {
            alogd("enable null-skip[%d->%d]", pContext->mConfigPara.mSrcFrameRate, pContext->mConfigPara.mVideoFrameRate);
            AW_MPI_VENC_EnableNullSkip(pContext->mVeChn, TRUE);
            break;
        }
        case 2:
        {
            alogd("enable p-skip[%d->%d]", pContext->mConfigPara.mSrcFrameRate, pContext->mConfigPara.mVideoFrameRate);
            AW_MPI_VENC_EnablePSkip(pContext->mVeChn, TRUE);
            break;
        }
        default:
            break;
        }

        VENC_IspVeLinkAttr stIspVeLinkAttr;
        memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
        if (pContext->mConfigPara.mIspAndVeLinkageEnable)
        {
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            stIspVeLinkAttr.bEnableVe2Isp = TRUE;
            stIspVeLinkAttr.nVipp = pContext->mViDev;
        }
        AW_MPI_VENC_EnableIspVeLink(pContext->mVeChn, &stIspVeLinkAttr);
        alogd("VencChn[%d] ispVeLink:%d-%d-%d", pContext->mVeChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
            stIspVeLinkAttr.nVipp);

        if (pContext->mConfigPara.mCameraAdaptiveMovingAndStaticEnable)
        {
            setVencLensMovingMaxQp(pContext->mVeChn, pContext->mConfigPara.mVencLensMovingMaxQp);
        }

        return SUCCESS;
    }
}

static ERRORTYPE createViChn(SAMPLE_VI2VENC2MUXER_S *pContext)
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
    pContext->mViAttr.wdr_mode = pContext->mConfigPara.wdr_en;
    alogd("wdr_mode %d", pContext->mViAttr.wdr_mode);
    pContext->mViAttr.fps = pContext->mConfigPara.mSrcFrameRate;
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

    // Saturation change
    if (pContext->mConfigPara.mSaturationChange)
    {
        int nSaturationValue = 0;
        AW_MPI_ISP_GetSaturation(pContext->mIspDev, &nSaturationValue);
        alogd("current SaturationValue: %d", nSaturationValue);
        nSaturationValue = nSaturationValue + pContext->mConfigPara.mSaturationChange;
        AW_MPI_ISP_SetSaturation(pContext->mIspDev, nSaturationValue);
        AW_MPI_ISP_GetSaturation(pContext->mIspDev, &nSaturationValue);
        alogd("after change, SaturationValue: %d", nSaturationValue);
    }

    ViVirChnAttrS stVirChnAttr;
    memset(&stVirChnAttr, 0, sizeof(ViVirChnAttrS));
    stVirChnAttr.mbRecvInIdleState = TRUE;
    ret = AW_MPI_VI_CreateVirChn(pContext->mViDev, pContext->mViChn, &stVirChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pContext->mViChn);
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VI_RegisterCallback(pContext->mViDev, &cbInfo);
    
    ret = AW_MPI_VI_EnableVipp(pContext->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
    }
    return ret;
}

static ERRORTYPE prepare(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    BOOL nSuccessFlag;
    MUX_CHN nMuxChn;
    MUX_CHN_INFO_S *pEntry, *pTmp;
    ERRORTYPE ret;
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

    if (createMuxChn(pContext) != SUCCESS)
    {
        aloge("create mux group fail");
        return result;
    }

    //set spspps
    if (pContext->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        VencHeaderData H264SpsPpsInfo;
        memset(&H264SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVeChn, &H264SpsPpsInfo);
        if (SUCCESS != ret)
        {
            aloge("fatal error, venc GetH264SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        AW_MPI_MUX_SetH264SpsPpsInfo(pContext->mMuxChn, pContext->mVeChn, &H264SpsPpsInfo);
    }
    else if(pContext->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        VencHeaderData H265SpsPpsInfo;
        memset(&H265SpsPpsInfo, 0, sizeof(VencHeaderData));
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVeChn, &H265SpsPpsInfo);
        if (SUCCESS != ret)
        {
            aloge("fatal error, venc GetH265SpsPpsInfo failed! ret=%d", ret);
            return result;
        }
        AW_MPI_MUX_SetH265SpsPpsInfo(pContext->mMuxChn, pContext->mVeChn, &H265SpsPpsInfo);
    }

    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};

        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    if (pContext->mVeChn >= 0 && pContext->mMuxChn >= 0)
    {
        MPP_CHN_S MuxChn = {MOD_ID_MUX, 0, pContext->mMuxChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};

        AW_MPI_SYS_Bind(&VeChn, &MuxChn);
        pContext->mCurrentState = REC_PREPARED;
    }

    result = SUCCESS;
    return result;
}

static ERRORTYPE start(SAMPLE_VI2VENC2MUXER_S *pContext)
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

    if (pContext->mMuxChn >= 0)
    {
        AW_MPI_MUX_StartChn(pContext->mMuxChn);
    }

    pContext->mCurrentState = REC_RECORDING;

    return ret;
}

static ERRORTYPE stop(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    MUX_CHN_INFO_S *pEntry, *pTmp;
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

    if (pContext->mMuxChn >= 0)
    {
        alogd("stop mux chn");
        AW_MPI_MUX_StopChn(pContext->mMuxChn, FALSE);
    }
    if (pContext->mMuxChn >= 0)
    {
        alogd("destory mux chn");
        AW_MPI_MUX_DestroyChn(pContext->mMuxChn);
        pContext->mMuxChn = MM_INVALID_CHN;
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

    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        alogd("free chn list node");
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFd > 0)
            {
                alogd("close file");
                close(pEntry->mSinkInfo.mOutputFd);
                pEntry->mSinkInfo.mOutputFd = -1;
            }

            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);

    return SUCCESS;
}

static ERRORTYPE resetCamera(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    alogd("stop");

    pContext->resetCameraCnt++;

    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pContext->mViDev, pContext->mViChn);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pContext->mVeChn);
    }

    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        alogd("UnBind vi ve");
        AW_MPI_SYS_UnBind(&ViChn, &VeChn);
    }

    if (pContext->mViChn >= 0)
    {
        alogd("DestroyVirChn");
        AW_MPI_VI_DestroyVirChn(pContext->mViDev, pContext->mViChn);
        alogd("DisableVipp");
        AW_MPI_VI_DisableVipp(pContext->mViDev);
#if ISP_RUN
        alogd("ISP_Stop");
        AW_MPI_ISP_Stop(pContext->mIspDev);
#endif
        alogd("DestroyVipp");
        AW_MPI_VI_DestroyVipp(pContext->mViDev);
    }

    alogd("createViChn");
    createViChn(pContext);

    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        alogd("Bind vi ve");
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    alogd("start");

    if (pContext->mViDev >= 0 && pContext->mViChn >= 0)
    {
        alogd("VI_EnableVirChn");
        AW_MPI_VI_EnableVirChn(pContext->mViDev, pContext->mViChn);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("VENC_StartRecvPic");
        AW_MPI_VENC_StartRecvPic(pContext->mVeChn);
    }

    return SUCCESS;
}

static int initGdcLdcProParam(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    int ret = 0;
    FILE *fp = fopen(pContext->mConfigPara.mGdcLdcProLutBin, "rb");
    if (NULL == fp)
    {
        ret = -1;
        aloge("fatal error! gdc ldc pro lut bin[%s] opne fail!", \
            pContext->mConfigPara.mGdcLdcProLutBin);
        goto _exit;
    }
    fseek(fp, 0, SEEK_END);
    pContext->mConfigPara.mGdcLdcProLutBinDataLen = ftell(fp);
    pContext->mConfigPara.mpGdcLdcProLutBinData = (char *)malloc(pContext->mConfigPara.mGdcLdcProLutBinDataLen);
    if (NULL == pContext->mConfigPara.mpGdcLdcProLutBinData)
    {
        ret = -1;
        aloge("fatal error! malloc gdc ldc pro lut data buffer fail!");
        goto _close_file;
    }
    fseek(fp, 0, SEEK_SET);
    fread(pContext->mConfigPara.mpGdcLdcProLutBinData, pContext->mConfigPara.mGdcLdcProLutBinDataLen, 1 ,fp);
_close_file:
    fclose(fp);
    fp = NULL;
_exit:
    return ret;
}

static void deInitGdcLdcProParam(SAMPLE_VI2VENC2MUXER_S *pContext)
{
    if (pContext->mConfigPara.mpGdcLdcProLutBinData)
    {
        free(pContext->mConfigPara.mpGdcLdcProLutBinData);
        pContext->mConfigPara.mpGdcLdcProLutBinData = NULL;
    }
    return;
}

static void *MsgQueueThread(void *pThreadData)
{
    SAMPLE_VI2VENC2MUXER_S *pContext = (SAMPLE_VI2VENC2MUXER_S*)pThreadData;
    message_t stCmdMsg;
    Vi2Venc2MuxerMsgType cmd;
    int nCmdPara;

    alogd("msg queue thread start run!");
    while (1)
    {
        if (0 == get_message(&pContext->mMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case Rec_NeedSetNextFd:
                {
                    int muxChn = nCmdPara;
                    char fileName[MAX_FILE_PATH_LEN] = {0};
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;

                    if (muxChn == pMsgData->mpVi2Venc2MuxerData->mMuxChn)
                    {
                        getFileNameByCurTime(pMsgData->mpVi2Venc2MuxerData, fileName);
                        FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
                        memset(pFilePathNode, 0, sizeof(FilePathNode));
                        strncpy(pFilePathNode->strFilePath, fileName, MAX_FILE_PATH_LEN-1);
                        list_add_tail(&pFilePathNode->mList, &pMsgData->mpVi2Venc2MuxerData->mMuxerFileListArray);
                    }
                    alogd("muxChn[%d] set next fd, filepath=%s", muxChn, fileName);
                    setOutputFileSync(pContext, fileName, 0, muxChn);
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Rec_FileDone:
                {
                    int ret;
                    int muxChn = nCmdPara;
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;

                    if (muxChn == pMsgData->mpVi2Venc2MuxerData->mMuxChn)
                    {
                        int cnt = 0;
                        struct list_head *pList;
                        list_for_each(pList, &pMsgData->mpVi2Venc2MuxerData->mMuxerFileListArray){cnt++;}
                        FilePathNode *pNode = NULL;
                        while (cnt > pMsgData->mpVi2Venc2MuxerData->mConfigPara.mDstFileMaxCnt)
                        {
                            pNode = list_first_entry(&pMsgData->mpVi2Venc2MuxerData->mMuxerFileListArray, FilePathNode, mList);
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
                    }
                    else
                    {
                        aloge("fatal error! check muxChn[%d!=%d]", muxChn, pMsgData->mpVi2Venc2MuxerData->mMuxChn);
                    }
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
                    break;
                }
                case Vi_Timeout:
                {
                    int ret;
                    Vi2Venc2Muxer_MessageData *pMsgData = (Vi2Venc2Muxer_MessageData*)stCmdMsg.mpData;

                    resetCamera(pContext);
                    
                    //free msg mpdata
                    free(stCmdMsg.mpData);
                    stCmdMsg.mpData = NULL;
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
            TMessage_WaitQueueNotEmpty(&pContext->mMsgQueue, 0);
        }
    }
_Exit:
    alogd("msg queue thread exit!");
    return NULL;
}

int main(int argc, char** argv)
{
    int result = -1;
    MUX_CHN_INFO_S *pEntry, *pTmp;
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
    
	alogd("sample_virvi2venc2muxer running!\n");
    SAMPLE_VI2VENC2MUXER_S *pContext = (SAMPLE_VI2VENC2MUXER_S* )malloc(sizeof(SAMPLE_VI2VENC2MUXER_S));

    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        result = FAILURE;
        goto _err0;
    }
    if (InitVi2Venc2MuxerData(pContext) != SUCCESS)
    {
        return -1;
    }
    gpVi2Venc2MuxerData = pContext;
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
    CreateFolder(pContext->mDstDir);
    if ((pContext->mConfigPara.mEnableGdc) && (Gdc_Warp_LDC_Pro == pContext->mConfigPara.mGdcWarpMode))
    {
        int ret = initGdcLdcProParam(pContext);
        if (ret)
        {
            aloge("initGdcLdcProParam fail! disable gdc!");
            pContext->mConfigPara.mEnableGdc = 0;
        }
    }

    INIT_LIST_HEAD(&pContext->mMuxChnList);
    pthread_mutex_init(&pContext->mMuxChnListLock, NULL);

    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

#ifdef TEST_LEAKTRACER
    //leaktracer has some problems with alsa functions. So first run alsa functions such as snd_pcm_open(), snd_mixer_attach(),
    //then run leaktracer. Then LeakTracer can work with alsa functions normally.
    leaktracer_startMonitoringAllThreads();
#endif

    result = addOutputFormatAndOutputSink(pContext, pContext->mConfigPara.dstVideoFile, pContext->eFileFormat);
    if (result < 0)
    {
        aloge("add first out file fail");
        goto err_out_1;
    }
    FilePathNode *pFilePathNode = (FilePathNode*)malloc(sizeof(FilePathNode));
    memset(pFilePathNode, 0, sizeof(FilePathNode));
    strncpy(pFilePathNode->strFilePath, pContext->mConfigPara.dstVideoFile, MAX_FILE_PATH_LEN-1);
    list_add_tail(&pFilePathNode->mList, &pContext->mMuxerFileListArray);

    if (prepare(pContext) != SUCCESS)
    {
        aloge("prepare fail!");
        goto err_out_2;
    }

    //create msg queue thread
    result = pthread_create(&pContext->mMsgQueueThreadId, NULL, MsgQueueThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create Msg Queue Thread fail[%d]", result);
        goto err_out_3;
    }
    else
    {
        alogd("create Msg Queue Thread success! threadId[0x%x]", &pContext->mMsgQueueThreadId);
    }

    start(pContext);

    //test roi.
    int i = 0;
    ERRORTYPE ret;
    VENC_ROI_CFG_S stMppRoiBlockInfo;
    memset(&stMppRoiBlockInfo, 0, sizeof(VENC_ROI_CFG_S));
    for(i=0; i<pContext->mConfigPara.mRoiNum; i++)
    {
        stMppRoiBlockInfo.Index = i;
        stMppRoiBlockInfo.bEnable = TRUE;
        stMppRoiBlockInfo.bAbsQp = TRUE;
        stMppRoiBlockInfo.Qp = pContext->mConfigPara.mRoiQp;
        stMppRoiBlockInfo.Rect.X = 128*i;
        stMppRoiBlockInfo.Rect.Y = 128*i;
        stMppRoiBlockInfo.Rect.Width = 128;
        stMppRoiBlockInfo.Rect.Height = 128;
        ret = AW_MPI_VENC_SetRoiCfg(pContext->mVeChn, &stMppRoiBlockInfo);
        if(ret != SUCCESS)
        {
            aloge("fatal error! set roi[%d] fail[0x%x]!", i, ret);
        }
        else
        {
            alogd("set roiIndex:%d, Qp:%d-%d, Rect[%d,%d,%dx%d]", i, stMppRoiBlockInfo.bAbsQp, stMppRoiBlockInfo.Qp, 
                stMppRoiBlockInfo.Rect.X, stMppRoiBlockInfo.Rect.Y, stMppRoiBlockInfo.Rect.Width, stMppRoiBlockInfo.Rect.Height);
        }
    }
    
    if(pContext->mConfigPara.mRoiNum>0 && pContext->mConfigPara.mRoiBgFrameRateEnable)
    {
        VENC_ROIBG_FRAME_RATE_S stRoiBgFrmRate;
        ret = AW_MPI_VENC_GetRoiBgFrameRate(pContext->mVeChn, &stRoiBgFrmRate);
        if(ret != SUCCESS)
        {
            aloge("fatal error! get roiBgFrameRate fail[0x%x]!", ret);
        }
        alogd("get roi bg frame rate:%d-%d", stRoiBgFrmRate.mSrcFrmRate, stRoiBgFrmRate.mDstFrmRate);
        if (pContext->mConfigPara.mRoiBgFrameRateAttenuation)
        {
            stRoiBgFrmRate.mDstFrmRate = stRoiBgFrmRate.mSrcFrmRate/pContext->mConfigPara.mRoiBgFrameRateAttenuation;
        }
        else
        {
            stRoiBgFrmRate.mDstFrmRate = stRoiBgFrmRate.mSrcFrmRate;
        }
        if(stRoiBgFrmRate.mDstFrmRate <= 0)
        {
            stRoiBgFrmRate.mDstFrmRate = 1;
        }
        ret = AW_MPI_VENC_SetRoiBgFrameRate(pContext->mVeChn, &stRoiBgFrmRate);
        if(ret != SUCCESS)
        {
            aloge("fatal error! set roiBgFrameRate fail[0x%x]!", ret);
        }
        alogd("set roi bg frame rate param:%d-%d", stRoiBgFrmRate.mSrcFrmRate, stRoiBgFrmRate.mDstFrmRate);
    }

    //test orl
    RGN_ATTR_S stRgnAttr;
    RGN_CHN_ATTR_S stRgnChnAttr;
    memset(&stRgnAttr, 0, sizeof(RGN_ATTR_S));
    memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    MPP_CHN_S viChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
    for(i=0; i<pContext->mConfigPara.mOrlNum; i++)
    {
        stRgnAttr.enType = ORL_RGN;
        ret = AW_MPI_RGN_Create(i, &stRgnAttr);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why create ORL region fail?[0x%x]", ret);
            break;
        }
        stRgnChnAttr.bShow = TRUE;
        stRgnChnAttr.enType = ORL_RGN;
        stRgnChnAttr.unChnAttr.stOrlChn.enAreaType = AREA_RECT;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.X = i*120;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Y = i*60;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Width = 100;
        stRgnChnAttr.unChnAttr.stOrlChn.stRect.Height = 50;
        stRgnChnAttr.unChnAttr.stOrlChn.mColor = 0xFF0000 >> ((i % 3)*8);
        stRgnChnAttr.unChnAttr.stOrlChn.mThick = 6;
        stRgnChnAttr.unChnAttr.stOrlChn.mLayer = i;
        ret = AW_MPI_RGN_AttachToChn(i, &viChn, &stRgnChnAttr);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why attach to vi channel[%d,%d] fail?", pContext->mViDev, pContext->mViChn);
        }
        
    }

    if(pContext->mConfigPara.mOrlNum > 0)
    {
        alogd("sleep 10s ...");
        sleep(10);
    }
    for(i=0; i<pContext->mConfigPara.mOrlNum; i++)
    {
        ret = AW_MPI_RGN_Destroy(i);
        if(ret != SUCCESS)
        {
            aloge("fatal error! why destory region:%d fail?", i);
        }
    }

    //test SEI
    if (pContext->mConfigPara.eSeiEnable <= VencSei_Enable)
    {
        VENC_SEI_ATTR stVencSeiAttr;
        memset(&stVencSeiAttr, 0, sizeof(stVencSeiAttr));
        stVencSeiAttr.eSeiEnableSetting = pContext->mConfigPara.eSeiEnable;
        if(pContext->mConfigPara.bSeiDataIsp)
        {
            stVencSeiAttr.nSeiDataTypeFlags |= SEIDataType_ISP;
        }
        if(pContext->mConfigPara.bSeiDataVipp)
        {
            stVencSeiAttr.nSeiDataTypeFlags |= SEIDataType_VIPP;
        }
        if(pContext->mConfigPara.bSeiDataVenc)
        {
            stVencSeiAttr.nSeiDataTypeFlags |= SEIDataType_VENC;
        }
        stVencSeiAttr.nIspDev = pContext->mIspDev;
        stVencSeiAttr.nVipp = pContext->mConfigPara.mVippDev;
        stVencSeiAttr.nFrameIntervalForISPLevel1 = pContext->mConfigPara.nSeiFrameIntervalIspLevel1;
        stVencSeiAttr.nFrameIntervalForISPLevel2 = pContext->mConfigPara.nSeiFrameIntervalIspLevel2;
        stVencSeiAttr.nFrameIntervalForISPLevel3 = pContext->mConfigPara.nSeiFrameIntervalIspLevel3;
        stVencSeiAttr.nFrameIntervalForVIPP = pContext->mConfigPara.nSeiFrameIntervalVipp;
        stVencSeiAttr.nFrameIntervalForVencLevel1 = pContext->mConfigPara.nSeiFrameIntervalVencLevel1;
        stVencSeiAttr.nFrameIntervalForVencLevel2 = pContext->mConfigPara.nSeiFrameIntervalVencLevel2;
        AW_MPI_VENC_ConfigSEI(pContext->mVeChn, &stVencSeiAttr);
    }
    else
    {
        alogd("ignore to config sei!");
    }

#if TEST_DROP_FRAME
    sleep(1);
    AW_MPI_VENC_DropFrame(pContext->mVeChn, 1);
    sleep(1);
    AW_MPI_VENC_DropFrame(pContext->mVeChn, 2);
    sleep(2);
#endif
    alogd("wait for test time %ds ...", pContext->mConfigPara.mTestDuration);
    if (pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    alogd("test time %ds is up, stop test", pContext->mConfigPara.mTestDuration);

    result = 0;

    //check result
    if (0 < pContext->resetCameraCnt)
    {
        result = FAILURE;
        aloge("fatal error! no frame input, resetCameraCnt=%d", pContext->resetCameraCnt);
    }

    //stop msg queue thread
    message_t stMsgCmd;
    stMsgCmd.command = MsgQueue_Stop;
    put_message(&pContext->mMsgQueue, &stMsgCmd);
    pthread_join(pContext->mMsgQueueThreadId, NULL);
    alogd("start to free res");
err_out_3:
    stop(pContext);
err_out_2:
    pthread_mutex_lock(&pContext->mMuxChnListLock);
    if (!list_empty(&pContext->mMuxChnList))
    {
        alogd("chn list not empty");
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mMuxChnList, mList)
        {
            if (pEntry->mSinkInfo.mOutputFd > 0)
            {
                close(pEntry->mSinkInfo.mOutputFd);
                pEntry->mSinkInfo.mOutputFd = -1;
            }
        }

        list_del(&pEntry->mList);
        free(pEntry);
    }
    pthread_mutex_unlock(&pContext->mMuxChnListLock);
err_out_1:
    AW_MPI_SYS_Exit();

#ifdef TEST_LEAKTRACER
    leaktracer_stopAllMonitoring();
    leaktracer_writeLeaksToFile("/mnt/extsd/leaks.out");
#endif

    pthread_mutex_destroy(&pContext->mMuxChnListLock);
err_out_0:
    cdx_sem_deinit(&pContext->mSemExit);
    message_destroy(&pContext->mMsgQueue);
    if ((pContext->mConfigPara.mEnableGdc) && (Gdc_Warp_LDC_Pro == pContext->mConfigPara.mGdcWarpMode))
    {
        deInitGdcLdcProParam(pContext);
    }
    free(pContext);
    gpVi2Venc2MuxerData = pContext = NULL;
_err0:
	alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();

    return result;
}
