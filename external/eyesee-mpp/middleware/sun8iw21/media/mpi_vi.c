/*
 ******************************************************************************
 *
 * MPI_VI.h
 *
 * Hawkview ISP - mpi_vi.h module
 *
 * Copyright (c) 2016 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Version        Author         Date           Description
 *
 *   1.1          yuanxianfeng   2016/08/01     VI
 *
 *****************************************************************************
 */

#define LOG_TAG "mpi_vi"
#include <utils/plat_log.h>

//ref platform headers
#include <errno.h>
#include <stdlib.h>
#include <string.h>
// #include "cdx_list.h"
#include "plat_defines.h"
#include "plat_errno.h"
#include "plat_math.h"
#include "plat_type.h"

//media api headers to app
#include "mm_comm_vi.h"
#include "mm_comm_video.h"
#include "mm_common.h"
#include "mpi_vi.h"
#include "mpi_vi_private.h"
#include "videoIn/videoInputHw.h"
#include "mpi_isp.h"

//media internal common headers.
#include <media_common.h>
#include <mm_component.h>
#include <tsemaphore.h>
#include <vencoder.h>


#define VI_FREQ_DEFAULT (432000000)
#define VI_FREQ_MIN     (0)
#define VI_FREQ_MAX     (580000000)


static ERRORTYPE VideoViEventHandler(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN void *pAppData,
                                     PARAM_IN COMP_EVENTTYPE eEvent, PARAM_IN unsigned int nData1,
                                     PARAM_IN unsigned int nData2, PARAM_IN void *pEventData)
{
    int ret = 0;
    VI_CHN_MAP_S *pChn = (VI_CHN_MAP_S *)pAppData;
    VI_DEV nViDev = pChn->mViChn>>16;
    VI_CHN nViChn = pChn->mViChn&0xFFFF;
    /** the blow code is not necessary and will lead to deadlock.
        VideoInputHw_CapThread() -> VideoViEmptyThisBuffer():           gpVIDevManager->gpVippManager[vipp_id]->mLock && VirVi's mStateLock, 
        user thread call AW_MPI_VI_GetFrame()/AW_MPI_VI_ReleaseFrame(): gpVIDevManager->mManagerLock && gpVIDevManager->gpVippManager[vipp_id]->mLock
        Vi_ComponentThread()->VideoViEventHandler():                    VirVi's mStateLock && gpVIDevManager->mManagerLock
        So we delete blow code.
    */
//    viChnManager *pViDev = NULL;
//    ERRORTYPE eError = videoInputHw_searchExistDev(nViDev, &pViDev);
//    if(eError == SUCCESS)
//    {
//        if(pViDev->vipp_dev_id != nViDev)
//        {
//            aloge("fatal error! vipp [%d!=%d] is not match!", pViDev->vipp_dev_id, nViDev);
//        }
//    }
//    else
//    {
//        aloge("fatal error! search vipp[%d] fail!", nViDev);
//    }
    switch (eEvent) {
        case COMP_EventCmdComplete: {
            if (COMP_CommandStateSet == nData1) {
                alogv("video vi EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
            } else {
                alogw("Low probability! what command[0x%x]?", nData1);
            }
            break;
        }
        case COMP_EventError: {
            if (ERR_VI_SAMESTATE == nData1) {
                alogv("set same state to vi!");
                cdx_sem_up(&pChn->mSemCompCmd);
            } else if (ERR_VI_INVALIDSTATE == nData1) {
                aloge("why vi state turn to invalid?");
            } else if (ERR_VI_INCORRECT_STATE_TRANSITION == nData1) {
                aloge("fatal error! vi state transition incorrect.");
            }
            break;
        }
        case COMP_EventRecVbvFull: {
            alogw("need handle vbvFull!");
            break;
        }
        case COMP_EventBufferFlag: {
            switch (nData1) {
                case COMP_IndexVendorViSetLongExp: {
                    videoInputHw_IncreaseLongShutterRef(nData2);
                    aloge("Got one COMP_IndexVendorViSetLongExp event, vipp %d", nData2);
                    /*if(pViDev && pViDev->mMppCallback)
                    {
                        MPP_CHN_S stChn = {MOD_ID_VIU, pViDev->vipp_dev_id, nViChn};
                        pViDev->mMppCallback(
                            pViDev->pAppData,
                            &stChn,
                            MPP_EVENT_NONE,
                            NULL);
                    }*/
                } break;
                case FF_LONGEXP: {
                    ret = videoInputHw_DecreaseLongShutterRef(nData2);
                    /* we only do resume operation for the first one returned VirChn
                    * do not worry about some VirChn will lost the real long exposure video frame
                    */
                    if (videoInputHw_IsLongShutterBusy(nData2)) {
                        videoInputHw_SetVippShutterTime(nData2, (VI_SHUTTIME_CFG_S *)pEventData);
                        aloge("Got on reset shutter event, vipp %d", nData2);
                    }
                } break;
                default: {
                    aloge("fatal error! unknown data in COMP_EventBufferFlag event.");
                }
            }
        } break;
        default: {
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
        }
    }
    return SUCCESS;
}

COMP_CALLBACKTYPE VideoViCallback = {
  .EventHandler = VideoViEventHandler, .EmptyBufferDone = NULL, .FillBufferDone = NULL,
};

/* VI init, Vipp hardware ops */
AW_S32 AW_MPI_VI_Init()
{
    return videoInputHw_Open_Media();
}
AW_S32 AW_MPI_VI_Exit()
{
    return  videoInputHw_Close_Media();
}
ERRORTYPE AW_MPI_VI_CreateVipp(VI_DEV ViDev)
{
    int iRet = 0;

    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) {
        aloge("invalid ViDev[%d]", ViDev);
        return ERR_VI_INVALID_DEVID;
    }

    iRet = videoInputHw_Construct(ViDev);
    if (iRet != SUCCESS) {
        aloge("construct video input hardware failed!!\n");
        goto HwCnst_Err;
    }
    iRet = videoInputHw_ChnInit(ViDev);
    if (iRet != SUCCESS) {
        aloge("initialize video input hardware failed!!\n");
        goto HwInit_Err;
    }

    return iRet;
HwInit_Err:
    videoInputHw_Destruct(ViDev);
HwCnst_Err:
    return iRet;
}
ERRORTYPE AW_MPI_VI_DestroyVipp(VI_DEV ViDev)
{
    ERRORTYPE ret;
    int iRet = 0;

    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) {
        aloge("invalid ViDev[%d]", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    int nVippStatus = 0;
    ret = videoInputHw_searchVippStatus(ViDev, &nVippStatus);
    if(!(SUCCESS == ret && 0 == nVippStatus))
    {
        aloge("fatal error! can't destroy vipp[%d], because:0x%x-%d", ViDev, ret, nVippStatus);
        return ERR_VI_SYS_NOTREADY;
    }
    /* we can not do anything if some error occurred,
     * but should return error code.
     */
    iRet = videoInputHw_ChnExit(ViDev);
    iRet |= videoInputHw_Destruct(ViDev);
    return iRet;
}
ERRORTYPE AW_MPI_VI_SetVippAttr(VI_DEV ViDev, VI_ATTR_S *pstAttr)
{
    int iRet = 0;

    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) {
        aloge("invalid ViDev[%d]", ViDev);
        return ERR_VI_INVALID_DEVID;
    }

    if(pstAttr->fps > VI_HIGH_FRAMERATE_STANDARD) {
        unsigned int recommendValue = (pstAttr->fps/30)*5;
        if(pstAttr->nbufs < recommendValue) {
            alogw("fatal error! suggest set buffer number[%u] to recommendValue[%u], when high frame rate[%u]",
                pstAttr->nbufs, recommendValue, pstAttr->fps);
        }
    }

    iRet = videoInputHw_SetChnAttr(ViDev, pstAttr);
    if (iRet == EN_ERR_EFUSE_ERROR) {
        return ERR_VI_EIS_EFUSE_ERR;
    }

    return iRet;
}
ERRORTYPE AW_MPI_VI_GetVippAttr(VI_DEV ViDev, VI_ATTR_S *pstAttr)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) {
        aloge("invalid ViDev[%d]", ViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_GetChnAttr(ViDev, pstAttr);
}
ERRORTYPE AW_MPI_VI_SetVIFreq(VI_DEV ViDev, int nFreq) //nFreq: MHz
{
    /* we need not have to specify the ViDev number,
     * videoInputHw_SetVIFreq will set all video devices' freq
     */
#if 0
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) {
        aloge("invalid ViDev[%d]", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
#endif
    if(nFreq <= VI_FREQ_MIN)
    {
        nFreq = VI_FREQ_DEFAULT;
        alogw("fatal error! Freq had low equal 0 MHz, set Freq is 432 MHz.");
    }

    if(nFreq > VI_FREQ_MAX)
    {
        nFreq = VI_FREQ_MAX;
        alogw("fatal error! Freq had over 480 MHz, set Freq is 480 MHz!");
    }

    return videoInputHw_SetVIFreq(ViDev, nFreq);
}
ERRORTYPE AW_MPI_VI_EnableVipp(VI_DEV ViDev)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) {
        aloge("invalid ViDev[%d]", ViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_ChnEnable(ViDev);
}
ERRORTYPE AW_MPI_VI_DisableVipp(VI_DEV ViDev)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) {
        aloge("invalid ViDev[%d]", ViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_ChnDisable(ViDev);
}
ERRORTYPE AW_MPI_VI_SetVippShutterTime(VI_DEV ViDev, VI_SHUTTIME_CFG_S *pTime)
{
    /* until now, to keep compatiblity, only set CHN0 do long exposure event.
    * we should use <AW_MPI_VI_SetShutterTime> instead of <AW_MPI_VI_SetVippShutterTime>
    */
    return AW_MPI_VI_SetShutterTime(ViDev, 0x1, pTime);
}
#if 0
/* Osd & Mask */
AW_S32 AW_MPI_VI_SetOsdMaskRegion(VI_DEV ViDev, VI_OsdMaskRegion *pstOsdMaskRegion)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    return videoInputHw_SetOsdMaskRegion(&ViDev, pstOsdMaskRegion);
}

AW_S32 AW_MPI_VI_UpdateOsdMaskRegion(VI_DEV ViDev, AW_U32 OnOff)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    return videoInputHw_UpdateOsdMaskRegion(&ViDev, OnOff);
}
#endif

/**
 * Store region. If region is available to show, draw region immediately.
 */
ERRORTYPE AW_MPI_VI_SetRegions(VI_DEV ViDev, RgnChnAttachDetailPack *pPack)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    return videoInputHw_SetRegions(ViDev, pPack);
}

/**
 * delete region. If region has already been shown, draw region immediately.
 */
ERRORTYPE AW_MPI_VI_DeleteRegions(VI_DEV ViDev, RgnChnAttachDetailPack *pPack)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    return videoInputHw_DeleteRegions(ViDev, pPack);
}

/**
 * draw this overlay immediately
 */
ERRORTYPE AW_MPI_VI_UpdateOverlayBitmap(VI_DEV ViDev, RGN_HANDLE RgnHandle, BITMAP_S *pBitmap)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_UpdateOverlayBitmap(ViDev, RgnHandle, pBitmap);
}

ERRORTYPE AW_MPI_VI_GetVippBuffer(VI_DEV ViDev, VIDEO_FRAME_INFO_S *pstFrameInfo, int BufNum)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_Get_VippBuffer(ViDev, pstFrameInfo, BufNum);
}


ERRORTYPE AW_MPI_VI_UpdateRegionChnAttr(VI_DEV ViDev, RGN_HANDLE RgnHandle, const RGN_CHN_ATTR_S *pRgnChnAttr)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    return videoInputHw_UpdateRegionChnAttr(ViDev, RgnHandle, pRgnChnAttr);
}
ERRORTYPE AW_MPI_VI_SetVippMirror(VI_DEV ViDev, int Value)
{
    int mViDev = ViDev;
    if (!(mViDev >= 0 && mViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", mViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_Isp_SetMirror(&mViDev, Value);
}
ERRORTYPE AW_MPI_VI_SetVippFlip(VI_DEV ViDev, int Value)
{
    int mViDev = ViDev;
    if (!(mViDev >= 0 && mViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", mViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_Isp_SetFlip(&mViDev, Value);
}
ERRORTYPE AW_MPI_VI_GetVippMirror(VI_DEV ViDev, int *Value)
{
    int mViDev = ViDev;
    if (!(mViDev >= 0 && mViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", mViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_Isp_GetMirror(&mViDev, Value);
}
ERRORTYPE AW_MPI_VI_GetVippFlip(VI_DEV ViDev, int *Value)
{
    int mViDev = ViDev;
    if (!(mViDev >= 0 && mViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", mViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_Isp_GetFlip(&mViDev, Value);
}

/* ISP IOCTL */
AW_S32 AW_MPI_ISP_AE_SetMode(ISP_DEV IspDev, int onOff)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetMode(&mIspDev, onOff);
}
AW_S32 AW_MPI_ISP_AE_SetExposureBias(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetExposureBias(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_SetExposure(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetExposure(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_SetISOSensitiveMode(ISP_DEV IspDev, int Mode)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetISOSensitiveMode(&mIspDev, Mode);
}
AW_S32 AW_MPI_ISP_AE_SetISOSensitive(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetISOSensitive(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_SetMetering(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetMetering(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_SetGain(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetGain(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_SetEvIdx(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetEvIdx(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_SetLock(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetLock(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_SetTable(ISP_DEV IspDev, struct ae_table_info *ae_table)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetTable(&mIspDev, ae_table);
}
AW_S32 AW_MPI_ISP_AE_SetRoiArea(ISP_DEV IspDev, SIZE_S Res, RECT_S RoiRgn, AW_U16 ForceAeTarget, AW_U16 Enable)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_RoiArea(&mIspDev, Res, RoiRgn, ForceAeTarget, Enable);
}
AW_S32 AW_MPI_ISP_AE_SetRoiMeteringArea(ISP_DEV IspDev, SIZE_S Res, RECT_S RoiRgn)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_RoiMeteringArea(&mIspDev, Res, RoiRgn);
}
/**
  For ISP to receive face_roi to enable faceAE.
  FaceAE maintains face brightness for some face detection.

  @param FaceAeInfo
    FaceAeInfo: some param about face_roi and faceAE param.
  @param Res
    Res: original image resolution size
  @return
    0: success
    others: fail
*/
AW_S32 AW_MPI_ISP_AE_SetFaceAeCfg(ISP_DEV IspDev, struct isp_face_ae_attr_info FaceAeInfo, SIZE_S Res)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_SetFaceAeCfg(&mIspDev, FaceAeInfo, Res);
}
/**
  For get FaceAeInfo to face detect.

  @param FaceAeInfo
    FaceAeInfo: some param about face_roi and faceAE param.
  @return
    0: success
    others: fail
*/
AW_S32 AW_MPI_ISP_AE_GetFaceAeCfg(ISP_DEV IspDev, struct isp_face_ae_attr_info *FaceAeInfo)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetFaceAeCfg(&mIspDev, FaceAeInfo);
}
AW_S32 AW_MPI_ISP_ReadIspBin(ISP_DEV IspDev, ISP_CFG_BIN_MODE ModeFlag, char *IspCfgBinPath)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_ReadIspBin(&mIspDev, ModeFlag, IspCfgBinPath);
}
AW_S32 AW_MPI_ISP_SetAiIsp(ISP_DEV IspDev, isp_ai_isp_info *ai_isp_info_entity)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetAiIsp(&mIspDev, ai_isp_info_entity);
}

AW_S32 AW_MPI_ISP_AWB_SetMode(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_SetMode(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_SetColorTemp(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_SetColorTemp(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_SetStatsSyncMode(ISP_DEV IspDev, ISP_AWB_STATS_MODE IspAwbStatsMode)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_SetStatsSyncMode(&mIspDev, IspAwbStatsMode);
}
AW_S32 AW_MPI_ISP_AWB_SetRGain(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_SetRGain(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_SetBGain(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_SetBGain(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_SetGrGain(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_SetGrGain(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_SetGbGain(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_SetGbGain(&mIspDev, Value);
}
/**
  For control ISP AF work status, it can stop or start foucs.
  Please run it after AW_MPI_ISP_Run.

  @param Value
    true: set ISP AF lock, it will stop foucs.
    false: set ISP AF unlock, it will start foucs.
  @return
    0: success
    others: fail
*/
AW_S32 AW_MPI_ISP_AF_SetLock(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAf_SetLock(&mIspDev, Value);
}
/**
  For get ISP AF result
  Please run it after AW_MPI_ISP_Run.

  @param AfResult
    AfResult: provide AF status and real code output for actuator.
  @return
    0: success
    others: fail
*/
AW_S32 AW_MPI_ISP_AF_GetResult(ISP_DEV IspDev, af_result_t *AfResult)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAf_GetResult(&mIspDev, AfResult);
}

AW_S32 AW_MPI_ISP_AF_SetAuto(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    ERRORTYPE eRet = SUCCESS;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAf_SetAuto(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_AF_SetAbsolute(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    ERRORTYPE eRet = SUCCESS;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAf_SetAbsolute(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_SetFlicker(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetFlicker(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_SetMirror(VI_DEV ViDev, int Value)
{
    alogd("move mirror to mpi_vi.h : AW_MPI_VI_SetVippMirror. \r\n");

    int mViDev = ViDev;
    if (!(mViDev >= 0 && mViDev < VI_VIPP_NUM_MAX)) {
        aloge("fatal error! invalid ViDev[%d]!", mViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_Isp_SetMirror(&mViDev, Value);
}
AW_S32 AW_MPI_ISP_SetFlip(VI_DEV ViDev, int Value)
{
    alogd("move flip to mpi_vi.h : AW_MPI_VI_SetVippFlip. \r\n");

    int mViDev = ViDev;
    if (!(mViDev >= 0 && mViDev < VI_VIPP_NUM_MAX)) {
        aloge("fatal error! invalid ViDev[%d]!", mViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_Isp_SetFlip(&mViDev, Value);
}

AW_S32 AW_MPI_ISP_SetBrightness(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetBrightness(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_SetContrast(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetContrast(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_SetSaturation(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetSaturation(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_SetSharpness(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetSharpness(&mIspDev, Value);
}


AW_S32 AW_MPI_ISP_SetHue(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
    return videoInputHw_Isp_SetHue(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_SetColorEffect(ISP_DEV IspDev, enum colorfx ColorStyle)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
    return videoInputHw_Isp_SetColorEffect(&mIspDev, ColorStyle);
}

/**
  @param Value
    [0, 8],ref to enum ae_table_mode. SCENE_MODE_PREVIEW, SCENE_MODE_CAPTURE, SCENE_MODE_VIDEO, SCENE_MODE_BACKLIGHT...
*/
AW_S32 AW_MPI_ISP_SetScene(ISP_DEV IspDev, enum ae_table_mode Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
    return videoInputHw_Isp_SetScene(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_SwitchIspConfig(ISP_DEV IspDev, ISP_CFG_MODE ModeFlag)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
    return videoInputHw_Isp_SetIrStatus(&mIspDev, ModeFlag);
}

AW_S32 AW_MPI_ISP_SetD3dLbcRatio(ISP_DEV IspDev, unsigned int ratio)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
    return videoInputHw_Isp_SetD3dLbcRatio(&mIspDev, ratio);
}

AW_S32 AW_MPI_ISP_SetStitchMode(ISP_DEV IspDev, enum stitch_mode_t stitch_mode)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
    return videoInputHw_Isp_SetStitchMode(&mIspDev, stitch_mode);
}

AW_S32 AW_MPI_ISP_SetSensorFps(ISP_DEV IspDev, int fps)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
    return videoInputHw_Isp_SetSensorFps(&mIspDev, fps);
}

AW_S32 AW_MPI_ISP_AE_GetMode(ISP_DEV IspDev, int *onOff)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetMode(&mIspDev, onOff);
}
AW_S32 AW_MPI_ISP_AE_GetExposureBias(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetExposureBias(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_GetExposure(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetExposure(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_GetISOSensitiveMode(ISP_DEV IspDev, int *Mode)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetISOSensitiveMode(&mIspDev, Mode);
}
AW_S32 AW_MPI_ISP_AE_GetISOSensitive(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetISOSensitive(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_GetMetering(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetMetering(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AE_GetGain(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetGain(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_GetMode(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_GetMode(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_GetColorTemp(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_GetColorTemp(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_GetRGain(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_GetRGain(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_GetBGain(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_GetBGain(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_GetGrGain(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_GetGrGain(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_AWB_GetGbGain(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_GetGbGain(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_GetFlicker(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetFlicker(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_GetMirror(VI_DEV ViDev, int *Value)
{
    printf("move mirror to mpi_vi.h : AW_MPI_VI_GetVippMirror. \r\n");
    int mViDev = ViDev;
    if (!(mViDev >= 0 && mViDev < VI_VIPP_NUM_MAX)) {
        aloge("fatal error! invalid ViDev[%d]!", mViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_Isp_GetMirror(&mViDev, Value);
}
AW_S32 AW_MPI_ISP_GetFlip(VI_DEV ViDev, int *Value)
{
    printf("move flip to mpi_vi.h : AW_MPI_VI_GetVippFlip. \r\n");

    int mViDev = ViDev;
    if (!(mViDev >= 0 && mViDev < VI_VIPP_NUM_MAX)) {
        aloge("fatal error! invalid ViDev[%d]!", mViDev);
        return ERR_VI_INVALID_DEVID;
    }

    return videoInputHw_Isp_GetFlip(&mViDev, Value);
}
AW_S32 AW_MPI_ISP_GetBrightness(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetBrightness(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_GetContrast(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetContrast(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_GetSaturation(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetSaturation(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_GetSharpness(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetSharpness(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_SetPltmWDR(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetWDR(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_GetPltmWDR(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetWDR(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_GetPltmNextStren(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetPltmNextStren(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_SetNRAttr(ISP_DEV IspDev,  int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetNR(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_GetNRAttr(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetNR(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_Set3NRAttr(ISP_DEV IspDev,int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_Set3DNR(&mIspDev, Value);
}
AW_S32 AW_MPI_ISP_Get3NRAttr(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_Get3DNR(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_GetHue(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
 
	return videoInputHw_Isp_GetHue(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_GetScene(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
 
	return videoInputHw_Isp_GetScene(&mIspDev, Value);
}

//add by jaosn

AW_S32 AW_MPI_ISP_AE_GetExposureLine(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetExposureLine(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_AWB_GetCurColorT(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAwb_GetCurColorT(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_AE_GetEvIdx(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetEvIdx(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_AE_GetMaxEvIdx(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetMaxEvIdx(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_AE_GetAeLock(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetAeLock(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_AE_GetISOLumIdx(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_IspAe_GetISOLumIdx(&mIspDev, Value);
}

int AW_MPI_ISP_GetEnvLV(ISP_DEV IspDev)
{
    int mIspDev = IspDev;
    int Value = 0;
    ERRORTYPE eRet = SUCCESS;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    eRet = videoInputHw_IspAe_GetEvLv(&mIspDev, &Value);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! IspDev[%d] GetEvLv failed! ret=%d", mIspDev, eRet);
        return -1;
    }

    return Value;
}

int AW_MPI_ISP_GetEvLvAdj(ISP_DEV IspDev)
{
    int mIspDev = IspDev;
    int Value = 0;
    ERRORTYPE eRet = SUCCESS;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    eRet = videoInputHw_IspAe_GetEvLvAdj(&mIspDev, &Value);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! IspDev[%d] GetEvLvAdj failed! ret=%d", mIspDev, eRet);
        return -1;
    }

    return Value;
}

AW_S32 AW_MPI_ISP_GetIrStatus(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetIrStatus(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_GetAwbGainIr(ISP_DEV IspDev, int *RgainIr, int *BgainIr)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetIrAwbGain(&mIspDev, RgainIr, BgainIr);
}

AW_S32 AW_MPI_ISP_SetSensorMipiSwitch(ISP_DEV IspDev, struct sensor_mipi_switch_entity *switch_entity)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetSensorMipiSwitch(&mIspDev, switch_entity);
}

AW_S32 AW_MPI_ISP_GetSensorMipiSwitch(ISP_DEV IspDev, struct sensor_mipi_switch_entity *switch_entity)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetSensorMipiSwitch(&mIspDev, switch_entity);
}

/*AW_S32 AW_MPI_ISP_GetIsp2VeParam(ISP_DEV IspDev, struct enc_VencIsp2VeParam *pIsp2VeParam)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
    if (!pIsp2VeParam) {
        aloge("fatal error! IspDev[%d] invalid input param! %p", mIspDev, pIsp2VeParam);
        return ERR_VI_INVALID_PARA;
    }

    return videoInputHw_Isp_GetIsp2VeParam(&mIspDev, pIsp2VeParam);
}

AW_S32 AW_MPI_ISP_SetVe2IspParam(ISP_DEV IspDev, struct enc_VencVe2IspParam *pVe2IspParam)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }
    if (!pVe2IspParam) {
        aloge("fatal error! IspDev[%d] invalid input param! %p", mIspDev, pVe2IspParam);
        return ERR_VI_INVALID_PARA;
    }

    return videoInputHw_Isp_SetVe2IspParam(&mIspDev, pVe2IspParam);
}*/

int AW_MPI_ISP_GetAeWeightLum(ISP_DEV IspDev)
{
    int mIspDev = IspDev;
    int Value = 0;
    ERRORTYPE eRet = SUCCESS;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    eRet = videoInputHw_IspAe_GetWeightLum(&mIspDev, &Value);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! IspDev[%d] GetAeWeightLum failed! ret=%d", mIspDev, eRet);
        return -1;
    }

    return Value;
}

AW_S32 AW_MPI_ISP_SetSpecialScene(ISP_DEV IspDev, scene_mode_t target_scene)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetSpecialScene(&mIspDev, target_scene);
}

AW_S32 AW_MPI_ISP_SetIspAlgoFreq(ISP_DEV IspDev, int Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_SetIspAlgoFreq(&mIspDev, Value);
}

AW_S32 AW_MPI_ISP_GetIspAlgoFreq(ISP_DEV IspDev, int *Value)
{
    int mIspDev = IspDev;
    if (!(mIspDev >= 0 && mIspDev < VI_ISP_NUM_MAX)) {
        aloge("fatal error! invalid IspDev[%d]!", mIspDev);
        return ERR_VI_INVALID_PHYCHNID;
    }

    return videoInputHw_Isp_GetIspAlgoFreq(&mIspDev, (unsigned short *)Value);
}

/* virtual chn */
ERRORTYPE AW_MPI_VI_CreateVirChn(VI_DEV ViDev, VI_CHN ViCh, ViVirChnAttrS *pAttr)
{
    ERRORTYPE eRet = SUCCESS;
    int status = -1;
    VI_CHN_MAP_S *pNode = NULL;

    if ((!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) ||
        (!(ViCh >= 0 && ViCh < VI_VIRCHN_NUM_MAX))){
        aloge("fatal error! invalid ViDev[%d], ViVirChn[%d]!", ViDev, ViCh);
        return ERR_VI_INVALID_CHNID;
    }
    eRet = videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode);
    if (SUCCESS == eRet)
    {
        alogv("vichn[%d] has exist!!\n", ViCh);
        return ERR_VI_EXIST;
    }
    else if(ERR_VI_SYS_NOTREADY == eRet)
    {
        aloge("fatal error! whu vipp[%d] is not ready when create virChn[%d]?", ViDev, ViCh);
        return ERR_VI_SYS_NOTREADY;
    }
    videoInputHw_searchVippStatus(ViDev, &status);
    if (0 == status) {
        alogd("we allow to create virChn[%d] in disable status of vipp[%d]", ViCh, ViDev);
        //return ERR_VI_UNEXIST;
    }

    pNode = videoInputHw_CHN_MAP_S_Construct();
    // pNode->mViDev = ViDev;
    // pNode->mViChn = ViCh;
    // printf("pNode->mViChn = %d, %d, .\r\n", ViDev, ViCh);
    pNode->mViChn = ((ViDev << 16) & 0xFFFF0000) | (ViCh & 0x0000FFFF);
    // printf("pNode->mViChn = %x.\r\n", pNode->mViChn);
    eRet =
      COMP_GetHandle((COMP_HANDLETYPE *)&(pNode->mViComp), CDX_ComponentNameViScale, (void *)pNode, &VideoViCallback);
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_VIU;
    ChannelInfo.mDevId = ViDev;
    ChannelInfo.mChnId = ViCh;
    eRet = COMP_SetConfig(pNode->mViComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);

    VI_ATTR_S stAttr;
    if (SUCCESS == videoInputHw_GetChnAttr(ViDev, &stAttr)) 
    {
        eRet = COMP_SetConfig(pNode->mViComp, COMP_IndexVendorViDevAttr, (void*)&stAttr);
    }
    eRet = COMP_SetConfig(pNode->mViComp, COMP_IndexVendorViChnAttr, (void*)pAttr);
    eRet = COMP_SendCommand(pNode->mViComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    videoInputHw_addChannel(ViDev, pNode);

    return eRet;
}
ERRORTYPE AW_MPI_VI_DestroyVirChn(VI_DEV ViDev, VI_CHN ViCh)
{
    ERRORTYPE eRet = SUCCESS;
    VI_CHN_MAP_S *pNode;

    if ((!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) ||
        (!(ViCh >= 0 && ViCh < VI_VIRCHN_NUM_MAX))){
        aloge("fatal error! invalid ViDev[%d], ViVirChn[%d]!", ViDev, ViCh);
        return ERR_VI_INVALID_CHNID;
    }
    if (SUCCESS != videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode)) {
        alogw("vipp[%d]vichn[%d] is unexist!!\n", ViDev, ViCh);
        return ERR_VI_UNEXIST;
    }

    if (pNode->mViComp) {
        COMP_STATETYPE nCompState;
        if (SUCCESS == pNode->mViComp->GetState(pNode->mViComp, &nCompState)) {
            if (nCompState == COMP_StateIdle) {
                eRet = pNode->mViComp->SendCommand(pNode->mViComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
                cdx_sem_down(&pNode->mSemCompCmd);
                eRet = SUCCESS;
            } else if (nCompState == COMP_StateLoaded) {
                eRet = SUCCESS;
            } else if (nCompState == COMP_StateInvalid) {
                alogw("Low probability! Component StateInvalid?");
                eRet = SUCCESS;
            } else {
                aloge("fatal error! vipp[%d]virChn[%d] invalid Vi state[0x%x]!", ViDev, ViCh, nCompState);
                eRet = FAILURE;
            }
            if (eRet == SUCCESS) {
                videoInputHw_removeChannel(ViDev, pNode);
                COMP_FreeHandle(pNode->mViComp);
                pNode->mViComp = NULL;
                videoInputHw_CHN_MAP_S_Destruct(pNode);
                eRet = SUCCESS;
            } else {
                eRet = ERR_VI_BUSY;
            }
        } else {
            aloge("fatal error! GetState fail!");
            eRet = ERR_VI_BUSY;
        }
    } else {
        aloge("fatal error! no Vi component!");
        if (NULL != pNode) {
            videoInputHw_CHN_MAP_S_Destruct(pNode);
            eRet = SUCCESS;
        }
    }
    return eRet;
}
ERRORTYPE AW_MPI_VI_GetVirChnAttr(VI_DEV ViDev, VI_CHN ViCh, ViVirChnAttrS *pAttr)
{
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    VI_CHN_MAP_S *pNode;

    if ((!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) || (!(ViCh >= 0 && ViCh < VI_VIRCHN_NUM_MAX)))
    {
        aloge("fatal error! invalid ViDev[%d], ViVirChn[%d]!", ViDev, ViCh);
        return ERR_VI_INVALID_CHNID;
    }
    if (SUCCESS != videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode)) 
    {
        aloge("vipp[%d]vichn[%d] is unexist\n", ViDev, ViCh);
        return ERR_VI_UNEXIST;
    }
    ret = COMP_GetState(pNode->mViComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) 
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VI_NOT_PERM;
    }
    ret = COMP_GetConfig(pNode->mViComp, COMP_IndexVendorViChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_VI_SetVirChnAttr(VI_DEV ViDev, VI_CHN ViCh, ViVirChnAttrS *pAttr)
{
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    VI_CHN_MAP_S *pNode;
    if ((!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) && (!(ViCh >= 0 && ViCh < VI_VIRCHN_NUM_MAX)))
    {
        aloge("fatal error! invalid ViDev[%d], ViVirChn[%d]!", ViDev, ViCh);
        return ERR_VI_INVALID_CHNID;
    }
    if (SUCCESS != videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode)) 
    {
        aloge("vipp[%d]vichn[%d] is unexist\n", ViDev, ViCh);
        return ERR_VI_UNEXIST;
    }
    ret = COMP_GetState(pNode->mViComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) 
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VI_NOT_PERM;
    }
    ret = COMP_SetConfig(pNode->mViComp, COMP_IndexVendorViChnAttr, (void*)pAttr);
    return ret;
}

ERRORTYPE AW_MPI_VI_EnableVirChn(VI_DEV ViDev, VI_CHN ViCh)
{
    VI_CHN_MAP_S *pNode;
    if ((!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) ||
        (!(ViCh >= 0 && ViCh < VI_VIRCHN_NUM_MAX))){
        aloge("fatal error! invalid ViDev[%d], ViVirChn[%d]!", ViDev, ViCh);
        return ERR_VI_INVALID_CHNID;
    }
    if (SUCCESS != videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode)) {
        aloge("vichn[%d] is unexist!!\n", ViCh);
        return ERR_VI_UNEXIST;
    }
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pNode->mViComp->GetState(pNode->mViComp, &nCompState);
    if (COMP_StateIdle == nCompState) {
        eRet = pNode->mViComp->SendCommand(pNode->mViComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        cdx_sem_down(&pNode->mSemCompCmd);
    } else {
        alogd("vi comp state[0x%x], do nothing!", nCompState);
        eRet = SUCCESS;
    }

    return eRet;
}
ERRORTYPE AW_MPI_VI_DisableVirChn(VI_DEV ViDev, VI_CHN ViCh)
{
    VI_CHN_MAP_S *pNode;
    if ((!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) ||
        (!(ViCh >= 0 && ViCh < VI_VIRCHN_NUM_MAX))){
        aloge("fatal error! invalid ViDev[%d], ViVirChn[%d]!", ViDev, ViCh);
        return ERR_VI_INVALID_CHNID;
    }
    if (SUCCESS != videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode)) {
        aloge("vichn[%d] is unexist!!\n", ViCh);
        return ERR_VI_UNEXIST;
    }

    int eRet;
    COMP_STATETYPE nCompState;
    eRet = pNode->mViComp->GetState(pNode->mViComp, &nCompState);
    if (COMP_StateExecuting == nCompState || COMP_StatePause == nCompState) {
        eRet = pNode->mViComp->SendCommand(pNode->mViComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        cdx_sem_down(&pNode->mSemCompCmd);
        //eRet = eRet;
    } else if (COMP_StateIdle == nCompState) {
        alogd("iseGroup comp state[0x%x], do nothing!", nCompState);
        eRet = SUCCESS;
    } else {
        aloge("fatal error! check iseGroup state[0x%x]!", nCompState);
        eRet = SUCCESS;
    }
    return eRet;
}

ERRORTYPE AW_MPI_VI_GetFrame(VI_DEV ViDev, VI_CHN ViCh, VIDEO_FRAME_INFO_S *pstFrameInfo, AW_S32 s32MilliSec)
{
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    VI_CHN_MAP_S *pNode;

    if ((!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) ||
        (!(ViCh >= 0 && ViCh < VI_VIRCHN_NUM_MAX))) {
        aloge("fatal error! invalid ViDev[%d], ViVirChn[%d]!", ViDev, ViCh);
        return ERR_VI_INVALID_CHNID;
    }
    if (SUCCESS != videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode)) {
        aloge("ViDev[%d] vichn[%d] is unexist!!\n", ViDev, ViCh);
        return ERR_VI_UNEXIST;
    }
    VI_Params viParams;
    viParams.mDev = ViDev;
    viParams.mChn = ViCh;
    viParams.pstFrameInfo = pstFrameInfo;
    viParams.s32MilliSec = s32MilliSec;

    ret = pNode->mViComp->GetState(pNode->mViComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VI_NOT_PERM;
    }

    ret = pNode->mViComp->GetConfig(pNode->mViComp, COMP_IndexVendorViGetFrame, (void *)&viParams);
    return ret;
}

ERRORTYPE AW_MPI_VI_ReleaseFrame(VI_DEV ViDev, VI_CHN ViCh, VIDEO_FRAME_INFO_S *pFrameInfo)
{
    ERRORTYPE eRet;
    COMP_STATETYPE nState;
    VI_CHN_MAP_S *pNode;

    if ((!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) ||
        (!(ViCh >= 0 && ViCh < VI_VIRCHN_NUM_MAX))) {
        aloge("fatal error! invalid ViDev[%d], ViVirChn[%d]!", ViDev, ViCh);
        return ERR_VI_INVALID_CHNID;
    }
    if (SUCCESS != videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode)) {
        aloge("vichn[%d] is unexist!!\n", ViCh);
        return ERR_VI_UNEXIST;
    }
    VI_Params viParams;
    viParams.mDev = ViDev;
    viParams.mChn = ViCh;
    viParams.pstFrameInfo = pFrameInfo;

    eRet = pNode->mViComp->GetState(pNode->mViComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VI_NOT_PERM;
    }
    eRet = pNode->mViComp->SetConfig(pNode->mViComp, COMP_IndexVendorViReleaseFrame, (void *)&viParams);
    return eRet;
}

/*
* ViChnMask: which ViChns you want to grab long exposure pictures.
*/
ERRORTYPE AW_MPI_VI_SetShutterTime(VI_DEV ViDev, VI_CHN ViChnMask, VI_SHUTTIME_CFG_S *pTime)
{
    ERRORTYPE ret;
    VI_CHN ViCh;
    COMP_STATETYPE nState;
    VI_CHN_MAP_S *pNode;

    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) {
        aloge("invalid ViDev[%d]", ViDev);
        return ERR_VI_INVALID_DEVID;
    }

    if (videoInputHw_IsLongShutterBusy(ViDev) && pTime->eShutterMode == VI_SHUTTIME_MODE_NIGHT_VIEW) {
        aloge("Video %d is in long exposure mode now, return busy.", ViDev);
        return ERR_VI_BUSY;
    }

    ret = videoInputHw_SetVippShutterTime(ViDev, pTime);
    if (ret != SUCCESS) {
        aloge("Set video shutter time failed.");
        goto exit;
    }

    if (pTime->eShutterMode == VI_SHUTTIME_MODE_PREVIEW)
        goto exit;

    for (ViCh = 0; ViCh < VI_VIRCHN_NUM_MAX; ViCh++) {
        if (!(ViChnMask & (1 << ViCh)))
            continue;
        if (SUCCESS != videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode))
            continue;
        ret = pNode->mViComp->GetState(pNode->mViComp, &nState);
        if (!ret && COMP_StateExecuting != nState && COMP_StateIdle != nState) {
            aloge("Channel %d is in wrong state %d when setting shutter time.", ViCh, ret);
            continue;
        }

        /* we do not care if this invoke is success or not, but you will know if it is success */
        ret = pNode->mViComp->SetConfig(pNode->mViComp, COMP_IndexVendorViSetLongExp, (void *)pTime);
        if (ret < 0)
            aloge("Set shutter for channel %d failed.", ViCh);
        else
            aloge("Set shutter for channel %d success", ViCh);
    }

exit:
    return ret;
}

ERRORTYPE AW_MPI_VI_Debug_StoreFrame(VI_DEV ViDev, VI_CHN ViCh, const char* pDirPath)
{
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    VI_CHN_MAP_S *pNode;

    if ((!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) ||
        (!(ViCh >= 0 && ViCh < VI_VIRCHN_NUM_MAX))) {
        aloge("fatal error! invalid ViDev[%d], ViVirChn[%d]!", ViDev, ViCh);
        return ERR_VI_INVALID_CHNID;
    }
    if (SUCCESS != videoInputHw_searchExistDevVirChn(ViDev, ViCh, &pNode)) {
        aloge("vichn[%d] is unexist!!\n", ViCh);
        return ERR_VI_UNEXIST;
    }
    if(NULL == pDirPath) {
        alogd("must set a directory path! return now");
        return ERR_VI_INVALID_PARA;
    }
    ret = COMP_GetState(pNode->mViComp, &nState);
    if (COMP_StateExecuting != nState && COMP_StateIdle != nState) {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_VI_NOT_PERM;
    }
    ret = COMP_SetConfig(pNode->mViComp, COMP_IndexVendorViStoreFrame, (void*)pDirPath);
    return ret;
}

ERRORTYPE AW_MPI_VI_RegisterCallback(VI_DEV ViDev, MPPCallbackInfo *pCallback)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX)) 
    {
        aloge("invalid ViDev[%d]", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    return videoInputHw_RegisterCallback(ViDev, pCallback->cookie, pCallback->callback);
}

ERRORTYPE AW_MPI_VI_GetIspDev(VI_DEV ViDev, ISP_DEV *pIspDev)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    if (!pIspDev) {
        aloge("fatal error! ViDev[%d] invalid input param!", ViDev);
        return ERR_VI_INVALID_PARA;
    }
    return videoInputHw_GetIspDev(ViDev, pIspDev);
}

ERRORTYPE AW_MPI_VI_SetCrop(VI_DEV ViDev, const VI_CROP_CFG_S *pCropCfg)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    if (!pCropCfg) {
        aloge("fatal error! ViDev[%d] invalid input param!", ViDev);
        return ERR_VI_INVALID_PARA;
    }
    return videoInputHw_SetCrop(ViDev, pCropCfg);
}

ERRORTYPE AW_MPI_VI_GetCrop(VI_DEV ViDev, VI_CROP_CFG_S *pCropCfg)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    if (!pCropCfg) {
        aloge("fatal error! ViDev[%d] invalid input param!", ViDev);
        return ERR_VI_INVALID_PARA;
    }
    return videoInputHw_GetCrop(ViDev, pCropCfg);
}

ERRORTYPE AW_MPI_VI_SetSyncCtrl(VI_DEV ViDev, const struct csi_sync_ctrl *pSync)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    if (!pSync) {
        aloge("fatal error! ViDev[%d] invalid input param!", ViDev);
        return ERR_VI_INVALID_PARA;
    }
    return videoInputHw_SetSyncCtrl(ViDev, pSync);
}

/**
  Used to set sensor raw bit width start for AI-ISP.

  @param bitwidth
    BX_TO_CLOSE: means do not change sensor raw bit width.
    B10_TO_B8: means set sensor raw 8bit.
    B8_TO_B10: means set sensor raw 10bit.
  @return
    0: success
    others: fail
*/
ERRORTYPE AW_MPI_VI_SetInputBitWidthStart(VI_DEV ViDev, const enum set_bit_width bitwidth)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    return videoInputHw_SetInputBitWidthStart(ViDev, bitwidth);
}

/**
  Used to set sensor raw bit width stop for AI-ISP.

  @param bitwidth
    BX_TO_CLOSE: means do not change sensor raw bit width.
    B10_TO_B8: means set sensor raw 8bit.
    B8_TO_B10: means set sensor raw 10bit.
  @return
    0: success
    others: fail
*/
ERRORTYPE AW_MPI_VI_SetInputBitWidthStop(VI_DEV ViDev, const enum set_bit_width bitwidth)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    return videoInputHw_SetInputBitWidthStop(ViDev, bitwidth);
}

/**
  Used to set vipp drop frame num.

  @param drop_num
    means vipp drop frame num.
  @return
    0: success
    others: fail
*/
ERRORTYPE AW_MPI_VI_SetDropFrame(VI_DEV ViDev, const unsigned int drop_num)
{
    if (!(ViDev >= 0 && ViDev < VI_VIPP_NUM_MAX))
    {
        aloge("fatal error! invalid ViDev[%d]!", ViDev);
        return ERR_VI_INVALID_DEVID;
    }
    return videoInputHw_SetDropFrame(ViDev, drop_num);
}

