/** @file */
#ifndef __ALSA_COMMON_H__
#define __ALSA_COMMON_H__

#include <alsa/asoundlib.h>
#include <string.h>

#define AUDIO_VOLUME_MIN    (0)
#define AUDIO_VOLUME_MAX    (31)
#define AUDIO_SOFT_VOLUME_MIN    (0)
#define AUDIO_SOFT_VOLUME_MAX    (255) ///< ref to asound.conf, pcm.PlaybackRateDmix, resolution 256. 256 stand for 256 points, so [0,255]
#define AUDIO_SOFT_VOLUME_MPP_SCOPE_MIN (-52) ///< @see ref to AW_MPI_AO_SetSoftVolume(AUDIO_DEV AudioDevId, int s32Volume), must equal to scope of s32Volume.
#define AUDIO_SOFT_VOLUME_MPP_SCOPE_MAX (50) ///< @see AUDIO_SOFT_VOLUME_MPP_SCOPE_MIN
#define AUDIO_LEFT_INPUT_MIC1 "Left Input Mixer MIC1 Boost"
#define AUDIO_LEFT_INPUT_MIC2 "Left Input Mixer MIC2 Boost"
#define AUDIO_RIGHT_INPUT_MIC1 "Right Input Mixer MIC1 Boost"
#define AUDIO_RIGHT_INPUT_MIC2 "Right Input Mixer MIC2 Boost"
#define HEAD_PHONE "Headphone"
#define HEAD_PHONE_VOL "Headphone volume"
#define DIGITAL_VOL "digital volume"
#define HPOUT_VOL "HPOUT volume"

// for v853
// about lineout
#define AUDIO_LINEOUT_SWITCH    "LINEOUT"
#define AUDIO_LINEOUT_MUX       "LINEOUT Output Select"
#define AUDIO_LINEOUT_VOL       "LINEOUT volume"
#define AUDIO_LINEOUT_SOFT_VOL  "Soft Volume Master"
#define AUDIO_PA_SWITCH         "SPK"

// about mic
#define AUDIO_ADC_MIC1_SWITCH   "MIC1"
#define AUDIO_ADC_MIC2_SWITCH   "MIC2"
#define AUDIO_ADC_MIC1_SELECT   "MIC1 Input Select"
#define AUDIO_ADC_MIC2_SELECT   "MIC2 Input Select"
#define AUDIO_MIC1_MAIN_GAIN    "MIC1 gain volume"
#define AUDIO_MIC2_MAIN_GAIN    "MIC2 gain volume"

// about linein
#define AUDIO_LINEIN_SWITCH        "LINEIN"
#define AUDIO_LINEIN_L_GAIN        "LINEINL gain volume"
#define AUDIO_LINEIN_R_GAIN        "LINEINR gain volume"

    // v459 aec to enable cap&play sync mode for aec feature
#define AUDIO_CAP_PLAY_SYNC_MODE "rx sync mode"

#define AUDIO_CODEC_HUB_MODE "tx hub mode"

#define DAUDIo0_HUB_MODE   "tx hub mode"
#define DAUDIo0_LOOPBACK_EN "loopback debug"

#define AUDIO_DACDRC_EN   "DACDRC"
#define AUDIO_ADCDRC_EN   "ADCDRC"

#define AUDIO_DACHPF_EN   "DACHPF"
#define AUDIO_ADCHPF_EN   "ADCHPF"

typedef struct PCM_CONFIG_S
{
    snd_pcm_t *handle;
    char cardName[128]; ///< pcm handle identifier. e.g., PlaybackRateDmix:16000,1,960

    snd_pcm_format_t format;
    unsigned int chnCnt; //underlying channel number of alsaLib, including refChn if aec is enable.
    unsigned int sampleRate;

    snd_pcm_uframes_t bufferSize;   //unit: sample number
    snd_pcm_uframes_t chunkSize;    //unit: sample number, we take it as period_size. 0:use default value.

    unsigned int bitsPerSample;
    unsigned int significantBitsPerSample;
    unsigned int bitsPerFrame;
    unsigned int chunkBytes;    //unit: bytes
    int aec_delay_ms;

    int snd_card_id;
    int read_pcm_aec;             // used to indicate aec condition or normal one when read pcm. 1:aec, 0:normal
} PCM_CONFIG_S;

typedef struct AIO_MIXER_S {
    snd_mixer_t *handle;
    snd_mixer_elem_t *elem;

    int snd_card_id;
} AIO_MIXER_S;

int alsaSetPcmParams(PCM_CONFIG_S *pcmCfg);
int alsaOpenPcm(PCM_CONFIG_S *pcmCfg, const char *card, int pcmFlag);
void alsaClosePcm(PCM_CONFIG_S *pcmCfg, int pcmFlag);
void alsaPreparePcm(PCM_CONFIG_S *pcmCfg);

ssize_t alsaReadPcm(PCM_CONFIG_S *pcmCfg, void *data, size_t rcount);
ssize_t alsaWritePcm(PCM_CONFIG_S *pcmCfg, void *data, size_t wcount);
int alsaDrainPcm(PCM_CONFIG_S *pcmCfg);
int alsaOpenMixer(AIO_MIXER_S *mixer, const char *card);
void alsaCloseMixer(AIO_MIXER_S *mixer);
int alsaMixerSetCapPlaySyncMode(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetAudioCodecHubMode(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetDAudio0HubMode(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetDAudio0LoopBackEn(AIO_MIXER_S *mixer,  int value);

int alsaMixerSetAudioCodecDacDrc(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetAudioCodecAdcDrc(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetAudioCodecDacHpf(AIO_MIXER_S *mixer,  int value);
int alsaMixerSetAudioCodecAdcHpf(AIO_MIXER_S *mixer,  int value);

int alsaMixerSetVolume(AIO_MIXER_S *mixer, int playFlag, long value);
int alsaMixerGetVolume(AIO_MIXER_S *mixer, int playFlag, long *value);
int alsaMixerSetSoftVolume(AIO_MIXER_S *mixer, int playFlag, long value);
int alsaMixerGetSoftVolume(AIO_MIXER_S *mixer, int playFlag, long *value);
int alsaMixerSetMute(AIO_MIXER_S *mixer, int playFlag, int bEnable);
int alsaMixerGetMute(AIO_MIXER_S *mixer, int playFlag, int *pVolVal);
void updateDebugfsByChnCnt(int pcmFlag, int cnt);

int alsaGetDelay(PCM_CONFIG_S *pcmCfg);
int alsaMixerSetPlayBackPA(AIO_MIXER_S *mixer, int bHighLevel);
int alsaMixerGetPlayBackPA(AIO_MIXER_S *mixer, int *pbHighLevel);

int alsaMixerSetMicXEnable(AIO_MIXER_S *mixer, int nMicId, int value);
int alsaMixerSetLineInEnable(AIO_MIXER_S *mixer,  int value);
#endif /* __ALSA_COMMON_H__ */
