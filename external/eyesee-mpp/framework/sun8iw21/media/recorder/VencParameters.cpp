/******************************************************************************
  Copyright (C), 2020-2022, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : VencParameters.cpp
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2020/11/10
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

#include <string.h>

#include <utils/plat_log.h>
#include <VencParameters.h>

using namespace std;
namespace EyeseeLinux {

/*unsigned int VencParameters::VEncBitRateControlAttr::GetBitRate()
{
    if(PT_H264 == mVEncType && VideoRCMode_CBR == mRcMode)
    {
        return mAttrH264Cbr.mBitRate;
    }
    else if(PT_H264 == mVEncType && VideoRCMode_VBR == mRcMode)
    {
        return mAttrH264Vbr.mMaxBitRate;
    }
    else if(PT_H264 == mVEncType && VideoRCMode_ABR == mRcMode)
    {
        return mAttrH264Abr.mMaxBitRate;
    }
    else if(PT_H265 == mVEncType && VideoRCMode_CBR == mRcMode)
    {
        return mAttrH265Cbr.mBitRate;
    }
    else if(PT_H265 == mVEncType && VideoRCMode_VBR == mRcMode)
    {
        return mAttrH265Vbr.mMaxBitRate;
    }
    else if(PT_H265 == mVEncType && VideoRCMode_ABR == mRcMode)
    {
        return mAttrH265Abr.mMaxBitRate;
    }
    else
    {
        aloge("fatal error! wrong bitRate of encType-RateContrlMode[%d-%d]", mVEncType, mRcMode);
        return (unsigned int)(-1);
    }
}*/

VencParameters::VencParameters()
{
    //mFrameRate = 30;
    //mVideoEncoder = PT_MAX;
    //mVideoWidth = 1920;
    //mVideoHeight = 1080;
    //mVideoMaxKeyItl = 30;
    //mIQpOffset = 0;
    //mFastEncFlag = 0;
    //mVideoRCMode = VideoRCMode_CBR;
    //mVideoPDMode = VideoEncodeProductMode::NORMAL_MODE;
    //mSensorType = VENC_ST_DIS_WDR;
    //mbPIntraEnable = true;
    mNullSkipEnable = false;
    mPSkipEnable = false;
    mbHorizonfilp = false;
    mbAdaptiveintrainp = false;
    mColor2Grey.bColor2Grey = false;
    mVeChn = MM_INVALID_CHN;
    //mSensorType = VENC_ST_DEFAULT;
    //mOnlineEnable = false;
    //mOnlineShareBufNum = 0;
    //mbEncppEnable = false;
    mbIspAndVeLinkEnable = false;
    mEncppSharpAttenCoefPer = 100;
    mbMainStreamFlag = false;

    memset(&mVenc3DnrParam, 0, sizeof(s3DfilterParam));
    //memset(&mVEncAttr, 0, sizeof(VEncAttr));
    //memset(&mVEncRcAttr, 0, sizeof(VEncBitRateControlAttr));
    memset(&mSmartPParam, 0, sizeof(VencSmartFun));
    memset(&mIntraRefreshParam, 0, sizeof(mIntraRefreshParam));
    memset(&mVEncChnAttr, 0, sizeof(mVEncChnAttr));
    memset(&mVEncRcParam, 0, sizeof(mVEncRcParam));
    memset(&mVEncRefParam, 0, sizeof(VENC_PARAM_REF_S));
    memset(&mVEncRoiCfg, 0, sizeof(mVEncRoiCfg));
    memset(&mVEncSuperFrameCfg, 0, sizeof(VENC_SUPERFRAME_CFG_S));
    memset(&mVuiInfo, 0, sizeof(VUI));
    memset(&mSaveBSFileParam, 0, sizeof(VencSaveBSFile));
    memset(&mVeProcSet, 0, sizeof(VeProcSet));
    mVeProcSet.bProcEnable = 1;
    mVeProcSet.nProcFreq = 120;
    mVeProcSet.nStatisBitRateTime = 1000;
    mVeProcSet.nStatisFrRateTime = 1000;
}

VencParameters::~VencParameters()
{

}

/*void VencParameters::setVEncAttr(VencParameters::VEncAttr &nVEncAttr)
{
    mVEncAttr = nVEncAttr;
}*/

/*VencParameters::VEncAttr VencParameters::getVEncAttr()
{
    return mVEncAttr;
}*/

/**
  update dstFrameRate to mVEncChnAttr, so must be called after setVencChnAttr().
*/
void VencParameters::setVideoFrameRate(int rate)
{
    VENC_FRAME_RATE_S stFrameRate;
    GetFrameRateFromVENC_CHN_ATTR_S(&mVEncChnAttr, &stFrameRate);
    stFrameRate.DstFrmRate = rate;
    SetFrameRateToVENC_CHN_ATTR_S(&stFrameRate, &mVEncChnAttr);
}

/**
  get dstFrameRate from mVEncChnAttr, so must be called after setVencChnAttr().
*/
int VencParameters::getVideoFrameRate()
{
    VENC_FRAME_RATE_S stFrameRate;
    GetFrameRateFromVENC_CHN_ATTR_S(&mVEncChnAttr, &stFrameRate);
    return stFrameRate.DstFrmRate;
}

/**
  update videoSize to mVEncChnAttr, so must be called after setVencChnAttr().
*/
void VencParameters::setVideoSize(const SIZE_S &stVideoSize)
{
    SetEncodeDstSizeToVENC_CHN_ATTR_S(&mVEncChnAttr, (SIZE_S*)&stVideoSize);
}

/**
  get venc dst width and height, so must be called after setVencChnAttr().
*/
void VencParameters::getVideoSize(SIZE_S &stVideoSize)
{
    GetEncodeDstSizeFromVENC_CHN_ATTR_S(&mVEncChnAttr, &stVideoSize);
}
/**
  update bitrate to mVEncChnAttr, so must be called after setVencChnAttr().
*/
void VencParameters::setVideoEncodingBitRate(int bitRate)
{
    setVideoEncodingBitRateToVENC_CHN_ATTR_S(&mVEncChnAttr, bitRate);
}
int VencParameters::getVideoEncodingBitRate()
{
    return GetBitRateFromVENC_CHN_ATTR_S(&mVEncChnAttr);
}

/*
status_t VencParameters::setIQpOffset(int nIQpOffset)
{
    if (PT_H264 == mVideoEncoder)
    {
        if (!(nIQpOffset>=0) && nIQpOffset<10)
        {
            aloge("IQpOffset value must be in [0, 10) for 264!");
            return BAD_VALUE;
        }
    }
    else if (PT_H265 == mVideoEncoder)
    {
        if (!(nIQpOffset>=-12 && nIQpOffset<=12))
        {
            aloge("IQpOffset value must be in [-12, 12] for 265");
        }
    }
    else
    {
        aloge("IQpOffset can not be set for other vencoder(%d)!", mVideoEncoder);
    }

    mIQpOffset = nIQpOffset;
    return NO_ERROR;
}
*/
/*
void VencParameters::setVEncBitRateControlAttr(VencParameters::VEncBitRateControlAttr &RcAttr)
{
    mVEncRcAttr = RcAttr;
}

VencParameters::VEncBitRateControlAttr VencParameters::getVEncBitRateControlAttr()
{
    return mVEncRcAttr;
}
*/
//void VencParameters::setVideoEncodingRateControlMode(const VencParameters::VideoEncodeRateControlMode &rcMode)
//{
//    mVideoRCMode = rcMode;
//}

VENC_RC_MODE_E VencParameters::getVideoEncodingRateControlMode()
{
    return mVEncChnAttr.RcAttr.mRcMode;
}

/*void VencParameters::setVideoEncodingProductMode(VencParameters::VideoEncodeProductMode &ndMode)
{
    mVideoPDMode = ndMode;
}*/

eVencProductMode VencParameters::getVideoEncodingProductMode()
{
    return mVEncChnAttr.RcAttr.mProductMode;
}

void VencParameters::set3DFilter(s3DfilterParam &n3DfilterParam)
{
    mVenc3DnrParam = n3DfilterParam;
}

s3DfilterParam VencParameters::get3DFilter()
{
    return mVenc3DnrParam;
}

void VencParameters::setVencSuperFrameConfig(VENC_SUPERFRAME_CFG_S &nSuperFrameConfig)
{
    mVEncSuperFrameCfg = nSuperFrameConfig;
}

VENC_SUPERFRAME_CFG_S VencParameters::getVencSuperFrameConfig()
{
    return mVEncSuperFrameCfg;
}

void VencParameters::enableSaveBSFile(VencSaveBSFile &nSavaParam)
{
    mSaveBSFileParam = nSavaParam;
}

VencSaveBSFile VencParameters::getenableSaveBSFile()
{
    return mSaveBSFileParam;
}

void VencParameters::setProcSet(VeProcSet &nVeProcSet)
{
    mVeProcSet = nVeProcSet;
}

VeProcSet VencParameters::getProcSet()
{
    return mVeProcSet;
}

void VencParameters::setVideoEncodingSmartP(VencSmartFun &nParam)
{
    mSmartPParam = nParam;
}

VencSmartFun VencParameters::getVideoEncodingSmartP()
{
    return mSmartPParam;
}

void VencParameters::setVideoEncodingIntraRefresh(VencCyclicIntraRefresh &nIntraRefresh)
{
    mIntraRefreshParam = nIntraRefresh;
}

VencCyclicIntraRefresh VencParameters::getVideoEncodingIntraRefresh()
{
    return mIntraRefreshParam;
}

void VencParameters::setGopAttr(const VENC_GOP_ATTR_S &nParam)
{
    mVEncChnAttr.GopAttr = nParam;
}

VENC_GOP_ATTR_S VencParameters::getGopAttr()
{
    return mVEncChnAttr.GopAttr;
}

void VencParameters::setVencChnAttr(VENC_CHN_ATTR_S &nVEncChnAttr)
{
    mVEncChnAttr = nVEncChnAttr;
}

VENC_CHN_ATTR_S VencParameters::getVencChnAttr()
{
    return mVEncChnAttr;
}

void VencParameters::setVencRcParam(VENC_RC_PARAM_S &stVEncRcParam)
{
    mVEncRcParam = stVEncRcParam;
}

VENC_RC_PARAM_S VencParameters::getVencRcParam()
{
    return mVEncRcParam;
}

void VencParameters::setRefParam(const VENC_PARAM_REF_S &nstRefParam)
{
    mVEncRefParam = nstRefParam;
}

VENC_PARAM_REF_S VencParameters::getRefParam()
{
    return mVEncRefParam;
}

void VencParameters::setRoiCfg(VENC_ROI_CFG_S &nVencRoiCfg)
{
    mVEncRoiCfg = nVencRoiCfg;
}

VENC_ROI_CFG_S VencParameters::getRoiCfg()
{
    return mVEncRoiCfg;
}

/*
status_t VencParameters::enableIframeFilter(bool enable)
{
    alogd("need implement");
    return UNKNOWN_ERROR;
}

bool VencParameters::getIframeFilter()
{
    alogw("need implement");
    return false;
}

status_t VencParameters::setVideoEncodingMode(int Mode)
{
    alogd("need implement!");
    return UNKNOWN_ERROR;
}
*/
};
