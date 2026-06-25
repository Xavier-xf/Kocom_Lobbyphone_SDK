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
#define LOG_TAG "sample_vencGdcZoom"

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "plat_log.h"
#include <mm_common.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_region.h>
#include <mpi_vi_private.h>
#include "sample_vencGdcZoom.h"
#include "sample_vencGdcZoom_config.h"
#include "../common/sample_common_venc.h"

#define ISP_RUN (1)

static SAMPLE_VENC_GDCZOOM_S *gpVencGdcZoomData = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpVencGdcZoomData)
    {
        cdx_sem_up(&gpVencGdcZoomData->mSemExit);
    }
}

static ERRORTYPE InitVencGdcZoomData(SAMPLE_VENC_GDCZOOM_S *pContext)
{
    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        return FAILURE;
    }
    memset(pContext, 0, sizeof(SAMPLE_VENC_GDCZOOM_S));
    pContext->mVeChn = MM_INVALID_CHN;
    pContext->mViChn = MM_INVALID_CHN;
    pContext->mViDev = MM_INVALID_DEV;

    return SUCCESS;
}

static ERRORTYPE parseCmdLine(SAMPLE_VENC_GDCZOOM_S *pContext, int argc, char** argv)
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

static ERRORTYPE loadConfigPara(SAMPLE_VENC_GDCZOOM_S *pContext, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;

    if (NULL == conf_path || NULL == pContext)
    {
        aloge("fatal error! invalid input params!");
        return -1;
    }
    CONFPARSER_S mConf;
    memset(&mConf, 0, sizeof(CONFPARSER_S));
    ret = createConfParser(conf_path, &mConf);
    if (ret)
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

    alogd("srcPixFmt=%d", pContext->mConfigPara.srcPixFmt);

    ptr = (char *)GetConfParaString(&mConf, CFG_DST_VIDEO_FILE_STR, NULL);
    if (ptr != NULL)
    {
        strcpy(pContext->mConfigPara.dstVideoFile, ptr);
    }

    pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_FRAMERATE, 0);
    pContext->mConfigPara.mViBufferNum = GetConfParaInt(&mConf, CFG_DST_VI_BUFFER_NUM, 0);
    pContext->mConfigPara.mVideoBitRate = GetConfParaInt(&mConf, CFG_DST_VIDEO_BITRATE, 0);

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

    alogd("vipp:%d, frame rate:%d, bitrate:%d, test_time=%d", pContext->mConfigPara.mVippDev,\
        pContext->mConfigPara.mVideoFrameRate, pContext->mConfigPara.mVideoBitRate,\
        pContext->mConfigPara.mTestDuration);

    pContext->mConfigPara.mDynamicCropEnable = GetConfParaInt(&mConf, CFG_DYNAMIC_CROP_ENABLE, 0);
    alogd("venc dynamic crop enable:%d", pContext->mConfigPara.mDynamicCropEnable);

    pContext->mConfigPara.mOnlineEnable = GetConfParaInt(&mConf, CFG_online_en, 0);
    pContext->mConfigPara.mOnlineShareBufNum = GetConfParaInt(&mConf, CFG_online_share_buf_num, 0);
    alogd("OnlineEnable: %d, OnlineShareBufNum: %d", pContext->mConfigPara.mOnlineEnable,
        pContext->mConfigPara.mOnlineShareBufNum);

    pContext->mConfigPara.wdr_en = GetConfParaInt(&mConf, CFG_WDR_EN, 0);
    alogd("wdr_en: %d", pContext->mConfigPara.wdr_en);

    pContext->mConfigPara.mEnableGdc = GetConfParaInt(&mConf, CFG_EnableGdc, 0);
    alogd("EnableGdc: %d", pContext->mConfigPara.mEnableGdc);

    if (pContext->mConfigPara.mEnableGdc)
    {
        pContext->mConfigPara.mEnableGdcZoom = GetConfParaInt(&mConf, CFG_EnableGdcZoom, 0);
        alogd("EnableGdcZoom: %d", pContext->mConfigPara.mEnableGdcZoom);
    }
    else
    {
        pContext->mConfigPara.mEnableGdcZoom = 0;
        alogw("Gdc is not enable, disable GdcZoom!");
    }

    if (pContext->mConfigPara.mEnableGdc && pContext->mConfigPara.mEnableGdcZoom && pContext->mConfigPara.mDynamicCropEnable)
    {
        alogw("GDC zoom and dynamic crop cannot open simultaneously");
        ret = -1;
    }

    destroyConfParser(&mConf);

    return ret;
}

static void initGdcParam(sGdcParam *pGdcParam)
{
    pGdcParam->bGDC_en = 1;
    pGdcParam->eWarpMode = Gdc_Warp_LDC;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
    pGdcParam->bMirror = 0;

    pGdcParam->fx = 2417.19;
    pGdcParam->fy = 2408.43;
    pGdcParam->cx = 1631.50;
    pGdcParam->cy = 1223.50;
    pGdcParam->fx_scale = 2161.82;
    pGdcParam->fy_scale = 2153.99;
    pGdcParam->cx_scale = 1631.50;
    pGdcParam->cy_scale = 1223.50;

    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    pGdcParam->distCoef_wide_ra[0] = -0.3849;
    pGdcParam->distCoef_wide_ra[1] = 0.1567;
    pGdcParam->distCoef_wide_ra[2] = -0.0030;
    pGdcParam->distCoef_wide_ta[0] = -0.00005;
    pGdcParam->distCoef_wide_ta[1] = 0.0016;

    pGdcParam->distCoef_fish_k[0]  = -0.0024;
    pGdcParam->distCoef_fish_k[1]  = 0.141;
    pGdcParam->distCoef_fish_k[2]  = -0.3;
    pGdcParam->distCoef_fish_k[3]  = 0.2328;

    pGdcParam->centerOffsetX         =      0;     //[-255,0]
    pGdcParam->centerOffsetY         =      0;     //[-255,0]
    pGdcParam->rotateAngle           =      0;     //[0,360]
    pGdcParam->radialDistortCoef     =      0;     //[-255,255]
    pGdcParam->trapezoidDistortCoef  =      0;     //[-255,255]
    pGdcParam->fanDistortCoef        =      0;     //[-255,255]
    pGdcParam->pan                   =      0;     //pano360:[0,360]; others:[-90,90]
    pGdcParam->tilt                  =      0;     //[-90,90]
    pGdcParam->zoomH                 =      100;   //[0,100]
    pGdcParam->zoomV                 =      100;   //[0,100]
    pGdcParam->scale                 =      100;   //[0,100]
    pGdcParam->innerRadius           =      0;     //[0,width/2]
    pGdcParam->roll                  =      0;     //[-90,90]
    pGdcParam->pitch                 =      0;     //[-90,90]
    pGdcParam->yaw                   =      0;     //[-90,90]

    pGdcParam->perspFunc             =    Gdc_Persp_Only;
    pGdcParam->perspectiveProjMat[0] =    1.0;
    pGdcParam->perspectiveProjMat[1] =    0.0;
    pGdcParam->perspectiveProjMat[2] =    0.0;
    pGdcParam->perspectiveProjMat[3] =    0.0;
    pGdcParam->perspectiveProjMat[4] =    1.0;
    pGdcParam->perspectiveProjMat[5] =    0.0;
    pGdcParam->perspectiveProjMat[6] =    0.0;
    pGdcParam->perspectiveProjMat[7] =    0.0;
    pGdcParam->perspectiveProjMat[8] =    1.0;

    pGdcParam->mountHeight           =      0.85; //meters
    pGdcParam->roiDist_ahead         =      4.5;  //meters
    pGdcParam->roiDist_left          =     -1.5;  //meters
    pGdcParam->roiDist_right         =      1.5;  //meters
    pGdcParam->roiDist_bottom        =      0.65; //meters

    pGdcParam->peaking_en            =      1;    //0/1
    pGdcParam->peaking_clamp         =      1;    //0/1
    pGdcParam->peak_m                =     16;    //[0,63]
    pGdcParam->th_strong_edge        =      6;    //[0,15]
    pGdcParam->peak_weights_strength =      2;    //[0,15]

    if (pGdcParam->eWarpMode == Gdc_Warp_LDC)
    {
        pGdcParam->birdsImg_width    = 768;
        pGdcParam->birdsImg_height   = 1080;
    }

    alogd("init gdc param, WarpMode:%d, zoomH:%d, zoomV:%d",
        pGdcParam->eWarpMode, pGdcParam->zoomH, pGdcParam->zoomV);
}

static ERRORTYPE configVencChnAttr(SAMPLE_VENC_GDCZOOM_S *pContext)
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
    pContext->mVencChnAttr.VeAttr.MaxKeyInterval = 100;
    pContext->mVencChnAttr.VeAttr.SrcPicWidth  = pContext->mConfigPara.srcWidth;
    pContext->mVencChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.srcHeight;
    pContext->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mVencChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.srcPixFmt;
    pContext->mVencChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_REC709;
    alogd("pixfmt:0x%x", pContext->mVencChnAttr.VeAttr.PixelFormat);
    pContext->mVencChnAttr.VeAttr.mDropFrameNum = 0;
    alogd("DropFrameNum:%d", pContext->mVencChnAttr.VeAttr.mDropFrameNum);
    pContext->mVencChnAttr.EncppAttr.eEncppSharpSetting = VencEncppSharp_FollowISPConfig;

    pContext->mVencChnAttr.RcAttr.mProductMode = PRODUCT_STATIC_IPC;

    if (pContext->mConfigPara.mVideoFrameRate)
    {
        vbvThreshSize = pContext->mConfigPara.mVideoBitRate/8/pContext->mConfigPara.mVideoFrameRate*15;
    }
    vbvBufSize = pContext->mConfigPara.mVideoBitRate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("sub vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    VencRateCtrlConfig stRcConfig;
    memset(&stRcConfig, 0, sizeof(VencRateCtrlConfig));
    stRcConfig.mEncodeType = pContext->mConfigPara.mVideoEncoderFmt;
    stRcConfig.mEncodeWidth = pContext->mConfigPara.dstWidth;
    stRcConfig.mEncodeHeight = pContext->mConfigPara.dstHeight;
    stRcConfig.mSrcFrameRate = pContext->mConfigPara.mVideoFrameRate;
    stRcConfig.mDstFrameRate = pContext->mConfigPara.mVideoFrameRate;
    stRcConfig.mEncodeBitrate = pContext->mConfigPara.mVideoBitRate;
    if (PT_H264 == pContext->mConfigPara.mVideoEncoderFmt)
        stRcConfig.mRcMode = VENC_RC_MODE_H264VBR;
    else if (PT_H265 == pContext->mConfigPara.mVideoEncoderFmt)
        stRcConfig.mRcMode = VENC_RC_MODE_H265VBR;
    else
        stRcConfig.mRcMode = VENC_RC_MODE_MJPEGCBR;
    stRcConfig.mMaxIQp = 45;
    stRcConfig.mMinIQp = 25;
    stRcConfig.mMaxPQp = 45;
    stRcConfig.mMinPQp = 25;
    stRcConfig.mInitQp = 37;
    stRcConfig.mEnMbQpLimit = 1;
    stRcConfig.mQuality = 1;
    stRcConfig.mMovingTh = 10;
    stRcConfig.mIBitsCoef = 10;
    stRcConfig.mPBitsCoef = 10;
    stRcConfig.mVbvBufSize = vbvBufSize;
    stRcConfig.mVbvThreshSize = vbvThreshSize;
    configVencRateCtrlParam(&stRcConfig, &pContext->mVencChnAttr, &pContext->mVencRcParam);

    alogd("venc set Rcmode=%d", pContext->mVencChnAttr.RcAttr.mRcMode);

    pContext->mVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->mVencChnAttr.GopAttr.mGopSize = 2;

    if (pContext->mConfigPara.mEnableGdc)
    {
        alogd("enable GDC and init GDC params");
        initGdcParam(&pContext->mVencChnAttr.GdcAttr);
    }

    return SUCCESS;
}

static ERRORTYPE createVencChn(SAMPLE_VENC_GDCZOOM_S *pContext)
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

        return SUCCESS;
    }
}

static ERRORTYPE createViChn(SAMPLE_VENC_GDCZOOM_S *pContext)
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
    pContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709;
    pContext->mViAttr.format.width = pContext->mConfigPara.srcWidth;
    pContext->mViAttr.format.height = pContext->mConfigPara.srcHeight;
    pContext->mViAttr.nbufs =  pContext->mConfigPara.mViBufferNum;
    alogd("vipp use %d v4l2 buffers, colorspace: 0x%x", pContext->mViAttr.nbufs, pContext->mViAttr.format.colorspace);
    pContext->mViAttr.nplanes = 2;
    pContext->mViAttr.wdr_mode = pContext->mConfigPara.wdr_en;
    alogd("wdr_mode %d", pContext->mViAttr.wdr_mode);
    pContext->mViAttr.fps = pContext->mConfigPara.mVideoFrameRate;
    pContext->mViAttr.drop_frame_num = 0;
    pContext->mViAttr.mbEncppEnable = TRUE;

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

static ERRORTYPE prepare(SAMPLE_VENC_GDCZOOM_S *pContext)
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

    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};

        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }

    return 0;
}

static ERRORTYPE start(SAMPLE_VENC_GDCZOOM_S *pContext)
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

static ERRORTYPE stop(SAMPLE_VENC_GDCZOOM_S *pContext)
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

static void *GetEncoderFrameThread(void *pThreadData)
{
    SAMPLE_VENC_GDCZOOM_S *pContext = (SAMPLE_VENC_GDCZOOM_S*)pThreadData;
    int ret = 0;
    int count = 0;
    VENC_CHN nVencChn = pContext->mVeChn;

    //set spspps
    VencHeaderData SpsPpsInfo;
    if (pContext->mConfigPara.mVideoEncoderFmt == PT_H264)
    {
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVeChn, &SpsPpsInfo);
        if (SUCCESS == ret)
        {
            if(SpsPpsInfo.nLength)
                fwrite(SpsPpsInfo.pBuffer, 1, SpsPpsInfo.nLength, pContext->mOutputFileFp);
        }
        else
        {
            alogd("AW_MPI_VENC_GetH264SpsPpsInfo failed!\n");
            ret = -1;
        }
    }
    else if(pContext->mConfigPara.mVideoEncoderFmt == PT_H265)
    {
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVeChn, &SpsPpsInfo);
        if (SUCCESS == ret)
        {
            if (SpsPpsInfo.nLength)
                fwrite(SpsPpsInfo.pBuffer, 1, SpsPpsInfo.nLength, pContext->mOutputFileFp);
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

    int gdc_zoom_cnt = 0;
    int test_enlarge_flag = 1;

    int crop_zoom_cnt = 0;
    int width = pContext->mConfigPara.srcWidth;
    int Height = pContext->mConfigPara.srcHeight;
    int crop_step = 16;

    while (0 == pContext->mExitFlag)
    {
        if (0 == test_enlarge_flag)
        {
            alogd("quit test.");
            break;
        }

        count++;

        if (pContext->mConfigPara.mDynamicCropEnable)
        {
            if(0 == count%20)
            {
                VENC_CROP_CFG_S stCropCfg;
                memset(&stCropCfg, 0, sizeof(VENC_CROP_CFG_S));
                stCropCfg.bEnable = 1;
                stCropCfg.Rect.X = AWALIGN(crop_step * crop_zoom_cnt, 16);
                stCropCfg.Rect.Y = AWALIGN(crop_step * crop_zoom_cnt, 16);
                if (width <= (stCropCfg.Rect.X * 2) || Height <= (stCropCfg.Rect.Y * 2))
                {
                    alogd("quit test, width:%d <= RectX:%d, Height:%d <= RectY:%d", width, stCropCfg.Rect.X*2, Height, stCropCfg.Rect.Y*2);
                    break;
                }
                stCropCfg.Rect.Width = AWALIGN(width - stCropCfg.Rect.X * 2, 16);
                stCropCfg.Rect.Height = AWALIGN(Height - stCropCfg.Rect.Y * 2, 16);
                AW_MPI_VENC_SetCrop(nVencChn, &stCropCfg);
                alogd("set Crop %d, [%d][%d][%d][%d]", stCropCfg.bEnable, stCropCfg.Rect.X, stCropCfg.Rect.Y, stCropCfg.Rect.Width, stCropCfg.Rect.Height);
                crop_zoom_cnt++;
            }
        }

        if (pContext->mConfigPara.mEnableGdcZoom)
        {
            VENC_CHN_ATTR_S stVencChAttr;
            memset(&stVencChAttr, 0, sizeof(VENC_CHN_ATTR_S));
            AW_MPI_VENC_GetChnAttr(nVencChn, &stVencChAttr);

            // change GDC zoom
            if (stVencChAttr.GdcAttr.bGDC_en && test_enlarge_flag)
            {
                alogv("current gdc param, WarpMode:%d, zoomH:%d, zoomV:%d",
                    stVencChAttr.GdcAttr.eWarpMode, stVencChAttr.GdcAttr.zoomH, stVencChAttr.GdcAttr.zoomV);

                if (100 >= gdc_zoom_cnt)
                {
                    stVencChAttr.GdcAttr.zoomH = 100 + gdc_zoom_cnt;
                    stVencChAttr.GdcAttr.zoomV = 100 + gdc_zoom_cnt;
                    gdc_zoom_cnt++;
                }
                else
                {
                    test_enlarge_flag = 0;
                    gdc_zoom_cnt = 0;
                    alogd("enlarge test end.");
                }

                alogv("change gdc param, WarpMode:%d, zoomH:%d, zoomV:%d",
                    stVencChAttr.GdcAttr.eWarpMode, stVencChAttr.GdcAttr.zoomH, stVencChAttr.GdcAttr.zoomV);

                AW_MPI_VENC_SetChnAttr(nVencChn, &stVencChAttr);
            }
        }

        if((ret = AW_MPI_VENC_GetStream(nVencChn, &VencFrame, 4000)) < 0) //6000(25fps) 4000(30fps)
        {
            alogd("get first frmae failed!\n");
            continue;
        }
        else
        {
            if(VencFrame.mpPack != NULL && VencFrame.mpPack->mLen0)
            {
                fwrite(VencFrame.mpPack->mpAddr0,1,VencFrame.mpPack->mLen0, pContext->mOutputFileFp);
            }
            if(VencFrame.mpPack != NULL && VencFrame.mpPack->mLen1)
            {
                fwrite(VencFrame.mpPack->mpAddr1,1,VencFrame.mpPack->mLen1, pContext->mOutputFileFp);
            }
            ret = AW_MPI_VENC_ReleaseStream(nVencChn, &VencFrame);
            if(ret < 0)
            {
                alogd("falied error,release failed!!!\n");
            }
         }
    }

    stop(pContext);

    if(NULL != gpVencGdcZoomData)
    {
        cdx_sem_up(&gpVencGdcZoomData->mSemExit);
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

    printf("sample_vencGdcZoom running!\n");
    SAMPLE_VENC_GDCZOOM_S *pContext = (SAMPLE_VENC_GDCZOOM_S* )malloc(sizeof(SAMPLE_VENC_GDCZOOM_S));

    if (pContext == NULL)
    {
        aloge("malloc struct fail");
        result = FAILURE;
        goto _err0;
    }
    if (InitVencGdcZoomData(pContext) != SUCCESS)
    {
        return -1;
    }
    gpVencGdcZoomData = pContext;
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

    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    if (prepare(pContext) != SUCCESS)
    {
        aloge("prepare fail!");
        goto err_out_3;
    }

//    alogd("set crop before vencInit()");
//    VENC_CROP_CFG_S stCropCfg;
//    stCropCfg.bEnable = TRUE;
//    stCropCfg.Rect.X = 0;
//    stCropCfg.Rect.Y = 0;
//    stCropCfg.Rect.Width = AWALIGN(pContext->mConfigPara.srcWidth/2, 16);
//    stCropCfg.Rect.Height = AWALIGN(pContext->mConfigPara.srcHeight/2, 16);
//    AW_MPI_VENC_SetCrop(pContext->mVeChn, &stCropCfg);

    result = pthread_create(&pContext->mThreadId, NULL, GetEncoderFrameThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! create Msg Queue Thread fail[%d]", result);
        goto err_out_3;
    }
    else
    {
        alogd("create Msg Queue Thread success! threadId[0x%x]", &pContext->mThreadId);
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
    alogd("start to free res");
err_out_3:
    //stop(pContext);
    result = 0;
err_out_1:
    AW_MPI_SYS_Exit();

err_out_0:
    cdx_sem_deinit(&pContext->mSemExit);
    free(pContext);
    gpVencGdcZoomData = pContext = NULL;
_err0:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
