
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_aec"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include <mm_common.h>
#include <media_common_aio.h>
#include <SystemBase.h>
#include <mpi_sys.h>
#include <mpi_ao.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>
#include <ClockCompPortIndex.h>
//#include <aec_lib.h>

#include <confparser.h>

#include "sample_common_adec.h"
#include "sample_aec_config.h"
#include "sample_aec.h"

#include <cdx_list.h>

//#define USE_CURLLIB

#ifdef USE_CURLLIB
#include <curl/curl.h>
#endif

#define UvoiceLicenseCode "099230901000800047c8651b8cd337ee8099407c777c4195"
#define UvoiceLicenseFilePath "/data/uvoice.lic"
#define UvoiceUUID "eric_v853-perf1"

static SampleAecContext *gpSampleAecContext = NULL;

int initSampleAecContext(SampleAecContext *pContext)
{
    memset(pContext, 0, sizeof(SampleAecContext));
    int err = cdx_sem_init(&pContext->mSemEofCome, 0);
    if(err!=0)
    {
        aloge("cdx sem init fail!");
    }
    return 0;
}

int destroySampleAecContext(SampleAecContext *pContext)
{
    cdx_sem_deinit(&pContext->mSemEofCome);
    return 0;
}

static ERRORTYPE SampleAec_CallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleAecContext *pContext = (SampleAecContext*)cookie;
    if(MOD_ID_AO == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mAOChn)
        {
            aloge("fatal error! AO chnId[%d]!=[%d]", pChn->mChnId, pContext->mAOChn);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_AUDIO_BUFFER:
            {
                alogd("AO channel notify APP that release audio input frame!");
                break;
            }
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("AO channel notify APP that play complete!");
                pContext->eof_flag = 1;
                cdx_sem_up(&pContext->mSemEofCome);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_AO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else if (MOD_ID_AI == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mAIChn)
        {
            aloge("fatal error! AI chnId[%d]!=[%d]", pChn->mChnId, pContext->mAIChn);
        }
        switch(event)
        {
            case MPP_EVENT_CAPTURE_AUDIO_DATA:
            {
                AISendDataInfo *pDataInfo = (AISendDataInfo*)pEventData;
                //alogd("AIChannel transport pcm:%d bytes, ignore:%d, pts:%lldms", pDataInfo->mLen, pDataInfo->mbIgnore, pDataInfo->mPts/1000);
                break;
            }
            default:
            alogw("ai chn should not send callback event[0x%x]!", event);
            break;
        }
    }
    else if (MOD_ID_CLOCK == pChn->mModId)
    {
        if(pChn->mChnId != pContext->mClockChn)
        {
            aloge("fatal error! CLK chnId[%d]!=[%d]", pChn->mChnId, pContext->mClockChn);
        }
        switch(event)
        {
            default:
            alogw("clk chn should not send callback event[0x%x]!", event);
            break;
        }
    }
    else
    {
        aloge("fatal error! why modId[0x%x] callback?", pChn->mModId);
        ret = FAILURE;
    }
    return ret;
}

typedef struct CurlWriteReceiver
{
    char strOutputString[1024];
    int nOutputLen;
}CurlWriteReceiver;

// receive server response
static size_t CurlWriteCb(void *buffer, size_t size, size_t nmemb, void *userp)
{
    CurlWriteReceiver *pRec = (CurlWriteReceiver*)userp;
    size_t realsize = size * nmemb;
    //alogd("receive response:[%s]", (char *)buffer);
    int nMaxLen = sizeof(pRec->strOutputString);
    if(pRec->nOutputLen + realsize > nMaxLen - 1)
    {
        aloge("fatal error! data exceed: %d + %d > %d", pRec->nOutputLen, realsize, nMaxLen - 1);
    }
    memcpy((void*)(pRec->strOutputString+pRec->nOutputLen), buffer, realsize);
    pRec->nOutputLen += realsize;
    pRec->strOutputString[pRec->nOutputLen] = '\0';
    return realsize;
}

static ERRORTYPE uvoice_auth_cb_curl(UvoiceServerHandshake *pHandshake)
{
    const char* body = pHandshake->pPostBody;
    int nResponseSize = sizeof(pHandshake->response);
    memset(pHandshake->response, 0, nResponseSize);
    //alogd("Body is:%s", body);
    //i.d.: curl -k -s --data-binary $body https://srv01.51asr.com:8007/asrsn_active2
#ifndef USE_CURLLIB
    char curl_cmd[1024];
    int rc = snprintf(curl_cmd, 1024, "curl -k -s --data-binary '%s' %s", body, "https://srv01.51asr.com:8007/asrsn_active2");
    //alogd("curl_cmd1:[%s]", curl_cmd);
    if(rc > 0)
    {
        int64_t tm0 = CDX_GetSysTimeUsMonotonic()/1000;
        FILE* https_req = popen(curl_cmd, "r");
        int64_t tm1 = CDX_GetSysTimeUsMonotonic()/1000;
        if(https_req != NULL)
        {
            rc = fread(curl_cmd, 1, 1024, https_req);
            if(rc > 0)
            {
                curl_cmd[rc]=0;
                //alogd("curl_cmd2:[%s], cost:%lldms", curl_cmd, tm1-tm0);
            }
            else
            {
                aloge("fatal error! popen fread fail:%d", rc);
            }
            pclose(https_req);
        }
        else
        {
            aloge("fatal error! popen [%s] fail", curl_cmd);
        }
        strncpy(pHandshake->response, curl_cmd, nResponseSize-1);
        return SUCCESS;
    }
    else
    {
        return FAILURE;
    }
#else
    CURL *curl;
    CURLcode res;
    CurlWriteReceiver stReceiver;
    memset(&stReceiver, 0, sizeof(stReceiver));
    curl = curl_easy_init();
    if(curl == NULL)
    {
        aloge("fatal error! curl_easy_init() failed");
        return FAILURE;
    }
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_URL, "https://srv01.51asr.com:8007/asrsn_active2");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    const char *post_data = body;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(post_data));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&stReceiver);
    res = curl_easy_perform(curl);
    if(res != CURLE_OK)
    {
        aloge("fatal error! curl_easy_perform() failed: %s", curl_easy_strerror(res));
    }
    else
    {
        //alogd("curl perform success.");
    }
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if(response_code != 200)
    {
        aloge("fatal error! HTTP POST request failed with response code %ld\n", response_code);
    }
    else
    {
        //alogd("response_code == 200, responseString:[%s]", stReceiver.strOutputString);
    }
    curl_easy_cleanup(curl);
    if(stReceiver.nOutputLen > nResponseSize-1)
    {
        aloge("fatal error! server response len[%d] exceed [%d]", stReceiver.nOutputLen, nResponseSize-1);
    }
    strncpy(pHandshake->response, stReceiver.strOutputString, nResponseSize-1);
    return SUCCESS;
#endif
}

static ERRORTYPE SampleAec_AudioDevCallback(void *cookie, AUDIO_DEV nAudioDevId, AudioDevEventType eEventType, int nPara0, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleAecContext *pContext = (SampleAecContext*)cookie;
    switch(eEventType)
    {
        case AudioDevEvent_GetUvoiceLicenseParam:
        {
            UvoiceLicenseParam *pUvLicenseParam = (UvoiceLicenseParam*)pEventData;
            strcpy(pUvLicenseParam->license, UvoiceLicenseCode);
            strcpy(pUvLicenseParam->license_path, UvoiceLicenseFilePath);
            strcpy(pUvLicenseParam->uuid, UvoiceUUID);
            break;
        }
        case AudioDevEvent_GetUvoiceServerResponse:
        {
            ret = uvoice_auth_cb_curl((UvoiceServerHandshake*)pEventData);
            break;
        }
        default:
        {
            aloge("fatal error! AudioDev[%d] callback unknown event:%d", nAudioDevId, eEventType);
            ret = FAILURE;
            break;
        }
    }
    return ret;
}

static int ParseCmdLine(int argc, char **argv, SampleAecCmdLineParam *pCmdLinePara)
{
    alogd("sample ao path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleAecCmdLineParam));
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                aloge("fatal error! use -h to learn how to set parameter!!!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error! file path[%s] too long: [%d]>=[%d]!", argv[i], strlen(argv[i]), MAX_FILE_PATH_SIZE);
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], MAX_FILE_PATH_SIZE-1);
            pCmdLinePara->mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
            alogd("CmdLine param:\n"
                "\t-path /home/sample_aec.conf\n");
            ret = 1;
            break;
        }
        else
        {
            alogd("ignore invalid CmdLine param:[%s], type -h to get how to set parameter!", argv[i]);
        }
        i++;
    }
    return ret;
}

static ERRORTYPE loadSampleAecConfig(SampleAecConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    strcpy(pConfig->mPcmSrcPath, "/mnt/extsd/sample_aoref_8000_ch1_bit16_aec_30s.wav");
    strcpy(pConfig->mPcmDstPath, "/mnt/extsd/ai_cap.wav");
    strcpy(pConfig->mPcmAecPath, "/mnt/extsd/ai_aec.wav");
    pConfig->mSampleRate = 8000;
    pConfig->mMicNum = 1;
    pConfig->mChannelCnt = 1;
    pConfig->mBitWidth = 16;
    pConfig->mFrameSize = 1024;
    pConfig->mAiVolume = 100;
    pConfig->mAoVolume = 100;
    pConfig->mAiAecEn = 1;
    pConfig->mAecBypass = 0;
    pConfig->mAiAnsEn = 0;
    pConfig->mAecRefDelay = 0;
    pConfig->mAiAnsMode = 0;
    pConfig->mAiAgcEn = 0;
    pConfig->mAecNlpMode = 1;//kAecNlpModerate;
    pConfig->mbAddWavHeader = true;
    pConfig->mAiAgcTargetDb = 0;
    pConfig->mAiAgcMaxGainDb = 30;

    if(conf_path != NULL)
    {
        char *ptr;
        CONFPARSER_S stConfParser;

        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }
        memset(pConfig, 0, sizeof(SampleAecConfig));
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AEC_PCM_SRC_PATH, NULL);
        strncpy(pConfig->mPcmSrcPath, ptr, MAX_FILE_PATH_SIZE-1);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AEC_PCM_DST_PATH, NULL);
        strncpy(pConfig->mPcmDstPath, ptr, MAX_FILE_PATH_SIZE-1);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AEC_PCM_AEC_PATH, NULL);
        strncpy(pConfig->mPcmAecPath, ptr, MAX_FILE_PATH_SIZE-1);
        pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AEC_PCM_SAMPLE_RATE, 0);
        pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AEC_PCM_BIT_WIDTH, 0);
        pConfig->mMicNum = GetConfParaInt(&stConfParser, SAMPLE_AEC_MIC_NUM, 0);
        pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AEC_PCM_CHANNEL_CNT, 0);
        pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AEC_PCM_FRAME_SIZE, 0);
        pConfig->mAiVolume = GetConfParaInt(&stConfParser, SAMPLE_AEC_AI_VOLUME, 0);
        pConfig->mAoVolume = GetConfParaInt(&stConfParser, SAMPLE_AEC_AO_VOLUME, 0);
        pConfig->mAiAecEn = GetConfParaInt(&stConfParser, SAMPLE_AEC_AEC_ENABLE, 0);
        pConfig->mAecRefDelay = GetConfParaInt(&stConfParser, SAMPLE_AEC_AEC_REF_DELAY, 0);
        pConfig->mAecNlpMode = GetConfParaInt(&stConfParser, SAMPLE_AEC_AEC_NLP_MODE, 0);
        pConfig->mAecBypass = GetConfParaInt(&stConfParser, SAMPLE_AEC_AEC_BYPASS, 0);
        pConfig->mAiAnsEn = GetConfParaInt(&stConfParser, SAMPLE_AEC_ANS_ENABLE, 0);
        pConfig->mAiAnsMode = GetConfParaInt(&stConfParser, SAMPLE_AEC_ANS_MODE, 0);
        pConfig->mAiAgcEn = GetConfParaInt(&stConfParser, SAMPLE_AEC_AGC_ENABLE, 0);
        pConfig->mbAddWavHeader = (bool)GetConfParaInt(&stConfParser, SAMPLE_AEC_ADD_WAV_HEADER, 0);
        pConfig->mAiAgcTargetDb = (float)GetConfParaDouble(&stConfParser, SAMPLE_AEC_AGC_TARGET_DB, 0);
        pConfig->mAiAgcMaxGainDb = (float)GetConfParaDouble(&stConfParser, SAMPLE_AEC_AGC_MAX_GAIN_DB, 0);
        destroyConfParser(&stConfParser);
    }
    alogd("config:%s-%d-%d-%d-%d-%d-%d-%d-%d-%d-%d", pConfig->mPcmSrcPath, pConfig->mSampleRate, pConfig->mMicNum, pConfig->mChannelCnt,
        pConfig->mAiVolume, pConfig->mAoVolume, pConfig->mAiAecEn, pConfig->mAecNlpMode, pConfig->mAecBypass, pConfig->mAiAnsEn,
        pConfig->mAiAgcEn);
    return SUCCESS;
}

void config_AIO_ATTR_S_by_SampleAecConfig(AIO_ATTR_S *dst, SampleAecConfig *src)
{
    memset(dst, 0, sizeof(AIO_ATTR_S));
    dst->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(src->mSampleRate);
    dst->enBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(src->mBitWidth);
    if(src->mAiAecEn)
    {
        dst->enSoundmode = AUDIO_SOUND_MODE_MONO;
    }
    else
    {
        if(1 == src->mChannelCnt)
        {
            dst->enSoundmode = AUDIO_SOUND_MODE_MONO;
        }
        else if(2 == src->mChannelCnt)
        {
            dst->enSoundmode = AUDIO_SOUND_MODE_STEREO;
        }
        else
        {
            aloge("fatal error! channel count[%d] wrong", src->mChannelCnt);
            dst->enSoundmode = AUDIO_SOUND_MODE_MONO;
        }
    }
    dst->mPtNumPerFrm = 960;
    dst->mMicNum = src->mMicNum;
    dst->mChnCnt = src->mChannelCnt;
    dst->ai_aec_en = src->mAiAecEn;
    dst->aec_delay_ms = src->mAecRefDelay;
    dst->mAecNlpMode = src->mAecNlpMode;
    dst->mbBypassAec = src->mAecBypass;
    dst->ai_ans_en = src->mAiAnsEn;
    dst->ai_ans_mode = src->mAiAnsMode;
    dst->ai_agc_en = src->mAiAgcEn;

   /*
    * agc param
    * SampleRate > 8000Hz
    * Channel only support 1 or 2 channels
    * SampleLen is the frame, it will be match to input len
    * BitWidth Only Support 16bit
    * TargetDb range: [-30~0], Support float.
    * MaxGaintDb range: [0~95], Support float.
    */

    if(dst->ai_agc_en)
    {
//        dst->ai_agc_float_cfg.iSampleRate = src->mSampleRate;
//        dst->ai_agc_float_cfg.iChannel = src->mChannelCnt;
//        dst->ai_agc_float_cfg.iBytePerSample = src->mBitWidth / 8;
//        dst->ai_agc_float_cfg.iSampleLen = 1024;
        dst->ai_agc_float_cfg.fTargetDb = src->mAiAgcTargetDb;
        dst->ai_agc_float_cfg.fMaxGainDb = src->mAiAgcMaxGainDb;
    }
}

static void PcmDataAddWaveHeader(SampleAecContext *pContext)
{
    struct WaveHeader{
        int riff_id;
        int riff_sz;
        int riff_fmt;
        int fmt_id;
        int fmt_sz;
        short audio_fmt;
        short num_chn;
        int sample_rate;
        int byte_rate;
        short block_align;
        short bits_per_sample;
        int data_id;
        int data_sz;
    } header;
    SampleAecConfig *pConf = &pContext->mConfigPara;
    AIO_ATTR_S *pAioAttr = &pContext->mAIOAttr;
    int nDstChnCnt;
    if(AUDIO_SOUND_MODE_MONO == pAioAttr->enSoundmode)
    {
        nDstChnCnt = 1;
    }
    else if(AUDIO_SOUND_MODE_STEREO == pAioAttr->enSoundmode)
    {
        nDstChnCnt = 2;
    }
    else
    {
        aloge("fatal error! wrong dst sound mode:%d", pAioAttr->enSoundmode);
        nDstChnCnt = 1;
    }
    memcpy(&header.riff_id, "RIFF", 4);
    header.riff_sz = pContext->mSavePcmSize + sizeof(struct WaveHeader) - 8;
    memcpy(&header.riff_fmt, "WAVE", 4);
    memcpy(&header.fmt_id, "fmt ", 4);
    header.fmt_sz = 16;
    header.audio_fmt = 1;       // s16le
    header.num_chn = nDstChnCnt;
    header.sample_rate = pConf->mSampleRate;
    header.byte_rate = pConf->mSampleRate * nDstChnCnt * pConf->mBitWidth/8;
    header.block_align = nDstChnCnt * pConf->mBitWidth/8;
    header.bits_per_sample = pConf->mBitWidth;
    memcpy(&header.data_id, "data", 4);
    header.data_sz = pContext->mSavePcmSize;

    fseek(pContext->mFpSaveWavFile, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(struct WaveHeader), pContext->mFpSaveWavFile);

    //write aec file
    if(pContext->mFpSaveAecWavFile)
    {
        int nAecChnCnt = 1;
        header.riff_sz = pContext->mSaveAecPcmSize + sizeof(struct WaveHeader) - 8;
        header.num_chn = nAecChnCnt;
        header.byte_rate = pConf->mSampleRate * nAecChnCnt * pConf->mBitWidth/8;
        header.block_align = nAecChnCnt * pConf->mBitWidth/8;
        header.data_sz = pContext->mSaveAecPcmSize;

        fseek(pContext->mFpSaveAecWavFile, 0, SEEK_SET);
        fwrite(&header, 1, sizeof(struct WaveHeader), pContext->mFpSaveAecWavFile);
    }
}

static void* AiSaveDataThread(void *param)
{
    SampleAecContext *pCtx = (SampleAecContext*)param;
    AUDIO_FRAME_S frame;
    AEC_FRAME_S stAecFrm;
    ERRORTYPE ret = SUCCESS;
    int nWriteLen;
    pCtx->mFpSaveWavFile = fopen(pCtx->mConfigPara.mPcmDstPath, "wb");
    alogd("AiSaveDataThread cap data file: %s, dst_fp:%p", pCtx->mConfigPara.mPcmDstPath, pCtx->mFpSaveWavFile);
    if(pCtx->mConfigPara.mbAddWavHeader)
    {
        fseek(pCtx->mFpSaveWavFile, 44, SEEK_SET);  // 44: size(WavHeader)
    }
    if(pCtx->mConfigPara.mAecBypass)
    {
        pCtx->mFpSaveAecWavFile = fopen(pCtx->mConfigPara.mPcmAecPath, "wb");
        alogd("aec data file: %s, dst_fp:%p", pCtx->mConfigPara.mPcmAecPath, pCtx->mFpSaveAecWavFile);
        if(pCtx->mConfigPara.mbAddWavHeader)
        {
            fseek(pCtx->mFpSaveAecWavFile, 44, SEEK_SET);  // 44: size(WavHeader)
        }
    }
    while (!pCtx->mOverFlag)
    {
        ret = AW_MPI_AI_GetFrame(pCtx->mAIODev, pCtx->mAIChn, &frame, &stAecFrm, 500);
        if(SUCCESS == ret)
        {
            if(NULL != pCtx->mFpSaveWavFile)
            {
                nWriteLen = fwrite(frame.mpAddr, 1, frame.mLen, pCtx->mFpSaveWavFile);
                if(nWriteLen != frame.mLen)
                {
                    aloge("fatal error! fwrite[%d]!=[%d]", frame.mLen, nWriteLen);
                }
                /*unsigned long long nFrameInterval = 0; //unit:ms
                if(pCtx->nLastFramePts > 0)
                {
                    nFrameInterval = (frame.mTimeStamp - pCtx->nLastFramePts)/1000;
                }
                if(nFrameInterval < 50)
                {
                    alogd("Be careful! frameInfo:%d-%lldms-%lldms", frame.mLen, frame.mTimeStamp/1000, nFrameInterval);
                }
                else
                {
                    alogd("frameInfo:%d-%lldms-%lldms", frame.mLen, frame.mTimeStamp/1000, nFrameInterval);
                }
                pCtx->nLastFramePts = frame.mTimeStamp;*/
                pCtx->mSavePcmSize += nWriteLen;
            }
            if(NULL != pCtx->mFpSaveAecWavFile)
            {
                if(stAecFrm.bValid!=TRUE)
                {
                    aloge("fatal error! check code!");
                }
                nWriteLen = fwrite(stAecFrm.stRefFrame.mpAddr, 1, stAecFrm.stRefFrame.mLen, pCtx->mFpSaveAecWavFile);
                if(nWriteLen != stAecFrm.stRefFrame.mLen)
                {
                    aloge("fatal error! fwrite[%d]!=[%d]", stAecFrm.stRefFrame.mLen, nWriteLen);
                }
                pCtx->mSaveAecPcmSize += nWriteLen;
            }
            AW_MPI_AI_ReleaseFrame(pCtx->mAIODev, pCtx->mAIChn, &frame, &stAecFrm);
        }
        else if(ERR_AI_BUF_EMPTY == ret)
        {
            alogw("Be careful! ai getFrame timeout?");
        }
        else
        {
            aloge("fatal error! ai getFrame fail[0x%x]", ret);
        }
    }
    if(pCtx->mConfigPara.mbAddWavHeader)
    {
        PcmDataAddWaveHeader(pCtx);
    }
    fclose(pCtx->mFpSaveWavFile);
    pCtx->mFpSaveWavFile = NULL;
    if(pCtx->mFpSaveAecWavFile)
    {
        fclose(pCtx->mFpSaveAecWavFile);
        pCtx->mFpSaveAecWavFile = NULL;
    }
    return NULL;
}

static void *SendPcmAoThread(void *pThreadData)
{
    ERRORTYPE ret = SUCCESS;
    SampleAecContext *pContext = (SampleAecContext *)pThreadData;
    //read pcm from file, play pcm through mpi_ao. we set pts by stContext.mConfigPara(mSampleRate,mFrameSize).
    uint64_t nPts = 0;   //unit:us

    AUDIO_FRAME_S stFrameInfo;
    memset(&stFrameInfo, 0, sizeof(stFrameInfo));
    int nReadLen = 0;
    int nReadTotalLen = 0;
    int nWantedReadLen = pContext->mConfigPara.mFrameSize * pContext->nPlayChnNum * (pContext->nPlayBitsPerSample/8);
    stFrameInfo.mId = 0;
    stFrameInfo.mSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(pContext->nPlaySampleRate);
    stFrameInfo.mBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(pContext->nPlayBitsPerSample);
    stFrameInfo.mSoundmode = (pContext->nPlayChnNum==1)?AUDIO_SOUND_MODE_MONO:AUDIO_SOUND_MODE_STEREO;
    stFrameInfo.mLen = nWantedReadLen;
    stFrameInfo.mpAddr = malloc(stFrameInfo.mLen);
    if(NULL == stFrameInfo.mpAddr)
    {
        aloge("fatal error! malloc fail");
    }
    if(pContext->mPauseDuration > 0)
    {
        alogd("wait [%d]ms before send pcm!", pContext->mPauseDuration);
        usleep(pContext->mPauseDuration*1000);
    }
    while(1)
    {
        if(pContext->mOverFlag)
        {
            alogd("send_pcm_ao_thread receive exit flag!");
            break;
        }
        //read pcm
        nReadLen = fread(stFrameInfo.mpAddr, 1, nWantedReadLen, pContext->mFpPcmFile);
        if(nReadLen < nWantedReadLen)
        {
            int bEof = feof(pContext->mFpPcmFile);
            if(bEof)
            {
                alogd("read file finish!");
            }
            break;
        }
        nReadTotalLen += nReadLen;
        int nReadTotalSample = nReadTotalLen/(pContext->nPlayChnNum * pContext->nPlayBitsPerSample/8);
        stFrameInfo.mTimeStamp = nPts;
        nPts = (uint64_t)nReadTotalSample*1000*1000/(pContext->nPlaySampleRate);

        //send pcm to ao
        ret = AW_MPI_AO_SendFrameSync(pContext->mAIODev, pContext->mAOChn, &stFrameInfo);
        if(ret != SUCCESS)
        {
            aloge("impossible, send frameId[%d] fail?", stFrameInfo.mId);
        }
        if(false==pContext->mbPauseDone && pContext->mPauseDuration>0 && nPts/1000>=pContext->mPauseSendTm)
        {
            pContext->mbPauseDone = true;
            alogd("pause sending pcm for [%d]ms", pContext->mPauseDuration);
            usleep(pContext->mPauseDuration*1000);
        }
    }
    AW_MPI_AO_SetStreamEof(pContext->mAIODev, pContext->mAOChn, TRUE, TRUE);

    while(!pContext->eof_flag)
    {
        usleep(100*1000);
    }
    pContext->mOverFlag = TRUE;
    if(stFrameInfo.mpAddr)
    {
        free(stFrameInfo.mpAddr);
        stFrameInfo.mpAddr = NULL;
    }
    return (void*)ret;
}

static void handle_exit()
{
    alogd("user want to exit!");
    if(NULL != gpSampleAecContext)
    {
        gpSampleAecContext->mOverFlag = TRUE;
        cdx_sem_up(&gpSampleAecContext->mSemEofCome);
    }
}

int main(int argc, char *argv[])
{
    int result = 0;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
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

    alogd("Hello, sample_aec!");
    SampleAecContext stContext;
    initSampleAecContext(&stContext);
    gpSampleAecContext = &stContext;
    //parse command line param
    if(ParseCmdLine(argc, argv, &stContext.mCmdLinePara) != 0)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(stContext.mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = stContext.mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }
    //parse config file.
    if(loadSampleAecConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    stContext.mPauseSendTm = 5000;
    stContext.mPauseDuration = 0;
    stContext.mbPauseDone = true;
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }
    //open pcm file
    stContext.mFpPcmFile = fopen(stContext.mConfigPara.mPcmSrcPath, "rb");
    if(!stContext.mFpPcmFile)
    {
        aloge("fatal error! can't open pcm file[%s]", stContext.mConfigPara.mPcmSrcPath);
        result = -1;
        goto _exit;
    }
    else
    {
        int nHeaderSize = ParseWavHeader(stContext.mFpPcmFile, &stContext.nPlayChnNum, &stContext.nPlaySampleRate, &stContext.nPlayBitsPerSample);
        alogd("parse wav header size:%d, ChnNum[%d], SampleRate[%d], BitsPerSample[%d]", nHeaderSize, stContext.nPlayChnNum, stContext.nPlaySampleRate, stContext.nPlayBitsPerSample);
        if(stContext.mConfigPara.mSampleRate != stContext.nPlaySampleRate
            || stContext.mConfigPara.mChannelCnt != stContext.nPlayChnNum
            || stContext.mConfigPara.mBitWidth != stContext.nPlayBitsPerSample)
        {
            alogw("Be careful! sample aec param [%d-%d-%d] is not match playParam[%d-%d-%d]!", 
                stContext.mConfigPara.mChannelCnt, stContext.mConfigPara.mSampleRate, stContext.mConfigPara.mBitWidth,
                stContext.nPlayChnNum, stContext.nPlaySampleRate, stContext.nPlayBitsPerSample);
        }
    }
    //init mpp system

    stContext.eof_flag = 0;

    
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //enable ao dev
    stContext.mAIODev = 0;
    config_AIO_ATTR_S_by_SampleAecConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    //AW_MPI_AO_SetPubAttr(stContext.mAIODev, &stContext.mAIOAttr);
    //AW_MPI_AO_Enable(stContext.mAIODev, stContext.mAOChn);

    //create ao channel and clock channel.
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
    stContext.mAOChn = 0;
    while(stContext.mAOChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AO_CreateChn(stContext.mAIODev, stContext.mAOChn);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ao channel[%d] success!", stContext.mAOChn);
            break;
        }
        else if (ERR_AO_EXIST == ret)
        {
            alogd("ao channel[%d] exist, find next!", stContext.mAOChn);
            stContext.mAOChn++;
        }
        else if(ERR_AO_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ao not started!");
            break;
        }
        else
        {
            aloge("create ao channel[%d] fail! ret[0x%x]!", stContext.mAOChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mAOChn = MM_INVALID_CHN;
        aloge("fatal error! create ao channel fail!");
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAec_CallbackWrapper;
    AW_MPI_AO_RegisterCallback(stContext.mAIODev, stContext.mAOChn, &cbInfo);

    bSuccessFlag = FALSE;
    stContext.mClockChnAttr.nWaitMask = 0;
    stContext.mClockChnAttr.nWaitMask |= 1<<CLOCK_PORT_INDEX_AUDIO;
    stContext.mClockChn = 0;
    while(stContext.mClockChn < CLOCK_MAX_CHN_NUM)
    {
        ret = AW_MPI_CLOCK_CreateChn(stContext.mClockChn, &stContext.mClockChnAttr);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create clock channel[%d] success!", stContext.mClockChn);
            break;
        }
        else if(ERR_CLOCK_EXIST == ret)
        {
            alogd("clock channel[%d] is exist, find next!", stContext.mClockChn);
            stContext.mClockChn++;
        }
        else
        {
            alogd("create clock channel[%d] ret[0x%x]!", stContext.mClockChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mClockChn = MM_INVALID_CHN;
        aloge("fatal error! create clock channel fail!");
    }
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAec_CallbackWrapper;
    AW_MPI_CLOCK_RegisterCallback(stContext.mClockChn, &cbInfo);

    //bind clock and ao
    MPP_CHN_S ClockChn = {MOD_ID_CLOCK, 0, stContext.mClockChn};
    MPP_CHN_S AOChn = {MOD_ID_AO, stContext.mAIODev, stContext.mAOChn};
//    AW_MPI_SYS_Bind(&ClockChn, &AOChn);  // for audio only condition,no need to bind clock component with ao component.

    AO_DRC_CONFIG_S DrcCfg;
    DrcCfg.sampling_rate = stContext.mConfigPara.mSampleRate;   //8000
    DrcCfg.attack_time = 1;
    DrcCfg.release_time = 100;
    DrcCfg.max_gain = 6;
    DrcCfg.min_gain = -9;
    DrcCfg.noise_threshold = -45;
    DrcCfg.target_level = -3;
    //AW_MPI_AO_EnableSoftDrc(stContext.mAIODev, stContext.mAOChn,&DrcCfg);

    //start ao and clock.
//    pthread_create(&stContext.mAiSaveDataTid, NULL, getFrameThread, &stContext);
    
    AW_MPI_CLOCK_Start(stContext.mClockChn);
    AW_MPI_AO_StartChn(stContext.mAIODev, stContext.mAOChn);
    AW_MPI_AO_SetDevVolume(stContext.mAIODev, stContext.mConfigPara.mAoVolume);

    AW_MPI_AI_SetPubAttr(stContext.mAIODev, &stContext.mAIOAttr);
    AW_MPI_AI_RegisterDevCallback(stContext.mAIODev, (void*)&stContext, &SampleAec_AudioDevCallback);
    AW_MPI_AI_Enable(stContext.mAIODev);
    AW_MPI_AI_SetDevVolume(stContext.mAIODev, stContext.mConfigPara.mAiVolume);
    
    //create ai channel
    bSuccessFlag = FALSE;
    stContext.mAIChn = 0;
    while(stContext.mAIChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(stContext.mAIODev, stContext.mAIChn, NULL);
        if(SUCCESS == ret)
        {
            bSuccessFlag = TRUE;
            alogd("create ai channel[%d] success!", stContext.mAIChn);
            break;
        }
        else if (ERR_AI_EXIST == ret)
        {
            alogd("ai channel[%d] exist, find next!", stContext.mAIChn);
            stContext.mAIChn++;
        }
        else if(ERR_AI_NOT_ENABLED == ret)
        {
            aloge("audio_hw_ai not started!");
            break;
        }
        else
        {
            aloge("create ai channel[%d] fail! ret[0x%x]!", stContext.mAIChn, ret);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        stContext.mAIChn = MM_INVALID_CHN;
        aloge("fatal error! create ai channel fail!");
    }
    cbInfo.cookie = (void*)&stContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleAec_CallbackWrapper;
    AW_MPI_AI_RegisterCallback(stContext.mAIODev, stContext.mAIChn, &cbInfo);

    AW_MPI_AI_EnableChn(stContext.mAIODev, stContext.mAIChn);

    result = pthread_create(&stContext.mSendPcmAoTid, NULL, SendPcmAoThread, &stContext);
    if(0 == result)
    {
        alogd("pthread create send_pcm_ao_thread[0x%x]!", stContext.mSendPcmAoTid);
    }
    else
    {
        aloge("fatal error! pthread create fail[%d]!", result);
    }
    
    result = pthread_create(&stContext.mAiSaveDataTidPcm, NULL, AiSaveDataThread, &stContext);
    if(0 == result)
    {
        alogd("pthread create ai_save_data_thread[0x%x]!", stContext.mAiSaveDataTidPcm);
    }
    else
    {
        aloge("fatal error! pthread create fail[%d]!", result);
    }

    //wait pcm sent over.
    cdx_sem_down(&stContext.mSemEofCome);

    //clean before exit.
    result = pthread_join(stContext.mSendPcmAoTid, NULL);
    if(0 == result)
    {
        alogd("sendPcmAoTid[0x%x] is joined!", stContext.mSendPcmAoTid);
    }
    else
    {
        aloge("fatal error! sendPcmAoTid[0x%x] join fail[%d]!", stContext.mSendPcmAoTid, result);
    }
    result = pthread_join(stContext.mAiSaveDataTidPcm, NULL);
    if(0 == result)
    {
        alogd("aiSaveDataTidPcm[0x%x] is joined!", stContext.mAiSaveDataTidPcm);
    }
    else
    {
        aloge("fatal error! aiSaveDataTidPcm[0x%x] join fail[%d]!", stContext.mAiSaveDataTidPcm, result);
    }
    
    //stop ao channel, clock channel
    AW_MPI_AO_StopChn(stContext.mAIODev, stContext.mAOChn);
    AW_MPI_CLOCK_Stop(stContext.mClockChn);
    AW_MPI_AO_DestroyChn(stContext.mAIODev, stContext.mAOChn);
    stContext.mAOChn = MM_INVALID_CHN;
    AW_MPI_CLOCK_DestroyChn(stContext.mClockChn);
    stContext.mClockChn = MM_INVALID_CHN;

    ret = AW_MPI_AI_DisableChn(stContext.mAIODev, stContext.mAIChn); 
    ret = AW_MPI_AI_ResetChn(stContext.mAIODev, stContext.mAIChn);
    ret = AW_MPI_AI_DestroyChn(stContext.mAIODev, stContext.mAIChn);
    AW_MPI_AI_Disable(stContext.mAIODev);
    AW_MPI_AI_ClrPubAttr(stContext.mAIODev);
    //AW_MPI_AO_Disable(stContext.mAIODev, stContext.mAOChn);

    //exit mpp system
    AW_MPI_SYS_Exit();
    //close pcm file
    fclose(stContext.mFpPcmFile);
    stContext.mFpPcmFile = NULL;

_exit:
    destroySampleAecContext(&stContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
