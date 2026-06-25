//#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_VERBOSE
#include <string.h>
#include <pthread.h>

#include "../utils/sys/include/sys_linux_ioctl.h"

#if ENABLE_AEC
#include "include/uac_aec.h"
#endif
#if ENABLE_ANS
#include "include/uac_ans.h"
#endif
#if ENABLE_AGC
#include "include/uac_agc.h"
#endif
#if VIDEO_SOURCE_RT_MEDIA
#include "include/alsa_interface.h"
#else
#include "media/include/audio/alsa_interface.h"
#endif
#include "include/uac.h"
#include "../utils/debug/include/debug.h"

//#define DEBUG_SAVE_UAC_CAPTURE_PCM
#ifdef DEBUG_SAVE_UAC_CAPTURE_PCM
#define DEBUG_SAVE_UAC_CAPTURE_PCM_FILE     "/tmp/save_uac_capture.pcm"
#endif
//#define DEBUG_SAVE_CAPTURE_PCM
#ifdef DEBUG_SAVE_CAPTURE_PCM
#define DEBUG_SAVE_CAPTURE_PCM_FILE         "/tmp/save_capture.pcm"
#endif

#define UAC1_PLAYBACK_STATE_NODE                 "/sys/class/usb_sunxi_f_uac1/uac1_playback_state"
#define UAC1_CAPTURE_STATE_NODE                  "/sys/class/usb_sunxi_f_uac1/uac1_capture_state"

typedef struct uac_context
{
    AIO_MIXER_S codec_mixer;
    AIO_MIXER_S daudio_mixer;
    PCM_CONFIG_S daudio_pcm_config;

    pthread_t uac_in_task_trd;
    int uac_in_task_running;

    pthread_t uac_out_task_trd;
    int uac_out_task_running;

    int exit_flag;
    uvc_demo_config config;
}uac_context;
static uac_context g_uac_context;

static int check_uac1_state(char *state_node)
{
    char read_value[10];
    int fd = open(state_node, O_RDONLY);
    if (fd < 0)
        return -1;
    read(fd, &read_value, sizeof(read_value));
    close(fd);
    return atoi(read_value);
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
            loge("unsupport bits_per_sample %d use default SND_PCM_FORMAT_S16_LE", bits_per_sample);
            return SND_PCM_FORMAT_S16_LE;
    }
}

/**
  In host perspective, so uac_out means play pcm data in device.
*/
static void *uac1_out_task_proc(void *thread_data)
{
    int ret;
    PCM_CONFIG_S capture_pcm_config;
    PCM_CONFIG_S playback_pcm_config;
    char *capture_data = NULL;
    int capture_data_len;
    int frame_len, write_len;
    uac_context *context = (uac_context *)thread_data;

    context->uac_out_task_running = 1;
    memset(&capture_pcm_config, 0, sizeof(PCM_CONFIG_S));
    capture_pcm_config.chnCnt = context->config.uac_channel;
    capture_pcm_config.sampleRate = context->config.uac_sample_rate;
    capture_pcm_config.format = map_alsa_format(context->config.uac_bitwidth);
    capture_pcm_config.bitsPerSample = context->config.uac_bitwidth;
    ret = alsaOpenPcm(&capture_pcm_config, SOUND_CARD_UAC, 0);
    if (ret)
    {
        loge("fatal error! open sound card %s fail!", SOUND_CARD_UAC);
        goto _exit;
    }
    ret = alsaSetPcmParams(&capture_pcm_config);
    if (ret)
    {
        goto _close_uac1_card;
    }
    alsaPreparePcm(&capture_pcm_config);
    capture_data_len = capture_pcm_config.chunkBytes;
    capture_data = malloc(capture_data_len);
    if (!capture_data)
        loge("malloc capture buffer fail!");

    memset(&playback_pcm_config, 0, sizeof(PCM_CONFIG_S));
    playback_pcm_config.chnCnt = context->config.uac_channel;
    playback_pcm_config.sampleRate = context->config.uac_sample_rate;
    playback_pcm_config.format = map_alsa_format(context->config.uac_bitwidth);
    playback_pcm_config.bitsPerSample = context->config.uac_bitwidth;
    ret = alsaOpenPcm(&playback_pcm_config, SOUND_CARD_PLAYBACKRATEDMIX, 1);
    if (ret)
    {
        loge("fatal error! open sound card %s fail!", SOUND_CARD_PLAYBACKRATEDMIX);
        goto _close_uac1_card;
    }
    ret = alsaSetPcmParams(&playback_pcm_config);
    if (ret)
    {
        goto _close_uac1_card;
    }
    alsaPreparePcm(&playback_pcm_config);
    alsaMixerSetVolume(&context->codec_mixer, 1, DEFAULT_PLAYBACK_VOLUME);

#ifdef DEBUG_SAVE_UAC_CAPTURE_PCM
    FILE *fp = fopen(DEBUG_SAVE_UAC_CAPTURE_PCM_FILE, "wb");
#endif

    while (1)
    {
        if (context->exit_flag)
            break;

        if (!check_uac1_state(UAC1_CAPTURE_STATE_NODE)) {
            usleep(20*1000);
            continue;
        }

        ret = alsaReadPcm(&capture_pcm_config, capture_data, capture_pcm_config.chunkSize);
        if (ret != capture_pcm_config.chunkSize)
        {
            //usleep(20*1000);
            continue;
        }
        frame_len = capture_data_len;
#ifdef DEBUG_SAVE_UAC_CAPTURE_PCM
        if (fp)
            fwrite(capture_data, capture_data_len, 1, fp);
#endif
        write_len = frame_len / (context->config.uac_bitwidth / 8);
        alsaWritePcm(&playback_pcm_config, capture_data, write_len);
    }
#ifdef DEBUG_SAVE_UAC_CAPTURE_PCM
    if (fp)
        fclose(fp);
#endif
    alsaDrainPcm(&playback_pcm_config);
    alsaClosePcm(&playback_pcm_config, 1);
_close_uac1_card:
    if (capture_data)
    {
        free(capture_data);
        capture_data = NULL;
    }
    alsaClosePcm(&capture_pcm_config, 0);
_exit:
    return (void *)NULL;
}

/**
  In host perspective, so uac_in means capture pcm data in device, so need aec.
*/
static void *uac1_in_task_proc(void *thread_data)
{
    int ret;
    PCM_CONFIG_S playback_pcm_config;
    PCM_CONFIG_S capture_pcm_config;
    int write_len;
    char sound_card[128] = {0};
    char *capture_data = NULL;
    int capture_data_len, frame_len;
#if ENABLE_AEC
    WebRtcAecContext *aec_context = NULL;
#endif
#if ENABLE_ANS
    WebRtcAnsContext *ans_context = NULL;
#endif
#if ENABLE_AGC
    WebRtcAgcContext *agc_context = NULL;
#endif
    uac_context *context = (uac_context *)thread_data;

#if ENABLE_AEC
    //alsaMixerSetAudioCodecHubMode(&context->codec_mixer,1);
    //alsaMixerSetDAudio0HubMode(&context->daudio_mixer,1);
    //alsaMixerSetDAudio0LoopBackEn(&context->daudio_mixer,1);
    if (context->config.uac_aec)
    {
        alsaMixerSetCapPlaySyncMode(&context->codec_mixer, 1);
        alsaMixerSetCapPlaySyncMode(&context->daudio_mixer, 1);
    }
    else
    {
        alsaMixerSetCapPlaySyncMode(&context->codec_mixer, 0);
        alsaMixerSetCapPlaySyncMode(&context->daudio_mixer, 0);
    }
#endif

    context->uac_in_task_running = 1;
    memset(&playback_pcm_config, 0, sizeof(PCM_CONFIG_S));
    playback_pcm_config.chnCnt = context->config.uac_channel;
    playback_pcm_config.sampleRate = context->config.uac_sample_rate;
    playback_pcm_config.format = map_alsa_format(context->config.uac_bitwidth);
    playback_pcm_config.bitsPerSample = context->config.uac_bitwidth;
    ret = alsaOpenPcm(&playback_pcm_config, SOUND_CARD_UAC, 1);
    if (ret)
    {
        loge("fatal error! open sound card %s fail!", SOUND_CARD_UAC);
        return (void *)NULL;
    }
    ret = alsaSetPcmParams(&playback_pcm_config);
    if (ret)
    {
        goto _close_uac1_card;
    }
    alsaPreparePcm(&playback_pcm_config);

    memset(&capture_pcm_config, 0, sizeof(PCM_CONFIG_S));
    if (context->config.uac_aec)
    {
#if ENABLE_AEC
        sprintf(sound_card, "%s", SOUND_CARD_CAPTURE1MICPLUSAEC);
        capture_pcm_config.chnCnt = context->config.uac_channel + 1; //need ref chn
#else
        logw("CONFIG_mpp_aec_libwebrtc is not set!");
        sprintf(sound_card, "%s:%d", SOUND_CARD_MIC, context->config.uac_sample_rate);
        capture_pcm_config.chnCnt = context->config.uac_channel;
#endif
    }
    else
    {
        sprintf(sound_card, "%s:%d", SOUND_CARD_MIC, context->config.uac_sample_rate);
        capture_pcm_config.chnCnt = context->config.uac_channel;
    }
    capture_pcm_config.sampleRate = context->config.uac_sample_rate;
    capture_pcm_config.format = map_alsa_format(context->config.uac_bitwidth);
    capture_pcm_config.bitsPerSample = context->config.uac_bitwidth;
    ret = alsaOpenPcm(&capture_pcm_config, sound_card, 0);
    if (ret)
    {
        loge("fatal error! open sound card %s fail!", sound_card);
        goto _close_uac1_card;
    }
    ret = alsaSetPcmParams(&capture_pcm_config);
    if (ret)
    {
        goto _close_capture_card;
    }
    alsaPreparePcm(&capture_pcm_config);
    capture_data_len = capture_pcm_config.chunkBytes;
    capture_data = malloc(capture_data_len);
    if (!capture_data)
        loge("malloc capture data len %d fail!", capture_pcm_config.chunkBytes);
#if ENABLE_AEC
    if (context->config.uac_aec)
    {
        /* samplerate,  mode, delayms, chunkbytes*/
        aec_context = ConstructWebRtcAecContext(capture_pcm_config.sampleRate, 1, 0, capture_pcm_config.chunkBytes);
        if (!aec_context)
            goto _exit;
    }
#endif
#if ENABLE_ANS
    if (context->config.uac_ans)
    {
        ans_context = ConstructWebRtcAnsContext();
        if (!ans_context)
            goto _exit;
    }
#endif
#if ENABLE_AGC
    if (context->config.uac_agc)
    {
        agc_context = ConstructWebRtcAgcContext(capture_pcm_config.sampleRate,
            capture_pcm_config.chnCnt, capture_pcm_config.chunkBytes, DEFAULT_AGC_FLOAT_TARGETDB, DEFAULT_AGC_FLOAT_MAXGAINDB);
        if (!agc_context)
            goto _exit;
    }
#endif
#ifdef DEBUG_SAVE_CAPTURE_PCM
    FILE *fp = fopen(DEBUG_SAVE_CAPTURE_PCM_FILE, "wb");
#endif
    while (1)
    {
        if (context->exit_flag)
            break;

        ret = alsaReadPcm(&capture_pcm_config, capture_data, capture_data_len);
        if (ret != capture_pcm_config.chunkSize)
        {
            //usleep(20*1000);
            continue;
        }
        frame_len = capture_data_len;
#if ENABLE_AEC
        if (context->config.uac_aec)
        {
            UAC_AEC_FRAME_S uac_aec_frame;
            memset(&uac_aec_frame, 0, sizeof(UAC_AEC_FRAME_S));
            uac_aec_frame.mSamplerate = context->config.uac_sample_rate;
            uac_aec_frame.mBitwidth = context->config.uac_bitwidth;
            uac_aec_frame.mLen = capture_data_len;
            uac_aec_frame.mpAddr = capture_data;
            ret = WebRtcAecProcess(aec_context, &uac_aec_frame, 0);
            if (ret != 0)
                continue;
            frame_len = uac_aec_frame.mLen;
        }
#endif
#if ENABLE_ANS
        if (context->config.uac_ans)
        {
            UAC_ANS_FRAME_S uac_ans_frame;
            memset(&uac_ans_frame, 0, sizeof(UAC_ANS_FRAME_S));
            uac_ans_frame.mSamplerate = context->config.uac_sample_rate;
            uac_ans_frame.mBitwidth = context->config.uac_bitwidth;
            uac_ans_frame.mLen = frame_len;
            uac_ans_frame.mpAddr = capture_data;
            ret = WebRtcAnsProcess(ans_context, &uac_ans_frame, capture_pcm_config.sampleRate, capture_pcm_config.chunkSize, DEFAULT_ANS_MODE, 0);
            if (ret != 0)
                continue;
            frame_len = uac_ans_frame.mLen;
        }
#endif
#if ENABLE_AGC
        if (context->config.uac_agc)
        {
            UAC_AGC_FRAME_S uac_agc_frame;
            memset(&uac_agc_frame, 0, sizeof(UAC_AGC_FRAME_S));
            uac_agc_frame.mSamplerate = context->config.uac_sample_rate;
            uac_agc_frame.mBitwidth = context->config.uac_bitwidth;
            uac_agc_frame.mLen = frame_len;
            uac_agc_frame.mpAddr = capture_data;
            ret = WebRtcAgcProcess(agc_context, &uac_agc_frame);
            if (ret)
                continue;
        }
#endif
#ifdef DEBUG_SAVE_CAPTURE_PCM
        if (fp)
            fwrite(capture_data, frame_len, 1, fp);
#endif
        if (check_uac1_state(UAC1_PLAYBACK_STATE_NODE)) {
            write_len = frame_len / (context->config.uac_bitwidth / 8);
            alsaWritePcm(&playback_pcm_config, capture_data, write_len);
        }
    }
#ifdef DEBUG_SAVE_CAPTURE_PCM
    if (fp)
        fclose(fp);
#endif
_exit:
#if ENABLE_AGC
    if (context->config.uac_agc)
    {
        if (agc_context)
            DestructWebRtcAgcContext(agc_context);
    }
#endif
_destroy_ans:
#if ENABLE_ANS
    if (context->config.uac_ans)
    {
        if (ans_context)
            DestructWebRtcAnsContext(ans_context);
    }
#endif
_destroy_aec:
#if ENABLE_AEC
    if (context->config.uac_aec)
    {
        if (aec_context)
            DestructWebRtcAecContext(aec_context);
    }
#endif
    if (capture_data)
    {
        free(capture_data);
        capture_data = NULL;
    }
_close_capture_card:
    alsaClosePcm(&capture_pcm_config, 0);
_close_uac1_card:
    alsaDrainPcm(&playback_pcm_config);
    alsaClosePcm(&playback_pcm_config, 1);
    return (void *)NULL;
}

static int open_daudio_sound_card(uac_context *context)
{
    int ret;

    memset(&context->daudio_pcm_config, 0, sizeof(PCM_CONFIG_S));
    context->daudio_pcm_config.chnCnt = 1;
    context->daudio_pcm_config.sampleRate = context->config.uac_sample_rate;
    context->daudio_pcm_config.format = map_alsa_format(context->config.uac_bitwidth);
    context->daudio_pcm_config.bitsPerSample = context->config.uac_bitwidth;
    ret = alsaOpenPcm(&context->daudio_pcm_config, SOUND_CARD_SNDDAUDIO0, 1);
    if (ret)
    {
        loge("fatal error! open sound card %s fail!", SOUND_CARD_SNDDAUDIO0);
        goto _exit;
    }
    ret = alsaSetPcmParams(&context->daudio_pcm_config);
    if (ret)
    {
        loge("sound card %s set param fail!", SOUND_CARD_SNDDAUDIO0);
    }
    alsaPreparePcm(&context->daudio_pcm_config);
_exit:
    return ret;
}

static int close_daudio_sound_card(uac_context *context)
{
    alsaClosePcm(&context->daudio_pcm_config, 1);
}

static int init_mixer(uac_context *context)
{
    int ret;

    ret = open_daudio_sound_card(context);
    if (ret)
        return ret;

    ret = alsaOpenMixer(&context->codec_mixer, SOUND_MIXER_AUDIOCODEC);
    if (ret)
    {
        loge("open mixer %s fail!", SOUND_MIXER_AUDIOCODEC);
        return ret;
    }

    ret = alsaOpenMixer(&context->daudio_mixer, SOUND_MIXER_SNDDAUDIO0);
    if (ret)
    {
        loge("open mixer %s fail!", SOUND_MIXER_SNDDAUDIO0);
        return ret;
    }

    return ret;
}

static int deinit_mixer(uac_context *context)
{
    if (context->daudio_mixer.handle)
        alsaCloseMixer(&context->daudio_mixer);
    if (context->codec_mixer.handle)
        alsaCloseMixer(&context->codec_mixer);
    close_daudio_sound_card(context);
}


int check_audio_param(uvc_demo_config *config)
{
    if ((config->uac_sample_rate <= 0) || (config->uac_bitwidth <= 0)
        || ((config->uac_channel <= 0) || (config->uac_channel > 2)))
    {
        loge("invalid param! sample_rate %d channel %d bitwidth %d",
            config->uac_sample_rate, config->uac_channel, config->uac_bitwidth);
        return -1;
    }
    return 0;
}

int uac_enable(uvc_demo_config * config)
{
    memset(&g_uac_context, 0, sizeof(g_uac_context));
    memcpy(&g_uac_context.config, config, sizeof(uvc_demo_config));
    if (check_audio_param(&g_uac_context.config))
        return -1;

    if (init_mixer(&g_uac_context))
        return -1;

    if (g_uac_context.config.uac_in)
    {
        pthread_create(&g_uac_context.uac_in_task_trd, NULL, uac1_in_task_proc, (void *)&g_uac_context);
    }
    if (g_uac_context.config.uac_out)
    {
        pthread_create(&g_uac_context.uac_out_task_trd, NULL, uac1_out_task_proc, (void *)&g_uac_context);
    }
    return 0;
}

int uac_disable(void)
{
    g_uac_context.exit_flag = 1;
    if (g_uac_context.config.uac_in && g_uac_context.uac_in_task_running)
        pthread_join(g_uac_context.uac_in_task_trd, NULL);
    if (g_uac_context.config.uac_out && g_uac_context.uac_out_task_running)
        pthread_join(g_uac_context.uac_out_task_trd, NULL);
    deinit_mixer(&g_uac_context);
    return 0;
}
