#ifndef __DOORLOCK_MAIN_H__
#define __DOORLOCK_MAIN_H__

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

#define APP_VERSION 0x00000100
#define APP_VERSION_MATCH(a, b) (((a) & 0xffffff00) == ((b) & 0xffffff00) ? 1 : 0)

#define UVC_SUPPORT APP_UVC_SUPPORT  // 1
#define UAC_SUPPORT APP_UAC_SUPPORT  // 0
#define UVC_DYNAMIC_SWITCH_SUPPORT 0
#define UVC_VIDEO_BASE 0
#define ALGO_VIDEO_BASE 4
#define VENC_INPUT_YUV422 0

#define APP_ROTATE 1
#define UVC_ROTATE 0
#define FACEAE_SUPPORT 1
#define ORL_SUPPORT 0
#define OSD_SUPPORT 0
#define SENSOR_WIDTH 640
#define SENSOR_HEIGHT 480
#define UVC_SENSOR_WIDTH  864
#define UVC_SENSOR_HEIGHT  480
#define SENSOR_FPS 30

#define SENSOR_CONFIG_BY_FLASH 1

#if (APP_ROTATE % 2)
#define ALGO_IMAGE_WIDTH SENSOR_HEIGHT
#define ALGO_IMAGE_HEIGHT SENSOR_WIDTH
#else
#define ALGO_IMAGE_WIDTH SENSOR_WIDTH
#define ALGO_IMAGE_HEIGHT SENSOR_HEIGHT
#endif
#define ALGO_IMAGE_SIZE (ALIGN_16B(ALGO_IMAGE_WIDTH) * ALIGN_16B(ALGO_IMAGE_HEIGHT) * 3 / 2)

static inline int64_t get_timeofday(void)
{
    struct timeval tv;
    int64_t time;

    memset(&tv, 0, sizeof(struct timeval));
    gettimeofday(&tv, NULL);
    time = tv.tv_sec * 1000000 + tv.tv_usec;
    return time;
}

int door_lock_read_thermal(void);
int door_lock_save_ae(int index);
int door_lock_set_video_flip(int flip_ctrl);
int door_lock_set_video_mode(int index, int mode);
int door_lock_select_uvc_video(int videoIndex);
int door_lock_set_led(int led_ctrl);
int door_lock_set_roi_ae(int index, uint8_t target, int x1, int y1, int x2, int y2);
int door_lock_get_jpg_data(uint32_t index, uint32_t width, uint32_t height, uint8_t *data,
                           uint32_t max_data_len);
int door_lock_save_isp_param(int channel);
int door_lock_set_audio_volume(int volume);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif /* __DOORLOCK_MAIN_H__ */
