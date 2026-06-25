/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : media_common.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/03/15
  Last Modified :
  Description   : multimedia common function for internal use.
  Function List :
  History       :
******************************************************************************/
// ref platform headers
#include <linux/videodev2.h>
#include <string.h>
#include <utils/plat_log.h>
#include "cdx_list.h"
#include "plat_defines.h"
#include "plat_errno.h"
#include "plat_math.h"
#include "plat_type.h"

// media api headers to app
#include "mm_comm_demux.h"
#include "mm_comm_venc.h"
#include "mm_comm_aenc.h"
#include "mm_comm_adec.h"
#include "mm_comm_video.h"
#include "mm_comm_uvc.h"
#include "mm_comm_aio.h"
#include "mm_common.h"

// media internal common headers.
#include "mm_component.h"

#include <media_common.h>
#include <media_common_vcodec.h>
#include <media_common_acodec.h>

ERRORTYPE copy_MPP_CHN_S(MPP_CHN_S *pDst, MPP_CHN_S *pSrc)
{
    *pDst = *pSrc;
    return SUCCESS;
}

VENC_CODEC_TYPE map_PAYLOAD_TYPE_E_to_VENC_CODEC_TYPE(PAYLOAD_TYPE_E nPayLoadType)
{
    VENC_CODEC_TYPE nVenclibType;
    switch (nPayLoadType) {
        case PT_H264:
            nVenclibType = VENC_CODEC_H264;
            break;
        case PT_H265:
            nVenclibType = VENC_CODEC_H265;
            break;
        case PT_JPEG:
        case PT_MJPEG:
            nVenclibType = VENC_CODEC_JPEG;
            break;
        default:
            aloge("fatal error! unknown PlayLoadType[%d]", nPayLoadType);
            nVenclibType = VENC_CODEC_H264;
            break;
    }
    return nVenclibType;
}

PAYLOAD_TYPE_E map_VENC_CODEC_TYPE_to_PAYLOAD_TYPE_E(VENC_CODEC_TYPE eVencCodecType)
{
    PAYLOAD_TYPE_E ePayloadType;
    switch (eVencCodecType)
    {
        case VENC_CODEC_H264:
            ePayloadType = PT_H264;
            break;
        case VENC_CODEC_H265:
            ePayloadType = PT_H265;
            break;
        case VENC_CODEC_JPEG:
            ePayloadType = PT_JPEG;
            break;
        default:
            aloge("fatal error! unknown VencCodecType[%d]", eVencCodecType);
            ePayloadType = PT_H264;
            break;
    }
    return ePayloadType;
}

AUDIO_ENCODER_TYPE map_PAYLOAD_TYPE_E_to_AUDIO_ENCODER_TYPE(PAYLOAD_TYPE_E nPayLoadType)
{
    AUDIO_ENCODER_TYPE nAenclibType;
    switch (nPayLoadType) {
        case PT_AAC:
            nAenclibType = AUDIO_ENCODER_AAC_TYPE;
            break;
        case PT_LPCM:
            nAenclibType = AUDIO_ENCODER_LPCM_TYPE;
            break;
        case PT_PCM_AUDIO:
            nAenclibType = AUDIO_ENCODER_PCM_TYPE;
            break;
        case PT_MP3:
            nAenclibType = AUDIO_ENCODER_MP3_TYPE;
            break;
        case PT_G711A:
            nAenclibType = AUDIO_ENCODER_G711A_TYPE;
            break;
        case PT_G711U:
            nAenclibType = AUDIO_ENCODER_G711U_TYPE;
            break;
        case PT_G726:
            nAenclibType = AUDIO_ENCODER_G726A_TYPE;
            break;
        case PT_G726U:
            nAenclibType = AUDIO_ENCODER_G726U_TYPE;
            break;
        case PT_OPUS:
            nAenclibType = AUDIO_ENCODER_OPUS_TYPE;
            break;
        default:
            aloge("fatal error! unknown PlayLoadType[%d]", nPayLoadType);
            nAenclibType = AUDIO_ENCODER_AAC_TYPE;
            break;
    }
    return nAenclibType;
}

PAYLOAD_TYPE_E map_AUDIO_ENCODER_TYPE_to_PAYLOAD_TYPE_E(AUDIO_ENCODER_TYPE eAencCodecType)
{
    PAYLOAD_TYPE_E ePayloadType;
    switch (eAencCodecType)
    {
        case AUDIO_ENCODER_AAC_TYPE:
            ePayloadType = PT_AAC;
            break;
        case AUDIO_ENCODER_LPCM_TYPE:
            ePayloadType = PT_LPCM;
            break;
        case AUDIO_ENCODER_PCM_TYPE:
            ePayloadType = PT_PCM_AUDIO;
            break;
        case AUDIO_ENCODER_MP3_TYPE:
            ePayloadType = PT_MP3;
            break;
        case AUDIO_ENCODER_G711A_TYPE:
            ePayloadType = PT_G711A;
            break;
        case AUDIO_ENCODER_G711U_TYPE:
            ePayloadType = PT_G711U;
            break;
        case AUDIO_ENCODER_G726A_TYPE:
            ePayloadType = PT_G726;
            break;
        case AUDIO_ENCODER_G726U_TYPE:
            ePayloadType = PT_G726U;
            break;
        case AUDIO_ENCODER_OPUS_TYPE:
            ePayloadType = PT_OPUS;
            break;
        default:
            aloge("fatal error! unknown AencCodecType[%d]", eAencCodecType);
            ePayloadType = PT_AAC;
            break;
    }
    return ePayloadType;
}

enum EVIDEOCODECFORMAT map_PAYLOAD_TYPE_E_to_EVIDEOCODECFORMAT(PAYLOAD_TYPE_E nPayLoadType)
{
    enum EVIDEOCODECFORMAT nVDecLibType;
    switch (nPayLoadType) {
        case PT_H264:
            nVDecLibType = VIDEO_CODEC_FORMAT_H264;
            break;
        case PT_H265:
            nVDecLibType = VIDEO_CODEC_FORMAT_H265;
            break;
        case PT_JPEG:
        case PT_MJPEG:
            nVDecLibType = VIDEO_CODEC_FORMAT_MJPEG;
            break;
        default:
            alogw("fatal error! unsupported format[0x%x]", nPayLoadType);
            nVDecLibType = VIDEO_CODEC_FORMAT_MJPEG;
            break;
    }
    return nVDecLibType;
}

PAYLOAD_TYPE_E map_EVIDEOCODECFORMAT_to_PAYLOAD_TYPE_E(enum EVIDEOCODECFORMAT eCodecFormat)
{
    PAYLOAD_TYPE_E dstType;
    switch (eCodecFormat) {
        case VIDEO_CODEC_FORMAT_H264:
            dstType = PT_H264;
            break;
        case VIDEO_CODEC_FORMAT_MJPEG:
            dstType = PT_MJPEG;
            break;
        case VIDEO_CODEC_FORMAT_H265:
            dstType = PT_H265;
            break;
        default:
            aloge("fatal error! unsupported format[0x%x]", eCodecFormat);
            dstType = PT_MAX;
            break;
    }
    return dstType;
}

PAYLOAD_TYPE_E map_EAUDIOCODECFORMAT_to_PAYLOAD_TYPE_E(enum EAUDIOCODECFORMAT eCodecFormat, int eSubCodecFormat)
{
    PAYLOAD_TYPE_E dstType;
    switch(eCodecFormat)
    {
        case AUDIO_CODEC_FORMAT_MPEG_AAC_LC:
        case AUDIO_CODEC_FORMAT_RAAC:
            dstType = PT_AAC;
            break;
        case AUDIO_CODEC_FORMAT_MP1:
        case AUDIO_CODEC_FORMAT_MP2:
        case AUDIO_CODEC_FORMAT_MP3:
            dstType = PT_MP3;
            break;
        case AUDIO_CODEC_FORMAT_PCM:
        {
            //WAVE_FORMAT_ALAW, ABS_EDIAN_FLAG_BIG, ABS_EDIAN_FLAG_MASK
            //if((tmpAbsFmt->eSubCodecFormat & ABS_EDIAN_FLAG_MASK) == ABS_EDIAN_FLAG_BIG)
            switch(eSubCodecFormat & (~ABS_EDIAN_FLAG_MASK))
            {
                case 0x1:
                {
                    dstType = PT_PCM_AUDIO;
                    break;
                }
                case 0x6:
                {
                    dstType = PT_G711A;
                    break;
                }
                case 0x7:
                {
                    dstType = PT_G711U;
                    break;
                }
                default:
                {
                    aloge("fatal error! unknown subCodecFormat:0x%x for pcm!", eSubCodecFormat);
                    dstType = PT_PCM_AUDIO;
                    break;
                }
            }
            break;
        }
        case AUDIO_CODEC_FORMAT_G711a:
            dstType = PT_G711A;
            break;
        case AUDIO_CODEC_FORMAT_G711u:
            dstType = PT_G711U;
            break;
        case AUDIO_CODEC_FORMAT_G726a:
            dstType = PT_G726;
            break;
        case AUDIO_CODEC_FORMAT_G726u:
            dstType = PT_G726U;
            break;
        case AUDIO_CODEC_FORMAT_AMR:
            dstType = PT_AMR;
            break;
        case AUDIO_CODEC_FORMAT_APE:
            dstType = PT_APE;
            break;
        case AUDIO_CODEC_FORMAT_FLAC:
            dstType = PT_FLAC;
            break;
        case AUDIO_CODEC_FORMAT_OGG:
            dstType = PT_OGG;
            break;
        case AUDIO_CODEC_FORMAT_OPUS:
            dstType = PT_OPUS;
            break;
        default:
            aloge("fatal error! unsupported format[0x%x]", eCodecFormat);
            dstType = PT_MAX;
            break;
    }
    return dstType;
}

enum EAUDIOCODECFORMAT map_PAYLOAD_TYPE_E_to_EAUDIOCODECFORMAT(PAYLOAD_TYPE_E srcFormat)
{
    enum EAUDIOCODECFORMAT dstFormat;
    switch(srcFormat)
    {
        case PT_AAC:
            dstFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
            break;
        case PT_MP3:
            dstFormat = AUDIO_CODEC_FORMAT_MP3;
            break;
        case PT_PCM_AUDIO:
            dstFormat = AUDIO_CODEC_FORMAT_PCM;
            break;
        case PT_ADPCMA:
            dstFormat = AUDIO_CODEC_FORMAT_ADPCM;
            break;
        case PT_G711A:
            dstFormat = AUDIO_CODEC_FORMAT_G711a;
            break;
        case PT_G711U:
            dstFormat = AUDIO_CODEC_FORMAT_G711u;
            break;
        case PT_G726:
            dstFormat = AUDIO_CODEC_FORMAT_G726a;
            break;
        case PT_G726U:
            dstFormat = AUDIO_CODEC_FORMAT_G726u;
            break;
        case PT_AMR:
            dstFormat = AUDIO_CODEC_FORMAT_AMR;
            break;
        case PT_APE:
            dstFormat = AUDIO_CODEC_FORMAT_APE;
            break;
        case PT_FLAC:
            dstFormat = AUDIO_CODEC_FORMAT_FLAC;
            break;
        case PT_OGG:
            dstFormat = AUDIO_CODEC_FORMAT_OGG;
            break;
        case PT_OPUS:
            dstFormat = AUDIO_CODEC_FORMAT_OPUS;
            break;
        default:
            dstFormat = AUDIO_CODEC_FORMAT_MPEG_AAC_LC;
            alogw("Unsupported audio decoder format [%d]! Set AAC to it!", srcFormat);
            break;
    }

    return dstFormat;
}

ERRORTYPE setVideoEncodingBitRateToVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pChnAttr, int bitRate)
{
    switch(pChnAttr->RcAttr.mRcMode)
    {
        case VENC_RC_MODE_H264CBR:
        {
            pChnAttr->RcAttr.mAttrH264Cbr.mBitRate = bitRate;
            break;
        }
        case VENC_RC_MODE_H264VBR:
        {
            pChnAttr->RcAttr.mAttrH264Vbr.mMaxBitRate = bitRate;
            break;
        }
        case VENC_RC_MODE_H264ABR:
        {
            pChnAttr->RcAttr.mAttrH264Abr.mMaxBitRate = bitRate;
            break;
        }
        case VENC_RC_MODE_H265CBR:
        {
            pChnAttr->RcAttr.mAttrH265Cbr.mBitRate = bitRate;
            break;
        }
        case VENC_RC_MODE_H265VBR:
        {
            pChnAttr->RcAttr.mAttrH265Vbr.mMaxBitRate = bitRate;
            break;
        }
        case VENC_RC_MODE_H265ABR:
        {
            pChnAttr->RcAttr.mAttrH265Abr.mMaxBitRate = bitRate;
            break;
        }
        case VENC_RC_MODE_MJPEGCBR:
        {
            pChnAttr->RcAttr.mAttrMjpegeCbr.mBitRate = bitRate;
            break;
        }
        default:
        {
            aloge("fatal error! other rc mode[0x%x] don't need set bitRate!", pChnAttr->RcAttr.mRcMode);
            break;
        }
    }
    return SUCCESS;
}

unsigned int GetBitRateFromVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pAttr)
{
    unsigned int nBitRate;
    if(PT_JPEG == pAttr->VeAttr.Type)
    {
        return 0;
    }
    switch (pAttr->RcAttr.mRcMode) 
    {
        case VENC_RC_MODE_H264CBR:
            nBitRate = pAttr->RcAttr.mAttrH264Cbr.mBitRate;
            break;
        case VENC_RC_MODE_H264VBR:
            nBitRate = pAttr->RcAttr.mAttrH264Vbr.mMaxBitRate;
            break;
        case VENC_RC_MODE_H264FIXQP:
            nBitRate = 0;
            break;
        case VENC_RC_MODE_H264ABR:
            nBitRate = pAttr->RcAttr.mAttrH264Abr.mMaxBitRate;
            break;
        case VENC_RC_MODE_H264QPMAP:
            nBitRate = pAttr->RcAttr.mAttrH264QpMap.mMaxBitRate;
            break;
        case VENC_RC_MODE_H265CBR:
            nBitRate = pAttr->RcAttr.mAttrH265Cbr.mBitRate;
            break;
        case VENC_RC_MODE_H265VBR:
            nBitRate = pAttr->RcAttr.mAttrH265Vbr.mMaxBitRate;
            break;
        case VENC_RC_MODE_H265FIXQP:
            nBitRate = 0;
            break;
        case VENC_RC_MODE_H265ABR:
            nBitRate = pAttr->RcAttr.mAttrH265Abr.mMaxBitRate;
            break;
        case VENC_RC_MODE_H265QPMAP:
            nBitRate = pAttr->RcAttr.mAttrH265QpMap.mMaxBitRate;
            break;
        case VENC_RC_MODE_MJPEGCBR:
            nBitRate = pAttr->RcAttr.mAttrMjpegeCbr.mBitRate;
            break;
        case VENC_RC_MODE_MJPEGFIXQP:
            nBitRate = 0;
            break;
        default:
            alogw("unsupported temporary: chn attr RcAttr RcMode[0x%x]", pAttr->RcAttr.mRcMode);
            nBitRate = 0;
            break;
    }
    return nBitRate;
}

ERRORTYPE SetFrameRateToVENC_CHN_ATTR_S(const VENC_FRAME_RATE_S *pFrameRate, VENC_CHN_ATTR_S *pAttr)
{
    int ret = 0;
    if (NULL == pAttr || NULL == pFrameRate)
    {
        aloge("fatal error! invalid input params! pAttr=%p, pFrameRate=%p", pAttr, pFrameRate);
        return -1;
    }

    switch (pAttr->RcAttr.mRcMode) 
    {
        case VENC_RC_MODE_H264CBR:
            pAttr->RcAttr.mAttrH264Cbr.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrH264Cbr.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        case VENC_RC_MODE_H264VBR:
            pAttr->RcAttr.mAttrH264Vbr.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrH264Vbr.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        case VENC_RC_MODE_H264FIXQP:
            pAttr->RcAttr.mAttrH264FixQp.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrH264FixQp.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        case VENC_RC_MODE_H264QPMAP:
            pAttr->RcAttr.mAttrH264QpMap.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrH264QpMap.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        case VENC_RC_MODE_H265CBR:
            pAttr->RcAttr.mAttrH265Cbr.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrH265Cbr.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        case VENC_RC_MODE_H265VBR:
            pAttr->RcAttr.mAttrH265Vbr.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrH265Vbr.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        case VENC_RC_MODE_H265FIXQP:
            pAttr->RcAttr.mAttrH265FixQp.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrH265FixQp.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        case VENC_RC_MODE_H265QPMAP:
            pAttr->RcAttr.mAttrH265QpMap.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrH265QpMap.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        case VENC_RC_MODE_MJPEGCBR:
            pAttr->RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrMjpegeCbr.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        case VENC_RC_MODE_MJPEGFIXQP:
            pAttr->RcAttr.mAttrMjpegeFixQp.mSrcFrmRate = pFrameRate->SrcFrmRate;
            pAttr->RcAttr.mAttrMjpegeFixQp.mDstFrmRate = pFrameRate->DstFrmRate;
            break;
        default:
            aloge("fatal error! unsupported temporary: chn attr RcAttr RcMode[0x%x], set default framerate 20", pAttr->RcAttr.mRcMode);
            pAttr->RcAttr.mAttrMjpegeFixQp.mSrcFrmRate = 20;
            pAttr->RcAttr.mAttrMjpegeFixQp.mDstFrmRate = 20;
            ret = -1;
            break;
    }

    return ret;
}

ERRORTYPE GetFrameRateFromVENC_CHN_ATTR_S(const VENC_CHN_ATTR_S *pAttr, VENC_FRAME_RATE_S *pFrameRate)
{
    int ret = 0;
    if (NULL == pAttr || NULL == pFrameRate)
    {
        aloge("fatal error! invalid input params! pAttr=%p, pFrameRate=%p", pAttr, pFrameRate);
        return -1;
    }

    if (PT_JPEG == pAttr->VeAttr.Type)
    {
        return 0;
    }

    switch (pAttr->RcAttr.mRcMode) 
    {
        case VENC_RC_MODE_H264CBR:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrH264Cbr.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrH264Cbr.mDstFrmRate;
            break;
        case VENC_RC_MODE_H264VBR:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrH264Vbr.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrH264Vbr.mDstFrmRate;
            break;
        case VENC_RC_MODE_H264FIXQP:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrH264FixQp.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrH264FixQp.mDstFrmRate;
            break;
        case VENC_RC_MODE_H264QPMAP:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrH264QpMap.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrH264QpMap.mDstFrmRate;
            break;
        case VENC_RC_MODE_H265CBR:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrH265Cbr.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrH265Cbr.mDstFrmRate;
            break;
        case VENC_RC_MODE_H265VBR:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrH265Vbr.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrH265Vbr.mDstFrmRate;
            break;
        case VENC_RC_MODE_H265FIXQP:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrH265FixQp.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrH265FixQp.mDstFrmRate;
            break;
        case VENC_RC_MODE_H265QPMAP:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrH265QpMap.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrH265QpMap.mDstFrmRate;
            break;
        case VENC_RC_MODE_MJPEGCBR:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrMjpegeCbr.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrMjpegeCbr.mDstFrmRate;
            break;
        case VENC_RC_MODE_MJPEGFIXQP:
            pFrameRate->SrcFrmRate = pAttr->RcAttr.mAttrMjpegeFixQp.mSrcFrmRate;
            pFrameRate->DstFrmRate = pAttr->RcAttr.mAttrMjpegeFixQp.mDstFrmRate;
            break;
        default:
            aloge("fatal error! unsupported temporary: chn attr RcAttr RcMode[0x%x], set default framerate 20", pAttr->RcAttr.mRcMode);
            pFrameRate->SrcFrmRate = 20;
            pFrameRate->DstFrmRate = 20;
            ret = -1;
            break;
    }

    return ret;
}

ERRORTYPE GetEncodeDstSizeFromVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pAttr, SIZE_S *pDstSize)
{
    ERRORTYPE ret = SUCCESS;
    if(NULL == pDstSize || NULL == pAttr)
    {
        alogw("Be careful! null pointer!");
        return FAILURE;
    }
    switch (pAttr->VeAttr.Type)
    {
        case PT_JPEG:
        {
            pDstSize->Width = pAttr->VeAttr.AttrJpeg.PicWidth;
            pDstSize->Height = pAttr->VeAttr.AttrJpeg.PicHeight;
            break;
        }
        case PT_MJPEG:
        {
            pDstSize->Width = pAttr->VeAttr.AttrMjpeg.mPicWidth;
            pDstSize->Height = pAttr->VeAttr.AttrMjpeg.mPicHeight;
            break;
        }
        case PT_H264:
        {
            pDstSize->Width = pAttr->VeAttr.AttrH264e.PicWidth;
            pDstSize->Height = pAttr->VeAttr.AttrH264e.PicHeight;
            break;
        }
        case PT_H265:
        {
            pDstSize->Width = pAttr->VeAttr.AttrH265e.mPicWidth;
            pDstSize->Height = pAttr->VeAttr.AttrH265e.mPicHeight;
            break;
        }
        default:
        {
            aloge("fatal error! unknown encode type:%d", pAttr->VeAttr.Type);
            ret = FAILURE;
            break;
        }
    }
    return ret;
}

ERRORTYPE SetEncodeDstSizeToVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pChnAttr, SIZE_S *pDstSize)
{
    ERRORTYPE ret = SUCCESS;
    if(NULL == pDstSize || NULL == pChnAttr)
    {
        alogw("Be careful! null pointer!");
        return FAILURE;
    }
    switch (pChnAttr->VeAttr.Type)
    {
        case PT_JPEG:
        {
            pChnAttr->VeAttr.AttrJpeg.PicWidth = pDstSize->Width;
            pChnAttr->VeAttr.AttrJpeg.PicHeight = pDstSize->Height;
            break;
        }
        case PT_MJPEG:
        {
            pChnAttr->VeAttr.AttrMjpeg.mPicWidth = pDstSize->Width;
            pChnAttr->VeAttr.AttrMjpeg.mPicHeight = pDstSize->Height;
            break;
        }
        case PT_H264:
        {
            pChnAttr->VeAttr.AttrH264e.PicWidth = pDstSize->Width;
            pChnAttr->VeAttr.AttrH264e.PicHeight = pDstSize->Height;
            break;
        }
        case PT_H265:
        {
            pChnAttr->VeAttr.AttrH265e.mPicWidth = pDstSize->Width;
            pChnAttr->VeAttr.AttrH265e.mPicHeight = pDstSize->Height;
            break;
        }
        default:
        {
            aloge("fatal error! unknown encode type:%d", pChnAttr->VeAttr.Type);
            ret = FAILURE;
            break;
        }
    }
    return ret;
}

int GetIQpOffsetFromVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pChnAttr)
{
    int nIQpOffset = 0;
    switch (pChnAttr->VeAttr.Type)
    {
        case PT_H264:
        {
            nIQpOffset = pChnAttr->VeAttr.AttrH264e.IQpOffset;
            break;
        }
        case PT_H265:
        {
            nIQpOffset = pChnAttr->VeAttr.AttrH265e.IQpOffset;
            break;
        }
        case PT_MJPEG:
        {
            break;
        }
        default:
        {
            aloge("fatal error! unknown encode type:%d", pChnAttr->VeAttr.Type);
            break;
        }
    }
    return nIQpOffset;
}

BOOL GetFastEncFlagFromVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pChnAttr)
{
    BOOL bFastEncFlag = FALSE;
    switch (pChnAttr->VeAttr.Type)
    {
        case PT_H264:
        {
            bFastEncFlag = pChnAttr->VeAttr.AttrH264e.FastEncFlag;
            break;
        }
        case PT_H265:
        {
            bFastEncFlag = pChnAttr->VeAttr.AttrH265e.mFastEncFlag;
            break;
        }
        case PT_MJPEG:
        {
            break;
        }
        default:
        {
            aloge("fatal error! unknown encode type:%d", pChnAttr->VeAttr.Type);
            break;
        }
    }
    return bFastEncFlag;
}

BOOL GetPIntraEnableFromVENC_CHN_ATTR_S(VENC_CHN_ATTR_S *pChnAttr)
{
    BOOL bPIntraEnable = FALSE;
    switch (pChnAttr->VeAttr.Type)
    {
        case PT_H264:
        {
            bPIntraEnable = pChnAttr->VeAttr.AttrH264e.mbPIntraEnable;
            break;
        }
        case PT_H265:
        {
            bPIntraEnable = pChnAttr->VeAttr.AttrH265e.mbPIntraEnable;
            break;
        }
        case PT_MJPEG:
        {
            break;
        }
        default:
        {
            aloge("fatal error! unknown encode type:%d", pChnAttr->VeAttr.Type);
            break;
        }
    }
    return bPIntraEnable;
}

AUDIO_SAMPLE_RATE_E map_SampleRate_to_AUDIO_SAMPLE_RATE_E(unsigned int nSampleRate)
{
    AUDIO_SAMPLE_RATE_E eSampleRate;
    switch(nSampleRate)
    {
        case 8000:
        {
            eSampleRate = AUDIO_SAMPLE_RATE_8000;
            break;
        }
        case 16000:
        {
            eSampleRate = AUDIO_SAMPLE_RATE_16000;
            break;
        }
        case 44100:
        {
            eSampleRate = AUDIO_SAMPLE_RATE_44100;
            break;
        }
        case 48000:
        {
            eSampleRate = AUDIO_SAMPLE_RATE_48000;
            break;
        }
        default:
        {
            aloge("fatal error! unsupport sampleRate[%ld]", nSampleRate);
            eSampleRate = AUDIO_SAMPLE_RATE_16000;
            break;
        }
    }
    return eSampleRate;
}

AUDIO_BIT_WIDTH_E map_BitWidth_to_AUDIO_BIT_WIDTH_E(unsigned int nBitWidth)
{
    AUDIO_BIT_WIDTH_E eBitWidth = AUDIO_BIT_WIDTH_16;
    switch(nBitWidth)
    {
        case 8:
        {
            eBitWidth = AUDIO_BIT_WIDTH_8;
            break;
        }
        case 16:
        {
            eBitWidth = AUDIO_BIT_WIDTH_16;
            break;
        }
        case 24:
        {
            eBitWidth = AUDIO_BIT_WIDTH_24;
            break;
        }
        case 32:
        {
            eBitWidth = AUDIO_BIT_WIDTH_32;
            break;
        }
        default:
        {
            aloge("fatal error! unsupport bit width[%ld]", nBitWidth);
            eBitWidth = AUDIO_BIT_WIDTH_16;
            break;
        }
    }
    return eBitWidth;
}

unsigned int map_AUDIO_SAMPLE_RATE_E_to_SampleRate(AUDIO_SAMPLE_RATE_E eSampleRate)
{
    unsigned int nSampleRate = (unsigned int)eSampleRate;
    return nSampleRate;
}
unsigned int map_AUDIO_BIT_WIDTH_E_to_BitWidth(AUDIO_BIT_WIDTH_E eBitWidth)
{
    unsigned int nBitWidth = (eBitWidth+1)*8;
    return nBitWidth;
}

/**
  judge audio channel number by sound mode.

  @param pnMicNum, pnRefNum
    store MicIn channel number and RefIn channel number
  @return
    total audio channel number, i.d, MicIn + RefIn
*/
int judgeAudioChnNumBySoundMode(AUDIO_SOUND_MODE_E eSoundmode, int *pnMicNum, int *pnRefNum)
{
    int nChnNum;
    if(AUDIO_SOUND_MODE_MONO == eSoundmode)
    {
        if(pnMicNum)
        {
            *pnMicNum = 1;
        }
        if(pnRefNum)
        {
            *pnRefNum = 0;
        }
        nChnNum = 1;
    }
    else if(AUDIO_SOUND_MODE_STEREO == eSoundmode)
    {
        if(pnMicNum)
        {
            *pnMicNum = 2;
        }
        if(pnRefNum)
        {
            *pnRefNum = 0;
        }
        nChnNum = 2;
    }
    else if(AUDIO_SOUND_MODE_AW_1Chn1Ref == eSoundmode)
    {
        if(pnMicNum)
        {
            *pnMicNum = 1;
        }
        if(pnRefNum)
        {
            *pnRefNum = 1;
        }
        nChnNum = 2;
    }
    else if(AUDIO_SOUND_MODE_AW_2Chn1Ref == eSoundmode)
    {
        if(pnMicNum)
        {
            *pnMicNum = 2;
        }
        if(pnRefNum)
        {
            *pnRefNum = 1;
        }
        nChnNum = 3;
    }
    else
    {
        aloge("fatal error! not support sound mode:%d", eSoundmode);
        if(pnMicNum)
        {
            *pnMicNum = 1;
        }
        if(pnRefNum)
        {
            *pnRefNum = 0;
        }
        nChnNum = 1;
    }
    return nChnNum;
}

static VENC_H264_NALU_TYPE map_H264E_NALU_TYPE_E_to_VENC_H264_NALU_TYPE(H264E_NALU_TYPE_E eMppType)
{
    VENC_H264_NALU_TYPE eVencType;
    switch(eMppType)
    {
        case H264E_NALU_PSLICE:
        {
            eVencType = VENC_H264_NALU_PSLICE;
            break;
        }
        case H264E_NALU_ISLICE:
        {
            eVencType = VENC_H264_NALU_ISLICE;
            break;
        }
        case H264E_NALU_SEI:
        {
            eVencType = VENC_H264_NALU_SEI;
            break;
        }
        case H264E_NALU_SPS:
        {
            eVencType = VENC_H264_NALU_SPS;
            break;
        }
        case H264E_NALU_PPS:
        {
            eVencType = VENC_H264_NALU_PPS;
            break;
        }
        case H264E_NALU_IPSLICE:
        {
            eVencType = VENC_H264_NALU_IPSLICE;
            break;
        }
        default:
        {
            aloge("fatal error! unknown h264 nalu type:%d", eMppType);
            eVencType = VENC_H264_NALU_ISLICE;
            break;
        }
    }
    return eVencType;
}
static VENC_H265_NALU_TYPE map_H265E_NALU_TYPE_E_to_VENC_H265_NALU_TYPE(H265E_NALU_TYPE_E eMppType)
{
    VENC_H265_NALU_TYPE eVencType;
    switch(eMppType)
    {
        case H265E_NALU_PSLICE:
        {
            eVencType = VENC_H265_NALU_PSLICE;
            break;
        }
        case H265E_NALU_ISLICE:
        {
            eVencType = VENC_H265_NALU_ISLICE;
            break;
        }
        case H265E_NALU_VPS:
        {
            eVencType = VENC_H265_NALU_VPS;
            break;
        }
        case H265E_NALU_SPS:
        {
            eVencType = VENC_H265_NALU_SPS;
            break;
        }
        case H265E_NALU_PPS:
        {
            eVencType = VENC_H265_NALU_PPS;
            break;
        }
        case H265E_NALU_SEI:
        {
            eVencType = VENC_H265_NALU_SEI;
            break;
        }
        default:
        {
            aloge("fatal error! unknown h265 nalu type:%d", eMppType);
            eVencType = VENC_H265_NALU_ISLICE;
            break;
        }
    }
    return eVencType;
}
static VENC_JPEGE_PACK_TYPE map_JPEGE_PACK_TYPE_E_to_VENC_JPEGE_PACK_TYPE(JPEGE_PACK_TYPE_E eMppType)
{
    VENC_JPEGE_PACK_TYPE eVencType;
    switch(eMppType)
    {
        case JPEGE_PACK_ECS:
        {
            eVencType = VENC_JPEG_PACK_ECS;
            break;
        }
        case JPEGE_PACK_APP:
        {
            eVencType = VENC_JPEG_PACK_APP;
            break;
        }
        case JPEGE_PACK_VDO:
        {
            eVencType = VENC_JPEG_PACK_VDO;
            break;
        }
        case JPEGE_PACK_PIC:
        {
            eVencType = VENC_JPEG_PACK_PIC;
            break;
        }
        default:
        {
            aloge("fatal error! unknown jpeg pack type:%d", eMppType);
            eVencType = VENC_JPEG_PACK_ECS;
            break;
        }
    }
    return eVencType;
}
VENC_OUTPUT_PACK_TYPE map_VENC_DATA_TYPE_U_to_VENC_OUTPUT_PACK_TYPE(VENC_DATA_TYPE_U UMppPackType, PAYLOAD_TYPE_E ePayloadType)
{
    VENC_OUTPUT_PACK_TYPE UVencPackType;
    switch(ePayloadType)
    {
        case PT_H264:
        {
            UVencPackType.eH264Type = map_H264E_NALU_TYPE_E_to_VENC_H264_NALU_TYPE(UMppPackType.enH264EType);
            break;
        }
        case PT_H265:
        {
            UVencPackType.eH265Type = map_H265E_NALU_TYPE_E_to_VENC_H265_NALU_TYPE(UMppPackType.enH265EType);
            break;
        }
        case PT_JPEG:
        case PT_MJPEG:
        {
            UVencPackType.eJpegType = map_JPEGE_PACK_TYPE_E_to_VENC_JPEGE_PACK_TYPE(UMppPackType.enJPEGEType);
            break;
        }
        default:
        {
            aloge("fatal error! unknown payload type:%d", ePayloadType);
            UVencPackType.eH264Type = VENC_H264_NALU_ISLICE;
            break;
        }
    }
    return UVencPackType;
}

static H264E_NALU_TYPE_E map_VENC_H264_NALU_TYPE_to_H264E_NALU_TYPE_E(VENC_H264_NALU_TYPE eVencType)
{
    H264E_NALU_TYPE_E eMppType;
    switch(eVencType)
    {
        case VENC_H264_NALU_PSLICE:
        {
            eMppType = H264E_NALU_PSLICE;
            break;
        }
        case VENC_H264_NALU_ISLICE:
        {
            eMppType = H264E_NALU_ISLICE;
            break;
        }
        case VENC_H264_NALU_SEI:
        {
            eMppType = H264E_NALU_SEI;
            break;
        }
        case VENC_H264_NALU_SPS:
        {
            eMppType = H264E_NALU_SPS;
            break;
        }
        case VENC_H264_NALU_PPS:
        {
            eMppType = H264E_NALU_PPS;
            break;
        }
        case VENC_H264_NALU_IPSLICE:
        {
            eMppType = H264E_NALU_IPSLICE;
            break;
        }
        default:
        {
            aloge("fatal error! unknown venc h264 nalu type:%d", eVencType);
            eMppType = H264E_NALU_ISLICE;
            break;
        }
    }
    return eMppType;
}
static H265E_NALU_TYPE_E map_VENC_H265_NALU_TYPE_to_H265E_NALU_TYPE_E(VENC_H265_NALU_TYPE eVencType)
{
    H265E_NALU_TYPE_E eMppType;
    switch(eVencType)
    {
        case VENC_H265_NALU_PSLICE:
        {
            eMppType = H265E_NALU_PSLICE;
            break;
        }
        case VENC_H265_NALU_ISLICE:
        {
            eMppType = H265E_NALU_ISLICE;
            break;
        }
        case VENC_H265_NALU_VPS:
        {
            eMppType = H265E_NALU_VPS;
            break;
        }
        case VENC_H265_NALU_SPS:
        {
            eMppType = H265E_NALU_SPS;
            break;
        }
        case VENC_H265_NALU_PPS:
        {
            eMppType = H265E_NALU_PPS;
            break;
        }
        case VENC_H265_NALU_SEI:
        {
            eMppType = H265E_NALU_SEI;
            break;
        }
        default:
        {
            aloge("fatal error! unknown venc h265 nalu type:%d", eVencType);
            eMppType = H265E_NALU_ISLICE;
            break;
        }
    }
    return eMppType;
}
static JPEGE_PACK_TYPE_E map_VENC_JPEGE_PACK_TYPE_to_JPEGE_PACK_TYPE_E(VENC_JPEGE_PACK_TYPE eVencType)
{
    JPEGE_PACK_TYPE_E eMppType;
    switch(eVencType)
    {
        case VENC_JPEG_PACK_ECS:
        {
            eMppType = JPEGE_PACK_ECS;
            break;
        }
        case VENC_JPEG_PACK_APP:
        {
            eMppType = JPEGE_PACK_APP;
            break;
        }
        case VENC_JPEG_PACK_VDO:
        {
            eMppType = JPEGE_PACK_VDO;
            break;
        }
        case VENC_JPEG_PACK_PIC:
        {
            eMppType = JPEGE_PACK_PIC;
            break;
        }
        default:
        {
            aloge("fatal error! unknown jpeg pack type:%d", eVencType);
            eMppType = JPEGE_PACK_ECS;
            break;
        }
    }
    return eMppType;
}
VENC_DATA_TYPE_U map_VENC_OUTPUT_PACK_TYPE_to_VENC_DATA_TYPE_U(VENC_OUTPUT_PACK_TYPE UVencPackType, PAYLOAD_TYPE_E ePayloadType)
{
    VENC_DATA_TYPE_U UMppPackType;
    switch(ePayloadType)
    {
        case PT_H264:
        {
            UMppPackType.enH264EType = map_VENC_H264_NALU_TYPE_to_H264E_NALU_TYPE_E(UVencPackType.eH264Type);
            break;
        }
        case PT_H265:
        {
            UMppPackType.enH265EType = map_VENC_H265_NALU_TYPE_to_H265E_NALU_TYPE_E(UVencPackType.eH265Type);
            break;
        }
        case PT_JPEG:
        case PT_MJPEG:
        {
            UMppPackType.enJPEGEType = map_VENC_JPEGE_PACK_TYPE_to_JPEGE_PACK_TYPE_E(UVencPackType.eJpegType);
            break;
        }
        default:
        {
            aloge("fatal error! unknown payload type:%d", ePayloadType);
            UMppPackType.enJPEGEType = H264E_NALU_ISLICE;
            break;
        }
    }
    return UMppPackType;
}

