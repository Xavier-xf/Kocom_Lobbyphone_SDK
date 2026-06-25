
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_uac"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>

#include <mm_common.h>
#include <mpi_sys.h>
#include <mpi_ai.h>
#include <mpi_ao.h>
#include <alsa_interface.h>
#include <confparser.h>
#include <media_common_aio.h>

#include "sample_uac_config.h"
#include "sample_uac.h"

static SampleUACContext *g_uac_context = NULL;
void signalHandler(int signo)
{
    if (g_uac_context)
        g_uac_context->exit_flag = 1;
}

static int ParseCmdLine(int argc, char **argv, SampleUACCmdLineParam *pCmdLinePara)
{
    alogd("sample_uac path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleUACCmdLineParam));
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
                "\t-path /home/sample_uac.conf\n");
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

static ERRORTYPE loadSampleUACConfig(SampleUACConfig *pConfig, const char *conf_path)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;

    pConfig->mSampleRate = 16000;
    pConfig->mBitWidth = 16;
    pConfig->mChannelCnt = 1;
    pConfig->mFrameSize = 1024;
    pConfig->enable_uac1_in = 1;
    pConfig->enable_uac1_in = 1;

    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleUACConfig));
    pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_UAC_PCM_SAMPLE_RATE, 0);
    pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_UAC_PCM_BIT_WIDTH, 0);
    pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_UAC_PCM_CHANNEL_CNT, 0);
    pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_UAC_PCM_FRAME_SIZE, 0);
    pConfig->mAecEn = GetConfParaInt(&stConfParser, SAMPLE_UAC_AEC_EN, 0);
    pConfig->mAnsEn = GetConfParaInt(&stConfParser, SAMPLE_UAC_ANS_EN, 0);
    pConfig->mAnsMode = GetConfParaInt(&stConfParser, SAMPLE_UAC_ANS_MODE, 0);
    pConfig->mAgcEn = GetConfParaInt(&stConfParser, SAMPLE_UAC_AGC_EN, 0);
    pConfig->agc_float_target_db = (float)GetConfParaDouble(&stConfParser, SAMPLE_UAC_AGC_FLOAT_TARGET_DB, 0);
    pConfig->agc_float_max_gain_db = (float)GetConfParaDouble(&stConfParser, SAMPLE_UAC_AGC_FLOAT_MAX_GAIN_DB, 30);
    pConfig->enable_uac1_in = GetConfParaInt(&stConfParser, SAMPLE_UAC_ENABLE_UAC_IN, 0);
    pConfig->enable_uac1_out = GetConfParaInt(&stConfParser, SAMPLE_UAC_ENABLE_UAC_OUT, 0);
    alogd("mSampleRate[%d], mBitWidth[%d], mChannelCnt[%d], mFrameSize[%d]",
        pConfig->mSampleRate, pConfig->mBitWidth, pConfig->mChannelCnt, pConfig->mFrameSize);
    alogd("aec[%d] ans[%d] ans mode[%d] agc[%d] agc gain[%f-%f], enable_uac1_in[%d] enable_uac1_out[%d]", \
        pConfig->mAecEn, pConfig->mAnsEn, pConfig->mAnsMode, pConfig->mAgcEn,
        pConfig->agc_float_target_db, pConfig->agc_float_max_gain_db,
        pConfig->enable_uac1_in, pConfig->enable_uac1_out);
    destroyConfParser(&stConfParser);

    return SUCCESS;
}

ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleUACContext *pCtx = (SampleUACContext*)cookie;
    int ret;
    switch (event)
    {
        case MPP_EVENT_RELEASE_AUDIO_BUFFER:
            break;
        default:
            alogd("unknown event: [%d]", event);
        break;
    }
    return 0;
}

static snd_pcm_format_t map_alsa_format(int bits_per_sample)
{
    switch (bits_per_sample)
    {
        case 8:
            return SND_PCM_FORMAT_S8;
        case 16:
            return SND_PCM_FORMAT_S16_LE;
        case 24:
            return SND_PCM_FORMAT_S24_LE;
        case 32:
            return SND_PCM_FORMAT_S32_LE;
        default:
            aloge("unsupport bits_per_sample %d use default SND_PCM_FORMAT_S16_LE", bits_per_sample);
            return SND_PCM_FORMAT_S16_LE;
    }
}

static void *uac1_out_task_proc(void *thread_data)
{
    int ret;
    PCM_CONFIG_S pcm_config;
    AUDIO_FRAME_S audio_frame;
    AUDIO_DEV ao_dev = 0;
    AO_CHN ao_chn = 0;
    int success = 0;
    ERRORTYPE result;
    char *data = NULL;
    AIO_ATTR_S ao_attr;
    SampleUACContext *uac_context = (SampleUACContext *)thread_data;

    memset(&pcm_config, 0, sizeof(PCM_CONFIG_S));
    pcm_config.chnCnt = uac_context->mConfigPara.mChannelCnt;
    pcm_config.sampleRate = uac_context->mConfigPara.mSampleRate;
    pcm_config.format = map_alsa_format(uac_context->mConfigPara.mBitWidth);
    pcm_config.bitsPerSample = uac_context->mConfigPara.mBitWidth;
    ret = alsaOpenPcm(&pcm_config, "hw:UAC1Gadget", 0);
    if (ret)
    {
        aloge("fatal error! open sound card hw:UAC1Gadget fail!");
        goto _exit;
    }
    ret = alsaSetPcmParams(&pcm_config);
    if (ret)
    {
        goto _close_uac1_card;
    }
    alsaPreparePcm(&pcm_config);
    data = malloc(pcm_config.chunkBytes);
    if (!data)
        aloge("malloc capture buffer fail!");

    while (ao_chn < AIO_MAX_CHN_NUM)
    {
        result = AW_MPI_AO_CreateChn(ao_dev, ao_chn);
        if (result == SUCCESS)
        {
            success = 1;
            break;
        }
        else if (result == ERR_AI_EXIST)
        {
            ao_chn++;
        }
        else if (result == ERR_AI_NOT_ENABLED)
        {
            break;
        }
    }
    if (!success)
    {
        aloge("create ao chn %d fail!", ao_chn);
        goto _close_uac1_card;
    }
    memset(&ao_attr, 0, sizeof(AIO_ATTR_S));
    ao_attr.enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(uac_context->mConfigPara.mSampleRate);
    ao_attr.enBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(uac_context->mConfigPara.mBitWidth);
    ao_attr.mChnCnt = uac_context->mConfigPara.mChannelCnt;
    ao_attr.enSoundmode = (ao_attr.mChnCnt == 1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    ao_attr.mPtNumPerFrm = 960;
    ao_attr.mPcmCardId = PCM_CARD_TYPE_AUDIOCODEC;
    /*result = AW_MPI_AO_SetPubAttr(ao_dev, &ao_attr);
    if (result != SUCCESS)
    {
        aloge("ao dev %d set public attr fail!", ao_dev);
        goto _destroy_ao_chn;
    }*/
    AW_MPI_AO_SetPcmCardType(ao_dev, ao_chn, PCM_CARD_TYPE_AUDIOCODEC);
    MPPCallbackInfo uac1_playback_cb = {uac_context, MPPCallbackWrapper};
    AW_MPI_AO_RegisterCallback(ao_dev, ao_chn, &uac1_playback_cb);
    result = AW_MPI_AO_StartChn(ao_dev, ao_chn);
    if (result != SUCCESS)
    {
        aloge("ao dev %d ao chn %d start fail!", ao_dev, ao_chn);
        goto _destroy_ao_chn;
    }

    while (1)
    {
        if (g_uac_context->exit_flag)
            break;
        ret = alsaReadPcm(&pcm_config, data, pcm_config.chunkSize);
        if (ret != pcm_config.chunkSize)
        {
            usleep(20*1000);
            continue;
        }

        memset(&audio_frame, 0, sizeof(AUDIO_FRAME_S));
        audio_frame.mBitwidth = ao_attr.enBitwidth;
        audio_frame.mSamplerate = ao_attr.enSamplerate;
        audio_frame.mSoundmode = ao_attr.enSoundmode;
        audio_frame.mLen = pcm_config.chunkBytes;
        audio_frame.mpAddr = (void *)data;
        AW_MPI_AO_SendFrameSync(ao_dev, ao_chn, &audio_frame);
    }

    AW_MPI_AO_StopChn(ao_dev, ao_chn);
_destroy_ao_chn:
    AW_MPI_AO_DestroyChn(ao_dev, ao_chn);
_close_uac1_card:
    if (data)
        free(data);
    alsaClosePcm(&pcm_config, 0);
_exit:
    return (void *)NULL;
}

static void *uac1_in_task_proc(void *thread_data)
{
    //create_uac1_out_task();
    int ret;
    ERRORTYPE result;
    PCM_CONFIG_S pcm_config;
    AUDIO_DEV ai_dev = 0;
    AIO_ATTR_S ai_attr;
    int success = 0;
    AI_CHN ai_chn = 0;
    AUDIO_FRAME_S audio_frame;
    int write_len;
    SampleUACContext *uac_context = (SampleUACContext *)thread_data;

    memset(&pcm_config, 0, sizeof(PCM_CONFIG_S));
    pcm_config.chnCnt = uac_context->mConfigPara.mChannelCnt;
    pcm_config.sampleRate = uac_context->mConfigPara.mSampleRate;
    pcm_config.format = map_alsa_format(uac_context->mConfigPara.mBitWidth);
    pcm_config.bitsPerSample = uac_context->mConfigPara.mBitWidth;
    ret = alsaOpenPcm(&pcm_config, "hw:UAC1Gadget", 1);
    if (ret)
    {
        aloge("fatal error! open sound card hw:UAC1Gadget fail!");
        goto _exit;
    }
    ret = alsaSetPcmParams(&pcm_config);
    if (ret)
    {
        goto _close_uac1_card;
    }
    alsaPreparePcm(&pcm_config);

    memset(&ai_attr, 0, sizeof(AIO_ATTR_S));
    ai_attr.enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(uac_context->mConfigPara.mSampleRate);
    ai_attr.enBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(uac_context->mConfigPara.mBitWidth);
    ai_attr.mChnCnt = uac_context->mConfigPara.mChannelCnt;
    ai_attr.enSoundmode = (ai_attr.mChnCnt == 1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    ai_attr.mPtNumPerFrm = 960;
    ai_attr.ai_aec_en = uac_context->mConfigPara.mAecEn;
    ai_attr.ai_ans_en = uac_context->mConfigPara.mAnsEn;
    ai_attr.ai_ans_mode = uac_context->mConfigPara.mAnsMode;
    ai_attr.ai_agc_en = uac_context->mConfigPara.mAgcEn;
    if (ai_attr.ai_agc_en)
    {
//        ai_attr.ai_agc_float_cfg.iSampleRate = ai_attr.enSamplerate;
//        ai_attr.ai_agc_float_cfg.iChannel = ai_attr.mChnCnt;
//        ai_attr.ai_agc_float_cfg.iBytePerSample = uac_context->mConfigPara.mBitWidth;
//        ai_attr.ai_agc_float_cfg.iSampleLen = uac_context->mConfigPara.mFrameSize;
        ai_attr.ai_agc_float_cfg.fTargetDb = uac_context->mConfigPara.agc_float_target_db;
        ai_attr.ai_agc_float_cfg.fMaxGainDb = uac_context->mConfigPara.agc_float_max_gain_db;
    }
    ai_attr.mPcmCardId = PCM_CARD_TYPE_AUDIOCODEC;
    result = AW_MPI_AI_SetPubAttr(ai_dev, &ai_attr);
    if (result != SUCCESS)
    {
        aloge("ai dev %d set public attr fail!", ai_dev);
        goto _close_uac1_card;
    }

    while (ai_chn < AIO_MAX_CHN_NUM)
    {
        result = AW_MPI_AI_CreateChn(ai_dev, ai_chn, NULL);
        if (result == SUCCESS)
        {
            success = 1;
            break;
        }
        else if (result == ERR_AI_EXIST)
        {
            ai_chn++;
        }
        else if (result == ERR_AI_NOT_ENABLED)
        {
            break;
        }
    }
    if (!success)
    {
        aloge("create ai chn fail!");
        goto _disable_ai_dev;
    }
    result = AW_MPI_AI_EnableChn(ai_dev, ai_chn);
    if (result != SUCCESS)
    {
        aloge("enable ai chn %d fail!", ai_chn);
        goto _destroy_ai_chn;
    }

    while (1)
    {
        if (g_uac_context->exit_flag)
            break;

        memset(&audio_frame, 0, sizeof(AUDIO_FRAME_S));
        result = AW_MPI_AI_GetFrame(ai_dev, ai_chn,
            &audio_frame, NULL, 200);
        if (result != SUCCESS)
            continue;
        write_len = audio_frame.mLen / ((ai_attr.enBitwidth+1)*8/8);
        alsaWritePcm(&pcm_config, audio_frame.mpAddr, write_len);
        AW_MPI_AI_ReleaseFrame(ai_dev, ai_chn, &audio_frame, NULL);
    }

    AW_MPI_AI_DisableChn(ai_dev, ai_chn);
    AW_MPI_AI_ResetChn(ai_dev, ai_chn);
_destroy_ai_chn:
    AW_MPI_AI_DestroyChn(ai_dev, ai_chn);
_disable_ai_dev:
    AW_MPI_AI_Disable(ai_dev);
_close_uac1_card:
    alsaDrainPcm(&pcm_config);
    alsaClosePcm(&pcm_config, 1);
_exit:
    return (void *)NULL;
}

int main(int argc, char *argv[])
{
    int result = 0;
    SampleUACContext uac_context;
    memset(&uac_context, 0, sizeof(SampleUACContext));
    g_uac_context = &uac_context;

    if(ParseCmdLine(argc, argv, &uac_context.mCmdLinePara) != 0)
    {
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(uac_context.mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = uac_context.mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = DEFAULT_SAMPLE_UAC_CONF_PATH;
    }
    if(loadSampleUACConfig(&uac_context.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    signal(SIGINT, signalHandler);

    MPP_SYS_CONF_S sys_conf;
    memset(&sys_conf, 0, sizeof(MPP_SYS_CONF_S));
    sys_conf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&sys_conf);
    AW_MPI_SYS_Init();

    pthread_t uac1_in_task_trd, uac1_out_task_trd;
    if (uac_context.mConfigPara.enable_uac1_in)
        pthread_create(&uac1_in_task_trd, NULL, uac1_in_task_proc, (void *)&uac_context);
    if (uac_context.mConfigPara.enable_uac1_out)
        pthread_create(&uac1_out_task_trd, NULL, uac1_out_task_proc, (void *)&uac_context);

    if (uac_context.mConfigPara.enable_uac1_in)
        pthread_join(uac1_in_task_trd, NULL);
    if (uac_context.mConfigPara.enable_uac1_out)
        pthread_join(uac1_out_task_trd, NULL);

    AW_MPI_SYS_Exit();
_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
