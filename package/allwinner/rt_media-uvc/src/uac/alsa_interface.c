/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : alsa_interface.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/04/19
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/
//#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_VERBOSE
#include <math.h>
#include "include/alsa_interface.h"
#include "../utils/debug/include/debug.h"

#define PERIOD_SIZE (160)
#define PERIOD_COUNT (8)

static int64_t CDX_GetSysTimeUsMonotonic()
{
    long long curr;
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = ((long long)(t.tv_sec)*1000000000LL + t.tv_nsec)/1000LL;
    return (int64_t)curr;
}

int alsaSetPcmParams(PCM_CONFIG_S *pcmCfg)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *sw_params;
    int err;
    unsigned int rate, bufTime, periodTime;
//    snd_pcm_uframes_t startThreshold, stopThreshold;
    int dir = 0;

    if (pcmCfg->handle == NULL) {
        loge("PCM is not open yet!");
        return -1;
    }
    logv("set pcm params");

    snd_pcm_hw_params_alloca(&params);
//    snd_pcm_sw_params_alloca(&swparams);
    err = snd_pcm_hw_params_any(pcmCfg->handle, params);
    if (err < 0) {
        loge("Broken configuration for this PCM: no configurations available");
        return -1;
    }

    err = snd_pcm_hw_params_set_access(pcmCfg->handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        loge("Access type not available");
        return -1;
    }

    err = snd_pcm_hw_params_set_format(pcmCfg->handle, params, pcmCfg->format);
    if (err < 0) {
        loge("Sample format not available");
        return -1;
    }

    err = snd_pcm_hw_params_set_channels(pcmCfg->handle, params, pcmCfg->chnCnt);
    if (err < 0) {
        loge("Channels count not available");
        return -1;
    }

    rate = pcmCfg->sampleRate;
    err = snd_pcm_hw_params_set_rate_near(pcmCfg->handle, params, &pcmCfg->sampleRate, NULL);
    if (err < 0) {
        loge("set_rate_near error!");
        return -1;
    }
    if (rate != pcmCfg->sampleRate) {
        loge("required sample_rate %d is not supported, use %d instead", rate, pcmCfg->sampleRate);
    }

    snd_pcm_uframes_t periodSize = PERIOD_SIZE;
    snd_pcm_uframes_t prePeriodSize = periodSize;
    err = snd_pcm_hw_params_set_period_size_near(pcmCfg->handle, params, &periodSize, &dir);
    if (err < 0) {
        loge("set_period_size_near error!");
        return -1;
    }
    if(prePeriodSize != periodSize)
    {
        logw("Be careful! periodSize change:%lu->%lu", prePeriodSize, periodSize);
    }

    // double 1024-sample capacity -> 4
    snd_pcm_uframes_t bufferSize = periodSize * PERIOD_COUNT;
    snd_pcm_uframes_t preBufferSize = bufferSize;
    err = snd_pcm_hw_params_set_buffer_size_near(pcmCfg->handle, params, &bufferSize);
    if (err < 0) {
        loge("set_buffer_size_near error!");
        return -1;
    }
    if(preBufferSize != bufferSize)
    {
        logw("Be careful! bufferSize change:%lu->%lu", preBufferSize, bufferSize);
    }

    err = snd_pcm_hw_params(pcmCfg->handle, params);
    if (err < 0) {
        loge("Unable to install hw params");
        return -1;
    }

    snd_pcm_hw_params_get_period_size(params, &pcmCfg->chunkSize, 0);
    snd_pcm_hw_params_get_buffer_size(params, &pcmCfg->bufferSize);
    if (pcmCfg->chunkSize == pcmCfg->bufferSize) {
        loge("Can't use period equal to buffer size (%lu == %lu)", pcmCfg->chunkSize, pcmCfg->bufferSize);
        return -1;
    }

    pcmCfg->bitsPerSample = snd_pcm_format_physical_width(pcmCfg->format);
    pcmCfg->significantBitsPerSample = snd_pcm_format_width(pcmCfg->format);
    pcmCfg->bitsPerFrame = pcmCfg->bitsPerSample * pcmCfg->chnCnt;
    pcmCfg->chunkBytes = pcmCfg->chunkSize * pcmCfg->bitsPerFrame / 8;

    logv("----------------ALSA setting, pcm_stream:%d----------------", snd_pcm_stream(pcmCfg->handle));
    logv(">>Channels:   %4d, BitWidth:  %4d,phsical_w:%4d, SampRate:   %4d", pcmCfg->chnCnt, pcmCfg->significantBitsPerSample, pcmCfg->bitsPerSample,pcmCfg->sampleRate);
    logv(">>ChunkBytes: %4d, ChunkSize: %4d, BufferSize: %4d", pcmCfg->chunkBytes, (int)pcmCfg->chunkSize, (int)pcmCfg->bufferSize);

    /* SW params */
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(pcmCfg->handle, sw_params);
    if (snd_pcm_stream(pcmCfg->handle) == SND_PCM_STREAM_CAPTURE) {
        snd_pcm_sw_params_set_start_threshold(pcmCfg->handle, sw_params, 1);
    } else {
        snd_pcm_uframes_t boundary = 0;
        snd_pcm_sw_params_get_boundary(sw_params, &boundary);
//        snd_pcm_uframes_t silence_size = 0;
//        snd_pcm_sw_params_get_silence_size(sw_params, &silence_size);
//        snd_pcm_uframes_t silence_threshold = 0;
//        snd_pcm_sw_params_get_silence_threshold(sw_params, &silence_size);
        logv("SW play params get: boundary:0x%lx", boundary);
        snd_pcm_sw_params_set_start_threshold(pcmCfg->handle, sw_params, pcmCfg->chunkSize*PERIOD_COUNT);
        /* set silence size, in order to fill silence data into ringbuffer */
        snd_pcm_sw_params_set_silence_size(pcmCfg->handle, sw_params, boundary);
        logv("SW play params set: start_threshold:%ld, silence_size:0x%lx", pcmCfg->chunkSize, boundary);
    }
    snd_pcm_sw_params_set_stop_threshold(pcmCfg->handle, sw_params, pcmCfg->bufferSize);
    snd_pcm_sw_params_set_avail_min(pcmCfg->handle, sw_params, pcmCfg->chunkSize);
    err = snd_pcm_sw_params(pcmCfg->handle, sw_params);
    if (err < 0) {
        loge("Unable to install sw prams!");
        return err;
    }
    return 0;
}

int alsaOpenPcm(PCM_CONFIG_S *pcmCfg, const char *card, int pcmFlag)
{
    snd_pcm_info_t *info;
    snd_pcm_stream_t stream;
    int err;

    int open_mode = 0;

    if (pcmCfg->handle != NULL) {
        logw("PCM is opened already!");
        return 0;
    }
    logv("open pcm! card:[%s], pcmFlag:[%d](0-cap;1-play)", card, pcmFlag);

    // 0-cap; 1-play
    stream = (pcmFlag == 0) ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK;
    memset(pcmCfg->cardName, 0, sizeof(pcmCfg->cardName));
    strncpy(pcmCfg->cardName, card, sizeof(pcmCfg->cardName));

    snd_pcm_info_alloca(&info);

    // open_mode |= SND_PCM_NO_AUTO_RESAMPLE;  // not to used the auto resample
    err = snd_pcm_open(&pcmCfg->handle, card, stream, open_mode);
    if (err < 0) {
        loge("fatal error! card[%s] audio open error: %s", card, snd_strerror(err));
        char *pTestMem = malloc(8*1024);
        if(pTestMem)
        {
            free(pTestMem);
            pTestMem = NULL;
        }
        else
        {
            loge("fatal error! malloc fail, no memory now!");
        }
//        system("cat /proc/meminfo")
        return -1;
    }
    if ((err = snd_pcm_info(pcmCfg->handle, info)) < 0) {
        loge("snd_pcm_info error: %s", snd_strerror(err));
        return -1;
    }

    return 0;
}

void alsaClosePcm(PCM_CONFIG_S *pcmCfg, int pcmFlag)
{
    //loge("close pcm");

    if (pcmCfg->handle == NULL) {
        loge("PCM is not open yet!");
        return;
    }
    snd_pcm_close(pcmCfg->handle);
    pcmCfg->handle = NULL;
}

void alsaPreparePcm(PCM_CONFIG_S *pcmCfg)
{
    logv("prepare pcm");

    if (pcmCfg->handle == NULL) {
        loge("PCM is not open yet!");
        return;
    }
    snd_pcm_prepare(pcmCfg->handle);
}


ssize_t alsaReadPcm(PCM_CONFIG_S *pcmCfg, void *data, size_t rcount)
{
    ssize_t ret;
    ssize_t result = 0;
    int err = 0;
    char cardName[sizeof(pcmCfg->cardName)] = {0};

    if (rcount != pcmCfg->chunkSize)
        rcount = pcmCfg->chunkSize;

    if (pcmCfg == NULL || data == NULL) {
        loge("invalid input parameter(pcmCfg=%p, data=%p)!", pcmCfg, data);
        return -1;
    }

    while (rcount > 0) {
        /* if(0 == pcmCfg->snd_card_id) // bug fixing,consume too much time to excute shell command,when debugfs is not mounted.
        {
            UpdateDebugfsInfo();
        } */
        ret = snd_pcm_readi(pcmCfg->handle, data, rcount);
        if (ret == -EAGAIN || (ret >= 0 && (size_t)ret < rcount)) {
            snd_pcm_wait(pcmCfg->handle, 100);
        } else if (ret == -EPIPE) {
            logv("aec_alsa_overflow_xrun:%d-%lld-(%s)!", pcmCfg->snd_card_id, CDX_GetSysTimeUsMonotonic(), strerror(errno));
            snd_pcm_prepare(pcmCfg->handle);
            if(pcmCfg->read_pcm_aec)    // for aec condition,need to return directly and re-trigger cap dma again
            {
//                loge("aec_rtn_drtly");
//                return ret;
                logw("fatal error! read pcm aec meet EPIPE.");
            }
        } else if (ret == -ESTRPIPE) {
            logw("need recover(%s)!", strerror(errno));
            snd_pcm_recover(pcmCfg->handle, ret, 0);
        } else if (ret < 0) {
            logw("read error: %s", snd_strerror(ret));
            return -1;
        }

        if (ret > 0) {
            result += ret;
            rcount -= ret;
            data += ret * pcmCfg->bitsPerFrame / 8;
        }
    }

    return result;
}

ssize_t alsaWritePcm(PCM_CONFIG_S *pcmCfg, void *data, size_t wcount)
{
    ssize_t ret;
    ssize_t result = 0;
    int err = 0;
    char cardName[sizeof(pcmCfg->cardName)] = {0};

    if (snd_pcm_state(pcmCfg->handle) == SND_PCM_STATE_SUSPENDED) {
         while ((err = snd_pcm_resume(pcmCfg->handle)) == -EAGAIN) {
             loge("snd_pcm_resume again!");
             sleep(1);
        }
        switch(snd_pcm_state(pcmCfg->handle))
        {
            case SND_PCM_STATE_XRUN:
            {
                snd_pcm_drop(pcmCfg->handle);
                break;
            }
            case SND_PCM_STATE_SETUP:
                break;
            default:
            {
                logw("pcm_lib_state:%s",snd_pcm_state_name(snd_pcm_state(pcmCfg->handle)));
                snd_pcm_prepare(pcmCfg->handle);
                break;
            }
        }
    alsaSetPcmParams(pcmCfg);
    }

    while (wcount > 0) {
//        if(0 == pcmCfg->snd_card_id)
//        {
//            UpdateDebugfsInfo();
//        }
        if (snd_pcm_state(pcmCfg->handle) == SND_PCM_STATE_SETUP) {
            snd_pcm_prepare(pcmCfg->handle);
        }
        ret = snd_pcm_writei(pcmCfg->handle, data, wcount);
        if (ret == -EAGAIN || (ret >= 0 && (size_t)ret < wcount)) {
            snd_pcm_wait(pcmCfg->handle, 100);
        } else if (ret == -EPIPE) {
            //logv("xrun!");
            snd_pcm_prepare(pcmCfg->handle);
        } else if (ret == -EBADFD) {
            //logw("careful! current pcm state: %d", snd_pcm_state(pcmCfg->handle));
            snd_pcm_prepare(pcmCfg->handle);
        } else if (ret == -ESTRPIPE) {
            loge("need recover!");
            snd_pcm_recover(pcmCfg->handle, ret, 0);
        } else if (ret < 0) {
            loge("write error! ret:%d, %s", ret, snd_strerror(ret));
            //0-cap; 1-play
            alsaClosePcm(pcmCfg, 1);
            //FIXME: reopen
            loge("cardName:[%s], pcmFlag:[play]", pcmCfg->cardName);
            strncpy(cardName, pcmCfg->cardName, sizeof(pcmCfg->cardName));
            ret = alsaOpenPcm(pcmCfg, cardName, 1);
            if (ret < 0) {
                loge("alsaOpenPcm failed!");
                return ret;
            }
            ret = alsaSetPcmParams(pcmCfg);
            if (ret < 0) {
                loge("alsa SetPcmParams failed!");
                return ret;
            }
            if (pcmCfg->handle != NULL) {
                snd_pcm_reset(pcmCfg->handle);
                snd_pcm_prepare(pcmCfg->handle);
                snd_pcm_start(pcmCfg->handle);
            }
            loge("set pcm prepare finished!");
            return ret;
        }

        if (ret > 0) {
            result += ret;
            wcount -= ret;
            data += ret * pcmCfg->bitsPerFrame / 8;
        }
    }

    return result;
}

int alsaDrainPcm(PCM_CONFIG_S *pcmCfg)
{
    int err = 0;

    err = snd_pcm_drain(pcmCfg->handle);
    if (err != 0){
        loge("drain pcm err! err=%d", err);
    }

    return err;
}

int alsaOpenMixer(AIO_MIXER_S *mixer, const char *card)
{
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    int err = 0;

    if (mixer->handle != NULL) {
        return 0;
    }
    logv("open mixer:%s",card);

    snd_mixer_selem_id_alloca(&sid);

    err = snd_mixer_open(&mixer->handle, 0);
    if (err < 0) {
        loge("Mixer %s open error: %s\n", card, snd_strerror(err));
        return err;
    }

    err = snd_mixer_attach(mixer->handle, card);
    if (err < 0) {
        loge("Mixer %s attach error: %s\n", card, snd_strerror(err));
        goto ERROR;
    }

    err = snd_mixer_selem_register(mixer->handle, NULL, NULL);
    if (err < 0) {
        loge("Mixer %s register error: %s\n", card, snd_strerror(err));
        goto ERROR;
    }

    err = snd_mixer_load(mixer->handle);
    if (err < 0) {
        loge("Mixer %s load error: %s\n", card, snd_strerror(err));
        goto ERROR;
    }

    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem))
    {
        snd_mixer_selem_get_id(elem, sid);
        //snd_mixer_selem_set_playback_volume_range(elem, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);
        //snd_mixer_selem_set_capture_volume_range(elem, AUDIO_VOLUME_MIN, AUDIO_VOLUME_MAX);
        // open lineout and mic switch
        const char *elem_name = snd_mixer_selem_get_name(elem);
        logv("alsa_elem:%s",elem_name);

        if ( !strcmp(elem_name, AUDIO_ADC_MIC1_SWITCH) )
        {
            snd_mixer_selem_set_playback_switch(elem, 0, 1);
        }
        else if ( !strcmp(elem_name, AUDIO_ADC_MIC2_SWITCH) )
        {
            snd_mixer_selem_set_playback_switch(elem, 0, 0);// disable mic2 by default
        }
        else if(!strcmp(elem_name, AUDIO_LINEIN_SWITCH))
        {
            snd_mixer_selem_set_playback_switch(elem, 0, 0);
        }
        else if (!strcmp(elem_name, AUDIO_LINEOUT_VOL))
        {
            // lineout volume. 0x1f~0x02 : 0dB~-43.5dB, 1.5dB/step. 27 : -6dB.
            // user had better not change this ctrls, nor will cause wave distort!
            long vol_val = 27;
            snd_mixer_selem_set_playback_volume(elem, 0, vol_val);
            logv("set playback vol_val to value: %ld", vol_val);
        }
        else if (!strcmp(elem_name, AUDIO_LINEOUT_SOFT_VOL))
        {
            // AW_MPI_AO_SetSoftVolume() scope is :[-52, 50].
            // lineout soft volume. [0,255] : -26dB~25dB, we set 130. (25-(-26))/255 = 0.2dB
            long vol_val = 130;
            snd_mixer_selem_set_playback_volume(elem, 0, vol_val);
        }
        else if (!strcmp(elem_name, AUDIO_LINEOUT_SWITCH))
        {
            snd_mixer_selem_set_playback_switch(elem, 0, 1);
        }
        else if (!strcmp(elem_name, AUDIO_LINEOUT_MUX))
        {
            snd_mixer_selem_set_enum_item(elem, 0, 1);    // increase play volume when amplifier differential input
        }
        else if(!strcmp(elem_name, AUDIO_PA_SWITCH)){
            snd_mixer_selem_set_playback_switch(elem, 0, 1);
        }
    }


    return err;

ERROR:
    snd_mixer_close(mixer->handle);
    mixer->handle = NULL;
    return err;
}

void alsaCloseMixer(AIO_MIXER_S *mixer)
{
    if (mixer->handle == NULL) {
        return;
    }
    logv("close mixer[%p].card_id:%d", mixer, mixer->snd_card_id);

    snd_mixer_close(mixer->handle);
    mixer->handle = NULL;
}

int alsaMixerSetMicXEnable(AIO_MIXER_S *mixer, int nMicId, int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }
    char strMicXSwitch[32] = {'\0'};
    sprintf(strMicXSwitch, "MIC%d", nMicId); //query 'MIC1 Switch' get 'MIC1' here.
    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if(!strcmp(elem_name, strMicXSwitch)) //AUDIO_ADC_MIC2_SWITCH
        {
            loge("snd_card[%d]-mic%d_switch:%s-%d", mixer->snd_card_id, nMicId, elem_name, value);
            err = snd_mixer_selem_set_playback_switch(elem, 0, value);
            break;
        }
    }
    return err;
}

int alsaMixerSetLineInEnable(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_LINEIN_SWITCH))
        {
            loge("aec_elem_linein_switch:%s-%d",elem_name,value);
            err = snd_mixer_selem_set_playback_switch(elem, 0, value);
            break;
        }
    }
    return err;
}
int alsaMixerSetCapPlaySyncMode(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_CAP_PLAY_SYNC_MODE))
        {
            logv("aec_elem_sync_mode_switch:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);

            break;
        }
    }
    return err;
}
/* to set drc function of dac */
int alsaMixerSetAudioCodecDacDrc(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_DACDRC_EN))
        {
            logv("audio_codec_elem_dac_drc_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);

            break;
        }
    }
    return err;
}
/* to set drc function of adc */
int alsaMixerSetAudioCodecAdcDrc(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_ADCDRC_EN))
        {
            logv("audio_codec_elem_adc_drc_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);

            break;
        }
    }
    return err;
}

/* to set hpf function of dac */
int alsaMixerSetAudioCodecDacHpf(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_DACHPF_EN))
        {
            logv("audio_codec_elem_dac_hpf_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);

            break;
        }
    }
    return err;
}

/* to set hpf function of adc */
int alsaMixerSetAudioCodecAdcHpf(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_ADCHPF_EN))
        {
            logv("audio_codec_elem_adc_hpf_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);

            break;
        }
    }
    return err;
}

int alsaMixerSetAudioCodecHubMode(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, AUDIO_CODEC_HUB_MODE))
        {
            logv("aec_elem_audio_codec_hub_mode:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);

            break;
        }
    }
    return err;
}

int alsaMixerSetDAudio0HubMode(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, DAUDIo0_HUB_MODE))
        {
            logv("aec_elem_daudio0_hub_mode:%s-%d",elem_name,value);
            snd_mixer_selem_set_enum_item(elem, 0, value);

            break;
        }
    }
    return err;
}

int alsaMixerSetDAudio0LoopBackEn(AIO_MIXER_S *mixer,  int value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);

       if(!strcmp(elem_name, DAUDIo0_LOOPBACK_EN))
        {
            logv("aec_elem_daudio0_loopback_en:%s-%d",elem_name,value);
            snd_mixer_selem_set_playback_switch(elem, 0, value);

            break;
        }
    }
    return err;
}

int alsaMixerSetVolume(AIO_MIXER_S *mixer, int playFlag, long value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }
    if (value < 0 || value > 100) {
        loge("want to setAIOVol[0,100], playFlag[%d], but usr value=%ld is invalid!", playFlag, value);
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_LINEOUT_VOL)) {
            long realVol = value*AUDIO_VOLUME_MAX/100;
            err = snd_mixer_selem_set_playback_volume(elem, 0, realVol);
            logv("snd_card:%d, elem_name:%s, playback setVolume:%ld, err:%d", mixer->snd_card_id, elem_name, realVol, err);
            break;
        }
        else if(!playFlag && (!strcmp(elem_name, AUDIO_MIC1_MAIN_GAIN) || !strcmp(elem_name, AUDIO_MIC2_MAIN_GAIN)))
        {
            long realVol = value*AUDIO_VOLUME_MAX/100;
            err = snd_mixer_selem_set_playback_volume(elem, 0, realVol);
            logv("snd_card:%d, elem_name:%s, set_ai_main_gain:%ld", mixer->snd_card_id, elem_name, realVol);
        }
    }
    return err;
}

int alsaMixerGetVolume(AIO_MIXER_S *mixer, int playFlag, long *value)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_LINEOUT_VOL)) {
            long realVal;
            err = snd_mixer_selem_get_playback_volume(elem, 0, &realVal);
            // scale from AUDIO_VOLUME_MAX to 100
            *value = realVal * 100 / AUDIO_VOLUME_MAX;
            loge("playback getVolume:%ld, dst:%ld, err:%d", realVal, *value, err);
            break;
        }
        else if(!playFlag && !strcmp(elem_name, AUDIO_MIC1_MAIN_GAIN))
        {
            long realVal;
            err = snd_mixer_selem_get_playback_volume(elem, 0, &realVal);

            // scale from AUDIO_VOLUME_MAX to 100
            *value = realVal * 100 / AUDIO_VOLUME_MAX;
            loge("get_ai_main_gain:%ld, dst:%ld, err:%d", realVal, *value, err);
        }
    }
    return err;
}

int alsaMixerSetSoftVolume(AIO_MIXER_S *mixer, int playFlag, long value)
{
    int err = 0;

    if (mixer->handle == NULL)
    {
        loge("fatal error! mixer handle is NULL");
        return -1;
    }
    if(!playFlag)
    {
        loge("fatal error! softVolume do not support capture");
        return -1;
    }
    if (value < AUDIO_SOFT_VOLUME_MPP_SCOPE_MIN || value > AUDIO_SOFT_VOLUME_MPP_SCOPE_MAX)
    {
        loge("want to setAIOSoftVol[%d,%d], playFlag[%d], but usr value=%ld is invalid!",
            AUDIO_SOFT_VOLUME_MPP_SCOPE_MIN, AUDIO_SOFT_VOLUME_MPP_SCOPE_MAX, playFlag, value);
        return -1;
    }

    snd_mixer_elem_t *elem;
    err = -1;
    int bFindMixerElemFlag = 0;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem))
    {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_LINEOUT_SOFT_VOL))
        {
            bFindMixerElemFlag = 1;
            int nSoftVolValue = value;
            double Vol = (double)(nSoftVolValue-AUDIO_SOFT_VOLUME_MPP_SCOPE_MIN)/(AUDIO_SOFT_VOLUME_MPP_SCOPE_MAX-AUDIO_SOFT_VOLUME_MPP_SCOPE_MIN)
                *(AUDIO_SOFT_VOLUME_MAX-AUDIO_SOFT_VOLUME_MIN) + AUDIO_SOFT_VOLUME_MIN;
            long realVol = floor(Vol);
            err = snd_mixer_selem_set_playback_volume(elem, 0, realVol);
            loge("playback setSoftVolume:%ld(%lf-%ld), err:%d", realVol, Vol, value, err);
            break;
        }
    }
    if(0 == bFindMixerElemFlag)
    {
        loge("fatal error! can not find mixer_elem:[%s]", AUDIO_LINEOUT_SOFT_VOL);
    }
    return err;
}

int alsaMixerGetSoftVolume(AIO_MIXER_S *mixer, int playFlag, long *value)
{
    int err = 0;

    if (mixer->handle == NULL)
    {
        loge("fatal error! mixer handle is NULL");
        return -1;
    }
    if(!playFlag)
    {
        loge("fatal error! softVolume do not support capture");
        return -1;
    }

    snd_mixer_elem_t *elem;
    err = -1;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem))
    {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_LINEOUT_SOFT_VOL))
        {
            long realVal;
            err = snd_mixer_selem_get_playback_volume(elem, 0, &realVal);
            int nSoftVolValue = (AUDIO_SOFT_VOLUME_MPP_SCOPE_MAX-AUDIO_SOFT_VOLUME_MPP_SCOPE_MIN)*(realVal-AUDIO_SOFT_VOLUME_MIN)
                /(AUDIO_SOFT_VOLUME_MAX-AUDIO_SOFT_VOLUME_MIN) + AUDIO_SOFT_VOLUME_MPP_SCOPE_MIN;
            *value = nSoftVolValue;
            loge("playback getSoftVolume:%ld(%ld), err:%d", realVal, *value, err);
            break;
        }
    }
    return err;
}

int alsaMixerSetMute(AIO_MIXER_S *mixer, int playFlag, int bEnable)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_PA_SWITCH/*AUDIO_LINEOUT_SWITCH*/)) {
            loge("set player master-volume switch state: %d", bEnable);
            if (bEnable) {
                err = snd_mixer_selem_set_playback_switch(elem, 0, 0);
            } else {
                err = snd_mixer_selem_set_playback_switch(elem, 0, 1);
            }
            break;
        }
    }
    return err;
}

int alsaMixerGetMute(AIO_MIXER_S *mixer, int playFlag, int *pVolVal)
{
    int err = 0;

    if (mixer->handle == NULL) {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem)) {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (playFlag && !strcmp(elem_name, AUDIO_PA_SWITCH)) {
            err = snd_mixer_selem_get_playback_switch(elem, 0, pVolVal);
            loge("get master-volume (0-mute; 1-unmute) switch state: %d", *pVolVal);
            break;
        }
    }
    return err;
}

int alsaMixerSetPlayBackPA(AIO_MIXER_S *mixer, int bHighLevel)
{
    int err = 0;

    if (mixer->handle == NULL)
    {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem))
    {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (!strcmp(elem_name, AUDIO_PA_SWITCH))
        {
            int ival;
            bHighLevel = bHighLevel?1:0;
            if(snd_mixer_selem_has_playback_switch(elem))
            {
                snd_mixer_selem_get_playback_switch(elem, 0, &ival);
                if (snd_mixer_selem_set_playback_switch(elem, 0, bHighLevel) >= 0)
                {
                }
                else
                {
                    loge("fatal error! set value of playback switch control of a mixer simple element fail[%d]!", err);
                }
            }
            else
            {
                loge("fatal error! playback switch control is not present!");
            }
            break;
        }
    }
    return err;
}

int alsaMixerGetPlayBackPA(AIO_MIXER_S *mixer, int *pbHighLevel)
{
    int err = 0;

    if (mixer->handle == NULL)
    {
        return -1;
    }

    snd_mixer_elem_t *elem;
    for (elem = snd_mixer_first_elem(mixer->handle); elem; elem = snd_mixer_elem_next(elem))
    {
        const char *elem_name = snd_mixer_selem_get_name(elem);
        if (!strcmp(elem_name, AUDIO_PA_SWITCH))
        {
            err = snd_mixer_selem_get_playback_switch(elem, 0, pbHighLevel);
            if(err!=0)
            {
                loge("fatal error! get player pa wrong[0x%x]", err);
            }
            break;
        }
    }
    return err;
}
