#ifndef __AW_UVCOUT_H__
#define __AW_UVCOUT_H__

#include <linux/usb/ch9.h>
#include <linux/usb/video.h>
#include <linux/videodev2.h>
#include "g_uvc.h"

#include "doorlock_common.h"
#include "aw_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ALIGN_16B
#else
#define ALIGN_16B(x) (((x) + (15)) & ~(15))
#endif

#define USB_FRAME_NUM 2
#define UVC_PAYLOAD_HEADER_LEN 12
#define UVC_TEST 0

typedef struct uvc_frame_info_s {
    unsigned int frame_index;
    unsigned int width;
    unsigned int height;
    unsigned int fps;
    unsigned int rotate_flag;  // 0:0, 1:90, 2:180, 3:270
} uvc_frame_info_t;

typedef struct uvc_format_info_s {
    unsigned int format_index;
    unsigned int format;
    unsigned int default_frame_index;
    unsigned int frame_cnt;
    uvc_frame_info_t *frames;
} uvc_format_info_t;

typedef struct uvc_frame_s {
    void *vir_addr;
    void *phy_addr;
    unsigned int buf_len;
} uvc_frame_t;

typedef struct uvc_out_s {
    uvc_format_info_t *uvcout_format_frame_info;
    int uvcout_format_cnt;
    char uvc_dev_name[256];
    uint16_t usb_vid;
    uint16_t usb_pid;
    uvc_frame_t *uvc_frames;

    int fast_connect;
    int bulk_mode;
    int usb_speed;
    int uvc_max_payload_size;
    int uvc_max_streaming_size;

    struct uvc_streaming_control streaming_probe;
    struct uvc_streaming_control streaming_commit;
    int default_format_index;
    int default_frame_index;
    uint32_t ctrl_interface_set_cur;
    uint32_t tansfer_frame_cnt;
    volatile uint32_t is_streaming;
    volatile uint32_t is_resource_ok;
    uint32_t force_exit;

    uint32_t format_v4l2;
    int format_index;
    int frame_index;
    uint32_t cap_width;
    uint32_t cap_height;
    uint32_t max_frame_size;
    uint32_t frame_rate;
    uint32_t rotate;
    uint32_t bufs_num;

    int uvcout_fd;
    pthread_t uvcout_thread;
    int uvcout_state;
    pthread_mutex_t uvcout_lock;
    frm_manager_t frm_manager;
    int (*resource_on_off)(struct uvc_out_s *uvcout_info, bool on_flag);
} uvc_out_t;

int uvc_transfer_video_frame(uvc_out_t *uvcout_info, uint8_t *data_buf, uint32_t data_size);
int uvc_out_init(uvc_out_t *uvcout_info);
int uvc_out_run(uvc_out_t *uvcout_info);
int uvc_out_deinit(uvc_out_t *uvcout_info);
int uvc_out_connect(uvc_out_t *uvcout_info);
int uvc_out_disconnect(uvc_out_t *uvcout_info);
int uvc_out_force_exit(uvc_out_t *uvcout_info);
int uvc_out_fast_setup(uvc_out_t *uvcout_info);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
