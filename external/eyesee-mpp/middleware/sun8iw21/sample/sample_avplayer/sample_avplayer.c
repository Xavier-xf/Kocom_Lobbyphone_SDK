/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/11/4
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleDemux2Vdec2Vo"

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#include <hwdisplay.h>

#include <plat_log.h>
#include "mpi_sys.h"
#include <mpi_demux.h>
#include <mpi_vdec.h>
#include <mpi_vo.h>
#include <mpi_adec.h>
#include <mpi_ao.h>
#include <mpi_clock.h>
#include <confparser.h>
#include <ClockCompPortIndex.h>

#include "sample_avplayer_config.h"
#include "sample_avplayer.h"

static SampleAVPlayerContext *gpSampleAVPlayerContext = NULL;

static SampleAVPlayerContext *CreateSampleAVPlayerContext()
{
    int ret;
    SampleAVPlayerContext *pContext = (SampleAVPlayerContext *)malloc(sizeof(SampleAVPlayerContext));
    if (pContext == NULL)
    {
        aloge("fatal error! malloc fail");
        return NULL;
    }
    memset(pContext, 0, sizeof(SampleAVPlayerContext));

    ret = cdx_sem_init(&pContext->stSemExit, 0);
    if (ret != 0)
    {
        aloge("fatal error! cdx sem init fail:%d", ret);
    }
    pthread_mutex_init(&pContext->EofLock, NULL);

    pContext->ePcmCardType = PCM_CARD_TYPE_AUDIOCODEC;

    pContext->nDmxChn = MM_INVALID_CHN;
    pContext->nVdecChn = MM_INVALID_CHN;
    pContext->nAdecChn = MM_INVALID_CHN;
    pContext->nVoDev = MM_INVALID_DEV;
    pContext->nVoLayer = MM_INVALID_LAYER;
    pContext->nVoChn = MM_INVALID_CHN;
    pContext->nAODev = MM_INVALID_DEV;
    pContext->nAOChn = MM_INVALID_CHN;
    pContext->nClockChn = MM_INVALID_CHN;

    return pContext;
}

static void DeleteSampleAVPlayerContext(SampleAVPlayerContext *pContext)
{
    cdx_sem_deinit(&pContext->stSemExit);
    pthread_mutex_destroy(&pContext->EofLock);
    free(pContext);
}

static int parseCmdLine(SampleAVPlayerContext *pContext, int argc, char** argv)
{
    int ret = -1;

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = 0;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pContext->stCmdLinePara.strConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pContext->stCmdLinePara.strConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_demux2vdec2vo.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static int loadConfigPara(SampleAVPlayerContext *pContext, const char *pConfPath)
{
    int ret = 0;
    char *ptr;
    CONFPARSER_S stConf;

    memset(&pContext->stConfigPara, 0, sizeof(pContext->stConfigPara));
    strcpy(pContext->stConfigPara.strSrcFile, "/mnt/extsd/test.mp4");
    pContext->stConfigPara.eVdecPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;   //MM_PIXEL_FORMAT_YVU_PLANAR_420
    pContext->stConfigPara.nMaxVdecOutputWidth = 1920;
    pContext->stConfigPara.nMaxVdecOutputHeight= 1080;
    pContext->stConfigPara.eVdecRotation = ROTATE_NONE;
    pContext->stConfigPara.nVdecExtraFrameNum = -1;
    pContext->stConfigPara.nLayerId = 0;
    pContext->stConfigPara.nDisplayX = 0;
    pContext->stConfigPara.nDisplayY = 0;
    pContext->stConfigPara.nDisplayWidth = 640;
    pContext->stConfigPara.nDisplayHeight = 360;
    pContext->stConfigPara.nVoChnFrameRate = 0;
    pContext->stConfigPara.bForbidAudio = false;
    pContext->stConfigPara.nAudioVolume = 100;
    pContext->stConfigPara.nVideoStreamIndex = 0;
    pContext->stConfigPara.nAudioStreamIndex = 0;
    pContext->stConfigPara.nSeekTime = 0;
    pContext->stConfigPara.fVps = 1.0;
    pContext->stConfigPara.nLoopCnt = 0;
    pContext->stConfigPara.nVeFreq = 0;
    pContext->stConfigPara.nTestDuration = 0;

    if(pConfPath != NULL)
    {
        ret = createConfParser(pConfPath, &stConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return -1;
        }

        ptr = (char *)GetConfParaString(&stConf, SAMPLE_AVPLAYER_SRC_FILE, NULL);
        strcpy(pContext->stConfigPara.strSrcFile, ptr);
        ptr = (char *)GetConfParaString(&stConf, SAMPLE_AVPLAYER_VDEC_PIXEL_FORMAT, NULL);
        if (!strcmp(ptr, "yv12"))
        {
            pContext->stConfigPara.eVdecPixelFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
        }
        else if (!strcmp(ptr, "yu12"))
        {
            pContext->stConfigPara.eVdecPixelFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
        }
        else if (!strcmp(ptr, "nv12"))
        {
            pContext->stConfigPara.eVdecPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if (!strcmp(ptr, "nv21"))
        {
            pContext->stConfigPara.eVdecPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else
        {
            aloge("fatal error! unknown pixel format:%s", ptr);
            pContext->stConfigPara.eVdecPixelFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        pContext->stConfigPara.nMaxVdecOutputWidth = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_VDEC_OUTPUT_MAX_WIDTH, 0);
        pContext->stConfigPara.nMaxVdecOutputHeight = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_VDEC_OUTPUT_MAX_HEIGHT, 0);
        int rotate = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_ROTATION, 0);
        switch (rotate)
        {
        case 0:
        {
            pContext->stConfigPara.eVdecRotation = ROTATE_NONE;
            break;
        }
        case 90:
        {
            pContext->stConfigPara.eVdecRotation = ROTATE_90;
            break;
        }
        case 180:
        {
            pContext->stConfigPara.eVdecRotation = ROTATE_180;
            break;
        }
        case 270:
        {
            pContext->stConfigPara.eVdecRotation = ROTATE_270;
            break;
        }
        default:
        {
            aloge("fatal error! wrong rotate:%d", rotate);
            pContext->stConfigPara.eVdecRotation = ROTATE_NONE;
            break;
        }
        }
        pContext->stConfigPara.nVdecExtraFrameNum = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_VDEC_EXTRAFRAMENUM, -1);
        pContext->stConfigPara.nLayerId = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_LAYER_ID, 0);
        pContext->stConfigPara.nDisplayX = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_DISPLAY_X, 0);
        pContext->stConfigPara.nDisplayY = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_DISPLAY_Y, 0);
        pContext->stConfigPara.nDisplayWidth = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_DISPLAY_WIDTH, 0);
        pContext->stConfigPara.nDisplayHeight = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_DISPLAY_HEIGHT, 0);
        pContext->stConfigPara.nVoChnFrameRate = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_VOCHN_FRAMERATE, 0);
        pContext->stConfigPara.bForbidAudio = (bool)GetConfParaInt(&stConf, SAMPLE_AVPLAYER_FORBID_AUDIO, 0);
        pContext->stConfigPara.nAudioVolume = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_AUDIO_VOLUME, 0);
        pContext->stConfigPara.nVideoStreamIndex = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_VIDEO_STREAM_INDEX, 0);
        pContext->stConfigPara.nAudioStreamIndex = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_AUDIO_STREAM_INDEX, 0);
        pContext->stConfigPara.nSeekTime = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_SEEK_POSITION, 0);
        pContext->stConfigPara.fVps = (float)GetConfParaDouble(&stConf, SAMPLE_AVPLAYER_VPS, 1);
        pContext->stConfigPara.nLoopCnt = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_LOOP, 0);
        pContext->stConfigPara.nVeFreq = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_VE_FREQ, 0);
        pContext->stConfigPara.nTestDuration = GetConfParaInt(&stConf, SAMPLE_AVPLAYER_TEST_DURATION, 0);
        alogd("src_file:%s, seek_time:%dms, test_time=%ds", pContext->stConfigPara.strSrcFile, pContext->stConfigPara.nSeekTime,
            pContext->stConfigPara.nTestDuration);
        destroyConfParser(&stConf);
    }
    return ret;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleAVPlayerContext *pContext = (SampleAVPlayerContext *)cookie;

    if (pChn->mModId == MOD_ID_DEMUX)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("demux to end of file");
            if (pContext->bVideoFlag)
            {
                AW_MPI_VDEC_SetStreamEof(pContext->nVdecChn, TRUE);
            }
            if (pContext->bAudioFlag)
            {
                AW_MPI_ADEC_SetStreamEof(pContext->nAdecChn, TRUE);
            }
            break;
        default:
            alogw("Be careful! ignore event[%d] of demux", event);
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_VDEC)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("vdec get EOF flag");
            AW_MPI_VO_SetStreamEof(pContext->nVoLayer, pContext->nVoChn, TRUE);
            break;
        default:
            alogw("Be careful! ignore event[%d] of vdec", event);
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_VOU)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("vo get EOF flag");
            pthread_mutex_lock(&pContext->EofLock);
            if(!pContext->bVONotifyEof)
            {
                pContext->bVONotifyEof = true;
                if ((false == pContext->bAudioFlag) || pContext->bAONotifyEof)
                {
                    alogd("vo notify end");
                    cdx_sem_up(&pContext->stSemExit);
                }
            }
            else
            {
                aloge("fatal error! vo eof many times?");
            }
            pthread_mutex_unlock(&pContext->EofLock);
            break;
        case MPP_EVENT_SET_VIDEO_SIZE:
        {
            SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
            alogd("vo report src display size[%dx%d]", pDisplaySize->Width, pDisplaySize->Height);
            break;
        }
        case MPP_EVENT_RENDERING_START:
            alogd("vo start to rendering");
            break;
        default:
            alogw("Be careful! ignore event[%d] of vo", event);
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_ADEC)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("adec get EOF flag");
            AW_MPI_AO_SetStreamEof(pContext->nAODev, pContext->nAOChn, TRUE, TRUE);
            break;
        default:
            alogw("Be careful! ignore event[%d] of adec", event);
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_AO)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("ao get EOF flag");
            pthread_mutex_lock(&pContext->EofLock);
            if (!pContext->bAONotifyEof)
            {
                pContext->bAONotifyEof = true;
                if ((false == pContext->bVideoFlag) || pContext->bVONotifyEof)
                {
                    alogd("ao notify end");
                    cdx_sem_up(&pContext->stSemExit);
                }
            }
            else
            {
                aloge("fatal error! ao eof many times?");
            }
            pthread_mutex_unlock(&pContext->EofLock);
            break;
        default:
            alogw("Be careful! ignore event[%d] of ao", event);
            break;
        }
    }
    else
    {
        alogw("Be careful! ignore event[%d] of mpp module[%d-%d-%d]", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
    }

    return SUCCESS;
}

static int configDmxChnAttr(SampleAVPlayerContext *pContext)
{
    //pDemux2Vdec2VoData->mDmxChnAttr.mStreamType = STREAMTYPE_LOCALFILE;
    pContext->stDmxChnAttr.mSourceType = SOURCETYPE_FD;
    pContext->stDmxChnAttr.mSourceUrl = NULL;
    pContext->stDmxChnAttr.mFd = pContext->nSrcFd;
    pContext->stDmxChnAttr.mDemuxDisableTrack = DEMUX_DISABLE_SUBTITLE_TRACK;
    if (pContext->stConfigPara.bForbidAudio)
    {
        pContext->stDmxChnAttr.mDemuxDisableTrack |= DEMUX_DISABLE_AUDIO_TRACK;
    }
    return 0;
}

static int configVdecChnAttr(SampleAVPlayerContext *pContext, DEMUX_VIDEO_STREAM_INFO_S *pStreamInfo)
{
    memset(&pContext->stVdecChnAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    pContext->stVdecChnAttr.mPicWidth = pContext->stConfigPara.nMaxVdecOutputWidth;
    pContext->stVdecChnAttr.mPicHeight = pContext->stConfigPara.nMaxVdecOutputHeight;
    pContext->stVdecChnAttr.mInitRotation = pContext->stConfigPara.eVdecRotation;
    pContext->stVdecChnAttr.mOutputPixelFormat = pContext->stConfigPara.eVdecPixelFormat;
    pContext->stVdecChnAttr.mType = pStreamInfo->mCodecType;
    pContext->stVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 0; //1
    pContext->stVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
    if (pContext->stConfigPara.nVdecExtraFrameNum >= 0)
    {
        pContext->stVdecChnAttr.bEnableExtraFrameNum = TRUE;
        pContext->stVdecChnAttr.mExtraFrameNum = pContext->stConfigPara.nVdecExtraFrameNum;
        alogd("vdec extraFrameNum: %d", pContext->stVdecChnAttr.mExtraFrameNum);
    }

    return 0;
}

static int configAdecChnAttr(SampleAVPlayerContext *pContext, DEMUX_AUDIO_STREAM_INFO_S *pStreamInfo)
{
    memset(&pContext->stAdecChnAttr, 0, sizeof(ADEC_CHN_ATTR_S));
    pContext->stAdecChnAttr.mType = pStreamInfo->mCodecType;
    pContext->stAdecChnAttr.sampleRate = pStreamInfo->mSampleRate;
    pContext->stAdecChnAttr.channels = pStreamInfo->mChannelNum;
    pContext->stAdecChnAttr.bitsPerSample = pStreamInfo->mBitsPerSample;
    return SUCCESS;
}

static int prepare(SampleAVPlayerContext *pContext)
{
    ERRORTYPE ret;
    int rc;
    MPP_CHN_S stDmxChn = {MOD_ID_DEMUX, 0, pContext->nDmxChn};
    MPP_CHN_S stVdecChn = {MOD_ID_VDEC, 0, pContext->nVdecChn};
    MPP_CHN_S stVoChn = {MOD_ID_VOU, pContext->nVoLayer, pContext->nVoChn};
    MPP_CHN_S stAdecChn = {MOD_ID_ADEC, 0, pContext->nAdecChn};
    MPP_CHN_S stAOChn = {MOD_ID_AO, pContext->nAODev, pContext->nAOChn};
    MPP_CHN_S stClockChn = {MOD_ID_CLOCK, 0, pContext->nClockChn};

    memset(&pContext->stClockChnAttr, 0, sizeof(pContext->stClockChnAttr));
    configDmxChnAttr(pContext);
    ret = AW_MPI_DEMUX_CreateChn(pContext->nDmxChn, &pContext->stDmxChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! create dmxChn[%d] fail:0x%x", pContext->nDmxChn, ret);
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void *)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    ret = AW_MPI_DEMUX_RegisterCallback(pContext->nDmxChn, &cbInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! dmxChn[%d] fail:0x%x", pContext->nDmxChn, ret);
    }
    ret = AW_MPI_DEMUX_GetMediaInfo(pContext->nDmxChn, &pContext->stDemuxMediaInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
    }
    if ((pContext->stDemuxMediaInfo.mVideoNum > 0
            && pContext->stDemuxMediaInfo.mVideoIndex >= pContext->stDemuxMediaInfo.mVideoNum)
        || (pContext->stDemuxMediaInfo.mAudioNum > 0
            && pContext->stDemuxMediaInfo.mAudioIndex >= pContext->stDemuxMediaInfo.mAudioNum)
        || (pContext->stDemuxMediaInfo.mSubtitleNum > 0
            && pContext->stDemuxMediaInfo.mSubtitleIndex >= pContext->stDemuxMediaInfo.mSubtitleNum))
    {
        aloge("fatal error! trackIndex wrong! [%d-%d,%d-%d,%d-%d]", pContext->stDemuxMediaInfo.mVideoNum,
            pContext->stDemuxMediaInfo.mVideoIndex, pContext->stDemuxMediaInfo.mAudioNum, pContext->stDemuxMediaInfo.mAudioIndex,
            pContext->stDemuxMediaInfo.mSubtitleNum, pContext->stDemuxMediaInfo.mSubtitleIndex);
    }
    if ((pContext->stConfigPara.nVideoStreamIndex != pContext->stDemuxMediaInfo.mVideoIndex)
        && (pContext->stConfigPara.nVideoStreamIndex < pContext->stDemuxMediaInfo.mVideoNum))
    {
        ret = AW_MPI_DEMUX_SelectVideoStream(pContext->nDmxChn, pContext->stConfigPara.nVideoStreamIndex);
        if(ret != SUCCESS)
        {
            aloge("fatal error! select video stream[%d] fail[0x%x].", pContext->stConfigPara.nVideoStreamIndex, ret);
        }
    }
    if ((pContext->stConfigPara.nAudioStreamIndex != pContext->stDemuxMediaInfo.mAudioIndex)
        && (pContext->stConfigPara.nAudioStreamIndex < pContext->stDemuxMediaInfo.mAudioNum))
    {
        ret = AW_MPI_DEMUX_SelectAudioStream(pContext->nDmxChn, pContext->stConfigPara.nAudioStreamIndex);
        if(ret != SUCCESS)
        {
            aloge("fatal error! select audio stream[%d] fail[0x%x].", pContext->stConfigPara.nAudioStreamIndex, ret);
        }
    }
    ret = AW_MPI_DEMUX_GetMediaInfo(pContext->nDmxChn, &pContext->stDemuxMediaInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! get media info fail!");
    }

    if (pContext->stDemuxMediaInfo.mVideoNum > 0)
    {
        configVdecChnAttr(pContext, &pContext->stDemuxMediaInfo.mVideoStreamInfo[pContext->stDemuxMediaInfo.mVideoIndex]);
        ret = AW_MPI_VDEC_CreateChn(pContext->nVdecChn, &pContext->stVdecChnAttr);
        if (ret != SUCCESS)
        {
            aloge("fatal error! create vdecChn[%d] fail[0x%x]!", pContext->nVdecChn, ret);
        }
        if (pContext->stConfigPara.nVeFreq)
        {
            alogd("vdec set ve freq %d MHz", pContext->stConfigPara.nVeFreq);
            AW_MPI_VDEC_SetVEFreq(pContext->nVdecChn, pContext->stConfigPara.nVeFreq);
        }
        AW_MPI_VDEC_RegisterCallback(pContext->nVdecChn, &cbInfo);
        AW_MPI_VDEC_ForceFramePackage(pContext->nVdecChn, pContext->bForceFramePackage);
        alogd("bind demux & vdec");
        ret = AW_MPI_SYS_Bind(&stDmxChn, &stVdecChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind dmxChn and vdecChn fail:0x%x", ret);
        }

        AW_MPI_VO_Enable(pContext->nVoDev);
        AW_MPI_VO_GetPubAttr(pContext->nVoDev, &pContext->stVoPubAttr);
        pContext->stVoPubAttr.enIntfType = VO_INTF_LCD;
        pContext->stVoPubAttr.enIntfSync = VO_OUTPUT_NTSC;
        AW_MPI_VO_SetPubAttr(pContext->nVoDev, &pContext->stVoPubAttr);
       //enable vo layer
        if (pContext->nVoLayer >= 0)
        {
            ret = AW_MPI_VO_EnableVideoLayer(pContext->nVoLayer);
            if (ret != SUCCESS)
            {
                aloge("fatal error! enable video layer[%d] fail!", pContext->nVoLayer);
            }
            AW_MPI_VO_GetVideoLayerAttr(pContext->nVoLayer, &pContext->stVoLayerAttr);
            pContext->stVoLayerAttr.stDispRect.X = pContext->stConfigPara.nDisplayX;
            pContext->stVoLayerAttr.stDispRect.Y = pContext->stConfigPara.nDisplayY;
            pContext->stVoLayerAttr.stDispRect.Width = pContext->stConfigPara.nDisplayWidth;
            pContext->stVoLayerAttr.stDispRect.Height = pContext->stConfigPara.nDisplayHeight;
            AW_MPI_VO_SetVideoLayerAttr(pContext->nVoLayer, &pContext->stVoLayerAttr);
        }
        ret = AW_MPI_VO_CreateChn(pContext->nVoLayer, pContext->nVoChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! create voChn[%d] fail:0x%x!", pContext->nVoChn, ret);
        }
        AW_MPI_VO_RegisterCallback(pContext->nVoLayer, pContext->nVoChn, &cbInfo);
        AW_MPI_VO_SetChnDispBufNum(pContext->nVoLayer, pContext->nVoChn, 2);
        AW_MPI_VO_SetChnFrameRate(pContext->nVoLayer, pContext->nVoChn, pContext->stConfigPara.nVoChnFrameRate);
        alogd("bind vdec & vo");
        ret = AW_MPI_SYS_Bind(&stVdecChn, &stVoChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind vdecChn and voChn fail:0x%x", ret);
        }

        pContext->stClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_VIDEO; //be careful this is too important!!!
        pContext->bVideoFlag = true;
    }
    if ((pContext->stDemuxMediaInfo.mAudioNum > 0) && !(pContext->stDmxChnAttr.mDemuxDisableTrack&DEMUX_DISABLE_AUDIO_TRACK))
    {
        configAdecChnAttr(pContext, &pContext->stDemuxMediaInfo.mAudioStreamInfo[pContext->stDemuxMediaInfo.mAudioIndex]);
        ret = AW_MPI_ADEC_CreateChn(pContext->nAdecChn, &pContext->stAdecChnAttr);
        if (ret != SUCCESS)
        {
            aloge("fatal error! create adecChn[%d] fail:0x%x!");
        }
        AW_MPI_ADEC_RegisterCallback(pContext->nAdecChn, &cbInfo);
        alogd("bind demux & adec");
        ret = AW_MPI_SYS_Bind(&stDmxChn, &stAdecChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind dmxChn and adecChn fail:0x%x", ret);
        }

        ret = AW_MPI_AO_CreateChn(pContext->nAODev, pContext->nAOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! create aoChn[%d] fail:0x%x", pContext->nAOChn, ret);
        }
        AW_MPI_AO_RegisterCallback(pContext->nAODev, pContext->nAOChn, &cbInfo);
        AW_MPI_AO_SetPcmCardType(pContext->nAODev, pContext->nAOChn, pContext->ePcmCardType);
        AW_MPI_AO_SetChnVps(pContext->nAODev, pContext->nAOChn, pContext->stConfigPara.fVps);
        AW_MPI_AO_SetDevVolume(pContext->nAODev, pContext->stConfigPara.nAudioVolume);
        alogd("bind adec & ao");
        ret = AW_MPI_SYS_Bind(&stAdecChn, &stAOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind adecChn and aoChn fail:0x%x", ret);
        }
        
        pContext->stClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_AUDIO;
        pContext->bAudioFlag = true;
    }

    ret = AW_MPI_CLOCK_CreateChn(pContext->nClockChn, &pContext->stClockChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! create clkChn[%d] fail:0x%x!", pContext->nClockChn, ret);
    }
    AW_MPI_CLOCK_RegisterCallback(pContext->nClockChn, &cbInfo);
    AW_MPI_CLOCK_SetVps(pContext->nClockChn, pContext->stConfigPara.fVps);
    alogd("bind clock & demux & vo & ao");
    ret = AW_MPI_SYS_Bind(&stClockChn, &stDmxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! bind clkChn and dmxChn fail:0x%x", ret);
    }
    if (pContext->bVideoFlag)
    {
        ret = AW_MPI_SYS_Bind(&stClockChn, &stVoChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind clkChn and voChn fail:0x%x", ret);
        }
    }
    if (pContext->bAudioFlag)
    {
        ret = AW_MPI_SYS_Bind(&stClockChn, &stAOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! bind clkChn and aoChn fail:0x%x", ret);
        }
    }

    return 0;
}

static int start(SampleAVPlayerContext *pContext)
{
    ERRORTYPE ret;
    ret = AW_MPI_CLOCK_Start(pContext->nClockChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! clkChn[%d] start fail:0x%x", pContext->nClockChn, ret);
    }

    if (pContext->bVideoFlag)
    {
        ret = AW_MPI_VDEC_StartRecvStream(pContext->nVdecChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vdecChn[%d] start fail:0x%x", pContext->nVdecChn, ret);
        }
        ret = AW_MPI_VO_StartChn(pContext->nVoLayer, pContext->nVoChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! voChn[%d-%d] start fail:0x%x", pContext->nVoLayer, pContext->nVoChn, ret);
        }
    }
    if (pContext->bAudioFlag)
    {
        ret = AW_MPI_ADEC_StartRecvStream(pContext->nAdecChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! adecChn[%d] start fail:0x%x", pContext->nAdecChn, ret);
        }
        ret = AW_MPI_AO_StartChn(pContext->nAODev, pContext->nAOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! aoChn[%d-%d] start fail:0x%x", pContext->nAODev, pContext->nAOChn, ret);
        }
    }
    ret = AW_MPI_DEMUX_Start(pContext->nDmxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! dmxChn[%d] start fail:0x%x", pContext->nDmxChn, ret);
    }

    return 0;
}

static int stop(SampleAVPlayerContext *pContext)
{
    ERRORTYPE ret;
    if (pContext->bVideoFlag)
    {
        ret = AW_MPI_VO_StopChn(pContext->nVoLayer, pContext->nVoChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! voChn[%d-%d] stop fail:0x%x", pContext->nVoLayer, pContext->nVoChn, ret);
        }
        ret = AW_MPI_VDEC_StopRecvStream(pContext->nVdecChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vdecChn[%d] stop fail:0x%x", pContext->nVdecChn, ret);
        }
    }
    if (pContext->bAudioFlag)
    {
        ret = AW_MPI_AO_StopChn(pContext->nAODev, pContext->nAOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! aoChn[%d-%d] stop fail:0x%x", pContext->nAODev, pContext->nAOChn, ret);
        }
        ret = AW_MPI_ADEC_StopRecvStream(pContext->nAdecChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! adecChn[%d] stop fail:0x%x", pContext->nAdecChn, ret);
        }
    }
    ret = AW_MPI_DEMUX_Stop(pContext->nDmxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! dmxChn[%d] stop fail:0x%x", pContext->nDmxChn, ret);
    }
    ret = AW_MPI_CLOCK_Stop(pContext->nClockChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! clkChn[%d] stop fail:0x%x", pContext->nClockChn, ret);
    }

    //vo stop/destroy must before vdec stop (when vo destroy, will return buffer to vdec(2 frames), just all buffer sync)
    if (pContext->bVideoFlag)
    {
        ret = AW_MPI_VO_DestroyChn(pContext->nVoLayer, pContext->nVoChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! voChn[%d-%d] destroy fail:0x%x", pContext->nVoLayer, pContext->nVoChn, ret);
        }
        if (pContext->nVoLayer >= 0)
        {
            ret = AW_MPI_VO_DisableVideoLayer(pContext->nVoLayer);
            if (ret != SUCCESS)
            {
                aloge("fatal error! voLayer[%d] disable fail:0x%x", pContext->nVoLayer, ret);
            }
        }
        //wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then vdec can free frame buffer.
        usleep(50*1000);
        ret = AW_MPI_VO_Disable(pContext->nVoDev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! voDev[%d] disable fail:0x%x", pContext->nVoDev, ret);
        }
        ret = AW_MPI_VDEC_DestroyChn(pContext->nVdecChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vdecChn[%d] destroy fail:0x%x", pContext->nVdecChn, ret);
        }
    }
    if (pContext->bAudioFlag)
    {
        ret = AW_MPI_AO_DestroyChn(pContext->nAODev, pContext->nAOChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! aoChn[%d-%d] destroy fail:0x%x", pContext->nAODev, pContext->nAOChn, ret);
        }
        ret = AW_MPI_ADEC_DestroyChn(pContext->nAdecChn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! adecChn[%d] destroy fail:0x%x", pContext->nAdecChn, ret);
        }
    }
    ret = AW_MPI_DEMUX_DestroyChn(pContext->nDmxChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! dmxChn[%d] destroy fail:0x%x", pContext->nDmxChn, ret);
    }
    ret = AW_MPI_CLOCK_DestroyChn(pContext->nClockChn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! clkChn[%d] destroy fail:0x%x", pContext->nClockChn, ret);
    }

    pContext->bVONotifyEof = false;
    pContext->bAONotifyEof = false;
    pContext->bVideoFlag = false;
    pContext->bAudioFlag = false;

    return 0;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleAVPlayerContext)
    {
        gpSampleAVPlayerContext->bOverFlag = true;
        cdx_sem_up(&gpSampleAVPlayerContext->stSemExit);
    }
}

int main(int argc, char** argv)
{
    int result = 0;
    int ret = 0;
    ERRORTYPE eRet;

    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_WARN,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);

    SampleAVPlayerContext *pContext = CreateSampleAVPlayerContext();
    if (NULL == pContext)
    {
        aloge("fatal error! create context fail!");
        return -1;
    }
    gpSampleAVPlayerContext = pContext;

    char *pConfFilePath = NULL;
    if(argc > 1)
    {
        if (parseCmdLine(pContext, argc, argv) != 0)
        {
            aloge("fatal error! parse cmd line param fail");
            goto err_out_0;
        }
        pConfFilePath = pContext->stCmdLinePara.strConfigFilePath;
    }
    else
    {
        pConfFilePath = NULL;
    }

    if (loadConfigPara(pContext, pConfFilePath) != 0)
    {
        aloge("fatal error! no config file or parse conf file fail");
        goto err_out_0;
    }

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    pContext->nSrcFd = open(pContext->stConfigPara.strSrcFile, O_RDONLY);
    if (pContext->nSrcFd < 0)
    {
        aloge("fatal error! cannot open video src file:%s", pContext->stConfigPara.strSrcFile);
        goto err_out_0;
    }

    pContext->stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->stSysConf);
    eRet = AW_MPI_SYS_Init();
    if (eRet != SUCCESS)
    {
        aloge("fatal error! mpi sys init fail:0x%x", eRet);
    }

    //decide mpp channel number in this sample.
    pContext->nDmxChn = 0;
    pContext->nVdecChn = 0;
    pContext->nAdecChn = 0;
    pContext->nVoDev = 0;
    pContext->nVoLayer = pContext->stConfigPara.nLayerId;
    pContext->nVoChn = 0;
    pContext->nAODev = 0;
    pContext->nAOChn = 0;
    pContext->nClockChn = 0;
    pContext->nUILayer = HLAY(2, 0);

    while (1)
    {
        ret = prepare(pContext);
        if (ret != 0)
        {
            aloge("fatal error! prepare failed");
            pContext->bOverFlag = true;
            goto err_out_1;
        }
        if (pContext->stConfigPara.nSeekTime > 0)
        {
            eRet = AW_MPI_DEMUX_Seek(pContext->nDmxChn, pContext->stConfigPara.nSeekTime);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! dmxChn[%d] seek to [%d]ms fail:0x%x", pContext->nDmxChn, pContext->stConfigPara.nSeekTime, eRet);
            }
        }
        ret = start(pContext);
        if (ret != 0)
        {
            aloge("fatal error! start play fail");
            pContext->bOverFlag = true;
            goto err_out_1;
        }

        if (0 == pContext->stConfigPara.nLoopCnt)
        {
            if (pContext->stConfigPara.nTestDuration > 0)
            {
                ret = cdx_sem_down_timedwait(&pContext->stSemExit, pContext->stConfigPara.nTestDuration*1000);
                if (ETIMEDOUT == ret)
                {
                    alogd("test duration [%d]s is reached, stop now.", pContext->stConfigPara.nTestDuration);
                }
            }
            else
            {
                cdx_sem_down(&pContext->stSemExit);
            }
            pContext->bOverFlag = true;
        }
        else
        {
            cdx_sem_down(&pContext->stSemExit);
            if (false == pContext->bOverFlag)
            {
                if (pContext->stConfigPara.nLoopCnt > 0)
                {
                    pContext->nLoopNum++;
                    if (pContext->nLoopNum >= pContext->stConfigPara.nLoopCnt)
                    {
                        alogd("loop cnt[%d] is reached, will stop now.", pContext->nLoopNum);
                        pContext->bOverFlag = true;
                    }
                }
            }
        }
    err_out_1:
        if (stop(pContext) != 0)
        {
            aloge("fatal error! stop fail");
        }

        alogd("overFlag:%d", pContext->bOverFlag);
        if (pContext->bOverFlag)
        {
            break;
        }
    }

    //exit mpp system
    AW_MPI_SYS_Exit();

    if (pContext->nSrcFd >= 0)
    {
        close(pContext->nSrcFd);
        pContext->nSrcFd = -1;
    }
err_out_0:
    DeleteSampleAVPlayerContext(pContext);
    gpSampleAVPlayerContext = NULL;

    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}

