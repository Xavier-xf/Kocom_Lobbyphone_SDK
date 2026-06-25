/******************************************************************************
  Copyright (C), 2001-2023, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner
  Created       : 2023/04/03
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleEncppGdcOffline"
#include <plat_log.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <plat_math.h>
#include <mpi_sys.h>
#include <veAdapter.h>
#include <memoryAdapter.h>
#include <confparser.h>

#include "sample_EncppGdcOffline.h"
#include "sample_EncppGdcOffline_conf.h"

static int parseCmdLine(SampleEncppGdcOfflineContext *pContext, int argc, char** argv)
{
    int ret = -1;

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

              strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_SIZE-1);
              pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_venc.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SampleEncppGdcOfflineContext *pContext, char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S mConf;

    ret = createConfParser(conf_path, &mConf);
    if (ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

    memset(&pContext->mConfigPara, 0, sizeof(SampleEncppGdcOfflineConfig));
    pContext->mConfigPara.mSrcWidth = GetConfParaInt(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_SRC_WIDTH, 0);
    pContext->mConfigPara.mSrcHeight = GetConfParaInt(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_SRC_HEIGHT, 0);
    ptr = (char *)GetConfParaString(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_SRC_PIC, NULL);
    if (ptr)
    {
        strncpy(pContext->mConfigPara.mSrcPic, ptr, strlen(ptr)+1);
    }
    ptr = (char *)GetConfParaString(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_SRC_FMT, NULL);
    if (ptr)
    {
        if (!strcmp(ptr, "nv21"))
            pContext->mConfigPara.mSrcFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        else if (!strcmp(ptr, "nv12"))
            pContext->mConfigPara.mSrcFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        else
            aloge("unsupport pixel format[%s]", ptr);
    }

    pContext->mConfigPara.mDstWidth = GetConfParaInt(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_DST_WIDTH, 0);
    pContext->mConfigPara.mDstHeight = GetConfParaInt(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_DST_HEIGHT, 0);
    ptr = (char *)GetConfParaString(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_DST_PIC, NULL);
    if (ptr)
    {
        strncpy(pContext->mConfigPara.mDstPic, ptr, strlen(ptr)+1);
    }
    ptr = (char *)GetConfParaString(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_DST_FMT, NULL);
    if (ptr)
    {
        if (!strcmp(ptr, "nv21"))
            pContext->mConfigPara.mDstFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        else if (!strcmp(ptr, "nv12"))
            pContext->mConfigPara.mDstFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        else
            aloge("unsupport pixel format[%s]", ptr);
    }

    pContext->mConfigPara.mGdcWarpType = (eGdcWarpType)GetConfParaInt(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_GDC_WARP_TYPE, 0);
    pContext->mConfigPara.mGdcMountType = (eGdcMountType)GetConfParaInt(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_GDC_MOUNT_TYPE, 0);
    pContext->mConfigPara.mbGdcMirror = GetConfParaInt(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_GDC_MIRROR, 0);
    ptr = (char *)GetConfParaString(&mConf, \
        SAMPLE_ENCPP_GDC_OFFLINE_KEY_GDC_LDC_PRO_LUT_BIN, NULL);
    if (ptr)
        strncpy(pContext->mConfigPara.mGdcLDCProLutBin, ptr, strlen(ptr)+1);

    alogd("src pic[%s] size[%dx%d] pixel format[%d]", pContext->mConfigPara.mSrcPic, \
        pContext->mConfigPara.mSrcWidth, pContext->mConfigPara.mSrcHeight, pContext->mConfigPara.mSrcFmt);
    alogd("dst pic[%s] size[%dx%d] pixel format[%d]", pContext->mConfigPara.mDstPic, \
        pContext->mConfigPara.mDstWidth, pContext->mConfigPara.mDstHeight, pContext->mConfigPara.mDstFmt);
    alogd("warpType[%d] mountType[%d] mirror[%d] LdcProLutBin[%s]", \
        pContext->mConfigPara.mGdcWarpType, pContext->mConfigPara.mGdcMountType, \
        pContext->mConfigPara.mbGdcMirror, pContext->mConfigPara.mGdcLDCProLutBin);
    destroyConfParser(&mConf);

    return SUCCESS;
}

static int alloc_frame_buffer(SampleEncppGdcOfflineContext *pContext, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    int result = 0;
    int pic_len = 0;

    pic_len = pFrameInfo->VFrame.mWidth*pFrameInfo->VFrame.mHeight;
    AW_MPI_SYS_MmzAlloc_Cached(&pFrameInfo->VFrame.mPhyAddr[0], \
        &pFrameInfo->VFrame.mpVirAddr[0], pic_len);
    if ((0 == pFrameInfo->VFrame.mPhyAddr[0]) || (NULL == pFrameInfo->VFrame.mpVirAddr[0]))
    {
        result = -1;
        aloge("fatal error! alloc src frame buffer fail!");
        return result;
    }
    AW_MPI_SYS_MmzAlloc_Cached(&pFrameInfo->VFrame.mPhyAddr[1], \
        &pFrameInfo->VFrame.mpVirAddr[1], pic_len/2);
    if ((0 == pFrameInfo->VFrame.mPhyAddr[1]) || (NULL == pFrameInfo->VFrame.mpVirAddr[1]))
    {
        result = -1;
        aloge("fatal error! alloc src frame buffer fail!");
        return result;
    }

    return result;
}

static int readPic(SampleEncppGdcOfflineContext *pContext, char *pic, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    int result = 0;
    FILE *fp = NULL;
    int pic_len = 0;

    fp = fopen(pic, "rb");
    if (NULL == fp)
    {
        aloge("fatal error! open file[%s] fail!", pic);
        goto _exit;
    }
    fseek(fp, 0, SEEK_SET);

    result = alloc_frame_buffer(pContext, pFrameInfo);
    if (result)
    {
        aloge("fatal error! alloc buffer fail!");
        goto _close_file;
    }
    pic_len = pContext->mConfigPara.mSrcWidth*pContext->mConfigPara.mSrcHeight;
    fread(pFrameInfo->VFrame.mpVirAddr[0], pic_len, 1, fp);
    AW_MPI_SYS_MmzFlushCache(pFrameInfo->VFrame.mPhyAddr[0], pFrameInfo->VFrame.mpVirAddr[0], pic_len);
    fread(pFrameInfo->VFrame.mpVirAddr[1], pic_len/2, 1, fp);
    AW_MPI_SYS_MmzFlushCache(pFrameInfo->VFrame.mPhyAddr[1], pFrameInfo->VFrame.mpVirAddr[1], pic_len/2);

_close_file:
    if (fp)
        fclose(fp);
_exit:
    return result;
}

static void save_dst_pic(SampleEncppGdcOfflineContext *pContext)
{
    FILE *fp = fopen(pContext->mConfigPara.mDstPic, "wb");
    if (fp)
    {
        int pic_len = pContext->mConfigPara.mDstWidth*pContext->mConfigPara.mDstHeight;
        AW_MPI_SYS_MmzFlushCache(pContext->mDstFrameInfo.VFrame.mPhyAddr[0], pContext->mDstFrameInfo.VFrame.mpVirAddr[0], pic_len);
        fwrite(pContext->mDstFrameInfo.VFrame.mpVirAddr[0], pic_len, 1, fp);
        AW_MPI_SYS_MmzFlushCache(pContext->mDstFrameInfo.VFrame.mPhyAddr[1], pContext->mDstFrameInfo.VFrame.mpVirAddr[1], pic_len/2);
        fwrite(pContext->mDstFrameInfo.VFrame.mpVirAddr[1], pic_len/2, 1, fp);
        fclose(fp);
    }
    else
    {
        aloge("fatal error! save dst pic[%s] fail!", pContext->mConfigPara.mDstPic);
    }
}

static void configGdcLDCParam(SampleEncppGdcOfflineContext *pContext, sGdcParam *pGdcParam)
{
    /* LDC test parameters use GC4663 lens parameters */
    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpType;
    pGdcParam->bMirror = pContext->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pContext->mConfigPara.mDstWidth;
    pGdcParam->calib_height = pContext->mConfigPara.mDstHeight;

    /* lens paramters */
    pGdcParam->fx = 1718.15f;
    pGdcParam->fy = 1713.46f;
    pGdcParam->cx = 1279.50f;
    pGdcParam->cy = 719.50f;
    pGdcParam->fx_scale = 1582.29f;
    pGdcParam->fy_scale = 1593.43f;
    pGdcParam->cx_scale = 1279.50f;
    pGdcParam->cy_scale = 719.50f;

    /* distortion parameters */
    pGdcParam->distCoef_wide_ra[0] = -0.427011f;
    pGdcParam->distCoef_wide_ra[1] = 0.196667f;
    pGdcParam->distCoef_wide_ra[2] = 0.000000f;
    pGdcParam->distCoef_wide_ta[0] = 0.000143f;
    pGdcParam->distCoef_wide_ta[1] = 0.002161f;
    pGdcParam->distCoef_fish_k[0] = 0.00;
    pGdcParam->distCoef_fish_k[1] = 0.00;
    pGdcParam->distCoef_fish_k[2] = 0.00;
    pGdcParam->distCoef_fish_k[3] = 0.00;

    /* LDC correction parameters */
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->centerOffsetX = 0;
    pGdcParam->centerOffsetY = 0;
    pGdcParam->rotateAngle = 0;
    pGdcParam->radialDistortCoef = 0;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->eLensDistModel = Gdc_DistModel_WideAngle;
    pGdcParam->eMountMode = pContext->mConfigPara.mGdcMountType;
}

static void configGdcLDCProParam(SampleEncppGdcOfflineContext *pContext, sGdcParam *pGdcParam)
{
    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpType;
    pGdcParam->bMirror = pContext->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pContext->mConfigPara.mDstWidth;
    pGdcParam->calib_height = pContext->mConfigPara.mDstHeight;

    pGdcParam->lut_data_buf = pContext->mpGdcLdcProLutData;
    pGdcParam->lut_data_size = pContext->mGdcLdcProLutDataLen;
    pGdcParam->eMountMode = pContext->mConfigPara.mGdcMountType;
}

static void configGdcPano180Param(SampleEncppGdcOfflineContext *pContext, sGdcParam *pGdcParam)
{
    /* only support mount wall */
    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpType;
    pGdcParam->bMirror = pContext->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pContext->mConfigPara.mDstWidth;
    pGdcParam->calib_height = pContext->mConfigPara.mDstHeight;

    /* lens paramters */
    pGdcParam->cx = 1024;
    pGdcParam->cx = 1024;
    pGdcParam->distCoef_fish_k[0] = 652;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    /* Pano180 correction parameters */
    pGdcParam->pan = 0;
    pGdcParam->tilt = 0;
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->radialDistortCoef = 0;
    pGdcParam->fanDistortCoef = 0;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->roll = 0;
    pGdcParam->pitch = 0;
    pGdcParam->yaw = 0;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
}

static void configGdcPano360Param(SampleEncppGdcOfflineContext *pContext, sGdcParam *pGdcParam)
{
    if (Gdc_Mount_Wall == pContext->mConfigPara.mGdcMountType)
    {
        alogw("pano 180 don't support mount wall");
    }
    alogd("configGdcPano360Param");

    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpType;
    pGdcParam->bMirror = pContext->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pContext->mConfigPara.mDstWidth;
    pGdcParam->calib_height = pContext->mConfigPara.mDstHeight;

    /* lens paramters */
    pGdcParam->cx = 960;
    pGdcParam->cx = 960;
    pGdcParam->distCoef_fish_k[0] = 611;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    /* Pano360 correction parameters */
    pGdcParam->pan = 0;
    pGdcParam->tilt = 0;
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->scale = 100;
    pGdcParam->innerRadius = 0;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->roll = 0;
    pGdcParam->pitch = 0;
    pGdcParam->yaw = 0;
    pGdcParam->eMountMode = pContext->mConfigPara.mGdcMountType;
}

static void configGdcNormalParam(SampleEncppGdcOfflineContext *pContext, sGdcParam *pGdcParam)
{
    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpType;
    pGdcParam->bMirror = pContext->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pContext->mConfigPara.mDstWidth;
    pGdcParam->calib_height = pContext->mConfigPara.mDstHeight;

    /* lens paramters */
    pGdcParam->cx = 1024;
    pGdcParam->cx = 1024;
    pGdcParam->innerRadius = 1024;
    pGdcParam->distCoef_fish_k[0] = 652;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    /* Normal correction parameters */
    pGdcParam->pan = 0;
    pGdcParam->tilt = 0;
    pGdcParam->zoomH = 50;
    pGdcParam->scale = 100;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->eMountMode = pContext->mConfigPara.mGdcMountType;
}

static void configGdcFish2WideParam(SampleEncppGdcOfflineContext *pContext, sGdcParam *pGdcParam)
{
    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpType;
    pGdcParam->bMirror = pContext->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pContext->mConfigPara.mDstWidth;
    pGdcParam->calib_height = pContext->mConfigPara.mDstHeight;

    /* lens paramters */
    pGdcParam->cx = 1024;
    pGdcParam->cx = 1024;
    pGdcParam->innerRadius = 1024;
    pGdcParam->distCoef_fish_k[0] = 652;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    /* Normal correction parameters */
    pGdcParam->pan = 0;
    pGdcParam->tilt = 0;
    pGdcParam->zoomH = 100;
    pGdcParam->roll = 0;
    pGdcParam->pitch = 0;
    pGdcParam->yaw = 0;
    pGdcParam->eMountMode = pContext->mConfigPara.mGdcMountType;
}

static void configGdcPerspectiveParam(SampleEncppGdcOfflineContext *pContext, sGdcParam *pGdcParam)
{
    /* LDC test parameters use GC4663 lens parameters */
    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpType;
    pGdcParam->bMirror = pContext->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = pContext->mConfigPara.mDstWidth;
    pGdcParam->calib_height = pContext->mConfigPara.mDstHeight;

    /* lens paramters */
    pGdcParam->fx = 1718.15f;
    pGdcParam->fy = 1713.46f;
    pGdcParam->cx = 1279.50f;
    pGdcParam->cy = 719.50f;
    pGdcParam->fx_scale = 1582.29f;
    pGdcParam->fy_scale = 1593.43f;
    pGdcParam->cx_scale = 1279.50f;
    pGdcParam->cy_scale = 719.50f;

    /* distortion parameters */
    pGdcParam->distCoef_wide_ra[0] = -0.427011f;
    pGdcParam->distCoef_wide_ra[1] = 0.196667f;
    pGdcParam->distCoef_wide_ra[2] = 0.000000f;
    pGdcParam->distCoef_wide_ta[0] = 0.000143f;
    pGdcParam->distCoef_wide_ta[1] = 0.002161f;
    pGdcParam->distCoef_fish_k[0] = 0.00;
    pGdcParam->distCoef_fish_k[1] = 0.00;
    pGdcParam->distCoef_fish_k[2] = 0.00;
    pGdcParam->distCoef_fish_k[3] = 0.00;

    /* LDC correction parameters */
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->centerOffsetX = 0;
    pGdcParam->centerOffsetY = 0;
    pGdcParam->rotateAngle = 0;
    pGdcParam->radialDistortCoef = 0;
    pGdcParam->trapezoidDistortCoef = 0;
    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;
    pGdcParam->eMountMode = pContext->mConfigPara.mGdcMountType;

    /* Perspective parameters */
    pGdcParam->perspectiveProjMat[0] = 0.8f;
    pGdcParam->perspectiveProjMat[1] = 0.1f;
    pGdcParam->perspectiveProjMat[2] = 7.0f;
    pGdcParam->perspectiveProjMat[3] = 0.01f;
    pGdcParam->perspectiveProjMat[4] = 0.87;
    pGdcParam->perspectiveProjMat[5] = 5.0f;
    pGdcParam->perspectiveProjMat[6] = 0.0f;
    pGdcParam->perspectiveProjMat[7] = 0.0;
    pGdcParam->perspectiveProjMat[8] = 1.0f;
    pGdcParam->perspFunc = Gdc_Persp_LDC;
}

static void configGdcBirdsEyeParam(SampleEncppGdcOfflineContext *pContext, sGdcParam *pGdcParam)
{
    pGdcParam->eWarpMode = pContext->mConfigPara.mGdcWarpType;
    pGdcParam->bMirror = pContext->mConfigPara.mbGdcMirror?1:0;
    pGdcParam->calib_widht = 1920;
    pGdcParam->calib_height = 1080;
    pGdcParam->birdsImg_width = pContext->mConfigPara.mDstWidth;
    pGdcParam->birdsImg_height = pContext->mConfigPara.mDstHeight;

    /* need set LDC parameters, this just show how to set birds eyeee paramters */
    /* lens paramters */
    pGdcParam->fx = 1891.85f / 2.0f;
    pGdcParam->fy = 1891.85f / 2.0f;
    pGdcParam->cx = 1928.20f / 2.0f;
    pGdcParam->cy = 1115.24f / 2.0f;
    pGdcParam->fx_scale = 1899.46f / 2.0f;
    pGdcParam->fy_scale = 1899.46f / 2.0f;
    pGdcParam->cx_scale = 1919.50f / 2.0f;
    pGdcParam->cy_scale = 1919.50f / 2.0f;

    /* distortion parameters */
    pGdcParam->distCoef_wide_ra[0] = -0.3560;
    pGdcParam->distCoef_wide_ra[1] = 0.1886f;
    pGdcParam->distCoef_wide_ra[2] = -0.0656f;
    pGdcParam->distCoef_wide_ta[0] = 0.00025f;
    pGdcParam->distCoef_wide_ta[1] = 0.00006f;
    pGdcParam->distCoef_fish_k[0] = -0.246f;
    pGdcParam->distCoef_fish_k[1] = -0.164f;
    pGdcParam->distCoef_fish_k[2] = 0.0207f;
    pGdcParam->distCoef_fish_k[3] = -0.0074f;

    /* BirdsEye correction parameters */
    pGdcParam->centerOffsetX = 0;
    pGdcParam->centerOffsetY = -100;
    pGdcParam->zoomH = 100;
    pGdcParam->zoomV = 100;
    pGdcParam->mountHeight = 0.85;
    pGdcParam->roll = -21;
    pGdcParam->pitch = 0;
    pGdcParam->yaw = 0;
    pGdcParam->roiDist_ahead = 4.5f;
    pGdcParam->roiDist_left = -1.5f;
    pGdcParam->roiDist_right = 1.5f;
    pGdcParam->roiDist_bottom = 0.65f;
    pGdcParam->eMountMode = pContext->mConfigPara.mGdcMountType;
}

static void configGdcParam(SampleEncppGdcOfflineContext *pContext, sGdcParam *pGdcParam)
{
    switch (pContext->mConfigPara.mGdcWarpType)
    {
        case Gdc_Warp_LDC:
            configGdcLDCParam(pContext, pGdcParam);
            break;
        case Gdc_Warp_LDC_Pro:
            configGdcLDCProParam(pContext, pGdcParam);
            break;
        case Gdc_Warp_Pano180:
            configGdcPano180Param(pContext, pGdcParam);
            break;
        case Gdc_Warp_Pano360:
            configGdcPano360Param(pContext, pGdcParam);
            break;
        case Gdc_Warp_Normal:
            configGdcNormalParam(pContext, pGdcParam);
            break;
        case Gdc_Warp_Fish2Wide:
            configGdcFish2WideParam(pContext, pGdcParam);
            break;
        case Gdc_Warp_Perspective:
            configGdcPerspectiveParam(pContext, pGdcParam);
            break;
        case Gdc_Warp_BirdsEye:
            configGdcBirdsEyeParam(pContext, pGdcParam);
            break;
        default:
            aloge("fatal error! unsupport gdc warp mode[%d] disbale gdc!", pContext->mConfigPara.mGdcWarpType);
            break;
    }
}


static int EncppGdcOffline_task_proc(SampleEncppGdcOfflineContext *pContext)
{
    int result = 0;
    VideoEncoderEncpp *pEncpp = NULL;
    VeOpsS *pVeOpsS = NULL;
    void *pVeOpsSelf = NULL;
    struct ScMemOpsS *memops = NULL;

    memops = MemAdapterGetOpsS();
    if (NULL == memops)
    {
        result = -1;
        aloge("fatal error! MemAdapterGetOpsS fail!");
        return result;
    }
    CdcMemOpen(memops);

    int type = VE_OPS_TYPE_NORMAL;
    pVeOpsS = GetVeOpsS(type);
    if (NULL == pVeOpsS)
    {
        result = -1;
        aloge("fatal error! get ve ops fail!");
        goto _mem_close;
    }

    VeConfig mVeConfig;
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 0;
    mVeConfig.nEncoderFlag = 1;
    mVeConfig.nEnableAfbcFlag = 0;
    mVeConfig.nFormat = 0;
    mVeConfig.nWidth = 0;
    mVeConfig.nResetVeMode = 0;
    pVeOpsSelf = CdcVeInit(pVeOpsS, &mVeConfig);
    if(pVeOpsSelf == NULL)
    {
        result = -1;
        aloge("init ve ops failed");
        goto _mem_close;
    }
    pEncpp = VencEncppCreate(1);
    if(pEncpp == NULL)
    {
        aloge("VideoEncIspCreate failed");
        result = -1;
        goto _release_ve;
    }

    VencEncppBufferInfo mInBuffer;
    memset(&mInBuffer, 0, sizeof(VencEncppBufferInfo));
    if (MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pContext->mConfigPara.mSrcFmt)
        mInBuffer.colorFormat = VENC_PIXEL_YVU420SP;
    else if (MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 == pContext->mConfigPara.mSrcFmt)
        mInBuffer.colorFormat = VENC_PIXEL_YUV420SP;
    else
        aloge("fatal error! unsupport pix fmt[%d]", pContext->mConfigPara.mSrcFmt);
    mInBuffer.nWidth = AWALIGN(pContext->mConfigPara.mSrcWidth, 16);
    mInBuffer.nHeight = AWALIGN(pContext->mConfigPara.mSrcHeight, 16);
    mInBuffer.nStride = mInBuffer.nWidth;
    mInBuffer.pAddrVirY = (unsigned char *)(pContext->mSrcFrameInfo.VFrame.mpVirAddr[0]);
    mInBuffer.pAddrPhyY = (unsigned char *)(pContext->mSrcFrameInfo.VFrame.mPhyAddr[0]);
    mInBuffer.pAddrVirC = (unsigned char *)(pContext->mSrcFrameInfo.VFrame.mpVirAddr[1]);
    mInBuffer.pAddrPhyC0 = (unsigned char *)(pContext->mSrcFrameInfo.VFrame.mPhyAddr[1]);
    mInBuffer.pAddrPhyC1 = (unsigned char *)(pContext->mSrcFrameInfo.VFrame.mPhyAddr[1]+\
        pContext->mConfigPara.mSrcWidth*pContext->mConfigPara.mSrcHeight/4);

    VencEncppBufferInfo mOutBuffer;
    memset(&mOutBuffer, 0, sizeof(VencEncppBufferInfo));
    if (MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pContext->mConfigPara.mDstFmt)
        mOutBuffer.colorFormat = VENC_PIXEL_YVU420SP;
    else if (MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420 == pContext->mConfigPara.mDstFmt)
        mOutBuffer.colorFormat = VENC_PIXEL_YUV420SP;
    else
        aloge("fatal error! unsupport pix fmt[%d]", pContext->mConfigPara.mDstFmt);
    mOutBuffer.nWidth = AWALIGN(pContext->mConfigPara.mDstWidth, 16);
    mOutBuffer.nHeight = AWALIGN(pContext->mConfigPara.mDstHeight, 16);
    mOutBuffer.nStride = mOutBuffer.nWidth;
    mOutBuffer.pAddrVirY = (unsigned char *)(pContext->mDstFrameInfo.VFrame.mpVirAddr[0]);
    mOutBuffer.pAddrPhyY = (unsigned char *)(pContext->mDstFrameInfo.VFrame.mPhyAddr[0]);
    mOutBuffer.pAddrVirC = (unsigned char *)(pContext->mDstFrameInfo.VFrame.mpVirAddr[1]);
    mOutBuffer.pAddrPhyC0 = (unsigned char *)(pContext->mDstFrameInfo.VFrame.mPhyAddr[1]);
    mOutBuffer.pAddrPhyC1 = (unsigned char *)(pContext->mDstFrameInfo.VFrame.mPhyAddr[1]+\
        pContext->mConfigPara.mDstWidth*pContext->mConfigPara.mDstHeight/4);

    sGdcParam stGdcParam;
    memset(&stGdcParam, 0, sizeof(sGdcParam));
    stGdcParam.bGDC_en = 1;
    configGdcParam(pContext, &stGdcParam);

    VencEncppFuncParam mIspFunction;
    memset(&mIspFunction, 0, sizeof(VencEncppFuncParam));
    mIspFunction.bEnableGdcFlag = 1;
    mIspFunction.pGdcParam = &stGdcParam;
    result = VencEncppFunction(pEncpp, &mInBuffer, &mOutBuffer, &mIspFunction);
    if (result)
    {
        aloge("fatal error! VencEncppFunction fail!");
    }

_destroy_encpp:
    if (pEncpp)
        VencEncppDestroy(pEncpp);
_release_ve:
    if (pVeOpsS)
        CdcVeRelease(pVeOpsS, pVeOpsSelf);
_mem_close:
    if (memops)
        CdcMemClose(memops);
_exit:
    return result;
}

int main(int argc, char *argv[])
{
    int result = 0;
    SampleEncppGdcOfflineContext stContext;
    SampleEncppGdcOfflineContext *pContext = NULL;

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

    memset(&stContext, 0, sizeof(SampleEncppGdcOfflineContext));
    pContext = &stContext;
    if (parseCmdLine(pContext, argc, argv) != 0)
    {
        result = -1;
        goto _exit;
    }

    if (loadConfigPara(pContext, pContext->mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        result = -1;
        aloge("somthing wrong in loading conf file");
        goto _exit;
    }

    if (Gdc_Warp_LDC_Pro == pContext->mConfigPara.mGdcWarpType)
    {
        BOOL b_read_success = TRUE;
        FILE *fp = fopen(pContext->mConfigPara.mGdcLDCProLutBin, "rb");
        if (NULL == fp)
        {
            b_read_success = FALSE;
            aloge("fatal error! LDC Pro lut bin[%s] open fail!", pContext->mConfigPara.mGdcLDCProLutBin);
        }
        else
        {
            fseek(fp, 0, SEEK_END);
            pContext->mGdcLdcProLutDataLen = ftell(fp);
            fseek(fp, 0, SEEK_SET);
            pContext->mpGdcLdcProLutData = malloc(pContext->mGdcLdcProLutDataLen);
            if (NULL == pContext->mpGdcLdcProLutData)
            {
                b_read_success = FALSE;
                aloge("fatal error! malloc lut buffer fail! lut len[%d]", pContext->mGdcLdcProLutDataLen);
            }
            else
            {
                memset(pContext->mpGdcLdcProLutData, 0, pContext->mGdcLdcProLutDataLen);
                fread(pContext->mpGdcLdcProLutData, pContext->mGdcLdcProLutDataLen, 1, fp);
            }
            fclose(fp);
            fp = NULL;
            if (!b_read_success)
                goto _exit;
        }
    }

    MPP_SYS_CONF_S stSysConf;
    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);
    AW_MPI_SYS_Init();

    memset(&pContext->mSrcFrameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));
    pContext->mSrcFrameInfo.VFrame.mWidth = pContext->mConfigPara.mSrcWidth;
    pContext->mSrcFrameInfo.VFrame.mHeight = pContext->mConfigPara.mSrcHeight;
    pContext->mSrcFrameInfo.VFrame.mPixelFormat = pContext->mConfigPara.mSrcFmt;
    result = readPic(pContext, pContext->mConfigPara.mSrcPic, &pContext->mSrcFrameInfo);
    if (result)
    {
        aloge("fatal error! read pic[%d] fail!", pContext->mConfigPara.mSrcPic);
        goto _free_buffer;
    }
    memset(&pContext->mDstFrameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));
    pContext->mDstFrameInfo.VFrame.mWidth = pContext->mConfigPara.mDstWidth;
    pContext->mDstFrameInfo.VFrame.mHeight = pContext->mConfigPara.mDstHeight;
    pContext->mDstFrameInfo.VFrame.mPixelFormat = pContext->mConfigPara.mDstFmt;
    result = alloc_frame_buffer(pContext, &pContext->mDstFrameInfo);
    if (result)
    {
        aloge("fatal error! alloc buffer fail!");
        goto _free_buffer;
    }

    result = EncppGdcOffline_task_proc(pContext);
    if (result)
    {
        goto _free_buffer;
    }

    save_dst_pic(pContext);

_free_buffer:
    if ((0 != pContext->mDstFrameInfo.VFrame.mPhyAddr[0]) && (NULL != pContext->mDstFrameInfo.VFrame.mpVirAddr[0]))
    {
        AW_MPI_SYS_MmzFree(pContext->mDstFrameInfo.VFrame.mPhyAddr[0], pContext->mDstFrameInfo.VFrame.mpVirAddr[0]);
    }
    if ((0 != pContext->mSrcFrameInfo.VFrame.mPhyAddr[0]) && (NULL != pContext->mSrcFrameInfo.VFrame.mpVirAddr[0]))
    {
        AW_MPI_SYS_MmzFree(pContext->mSrcFrameInfo.VFrame.mPhyAddr[0], pContext->mSrcFrameInfo.VFrame.mpVirAddr[0]);
    }
    AW_MPI_SYS_Exit();
_exit:
    if (pContext->mpGdcLdcProLutData)
    {
        free(pContext->mpGdcLdcProLutData);
        pContext->mpGdcLdcProLutData = NULL;
    }
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
