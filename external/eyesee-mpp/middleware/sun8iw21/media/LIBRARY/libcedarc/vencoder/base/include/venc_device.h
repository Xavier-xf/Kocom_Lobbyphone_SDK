/*
 * Copyright (C) 2008-2015 Allwinner Technology Co. Ltd.
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

#ifndef _VENC_DEVICE_H_
#define _VENC_DEVICE_H_


#include "vencoder.h"
#include "FrameBufferManager.h"

typedef struct VencDeviceCbType
{
    #if 0
    int (*encodeDone)(
        void* pPrivateData,
        EncodeDoneInfo* pEncodeDoneInfo);

    int (*updateEncodeParamImmediately)(
        void* pPrivateData,
        VcsChannelRegConf *pRegConf,
        VencInputBuffer   *pInputbuffer);
    #endif

    int (*notifyReadyToComputeParam)(
        void* pPrivateData);

} VencDeviceCbType;

typedef struct VENC_DEVICE
{
    const char *codecType;
    void*      (*open)(VencBaseConfig* pBaseConfig, unsigned int nIcVersion);
    int        (*init)(void *handle,VencBaseConfig* pBaseConfig);
    int        (*uninit)(void *handle);
    void       (*close)(void *handle);
    int        (*encode)(void *handle, VencInputBuffer* pInBuffer);
    int        (*GetParameter)(void *handle, int indexType, void* param);
    int        (*SetParameter)(void *handle, int indexType, void* param);
    int        (*ValidBitStreamFrameNum)(void *handle);
    int        (*GetOneBitStreamFrame)(void *handle, VencOutputBuffer *pOutBuffer);
    int        (*FreeOneBitStreamFrame)(void *handle, VencOutputBuffer *pOutBuffer);
    int        (*ResetBitStreamFrame)(void *handle);
    int        (*GetBitStreamUnReturnNum)(void *handle);
    int        (*GetBitStreamUnReadNum)(void *handle);
    int        (*SetCallback)(void *handle, VencDeviceCbType* pCallbacks, void* pPrivateData);
    void*      (*GetVcs)(void *handle);
	int        (*SetFbm)(void *handle, FrameBufferManager *pFbm);
    int        (*RequestHwStaticBuf)(void *handle);
    int        (*ReturnHwStaticBuf)(void *handle);
}VENC_DEVICE;

#endif //_VENC_DEVICE_H_

#ifdef __cplusplus
}
#endif /* __cplusplus */

