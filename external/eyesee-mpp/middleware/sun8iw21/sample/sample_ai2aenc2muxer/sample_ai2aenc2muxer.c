/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_ai2aenc2muxer.c
  Version       : V1.0
  Author        : Allwinner BU3-PD2 Team
  Created       : 2017/09/08
  Last Modified :
  Description   : test code for ai & aenc & muxer
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleAI2AEnc2Muxer"
#include "plat_log.h"

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>

#include "mm_comm_sys.h"
#include "mm_comm_aio.h"
#include "mm_comm_aenc.h"
#include <media_common_aio.h>
#include <mpi_sys.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>
#include <aenc_sw_lib.h>

#include "sample_ai2aenc2muxer.h"

//#define TEST_LEAKTRACER

#ifdef TEST_LEAKTRACER
#include <LeakTracer/leaktracer.h>
#endif

// Default Params definition
#define DEFAULT_CONF_FILE_PATH      "/mnt/extsd/sample_ai2aenc2muxer/sample_ai2aenc2muxer.conf"
#define DEFAULT_DST_FILE_PATH       "/mnt/extsd/test.aac"
#define DEFAULT_FILE_FORMAT         MEDIA_FILE_FORMAT_AAC
#define DEFAULT_AUDIO_ENCODE_TYPE   PT_AAC
#define DEFAULT_CAPTURE_DURATION    (10)   //Unit:Second
#define DEFAULT_CHANNEL_COUNT       (1)
#define DEFAULT_BIT_WIDTH           (16)
#define DEFAULT_SAMPLE_RATE         (8000)
#define DEFAULT_BITRATE             (16000)

static int parseCmdLine(SAMPLE_AI2AENC2MUXER_S *pSampleData, int argc, char** argv)
{
    int ret = 0;

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

              strncpy(pSampleData->confFilePath, *argv, MAX_FILE_PATH_LEN);
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            alogd("CmdLine param: -path /mnt/extsd/sample_ai2aenc2muxer/sample_ai2aenc2muxer.conf");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SAMPLE_AI2AENC2MUXER_S *pSampleData)
{
    int ret;
    char *ptr;

    char *pConfFilePath;
	// Ensure config file valid
    if (!strlen(pSampleData->confFilePath))
    {
        //alogw("use dafault confFile [%s]", DEFAULT_CONF_FILE_PATH);
        //strncpy(pSampleData->confFilePath, DEFAULT_CONF_FILE_PATH, MAX_FILE_PATH_LEN);
        pConfFilePath = NULL;
    }
    else
    {
        pConfFilePath = pSampleData->confFilePath;
    }

    strncpy(pSampleData->mConfDstFile, DEFAULT_DST_FILE_PATH, MAX_FILE_PATH_LEN);
    pSampleData->mConfFileFormat = DEFAULT_FILE_FORMAT;
    pSampleData->mConfCodecType = DEFAULT_AUDIO_ENCODE_TYPE;
    pSampleData->mConfCapDuration = DEFAULT_CAPTURE_DURATION;
    pSampleData->mConfChnCnt      = DEFAULT_CHANNEL_COUNT;
    pSampleData->mConfBitWidth    = DEFAULT_BIT_WIDTH;
    pSampleData->mConfSampleRate  = DEFAULT_SAMPLE_RATE;
    pSampleData->mConfBitRate   = DEFAULT_BITRATE;
    pSampleData->mConfAISaveFileFlag = false;
        
    if(pConfFilePath)
    {
		// Load config file
        CONFPARSER_S cfg;
        ret = createConfParser(pSampleData->confFilePath, &cfg);
        if (ret < 0)
        {
            aloge("load conf fail!");
            return FAILURE;
        }
		
		// Read params
        ptr = (char*)GetConfParaString(&cfg, DST_FILE_PATH, NULL);   //read dest file
        strncpy(pSampleData->mConfDstFile, ptr, MAX_FILE_PATH_LEN);
        ptr = strrchr(pSampleData->mConfDstFile, '.') + 1;
        if (!strcmp(ptr, "aac"))
        {
            pSampleData->mConfFileFormat = MEDIA_FILE_FORMAT_AAC;
        }
        else if (!strcmp(ptr, "mp3"))
        {
            pSampleData->mConfFileFormat = MEDIA_FILE_FORMAT_MP3;
        }
        else if (!strcmp(ptr, "wav"))
        {
            pSampleData->mConfFileFormat = MEDIA_FILE_FORMAT_WAV;
        }
        else
        {
            alogw("Unknown audio file format[%s]! Set to default [aac]", ptr);
            pSampleData->mConfFileFormat = MEDIA_FILE_FORMAT_AAC;
        }

        ptr = (char*)GetConfParaString(&cfg, CODEC_TYPE, "aac");   //read dest file
        if (!strcmp(ptr, "aac"))
        {
            pSampleData->mConfCodecType = PT_AAC;
        }
        else if (!strcmp(ptr, "mp3"))
        {
            pSampleData->mConfCodecType = PT_MP3;
        }
        else if (!strcmp(ptr, "pcm"))
        {
            pSampleData->mConfCodecType = PT_PCM_AUDIO;
        }
        else if (!strcmp(ptr, "g711a"))
        {
            pSampleData->mConfCodecType = PT_G711A;
        }
        else if (!strcmp(ptr, "g711u"))
        {
            pSampleData->mConfCodecType = PT_G711U;
        }
        else
        {
            alogw("Unknown audio codec type[%s]! Set to default [aac]", ptr);
            pSampleData->mConfCodecType = PT_AAC;
        }
       
        pSampleData->mConfCapDuration = GetConfParaInt(&cfg, CAPTURE_DURATION, 0); // capture duration
		
        pSampleData->mConfChnCnt     = GetConfParaInt(&cfg, CHANNEL_COUNT, 0);	// audio params
        pSampleData->mConfBitWidth   = GetConfParaInt(&cfg, BIT_WIDTH, 0);
        pSampleData->mConfSampleRate = GetConfParaInt(&cfg, SAMPLE_RATE, 0);
        pSampleData->mConfBitRate = GetConfParaInt(&cfg, KEY_BITRATE, 0);
		
		// Release config file
        destroyConfParser(&cfg);
    }
	// show config
    alogd("config para: dst_file [%s], codec_type [%d]\n"
          "             cap_duration [%d], chn_cnt [%d], bit_width [%d], sample_rate [%d], bitRate [%d]",
        pSampleData->mConfDstFile, pSampleData->mConfCodecType,
        pSampleData->mConfCapDuration, pSampleData->mConfChnCnt, pSampleData->mConfBitWidth, pSampleData->mConfSampleRate, pSampleData->mConfBitRate);
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SAMPLE_AI2AENC2MUXER_S *pContext = (SAMPLE_AI2AENC2MUXER_S *)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_AI == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_CAPTURE_AUDIO_DATA:
            {
                AISendDataInfo *pAiDataInfo = (AISendDataInfo*)pEventData;
                //alogd("aiChn[%d] capture audio data:%lldus-%d-%d", pChn->mChnId, pAiDataInfo->mPts, pAiDataInfo->mLen, pAiDataInfo->mbIgnore);
                break;
            }
            default:
            {
                aloge("fatal error! receive aiChn[%d] event[%d]", pChn->mChnId, event);
                break;
            }
        }
    }
    else if (MOD_ID_AENC == pChn->mModId)
    {
        aloge("fatal error! receive aencChn[%d] event[%d]", pChn->mChnId, event);
    }
    else if (MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                MUX_CHN nMuxChn = (MUX_CHN)*(int*)pEventData;
                alogd("MuxChn[%d] record file done.", nMuxChn);
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                MUX_CHN nMuxChn = (MUX_CHN)*(int*)pEventData;
                alogd("MuxChn[%d] need next fd.", nMuxChn);
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("muxChn[%d] bs frame available", pChn->mChnId);
                break;
            }
            default:
            {
                aloge("fatal error! muxChn[%d] receive mux known event:%d", pChn->mChnId, event);
                break;
            }
        }
    }

    return SUCCESS;
}

void configAioAttr(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    AIO_ATTR_S *pAttr = &ctx->mAioAttr;

    pAttr->mChnCnt    = ctx->mConfChnCnt;
    if(1 == pAttr->mChnCnt)
    {
        pAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
    }
    else if(2 == pAttr->mChnCnt)
    {
        pAttr->enSoundmode = AUDIO_SOUND_MODE_STEREO;
    }
    else
    {
        aloge("fatal error! unsupport chnCnt[%d]", pAttr->mChnCnt);
        pAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
    }
    pAttr->mMicNum = 1;
    pAttr->enBitwidth   = map_BitWidth_to_AUDIO_BIT_WIDTH_E(ctx->mConfBitWidth);
    pAttr->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(ctx->mConfSampleRate);
}

void configAEncAttr(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    AENC_CHN_ATTR_S *pAttr = &ctx->mAEncAttr;
    pAttr->AeAttr.Type = ctx->mConfCodecType;
    pAttr->AeAttr.channels = ctx->mConfChnCnt;
    pAttr->AeAttr.bitsPerSample = ctx->mConfBitWidth;
    pAttr->AeAttr.sampleRate = ctx->mConfSampleRate;
    pAttr->AeAttr.bitRate = ctx->mConfBitRate;
    pAttr->AeAttr.attachAACHeader = 0; //aacMuxer will add adts header, so aac encoder need not attach aac header.
    pAttr->AeAttr.mInBufSize = 0;
    pAttr->AeAttr.mOutBufCnt = 0;
}

static ERRORTYPE configMuxChnAttr(SAMPLE_AI2AENC2MUXER_S *pContext)
{
    memset(&pContext->mMuxChnAttr, 0, sizeof(MUX_CHN_ATTR_S));

    pContext->mMuxChnAttr.mChannels = pContext->mConfChnCnt;
    pContext->mMuxChnAttr.mBitsPerSample = pContext->mConfBitWidth;
    pContext->mMuxChnAttr.mSamplesPerFrame = MAXDECODESAMPLE;
    pContext->mMuxChnAttr.mSampleRate = pContext->mConfSampleRate;
    pContext->mMuxChnAttr.mAudioEncodeType = pContext->mConfCodecType;
    pContext->mMuxChnAttr.mTextEncodeType = PT_MAX;

    //pContext->mMuxChnAttr.mMuxerId = pContext->mMuxerIdCounter++;
    pContext->mMuxChnAttr.mMediaFileFormat = pContext->mConfFileFormat;
    pContext->mMuxChnAttr.mMaxFileDuration = 0;
    pContext->mMuxChnAttr.mMaxFileSizeBytes = 0;
    pContext->mMuxChnAttr.mCallbackOutFlag = FALSE;
    pContext->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pContext->mMuxChnAttr.mSimpleCacheSize = 64*1024;
    pContext->mMuxChnAttr.mAddRepairInfo = 0;
    pContext->mMuxChnAttr.mMaxFrmsTagInterval = 0;
    return SUCCESS;
}

static ERRORTYPE createAIChn(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    //enable audio_hw_ai
    AW_MPI_AI_SetPubAttr(ctx->mAIDevId, &ctx->mAioAttr);
    AW_MPI_AI_Enable(ctx->mAIDevId);

    BOOL nSuccessFlag = FALSE;
    ERRORTYPE ret = 0;
    while (ctx->mAIChnId < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(ctx->mAIDevId, ctx->mAIChnId, NULL);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", ctx->mAIChnId);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", ctx->mAIChnId);
            ctx->mAIChnId++;
        }
        else if (ERR_AI_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! ret[0x%x]!", ctx->mAIChnId, ret);
            break;
        }
    }
    if(FALSE == nSuccessFlag)
    {
        ctx->mAIChnId = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
        ret = -1;
    }
    else
    {
        ctx->mAiChn.mModId = MOD_ID_AI;
        ctx->mAiChn.mDevId = ctx->mAIDevId;
        ctx->mAiChn.mChnId = ctx->mAIChnId;
    }

    return ret;
}

static ERRORTYPE createAEncChn(SAMPLE_AI2AENC2MUXER_S *ctx)
{
    BOOL nSuccessFlag = FALSE;
    ERRORTYPE ret = 0;
    while (ctx->mAEncChnId < AENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_AENC_CreateChn(ctx->mAEncChnId, &ctx->mAEncAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create aenc channel[%d] success!", ctx->mAEncChnId);
            break;
        }
        else if (ERR_AENC_EXIST == ret)
        {
            alogd("aenc channel[%d] exist, find next!", ctx->mAEncChnId);
            ctx->mAEncChnId++;
        }
        else
        {
            alogd("create aenc channel[%d] ret[0x%x], find next!", ctx->mAEncChnId, ret);
            ctx->mAEncChnId++;
        }
    }
    if (FALSE == nSuccessFlag)
    {
        ctx->mAEncChnId = MM_INVALID_CHN;
        aloge("fatal error! create aenc channel fail!");
        ret = -1;
    }
    else
    {
        ctx->mAEncChn.mModId = MOD_ID_AENC;
        ctx->mAEncChn.mDevId = 0;
        ctx->mAEncChn.mChnId = ctx->mAEncChnId;
    }

    return ret;
}

static ERRORTYPE createMuxChn(SAMPLE_AI2AENC2MUXER_S *pContext)
{
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    configMuxChnAttr(pContext);
    pContext->mMuxChn = 0;
    while (pContext->mMuxChn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(pContext->mMuxChn, &pContext->mMuxChnAttr, pContext->mFdDst, 0);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux chn[%d] success!", pContext->mMuxChn);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("mux chn[%d] is exist, find next!", pContext->mMuxChn);
            pContext->mMuxChn++;
        }
        else
        {
            alogd("create mux chn[%d] ret[0x%x], find next!", pContext->mMuxChn, ret);
            pContext->mMuxChn++;
        }
    }

    if (FALSE == nSuccessFlag)
    {
        pContext->mMuxChn = MM_INVALID_CHN;
        aloge("fatal error! create mux chn fail!");
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pContext->mMuxChn, &cbInfo);

        pContext->mMppMuxChn.mModId = MOD_ID_MUX;
        pContext->mMppMuxChn.mDevId = 0;
        pContext->mMppMuxChn.mChnId = pContext->mMuxChn;
        return SUCCESS;
    }
}

int main(int argc, char** argv)
{
    int ret = 0;
    SAMPLE_AI2AENC2MUXER_S nSampleContext;
    memset(&nSampleContext, 0, sizeof(SAMPLE_AI2AENC2MUXER_S));

    if (parseCmdLine(&nSampleContext, argc, argv) != 0)
    {
        aloge("parseCmdLine fail!");
    }

    if (loadConfigPara(&nSampleContext) != SUCCESS)
    {
        aloge("no config file or parse conf file fail");
        goto _END;
    }

    nSampleContext.mFdDst = open(nSampleContext.mConfDstFile, O_RDWR | O_CREAT, 0666);
    if (nSampleContext.mFdDst < 0)
    {
        aloge("cann't open dest file %s", nSampleContext.mConfDstFile);
        goto _END;
    }
    // init mpp system
    nSampleContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&nSampleContext.mSysConf);
    AW_MPI_SYS_Init();

#ifdef TEST_LEAKTRACER
    //leaktracer has some problems with alsa functions. So first run alsa functions such as snd_pcm_open(), snd_mixer_attach(),
    //then run leaktracer. Then LeakTracer can work with alsa functions normally.
    leaktracer_startMonitoringAllThreads();
#endif

    // config ai & aenc attr
    configAioAttr(&nSampleContext);
    configAEncAttr(&nSampleContext);

    // config ai & aenc chn id
    nSampleContext.mAIDevId = 0;
    nSampleContext.mAIChnId = 0;
    nSampleContext.mAEncChnId = 0;

    // create ai & aenc chn
    if (createAIChn(&nSampleContext) != SUCCESS)
    {
        aloge("create ai chn fail!");
        goto _END;
    }
    if (createAEncChn(&nSampleContext) != SUCCESS)
    {
        aloge("create aenc chn fail!");
        goto _END;
    }
    if (createMuxChn(&nSampleContext) != SUCCESS)
    {
        aloge("create mux channel fail");
        goto _END;
    }

    // test ai save file api
    if(nSampleContext.mConfAISaveFileFlag)
    {
        strcpy(nSampleContext.mSaveFileInfo.mFilePath, "/mnt/extsd/");
        strcpy(nSampleContext.mSaveFileInfo.mFileName, "SampleAi2Aenc2Muxer_AiSaveFile.pcm");
        AW_MPI_AI_SaveFile(nSampleContext.mAIDevId, nSampleContext.mAIChnId, &nSampleContext.mSaveFileInfo);
    }

    // bind ai & aenc & muxer
    AW_MPI_SYS_Bind(&nSampleContext.mAiChn, &nSampleContext.mAEncChn);
    AW_MPI_SYS_Bind(&nSampleContext.mAEncChn, &nSampleContext.mMppMuxChn);

	// set start time
    alogd("will capture for %d seconds, wait ...", nSampleContext.mConfCapDuration);
//    struct timeval tv;
//    long long val_begin, val_end;
//    gettimeofday(&tv, NULL);
//    val_begin = 1000000 * tv.tv_sec + tv.tv_usec;

    //start ai & aenc & muxer
    AW_MPI_AI_EnableChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    AW_MPI_AENC_StartRecvPcm(nSampleContext.mAEncChnId);
    AW_MPI_MUX_StartChn(nSampleContext.mMuxChn);

    //capturing
    sleep(nSampleContext.mConfCapDuration);

    // stop ai & aenc
    AW_MPI_AI_DisableChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    AW_MPI_AENC_StopRecvPcm(nSampleContext.mAEncChnId);
    AW_MPI_MUX_StopChn(nSampleContext.mMuxChn, FALSE);

    // destruct ai & aenc & muxer
    AW_MPI_MUX_DestroyChn(nSampleContext.mMuxChn);
    //AW_MPI_AENC_ResetChn(nSampleContext.mAEncChnId);
    AW_MPI_AENC_DestroyChn(nSampleContext.mAEncChnId);
    //AW_MPI_AI_ResetChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    AW_MPI_AI_DestroyChn(nSampleContext.mAIDevId, nSampleContext.mAIChnId);
    
    // exit mpp system
    AW_MPI_SYS_Exit();

#ifdef TEST_LEAKTRACER
    leaktracer_stopAllMonitoring();
    leaktracer_writeLeaksToFile("/mnt/extsd/leaks.out");
#endif

_END:
    if(nSampleContext.mFdDst >= 0)
    {
        close(nSampleContext.mFdDst);
        nSampleContext.mFdDst = -1;
    }
    alogd("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));
    return ret;
}
