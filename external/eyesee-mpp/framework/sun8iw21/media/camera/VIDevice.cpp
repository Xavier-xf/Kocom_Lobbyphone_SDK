/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VIDevice.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/06
  Last Modified :
  Description   : camera wrap MPP components.
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "VIDevice"
#include <utils/plat_defines.h>
#include <utils/plat_log.h>

#include <stdlib.h>
#include <memory.h>
#include <stdbool.h>
#include <mpi_vi.h>
#include <mpi_awb.h>
#include <mpi_isp.h>
#include <mpi_region.h>
#include <type_camera.h>

#include "VIChannel.h"
#include "VIDevice.h"


#define SUPPORT_ISP

using namespace std;
namespace EyeseeLinux {

VIDevice::VIDevice(int cameraId, CameraInfo *pCameraInfo)
    : mVIDeviceState(VI_STATE_CONSTRUCTED)
    , mCameraId(cameraId)
{
    alogd("Construct");
    mCameraInfo = *pCameraInfo;
    for (ISPGeometry &n : mCameraInfo.mMPPGeometry.mISPGeometrys)
    {
        mParamsOfISPs[n.mISPDev] = {};
    }
    //memset(&mDevAttr, 0, sizeof(mDevAttr));
    mbIspRun = false;
    //mbRefChannelPrepared = false;
}

VIDevice::~VIDevice()
{
    alogd("Destruct");
    ERRORTYPE ret;
    size_t num = mRgnHandleList.size();
    if(num > 0)
    {
        alogw("Be careful! There are [%d]regions need to destroy!", num);
        for(RGN_HANDLE& i : mRgnHandleList)
        {
            ret = AW_MPI_RGN_Destroy(i);
            if(SUCCESS != ret)
            {
                aloge("fatal error! destroy region[%d] fail!", i);
            }
        }
    }
}

VIChannel *VIDevice::searchVIChannel(int chnId)
{
    AutoMutex lock(mVIChannelVectorLock);
    for (vector<VIChannelInfo>::iterator it = mVIChannelVector.begin(); it != mVIChannelVector.end(); ++it) {
        if (it->mChnId == chnId) {
            return it->mpChannel;
        }
    }
    return NULL;
}

status_t VIDevice::setChannelDisplay(int chnId, int hlay)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setPreviewDisplay(hlay);
}

status_t VIDevice::changeChannelDisplay(int chnId, int hlay)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->changePreviewDisplay(hlay);
}

bool VIDevice::previewEnabled(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return false;
    }
    return pChannel->isPreviewEnabled();
}

status_t VIDevice::startRender(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startRender();
}

status_t VIDevice::stopRender(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopRender();
}

status_t VIDevice::pauseRender(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->pauseRender();
}

status_t VIDevice::resumeRender(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->resumeRender();
}

status_t VIDevice::storeDisplayFrame(int chnId, uint64_t framePts)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->storeDisplayFrame(framePts);
}

/*
status_t VIDevice::setDeviceAttr(VI_DEV_ATTR_S *devAttr)
{
    mDevAttr = *devAttr;
    return NO_ERROR;
}

status_t VIDevice::getDeviceAttr(VI_DEV_ATTR_S *devAttr)
{
    *devAttr = mDevAttr;
    return NO_ERROR;
}
*/

status_t VIDevice::setParameters(int chnId, CameraParameters &param)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        alogd("channel %d is not exist!", chnId);
        return NO_INIT;
    }

    return pChannel->setParameters(param);
}

void VIDevice::increaseBufRef(int chnId, VIDEO_FRAME_BUFFER_S *pBuf)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL){
        alogd("channel %d is not exist!", chnId);
        return ;
    }

    return pChannel->increaseBufRef(pBuf);
}

status_t VIDevice::setPicCapMode(int chnId, uint32_t cap_mode_en)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        alogd("channel %d is not exist!", chnId);
        return NO_INIT;
    }

    pChannel->setPicCapMode(cap_mode_en);
    return NO_ERROR;
}

status_t VIDevice::setFrmDrpThrForPicCapMode(int chnId,uint32_t frm_cnt)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        alogd("channel %d is not exist!", chnId);
        return NO_INIT;
    }

    pChannel->setFrmDrpThrForPicCapMode(frm_cnt);
    return NO_ERROR;
} 


status_t VIDevice::getParameters(int chnId, CameraParameters &param)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        alogd("channel %d is not exist!", chnId);
        return NO_INIT;
    }

    return pChannel->getParameters(param);
}

status_t VIDevice::setISPParameters(ISP_DEV nIspDev, CameraParameters &param)
{
    status_t ret = NO_ERROR;
//    if(mVIDeviceState!=VI_STATE_PREPARED && mVIDeviceState!=VI_STATE_STARTED)
//    {
//        alogw("set isp parameters in wrong state[0x%x]", mVIDeviceState);
//        return INVALID_OPERATION;
//    }
    CameraParameters *pISPParameters = NULL;

    auto search = mParamsOfISPs.find(nIspDev);
    if (search != mParamsOfISPs.end())
    {
        pISPParameters = &search->second;
    }
    else
    {
        aloge("fatal error! ispDev[%d] wrong, can't get ISP parameters!", nIspDev);
        return UNKNOWN_ERROR;
    }
    Mutex::Autolock lock(mVIChannelVectorLock);
    bool bCanSet = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(VIChannel::VI_CHN_STATE_STARTED == i.mpChannel->getState())
        {
            bCanSet = true;
            break;
        }
    }
    if(bCanSet)
    {
      #if (defined(SUPPORT_ISP))

        printf("[FUN]:%s mIspDevId:%d \n", __FUNCTION__, nIspDev);

        //detect param change, and update.
        //ModuleOnOff
        ISP_MODULE_ONOFF& curModuleOnOff = pISPParameters->getModuleOnOff();
        ISP_MODULE_ONOFF& newModuleOnOff = param.getModuleOnOff();
        if(memcmp(&curModuleOnOff, &newModuleOnOff, sizeof(ISP_MODULE_ONOFF)))
        {
            alogd("isp[%d] change ISP_ModuleOnOff", nIspDev);
            AW_MPI_ISP_SetModuleOnOff(nIspDev, &newModuleOnOff);
        }
        //AE mode
        int curAEMode = pISPParameters->ChnIspAe_GetMode();
        int newAEMode = param.ChnIspAe_GetMode();
        if(curAEMode!= newAEMode)
        {
            alogd("isp[%d] change AE mode", nIspDev);
            AW_MPI_ISP_AE_SetMode(nIspDev, newAEMode);
        }
        //AE exposure bias
        int curAEExposureBias = pISPParameters->ChnIspAe_GetExposureBias();
        int newAEExposureBias = param.ChnIspAe_GetExposureBias();
        if(curAEExposureBias != newAEExposureBias)
        {
            alogd("isp[%d] change AE Exposure Bias", nIspDev);
            AW_MPI_ISP_AE_SetExposureBias(nIspDev, newAEExposureBias);
        }
        //AE exposure
        int curAEExposure = pISPParameters->ChnIspAe_GetExposure();
        int newAEExposure = param.ChnIspAe_GetExposure();
        if(curAEExposure != newAEExposure)
        {
            alogd("isp[%d] change AE exposure", nIspDev);
            AW_MPI_ISP_AE_SetExposure(nIspDev, newAEExposure);
        }
        //AE gain
        int curAEGain = pISPParameters->ChnIspAe_GetGain();
        int newAEGain = param.ChnIspAe_GetGain();
        if(curAEGain != newAEGain)
        {
            alogd("isp[%d] change AE Gain", nIspDev);
            AW_MPI_ISP_AE_SetGain(nIspDev, newAEGain);
        }
        //AE ISOSensitive
        int curAEISOSensitive = pISPParameters->ChnIspAe_GetISOSensitive();
        int newAEISOSensitive = param.ChnIspAe_GetISOSensitive();
        if(curAEISOSensitive != newAEISOSensitive)
        {
            alogd("isp[%d] change AE ISOSensitive[%d]->[%d]", nIspDev, curAEISOSensitive, newAEISOSensitive);
            AW_MPI_ISP_AE_SetISOSensitive(nIspDev, newAEISOSensitive);
        }
        //AE Metering
        int curMetering = pISPParameters->ChnIspAe_GetMetering();
        int newMetering = param.ChnIspAe_GetMetering();
        if(curMetering != newMetering)
        {
            alogd("isp[%d] change AE curMetering[%d]->[%d]", nIspDev, curMetering, newMetering);
            AW_MPI_ISP_AE_SetMetering(nIspDev, newMetering);
        }

        //AWB Mode(0-1,2-7)
        int curAwbMode = pISPParameters->ChnIspAwb_GetMode();
        int newAwbMode = param.ChnIspAwb_GetMode();
        if(curAwbMode != newAwbMode)
        {
            if (newAwbMode==0 || newAwbMode==1)
            {
                AW_MPI_ISP_AWB_SetMode(nIspDev, newAwbMode);
            }
            else
            {
                AW_MPI_ISP_AWB_SetColorTemp(nIspDev, newAwbMode);
            }
        }
        //AWB RGain
        int curAwbRGain= pISPParameters->ChnIspAwb_GetRGain();
        int newAwbRGain = param.ChnIspAwb_GetRGain();
        if(curAwbRGain != newAwbRGain)
        {
            alogd("isp[%d] change AWB RGain", nIspDev);
            AW_MPI_ISP_AWB_SetRGain(nIspDev, newAwbRGain);
        }
        //AWB GrGain
        int curAwbGrGain= pISPParameters->ChnIspAwb_GetGrGain();
        int newAwbGrGain = param.ChnIspAwb_GetGrGain();
        if(curAwbGrGain != newAwbGrGain)
        {
            alogd("isp[%d] change AWB GrGain", nIspDev);
            AW_MPI_ISP_AWB_SetGrGain(nIspDev, newAwbGrGain);
        }
        //AWB GbGain
        int curAwbGbGain= pISPParameters->ChnIspAwb_GetGbGain();
        int newAwbGbGain = param.ChnIspAwb_GetGbGain();
        if(curAwbGbGain != newAwbGbGain)
        {
            alogd("isp[%d] change AWB GbGain", nIspDev);
            AW_MPI_ISP_AWB_SetGbGain(nIspDev, newAwbGbGain);
        }
        //AWB BGain
        int curAwbBGain= pISPParameters->ChnIspAwb_GetBGain();
        int newAwbBGain = param.ChnIspAwb_GetBGain();
        if(curAwbBGain != newAwbBGain)
        {
            alogd("isp[%d] change AWB BGain", nIspDev);
            AW_MPI_ISP_AWB_SetBGain(nIspDev, newAwbBGain);
        }
        //Flicker
        int curFlicker = pISPParameters->ChnIsp_GetFlicker();
        int newFlicker = param.ChnIsp_GetFlicker();
        if(curFlicker != newFlicker)
        {
            alogd("isp[%d] change flicker", nIspDev);
            AW_MPI_ISP_SetFlicker(nIspDev, newFlicker);
        }
        //brightness
        int curBrightness = pISPParameters->ChnIsp_GetBrightness();
        int newBrightness = param.ChnIsp_GetBrightness();
        if(curBrightness != newBrightness)
        {
            alogd("isp[%d] change brightness", nIspDev);
            AW_MPI_ISP_SetBrightness(nIspDev, newBrightness);
        }
        //contrast
        int curContrast = pISPParameters->ChnIsp_GetContrast();
        int newContrast = param.ChnIsp_GetContrast();
        if(curContrast != newContrast)
        {
            alogd("isp[%d] change contrast", nIspDev);
            AW_MPI_ISP_SetContrast(nIspDev, newContrast);
        }
        //saturation
        int curSaturation = pISPParameters->ChnIsp_GetSaturation();
        int newSaturation = param.ChnIsp_GetSaturation();
        if(curSaturation != newSaturation)
        {
            alogd("isp[%d] change saturation", nIspDev);
            AW_MPI_ISP_SetSaturation(nIspDev, newSaturation);
        }
        //sharpness
        int curSharpness = pISPParameters->ChnIsp_GetSharpness();
        int newSharpness = param.ChnIsp_GetSharpness();
        if(curSharpness != newSharpness)
        {
            alogd("isp[%d] change sharpness", nIspDev);
            AW_MPI_ISP_SetSharpness(nIspDev, newSharpness);
        }
        //hue
        /*int curHue = mISPParameters.ChnIsp_GetHue();
        int newHue = param.ChnIsp_GetHue();
        if(curHue != newHue)
        {
            alogd("change hue");
            AW_MPI_ISP_SetHue(mIspDevId, newHue);
        }*/
        enum ae_table_mode curScene = pISPParameters->ChnIsp_GetScene();
        enum ae_table_mode newScene = param.ChnIsp_GetScene();
        if (curScene != newScene)
        {
            alogd("isp[%d] change Scene[%d->%d]", nIspDev, curScene, newScene);
            ret = AW_MPI_ISP_SetScene(nIspDev, newScene);
            if (ret != SUCCESS)
            {
                aloge("fatal error! isp[%d] set Scene fail[0x%x]", nIspDev, ret);
            }
        }
        enum colorfx curColorEffect = pISPParameters->ChnIsp_GetColorEffect();
        enum colorfx newColorEffect = param.ChnIsp_GetColorEffect();
        if (curColorEffect != newColorEffect)
        {
            alogd("isp[%d] change ColorEffect[%d->%d]", nIspDev, curColorEffect, newColorEffect);
            ret = AW_MPI_ISP_SetColorEffect(nIspDev, newColorEffect);
            if (ret != SUCCESS)
            {
                aloge("fatal error! isp[%d] change ColorEffect fail[0x%x]", nIspDev, ret);
            }
        }
        scene_mode_t curSpecialScene = pISPParameters->ChnIsp_GetSpecialScene();
        scene_mode_t newSpecialScene = param.ChnIsp_GetSpecialScene();
        if (curSpecialScene != newSpecialScene)
        {
            alogd("isp[%d] change SpecialScene[%d->%d]", nIspDev, curSpecialScene, newSpecialScene);
            ret = AW_MPI_ISP_SetSpecialScene(nIspDev, newSpecialScene);
            if (ret != SUCCESS)
            {
                aloge("fatal error! isp[%d] change SpecialScene fail[0x%x]", nIspDev, ret);
            }
        }

        //INI ISP config
        int curNRAttr = pISPParameters->getNRAttrValue();
        int newNRAttr = param.getNRAttrValue();
        if(curNRAttr != newNRAttr)
        {
            alogd("isp[%d] change ISP_NR_ATTR", nIspDev);
            AW_MPI_ISP_SetNRAttr(nIspDev, newNRAttr);
        }
        int cur3NRAttr = pISPParameters->get3NRAttrValue();
        int new3NRAttr = param.get3NRAttrValue();
        if(cur3NRAttr != new3NRAttr)
        {
            alogd("isp[%d] change ISP_3NR_ATTR", nIspDev);
            AW_MPI_ISP_Set3NRAttr(nIspDev, new3NRAttr);
        }

        //WDR
        int curPltmWDR = pISPParameters->getPltmWDR();
        int newPltmWDR = param.getPltmWDR();
        if(curPltmWDR != newPltmWDR)
        {
            alogd("isp[%d] change ISP_PltmWDR", nIspDev);
            AW_MPI_ISP_SetPltmWDR(nIspDev, newPltmWDR);
        }

      #endif
    }
    *pISPParameters = param;
    return NO_ERROR;
}

status_t VIDevice::getISPParameters(ISP_DEV nIspDev, CameraParameters &param)
{
    status_t ret = NO_ERROR;

    auto search = mParamsOfISPs.find(nIspDev);
    if (search != mParamsOfISPs.end())
    {
        param = search->second;
    }
    else
    {
        aloge("fatal error! ispDev[%d] wrong, can't get ISP parameters!", nIspDev);
        ret = UNKNOWN_ERROR;
    }

    return ret;
}
/*
status_t VIDevice::setOSDRects(int chnId, std::list<OSDRectInfo> &rects)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("fatal error! VIPP channel[%d] is not exist!", chnId);
        return UNKNOWN_ERROR;
    }
    return pChannel->setOSDRects(rects);
}

status_t VIDevice::getOSDRects(int chnId, std::list<OSDRectInfo> **ppRects)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("fatal error! VIPP channel[%d] is not exist!", chnId);
        return UNKNOWN_ERROR;
    }
    return pChannel->getOSDRects(ppRects);
}

status_t VIDevice::OSDOnOff(int chnId, bool bOnOff)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("fatal error! VIPP channel[%d] is not exist!", chnId);
        return UNKNOWN_ERROR;
    }
    return pChannel->OSDOnOff(bOnOff);
}
*/
RGN_HANDLE VIDevice::createRegion(const RGN_ATTR_S *pstRegion)
{
    Mutex::Autolock lock(mRgnLock);
    ERRORTYPE ret;
    bool bSuccess = false;
    RGN_HANDLE handle = 0;
    while(handle < RGN_HANDLE_MAX)
    {
        ret = AW_MPI_RGN_Create(handle, pstRegion);
        if(SUCCESS == ret)
        {
            bSuccess = true;
            alogd("create region[%d] success!", handle);
            break;
        }
        else if(ERR_RGN_EXIST == ret)
        {
            alogv("region[%d] is exist, find next!", handle);
            handle++;
        }
        else
        {
            aloge("fatal error! create region[%d] ret[0x%x]!", handle, ret);
            break;
        }
    }
    if(bSuccess)
    {
        mRgnHandleList.push_back(handle);
        return handle;
    }
    else
    {
        alogd("create region fail");
        return MM_INVALID_HANDLE;
    }
}

status_t VIDevice::getRegionAttr(RGN_HANDLE Handle, RGN_ATTR_S *pstRgnAttr)
{
    Mutex::Autolock lock(mRgnLock);
    status_t result = NO_ERROR;
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        ERRORTYPE ret = AW_MPI_RGN_GetAttr(Handle, pstRgnAttr);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        result = UNKNOWN_ERROR;
    }
    return result;
}

status_t VIDevice::setRegionBitmap(RGN_HANDLE Handle, const BITMAP_S *pBitmap)
{
    Mutex::Autolock lock(mRgnLock);
    status_t result = NO_ERROR;
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        ERRORTYPE ret = AW_MPI_RGN_SetBitMap(Handle, pBitmap);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        result = UNKNOWN_ERROR;
    }
    return result;
}

status_t VIDevice::attachRegionToChannel(RGN_HANDLE Handle, int chnId, const RGN_CHN_ATTR_S *pstChnAttr)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mVIChannelVectorLock);
    VIChannel::VIChannelState chnState;
    bool bChannelExist = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(i.mChnId == chnId)
        {
            chnState = i.mpChannel->getState();
            bChannelExist = true;
            break;
        }
    }
    if(false == bChannelExist)
    {
        aloge("fatal error! wrong camera vipp[%d]", chnId);
        return UNKNOWN_ERROR;
    }
    if(chnState != VIChannel::VI_CHN_STATE_STARTED)
    {
        aloge("fatal error! vipp state[%d] is not started, can't attach region!", chnState);
        return UNKNOWN_ERROR;
    }
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        MPP_CHN_S stChn = {MOD_ID_VIU, chnId, 0};
        ERRORTYPE ret = AW_MPI_RGN_AttachToChn(Handle, &stChn, pstChnAttr);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}
status_t VIDevice::detachRegionFromChannel(RGN_HANDLE Handle, int chnId)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mVIChannelVectorLock);
    bool bChannelExist = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(i.mChnId == chnId)
        {
            bChannelExist = true;
            break;
        }
    }
    if(false == bChannelExist)
    {
        aloge("fatal error! wrong camera vipp[%d]", chnId);
        return UNKNOWN_ERROR;
    }
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        MPP_CHN_S stChn = {MOD_ID_VIU, chnId, 0};
        ERRORTYPE ret = AW_MPI_RGN_DetachFromChn(Handle, &stChn);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}
status_t VIDevice::setRegionDisplayAttr(RGN_HANDLE Handle, int chnId, const RGN_CHN_ATTR_S *pstChnAttr)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mVIChannelVectorLock);
    bool bChannelExist = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(i.mChnId == chnId)
        {
            bChannelExist = true;
            break;
        }
    }
    if(false == bChannelExist)
    {
        aloge("fatal error! wrong camera vipp[%d]", chnId);
        return UNKNOWN_ERROR;
    }
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        MPP_CHN_S stChn = {MOD_ID_VIU, chnId, 0};
        ERRORTYPE ret = AW_MPI_RGN_SetDisplayAttr(Handle, &stChn, pstChnAttr);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}
status_t VIDevice::getRegionDisplayAttr(RGN_HANDLE Handle, int chnId, RGN_CHN_ATTR_S *pstChnAttr)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mVIChannelVectorLock);
    bool bChannelExist = false;
    for(VIChannelInfo& i : mVIChannelVector)
    {
        if(i.mChnId == chnId)
        {
            bChannelExist = true;
            break;
        }
    }
    if(false == bChannelExist)
    {
        aloge("fatal error! wrong camera vipp[%d]", chnId);
        return UNKNOWN_ERROR;
    }
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    for(RGN_HANDLE& i : mRgnHandleList)
    {
        if(i == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        MPP_CHN_S stChn = {MOD_ID_VIU, chnId, 0};
        ERRORTYPE ret = AW_MPI_RGN_GetDisplayAttr(Handle, &stChn, pstChnAttr);
        if(SUCCESS == ret)
        {
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}
status_t VIDevice::destroyRegion(RGN_HANDLE Handle)
{
    status_t result = NO_ERROR;
    Mutex::Autolock lock(mRgnLock);
    bool bExist = false;
    std::list<RGN_HANDLE>::iterator it;
    for(it = mRgnHandleList.begin(); it != mRgnHandleList.end(); ++it)
    {
        if(*it == Handle)
        {
            bExist = true;
            break;
        }
    }
    if(bExist)
    {
        ERRORTYPE ret = AW_MPI_RGN_Destroy(Handle);
        if(SUCCESS == ret)
        {
            mRgnHandleList.erase(it);
            result = NO_ERROR;
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else
    {
        aloge("fatal error! region[%d] is unexist!", Handle);
        result = UNKNOWN_ERROR;
    }
    return result;
}

/**
  get actual initial value for some isp params.
*/
status_t VIDevice::initIspParameters(ISP_DEV nIspDev)
{
#if 0
    //brightness
    int defaultBrightness;
    AW_MPI_ISP_GetBrightness(mIspDevId, &defaultBrightness);
    //contrast
    int defaultContrast;
    AW_MPI_ISP_GetContrast(mIspDevId, &defaultContrast);
    //saturation
    int defaultSaturation;
    AW_MPI_ISP_GetSaturation(mIspDevId, &defaultSaturation);
    //hue
//    int defaultHue;
//    AW_MPI_ISP_GetHue(mIspDevId, &defaultHue);
    //AE mode
    int defaultAEMode;
    AW_MPI_ISP_AE_GetMode(mIspDevId, &defaultAEMode);
    //AE exposure bias
    int defaultAEExposureBias;
    AW_MPI_ISP_AE_GetExposureBias(mIspDevId, &defaultAEExposureBias);
    //AE exposure
    int defaultAEExposure;
    AW_MPI_ISP_AE_GetExposure(mIspDevId, &defaultAEExposure);
    //AE gain
    int defaultAEGain;
    AW_MPI_ISP_AE_GetGain(mIspDevId, &defaultAEGain);
    //AE ISOSensitive
    int defaultAEISOSensitive = -1;
    AW_MPI_ISP_AE_GetISOSensitive(mIspDevId, &defaultAEISOSensitive);
    //AE Metering
    int defaultAEMetering = -1;
    AW_MPI_ISP_AE_GetMetering(mIspDevId, &defaultAEMetering);

    //AWB mode
    int defaultAwbMode;
    AW_MPI_ISP_AWB_GetMode(mIspDevId, &defaultAwbMode);
    //AWB color temp
    int defaultColorTemp;
    AW_MPI_ISP_AWB_GetColorTemp(mIspDevId, &defaultColorTemp);
    //AWB color RGain
    int defaultColorRGain = -1;
    AW_MPI_ISP_AWB_GetRGain(mIspDevId, &defaultColorRGain);
    //AWB color BGain
    int defaultColorBGain = -1;
    AW_MPI_ISP_AWB_GetBGain(mIspDevId, &defaultColorBGain);
    //flicker
    int defaultFlicker = -1;
    AW_MPI_ISP_GetFlicker(mIspDevId, &defaultFlicker);

    //sharpness
    int defaultSharpness = -1;
    AW_MPI_ISP_GetSharpness(mIspDevId, &defaultSharpness);
    //detailed AWB param setting
    //ISP_WB_ATTR_S defaultWBAttr;
    //AW_MPI_ISP_AWB_GetWBAttr(mIspDevId, &defaultWBAttr);
//    ISP_COLORMATRIX_ATTR_S defaultCMAttr;
//    AW_MPI_ISP_AWB_GetCCMAttr(mIspDevId, 0, &defaultCMAttr);
//    ISP_AWB_SPEED_S defaultWBSpeed;
//    AW_MPI_ISP_AWB_GetSpeed(mIspDevId, &defaultWBSpeed);
//    ISP_AWB_TEMP_RANGE_S defaultWBTempRange;
//    AW_MPI_ISP_AWB_GetTempRange(mIspDevId, &defaultWBTempRange);
//    std::map<int, ISP_AWB_TEMP_INFO_S> defaultWBLights;
//    int nLightMode;
//    ISP_AWB_TEMP_INFO_S defaultWBTempInfo;
//    nLightMode = 0x01;
//    AW_MPI_ISP_AWB_GetLight(mIspDevId, nLightMode, &defaultWBTempInfo);
//    defaultWBLights[nLightMode] = defaultWBTempInfo;
//    nLightMode = 0x02;
//    AW_MPI_ISP_AWB_GetLight(mIspDevId, nLightMode, &defaultWBTempInfo);
//    defaultWBLights[nLightMode] = defaultWBTempInfo;
//    nLightMode = 0x03;
//    AW_MPI_ISP_AWB_GetLight(mIspDevId, nLightMode, &defaultWBTempInfo);
//    defaultWBLights[nLightMode] = defaultWBTempInfo;
//    nLightMode = 0x04;
//    AW_MPI_ISP_AWB_GetLight(mIspDevId, nLightMode, &defaultWBTempInfo);
//    defaultWBLights[nLightMode] = defaultWBTempInfo;
//    ISP_AWB_FAVOR_S defaultWBFavor;
//    AW_MPI_ISP_AWB_GetFavor(mIspDevId, &defaultWBFavor);

    mISPParameters.ChnIspAe_SetMode(defaultAEMode);
    mISPParameters.ChnIspAe_SetExposureBias(defaultAEExposureBias);
    mISPParameters.ChnIspAe_SetExposure(defaultAEExposure);
    mISPParameters.ChnIspAe_SetGain(defaultAEGain);
    mISPParameters.ChnIspAe_SetISOSensitive(defaultAEISOSensitive);
    mISPParameters.ChnIspAe_SetMetering(defaultAEMetering);
    mISPParameters.ChnIspAwb_SetMode(defaultAwbMode);
    mISPParameters.ChnIspAwb_SetColorTemp(defaultColorTemp);
    mISPParameters.ChnIsp_SetFlicker(defaultFlicker);
    mISPParameters.ChnIsp_SetBrightness(defaultBrightness);
    mISPParameters.ChnIsp_SetContrast(defaultContrast);
    mISPParameters.ChnIsp_SetSaturation(defaultSaturation);
    mISPParameters.ChnIsp_SetSharpness(defaultSharpness);
    //mISPParameters.ChnIsp_SetHue(defaultHue);
#else
    CameraParameters *pISPParameters = NULL;
    auto search = mParamsOfISPs.find(nIspDev);
    if (search != mParamsOfISPs.end())
    {
        pISPParameters = &search->second;
    }
    else
    {
        aloge("fatal error! ispDev[%d] wrong, can't get ISP parameters!", nIspDev);
    }
//    pISPParameters->ChnIspAe_SetMode(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIspAe_SetExposureBias(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIspAe_SetExposure(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIspAe_SetGain(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIspAe_SetISOSensitive(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIspAe_SetMetering(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIspAwb_SetMode(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIspAwb_SetColorTemp(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIsp_SetFlicker(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIsp_SetBrightness(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIsp_SetContrast(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIsp_SetSaturation(ILLEGAL_ISP_PARAM);
//    pISPParameters->ChnIsp_SetSharpness(ILLEGAL_ISP_PARAM);
    //pISPParameters->ChnIsp_SetHue(ILLEGAL_ISP_PARAM); 

    ERRORTYPE ret;
    //INI ISP config
    ISP_MODULE_ONOFF defaultModuleOnOff;
    AW_MPI_ISP_GetModuleOnOff(nIspDev, &defaultModuleOnOff);
    pISPParameters->setModuleOnOff(defaultModuleOnOff);
    int defaultNRAttrValue = -1;
    ret = AW_MPI_ISP_GetNRAttr(nIspDev, &defaultNRAttrValue);
    if (SUCCESS == ret)
    {
        alogd("isp[%d] get NRAttr:%d", nIspDev, defaultNRAttrValue);
        pISPParameters->setNRAttrValue(defaultNRAttrValue);
    }
    else
    {
        aloge("fatal error! isp[%d] get NRAttr fail[0x%x]", nIspDev, ret);
    }
    int default3NRAttrValue = -1;
    ret = AW_MPI_ISP_Get3NRAttr(nIspDev, &default3NRAttrValue);
    if (SUCCESS == ret)
    {
        alogd("isp[%d] get 3NRAttr:%d", nIspDev, default3NRAttrValue);
        pISPParameters->set3NRAttrValue(default3NRAttrValue);
    }
    else
    {
        aloge("fatal error! isp[%d] get 3NRAttr fail[0x%x]", nIspDev, ret);
    }
    int defaultPltmWDR = -1;
    ret = AW_MPI_ISP_GetPltmWDR(nIspDev, &defaultPltmWDR);
    if (SUCCESS == ret)
    {
        alogd("isp[%d] get PltmWDR:%d", nIspDev, defaultPltmWDR);
        pISPParameters->setPltmWDR(defaultPltmWDR);
    }
    else
    {
        aloge("fatal error! isp[%d] get PltmWDR fail[0x%x]", nIspDev, ret);
    }

//    mISPParameters.setAWB_WBAttrValue(defaultWBAttr);
//    mISPParameters.setAWB_CCMAttrValue(defaultCMAttr);
//    mISPParameters.setAWB_SpeedValue(defaultWBSpeed);
//    mISPParameters.setAWB_TempRangeValue(defaultWBTempRange);
//    mISPParameters.setAWB_LightValues(defaultWBLights);
//    mISPParameters.setAWB_FavorValue(defaultWBFavor);
//    mISPParameters.setFlickerValue(defaultFlicker);   
#endif
    return NO_ERROR;
}
status_t VIDevice::prepareDevice()
{
    int ret;

    if (mVIDeviceState != VI_STATE_CONSTRUCTED)
    {
        aloge("prepareDevice in error state %d", mVIDeviceState);
        return INVALID_OPERATION;
    }
    if(CameraInfo::CAMERA_CSI == mCameraInfo.mCameraDeviceType)
    {
    }
    else if(CameraInfo::CAMERA_USB == mCameraInfo.mCameraDeviceType)
    {
        aloge("need implement");
    }
    else
    {
        aloge("unsupported temporary");
    }
    mVIDeviceState = VI_STATE_PREPARED;
    return NO_ERROR;
}

status_t VIDevice::releaseDevice()
{
    if (mVIDeviceState != VI_STATE_PREPARED)
    {
        aloge("releaseDevice in error state %d", mVIDeviceState);
        return INVALID_OPERATION;
    }
    if(CameraInfo::CAMERA_CSI == mCameraInfo.mCameraDeviceType)
    {
    }
    else if(CameraInfo::CAMERA_USB == mCameraInfo.mCameraDeviceType)
    {
        aloge("need implement");
    }
    else
    {
        aloge("unsupported temporary");
    }
    mVIDeviceState = VI_STATE_CONSTRUCTED;
    return NO_ERROR;
}

status_t VIDevice::startDevice()
{
    if (mVIDeviceState != VI_STATE_PREPARED)
    {
        aloge("startDevice in error state %d", mVIDeviceState);
        return INVALID_OPERATION;
    }
    if(CameraInfo::CAMERA_CSI == mCameraInfo.mCameraDeviceType)
    {
    }
    else if(CameraInfo::CAMERA_USB == mCameraInfo.mCameraDeviceType)
    {
        aloge("need implement");
    }
    else
    {
        aloge("unsupported temporary");
    }

    mVIDeviceState = VI_STATE_STARTED;
    return NO_ERROR;
}

status_t VIDevice::stopDevice()
{
    status_t eError = NO_ERROR;
    if (mVIDeviceState != VI_STATE_STARTED)
    {
        aloge("stopDevice in error state %d", mVIDeviceState);
        return INVALID_OPERATION;
    }
    int nChnNum = 0;
    AutoMutex lock(mVIChannelVectorLock);
    for (vector<VIChannelInfo>::iterator it = mVIChannelVector.begin(); it != mVIChannelVector.end(); ++it)
    {
        aloge("fatal error! camera[%d] vipp[%d] is exist when stop device!", mCameraId, it->mChnId);
        nChnNum++;
    }
    if(0 == nChnNum)
    {
        mVIDeviceState = VI_STATE_PREPARED;
        eError = NO_ERROR;
    }
    else
    {
        eError = INVALID_OPERATION;
    }
    return eError;
}

ISP_DEV VIDevice::deduceIspForScalerChn(int chnId)
{
    ISP_DEV nIspDevId = MM_INVALID_DEV;
    for(ISPGeometry& ispEntry : mCameraInfo.mMPPGeometry.mISPGeometrys)
    {
        for(VI_DEV& vippIndex : ispEntry.mScalerOutChns)
        {
            if(vippIndex == chnId)
            {
                nIspDevId = ispEntry.mISPDev;
                break;
            }
        }
        if(nIspDevId != MM_INVALID_DEV)
        {
            break;
        }
    }
    return nIspDevId;
}

/**
  If camera is in stitch mode, get final output scaler chn. If camera is in normal mode, return MM_INVALID_DEV.
*/
VI_DEV VIDevice::getStitchOutputScalerChn()
{
    VI_DEV nScalerChn = MM_INVALID_DEV;
    if (mCameraInfo.mStitchMode)
    {
        if (mCameraInfo.mMPPGeometry.mISPGeometrys.size() > 0)
        {
            ISPGeometry &LastIspEntry = mCameraInfo.mMPPGeometry.mISPGeometrys.back();
            if (LastIspEntry.mScalerOutChns.size() > 0)
            {
                nScalerChn = LastIspEntry.mScalerOutChns.back();
            }
            else
            {
                aloge("fatal error! why ISP Geomery contain no scalerChn?");
            }
        }
        else
        {
            aloge("fatal error! why mpp Geomery contain no ISP geometry?");
        }
    }
    else
    {
        aloge("fatal error! camera[%d]: %d is not stitch mode!", mCameraId, mCameraInfo.mStitchMode);
    }
    return nScalerChn;
}

status_t VIDevice::openChannel(int chnId, bool bForceRef)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel != NULL)
    {
        aloge("channel %d is opened!", chnId);
        return INVALID_OPERATION;
    }

    AutoMutex lock(mVIChannelVectorLock);
//    if(bForceRef)
//    {
//        if(!mVIChannelVector.empty())
//        {
//            aloge("fatal error! Some channels are running. ISP sensor param has done. can't open current channel as reference channel");
//            return BAD_VALUE;
//        }
//    }
//    else
//    {
//        if(mVIChannelVector.empty())
//        {
//            aloge("fatal error! first channel must be ref channel! can't open current channel as sub channel!");
//            return BAD_VALUE;
//        }
//    }

    bool bStitchScalerChn = false;
    if (mCameraInfo.mStitchMode)
    {
        VI_DEV nStitchOutputChn = getStitchOutputScalerChn();
        if(chnId == nStitchOutputChn)
        {
            bStitchScalerChn = true;
        }
    }
    VIChannelInfo chnInfo;
    chnInfo.mpChannel = new VIChannel(chnId, bStitchScalerChn);
    chnInfo.mChnId = chnId;
    mVIChannelVector.push_back(chnInfo);
    return NO_ERROR;
}

status_t VIDevice::closeChannel(int chnId)
{
    vector<VIChannelInfo>::iterator it;
    bool found = false;

    {
        AutoMutex lock(mVIChannelVectorLock);
        for (it = mVIChannelVector.begin(); it != mVIChannelVector.end(); ++it)
        {
            if (it->mChnId == chnId) {
                delete it->mpChannel;
                mVIChannelVector.erase(it);
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        aloge("channel %d is not exist!", chnId);
        return INVALID_OPERATION;
    }

    //delete it->mpChannel;

    return NO_ERROR;
}
status_t VIDevice::prepareChannel(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    // if(false == mbRefChannelPrepared)
    // {
    //     if(false == pChannel->mbForceRef)
    //     {
    //         aloge("fatal error! must prepare reference channel first!");
    //         return INVALID_OPERATION;
    //     }
    // }
    status_t ret = pChannel->prepare();
    if (ret != NO_ERROR)
    {
        aloge("prepare channel error!");
        return ret;
    }
//    if(false == mbRefChannelPrepared)
//    {
//        mbRefChannelPrepared = true;
//    }
    return NO_ERROR;
}

status_t VIDevice::releaseChannel(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    AutoMutex lock(mVIChannelVectorLock);
    status_t ret = pChannel->release();
    if(NO_ERROR == ret)
    {
        bool bAllRelease = true;
        for(VIChannelInfo& i : mVIChannelVector)
        {
            if(i.mpChannel->getState() != VIChannel::VI_CHN_STATE_CONSTRUCTED)
            {
                bAllRelease = false;
                break;
            }
        }
        if(bAllRelease)
        {
            //mbRefChannelPrepared = false;
        }
    }
    return ret;
}

/**
  set initial config to isp before isp_init().
*/
status_t VIDevice::setInitialConfigToIsp(ISP_DEV nIspDev)
{
    CameraParameters *pISPParameters = NULL;
    auto search = mParamsOfISPs.find(nIspDev);
    if (search != mParamsOfISPs.end())
    {
        pISPParameters = &search->second;
    }
    else
    {
        aloge("fatal error! can't get ISP[%d] in paramsOfISPs.", nIspDev);
    }
    int curChnIspAe_Mode = pISPParameters->ChnIspAe_GetMode();
    if (curChnIspAe_Mode != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIspAe_Mode[%d]", nIspDev, curChnIspAe_Mode);
        AW_MPI_ISP_Init_AE_SetMode(nIspDev, curChnIspAe_Mode);
    }
    int curChnIspAe_ExposureBias = pISPParameters->ChnIspAe_GetExposureBias();
    if(curChnIspAe_ExposureBias != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIspAe_ExposureBias[%d]", nIspDev, curChnIspAe_ExposureBias);
        AW_MPI_ISP_Init_AE_SetExposureBias(nIspDev, curChnIspAe_ExposureBias);
    }
    int curChnIspAe_Exposure = pISPParameters->ChnIspAe_GetExposure();
    if(curChnIspAe_Exposure != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIspAe_Exposure[%d]", nIspDev, curChnIspAe_Exposure);
        AW_MPI_ISP_Init_AE_SetExposure(nIspDev, curChnIspAe_Exposure);
    }
    int curChnIspAe_Gain = pISPParameters->ChnIspAe_GetGain();
    if(curChnIspAe_Gain != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIspAe_Gain[%d]", nIspDev, curChnIspAe_Gain);
        AW_MPI_ISP_Init_AE_SetGain(nIspDev, curChnIspAe_Gain);
    }
    int curChnIspAe_ISOSensitive = pISPParameters->ChnIspAe_GetISOSensitive();
    if(curChnIspAe_ISOSensitive != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIspAe_ISOSensitive[%d]", nIspDev, curChnIspAe_ISOSensitive);
        AW_MPI_ISP_Init_AE_SetISOSensitive(nIspDev, curChnIspAe_ISOSensitive);
    }
    int curChnIspAe_Metering = pISPParameters->ChnIspAe_GetMetering();
    if(curChnIspAe_Metering != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIspAe_Metering[%d]", nIspDev, curChnIspAe_Metering);
        AW_MPI_ISP_Init_AE_SetMetering(nIspDev, curChnIspAe_Metering);
    }
    int curChnIspAwb_Mode = pISPParameters->ChnIspAwb_GetMode();
    if(curChnIspAwb_Mode != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIspAwb_Mode[%d]", nIspDev, curChnIspAwb_Mode);
        AW_MPI_ISP_Init_AWB_SetMode(nIspDev, curChnIspAwb_Mode);
    }
    int curChnIspAwb_RGain = pISPParameters->ChnIspAwb_GetRGain();
    int curChnIspAwb_GrGain = pISPParameters->ChnIspAwb_GetGrGain();
    int curChnIspAwb_GbGain = pISPParameters->ChnIspAwb_GetGbGain();
    int curChnIspAwb_BGain = pISPParameters->ChnIspAwb_GetBGain();
    if ((curChnIspAwb_RGain != -1) && (curChnIspAwb_GrGain != -1) && (curChnIspAwb_GbGain != -1) && (curChnIspAwb_BGain != -1))
    {
        alogd("isp[%d] set chnIspAwb_Gain[%d-%d-%d-%d]", nIspDev, curChnIspAwb_RGain, curChnIspAwb_GrGain, curChnIspAwb_GbGain,
            curChnIspAwb_BGain);
        struct isp_wb_gain stIspWbGain = {(HW_U16)curChnIspAwb_RGain, (HW_U16)curChnIspAwb_GrGain, (HW_U16)curChnIspAwb_GbGain,
            (HW_U16)curChnIspAwb_BGain};
        AW_MPI_ISP_Init_AWB_SetGain(nIspDev, &stIspWbGain);
    }
    int curChnIsp_Flicker = pISPParameters->ChnIsp_GetFlicker();
    if(curChnIsp_Flicker != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIsp_Flicker[%d]", nIspDev, curChnIsp_Flicker);
        AW_MPI_ISP_Init_SetFlicker(nIspDev, curChnIsp_Flicker);
    }
    int curChnIsp_Brightness = pISPParameters->ChnIsp_GetBrightness();
    if(curChnIsp_Brightness != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIsp_Brightness[%d]", nIspDev, curChnIsp_Brightness);
        AW_MPI_ISP_Init_SetBrightness(nIspDev, curChnIsp_Brightness);
    }
    int curChnIsp_Contrast = pISPParameters->ChnIsp_GetContrast();
    if(curChnIsp_Contrast != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIsp_Contrast[%d]", nIspDev, curChnIsp_Contrast);
        AW_MPI_ISP_Init_SetContrast(nIspDev, curChnIsp_Contrast);
    }
    int curChnIsp_Saturation = pISPParameters->ChnIsp_GetSaturation();
    if(curChnIsp_Saturation != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIsp_Saturation[%d]", nIspDev, curChnIsp_Saturation);
        AW_MPI_ISP_Init_SetSaturation(nIspDev, curChnIsp_Saturation);
    }
    int curChnIsp_Sharpness = pISPParameters->ChnIsp_GetSharpness();
    if(curChnIsp_Sharpness != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIsp_Sharpness[%d]", nIspDev, curChnIsp_Sharpness);
        AW_MPI_ISP_Init_SetSharpness(nIspDev, curChnIsp_Sharpness);
    }
    /*int curChnIsp_Hue = mISPParameters.ChnIsp_GetHue();
    if(curChnIsp_Hue != -1)
    {
        alogd("set chnIsp_Hue[%d]", curChnIsp_Hue);
        AW_MPI_ISP_SetHue(mIspDevId, curChnIsp_Hue);
    }*/
    enum ae_table_mode curChnIsp_Scene = pISPParameters->ChnIsp_GetScene();
    if (curChnIsp_Scene != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIsp Scene[%d]", nIspDev, curChnIsp_Scene);
        AW_MPI_ISP_Init_SetScene(nIspDev, curChnIsp_Scene);
    }
    enum colorfx curChnIsp_ColorEffect = pISPParameters->ChnIsp_GetColorEffect();
    if (curChnIsp_ColorEffect != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set chnIsp ColorEffect[%d]", nIspDev, curChnIsp_ColorEffect);
        AW_MPI_ISP_Init_SetColorEffect(nIspDev, curChnIsp_ColorEffect);
    }
    scene_mode_t curChnIsp_SpecialScene = pISPParameters->ChnIsp_GetSpecialScene();
    if (curChnIsp_SpecialScene != ILLEGAL_ISP_PARAM)
    {
        alogd("isp[%d] set curChnIsp SpecialScene[%d]", nIspDev, curChnIsp_SpecialScene);
        AW_MPI_ISP_Init_SetSpecialScene(nIspDev, curChnIsp_SpecialScene);
    }
    return NO_ERROR;
}

status_t VIDevice::startChannel(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
#if (defined(SUPPORT_ISP))
    if(!mbIspRun)
    {
        mbIspRun = true;
        ERRORTYPE eRet;
        ISP_DEV nIspDevId = pChannel->getIspDev();
        ISP_DEV nIspDevFromCameraInfo = deduceIspForScalerChn(chnId);
        if (nIspDevId != nIspDevFromCameraInfo)
        {
            aloge("fatal error! vipp[%d] ispDev from cameraInfo is wrong[%d!=%d]", chnId, nIspDevId, nIspDevFromCameraInfo);
        }
        CameraParameters *pISPParameters = NULL;
        auto search = mParamsOfISPs.find(nIspDevId);
        if (search != mParamsOfISPs.end())
        {
            pISPParameters = &search->second;
        }
        else
        {
            aloge("fatal error! can't get ISP[%d] in paramsOfISPs.", nIspDevId);
        }

        if (mCameraInfo.mStitchMode)
        {
            VI_DEV nStitchOutputChn = getStitchOutputScalerChn();
            if (chnId == nStitchOutputChn) //run all ISPs for stitch mode.
            {
                alogd("========= STITCH MODE IspDevId: %d =========\n", nIspDevId);
                AW_MPI_ISP_SetStitchMode(nIspDevId, STITCH_2IN1_LINNER);
                AW_MPI_ISP_AWB_SetStatsSyncMode(nIspDevId, ISP0_ISP1_COMBINE);
                setInitialConfigToIsp(nIspDevId);
                for (ISPGeometry& elem : mCameraInfo.mMPPGeometry.mISPGeometrys)
                {
                    alogd("camera[%d-%d] stitch enable: run ISP[%d] of all ISPs", mCameraId, chnId, elem.mISPDev);
                    AW_MPI_ISP_Run(elem.mISPDev);
                }
            }
            else
            {
                alogd("camera[%d-%d] stitch disable: run ISP[%d]", mCameraId, chnId, nIspDevId);
                setInitialConfigToIsp(nIspDevId);
                AW_MPI_ISP_Run(nIspDevId);
            }
        }
        else
        {
            alogd("camera[%d-%d] normal run ISP[%d]", mCameraId, chnId, nIspDevId);
            setInitialConfigToIsp(nIspDevId);
            AW_MPI_ISP_Run(nIspDevId);
        }
        int nWaitMs = 1;
        alogd("wait %dms, then can set isp parameters", nWaitMs);
        usleep(nWaitMs*1000);

        #if 1
//        ISP_MODULE_ONOFF ModuleOnOff = pISPParameters->getModuleOnOff();
//        if(ModuleOnOff.pltm != -1)
//        {
//            alogd("isp[%d]-%d set ISP_ModuleOnOff", nIspDevId, chnId);
//            AW_MPI_ISP_SetModuleOnOff(nIspDevId, &ModuleOnOff);
//        }
        //INI ISP config
        int stNRAttrValue = pISPParameters->getNRAttrValue();
        if(stNRAttrValue != -1)
        {
            alogd("isp[%d]-%d set ISP_NR_ATTR[%d]", nIspDevId, chnId, stNRAttrValue);
            AW_MPI_ISP_SetNRAttr(nIspDevId, stNRAttrValue);
        }
        int st3NRAttrValue = pISPParameters->get3NRAttrValue();
        if(st3NRAttrValue != -1)
        {
            alogd("isp[%d]-%d set ISP_3NR_ATTR[%d]", nIspDevId, chnId, st3NRAttrValue);
            AW_MPI_ISP_Set3NRAttr(nIspDevId, st3NRAttrValue);
        }

        int stPltmWDR = pISPParameters->getPltmWDR();
        if(stPltmWDR != -1)
        {
            alogd("isp[%d]-%d set ISP_PltmWDR[%d]", nIspDevId, chnId, stPltmWDR);
            AW_MPI_ISP_SetPltmWDR(nIspDevId, stPltmWDR);
        }
        #endif
        initIspParameters(nIspDevId);
    }
#endif
    status_t ret = pChannel->startChannel();
    return ret;
}

status_t VIDevice::stopChannel(int chnId, bool bKeepRender)
{
    status_t ret;
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    ret = pChannel->stopChannel(bKeepRender);
#if (defined(SUPPORT_ISP))
    if(mbIspRun)
    {
        AutoMutex lock(mVIChannelVectorLock);
        int num = 0;
        for(VIChannelInfo& i : mVIChannelVector)
        {
            if(VIChannel::VI_CHN_STATE_STARTED == i.mpChannel->getState() && i.mChnId != chnId)
            {
                num++;
            }
        }
        if(0 == num)
        {
            ISP_DEV nIspDevId = pChannel->getIspDev();
            //if(VIChannel::VI_CHN_STATE_STARTED == pChannel->getState())
            //{
                if (mCameraInfo.mStitchMode)
                {
                    VI_DEV nStitchOutputChn = getStitchOutputScalerChn();
                    if (chnId == nStitchOutputChn) //stop all ISPs for stitch mode.
                    {
                        for (ISPGeometry& elem : mCameraInfo.mMPPGeometry.mISPGeometrys)
                        {
                            alogd("camera[%d-%d] stitch enable: stop ISP[%d] of all ISPs", mCameraId, chnId, elem.mISPDev);
                            AW_MPI_ISP_Stop(elem.mISPDev);
                        }
                    }
                    else
                    {
                        alogd("camera[%d-%d] stitch disable: stop ISP[%d]", mCameraId, chnId, nIspDevId);
                        AW_MPI_ISP_Stop(nIspDevId);
                    }
                }
                else
                {
                    alogd("camera[%d-%d] normal stop ISP[%d]", mCameraId, chnId, nIspDevId);
                    AW_MPI_ISP_Stop(nIspDevId);
                }
                mbIspRun = false;
            //}
        }
    }
#endif
    return ret;
}

status_t VIDevice::getMODParams(int chnId, MOTION_DETECT_ATTR_S *pParamMD)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getMODParams(pParamMD);
}

status_t VIDevice::setMODParams(int chnId, MOTION_DETECT_ATTR_S pParamMD)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setMODParams(pParamMD);
}

status_t VIDevice::startMODDetect(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startMODDetect();
}

status_t VIDevice::stopMODDetect(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopMODDetect();
}

status_t VIDevice::getAdasParams(int chnId, AdasDetectParam *pParamADAS)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getAdasParams(pParamADAS);
}
status_t VIDevice::setAdasParams(int chnId, AdasDetectParam pParamADAS)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setAdasParams(pParamADAS);
}
status_t VIDevice::getAdasInParams(int chnId, AdasInParam *pParamADAS)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getAdasInParams(pParamADAS);
}
status_t VIDevice::setAdasInParams(int chnId, AdasInParam pParamADAS)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setAdasInParams(pParamADAS);
}
status_t VIDevice::startAdasDetect(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startAdasDetect();
}
status_t VIDevice::stopAdasDetect(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopAdasDetect();
}
///============
status_t VIDevice::getAdasParams_v2(int chnId, AdasDetectParam_v2 *pParamADAS_v2)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getAdasParams_v2(pParamADAS_v2);
}
status_t VIDevice::setAdasParams_v2(int chnId, AdasDetectParam_v2 pParamADAS_v2)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setAdasParams_v2(pParamADAS_v2);
}
status_t VIDevice::getAdasInParams_v2(int chnId, AdasInParam_v2 *pParamADAS_v2)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->getAdasInParams_v2(pParamADAS_v2);
}
status_t VIDevice::setAdasInParams_v2(int chnId, AdasInParam_v2 pParamADAS_v2)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->setAdasInParams_v2(pParamADAS_v2);
}

status_t VIDevice::startAdasDetect_v2(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if(pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startAdasDetect_v2();
}
status_t VIDevice::stopAdasDetect_v2(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopAdasDetect_v2();
}

void VIDevice::releaseRecordingFrame(int chnId, uint32_t index)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->releaseFrame(index);
}

status_t VIDevice::startRecording(int chnId, CameraRecordingProxyListener *pCb, int recorderId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->startRecording(pCb, recorderId);
}

status_t VIDevice::stopRecording(int chnId, int recorderId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->stopRecording(recorderId);
}

void VIDevice::setDataListener(int chnId, DataListener *pCb)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->setDataListener(pCb);
}

void VIDevice::setNotifyListener(int chnId, NotifyListener *pCb)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->setNotifyListener(pCb);
}

/*
void VIDevice::postDataCompleted(int chnId, const void *pData, int size)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->postDataCompleted(pData, size);
}

void VIDevice::postVdaDataCompleted(int chnId, const VDA_DATA_S *pData)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL)
    {
        aloge("channel %d is not exist!", chnId);
        return;
    }
    pChannel->postVdaDataCompleted(pData);
}
*/

status_t VIDevice::takePicture(int chnId, unsigned int msgType, PictureRegionCallback *pPicReg)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->takePicture(msgType, pPicReg);
}

status_t VIDevice::notifyPictureRelease(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->notifyPictureRelease();
}

status_t VIDevice::cancelContinuousPicture(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->cancelContinuousPicture();
}

status_t VIDevice::KeepPictureEncoder(int chnId, bool bKeep)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->KeepPictureEncoder(bKeep);
}

status_t VIDevice::releasePictureEncoder(int chnId)
{
    VIChannel *pChannel = searchVIChannel(chnId);
    if (pChannel == NULL) {
        aloge("channel %d is not exist!", chnId);
        return NO_INIT;
    }
    return pChannel->releasePictureEncoder();
}

status_t VIDevice::getISPDMsg(ISP_DEV nIspDev, int *exp, int *exp_line, int *gain, int *lv_idx, int *color_temp,
    int *rgain, int *bgain, int *grgain, int *gbgain)
{
    auto search = mParamsOfISPs.find(nIspDev);
    if (search == mParamsOfISPs.end())
    {
        aloge("fatal error! ispDev[%d] wrong, can't get ISP parameters!", nIspDev);
    }
      int tmp;
 
      AW_MPI_ISP_AE_GetExposure(nIspDev, &tmp);
      *exp = tmp;
 
      AW_MPI_ISP_AE_GetExposureLine(nIspDev, &tmp);
      *exp_line = tmp;
 
      AW_MPI_ISP_AE_GetGain(nIspDev, &tmp);
      *gain = tmp;
 
      AW_MPI_ISP_AWB_GetRGain(nIspDev, &tmp);
      *rgain = tmp;
 
      AW_MPI_ISP_AWB_GetBGain(nIspDev, &tmp);
      *bgain = tmp;

      AW_MPI_ISP_AWB_GetGrGain(nIspDev, &tmp);        //get gr_gain
      *grgain = tmp;
      
      AW_MPI_ISP_AWB_GetGbGain(nIspDev, &tmp);        //get gb_gain
      *gbgain = tmp;

      AW_MPI_ISP_AWB_GetCurColorT(nIspDev, &tmp);
      *color_temp = tmp;
 
      AW_MPI_ISP_AE_GetEvIdx(nIspDev, &tmp);
      *lv_idx= tmp;
      
      return NO_ERROR;
}

  


}; /* namespace EyeseeLinux */
