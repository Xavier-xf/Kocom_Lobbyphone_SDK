/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/

/**
                        abbreviation description

    avsync_rc_swfv: rc: recRenderSink
                    swf: switch file
                    v: video duration is enough, prepare to switch file.
    avsync_rc_swfa: audio duration is enough, prepare to switch file
    avsync_ch_v: ch: cache.
                 v: video
    avsync_cal: cal: calculate. calculate number of video frames which are written more.
    mVideoPtsWriteMoreSt: St: start.
    avsync_bk1: bk: break.
*/

//#include <CDX_LogNDebug.h>
//#define LOG_NDEBUG 0
#define LOG_TAG "RecRender_Component"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <math.h>
#include <sys/prctl.h>

#include <tmessage.h>
#include <tsemaphore.h>

//#include <type.h>
//#include <H264encLibApi.h>
#include <vencoder.h>
#include <aenc_sw_lib.h>
#include <TextEncApi.h>
#include <mp4_mux_lib.h>
#include <sa_config.h>
//#include <cedarv_osal_linux.h>
//#include <type_camera.h>
#include <record_writer.h>
//#include <include_system/cedarx_avs_counter.h>
//#include <DataQuene.h>
#include <FsWriter.h>
//#include "RecRenderSink.h"
//#include "RecRender_cache.h"

#include <SystemBase.h>
//#include <aenc_sw_lib.h>
#include <cdx_list.h>

#include "RecRender_Component.h"
#include <EncodedStream.h>
#include <VencCompStream.h>
#include <AencCompStream.h>
#include <media_common.h>
#include <media_common_vcodec.h>
#include <media_common_acodec.h>
//#include <media_debug.h>

//#define __SAVE_INPUT_BS__
#ifdef __SAVE_INPUT_BS__
static FILE *fp_bs;
#endif

#define RECSINK_MAX_PACKET_NUM  (256)   //(BITSTREAM_FRAME_SIZE+FIFO_LEVEL)
#define NOTIFY_NEEDNEXTFD_IN_ADVANCE (10*1000)  //ms

static void* RecRender_ComponentThread(void* pThreadData);
//extern ERRORTYPE RecRender_ReleaseBuffer(PARAM_IN RECRENDERDATATYPE *pRecRenderData, PARAM_IN RecSinkPacket* pRSPacket);

ERRORTYPE copy_MUX_CHN_ATTR_S(MUX_CHN_ATTR_S *pDst, MUX_CHN_ATTR_S *pSrc)
{
    memcpy(pDst, pSrc, sizeof(MUX_CHN_ATTR_S));
    return SUCCESS;
}

MEDIA_FILE_FORMAT_E map_MUXERMODES_to_MEDIA_FILE_FORMAT_E(MUXERMODES nMuxerMode)
{
    MEDIA_FILE_FORMAT_E dstFormat;
    switch(nMuxerMode)
    {
        case MUXER_MODE_MP4:
            dstFormat = MEDIA_FILE_FORMAT_MP4;
            break;
        case MUXER_MODE_MP3:
            dstFormat = MEDIA_FILE_FORMAT_MP3;
            break;
        case MUXER_MODE_AAC:
            dstFormat = MEDIA_FILE_FORMAT_AAC;
            break;
        case MUXER_MODE_WAV:
            dstFormat = MEDIA_FILE_FORMAT_WAV;
            break;
        case MUXER_MODE_RAW:
            dstFormat = MEDIA_FILE_FORMAT_RAW;
            break;
        case MUXER_MODE_TS:
            dstFormat = MEDIA_FILE_FORMAT_TS;
            break;
        default:
            dstFormat = MEDIA_FILE_FORMAT_UNKNOWN;
            break;
    }
    return dstFormat;
}

MUXERMODES map_MEDIA_FILE_FORMAT_E_to_MUXERMODES(MEDIA_FILE_FORMAT_E nMuxFileFormat)
{
    MUXERMODES dstFormat;
    switch(nMuxFileFormat)
    {
        case MEDIA_FILE_FORMAT_MP4:
            dstFormat = MUXER_MODE_MP4;
            break;
        case MEDIA_FILE_FORMAT_MP3:
            dstFormat = MUXER_MODE_MP3;
            break;
        case MEDIA_FILE_FORMAT_AAC:
            dstFormat = MUXER_MODE_AAC;
            break;
        case MEDIA_FILE_FORMAT_WAV:
            dstFormat = MUXER_MODE_WAV;
            break;
        case MEDIA_FILE_FORMAT_RAW:
            dstFormat = MUXER_MODE_RAW;
            break;
        case MEDIA_FILE_FORMAT_TS:
            dstFormat = MUXER_MODE_TS;
            break;
        default:
            dstFormat = MUXER_MODE_MP4;
            break;
    }
    return dstFormat;
}

static int CDXRecoder_WritePacket_CB(void *parent, CDXRecorderBsInfo *bs_info)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE*)parent;
    return pRecRenderData->pCallbacks->EventHandler(
                            pRecRenderData->hSelf,
                            pRecRenderData->pAppData,
                            COMP_EventBsframeAvailable,
                            0,
                            0,
                            bs_info);
}

ERRORTYPE setRecSinkPacketByVEncCompOutputBuffer(
            PARAM_IN RECRENDERDATATYPE *pRecRenderData,
            PARAM_OUT RecSinkPacket* pRSPacket,
            PARAM_IN EncodedStream* pEncodedStream,
            PARAM_IN int PortIndex)
{
    if(pRecRenderData->mnBasePts < 0)
    {
        pRecRenderData->mnBasePts = pEncodedStream->nTimeStamp;
    }

    int iSize0 = 0;
    int iSize1 = 0;
    if (pEncodedStream->nFilledLen > 0)
    {
        if (pEncodedStream->nFilledLen <= pEncodedStream->nBufferLen)
        {
            iSize0 = pEncodedStream->nFilledLen;
            iSize1 = 0;
        }
        else
        {
            iSize0 = pEncodedStream->nBufferLen;
            iSize1 = pEncodedStream->nFilledLen - pEncodedStream->nBufferLen;
        }
    }

    pRSPacket->mId = pEncodedStream->nID;
    pRSPacket->mStreamType = CODEC_TYPE_VIDEO;
    pRSPacket->mFlags = 0;
    pRSPacket->mFlags |= (pEncodedStream->nFlags & CEDARV_FLAG_KEYFRAME)? AVPACKET_FLAG_KEYFRAME : 0;
    pRSPacket->mPts = pEncodedStream->nTimeStamp/* - pRecRenderData->mnBasePts*/;
    if(iSize0 > 0)
    {
        pRSPacket->mpData0 = (char*)pEncodedStream->pBuffer;
    }
    else
    {
        pRSPacket->mpData0 = NULL;
    }
    pRSPacket->mSize0 = iSize0;
    if(iSize1 > 0)
    {
        pRSPacket->mpData1 = (char*)pEncodedStream->pBufferExtra;
    }
    else
    {
        pRSPacket->mpData1 = NULL;
    }
    pRSPacket->mSize1 = iSize1;
    pRSPacket->mCurrQp = pEncodedStream->video_frame_info.CurrQp;
    pRSPacket->mavQp = pEncodedStream->video_frame_info.avQp;
    pRSPacket->mnGopIndex = pEncodedStream->video_frame_info.nGopIndex;
    pRSPacket->mnFrameIndex = pEncodedStream->video_frame_info.nFrameIndex;
    pRSPacket->mnTotalIndex = pEncodedStream->video_frame_info.nTotalIndex;

    pRSPacket->nPackNum = pEncodedStream->nPackNum;
    memcpy(pRSPacket->stPackInfos, pEncodedStream->stPackInfos, sizeof(VencPackInfo)*MAX_OUTPUT_PACK_NUM);

    //pRSPacket->mSourceType = SOURCE_TYPE_COMPONENT;
    pRSPacket->mRefCnt = 0;

    pRSPacket->mStreamId = PortIndex;

    return SUCCESS;
}

ERRORTYPE setRecSinkPacketByAEncCompOutputBuffer(PARAM_IN RECRENDERDATATYPE *pRecRenderData, PARAM_OUT RecSinkPacket* pRSPacket, PARAM_IN EncodedStream* pEncodedStream, int PortIndex)
{
    if(pRecRenderData->mnAudioBasePts < 0)
    {
        pRecRenderData->mnAudioBasePts = pEncodedStream->nTimeStamp;
    }
    pRSPacket->mId = pEncodedStream->nID;
    pRSPacket->mStreamType = CODEC_TYPE_AUDIO;
    pRSPacket->mFlags = 0;
    pRSPacket->mPts = pEncodedStream->nTimeStamp/* - pRecRenderData->mnAudioBasePts*/;
    pRSPacket->mpData0 = (char*)pEncodedStream->pBuffer;
    pRSPacket->mSize0 = pEncodedStream->nFilledLen;
    pRSPacket->mpData1 =  (char*)pEncodedStream->pBufferExtra;
    pRSPacket->mSize1 = (int)pEncodedStream->nBufferExtraLen;
    pRSPacket->mCurrQp = 0;
    pRSPacket->mavQp = 0;
    pRSPacket->mnGopIndex = 0;
    pRSPacket->mnFrameIndex = 0;
    pRSPacket->mnTotalIndex = 0;
    //pRSPacket->mSourceType = SOURCE_TYPE_COMPONENT;
    pRSPacket->mRefCnt = 0;

    pRSPacket->mStreamId = PortIndex;
    return SUCCESS;
}

ERRORTYPE setRecSinkPacketByTEncCompOutputBuffer(PARAM_IN RECRENDERDATATYPE *pRecRenderData, PARAM_OUT RecSinkPacket* pRSPacket, PARAM_IN EncodedStream* pEncodedStream, int PortIndex)
{
    if(pRecRenderData->mnTextBasePts < 0)
    {
        pRecRenderData->mnTextBasePts = pEncodedStream->nTimeStamp;
    }
    pRSPacket->mId = pEncodedStream->nID;
    pRSPacket->mStreamType = CODEC_TYPE_TEXT;
    pRSPacket->mFlags = 0;
    pRSPacket->mPts = pEncodedStream->nTimeStamp/* - pRecRenderData->mnTextBasePts*/;
    pRSPacket->mpData0 = (char*)pEncodedStream->pBuffer;
    pRSPacket->mSize0 = (int)pEncodedStream->nBufferLen;
    pRSPacket->mpData1 = (char*)pEncodedStream->pBufferExtra;
    pRSPacket->mSize1 = (int)pEncodedStream->nBufferExtraLen;
    pRSPacket->mCurrQp = 0;
    pRSPacket->mavQp = 0;
    pRSPacket->mnGopIndex = 0;
    pRSPacket->mnFrameIndex = 0;
    pRSPacket->mnTotalIndex = 0;
    //pRSPacket->mSourceType = SOURCE_TYPE_COMPONENT;
    pRSPacket->mRefCnt = 0;

    pRSPacket->mStreamId = PortIndex;
    return SUCCESS;
}

ERRORTYPE RecRender_GetVideoBuffer(
        PARAM_IN RECRENDERDATATYPE *pRecRenderData,
        PARAM_OUT RecSinkPacket* pRSPacket)
{
    ERRORTYPE eError;
    pthread_mutex_lock(&pRecRenderData->mVideoInputFrameListMutex);
    if (!list_empty(&pRecRenderData->mVideoInputFrameReadyList))
    {
        ENCODER_NODE_T *pEntry = list_first_entry(&pRecRenderData->mVideoInputFrameReadyList, ENCODER_NODE_T, mList);
        setRecSinkPacketByVEncCompOutputBuffer(pRecRenderData, pRSPacket, &pEntry->stEncodedStream, pEntry->mPortIndex);
        pEntry->mUsedRefCnt = 1;
        list_move_tail(&pEntry->mList, &pRecRenderData->mVideoInputFrameUsedList);
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_MUX_NOMEM;
    }
    pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
    return eError;
}

ERRORTYPE RecRender_GetAudioBuffer(
        PARAM_IN RECRENDERDATATYPE *pRecRenderData,
        PARAM_INOUT RecSinkPacket* pRSPacket)

{
    ERRORTYPE eError;
    pthread_mutex_lock(&pRecRenderData->mAudioInputFrameListMutex);
    if (!list_empty(&pRecRenderData->mAudioInputFrameReadyList))
    {
        ENCODER_NODE_T *pEntry = list_first_entry(&pRecRenderData->mAudioInputFrameReadyList, ENCODER_NODE_T, mList);
        setRecSinkPacketByAEncCompOutputBuffer(pRecRenderData, pRSPacket, &pEntry->stEncodedStream, pEntry->mPortIndex);
        pEntry->mUsedRefCnt = 1;
        list_move_tail(&pEntry->mList, &pRecRenderData->mAudioInputFrameUsedList);
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_MUX_NOMEM;
    }
    pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
    return eError;
}
ERRORTYPE RecRender_GetTextBuffer(
        PARAM_IN RECRENDERDATATYPE *pRecRenderData,
        PARAM_INOUT RecSinkPacket* pRSPacket)
{
    ERRORTYPE eError;
    pthread_mutex_lock(&pRecRenderData->mTextInputFrameListMutex);
    if(!list_empty(&pRecRenderData->mTextInputFrameReadyList))
    {
        ENCODER_NODE_T *pEntry = list_first_entry(&pRecRenderData->mTextInputFrameReadyList, ENCODER_NODE_T, mList);
        setRecSinkPacketByTEncCompOutputBuffer(pRecRenderData, pRSPacket, &pEntry->stEncodedStream, pEntry->mPortIndex);
        pEntry->mUsedRefCnt = 1;
        list_move_tail(&pEntry->mList, &pRecRenderData->mTextInputFrameUsedList);
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_MUX_NOMEM;
    }
    pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
    return eError;
}

ERRORTYPE RecRender_RefBuffer(
        PARAM_IN RECRENDERDATATYPE *pRecRenderData,
        PARAM_IN RecSinkPacket* pRSPacket)
{
    ERRORTYPE eError = SUCCESS;
    if(CODEC_TYPE_VIDEO == pRSPacket->mStreamType)
    {
        pthread_mutex_lock(&pRecRenderData->mVideoInputFrameListMutex);
        ENCODER_NODE_T *pEntry;
        BOOL bFindFlag = FALSE;
        list_for_each_entry(pEntry, &pRecRenderData->mVideoInputFrameUsedList, mList)
        {
            if((pEntry->stEncodedStream.nID == pRSPacket->mId) && (pEntry->mPortIndex == pRSPacket->mStreamId))
            {
                bFindFlag = TRUE;
                break;
            }
        }
        if (bFindFlag)
        {
            pEntry->mUsedRefCnt++;
        }
        else
        {
            aloge("fatal error! not find vFrmId[%d-%d-%d] in used list.", pRSPacket->mStreamId, pRSPacket->mId, pRSPacket->mRefCnt);
            eError = ERR_MUX_UNEXIST;
        }
        pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
    }
    else if(CODEC_TYPE_AUDIO == pRSPacket->mStreamType)
    {
        pthread_mutex_lock(&pRecRenderData->mAudioInputFrameListMutex);
        ENCODER_NODE_T *pEntry;
        BOOL bFindFlag = FALSE;
        list_for_each_entry(pEntry, &pRecRenderData->mAudioInputFrameUsedList, mList)
        {
            if(pEntry->stEncodedStream.nID == pRSPacket->mId)
            {
                bFindFlag = TRUE;
                break;
            }
        }
        if(bFindFlag)
        {
            pEntry->mUsedRefCnt++;
        }
        else
        {
            aloge("fatal error! not find AFrmId[%d] in used list.", pRSPacket->mId);
            eError = ERR_MUX_UNEXIST;
        }
        pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
    }
    else if(CODEC_TYPE_TEXT == pRSPacket->mStreamType)
    {
        pthread_mutex_lock(&pRecRenderData->mTextInputFrameListMutex);
        ENCODER_NODE_T *pEntry;
        BOOL bFindFlag = FALSE;
        list_for_each_entry(pEntry, &pRecRenderData->mTextInputFrameUsedList, mList)
        {
            if(pEntry->stEncodedStream.nID == pRSPacket->mId)
            {
                bFindFlag = TRUE;
                break;
            }
        }
        if(bFindFlag)
        {
            pEntry->mUsedRefCnt++;
        }
        else
        {
            aloge("fatal error! not find TFrmId[%d] in used list.", pRSPacket->mId);
            eError = ERR_MUX_UNEXIST;
        }
        pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
    }
    else
    {
        aloge("fatal error! invalid streamType[%d]", pRSPacket->mStreamType);
        eError = ERR_MUX_UNEXIST;
    }
    return eError;
}
ERRORTYPE RecRender_ReleaseBuffer(
        PARAM_IN RECRENDERDATATYPE *pRecRenderData,
        PARAM_IN RecSinkPacket* pRSPacket)
{
    ERRORTYPE eRet = SUCCESS;
    if (CODEC_TYPE_AUDIO == pRSPacket->mStreamType)
    {
        //alogd("pRSPacket->mStreamId: %d", pRSPacket->mStreamId);
    }
    if (CODEC_TYPE_VIDEO == pRSPacket->mStreamType)
    {
        pthread_mutex_lock(&pRecRenderData->mVideoInputFrameListMutex);
        ENCODER_NODE_T *pEntry;
        BOOL bFindFlag = FALSE;
        list_for_each_entry(pEntry, &pRecRenderData->mVideoInputFrameUsedList, mList)
        {
            if((pEntry->stEncodedStream.nID == pRSPacket->mId) && (pEntry->mPortIndex == pRSPacket->mStreamId))
            {
                bFindFlag = TRUE;
                //alogd("pEntry->mPortIndex: %d", pEntry->mPortIndex);
                break;
            }
            else
            {
//                if((pEntry->stEncodedStream.nID == pRSPacket->mId) && (pEntry->mPortIndex != pRSPacket->mStreamId))
//                {
//                    alogd("we catch it! vframeId[%d-%d-%d]!=[%d-%d-%d]", 
//                        pRSPacket->mStreamId, pRSPacket->mId, pRSPacket->mRefCnt, pEntry->mPortIndex, pEntry->stEncodedStream.nID, pEntry->mUsedRefCnt);
//                }
            }
        }
        if(bFindFlag)
        {
            pEntry->mUsedRefCnt--;
            if (pEntry->mUsedRefCnt == 0)
            {
                list_del(&pEntry->mList);
                //list_move_tail(&pEntry->mList, &pRecRenderData->mVideoInputFrameIdleList);
                pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);

                if(pRecRenderData->mInputPortTunnelFlag[pRSPacket->mStreamId])
                {
                    //MM_COMPONENTTYPE *pInPortTunnelComp = (MM_COMPONENTTYPE*)(pRecRenderData->sInPortTunnelInfo[RECR_PORT_INDEX_VIDEO].hTunnel);
                    MM_COMPONENTTYPE *pInPortTunnelComp = (MM_COMPONENTTYPE*)(pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].hTunnel);
                    COMP_BUFFERHEADERTYPE obh;
                    //obh.nOutputPortIndex   = pRecRenderData->sInPortTunnelInfo[RECR_PORT_INDEX_VIDEO].nTunnelPortIndex;
                    //obh.nInputPortIndex    = pRecRenderData->sInPortTunnelInfo[RECR_PORT_INDEX_VIDEO].nPortIndex;

                    obh.nOutputPortIndex = pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].nTunnelPortIndex;
                    obh.nInputPortIndex = pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].nPortIndex;
                    obh.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
                    
                    //alogd("VideoBuff StreamId: %d", pRSPacket->mStreamId);
                    eRet = COMP_FillThisBuffer(pInPortTunnelComp, &obh);
                    if(eRet != SUCCESS)
                    {
                        aloge("fatal error! muxChn[%d] fill this buffer fail[0x%x], video frame id=[%d-%d-%d], check code!",
                            pRecRenderData->mMppChnInfo.mChnId,
                            eRet, pEntry->mPortIndex, pEntry->stEncodedStream.nID, pEntry->mUsedRefCnt);
                        pthread_mutex_lock(&pRecRenderData->mVideoInputFrameListMutex);
                        list_add_tail(&pEntry->mList, &pRecRenderData->mVideoInputFrameUsedList);
                        //list_move_tail(&pEntry->mList, &pRecRenderData->mVideoInputFrameUsedList);
                        pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
                    }
                    else
                    {
                        pthread_mutex_lock(&pRecRenderData->mVideoInputFrameListMutex);
                        list_add_tail(&pEntry->mList, &pRecRenderData->mVideoInputFrameIdleList);
                        pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
                    }
                }
                else
                {
                    COMP_BUFFERHEADERTYPE obh;
                    obh.nInputPortIndex = pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].nPortIndex;
                    obh.pAppPrivate = (void*)&pEntry->stEncodedStream;
                    if(pEntry->mPortIndex != obh.nInputPortIndex)
                    {
                        aloge("fatal error! muxChn[%d] video streamId[%d!=%d]", pRecRenderData->mMppChnInfo.mChnId,
                            pEntry->mPortIndex, obh.nInputPortIndex);
                    }
                    pRecRenderData->pCallbacks->EmptyBufferDone(pRecRenderData->hSelf, pRecRenderData->pAppData, &obh);
                    pthread_mutex_lock(&pRecRenderData->mVideoInputFrameListMutex);
                    list_add_tail(&pEntry->mList, &pRecRenderData->mVideoInputFrameIdleList);
                    pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
                }
            }
            else
            {
                if(pEntry->mUsedRefCnt < 0)
                {
                    aloge("fatal error! muxChn[%d] vfrm[%d-%d] usedRefCnt[%d]<0, check code!",
                        pRecRenderData->mMppChnInfo.mChnId,
                        pEntry->mPortIndex, pEntry->stEncodedStream.nID, pEntry->mUsedRefCnt);
                    eRet = ERR_MUX_NOT_PERM;
                }
                pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
            }
        }
        else
        {
            aloge("fatal error! muxChn[%d] not find VFrmId[%d-%d-%d] in used list.",
                pRecRenderData->mMppChnInfo.mChnId,
                pRSPacket->mStreamId, pRSPacket->mId, pRSPacket->mRefCnt);
            eRet = ERR_MUX_UNEXIST;
            pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
        }
    }
    else if(CODEC_TYPE_AUDIO == pRSPacket->mStreamType)
    {
        pthread_mutex_lock(&pRecRenderData->mAudioInputFrameListMutex);
        ENCODER_NODE_T *pEntry;
        BOOL bFindFlag = FALSE;
        list_for_each_entry(pEntry, &pRecRenderData->mAudioInputFrameUsedList, mList)
        {
            if(pEntry->stEncodedStream.nID == pRSPacket->mId)
            {
                bFindFlag = TRUE;
                break;
            }
        }

        if(bFindFlag)
        {
            pEntry->mUsedRefCnt--;
            if(pEntry->mUsedRefCnt == 0)
            {
                list_del(&pEntry->mList);
                //list_move_tail(&pEntry->mList, &pRecRenderData->mAudioInputFrameIdleList);
                pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
                if(pRecRenderData->mInputPortTunnelFlag[pRSPacket->mStreamId])
                {
                    MM_COMPONENTTYPE *pInPortTunnelComp = (MM_COMPONENTTYPE*)(pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].hTunnel);
                    COMP_BUFFERHEADERTYPE obh;
                    obh.nOutputPortIndex   = pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].nTunnelPortIndex;
                    obh.nInputPortIndex    = pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].nPortIndex;
                    obh.pOutputPortPrivate = (void*)&pEntry->stEncodedStream;
                    //alogd("Audio Buffer streamId: %d", pRSPacket->mStreamId);
                    eRet = COMP_FillThisBuffer(pInPortTunnelComp, &obh);
                    if (eRet != SUCCESS)
                    {
                        aloge("fatal error! muxChn[%d] fill this buffer fail[0x%x], audio frame id=[%d], check code!",
                            pRecRenderData->mMppChnInfo.mChnId,
                            eRet, pEntry->stEncodedStream.nID);
                        pthread_mutex_lock(&pRecRenderData->mAudioInputFrameListMutex);
                        list_add_tail(&pEntry->mList, &pRecRenderData->mAudioInputFrameUsedList);
                        //list_move_tail(&pEntry->mList, &pRecRenderData->mAudioInputFrameUsedList);
                        pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
                    }
                    else
                    {
                        pthread_mutex_lock(&pRecRenderData->mAudioInputFrameListMutex);
                        list_add_tail(&pEntry->mList, &pRecRenderData->mAudioInputFrameIdleList);
                        pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
                    }
                }
                else
                {
                    COMP_BUFFERHEADERTYPE obh;
                    obh.nInputPortIndex = pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].nPortIndex;
                    obh.pAppPrivate = (void*)&pEntry->stEncodedStream;
                    if(pEntry->mPortIndex != obh.nInputPortIndex)
                    {
                        aloge("fatal error! muxChn[%d] audio streamId[%d!=%d]", pRecRenderData->mMppChnInfo.mChnId,
                            pEntry->mPortIndex, obh.nInputPortIndex);
                    }
                    pRecRenderData->pCallbacks->EmptyBufferDone(pRecRenderData->hSelf, pRecRenderData->pAppData, &obh);
                    pthread_mutex_lock(&pRecRenderData->mAudioInputFrameListMutex);
                    list_add_tail(&pEntry->mList, &pRecRenderData->mAudioInputFrameIdleList);
                    pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
                }
            }
            else
            {
                if(pEntry->mUsedRefCnt < 0)
                {
                    aloge("fatal error! muxChn[%d] usedRefCnt[%d]<0, check code!",
                        pRecRenderData->mMppChnInfo.mChnId, pEntry->mUsedRefCnt);
                    eRet = ERR_MUX_NOT_PERM;
                }
                pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
            }
        }
        else
        {
            aloge("fatal error! muxChn[%d] not find AFrmId[%d] in used list.",
                pRecRenderData->mMppChnInfo.mChnId, pRSPacket->mId);
            eRet = ERR_MUX_UNEXIST;
            pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
        }
    }
    else if(CODEC_TYPE_TEXT == pRSPacket->mStreamType)
    {
        pthread_mutex_lock(&pRecRenderData->mTextInputFrameListMutex);
        ENCODER_NODE_T *pEntry;
        BOOL bFindFlag = FALSE;
        list_for_each_entry(pEntry, &pRecRenderData->mTextInputFrameUsedList, mList)
        {
            if(pEntry->stEncodedStream.nID == pRSPacket->mId)
            {
                bFindFlag = TRUE;
                break;
            }
        }
        if(bFindFlag)
        {
            pEntry->mUsedRefCnt--;
            if(pEntry->mUsedRefCnt == 0)
            {
                list_del(&pEntry->mList);
                //list_move_tail(&pEntry->mList, &pRecRenderData->mTextInputFrameIdleList);
                pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);

                if(pRecRenderData->mInputPortTunnelFlag[pRSPacket->mStreamId])
                {
                    MM_COMPONENTTYPE *pInPortTunnelComp = (MM_COMPONENTTYPE*)(pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].hTunnel);
                    COMP_BUFFERHEADERTYPE obh;
                    obh.nOutputPortIndex   = pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].nTunnelPortIndex;
                    obh.nInputPortIndex    = pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].nPortIndex;
                    obh.pOutputPortPrivate = (void*)pEntry;
                    eRet = COMP_FillThisBuffer(pInPortTunnelComp, &obh);
                    if(eRet != SUCCESS)
                    {
                        aloge("fatal error! muxChn[%d] fill this buffer fail[0x%x], text frame id=[%d], check code!",
                            pRecRenderData->mMppChnInfo.mChnId,
                            eRet, pEntry->stEncodedStream.nID);
                        pthread_mutex_lock(&pRecRenderData->mTextInputFrameListMutex);
                        list_add_tail(&pEntry->mList, &pRecRenderData->mTextInputFrameUsedList);
                        //list_move_tail(&pEntry->mList, &pRecRenderData->mTextInputFrameUsedList);
                        pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
                    }
                    else
                    {
                        pthread_mutex_lock(&pRecRenderData->mTextInputFrameListMutex);
                        list_add_tail(&pEntry->mList, &pRecRenderData->mTextInputFrameIdleList);
                        pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
                    }
                }
                else
                {
                    COMP_BUFFERHEADERTYPE obh;
                    obh.nInputPortIndex = pRecRenderData->sInPortTunnelInfo[pRSPacket->mStreamId].nPortIndex;
                    obh.pAppPrivate = (void*)&pEntry->stEncodedStream;
                    if(pEntry->mPortIndex != obh.nInputPortIndex)
                    {
                        aloge("fatal error! muxChn[%d] text streamId[%d!=%d]", pRecRenderData->mMppChnInfo.mChnId,
                            pEntry->mPortIndex, obh.nInputPortIndex);
                    }
                    pRecRenderData->pCallbacks->EmptyBufferDone(pRecRenderData->hSelf, pRecRenderData->pAppData, &obh);
                    pthread_mutex_lock(&pRecRenderData->mTextInputFrameListMutex);
                    list_add_tail(&pEntry->mList, &pRecRenderData->mTextInputFrameIdleList);
                    pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
                }
            }
            else
            {
                if(pEntry->mUsedRefCnt < 0)
                {
                    aloge("fatal error! muxChn[%d] usedRefCnt[%d]<0, check code!",
                        pRecRenderData->mMppChnInfo.mChnId, pEntry->mUsedRefCnt);
                    eRet = ERR_MUX_NOT_PERM;
                }
                pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
            }
        }
        else
        {
            aloge("fatal error! muxChn[%d] not find TFrmId[%d] in used list.",
                pRecRenderData->mMppChnInfo.mChnId, pRSPacket->mId);
            eRet = ERR_MUX_UNEXIST;
            pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
        }
    }
    else
    {
        aloge("fatal error! muxChn[%d] invalid streamType[%d]", pRecRenderData->mMppChnInfo.mChnId, pRSPacket->mStreamType);
        eRet = ERR_MUX_NOT_PERM;
    }
    return eRet;
}

ERRORTYPE RecRenderGetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    unsigned int portIdx = pPortDef->nPortIndex;

    if (portIdx < pRecRenderData->sPortParam.nPorts)
    {
        memcpy(pPortDef, &pRecRenderData->sInPortDef[portIdx], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else
        eError = ERR_MUX_ILLEGAL_PARAM;
    return eError;
}

ERRORTYPE RecRenderSetPortDefinition(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_PORTDEFINITIONTYPE *pPortDef)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    unsigned int portIndex = pPortDef->nPortIndex;
    if (portIndex < pRecRenderData->sPortParam.nPorts)
    {
        memcpy(&pRecRenderData->sInPortDef[portIndex], pPortDef, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
    }
    else
        eError = ERR_MUX_ILLEGAL_PARAM;
    return eError;
}

ERRORTYPE RecRenderGetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_REC_RENDER_PORTS; i++)
    {
        if(pRecRenderData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(pPortBufSupplier, &pRecRenderData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_MUX_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE RecRenderSetCompBufferSupplier(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    //find nPortIndex
    BOOL bFindFlag = FALSE;
    int i;
    for(i=0; i<MAX_REC_RENDER_PORTS; i++)
    {
        if(pRecRenderData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex)
        {
            bFindFlag = TRUE;
            memcpy(&pRecRenderData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
            break;
        }
    }
    if(bFindFlag)
    {
        eError = SUCCESS;
    }
    else
    {
        eError = ERR_MUX_ILLEGAL_PARAM;
    }
    return eError;
}

ERRORTYPE RecRenderGetPortParam(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_PORT_PARAM_TYPE *pPortParam)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    memcpy(pPortParam, &pRecRenderData->sPortParam, sizeof(COMP_PORT_PARAM_TYPE));
    return eError;
}

ERRORTYPE RecRenderGetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT MPP_CHN_S *pChn)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    copy_MPP_CHN_S(pChn, &pRecRenderData->mMppChnInfo);
    return eError;
}

ERRORTYPE RecRenderSetMPPChannelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MPP_CHN_S *pChn)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    copy_MPP_CHN_S(&pRecRenderData->mMppChnInfo, pChn);
    return SUCCESS;
}

ERRORTYPE RecRenderGetTunnelInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_INOUT COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = ERR_MUX_UNEXIST;
    int i;
    for(i=0; i<MAX_REC_RENDER_PORTS; i++)
    {
        if(pRecRenderData->sInPortTunnelInfo[i].nPortIndex == pTunnelInfo->nPortIndex)
        {
            memcpy(pTunnelInfo, &pRecRenderData->sInPortTunnelInfo[i], sizeof(COMP_INTERNAL_TUNNELINFOTYPE));
            eError = SUCCESS;
            break;
        }
    }
    return eError;
}

ERRORTYPE RecRenderGetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT MUX_CHN_ATTR_S *pChnAttr)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    copy_MUX_CHN_ATTR_S(pChnAttr, &pRecRenderData->mChnAttr);
    return eError;
}

ERRORTYPE RecRenderSetChnAttr(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN MUX_CHN_ATTR_S *pChnAttr)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if(COMP_StateLoaded==pRecRenderData->state || COMP_StateIdle==pRecRenderData->state)
    {
        copy_MUX_CHN_ATTR_S(&pRecRenderData->mChnAttr, pChnAttr);

        //MediaDebugLoadMppMuxParams(pRecRenderData, NULL);

        pRecRenderData->mRecordMode = RECORDER_MODE_NONE;
        if(pChnAttr->mVideoAttrValidNum > 0)
        {
            pRecRenderData->mRecordMode |= RECORDER_MODE_VIDEO;
        }
        for (int VideoInfoIndex = 0; VideoInfoIndex < pRecRenderData->mChnAttr.mVideoAttrValidNum; VideoInfoIndex++)
        {
            //config media_inf
            if(pRecRenderData->mChnAttr.mVideoAttr[VideoInfoIndex].mVideoEncodeType != PT_MAX)
            {
                pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].nHeight = pRecRenderData->mChnAttr.mVideoAttr[VideoInfoIndex].mHeight;
                pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].nWidth = pRecRenderData->mChnAttr.mVideoAttr[VideoInfoIndex].mWidth;
                pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate = pRecRenderData->mChnAttr.mVideoAttr[VideoInfoIndex].mVideoFrmRate;
                pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].create_time = pRecRenderData->mChnAttr.mVideoAttr[VideoInfoIndex].mCreateTime;
                pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].maxKeyInterval = pRecRenderData->mChnAttr.mVideoAttr[VideoInfoIndex].mMaxKeyInterval;
                pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].mVideoEncodeType = map_PAYLOAD_TYPE_E_to_VENC_CODEC_TYPE(pRecRenderData->mChnAttr.mVideoAttr[VideoInfoIndex].mVideoEncodeType);
                pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].rotate_degree = pRecRenderData->mChnAttr.mVideoAttr[VideoInfoIndex].mRotateDegree;
            }
        }
        pRecRenderData->media_inf.mVideoInfoValidNum = pRecRenderData->mChnAttr.mVideoAttrValidNum;

        if(pRecRenderData->mChnAttr.mAudioEncodeType != PT_MAX)
        {
            pRecRenderData->mRecordMode |= RECORDER_MODE_AUDIO;
        }
        if(PT_TEXT == pRecRenderData->mChnAttr.mTextEncodeType)
        {
            pRecRenderData->mRecordMode |= RECORDER_MODE_TEXT;
        }
        if(pRecRenderData->mChnAttr.mAudioEncodeType != PT_MAX)
        {
            pRecRenderData->media_inf.channels = pRecRenderData->mChnAttr.mChannels;
            pRecRenderData->media_inf.bits_per_sample = pRecRenderData->mChnAttr.mBitsPerSample;
            pRecRenderData->media_inf.frame_size = pRecRenderData->mChnAttr.mSamplesPerFrame;
            pRecRenderData->media_inf.sample_rate = pRecRenderData->mChnAttr.mSampleRate;
            pRecRenderData->media_inf.audio_encode_type = map_PAYLOAD_TYPE_E_to_AUDIO_ENCODER_TYPE(pRecRenderData->mChnAttr.mAudioEncodeType);
        }
        if(PT_TEXT == pRecRenderData->mChnAttr.mTextEncodeType)
        {
            pRecRenderData->media_inf.text_encode_type = TEXT_ENCODER_GGAD;
            pRecRenderData->media_inf.geo_available = 1;
        }
        pRecRenderData->nCallbackOutFlag = pRecRenderData->mChnAttr.mCallbackOutFlag;
    }
    else if(COMP_StateExecuting==pRecRenderData->state || COMP_StatePause==pRecRenderData->state)
    {
        //only process dynamic changing of some params
        //(1) max file duration, current file takes effect.
        if (pRecRenderData->mChnAttr.mMaxFileDuration != pChnAttr->mMaxFileDuration)
        {
            pthread_mutex_lock(&pRecRenderData->mJudgeSwitchlock);
            if(pRecRenderData->bNeedSw || pRecRenderData->bNeedSwAudio)
            {
                alogd("muxChn[%d] are in switch file process[%d-%d], can't change file duration now",
                    pRecRenderData->mMppChnInfo.mChnId,
                    pRecRenderData->bNeedSw, pRecRenderData->bNeedSwAudio);
                eError = ERR_MUX_NOT_PERM;
            }
            else
            {
                pthread_mutex_lock(&pRecRenderData->mMuxerInitlock);
                alogd("muxChn[%d] change max file duration[%lld->%lld]ms", pRecRenderData->mMppChnInfo.mChnId,
                    pRecRenderData->mChnAttr.mMaxFileDuration, pChnAttr->mMaxFileDuration);
                //pthread_mutex_lock(&pRecRenderData->mutex_reset_writer_lock);
                pRecRenderData->mChnAttr.mMaxFileDuration = pChnAttr->mMaxFileDuration;

                if(pRecRenderData->mbMuxerInit)
                {
                    if(pRecRenderData->mChnAttr.mMaxFileDuration > pRecRenderData->mCurMaxFileDuration)
                    {
                        pRecRenderData->mCurFileEndTm += (pRecRenderData->mChnAttr.mMaxFileDuration - pRecRenderData->mCurMaxFileDuration);
                        alogd("change file duration long: muxChn[%d] Dur:[%lld->%lld]ms newFileEndTm:%lldms",
                            pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mCurMaxFileDuration,
                            pRecRenderData->mChnAttr.mMaxFileDuration, pRecRenderData->mCurFileEndTm);
                        pRecRenderData->mCurMaxFileDuration = pRecRenderData->mChnAttr.mMaxFileDuration;
                    }
                    else if(pRecRenderData->mChnAttr.mMaxFileDuration < pRecRenderData->mCurMaxFileDuration)
                    {
                        pRecRenderData->mCurFileEndTm -= (pRecRenderData->mCurMaxFileDuration - pRecRenderData->mChnAttr.mMaxFileDuration);
                        alogd("change file duration short: muxChn[%d] Dur:[%lld->%lld]ms newFileEndTm:%lldms",
                            pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mCurMaxFileDuration,
                            pRecRenderData->mChnAttr.mMaxFileDuration, pRecRenderData->mCurFileEndTm);
                        pRecRenderData->mCurMaxFileDuration = pRecRenderData->mChnAttr.mMaxFileDuration;
                    }
                }
                //pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
                pthread_mutex_unlock(&pRecRenderData->mMuxerInitlock);
            }
            pthread_mutex_unlock(&pRecRenderData->mJudgeSwitchlock);
        }
    }
    else
    {
        aloge("fatal error! muxChn[%d] cannot set chnAttr in wrong state[0x%x]", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->state);
        eError = ERR_MUX_INCORRECT_STATE_OPERATION;
    }
    return eError;
}

static ERRORTYPE RecRenderSetFd(COMP_HANDLETYPE hComponent, CdxFdT *pCdxFd)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if(pRecRenderData->nOutputFd >= 0)
    {
        aloge("fatal error! muxChn[%d] nOutputFd[%d]>=0", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nOutputFd);
        close(pRecRenderData->nOutputFd);
        pRecRenderData->nOutputFd = -1;
    }
    if(pCdxFd->mFd >= 0)
    {
        pRecRenderData->nOutputFd = dup(pCdxFd->mFd);
        pRecRenderData->nFallocateLen = pCdxFd->mnFallocateLen;
        //pRecRenderSink->nOutputFd = dup2SeldomUsedFd(pCdxSink->nOutputFd);
        alogd("muxChn[%d] dup fd[%d]->[%d]", pRecRenderData->mMppChnInfo.mChnId, pCdxFd->mFd, pRecRenderData->nOutputFd);
    }
    else
    {
        aloge("muxChn[%d] fd[%d] < 0!", pRecRenderData->mMppChnInfo.mChnId, pCdxFd->mFd);
    }
    return SUCCESS;
}

/*****************************************************************************/
ERRORTYPE RecRenderSendCommand(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_COMMANDTYPE Cmd,
        PARAM_IN unsigned int nParam1,
        PARAM_IN void* pCmdData)
{
    RECRENDERDATATYPE *pRecRenderData;
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;
    void*   pMsgData = NULL;
    int     nMsgDataSize = 0;
    memset(&msg, 0, sizeof(message_t));

    alogv("RecRenderSendCommand: %d", Cmd);

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    if (pRecRenderData->state == COMP_StateInvalid)
    {
        alogd("stateInvalid deny command[0x%x]", Cmd);
        eError = ERR_MUX_INVALIDSTATE;
        goto OMX_CONF_CMD_BAIL;
    }

    switch (Cmd)
    {
        case COMP_CommandStateSet:
            eCmd = SetState;
            break;

        case COMP_CommandFlush:
            eCmd = Flush;
            break;

        default:
            aloge("fatal error! unknown command[0x%x]", Cmd);
            eCmd = -1;
            break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    msg.mpData = pMsgData;
    msg.mDataSize = nMsgDataSize;
    putMessageWithData(&pRecRenderData->cmd_queue, &msg);
OMX_CONF_CMD_BAIL:
    return eError;
}

/*****************************************************************************/
ERRORTYPE RecRenderGetState(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT COMP_STATETYPE* pState) {
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    //pthread_mutex_lock(&pRecRenderData->mStateMutex);
    *pState = pRecRenderData->state;
    //pthread_mutex_unlock(&pRecRenderData->mStateMutex);

    //OMX_CONF_CMD_BAIL:
    return eError;
}

/*****************************************************************************/
ERRORTYPE RecRenderSetCallbacks(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_CALLBACKTYPE* pCallbacks,
        PARAM_IN void* pAppData)
{
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pRecRenderData->pCallbacks = pCallbacks;
    pRecRenderData->pAppData = pAppData;

    //OMX_CONF_CMD_BAIL:
    return eError;
}

/**
  set spspps.

  @return
    SUCCESS
    FAILURE
    ERR_MUX_NOMEM
*/
ERRORTYPE RecRenderSetH264SpsPpsInfo(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VencHeaderDataParam *pVencHeaderDataParam)
{
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;
    bool bFindFlag = false;

    if (0 == pVencHeaderDataParam->mH264SpsPpsInfo.nLength || NULL == pVencHeaderDataParam->mH264SpsPpsInfo.pBuffer)
    {
        aloge("fatal error! invalid params! %d, %p", pVencHeaderDataParam->mH264SpsPpsInfo.nLength, pVencHeaderDataParam->mH264SpsPpsInfo.pBuffer);
        return FAILURE;
    }

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE *) hComponent)->pComponentPrivate);

    VencHeaderDataNode *pEntry = NULL;
    bFindFlag = false;
    if(!list_empty(&pRecRenderData->mVencHeaderDataList))
    {
        list_for_each_entry(pEntry, &pRecRenderData->mVencHeaderDataList, mList)
        {
            if(pEntry->mVeChn == pVencHeaderDataParam->mVeChn)
            {
                bFindFlag = true;
                break;
            }
        }
    }
    if(bFindFlag) //update spspps
    {
        alogd("Be careful! muxChn[%d] set streamId[%d] veChn[%d] spspps again, update it.",
            pRecRenderData->mMppChnInfo.mChnId, pEntry->mStreamId, pEntry->mVeChn);
        if(pEntry->mH264SpsPpsInfo.pBuffer)
        {
            free(pEntry->mH264SpsPpsInfo.pBuffer);
            pEntry->mH264SpsPpsInfo.pBuffer = NULL;
            pEntry->mH264SpsPpsInfo.nLength = 0;
        }
        pEntry->mH264SpsPpsInfo.pBuffer = (unsigned char *)malloc(pVencHeaderDataParam->mH264SpsPpsInfo.nLength);
        if (pEntry->mH264SpsPpsInfo.pBuffer)
        {
            memcpy(pEntry->mH264SpsPpsInfo.pBuffer, pVencHeaderDataParam->mH264SpsPpsInfo.pBuffer, pVencHeaderDataParam->mH264SpsPpsInfo.nLength);
            pEntry->mH264SpsPpsInfo.nLength = pVencHeaderDataParam->mH264SpsPpsInfo.nLength;
        }
        else
        {
            aloge("fatal error! pVencHeaderDataNode->mH264SpsPpsInfo.pBuffer malloc fail!");
            eError = ERR_MUX_NOMEM;
        }
    }
    else
    {
        VencHeaderDataNode *pVencHeaderDataNode = (VencHeaderDataNode *)malloc(sizeof(VencHeaderDataNode));
        if (NULL == pVencHeaderDataNode)
        {
            aloge("fatal error! malloc pVencHeaderDataNode fail!");
            return FAILURE;
        }

        memset(pVencHeaderDataNode, 0, sizeof(VencHeaderDataNode));
        pVencHeaderDataNode->mStreamId = -1;
        pVencHeaderDataNode->mVeChn = pVencHeaderDataParam->mVeChn;
        pVencHeaderDataNode->mH264SpsPpsInfo.pBuffer = (unsigned char *)malloc(pVencHeaderDataParam->mH264SpsPpsInfo.nLength);
        if (NULL == pVencHeaderDataNode->mH264SpsPpsInfo.pBuffer)
        {
            aloge("fatal error! pVencHeaderDataNode->mH264SpsPpsInfo.pBuffer malloc fail!");
            free(pVencHeaderDataNode);
            pVencHeaderDataNode = NULL;
            return FAILURE;
        }
        memcpy(pVencHeaderDataNode->mH264SpsPpsInfo.pBuffer, pVencHeaderDataParam->mH264SpsPpsInfo.pBuffer, pVencHeaderDataParam->mH264SpsPpsInfo.nLength);
        pVencHeaderDataNode->mH264SpsPpsInfo.nLength = pVencHeaderDataParam->mH264SpsPpsInfo.nLength;

        list_add_tail(&pVencHeaderDataNode->mList, &pRecRenderData->mVencHeaderDataList);

        VeChnBindStreamIdNode *pTmp = NULL;
        bFindFlag = false;
        list_for_each_entry(pTmp, &pRecRenderData->mVeChnBindStreamIdList, mList)
        {
            if (pVencHeaderDataParam->mVeChn == pTmp->mVeChn.mChnId)
            {
                bFindFlag = true;
                break;
            }
        }
        if (bFindFlag)
        {
            pVencHeaderDataNode->mStreamId = pTmp->mStreamId;
            alogd("muxChn[%d] set Venc header data is success! StreamId: %d, veChn: %d, SpsPps info length: %d",
                pRecRenderData->mMppChnInfo.mChnId, pVencHeaderDataNode->mStreamId,
                pVencHeaderDataNode->mVeChn, pVencHeaderDataNode->mH264SpsPpsInfo.nLength);
        }
        else
        {
            alogd("muxChn[%d] set Venc header data! not find VeChn[%d] in streamId binding list, perhaps set SpsPps before binding tunnel!",
                pRecRenderData->mMppChnInfo.mChnId, pVencHeaderDataParam->mVeChn);
            //eError = ERR_MUX_UNEXIST;
        }
    }
    return eError;
}

ERRORTYPE RecRenderSetSwitchPolicy(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN RecordFileDurationPolicy *pPolicy
        )
{
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    pRecRenderData->mFileDurationPolicy = *pPolicy;

    return eError;
}

ERRORTYPE RecRenderGetSwitchPolicy(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN RecordFileDurationPolicy *pPolicy
        )
{
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    *pPolicy = pRecRenderData->mFileDurationPolicy;

    return eError;
}

ERRORTYPE RecRenderSetShutDownType(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN BOOL bShutDownNowFlag)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    pRecRenderData->mbShutDownNowFlag = bShutDownNowFlag;
    return SUCCESS;
}

ERRORTYPE RecRenderSwitchFileNormal(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN SwitchFileNormalInfo* pInfo)
{
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    message_t msg;
    InitMessage(&msg);
    msg.command = SwitchFileNormal;
    msg.mpData = pInfo;
    msg.mDataSize = sizeof(SwitchFileNormalInfo);
    msg.pReply = ConstructMessageReply();
    putMessageWithData(&pRecRenderData->cmd_queue, &msg);

    int ret;
    while(1)
    {
        ret = cdx_sem_down_timedwait(&msg.pReply->ReplySem, 5000);
        if(ret != 0)
        {
            aloge("fatal error! wait switch file normal fail[0x%x]", ret);
        }
        else
        {
            break;
        }
    }
    eError = (ERRORTYPE)msg.pReply->ReplyResult;
    alogd("receive switch file normal reply: 0x%x!", eError);
    DestructMessageReply(msg.pReply);
    msg.pReply = NULL;
    return eError;
}

ERRORTYPE RecRenderVeChnBindStreamId(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN VeChnBindStreamIdNode* pChn)
{
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    VeChnBindStreamIdNode *pNode = (VeChnBindStreamIdNode *)malloc(sizeof(VeChnBindStreamIdNode));

    memset(pNode, 0, sizeof(VeChnBindStreamIdNode));
    pNode->mStreamId = pChn->mStreamId;
    memcpy(&pNode->mVeChn, &pChn->mVeChn, sizeof(MPP_CHN_S));

    alogd("VeChn mChnId: %d bind StreamId: %d", pChn->mVeChn.mChnId, pChn->mStreamId);

    //process streamId of pRecRenderData->media_inf.
    for (int VideoInfoIndex = 0; VideoInfoIndex < pRecRenderData->mChnAttr.mVideoAttrValidNum; VideoInfoIndex++)
    {
        if (pRecRenderData->mChnAttr.mVideoAttr[VideoInfoIndex].mVeChn == pNode->mVeChn.mChnId)
        {
            pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].mStreamId = pNode->mStreamId;
        }
    }
    //process streamId of pRecRenderData->mVencHeaderDataList
    if(!list_empty(&pRecRenderData->mVencHeaderDataList))
    {
        int nFindNum = 0;
        VencHeaderDataNode *pEntry = NULL;
        list_for_each_entry(pEntry, &pRecRenderData->mVencHeaderDataList, mList)
        {
            if (pEntry->mVeChn == pNode->mVeChn.mChnId)
            {
                if(pEntry->mStreamId != -1)
                {
                    aloge("fatal error! VencHeader data node: veChn[%d], streamId[%d]!=-1", pEntry->mVeChn, pEntry->mStreamId);
                }
                pEntry->mStreamId = pNode->mStreamId;
                nFindNum++;
            }
        }
        if(nFindNum != 1)
        {
            aloge("fatal error! veChn[%d] is found wrong times[%d]", pNode->mVeChn.mChnId, nFindNum);
        }
    }
    
    list_add_tail(&pNode->mList, &pRecRenderData->mVeChnBindStreamIdList);

    return eError;
}

/*****************************************************************************/
ERRORTYPE RecRenderGetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_INOUT void* pComponentConfigStructure)
{
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;
    //COMP_INDEXTYPE portIdx;

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = RecRenderGetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = RecRenderGetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamOtherInit:
        {
            eError = RecRenderGetPortParam(hComponent, (COMP_PORT_PARAM_TYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorGetPortParam:
        {
            eError = RecRenderGetPortParam(hComponent, (COMP_PORT_PARAM_TYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = RecRenderGetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorTunnelInfo:
        {
            eError = RecRenderGetTunnelInfo(hComponent, (COMP_INTERNAL_TUNNELINFOTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMuxChnAttr:
        {
            eError = RecRenderGetChnAttr(hComponent, (MUX_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMuxGetDuration:
            *(int64_t*)pComponentConfigStructure = pRecRenderData->duration;
            break;
        case COMP_IndexVendorMuxSwitchPolicy:
        {
            eError = RecRenderGetSwitchPolicy(hComponent, (RecordFileDurationPolicy *)pComponentConfigStructure);
            break;
        }
        default:
            aloge("fatal error! unknown index[0x%x]", nIndex);
            break;
    }

    if (pRecRenderData->state == COMP_StateInvalid)
        eError = ERR_MUX_INCORRECT_STATE_OPERATION;

    //OMX_CONF_CMD_BAIL:
    return eError;
}

ERRORTYPE RecRenderSetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    RECRENDERDATATYPE *pRecRenderData = NULL;
    ERRORTYPE eError = SUCCESS;

    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            eError = RecRenderSetPortDefinition(hComponent, (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            eError = RecRenderSetCompBufferSupplier(hComponent, (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            eError = RecRenderSetMPPChannelInfo(hComponent, (MPP_CHN_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMuxChnAttr:
        {
            eError = RecRenderSetChnAttr(hComponent, (MUX_CHN_ATTR_S*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMuxFD:
        {
            eError = RecRenderSetFd(hComponent, (CdxFdT*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMuxSwitchFd:
        {
            if (pRecRenderData->state != COMP_StateExecuting)
            {
                aloge("fatal error! muxChn[%d] comp IndexConfigVendorSwitchFd state[%d] is not valid",
                    pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->state);
            }
            CdxFdT *pCdxFd = (CdxFdT*)pComponentConfigStructure;
            message_t   msg;
            InitMessage(&msg);
            msg.command = RecSink_SwitchFd;
            msg.para0 = pCdxFd->mFd;
            msg.para1 = pCdxFd->mnFallocateLen;
            msg.pReply = ConstructMessageReply();
            putMessageWithData(&pRecRenderData->cmd_queue, &msg);

            int ret;
            while(1)
            {
                ret = cdx_sem_down_timedwait(&msg.pReply->ReplySem, 5000);
                if(ret != 0)
                {
                    aloge("fatal error! muxChn[%d] wait switch fd fail[0x%x]", pRecRenderData->mMppChnInfo.mChnId, ret);
                }
                else
                {
                    break;
                }
            }
            eError = (ERRORTYPE)msg.pReply->ReplyResult;
            alogd("muxChn[%d] receive switch fd reply: 0x%x!", pRecRenderData->mMppChnInfo.mChnId, eError);
            DestructMessageReply(msg.pReply);
            msg.pReply = NULL;
            break;
        }
        case COMP_IndexVendorSdcardState:
        {
            if (pRecRenderData->state != COMP_StateExecuting)
            {
                alogw("COMP IndexVendorSdcardState state[%d] is not valid, sdcardState[%d]", pRecRenderData->state, *(int*)pComponentConfigStructure);
            }
            pRecRenderData->mbSdCardState = (BOOL)*(int*)pComponentConfigStructure;
            break;
        }
        case COMP_IndexVendorMuxStrmIds:
        {
            MuxStreamIdsInfo *pStrmIdsInfo = (MuxStreamIdsInfo*)pComponentConfigStructure;
            pRecRenderData->mValidStreamCount = pStrmIdsInfo->mStrmIdsCnt;
            if(pRecRenderData->mValidStreamCount > MAX_TRACK_COUNT)
            {
                aloge("fatal error! muxChn[%d] stream count[%d] too many!", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mValidStreamCount);
                pRecRenderData->mValidStreamCount = MAX_TRACK_COUNT;
            }
            for(int i=0; i<pRecRenderData->mValidStreamCount; i++)
            {
                pRecRenderData->mValidStreamIds[i] = pStrmIdsInfo->mStrmIds[i];
            }
            break;
        }
        case COMP_IndexVendorExtraData:
        {
            eError = RecRenderSetH264SpsPpsInfo(hComponent, (VencHeaderDataParam *)pComponentConfigStructure);
            break;
        }
//        case OMX_IndexVendorSetRecRenderCallbackInfo:
//            pRecRenderData->callback_writer = (void*)ComponentParameterStructure;
//            break;

//        case COMP_IndexVendorFsWriteMode:
//            pRecRenderData->mFsWriteMode = *(FSWRITEMODE*)pComponentConfigStructure;
//            break;
//        case COMP_IndexVendorFsSimpleCacheSize:
//            pRecRenderData->mSimpleCacheSize = *(int*)pComponentConfigStructure;
//            break;
        case COMP_IndexVendorMuxSwitchPolicy:
        {
            eError = RecRenderSetSwitchPolicy(hComponent, (RecordFileDurationPolicy *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMuxShutDownType:
        {
            eError = RecRenderSetShutDownType(hComponent, *(BOOL*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexSwitchFileNormal:
        {
            eError = RecRenderSwitchFileNormal(hComponent, (SwitchFileNormalInfo*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorMuxSetThmPic:
        {
            THM_PIC *pThmPic = (THM_PIC*)pComponentConfigStructure;
            pthread_mutex_lock(&pRecRenderData->mThmPicLock);
            if(1 == pRecRenderData->rc_thm_pic_ready)
            {
                alogw("Be careful! thm_pic is in ready, so replace it!");
                pRecRenderData->rc_thm_pic_ready = 0;
                if(pRecRenderData->rc_thm_pic.p_thm_pic_addr)
                {
                    free(pRecRenderData->rc_thm_pic.p_thm_pic_addr);
                    pRecRenderData->rc_thm_pic.p_thm_pic_addr = NULL;
                    pRecRenderData->rc_thm_pic.thm_pic_size = 0;
                }
            }
            if(pThmPic->thm_pic_size > 0)
            {
                pRecRenderData->rc_thm_pic.p_thm_pic_addr = (char*)malloc(pThmPic->thm_pic_size);
                if(pRecRenderData->rc_thm_pic.p_thm_pic_addr)
                {
                    memcpy(pRecRenderData->rc_thm_pic.p_thm_pic_addr, pThmPic->p_thm_pic_addr, pThmPic->thm_pic_size);
                    pRecRenderData->rc_thm_pic.thm_pic_size = pThmPic->thm_pic_size;
                    pRecRenderData->rc_thm_pic_ready = 1;
                }
                else
                {
                    aloge("fatal error! malloc fail");
                    pRecRenderData->rc_thm_pic_ready = 0;
                }
            }
            else
            {
                aloge("fatal error! muxChn[%d] rc_set_thm_pic_size[%d] <= 0!", pRecRenderData->mMppChnInfo.mChnId, pThmPic->thm_pic_size);
                pRecRenderData->rc_thm_pic_ready = 0;
            }
            pthread_mutex_unlock(&pRecRenderData->mThmPicLock);
            break;
        }
        case COMP_IndexVendorMuxVeChnBindStreamId:
        {
            RecRenderVeChnBindStreamId(hComponent, (VeChnBindStreamIdNode*)pComponentConfigStructure);
            break;
        }
        default:
        {
            aloge("fatal error! unknown nIndex[0x%x] in state[%d]", nIndex, pRecRenderData->state);
            eError = ERR_MUX_ILLEGAL_PARAM;
            break;
        }
    }

    return eError;
}

ERRORTYPE RecRenderComponentTunnelRequest(
    PARAM_IN  COMP_HANDLETYPE hComponent,
    PARAM_IN  unsigned int nPort,
    PARAM_IN  COMP_HANDLETYPE hTunneledComp,
    PARAM_IN  unsigned int nTunneledPort,
    PARAM_INOUT  COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    alogd("unsigned int nPort: %d", nPort);
    ERRORTYPE eError = SUCCESS;
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    if (pRecRenderData->state == COMP_StateExecuting)
    {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    }
    else if (pRecRenderData->state != COMP_StateIdle)
    {
        aloge("fatal error! tunnel request can't be in state[0x%x]!", pRecRenderData->state);
        eError = ERR_MUX_INCORRECT_STATE_OPERATION;
        goto COMP_CMD_FAIL;
    }
    COMP_PARAM_PORTDEFINITIONTYPE    *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE     *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE    *pPortBufSupplier;
    BOOL bFindFlag;
    int i;
    bFindFlag = FALSE;
    for(i=0; i<MAX_REC_RENDER_PORTS; i++)
    {
        if(pRecRenderData->sInPortDef[i].nPortIndex == nPort)
        {
            pPortDef = &pRecRenderData->sInPortDef[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_MUX_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for(i=0; i<MAX_REC_RENDER_PORTS; i++)
    {
        if(pRecRenderData->sInPortTunnelInfo[i].nPortIndex == nPort)
        {
            pPortTunnelInfo = &pRecRenderData->sInPortTunnelInfo[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_MUX_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    bFindFlag = FALSE;
    for(i=0; i<MAX_REC_RENDER_PORTS; i++)
    {
        if(pRecRenderData->sPortBufSupplier[i].nPortIndex == nPort)
        {
            pPortBufSupplier = &pRecRenderData->sPortBufSupplier[i];
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_MUX_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if(NULL==hTunneledComp && 0==nTunneledPort && NULL==pTunnelSetup)
    {
        alogd("omx_core cancel setup tunnel on port[%d]", nPort);
        eError = SUCCESS;
        if(pPortDef->eDir == COMP_DirInput)
        {
            pRecRenderData->mInputPortTunnelFlag[nPort] = FALSE;
        }
        else
        {
            aloge("fatal error! mux has not output port!");
        }
        goto COMP_CMD_FAIL;
    }
    if(pPortDef->eDir == COMP_DirOutput)
    {
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
    }
    else
    {
        //Check the data compatibility between the ports using one or more GetParameter calls.
        //B checks if its input port is compatible with the output port of component A.
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if(out_port_def.eDir != COMP_DirOutput)
        {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_MUX_ILLEGAL_PARAM;
            goto COMP_CMD_FAIL;
        }
        pPortDef->format = out_port_def.format;

        ERRORTYPE eResult = SUCCESS;
        MPP_CHN_S stMppChn;
        eResult = ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexVendorMPPChannelInfo, &stMppChn);
        if ((SUCCESS == eResult) && (MOD_ID_VENC == stMppChn.mModId))
        {
            VeChnBindStreamIdNode stVeChnStreamInfo;
            stVeChnStreamInfo.mStreamId = nPort;
            memcpy(&stVeChnStreamInfo.mVeChn, &stMppChn, sizeof(MPP_CHN_S));

            RecRenderVeChnBindStreamId(hComponent, &stVeChnStreamInfo);
        }

        //The component B informs component A about the final result of negotiation.
        if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier)
        {
            alogw("Low probability! use input portIndex[%d] buffer supplier[%d] as final!", nPort, pPortBufSupplier->eBufferSupplier);
            pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        }
        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        ((MM_COMPONENTTYPE*)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        pRecRenderData->mInputPortTunnelFlag[nPort] = TRUE;
    }

COMP_CMD_FAIL:
    return eError;
}

//ERRORTYPE AudioRecRequstBuffer(
//        PARAM_IN  COMP_HANDLETYPE hComponent,
//        PARAM_IN  unsigned int nPortIndex,
//        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer) {
//
//    RECRENDERDATATYPE *pRecRenderData = NULL;
//  ERRORTYPE eError = SUCCESS;
//
//  pRecRenderData
//          = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
//
//  put_data(&pRecRenderData->Audio_buffer_quene, pBuffer);
//
//ERROR:
//  return eError;
//}
//
//ERRORTYPE AudioRecReleaseBuffer(
//        PARAM_IN  COMP_HANDLETYPE hComponent,
//        PARAM_IN  unsigned int nPortIndex,
//        PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer) {
//
//    RECRENDERDATATYPE *pRecRenderData = NULL;
//  ERRORTYPE eError = SUCCESS;
//
//  pRecRenderData
//          = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
//
//ERROR:
//  return eError;
//
//}
#if 0
/*
 * malloc memory to store data from VENC_STREAM_S.
 */
static ERRORTYPE InitEncodedStreamByVENC_STREAM_S(EncodedStream *pDst, VENC_STREAM_S *pSrc, PAYLOAD_TYPE_E eType)
{
    if (pDst == NULL || pSrc == NULL)
    {
        aloge("fatal error! Empty pointer.");
        return FAILURE;
    }
    memset(pDst, 0, sizeof(EncodedStream));
    pDst->media_type = CDX_PacketVideo;
    pDst->nID = pSrc->mSeq;
    pDst->nFilledLen = pSrc->mpPack[0].mLen0 + pSrc->mpPack[0].mLen1 + pSrc->mpPack[0].mLen2;
    pDst->nTobeFillLen = 0;
    pDst->nTimeStamp = pSrc->mpPack[0].mPTS;
    pDst->nFlags = 0;
    if(PT_H264 == eType)
    {
        if(H264E_NALU_ISLICE == pSrc->mpPack[0].mDataType.enH264EType)
        {
            pDst->nFlags |= CEDARV_FLAG_KEYFRAME;
        }
    }
    else if(PT_H265 == eType)
    {
        if(H265E_NALU_ISLICE == pSrc->mpPack[0].mDataType.enH265EType)
        {
            pDst->nFlags |= CEDARV_FLAG_KEYFRAME;
        }
    }
    
    pDst->pBuffer = (unsigned char*)malloc(pDst->nFilledLen);
    if(NULL == pDst->pBuffer)
    {
        aloge("fatal error! malloc [%d]bytes fail!", pDst->nFilledLen);
        return ERR_MUX_NOMEM;
    }
    unsigned char *pBuf = pDst->pBuffer;
    if(pSrc->mpPack[0].mLen0 > 0)
    {
        memcpy(pBuf, pSrc->mpPack[0].mpAddr0, pSrc->mpPack[0].mLen0);
        pBuf += pSrc->mpPack[0].mLen0;
    }
    if(pSrc->mpPack[0].mLen1 > 0)
    {
        memcpy(pBuf, pSrc->mpPack[0].mpAddr1, pSrc->mpPack[0].mLen1);
        pBuf += pSrc->mpPack[0].mLen1;
    }
    if(pSrc->mpPack[0].mLen2 > 0)
    {
        memcpy(pBuf, pSrc->mpPack[0].mpAddr2, pSrc->mpPack[0].mLen2);
        pBuf += pSrc->mpPack[0].mLen2;
    }
    pDst->nBufferLen = pDst->nFilledLen;
    pDst->pBufferExtra = NULL;
    pDst->nBufferExtraLen = 0;
    pDst->pBufferExtra2 = NULL;
    pDst->nBufferExtraLen2 = 0;

    pDst->infoVersion = -1;
    pDst->pChangedStreamsInfo = NULL;
    pDst->duration = -1;
    return SUCCESS;
}

static ERRORTYPE InitEncodedStreamByAUDIO_STREAM_S(EncodedStream *pDst, AUDIO_STREAM_S *pSrc)
{
    if (pDst == NULL || pSrc == NULL)
    {
        aloge("fatal error! Empty pointer.");
        return FAILURE;
    }
    pDst->media_type = CDX_PacketAudio;
    memset(pDst, 0, sizeof(EncodedStream));
    pDst->pBuffer = (unsigned char*)malloc(pSrc->mLen);
    if(NULL == pDst->pBuffer)
    {
        aloge("fatal error! malloc [%d]bytes fail!", pSrc->mLen);
        return ERR_MUX_NOMEM;
    }
    memcpy(pDst->pBuffer, pSrc->pStream, pSrc->mLen);
    pDst->nFilledLen = pSrc->mLen;
    pDst->nBufferLen = pSrc->mLen;
    pDst->nTimeStamp = pSrc->mTimeStamp;
    pDst->nID = pSrc->mId;
    return SUCCESS;
}
#endif

static CDX_PACKETTYPE GetStreamTypeByStreamId(int nStreamId, _media_file_inf_t *pMediaInf)
{
    CDX_PACKETTYPE eType;
    if(nStreamId < pMediaInf->mVideoInfoValidNum)
    {
        eType = CDX_PacketVideo;
    }
    else if(nStreamId == pMediaInf->mVideoInfoValidNum)
    {
        eType = CDX_PacketAudio;
    }
    else
    {
        eType = CDX_PacketSubtitle;
    }
    return eType;
}

ERRORTYPE RecRenderEmptyThisBuffer(
            PARAM_IN  COMP_HANDLETYPE hComponent,
            PARAM_IN  COMP_BUFFERHEADERTYPE* pBuffer)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    int ret=0;

    pthread_mutex_lock(&pRecRenderData->mStateMutex);

    // Check state
    if ((pRecRenderData->state != COMP_StateIdle) && (pRecRenderData->state != COMP_StateExecuting) && (pRecRenderData->state != COMP_StatePause))
    {
        alogd("muxChn[%d] send buffer in invalid state[0x%x]!", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->state);
        pthread_mutex_unlock(&pRecRenderData->mStateMutex);
        return ERR_MUX_INCORRECT_STATE_OPERATION;
    }
    BOOL bFindFlag = FALSE;
    int Index;
    for (Index = 0; Index < MAX_REC_RENDER_PORTS; Index++)
    {
        if (pBuffer->nInputPortIndex == pRecRenderData->sInPortDef[Index].nPortIndex)
        {
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        aloge("fatal error! muxChn[%d] find port[%d] failure!", pRecRenderData->mMppChnInfo.mChnId, pBuffer->nInputPortIndex);
        pthread_mutex_unlock(&pRecRenderData->mStateMutex);
        return ERR_MUX_ILLEGAL_PARAM;
    }
    EncodedStream *pOutFrame = NULL;
    COMP_PORTDOMAINTYPE ePortDomain;
    if(pRecRenderData->mInputPortTunnelFlag[Index])
    {
        ePortDomain = pRecRenderData->sInPortDef[Index].eDomain;
        pOutFrame = (EncodedStream*)pBuffer->pOutputPortPrivate;
    }
    else
    {
        pOutFrame = (EncodedStream*)pBuffer->pAppPrivate;
        switch(pOutFrame->media_type)
        {
            case CDX_PacketVideo:
            {
                ePortDomain = COMP_PortDomainVideo;
                break;
            }
            case CDX_PacketAudio:
            {
                ePortDomain = COMP_PortDomainAudio;
                break;
            }
            case CDX_PacketSubtitle:
            {
                ePortDomain = COMP_PortDomainText;
                break;
            }
            default:
            {
                aloge("fatal error! unknown streamType[%d]", pOutFrame->media_type);
                ePortDomain = COMP_PortDomainVendorStartUnused;
                break;
            }
        }
    }
#ifdef __SAVE_INPUT_BS__
    if (pOutFrame->media_type == CDX_PacketVideo)
    {
        if (ftell(fp_bs) == 0)
        {// Write SPS/PPS info
            alogd("===== Write SPS/PPS Info");

            int Index;
            for (Index = 0; Index < MAX_REC_RENDER_PORTS; Index++)
            {
                if (pBuffer->nInputPortIndex == pRecRenderData->sInPortDef[Index].nPortIndex)
                {
                    if(Index != pRecRenderData->sInPortDef[Index].nPortIndex)
                    {
                        aloge("fatal error! check port index:%d!=%d", Index, pRecRenderData->sInPortDef[Index].nPortIndex);
                    }
                    break;
                }
            }

            VencHeaderDataNode *pEntry = NULL;
            if (!list_empty(pRecRenderData->mVencHeaderDataList))
            {
                list_for_each_entry(pEntry, &pRecRenderData->mVencHeaderDataList, mList)
                {
                    if (pEntry->mStreamId = Index);
                        break;
                }
            }

            fwrite(pEntry->mH264SpsPpsInfo.pBuffer, 1, pEntry.mH264SpsPpsInfo.nLength, fp_bs);
        }

        fwrite(pOutFrame->pBuffer, 1, pOutFrame->nBufferLen, fp_bs);
        fwrite(pOutFrame->pBufferExtra, 1, pOutFrame->nBufferExtraLen, fp_bs);
        alogv("nBufferLen = %d nBufferExtraLen = %d KeyFrame = %d", pOutFrame->nBufferLen, pOutFrame->nBufferExtraLen, (pOutFrame->nFlags & CEDARV_FLAG_KEYFRAME) != 0);
    }
#endif

    //if (pBuffer->nInputPortIndex == pRecRenderData->sInPortDef[RECR_PORT_INDEX_VIDEO].nPortIndex)
    if ((TRUE == bFindFlag) && (COMP_PortDomainVideo == ePortDomain))
    {// VENC input tunnel mode
        if (pOutFrame->nFilledLen == 0)
        {
            //alogw("Video Pkt[ID=%d Pts=%lld] FilledLen[%d] not Match, BufferLen=%d BufferExtraLen=%d",
            //    pOutFrame->nID, pOutFrame->nTimeStamp, pOutFrame->nFilledLen, pOutFrame->nBufferLen, pOutFrame->nBufferExtraLen);
            //pOutFrame->nFilledLen = pOutFrame->nBufferLen + pOutFrame->nBufferExtraLen;
        }

        pthread_mutex_lock(&pRecRenderData->mVideoInputFrameListMutex);

        if(0 == Index)
        {
            pRecRenderData->video_frm_cnt++;
        }

        int nVideoStreamId = pRecRenderData->sInPortDef[Index].nPortIndex;
        if(-1 != pRecRenderData->mDebugInputPts[nVideoStreamId])
        {
            if (nVideoStreamId != pRecRenderData->media_inf.mMediaVideoInfo[Index].mStreamId)
            {
                aloge("fatal error! muxChn[%d] media_inf videoInfo[%d] streamId not match: %d!=%d", pRecRenderData->mMppChnInfo.mChnId,
                    Index, nVideoStreamId, pRecRenderData->media_inf.mMediaVideoInfo[Index].mStreamId);
            }
            if((pOutFrame->nTimeStamp - pRecRenderData->mDebugInputPts[nVideoStreamId])/1000
                > (int64_t)2*1000*1000/pRecRenderData->media_inf.mMediaVideoInfo[Index].uVideoFrmRate)
            {
                alogw("muxChn[%d] rrn_v_pts_invalid:[%d]-%lld-%lld-%lld-%d-%d-%d",
                    pRecRenderData->mMppChnInfo.mChnId,
                    nVideoStreamId, pRecRenderData->mDebugInputPts[nVideoStreamId], pOutFrame->nTimeStamp,
                    (pOutFrame->nTimeStamp - pRecRenderData->mDebugInputPts[nVideoStreamId])/1000,
                    pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt,
                    pRecRenderData->media_inf.mMediaVideoInfo[Index].uVideoFrmRate/1000);
            }
        }
        pRecRenderData->mDebugInputPts[nVideoStreamId] = pOutFrame->nTimeStamp;

        // Ensure there is Idle node
        if (list_empty(&pRecRenderData->mVideoInputFrameIdleList))
        {
            alogv("Low probability! RecRender idle frame is empty! Total Num = %d", pRecRenderData->mVideoInputFrameNum);
            ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
            if (NULL == pNode)
            {
                pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
                pthread_mutex_unlock(&pRecRenderData->mStateMutex);
                aloge("fatal error! malloc fail!");
                eError = ERR_MUX_NOMEM;
                return eError;
            }
            memset(pNode, 0, sizeof(ENCODER_NODE_T));
            list_add_tail(&pNode->mList, &pRecRenderData->mVideoInputFrameIdleList);
            pRecRenderData->mVideoInputFrameNum++;
            if(0 == pRecRenderData->mVideoInputFrameNum%100)
            {
                alogd("Be careful! RecRender input frame total Num increase to %d", pRecRenderData->mVideoInputFrameNum);
            }
        }

        // Move the node from IdelList to ReadyList
        ENCODER_NODE_T *pFirstNode = list_first_entry(&pRecRenderData->mVideoInputFrameIdleList, ENCODER_NODE_T, mList);
        memcpy(&pFirstNode->stEncodedStream, pOutFrame, sizeof(EncodedStream));
        pFirstNode->mUsedRefCnt = 0;  // clear ref count when moving it to ReadyList
        pFirstNode->mPortIndex = pRecRenderData->sInPortDef[Index].nPortIndex;
        list_move_tail(&pFirstNode->mList, &pRecRenderData->mVideoInputFrameReadyList);

        // Send Input data valid message
        if (pRecRenderData->mNoInputFrameFlag)
        {
            pRecRenderData->mNoInputFrameFlag = 0;
            message_t msg;
            msg.command = RecRenderComp_InputFrameAvailable;
            put_message(&pRecRenderData->cmd_queue, &msg);
        }
        pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
    }
    //else if(pBuffer->nInputPortIndex == pRecRenderData->sInPortDef[RECR_PORT_INDEX_AUDIO].nPortIndex)
    else if ((TRUE == bFindFlag && (COMP_PortDomainAudio == ePortDomain)))
    {// AENC input tunnel mode
        if (pOutFrame->nFilledLen == 0)
        {
            alogw("Audio Pkt[ID=%d Pts=%lld] FilledLen[%d] not Match, BufferLen=%d BufferExtraLen=%d",
                pOutFrame->nID, pOutFrame->nTimeStamp, pOutFrame->nFilledLen, pOutFrame->nBufferLen, pOutFrame->nBufferExtraLen);
            //pOutFrame->nFilledLen = pOutFrame->nBufferLen + pOutFrame->nBufferExtraLen;
        }

        pthread_mutex_lock(&pRecRenderData->mAudioInputFrameListMutex);

        pRecRenderData->audio_frm_cnt++;

        int nAudioStreamId = pRecRenderData->sInPortDef[Index].nPortIndex;
        if(-1 != pRecRenderData->mDebugInputPts[nAudioStreamId])
        {
            if((pOutFrame->nTimeStamp - pRecRenderData->mDebugInputPts[nAudioStreamId])/1000 > 2*1024*1000/pRecRenderData->media_inf.sample_rate)
            {
                aloge("muxChn[%d] rrn_a_pts_invalid:[%d]-%lld-%lld-%lld-%d-%d",
                    pRecRenderData->mMppChnInfo.mChnId,
                    nAudioStreamId, pRecRenderData->mDebugInputPts[nAudioStreamId], pOutFrame->nTimeStamp,
                    (pOutFrame->nTimeStamp - pRecRenderData->mDebugInputPts[nAudioStreamId])/1000,
                    pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt);
            }
        }
        pRecRenderData->mDebugInputPts[nAudioStreamId] = pOutFrame->nTimeStamp;

        // Ensure there is Idle node
        if (list_empty(&pRecRenderData->mAudioInputFrameIdleList))
        {
            alogw("Low probability! RecRender idle frame is empty! Total Num = %d", pRecRenderData->mAudioInputFrameNum);
            ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
            if (NULL == pNode)
            {
                pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
                pthread_mutex_unlock(&pRecRenderData->mStateMutex);
                aloge("fatal error! malloc fail!");
                eError = ERR_MUX_NOMEM;
                return eError;
            }
            memset(pNode, 0, sizeof(ENCODER_NODE_T));
            list_add_tail(&pNode->mList, &pRecRenderData->mAudioInputFrameIdleList);
            pRecRenderData->mAudioInputFrameNum++;
        }

        // Move the node from IdelList to ReadyList
        ENCODER_NODE_T *pFirstNode = list_first_entry(&pRecRenderData->mAudioInputFrameIdleList, ENCODER_NODE_T, mList);
        memcpy(&pFirstNode->stEncodedStream, pOutFrame, sizeof(EncodedStream));
        pFirstNode->mUsedRefCnt = 0;  // clear ref count when moving it to ReadyList
        pFirstNode->mPortIndex = pRecRenderData->sInPortDef[Index].nPortIndex;
        //alogd("pFirstNode->mPortIndex: %d", pFirstNode->mPortIndex);
        list_move_tail(&pFirstNode->mList, &pRecRenderData->mAudioInputFrameReadyList);

        if (pRecRenderData->mNoInputFrameFlag)
        {
            pRecRenderData->mNoInputFrameFlag = 0;
            message_t msg;
            msg.command = RecRenderComp_InputFrameAvailable;
            put_message(&pRecRenderData->cmd_queue, &msg);
        }
        pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
    }
    //else if(pBuffer->nInputPortIndex == pRecRenderData->sInPortDef[RECR_PORT_INDEX_TEXT].nPortIndex)
    else if ((TRUE == bFindFlag) && (COMP_PortDomainText == ePortDomain))
    {
        if (pOutFrame->nFilledLen == 0)
        {
            alogw("text Pkt[ID=%d Pts=%lld] FilledLen[%d] not Match, BufferLen=%d BufferExtraLen=%d"
                ,pOutFrame->nID, pOutFrame->nTimeStamp, pOutFrame->nFilledLen, pOutFrame->nBufferLen, pOutFrame->nBufferExtraLen);
            //pOutFrame->nFilledLen = pOutFrame->nBufferLen + pOutFrame->nBufferExtraLen;
        }

        pthread_mutex_lock(&pRecRenderData->mTextInputFrameListMutex);

        if (list_empty(&pRecRenderData->mTextInputFrameIdleList))
        {
            alogw("Low probability! RecRender idle frame is empty! Total Num = %d", pRecRenderData->mAudioInputFrameNum);
            if(FALSE == pRecRenderData->mInputPortTunnelFlag[Index])
            {
                aloge("fatal error! no idle text node in non-tunnel mode.");
            }
            ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
            if (NULL == pNode)
            {
                pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
                pthread_mutex_unlock(&pRecRenderData->mStateMutex);
                aloge("fatal error! malloc fail!");
                eError = ERR_MUX_NOMEM;
                return eError;
            }
            memset(pNode, 0, sizeof(ENCODER_NODE_T));
            list_add_tail(&pNode->mList, &pRecRenderData->mTextInputFrameIdleList);
            pRecRenderData->mTextInputFrameNum++;
        }

        // Move the node from IdelList to ReadyList
        ENCODER_NODE_T *pFirstNode = list_first_entry(&pRecRenderData->mTextInputFrameIdleList, ENCODER_NODE_T, mList);
        memcpy(&pFirstNode->stEncodedStream, pOutFrame, sizeof(EncodedStream));
        pFirstNode->mUsedRefCnt = 0;  // clear ref count when moving it to ReadyList
        pFirstNode->mPortIndex = pRecRenderData->sInPortDef[Index].nPortIndex;
        list_move_tail(&pFirstNode->mList, &pRecRenderData->mTextInputFrameReadyList);
        if(pRecRenderData->mNoInputFrameFlag)
        {
            pRecRenderData->mNoInputFrameFlag = 0;
            message_t msg;
            msg.command = RecRenderComp_InputFrameAvailable;
            put_message(&pRecRenderData->cmd_queue, &msg);
        }
        pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
    }
    else
    {
        aloge("fatal error! inputPortIndex[%d] match nothing!", pBuffer->nInputPortIndex);
    }
    pthread_mutex_unlock(&pRecRenderData->mStateMutex);
    return eError;
}

ERRORTYPE RecRenderComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;
    int err = 0;
    CompInternalMsgType eCmd = Stop;
    message_t msg;
    //int ret;
    int i;
    //ERRORTYPE omx_ret;
    memset(&msg, 0, sizeof(message_t));
    pRecRenderData = (RECRENDERDATATYPE *) (((MM_COMPONENTTYPE*) hComponent)->pComponentPrivate);

    // should already release all video frame
    int iIdleVideoFrameCnt = 0;
    struct list_head *pList;
    list_for_each(pList, &pRecRenderData->mVideoInputFrameIdleList)
    {
        iIdleVideoFrameCnt++;
    }
    if (iIdleVideoFrameCnt < pRecRenderData->mVideoInputFrameNum)
    {
        aloge("fatal error! inputFrames[%d]<[%d] must return all before!", iIdleVideoFrameCnt, pRecRenderData->mVideoInputFrameNum);
    }
    if (!list_empty(&pRecRenderData->mVideoInputFrameReadyList))
    {
        aloge("fatal error! why readyInputFrame is not empty?");
    }
    if (!list_empty(&pRecRenderData->mVideoInputFrameUsedList))
    {
        aloge("fatal error! why usedInputFrame is not empty?");
    }
    if (!list_empty(&pRecRenderData->mVideoInputFrameIdleList))
    {
        ENCODER_NODE_T *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pRecRenderData->mVideoInputFrameIdleList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    if(pRecRenderData->mVideoInputFrameNum != MUX_FIFO_LEVEL)
    {
        alogw("Low probability! RecRender idle frame Total Num: %d -> %d", MUX_FIFO_LEVEL, pRecRenderData->mVideoInputFrameNum);
    }
    // should already release all audio frame
    int iIdleAudioFrameCnt = 0;
    list_for_each(pList, &pRecRenderData->mAudioInputFrameIdleList)
    {
        iIdleAudioFrameCnt++;
    }
    if (iIdleAudioFrameCnt < pRecRenderData->mAudioInputFrameNum)
    {
        aloge("fatal error! inputFrames[%d]<[%d] must return all before!", iIdleAudioFrameCnt, pRecRenderData->mAudioInputFrameNum);
    }
    if (!list_empty(&pRecRenderData->mAudioInputFrameReadyList))
    {
        aloge("fatal error! why readyInputFrame is not empty?");
    }
    if (!list_empty(&pRecRenderData->mAudioInputFrameUsedList))
    {
        aloge("fatal error! why usedInputFrame is not empty?");
    }
    if (!list_empty(&pRecRenderData->mAudioInputFrameIdleList))
    {
        ENCODER_NODE_T *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pRecRenderData->mAudioInputFrameIdleList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }

    // should already release all text frame
    int iIdleTextFrameCnt = 0;
    list_for_each(pList, &pRecRenderData->mTextInputFrameIdleList)
    {
        iIdleTextFrameCnt++;
    }
    if (iIdleTextFrameCnt < pRecRenderData->mTextInputFrameNum)
    {
        aloge("fatal error! inputFrames[%d]<[%d] must return all before!", iIdleTextFrameCnt, pRecRenderData->mTextInputFrameNum);
    }
    if (!list_empty(&pRecRenderData->mTextInputFrameReadyList))
    {
        aloge("fatal error! why readyInputFrame is not empty?");
    }
    if (!list_empty(&pRecRenderData->mTextInputFrameUsedList))
    {
        aloge("fatal error! why usedInputFrame is not empty?");
    }
    if (!list_empty(&pRecRenderData->mTextInputFrameIdleList))
    {
        ENCODER_NODE_T *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pRecRenderData->mTextInputFrameIdleList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }

    msg.command = eCmd;
    put_message(&pRecRenderData->cmd_queue, &msg);
    //cdx_sem_up(&pRecRenderData->cdx_sem_wait_message);

    alogv("wait recorder render component exit!...");
    // Wait for thread to exit so we can get the status into "error"
    pthread_join(pRecRenderData->thread_id, (void*) &eError);

    //cdx_sem_deinit(&pRecRenderData->cdx_sem_wait_message);
    message_destroy(&pRecRenderData->cmd_queue);
    //data_quene_destroy(&pRecRenderData->Audio_buffer_quene);

#ifdef __SAVE_INPUT_BS__
    fclose(fp_bs);
#endif

#if 0
    if(pRecRenderData->venc_extradata_info.pBuffer)
    {
        free(pRecRenderData->venc_extradata_info.pBuffer);
        pRecRenderData->venc_extradata_info.pBuffer = NULL;
    }
#endif

    if (!list_empty(&pRecRenderData->mVencHeaderDataList))
    {
        VencHeaderDataNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pRecRenderData->mVencHeaderDataList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry->mH264SpsPpsInfo.pBuffer);
            pEntry->mH264SpsPpsInfo.pBuffer = NULL;
            free(pEntry);
        }
    }

    if (!list_empty(&pRecRenderData->mVeChnBindStreamIdList))
    {
        VeChnBindStreamIdNode *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pRecRenderData->mVeChnBindStreamIdList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }

    pthread_mutex_lock(&pRecRenderData->mRSPacketListMutex);
    if(!list_empty(&pRecRenderData->mPrefetchRSPacketList))
    {
        aloge("fatal error! prefetch RSPacket list is not empty! check code!");
    }
    if(!list_empty(&pRecRenderData->mValidRSPacketList))
    {
        aloge("fatal error! valid RSPacket list is not empty! check code!");
    }
    if(!list_empty(&pRecRenderData->mIdleRSPacketList))
    {
        RecSinkPacket   *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pRecRenderData->mIdleRSPacketList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pRecRenderData->mRSPacketListMutex);

    pthread_mutex_destroy(&pRecRenderData->mutex_reset_writer_lock);
    pthread_mutex_destroy(&pRecRenderData->mJudgeSwitchlock);
    pthread_mutex_destroy(&pRecRenderData->mMuxerInitlock);
    pthread_mutex_destroy(&pRecRenderData->mVideoInputFrameListMutex);
    pthread_mutex_destroy(&pRecRenderData->mAudioInputFrameListMutex);
    pthread_mutex_destroy(&pRecRenderData->mTextInputFrameListMutex);
    pthread_mutex_destroy(&pRecRenderData->mStateMutex);
    pthread_mutex_destroy(&pRecRenderData->mRSPacketListMutex);
    pthread_mutex_destroy(&pRecRenderData->mThmPicLock);

    //cdx_sem_deinit(&pRecRenderData->mSemSwitchFileNormalDone);
    if(pRecRenderData)
    {
        free(pRecRenderData);
        pRecRenderData = NULL;
    }
    //alogd("[%s]sync_1", strrchr(__FILE__, '/')+1);
    //sync();
    //alogd("[%s]sync_2", strrchr(__FILE__, '/')+1);
    alogd("recorder render component exited!");

    return eError;
}

/*****************************************************************************/
ERRORTYPE RecRenderComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE *pComp;
    RECRENDERDATATYPE *pRecRenderData;
    ERRORTYPE eError = SUCCESS;
    int err;
    int i;
    pComp = (MM_COMPONENTTYPE *) hComponent;

    // Create private data
    pRecRenderData = (RECRENDERDATATYPE *) malloc(sizeof(RECRENDERDATATYPE));
    memset(pRecRenderData, 0x0, sizeof(RECRENDERDATATYPE));

    pComp->pComponentPrivate = (void*) pRecRenderData;
    pRecRenderData->state = COMP_StateLoaded;
    err = pthread_mutex_init(&pRecRenderData->mStateMutex, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
        //eError = ERR_MUX_NOMEM;
        //goto EXIT;
    }
    pRecRenderData->hSelf = hComponent;
    // Initialize component data structures to default values
    pRecRenderData->sPortParam.nPorts = 0;
    pRecRenderData->sPortParam.nStartPortNumber = 0x0;
    //port def
    while (pRecRenderData->sPortParam.nPorts < MAX_REC_RENDER_PORTS)
    {
        pRecRenderData->sInPortDef[pRecRenderData->sPortParam.nPorts].nPortIndex = pRecRenderData->sPortParam.nPorts;
        pRecRenderData->sInPortDef[pRecRenderData->sPortParam.nPorts].eDir = COMP_DirInput;
        pRecRenderData->sInPortDef[pRecRenderData->sPortParam.nPorts].bEnabled = FALSE;
        pRecRenderData->sInPortDef[pRecRenderData->sPortParam.nPorts].eDomain = COMP_PortDomainVendorStartUnused;
        pRecRenderData->sPortParam.nPorts++;
    }
#if 0
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_VIDEO].nPortIndex = pRecRenderData->sPortParam.nPorts;
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_VIDEO].eDir = COMP_DirInput;
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_VIDEO].bEnabled = TRUE;
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_VIDEO].eDomain = COMP_PortDomainVideo;
    pRecRenderData->sPortParam.nPorts++;

    pRecRenderData->sInPortDef[RECR_PORT_INDEX_AUDIO].nPortIndex = pRecRenderData->sPortParam.nPorts;
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_AUDIO].eDir = COMP_DirInput;
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_AUDIO].bEnabled = TRUE;
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_AUDIO].eDomain = COMP_PortDomainAudio;
    pRecRenderData->sPortParam.nPorts++;

    pRecRenderData->sInPortDef[RECR_PORT_INDEX_TEXT].nPortIndex = pRecRenderData->sPortParam.nPorts;
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_TEXT].eDir = COMP_DirInput;
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_TEXT].bEnabled = TRUE;
    pRecRenderData->sInPortDef[RECR_PORT_INDEX_TEXT].eDomain = COMP_PortDomainText;
    pRecRenderData->sPortParam.nPorts++;
#endif
    //port tunnel info
    for (int nPortIndex = 0; nPortIndex < MAX_REC_RENDER_PORTS; nPortIndex++)
    {
        pRecRenderData->sInPortTunnelInfo[nPortIndex].nPortIndex = nPortIndex;
        pRecRenderData->sInPortTunnelInfo[nPortIndex].eTunnelType = TUNNEL_TYPE_COMMON;
    }
    //port buf supplier
    for (int nPortIndex = 0; nPortIndex < MAX_REC_RENDER_PORTS; nPortIndex++)
    {
        pRecRenderData->sPortBufSupplier[nPortIndex].nPortIndex = nPortIndex;
        pRecRenderData->sPortBufSupplier[nPortIndex].eBufferSupplier = COMP_BufferSupplyOutput;
    }
#if 0
    pRecRenderData->sPortBufSupplier[RECR_PORT_INDEX_VIDEO].nPortIndex = 0x0;
    pRecRenderData->sPortBufSupplier[RECR_PORT_INDEX_VIDEO].eBufferSupplier = COMP_BufferSupplyOutput;
    pRecRenderData->sPortBufSupplier[RECR_PORT_INDEX_AUDIO].nPortIndex = 0x1;
    pRecRenderData->sPortBufSupplier[RECR_PORT_INDEX_AUDIO].eBufferSupplier = COMP_BufferSupplyOutput;
    pRecRenderData->sPortBufSupplier[RECR_PORT_INDEX_TEXT].nPortIndex = 0x2;
    pRecRenderData->sPortBufSupplier[RECR_PORT_INDEX_TEXT].eBufferSupplier = COMP_BufferSupplyOutput;

    pRecRenderData->sInPortTunnelInfo[RECR_PORT_INDEX_VIDEO].nPortIndex = 0x0;
    pRecRenderData->sInPortTunnelInfo[RECR_PORT_INDEX_VIDEO].eTunnelType = TUNNEL_TYPE_COMMON;
    pRecRenderData->sInPortTunnelInfo[RECR_PORT_INDEX_AUDIO].nPortIndex = 0x1;
    pRecRenderData->sInPortTunnelInfo[RECR_PORT_INDEX_AUDIO].eTunnelType = TUNNEL_TYPE_COMMON;
    pRecRenderData->sInPortTunnelInfo[RECR_PORT_INDEX_TEXT].nPortIndex = 0x2;
    pRecRenderData->sInPortTunnelInfo[RECR_PORT_INDEX_TEXT].eTunnelType = TUNNEL_TYPE_COMMON;
#endif

    if(message_create(&pRecRenderData->cmd_queue) < 0)
    {
        aloge("fatal error! create message error!");
//        eError = ERR_MUX_NOMEM;
//        goto EXIT;
    }
    pRecRenderData->mNoInputFrameFlag = 0;
    pRecRenderData->callback_writer.parent = (void*)pRecRenderData;
    pRecRenderData->callback_writer.writer = CDXRecoder_WritePacket_CB;
    pRecRenderData->duration = 0;
    pRecRenderData->duration_audio = 0;
    pRecRenderData->duration_text = 0;
    pRecRenderData->mnBasePts = -1;
    pRecRenderData->mnAudioBasePts = -1;
    pRecRenderData->mnTextBasePts = -1;
    //Init media_inf
    for (int VideoInfoIndex = 0; VideoInfoIndex < MAX_VIDEO_TRACK_COUNT; VideoInfoIndex++)
    {
        pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].mVideoEncodeType = PT_MAX;
        pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].mStreamId = -1;
    }
    //pRecRenderData->mPath = NULL;
    pRecRenderData->nOutputFd = -1;
    pRecRenderData->nFallocateLen = 0;
    pRecRenderData->nCallbackOutFlag = FALSE;
    int j;
    for(j=0;j<MAX_TRACK_COUNT;j++)
    {
        pRecRenderData->mbTrackInit[j] = FALSE;
        pRecRenderData->mPrevPts[j] = -1;
        pRecRenderData->mBasePts[j] = -1;
        pRecRenderData->mInputPrevPts[j] = -1;
        pRecRenderData->mOrigBasePts[j] = -1;
        pRecRenderData->mDebugInputPts[j] = -1;
    }
    pRecRenderData->mRefVideoStreamIndex = 0;
    pRecRenderData->mValidStreamCount = -1;
    pRecRenderData->mBasePtsRefStreamIndex = -1;
    err = pthread_mutex_init(&pRecRenderData->mutex_reset_writer_lock, NULL);
    if(0!=err)
    {
        aloge("fatal error! pthread_mutex init fail");
        //return ERR_MUX_NOMEM;
    }
    err = pthread_mutex_init(&pRecRenderData->mJudgeSwitchlock, NULL);
    if(0!=err)
    {
        aloge("fatal error! pthread_mutex init fail");
    }
    err = pthread_mutex_init(&pRecRenderData->mMuxerInitlock, NULL);
    if(0!=err)
    {
        aloge("fatal error! pthread_mutex init fail");
    }
    pRecRenderData->nSwitchFd = -1;
    pRecRenderData->mCurFileEndTm = 0;
    pRecRenderData->mRecordMode = RECORDER_MODE_NONE;
    pRecRenderData->mFileDurationPolicy = RecordFileDurationPolicy_MinDuration;
    pRecRenderData->mbSdCardState = TRUE;
    pRecRenderData->mVideoPtsWriteMoreSt = -1;
    INIT_LIST_HEAD(&pRecRenderData->mPrefetchRSPacketList);
    INIT_LIST_HEAD(&pRecRenderData->mValidRSPacketList);
    INIT_LIST_HEAD(&pRecRenderData->mIdleRSPacketList);
    RecSinkPacket   *pRSPacket;
    for(i=0;i<RECSINK_MAX_PACKET_NUM;i++)
    {
        pRSPacket = (RecSinkPacket*)malloc(sizeof(RecSinkPacket));
        if(NULL == pRSPacket)
        {
            aloge("fatal error! malloc fail");
            //eError = ERR_MUX_NOMEM;
            break;
        }
        list_add_tail(&pRSPacket->mList, &pRecRenderData->mIdleRSPacketList);
    }
    err = pthread_mutex_init(&pRecRenderData->mRSPacketListMutex, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
    err = pthread_mutex_init(&pRecRenderData->mThmPicLock, NULL);
    if(err!=0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }

    // Init video output frame buffer
    INIT_LIST_HEAD(&pRecRenderData->mVideoInputFrameIdleList);
    INIT_LIST_HEAD(&pRecRenderData->mVideoInputFrameReadyList);
    INIT_LIST_HEAD(&pRecRenderData->mVideoInputFrameUsedList);
    for (int i = 0; i < MUX_FIFO_LEVEL; i++)
    {
        ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
        if (NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
            eError = ERR_MUX_NOMEM;
            goto EXIT;
        }
        memset(pNode, 0, sizeof(ENCODER_NODE_T));
        list_add_tail(&pNode->mList, &pRecRenderData->mVideoInputFrameIdleList);
        pRecRenderData->mVideoInputFrameNum++;
    }
//    pthread_condattr_t  condAttr;
//    pthread_condattr_init(&condAttr);
//    pthread_condattr_setclock(&condAttr, CLOCK_MONOTONIC);
//    err = pthread_cond_init(&pRecRenderData->mWaitVideoInputFrameCond, &condAttr);
//    if(err!=0)
//    {
//        aloge("fatal error! pthread cond init fail!");
//    }
    err = pthread_mutex_init(&pRecRenderData->mVideoInputFrameListMutex, NULL);
    if (err!=0)
    {
        aloge("pthread mutex init fail!");
        eError = ERR_MUX_NOMEM;
        goto EXIT;
    }

    // Init audio output frame buffer
    INIT_LIST_HEAD(&pRecRenderData->mAudioInputFrameIdleList);
    INIT_LIST_HEAD(&pRecRenderData->mAudioInputFrameReadyList);
    INIT_LIST_HEAD(&pRecRenderData->mAudioInputFrameUsedList);
    for (int i = 0; i < MUX_FIFO_LEVEL; i++)
    {
        ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
        if (NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
            eError = ERR_MUX_NOMEM;
            goto EXIT;
        }
        memset(pNode, 0, sizeof(ENCODER_NODE_T));
        list_add_tail(&pNode->mList, &pRecRenderData->mAudioInputFrameIdleList);
        pRecRenderData->mAudioInputFrameNum++;
    }
    err = pthread_mutex_init(&pRecRenderData->mAudioInputFrameListMutex, NULL);
    if(err!=0)
    {
        aloge("pthread mutex init fail!");
        eError = ERR_MUX_NOMEM;
        goto EXIT;
    }

    // Init text output frame buffer
    INIT_LIST_HEAD(&pRecRenderData->mTextInputFrameIdleList);
    INIT_LIST_HEAD(&pRecRenderData->mTextInputFrameReadyList);
    INIT_LIST_HEAD(&pRecRenderData->mTextInputFrameUsedList);
    for (int i = 0; i < MUX_FIFO_LEVEL; i++)
    {
        ENCODER_NODE_T *pNode = (ENCODER_NODE_T*)malloc(sizeof(ENCODER_NODE_T));
        if (NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
            eError = ERR_MUX_NOMEM;
            goto EXIT;
        }
        memset(pNode, 0, sizeof(ENCODER_NODE_T));
        list_add_tail(&pNode->mList, &pRecRenderData->mTextInputFrameIdleList);
        pRecRenderData->mTextInputFrameNum++;
    }

    err = pthread_mutex_init(&pRecRenderData->mTextInputFrameListMutex, NULL);
    if(err!=0)
    {
        aloge("pthread mutex init fail!");
        eError = ERR_MUX_NOMEM;
        goto EXIT;
    }

    INIT_LIST_HEAD(&pRecRenderData->mVencHeaderDataList);
    INIT_LIST_HEAD(&pRecRenderData->mVeChnBindStreamIdList);

    // Fill in function pointers
    pComp->SetCallbacks = RecRenderSetCallbacks;
    pComp->SendCommand  = RecRenderSendCommand;
    pComp->GetConfig = RecRenderGetConfig;
    pComp->SetConfig = RecRenderSetConfig;
    pComp->GetState = RecRenderGetState;
    pComp->ComponentTunnelRequest = RecRenderComponentTunnelRequest;
    pComp->EmptyThisBuffer = RecRenderEmptyThisBuffer;
    pComp->ComponentDeInit = RecRenderComponentDeInit;

    
#ifdef __SAVE_INPUT_BS__
    fp_bs = fopen("/data/input_video.bin", "wb");
#endif
    
    // Create the component thread
    err = pthread_create(&pRecRenderData->thread_id, NULL, RecRender_ComponentThread, pRecRenderData);
    if (err!=0)
    {
        aloge("fatal error! create thread fail[%d]", err);
        eError = ERR_MUX_NOMEM;
        goto EXIT;
    }

    EXIT: return eError;
}
/*
void adjustAvsCounter(void* pThreadData)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE*) pThreadData;
    int         mediaTimediff;
    OMX_TICKS       now_avs_counter;

    // avs_counter_get_time_us(&now_avs_counter);
    pRecRenderData->avs_counter->get_time(pRecRenderData->avs_counter, &now_avs_counter);
    alogv("now_avs_counter: %lld", now_avs_counter);

    mediaTimediff = (int)(pRecRenderData->duration - pRecRenderData->duration_audio);
    if(mediaTimediff > 100 || mediaTimediff < -100)
    {
#if 0
        int adjust_ratio;
        adjust_ratio = mediaTimediff / (AVS_ADJUST_PERIOD_MS/100);
        adjust_ratio = adjust_ratio >  1 ? 1 : adjust_ratio;
        adjust_ratio = adjust_ratio < -1 ? -1 : adjust_ratio;

        if(adjust_ratio){
            avs_counter_adjust(adjust_ratio);
        }
#else
        int adjust_ratio;
        adjust_ratio = mediaTimediff / (AVS_ADJUST_PERIOD_MS/100);
        adjust_ratio = adjust_ratio >  2 ? 2 : adjust_ratio;
        adjust_ratio = adjust_ratio < -2 ? -2 : adjust_ratio;

        // avs_counter_adjust_abs(adjust_ratio);
        pRecRenderData->avs_counter->adjust(pRecRenderData->avs_counter, adjust_ratio);
#endif
        alogd("----adjust ratio:%d, video:%lld audio:%lld diff:%d diff-percent:%d ----",
                adjust_ratio, pRecRenderData->duration, pRecRenderData->duration_audio,
                mediaTimediff, mediaTimediff / (AVS_ADJUST_PERIOD_MS/100));
    }
    else
    {
        //avs_counter_adjust_abs(0);
        pRecRenderData->avs_counter->adjust(pRecRenderData->avs_counter, 0);
        alogd("----adjust ratio: 0, video: %lld(ms), auido: %lld(ms), diff: %lld(ms)",
            pRecRenderData->duration, pRecRenderData->duration_audio, pRecRenderData->duration - pRecRenderData->duration_audio);
    }
}
*/
/*****************************************************************************/

/**
  select stream packet by time stamp.
  The min pts frame will be selected.
*/
static enum CodecType SelectStreamToGet(RECRENDERDATATYPE *pRecRenderData)
{
    enum CodecType eCodecType;
//    int nVideoCnt = 0;
//    int nAudioCnt = 0;
//    int nTextCnt = 0;
    int64_t nVideoPts = -1;
    int64_t nAudioPts = -1;
    int64_t nTextPts = -1;
    pthread_mutex_lock(&pRecRenderData->mVideoInputFrameListMutex);
    pthread_mutex_lock(&pRecRenderData->mAudioInputFrameListMutex);
    pthread_mutex_lock(&pRecRenderData->mTextInputFrameListMutex);
    if(pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO)
    {
//        struct list_head *pList;
//        list_for_each(pList, &pRecRenderData->mVideoInputFrameReadyList)
//        {
//            nVideoCnt++;
//        }
        if(!list_empty(&pRecRenderData->mVideoInputFrameReadyList))
        {
            ENCODER_NODE_T *pFirstNode = list_first_entry(&pRecRenderData->mVideoInputFrameReadyList, ENCODER_NODE_T, mList);
            nVideoPts = pFirstNode->stEncodedStream.nTimeStamp;
        }
    }
    if(pRecRenderData->mRecordMode & RECORDER_MODE_AUDIO)
    {
//        struct list_head *pList;
//        list_for_each(pList, &pRecRenderData->mAudioInputFrameReadyList)
//        {
//            nAudioCnt++;
//        }
        if(!list_empty(&pRecRenderData->mAudioInputFrameReadyList))
        {
            ENCODER_NODE_T *pFirstNode = list_first_entry(&pRecRenderData->mAudioInputFrameReadyList, ENCODER_NODE_T, mList);
            nAudioPts = pFirstNode->stEncodedStream.nTimeStamp;
        }
    }
    if(pRecRenderData->mRecordMode & RECORDER_MODE_TEXT)
    {
//        struct list_head *pList;
//        list_for_each(pList, &pRecRenderData->mTextInputFrameReadyList)
//        {
//            nTextCnt++;
//        }
        if(!list_empty(&pRecRenderData->mTextInputFrameReadyList))
        {
            ENCODER_NODE_T *pFirstNode = list_first_entry(&pRecRenderData->mTextInputFrameReadyList, ENCODER_NODE_T, mList);
            nTextPts = pFirstNode->stEncodedStream.nTimeStamp;
        }
    }
    int64_t nMinPts = -1;
    if(nVideoPts != -1)
    {
        nMinPts = nVideoPts;
        eCodecType = CODEC_TYPE_VIDEO;
    }
    if(nAudioPts != -1)
    {
        if(nAudioPts < nMinPts || -1 == nMinPts)
        {
            nMinPts = nAudioPts;
            eCodecType = CODEC_TYPE_AUDIO;
        }
    }
    if(nTextPts != -1)
    {
        if(nTextPts < nMinPts || -1 == nMinPts)
        {
            nMinPts = nTextPts;
            eCodecType = CODEC_TYPE_TEXT;
        }
    }
    if(-1 == nMinPts)
    {
        eCodecType = CODEC_TYPE_UNKNOWN;
        //set flag to indicate thread needs input frame.
        pRecRenderData->mNoInputFrameFlag = 1;
    }
    pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);
    pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);
    pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);
    return eCodecType;
}

static ERRORTYPE RecSinkIncreaseIdleRSPacketList(RECRENDERDATATYPE *pRecRenderData)
{
    ERRORTYPE   eError = SUCCESS;
    RecSinkPacket   *pRSPacket;
    int i;
    for(i=0;i<RECSINK_MAX_PACKET_NUM;i++)
    {
        pRSPacket = (RecSinkPacket*)malloc(sizeof(RecSinkPacket));
        if(NULL == pRSPacket)
        {
            aloge("fatal error! malloc fail");
            eError = ERR_MUX_NOMEM;
            break;
        }
        list_add_tail(&pRSPacket->mList, &pRecRenderData->mIdleRSPacketList);
    }
    return eError;
}

static ERRORTYPE RSSetRecSinkPacket(PARAM_IN RecSinkPacket *pDes, PARAM_IN RecSinkPacket *pSrc)
{
    pDes->mId           = pSrc->mId;
    pDes->mStreamType   = pSrc->mStreamType;
    pDes->mFlags        = pSrc->mFlags;
    pDes->mPts          = pSrc->mPts;
    pDes->mpData0       = pSrc->mpData0;
    pDes->mSize0        = pSrc->mSize0;
    pDes->mpData1       = pSrc->mpData1;
    pDes->mSize1        = pSrc->mSize1;
    pDes->mCurrQp       = pSrc->mCurrQp;
    pDes->mavQp         = pSrc->mavQp;
	pDes->mnGopIndex    = pSrc->mnGopIndex;
	pDes->mnFrameIndex  = pSrc->mnFrameIndex;
	pDes->mnTotalIndex  = pSrc->mnTotalIndex;

    pDes->nPackNum      = pSrc->nPackNum;
    memcpy(pDes->stPackInfos, pSrc->stPackInfos, sizeof(VencPackInfo)*MAX_OUTPUT_PACK_NUM);

    //pDes->mSourceType   = pSrc->mSourceType;
    pDes->mRefCnt       = 0;

    pDes->mStreamId = pSrc->mStreamId;
    return SUCCESS;
}

/**
  set packet content to one idle packet from idle list, and move it to valid list.
*/
static ERRORTYPE RecSinkPutRSPacket(RECRENDERDATATYPE *pRecRenderData, RecSinkPacket *pRSPacket)
{
    ERRORTYPE eError = SUCCESS;
    RecSinkPacket   *pEntryPacket;
    pthread_mutex_lock(&pRecRenderData->mRSPacketListMutex);
    if(list_empty(&pRecRenderData->mIdleRSPacketList))
    {
        alogw("muxChn[%d] idleRSPacketList are all used, malloc more!", pRecRenderData->mMppChnInfo.mChnId);
        if(SUCCESS!=RecSinkIncreaseIdleRSPacketList(pRecRenderData))
        {
            pthread_mutex_unlock(&pRecRenderData->mRSPacketListMutex);
            return ERR_MUX_NOMEM;
        }
    }
    pEntryPacket = list_first_entry(&pRecRenderData->mIdleRSPacketList, RecSinkPacket, mList);
    RSSetRecSinkPacket(pEntryPacket, pRSPacket);
    list_move_tail(&pEntryPacket->mList, &pRecRenderData->mValidRSPacketList);
    pthread_mutex_unlock(&pRecRenderData->mRSPacketListMutex);
    return eError;
}

/**
  move prefetch packets from prefetch list to valid list.
*/
static void RecSinkMovePrefetchRSPackets(RECRENDERDATATYPE *pRecRenderData)
{
    pthread_mutex_lock(&pRecRenderData->mRSPacketListMutex);
    list_splice_init(&pRecRenderData->mPrefetchRSPacketList, &pRecRenderData->mValidRSPacketList);
    pthread_mutex_unlock(&pRecRenderData->mRSPacketListMutex);
}
/**
  move valid packet from valid list, delete from valid list to make it independent.
*/
static RecSinkPacket* RecSinkGetRSPacket_l(RECRENDERDATATYPE *pRecRenderData)
{
    RecSinkPacket   *pRSPacket = NULL;
    if(!list_empty(&pRecRenderData->mValidRSPacketList))
    {
        pRSPacket = list_first_entry(&pRecRenderData->mValidRSPacketList, RecSinkPacket, mList);
        list_del(&pRSPacket->mList);
    }
    return pRSPacket;
}

static RecSinkPacket* RecSinkGetRSPacket(RECRENDERDATATYPE *pRecRenderData)
{
    RecSinkPacket   *pRSPacket = NULL;
    pthread_mutex_lock(&pRecRenderData->mRSPacketListMutex);
    pRSPacket = RecSinkGetRSPacket_l(pRecRenderData);
    pthread_mutex_unlock(&pRecRenderData->mRSPacketListMutex);
    return pRSPacket;
}

/**
  return independent valid packet, add it to idle list.
*/
static ERRORTYPE RecSinkReleaseRSPacket_l(RECRENDERDATATYPE *pRecRenderData, RecSinkPacket *pRSPacket)
{
    ERRORTYPE ret = RecRender_ReleaseBuffer(pRecRenderData, pRSPacket);
    if(ret != SUCCESS)
    {
        aloge("fatal error! muxChn[%d] release buffer fail[0x%x]", pRecRenderData->mMppChnInfo.mChnId, ret);
    }
    list_add_tail(&pRSPacket->mList, &pRecRenderData->mIdleRSPacketList);
    return ret;
}
static ERRORTYPE RecSinkReleaseRSPacket(RECRENDERDATATYPE *pRecRenderData, RecSinkPacket *pRSPacket)
{
    ERRORTYPE ret = RecRender_ReleaseBuffer(pRecRenderData, pRSPacket);
    if(ret != SUCCESS)
    {
        aloge("fatal error! muxChn[%d] release buffer fail[0x%x]", pRecRenderData->mMppChnInfo.mChnId, ret);
    }
    pthread_mutex_lock(&pRecRenderData->mRSPacketListMutex);
    list_add_tail(&pRSPacket->mList, &pRecRenderData->mIdleRSPacketList);
    pthread_mutex_unlock(&pRecRenderData->mRSPacketListMutex);
    return ret;
}
/**
  add independent valid packet to prefetch list.
*/
static ERRORTYPE RecSinkAddRSPacketToPrefetchList(RECRENDERDATATYPE *pRecRenderData, RecSinkPacket *pRSPacket)
{
    pthread_mutex_lock(&pRecRenderData->mRSPacketListMutex);
    list_add_tail(&pRSPacket->mList, &pRecRenderData->mPrefetchRSPacketList);
    pthread_mutex_unlock(&pRecRenderData->mRSPacketListMutex);
    return SUCCESS;
}

static void RecSinkUpdateStreamBasePts(RECRENDERDATATYPE *pRecRenderData, RecSinkPacket *pSrcPkt)
{
    if (!pRecRenderData->mbTrackInit[pSrcPkt->mStreamId])
	{
		pRecRenderData->mbTrackInit[pSrcPkt->mStreamId] = TRUE;
        pRecRenderData->mBasePts[pSrcPkt->mStreamId] = pSrcPkt->mPts;
        if(-1 == pRecRenderData->mBasePtsRefStreamIndex)
        {
            pRecRenderData->mBasePtsRefStreamIndex = pSrcPkt->mStreamId;
        }
        alogd("muxChn[%d] streamIdxBasePts[%d-%lld]us, RefStreamBasePts[%d-%lld], diff:%lld us!",
            pRecRenderData->mMppChnInfo.mChnId,
            pSrcPkt->mStreamId, pRecRenderData->mBasePts[pSrcPkt->mStreamId],
            pRecRenderData->mBasePtsRefStreamIndex, pRecRenderData->mBasePts[pRecRenderData->mBasePtsRefStreamIndex],
            pRecRenderData->mBasePts[pSrcPkt->mStreamId] - pRecRenderData->mBasePts[pRecRenderData->mBasePtsRefStreamIndex]);
        if(-1 == pRecRenderData->mOrigBasePts[pSrcPkt->mStreamId])
        {
            pRecRenderData->mOrigBasePts[pSrcPkt->mStreamId] = pSrcPkt->mPts;
            alogd("muxChn[%d]streamIdx[%d] OrigBasePts[%lld]us!", pRecRenderData->mMppChnInfo.mChnId,
                pSrcPkt->mStreamId, pSrcPkt->mPts);
        }
	}
    //if(pRecSink->mInputPrevPts[pDesPkt->stream_index] != -1)
    //{
    //    pDesPkt->duration = (pSrcPkt->mPts - pRecSink->mInputPrevPts[pDesPkt->stream_index])/1000;
    //    if(pDesPkt->duration <= 0)
    //    {
    //        aloge("fatal error! stream[%d], curPts[%lld]us<=prevPts[%lld]us!", pDesPkt->stream_index, pSrcPkt->mPts, pRecSink->mInputPrevPts[pDesPkt->stream_index]);
    //    }
    //}
    //if(pDesPkt->duration < 0)
    //{
    //    if(pSrcPkt->mStreamType == CODEC_TYPE_VIDEO)
    //    {
    //        pDesPkt->duration = 1000*1000 / pRecSink->mpMediaInf->uVideoFrmRate;
    //    }
    //    else if (pSrcPkt->mStreamType == CODEC_TYPE_AUDIO)
    //    {
    //        pDesPkt->duration = MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate;
    //    }
    //    else if (pSrcPkt->mStreamType == CODEC_TYPE_TEXT)
    //    {
    //        pDesPkt->duration = 1000;
    //    }
    //    alogd("first packet of stream[%d], duration[%d]ms!", pSrcPkt->mStreamType, pDesPkt->duration);
    //}
}

/**
Description:
    AVPacket,
    mbTrackInit[]
    mBasePts[]
    mPrevPts[]
    mDuration
    mDurationAudio
    mVideoFrameCounter

Parameters:

Return:

Time: 2014/2/18
*/
static ERRORTYPE RecSinkWriteRSPacket(RECRENDERDATATYPE *pRecRenderData, RecSinkPacket *pSrcPkt)
{
    int ret;
    CdxAVPacket packet;
    CdxAVPacket *pDesPkt = &packet;
    memset(pDesPkt, 0, sizeof(CdxAVPacket));
	pDesPkt->dts = -1;
    pDesPkt->data0 = pSrcPkt->mpData0;
    pDesPkt->size0 = pSrcPkt->mSize0;
    pDesPkt->data1 = pSrcPkt->mpData1;
    pDesPkt->size1 = pSrcPkt->mSize1;
    pDesPkt->stream_index = pSrcPkt->mStreamId;
    pDesPkt->flags = pSrcPkt->mFlags;
    pDesPkt->duration = -1;
    pDesPkt->pos = -1;
    pDesPkt->CurrQp = pSrcPkt->mCurrQp;
    pDesPkt->avQp = pSrcPkt->mavQp;
	pDesPkt->nGopIndex = pSrcPkt->mnGopIndex;
	pDesPkt->nFrameIndex = pSrcPkt->mnFrameIndex;
	pDesPkt->nTotalIndex = pSrcPkt->mnTotalIndex;
    pDesPkt->nPackNum = pSrcPkt->nPackNum;
    memcpy(pDesPkt->stPackInfos, pSrcPkt->stPackInfos, sizeof(VencPackInfo)*MAX_OUTPUT_PACK_NUM);

    //find VideoStream mediainfo
    int VideoInfoIndex = 0;
    for (VideoInfoIndex = 0; VideoInfoIndex < pRecRenderData->media_inf.mVideoInfoValidNum; VideoInfoIndex++)
    {
        if (pSrcPkt->mStreamId == pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].mStreamId)
        {
            //alogd("VideoInfoIndex: %d, VideoStreamId: %d", VideoInfoIndex,
                    //pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].mStreamId);
            break;
        }
    }
    RecSinkUpdateStreamBasePts(pRecRenderData, pSrcPkt);
    //note: pDesPkt->pts is not equal to pRecSink->mPrevPts[pDesPkt->stream_index], because we think as audio,video stream pts
    //is based on same system time, then new file's a/v stream time can use same basePts to implement avsync.
    pDesPkt->pts = pSrcPkt->mPts - pRecRenderData->mBasePts[pRecRenderData->mBasePtsRefStreamIndex];
    if(pDesPkt->pts < 0)
    {
        alogd("Be careful! muxChn[%d]maybe stream[%d]BasePts[%lld-%lld] < refStream[%d]BasePts[%lld], [%lld-%lld]us",
            pRecRenderData->mMppChnInfo.mChnId,
            pDesPkt->stream_index, pRecRenderData->mBasePts[pDesPkt->stream_index], pSrcPkt->mPts,
            pRecRenderData->mBasePtsRefStreamIndex, pRecRenderData->mBasePts[pRecRenderData->mBasePtsRefStreamIndex],
            pRecRenderData->mBasePts[pDesPkt->stream_index] - pRecRenderData->mBasePts[pRecRenderData->mBasePtsRefStreamIndex],
            pDesPkt->pts);
        pDesPkt->pts = 0;
    }
    //if(pRecSink->mPrevPts[pDesPkt->stream_index] >= 0)
    //{
    //    pDesPkt->duration = (int)((pDesPkt->pts - pRecSink->mPrevPts[pDesPkt->stream_index])/1000);
    //}
    pRecRenderData->mPrevPts[pDesPkt->stream_index] = pSrcPkt->mPts - pRecRenderData->mBasePts[pDesPkt->stream_index];

    if(pRecRenderData->mInputPrevPts[pDesPkt->stream_index] != -1)
    {
        //here detect if frame interval is normal, if frame interval is large, perhaps discarding frame happened.
        if (pSrcPkt->mStreamType == CODEC_TYPE_VIDEO)
        {
            if ((pSrcPkt->mPts - pRecRenderData->mInputPrevPts[pDesPkt->stream_index])/1000 >
                    1000*1000/pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate * 2)
            {
                alogw("rec_in_pts_v_invalid:muxChn[%d]:%lld-%lld=%lld ms",
                    pRecRenderData->mMppChnInfo.mChnId,
                    pSrcPkt->mPts, pRecRenderData->mInputPrevPts[pDesPkt->stream_index],
                    (pSrcPkt->mPts - pRecRenderData->mInputPrevPts[pDesPkt->stream_index])/1000);
            }
        }
        else if (pSrcPkt->mStreamType == CODEC_TYPE_AUDIO)
        {
            //if ((pSrcPkt->mPts - pRecSink->mInputPrevPts[pDesPkt->stream_index])/1000 > MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate *2)
            if ((pSrcPkt->mPts - pRecRenderData->mInputPrevPts[pDesPkt->stream_index])/1000 >
                    MAXDECODESAMPLE*1000 / pRecRenderData->media_inf.sample_rate * 2)
            {
                alogw("rec_in_pts_a_invalid:muxChn[%d]:%lld-%lld=%lld ms",
                    pRecRenderData->mMppChnInfo.mChnId,
                    pSrcPkt->mPts, pRecRenderData->mInputPrevPts[pDesPkt->stream_index],
                    (pSrcPkt->mPts - pRecRenderData->mInputPrevPts[pDesPkt->stream_index])/1000);
            }
        }
    }

    pRecRenderData->mInputPrevPts[pDesPkt->stream_index] = pSrcPkt->mPts;

    if(pSrcPkt->mStreamType == CODEC_TYPE_VIDEO)   //if base on videoBasePts, then video duration is accurate.
    {
        //pRecSink->mDuration += pDesPkt->duration;
        //pRecSink->mLoopDuration += pDesPkt->duration;
        //pRecSink->mDuration = lround((double)pDesPkt->pts/1000 + 1000*1000/pRecSink->mpMediaInf->uVideoFrmRate);
        //pRecRenderData->mDuration[pDesPkt->stream_index] = lround((double)pRecRenderData->mPrevPts[pDesPkt->stream_index]/1000
        //                        /*+ 1000*1000/pRecSink->mpMediaInf->mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate*/);
        pRecRenderData->mDuration[pDesPkt->stream_index] = (int)(pRecRenderData->mPrevPts[pDesPkt->stream_index]/1000);
        if(pRecRenderData->mDuration[pDesPkt->stream_index] <= 0)
        {
            //pDesPkt->duration = lround(1000*1000/pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate);
            pDesPkt->duration = 1000*1000/pRecRenderData->media_inf.mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate;
        }
        else
        {
            pDesPkt->duration = pRecRenderData->mDuration[pDesPkt->stream_index] - pRecRenderData->mPrevDuration[pDesPkt->stream_index];
        }
        pRecRenderData->mPrevDuration[pDesPkt->stream_index] = pRecRenderData->mDuration[pDesPkt->stream_index];
        if(pRecRenderData->mRefVideoStreamIndex == pSrcPkt->mStreamId)
        {
            //pRecSink->mLoopDuration = llround((double)(pSrcPkt->mPts - pRecSink->mOrigBasePts[pDesPkt->stream_index])/1000 +
                                                //1000*1000/pRecSink->mpMediaInf->uVideoFrmRate);
            //pRecRenderData->mLoopDuration = llround((double)(pSrcPkt->mPts - pRecRenderData->mOrigBasePts[pDesPkt->stream_index])/1000
            //                /*+ 1000*1000/pRecSink->mpMediaInf->mMediaVideoInfo[VideoInfoIndex].uVideoFrmRate*/);
            pRecRenderData->mLoopDuration = (pSrcPkt->mPts - pRecRenderData->mOrigBasePts[pDesPkt->stream_index])/1000;
            pRecRenderData->mVideoFrameCounter++;
            //pRecRenderData->video_frm_cnt++;
        }
        pRecRenderData->mFileSizeBytes += (int64_t)(pDesPkt->size0 + pDesPkt->size1 + 32);
    }
    else if (pSrcPkt->mStreamType == CODEC_TYPE_AUDIO)   //if base on videoBasePts, audio duration is not accurate.
    {
        //pRecSink->mDurationAudio += pDesPkt->duration;
        pRecRenderData->mDurationAudio = pRecRenderData->mPrevPts[pDesPkt->stream_index]/1000 /*+ MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate*/;
        if(pRecRenderData->mDurationAudio <= 0)
        {
            pDesPkt->duration = MAXDECODESAMPLE*1000 / pRecRenderData->media_inf.sample_rate;
        }
        else
        {
            pDesPkt->duration = pRecRenderData->mDurationAudio - pRecRenderData->mPrevDurationAudio;
        }
        pRecRenderData->mPrevDurationAudio = pRecRenderData->mDurationAudio;
        pRecRenderData->mFileSizeBytes += (int64_t)(pDesPkt->size0 + pDesPkt->size1);
        pRecRenderData->mLoopDurationAudio = (pSrcPkt->mPts - pRecRenderData->mOrigBasePts[pDesPkt->stream_index])/1000
                                                /*+ MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate*/;
        //pRecRenderData->audio_frm_cnt++;
        pRecRenderData->mAudioFrmCnt++;
        if(0 == (pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO))
        {
            pRecRenderData->mDuration[0] = pRecRenderData->mDurationAudio;
            pRecRenderData->mPrevDuration[0] = pRecRenderData->mPrevDurationAudio;
            pRecRenderData->mLoopDuration = pRecRenderData->mLoopDurationAudio;
        }
    }
	else if (pSrcPkt->mStreamType == CODEC_TYPE_TEXT)
	{
        //pRecSink->mDurationText += pDesPkt->duration;
        pRecRenderData->mDurationText = pRecRenderData->mPrevPts[pDesPkt->stream_index]/1000 /*+ 1000*/;
        pDesPkt->duration = pRecRenderData->mDurationText - pRecRenderData->mPrevDurationText;
        pRecRenderData->mPrevDurationText = pRecRenderData->mDurationText;
        pRecRenderData->mFileSizeBytes += (int64_t)(pDesPkt->size0 + pDesPkt->size1);
    }

    if(pRecRenderData->mbSdCardState || pRecRenderData->nCallbackOutFlag)
    {
		if (pRecRenderData->pWriter != NULL)
        {
            pthread_mutex_lock(&pRecRenderData->mThmPicLock);
            if(1 == pRecRenderData->rc_thm_pic_ready) // insert thm pic
            {
                CdxAVPacket thm_packet;
                memset(&thm_packet,0,sizeof(CdxAVPacket));

                thm_packet.flags |= AVPACKET_FLAG_THUMBNAIL;
                thm_packet.data0 = pRecRenderData->rc_thm_pic.p_thm_pic_addr;
                thm_packet.size0 = pRecRenderData->rc_thm_pic.thm_pic_size;

                ret = pRecRenderData->pWriter->MuxerWritePacket(pRecRenderData->pMuxerCtx, &thm_packet);
                if(0 != ret)
                {
                    aloge("rc_wrt_thm_pic_failed!!");
                }
                if(NULL != pRecRenderData->rc_thm_pic.p_thm_pic_addr)
                {
                    free(pRecRenderData->rc_thm_pic.p_thm_pic_addr);
                    pRecRenderData->rc_thm_pic.p_thm_pic_addr = NULL;
                }
                pRecRenderData->rc_thm_pic.thm_pic_size = 0;
                pRecRenderData->rc_thm_pic_ready = 0;
            }
            pthread_mutex_unlock(&pRecRenderData->mThmPicLock);
            ret = pRecRenderData->pWriter->MuxerWritePacket(pRecRenderData->pMuxerCtx, pDesPkt);
            if (ret != 0)
            {
                aloge("fatal error! muxChn[%d] muxerWritePacket FAILED", pRecRenderData->mMppChnInfo.mChnId);
            }
		}
        else
        {
			alogw("pWriter=NULL, muxChn[%d] not initialize", pRecRenderData->mMppChnInfo.mChnId);
		}
    }
    else
    {
        alogw("Sdcard is not exist, skip MuxerWritePacket()!");
    }
    return SUCCESS;
}

static ERRORTYPE RecSinkStreamCallback(void *hComp, int event)
{
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE*)hComp;
    alogd("muxChn[%d] WriteDisk fail: muxerFileType[0x%x], fd[%d]",
        pRecRenderData->mMppChnInfo.mChnId,
        pRecRenderData->mChnAttr.mMediaFileFormat, pRecRenderData->nOutputFd);
    pRecRenderData->pCallbacks->EventHandler(pRecRenderData->hSelf, pRecRenderData->pAppData, COMP_EventWriteDiskError,
                                            pRecRenderData->mMppChnInfo.mChnId, 0, NULL);
    return SUCCESS;
}

ERRORTYPE RecSinkMuxerInit(RECRENDERDATATYPE *pRecRenderData, RecSinkPacket *pRSPacket)
{
    int result = SUCCESS;
    int ret = 0;
    char *file_path = NULL;
    unsigned int nFallocateLen = pRecRenderData->nFallocateLen;

    pthread_mutex_lock(&pRecRenderData->mMuxerInitlock);
    alogd("muxChn[%d]: fd[%d], nCallbackOutFlag[%d], fileDurPolicy[%d], fileDur[%lld]ms, fileSize[%lld]bytes, first packet:%d-%d",
        pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nOutputFd, pRecRenderData->nCallbackOutFlag, pRecRenderData->mFileDurationPolicy,
        pRecRenderData->mChnAttr.mMaxFileDuration, pRecRenderData->mChnAttr.mMaxFileSizeBytes, pRSPacket->mStreamId, pRSPacket->mStreamType);
    if(pRecRenderData->rec_file == FILE_NORMAL)
    {
        pRecRenderData->mCurMaxFileDuration = pRecRenderData->mChnAttr.mMaxFileDuration;
        pRecRenderData->mCurFileEndTm += pRecRenderData->mCurMaxFileDuration;
    }
    else
    {
        aloge("fatal error! wrong rec file state[%d]", pRecRenderData->rec_file);
        pRecRenderData->mCurMaxFileDuration = pRecRenderData->mChnAttr.mMaxFileDuration;
        pRecRenderData->mCurFileEndTm += pRecRenderData->mCurMaxFileDuration;
    }

    alogd("muxChn[%d] muxer_init:%d-%d-%lld-%lld", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->reset_fd_flag,
        pRecRenderData->nOutputFd, pRecRenderData->mCurMaxFileDuration, pRecRenderData->mCurFileEndTm);
    if(FALSE == pRecRenderData->nCallbackOutFlag)
    {
        if(pRecRenderData->nOutputFd<0)
        {
            alogw("Be careful! nOutputFd<0 when call RecRender MuxerInit, have last chance to get fd!");
            if(pRecRenderData->reset_fd_flag)
            {
                if(pRecRenderData->nSwitchFd >= 0)
                {
                    pRecRenderData->nOutputFd = pRecRenderData->nSwitchFd;
                    pRecRenderData->nSwitchFd = -1;
                    nFallocateLen = pRecRenderData->nSwitchFdFallocateSize;
                    pRecRenderData->nSwitchFdFallocateSize = 0;
                    pRecRenderData->reset_fd_flag = FALSE;
                }
                else
                {
                    aloge("fatal error! pSinkInfo->nSwitchFd[%d] < 0", pRecRenderData->nSwitchFd);
                }
            }
            else
            {
                aloge("fatal error! reset_fd flag != TRUE");
            }
        }
        file_path = generateFilepathFromFd(pRecRenderData->nOutputFd);
        if(NULL != file_path)
        {
            alogd("muxChn[%d] muxer_init2:%d-%s", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nOutputFd, file_path);
            free(file_path);
            file_path = NULL;
        }
        else
        {
            aloge("fatal error! muxChn[%d] is not set nOutputFd[%d]", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nOutputFd);
        }
    }
    int i;
    for(i=0;i<MAX_VIDEO_TRACK_COUNT;i++)
    {
        pRecRenderData->mDuration[i] = 0;
        pRecRenderData->mPrevDuration[i] = 0;
        pRecRenderData->mPrefetchFlag[i] = FALSE;
        //pRecRenderData->need_to_force_i_frm[i] = 0;
        pRecRenderData->forced_i_frm[i] = 0;
        pRecRenderData->key_frm_sent[i] = 0;
        pRecRenderData->v_frm_drp_cnt[i] = 0;
    }
    pRecRenderData->mDurationAudio = 0;
    pRecRenderData->mDurationText = 0;
    pRecRenderData->mPrevDurationAudio = 0;
    pRecRenderData->mPrevDurationText = 0;
    pRecRenderData->mPrefetchFlagAudio = FALSE;
    pRecRenderData->mFileSizeBytes = 0;
    for(i=0;i<MAX_TRACK_COUNT;i++)
    {
        pRecRenderData->mbTrackInit[i] = FALSE;
        pRecRenderData->mPrevPts[i] = -1;
        pRecRenderData->mBasePts[i] = -1;
        pRecRenderData->mInputPrevPts[i] = -1;
    }
    pRecRenderData->mBasePtsRefStreamIndex = -1;
    pRecRenderData->mVideoFrameCounter = 0;
    pRecRenderData->mAudioFrmCnt = 0;
    if (pRecRenderData->pWriter == NULL)
    {
        pRecRenderData->pWriter = cedarx_record_writer_create(map_MEDIA_FILE_FORMAT_E_to_MUXERMODES(pRecRenderData->mChnAttr.mMediaFileFormat));
        if (NULL == pRecRenderData->pWriter)
        {
            aloge("fatal error! cedarx_record_writer_create failed");
            result = ERR_MUX_NOMEM;
            goto WRITER_CREATE_ERR;
        }
        pRecRenderData->pMuxerCtx = pRecRenderData->pWriter->MuxerOpen((int*)&ret);
        if (ret != SUCCESS)
        {
            aloge("fatal error! muxChn[%d] MuxerOpen failed[%d]", pRecRenderData->mMppChnInfo.mChnId);
            result = ERR_MUX_ILLEGAL_PARAM;
            goto MUXER_OPEN_ERR;
        }
        else
        {
            alogv("MuxerOpen OK");
        }
        //TODO: now, write fd and callback mode are mutex. In the future, we will improve it if necessary.
        if (FALSE == pRecRenderData->nCallbackOutFlag)
        {
            if(pRecRenderData->nOutputFd>=0)
            {
                pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SETFALLOCATELEN, (unsigned int)nFallocateLen, NULL);
                if (pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SETCACHEFD2, (unsigned int)pRecRenderData->nOutputFd, NULL) != 0)
                {
                    aloge("fatal error! SETCACHEFD2 failed");
                    result = ERR_MUX_ILLEGAL_PARAM;
                    goto SETCACHEFD_ERR;
                }
            }
            alogd("muxChn[%d] FileDurationPolicy[0x%x], use fsWriteMode[%d], simpleCacheSize[%ld]KB",
                    pRecRenderData->mMppChnInfo.mChnId,
                    pRecRenderData->mFileDurationPolicy, pRecRenderData->mChnAttr.mFsWriteMode, pRecRenderData->mChnAttr.mSimpleCacheSize/1024);
            if(FSWRITEMODE_CACHETHREAD == pRecRenderData->mChnAttr.mFsWriteMode)
            {
                aloge("fatal error! not use cacheThread mode now!");
            }
            pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SET_FS_WRITE_MODE, pRecRenderData->mChnAttr.mFsWriteMode, NULL);
            pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SET_FS_SIMPLE_CACHE_SIZE, pRecRenderData->mChnAttr.mSimpleCacheSize, NULL);

            cdx_write_callback_t  callback;
            callback.hComp = pRecRenderData;
            callback.cb = RecSinkStreamCallback;
            pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SET_STREAM_CALLBACK, 0, (void*)&callback);
        }
        else
        {
            //pRecRenderData->writer->MuxerIoctrl(pRecRenderData->p_muxer_ctx, SETOUTURL, (unsigned int)pRecRenderData->url);
            if (pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, REGISTER_WRITE_CALLBACK, 0, (void*)&pRecRenderData->callback_writer) != 0)
            {
                aloge("fatal error! REGISTER_WRITE_CALLBACK failed");
                result = ERR_MUX_ILLEGAL_PARAM;
                goto SETCACHEFD_ERR;
            }
        }
    }
    else
    {
        aloge("fatal error! muxChn[%d] pWriter[%p] is not NULL!", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->pWriter);
    }

    // mjpeg source form camera
//  if(pRecRenderData->is_compress_source == 1) {  /* gushiming compressed source */
//      pSinkInfo->pWriter->MuxerIoctrl(pSinkInfo->pMuxerCtx, SET_VIDEO_CODEC_ID, CODEC_ID_MJPEG);
//  }

    pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SET_ADD_REPAIR_INFO_FLAG, pRecRenderData->mChnAttr.mAddRepairInfo, NULL);
    pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SET_FILE_REPAIR_INTERVAL, pRecRenderData->mChnAttr.mMaxFrmsTagInterval, NULL);
    int tmp_ret =  pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SETAVPARA, 0, (void*)&pRecRenderData->media_inf);
    if(0 != tmp_ret)
    {
        aloge("muxer_set_media_info_failed!!");
        result = ERR_MUX_ILLEGAL_PARAM;
        goto SETCACHEFD_ERR;
    }

    if(pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO)
    {
        if (!list_empty(&pRecRenderData->mVencHeaderDataList))
        {
            VencHeaderDataNode *pEntry;
            list_for_each_entry(pEntry, &pRecRenderData->mVencHeaderDataList, mList)
            {
                if (pEntry->mH264SpsPpsInfo.pBuffer && pEntry->mH264SpsPpsInfo.nLength > 0)
                {
                    alogd("pEntry->mH264SpsPpsInfo.nLength: %d", pEntry->mH264SpsPpsInfo.nLength);
                    pRecRenderData->pWriter->MuxerWriteExtraData(pRecRenderData->pMuxerCtx, 
                            (unsigned char *)pEntry->mH264SpsPpsInfo.pBuffer, pEntry->mH264SpsPpsInfo.nLength, pEntry->mStreamId);
                }
            }
        }
        else
        {
            alogd("VencExtraDataList is empty! May be use MJPEG encode or code error?");
        }
    }
    int nAudioStreamNum = 0;
    if(pRecRenderData->mRecordMode & RECORDER_MODE_AUDIO)
    {
        nAudioStreamNum = 1;
        __extra_data_t  AudioExtraDataForMp4;
        MuxerGenerateAudioExtraData(&AudioExtraDataForMp4, &pRecRenderData->media_inf);
        pRecRenderData->pWriter->MuxerWriteExtraData(pRecRenderData->pMuxerCtx, AudioExtraDataForMp4.extra_data,
                                AudioExtraDataForMp4.extra_data_len, pRecRenderData->media_inf.mVideoInfoValidNum);
    }
    if(pRecRenderData->mRecordMode & RECORDER_MODE_TEXT)
    {   // do nothing
        pRecRenderData->pWriter->MuxerWriteExtraData(pRecRenderData->pMuxerCtx, NULL, 0, pRecRenderData->media_inf.mVideoInfoValidNum+nAudioStreamNum);
    }
    ret = pRecRenderData->pWriter->MuxerWriteHeader(pRecRenderData->pMuxerCtx);
    if (ret != 0)
    {
        aloge("fatal error! write header failed");
        result = ERR_MUX_ILLEGAL_PARAM;
        goto SETCACHEFD_ERR;
    }
    else
    {
        alogv("write header ok");
    }
//    if(FALSE==pSinkInfo->reset_fd_flag)
//    {
//        aloge("fatal error! why reset fd flag = 0?");
//    }
//    pSinkInfo->reset_fd_flag = FALSE;
    pRecRenderData->mbMuxerInit = TRUE;
    pthread_mutex_unlock(&pRecRenderData->mMuxerInitlock);
    return SUCCESS;

SETCACHEFD_ERR:
    pRecRenderData->pWriter->MuxerClose(pRecRenderData->pMuxerCtx);
MUXER_OPEN_ERR:
    cedarx_record_writer_destroy(pRecRenderData->pWriter);
    pRecRenderData->pWriter = NULL;
WRITER_CREATE_ERR:
    pthread_mutex_unlock(&pRecRenderData->mMuxerInitlock);
    return result;
}

/**
    1. now, one muxer can only support one method: fwrite or CallbackOut.
        In future, we will change it if necessary.

  @param

  @return

  @date 2014/7/12
*/
static ERRORTYPE RecSinkMuxerClose(RECRENDERDATATYPE *pRecRenderData, int clrFile)
{
    ERRORTYPE ret = SUCCESS;
	if (pRecRenderData->pWriter != NULL)
    {
        /*
        alogw("avsync_muxer_close:%d-%d-%d-%d--%lld-%lld-%lld-%lld--%d-%d",
				pSinkInfo->mMuxerGrpId,pSinkInfo->mpMediaInf->nWidth,
				pSinkInfo->mDuration,pSinkInfo->mDurationAudio,
				pSinkInfo->mLoopDuration,pSinkInfo->mLoopDurationAudio,
				pSinkInfo->mCurMaxFileDuration,pSinkInfo->mCurFileEndTm,
				pSinkInfo->mVideoFrameCounter,pSinkInfo->mAudioFrmCnt);
        */
        if(FALSE == pRecRenderData->nCallbackOutFlag)
        {
            if(pRecRenderData->mbSdCardState)
            {
                ret = pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SETTOTALTIME,
                    pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex], NULL);
                if(ret != 0)
                {
                    aloge("fatal error! writer->Muxer Ioctrl setTOTALTIME FAILED, ret: %d", ret);
                }
                ret = pRecRenderData->pWriter->MuxerWriteTrailer(pRecRenderData->pMuxerCtx);
                if(ret!=0)
                {
                    aloge("fatal error! muxChn[%d] muxer WriteTrailer ret:[%d]", pRecRenderData->mMppChnInfo.mChnId, ret);
                }
                ret = pRecRenderData->pWriter->MuxerClose(pRecRenderData->pMuxerCtx);
                if (ret != 0)
                {
                    aloge("fatal error! muxChn[%d] muxer Close failed, ret = %d", pRecRenderData->mMppChnInfo.mChnId, ret);
                }
                pRecRenderData->pMuxerCtx = NULL;
                //fflush(pSinkInfo->pOutputFile);
                if (clrFile == 1)
                {
                    aloge("fatal error! clrFile[%d]!=0", clrFile);
//                    #if (CDXCFG_FILE_SYSTEM==OPTION_FILE_SYSTEM_DIRECT_FATFS)
//                        fat_sync((int)pSinkInfo->pOutputFile);
//                        fat_lseek((int)pSinkInfo->pOutputFile, 0);
//                        fat_truncate((int)pSinkInfo->pOutputFile);
//                    #else
//                        fflush(pSinkInfo->pOutputFile);
//                        ftruncate(fileno(pSinkInfo->pOutputFile), 0);
//                        rewind(pSinkInfo->pOutputFile);
//                    #endif
                }
                else
                {
//                    alogd("before fflush");
//                    fflush(pSinkInfo->pOutputFile);
//                    alogd("before fsync");
//                    fsync(pSinkInfo->nOutputFd);
//                    alogd("after fsync");
                }
            }
            else
            {
                alogd("muxChn[%d] mbSdCardState[%d], sdcard is pull out", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mbSdCardState);
                pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SETSDCARDSTATE, pRecRenderData->mbSdCardState, NULL);
                pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SETTOTALTIME, pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex], NULL);
                pRecRenderData->pWriter->MuxerClose(pRecRenderData->pMuxerCtx);
                pRecRenderData->pMuxerCtx = NULL;
                alogd("muxChn[%d] mbSdCardState[%d], sdcard is pull out, muxer close done!",
                    pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mbSdCardState);
            }
            if(pRecRenderData->nOutputFd>=0)
            {
                close(pRecRenderData->nOutputFd);
                pRecRenderData->nOutputFd = -1;
            }
            else
            {
                aloge("fatal error! muxChn[%d] nOutputFd[%d]<0", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nOutputFd);
            }
            if(FSWRITEMODE_CACHETHREAD == pRecRenderData->mChnAttr.mFsWriteMode)
            {
                aloge("fatal error! muxChn[%d] not use cacheThread mode now!", pRecRenderData->mMppChnInfo.mChnId);
            }
        }
        else
        {
            pRecRenderData->pWriter->MuxerIoctrl(pRecRenderData->pMuxerCtx, SETTOTALTIME, pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex], NULL);
		    pRecRenderData->pWriter->MuxerWriteTrailer(pRecRenderData->pMuxerCtx);
		    pRecRenderData->pWriter->MuxerClose(pRecRenderData->pMuxerCtx);
            pRecRenderData->pMuxerCtx = NULL;
            //pSinkInfo->nCallbackOutFlag = FALSE;
        }
		cedarx_record_writer_destroy(pRecRenderData->pWriter);
		pRecRenderData->pWriter = NULL;
        pthread_mutex_lock(&pRecRenderData->mJudgeSwitchlock);
        pRecRenderData->bNeedSw = FALSE;
        pRecRenderData->bNeedSwAudio = FALSE;
        pRecRenderData->mbMuxerInit = FALSE;
        pthread_mutex_unlock(&pRecRenderData->mJudgeSwitchlock);
        pRecRenderData->pCallbacks->EventHandler(pRecRenderData->hSelf, pRecRenderData->pAppData, COMP_EventRecordDone, pRecRenderData->mMppChnInfo.mChnId, 0, NULL);
	}
    else
    {
        aloge("fatal error! muxChn[%d] record writer is null", pRecRenderData->mMppChnInfo.mChnId);
        pRecRenderData->mbMuxerInit = FALSE;
    }
    int videoStreamIndex = pRecRenderData->mRefVideoStreamIndex;
    int audioStreamIndex = pRecRenderData->media_inf.mVideoInfoValidNum;
    // alogd("TOTAL duration: %d(ms)", pSinkInfo->mDuration);
    // alogd("TOTAL duration audio: %d(ms)", pSinkInfo->mDurationAudio);
    // alogd("TOTAL duration text: %d(ms)", pSinkInfo->mDurationText);
    alogd("muxChn[%d] LOOP duration:[%lld-%lld]ms, lastPts v[%lld]-a[%lld]=[%lld]ms, duration:[%d-%d-%d]ms,"
        "curInputPts v[%lld]-a[%lld]=[%lld]ms, fileSize:[%lld]bytes, needSwitchFlag:%d-%d",
        pRecRenderData->mMppChnInfo.mChnId,
        pRecRenderData->mLoopDuration, pRecRenderData->mLoopDurationAudio,
        (pRecRenderData->mPrevPts[videoStreamIndex] + pRecRenderData->mBasePts[videoStreamIndex])/1000,
        (pRecRenderData->mPrevPts[audioStreamIndex] + pRecRenderData->mBasePts[audioStreamIndex])/1000,
        (pRecRenderData->mPrevPts[videoStreamIndex] + pRecRenderData->mBasePts[videoStreamIndex] - (pRecRenderData->mPrevPts[audioStreamIndex] + pRecRenderData->mBasePts[audioStreamIndex]))/1000,
        pRecRenderData->mDuration[videoStreamIndex], pRecRenderData->mDurationAudio, pRecRenderData->mDurationText,
        pRecRenderData->mDebugInputPts[videoStreamIndex]/1000,
        pRecRenderData->mDebugInputPts[audioStreamIndex]/1000,
        (pRecRenderData->mDebugInputPts[videoStreamIndex] - pRecRenderData->mDebugInputPts[audioStreamIndex])/1000,
        pRecRenderData->mFileSizeBytes, pRecRenderData->bNeedSw, pRecRenderData->bNeedSwAudio);
    return SUCCESS;
}

static ERRORTYPE RecSinkDrainAllRSPackets(RECRENDERDATATYPE *pRecRenderData)
{
    int cnt = 0;
    int sendCnt = 0;
    RecSinkPacket   *pEntry, *pTmp;
    RecSinkPacket   *pRSPacket;
    pthread_mutex_lock(&pRecRenderData->mRSPacketListMutex);
    alogd("muxChn[%d] muxerMode[%d] begin to drain packets", pRecRenderData->mMppChnInfo.mChnId,
        pRecRenderData->mChnAttr.mMediaFileFormat);
    while(1)
    {
        if(!pRecRenderData->mbSdCardState || pRecRenderData->mbShutDownNowFlag)
        {
            alogd("sdcard is pull out or shutdown now!");
            break;
        }
        pRSPacket = RecSinkGetRSPacket_l(pRecRenderData);
        if(pRSPacket)
        {
            cnt++;
            if(pRecRenderData->mbMuxerInit == FALSE)
            {
                BOOL bGrant = TRUE;
                if(pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO)
                {
                    if(!(pRSPacket->mFlags & AVPACKET_FLAG_KEYFRAME))
                    {
                        bGrant = FALSE;
                    }
                }
                if(bGrant)
                {
                    pthread_mutex_lock(&pRecRenderData->mutex_reset_writer_lock);
                    if(pRecRenderData->nCallbackOutFlag==TRUE || (pRecRenderData->nOutputFd>=0) || pRecRenderData->reset_fd_flag==TRUE)
                    {
                        if(RecSinkMuxerInit(pRecRenderData, pRSPacket) != SUCCESS)
                        {
                            aloge("fatal error! muxChn[%d] MuxerInit Error!", pRecRenderData->mMppChnInfo.mChnId);
                            //pRecRenderData->state = COMP_StateInvalid;   //OMX_StateIdle;
                            pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
                            RecSinkReleaseRSPacket_l(pRecRenderData, pRSPacket);
                            break;
                        }
                    }
                    pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
                }
            }
            if(pRecRenderData->mbMuxerInit)
            {
                RecSinkWriteRSPacket(pRecRenderData, pRSPacket);
            }
            //release RSPacket
            RecSinkReleaseRSPacket_l(pRecRenderData, pRSPacket);
        }
        else
        {
            break;
        }
    }
    if(!list_empty(&pRecRenderData->mValidRSPacketList))
    {
        list_for_each_entry_safe(pEntry, pTmp, &pRecRenderData->mValidRSPacketList, mList)
        {
            //pRecSink->mpCallbacks->EmptyBufferDone(pRecSink, pRecSink->mpAppData, pEntry);
            RecRender_ReleaseBuffer(pRecRenderData, pEntry);
            list_move_tail(&pEntry->mList, &pRecRenderData->mIdleRSPacketList);
            cnt++;
            sendCnt++;
        }
        alogd("left [%d]packets to send out immediately", sendCnt);
    }
    pthread_mutex_unlock(&pRecRenderData->mRSPacketListMutex);
    alogd("muxChn[%d]muxerMode[%d] drain [%d]packets, ShutDownNow[%d]",
        pRecRenderData->mMppChnInfo.mChnId,
        map_MEDIA_FILE_FORMAT_E_to_MUXERMODES(pRecRenderData->mChnAttr.mMediaFileFormat),
        cnt, pRecRenderData->mbShutDownNowFlag);
    return SUCCESS;
}

static ERRORTYPE RecRenderReleaseAllFrames(RECRENDERDATATYPE *pRecRenderData)
{
    ERRORTYPE eRet;
    pthread_mutex_lock(&pRecRenderData->mVideoInputFrameListMutex);
    ENCODER_NODE_T *pVEntry, *pVTmp;
    if (!list_empty(&pRecRenderData->mVideoInputFrameUsedList))
    {
        aloge("fatal error! muxChn[%d] Used video frame should all released before!", pRecRenderData->mMppChnInfo.mChnId);
    }
    if (!list_empty(&pRecRenderData->mVideoInputFrameReadyList))
    {
        int nCnt = 0;
        list_for_each_entry_safe(pVEntry, pVTmp, &pRecRenderData->mVideoInputFrameReadyList, mList)
        {
            nCnt++;
            if(pVEntry->mUsedRefCnt != 0)
            {
                aloge("fatal error! muxChn[%d] RefCnt[%d]", pRecRenderData->mMppChnInfo.mChnId, pVEntry->mUsedRefCnt);
            }
            list_move_tail(&pVEntry->mList, &pRecRenderData->mVideoInputFrameIdleList);
            if(pVEntry->mPortIndex >= MAX_REC_RENDER_PORTS)
            {
                aloge("fatal error! check code! port index:%d", pVEntry->mPortIndex);
            }
            if(pRecRenderData->sInPortDef[pVEntry->mPortIndex].nPortIndex != pVEntry->mPortIndex)
            {
                aloge("fatal error! check port index and suffix:%d!=%d", pRecRenderData->sInPortDef[pVEntry->mPortIndex].nPortIndex, pVEntry->mPortIndex);
            }
            if(pRecRenderData->mInputPortTunnelFlag[pVEntry->mPortIndex])
            {
                if (COMP_PortDomainVideo != pRecRenderData->sInPortDef[pVEntry->mPortIndex].eDomain)
                {
                    aloge("fatal error! port[%d] domain[0x%x] is not video!", pVEntry->mPortIndex, pRecRenderData->sInPortDef[pVEntry->mPortIndex].eDomain);
                }
                COMP_BUFFERHEADERTYPE obh;
                obh.nOutputPortIndex   = pRecRenderData->sInPortTunnelInfo[pVEntry->mPortIndex].nTunnelPortIndex;
                obh.nInputPortIndex    = pRecRenderData->sInPortTunnelInfo[pVEntry->mPortIndex].nPortIndex;
                obh.pOutputPortPrivate = (void*)&pVEntry->stEncodedStream;
                eRet = COMP_FillThisBuffer(pRecRenderData->sInPortTunnelInfo[pVEntry->mPortIndex].hTunnel, &obh);
                if(eRet != SUCCESS)
                {
                    aloge("fatal error! fill this buffer fail[0x%x], video frame id=[%d], discard it!", eRet, pVEntry->stEncodedStream.nID);
                }
            }
            else
            {
                COMP_BUFFERHEADERTYPE obh;
                obh.nInputPortIndex = pRecRenderData->sInPortTunnelInfo[pVEntry->mPortIndex].nPortIndex;
                obh.pAppPrivate = (void*)&pVEntry->stEncodedStream;
                if(pVEntry->mPortIndex != obh.nInputPortIndex)
                {
                    aloge("fatal error! muxChn[%d] video streamId[%d!=%d]", pRecRenderData->mMppChnInfo.mChnId,
                        pVEntry->mPortIndex, obh.nInputPortIndex);
                }
                pRecRenderData->pCallbacks->EmptyBufferDone(pRecRenderData->hSelf, pRecRenderData->pAppData, &obh);
            }
        }
        alogd("muxChn[%d] [%d]Ready video frames should all release before!", pRecRenderData->mMppChnInfo.mChnId, nCnt);
    }

    int iIdleVideoFrameCnt = 0;
    list_for_each_entry(pVEntry, &pRecRenderData->mVideoInputFrameIdleList, mList)
    {
        iIdleVideoFrameCnt++;
    }
    if (iIdleVideoFrameCnt != pRecRenderData->mVideoInputFrameNum)
    {
        aloge("fatal error! muxChn[%d] video input idle frames [%d]<[%d] must return all before",
            pRecRenderData->mMppChnInfo.mChnId, iIdleVideoFrameCnt, pRecRenderData->mVideoInputFrameNum);
    }
    else
    {
        //alogd("Release all video input frame [%d]", pRecRenderData->mVideoInputFrameNum);
    }
    pthread_mutex_unlock(&pRecRenderData->mVideoInputFrameListMutex);

    pthread_mutex_lock(&pRecRenderData->mAudioInputFrameListMutex);
    ENCODER_NODE_T *pAEntry, *pATmp;
    if (!list_empty(&pRecRenderData->mAudioInputFrameUsedList))
    {
        aloge("fatal error! muxChn[%d] used audio frame should all released before!", pRecRenderData->mMppChnInfo.mChnId);
    }
    if (!list_empty(&pRecRenderData->mAudioInputFrameReadyList))
    {
        int nCnt = 0;
        list_for_each_entry_safe(pAEntry, pATmp, &pRecRenderData->mAudioInputFrameReadyList, mList)
        {
            nCnt++;
            if(pAEntry->mUsedRefCnt != 0)
            {
                aloge("fatal error! muxChn[%d] RefCnt[%d]", pRecRenderData->mMppChnInfo.mChnId, pAEntry->mUsedRefCnt);
            }
            list_move_tail(&pAEntry->mList, &pRecRenderData->mAudioInputFrameIdleList);
            if(pAEntry->mPortIndex >= MAX_REC_RENDER_PORTS)
            {
                aloge("fatal error! check code! port index:%d", pAEntry->mPortIndex);
            }
            if(pRecRenderData->sInPortDef[pAEntry->mPortIndex].nPortIndex != pAEntry->mPortIndex)
            {
                aloge("fatal error! check port index and suffix:%d!=%d", pRecRenderData->sInPortDef[pAEntry->mPortIndex].nPortIndex, pAEntry->mPortIndex);
            }
            if(pRecRenderData->mInputPortTunnelFlag[pAEntry->mPortIndex])
            {
                if (COMP_PortDomainAudio != pRecRenderData->sInPortDef[pAEntry->mPortIndex].eDomain)
                {
                    aloge("fatal error! port[%d] domain[0x%x] is not audio!", pAEntry->mPortIndex, pRecRenderData->sInPortDef[pAEntry->mPortIndex].eDomain);
                }
                COMP_BUFFERHEADERTYPE obh;
                obh.nOutputPortIndex   = pRecRenderData->sInPortTunnelInfo[pAEntry->mPortIndex].nTunnelPortIndex;
                obh.nInputPortIndex    = pRecRenderData->sInPortTunnelInfo[pAEntry->mPortIndex].nPortIndex;
                obh.pOutputPortPrivate = (void*)&pAEntry->stEncodedStream;
                eRet = COMP_FillThisBuffer(pRecRenderData->sInPortTunnelInfo[pAEntry->mPortIndex].hTunnel, &obh);
                if(eRet != SUCCESS)
                {
                    aloge("fatal error! fill this buffer fail[0x%x], audio frame id=[%d], discard it!", eRet, pAEntry->stEncodedStream.nID);
                }
            }
            else
            {
                COMP_BUFFERHEADERTYPE obh;
                obh.nInputPortIndex = pRecRenderData->sInPortTunnelInfo[pAEntry->mPortIndex].nPortIndex;
                obh.pAppPrivate = (void*)&pAEntry->stEncodedStream;
                if(pAEntry->mPortIndex != obh.nInputPortIndex)
                {
                    aloge("fatal error! muxChn[%d] audio streamId[%d!=%d]", pRecRenderData->mMppChnInfo.mChnId,
                        pAEntry->mPortIndex, obh.nInputPortIndex);
                }
                pRecRenderData->pCallbacks->EmptyBufferDone(pRecRenderData->hSelf, pRecRenderData->pAppData, &obh);
            }
        }
        alogd("muxChn[%d] [%d]Ready audio frames should all released before!", pRecRenderData->mMppChnInfo.mChnId, nCnt);
    }

    int iIdleAudioFrameCnt = 0;
    list_for_each_entry(pAEntry, &pRecRenderData->mAudioInputFrameIdleList, mList)
    {
        iIdleAudioFrameCnt++;
    }
    if (iIdleAudioFrameCnt != pRecRenderData->mAudioInputFrameNum)
    {
        aloge("fatal error! muxChn[%d] audio input frames [%d]<[%d] must return all before",
            pRecRenderData->mMppChnInfo.mChnId, iIdleAudioFrameCnt, pRecRenderData->mAudioInputFrameNum);
    }
    else
    {
        //alogd("Release all audio input frame [%d]", pRecRenderData->mAudioInputFrameNum);
    }
    pthread_mutex_unlock(&pRecRenderData->mAudioInputFrameListMutex);

    pthread_mutex_lock(&pRecRenderData->mTextInputFrameListMutex);
    ENCODER_NODE_T *pTEntry, *pTTmp;
    if (!list_empty(&pRecRenderData->mTextInputFrameUsedList))
    {
        aloge("fatal error! muxChn[%d] used text frame should all released before!", pRecRenderData->mMppChnInfo.mChnId);
    }
    if (!list_empty(&pRecRenderData->mTextInputFrameReadyList))
    {
        int nCnt = 0;
        list_for_each_entry_safe(pTEntry, pTTmp, &pRecRenderData->mTextInputFrameReadyList, mList)
        {
            nCnt++;
            if(pTEntry->mUsedRefCnt != 0)
            {
                aloge("fatal error! muxChn[%d] RefCnt[%d]", pRecRenderData->mMppChnInfo.mChnId, pTEntry->mUsedRefCnt);
            }
            list_move_tail(&pTEntry->mList, &pRecRenderData->mTextInputFrameIdleList);
            if(pTEntry->mPortIndex >= MAX_REC_RENDER_PORTS)
            {
                aloge("fatal error! check code! port index:%d", pTEntry->mPortIndex);
            }
            if(pRecRenderData->sInPortDef[pTEntry->mPortIndex].nPortIndex != pTEntry->mPortIndex)
            {
                aloge("fatal error! check port index and suffix:%d!=%d", pRecRenderData->sInPortDef[pTEntry->mPortIndex].nPortIndex, pTEntry->mPortIndex);
            }
            if(pRecRenderData->mInputPortTunnelFlag[pTEntry->mPortIndex])
            {
                if (COMP_PortDomainText != pRecRenderData->sInPortDef[pTEntry->mPortIndex].eDomain)
                {
                    aloge("fatal error! port[%d] domain[0x%x] is not text!", pTEntry->mPortIndex, pRecRenderData->sInPortDef[pTEntry->mPortIndex].eDomain);
                }
                COMP_BUFFERHEADERTYPE obh;
                obh.nOutputPortIndex   = pRecRenderData->sInPortTunnelInfo[pTEntry->mPortIndex].nTunnelPortIndex;
                obh.nInputPortIndex    = pRecRenderData->sInPortTunnelInfo[pTEntry->mPortIndex].nPortIndex;
                obh.pOutputPortPrivate = (void*)&pTEntry->stEncodedStream;
                eRet = COMP_FillThisBuffer(pRecRenderData->sInPortTunnelInfo[pTEntry->mPortIndex].hTunnel, &obh);
                if(eRet != SUCCESS)
                {
                    aloge("fatal error! fill this buffer fail[0x%x], text frame id=[%d], discard it!", eRet, pTEntry->stEncodedStream.nID);
                }
            }
            else
            {
                COMP_BUFFERHEADERTYPE obh;
                obh.nInputPortIndex = pRecRenderData->sInPortTunnelInfo[pTEntry->mPortIndex].nPortIndex;
                obh.pAppPrivate = (void*)&pTEntry->stEncodedStream;
                if(pTEntry->mPortIndex != obh.nInputPortIndex)
                {
                    aloge("fatal error! muxChn[%d] text streamId[%d!=%d]", pRecRenderData->mMppChnInfo.mChnId,
                        pTEntry->mPortIndex, obh.nInputPortIndex);
                }
                pRecRenderData->pCallbacks->EmptyBufferDone(pRecRenderData->hSelf, pRecRenderData->pAppData, &obh);
            }
        }
        alogd("muxChn[%d] [%d]Ready text frame should all released before!", pRecRenderData->mMppChnInfo.mChnId, nCnt);
    }

    int iIdleTextFrameCnt = 0;
    list_for_each_entry(pTEntry, &pRecRenderData->mTextInputFrameIdleList, mList)
    {
        iIdleTextFrameCnt++;
    }
    if (iIdleTextFrameCnt != pRecRenderData->mTextInputFrameNum)
    {
        aloge("fatal error! muxChn[%d] text input frames [%d]<[%d] must return all before",
            pRecRenderData->mMppChnInfo.mChnId, iIdleTextFrameCnt, pRecRenderData->mTextInputFrameNum);
    }
    else
    {
        //alogd("Release all text input frame [%d]", pRecRenderData->mTextInputFrameNum);
    }
    pthread_mutex_unlock(&pRecRenderData->mTextInputFrameListMutex);

    return SUCCESS;
}

static ERRORTYPE RecSinkToForceIFrame(RECRENDERDATATYPE *pRecRenderData, int nStreamId)
{
    ERRORTYPE   eError = SUCCESS;
    if(nStreamId >= MAX_REC_RENDER_PORTS)
    {
        aloge("fatal error! muxChn[%d] streamId:%d wrong", pRecRenderData->mMppChnInfo.mChnId, nStreamId);
    }
    if(pRecRenderData->sInPortDef[nStreamId].nPortIndex != nStreamId)
    {
        aloge("fatal error! muxChn[%d] check portIndex/streamId and suffix:%d!=%d",
            pRecRenderData->mMppChnInfo.mChnId,
            pRecRenderData->sInPortDef[nStreamId].nPortIndex, nStreamId);
    }
    if(pRecRenderData->mInputPortTunnelFlag[nStreamId])
    {
        if (COMP_PortDomainVideo != pRecRenderData->sInPortDef[nStreamId].eDomain)
        {
            aloge("fatal error! muxChn[%d] port[%d] domain[0x%x] is not video!",
                pRecRenderData->mMppChnInfo.mChnId,
                nStreamId, pRecRenderData->sInPortDef[nStreamId].eDomain);
        }
        BOOL bRequest = TRUE;
        eError = COMP_SetConfig(pRecRenderData->sInPortTunnelInfo[nStreamId].hTunnel, COMP_IndexVendorVencRequestIDR, (void*)&bRequest);
        if(eError != SUCCESS)
        {
            aloge("fatal error! muxChn[%d] streamId[%d] forceIFrame fail[0x%x]!",
                pRecRenderData->mMppChnInfo.mChnId,
                nStreamId, eError);
        }
    }
    else
    {
        eError = pRecRenderData->pCallbacks->EventHandler(pRecRenderData->hSelf, pRecRenderData->pAppData,
            COMP_EventMuxForceIFrame, nStreamId, 0, NULL);
    }
    return eError;
}

/**
  check if need switch file.

  @param[out] need_switch_audio
    if audio reach file duration first, set need_switch_audio to true, else set to false.

  @return
    true: video reach file duration first.
    false: video not reach file duration.
*/
static BOOL RecSinkIfNeedSwitchFile(RECRENDERDATATYPE *pRecRenderData, int *need_switch_audio)
{
    BOOL bNeedSwitch = FALSE;
    pthread_mutex_lock(&pRecRenderData->mJudgeSwitchlock);
    if(TRUE == pRecRenderData->bNeedSw)
    {
        pthread_mutex_unlock(&pRecRenderData->mJudgeSwitchlock);
        return pRecRenderData->bNeedSw;
    }
    else if(TRUE == pRecRenderData->bNeedSwAudio)
    {
        *need_switch_audio = 1;
        pthread_mutex_unlock(&pRecRenderData->mJudgeSwitchlock);
        return FALSE;
    }

    if(pRecRenderData->mbMuxerInit /* && pRecRenderData->reset_fd_flag == TRUE */ && !pRecRenderData->nCallbackOutFlag) // no network
    {
        if(pRecRenderData->rec_file == FILE_NEED_SWITCH_TO_NORMAL)
        {
            bNeedSwitch = TRUE;
            pRecRenderData->bNeedSw = TRUE;
        }
        else if(pRecRenderData->mCurMaxFileDuration > 0)   //user set max duration, then segment file base on duration
        {
            //if(pRecSink->mLoopDuration >= pRecSink->mCurFileEndTm)
            //{
            //    bNeedSwitch = TRUE;
            //}
            //int nErrorInterval = 1000*1000/pRecSink->mpMediaInf->uVideoFrmRate/3;   //unit:ms
            //int nErrorInterval = 1000*1000/pRecSink->mpMediaInf->mMediaVideoInfo[pRecSink->mRefVideoStreamIndex].uVideoFrmRate/3;
            if((pRecRenderData->mFileDurationPolicy == RecordFileDurationPolicy_MinDuration) 
                || (pRecRenderData->mFileDurationPolicy == RecordFileDurationPolicy_AccurateDuration))
            {
                if(pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex] /*+ nErrorInterval*/ >= pRecRenderData->mCurMaxFileDuration) // to use current actual duration,since current pkt will not be sent out
                {
                    bNeedSwitch = TRUE;
                    pRecRenderData->bNeedSw = TRUE;
					alogd("muxChn[%d] need_file_switch_v:%d-%d-%lld-%d-%d-%d-%d-%d",
                        pRecRenderData->mMppChnInfo.mChnId,
                        pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex],
                        pRecRenderData->mDurationAudio,
                        pRecRenderData->mCurMaxFileDuration,
                        pRecRenderData->mFileDurationPolicy,
                        pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt,
                        pRecRenderData->mVideoFrameCounter, pRecRenderData->mAudioFrmCnt);
                }
            }
            else if(pRecRenderData->mFileDurationPolicy == RecordFileDurationPolicy_AverageDuration)
            {
                if(pRecRenderData->mLoopDuration /*+ nErrorInterval*/ >= pRecRenderData->mCurFileEndTm)
                {
                    bNeedSwitch = TRUE;
                    pRecRenderData->bNeedSw = TRUE;
					alogd("muxChn[%d] need_file_switch_v3:%d-%d-%lld-%d-%lld-%lld-%lld-%d-%d-%d-%d",
                        pRecRenderData->mMppChnInfo.mChnId,
                        pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex], pRecRenderData->mDurationAudio,
                        pRecRenderData->mCurMaxFileDuration,
                        pRecRenderData->mFileDurationPolicy,
                        pRecRenderData->mLoopDuration, pRecRenderData->mCurFileEndTm,
                        pRecRenderData->mLoopDurationAudio,
                        pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt,
                        pRecRenderData->mVideoFrameCounter, pRecRenderData->mAudioFrmCnt);
                }
            }
            else
            {
                aloge("fatal error! unknown FileDurationPolicy[0x%x], check code!", pRecRenderData->mFileDurationPolicy);
            }

            if(FALSE == bNeedSwitch && (pRecRenderData->mRecordMode & RECORDER_MODE_AUDIO) && (pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO))
            {
                // to check audio status, must has video. when only has audio, pRecRenderData->mDuration[0] and pRecRenderData->mLoopDuration
                // are used to record audio duration and loopDurationAudio.
                //int nErrorInterval = MAXDECODESAMPLE*1000 / pRecSink->mpMediaInf->sample_rate/3;   //unit:ms
                *need_switch_audio = 0;
                if(pRecRenderData->mFileDurationPolicy == RecordFileDurationPolicy_MinDuration ||
                    pRecRenderData->mFileDurationPolicy == RecordFileDurationPolicy_AccurateDuration)
                {
                    if(pRecRenderData->mDurationAudio /*+ nErrorInterval*/ >= pRecRenderData->mCurMaxFileDuration)
                    {
                        *need_switch_audio = 1;
                        pRecRenderData->bNeedSwAudio = TRUE;
						aloge("muxChn[%d] need_file_switch_a:%d-%d-%lld-%d-%lld-%lld-%d-%d-%d-%d",
                            pRecRenderData->mMppChnInfo.mChnId,
                            pRecRenderData->mDurationAudio, pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex],
                            pRecRenderData->mCurMaxFileDuration, pRecRenderData->mFileDurationPolicy,
                            pRecRenderData->mLoopDurationAudio, pRecRenderData->mLoopDuration,
                            pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt,
                            pRecRenderData->mVideoFrameCounter, pRecRenderData->mAudioFrmCnt);
                    }
                }
                else if(pRecRenderData->mFileDurationPolicy == RecordFileDurationPolicy_AverageDuration)
                {
                    if(pRecRenderData->mLoopDurationAudio /*+ nErrorInterval*/ >= pRecRenderData->mCurFileEndTm)
                    {
                        *need_switch_audio = 1;
                        pRecRenderData->bNeedSwAudio = TRUE;
						aloge("muxChn[%d] need_file_switch_a3:%d-%d-%lld-%d-%lld-%lld-%lld-%d-%d-%d-%d",
                            pRecRenderData->mMppChnInfo.mChnId,
                            pRecRenderData->mDurationAudio, pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex],
                            pRecRenderData->mCurMaxFileDuration, pRecRenderData->mFileDurationPolicy,
                            pRecRenderData->mLoopDurationAudio, pRecRenderData->mLoopDuration, pRecRenderData->mCurFileEndTm,
                            pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt,
                            pRecRenderData->mVideoFrameCounter, pRecRenderData->mAudioFrmCnt);
                    }
                }
                else
                {
                    aloge("fatal error! unknown FileDurationPolicy[0x%x], check code!", pRecRenderData->mFileDurationPolicy);
                }
            }
        }
        else if(pRecRenderData->mChnAttr.mMaxFileSizeBytes > 0)   //user set max file size, then segment file base on fileSize.
        {
            if(pRecRenderData->mFileSizeBytes >= pRecRenderData->mChnAttr.mMaxFileSizeBytes)
            {
                double fileSizeMB = (double)pRecRenderData->mFileSizeBytes/(1024*1024);
                alogv("muxChn[%d] fileSize[%lld]Bytes([%7.3lf]MB) >= max[%7.3lf]MB, rec_file[%d], need switch file",
                    pRecRenderData->mMppChnInfo.mChnId,
                    pRecRenderData->mFileSizeBytes, fileSizeMB, (double)pRecRenderData->mChnAttr.mMaxFileSizeBytes/(1024*1024),
                    pRecRenderData->rec_file);
                bNeedSwitch = TRUE;
                pRecRenderData->bNeedSw = TRUE;
            }
        }
        else    //not switch file
        {
            bNeedSwitch = FALSE;
            pRecRenderData->bNeedSw = FALSE;
        }
    }
    else
    {
        bNeedSwitch = FALSE;
    }
    pthread_mutex_unlock(&pRecRenderData->mJudgeSwitchlock);
    return bNeedSwitch;
}

static BOOL RecSink_CheckStreamIdValid(RECRENDERDATATYPE *pRecRenderData, int nStreamId)
{
    if(pRecRenderData->mValidStreamCount < 0)
    {
        return TRUE;
    }
    for(int i=0; i<pRecRenderData->mValidStreamCount; i++)
    {
        if(pRecRenderData->mValidStreamIds[i] == nStreamId)
        {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL RecSinkGrantSwitchFile(RECRENDERDATATYPE *pRecRenderData, RecSinkPacket *pRSPacket)
{
    BOOL bGrant = TRUE;

    if(RecordFileDurationPolicy_AccurateDuration == pRecRenderData->mFileDurationPolicy)
    {
        bGrant = TRUE;
    }
    else if(RecordFileDurationPolicy_MinDuration == pRecRenderData->mFileDurationPolicy 
        || RecordFileDurationPolicy_AverageDuration == pRecRenderData->mFileDurationPolicy)
    {
        if(pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO)
        {
            if(pRecRenderData->media_inf.mVideoInfoValidNum > MAX_VIDEO_TRACK_COUNT)
            {
                aloge("fatal error! muxChn[%d] video stream num[%d] too many!",
                    pRecRenderData->mMppChnInfo.mChnId,
                    pRecRenderData->media_inf.mVideoInfoValidNum);
            }
            //when video meets key frame, we will set prefetch flag, so by prefetch flags, we can decide if all videos meet key frame.
            BOOL bKeyFrameReady = FALSE;
            int i;
            for(i=0; i<pRecRenderData->media_inf.mVideoInfoValidNum; i++)
            {
                if(FALSE == RecSink_CheckStreamIdValid(pRecRenderData, i))
                {
                    alogd("muxChn[%d] streamId[%d] is not valid, ignore", pRecRenderData->mMppChnInfo.mChnId, i);
                    continue;
                }
                if(!pRecRenderData->mPrefetchFlag[i])
                {
                    break;
                }
            }
            if(i == pRecRenderData->media_inf.mVideoInfoValidNum)
            {
                bKeyFrameReady = TRUE;
            }

            //use videoStream0 to compare with audioStream
            BOOL bAudioDone = FALSE;
            if(pRecRenderData->mPrefetchFlag[pRecRenderData->mRefVideoStreamIndex])
            {
                int64_t nVideoTotalTm = -1; //unit:ms
                int64_t nAudioTotalTm = -1; //unit:ms
                int VFrameDuration = 1000*1000
                    / pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].uVideoFrmRate;
                nVideoTotalTm = (pRecRenderData->mPrevPts[pRecRenderData->mRefVideoStreamIndex]
                    + pRecRenderData->mBasePts[pRecRenderData->mRefVideoStreamIndex])/1000 + VFrameDuration;
                if(pRecRenderData->mRecordMode & RECORDER_MODE_AUDIO)
                {
                    int AFrameDuration = MAXDECODESAMPLE*1000 / pRecRenderData->media_inf.sample_rate;
                    int audioStreamIndex = pRecRenderData->media_inf.mVideoInfoValidNum;
                    nAudioTotalTm = (pRecRenderData->mPrevPts[audioStreamIndex]+pRecRenderData->mBasePts[audioStreamIndex])/1000
                        + AFrameDuration;
                    if(nAudioTotalTm >= nVideoTotalTm)
                    {
                        alogd("muxChn[%d] refVideoStream[%d] can switch file. aSrcPts[%lld]ms, vSrcPts[%lld]ms, ATime[%lld]-VTime[%lld]=[%lld]ms, rec_file[%d]",
                            pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mRefVideoStreamIndex,
                            (pRecRenderData->mPrevPts[audioStreamIndex]+pRecRenderData->mBasePts[audioStreamIndex])/1000,
                            (pRecRenderData->mPrevPts[pRecRenderData->mRefVideoStreamIndex]
                                + pRecRenderData->mBasePts[pRecRenderData->mRefVideoStreamIndex])/1000,
                            nAudioTotalTm, nVideoTotalTm, nAudioTotalTm - nVideoTotalTm, pRecRenderData->rec_file);
                        bAudioDone = TRUE;
                        if(!bKeyFrameReady)
                        {
                            alogw("Be careful! muxChn[%d] refVideoStream[%d]. Not all video streams meet key frame! we still can't switch file, begin to prefetch audio!",
                                pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mRefVideoStreamIndex);
                            //we need cache audio now. wait other videos to meet key frame.
                            pRecRenderData->mPrefetchFlagAudio = TRUE;
                        }
                    }
                    else
                    {
                        if(CODEC_TYPE_AUDIO == pRSPacket->mStreamType)
                        {
                            alogv("muxChn[%d] wait switch file. audio packet. Time a[%lld]ms < v[%lld]ms, rec_file[%d]",
                                pRecRenderData->mMppChnInfo.mChnId, nAudioTotalTm, nVideoTotalTm, pRecRenderData->rec_file);
                        }
                        bAudioDone = FALSE;
                    }
                }
                else
                {
                    bAudioDone = TRUE;
                }
            }

            if(bKeyFrameReady && bAudioDone)
            {
                int i;
                for(i=0; i<pRecRenderData->media_inf.mVideoInfoValidNum; i++)
                {
                    alogd("muxChn[%d] can switch file. VStreamId[%d]Dur:[%d]ms", pRecRenderData->mMppChnInfo.mChnId, i, pRecRenderData->mDuration[i]);
                }
                alogd("muxChn[%d] can switch file. ADur:[%d]ms", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mDurationAudio);
                bGrant = TRUE;
            }
            else
            {
                bGrant = FALSE;
            }
        }
        else
        {
            bGrant = TRUE;
        }
    }
    return bGrant;
}

/**
  decide if can switch file when audio first meet switch file duration.
  need cache following audio frames to wait video pts to reach audioPts that is at switchFile point. But video need
  switch file at key frame, so when video pts reach that audio pts but witch not key frame,  video frames need to be
  written to file contiguously until key frame coming. So cached audio frames need to be written to file again to
  reach video pts, this is done in RecSinkSwitchFile().
*/
BOOL RecSinkGrantSwitchFileAudio(RECRENDERDATATYPE *pRecRenderData, RecSinkPacket *pRSPacket)
{
    BOOL bGrant = TRUE;

    if(RecordFileDurationPolicy_AccurateDuration == pRecRenderData->mFileDurationPolicy)
    {
        bGrant = TRUE;
    }
    else if(RecordFileDurationPolicy_MinDuration == pRecRenderData->mFileDurationPolicy
        || RecordFileDurationPolicy_AverageDuration == pRecRenderData->mFileDurationPolicy)
    {
        if(!(pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO) || !(pRecRenderData->mRecordMode & RECORDER_MODE_AUDIO))
        {
            aloge("fatal error! muxChn[%d] recordMode[0x%x] wrong!", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mRecordMode);
        }
        //(1)video0 duration >= audio duration
        if(!pRecRenderData->bTimeMeetAudio)
        {
            int64_t nVideoTotalTm = -1; //unit:ms
            int64_t nAudioTotalTm = -1; //unit:ms
            int audioStreamIndex = pRecRenderData->media_inf.mVideoInfoValidNum;
            int VFrameDuration = 1000*1000
                / pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].uVideoFrmRate;
            int AFrameDuration = MAXDECODESAMPLE*1000 / pRecRenderData->media_inf.sample_rate;
            nVideoTotalTm = (pRecRenderData->mPrevPts[pRecRenderData->mRefVideoStreamIndex]
                + pRecRenderData->mBasePts[pRecRenderData->mRefVideoStreamIndex])/1000 + VFrameDuration;
            nAudioTotalTm = (pRecRenderData->mPrevPts[audioStreamIndex]+pRecRenderData->mBasePts[audioStreamIndex])/1000
                + AFrameDuration;
            if(nAudioTotalTm <= nVideoTotalTm)
            {
                if (RecordFileDurationPolicy_MinDuration == pRecRenderData->mFileDurationPolicy)
                {
                    if (pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex] >= pRecRenderData->mCurMaxFileDuration)
                    {
                        pRecRenderData->bTimeMeetAudio = TRUE;
                    }
                }
                else
                {
                    pRecRenderData->bTimeMeetAudio = TRUE;
                }
                if (pRecRenderData->bTimeMeetAudio)
                {
                    alogd("muxChn[%d] can switch file audio. vDur[%d-%d]>=[%lld]ms. VTime[%lld]-ATime[%lld]=[%lld]ms, fileDurPolicy[%d], rec_file[%d]",
                        pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mRefVideoStreamIndex,
                        pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex], pRecRenderData->mCurMaxFileDuration, nVideoTotalTm,
                        nAudioTotalTm, nVideoTotalTm-nAudioTotalTm, pRecRenderData->mFileDurationPolicy, pRecRenderData->rec_file);
                }
            }
        }
        //(2)all videos meet key frame.
        BOOL bKeyFrameReady = FALSE;
        if(pRecRenderData->bTimeMeetAudio)
        {
            if((pRSPacket->mStreamType==CODEC_TYPE_VIDEO) && (pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME))
            {
                if(pRSPacket->mStreamId >= MAX_VIDEO_TRACK_COUNT)
                {
                    aloge("fatal error! muxChn[%d] wrong streamId[%d]", pRecRenderData->mMppChnInfo.mChnId, pRSPacket->mStreamId);
                }
                if(!pRecRenderData->mPrefetchFlag[pRSPacket->mStreamId])
                {
                    //if current frame is video key frame, we can set prefetchFlag for this video.
                    pRecRenderData->mPrefetchFlag[pRSPacket->mStreamId] = TRUE;
                }
            }
            if(pRecRenderData->media_inf.mVideoInfoValidNum > MAX_VIDEO_TRACK_COUNT)
            {
                aloge("fatal error! muxChn[%d] video stream num[%d] too many!", pRecRenderData->mMppChnInfo.mChnId,
                    pRecRenderData->media_inf.mVideoInfoValidNum);
            }
            int i;
            for(i=0; i<pRecRenderData->media_inf.mVideoInfoValidNum; i++)
            {
                if(FALSE == RecSink_CheckStreamIdValid(pRecRenderData, i))
                {
                    alogd("muxChn[%d] streamId[%d] is not valid, ignore", pRecRenderData->mMppChnInfo.mChnId, i);
                    continue;
                }
                if(!pRecRenderData->mPrefetchFlag[i])
                {
                    break;
                }
            }
            if(i == pRecRenderData->media_inf.mVideoInfoValidNum)
            {
                bKeyFrameReady = TRUE;
            }
        }
        if(pRecRenderData->bTimeMeetAudio && bKeyFrameReady)
        {
            int i;
            for(i=0; i<pRecRenderData->media_inf.mVideoInfoValidNum; i++)
            {
                alogd("muxChn[%d] can switch file. VStrm[%d]Dur:[%d]ms", pRecRenderData->mMppChnInfo.mChnId, i, pRecRenderData->mDuration[i]);
            }
            alogd("muxChn[%d] can switch file. ADur:[%d]ms", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mDurationAudio);
            bGrant = TRUE;
        }
        else
        {
            bGrant = FALSE;
        }
    }
    return bGrant;
}

/**
  switch file. Do following things:
    muxer close,
    clear some variables,
    move prefetch packets to ready list,
    prepare next file fd,
    set rec_file.
*/
static void RecSinkSwitchFile(RECRENDERDATATYPE *pRecRenderData, int clrFile)
{
    if(0 == pRecRenderData->nOutputFd)
    {
        aloge("Be careful, muxChn[%d] fd == 0", pRecRenderData->mMppChnInfo.mChnId);
    }
//    alogd("TOTAL duration: %d(ms)", pSinkInfo->mDuration);
//    alogd("TOTAL duration audio: %d(ms)", pSinkInfo->mDurationAudio);
//    alogd("TOTAL file size: %lld(bytes)", pSinkInfo->mFileSizeBytes);

    //if switchFileAudio, let video duration >= audio duration.
    if(TRUE==pRecRenderData->bTimeMeetAudio && 0<pRecRenderData->mVideoFrmCntWriteMore)   // more video frame was sent to file
    {
        if(!(pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO) || !(pRecRenderData->mRecordMode & RECORDER_MODE_AUDIO))
        {
            aloge("fatal error! muxChn[%d] recordMode[0x%x] wrong!", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->mRecordMode);
        }
        int cnt = 0;
        int nAudCnt = 0;
        RecSinkPacket   *pEntry, *pTmp;
        list_for_each_entry(pEntry, &pRecRenderData->mPrefetchRSPacketList, mList)
        {
            if(CODEC_TYPE_AUDIO == pEntry->mStreamType)
            {
                nAudCnt++;
            }
            cnt++;
        }
        alogd("muxChn[%d] avsync_a_list:[%d]-%d-%d-%d-%lld-%lld-%d", pRecRenderData->mMppChnInfo.mChnId,
            pRecRenderData->mRefVideoStreamIndex, cnt, nAudCnt, pRecRenderData->mVideoFrmCntWriteMore,
            pRecRenderData->mVideoPtsWriteMoreSt, pRecRenderData->mVideoPtsWriteMoreEnd,
            pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].nWidth);

        int VFrameDuration = 1000*1000 / pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].uVideoFrmRate;
        int AFrameDuration = MAXDECODESAMPLE*1000 / pRecRenderData->media_inf.sample_rate;
        int nAFrmMore = 0; //the number of audio frames which will be written more to meet video pts.
        BOOL bAllUsedFlag = TRUE; // the audio frames in prefetch list are all used to be written to file
        list_for_each_entry_safe(pEntry, pTmp, &pRecRenderData->mPrefetchRSPacketList, mList)
        {
            //check video time and audio time.
            if(CODEC_TYPE_AUDIO == pEntry->mStreamType)
            {
                int nAudioTotalTm = pEntry->mPts/1000+AFrameDuration;
                int nVideoTotalTm = (pRecRenderData->mPrevPts[pRecRenderData->mRefVideoStreamIndex] + pRecRenderData->mBasePts[pRecRenderData->mRefVideoStreamIndex])/1000+VFrameDuration;
                if(nAudioTotalTm > nVideoTotalTm)
                {
                    //this aFrame don't be written to current file. because in switchFileAudio case, audioTime <= videoTime.
                    alogd("muxChn[%d] avsync_rc_swfa: [%d]-%d-%d-%d", pRecRenderData->mMppChnInfo.mChnId,
                        pRecRenderData->mRefVideoStreamIndex, nVideoTotalTm, nAudioTotalTm, nAFrmMore);
                    bAllUsedFlag = FALSE;
                    break;
                }
                else
                {
                    list_del(&pEntry->mList);
                    RecSinkWriteRSPacket(pRecRenderData, pEntry);
                    RecSinkReleaseRSPacket(pRecRenderData, pEntry);
                    nAFrmMore++;
                }
            }
        }
        if(bAllUsedFlag)
        {
            alogd("muxChn[%d] avsync_rc_swfa write all afrms: %d-%d-%d", pRecRenderData->mMppChnInfo.mChnId,
                pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex], pRecRenderData->mDurationAudio, nAFrmMore);
        }
    }
    if(pRecRenderData->mPrefetchFlagAudio)
    {
        if(pRecRenderData->bNeedSw)
        {
            alogd("muxChn[%d] switchFile cache video, but cache audio too!", pRecRenderData->mMppChnInfo.mChnId);
        }
    }

    int64_t tm1, tm2;
    tm1 = CDX_GetSysTimeUsMonotonic();
    RecSinkMuxerClose(pRecRenderData, clrFile);
    tm2 = CDX_GetSysTimeUsMonotonic();
    alogd("muxChn[%d] recRender_MuxerClose itl[%lld]ms", pRecRenderData->mMppChnInfo.mChnId, (tm2-tm1)/1000);

    //post process: clear some variables, move prefetch packets to ready list, prepare next file fd, set rec_file.
    int i;
    for (i=0;i<MAX_VIDEO_TRACK_COUNT;i++)
    {
        pRecRenderData->mDuration[i] = 0;
        pRecRenderData->mPrevDuration[i] = 0;
        pRecRenderData->mPrefetchFlag[i] = FALSE;
        //pSinkInfo->need_to_force_i_frm[i] = 0;
        pRecRenderData->forced_i_frm[i] = 0;
        pRecRenderData->key_frm_sent[i] = 0;
        pRecRenderData->v_frm_drp_cnt[i] = 0;
    }
    pRecRenderData->mPrefetchFlagAudio = FALSE;
    pRecRenderData->mDurationAudio = 0;
    pRecRenderData->mDurationText = 0;
    pRecRenderData->mPrevDurationAudio = 0;
    pRecRenderData->mPrevDurationText = 0;
    pRecRenderData->mFileSizeBytes = 0;
    pRecRenderData->bTimeMeetAudio = FALSE;
    pRecRenderData->mVideoFrmCntWriteMore = 0;
    pRecRenderData->mVideoPtsWriteMoreSt = -1;
    pRecRenderData->mVideoPtsWriteMoreEnd = -1;
    for(i=0;i<MAX_TRACK_COUNT;i++)
    {
        pRecRenderData->mbTrackInit[i] = FALSE;
        pRecRenderData->mPrevPts[i] = -1;
        pRecRenderData->mBasePts[i] = -1;
        pRecRenderData->mInputPrevPts[i] = -1;
    }
    pRecRenderData->mBasePtsRefStreamIndex = -1;
    pRecRenderData->mVideoFrameCounter = 0;
    pRecRenderData->mAudioFrmCnt = 0;

    RecSinkMovePrefetchRSPackets(pRecRenderData);
    
    //change fd.
    if(pRecRenderData->nSwitchFd >= 0)
    {
        pRecRenderData->nOutputFd = pRecRenderData->nSwitchFd;
        pRecRenderData->nSwitchFd = -1;
        pRecRenderData->nFallocateLen = pRecRenderData->nSwitchFdFallocateSize;
        pRecRenderData->nSwitchFdFallocateSize = 0;
        pRecRenderData->reset_fd_flag = FALSE;
    }
    else
    {
        alogd("Be careful. muxChn[%d] nSwitchFd<0, reset_fd_flag[%d] should be 0",
            pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->reset_fd_flag);
    }

    //set rec_file
    if(pRecRenderData->rec_file == FILE_NEED_SWITCH_TO_NORMAL)
    {
        int64_t oldTm = pRecRenderData->mCurFileEndTm;
        pRecRenderData->mCurFileEndTm = pRecRenderData->mLoopDuration;
        alogd("muxChn[%d] switch file normal done. change CurFileEndTm[%lld->%lld]ms",
            pRecRenderData->mMppChnInfo.mChnId, oldTm, pRecRenderData->mCurFileEndTm);
        pRecRenderData->rec_file = FILE_NORMAL;
    }
    else
    {
        if(pRecRenderData->rec_file != FILE_NORMAL)
        {
            aloge("fatal error! muxChn[%d] wrong rec_file[%d]", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->rec_file);
        }
    }
}

static BOOL RecSinkIfNeedRequestNextFd(RECRENDERDATATYPE *pRecRenderData)
{
    if(pRecRenderData->need_set_next_fd == FALSE)
    {
        return FALSE;
    }
    BOOL bNeedRequest = FALSE;
    if(pRecRenderData->mCurMaxFileDuration > 0)   //user set max duration, then segment file base on duration
    {
        //if(pRecSink->mLoopDuration + NOTIFY_NEEDNEXTFD_IN_ADVANCE >= pRecSink->mCurFileEndTm)
        //{
        //    bNeedRequest = TRUE;
        //}
        if(pRecRenderData->mFileDurationPolicy == RecordFileDurationPolicy_MinDuration
            || pRecRenderData->mFileDurationPolicy == RecordFileDurationPolicy_AccurateDuration)
        {
            if((pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex] + NOTIFY_NEEDNEXTFD_IN_ADVANCE) >= pRecRenderData->mCurMaxFileDuration)
            {
                bNeedRequest = TRUE;
            }
        }
        else if(pRecRenderData->mFileDurationPolicy == RecordFileDurationPolicy_AverageDuration)
        {
            if((pRecRenderData->mLoopDuration + NOTIFY_NEEDNEXTFD_IN_ADVANCE) >= pRecRenderData->mCurFileEndTm)
            {
                bNeedRequest = TRUE;
            }
            if((pRecRenderData->mLoopDurationAudio + NOTIFY_NEEDNEXTFD_IN_ADVANCE) >= pRecRenderData->mCurFileEndTm)
            {
                bNeedRequest = TRUE;
            }
        }
        else
        {
            aloge("fatal error! unknown FileDurationPolicy[0x%x], check code!", pRecRenderData->mFileDurationPolicy);
        }
    }
    else if(pRecRenderData->mChnAttr.mMaxFileSizeBytes > 0)   //user set max file size, then segment file base on fileSize.
    {
        if(pRecRenderData->mFileSizeBytes + pRecRenderData->mChnAttr.mMaxFileSizeBytes/10 >= pRecRenderData->mChnAttr.mMaxFileSizeBytes)
        {
            double fileSizeMB = (double)pRecRenderData->mFileSizeBytes/(1024*1024);
            alogd("fileSize[%lld]Bytes([%7.3lf]MB) < max[%7.3lf]MB, rec_file[%d], need request next fd",
                pRecRenderData->mFileSizeBytes, fileSizeMB, (double)pRecRenderData->mChnAttr.mMaxFileSizeBytes/(1024*1024), pRecRenderData->rec_file);
            bNeedRequest = TRUE;
        }
    }
    else
    {
        bNeedRequest = FALSE;
    }
    return bNeedRequest;
}

/**
  get one input buffer from mVideoInputFrameReadyList, mAudioInputFrameReadyList, or mTextInputFrameReadyList, to
  mValidRSPacketList.

  @return
    SUCCESS:
    ERR_MUX_NOMEM: no data.
*/
static ERRORTYPE RecRender_GetOneBuffer(RECRENDERDATATYPE *pRecRenderData)
{
    ERRORTYPE ret = SUCCESS;
    ERRORTYPE eRet;
    RecSinkPacket RSPacket;
    memset(&RSPacket, 0, sizeof(RSPacket));
    enum CodecType eSelectType = SelectStreamToGet(pRecRenderData);
    if(CODEC_TYPE_VIDEO == eSelectType)
    {
        eRet = RecRender_GetVideoBuffer(pRecRenderData, &RSPacket);
        if(SUCCESS == eRet)
        {
            RecSinkPutRSPacket(pRecRenderData, &RSPacket);
            pRecRenderData->duration = RSPacket.mPts - pRecRenderData->mnBasePts;
        }
        else
        {
            aloge("fatal error! why get video buffer fail[0x%x]?", eRet);
            ret = ERR_MUX_NOMEM;
        }
    }
    else if(CODEC_TYPE_AUDIO == eSelectType)
    {
        eRet = RecRender_GetAudioBuffer(pRecRenderData, &RSPacket);
        if(SUCCESS == eRet)
        {
            RecSinkPutRSPacket(pRecRenderData, &RSPacket);
            pRecRenderData->duration_audio = RSPacket.mPts - pRecRenderData->mnAudioBasePts;
        }
        else
        {
            aloge("fatal error! why get audio buffer fail[0x%x]?", eRet);
            ret = ERR_MUX_NOMEM;
        }
    }
    else if(CODEC_TYPE_TEXT == eSelectType)
    {
        eRet = RecRender_GetTextBuffer(pRecRenderData, &RSPacket);
        if(SUCCESS == eRet)
        {
            RecSinkPutRSPacket(pRecRenderData, &RSPacket);
            pRecRenderData->duration_text = RSPacket.mPts - pRecRenderData->mnTextBasePts;
        }
        else
        {
            aloge("fatal error! why get text buffer fail[0x%x]?", eRet);
            ret = ERR_MUX_NOMEM;
        }
    }
    else
    {
        ret = ERR_MUX_NOMEM;
    }
    return ret;
}

/**
  get all input buffers from mVideoInputFrameReadyList, mAudioInputFrameReadyList, or mTextInputFrameReadyList, to
  mValidRSPacketList.

  @return
    number of input buffers.
*/
static int RecRender_GetAllBuffers(RECRENDERDATATYPE *pRecRenderData)
{
    int nNum = 0;
    ERRORTYPE eRet;
    while(1)
    {
        eRet = RecRender_GetOneBuffer(pRecRenderData);
        if(SUCCESS == eRet)
        {
            nNum++;
        }
        else
        {
            break;
        }
    }
    return nNum;
}

static void* RecRender_ComponentThread(void* pThreadData)
{
    //int ret = SUCCESS;
    //int track_index = -1;
    //int track_process_num = 0;
    int buffer_fail_flags = 0;
    unsigned int cmddata;
    CompInternalMsgType cmd;
    RECRENDERDATATYPE *pRecRenderData = (RECRENDERDATATYPE*) pThreadData;
    message_t cmd_msg;
    //COMP_BUFFERHEADERTYPE omx_buffer_header;
    //COMP_BUFFERHEADERTYPE * pAudioHeader;
    //VencOutputBuffer data_ctrl;
    //MM_COMPONENTTYPE *pTunnelComp;
    //COMP_INTERNAL_TUNNELINFOTYPE *pTunnelInfo;
    //COMP_BUFFERHEADERTYPE *p_omx_buffer;
    //AVPacket packet;
    //int64_t origin_pts/*, prev_pts[MAX_TRACK_COUNT],base_pts[MAX_TRACK_COUNT]*/;    //prev_pts[],base_pts[] are deprecated.
    RecSinkPacket   RSPacket;
    RecSinkPacket   *pRSPacket = NULL;
    ERRORTYPE eRet;
    alogv("Recorder Render ComponentThread start run...");
    sprintf(pRecRenderData->mThreadName, "muxChn%d", pRecRenderData->mMppChnInfo.mChnId);
    prctl(PR_SET_NAME, (unsigned long)pRecRenderData->mThreadName, 0, 0, 0);

    while (1)
    {
PROCESS_MESSAGE:
        if(get_message(&pRecRenderData->cmd_queue, &cmd_msg) == 0)
        {
            cmd = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;

            alogv("RecRender ComponentThread get_message cmd:%d", cmd);

            // State transition command
            if (cmd == SetState)
            {
                pthread_mutex_lock(&pRecRenderData->mStateMutex);
                // If the parameter states a transition to the same state
                // raise a same state transition error.
                if (pRecRenderData->state == (COMP_STATETYPE) (cmddata))
                    pRecRenderData->pCallbacks->EventHandler(
                            pRecRenderData->hSelf,
                            pRecRenderData->pAppData,
                            COMP_EventError,
                            ERR_MUX_SAMESTATE,
                            0,
                            NULL);
                else
                {
                    // transitions/callbacks made based on state transition table
                    // cmddata contains the target state
                    switch ((COMP_STATETYPE) (cmddata))
                    {
                        case COMP_StateInvalid:
                        {
                            pRecRenderData->state = COMP_StateInvalid;
                            pRecRenderData->pCallbacks->EventHandler(
                                    pRecRenderData->hSelf,
                                    pRecRenderData->pAppData,
                                    COMP_EventError,
                                    ERR_MUX_INVALIDSTATE,
                                    0,
                                    NULL);
                            pRecRenderData->pCallbacks->EventHandler(
                                    pRecRenderData->hSelf,
                                    pRecRenderData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pRecRenderData->state,
                                    NULL);
                            break;
                        }
                        case COMP_StateLoaded:
                        {
                            if (pRecRenderData->state != COMP_StateIdle)
                            {
                                aloge("fatal error! curState[0x%x] is wrong", pRecRenderData->state);
                                pRecRenderData->pCallbacks->EventHandler(
                                        pRecRenderData->hSelf,
                                        pRecRenderData->pAppData,
                                        COMP_EventError,
                                        ERR_MUX_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            alogv("RecRender set state LOADED");
                            //release all frames.
                            //alogd("release all frames to VEnc, AEnc and TEnc, when state[0x%x]->loaded", pRecRenderData->state);
                            RecRenderReleaseAllFrames(pRecRenderData);

                            pRecRenderData->state = COMP_StateLoaded;
                            pRecRenderData->pCallbacks->EventHandler(
                                    pRecRenderData->hSelf,
                                    pRecRenderData->pAppData,
                                    COMP_EventCmdComplete,
                                    COMP_CommandStateSet,
                                    pRecRenderData->state,
                                    NULL);
                            alogv("RecRender set state LOADED ok");
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            if (COMP_StateExecuting==pRecRenderData->state || COMP_StatePause==pRecRenderData->state)
                            {
                                alogv("recRender state[%d] to idle", pRecRenderData->state);
                                RecSinkMovePrefetchRSPackets(pRecRenderData);
                                int nNum = RecRender_GetAllBuffers(pRecRenderData);
                                RecSinkDrainAllRSPackets(pRecRenderData);
                                if(pRecRenderData->mbMuxerInit)
                                {
                                    int64_t tm1, tm2;
                                    tm1 = CDX_GetSysTimeUsMonotonic();
                                    RecSinkMuxerClose(pRecRenderData, 0);
                                    tm2 = CDX_GetSysTimeUsMonotonic();
                                    alogd("muxChn[%d] recRender_MuxerClose itl[%lld]ms", pRecRenderData->mMppChnInfo.mChnId, (tm2-tm1)/1000);
                                }
                                for(int i=0; i<MAX_VIDEO_TRACK_COUNT; i++)
                                {
                                    pRecRenderData->mPrefetchFlag[i] = FALSE;
                                }
                                pRecRenderData->mPrefetchFlagAudio = FALSE;
                                if(pRecRenderData->nOutputFd >= 0)
                                {
                                    aloge("fatal error! muxChn[%d] maybe not muxerInit? nOutputFd[%d]>=0", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nOutputFd);
                                    close(pRecRenderData->nOutputFd);
                                    pRecRenderData->nOutputFd = -1;
                                }
                                pthread_mutex_lock(&pRecRenderData->mutex_reset_writer_lock);
                                if(pRecRenderData->nSwitchFd >= 0)
                                {
                                    close(pRecRenderData->nSwitchFd);
                                    pRecRenderData->nSwitchFd = -1;
                                    pRecRenderData->reset_fd_flag = FALSE;
                                }
                                pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);

                                //release all frames.
                                alogd("muxChn[%d] release all frames to VEnc, AEnc and TEnc, when state[0x%x]->idle", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->state);
                                RecRenderReleaseAllFrames(pRecRenderData);

                                pRecRenderData->mnBasePts = -1;
                                pRecRenderData->mnAudioBasePts = -1;
                                pRecRenderData->mnTextBasePts = -1;
                                pRecRenderData->duration = 0;
                                pRecRenderData->duration_audio = 0;
                                pRecRenderData->duration_text = 0;
                                pRecRenderData->mLoopDuration = 0;
                                pRecRenderData->mLoopDurationAudio = 0;
                                for(int j=0;j<MAX_TRACK_COUNT;j++)
                                {
                                    pRecRenderData->mOrigBasePts[j] = -1;
                                    pRecRenderData->mDebugInputPts[j] = -1;
                                }
                                pRecRenderData->mCurFileEndTm = 0;
                                
                                pRecRenderData->state = COMP_StateIdle;
                                pRecRenderData->pCallbacks->EventHandler(
                                        pRecRenderData->hSelf,
                                        pRecRenderData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pRecRenderData->state,
                                        NULL);
                            }
                            else if(COMP_StateLoaded == pRecRenderData->state)
                            {
                                pRecRenderData->state = COMP_StateIdle;
                                pRecRenderData->pCallbacks->EventHandler(
                                        pRecRenderData->hSelf,
                                        pRecRenderData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pRecRenderData->state,
                                        NULL);
                            }
                            else if (COMP_StateInvalid == pRecRenderData->state)
                            {
                                aloge("fatal error! InvalidState[%d]->[%d]", pRecRenderData->state, COMP_StateIdle);
                                pRecRenderData->pCallbacks->EventHandler(
                                        pRecRenderData->hSelf,
                                        pRecRenderData->pAppData,
                                        COMP_EventError,
                                        ERR_MUX_INCORRECT_STATE_OPERATION,
                                        0,
                                        NULL);
                            }
                            else
                            {
                                aloge("fatal error! recRender state[%d] to idle", pRecRenderData->state);
                                pRecRenderData->state = COMP_StateIdle;
                                pRecRenderData->pCallbacks->EventHandler(
                                        pRecRenderData->hSelf,
                                        pRecRenderData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pRecRenderData->state,
                                        NULL);
                            }
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            // Transition can only happen from pause or idle state
                            if (pRecRenderData->state == COMP_StateIdle || pRecRenderData->state == COMP_StatePause)
                            {
                                // Return buffers if currently in pause
                                if(pRecRenderData->state == COMP_StateIdle)
                                {
                                    for (int index = 0; index < MAX_REC_RENDER_PORTS; index++)
                                    {
                                        if(pRecRenderData->mInputPortTunnelFlag[index] && (pRecRenderData->sInPortTunnelInfo[index].hTunnel == NULL))
                                        {
                                            aloge("fatal error! muxChn[%d] port[%d]domain[0x%x] is tunnel mode, but comp is null",
                                                pRecRenderData->mMppChnInfo.mChnId, index, pRecRenderData->sInPortDef[index].eDomain);
                                            pRecRenderData->state = COMP_StateInvalid;
                                            pRecRenderData->pCallbacks->EventHandler(
                                                    pRecRenderData->hSelf,
                                                    pRecRenderData->pAppData,
                                                    COMP_EventError,
                                                    ERR_MUX_INVALIDSTATE,
                                                    0,
                                                    NULL);
                                            //goto EXIT;
                                        }
                                    }
                                }
                                else if (pRecRenderData->state == COMP_StatePause)
                                {
                                }
                                pRecRenderData->state = COMP_StateExecuting;
                                pRecRenderData->pCallbacks->EventHandler(
                                        pRecRenderData->hSelf,
                                        pRecRenderData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pRecRenderData->state,
                                        NULL);
                            }
                            else
                            {
                                aloge("fatal error! muxChn[%d] InvalidState[%d]->Executing", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->state);
                                pRecRenderData->pCallbacks->EventHandler(
                                        pRecRenderData->hSelf,
                                        pRecRenderData->pAppData,
                                        COMP_EventError,
                                        ERR_MUX_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        case COMP_StatePause:
                        {
                            // Transition can only happen from executing state
                            if (pRecRenderData->state == COMP_StateExecuting)
                            {
                                pRecRenderData->state = COMP_StatePause;
                                pRecRenderData->pCallbacks->EventHandler(
                                        pRecRenderData->hSelf,
                                        pRecRenderData->pAppData,
                                        COMP_EventCmdComplete,
                                        COMP_CommandStateSet,
                                        pRecRenderData->state,
                                        NULL);
                            }
                            else
                            {
                                aloge("fatal error! muxChn[%d] InvalidState[%d]->pause", pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->state);
                                pRecRenderData->pCallbacks->EventHandler(
                                        pRecRenderData->hSelf,
                                        pRecRenderData->pAppData,
                                        COMP_EventError,
                                        ERR_MUX_INCORRECT_STATE_TRANSITION,
                                        0,
                                        NULL);
                            }
                            break;
                        }
                        default:
                        {
                            aloge("fatal error! InvalidState[%d]->[%d]", pRecRenderData->state, (COMP_STATETYPE) (cmddata));
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&pRecRenderData->mStateMutex);
            }
            else if (cmd == StopPort)
            {
            }
            else if (cmd == Stop)
            {
                // Kill thread
                goto EXIT;
            }
            else if (SwitchFileNormal == cmd)
            {
                SwitchFileNormalInfo *pInfo = (SwitchFileNormalInfo*)cmd_msg.mpData;
                ERRORTYPE eError = SUCCESS;
                alogd("muxChn[%d] Switch file normal", pRecRenderData->mMppChnInfo.mChnId);
                if(pRecRenderData->rec_file != FILE_NORMAL)
                {
                    aloge("fatal error! Switch file normal, but rec_file_state=%d", pRecRenderData->rec_file);
                    goto PROCESS_MESSAGE;
                }
                //------------------set switchFd.---------------
                pthread_mutex_lock(&pRecRenderData->mutex_reset_writer_lock);
                int nFd = pInfo->mFd;
                int nFallocateLen = pInfo->mnFallocateLen;
                if(nFd < 0)
                {
                    aloge("Be careful! fd[%d] < 0, app maybe want to bypass frames in this muxChn[%d] next file", 
                        nFd, pRecRenderData->mMppChnInfo.mChnId);
                    pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
                    //return ERR_MUX_ILLEGAL_PARAM;
                }
                else
                {
                    if(pRecRenderData->nSwitchFd >= 0)
                    {
                        alogd("muxChn[%d] nSwitchFd[%d] already exist, directly close it! maybe switch file happen during new fd is setting.",
                            pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nSwitchFd);
                        close(pRecRenderData->nSwitchFd);
                        pRecRenderData->nSwitchFd = -1;
                        pRecRenderData->nSwitchFdFallocateSize = 0;
                    }
                //    if(pThiz->mSwitchFilePath)
                //    {
                //        alogd("switchFilePath[%s] already exist, maybe impact happen during new fd is setting.", pThiz->mSwitchFilePath);
                //        free(pThiz->mSwitchFilePath);
                //        pThiz->mSwitchFilePath = NULL;
                //        pThiz->nSwitchFdFallocateSize = 0;
                //    }
                    pRecRenderData->nSwitchFd = dup(nFd);
                    if(pRecRenderData->nSwitchFd < 0)
                    {
                        aloge("fatal error! muxChn[%d] dup fail:[%d]->[%d],(%s)", pRecRenderData->mMppChnInfo.mChnId, nFd, pRecRenderData->nSwitchFd, strerror(errno));
                        system("lsof");
                    }
                    //pThiz->nSwitchFd = dup2SeldomUsedFd(nFd);
                    pRecRenderData->nSwitchFdFallocateSize = nFallocateLen;
                    //pThiz->mSwitchFdImpactFlag = nIsImpact;
                    alogd("muxChn[%d] dup setfd[%d] to nSwitchFd[%d]", pRecRenderData->mMppChnInfo.mChnId, nFd, pRecRenderData->nSwitchFd);
                    if(TRUE == pRecRenderData->reset_fd_flag)
                    {
                        alogd("muxChn[%d] reset__fd_flag is already true, maybe switch file happen during new fd is setting", pRecRenderData->mMppChnInfo.mChnId);
                    }
                    pRecRenderData->reset_fd_flag = TRUE;
                    pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
                }
                //----------------------------------------------------------------------------
                ERRORTYPE switchRet = ERR_MUX_NOT_SUPPORT;
                if(FALSE == pRecRenderData->mbMuxerInit)
                {
                    alogd("muxerId[%d] switch file normal begin, but not muxerInit, so close nOutputFd[%d]",
                        pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nOutputFd);
                    pRecRenderData->rec_file = FILE_NEED_SWITCH_TO_NORMAL;
                    if(pRecRenderData->nOutputFd>=0)
                    {
                        close(pRecRenderData->nOutputFd);
                        pRecRenderData->nOutputFd = -1;
                        pRecRenderData->pCallbacks->EventHandler(pRecRenderData->hSelf, pRecRenderData->pAppData, COMP_EventRecordDone, pRecRenderData->mMppChnInfo.mChnId, 0, NULL);
                    }
                    pthread_mutex_lock(&pRecRenderData->mutex_reset_writer_lock);
                    if(pRecRenderData->reset_fd_flag)
                    {
                        if(pRecRenderData->nSwitchFd < 0)
                        {
                            aloge("fatal error! muxChn[%d] reset__fd_flag is true but switchFd[%d]<0, check code!",
                                pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nSwitchFd);
                            pRecRenderData->reset_fd_flag = FALSE;
                        }
                    }
                    pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
                }
                else
                {
                    alogd("muxChn[%d] switch file normal begin", pRecRenderData->mMppChnInfo.mChnId);
                    pRecRenderData->rec_file = FILE_NEED_SWITCH_TO_NORMAL;
                }
                switchRet = SUCCESS;
                cmd_msg.pReply->ReplyResult = (int)switchRet;
                cdx_sem_up(&cmd_msg.pReply->ReplySem);
                free(cmd_msg.mpData);
                cmd_msg.mpData = NULL;
            }
            else if(RecSink_SwitchFd == cmd)
            {
                ERRORTYPE eSwitchFdRet = SUCCESS;
                pthread_mutex_lock(&pRecRenderData->mutex_reset_writer_lock);
                /*if (nIsImpact == 1 && (pThiz->rec_file == FILE_IMPACT_RECDRDING || pThiz->rec_file == FILE_NEED_SWITCH_TO_IMPACT))
                {
                    alogd("impact file is recording, don't accept new impact Fd[%d].", nFd);
                    pthread_mutex_unlock(&pThiz->mutex_reset_writer_lock);
                    return SUCCESS;
                }*/
                if(FILE_NEED_SWITCH_TO_NORMAL == pRecRenderData->rec_file)
                {
                    alogw("Be careful! muxChn[%d] is in switch_to_normal, refuse to switch fd",
                        pRecRenderData->mMppChnInfo.mChnId);
                    pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
                    eSwitchFdRet = ERR_MUX_NOT_PERM;
                    goto _switchFdExit;
                }
                int nFd = cmddata;
                int nFallocateLen = cmd_msg.para1;
                if(nFd < 0)
                {
                    aloge("Be careful! fd[%d] < 0, app maybe want to bypass frames in this muxChn[%d] next file",
                        nFd, pRecRenderData->mMppChnInfo.mChnId);
                    pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
                    //return ERR_MUX_ILLEGAL_PARAM;
                    goto _switchFdExit;
                }
                if(pRecRenderData->nSwitchFd >= 0)
                {
                    aloge("fatal error! muxChn[%d] nSwitchFd[%d] already exist, check code!",
                        pRecRenderData->mMppChnInfo.mChnId, pRecRenderData->nSwitchFd);
                    close(pRecRenderData->nSwitchFd);
                    pRecRenderData->nSwitchFd = -1;
                    pRecRenderData->nSwitchFdFallocateSize = 0;
                }
                if(nFd >= 0)
                {
                    pRecRenderData->nSwitchFd = dup(nFd);
                    if(pRecRenderData->nSwitchFd < 0)
                    {
                        aloge("fatal error! muxChn[%d] dup fail:[%d]->[%d],(%s)",
                            pRecRenderData->mMppChnInfo.mChnId,
                            nFd, pRecRenderData->nSwitchFd, strerror(errno));
                        system("lsof");
                    }
                    //pThiz->nSwitchFd = dup2SeldomUsedFd(nFd);
                }
                pRecRenderData->nSwitchFdFallocateSize = nFallocateLen;
                alogd("muxChn[%d] dup setfd[%d] to nSwitchFd[%d]", 
                    pRecRenderData->mMppChnInfo.mChnId,
                    nFd, pRecRenderData->nSwitchFd);
                if(TRUE == pRecRenderData->reset_fd_flag)
                {
                    aloge("fatal error! muxChn[%d] reset__fd_flag is already true, check code!",
                        pRecRenderData->mMppChnInfo.mChnId);
                }
                pRecRenderData->reset_fd_flag = TRUE;
                pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
            _switchFdExit:
                if(cmd_msg.pReply)
                {
                    cmd_msg.pReply->ReplyResult = (int)eSwitchFdRet;
                    cdx_sem_up(&cmd_msg.pReply->ReplySem);
                }
                else
                {
                    aloge("fatal error! must need pReply to reply!");
                }
            }
            else if (RecRenderComp_InputFrameAvailable == cmd)
            {
                alogv("inputFrameAvailable");
                //pRecRenderData->mNoInputFrameFlag = 0;
            }
EndProcessMessage:
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        if (pRecRenderData->state == COMP_StateExecuting)
        {
            eRet = RecRender_GetOneBuffer(pRecRenderData);
            if(eRet != SUCCESS)
            {
                if(list_empty(&pRecRenderData->mValidRSPacketList))
                {
                    int nMsgNum = TMessage_WaitQueueNotEmpty(&pRecRenderData->cmd_queue, 5*1000);
                    if(nMsgNum <= 0)
                    {
                        alogd("Be careful! muxChn[%d] wait InputFrame timeout, msgNum:%d",
                            pRecRenderData->mMppChnInfo.mChnId, nMsgNum);
                    }
                    goto PROCESS_MESSAGE;
                }
            }

            pthread_mutex_lock(&pRecRenderData->mutex_reset_writer_lock);
            pRSPacket = RecSinkGetRSPacket(pRecRenderData);
            /* if(pRecSink->need_to_force_i_frm == 1)
            {
                aloge("frc_i_frm_p_ck:%d-%d-%d",pRecSink->mMuxerGrpId,pRSPacket->mStreamType,pRSPacket->mFlags);
            } */
            int need_switch_audio = 0;      // audio duration meet the maxduration
            BOOL need_switch_video = FALSE;
            need_switch_video = RecSinkIfNeedSwitchFile(pRecRenderData, &need_switch_audio);

            if(TRUE == need_switch_video)   // video time duration reach file duration. meet setting.case1: key frm,with audio;case2:key frm,without audio;case3:not key frm
            {
                if(pRSPacket->mStreamType == CODEC_TYPE_VIDEO)
                {
                    if(pRSPacket->mStreamId >= MAX_VIDEO_TRACK_COUNT)
                    {
                        aloge("fatal error! muxChn[%d] wrong streamId[%d]", pRecRenderData->mMppChnInfo.mChnId, pRSPacket->mStreamId);
                    }
                    if(!(pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME))
                    { //(1)current packet is not key frame, so notify venc to force I frame, one time is enough.
                        if(FALSE == pRecRenderData->mPrefetchFlag[pRSPacket->mStreamId])
                        {
                            //pRecRenderData->need_to_force_i_frm[pRSPacket->mStreamId] = 1;
                            if(0 == pRecRenderData->forced_i_frm[pRSPacket->mStreamId])
                            {
                                if(pRecRenderData->mRefVideoStreamIndex == pRSPacket->mStreamId)
                                {
                                    int nAudioStreamId = pRecRenderData->media_inf.mVideoInfoValidNum;
                                    alogd("muxChn[%d] rec_sink_to_force_i_frm_v:[%d]-%d-%d-%d-%lld-%lld-%lld--%lld-%lld-%lld-%lld-%d-%d-%d-%d-%d-%d",
                                        pRecRenderData->mMppChnInfo.mChnId,
                                        pRSPacket->mStreamId,
                                        pRecRenderData->media_inf.mMediaVideoInfo[pRSPacket->mStreamId].nWidth,
                                        pRecRenderData->mDuration[pRSPacket->mStreamId],
                                        pRecRenderData->mDurationAudio,
                                        pRecRenderData->mLoopDuration,
                                        pRecRenderData->mLoopDurationAudio,
                                        pRecRenderData->mCurFileEndTm,
                                        pRecRenderData->mBasePts[pRSPacket->mStreamId],
                                        pRecRenderData->mBasePts[nAudioStreamId],
                                        pRecRenderData->mOrigBasePts[pRSPacket->mStreamId],
                                        pRSPacket->mPts, pRSPacket->mStreamType, pRSPacket->mFlags,
                                        pRecRenderData->mVideoFrameCounter, pRecRenderData->mAudioFrmCnt,
                                        pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt);
                                }
                                else
                                {
                                    alogd("muxChn[%d] rec_sink_to_force_i_frm_v:[%d]-%d-%d--%lld-%lld-%lld-%d-%d",
                                        pRecRenderData->mMppChnInfo.mChnId,
                                        pRSPacket->mStreamId,
                                        pRecRenderData->media_inf.mMediaVideoInfo[pRSPacket->mStreamId].nWidth,
                                        pRecRenderData->mDuration[pRSPacket->mStreamId],
                                        pRecRenderData->mBasePts[pRSPacket->mStreamId],
                                        pRecRenderData->mOrigBasePts[pRSPacket->mStreamId],
                                        pRSPacket->mPts, pRSPacket->mStreamType, pRSPacket->mFlags);
                                }
                                //if file duration policy is accurate, no need to force I frame, because we will switch
                                //file immediately.
                                if(pRecRenderData->mFileDurationPolicy != RecordFileDurationPolicy_AccurateDuration)
                                {
                                    RecSinkToForceIFrame(pRecRenderData, pRSPacket->mStreamId);
                                }
                                pRecRenderData->forced_i_frm[pRSPacket->mStreamId] = 1;
                            }
                        }
                    }
                    else
                    { //(2)if meet KeyFrame, begin to prefetch.
                        if(!pRecRenderData->mPrefetchFlag[pRSPacket->mStreamId])
                        {
                            if(pRecRenderData->mRecordMode & RECORDER_MODE_AUDIO)
                            {
                                int videoStreamIndex = pRSPacket->mStreamId;
                                int audioStreamIndex = pRecRenderData->media_inf.mVideoInfoValidNum;
                                int64_t nVideoTotalTmUs = pRecRenderData->mPrevPts[videoStreamIndex]+pRecRenderData->mBasePts[videoStreamIndex];
                                int64_t nAudioTotalTmUs = pRecRenderData->mPrevPts[audioStreamIndex]+pRecRenderData->mBasePts[audioStreamIndex];
                                alogd("muxChn[%d] avsync_ch_v:[%d]-%d-%lld-%lld-%d-%d",
                                    pRecRenderData->mMppChnInfo.mChnId,
                                    pRSPacket->mStreamId,
                                    pRecRenderData->media_inf.mMediaVideoInfo[videoStreamIndex].nWidth,
                                    nAudioTotalTmUs,
                                    nVideoTotalTmUs,
                                    pRSPacket->mStreamType, pRSPacket->mFlags);
                            }
                            pRecRenderData->mPrefetchFlag[pRSPacket->mStreamId] = TRUE;   // to cache video pkt in order to push more audio packets to current file
                        }
                    }
                }
                BOOL bGrant = RecSinkGrantSwitchFile(pRecRenderData, pRSPacket);
                if(bGrant)
                {
                    if(pRecRenderData->nOutputFd >= 0)
                    {
                        int nAudioStreamId = pRecRenderData->media_inf.mVideoInfoValidNum;
                        alogd("muxChn[%d] avsync_rc_swfv:[%d]-%d-%d-%d-%lld-%lld--%lld-%lld-%lld-%d-%d-%d-%d-%lld-%d-%d",
                            pRecRenderData->mMppChnInfo.mChnId,
                            pRecRenderData->mRefVideoStreamIndex,
                            pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].nWidth,
                            pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex], pRecRenderData->mDurationAudio,
                            pRecRenderData->mLoopDuration, pRecRenderData->mCurFileEndTm,
                            pRecRenderData->mBasePts[pRecRenderData->mRefVideoStreamIndex],
                            pRecRenderData->mBasePts[nAudioStreamId],
                            pRecRenderData->mOrigBasePts[pRecRenderData->mRefVideoStreamIndex],
                            pRecRenderData->mVideoFrameCounter, pRecRenderData->mAudioFrmCnt,
                            pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt,
                            pRSPacket->mPts, pRSPacket->mStreamType, pRSPacket->mStreamId);
                        RecSinkAddRSPacketToPrefetchList(pRecRenderData, pRSPacket);
                        pRSPacket = NULL;
                        RecSinkSwitchFile(pRecRenderData, 0);
                        pRSPacket = RecSinkGetRSPacket(pRecRenderData);
                        if(pRSPacket != NULL)
                        {
                            if(pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO)
                            {
                                if(pRSPacket->mStreamType==CODEC_TYPE_VIDEO)
                                {
                                    if(!(pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME))
                                    {
                                        aloge("fatal error! muxChn[%d] next file streamId[%d] first frame is not key video frame!",
                                            pRecRenderData->mMppChnInfo.mChnId, pRSPacket->mStreamId);
                                    }
                                }
                            }
                        }
                        else
                        {
                            aloge("fatal error! muxChn[%d] has no packet!", pRecRenderData->mMppChnInfo.mChnId);
                        }
                    }
                    else
                    {
                        if(FALSE == pRecRenderData->nCallbackOutFlag)
                        {
                            aloge("fatal error! muxChn[%d] has not fd, but callback mode is false, so don't switch file!",
                                pRecRenderData->mMppChnInfo.mChnId);
                        }
                    }
                }
            }
            else if(need_switch_audio) //audio duration reach file duration first.
            {
                if(pRSPacket->mStreamType==CODEC_TYPE_AUDIO)
                {
                    if(!pRecRenderData->mPrefetchFlagAudio)
                    {
                        if(!(pRecRenderData->mRecordMode & RECORDER_MODE_AUDIO))
                        {
                            aloge("fatal error! muxChn[%d] recorder no audio?", pRecRenderData->mMppChnInfo.mChnId);
                        }
                        int audioStreamIndex = pRecRenderData->media_inf.mVideoInfoValidNum;
                        int64_t nVideoTotalTmUs = pRecRenderData->mPrevPts[pRecRenderData->mRefVideoStreamIndex]+pRecRenderData->mBasePts[pRecRenderData->mRefVideoStreamIndex];
                        int64_t nAudioTotalTmUs = pRecRenderData->mPrevPts[audioStreamIndex]+pRecRenderData->mBasePts[audioStreamIndex];
                        alogd("muxChn[%d] avsync_ch_a:[%d]-%d-%lld-%lld-%d-%d", pRecRenderData->mMppChnInfo.mChnId,
                            pRecRenderData->mRefVideoStreamIndex, pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].nWidth,
                            nAudioTotalTmUs, nVideoTotalTmUs, pRSPacket->mStreamType, pRSPacket->mFlags);
                        pRecRenderData->mPrefetchFlagAudio = TRUE; // to cache audio pkt in order to push more video packet to current file
                    }
                }
                BOOL bGrant = RecSinkGrantSwitchFileAudio(pRecRenderData, pRSPacket);
                if(TRUE == bGrant)  // grant to switch file
                {
                    if(pRecRenderData->nOutputFd >= 0)
                    {
                        int nAudioStreamId = pRecRenderData->media_inf.mVideoInfoValidNum;
                        alogd("muxChn[%d] avsync_rc_swfa:[%d]-%d-%d-%d-%lld-%lld-%lld--%lld-%lld-%lld-%d-%d-%d-%d-%d-%d-%lld",
                            pRecRenderData->mMppChnInfo.mChnId,
                            pRecRenderData->mRefVideoStreamIndex, pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].nWidth,
                            pRecRenderData->mDuration[pRecRenderData->mRefVideoStreamIndex], pRecRenderData->mDurationAudio,
                            pRecRenderData->mLoopDuration, pRecRenderData->mLoopDurationAudio, pRecRenderData->mCurFileEndTm,
                            pRecRenderData->mBasePts[pRecRenderData->mRefVideoStreamIndex],
                            pRecRenderData->mBasePts[nAudioStreamId],
                            pRecRenderData->mOrigBasePts[pRecRenderData->mRefVideoStreamIndex],
                            pRecRenderData->mVideoFrameCounter, pRecRenderData->mAudioFrmCnt,
                            pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt,
                            pRSPacket->mStreamType, pRSPacket->mFlags, pRSPacket->mPts);
                        RecSinkAddRSPacketToPrefetchList(pRecRenderData, pRSPacket);
                        pRSPacket = NULL;
                        RecSinkSwitchFile(pRecRenderData, 0);
                        pRSPacket = RecSinkGetRSPacket(pRecRenderData);
                        if(NULL == pRSPacket)
                        {
                            aloge("fatal error! muxChn[%d] has no packet!", pRecRenderData->mMppChnInfo.mChnId);
                        }
                    }
                    else
                    {
                        if(FALSE == pRecRenderData->nCallbackOutFlag)
                        {
                            aloge("fatal error! muxChn[%d] has not fd, but callback mode is false, so don't switch file!",
                                pRecRenderData->mMppChnInfo.mChnId);
                        }
                    }
                }
                else        // not grant to switch file now
                {
                    if(TRUE == pRecRenderData->bTimeMeetAudio) //time meet,but not all videos meet key frame
                    {
                        if(pRSPacket->mStreamType==CODEC_TYPE_VIDEO)
                        {
                            if(pRSPacket->mStreamId >= MAX_VIDEO_TRACK_COUNT)
                            {
                                aloge("fatal error! muxChn[%d] wrong streamId[%d]", pRecRenderData->mMppChnInfo.mChnId, pRSPacket->mStreamId);
                            }
                            //(1)if video need force I frame?
                            if(!(pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME))
                            {
                                if(FALSE == pRecRenderData->mPrefetchFlag[pRSPacket->mStreamId])
                                {
                                    //pRecRenderData->need_to_force_i_frm[pRSPacket->mStreamId] = 1;
                                    if(0 == pRecRenderData->forced_i_frm[pRSPacket->mStreamId])
                                    {
                                        int nAudioStreamId = pRecRenderData->media_inf.mVideoInfoValidNum;
                                        alogd("muxChn[%d] rec_sink_to_force_i_frm_a:[%d]-%d-%d-%d-%lld-%lld-%lld--%lld-%lld-%lld-%d-%d-%d-%d-%d-%d-%lld",
                                            pRecRenderData->mMppChnInfo.mChnId, pRSPacket->mStreamId,
                                            pRecRenderData->media_inf.mMediaVideoInfo[pRSPacket->mStreamId].nWidth,
                                            pRecRenderData->mDuration[pRSPacket->mStreamId], pRecRenderData->mDurationAudio,
                                            pRecRenderData->mLoopDuration, pRecRenderData->mLoopDurationAudio, pRecRenderData->mCurFileEndTm,
                                            pRecRenderData->mBasePts[pRSPacket->mStreamId], pRecRenderData->mBasePts[nAudioStreamId],
                                            pRecRenderData->mOrigBasePts[pRSPacket->mStreamId],
                                            pRecRenderData->mVideoFrameCounter, pRecRenderData->mAudioFrmCnt,
                                            pRecRenderData->video_frm_cnt, pRecRenderData->audio_frm_cnt,
                                            pRSPacket->mStreamType, pRSPacket->mFlags, pRSPacket->mPts);
                                        RecSinkToForceIFrame(pRecRenderData, pRSPacket->mStreamId);
                                        pRecRenderData->forced_i_frm[pRSPacket->mStreamId] = 1;
                                    }
                                }
                            }
                            //(2)if meet KeyFrame, begin to prefetch.
                            else
                            {
                                if(!pRecRenderData->mPrefetchFlag[pRSPacket->mStreamId])
                                {
                                    pRecRenderData->mPrefetchFlag[pRSPacket->mStreamId] = TRUE;   // can cache video pkt
                                }
                            }

                            //if video0 need continue to write, then calculate video0's write packet number and pts.
                            if((pRecRenderData->mRefVideoStreamIndex == pRSPacket->mStreamId) && (FALSE == pRecRenderData->mPrefetchFlag[pRecRenderData->mRefVideoStreamIndex]))
                            {
                                pRecRenderData->mVideoFrmCntWriteMore++;
                                alogd("muxChn[%d] avsync_cal:[%d]-%d-%lld-%lld-%d", pRecRenderData->mMppChnInfo.mChnId,
                                    pRecRenderData->mRefVideoStreamIndex, pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].nWidth,
                                    pRSPacket->mPts, pRecRenderData->mVideoPtsWriteMoreSt, pRecRenderData->mVideoFrmCntWriteMore);
                                if(-1 == pRecRenderData->mVideoPtsWriteMoreSt)    // to record the pts of video frame write more
                                {
                                    pRecRenderData->mVideoPtsWriteMoreSt = pRSPacket->mPts;
                                    pRecRenderData->mVideoPtsWriteMoreEnd = pRSPacket->mPts;
                                }
                                else
                                {
                                    pRecRenderData->mVideoPtsWriteMoreEnd = pRSPacket->mPts;
                                }
                            }
                        }
                    }
                }
            }

            if(pRecRenderData->mbMuxerInit == FALSE && (pRecRenderData->nCallbackOutFlag==TRUE || pRecRenderData->nOutputFd>=0 || pRecRenderData->reset_fd_flag==TRUE))
            {
                BOOL bGrant = TRUE;
                if(pRecRenderData->mRecordMode & RECORDER_MODE_VIDEO)
                {
                    if(pRSPacket->mStreamType==CODEC_TYPE_VIDEO)
                    {
                        if(pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME)
                        {
                            bGrant = TRUE;
                        }
                        else
                        {
                            alogd("Be careful! muxChn[%d] video stream[%d] first packet[%d-0x%x] is not key frame! But still grant!",
                                pRecRenderData->mMppChnInfo.mChnId,
                                pRSPacket->mStreamId, pRSPacket->mStreamType, pRSPacket->mFlags);
                            bGrant = TRUE;
                        }
                    }
                    else
                    {
                        alogd("Be careful! muxChn[%d] stream[%d] first packet[%d-0x%x] is not video stream! But still grant!", 
                            pRecRenderData->mMppChnInfo.mChnId,
                            pRSPacket->mStreamId, pRSPacket->mStreamType, pRSPacket->mFlags);
                        bGrant = TRUE;
                    }
                }
                //first packet will be video or not.
                //first video frame may not be key frame, if not key frame, we will not write it to file, we will only
                //record its pts as basePts of this file, then release it.
//                    alogd("muxerId[%d]: first packet grant[%d]: stream_type[%d], flags[0x%x], pts[%lld]ms, videoSize[%dx%d]",
//                        pRecSink->mMuxerId, bGrant, pRSPacket->mStreamType, pRSPacket->mFlags, pRSPacket->mPts, pRecSink->mpMediaInf->nWidth, pRecSink->mpMediaInf->nHeight);
                if(bGrant)
                {
                    if(RecSinkMuxerInit(pRecRenderData, pRSPacket) != SUCCESS)
                    {
                        aloge("fatal error! muxChn[%d] ValidMuxerInit Error!", pRecRenderData->mMppChnInfo.mChnId);
                        pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
                        RecSinkReleaseRSPacket(pRecRenderData, pRSPacket);
                        goto PROCESS_MESSAGE;
                    }
                    pRecRenderData->mVideoPtsWriteMoreSt = -1;
                    if(pRecRenderData->nCallbackOutFlag == FALSE)
                    {
                        pRecRenderData->need_set_next_fd = TRUE;
                    }
                }
            }
            pthread_mutex_unlock(&pRecRenderData->mutex_reset_writer_lock);
            BOOL bReleasePacket = TRUE;
            if(pRecRenderData->mbMuxerInit)
            {
                if(CODEC_TYPE_VIDEO == pRSPacket->mStreamType)
                {
                    if(pRSPacket->mStreamId >= MAX_VIDEO_TRACK_COUNT)
                    {
                        aloge("fatal error! muxChn[%d] streamId[%d] wrong!", pRecRenderData->mMppChnInfo.mChnId, pRSPacket->mStreamId);
                    }
                    if(pRecRenderData->mPrefetchFlag[pRSPacket->mStreamId])
                    {
                        RecSinkAddRSPacketToPrefetchList(pRecRenderData, pRSPacket);
                        bReleasePacket = FALSE;
                    }
                    else
                    {
                        if(!pRecRenderData->key_frm_sent[pRSPacket->mStreamId])
                        {
                            if(pRSPacket->mFlags&AVPACKET_FLAG_KEYFRAME)
                            {
                                pRecRenderData->key_frm_sent[pRSPacket->mStreamId] = 1;
                                RecSinkWriteRSPacket(pRecRenderData, pRSPacket);
                            }
                            else
                            {
                                pRecRenderData->v_frm_drp_cnt[pRSPacket->mStreamId]++;
                                alogd("muxChn[%d] stream[%d] rc_snk_drp_v_frm:%d", pRecRenderData->mMppChnInfo.mChnId,
                                    pRSPacket->mStreamId, pRecRenderData->v_frm_drp_cnt[pRSPacket->mStreamId]);
                                //even we drop the front video frames, but first video frame's pts must be set as basePts of
                                //current stream, to guarantee av sync.
                                RecSinkUpdateStreamBasePts(pRecRenderData, pRSPacket);
                            }
                        }
                        else
                        {
                            RecSinkWriteRSPacket(pRecRenderData, pRSPacket);
                        }
                    }
                }
                else if(CODEC_TYPE_AUDIO == pRSPacket->mStreamType)
                {
                    if(pRecRenderData->mPrefetchFlagAudio)
                    {
                        RecSinkAddRSPacketToPrefetchList(pRecRenderData, pRSPacket);
                        bReleasePacket = FALSE;
                    }
                    else
                    {
                        RecSinkWriteRSPacket(pRecRenderData, pRSPacket);
                    }
                }
                else
                {
                    RecSinkWriteRSPacket(pRecRenderData, pRSPacket);
                }
            }
            //release RSPacket
            if(bReleasePacket)
            {
                RecSinkReleaseRSPacket(pRecRenderData, pRSPacket);
            }
RELEASE:
            if(RecSinkIfNeedRequestNextFd(pRecRenderData))
            {
                alogd("muxChn[%d] Need set next fd: Policy[%d] refvStrm[%d] videosize[%dx%d], vLoopDur[%lld]ms, aLoopDur[%lld]ms, curFileEndTm[%lld]ms",
                    pRecRenderData->mMppChnInfo.mChnId,
                    pRecRenderData->mFileDurationPolicy, pRecRenderData->mRefVideoStreamIndex,
                    pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].nWidth,
                    pRecRenderData->media_inf.mMediaVideoInfo[pRecRenderData->mRefVideoStreamIndex].nHeight,
                    pRecRenderData->mLoopDuration, pRecRenderData->mLoopDurationAudio, pRecRenderData->mCurFileEndTm);
                pRecRenderData->need_set_next_fd = FALSE;
                pRecRenderData->pCallbacks->EventHandler(pRecRenderData->hSelf, pRecRenderData->pAppData, 
                    COMP_EventNeedNextFd, pRecRenderData->mMppChnInfo.mChnId, 0, NULL);
            }
            //no need ReleaseBuffer because we don't RecRender_RefBuffer().
            //RecRender_ReleaseBuffer(pRecRenderData, &RSPacket);
        }
        else
        {
            //cdx_sem_down(&pRecRenderData->cdx_sem_wait_message);
            TMessage_WaitQueueNotEmpty(&pRecRenderData->cmd_queue, 0);
        }
    }
EXIT:
    alogv("Recorder Render ComponentThread stopped");
    return (void*) SUCCESS;
}

