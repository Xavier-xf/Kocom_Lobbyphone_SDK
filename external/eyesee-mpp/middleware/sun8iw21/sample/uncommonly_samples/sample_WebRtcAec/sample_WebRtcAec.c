
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_WebRtcAec"
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

#include <confparser.h>

#include "sample_WebRtcAec_config.h"
#include "sample_WebRtcAec.h"

#include <cdx_list.h>

#define AecProcessUnitSize (160) //unit:alsaFrame

static SampleWebRtcAecContext *gpSampleWebRtcAecContext = NULL;

SampleWebRtcAecContext* createSampleWebRtcAecContext()
{
    SampleWebRtcAecContext *pContext = (SampleWebRtcAecContext*)malloc(sizeof(SampleWebRtcAecContext));
    if(NULL == pContext)
    {
        aloge("fatal error! malloc fail");
    }
    memset(pContext, 0, sizeof(SampleWebRtcAecContext));
    return pContext;
}

int freeSampleWebRtcAecContext(SampleWebRtcAecContext *pContext)
{
    free(pContext);
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleWebRtcAecCmdLineParam *pCmdLinePara)
{
    alogd("sample ao path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleWebRtcAecCmdLineParam));
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

static ERRORTYPE loadSampleWebRtcAecConfig(SampleWebRtcAecConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    strcpy(pConfig->mPcmInPath, "/mnt/extsd/tmp_in_ai_pcm.pcm");
    strcpy(pConfig->mPcmRefPath, "/mnt/extsd/tmp_ref_ai_pcm.pcm");
    strcpy(pConfig->mPcmOutPath, "/mnt/extsd/tmp_out_ai_pcm.pcm");
    pConfig->mSampleRate = 16000;
    pConfig->mChannelCnt = 1;
    pConfig->mBitWidth = 16;
    pConfig->mAecNlpMode = kAecNlpModerate;
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
        memset(pConfig, 0, sizeof(SampleWebRtcAecConfig));
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_WEBRTCAEC_PCM_IN_PATH, NULL);
        strncpy(pConfig->mPcmInPath, ptr, MAX_FILE_PATH_SIZE-1);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_WEBRTCAEC_PCM_REF_PATH, NULL);
        strncpy(pConfig->mPcmRefPath, ptr, MAX_FILE_PATH_SIZE-1);
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_WEBRTCAEC_PCM_OUT_PATH, NULL);
        strncpy(pConfig->mPcmOutPath, ptr, MAX_FILE_PATH_SIZE-1);
        pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_WEBRTCAEC_PCM_SAMPLE_RATE, 0);
        pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_WEBRTCAEC_PCM_CHANNEL_CNT, 0);
        pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_WEBRTCAEC_PCM_BIT_WIDTH, 0);
        pConfig->mAecNlpMode = GetConfParaInt(&stConfParser, SAMPLE_WEBRTCAEC_AEC_NLP_MODE, 0);
        pConfig->mRefPcmSkipMs = GetConfParaInt(&stConfParser, SAMPLE_WEBRTCAEC_REF_PCM_SKIP, 0);
        destroyConfParser(&stConfParser);
    }
    alogd("config:%s-%s-%s-%d-%d-%d-%d-%d", pConfig->mPcmInPath, pConfig->mPcmRefPath, pConfig->mPcmOutPath, pConfig->mSampleRate,
        pConfig->mChannelCnt, pConfig->mBitWidth, pConfig->mAecNlpMode, pConfig->mRefPcmSkipMs);
    return SUCCESS;
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
    SampleWebRtcAecContext *pContext = createSampleWebRtcAecContext();
    gpSampleWebRtcAecContext = pContext;
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
    if(loadSampleWebRtcAecConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
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
    //int WebRtcAec library
    ret = aw_WebRtcAec_Create(&pContext->mpAecHdl);
    if(NULL == pContext->mpAecHdl || ret != 0)
    {
        aloge("fatal error! create WebRtcAec lib fail: %d", ret);
    }
    ret = aw_WebRtcAec_Init(pContext->mpAecHdl, pContext->mConfigPara.mSampleRate, pContext->mConfigPara.mSampleRate);
    if(ret != 0)
    {
        aloge("fatal error! init WebRtcAec lib fail:%d", ret);
    }
    memset(&pContext->mWebRtcAecConfig, 0, sizeof(AecConfig));
    pContext->mWebRtcAecConfig.nlpMode = pContext->mConfigPara.mAecNlpMode;
    ret = aw_WebRtcAec_set_config(pContext->mpAecHdl, pContext->mWebRtcAecConfig);
    if(ret != 0)
    {
        aloge("fatal error! set config to WebRtcAec lib fail:%d", ret);
    }
    //prepare memory
    int nAlsaFrameBytes = pContext->mConfigPara.mBitWidth/8*pContext->mConfigPara.mChannelCnt;
    int nAecProcessUnitBytes = nAlsaFrameBytes * AecProcessUnitSize;
    pContext->near_buff = (short*)malloc(nAecProcessUnitBytes);
    if(NULL == pContext->near_buff)
    {
        aloge("fatal error! malloc fail");
    }
    pContext->near_buff_len = nAecProcessUnitBytes;
    pContext->ref_buff = (short*)malloc(nAecProcessUnitBytes);
    if(NULL == pContext->ref_buff)
    {
        aloge("fatal error! malloc fail");
    }
    pContext->ref_buff_len = nAecProcessUnitBytes;
    pContext->out_buff = (short*)malloc(nAecProcessUnitBytes);
    if(NULL == pContext->out_buff)
    {
        aloge("fatal error! malloc fail");
    }
    pContext->out_buff_len = nAecProcessUnitBytes;
    //aec process
    int aec_delay_ms = 0;
    int nRdCnt;
    int nWtCnt;
    //skip ref pcm
    int nSkipAlsaFrames = (int)((int64_t)pContext->mConfigPara.mSampleRate*pContext->mConfigPara.mRefPcmSkipMs/1000);
    int nSkipBytes = nAlsaFrameBytes * nSkipAlsaFrames;
    ret = fseek(pContext->mFpPcmRefFile, nSkipBytes, SEEK_SET);
    alogd("refPcm fseek [%d]bytes == [%d]alsaFrames == [%d]ms, ret:%d", nSkipBytes, nSkipAlsaFrames, pContext->mConfigPara.mRefPcmSkipMs, ret);
    while(1)
    {
        nRdCnt = fread((void*)pContext->near_buff, 1, nAecProcessUnitBytes, pContext->mFpPcmInFile);
        if(nRdCnt != nAecProcessUnitBytes)
        {
            alogw("PcmInFile eof. %d!=%d", nRdCnt, nAecProcessUnitBytes);
            break;
        }
        nRdCnt = fread((void*)pContext->ref_buff, 1, nAecProcessUnitBytes, pContext->mFpPcmRefFile);
        if(nRdCnt != nAecProcessUnitBytes)
        {
            alogw("PcmRefFile eof. %d!=%d", nRdCnt, nAecProcessUnitBytes);
            break;
        }
        ret = aw_WebRtcAec_BufferFarend(pContext->mpAecHdl, pContext->ref_buff, AecProcessUnitSize);
        if(0 != ret)
        {
            aloge("fatal error! WebRtcAec insert far data failed:%d-%d", ret, ((aecpc_t*)pContext->mpAecHdl)->lastError);
        }
        ret = aw_WebRtcAec_Process(pContext->mpAecHdl, pContext->near_buff, NULL, pContext->out_buff, NULL, AecProcessUnitSize, aec_delay_ms, 0);
        if(0 != ret)
        {
            aloge("fatal error! WebRtcAec aec process failed:%d-%d", ret, ((aecpc_t*)pContext->mpAecHdl)->lastError);
        }
        nWtCnt = fwrite((void*)pContext->out_buff, 1, nAecProcessUnitBytes, pContext->mFpPcmOutFile);
        if(nWtCnt != nAecProcessUnitBytes)
        {
            aloge("fatal error! write PcmOutFile fail. %d!=%d", nWtCnt, nAecProcessUnitBytes);
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
        ret = aw_WebRtcAec_Free(pContext->mpAecHdl);
        if(0 == ret)
        {
            pContext->mpAecHdl = NULL;
        }
        else
        {
            aloge("fatal error! WebRtcAec aec_free_failed");
        }
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
    freeSampleWebRtcAecContext(pContext);
    gpSampleWebRtcAecContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
