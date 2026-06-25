#ifndef __AW_SNDCARD_H__
#define __AW_SNDCARD_H__
#include <alsa/asoundlib.h>
#include <pthread.h>

#include "doorlock_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct snd_alsa_config_s {
    snd_pcm_t *snd_handle;              // 调用snd_pcm_open打开PCM设备返回的文件句柄
    snd_pcm_hw_params_t *snd_hwparams;  // 设置流的硬件参数
    snd_pcm_sw_params_t *snd_swparams;
    snd_pcm_uframes_t period_size;
    int periods;
    int buffer_size;
    int bytes_per_frame;
} snd_alsa_config_t;

typedef struct snd_card_s {
    char snd_devname[256];
    int snd_card_fd;

    uint8_t spk;
    uint8_t mic;
    uint8_t mic_is_streaming;
    uint8_t spk_is_streaming;

    uint32_t mic_bitwidth;
    uint32_t mic_channel_cnt;
    uint32_t mic_sample_rate;

    uint32_t spk_bitwidth;
    uint32_t spk_channel_cnt;
    uint32_t spk_sample_rate;

    snd_alsa_config_t mic_alsa_cfg;
    snd_alsa_config_t spk_alsa_cfg;

} snd_card_t;

int snd_card_write_spk_frame(snd_card_t *snd_card_info, uint8_t *data_buf, uint32_t data_size);
int snd_card_read_mic_frame(snd_card_t *snd_card_info, uint8_t *data_buf, uint32_t data_size);
int snd_card_init(snd_card_t *snd_card_info);
int snd_card_deinit(snd_card_t *snd_card_info);
int snd_card_open_mic(snd_card_t *snd_card_info);
int snd_card_close_mic(snd_card_t *snd_card_info);
int snd_card_open_spk(snd_card_t *snd_card_info);
int snd_card_close_spk(snd_card_t *snd_card_info);
int snd_card_find(char *dev_name);
int snd_card_set_mic_volume(snd_card_t *snd_card_info, int volume_percent);
int snd_card_set_spk_volume(snd_card_t *snd_card_info, int volume_percent);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
