/**
sample_ai测试流程：
	根据配置参数采集对应的pcm数据，并写入到文件中保存。

读取测试参数的流程：
	sample提供了sample_ai.conf，测试参数包括：生成的pcm文件路径(pcm_file_path)、采样率(pcm_sample_rate)、
	通道数目(pcm_channel_cnt)、数据位宽(pcm_bit_width)。
	启动sample_ai时，在命令行参数中给出sample_ai.conf的具体路径，sample_ai会读取该文件，完成参数解析。
	然后按照参数运行测试。

从命令行启动sample_ai的指令：
	./sample_ai -path /mnt/extsd/sample_ai/sample_ai.conf
	"-path /mnt/extsd/sample_ai/sample_ai.conf"指定了测试参数配置文件的路径。

测试参数的说明：
(1)pcm_file_path：指定目标pcm文件的路径，该文件是包含wave头的wav格式文件。
(2)pcm_sample_rate：指定采样率，通常设置为8000。
(3)pcm_channel_cnt：指定通道数目，通常为1或2。
(4)pcm_bit_width：指定位宽，必须设置为16。
(5)pcm_frame_size：指定frame_size，此值可不指定。
(6)mic_num:麦克风的数量。mic_num和pcm_channel_cnt有一定的联系，但也不是必然一一对应。一般一个mic对应一个channel。
    2mic1ref模式下的回声消除，2个mic对应2个audio channel，再加上回采通道，最后回声消除为1个channel输出。
    在alsa plug插件的帮助下，即使底层为1个mic的单通道，经过plug插件后，也可以变换为2个channel输出。但2mic1ref模式，我们
    不会启用plug插件。
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "sample_ai"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <mm_common.h>
#include <media_common_aio.h>
#include <mpi_sys.h>
#include <mpi_ai.h>

#include <confparser.h>

#include "sample_ai_config.h"
#include "sample_ai.h"

#include <cdx_list.h>

static int ParseCmdLine(int argc, char **argv, SampleAICmdLineParam *pCmdLinePara)
{
    alogd("sample ai path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleAICmdLineParam));
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
                "\t-path /home/sample_ai.conf\n");
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

static ERRORTYPE loadSampleAIConfig(SampleAIConfig *pConfig, const char *conf_path)
{
    int ret = SUCCESS;
    
    strcpy(pConfig->mPcmFilePath, "/mnt/extsd/test.wav");
    pConfig->mSampleRate = 8000;
    pConfig->mMicNum = 1;
    pConfig->mChannelCnt = 1;
    pConfig->mBitWidth = 16;
    pConfig->mFrameSize = 1024;
    pConfig->mCapDuraSec = 10;
    pConfig->ai_gain = -1;
    pConfig->mAiAnsEn = 0;
    pConfig->mAiAnsMode = 0;
    pConfig->mAiAgcEn = 0;
    pConfig->mAiAgcTargetDb = 0;
    pConfig->mAiAgcMaxGainDb = 30;

    if(conf_path)
    {
        char *ptr;
        CONFPARSER_S stConfParser;

        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail");
            ret = FAILURE;
        }
        memset(pConfig, 0, sizeof(SampleAIConfig));
        ptr = (char*)GetConfParaString(&stConfParser, SAMPLE_AI_PCM_FILE_PATH, NULL);
        if (ptr == NULL)
        {
            aloge("parse item[%s] failed!", conf_path);
            ret = FAILURE;
        }
        else
        {
            strncpy(pConfig->mPcmFilePath, ptr, MAX_FILE_PATH_SIZE-1);
            alogd("parse info: mPcmFilePath[%s]", pConfig->mPcmFilePath);
        }
        pConfig->mPcmFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        pConfig->mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_SAMPLE_RATE, 0);
        pConfig->mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_BIT_WIDTH, 0);
        pConfig->mMicNum = GetConfParaInt(&stConfParser, SAMPLE_AI_MIC_NUM, 0);
        pConfig->mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_CHANNEL_CNT, 0);
        pConfig->mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_FRAME_SIZE, 0);
        pConfig->mCapDuraSec = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_CAP_DURATION, 0); 
        pConfig->ai_gain = GetConfParaInt(&stConfParser, SAMPLE_AI_PCM_GAIN, 0);
        pConfig->mAiAnsEn = GetConfParaInt(&stConfParser, SAMPLE_AI_ANS_ENABLE, 0);
        pConfig->mAiAnsMode = GetConfParaInt(&stConfParser, SAMPLE_AI_ANS_MODE, 0);
        pConfig->mAiAgcEn = GetConfParaInt(&stConfParser, SAMPLE_AI_AGC_ENABLE, 0);
        pConfig->mAiAgcTargetDb = (float)GetConfParaDouble(&stConfParser, SAMPLE_AI_AGC_TARGET_DB, 0);
        pConfig->mAiAgcMaxGainDb = (float)GetConfParaDouble(&stConfParser, SAMPLE_AI_AGC_MAX_GAIN_DB, 0);
        if (pConfig->mSampleRate && pConfig->mBitWidth && pConfig->mChannelCnt && pConfig->mFrameSize && pConfig->mCapDuraSec)
        {
//            alogd("para: mSampleRate[%d], mBitWidth[%d], mChannelCnt[%d], mFrameSize[%d], mCapTime[%d]s",
//                pConfig->mSampleRate, pConfig->mBitWidth, pConfig->mChannelCnt, pConfig->mFrameSize, pConfig->mCapDuraSec);
        }
        else
        {
            aloge("parse file(%s) for some items failed!", conf_path);
            ret = FAILURE;
        }
        destroyConfParser(&stConfParser);
    }
    alogd("para: mSampleRate[%d], mBitWidth[%d], mChannelCnt[%d], mFrameSize[%d], mCapTime[%d]s", pConfig->mSampleRate,
        pConfig->mBitWidth, pConfig->mChannelCnt, pConfig->mFrameSize, pConfig->mCapDuraSec);
    return ret;
}

void config_AIO_ATTR_S_by_SampleAIConfig(AIO_ATTR_S *dst, SampleAIConfig *src)
{
    memset(dst, 0, sizeof(AIO_ATTR_S));
    dst->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(src->mSampleRate);
    dst->enBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(src->mBitWidth);
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
    dst->mMicNum = src->mMicNum;
    dst->mChnCnt = src->mChannelCnt;
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

/*void* AgcDbGainAdjustThread(void* argv)
{
    int *exitFlag = (int*)argv;
    char buf[32] = {0};
    char *ptr;

    while (!*exitFlag) {
        fgets(buf, 32, stdin);
        // setdbgain -5 (-30~0)
        ptr = buf;
        while(*ptr++ != ' ' && *ptr != '\n');
        if (!strncmp(buf, "setdbgain ", 10)) {
            AW_MPI_AI_SetAgcDb(0, atof(ptr));
        }else if (!strncmp(buf, "getdbgain", 9)) {
            float dbgain;
            AW_MPI_AI_GetAgcDb(0, &dbgain);
            alogd("getAgcDbGain: %f", dbgain);
        }
        else {
            alogd("unknow cmd: %s!", buf);
            alogd("help: setdbgain x for set agc db gain");
            alogd("      getdbgain for get agc db gain value");
        }
        memset(buf, 0, 32);
    }
    return NULL;
}*/

void PcmDataAddWaveHeader(SampleAIContext *ctx)
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
    SampleAIConfig *pConf = &ctx->mConfigPara;

    memcpy(&header.riff_id, "RIFF", 4);
    header.riff_sz = ctx->mPcmSize + sizeof(struct WaveHeader) - 8;
    memcpy(&header.riff_fmt, "WAVE", 4);
    memcpy(&header.fmt_id, "fmt ", 4);
    header.fmt_sz = 16;
    header.audio_fmt = 1;       // s16le
    header.num_chn = pConf->mChannelCnt;
    header.sample_rate = pConf->mSampleRate;
    
    if(pConf->mBitWidth == 24) // the data captured from driver is 32bits when set 24 bitdepth
    {
        header.byte_rate = pConf->mSampleRate * pConf->mChannelCnt * 32/8;
        header.block_align = pConf->mChannelCnt * 32/8;
        header.bits_per_sample = 32;
    }
    else
    {
        header.byte_rate = pConf->mSampleRate * pConf->mChannelCnt * pConf->mBitWidth/8;
        header.block_align = pConf->mChannelCnt * pConf->mBitWidth/8;
        header.bits_per_sample = pConf->mBitWidth;
    }
    memcpy(&header.data_id, "data", 4);
    header.data_sz = ctx->mPcmSize;

    fseek(ctx->mFpPcmFile, 0, SEEK_SET);
    fwrite(&header, 1, sizeof(struct WaveHeader), ctx->mFpPcmFile);
}

int main(int argc, char *argv[])
{
    int result = 0;
    alogd("Hello, sample_ai!");
    SampleAIContext stContext;
    memset(&stContext, 0, sizeof(SampleAIContext));

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
    if(loadSampleAIConfig(&stContext.mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    //open pcm file
    stContext.mFpPcmFile = fopen(stContext.mConfigPara.mPcmFilePath, "wb");
    if(!stContext.mFpPcmFile)
    {
        aloge("fatal error! can't open pcm file[%s]", stContext.mConfigPara.mPcmFilePath);
        result = -1;
        goto _exit;
    }
    else
    {
        alogd("sample_ai produce file: %s", stContext.mConfigPara.mPcmFilePath);
        fseek(stContext.mFpPcmFile, 44, SEEK_SET);  // 44: size(WavHeader)
    }

    //init mpp system
    stContext.mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stContext.mSysConf);
    AW_MPI_SYS_Init();

    //enable ai dev
    stContext.mAIDev = 0;
    config_AIO_ATTR_S_by_SampleAIConfig(&stContext.mAIOAttr, &stContext.mConfigPara);
    AW_MPI_AI_SetPubAttr(stContext.mAIDev, &stContext.mAIOAttr);
    //embedded in AW_MPI_AI_CreateChn
    //AW_MPI_AI_Enable(stContext.mAIDev);

    //create ai channel.
    ERRORTYPE ret;
    BOOL bSuccessFlag = FALSE;
    stContext.mAIChn = 0;
    stContext.mAIChnAttr.nFrameSize = stContext.mConfigPara.mFrameSize;
    while(stContext.mAIChn < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(stContext.mAIDev, stContext.mAIChn, &stContext.mAIChnAttr);
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
        goto _exit;
    }

    AW_MPI_AI_SetDevVolume(stContext.mAIDev,stContext.mConfigPara.ai_gain);

    int tmp_gain = 0;
    AW_MPI_AI_GetDevVolume(stContext.mAIDev,&tmp_gain);

    aloge("sampl_ai_gain_value:%d",tmp_gain);

    //start ai dev.
    AW_MPI_AI_EnableChn(stContext.mAIDev, stContext.mAIChn);

    int nWriteLen;
    AUDIO_FRAME_S nAFrame;
    SampleAIConfig *pAiConf = &stContext.mConfigPara;
    //cap pcm for xx s
    int nMaxWantedSize = 0; 
    if(pAiConf->mBitWidth == 24)
    {
        nMaxWantedSize = pAiConf->mSampleRate * pAiConf->mChannelCnt * 32/8 * pAiConf->mCapDuraSec;
    }
    else
    {
        nMaxWantedSize = pAiConf->mSampleRate * pAiConf->mChannelCnt * pAiConf->mBitWidth/8 * pAiConf->mCapDuraSec;
    }

//    pthread_t tid;
//    int exit = 0;
//    pthread_create(&tid, NULL, AgcDbGainAdjustThread, &exit);

    while (1)
    {
        ret = AW_MPI_AI_GetFrame(stContext.mAIDev, stContext.mAIChn, &nAFrame, NULL, -1);
        if (SUCCESS == ret)
        {
            nWriteLen = fwrite(nAFrame.mpAddr, 1, nAFrame.mLen, stContext.mFpPcmFile);
            stContext.mPcmSize += nWriteLen;
            ret = AW_MPI_AI_ReleaseFrame(stContext.mAIDev, stContext.mAIChn, &nAFrame, NULL);
            if (SUCCESS != ret)
            {
                aloge("release frame to ai fail! ret: %#x", ret);
            }
        }
        else
        {
            aloge("get pcm from ai in block mode fail! ret: %#x", ret);
            break;
        }

        if (stContext.mPcmSize >= nMaxWantedSize)
        {
            alogd("capture %d Bytes pcm data, finish!", stContext.mPcmSize);
            break;
        }
    }
    PcmDataAddWaveHeader(&stContext);
    fclose(stContext.mFpPcmFile);
    stContext.mFpPcmFile = NULL;


    //stop ai chn.
    AW_MPI_AI_DisableChn(stContext.mAIDev, stContext.mAIChn);

    //reset and destroy ai chn & dev.
    AW_MPI_AI_ResetChn(stContext.mAIDev, stContext.mAIChn);
    AW_MPI_AI_DestroyChn(stContext.mAIDev, stContext.mAIChn);

//    exit = 1;
//    pthread_join(tid, NULL);

    stContext.mAIDev = MM_INVALID_DEV;
    stContext.mAIChn = MM_INVALID_CHN;

    //exit mpp system
    AW_MPI_SYS_Exit();

_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
