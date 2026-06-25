/*************************************************
Copyright (C), 2015, AllwinnerTech. Co., Ltd.
File name: stream_recorder.cpp
Author: yinzh@allwinnertech.com
Version: 0.1
Date: 2016-5-12
Description:
History:
*************************************************/

#include "device_model/media/recorder/stream_recorder.h"
#include "device_model/media/camera/camera.h"
#include "common/app_log.h"

#include <stdlib.h>
#include <string.h>
#include <string>

#undef LOG_TAG
#define LOG_TAG "stream_recorder.cpp"

using namespace EyeseeLinux;
using namespace std;

StreamRecorder::StreamRecorder(Camera *camera)
    : Recorder(camera)
{
}

StreamRecorder::~StreamRecorder()
{
    StopRecord();
}

int StreamRecorder::InitRecorder(const RecorderParam &param)
{
    int ret = -1;

    if (current_status_ != RECORDER_IDLE) {
        db_error("status: [%d], recorder is not idle, you can not init recorder", current_status_);
        return ret;
    }

    this->param_ = param;

    PAYLOAD_TYPE_E enc_type = PT_H264;
    switch (param_.enc_type) {
        case VENC_H264:
            enc_type = PT_H264;
            break;
        case VENC_H265:
            enc_type = PT_H265;
            break;
        case VENC_MJPEG:
            enc_type = PT_MJPEG;
            break;
        default:
            db_warn("unsuppoort video encode type, use h264 as default");
            enc_type = PT_H264;
            break;

    }
    //recorder_->setVideoEncoder(enc_type);

    if (param.audio_record) {
        recorder_->setAudioSource(EyeseeRecorder::AudioSource::MIC);
        recorder_->setAudioSamplingRate(44100);
        recorder_->setAudioChannels(1);
        recorder_->setAudioEncodingBitRate(12200);
        recorder_->setAudioEncoder(PT_AAC);
        //recorder_->setBsFrameRawDataType(CALLBACK_OUT_DATA_VIDEO_AUDIO);
    } else {
        //recorder_->setBsFrameRawDataType(CALLBACK_OUT_DATA_VIDEO_ONLY);
    }

    if (camera_ != NULL) {
        recorder_->setCameraProxy(camera_->GetEyeseeCamera()->getRecordingProxy(), param_.vi_chn);
    }

    ret = recorder_->setVideoSource(EyeseeRecorder::VideoSource::CAMERA);
    if (ret != NO_ERROR) {
        db_error("setVideoSource Failed(%d)", ret);
        return ret;
    }

    int muxer_id;
    //muxer_id = recorder_->addOutputFormatAndOutputSink(
    //        MEDIA_FILE_FORMAT_RAW, -1, 0, true);
    SinkParam stSinkParam;
    memset(&stSinkParam, 0, sizeof(SinkParam));
    stSinkParam.mOutputFormat = MEDIA_FILE_FORMAT_RAW;
    stSinkParam.mOutputFd = -1;
    stSinkParam.mOutputPath = NULL;
    stSinkParam.mFallocateLen = 0;
    stSinkParam.mMaxDurationMs = 0;
    stSinkParam.bCallbackOutFlag = true;
    stSinkParam.bBufFromCacheFlag = false;
    muxer_id = recorder_->addOutputSink(&stSinkParam);
    muxer_id_map_.insert(make_pair(MEDIA_FILE_FORMAT_RAW, muxer_id));

    VencParameters stVencParam;

    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_RC_PARAM_S stVencRCParam;
    memset(&stVencChnAttr, 0, sizeof(stVencChnAttr));
    memset(&stVencRCParam, 0, sizeof(stVencRCParam));
    stVencChnAttr.VeAttr.Type = enc_type;
    if (PT_H264 == stVencChnAttr.VeAttr.Type)
    {
        stVencChnAttr.VeAttr.AttrH264e.BufSize = 0;
        stVencChnAttr.VeAttr.AttrH264e.Profile = 2;
        stVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        stVencChnAttr.VeAttr.AttrH264e.PicWidth  = param_.video_size.width;
        stVencChnAttr.VeAttr.AttrH264e.PicHeight = param_.video_size.height;
        stVencChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_Default;
        stVencChnAttr.VeAttr.AttrH264e.FastEncFlag = FALSE;
        stVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        stVencChnAttr.VeAttr.AttrH264e.mThreshSize = 0;
    }
    else if (PT_H265 == stVencChnAttr.VeAttr.Type)
    {
        stVencChnAttr.VeAttr.AttrH265e.mBufSize = 0;
        stVencChnAttr.VeAttr.AttrH265e.mProfile = 0;
        stVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        stVencChnAttr.VeAttr.AttrH265e.mPicWidth = param_.video_size.width;
        stVencChnAttr.VeAttr.AttrH265e.mPicHeight = param_.video_size.height;
        stVencChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_Default;
        stVencChnAttr.VeAttr.AttrH265e.mFastEncFlag = FALSE;
        stVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        stVencChnAttr.VeAttr.AttrH265e.mThreshSize = 0;
    }
    else if (PT_MJPEG == stVencChnAttr.VeAttr.Type)
    {
        stVencChnAttr.VeAttr.AttrMjpeg.mBufSize = 0;
        stVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        stVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = param_.video_size.width;
        stVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = param_.video_size.height;
        stVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = 0;
    }
    else
    {
        aloge("fatal error! unknown vencType:%d", stVencChnAttr.VeAttr.Type);
    }
    stVencChnAttr.VeAttr.MaxKeyInterval = param_.framerate; //mpi_venc decide it self.
    //stVencChnAttr.VeAttr.SrcPicWidth = ; // no need set src frame width and height, because EyeseeRecorder will get them from EyeseeCamera.
    //stVencChnAttr.VeAttr.SrcPicHeight = ;
    //stVencChnAttr.VeAttr.Field = ;
    //stVencChnAttr.VeAttr.PixelFormat = ;
    //stVencChnAttr.VeAttr.mColorSpace = ;
    stVencChnAttr.VeAttr.Rotate = ROTATE_NONE;
    stVencChnAttr.VeAttr.mOnlineEnable = 0;
    stVencChnAttr.VeAttr.mOnlineShareBufNum = 0;
    stVencChnAttr.VeAttr.mDropFrameNum = 0;
    stVencChnAttr.VeAttr.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT;
    stVencChnAttr.VeAttr.mVeRecRefBufReduceEnable = 0;
    stVencChnAttr.VeAttr.mVbrOptEnable = 0;
    stVencChnAttr.RcAttr.mRcMode = param_.judgeVENC_RC_MODE_E();
    setVideoEncodingBitRateToVENC_CHN_ATTR_S(&stVencChnAttr, param_.bitrate);
    VENC_FRAME_RATE_S stFrameRate;
    memset(&stFrameRate, 0, sizeof(stFrameRate));
    //stFrameRate.SrcFrmRate = ; //EyeseeRecorder will get it from EyeseeCamera
    stFrameRate.DstFrmRate = param_.framerate;
    SetFrameRateToVENC_CHN_ATTR_S(&stFrameRate, &stVencChnAttr);
    if(VENC_RC_MODE_H264FIXQP == stVencChnAttr.RcAttr.mRcMode)
    {
        stVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 25;
        stVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 25;
    }
    else if(VENC_RC_MODE_H265FIXQP == stVencChnAttr.RcAttr.mRcMode)
    {
        stVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 25;
        stVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 25;
    }
    else if(VENC_RC_MODE_MJPEGFIXQP == stVencChnAttr.RcAttr.mRcMode)
    {
        stVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 80;
    }
    stVencChnAttr.RcAttr.mProductMode = PRODUCT_CDR;
    stVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    stVencChnAttr.GopAttr.mGopSize = 2;
    //stVencChnAttr.EncppAttr.mbEncppEnable = ; //EyeseeRecorder will get it from EyeseeCamera
    if(VENC_RC_MODE_H264CBR == stVencChnAttr.RcAttr.mRcMode)
    {
        stVencRCParam.ParamH264Cbr.mMaxQp = 50;
        stVencRCParam.ParamH264Cbr.mMinQp = 10;
        stVencRCParam.ParamH264Cbr.mMaxPqp = 50;
        stVencRCParam.ParamH264Cbr.mMinPqp = 10;
        stVencRCParam.ParamH264Cbr.mQpInit = 35;
        stVencRCParam.ParamH264Cbr.mbEnMbQpLimit = 0;
    }
    else if(VENC_RC_MODE_H264VBR == stVencChnAttr.RcAttr.mRcMode)
    {
        stVencRCParam.ParamH264Vbr.mMaxQp = 50;
        stVencRCParam.ParamH264Vbr.mMinQp = 10;
        stVencRCParam.ParamH264Vbr.mMaxPqp = 50;
        stVencRCParam.ParamH264Vbr.mMinPqp = 10;
        stVencRCParam.ParamH264Vbr.mQpInit = 35;
        stVencRCParam.ParamH264Vbr.mbEnMbQpLimit = 0;
        stVencRCParam.ParamH264Vbr.mMovingTh = 0;
        stVencRCParam.ParamH264Vbr.mQuality = 0;
        stVencRCParam.ParamH264Vbr.mIFrmBitsCoef = 10;
        stVencRCParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    }
    else if(VENC_RC_MODE_H265CBR == stVencChnAttr.RcAttr.mRcMode)
    {
        stVencRCParam.ParamH265Cbr.mMaxQp = 50;
        stVencRCParam.ParamH265Cbr.mMinQp = 10;
        stVencRCParam.ParamH265Cbr.mMaxPqp = 50;
        stVencRCParam.ParamH265Cbr.mMinPqp = 10;
        stVencRCParam.ParamH265Cbr.mQpInit = 35;
        stVencRCParam.ParamH265Cbr.mbEnMbQpLimit = 0;
    }
    else if(VENC_RC_MODE_H265VBR == stVencChnAttr.RcAttr.mRcMode)
    {
        stVencRCParam.ParamH265Vbr.mMaxQp = 50;
        stVencRCParam.ParamH265Vbr.mMinQp = 10;
        stVencRCParam.ParamH265Vbr.mMaxPqp = 50;
        stVencRCParam.ParamH265Vbr.mMinPqp = 10;
        stVencRCParam.ParamH265Vbr.mQpInit = 35;
        stVencRCParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        stVencRCParam.ParamH265Vbr.mMovingTh = 0;
        stVencRCParam.ParamH265Vbr.mQuality = 0;
        stVencRCParam.ParamH265Vbr.mIFrmBitsCoef = 10;
        stVencRCParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }
    stVencParam.setVencChnAttr(stVencChnAttr);
    stVencParam.setVencRcParam(stVencRCParam);

    stVencParam.enableIspAndVeLink(false);
    stVencParam.setMainStreamFlag(true);
    stVencParam.setEncppSharpAttenCoefPer(100);

    s3DfilterParam st3DfilterParam;
    memset(&st3DfilterParam, 0, sizeof(st3DfilterParam));
    stVencParam.set3DFilter(st3DfilterParam);

    //open ve debug node
    VeProcSet stVeProcSet;
    memset(&stVeProcSet, 0, sizeof(stVeProcSet));
    stVeProcSet.bProcEnable = 1;
    stVeProcSet.nProcFreq = 30;
    stVeProcSet.nStatisBitRateTime = 1000;
    stVeProcSet.nStatisFrRateTime = 1000;
    stVencParam.setProcSet(stVeProcSet);

    recorder_->setVencParameters(mVencId, &stVencParam);

    ret = recorder_->prepare();
    if (ret != NO_ERROR) {
        db_error("prepare Failed(%d)", ret);
        return ret;
    }

    current_status_ = RECORDER_PREPARED;

    return ret;
}

void StreamRecorder::Update(MSG_TYPE msg,int cam_id,int rec_id)
{
    switch (msg) {
        default:
            #if 0
            Recorder::Update(msg);
            #endif
            break;
    }
}

int StreamRecorder::StopRecord()
{
    int ret;

    ret = Recorder::StopRecord();

    current_status_ = RECORDER_IDLE;

    return ret;
}

void StreamRecorder::DumpRecorderParm()
{
}

void StreamRecorder::setWifiFlag(bool flag)
{
}

int StreamRecorder::GetSoSRecorderFileName(char *p_FileName)
{
    return 0;
}
