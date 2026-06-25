/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : AOChannel_Component.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/12
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "AOChannel_Component"
#include <utils/plat_log.h>

#include <errno.h>
#include <sys/prctl.h>

#include <mm_component.h>
#include <tmessage.h>
#include <tsemaphore.h>
#include <cdx_list.h>
#include <SystemBase.h>
#include <media_common_aio.h>

#include <resample.h>
#include <change_speed.h>

#include "AOChannel_Component.h"
#include <ConfigOption.h>

#if (MPPCFG_AGC == OPTION_AGC_ENABLE)
#if (MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_WEBRTC)
#include <AOWebRtcAgc.h>
#elif (MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_AGCFLOAT)
#include <AOFloatAgc.h>
#endif
#endif

static void *AOChannel_ComponentThread(void *pThreadData);

static ERRORTYPE AOChannel_SendCommand(PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN COMP_COMMANDTYPE Cmd, PARAM_IN unsigned int nParam1, PARAM_IN void* pCmdData)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    CompInternalMsgType eCmd;
    ERRORTYPE eError = SUCCESS;
    message_t msg;

    //alogv("AOChannel_SendCommand: %d", Cmd);

    if (NULL == pChnData) {
        eError = ERR_AO_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_BAIL;
    }

    if (Cmd == COMP_CommandMarkBuffer) {
        if (NULL == pCmdData) {
            eError = ERR_AO_ILLEGAL_PARAM;
            goto COMP_CONF_CMD_BAIL;
        }
    }

    if (pChnData->state == COMP_StateInvalid) {
        eError = ERR_AO_SYS_NOTREADY;
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
                eError = ERR_AO_ILLEGAL_PARAM;
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

static ERRORTYPE AOChannel_GetState(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_OUT COMP_STATETYPE* pState)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if (NULL == pChnData || NULL == pState) {
        eError = ERR_AO_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_BAIL;
    }

    *pState = pChnData->state;

COMP_CONF_CMD_BAIL:
    return eError;
}

static ERRORTYPE AOChannel_SetCallbacks(PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN COMP_CALLBACKTYPE* pCallbacks, PARAM_IN void* pAppData)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if (NULL == pChnData || NULL == pCallbacks || NULL == pAppData) {
        aloge("pChnData=%p, pCallbacks=%p, pAppData=%p", pChnData, pCallbacks, pAppData);
        eError = ERR_AO_ILLEGAL_PARAM;
        goto COMP_CONF_CMD_BAIL;
    }

    pChnData->pCallbacks = pCallbacks;
    pChnData->pAppData = pAppData;

COMP_CONF_CMD_BAIL:
    return eError;
}

ERRORTYPE AOSetAVSync(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN char avSyncFlag)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;
    pChnData->av_sync = avSyncFlag;
    return eError;
}

ERRORTYPE AOSetTimeActiveRefClock(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_TIME_CONFIG_ACTIVEREFCLOCKTYPE *pRefClock)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;
    if(pRefClock->eClock == COMP_TIME_RefClockAudio)
    {
        pChnData->is_ref_clock = TRUE;
    }
    else
    {
        aloge("fatal error! check RefClock[0x%x]", pRefClock->eClock);
        pChnData->is_ref_clock = FALSE;
    }
    return eError;
}

ERRORTYPE AONotifyStartToRun(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;
    pChnData->start_to_play = TRUE;
    return eError;
}

ERRORTYPE AOSetStreamEof(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN BOOL drainFlag)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;

    alogd("AO end flag is set! drain pcm flag:%d!", drainFlag);
    pthread_mutex_lock(&pChnData->mInputFrameListMutex);
    pChnData->mWaitDrainPcmSemFlag = drainFlag;
    pChnData->priv_flag |= CDX_comp_PRIV_FLAGS_STREAMEOF;
    pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
    message_t   msg;
    msg.command = AOChannel_InputPcmAvailable;
    put_message(&pChnData->mCmdQueue, &msg);

//    if (pChnData->mWaitDrainPcmSemFlag == TRUE)
//    {
//        cdx_sem_down(&pChnData->mWaitDrainPcmSem);
//    }

    return eError;
}

ERRORTYPE AOClearStreamEof(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;
    pthread_mutex_lock(&pChnData->mInputFrameListMutex);
    pChnData->priv_flag &= ~CDX_comp_PRIV_FLAGS_STREAMEOF;
    pChnData->mbEof = FALSE;
    pChnData->bDrainPostDataFlag = false;
    pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
    return eError;
}

static ERRORTYPE AOReleaseFrame_l(
    PARAM_IN AO_CHN_DATA_S *pChnData,
    PARAM_IN AOCompInputFrame *pInFrame)
{
    ERRORTYPE omxRet = SUCCESS;
    AOCompInputFrame *pEntry;
    BOOL bFindFlag = FALSE;
    list_for_each_entry(pEntry, &pChnData->mAudioInputFrameUsedList, mList)
    {
        if(pEntry == pInFrame)
        {
            bFindFlag = TRUE;
            break;
        }
    }
    if(FALSE == bFindFlag)
    {
        list_for_each_entry(pEntry, &pChnData->mAudioInputFrameReadyList, mList)
        {
            if(pEntry == pInFrame)
            {
                bFindFlag = TRUE;
                break;
            }
        }
    }
    if(bFindFlag)
    {
        if (pChnData->mInputPortTunnelFlag[AO_INPORT_SUFFIX_AUDIO])
        {
            COMP_INTERNAL_TUNNELINFOTYPE *pADecTunnel = &pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_IN_PCM];
            COMP_BUFFERHEADERTYPE obh;
            obh.nOutputPortIndex = pADecTunnel->nTunnelPortIndex;
            obh.nInputPortIndex = pADecTunnel->nPortIndex;
            obh.pOutputPortPrivate = (void*)&pEntry->mAFrame;
            omxRet = COMP_FillThisBuffer(pADecTunnel->hTunnel, &obh);
            if(omxRet != SUCCESS)
            {
                aloge("fatal error! fill this buffer fail[0x%x], pts=[%lld], nId=[%d], check code!",
                    omxRet, pInFrame->mAFrame.mTimeStamp, pInFrame->mAFrame.mId);
            }
            else
            {
                list_move_tail(&pEntry->mList, &pChnData->mAudioInputFrameIdleList);
            }
        }
        else
        {
            COMP_BUFFERHEADERTYPE obh;
            obh.pOutputPortPrivate = (void*)&pEntry->mAFrame;
            //must return pcm to APP!
            pChnData->pCallbacks->EmptyBufferDone(pChnData->hSelf, pChnData->pAppData, &obh);
            list_move_tail(&pEntry->mList, &pChnData->mAudioInputFrameIdleList);
        }
    }
    else
    {
        aloge("fatal error! not find pInFrame[%p], pts[%lld], nId[%d] in used and ready list.",
            pInFrame, pInFrame->mAFrame.mTimeStamp, pInFrame->mAFrame.mId);
        omxRet = ERR_AO_ILLEGAL_PARAM;
    }
    return SUCCESS;
}

ERRORTYPE AOReleaseFrame(
        PARAM_IN AO_CHN_DATA_S *pChnData,
        PARAM_IN AOCompInputFrame *pFrame)
{
    ERRORTYPE eError;
    pthread_mutex_lock(&pChnData->mInputFrameListMutex);
    eError = AOReleaseFrame_l(pChnData, pFrame);
    pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
    return eError;
}

static ERRORTYPE AOAddFrame_l(
    PARAM_IN AO_CHN_DATA_S *pChnData,
    PARAM_IN AUDIO_FRAME_S *pInFrame)
{
    AOCompInputFrame *pNode;
    if(list_empty(&pChnData->mAudioInputFrameIdleList))
    {
        alogw("input frame list is empty, increase one");
        pNode = (AOCompInputFrame*)malloc(sizeof(AOCompInputFrame));
        if(NULL == pNode)
        {
            aloge("fatal error! malloc fail!");
        }
        list_add_tail(&pNode->mList, &pChnData->mAudioInputFrameIdleList);
        pChnData->mFrameNodeNum++;
    }
    pNode = list_first_entry(&pChnData->mAudioInputFrameIdleList, AOCompInputFrame, mList);
    memcpy(&pNode->mAFrame, pInFrame, sizeof(AUDIO_FRAME_S));
    list_move_tail(&pNode->mList, &pChnData->mAudioInputFrameReadyList);
    return SUCCESS;
}

ERRORTYPE AOSeek(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;
    pChnData->seek_flag = 1;

    AOCompInputFrame *pEntry, *pTmp;
    ERRORTYPE omxRet = SUCCESS;
    int cnt = 0;

    pthread_mutex_lock(&pChnData->mInputFrameListMutex);
    list_for_each_entry_safe(pEntry, pTmp, &pChnData->mAudioInputFrameUsedList, mList)
    {
        AOReleaseFrame_l(pChnData, pEntry);
        cnt++;
    }
    list_for_each_entry_safe(pEntry, pTmp, &pChnData->mAudioInputFrameReadyList, mList)
    {
        AOReleaseFrame_l(pChnData, pEntry);
        cnt++;
    }
    if (pChnData->mpAgcHandle)
    {
        pChnData->mpAgcHandle->mpAOAgcClearData(pChnData->mpAgcHandle);
    }
    alogd("AO seek, release [%d]input audio Frames! aoAgc clear data", cnt);
    pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
    return eError;
}

static ERRORTYPE AOChannel_SetSaveFileInfo(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN AUDIO_SAVE_FILE_INFO_S *pFileInfo)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if (COMP_StateIdle != pChnData->state && COMP_StateExecuting != pChnData->state)
    {
        aloge("call SetSaveFileInfo in wrong state[0x%x]!", pChnData->state);
        return ERR_AO_NOT_PERM;
    }

    int nPathLen = strlen(pFileInfo->mFilePath) + strlen(pFileInfo->mFileName) + 1;
    pChnData->mpSaveFileFullPath = (char*)malloc(nPathLen);
    if (NULL == pChnData->mpSaveFileFullPath)
    {
        aloge("malloc %d fail! FilePath:[%s], FileName:[%s]", nPathLen, pFileInfo->mFilePath, pFileInfo->mFileName);
        return ERR_AO_NOMEM;
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

static ERRORTYPE AOChannel_QueryFileStatus(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_OUT AUDIO_SAVE_FILE_INFO_S *pFileInfo)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S *)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if (COMP_StateIdle != pChnData->state && COMP_StateExecuting != pChnData->state)
    {
        aloge("call SetSaveFileInfo in wrong state[0x%x]!", pChnData->state);
        return ERR_AO_NOT_PERM;
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
        alogw("AO NOT in save file status!");
    }
    return SUCCESS;
}

#if (MPPCFG_SOFTDRC == OPTION_SOFTDRC_ENABLE)
static ERRORTYPE AODrcInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;
    drcLog_prms_t drc_prms;
    drc_prms.sampling_rate = pChnData->mDrcCfg.sampling_rate;
    drc_prms.attack_time = pChnData->mDrcCfg.attack_time;
    drc_prms.release_time = pChnData->mDrcCfg.release_time;
    drc_prms.max_gain = pChnData->mDrcCfg.max_gain;
    drc_prms.min_gain = pChnData->mDrcCfg.min_gain;
    drc_prms.noise_threshold = pChnData->mDrcCfg.noise_threshold;
    drc_prms.target_level = pChnData->mDrcCfg.target_level;
    pChnData->mDrc = drcLog_create(&drc_prms);
    return eError;
}
#endif
#if (MPPCFG_AGC == OPTION_AGC_ENABLE)
static ERRORTYPE AOAgcInit(PARAM_IN AO_CHN_DATA_S *pChnData, AUDIO_FRAME_S *pAudioFrame)
{
    //AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;
    if ((pChnData->mAgcCfg.fTargetDb < -30 || pChnData->mAgcCfg.fTargetDb > 0)
        || (pChnData->mAgcCfg.fMaxGainDb < 0 || pChnData->mAgcCfg.fMaxGainDb > 30))
    {
        aloge("fatal error! agc float init param invalid[%f-%f]", pChnData->mAgcCfg.fTargetDb, pChnData->mAgcCfg.fMaxGainDb);
        //pChnData->mpAgcHandle = NULL;
        //eError = ERR_AO_ILLEGAL_PARAM;
    }
    if(NULL == pChnData->mpAgcHandle)
    {
        int nSampleRate = map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pChnData->mAioAttr.enSamplerate);
        int nChnNum = pChnData->mAioAttr.mChnCnt;
        int nBitWidth = map_AUDIO_BIT_WIDTH_E_to_BitWidth(pChnData->mAioAttr.enBitwidth);
        int nFrameLen = pAudioFrame->mLen;
    #if (MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_WEBRTC)
        pChnData->mpAgcHandle = (AOAgc *)CreateAOWebRtcAgc(nSampleRate, nChnNum, nBitWidth, nFrameLen, &pChnData->mAgcCfg);
    #elif (MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_AGCFLOAT)
        pChnData->mpAgcHandle = (AOAgc *)CreateAOFloatAgc(nSampleRate, nChnNum, nBitWidth, nFrameLen, &pChnData->mAgcCfg);
    #else
        aloge("fatal error! no agc lib!");
        pChnData->mpAgcHandle = NULL;
    #endif
    }
    else
    {
        alogw("Be careful! aoChn[%d-%d] agc lib aready exist!", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
    }
    pChnData->bAgcCfgUpdateFlag = false;
    return eError;
}
static ERRORTYPE AOAgcDestroy(PARAM_IN AO_CHN_DATA_S *pChnData)
{
    //AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    ERRORTYPE eError = SUCCESS;
    if(pChnData->mpAgcHandle)
    {
        pChnData->mpAgcHandle->mpAOAgcDelete(pChnData->mpAgcHandle);
        pChnData->mpAgcHandle = NULL;
    }
    return eError;
}
/**
  process one frame, output len may not same to input len.

  @return
    0: success, valid frame is ready
    1: success, valid frame is not ready, process is successful.
    -1:fail, valid frame is not ready, process is failure.
*/
static int AOAgcProcess(AO_CHN_DATA_S *pChnData, AUDIO_FRAME_S *pAudioFrame, bool bDrainFlag)
{
    int result = 0;
    int ret;
    char *pInnerData = NULL;
    int nInnerDataLen = 0;
    if(pChnData->pPostBuf != pAudioFrame->mpAddr)
    {
        aloge("fatal error! aoChn[%d-%d] buf not match[%p!=%p], check code!", pChnData->mMppChnInfo.mDevId,
            pChnData->mMppChnInfo.mChnId, pChnData->pPostBuf, pAudioFrame->mpAddr);
    }
    //destroy and create agclib again.
    if (FALSE == pChnData->mUseAgcLib)
    {
        //get inner data of agclib, malloc memory to store.
        ret = pChnData->mpAgcHandle->mpAOAgcGetInputPcmData(pChnData->mpAgcHandle, &pInnerData, &nInnerDataLen);
        if(0 == ret)
        {
            if(nInnerDataLen > 0)
            {
                //copy innerData to audioFrame front.
                int nNewAudioFrameLen = nInnerDataLen + pAudioFrame->mLen;
                if (nNewAudioFrameLen > pAudioFrame->nBufSize)
                {
                    pAudioFrame->mpAddr = (short *)realloc((void *)pAudioFrame->mpAddr, nNewAudioFrameLen);
                    if (NULL == pAudioFrame->mpAddr)
                    {
                        aloge("fatal error! realloc fail!");
                    }
                    pAudioFrame->nBufSize = nNewAudioFrameLen;

                    pChnData->pPostBuf = pAudioFrame->mpAddr;
                    pChnData->nPostBufSize = pAudioFrame->nBufSize;
                }
                memmove((void *)((char *)pAudioFrame->mpAddr + nInnerDataLen), pAudioFrame->mpAddr, pAudioFrame->mLen);
                memcpy(pAudioFrame->mpAddr, (void *)pInnerData, nInnerDataLen);
                pAudioFrame->mLen += nInnerDataLen;
            }
            alogd("aoChn[%d-%d] disable agc, need get innerDataInfo[%p-%d]", pChnData->mMppChnInfo.mDevId,
                pChnData->mMppChnInfo.mChnId, pInnerData, nInnerDataLen);
        }
        //destroy
        pChnData->mpAgcHandle->mpAOAgcDelete(pChnData->mpAgcHandle);
        pChnData->mpAgcHandle = NULL;

        if (pChnData->bAgcCfgUpdateFlag)
        {
            alogd("update agc cfg before disable agc? need not update now");
            pChnData->bAgcCfgUpdateFlag = false;
        }
    }
    else
    {
        if (pChnData->bAgcCfgUpdateFlag)
        {
            alogd("Be careful! aoChn[%d-%d] update agcConfig!", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
            ret = pChnData->mpAgcHandle->mpAOAgcUpdateConfig(pChnData->mpAgcHandle, &pChnData->mAgcCfg);
            if (ret != 0)
            {
                aloge("fatal error! aoChn[%d-%d] aoAgc update config fail[%d]", pChnData->mMppChnInfo.mDevId,
                    pChnData->mMppChnInfo.mChnId, ret);
            }
            pChnData->bAgcCfgUpdateFlag = false;
        }
        ret = pChnData->mpAgcHandle->mpAOAgcProcess(pChnData->mpAgcHandle, pAudioFrame, bDrainFlag);
        if(ret != 0)
        {
            if(ret < 0)
            {
                aloge("fatal error! aoChn[%d-%d] agc process fail:%d", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, ret);
            }
        }
        pChnData->pPostBuf = pAudioFrame->mpAddr;
        pChnData->nPostBufSize = pAudioFrame->nBufSize;
    }

    if (pAudioFrame->mLen > 0)
    {
        result = 0;
    }
    else
    {
        result = 1;
    }
    return result;
}
#endif

static ERRORTYPE AOChannel_SetChnMute(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN BOOL bMute)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    pChnData->mbMute = bMute;
    return SUCCESS;
}

static ERRORTYPE AOChannel_GetChnMute(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT BOOL *pbMute)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    *pbMute = pChnData->mbMute;
    return SUCCESS;
}

static ERRORTYPE AOChannel_SetVps(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN float fVps)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    if(fVps < 0.5 || fVps > 4)
    {
        aloge("fatal error! fVps[%f] is wrong!", fVps);
        return ERR_AO_ILLEGAL_PARAM;
    }
    pChnData->mfVps = fVps;
    return SUCCESS;
}
static ERRORTYPE AOChannel_GetVps(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_OUT float *pfVps)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    *pfVps = pChnData->mfVps;
    return SUCCESS;
}

static ERRORTYPE AOChannel_SetStartThresholdTime(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN int nTimeMs)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    pChnData->nStartThresholdTimeMs = nTimeMs;
    return SUCCESS;
}

static ERRORTYPE AOChannel_SetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_IN void* pComponentConfigStructure)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            COMP_PARAM_PORTDEFINITIONTYPE *port = (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure;
            int i;
            for(i = 0; i < AO_CHN_MAX_PORTS; i++) {
                if (port->nPortIndex == pChnData->sPortDef[i].nPortIndex) {
                    memcpy(&pChnData->sPortDef[i], port, sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
                }
            }
            if (i == AO_CHN_MAX_PORTS) {
                eError = FAILURE;
            }
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier = (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure;
            int i;
            for(i=0; i<AO_CHN_MAX_PORTS; i++) {
                if(pChnData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
                    memcpy(&pChnData->sPortBufSupplier[i], pPortBufSupplier, sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
                    break;
                }
            }
            if(i == AO_CHN_MAX_PORTS) {
                eError = FAILURE;
            }
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            pChnData->mMppChnInfo = *(MPP_CHN_S*)pComponentConfigStructure;
            break;
        }
        case COMP_IndexVendorAOChnSendFrame:
        {
            alogd("Not use this any more! non-tunnel send frame api use EmptyThisBuffer().");
            break;
        }
        case COMP_IndexVendorAIOReSmpEnable:
        {
            pChnData->mbEnableReSmp = TRUE;
            break;
        }
        case COMP_IndexVendorAIOReSmpDisable:
        {
            pChnData->mbEnableReSmp = FALSE;
            break;
        }
        case COMP_IndexVendorAIOVqeAttr:
        {
            memcpy(&pChnData->mVqeCfg, (AO_VQE_CONFIG_S*)pComponentConfigStructure, sizeof(AO_VQE_CONFIG_S));
            break;
        }
        case COMP_IndexVendorAIOVqeEnable:
        {
            pChnData->mUseVqeLib = TRUE;
            break;
        }
        case COMP_IndexVendorAIOVqeDisable:
        {
            pChnData->mUseVqeLib = FALSE;
            break;
        }
        case COMP_IndexVendorAIODrcEnable:
        {
        #if (MPPCFG_SOFTDRC == OPTION_SOFTDRC_ENABLE)
            pChnData->mUseDrcLib = TRUE;
            memcpy(&pChnData->mDrcCfg, (AO_DRC_CONFIG_S*)pComponentConfigStructure, sizeof(AO_DRC_CONFIG_S));
            eError = AODrcInit(hComponent);
        #endif
            break;
        }
        case COMP_IndexVendorAIOAgcEnable:
        {
        #if (MPPCFG_AGC == OPTION_AGC_ENABLE)
            pthread_mutex_lock(&pChnData->mAgcLock);
            if(pChnData->mUseAgcLib)
            {
                alogd("aoChn[%d-%d] agc already enable, update agcConfig", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
            }
            memcpy(&pChnData->mAgcCfg, (AGC_FLOAT_CONFIG_S*)pComponentConfigStructure, sizeof(AGC_FLOAT_CONFIG_S));
            pChnData->bAgcCfgUpdateFlag = true;
            pChnData->mUseAgcLib = TRUE;
            pthread_mutex_unlock(&pChnData->mAgcLock);
        #else
            aloge("fatal error! agc disable");
            eError = ERR_AO_NOT_SUPPORT;
        #endif
            break;
        }
        case COMP_IndexVendorAIOAgcDisable:
        {
        #if (MPPCFG_AGC == OPTION_AGC_ENABLE)
            pthread_mutex_lock(&pChnData->mAgcLock);
            pChnData->mUseAgcLib = FALSE;
            pthread_mutex_unlock(&pChnData->mAgcLock);
        #else
            aloge("fatal error! agc disable");
            eError = ERR_AO_NOT_SUPPORT;
        #endif
            break;
        }
        case COMP_IndexVendorSetAVSync:
        {
            eError = AOSetAVSync(hComponent, *(char*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexConfigTimeActiveRefClock:
        {
            eError = AOSetTimeActiveRefClock(hComponent, (COMP_TIME_CONFIG_ACTIVEREFCLOCKTYPE*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorNotifyStartToRun:
        {
            eError = AONotifyStartToRun(hComponent);
            break;
        }
        case COMP_IndexVendorSetStreamEof:
        {
            eError = AOSetStreamEof(hComponent, *(BOOL *)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorClearStreamEof:
        {
            eError = AOClearStreamEof(hComponent);
            break;
        }
        case COMP_IndexVendorSeekToPosition:
        {
            eError = AOSeek(hComponent);
            break;
        }
        case COMP_IndexVendorAOSetSaveFileInfo:
        {
            AUDIO_SAVE_FILE_INFO_S *pSaveFileInfo = (AUDIO_SAVE_FILE_INFO_S*)pComponentConfigStructure;
            eError = AOChannel_SetSaveFileInfo(hComponent, pSaveFileInfo);
            break;
        }
        case COMP_IndexVendorAOChnPcmCardType:
        {
            pChnData->card_id = *(PCM_CARD_TYPE_E*)pComponentConfigStructure;
            break;
        }
        case COMP_IndexVendorAOChnMute:
        {
            eError = AOChannel_SetChnMute(hComponent, *(BOOL*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAOChnVps:
        {
            eError = AOChannel_SetVps(hComponent, *(float*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAOStartThresholdTime:
        {
            eError = AOChannel_SetStartThresholdTime(hComponent, *(int*)pComponentConfigStructure);
            break;
        }
        default:
        {
            eError = FAILURE;
            break;
        }
    }

    return eError;
}

static ERRORTYPE AOChannel_GetConfig(
        PARAM_IN COMP_HANDLETYPE hComponent,
        PARAM_IN COMP_INDEXTYPE nIndex,
        PARAM_INOUT void* pComponentConfigStructure)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    switch (nIndex)
    {
        case COMP_IndexParamPortDefinition:
        {
            COMP_PARAM_PORTDEFINITIONTYPE *port = (COMP_PARAM_PORTDEFINITIONTYPE*)pComponentConfigStructure;
            int i;
            for(i = 0; i < AO_CHN_MAX_PORTS; i++) {
                if (port->nPortIndex == pChnData->sPortDef[i].nPortIndex) {
                    memcpy(port, &pChnData->sPortDef[i], sizeof(COMP_PARAM_PORTDEFINITIONTYPE));
                }
            }
            if (i == AO_CHN_MAX_PORTS) {
                eError = FAILURE;
            }
            break;
        }
        case COMP_IndexParamCompBufferSupplier:
        {
            COMP_PARAM_BUFFERSUPPLIERTYPE *pPortBufSupplier = (COMP_PARAM_BUFFERSUPPLIERTYPE*)pComponentConfigStructure;
            int i;
            for(i=0; i<AO_CHN_MAX_PORTS; i++) {
                if(pChnData->sPortBufSupplier[i].nPortIndex == pPortBufSupplier->nPortIndex) {
                    memcpy(pPortBufSupplier, &pChnData->sPortBufSupplier[i], sizeof(COMP_PARAM_BUFFERSUPPLIERTYPE));
                    break;
                }
            }
            if(i == AO_CHN_MAX_PORTS) {
                eError = FAILURE;
            }
            break;
        }
        case COMP_IndexVendorMPPChannelInfo:
        {
            *(MPP_CHN_S*)pComponentConfigStructure = pChnData->mMppChnInfo;
            break;
        }
        case COMP_IndexVendorAIOVqeAttr:
        {
            *(AO_VQE_CONFIG_S*)pComponentConfigStructure = pChnData->mVqeCfg;
            break;
        }
        case COMP_IndexVendorAOQueryChnStat:
        {
            // todo
            break;
        }
        case COMP_IndexVendorAOQueryFileStatus:
        {
            AUDIO_SAVE_FILE_INFO_S *pSaveFileInfo = (AUDIO_SAVE_FILE_INFO_S*)pComponentConfigStructure;
            eError = AOChannel_QueryFileStatus(hComponent, pSaveFileInfo);
            break;
        }
        case COMP_IndexVendorAOChnMute:
        {
            eError = AOChannel_GetChnMute(hComponent, (BOOL*)pComponentConfigStructure);
            break;
        }
        case COMP_IndexVendorAOChnVps:
        {
            eError = AOChannel_GetVps(hComponent, (float*)pComponentConfigStructure);
            break;
        }
        default:
            eError = FAILURE;;
            break;
    }

    return eError;
}

static ERRORTYPE AOChannel_ComponentTunnelRequest(
    PARAM_IN COMP_HANDLETYPE hComponent,
    PARAM_IN unsigned int nPort,
    PARAM_IN COMP_HANDLETYPE hTunneledComp,
    PARAM_IN unsigned int nTunneledPort,
    PARAM_INOUT COMP_TUNNELSETUPTYPE* pTunnelSetup)
{
    ERRORTYPE eError = SUCCESS;
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
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
        eError = ERR_AO_INCORRECT_STATE_OPERATION;
        goto COMP_CMD_FAIL;
    }

    for (i = 0; i < AO_CHN_MAX_PORTS; ++i) {
        if (pChnData->sPortDef[i].nPortIndex == nPort) {
            pPortDef = &pChnData->sPortDef[i];
            break;
        }
    }
    if (i == AO_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AO_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    for (i = 0; i < AO_CHN_MAX_PORTS; ++i) {
        if (pChnData->sPortTunnelInfo[i].nPortIndex == nPort) {
            pPortTunnelInfo = &pChnData->sPortTunnelInfo[i];
            break;
        }
    }
    if (i == AO_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AO_ILLEGAL_PARAM;
        goto COMP_CMD_FAIL;
    }

    for (i = 0; i < AO_CHN_MAX_PORTS; ++i) {
        if (pChnData->sPortBufSupplier[i].nPortIndex == nPort) {
            pPortBufSupplier = &pChnData->sPortBufSupplier[i];
            break;
        }
    }
    if (i == AO_CHN_MAX_PORTS) {
        aloge("fatal error! portIndex[%d] wrong!", nPort);
        eError = ERR_AO_ILLEGAL_PARAM;
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
    if (pPortTunnelInfo->eTunnelType == TUNNEL_TYPE_CLOCK)
    {
        CDX_NotifyStartToRunTYPE NotifyStartToRunInfo;
        NotifyStartToRunInfo.nPortIndex = pPortTunnelInfo->nTunnelPortIndex;
        NotifyStartToRunInfo.mbNotify = TRUE;
        COMP_SetConfig(pPortTunnelInfo->hTunnel, COMP_IndexVendorNotifyStartToRunInfo, (void*)&NotifyStartToRunInfo);
    }
    if(pPortDef->eDir == COMP_DirOutput) {
        if (pChnData->mOutputPortTunnelFlag) {
            aloge("AO_Comp outport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        pTunnelSetup->nTunnelFlags = 0;
        pTunnelSetup->eSupplier = pPortBufSupplier->eBufferSupplier;
        // ao -> ai
        pChnData->mOutputPortTunnelFlag = TRUE;
    } else {
        if (pChnData->mInputPortTunnelFlag[AO_INPORT_SUFFIX_AUDIO] && pChnData->mInputPortTunnelFlag[AO_INPORT_SUFFIX_CLOCK]) {
            aloge("AO_Comp inport already bind, why bind again?!");
            eError = FAILURE;
            goto COMP_CMD_FAIL;
        }
        //Check the data compatibility between the ports using one or more GetParameter calls.
        //B checks if its input port is compatible with the output port of component A.
        COMP_PARAM_PORTDEFINITIONTYPE out_port_def;
        out_port_def.nPortIndex = nTunneledPort;
        ((MM_COMPONENTTYPE*)hTunneledComp)->GetConfig(hTunneledComp, COMP_IndexParamPortDefinition, &out_port_def);
        if(out_port_def.eDir != COMP_DirOutput) {
            aloge("fatal error! tunnel port index[%d] direction is not output!", nTunneledPort);
            eError = ERR_AO_ILLEGAL_PARAM;
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
        // adec->ao or ai->ao
        if (pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_PCM].nPortIndex == nPort)
            pChnData->mInputPortTunnelFlag[AO_INPORT_SUFFIX_AUDIO] = TRUE;
        else if (pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_CLK].nPortIndex == nPort)
            pChnData->mInputPortTunnelFlag[AO_INPORT_SUFFIX_CLOCK] = TRUE;
    }

COMP_CMD_FAIL:
    return eError;
}


/**
 * send frame to ao dev in tunnel/non-tunnel mode.
 *
 * @return SUCCESS
 * @param hComponent AOComp handle.
 * @param pBuffer frame buffer info.
 */
static ERRORTYPE AOChannel_EmptyThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;
    AUDIO_FRAME_S *pInFrame;

    pthread_mutex_lock(&pChnData->mStateLock);
    if(pChnData->state!=COMP_StateIdle && pChnData->state!=COMP_StateExecuting && pChnData->state!=COMP_StatePause)
    {
        aloge("Call function in invalid state[0x%x]!", pChnData->state);
        pthread_mutex_unlock(&pChnData->mStateLock);
        return ERR_AO_SYS_NOTREADY;
    }
    pInFrame = pBuffer->pOutputPortPrivate;

    if (pChnData->mInputPortTunnelFlag[AO_INPORT_SUFFIX_AUDIO])
    {
        if(pBuffer->nInputPortIndex == pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_PCM].nPortIndex)
        {
            pthread_mutex_lock(&pChnData->mInputFrameListMutex);
            AOAddFrame_l(pChnData, pInFrame);
            if(pChnData->mWaitInputFrameFlag || pChnData->mbEof)
            {
                pChnData->mWaitInputFrameFlag = FALSE;
                message_t msg;
                msg.command = AOChannel_InputPcmAvailable;
                put_message(&pChnData->mCmdQueue, &msg);
            }
            pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
        }
        else
        {
            aloge("Fatal error! inputPortIndex[%d] match nothing!", pBuffer->nInputPortIndex);
            eError = FAILURE;
        }
    }
    else
    {
        pthread_mutex_lock(&pChnData->mInputFrameListMutex);
        AOAddFrame_l(pChnData, pInFrame);
        if(pChnData->mWaitInputFrameFlag || pChnData->mbEof)
        {
            pChnData->mWaitInputFrameFlag = FALSE;
            message_t msg;
            msg.command = AOChannel_InputPcmAvailable;
            put_message(&pChnData->mCmdQueue, &msg);
        }
        pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
    }
    pthread_mutex_unlock(&pChnData->mStateLock);

    return eError;
}

static ERRORTYPE AOGetInputFrames(AO_CHN_DATA_S *pChnData)
{
    ERRORTYPE eError;
    if(!list_empty(&pChnData->mAudioInputFrameReadyList))
    {
        list_splice_tail_init(&pChnData->mAudioInputFrameReadyList, &pChnData->mAudioInputFrameUsedList);
        eError = SUCCESS;
    }
    else
    {
        eError = FAILURE;
    }
    return eError;
}

static ERRORTYPE AOChannel_FillThisBuffer(PARAM_IN COMP_HANDLETYPE hComponent, PARAM_IN COMP_BUFFERHEADERTYPE* pBuffer)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    ERRORTYPE eError = SUCCESS;

    if (pBuffer->nOutputPortIndex == pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_PCM].nPortIndex) {
#if 0
        AUDIO_FRAME_S *frame = pBuffer->pOutputPortPrivate;
        AUDIO_FRAME_S *tmp = pChnData->mpPcmMgr->getFreeFrame(pChnData->mpPcmMgr);
        if (tmp != NULL) {
            *frame = *tmp;
        } else {
            aloge("getFreeFrame error!");
            eError = FAILURE;
        }
#endif
    } else if (pBuffer->nOutputPortIndex == pChnData->sPortDef[AO_CHN_PORT_INDEX_OUT_AI].nPortIndex) {
#if 0
        AUDIO_FRAME_S *pFrm = (AUDIO_FRAME_S*)pBuffer->pOutputPortPrivate;
        pChnData->mpPcmMgr->releaseFrame(pChnData->mpPcmMgr, pFrm);
        if (pChnData->mWaitAllFrameReleaseFlag) {
            if (pChnData->mpPcmMgr->usingFrmEmpty(pChnData->mpPcmMgr)) {
                cdx_sem_signal(&pChnData->mAllFrameRelSem);
            }
        }
#endif
    } else {
        aloge("fatal error! invalid outPortIndex[%d]", pBuffer->nOutputPortIndex);
    }

    return eError;
}

static ERRORTYPE AOChannel_ComponentDeInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)(((MM_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    CompInternalMsgType eCmd = Stop;
    message_t msg;
    ERRORTYPE eError = SUCCESS;

    msg.command = eCmd;
    put_message(&pChnData->mCmdQueue, &msg);
    alogd("wait AO channel component exit!...");
    pthread_join(pChnData->mThreadId, (void*) &eError);

    cdx_sem_deinit(&pChnData->mAllFrameRelSem);
    cdx_sem_deinit(&pChnData->mWaitDrainPcmSem);
    message_destroy(&pChnData->mCmdQueue);

    pthread_mutex_lock(&pChnData->mInputFrameListMutex);
    if (!list_empty(&pChnData->mAudioInputFrameUsedList))
    {
        aloge("fatal error! inputUsedFrame must be 0!");
    }
    if (!list_empty(&pChnData->mAudioInputFrameReadyList))
    {
        aloge("fatal error! inputReadyFrame must be 0!");
    }
    if (!list_empty(&pChnData->mAudioInputFrameIdleList))
    {
        AOCompInputFrame *pEntry, *pTmp;
        int cnt = 0;
        list_for_each_entry_safe(pEntry, pTmp, &pChnData->mAudioInputFrameIdleList, mList)
        {
            list_del(&pEntry->mList);
            free(pEntry);
            cnt++;
        }
        if (cnt != pChnData->mFrameNodeNum)
        {
            aloge("fatal error! Why node_cnt[%d] in IdleList not match mFrameNodeNum[%d]?!", cnt, pChnData->mFrameNodeNum);
        }
    }
    pthread_mutex_unlock(&pChnData->mInputFrameListMutex);

    pthread_mutex_destroy(&pChnData->mStateLock);
    pthread_mutex_destroy(&pChnData->mInputFrameListMutex);
    pthread_mutex_destroy(&pChnData->mOutputBufMutex);

#ifdef CFG_AUDIO_EFFECT_RESAMPLE
    if (pChnData->res != NULL)
    {
        Destroy_Resampler(pChnData->res);
        pChnData->res = NULL;
    }
#endif
    if(pChnData->mpResampler)
    {
        speex_resampler_destroy(pChnData->mpResampler);
        pChnData->mpResampler = NULL;
    }
//    if(pChnData->mpReSmpOutBuf)
//    {
//        free(pChnData->mpReSmpOutBuf);
//        pChnData->mpReSmpOutBuf = NULL;
//    }
    if (pChnData->mpVpsSampleConverter)
    {
        sonicDestroyStream(pChnData->mpVpsSampleConverter);
        pChnData->mpVpsSampleConverter = NULL;
    }
//    if(pChnData->mpVpsOutBuf)
//    {
//        free(pChnData->mpVpsOutBuf);
//        pChnData->mpVpsOutBuf = NULL;
//        pChnData->mVpsOutBufSize = 0;
//        pChnData->mVpsOutValidLen = 0;
//    }
#if (MPPCFG_SOFTDRC == OPTION_SOFTDRC_ENABLE)
    if(pChnData->mUseDrcLib)
        drcLog_destroy(pChnData->mDrc);
#endif
#if (MPPCFG_AGC == OPTION_AGC_ENABLE)
    AOAgcDestroy(pChnData);
//    if(pChnData->mpAgcTmpBuff)
//    {
//        free(pChnData->mpAgcTmpBuff);
//        pChnData->mpAgcTmpBuff = NULL;
//    }
//    pChnData->mnAgcTmpBuffLen = 0;
#endif
    pthread_mutex_destroy(&pChnData->mAgcLock);

    if (pChnData->mSaveFileFlag)
    {
        fclose(pChnData->mFpSaveFile);
        free(pChnData->mpSaveFileFullPath);
    }
//    if(pChnData->pMuteBuf)
//    {
//        free(pChnData->pMuteBuf);
//        pChnData->pMuteBuf = NULL;
//    }
    if(pChnData->pPostBuf)
    {
        free(pChnData->pPostBuf);
        pChnData->pPostBuf = NULL;
    }
    free(pChnData);
    alogd("AO component exited!");

    return SUCCESS;
}

ERRORTYPE AOChannel_ComponentInit(PARAM_IN COMP_HANDLETYPE hComponent)
{
    MM_COMPONENTTYPE *pComp;
    AO_CHN_DATA_S *pChnData;
    ERRORTYPE eError = SUCCESS;
    int err;

    pComp = (MM_COMPONENTTYPE*)hComponent;

    // Create private data
    pChnData = (AO_CHN_DATA_S*)malloc(sizeof(AO_CHN_DATA_S));
    if (pChnData == NULL) {
        aloge("Alloc AO_CHN_DATA_S error!");
        return FAILURE;
    }
    memset(pChnData, 0x0, sizeof(AO_CHN_DATA_S));

    pComp->pComponentPrivate = (void*)pChnData;
    pChnData->state = COMP_StateLoaded;
    pChnData->hSelf = hComponent;

    // Fill in function pointers
    pComp->SetCallbacks             = AOChannel_SetCallbacks;
    pComp->SendCommand              = AOChannel_SendCommand;
    pComp->GetConfig                = AOChannel_GetConfig;
    pComp->SetConfig                = AOChannel_SetConfig;
    pComp->GetState                 = AOChannel_GetState;
    pComp->ComponentTunnelRequest   = AOChannel_ComponentTunnelRequest;
    pComp->ComponentDeInit          = AOChannel_ComponentDeInit;
    pComp->EmptyThisBuffer          = AOChannel_EmptyThisBuffer;
    pComp->FillThisBuffer           = AOChannel_FillThisBuffer;

    pChnData->sPortParam.nPorts = 0;
    pChnData->sPortParam.nStartPortNumber = 0x0;

    pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_CLK].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_CLK].bEnabled = TRUE;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_CLK].eDomain = COMP_PortDomainOther;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_CLK].eDir = COMP_DirInput;
    pChnData->sPortBufSupplier[AO_CHN_PORT_INDEX_IN_CLK].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortBufSupplier[AO_CHN_PORT_INDEX_IN_CLK].eBufferSupplier = COMP_BufferSupplyOutput;
    pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_IN_CLK].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_IN_CLK].eTunnelType = TUNNEL_TYPE_CLOCK;
    pChnData->sPortParam.nPorts++;

    pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_PCM].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_PCM].bEnabled = TRUE;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_PCM].eDomain = COMP_PortDomainAudio;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_IN_PCM].eDir = COMP_DirInput;
    pChnData->sPortBufSupplier[AO_CHN_PORT_INDEX_IN_PCM].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortBufSupplier[AO_CHN_PORT_INDEX_IN_PCM].eBufferSupplier = COMP_BufferSupplyOutput;
    pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_IN_PCM].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_IN_PCM].eTunnelType = TUNNEL_TYPE_COMMON;
    pChnData->sPortParam.nPorts++;

    pChnData->sPortDef[AO_CHN_PORT_INDEX_OUT_PLAY].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_OUT_PLAY].bEnabled = TRUE;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_OUT_PLAY].eDomain = COMP_PortDomainAudio;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_OUT_PLAY].eDir = COMP_DirOutput;
    pChnData->sPortBufSupplier[AO_CHN_PORT_INDEX_OUT_PLAY].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortBufSupplier[AO_CHN_PORT_INDEX_OUT_PLAY].eBufferSupplier = COMP_BufferSupplyOutput;
    pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_OUT_PLAY].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_OUT_PLAY].eTunnelType = TUNNEL_TYPE_COMMON;
    pChnData->sPortParam.nPorts++;

    pChnData->sPortDef[AO_CHN_PORT_INDEX_OUT_AI].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_OUT_AI].bEnabled = TRUE;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_OUT_AI].eDomain = COMP_PortDomainAudio;
    pChnData->sPortDef[AO_CHN_PORT_INDEX_OUT_AI].eDir = COMP_DirOutput;
    pChnData->sPortBufSupplier[AO_CHN_PORT_INDEX_OUT_AI].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortBufSupplier[AO_CHN_PORT_INDEX_OUT_AI].eBufferSupplier = COMP_BufferSupplyOutput;
    pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_OUT_AI].nPortIndex = pChnData->sPortParam.nPorts;
    pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_OUT_AI].eTunnelType = TUNNEL_TYPE_COMMON;
    pChnData->sPortParam.nPorts++;

//    if (audioHw_AO_GetPcmConfig(pChnData->mMppChnInfo.mDevId, &pChnData->mpPcmCfg) != SUCCESS) {
//        aloge("audioHw_AO_GetPcmConfig error!");
//        eError = FAILURE;
//        goto ERR_EXIT0;
//    }
//    if (audioHw_AO_GetAIOAttr(pChnData->mMppChnInfo.mDevId, &pChnData->mpAioAttr) != SUCCESS) {
//        aloge("audioHw_AO_GetAIOAttr error!");
//        eError = FAILURE;
//        goto ERR_EXIT0;
//    }

    INIT_LIST_HEAD(&pChnData->mAudioInputFrameIdleList);
    INIT_LIST_HEAD(&pChnData->mAudioInputFrameReadyList);
    INIT_LIST_HEAD(&pChnData->mAudioInputFrameUsedList);
    int i;
    for (i = 0; i < AO_CHN_MAX_CACHE_FRAME; i++)
    {
        AOCompInputFrame *pNode = (AOCompInputFrame*)malloc(sizeof(AOCompInputFrame));
        if (NULL == pNode)
        {
            aloge("fatal error! malloc fail[%s]!", strerror(errno));
            break;
        }
        memset(pNode, 0, sizeof(AOCompInputFrame));
        list_add_tail(&pNode->mList, &pChnData->mAudioInputFrameIdleList);
        pChnData->mFrameNodeNum++;
    }

    err = pthread_mutex_init(&pChnData->mInputFrameListMutex, NULL);
    if(err != 0)
    {
        aloge("Fatal error! pthread mutex init fail!");
        eError = FAILURE;
        goto ERR_EXIT0;
    }

    err = pthread_mutex_init(&pChnData->mOutputBufMutex, NULL);
    if(err != 0)
    {
        aloge("Fatal error! pthread mutex init fail!");
        eError = FAILURE;
        goto ERR_EXIT0;
    }

    err = pthread_mutex_init(&pChnData->mStateLock, NULL);
    if(err != 0)
    {
        aloge("Fatal error! pthread mutex init fail!");
        eError = FAILURE;
        goto ERR_EXIT0;
    }

    err = pthread_mutex_init(&pChnData->mAgcLock, NULL);
    if(err != 0)
    {
        aloge("fatal error! pthread mutex init fail!");
        eError = FAILURE;
        goto ERR_EXIT0;
    }

    if (message_create(&pChnData->mCmdQueue) < 0){
        aloge("message error!");
        eError = FAILURE;
        goto ERR_EXIT2;
    }

    err = cdx_sem_init(&pChnData->mAllFrameRelSem, 0);
    if (err != 0) {
        aloge("cdx_sem_init AllFrameRelSem error!");
        goto ERR_EXIT3;
    }

    err = cdx_sem_init(&pChnData->mWaitDrainPcmSem, 0);
    if (err != 0) {
        aloge("cdx_sem_init mWaitDrainPcmSem error!");
        goto ERR_EXIT4;
    }

    //pChnData->res = Creat_Resampler();
    //if(pChnData->res == NULL)
    //{
    //   aloge("create resample res error!");
    //   goto ERR_EXIT4;
    //}

    pChnData->mfVps = 1;
    err = pthread_create(&pChnData->mThreadId, NULL, AOChannel_ComponentThread, pChnData);
    if (err) {
        eError = FAILURE;
        goto ERR_EXIT6;
    }
    alogd("create AOChannel threadId[0x%lx]", pChnData->mThreadId);
    return eError;

ERR_EXIT6:
ERR_EXIT5:
    //Destroy_Resampler(pChnData->res);
    //pChnData->res = NULL;
    cdx_sem_deinit(&pChnData->mWaitDrainPcmSem);
ERR_EXIT4:
    cdx_sem_deinit(&pChnData->mAllFrameRelSem);
ERR_EXIT3:
    message_destroy(&pChnData->mCmdQueue);
ERR_EXIT2:
    pthread_mutex_destroy(&pChnData->mStateLock);
//ERR_EXIT1:
//    pcmBufMgrDestroy(pChnData->mpPcmMgr);
ERR_EXIT0:
    free(pChnData);
    return eError;
}

#ifdef CFG_AUDIO_EFFECT_GAIN
static int audioGainHandle(AO_CHN_DATA_S *pChnData, AUDIO_FRAME_S *pInFrm)
{
    AudioGain AGX;

    memset(&AGX, 0x00, sizeof(AudioGain));
    AGX.InputChan  = (pInFrm->mSoundmode==AUDIO_SOUND_MODE_MONO)?1:2;
    AGX.OutputChan = AGX.InputChan;
    AGX.InputLen   = pInFrm->mLen;
    AGX.OutputPtr  = AGX.InputPtr = (short*)pInFrm->mpAddr;
    AGX.preamp = pChnData->mVqeCfg.stGainCfg.s8GainValue;
    do_AudioGain(&AGX);

    return AGX.InputLen;
}
#endif

#ifdef CFG_AUDIO_EFFECT_EQ
static int audioEQHandle(AO_CHN_DATA_S *pChnData, AUDIO_FRAME_S *pInFrm)
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

    return pInFrm->mLen;
}
#endif

#ifdef CFG_AUDIO_EFFECT_RESAMPLE
static int audioResampleHandle(AO_CHN_DATA_S *pChnData, PARAM_INOUT AUDIO_FRAME_S *pInFrm)
{
    int outsamples, outlen;
    ResCfg cfg;

    if (pChnData->res == NULL)
    {
        pChnData->res = Creat_Resampler();
        if (pChnData->res == NULL)
        {
            aloge("create resample res error!");
            return 0;
        }
    }

    cfg.inch   = pInFrm->mSoundmode + 1;
    cfg.insrt  = pChnData->mpPcmCfg->sampleRate;
    cfg.inbuf   = pInFrm->mpAddr;
    cfg.samples = 1024;
    cfg.outsrt = pChnData->mVqeCfg.s32WorkSampleRate;   // dst sample rate

    pChnData->res->prepare(pChnData->res, &cfg);
    outsamples = pChnData->res->process(pChnData->res);
    outlen = outsamples*2*cfg.inch;                     // 2: format must be S16_LE
    pChnData->res->getData(pChnData->res, pChnData->tmpHandleBuf, outlen);

    // update pInFrm with resample process result.
    pInFrm->mpAddr = pChnData->tmpHandleBuf;
    pInFrm->mLen = outlen;

    return outlen;
}
#endif

static int audioEffectProc(AO_CHN_DATA_S *pChnData, PARAM_INOUT AUDIO_FRAME_S *pInFrm)
{
    int ret = 0;

    {
        //alogd("inframe_size=%d", inFrame->mLen);
#ifdef CFG_AUDIO_EFFECT_GAIN
        if (pChnData->mVqeCfg.bGainOpen)
        {
            ret = audioGainHandle(pChnData, pInFrm);
        }
#endif

#ifdef CFG_AUDIO_EFFECT_EQ
        if (pChnData->mVqeCfg.bEqOpen)
        {
            ret = audioEQHandle(pChnData, pInFrm);
        }
#endif

#ifdef CFG_AUDIO_EFFECT_RESAMPLE
        if (((pChnData->mVqeCfg.s32WorkSampleRate >= AUDIO_SAMPLE_RATE_8000)     \
            && (pChnData->mVqeCfg.s32WorkSampleRate <= AUDIO_SAMPLE_RATE_48000)) \
            && (pChnData->mVqeCfg.s32WorkSampleRate != pChnData->mpPcmCfg->sampleRate))
        {
            // pInFrm may be updated!
            ret = audioResampleHandle(pChnData, pInFrm);
        }
#endif
    }

    return ret;
}
/*
 * may use mpReSmpOutBuf to replace pPcmFrame->mpAddr.
 * pPcmFrame is temporary variable, so there is no problem to do this.
 */
static void AOResampleProcess(AO_CHN_DATA_S *pChnData, AUDIO_FRAME_S *pPcmFrame)
{
    int ret;
    if(pPcmFrame->mpAddr != (void*)pChnData->pPostBuf)
    {
        aloge("fatal error! frame addr not same[%p!=%p], check code!", pPcmFrame->mpAddr, pChnData->pPostBuf);
    }
    if(pPcmFrame->mSamplerate<=0 || pPcmFrame->mSamplerate>=AUDIO_SAMPLE_RATE_BUTT)
    {
        aloge("fatal error! pcmFrame sampleRate[%d] is wrong, don't resample!", pPcmFrame->mSamplerate);
        return;
    }
    if(pChnData->mAioAttr.mChnCnt != 1)
    {
        alogd("Sorry. only support one channel temporary.");
        return;
    }
    if(pChnData->mAioAttr.enBitwidth != pPcmFrame->mBitwidth)
    {
        aloge("fatal error! bitWidth is not same:%d!=%d", pChnData->mAioAttr.enBitwidth, pPcmFrame->mBitwidth);
    }
    if(pChnData->mAioAttr.enSamplerate == pPcmFrame->mSamplerate)
    {
        return;
    }
    int nAttrSampleRate = map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pChnData->mAioAttr.enSamplerate);
    int nCurSampleRate = map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pPcmFrame->mSamplerate);
    int nCurBitWidth = map_AUDIO_BIT_WIDTH_E_to_BitWidth(pPcmFrame->mBitwidth);
    int nLen = pPcmFrame->mLen*((nAttrSampleRate+nCurSampleRate-1)/nCurSampleRate);
    if(nLen > pChnData->nPostBufSize)
    {
        if(pChnData->pPostBuf)
        {
            free(pChnData->pPostBuf);
        }
        pChnData->pPostBuf = (char*)malloc(nLen);
        if(NULL == pChnData->pPostBuf)
        {
            aloge("fatal error! malloc fail");
        }
        pChnData->nPostBufSize = nLen;
    }

    if(pChnData->mReSmpInSampleRate != pPcmFrame->mSamplerate)
    {
        alogd("srcSampleRate change:%d->%d", pChnData->mReSmpInSampleRate, pPcmFrame->mSamplerate);
        if(pChnData->mpResampler)
        {
            alogd("need destroy Resampler!");
            speex_resampler_destroy(pChnData->mpResampler);
            pChnData->mpResampler = NULL;
        }
        pChnData->mReSmpInSampleRate = pPcmFrame->mSamplerate;
    }
    if(NULL == pChnData->mpResampler)
    {
        pChnData->mpResampler = speex_resampler_init(pChnData->mAioAttr.mChnCnt, nCurSampleRate, nAttrSampleRate, 1, NULL);
        if(NULL == pChnData->mpResampler)
        {
            aloge("fatal error! create Resampler fail");
            goto _err0;
        }
        ret = speex_resampler_set_rate(pChnData->mpResampler, nCurSampleRate, nAttrSampleRate);
        if(ret != 0)
        {
            aloge("fatal error! resampler set rate fail[%d]", ret);
            goto _err1;
        }
    }
    uint32_t nInSampleCount = pPcmFrame->mLen/(nCurBitWidth/8);
    uint32_t nInSampleNum = nInSampleCount;
    uint32_t nOutSampleCount = pChnData->nPostBufSize/(nCurBitWidth/8);
    uint32_t nOutSampleNum = nOutSampleCount;
    ret = speex_resampler_process_int(pChnData->mpResampler, 0, pPcmFrame->mpAddr, &nInSampleNum, (short*)pChnData->pPostBuf, &nOutSampleNum);
    if(ret != 0)
    {
        aloge("fatal error! resample process fail:%d", ret);
    }
    if(nInSampleCount - nInSampleNum != 0)
    {
        alogw("fatal error! resample samples: [%d-%d-%d-%d]", nInSampleCount, nInSampleNum, nOutSampleCount, nOutSampleNum);
    }
    int nReSmpOutValidLen = nOutSampleNum*(nCurBitWidth/8);

    pPcmFrame->mSamplerate = pChnData->mAioAttr.enSamplerate;
    pPcmFrame->mpAddr = pChnData->pPostBuf;
    pPcmFrame->mLen = nReSmpOutValidLen;
    return;

_err1:
    speex_resampler_destroy(pChnData->mpResampler);
    pChnData->mpResampler = NULL;
_err0:
    return;
}

/**
  increase or decrease sample number to implement vps.
*/
static ERRORTYPE AOVpsProcess(AO_CHN_DATA_S *pChnData, AUDIO_FRAME_S *pPcmFrame)
{
    ERRORTYPE ret = SUCCESS;
    int rc;
    if(pPcmFrame->mpAddr != (void*)pChnData->pPostBuf)
    {
        aloge("fatal error! frame addr not same[%p!=%p], check code!", pPcmFrame->mpAddr, pChnData->pPostBuf);
    }
    int nSampleRate = map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pChnData->mAioAttr.enSamplerate);
    int nChnCnt = pChnData->mAioAttr.mChnCnt;
    int nBitwidth = map_AUDIO_BIT_WIDTH_E_to_BitWidth(pChnData->mAioAttr.enBitwidth);
    float fVps = pChnData->mfVps;
    //if vps = 1, don't need process when vps lib has no input data.
    if(1 == fVps)
    {
        if(pChnData->mpVpsSampleConverter)
        {
            int numInputSamples = sonicGetNumInputSamples(pChnData->mpVpsSampleConverter);
            if(numInputSamples <= 0)
            {
                if(numInputSamples < 0)
                {
                    aloge("fatal error! check sonicStreamStruct input samples:[%d]!", numInputSamples);
                }
                return ret;
            }
            else
            {
                alogd("Be careful! catch it! vps change to 1, but vps lib has [%d] input samples to process, so continue!", numInputSamples);
            }
        }
        else
        {
            return ret;
        }
    }
    if(NULL == pChnData->mpVpsSampleConverter)
    {
        //alogd("sampleRate:%d, chnCnt:%d, Bitwidth:%d, fVps:%f", nSampleRate, nChnCnt, nBitwidth, fVps);
        pChnData->mpVpsSampleConverter = (void*)sonicCreateStream(nSampleRate, nChnCnt);
        if(NULL == pChnData->mpVpsSampleConverter)
        {
            aloge("fatal error! create sonic stream fail");
        }
    }
    sonicSetSpeed(pChnData->mpVpsSampleConverter, fVps);

    int nAlsaFrameLen = (nChnCnt*nBitwidth/8);
    if(pPcmFrame->mLen%nAlsaFrameLen != 0)
    {
        aloge("fatal error! check audio frame![%d-%d-%d]", pPcmFrame->mLen, nChnCnt, nBitwidth);
    }
    int numSamples = pPcmFrame->mLen/nAlsaFrameLen;
    //alogd("numSamples:%d, frameLen:%d, alsaFrameLen:%d", numSamples, pPcmFrame->mLen, nAlsaFrameLen);
    rc = sonicWriteShortToStream(pChnData->mpVpsSampleConverter, (short *)pPcmFrame->mpAddr, numSamples);
    if(1 == rc)
    {
        int numOutSamples = numSamples / fVps;
        int numMaxOutSamples = numOutSamples + 1024; //must consider previous left datas.
        int nOutBufSize = numMaxOutSamples*nAlsaFrameLen;
        if(pChnData->nPostBufSize < nOutBufSize)
        {
            if(pChnData->pPostBuf)
            {
                free(pChnData->pPostBuf);
                pChnData->pPostBuf = NULL;
            }
            pChnData->nPostBufSize = 0;
        }
        if(NULL == pChnData->pPostBuf)
        {
            pChnData->pPostBuf = (char*)malloc(nOutBufSize);
            if(pChnData->pPostBuf)
            {
                pChnData->nPostBufSize = nOutBufSize;
            }
            else
            {
                aloge("fatal error! malloc fail.");
            }
        }
        int nOutputSamples = sonicReadShortFromStream(pChnData->mpVpsSampleConverter, (short *)pChnData->pPostBuf, numMaxOutSamples);
        if(nOutputSamples != numOutSamples)
        {
            //alogd("outputSamples diff:[%d, %d-%d=%d]", numMaxOutSamples, nOutputSamples, numOutSamples, nOutputSamples - numOutSamples);
        }
        else
        {
            //alogd("outputSamples diff-none:[%d, %d-%d=%d]", numMaxOutSamples, nOutputSamples, numOutSamples, nOutputSamples - numOutSamples);
        }
        pPcmFrame->mpAddr = pChnData->pPostBuf;
        pPcmFrame->mLen = nOutputSamples*nAlsaFrameLen;
    }
    else
    {
        aloge("fatal error! ret:%d, check code!", rc);
        ret = FAILURE;
    }
    return ret;
}

#define AUDIO_RENDER_FIRST_FRAME_FLAG 1
#define AUDIO_RENDER_INIT_RENDER_FLAG 2
static void *AOChannel_ComponentThread(void *pThreadData)
{
    unsigned int cmddata;
    CompInternalMsgType cmd;
    message_t cmdmsg;
    AO_CHN_DATA_S *pChnData = (AO_CHN_DATA_S*)pThreadData;
    ERRORTYPE eError;
    MM_COMPONENTTYPE *pClkTunnelComp = NULL;
    MM_COMPONENTTYPE *pADecTunnelComp = NULL;
    COMP_INTERNAL_TUNNELINFOTYPE* pClkTunnel = NULL;
    COMP_INTERNAL_TUNNELINFOTYPE* pADecTunnel = NULL;

    alogv("AO channel ComponentThread start run...");
    sprintf(pChnData->mThreadName, "AOChn[%d-%d]", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
    prctl(PR_SET_NAME, (unsigned long)pChnData->mThreadName, 0, 0, 0);

    long long RelativeTime = 0;
    long long CurrentTime;

    pChnData->audio_rend_flag = AUDIO_RENDER_FIRST_FRAME_FLAG | AUDIO_RENDER_INIT_RENDER_FLAG;

    while (1) {
PROCESS_MESSAGE:
        if (get_message(&pChnData->mCmdQueue, &cmdmsg) == 0) {
            cmd = cmdmsg.command;
            cmddata = (unsigned int)cmdmsg.para0;
            if (cmd == SetState) {
                //alogv("cmd=SetState, cmddata=%d", cmddata);
                pthread_mutex_lock(&pChnData->mStateLock);
                if (pChnData->state == (COMP_STATETYPE) (cmddata)) {
                    CompSendEvent(pChnData, COMP_EventError, ERR_AO_SAMESTATE, 0);
                } else {
                    switch ((COMP_STATETYPE)cmddata) {
                        case COMP_StateInvalid:
                        {
                            pChnData->state = COMP_StateInvalid;
                            CompSendEvent(pChnData, COMP_EventError, ERR_AO_INVALIDSTATE, 0);
                            CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            break;
                        }
                        case COMP_StateLoaded:
                        {
                            if (pChnData->state != COMP_StateIdle)
                            {
                                aloge("fatal error! AO incorrect state transition [0x%x]->Loaded!", pChnData->state);
                                CompSendEvent(pChnData, COMP_EventError, ERR_AO_INCORRECT_STATE_TRANSITION, 0);
                            }
                            ERRORTYPE   omxRet;
                            //release all frames.
                            alogd("release all frames to ADec, when state[0x%x]->[0x%x]", pChnData->state, COMP_StateLoaded);
                            pthread_mutex_lock(&pChnData->mInputFrameListMutex);
                            if(!list_empty(&pChnData->mAudioInputFrameUsedList))
                            {
                                int cnt = 0;
                                struct list_head *pList;
                                list_for_each(pList, &pChnData->mAudioInputFrameUsedList)
                                {
                                    cnt++;
                                }
                                alogd("release [%d]used inputFrame!", cnt);
                                AOCompInputFrame *pEntry, *pTmp;
                                list_for_each_entry_safe(pEntry, pTmp, &pChnData->mAudioInputFrameUsedList, mList)
                                {
                                    AOReleaseFrame_l(pChnData, pEntry);
                                }
                            }
                            if(!list_empty(&pChnData->mAudioInputFrameReadyList))
                            {
                                int cnt = 0;
                                struct list_head *pList;
                                list_for_each(pList, &pChnData->mAudioInputFrameReadyList)
                                {
                                    cnt++;
                                }
                                alogd("release [%d]ready inputFrame!", cnt);
                                AOCompInputFrame *pEntry, *pTmp;
                                list_for_each_entry_safe(pEntry, pTmp, &pChnData->mAudioInputFrameReadyList, mList)
                                {
                                    AOReleaseFrame_l(pChnData, pEntry);
                                }
                            }
                            pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
                            pChnData->state = COMP_StateLoaded;
                            CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            break;
                        }
                        case COMP_StateIdle:
                        {
                            if (pChnData->state == COMP_StateInvalid) {
                                CompSendEvent(pChnData, COMP_EventError, ERR_AO_INCORRECT_STATE_OPERATION, 0);
                            } else {
                                pChnData->state = COMP_StateIdle;
                                pChnData->mbEof = FALSE;
                                CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            }
                            break;
                        }
                        case COMP_StateExecuting:
                        {
                            // Transition can only happen from pause or idle state
                            if (pChnData->state == COMP_StateIdle || pChnData->state == COMP_StatePause)
                            {
                                pClkTunnel = &pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_IN_CLK];
                                pClkTunnelComp = (MM_COMPONENTTYPE*)(pClkTunnel->hTunnel);
                                //pADecTunnel = &pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_IN_PCM];
                                //pADecTunnelComp = (MM_COMPONENTTYPE*)(pADecTunnel->hTunnel);
                                if (pChnData->state == COMP_StateIdle)
                                {
                                    pChnData->audio_rend_flag |= AUDIO_RENDER_FIRST_FRAME_FLAG;
                                    if (!pChnData->av_sync)
                                        pChnData->start_to_play = TRUE;
                                    else
                                        pChnData->start_to_play = FALSE;
                                    pChnData->wait_time_out = 0;
                                    if (pChnData->seek_flag)
                                    {
                                        alogd("seek in idle state?");
                                        pChnData->seek_flag = 0;
                                    }
                                }
                                else if (pChnData->state == COMP_StatePause)
                                {
                                    //if current Pause is caused by jump, we need set first frame flag. Because we need reset start_time to clock_component.
                                    if(pChnData->seek_flag)
                                    {
                                        pChnData->audio_rend_flag |= AUDIO_RENDER_FIRST_FRAME_FLAG;
                                        if (!pChnData->av_sync)
                                            pChnData->start_to_play = TRUE;
                                        else
                                            pChnData->start_to_play = FALSE;
                                        pChnData->wait_time_out = 0;
                                        pChnData->seek_flag = 0;
                                    }
                                }
                                if (pChnData->priv_flag & CDX_comp_PRIV_FLAGS_REINIT)
                                {
                                    alogd("pChnData->priv_flag = %#x, reinit AudioRender...", pChnData->priv_flag);
                                    pChnData->audio_rend_flag |= AUDIO_RENDER_INIT_RENDER_FLAG;
                                    pChnData->priv_flag &= ~CDX_comp_PRIV_FLAGS_REINIT;
                                }

                                if (pChnData->state == COMP_StatePause)
                                {
                                    //*resume the audio track.
                                    if (!(pChnData->audio_rend_flag & AUDIO_RENDER_INIT_RENDER_FLAG))
                                    {
                                    }
                                }

                                pChnData->state = COMP_StateExecuting;
                                CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            }
                            else
                            {
                                CompSendEvent(pChnData, COMP_EventError, ERR_AO_INCORRECT_STATE_OPERATION, 0);
                            }
                            break;
                        }
                        case COMP_StatePause:
                        {
                            if (pChnData->state == COMP_StateIdle || pChnData->state == COMP_StateExecuting)
                            {
                                pChnData->state = COMP_StatePause;
                                CompSendEvent(pChnData, COMP_EventCmdComplete, COMP_CommandStateSet, pChnData->state);
                            }
                            else
                            {
                                CompSendEvent(pChnData, COMP_EventError, ERR_AO_INCORRECT_STATE_OPERATION, 0);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
                pthread_mutex_unlock(&pChnData->mStateLock);
            } else if (cmd == AOChannel_InputPcmAvailable) {
                //pChnData->mWaitInputFrameFlag = FALSE;
                //alogv("AOChannel_InputPcmAvailable");
            } else if (cmd == AOChannel_OutBufAvailable) {
                alogv("AOChannel_OutBufAvailable. But NOT use!");
            } else if (cmd == StopPort) {
            } else if (cmd == Stop) {
                goto EXIT;
            }
            goto PROCESS_MESSAGE;
        }

        if (pChnData->state != COMP_StateExecuting)
        {
            alogv("AO channel Component state[0x%x] not OMX_StateExecuting", pChnData->state);
            TMessage_WaitQueueNotEmpty(&pChnData->mCmdQueue, 0);
            goto PROCESS_MESSAGE;
        }
        pthread_mutex_lock(&pChnData->mInputFrameListMutex);
        if (pChnData->mbEof)
        {
            alogd("AO EOF!");
            int cnt = 0;
            AOGetInputFrames(pChnData);
            if (!list_empty(&pChnData->mAudioInputFrameUsedList))
            {
                AOCompInputFrame *pEntry, *pTmp;
                list_for_each_entry_safe(pEntry, pTmp, &pChnData->mAudioInputFrameUsedList, mList)
                {
                    AOReleaseFrame_l(pChnData, pEntry);
                    cnt++;
                }
            }
            if(cnt > 0)
            {
                alogd("release [%d]audio frames when eof!", cnt);
            }
            pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
            TMessage_WaitQueueNotEmpty(&pChnData->mCmdQueue, 0);
            goto PROCESS_MESSAGE;
        }
        ERRORTYPE ret;
        COMP_TIME_CONFIG_TIMESTAMPTYPE time_stamp;

        ret = AOGetInputFrames(pChnData);
        if(ret == SUCCESS)
        {
            pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
        }
        else if (!list_empty(&pChnData->mAudioInputFrameUsedList))
        {
            pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
        }
        else
        {
            if (pChnData->priv_flag & CDX_comp_PRIV_FLAGS_STREAMEOF)
            {
                if (0 == (pChnData->audio_rend_flag & AUDIO_RENDER_FIRST_FRAME_FLAG))
                {
                    if (false == pChnData->bDrainPostDataFlag)
                    {
                        pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
                        goto processPcmData;
                    }
                }
                else
                {
                    alogw("Be careful! Low probability! aoChn[%d-%d] no audio frame come but eof!", pChnData->mMppChnInfo.mDevId,
                        pChnData->mMppChnInfo.mChnId);
                }
            }
            pChnData->mWaitInputFrameFlag = TRUE;
            if (pChnData->priv_flag & CDX_comp_PRIV_FLAGS_STREAMEOF)
            {
                alogd("A. AOChn[%d-%d] notify player eof!", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
                if (pChnData->mWaitDrainPcmSemFlag)
                {
                    alogd("AOChn[%d-%d] drain pcm ring buf!", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
                    int64_t tm1,tm2;
                    tm1 = CDX_GetSysTimeUsMonotonic();
                    if(audioHw_AO_IsDevStarted(pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId))
                    {
                        audioHw_AO_DrainPcmRingBuf(pChnData->mMppChnInfo.mDevId,pChnData->mMppChnInfo.mChnId);
                    }
                    else
                    {
                        aloge("fatal error! AOChn[%d-%d] is not start, so do not drain!", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
                    }
                    tm2 = CDX_GetSysTimeUsMonotonic();
                    pChnData->mWaitDrainPcmSemFlag = FALSE;
                    alogd("AOChn[%d-%d] drain pcm ring buf done, cost[%lld]ms!", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, (tm2-tm1)/1000);
                    //cdx_sem_up(&pChnData->mWaitDrainPcmSem);
                }
                //pChnData->state = COMP_StateIdle;
                CompSendEvent(pChnData, COMP_EventBufferFlag, COMP_CommandStateSet, 0);
                pChnData->mbEof = TRUE;
                pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
                goto PROCESS_MESSAGE;
            }
            else
            {
                pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
            }
            TMessage_WaitQueueNotEmpty(&pChnData->mCmdQueue, 0);
            goto PROCESS_MESSAGE;
        }

        processPcmData:
        if (pChnData->audio_rend_flag & AUDIO_RENDER_FIRST_FRAME_FLAG) {
            AOCompInputFrame *pNode = list_first_entry_or_null(&pChnData->mAudioInputFrameUsedList, AOCompInputFrame, mList);
            if (NULL == pNode)
            {
                aloge("fatal error! aoChn[%d-%d] audio input frame used list is empty?", pChnData->mMppChnInfo.mDevId,
                    pChnData->mMppChnInfo.mChnId);
            }
            AUDIO_FRAME_S *pFirstFrame = &pNode->mAFrame;
            alogd("AO get first pcm from ADec, nBufferLen: %d, nTimeStamp:%lldus, param[%d-%d-%d],use it to init AudioRenderHal",
                pFirstFrame->mLen, pFirstFrame->mTimeStamp, pFirstFrame->mSamplerate, pFirstFrame->mBitwidth, pFirstFrame->mSoundmode);
            if (pChnData->audio_rend_flag & AUDIO_RENDER_INIT_RENDER_FLAG) {
                int nSampleRate = map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pFirstFrame->mSamplerate);
                AIO_ATTR_S aio_attr;
                memset(&aio_attr, 0, sizeof(aio_attr));
                aio_attr.mPcmCardId = pChnData->card_id;
                aio_attr.enSamplerate = pFirstFrame->mSamplerate;
                aio_attr.enBitwidth = pFirstFrame->mBitwidth;
                aio_attr.enSoundmode = pFirstFrame->mSoundmode;
                aio_attr.mChnCnt = pFirstFrame->mSoundmode==AUDIO_SOUND_MODE_MONO ? 1:2;
                aio_attr.nAlsaFrameNumberForStartThreshold = (int)((int64_t)nSampleRate*pChnData->nStartThresholdTimeMs/1000);
                audioHw_AO_Dev_lock(pChnData->mMppChnInfo.mDevId);
                if(!audioHw_AO_IsDevConfigured(pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId))
                {
                    alogd("audioDev[%d-%d] is not config, config it now, [%d-%d-%d-%d]", pChnData->mMppChnInfo.mDevId,
                        pChnData->mMppChnInfo.mChnId, aio_attr.mPcmCardId, aio_attr.enSamplerate, aio_attr.enBitwidth, aio_attr.mChnCnt);
                    audioHw_AO_SetChnPubAttr(pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, &aio_attr);
                }
                audioHw_AO_GetChnPubAttr(pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, &pChnData->mAioAttr);
                if(!audioHw_AO_IsDevStarted(pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId))
                {
                    alogd("audioDev[%d-%d] is not start, start it now", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
                    audioHw_AO_EnableChn(pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId);
                }
                audioHw_AO_Dev_unlock(pChnData->mMppChnInfo.mDevId);
                alogd("audio render hal init and run ok:%d!",(pChnData->mMppChnInfo.mChnId));

                pChnData->audio_rend_flag &= ~AUDIO_RENDER_INIT_RENDER_FLAG;
            }

            if (NULL != pClkTunnelComp)
            {
                time_stamp.nPortIndex = pClkTunnel->nTunnelPortIndex;
                time_stamp.nTimestamp = pFirstFrame->mTimeStamp;
                //set audio start time.
                alogd("AO set config start_time to [%lld]us", time_stamp.nTimestamp);
                pClkTunnelComp->SetConfig(pClkTunnelComp, COMP_IndexConfigTimeClientStartTime, &time_stamp);
                RelativeTime = pFirstFrame->mTimeStamp;

                //wait 1.
                while(pChnData->start_to_play != TRUE)
                {
                    alogd("AO wait 50ms for flag change...");
                    usleep(50*1000);
                    if (pChnData->wait_time_out > 20)
                    {
                        alogw("AO wait too long, force clock component to start.");
                        pClkTunnelComp->SetConfig(pClkTunnelComp, COMP_IndexVendorConfigTimeClientForceStart, &time_stamp);
                        pChnData->wait_time_out = 0;
                    }
                    pChnData->wait_time_out++;

                    if (get_message_count(&pChnData->mCmdQueue) > 0)
                    {
                        alogd("AO detect message come when start_to_play is not ture!");
                        goto PROCESS_MESSAGE;
                    }
                }
            }
            pChnData->audio_rend_flag &= ~AUDIO_RENDER_FIRST_FRAME_FLAG;
        }

        if (NULL == pChnData->mpCurrentInputFrame)
        {
            pthread_mutex_lock(&pChnData->mInputFrameListMutex);
            if (!list_empty(&pChnData->mAudioInputFrameUsedList))
            {
                pChnData->mpCurrentInputFrame = list_first_entry(&pChnData->mAudioInputFrameUsedList, AOCompInputFrame, mList);
                pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
            }
            else
            {
                alogw("Be careful! aoChn[%d-%d] no node in UsedList?! privFlag[0x%x], drainPostDataFlag:%d",
                    pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, pChnData->priv_flag, pChnData->bDrainPostDataFlag);
                if (pChnData->bDrainPostDataFlag)
                {
                    aloge("fatal error! why drain post data done[%d], but still here?", pChnData->bDrainPostDataFlag);
                }
                pChnData->bDrainPostDataFlag = true;
                pthread_mutex_unlock(&pChnData->mInputFrameListMutex);
                //goto PROCESS_MESSAGE;
            }
        }
        AUDIO_FRAME_S *pPcmFrame = NULL;
        AUDIO_FRAME_S tmpPcmFrame;
        if (pChnData->mpCurrentInputFrame != NULL)
        {
            pPcmFrame = &pChnData->mpCurrentInputFrame->mAFrame;
            if (NULL != pClkTunnelComp)
            {
                pClkTunnelComp->GetConfig(pClkTunnelComp, COMP_IndexConfigTimeCurrentMediaTime, &time_stamp);
                CurrentTime = pPcmFrame->mTimeStamp;
                if (pChnData->is_ref_clock && CurrentTime - RelativeTime > AVS_ADJUST_PERIOD)
                {
                    //avs_adjust_period is based on media clock time coordinate.
                    //alogv("AO adjust av clock, AudioPts:Cur[%lld], Relative[%lld]", CurrentTime, RelativeTime);
                    time_stamp.nTimestamp = pPcmFrame->mTimeStamp;
                    pClkTunnelComp->SetConfig(pClkTunnelComp, COMP_IndexVendorAdjustClock, &time_stamp);
                    RelativeTime = CurrentTime;
                }
            }

            if(pChnData->nPostBufSize < pPcmFrame->mLen)
            {
                if(pChnData->pPostBuf)
                {
                    free(pChnData->pPostBuf);
                }
                pChnData->pPostBuf = (char*)malloc(pPcmFrame->mLen);
                if(NULL == pChnData->pPostBuf)
                {
                    aloge("fatal error! malloc fail, len:%d", pPcmFrame->mLen);
                }
                pChnData->nPostBufSize = pPcmFrame->mLen;
            }
            memcpy(pChnData->pPostBuf, pPcmFrame->mpAddr, pPcmFrame->mLen);
            tmpPcmFrame = *pPcmFrame;
        }
        else
        {
            memset(&tmpPcmFrame, 0, sizeof(tmpPcmFrame));
            tmpPcmFrame.mBitwidth = pChnData->mAioAttr.enBitwidth;
            tmpPcmFrame.mSoundmode = (1==pChnData->mAioAttr.mChnCnt)?AUDIO_SOUND_MODE_MONO:AUDIO_SOUND_MODE_STEREO;
            tmpPcmFrame.mSamplerate = pChnData->mAioAttr.enSamplerate;
            alogd("Be careful! we need create empty audio frame[%d-%d-%d] to drain postData!", tmpPcmFrame.mBitwidth,
                tmpPcmFrame.mSoundmode, tmpPcmFrame.mSamplerate);
        }
        tmpPcmFrame.mpAddr = (void*)pChnData->pPostBuf;
        tmpPcmFrame.nBufSize = pChnData->nPostBufSize;

#if (MPPCFG_SOFTDRC == OPTION_SOFTDRC_ENABLE)
        if (pChnData->mUseDrcLib)
        {
            int nBitwidth = map_AUDIO_BIT_WIDTH_E_to_BitWidth(pChnData->mAioAttr.enBitwidth);
            drcLog_process(pChnData->mDrc,tmpPcmFrame.mpAddr,tmpPcmFrame.mLen/(nBitwidth/8*pChnData->mAioAttr.mChnCnt),
                pChnData->mAioAttr.mChnCnt);
        }
#endif
#if (MPPCFG_AGC == OPTION_AGC_ENABLE)
        pthread_mutex_lock(&pChnData->mAgcLock);
        if (pChnData->mUseAgcLib)
        {
            if(NULL == pChnData->mpAgcHandle)
            {
                AOAgcInit(pChnData, &tmpPcmFrame);
            }
        }
        if(pChnData->mpAgcHandle) //process left data in agclib.
        {
            int agcRet = AOAgcProcess(pChnData, &tmpPcmFrame, pChnData->bDrainPostDataFlag);
            if(agcRet != 0)
            {
                if(agcRet < 0)
                {
                    aloge("fatal error! aoChn[%d-%d] agc process fail:%d", pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId,
                        agcRet);
                }
                pthread_mutex_unlock(&pChnData->mAgcLock);
                goto _releaseAOFrame;
            }
        }
        pthread_mutex_unlock(&pChnData->mAgcLock);
#endif
        if (pChnData->mUseVqeLib)
        {
            ret = audioEffectProc(pChnData, &tmpPcmFrame);
        }
        AOVpsProcess(pChnData, &tmpPcmFrame);
        if(pChnData->mbEnableReSmp)
        {
            AOResampleProcess(pChnData, &tmpPcmFrame);
        }
        if(pChnData->mbMute)
        {
            memset(tmpPcmFrame.mpAddr, 0, tmpPcmFrame.mLen);
        }

        if (tmpPcmFrame.mLen > 0)
        {
            MM_COMPONENTTYPE *pAiTunnelComp;
            COMP_INTERNAL_TUNNELINFOTYPE *pAiTunnel;
            COMP_BUFFERHEADERTYPE obh;

            pAiTunnel = &pChnData->sPortTunnelInfo[AO_CHN_PORT_INDEX_OUT_AI];
            pAiTunnelComp = (MM_COMPONENTTYPE*)(pAiTunnel->hTunnel);

            if (pAiTunnelComp != NULL)
            {
                AUDIO_FRAME_S frame = tmpPcmFrame;
                obh.nOutputPortIndex = pAiTunnel->nTunnelPortIndex;
                obh.pOutputPortPrivate = &frame;
                pAiTunnelComp->EmptyThisBuffer(pAiTunnelComp, &obh);
            }
        }

        if (pChnData->mSaveFileFlag)
        {
            fwrite(tmpPcmFrame.mpAddr, 1, tmpPcmFrame.mLen, pChnData->mFpSaveFile);
            pChnData->mSaveFileSize += tmpPcmFrame.mLen;
        }

        if (tmpPcmFrame.mLen > 0)
        {
            ret = audioHw_AO_FillPcmRingBuf(pChnData->mMppChnInfo.mDevId, pChnData->mMppChnInfo.mChnId, tmpPcmFrame.mpAddr,
                tmpPcmFrame.mLen);
            if (ret != SUCCESS)
            {
                aloge("fatal error! send to alsaLib fail[0x%x], but still release frame", ret);
            }
        }
    _releaseAOFrame:
        if (pChnData->mpCurrentInputFrame)
        {
            AOReleaseFrame(pChnData, pChnData->mpCurrentInputFrame);
            pChnData->mpCurrentInputFrame = NULL;
        }
        else
        {
            alogd("Be careful! maybe drain post datalen[%d], flag:%d!", tmpPcmFrame.mLen, pChnData->bDrainPostDataFlag);
        }
    }

EXIT:
    alogv("AO channel ComponentThread stop!");
    return NULL;
}
