/** @file
  File Name     : BufferManager.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/20
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "BufferManager"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <memory.h>

#include <cdx_list.h>
#include <BufferManager.h>


int initPCMDataNode(PCMDataNode *pNode)
{
    pNode->pData = NULL;
    pNode->nDataLen = 0;
    pNode->pDataExtra = NULL;
    pNode->nDataExtraLen = 0;
    pNode->nPts = 0;
    return 0;
}

/**
  write data to buffer.

  @return
    0: success
    -1: fail. If fail, write none data.
*/
static int bufferMgr_WriteData(struct PCMBufferManager *pMgr, char *pInBuf, int inSize, int64_t nPts, bool bMute)
{
    pthread_mutex_lock(&pMgr->mLock);
    if (inSize > pMgr->mFreeSize)
    {
        pthread_mutex_unlock(&pMgr->mLock);
        alogw("Be careful! Not enough buffer to write! freeSize[%d]<inSize[%d]", pMgr->mFreeSize, inSize);
        return -1;
    }
    if(inSize != pMgr->mFrameBytes)
    {
        aloge("fatal error! audioHw frameBytes not match[%d!=%d]!", inSize, pMgr->mFrameBytes);
    }
    if(list_empty(&pMgr->mIdlePCMDataList))
    {
        PCMDataNode *pNode = (PCMDataNode*)malloc(sizeof(PCMDataNode));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
        }
        list_add_tail(&pNode->mList, &pMgr->mIdlePCMDataList);
        pMgr->mNodeNum++;
        if(0 == pMgr->mNodeNum % 100)
        {
            aloge("fatal error! why need so many nodes[%d]?", pMgr->mNodeNum);
        }
    }
    PCMDataNode *pFirstNode = list_first_entry(&pMgr->mIdlePCMDataList, PCMDataNode, mList);
    initPCMDataNode(pFirstNode);
    pFirstNode->nPts = nPts;
    int endSize = pMgr->mpStart + pMgr->mTotalSize - pMgr->mpWrite;
    if (endSize <= inSize)
    {
        if(!bMute)
        {
            memcpy(pMgr->mpWrite, pInBuf, endSize);
        }
        else
        {
            memset(pMgr->mpWrite, 0, endSize);
        }
        pFirstNode->pData = pMgr->mpWrite;
        pFirstNode->nDataLen = endSize;
        if(inSize - endSize > 0)
        {
            aloge("fatal error! mTotalSize[%d] must be an integer multiple of frameBytes[%d-%d]", pMgr->mTotalSize, pMgr->mFrameBytes, inSize);
            if(!bMute)
            {
                memcpy(pMgr->mpStart, pInBuf + endSize, inSize - endSize);
            }
            else
            {
                memset(pMgr->mpStart, 0, inSize - endSize);
            }
            pFirstNode->pDataExtra = pMgr->mpStart;
            pFirstNode->nDataExtraLen = inSize - endSize;
        }
        pMgr->mpWrite = pMgr->mpStart + inSize - endSize;
    }
    else
    {
        if(!bMute)
        {
            memcpy(pMgr->mpWrite, pInBuf, inSize);
        }
        else
        {
            memset(pMgr->mpWrite, 0, inSize);
        }
        pFirstNode->pData = pMgr->mpWrite;
        pFirstNode->nDataLen = inSize;
        pMgr->mpWrite += inSize;
    }
    pMgr->mFreeSize -= inSize;
    pMgr->mDataSize += inSize;
    alogv("Data write: inSize:%d, [%p-%p-%p-%p,%d-%d-%d-%d]", inSize, pMgr->mpStart, pMgr->mpRead, pMgr->mpWrite, pMgr->mpPrefetch,
        pMgr->mTotalSize, pMgr->mDataSize, pMgr->mPrefetchSize, pMgr->mFreeSize);
    list_move_tail(&pFirstNode->mList, &pMgr->mValidPCMDataList);
    pthread_mutex_unlock(&pMgr->mLock);

    return 0;
}

/**
  get data from buffer, but data need to be release back in future.

  @return
    0: success
    -1: fail because not enough data.
*/
static int bufferMgr_GetData(struct PCMBufferManager *pMgr, int reqSize, char **ppOutBuf, int *pSize, char **ppOutBufExtra, int *pSizeExtra, int64_t *pPts)
{
    pthread_mutex_lock(&pMgr->mLock);
    int nLeftDataSize = pMgr->mDataSize - pMgr->mPrefetchSize;
    if (nLeftDataSize < reqSize)
    {
        pthread_mutex_unlock(&pMgr->mLock);
        alogv("Not enough buffer to read! leftDataSize[%d=%d-%d]<reqSize[%d]", nLeftDataSize, pMgr->mDataSize, pMgr->mPrefetchSize, reqSize);
        return -1;
    }
    int endSize = pMgr->mpStart + pMgr->mTotalSize - pMgr->mpPrefetch;
    if (endSize <= reqSize)
    {
        *ppOutBuf = pMgr->mpPrefetch;
        *pSize = endSize;
        if(reqSize > endSize)
        {
            *ppOutBufExtra = pMgr->mpStart;
            *pSizeExtra = reqSize - endSize;
        }
        else
        {
            *ppOutBufExtra = NULL;
            *pSizeExtra = 0;
        }
        pMgr->mpPrefetch = pMgr->mpStart + (reqSize - endSize);
    }
    else
    {
        *ppOutBuf = pMgr->mpPrefetch;
        *ppOutBufExtra = NULL;
        *pSize = reqSize;
        *pSizeExtra = 0;
        pMgr->mpPrefetch += reqSize;
    }
    pMgr->mPrefetchSize += reqSize;

    //calculate pts
    char *pData = *ppOutBuf;
    if(!list_empty(&pMgr->mValidPCMDataList))
    {
        PCMDataNode *pEntry, *pTmp;
        bool bInDuration = false;
        int nItlBytes = 0;
        list_for_each_entry_safe(pEntry, pTmp, &pMgr->mValidPCMDataList, mList)
        {
            //1. judge if pData is in duration of pEntry
            if(NULL == pEntry->pDataExtra)
            {
                if(pData >= pEntry->pData && pData < pEntry->pData + pEntry->nDataLen)
                {
                    bInDuration = true;
                    nItlBytes = pData - pEntry->pData;
                    break;
                }
                if(pData < pEntry->pData && (pEntry->pData + pEntry->nDataLen < pMgr->mpStart + pMgr->mTotalSize))
                {
                    //aloge("fatal error! check code![%p-%p-%d-%p-%d]", pData, pEntry->pData, pEntry->nDataLen, pMgr->mpStart, pMgr->mTotalSize);
                }
            }
            else
            {
                if(pData >= pEntry->pData)
                {
                    bInDuration = true;
                    nItlBytes = pData - pEntry->pData;
                    break;
                }
                else
                {
                    if(pData < pEntry->pDataExtra)
                    {
                        aloge("fatal error! check code![%p-%p-%p]", pData, pEntry->pData, pEntry->pDataExtra);
                    }
                    if(pData < pEntry->pDataExtra + pEntry->nDataExtraLen)
                    {
                        bInDuration = true;
                        nItlBytes = pEntry->nDataLen + (pData - pEntry->pDataExtra);
                        break;
                    }
                }
            }
            list_move_tail(&pEntry->mList, &pMgr->mIdlePCMDataList);
        }
        if(bInDuration)
        {
            int64_t nItl = (int64_t)(nItlBytes*1000*1000)/pMgr->mAlsaFrameBytes/pMgr->mSampleRate;
            *pPts = pEntry->nPts + nItl;
        }
        else
        {
            aloge("fatal error! pData[%p] not find duration?", pData);
        }
    }
    else
    {
        aloge("fatal error! valid pcm data list is empty! check code![%p-%p,%d-%d-%d]", pMgr->mpStart, pData, pMgr->mDataSize,
            pMgr->mPrefetchSize, pMgr->mFreeSize);
    }
    alogv("Data get:%p-%p-%d-%d-%lld", *ppOutBuf, *ppOutBufExtra, *pSize, *pSizeExtra, *pPts);
    pthread_mutex_unlock(&pMgr->mLock);
    return 0;
}

/**
  release data to buffer.

  @return
    0:success
    -1:fail
*/
static int bufferMgr_ReleaseData(struct PCMBufferManager *pMgr, char *pOutBuf, int relSize)
{
    int ret = 0;
    pthread_mutex_lock(&pMgr->mLock);
    if(pMgr->mpRead != pOutBuf)
    {
        aloge("fatal error! why release buf not match[%p!=%p]?", pMgr->mpRead, pOutBuf);
        ret = -1;
    }
    if(relSize > pMgr->mPrefetchSize)
    {
        aloge("fatal error! why release size too large[%d>%d]?", relSize, pMgr->mPrefetchSize);
        ret = -1;
    }
    if(ret != 0)
    {
        pthread_mutex_unlock(&pMgr->mLock);
        return ret;
    }
    int endSize = pMgr->mpStart + pMgr->mTotalSize - pMgr->mpRead;
    if(endSize <= relSize)
    {
        pMgr->mpRead = pMgr->mpStart + (relSize - endSize);
        if(pMgr->mpRead > pMgr->mpPrefetch)
        {
            aloge("fatal error! check code![%p-%p-%d-%d-%d-%d]", pMgr->mpRead, pMgr->mpPrefetch, pMgr->mDataSize, pMgr->mPrefetchSize, pMgr->mFreeSize, relSize);
        }
    }
    else
    {
        pMgr->mpRead += relSize;
    }
    pMgr->mDataSize -= relSize;
    pMgr->mPrefetchSize -= relSize;
    pMgr->mFreeSize += relSize;
    alogv("Data rel:%p-%d, %d-%d-%d", pOutBuf, relSize, pMgr->mDataSize, pMgr->mPrefetchSize, pMgr->mFreeSize);
    pthread_mutex_unlock(&pMgr->mLock);
    return ret;
}

static int bufferMgr_GetFreeSize(PCMBufferManager *pMgr)
{
    int nSize;
    pthread_mutex_lock(&pMgr->mLock);
    nSize = pMgr->mFreeSize;
    pthread_mutex_unlock(&pMgr->mLock);
    return nSize;
}

static int bufferMgr_GetPrefetchSize(PCMBufferManager *pMgr)
{
    int nSize;
    pthread_mutex_lock(&pMgr->mLock);
    nSize = pMgr->mPrefetchSize;
    pthread_mutex_unlock(&pMgr->mLock);
    return nSize;
}

void PCMBufferMgr_Destroy(PCMBufferManager *pMgr)
{
    if (pMgr != NULL)
    {
        pthread_mutex_lock(&pMgr->mLock);
        if(pMgr->mPrefetchSize != 0)
        {
            aloge("fatal error! pcm buffer manager still has prefetchSize[%d], check code!", pMgr->mPrefetchSize);
        }
        if(pMgr->mDataSize != 0)
        {
            alogd("pcm buffer manager still has dataSize[%d] to send.", pMgr->mDataSize);
        }
        int nIdleNodeNum = 0;
        if(!list_empty(&pMgr->mIdlePCMDataList))
        {
            PCMDataNode *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pMgr->mIdlePCMDataList, mList)
            {
                list_del(&pEntry->mList);
                free(pEntry);
                nIdleNodeNum++;
            }
        }
        int nValidNodeNum = 0;
        if(!list_empty(&pMgr->mValidPCMDataList))
        {
            PCMDataNode *pEntry, *pTmp;
            list_for_each_entry_safe(pEntry, pTmp, &pMgr->mValidPCMDataList, mList)
            {
                list_del(&pEntry->mList);
                free(pEntry);
                nValidNodeNum++;
            }
        }
        if(nIdleNodeNum + nValidNodeNum != pMgr->mNodeNum)
        {
            aloge("fatal error! check pcm buffer manager node num:[%d-%d-%d]", nIdleNodeNum, nValidNodeNum, pMgr->mNodeNum);
        }
        free(pMgr->mpStart);
        pthread_mutex_unlock(&pMgr->mLock);
        pthread_mutex_destroy(&pMgr->mLock);
        free(pMgr);
    }
}

/**
  create PCMBufferManager.

  @param nFrmNum
    buffer size is based on frame number and frame bytes.
  @param nFrameBytes.
    buffer size is based on frame number and frame bytes. frame here is notion of audio codec, 1024 in common.
  @param nChnNum,nSampleBitWidth,nSampleRate
    audioHw output params.
*/
PCMBufferManager *PCMBufferMgr_Create(int nFrmNum, int nFrameBytes, int nAlsaFrameBytes, int nSampleRate)
{
    int ret;
    PCMBufferManager *pMgr = (PCMBufferManager*)malloc(sizeof(PCMBufferManager));
    if (pMgr == NULL)
    {
        aloge("fatal error! malloc fail.");
        return NULL;
    }
    pMgr->mAlsaFrameBytes = nAlsaFrameBytes;
    pMgr->mSampleRate = nSampleRate;
    pMgr->mFrameBytes = nFrameBytes;

    // frameBytes is based on period size, its sample number is period size.
    int nSize = nFrmNum*nFrameBytes;
    pMgr->mpStart = (char*)malloc(nSize);
    if (pMgr->mpStart == NULL)
    {
        aloge("buffer manager alloc size %d error!", nSize);
        free(pMgr);
        return NULL;
    }
    pMgr->mpRead = pMgr->mpStart;
    pMgr->mpWrite = pMgr->mpStart;
    pMgr->mpPrefetch = pMgr->mpStart;
    pMgr->mTotalSize = nSize;
    pMgr->mDataSize = 0;
    pMgr->mPrefetchSize = 0;
    pMgr->mFreeSize = nSize;
    ret = pthread_mutex_init(&pMgr->mLock, NULL);
    if(ret != 0)
    {

        aloge("fatal error! pthread mutex init error!");
    }

    INIT_LIST_HEAD(&pMgr->mIdlePCMDataList);
    INIT_LIST_HEAD(&pMgr->mValidPCMDataList);
    for (int i = 0; i < nFrmNum; i++)
    {
        PCMDataNode *pNode = (PCMDataNode*)malloc(sizeof(PCMDataNode));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
        }
        memset(pNode, 0, sizeof(PCMDataNode));
        list_add_tail(&pNode->mList, &pMgr->mIdlePCMDataList);
    }
    pMgr->mNodeNum = nFrmNum;

    pMgr->writeData = bufferMgr_WriteData;
    pMgr->getData = bufferMgr_GetData;
    pMgr->releaseData = bufferMgr_ReleaseData;
    pMgr->getFreeSize = bufferMgr_GetFreeSize;
    pMgr->getPrefetchSize = bufferMgr_GetPrefetchSize;

    return pMgr;
}

