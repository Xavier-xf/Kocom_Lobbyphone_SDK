/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : EyeseeRecorder.cpp
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/07
  Last Modified :
  Description   : recorder use mpp modules to implement video recording.
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "EyeseeRecorder"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <utility>

#include <RecAVSync.h>
#include <aenc_sw_lib.h>
#include <mm_common.h>
#include <mm_comm_aio.h>
#include <mpi_sys.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>
#include <TextEncApi.h>
#include <mpi_tenc.h>
#include <mpi_venc_private.h>
#include <mpi_venc.h>
#include <mpi_region.h>
#include <mpi_mux.h>
#include <record_writer.h>
#include <media_common.h>

#include <utils/Mutex.h>
#include <MediaStructConvert.h>
//#include <EyeseeCamera.h>
#include <EyeseeRecorder.h>
#include <MediaCallbackDispatcher.h>
#include "CameraFrameManager.h"
#include "DynamicBitRateControl.h"
#include <dup2SeldomUsedFd.h>
//#include <system/audio.h>
#include <SystemBase.h>

#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)//(4*1024)
namespace EyeseeLinux {

/*
static ERRORTYPE setQpRangeToVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pChnAttr, int minqp, int maxqp)
{
    switch(pChnAttr->RcAttr.mRcMode)
    {
        case VENC_RC_MODE_H264CBR:
        {
            pChnAttr->RcAttr.mAttrH264Cbr.mMaxQp = maxqp;
            pChnAttr->RcAttr.mAttrH264Cbr.mMinQp = minqp;
            break;
        }
        case VENC_RC_MODE_H264VBR:
        {
            pChnAttr->RcAttr.mAttrH264Vbr.mMaxQp = maxqp;
            pChnAttr->RcAttr.mAttrH264Vbr.mMinQp = minqp;
            break;
        }
        case VENC_RC_MODE_H264FIXQP:
        {
            pChnAttr->RcAttr.mAttrH264FixQp.mIQp = minqp;
            pChnAttr->RcAttr.mAttrH264FixQp.mPQp = maxqp;
            break;
        }
        case VENC_RC_MODE_H264ABR:
        {
            pChnAttr->RcAttr.mAttrH264Abr.mMaxIQp = maxqp;
            pChnAttr->RcAttr.mAttrH264Abr.mMinIQp = minqp;
            break;
        }
        case VENC_RC_MODE_H265CBR:
        {
            pChnAttr->RcAttr.mAttrH265Cbr.mMaxQp = maxqp;
            pChnAttr->RcAttr.mAttrH265Cbr.mMinQp = minqp;
            break;
        }
        case VENC_RC_MODE_H265VBR:
        {
            pChnAttr->RcAttr.mAttrH265Vbr.mMaxQp = maxqp;
            pChnAttr->RcAttr.mAttrH265Vbr.mMinQp = minqp;
            break;
        }
        case VENC_RC_MODE_H265FIXQP:
        {
            pChnAttr->RcAttr.mAttrH265FixQp.mIQp = minqp;
            pChnAttr->RcAttr.mAttrH265FixQp.mPQp = maxqp;
            break;
        }
        case VENC_RC_MODE_H265ABR:
        {
            pChnAttr->RcAttr.mAttrH265Abr.mMaxIQp = maxqp;
            pChnAttr->RcAttr.mAttrH265Abr.mMinIQp = minqp;
            break;
        }
        case VENC_RC_MODE_MJPEGFIXQP:
        {
            pChnAttr->RcAttr.mAttrMjpegeFixQp.mQfactor = minqp;
            break;
        }
        default:
        {
            alogd("fatal error! other rc mode[0x%x] don't need set qp!", pChnAttr->RcAttr.mRcMode);
            break;
        }
    }
    return SUCCESS;
}
*/

extern "C" ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
	((EyeseeRecorder*)cookie)->notify(pChn, event, pEventData);
    return SUCCESS;
}

unsigned int EyeseeRecorder::gRecorderIdCounter = RecorderIdPrefixMark | 0x00;

EyeseeRecorder::EventHandler::EventHandler(EyeseeRecorder *pC)
{
    mpMediaRecorder = pC;
}

EyeseeRecorder::EventHandler::~EventHandler()
{
}

void EyeseeRecorder::EventHandler::handleMessage(const CallbackMessage &msg)
{
    switch (msg.what)
    {
        case MEDIA_RECORDER_EVENT_ERROR:
            if (mpMediaRecorder->mOnErrorListener != NULL)
            {
                int extra;
                if(msg.arg1 == MEDIA_ERROR_VENC_TIMEOUT)
                {
                    void *pData = msg.mDataPtr->getPointer();
                    extra = (int)pData;
                }
                else
                {
                    extra = msg.arg2;
                }
                mpMediaRecorder->mOnErrorListener->onError(mpMediaRecorder, msg.arg1, extra);
            }
            return;
        case MEDIA_RECORDER_EVENT_INFO:
            if (mpMediaRecorder->mOnInfoListener != NULL)
            {
                mpMediaRecorder->mOnInfoListener->onInfo(mpMediaRecorder, msg.arg1, msg.arg2);
            }
            return;
        case MEDIA_RECORDER_VENDOR_EVENT_BSFRAME_AVAILABLE:
            if (mpMediaRecorder->mOnDataListener != NULL)
            {
                mpMediaRecorder->mOnDataListener->onData(mpMediaRecorder, msg.arg1, msg.arg2);
            }
            return;
        default:
            aloge("fatal error! Unknown message type 0x%x, 0x%x, 0x%x", msg.what, msg.arg1, msg.arg2);
            return;
    }
}

EyeseeRecorder::CameraProxyListener::CameraProxyListener(EyeseeRecorder *recorder, VI_DEV Vipp)
{
    mRecorder = recorder;
    mVipp = Vipp;
}

void EyeseeRecorder::CameraProxyListener::dataCallbackTimestamp(const void *pdata)
{
    mRecorder->dataCallbackTimestamp((const VIDEO_FRAME_BUFFER_S*)pdata, mVipp);
}

bool EyeseeRecorder::process_media_recorder_call(status_t opStatus, const char* message)
{
    alogv("process_media_recorder_call");
    if (opStatus == (status_t)INVALID_OPERATION)
    {
        aloge("INVALID_OPERATION");
        return true;
    }
    else if (opStatus != (status_t)OK)
    {
        aloge("%s", message);
        return true;
    }
    return false;
}

EyeseeRecorder::EyeseeRecorder() :
    mRecorderId(gRecorderIdCounter++),
    mOnErrorListener(NULL),
    mOnInfoListener(NULL),
    mOnDataListener(NULL)
{
    alogv("Constructor");
    mpInputFrameManager = NULL;
    //mpCameraProxy = NULL;
    //mCameraSourceChannel = 0;
    mTimeLapseEnable = false;
    mTimeBetweenFrameCapture = 0;
    mMaxFileDuration = 0;
    mMuxCacheDuration = 0;
    mMuxCacheStrmIds.mStrmIdsCnt = -1;
    mMaxFileSizeBytes = 0;
    //mVideoEncoder = PT_MAX;
    mAudioEncoder = PT_MAX;
    last_frm_pts = -1;
    frm_cnt = 0;
    gps_state = 0;
    rec_start_timestamp = -1;
    mpMuxCacheManager = NULL;
    mIgnoreAudioBlockNum = 0;
    mIgnoreAudioBytes = 0;
    mPauseAudioDuration = 0;
    mPauseVideoPts = 0;
    mPauseVideoDuration = 0;

    mVeChnCount = 0;

    //mVideoMaxKeyItl = 30;
    //mCallbackOutDataType = CALLBACK_OUT_DATA_VIDEO_ONLY;
    //mCallbackOutStreamIdList.clear();
    mIdleEncBufList.resize(ENC_BACKUP_BUFFER_NUM);
    for(std::list<VEncBuffer>::iterator it = mIdleEncBufList.begin(); it != mIdleEncBufList.end(); ++it)
    {
        memset(&*it, 0, sizeof(VEncBuffer));
    }

    //MPP components
    //mVeChn = MM_INVALID_CHN;
    mAiDev = -1;
    mAiChn = MM_INVALID_CHN;
    mAeChn = MM_INVALID_CHN;
    mTeChn = MM_INVALID_CHN;
    //mAVSync = NULL;
    mpDBRC = NULL;
    mEnableDBRC = false;
    mMuteMode = false;

    doCleanUp();
    mEventHandler = new EventHandler(this);

    mCurrentState = MEDIA_RECORDER_IDLE;
}

EyeseeRecorder::~EyeseeRecorder()
{
	alogv("destructor");
    Mutex::Autolock autoLock(mLock);
    if (!(mCurrentState & MEDIA_RECORDER_IDLE))
    {
        aloge("fatal error! can't destruct in an invalid state: 0x%x", mCurrentState);
        reset_l();
    }
    if(mEventHandler)
    {
        delete mEventHandler;
        mEventHandler = NULL;
    }

    if(mpInputFrameManager)
    {
        delete mpInputFrameManager;
        mpInputFrameManager = NULL;
    }
#if 0
    if(mpCameraProxy)
    {
        delete mpCameraProxy;
    }
#endif
    for (std::map<VI_DEV, CameraRecordingProxy*>::iterator it = mCameraProxyMap.begin(); it != mCameraProxyMap.end();)
    {
        if (it->second)
        {
            delete it->second;
            it->second = NULL;
        }
        it = mCameraProxyMap.erase(it);
    }

    for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end();)
    {
        if (it->second)
        {
            delete it->second;
            it->second = NULL;
        }
        it = mVencInfoMap.erase(it);
    }
}
/*
status_t EyeseeRecorder::setCamera(EyeseeCamera *pC)
{
    if(mpCameraProxy)
    {
        alogw("Be careful! CameraProxy[%p] is already set!", mpCameraProxy);
        delete mpCameraProxy;
    }
    mpCameraProxy = pC->getRecordingProxy();
    return NO_ERROR;
}

status_t EyeseeRecorder::setCamera(EyeseeCamera *pC, int channelId)
{
    status_t ret;
    ret = setCamera(pC);
    ret = setSourceChannel(channelId);
    return ret;
}

status_t EyeseeRecorder::setISE(EyeseeISE *pISE)
{
    if(mpCameraProxy)
    {
        alogw("Be careful! CameraProxy[%p] is already set!", mpCameraProxy);
        delete mpCameraProxy;
    }
    mpCameraProxy = pISE->getRecordingProxy();
    return NO_ERROR;
}

status_t EyeseeRecorder::setISE(EyeseeISE *pISE, int channelId)
{
    status_t ret;
    ret = setISE(pISE);
    ret = setSourceChannel(channelId);
    return ret;
}

status_t EyeseeRecorder::setSourceChannel(int channelId)
{
    mCameraSourceChannel = channelId;
    return NO_ERROR;
}
*/
status_t EyeseeRecorder::setCameraProxy(CameraRecordingProxy *pCameraProxy, VI_DEV Vipp)
{
    if (mCameraProxyMap.end() != mCameraProxyMap.find(Vipp))
    {
        aloge("fatal error! channelId[%d] is already exist!");
        delete pCameraProxy;
        return BAD_VALUE;
    }
    auto ret = mCameraProxyMap.insert(std::make_pair(Vipp, pCameraProxy));
    if (ret.second != true)
    {
        aloge("fatal error! mCameraProxyMap insert fail!");
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
#if 0
    if(mpCameraProxy)
    {
        alogw("Be careful! CameraProxy[%p] is already set!", mpCameraProxy);
        delete mpCameraProxy;
    }
    mpCameraProxy = pCameraProxy;
    mCameraSourceChannel = channelId;
    return NO_ERROR;
#endif
}

status_t EyeseeRecorder::setCaptureRate(double fps)
{
    if(fps <= 0)
    {
        mTimeLapseEnable = false;
        return NO_ERROR;
    }
    int64_t timeUs = (int64_t) (1000000.0 / fps + 0.5f);
    // Not allowing time more than a day
    if (timeUs <= 0 || timeUs > 86400*1E6)
    {
        aloge("Time between frame capture (%lld) is out of range [0, 1 Day]", (long long)timeUs);
        return BAD_VALUE;
    }
    mTimeLapseEnable = true;
    mTimeBetweenFrameCapture = timeUs;
    return NO_ERROR;
}

status_t EyeseeRecorder::setSlowRecordMode(bool bEnable)
{
    mTimeLapseEnable = bEnable;
    if(mTimeLapseEnable)
    {
        mTimeBetweenFrameCapture = 0;
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::setOrientationHint(int degrees)
{
    mRotationDegrees = degrees;
    return NO_ERROR;
}

status_t EyeseeRecorder::setLocation(float latitude, float longitude)
{
    mGeoAvailable = true;
    mLatitude = latitude;
    mLongitude = longitude;
    return NO_ERROR;
}

#if 0
status_t EyeseeRecorder::setVideoSize(int width, int height)
{
    mVideoWidth = width;
    mVideoHeight = height;
    return NO_ERROR;
}

status_t EyeseeRecorder::setVideoFrameRate(int rate)
{
    mFrameRate = rate;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        CameraParameters cameraParam;
        mpCameraProxy->getParameters(mCameraSourceChannel, cameraParam);
        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = cameraParam.getPreviewFrameRate();
        stFrameRate.DstFrmRate = mFrameRate;
        AW_MPI_VENC_SetFrameRate(mVeChn, &stFrameRate);
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::setVideoEncodingProductMode(VideoEncodeProductMode pdMode)
{
    alogv("set_pd_mode:%d", (int)pdMode);
    mVideoPDMode = pdMode;
    return NO_ERROR;
}

status_t EyeseeRecorder::setSensorType(eSensorType eType)
{
    alogv("set sensor type:%d", eType);
    mSensorType = eType;
    return NO_ERROR;
}

status_t EyeseeRecorder::setVideoEncodingRateControlMode(VideoEncodeRateControlMode rcMode)
{
    mVideoRCMode = rcMode;
    return NO_ERROR;
}

status_t EyeseeRecorder::setVEncBitRateControlAttr(VEncBitRateControlAttr& RcAttr)
{
    if(RcAttr.mVEncType == mVideoEncoder && RcAttr.mRcMode == mVideoRCMode)
    {
        //Todo: we will implement dynamic param setting in future.
        Mutex::Autolock autoLock(mLock);
        if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
        {
            if(RcAttr.mVEncType == mVEncRcAttr.mVEncType && RcAttr.mRcMode == mVEncRcAttr.mRcMode)
            {
                //update bitRate when cbr.
                if(VideoRCMode_CBR == RcAttr.mRcMode)
                {
                    if(PT_H264 == RcAttr.mVEncType)
                    {
                        if(mVEncRcAttr.mAttrH264Cbr.mBitRate != RcAttr.mAttrH264Cbr.mBitRate
                            || mVEncRcAttr.mAttrH264Cbr.mMaxQp != RcAttr.mAttrH264Cbr.mMaxQp
                            || mVEncRcAttr.mAttrH264Cbr.mMinQp != RcAttr.mAttrH264Cbr.mMinQp)
                        {
                            alogd("need update h264 cbr bitRate[%d]->[%d], maxQp[%d]->[%d], minQp[%d]->[%d]",
                                mVEncRcAttr.mAttrH264Cbr.mBitRate, RcAttr.mAttrH264Cbr.mBitRate,
                                mVEncRcAttr.mAttrH264Cbr.mMaxQp, RcAttr.mAttrH264Cbr.mMaxQp,
                                mVEncRcAttr.mAttrH264Cbr.mMinQp, RcAttr.mAttrH264Cbr.mMinQp);
                            mVEncRcAttr.mAttrH264Cbr.mBitRate = RcAttr.mAttrH264Cbr.mBitRate;
                            mVEncRcAttr.mAttrH264Cbr.mMaxQp = RcAttr.mAttrH264Cbr.mMaxQp;
                            mVEncRcAttr.mAttrH264Cbr.mMinQp = RcAttr.mAttrH264Cbr.mMinQp;
                            VENC_CHN_ATTR_S attr;
                            AW_MPI_VENC_GetChnAttr(mVeChn, &attr);
                            if(attr.RcAttr.mRcMode!=VENC_RC_MODE_H264CBR)
                            {
                                aloge("fatal error! check mpp_rcMode[0x%x]", attr.RcAttr.mRcMode);
                            }
                            attr.RcAttr.mAttrH264Cbr.mBitRate = RcAttr.mAttrH264Cbr.mBitRate;
                            attr.RcAttr.mAttrH264Cbr.mMaxQp = RcAttr.mAttrH264Cbr.mMaxQp;
                            attr.RcAttr.mAttrH264Cbr.mMinQp = RcAttr.mAttrH264Cbr.mMinQp;
                            AW_MPI_VENC_SetChnAttr(mVeChn, &attr);
                        }
                    }
                    else if(PT_H265 == RcAttr.mVEncType)
                    {
                        if(mVEncRcAttr.mAttrH265Cbr.mBitRate != RcAttr.mAttrH265Cbr.mBitRate)
                        {
                            alogd("need update h265 cbr bitRate[%d]->[%d]", mVEncRcAttr.mAttrH265Cbr.mBitRate, RcAttr.mAttrH265Cbr.mBitRate);
                            mVEncRcAttr.mAttrH265Cbr.mBitRate = RcAttr.mAttrH265Cbr.mBitRate;
                            VENC_CHN_ATTR_S attr;
                            AW_MPI_VENC_GetChnAttr(mVeChn, &attr);
                            if(attr.RcAttr.mRcMode!=VENC_RC_MODE_H265CBR)
                            {
                                aloge("fatal error! check mpp_rcMode[0x%x]", attr.RcAttr.mRcMode);
                            }
                            attr.RcAttr.mAttrH265Cbr.mBitRate = RcAttr.mAttrH265Cbr.mBitRate;
                            AW_MPI_VENC_SetChnAttr(mVeChn, &attr);
                        }
                    }
                    else if(PT_MJPEG == RcAttr.mVEncType)
                    {
                        if(mVEncRcAttr.mAttrMjpegCbr.mBitRate != RcAttr.mAttrMjpegCbr.mBitRate)
                        {
                            alogd("need update mjpeg cbr bitRate[%d]->[%d]", mVEncRcAttr.mAttrMjpegCbr.mBitRate, RcAttr.mAttrMjpegCbr.mBitRate);
                            mVEncRcAttr.mAttrMjpegCbr.mBitRate = RcAttr.mAttrMjpegCbr.mBitRate;
                            VENC_CHN_ATTR_S attr;
                            AW_MPI_VENC_GetChnAttr(mVeChn, &attr);
                            if(attr.RcAttr.mRcMode!=VENC_RC_MODE_MJPEGCBR)
                            {
                                aloge("fatal error! check mpp_rcMode[0x%x]", attr.RcAttr.mRcMode);
                            }
                            attr.RcAttr.mAttrMjpegeCbr.mBitRate = RcAttr.mAttrMjpegCbr.mBitRate;
                            AW_MPI_VENC_SetChnAttr(mVeChn, &attr);
                        }
                    }
                    else
                    {
                        aloge("fatal error! unsupport vencType[0x%x]", RcAttr.mVEncType);
                    }
                }
            }
            else
            {
                aloge("fatal error! check VEncType[0x%x]->[0x%x], RcMode[0x%x]->[0x%x]", mVEncRcAttr.mVEncType, RcAttr.mVEncType, mVEncRcAttr.mRcMode, RcAttr.mRcMode);
            }
        }
        mVEncRcAttr = RcAttr;
        return NO_ERROR;
    }
    else
    {
        aloge("fatal error! VEncType[0x%x][0x%x] or RcMode[0x%x][0x%x] is not match!", RcAttr.mVEncType, mVideoEncoder, RcAttr.mRcMode, mVideoRCMode);
        return BAD_VALUE;
    }
}

status_t EyeseeRecorder::getVEncBitRateControlAttr(VEncBitRateControlAttr& RcAttr)
{
    RcAttr = mVEncRcAttr;
    return NO_ERROR;
}

/*
status_t EyeseeRecorder::setVideoEncodingBitRate(int bitRate)
{
    mVideoBitRate = bitRate;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        VENC_CHN_ATTR_S attr;
        AW_MPI_VENC_GetChnAttr(mVeChn, &attr);
        setVideoEncodingBitRateToVENC_CHN_ATTR_S(&attr, bitRate);
        return AW_MPI_VENC_SetChnAttr(mVeChn, &attr);
    }
    return NO_ERROR;
}
*/

/*status_t EyeseeRecorder::setVideoEncodingBufferTime(int nBufferTime)
{
    mVideoEncodingBufferTime = nBufferTime;
    return NO_ERROR;
}*/

/*
status_t EyeseeRecorder::SetVideoEncodingQpRange(int minqp, int maxqp)
{
    mMaxQp = maxqp;
    mMinQp = minqp;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        VENC_CHN_ATTR_S attr;
        AW_MPI_VENC_GetChnAttr(mVeChn, &attr);
        setQpRangeToVENC_CHN_ATTR_S(&attr, minqp, maxqp);
        return AW_MPI_VENC_SetChnAttr(mVeChn, &attr);
    }
    return OK;
}
*/

status_t EyeseeRecorder::setVideoEncodingIFramesNumberInterval(int nMaxKeyItl)
{
    //mVideoMaxKeyItl = nMaxKeyItl;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        VENC_CHN_ATTR_S attr;
        AW_MPI_VENC_GetChnAttr(mVeChn, &attr);
        attr.VeAttr.MaxKeyInterval = nMaxKeyItl;
        return AW_MPI_VENC_SetChnAttr(mVeChn, &attr);
    }
    return OK;
}
#if 0
status_t EyeseeRecorder::setVirtualIFrameInterval(int nVirtualIFrameInterval)
{
    mVirtualIFrameInterval = nVirtualIFrameInterval;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        VENC_CHN_ATTR_S attr;
        AW_MPI_VENC_GetChnAttr(mVeChn, &attr);
        if(PT_H264 == attr.VeAttr.Type)
        {
            attr.VeAttr.AttrH264e.mVirtualIFrameInterval = nVirtualIFrameInterval;
        }
        else if(PT_H265 == attr.VeAttr.Type)
        {
            attr.VeAttr.AttrH265e.mVirtualIFrameInterval = nVirtualIFrameInterval;
        }
        else
        {
            aloge("fatal error! vencType[0x%x] don't support virtual frame!", attr.VeAttr.Type);
        }
        return AW_MPI_VENC_SetChnAttr(mVeChn, &attr);
    }
    return NO_ERROR;
}
#endif
#endif

status_t EyeseeRecorder::reencodeIFrame(int VencId)
{
    status_t result = NO_ERROR;
    ERRORTYPE ret;
    if (!(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("reencodeIFrame called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(VencId);
    if(mVencInfoMap.end() == it)
    {
        aloge("fatal error! RecorderId[%d] mVencInfoMap don't have VencId[%d]", mRecorderId, VencId);
        return BAD_VALUE;
    }
    VencParameters *pVencParam = it->second;
    VENC_CHN nVeChn = pVencParam->getVencChnIndex();
    if(nVeChn >= 0)
    {
        ret = AW_MPI_VENC_RequestIDR(nVeChn, TRUE);
        if(ret != SUCCESS)
        {
            aloge("fatal error! RecorderId[%d] vencChn[%d] forceIFrame fail:0x%x", mRecorderId, nVeChn, ret);
            result = UNKNOWN_ERROR;
        }
        return result;
    }
    else
    {
        aloge("fatal error! no venc channel for VencId[%d]!", VencId);
        return UNKNOWN_ERROR;
    }
}

#if 0
status_t EyeseeRecorder::SetVideoEncodingIntraRefresh(VENC_PARAM_INTRA_REFRESH_S *pIntraRefresh)
{
    mIntraRefreshParam = *pIntraRefresh;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        if(PT_H264 == mVideoEncoder || PT_H265 == mVideoEncoder)
        {
            AW_MPI_VENC_SetIntraRefresh(mVeChn, pIntraRefresh);
        }
        else
        {
            aloge("fatal error! encoder[0x%x] don't support IntraRefresh!", mVideoEncoder);
        }
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::setVideoEncodingSmartP(VencSmartFun *pParam)
{
    mSmartPParam = *pParam;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        if(PT_H264 == mVideoEncoder || PT_H265 == mVideoEncoder)
        {
            AW_MPI_VENC_SetSmartP(mVeChn, pParam);
        }
        else
        {
            aloge("fatal error! encoder[0x%x] don't support smartP!", mVideoEncoder);
        }
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::setGopAttr(const VENC_GOP_ATTR_S *pParam)
{
    if(pParam)
    {
        mVEncChnAttr.GopAttr = *pParam;
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::getGopAttr(VENC_GOP_ATTR_S *pParam)
{
    if(pParam)
    {
        *pParam = mVEncChnAttr.GopAttr;
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::setRefParam(const VENC_PARAM_REF_S * pstRefParam)
{
    if(pstRefParam)
    {
        mVEncRefParam = *pstRefParam;
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::getRefParam(VENC_PARAM_REF_S * pstRefParam)
{
    if(pstRefParam)
    {
        *pstRefParam = mVEncRefParam;
    }
    return NO_ERROR;
}
#endif

status_t EyeseeRecorder::setMuteMode(bool mute)
{
    mMuteMode = mute;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        if(mAiDev >=0 && mAiChn >= 0)
        {
            AW_MPI_AI_SetChnMute(mAiDev, mAiChn, mMuteMode?TRUE:FALSE);
        }
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::setMaxDuration(int max_duration_ms)
{
    Mutex::Autolock autoLock(mLock);
    mMaxFileDuration = max_duration_ms;
    return NO_ERROR;
}

/**
  set duration to indicated muxer.

  @param max_duration_ms
    unit:ms
  @return
    NO_ERROR
    PERMISSION_DENIED: maybe because muxChn is in switch file process, can't change file duration.
    UNKNOWN_ERROR
*/
status_t EyeseeRecorder::setMaxDuration(int muxer_id, int max_duration_ms)
{
    Mutex::Autolock autoLock(mLock);
    int idx = 0;
    status_t result = NO_ERROR;
    bool bSuccessFlag = false;
    for(OutputSinkInfo& elem: mSinkInfos)
    {
        if(elem.mMuxerId == muxer_id)
        {
            elem.mMaxDurationMs = max_duration_ms;
            bSuccessFlag = true;
            break;
        }
        idx++;
    }
    if(bSuccessFlag)
    {
        if (mCurrentState & (MEDIA_RECORDER_PREPARED|MEDIA_RECORDER_RECORDING|MEDIA_RECORDER_PAUSE))
        {
//            if(mMuxChnAttrs[idx].mMuxerId != muxer_id)
//            {
//                aloge("fatal error! RecorderId[%d] suffix[%d] muxChn[%d] muxerId[%d!=%d]", mRecorderId, idx, mMuxChns[idx], mMuxChnAttrs[idx].mMuxerId, muxer_id);
//            }
            mMuxChnAttrs[idx].mMaxFileDuration = max_duration_ms;

            MUX_CHN_ATTR_S stChnAttr;
            ERRORTYPE ret = AW_MPI_MUX_GetChnAttr(mMuxChns[idx], &stChnAttr);
            if(SUCCESS == ret)
            {
                stChnAttr.mMaxFileDuration = max_duration_ms;
                ret = AW_MPI_MUX_SetChnAttr(mMuxChns[idx], &stChnAttr);
                if(ret != SUCCESS)
                {
                    alogd("RecorderId[%d] muxChn[%d-%d] change max duration fail", mRecorderId, mMuxChns[idx], muxer_id);
                    result = PERMISSION_DENIED;
                }
            }
        }
    }
    else
    {
        aloge("fatal error! RecorderId[%d] find not muxerId[%d]", mRecorderId, muxer_id);
        result = UNKNOWN_ERROR;
    }
    return result;
}

status_t EyeseeRecorder::setMaxFileSize(int64_t max_filesize_bytes)
{
    Mutex::Autolock autoLock(mLock);
    mMaxFileSizeBytes = max_filesize_bytes;
    if (mMaxFileSizeBytes > (int64_t)MAX_FILE_SIZE)
    {
        aloge("fatal error! maxFileSizeBytes[%lld] bigger than max[%lld]", mMaxFileSizeBytes, MAX_FILE_SIZE);
    }
    return OK;
}

status_t EyeseeRecorder::setAudioEncoder(PAYLOAD_TYPE_E audio_encoder)
{
    mAudioEncoder = audio_encoder;
    return NO_ERROR;
}

#if 0
status_t EyeseeRecorder::setVideoEncoder(PAYLOAD_TYPE_E video_encoder)
{
    mVideoEncoder = video_encoder;
    return NO_ERROR;
}
#endif

status_t EyeseeRecorder::setAudioSamplingRate(int samplingRate)
{
    if ((samplingRate!=AUDIO_SAMPLE_RATE_8000 ) &&
        (samplingRate!=AUDIO_SAMPLE_RATE_12000) &&
        (samplingRate!=AUDIO_SAMPLE_RATE_11025) &&
        (samplingRate!=AUDIO_SAMPLE_RATE_16000) &&
        (samplingRate!=AUDIO_SAMPLE_RATE_22050) &&
        (samplingRate!=AUDIO_SAMPLE_RATE_24000) &&
        (samplingRate!=AUDIO_SAMPLE_RATE_32000) &&
        (samplingRate!=AUDIO_SAMPLE_RATE_44100) &&
        (samplingRate!=AUDIO_SAMPLE_RATE_48000) )
    {
        alogw("wrong audio SampleRate(%d) setting, change to default(8000)!", samplingRate);
        samplingRate = 8000;
    }
    mSampleRate = samplingRate;

    return NO_ERROR;
}

status_t EyeseeRecorder::setAudioChannels(int numChannels)
{
    if ((numChannels!=1) &&
        (numChannels!=2) )
    {
        alogw("wrong audio TrackCnt(%d) setting, change to default(1)!", numChannels);
        numChannels = 1;
    }
    mAudioChannels = numChannels;
    return NO_ERROR;
}

status_t EyeseeRecorder::setAudioEncodingBitRate(int bitRate)
{
    mAudioBitRate = bitRate;
    return NO_ERROR;
}

status_t EyeseeRecorder::setAudioSource(int audio_source)
{
    if (audio_source != AudioSource::MIC && audio_source != AudioSource::DEFAULT)
    {
		aloge("not support audio source[0x%x] now", audio_source);
		return BAD_VALUE;
	}
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_IDLE)
    {
        alogv("Call init() since the media recorder is not initialized yet");
        status_t ret = init();
        if (OK != ret)
        {
            return ret;
        }
    }
    if (mIsAudioSourceSet)
    {
        aloge("audio source has already been set");
        return INVALID_OPERATION;
    }
    if (!(mCurrentState & MEDIA_RECORDER_INITIALIZED))
    {
        aloge("setAudioSource called in an invalid state(%d)", mCurrentState);
        return INVALID_OPERATION;
    }

    mAudioSource = audio_source;

    mIsAudioSourceSet = true;
    return NO_ERROR;
}

status_t EyeseeRecorder::setVideoSource(int video_source)
{
    if (video_source != VideoSource::CAMERA && video_source != VideoSource::DEFAULT)
    {
		aloge("not support video source[0x%x] now", video_source);
		return BAD_VALUE;
	}
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_IDLE)
    {
        alogv("Call init() since the media recorder is not initialized yet");
        status_t ret = init();
        if (OK != ret)
        {
            return ret;
        }
    }
    if (mIsVideoSourceSet)
    {
        aloge("video source has already been set");
        return INVALID_OPERATION;
    }
    if (!(mCurrentState & MEDIA_RECORDER_INITIALIZED))
    {
        aloge("setVideoSource called in an invalid state(%d)", mCurrentState);
        return INVALID_OPERATION;
    }

    mVideoSource = video_source;

    mIsVideoSourceSet = true;
    return NO_ERROR;
}
#if 0
int EyeseeRecorder::addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_E output_format, int fd, int FallocateLen, bool callback_out_flag)
{
    int retMuxerId = -1;
    alogv("addOutputFormatAndOutputSink(%d)", output_format);
    Mutex::Autolock autoLock(mLock);
    if (!(mCurrentState & MEDIA_RECORDER_INITIALIZED) && !(mCurrentState & MEDIA_RECORDER_DATASOURCE_CONFIGURED) && !(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("addOutputFormatAndOutputSink called in an invalid state: %d", mCurrentState);
        return -1;
    }
    if (mIsVideoSourceSet && judgeAudioFileFormat(output_format))
    { //first non-video output format
        aloge("output format (%d) is meant for audio recording only and incompatible with video recording", output_format);
        return -1;
    }

    alogd("(of:0x%x, fd:%d, FallocateLen:%d, callback_out_flag:%d)", output_format, fd, FallocateLen, callback_out_flag);
    if(fd >= 0 && true == callback_out_flag)
    {
        aloge("fatal error! one muxer cannot support two sink methods!");
        return -1;
    }
    //find if the same output_format sinkInfo exist or callback out stream is exist.
    for(std::vector<OutputSinkInfo>::iterator it = mSinkInfos.begin(); it != mSinkInfos.end(); ++it)
    {
        if(it->mOutputFormat == output_format)
        {
            alogd("Be careful! same outputForamt[0x%x] exist in array", output_format);
        }
        if(callback_out_flag && it->mCallbackOutFlag == callback_out_flag)
        {
            aloge("fatal error! only support one callback out stream.");
        }
    }
    OutputSinkInfo sinkInfo;
    sinkInfo.mMuxerId = mMuxerIdCounter;
    sinkInfo.mOutputFormat = output_format;
    if(fd >= 0)
    {
        sinkInfo.mOutputFd = dup(fd);
        //sinkInfo.mOutputFd = dup2SeldomUsedFd(fd);
    }
    else
    {
        sinkInfo.mOutputFd = -1;
    }
    sinkInfo.mFallocateLen = FallocateLen;
    sinkInfo.mCallbackOutFlag = callback_out_flag;
    mIsOutputFileSet = true;

    //config mMuxChnAttrs.
    MUX_CHN_ATTR_S muxChnAttr;
    muxChnAttr.mMuxerId = sinkInfo.mMuxerId;
    muxChnAttr.mMuxerGrpId = mMuxGrp;
    muxChnAttr.mMediaFileFormat = sinkInfo.mOutputFormat;
    muxChnAttr.mMaxFileDuration = mMaxFileDuration;
    muxChnAttr.mMaxFileSizeBytes = mMaxFileSizeBytes;
    muxChnAttr.mFallocateLen = sinkInfo.mFallocateLen;
    muxChnAttr.mCallbackOutFlag = sinkInfo.mCallbackOutFlag;
    muxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    muxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
    muxChnAttr.bBufFromCacheFlag = false;

    if(mCurrentState==MEDIA_RECORDER_PREPARED || mCurrentState==MEDIA_RECORDER_RECORDING)
    {
        ERRORTYPE ret;
        BOOL nSuccessFlag = FALSE;
        MUX_CHN nMuxChn = 0;
        while(nMuxChn < MUX_MAX_CHN_NUM)
        {
            ret = AW_MPI_MUX_CreateChn(mMuxGrp, nMuxChn, &muxChnAttr, sinkInfo.mOutputFd);
            if(SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create mux group[%d] channel[%d] success, muxerId[%d]!", mMuxGrp, nMuxChn, muxChnAttr.mMuxerId);
                break;
            }
            else if(ERR_MUX_EXIST == ret)
            {
                alogv("mux group[%d] channel[%d] is exist, find next!", mMuxGrp, nMuxChn);
                nMuxChn++;
            }
            else
            {
                aloge("fatal error! create mux group[%d] channel[%d] fail ret[0x%x], find next!", mMuxGrp, nMuxChn, ret);
                nMuxChn++;
            }
        }
        if(nSuccessFlag)
        {
            mMuxChns.push_back(nMuxChn);
            mSinkInfos.push_back(sinkInfo);
            mMuxChnAttrs.push_back(muxChnAttr);
            mPolicy.insert(std::make_pair(muxChnAttr.mMuxerId, RecordFileDurationPolicy_AverageDuration));
            retMuxerId = sinkInfo.mMuxerId;
            mMuxerIdCounter++;
        }
        else
        {
            aloge("fatal error! create mux group[%d] channel fail!", mMuxGrp);
            if(sinkInfo.mOutputFd>=0)
            {
                ::close(sinkInfo.mOutputFd);
                sinkInfo.mOutputFd = -1;
            }
            retMuxerId = -1;
        }
    }
    else
    {
        mMuxChns.push_back(MM_INVALID_CHN);
        mSinkInfos.push_back(sinkInfo);
        mMuxChnAttrs.push_back(muxChnAttr);
        retMuxerId = sinkInfo.mMuxerId;
        mPolicy.insert(std::make_pair(muxChnAttr.mMuxerId, RecordFileDurationPolicy_AverageDuration));
        mMuxerIdCounter++;
    }

    if ((mCurrentState & MEDIA_RECORDER_INITIALIZED))
    {
        mCurrentState = MEDIA_RECORDER_DATASOURCE_CONFIGURED;
    }
    return retMuxerId;
}

int EyeseeRecorder::addOutputFormatAndOutputSink(MEDIA_FILE_FORMAT_E output_format, char* path, int FallocateLen, bool callback_out_flag)
{
    int muxerId = -1;
    if(path!=NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT, 0666);
	if (fd < 0)
        {
		aloge("Failed to open %s", path);
		return -1;
	}
        muxerId = addOutputFormatAndOutputSink(output_format, fd, FallocateLen, callback_out_flag);
        ::close(fd);
    }
    return muxerId;
}
#endif
/**
  add muxerId

  @return
    muxerId
    -1
    INVALID_OPERATION
    BAD_VALUE
    BAD_FILE
    ALREADY_EXISTS: if bufFromCache sink is exist, can't add more.
*/
int EyeseeRecorder::addOutputSink(SinkParam *pSinkParam)
{
    int retMuxerId = -1;
    Mutex::Autolock autoLock(mLock); //for mCurrentState
    std::lock_guard<std::mutex> autoStreamLock(mStreamNodeLock);
    std::lock_guard<std::mutex> autoMuxChnLock(mMuxChnLock);
    alogd("addOutputSink Format:0x%x, fd:%d, path:%s, FallocateLen:%d, callback_out_flag:%d, cache_flag:%d",
           pSinkParam->mOutputFormat, pSinkParam->mOutputFd, pSinkParam->mOutputPath, pSinkParam->mFallocateLen ,
           pSinkParam->bCallbackOutFlag, pSinkParam->bBufFromCacheFlag);
    if (!(mCurrentState & MEDIA_RECORDER_INITIALIZED) && !(mCurrentState & MEDIA_RECORDER_DATASOURCE_CONFIGURED) && !(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("fatal error! addOutputSink called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    if((pSinkParam->mOutputFd >= 0 || pSinkParam->mOutputPath != NULL) && true == pSinkParam->bCallbackOutFlag)
    {
        aloge("fatal error! one muxer cannot support two sink methods!");
        return BAD_VALUE;
    }
    if (pSinkParam->mOutputFormat >= MEDIA_FILE_FORMAT_UNKNOWN)
    {
        aloge("fatal error! called in an invalid OutputFormat: %d", pSinkParam->mOutputFormat);
        return BAD_VALUE;
    }
    if(true == pSinkParam->bBufFromCacheFlag)
    {
        int nFromCacheCnt = 0;
        for(OutputSinkInfo& elem : mSinkInfos)
        {
            if(true == elem.mbBufFromCacheFlag)
            {
                nFromCacheCnt++;
            }
        }
        if(nFromCacheCnt > 0)
        {
            aloge("fatal error! [%d]sinks use cache, only allow one!", nFromCacheCnt);
            return ALREADY_EXISTS;
        }
    }

    OutputSinkInfo sinkInfo;
    sinkInfo.mMuxerId = mMuxerIdCounter;
    sinkInfo.mOutputFormat = pSinkParam->mOutputFormat;
    if(pSinkParam->mOutputFd >= 0)
    {
        sinkInfo.mOutputFd = dup(pSinkParam->mOutputFd);
    }
    else
    {
        sinkInfo.mOutputFd = -1;
        if(pSinkParam->mOutputPath != NULL)
        {
            int fd = open(pSinkParam->mOutputPath, O_RDWR | O_CREAT | O_TRUNC, 0666);
            if (fd < 0)
            {
                aloge("Failed to open %s", pSinkParam->mOutputPath);
                return BAD_FILE;
            }
            sinkInfo.mOutputFd = fd;
        }
    }
    sinkInfo.mFallocateLen = pSinkParam->mFallocateLen;
    sinkInfo.mMaxDurationMs  = pSinkParam->mMaxDurationMs;
    sinkInfo.mCallbackOutFlag = pSinkParam->bCallbackOutFlag;
    sinkInfo.mbBufFromCacheFlag = pSinkParam->bBufFromCacheFlag;
    sinkInfo.mbAddRepairInfo = pSinkParam->bAddRepairInfo;
    mSinkInfos.push_back(sinkInfo);

    if(mCurrentState==MEDIA_RECORDER_PREPARED || mCurrentState==MEDIA_RECORDER_RECORDING)
    {
        OutputSinkInfo *pSinkInfo = &mSinkInfos.back();
        MUX_CHN_ATTR_S muxChnAttr;
        config_MUX_CHN_ATTR_S(&muxChnAttr, pSinkInfo);
        mMuxChnAttrs.push_back(muxChnAttr);

        ERRORTYPE ret;
        BOOL nSuccessFlag = FALSE;
        MUX_CHN nMuxChn = 0;
        while(nMuxChn < MUX_MAX_CHN_NUM)
        {
            ret = AW_MPI_MUX_CreateChn(nMuxChn, &muxChnAttr, pSinkInfo->mOutputFd, pSinkInfo->mFallocateLen);
            if(SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create muxChn[%d] success, muxerId[%d]!", nMuxChn, pSinkInfo->mMuxerId);
                break;
            }
            else if(ERR_MUX_EXIST == ret)
            {
                alogv("muxChn[%d] is exist, find next!", nMuxChn);
                nMuxChn++;
            }
            else
            {
                aloge("fatal error! create muxChn[%d] fail ret[0x%x], find next!", nMuxChn, ret);
                nMuxChn++;
            }
        }
        if(nSuccessFlag)
        {
            mMuxChns.push_back(nMuxChn);
            mPolicyMap.insert(std::make_pair(pSinkInfo->mMuxerId, pSinkParam->mFileDurationPolicy));
            retMuxerId = pSinkInfo->mMuxerId;
            mMuxerIdCounter++;

            if(mCurrentState==MEDIA_RECORDER_RECORDING)
            {
                ret = AW_MPI_MUX_SetSwitchFileDurationPolicy(nMuxChn, pSinkParam->mFileDurationPolicy);
                if(ret != SUCCESS)
                {
                    aloge("fatal error! set file duration policy[%d] to muxChn[%d-%d] fail!", pSinkParam->mFileDurationPolicy, nMuxChn, pSinkInfo->mMuxerId);
                }
                //set VeChn-StreamId binding.
                for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
                {
                    int nStreamId = it->first;
                    VencParameters *pVencParam = it->second;
                    VENC_CHN VeChn = pVencParam->getVencChnIndex();
                    AW_MPI_MUX_SetVeChnBindStreamId(nMuxChn, VeChn, nStreamId);
                }
                std::map<int, MuxStreamIdsInfo>::iterator IterMuxStrmIds = mMuxStrmIdsMap.find(pSinkInfo->mMuxerId);
                if(IterMuxStrmIds != mMuxStrmIdsMap.end())
                {
                    MuxStreamIdsInfo *pMuxStreamIdsInfo = &IterMuxStrmIds->second;
                    alogv("recorder[%d] muxChn[%d-%d] streamIds cnt:%x", mRecorderId, nMuxChn, pSinkInfo->mMuxerId, pMuxStreamIdsInfo->mStrmIdsCnt);
                    AW_MPI_MUX_SetStrmIds(nMuxChn, pMuxStreamIdsInfo);
                }
                //set callback MPPCallbackInfo cbInfo;
                MPPCallbackInfo cbInfo;
                cbInfo.cookie = (void*)this;
                cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
                AW_MPI_MUX_RegisterCallback(nMuxChn, &cbInfo);
                //set mux group attr & sps
                for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
                {
                    VencParameters *pVencParam = it->second;
                    VENC_CHN VeChn = pVencParam->getVencChnIndex();
                    PAYLOAD_TYPE_E VideoEncoder = pVencParam->getVideoEncoder();
                    if ((VeChn >= 0) && (VideoEncoder != PT_MAX))
                    {
                        setmuxChnsps(VeChn, VideoEncoder, nMuxChn);
                    }
                }

                ret = AW_MPI_MUX_StartChn(nMuxChn);
                if(ret != SUCCESS)
                {
                    aloge("fatal error! RecorderId[%d] muxChn[%d] start fail[0x%x]!", mRecorderId, nMuxChn, ret);
                }
                if(pSinkInfo->mbBufFromCacheFlag)
                {
                    if(mpMuxCacheManager)
                    {
                        SendAllCacheManagerStreamToMuxChn(nMuxChn);
                    }
                    else
                    {
                        aloge("fatal error! RecorderId[%d] has not cache manager!", mRecorderId);
                    }
                }
            }
        }
        else
        {
            mMuxChns.push_back(MM_INVALID_CHN);
            aloge("fatal error! create mux channel fail!");
            retMuxerId = -1;
        }
    }
    else
    {
        OutputSinkInfo *pSinkInfo = &mSinkInfos.back();
        //mMuxChns.push_back(MM_INVALID_CHN);
        //mMuxChnAttrs.push_back(muxChnAttr);
        mPolicyMap.insert(std::make_pair(pSinkInfo->mMuxerId, pSinkParam->mFileDurationPolicy)); //RecordFileDurationPolicy_AverageDuration
        retMuxerId = pSinkInfo->mMuxerId;
        mMuxerIdCounter++;
    }
    
    if ((mCurrentState & MEDIA_RECORDER_INITIALIZED))
    {
        mCurrentState = MEDIA_RECORDER_DATASOURCE_CONFIGURED;
    }
    return retMuxerId;
}

/**
  bind VencId and Vipp. Tell recorder send which vipp frame to which vencId.
  need called before prepare.
*/
status_t EyeseeRecorder::bindVeVipp(int VencId, VI_DEV Vipp)
{
    auto ret = mVeVippBindMap.insert(std::make_pair(VencId, Vipp));
    if (ret.second != true)
    {
        aloge("fatal error! VencId[%d] bind Vipp[%d] fail!", VencId, Vipp);
        return BAD_VALUE;
    }
    alogd("Venc[%d] bind Vipp[%d] is successful!", VencId, Vipp);

    return NO_ERROR;
}

status_t EyeseeRecorder::getVencParameters(int VencId, VencParameters &param)
{
    std::map<int, VencParameters*>::iterator it =  mVencInfoMap.find(VencId);
    if (mVencInfoMap.end() == it)
    {
        aloge("fatal error! VencId[%d] find VencParameters fail", VencId);
        return BAD_VALUE;
    }
    VencParameters *pVencParam = it->second;
    param = *pVencParam;

    return NO_ERROR;
}

/**
  set venc parameters.
  VencId is streamId of video. called before prepare().
*/
status_t EyeseeRecorder::setVencParameters(int VencId, VencParameters *pVencParam)
{
    Mutex::Autolock autoLock(mLock);
    VencParameters *pCurVencParam = NULL;
    //if VEncInfo map haven't this VEncId
    if (mVencInfoMap.empty() || (mVencInfoMap.end() == mVencInfoMap.find(VencId)))
    {
        pCurVencParam = new VencParameters;
        if (NULL == pCurVencParam)
        {
            aloge("fatal error! new pCurVencParam is fail!");
            return NO_MEMORY;
        }
        auto insertRet = mVencInfoMap.insert(std::make_pair(VencId, pCurVencParam));
        if(insertRet.second != true)
        {
            aloge("fatal error! VencId[%d] parameters insert fail!", VencId);
            return BAD_VALUE;
        }
        *pCurVencParam = *pVencParam;
        if(!(mCurrentState & (MEDIA_RECORDER_IDLE | MEDIA_RECORDER_INITIALIZED | MEDIA_RECORDER_DATASOURCE_CONFIGURED)))
        {
            aloge("fatal error! add new vencParameters in wrong state[0x%x]", mCurrentState);
        }
        return NO_ERROR;
    }
    else
    {
        pCurVencParam = mVencInfoMap.find(VencId)->second;
    }

    if (!(mCurrentState & (MEDIA_RECORDER_PREPARED | MEDIA_RECORDER_RECORDING)))
    {
        *pCurVencParam = *pVencParam;
        return NO_ERROR;
    }

    //---------below is dynamic processing. Only change dynamic setting which can be changed.-------------
    ERRORTYPE eRet = SUCCESS;
    VENC_CHN VeChn = pCurVencParam->getVencChnIndex();
    //set video framerate
    int oldFrameRate, newFrameRate;
    oldFrameRate = pCurVencParam->getVideoFrameRate();
    newFrameRate = pVencParam->getVideoFrameRate();
    if (oldFrameRate != newFrameRate)
    {
        alogd("VencId[%d], VencParam dstVideoFrame change[%d]->[%d]", VencId, oldFrameRate, newFrameRate);
        pCurVencParam->setVideoFrameRate(newFrameRate);
        //set mpi_venc
        {
            VENC_FRAME_RATE_S stFrameRate;
            AW_MPI_VENC_GetFrameRate(VeChn, &stFrameRate);
            stFrameRate.DstFrmRate = newFrameRate;
            eRet = AW_MPI_VENC_SetFrameRate(VeChn, &stFrameRate);
            if(eRet != SUCCESS)
            {
                aloge("fatal error! ret=0x%x", eRet);
            }
        }
    }

    //set encpp sharp
    VencEncppSharpSettingE oldEncppSharpSetting, newEncppSharpSetting;
    oldEncppSharpSetting = pCurVencParam->getEncppSharpSetting();
    newEncppSharpSetting = pVencParam->getEncppSharpSetting();
    if (oldEncppSharpSetting != newEncppSharpSetting)
    {
        pCurVencParam->setEncppSharpSetting(newEncppSharpSetting);
        alogd("VencId[%d], VencParam encpp sharp setting change[%d]->[%d]", VencId, oldEncppSharpSetting, newEncppSharpSetting);
        VENC_CHN_ATTR_S attr;
        AW_MPI_VENC_GetChnAttr(VeChn, &attr);
        attr.EncppAttr.eEncppSharpSetting = newEncppSharpSetting;
        eRet = AW_MPI_VENC_SetChnAttr(VeChn, &attr);
        if(eRet != SUCCESS)
        {
            aloge("fatal error! ret=0x%x", eRet);
        }
    }

    //set isp2ve linkage
    bool oldIspAndVeLinkEnable, newIspAndVeLinkEnable;
    oldIspAndVeLinkEnable = pCurVencParam->getIspAndVeLinkEnable();
    newIspAndVeLinkEnable = pVencParam->getIspAndVeLinkEnable();
    if (oldIspAndVeLinkEnable != newIspAndVeLinkEnable)
    {
        pCurVencParam->enableIspAndVeLink(newIspAndVeLinkEnable);
        alogd("VencId[%d], VencParam isp and ve link neable change[%d]->[%d]", VencId, oldIspAndVeLinkEnable, newIspAndVeLinkEnable);
    }

    //set encpp sharp attenuation coefficent percentage
    int oldEncppSharpAttenCoefPer, newEncppSharpAttenCoefPer;
    oldEncppSharpAttenCoefPer = pCurVencParam->getEncppSharpAttenCoefPer();
    newEncppSharpAttenCoefPer = pVencParam->getEncppSharpAttenCoefPer();
    if (oldEncppSharpAttenCoefPer != newEncppSharpAttenCoefPer)
    {
        pCurVencParam->setEncppSharpAttenCoefPer(newEncppSharpAttenCoefPer);
        alogd("VencId[%d], VencParam isp and ve link neable change[%d]->[%d]", VencId, oldEncppSharpAttenCoefPer, newEncppSharpAttenCoefPer);
    }

    //set video encoding I frames number interval
    int oldIDRFrameInterval, newIDRFrameInterval;
    oldIDRFrameInterval = pCurVencParam->getVideoEncodingIFramesNumberInterVal();
    newIDRFrameInterval = pVencParam->getVideoEncodingIFramesNumberInterVal();
    if (oldIDRFrameInterval != newIDRFrameInterval)
    {
        alogd("VencId[%d] VEncParam IDRFrameInterval change[%d]->[%d]", VencId, oldIDRFrameInterval, newIDRFrameInterval);
        pCurVencParam->setVideoEncodingIFramesNumberInterVal(newIDRFrameInterval);
        {
            VENC_CHN_ATTR_S attr;
            AW_MPI_VENC_GetChnAttr(VeChn, &attr);
            attr.VeAttr.MaxKeyInterval = newIDRFrameInterval;
            eRet = AW_MPI_VENC_SetChnAttr(VeChn, &attr);
            if(eRet != SUCCESS)
            {
                aloge("fatal error! ret=0x%x", eRet);
            }
        }
    }

    //set VEnc bit rate control attr: bitrate, qp.
    VENC_CHN_ATTR_S oldVencChnAttr = pCurVencParam->getVencChnAttr();
    VENC_CHN_ATTR_S newVencChnAttr = pVencParam->getVencChnAttr();
    VENC_RC_PARAM_S oldRcAttr = pCurVencParam->getVencRcParam();
    VENC_RC_PARAM_S newRcAttr = pVencParam->getVencRcParam();
    if(oldVencChnAttr.VeAttr.Type != newVencChnAttr.VeAttr.Type)
    {
        aloge("fatal error! vencId[%d] VEncType is not match, [%d]!=[%d]", VencId, oldVencChnAttr.VeAttr.Type, newVencChnAttr.VeAttr.Type);
    }
    if ((oldVencChnAttr.VeAttr.Type == newVencChnAttr.VeAttr.Type) && (oldVencChnAttr.RcAttr.mRcMode == newVencChnAttr.RcAttr.mRcMode))
    {
        if (PT_H264 == newVencChnAttr.VeAttr.Type)
        {
            if(VENC_RC_MODE_H264CBR == newVencChnAttr.RcAttr.mRcMode)
            {
                //bitRate
                if(oldVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate != newVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate)
                {
                    alogd("need update h264 cbr bitRate[%d]->[%d]", oldVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate, newVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate);
                    VENC_CHN_ATTR_S attr;
                    AW_MPI_VENC_GetChnAttr(VeChn, &attr);
                    if(attr.RcAttr.mRcMode!=VENC_RC_MODE_H264CBR)
                    {
                        aloge("fatal error! check mpp_rcMode[0x%x]", attr.RcAttr.mRcMode);
                    }
                    attr.RcAttr.mAttrH264Cbr.mBitRate = newVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate;
                    AW_MPI_VENC_SetChnAttr(VeChn, &attr);
                    oldVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = newVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate;
                }
                //qp control
                if(oldRcAttr.ParamH264Cbr.mMaxQp != newRcAttr.ParamH264Cbr.mMaxQp
                    || oldRcAttr.ParamH264Cbr.mMinQp != newRcAttr.ParamH264Cbr.mMinQp
                    || oldRcAttr.ParamH264Cbr.mMaxPqp != newRcAttr.ParamH264Cbr.mMaxPqp
                    || oldRcAttr.ParamH264Cbr.mMinPqp != newRcAttr.ParamH264Cbr.mMinPqp
                    || oldRcAttr.ParamH264Cbr.mQpInit != newRcAttr.ParamH264Cbr.mQpInit
                    || oldRcAttr.ParamH264Cbr.mbEnMbQpLimit != newRcAttr.ParamH264Cbr.mbEnMbQpLimit)
                {
                    alogd("need update h264 qpInfo[%d,%d,%d,%d,%d,%d]->[%d,%d,%d,%d,%d,%d]",
                        oldRcAttr.ParamH264Cbr.mMaxQp, oldRcAttr.ParamH264Cbr.mMinQp, oldRcAttr.ParamH264Cbr.mMaxPqp,
                        oldRcAttr.ParamH264Cbr.mMinPqp, oldRcAttr.ParamH264Cbr.mQpInit, oldRcAttr.ParamH264Cbr.mbEnMbQpLimit,
                        newRcAttr.ParamH264Cbr.mMaxQp, newRcAttr.ParamH264Cbr.mMinQp, newRcAttr.ParamH264Cbr.mMaxPqp,
                        newRcAttr.ParamH264Cbr.mMinPqp, newRcAttr.ParamH264Cbr.mQpInit, newRcAttr.ParamH264Cbr.mbEnMbQpLimit);
                    VENC_RC_PARAM_S stRcParam;
                    AW_MPI_VENC_GetRcParam(VeChn, &stRcParam);
                    stRcParam.ParamH264Cbr.mMaxQp = newRcAttr.ParamH264Cbr.mMaxQp;
                    stRcParam.ParamH264Cbr.mMinQp = newRcAttr.ParamH264Cbr.mMinQp;
                    stRcParam.ParamH264Cbr.mMaxPqp = newRcAttr.ParamH264Cbr.mMaxPqp;
                    stRcParam.ParamH264Cbr.mMinPqp = newRcAttr.ParamH264Cbr.mMinPqp;
                    stRcParam.ParamH264Cbr.mQpInit = newRcAttr.ParamH264Cbr.mQpInit;
                    stRcParam.ParamH264Cbr.mbEnMbQpLimit = newRcAttr.ParamH264Cbr.mbEnMbQpLimit;
                    eRet = AW_MPI_VENC_SetRcParam(VeChn, &stRcParam);
                    if(SUCCESS == eRet)
                    {
                        oldRcAttr.ParamH264Cbr.mMaxQp = newRcAttr.ParamH264Cbr.mMaxQp;
                        oldRcAttr.ParamH264Cbr.mMinQp = newRcAttr.ParamH264Cbr.mMinQp;
                        oldRcAttr.ParamH264Cbr.mMaxPqp = newRcAttr.ParamH264Cbr.mMaxPqp;
                        oldRcAttr.ParamH264Cbr.mMinPqp = newRcAttr.ParamH264Cbr.mMinPqp;
                        oldRcAttr.ParamH264Cbr.mQpInit = newRcAttr.ParamH264Cbr.mQpInit;
                        oldRcAttr.ParamH264Cbr.mbEnMbQpLimit = newRcAttr.ParamH264Cbr.mbEnMbQpLimit;
                    }
                    else
                    {
                        aloge("fatal error! rcMode[%d] of encType[%d] set rc param fail!", newVencChnAttr.RcAttr.mRcMode, newVencChnAttr.VeAttr.Type);
                    }
                }
            }
            else
            {
                alogw("Be careful! temporary don't support other rcMode[%d] of encType[%d] change dynamically!", newVencChnAttr.RcAttr.mRcMode, newVencChnAttr.VeAttr.Type);
            }
        }
        else if (PT_H265 == newVencChnAttr.VeAttr.Type)
        {
            if(VENC_RC_MODE_H265CBR == newVencChnAttr.RcAttr.mRcMode)
            {
                //bitRate
                if(oldVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate != newVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate)
                {
                    alogd("need update h265 cbr bitRate[%d]->[%d]", oldVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate, newVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate);
                    VENC_CHN_ATTR_S attr;
                    AW_MPI_VENC_GetChnAttr(VeChn, &attr);
                    if(attr.RcAttr.mRcMode!=VENC_RC_MODE_H265CBR)
                    {
                        aloge("fatal error! check mpp_rcMode[0x%x]", attr.RcAttr.mRcMode);
                    }
                    attr.RcAttr.mAttrH265Cbr.mBitRate = newVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate;
                    AW_MPI_VENC_SetChnAttr(VeChn, &attr);
                    oldVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = newVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate;
                }
                //qp control
                if(oldRcAttr.ParamH265Cbr.mMaxQp != newRcAttr.ParamH265Cbr.mMaxQp
                    || oldRcAttr.ParamH265Cbr.mMinQp != newRcAttr.ParamH265Cbr.mMinQp
                    || oldRcAttr.ParamH265Cbr.mMaxPqp != newRcAttr.ParamH265Cbr.mMaxPqp
                    || oldRcAttr.ParamH265Cbr.mMinPqp != newRcAttr.ParamH265Cbr.mMinPqp
                    || oldRcAttr.ParamH265Cbr.mQpInit != newRcAttr.ParamH265Cbr.mQpInit
                    || oldRcAttr.ParamH265Cbr.mbEnMbQpLimit != newRcAttr.ParamH265Cbr.mbEnMbQpLimit)
                {
                    alogd("need update h265 qpInfo[%d,%d,%d,%d,%d,%d]->[%d,%d,%d,%d,%d,%d]",
                        oldRcAttr.ParamH265Cbr.mMaxQp, oldRcAttr.ParamH265Cbr.mMinQp, oldRcAttr.ParamH265Cbr.mMaxPqp,
                        oldRcAttr.ParamH265Cbr.mMinPqp, oldRcAttr.ParamH265Cbr.mQpInit, oldRcAttr.ParamH265Cbr.mbEnMbQpLimit,
                        newRcAttr.ParamH265Cbr.mMaxQp, newRcAttr.ParamH265Cbr.mMinQp, newRcAttr.ParamH265Cbr.mMaxPqp,
                        newRcAttr.ParamH265Cbr.mMinPqp, newRcAttr.ParamH265Cbr.mQpInit, newRcAttr.ParamH265Cbr.mbEnMbQpLimit);
                    VENC_RC_PARAM_S stRcParam;
                    AW_MPI_VENC_GetRcParam(VeChn, &stRcParam);
                    stRcParam.ParamH265Cbr.mMaxQp = newRcAttr.ParamH265Cbr.mMaxQp;
                    stRcParam.ParamH265Cbr.mMinQp = newRcAttr.ParamH265Cbr.mMinQp;
                    stRcParam.ParamH265Cbr.mMaxPqp = newRcAttr.ParamH265Cbr.mMaxPqp;
                    stRcParam.ParamH265Cbr.mMinPqp = newRcAttr.ParamH265Cbr.mMinPqp;
                    stRcParam.ParamH265Cbr.mQpInit = newRcAttr.ParamH265Cbr.mQpInit;
                    stRcParam.ParamH265Cbr.mbEnMbQpLimit = newRcAttr.ParamH265Cbr.mbEnMbQpLimit;
                    eRet = AW_MPI_VENC_SetRcParam(VeChn, &stRcParam);
                    if(SUCCESS == eRet)
                    {
                        oldRcAttr.ParamH265Cbr.mMaxQp = newRcAttr.ParamH265Cbr.mMaxQp;
                        oldRcAttr.ParamH265Cbr.mMinQp = newRcAttr.ParamH265Cbr.mMinQp;
                        oldRcAttr.ParamH265Cbr.mMaxPqp = newRcAttr.ParamH265Cbr.mMaxPqp;
                        oldRcAttr.ParamH265Cbr.mMinPqp = newRcAttr.ParamH265Cbr.mMinPqp;
                        oldRcAttr.ParamH265Cbr.mQpInit = newRcAttr.ParamH265Cbr.mQpInit;
                        oldRcAttr.ParamH265Cbr.mbEnMbQpLimit = newRcAttr.ParamH265Cbr.mbEnMbQpLimit;
                    }
                    else
                    {
                        aloge("fatal error! rcMode[%d] of encType[%d] set rc param fail!", newVencChnAttr.RcAttr.mRcMode, newVencChnAttr.VeAttr.Type);
                    }
                }
            }
            else
            {
                alogw("Be careful! temporary don't support other rcMode[%d] of encType[%d] change dynamically!", newVencChnAttr.RcAttr.mRcMode, newVencChnAttr.VeAttr.Type);
            }
        }
        else if (PT_MJPEG == newVencChnAttr.VeAttr.Type)
        {
            if(VENC_RC_MODE_MJPEGCBR == newVencChnAttr.RcAttr.mRcMode)
            {
                if (oldVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate != newVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate)
                {
                    alogd("VencParam mAttrH265Cbr.mBitRate[%]->[%d]", oldVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate, newVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate);
                    {
                        VENC_CHN_ATTR_S attr;
                        AW_MPI_VENC_GetChnAttr(VeChn, &attr);
                        if (attr.RcAttr.mRcMode != VENC_RC_MODE_MJPEGCBR)
                        {
                            aloge("fatal error! check mpp_rcMode[0x%x]", attr.RcAttr.mRcMode);
                        }
                        attr.RcAttr.mAttrMjpegeCbr.mBitRate = newVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate;
                        AW_MPI_VENC_SetChnAttr(VeChn, &attr);
                        oldVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = newVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate;
                    }
                }
            }
            else
            {
                alogw("Be careful! temporary don't support other rcMode[%d] of encType[%d] change dynamically!",
                    newVencChnAttr.RcAttr.mRcMode, newVencChnAttr.VeAttr.Type);
            }
        }
        pCurVencParam->setVencChnAttr(oldVencChnAttr);
        pCurVencParam->setVencRcParam(oldRcAttr);
    }
    else
    {
        aloge("fatal error! VEncType[%d][%d] or RcMode[%d][%d] is not match!", oldVencChnAttr.VeAttr.Type, newVencChnAttr.VeAttr.Type,
            oldVencChnAttr.RcAttr.mRcMode, newVencChnAttr.RcAttr.mRcMode);
    }

    //set video encoding smart P
    VencSmartFun oldsmartParam, newsmartParam;
    oldsmartParam = pCurVencParam->getVideoEncodingSmartP();
    newsmartParam = pVencParam->getVideoEncodingSmartP();
    if ((oldsmartParam.smart_fun_en != newsmartParam.smart_fun_en) ||
        (oldsmartParam.img_bin_en != newsmartParam.img_bin_en) ||
        (oldsmartParam.img_bin_th != newsmartParam.img_bin_th) ||
        (oldsmartParam.shift_bits != newsmartParam.shift_bits))
    {
        alogd("VEncParam smartParam change, smart_fun_en[%d]->[%d]"
              "img_bin_en[%d]->[%d]"
              "img_bin_th[%d]->[%d]"
              "shift_bits[%d]->[%d]",
              oldsmartParam.smart_fun_en, newsmartParam.smart_fun_en,
              oldsmartParam.img_bin_en, newsmartParam.img_bin_en,
              oldsmartParam.img_bin_th, newsmartParam.img_bin_th,
              oldsmartParam.shift_bits, newsmartParam.shift_bits);
        pCurVencParam->setVideoEncodingSmartP(newsmartParam);
        {
            PAYLOAD_TYPE_E VideoEncoder = pCurVencParam->getVideoEncoder();
            if(PT_H264 == VideoEncoder || PT_H265 == VideoEncoder)
            {
                AW_MPI_VENC_SetSmartP(VeChn, &newsmartParam);
            }
            else
            {
                aloge("fatal error! encoder[0x%x] don't support smartP!", VideoEncoder);
            }
        }
    }

    //set video encoding intra refresh
    VencCyclicIntraRefresh oldstIntraRefresh, newstIntraRefresh;
    oldstIntraRefresh = pCurVencParam->getVideoEncodingIntraRefresh();
    newstIntraRefresh = pVencParam->getVideoEncodingIntraRefresh();
    if ((oldstIntraRefresh.bEnable != newstIntraRefresh.bEnable)
        || (oldstIntraRefresh.nBlockNumber != newstIntraRefresh.nBlockNumber))
    {
        alogd("VEncParam IntraRefreshParam change, bEnable[%d]->[%d], nBlockNumber[%d]->[%d]",
              oldstIntraRefresh.bEnable, newstIntraRefresh.bEnable,
              oldstIntraRefresh.nBlockNumber, newstIntraRefresh.nBlockNumber);
        pCurVencParam->setVideoEncodingIntraRefresh(newstIntraRefresh);
        {
            PAYLOAD_TYPE_E VideoEncoder = pCurVencParam->getVideoEncoder();
            if(PT_H264 == VideoEncoder || PT_H265 == VideoEncoder)
            {
                AW_MPI_VENC_SetIntraRefresh(VeChn, &newstIntraRefresh);
            }
            else
            {
                aloge("fatal error! encoder[0x%x] don't support IntraRefresh!", VideoEncoder);
            }
        }
    }

    //enable Horizon Filp
    bool oldHorizonFilpFlag, newHorizonFilpFlag;
    oldHorizonFilpFlag = pCurVencParam->getHorizonFilpFlag();
    newHorizonFilpFlag = pVencParam->getHorizonFilpFlag();
    if (oldHorizonFilpFlag != newHorizonFilpFlag)
    {
        alogd("VEncParam HorizonFilp change[%d]->[%d]", oldHorizonFilpFlag, newHorizonFilpFlag);
        pCurVencParam->enableHorizonFlip(newHorizonFilpFlag);
        {
            BOOL bHorizonFlipFlag;
            if(newHorizonFilpFlag)
            {
                bHorizonFlipFlag = TRUE;
            }
            else
            {
                bHorizonFlipFlag = FALSE;
            }
            AW_MPI_VENC_SetHorizonFlip(VeChn, bHorizonFlipFlag);
        }
    }

    //enable Adaptive Intra Inp
    bool oldAdaptiveintrainp, newAdaptiveintrainp;
    oldAdaptiveintrainp = pCurVencParam->getAdaptiveIntraInpFlag();
    newAdaptiveintrainp = pVencParam->getAdaptiveIntraInpFlag();
    if (oldAdaptiveintrainp != newAdaptiveintrainp)
    {
        alogd("VEncParam AdaptiveIntraInp change[%d]->[%d]", oldAdaptiveintrainp, newAdaptiveintrainp);
        pCurVencParam->enableAdaptiveIntraInp(newAdaptiveintrainp);
        {
            BOOL bAdaptiveIntraInpFlag;
            if(newAdaptiveintrainp)
            {
                bAdaptiveIntraInpFlag = TRUE;
            }
            else
            {
                bAdaptiveIntraInpFlag = FALSE;
            }
            AW_MPI_VENC_SetAdaptiveIntraInP(VeChn, bAdaptiveIntraInpFlag);
        }
    }

    //enable 3DNR
    s3DfilterParam old3DnrParam, new3DnrParam;
    old3DnrParam = pCurVencParam->get3DFilter();
    new3DnrParam = pVencParam->get3DFilter();
    if ((old3DnrParam.enable_3d_filter != new3DnrParam.enable_3d_filter) ||
        (old3DnrParam.smooth_filter_enable != new3DnrParam.smooth_filter_enable) ||
        (old3DnrParam.max_pix_diff_th != new3DnrParam.max_pix_diff_th) ||
        (old3DnrParam.max_mad_th != new3DnrParam.max_mad_th) ||
        (old3DnrParam.max_mv_th != new3DnrParam.max_mv_th) ||
        (old3DnrParam.min_coef != new3DnrParam.min_coef) ||
        (old3DnrParam.max_coef != new3DnrParam.max_coef))
    {
        alogd("VEncParam 3DNR change, enable_3d_filter[%d]->[%d]"
              "smooth_filter_enable[%d]->[%d]"
              "max_pix_diff_th[%d]->[%d]"
              "max_mad_th[%d]->[%d]"
              "max_mv_th[%d]->[%d]"
              "min_coef[%d]->[%d]"
              "max_coef[%d]->[%d]",
              old3DnrParam.enable_3d_filter, new3DnrParam.enable_3d_filter,
              old3DnrParam.smooth_filter_enable, new3DnrParam.smooth_filter_enable,
              old3DnrParam.max_pix_diff_th, new3DnrParam.max_pix_diff_th,
              old3DnrParam.max_mad_th, new3DnrParam.max_mad_th,
              old3DnrParam.max_mv_th, new3DnrParam.max_mv_th,
              old3DnrParam.min_coef, new3DnrParam.min_coef,
              old3DnrParam.max_coef, new3DnrParam.max_coef);
        pCurVencParam->set3DFilter(new3DnrParam);
        {
            PAYLOAD_TYPE_E VideoEncoder = pCurVencParam->getVideoEncoder();
            if(PT_H264 == VideoEncoder || PT_H265 == VideoEncoder)
            {
                AW_MPI_VENC_Set3DFilter(VeChn, &new3DnrParam);
            }
            else
            {
                aloge("fatal error! encoder[0x%x] don't support 3DNR!", VideoEncoder);
            }
        }
    }

    //enable Color2Grey
    bool oldColor2Grey, newColor2Grey;
    oldColor2Grey = pCurVencParam->getColor2GreyFlag();
    newColor2Grey = pVencParam->getColor2GreyFlag();
    if (oldColor2Grey != newColor2Grey)
    {
        alogd("VEncParam Color2Grey change[%d]->[%d]", oldColor2Grey, newColor2Grey);
        pCurVencParam->enableColor2Grey(newColor2Grey);
        {
            VENC_COLOR2GREY_S bColor2GreyFlag;
            if(newColor2Grey)
            {
                bColor2GreyFlag.bColor2Grey = TRUE;
            }
            else
            {
                bColor2GreyFlag.bColor2Grey = FALSE;
            }
            AW_MPI_VENC_SetColor2Grey(VeChn, &bColor2GreyFlag);
        }
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::vencprepare(int VencId)
{
    status_t result = UNKNOWN_ERROR;
    std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(VencId);
    if (mVencInfoMap.end() == it)
    {
        alogd("fatal error! mVencInfoMap don't have VencId[%d]", VencId);
        return BAD_VALUE;
    }
    VencParameters *pVencParam = it->second;

    PAYLOAD_TYPE_E VideoEncoder = pVencParam->getVideoEncoder();
    if (VideoEncoder != PT_MAX)
    {
        //define CameraProxy
        CameraRecordingProxy *pCameraProxy = NULL;
        VI_DEV Vipp = MM_INVALID_DEV;

        //create CameraFrameManager
        if (NULL == mpInputFrameManager)
        {
            std::map<int, VI_DEV>::iterator itVipp = mVeVippBindMap.find(VencId);
            if (mVeVippBindMap.end() == itVipp)
            {
                aloge("fatal error! mVeVippBindMap don't have VencId[%d]", VencId);
            }
            Vipp = itVipp->second;

            std::map<VI_DEV, CameraRecordingProxy*>::iterator itCRP = mCameraProxyMap.find(Vipp);
            if (mCameraProxyMap.end() == itCRP)
            {
                aloge("fatal error! mCameraProxyMap don't have vipp[%d]", Vipp);
            }
            pCameraProxy = itCRP->second;
            mpInputFrameManager = new CameraFrameManager(pCameraProxy, Vipp);
        }
        //CameraFrameManger add new CameraProxy and new Vipp
        else
        {
            alogd("inputFrameManager is exist should addCameraProxy.");
            //find Vipp
            std::map<int, VI_DEV>::iterator itVIpp = mVeVippBindMap.find(VencId);
            if (mVeVippBindMap.end() == itVIpp)
            {
                aloge("fatal error! mVeVippBindMap don't have VencId[%d]!", VencId);
            }
            Vipp = itVIpp->second;

            //find CameraProxy
            std::map<VI_DEV, CameraRecordingProxy*>::iterator itCRP = mCameraProxyMap.find(Vipp);
            if (mCameraProxyMap.end() == itCRP)
            {
                aloge("fatal error! mCameraProxyMap don't have Vipp[%d]", Vipp);
            }
            pCameraProxy = itCRP->second;
            mpInputFrameManager->addCameraProxy(pCameraProxy, Vipp);
        }

        //create venc channel.
        bool nSuccessFlag = false;
        ERRORTYPE ret;
        config_VENC_CHN_ATTR_S(VencId);
        
        VENC_CHN VeChn = 0;
        VENC_CHN_ATTR_S stVEncChnAttr = pVencParam->getVencChnAttr();
        VENC_RC_PARAM_S stVEncRcParam = pVencParam->getVencRcParam();
        while(VeChn < VENC_MAX_CHN_NUM)
        {
            ret = AW_MPI_VENC_CreateChn(VeChn, &stVEncChnAttr);
            if(SUCCESS == ret)
            {
                nSuccessFlag = true;
                mVeChnCount++;
                alogd("create venc channel[%d] success!", VeChn);
                break;
            }
            else if(ERR_VENC_EXIST == ret)
            {
                alogv("venc channel[%d] is exist, find next!", VeChn);
                VeChn++;
            }
            else
            {
                alogd("create venc channel[%d] ret[0x%x], find next!", VeChn, ret);
                VeChn++;
            }
        }
        if(false == nSuccessFlag)
        {
            VeChn = MM_INVALID_CHN;
            aloge("fatal error! create venc channel fail!");
            result = UNKNOWN_ERROR;
            goto _err0;
        }
        pVencParam->setVencChnIndex(VeChn);

        AW_MPI_VENC_SetRcParam(VeChn, &stVEncRcParam);
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)this;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(VeChn, &cbInfo);

        CameraParameters cameraParam;
        pCameraProxy->getParameters(Vipp, cameraParam);
        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = cameraParam.getPreviewFrameRate();
        stFrameRate.DstFrmRate = pVencParam->getVideoFrameRate();
        AW_MPI_VENC_SetFrameRate(VeChn, &stFrameRate);

        if(mTimeLapseEnable)
        {
            AW_MPI_VENC_SetTimeLapse(VeChn, mTimeBetweenFrameCapture);
        }
        //IntraRefresh
        VencCyclicIntraRefresh stIntraRefreshParam = pVencParam->getVideoEncodingIntraRefresh();
        AW_MPI_VENC_SetIntraRefresh(VeChn, &stIntraRefreshParam);

        //smartP
        if(PT_H264 == VideoEncoder || PT_H265 == VideoEncoder)
        {
            VencSmartFun stSmartPParam = pVencParam->getVideoEncodingSmartP();
            AW_MPI_VENC_SetSmartP(VeChn, &stSmartPParam);
        }

        //AW_MPI_VENC_SetVEFreq(mVeChn, 534);

        VENC_PARAM_REF_S stVEncRefParam = pVencParam->getRefParam();
        AW_MPI_VENC_SetRefParam(VeChn, &stVEncRefParam);

        bool bHorizonfilp = pVencParam->getHorizonFilpFlag();
        AW_MPI_VENC_SetHorizonFlip(VeChn, bHorizonfilp);

        bool bAdaptiveintrainp = pVencParam->getAdaptiveIntraInpFlag();
        AW_MPI_VENC_SetAdaptiveIntraInP(VeChn, bAdaptiveintrainp);

        s3DfilterParam st3DnrParam = pVencParam->get3DFilter();
        AW_MPI_VENC_Set3DFilter(VeChn, &st3DnrParam);

        VENC_SUPERFRAME_CFG_S stVencSuperFrameCfg;
        stVencSuperFrameCfg = pVencParam->getVencSuperFrameConfig();
        AW_MPI_VENC_SetSuperFrameCfg(VeChn, &stVencSuperFrameCfg);

        VencParameters::VUI vuiInfo = pVencParam->getVui();
        if(PT_H264 == VideoEncoder)
        {
            AW_MPI_VENC_SetH264Vui(VeChn, &vuiInfo.H264Vui);
        }
        else if(PT_H265 == VideoEncoder)
        {
            AW_MPI_VENC_SetH265Vui(VeChn, &vuiInfo.H265Vui);
        }

        VencSaveBSFile stSaveBSFileParam;
        stSaveBSFileParam = pVencParam->getenableSaveBSFile();
        AW_MPI_VENC_SaveBsFile(VeChn, &stSaveBSFileParam);

        VeProcSet stVeProcSet = pVencParam->getProcSet();
        AW_MPI_VENC_SetProcSet(VeChn, &stVeProcSet);

        VENC_COLOR2GREY_S  stColor2Grey;
        stColor2Grey.bColor2Grey = pVencParam->getColor2GreyFlag();
        AW_MPI_VENC_SetColor2Grey(VeChn, &stColor2Grey);

        bool bNullSkipEnable = pVencParam->getNullSkipFlag();
        if(bNullSkipEnable)
        {
            AW_MPI_VENC_EnableNullSkip(VeChn, (BOOL)bNullSkipEnable);
        }

        bool bPSkipEnable = pVencParam->getPSkipFlag();
        if(bPSkipEnable)
        {
            AW_MPI_VENC_EnablePSkip(VeChn, (BOOL)bPSkipEnable);
        }

        VENC_IspVeLinkAttr stIspVeLinkAttr;
        memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
        if (pVencParam->getIspAndVeLinkEnable())
        {
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            if (pVencParam->getMainStreamFlag())
            {
                stIspVeLinkAttr.bEnableVe2Isp = TRUE;
            }
            else
            {
                stIspVeLinkAttr.bEnableVe2Isp = FALSE;
            }
            stIspVeLinkAttr.nVipp = Vipp;
        }
        AW_MPI_VENC_EnableIspVeLink(VeChn, &stIspVeLinkAttr);
        alogd("recorder[%d] VencChn[%d] ispVeLink:%d-%d-%d", mRecorderId, VeChn, stIspVeLinkAttr.bEnableIsp2Ve,
            stIspVeLinkAttr.bEnableVe2Isp, stIspVeLinkAttr.nVipp);

        rec_start_timestamp = -1;

    }
    return NO_ERROR;
_err0:
    return result;
}

status_t EyeseeRecorder::setmuxChnsps(VENC_CHN VeChn, PAYLOAD_TYPE_E VideoEncoder, MUX_CHN muxChn)
{
    status_t result = NO_ERROR;

    if(VideoEncoder == PT_H264)
    {
        int venc_ret = 0;
        VencHeaderData stH264SpsPpsInfo;
        memset(&stH264SpsPpsInfo,0,sizeof(stH264SpsPpsInfo));
        venc_ret = AW_MPI_VENC_GetH264SpsPpsInfo(VeChn, &stH264SpsPpsInfo);
        if(SUCCESS == venc_ret) // failure process to avoid using of null pointer
        {
            alogd("VencChn: %d, H264SpsPpsInfo.nLength: %d, muxChn:%d", VeChn, stH264SpsPpsInfo.nLength, muxChn);
            AW_MPI_MUX_SetH264SpsPpsInfo(muxChn, VeChn, &stH264SpsPpsInfo);
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }
    else if(VideoEncoder == PT_H265)
    {
        int venc_ret = 0;
        VencHeaderData stH265SpsPpsInfo;
        memset(&stH265SpsPpsInfo,0,sizeof(stH265SpsPpsInfo));
        venc_ret = AW_MPI_VENC_GetH265SpsPpsInfo(VeChn, &stH265SpsPpsInfo);
        if(SUCCESS == venc_ret)
        {
            alogd("Venc: %d, H265SpsPpsInfo.nLength: %d, muxChn:%d", VeChn, stH265SpsPpsInfo.nLength, muxChn);
            AW_MPI_MUX_SetH265SpsPpsInfo(muxChn, VeChn, &stH265SpsPpsInfo);
        }
        else
        {
            result = UNKNOWN_ERROR;
        }
    }

    return result;
}

status_t EyeseeRecorder::setmuxsps(VENC_CHN VeChn, PAYLOAD_TYPE_E VideoEncoder)
{
    for(auto&& muxChn : mMuxChns)
    {
        setmuxChnsps(VeChn, VideoEncoder, muxChn);
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::prepare()
{
    status_t result = UNKNOWN_ERROR;
    Mutex::Autolock autoLock(mLock);
    if (!(mCurrentState & MEDIA_RECORDER_DATASOURCE_CONFIGURED))
    {
        aloge("prepare called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }

    bool nSuccessFlag = false;
    MPPCallbackInfo cbInfo;
    ERRORTYPE ret;

    //venc prepare
    for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
    {
        result = vencprepare(it->first);
        if (NO_ERROR != result)
        {
            aloge("fatal error! vencprepare fail! this VencId is [%d]", it->first);
            return result;
        }
    }

    if (!mTimeLapseEnable && mAudioEncoder != PT_MAX)
    {
        mAiDev = 0;
        config_AIO_ATTR_S();
        AW_MPI_AI_SetPubAttr(mAiDev, &mAioAttr);

        //enable audio_hw_ai
        AW_MPI_AI_Enable(mAiDev);

        //create ai channel.
        ERRORTYPE ret;
        BOOL nSuccessFlag = FALSE;
        mAiChn = 0;
        while (mAiChn < AIO_MAX_CHN_NUM)
        {
            ret = AW_MPI_AI_CreateChn(mAiDev, mAiChn, NULL);
            if (SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create ai channel[%d] success!", mAiChn);
                break;
            }
            else if (ERR_AI_EXIST == ret)
            {
                alogv("ai channel[%d] exist, find next!", mAiChn);
                mAiChn++;
            }
            else if (ERR_AI_NOT_ENABLED == ret)
            {
                aloge("audio_hw_ai not started!");
                break;
            }
            else
            {
                aloge("create ai channel[%d] fail! ret[0x%x]!", mAiChn, ret);
                break;
            }
        }
        if(FALSE == nSuccessFlag)
        {
            mAiChn = MM_INVALID_CHN;
            aloge("fatal error! create ai channel fail!");
            result = UNKNOWN_ERROR;
            return result;
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)this;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_AI_RegisterCallback(mAiDev, mAiChn, &cbInfo);
        AW_MPI_AI_SetChnMute(mAiDev, mAiChn, mMuteMode?TRUE:FALSE);

        //create aenc channel.
        nSuccessFlag = FALSE;
        config_AENC_CHN_ATTR_S();
        mAeChn = 0;
        while(mAeChn < AENC_MAX_CHN_NUM)
        {
            ret = AW_MPI_AENC_CreateChn(mAeChn, &mAEncChnAttr);
            if(SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create aenc channel[%d] success!", mAeChn);
                break;
            }
            else if(ERR_AENC_EXIST == ret)
            {
                alogv("aenc channel[%d] exist, find next!", mAeChn);
                mAeChn++;
            }
            else
            {
                alogd("create aenc channel[%d] ret[0x%x], find next!", mAeChn, ret);
                mAeChn++;
            }
        }
        if(FALSE == nSuccessFlag)
        {
            mAeChn = MM_INVALID_CHN;
            aloge("fatal error! create aenc channel fail!");
            result = UNKNOWN_ERROR;
            return result;
        }
        AW_MPI_AENC_RegisterCallback(mAeChn, &cbInfo);
        mAudioStreamId = mVencInfoMap.size();
    }

    if((mVeChnCount > 0) || (mAeChn >= 0))
    {
        if(gps_state)
        {
        #if (MPPCFG_TEXTENC!=0)
            config_TENC_CHN_ATTR_S();
            mTeChn = 0;
            while(mTeChn < TENC_MAX_CHN_NUM)
            {
                ret = AW_MPI_TENC_CreateChn(mTeChn, &mTEncChnAttr);
                if(SUCCESS == ret)
                {
                    nSuccessFlag = TRUE;
                    alogd("create tenc channel[%d] success!", mTeChn);
                    break;
                }
                else if(ERR_TENC_EXIST == ret)
                {
                    alogv("tenc channel[%d] exist, find next!", mTeChn);
                    mTeChn++;
                }
                else
                {
                    alogd("create tenc channel[%d] ret[0x%x], find next!", mTeChn, ret);
                    mTeChn++;
                }
            }
            if(FALSE == nSuccessFlag)
            {
                mTeChn = MM_INVALID_CHN;
                aloge("fatal error! create aenc channel fail!");
                result = UNKNOWN_ERROR;
                return result;
            }

            cbInfo.cookie = (void*)this;
            cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
            AW_MPI_TENC_RegisterCallback(mTeChn, &cbInfo);
            if(mAudioStreamId != -1)
            {
                mTextStreamId = mAudioStreamId + 1;
            }
            else
            {
                mTextStreamId = mVencInfoMap.size();
            }
        #else
            alogw("textenc is disable!");
        #endif
        }
    }

    //set cache duration to muxGroup.
    if(mMuxCacheDuration > 0)
    {
        mpMuxCacheManager = new MuxCacheManager(mMuxCacheDuration, this);
        if(NULL == mpMuxCacheManager)
        {
            aloge("fatal error! create mux cache manager fail");
        }
        if(-1 == mMuxCacheStrmIds.mStrmIdsCnt)
        {
            int nStreamId = 0;
            for (std::pair<const int, VencParameters*>& pair : mVencInfoMap)
            {
                int nVencId = pair.first; //vencId is streamId of vencStream.
                VencParameters *pVencParam = pair.second;
                VENC_CHN_ATTR_S stVEncChnAttr = pVencParam->getVencChnAttr();
                unsigned int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&stVEncChnAttr);
                int nBufSize = (int)((int64_t)nBitRate*mMuxCacheDuration*12/10/8/1000); // bufSize = 1.2 * bitRate * duration.
                if(nStreamId != nVencId)
                {
                    aloge("fatal error! check vencId[%d-%d]", nVencId, nStreamId);
                }
                mpMuxCacheManager->AddStreamBufManager(nVencId, nBufSize);
                nStreamId++;
            }
            if(PT_MAX != mAudioEncoder)
            {
                int nBufSize = (int)((int64_t)mAudioBitRate*mMuxCacheDuration*12/10/8/1000);
                if(nStreamId != mAudioStreamId)
                {
                    aloge("fatal error! check audioStreamId[%d-%d]", mAudioStreamId, nStreamId);
                }
                mpMuxCacheManager->AddStreamBufManager(mAudioStreamId, nBufSize);
                nStreamId++;
            }
            if(gps_state)
            {
            #if (MPPCFG_TEXTENC!=0)
                int nTextStreamId = nStreamId;
                //we assume one second one gps info, one gpsinfo is 10K Bytes.
                int nTextBitRate = 10240 * 8;
                int nBufSize = (int)((int64_t)nTextBitRate*mMuxCacheDuration*12/10/8/1000);
                if(nStreamId != mTextStreamId)
                {
                    aloge("fatal error! check audioStreamId[%d-%d]", mTextStreamId, nStreamId);
                }
                mpMuxCacheManager->AddStreamBufManager(mTextStreamId, nBufSize);
                nStreamId++;
            #else
                alogw("textenc is disable!");
            #endif
            }
        }
        else
        {
            for(int i=0; i<mMuxCacheStrmIds.mStrmIdsCnt; i++)
            {
                bool bFindStream = false;
                for (std::pair<const int, VencParameters*>& pair : mVencInfoMap)
                {
                    int nVencId = pair.first; //vencId is streamId of vencStream.
                    if(nVencId == mMuxCacheStrmIds.mStrmIds[i])
                    {
                        bFindStream = true;
                        VencParameters *pVencParam = pair.second;
                        VENC_CHN_ATTR_S stVEncChnAttr = pVencParam->getVencChnAttr();
                        unsigned int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&stVEncChnAttr);
                        int nBufSize = (int)((int64_t)nBitRate*mMuxCacheDuration*12/10/8/1000);
                        mpMuxCacheManager->AddStreamBufManager(nVencId, nBufSize);
                        break;
                    }
                }
                if(false == bFindStream)
                {
                    if(mMuxCacheStrmIds.mStrmIds[i] == mAudioStreamId)
                    {
                        bFindStream = true;
                        int nBufSize = (int)((int64_t)mAudioBitRate*mMuxCacheDuration*12/10/8/1000);
                        mpMuxCacheManager->AddStreamBufManager(mAudioStreamId, nBufSize);
                    }
                }
                if(false == bFindStream)
                {
                    if(mMuxCacheStrmIds.mStrmIds[i] == mTextStreamId)
                    {
                        bFindStream = true;
                        int nTextBitRate = 10240 * 8;
                        int nBufSize = (int)((int64_t)nTextBitRate*mMuxCacheDuration*12/10/8/1000);
                        mpMuxCacheManager->AddStreamBufManager(mTextStreamId, nBufSize);
                    }
                }
                if(false == bFindStream)
                {
                    aloge("fatal error! RecorderId[%d] streamId[%d-%d] is not find!", mRecorderId, i, mMuxCacheStrmIds.mStrmIds[i]);
                }
            }
        }
    }

    //create mux channels.
    alogd("there are [%d]mux channels need to create", mSinkInfos.size());
    unsigned int i;
    for(i=0; i<mSinkInfos.size(); i++)
    {
        OutputSinkInfo *pSinkInfo = &mSinkInfos[i];
        MUX_CHN_ATTR_S muxChnAttr;
        config_MUX_CHN_ATTR_S(&muxChnAttr, pSinkInfo);
        mMuxChnAttrs.push_back(muxChnAttr);

        ERRORTYPE ret;
        BOOL nSuccessFlag = FALSE;
        MUX_CHN nMuxChn = 0;
        while(nMuxChn < MUX_MAX_CHN_NUM)
        {
            ret = AW_MPI_MUX_CreateChn(nMuxChn, &muxChnAttr, pSinkInfo->mOutputFd, pSinkInfo->mFallocateLen);
            if(SUCCESS == ret)
            {
                nSuccessFlag = TRUE;
                alogd("create muxChn[%d] success, muxerId[%d]!", nMuxChn, pSinkInfo->mMuxerId);
                break;
            }
            else if(ERR_MUX_EXIST == ret)
            {
                alogv("muxChn[%d] is exist, find next!", nMuxChn);
                nMuxChn++;
            }
            else
            {
                aloge("fatal error! create muxChn[%d] fail ret[0x%x], find next!", nMuxChn, ret);
                nMuxChn++;
            }
        }
        if(nSuccessFlag)
        {
            mMuxChns.push_back(nMuxChn);
            //mPolicyMap.insert(std::make_pair(muxChnAttr.mMuxerId, RecordFileDurationPolicy_AverageDuration));
        }
        else
        {
            mMuxChns.push_back(MM_INVALID_CHN);
            aloge("fatal error! create mux channel fail!");
        }
        RecordFileDurationPolicy ePolicy = mPolicyMap.find(pSinkInfo->mMuxerId)->second;
        ret = AW_MPI_MUX_SetSwitchFileDurationPolicy(nMuxChn, ePolicy);
        if(ret != SUCCESS)
        {
            aloge("fatal error! set file duration policy[%d] to mux[%d-%d] fail!", ePolicy, nMuxChn, pSinkInfo->mMuxerId);
        }
        //set VeChn-StreamId binding.
        for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
        {
            int nStreamId = it->first;
            VencParameters *pVencParam = it->second;
            VENC_CHN VeChn = pVencParam->getVencChnIndex();
            AW_MPI_MUX_SetVeChnBindStreamId(nMuxChn, VeChn, nStreamId);
        }
        std::map<int, MuxStreamIdsInfo>::iterator IterMuxStrmIds = mMuxStrmIdsMap.find(pSinkInfo->mMuxerId);
        if(IterMuxStrmIds != mMuxStrmIdsMap.end())
        {
            MuxStreamIdsInfo *pMuxStreamIdsInfo = &IterMuxStrmIds->second;
            alogv("recorder[%d] muxChn[%d-%d] streamIds cnt:%x", mRecorderId, nMuxChn, pSinkInfo->mMuxerId, pMuxStreamIdsInfo->mStrmIdsCnt);
            AW_MPI_MUX_SetStrmIds(nMuxChn, pMuxStreamIdsInfo);
        }
        //set callback MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)this;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(nMuxChn, &cbInfo);
    }
    //set mux group attr & sps
    for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
    {
        VencParameters *pVencParam = it->second;
        VENC_CHN VeChn = pVencParam->getVencChnIndex();
        PAYLOAD_TYPE_E VideoEncoder = pVencParam->getVideoEncoder();

        if ((VeChn >= 0) && (VideoEncoder != PT_MAX))
        {
            setmuxsps(VeChn, VideoEncoder);
        }
    }
    
    /**
      use non-tunnel mode of mpi_venc, mpi_aenc, mpi_tenc and mpi_mux, because eyeseerecorder can create many muxChns to
      receive different venc stream, aenc stream, tenc stream.
    */

    if (mAeChn >= 0)
    {
        MPP_CHN_S AiChn{MOD_ID_AI, mAiDev, mAiChn};
        MPP_CHN_S AeChn{MOD_ID_AENC, 0, mAeChn};
        AW_MPI_SYS_Bind(&AiChn, &AeChn);
    }

    /**
      audio pts and video pts all use system time, so no need to calculate audio sample number to get time.
      so don't need to do RecAVSync.
    */
    mCurrentState = MEDIA_RECORDER_PREPARED;
    return NO_ERROR;
}

/**
  send stream to one muxChn.
  Don't consider mutex, it will be done out.

  @return
    NO_ERROR
*/
status_t EyeseeRecorder::SendStreamToMuxChn(MUX_CHN muxChn, MuxStreamNode *pStreamNode)
{
    ERRORTYPE ret;
    switch(pStreamNode->mStreamType)
    {
        case MuxStreamNode::Type::Video:
        {
            pStreamNode->mRefCnt++;
            ret = AW_MPI_MUX_SendVideoStream(muxChn, &pStreamNode->mVencStream, pStreamNode->mStreamId);
            if(ret != SUCCESS)
            {
                aloge("fatal error! RecorderId[%d] send video stream to muxChn[%d] fail[0x%x]", mRecorderId, muxChn, ret);
                pStreamNode->mRefCnt--;
            }
            break;
        }
        case MuxStreamNode::Type::Audio:
        {
            pStreamNode->mRefCnt++;
            ret = AW_MPI_MUX_SendAudioStream(muxChn, &pStreamNode->mAencStream, pStreamNode->mStreamId);
            if(ret != SUCCESS)
            {
                aloge("fatal error! RecorderId[%d] send audio stream to muxChn[%d] fail[0x%x]", mRecorderId, muxChn, ret);
                pStreamNode->mRefCnt--;
            }
            break;
        }
        case MuxStreamNode::Type::Text:
        {
            pStreamNode->mRefCnt++;
            ret = AW_MPI_MUX_SendTextStream(muxChn, &pStreamNode->mTencStream, pStreamNode->mStreamId);
            if(ret != SUCCESS)
            {
                aloge("fatal error! RecorderId[%d] send text stream to muxChn[%d] fail[0x%x]", mRecorderId, muxChn, ret);
                pStreamNode->mRefCnt--;
            }
            break;
        }
        default:
        {
            aloge("fatal error! unknown stream type:%d", pStreamNode->mStreamType);
            break;
        }
    }
    return NO_ERROR;
}

/**
  send X Stream to all muxChns.
  first need decide if this streamId can be permited to send to muxChn.

  @param nStreamId
    we use nVencId as nStreamId, from 0. then audio stream index, text stream index.
  @return
    NO_ERROR
*/
status_t EyeseeRecorder::SendStreamToMux(MuxStreamNode *pStreamNode)
{
    std::lock_guard<std::mutex> autoMuxChnLock(mMuxChnLock); //for mMuxChns and mMuxChnAttrs
    ERRORTYPE ret;
    for(int i=0; i<(int)mMuxChns.size(); i++)
    {
        MUX_CHN muxChn = mMuxChns[i];
        bool bStreamPermit = true; //permit this stream to send to this muxChn.
        for(MUX_CHN& forbidChn : mForbidMuxChns)
        {
            if(muxChn == forbidChn)
            {
                bStreamPermit = false;
                break;
            }
        }
        if(bStreamPermit)
        {
            int nMuxerId = mSinkInfos[i].mMuxerId;
            std::map<int, MuxStreamIdsInfo>::iterator IterMuxStrmIds = mMuxStrmIdsMap.find(nMuxerId);
            if(IterMuxStrmIds != mMuxStrmIdsMap.end())
            {
                MuxStreamIdsInfo *pMuxStreamIdsInfo = &IterMuxStrmIds->second;
                alogv("recorder[%d] muxChn[%d-%d] streamIds cnt:%x", mRecorderId, muxChn, nMuxerId, pMuxStreamIdsInfo->mStrmIdsCnt);
                if(pMuxStreamIdsInfo->mStrmIdsCnt >= 0)
                {
                    bStreamPermit = false;
                    for(int j=0; j<pMuxStreamIdsInfo->mStrmIdsCnt; j++)
                    {
                        if(pMuxStreamIdsInfo->mStrmIds[j] == pStreamNode->mStreamId)
                        {
                            bStreamPermit = true;
                            break;
                        }
                    }
                }
            }
        }
        if(!bStreamPermit)
        {
            continue;
        }
        SendStreamToMuxChn(muxChn, pStreamNode);
    }
    return NO_ERROR;
}

/**
  send all streams of cacheManger to muxChn.
  When switch impact file or add impact OutputSink, need call this function.

  @return
    NO_ERROR
*/
status_t EyeseeRecorder::SendAllCacheManagerStreamToMuxChn(MUX_CHN muxChn)
{
    std::lock_guard<std::mutex> autoLock2(mpMuxCacheManager->mLock);
    int64_t nRefVideoStreamFrontPts = -1; //unit:us
    for(MuxCacheManager::StreamBufManager& elemStreamBufManager : mpMuxCacheManager->mStreamBufManagerList)
    {
        bool bFirstKeyFrameDone = false;
        if(elemStreamBufManager.mStreamNodeUsingList.size() > 0)
        {
            aloge("fatal error! recorderId[%d] why cacheManger streamId[%d] has using node when switch impact file?", mRecorderId, elemStreamBufManager.mStreamId);
            for(MuxStreamNode& elemNode : elemStreamBufManager.mStreamNodeUsingList)
            {
                if(elemNode.mRefCnt <= 0)
                {
                    aloge("fatal error! recorderId[%d] cacheManager streamId[%d] node refcnt:%d wrong!", mRecorderId, elemNode.mStreamId, elemNode.mRefCnt);
                }
                if(MuxStreamNode::Type::Video == elemNode.mStreamType)
                {
                    if(false == bFirstKeyFrameDone)
                    {
                        std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(elemNode.mStreamId);
                        if (mVencInfoMap.end() == it)
                        {
                            aloge("fatal error! recorderId[%d] VencInfoMap don't have VencId[%d]", mRecorderId, elemNode.mStreamId);
                        }
                        VencParameters *pVencParam = it->second;
                        PAYLOAD_TYPE_E eVencType = pVencParam->getVideoEncoder();
                        if(elemNode.CheckKeyFrameFlag(eVencType))
                        {
                            bFirstKeyFrameDone = true;
                            if(-1 == nRefVideoStreamFrontPts)
                            {
                                nRefVideoStreamFrontPts = elemNode.GetStreamPts();
                            }
                            SendStreamToMuxChn(muxChn, &elemNode);
                        }
                    }
                    else
                    {
                        SendStreamToMuxChn(muxChn, &elemNode);
                    }
                }
                else
                {
                    if((-1 == nRefVideoStreamFrontPts) || (elemNode.GetStreamPts() >= nRefVideoStreamFrontPts))
                    {
                        SendStreamToMuxChn(muxChn, &elemNode);
                    }
                }
            }
        }
        bool bSentFlag = false;
        while(1)
        {
            MuxStreamNode *pNode = elemStreamBufManager.GetStreamNode();
            if(NULL == pNode)
            {
                break;
            }
            if(MuxStreamNode::Type::Video == pNode->mStreamType)
            {
                if(false == bFirstKeyFrameDone)
                {
                    std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(pNode->mStreamId);
                    if (mVencInfoMap.end() == it)
                    {
                        aloge("fatal error! RecorderId[%d] VencInfoMap don't have VencId[%d]", mRecorderId, pNode->mStreamId);
                    }
                    VencParameters *pVencParam = it->second;
                    PAYLOAD_TYPE_E eVencType = pVencParam->getVideoEncoder();
                    if(pNode->CheckKeyFrameFlag(eVencType))
                    {
                        bFirstKeyFrameDone = true;
                        if(-1 == nRefVideoStreamFrontPts)
                        {
                            nRefVideoStreamFrontPts = pNode->GetStreamPts();
                        }
                        SendStreamToMuxChn(muxChn, pNode);
                    }
                }
                else
                {
                    SendStreamToMuxChn(muxChn, pNode);
                }
            }
            else
            {
                if(bSentFlag)
                {
                    if(nRefVideoStreamFrontPts != -1)
                    {
                        if(pNode->GetStreamPts() < nRefVideoStreamFrontPts)
                        {
                            alogd("Be careful! streamNode[%d-%d], pts[%lld<%lld], but still send!", pNode->mStreamId, pNode->mStreamType,
                                pNode->GetStreamPts(), nRefVideoStreamFrontPts);
                        }
                    }
                    SendStreamToMuxChn(muxChn, pNode);
                }
                else
                {
                    if((-1 == nRefVideoStreamFrontPts) || (pNode->GetStreamPts() >= nRefVideoStreamFrontPts))
                    {
                        SendStreamToMuxChn(muxChn, pNode);
                        bSentFlag = true;
                    }
                }
            }
            elemStreamBufManager.ReleaseStreamNode(pNode->GetStreamNodeId());
        }
    }
    return NO_ERROR;
}

/**
  release video stream to vencChn, allowing out-of-order returning stream.

  @return
    NO_ERROR: find streamNode and release.
    UNKNOWN_ERROR: not find streamNode.
*/
status_t EyeseeRecorder::ReleaseVideoStreamToVencChn(VENC_STREAM_S *pVencStream, int nStreamId, VENC_CHN VeChn)
{
    ERRORTYPE ret;
    bool bFindFlag = false;
    std::lock_guard<std::mutex> autoLock(mStreamNodeLock);
    auto iter = mStreamNodeListMap.find(nStreamId);
    if(iter == mStreamNodeListMap.end())
    {
        aloge("fatal error! recorder[%d] has not streamNodeList for streamId[%d]", mRecorderId, nStreamId);
        return UNKNOWN_ERROR;
    }
    std::list<MuxStreamNode>& streamNodeList = iter->second;
    //check if it is first node in list, if it is not, keep it in list. If it is, try to release it and following nodes
    //whose refCnt is 0.
    MuxStreamNode *pFrontNode = &streamNodeList.front();
    if((pFrontNode->mStreamId != nStreamId) || (pFrontNode->mStreamType != MuxStreamNode::Type::Video))
    {
        aloge("fatal error! recorder[%d] streamId or streamType wrong! [%d-%d,%d]check code!",
            mRecorderId, pFrontNode->mStreamId, pFrontNode->mStreamType, nStreamId);
    }
    if(pFrontNode->mVencStream.mSeq == pVencStream->mSeq)
    {
        pFrontNode->mRefCnt--;
        if(pFrontNode->mRefCnt <= 0)
        {
            if(pFrontNode->mRefCnt < 0)
            {
                aloge("fatal error! check code! recorder[%d] elem.mRefCnt[%d] < 0, stream node info:%d-%d-%d",
                    mRecorderId, pFrontNode->mRefCnt, pFrontNode->mStreamId, pFrontNode->mStreamType, pFrontNode->mVencStream.mSeq);
            }
            //check preceding nodes, continue release the node whose refCnt is 0!
            int cnt = 0;
            for(std::list<MuxStreamNode>::iterator it = streamNodeList.begin(); it != streamNodeList.end();)
            {
                if((it->mStreamId != nStreamId) || (it->mStreamType != MuxStreamNode::Type::Video))
                {
                    aloge("fatal error! recorder[%d] streamId or streamType wrong! [%d-%d,%d]check code!",
                        mRecorderId, it->mStreamId, it->mStreamType, nStreamId);
                }
                if(it->mRefCnt <= 0)
                {
                    if(it->mRefCnt < 0)
                    {
                        aloge("fatal error! check code! recorder[%d] elem.mRefCnt[%d]<0, stream node info:%d-%d-%d",
                            mRecorderId, it->mRefCnt, it->mStreamId, it->mStreamType, it->mVencStream.mSeq);
                    }
                    //release stream to vencChn
                    ret = AW_MPI_VENC_ReleaseStream(VeChn, &it->mVencStream);
                    if(ret != SUCCESS)
                    {
                        aloge("fatal error! RecorderId[%d] streamId[%d] veChn[%d] why venc release stream fail[0x%x]?", mRecorderId, nStreamId, VeChn, ret);
                    }
                    //delete from StreamNodeList
                    it = streamNodeList.erase(it);
                    cnt++;
                }
                else
                {
                    break;
                }
            }
            if(cnt > 1)
            {
                alogd("recorder[%d] streamId[%d] release continuous [%d]nodes, out-of-order returning frame happen", mRecorderId, nStreamId, cnt);
            }
        }
        return NO_ERROR;
    }
    else
    {
        for(std::list<MuxStreamNode>::iterator it = streamNodeList.begin(); it != streamNodeList.end(); ++it)
        {
            if((it->mStreamId != nStreamId) || (it->mStreamType != MuxStreamNode::Type::Video))
            {
                aloge("fatal error! recorder[%d] streamId or streamType wrong! [%d-%d,%d]check code!", mRecorderId, it->mStreamId, it->mStreamType, nStreamId);
            }
            if(it->mVencStream.mSeq == pVencStream->mSeq)
            {
                it->mRefCnt--;
                if(0 == it->mRefCnt)
                {
                    alogv("recorder[%d] release stream out-of-order. stream node info:%d-%d-%d-%d",
                        mRecorderId, it->mStreamId, it->mStreamType, it->mVencStream.mSeq, it->mRefCnt);
                }
                if(it->mRefCnt < 0)
                {
                    aloge("fatal error! check code! recorder[%d] elem.mRefCnt[%d]<0, stream node info:%d-%d-%d",
                        mRecorderId, it->mRefCnt, it->mStreamId, it->mStreamType, it->mVencStream.mSeq);
                }
                bFindFlag = true;
                break;
            }
        }
        if(bFindFlag)
        {
            return NO_ERROR;
        }
        else
        {
            aloge("fatal error! RecorderId[%d] not find stream node info:%d-%d, check code!", mRecorderId, nStreamId, pVencStream->mSeq);
            return UNKNOWN_ERROR;
        }
    }
}

status_t EyeseeRecorder::ReleaseAudioStreamToAencChn(AUDIO_STREAM_S *pAencStream, int nStreamId, AENC_CHN AeChn)
{
    ERRORTYPE ret;
    bool bFindFlag = false;
    std::lock_guard<std::mutex> autoLock(mStreamNodeLock);
    auto iter = mStreamNodeListMap.find(nStreamId);
    if(iter == mStreamNodeListMap.end())
    {
        aloge("fatal error! recorder[%d] has not streamNodeList for audio streamId[%d]", mRecorderId, nStreamId);
        return UNKNOWN_ERROR;
    }
    std::list<MuxStreamNode>& streamNodeList = iter->second;
    //check if it is first node in list, if it is not, keep it in list. If it is, try to release it and following nodes
    //whose refCnt is 0.
    MuxStreamNode *pFrontNode = &streamNodeList.front();
    if((pFrontNode->mStreamId != nStreamId) || (pFrontNode->mStreamType != MuxStreamNode::Type::Audio))
    {
        aloge("fatal error! recorder[%d] streamId or streamType wrong! [%d-%d,%d]check code!",
            mRecorderId, pFrontNode->mStreamId, pFrontNode->mStreamType, nStreamId);
    }
    if(pFrontNode->mAencStream.mId == pAencStream->mId)
    {
        pFrontNode->mRefCnt--;
        if(pFrontNode->mRefCnt <= 0)
        {
            if(pFrontNode->mRefCnt < 0)
            {
                aloge("fatal error! check code! recorder[%d] elem.mRefCnt[%d] < 0, stream node info:%d-%d-%d",
                    mRecorderId, pFrontNode->mRefCnt, pFrontNode->mStreamId, pFrontNode->mStreamType, pFrontNode->mAencStream.mId);
            }
            //check preceding nodes, continue release the node whose refCnt is 0!
            int cnt = 0;
            for(std::list<MuxStreamNode>::iterator it = streamNodeList.begin(); it != streamNodeList.end();)
            {
                if((it->mStreamId != nStreamId) || (it->mStreamType != MuxStreamNode::Type::Audio))
                {
                    aloge("fatal error! recorder[%d] streamId or streamType wrong! [%d-%d,%d]check code!",
                        mRecorderId, it->mStreamId, it->mStreamType, nStreamId);
                }
                if(it->mRefCnt <= 0)
                {
                    if(it->mRefCnt < 0)
                    {
                        aloge("fatal error! check code! recorder[%d] elem.mRefCnt[%d]<0, stream node info:%d-%d-%d",
                            mRecorderId, it->mRefCnt, it->mStreamId, it->mStreamType, it->mAencStream.mId);
                    }
                    //release stream to aencChn
                    ret = AW_MPI_AENC_ReleaseStream(AeChn, &it->mAencStream);
                    if(ret != SUCCESS)
                    {
                        aloge("fatal error! RecorderId[%d] streamId[%d] AeChn[%d] why aenc release stream fail[0x%x]?", mRecorderId, nStreamId, AeChn, ret);
                    }
                    //delete from StreamNodeList
                    it = streamNodeList.erase(it);
                    cnt++;
                }
                else
                {
                    break;
                }
            }
            if(cnt > 1)
            {
                alogd("recorder[%d] audio streamId[%d] release continuous [%d]nodes, out-of-order returning frame happen", mRecorderId, nStreamId, cnt);
            }
        }
        return NO_ERROR;
    }
    else
    {
        for(std::list<MuxStreamNode>::iterator it = streamNodeList.begin(); it != streamNodeList.end(); ++it)
        {
            if((it->mStreamId != nStreamId) || (it->mStreamType != MuxStreamNode::Type::Audio))
            {
                aloge("fatal error! recorder[%d] streamId or streamType wrong! [%d-%d,%d]check code!", mRecorderId, it->mStreamId, it->mStreamType, nStreamId);
            }
            if(it->mAencStream.mId == pAencStream->mId)
            {
                it->mRefCnt--;
                if(0 == it->mRefCnt)
                {
                    alogv("recorder[%d] release audio stream out-of-order. stream node info:%d-%d-%d-%d",
                        mRecorderId, it->mStreamId, it->mStreamType, it->mAencStream.mId, it->mRefCnt);
                }
                if(it->mRefCnt < 0)
                {
                    aloge("fatal error! check code! recorder[%d] elem.mRefCnt[%d]<0, stream node info:%d-%d-%d",
                        mRecorderId, it->mRefCnt, it->mStreamId, it->mStreamType, it->mAencStream.mId);
                }
                bFindFlag = true;
                break;
            }
        }
        if(bFindFlag)
        {
            return NO_ERROR;
        }
        else
        {
            aloge("fatal error! RecorderId[%d] not find stream node info:%d-%d, check code!", mRecorderId, nStreamId, pAencStream->mId);
            return UNKNOWN_ERROR;
        }
    }
}

status_t EyeseeRecorder::ReleaseTextStreamToTencChn(TEXT_STREAM_S *pTencStream, int nStreamId, TENC_CHN TeChn)
{
    ERRORTYPE ret;
    bool bFindFlag = false;
    std::lock_guard<std::mutex> autoLock(mStreamNodeLock);
    auto iter = mStreamNodeListMap.find(nStreamId);
    if(iter == mStreamNodeListMap.end())
    {
        aloge("fatal error! recorder[%d] has not streamNodeList for text streamId[%d]", mRecorderId, nStreamId);
        return UNKNOWN_ERROR;
    }
    std::list<MuxStreamNode>& streamNodeList = iter->second;
    //check if it is first node in list, if it is not, keep it in list. If it is, try to release it and following nodes
    //whose refCnt is 0.
    MuxStreamNode *pFrontNode = &streamNodeList.front();
    if((pFrontNode->mStreamId != nStreamId) || (pFrontNode->mStreamType != MuxStreamNode::Type::Text))
    {
        aloge("fatal error! recorder[%d] streamId or streamType wrong! [%d-%d,%d]check code!",
            mRecorderId, pFrontNode->mStreamId, pFrontNode->mStreamType, nStreamId);
    }
    if(pFrontNode->mTencStream.mId == pTencStream->mId)
    {
        pFrontNode->mRefCnt--;
        if(pFrontNode->mRefCnt <= 0)
        {
            if(pFrontNode->mRefCnt < 0)
            {
                aloge("fatal error! check code! recorder[%d] elem.mRefCnt[%d] < 0, stream node info:%d-%d-%d",
                    mRecorderId, pFrontNode->mRefCnt, pFrontNode->mStreamId, pFrontNode->mStreamType, pFrontNode->mTencStream.mId);
            }
            //check preceding nodes, continue release the node whose refCnt is 0!
            int cnt = 0;
            for(std::list<MuxStreamNode>::iterator it = streamNodeList.begin(); it != streamNodeList.end();)
            {
                if((it->mStreamId != nStreamId) || (it->mStreamType != MuxStreamNode::Type::Text))
                {
                    aloge("fatal error! recorder[%d] streamId or streamType wrong! [%d-%d,%d]check code!",
                        mRecorderId, it->mStreamId, it->mStreamType, nStreamId);
                }
                if(it->mRefCnt <= 0)
                {
                    if(it->mRefCnt < 0)
                    {
                        aloge("fatal error! check code! recorder[%d] elem.mRefCnt[%d]<0, stream node info:%d-%d-%d",
                            mRecorderId, it->mRefCnt, it->mStreamId, it->mStreamType, it->mTencStream.mId);
                    }
                    //release stream to tencChn
                    ret = AW_MPI_TENC_ReleaseStream(TeChn, &it->mTencStream);
                    if(ret != SUCCESS)
                    {
                        aloge("fatal error! RecorderId[%d] streamId[%d] TeChn[%d] why tenc release stream fail[0x%x]?", mRecorderId, nStreamId, TeChn, ret);
                    }
                    //delete from StreamNodeList
                    it = streamNodeList.erase(it);
                    cnt++;
                }
                else
                {
                    break;
                }
            }
            if(cnt > 1)
            {
                alogd("recorder[%d] text streamId[%d] release continuous [%d]nodes, out-of-order returning frame happen", mRecorderId, nStreamId, cnt);
            }
        }
        return NO_ERROR;
    }
    else
    {
        for(std::list<MuxStreamNode>::iterator it = streamNodeList.begin(); it != streamNodeList.end(); ++it)
        {
            if((it->mStreamId != nStreamId) || (it->mStreamType != MuxStreamNode::Type::Text))
            {
                aloge("fatal error! recorder[%d] streamId or streamType wrong! [%d-%d,%d]check code!", mRecorderId, it->mStreamId, it->mStreamType, nStreamId);
            }
            if(it->mTencStream.mId == pTencStream->mId)
            {
                it->mRefCnt--;
                if(0 == it->mRefCnt)
                {
                    alogv("recorder[%d] release text stream out-of-order. stream node info:%d-%d-%d-%d",
                        mRecorderId, it->mStreamId, it->mStreamType, it->mTencStream.mId, it->mRefCnt);
                }
                if(it->mRefCnt < 0)
                {
                    aloge("fatal error! check code! recorder[%d] elem.mRefCnt[%d]<0, stream node info:%d-%d-%d",
                        mRecorderId, it->mRefCnt, it->mStreamId, it->mStreamType, it->mTencStream.mId);
                }
                bFindFlag = true;
                break;
            }
        }
        if(bFindFlag)
        {
            return NO_ERROR;
        }
        else
        {
            aloge("fatal error! RecorderId[%d] not find stream node info:%d-%d, check code!", mRecorderId, nStreamId, pTencStream->mId);
            return UNKNOWN_ERROR;
        }
    }
}

/**
  try to get stream from all encode channels(venc, aenc and tenc) and send them to multi muxChns and cacheManager.
  It is function of EyeseeRecorder::mpSendStreamThread.
*/
void EyeseeRecorder::SendStreamThreadFunc(std::promise<int> SendStreamPromise)
{
    //1. get all chn fds, then prepare fd_set
    handle_set MppStreamFds;
    int nMaxFd = -1;
    int nAeChnFd = -1;
    int nTeChnFd = -1;
    AW_MPI_SYS_HANDLE_ZERO(&MppStreamFds);
    for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
    {
        VencParameters *pVencParam = it->second;
        VENC_CHN VeChn = pVencParam->getVencChnIndex();
        if(VeChn >= 0)
        {
            int nVeChnFd = AW_MPI_VENC_GetHandle(VeChn);
            if(nVeChnFd >= 0)
            {
                AW_MPI_SYS_HANDLE_SET(nVeChnFd, &MppStreamFds);
                if(nVeChnFd > nMaxFd)
                {
                    nMaxFd = nVeChnFd;
                }
            }
            else
            {
                aloge("fatal error! why VeChn[%d-%d] has wrong ChnFd[%d]?", mRecorderId, VeChn, nVeChnFd);
            }
        }
        else
        {
            aloge("fatal error! RecorderId[%d] vencId[%d] has wrong veChn[%d]", mRecorderId, it->first, VeChn);
        }
    }
    if(mAeChn >= 0)
    {
        nAeChnFd = AW_MPI_AENC_GetHandle(mAeChn);
        if(nAeChnFd >= 0)
        {
            AW_MPI_SYS_HANDLE_SET(nAeChnFd, &MppStreamFds);
            if(nAeChnFd > nMaxFd)
            {
                nMaxFd = nAeChnFd;
            }
        }
        else
        {
            aloge("fatal error! why AeChn[%d-%d] has wrong ChnFd[%d]?", mRecorderId, mAeChn, nAeChnFd);
        }
    }
#if (MPPCFG_TEXTENC!=0)
    if(mTeChn >= 0)
    {
        nTeChnFd = AW_MPI_TENC_GetHandle(mTeChn);
        if(nTeChnFd >= 0)
        {
            AW_MPI_SYS_HANDLE_SET(nTeChnFd, &MppStreamFds);
            if(nTeChnFd > nMaxFd)
            {
                nMaxFd = nTeChnFd;
            }
        }
        else
        {
            aloge("fatal error! why TeChn[%d-%d] has wrong ChnFd[%d]?", mRecorderId, mTeChn, nTeChnFd);
        }
    }
#endif
    //2. get stream from all chns, send stream to muxChn.
    mSendStreamThreadState = SendStreamThreadState::Idle;
    SmartMessage stMsg;

    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    memset(&stVencStream, 0, sizeof(stVencStream));
    memset(&stVencPack, 0, sizeof(stVencPack));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;

    AUDIO_STREAM_S stAencStream;
    memset(&stAencStream, 0, sizeof(stAencStream));

    TEXT_STREAM_S stTencStream;
    memset(&stAencStream, 0, sizeof(stAencStream));

    ERRORTYPE ret;
    while (1)
    {
    PROCESS_MESSAGE:
        //2.1 process message
        if(mpSendStreamMsgMgr->GetMessage(stMsg) == 0)
        {
            if(SendStreamMsgType::SetState == (SendStreamMsgType)stMsg.what)
            {
                int ret = 0;
                SendStreamThreadState eDstState = (SendStreamThreadState)stMsg.args[0];
                if(mSendStreamThreadState == eDstState)
                {
                    alogw("Be careful! turn to same state:%d", mSendStreamThreadState);
                    ret = 0;
                }
                else
                {
                    switch(eDstState)
                    {
                        case SendStreamThreadState::Idle:
                        {
                            if((SendStreamThreadState::Executing == mSendStreamThreadState) || (SendStreamThreadState::Pause == mSendStreamThreadState))
                            {
                                mSendStreamThreadState = SendStreamThreadState::Idle;
                                ret = 0;
                            }
                            else
                            {
                                aloge("fatal error! send_stream_state wrong:%d", mSendStreamThreadState);
                                ret = -1;
                            }
                            break;
                        }
                        case SendStreamThreadState::Executing:
                        {
                            if((SendStreamThreadState::Idle == mSendStreamThreadState) || (SendStreamThreadState::Pause == mSendStreamThreadState))
                            {
                                mSendStreamThreadState = SendStreamThreadState::Executing;
                                ret = 0;
                            }
                            else
                            {
                                aloge("fatal error! send_stream_state wrong:%d", mSendStreamThreadState);
                                ret = -1;
                            }
                            break;
                        }
                        case SendStreamThreadState::Pause:
                        {
                            if(SendStreamThreadState::Executing == mSendStreamThreadState)
                            {
                                mSendStreamThreadState = SendStreamThreadState::Pause;
                                ret = 0;
                            }
                            else
                            {
                                aloge("fatal error! send_stream_state wrong:%d", mSendStreamThreadState);
                                ret = -1;
                            }
                            break;
                        }
                        default:
                        {
                            aloge("fatal error! wrong dstState:%d", eDstState);
                            ret = -1;
                            break;
                        }
                    }
                }
                //after processing done, use msgReply to notify sender.
                if(stMsg.spMsgReply != NULL)
                {
                    stMsg.spMsgReply->mnReplyResult = ret;
                    stMsg.spMsgReply->NotifyOne();
                }
            }
            else if(SendStreamMsgType::Stop == (SendStreamMsgType)stMsg.what)
            {
                alogd("Recorder[%d] send_stream_thread receive stop message. exit now", mRecorderId);
                goto _exit0;
            }
            else
            {
                aloge("fatal error! unknown message:%d", stMsg.what);
            }
            stMsg.Reset();
            goto PROCESS_MESSAGE;
        }

        //2.2 get stream, send stream
        if(SendStreamThreadState::Executing == mSendStreamThreadState)
        {
            handle_set tmpRdFds = MppStreamFds;
            int nReadyCnt = AW_MPI_SYS_HANDLE_Select(nMaxFd+1, &tmpRdFds, 200);
            if(nReadyCnt > 0)
            {
                int tmpCnt = 0; //record get stream number this time.
                //check vencChn to try to get stream.
                for (std::pair<const int, VencParameters*>& pair : mVencInfoMap)
                {
                    int nVencId = pair.first;
                    VencParameters *pVencParam = pair.second;
                    VENC_CHN VeChn = pVencParam->getVencChnIndex();
                    if(VeChn < 0)
                    {
                        aloge("fatal error! RecorderId[%d] vencId[%d] has wrong veChn[%d]", mRecorderId, nVencId, VeChn);
                    }
                    int nVeChnFd = AW_MPI_VENC_GetHandle(VeChn);
                    if(AW_MPI_SYS_HANDLE_ISSET(nVeChnFd, &tmpRdFds))
                    {
                        tmpCnt++;
                        ret = AW_MPI_VENC_GetStream(VeChn, &stVencStream, 0);
                        if (ret == SUCCESS)
                        {
                            alogv("RecorderId[%d] veChn[%d] get stream:[%d][%p-%p-%p][%d-%d-%d][%lld]ms",
                                mRecorderId, VeChn, stVencStream.mSeq,
                                stVencStream.mpPack[0].mpAddr0, stVencStream.mpPack[0].mpAddr1, stVencStream.mpPack[0].mpAddr2,
                                stVencStream.mpPack[0].mLen0, stVencStream.mpPack[0].mLen1, stVencStream.mpPack[0].mLen2,
                                stVencStream.mpPack[0].mPTS/1000);
                            MuxStreamNode *pStreamNode = PutStreamToNodeList(stVencStream, nVencId);
                            mStreamNodeLock.lock();
                            SendStreamToMux(pStreamNode);
                            if(mpMuxCacheManager)
                            {
                                if(NO_ERROR == mpMuxCacheManager->PushStream(stVencStream, nVencId))
                                {
                                    mpMuxCacheManager->ControlCacheLevel();
                                }
                            }
                            mStreamNodeLock.unlock();
                            ReleaseVideoStreamToVencChn(&pStreamNode->mVencStream, pStreamNode->mStreamId, VeChn);
                        }
                        else
                        {
                            aloge("fatal error! RecorderId[%d] veChn[%d] get stream fail:0x%x!", mRecorderId, VeChn, ret);
                        }
                    }
                }
                //check aencChn to try to get stream.
                if(nAeChnFd >= 0)
                {
                    if(AW_MPI_SYS_HANDLE_ISSET(nAeChnFd, &tmpRdFds))
                    {
                        tmpCnt++;
                        ret = AW_MPI_AENC_GetStream(mAeChn, &stAencStream, 0);
                        if (ret == SUCCESS)
                        {
                            alogv("RecorderId[%d] aeChn[%d] get stream:[%d][%p-%d][%lld]ms", mRecorderId, mAeChn,
                                stAencStream.mId, stAencStream.pStream, stAencStream.mLen, stAencStream.mTimeStamp/1000);
                            MuxStreamNode *pStreamNode = PutStreamToNodeList(stAencStream, mAudioStreamId);
                            mStreamNodeLock.lock();
                            SendStreamToMux(pStreamNode);
                            if(mpMuxCacheManager)
                            {
                                if(NO_ERROR == mpMuxCacheManager->PushStream(stAencStream, mAudioStreamId))
                                {
                                    mpMuxCacheManager->ControlCacheLevel();
                                }
                            }
                            mStreamNodeLock.unlock();
                            ReleaseAudioStreamToAencChn(&pStreamNode->mAencStream, pStreamNode->mStreamId, mAeChn);
                        }
                        else
                        {
                            aloge("fatal error! RecorderId[%d] AeChn[%d] get stream fail:0x%x!", mRecorderId, mAeChn, ret);
                        }
                    }
                }
                //check aencChn to try to get stream.
                if(nTeChnFd >= 0)
                {
                    if(AW_MPI_SYS_HANDLE_ISSET(nTeChnFd, &tmpRdFds))
                    {
                        tmpCnt++;
                        ret = AW_MPI_TENC_GetStream(mTeChn, &stTencStream, 0);
                        if (ret == SUCCESS)
                        {
                            alogv("RecorderId[%d] teChn[%d] get stream:[%d][%p-%d][%lld]ms", mRecorderId, mTeChn,
                                stTencStream.mId, stTencStream.pStream, stTencStream.mLen, stTencStream.mTimeStamp/1000);
                            MuxStreamNode *pStreamNode = PutStreamToNodeList(stTencStream, mTextStreamId);
                            mStreamNodeLock.lock();
                            SendStreamToMux(pStreamNode);
                            if(mpMuxCacheManager)
                            {
                                if(NO_ERROR == mpMuxCacheManager->PushStream(stTencStream, mTextStreamId))
                                {
                                    mpMuxCacheManager->ControlCacheLevel();
                                }
                            }
                            mStreamNodeLock.unlock();
                            ReleaseTextStreamToTencChn(&pStreamNode->mTencStream, pStreamNode->mStreamId, mTeChn);
                        }
                        else
                        {
                            aloge("fatal error! RecorderId[%d] TeChn[%d] get stream fail:0x%x!", mRecorderId, mTeChn, ret);
                        }
                    }
                }
            }
            else
            {
                alogd("RecorderId[%d] handle_select timeout! fd cnts:%d", mRecorderId, nReadyCnt);
            }
        }
        else
        {
            mpSendStreamMsgMgr->WaitMessage(10*1000);
        }
    }
_exit0:
    SendStreamPromise.set_value(0);  // set result to 0. Notify future
}

status_t EyeseeRecorder::start()
{
    Mutex::Autolock autoLock(mLock);
    if (!(mCurrentState & MEDIA_RECORDER_PREPARED))
    {
        aloge("start called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    //alogd("EyeseeRecorder[%p] start, videoSize[%dx%d]!", this, mVideoWidth, mVideoHeight);
    status_t result = NO_ERROR;
    ERRORTYPE ret;
    Mutex::Autolock autoLock2(mSendFrameLock);
    for (std::map<VI_DEV, CameraRecordingProxy*>::iterator it = mCameraProxyMap.begin(); it != mCameraProxyMap.end(); ++it)
    {
        VI_DEV Vipp = it->first;
        CameraRecordingProxy *pCameraProxy = it->second;
        pCameraProxy->startRecording(Vipp, new CameraProxyListener(this, Vipp), mRecorderId);
    }
#if 0
    if(mpCameraProxy)
    {
        mpCameraProxy->startRecording(mCameraSourceChannel, new CameraProxyListener(this, mCameraSourceChannel), mRecorderId);
    }
#endif
    if(mEnableDBRC)
    {
        std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin();
        VencParameters *pVencParam = it->second;
        VENC_RC_MODE_E VideoRCMode = pVencParam->getVideoEncodingRateControlMode();
        if (VideoRCMode == VENC_RC_MODE_H264CBR || VideoRCMode == VENC_RC_MODE_H265CBR || VideoRCMode == VENC_RC_MODE_MJPEGCBR)
        {
            mpDBRC = new DynamicBitRateControl(this);
            if (mpDBRC == NULL)
            {
                aloge("fatal error! DynamicBitRateControl construct fail!");
            }
            else
            {
                alogd("Start DBRC to control VEnc bitrate when write_card speed is low!");
            }
        }
        else
        {
            alogw("Dynamic BitRate Control only run when CBR, but yours: %d!", VideoRCMode);
        }
    }

    for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
    {
        VencParameters *pVencParam = it->second;
        VENC_CHN VeChn = pVencParam->getVencChnIndex();
        if(VeChn >= 0)
        {
            int venc_ret = 0;
            venc_ret = AW_MPI_VENC_StartRecvPic(VeChn);
            alogd("Venc[%d] startRecvPic!", VeChn);
            if(SUCCESS != venc_ret)
            {
                aloge("fatal error:%x Venc[%d] rec AW_MPI_VENC_StartRecvPic",venc_ret, VeChn);
                if(ERR_VENC_NOMEM == venc_ret)
                {
                    result = NO_MEMORY;
                }
                else
                {
                    result = UNKNOWN_ERROR;
                }
            }
        }
    }

    if(mAiChn >= 0)
    {
        AW_MPI_AI_EnableChn(mAiDev, mAiChn);
    }
    if(mAeChn >= 0)
    {
        AW_MPI_AENC_StartRecvPcm(mAeChn);
    }
#if (MPPCFG_TEXTENC!=0)
    if(mTeChn >= 0)
    {
        AW_MPI_TENC_StartRecvText(mTeChn);
    }
#endif
    //start all muxChns.
    for(auto&& muxChn : mMuxChns)
    {
        ret = AW_MPI_MUX_StartChn(muxChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! RecorderId[%d] muxChn[%d] start fail[0x%x]!", mRecorderId, muxChn, ret);
        }
    }
    //start send_stream_thread.
    for (std::pair<const int, VencParameters*>& elem : mVencInfoMap)
    {
        int nVencId = elem.first; //vencId is streamId of vencStream.
        auto iRet = mStreamNodeListMap.insert({nVencId, std::list<MuxStreamNode>()});
        if(iRet.second != true)
        {
            aloge("fatal error! recorder[%d] streamNodeListMap insert streamId[%d] fail!", mRecorderId, nVencId);
        }
    }
    if(mAudioStreamId >= 0)
    {
        auto iRet = mStreamNodeListMap.insert({mAudioStreamId, std::list<MuxStreamNode>()});
        if(iRet.second != true)
        {
            aloge("fatal error! recorder[%d] streamNodeListMap insert streamId[%d] fail!", mRecorderId, mAudioStreamId);
        }
    }
    if(mTextStreamId >= 0)
    {
        auto iRet = mStreamNodeListMap.insert({mTextStreamId, std::list<MuxStreamNode>()});
        if(iRet.second != true)
        {
            aloge("fatal error! recorder[%d] streamNodeListMap insert streamId[%d] fail!", mRecorderId, mTextStreamId);
        }
    }
    std::promise<int> SendStreamPromise;
    mSendStreamFuture = SendStreamPromise.get_future();
    mpSendStreamMsgMgr = new MessageManager;
    if(NULL == mpSendStreamMsgMgr)
    {
        aloge("fatal error! create message manager fail");
    }
    mpSendStreamThread = new std::thread(&EyeseeRecorder::SendStreamThreadFunc, this, std::move(SendStreamPromise));
    if(NULL == mpSendStreamThread)
    {
        aloge("fatal error! recorderId[%d] create send stream thread failure.", mRecorderId);
    }
    SmartMessage stSendMsg;
    stSendMsg.what = (int)SendStreamMsgType::SetState;
    stSendMsg.args.push_back((int)SendStreamThreadState::Executing);
    stSendMsg.spMsgReply = std::make_shared<SmartMessage::MessageReply>();
    mpSendStreamMsgMgr->PutMessage(stSendMsg);
    //wait message reply
    int waitRet = stSendMsg.spMsgReply->WaitReply(1000);
    if(0 == waitRet)
    {
        //read message reply information
        alogd("RecorderId[%d] receive send_stream_thread SetState reply: 0x%x", mRecorderId, stSendMsg.spMsgReply->mnReplyResult);
    }
    else
    {
        aloge("fatal error! RecorderId[%d] wait set state timeout, ret:%d", mRecorderId, waitRet);
    }
    mCurrentState = MEDIA_RECORDER_RECORDING;
    return result;
}

status_t EyeseeRecorder::stop(bool bShutDownNowFlag)
{
    status_t ret = NO_ERROR;
    alogv("stop");
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState != MEDIA_RECORDER_RECORDING && mCurrentState != MEDIA_RECORDER_PAUSE)
    {
        aloge("stop called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    stop_l(bShutDownNowFlag);
    doCleanUp();
    mCurrentState = MEDIA_RECORDER_IDLE;
    return ret;
}

status_t EyeseeRecorder::pause(bool bPause)
{
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState != MEDIA_RECORDER_RECORDING && mCurrentState != MEDIA_RECORDER_PAUSE)
    {
        aloge("stop called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    if((bPause && mCurrentState==MEDIA_RECORDER_PAUSE) || (bPause==false && mCurrentState==MEDIA_RECORDER_RECORDING))
    {
        alogd("already in state[0x%x]", mCurrentState);
        return NO_ERROR;
    }
    Mutex::Autolock autoLock2(mSendFrameLock);
    if(mAiDev >=0 && mAiChn >= 0)
    {
        AW_MPI_AI_IgnoreData(mAiDev, mAiChn, bPause?TRUE:FALSE, TRUE);
    }
    if (bPause == true)
        mCurrentState = MEDIA_RECORDER_PAUSE;
    else
        mCurrentState = MEDIA_RECORDER_RECORDING;
    return NO_ERROR;
}

status_t EyeseeRecorder::reset()
{
    alogv("reset");
    Mutex::Autolock autoLock(mLock);
    //doCleanUp();
    status_t ret = UNKNOWN_ERROR;
    switch (mCurrentState)
    {
        case MEDIA_RECORDER_IDLE:
            ret = OK;
            break;
        case MEDIA_RECORDER_PREPARED:
        case MEDIA_RECORDER_RECORDING:
        case MEDIA_RECORDER_DATASOURCE_CONFIGURED:
        case MEDIA_RECORDER_PAUSE:
        case MEDIA_RECORDER_ERROR:
        {
            ret = doReset();
            if (OK != ret)
            {
                return ret;  // No need to continue
            }
        }  // Intentional fall through
        case MEDIA_RECORDER_INITIALIZED:
            ret = close();
            break;

        default:
        {
            aloge("Unexpected non-existing state: %d", mCurrentState);
            break;
        }
    }
    doCleanUp();
    return ret;
}

status_t EyeseeRecorder::init()
{
    alogv("init");
    if (!(mCurrentState & MEDIA_RECORDER_IDLE))
    {
        aloge("init called in an invalid state(%d)", mCurrentState);
        return INVALID_OPERATION;
    }

    mCurrentState = MEDIA_RECORDER_INITIALIZED;
    return NO_ERROR;
}

status_t EyeseeRecorder::close()
{
    alogv("close");
    if (!(mCurrentState & MEDIA_RECORDER_INITIALIZED))
    {
        aloge("close called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    mCurrentState = MEDIA_RECORDER_IDLE;
    return NO_ERROR;
}

status_t EyeseeRecorder::doReset()
{
    alogv("doReset");
    if (!(mCurrentState & MEDIA_RECORDER_PREPARED)
        && !(mCurrentState & MEDIA_RECORDER_RECORDING)
        && !(mCurrentState & MEDIA_RECORDER_DATASOURCE_CONFIGURED)
        && !(mCurrentState & MEDIA_RECORDER_ERROR))
    {
        aloge("doReset called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    status_t ret = stop_l();
    if (OK != ret)
    {
        aloge("doReset failed: %d", ret);
        mCurrentState = MEDIA_RECORDER_ERROR;
        return ret;
    }
    else
    {
        mCurrentState = MEDIA_RECORDER_INITIALIZED;
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::stop_l(bool bShutDownNowFlag)
{
    ERRORTYPE ret;
    for (std::map<VI_DEV, CameraRecordingProxy*>::iterator it = mCameraProxyMap.begin(); it != mCameraProxyMap.end(); ++it)
    {
        CameraRecordingProxy *pCameraProxy = it->second;
        pCameraProxy->stopRecording(it->first, mRecorderId);
    }
#if 0
    if(mpCameraProxy)
    {
        mpCameraProxy->stopRecording(mCameraSourceChannel, mRecorderId);
    }
#endif

    if (mpDBRC != NULL)
    {
        delete mpDBRC;
        mpDBRC = NULL;
    }

    VencParameters *pVencParam = NULL;
    for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
    {
        pVencParam = it->second;
        VENC_CHN VeChn = pVencParam->getVencChnIndex();
        if(VeChn >= 0)
        {
            AW_MPI_VENC_StopRecvPic(VeChn);
        }
    }
#if (MPPCFG_TEXTENC!=0)
    if(mTeChn >= 0)
    {
        AW_MPI_TENC_StopRecvText(mTeChn);
    }
#endif
    if(mAiChn >= 0)
    {
        AW_MPI_AI_DisableChn(mAiDev, mAiChn);
    }
    if(mAeChn >= 0)
    {
        AW_MPI_AENC_StopRecvPcm(mAeChn);
    }
    if(mpSendStreamThread)
    {
        SmartMessage stSendMsg;
        stSendMsg.what = (int)SendStreamMsgType::Stop;
        mpSendStreamMsgMgr->PutMessage(stSendMsg);
        
        int nSendStreamThreadResult = mSendStreamFuture.get();
        alogd("RecorderId[%d] send stream thread exit result:%d", mRecorderId, nSendStreamThreadResult);
        mpSendStreamThread->join();
        delete mpSendStreamThread;
        mpSendStreamThread = NULL;
    }
    if(mpSendStreamMsgMgr)
    {
        delete mpSendStreamMsgMgr;
        mpSendStreamMsgMgr = NULL;
    }
    //stop all muxChns
    for(auto&& muxChn : mMuxChns)
    {
        ret = AW_MPI_MUX_StopChn(muxChn, (BOOL)bShutDownNowFlag);
        if(ret != SUCCESS)
        {
            aloge("fatal error! RecorderId[%d] muxChn[%d] stop fail[0x%x]!", mRecorderId, muxChn, ret);
        }
    }
    //destroy all muxChns
    for(auto&& muxChn : mMuxChns)
    {
        ret = AW_MPI_MUX_DestroyChn(muxChn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! RecorderId[%d] muxChn[%d] destroy fail[0x%x]!", mRecorderId, muxChn, ret);
        }
    }
    {
        std::lock_guard<std::mutex> autoLock(mStreamNodeLock);
        for (std::pair<const int, std::list<MuxStreamNode>>& pair : mStreamNodeListMap)
        {
            int nStreamId = pair.first; //vencId is streamId of vencStream.
            std::list<MuxStreamNode>& streamNodeList = pair.second;
            int nNum = streamNodeList.size();
            if(nNum != 0)
            {
                aloge("fatal error! RecorderId[%d] need check why streamId[%d] node list has [%d]nodes, is not empty!",
                    mRecorderId, nStreamId, nNum);
            }
        }
        mStreamNodeListMap.clear();
    }
    mMuxChns.clear();
    mMuxChnAttrs.clear();
    mPolicyMap.clear();
    mMuxStrmIdsMap.clear();
    
    for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
    {
        pVencParam = it->second;
        VENC_CHN VeChn = pVencParam->getVencChnIndex();
        if(VeChn >= 0)
        {
            AW_MPI_VENC_ResetChn(VeChn);
            AW_MPI_VENC_DestroyChn(VeChn);
            VeChn = MM_INVALID_CHN;
            pVencParam->setVencChnIndex(VeChn);
        }
     }
#if (MPPCFG_TEXTENC!=0)
    if(mTeChn >= 0)
    {
        AW_MPI_TENC_ResetChn(mTeChn);
        AW_MPI_TENC_DestroyChn(mTeChn);
        mTeChn = MM_INVALID_CHN;
    }
#endif
    if(mAiChn >= 0)
    {
        AW_MPI_AI_ResetChn(mAiDev, mAiChn);
        AW_MPI_AI_DestroyChn(mAiDev, mAiChn);
        mAiChn = MM_INVALID_CHN;
    }
    if(mAeChn >= 0)
    {
        AW_MPI_AENC_ResetChn(mAeChn);
        AW_MPI_AENC_DestroyChn(mAeChn);
        mAeChn = MM_INVALID_CHN;
    }
    for(std::vector<OutputSinkInfo>::iterator it = mSinkInfos.begin(); it != mSinkInfos.end(); ++it)
    {
        if(it->mOutputFd >= 0)
        {
            ::close(it->mOutputFd);
            it->mOutputFd = -1;
        }
    }
    mSinkInfos.clear();

    if(mpInputFrameManager!=NULL)
    {
        delete mpInputFrameManager;
        mpInputFrameManager = NULL;
    }
#if 0
    if(mpCameraProxy)
    {
        delete mpCameraProxy;
        mpCameraProxy = NULL;
    }
#endif
    for (std::map<VI_DEV, CameraRecordingProxy*>::iterator it = mCameraProxyMap.begin(); it != mCameraProxyMap.end();)
    {
        if (it->second)
        {
            delete it->second;
            it->second = NULL;
        }
        it = mCameraProxyMap.erase(it);
    }
    for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end();)
    {
        if (it->second)
        {
            delete it->second;
            it->second = NULL;
        }
        it = mVencInfoMap.erase(it);
    }
    mVeVippBindMap.clear();
    return NO_ERROR;
}

status_t EyeseeRecorder::reset_l()
{
    alogv("reset_l");
    //doCleanUp();
    status_t ret = UNKNOWN_ERROR;
    switch (mCurrentState)
    {
        case MEDIA_RECORDER_IDLE:
            ret = OK;
            break;
        case MEDIA_RECORDER_PREPARED:
        case MEDIA_RECORDER_RECORDING:
        case MEDIA_RECORDER_DATASOURCE_CONFIGURED:
        case MEDIA_RECORDER_ERROR:
        {
            ret = doReset();
            if (OK != ret)
            {
                return ret;  // No need to continue
            }
        }  // Intentional fall through
        case MEDIA_RECORDER_INITIALIZED:
            ret = close();
            break;

        default:
        {
            aloge("Unexpected non-existing state: %d", mCurrentState);
            break;
        }
    }
    doCleanUp();
    return ret;
}

void EyeseeRecorder::setOnErrorListener(OnErrorListener *pl)
{
    mOnErrorListener = pl;
}

void EyeseeRecorder::setOnInfoListener(OnInfoListener *pListener)
{
	mOnInfoListener = pListener;
}

void EyeseeRecorder::setOnDataListener(OnDataListener *pListener)
{
	mOnDataListener = pListener;
}

/*status_t EyeseeRecorder::setBsFrameRawDataType(callback_out_data_type type)
{
    mCallbackOutDataType = type;
    return NO_ERROR;
}*/

/*status_t EyeseeRecorder::setCallbackOutStreamList(std::vector<int> nStreamIds)
{
    mCallbackOutStreamIdList = nStreamIds;
    return NO_ERROR;
}*/

void EyeseeRecorder::dataCallbackTimestamp(const VIDEO_FRAME_BUFFER_S *pCameraFrameInfo, VI_DEV Vipp)
{
    //Mutex::Autolock autoLock(mLock);
    Mutex::Autolock autoLock2(mSendFrameLock);
    media_recorder_states eCurState = mCurrentState;

    //first find vipp matching cameraRecordingProxy.
    CameraRecordingProxy *pCameraProxy = NULL;
    std::map<VI_DEV, CameraRecordingProxy*>::iterator itCameraMap = mCameraProxyMap.find(Vipp);
    if (mCameraProxyMap.end() == itCameraMap)
    {
        aloge("fatal error! Rec[%d] mCameraProxyMap don't have ChnId[%d]", mRecorderId, Vipp);
    }
    else
    {
        pCameraProxy = itCameraMap->second;
    }

    // if encoder is stopped,release this frame.if encoder is paused ,because need to avsync so continue.
    if(eCurState != MEDIA_RECORDER_RECORDING && eCurState != MEDIA_RECORDER_PAUSE)
    {
        aloge("fatal error! call dataCallbackTimestamp when recorder state is [0x%x]", eCurState);
//        std::map<VI_DEV, CameraRecordingProxy*>::iterator it = mCameraProxyMap.find(Vipp);
//        if (mCameraProxyMap.end() == it)
//        {
//            aloge("fatal error! mCameraProxyMap don't have ChnId[%d]", mRecorderId, Vipp);
//            return;
//        }
//        CameraRecordingProxy *pCameraProxy = it->second;
        pCameraProxy->releaseRecordingFrame(Vipp, pCameraFrameInfo->mFrameBuf.mId);
        return;
    }
    VIDEO_FRAME_INFO_S *pFrameInfo = (VIDEO_FRAME_INFO_S*)&pCameraFrameInfo->mFrameBuf;
    if(!mTimeLapseEnable && mAudioEncoder != PT_MAX && ((int64_t)pFrameInfo->VFrame.mpts < rec_start_timestamp || -1==rec_start_timestamp))
    {
//        std::map<VI_DEV, CameraRecordingProxy*>::iterator it = mCameraProxyMap.find(Vipp);
//        if (mCameraProxyMap.end() == it)
//        {
//            aloge("fatal error! mCameraProxyMap don't have ChnId[%d]", Vipp);
//            return;
//        }
//        CameraRecordingProxy *pCameraProxy = it->second;
        alogw("RecorderId[%d] avsync_drp:%lld-%lld-%d-%dx%d", mRecorderId, rec_start_timestamp, pFrameInfo->VFrame.mpts, Vipp,
            pFrameInfo->VFrame.mWidth, pFrameInfo->VFrame.mHeight);
        pCameraProxy->releaseRecordingFrame(Vipp, pFrameInfo->mId);
    }
    else
    {
        if(eCurState == MEDIA_RECORDER_PAUSE)
        {
//            std::map<VI_DEV, CameraRecordingProxy*>::iterator it = mCameraProxyMap.find(Vipp);
//            if (mCameraProxyMap.end() == it)
//            {
//                aloge("fatal error! mCameraProxyMap don't have vipp[%d]", Vipp);
//                return;
//            }
            if ((mAiChn == MM_INVALID_CHN) && !mPauseVideoPts)
            {
                mPauseVideoPts = pFrameInfo->VFrame.mpts;
            }
            //CameraRecordingProxy *pCameraProxy = it->second;
            pCameraProxy->releaseRecordingFrame(Vipp, pFrameInfo->mId);
        }
        else
        {
            //modify video frame pts to suit to pause.
            VIDEO_FRAME_INFO_S tmpFrameInfo;
            if(mPauseAudioDuration > 0)
            {
                //we don't want original frameInfo to be changed in framePts, because this frameInfo may be sent to many EyeseeRecorders.
                tmpFrameInfo = pCameraFrameInfo->mFrameBuf;
                pFrameInfo = &tmpFrameInfo;
                pFrameInfo->VFrame.mpts -= mPauseAudioDuration*1000;
            }
            if ((mAiChn == MM_INVALID_CHN) && mPauseVideoPts > 0) {
                mPauseVideoDuration += pFrameInfo->VFrame.mpts - mPauseVideoPts;
                mPauseVideoPts = 0;
            }
            if ((mAiChn == MM_INVALID_CHN) && mPauseVideoDuration > 0)
            {
                //do this process when only record video
                tmpFrameInfo = pCameraFrameInfo->mFrameBuf;
                pFrameInfo = &tmpFrameInfo;
                pFrameInfo->VFrame.mpts -= mPauseVideoDuration;
            }
            bool bIncreaseBufRef = false;
            VencParameters *pVencParam = NULL;
            VENC_CHN VeChn = MM_INVALID_CHN;
            ERRORTYPE ret = FAILURE;
            int nVeNumBindVipp = 0;
            for (std::map<int, VI_DEV>::iterator it = mVeVippBindMap.begin(); it != mVeVippBindMap.end(); ++it)
            {
                if (Vipp == it->second)
                {
                    nVeNumBindVipp++;
                    pVencParam = mVencInfoMap.find(it->first)->second;
                    VeChn = pVencParam->getVencChnIndex();
//                    std::map<VI_DEV, CameraRecordingProxy*>::iterator itCRP = mCameraProxyMap.find(Vipp);
//                    if (mCameraProxyMap.end() == itCRP)
//                    {
//                        aloge("fatal error! mCameraProxyMap don't have ChnId[%d]", Vipp);
//                        return;
//                    }
//                    CameraRecordingProxy *pCameraProxy = itCRP->second;
                    if (true == bIncreaseBufRef)
                    {
                        pCameraProxy->increaseBufRef(Vipp, (VIDEO_FRAME_BUFFER_S *)pCameraFrameInfo);
                    }
                    ret = AW_MPI_VENC_SendFrame(VeChn, pFrameInfo, -1);
                    if(ret!=SUCCESS)
                    {
                        if(ERR_VENC_EXIST == ret)
                        {
                            //alogd("copy success ,return frame now!");
                        }
                        else if(ERR_VENC_NOT_PERM == ret)
                        {
                            //alogd("RecorderId[%d] send frame to venc not permit [0x%x]", mRecorderId, ret);
                        }
                        else
                        {
                            aloge("fatal error! send frame to venc fail[0x%x]", ret);
                        }

                        pCameraProxy->releaseRecordingFrame(Vipp, pFrameInfo->mId);
                    }
                    bIncreaseBufRef = true;
                }
            }
            if (0 == nVeNumBindVipp)
            {
//                alogd("Be careful! Rec[%d] has no Venc bind vipp[%d], frameId[%d] refCnt[%d] must be release!",mRecorderId, Vipp,
//                    pFrameInfo->mId, pCameraFrameInfo->mRefCnt);
                pCameraProxy->releaseRecordingFrame(Vipp, pFrameInfo->mId);
            }
        }
    }
}

status_t EyeseeRecorder::removeOutputSink(int muxerId)
{
    alogd("RecorderId[%d] remove Output Sink, muxerId[%d]", mRecorderId, muxerId);
    //status_t result = UNKNOWN_ERROR;
    ERRORTYPE ret;
    if (muxerId < 0)
    {
        aloge("Invalid muxerId");
        return BAD_TYPE;
    }

    Mutex::Autolock autoLock(mLock); //for mCurrentState
    if (!(mCurrentState & MEDIA_RECORDER_DATASOURCE_CONFIGURED) && !(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("remove Output Sink called in an invalid state: %d", mCurrentState);
        return INVALID_OPERATION;
    }
    mMuxChnLock.lock();
    int idx = 0;
    bool bFindFlag = false;
    //test if normal.
    int findNum = 0;
    for(std::vector<OutputSinkInfo>::iterator it = mSinkInfos.begin(); it != mSinkInfos.end(); ++it)
    {
        if(it->mMuxerId == muxerId)
        {
            findNum++;
        }
    }
    if(findNum > 1)
    {
        aloge("fatal error! find more [%d] muxerId[%d], check code!", findNum, muxerId);
    }
    //remote OutputSinkInfo
    for(std::vector<OutputSinkInfo>::iterator it = mSinkInfos.begin(); it != mSinkInfos.end(); ++it, ++idx)
    {
        if(it->mMuxerId == muxerId)
        {
            bFindFlag = true;
            break;
        }
    }
    if(bFindFlag)
    {
        if(mSinkInfos[idx].mOutputFd >= 0)
        {
            //alogd("close fd[%d]", mSinkInfos[idx].mOutputFd);
            ::close(mSinkInfos[idx].mOutputFd);
            mSinkInfos[idx].mOutputFd = -1;
        }
        mSinkInfos.erase(mSinkInfos.begin()+idx);
    }

//    idx = 0;
//    bFindFlag = false;
//    for(std::vector<MUX_CHN_ATTR_S>::iterator it = mMuxChnAttrs.begin(); it != mMuxChnAttrs.end(); ++it, ++idx)
//    {
//        if(it->mMuxerId == muxerId)
//        {
//            bFindFlag = true;
//            break;
//        }
//    }
    if(bFindFlag)
    {
        MUX_CHN dstMuxChn = mMuxChns[idx];
        mMuxChns.erase(mMuxChns.begin()+idx);
        mMuxChnAttrs.erase(mMuxChnAttrs.begin()+idx);
        mPolicyMap.erase(muxerId);
        mMuxStrmIdsMap.erase(muxerId);
        mMuxChnLock.unlock();

        //can't stop and destroy muxChn in mMuxChnLock, it will lead to deadlock. So must unlock mMuxChnLock, then stop and destroy muxChn.
        //SendStreamThreadFunc(), RecRender_ComponentThread(), caller_thread calling removeOutputSink().
        //SendStreamThreadFunc()->SendStreamToMux(): mStreamNodeLock,mMuxChnLock
        //RecRender_ComponentThread()->RecRenderEmptyBufferDone()->notify()-MPP_EVENT_RELEASE_VENC_STREAM->ReleaseVideoStreamToVencChn(): mStreamNodeLock
        //removeOutputSink(): mMuxChnLock.
        if(mCurrentState & (MEDIA_RECORDER_PREPARED | MEDIA_RECORDER_RECORDING | MEDIA_RECORDER_PAUSE))
        {
            if(mCurrentState & (MEDIA_RECORDER_RECORDING | MEDIA_RECORDER_PAUSE))
            {
                ret = AW_MPI_MUX_StopChn(dstMuxChn, FALSE);
                if(ret != SUCCESS)
                {
                    aloge("fatal error! RecorderId[%d] muxChn[%d-%d] stop fail:0x%x", mRecorderId, dstMuxChn, muxerId, ret);
                }
            }
            ret = AW_MPI_MUX_DestroyChn(dstMuxChn);
            if(ret != SUCCESS)
            {
                aloge("fatal error! RecorderId[%d] muxChn[%d-%d] destroy fail:0x%x", mRecorderId, dstMuxChn, muxerId, ret);
            }
        }
        return NO_ERROR;
    }
    else
    {
        aloge("fatal error! RecorderId[%d] can't find muxerId[%d]!", mRecorderId, muxerId);
        mMuxChnLock.unlock();
        return BAD_VALUE;
    }
}

/**
  set next fd to muxChn.

  After app receive need_set_next_fd, app call this api to set next fd to muxChn, then muxChn will switch to next file
  when current file reach duration.
*/
status_t EyeseeRecorder::setOutputFileSync(int fd, int64_t fallocateLength, int muxerId)
{
    Mutex::Autolock autoLock(mLock);
    alogv("setOutputFileSync fd=%d", fd);
	if (fd < 0)
    {
		aloge("Invalid parameter");
		return BAD_VALUE;
	}
    if (!(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("set OutputFileSync called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(mMuxChns.size() != mMuxChnAttrs.size())
    {
        aloge("fatal error! mux channels are not same, check code!");
    }
    //find mux channel by muxerId
    MUX_CHN muxChn = MM_INVALID_CHN;
    std::vector<MUX_CHN>::iterator itChn = mMuxChns.begin();
    for(std::vector<OutputSinkInfo>::iterator it = mSinkInfos.begin(); it != mSinkInfos.end(); ++it)
    {
        if(it->mMuxerId == muxerId)
        {
            muxChn = *itChn;
            break;
        }
        ++itChn;
    }
    if(muxChn != MM_INVALID_CHN)
    {
        ERRORTYPE ret = AW_MPI_MUX_SwitchFd(muxChn, fd, fallocateLength);
        int nFindCnt = 0;
        for(OutputSinkInfo& i : mSinkInfos)
        {
            if(i.mMuxerId == muxerId)
            {
                if(0 == nFindCnt)
                {
                    if(SUCCESS == ret)
                    {
                        if(i.mOutputFd >= 0)
                        {
                            ::close(i.mOutputFd);
                            i.mOutputFd = -1;
                        }
                        i.mOutputFd = dup(fd);
                    }
                    else
                    {
                        aloge("Be careful! mux switch fd fail[0x%x]", ret);
                    }
                }
                else
                {
                    aloge("fatal error! nFindCnt[%d]", nFindCnt);
                }
                nFindCnt++;
            }
        }
        if(nFindCnt <= 0)
        {
            aloge("fatal error! why muxerId[%d] is not found!", muxerId);
        }

        return NO_ERROR;
    }
    else
    {
        aloge("fatal error! can't find muxChn which muxerId[%d]", muxerId);
        return BAD_VALUE;
    }
}

status_t EyeseeRecorder::setOutputFileSync(char* path, int64_t fallocateLength, int muxerId)
{
    status_t ret;
    if(path!=NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fd < 0)
        {
		aloge("Failed to open %s", path);
		return BAD_VALUE;
	}
        ret = setOutputFileSync(fd, fallocateLength, muxerId);
        ::close(fd);
        return ret;
    }
    else
    {
        return BAD_VALUE;
    }
}

status_t EyeseeRecorder::setSdcardState(bool bExist)
{
    alogd("need implement");
    return UNKNOWN_ERROR;
}

#if 0
status_t EyeseeRecorder::setImpactFileDuration(int bfTimeMs, int afTimeMs)
{
    if (!(mCurrentState & MEDIA_RECORDER_IDLE) && !(mCurrentState & MEDIA_RECORDER_INITIALIZED) && !(mCurrentState & MEDIA_RECORDER_DATASOURCE_CONFIGURED))
    {
        aloge("setImpactFileDuration called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    mImpactFileDuration[0] = bfTimeMs;
    mImpactFileDuration[1] = afTimeMs;
    return UNKNOWN_ERROR;
}

status_t EyeseeRecorder::setImpactOutputFile(int fd, int64_t fallocateLength, int muxerId)
{
    if (!(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("setImpactOutputFile called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    alogd("need implement");
    return UNKNOWN_ERROR;
}

status_t EyeseeRecorder::setImpactOutputFile(char* path, int64_t fallocateLength, int muxerId)
{
    status_t ret;
    if(path!=NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT, 0666);
	if (fd < 0)
        {
		aloge("Failed to open %s", path);
		return BAD_VALUE;
	}
        ret = setImpactOutputFile(fd, fallocateLength, muxerId);
        ::close(fd);
        return ret;
    }
    else
    {
        return BAD_VALUE;
    }
}
#endif

/**
  set mux cache duration. unit:ms
  must called before prepare().
*/
status_t EyeseeRecorder::setMuxCacheDuration(int nCacheMs)
{
    if (!(mCurrentState & MEDIA_RECORDER_IDLE) && !(mCurrentState & MEDIA_RECORDER_INITIALIZED)
        && !(mCurrentState & MEDIA_RECORDER_DATASOURCE_CONFIGURED))
    {
        aloge("called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if (nCacheMs < 0)
    {
        aloge("fatal error! called in an invalid bfTimeMs: %d", nCacheMs);
        return BAD_VALUE;
    }
    mMuxCacheDuration = nCacheMs;
    return NO_ERROR;
}

/**
  set streamIds cacheManager receive.
  must call before EyeseeRecorder::prepare() and after setVencParameters() and setAudioEncoder().
*/
status_t EyeseeRecorder::setMuxCacheStrmIds(MuxStreamIdsInfo &StrmIdsInfo)
{
    if (!(mCurrentState & MEDIA_RECORDER_IDLE) && !(mCurrentState & MEDIA_RECORDER_INITIALIZED) 
        && !(mCurrentState & MEDIA_RECORDER_DATASOURCE_CONFIGURED))
    {
        aloge("called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    mMuxCacheStrmIds = StrmIdsInfo;
    return NO_ERROR;
}

/**
  set streamIds the muxChn receive.
  must call before EyeseeRecorder::prepare() and after setVencParameters() and setAudioEncoder().
*/
status_t EyeseeRecorder::setMuxStrmIds(int nMuxerId, MuxStreamIdsInfo &StrmIdsInfo)
{
    if (!(mCurrentState & MEDIA_RECORDER_IDLE) && !(mCurrentState & MEDIA_RECORDER_INITIALIZED) 
        && !(mCurrentState & MEDIA_RECORDER_DATASOURCE_CONFIGURED))
    {
        aloge("called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    auto iRet = mMuxStrmIdsMap.insert(std::pair<int, MuxStreamIdsInfo>{nMuxerId, StrmIdsInfo});
    if(iRet.second != true)
    {
        aloge("fatal error! recorder[%d] muxerId[%d] streamIds insert fail!", mRecorderId, nMuxerId);
        return BAD_VALUE;
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::switchFileNormal(int fd, int64_t fallocateLength, int muxerId)
{
    Mutex::Autolock autoLock(mLock);
    if (fd < 0)
    {
        if(fd != -1)
        {
            aloge("Invalid parameter");
            return BAD_VALUE;
        }
        else
        {
            alogd("fd is -1, only close current file");
        }
    }
    if (!(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("switch file smooth called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(mMuxChns.size() != mMuxChnAttrs.size())
    {
        aloge("fatal error! mux channels are not same, check code!");
    }
    //find mux channel by muxerId
    MUX_CHN muxChn = MM_INVALID_CHN;
    std::vector<MUX_CHN>::iterator itChn = mMuxChns.begin();
    for(std::vector<OutputSinkInfo>::iterator it = mSinkInfos.begin(); it != mSinkInfos.end(); ++it)
    {
        if(it->mMuxerId == muxerId)
        {
            muxChn = *itChn;
            break;
        }
        ++itChn;
    }
    if(muxChn != MM_INVALID_CHN)
    {
        for(auto&& i : mSinkInfos)
        {
            if(i.mMuxerId == muxerId)
            {
                if(i.mOutputFd >= 0)
                {
                    ::close(i.mOutputFd);
                    i.mOutputFd = -1;
                }
                else
                {
                    alogd("i.mOutput fd < 0, maybe previous fd is -1.");
                }
                if(fd >= 0)
                {
                    i.mOutputFd = dup(fd);
                }
                else
                {
                    alogd("set new fd -1, only close current file.");
                    i.mOutputFd = -1;
                }
            }
        }
        //AW_MPI_MUX_SwitchFd(mMuxGrp, muxChn, fd, fallocateLength);
        AW_MPI_MUX_SwitchFileNormal(muxChn, fd, fallocateLength);
        return NO_ERROR;
    }
    else
    {
        aloge("fatal error! can't find muxChn match muxerId[%d]", muxerId);
        return BAD_VALUE;
    }
}

status_t EyeseeRecorder::setThmPic(char *p_thm_buff,int thm_size, int muxerId)
{
    if (!(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("set thum pic called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(mMuxChns.size() != mMuxChnAttrs.size())
    {
        aloge("fatal error! mux channels are not same, check code!");
    }
    //find mux channel by muxerId
    MUX_CHN muxChn = MM_INVALID_CHN;
    std::vector<MUX_CHN>::iterator itChn = mMuxChns.begin();
    for(std::vector<OutputSinkInfo>::iterator it = mSinkInfos.begin(); it != mSinkInfos.end(); ++it)
    {
        if(it->mMuxerId == muxerId)
        {
            muxChn = *itChn;
            break;
        }
        ++itChn;
    }
    if(muxChn != MM_INVALID_CHN)
    {
        AW_MPI_MUX_SetThmPic(muxChn, p_thm_buff, thm_size);
        return NO_ERROR;
    }
    else
    {
        aloge("fatal error! can't find muxChn match muxerId[%d]", muxerId);
        return BAD_VALUE;
    }
}
/**
  set switch file duration policy.
  we suggest called before prepare(). we support called after prepare().
*/
status_t EyeseeRecorder::setSwitchFileDurationPolicy(int muxerId,const RecordFileDurationPolicy ePolicy)
{
    Mutex::Autolock autoLock(mLock);
    bool bFindFlag = false;
    MUX_CHN muxchn = MM_INVALID_CHN;
    ERRORTYPE ret;
    status_t result = NO_ERROR;

    mPolicyMap.find(muxerId)->second = ePolicy;
    if(mCurrentState & (MEDIA_RECORDER_PREPARED|MEDIA_RECORDER_RECORDING|MEDIA_RECORDER_PAUSE))
    {
        for(std::vector<OutputSinkInfo>::iterator it = mSinkInfos.begin(); it != mSinkInfos.end(); ++it)
        {
            if(it->mMuxerId == muxerId)
            {
                muxchn = mMuxChns[it - mSinkInfos.begin()];
                bFindFlag = true;
                break;
            }
        }
        if(!bFindFlag)
        {
            aloge("fatal error, the muxerId[%d] is error!", muxerId);
            return UNKNOWN_ERROR;
        }
        if(muxchn >= 0)
        {
            ret = AW_MPI_MUX_SetSwitchFileDurationPolicy(muxchn, ePolicy);
            if(ret != SUCCESS)
            {
                aloge("fatal error! RecorderId[%d] set switchFileDuration Policy fail[0x%x]", mRecorderId, ret);
                result = UNKNOWN_ERROR;
            }
        }
    }
    return result;
}

status_t EyeseeRecorder::getSwitchFileDurationPolicy(int muxerId, RecordFileDurationPolicy *pPolicy) const
{
    if(pPolicy)
    {
        if(mPolicyMap.find(muxerId) == mPolicyMap.end())
        {
            aloge("fatal error! RecorderId[%d] the muxerId[%d] is error!", mRecorderId, muxerId);
            return UNKNOWN_ERROR;
        }

        *pPolicy = mPolicyMap.find(muxerId)->second;
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::switchFileNormal(char* path, int64_t fallocateLength, int muxerId)
{
    status_t ret;
    if(path!=NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
        {
            aloge("Failed to open %s", path);
            return BAD_VALUE;
        }
        ret = switchFileNormal(fd, fallocateLength, muxerId);
        ::close(fd);
        return ret;
    }
    else
    {
        return BAD_VALUE;
    }
}

/**
  switch to impact file, must include muxCache.

  About impactFileDuration, it share file duration of muxerId with common file, and it can be changed during
  recording by setMaxDuration(). When reach file duration, muxerId switch to next normal file. If you want normal
  file to use previous file duration, you must call setMaxDuration() again after impace file finished.

  1 SendStreamThreadFunc() stop sending stream to this muxchn.
  2. stop muxChn.
  3. set new fd
  4. startChn() again.
  5. send stream of cacheManager.
  6. SendStreamThreadFunc() restore sending stream to this muxchn.

  @return
    BAD_VALUE
    INVALID_OPERATION
    NO_ERROR
*/
status_t EyeseeRecorder::switchImpactFile(int fd, int64_t fallocateLength, int muxerId)
{
    ERRORTYPE ret;
    Mutex::Autolock autoLock(mLock);
    if (fd < 0)
    {
        aloge("Invalid parameter");
        return BAD_VALUE;
    }
    if (!(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("recorderId[%d] switch file smooth called in an invalid state: 0x%x", mRecorderId, mCurrentState);
        return INVALID_OPERATION;
    }
    if(mMuxChns.size() != mMuxChnAttrs.size())
    {
        aloge("fatal error! mux channels are not same, check code!");
    }
    //find mux channel by muxerId
    MUX_CHN muxChn = MM_INVALID_CHN;
    for(int i=0; i<(int)mMuxChns.size(); i++)
    {
        if(mSinkInfos[i].mMuxerId == muxerId)
        {
            muxChn = mMuxChns[i];
            break;
        }
    }
    if(muxChn != MM_INVALID_CHN)
    {
        for(auto&& i : mSinkInfos)
        {
            if(i.mMuxerId == muxerId)
            {
                if(i.mOutputFd >= 0)
                {
                    ::close(i.mOutputFd);
                    i.mOutputFd = -1;
                }
                else
                {
                    alogd("i.mOutput fd < 0, maybe previous fd is -1.");
                }
                i.mOutputFd = dup(fd);
                break;
            }
        }
        //1. stop SendStreamThreadFunc() sending stream to this muxchn.
        mMuxChnLock.lock();
        if(mForbidMuxChns.size() > 0)
        {
            aloge("fatal error! recorderId[%d] forbidMuxChns size[%d] > 0", mRecorderId, mForbidMuxChns.size());
            mForbidMuxChns.clear();
        }
        mForbidMuxChns.push_back(muxChn);
        mMuxChnLock.unlock();
        //2. stop muxchn immediately
        AW_MPI_MUX_StopChn(muxChn, TRUE);
        //3. set new fd
        AW_MPI_MUX_SetFd(muxChn, fd, fallocateLength);
        //4. startChn() again
        AW_MPI_MUX_StartChn(muxChn);
        //5. send stream of cacheManager
        if(mpMuxCacheManager)
        {
            std::lock_guard<std::mutex> autoLock(mStreamNodeLock);
            SendAllCacheManagerStreamToMuxChn(muxChn);
            //6. SendStreamThreadFunc() restore sending stream to this muxchn.
            std::lock_guard<std::mutex> autoLock3(mMuxChnLock);
            mForbidMuxChns.clear();
        }
        else
        {
            aloge("fatal error! RecorderId[%d] has not muxCacheManager, why switch impact file?", mRecorderId);
            //6. SendStreamThreadFunc() restore sending stream to this muxchn.
            std::lock_guard<std::mutex> autoLock3(mMuxChnLock);
            mForbidMuxChns.clear();
        }
        return NO_ERROR;
    }
    else
    {
        aloge("fatal error! RecorderId[%d] can't find muxChn match muxerId[%d]", mRecorderId, muxerId);
        return BAD_VALUE;
    }
}

status_t EyeseeRecorder::switchImpactFile(char* path, int64_t fallocateLength, int muxerId)
{
    status_t ret;
    if(path!=NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (fd < 0)
        {
            aloge("Failed to open %s", path);
            return BAD_VALUE;
        }
        ret = switchImpactFile(fd, fallocateLength, muxerId);
        ::close(fd);
        return ret;
    }
    else
    {
        return BAD_VALUE;
    }
}

status_t EyeseeRecorder::enableDynamicBitRateControl(bool bEnable)
{
    alogd("need implement");
    return UNKNOWN_ERROR;
}

#if 0
status_t EyeseeRecorder::enableHorizonFlip(bool enable)
{
    mbHorizonfilp = enable;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        BOOL bHorizonFlipFlag;
        if(enable)
        {
            bHorizonFlipFlag = TRUE;
        }
        else
        {
            bHorizonFlipFlag = FALSE;
        }

        AW_MPI_VENC_SetHorizonFlip(mVeChn, bHorizonFlipFlag);
    }
    return NO_ERROR;

}

status_t EyeseeRecorder::enableSaveBsFile(VencSaveBSFile *pSaveParam)
{
    mSaveBSFileParam = *pSaveParam;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        AW_MPI_VENC_SaveBsFile(mVeChn, pSaveParam);
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::setProcSet(VeProcSet *pVeProcSet)
{
    mVeProcSet = *pVeProcSet;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        AW_MPI_VENC_SetProcSet(mVeChn, pVeProcSet);
    }
    return NO_ERROR;
}

 status_t EyeseeRecorder::enableColor2Grey(bool enable)
 {
    mbColor2Grey = enable;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        VENC_COLOR2GREY_S bColor2GreyFlag;
        if(enable)
        {
            bColor2GreyFlag.bColor2Grey = TRUE;
        }
        else
        {
            bColor2GreyFlag.bColor2Grey = FALSE;
        }
        AW_MPI_VENC_SetColor2Grey(mVeChn, &bColor2GreyFlag);
    }
    return NO_ERROR;
 }

 status_t EyeseeRecorder::enableAdaptiveIntraInp(bool enable)
 {
    mbAdaptiveintrainp = enable;
    Mutex::Autolock autoLock(mLock);
    if (mCurrentState & MEDIA_RECORDER_PREPARED)
    {
        BOOL bAdaptiveIntraInpFlag;
        if(enable)
        {
            bAdaptiveIntraInpFlag = TRUE;
        }
        else
        {
            bAdaptiveIntraInpFlag = FALSE;
        }
        AW_MPI_VENC_SetAdaptiveIntraInP(mVeChn, bAdaptiveIntraInpFlag);
    }
    return NO_ERROR;
 }

status_t EyeseeRecorder::enableIframeFilter(bool enable)
{
    alogd("need implement");
    return UNKNOWN_ERROR;
}

status_t EyeseeRecorder::enableNullSkip(bool enable)
{
    mNullSkipEnable = enable;
    return NO_ERROR;
}

status_t EyeseeRecorder::enablePSkip(bool enable)
{
    mPSkipEnable = enable;
    return NO_ERROR;
}

#if 0
status_t EyeseeRecorder::enableVideoEncodingLongTermRef(bool enable)
{
    mbLongTermRef = enable;
    return NO_ERROR;
}
#endif

status_t EyeseeRecorder::enableFastEncode(bool enable)
{
    mFastEncFlag = enable;
    return NO_ERROR;
}

status_t EyeseeRecorder::enableVideoEncodingPIntra(bool enable)
{
    mbPIntraEnable = enable;
    return NO_ERROR;
}
status_t EyeseeRecorder::setVideoEncodingMode(int Mode)
{
    alogd("need implement");
    return UNKNOWN_ERROR;
}

status_t EyeseeRecorder::setVideoSliceHeight(int sliceHeight)
{
    alogd("need implement");
    return UNKNOWN_ERROR;
}

/**
 * nIQpOffset:
 * for h264, [0,10). default 0, if want to decrease I frame size, increase IQpOffset to 6 in common.
 * for h265, [-12, 12]. decrease bs_size to 50% for every increase 6.
 */
status_t EyeseeRecorder::setIQpOffset(int nIQpOffset)
{
    if (PT_H264 == mVideoEncoder)
    {
        if (!(nIQpOffset>=0 && nIQpOffset<10))
        {
            aloge("IQpOffset value must be in [0, 10) for 264!");
            return BAD_VALUE;
        }
    }
    else if (PT_H265 == mVideoEncoder)
    {
        if (!(nIQpOffset>=-12 && nIQpOffset<=12))
        {
            aloge("IQpOffset value must be in [-12, 12] for 265!");
            return BAD_VALUE;
        }
    }
    else
    {
        aloge("IQpOffset can not be set for other vencoder(%d)!", mVideoEncoder);
        return BAD_VALUE;
    }

    mIQpOffset = nIQpOffset;
    return NO_ERROR;
}

/*
status_t EyeseeRecorder::setIQpRange(int maxIQp, int minIQp)
{
    if ((VideoRCMode_ABR==mVideoRCMode) && (PT_H265==mVideoEncoder || PT_H264==mVideoEncoder))
    {
        mMaxIQp = maxIQp;
        mMinIQp = minIQp;
        return NO_ERROR;
    }
    else
    {
        aloge("maxIQp and minIQp only used in ABR mode! current_vtype:%d, rc_mode:%d", mVideoEncoder, mVideoRCMode);
        return BAD_VALUE;
    }
}
*/

/*
status_t EyeseeRecorder::setIQpAndPQp(int nIQp, int nPQp)
{
    if ((VideoRCMode_FIXQP==mVideoRCMode) && (PT_H265==mVideoEncoder || PT_H264==mVideoEncoder))
    {
        mIQp = nIQp;
        mPQp = nPQp;
        return NO_ERROR;
    }
    else
    {
        aloge("mIQp and mPQp only used in FixQp mode! current_vtype:%d, rc_mode:%d", mVideoEncoder, mVideoRCMode);
        return BAD_VALUE;
    }
}
*/

/*
status_t EyeseeRecorder::setMaxVideoBitRate(int maxBitRate)
{
    mMaxVideoBitRate = maxBitRate;
    if (VideoRCMode_VBR==mVideoRCMode || VideoRCMode_ABR==mVideoRCMode)
    {
        return NO_ERROR;
    }
    else
    {
        alogw("maxBitRate only used in VBR or ABR mode! current_vtype:%d, rc_mode:%d", mVideoEncoder, mVideoRCMode);
        return NO_ERROR;
    }
}
*/

/*
status_t EyeseeRecorder::setAbrRatioChangeQp(int nAbrRatioQp)
{
    mAbrRatioChangeQp = nAbrRatioQp;
    if ((PT_H264==mVideoEncoder || PT_H265==mVideoEncoder) && VideoRCMode_ABR==mVideoRCMode)
    {
        return NO_ERROR;
    }
    else
    {
        alogw("mAbrRatioChangeQp only used in H264ABR or H265ABR mode! current_vtype:%d, rc_mode:%d", mVideoEncoder, mVideoRCMode);
        return NO_ERROR;
    }
}
*/

/*
status_t EyeseeRecorder::setAbrQuality(int nAbrQuality)
{
    mAbrQuality = nAbrQuality;
    if ((PT_H264==mVideoEncoder || PT_H265==mVideoEncoder) && VideoRCMode_ABR==mVideoRCMode)
    {
        return NO_ERROR;
    }
    else
    {
        alogw("mAbrQuality only used in H264ABR or H265ABR mode! current_vtype:%d, rc_mode:%d", mVideoEncoder, mVideoRCMode);
        return NO_ERROR;
    }
}
*/

/*
status_t EyeseeRecorder::setVEncProfile(VEncProfile nProfile)
{
    mVEncAttr.mType = mVideoEncoder;
    if(PT_H264 == mVEncAttr.mType)
    {
        switch(nProfile)
        {
            case VEncProfile_BaseLine:
                mVEncAttr.mAttrH264.mProfile = 0;
                break;
            case VEncProfile_MP:
                mVEncAttr.mAttrH264.mProfile = 1;
                break;
            case VEncProfile_HP:
                mVEncAttr.mAttrH264.mProfile = 2;
                break;
            default:
                aloge("fatal error! unsupport h264 profile[0x%x]", nProfile);
                mVEncAttr.mAttrH264.mProfile = 1;
                break;
        }
        mVEncAttr.mAttrH264.mLevel = H264_LEVEL_51;
    }
    else if(PT_H265 == mVEncAttr.mType)
    {
        switch(nProfile)
        {
            case VEncProfile_MP:
                mVEncAttr.mAttrH265.mProfile = 0;
                break;
            default:
                aloge("fatal error! unsupport h265 profile[0x%x]", nProfile);
                mVEncAttr.mAttrH265.mProfile = 0;
                break;
        }
        mVEncAttr.mAttrH265.mLevel = H265_LEVEL_62;
    }
    return NO_ERROR;
}
*/

status_t EyeseeRecorder::setVEncAttr(VEncAttr *pVEncAttr)
{
    mVEncAttr = *pVEncAttr;
    return NO_ERROR;
}

status_t EyeseeRecorder::setRoiCfg(VENC_ROI_CFG_S *pVencRoiCfg)
{
    status_t ret = NO_ERROR;
    if (!(mCurrentState & MEDIA_RECORDER_PREPARED) && !(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(SUCCESS != AW_MPI_VENC_SetRoiCfg(mVeChn, pVencRoiCfg))
    {
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

status_t EyeseeRecorder::getRoiCfg(unsigned int nIndex, VENC_ROI_CFG_S *pVencRoiCfg)
{
    status_t ret = NO_ERROR;
    if (!(mCurrentState & MEDIA_RECORDER_PREPARED) && !(mCurrentState & MEDIA_RECORDER_RECORDING))
    {
        aloge("called in an invalid state: 0x%x", mCurrentState);
        return INVALID_OPERATION;
    }
    if(SUCCESS != AW_MPI_VENC_GetRoiCfg(mVeChn, nIndex, pVencRoiCfg))
    {
        ret = UNKNOWN_ERROR;
    }
    return ret;
}

status_t EyeseeRecorder::setVencSuperFrameConfig(VENC_SUPERFRAME_CFG_S *pSuperFrameConfig)
{
    status_t ret = NO_ERROR;
    Mutex::Autolock autoLock(mLock);
    mVencSuperFrameCfg = *pSuperFrameConfig;
    if (mCurrentState & MEDIA_RECORDER_PREPARED || mCurrentState & MEDIA_RECORDER_RECORDING)
    {
        if(SUCCESS != AW_MPI_VENC_SetSuperFrameCfg(mVeChn, &mVencSuperFrameCfg))
        {
            ret = UNKNOWN_ERROR;
        }
    }
    return ret;
}
#endif

status_t EyeseeRecorder::enableAttachAACHeader(bool enable)
{
    mAttachAacHeaderFlag = enable;
    return NO_ERROR;
}

status_t EyeseeRecorder::enableDBRC(bool enable)
{
    mEnableDBRC = enable;
    return NO_ERROR;
}

status_t EyeseeRecorder::GetBufferState(BufferState &state)
{
    if(mEnableDBRC)
    {
        return mpDBRC->GetBufferState(state);
    }
    else
    {
        return NO_INIT;
    }
}

void EyeseeRecorder::notify(MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    if(MOD_ID_AI == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_CAPTURE_AUDIO_DATA:
            {
                AISendDataInfo * pUserData = (AISendDataInfo *)pEventData;
                unsigned int nSize = pUserData->mLen;
                unsigned int nPause = pUserData->mbIgnore;
                if(nPause == 1)
                {
                    mIgnoreAudioBlockNum++;
                    mIgnoreAudioBytes += nSize;
                    int nBitsPerSample = 16;
                    switch(mAioAttr.enBitwidth)
                    {
                        case AUDIO_BIT_WIDTH_8:
                            nBitsPerSample = 8;
                            break;
                        case AUDIO_BIT_WIDTH_16:
                            nBitsPerSample = 16;
                            break;
                        case AUDIO_BIT_WIDTH_24:
                            nBitsPerSample = 24;
                            break;
                        case AUDIO_BIT_WIDTH_32:
                            nBitsPerSample = 32;
                            break;
                        default:
                            nBitsPerSample = 16;
                            break;
                    }
                    int nChannels = mAioAttr.mChnCnt;
                    int nSampleRate = mAioAttr.enSamplerate;
                    mPauseAudioDuration = mIgnoreAudioBytes*1000/((nBitsPerSample/8) * nChannels * nSampleRate);
                }
                else
                {
                    if(-1 == rec_start_timestamp)
                    {
                        rec_start_timestamp = pUserData->mPts;
                    }
                }
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if(MOD_ID_VENC == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                //find VencId
                int VencId = -1;
                for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); it++)
                {
                    VencParameters *pVencParam = it->second;
                    if (pVencParam->getVencChnIndex() == pChn->mChnId)
                    {
                        VencId = it->first;
                        break;
                    }
                }
                //find Vipp
                std::map<int, VI_DEV>::iterator it = mVeVippBindMap.find(VencId);
                if (mVeVippBindMap.end() == it)
                {
                    aloge("fatal error! mVeVippBindMap don't have VencId[%d]!", VencId);
                }
                VI_DEV Vipp = it->second;

                mpInputFrameManager->addReleaseFrame(Vipp, (VIDEO_FRAME_INFO_S*)pEventData);
                break;
            }
            case MPP_EVENT_VENC_TIMEOUT:
            {
                uint64_t framePts = *(uint64_t*)pEventData;
                std::shared_ptr<CMediaMemory> spMem = std::make_shared<CMediaMemory>(sizeof(framePts));
                memcpy(spMem->getPointer(), &framePts, sizeof(framePts));
                postEventFromNative(this, MEDIA_RECORDER_EVENT_ERROR, MEDIA_ERROR_VENC_TIMEOUT, 0, &spMem);
                break;
            }
            case MPP_EVENT_VENC_BUFFER_FULL:
            {
                postEventFromNative(this, MEDIA_RECORDER_EVENT_ERROR, MEDIA_ERROR_VENC_BUFFER_FULL, 0, NULL);
                break;
            }
            /*case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                VencIsp2VeParam *pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                if (NULL == pIsp2VeParam)
                {
                    return;
                }
                //find VencId
                int VencId = -1;
                VencParameters *pVencParam = NULL;
                for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); it++)
                {
                    pVencParam = it->second;
                    if (pVencParam->getVencChnIndex() == pChn->mChnId)
                    {
                        VencId = it->first;
                        break;
                    }
                }
                if (-1 == VencId)
                {
                    aloge("fatal error! venc chn[%d] not find!",pChn->mChnId);
                    return;
                }
                if (!pVencParam->getIspAndVeLinkEnable())
                {
                    return;
                }
                //find Vipp
                std::map<int, VI_DEV>::iterator it = mVeVippBindMap.find(VencId);
                if (mVeVippBindMap.end() == it)
                {
                    aloge("fatal error! mVeVippBindMap don't have VencId[%d]!", VencId);
                    return;
                }
                //find camera proxy
                VI_DEV Vipp = it->second;
                std::map<VI_DEV, CameraRecordingProxy*>::iterator itCRP = mCameraProxyMap.find(Vipp);
                if (mCameraProxyMap.end() == itCRP)
                {
                    alogw("mCameraProxyMap don't have ChnId[%d]", Vipp);
                    return;
                }
                CameraRecordingProxy *pCameraProxy = itCRP->second;
                CameraParameters cameraParam;
                pCameraProxy->getParameters(Vipp, cameraParam);
                enc_VencIsp2VeParam stIsp2Veparam = cameraParam.getIsp2VeParam();
                isp_ae_stats_s stIspAeState = cameraParam.getIspAeState();
                memcpy(&pIsp2VeParam->mIspAeStatus, &stIspAeState, sizeof(stIspAeState));
                if (pVencParam->getEncppEnable())
                {
                    pVencParam->enableEncpp(true);
                    setVencParameters(VencId, pVencParam);
                    if (!pVencParam->getMainStreamFlag())
                    {
                        int nEncppSharpAttenCoefPer = pVencParam->getEncppSharpAttenCoefPer();
                        stIsp2Veparam.mDynamicSharpCfg.ss_blk_stren = stIsp2Veparam.mDynamicSharpCfg.ss_blk_stren * nEncppSharpAttenCoefPer / 100;
                        stIsp2Veparam.mDynamicSharpCfg.ss_wht_stren = stIsp2Veparam.mDynamicSharpCfg.ss_wht_stren * nEncppSharpAttenCoefPer / 100;
                        stIsp2Veparam.mDynamicSharpCfg.ls_blk_stren = stIsp2Veparam.mDynamicSharpCfg.ls_blk_stren * nEncppSharpAttenCoefPer / 100;
                        stIsp2Veparam.mDynamicSharpCfg.ls_wht_stren = stIsp2Veparam.mDynamicSharpCfg.ls_wht_stren * nEncppSharpAttenCoefPer / 100;
                    }
                    sEncppSharpParam *pSharpParam = &pIsp2VeParam->mSharpParam;
                    memcpy(&pSharpParam->mDynamicParam, &stIsp2Veparam.mDynamicSharpCfg,sizeof(sEncppSharpParamDynamic));
                    memcpy(&pSharpParam->mStaticParam, &stIsp2Veparam.mStaticSharpCfg, sizeof(sEncppSharpParamStatic));
                }
                else
                {
                    pVencParam->enableEncpp(false);
                    setVencParameters(VencId, pVencParam);
                }
                pIsp2VeParam->mEnvLv = cameraParam.getEnvLv();
                pIsp2VeParam->mAeWeightLum = cameraParam.getAeWeightLum();
                pIsp2VeParam->mEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                break;
            }
            case MPP_EVENT_LINKAGE_VE2ISP_PARAM:
            {
                VencVe2IspParam *pVe2IspParam = (VencVe2IspParam *)pEventData;
                if (NULL == pVe2IspParam)
                {
                    return;
                }
                //find VencId
                int VencId = -1;
                VencParameters *pVencParam = NULL;
                for (std::map<int, VencParameters*>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); it++)
                {
                    pVencParam = it->second;
                    if (pVencParam->getVencChnIndex() == pChn->mChnId)
                    {
                        VencId = it->first;
                        break;
                    }
                }
                if (-1 == VencId)
                {
                    aloge("fatal error! venc chn[%d] not find!",pChn->mChnId);
                    return;
                }
                if (!pVencParam->getIspAndVeLinkEnable())
                {
                    return;
                }
                //find Vipp
                std::map<int, VI_DEV>::iterator it = mVeVippBindMap.find(VencId);
                if (mVeVippBindMap.end() == it)
                {
                    aloge("fatal error! mVeVippBindMap don't have VencId[%d]!", VencId);
                    return;
                }
                //find camera proxy
                VI_DEV Vipp = it->second;
                std::map<VI_DEV, CameraRecordingProxy*>::iterator itCRP = mCameraProxyMap.find(Vipp);
                if (mCameraProxyMap.end() == itCRP)
                {
                    alogw("mCameraProxyMap don't have ChnId[%d]", Vipp);
                    return;
                }
                CameraRecordingProxy *pCameraProxy = itCRP->second;

                if (pVencParam->getIspAndVeLinkEnable())
                {
                    CameraParameters param;
                    struct enc_VencVe2IspParam stVe2IspParam;
                    memset(&stVe2IspParam, 0, sizeof(stVe2IspParam));
                    stVe2IspParam.d2d_level = pVe2IspParam->d2d_level;
                    stVe2IspParam.d3d_level = pVe2IspParam->d3d_level;
                    memcpy(&stVe2IspParam.mMovingLevelInfo, &pVe2IspParam->mMovingLevelInfo, sizeof(MovingLevelInfo));
                    pCameraProxy->getParameters(Vipp, param);
                    param.setVe2IspParam(stVe2IspParam);
                    //param.setNRAttrValue(pVe2IspParam->d2d_level);
                    //param.set3NRAttrValue(pVe2IspParam->d3d_level);
                    pCameraProxy->setParameters(Vipp, param);
                }
                break;
            }*/
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM_EXTRA:
            {
                VENC_Isp2VeExtraParam *pExtraParam = (VENC_Isp2VeExtraParam *)pEventData;
                pExtraParam->eEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                break;
            }
            case MPP_EVENT_DROP_FRAME:
            {
                alogd("VeChn[%d] receive dropFrame message", pChn->mChnId);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if(MOD_ID_AENC == pChn->mModId)
    {
        alogw("not support notify recorder by AEnc with event(%d)", event);
    }
    else if(MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_NEED_NEXT_FD:
            {
                int muxChn = *(int*)pEventData;
                int muxerId = -1;
                for(int i=0; i<(int)mMuxChns.size(); i++)
                {
                    if(mMuxChns[i] == muxChn)
                    {
                        muxerId = mSinkInfos[i].mMuxerId;
                        break;
                    }
                }
                if(-1 == muxerId)
                {
                    aloge("fatal error! RecorderId[%d] why not find muxerId of muxChn[%d]", mRecorderId, muxChn);
                }
                postEventFromNative(this, MEDIA_RECORDER_EVENT_INFO, MEDIA_RECORDER_INFO_NEED_SET_NEXT_FD, muxerId, NULL);
                break;
            }
            case MPP_EVENT_RECORD_DONE:
            {
                int muxChn = *(int*)pEventData;
                int muxerId = -1;
                for(int i=0; i<(int)mMuxChns.size(); i++)
                {
                    if(mMuxChns[i] == muxChn)
                    {
                        muxerId = mSinkInfos[i].mMuxerId;
                        break;
                    }
                }
                if(-1 == muxerId)
                {
                    aloge("fatal error! RecorderId[%d] why not find muxerId of muxChn[%d]", mRecorderId, muxChn);
                }
                if (mOnInfoListener != NULL)
                {
                    mOnInfoListener->onInfo(this, MEDIA_RECORDER_INFO_RECORD_FILE_DONE_SYNC, muxerId);
                }
                postEventFromNative(this, MEDIA_RECORDER_EVENT_INFO, MEDIA_RECORDER_INFO_RECORD_FILE_DONE, muxerId, NULL);
                break;
            }
            case MPP_EVENT_WRITE_DISK_ERROR:
            {
                int muxChn = *(int*)pEventData;
                int muxerId = -1;
                for(int i=0; i<(int)mMuxChns.size(); i++)
                {
                    if(mMuxChns[i] == muxChn)
                    {
                        muxerId = mSinkInfos[i].mMuxerId;
                        break;
                    }
                }
                if(-1 == muxerId)
                {
                    aloge("fatal error! RecorderId[%d] why not find muxerId of muxChn[%d]", mRecorderId, muxChn);
                }
                postEventFromNative(this, MEDIA_RECORDER_EVENT_ERROR, MEDIA_ERROR_WRITE_DISK_ERROR, muxerId, NULL);
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                status_t ret = pushOneBsFrame((CDXRecorderBsInfo*)pEventData);
                if (ret == NO_ERROR)
                {
                    //judge if need send message.
                    Mutex::Autolock autoLock(mEncBufLock);
                    if(1 == mReadyEncBufList.size())
                    {
                        postEventFromNative(this, MEDIA_RECORDER_VENDOR_EVENT_BSFRAME_AVAILABLE, 0, 0, NULL);
                    }
                }
                else if(ret == NO_MEMORY)
                {
                    //postEventFromNative(this, MEDIA_RECORDER_EVENT_ERROR,
                    //    MPP_EVENT_ERROR_ENCBUFFER_OVERFLOW, 0, NULL);
                    alogv("bsFrame buf queue is full");
                }
                else if(ret == PERMISSION_DENIED)
                {
                    alogv("discard stream");
                }
                else
                {
                    aloge("UNKNOWN ERROR");
                }
                break;
            }
            case MPP_EVENT_RELEASE_VENC_STREAM:
            {
                MUX_VENC_STREAM_S *pMuxVencStream = (MUX_VENC_STREAM_S*)pEventData;
                int nNodeId = pMuxVencStream->mVencStream.mSeq;
                if((nNodeId&0xFFFF0000) == CacheSourcePrefixFlag) //stream is belong to muxCacheManager.
                {
                    mpMuxCacheManager->ReleaseStream(pMuxVencStream->mVencStream, pMuxVencStream->mStreamId);
                }
                else
                {
                    VENC_CHN VeChn = MM_INVALID_CHN;
                    for (std::pair<const int, VencParameters*>& pair : mVencInfoMap)
                    {
                        int nVencId = pair.first;
                        if(pMuxVencStream->mStreamId == nVencId)
                        {
                            VencParameters *pVencParam = pair.second;
                            VeChn = pVencParam->getVencChnIndex();
                            if(VeChn < 0)
                            {
                                aloge("fatal error! RecorderId[%d] vencId[%d] has wrong veChn[%d]", mRecorderId, nVencId, VeChn);
                            }
                            break;
                        }
                    }
                    if(VeChn >= 0)
                    {
                        ReleaseVideoStreamToVencChn(&pMuxVencStream->mVencStream, pMuxVencStream->mStreamId, VeChn);
                    }
                    else
                    {
                        aloge("fatal error! RecorderId[%d] muxChn[%d] release stream, why veChn[%d] invalid?", mRecorderId, pChn->mChnId, VeChn);
                    }
                }
                break;
            }
            case MPP_EVENT_RELEASE_AENC_STREAM:
            {
                MUX_AENC_STREAM_S *pMuxAencStream = (MUX_AENC_STREAM_S*)pEventData;
                int nNodeId = pMuxAencStream->mAencStream.mId;
                if((nNodeId&0xFFFF0000) == CacheSourcePrefixFlag) //stream is belong to muxCacheManager.
                {
                    mpMuxCacheManager->ReleaseStream(pMuxAencStream->mAencStream, pMuxAencStream->mStreamId);
                }
                else
                {
                    if(mAeChn >= 0)
                    {
                        ReleaseAudioStreamToAencChn(&pMuxAencStream->mAencStream, pMuxAencStream->mStreamId, mAeChn);
                    }
                    else
                    {
                        aloge("fatal error! RecorderId[%d] muxChn[%d] release stream, why aeChn[%d] invalid?", mRecorderId, pChn->mChnId, mAeChn);
                    }
                }
                break;
            }
            case MPP_EVENT_RELEASE_TENC_STREAM:
            {
                MUX_TENC_STREAM_S *pMuxTencStream = (MUX_TENC_STREAM_S*)pEventData;
                int nNodeId = pMuxTencStream->mTencStream.mId;
                if((nNodeId&0xFFFF0000) == CacheSourcePrefixFlag) //stream is belong to muxCacheManager.
                {
                    mpMuxCacheManager->ReleaseStream(pMuxTencStream->mTencStream, pMuxTencStream->mStreamId);
                }
                else
                {
                    if(mTeChn >= 0)
                    {
                        ReleaseTextStreamToTencChn(&pMuxTencStream->mTencStream, pMuxTencStream->mStreamId, mTeChn);
                    }
                    else
                    {
                        aloge("fatal error! RecorderId[%d] muxChn[%d] release stream, why teChn[%d] invalid?", mRecorderId, pChn->mChnId, mTeChn);
                    }
                }
                break;
            }
            case MPP_EVENT_MUX_FORCE_I_FRAME:
            {
                int nStreamId = *(int*)pEventData; //streamId == vencId
                VENC_CHN VeChn = MM_INVALID_CHN;
                for (std::pair<const int, VencParameters*>& pair : mVencInfoMap)
                {
                    int nVencId = pair.first;
                    if(nStreamId == nVencId)
                    {
                        VencParameters *pVencParam = pair.second;
                        VeChn = pVencParam->getVencChnIndex();
                        if(VeChn < 0)
                        {
                            aloge("fatal error! RecorderId[%d] muxChn[%d] force I frame, vencId[%d] has wrong veChn[%d]", mRecorderId, pChn->mChnId, nVencId, VeChn);
                        }
                        break;
                    }
                }
                if(VeChn >= 0)
                {
                    AW_MPI_VENC_RequestIDR(VeChn, TRUE);
                }
                else
                {
                    aloge("fatal error! RecorderId[%d] muxChn[%d] force I frame, why veChn[%d] invalid?", mRecorderId, pChn->mChnId, VeChn);
                }
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! RecorderId[%d] unknown event[0x%x] from channel[%d-%d-%d]!", mRecorderId, event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! need implement!");
    }
}

void EyeseeRecorder::doCleanUp()
{
    mIsAudioSourceSet  = false;
    mIsVideoSourceSet  = false;
    mIsAudioEncoderSet = false;
    mIsVideoEncoderSet = false;
    mIsOutputFileSet   = false;
    mMuxerIdCounter = 0;

    mIgnoreAudioBlockNum = 0;
    mIgnoreAudioBytes = 0;
    mPauseAudioDuration = 0;

    mAudioEncoder = PT_MAX;
    mTimeLapseEnable = false;
    mAttachAacHeaderFlag = false;

    mMuteMode = false;
    mAudioBitRate = 0;
    mAudioStreamId = -1;
    mTextStreamId = -1;
#if 0
    mVideoEncoder = PT_MAX;
    mVideoRCMode = VideoRCMode_CBR;
    mVideoPDMode = VideoEncodeProductMode::NORMAL_MODE;
    mSensorType = VENC_ST_DEFAULT;
    mVEncRcAttr = {
        .mVEncType = PT_H264,
        .mRcMode = VideoRCMode_CBR,
        {
            .mAttrH264Cbr = {
                .mBitRate = 1*1024*1024,
                .mMaxQp = 51,
                .mMinQp = 1,
            },
        },
    };
    //mVideoBitRate = 0;
    //mVideoEncodingBufferTime = 0;
    mIQpOffset = 0;
    mVEncAttr = {
        .mType = PT_H264,
        .mBufSize = 0,
        .mThreshSize = 0,
        {
            .mAttrH264 = {
                .mProfile = 1,
                .mLevel = H264_LEVEL_51,
            },
        },
    };
    //mbLongTermRef = true;
    mNullSkipEnable = false;
    mPSkipEnable = false;
    mFastEncFlag = false;
    //mbPIntraEnable = true;
    mb3DNR = 0;
    mbAdaptiveintrainp = false;
    mVencSuperFrameCfg = {
        .enSuperFrmMode = SUPERFRM_NONE,
        .SuperIFrmBitsThr = 0,
        .SuperPFrmBitsThr = 0,
        .SuperBFrmBitsThr = 0,
    };
    mbColor2Grey = false;
    mbHorizonfilp = false;

    memset(&mVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&mVEncRefParam, 0, sizeof(VENC_PARAM_REF_S));
    memset(&mSmartPParam, 0, sizeof(VencSmartFun));
    memset(&mIntraRefreshParam, 0, sizeof(VENC_PARAM_INTRA_REFRESH_S));
    memset(&mSaveBSFileParam, 0, sizeof(mSaveBSFileParam));
    memset(&mVeProcSet, 0, sizeof(mVeProcSet));
#endif
    Mutex::Autolock lock(mRgnLock);
    ERRORTYPE ret;
    size_t num = mRgnHandleList.size();
    if(num > 0)
    {
        alogd("Be careful! There are [%d]regions need to destroy!", num);
    }
    for(std::list<RGN_HANDLE>::iterator it = mRgnHandleList.begin(); it != mRgnHandleList.end();)
    {
        ret = AW_MPI_RGN_Destroy(*it);
        if(SUCCESS == ret)
        {
            it = mRgnHandleList.erase(it);
        }
        else
        {
            aloge("fatal error! destroy region[%d] fail!", *it);
            ++it;
        }
    }
    if(mpMuxCacheManager)
    {
        delete mpMuxCacheManager;
        mpMuxCacheManager = NULL;
    }
}

void EyeseeRecorder::postEventFromNative(EyeseeRecorder *pRecorder, int what, int arg1, int arg2, const std::shared_ptr<CMediaMemory>* pDataPtr)
{
    if (pRecorder == NULL)
    {
        aloge("fatal error! pRecorder == NULL");
        return;
    }

    if (pRecorder->mEventHandler != NULL)
    {
        CallbackMessage msg;
        msg.what = what;
        msg.arg1 = arg1;
        msg.arg2 = arg2;
        if(pDataPtr)
        {
            msg.mDataPtr = std::const_pointer_cast<const CMediaMemory>(*pDataPtr);
        }
        pRecorder->mEventHandler->post(msg);
    }
}

status_t EyeseeRecorder::config_AIO_ATTR_S()
{
    memset(&mAioAttr, 0, sizeof(AIO_ATTR_S));
    mAioAttr.enBitwidth = AUDIO_BIT_WIDTH_16;

    if ((mSampleRate!=AUDIO_SAMPLE_RATE_8000 ) &&
        (mSampleRate!=AUDIO_SAMPLE_RATE_12000) &&
        (mSampleRate!=AUDIO_SAMPLE_RATE_11025) &&
        (mSampleRate!=AUDIO_SAMPLE_RATE_16000) &&
        (mSampleRate!=AUDIO_SAMPLE_RATE_22050) &&
        (mSampleRate!=AUDIO_SAMPLE_RATE_24000) &&
        (mSampleRate!=AUDIO_SAMPLE_RATE_32000) &&
        (mSampleRate!=AUDIO_SAMPLE_RATE_44100) &&
        (mSampleRate!=AUDIO_SAMPLE_RATE_48000) )
    {
        alogw("wrong audio SampleRate(%d) setting, change to default(8000)!", mSampleRate);
        mSampleRate = 8000;
    }
    mAioAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)mSampleRate;

    if ((mAudioChannels!=1) && (mAudioChannels!=2))
    {
        alogw("wrong audio TrackCnt(%d) setting, change to default(1)!", mAudioChannels);
        mAudioChannels = 1;
    }
    mAioAttr.mChnCnt = mAudioChannels;
    mAioAttr.enSoundmode = (mAudioChannels==1)?AUDIO_SOUND_MODE_MONO:AUDIO_SOUND_MODE_STEREO;
    alogd("AIO_Attr ==>> SampleRate:%d, TrackCnt:%d", mAioAttr.enSamplerate, mAioAttr.mChnCnt);

    return NO_ERROR;
}

status_t EyeseeRecorder::config_AENC_CHN_ATTR_S()
{
    memset(&mAEncChnAttr, 0, sizeof(AENC_CHN_ATTR_S));
    if((PT_AAC==mAudioEncoder) ||
       (PT_PCM_AUDIO==mAudioEncoder) ||
       (PT_LPCM==mAudioEncoder) ||
       (PT_ADPCMA==mAudioEncoder) ||
       (PT_MP3==mAudioEncoder) ||
       (PT_G726==mAudioEncoder)
       || (PT_G726U==mAudioEncoder)
       || (PT_G711A==mAudioEncoder) ||
       (PT_G711U==mAudioEncoder))
    {
        mAEncChnAttr.AeAttr.Type = mAudioEncoder;
        mAEncChnAttr.AeAttr.sampleRate = mSampleRate;
        mAEncChnAttr.AeAttr.channels = mAudioChannels;
        mAEncChnAttr.AeAttr.bitRate = mAudioBitRate;
        mAEncChnAttr.AeAttr.bitsPerSample = 16;
        mAEncChnAttr.AeAttr.attachAACHeader = (int)mAttachAacHeaderFlag;
        //mAEncChnAttr.AeAttr.mOutBufCnt = mSampleRate*4/1024;
    }
    else
    {
        aloge("unsupported audio encoder formate(%d) temporaryly", mAudioEncoder);
    }
    alogd("AEnc_Attr ==>> Type:%d, SampleRate:%d, TrackCnt:%d, AttachAacHeader:%d, mOutBufCnt:%d",
        mAudioEncoder, mSampleRate, mAudioChannels, mAttachAacHeaderFlag, mAEncChnAttr.AeAttr.mOutBufCnt);

    return NO_ERROR;
}

status_t EyeseeRecorder::config_TENC_CHN_ATTR_S()
{
    memset(&mTEncChnAttr, 0, sizeof(TENC_CHN_ATTR_S));
    mTEncChnAttr.tInfo.enc_enable_type |= 1<<0; // just enable gps enc

    return NO_ERROR;
}

status_t EyeseeRecorder::gpsInfoEn(int gps_en)
{
    gps_state = gps_en;

    memset(&mTEncChnAttr, 0, sizeof(TENC_CHN_ATTR_S));
    if(gps_state)
    {
        mTEncChnAttr.tInfo.enc_enable_type |= 1<<0; // just enable gps enc
    }

    return NO_ERROR;
}

status_t EyeseeRecorder::gpsInfoSend(void *gps_info)
{
#if (MPPCFG_TEXTENC!=0)
    TEXT_FRAME_S text_frm;

    if(NULL!=gps_info && gps_state)
    {
        memset(&text_frm,0,sizeof(TEXT_FRAME_S));

        memcpy((void *)text_frm.mpAddr,gps_info,sizeof(RMCINFO));
        text_frm.mLen = sizeof(RMCINFO);
        text_frm.mTimeStamp = CDX_GetSysTimeUsMonotonic();
        text_frm.mId = 0;

        ERRORTYPE ret = AW_MPI_TENC_SendFrame(mTeChn, &text_frm);
        if(SUCCESS != ret)
        {
            aloge("send_text_frame_fail");
        }
    }
    return NO_ERROR;
#else
    alogw("textenc is disable!");
    return INVALID_OPERATION;
#endif

}

status_t EyeseeRecorder::config_VENC_CHN_ATTR_S(int VencId)
{
    //memset(&mVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(VencId);
    if (mVencInfoMap.end() == it)
    {
        aloge("fatal error! mVencInfoMap don't have VencId[%d]", VencId);
        return BAD_VALUE;
    }
    VencParameters *pVencParam = it->second;

    PAYLOAD_TYPE_E VideoEncoder = pVencParam->getVideoEncoder();
    VENC_CHN_ATTR_S stVEncChnAttr = pVencParam->getVencChnAttr();
    VENC_RC_PARAM_S stVEncRcParam = pVencParam->getVencRcParam();

    // online
    //stVEncChnAttr.VeAttr.mOnlineEnable = pVencParam->getOnlineEnable();
    //stVEncChnAttr.VeAttr.mOnlineShareBufNum = pVencParam->getOnlineShareBufNum();
    
    //stVEncChnAttr.VeAttr.Type = VideoEncoder;
#if 0
    if(NULL == mpCameraProxy)
    {
        aloge("fatal error! camera is null!");
    }
#endif
    CameraParameters param;
    std::map<int, VI_DEV>::iterator itVipp = mVeVippBindMap.find(VencId);
    if (mVeVippBindMap.end() == itVipp)
    {
        alogw("mVeVippBindMap don't have VencId[%d]", VencId);
    }
    VI_DEV Vipp = itVipp->second;

    std::map<VI_DEV, CameraRecordingProxy*>::iterator itCRP = mCameraProxyMap.find(Vipp);
    if (mCameraProxyMap.end() == itCRP)
    {
        alogw("mCameraProxyMap don't have ChnId[%d]", Vipp);
    }
    CameraRecordingProxy *pCameraProxy = itCRP->second;

    pCameraProxy->getParameters(Vipp, param);

    //int VideoMaxKeyItl = pVencParam->getVideoEncodingIFramesNumberInterVal();
    //stVEncChnAttr.VeAttr.MaxKeyInterval = VideoMaxKeyItl;

    SIZE_S stframeBufSize;
    param.getVideoBufSizeOut(stframeBufSize);
    stVEncChnAttr.VeAttr.SrcPicWidth = stframeBufSize.Width;
    stVEncChnAttr.VeAttr.SrcPicHeight = stframeBufSize.Height;
    stVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    stVEncChnAttr.VeAttr.PixelFormat = param.getPreviewFormat();
    stVEncChnAttr.VeAttr.mColorSpace = param.getColorSpace();
    VENC_FRAME_RATE_S stFrameRate;
    GetFrameRateFromVENC_CHN_ATTR_S(&stVEncChnAttr, &stFrameRate);
    stFrameRate.SrcFrmRate = param.getPreviewFrameRate();
    SetFrameRateToVENC_CHN_ATTR_S(&stFrameRate, &stVEncChnAttr);
    //stVEncChnAttr.EncppAttr.mbEncppEnable = (BOOL)param.getEncppEnable();

//    VencParameters::VEncAttr stVEncAttr = pVencParam->getVEncAttr();
//    if(VideoEncoder != stVEncAttr.mType)
//    {
//        aloge("fatal error! vencType is not match: [0x%x]!=[0x%x]!", VideoEncoder, stVEncAttr.mType);
//    }

//    SIZE_S stVideoSize;
//    pVencParam->getVideoSize(stVideoSize);
//    alogd("VencId[%d], Video.Width: %d, Video.Height: %d", VencId, stVideoSize.Width, stVideoSize.Height);
//    int nIQpOffset = pVencParam->getIQpOffset();
//    bool nFastEncFlag = pVencParam->getFastEncodeFlag();
//    bool nbPIntraEnable;
//    nbPIntraEnable = pVencParam->getVideoEncodingPIntraFlag();
/*
    if(PT_H264 == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrH264e.BufSize = stVEncAttr.mBufSize;
        stVEncChnAttr.VeAttr.AttrH264e.mThreshSize = stVEncAttr.mThreshSize;
        stVEncChnAttr.VeAttr.AttrH264e.Profile = stVEncAttr.mAttrH264.mProfile;
        stVEncChnAttr.VeAttr.AttrH264e.mLevel = stVEncAttr.mAttrH264.mLevel;
        stVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        stVEncChnAttr.VeAttr.AttrH264e.PicWidth = stVideoSize.Width;
        stVEncChnAttr.VeAttr.AttrH264e.PicHeight = stVideoSize.Height;
        stVEncChnAttr.VeAttr.AttrH264e.IQpOffset = nIQpOffset;
       // stVEncChnAttr.VeAttr.AttrH264e.mbLongTermRef = mbLongTermRef;
        stVEncChnAttr.VeAttr.AttrH264e.FastEncFlag = nFastEncFlag;
       // stVEncChnAttr.VeAttr.AttrH264e.mVirtualIFrameInterval = mVirtualIFrameInterval;
        stVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = nbPIntraEnable;
        alogd("config venc bufSize[%d], threshSize[%d]", stVEncChnAttr.VeAttr.AttrH264e.BufSize, stVEncChnAttr.VeAttr.AttrH264e.mThreshSize);
    }
    else if(PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrH265e.mBufSize = stVEncAttr.mBufSize;
        stVEncChnAttr.VeAttr.AttrH265e.mThreshSize = stVEncAttr.mThreshSize;
        stVEncChnAttr.VeAttr.AttrH265e.mProfile = stVEncAttr.mAttrH265.mProfile;
        stVEncChnAttr.VeAttr.AttrH265e.mLevel = stVEncAttr.mAttrH265.mLevel;
        stVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        stVEncChnAttr.VeAttr.AttrH265e.mPicWidth = stVideoSize.Width;
        stVEncChnAttr.VeAttr.AttrH265e.mPicHeight = stVideoSize.Height;
        stVEncChnAttr.VeAttr.AttrH265e.IQpOffset = nIQpOffset;
        //stVEncChnAttr.VeAttr.AttrH265e.mbLongTermRef = mbLongTermRef;
        stVEncChnAttr.VeAttr.AttrH265e.mFastEncFlag = nFastEncFlag;
        //stVEncChnAttr.VeAttr.AttrH265e.mVirtualIFrameInterval = mVirtualIFrameInterval;
        stVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = nbPIntraEnable;
    }
    else if(PT_MJPEG == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrMjpeg.mBufSize = stVEncAttr.mBufSize;
        stVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        stVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth = stVideoSize.Width;
        stVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = stVideoSize.Height;
    }
    else
    {
        aloge("fatal error! unsupported temporary");
    }
*/
    /*
    VencParameters::VEncBitRateControlAttr stVEncRcAttr;
    stVEncRcAttr = pVencParam->getVEncBitRateControlAttr();
    VencParameters::VideoEncodeRateControlMode VideoRCMode = stVEncRcAttr.mRcMode;
    VencParameters::VideoEncodeProductMode VideoPDMode;
    VideoPDMode = pVencParam->getVideoEncodingProductMode();
    eSensorType SensorType = pVencParam->getSensorType();

//    if(VideoRCMode != stVEncRcAttr.mRcMode)
//    {
//        aloge("fatal error! why rcMode[0x%x]!=[0x%x], check code!", VideoRCMode, stVEncRcAttr.mRcMode);
//    }
    if(PT_H264 == stVEncChnAttr.VeAttr.Type)
    {
        switch(VideoPDMode)
        {
            case VencParameters::VideoEncodeProductMode::NORMAL_MODE:
            {
                stVEncRcParam.product_mode = VENC_PRODUCT_NORMAL_MODE;
                break;
            }
            case VencParameters::VideoEncodeProductMode::IPC_MODE:
            {
                stVEncRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
                break;
            }
            default:
            {
                stVEncRcParam.product_mode = VENC_PRODUCT_NORMAL_MODE;
                aloge("eye_rc_h264_use_default_product_mode");
                break;
            }
        }
        stVEncRcParam.sensor_type = (unsigned int)SensorType;
        switch(VideoRCMode)
        {
            case VencParameters::VideoRCMode_CBR:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                break;
            case VencParameters::VideoRCMode_VBR:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
                break;
            case VencParameters::VideoRCMode_FIXQP:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
                break;
            case VencParameters::VideoRCMode_ABR:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
                break;
            case VencParameters::VideoRCMode_QPMAP:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264QPMAP;
                break;
            default:
                aloge("fatal error! unknown rcMode[%d]", VideoRCMode);
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                break;
        }
        if(VENC_RC_MODE_H264CBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate = stVEncRcAttr.mAttrH264Cbr.mBitRate;
            stVEncRcParam.ParamH264Cbr.mMaxQp = stVEncRcAttr.mAttrH264Cbr.mMaxQp;
            stVEncRcParam.ParamH264Cbr.mMinQp = stVEncRcAttr.mAttrH264Cbr.mMinQp;
            stVEncRcParam.ParamH264Cbr.mMaxPqp = stVEncRcAttr.mAttrH264Cbr.mMaxPqp;
            stVEncRcParam.ParamH264Cbr.mMinPqp = stVEncRcAttr.mAttrH264Cbr.mMinPqp;
            stVEncRcParam.ParamH264Cbr.mQpInit = stVEncRcAttr.mAttrH264Cbr.mQpInit;
            stVEncRcParam.ParamH264Cbr.mbEnMbQpLimit = stVEncRcAttr.mAttrH264Cbr.mbEnMbQpLimit;
        }
        else if(VENC_RC_MODE_H264VBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = stVEncRcAttr.mAttrH264Vbr.mMaxBitRate;
            stVEncRcParam.ParamH264Vbr.mMaxQp = stVEncRcAttr.mAttrH264Vbr.mMaxQp;
            stVEncRcParam.ParamH264Vbr.mMinQp = stVEncRcAttr.mAttrH264Vbr.mMinQp;
            stVEncRcParam.ParamH264Vbr.mMaxPqp = stVEncRcAttr.mAttrH264Vbr.mMaxPqp;
            stVEncRcParam.ParamH264Vbr.mMinPqp = stVEncRcAttr.mAttrH264Vbr.mMinPqp;
            stVEncRcParam.ParamH264Vbr.mQpInit = stVEncRcAttr.mAttrH264Vbr.mQpInit;
            stVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = stVEncRcAttr.mAttrH264Vbr.mbEnMbQpLimit;
            stVEncRcParam.ParamH264Vbr.mMovingTh = stVEncRcAttr.mAttrH264Vbr.mMovingTh;
            stVEncRcParam.ParamH264Vbr.mQuality = stVEncRcAttr.mAttrH264Vbr.mQuality;
            stVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = stVEncRcAttr.mAttrH264Vbr.mIFrmBitsCoef;
            stVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = stVEncRcAttr.mAttrH264Vbr.mPFrmBitsCoef;
        }
        else if(VENC_RC_MODE_H264FIXQP == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH264FixQp.mIQp = stVEncRcAttr.mAttrH264FixQp.mIQp;
            stVEncChnAttr.RcAttr.mAttrH264FixQp.mPQp = stVEncRcAttr.mAttrH264FixQp.mPQp;
        }
        else if(VENC_RC_MODE_H264ABR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH264Abr.mMaxBitRate = stVEncRcAttr.mAttrH264Abr.mMaxBitRate;
            stVEncChnAttr.RcAttr.mAttrH264Abr.mRatioChangeQp = stVEncRcAttr.mAttrH264Abr.mRatioChangeQp;
            stVEncChnAttr.RcAttr.mAttrH264Abr.mQuality = stVEncRcAttr.mAttrH264Abr.mQuality;
            stVEncChnAttr.RcAttr.mAttrH264Abr.mMinIQp = stVEncRcAttr.mAttrH264Abr.mMinIQp;
            stVEncChnAttr.RcAttr.mAttrH264Abr.mMaxIQp = stVEncRcAttr.mAttrH264Abr.mMaxIQp;
            stVEncChnAttr.RcAttr.mAttrH264Abr.mMaxQp = stVEncRcAttr.mAttrH264Abr.mMaxQp;
            stVEncChnAttr.RcAttr.mAttrH264Abr.mMinQp = stVEncRcAttr.mAttrH264Abr.mMinQp;
        }
        else if(VENC_RC_MODE_H264QPMAP == stVEncChnAttr.RcAttr.mRcMode)
        {
            aloge("QPMap not support now!");
        }
        else
        {
            aloge("fatal error! unknown rcMode[%d]", stVEncChnAttr.RcAttr.mRcMode);
        }
    }
    else if(PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
        switch(VideoPDMode)
        {
            case VencParameters::VideoEncodeProductMode::NORMAL_MODE:
            {
                stVEncRcParam.product_mode = VENC_PRODUCT_NORMAL_MODE;
                break;
            }
            case VencParameters::VideoEncodeProductMode::IPC_MODE:
            {
                stVEncRcParam.product_mode = VENC_PRODUCT_IPC_MODE;
                break;
            }
            default:
            {
                stVEncRcParam.product_mode = VENC_PRODUCT_NORMAL_MODE;
                aloge("eye_rc_h265_use_default_product_mode");
                break;
            }
        }
        stVEncRcParam.sensor_type = (unsigned int)SensorType;
        switch(VideoRCMode)
        {
            case VencParameters::VideoRCMode_CBR:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                break;
            case VencParameters::VideoRCMode_VBR:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
                break;
            case VencParameters::VideoRCMode_FIXQP:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
                break;
            case VencParameters::VideoRCMode_ABR:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
                break;
            case VencParameters::VideoRCMode_QPMAP:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265QPMAP;
                break;
            default:
                aloge("fatal error! unknown rcMode[%d]", VideoRCMode);
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                break;
        }
        if(VENC_RC_MODE_H265CBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate = stVEncRcAttr.mAttrH265Cbr.mBitRate;
            stVEncRcParam.ParamH265Cbr.mMaxQp = stVEncRcAttr.mAttrH265Cbr.mMaxQp;
            stVEncRcParam.ParamH265Cbr.mMinQp = stVEncRcAttr.mAttrH265Cbr.mMinQp;
            stVEncRcParam.ParamH265Cbr.mMaxPqp = stVEncRcAttr.mAttrH265Cbr.mMaxPqp;
            stVEncRcParam.ParamH265Cbr.mMinPqp = stVEncRcAttr.mAttrH265Cbr.mMinPqp;
            stVEncRcParam.ParamH265Cbr.mQpInit = stVEncRcAttr.mAttrH265Cbr.mQpInit;
            stVEncRcParam.ParamH265Cbr.mbEnMbQpLimit = stVEncRcAttr.mAttrH265Cbr.mbEnMbQpLimit;
        }
        else if(VENC_RC_MODE_H265VBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = stVEncRcAttr.mAttrH265Vbr.mMaxBitRate;
            stVEncRcParam.ParamH265Vbr.mMaxPqp = stVEncRcAttr.mAttrH265Vbr.mMaxPqp;
            stVEncRcParam.ParamH265Vbr.mMinPqp = stVEncRcAttr.mAttrH265Vbr.mMinPqp;
            stVEncRcParam.ParamH265Vbr.mQpInit = stVEncRcAttr.mAttrH265Vbr.mQpInit;
            stVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = stVEncRcAttr.mAttrH265Vbr.mbEnMbQpLimit;
            stVEncRcParam.ParamH265Vbr.mMovingTh = stVEncRcAttr.mAttrH265Vbr.mMovingTh;
            stVEncRcParam.ParamH265Vbr.mQuality = stVEncRcAttr.mAttrH265Vbr.mQuality;
            stVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = stVEncRcAttr.mAttrH265Vbr.mIFrmBitsCoef;
            stVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = stVEncRcAttr.mAttrH265Vbr.mPFrmBitsCoef;
        }
        else if(VENC_RC_MODE_H265FIXQP == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH265FixQp.mIQp = stVEncRcAttr.mAttrH265FixQp.mIQp;
            stVEncChnAttr.RcAttr.mAttrH265FixQp.mPQp = stVEncRcAttr.mAttrH265FixQp.mPQp;
        }
        else if(VENC_RC_MODE_H265ABR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH265Abr.mMaxBitRate = stVEncRcAttr.mAttrH265Abr.mMaxBitRate;
            stVEncChnAttr.RcAttr.mAttrH265Abr.mRatioChangeQp = stVEncRcAttr.mAttrH265Abr.mRatioChangeQp;
            stVEncChnAttr.RcAttr.mAttrH265Abr.mQuality = stVEncRcAttr.mAttrH265Abr.mQuality;
            stVEncChnAttr.RcAttr.mAttrH265Abr.mMinIQp = stVEncRcAttr.mAttrH265Abr.mMinIQp;
            stVEncChnAttr.RcAttr.mAttrH265Abr.mMaxIQp = stVEncRcAttr.mAttrH265Abr.mMaxIQp;
            stVEncChnAttr.RcAttr.mAttrH265Abr.mMaxQp = stVEncRcAttr.mAttrH265Abr.mMaxQp;
            stVEncChnAttr.RcAttr.mAttrH265Abr.mMinQp = stVEncRcAttr.mAttrH265Abr.mMinQp;
        }
        else if(VENC_RC_MODE_H265QPMAP == stVEncChnAttr.RcAttr.mRcMode)
        {
            aloge("QPMap not support now!");
        }
        else
        {
            aloge("fatal error! unknown rcMode[%d]", stVEncChnAttr.RcAttr.mRcMode);
        }
    }
    else if(PT_MJPEG == stVEncChnAttr.VeAttr.Type)
    {
        switch(VideoRCMode)
        {
            case VencParameters::VideoRCMode_CBR:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
                break;
            case VencParameters::VideoRCMode_FIXQP:
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
                break;
            default:
                aloge("fatal error! unknown rcMode[%d]", VideoRCMode);
                stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
                break;
        }
        if(VENC_RC_MODE_MJPEGCBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = stVEncRcAttr.mAttrMjpegCbr.mBitRate;
        }
        else if(VENC_RC_MODE_MJPEGFIXQP == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = stVEncRcAttr.mAttrMjpegFixQp.mQfactor;
        }
        else
        {
            aloge("fatal error! unknown rcMode[%d]", stVEncChnAttr.RcAttr.mRcMode);
        }
    }
    else
    {
        aloge("fatal error! unsupported temporary");
    }
    */
    pVencParam->setVencChnAttr(stVEncChnAttr);
    pVencParam->setVencRcParam(stVEncRcParam);

    //set BufSize
//    unsigned int nBufSize = 0;
//    unsigned int nThreshSize = mVideoWidth*mVideoHeight;
//    int nBitRate = GetBitRateFromVENC_CHN_ATTR_S(&mVEncChnAttr);
//    if(mVideoEncodingBufferTime > 0)
//    {
//        nBufSize = (unsigned int)((uint64_t)nBitRate*mVideoEncodingBufferTime/(8*1000))+ nThreshSize;
//    }
//    if(PT_H264 == mVEncChnAttr.VeAttr.Type)
//    {
//        mVEncChnAttr.VeAttr.AttrH264e.BufSize = nBufSize;
//    }
//    else if(PT_H265 == mVEncChnAttr.VeAttr.Type)
//    {
//        mVEncChnAttr.VeAttr.AttrH265e.mBufSize = nBufSize;
//    }
//    else if(PT_MJPEG == mVEncChnAttr.VeAttr.Type)
//    {
//        mVEncChnAttr.VeAttr.AttrMjpeg.mBufSize = nBufSize;
//    }
//    else
//    {
//        aloge("fatal error! unsupported temporary");
//    }

    return NO_ERROR;
}

/**
  config mux channel attr.

  @param[out] pMuxChnAttr
  @param[in] pSinkInfo
*/
status_t EyeseeRecorder::config_MUX_CHN_ATTR_S(MUX_CHN_ATTR_S *pMuxChnAttr, OutputSinkInfo *pSinkInfo)
{
    memset(pMuxChnAttr, 0, sizeof(MUX_CHN_ATTR_S));
    int VideoInfoIndex = 0;
    for (VideoInfoIndex = 0; VideoInfoIndex < MAX_VIDEO_TRACK_COUNT; VideoInfoIndex++)
    {
        pMuxChnAttr->mVideoAttr[VideoInfoIndex].mVideoEncodeType = PT_MAX;
        pMuxChnAttr->mVideoAttr[VideoInfoIndex].mVeChn = MM_INVALID_CHN;
    }
    pMuxChnAttr->mAudioEncodeType = PT_MAX;
    pMuxChnAttr->mTextEncodeType = PT_MAX;
    VideoInfoIndex = 0;
    for (std::map<int, VencParameters *>::iterator it = mVencInfoMap.begin(); it != mVencInfoMap.end(); ++it)
    {
        int nVencId = it->first;
        if(VideoInfoIndex!=nVencId)
        {
            aloge("fatal error! muxChn->mVideoAttr suffix[%d] != vencId[%d]", VideoInfoIndex, nVencId);
        }
        VencParameters *pVencParam = it->second;
        VENC_CHN VeChn = pVencParam->getVencChnIndex();
        if(VeChn >= 0)
        {
            PAYLOAD_TYPE_E VideoEncoder = pVencParam->getVideoEncoder();
            pMuxChnAttr->mVideoAttr[VideoInfoIndex].mVideoEncodeType = VideoEncoder;
            int FrameRate = pVencParam->getVideoFrameRate();
            if(pMuxChnAttr->mVideoAttr[VideoInfoIndex].mVideoEncodeType != PT_MAX)
            {
                SIZE_S stVideoSize;
                pVencParam->getVideoSize(stVideoSize);
                pMuxChnAttr->mVideoAttr[VideoInfoIndex].mWidth = stVideoSize.Width;
                pMuxChnAttr->mVideoAttr[VideoInfoIndex].mHeight = stVideoSize.Height;

                CameraParameters param;
                std::map<int, VI_DEV>::iterator itVipp = mVeVippBindMap.find(it->first);
                if (mVeVippBindMap.end() == itVipp)
                {
                    aloge("fatal error! mVeVippBindMap don't have VencId[%d]", it->first);
                }
                VI_DEV Vipp = itVipp->second;
                std::map<VI_DEV, CameraRecordingProxy*>::iterator itCRP = mCameraProxyMap.find(Vipp);
                if (mCameraProxyMap.end() == itCRP)
                {
                    aloge("fatal error! mCameraProxyMap don't have vipp[%d]", Vipp);
                }
                CameraRecordingProxy *pCameraProxy = itCRP->second;
                pCameraProxy->getParameters(Vipp, param);

                pMuxChnAttr->mVideoAttr[VideoInfoIndex].mVideoFrmRate = FrameRate*1000; //param.getPreviewFrameRate()*1000;
                pMuxChnAttr->mVideoAttr[VideoInfoIndex].mMaxKeyInterval = pVencParam->getVideoEncodingIFramesNumberInterVal(); //param.getPreviewFrameRate();
                pMuxChnAttr->mVideoAttr[VideoInfoIndex].mVeChn = VeChn;
            }
            else
            {
                aloge("fatal error! why veChn create when vencType is not known?");
            }
        }
        else
        {
            aloge("fatal error! why not create VeChn for VencId[%d]?", it->first);
        }
        VideoInfoIndex++;
    }
    pMuxChnAttr->mVideoAttrValidNum = VideoInfoIndex;
    if(mAeChn >= 0)
    {
        pMuxChnAttr->mAudioEncodeType = mAudioEncoder;
        if(pMuxChnAttr->mAudioEncodeType != PT_MAX)
        {
            pMuxChnAttr->mChannels = mAudioChannels;
            pMuxChnAttr->mBitsPerSample = 16;
            pMuxChnAttr->mSamplesPerFrame = MAXDECODESAMPLE;
            pMuxChnAttr->mSampleRate = mSampleRate;
        }
        else
        {
            aloge("fatal error! why aeChn create when aencType is not known?");
        }
    }
    if(mTeChn >= 0)
    {
        pMuxChnAttr->mTextEncodeType = PT_TEXT;
    }

    //pMuxChnAttr->mMuxerId = pSinkInfo->mMuxerId;
    pMuxChnAttr->mMediaFileFormat = pSinkInfo->mOutputFormat;
    pMuxChnAttr->mMaxFileDuration = pSinkInfo->mMaxDurationMs>0?pSinkInfo->mMaxDurationMs:mMaxFileDuration;
    pMuxChnAttr->mMaxFileSizeBytes = mMaxFileSizeBytes;
    //muxChnAttr.mFallocateLen = sinkInfo.mFallocateLen;
    pMuxChnAttr->mCallbackOutFlag = pSinkInfo->mCallbackOutFlag;
    pMuxChnAttr->mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pMuxChnAttr->mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
    //muxChnAttr.bBufFromCacheFlag = pSinkParam->bBufFromCacheFlag;
    pMuxChnAttr->mAddRepairInfo = pSinkInfo->mbAddRepairInfo;
    return NO_ERROR;
}

status_t EyeseeRecorder::getEncDataHeader(int VencId, VencHeaderData *pEncDataHeader)
{
    if(mCurrentState != MEDIA_RECORDER_PREPARED && mCurrentState != MEDIA_RECORDER_RECORDING)
    {
        alogw("can't get EncDataHeader before prepare()");
        return INVALID_OPERATION;
    }
    std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(VencId);
    if (mVencInfoMap.end() == it)
    {
        aloge("fatal error! mVencInfoMap don't have VencId[%d]", VencId);
        return BAD_VALUE;
    }
    VencParameters *pVencParam = it->second;
    VENC_CHN VeChn = pVencParam->getVencChnIndex();
    if(SUCCESS != AW_MPI_VENC_GetH264SpsPpsInfo(VeChn, pEncDataHeader))
    {
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t EyeseeRecorder::pushOneBsFrame(CDXRecorderBsInfo *frame)
{
    int total_bs_size=0;
    for (int i=0; i<frame->bs_count; i++)
    {
        total_bs_size+=frame->bs_size[i];
    }
    if(total_bs_size != frame->total_size)
    {
        aloge("fatal error! BsFrameSize[%d]!=[%d], check code!", total_bs_size, frame->total_size);
    }

    if (frame->mode == 0)
    {
        RawPacketHeader *pRawPacketHeader = (RawPacketHeader*)frame->bs_data[0];
        if(pRawPacketHeader->size != frame->total_size - frame->bs_size[0])
        {
            aloge("fatal error! BsFrameDataSize[%d]!=[%d], check code!", pRawPacketHeader->size, frame->total_size - frame->bs_size[0]);
        }
    }
    else if(frame->mode == 1)
    {
        TSPacketHeader *pTsPacketHeader = (TSPacketHeader*)frame->bs_data[0];
        if(pTsPacketHeader->size != frame->total_size - frame->bs_size[0])
        {
            aloge("fatal error! BsFrameDataSize[%d]!=[%d], check code!", pTsPacketHeader->size, frame->total_size - frame->bs_size[0]);
        }
    }
    Mutex::Autolock autoLock(mEncBufLock);
    if(mIdleEncBufList.empty())
    {
        alogw("EncBufList is full! ReadyNum[%d], GetOutNum[%d]", mReadyEncBufList.size(), mGetOutEncBufList.size());
        return NO_MEMORY;
    }
    std::list<VEncBuffer>::iterator it = mIdleEncBufList.begin();
    //config VEncBuffer
    if(0 == frame->mode)
    {
        RawPacketHeader *pRawPacketHeader = (RawPacketHeader*)frame->bs_data[0];
        it->mStreamId = pRawPacketHeader->mStreamId;
        it->stream_type = pRawPacketHeader->stream_type;
        it->data_size = pRawPacketHeader->size;
        it->pts = pRawPacketHeader->pts;
        it->CurrQp = pRawPacketHeader->CurrQp;
        it->avQp = pRawPacketHeader->avQp;
        it->nGopIndex = pRawPacketHeader->nGopIndex;
        it->nFrameIndex = pRawPacketHeader->nFrameIndex;
        it->nTotalIndex = pRawPacketHeader->nTotalIndex;
        if(it->data_size > 0)
        {
            it->data = (char*)malloc(it->data_size);
            if(NULL == it->data)
            {
                aloge("fatal error! malloc fail!");
            }
            int nCopyLen = 0;
            for(int i=1; i<frame->bs_count; i++)
            {
                memcpy(it->data+nCopyLen, frame->bs_data[i], frame->bs_size[i]);
                nCopyLen += frame->bs_size[i];
            }
        }
        else
        {
            aloge("fatal error! BsFrameDataSize == 0!");
            it->data = NULL;
        }
    }
    else if(1 == frame->mode)
    {
        TSPacketHeader *pTsPacketHeader = (TSPacketHeader*)frame->bs_data[0];
        it->mStreamId = -1;
        it->stream_type = pTsPacketHeader->stream_type;
        it->data_size = pTsPacketHeader->size;
        it->pts = pTsPacketHeader->pts;
        it->CurrQp = 0;
        it->avQp = 0;
        it->nGopIndex = 0;
        it->nFrameIndex = 0;
        it->nTotalIndex = 0;
        if(it->data_size > 0)
        {
            it->data = (char*)malloc(it->data_size);
            if(NULL == it->data)
            {
                aloge("fatal error! malloc fail!");
            }
            int nCopyLen = 0;
            for(int i=1; i<frame->bs_count; i++)
            {
                memcpy(it->data+nCopyLen, frame->bs_data[i], frame->bs_size[i]);
                nCopyLen += frame->bs_size[i];
            }
        }
        else
        {
            aloge("fatal error! BsFrameDataSize == 0!");
            it->data = NULL;
        }
    }
    else
    {
        aloge("fatal error! unsupport callback mode[%d]!", frame->mode);
    }
    //move to ready list
    mReadyEncBufList.splice(mReadyEncBufList.end(), mIdleEncBufList, it);
    return NO_ERROR;
}

VEncBuffer* EyeseeRecorder::getOneBsFrame()
{
    Mutex::Autolock autoLock(mEncBufLock);
    if(mReadyEncBufList.empty())
    {
        alogv("ReadyBufList is empty! GetOutBufList[%d]", mGetOutEncBufList.size());
        return NULL;
    }
    mGetOutEncBufList.splice(mGetOutEncBufList.end(), mReadyEncBufList, mReadyEncBufList.begin());
    return &mGetOutEncBufList.back();
}

void EyeseeRecorder::freeOneBsFrame(VEncBuffer *pEncData)
{
    Mutex::Autolock autoLock(mEncBufLock);
    if(NULL == pEncData)
    {
        return;
    }
    if(mGetOutEncBufList.empty())
    {
        aloge("fatal error! GetOutBufList is empty");
        return;
    }
    int nFindFlag = 0;
    std::list<VEncBuffer>::iterator DstIt;
    for(std::list<VEncBuffer>::iterator it = mGetOutEncBufList.begin(); it != mGetOutEncBufList.end(); ++it)
    {
        if(&*it == pEncData)
        {
            alogv("find EncBuf[%p]", pEncData);
            if(0 == nFindFlag)
            {
                DstIt = it;
            }
            nFindFlag++;
            //break;
        }
    }
    if(nFindFlag <= 0)
    {
        aloge("fatal error! not find in GetOutEncBufList, pEncData[%p]", pEncData);
    }
    else if(nFindFlag > 1)
    {
        aloge("fatal error! find [%d]nodes in GetOutEncBufList, pEncData[%p]", nFindFlag, pEncData);
    }
    else
    {
        if(DstIt->data)
        {
            free(DstIt->data);
            DstIt->data = NULL;
        }
        memset(&*DstIt, 0, sizeof(VEncBuffer));
        mIdleEncBufList.splice(mIdleEncBufList.end(), mGetOutEncBufList, DstIt);
    }
}
/*
bool EyeseeRecorder::compareOSDRectPriority(const VEncOSDRectInfo& first, const VEncOSDRectInfo& second)
{
    if (first.nPriority < second.nPriority)
    {
        return true;
    }
    return false;
}

status_t EyeseeRecorder::setOSDRects(std::list<VEncOSDRectInfo> &rects)
{
    mOSDRects = rects;
    mOSDRects.sort(compareOSDRectPriority);

    VENC_OVERLAY_INFO stOsdRegion;
    memset(&stOsdRegion, 0, sizeof(VENC_OVERLAY_INFO));
    stOsdRegion.regionNum = mOSDRects.size();
    if (stOsdRegion.regionNum > 0)
    {
        int pixelSize = 0;
        if (OSD_BITMAP_ARGB8888 == mOSDRects.begin()->mFormat)
        {
            stOsdRegion.nBitMapColorType = BITMAP_COLOR_ARGB8888;
            pixelSize = 4;
        }
        else if(OSD_BITMAP_ARGB4444 == mOSDRects.begin()->mFormat)
        {
            stOsdRegion.nBitMapColorType = BITMAP_COLOR_ARGB4444;
            pixelSize = 2;
        }
        else
        {
            stOsdRegion.nBitMapColorType = BITMAP_COLOR_ARGB1555;
            pixelSize = 2;
        }

        std::list<VEncOSDRectInfo>::iterator it;
        int i = 0;
        int bufSize = 0;
        for(it=mOSDRects.begin(); it!=mOSDRects.end(); ++it)
        {
            stOsdRegion.region[i].rect.X = it->mRect.X;
            stOsdRegion.region[i].rect.Y = it->mRect.Y;
            stOsdRegion.region[i].rect.Width = it->mRect.Width;
            stOsdRegion.region[i].rect.Height = it->mRect.Height;
            if (it->mOsdType == OSD_TYPE_OVERLAY)
            {
                stOsdRegion.region[i].bOverlayType = OVERLAY_STYLE_NORMAL;
            }
            else if (it->mOsdType == OSD_TYPE_COVER)
            {
                stOsdRegion.region[i].bOverlayType = OVERLAY_STYLE_COVER;
            }
            else
            {
                stOsdRegion.region[i].bOverlayType = OVERLAY_STYLE_LUMA_REVERSE;
            }
            stOsdRegion.region[i].bRegionID = i;
            stOsdRegion.region[i].nPriority = it->nPriority;
            stOsdRegion.region[i].coverYUV.bCoverY = (it->coverYUVColor >> 16) & 0xFF;
            stOsdRegion.region[i].coverYUV.bCoverU = (it->coverYUVColor >> 8) & 0xFF;
            stOsdRegion.region[i].coverYUV.bCoverV = (it->coverYUVColor & 0xFF);
            stOsdRegion.region[i].extraAlphaFlag = 0;
            stOsdRegion.region[i].extraAlphaVal = 0;
            stOsdRegion.region[i].pBitMapAddr = it->mpBuf;
            stOsdRegion.region[i].nBitMapSize = pixelSize * it->mRect.Width * it->mRect.Height;
            bufSize += stOsdRegion.region[i].nBitMapSize;
            i++;
        }
    }
    AW_MPI_VENC_setOsdMaskRegions(mVeChn, &stOsdRegion);

    return NO_ERROR;
}
*/
RGN_HANDLE EyeseeRecorder::createRegion(const RGN_ATTR_S *pstRegion)
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
            alogv("create region[%d] success!", handle);
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

status_t EyeseeRecorder::setRegionBitmap(RGN_HANDLE Handle, const BITMAP_S *pBitmap)
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

status_t EyeseeRecorder::attachRegionToVenc(int VencId, RGN_HANDLE Handle, const RGN_CHN_ATTR_S *pstChnAttr)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mLock);
    if (!(mCurrentState&MEDIA_RECORDER_PREPARED || mCurrentState&MEDIA_RECORDER_RECORDING))
    {
        alogw("Be careful! current state[0x%x], must set region in state prepared or recording!", mCurrentState);
        return INVALID_OPERATION;
    }
    std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(VencId);
    if (mVencInfoMap.end() == it)
    {
        aloge("fatal error! mVencInfoMap don't have VencId[%d]", VencId);
        return BAD_VALUE;
    }
    VencParameters *pVencParam = it->second;
    VENC_CHN VeChn = pVencParam->getVencChnIndex();
    if(VeChn < 0)
    {
        aloge("fatal error! venc is not created");
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
        MPP_CHN_S stChn = {MOD_ID_VENC, 0, VeChn};
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

status_t EyeseeRecorder::detachRegionFromVenc(int VencId, RGN_HANDLE Handle)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mLock);
    std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(VencId);
    if (mVencInfoMap.end() == it)
    {
        aloge("fatal error! mVencInfoMap don't have VencId[%d]", VencId);
        return BAD_VALUE;
    }
    VencParameters *pVencParam = it->second;
    VENC_CHN VeChn = pVencParam->getVencChnIndex();
    if(VeChn < 0)
    {
        aloge("fatal error! venc is not created");
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
        MPP_CHN_S stChn = {MOD_ID_VENC, 0, VeChn};
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

status_t EyeseeRecorder::setRegionDisplayAttrOfVenc(int VencId, RGN_HANDLE Handle, const RGN_CHN_ATTR_S *pstChnAttr)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mLock);
    std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(VencId);
    if (mVencInfoMap.end() == it)
    {
        aloge("fatal error! mVencInfoMap don't have VencId[%d]", VencId);
        return BAD_VALUE;
    }
    VencParameters *pVencParam = it->second;
    VENC_CHN VeChn = pVencParam->getVencChnIndex();
    if(VeChn < 0)
    {
        aloge("fatal error! venc is not created");
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
        MPP_CHN_S stChn = {MOD_ID_VENC, 0, VeChn};
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
status_t EyeseeRecorder::getRegionDisplayAttrOfVenc(int VencId, RGN_HANDLE Handle, RGN_CHN_ATTR_S *pstChnAttr)
{
    status_t result = NO_ERROR;
    Mutex::Autolock autoLock(mLock);
    std::map<int, VencParameters*>::iterator it = mVencInfoMap.find(VencId);
    if (mVencInfoMap.end() == it)
    {
        aloge("fatal error! mVencInfoMap don't have VencId[%d]", VencId);
        return BAD_VALUE;
    }
    VencParameters *pVencParam = it->second;
    VENC_CHN VeChn = pVencParam->getVencChnIndex();
    if(VeChn < 0)
    {
        aloge("fatal error! venc is not created");
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
        MPP_CHN_S stChn = {MOD_ID_VENC, 0, VeChn};
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
status_t EyeseeRecorder::destroyRegion(RGN_HANDLE Handle)
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

};
