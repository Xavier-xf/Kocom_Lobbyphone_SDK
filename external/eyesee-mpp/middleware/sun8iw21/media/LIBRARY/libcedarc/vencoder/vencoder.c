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

//#define LOG_TAG "venc"
#ifdef CONFIG_VIDEO_RT_MEDIA
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include "mem_interface.h"
#include <linux/vmalloc.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/prctl.h>
#include "CdcUtil.h"
#include "CdcIniparserapi.h"
#endif
#include "CdcMessageQueue.h"
#include "cdc_log.h"
#include "vencoder.h"
#include "FrameBufferManager.h"
#include "venc_device.h"
#include "EncAdapter.h"
#include "veAdapter.h"
#include "memoryAdapter.h"
#include "cdc_version.h"
#include "EncoderList.h"
#include "veCoreScheduler.h"
#if VENC_SUPPORT_EXT_PARAM
#include "CdcConfigParamParser.h"
#endif

#define MAX_FRAME_BUFFER_NUM 32

#define AW_ENCODER_SHOW_SPEED_INFO (0)

#define MAX_VENCODER_CHANNEL_NUM (16)


#ifdef CONFIG_VIDEO_RT_MEDIA
int OnlineThreadProcess_new(void* param);

extern VENC_DEVICE video_encoder_jpeg;
extern VENC_DEVICE video_encoder_h264_ver2;
extern VENC_DEVICE video_encoder_h265;
extern VENC_DEVICE video_encoder_vp8;

VENC_DEVICE* video_encoder_devices[] =
{
    &video_encoder_h264_ver2,
    &video_encoder_jpeg,
    &video_encoder_h264_ver2,
    &video_encoder_h265,
    //&video_encoder_vp8,  // not any product use vp8 encoder now. (bz)
    0
};

int raise(int signum)
{
    logd("raise %d", signum);
    return 0;
}

#else
void* OnlineThreadProcess_new(void* param);
extern void *EncppCreate(int bVcuEn);
extern void EncppDestroy(void* encpp);
extern int EncppFunction(void* encpp,
                         VencEncppBufferInfo* pInBuffer,
                         VencEncppBufferInfo* pOutBuffer,
                         VencEncppFuncParam* pEncppFunction);
extern VENC_DEVICE* CreateSpecificEncoder(VENC_CODEC_TYPE format);
void AddVENCPlugin(void);
void DelVENCPlugin(void);
#endif

int gVencoderRegCnt = -1;

typedef enum VencoderCmd
{
    VENCODER_CMD_SET_STATE_INIT          = 0,
    VENCODER_CMD_SET_STATE_EXECUT        = 1,//  sync cmd
    VENCODER_CMD_SET_STATE_PAUSE         = 2,//  sync cmd
    VENCODER_CMD_RESET                   = 3,//  sync cmd
    VENCODER_CMD_FLUSH                   = 4,// async cmd
    VENCODER_CMD_EXIT                    = 5,//  sync cmd
    VENCODER_CMD_InputFrameAvailable     = 6,// async cmd
    VENCODER_CMD_OutputStreamAvailable   = 7,// async cmd
    VENCODER_CMD_ReadyToCompute           = 8,// async cmd
}VencoderCmd;

#define VENC_SEM_WAIT_REPLY_NUM (VENCODER_CMD_EXIT + 1)

typedef enum VencoderState
{
	VENCODER_STATE_IDLE     = 0,
	VENCODER_STATE_INITED   = 1,
	VENCODER_STATE_EXCUTING = 2,
	VENCODER_STATE_PAUSE    = 3,
}VencoderState;

typedef struct ShowSpeedInfoCtx
{
    int bEnable;

    int64_t nCurFrameStartTime;
    int64_t nCurFrameEndTime;

    int64_t nPeriodStartTime;
    int64_t nPeriodEndTime;
    int64_t nPeriodCostTotalTime;
    int64_t nPeriodFrameNum;

    int64_t nPeriodMaxTime;
    int64_t nPeriodMinTime;

}ShowSpeedInfoCtx;

typedef struct OverlayParam
{
    unsigned char*   dataBuf;
    unsigned int     dataSize;
    VencOverlayInfoS mOverlayInfo;
    MUTEX_STRUCT  mutex;
    unsigned int     bNeedUpdateFlag;
}OverlayParam;

typedef struct VencContext
{
    unsigned int         mDbgVbvFullContiguousCnt;
    int                  nChannelId;

    OverlayParam         mOverlayParam;
    VENC_DEVICE*         pVEncDevice;
    void*                pEncoderHandle;
    FrameBufferManager*  pFBM;
    VencBaseConfig        baseConfig;
    VencHeaderData        headerData;
    VencInputBuffer      curEncInputbuffer;
    VENC_CODEC_TYPE         codecType;
    unsigned int         ICVersion;
    int                  bInit;
    MEMOPS_STRUCT    *memops;
    VEOPS_STRUCT*           veOpsS;
    void*             pVeOpsSelf;
    ShowSpeedInfoCtx  mShowSpeedInfoCtx;
    THREAD_STRUCT         OfflineThread;
    int bThreadHaveStoped;
    VencoderState     eState;
    CdcMessageQueue*  mq;
    SEM_STRUCT             reply_sem[VENC_SEM_WAIT_REPLY_NUM];

    VENC_RC_MODE      eRcMode;
    unsigned int      bNeedUpdateMbStatInfoFlag;
    unsigned int      bNeedUpdateSharpParamFlag;

    VencCbType*      pCallbacks;
    void*            pAppData;
    int              bEnableResetWholeVE;

    //* h264/h265/jpeg callback to vencoder.c
    VencDeviceCbType mVencDeviceCb;
    //* VeCoreScheduler callback to vencoder.c
    VcsCbTypeEnc     mVcsCb;
    VeCoreScheduler* pVcS;
	StaticTImeInfo stat_time[4];
}VencContext;

typedef enum OnlineBufNumInfo
{
    Had_No_Online_Channel,
    Had_Online_Channel_One_Buf,
    Had_Online_Channel_Two_Buf
}OnlineBufNumInfo;

typedef struct OnlineVencContext
{
    VencContext *pVencCxt[MAX_VENCODER_CHANNEL_NUM];
    THREAD_STRUCT    onlineThread;
    int          bThreadHaveStoped;
	unsigned int hasInputFrame;
    MUTEX_STRUCT   mInputFrameLock;
	unsigned int hasOutputStreamBuf;
    MUTEX_STRUCT      mOutputStreamLock;

    unsigned int      bReadyToCompute;
    MUTEX_STRUCT      mcomputeLock;

    CdcMessageQueue*  mq;
    int               nOnlineChannelIndex;
    OnlineBufNumInfo  eOnlineBufNum;
    int               bVeInitTopFlag;
    int valid_ch_num;
    int valid_online_num;
    int valid_offline_num;
	int exe_ch_num;
}OnlineVencContext;

static OnlineVencContext *pOnlineCxt = NULL;

DEF_STATIC_MUTEX(OnlineMutex);
#if VENC_SUPPORT_EXT_PARAM
#define VE_PARAM_DYNAMIC 0
#define VE_PARAM_STATIC  1

static int check_ve_param_file(int is_static, VencContext *pCurVencCxt, int ch_id);
#endif

static int postMessgaeAndWait(VencContext* venc_ctx, int cmd,  unsigned char* param1, unsigned char* param2)
{
	CdcMessage msg;
	memset(&msg, 0, sizeof(CdcMessage));
	msg.messageId  = cmd;
	msg.params[0]  = (uintptr_t)(&venc_ctx->reply_sem[cmd]);
    msg.params[1]  = (uintptr_t)venc_ctx;
    msg.params[2]  = (uintptr_t)param1;
    msg.params[3]  = (uintptr_t)param2;

	CdcMessageQueuePostMessage(pOnlineCxt->mq, &msg);

	logv("********* cmd = %d, wait reply start", cmd);
	CdcSemTimedWait(&venc_ctx->reply_sem[cmd], -1);
	logv("********* cmd = %d, wait reply finish", cmd);

	return 0;
}

static VENC_DEVICE *vencoderDeviceCreate(VENC_CODEC_TYPE type)
{
    VENC_DEVICE *vencoder_device_handle;
#ifdef CONFIG_VIDEO_RT_MEDIA
    vencoder_device_handle = (VENC_DEVICE *)kmalloc(sizeof(VENC_DEVICE), GFP_KERNEL);
    if(vencoder_device_handle == NULL)
    {
        return NULL;
    }

    memcpy(vencoder_device_handle, video_encoder_devices[type], sizeof(VENC_DEVICE));
#else
    VENC_DEVICE *match_interface = CreateSpecificEncoder(type);

    logv("CreateSpecificEncoder: %p", match_interface);
    if(match_interface == NULL)
    {
        loge("CreateSpecificEncoder failed");
        return NULL;
    }

    vencoder_device_handle = (VENC_DEVICE *)MALLOC(sizeof(VENC_DEVICE));
    if(vencoder_device_handle == NULL)
    {
        return NULL;
    }

    memcpy(vencoder_device_handle, match_interface, sizeof(VENC_DEVICE));
#endif
    return vencoder_device_handle;
}
static void vencoderDeviceDestroy(void *handle)
{
    if (handle != NULL) {
        FREE(handle);
    }
}

static int updateSpeedInfo(VencContext* venc_ctx)
{
    ShowSpeedInfoCtx* pSpeedCtx = &venc_ctx->mShowSpeedInfoCtx;

    pSpeedCtx->nPeriodFrameNum++;

    pSpeedCtx->nCurFrameEndTime = getCurrentTime();
    pSpeedCtx->nPeriodEndTime = getCurrentTime();

    int64_t nCurFrameCostTime = pSpeedCtx->nCurFrameEndTime - pSpeedCtx->nCurFrameStartTime;
    pSpeedCtx->nPeriodCostTotalTime += nCurFrameCostTime;

    if(pSpeedCtx->nPeriodMaxTime < nCurFrameCostTime || pSpeedCtx->nPeriodMaxTime == 0)
        pSpeedCtx->nPeriodMaxTime = nCurFrameCostTime;

    if(pSpeedCtx->nPeriodMinTime > nCurFrameCostTime || pSpeedCtx->nPeriodMinTime == 0)
        pSpeedCtx->nPeriodMinTime = nCurFrameCostTime;

    int64_t nPeriodTime = pSpeedCtx->nPeriodEndTime - pSpeedCtx->nPeriodStartTime;
    if(nPeriodTime > 1*1000*1000) //* 1 second
    {
        int64_t nHardwareSpeed = pSpeedCtx->nPeriodCostTotalTime/pSpeedCtx->nPeriodFrameNum;
        int64_t nRealSpeed = nPeriodTime/pSpeedCtx->nPeriodFrameNum;
        logd(" hardware_speed = %0.2f fps, real_speed = %0.2f fps, maxTime = %0.2f ms, minTime = %0.2f ms",
            (float)1000*1000/nHardwareSpeed, (float)1000*1000/nRealSpeed,
            (float)pSpeedCtx->nPeriodMaxTime/1000,
            (float)pSpeedCtx->nPeriodMinTime/1000);

        pSpeedCtx->nCurFrameStartTime = 0;
        pSpeedCtx->nCurFrameEndTime   = 0;

        pSpeedCtx->nPeriodStartTime     = 0;
        pSpeedCtx->nPeriodEndTime       = 0;
        pSpeedCtx->nPeriodCostTotalTime = 0;
        pSpeedCtx->nPeriodFrameNum      = 0;
        pSpeedCtx->nPeriodMaxTime       = 0;
        pSpeedCtx->nPeriodMinTime       = 0;
    }

    return 0;
}

int encodeOneFrame(VideoEncoder* pEncoder)
{
    int result = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;
    VencCbInputBufferDoneInfo mCbDoneInfo;
    memset(&mCbDoneInfo, 0 , sizeof(VencCbInputBufferDoneInfo));
    VencMBModeCtrl mMbModeCtl;
    memset(&mMbModeCtl, 0, sizeof(VencMBModeCtrl));
    int bBSBufIsFull = 0;

    if(!venc_ctx) {
        loge("param is NULL");
        return VENC_RESULT_NULL_PTR;
    }

    MUTEX_LOCK(&venc_ctx->mOverlayParam.mutex);
    if(venc_ctx->mOverlayParam.bNeedUpdateFlag)
    {
        venc_ctx->pVEncDevice->SetParameter(venc_ctx->pEncoderHandle, VENC_IndexParamSetOverlay, (void*)&venc_ctx->mOverlayParam.mOverlayInfo);
        venc_ctx->mOverlayParam.bNeedUpdateFlag = 0;
    }
    MUTEX_UNLOCK(&venc_ctx->mOverlayParam.mutex);

    MEMOPS_STRUCT *_memops = venc_ctx->baseConfig.memops;
    unsigned int phyOffset = EncAdapterGetVeAddrOffset();

    if(venc_ctx->baseConfig.bOnlineChannel == 0)
    {
        if(VencFbmRequestValidBuffer(venc_ctx->pFBM, &venc_ctx->curEncInputbuffer) != 0)
        {
            return VENC_RESULT_NO_FRAME_BUFFER;
        }
    }
    else
    {
        memset(&venc_ctx->curEncInputbuffer, 0, sizeof(VencInputBuffer));
    }

    VencGetParameter((VideoEncoder* )venc_ctx, VENC_IndexParamBSbufIsFull, &bBSBufIsFull);

    if(bBSBufIsFull == 1)
    {
        if(venc_ctx->mDbgVbvFullContiguousCnt == 0)
        {
            logw("bitstream empty buffer is full: id = %d", venc_ctx->nChannelId);
        }
        venc_ctx->mDbgVbvFullContiguousCnt++;
    }
    else if(venc_ctx->mDbgVbvFullContiguousCnt > 0)
    {
        logw("bitstream empty buffer is enough: id = %d", venc_ctx->nChannelId);
        venc_ctx->mDbgVbvFullContiguousCnt = 0;
    }

    if(bBSBufIsFull == 1)
    {
        if(venc_ctx->baseConfig.bOnlineChannel == 0)
        {
            VencFbmReturnValidBuffer(venc_ctx->pFBM, &venc_ctx->curEncInputbuffer);
        }
        if(venc_ctx->pCallbacks->InputBufferDone)
        {
            result = VENC_RESULT_BITSTREAM_IS_FULL;
            mCbDoneInfo.nResult = result;
            mCbDoneInfo.pInputBuffer = &venc_ctx->curEncInputbuffer;
            venc_ctx->pCallbacks->InputBufferDone(pEncoder, venc_ctx->pAppData, &mCbDoneInfo);
        }
        return result;
    }

    venc_ctx->curEncInputbuffer.pAddrPhyY -= phyOffset;
    venc_ctx->curEncInputbuffer.pAddrPhyC -= phyOffset;

    if(venc_ctx->mShowSpeedInfoCtx.bEnable == 1)
    {
        venc_ctx->mShowSpeedInfoCtx.nCurFrameStartTime = getCurrentTime();
        if(venc_ctx->mShowSpeedInfoCtx.nPeriodStartTime == 0)
           venc_ctx->mShowSpeedInfoCtx.nPeriodStartTime = getCurrentTime();
    }

    if(venc_ctx->curEncInputbuffer.bEnableCorp == 1)
    {
        if(venc_ctx->baseConfig.bOnlineChannel == 1 || venc_ctx->baseConfig.eInputFormat == VENC_PIXEL_LBC_AW)
        {
            logw("online-mode and lbc-format are not support crop function, online = %d, inputformat = %d",
                venc_ctx->baseConfig.bOnlineChannel, venc_ctx->baseConfig.eInputFormat);
            venc_ctx->curEncInputbuffer.bEnableCorp = 0;
        }
    }

    //* update encpp sharp param before do encode frame
    if(venc_ctx->pCallbacks->EventHandler)
    {
		VencIsp2VeParam *pstIsp2VeParam = NULL;
		VencIspVeLinkParam *pstIspVeLink = NULL;

        pstIsp2VeParam = MALLOC_BIG(sizeof(VencIsp2VeParam));
        pstIspVeLink = MALLOC_BIG(sizeof(VencIspVeLinkParam));

        memset(pstIsp2VeParam, 0, sizeof(VencIsp2VeParam));
        memset(pstIspVeLink, 0, sizeof(VencIspVeLinkParam));

		VencGetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamIspVeLinkParam, pstIspVeLink);
		if (pstIspVeLink->isp2VeEn) {
			int ret = venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
														VencEvent_UpdateIspToVeParam, 0, 0, pstIsp2VeParam);
			if (ret == 0) {
				VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamEnCameraMove, &pstIsp2VeParam->mEnCameraMove);
				VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamEnvLv, &pstIsp2VeParam->mEnvLv);
				VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamAeWeightLum, &pstIsp2VeParam->mAeWeightLum);
				VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamIspAeStatus, &pstIsp2VeParam->mIspAeStatus);
				if (venc_ctx->bNeedUpdateSharpParamFlag == 1)
					VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamSharpConfig, &pstIsp2VeParam->mSharpParam);
			}
		}

        if(venc_ctx->eRcMode == AW_QPMAP)
        {
            venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                                VencEvent_UpdateMbModeInfo, 0, 0, &mMbModeCtl);
            if(mMbModeCtl.mode_ctrl_en == 1)
                VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamMBModeCtrl, &mMbModeCtl);
        }

        venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                            VencEvent_UpdateIspMotionParam, 0, 0, NULL);
        FREE_BIG(pstIsp2VeParam);
        FREE_BIG(pstIspVeLink);
    }

    logv("pAddrPhyY = %p, pAddrPhyC = %p", venc_ctx->curEncInputbuffer.pAddrPhyY, venc_ctx->curEncInputbuffer.pAddrPhyC);
    CDCVeLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
    result = venc_ctx->pVEncDevice->encode(venc_ctx->pEncoderHandle, &venc_ctx->curEncInputbuffer);
    CDCVeUnLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
    if(venc_ctx->pCallbacks->EventHandler)
    {
        if(venc_ctx->bNeedUpdateMbStatInfoFlag == 1)
        {
            venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                                VencEvent_UpdateMbStatInfo, 0, 0, NULL);
        }

        if(venc_ctx->codecType == VENC_CODEC_H265 || venc_ctx->codecType == VENC_CODEC_H264)
        {
			VencVe2IspParam *pstVe2IspParam = NULL;
			VencIspVeLinkParam *pstIspVeLink = NULL;

            pstVe2IspParam = MALLOC_BIG(sizeof(VencVe2IspParam));
            pstIspVeLink = MALLOC_BIG(sizeof(VencIspVeLinkParam));

            memset(pstVe2IspParam, 0, sizeof(VencVe2IspParam));
            memset(pstIspVeLink, 0, sizeof(VencIspVeLinkParam));


			VencGetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamIspVeLinkParam, pstIspVeLink);
			if (pstIspVeLink->ve2IspEn) {
				if (venc_ctx->pVEncDevice->GetParameter(venc_ctx->pEncoderHandle, VENC_IndexParamVe2IspParam, pstVe2IspParam) == 0) {
					venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
							VencEvent_UpdateVeToIspParam, 0, 0, pstVe2IspParam);
				}
			}
            FREE_BIG(pstVe2IspParam);
            FREE_BIG(pstIspVeLink);
        }
    }

    if(venc_ctx->baseConfig.bOnlineChannel == 0)
    {
        VencFbmReturnValidBuffer(venc_ctx->pFBM, &venc_ctx->curEncInputbuffer);
        mCbDoneInfo.nResult = result;
        mCbDoneInfo.pInputBuffer = &venc_ctx->curEncInputbuffer;
        venc_ctx->pCallbacks->InputBufferDone(pEncoder, venc_ctx->pAppData, &mCbDoneInfo);
    }
    else
    {
        if(venc_ctx->pCallbacks && venc_ctx->pCallbacks->InputBufferDone)
        {
            mCbDoneInfo.nResult = result;
            mCbDoneInfo.pInputBuffer = &venc_ctx->curEncInputbuffer;
            venc_ctx->pCallbacks->InputBufferDone(pEncoder, venc_ctx->pAppData, &mCbDoneInfo);
        }
    }

    if(venc_ctx->mShowSpeedInfoCtx.bEnable == 1 && result == VENC_RESULT_OK)
    {
        updateSpeedInfo(venc_ctx);
    }

    return result;
}

static int encodeOneFrame_new(VideoEncoder* pEncoder)
{
    int result = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;
    VencCbInputBufferDoneInfo mCbDoneInfo;
    memset(&mCbDoneInfo, 0 , sizeof(VencCbInputBufferDoneInfo));
    VencMBModeCtrl mMbModeCtl;
    memset(&mMbModeCtl, 0, sizeof(VencMBModeCtrl));
    int bBSBufIsFull = 0;

    if(!venc_ctx) {
        return -1;
    }

	logs("1. exe vencoder.c encodeOneFrame_new");
	if (venc_ctx->baseConfig.extend_flag & (0x1 << 3))
	{
		if (venc_ctx->stat_time[0].startTime == 0) {
			snprintf(venc_ctx->stat_time[0].stat_name, 32, "%d encodeOneFrame_new", venc_ctx->nChannelId);
			create_stat_time(&venc_ctx->stat_time[0], 10*US_PER_MS, 5*US_PER_MS, NULL);
		}
		set_start_time(&venc_ctx->stat_time[0]);
	}
    MUTEX_LOCK(&venc_ctx->mOverlayParam.mutex);
    if(venc_ctx->mOverlayParam.bNeedUpdateFlag)
    {
        venc_ctx->pVEncDevice->SetParameter(venc_ctx->pEncoderHandle, VENC_IndexParamSetOverlay, (void*)&venc_ctx->mOverlayParam.mOverlayInfo);
        venc_ctx->mOverlayParam.bNeedUpdateFlag = 0;
    }
    MUTEX_UNLOCK(&venc_ctx->mOverlayParam.mutex);

    if(venc_ctx->mShowSpeedInfoCtx.bEnable == 1)
    {
        venc_ctx->mShowSpeedInfoCtx.nCurFrameStartTime = getCurrentTime();
        if(venc_ctx->mShowSpeedInfoCtx.nPeriodStartTime == 0)
           venc_ctx->mShowSpeedInfoCtx.nPeriodStartTime = getCurrentTime();
    }

    //* update encpp sharp param before do encode frame
    if(venc_ctx->pCallbacks->EventHandler)
    {
		VencIsp2VeParam *pstIsp2VeParam = NULL;
		VencIspVeLinkParam *pstIspVeLink = NULL;

        pstIsp2VeParam = MALLOC_BIG(sizeof(VencIsp2VeParam));
        pstIspVeLink = MALLOC_BIG(sizeof(VencIspVeLinkParam));

        memset(pstIsp2VeParam, 0, sizeof(VencIsp2VeParam));
        memset(pstIspVeLink, 0, sizeof(VencIspVeLinkParam));

		VencGetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamIspVeLinkParam, pstIspVeLink);
		if (pstIspVeLink->isp2VeEn) {
			int ret = venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
														VencEvent_UpdateIspToVeParam, 0, 0, pstIsp2VeParam);
			if (ret == 0) {
				VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamEnCameraMove, &pstIsp2VeParam->mEnCameraMove);
				VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamEnvLv, &pstIsp2VeParam->mEnvLv);
				VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamAeWeightLum, &pstIsp2VeParam->mAeWeightLum);
				VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamIspAeStatus, &pstIsp2VeParam->mIspAeStatus);
				if (venc_ctx->bNeedUpdateSharpParamFlag == 1)
					VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamSharpConfig, &pstIsp2VeParam->mSharpParam);
			}
		}

        if(venc_ctx->eRcMode == AW_QPMAP)
        {
            venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                                VencEvent_UpdateMbModeInfo, 0, 0, &mMbModeCtl);
            if(mMbModeCtl.mode_ctrl_en == 1)
                VencSetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamMBModeCtrl, &mMbModeCtl);
        }

        venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
                                            VencEvent_UpdateIspMotionParam, 0, 0, NULL);
        FREE_BIG(pstIsp2VeParam);
        FREE_BIG(pstIspVeLink);
    }

    logv("pAddrPhyY = %p, pAddrPhyC = %p", venc_ctx->curEncInputbuffer.pAddrPhyY, venc_ctx->curEncInputbuffer.pAddrPhyC);

    result = venc_ctx->pVEncDevice->RequestHwStaticBuf(venc_ctx->pEncoderHandle);
	if (result == 0) {
		result = venc_ctx->pVEncDevice->encode(venc_ctx->pEncoderHandle, &venc_ctx->curEncInputbuffer);
		if (venc_ctx->pCallbacks->EventHandler && venc_ctx->bNeedUpdateMbStatInfoFlag == 1) {
			venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
				VencEvent_UpdateMbStatInfo, 0, 0, NULL);
		}
		venc_ctx->pVEncDevice->ReturnHwStaticBuf(venc_ctx->pEncoderHandle);
	}

    if(venc_ctx->pCallbacks->EventHandler)
    {
        if(venc_ctx->codecType == VENC_CODEC_H265 || venc_ctx->codecType == VENC_CODEC_H264)
        {
			VencVe2IspParam *pstVe2IspParam = NULL;
			VencIspVeLinkParam *pstIspVeLink = NULL;

            pstVe2IspParam = MALLOC_BIG(sizeof(VencVe2IspParam));
            pstIspVeLink = MALLOC_BIG(sizeof(VencIspVeLinkParam));

            memset(pstVe2IspParam, 0, sizeof(VencVe2IspParam));
            memset(pstIspVeLink, 0, sizeof(VencIspVeLinkParam));


			VencGetParameter((VideoEncoder *)venc_ctx, VENC_IndexParamIspVeLinkParam, pstIspVeLink);
			if (pstIspVeLink->ve2IspEn) {
				if (venc_ctx->pVEncDevice->GetParameter(venc_ctx->pEncoderHandle, VENC_IndexParamVe2IspParam, pstVe2IspParam) == 0) {
					venc_ctx->pCallbacks->EventHandler((VideoEncoder *)venc_ctx, venc_ctx->pAppData,
							VencEvent_UpdateVeToIspParam, 0, 0, pstVe2IspParam);
				}
			}
            FREE_BIG(pstVe2IspParam);
            FREE_BIG(pstIspVeLink);
        }
    }

    if(venc_ctx->mShowSpeedInfoCtx.bEnable == 1 && result == VENC_RESULT_OK)
    {
        updateSpeedInfo(venc_ctx);
    }
	if (venc_ctx->baseConfig.extend_flag & (0x1 << 3))
	{
		set_end_time_do_all(&venc_ctx->stat_time[0], 1000);
	}
	if (venc_ctx->baseConfig.extend_flag & (0x1 << 3))
	{
		char name[32] = {0};
		snprintf(name, 32, "%d encodeOneFrame_new interval", venc_ctx->nChannelId);
		//logw("frameRate %d timd %lld", pCurChannelCxt->frameRate, US_PER_S/pCurChannelCxt->frameRate+US_PER_MS);
		stat_time_interval(&venc_ctx->stat_time[1], 34*US_PER_MS,
			10*US_PER_MS, 1000, name);
	}
    return result;
}

static void enableCheckOnlineStatus(void)
{
    unsigned int bEnableCheckOnlineStatus = 0;
    if(pOnlineCxt->valid_online_num && pOnlineCxt->valid_offline_num)
    {
        bEnableCheckOnlineStatus = 1;
        VencSetParameter((VideoEncoder*)pOnlineCxt->pVencCxt[pOnlineCxt->nOnlineChannelIndex],
                          VENC_IndexParamEnableCheckOnlineStatus,
                          &bEnableCheckOnlineStatus);
    }
    else if(pOnlineCxt->valid_online_num)
    {
        bEnableCheckOnlineStatus = 0;
        VencSetParameter((VideoEncoder*)pOnlineCxt->pVencCxt[pOnlineCxt->nOnlineChannelIndex],
                          VENC_IndexParamEnableCheckOnlineStatus,
                          &bEnableCheckOnlineStatus);
    }

}

static void flushInputBuffer(VideoEncoder* pEncoder)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    VencCbInputBufferDoneInfo mCbDoneInfo;

    memset(&mCbDoneInfo, 0 , sizeof(VencCbInputBufferDoneInfo));

    if(venc_ctx->baseConfig.bOnlineChannel == 0) {
        while (VencFbmGetValidBufferNum(venc_ctx->pFBM)) {
            if (VencFbmRequestValidBuffer(venc_ctx->pFBM, &venc_ctx->curEncInputbuffer) != 0) {
                logw("no inputbuffer break!");
                break;
            }
            VencFbmReturnValidBuffer(venc_ctx->pFBM, &venc_ctx->curEncInputbuffer);
            if(venc_ctx->pCallbacks->InputBufferDone)
            {
                mCbDoneInfo.nResult = VENC_RESULT_DROP_FRAME;
                mCbDoneInfo.pInputBuffer = &venc_ctx->curEncInputbuffer;
                venc_ctx->pCallbacks->InputBufferDone(pEncoder, venc_ctx->pAppData, &mCbDoneInfo);
            }
        }
    }

    //reset input buffer
    VencFbmReset(venc_ctx->pFBM);

}
#ifdef CONFIG_VIDEO_RT_MEDIA

#define SetLogLevel()

#else
static void SetLogLevel(void)
{
    int loglevel;

    const char* loglevelFilePath = CdcGetConfigParamterString("cdc_log_level_file_path", NULL);
    loglevel = CdcGetSpecifyFilePathLogLevel(loglevelFilePath);
    if(loglevel == 0)
    {
        loglevel = CdcGetConfigParamterInt("cdc_log_level", 3);
    }
    if(loglevel < 2 || 6 < loglevel)
    {
        loglevel = 3;
    }

    logd("now cedarc log level:%d", loglevel);
    cdc_log_set_level(loglevel);
    return ;
}

int VencGetVeIommuAddr(VideoEncoder* pEncoder, struct user_iommu_param *pIommuBuf)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL || pIommuBuf == NULL)
    {
        loge("param is NULL, venc_ctx %p pIommuBuf %p", venc_ctx, pIommuBuf);
        return VENC_RESULT_NULL_PTR;
    }

    CdcVeGetIommuAddr(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf, pIommuBuf);
    return VENC_RESULT_OK;
}

int VencFreeVeIommuAddr(VideoEncoder* pEncoder, struct user_iommu_param *pIommuBuf)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL || pIommuBuf == NULL)
    {
        loge("param is NULL, venc_ctx %p pIommuBuf %p", venc_ctx, pIommuBuf);
        return VENC_RESULT_NULL_PTR;
    }

    CdcVeFreeIommuAddr(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf, pIommuBuf);
    return VENC_RESULT_OK;
}

int VencJpegEnc(JpegEncInfo* pJpegInfo, EXIFInfo* pExifInfo, void* pOutBuffer, int* pOutBufferSize)
{
    CEDARC_UNUSE(VencJpegEnc);

    VencAllocateBufferParam bufferParam;
    VideoEncoder* pVideoEnc = NULL;
    VencInputBuffer inputBuffer;
    VencOutputBuffer outputBuffer;
    int result = 0;
    struct user_iommu_param iommu_buffer;
    int ion_fd = -1;
    aw_ion_user_handle_t  handle_ion = ION_USER_HANDLE_INIT_VALUE;
    int                   nBufFd = -1;
    unsigned char         memory_type;

    if (!pJpegInfo || !pExifInfo || !pOutBuffer || !pOutBufferSize)
    {
        loge("param is null, pJpegInfo %p pExifInfo %p pOutBuffer %p pOutBufferSize %p",
            pJpegInfo, pExifInfo, pOutBuffer, pOutBufferSize);
        return VENC_RESULT_NULL_PTR;
    }

    memory_type = CDCIonGetMemType();
    memset(&inputBuffer, 0, sizeof(VencInputBuffer));

    logd("memory_type[%d](0:normal, 1:iommu), pAddrPhyY[%p], pAddrPhyC[%p],\
    quality[%d], nShareBufFd[%d], cropFlag[%d]\n",
                                                        memory_type,
                                                        pJpegInfo->pAddrPhyY,
                                                        pJpegInfo->pAddrPhyC,
                                                        pJpegInfo->quality,
                                                        pJpegInfo->nShareBufFd,
                                                        pJpegInfo->bEnableCorp);
    logd("input_size[%dx%d], output_size[%dx%d], stride[%d], color_format[%d]\n",
                                                        pJpegInfo->sBaseInfo.nInputWidth,
                                                        pJpegInfo->sBaseInfo.nInputHeight,
                                                        pJpegInfo->sBaseInfo.nDstWidth,
                                                        pJpegInfo->sBaseInfo.nDstHeight,
                                                        pJpegInfo->sBaseInfo.nStride,
                                                        pJpegInfo->sBaseInfo.eInputFormat);
    logd("thumb_size[%dx%d]\n",
                                                        pExifInfo->ThumbWidth,
                                                        pExifInfo->ThumbHeight);

    pVideoEnc = VencCreate(VENC_CODEC_JPEG);

    VencSetParameter(pVideoEnc, VENC_IndexParamJpegExifInfo, pExifInfo);
    VencSetParameter(pVideoEnc, VENC_IndexParamJpegQuality, &pJpegInfo->quality);

    if(VencInit(pVideoEnc, &pJpegInfo->sBaseInfo) != 0)
    {
        result = -1;
        goto ERROR;
    }

    if(pJpegInfo->bNoUseAddrPhy)
    {
        bufferParam.nSizeY = pJpegInfo->sBaseInfo.nStride*pJpegInfo->sBaseInfo.nInputHeight;
        bufferParam.nSizeC = bufferParam.nSizeY>>1;

        if(VencAllocateInputBuf(pVideoEnc, &bufferParam, &inputBuffer)<0)
        {
            result = -1;
            goto ERROR;
        }
    }
    else
    {
        //get ion fd
        ion_fd = CdcIonOpen();
        if (ion_fd < 0)
        {
            loge("open ion fd failed \n");
            result = -1;
            goto ERROR;
        }

        //import ion buffer handle
        result = CdcIonImport(ion_fd, pJpegInfo->nShareBufFd, &handle_ion);
        if (result < 0)
        {
            loge("CdcIonImport get handle failed\n");
            result = -1;
            goto ERROR;
        }

        //get ion buffer fd
        nBufFd = CdcIonGetFd(ion_fd, (uintptr_t)handle_ion);
        if (nBufFd < 0)
        {
            loge("CdcIonGetFd get fd failed\n");
            result = -1;
            goto ERROR;
        }

        //get iommu addr
        if(memory_type == MEMORY_IOMMU)
        {
            iommu_buffer.fd = nBufFd;
            VencGetVeIommuAddr(pVideoEnc, &iommu_buffer);
            inputBuffer.pAddrPhyY = (unsigned char*)(uintptr_t)iommu_buffer.iommu_addr;
            inputBuffer.pAddrPhyC = inputBuffer.pAddrPhyY +
                pJpegInfo->sBaseInfo.nStride * ((pJpegInfo->sBaseInfo.nInputHeight + 15) & (~15));
        }
        else
        {
            inputBuffer.pAddrPhyY = (unsigned char *) CdcIonGetPhyAdr(ion_fd,
                                                            (uintptr_t)handle_ion);
            inputBuffer.pAddrPhyC = inputBuffer.pAddrPhyY +
                pJpegInfo->sBaseInfo.nStride * pJpegInfo->sBaseInfo.nInputHeight;
        }
    }

    VencStart(pVideoEnc);

    inputBuffer.bEnableCorp         = pJpegInfo->bEnableCorp;
    inputBuffer.sCropInfo.nLeft     =  pJpegInfo->sCropInfo.nLeft;
    inputBuffer.sCropInfo.nTop        =  pJpegInfo->sCropInfo.nTop;
    inputBuffer.sCropInfo.nWidth    =  pJpegInfo->sCropInfo.nWidth;
    inputBuffer.sCropInfo.nHeight   =  pJpegInfo->sCropInfo.nHeight;

    if(pJpegInfo->bNoUseAddrPhy)
    {
        memcpy(inputBuffer.pAddrVirY, pJpegInfo->pAddrPhyY, bufferParam.nSizeY);
        memcpy(inputBuffer.pAddrVirC, pJpegInfo->pAddrPhyC, bufferParam.nSizeC);
        inputBuffer.bNeedFlushCache = 1;
    }

    VencQueueInputBuf(pVideoEnc, &inputBuffer);

    result = -1;
    int timeoutNum = 400;
    while(result != 0)
    {
        result = VencDequeueOutputBuf(pVideoEnc, &outputBuffer);
        USLEEP(5*1000);
        timeoutNum--;
        if(timeoutNum <= 0)
        {
            loge("dequeue out buffer timeout");
            break;
        }
    }

    if(result == 0)
    {
        memcpy(pOutBuffer, outputBuffer.pData0, outputBuffer.nSize0);
        if(outputBuffer.nSize1)
        {
            memcpy(((unsigned char*)pOutBuffer + outputBuffer.nSize0), \
                outputBuffer.pData1, outputBuffer.nSize1);
            *pOutBufferSize = outputBuffer.nSize0 + outputBuffer.nSize1;
        }
        else
        {
            *pOutBufferSize = outputBuffer.nSize0;
        }

        VencQueueOutputBuf(pVideoEnc, &outputBuffer);
    }

    //deattach buf fd from ve
    if(memory_type == MEMORY_IOMMU)
        VencFreeVeIommuAddr(pVideoEnc, &iommu_buffer);

    //close buf fd
    if(nBufFd != -1)
    {
        result = CdcIonClose(nBufFd);
        if(result < 0)
        {
            loge("CdcIonClose close buf fd error\n");
            result = -1;
            goto ERROR;
        }
    }

    //free ion handle
    if(handle_ion)
    {
        result = CdcIonFree(ion_fd, handle_ion);
        if(result < 0)
        {
            loge("CdcIonFree free ion_handle error\n");
            goto ERROR;
        }
    }

    if (ion_fd != -1)
    {
        result = CdcIonClose(ion_fd);
        if(result < 0)
        {
            loge("CdcIonClose ion fd error\n");
            goto ERROR;
        }
        ion_fd = -1;
    }

ERROR:

    if(pVideoEnc)
    {
        VencPause(pVideoEnc);
        VencDestroy(pVideoEnc);
    }

    return result;
}

VideoEncoderEncpp *VencEncppCreate(int bVcuEn)
{
    SetLogLevel();
    LogVersionInfo();

	return (VideoEncoderEncpp *)EncppCreate(bVcuEn);
}

int VencEncppDestroy(VideoEncoderEncpp* pEncpp)
{

    if(pEncpp == NULL)
    {
        loge("param is NULL");
        return VENC_RESULT_NULL_PTR;
    }

    EncppDestroy((void *)pEncpp);
    return VENC_RESULT_OK;
}

int VencEncppFunction(VideoEncoderEncpp* pEncpp,
                              VencEncppBufferInfo* pInBuffer,
                              VencEncppBufferInfo* pOutBuffer,
                              VencEncppFuncParam* pEncppFunction)
{

    if (!pEncpp || !pInBuffer || !pOutBuffer || !pEncppFunction)
    {
        loge("param is null, pEncpp %p pInBuffer %p pOutBuffer %p pEncppFunction %p",
            pEncpp, pInBuffer, pOutBuffer, pEncppFunction);
        return VENC_RESULT_NULL_PTR;
    }

    int ret = EncppFunction((void*)pEncpp, pInBuffer, pOutBuffer, pEncppFunction);

    return ret;
}

#endif

static int cbSubEnc_notifyReadyToComputeParam(void* handle)
{
    VencContext* venc_ctx = (VencContext*)handle;

    MUTEX_LOCK(&pOnlineCxt->mcomputeLock);
	if (pOnlineCxt->bReadyToCompute == 0)
    {
		CdcMessage msg;
		memset(&msg, 0, sizeof(CdcMessage));
		msg.messageId  = VENCODER_CMD_ReadyToCompute;
		CdcMessageQueuePostMessage(pOnlineCxt->mq, &msg);
		pOnlineCxt->bReadyToCompute = 1;
    }
    MUTEX_UNLOCK(&pOnlineCxt->mcomputeLock);

    return 0;
}

static int cbVcs_inputBufferDone(void *handle, int nResult, VencInputBuffer *pInputBuffer)
{
    VencContext* venc_ctx = (VencContext*)handle;
    VencCbInputBufferDoneInfo mCbDoneInfo;

    memset(&mCbDoneInfo, 0, sizeof(VencCbInputBufferDoneInfo));

    if(venc_ctx->pCallbacks && venc_ctx->pCallbacks->InputBufferDone)
    {
        mCbDoneInfo.nResult = nResult;
        mCbDoneInfo.pInputBuffer = pInputBuffer;
        venc_ctx->pCallbacks->InputBufferDone((VideoEncoder*)venc_ctx, venc_ctx->pAppData, &mCbDoneInfo);
    }
    else
    {
        logw("the venc_ctx->pCallbacks->InputBufferDone is null");
        return -1;
    }

    return 0;
}

static int checkIsReady_online2Buf(void)
{
    VencContext *pVenc = NULL;
    int isReady = 0;
    int onlineHasOut = 0;
    int onlineCnt = 0;
    int offlineCnt = 0;
    int isSingleOnline = 0;
	int i = 0;
    pOnlineCxt->hasInputFrame = 1;
    pOnlineCxt->hasOutputStreamBuf = 1;

    MUTEX_LOCK(&pOnlineCxt->mInputFrameLock);
    MUTEX_LOCK(&pOnlineCxt->mOutputStreamLock);

    for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++) {
        if(!pOnlineCxt->pVencCxt[i]) continue;

        pVenc = pOnlineCxt->pVencCxt[i];
        if(pVenc->eState != VENCODER_STATE_EXCUTING) continue;

        unsigned int isBufFull = 0;
        VencGetParameter((VideoEncoder*)pVenc, VENC_IndexParamBSbufIsFull, &isBufFull);
        int outReady = !isBufFull;

        int inReady = 0;
        if(pVenc->baseConfig.bOnlineChannel) {
            onlineCnt++;
            unsigned int onlineReady = 0;
            if(pVenc->codecType != VENC_CODEC_JPEG) {
                VencGetParameter((VideoEncoder*)pVenc, VENC_IndexParamCheckOnlineStatus, &onlineReady);
            }
            inReady = onlineReady;

            if(outReady) {
                onlineHasOut = 1;
            }
        } else {
            offlineCnt++;
            inReady = (VencGetValidInputBufNum((VideoEncoder*)pVenc) > 0);
        }

        if(inReady && outReady) {
            isReady = 1;
        }

        logv("ch=%d, ready=%d, in=%d, out=%d", pVenc->nChannelId, isReady, inReady, outReady);
    }

    isSingleOnline = (offlineCnt == 0 && onlineCnt == 1);
    if(isSingleOnline && !isReady && onlineHasOut) {
        isReady = 1;
    }

    if(!isReady) {
        logv("waiting for encoding resources");
        pOnlineCxt->hasInputFrame = 0;
        pOnlineCxt->hasOutputStreamBuf = 0;
    }

    MUTEX_UNLOCK(&pOnlineCxt->mOutputStreamLock);
    MUTEX_UNLOCK(&pOnlineCxt->mInputFrameLock);

	return isReady;
}

static int checkIsReady_online1Buf(void)
{
    VencContext *pVenc = NULL;
    int hasOfflineInput = 0;
    int hasOnlineOutput = 0;
    int isReady = 1;
	int i = 0;
    pOnlineCxt->hasInputFrame = 1;
    pOnlineCxt->hasOutputStreamBuf = 1;

    MUTEX_LOCK(&pOnlineCxt->mInputFrameLock);
    MUTEX_LOCK(&pOnlineCxt->mOutputStreamLock);

    for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++) {
        if(!pOnlineCxt->pVencCxt[i]) continue;

        pVenc = pOnlineCxt->pVencCxt[i];
        if(pVenc->eState != VENCODER_STATE_EXCUTING) continue;

        if(pVenc->baseConfig.bOnlineChannel) {
            unsigned int isBufFull = 0;
            VencGetParameter((VideoEncoder*)pVenc, VENC_IndexParamBSbufIsFull, &isBufFull);
            if(!isBufFull) {
                hasOnlineOutput = 1;
            }
        } else {
            if(VencGetValidInputBufNum((VideoEncoder*)pVenc) > 0) {
                hasOfflineInput = 1;
            }
        }
    }

    if(!hasOfflineInput && !hasOnlineOutput) {
        logv("waiting for encoding resources");
        pOnlineCxt->hasInputFrame = 0;
        pOnlineCxt->hasOutputStreamBuf = 0;
        isReady = 0;
    }

    MUTEX_UNLOCK(&pOnlineCxt->mOutputStreamLock);
    MUTEX_UNLOCK(&pOnlineCxt->mInputFrameLock);
	return isReady;
}

static int cbVcs_checkIsReadyToDoEncode(void* handle)
{
	CEDARC_UNUSE(handle);
	int is_ready = 1;
	if(pOnlineCxt->eOnlineBufNum == Had_Online_Channel_One_Buf)
		is_ready = checkIsReady_online1Buf();
	else if(pOnlineCxt->eOnlineBufNum == Had_Online_Channel_Two_Buf || pOnlineCxt->eOnlineBufNum == Had_No_Online_Channel)
		is_ready = checkIsReady_online2Buf();
	else
		loge("eOnlineBufNum[%d] is error", pOnlineCxt->eOnlineBufNum);
	return is_ready;
}

VideoEncoder* VencCreate(VENC_CODEC_TYPE eCodecType)
{
    VencContext* venc_ctx = NULL;
    int i = 0;
    SetLogLevel();
	if (gVencoderRegCnt == -1)
		gVencoderRegCnt = 0;
#ifndef CONFIG_VIDEO_RT_MEDIA
    AddVENCPlugin();
#endif
    LogVersionInfo();
    venc_ctx = (VencContext*)MALLOC(sizeof(VencContext));
    if(!venc_ctx){
        loge("malloc VencContext fail!");
        return NULL;
    }

    memset(venc_ctx, 0,sizeof(VencContext));

    venc_ctx->codecType = eCodecType;
    venc_ctx->bInit = 0;

    if(MUTEX_INIT(&venc_ctx->mOverlayParam.mutex, NULL) != 0)
    {
        loge("init mutex failed");
        FREE(venc_ctx);
        return NULL;
    }

    CDCGetVeOpsS(venc_ctx->veOpsS, VE_OPS_TYPE_NORMAL);
    if(venc_ctx->veOpsS == NULL)
    {
        loge("get ve ops failed , type = %d",VE_OPS_TYPE_NORMAL);
        FREE(venc_ctx);
        return NULL;
    }

    VeConfig mVeConfig;
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 0;
    mVeConfig.nEncoderFlag = 1;
    mVeConfig.nEnableAfbcFlag = 0;
    mVeConfig.nFormat = eCodecType;
    mVeConfig.nWidth = 0;
    mVeConfig.nResetVeMode = 0;
    venc_ctx->pVeOpsSelf = CDCVeInit(venc_ctx->veOpsS, &mVeConfig);
    if(venc_ctx->pVeOpsSelf == NULL)
    {
        loge("init ve ops failed");
        CDCVeRelease(venc_ctx->veOpsS,venc_ctx->pVeOpsSelf);
        FREE(venc_ctx);
        return NULL;
    }

    char* baseAddr = CDCVeGetGroupRegAddr(venc_ctx->veOpsS,
                                          venc_ctx->pVeOpsSelf,
                                          REG_GROUP_VETOP);
    CDCVeLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
    venc_ctx->ICVersion = EncAdapterGetICVersion(baseAddr);
    CDCVeUnLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);

    //for h264 enc use new driver from ic_1708
    if(eCodecType == VENC_CODEC_H264 && venc_ctx->ICVersion >= 0x1708)
        eCodecType = VENC_CODEC_H264_VER2;

    venc_ctx->pVEncDevice = vencoderDeviceCreate(eCodecType);
    if(venc_ctx->pVEncDevice == NULL)
    {
        loge("VencoderDeviceCreate failed\n");
        FREE(venc_ctx);
        return NULL;
    }

    venc_ctx->memops = CDCGetMemOps();
    if (venc_ctx->memops == NULL)
    {
        loge("CDCGetMemOps failed\n");
        FREE(venc_ctx);
        return NULL;
    }

    if(CDCMemOpen(venc_ctx->memops) != 0)
    {
        loge("can not set up memory runtime environment.");
        FREE(venc_ctx);
        return NULL;
    }

    venc_ctx->baseConfig.veOpsS = venc_ctx->veOpsS;
    venc_ctx->baseConfig.pVeOpsSelf = venc_ctx->pVeOpsSelf;
    venc_ctx->baseConfig.memops = venc_ctx->memops;

    venc_ctx->pEncoderHandle =
                    venc_ctx->pVEncDevice->open(&venc_ctx->baseConfig, venc_ctx->ICVersion);
    if(!venc_ctx->pEncoderHandle)
    {
        logd("open VEncDevice error\n");
        vencoderDeviceDestroy(venc_ctx->pVEncDevice);
        venc_ctx->pVEncDevice = NULL;
        FREE(venc_ctx);
        return NULL;
    }

    venc_ctx->mq = CdcMessageQueueCreate(16, "venc_thread");
    for(i = 0; i < VENC_SEM_WAIT_REPLY_NUM; i++)
    {
        SEM_INIT(&venc_ctx->reply_sem[i], 0, 0);
    }

#if(AW_ENCODER_SHOW_SPEED_INFO)
    venc_ctx->mShowSpeedInfoCtx.bEnable = 1;
#endif

    venc_ctx->eState = VENCODER_STATE_IDLE;
	gVencoderRegCnt++;
    return (VideoEncoder*)venc_ctx;
}

void VencDestroy(VideoEncoder* pEncoder)
{
    int i = 0;
    int ret = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if (pEncoder == NULL) {
        loge("pEncoder is NULL.");
        return ;
    }
	if (venc_ctx->baseConfig.extend_flag & (0x1 << 3)) {
		print_stat_info(&venc_ctx->stat_time[0], 1);
		print_stat_info(&venc_ctx->stat_time[1], 1);
	}
    if(venc_ctx->eState != VENCODER_STATE_IDLE)
    {
		MUTEX_LOCK(&OnlineMutex);
		if(pOnlineCxt)
		{
			postMessgaeAndWait(venc_ctx, VENCODER_CMD_EXIT, NULL, NULL);

			//* need re-enable checkOnlineStatus when had channel destroy
			enableCheckOnlineStatus();

			if (pOnlineCxt->valid_ch_num == 0)
			{
				if (!pOnlineCxt->bThreadHaveStoped)
					THREAD_JOIN(pOnlineCxt->onlineThread, (void**)&ret);

				if(pOnlineCxt->mq)
					CdcMessageQueueDestroy(pOnlineCxt->mq);

				MUTEX_DESTROY(&pOnlineCxt->mInputFrameLock);
				MUTEX_DESTROY(&pOnlineCxt->mOutputStreamLock);
				MUTEX_DESTROY(&pOnlineCxt->mcomputeLock);
				FREE(pOnlineCxt);
				pOnlineCxt = NULL;
			}
		}
		MUTEX_UNLOCK(&OnlineMutex);

        venc_ctx->pVEncDevice->uninit(venc_ctx->pEncoderHandle);
#if 0//not used, reserve
        if(venc_ctx->ICVersion == 0x1639)
        {
            if(venc_ctx->baseConfig.nDstWidth >= 3840 || venc_ctx->baseConfig.nDstHeight >= 2160)
            {
                CdcVeUnInitEncoderPerformance(venc_ctx->baseConfig.veOpsS, venc_ctx->baseConfig.pVeOpsSelf, 1);
            }
            else
            {
                CdcVeUnInitEncoderPerformance(venc_ctx->baseConfig.veOpsS, venc_ctx->baseConfig.pVeOpsSelf, 0);
                logd("VeUninitEncoderPerformance");
            }
        }
#endif
        flushInputBuffer(pEncoder);

        if(venc_ctx->pFBM)
        {
            VencFbmDestroy(venc_ctx->pFBM);
            venc_ctx->pFBM = NULL;
        }

        venc_ctx->eState = VENCODER_STATE_IDLE;
    }

    if(venc_ctx->mq)
    {
        CdcMessageQueueDestroy(venc_ctx->mq);
    }
    for(i = 0; i < VENC_SEM_WAIT_REPLY_NUM; i++)
    {
        SEM_DESTROY(&venc_ctx->reply_sem[i]);
    }
    MUTEX_DESTROY(&venc_ctx->mOverlayParam.mutex);

    if(venc_ctx->mOverlayParam.dataBuf)
        FREE(venc_ctx->mOverlayParam.dataBuf);

    if(venc_ctx->pVEncDevice)
    {
        venc_ctx->pVEncDevice->close(venc_ctx->pEncoderHandle);
        vencoderDeviceDestroy(venc_ctx->pVEncDevice);
        venc_ctx->pVEncDevice = NULL;
        venc_ctx->pEncoderHandle = NULL;
    }

    CDCMemClose(venc_ctx->memops);

    if(venc_ctx->veOpsS)
    {
        if(venc_ctx->baseConfig.bOnlineChannel)
            CDCVeSetOnlineChannel(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf, 0);

        CDCVeRelease(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
    }
	gVencoderRegCnt--;
#ifndef CONFIG_VIDEO_RT_MEDIA
	if (gVencoderRegCnt == 0) {
		DelVENCPlugin();
	}
#endif
    FREE(venc_ctx);
}

int VencInit(VideoEncoder* pEncoder, VencBaseConfig* pConfig)
{
    int i = 0;
    int result = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;

	pConfig->bVcuOn = 1;//default vir vcu, change to 0 will use old method.
    if(pEncoder == NULL || pConfig == NULL || venc_ctx->bInit)
    {
        loge("InitVideoEncoder, param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->eState != VENCODER_STATE_IDLE)
    {
        logw("the state[%d] is not idle when call init", venc_ctx->eState);
        return VENC_RESULT_ERROR;
    }

    logd("bOnlineMode = %d, bOnlineChannel = %d", pConfig->bOnlineMode, pConfig->bOnlineChannel);
    logd("lbc2.5 = %d, 2.0 = %d, 1.5 = %d, format = %d",
        pConfig->bLbcLossyComEnFlag2_5x,
        pConfig->bLbcLossyComEnFlag2x, pConfig->bLbcLossyComEnFlag1_5x, pConfig->eInputFormat);

    pConfig->memops = venc_ctx->memops;
    pConfig->veOpsS = venc_ctx->veOpsS;
    pConfig->pVeOpsSelf = venc_ctx->pVeOpsSelf;
    venc_ctx->pFBM = VencFbmCreate(MAX_FRAME_BUFFER_NUM, pConfig->memops,
                                    (void *)venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);

    if(venc_ctx->pFBM == NULL)
    {

        loge("venc_ctx->pFBM == NULL");
        return VENC_RESULT_NO_MEMORY;
    }

    //logd("(f:%s, l:%d)", __FUNCTION__, __LINE__);
#if 0//not used, reserve
    if(venc_ctx->ICVersion == 0x1639)
    {
        if(pConfig->nDstWidth >= 3840 || pConfig->nDstHeight>= 2160)
        {
            CdcVeInitEncoderPerformance(pConfig->veOpsS, pConfig->pVeOpsSelf, 1);
        }
        else
        {
            CdcVeInitEncoderPerformance(pConfig->veOpsS, pConfig->pVeOpsSelf, 0);
            logd("VeInitEncoderPerformance");
        }
    }
#endif
    //logd("(f:%s, l:%d)", __FUNCTION__, __LINE__);

    memcpy(&venc_ctx->baseConfig, pConfig, sizeof(VencBaseConfig));

    venc_ctx->baseConfig.veOpsS = venc_ctx->veOpsS;
    venc_ctx->baseConfig.pVeOpsSelf = venc_ctx->pVeOpsSelf;
#if VENC_SUPPORT_EXT_PARAM
    check_ve_param_file(VE_PARAM_STATIC, venc_ctx, venc_ctx->nChannelId);
#endif
    result = venc_ctx->pVEncDevice->init(venc_ctx->pEncoderHandle, &venc_ctx->baseConfig);
    if(VENC_RESULT_OK == result)
    {
        venc_ctx->bInit = 1;
    }
    else
    {
        loge("venc_init_fail");
    }

    venc_ctx->mVencDeviceCb.notifyReadyToComputeParam = cbSubEnc_notifyReadyToComputeParam;

    if(venc_ctx->pVEncDevice->SetCallback)
        venc_ctx->pVEncDevice->SetCallback(venc_ctx->pEncoderHandle, &venc_ctx->mVencDeviceCb, venc_ctx);
    else
        loge("the venc_ctx->pVEncDevice->SetCallback is null, please implement it");

    venc_ctx->pVcS = venc_ctx->pVEncDevice->GetVcs(venc_ctx->pEncoderHandle);
    if(venc_ctx->pVcS == NULL)
        loge("the venc_ctx->pVcS is null");

    venc_ctx->mVcsCb.inputbufferDone        = cbVcs_inputBufferDone;
    venc_ctx->mVcsCb.checkIsReadyToDoEncode = cbVcs_checkIsReadyToDoEncode;

    vcsSetCallbackEnc(venc_ctx->pVcS, &venc_ctx->mVcsCb, venc_ctx);

    vcsSetFbm(venc_ctx->pVcS, venc_ctx->pFBM);
	venc_ctx->pVEncDevice->SetFbm(venc_ctx->pEncoderHandle, venc_ctx->pFBM);

	int bNeedCreateThreadFlag = 0;
	MUTEX_LOCK(&OnlineMutex);
	if(pOnlineCxt == NULL)
	{
		pOnlineCxt = CALLOC(1, sizeof(OnlineVencContext));
		if(pOnlineCxt == NULL)
		{
			loge("calloc failed, size = %d", sizeof(OnlineVencContext));
		}
		pOnlineCxt->nOnlineChannelIndex = -1;
		pOnlineCxt->eOnlineBufNum = Had_No_Online_Channel;

		pOnlineCxt->mq = CdcMessageQueueCreate(32, "vencOn_thread");

		if(MUTEX_INIT(&pOnlineCxt->mInputFrameLock, NULL) != 0)
		{
			loge("init mutex failed");
			FREE(pOnlineCxt);
			MUTEX_UNLOCK(&OnlineMutex);
			return VENC_RESULT_ERROR;
		}
		if(MUTEX_INIT(&pOnlineCxt->mOutputStreamLock, NULL) != 0)
		{
			loge("init mutex failed");
			FREE(pOnlineCxt);
			MUTEX_UNLOCK(&OnlineMutex);
			return VENC_RESULT_ERROR;
		}
		if(MUTEX_INIT(&pOnlineCxt->mcomputeLock, NULL) != 0)
		{
			loge("init mutex failed");
			FREE(pOnlineCxt);
			MUTEX_UNLOCK(&OnlineMutex);
			return VENC_RESULT_ERROR;
		}
		bNeedCreateThreadFlag = 1;
	}

	if(pOnlineCxt)
	{
		for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
		{
			if(pOnlineCxt->pVencCxt[i] == NULL)
			{
				venc_ctx->nChannelId = i;
				pOnlineCxt->pVencCxt[i] = venc_ctx;
				pOnlineCxt->valid_ch_num++;

				if(venc_ctx->baseConfig.bOnlineChannel == 1) {
					pOnlineCxt->nOnlineChannelIndex = i;
					pOnlineCxt->valid_online_num++;
				} else {
					pOnlineCxt->valid_offline_num++;
				}

				break;
			}
		}
		if(i >= MAX_VENCODER_CHANNEL_NUM)
		{
			loge("cannot find empty pVencCxt: %d", i);
			return -1;
		}
	}

	if(pConfig->bOnlineChannel)
	{
		if(pConfig->nOnlineShareBufNum == 1)
			pOnlineCxt->eOnlineBufNum = Had_Online_Channel_One_Buf;
		if(pConfig->nOnlineShareBufNum == 2)
			pOnlineCxt->eOnlineBufNum = Had_Online_Channel_Two_Buf;
	}
	logd("pOnlineCxt->eOnlineBufNum = %d", pOnlineCxt->eOnlineBufNum);

	enableCheckOnlineStatus();

	if(bNeedCreateThreadFlag == 1) {
		THREAD_CREATE(pOnlineCxt->onlineThread, NULL, OnlineThreadProcess_new, (void*)venc_ctx, "OnlineVenc_new");
	}

	MUTEX_UNLOCK(&OnlineMutex);

    postMessgaeAndWait(venc_ctx, VENCODER_CMD_SET_STATE_INIT, NULL, NULL);
    return result;
}

int VencStart(VideoEncoder* pEncoder)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;
    if(venc_ctx == NULL)
    {
        loge("VencoderStart, param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->eState != VENCODER_STATE_INITED && venc_ctx->eState != VENCODER_STATE_PAUSE)
    {
        logw("the state[%d] is not inited or pause when call start", venc_ctx->eState);
        return VENC_RESULT_ERROR;
    }

    postMessgaeAndWait(venc_ctx, VENCODER_CMD_SET_STATE_EXECUT, NULL, NULL);
    return VENC_RESULT_OK;
}

int VencPause(VideoEncoder* pEncoder)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;
    if(venc_ctx == NULL)
    {
        loge("VencoderPause, param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->eState != VENCODER_STATE_EXCUTING)
    {
        logw("the state[%d] is not excuting when call pause", venc_ctx->eState);
        return VENC_RESULT_ERROR;
    }

    postMessgaeAndWait(venc_ctx, VENCODER_CMD_SET_STATE_PAUSE, NULL, NULL);
    return VENC_RESULT_OK;
}

int VencReset(VideoEncoder* pEncoder)
{
    int result = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL)
    {
        loge("VencReset, param is NULL");
        return VENC_RESULT_NULL_PTR;
    }

    if(venc_ctx->eState != VENCODER_STATE_EXCUTING && venc_ctx->eState != VENCODER_STATE_PAUSE)
    {
        logw("the state[%d] is not excuting or pause when call reset", venc_ctx->eState);
        return VENC_RESULT_ERROR;
    }

    postMessgaeAndWait(venc_ctx, VENCODER_CMD_RESET, NULL, NULL);
    return result;
}

int VencFlush(VideoEncoder* pEncoder, VencFlushType eFlushType)
{
    int result = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL)
    {
        loge("VencReset, param is NULL");
        return VENC_RESULT_NULL_PTR;
    }

    if(venc_ctx->eState == VENCODER_STATE_IDLE)
    {
        logw("flush invalid when state is idle");
        return VENC_RESULT_ERROR;
    }

    postMessgaeAndWait(venc_ctx, VENCODER_CMD_FLUSH, (unsigned char*)eFlushType, NULL);
    return result;


}


static int updateOverlayParam(VencContext* venc_ctx, VencOverlayInfoS *pSrcOverlayInfo)
{
    int i = 0;
    int totalOverlayDataLen = 0;
    OverlayParam* pDstOverlay = &venc_ctx->mOverlayParam;
    unsigned char* pCurBuf = NULL;


    MUTEX_LOCK(&pDstOverlay->mutex);

    for(i = 0; i < pSrcOverlayInfo->blk_num; i++)
        totalOverlayDataLen += pSrcOverlayInfo->overlayHeaderList[i].bitmap_size;

    if(totalOverlayDataLen > pDstOverlay->dataSize)
    {
        if(pDstOverlay->dataBuf)
            FREE(pDstOverlay->dataBuf);
        pDstOverlay->dataBuf = MALLOC(totalOverlayDataLen);
        if(pDstOverlay->dataBuf == NULL)
        {
            loge("malloc failed, size = %d", totalOverlayDataLen);
            MUTEX_UNLOCK(&pDstOverlay->mutex);
            return -1;
        }
        memset(pDstOverlay->dataBuf, 0, totalOverlayDataLen);
        pDstOverlay->dataSize = totalOverlayDataLen;
    }

    memcpy(&pDstOverlay->mOverlayInfo, pSrcOverlayInfo, sizeof(VencOverlayInfoS));
    pCurBuf = pDstOverlay->dataBuf;
    for(i = 0; i < pSrcOverlayInfo->blk_num; i++)
    {
        memcpy(pCurBuf, pSrcOverlayInfo->overlayHeaderList[i].overlay_blk_addr, pSrcOverlayInfo->overlayHeaderList[i].bitmap_size);
        pDstOverlay->mOverlayInfo.overlayHeaderList[i].overlay_blk_addr = pCurBuf;
        pDstOverlay->mOverlayInfo.overlayHeaderList[i].bitmap_size      = pSrcOverlayInfo->overlayHeaderList[i].bitmap_size;
        pCurBuf += pSrcOverlayInfo->overlayHeaderList[i].bitmap_size;
    }
    pDstOverlay->bNeedUpdateFlag = 1;

    MUTEX_UNLOCK(&pDstOverlay->mutex);
    return 0;
}

int VencGetParameter(VideoEncoder* pEncoder, VENC_INDEXTYPE indexType, void* paramData)
{
    CEDARC_UNUSE(VencGetParameter);
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL)
    {
        loge("Type(%d) pEncoder is NULL!", indexType);
        return -1;
    }

    return venc_ctx->pVEncDevice->GetParameter(venc_ctx->pEncoderHandle, indexType, paramData);
}

int VencSetParameter(VideoEncoder* pEncoder, VENC_INDEXTYPE indexType, void* paramData)
{
    int ret = 0;
    VencContext* venc_ctx = (VencContext*)pEncoder;
    int setParamInThread = 0;

    if(venc_ctx == NULL)
    {
        loge("Type(%d) pEncoder is NULL!", indexType);
        return -1;
    }

    if(indexType == VENC_IndexParamH264Param)
    {
        VencH264Param *pH264Param = (VencH264Param *)paramData;
        venc_ctx->eRcMode = pH264Param->sRcParam.eRcMode;
    }
    else if(indexType == VENC_IndexParamH265Param)
    {
        VencH265Param *pH265Param = (VencH265Param *)paramData;
        venc_ctx->eRcMode = pH265Param->sRcParam.eRcMode;
    }
    else if(indexType == VENC_IndexParamMBInfoOutput)
    {
        VencMBInfo *pMBInfo = (VencMBInfo *)paramData;
        if(pMBInfo->num_mb > 0 && pMBInfo->p_para)
            venc_ctx->bNeedUpdateMbStatInfoFlag = 1;
    }
    else if(indexType == VENC_IndexParamEnableEncppSharp)
    {
        venc_ctx->bNeedUpdateSharpParamFlag = *((unsigned int*)paramData);
    }
    else if(indexType == VENC_IndexParamEncAndDecCase)
    {
        //* enable "reset the whole ve when call resetVe()"
        int ve_is_decoding = *((int *)paramData);
        venc_ctx->bEnableResetWholeVE = ve_is_decoding ? 1 : 0;
    }

    if(indexType == VENC_IndexParamSetOverlay)
    {
        return updateOverlayParam(venc_ctx, (VencOverlayInfoS *)paramData);
    }

    if(indexType == VENC_IndexParamEnableRecRefBufReduceFunc)
    {
        if(venc_ctx->bInit == 1)
        {
            logw("The encoder has been initialized, Please use this interface before initialization.");
            return 0;
        }
    }
    logv("indexType = %d, gdcIndex = %d", indexType, VENC_IndexParamGdcConfig);
    //* sync mutex in sub-encoder when gdc and sharp config, as it will call every frame
    //* and if sync by CDCVeLock, it cost too much time
    if(indexType == VENC_IndexParamGdcConfig || indexType == VENC_IndexParamSharpConfig)
    {
        ret = venc_ctx->pVEncDevice->SetParameter(venc_ctx->pEncoderHandle, indexType, paramData);
    }
    else
    {
        CDCVeLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
        ret = venc_ctx->pVEncDevice->SetParameter(venc_ctx->pEncoderHandle, indexType, paramData);
        CDCVeUnLock(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf);
    }

    return ret;
}

int VencGetValidOutputBufNum(VideoEncoder* pEncoder)
{
    CEDARC_UNUSE(VencGetValidOutputBufNum);
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL)
    {
        loge("param is NULL");
        return VENC_RESULT_NULL_PTR;
    }

    return venc_ctx->pVEncDevice->ValidBitStreamFrameNum(venc_ctx->pEncoderHandle);
}

int VencGetUnReadOutputBufNum(VideoEncoder* pEncoder)
{
    CEDARC_UNUSE(VencGetUnReadOutputBufNum);
    VencContext* venc_ctx = (VencContext*)pEncoder;
    return venc_ctx->pVEncDevice->GetBitStreamUnReadNum(venc_ctx->pEncoderHandle);
}

int VencDequeueOutputBuf(VideoEncoder* pEncoder, VencOutputBuffer* pBuffer)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL || pBuffer == NULL)
    {
        loge("param is NULL venc_ctx %p pBuffer %p", venc_ctx, pBuffer);
        return VENC_RESULT_NULL_PTR;
    }

    if(venc_ctx->pVEncDevice->GetOneBitStreamFrame(venc_ctx->pEncoderHandle, pBuffer)!=0)
    {
        return VENC_RESULT_BITSTREAM_IS_EMPTY;
    }
#if DEBUG_SHOW_ENCODE_TIME
    static int cnt_1 = 1;
    if (cnt_1 && venc_ctx->baseConfig.channel_id == 0) {
        cnt_1 = 0;
        logd("channel %d first time GetOneBitStreamFrame, time: %lld", venc_ctx->baseConfig.channel_id, getCurrentTime());
    }
    static int cnt_11 = 1;
    if (cnt_11 && venc_ctx->baseConfig.channel_id == 1) {
        cnt_11 = 0;
        logd("channel %d first time GetOneBitStreamFrame, time: %lld", venc_ctx->baseConfig.channel_id, getCurrentTime());
    }
#endif
    return 0;
}

int VencQueueOutputBuf(VideoEncoder* pEncoder, VencOutputBuffer* pBuffer)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(!venc_ctx || (venc_ctx->baseConfig.bOnlineMode && !pOnlineCxt) || !pBuffer) {
        loge("param is NULL venc_ctx %p pBuffer %p", venc_ctx, pBuffer);
        return VENC_RESULT_NULL_PTR;
    }

    if(venc_ctx->pVEncDevice->FreeOneBitStreamFrame(venc_ctx->pEncoderHandle, pBuffer)!=0)
    {
        return -1;
    }
#if DEBUG_SHOW_ENCODE_TIME
    static int cnt_1 = 1;
    if (cnt_1) {
        cnt_1 = 0;
        logd("first time FreeOneBitStreamFrame, time: %lld", getCurrentTime());
    }
#endif
	MUTEX_LOCK(&pOnlineCxt->mOutputStreamLock);
	if(venc_ctx->baseConfig.bOnlineChannel == 1 && pOnlineCxt->hasOutputStreamBuf == 0)
	{
		pOnlineCxt->hasOutputStreamBuf = 1;
	}
	MUTEX_UNLOCK(&pOnlineCxt->mOutputStreamLock);

    return 0;
}

int VencAllocateInputBuf(VideoEncoder* pEncoder, VencAllocateBufferParam *pBufferParam, VencInputBuffer* dst_inputBuf)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(pEncoder == NULL || pBufferParam == NULL || dst_inputBuf == NULL)
    {
        loge("param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->pFBM == NULL)
    {
        loge("venc_ctx->pFBM == NULL, must call InitVideoEncoder firstly");
        return VENC_RESULT_NO_RESOURCE;
    }

    if(VencFbmAllocateBuffer(venc_ctx->pFBM, pBufferParam, dst_inputBuf)!=0)
    {
        loge("allocat inputbuffer failed");
        return VENC_RESULT_NO_MEMORY;
    }

    return 0;
}

int VencQueueInputBuf(VideoEncoder* pEncoder, VENC_IN VencInputBuffer *inputbuffer)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;
    int nSemCnt = 0;

    if(pEncoder == NULL || inputbuffer == NULL || (venc_ctx->baseConfig.bOnlineMode && !pOnlineCxt))
    {
        loge("param is NULL: pEncoder = %p, inputbuffer = %p", pEncoder, inputbuffer);
        return VENC_RESULT_NULL_PTR;
    }
    if(venc_ctx->pFBM == NULL)
    {

        loge("venc_ctx->pFBM == NULL, must call InitVideoEncoder firstly");
        return VENC_RESULT_NO_RESOURCE;
    }

    if(VencFbmAddValidBuffer(venc_ctx->pFBM, inputbuffer) != 0)
    {
        logv("AddValidBuffer failed");
        return -1;
    }
#if DEBUG_SHOW_ENCODE_TIME
    static int cnt_1 = 1;
    if (cnt_1) {
        cnt_1 = 0;
        logd("first time VencFbmAddValidBuffer, time: %lld", getCurrentTime());
    }
#endif
	MUTEX_LOCK(&pOnlineCxt->mInputFrameLock);
	if(pOnlineCxt->hasInputFrame == 0)
	{
		logv("in buffer notify.");
		pOnlineCxt->hasInputFrame = 1;
		vcsNotifyReadyToEncode(venc_ctx->pVcS);
	}
	MUTEX_UNLOCK(&pOnlineCxt->mInputFrameLock);

    return 0;
}

int VencGetValidInputBufNum(VideoEncoder* pEncoder)
{
    CEDARC_UNUSE(VencGetValidInputBufNum);

    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL)
    {
        loge("param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    return VencFbmGetValidBufferNum(venc_ctx->pFBM);
}


int VencSetFreq(VideoEncoder* pEncoder, int nVeFreq)
{
    CEDARC_UNUSE(VencSetFreq);
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(venc_ctx == NULL)
    {
        loge("param is NULL");
        return VENC_RESULT_NULL_PTR;
    }

    int ret;
    ret = CDCVeSetSpeed(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf, nVeFreq);
    if(ret < 0)
        loge("VideoEncoderSetFreq %d error, ret is %d\n", nVeFreq, ret);

    return ret;
}

int VencSetDdrMode(VideoEncoder* pEncoder, int nDdrType)
{
    CEDARC_UNUSE(VencSetDdrMode);
    VencContext* venc_ctx = (VencContext*)pEncoder;
    if(venc_ctx == NULL)
    {
        loge("param is NULL");
        return VENC_RESULT_NULL_PTR;
    }
    CDCVeSetDdrMode(venc_ctx->veOpsS, venc_ctx->pVeOpsSelf, nDdrType);

    return VENC_RESULT_OK;
}

int VencSetCallbacks(VideoEncoder* pEncoder, VencCbType* pCallbacks, void* pAppData)
{
    VencContext* venc_ctx = (VencContext*)pEncoder;

    if(pEncoder == NULL || pCallbacks == NULL)
    {
        loge("VencSetCallbacks, param is NULL: %p, %p", pEncoder, pCallbacks);
        return VENC_RESULT_NULL_PTR;
    }

    venc_ctx->pCallbacks = pCallbacks;
    venc_ctx->pAppData   = pAppData;

    return 0;
}


#ifndef CONFIG_VIDEO_RT_MEDIA
#ifdef CONFIG_COMPILE_STATIC_LIB

extern void CedarPluginVENCInit_h264();
extern void CedarPluginVENCInit_h265();
extern void CedarPluginVENCInit_jpeg();

void AddVENCPlugin(void)
{

#ifdef CONF_ENABLE_VENC_H264
  CedarPluginVENCInit_h264();
#endif

#ifdef CONF_ENABLE_VENC_H265
  CedarPluginVENCInit_h265();
#endif

#ifdef CONF_ENABLE_VENC_JPEG
  CedarPluginVENCInit_jpeg();
#endif

}
#else

/* /proc/[pid]/maps */
static int GetLocalPathFromProcessMapsVenc(char *localPath, int len)
{
#define LOCAL_LIB "libvencoder.so"

  char path[512] = {0};
  char line[1024] = {0};
  FILE_STRUCT *file = NULL;
  char *strLibPos = NULL;
  int ret = -1;

  memset(localPath, 0x00, len);

  sprintf(path, "/proc/%d/maps", getpid());
  file = fopen(path, "r");
  if (file == NULL)
  {
      loge("fopen failure, errno(%d)", errno);
      ret = -1;
      goto out;
  }

  while (fgets(line, 1023, file) != NULL)
  {
      if ((strLibPos = strstr(line, LOCAL_LIB)) != NULL)
      {
          char *rootPathPos = NULL;
          int localPathLen = 0;
          rootPathPos = strchr(line, '/');
          if (rootPathPos == NULL)
          {
              loge("some thing error, cur line '%s'", line);
              ret = -1;
              goto out;
          }

          localPathLen = strLibPos - rootPathPos - 1;
          if (localPathLen > len -1)
          {
              loge("localPath too long :%s ", localPath);
              ret = -1;
              goto out;
          }

          memcpy(localPath, rootPathPos, localPathLen);
          ret = 0;
          goto out;
      }
  }
  loge("Are you kidding? not found?");

out:
  if (file)
  {
      fclose(file);
  }
  return ret;
}

typedef void VENCPluginEntry(void);

void AddVENCPluginSingle(char *lib)
{
    void *libFd = NULL;
    if(lib == NULL)
    {
        loge(" open lib == NULL ");
        return;
    }

    libFd = dlopen(lib, RTLD_NOW);

    VENCPluginEntry *PluginInit = NULL;

    if (libFd == NULL)
    {
        loge("dlopen '%s' fail: %s", lib, dlerror());
        return ;
    }

    PluginInit = dlsym(libFd, "CedarPluginVENCInit");
    if (PluginInit == NULL)
    {
        logw("Invalid plugin, CedarPluginVDInit not found.");
        return;
    }
    logv("vencoder open lib: %s", lib);
    PluginInit(); /* init plugin */
    return ;
}
/* executive when load */
//static void AddVDPlugin(void) __attribute__((constructor));
void AddVENCPlugin(void)
{
    CEDARC_UNUSE(AddVENCPlugin);
    char localPath[512];
    char slash[4] = "/";
    char loadLib[512];
    struct dirent **namelist = NULL;
    int num = 0, index = 0;
    int pathLen = 0;
    int ret;

    logd("add vencoder plugin from so");

    memset(localPath, 0, 512);
    memset(loadLib, 0, 512);
    //scan_local_path:
    ret = GetLocalPathFromProcessMapsVenc(localPath, 512);
    if (ret != 0)
    {
        logw("get local path failure, scan /system/lib ");
        goto scan_system_lib;
    }

    num = scandir(localPath, &namelist, NULL, NULL);
    if (num <= 0)
    {
        logw("scandir failure, errno(%d), scan /system/lib ", errno);
        goto scan_system_lib;
    }
    strcat(localPath, slash);
    pathLen = strlen(localPath);
    strcpy(loadLib, localPath);
    logw("get local path: %s", localPath);
    for(index = 0; index < num; index++)
    {
        if((strstr((namelist[index])->d_name, "libvenc_h264.so") != NULL) ||
            (strstr((namelist[index])->d_name, "libvenc_h265.so") != NULL) ||
            (strstr((namelist[index])->d_name, "libvenc_jpeg.so") != NULL))
        {
            loadLib[pathLen] = '\0';
            strcat(loadLib, (namelist[index])->d_name);
            logw("load so: %s ", loadLib);
            AddVENCPluginSingle(loadLib);
        }
        FREE(namelist[index]);
        namelist[index] = NULL;
    }

scan_system_lib:
    // TODO: scan /system/lib

    return;
}

#endif

void DelVENCPlugin(void)
{
    VENC_CODEC_TYPE format;
    int i, ret;
    for (i = 0; i <= VENC_CODEC_VP8; i++) {
		ret = VEncoderUnRegister(i);
		if (ret == 1)
			break;
    }
    return;
}

#endif

static int doReset(VencContext *pVencCxt)
{
    int ret = 0;
    //reset input buffer
    ret = VencFbmReset(pVencCxt->pFBM);
    if(ret)
        return -1;

    //reset output buffer
    ret = pVencCxt->pVEncDevice->ResetBitStreamFrame(pVencCxt->pEncoderHandle);
    if(ret)
        return -1;

    return ret;
}

static int doFlush(VencContext *pVencCxt, VencFlushType eFlushType)
{
    int ret = 0;
    int nOutputUnReturnNum = 0;

    if(eFlushType == VENC_FLUSH_INPUT_BUFFER)
    {
        flushInputBuffer((VideoEncoder *) pVencCxt);
    }
    else if(eFlushType == VENC_FLUSH_OUTPUT_BUFFER)
    {
        nOutputUnReturnNum = pVencCxt->pVEncDevice->GetBitStreamUnReturnNum(pVencCxt->pEncoderHandle);
        if(nOutputUnReturnNum == 0)
        {
            ret = pVencCxt->pVEncDevice->ResetBitStreamFrame(pVencCxt->pEncoderHandle);
            if(ret)
                return -1;
        }
        else
        {
            loge("flush output buffer failed as caller not return all output buffer to vencoder, nOutputUnReturnNum = %d", nOutputUnReturnNum);
        }
    }
    else if(eFlushType == VENC_FLUSH_IN_AND_OUT_BUFFER)
    {
        flushInputBuffer((VideoEncoder *) pVencCxt);

        nOutputUnReturnNum = pVencCxt->pVEncDevice->GetBitStreamUnReturnNum(pVencCxt->pEncoderHandle);
        if(nOutputUnReturnNum == 0)
        {
            ret = pVencCxt->pVEncDevice->ResetBitStreamFrame(pVencCxt->pEncoderHandle);
            if(ret)
                return -1;
        }
        else
        {
            loge("flush output buffer failed as caller not return all output buffer to vencoder, nOutputUnReturnNum = %d", nOutputUnReturnNum);
        }
    }

    return ret;
}

#if VENC_SUPPORT_EXT_PARAM

#define VE_PARAM_FILE_PATH "/tmp/ve_param_file_path.conf"
#define VE_PARAM_XML_NAME "/etc/ve_param_"

void ve_param_set_res(VideoEncoder* pVideoEnc, VencParamFromFiles *p_ve_param)
{
    VencContext *pCurVencCxt = (VencContext*)pVideoEnc;
    pCurVencCxt->baseConfig.nInputWidth = p_ve_param->nInputWidth;
    pCurVencCxt->baseConfig.nInputHeight = p_ve_param->nInputHeight;
    pCurVencCxt->baseConfig.nDstWidth = p_ve_param->nDstWidth;
    pCurVencCxt->baseConfig.nDstHeight = p_ve_param->nDstHeight;
}

void ve_param_set_pxl_format(VideoEncoder* pVideoEnc, VencParamFromFiles *p_ve_param)
{
    VencContext *pCurVencCxt = (VencContext*)pVideoEnc;
    pCurVencCxt->baseConfig.eInputFormat = p_ve_param->eInputFormat;
    pCurVencCxt->baseConfig.eOutputFormat = p_ve_param->eOutputFormat;
}

static int check_ve_param_file(int is_static, VencContext *pCurVencCxt, int ch_id)
{
    char ve_param_filename[256] = {0};
    char ve_param_filename_used[256] = {0};
    VencParamFromFiles st_ve_param;
    XML_Parser parser = NULL;
    FILE_STRUCT *xml_file = NULL;
    FILE_STRUCT *path_file = NULL;

    memset(&st_ve_param, 0, sizeof(VencParamFromFiles));
    st_ve_param.is_static = is_static;
    st_ve_param.pVideoEnc = (VideoEncoder*)pCurVencCxt;
    st_ve_param.codecType = pCurVencCxt->codecType;

    path_file = fopen(VE_PARAM_FILE_PATH, "r");
    if(path_file) {
        char line[256] = {0};
        int file_size = 0;

        fseek(path_file, 0, SEEK_END);
        file_size = ftell(path_file);
        rewind(path_file);
        if (file_size > sizeof(line)) {
            loge("file_size large than %d, please change.", sizeof(line));
            fclose(path_file);
            goto ERROR_FOPEN_XML_FILE;
        }
        if (fgets(line, sizeof(line), path_file) != NULL) {
            if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
            snprintf(ve_param_filename, sizeof(ve_param_filename), "%s%d.xml", line, ch_id);
            //logd("read xml file path: %s", ve_param_filename);
        } else {
            logw("read failed, maybe %s context is null, use %s\n", VE_PARAM_FILE_PATH, VE_PARAM_XML_NAME);
            snprintf(ve_param_filename, sizeof(ve_param_filename), "%s%d.xml", VE_PARAM_XML_NAME, ch_id);
        }
        fclose(path_file);
    } else {
        snprintf(ve_param_filename, sizeof(ve_param_filename), "%s%d.xml", VE_PARAM_XML_NAME, ch_id);
    }
    if (access(ve_param_filename, F_OK) == 0) {
        if (is_static)
            logd("static get param xml file: %s", ve_param_filename);
        else
            logd("dynamic get param xml file: %s", ve_param_filename);
        xml_file = fopen(ve_param_filename, "r");
        if(xml_file) {
            if (ParserCreateAndParse(parser, &st_ve_param, xml_file ) != 0)
                goto CREATE_ERROR;

            if (is_static) {
                logd("static_set_ve_param done!");
            } else {
                char *tmp = strstr(ve_param_filename, ".xml");
                strncpy(ve_param_filename_used, ve_param_filename, tmp - ve_param_filename);
                strcat(ve_param_filename_used, "_used");
                strcat(ve_param_filename_used, tmp);
                rename(ve_param_filename, ve_param_filename_used);
                logd("dynamic_set_ve_param done! change file name to %s", ve_param_filename_used);
            }
        } else {
            loge("Please push %s.xml", ve_param_filename);
            goto ERROR_FOPEN_XML_FILE;
        }
    }

    CREATE_ERROR:
        if (xml_file)
            fclose(xml_file);
    ERROR_FOPEN_XML_FILE:
        return 0;
}
#endif

static void checkIsReadyOfAllChannel(void)
{
    int i = 0;
    VencContext *pCurVencCxt = NULL;
    //*check whether need wait for in-frame
    unsigned int bOfflineChannelHadInFrame = 0;
    unsigned int bOnlineChannelHadOutFrame = 0;
    unsigned int bIsReadyToEncode = 0;

    for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
    {
        if(pOnlineCxt->pVencCxt[i])
        {
            pCurVencCxt = pOnlineCxt->pVencCxt[i];

            if(pCurVencCxt->eState != VENCODER_STATE_EXCUTING)
                continue;

            //* break the loop check when had channel which ready to encode
            bIsReadyToEncode = 0;
            VencGetParameter((VideoEncoder* )pCurVencCxt, VENC_IndexParamCheckIsReadyToEncode, &bIsReadyToEncode);
			MUTEX_LOCK(&pOnlineCxt->mcomputeLock);
			if (bIsReadyToEncode == 1) {
				pOnlineCxt->bReadyToCompute = 1;
				MUTEX_UNLOCK(&pOnlineCxt->mcomputeLock);
				break;
			} else {
				pOnlineCxt->bReadyToCompute = 0;
			}
			MUTEX_UNLOCK(&pOnlineCxt->mcomputeLock);
        }
    }

	logv("bReadyToCompute %d", pOnlineCxt->bReadyToCompute);
	if (pOnlineCxt->bReadyToCompute == 0) {
		CdcMessageQueueWaitMessage(pOnlineCxt->mq, 200);
	}

    return ;
}

#ifdef CONFIG_VIDEO_RT_MEDIA
int OnlineThreadProcess_new(void* param)
#else
void* OnlineThreadProcess_new(void* param)
#endif
{
    static unsigned int online_thread_num = 0;
    int i = 0;
    int ret = 0;
    VencContext *pCurVencCxt = NULL;
    CdcMessage msg;
    memset(&msg, 0, sizeof(CdcMessage));
    SEM_STRUCT* pReplySem = NULL;
    U64 loop_cnt = 0;

    logd("online thread start(%u-%p)", online_thread_num++, param);
    #ifdef CONFIG_VIDEO_RT_MEDIA
    #else
    prctl(PR_SET_NAME, (unsigned long)"OnlineVenc_new", 0, 0, 0);
    #endif

    while(1)
    {
        //* process message
    #ifdef CONFIG_VIDEO_RT_MEDIA
        if(kthread_should_stop()) break;
    #endif
        if(CdcMessageQueueTryGetMessage(pOnlineCxt->mq, &msg, 0) == 0)
        {
            pCurVencCxt = (VencContext *)msg.params[1];

            if(msg.messageId == VENCODER_CMD_SET_STATE_INIT)
            {
                pCurVencCxt->eState = VENCODER_STATE_INITED;
            }
            else if(msg.messageId == VENCODER_CMD_SET_STATE_EXECUT)
            {
                 //* disable "reset the whole ve when call resetVe()"
                if(pCurVencCxt->veOpsS && pCurVencCxt->pVeOpsSelf && pCurVencCxt->baseConfig.bOnlineChannel && pCurVencCxt->bEnableResetWholeVE == 0)
                    CDCVeSetOnlineChannel(pCurVencCxt->veOpsS, pCurVencCxt->pVeOpsSelf, 1);

				if (pOnlineCxt->bVeInitTopFlag == 1) {
					CDCVeEnableVe(pCurVencCxt->veOpsS, pCurVencCxt->pVeOpsSelf);
					pOnlineCxt->bVeInitTopFlag = 0;
				}
                if(pCurVencCxt->pVcS)
                    vcsStart(pCurVencCxt->pVcS, 1);

                pCurVencCxt->eState = VENCODER_STATE_EXCUTING;
				pOnlineCxt->exe_ch_num++;
            }
            else if(msg.messageId == VENCODER_CMD_SET_STATE_PAUSE)
            {
                if(pCurVencCxt->pVcS)
                    vcsStart(pCurVencCxt->pVcS, 0);

                pCurVencCxt->eState = VENCODER_STATE_PAUSE;
				pOnlineCxt->exe_ch_num--;
            }
            else if(msg.messageId == VENCODER_CMD_RESET)
            {
                doReset(pCurVencCxt);
            }
            else if(msg.messageId == VENCODER_CMD_FLUSH)
            {
                VencFlushType eFlushType = (VencFlushType)msg.params[2];
                doFlush(pCurVencCxt, eFlushType);
            }
            else if(msg.messageId == VENCODER_CMD_EXIT)
            {
				if (pCurVencCxt->pVcS && pCurVencCxt->eState != VENCODER_STATE_PAUSE)
					vcsStart(pCurVencCxt->pVcS, 0);

                for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
                {
                    if(pOnlineCxt->pVencCxt[i] == pCurVencCxt)
                    {
						pOnlineCxt->pVencCxt[i] = NULL;
						if (pCurVencCxt->eState != VENCODER_STATE_PAUSE) {
							pOnlineCxt->exe_ch_num--;
						}
						pOnlineCxt->valid_ch_num--;
						if(pCurVencCxt->baseConfig.bOnlineChannel == 1) {
                            pOnlineCxt->eOnlineBufNum = Had_No_Online_Channel;
							pOnlineCxt->valid_online_num--;
						} else {
							pOnlineCxt->valid_offline_num--;
						}
                        break;
                    }
                }
            }
			else if (msg.messageId == VENCODER_CMD_ReadyToCompute) {
				pOnlineCxt->bReadyToCompute = 1;
			}
            pReplySem = (SEM_STRUCT*)msg.params[0];
			logv("pReplySem = %p, pCurVencCxt = %p, cmd = %d, cnt = %d", pReplySem, pCurVencCxt, msg.messageId, loop_cnt);
            if(pReplySem)
                SEM_POST(pReplySem);
        }

		if (pOnlineCxt->valid_ch_num <= 0)
        {
			logd("exit the loop process, pOnlineCxt->valid_ch_num = %d", pOnlineCxt->valid_ch_num);
            break;
        }

		if (pOnlineCxt->exe_ch_num >= 0)
        {
            //* step1: do encode frame of all channel, include online and offline channel
            for(i = 0; i < MAX_VENCODER_CHANNEL_NUM; i++)
            {
                if(pOnlineCxt->pVencCxt[i])
                {
                    pCurVencCxt = pOnlineCxt->pVencCxt[i];
                    #if VENC_SUPPORT_EXT_PARAM
                    check_ve_param_file(VE_PARAM_DYNAMIC, pCurVencCxt, i);
                    #endif
                    logv("pCurVencCxt = %p, i  = %d", pCurVencCxt, i);
                    if(pCurVencCxt->eState == VENCODER_STATE_EXCUTING)
                    {
                        int64_t start_t = getCurrentTime();
                        ret = encodeOneFrame_new((VideoEncoder*)pCurVencCxt);
                        int64_t end_t = getCurrentTime();
                        if (ret == VENC_RESULT_OK)
							logv("offline[%d] encoder, ret = %d, time = %lld ms, cnt = %d", i, ret, (end_t - start_t)/1000, loop_cnt);

                    }
                }
            }

            //* step2: check had channel which is ready to encode frame, or will make thread wait for message
            checkIsReadyOfAllChannel();
            //* end
        }
        else
        {
            CdcMessageQueueWaitMessage(pOnlineCxt->mq, 200);
        }
		loop_cnt++;
    }

    logd("exit the thread, loop cnt: %lld", loop_cnt);
    return 0;
}

#ifdef CONFIG_VIDEO_RT_MEDIA
EXPORT_SYMBOL(cdc_log_set_level);
EXPORT_SYMBOL(encodeOneFrame);
EXPORT_SYMBOL(VencCreate);
EXPORT_SYMBOL(VencDestroy);
EXPORT_SYMBOL(VencInit);
EXPORT_SYMBOL(VencStart);
EXPORT_SYMBOL(VencPause);
EXPORT_SYMBOL(VencGetParameter);
EXPORT_SYMBOL(VencGetValidOutputBufNum);
EXPORT_SYMBOL(VencSetParameter);
EXPORT_SYMBOL(VencDequeueOutputBuf);
EXPORT_SYMBOL(VencQueueOutputBuf);
EXPORT_SYMBOL(VencQueueInputBuf);
EXPORT_SYMBOL(VencSetCallbacks);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

