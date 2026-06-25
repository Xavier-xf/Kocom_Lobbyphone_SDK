/*
 * Copyright (C) 2008-2016 Allwinner Technology Co. Ltd.
 * Author: Ning Fang <fangning@allwinnertech.com>
 *         Caoyuan Yang <yangcaoyuan@allwinnertech.com>
 *
 * This software is confidential and proprietary and may be used
 * only as expressly authorized by a licensing agreement from
 * Softwinner Products.
 *
 * The entire notice above must be reproduced on all copies
 * and should not be removed.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <cdc_log.h>
#include "EncAdapter.h"
#include "BitstreamManager.h"

#ifdef CONFIG_VIDEO_RT_MEDIA
#include <linux/vmalloc.h>
#include "linux/string.h"
#include "linux/slab.h"
#else
#include <stdlib.h>
#include <string.h>
#endif

#define BITSTREAM_FRAME_FIFO_SIZE 1024  //720p@240fps, buffer 4 seconds.

#define ALIGN_32B(x) (((x) + (31)) & ~(31))
#define ALIGN_64B(x) (((x) + (63)) & ~(63))

BitStreamManager* BitStreamCreate(unsigned char bIsVbvNoCache, int nBufferSize, MEMOPS_STRUCT *memops,
                                      void *veOps, void *pVeopsSelf)
{
    BitStreamManager *handle;
    char *buffer = NULL;
    int nMaxBufferSize = nBufferSize;
    int nMinBufferSize = 32*1024;

    MEMOPS_STRUCT *_memops = memops;/* will be used in meminterface */

    if(nBufferSize < nMinBufferSize)
    {
        logw("vbv buf size[%d] is too small, set to %d", nBufferSize, nMinBufferSize);
        nBufferSize = nMinBufferSize;
        nMaxBufferSize = nBufferSize;
    }

    if (nBufferSize <= 0)
    {
        return NULL;
    }

    do
    {
        if(bIsVbvNoCache == 0)
        {
            buffer = (char*)EncAdapterMemPalloc(nBufferSize);
        }
        else
        {
            buffer = (char*)EncAdapterNoCacheMemPalloc(nBufferSize);
        }
        if(buffer == NULL)
            nBufferSize -= 512*1024;
    }while(buffer == NULL && nBufferSize > 0);

    if (buffer == NULL)
    {
        loge("pSbmBuf == NULL.");
        return NULL;
    }

    if(nBufferSize != nMaxBufferSize)
    {
        logw("just can palloc partial buffer for vbv: %d < %d",nBufferSize, nMaxBufferSize);
    }

    EncAdapterMemFlushCache(buffer, nBufferSize);

    handle = (BitStreamManager *)MALLOC(sizeof(BitStreamManager));
    if (handle == NULL)
    {
        loge("pSbm == NULL.");
        EncAdapterMemPfree(buffer);
        return NULL;
    }

    memset(handle, 0, sizeof(BitStreamManager));

#ifdef CONFIG_VIDEO_RT_MEDIA
    handle->buf_share_fd = -1;
#endif
    handle->nBSListQ.pStreamInfos = (StreamInfo *)MALLOC_BIG(BITSTREAM_FRAME_FIFO_SIZE * sizeof(StreamInfo));
    if (handle->nBSListQ.pStreamInfos == NULL)
    {
        loge("context->nBSListQ.pStreamInfo == NULL.");
        FREE(handle);
        EncAdapterMemPfree(buffer);
        return NULL;
    }

    memset(handle->nBSListQ.pStreamInfos, 0, BITSTREAM_FRAME_FIFO_SIZE * sizeof(StreamInfo));

    MUTEX_INIT(&handle->mutex, NULL);

    handle->memops = memops;
    handle->veOps = veOps;
    handle->pVeopsSelf = pVeopsSelf;

    handle->pStreamBuffer      = buffer;

    handle->pStreamBufferPhyAddr = (char *)EncAdapterMemGetPhysicAddress(handle->pStreamBuffer);
    handle->pStreamBufferPhyAddrEnd   = handle->pStreamBufferPhyAddr + nBufferSize -1;

    handle->nStreamBufferSize  = nBufferSize;
    handle->nWriteOffset       = 0;
    handle->nValidDataSize     = 0;

    handle->nBSListQ.nMaxFrameNum     = BITSTREAM_FRAME_FIFO_SIZE;
    handle->nBSListQ.nValidFrameNum   = 0;
    handle->nBSListQ.nUnReadFrameNum  = 0;
    handle->nBSListQ.nReadPos         = 0;
    handle->nBSListQ.nWritePos        = 0;

    logd("BitStreamCreate OK, phy %px vir %px size %d", handle->pStreamBufferPhyAddr, handle->pStreamBuffer, handle->nStreamBufferSize);
    return handle;
}

void BitStreamDestroy(BitStreamManager* handle)
{
    if (handle != NULL)
    {
        MEMOPS_STRUCT *_memops = handle->memops;/* will be used in meminterface */
        void *veOps = handle->veOps;
        void *pVeopsSelf = handle->pVeopsSelf;

        MUTEX_DESTROY(&handle->mutex);

        if (handle->pStreamBuffer != NULL)
        {
            EncAdapterMemPfree(handle->pStreamBuffer);
            handle->pStreamBuffer = NULL;
        }

        if (handle->nBSListQ.pStreamInfos != NULL)
        {
            FREE_BIG(handle->nBSListQ.pStreamInfos);
            handle->nBSListQ.pStreamInfos = NULL;
        }

        FREE(handle);
    }
}

void* BitStreamBaseAddress(BitStreamManager* handle)
{
    if (handle == NULL)
    {
        loge("BitStreamManager == NULL.");
        return NULL;
    }

    return (void *)handle->pStreamBuffer;
}

void* BitStreamBasePhyAddress(BitStreamManager* handle)
{
    if (handle == NULL)
    {
        loge("BitStreamManager == NULL.");
        return NULL;
    }

    return (void *)handle->pStreamBufferPhyAddr;
}

void* BitStreamEndPhyAddress(BitStreamManager* handle)
{
    if (handle == NULL)
    {
        loge("BitStreamManager == NULL.");
        return NULL;
    }

    return (void *)handle->pStreamBufferPhyAddrEnd;
}

int BitStreamBufferSize(BitStreamManager* handle)
{
    if (handle == NULL)
    {
        loge("BitStreamManager == NULL.");
        return 0;
    }

    return handle->nStreamBufferSize;
}

int BitStreamFreeBufferSize(BitStreamManager* handle)
{
    if (handle == NULL)
    {
        loge("BitStreamManager == NULL.");
        return 0;
    }

    return (handle->nStreamBufferSize - handle->nValidDataSize);
}

int BitStreamFrameNum(BitStreamManager* handle)
{
    if (handle == NULL)
    {
        loge("BitStreamManager == NULL.");
        return -1;
    }

    return handle->nBSListQ.nValidFrameNum;
}

int BitStreamWriteOffset(BitStreamManager* handle)
{
    if (handle == NULL)
    {
        loge("BitStreamManager == NULL.");
        return -1;
    }

    return handle->nWriteOffset;
}

int BitStreamAddOneBitstream(BitStreamManager* handle, StreamInfo* pStreamInfo)
{
    int nWritePos;
    int NewWriteOffset;
    static unsigned int cnt = 0;

    if (handle == NULL || pStreamInfo == NULL)
    {
        loge("param error.");
        return -1;
    }

	if (pStreamInfo->nStreamLength == 0) {
        loge("nStreamLength is 0 pStreamBuffer %px.", handle->pStreamBuffer);
        return -1;
	}

	logv("******** nStreamOffset %d nStreamLength = %d", pStreamInfo->nStreamOffset, pStreamInfo->nStreamLength);
    MUTEX_LOCK(&handle->mutex);
    if (handle->nBSListQ.nValidFrameNum >= handle->nBSListQ.nMaxFrameNum)
    {
        if (cnt % 200 == 0) {
            logw("nValidFrameNum > nMaxFrameNum, data frames will be discarded, cnt %d", cnt);
        }
        cnt++;
        MUTEX_UNLOCK(&handle->mutex);
        return -1;
    }

    if (pStreamInfo->nStreamLength > (handle->nStreamBufferSize - handle->nValidDataSize))
    {
        loge("pStreamInfo->nStreamLength[%d] > freebuffer[%d]\n",pStreamInfo->nStreamLength,\
            handle->nStreamBufferSize - handle->nValidDataSize);
        MUTEX_UNLOCK(&handle->mutex);
        return -1;
    }

    nWritePos = handle->nBSListQ.nWritePos;
    memcpy(&handle->nBSListQ.pStreamInfos[nWritePos], pStreamInfo, sizeof(StreamInfo));

    handle->nBSListQ.pStreamInfos[nWritePos].nID = nWritePos;
    nWritePos++;
    if (nWritePos >= handle->nBSListQ.nMaxFrameNum)
    {
        nWritePos = 0;
    }

    handle->nBSListQ.nWritePos = nWritePos;
    handle->nBSListQ.nValidFrameNum++;
    handle->nBSListQ.nUnReadFrameNum++;
    handle->nValidDataSize += ALIGN_64B(pStreamInfo->nStreamLength); // encoder need 64 byte align

    if(handle->bEncH264Nalu == 1)
    {
        int nLen;
        char *pData;
        pData = handle->pStreamBuffer + handle->nWriteOffset;
        nLen = pStreamInfo->nStreamLength - 4;
        pData[0] = (unsigned char)((nLen>>24)&0x000000FF);
        pData[1] = (unsigned char)((nLen>>16)&0x000000FF);
        pData[2] = (unsigned char)((nLen>>8)&0x000000FF);
        pData[3] = (unsigned char)(nLen&0x000000FF);
        logv("nLen=%d,the_data:%x,%x,%x,%x",nLen,pData[0],pData[1],pData[2],pData[3]);
    }
    NewWriteOffset = handle->nWriteOffset + ALIGN_64B(pStreamInfo->nStreamLength);

    if (NewWriteOffset >= handle->nStreamBufferSize)
    {
        NewWriteOffset -= handle->nStreamBufferSize;
    }

    handle->nWriteOffset = NewWriteOffset;

    MUTEX_UNLOCK(&handle->mutex);

    return 0;
}

StreamInfo* BitStreamGetOneBitstream(BitStreamManager* handle)
{
    StreamInfo* pStreamInfo;

    if (handle == NULL)
    {
        loge("handle == NULL");
        return NULL;
    }

    MUTEX_LOCK(&handle->mutex);
    if (handle->nBSListQ.nUnReadFrameNum == 0)
    {
        logv("nUnReadFrameNum == 0.");
        MUTEX_UNLOCK(&handle->mutex);
        return NULL;
    }

    pStreamInfo = &handle->nBSListQ.pStreamInfos[handle->nBSListQ.nReadPos];

    if (pStreamInfo == NULL)
    {
        loge("request failed.");
        MUTEX_UNLOCK(&handle->mutex);
        return NULL;
    }

    handle->nBSListQ.nReadPos++;
    handle->nBSListQ.nUnReadFrameNum--;
    if (handle->nBSListQ.nReadPos >= handle->nBSListQ.nMaxFrameNum)
    {
        handle->nBSListQ.nReadPos = 0;
    }
    MUTEX_UNLOCK(&handle->mutex);

    return pStreamInfo;
}

StreamInfo* BitStreamGetOneBitstreamInfo(BitStreamManager* handle)
{
    StreamInfo* pStreamInfo;

    if (handle == NULL)
    {
        loge("handle == NULL");
        return NULL;
    }

    MUTEX_LOCK(&handle->mutex);
    if (handle->nBSListQ.nUnReadFrameNum == 0)
    {
        logv("nUnReadFrameNum == 0.");
        MUTEX_UNLOCK(&handle->mutex);
        return NULL;
    }

    pStreamInfo = &handle->nBSListQ.pStreamInfos[handle->nBSListQ.nReadPos];

    if (pStreamInfo == NULL)
    {
        logv("request failed.");
        MUTEX_UNLOCK(&handle->mutex);
        return NULL;
    }
    MUTEX_UNLOCK(&handle->mutex);
    logv("get bitstream info, offset = %d, len = %d",pStreamInfo->nStreamOffset,pStreamInfo->nStreamLength);

    return pStreamInfo;
}

int BitStreamGetUnReturnNum(BitStreamManager* handle)
{
    int nUnReturnNum = 0;

    if (handle == NULL)
    {
        loge("handle == NULL");
        return nUnReturnNum;
    }

    MUTEX_LOCK(&handle->mutex);
    nUnReturnNum = handle->nBSListQ.nValidFrameNum - handle->nBSListQ.nUnReadFrameNum;

    logv("nUnReturnNum = %d, nValidFrameNum = %d, nUnReadFrameNum = %d",
        nUnReturnNum, handle->nBSListQ.nValidFrameNum, handle->nBSListQ.nUnReadFrameNum);

    MUTEX_UNLOCK(&handle->mutex);

    return nUnReturnNum;
}

int BitStreamGetUnReadNum(BitStreamManager* handle)
{
    int nUnReadNum = 0;

    if (handle == NULL)
    {
        loge("handle == NULL");
        return nUnReadNum;
    }

    MUTEX_LOCK(&handle->mutex);
    nUnReadNum = handle->nBSListQ.nUnReadFrameNum;

    logv("nValidFrameNum = %d, nUnReadFrameNum = %d",
        handle->nBSListQ.nValidFrameNum, handle->nBSListQ.nUnReadFrameNum);

    MUTEX_UNLOCK(&handle->mutex);

    return nUnReadNum;
}

int BitStreamReturnOneBitstream(BitStreamManager* handle, StreamInfo* pStreamInfo)
{
    int stream_size;

    if (handle == NULL)
    {
        loge("BitStreamManager == NULL.");
        return 0;
    }

    if (pStreamInfo->nID < 0 || pStreamInfo->nID > handle->nBSListQ.nMaxFrameNum)
    {
        loge("pStreamInfo->nID is error");
    }

    MUTEX_LOCK(&handle->mutex);
    if (handle->nBSListQ.nValidFrameNum == 0)
    {
        loge("no valid frame.");
        MUTEX_UNLOCK(&handle->mutex);
        return -1;
    }

    stream_size = handle->nBSListQ.pStreamInfos[pStreamInfo->nID].nStreamLength;
    handle->nBSListQ.nValidFrameNum--;
    handle->nValidDataSize -= ALIGN_64B(stream_size);
    MUTEX_UNLOCK(&handle->mutex);

    return 0;
}

int BitStreamReset(BitStreamManager* handle, MEMOPS_STRUCT *memops)
{
    MEMOPS_STRUCT *_memops = memops;/* will be used in meminterface */

    if (handle == NULL)
    {
        loge("BitStreamManager == NULL.");
        return -1;
    }

    MUTEX_LOCK(&handle->mutex);

    handle->nWriteOffset       = 0;
    handle->nValidDataSize     = 0;

    handle->nBSListQ.nMaxFrameNum     = BITSTREAM_FRAME_FIFO_SIZE;
    handle->nBSListQ.nValidFrameNum   = 0;
    handle->nBSListQ.nUnReadFrameNum  = 0;
    handle->nBSListQ.nReadPos         = 0;
    handle->nBSListQ.nWritePos        = 0;
    memset(handle->nBSListQ.pStreamInfos, 0, BITSTREAM_FRAME_FIFO_SIZE * sizeof(StreamInfo));

    EncAdapterMemFlushCache(handle->pStreamBuffer, handle->nStreamBufferSize);

    MUTEX_UNLOCK(&handle->mutex);

    return 0;
}

void BitstreamUpdatePackInfo(StreamInfo* pStreamInfo, int nLength, int nType)
{
    int curr = pStreamInfo->nPackNum;
    int prev = (curr - 1) < 0 ? 0 : (curr - 1);
    StreamPackInfo *pInfo = pStreamInfo->mPackInfo;

    pInfo[curr].nOffset = pInfo[prev].nOffset + pInfo[prev].nLength;
    pInfo[curr].nLength = nLength;
    pInfo[curr].nType = nType;

    pStreamInfo->nPackNum++;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

