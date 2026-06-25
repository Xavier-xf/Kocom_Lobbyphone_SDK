/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : mpi_venc.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/23
  Last Modified :
  Description   : mpi functions implement for mux module.
  Function List :
  History       :
******************************************************************************/
#define LOG_TAG "mpi_mux"
#include <utils/plat_log.h>

//ref platform headers
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "plat_type.h"
#include "plat_errno.h"
#include "plat_defines.h"
#include "plat_math.h"
#include "cdx_list.h"

//media api headers to app
#include "mm_common.h"
#include "mm_comm_mux.h"
#include "mpi_mux.h"

//media internal common headers.
#include <tsemaphore.h>
#include <media_common.h>
#include <media_common_vcodec.h>
#include <mm_component.h>
#include <mpi_mux_common.h>
#include <RecRender_Component.h>
#include <EncodedStream.h>

typedef struct
{
    struct list_head mMuxChnList;   //element type: MUX_CHN_S
    pthread_mutex_t mMuxChnListLock;
}MuxChnManager;

MuxChnManager *gMuxChnMgr = NULL;

typedef struct MUX_CHN_S
{
    MUX_CHN             mMuxChn;    // mux channel id, [0, MUX_MAX_CHN_NUM)
    MM_COMPONENTTYPE    *mComp;  // mux component instance
    cdx_sem_t           mSemCompCmd;
    //cdx_sem_t           mSemAddChn;
    //cdx_sem_t           mSemRemoveChn;
    MPPCallbackInfo     mCallbackInfo;
    cdx_sem_t           mSemWaitEncodedStreamRelease;
    pthread_mutex_t     mSendStreamSyncLock; //used for AW_MPI_MUX_SendVideoStreamSync(),etc.
    struct list_head    mList;
}MUX_CHN_S;

static ERRORTYPE MUX_searchExistChn_l(PARAM_IN MUX_CHN muxChn, PARAM_OUT MUX_CHN_S **ppChn)
{
    ERRORTYPE ret = ERR_MUX_UNEXIST;
    MUX_CHN_S* pEntry;
    list_for_each_entry(pEntry, &gMuxChnMgr->mMuxChnList, mList)
    {
        if(pEntry->mMuxChn == muxChn)
        {
            if(ppChn)
            {
                *ppChn = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    return ret;
}

static ERRORTYPE MUX_searchExistChn(PARAM_IN MUX_CHN muxChn, PARAM_OUT MUX_CHN_S **ppChn)
{
    ERRORTYPE ret = ERR_MUX_UNEXIST;
    MUX_CHN_S* pEntry;
    pthread_mutex_lock(&gMuxChnMgr->mMuxChnListLock);
    ret = MUX_searchExistChn_l(muxChn, ppChn);
    pthread_mutex_unlock(&gMuxChnMgr->mMuxChnListLock);
    return ret;
}

static ERRORTYPE addChn_l(MUX_CHN_S *pChn)
{
    list_add_tail(&pChn->mList, &gMuxChnMgr->mMuxChnList);
    return SUCCESS;
}

static ERRORTYPE addChn(MUX_CHN_S *pChn)
{
    pthread_mutex_lock(&gMuxChnMgr->mMuxChnListLock);
    list_add_tail(&pChn->mList, &gMuxChnMgr->mMuxChnList);
    pthread_mutex_unlock(&gMuxChnMgr->mMuxChnListLock);
    return SUCCESS;
}

static ERRORTYPE removeChn(MUX_CHN_S *pChn)
{
    pthread_mutex_lock(&gMuxChnMgr->mMuxChnListLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gMuxChnMgr->mMuxChnListLock);
    return SUCCESS;
}

static MUX_CHN_S* MUX_CHN_S_Construct()
{
    int ret;
    MUX_CHN_S *pChn = (MUX_CHN_S*)malloc(sizeof(MUX_CHN_S));
    if(NULL == pChn)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChn, 0, sizeof(MUX_CHN_S));
    ret = cdx_sem_init(&pChn->mSemCompCmd, 0);
    if (ret!=0)
	{
        aloge("fatal error! cdx sem init fail:%d", ret);
	}
    //cdx_sem_init(&pChn->mSemAddChn, 0);
    //cdx_sem_init(&pChn->mSemRemoveChn, 0);
    ret = cdx_sem_init(&pChn->mSemWaitEncodedStreamRelease, 0);
    if (ret!=0)
	{
        aloge("fatal error! cdx sem init fail:%d", ret);
	}
    ret = pthread_mutex_init(&pChn->mSendStreamSyncLock, NULL);
	if (ret!=0)
	{
        aloge("fatal error! mutex init fail:%d", ret);
	}
    return pChn;
}

static void MUX_CHN_S_Destruct(MUX_CHN_S *pChn)
{
    if(pChn->mComp)
    {
        aloge("fatal error! mux component need free before!");
        COMP_FreeHandle(pChn->mComp);
        pChn->mComp = NULL;
    }
    cdx_sem_deinit(&pChn->mSemCompCmd);
    //cdx_sem_deinit(&pGroup->mSemAddChn);
    //cdx_sem_deinit(&pGroup->mSemRemoveChn);
    cdx_sem_deinit(&pChn->mSemWaitEncodedStreamRelease);
    pthread_mutex_destroy(&pChn->mSendStreamSyncLock);
    free(pChn);
}

ERRORTYPE MUX_Construct(void)
{
    ERRORTYPE eError = SUCCESS;
    int ret;
    if(gMuxChnMgr)
    {
        alogw("mpi_mux module already create!");
        return SUCCESS;
    }
    gMuxChnMgr = malloc(sizeof(MuxChnManager));
    if(NULL == gMuxChnMgr)
    {
        aloge("fatal error! malloc fail!");
        return ERR_MUX_NULL_PTR;
    }
    INIT_LIST_HEAD(&gMuxChnMgr->mMuxChnList);
    ret = pthread_mutex_init(&gMuxChnMgr->mMuxChnListLock, NULL);
	if (ret!=0)
	{
        aloge("fatal error! mutex init fail");
        eError = ERR_MUX_NOMEM;
        goto _err0;
	}
    return eError;

_err0:
    free(gMuxChnMgr);
    gMuxChnMgr = NULL;
    return eError;

}

ERRORTYPE MUX_Destruct(void)
{
    if(NULL == gMuxChnMgr)
    {
        alogw("mpi_mux module already NULL!");
        return SUCCESS;
    }
    pthread_mutex_lock(&gMuxChnMgr->mMuxChnListLock);
    if(!list_empty(&gMuxChnMgr->mMuxChnList))
    {
        pthread_mutex_unlock(&gMuxChnMgr->mMuxChnListLock);
        aloge("fatal error! mux channel list is not empty!");
        return ERR_MUX_BUSY;
    }
    pthread_mutex_unlock(&gMuxChnMgr->mMuxChnListLock);
    pthread_mutex_destroy(&gMuxChnMgr->mMuxChnListLock);
    free(gMuxChnMgr);
    gMuxChnMgr = NULL;
    return SUCCESS;

}

MM_COMPONENTTYPE *MUX_GetChnComp(PARAM_IN MPP_CHN_S *pMppchn)
{
    MUX_CHN_S* pChn;

    if (MUX_searchExistChn(pMppchn->mChnId, &pChn) != SUCCESS)
    {
        return NULL;
    }
    return pChn->mComp;
}

static ERRORTYPE RecRenderEventHandler(
     PARAM_IN COMP_HANDLETYPE hComponent,
     PARAM_IN void* pAppData,
     PARAM_IN COMP_EVENTTYPE eEvent,
     PARAM_IN unsigned int nData1,
     PARAM_IN unsigned int nData2,
     PARAM_IN void* pEventData)
{
    ERRORTYPE ret;
    MUX_CHN_S *pChn = (MUX_CHN_S*)pAppData;
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_MUX;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pChn->mMuxChn;

    switch(eEvent)
    {
        case COMP_EventCmdComplete:
        {
            if(COMP_CommandStateSet == nData1)
            {
                alogv("muxChn EventCmdComplete, current StateSet[%d]", nData2);
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else
            {
                alogw("Low probability! what command[0x%x]?", nData1);
                break;
            }
        }
        case COMP_EventError:
        {
            if(ERR_MUX_SAMESTATE == nData1)
            {
                alogv("set same state to muxChn!");
                cdx_sem_up(&pChn->mSemCompCmd);
                break;
            }
            else if(ERR_MUX_INVALIDSTATE == nData1)
            {
                aloge("why muxChn state turn to invalid?");
                break;
            }
            else if(ERR_MUX_INCORRECT_STATE_TRANSITION == nData1)
            {
                aloge("fatal error! muxChn state transition incorrect.");
                break;
            }
            else
            {
                aloge("fatal error! other param[%d][%d]", nData1, nData2);
                break;
            }
        }
        case COMP_EventNeedNextFd:
        {
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_NEED_NEXT_FD, (void*)&nData1);
            break;
        }
        case COMP_EventRecordDone:
        {
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_RECORD_DONE, (void*)&nData1);
            break;
        }
        case COMP_EventBsframeAvailable:
        {
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_BSFRAME_AVAILABLE, pEventData);
            break;
        }
        case COMP_EventWriteDiskError:
        {
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_WRITE_DISK_ERROR, (void*)&nData1);
            break;
        }
        case COMP_EventMuxForceIFrame:
        {
            CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &ChannelInfo, MPP_EVENT_MUX_FORCE_I_FRAME, (void*)&nData1);
            break;
        }
        default:
            aloge("fatal error! unknown event[0x%x]", eEvent);
            break;
    }
    return SUCCESS;
}

static ERRORTYPE RecRenderEmptyBufferDone(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN void* pAppData,
        PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    MUX_CHN_S *pChn = (MUX_CHN_S*)pAppData;
    EncodedStream *pEncodedStream = (EncodedStream*)pBuffer->pAppPrivate;
    int nStreamId = pBuffer->nInputPortIndex;

    MPP_CHN_S stMppChn;
    stMppChn.mModId = MOD_ID_MUX;
    stMppChn.mDevId = 0;
    stMppChn.mChnId = pChn->mMuxChn;
    /* ENCODED_FRAME_ID_INVALID means that usr sends frame synchronously */
    if (ENCODED_FRAME_ID_INVALID == pEncodedStream->nID)
    {
        cdx_sem_up(&pChn->mSemWaitEncodedStreamRelease);
        return SUCCESS;
    }
    else
    {
        CHECK_MPP_CALLBACK(pChn->mCallbackInfo.callback);
        if(CDX_PacketVideo == pEncodedStream->media_type)
        {
            MUX_VENC_STREAM_S stMuxVencStream;
            VENC_PACK_S stVencPack;
            memset(&stMuxVencStream, 0, sizeof(MUX_VENC_STREAM_S));
            memset(&stVencPack, 0, sizeof(VENC_PACK_S));
            stMuxVencStream.mVencStream.mpPack = &stVencPack;
            stMuxVencStream.mVencStream.mPackCount = 1;

            stMuxVencStream.mVencStream.mpPack[0].mpAddr0 = pEncodedStream->pBuffer;
            stMuxVencStream.mVencStream.mpPack[0].mpAddr1 = pEncodedStream->pBufferExtra;
            stMuxVencStream.mVencStream.mpPack[0].mpAddr2 = pEncodedStream->pBufferExtra2;
            stMuxVencStream.mVencStream.mpPack[0].mLen0 = pEncodedStream->nBufferLen;
            stMuxVencStream.mVencStream.mpPack[0].mLen1 = pEncodedStream->nBufferExtraLen;
            stMuxVencStream.mVencStream.mpPack[0].mLen2 = pEncodedStream->nBufferExtraLen2;
            stMuxVencStream.mVencStream.mpPack[0].mPTS = pEncodedStream->nTimeStamp;
            stMuxVencStream.mVencStream.mpPack[0].mbFrameEnd = TRUE;
            stMuxVencStream.mVencStream.mSeq = pEncodedStream->nID;
            stMuxVencStream.mStreamId = nStreamId;

            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &stMppChn, MPP_EVENT_RELEASE_VENC_STREAM, (void*)&stMuxVencStream);
        }
        else if(CDX_PacketAudio == pEncodedStream->media_type)
        {
            MUX_AENC_STREAM_S stMuxAencStream;
            memset(&stMuxAencStream, 0, sizeof(MUX_AENC_STREAM_S));

            stMuxAencStream.mAencStream.pStream = pEncodedStream->pBuffer;
            stMuxAencStream.mAencStream.mLen = pEncodedStream->nBufferLen;
            stMuxAencStream.mAencStream.pStreamExtra = pEncodedStream->pBufferExtra;
            stMuxAencStream.mAencStream.mExtraLen = pEncodedStream->nBufferExtraLen;
            stMuxAencStream.mAencStream.mTimeStamp = pEncodedStream->nTimeStamp;
            stMuxAencStream.mAencStream.mId = pEncodedStream->nID;
            stMuxAencStream.mStreamId = nStreamId;

            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &stMppChn, MPP_EVENT_RELEASE_AENC_STREAM, (void*)&stMuxAencStream);
        }
        else if(CDX_PacketSubtitle == pEncodedStream->media_type)
        {
            MUX_TENC_STREAM_S stMuxTencStream;
            memset(&stMuxTencStream, 0, sizeof(MUX_TENC_STREAM_S));

            stMuxTencStream.mTencStream.pStream = pEncodedStream->pBuffer;
            stMuxTencStream.mTencStream.mLen = pEncodedStream->nBufferLen;
            stMuxTencStream.mTencStream.pStreamExtra = pEncodedStream->pBufferExtra;
            stMuxTencStream.mTencStream.mExtraLen = pEncodedStream->nBufferExtraLen;
            stMuxTencStream.mTencStream.mTimeStamp = pEncodedStream->nTimeStamp;
            stMuxTencStream.mTencStream.mId = pEncodedStream->nID;
            stMuxTencStream.mStreamId = nStreamId;

            pChn->mCallbackInfo.callback(pChn->mCallbackInfo.cookie, &stMppChn, MPP_EVENT_RELEASE_TENC_STREAM, (void*)&stMuxTencStream);
        }
        else
        {
            aloge("fatal error! muxChn[%d] encoded stream type[%d] is wrong!", pChn->mMuxChn, pEncodedStream->media_type);
        }
        return SUCCESS;
    }
}

COMP_CALLBACKTYPE RecRenderCallback = {
	.EventHandler = RecRenderEventHandler,
	.EmptyBufferDone = RecRenderEmptyBufferDone,
	.FillBufferDone = NULL,
};

ERRORTYPE AW_MPI_MUX_CreateChn(MUX_CHN muxChn, MUX_CHN_ATTR_S *pChnAttr, int nFd, int nFallocateLen)
{
    if(!(muxChn>=0 && muxChn <MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    if(NULL == pChnAttr)
    {
        aloge("fatal error! illagal MuxChnAttr!");
        return ERR_MUX_ILLEGAL_PARAM;
    }
    pthread_mutex_lock(&gMuxChnMgr->mMuxChnListLock);
    if(SUCCESS == MUX_searchExistChn_l(muxChn, NULL))
    {
        pthread_mutex_unlock(&gMuxChnMgr->mMuxChnListLock);
        return ERR_MUX_EXIST;
    }

    MUX_CHN_S *pNode = MUX_CHN_S_Construct();
    pNode->mMuxChn = muxChn;

    //create mux Component here
    ERRORTYPE eRet = SUCCESS;
    eRet = COMP_GetHandle((COMP_HANDLETYPE*)&pNode->mComp, CDX_ComponentNameMuxer, (void*)pNode, &RecRenderCallback);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! get comp handle fail!");
    }
    MPP_CHN_S ChannelInfo;
    ChannelInfo.mModId = MOD_ID_MUX;
    ChannelInfo.mDevId = 0;
    ChannelInfo.mChnId = pNode->mMuxChn;
    eRet = COMP_SetConfig(pNode->mComp, COMP_IndexVendorMPPChannelInfo, (void*)&ChannelInfo);
    eRet = COMP_SetConfig(pNode->mComp, COMP_IndexVendorMuxChnAttr, (void*)pChnAttr);
    CdxFdT stCdxFd;
    stCdxFd.mFd = nFd;
    stCdxFd.mnFallocateLen = nFallocateLen;
    eRet = COMP_SetConfig(pNode->mComp, COMP_IndexVendorMuxFD, (void*)&stCdxFd);
    eRet = COMP_SendCommand(pNode->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
    cdx_sem_down(&pNode->mSemCompCmd);
    //create mux Component done!

    addChn_l(pNode);
    pthread_mutex_unlock(&gMuxChnMgr->mMuxChnListLock);
    return SUCCESS;
}

ERRORTYPE AW_MPI_MUX_DestroyChn(MUX_CHN muxChn)
{
    ERRORTYPE ret;
    if(!(muxChn >= 0 && muxChn < MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE eRet;
    if(pChn->mComp)
    {
        COMP_STATETYPE nCompState;
        if(SUCCESS == COMP_GetState(pChn->mComp, &nCompState))
        {
            if(nCompState == COMP_StateIdle)
            {
                //COMP_SetConfig(pChn->mComp, COMP_IndexVendorMuxShutDownType, (void *)&bShutDownNowFlag);
                eRet = COMP_SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateLoaded, NULL);
                cdx_sem_down(&pChn->mSemCompCmd);
                if(eRet != SUCCESS)
                {
                    aloge("fatal error! why transmit state to loaded fail?");
                }
            }
            else if(nCompState == COMP_StateLoaded)
            {
                eRet = SUCCESS;
            }
            else if(nCompState == COMP_StateInvalid)
            {
                alogw("Low probability! Component StateInvalid?");
                eRet = SUCCESS;
            }
            else
            {
                aloge("fatal error! invalid VeChn[%d] state[0x%x]!", muxChn, nCompState);
                eRet = ERR_MUX_INCORRECT_STATE_OPERATION;
            }

            if(eRet == SUCCESS)
            {
                removeChn(pChn);
                COMP_FreeHandle(pChn->mComp);
                pChn->mComp = NULL;
                MUX_CHN_S_Destruct(pChn);
            }
            ret = eRet;
        }
        else
        {
            aloge("fatal error! GetState fail!");
            ret = ERR_MUX_BUSY;
        }
    }
    else
    {
        aloge("fatal error! no mux component!");
        list_del(&pChn->mList);
        MUX_CHN_S_Destruct(pChn);
        ret = SUCCESS;
    }
    return ret;
}

ERRORTYPE AW_MPI_MUX_StartChn(MUX_CHN muxChn)
{
    if(!(muxChn>=0 && muxChn <MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    int ret;
    int eRet;
    COMP_STATETYPE nCompState;
    eRet = COMP_GetState(pChn->mComp, &nCompState);
    if(SUCCESS == eRet && COMP_StateIdle == nCompState)
    {
        eRet = COMP_SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateExecuting, NULL);
        cdx_sem_down(&pChn->mSemCompCmd);
        ret = SUCCESS;
    }
    else
    {
        alogd("mux comp state[0x%x], do nothing!", nCompState);
        ret = SUCCESS;
    }
    return ret;
}

/**
  if bShutDownNowFlag is true, will discard cache data.
*/
ERRORTYPE AW_MPI_MUX_StopChn(MUX_CHN muxChn, BOOL bShutDownNowFlag)
{
    if(!(muxChn>=0 && muxChn<MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid mux chn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE eError;
    ERRORTYPE eRet;
    COMP_STATETYPE nCompState;
    eRet = COMP_GetState(pChn->mComp, &nCompState);
    if(COMP_StateExecuting == nCompState || COMP_StatePause == nCompState)
    {
        COMP_SetConfig(pChn->mComp, COMP_IndexVendorMuxShutDownType, (void *)&bShutDownNowFlag);
        eRet = COMP_SendCommand(pChn->mComp, COMP_CommandStateSet, COMP_StateIdle, NULL);
        cdx_sem_down(&pChn->mSemCompCmd);
        eError = eRet;
    }
    else if(COMP_StateIdle == nCompState)
    {
        alogv("mux comp state[0x%x], do nothing!", nCompState);
        eError = SUCCESS;
    }
    else
    {
        aloge("fatal error! check mux state[0x%x]!", nCompState);
        eError = SUCCESS;
    }
    return eError;
}

/**
  set output fd.
*/
ERRORTYPE AW_MPI_MUX_SetFd(MUX_CHN muxChn, int nFd, int nFallocateLen)
{
    if(!(muxChn>=0 && muxChn < MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE eState;
    ret = COMP_GetState(pChn->mComp, &eState);
    if(eState != COMP_StateIdle)
    {
        aloge("muxChn[%d] wrong state[0x%x], return!", muxChn, eState);
        return ERR_MUX_NOT_PERM;
    }
    CdxFdT stCdxFd;
    stCdxFd.mFd = nFd;
    stCdxFd.mnFallocateLen = nFallocateLen;
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorMuxFD, (void*)&stCdxFd);
    return ret;
}

ERRORTYPE AW_MPI_MUX_GetChnAttr(MUX_CHN muxChn, MUX_CHN_ATTR_S *pChnAttr)
{
    if(!(muxChn>=0 && muxChn < MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    ret = COMP_GetConfig(pChn->mComp, COMP_IndexVendorMuxChnAttr, (void*)pChnAttr);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SetChnAttr(MUX_CHN muxChn, MUX_CHN_ATTR_S *pChnAttr)
{
    if(!(muxChn>=0 && muxChn < MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorMuxChnAttr, (void*)pChnAttr);
    return ret;
}

/**
  set spspps to mux.
  It is better to call this function after calling AW_MPI_MUX_SetVeChnBindStreamId(), because we need to know VeChn
  match to which streamId.
*/
ERRORTYPE AW_MPI_MUX_SetH264SpsPpsInfo(MUX_CHN muxChn, VENC_CHN VeChn, VencHeaderData *pH264SpsPpsInfo)
{
    if(!(muxChn>=0 && muxChn < MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState && COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    VencHeaderDataParam stVencHeaderDataParam;
    stVencHeaderDataParam.mVeChn = VeChn;
    stVencHeaderDataParam.mH264SpsPpsInfo.nLength = pH264SpsPpsInfo->nLength;
    stVencHeaderDataParam.mH264SpsPpsInfo.pBuffer = pH264SpsPpsInfo->pBuffer;
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorExtraData, (void*)&stVencHeaderDataParam);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SetH265SpsPpsInfo(MUX_CHN muxChn, VENC_CHN VeChn, VencHeaderData *pH265SpsPpsInfo)
{
    return AW_MPI_MUX_SetH264SpsPpsInfo(muxChn, VeChn, pH265SpsPpsInfo);
}

ERRORTYPE AW_MPI_MUX_SetThmPic(MUX_CHN muxChn, char *p_thm_pic, int thm_pic_size)
{
    if(!(muxChn >=0 && muxChn < MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    THM_PIC thm_pic;
    thm_pic.p_thm_pic_addr = p_thm_pic;
    thm_pic.thm_pic_size = thm_pic_size;
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorMuxSetThmPic, (void*)&thm_pic);
    return ret;
}

/**
  set next fd to muxChn.

  muxChn will store this fd, when file duration come, close current file, use this fd to create next file to store
  stream contiguously.
*/
ERRORTYPE AW_MPI_MUX_SwitchFd(MUX_CHN muxChn, int fd, int nFallocateLen)
{
    if(!(muxChn>=0 && muxChn < MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    CdxFdT stCdxFd;
    stCdxFd.mFd = fd;
    stCdxFd.mnFallocateLen = nFallocateLen;
    //stCdxFd.mIsImpact = 0;
    //stCdxFd.mMuxerId = stMppChnAttr.mMuxerId;
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexVendorMuxSwitchFd, (void*)&stCdxFd);
    return ret;
}

/**
  switch file right now.

  muxChn will store this fd, then switch file after this call, close current file, use fd to create next file. When fd
  is -1, not create next file, and ignore stream until valid fd coming.
  user will receive MPP_EVENT_RECORD_DONE message when switching file is done.

  @param fd
    for next file. if -1, not create next file.
*/
ERRORTYPE AW_MPI_MUX_SwitchFileNormal(MUX_CHN muxChn, int fd, int nFallocateLen)
{
    if(!(muxChn>=0 && muxChn < MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateExecuting != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }
    SwitchFileNormalInfo stInfo;
    memset(&stInfo, 0, sizeof(SwitchFileNormalInfo));
    //stInfo.mMuxerId = stMppChnAttr.mMuxerId;
    stInfo.mFd = fd;
    stInfo.mnFallocateLen = nFallocateLen;
    ret = COMP_SetConfig(pChn->mComp, COMP_IndexSwitchFileNormal, (void*)&stInfo);
    return ret;
}

ERRORTYPE AW_MPI_MUX_RegisterCallback(MUX_CHN muxChn, MPPCallbackInfo *pCallback)
{
    if(!(muxChn>=0 && muxChn <MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    pChn->mCallbackInfo.callback = pCallback->callback;
    pChn->mCallbackInfo.cookie = pCallback->cookie;
    return SUCCESS;
}

/**
  List valid streamIds in this mux channel.
  MUX_CHN_ATTR_S->mVideoAttr[MAX_VIDEO_TRACK_COUNT] must list all streams whatever is used in mux channel.
  But not all streams will be sent to this mux channel.
  So use this api to tell mux channel which streams will be sent to it.

  @param nStrmIdsCnt
    must <= MAX_TRACK_COUNT
*/
ERRORTYPE AW_MPI_MUX_SetStrmIds(MUX_CHN muxChn, MuxStreamIdsInfo *pStrmIdsInfo)
{
    if(!(muxChn>=0 && muxChn <MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE eState;
    ret = COMP_GetState(pChn->mComp, &eState);
    if(eState != COMP_StateIdle)
    {
        aloge("muxChn[%d] wrong state[0x%x], return!", muxChn, eState);
        return ERR_MUX_NOT_PERM;
    }
    return COMP_SetConfig(pChn->mComp, COMP_IndexVendorMuxStrmIds, (void *)pStrmIdsInfo);
}

ERRORTYPE AW_MPI_MUX_SetSwitchFileDurationPolicy(MUX_CHN muxChn, RecordFileDurationPolicy ePolicy)
{
    if(!(muxChn>=0 && muxChn <MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateIdle != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    return COMP_SetConfig(pChn->mComp, COMP_IndexVendorMuxSwitchPolicy, (void *)&ePolicy);
}

ERRORTYPE AW_MPI_MUX_GetSwitchFileDurationPolicy(MUX_CHN muxChn, RecordFileDurationPolicy *pPolicy)
{
    if(!(muxChn>=0 && muxChn <MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(COMP_StateIdle != nState || COMP_StateExecuting != nState || COMP_StatePause != nState)
    {
        aloge("wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    return COMP_GetConfig(pChn->mComp, COMP_IndexVendorMuxSwitchPolicy, (void *)pPolicy);

}

ERRORTYPE AW_MPI_MUX_SetSdCardState(MUX_CHN muxChn, BOOL bExist)
{
    if(!(muxChn>=0 && muxChn<MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pGrp;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pGrp))
    {
        return ERR_MUX_UNEXIST;
    }
    int bState = (int)bExist;
    return COMP_SetConfig(pGrp->mComp, COMP_IndexVendorSdcardState, (void*)&bState);
}

/**
  for non-tunnel mode, directly bind veChn and streamId. streamId = portIndex in mpi_mux.
  must be set before AW_MPI_MUX_SetH264SpsPpsInfo(), because AW_MPI_MUX_SetH264SpsPpsInfo() need to know which stream
  spsppsinfo is matched to.
*/
ERRORTYPE AW_MPI_MUX_SetVeChnBindStreamId(MUX_CHN muxChn, VENC_CHN VeChn, int nStreamId)
{
    if(!(muxChn>=0 && muxChn<MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxGroup[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    ERRORTYPE ret;
    COMP_STATETYPE nState;
    ret = COMP_GetState(pChn->mComp, &nState);
    if(nState != COMP_StateIdle)
    {
        aloge("fatal error! wrong state[0x%x], return!", nState);
        return ERR_MUX_NOT_PERM;
    }

    VeChnBindStreamIdNode stVeChnBindStreamId;
    memset(&stVeChnBindStreamId, 0, sizeof(stVeChnBindStreamId));
    stVeChnBindStreamId.mStreamId = nStreamId;
    stVeChnBindStreamId.mVeChn.mModId = MOD_ID_VENC;
    stVeChnBindStreamId.mVeChn.mDevId = 0;
    stVeChnBindStreamId.mVeChn.mChnId = VeChn;
    return COMP_SetConfig(pChn->mComp, COMP_IndexVendorMuxVeChnBindStreamId, &stVeChnBindStreamId);
}

static ERRORTYPE aw_mux_SendStream(MUX_CHN_S *pChn, MUX_CHN_ATTR_S *pChnAttr, EncodedStream *pEncodedStream, int nStreamId, BOOL bSyncFlag)
{
    ERRORTYPE ret = SUCCESS;
    COMP_STATETYPE eState;
    ret = COMP_GetState(pChn->mComp, &eState);
    if(eState != COMP_StateExecuting && eState != COMP_StateIdle && eState != COMP_StatePause)
    {
        aloge("fatal error! muxChn[%d] wrong state[0x%x], return!", pChn->mMuxChn, eState);
        return ERR_MUX_INCORRECT_STATE_OPERATION;
    }
    if(bSyncFlag)
    {
        pthread_mutex_lock(&pChn->mSendStreamSyncLock);
        // ignore usr pEncodedStream->nID and don't change it, just use it to identify
        // sync or async in RecRenderEmptyBufferDone().
        EncodedStream tmpEncodedStream;
        memcpy(&tmpEncodedStream, pEncodedStream, sizeof(EncodedStream));
        tmpEncodedStream.nID = ENCODED_FRAME_ID_INVALID;

        COMP_BUFFERHEADERTYPE bufferHeader;
        bufferHeader.pAppPrivate = (void*)&tmpEncodedStream;
        bufferHeader.nInputPortIndex = nStreamId;
        ret = COMP_EmptyThisBuffer(pChn->mComp, &bufferHeader);

        // blocked here, until the encodedStream be returned.
        cdx_sem_down(&pChn->mSemWaitEncodedStreamRelease);
        pthread_mutex_unlock(&pChn->mSendStreamSyncLock);
    }
    else
    {
        if (ENCODED_FRAME_ID_INVALID == pEncodedStream->nID)
        {
            aloge("fatal error! muxChn[%d] invalid audio frame id[0x%x] in async mode!", pChn->mMuxChn, pEncodedStream->nID);
            return ERR_AO_ILLEGAL_PARAM;
        }
        COMP_BUFFERHEADERTYPE bufferHeader;
        bufferHeader.pAppPrivate = (void*)pEncodedStream;
        bufferHeader.nInputPortIndex = nStreamId;
        ret = COMP_EmptyThisBuffer(pChn->mComp, &bufferHeader);
    }

    return ret;
}

/*
 * use memory from VENC_STREAM_S. no need malloc memory self.
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
    else if(PT_JPEG == eType || PT_MJPEG == eType)
    {
        pDst->nFlags |= CEDARV_FLAG_KEYFRAME;
    }
    
    pDst->pBuffer = pSrc->mpPack[0].mpAddr0;
    pDst->nBufferLen = pSrc->mpPack[0].mLen0;
    pDst->pBufferExtra = pSrc->mpPack[0].mpAddr1;
    pDst->nBufferExtraLen = pSrc->mpPack[0].mLen1;
    pDst->pBufferExtra2 = pSrc->mpPack[0].mpAddr2;
    pDst->nBufferExtraLen2 = pSrc->mpPack[0].mLen2;

    pDst->nPackNum = pSrc->mpPack[0].mDataNum;
    for(int i=0; i<pSrc->mpPack[0].mDataNum; i++)
    {
        pDst->stPackInfos[i].uType = map_VENC_DATA_TYPE_U_to_VENC_OUTPUT_PACK_TYPE(pSrc->mpPack[0].mPackInfo[i].mPackType, eType);
        pDst->stPackInfos[i].nOffset = pSrc->mpPack[0].mPackInfo[i].mPackOffset;
        pDst->stPackInfos[i].nLength = pSrc->mpPack[0].mPackInfo[i].mPackLength;
    }

    pDst->infoVersion = -1;
    pDst->pChangedStreamsInfo = NULL;
    pDst->duration = -1;
    return SUCCESS;
}

static ERRORTYPE aw_mux_SendVideoStream(MUX_CHN muxChn, VENC_STREAM_S *pStream, int nStreamId, BOOL bSyncFlag)
{
    ERRORTYPE ret;
    if(!(muxChn>=0 && muxChn<MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    MUX_CHN_ATTR_S stChnAttr;
    ret = COMP_GetConfig(pChn->mComp, COMP_IndexVendorMuxChnAttr, (void*)&stChnAttr);
    PAYLOAD_TYPE_E ePayloadType;
    if(nStreamId < stChnAttr.mVideoAttrValidNum)
    {
        ePayloadType = stChnAttr.mVideoAttr[nStreamId].mVideoEncodeType;
    }
    else
    {
        aloge("fatal error! muxChn[%d] video streamId[%d>=%d] wrong", muxChn, nStreamId, stChnAttr.mVideoAttrValidNum);
        ePayloadType = PT_MAX;
    }
    EncodedStream stEncodedStream;
    ret = InitEncodedStreamByVENC_STREAM_S(&stEncodedStream, pStream, ePayloadType);
    ret = aw_mux_SendStream(pChn, &stChnAttr, &stEncodedStream, nStreamId, bSyncFlag);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SendVideoStream(MUX_CHN muxChn, VENC_STREAM_S *pStream, int nStreamId)
{
    return aw_mux_SendVideoStream(muxChn, pStream, nStreamId, FALSE);
}

/**
  use sync mode, when function return, user can release stream.
  component use internal memory to store input stream, so it can return immediately.
*/
ERRORTYPE AW_MPI_MUX_SendVideoStreamSync(MUX_CHN muxChn, VENC_STREAM_S *pStream, int nStreamId)
{
    return aw_mux_SendVideoStream(muxChn, pStream, nStreamId, TRUE);
}

static ERRORTYPE InitEncodedStreamByAUDIO_STREAM_S(EncodedStream *pDst, AUDIO_STREAM_S *pSrc)
{
    if (pDst == NULL || pSrc == NULL)
    {
        aloge("fatal error! Empty pointer.");
        return FAILURE;
    }
    memset(pDst, 0, sizeof(EncodedStream));
    pDst->media_type = CDX_PacketAudio;
    pDst->pBuffer = pSrc->pStream;
    pDst->nBufferLen = pSrc->mLen;
    pDst->pBufferExtra = pSrc->pStreamExtra;
    pDst->nBufferExtraLen = pSrc->mExtraLen;
    pDst->nFilledLen = pSrc->mLen + pSrc->mExtraLen;
    pDst->nTimeStamp = pSrc->mTimeStamp;
    pDst->nID = pSrc->mId;
    return SUCCESS;
}

static ERRORTYPE aw_mux_SendAudioStream(MUX_CHN muxChn, AUDIO_STREAM_S *pStream, int nStreamId, BOOL bSyncFlag)
{
    ERRORTYPE ret;
    if(!(muxChn>=0 && muxChn<MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    MUX_CHN_ATTR_S stChnAttr;
    ret = COMP_GetConfig(pChn->mComp, COMP_IndexVendorMuxChnAttr, (void*)&stChnAttr);
    if(nStreamId != stChnAttr.mVideoAttrValidNum)
    {
        aloge("fatal error! muxChn[%d] auido streamId[%d!=%d] wrong", muxChn, nStreamId, stChnAttr.mVideoAttrValidNum);
    }
    EncodedStream stEncodedStream;
    ret = InitEncodedStreamByAUDIO_STREAM_S(&stEncodedStream, pStream);
    ret = aw_mux_SendStream(pChn, &stChnAttr, &stEncodedStream, nStreamId, bSyncFlag);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SendAudioStream(MUX_CHN muxChn, AUDIO_STREAM_S *pStream, int nStreamId)
{
    return aw_mux_SendAudioStream(muxChn, pStream, nStreamId, FALSE);
}

ERRORTYPE AW_MPI_MUX_SendAudioStreamSync(MUX_CHN muxChn, AUDIO_STREAM_S *pStream, int nStreamId)
{
    return aw_mux_SendAudioStream(muxChn, pStream, nStreamId, TRUE);
}

static ERRORTYPE InitEncodedStreamByTEXT_STREAM_S(EncodedStream *pDst, TEXT_STREAM_S *pSrc)
{
    if (pDst == NULL || pSrc == NULL)
    {
        aloge("fatal error! Empty pointer.");
        return FAILURE;
    }
    memset(pDst, 0, sizeof(EncodedStream));
    pDst->media_type = CDX_PacketSubtitle;
    pDst->pBuffer = pSrc->pStream;
    pDst->nBufferLen = pSrc->mLen;
    pDst->pBufferExtra = pSrc->pStreamExtra;
    pDst->nBufferExtraLen = pSrc->mExtraLen;
    pDst->nFilledLen = pSrc->mLen + pSrc->mExtraLen;
    pDst->nTimeStamp = pSrc->mTimeStamp;
    pDst->nID = pSrc->mId;
    return SUCCESS;
}

static ERRORTYPE aw_mux_SendTextStream(MUX_CHN muxChn, TEXT_STREAM_S *pStream, int nStreamId, BOOL bSyncFlag)
{
    ERRORTYPE ret;
    if(!(muxChn>=0 && muxChn<MUX_MAX_CHN_NUM))
    {
        aloge("fatal error! invalid muxChn[%d]!", muxChn);
        return ERR_MUX_INVALID_CHNID;
    }
    MUX_CHN_S *pChn;
    if(SUCCESS != MUX_searchExistChn(muxChn, &pChn))
    {
        return ERR_MUX_UNEXIST;
    }
    MUX_CHN_ATTR_S stChnAttr;
    ret = COMP_GetConfig(pChn->mComp, COMP_IndexVendorMuxChnAttr, (void*)&stChnAttr);
    if(nStreamId < stChnAttr.mVideoAttrValidNum)
    {
        aloge("fatal error! muxChn[%d] text streamId[%d<%d] wrong", muxChn, nStreamId, stChnAttr.mVideoAttrValidNum);
    }
    EncodedStream stEncodedStream;
    ret = InitEncodedStreamByTEXT_STREAM_S(&stEncodedStream, pStream);
    ret = aw_mux_SendStream(pChn, &stChnAttr, &stEncodedStream, nStreamId, bSyncFlag);
    return ret;
}

ERRORTYPE AW_MPI_MUX_SendTextStream(MUX_CHN muxChn, TEXT_STREAM_S *pStream, int nStreamId)
{
    return aw_mux_SendTextStream(muxChn, pStream, nStreamId, FALSE);
}

ERRORTYPE AW_MPI_MUX_SendTextStreamSync(MUX_CHN muxChn, TEXT_STREAM_S *pStream, int nStreamId)
{
    return aw_mux_SendTextStream(muxChn, pStream, nStreamId, TRUE);
}

