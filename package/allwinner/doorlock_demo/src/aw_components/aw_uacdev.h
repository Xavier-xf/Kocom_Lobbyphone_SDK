#ifndef __AW_UAC_DEV_H__
#define __AW_UAC_DEV_H__
#include <alsa/asoundlib.h>
#include <pthread.h>

#include "doorlock_common.h"
#include "aw_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UAC_CHECK_EVENT_SUPPORT 0
#define UAC_MIC_SAMPLING_FREQ 1
#define UAC_SPK_SAMPLING_FREQ 1

#define UAC_TEST 0

typedef struct uac_alsa_config_s {
    snd_pcm_t *uac_handle;              // 调用snd_pcm_open打开PCM设备返回的文件句柄
    snd_pcm_hw_params_t *uac_hwparams;  // 设置流的硬件参数
    snd_pcm_sw_params_t *uac_swparams;
    snd_pcm_uframes_t period_size;
    int periods;
    int buffer_size;
    int bytes_per_frame;
    int nonblock_mode;
} uac_alsa_config_t;

typedef struct uac_dev_s {
    char uac_devname[256];
    uint16_t usb_vid;
    uint16_t usb_pid;

    int fast_connect;
    int usb_speed;

    uint8_t spk;
    uint8_t mic;

    uint32_t mic_bitwidth;
    uint32_t mic_channel_cnt;
    uint32_t mic_sample_rate;

    uint32_t spk_bitwidth;
    uint32_t spk_channel_cnt;
    uint32_t spk_sample_rate;

    uac_alsa_config_t mic_alsa_cfg;
    uac_alsa_config_t spk_alsa_cfg;
    uint32_t ctrl_interface_set_cur;
    uint32_t mic_frame_cnt;
    uint32_t spk_frame_cnt;
    uint32_t mic_nonblock;
    uint32_t spk_nonblock;
    volatile uint32_t mic_is_streaming;
    volatile uint32_t mic_is_resource_ok;
    volatile uint32_t spk_is_streaming;
    volatile uint32_t spk_is_resource_ok;
    volatile uint32_t force_exit;

    uint32_t max_mic_frame_size;  // in bytes
    uint32_t max_spk_frame_size;  // in bytes
    uint32_t frame_rate;

    int uac_dev_fd;
    pthread_t mic_thread;
    pthread_t spk_thread;
    int uac_dev_state;
    pthread_mutex_t uac_dev_lock;
    frm_manager_t mic_frm_manager;
    frm_manager_t spk_frm_manager;
    int (*mic_resource_on_off)(struct uac_dev_s *uac_dev_info, bool on_flag);
    int (*spk_resource_on_off)(struct uac_dev_s *uac_dev_info, bool on_flag);
    int (*dev_on_off)(struct uac_dev_s *uac_dev_info, bool on_flag);
} uac_dev_t;

int uac_dev_transfer_audio_spk_frame(uac_dev_t *uac_dev_info, uint8_t *data_buf, uint32_t data_size);
int uac_dev_transfer_audio_mic_frame(uac_dev_t *uac_dev_info, uint8_t *data_buf, uint32_t data_size);
int uac_dev_init(uac_dev_t *uac_dev_info);
int uac_dev_run(uac_dev_t *uac_dev_info);
int uac_dev_deinit(uac_dev_t *uac_dev_info);
int uac_dev_connect(uac_dev_t *uac_dev_info);
int uac_dev_disconnect(uac_dev_t *uac_dev_info);
int uac_dev_force_exit(uac_dev_t *uac_dev_info);
int uac_dev_fast_connect_mic(uac_dev_t *uac_dev_info);
int uac_dev_fast_connect_spk(uac_dev_t *uac_dev_info);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
