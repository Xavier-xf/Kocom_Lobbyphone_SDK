
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_UvoiceAec"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <math.h>

#include <mm_common.h>
#include <media_common_aio.h>
#include <SystemBase.h>
#include <mpi_sys.h>
#include <mpi_ao.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>
#include <ClockCompPortIndex.h>
#include <uvoice_ecnr_api.h>
#include <uvoice_license.h>

#include <confparser.h>

#include "sample_UvoiceAec_config.h"
#include "sample_UvoiceAec.h"

#include <cdx_list.h>

#define UvoiceLicenseCode "099230901000800047c8651b8cd337ee8099407c777c4195"
#define UvoiceLicenseFilePath "/data/uvoice.lic"
#define UvoiceUUID "eric_v853-perf1"

#define AecProcessUnitSize (160) //unit:alsaFrame

static SampleUvoiceAecContext *gpSampleUvoiceAecContext = NULL;

SampleUvoiceAecContext* createSampleUvoiceAecContext()
{
    SampleUvoiceAecContext *pContext = (SampleUvoiceAecContext*)malloc(sizeof(SampleUvoiceAecContext));
    if(NULL == pContext)
    {
        aloge("fatal error! malloc fail");
    }
    memset(pContext, 0, sizeof(SampleUvoiceAecContext));
    return pContext;
}

int freeSampleUvoiceAecContext(SampleUvoiceAecContext *pContext)
{
    free(pContext);
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleUvoiceAecCmdLineParam *pCmdLinePara)
{
    alogd("sample ao path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleUvoiceAecCmdLineParam));
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

static ERRORTYPE loadSampleUvoiceAecConfig(SampleUvoiceAecConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    strcpy(pConfig->mPcmInPath, "/mnt/extsd/tmp_in_ai_pcm.pcm");
    strcpy(pConfig->mPcmRefPath, "/mnt/extsd/tmp_ref_ai_pcm.pcm");
    strcpy(pConfig->mPcmOutPath, "/mnt/extsd/tmp_out_ai_pcm.pcm");
    pConfig->mSampleRate = 16000;
    pConfig->mChannelCnt = 1;
    pConfig->mBitWidth = 16;
    pConfig->mRefPcmSkipMs = 0;
    if(conf_path != NULL)
    {
        char *ptr;
        CONFPARSER_S stConfParser;
        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("fatal error! load conf fail");
            return FAILURE;
        }
        memset(pConfig, 0, sizeof(SampleUvoiceAecConfig));
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_UVOICEAEC_PCM_IN_PATH, NULL);
        strncpy(pConfig->mPcmInPath, ptr, MAX_FILE_PATH_SIZE-1);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_UVOICEAEC_PCM_REF_PATH, NULL);
        strncpy(pConfig->mPcmRefPath, ptr, MAX_FILE_PATH_SIZE-1);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_UVOICEAEC_PCM_OUT_PATH, NULL);
        strncpy(pConfig->mPcmOutPath, ptr, MAX_FILE_PATH_SIZE-1);
        pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_UVOICEAEC_PCM_SAMPLE_RATE, 0);
        pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_UVOICEAEC_PCM_CHANNEL_CNT, 0);
        pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_UVOICEAEC_PCM_BIT_WIDTH, 0);
        pConfig->mRefPcmSkipMs = GetConfParaInt(&stConfParser, SAMPLE_UVOICEAEC_REF_PCM_SKIP, 0);
        destroyConfParser(&stConfParser);
    }
    alogd("config:%s-%s-%s-%d-%d-%d-%d", pConfig->mPcmInPath, pConfig->mPcmRefPath, pConfig->mPcmOutPath, pConfig->mSampleRate,
        pConfig->mChannelCnt, pConfig->mBitWidth, pConfig->mRefPcmSkipMs);
    return SUCCESS;
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

/**
  get server return string to take it to generate license file.
  shell command is:
  curl -k -s --data-binary $body https://srv01.51asr.com:8007/asrsn_active2
*/
static const char* uvoice_auth_cb(const char* body)
{
    //alogd("Body is:%s", body);
    UvoiceServerHandshake stHandshake;
    memset(&stHandshake, 0, sizeof(stHandshake));
    stHandshake.pPostBody = body;
    ERRORTYPE ret = uvoice_auth_cb_curl(&stHandshake);
    if(SUCCESS == ret)
    {
        return strdup(stHandshake.response);
    }
    else
    {
        aloge("fatal error! get uvoice server response fail[0x%x]", ret);
        return "";
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

    alogd("Hello, sample_WebRtcAec!");
    SampleUvoiceAecContext *pContext = createSampleUvoiceAecContext();
    gpSampleUvoiceAecContext = pContext;
    //parse command line param
    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        //aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }
    //parse config file.
    if(loadSampleUvoiceAecConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    if(pContext->mConfigPara.mChannelCnt != 1)
    {
        aloge("fatal error! now only support one channel inPcm! channelsNum[%d]", pContext->mConfigPara.mChannelCnt);
    }

    int ret;
    //open pcm files
    pContext->mFpPcmInFile = fopen(pContext->mConfigPara.mPcmInPath, "rb");
    if(!pContext->mFpPcmInFile)
    {
        aloge("fatal error! can't open pcm file[%s]", pContext->mConfigPara.mPcmInPath);
        result = -1;
        goto _exit;
    }
    pContext->mFpPcmRefFile = fopen(pContext->mConfigPara.mPcmRefPath, "rb");
    if(!pContext->mFpPcmRefFile)
    {
        aloge("fatal error! can't open pcm file[%s]", pContext->mConfigPara.mPcmRefPath);
        result = -1;
        goto _exit;
    }
    pContext->mFpPcmOutFile = fopen(pContext->mConfigPara.mPcmOutPath, "wb");
    if(!pContext->mFpPcmOutFile)
    {
        aloge("fatal error! can't open pcm file[%s]", pContext->mConfigPara.mPcmOutPath);
        result = -1;
        goto _exit;
    }
    //int UvoiceAec library
    UvEcnr_InputMode eInputMode;
    if(1 == pContext->mConfigPara.mChannelCnt)
    {
        eInputMode = uvoice_ecnr_mode_1mic1ref;
    }
    else if(2 == pContext->mConfigPara.mChannelCnt)
    {
        eInputMode = uvoice_ecnr_mode_2mic1ref;
    }
    else
    {
        aloge("fatal error! not support [%d]channels", pContext->mConfigPara.mChannelCnt);
        eInputMode = uvoice_ecnr_mode_1mic1ref;
    }
    UvEcnr_OutputMode eOutputMode = uvoice_ecnr_mode_phone;
    uv_activate_param stParam;
    memset(&stParam, 0, sizeof(stParam));
    stParam.license = UvoiceLicenseCode;
    stParam.license_path = UvoiceLicenseFilePath;
    stParam.uuid = UvoiceUUID;
    stParam.activate_type = 0;
    stParam.auth_cb = uvoice_auth_cb;
    int64_t tm0 = CDX_GetSysTimeUsMonotonic()/1000;
    pContext->mpAecHdl = UvEcnr_Create(eInputMode, eOutputMode, pContext->mConfigPara.mSampleRate, &stParam);
    const char* version = UvEcnr_GetVersion();
    int64_t tm1 = CDX_GetSysTimeUsMonotonic()/1000;
    alogd("[UVOICE] ecnr create. version: %s, license: %s-%s-%s, param:%d-%d-%d, cost:%lldms", version, stParam.license,
        stParam.license_path, stParam.uuid, eInputMode, eOutputMode, pContext->mConfigPara.mSampleRate, tm1-tm0);
    // 不适用云端授权调用API，process在半小时后将返回错误信息，处理输出为全0
    //handle_ecnr = UvEcnr_Create((UvEcnr_InputMode)in_mode,(UvEcnr_OutputMode)out_mode,sample_rate, NULL);
    // 授权失败时也会返回空句柄，注意看LOG
    if (NULL == pContext->mpAecHdl)
    {
        aloge("fatal error! UvEcnr create fail!");
    }
    uvoice_ecnr_status st = UvEcnr_Init(pContext->mpAecHdl);
    if(st != UV_ECNR_OK)
    {
        aloge("fatal error! [UVOICE] ECNR init error:%d!", st);
        UvEcnr_Destroy(pContext->mpAecHdl);
        pContext->mpAecHdl = NULL;
    }
    //prepare memory
    int nAlsaFrameBytes = pContext->mConfigPara.mBitWidth/8*pContext->mConfigPara.mChannelCnt;
    int nAecProcessUnitBytes = nAlsaFrameBytes * AecProcessUnitSize;
    int nRefAlsaFrameBytes = pContext->mConfigPara.mBitWidth/8*1;
    int nRefUnitBytes = nRefAlsaFrameBytes*AecProcessUnitSize;
    pContext->near_buff = (short*)malloc(nAecProcessUnitBytes);
    if(NULL == pContext->near_buff)
    {
        aloge("fatal error! malloc fail");
    }
    pContext->near_buff_len = nAecProcessUnitBytes;
    pContext->ref_buff = (short*)malloc(nRefUnitBytes);
    if(NULL == pContext->ref_buff)
    {
        aloge("fatal error! malloc fail");
    }
    pContext->ref_buff_len = nRefUnitBytes;
    pContext->out_buff = (short*)malloc(nRefUnitBytes);
    if(NULL == pContext->out_buff)
    {
        aloge("fatal error! malloc fail");
    }
    pContext->out_buff_len = nRefUnitBytes;
    //aec process
    int aec_delay_ms = 0;
    int nRdCnt;
    int nWtCnt;
    //skip ref pcm
    int nSkipAlsaFrames = (int)((int64_t)pContext->mConfigPara.mSampleRate*pContext->mConfigPara.mRefPcmSkipMs/1000);
    int nSkipBytes = nRefAlsaFrameBytes * nSkipAlsaFrames;
    ret = fseek(pContext->mFpPcmRefFile, nSkipBytes, SEEK_SET);
    alogd("refPcm fseek [%d]bytes == [%d]alsaFrames == [%d]ms, ret:%d", nSkipBytes, nSkipAlsaFrames, pContext->mConfigPara.mRefPcmSkipMs, ret);
    int i;
    while(1)
    {
        nRdCnt = fread((void*)pContext->near_buff, 1, nAecProcessUnitBytes, pContext->mFpPcmInFile);
        if(nRdCnt != nAecProcessUnitBytes)
        {
            alogw("PcmInFile eof. %d!=%d", nRdCnt, nAecProcessUnitBytes);
            break;
        }
        nRdCnt = fread((void*)pContext->ref_buff, 1, nRefUnitBytes, pContext->mFpPcmRefFile);
        if(nRdCnt != nRefUnitBytes)
        {
            alogw("PcmRefFile eof. %d!=%d", nRdCnt, nRefUnitBytes);
            break;
        }
        uv_ecnr_audio_buf audioBuf;
        for(i = 0; i < pContext->mConfigPara.mChannelCnt; i++)
        {
            audioBuf.audioin[i] = pContext->near_buff;
        }
        for(i = 0; i < 1; i++)
        {
            audioBuf.audioref[i] = pContext->ref_buff;
        }
        short *pOutAudio = NULL;
        st = UvEcnr_Process(pContext->mpAecHdl, &audioBuf, &pOutAudio);
        if(UV_ECNR_OK == st)
        {
            memcpy((void*)((char*)pContext->out_buff), (void*)pOutAudio, AecProcessUnitSize*(pContext->mConfigPara.mBitWidth/8));
        }
        else
        {
            if(UV_ECNR_VERIFY_ERROR == st)
            {
                aloge("fatal error! ecnr license time passed.");
            }
            aloge("fatal error! process wav failed[%d]", st);
        }
        nWtCnt = fwrite((void*)pContext->out_buff, 1, nRefUnitBytes, pContext->mFpPcmOutFile);
        if(nWtCnt != nRefUnitBytes)
        {
            aloge("fatal error! write PcmOutFile fail. %d!=%d", nWtCnt, nRefUnitBytes);
        }
    }
    //free memory
    if(pContext->near_buff)
    {
        free(pContext->near_buff);
        pContext->near_buff = NULL;
        pContext->near_buff_len = 0;
    }
    if(pContext->ref_buff)
    {
        free(pContext->ref_buff);
        pContext->ref_buff = NULL;
        pContext->ref_buff_len = 0;
    }
    if(pContext->out_buff)
    {
        free(pContext->out_buff);
        pContext->out_buff = NULL;
        pContext->out_buff_len = 0;
    }
    //destroy WebRtcAec lib
    if(NULL != pContext->mpAecHdl)
    {
        UvEcnr_Destroy(pContext->mpAecHdl);
        pContext->mpAecHdl = NULL;
    }
    //close pcm files
    if(pContext->mFpPcmInFile)
    {
        fclose(pContext->mFpPcmInFile);
        pContext->mFpPcmInFile = NULL;
    }
    if(pContext->mFpPcmRefFile)
    {
        fclose(pContext->mFpPcmRefFile);
        pContext->mFpPcmRefFile = NULL;
    }
    if(pContext->mFpPcmOutFile)
    {
        fclose(pContext->mFpPcmOutFile);
        pContext->mFpPcmOutFile = NULL;
    }

_exit:
    freeSampleUvoiceAecContext(pContext);
    gpSampleUvoiceAecContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
