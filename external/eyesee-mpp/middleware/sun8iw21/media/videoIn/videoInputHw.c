/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : videoInputHw.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/06/28
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "videoInputHw"
#include <utils/plat_log.h>

#include <errno.h>
#include <memory.h>
#include <sched.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <sys/syscall.h>

//#include <ion_memmanager.h>
#include <ion_mem_alloc.h>
#include <sys_linux_ioctl.h>
#include "isp_dev.h"
#include "isp.h"
#include "mm_component.h"

#include <VideoVirViCompPortIndex.h>
// #include "../include/videoIn/videoInputHw.h"
#include "videoInputHw.h"
#include <mpi_videoformat_conversion.h>
#include <ChannelRegionInfo.h>
#include <BITMAP_S.h>
#include "VIPPDrawOSD_V5.h"
#include <SystemBase.h>

#include "ISPCapsYUVData.h"

#include <cdx_list.h>
#include <ConfigOption.h>
#include <media_debug.h>

/* ref to ./lichee/linux-4.9/drivers/media/platform/sunxi-vin/platform/platform_cfg.h */
#define VIN_ALIGN_WIDTH 16
#define VIN_ALIGN_HEIGHT 16

// viChnManager *gpViChnManager = NULL;
//viChnManager *gpVippManager[VI_VIPP_NUM_MAX] = {NULL}; // { NULL, NULL, NULL, NULL };
//struct hw_isp_media_dev *media;
// struct isp_video_device *video_node[HW_VIDEO_DEVICE_NUM] = {NULL, NULL};
#define ICE_THREAD_UP 0
#if ICE_THREAD_UP
#define gettid() syscall(__NR_gettid)
#define SCHED_DEADLINE	6

struct sched_attr
{
	__u32 size;
	__u32 sched_policy;
	__u64 sched_flags;
	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;
	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;
	/* SCHED_DEADLINE (nsec) */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;
};
int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags)
{
	return syscall(__NR_sched_setattr, pid, attr, flags);
}
int sched_getattr(pid_t pid, struct sched_attr *attr,
		unsigned int size, unsigned int flags)
{
	return syscall(__NR_sched_getattr, pid, attr, size, flags);
}

#endif

VIDevManager *gpVIDevManager;

void *VideoInputHw_CapThread(void *pThreadData);

ERRORTYPE videoInputHw_Construct(int vipp_id)
{
    int i, ret;

    pthread_mutex_lock(&gpVIDevManager->mManagerLock);

    if (gpVIDevManager->gpVippManager[vipp_id] != NULL) {
        pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
        return SUCCESS;
    }
    gpVIDevManager->gpVippManager[vipp_id] = (viChnManager *)malloc(sizeof(viChnManager));
    if (NULL == gpVIDevManager->gpVippManager[vipp_id]) {
        aloge("alloc viChnManager error(%s)!", strerror(errno));
        pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
        return FAILURE;
    }
    memset(gpVIDevManager->gpVippManager[vipp_id], 0, sizeof(viChnManager));

    ret = pthread_mutex_init(&gpVIDevManager->gpVippManager[vipp_id]->mLock, NULL);
    if (ret != 0) {
        aloge("fatal error! mutex init fail");
        free(gpVIDevManager->gpVippManager[vipp_id]);
        gpVIDevManager->gpVippManager[vipp_id] = NULL;
        pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
        return FAILURE;
    }
    pthread_mutex_init(&gpVIDevManager->gpVippManager[vipp_id]->mRefsLock, NULL);
    pthread_mutex_init(&gpVIDevManager->gpVippManager[vipp_id]->mFrameListLock, NULL);
    pthread_mutex_init(&gpVIDevManager->gpVippManager[vipp_id]->mRegionLock, NULL);
    pthread_mutex_init(&gpVIDevManager->gpVippManager[vipp_id]->mLongExpLock, NULL);
    INIT_LIST_HEAD(&gpVIDevManager->gpVippManager[vipp_id]->mChnList);
    INIT_LIST_HEAD(&gpVIDevManager->gpVippManager[vipp_id]->mOverlayList);
    INIT_LIST_HEAD(&gpVIDevManager->gpVippManager[vipp_id]->mCoverList);
    INIT_LIST_HEAD(&gpVIDevManager->gpVippManager[vipp_id]->mOrlList);
    INIT_LIST_HEAD(&gpVIDevManager->gpVippManager[vipp_id]->mIdleFrameList);
    INIT_LIST_HEAD(&gpVIDevManager->gpVippManager[vipp_id]->mReadyFrameList);
    for(i=0;i<32;i++)
    {
        VippFrame *pNode = (VippFrame*)malloc(sizeof(VippFrame));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
            break;
        }
        memset(pNode, 0, sizeof(VippFrame));
        list_add_tail(&pNode->mList, &gpVIDevManager->gpVippManager[vipp_id]->mIdleFrameList);
    }
	gpVIDevManager->gpVippManager[vipp_id]->vipp_dev_id = vipp_id;
	for (i=0; i<32; i++)
		gpVIDevManager->gpVippManager[vipp_id]->refs[i] = 0;


    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);

	return SUCCESS;
}
ERRORTYPE videoInputHw_Destruct(int vipp_id)
{
    int i;

    pthread_mutex_lock(&gpVIDevManager->mManagerLock);

    if (gpVIDevManager->gpVippManager[vipp_id] != NULL) {
        if (!list_empty(&gpVIDevManager->gpVippManager[vipp_id]->mChnList)) {
            aloge("fatal error! some vi channel still running when destroy vi device!");
        }
        pthread_mutex_lock(&gpVIDevManager->gpVippManager[vipp_id]->mRegionLock);
        ChannelRegionInfo *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &gpVIDevManager->gpVippManager[vipp_id]->mOverlayList, mList)
        {
            list_del(&pEntry->mList);
            ChannelRegionInfo_Destruct(pEntry);
        }
        list_for_each_entry_safe(pEntry, pTmp, &gpVIDevManager->gpVippManager[vipp_id]->mCoverList, mList)
        {
            list_del(&pEntry->mList);
            ChannelRegionInfo_Destruct(pEntry);
        }
        list_for_each_entry_safe(pEntry, pTmp, &gpVIDevManager->gpVippManager[vipp_id]->mOrlList, mList)
        {
            list_del(&pEntry->mList);
            ChannelRegionInfo_Destruct(pEntry);
        }
        pthread_mutex_unlock(&gpVIDevManager->gpVippManager[vipp_id]->mRegionLock);
        pthread_mutex_lock(&gpVIDevManager->gpVippManager[vipp_id]->mFrameListLock);
        if(!list_empty(&gpVIDevManager->gpVippManager[vipp_id]->mReadyFrameList))
        {
            int cnt = 0;
            VippFrame *pEntry;
            list_for_each_entry(pEntry, &gpVIDevManager->gpVippManager[vipp_id]->mReadyFrameList, mList)
            {
                aloge("fatal error! vipp[%d] frameBufId[%d] is not released?", pEntry->mVipp, pEntry->mFrameBufId);
                cnt++;
            }
            aloge("fatal error! There is %d frame is not release in vipp[%d]!", cnt, vipp_id);
            list_splice_tail_init(&gpVIDevManager->gpVippManager[vipp_id]->mReadyFrameList, &gpVIDevManager->gpVippManager[vipp_id]->mIdleFrameList);
        }
        VippFrame *pFrameEntry, *pFrameTmp;
        list_for_each_entry_safe(pFrameEntry, pFrameTmp, &gpVIDevManager->gpVippManager[vipp_id]->mIdleFrameList, mList)
        {
            list_del(&pFrameEntry->mList);
            free(pFrameEntry);
        }
        pthread_mutex_unlock(&gpVIDevManager->gpVippManager[vipp_id]->mFrameListLock);

        pthread_mutex_destroy(&gpVIDevManager->gpVippManager[vipp_id]->mLongExpLock);
        pthread_mutex_destroy(&gpVIDevManager->gpVippManager[vipp_id]->mRegionLock);
        pthread_mutex_destroy(&gpVIDevManager->gpVippManager[vipp_id]->mRefsLock);
        pthread_mutex_destroy(&gpVIDevManager->gpVippManager[vipp_id]->mFrameListLock);
        pthread_mutex_destroy(&gpVIDevManager->gpVippManager[vipp_id]->mLock);
        for (i=0; i<32; i++)
        {
            if(gpVIDevManager->gpVippManager[vipp_id]->refs[i] != 0)
            {
                aloge("fatal error! vipp[%d], idx[%d], ref[%d]!=0, check code!", vipp_id, i, gpVIDevManager->gpVippManager[vipp_id]->refs[i]);
                gpVIDevManager->gpVippManager[vipp_id]->refs[i] = 0;
            }
        }
//        if(gpVIDevManager->gpVippManager[vipp_id]->mpOsdGroups)
//        {
//            OsdGroupsDestruct(gpVIDevManager->gpVippManager[vipp_id]->mpOsdGroups);
//            gpVIDevManager->gpVippManager[vipp_id]->mpOsdGroups = NULL;
//        }
        free(gpVIDevManager->gpVippManager[vipp_id]);
        gpVIDevManager->gpVippManager[vipp_id] = NULL;

    }

    // cdx_sem_init(&mVideoStabilization.sync_exit, 0);
    // cdx_sem_deinit(&mVideoStabilization.sync_exit);
    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);

    return SUCCESS;
}

ERRORTYPE videoInputHw_searchExistDev(VI_DEV vipp_id, viChnManager **ppViDev)
{
    ERRORTYPE ret = FAILURE;
    pthread_mutex_lock(&gpVIDevManager->mManagerLock);
    if (gpVIDevManager->gpVippManager[vipp_id] == NULL)
    {
        pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
        return ERR_VI_UNEXIST;
    }
    if(ppViDev!=NULL)
    {
        *ppViDev = gpVIDevManager->gpVippManager[vipp_id];
        ret = SUCCESS;
    }
    else
    {
        ret = ERR_VI_INVALID_NULL_PTR;
    }
    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
    return ret;
}

ERRORTYPE videoInputHw_RegisterCallback(int vipp_id, void *pAppData, MPPCallbackFuncType pMppCallBack)
{
    if (gpVIDevManager->gpVippManager[vipp_id] == NULL)
    {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVIDevManager->gpVippManager[vipp_id]->mLock);
    gpVIDevManager->gpVippManager[vipp_id]->mMppCallback = pMppCallBack;
    gpVIDevManager->gpVippManager[vipp_id]->pAppData = pAppData;
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[vipp_id]->mLock);

    return SUCCESS;
}

ERRORTYPE videoInputHw_addChannel(int vipp_id, VI_CHN_MAP_S *pChn)
{
    if (gpVIDevManager->gpVippManager[vipp_id] == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVIDevManager->gpVippManager[vipp_id]->mLock);
    list_add_tail(&pChn->mList, &gpVIDevManager->gpVippManager[vipp_id]->mChnList);
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[vipp_id]->mLock);

    return SUCCESS;
}
ERRORTYPE videoInputHw_removeChannel(int vipp_id, VI_CHN_MAP_S *pChn)
{
    if (gpVIDevManager->gpVippManager[vipp_id] == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVIDevManager->gpVippManager[vipp_id]->mLock);
    list_del(&pChn->mList);
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[vipp_id]->mLock);
    return SUCCESS;
}

ERRORTYPE videoInputHw_searchExistDevVirChn(VI_DEV vipp_id, VI_CHN ViChn, VI_CHN_MAP_S **ppChn)
{
    ERRORTYPE ret = FAILURE;
    VI_CHN_MAP_S *pEntry;
	int mVirviComVippChn = 0;
	mVirviComVippChn = ((vipp_id << 16) & 0xFFFF0000) | (ViChn & 0x0000FFFF);
	// printf("===== dev=%d, chn=%d, mVirviComVippChn=%x.\r\n", vipp_id, ViChn, mVirviComVippChn);

    pthread_mutex_lock(&gpVIDevManager->mManagerLock);

    if (gpVIDevManager->gpVippManager[vipp_id] == NULL) {
        pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
        return ERR_VI_SYS_NOTREADY;
    }
    pthread_mutex_lock(&gpVIDevManager->gpVippManager[vipp_id]->mLock);
    list_for_each_entry(pEntry, &gpVIDevManager->gpVippManager[vipp_id]->mChnList, mList)
    {
    	// printf("-------%x,,,%x.\r\n", pEntry->mViChn , mVirviComVippChn);
        if (pEntry->mViChn == mVirviComVippChn) {
            if (ppChn) {
                *ppChn = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[vipp_id]->mLock);

    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);

    return ret;
}

/*ERRORTYPE videoInputHw_initVipp(VI_DEV Vipp_id)
{
    if (gpVIDevManager->gpVippManager[Vipp_id] == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVIDevManager->gpVippManager[Vipp_id]->mLock);
    gpVIDevManager->gpVippManager[Vipp_id]->vipp_enable = -1;
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[Vipp_id]->mLock);
    return SUCCESS;
}*/
ERRORTYPE videoInputHw_setVippEnable(VI_DEV Vipp_id)
{
    if (gpVIDevManager->gpVippManager[Vipp_id] == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVIDevManager->gpVippManager[Vipp_id]->mLock);
    gpVIDevManager->gpVippManager[Vipp_id]->vipp_enable = 1;
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[Vipp_id]->mLock);
    return SUCCESS;
}
ERRORTYPE videoInputHw_setVippDisable(VI_DEV Vipp_id)
{
    if (gpVIDevManager->gpVippManager[Vipp_id] == NULL) {
        return FAILURE;
    }
    pthread_mutex_lock(&gpVIDevManager->gpVippManager[Vipp_id]->mLock);
    gpVIDevManager->gpVippManager[Vipp_id]->vipp_enable = 0;
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[Vipp_id]->mLock);
    return SUCCESS;
}

int videoInputHw_IsLongShutterBusy(VI_DEV Vipp_id)
{
    int bIsBusy = 0;

    if (gpVIDevManager->gpVippManager[Vipp_id] == NULL) {
        return FAILURE;
    }

    pthread_mutex_lock(&gpVIDevManager->gpVippManager[Vipp_id]->mLongExpLock);
    bIsBusy = gpVIDevManager->gpVippManager[Vipp_id]->bTakeLongExpPic;
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[Vipp_id]->mLongExpLock);

    return bIsBusy;
}

ERRORTYPE videoInputHw_IncreaseLongShutterRef(VI_DEV Vipp_id)
{
    int bIsBusy = 0;

    if (gpVIDevManager->gpVippManager[Vipp_id] == NULL) {
        aloge("No such video device %d", Vipp_id);
        goto failed;
    }

    pthread_mutex_lock(&gpVIDevManager->gpVippManager[Vipp_id]->mLongExpLock);
    if (gpVIDevManager->gpVippManager[Vipp_id]->iTakeLongExpRef < VI_VIRCHN_NUM_MAX)
        gpVIDevManager->gpVippManager[Vipp_id]->iTakeLongExpRef++;
    else
        aloge("The reference has been got upper limit %d, vipp id %d", VI_VIPP_NUM_MAX, Vipp_id);
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[Vipp_id]->mLongExpLock);
    return gpVIDevManager->gpVippManager[Vipp_id]->iTakeLongExpRef;
failed:
    return FAILURE;
}

ERRORTYPE videoInputHw_DecreaseLongShutterRef(VI_DEV Vipp_id)
{
    int bIsBusy = 0;

    if (gpVIDevManager->gpVippManager[Vipp_id] == NULL) {
        aloge("No such video device %d", Vipp_id);
        goto failed;
    }

    pthread_mutex_lock(&gpVIDevManager->gpVippManager[Vipp_id]->mLongExpLock);
    if (gpVIDevManager->gpVippManager[Vipp_id]->iTakeLongExpRef > 0)
        gpVIDevManager->gpVippManager[Vipp_id]->iTakeLongExpRef--;
    else
        aloge("The reference has been got lowwer limit 0, vipp id %d", Vipp_id);
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[Vipp_id]->mLongExpLock);
    return gpVIDevManager->gpVippManager[Vipp_id]->iTakeLongExpRef;
failed:
    return FAILURE;
}

/* Long exposure is beasd on signal abstract vipp device. */
ERRORTYPE videoInputHw_SetVippShutterTime(VI_DEV Vipp_id, VI_SHUTTIME_CFG_S *pShutTime)
{
    struct isp_video_device *video = NULL;
    int iIspId;
    int time = pShutTime->iTime;

    if ((0 == time) && (VI_SHUTTIME_MODE_AUTO != pShutTime->eShutterMode)) {
        aloge("Wrong shutter time value[%d]", time);
        goto failed;
    }

    if (gpVIDevManager->gpVippManager[Vipp_id] == NULL ||
            gpVIDevManager->media->video_dev[Vipp_id] == NULL) {
        aloge("No such video device %d", Vipp_id);
        goto failed;
    }
    video = gpVIDevManager->media->video_dev[Vipp_id];
    iIspId = video_to_isp_id(video);

    int iExpNewTimeUs = 0;
    int iCurFps = 30;
    struct sensor_config stConfig;

    int iSensorGainVal = 40;
    int iSensorExpVal  = 30000;

    pthread_mutex_lock(&gpVIDevManager->gpVippManager[Vipp_id]->mLongExpLock);
    memset(&stConfig, 0, sizeof(struct sensor_config));
    if (isp_get_sensor_info(iIspId, &stConfig) < 0) {
        aloge("Get isp sensor information failed, isp id %d", iIspId);
        goto failed;
    }
    iCurFps = stConfig.fps_fixed;

    switch(pShutTime->eShutterMode) {
        case VI_SHUTTIME_MODE_AUTO: { /* auto shutter mode */
            video_set_control(video, V4L2_CID_EXPOSURE_AUTO, 0); /* auto exp */
            video_set_control(video, V4L2_CID_AUTOGAIN, 1);      /* auto gain */

//            video_set_vin_reset_time(video, 0);
            if (isp_set_fps(iIspId, iCurFps) < 0) {
                aloge("Set sensor fps %d failed, isp id %d", iCurFps, iIspId);
                goto failed;
            }
            gpVIDevManager->gpVippManager[Vipp_id]->bTakeLongExpPic = 0;
        } break;

        case VI_SHUTTIME_MODE_PREVIEW: { /* preview shutter mode */
            /* get realtime gain exp value */
            video_get_control(video, V4L2_CID_GAIN, &iSensorGainVal);
            video_get_control(video, V4L2_CID_EXPOSURE_ABSOLUTE, &iSensorExpVal);

            if (iCurFps <= time) {
//                video_set_vin_reset_time(video, 0);

                video_set_control(video, V4L2_CID_EXPOSURE_AUTO, 1); // 1:manual exp 2:shutter prio
                video_set_control(video, V4L2_CID_AUTOGAIN, 0); //manual gain

                iExpNewTimeUs = 1000000 / time;
                iSensorGainVal = iSensorGainVal * iSensorExpVal / iExpNewTimeUs;
                iSensorGainVal = (iSensorGainVal < 16) ? 16 : iSensorGainVal;
                iSensorGainVal = iSensorGainVal * 3; /* use 3 times of calc vaule */
                if (video_set_control(video, V4L2_CID_EXPOSURE_ABSOLUTE, iExpNewTimeUs) < 0 ||
                    video_set_control(video, V4L2_CID_GAIN, iSensorGainVal) < 0)
                {
                    aloge("Set gain %d, exposure %d failed.", iSensorGainVal, iExpNewTimeUs);
                    goto failed;
                }
            } else {
                aloge("wrong time value[%d] with <preview shutter mode>", time);
            }
        } break;
        case VI_SHUTTIME_MODE_NIGHT_VIEW: { /* night view mode */
            /* get realtime gain exp value */
            video_get_control(video, V4L2_CID_GAIN, &iSensorGainVal);
            video_get_control(video, V4L2_CID_EXPOSURE_ABSOLUTE, &iSensorExpVal);

            if (iCurFps > time) {
                video_set_control(video, V4L2_CID_EXPOSURE_AUTO, 1); // 1:manual exp 2:shutter prio
                video_set_control(video, V4L2_CID_AUTOGAIN, 0); //manual gain

                /* (time = 0) has been excluded */
                if (time > 0) {
                    iExpNewTimeUs = 1000000 / time;
//                    video_set_vin_reset_time(video, 0);
                } else if (time < 0) {
                    iExpNewTimeUs = 1000000 * (0-time);
//                    video_set_vin_reset_time(video, (0-time));
                }

                iSensorGainVal = iSensorGainVal * iSensorExpVal / iExpNewTimeUs;
                iSensorGainVal = (iSensorGainVal < 16) ? 16 : iSensorGainVal;
                iSensorGainVal = iSensorGainVal * 3; /* use 3 times of calc vaule */
                /* we do not care it is success or not */
                video_set_control(video, V4L2_CID_EXPOSURE_ABSOLUTE, iExpNewTimeUs);
                video_set_control(video, V4L2_CID_GAIN, iSensorGainVal);

                if (isp_set_fps(iIspId, time) < 0) {
                    goto failed;
                }
                gpVIDevManager->gpVippManager[Vipp_id]->bTakeLongExpPic = 1;
            } else {
                aloge("wrong time value[%d] with <night view mode>", time);
            }
        } break;

        default: {
            aloge("wrong shutter mode[%d], use[0~2]", pShutTime->eShutterMode);
            goto failed;
        } break;
    }

    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[Vipp_id]->mLongExpLock);
    return SUCCESS;

failed:
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[Vipp_id]->mLongExpLock);
    return FAILURE;
}

ERRORTYPE videoInputHw_searchVippStatus(VI_DEV Vipp_id, int *pStatus)
{
	int ret = -1;
    pthread_mutex_lock(&gpVIDevManager->mManagerLock);

    if (gpVIDevManager->gpVippManager[Vipp_id] == NULL)
    {
        pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
        *pStatus = 0;
        return FAILURE;
    }
	pthread_mutex_lock(&gpVIDevManager->gpVippManager[Vipp_id]->mLock);
    ret = gpVIDevManager->gpVippManager[Vipp_id]->vipp_enable;
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[Vipp_id]->mLock);
	*pStatus = ret;

    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);

    return SUCCESS;
}

MM_COMPONENTTYPE *videoInputHw_GetChnComp(VI_DEV ViDev, VI_CHN ViChn)
{
    VI_CHN_MAP_S *pChn;
    if (videoInputHw_searchExistDevVirChn(ViDev, ViChn, &pChn) != SUCCESS) {
        return NULL;
    }
    return pChn->mViComp;
}

VI_CHN_MAP_S *videoInputHw_CHN_MAP_S_Construct()
{
    VI_CHN_MAP_S *pChannel = (VI_CHN_MAP_S *)malloc(sizeof(VI_CHN_MAP_S));
    if (NULL == pChannel) {
        aloge("fatal error! malloc fail[%s]!", strerror(errno));
        return NULL;
    }
    memset(pChannel, 0, sizeof(VI_CHN_MAP_S));
    cdx_sem_init(&pChannel->mSemCompCmd, 0);
    return pChannel;
}
void videoInputHw_CHN_MAP_S_Destruct(VI_CHN_MAP_S *pChannel)
{
    if (pChannel->mViComp) {
        aloge("fatal error! Vi component need free before!");
        COMP_FreeHandle(pChannel->mViComp);
        pChannel->mViComp = NULL;
    }
    cdx_sem_deinit(&pChannel->mSemCompCmd);
    free(pChannel);
    //pChannel = NULL;
}
ERRORTYPE videoInputHw_Open_Media() /*Open Media+ISP+CSI Device*/
{
    if (gpVIDevManager)
    {
        if(gpVIDevManager->media)
        {
            alogd("videoInputHw already open.");
            return SUCCESS;
        }
        else
        {
            aloge("fatal error! media is not construct");
        }
    }

    gpVIDevManager = (VIDevManager *)malloc(sizeof(VIDevManager));
    if(gpVIDevManager == NULL)
    {
        aloge("error, gpVIDevManager can not be allocted");
        return FAILURE;
    }
    int index;
    pthread_mutex_init(&gpVIDevManager->mManagerLock, NULL);
    for(index = 0; index < VI_VIPP_NUM_MAX; ++index)
    {
        gpVIDevManager->gpVippManager[index] = NULL;
    }

    gpVIDevManager->media = isp_md_open(MEDIA_DEVICE);
    if (gpVIDevManager->media == NULL) {
        alogd("error: unable to open media device %s\n", MEDIA_DEVICE);
        return FAILURE;
    }

    gpVIDevManager->mSetFrequency = TRUE; //
    gpVIDevManager->mClockFrequency = -1;//deauflt

    pthread_mutex_lock(&gpVIDevManager->mManagerLock);
    memset(&gpVIDevManager->mCmdQueue, 0, sizeof(message_queue_t));
    int ret = message_create(&gpVIDevManager->mCmdQueue);
    if(ret < 0)
    {
        aloge("fatal error! create message queue error!");
    }
    ret = pthread_create(&gpVIDevManager->mCapThreadId, NULL, VideoInputHw_CapThread, NULL);
    if (ret != 0)
    {
        aloge("fatal error! create VideoInputHw_Cap Thread fail[0x%x]!", ret);
    }
    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);

    return SUCCESS;
}
ERRORTYPE videoInputHw_Close_Media() /*Close Media+ISP+CSI Device*/
{
    pthread_mutex_lock(&gpVIDevManager->mManagerLock);
    int nVippIdx = 0;
    for(nVippIdx=0; nVippIdx<VI_VIPP_NUM_MAX;nVippIdx++)
    {
        if (gpVIDevManager->gpVippManager[nVippIdx] != NULL)
        {
            aloge("fatal error! vipp[%d] exist! check code!", nVippIdx);
        }
    }
    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);

    message_t msg;
    InitMessage(&msg);
    msg.command = Stop;
    put_message(&gpVIDevManager->mCmdQueue, &msg);
    // Wait for thread to exit so we can get the status into "error"
    ERRORTYPE eError = SUCCESS;
    int ret = pthread_join(gpVIDevManager->mCapThreadId, (void**) &eError);
    if(ret != 0)
    {
        aloge("fatal error! pthread join fail[%d]", ret);
    }
    message_destroy(&gpVIDevManager->mCmdQueue);

    if (gpVIDevManager->media) { /* Cleanup the ISP resources. */
        isp_md_close(gpVIDevManager->media);
    }
    gpVIDevManager->media = NULL;
    gpVIDevManager->mSetFrequency = TRUE;
    gpVIDevManager->mClockFrequency = -1;
    pthread_mutex_destroy(&gpVIDevManager->mManagerLock);
    free(gpVIDevManager);
    gpVIDevManager = NULL;

    return SUCCESS;
}
ERRORTYPE videoInputHw_ChnInit(int ViCh) /*Open /dev/video[0~3] node*/
{
    //return (ERRORTYPE)isp_video_open(gpVIDevManager->media, ViCh);
    //ERRORTYPE ret = -1;
    pthread_mutex_lock(&gpVIDevManager->mManagerLock);

    if(isp_video_open(gpVIDevManager->media, ViCh) < 0)
    {
        aloge("error: isp video can not open, chn[%d]!", ViCh);
        pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
        return FAILURE;
    }

    if(!gpVIDevManager->mSetFrequency)
    {
        struct isp_video_device *video = gpVIDevManager->media->video_dev[ViCh];
        if (video_set_top_clk(video, gpVIDevManager->mClockFrequency) < 0)
        {
            aloge("Cuation:can not set ISP clock frequency!");
            pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
            return FAILURE;
        }
        alogw("Attention: the ISP clock frequecy had been set %f MHZ", gpVIDevManager->mClockFrequency / 1000000.0);
        gpVIDevManager->mSetFrequency = TRUE;
    }

    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
    return SUCCESS;

}
ERRORTYPE videoInputHw_ChnExit(int ViCh) /*Close /dev/video[0~3] node*/
{
    pthread_mutex_lock(&gpVIDevManager->mManagerLock);

    if(gpVIDevManager->media->video_dev[ViCh])
    {
        int ret = overlay_update(gpVIDevManager->media->video_dev[ViCh], 0);
        if(ret != 0)
        {
            aloge("fatal error! the vipp[%d] OSD can not closed!", ViCh);
        }
    }

    isp_video_close(gpVIDevManager->media, ViCh);

    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
    return 0;
}

#if (AWCHIP == AW_V853)
static int lbcMode_Select(struct VI_DMA_LBC_CMP_S *lbc_cmp, unsigned int fourcc)
{
    switch (fourcc) {
    case V4L2_PIX_FMT_LBC_2_0X: /* 2x */
        lbc_cmp->is_lossy = 1;
        lbc_cmp->bit_depth = 8;
        lbc_cmp->glb_enable = 1;
        lbc_cmp->dts_enable = 1;
        lbc_cmp->ots_enable = 1;
        lbc_cmp->msq_enable = 1;
        lbc_cmp->cmp_ratio_even = 600;
        lbc_cmp->cmp_ratio_odd  = 450;
        lbc_cmp->mb_mi_bits[0]  = 55;
        lbc_cmp->mb_mi_bits[1]  = 110;
        lbc_cmp->rc_adv[0] = 60;
        lbc_cmp->rc_adv[1] = 30;
        lbc_cmp->rc_adv[2] = 15;
        lbc_cmp->rc_adv[3] = 8;
        lbc_cmp->lmtqp_en  = 1;
        lbc_cmp->lmtqp_min = 1;
        lbc_cmp->updata_adv_en = 1;
        lbc_cmp->updata_adv_ratio = 2;
        break;
    case V4L2_PIX_FMT_LBC_1_5X: /* 1.5x */
        lbc_cmp->is_lossy = 1;
        lbc_cmp->bit_depth = 8;
        lbc_cmp->glb_enable = 1;
        lbc_cmp->dts_enable = 1;
        lbc_cmp->ots_enable = 1;
        lbc_cmp->msq_enable = 1;
        lbc_cmp->cmp_ratio_even = 670;
        lbc_cmp->cmp_ratio_odd  = 658;
        lbc_cmp->mb_mi_bits[0]  = 87;
        lbc_cmp->mb_mi_bits[1]  = 167;
        lbc_cmp->rc_adv[0] = 60;
        lbc_cmp->rc_adv[1] = 30;
        lbc_cmp->rc_adv[2] = 15;
        lbc_cmp->rc_adv[3] = 8;
        lbc_cmp->lmtqp_en  = 1;
        lbc_cmp->lmtqp_min = 1;
        lbc_cmp->updata_adv_en = 1;
        lbc_cmp->updata_adv_ratio = 2;
        break;
    case V4L2_PIX_FMT_LBC_2_5X: /* 2.5x */
        lbc_cmp->is_lossy = 1;
        lbc_cmp->bit_depth = 8;
        lbc_cmp->glb_enable = 1;
        lbc_cmp->dts_enable = 1;
        lbc_cmp->ots_enable = 1;
        lbc_cmp->msq_enable = 1;
        lbc_cmp->cmp_ratio_even = 440;
        lbc_cmp->cmp_ratio_odd  = 380;
        lbc_cmp->mb_mi_bits[0]  = 55;
        lbc_cmp->mb_mi_bits[1]  = 94;
        lbc_cmp->rc_adv[0] = 60;
        lbc_cmp->rc_adv[1] = 30;
        lbc_cmp->rc_adv[2] = 15;
        lbc_cmp->rc_adv[3] = 8;
        lbc_cmp->lmtqp_en  = 1;
        lbc_cmp->lmtqp_min = 1;
        lbc_cmp->updata_adv_en = 1;
        lbc_cmp->updata_adv_ratio = 2;
        break;
    case V4L2_PIX_FMT_LBC_1_0X: /* lossless */
        lbc_cmp->is_lossy = 0;
        lbc_cmp->bit_depth = 8;
        lbc_cmp->glb_enable = 1;
        lbc_cmp->dts_enable = 1;
        lbc_cmp->ots_enable = 1;
        lbc_cmp->msq_enable = 1;
        lbc_cmp->cmp_ratio_even = 1000;
        lbc_cmp->cmp_ratio_odd  = 1000;
        lbc_cmp->mb_mi_bits[0]  = 55;
        lbc_cmp->mb_mi_bits[1]  = 94;
        lbc_cmp->rc_adv[0] = 60;
        lbc_cmp->rc_adv[1] = 30;
        lbc_cmp->rc_adv[2] = 15;
        lbc_cmp->rc_adv[3] = 8;
        lbc_cmp->lmtqp_en  = 1;
        lbc_cmp->lmtqp_min = 1;
        lbc_cmp->updata_adv_en = 1;
        lbc_cmp->updata_adv_ratio = 2;
        break;
    default:
        return -1;
    }
    return 0;
}

static void lbcLine_StmWrBit(VI_DMA_LBC_BS_S *bs, unsigned int word, unsigned int len)
{
    if (NULL == bs) {
        return;
    }
    bs->cnt++;
    bs->sum = bs->sum + len;

    while (len > 0) {
        if (len < 32) {
            word = word & ((1 << len) - 1);
        }

        if (bs->left_bits > len) {
            *bs->cur_buf_ptr = *bs->cur_buf_ptr | (word << (8 - bs->left_bits));
            bs->left_bits = bs->left_bits - len;
            break;
        } else {
            *bs->cur_buf_ptr = *bs->cur_buf_ptr | (word << (8 - bs->left_bits));
            len = len - bs->left_bits;
            word = word >> bs->left_bits;
            bs->left_bits = 8;
            bs->cur_buf_ptr++;
        }
    }
}

static void lbcLine_StmInit(VI_DMA_LBC_BS_S *bs, unsigned char* bs_buf_ptr)
{
    if (NULL == bs || NULL == bs_buf_ptr) {
        return;
    }
    bs->cur_buf_ptr = bs_buf_ptr;
    bs->left_bits = 8;
    bs->cnt = 0;
    bs->sum = 0;
}

static void lbcLine_ParaInit(VI_DMA_LBC_PARAM_S *para, VI_DMA_LBC_CFG_S *cfg)
{
    if (NULL == para || NULL == cfg) {
        return;
    }
    para->mb_wth = 16;
    para->frm_wth = (cfg->frm_wth + 31) / 32 * 32;
    para->frm_hgt = cfg->frm_hgt;
    para->line_tar_bits[0] = cfg->line_tar_bits[0];
    para->line_tar_bits[1] = cfg->line_tar_bits[1];
    para->frm_bits = 0;
}

static void lbcLine_Align(VI_DMA_LBC_PARAM_S *para, VI_DMA_LBC_BS_S *bs)
{
    unsigned int align_bit = 0;
    unsigned int align_bit_1 = 0;
    unsigned int align_bit_2 = 0;
    unsigned int i = 0;

    if (NULL == para || NULL == bs) {
        return;
    }

    align_bit = para->line_tar_bits[para->frm_y % 2] - para->line_bits;
    align_bit_1 = align_bit / 1024;
    align_bit_2 = align_bit % 1024;

    for (i = 0; i < align_bit_1; i++) {
        lbcLine_StmWrBit(bs, 0, 1024);
    }
    lbcLine_StmWrBit(bs, 0, align_bit_2);
    para->frm_bits += para->line_tar_bits[para->frm_y % 2];
}

static void lbcLine_Enc(VI_DMA_LBC_PARAM_S *para, VI_DMA_LBC_BS_S *bs, unsigned int frm_x)
{
    unsigned int i = 0;
    unsigned int bits = 0;

    if (NULL == para || NULL == bs) {
        return;
    }

    if (para->frm_y % 2 == 0) {
        lbcLine_StmWrBit(bs, 1, 2); //mode-dts
        if (para->frm_x == 0) { //qp_code
            lbcLine_StmWrBit(bs, 0, 3);
            bits = 3;
        } else {
            lbcLine_StmWrBit(bs, 1, 1);
            bits = 1;
        }
        lbcLine_StmWrBit(bs, 0, 3);
        para->line_bits += bits + 5;
    } else {
        for (i = 0; i < 2; i++) {
            lbcLine_StmWrBit(bs, 1, 2); //mode-dts
            if (i == 1) { //qp_code
                lbcLine_StmWrBit(bs, 0, 1);
                bits = 1;
            } else {
                if (para->frm_x == 0) { //qp_code
                    lbcLine_StmWrBit(bs, 0, 3);
                    bits = 3;
                } else {
                    lbcLine_StmWrBit(bs, 1, 1);
                    bits = 1;;
                }
            }
            lbcLine_StmWrBit(bs, 0, 3);
            para->line_bits += bits + 5;
        }
    }
}

static unsigned int lbcLine_Cmp(VI_DMA_LBC_CFG_S *cfg, VI_DMA_LBC_STM_S *stm)
{
    VI_DMA_LBC_PARAM_S stLbcPara;
    VI_DMA_LBC_BS_S stLbcBs;
    unsigned int frm_x = 0;
    unsigned int frm_bits = 0;

    if (NULL == cfg || NULL == stm) {
        aloge("fatal error! null pointer");
        return 0;
    }

    memset(&stLbcPara, 0, sizeof(VI_DMA_LBC_PARAM_S));
    memset(&stLbcBs, 0, sizeof(VI_DMA_LBC_BS_S));

    lbcLine_ParaInit(&stLbcPara, cfg);
    lbcLine_StmInit(&stLbcBs, stm->bs);

    for (stLbcPara.frm_y = 0; stLbcPara.frm_y < stLbcPara.frm_hgt; stLbcPara.frm_y = stLbcPara.frm_y + 1) {
        stLbcPara.line_bits = 0;
        for (stLbcPara.frm_x = 0; stLbcPara.frm_x < stLbcPara.frm_wth; stLbcPara.frm_x = stLbcPara.frm_x + stLbcPara.mb_wth) {
            lbcLine_Enc(&stLbcPara, &stLbcBs, stLbcPara.frm_x);
        }
        lbcLine_Align(&stLbcPara, &stLbcBs);
    }
    frm_bits = (stLbcPara.frm_bits + 7) / 8;

    return frm_bits;
}
#endif

/**
  VIPP配置frameBuffer的宽高需要考虑视频编码器venc的硬件特性，说明如下：
  硬件编码器VE读取内存的一些行为，导致对frameBuffer的宽高，字节总长有内存扩展的要求。

  VE的内存读取扩展对编码结果有无影响和有影响的区分：

  (1)纯粹的多读一点，再内部丢弃掉，就是无影响的。
  (2)认为多读的是有效数据，会干扰最终编码效果，就是有影响的。

  Y-stride和图像宽度的关系：图像宽度<=Y-stride，VE硬件读取一行只读到图像宽度，然后跳到Y-stride处。

  下面介绍VE的一些硬件特性，这些特性将导致对framebuffer宽高、字节总长有扩展要求。也伴随介绍vin驱动和venc驱动的处理方式。

  1. VE的stride寄存器有Y-stride和uv-stride。Y-stride寄存器单位是16像素，即要求右移4个比特再填。uv-stride寄存器单位是8像
     素，即要求右移3个比特再填。
     目前Y分量一个字节就是一个像素，所以这就要求Y-Stride是16字节对齐。uv-stride寄存器是右移3个比特再填，所以uv-stride数
     值8对齐即可。
     isp_set_bk_width_stride()配置vin驱动主动扩展stride到16对齐。
     实例：笔记本屏幕1366x768，投屏项目要求编码器编码1366x768，那么编码器就要求buffer的宽高必须是1376x768，stride是1376
     ，width是1366。vin驱动必须按照这个规格填写framebuffer。VE的stride的行为是：读取一行只要达到或超过配置的图像宽度(读
     取周期最小为64字节，所以可能读超过图像宽度)，就直接跳到stride处。
     mpi_venc和mpi_vi组件约定stride必须16对齐，各自按照这个约定配置驱动。

  2. VE的encpp图像宽高的寄存器的单位是8像素，所以要求像素值右移3个比特再填。
     这就要求设置的图像宽高必须都是8个像素对齐。因为venc会读到！宽度如果有y-stride的保证就一定满足条件，高度需要注意。
     以高度为例，如果图像宽高是1920x1078，frameBuffer内的图像宽高必须扩展到1920x1080的大小，这个内存读取扩展是有影响的，
     因为最后两行是会被VE读取从而参与编码，所以为保证编码质量，最好拷贝1078行的数据到1079行和1080行。这个工作只能由软件
     完成。
     再以宽度为例，如果图像宽高是1918x1080，frameBuffer内的图像宽高必须扩展到1920x1080的大小，这个内存读取扩展是有影响的
     ，因为最后两列数据也会被VE读取从而参与编码。
     如果想精确的把编码输出的图像变为用户设定的值如1920x1078或1918x1080，VE编码驱动设计了接口VENC_IndexParamForceConfWin
     ，该接口把设置的区域坐标、宽高值写入spspps。这样VencInit()填写baseConfig.nInputWidth和baseConfig.nInputHeight时就可
     以按8对齐写，编码的区域就可以大一些，但spspps精确指定了图像区域，就不影响解码了。

     v853的VE的encpp的硬件bug和软件弥补：
     要求输入buffer高度8对齐。如果叠加Overlay等，要求图像高度16对齐，这个高度扩展到16对齐的操作是encpp硬件自己处理，不会
     多读，预期设计规格是对外部输入buffer的高度的要求是8对齐就可以。但是因为encpp的新通路sharp的内部扩展到高度16对齐的操
     作的处理没做好，扩展的行是用零填充的，导致编码的图像底部有绿边(如果扩展的行是用最后一个有效行的数据复制填充，就不会
     有绿边问题了)。软件为了解决这个bug，只能把内存的高度扩展到16对齐，再自己用最后一有效行复制填充。为了让VE硬件去多读
     扩展的行，还需要将设置给VE硬件的高度的值改为16对齐。

     v833的VE的encpp的硬件bug和软件弥补：
     encpp的图像宽高寄存器的单位是8像素，宽高8对齐即可。但是VE内部会将高度扩展到16对齐，而V833的VE不会内部处理，而是真的
     读到了16对齐的行数，导致读越界。这就要求frameBuffer的高度按照16对齐去分配。
     该问题在V853修复了，V853的VE读行数只读到8对齐，如果要扩展到16对齐，VE内部处理多读的行，而且用最后一个有效行的数据复
     制到多读的行。但是V853的encpp的新通路sharp的扩展行的数值填充并未这样做，而是直接填充零。
     最后为了绕开这个bug，如果使用encpp新通路的sharp，外部分配内存还是必须16对齐。所以为了统一，v853分配内存，仍然高度要
     求16对齐。

  3. VE一次读取周期读取的字节数是64字节。

     这意味着读取一行时，是有可能超出该行的stride(如果图像宽度刚好等于stride，而宽度又不是64字节的整数倍)而多读一点。但
     硬件内部会去掉多读的字节，然后跳到stride处，所以这个内存读取扩展无影响。但外部分配内存就要根据VE是否会读越界而适当
     多分配一点。注意，y,u,v都是一个周期读取64字节。LBC格式，VE一次也是读取64个字节，但为安全，延长了1024字节。
     isp_set_bk_buffer_align()是配置vin驱动增加bufferSize的接口，理论上总长增加64字节即可，实际增加1024字节。

  4. 如果是在线编码，frameBuffer只能配一个地址给VE寄存器。

     那么如果是NV21格式，就需要进行UV地址的推算，目前v853约定宽度stride按16对齐，高度16对齐。vin驱动按照宽高16对齐去分配
     和填写framebuffer的yuv数据。venc驱动也必须按照这个规则去推算yuv的起始地址才能读取正确。LBC是有损压缩格式，没有yuv的
     区分，故不考虑这个。

  5. V853内部编码h264是16x16，h265是32x32，但是像素扩展是驱动和硬件自己做的，所以外部输入buffer还是满足stride的16对齐和
     高度8对齐就可以。

  6. 使用iommu分配内存，内部的分配基本单位是页(4096字节)，所以实际分配的长度是4096对齐的。但告诉用户的是用户申请的长度。
*/
ERRORTYPE videoInputHw_SetChnAttr(VI_DEV ViCh, VI_ATTR_S *pstAttr) /*Set /dev/video[0~3] node attr*/
{
    struct isp_video_device *video = NULL;
    struct video_fmt vfmt;
    if (ViCh >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[ViCh]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", ViCh);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = gpVIDevManager->media->video_dev[ViCh];
    }
    viChnManager *pVipp = gpVIDevManager->gpVippManager[ViCh];
    memcpy(&pVipp->mstAttr, pstAttr, sizeof(VI_ATTR_S));

    MediaDebugLoadMppViParams(pVipp, NULL);

    /* Currently only vipp0 supports online. */
    if ((pVipp->mstAttr.mOnlineEnable) && (0 != ViCh)) {
        aloge("fatal error! Exception Case: only vipp0 supports online, vipp[%d] is not support online!", ViCh);
        return ERR_VI_INVALID_PARA;
    }

    if (pVipp->mstAttr.large_dma_merge_en >= DMA_STITCH_2IN1_LINNER) {
        if (pVipp->mstAttr.format.pixelformat == V4L2_PIX_FMT_LBC_1_0X ||
            pVipp->mstAttr.format.pixelformat == V4L2_PIX_FMT_LBC_1_5X ||
            pVipp->mstAttr.format.pixelformat == V4L2_PIX_FMT_LBC_2_0X ||
            pVipp->mstAttr.format.pixelformat == V4L2_PIX_FMT_LBC_2_5X) {
                aloge("DMA STITCH Mode is not support LBC mode!!!\n");
                return FAILURE;
            }
    }

    memset(&vfmt, 0, sizeof(vfmt));
    vfmt.type = pVipp->mstAttr.type;
    vfmt.memtype = pVipp->mstAttr.memtype;
    vfmt.format = pVipp->mstAttr.format;

    if (V4L2_PIX_FMT_NV21 == vfmt.format.pixelformat || V4L2_PIX_FMT_NV12 == vfmt.format.pixelformat ||
        V4L2_PIX_FMT_YUV420 == vfmt.format.pixelformat || V4L2_PIX_FMT_YVU420 == vfmt.format.pixelformat ||
        V4L2_PIX_FMT_NV61 == vfmt.format.pixelformat || V4L2_PIX_FMT_NV16 == vfmt.format.pixelformat)
    {
        unsigned int width_stride = AWALIGN(vfmt.format.width, 16);
        unsigned int width_stride_64align = AWALIGN(width_stride, 64);
        alogv("vipp[%d] width_stride:%d, width_stride_64align:%d", ViCh, width_stride, width_stride_64align);
        if (width_stride != width_stride_64align) {
            struct bk_buffer_align bk_align;
            memset(&bk_align, 0, sizeof(struct bk_buffer_align));
            bk_align.yuv_align_en = 1;
            isp_set_bk_buffer_align(video, &bk_align);
        }

        if ((vfmt.format.width%16) != 0) {
            unsigned char width_stride_en = (0 == pVipp->mstAttr.mbWidthStrideDisable) ? 1 : 0;
            alogd("vipp[%d] yuv format, width %d is not 16 aligned, set bk width stride en %d", ViCh, vfmt.format.width, width_stride_en);
            isp_set_bk_width_stride(video, width_stride_en);
        }
        if ((vfmt.format.width%4) != 0) //vipp output width must 4 align! if not, border will wrong.
        {
            vfmt.format.width = AWALIGN(pVipp->mstAttr.format.width, 4);
            alogd("vipp[%d] resolution reduce, width needs to be 4 aligned, %d->%d", ViCh, pVipp->mstAttr.format.width, vfmt.format.width);
        }
        if ((vfmt.format.height%8) != 0)
        {
            alogw("Be careful! vipp[%d] height[%d] is not 8 aligned, if not send to venc, please ignore it.", ViCh, pVipp->mstAttr.format.height);
        }
    }

#if (AWCHIP == AW_V853)
    if (TRUE == pVipp->mstAttr.mbEncppEnable) {
        if (pVipp->mstAttr.mOnlineEnable) {
            vfmt.format.width = AWALIGN(pVipp->mstAttr.format.width, VIN_ALIGN_WIDTH);
            vfmt.format.height = AWALIGN(pVipp->mstAttr.format.height, VIN_ALIGN_HEIGHT);
            alogd("ViCh[%d] update width:%d(%d), height:%d(%d)", ViCh, pVipp->mstAttr.format.width, vfmt.format.width, pVipp->mstAttr.format.height, vfmt.format.height);
        }

        if (V4L2_PIX_FMT_LBC_1_0X == vfmt.format.pixelformat ||
            V4L2_PIX_FMT_LBC_1_5X == vfmt.format.pixelformat ||
            V4L2_PIX_FMT_LBC_2_0X == vfmt.format.pixelformat ||
            V4L2_PIX_FMT_LBC_2_5X == vfmt.format.pixelformat) {

            struct bk_buffer_align bk_align;
            memset(&bk_align, 0, sizeof(struct bk_buffer_align));
            bk_align.lbc_align_en = 1;
            isp_set_bk_buffer_align(video, &bk_align);

            lbcMode_Select(&pVipp->mLbcCmp, vfmt.format.pixelformat);
            int wth = AWALIGN(pVipp->mstAttr.format.width, 32);
            if (pVipp->mLbcCmp.is_lossy) {
                pVipp->mLbcCmp.line_tar_bits[0] = AWALIGN(pVipp->mLbcCmp.cmp_ratio_even * wth * pVipp->mLbcCmp.bit_depth/1000, 512);
                pVipp->mLbcCmp.line_tar_bits[1] = AWALIGN(pVipp->mLbcCmp.cmp_ratio_odd * wth * pVipp->mLbcCmp.bit_depth/500, 512);
            } else {
                pVipp->mLbcCmp.line_tar_bits[0] = AWALIGN(wth * pVipp->mLbcCmp.bit_depth * 1 + (wth * 1 / 16 * 2), 512);
                pVipp->mLbcCmp.line_tar_bits[1] = AWALIGN(wth * pVipp->mLbcCmp.bit_depth * 2 + (wth * 2 / 16 * 2), 512);
            }
            alogd("ViCh[%d] LBC pix:0x%x, line_tar_bits[0]:%d, line_tar_bits[1]:%d", ViCh, vfmt.format.pixelformat, pVipp->mLbcCmp.line_tar_bits[0], pVipp->mLbcCmp.line_tar_bits[1]);

            VI_DMA_LBC_CFG_S stLbcCfg;
            VI_DMA_LBC_STM_S stLbcStm;
            memset(&stLbcCfg, 0, sizeof(VI_DMA_LBC_CFG_S));
            stLbcCfg.frm_wth = pVipp->mstAttr.format.width;
            stLbcCfg.frm_hgt = AWALIGN(pVipp->mstAttr.format.height, VIN_ALIGN_HEIGHT) - pVipp->mstAttr.format.height;
            stLbcCfg.line_tar_bits[0] = pVipp->mLbcCmp.line_tar_bits[0];
            stLbcCfg.line_tar_bits[1] = pVipp->mLbcCmp.line_tar_bits[1];

            memset(&stLbcStm, 0, sizeof(VI_DMA_LBC_STM_S));
            unsigned int bs_len = stLbcCfg.frm_wth * stLbcCfg.frm_hgt * 4;
            stLbcStm.bs = (unsigned char*)malloc(bs_len);
            if (NULL == stLbcStm.bs) {
                aloge("fatal error! malloc stLbcStm.bs failed! size=%d", bs_len);
                return ERR_VI_NOMEM;
            }
            memset(stLbcStm.bs, 0, bs_len);

            unsigned int frm_bit = lbcLine_Cmp(&stLbcCfg, &stLbcStm);
            alogd("bs_len:%d, frm_bit:%d", bs_len, frm_bit);
            if (frm_bit > bs_len) {
                aloge("fatal error! wrong frm_bit:%d > bs_len:%d", frm_bit, bs_len);
                if (stLbcStm.bs) {
                    free(stLbcStm.bs);
                    stLbcStm.bs = NULL;
                }
                return ERR_VI_INVALID_PARA;
            }

            if (pVipp->mLbcFillDataAddr) {
                alogw("LbcFillDataAddr %p is not NULL! free it before malloc.", pVipp->mLbcFillDataAddr);
                free(pVipp->mLbcFillDataAddr);
                pVipp->mLbcFillDataAddr = NULL;
            }

            pVipp->mLbcFillDataLen = frm_bit;
            pVipp->mLbcFillDataAddr = (unsigned char*)malloc(pVipp->mLbcFillDataLen);
            if (NULL == pVipp->mLbcFillDataAddr) {
                aloge("fatal error! malloc LbcFillDataAddr failed! size=%d", pVipp->mLbcFillDataLen);
                if (stLbcStm.bs) {
                    free(stLbcStm.bs);
                    stLbcStm.bs = NULL;
                }
                return ERR_VI_NOMEM;
            }
            memcpy(pVipp->mLbcFillDataAddr, stLbcStm.bs, pVipp->mLbcFillDataLen);

            /*FILE *bs_data_file = fopen("./lbc101.bin", "wb");
            fwrite(pVipp->mLbcFillDataAddr, pVipp->mLbcFillDataLen, 1, bs_data_file);
            fclose(bs_data_file);*/

            if (stLbcStm.bs) {
                free(stLbcStm.bs);
                stLbcStm.bs = NULL;
            }
        }
    } else {
        alogd("ViCh[%d], user set disable Encpp", ViCh);
    }
#endif

    vfmt.nbufs = pVipp->mstAttr.nbufs;
    vfmt.nplanes = pVipp->mstAttr.nplanes;
    vfmt.fps = pVipp->mstAttr.fps;
    vfmt.capturemode = pVipp->mstAttr.capturemode; // V4L2_MODE_VIDEO
    vfmt.use_current_win = pVipp->mstAttr.use_current_win;
    vfmt.drop_frame_num  = (pVipp->mstAttr.drop_frame_num >= 0) ? pVipp->mstAttr.drop_frame_num : 0;
    if ((0 == pVipp->mstAttr.wdr_mode) ||
        (1 == pVipp->mstAttr.wdr_mode) ||
        (2 == pVipp->mstAttr.wdr_mode)) {
        vfmt.wdr_mode = pVipp->mstAttr.wdr_mode; /*0:normal; 1:DOL; 2:sensor commanding*/
    } else {
        vfmt.wdr_mode = 0; /* defaule value : 0 */
    }
    /* Set OnlineShareBufNum must be in the case of online enabled. */
    vfmt.ve_online_en = pVipp->mstAttr.mOnlineEnable;
    if (pVipp->mstAttr.mOnlineEnable) {
        if ((BK_TWO_BUFFER < pVipp->mstAttr.mOnlineShareBufNum) || (BK_MUL_BUFFER >= pVipp->mstAttr.mOnlineShareBufNum)) {
            vfmt.dma_buf_num = BK_ONE_BUFFER;
            alogw("user set OnlineShareBufNum=%d is invalid value, reset it to 1.", pVipp->mstAttr.mOnlineShareBufNum);
        } else {
            vfmt.dma_buf_num = pVipp->mstAttr.mOnlineShareBufNum;
        }
    } else {
        vfmt.dma_buf_num = BK_MUL_BUFFER;
    }

    vfmt.pixel_num = pVipp->mstAttr.pixel_num;
    vfmt.tdm_speed_down_en = pVipp->mstAttr.tdm_speed_down_en;

    vfmt.video_selection_en = pVipp->mstAttr.mCropCfg.bEnable;
    vfmt.rect.left = pVipp->mstAttr.mCropCfg.Rect.X;
    vfmt.rect.top = pVipp->mstAttr.mCropCfg.Rect.Y;
    vfmt.rect.width = pVipp->mstAttr.mCropCfg.Rect.Width;
    vfmt.rect.height = pVipp->mstAttr.mCropCfg.Rect.Height;
    vfmt.tdm_rxbuf_cnt = pVipp->mstAttr.tdm_rxbuf_cnt;
    vfmt.large_dma_merge_en = pVipp->mstAttr.large_dma_merge_en;
    memcpy(&vfmt.scaler_cfg, &pVipp->mstAttr.mStitchModeSensorScalerCfg, sizeof(struct dma_merge_scaler_cfg));

    if (video_set_fmt(video, &vfmt) < 0) {
        aloge("video set_fmt failed, chn[%d]", ViCh);
        return FAILURE;
    }
    if (vfmt.format.colorspace != video->format.colorspace)
    {
        alogw("Be careful! vipp[%d] colorspace change [%d->%d]", ViCh, vfmt.format.colorspace, video->format.colorspace);
    }
    if (vfmt.nplanes != video->nplanes)
    {
        alogw("Be careful! vipp[%d] pixelFormat[0x%x] nplanes change [%d->%d]", ViCh, vfmt.format.pixelformat, vfmt.nplanes,
            video->nplanes);
    }

    return SUCCESS;
}

int videoInputHw_SetVIFreq(VI_DEV ViCh, int freq)
{
    struct isp_video_device *video = NULL;
    struct video_fmt vfmt;
   //if (ViCh >= HW_VIDEO_DEVICE_NUM || NULL == media->video_dev[ViCh]) {
   //    ISP_ERR("VIN CH[%d] number is invalid!\n", ViCh);
   //     return ERR_VI_INVALID_CHNID;
   // } else {
    //    video = media->video_dev[ViCh];
   // }
   pthread_mutex_lock(&gpVIDevManager->mManagerLock);

    if(gpVIDevManager->mClockFrequency != freq)
    {
        int video_dev_index;
        for(video_dev_index = 0; video_dev_index < HW_VIDEO_DEVICE_NUM; ++video_dev_index )
        {
            if(gpVIDevManager->media->video_dev[video_dev_index])
            {
                video = gpVIDevManager->media->video_dev[video_dev_index];
                if (video_set_top_clk(video, freq) < 0)
                {
                    aloge("Cuation:can not set ISP clock frequency!");
                    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
                    return FAILURE;
                }
                alogw("the isp clock freq had been set %f MHz", freq / 1000000.0);
                gpVIDevManager->mSetFrequency = TRUE;
                gpVIDevManager->mClockFrequency = freq;
                pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
                return SUCCESS;
            }
        }
        gpVIDevManager->mSetFrequency = FALSE;
        gpVIDevManager->mClockFrequency = freq;
        alogw("The Device do not open, and waitting to set the isp clock freq");
    }
    else
    {
        alogw("The isp clock frequency same as you wanted, the freq is %d MHz!!", gpVIDevManager->mClockFrequency / 1000000);
    }

    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
    return 0;//
}

ERRORTYPE videoInputHw_GetChnAttr(VI_DEV ViCh, VI_ATTR_S *pstAttr) /*Get /dev/video[0~3] node attr*/
{
    struct isp_video_device *video = NULL;
    struct video_fmt vfmt;
    if (ViCh >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[ViCh]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", ViCh);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = gpVIDevManager->media->video_dev[ViCh];
    }

    memset(&vfmt, 0, sizeof(vfmt));
    video_get_fmt(video, &vfmt);
    pstAttr->type = vfmt.type;
    pstAttr->memtype = vfmt.memtype;
    pstAttr->format = vfmt.format;
    pstAttr->nbufs = vfmt.nbufs;
    pstAttr->nplanes = vfmt.nplanes;
    pstAttr->fps = vfmt.fps;
    pstAttr->capturemode = vfmt.capturemode;
    pstAttr->use_current_win = vfmt.use_current_win;
    pstAttr->wdr_mode = vfmt.wdr_mode;
    pstAttr->drop_frame_num = vfmt.drop_frame_num;
    pstAttr->mOnlineEnable = vfmt.ve_online_en;
    pstAttr->mOnlineShareBufNum = vfmt.dma_buf_num;
    pstAttr->tdm_rxbuf_cnt = vfmt.tdm_rxbuf_cnt;

    return SUCCESS;
}

ERRORTYPE videoInputHw_ChnEnable(int ViVipp) /*Enable /dev/video[0~3] node*/
{
    ERRORTYPE rc;
    int ret;
    if (ViVipp >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[ViVipp])
    {
        aloge("VIN CH[%d] number is invalid!", ViVipp);
        return ERR_VI_INVALID_CHNID;
    }

    message_t msg;
    InitMessage(&msg);
    msg.command = VVideoInputHw_EnableVipp;
    msg.para0 = ViVipp;
    msg.pReply = ConstructMessageReply();
    putMessageWithData(&gpVIDevManager->mCmdQueue, &msg);
    while(1)
    {
        ret = cdx_sem_down_timedwait(&msg.pReply->ReplySem, 5000);
        if(ret != 0)
        {
            aloge("fatal error! wait enable vipp[%d] fail[0x%x]", ViVipp, ret);
        }
        else
        {
            break;
        }
    }
    rc = (ERRORTYPE)msg.pReply->ReplyResult;
    alogd("receive enable vipp[%d] reply: 0x%x!", ViVipp, rc);
    DestructMessageReply(msg.pReply);
    msg.pReply = NULL;
    return rc;
}

ERRORTYPE videoInputHw_ChnDisable(int ViVipp) /*Disable /dev/video[0~3] node*/
{
    ERRORTYPE rc;
    int ret;

    if (ViVipp >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[ViVipp])
    {
        aloge("VIN CH[%d] number is invalid!\n", ViVipp);
        return ERR_VI_INVALID_CHNID;
    }

    message_t msg;
    InitMessage(&msg);
    msg.command = VVideoInputHw_DisableVipp;
    msg.para0 = ViVipp;
    msg.pReply = ConstructMessageReply();
    putMessageWithData(&gpVIDevManager->mCmdQueue, &msg);
    while(1)
    {
        ret = cdx_sem_down_timedwait(&msg.pReply->ReplySem, 5000);
        if(ret != 0)
        {
            aloge("fatal error! wait disable vipp[%d] fail[0x%x]", ViVipp, ret);
        }
        else
        {
            break;
        }
    }
    rc = (ERRORTYPE)msg.pReply->ReplyResult;
    alogd("receive disable vipp[%d] reply: 0x%x!", ViVipp, rc);
    DestructMessageReply(msg.pReply);
    msg.pReply = NULL;
    return rc;
}

ERRORTYPE videoInputHw_GetIspDev(VI_DEV Vipp_id, ISP_DEV *pIspDev)
{
    int nIspId = 0;
    *pIspDev = MM_INVALID_DEV;
    if (gpVIDevManager->gpVippManager[Vipp_id] == NULL) {
        return FAILURE;
    }

    nIspId = video_to_isp_id(gpVIDevManager->media->video_dev[Vipp_id]);
    if (nIspId < 0)
    {
        aloge("fatal error! ISP ID[%d] number is invalid!\n", nIspId);
        return ERR_VI_INVALID_CHNID;
    }
    if (nIspId >= MAX_ISP_DEV_NUM)
    {
        //alogd("Be careful! IspId[%d] may be dvp isp", nIspId);
    }
    *pIspDev = nIspId;

    return SUCCESS;
}

/**
  get vippId from ispDev.
  @return
    MM_INVALID_DEV: fail
    >=0: success
*/
VI_DEV videoInputHw_GetVippByIspDev(ISP_DEV nIspDev)
{
    int i = -1;
    int isp_id;
    int found = 0;
    struct isp_video_device *video = NULL;
    if ((nIspDev < 0) || (nIspDev >= MAX_ISP_DEV_NUM))
    {
        aloge("fatal error! ISP ID[%d] number is invalid!", nIspDev);
        return MM_INVALID_DEV;
    }
    else
    {
        for (i = 0; i < HW_VIDEO_DEVICE_NUM; i++)
        {
            video = gpVIDevManager->media->video_dev[i];
            if (NULL == video)
            {
                continue;
            }
            isp_id = video_to_isp_id(gpVIDevManager->media->video_dev[i]);
            if (isp_id == nIspDev)
            {
                //alogd("isp[%d]-vipp[%d]", nIspDev, i);
                found = 1;
                break;
            }
        }
    }
    if (0 == found)
    {
        aloge("fatal error! No find video open @ isp[%d].", nIspDev);
        return MM_INVALID_DEV;
    }
    else
    {
        return i;
    }
}

/**
  get video device from ispDev.
  @return
    NULL: fail
    video: success
*/
static struct isp_video_device* GetVideoByIspDev(int nIspId)
{
    struct isp_video_device *video = NULL;

    VI_DEV nVippId = videoInputHw_GetVippByIspDev(nIspId);
    if ((nVippId < 0) || (NULL == gpVIDevManager->media->video_dev[nVippId]))
    {
        aloge("fatal error! vipp[%d] is invalid!", nVippId);
        return NULL;
    }
    else
    {
        video = gpVIDevManager->media->video_dev[nVippId];
    }

    return video;
}

ERRORTYPE videoInputHw_SetCrop(VI_DEV Vipp_id, const VI_CROP_CFG_S *pCropCfg)
{
    struct isp_video_device *video = NULL;

    if (Vipp_id >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[Vipp_id]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", Vipp_id);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = gpVIDevManager->media->video_dev[Vipp_id];
    }
    viChnManager *pVipp = gpVIDevManager->gpVippManager[Vipp_id];

    if (TRUE == pCropCfg->bEnable) {
        alogd("vipp[%d] set crop X:%d, Y:%d, Width:%d, Height:%d", Vipp_id,
            pCropCfg->Rect.X, pCropCfg->Rect.Y, pCropCfg->Rect.Width, pCropCfg->Rect.Height);
        struct video_selection_rect rect;
        memset(&rect, 0, sizeof(struct video_selection_rect));
        rect.left = pCropCfg->Rect.X;
        rect.top = pCropCfg->Rect.Y;
        rect.width = pCropCfg->Rect.Width;
        rect.height = pCropCfg->Rect.Height;
        int ret = video_set_selection(video, &rect);
        if (ret) {
            aloge("vipp[%d] set selection failed! ret=%d", Vipp_id, ret);
            return FAILURE;
        }
    } else {
        alogd("vipp[%d] disable crop", Vipp_id);
    }
    memcpy(&pVipp->mstAttr.mCropCfg, pCropCfg, sizeof(VI_CROP_CFG_S));

    return 0;
}
ERRORTYPE videoInputHw_GetCrop(VI_DEV Vipp_id, VI_CROP_CFG_S *pCropCfg)
{
    struct isp_video_device *video = NULL;

    if (Vipp_id >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[Vipp_id]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", Vipp_id);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = gpVIDevManager->media->video_dev[Vipp_id];
    }
    viChnManager *pVipp = gpVIDevManager->gpVippManager[Vipp_id];
    memcpy(pCropCfg, &pVipp->mstAttr.mCropCfg, sizeof(VI_CROP_CFG_S));

    return 0;
}
ERRORTYPE videoInputHw_SetSyncCtrl(VI_DEV Vipp_id, const struct csi_sync_ctrl *sync)
{
    struct isp_video_device *video = NULL;

    if (Vipp_id >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[Vipp_id]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", Vipp_id);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = gpVIDevManager->media->video_dev[Vipp_id];
    }

    return video_set_sync_ctrl(video, sync);
}

/*
ERRORTYPE videoInputHw_SetOsdMaskRegion(int *pvipp_id, VI_OsdMaskRegion *pstOsdMaskRegion)
{
    int ViCh = *pvipp_id;
    VI_OsdMaskRegion *pOsdMaskRegion = pstOsdMaskRegion;

    struct isp_video_device *video = NULL;
    if (ViCh >= HW_VIDEO_DEVICE_NUM || NULL == media->video_dev[ViCh]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", ViCh);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = media->video_dev[ViCh];
    }

    if (overlay_set_fmt(video, (struct osd_fmt *)pOsdMaskRegion) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_UpdateOsdMaskRegion(int *pvipp_id, int onoff)
{
    int ViCh = *pvipp_id;

    struct isp_video_device *video = NULL;
    if (ViCh >= HW_VIDEO_DEVICE_NUM || NULL == media->video_dev[ViCh]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", ViCh);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = media->video_dev[ViCh];
    }

    if (overlay_update(video, onoff) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}
*/

#define MAX_GLOBAL_ALPHA (16)
//ref to vipp_reg.h
#if(AWCHIP == AW_V5)
  #define MAX_OVERLAY_NUM 64
  #define MAX_COVER_NUM 8
#elif(AWCHIP == AW_V316)
  #define MAX_OVERLAY_NUM 8
  #define MAX_COVER_NUM 8
#elif(AWCHIP == AW_V459 || AWCHIP == AW_V853)
  #define MAX_OVERLAY_NUM 0
  #define MAX_COVER_NUM 0
  #define MAX_ORL_NUM 16
#else
  #define MAX_OVERLAY_NUM 64
  #define MAX_COVER_NUM 8
#endif

#define OVERLAY_INVERT_UNIT_WIDTH (16)
#define OVERLAY_INVERT_UNIT_HEIGHT (16)

ERRORTYPE videoInputHw_DrawOSD(VI_DEV vipp_id)
{
    ERRORTYPE ret = SUCCESS;
    viChnManager *pVipp = gpVIDevManager->gpVippManager[vipp_id];
    struct isp_video_device *video = gpVIDevManager->media->video_dev[vipp_id];
    //draw overlay
    if(!list_empty(&pVipp->mOverlayList))
    {
        struct osd_fmt stOsdFmt;
        memset(&stOsdFmt, 0, sizeof(struct osd_fmt));
        ChannelRegionInfo *pEntry;
        list_for_each_entry(pEntry, &pVipp->mOverlayList, mList)
        {
            if(pEntry->mbDraw)
            {
                if(FALSE == pEntry->mbSetBmp)
                {
                    aloge("fatal error! bmp is not set");
                }
                if(NULL == pEntry->mBmp.mpData)
                {
                    aloge("fatal error! bmpData is not set");
                }
                stOsdFmt.chromakey = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt);
                switch(stOsdFmt.chromakey)
                {
                    case V4L2_PIX_FMT_RGB32:
                        stOsdFmt.global_alpha = MAX_GLOBAL_ALPHA;
                        break;
                    case V4L2_PIX_FMT_RGB555:
                        stOsdFmt.global_alpha = (unsigned int)pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha * MAX_GLOBAL_ALPHA/128;
                        break;
                    default:
                        aloge("fatal error! unkown pix fmt [0x%x]->[0x%x]", pEntry->mRgnAttr.unAttr.stOverlay.mPixelFmt, stOsdFmt.chromakey);
                        stOsdFmt.global_alpha = MAX_GLOBAL_ALPHA;
                        break;
                }
                if(pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width%OVERLAY_INVERT_UNIT_WIDTH != 0
                    || pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height%OVERLAY_INVERT_UNIT_HEIGHT != 0)
                {
                    aloge("fatal error! InvColArea[%dx%d] is not align to [%dx%d]",
                        pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width,
                        pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height,
                        OVERLAY_INVERT_UNIT_WIDTH, OVERLAY_INVERT_UNIT_HEIGHT);
                }
                if(stOsdFmt.clipcount >= 8)
                {
                    aloge("fatal error! why elem number[%d] >= 8?", stOsdFmt.clipcount);
                }
                stOsdFmt.inv_w_rgn[stOsdFmt.clipcount] = pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width/OVERLAY_INVERT_UNIT_WIDTH - 1;
	            stOsdFmt.inv_h_rgn[stOsdFmt.clipcount] = pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height/OVERLAY_INVERT_UNIT_HEIGHT - 1;
                stOsdFmt.inv_th = pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh;
                stOsdFmt.reverse_close[stOsdFmt.clipcount] = pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn==TRUE?0:1;
                if(pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn
                    && pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod != LESSTHAN_LUMDIFF_THRESH)
                {
                    alogd("Be careful! vipp invert color mode only support LESSTHAN_LUMDIFF_THRESH! But user set mode[0x%x]", pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod);
                }
                stOsdFmt.bitmap[stOsdFmt.clipcount] = pEntry->mBmp.mpData;
                stOsdFmt.region[stOsdFmt.clipcount].left = pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X;
                stOsdFmt.region[stOsdFmt.clipcount].top = pEntry->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y;
                stOsdFmt.region[stOsdFmt.clipcount].width = pEntry->mRgnAttr.unAttr.stOverlay.mSize.Width;
                stOsdFmt.region[stOsdFmt.clipcount].height = pEntry->mRgnAttr.unAttr.stOverlay.mSize.Height;
                stOsdFmt.clipcount++;
                if(stOsdFmt.clipcount > MAX_OVERLAY_NUM)
                {
                    aloge("fatal error! clipcount[%d] exceed! max overlay num:%d", stOsdFmt.clipcount, MAX_OVERLAY_NUM);
                }
            }
        }
        char bitmap[100];
        if(0 == stOsdFmt.clipcount)
        {
            bitmap[0] = 'c';
            stOsdFmt.bitmap[0] = &bitmap[0];
        }
        int ret1 = overlay_set_fmt(video, &stOsdFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set overlay fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! overlay update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    else
    {
        struct osd_fmt stOsdFmt;
        memset(&stOsdFmt, 0, sizeof(struct osd_fmt));
        char bitmap[100];
        bitmap[0] = 'c';
        stOsdFmt.clipcount = 0;
        stOsdFmt.bitmap[0] = &bitmap[0];
        int ret1 = overlay_set_fmt(video, &stOsdFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set overlay fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! overlay update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    //draw cover
    if(!list_empty(&pVipp->mCoverList))
    {
        struct osd_fmt stOsdFmt;
        memset(&stOsdFmt, 0, sizeof(struct osd_fmt));
        ChannelRegionInfo *pEntry;
        list_for_each_entry(pEntry, &pVipp->mCoverList, mList)
        {
            if(pEntry->mbDraw)
            {
                stOsdFmt.global_alpha = MAX_GLOBAL_ALPHA;
                stOsdFmt.rgb_cover[stOsdFmt.clipcount] = pEntry->mRgnChnAttr.unChnAttr.stCoverChn.mColor;
                stOsdFmt.bitmap[stOsdFmt.clipcount] = NULL;
                if(AREA_RECT == pEntry->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType)
                {
                    stOsdFmt.region[stOsdFmt.clipcount].left = pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.X;
                    stOsdFmt.region[stOsdFmt.clipcount].top = pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Y;
                    stOsdFmt.region[stOsdFmt.clipcount].width = pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Width;
                    stOsdFmt.region[stOsdFmt.clipcount].height = pEntry->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Height;
                    stOsdFmt.clipcount++;
                }
                else
                {
                    aloge("fatal error! coverType[0x%x] is not rect!", pEntry->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType);
                }
                if(stOsdFmt.clipcount > MAX_COVER_NUM)
                {
                    aloge("fatal error! clipcount[%d] exceed! max cover num:%d", stOsdFmt.clipcount, MAX_COVER_NUM);
                }
            }
        }
        int ret1 = overlay_set_fmt(video, &stOsdFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set cover fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! cover update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    else
    {
        struct osd_fmt stOsdFmt;
        memset(&stOsdFmt, 0, sizeof(struct osd_fmt));
        stOsdFmt.clipcount = 0;
        stOsdFmt.bitmap[0] = NULL;
        int ret1 = overlay_set_fmt(video, &stOsdFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set cover fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! cover update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    //draw ORL(Object Rectangle Label)
    if(!list_empty(&pVipp->mOrlList))
    {
        struct orl_fmt stOrlFmt;
        memset(&stOrlFmt, 0, sizeof(struct orl_fmt));
        ChannelRegionInfo *pEntry;
        list_for_each_entry(pEntry, &pVipp->mOrlList, mList)
        {
            if(pEntry->mbDraw)
            {
                stOrlFmt.mThick = pEntry->mRgnChnAttr.unChnAttr.stOrlChn.mThick/2;
                if(stOrlFmt.mThick <= 0)
                {
                    stOrlFmt.mThick = 1;
                }
                else if(stOrlFmt.mThick > 8)
                {
                    stOrlFmt.mThick = 8;
                }
                stOrlFmt.mRgbColor[stOrlFmt.clipcount] = pEntry->mRgnChnAttr.unChnAttr.stOrlChn.mColor;
                if(AREA_RECT == pEntry->mRgnChnAttr.unChnAttr.stOrlChn.enAreaType)
                {
                    stOrlFmt.region[stOrlFmt.clipcount].left = pEntry->mRgnChnAttr.unChnAttr.stOrlChn.stRect.X;
                    stOrlFmt.region[stOrlFmt.clipcount].top = pEntry->mRgnChnAttr.unChnAttr.stOrlChn.stRect.Y;
                    stOrlFmt.region[stOrlFmt.clipcount].width = pEntry->mRgnChnAttr.unChnAttr.stOrlChn.stRect.Width;
                    stOrlFmt.region[stOrlFmt.clipcount].height = pEntry->mRgnChnAttr.unChnAttr.stOrlChn.stRect.Height;
                    stOrlFmt.clipcount++;
                }
                else
                {
                    aloge("fatal error! Orl areaType[0x%x] is not rect!", pEntry->mRgnChnAttr.unChnAttr.stOrlChn.enAreaType);
                }

                if(stOrlFmt.clipcount >= MAX_ORL_NUM)
                {
                    aloge("fatal error! clipcount[%d] exceed! max orl num:%d", stOrlFmt.clipcount, MAX_ORL_NUM);
                    break;
                }
            }
        }
        int ret1 = orl_set_fmt(video, &stOrlFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set orl fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! orl update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    else
    {
        struct orl_fmt stOrlFmt;
        memset(&stOrlFmt, 0, sizeof(struct orl_fmt));
        stOrlFmt.clipcount = 0;
        int ret1 = orl_set_fmt(video, &stOrlFmt);
        int ret2 = overlay_update(video, 1);
        if(ret1 != 0)
        {
            aloge("fatal error! set cover fail[%d]", ret1);
            ret = ERR_VI_NOT_SUPPORT;
        }
        if(ret2 != 0)
        {
            aloge("fatal error! cover update fail[%d]", ret2);
            ret = ERR_VI_NOT_SUPPORT;
        }
    }
    return ret;
}

/**
 * @return true: first < second, false:first >= second
 */
static BOOL compareRegionPosition(const RGN_CHN_ATTR_S *pFirst, const RGN_CHN_ATTR_S *pSecond)
{
    if(pFirst->enType != pSecond->enType)
    {
        aloge("fatal error! why rgnType is not match[0x%x]!=[0x%x]", pFirst->enType, pSecond->enType);
        return FALSE;
    }
    if(OVERLAY_RGN == pFirst->enType)
    {
        if(pFirst->unChnAttr.stOverlayChn.stPoint.Y < pSecond->unChnAttr.stOverlayChn.stPoint.Y)
        {
            return TRUE;
        }
        if(pFirst->unChnAttr.stOverlayChn.stPoint.Y > pSecond->unChnAttr.stOverlayChn.stPoint.Y)
        {
            return FALSE;
        }
        if(pFirst->unChnAttr.stOverlayChn.stPoint.X < pSecond->unChnAttr.stOverlayChn.stPoint.X)
        {
            return TRUE;
        }
        if(pFirst->unChnAttr.stOverlayChn.stPoint.X > pSecond->unChnAttr.stOverlayChn.stPoint.X)
        {
            return FALSE;
        }
        return FALSE;
    }
    else if(COVER_RGN == pFirst->enType)
    {
        if(AREA_RECT == pFirst->unChnAttr.stCoverChn.enCoverType)
        {
            if(pFirst->unChnAttr.stCoverChn.stRect.Y < pSecond->unChnAttr.stCoverChn.stRect.Y)
            {
                return TRUE;
            }
            if(pFirst->unChnAttr.stCoverChn.stRect.Y > pSecond->unChnAttr.stCoverChn.stRect.Y)
            {
                return FALSE;
            }
            if(pFirst->unChnAttr.stCoverChn.stRect.X < pSecond->unChnAttr.stCoverChn.stRect.X)
            {
                return TRUE;
            }
            if(pFirst->unChnAttr.stCoverChn.stRect.X > pSecond->unChnAttr.stCoverChn.stRect.X)
            {
                return FALSE;
            }
            return FALSE;
        }
        else
        {
            aloge("fatal error! not support cover type[0x%x]", pFirst->unChnAttr.stCoverChn.enCoverType);
            return FALSE;
        }
    }
    else
    {
        aloge("fatal error! unsupport rgnType[0x%x]", pFirst->enType);
        return FALSE;
    }
}

/**
 * @return true: priority first < second, false:priority first >= second
 */
static BOOL compareRegionPriority(const RGN_CHN_ATTR_S *pFirst, const RGN_CHN_ATTR_S *pSecond)
{
    if(pFirst->enType != pSecond->enType)
    {
        aloge("fatal error! why rgnType is not match[0x%x]!=[0x%x]", pFirst->enType, pSecond->enType);
        return FALSE;
    }
    if(OVERLAY_RGN == pFirst->enType)
    {
        if(pFirst->unChnAttr.stOverlayChn.mLayer < pSecond->unChnAttr.stOverlayChn.mLayer)
        {
            return TRUE;
        }
        return FALSE;
    }
    else if(COVER_RGN == pFirst->enType)
    {
        if(AREA_RECT == pFirst->unChnAttr.stCoverChn.enCoverType)
        {
            if(pFirst->unChnAttr.stCoverChn.mLayer < pSecond->unChnAttr.stCoverChn.mLayer)
            {
                return TRUE;
            }
            return FALSE;
        }
        else
        {
            aloge("fatal error! not support cover type[0x%x]", pFirst->unChnAttr.stCoverChn.enCoverType);
            return FALSE;
        }
    }
    else if(ORL_RGN == pFirst->enType)
    {
        if(AREA_RECT == pFirst->unChnAttr.stOrlChn.enAreaType)
        {
            if(pFirst->unChnAttr.stOrlChn.mLayer < pSecond->unChnAttr.stOrlChn.mLayer)
            {
                return TRUE;
            }
            return FALSE;
        }
        else
        {
            aloge("fatal error! not support cover type[0x%x]", pFirst->unChnAttr.stOrlChn.enAreaType);
            return FALSE;
        }
    }
    else
    {
        aloge("fatal error! unsupport rgnType[0x%x]", pFirst->enType);
        return FALSE;
    }
}

BOOL checkRegionPositionValid(RGN_ATTR_S *pRgnAttr, const RGN_CHN_ATTR_S *pRgnChnAttr)
{
    BOOL bValid = TRUE;
#if(AWCHIP == AW_V5)
#elif(AWCHIP == AW_V316)
    switch(pRgnAttr->enType)
    {
        case OVERLAY_RGN:
        {
            if(pRgnAttr->unAttr.stOverlay.mSize.Width%OVERLAY_INVERT_UNIT_WIDTH != 0
                || pRgnAttr->unAttr.stOverlay.mSize.Height%OVERLAY_INVERT_UNIT_HEIGHT != 0
                || pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.X%OVERLAY_INVERT_UNIT_WIDTH != 0
                || pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.Y%OVERLAY_INVERT_UNIT_HEIGHT != 0)
            {
                aloge("fatal error! region position [%d,%d, %dx%d] is invalid!",
                    pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.X,pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.Y,
                    pRgnAttr->unAttr.stOverlay.mSize.Width, pRgnAttr->unAttr.stOverlay.mSize.Height);
                bValid = FALSE;
            }
            break;
        }
        case COVER_RGN:
            break;
        default:
        {
            aloge("fatal error! 0x%x don't support region type:%d!", AWCHIP, pRgnAttr->enType);
            bValid = FALSE;
            break;
        }
    }
#elif(AWCHIP == AW_V459 || AWCHIP == AW_V853)
    switch(pRgnChnAttr->enType)
    {
        case ORL_RGN:
        {
            if(pRgnChnAttr->unChnAttr.stOrlChn.stRect.X%2 != 0
                || pRgnChnAttr->unChnAttr.stOrlChn.stRect.Y%2 != 0
                || pRgnChnAttr->unChnAttr.stOrlChn.stRect.Width%2 != 0
                || pRgnChnAttr->unChnAttr.stOrlChn.stRect.Height%2 != 0)
            {
                //aloge("fatal error! region position [%d,%d, %dx%d] is invalid!",
                //    pRgnChnAttr->unChnAttr.stOrlChn.stRect.X, pRgnChnAttr->unChnAttr.stOrlChn.stRect.Y,
                //    pRgnChnAttr->unChnAttr.stOrlChn.stRect.Width, pRgnChnAttr->unChnAttr.stOrlChn.stRect.Height);
                //bValid = FALSE;
            }
            break;
        }
        default:
        {
            aloge("fatal error! 0x%x don't support region type:%d!", AWCHIP, pRgnChnAttr->enType);
            bValid = FALSE;
            break;
        }
    }
#endif
    return bValid;
}

ERRORTYPE videoInputHw_SetRegions(VI_DEV vipp_id, RgnChnAttachDetailPack *pPack)
{
    ERRORTYPE ret = SUCCESS;
    if (vipp_id >= HW_VIDEO_DEVICE_NUM || vipp_id < 0)
    {
        aloge("vipp[%d] is invalid!", vipp_id);
        return ERR_VI_INVALID_CHNID;
    }

    if (gpVIDevManager->gpVippManager[vipp_id] == NULL)
    {
        return ERR_VI_INVALID_NULL_PTR;
    }
    viChnManager *pVipp = gpVIDevManager->gpVippManager[vipp_id];

    //check if region position and size fulfill the align request.
    int i;
    for(i=0; i<pPack->nNum; i++)
    {
        if(FALSE == checkRegionPositionValid(pPack->RgnChnAttachDetailArray[i].pRgnAttr, pPack->RgnChnAttachDetailArray[i].pRgnChnAttr))
        {
            aloge("fatal error! region position[%d] is invalid, ignore this region!", i);
            ret = ERR_VI_INVALID_PARA;
            break;
        }
    }
    if(i != pPack->nNum)
    {
        return ret;
    }

    pthread_mutex_lock(&pVipp->mRegionLock);
    ret = SUCCESS;
    //if handle is exist, return.
    for(i=0; i<pPack->nNum; i++)
    {
        ChannelRegionInfo *pEntry;
        list_for_each_entry(pEntry, &pVipp->mOverlayList, mList)
        {
            if(pPack->RgnChnAttachDetailArray[i].nHandle == pEntry->mRgnHandle)
            {
                aloge("fatal error! RgnHandle[%d-%d] is already exist!", i, pPack->RgnChnAttachDetailArray[i].nHandle);
                ret = ERR_VI_EXIST;
                break;
            }
        }
        if(ret != SUCCESS)
        {
            break;
        }
        list_for_each_entry(pEntry, &pVipp->mCoverList, mList)
        {
            if(pPack->RgnChnAttachDetailArray[i].nHandle == pEntry->mRgnHandle)
            {
                aloge("fatal error! RgnHandle[%d] is already exist!", i, pPack->RgnChnAttachDetailArray[i].nHandle);
                ret = ERR_VI_EXIST;
                break;
            }
        }
        if(ret != SUCCESS)
        {
            break;
        }
        list_for_each_entry(pEntry, &pVipp->mOrlList, mList)
        {
            if(pPack->RgnChnAttachDetailArray[i].nHandle == pEntry->mRgnHandle)
            {
                aloge("fatal error! RgnHandle[%d] is already exist!", i, pPack->RgnChnAttachDetailArray[i].nHandle);
                ret = ERR_VI_EXIST;
                break;
            }
        }
        if(ret != SUCCESS)
        {
            break;
        }
    }
    if(ret != SUCCESS)
    {
        goto _err0;
    }

    //add all regions to vipp overlay/cover/orl list.
    BOOL bDraw = FALSE;
    for(i=0; i<pPack->nNum; i++)
    {
        ChannelRegionInfo *pRegion = ChannelRegionInfo_Construct();
        if(NULL == pRegion)
        {
            aloge("fatal error! malloc fail!");
            break;
        }
        pRegion->mRgnHandle = pPack->RgnChnAttachDetailArray[i].nHandle;
        pRegion->mRgnAttr = *pPack->RgnChnAttachDetailArray[i].pRgnAttr;
        pRegion->mRgnChnAttr = *pPack->RgnChnAttachDetailArray[i].pRgnChnAttr;
        if(pPack->RgnChnAttachDetailArray[i].pBmp)
        {
            pRegion->mbSetBmp = TRUE;
            pRegion->mBmp = *pPack->RgnChnAttachDetailArray[i].pBmp;
        }
        else
        {
            pRegion->mbSetBmp = FALSE;
        }
        if(OVERLAY_RGN == pRegion->mRgnAttr.enType)
        {
            //sort from small priority to large priority.
            if(!list_empty(&pVipp->mOverlayList))
            {
                BOOL bInsert = FALSE;
                ChannelRegionInfo *pEntry;
                list_for_each_entry(pEntry, &pVipp->mOverlayList, mList)
                {
                    if(TRUE == compareRegionPriority(&pRegion->mRgnChnAttr, &pEntry->mRgnChnAttr))
                    {
                        list_add_tail(&pRegion->mList, &pEntry->mList);
                        bInsert = TRUE;
                        break;
                    }
                }
                if(!bInsert)
                {
                    list_add_tail(&pRegion->mList, &pVipp->mOverlayList);
                }
            }
            else
            {
                list_add_tail(&pRegion->mList, &pVipp->mOverlayList);
            }
        }
        else if(COVER_RGN == pRegion->mRgnAttr.enType)
        {
            //sort from small priority to large priority.
            if(!list_empty(&pVipp->mCoverList))
            {
                BOOL bInsert = FALSE;
                ChannelRegionInfo *pEntry;
                list_for_each_entry(pEntry, &pVipp->mCoverList, mList)
                {
                    if(TRUE == compareRegionPriority(&pRegion->mRgnChnAttr, &pEntry->mRgnChnAttr))
                    {
                        list_add_tail(&pRegion->mList, &pEntry->mList);
                        bInsert = TRUE;
                        break;
                    }
                }
                if(!bInsert)
                {
                    list_add_tail(&pRegion->mList, &pVipp->mCoverList);
                }
            }
            else
            {
                list_add_tail(&pRegion->mList, &pVipp->mCoverList);
            }
        }
        else if(ORL_RGN == pRegion->mRgnAttr.enType)
        {
            //sort from small priority to large priority.
            if(!list_empty(&pVipp->mOrlList))
            {
                BOOL bInsert = FALSE;
                ChannelRegionInfo *pEntry;
                list_for_each_entry(pEntry, &pVipp->mOrlList, mList)
                {
                    if(TRUE == compareRegionPriority(&pRegion->mRgnChnAttr, &pEntry->mRgnChnAttr))
                    {
                        list_add_tail(&pRegion->mList, &pEntry->mList);
                        bInsert = TRUE;
                        break;
                    }
                }
                if(!bInsert)
                {
                    list_add_tail(&pRegion->mList, &pVipp->mOrlList);
                }
            }
            else
            {
                list_add_tail(&pRegion->mList, &pVipp->mOrlList);
            }
        }
        else
        {
            aloge("fatal error! unsupport rgnType[0x%x]", pRegion->mRgnAttr.enType);
            if(pRegion->mBmp.mpData)
            {
                pRegion->mBmp.mpData = NULL;
            }
            free(pRegion);
            break;
        }
        //decide if draw this region
        if(pRegion->mRgnChnAttr.bShow)
        {
            if(OVERLAY_RGN == pRegion->mRgnAttr.enType)
            {
                if(pRegion->mbSetBmp)
                {
                    pRegion->mbDraw = TRUE;
                }
                else
                {
                    pRegion->mbDraw = FALSE;
                }
            }
            else
            {
                pRegion->mbDraw = TRUE;
            }
        }
        else
        {
            pRegion->mbDraw = FALSE;
        }
        if(pRegion->mbDraw)
        {
            bDraw = TRUE;
        }
    }

    if(bDraw)
    {
        pthread_mutex_lock(&pVipp->mLock);
        if(!pVipp->vipp_enable)
        {
            //alogw("Be careful! can't draw osd during vipp disable!");
        }
      #if(AWCHIP == AW_V5)
        videoInputHw_DrawOSD_V5(vipp_id);
      #elif(AWCHIP == AW_V316 || AWCHIP == AW_V459 || AWCHIP == AW_V853)
        videoInputHw_DrawOSD(vipp_id);
      #else
        videoInputHw_DrawOSD(vipp_id);
      #endif
        pthread_mutex_unlock(&pVipp->mLock);
    }
    pthread_mutex_unlock(&pVipp->mRegionLock);
    return ret;

_err1:
_err0:
    pthread_mutex_unlock(&pVipp->mRegionLock);
    return ret;
}

ERRORTYPE videoInputHw_DeleteRegions(VI_DEV vipp_id, RgnChnAttachDetailPack *pPack)
{
    ERRORTYPE ret = SUCCESS;
    if (vipp_id >= HW_VIDEO_DEVICE_NUM || vipp_id < 0)
    {
        aloge("vipp[%d] is invalid!", vipp_id);
        return ERR_VI_INVALID_CHNID;
    }

    if (gpVIDevManager->gpVippManager[vipp_id] == NULL)
    {
        return ERR_VI_INVALID_NULL_PTR;
    }
    viChnManager *pVipp = gpVIDevManager->gpVippManager[vipp_id];
    pthread_mutex_lock(&pVipp->mRegionLock);
    int i;
    BOOL bDraw = FALSE;
    //find all handles and delete them from vipp overlay/cover/orl list.
    for(i=0; i<pPack->nNum; i++)
    {
        BOOL bFind = FALSE;
        ChannelRegionInfo *pRegion;
        if(!list_empty(&pVipp->mOverlayList))
        {
            list_for_each_entry(pRegion, &pVipp->mOverlayList, mList)
            {
                if(pPack->RgnChnAttachDetailArray[i].nHandle == pRegion->mRgnHandle)
                {
                    bFind = TRUE;
                    break;
                }
            }
        }
        if(FALSE == bFind && !list_empty(&pVipp->mCoverList))
        {
            list_for_each_entry(pRegion, &pVipp->mCoverList, mList)
            {
                if(pPack->RgnChnAttachDetailArray[i].nHandle == pRegion->mRgnHandle)
                {
                    bFind = TRUE;
                    break;
                }
            }
        }
        if(FALSE == bFind && !list_empty(&pVipp->mOrlList))
        {
            list_for_each_entry(pRegion, &pVipp->mOrlList, mList)
            {
                if(pPack->RgnChnAttachDetailArray[i].nHandle == pRegion->mRgnHandle)
                {
                    bFind = TRUE;
                    break;
                }
            }
        }
        if(FALSE == bFind)
        {
            aloge("fatal error! can't find rgnHandle[%d-%d]", i, pPack->RgnChnAttachDetailArray[i].nHandle);
            ret = ERR_VI_UNEXIST;
            continue;
        }
        if(pRegion->mbDraw)
        {
            bDraw = TRUE;
        }
        list_del(&pRegion->mList);
        ChannelRegionInfo_Destruct(pRegion);
    }

    if(bDraw)
    {
        //need redraw osd
        pthread_mutex_lock(&pVipp->mLock);
        if(!pVipp->vipp_enable)
        {
            //alogw("Be careful! can't draw osd during vipp disable!");
        }
      #if(AWCHIP == AW_V5)
        videoInputHw_DrawOSD_V5(vipp_id);
      #elif(AWCHIP == AW_V316 || AWCHIP == AW_V459 || AWCHIP == AW_V853)
        videoInputHw_DrawOSD(vipp_id);
      #else
        videoInputHw_DrawOSD(vipp_id);
      #endif
        pthread_mutex_unlock(&pVipp->mLock);
    }

    pthread_mutex_unlock(&pVipp->mRegionLock);
    return ret;

_err0:
    pthread_mutex_unlock(&pVipp->mRegionLock);
    return ret;
}

ERRORTYPE videoInputHw_UpdateOverlayBitmap(VI_DEV vipp_id, RGN_HANDLE RgnHandle, BITMAP_S *pBitmap)
{
    ERRORTYPE ret = SUCCESS;
    if (vipp_id >= HW_VIDEO_DEVICE_NUM || vipp_id < 0)
    {
        aloge("vipp[%d] is invalid!", vipp_id);
        return ERR_VI_INVALID_CHNID;
    }

    if (gpVIDevManager->gpVippManager[vipp_id] == NULL)
    {
        return ERR_VI_INVALID_NULL_PTR;
    }
    viChnManager *pVipp = gpVIDevManager->gpVippManager[vipp_id];
    pthread_mutex_lock(&pVipp->mRegionLock);
    //find handle
    BOOL bFind = FALSE;
    ChannelRegionInfo *pRegion;
    list_for_each_entry(pRegion, &pVipp->mOverlayList, mList)
    {
        if(RgnHandle == pRegion->mRgnHandle)
        {
            bFind = TRUE;
            break;
        }
    }
    if(FALSE == bFind)
    {
        ret = ERR_VI_UNEXIST;
        goto _err0;
    }
    if(pRegion->mRgnAttr.enType != OVERLAY_RGN)
    {
        aloge("fatal error! rgn type[0x%x] is not overlay!", pRegion->mRgnAttr.enType);
        ret = ERR_VI_INVALID_PARA;
        goto _err0;
    }
    int size0 = 0;
    int size1 = BITMAP_S_GetdataSize(pBitmap);
    if(pRegion->mbSetBmp)
    {
        size0 = BITMAP_S_GetdataSize(&pRegion->mBmp);
        if(size0 != size1)
        {
            aloge("fatal error! bmp size[%d]!=[%d]", size0, size1);
//            pRegion->mBmp.mpData = NULL;
//            pRegion->mbSetBmp = FALSE;
        }
        pRegion->mBmp = *pBitmap;
    }
    if(FALSE == pRegion->mbSetBmp)
    {
        pRegion->mBmp = *pBitmap;
        pRegion->mbSetBmp = TRUE;
    }
    if(pBitmap->mWidth != pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width || pBitmap->mHeight != pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height)
    {
        alogw("Be careful! bitmap size[%dx%d] != region size[%dx%d], need update region size!", pBitmap->mWidth, pBitmap->mHeight, pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width, pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height);
        pRegion->mRgnAttr.unAttr.stOverlay.mSize.Width = pBitmap->mWidth;
        pRegion->mRgnAttr.unAttr.stOverlay.mSize.Height = pBitmap->mHeight;
    }
    //decide if draw this region
    if(pRegion->mRgnChnAttr.bShow)
    {
        pRegion->mbDraw = TRUE;
    }
    else
    {
        pRegion->mbDraw = FALSE;
    }

    if(pRegion->mbDraw)
    {
        pthread_mutex_lock(&pVipp->mLock);
        if(!pVipp->vipp_enable)
        {
            //alogw("Be careful! can't draw osd during vipp disable!");
        }
      #if(AWCHIP == AW_V5)
        videoInputHw_DrawOSD_V5(vipp_id);
      #elif(AWCHIP == AW_V316 || AWCHIP == AW_V459 || AWCHIP == AW_V853)
        videoInputHw_DrawOSD(vipp_id);
      #else
        videoInputHw_DrawOSD(vipp_id);
      #endif
        pthread_mutex_unlock(&pVipp->mLock);
    }
    pthread_mutex_unlock(&pVipp->mRegionLock);
    return ret;

_err0:
    pthread_mutex_unlock(&pVipp->mRegionLock);
    return ret;
}

ERRORTYPE videoInputHw_UpdateRegionChnAttr(VI_DEV vipp_id, RGN_HANDLE RgnHandle, const RGN_CHN_ATTR_S *pRgnChnAttr)
{
    ERRORTYPE ret = SUCCESS;
    if (vipp_id >= HW_VIDEO_DEVICE_NUM || vipp_id < 0)
    {
        aloge("vipp[%d] is invalid!", vipp_id);
        return ERR_VI_INVALID_CHNID;
    }

    if (gpVIDevManager->gpVippManager[vipp_id] == NULL)
    {
        return ERR_VI_INVALID_NULL_PTR;
    }
    viChnManager *pVipp = gpVIDevManager->gpVippManager[vipp_id];
    pthread_mutex_lock(&pVipp->mRegionLock);
    //find handle
    BOOL bFind = FALSE;
    ChannelRegionInfo *pRegion;
    if(OVERLAY_RGN == pRgnChnAttr->enType)
    {
        list_for_each_entry(pRegion, &pVipp->mOverlayList, mList)
        {
            if(RgnHandle == pRegion->mRgnHandle)
            {
                bFind = TRUE;
                break;
            }
        }
    }
    else if(COVER_RGN == pRgnChnAttr->enType)
    {
        list_for_each_entry(pRegion, &pVipp->mCoverList, mList)
        {
            if(RgnHandle == pRegion->mRgnHandle)
            {
                bFind = TRUE;
                break;
            }
        }
    }
    else if(ORL_RGN == pRgnChnAttr->enType)
    {
        list_for_each_entry(pRegion, &pVipp->mOrlList, mList)
        {
            if(RgnHandle == pRegion->mRgnHandle)
            {
                bFind = TRUE;
                break;
            }
        }
    }
    if(FALSE == bFind)
    {
        ret = ERR_VI_UNEXIST;
        goto _err0;
    }

    if(OVERLAY_RGN == pRgnChnAttr->enType)
    {
        BOOL bUpdate = FALSE;
        if(pRegion->mRgnChnAttr.bShow != pRgnChnAttr->bShow)
        {
            alogd("bShow change [%d]->[%d]", pRegion->mRgnChnAttr.bShow, pRgnChnAttr->bShow);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X != pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.X
            || pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y != pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.Y)
        {
            alogd("stPoint change [%d,%d]->[%d,%d]",
                pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X,
                pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y,
                pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.X,
                pRgnChnAttr->unChnAttr.stOverlayChn.stPoint.Y
                );
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha != pRgnChnAttr->unChnAttr.stOverlayChn.mFgAlpha)
        {
            alogd("FgAlpha change [%d]->[%d]", pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha, pRgnChnAttr->unChnAttr.stOverlayChn.mFgAlpha);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mLayer != pRgnChnAttr->unChnAttr.stOverlayChn.mLayer)
        {
            alogd("overlay priority(mLayer) change [%d]->[%d]", pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.mLayer, pRgnChnAttr->unChnAttr.stOverlayChn.mLayer);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn != pRgnChnAttr->unChnAttr.stOverlayChn.stInvertColor.bInvColEn)
        {
            alogd("overlay InvColEn change [%d]->[%d]", pRegion->mRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn, pRgnChnAttr->unChnAttr.stOverlayChn.stInvertColor.bInvColEn);
            bUpdate = TRUE;
        }
        pRegion->mRgnChnAttr = *pRgnChnAttr;
        //decide if draw this region
        if(pRegion->mRgnChnAttr.bShow && pRegion->mbSetBmp)
        {
            pRegion->mbDraw = TRUE;
        }
        else
        {
            pRegion->mbDraw = FALSE;
        }
        if(bUpdate)
        {
            if(pRegion->mbSetBmp)
            {
                pthread_mutex_lock(&pVipp->mLock);
                if(!pVipp->vipp_enable)
                {
                    //alogw("Be careful! can't draw osd during vipp disable!");
                }
              #if(AWCHIP == AW_V5)
                videoInputHw_DrawOSD_V5(vipp_id);
              #elif(AWCHIP == AW_V316 || AWCHIP == AW_V459 || AWCHIP == AW_V853)
                videoInputHw_DrawOSD(vipp_id);
              #else
                videoInputHw_DrawOSD(vipp_id);
              #endif
                pthread_mutex_unlock(&pVipp->mLock);
            }
        }
    }
    else if(COVER_RGN == pRgnChnAttr->enType)
    {
        BOOL bUpdate = FALSE;
        if(pRegion->mRgnChnAttr.bShow != pRgnChnAttr->bShow)
        {
            alogd("bShow change [%d]->[%d]", pRegion->mRgnChnAttr.bShow, pRgnChnAttr->bShow);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType != pRgnChnAttr->unChnAttr.stCoverChn.enCoverType)
        {
            aloge("fatal error! cover type change [0x%x]->[0x%x]", pRegion->mRgnChnAttr.unChnAttr.stCoverChn.enCoverType, pRgnChnAttr->unChnAttr.stCoverChn.enCoverType);
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.X != pRgnChnAttr->unChnAttr.stCoverChn.stRect.X
            || pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Y != pRgnChnAttr->unChnAttr.stCoverChn.stRect.Y
            || pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Width != pRgnChnAttr->unChnAttr.stCoverChn.stRect.Width
            || pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Height != pRgnChnAttr->unChnAttr.stCoverChn.stRect.Height)
        {
            alogd("cover rect change [%d,%d,%d,%d]->[%d,%d,%d,%d]",
                pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.X, pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Y,
                pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Width, pRegion->mRgnChnAttr.unChnAttr.stCoverChn.stRect.Height,
                pRgnChnAttr->unChnAttr.stCoverChn.stRect.X, pRgnChnAttr->unChnAttr.stCoverChn.stRect.Y,
                pRgnChnAttr->unChnAttr.stCoverChn.stRect.Width, pRgnChnAttr->unChnAttr.stCoverChn.stRect.Height);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mColor != pRgnChnAttr->unChnAttr.stCoverChn.mColor)
        {
            alogd("cover color change [0x%x]->[0x%x]", pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mColor, pRgnChnAttr->unChnAttr.stCoverChn.mColor);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mLayer != pRgnChnAttr->unChnAttr.stCoverChn.mLayer)
        {
            alogd("cover priority(mLayer) change [%d]->[%d]", pRegion->mRgnChnAttr.unChnAttr.stCoverChn.mLayer, pRgnChnAttr->unChnAttr.stCoverChn.mLayer);
            bUpdate = TRUE;
        }
        pRegion->mRgnChnAttr = *pRgnChnAttr;
        //decide if draw this region
        if(pRegion->mRgnChnAttr.bShow)
        {
            pRegion->mbDraw = TRUE;
        }
        else
        {
            pRegion->mbDraw = FALSE;
        }
        if(bUpdate)
        {
            pthread_mutex_lock(&pVipp->mLock);
            if(!pVipp->vipp_enable)
            {
                //alogw("Be careful! can't draw osd during vipp disable!");
            }
          #if(AWCHIP == AW_V5)
            videoInputHw_DrawOSD_V5(vipp_id);
          #elif(AWCHIP == AW_V316 || AWCHIP == AW_V459 || AWCHIP == AW_V853)
            videoInputHw_DrawOSD(vipp_id);
          #else
            videoInputHw_DrawOSD(vipp_id);
          #endif
            pthread_mutex_unlock(&pVipp->mLock);
        }
    }
    else if(ORL_RGN == pRgnChnAttr->enType)
    {
        BOOL bUpdate = FALSE;
        if(pRegion->mRgnChnAttr.unChnAttr.stOrlChn.enAreaType != pRgnChnAttr->unChnAttr.stOrlChn.enAreaType)
        {
            aloge("fatal error! orl type change [0x%x]->[0x%x], ignore this region!", pRegion->mRgnChnAttr.unChnAttr.stOrlChn.enAreaType, pRgnChnAttr->unChnAttr.stOrlChn.enAreaType);
            bUpdate = FALSE;
            goto _update;
        }
        //check if region position and size fulfill the align request.
        if(FALSE == checkRegionPositionValid(NULL, pRgnChnAttr))
        {
            aloge("fatal error! region position is invalid, ignore this region!");
            bUpdate = FALSE;
            goto _update;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOrlChn.stRect.X != pRgnChnAttr->unChnAttr.stOrlChn.stRect.X
            || pRegion->mRgnChnAttr.unChnAttr.stOrlChn.stRect.Y != pRgnChnAttr->unChnAttr.stOrlChn.stRect.Y
            || pRegion->mRgnChnAttr.unChnAttr.stOrlChn.stRect.Width != pRgnChnAttr->unChnAttr.stOrlChn.stRect.Width
            || pRegion->mRgnChnAttr.unChnAttr.stOrlChn.stRect.Height != pRgnChnAttr->unChnAttr.stOrlChn.stRect.Height)
        {
            alogd("orl rect change [%d,%d,%d,%d]->[%d,%d,%d,%d]",
                pRegion->mRgnChnAttr.unChnAttr.stOrlChn.stRect.X, pRegion->mRgnChnAttr.unChnAttr.stOrlChn.stRect.Y,
                pRegion->mRgnChnAttr.unChnAttr.stOrlChn.stRect.Width, pRegion->mRgnChnAttr.unChnAttr.stOrlChn.stRect.Height,
                pRgnChnAttr->unChnAttr.stOrlChn.stRect.X, pRgnChnAttr->unChnAttr.stOrlChn.stRect.Y,
                pRgnChnAttr->unChnAttr.stOrlChn.stRect.Width, pRgnChnAttr->unChnAttr.stOrlChn.stRect.Height);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOrlChn.mColor != pRgnChnAttr->unChnAttr.stOrlChn.mColor)
        {
            alogd("orl color change [0x%x]->[0x%x]", pRegion->mRgnChnAttr.unChnAttr.stOrlChn.mColor, pRgnChnAttr->unChnAttr.stOrlChn.mColor);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.unChnAttr.stOrlChn.mThick != pRgnChnAttr->unChnAttr.stOrlChn.mThick)
        {
            alogd("orl thick change [0x%x]->[0x%x]", pRegion->mRgnChnAttr.unChnAttr.stOrlChn.mThick, pRgnChnAttr->unChnAttr.stOrlChn.mThick);
            bUpdate = TRUE;
        }

        if(pRegion->mRgnChnAttr.unChnAttr.stOrlChn.mLayer != pRgnChnAttr->unChnAttr.stOrlChn.mLayer)
        {
            alogd("orl priority(mLayer) change [%d]->[%d]", pRegion->mRgnChnAttr.unChnAttr.stOrlChn.mLayer, pRgnChnAttr->unChnAttr.stOrlChn.mLayer);
            bUpdate = TRUE;
        }
        if(pRegion->mRgnChnAttr.bShow != pRgnChnAttr->bShow)
        {
            alogd("bShow change [%d]->[%d]", pRegion->mRgnChnAttr.bShow, pRgnChnAttr->bShow);
            bUpdate = TRUE;
        }
        else
        {
            if(FALSE == pRgnChnAttr->bShow)
            {
                alogd("this region remain unshow, so need not update!");
                bUpdate = FALSE;
            }
        }
        pRegion->mRgnChnAttr = *pRgnChnAttr;
        //decide if draw this region
        if(pRegion->mRgnChnAttr.bShow)
        {
            pRegion->mbDraw = TRUE;
        }
        else
        {
            pRegion->mbDraw = FALSE;
        }
      _update:
        if(bUpdate)
        {
            pthread_mutex_lock(&pVipp->mLock);
            if(!pVipp->vipp_enable)
            {
                //alogw("Be careful! can't draw osd during vipp disable!");
            }
          #if(AWCHIP == AW_V5)
            videoInputHw_DrawOSD_V5(vipp_id);
          #elif(AWCHIP == AW_V316 || AWCHIP == AW_V459 || AWCHIP == AW_V853)
            videoInputHw_DrawOSD(vipp_id);
          #else
            videoInputHw_DrawOSD(vipp_id);
          #endif
            pthread_mutex_unlock(&pVipp->mLock);
        }
    }
    else
    {
        aloge("fatal error! rgn type[0x%x]", pRgnChnAttr->enType);
    }
    pthread_mutex_unlock(&pVipp->mRegionLock);
    return ret;

_err0:
    pthread_mutex_unlock(&pVipp->mRegionLock);
    return ret;
}

ERRORTYPE videoInputHw_SetInputBitWidthStart(VI_DEV Vipp_id, const enum set_bit_width bitwidth)
{
    struct isp_video_device *video = NULL;

    if (Vipp_id >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[Vipp_id]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", Vipp_id);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = gpVIDevManager->media->video_dev[Vipp_id];
    }

    int ret = video_set_input_bit_width_start(video, bitwidth);
    if (ret) {
        aloge("vipp[%d] set input bit width start failed! ret=%d", Vipp_id, ret);
        return FAILURE;
    }

    return 0;
}

ERRORTYPE videoInputHw_SetInputBitWidthStop(VI_DEV Vipp_id, const enum set_bit_width bitwidth)
{
    struct isp_video_device *video = NULL;

    if (Vipp_id >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[Vipp_id]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", Vipp_id);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = gpVIDevManager->media->video_dev[Vipp_id];
    }

    int ret = video_set_input_bit_width_stop(video, bitwidth);
    if (ret) {
        aloge("vipp[%d] set input bit width stop failed! ret=%d", Vipp_id, ret);
        return FAILURE;
    }

    return 0;
}

ERRORTYPE videoInputHw_SetDropFrame(VI_DEV Vipp_id, unsigned int drop_num)
{
    struct isp_video_device *video = NULL;

    if (Vipp_id >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[Vipp_id]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", Vipp_id);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = gpVIDevManager->media->video_dev[Vipp_id];
    }

    int ret = video_set_tdm_drop_frame_num(video, drop_num);
    if (ret) {
        aloge("vipp[%d] set drop frame num failed! ret=%d", Vipp_id, ret);
        return FAILURE;
    }

    return 0;
}

ERRORTYPE videoInputHw_Get_VippBuffer(VI_DEV Vipp_id, VIDEO_FRAME_INFO_S *pstFrameInfo, int BufNum)
{
    struct isp_video_device *video = NULL;

    if (Vipp_id >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[Vipp_id]) {
        ISP_ERR("VIN CH[%d] number is invalid!\n", Vipp_id);
        return ERR_VI_INVALID_CHNID;
    } else {
        video = gpVIDevManager->media->video_dev[Vipp_id];
    }

    if (video->nbufs != BufNum) {
        ISP_ERR("VIN CH[%d] video buffer number does not match the configured number\n", Vipp_id);
        return ERR_VI_INVALID_PARA;
    }

    for (int i = 0; i < video->nbufs; i++)
    {
        for (int j = 0; j < video->nplanes; j++) {
            pstFrameInfo[i].VFrame.mpVirAddr[j] = video->pool->buffers[i].planes[j].mem;
            pstFrameInfo[i].VFrame.mPhyAddr[j] = video->pool->buffers[i].planes[j].mem_phy;
        }
    }

    return 0;
}

#if 1
/* ==================================================== */
/* Isp set api. start */
/* ==================================================== */
ERRORTYPE videoInputHw_IspAe_SetMode(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (0 == value) {
		printf("auto ae.\r\n");
		video_set_control(video, V4L2_CID_EXPOSURE_AUTO, 0); // auto ae
		video_set_control(video, V4L2_CID_AUTOGAIN, 1);
	} else if (1 == value) {
		printf("manual ae.\r\n");
		video_set_control(video, V4L2_CID_EXPOSURE_AUTO, 1); // manual ae
		video_set_control(video, V4L2_CID_AUTOGAIN, 0);
	} else {
		return ERR_VI_INVALID_CHNID;
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_SetExposureBias(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

        // auto ae
    if (video_set_control(video, V4L2_CID_AUTO_EXPOSURE_BIAS, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}
ERRORTYPE videoInputHw_IspAe_SetExposure(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_set_control(video, V4L2_CID_EXPOSURE_ABSOLUTE, value) < 0) {// manual ae,300000
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_SetISOSensitiveMode(int *pIspId, int mode)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    mode = !!mode;
    if (video_set_control(video, V4L2_CID_ISO_SENSITIVITY_AUTO, mode) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_SetISOSensitive(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (value == 0) {
        return video_set_control(video, V4L2_CID_ISO_SENSITIVITY_AUTO, 1); // auto mode
    }

    if (value < 0 || value > 7) {
        aloge("value range should be [1~7], value(%d)", value);
        return ERR_VI_INVALID_PARA;
    }

    if ((video_set_control(video, V4L2_CID_ISO_SENSITIVITY_AUTO, 0/*manual*/) < 0) ||
        (video_set_control(video, V4L2_CID_ISO_SENSITIVITY, value-1) < 0)) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_SetMetering(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((value < 0) || (value > 3)) {
        aloge("out of range, you shoule use [0~3], value(%d)", value);
        return FAILURE;
    }

    /* 0:average, 1:center, 2:spot, 3:matrix */
    if (video_set_control(video, V4L2_CID_EXPOSURE_METERING, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}
ERRORTYPE videoInputHw_IspAe_SetGain(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_set_control(video, V4L2_CID_GAIN, value) < 0) {// manual ae,8000
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_SetEvIdx(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_EV_IDX, &value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_SetLock(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((value < 0) || (value > 1)) {
        ISP_ERR("Invalid option for set ae lock!\n");
        return FAILURE;
    }

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_LOCK, &value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_SetTable(int *pIspId, struct ae_table_info *ae_table)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (ae_table == NULL) {
        ISP_ERR("Invalid ae_table for set ae table!\n");
        return FAILURE;
    }

    if (ae_table->length <= 0) {
        ISP_ERR("Invalid length for set ae table!\n");
        return FAILURE;
    }

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_TABLE, ae_table) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_RoiArea(int *pIspId, SIZE_S Res, RECT_S RoiRgn, AW_U16 ForceAeTarget, AW_U16 Enable)
{
    struct isp_ae_roi_attr ae_roi_attr;
    POINT_S PtRightBottom;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }
    if (Enable < 0 || Enable > 1) {
        aloge("Invaild option for set Ae_RoiArea!!!\n");
        return -1;
    }

    PtRightBottom.X = RoiRgn.X + RoiRgn.Width;
    PtRightBottom.Y = RoiRgn.Y + RoiRgn.Height;

    if ((RoiRgn.X > Res.Width) || (RoiRgn.Y > Res.Height) ||
            (PtRightBottom.X > Res.Width) || (PtRightBottom.Y > Res.Height)) {
        aloge("(%d, %d) (%d, %d), it is invalid coordinate for src:%d*%d !!!\n",
            RoiRgn.X, RoiRgn.Y, PtRightBottom.X, PtRightBottom.Y, Res.Width, Res.Height);
        return FAILURE;
    }

    ae_roi_attr.enable = Enable;
    ae_roi_attr.force_ae_target = ForceAeTarget;
    ae_roi_attr.coor.x1 = (int)( (float)RoiRgn.X*2000 / Res.Width  - 1000 );
    ae_roi_attr.coor.y1 = (int)( (float)RoiRgn.Y*2000 / Res.Height - 1000 );
    ae_roi_attr.coor.x2 = (int)( (float)PtRightBottom.X*2000 / Res.Width  - 1000 );
    ae_roi_attr.coor.y2 = (int)( (float)PtRightBottom.Y*2000 / Res.Height - 1000 );

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_ROI_TARGET, &ae_roi_attr) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_RoiMeteringArea(int *pIspId, SIZE_S Res, RECT_S RoiRgn)
{
    struct isp_h3a_coor_win ae_roi_attr;
    POINT_S PtRightBottom;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    PtRightBottom.X = RoiRgn.X + RoiRgn.Width;
    PtRightBottom.Y = RoiRgn.Y + RoiRgn.Height;
    if ((RoiRgn.X > Res.Width) || (RoiRgn.Y > Res.Height) ||
            (PtRightBottom.X > Res.Width) || (PtRightBottom.Y > Res.Height)) {
        aloge("(%d, %d) (%d, %d), it is invalid coordinate for src:%d*%d !!!\n",
            RoiRgn.X, RoiRgn.Y, PtRightBottom.X, PtRightBottom.Y, Res.Width, Res.Height);
        return FAILURE;
    }

    ae_roi_attr.x1 = (int)( (float)RoiRgn.X*2000 / Res.Width  - 1000 );
    ae_roi_attr.y1 = (int)( (float)RoiRgn.Y*2000 / Res.Height - 1000 );
    ae_roi_attr.x2 = (int)( (float)PtRightBottom.X*2000 / Res.Width  - 1000 );
    ae_roi_attr.y2 = (int)( (float)PtRightBottom.Y*2000 / Res.Height - 1000 );

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_ROI, &ae_roi_attr) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_SetFaceAeCfg(int *pIspId, struct isp_face_ae_attr_info FaceAeInfo, SIZE_S Res)
{
    int real_vaild_cnt, i;
    POINT_S PtRightBottom[AE_FACE_MAX_NUM];
    struct ae_face_cfg face_ae_cfg_load;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (FaceAeInfo.enable == true) {
        if (FaceAeInfo.vaild_face_num < 0) {
            aloge("face_num = %d is < 0, please check it!\n", FaceAeInfo.vaild_face_num);
            return FAILURE;
        } else if (FaceAeInfo.vaild_face_num > AE_FACE_MAX_NUM) {
            alogw("vaild_face_num:%d > AE_FACE_MAX_NUM:%d, it will only use %d face_roi!",
            FaceAeInfo.vaild_face_num, AE_FACE_MAX_NUM, AE_FACE_MAX_NUM);
            FaceAeInfo.vaild_face_num = AE_FACE_MAX_NUM;
        }

        /* reset face_ae_cfg_load for detect */
        memset(&face_ae_cfg_load, 0, sizeof(struct ae_face_cfg));
        /* parser face_roi */
        for (i = 0, real_vaild_cnt = 0; i < FaceAeInfo.vaild_face_num; i++) {
            /* calc PtRightBottom */
            PtRightBottom[i].X = FaceAeInfo.face_roi_rgn[i].X + FaceAeInfo.face_roi_rgn[i].Width;
            PtRightBottom[i].Y = FaceAeInfo.face_roi_rgn[i].Y + FaceAeInfo.face_roi_rgn[i].Height;
            /* check area */
            if ((FaceAeInfo.face_roi_rgn[i].X > Res.Width) || (FaceAeInfo.face_roi_rgn[i].Y > Res.Height) ||
                (PtRightBottom[i].X > Res.Width) || (PtRightBottom[i].Y > Res.Height) ||
                (FaceAeInfo.face_roi_rgn[i].X == PtRightBottom[i].X && FaceAeInfo.face_roi_rgn[i].Y == PtRightBottom[i].Y)) {
                aloge("face_ae_cfg_load.face_ae_coor[%d]: (%d, %d) (%d, %d), it is invalid coordinate from src:%d*%d, will lost it\n",
                FaceAeInfo.face_roi_rgn[i].X, FaceAeInfo.face_roi_rgn[i].Y,
                PtRightBottom[i].X, PtRightBottom[i].Y, Res.Width, Res.Height);
                /* reset this face_ae_coor */
                memset(&face_ae_cfg_load.face_ae_coor[i], 0, sizeof(struct isp_h3a_coor_win));
            } else {
            /* calc vaild face_roi */
                face_ae_cfg_load.face_ae_coor[real_vaild_cnt].x1 = (int)(FaceAeInfo.face_roi_rgn[i].X * 2000 / Res.Width  - 1000 );
                face_ae_cfg_load.face_ae_coor[real_vaild_cnt].y1 = (int)(FaceAeInfo.face_roi_rgn[i].Y * 2000 / Res.Height - 1000 );
                face_ae_cfg_load.face_ae_coor[real_vaild_cnt].x2 = (int)(PtRightBottom[i].X * 2000 / Res.Width  - 1000 );
                face_ae_cfg_load.face_ae_coor[real_vaild_cnt].y2 = (int)(PtRightBottom[i].Y * 2000 / Res.Height - 1000 );
//#define PRINT_INFO
#ifdef PRINT_INFO
                alogd("face_ae_cfg_load.face_ae_coor[%d]: (%d, %d) & (%d, %d) -> (%d, %d) (%d, %d)\n", i,
                FaceAeInfo.face_roi_rgn[i].X, FaceAeInfo.face_roi_rgn[i].Y,
                PtRightBottom[i].X, PtRightBottom[i].Y,
                face_ae_cfg_load.face_ae_coor[real_vaild_cnt].x1, face_ae_cfg_load.face_ae_coor[real_vaild_cnt].y1,
                face_ae_cfg_load.face_ae_coor[real_vaild_cnt].x2, face_ae_cfg_load.face_ae_coor[real_vaild_cnt].y2);
#endif
                real_vaild_cnt++;
            }
        }

        if (real_vaild_cnt > AE_FACE_MAX_NUM || real_vaild_cnt < 0) {
            aloge("real_vaild_num = %d, it will not do faceAE, please check face_roi!\n", real_vaild_cnt);
            return FAILURE;
        } else if (real_vaild_cnt == 0) {
            alogd("real_vaild_num = %d, it will recover AE\n", real_vaild_cnt);
        }

        face_ae_cfg_load.vaild_face_cnt = real_vaild_cnt;
        face_ae_cfg_load.face_ae_tolerance = FaceAeInfo.face_ae_tolerance;
        face_ae_cfg_load.face_ae_speed = FaceAeInfo.face_ae_speed;
        face_ae_cfg_load.face_ae_target = FaceAeInfo.face_ae_target;
        face_ae_cfg_load.face_ae_delay_cnt = FaceAeInfo.face_ae_delay_cnt;
        face_ae_cfg_load.face_up_percent = FaceAeInfo.face_up_percent;
        face_ae_cfg_load.face_down_percent = FaceAeInfo.face_down_percent;
        face_ae_cfg_load.ae_face_block_num_thrd = FaceAeInfo.ae_face_block_num_thrd;
        face_ae_cfg_load.ae_face_block_weight = FaceAeInfo.ae_face_block_weight;
        face_ae_cfg_load.ae_over_face_max_exp_control = FaceAeInfo.ae_over_face_max_exp_control;
        if (real_vaild_cnt != 0) {
            memcpy(&face_ae_cfg_load.ae_face_win_weight, &FaceAeInfo.ae_face_win_weight, 16 * sizeof(unsigned short));
            memcpy(&face_ae_cfg_load.ae_face_pos_weight, &FaceAeInfo.ae_face_pos_weight, 64 * sizeof(HW_S32));
        }
        face_ae_cfg_load.enable = true;
    } else {
        alogd("It will disable faceAE!\n");
        face_ae_cfg_load.enable = false;
    }

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_FACE_CFG, &face_ae_cfg_load) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetFaceAeCfg(int *pIspId, struct isp_face_ae_attr_info *FaceAeInfo)
{
    struct ae_face_cfg face_ae_cfg_load;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (!FaceAeInfo) {
		aloge("FaceAeInfo is NULL, getFaceAeCfg failed\n");
		return FAILURE;
	}

	memset(&face_ae_cfg_load, 0, sizeof(struct ae_face_cfg));
	if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_FACE_CFG, &face_ae_cfg_load) < 0) {
		return FAILURE;
	}

	FaceAeInfo->enable = face_ae_cfg_load.enable;
	FaceAeInfo->vaild_face_num = face_ae_cfg_load.vaild_face_cnt;
	FaceAeInfo->face_ae_tolerance = face_ae_cfg_load.face_ae_tolerance;
	FaceAeInfo->face_ae_speed = face_ae_cfg_load.face_ae_speed;
	FaceAeInfo->face_ae_target = face_ae_cfg_load.face_ae_target;
	FaceAeInfo->face_ae_delay_cnt = face_ae_cfg_load.face_ae_delay_cnt;
	FaceAeInfo->face_up_percent = face_ae_cfg_load.face_up_percent;
	FaceAeInfo->face_down_percent = face_ae_cfg_load.face_down_percent;
	FaceAeInfo->ae_face_block_num_thrd = face_ae_cfg_load.ae_face_block_num_thrd;
	FaceAeInfo->ae_face_block_weight = face_ae_cfg_load.ae_face_block_weight;
	FaceAeInfo->ae_over_face_max_exp_control = face_ae_cfg_load.ae_over_face_max_exp_control;
	memcpy(&FaceAeInfo->ae_face_win_weight, &face_ae_cfg_load.ae_face_win_weight, 16 * sizeof(unsigned short));
	memcpy(&FaceAeInfo->ae_face_pos_weight, &face_ae_cfg_load.ae_face_pos_weight, 64 * sizeof(HW_S32));

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_ReadIspBin(int *pIspId, ISP_CFG_BIN_MODE ModeFlag, char *IspCfgBinPath)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (IspCfgBinPath == NULL) {
        aloge("Invalid IspCfgBinPath\n");
        return FAILURE;
    }

    isp_cfg_info isp_cfg_info_entity;
    isp_cfg_info_entity.mode_flag = ModeFlag;
    if (strlen(IspCfgBinPath) > 100) {
        aloge("IspCfgBinPath is more than 128!!!\n");
    } else {
        strcpy((char*)&isp_cfg_info_entity.isp_cfg_bin_path, IspCfgBinPath);
    }

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_READ_BIN_PARAM, &isp_cfg_info_entity) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetAiIsp(int *pIspId, isp_ai_isp_info *ai_isp_info_entity)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AI_ISP, ai_isp_info_entity) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}


ERRORTYPE videoInputHw_IspAwb_SetMode(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

//    if ((value != 0) && (value != 1))
//            return ERR_VI_INVALID_CHNID;
    if (value < 0 || value > 9) {
        aloge("fatal error! Please use <0~9> to set awb mode, value = [%d]", value);
        return ERR_VI_INVALID_PARA;
    }

    if (0 == value) {
        video_set_control(video, V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE, 1);// auto awb
    } else if (1 == value) {
        video_set_control(video, V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE, 0);// manual awb
    } else {
        video_set_control(video, V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE, value);
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_SetColorTemp(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (value == 0 || value == 1) {
        printf("Please use <videoInputHw_IspAwb_SetMode> to set AWB mode, value = [%d]\r\n", value);
        return -1;
    }

    if (value < 0 || value > 9) {
        printf("Please use <2~9> to set color temperature, value = [%d]\r\n", value);
        return ERR_VI_INVALID_CHNID;
    }

    if (video_set_control(video, V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAf_SetLock(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((value < 0) || (value > 1)) {
        ISP_ERR("Invalid option for set af lock!\n");
        return ERR_VI_INVALID_PARA;
    }

    if (value) {
        if (video_set_control(video, V4L2_CID_AUTO_FOCUS_STOP, value) < 0) {
            return FAILURE;
        }
    } else {
        if (video_set_control(video, V4L2_CID_AUTO_FOCUS_START, value) < 0) {
            return FAILURE;
        }
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAf_GetResult(int *pIspId, af_result_t *af_result)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (af_result == NULL) {
        aloge("af_result is NULL!!!\n");
        return ERR_VI_INVALID_PARA;
    }

	if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AF_RESULT, af_result) < 0) {
        return FAILURE;
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAf_SetAuto(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((value != 0) && (value != 1)) {
        aloge("fatal error! Please use <0~1> to set af auto, value = [%d]", value);
        return ERR_VI_INVALID_PARA;
    }

    if (video_set_control(video, V4L2_CID_FOCUS_AUTO, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAf_SetAbsolute(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (value < 0 || value > 127) {
        aloge("fatal error! Please use <0~127> to set af absolute, value = [%d]", value);
        return ERR_VI_INVALID_PARA;
    }

    if (video_set_control(video, V4L2_CID_FOCUS_ABSOLUTE, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_SetStatsSyncMode(int *pIspId, ISP_AWB_STATS_MODE IspAwbStatsMode)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((IspAwbStatsMode < ISP_AWB_DEFAULT) || (IspAwbStatsMode > AWB_STATS_MODE_MAX)) {
        ISP_ERR("Invalid IspAwbStatsMode!!!!\n");
        return FAILURE;
    }

    if (isp_set_sync(IspAwbStatsMode) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetFlicker(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value == 0) || (value == 1) || (value == 2) || (value == 3)) {
		if (video_set_control(video, V4L2_CID_POWER_LINE_FREQUENCY, value) < 0) {
	        return FAILURE;
	    }
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetMirror(int *pvipp_id, int value)
{
    VI_DEV nVippId = *pvipp_id;
    struct isp_video_device *video = NULL;

    if (nVippId >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[nVippId])
    {
        ISP_ERR("VIN CH[%d] number is invalid!\n", nVippId);
        return ERR_VI_INVALID_CHNID;
    }
    else
    {
        video = gpVIDevManager->media->video_dev[nVippId];
    }

	if ((value == 0) || (value == 1)) {
		if (video_set_control(video, V4L2_CID_HFLIP, value) < 0) {
	        return FAILURE;
	    }
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetFlip(int *pvipp_id, int value)
{
    VI_DEV nVippId = *pvipp_id;
    struct isp_video_device *video = NULL;

    if (nVippId >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[nVippId])
    {
        ISP_ERR("VIN CH[%d] number is invalid!\n", nVippId);
        return ERR_VI_INVALID_CHNID;
    }
    else
    {
        video = gpVIDevManager->media->video_dev[nVippId];
    }

	if ((value == 0) || (value == 1)) {
		if (video_set_control(video, V4L2_CID_VFLIP, value) < 0) {
        	return FAILURE;
    	}
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetBrightness(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= 0) && (value <= 512)) {
		if (video_set_control(video, V4L2_CID_BRIGHTNESS, value) < 0) {
        	return FAILURE;
    	}
	}

    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_SetContrast(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= 0) && (value <= 512)) {
		if (video_set_control(video, V4L2_CID_CONTRAST, value) < 0) {
        	return FAILURE;
    	}
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetSaturation(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((value >= 0) && (value <= 768)) {
        if (video_set_control(video, V4L2_CID_SATURATION, value) < 0) {
            return FAILURE;
        }
    } else {
        ISP_ERR("invalid parameter range, [-256, 512]");
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetSharpness(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((value < 1) || (value > 4095)) {
        aloge("out of range, should be[-32~32], value(%d)", value);
        return FAILURE;
    }

    if (video_set_control(video, V4L2_CID_SHARPNESS, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_SetHue(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= 0) && (value <= 255)) {
		if (video_set_control(video, V4L2_CID_HUE, value) < 0) {
        	return FAILURE;
    	}
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetColorEffect(int *pIspId, enum colorfx ColorStyle)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_COLOR_EFFECT, &ColorStyle) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetScene(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (video_set_control(video, V4L2_CID_SCENE_MODE, value) < 0) {
		return FAILURE;
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetIrStatus(int *pIspId, ISP_CFG_MODE ModeFlag)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((ModeFlag < 0) || (ModeFlag > 3)) {
        ISP_ERR("Invalid option for set ISP ir status!\n");
        return FAILURE;
    }

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_IR_STATUS, &ModeFlag) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetD3dLbcRatio(int *pIspId, unsigned int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((value < 100) || (value > 400)) {
        ISP_ERR("Invalid lbc_ratio, please set ratio from [100,400]!\n");
        return FAILURE;
    }

    if (isp_set_d3d_lbc_ratio(video, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetStitchMode(int *pIspId, enum stitch_mode_t stitch_mode)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((stitch_mode < STITCH_NONE) || (stitch_mode > STITCH_MODE_MAX)) {
        ISP_ERR("Invalid stitch_mode!\n");
        return FAILURE;
    }

    if (isp_set_stitch_mode(nIspId, stitch_mode) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetSensorFps(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((value <= 0)) {
        ISP_ERR("Invalid fps = %d!\n", value);
        return FAILURE;
    }

    if (isp_set_fps(nIspId, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

/* ==================================================== */
/* Isp set api. end */
/* ==================================================== */
#endif

#if 1
/* ==================================================== */
/* Isp get api. start */
/* ==================================================== */
ERRORTYPE videoInputHw_IspAe_GetMode(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_EXPOSURE_AUTO, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetExposureBias(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_AUTO_EXPOSURE_BIAS, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}
ERRORTYPE videoInputHw_IspAe_GetExposure(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_EXPOSURE_ABSOLUTE, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetISOSensitiveMode(int *pIspId, int *mode)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_ISO_SENSITIVITY_AUTO, mode) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetISOSensitive(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_ISO_SENSITIVITY_AUTO, value) < 0) {
        return FAILURE;
    } else {
        if (1 == *value) {
            *value = 0;
            return SUCCESS;
        }
    }

    if (video_get_control(video, V4L2_CID_ISO_SENSITIVITY, value) < 0) {
        return FAILURE;
    }
    *value = *value + 1;

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetMetering(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_EXPOSURE_METERING, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}
ERRORTYPE videoInputHw_IspAe_GetGain(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_GAIN, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_GetMode(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE, value) < 0) {
        return FAILURE;
    }
	if (*value == 0)
		*value = 1;
	else if (*value == 1)
		*value = 0;

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_GetColorTemp(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_GetFlicker(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (video_get_control(video, V4L2_CID_POWER_LINE_FREQUENCY, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_GetMirror(int *pvipp_id, int *value)
{
    VI_DEV nVippId = *pvipp_id;
    struct isp_video_device *video = NULL;

    if (nVippId >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[nVippId])
    {
        ISP_ERR("VIN CH[%d] number is invalid!\n", nVippId);
        return ERR_VI_INVALID_CHNID;
    }
    else
    {
        video = gpVIDevManager->media->video_dev[nVippId];
    }

	if (video_get_control(video, V4L2_CID_HFLIP, value) < 0) {
     	return FAILURE;
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_GetFlip(int *pvipp_id, int *value)
{
    VI_DEV nVippId = *pvipp_id;
    struct isp_video_device *video = NULL;

    if (nVippId >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[nVippId])
    {
        ISP_ERR("VIN CH[%d] number is invalid!\n", nVippId);
        return ERR_VI_INVALID_CHNID;
    }
    else
    {
        video = gpVIDevManager->media->video_dev[nVippId];
    }

	if (video_get_control(video, V4L2_CID_VFLIP, value) < 0) {
    	return FAILURE;
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_GetBrightness(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (video_get_control(video, V4L2_CID_BRIGHTNESS, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_GetContrast(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (video_get_control(video, V4L2_CID_CONTRAST, value) < 0) {
    	return FAILURE;
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_GetSaturation(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (video_get_control(video, V4L2_CID_SATURATION, value) < 0) {
    	return FAILURE;
	}

    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_GetSharpness(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (video_get_control(video, V4L2_CID_SHARPNESS, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_GetHue(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (video_get_control(video, V4L2_CID_HUE, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_GetScene(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (video_get_control(video, V4L2_CID_SCENE_MODE, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}

#include "isp_tuning_priv.h"
#include "isp_tuning.h"
#include "isp.h"
ERRORTYPE videoInputHw_Isp_SetWDR(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= -2500) && (value <= 2500)) {
		if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_PLTMWDR_STR, &value) < 0) {
        	return FAILURE;
    	}
	}

    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_GetWDR(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_PLTMWDR_STR, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_GetPltmNextStren(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (isp_get_attr_cfg(nIspId, ISP_CTRL_PLTM_HARDWARE_STR, value) < 0) {
        return FAILURE;
	}
    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_SetNR(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= 1) && (value <= 4095)) {
		if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_DN_STR, &value) < 0) {
		return FAILURE;
		}
	}

    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_GetNR(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_DN_STR, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_Set3DNR(int *pIspId, int value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= 1) && (value <= 4095)) {
		if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_3DN_STR, &value) < 0) {
		return FAILURE;
		}
	}

    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_Get3DNR(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_3DN_STR, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetSensorMipiSwitch(int *pIspId, struct sensor_mipi_switch_entity *switch_entity)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (!switch_entity)
    {
        aloge("switch_entity is NULL!!!\n");
         return FAILURE;
    }

    if (switch_entity->switch_ctrl != SET_SWITCH) {
        alogw("switch_ctrl should set SET_SWITCH!!\n");
        switch_entity->switch_ctrl = SET_SWITCH;
    }

    if (switch_entity->mipi_switch_status < SWITCH_A || switch_entity->mipi_switch_status > SWITCH_MAX) {
        aloge("Invaild switch_choice!!!\r\n");
        return -1;
    }

#if MPPCFG_SUPPORT_FASTBOOT
    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_MIPI_SWITCH, switch_entity) < 0) {
        aloge("fatal error, set isp_sensor_mipi_switch_ctrl failed!");
        return FAILURE;
    }
#else
    if (isp_sensor_mipi_switch_ctrl(nIspId, video, switch_entity) < 0) {
        aloge("fatal error, set isp_sensor_mipi_switch_ctrl failed!");
        return FAILURE;
    }
#endif

    return SUCCESS;
}
ERRORTYPE videoInputHw_Isp_GetSensorMipiSwitch(int *pIspId, struct sensor_mipi_switch_entity *switch_entity)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (!switch_entity)
    {
        aloge("switch_entity is NULL!!!\n");
         return FAILURE;
    }

    if (switch_entity->switch_ctrl != GET_SWITCH) {
        alogw("switch_ctrl should set GET_SWITCH!!\n");
        switch_entity->switch_ctrl = GET_SWITCH;
    }

#if MPPCFG_SUPPORT_FASTBOOT
    if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_MIPI_SWITCH, switch_entity) < 0) {
        aloge("fatal error, set isp_sensor_mipi_switch_ctrl failed!");
        return FAILURE;
    }
#else
    if (isp_sensor_mipi_switch_ctrl(nIspId, video, switch_entity) < 0) {
        aloge("fatal error, set isp_sensor_mipi_switch_ctrl failed!");
        return FAILURE;
    }
#endif

    return SUCCESS;
}
#if 0
ERRORTYPE videoInputHw_Isp_GetIsp2VeParam(int *pIspId, struct enc_VencIsp2VeParam *pIsp2VeParam)
{
    int nIspId = *pIspId;
    int i, isp_id, found = 0;

    struct isp_video_device *video = NULL;
    if ((nIspId < 0) || (nIspId >= MAX_ISP_DEV_NUM)) {
        ISP_ERR("ISP ID[%d] number is invalid!\n", nIspId);
        return ERR_VI_INVALID_CHNID;
    } else {
        for (i = 0; i < MAX_VIPP_DEV_NUM; i++) {
            video = gpVIDevManager->media->video_dev[i];
            if (NULL == video) {
                continue ;
            }
            isp_id = video_to_isp_id(gpVIDevManager->media->video_dev[i]);
            if (isp_id == nIspId) {
                //alogd("isp[%d]2vipp[%d].\r\n", nIspId, i);
                found = 1;
                break;
            }
        }
    }

    if (0 == found) {
        aloge("No find video open @ isp[%d].\r\n", nIspId);
        return -1;
    }

    struct sensor_config stConfig;
    memset(&stConfig, 0, sizeof(struct sensor_config));
    if (isp_get_sensor_info(isp_id, &stConfig) < 0) {
        aloge("fatal error! Get isp[%d] sensor information failed!", isp_id);
        return FAILURE;
    }

    /*
        ISP will not process YUV Data, mbus_code will be set in Sensor Driver,
        RAW format's range is [0x3001, 0x3012]
    */
    if (stConfig.mbus_code < 0x3001) {
        /* It will do nothing */
        return 0;
    }

    if (NULL == pIsp2VeParam) {
        aloge("fatal error! invalid input param!");
        return ERR_VI_INVALID_PARA;
    }

    if (isp_get_encpp_cfg_ctrl(isp_id, video, ISP_CTRL_ENCPP_EN, (void*)&pIsp2VeParam->encpp_en)) {
        aloge("fatal error, get ISP_CTRL_ENCPP_EN failed!");
        return FAILURE;
    }

    if (isp_get_encpp_cfg_ctrl(isp_id, video, ISP_CTRL_ENCPP_STATIC_CFG, &pIsp2VeParam->mStaticSharpCfg)) {
        aloge("fatal error, get ISP_CTRL_ENCPP_STATIC_CFG failed!");
        return FAILURE;
    }

    if (isp_get_encpp_cfg_ctrl(isp_id, video, ISP_CTRL_ENCPP_DYNAMIC_CFG, &pIsp2VeParam->mDynamicSharpCfg)) {
        aloge("fatal error, get ISP_CTRL_ENCPP_DYNAMIC_CFG failed!");
        return FAILURE;
    }

    if (pIsp2VeParam->AeStatsInfo)
    {
        if (isp_get_attr_cfg_ctrl(isp_id, video, ISP_CTRL_AE_STATS, pIsp2VeParam->AeStatsInfo)) {
            aloge("fatal error, get ISP_CTRL_AE_STATS failed!");
            return FAILURE;
        }
    }

    return SUCCESS;
}
#endif

/**
  @return
    SUCCESS
    FAILURE
    ERR_VI_INVALID_CHNID
    ERR_VI_INVALID_PARA
    ERR_VI_NOT_SUPPORT: ISP will not process YUV Data, mbus_code will be set in Sensor Driver,
        RAW format's range is [0x3001, 0x3012]
*/
ERRORTYPE videoInputHw_Isp_GetIsp2VeParam_AeStats(VI_DEV nVippId, struct isp_ae_stats_s *pAeStatsInfo)
{
    int isp_id;
    ERRORTYPE result = SUCCESS;
    int ret;

    struct isp_video_device *video = NULL;
    if (nVippId >= HW_VIDEO_DEVICE_NUM || NULL == gpVIDevManager->media->video_dev[nVippId])
    {
        aloge("vipp[%d] is invalid!\n", nVippId);
        return ERR_VI_INVALID_CHNID;
    }
    else
    {
        video = gpVIDevManager->media->video_dev[nVippId];
    }
    isp_id = video_to_isp_id(gpVIDevManager->media->video_dev[nVippId]);

    struct sensor_config stConfig;
    memset(&stConfig, 0, sizeof(struct sensor_config));
    if (isp_get_sensor_info(isp_id, &stConfig) < 0) {
        aloge("fatal error! Get isp[%d] sensor information failed!", isp_id);
        return FAILURE;
    }

    /*
        ISP will not process YUV Data, mbus_code will be set in Sensor Driver,
        RAW format's range is [0x3001, 0x3012]
    */
    if (stConfig.mbus_code < 0x3001)
    {
        /* It will do nothing */
        return ERR_VI_NOT_SUPPORT;
    }

    if (pAeStatsInfo)
    {
        ret = isp_get_attr_cfg_ctrl(isp_id, video, ISP_CTRL_AE_STATS, pAeStatsInfo);
        if (ret != 0)
        {
            aloge("fatal error, isp[%d] get ISP_CTRL_AE_STATS failed[%d]!", isp_id, ret);
            result = FAILURE;
        }
    }
    else
    {
        result = ERR_VI_INVALID_PARA;
    }
    return result;
}

ERRORTYPE videoInputHw_Isp_GetIsp2VeParam_EncppSharpEn(ISP_DEV isp_id, int *pEncppSharpEnable)
{
    //int isp_id;
    ERRORTYPE result = SUCCESS;
    int ret;

    struct isp_video_device *video = GetVideoByIspDev(isp_id);
    if (video == NULL)
    {
        return ERR_VI_INVALID_CHNID;
    }

    struct sensor_config stConfig;
    memset(&stConfig, 0, sizeof(struct sensor_config));
    if (isp_get_sensor_info(isp_id, &stConfig) < 0)
    {
        aloge("fatal error! Get isp[%d] sensor information failed!", isp_id);
        return FAILURE;
    }

    /*
        ISP will not process YUV Data, mbus_code will be set in Sensor Driver,
        RAW format's range is [0x3001, 0x3012]
    */
    if (stConfig.mbus_code < 0x3001)
    {
        /* It will do nothing */
        return ERR_VI_NOT_SUPPORT;
    }

    if (pEncppSharpEnable)
    {
        ret = isp_get_encpp_cfg_ctrl(isp_id, video, ISP_CTRL_ENCPP_EN, (void*)pEncppSharpEnable);
        if (ret != 0)
        {
            aloge("fatal error! isp[%d] get ISP_CTRL_ENCPP_EN failed[%d]!", isp_id, ret);
            result = FAILURE;
        }
    }
    else
    {
        result = ERR_VI_INVALID_PARA;
    }
    return result;
}

ERRORTYPE videoInputHw_Isp_GetIsp2VeParam_EncppSharpCfg(ISP_DEV isp_id, IspEncppSharpConfig *pEncppSharpCfg)
{
    //int isp_id;
    ERRORTYPE result = SUCCESS;
    int ret;

    struct isp_video_device *video = GetVideoByIspDev(isp_id);
    if (video == NULL)
    {
        return ERR_VI_INVALID_CHNID;
    }

    struct sensor_config stConfig;
    memset(&stConfig, 0, sizeof(struct sensor_config));
    if (isp_get_sensor_info(isp_id, &stConfig) < 0)
    {
        aloge("fatal error! Get isp[%d] sensor information failed!", isp_id);
        return FAILURE;
    }

    /*
        ISP will not process YUV Data, mbus_code will be set in Sensor Driver,
        RAW format's range is [0x3001, 0x3012]
    */
    if (stConfig.mbus_code < 0x3001)
    {
        /* It will do nothing */
        return ERR_VI_NOT_SUPPORT;
    }

    if (NULL == pEncppSharpCfg)
    {
        aloge("fatal error! invalid input param!");
        return ERR_VI_INVALID_PARA;
    }
    ret = isp_get_encpp_cfg_ctrl(isp_id, video, ISP_CTRL_ENCPP_STATIC_CFG, &pEncppSharpCfg->stStaticSharpCfg);
    if (ret != 0)
    {
        aloge("fatal error! isp[%d] get ISP_CTRL_ENCPP_STATIC_CFG failed[%d]!", isp_id, ret);
        result = FAILURE;
    }
    ret = isp_get_encpp_cfg_ctrl(isp_id, video, ISP_CTRL_ENCPP_DYNAMIC_CFG, &pEncppSharpCfg->stDynamicSharpCfg);
    if (ret != 0)
    {
        aloge("fatal error! isp[%d] get ISP_CTRL_ENCPP_DYNAMIC_CFG failed[%d]!", isp_id, ret);
        result = FAILURE;
    }

    //alogd("vipp/isp:[%dx%d/%dx%d]", video->format.width, video->format.height, stConfig.width, stConfig.height);
    int nEncppSharpAttenCoefPer = video->format.width*100/stConfig.width;
    if (nEncppSharpAttenCoefPer > 0 && nEncppSharpAttenCoefPer != 100)
    {
        pEncppSharpCfg->stDynamicSharpCfg.ss_blk_stren = (HW_U16)((int)pEncppSharpCfg->stDynamicSharpCfg.ss_blk_stren
            * nEncppSharpAttenCoefPer / 100);
        pEncppSharpCfg->stDynamicSharpCfg.ss_wht_stren = (HW_U16)((int)pEncppSharpCfg->stDynamicSharpCfg.ss_wht_stren
            * nEncppSharpAttenCoefPer / 100);
        pEncppSharpCfg->stDynamicSharpCfg.ls_blk_stren = (HW_U16)((int)pEncppSharpCfg->stDynamicSharpCfg.ls_blk_stren
            * nEncppSharpAttenCoefPer / 100);
        pEncppSharpCfg->stDynamicSharpCfg.ls_wht_stren = (HW_U16)((int)pEncppSharpCfg->stDynamicSharpCfg.ls_wht_stren
            * nEncppSharpAttenCoefPer / 100);
    }
    return result;
}

ERRORTYPE videoInputHw_Isp_SetVe2IspParam(int *pIspId, struct enc_VencVe2IspParam *pVe2IspParam)
{
    int nIspId = *pIspId;

    struct isp_video_device *video = GetVideoByIspDev(nIspId);
    if (video == NULL)
    {
        return ERR_VI_INVALID_CHNID;
    }

    /*
        ISP will not process YUV Data, mbus_code will be set in Sensor Driver,
        RAW format's range is [0x3001, 0x3012]
    */
    struct sensor_config stConfig;
    memset(&stConfig, 0, sizeof(struct sensor_config));
    if (isp_get_sensor_info(nIspId, &stConfig) < 0)
    {
        aloge("fatal error! Get isp[%d] sensor information failed!", nIspId);
        return FAILURE;
    }

    if (stConfig.mbus_code < 0x3001)
    {
        /* It will do nothing */
        return 0;
    }

    if (isp_set_attr_cfg_ctrl(nIspId, video, ISP_CTRL_VENC2ISP_PARAM, (void*)pVe2IspParam) < 0)
    {
        aloge("fatal error, set ISP_CTRL_VENC2ISP_PARAM failed!");
        return FAILURE;
    }

    return SUCCESS;
}
ERRORTYPE videoInputHw_IspAwb_SetRGain(int *pIspId, int value)
{
    struct isp_wb_gain wb_gain;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= 256) && (value <= 256 * 64)) {
		memset(&wb_gain, 0, sizeof(struct isp_wb_gain));
		if (isp_get_attr_cfg(nIspId, ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
			return FAILURE;
		}
		if(wb_gain.r_gain != value){
			wb_gain.r_gain = value;
			if (isp_set_attr_cfg(nIspId,
					ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
				return FAILURE;
			}
		}
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_GetRGain(int *pIspId, int *value)
{
    struct isp_wb_gain wb_gain;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    memset(&wb_gain, 0, sizeof(struct isp_wb_gain));
    if (isp_get_attr_cfg(nIspId, ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
	    return FAILURE;
    }
    *value = wb_gain.r_gain;


    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_SetBGain(int *pIspId, int value)
{
    struct isp_wb_gain wb_gain;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= 256) && (value <= 256 * 64)) {
		memset(&wb_gain, 0, sizeof(struct isp_wb_gain));
		if (isp_get_attr_cfg(nIspId, ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
			return FAILURE;
		}
		if(wb_gain.b_gain != value){
			wb_gain.b_gain = value;
			if (isp_set_attr_cfg(nIspId,
					ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
				return FAILURE;
			}
		}

	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_GetBGain(int *pIspId, int *value)
{
    struct isp_wb_gain wb_gain;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    memset(&wb_gain, 0, sizeof(struct isp_wb_gain));
    if (isp_get_attr_cfg(nIspId, ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
	return FAILURE;
    }
    *value = wb_gain.b_gain;

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_SetGrGain(int *pIspId, int value)
{
    struct isp_wb_gain wb_gain;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= 256) && (value <= 256 * 64)) {
		memset(&wb_gain, 0, sizeof(struct isp_wb_gain));
		if (isp_get_attr_cfg(nIspId, ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
			return FAILURE;
		}
		if(wb_gain.gr_gain != value){
			wb_gain.gr_gain = value;
			if (isp_set_attr_cfg(nIspId,
					ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
				return FAILURE;
			}
		}
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_GetGrGain(int *pIspId, int *value)
{
    struct isp_wb_gain wb_gain;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    memset(&wb_gain, 0, sizeof(struct isp_wb_gain));
    if (isp_get_attr_cfg(nIspId, ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
	    return FAILURE;
    }
    *value = wb_gain.gr_gain;

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_SetGbGain(int *pIspId, int value)
{
    struct isp_wb_gain wb_gain;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if ((value >= 256) && (value <= 256 * 64)) {
		memset(&wb_gain, 0, sizeof(struct isp_wb_gain));
		if (isp_get_attr_cfg(nIspId, ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
			return FAILURE;
		}
		if(wb_gain.gb_gain != value){
			wb_gain.gb_gain = value;
			if (isp_set_attr_cfg(nIspId,
					ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
				return FAILURE;
			}
		}
	}

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_GetGbGain(int *pIspId, int *value)
{
    struct isp_wb_gain wb_gain;
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    memset(&wb_gain, 0, sizeof(struct isp_wb_gain));
    if (isp_get_attr_cfg(nIspId, ISP_CTRL_WB_MGAIN, &wb_gain) < 0) {
	    return FAILURE;
    }
    *value = wb_gain.gb_gain;

    return SUCCESS;
}

//add by jason

ERRORTYPE videoInputHw_IspAe_GetExposureLine(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (video_get_control(video, V4L2_CID_EXPOSURE, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAwb_GetCurColorT(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_COLOR_TEMP, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetEvIdx(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_EV_IDX, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetMaxEvIdx(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_MAX_EV_IDX, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetAeLock(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_LOCK, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetISOLumIdx(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_ISO_LUM_IDX, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetEvLv(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;

    struct isp_video_device *video = GetVideoByIspDev(nIspId);
    if (video == NULL)
    {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_EV_LV, value) < 0)
    {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetEvLvAdj(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_EV_LV_ADJ, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_GetIrStatus(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_IR_STATUS, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_GetIrAwbGain(int *pIspId, int *rgain_ir, int *bgain_ir)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

#if MPPCFG_SUPPORT_FASTBOOT
    struct isp_ir_awb_gain awb_ir_gain;
    memset(&awb_ir_gain, 0, sizeof(struct isp_ir_awb_gain));
    if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_IR_AWB_GAIN, &awb_ir_gain) < 0) {
        return FAILURE;
    } else {
        *rgain_ir = awb_ir_gain.awb_rgain_ir;
        *bgain_ir = awb_ir_gain.awb_bgain_ir;
    }
#else
    if (isp_get_awb_gain_ir(nIspId, rgain_ir, bgain_ir) < 0) {
        return FAILURE;
    }
#endif

    return SUCCESS;
}

ERRORTYPE videoInputHw_IspAe_GetWeightLum(int *pIspId, int *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_get_attr_cfg_ctrl(nIspId, video, ISP_CTRL_AE_WEIGHT_LUM, value) < 0) {
        return FAILURE;
    }
    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetSpecialScene(int *pIspId, scene_mode_t target_scene)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if ((target_scene < 0) || (target_scene > ISP_AI_SCENE_MODE_MAX)) {
        aloge("Invaild target_scene\r\n");
        return FAILURE;
    }

    if (isp_set_scene(nIspId, target_scene) < 0) {
        aloge("fatal error, isp_set_scene failed!");
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_SetIspAlgoFreq(int *pIspId, unsigned short value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

    if (isp_set_algo_freq_div(nIspId, value) < 0) {
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE videoInputHw_Isp_GetIspAlgoFreq(int *pIspId, unsigned short *value)
{
    ISP_DEV nIspId = *pIspId;
    struct isp_video_device *video = NULL;

    video = GetVideoByIspDev(nIspId);
    if (video == NULL) {
        return ERR_VI_INVALID_CHNID;
    }

	if (isp_get_algo_freq_div(nIspId, value) < 0) {
    	return FAILURE;
	}
    return SUCCESS;
}

/* ==================================================== */
/* Isp get api. end */
/* ==================================================== */
#endif

/**
  judge if pixel format is compress.

  @return
    true: is compress, e.g., V4L2_PIX_FMT_NV21
    false: normal, e.g., V4L2_PIX_FMT_LBC_2_5X
*/
static bool IsV4L2_PIX_FMTCompress(int nPixFmt)
{
    bool bCompressFlag = false;
    switch(nPixFmt)
    {
        case V4L2_PIX_FMT_LBC_1_0X:
        case V4L2_PIX_FMT_LBC_1_5X:
        case V4L2_PIX_FMT_LBC_2_0X:
        case V4L2_PIX_FMT_LBC_2_5X:
        {
            bCompressFlag = true;
            break;
        }
        default:
        {
            bCompressFlag = false;
            break;
        }
    }
    return bCompressFlag;
}
/**
  get frame from isp_video_device.

  @return
    ERR_VI_INVALID_CHNID
    ERR_VI_BUF_EMPTY: dequeue buffer fail.
    ERR_VI_NOT_PERM: data is wrong.
    SUCCESS:
*/
static ERRORTYPE videoInputHw_GetData(int nVipp, VIDEO_FRAME_INFO_S *pstFrameInfo)
{
    if (nVipp >= HW_VIDEO_DEVICE_NUM)
    {
        aloge("fatal error! vipp[%d] number is invalid!", nVipp);
        return ERR_VI_INVALID_CHNID;
    }
    viChnManager *pVippInfo = gpVIDevManager->gpVippManager[nVipp];
    struct isp_video_device *video = gpVIDevManager->media->video_dev[nVipp];
    struct video_buffer buffer;
    struct video_fmt vfmt;
    int i;
    int ev_value;

    if (video_dequeue_buffer(video, &buffer) < 0)
    {
        return ERR_VI_BUF_EMPTY;
    }
    int isp_id = video_to_isp_id(video);
    memset(&vfmt, 0, sizeof(vfmt));
    video_get_fmt(video, &vfmt);
    unsigned int width_stride = AWALIGN(vfmt.format.width, VIN_ALIGN_WIDTH);
    unsigned int u_width_stride = AWALIGN(vfmt.format.width/2, VIN_ALIGN_WIDTH/2);
    for (i = 0; i < vfmt.nplanes; i++)
    {
        pstFrameInfo->VFrame.mpVirAddr[i] = buffer.planes[i].mem;
        if(IsV4L2_PIX_FMTCompress(vfmt.format.pixelformat))
        {
            pstFrameInfo->VFrame.mStride[i] = buffer.planes[i].size; //buffer.planes[i].size, 0
        }
        else
        {
            pstFrameInfo->VFrame.mStride[i] = width_stride;
        }
        pstFrameInfo->VFrame.mPhyAddr[i] = buffer.planes[i].mem_phy;
    }

    //set addr[1] and addr[2] for one buffer pixel format.
    if(V4L2_PIX_FMT_NV21 == vfmt.format.pixelformat || V4L2_PIX_FMT_NV12 == vfmt.format.pixelformat
        || V4L2_PIX_FMT_YUV420 == vfmt.format.pixelformat || V4L2_PIX_FMT_YVU420 == vfmt.format.pixelformat
        || V4L2_PIX_FMT_NV61 == vfmt.format.pixelformat || V4L2_PIX_FMT_NV16 == vfmt.format.pixelformat)
    {
        //set all pstFrameInfo->VFrame.mpVirAddr for compatibility.
      #if (AWCHIP == AW_V853)
        int ySize = AWALIGN(vfmt.format.width, VIN_ALIGN_WIDTH) * AWALIGN(vfmt.format.height, VIN_ALIGN_HEIGHT);
      #else
        int ySize = AWALIGN(vfmt.format.width, VIN_ALIGN_WIDTH) * vfmt.format.height;
      #endif
        pstFrameInfo->VFrame.mpVirAddr[1] = buffer.planes[0].mem + ySize;
        if(V4L2_PIX_FMT_YUV420 == vfmt.format.pixelformat || V4L2_PIX_FMT_YVU420 == vfmt.format.pixelformat)
        {
            pstFrameInfo->VFrame.mStride[1] = u_width_stride;
        }
        else
        {
            pstFrameInfo->VFrame.mStride[1] = width_stride;
        }
        pstFrameInfo->VFrame.mPhyAddr[1] = buffer.planes[0].mem_phy + ySize;
        //yu12 yv12 is YUV420 Planar, YUV are stored separately.
        if (V4L2_PIX_FMT_YUV420 == vfmt.format.pixelformat || V4L2_PIX_FMT_YVU420 == vfmt.format.pixelformat)
        {
            pstFrameInfo->VFrame.mpVirAddr[2] = buffer.planes[0].mem + ySize + ySize/4;
            pstFrameInfo->VFrame.mStride[2] = u_width_stride;
            pstFrameInfo->VFrame.mPhyAddr[2] = buffer.planes[0].mem_phy + ySize + ySize/4;
        }
    }

#if (AWCHIP == AW_V853)
    //v853 offline mode, when enable encpp, need fill left memory space to avoid green bottom of encoded frame.
    if ((0 == vfmt.ve_online_en) && (TRUE == pVippInfo->mstAttr.mbEncppEnable))
    {
        unsigned int height = pVippInfo->mstAttr.format.height;
        unsigned int height_diff = AWALIGN(height, VIN_ALIGN_HEIGHT) - height;
        if (0 != height_diff)
        {
            struct SunxiMemOpsS *pMemOps = GetMemAdapterOpsS();
            if(IsV4L2_PIX_FMTCompress(vfmt.format.pixelformat))
            {
                alogv("vipp:%d, LBC pix:0x%x, height_diff:%d", nVipp, vfmt.format.pixelformat, height_diff);
                void* baseAddr = buffer.planes[0].mem;
                unsigned int line_bits_sum = (pVippInfo->mLbcCmp.line_tar_bits[0] + pVippInfo->mLbcCmp.line_tar_bits[1]) / 8;
                /*unsigned int offset_src = line_bits_sum/2*height - line_bits_sum;
                unsigned int offset_dst = 0;
                for (i = 0; i < height_diff/2; i++) {
                    offset_dst = line_bits_sum/2*height + line_bits_sum * i;
                    //alogd("i:%d, baseAddr:0x%x, offset_src:%d, offset_dst:%d, line_bits_sum:%d", i, baseAddr, offset_src, offset_dst, line_bits_sum);
                    memcpy(baseAddr + offset_dst, baseAddr + offset_src, line_bits_sum);
                }
                */
                /*unsigned int offset_src = line_bits_sum/2*height - line_bits_sum*height_diff/2;
                unsigned int offset_dst = line_bits_sum/2*height;
                alogd("ViCh:%d, LBC pix:0x%x, base:0x%x, src:%d, dst:%d, %d %d", ViCh, vfmt.format.pixelformat, (unsigned int)baseAddr, offset_src, offset_dst, buffer.planes[0].size, buffer.planes[1].size);
                memcpy(baseAddr + offset_dst, baseAddr + offset_src, line_bits_sum*height_diff/2);
                ion_flushCache_check(baseAddr + offset_dst, line_bits_sum*height_diff/2, 0);
                */
                unsigned int offset_dst = line_bits_sum/2*height;
                alogv("vipp:%d, LBC pix:0x%x, base:%p, dst:%d, src:%p, len:%d, %d %d", nVipp, vfmt.format.pixelformat,
                    baseAddr, offset_dst, pVippInfo->mLbcFillDataAddr, pVippInfo->mLbcFillDataLen, buffer.planes[0].size,
                    buffer.planes[1].size);
                unsigned int mDataLen = line_bits_sum*height_diff/2;
                if (pVippInfo->mLbcFillDataLen != mDataLen)
                {
                    alogw("fatal error! wrong data len %d != %d", pVippInfo->mLbcFillDataLen, mDataLen);
                }
                // check data len and buf size, avoid cross-border error
                if (offset_dst + pVippInfo->mLbcFillDataLen > buffer.planes[0].size)
                {
                    aloge("fatal error! LBC wrong data len or buf size, offset_dst:%d + data_len:%d > size:%d",
                        offset_dst, pVippInfo->mLbcFillDataLen, buffer.planes[0].size);
                    return ERR_VI_NOT_PERM;
                }
                memcpy(baseAddr + offset_dst, pVippInfo->mLbcFillDataAddr, pVippInfo->mLbcFillDataLen);
                SunxiMemFlushCache(pMemOps, baseAddr + offset_dst, pVippInfo->mLbcFillDataLen);
            }
            else
            {
                alogv("vipp:%d, YUV pix:0x%x, height_diff:%d", nVipp, vfmt.format.pixelformat, height_diff);
                // fill Y
                unsigned int width_align = AWALIGN(vfmt.format.width, VIN_ALIGN_WIDTH);
                unsigned int offset_src = width_align*height - (width_align*1);
                unsigned int offset_dst = 0;
                // check data len and buf size, avoid cross-border error
                unsigned int height_align = AWALIGN(vfmt.format.height, VIN_ALIGN_HEIGHT);
                unsigned int buf_total_size = 0;
                for (i = 0; i < vfmt.nplanes; i++)
                {
                    if (0 < buffer.planes[i].size)
                    {
                        buf_total_size += buffer.planes[i].size;
                    }
                }
                if (width_align*height_align*3/2 > buf_total_size)
                {
                    aloge("fatal error! YUV wrong data len or buf size, width_align:%d*height_align:%d*3/2 > size:%d, pixelformat 0x%x",
                        width_align, height_align, buffer.planes[0].size, vfmt.format.pixelformat);
                    return ERR_VI_NOT_PERM;
                }
                void* baseAddr = buffer.planes[0].mem;
                for (i = 0; i < height_diff; i++)
                {
                    offset_dst = width_align*height + (width_align*i);
                    memcpy(baseAddr + offset_dst, baseAddr + offset_src, width_align);
                }
                SunxiMemFlushCache(pMemOps, baseAddr + width_align*height, width_align*height_diff);

                int ySize = AWALIGN(vfmt.format.width, VIN_ALIGN_WIDTH) * AWALIGN(vfmt.format.height, VIN_ALIGN_HEIGHT);
                if (V4L2_PIX_FMT_NV21 == vfmt.format.pixelformat || V4L2_PIX_FMT_NV12 == vfmt.format.pixelformat)
                {
                    // fill UV
                    int uvData= AWALIGN(vfmt.format.width, VIN_ALIGN_WIDTH) * height / 2;
                    offset_src = ySize + uvData - (width_align*1);
                    for (i = 0; i < height_diff/2; i++)
                    {
                        offset_dst = ySize + uvData + (width_align*i);
                        //alogd("i:%d, baseAddr:0x%x, offset_src:%d, offset_dst:%d, width_align:%d", i, baseAddr, offset_src, offset_dst, width_align);
                        memcpy(baseAddr + offset_dst, baseAddr + offset_src, width_align);
                    }
                    SunxiMemFlushCache(pMemOps, baseAddr + ySize + uvData, width_align*height_diff/2);
                }
                else if (V4L2_PIX_FMT_YUV420 == vfmt.format.pixelformat || V4L2_PIX_FMT_YVU420 == vfmt.format.pixelformat)
                {
                    // fill U
                    int uData = AWALIGN(vfmt.format.width, VIN_ALIGN_WIDTH) * height / 4;
                    offset_src = ySize + uData - (width_align/4 * 1);
                    for (i = 0; i < height_diff; i++)
                    {
                        offset_dst = ySize + uData + (width_align/4 * i);
                        memcpy(baseAddr + offset_dst, baseAddr + offset_src, width_align/4);
                    }
                    SunxiMemFlushCache(pMemOps, baseAddr + ySize + uData, width_align/4*height_diff);
                    // fill V
                    int uvData = AWALIGN(vfmt.format.width, VIN_ALIGN_WIDTH) * height / 2;
                    offset_src = ySize + uvData - (width_align/4 * 1);
                    for (i = 0; i < height_diff; i++)
                    {
                        offset_dst = ySize + uvData + (width_align/4 * i);
                        memcpy(baseAddr + offset_dst, baseAddr + offset_src, width_align/4);
                    }
                    SunxiMemFlushCache(pMemOps, baseAddr + ySize + uvData, width_align/4*height_diff);
                }
                else
                {
                    aloge("fatal error! pixel format 0x%x is not support fill yuv data when offline and enable encpp sharp.",vfmt.format.pixelformat);
                }
            }
        }
    }
#endif

#if (AWCHIP == AW_V853)
    if (TRUE == pVippInfo->mstAttr.mbEncppEnable) //because encpp must process 16 align height to avoid self extending with zero data.
    {
        pstFrameInfo->VFrame.mWidth = vfmt.format.width;
        pstFrameInfo->VFrame.mHeight = AWALIGN(vfmt.format.height, VIN_ALIGN_HEIGHT);
    }
    else
    {
        pstFrameInfo->VFrame.mWidth = vfmt.format.width;
        pstFrameInfo->VFrame.mHeight = AWALIGN(vfmt.format.height, VIN_ALIGN_HEIGHT);
    }
#else
    pstFrameInfo->VFrame.mWidth = vfmt.format.width;
    pstFrameInfo->VFrame.mHeight = AWALIGN(vfmt.format.height, VIN_ALIGN_HEIGHT);
#endif
    pstFrameInfo->VFrame.mOffsetTop = 0;
    pstFrameInfo->VFrame.mOffsetBottom = pVippInfo->mstAttr.format.height;//pstFrameInfo->VFrame.mHeight
    pstFrameInfo->VFrame.mOffsetLeft = 0;
    pstFrameInfo->VFrame.mOffsetRight = pVippInfo->mstAttr.format.width;//pstFrameInfo->VFrame.mWidth
    pstFrameInfo->VFrame.mField = vfmt.format.field;
    pstFrameInfo->VFrame.mPixelFormat = map_V4L2_PIX_FMT_to_PIXEL_FORMAT_E(vfmt.format.pixelformat);// V4L2_PIX_FMT_SBGGR12;
    pstFrameInfo->VFrame.mpts = (int64_t)buffer.timestamp.tv_sec*1000*1000 + buffer.timestamp.tv_usec;
    pstFrameInfo->VFrame.mFramecnt = buffer.frame_cnt;
    pstFrameInfo->VFrame.mExposureTime = buffer.exp_time / 1000;
    if (0 == pVippInfo->mstAttr.mbDataFromYuvSensorEnable)
    {
#if MPPCFG_SUPPORT_FASTBOOT
        isp_get_attr_cfg_ctrl(isp_id, video, ISP_CTRL_AE_EV_LV, &ev_value);
        pstFrameInfo->VFrame.mEnvLV = ev_value;
        isp_get_attr_cfg_ctrl(isp_id, video, ISP_CTRL_AE_EV_LV_ADJ, &ev_value);
        pstFrameInfo->VFrame.mEnvLVAdj = ev_value;
#else
        pstFrameInfo->VFrame.mEnvLV = isp_get_lv(isp_id);
        pstFrameInfo->VFrame.mEnvLVAdj = isp_get_ev_lv_adj(isp_id);
#endif
    }
    pstFrameInfo->mId = buffer.index;
    alogv("vipp%d got id%d", nVipp, pstFrameInfo->mId);

    pthread_mutex_lock(&pVippInfo->mFrameListLock);
    if(list_empty(&pVippInfo->mIdleFrameList))
    {
        alogw("impossible, idle frame list is empty, malloc one");
        VippFrame *pNode = (VippFrame*)malloc(sizeof(VippFrame));
        if(pNode != NULL)
        {
            memset(pNode, 0, sizeof(VippFrame));
            list_add_tail(&pNode->mList, &pVippInfo->mIdleFrameList);
        }
        else
        {
            aloge("fatal error! malloc fail!");
        }
    }
    if(!list_empty(&pVippInfo->mIdleFrameList))
    {
        VippFrame *pNode = list_first_entry(&pVippInfo->mIdleFrameList, VippFrame, mList);
        pNode->mVipp = nVipp;
        pNode->mFrameBufId = pstFrameInfo->mId;
        list_move_tail(&pNode->mList, &pVippInfo->mReadyFrameList);
    }
    pthread_mutex_unlock(&pVippInfo->mFrameListLock);

    return SUCCESS;
}

/**
  release frame to vipp.

  @return
    ERR_VI_INVALID_CHNID:
    FAILURE: queue buffer fail.
*/
static ERRORTYPE videoInputHw_ReleaseData(int nVipp, VIDEO_FRAME_INFO_S *pstFrameInfo)
{
    if (nVipp >= HW_VIDEO_DEVICE_NUM)
    {
        aloge("fatal error! vipp[%d] number is invalid!", nVipp);
        return ERR_VI_INVALID_CHNID;
    }
    viChnManager *pVippInfo = gpVIDevManager->gpVippManager[nVipp];
    struct isp_video_device *video = gpVIDevManager->media->video_dev[nVipp];

    if (video_queue_buffer(video, pstFrameInfo->mId) < 0)
    {
        aloge("fatal error! vipp[%d] queue bufferId[%d] fail!", nVipp, pstFrameInfo->mId);
        return FAILURE;
    }
    alogv("vipp%d release id%d", nVipp, pstFrameInfo->mId);

    pthread_mutex_lock(&pVippInfo->mFrameListLock);
    int nMatchNum = 0;
    VippFrame *pEntry, *pTmp;
    list_for_each_entry_safe(pEntry, pTmp, &pVippInfo->mReadyFrameList, mList)
    {
        if(pEntry->mFrameBufId == pstFrameInfo->mId)
        {
            if(pEntry->mVipp != nVipp)
            {
                aloge("fatal error! vipp[%d]!=[%d], check code!", pEntry->mVipp, nVipp);
            }
            nMatchNum++;
            list_move_tail(&pEntry->mList, &pVippInfo->mIdleFrameList);
        }
    }
    if(nMatchNum != 1)
    {
        aloge("fatal error! matchNum[%d]!=1, vipp[%d]frameBufId[%d]", nMatchNum, nVipp, pstFrameInfo->mId);
    }
    pthread_mutex_unlock(&pVippInfo->mFrameListLock);

    return SUCCESS;
}

static ERRORTYPE videoInputHw_RefsIncrease(int vipp_id, VIDEO_FRAME_INFO_S *pstFrameInfo)
{ // it will be not called.
    pthread_mutex_lock(&gpVIDevManager->gpVippManager[vipp_id]->mRefsLock);
	gpVIDevManager->gpVippManager[vipp_id]->refs[pstFrameInfo->mId]++;
    pthread_mutex_unlock(&gpVIDevManager->gpVippManager[vipp_id]->mRefsLock);
	return SUCCESS;
}

ERRORTYPE videoInputHw_RefsReduceAndRleaseData(int vipp_id, VIDEO_FRAME_INFO_S *pstFrameInfo)
{
	int refs = 0, ret = -1;
    pthread_mutex_lock(&gpVIDevManager->gpVippManager[vipp_id]->mRefsLock);
    if(gpVIDevManager->gpVippManager[vipp_id]->refs[pstFrameInfo->mId] <= 0)
    {
        for(int i=0; i<100; i++)
        {
            aloge("fatal error! vipp[%d], frmid[%d]: ref=[%d] when reduce refs, check code!", vipp_id, pstFrameInfo->mId,
                gpVIDevManager->gpVippManager[vipp_id]->refs[pstFrameInfo->mId]);
        }
        abort();
    }
    gpVIDevManager->gpVippManager[vipp_id]->refs[pstFrameInfo->mId]--;
    refs = gpVIDevManager->gpVippManager[vipp_id]->refs[pstFrameInfo->mId];
	pthread_mutex_unlock(&gpVIDevManager->gpVippManager[vipp_id]->mRefsLock);

	if (0 == refs)
	{
		ret = videoInputHw_ReleaseData(vipp_id, pstFrameInfo);
	}
    return ret;
}

/**
  check if get frame from vipp timeout, and send message to upper module through callback.
  Timeout threshold is 10s.
*/
static ERRORTYPE CheckVippGetFrameTimeout(int nVippIndex)
{
    viChnManager *pVippInfo = gpVIDevManager->gpVippManager[nVippIndex];
    if(pVippInfo->mLastGetFrameTm >= 0)
    {
        int64_t nCurTm = CDX_GetSysTimeUsMonotonic()/1000; //unit:ms
        int64_t nItl = (nCurTm - pVippInfo->mLastGetFrameTm)/1000; //unit:s
        if((nItl >= 10) && (0 == nItl % 10))
        {
            pthread_mutex_lock(&pVippInfo->mFrameListLock);
            int num = 0;
            VippFrame *pEntry;
            list_for_each_entry(pEntry, &pVippInfo->mReadyFrameList, mList)
            {
                if(pEntry->mVipp != nVippIndex)
                {
                    aloge("fatal error! vipp[%d]!=[%d], check code!", pEntry->mVipp, nVippIndex);
                }
                num++;
                alogw("Be careful! vipp[%d] get frame fail! frameBufId[%d] is not release", pEntry->mVipp, pEntry->mFrameBufId);
            }
            if(num > 0)
            {
                alogw("Be careful! vipp[%d] get frame fail! [%d]frames are not release", nVippIndex, num);
            }
            else
            {
                aloge("fatal error! vipp[%d] get frame fail, but all frames are release!", nVippIndex);
            }
            int nVirChnNum = 0;
            VI_CHN_MAP_S *pViChnEntry = NULL;
            list_for_each_entry(pViChnEntry, &pVippInfo->mChnList, mList)
            {
                nVirChnNum++;
                alogw("Be careful! vipp[%d] get frame fail, viChn[0x%x] exist!", nVippIndex, pViChnEntry->mViChn);
            }
            alogw("Be careful! vipp[%d] get frame fail! [%d]viChns exist!", nVippIndex, nVirChnNum);
            pthread_mutex_unlock(&pVippInfo->mFrameListLock);
            if(pVippInfo->mMppCallback && 0==num)
            {
                aloge("fatal error! vipp[%d]: timeout[%lld]s, dequeue fail!", nVippIndex, nItl);
                MPP_CHN_S stChn = {MOD_ID_VIU, nVippIndex, MM_INVALID_CHN};
                pVippInfo->mMppCallback(
                    pVippInfo->pAppData,
                    &stChn,
                    MPP_EVENT_VI_TIMEOUT,
                    NULL);
            }
        }
    }
    return SUCCESS;
}

/**
  use select() to traverse all isp_video_dev which is enabled.
*/
void * VideoInputHw_CapThread(void *pThreadData)
{
    int ret = -1;
    ERRORTYPE eError = SUCCESS;
    int status;
    VIDEO_FRAME_INFO_S stFrameInfo;
    //VI_ATTR_S attr;
    //int num_buf= 0;
    int nMilliSec = 2000;   //unit:ms; default == 5000; icekirin fix me
    char strThreadName[32];
    sprintf(strThreadName, "VICapture");
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    message_t stCmdMsg;
    while (1)
    {
PROCESS_MESSAGE:
        if(get_message(&gpVIDevManager->mCmdQueue, &stCmdMsg) == 0)
        {
            alogv("VideoInputHw CapThread get_message cmd: %d", stCmdMsg.command);

            if (Stop == stCmdMsg.command)
            {
                // Kill thread
                goto EXIT;
            }
            else if (VVideoInputHw_EnableVipp == stCmdMsg.command)
            {
                ERRORTYPE cmdRet = SUCCESS;
                int nVipp = stCmdMsg.para0;
                struct isp_video_device *video = gpVIDevManager->media->video_dev[nVipp];
                viChnManager *pVipp = gpVIDevManager->gpVippManager[nVipp];
                // For get sensor resolution, it only takes effect after AW_MPI_ISP_Run.
                // So we suggest calling AW_MPI_ISP_Run before AW_MPI_VI_EnableVipp.
                // But to be compatible with the old flow, it is allowed to call AW_MPI_ISP_Run after AW_MPI_VI_EnableVipp.
                int iIspId = video_to_isp_id(video);
                struct sensor_config stConfig;
                memset(&stConfig, 0, sizeof(struct sensor_config));
                if (isp_get_sensor_info(iIspId, &stConfig) < 0)
                {
                    alogw("fatal error! Get isp[%d] sensor information failed! must be called AW_MPI_ISP_Run before AW_MPI_VI_EnableVipp.", iIspId);
                    //return FAILURE;
                }
                else
                {
                    alogd("isp[%d] sensor info size:%dx%d, user set size:%dx%d", iIspId, stConfig.width, stConfig.height,
                        pVipp->mstAttr.format.width, pVipp->mstAttr.format.height);
                    if (!pVipp->mstAttr.large_dma_merge_en)
                    {
                        if((pVipp->mstAttr.format.width > stConfig.width) || (pVipp->mstAttr.format.height > stConfig.height))
                        {
                            aloge("Be careful! Exception Case: user resolution[%dx%d] Exceeds the sensor resolution[%dx%d].",
                                pVipp->mstAttr.format.width, pVipp->mstAttr.format.height, stConfig.width, stConfig.height);
                            //cmdRet = ERR_VI_INVALID_PARA;
                            //goto _enableVippExit;
                        }
                    }
                    if(pVipp->mstAttr.mbEncppEnable && pVipp->mstAttr.mOnlineEnable && (stConfig.width % VIN_ALIGN_WIDTH != 0 || stConfig.height % VIN_ALIGN_HEIGHT != 0))
                    {
                        aloge("fatal error! Exception Case: Encpp enable & Online & sensor W/H[%dx%d] is not %dx%d align.",
                            stConfig.width, stConfig.height, VIN_ALIGN_WIDTH, VIN_ALIGN_HEIGHT);
                        cmdRet = ERR_VI_INVALID_PARA;
                        goto _enableVippExit;
                    }
                }

                struct buffers_pool *pool = buffers_pool_new(video);
                if(NULL == pool)
                {
                    aloge("fatal error! new buffers pool fail");
                    cmdRet = ERR_VI_SYS_NOTREADY;
                    goto _enableVippExit;
                }
                ret = video_req_buffers(video, pool);
                if(ret < 0)
                {
                    aloge("fatal error! video req buffers fail");
                    cmdRet = ERR_VI_NOMEM;
                    goto _enableVippExit;
                }
                struct video_fmt vfmt;
                memset(&vfmt, 0, sizeof(vfmt));
                video_get_fmt(video, &vfmt);
                alogd("EnableVipp[%d]: vfmt.bufs:%d,nplanes:%d, online:%d", nVipp, vfmt.nbufs, vfmt.nplanes, vfmt.ve_online_en);
                int i;
                for(i = 0; i < vfmt.nbufs; i++)
                {
                    ret = video_queue_buffer(video, i);
                    if(ret != 0)
                    {
                        aloge("fatal error! video queue buffer fail:%d", ret);
                    }
                }

                alogd("pVipp[%d]->mstAttr.large_dma_merge_en = %d", pVipp->vipp_dev_id, pVipp->mstAttr.large_dma_merge_en);
                if (pVipp->mstAttr.large_dma_merge_en == DMA_STITCH_VERTICAL)
                {
                    if (pVipp->mstAttr.dma_overlay_ctx.dma_overlay_en)
                    {
                        if (pVipp->mstAttr.dma_overlay_ctx.overlay_width != 0  && pVipp->mstAttr.dma_overlay_ctx.overlay_height != 0)
                        {
                            if (pVipp->mstAttr.dma_overlay_ctx.overlay_data)
                            {
                                video_set_dma_overlay(video, &pVipp->mstAttr.dma_overlay_ctx);
                            }
                            else
                            {
                                aloge("dma_overlay_en = %d, but overlay_data is NULL, it will disable dma_overlay_en\n");
                                pVipp->mstAttr.dma_overlay_ctx.dma_overlay_en = 0;
                            }
                        }
                        else
                        {
                            aloge("dma_overlay is %d*%d, it will disable dma_overlay_en, please check it!!!\n");
                            pVipp->mstAttr.dma_overlay_ctx.dma_overlay_en = 0;
                        }
                    }
                    else
                    {
                        alogd("large_dma_merge_en is DMA_STITCH_VERTICAL, but not to set dma_overlay\n");
                    }
                }

                ret = video_stream_on(video);
                if(ret < 0)
                {
                    aloge("fatal error! video stream on fail:%d", ret);
                    cmdRet = ERR_VI_SYS_NOTREADY;
                    goto _enableVippExit;
                }
                pVipp->iDropFrameNum = vfmt.drop_frame_num;
                pVipp->mLastGetFrameTm = -1;
                pVipp->last_v_frm_pts = -1;

                pthread_mutex_lock(&gpVIDevManager->mManagerLock);
                videoInputHw_setVippEnable(nVipp);
                pthread_mutex_unlock(&gpVIDevManager->mManagerLock);

            _enableVippExit:
                stCmdMsg.pReply->ReplyResult = (int)cmdRet;
                cdx_sem_up(&stCmdMsg.pReply->ReplySem);
            }
            else if(VVideoInputHw_DisableVipp == stCmdMsg.command)
            {
                ERRORTYPE cmdRet = SUCCESS;
                int nVipp = stCmdMsg.para0;
                struct isp_video_device *video = gpVIDevManager->media->video_dev[nVipp];
                viChnManager *pVipp = gpVIDevManager->gpVippManager[nVipp];

                pthread_mutex_lock(&gpVIDevManager->mManagerLock);
                pthread_mutex_lock(&pVipp->mLock);
                int cnt = 0;
                struct list_head *pList;
                list_for_each(pList, &pVipp->mChnList) { cnt++;}
                if(cnt > 0)
                {
                    aloge("fatal error! there is [%d] vir channel exist in vipp[%d], must destroy them first!", cnt, nVipp);
                    cmdRet = ERR_VI_NOT_PERM;
                    pthread_mutex_unlock(&pVipp->mLock);
                    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
                    goto _disableVippExit;
                }
                struct video_fmt vfmt;
                memset(&vfmt, 0, sizeof(vfmt));
                video_get_fmt(video, &vfmt);
                alogd("DisableVipp[%d]: vfmt.bufs:%d, online:%d", nVipp, vfmt.nbufs, vfmt.ve_online_en);
                int num_buf = vfmt.nbufs;
                for (int i = 0; i < num_buf; i++)
                {
                    if (0 != pVipp->refs[i])
                    {
                        aloge("fatal error! Virvi Com not return all yuv frame !!! frame id:%d, ref:%d", i, pVipp->refs[i]);
                        //videoInputHw_ReleaseData(&nVipp, &pVipp->VideoFrameInfo[i]);
                        //pVipp->refs[i] = 0;
                        //memset(&pVipp->VideoFrameInfo[i], 0, sizeof(VIDEO_FRAME_INFO_S));
                    }
                }
                pthread_mutex_unlock(&pVipp->mLock);
                videoInputHw_setVippDisable(nVipp);
                pthread_mutex_unlock(&gpVIDevManager->mManagerLock);

                ret = video_stream_off(video);
                if(ret < 0)
                {
                    aloge("fatal error! video stream off fail:%d", ret);
                }
                ret = video_free_buffers(video);
                if (ret < 0)
                {
                    aloge("fatal error! video free buffers fail:%d", ret);
                }
                buffers_pool_delete(video);

                if (pVipp->mLbcFillDataAddr)
                {
                    free(pVipp->mLbcFillDataAddr);
                    pVipp->mLbcFillDataAddr = NULL;
                    pVipp->mLbcFillDataLen = 0;
                }

            _disableVippExit:
                stCmdMsg.pReply->ReplyResult = (int)cmdRet;
                cdx_sem_up(&stCmdMsg.pReply->ReplySem);
            }
            else
            {
                aloge("fatal error! unknown cmd:0x%x", stCmdMsg.command);
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        //check all enabled vipp, find max fd.
        int nMaxFd = -1;
        fd_set fds;
        FD_ZERO(&fds);
        pthread_mutex_lock(&gpVIDevManager->mManagerLock);
        for(int i=0; i<VI_VIPP_NUM_MAX; i++)
        {
            if (NULL == gpVIDevManager->gpVippManager[i])
            {
                continue;
            }
            viChnManager *pVippInfo = gpVIDevManager->gpVippManager[i];
            pthread_mutex_lock(&pVippInfo->mLock);
            if(pVippInfo->mstAttr.mOnlineEnable && (pVippInfo->mstAttr.mOnlineShareBufNum != BK_TWO_BUFFER))
            { //online mode with one_buffer don't need to request and release frame.
                pthread_mutex_unlock(&pVippInfo->mLock);
                continue;
            }
            if(pVippInfo->vipp_enable)
            {
                struct isp_video_device *video = gpVIDevManager->media->video_dev[i];
                if(video->entity->fd < 0)
                {
                    aloge("fatal error! check vipp[%d] fd[%d] wrong", i, video->entity->fd);
                }
                FD_SET(video->entity->fd, &fds);
                if(video->entity->fd > nMaxFd)
                {
                    nMaxFd = video->entity->fd;
                }
            }
            pthread_mutex_unlock(&pVippInfo->mLock);
        }
        pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
        //poll data
        if(nMaxFd >= 0)
        {
            struct timeval tv;
            tv.tv_sec = nMilliSec / 1000;
            tv.tv_usec = (nMilliSec % 1000) * 1000;
            int nSetNum = select(nMaxFd + 1, &fds, NULL, NULL, &tv);
            if(nSetNum > 0)
            {
                int nValidFdNum = 0;
                int nVippIndex;
                //traverse all vipp fds
                pthread_mutex_lock(&gpVIDevManager->mManagerLock);
                for(nVippIndex=0; nVippIndex<VI_VIPP_NUM_MAX; nVippIndex++)
                {
                    viChnManager *pVippInfo = NULL;
                    struct isp_video_device *video = NULL;
                    if (NULL == gpVIDevManager->gpVippManager[nVippIndex])
                    {
                        continue;
                    }
                    pVippInfo = gpVIDevManager->gpVippManager[nVippIndex];
                    pthread_mutex_lock(&pVippInfo->mLock);
                    if(pVippInfo->mstAttr.mOnlineEnable && (pVippInfo->mstAttr.mOnlineShareBufNum != BK_TWO_BUFFER))
                    { //online mode with one_buffer don't need to request and release frame.
                        pthread_mutex_unlock(&pVippInfo->mLock);
                        continue;
                    }
                    if(pVippInfo->vipp_enable)
                    {
                        video = gpVIDevManager->media->video_dev[nVippIndex];
                        if(video->entity->fd < 0)
                        {
                            aloge("fatal error! check vipp[%d] fd[%d] wrong", nVippIndex, video->entity->fd);
                        }
                    }
                    else
                    {
                        pthread_mutex_unlock(&pVippInfo->mLock);
                        continue;
                    }
                    pthread_mutex_unlock(&pVippInfo->mLock);

                    if(FD_ISSET(video->entity->fd, &fds)) //if vipp has stream, only get one frame, next frame is got in next loop.
                    {
                        nValidFdNum++;
                        //request one frame from this vipp.
                        eError = videoInputHw_GetData(nVippIndex, &stFrameInfo);
                        if(SUCCESS == eError)
                        {
                            int64_t nGetFrameTm = CDX_GetSysTimeUsMonotonic()/1000;
                            if(pVippInfo->mLastGetFrameTm <= 0)
                            {
                                pVippInfo->mLastGetFrameTm = nGetFrameTm;
                            }
                            else
                            {
                                if(nGetFrameTm - pVippInfo->mLastGetFrameTm >= 500)
                                {
                                    alogw("Be careful! vipp[%d] get two frame interval[%lld]ms too large, some frames may discard", nVippIndex,
                                        nGetFrameTm - pVippInfo->mLastGetFrameTm);
                                }
                                pVippInfo->mLastGetFrameTm = nGetFrameTm;
                            }
                            if (pVippInfo->mstAttr.mOnlineEnable) //if online encode, release frame immediately.
                            {
                                if (pVippInfo->mstAttr.mOnlineShareBufNum != BK_TWO_BUFFER)
                                {
                                    aloge("fatal error! check code!");
                                }
                                videoInputHw_ReleaseData(nVippIndex, &stFrameInfo);
                            }
                            else //if offline encode, distribute frame to all virChns or drop it.
                            {
                                if (0 == pVippInfo->mstAttr.mbDataFromYuvSensorEnable)
                                {
                                #if MPPCFG_SUPPORT_FASTBOOT
                                    alogv("The FastBoot solution is not currently supported");
                                #else
                                    if (SendYuvToApp(&stFrameInfo, pVippInfo, nVippIndex) == 0)
                                    {
                                        alogv("Have sent yuv data to app");
                                    }
                                    /*else
                                    {
                                        aloge("Failed to grab YUV data");
                                    }*/
                                #endif
                                }
                                //check frame interval
                                if(-1 != pVippInfo->last_v_frm_pts)
                                {
                                    if(stFrameInfo.VFrame.mpts- pVippInfo->last_v_frm_pts >= 200000)
                                    {
                                        int bWarnFlag = 1;
                                        int IspDev = video_to_isp_id(video);
                                        int sensor_fps = 0;
                                        if(0==isp_get_fps(IspDev, &sensor_fps))
                                        {
                                            alogv("sensor_fps:%d, fps:%d", sensor_fps, pVippInfo->mstAttr.fps);
                                            if (sensor_fps != pVippInfo->mstAttr.fps) //fps auto change, so no need print warning.
                                            {
                                                bWarnFlag = 0;
                                            }
                                        }
                                        if(bWarnFlag)
                                        {
                                            alogw("vi_v_frm_pts_invalid:vipp%d--%lld-%lld=%lld(us)", nVippIndex, stFrameInfo.VFrame.mpts, pVippInfo->last_v_frm_pts,
                                                stFrameInfo.VFrame.mpts-pVippInfo->last_v_frm_pts);
                                        }
                                    }
                                }
                                pVippInfo->last_v_frm_pts = stFrameInfo.VFrame.mpts;
                                //drop the specified frame number
                                if (pVippInfo->iDropFrameNum > 0)
                                {
                                    alogv("vipp[%d] should drop %d frames, now still has %d frames should be dropped", nVippIndex, pVippInfo->mstAttr.drop_frame_num,
                                        pVippInfo->iDropFrameNum);
                                    pVippInfo->iDropFrameNum--;
                                    videoInputHw_ReleaseData(nVippIndex, &stFrameInfo);
                                    if(0 == pVippInfo->iDropFrameNum)
                                    {
                                        alogd("vipp[%d] drop %d frames done!", nVippIndex, pVippInfo->mstAttr.drop_frame_num);
                                    }
                                }
                                else //distribute frame to all virChns
                                {
                                    VI_CHN_MAP_S *pEntry;
                                    pthread_mutex_lock(&pVippInfo->mLock);// Fix me mID
                                    /*
                                    1. normal video have this mID , it from videoX buffer.
                                    2. stabilization video no this mID
                                    */
                                    if (0 == pVippInfo->refs[stFrameInfo.mId])
                                    {
                                        if (!list_empty(&pVippInfo->mChnList))
                                        {
                                            videoInputHw_RefsIncrease(nVippIndex, &stFrameInfo);
                                            list_for_each_entry(pEntry, &pVippInfo->mChnList, mList)
                                            {
                                                memcpy(&pVippInfo->VideoFrameInfo[stFrameInfo.mId], &stFrameInfo, sizeof(stFrameInfo));
                                                COMP_BUFFERHEADERTYPE bufferHeader;
                                                bufferHeader.nInputPortIndex = VI_CHN_PORT_INDEX_CAP_IN;
                                                bufferHeader.pOutputPortPrivate = &stFrameInfo;
                                                videoInputHw_RefsIncrease(nVippIndex, &stFrameInfo);
                                                ERRORTYPE ret2 = COMP_EmptyThisBuffer(pEntry->mViComp, &bufferHeader);
                                                if (ret2 != SUCCESS)
                                                {
                                                    videoInputHw_RefsReduceAndRleaseData(nVippIndex, &stFrameInfo);
                                                }
                                            }
                                            videoInputHw_RefsReduceAndRleaseData(nVippIndex, &stFrameInfo);
                                        }
                                        else
                                        {
                                            alogv("VIPP[%d] has no Virvi Component, drop this yuv data[%d-%d].", nVippIndex, stFrameInfo.mId, stFrameInfo.VFrame.mFramecnt);
                                            videoInputHw_ReleaseData(nVippIndex, &stFrameInfo);
                                        }
                                    }
                                    else
                                    {
                                        aloge("fatal error! vipp[%d] frameId[%d] refCnt[%d]!=0, y_addr=%p, maybe buf not return, drop this yuv data",
                                            nVippIndex, stFrameInfo.mId, pVippInfo->refs[stFrameInfo.mId], stFrameInfo.VFrame.mpVirAddr[0]);
                                        videoInputHw_ReleaseData(nVippIndex, &stFrameInfo);
                                    }
                                    pthread_mutex_unlock(&pVippInfo->mLock);
                                }
                            }
                        }
                        else
                        {
                            if(ERR_VI_BUF_EMPTY == eError)
                            {
                                aloge("fatal error! why dequeue buffer fail? check code!");
                            }
                            else
                            {
                                aloge("fatal error! videoInpuHw get data fail[0x%x], check code!", eError);
                            }
                        }
                    }
                    else
                    {
                        CheckVippGetFrameTimeout(nVippIndex);
                    }
                }
                if(nSetNum != nValidFdNum)
                {
                    aloge("fatal error! why select fd number is not match[%d!=%d]?", nSetNum, nValidFdNum);
                }
                pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
            }
            else
            {
                if(nSetNum < 0)
                {
                    aloge("fatal error! vipp fds select error(%s)! setNum:%d", strerror(errno), nSetNum);
                }
                else
                {
                    alogw("Be careful! vipp fds select timeout[%d]ms, setNum:%d!", nMilliSec, nSetNum);
                    pthread_mutex_lock(&gpVIDevManager->mManagerLock);
                    int nVippIndex;
                    for(nVippIndex=0; nVippIndex<VI_VIPP_NUM_MAX; nVippIndex++)
                    {
                        viChnManager *pVippInfo = NULL;
                        struct isp_video_device *video = NULL;
                        if (NULL == gpVIDevManager->gpVippManager[nVippIndex])
                        {
                            continue;
                        }
                        pVippInfo = gpVIDevManager->gpVippManager[nVippIndex];
                        pthread_mutex_lock(&pVippInfo->mLock);
                        if(pVippInfo->mstAttr.mOnlineEnable && (pVippInfo->mstAttr.mOnlineShareBufNum != BK_TWO_BUFFER))
                        { //online mode with one_buffer don't need to request and release frame.
                            pthread_mutex_unlock(&pVippInfo->mLock);
                            continue;
                        }
                        if(pVippInfo->vipp_enable)
                        {
                            video = gpVIDevManager->media->video_dev[nVippIndex];
                        }
                        else
                        {
                            pthread_mutex_unlock(&pVippInfo->mLock);
                            continue;
                        }
                        pthread_mutex_unlock(&pVippInfo->mLock);

                        CheckVippGetFrameTimeout(nVippIndex);
                    }
                    pthread_mutex_unlock(&gpVIDevManager->mManagerLock);
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&gpVIDevManager->mCmdQueue, 10*1000);
        }
    }
EXIT:
    return (void*)SUCCESS;
}

