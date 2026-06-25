
/*
 ******************************************************************************
 *
 * MPI_ISP.h
 *
 * Hawkview ISP - mpi_isp.h module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version		  Author         Date		    Description
 *
 *   1.0		  yuanxianfeng   2016/04/01	    ISP
 *
 *****************************************************************************
 */
 //#define LOG_NDEBUG
#define LOG_TAG "mpi_isp"
#include <utils/plat_log.h>

#include <stdio.h>
#include <unistd.h>
#include "cdx_list.h"

#include "mpi_isp.h"
#include "isp_tuning.h"
#include "tmessage.h"
#include "ComponentCommon.h"
#include "videoIn/TDMRawProcessForTools.h"

#define MEDIA_DEVICE_0 "/dev/media0"
#define MEDIA_DEVICE_1 "/dev/media1"

/* ======================================= */
/* function list */
/* ======================================= */
#if 0
AW_S32 AW_MPI_ISP_SetRegister(ISP_DEV IspDev, AW_U32 u32Addr, AW_U32 u32Value);
AW_S32 AW_MPI_ISP_GetRegister(ISP_DEV IspDev, AW_U32 u32Addr, AW_U32 *pu32Value);
//AW_S32 AW_MPI_ISP_SetFlash(ISP_DEV IspDev,  ISP_FLASH_S *pFlash);
//AW_S32 AW_MPI_ISP_GetFlash(ISP_DEV IspDev, ISP_FLASH_S *pFlash);
//AW_S32 AW_MPI_ISP_SetFlicker(ISP_DEV IspDev,  ISP_FLICKER_S *pFlicker);
//AW_S32 AW_MPI_ISP_GetFlicker(ISP_DEV IspDev, ISP_FLICKER_S *pFlicker);
AW_S32 AW_MPI_ISP_SetDeFogAttr(ISP_DEV IspDev,  ISP_DEFOG_ATTR_S *pstDefogAttr);
AW_S32 AW_MPI_ISP_GetDeFogAttr(ISP_DEV IspDev, ISP_DEFOG_ATTR_S *pstDefogAttr);
AW_S32 AW_MPI_ISP_SetDPC(ISP_DEV IspDev, ISP_OTF *pDpcOtf);
AW_S32 AW_MPI_ISP_GetDPC(ISP_DEV IspDev, ISP_OTF *pDpcOtf);
AW_S32 AW_MPI_ISP_SetBlackLevel(ISP_DEV IspDev,  ISP_BLACK_LEVEL_S *pstBlackLevel);
AW_S32 AW_MPI_ISP_GetBlackLevel(ISP_DEV IspDev, ISP_BLACK_LEVEL_S *pstBlackLevel);
AW_S32 AW_MPI_ISP_SetShadingAttr(ISP_DEV IspDev,  ISP_SHADING_ATTR_S *pstShadingAttr);
AW_S32 AW_MPI_ISP_GetShadingAttr(ISP_DEV IspDev, ISP_SHADING_ATTR_S *pstShadingAttr);
AW_S32 AW_MPI_ISP_SetGamma(ISP_DEV IspDev,  ISP_GAMMA_ATTR_S *pstGammaAttr);
AW_S32 AW_MPI_ISP_GetGamma(ISP_DEV IspDev, ISP_GAMMA_ATTR_S *pstGammaAttr);
AW_S32 AW_MPI_ISP_SetLinearity(ISP_DEV IspDev,  ISP_LINEARITY_S *pLinearity);
AW_S32 AW_MPI_ISP_GetLinearity(ISP_DEV IspDev, ISP_LINEARITY_S *pLinearity);
AW_S32 AW_MPI_ISP_SetDistortion(ISP_DEV IspDev,  ISP_DISTORTION_S *pDistortion);
AW_S32 AW_MPI_ISP_GetDistortion(ISP_DEV IspDev, ISP_DISTORTION_S *pDistortion);
// AW_S32 AW_MPI_ISP_SetTrigger(ISP_DEV IspDev, ISP_TRIGER_S *pTriger);
// AW_S32 AW_MPI_ISP_GetTrigger(ISP_DEV IspDev, ISP_TRIGER_S *pTriger);
// AW_S32 AW_MPI_ISP_SetLumMapPoint(ISP_DEV IspDev, ISP_SINGLE_S *plumP);
// AW_S32 AW_MPI_ISP_GetLumMapPoint(ISP_DEV IspDev, ISP_SINGLE_S *plumP);
// AW_S32 AW_MPI_ISP_SetGainMapPoint(ISP_DEV IspDev, ISP_SINGLE_S *pGainP);
// AW_S32 AW_MPI_ISP_GetGainMapPoint(ISP_DEV IspDev, ISP_SINGLE_S *pGainP);
AW_S32 AW_MPI_ISP_AWB_SetCCMAttr(ISP_DEV IspDev, int color_temp, ISP_COLORMATRIX_ATTR_S *pstCCMAttr);
AW_S32 AW_MPI_ISP_AWB_GetCCMAttr(ISP_DEV IspDev, int color_temp, ISP_COLORMATRIX_ATTR_S *pstCCMAttr);
/*
AW_S32 AW_MPI_ISP_SetBrightness(ISP_DEV IspDev, ISP_BRIGHTNESS_S *pBrightness);
AW_S32 AW_MPI_ISP_GetBrightness(ISP_DEV IspDev, ISP_BRIGHTNESS_S *pBrightness);
AW_S32 AW_MPI_ISP_SetContrast(ISP_DEV IspDev, ISP_CONTRAST_S *pContrast);
AW_S32 AW_MPI_ISP_GetContrast(ISP_DEV IspDev, ISP_CONTRAST_S *pContrast);
AW_S32 AW_MPI_ISP_SetSaturation(ISP_DEV IspDev, ISP_SATURATION_S *pSta);
AW_S32 AW_MPI_ISP_GetSaturation(ISP_DEV IspDev, ISP_SATURATION_S *pSta);
AW_S32 AW_MPI_ISP_SetHue(ISP_DEV IspDev, ISP_HUE_S *pHue);
AW_S32 AW_MPI_ISP_GetHue(ISP_DEV IspDev, ISP_HUE_S *pHue);
*/
AW_S32 AW_MPI_ISP_SetSharpness(ISP_DEV IspDev,  ISP_SHARPEN_ATTR_S *pstSharpenAttr);
AW_S32 AW_MPI_ISP_GetSharpness(ISP_DEV IspDev, ISP_SHARPEN_ATTR_S *pstSharpenAttr);
AW_S32 AW_MPI_ISP_SetNRAttr(ISP_DEV IspDev,  ISP_NR_ATTR_S *pstNRAttr);
AW_S32 AW_MPI_ISP_GetNRAttr(ISP_DEV IspDev, ISP_NR_ATTR_S *pstNRAttr);
AW_S32 AW_MPI_ISP_Set3NRAttr(ISP_DEV IspDev, ISP_3NR_ATTR_S *pTdf);
AW_S32 AW_MPI_ISP_Get3NRAttr(ISP_DEV IspDev, ISP_3NR_ATTR_S *pTdf);
AW_S32 AW_MPI_ISP_SetVisualAngle(ISP_DEV IspDev,  ISP_VISUAL_ANGLE_S *pVisualAngle);
AW_S32 AW_MPI_ISP_GetVisualAngle(ISP_DEV IspDev, ISP_VISUAL_ANGLE_S *pVisualAngle);
// AW_S32 AW_MPI_ISP_SetAe(ISP_DEV IspDev, ISP_AE_S *pAe);
// AW_S32 AW_MPI_ISP_GetAe(ISP_DEV IspDev, ISP_AE_S *pAe);
AW_S32 AW_MPI_ISP_SetDynamicGtm(ISP_DEV IspDev, ISP_DYNAMIC_GTM_S *pGtm);
AW_S32 AW_MPI_ISP_GetDynamicGtm(ISP_DEV IspDev, ISP_DYNAMIC_GTM_S *pGtm);
AW_S32 AW_MPI_ISP_SetDynamicPltm(ISP_DEV IspDev, ISP_DYNAMIC_PLTM_S *pPltm);
AW_S32 AW_MPI_ISP_GetDynamicPltm(ISP_DEV IspDev, ISP_DYNAMIC_PLTM_S *pPltm);
// AW_S32 AW_MPI_ISP_SetGtm(ISP_DEV IspDev, ISP_GTM_S *pGtm);
// AW_S32 AW_MPI_ISP_GetGtm(ISP_DEV IspDev, ISP_GTM_S *pGtm);
#endif

#define ConfigIspRunFilePath "/mnt/extsd/ConfigIspRun"

#ifdef clamp
#undef clamp
#endif
#define clamp(val, min, max) ({			\
	typeof(val) __val = (val);		\
	typeof(min) __min = (min);		\
	typeof(max) __max = (max);		\
	__val = __val < __min ? __min : __val;	\
	__val > __max ? __max : __val;		\
})

typedef struct IspCtx
{
    int nIspDev;
    int nUseCnt;
#if (MPPCFG_SUPPORT_FASTBOOT == 0 || MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW == 1)
    pthread_t TdmRawThread;
    message_queue_t msg_queue;
#endif
    struct list_head mList;
}IspCtx;

typedef struct IspManager
{
    pthread_mutex_t mLock;
    struct list_head mIspCtxList;   //element type: IspCtx
    int bIspRun;
} IspManager;

static IspManager *gpIspManager = NULL;

ERRORTYPE AW_MPI_ISP_Init()
{
    if(gpIspManager)
    {
        alogd("IspManager already construct!");
        return SUCCESS;
    }
    int ret;
    gpIspManager = (IspManager*)malloc(sizeof(IspManager));
    if (NULL == gpIspManager)
    {
        aloge("alloc IspManager error(%s)!", strerror(errno));
        return FAILURE;
    }
    memset(gpIspManager, 0, sizeof(IspManager));
    ret = pthread_mutex_init(&gpIspManager->mLock, NULL);
    if (ret != 0)
    {
        aloge("fatal error! mutex init fail");
        free(gpIspManager);
        gpIspManager = NULL;
        return FAILURE;
    }
    INIT_LIST_HEAD(&gpIspManager->mIspCtxList);

#if MPPCFG_SUPPORT_FASTBOOT
    gpIspManager->bIspRun = true;
    return 0;
#else
    gpIspManager->bIspRun = true;
    char strIspRun[8];
    int nIspRun = 0;
    FILE *pConfigFile = fopen(ConfigIspRunFilePath, "r");
    if(pConfigFile != NULL)
    {
        if(fgets(strIspRun, 8, pConfigFile) != NULL)
        {
            nIspRun = strtol(strIspRun, NULL, 0);
            if(0 == nIspRun)
            {
                gpIspManager->bIspRun = false;
            }
        }
        fclose(pConfigFile);
    }
    if(gpIspManager->bIspRun)
    {
        ret = media_dev_init();
        if(ret != 0)
        {
            aloge("fatal error! media_dev init fail[%d]", ret);
        }
        return SUCCESS;
    }
    else
    {
        alogd("ConfigFile forbids to run isp, [%s] config ispRun:%d", ConfigIspRunFilePath, gpIspManager->bIspRun);
        return SUCCESS;
    }
#endif
}

/**
  set AE mode before isp_init().
*/
ERRORTYPE AW_MPI_ISP_Init_AE_SetMode(ISP_DEV IspDev, int mode)
{
    ERRORTYPE eRet = SUCCESS;
    int value;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if (0 == mode) // auto ae
    {
        value = 0;
        isp_set_initial_cfg(IspDev, ISP_INITIAL_EXPOSURE_AUTO, &value);
        value = 1;
        isp_set_initial_cfg(IspDev, ISP_INITIAL_AUTOGAIN, &value);
    }
    else if (1 == mode)
    {
        value = 1;
        isp_set_initial_cfg(IspDev, ISP_INITIAL_EXPOSURE_AUTO, &value);
        value = 0;
        isp_set_initial_cfg(IspDev, ISP_INITIAL_AUTOGAIN, &value);
    }
    else
    {
        aloge("fatal error! isp[%d] ae mode wrong value[%d]", IspDev, mode);
        eRet = ERR_ISP_ILLEGAL_PARAM;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_AE_SetExposureBias(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if ((Value < 0) || (Value > 8))
    {
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_AUTO_EXPOSURE_BIAS, &Value);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial AUTO_EXPOSURE_BIAS [%d] fail", IspDev, Value);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_AE_SetExposure(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_EXPOSURE_ABSOLUTE, &Value);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial EXPOSURE_ABSOLUTE [%d] fail", IspDev, Value);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_AE_SetGain(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_GAIN, &Value);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial GAIN [%d] fail", IspDev, Value);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_AE_SetISOSensitive(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet = SUCCESS;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }

    if (Value < 0 || Value > 7)
    {
        aloge("fatal error! isp[%d] ISOSensitive value range should be [1~7], value(%d)", IspDev, Value);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if (0 == Value) // auto mode
    {
        int nMode = 1;
        ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_ISO_SENSITIVITY_AUTO, (void*)&nMode);
        if (ret != 0)
        {
            aloge("fatal error! isp[%d] set initial ISO_SENSITIVITY_AUTO [%d] fail[%d]", IspDev, nMode, ret);
            eRet = FAILURE;
        }
    }
    else //manual mode
    {
        int nMode = 0;
        ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_ISO_SENSITIVITY_AUTO, (void*)&nMode);
        if (ret != 0)
        {
            aloge("fatal error! isp[%d] set initial ISO_SENSITIVITY_AUTO [%d] fail[%d]", IspDev, nMode, ret);
            eRet = FAILURE;
        }
        int nSensitivityIndex = Value - 1;
        ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_ISO_SENSITIVITY, (void*)&nSensitivityIndex);
        if (ret != 0)
        {
            aloge("fatal error! isp[%d] set initial ISO_SENSITIVITY [%d] fail[%d]", IspDev, nSensitivityIndex, ret);
            eRet = FAILURE;
        }
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_AE_SetMetering(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet = SUCCESS;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }

    if ((Value < 0) || (Value > 3))
    {
        aloge("fatal error! isp[%d] set initial AE_SetMetering out of range, you shoule use [0~3], value(%d)", Value);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_EXPOSURE_METERING, (void*)&Value);
    if (ret != 0)
    {
        aloge("fatal error! isp[%d] set initial AE_SetMetering [%d] fail[%d]", IspDev, Value, ret);
        eRet = FAILURE;
    }
    return eRet;
}

/**
  @param Value ref to v4l2_auto_n_preset_white_balance but not same.
*/
ERRORTYPE AW_MPI_ISP_Init_AWB_SetMode(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if (Value < 0 || Value > 9)
    {
        aloge("fatal error! isp[%d] Please use <0~9> to set awb mode, value = [%d]", IspDev, Value);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    int AWBMode;
    if (0 == Value) //auto awb
    {
        AWBMode = 1;
    }
    else if (1 == Value) //manual awb
    {
        AWBMode = 0;
    }
    else
    {
        AWBMode = Value;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_AUTO_N_PRESET_WHITE_BALANCE, &AWBMode);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial AUTO_N_PRESET_WHITE_BALANCE [%d] fail", IspDev, AWBMode);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_AWB_SetGain(ISP_DEV IspDev, struct isp_wb_gain *pIspWbGain)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if(NULL == pIspWbGain)
    {
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_WB_MGAIN, pIspWbGain);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial WB_MGAIN [%d-%d-%d-%d] fail", IspDev, pIspWbGain->r_gain, pIspWbGain->gr_gain,
            pIspWbGain->gb_gain, pIspWbGain->b_gain);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_SetFlicker(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if (Value < 0 || Value > 3)
    {
        aloge("fatal error! isp[%d] value[%d] illegal", IspDev, Value);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_POWER_LINE_FREQUENCY, &Value);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial POWER_LINE_FREQUENCY[%d] fail", IspDev, Value);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_SetBrightness(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if (Value < 0 || Value > 512)
    {
        aloge("fatal error! isp[%d] value[%d] illegal", IspDev, Value);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_BRIGHTNESS, &Value);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial BRIGHTNESS[%d] fail", IspDev, Value);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_SetContrast(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if (Value < 0 || Value > 512)
    {
        aloge("fatal error! isp[%d] value[%d] illegal", IspDev, Value);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_CONTRAST, &Value);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial CONTRAST[%d] fail", IspDev, Value);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_SetSaturation(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if (Value < 0 || Value > 768)
    {
        aloge("fatal error! isp[%d] value[%d] illegal", IspDev, Value);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_SATURATION, &Value);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial CONTRAST[%d] fail", IspDev, Value);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_SetSharpness(ISP_DEV IspDev, int Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    if (Value < 1 || Value > 4095)
    {
        aloge("fatal error! isp[%d] value[%d] illegal", IspDev, Value);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_SHARPNESS, &Value);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial SHARPNESS[%d] fail", IspDev, Value);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_SetScene(ISP_DEV IspDev, enum ae_table_mode Value)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_SCENE_MODE, &Value);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial SCENE_MODE[%d] fail", IspDev, Value);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_SetColorEffect(ISP_DEV IspDev, enum colorfx ColorStyle)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_COLOR_EFFECT, &ColorStyle);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial COLOR_EFFECT[%d] fail", IspDev, ColorStyle);
        eRet = FAILURE;
    }
    return eRet;
}

ERRORTYPE AW_MPI_ISP_Init_SetSpecialScene(ISP_DEV IspDev, scene_mode_t eTargetScene)
{
    ERRORTYPE eRet;
    int ret;
    if ((IspDev < 0) || (IspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! isp[%d] number is invalid!", IspDev);
        return ERR_ISP_ILLEGAL_PARAM;
    }
    ret = isp_set_initial_cfg(IspDev, ISP_INITIAL_AI_SCENE, &eTargetScene);
    if(0 == ret)
    {
        eRet = SUCCESS;
    }
    else
    {
        aloge("fatal error! isp[%d] set initial AI_SCENE[%d] fail", IspDev, eTargetScene);
        eRet = FAILURE;
    }
    return eRet;
}

AW_S32 AW_MPI_ISP_Run(ISP_DEV IspDev)
{
    AW_S32 eErr;
    if(false == gpIspManager->bIspRun)
    {
        alogd("ConfigFile forbids to run isp.");
        return SUCCESS;
    }
    eErr = isp_init(IspDev); // 0, 1
    if (eErr == EN_ERR_EFUSE_ERROR)
    {
        aloge("fatal error! isp init fail[%d]", eErr);
        return ERR_ISP_EFUSE_ERR;
    }
    else if (eErr != 0)
    {
        aloge("fatal error! isp init fail[%d]", eErr);
        return eErr;
    }

#if (MPPCFG_SUPPORT_FASTBOOT == 0 || MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW == 1)
    eErr = isp_run(IspDev);
    if (eErr != 0)
    {
        aloge("fatal error! isp run fail[%d]", eErr);
    }
#endif

    pthread_mutex_lock(&gpIspManager->mLock);
    //detect isp_ctx ,if not exist then create.
    IspCtx *entry;
    int Ispctx_Detect = 0;
    list_for_each_entry(entry, &gpIspManager->mIspCtxList, mList)
    {
        if (entry->nIspDev == IspDev)
        {
            Ispctx_Detect = 1;
            entry->nUseCnt++;
            break;
        }
    }
    if (!Ispctx_Detect)
    {
        IspCtx *newCtx = (IspCtx *)malloc(sizeof(IspCtx));
        memset(newCtx, 0, sizeof(IspCtx));
        newCtx->nIspDev = IspDev;
        newCtx->nUseCnt = 1;

    #if (MPPCFG_SUPPORT_FASTBOOT == 0 || MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW == 1)
        //get sensor info
        struct sensor_config stConfig;
        memset(&stConfig, 0, sizeof(struct sensor_config));
        int ret = isp_get_sensor_info(IspDev, &stConfig);
        if (ret != 0)
        {
            aloge("fatal error! Get isp[%d] sensor information failed[%d]!", IspDev, ret);
        }
        if (message_create(&newCtx->msg_queue) != 0)
        {
            aloge("fatal error! create message queue error!");
        }
        newCtx->TdmRawThread = SendRawToApp(&stConfig, IspDev, &newCtx->msg_queue);
    #endif
        list_add_tail(&newCtx->mList, &gpIspManager->mIspCtxList);
    }
    pthread_mutex_unlock(&gpIspManager->mLock);
    return eErr;
}

AW_S32 AW_MPI_ISP_Stop(ISP_DEV IspDev)
{
    int ret = SUCCESS;
    if(false == gpIspManager->bIspRun)
    {
        alogd("ConfigFile forbids to stop isp.");
        return SUCCESS;
    }

#if (MPPCFG_SUPPORT_FASTBOOT == 0 || MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW == 1)
    ret = isp_stop(IspDev);
    if(ret != 0)
    {
        aloge("fatal error! isp stop fail[%d]", ret);
    }
    ret = isp_pthread_join(IspDev);
    if(ret != 0)
    {
        aloge("fatal error! isp pthread join fail[%d]", ret);
    }
#endif

    ret = isp_exit(IspDev);
    if(ret != 0)
    {
        aloge("fatal error! isp exit fail[%d]", ret);
    }

    pthread_mutex_lock(&gpIspManager->mLock);
    void *pRetVal = NULL;
    IspCtx *entry, *pTmp;
    list_for_each_entry_safe(entry, pTmp, &gpIspManager->mIspCtxList, mList)
    {
        if (entry->nIspDev == IspDev)
        {
            if (0 == entry->nUseCnt)
            {
                aloge("fatal error! why ispCtx refCnt is 0?");
            }
            entry->nUseCnt--;
            if(!entry->nUseCnt)
            {
            #if (MPPCFG_SUPPORT_FASTBOOT == 0 || MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW == 1)
                //destroy TdmRawThread
                message_t msg;
                msg.command = Stop;
                put_message(&entry->msg_queue, &msg);
                pthread_join(entry->TdmRawThread, &pRetVal);
                message_destroy(&entry->msg_queue);
                alogd("TdmRawThread pRetVal=%p", pRetVal);
            #endif
                list_del(&entry->mList);
                free(entry);
            }
            break;
        }
    }
    pthread_mutex_unlock(&gpIspManager->mLock);
    return ret;
}

ERRORTYPE AW_MPI_ISP_Exit()
{
    int ret = SUCCESS;
    if (gpIspManager != NULL)
    {
        if(gpIspManager->bIspRun)
        {
            media_dev_exit();
        }

        pthread_mutex_lock(&gpIspManager->mLock);
        IspCtx *entry;
        list_for_each_entry(entry, &gpIspManager->mIspCtxList, mList)
        {
            if(entry)
            {
                aloge("fatal error! isp[%d] still running, usecnt[%d]", entry->nIspDev ,entry->nUseCnt);
            }
        }
        pthread_mutex_unlock(&gpIspManager->mLock);
        pthread_mutex_destroy(&gpIspManager->mLock);
        free(gpIspManager);
        gpIspManager = NULL;
    }
    return ret;
}

AW_S32 AW_MPI_ISP_EventsStop(ISP_DEV IspDev)
{
	AW_S32 eErr;

#if MPPCFG_SUPPORT_FASTBOOT
		return 0;
#endif
	eErr = isp_events_stop(IspDev);

	return eErr;
}

AW_S32 AW_MPI_ISP_EventsRestart(ISP_DEV IspDev)
{
	AW_S32 eErr;

#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	eErr = isp_events_restar(IspDev);

	return eErr;
}

AW_S32 AW_MPI_ISP_SetModuleOnOff(ISP_DEV IspDev, ISP_MODULE_ONOFF *pstIspModuleOnOff)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	struct isp_test_enable_cfg isp_enable;
	memcpy(&isp_enable, pstIspModuleOnOff, sizeof(struct isp_test_enable_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_TEST, HW_ISP_CFG_TEST_ENABLE, &isp_enable);
	isp_update(IspDev);

	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetModuleOnOff(ISP_DEV IspDev, ISP_MODULE_ONOFF *pstIspModuleOnOff)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	struct isp_test_enable_cfg isp_enable;
	isp_get_cfg(IspDev, HW_ISP_CFG_TEST, HW_ISP_CFG_TEST_ENABLE, &isp_enable);
	memcpy(pstIspModuleOnOff, &isp_enable, sizeof(struct isp_test_enable_cfg));

	return SUCCESS;
}

AW_S32 AW_MPI_ISP_SetSaveCTX(ISP_DEV IspDev)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	return isp_set_saved_ctx(IspDev);
}

AW_S32 AW_MPI_ISP_SetAe(ISP_DEV IspDev, ISP_AE_S *pAe)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_AE, pAe);
	isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetAe(ISP_DEV IspDev, ISP_AE_S *pAe)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_AE, pAe);
	return ret;
}

AW_S32 AW_MPI_ISP_ReadIspCfgBin(ISP_DEV IspDev, ISP_CFG_BIN_MODE ModeFlag, char *isp_cfg_bin_path)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	AW_S32 ret = FAILURE;

	if((ModeFlag < 0) || (ModeFlag > 3))
	{
		aloge("Invalid ISP_CFG_BIN_MODE\n");
		return ret;
	}

	if (isp_cfg_bin_path == NULL)
	{
		aloge("Invalid isp_cfg_ini_path\n");
		return ret;
	}

	ret = isp_read_cfg_bin(IspDev, (int)ModeFlag, isp_cfg_bin_path);
	return ret;
}

AW_S32 AW_MPI_ISP_GetSensorFps(ISP_DEV IspDev, int *fps)
{
	return isp_get_fps(IspDev, fps);
}

int AW_MPI_ISP_GetEvDigitalGain(ISP_DEV IspDev, AW_U32 *EvDigitalGain)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	return isp_get_attr_cfg(IspDev, ISP_CTRL_DIGITAL_GAIN, EvDigitalGain);
}

int AW_MPI_ISP_GetEvTotalGain(ISP_DEV IspDev, AW_U32 *EvTotalGain)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	return isp_get_attr_cfg(IspDev, ISP_CTRL_TOTAL_GAIN, EvTotalGain);
}

int AW_MPI_ISP_GetAwbStatsAvg(ISP_DEV IspDev, AW_U32 *awb_stats_ravg, AW_U32 *awb_stats_gavg, AW_U32 *awb_stats_bavg)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	return isp_get_awb_stats_avg(IspDev, awb_stats_ravg, awb_stats_gavg, awb_stats_bavg);
}

int AW_MPI_ISP_GetTemperature(ISP_DEV IspDev)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	return isp_get_temp(IspDev);
}

AW_U32 AW_MPI_ISP_3AInfo_Ae(ISP_DEV IspDev, isp_3a_info_ae *Isp_3A_Info_Ae)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	AW_S32 ret = FAILURE;

	ret = isp_get_ae_info(IspDev, Isp_3A_Info_Ae);
	return ret;
}

AW_U32 AW_MPI_ISP_3AInfo_Awb(ISP_DEV IspDev, isp_3a_info_awb *Isp_3A_Info_Awb)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	AW_S32 ret = FAILURE;

	ret = isp_get_awb_info(IspDev, Isp_3A_Info_Awb);
	return ret;
}

AW_U32 AW_MPI_ISP_GetYuvPortionY(SIZE_S Res, RECT_S RoiRgn, VIDEO_FRAME_INFO_S pstFrameInfo)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	struct isp_h3a_coor_win target_area;
	struct isp_size pic_size;
	AW_U32 y_stat = 0;

	if (pstFrameInfo.VFrame.mpVirAddr[0] == NULL) {
		aloge("Invalid mpVirAddr[0]\n");
		return -1;
	}

	pic_size.width = Res.Width;
	pic_size.height = Res.Height;
	target_area.x1 = RoiRgn.X;
	target_area.y1 = RoiRgn.Y;
	target_area.x2 = RoiRgn.X + RoiRgn.Width;
	target_area.y2 = RoiRgn.Y +RoiRgn.Height;
	if ((target_area.x1 < 0) || (target_area.x2 < 0) ||
			(target_area.y1 < 0) || (target_area.y2 < 0)) {
		aloge("Invalid RoiRgn\n");
		return -1;
	}
	y_stat = isp_get_yuv_ystat(pic_size, target_area, pstFrameInfo.VFrame.mpVirAddr[0]);

	return y_stat;
}

AW_S32 AW_MPI_ISP_SetLocalExposureArea(ISP_DEV IspDev, SIZE_S Res, RECT_S RoiRgn, AW_U16 ForceAeTarget, AW_U16 Enable)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	AW_S32 ret = FAILURE;
	struct isp_h3a_coor_win coor;
	POINT_S PtRightBottom;

	PtRightBottom.X = RoiRgn.X + RoiRgn.Width;
	PtRightBottom.Y = RoiRgn.Y + RoiRgn.Height;

	if ((RoiRgn.X > Res.Width) || (RoiRgn.Y > Res.Height) ||
		(PtRightBottom.X > Res.Width) || (PtRightBottom.Y > Res.Height)) {
		aloge("(%d, %d) (%d, %d), it is invalid coordinate for src:%d*%d !!!\n",
			RoiRgn.X, RoiRgn.Y, PtRightBottom.X, PtRightBottom.Y, Res.Width, Res.Height);
		return -1;
	}

	coor.x1 = (int)( (float)RoiRgn.X*2000/Res.Width  - 1000 );
	coor.y1 = (int)( (float)RoiRgn.Y*2000/Res.Height - 1000 );
	coor.x2 = (int)( (float)PtRightBottom.X*2000/Res.Width  - 1000 );
	coor.y2 = (int)( (float)PtRightBottom.Y*2000/Res.Height - 1000 );
	isp_set_ae_roi(IspDev, &coor, ForceAeTarget, Enable);

	return ret;
}

AW_S32 AW_MPI_ISP_SetRoiArea(ISP_DEV IspDev, SIZE_S ModelRes, isp_rect_roi_t *res)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	struct npu_face_nr_config npu_face_ctx;
	isp_rect_t *target_box_rect;
	AW_S32 ret = FAILURE;
	AW_S32 i;

	if (res == NULL) {
		aloge("Invalid BBoxResults_t\n");
		return -1;
	}

	target_box_rect = res->boxes;
	npu_face_ctx.roi_num = res->valid_cnt;
	for (i = 0;i < res->valid_cnt;i++, target_box_rect++) {
		npu_face_ctx.face_roi[i].x1 = (target_box_rect->xmin * 2000 / ModelRes.Width)  - 1000;
		npu_face_ctx.face_roi[i].y1 = (target_box_rect->ymin * 2000 / ModelRes.Height) - 1000;
		npu_face_ctx.face_roi[i].x2 = (target_box_rect->xmax * 2000 / ModelRes.Width)  - 1000;
		npu_face_ctx.face_roi[i].y2 = (target_box_rect->ymax * 2000 / ModelRes.Height) - 1000;
		/* keep range in [-1000, 1000] */
		npu_face_ctx.face_roi[i].x1 = clamp(npu_face_ctx.face_roi[i].x1, -1000, 1000);
		npu_face_ctx.face_roi[i].y1 = clamp(npu_face_ctx.face_roi[i].y1, -1000, 1000);
		npu_face_ctx.face_roi[i].x2 = clamp(npu_face_ctx.face_roi[i].x2, -1000, 1000);
		npu_face_ctx.face_roi[i].y2 = clamp(npu_face_ctx.face_roi[i].y2, -1000, 1000);
	}
	isp_set_attr_cfg(IspDev, ISP_CTRL_NPU_NR_PARAM, &npu_face_ctx);

	return ret;
}

AW_S32 AW_MPI_ISP_SetAeFlickerComp(ISP_DEV IspDev, HW_S16 enable)
{
#if MPPCFG_SUPPORT_FASTBOOT
	return 0;
#endif
	AW_S32 ret = FAILURE;

	ret = isp_set_ae_flicker_comp(IspDev, enable);

	return ret;
}

ERRORTYPE AW_MPI_ISP_GetSensorInfo(ISP_DEV IspDev, struct sensor_config *cfg)
{
    int ret;
    if ((ret=isp_get_sensor_info(IspDev, cfg)) != 0)
    {
        aloge("fatal error! Get isp[%d] sensor information failed[%d]!", IspDev, ret);
        return FAILURE;
    }
    //alogd("isp[%d] sensor info width:%d, height:%d", IspDev, cfg->width, cfg->height);
    return SUCCESS;
}

AW_S32 AW_MPI_ISP_RegisterTdmBufDoneCallback(ISP_DEV IspDev, void *func)
{
#if (MPPCFG_SUPPORT_FASTBOOT == 1 && MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW == 0)
    return 0;
#endif

    isp_register_tdmbuffer_done_callback(IspDev, func);

    return SUCCESS;
}

AW_S32 AW_MPI_ISP_ReturnTdmBuf(ISP_DEV IspDev, struct vin_isp_tdm_event_status *status)
{
#if (MPPCFG_SUPPORT_FASTBOOT == 1 && MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW == 0)
    return 0;
#endif

    return isp_return_tdmbuffer(IspDev, status);
}

AW_S32 AW_MPI_ISP_GetTdmData(ISP_DEV IspDev, struct vin_isp_tdm_data *data)
{
#if (MPPCFG_SUPPORT_FASTBOOT == 1 && MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW == 0)
    return 0;
#endif

    return isp_get_tdm_data(IspDev, data);
}

AW_S32 AW_MPI_ISP_SendTdmData(ISP_DEV IspDev, struct vin_isp_tdm_data *data)
{
#if (MPPCFG_SUPPORT_FASTBOOT == 1 && MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW == 0)
    return 0;
#endif

    return isp_send_tdm_data(IspDev, data);
}

AW_S32 AW_MPI_ISP_SetLdciSource(ISP_DEV IspDev, int mode)
{
#if MPPCFG_SUPPORT_FASTBOOT
    return 0;
#endif

    return isp_set_ldci_source(IspDev, mode);
}

AW_S32 AW_MPI_ISP_SetLdciFrame(ISP_DEV IspDev, ldci_frame_config_t *frame_params)
{
#if MPPCFG_SUPPORT_FASTBOOT
    return 0;
#endif

    return isp_set_ldci_frame(IspDev, frame_params);
}

#if 0
// WDR
AW_S32 AW_MPI_ISP_SetPltmWDR(ISP_DEV IspDev, ISP_DYNAMIC_PLTM_S *pPltm)
{
	AW_S32 ret = FAILURE;
	ISP_DYNAMIC_PLTM_S isp_pltm;
	// struct isp_tuning_pltm_cfg pltm_cfg;//HW_ISP_CFG_TUNING_PLTM
	// struct isp_tuning_pltm_table_cfg pltm_table_cfg; // HW_ISP_CFG_TUNING_PLTM_TBL
	memcpy(&isp_pltm.pltm_cfg, &pPltm->pltm_cfg, sizeof(struct isp_tuning_pltm_cfg));
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_PLTM, &isp_pltm.pltm_cfg);
	// ret = isp_set_cfg(IspDev, HW_ISP_CFG_TUNING_TABLES, HW_ISP_CFG_TUNING_PLTM_TBL, &isp_pltm.pltm_table_cfg);
    isp_update(IspDev);

	return 0;
}
AW_S32 AW_MPI_ISP_GetPltmWDR(ISP_DEV IspDev, ISP_DYNAMIC_PLTM_S *pPltm)
{
	AW_S32 ret = FAILURE;
	ISP_DYNAMIC_PLTM_S isp_pltm;
	// struct isp_tuning_pltm_cfg;//HW_ISP_CFG_TUNING_PLTM
	// struct isp_tuning_pltm_table_cfg;// HW_ISP_CFG_TUNING_PLTM_TBL
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_PLTM, &isp_pltm.pltm_cfg);
	memcpy(&pPltm->pltm_cfg, &isp_pltm.pltm_cfg, sizeof(struct isp_tuning_pltm_cfg));
	// ret = isp_get_cfg(IspDev, HW_ISP_CFG_TUNING_TABLES, HW_ISP_CFG_TUNING_PLTM_TBL, &isp_pltm.pltm_table_cfg);
	// memcpy(pPltm, &isp_pltm, sizeof(struct isp_dynamic_pltm_cfg));
	return 0;
}

// 2DNR
AW_S32 AW_MPI_ISP_SetNRAttr(ISP_DEV IspDev, ISP_NR_ATTR_S *pstNRAttr)
{
	struct isp_dynamic_denoise_cfg isp_denoise_cfg;
	if (NULL == pstNRAttr)
		return FALSE;
    memcpy(&isp_denoise_cfg, pstNRAttr, sizeof(struct isp_dynamic_denoise_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_DENOISE, &isp_denoise_cfg);
    isp_update(IspDev);
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetNRAttr(ISP_DEV IspDev, ISP_NR_ATTR_S *pstNRAttr)
{
	struct isp_dynamic_denoise_cfg isp_denoise_cfg;
	isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_DENOISE, &isp_denoise_cfg);
    memcpy(pstNRAttr, &isp_denoise_cfg, sizeof(struct isp_dynamic_denoise_cfg));
	return SUCCESS;
}
// 3DNR
AW_S32 AW_MPI_ISP_Set3NRAttr(ISP_DEV IspDev, ISP_3NR_ATTR_S *pstNRAttr)
{
	struct isp_dynamic_tdf_cfg isp_tdf_cfg;
	if (NULL == pstNRAttr)
		return FALSE;
    memcpy(&isp_tdf_cfg, pstNRAttr, sizeof(struct isp_dynamic_tdf_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_TDF, &isp_tdf_cfg);
    isp_update(IspDev);
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_Get3NRAttr(ISP_DEV IspDev, ISP_3NR_ATTR_S *pstNRAttr)
{
	struct isp_dynamic_tdf_cfg isp_tdf_cfg;
	isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_TDF, &isp_tdf_cfg);
    memcpy(pstNRAttr, &isp_tdf_cfg, sizeof(struct isp_dynamic_tdf_cfg));
	return SUCCESS;
}
#else

#endif

#if 0
AW_S32 AW_MPI_ISP_SetRegister(ISP_DEV IspDev, AW_U32 u32Addr, AW_U32 u32Value)
{ 
    isp_update(IspDev);
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetRegister(ISP_DEV IspDev, AW_U32 u32Addr, AW_U32 *pu32Value)
{
	return SUCCESS;
}
// on_off
AW_S32 AW_MPI_ISP_SetModuleOnOff(ISP_DEV IspDev, ISP_MODULE_ONOFF *pstIspModuleOnOff)
{
	struct isp_test_enable_cfg isp_enable;
	memcpy(&isp_enable, pstIspModuleOnOff, sizeof(struct isp_test_enable_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_TEST, HW_ISP_CFG_TEST_ENABLE, &isp_enable);
	isp_update(IspDev);

	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetModuleOnOff(ISP_DEV IspDev, ISP_MODULE_ONOFF *pstIspModuleOnOff)
{
	struct isp_test_enable_cfg isp_enable;
	isp_get_cfg(IspDev, HW_ISP_CFG_TEST, HW_ISP_CFG_TEST_ENABLE, &isp_enable);
	memcpy(pstIspModuleOnOff, &isp_enable, sizeof(struct isp_test_enable_cfg));

	return SUCCESS;
}
// Flash 
AW_S32 AW_MPI_ISP_SetFlash(ISP_DEV IspDev, ISP_FLASH_S *pFlash)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_FLASH, pFlash);
    isp_update(IspDev);
    return ret;
}
AW_S32 AW_MPI_ISP_GetFlash(ISP_DEV IspDev, ISP_FLASH_S *pFlash)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_FLASH, pFlash);
	return ret;
}

/*// Flicker
AW_S32 AW_MPI_ISP_SetFlicker(ISP_DEV IspDev, ISP_FLICKER_S *pFlicker)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_FLICKER, pFlicker);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetFlicker(ISP_DEV IspDev, ISP_FLICKER_S *pFlicker)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_FLICKER, pFlicker);
	return ret;
}*/
// defog
AW_S32 AW_MPI_ISP_SetDeFogAttr(ISP_DEV IspDev, ISP_DEFOG_ATTR_S *pstDefogAttr)
{
	struct isp_tuning_defog_cfg isp_defog;

	if (NULL == pstDefogAttr)
		return FALSE;

	isp_defog.strength = pstDefogAttr->strength;
	isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_DEFOG , &isp_defog);
    isp_update(IspDev);
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetDeFogAttr(ISP_DEV IspDev, ISP_DEFOG_ATTR_S *pstDefogAttr)
{
	struct isp_tuning_defog_cfg isp_defog;
	isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_DEFOG , &isp_defog);
	pstDefogAttr->strength = isp_defog.strength;

	return SUCCESS;
}
// dpc
AW_S32 AW_MPI_ISP_SetDPC(ISP_DEV IspDev, ISP_OTF *pDpcOtf)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_DPC_OTF , pDpcOtf);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetDPC(ISP_DEV IspDev, ISP_OTF *pDpcOtf)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_DPC_OTF , pDpcOtf);
	return ret;
}
// blc
AW_S32 AW_MPI_ISP_SetBlackLevel(ISP_DEV IspDev, ISP_BLACK_LEVEL_S *pstBlackLevel)
{
	struct isp_tuning_blc_gain_cfg  isp_gain_offset;

	isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_BLACK_LV , &isp_gain_offset);
	isp_gain_offset.value[0] = pstBlackLevel->au16BlackLevel[0];
	isp_gain_offset.value[1] = pstBlackLevel->au16BlackLevel[1];
	isp_gain_offset.value[2] = pstBlackLevel->au16BlackLevel[2];
	isp_gain_offset.value[3] = pstBlackLevel->au16BlackLevel[3];
	isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_BLACK_LV , &isp_gain_offset);
    isp_update(IspDev);
    
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetBlackLevel(ISP_DEV IspDev, ISP_BLACK_LEVEL_S *pstBlackLevel)
{
	struct isp_tuning_blc_gain_cfg  isp_gain_offset;

	isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_BLACK_LV , &isp_gain_offset);
	pstBlackLevel->au16BlackLevel[0] = isp_gain_offset.value[0];
	pstBlackLevel->au16BlackLevel[1] = isp_gain_offset.value[1];
	pstBlackLevel->au16BlackLevel[2] = isp_gain_offset.value[2];
	pstBlackLevel->au16BlackLevel[3] = isp_gain_offset.value[3];

	return SUCCESS;
}
// lsc
AW_S32 AW_MPI_ISP_SetShadingAttr(ISP_DEV IspDev, ISP_SHADING_ATTR_S *pstShadingAttr)
{
/*	struct isp_tuning_lens_shading_cfg isp_lsc;

	if (NULL == pstShadingAttr)
		return FALSE;

	isp_lsc.ff_mod = pstShadingAttr->ff_mod;
	isp_lsc.center_x = pstShadingAttr->center_x;
	isp_lsc.center_y = pstShadingAttr->center_y;
    isp_lsc.rolloff_ratio = pstShadingAttr->rolloff_ratio;
	memcpy(&(isp_lsc.value[0][0]), &(pstShadingAttr->value[0][0]), sizeof(isp_lsc.value));
	memcpy(isp_lsc.color_temp_triggers, pstShadingAttr->color_temp_triggers, sizeof(isp_lsc.color_temp_triggers));

	isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_LSC , &isp_lsc);
    isp_update(IspDev);
*/
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetShadingAttr(ISP_DEV IspDev, ISP_SHADING_ATTR_S *pstShadingAttr)
{
/*	struct isp_tuning_lens_shading_cfg isp_lsc;

	if (NULL == pstShadingAttr)
		return FALSE;

	isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_LSC , &isp_lsc);
	pstShadingAttr->ff_mod= isp_lsc.ff_mod;
	pstShadingAttr->center_x= isp_lsc.center_x;
	pstShadingAttr->center_y= isp_lsc.center_y;
    pstShadingAttr->rolloff_ratio = isp_lsc.rolloff_ratio;
	memcpy(&(pstShadingAttr->value[0][0]), &(isp_lsc.value[0][0]), sizeof(isp_lsc.value));
	memcpy(pstShadingAttr->color_temp_triggers, isp_lsc.color_temp_triggers, sizeof(isp_lsc.color_temp_triggers));
*/
	return SUCCESS;
}
// gamma
AW_S32 AW_MPI_ISP_SetGamma(ISP_DEV IspDev, ISP_GAMMA_ATTR_S *pstGammaAttr)
{
    struct isp_tuning_gamma_table_cfg isp_tuning_gamma;
    memcpy(&isp_tuning_gamma, pstGammaAttr, sizeof(struct isp_tuning_gamma_table_cfg));

	isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_GAMMA , &isp_tuning_gamma);
    isp_update(IspDev);
    
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetGamma(ISP_DEV IspDev, ISP_GAMMA_ATTR_S *pstGammaAttr)
{
	struct isp_tuning_gamma_table_cfg isp_tuning_gamma;
	isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_GAMMA , &isp_tuning_gamma);
    memcpy(pstGammaAttr, &isp_tuning_gamma, sizeof(struct isp_tuning_gamma_table_cfg));

	return SUCCESS;
}
// linear
AW_S32 AW_MPI_ISP_SetLinearity(ISP_DEV IspDev, ISP_LINEARITY_S *pLinearity)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_LINEARITY, pLinearity);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetLinearity(ISP_DEV IspDev, ISP_LINEARITY_S *pLinearity)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_LINEARITY, pLinearity);
	return ret;
}
// dis
AW_S32 AW_MPI_ISP_SetDistortion(ISP_DEV IspDev, ISP_DISTORTION_S *pDistortion)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_DISTORTION, pDistortion);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetDistortion(ISP_DEV IspDev, ISP_DISTORTION_S *pDistortion)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_DISTORTION, pDistortion);
	return ret;
}
#if 0
AW_S32 AW_MPI_ISP_SetTrigger(ISP_DEV IspDev, ISP_TRIGER_S *pTriger)
{
	AW_S32 ret = FAILURE;
	// ret = isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_TRIGGER, pTriger);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetTrigger(ISP_DEV IspDev, ISP_TRIGER_S *pTriger)
{
	AW_S32 ret = FAILURE;
	// ret = isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_TRIGGER, pTriger);
	return ret;
}
AW_S32 AW_MPI_ISP_SetLumMapPoint(ISP_DEV IspDev, ISP_SINGLE_S *plumP)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_LUM_POINT, plumP);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetLumMapPoint(ISP_DEV IspDev, ISP_SINGLE_S *plumP)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_LUM_POINT, plumP);
	return ret;
}
AW_S32 AW_MPI_ISP_SetGainMapPoint(ISP_DEV IspDev, ISP_SINGLE_S *pGainP)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_GAIN_POINT, pGainP);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetGainMapPoint(ISP_DEV IspDev, ISP_SINGLE_S *pGainP)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_GAIN_POINT, pGainP);
	return ret;
}
#endif
AW_S32 AW_MPI_ISP_AWB_SetCCMAttr(ISP_DEV IspDev, int color_temp, ISP_COLORMATRIX_ATTR_S *pstCCMAttr)
{
    switch (color_temp) {
    case 0:
        isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_CCM_LOW, pstCCMAttr);
        break;
    case 1:
        isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_CCM_MID, pstCCMAttr);
        break;
    case 2:
        isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_CCM_HIGH, pstCCMAttr);
        break;
    }
    isp_update(IspDev);
    return SUCCESS;
}
AW_S32 AW_MPI_ISP_AWB_GetCCMAttr(ISP_DEV IspDev, int color_temp, ISP_COLORMATRIX_ATTR_S *pstCCMAttr)
{
    struct isp_tuning_ccm_cfg isp_ccm;
    switch (color_temp) {
    case 0:
        isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_CCM_LOW, &isp_ccm);
        break;
    case 1:
        isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_CCM_MID, &isp_ccm);
        break;
    case 2:
        isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_CCM_HIGH, &isp_ccm);
        break;
    }
    memcpy(pstCCMAttr, &isp_ccm, sizeof(struct isp_tuning_ccm_cfg));
    
    return SUCCESS;
}
/*
// brightness
AW_S32 AW_MPI_ISP_SetBrightness(ISP_DEV IspDev, ISP_BRIGHTNESS_S *pBrightness)
{
	struct isp_dynamic_brightness_cfg isp_brightness;
    memcpy(&isp_brightness, pBrightness, sizeof(struct isp_dynamic_brightness_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_BRIGHTNESS, &isp_brightness);
    isp_update(IspDev);
	return 0;
}
AW_S32 AW_MPI_ISP_GetBrightness(ISP_DEV IspDev, ISP_BRIGHTNESS_S *pBrightness)
{
	AW_S32 ret = FAILURE;
    struct isp_dynamic_brightness_cfg isp_brightness;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_BRIGHTNESS, pBrightness);
    memcpy(pBrightness, &isp_brightness, sizeof(struct isp_dynamic_brightness_cfg));
	return ret;
}
// contrast
AW_S32 AW_MPI_ISP_SetContrast(ISP_DEV IspDev, ISP_CONTRAST_S *pContrast)
{
	struct isp_dynamic_contrast_cfg isp_contrast;
    if (NULL == pContrast)
        return FALSE;
    memcpy(&isp_contrast, pContrast, sizeof(struct isp_dynamic_contrast_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_CONTRAST, &isp_contrast);
    isp_update(IspDev);
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetContrast(ISP_DEV IspDev, ISP_CONTRAST_S *pContrast)
{
	struct isp_dynamic_contrast_cfg isp_contrast;
	isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_CONTRAST, &isp_contrast);
    memcpy(pContrast, &isp_contrast, sizeof(struct isp_dynamic_contrast_cfg));
	return SUCCESS;
}
// saturation
AW_S32 AW_MPI_ISP_SetSaturation(ISP_DEV IspDev, ISP_SATURAT\ION_S *pSta)
{
    int ret ;
	struct isp_dynamic_saturation_cfg isp_saturation;
    memcpy(&isp_saturation, pSta, sizeof(struct isp_dynamic_saturation_cfg));
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_SATURATION, &isp_saturation);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetSaturation(ISP_DEV IspDev, ISP_SATURATION_S *pSta)
{
	AW_S32 ret = FAILURE;
	struct isp_dynamic_saturation_cfg isp_saturation;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_SATURATION, &isp_saturation);
    memcpy(pSta, &isp_saturation, sizeof(struct isp_dynamic_saturation_cfg));
	return ret;
}
// hue
AW_S32 AW_MPI_ISP_SetHue(ISP_DEV IspDev, ISP_HUE_S *pHue)
{
    aloge("need implement");
    isp_update(IspDev);
    return FAILURE;
}
AW_S32 AW_MPI_ISP_GetHue(ISP_DEV IspDev, ISP_HUE_S *pHue)
{
    aloge("need implement");
    return FAILURE;
}
*/
// sharp
AW_S32 AW_MPI_ISP_SetSharpness(ISP_DEV IspDev, ISP_SHARPEN_ATTR_S *pstSharpenAttr)
{
	struct isp_dynamic_sharp_cfg isp_sharp_cfg;
	if (NULL == pstSharpenAttr)
		return FALSE;
	memcpy(&isp_sharp_cfg, pstSharpenAttr, sizeof(struct isp_dynamic_sharp_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_SHARP , &isp_sharp_cfg);
    isp_update(IspDev); 
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetSharpness(ISP_DEV IspDev, ISP_SHARPEN_ATTR_S *pstSharpenAttr)
{
	struct isp_dynamic_sharp_cfg isp_sharp_cfg;
	isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_SHARP , &isp_sharp_cfg);
	memcpy(pstSharpenAttr, &isp_sharp_cfg, sizeof(struct isp_dynamic_sharp_cfg));
	return SUCCESS;
}
// 2DNR
AW_S32 AW_MPI_ISP_SetNRAttr(ISP_DEV IspDev, ISP_NR_ATTR_S *pstNRAttr)
{
	struct isp_dynamic_denoise_cfg isp_denoise_cfg;
	if (NULL == pstNRAttr)
		return FALSE;
    memcpy(&isp_denoise_cfg, pstNRAttr, sizeof(struct isp_dynamic_denoise_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_DENOISE, &isp_denoise_cfg);
    isp_update(IspDev);
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_GetNRAttr(ISP_DEV IspDev, ISP_NR_ATTR_S *pstNRAttr)
{
	struct isp_dynamic_denoise_cfg isp_denoise_cfg;
	isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_DENOISE, &isp_denoise_cfg);
    memcpy(pstNRAttr, &isp_denoise_cfg, sizeof(struct isp_dynamic_denoise_cfg));
	return SUCCESS;
}
// 3DNR
AW_S32 AW_MPI_ISP_Set3NRAttr(ISP_DEV IspDev, ISP_3NR_ATTR_S *pstNRAttr)
{
	struct isp_dynamic_tdf_cfg isp_tdf_cfg;
	if (NULL == pstNRAttr)
		return FALSE;
    memcpy(&isp_tdf_cfg, pstNRAttr, sizeof(struct isp_dynamic_tdf_cfg));
	isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_TDF, &isp_tdf_cfg);
    isp_update(IspDev);
	return SUCCESS;
}
AW_S32 AW_MPI_ISP_Get3NRAttr(ISP_DEV IspDev, ISP_3NR_ATTR_S *pstNRAttr)
{
	struct isp_dynamic_tdf_cfg isp_tdf_cfg;
	isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_TDF, &isp_tdf_cfg);
    memcpy(pstNRAttr, &isp_tdf_cfg, sizeof(struct isp_dynamic_tdf_cfg));
	return SUCCESS;
}
// visualangle
AW_S32 AW_MPI_ISP_SetVisualAngle(ISP_DEV IspDev, ISP_VISUAL_ANGLE_S *pVisualAngle)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_VISUAL_ANGLE, pVisualAngle);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetVisualAngle(ISP_DEV IspDev, ISP_VISUAL_ANGLE_S *pVisualAngle)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_VISUAL_ANGLE, pVisualAngle);
	return ret;
}

// Global Constrast
AW_S32 AW_MPI_ISP_SetDynamicGtm(ISP_DEV IspDev, ISP_DYNAMIC_GTM_S *pGtm)
{
	AW_S32 ret = FAILURE;
	struct isp_dynamic_gtm_cfg isp_gtm;
	memcpy(&isp_gtm, pGtm, sizeof(struct isp_dynamic_gtm_cfg));
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_GTM, &isp_gtm);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetDynamicGtm(ISP_DEV IspDev, ISP_DYNAMIC_GTM_S *pGtm)
{
	AW_S32 ret = FAILURE;
	struct isp_dynamic_gtm_cfg isp_gtm;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_DYNAMIC, HW_ISP_CFG_DYNAMIC_GTM, &isp_gtm);
	memcpy(pGtm, &isp_gtm, sizeof(struct isp_dynamic_gtm_cfg));
	return ret;
}

#if 0
AW_S32 AW_MPI_ISP_SetGtm(ISP_DEV IspDev, ISP_GTM_S *pGtm)
{
	AW_S32 ret = FAILURE;
	ret = isp_set_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_GTM, pGtm);
    isp_update(IspDev);
	return ret;
}
AW_S32 AW_MPI_ISP_GetGtm(ISP_DEV IspDev, ISP_GTM_S *pGtm)
{
	AW_S32 ret = FAILURE;
	ret = isp_get_cfg(IspDev, HW_ISP_CFG_TUNING, HW_ISP_CFG_TUNING_GTM, pGtm);
	return ret;
}
#endif

#endif

