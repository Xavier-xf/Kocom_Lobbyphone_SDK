/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : AIChannel_Component.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/04/27
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "AIChannel_Component"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <memory.h>
#include <sys/prctl.h>

#include <mm_component.h>
#include <media_common_aio.h>
#include <tmessage.h>
#include <tsemaphore.h>
#include <SystemBase.h>
#include "AIChannel_Component.h"
#include <AIOCompStream.h>

#include <ConfigOption.h>

#include <cdx_list.h>

//#define AI_SAVE_AUDIO_PCM

#define AUDIOCODEC_FRAMESIZE (1024) //1024 alsaFrames make an AudioFrame.
static void *AIChannel_ComponentThread(void *pThreadData);

AudioCaptureFrameInfo* constructAudioCaptureFrameInfo(unsigned int id)
{
    AudioCaptureFrameInfo *pNode = (AudioCaptureFrameInfo*)malloc(sizeof(AudioCaptureFrameInfo));
    if(NULL == pNode)
    {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
    }
    memset(pNode, 0, sizeof(*pNode));
    pNode->mFrame.mId = id;
    return pNode;
}

int clearAudioCaptureFrameInfo(AudioCaptureFrameInfo *pThiz)
{
    if(pThiz->mpCaptureFrame)
    {
        pThiz->mpCaptureFrame->mTimeStamp = -1;
    }
    if(pThiz->mpAecFrame)
    {
        pThiz->mpAecFrame->stRefFrame.mTimeStamp = -1;
        pThiz->mpAecFrame->bValid = FALSE;
    }
    if(pThiz->mbMallocFlag)
    {
        if(pThiz->mFrame.mpAddr)
        {
            free(pThiz->mFrame.mpAddr);
            pThiz->mFrame.mpAddr = NULL;
            pThiz->mFrame.mLen = 0;
        }
        pThiz->mbMallocFlag = FALSE;
    }
    pThiz->mFrame.mTimeStamp = -1;
    return 0;
}

void destructAudioCaptureFrameInfo(AudioCaptureFrameInfo *pThiz)
{
    if(pThiz->mpCaptureFrame)
    {
        if(pThiz->mpCaptureFrame->mpAddr)
        {
            free(pThiz->mpCaptureFrame->mpAddr);
            pThiz->mpCaptureFrame->mpAddr = NULL;
            pThiz->mpCaptureFrame->mLen = 0;
        }
        free(pThiz->mpCaptureFrame);
        pThiz->mpCaptureFrame = NULL;
    }
    if(pThiz->mpAecFrame)
    {
        if(pThiz->mpAecFrame->stRefFrame.mpAddr)
        {
            free(pThiz->mpAecFrame->stRefFrame.mpAddr);
            pThiz->mpAecFrame->stRefFrame.mpAddr = NULL;
            pThiz->mpAecFrame->stRefFrame.mLen = 0;
        }
        free(pThiz->mpAecFrame);
        pThiz->mpAecFrame = NULL;
    }
    if(pThiz->mbMallocFlag)
    {
        if(pThiz->mFrame.mpAddr)
        {
            free(pThiz->mFrame.mpAddr);
            pThiz->mFrame.mpAddr = NULL;
            pThiz->mFrame.mLen = 0;
        }
        pThiz->mbMallocFlag = FALSE;
    }
    free(pThiz);
}

static AudioCaptureFrameInfo* SetPCMDataToAudioCaptureFrameInfoList(AI_CHN_DATA_S *pChnData, char *pOutBuf, int nSize,
    char *pOutBufExtra, int nSizeExtra, int64_t nPts)
{
    int nFrameBytes = nSize + nSizeExtra;
    if(list_empty(&pChnData->mIdleOutFrameList))
    {
        AudioCaptureFrameInfo *pNode = constructAudioCaptureFrameInfo(pChnData->mFrameNodeNum);
        list_add_tail(&pNode->mList, &pChnData->mIdleOutFrameList);
        pChnData->mFrameNodeNum++;
        if(0 == pChnData->mFrameNodeNum % 100)
        {
            alogw("Be careful! audio capture frame node num reach:%d", pChnData->mFrameNodeNum);
        }
    }
    AudioCaptureFrameInfo *pFrameNode = list_first_entry(&pChnData->mIdleOutFrameList, AudioCaptureFrameInfo, mList);
    pFrameNode->pOrigData = pOutBuf;
    pFrameNode->nOrigDataLen = nSize;
    pFrameNode->pOrigDataExtra = pOutBufExtra;
    pFrameNode->nOrigDataExtraLen = nSizeExtra;
    if(0 == pChnData->mpAioAttr->mbBypassAec)
    {
        pFrameNode->mFrame.mSamplerate = pChnData->mpAioAttr->enSamplerate;
        pFrameNode->mFrame.mBitwidth = pChnData->mpAioAttr->enBitwidth;
        pFrameNode->mFrame.mSoundmode = pChnData->mpAioAttr->enSoundmode;
        pFrameNode->mFrame.mTimeStamp = nPts;
        pFrameNode->mFrame.mLen = nFrameBytes;
        if(NULL == pOutBufExtra)
        {
            pFrameNode->mFrame.mpAddr = pOutBuf;
            pFrameNode->mbMallocFlag = false;
        }
        else
        {
            pFrameNode->mFrame.mpAddr = malloc(nFrameBytes);
            if(NULL == pFrameNode->mFrame.mpAddr)
            {
                aloge("fatal error! aiChn[%d-%d] malloc fail!", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
            }
            memcpy(pFrameNode->mFrame.mpAddr, pOutBuf, nSize);
            memcpy(pFrameNode->mFrame.mpAddr+nSize, pOutBufExtra, nSizeExtra);
            pFrameNode->mbMallocFlag = true;
        }
    }
    else
    {
        int nTotalChnNum = pChnData->mpPcmCfg->chnCnt;
        int nCaptureChnNum = pChnData->mpAioAttr->mChnCnt;
        int nRefChnNum = nTotalChnNum-nCaptureChnNum;
        if(nRefChnNum <= 0)
        {
            aloge("fatal error! why refChnNum wrong?[%d-%d-%d]", nTotalChnNum, nCaptureChnNum, nRefChnNum);
        }
        if(NULL == pFrameNode->mpCaptureFrame)
        {
            pFrameNode->mpCaptureFrame = (AUDIO_FRAME_S*)malloc(sizeof(AUDIO_FRAME_S));
            if(NULL == pFrameNode->mpCaptureFrame)
            {
                aloge("fatal error! malloc fail");
            }
            memset(pFrameNode->mpCaptureFrame, 0, sizeof(AUDIO_FRAME_S));
            pFrameNode->mpCaptureFrame->mId = pFrameNode->mFrame.mId;
        }
        pFrameNode->mpCaptureFrame->mSamplerate = pChnData->mpAioAttr->enSamplerate;
        pFrameNode->mpCaptureFrame->mBitwidth = pChnData->mpAioAttr->enBitwidth;
        pFrameNode->mpCaptureFrame->mSoundmode = pChnData->mpAioAttr->enSoundmode;
        pFrameNode->mpCaptureFrame->mTimeStamp = nPts;
        int nCaptureLen = nFrameBytes*nCaptureChnNum/nTotalChnNum;
        if(pFrameNode->mpCaptureFrame->mLen < nCaptureLen)
        {
            if(pFrameNode->mpCaptureFrame->mpAddr)
            {
                alogw("fatal error! audio capture frame len:%d<%d, check code!", pFrameNode->mpCaptureFrame->mLen, nCaptureLen);
                free(pFrameNode->mpCaptureFrame->mpAddr);
                pFrameNode->mpCaptureFrame->mpAddr = NULL;
                pFrameNode->mpCaptureFrame->mLen = 0;
            }
        }
        if(NULL == pFrameNode->mpCaptureFrame->mpAddr)
        {
            pFrameNode->mpCaptureFrame->mLen = nFrameBytes*nCaptureChnNum/nTotalChnNum;
            pFrameNode->mpCaptureFrame->mpAddr = malloc(pFrameNode->mpCaptureFrame->mLen);
            if(NULL == pFrameNode->mpCaptureFrame->mpAddr)
            {
                aloge("fatal error! malloc fail, len:%d", pFrameNode->mpCaptureFrame->mLen);
            }
        }

        if(NULL == pFrameNode->mpAecFrame)
        {
            pFrameNode->mpAecFrame = (AEC_FRAME_S*)malloc(sizeof(AEC_FRAME_S));
            if(NULL == pFrameNode->mpAecFrame)
            {
                aloge("fatal error! malloc fail");
            }
            memset(pFrameNode->mpAecFrame, 0, sizeof(AEC_FRAME_S));
            pFrameNode->mpAecFrame->stRefFrame.mId = pFrameNode->mFrame.mId;
        }
        pFrameNode->mpAecFrame->stRefFrame.mSamplerate = pChnData->mpAioAttr->enSamplerate;
        pFrameNode->mpAecFrame->stRefFrame.mBitwidth = pChnData->mpAioAttr->enBitwidth;
        pFrameNode->mpAecFrame->stRefFrame.mSoundmode = pChnData->mpAioAttr->enSoundmode;
        pFrameNode->mpAecFrame->stRefFrame.mTimeStamp = nPts;
        int nRefLen = nFrameBytes*nRefChnNum/nTotalChnNum;
        if(pFrameNode->mpAecFrame->stRefFrame.mLen < nRefLen)
        {
            if(pFrameNode->mpAecFrame->stRefFrame.mpAddr)
            {
                alogw("fatal error! audio ref frame len:%d<%d, check code!", pFrameNode->mpAecFrame->stRefFrame.mLen, nRefLen);
                free(pFrameNode->mpAecFrame->stRefFrame.mpAddr);
                pFrameNode->mpAecFrame->stRefFrame.mpAddr = NULL;
                pFrameNode->mpAecFrame->stRefFrame.mLen = 0;
            }
        }
        if(NULL == pFrameNode->mpAecFrame->stRefFrame.mpAddr)
        {
            pFrameNode->mpAecFrame->stRefFrame.mLen = nFrameBytes*nRefChnNum/nTotalChnNum;
            pFrameNode->mpAecFrame->stRefFrame.mpAddr = malloc(pFrameNode->mpAecFrame->stRefFrame.mLen);
            if(NULL == pFrameNode->mpAecFrame->stRefFrame.mpAddr)
            {
                aloge("fatal error! malloc fail, len:%d", pFrameNode->mpAecFrame->stRefFrame.mLen);
            }
        }
        //divide pcm data to capture frame and refFrame.
        int nCapAlsaFrameBytes = pChnData->mAudioHwOutputAlsaFrameBytes*nCaptureChnNum/nTotalChnNum;
        int nRefAlsaFrameBytes = pChnData->mAudioHwOutputAlsaFrameBytes*nRefChnNum/nTotalChnNum;
        if(nCapAlsaFrameBytes + nRefAlsaFrameBytes != pChnData->mAudioHwOutputAlsaFrameBytes)
        {
            aloge("fatal error! why alsaFrame divide is wrong?[%d-%d-%d]", nCapAlsaFrameBytes, nRefAlsaFrameBytes,
                pChnData->mAudioHwOutputAlsaFrameBytes);
        }
        int nAlsaFrameCnt = nSize/pChnData->mAudioHwOutputAlsaFrameBytes;
        if(nSize%pChnData->mAudioHwOutputAlsaFrameBytes != 0)
        {
            aloge("fatal error! PCMBufDataLen error! [%d-%d]", nSize, pChnData->mAudioHwOutputAlsaFrameBytes);
        }
        if(pOutBuf)
        {
            char *pCaptureAddr = (char*)pFrameNode->mpCaptureFrame->mpAddr;
            char *pRefAddr = (char*)pFrameNode->mpAecFrame->stRefFrame.mpAddr;
            for(int i=0; i<nAlsaFrameCnt; i++)
            {
                memcpy(pCaptureAddr+i*nCapAlsaFrameBytes, pOutBuf+i*pChnData->mAudioHwOutputAlsaFrameBytes, nCapAlsaFrameBytes);
                memcpy(pRefAddr+i*nRefAlsaFrameBytes, pOutBuf+i*pChnData->mAudioHwOutputAlsaFrameBytes+nCapAlsaFrameBytes, nRefAlsaFrameBytes);
            }
        }
        int nExtraAlsaFrameCnt = nSizeExtra/pChnData->mAudioHwOutputAlsaFrameBytes;
        if(nSizeExtra%pChnData->mAudioHwOutputAlsaFrameBytes != 0)
        {
            aloge("fatal error! PCMBufDataLenExtra error! [%d-%d]", nSizeExtra, pChnData->mAudioHwOutputAlsaFrameBytes);
        }
        if(pOutBufExtra)
        {
            char *pExtraCaptureAddr = (char*)pFrameNode->mpCaptureFrame->mpAddr + nAlsaFrameCnt*nCapAlsaFrameBytes;
            char *pExtraRefAddr = (char*)pFrameNode->mpAecFrame->stRefFrame.mpAddr + nAlsaFrameCnt*nRefAlsaFrameBytes;
            for(int i=0; i<nExtraAlsaFrameCnt; i++)
            {
                memcpy(pExtraCaptureAddr+i*nCapAlsaFrameBytes, pOutBufExtra+i*pChnData->mAudioHwOutputAlsaFrameBytes, nCapAlsaFrameBytes);
                memcpy(pExtraRefAddr+i*nRefAlsaFrameBytes, pOutBufExtra+i*pChnData->mAudioHwOutputAlsaFrameBytes+nCapAlsaFrameBytes, nRefAlsaFrameBytes);
            }
        }
        pFrameNode->mpAecFrame->bValid = TRUE;
    }
    list_move_tail(&pFrameNode->mList, &pChnData->mValidOutFrameList);
    return pFrameNode;
}

static ERRORTYPE ReleaseAudioCaptureFrame(AI_CHN_DATA_S *pChnData, AUDIO_FRAME_S *pAudioFrame)
{
    ERRORTYPE rc = SUCCESS;
    int ret;
    PCMBufferManager *pCapMgr = pChnData->mpCapMgr;
    AudioCaptureFrameInfo *pFrameNode = list_first_entry_or_null(&pChnData->mValidOutFrameList, AudioCaptureFrameInfo, mList);
    if(pFrameNode)
    {
        AUDIO_FRAME_S *pLocalFrame = NULL;
        if(0 == pChnData->mpAioAttr->mbBypassAec)
        {
            pLocalFrame = &pFrameNode->mFrame;
        }
        else
        {
            pLocalFrame = pFrameNode->mpCaptureFrame;
            if(NULL == pLocalFrame)
            {
                aloge("fatal error! aiChn[%d-%d] capture frame NULL!", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
            }
        }
        if (pLocalFrame->mId == pAudioFrame->mId && pLocalFrame->mpAddr == pAudioFrame->mpAddr)
        {
            //release to PCMBufferManager.
            if(0 == pChnData->mpAioAttr->mbBypassAec)
            {
                if(pFrameNode->nOrigDataLen + pFrameNode->nOrigDataExtraLen != pAudioFrame->mLen)
                {
                    aloge("fatal error! dataLen not match:[%d-%d-%d-%d]", pFrameNode->nOrigDataLen, pFrameNode->nOrigDataExtraLen,
                        pFrameNode->mFrame.mLen, pAudioFrame->mLen);
                }
                if(false == pFrameNode->mbMallocFlag)
                {
                    if(pFrameNode->mFrame.mpAddr != pFrameNode->pOrigData)
                    {
                        aloge("fatal error! pAddr not match[%p-%p]", pFrameNode->mFrame.mpAddr, pFrameNode->pOrigData);
                    }
                }
            }

            ret = pCapMgr->releaseData(pCapMgr, pFrameNode->pOrigData, pFrameNode->nOrigDataLen+pFrameNode->nOrigDataExtraLen);
            if(ret != 0)
            {
                aloge("fatal error! aiChn[%d-%d] release data[%p-%d-%p-%d] fail", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId,
                    pFrameNode->pOrigData, pFrameNode->nOrigDataLen, pFrameNode->pOrigDataExtra, pFrameNode->nOrigDataExtraLen);
            }
            //clear and move valid frame node to idle list.
            clearAudioCaptureFrameInfo(pFrameNode);
            list_move_tail(&pFrameNode->mList, &pChnData->mIdleOutFrameList);
        }
        else
        {
            aloge("fatal error! aiChn[%d-%d] local frame not match![%d-%d,%p-%p]", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId,
                pLocalFrame->mId, pAudioFrame->mId, pLocalFrame->mpAddr, pAudioFrame->mpAddr);
            rc = ERR_AI_NOBUF;
        }
    }
    else
    {
        aloge("fatal error! aiChn[%d-%d] has no valid out frame?", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
        rc = ERR_AI_NOBUF;
    }
    return rc;
}

/**
 * get frame, used in non-tunnel mode.
 *
 * @return SUCCESS.
 * @param hComponent ai component.
 * @param pAudioFrame store frame info, caller malloc.
 * @param nMilliSec 0:return immediately, <0:wait forever, >0:wait some time.
 */
static ERRORTYPE AIChannel_GetFrame(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT AUDIO_FRAME_S *pAudioFrame,
    PARAM_OUT AEC_FRAME_S *pAecFrame,
    PARAM_IN int nMilliSec)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    PCMBufferManager *pCapMgr = pChnData->mpCapMgr;
    ERRORTYPE eError = SUCCESS;
    int ret;

    if (COMP_StateIdle != pChnData->state && COMP_StateExecuting != pChnData->state)
    {
        alogw("call GetFrame() in wrong state[0x%x]", pChnData->state);
        return ERR_AI_NOT_PERM;
    }
    if (pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AENC] || pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AO])
    {
        aloge("fatal error! can't call GetFrame() in tunnel mode!");
        return ERR_AI_NOT_PERM;
    }

    int nFrameBytes = pChnData->mAiChnAttr.nFrameSize*pChnData->mAudioHwOutputAlsaFrameBytes;
    char *pOutBuf = NULL;
    int nSize = 0;
    char *pOutBufExtra = NULL;
    int nSizeExtra = 0;
    int64_t nPts = 0;
_TryToGetOutFrame:
    pthread_mutex_lock(&pChnData->mCapMgrLock);
    ret = pCapMgr->getData(pCapMgr, nFrameBytes, &pOutBuf, &nSize, &pOutBufExtra, &nSizeExtra, &nPts);
    if (0 == ret)
    {
        if(nFrameBytes != (nSize+nSizeExtra))
        {
            aloge("fatal error! PCM buffer size is not match:%d-%d-%d", nFrameBytes, nSize, nSizeExtra);
        }
        //1. set PCMBuf data to AudioCaptureFrameInfo list.
        AudioCaptureFrameInfo *pFrameNode = SetPCMDataToAudioCaptureFrameInfoList(pChnData, pOutBuf, nSize, pOutBufExtra, nSizeExtra, nPts);
        if(NULL == pFrameNode)
        {
            aloge("fatal error! malloc fail");
        }
        //2. fill pAudioFrame and pAecFrame.
        if(0 == pChnData->mpAioAttr->mbBypassAec) //pChnData->mpAioAttr->mbBypassAec is 0. NULL == pFrameNode->mpCaptureFrame
        {
            memcpy(pAudioFrame, &pFrameNode->mFrame, sizeof(AUDIO_FRAME_S));
        }
        else
        {
            memcpy(pAudioFrame, pFrameNode->mpCaptureFrame, sizeof(AUDIO_FRAME_S));
            if(pAecFrame)
            {
                memcpy(pAecFrame, pFrameNode->mpAecFrame, sizeof(AEC_FRAME_S));
            }
        }
        eError = SUCCESS;
        pthread_mutex_unlock(&pChnData->mCapMgrLock);
    }
    else
    {
        pthread_mutex_unlock(&pChnData->mCapMgrLock);
        if (nMilliSec == 0)
        {
            eError = ERR_AI_BUF_EMPTY;
        }
        else if (nMilliSec < 0)
        {
            //pChnData->mWaitingOutFrameFlag = TRUE;
            cdx_sem_down(&pChnData->mWaitOutFrameSem);
            //pChnData->mWaitingOutFrameFlag = FALSE;
            goto _TryToGetOutFrame;
        }
        else
        {
            //pChnData->mWaitingOutFrameFlag = TRUE;
            ret = cdx_sem_down_timedwait(&pChnData->mWaitOutFrameSem, nMilliSec);
            if (ETIMEDOUT == ret)
            {
                alogv("wait output frame timeout[%d]ms, ret[%d]", nMilliSec, ret);
                eError = ERR_AI_BUF_EMPTY;
                //pChnData->mWaitingOutFrameFlag = FALSE;
            }
            else if (0 == ret)
            {
                //pChnData->mWaitingOutFrameFlag = FALSE;
                goto _TryToGetOutFrame;
            }
            else
            {
                aloge("fatal error! AI pthread cond wait timeout ret[%d]", ret);
                eError = ERR_AI_BUF_EMPTY;
                //pChnData->mWaitingOutFrameFlag = FALSE;
            }
        }
    }

    return eError;
}

/**
 * release frame, used in non-tunnel mode.
 *
 * @return SUCCESS.
 * @param hComponent ai component.
 * @param pAudioFrame frame info.
 */
static ERRORTYPE AIChannel_ReleaseFrame(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN AUDIO_FRAME_S *pAudioFrame)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if (COMP_StateIdle != pChnData->state && COMP_StateExecuting != pChnData->state)
    {
        alogw("call ReleaseFrame in wrong state[0x%x]", pChnData->state);
        return ERR_AI_SYS_NOTREADY;
    }
    if (pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AENC] || pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AO])
    {
        aloge("fatal error! can't call ReleaseFrame in tunnel mode!");
        return ERR_AI_NOT_PERM;
    }

    bool bFound = false;
    pthread_mutex_lock(&pChnData->mCapMgrLock);
    if(SUCCESS == ReleaseAudioCaptureFrame(pChnData, pAudioFrame))
    {
        bFound = true;
    }
    pthread_mutex_unlock(&pChnData->mCapMgrLock);
    if (bFound)
    {
        if (pChnData->mWaitAllFrameReleaseFlag)
        {
            cdx_sem_up_unique(&pChnData->mAllFrameRelSem);
        }
        eError = SUCCESS;
    }
    else
    {
        aloge("fatal error! aiChn[%d-%d]AI frame[%p][%u] is not find, maybe reset channel before call this function?",
            pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, pAudioFrame->mpAddr, pAudioFrame->mLen);
        eError = ERR_AI_ILLEGAL_PARAM;
    }
    return eError;
}

static ERRORTYPE AIChannel_SetSaveFileInfo(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN AUDIO_SAVE_FILE_INFO_S *pFileInfo)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if (COMP_StateIdle != pChnData->state && COMP_StateExecuting != pChnData->state)
    {
        aloge("call SetSaveFileInfo in wrong state[0x%x]!", pChnData->state);
        return ERR_AI_NOT_PERM;
    }

    int nPathLen = strlen(pFileInfo->mFilePath) + strlen(pFileInfo->mFileName) + 1;
    pChnData->mpSaveFileFullPath = (char*)malloc(nPathLen);
    if (NULL == pChnData->mpSaveFileFullPath)
    {
        aloge("malloc %d fail! FilePath:[%s], FileName:[%s]", nPathLen, pFileInfo->mFilePath, pFileInfo->mFileName);
        return ERR_AI_NOMEM;
    }

    memset(pChnData->mpSaveFileFullPath, 0, nPathLen);
    strcpy(pChnData->mpSaveFileFullPath, pFileInfo->mFilePath);
    strcat(pChnData->mpSaveFileFullPath, pFileInfo->mFileName);
    pChnData->mFpSaveFile = fopen(pChnData->mpSaveFileFullPath, "wb+");
    if (pChnData->mFpSaveFile)
    {
        alogd("create file(%s) to save pcm file", pChnData->mpSaveFileFullPath);
        pChnData->mSaveFileFlag = TRUE;
        pChnData->mSaveFileSize = 0;
    }
    else
    {
        aloge("create file(%s) failed!", pChnData->mpSaveFileFullPath);
        pChnData->mSaveFileFlag = FALSE;
    }

    return SUCCESS;
}

static ERRORTYPE AIChannel_QueryFileStatus(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT AUDIO_SAVE_FILE_INFO_S *pFileInfo)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if (COMP_StateIdle != pChnData->state && COMP_StateExecuting != pChnData->state)
    {
        aloge("call SetSaveFileInfo in wrong state[0x%x]!", pChnData->state);
        return ERR_AI_NOT_PERM;
    }

    memset(pFileInfo, 0, sizeof(AUDIO_SAVE_FILE_INFO_S));
    if (pChnData->mSaveFileFlag)
    {
        pFileInfo->bCfg = pChnData->mSaveFileFlag;
        pFileInfo->mFileSize = pChnData->mSaveFileSize;
        const char *ptr = strrchr(pChnData->mpSaveFileFullPath, '/');
        int pathLen = ptr - pChnData->mpSaveFileFullPath;
        strncpy(pFileInfo->mFilePath, pChnData->mpSaveFileFullPath, pathLen);
        strcpy(pFileInfo->mFileName, ptr);
    }
    else
    {
        alogw("AI NOT in save file status!");
    }
    return SUCCESS;
}

static ERRORTYPE AIChannel_SetChnMute(
        PARAM_IN COMP_HANDLETYPE hComponent, 
        PARAM_IN BOOL bMute)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    pChnData->mbMute = bMute;
    return SUCCESS;
}

static ERRORTYPE AIChannel_GetChnMute(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT BOOL *pbMute)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    *pbMute = pChnData->mbMute;
    return SUCCESS;
}
static ERRORTYPE AIChannel_IgnoreData(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN BOOL bIgnore, BOOL bChangePts)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    pthread_mutex_lock(&pChnData->mIgnoreDataLock);
    pChnData->mbIgnore = bIgnore;
    pChnData->bIgnoreChangePts = bChangePts;
    pthread_mutex_unlock(&pChnData->mIgnoreDataLock);
    return SUCCESS;
}

static ERRORTYPE AIChannel_SendCommand(PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN COMP_COMMANDTYPE Cmd, PARAM_IN unsigned int nParam1, PARAM_IN void* pCmdData)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;

    //alogv("Command: %d", Cmd);
    if (NULL == pChnData) {
        eError = ERR_AI_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_BAIL;
    }

    if (Cmd == COMP_CommandMarkBuffer) {
        if (NULL == pCmdData) {
            eError = ERR_AI_ILLEGAL_PARAM;
            goto COMP_CONF_CMD_BAIL;
        }
    }

    if (pChnData->state == COMP_StateInvalid) {
        eError = ERR_AI_SYS_NOTREADY;
        goto COMP_CONF_CMD_BAIL;
    }

    switch (Cmd) {
        case COMP_CommandStateSet:
            eCmd = SetState;
            break;

        case COMP_CommandFlush:
            eCmd = Flush;
            break;

        case COMP_CommandPortDisable:
            eCmd = StopPort;
            break;

        case COMP_CommandPortEnable:
            eCmd = RestartPort;
            break;

        case COMP_CommandMarkBuffer:
            eCmd = MarkBuf;
            if (nParam1 > 0) {
                eError = ERR_AI_ILLEGAL_PARAM;
                goto COMP_CONF_CMD_BAIL;
            }
            break;

        default:
            eCmd = -1;
            break;
    }

    msg.command = eCmd;
    msg.para0 = nParam1;
    put_message(&pChnData->mCmdQueue, &msg);

COMP_CONF_CMD_BAIL:
    return eError;
}

static ERRORTYPE AIChannel_GetState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT COMP_STATETYPE* pState)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if (NULL == pChnData || NULL == pState) {
        eError = ERR_AI_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_BAIL;
    }

    *pState = pChnData->state;

COMP_CONF_CMD_BAIL:
    return eError;
}

static ERRORTYPE AIChannel_SetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN COMP_CALLBACKTYPE* pCallbacks, PARAM_IN void* pAppData)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if (NULL == pChnData || NULL == pCallbacks || NULL == pAppData) {
        aloge("pChnData=%p, pCallbacks=%p, pAppData=%p", pChnData, pCallbacks, pAppData);
        eError = ERR_AI_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_BAIL;
    }

    pChnData->pCallbacks = pCallbacks;
    pChnData->pAppData = pAppData;

COMP_CONF_CMD_BAIL:
    return eError;
}

static ERRORTYPE AIChannel_SetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    MM_COMPONENTTYPE *pAOTunnelComp = NULL;
    COMP_INTERNAL_TUNNELINFOTYPE* pAOTunnel = NULL;

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            COMP_PARAM_PORTDEFINITIONTYPE *port = (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure;
            int i;
            for(i = 0; i < AI_CHN_MAX_PORTS; i++) {
                if (port->nPortIndex == pChnData->sPortDef[i].nPortIndex) {
                    memcpy(&pChnData->sPortDef[i], port, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
                }
            }
            if (i == AI_CHN_MAX_PORTS) {
                eError = FAILURE;
            }
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier = (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure;
            int i;
            for(i=0; i<AI_CHN_MAX_PORTS; i++) {
                if(pChnData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
                    memcpy(&pChnData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
                    break;
                }
            }
            if(i == AI_CHN_MAX_PORTS) {
                eError = FAILURE;
            }
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            pChnData->mMppChnInfo = *(MPP_CHN_S*)pComponentConfigStructure;
            break;
        }
        case COMP_IndexVendorAIChnReleaseFrame:
        {
            AudioFrame *pAudioFrame = (AudioFrame*)pComponentConfigStructure;
            eError = AIChannel_ReleaseFrame(hComponent, pAudioFrame->pFrame);
            break;
        }
        case COMP_IndexVendorAIChnAttr:
        {
            AI_CHN_ATTR_S *pChnAttr = (AI_CHN_ATTR_S*)pComponentConfigStructure;
            if(pChnAttr)
            {
                pChnData->mAiChnAttr = *pChnAttr;
            }
            if(0 == pChnData->mAiChnAttr.nFrameSize)
            {
                pChnData->mAiChnAttr.nFrameSize = AUDIOCODEC_FRAMESIZE;
            }
            break;
        }
        case COMP_IndexVendorAIChnParameter:
        {
            pChnData->mParam = *(AI_CHN_PARAM_S*)pComponentConfigStructure;
            break;
        }
        case COMP_IndexVendorAIOVqeAttr:
        {//
            memcpy(&pChnData->mVqeCfg, (AI_VQE_CONFIG_S*)pComponentConfigStructure, sizeof(AI_VQE_CONFIG_S));
            break;
        }
        case COMP_IndexVendorAIOVqeEnable:
        {//
            pChnData->mUseVqeLib = TRUE;
            break;
        }
        case COMP_IndexVendorAIOVqeDisable:
        {
            pChnData->mUseVqeLib = FALSE;
            break;
        }
        case COMP_IndexVendorAIOReSmpEnable:
        {
            // todo
            break;
        }
        case COMP_IndexVendorAIOReSmpDisable:
        {
            // todo
            break;
        }
        case COMP_IndexVendorAISetSaveFileInfo:
        {
            AUDIO_SAVE_FILE_INFO_S *pSaveFileInfo = (AUDIO_SAVE_FILE_INFO_S*)pComponentConfigStructure;
            eError = AIChannel_SetSaveFileInfo(hComponent, pSaveFileInfo);
            break;
        }
        case COMP_IndexVendorAIChnMute:
        {
            eError = AIChannel_SetChnMute(hComponent, *(BOOL*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAIIgnoreData:
        {
            AIIgnoreDataParam *pIgnoreParam = (AIIgnoreDataParam*)pComponentConfigStructure;
            eError = AIChannel_IgnoreData(hComponent, pIgnoreParam->bIgnore, pIgnoreParam->bChangePts);
            break;
        }
        default:
            eError = FAILURE;
            break;
    }

    return eError;
}

static ERRORTYPE AIChannel_GetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_INOUT void* pComponentConfigStructure)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            COMP_PARAM_PORTDEFINITIONTYPE *port = (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure;
            int i;
            for(i = 0; i < AI_CHN_MAX_PORTS; i++) {
                if (port->nPortIndex == pChnData->sPortDef[i].nPortIndex) {
                    memcpy(port, &pChnData->sPortDef[i], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
                }
            }
            if (i == AI_CHN_MAX_PORTS) {
                eError = FAILURE;
            }
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier = (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure;
            int i;
            for(i=0; i<AI_CHN_MAX_PORTS; i++) {
                if(pChnData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
                    memcpy(pPortBufSupplier, &pChnData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
                    break;
                }
            }
            if(i == AI_CHN_MAX_PORTS) {
                eError = FAILURE;
            }
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            *(MPP_CHN_S*)pComponentConfigStructure = pChnData->mMppChnInfo;
            break;
        }
        case COMP_IndexVendorAIChnGetValidFrame:
        {
            AudioFrame *pAudioFrame = (AudioFrame *)pComponentConfigStructure;
            eError = AIChannel_GetFrame(hComponent, pAudioFrame->pFrame, pAudioFrame->pAecFrame, pAudioFrame->nMilliSec);
            break;
        }        
        case COMP_IndexVendorAIChnAttr:
        {
            *(AI_CHN_ATTR_S*)pComponentConfigStructure = pChnData->mAiChnAttr;
            break;
        }
        case COMP_IndexVendorAIChnParameter:
        {
            *(AI_CHN_PARAM_S*)pComponentConfigStructure = pChnData->mParam;
            break;
        }
        case COMP_IndexVendorAIOVqeAttr:
        {
            *(AI_VQE_CONFIG_S*)pComponentConfigStructure = pChnData->mVqeCfg;
            break;
        }
        case COMP_IndexVendorAIChnGetFreeFrame:
        {
//            if (pChnData->state != COMP_StateExecuting) {
//                eError = FAILURE;
//                break;
//            }

//            AUDIO_FRAME_S *tmp = pChnData->mpCapMgr->getFreeFrame(pChnData->mpCapMgr);
//            if (tmp != NULL) {
//                *(AUDIO_FRAME_S *)pComponentConfigStructure = *tmp;
//                eError = SUCCESS;
//            } else {
//                eError = FAILURE;
//            }
            break;
        }
        case COMP_IndexVendorAIQueryFileStatus:
        {
            AUDIO_SAVE_FILE_INFO_S *pSaveFileInfo = (AUDIO_SAVE_FILE_INFO_S*)pComponentConfigStructure;
            eError = AIChannel_QueryFileStatus(hComponent, pSaveFileInfo);
            break;
        }
        case COMP_IndexVendorAIChnMute:
        {
            eError = AIChannel_GetChnMute(hComponent, (BOOL*)pComponentConfigStructure);
            break;
        }
        default:
            eError = FAILURE;
            break;
    }

    return eError;
}

static ERRORTYPE AIChannel_ComponentTunnelRequest(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN unsigned int nPort,
    PARAM_IN COMP_HANDLETYPE hTunneledComp,
    PARAM_IN unsigned int nTunneledPort,
    PARAM_INOUT COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    COMP_PARAM_PORTDEFINITIONTYPE    *pPortDef;
    COMP_INTERNAL_TUNNELINFOTYPE     *pPortTunnelInfo;
    COMP_PARAM_BUFFERSUPPLIERTYPE    *pPortBufSupplier;
    int i;

    if (pChnData->state == COMP_StateExecuting)
    {
        alogw("Be careful! tunnel request may be some danger in StateExecuting");
    }
    else if(pChnData->state != COMP_StateIdle)
    {
        aloge("fatal error! tunnel request can't be in state[0x%x]", pChnData->state);
        eError = ERR_AI_INCORRECT_STATE_OPERATION;
        goto COMP_CMD_FAIL;
    }

    for (i = 0; i < AI_CHN_MAX_PORTS; ++i) {
        if (pChnData->sPortDef[i].nPortIndex == nPort) {
            pPortDef = &pChnData->sPortDef[i];
            break;
        }
    }
    if (i == AI_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AI_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    for (i = 0; i < AI_CHN_MAX_PORTS; ++i) {
        if (pChnData->sPortTunnelInfo[i].nPortIndex == nPort) {
            pPortTunnelInfo = &pChnData->sPortTunnelInfo[i];
            break;
        }
    }
    if (i == AI_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AI_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    for (i = 0; i < AI_CHN_MAX_PORTS; ++i) {
        if (pChnData->sPortBufSupplier[i].nPortIndex == nPort) {
            pPortBufSupplier = &pChnData->sPortBufSupplier[i];
            break;
        }
    }
    if (i == AI_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AI_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    pPortTunnelInfo->nPortIndex = nPort;
    pPortTunnelInfo->hTunnel = hTunneledComp;
    pPortTunnelInfo->nTunnelPortIndex = nTunneledPort;
    pPortTunnelInfo->eTunnelType = (pPortDef->eDomain == COMP_PortDomainOther) ? TUNNEL_TYPE_CLOCK : TUNNEL_TYPE_COMMON;
    if(NULL==hTunneledComp && 0==nTunneledPort && NULL==pTunnelSetup) {
        alogd("Cancel setup tunnel on port[%d]", nPort);
        eError = SUCCESS;
        goto COMP_CMD_FAIL;
    }
    if(pPortDef->eDir == COMP_DirOutput) {
        if (pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AENC] || pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AO]) {
            aloge("AI_Comp outport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        // judge which B: aenc or ao?
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        if (pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AENC].nPortIndex == nPort)
            pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AENC] = TRUE;
        else if (pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AO].nPortIndex == nPort)
            pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AO] = TRUE;
        else
            aloge("fatal error! ao bind with portIndex(%d, %d)", nPort, nTunneledPort);

    } else {
        //Check the data compatibility between the ports using one or more GetParameter calls.
        //B checks if its input port is compatible with the output port of component A.
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if(out_port_def.eDir != COMP_DirOutput) {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_AI_ILLEGAL_PARAM;
            goto COMP_CMD_FAIL;
        }
        pPortDef->format = out_port_def.format;

        //The component B informs component A about the final result of negotiation.
        if(pTunnelSetup->eSupplier != pPortBufSupplier->eBufferSupplier) {
            alogw("Low probability! use input portIndex[%d] buffer supplier[%d] as final!", nPort, pPortBufSupplier->eBufferSupplier);
            pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        }
        COMP_PARAM_BUFFERSUPPLIERTYPE oSupplier;
        oSupplier.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);
        oSupplier.eBufferSupplier = pTunnelSetup->eSupplier;
        ((MM_COMPONENTTYPE*)hTunneledComp)->SetConfig(hTunneledComp, COMP_IndexParamCompBufferSupplier, &oSupplier);

        
        if (pChnData->sPortDef[AI_CHN_PORT_INDEX_CAP_IN].nPortIndex == nPort)
        {
            pChnData->mInputPortTunnelFlag[AI_CHN_PORT_INDEX_CAP_IN] = TRUE;
        }
        else if (pChnData->sPortDef[AI_CHN_PORT_INDEX_AO_IN].nPortIndex == nPort)
        {
            pChnData->mInputPortTunnelFlag[AI_CHN_PORT_INDEX_AO_IN] = TRUE;
        }
    }

COMP_CMD_FAIL:
    return eError;
}

static ERRORTYPE AIChannel_EmptyThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    ERRORTYPE eError = SUCCESS;
    int ret;
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    //pthread_mutex_lock(&pChnData->mStateLock);
    if (pChnData->state != COMP_StateExecuting)
    {
        //aloge("send frame when AI channel state[0x%x] is not executing", pChnData->state);
        eError = ERR_AI_SYS_NOTREADY;
        goto EIXT;
    }

    if (pBuffer->nOutputPortIndex == pChnData->sPortDef[AI_CHN_PORT_INDEX_CAP_IN].nPortIndex)
    {
        AUDIO_FRAME_S *pSrcFrm = (AUDIO_FRAME_S*)pBuffer->pOutputPortPrivate;

        pthread_mutex_lock(&pChnData->mIgnoreDataLock);
        BOOL bIgnoreFlag = pChnData->mbIgnore;
        pthread_mutex_unlock(&pChnData->mIgnoreDataLock);
        if(bIgnoreFlag != TRUE)
        {
            AISendDataInfo stAudioInfo;
            memset(&stAudioInfo, 0, sizeof(AISendDataInfo));
            stAudioInfo.mLen = pSrcFrm->mLen;
            stAudioInfo.mbIgnore = 0;
            stAudioInfo.mPts = pSrcFrm->mTimeStamp;
            pChnData->pCallbacks->EventHandler(pChnData->hSelf, pChnData->pAppData, COMP_EventBufferPrefilled, 0, 0, (void*)&stAudioInfo);
            //don't modify pSrcFrm's content because it may be sent to many AIChannel_components.
            int64_t nFramePts = pSrcFrm->mTimeStamp;
            if(pChnData->bIgnoreChangePts)
            {
                if(nFramePts > pChnData->nIgnoreAudioDuration)
                {
                    nFramePts -= pChnData->nIgnoreAudioDuration;
                }
                else
                {
                    aloge("fatal error! aiChn[%d-%d] audio frame pts too small, check [%lld-%lld]us", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId,
                        nFramePts, pChnData->nIgnoreAudioDuration);
                    nFramePts = 0;
                }
            }
            pthread_mutex_lock(&pChnData->mCapMgrLock);
            ret = pChnData->mpCapMgr->writeData(pChnData->mpCapMgr, pSrcFrm->mpAddr, pSrcFrm->mLen, nFramePts, (bool)pChnData->mbMute);
            if(0 == ret)
            {
                if (pChnData->mWaitingCapDataFlag)
                {
                    pChnData->mWaitingCapDataFlag = FALSE;
                    message_t msg;
                    msg.command = AIChannel_CapDataAvailable;
                    put_message(&pChnData->mCmdQueue, &msg);
                }
                pthread_mutex_unlock(&pChnData->mCapMgrLock);
                cdx_sem_up_unique(&pChnData->mWaitOutFrameSem);
            }
            else
            {
                pthread_mutex_unlock(&pChnData->mCapMgrLock);
                //aloge("no node in FreeFrameList!");
                //PcmBufferManager *pBufMgr = pChnData->mpCapMgr;
                //aloge("PcmBufMgrFrmCntInfo >> FillingList:%d, ValidList:%d, UsingList:%d", pBufMgr->fillingFrmCnt(pBufMgr),
                //    pBufMgr->validFrmCnt(pBufMgr), pBufMgr->usingFrmCnt(pBufMgr));
                pChnData->mDiscardLen += pSrcFrm->mLen;
                pChnData->mDiscardNum++;
                if(pChnData->mDiscardNum%32 == 0)
                {
                    alogw("Be careful! aiChn[%d-%d] discard audio block num:%d, discard audio data len:%d", pChnData->mMppChnInfo.mDevId,
                        pChnData->mMppChnInfo.mChnId, pChnData->mDiscardNum, pChnData->mDiscardLen);
                }
                eError = ERR_AI_BUF_FULL;
                goto EIXT;
            }
        }
        else
        {
            //CompSendEvent(pChnData, COMP_EventBufferPrefilled, pFrm->mLen, 1);
            AISendDataInfo stAudioInfo;
            memset(&stAudioInfo, 0, sizeof(AISendDataInfo));
            stAudioInfo.mLen = pSrcFrm->mLen;
            stAudioInfo.mbIgnore = 1;
            stAudioInfo.mPts = pSrcFrm->mTimeStamp;
            pChnData->pCallbacks->EventHandler(pChnData->hSelf, pChnData->pAppData, COMP_EventBufferPrefilled, 0, 0, (void*)&stAudioInfo);
            pChnData->nIgnoreAudioBytes += pSrcFrm->mLen;
            int nSampleRate = map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pSrcFrm->mSamplerate);
            int nBitsPerSample = map_AUDIO_BIT_WIDTH_E_to_BitWidth(pSrcFrm->mBitwidth);
            int nChannels;
            if(0 == pChnData->mpAioAttr->mbBypassAec)
            {
                nChannels = judgeAudioChnNumBySoundMode(pSrcFrm->mSoundmode, NULL, NULL);
            }
            else
            {
                nChannels = pChnData->mpPcmCfg->chnCnt;
            }
            pChnData->nIgnoreAudioDuration = pChnData->nIgnoreAudioBytes*1000*1000/((nBitsPerSample/8) * nChannels * nSampleRate);
        }
    } 
    else
    {
        aloge("fatal error! inputPortIndex[%d] match nothing!", pBuffer->nOutputPortIndex);
    }

EIXT:
    //pthread_mutex_unlock(&pChnData->mStateLock);
    return eError;
}

/**
 * release frame, used in tunnel mode.
 * usualy use it when ao return frame to ai
 *
 * @return SUCCESS.
 * @param hComponent ai component.
 * @param pAudioFrame frame info.
 */
static ERRORTYPE AIChannel_FillThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    ERRORTYPE eError = SUCCESS;
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if (pBuffer->nOutputPortIndex == pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AENC].nPortIndex
        || pBuffer->nOutputPortIndex == pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AO].nPortIndex)
    {
        AUDIO_FRAME_S *pFrm = (AUDIO_FRAME_S*)pBuffer->pOutputPortPrivate;
        bool bFound = false;
        pthread_mutex_lock(&pChnData->mCapMgrLock);
        if(SUCCESS == ReleaseAudioCaptureFrame(pChnData, pFrm))
        {
            bFound = true;
        }
        pthread_mutex_unlock(&pChnData->mCapMgrLock);
        if (bFound)
        {
            if (pChnData->mWaitAllFrameReleaseFlag)
            {
                cdx_sem_up_unique(&pChnData->mAllFrameRelSem);
            }
            eError = SUCCESS;
        }
        else
        {
            aloge("fatal error! aiChn[%d-%d] return audio frame[%p-%d] is not find", pChnData->mMppChnInfo.mDevId,
                pChnData->mMppChnInfo.mChnId, pFrm->mpAddr, pFrm->mLen);
            eError = ERR_AI_ILLEGAL_PARAM;
        }
    }
    else
    {
        aloge("fatal error! frame portIndex[%d-%d] wrong", pBuffer->nOutputPortIndex, pBuffer->nInputPortIndex);
        eError = ERR_AI_ILLEGAL_PARAM;
    }
    return eError;
}

static ERRORTYPE AIChannel_ComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    CompInternalMsgType eCmd = Stop;
    message_t msg;
    int ret = 0;

    if(pChnData->mDiscardNum > 0)
    {
        alogw("Be careful! aiDev[%d]Chn[%d] discard audio block num:%d, discard audio data len:%d",
            pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, pChnData->mDiscardNum, pChnData->mDiscardLen);
    }
    msg.command = eCmd;
    put_message(&pChnData->mCmdQueue, &msg);
    alogv("wait AI channel component exit!...");

    // Wait for thread to exit so we can get the status into "error"
    pthread_join(pChnData->mThreadId, (void*) &eError);
    message_destroy(&pChnData->mCmdQueue);

    cdx_sem_deinit(&pChnData->mAllFrameRelSem);
    cdx_sem_deinit(&pChnData->mWaitOutFrameSem);
    //cdx_sem_deinit(&pChnData->mWaitGetAllOutFrameSem);

    pthread_mutex_destroy(&pChnData->mStateLock);
    pthread_mutex_destroy(&pChnData->mCapMgrLock);
    pthread_mutex_destroy(&pChnData->mIgnoreDataLock);

    if(!list_empty(&pChnData->mValidOutFrameList))
    {
        int cnt = 0;
        struct list_head *pList;list_for_each(pList, &pChnData->mValidOutFrameList) { cnt++; }
        aloge("fatal error! valid out frame list has %d nodes", cnt);
        list_splice_tail_init(&pChnData->mValidOutFrameList, &pChnData->mIdleOutFrameList);
    }
    if(!list_empty(&pChnData->mIdleOutFrameList))
    {
        int cnt = 0;
        AudioCaptureFrameInfo *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pChnData->mIdleOutFrameList, mList)
        {
            list_del(&pEntry->mList);
            destructAudioCaptureFrameInfo(pEntry);
            cnt++;
        }
        if(cnt != pChnData->mFrameNodeNum)
        {
            aloge("fatal error! audio frame number not match [%d!=%d]", cnt, pChnData->mFrameNodeNum);
        }
    }
    if (pChnData->mpCapMgr != NULL)
    {
        PCMBufferMgr_Destroy(pChnData->mpCapMgr);
        pChnData->mpCapMgr = NULL;
    }

#ifdef AI_SAVE_AUDIO_PCM
    fclose(pChnData->pcm_fp);
    alogd("AI_Comp pcm_file size: %d", pChnData->pcm_sz);
#endif
    if (pChnData->mSaveFileFlag)
    {
        fclose(pChnData->mFpSaveFile);
        free(pChnData->mpSaveFileFullPath);
    }


    free(pChnData);
    alogd("Ai component exited!");
    return eError;
}

#if defined(CFG_AUDIO_EFFECT_EQ) && CFG_AUDIO_EFFECT_EQ!=0
static int audioEQHandle(AI_CHN_DATA_S *pChnData, AUDIO_FRAME_S *pInFrm)
{
    short *proc_ptr;
    int sample_rate = pChnData->mpAioAttr->enSamplerate;

    if (AUDIO_BIT_WIDTH_16 != pInFrm->mBitwidth)
    {
        aloge("audio pcm format error! bitwidth=%d", pInFrm->mBitwidth);
        return FAILURE;
    }

    eq_prms_t prms[4] =
    {
        {4, 600, 1, BANDPASS_PEAK, sample_rate},
        {4, 1000, 1, BANDPASS_PEAK, sample_rate},
        {4, 2000, 2, BANDPASS_PEAK, sample_rate},
        {4, 4000, 1, BANDPASS_PEAK, sample_rate},
    };

    if (pChnData->equalizer == NULL)
    {
        pChnData->equalizer = eq_create(&prms[0], sizeof(prms)/sizeof(prms[0]));
        if (pChnData->equalizer == NULL)
        {
            aloge("eq create fail!");
            return FAILURE;
        }
    }

    proc_ptr = (short*)pInFrm->mpAddr;
    int left_item_cnt = sizeof(pInFrm->mLen);
    while (left_item_cnt>0)
    {
        int proc_sz = (left_item_cnt>64)? 64:left_item_cnt;
        eq_process(pChnData->equalizer, proc_ptr, proc_sz);
        proc_ptr += proc_sz;
        left_item_cnt -= proc_sz;
    }

    return SUCCESS;
}
#endif

static int audioEffectProc(AI_CHN_DATA_S *pChnData, AUDIO_FRAME_S *pInFrm)
{
    if (1)
    {
#ifdef CFG_AUDIO_EFFECT_EQ
        if (pChnData->mVqeCfg.bEqOpen)
        {
            audioEQHandle(pChnData, pInFrm);
        }
#endif
    }

    return 0;
}


ERRORTYPE AIChannel_ComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE *pComp;
    AI_CHN_DATA_S *pChnData;
    ERRORTYPE eError = SUCCESS;
    int err;
    int i;

    pComp = (MM_COMPONENTTYPE*)hComponent;

    // Create private data
    pChnData = (AI_CHN_DATA_S*)malloc(sizeof(AI_CHN_DATA_S));
    if (pChnData == NULL) {
        aloge("alloc AI_CHN_DATA_S error!");
        return FAILURE;
    }
    memset(pChnData, 0x0, sizeof(AI_CHN_DATA_S));

    pComp->pComponentPrivate = (void*)pChnData;
    pChnData->state = COMP_StateLoaded;
    pChnData->hSelf = hComponent;

    // Fill in function pointers
    pComp->SetCallbacks             = AIChannel_SetCallbacks;
    pComp->SendCommand              = AIChannel_SendCommand;
    pComp->GetConfig                = AIChannel_GetConfig;
    pComp->SetConfig                = AIChannel_SetConfig;
    pComp->GetState                 = AIChannel_GetState;
    pComp->ComponentTunnelRequest   = AIChannel_ComponentTunnelRequest;
    pComp->ComponentDeInit          = AIChannel_ComponentDeInit;
    pComp->EmptyThisBuffer          = AIChannel_EmptyThisBuffer;
    pComp->FillThisBuffer           = AIChannel_FillThisBuffer;

    pChnData->sPortParam.nPorts = 0;
    pChnData->sPortParam.nStartPortNumber = 0x0;

    pChnData->sPortDef[AI_CHN_PORT_INDEX_CAP_IN].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_CAP_IN].bEnabled = TRUE;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_CAP_IN].eDomain = COMP_PortDomainAudio;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_CAP_IN].eDir = COMP_DirInput;
    pChnData->sPortBufSupplier[AI_CHN_PORT_INDEX_CAP_IN].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortBufSupplier[AI_CHN_PORT_INDEX_CAP_IN].eBufferSupplier = COMP_BufferSupplyOutput;
    pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_CAP_IN].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_CAP_IN].eTunnelType = TUNNEL_TYPE_COMMON;
    pChnData->sPortParam.nPorts++;

    pChnData->sPortDef[AI_CHN_PORT_INDEX_AO_IN].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_AO_IN].bEnabled = TRUE;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_AO_IN].eDomain = COMP_PortDomainAudio;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_AO_IN].eDir = COMP_DirInput;
    pChnData->sPortBufSupplier[AI_CHN_PORT_INDEX_AO_IN].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortBufSupplier[AI_CHN_PORT_INDEX_AO_IN].eBufferSupplier = COMP_BufferSupplyOutput;
    pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_AO_IN].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_AO_IN].eTunnelType = TUNNEL_TYPE_COMMON;
    pChnData->sPortParam.nPorts++;

    pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AENC].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AENC].bEnabled = TRUE;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AENC].eDomain = COMP_PortDomainAudio;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AENC].eDir = COMP_DirOutput;
    pChnData->sPortBufSupplier[AI_CHN_PORT_INDEX_OUT_AENC].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortBufSupplier[AI_CHN_PORT_INDEX_OUT_AENC].eBufferSupplier = COMP_BufferSupplyOutput;
    pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_OUT_AENC].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_OUT_AENC].eTunnelType = TUNNEL_TYPE_COMMON;
    pChnData->sPortParam.nPorts++;

    pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AO].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AO].bEnabled = TRUE;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AO].eDomain = COMP_PortDomainAudio;
    pChnData->sPortDef[AI_CHN_PORT_INDEX_OUT_AO].eDir = COMP_DirOutput;
    pChnData->sPortBufSupplier[AI_CHN_PORT_INDEX_OUT_AO].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortBufSupplier[AI_CHN_PORT_INDEX_OUT_AO].eBufferSupplier = COMP_BufferSupplyOutput;
    pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_OUT_AO].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_OUT_AO].eTunnelType = TUNNEL_TYPE_COMMON;
    pChnData->sPortParam.nPorts++;

    if (audioHw_AI_GetPcmConfig(pChnData->mMppChnInfo.mDevId, &pChnData->mpPcmCfg) != SUCCESS) {
        aloge("audioHw_AI_GetPcmConfig error!");
        eError = FAILURE;
        goto ERR_EXIT0;
    }
    if (audioHw_AI_GetAIOAttr(pChnData->mMppChnInfo.mDevId, &pChnData->mpAioAttr) != SUCCESS) {
        aloge("audioHw_AI_GetAIOAttr error!");
        eError = FAILURE;
        goto ERR_EXIT0;
    }

    int nAudioHwOutputChnNum = 0;
    if(0 == pChnData->mpAioAttr->mbBypassAec)
    {
        nAudioHwOutputChnNum = judgeAudioChnNumBySoundMode(pChnData->mpAioAttr->enSoundmode, NULL, NULL);
    }
    else
    {
        nAudioHwOutputChnNum = pChnData->mpPcmCfg->chnCnt;
    }
    int nSampleBitWidth = map_AUDIO_BIT_WIDTH_E_to_BitWidth(pChnData->mpAioAttr->enBitwidth);
    int nSampleRate = map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pChnData->mpAioAttr->enSamplerate);
    pChnData->mAudioHwOutputAlsaFrameBytes = nSampleBitWidth/8*nAudioHwOutputChnNum;
    int nAudioHwOutputFrameBytes = pChnData->mpPcmCfg->chunkSize*pChnData->mAudioHwOutputAlsaFrameBytes;
    pChnData->mpCapMgr = PCMBufferMgr_Create(AI_CHN_MAX_CACHE_FRAME, nAudioHwOutputFrameBytes, pChnData->mAudioHwOutputAlsaFrameBytes, nSampleRate);
    if (pChnData->mpCapMgr == NULL)
    {
        aloge("BufMgrCreate error!");
        eError = FAILURE;
        goto ERR_EXIT0;
    }
    INIT_LIST_HEAD(&pChnData->mIdleOutFrameList);
    INIT_LIST_HEAD(&pChnData->mValidOutFrameList);
    for(i=0; i<AI_CHN_MAX_CACHE_FRAME; i++)
    {
        AudioCaptureFrameInfo *pNode = constructAudioCaptureFrameInfo(i);
        list_add_tail(&pNode->mList, &pChnData->mIdleOutFrameList);
        pChnData->mFrameNodeNum++;
    }
    err = pthread_mutex_init(&pChnData->mIgnoreDataLock, NULL);
    if(err != 0)
    {
        aloge("pthread mutex init fail!");
        eError = FAILURE;
        goto ERR_EXIT3_3;
    }
    err = pthread_mutex_init(&pChnData->mStateLock, NULL);
    if(err != 0)
    {
        aloge("pthread mutex init fail!");
        eError = FAILURE;
        goto ERR_EXIT3;
    }
    err = pthread_mutex_init(&pChnData->mCapMgrLock, NULL);
    if(err != 0)
    {
        aloge("fatal error! pthread mutex init fail!");
    }
 
    if (message_create(&pChnData->mCmdQueue) < 0){
        aloge("message_create error!");
        eError = FAILURE;
        goto ERR_EXIT4;
    }
    err = cdx_sem_init(&pChnData->mAllFrameRelSem, 0);
    if (err != 0) {
        aloge("cdx_sem_init AllFrameRelSem error!");
        goto ERR_EXIT5;
    }

    err = cdx_sem_init(&pChnData->mWaitOutFrameSem, 0);
    if (err != 0) {
        aloge("cdx_sem_init mWaitOutFrameSem error!");
        goto ERR_EXIT6;
    }

//    err = cdx_sem_init(&pChnData->mWaitGetAllOutFrameSem, 0);
//    if (err != 0) {
//        aloge("cdx_sem_init mWaitGetAllOutFrameSem error!");
//        goto ERR_EXIT7;
//    }

#ifdef AI_SAVE_AUDIO_PCM
    pChnData->pcm_fp = fopen("/mnt/extsd/ai_pcm", "wb");
#endif

    err = pthread_create(&pChnData->mThreadId, NULL, AIChannel_ComponentThread, pChnData);
    if (err) {
        aloge("pthread_create error!");
        eError = FAILURE;
        goto ERR_EXIT8;
    }
    alogd("create AiChannel threadId:0x%lx", pChnData->mThreadId);
    return SUCCESS;


ERR_EXIT8:
    //cdx_sem_deinit(&pChnData->mWaitGetAllOutFrameSem);
ERR_EXIT7:
    cdx_sem_deinit(&pChnData->mWaitOutFrameSem);
ERR_EXIT6:
    cdx_sem_deinit(&pChnData->mAllFrameRelSem);
ERR_EXIT5:
    message_destroy(&pChnData->mCmdQueue);
ERR_EXIT4:
    pthread_mutex_destroy(&pChnData->mStateLock);
ERR_EXIT3:
    pthread_mutex_destroy(&pChnData->mIgnoreDataLock);
ERR_EXIT3_3:
ERR_EXIT2:
ERR_EXIT1:
    if(!list_empty(&pChnData->mIdleOutFrameList))
    {
        AudioCaptureFrameInfo *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pChnData->mIdleOutFrameList, mList)
        {
            list_del(&pEntry->mList);
            destructAudioCaptureFrameInfo(pEntry);
        }
    }
    PCMBufferMgr_Destroy(pChnData->mpCapMgr);
ERR_EXIT0:
    free(pChnData);
    return eError;
}

static void *AIChannel_ComponentThread(void *pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    message_t cmd_msg;
    AI_CHN_DATA_S *pChnData = (AI_CHN_DATA_S*)pThreadData;
    ERRORTYPE eError;
    BOOL bIgnoreFlag;
    alogv("AI channel ComponentThread start run...");
    sprintf(pChnData->mThreadName, "AIChn[%d-%d]", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
    prctl(PR_SET_NAME, (unsigned long)pChnData->mThreadName, 0, 0, 0);

    while (1) {
PROCESS_MESSAGE:
        if (get_message(&pChnData->mCmdQueue, &cmd_msg) == 0)
        {
            cmd = cmd_msg.command;
            cmddata = (unsigned int)cmd_msg.para0;
            if (cmd == SetState)
            {
                //alogv("cmd=SetState, cmddata=%d", cmddata);
                //pthread_mutex_lock(&pChnData->mStateLock);
                if (pChnData->state == (COMP_STATETYPE) (cmddata))
                {
                    CompSendEvent(pChnData, COMP_EventError, ERR_AI_SAMESTATE, 0);
                    CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                }
                else
                {
                    switch ((COMP_STATETYPE)cmddata)
                    {
                        case COMP_StateInvalid:
                        {
                            pChnData->state = COMP_StateInvalid;
                            CompSendEvent(pChnData, COMP_EventError, ERR_AI_INVALIDSTATE, 0);
                            CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            break;
                        }
                        case COMP_StateLoaded:
                        {
                            if (pChnData->state != COMP_StateIdle)
                            {
                                CompSendEvent(pChnData, COMP_EventError, ERR_AI_INCORRECT_STATE_TRANSITION, 0);
                            }
                            alogv("AI_Comp: idle->loaded. StateLoaded begin...");
                            int ret;
                            pChnData->mWaitAllFrameReleaseFlag = 1;
                            while (1)
                            {
                                pthread_mutex_lock(&pChnData->mCapMgrLock);
                                if(list_empty(&pChnData->mValidOutFrameList))
                                {
                                    int nPrefetchSize = pChnData->mpCapMgr->getPrefetchSize(pChnData->mpCapMgr);
                                    if(nPrefetchSize != 0)
                                    {
                                        aloge("fatal error! aiChn[%d-%d] prefetchSize[%d] not 0.", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, nPrefetchSize);
                                    }
                                    pthread_mutex_unlock(&pChnData->mCapMgrLock);
                                    break;
                                }
                                else
                                {
                                    pthread_mutex_unlock(&pChnData->mCapMgrLock);
                                    ret = cdx_sem_down_timedwait(&pChnData->mAllFrameRelSem, 1000);
                                    if(ret != 0)
                                    {
                                        alogw("AIChn[%d-%d] wait output Frames fail:0x%x", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, ret);
                                    }
                                }
                            }
                            pChnData->mWaitAllFrameReleaseFlag = 0;
                            pChnData->state = COMP_StateLoaded;
                            CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            if (pChnData->state == COMP_StateLoaded)
                            {
                                alogv("AI_Comp: loaded->idle ...");
                                pChnData->state = COMP_StateIdle;
                            }
                            else if (pChnData->state == COMP_StatePause || pChnData->state == COMP_StateExecuting)
                            {
                                alogv("AI_Comp: pause/executing[0x%x]->idle ...", pChnData->state);
                                pChnData->state = COMP_StateIdle;
                            }
                            else
                            {
                                CompSendEvent(pChnData, COMP_EventError, ERR_AI_INCORRECT_STATE_TRANSITION, 0);
                            }
                            pChnData->nIgnoreAudioBytes = 0;
                            pChnData->nIgnoreAudioDuration = 0;
                            CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            if (pChnData->state == COMP_StateIdle || pChnData->state == COMP_StatePause)
                            {
                                alogv("AI_Comp: idle/pause[0x%x]->executing ...", pChnData->state);
                                pChnData->state = COMP_StateExecuting;
                            }
                            else
                            {
                                CompSendEvent(pChnData, COMP_EventError, ERR_AI_INCORRECT_STATE_TRANSITION, 0);
                            }
                            CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            break;
                        }
                        case COMP_StatePause:
                        {
                            if (pChnData->state == COMP_StateIdle || pChnData->state == COMP_StateExecuting)
                            {
                                pChnData->state = COMP_StatePause;
                            }
                            else
                            {
                                CompSendEvent(pChnData, COMP_EventError, ERR_AI_INCORRECT_STATE_TRANSITION, 0);
                            }
                            CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            break;
                        }
                        default:
                            break;
                    }
                }
                //pthread_mutex_unlock(&pChnData->mStateLock);
            }
            else if (cmd == AIChannel_CapDataAvailable)
            {
                pChnData->mWaitingCapDataFlag = FALSE;
            }
            else if (cmd == AIChannel_PlayDataAvailable)
            {
            }
            else if (cmd == StopPort)
            {
            }
            else if (cmd == Stop)
            {
                goto EXIT;
            }
            goto PROCESS_MESSAGE;
        }

        if (pChnData->state == COMP_StateExecuting)
        {
            if(pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AENC] || pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AO])
            {
                PCMBufferManager *pCapMgr = pChnData->mpCapMgr;
                int nFrameBytes = pChnData->mAiChnAttr.nFrameSize*pChnData->mAudioHwOutputAlsaFrameBytes;
                char *pOutBuf = NULL;
                int nSize = 0;
                char *pOutBufExtra = NULL;
                int nSizeExtra = 0;
                int64_t nPts = 0;
                pthread_mutex_lock(&pChnData->mCapMgrLock);
                int ret = pCapMgr->getData(pCapMgr, nFrameBytes, &pOutBuf, &nSize, &pOutBufExtra, &nSizeExtra, &nPts);
                if(ret != 0)
                {
                    pChnData->mWaitingCapDataFlag = TRUE;
                    pthread_mutex_unlock(&pChnData->mCapMgrLock);
                    TMessage_WaitQueueNotEmpty(&pChnData->mCmdQueue, 0);
                    goto PROCESS_MESSAGE;
                }
                else
                {
                    //1. set PCMBuf data to AudioCaptureFrameInfo list.
                    AudioCaptureFrameInfo *pFrameNode = SetPCMDataToAudioCaptureFrameInfoList(pChnData, pOutBuf, nSize, pOutBufExtra, nSizeExtra, nPts);
                    pthread_mutex_unlock(&pChnData->mCapMgrLock);
                    if(NULL == pFrameNode)
                    {
                        aloge("fatal error! malloc fail");
                    }
                    //2. send frame out.
                    COMP_INTERNAL_TUNNELINFOTYPE *pPortTunnelInfo = NULL;
                    if(pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AENC])
                    {
                        pPortTunnelInfo = &pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_OUT_AENC];
                    }
                    else if(pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AO])
                    {
                        pPortTunnelInfo = &pChnData->sPortTunnelInfo[AI_CHN_PORT_INDEX_OUT_AO];
                    }
                    MM_COMPONENTTYPE *pOutTunnelComp = (MM_COMPONENTTYPE*)pPortTunnelInfo->hTunnel;
                    AUDIO_FRAME_S *pFrm = &pFrameNode->mFrame;
                  #ifdef AI_SAVE_AUDIO_PCM
                    fwrite(pFrm->mpAddr, 1, pFrm->mLen, pChnData->pcm_fp);
                    pChnData->pcm_sz += pFrm->mLen;
                  #endif
                    if (pChnData->mSaveFileFlag)
                    {
                        fwrite(pFrm->mpAddr, 1, pFrm->mLen, pChnData->mFpSaveFile);
                        pChnData->mSaveFileSize += pFrm->mLen;
                    }
                    if (pChnData->mUseVqeLib)
                    {
                        audioEffectProc(pChnData, pFrm);
                    }
                    COMP_BUFFERHEADERTYPE obh;
                    obh.nOutputPortIndex = pPortTunnelInfo->nPortIndex;
                    obh.nInputPortIndex = pPortTunnelInfo->nTunnelPortIndex;
                    obh.pOutputPortPrivate = pFrm;
                    eError = COMP_EmptyThisBuffer(pOutTunnelComp, &obh);
                    if(SUCCESS != eError)
                    {
                        alogw("Be careful! aiChn[%d-%d]:[AI->%s] send pcm failed!", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId,
                            pChnData->mOutputPortTunnelFlag[AI_OUTPORT_SUFFIX_AENC]?"AEnc":"AO");
                    }
                }
            }
            else
            {
                //do nothing in non-tunnel mode.
                TMessage_WaitQueueNotEmpty(&pChnData->mCmdQueue, 0);
            }
        }
        else
        {
            alogv("AI_Comp not StateExecuting");
            TMessage_WaitQueueNotEmpty(&pChnData->mCmdQueue, 0);
        }
    }

EXIT:
    alogv("AI channel ComponentThread stopped!");
    return NULL;
}

