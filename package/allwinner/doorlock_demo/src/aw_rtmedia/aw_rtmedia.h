#ifndef __AW_RTMEDIA_H__
#define __AW_RTMEDIA_H__

#include "doorlock_common.h"
#include <AW_VideoInput_API.h>
#include <cdx_list.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef V4L2_PIX_FMT_H265
#define V4L2_PIX_FMT_H265 v4l2_fourcc('H', '2', '6', '5')
#endif

#define MOTION_SEARCH_HOR_NUM 16
#define MOTION_SEARCH_VER_NUM 9
#define MOTION_SEARCH_TOTAL_NUM (MOTION_SEARCH_HOR_NUM * MOTION_SEARCH_VER_NUM)

typedef struct rt_media_chn_s {
    uint32_t v4l2_format_type;          // V4L2_PIX_FMT_NV21;
    uint32_t input_rt_format;           // RT_PIXEL_LBC_25X, RT_PIXEL_YVU420SP
    uint32_t chn_state;                 // 0: stop, 1:start, 2:other
    Channel_Thread_exit exit_callback;  // typedef void (*Channel_Thread_exit)(void);

    VideoInputConfig chn_attr;
    pthread_mutex_t chn_mutex;
    uint32_t drop_frame_num;
    uint32_t src_width;
    uint32_t src_height;
    uint32_t dst_width;
    uint32_t dst_height;
    uint32_t src_fps;
    uint32_t dst_fps;
    uint32_t rotate;  // ROTATE_NONE
    uint32_t bitrate;
    uint32_t enc_quality;  // 99
    uint32_t rc_mode;      // 0:CBR, 1:VBR, 2:FIXQP, 3:QPMAP
    uint32_t vi_dev;       //
    uint32_t chn;          //

    uint32_t frame_index;
    pthread_t thread;
    pthread_attr_t *thread_attr;
    uint32_t thread_exit;
    void (*stream_callback)(const AWVideoInput_StreamInfo *stream_info);
} rt_media_chn_t;

typedef struct rt_media_orl_s {
    RTIspOrl isp_osd_info;
    uint32_t num;
    uint32_t line_width;
    uint32_t pos_x[MAX_ISP_ORL_NUM];
    uint32_t pos_y[MAX_ISP_ORL_NUM];
    uint32_t width[MAX_ISP_ORL_NUM];
    uint32_t height[MAX_ISP_ORL_NUM];
    uint32_t color_rgb[MAX_ISP_ORL_NUM];
    uint32_t vi_dev;
} rt_media_orl_t;

typedef struct rt_media_osd_s {
    VideoInputOSD video_osd_info;
    uint8_t item_valid[MAX_OVERLAY_ITEM_SIZE];
    uint32_t vi_dev;  //
} rt_media_osd_t;

typedef struct rt_media_osd_chn_s {
    rt_media_osd_t *media_osd;
    uint32_t show;
    uint32_t osd_type;  // line bitmap
    uint8_t *argb_data;
    VENC_OVERLAY_ARGB_TYPE overlay_argb_type;
    uint32_t chn;
    uint32_t pos_x;
    uint32_t pos_y;
    uint32_t width;
    uint32_t height;

    uint32_t color_rgb;
    uint32_t line_width;

    uint8_t color_y;
    uint8_t color_u;
    uint8_t color_v;
} rt_media_osd_chn_t;

int rt_media_init(void);
int rt_media_deinit(void);
int rt_media_chn_reset(rt_media_chn_t *rt_media_chn_info);
int rt_media_chn_init(rt_media_chn_t *rt_media_chn_info);
int rt_media_chn_deinit(rt_media_chn_t *rt_media_chn_info);
int rt_media_chn_start(rt_media_chn_t *rt_media_chn_info);
int rt_media_chn_stop(rt_media_chn_t *rt_media_chn_info);
int rt_media_chn_restart(rt_media_chn_t *rt_media_chn_info);
int rt_media_chn_set_ir_mode(int chn_id, bool ir_mode);
int rt_media_chn_set_flip_ctrl(int chn_id, int flip_mode);

int rt_media_chn_request_yuv_data(rt_media_chn_t *rt_media_chn_info, VideoYuvFrame *frame);
int rt_media_chn_return_yuv_data(rt_media_chn_t *rt_media_chn_info, VideoYuvFrame *frame);
int rt_media_chn_get_jpeg_data(rt_media_chn_t *rt_media_chn_info, uint8_t *data_buf, uint32_t max_data_len,
                          uint32_t jpeg_width, uint32_t jpeg_height, uint8_t jpeg_quality,
                          uint32_t rotate_angle);

unsigned int rt_media_compute_video_bitrate(uint32_t src_pix_format, uint32_t v4l2_fmt, int width,
                                        int height, int fps);

int rt_media_isp_save_ae(int chn_id);
int rt_media_isp_set_local_exparea(int chn_id, int res_w, int res_h, int x1, int y1, int x2, int y2);
int rt_media_isp_set_local_exparea_force(int chn_id, int res_w, int res_h, int x1, int y1, int x2, int y2,
                                   uint8_t force_value);

int rt_media_osd_init(rt_media_osd_t *media_osd);
int rt_media_osd_deinit(rt_media_osd_t *media_osd);
int rt_media_osd_chn_update(rt_media_osd_chn_t *rt_media_osd_chn_info);
int rt_media_osd_chn_init(rt_media_osd_chn_t *rt_media_osd_chn_info);
int rt_media_osd_chn_deinit(rt_media_osd_chn_t *rt_media_osd_chn_info);
int rt_media_osd_chn_draw_text(rt_media_osd_chn_t *rt_media_osd_chn_info, char *str_buf, int rel_x, int rel_y,
                          uint32_t ft_color, uint32_t bk_color, uint8_t font_size, char *font_dir);
int rt_media_osd_chn_draw_line(rt_media_osd_chn_t *rt_media_osd_chn_info, int rel_x, int rel_y, int width,
                          int height, uint32_t color, uint8_t line_width);
int rt_media_osd_chn_draw_rect(rt_media_osd_chn_t *rt_media_osd_chn_info, int rel_x, int rel_y, int width,
                          int height, uint32_t color, uint8_t line_width);

int rt_media_orl_init(rt_media_orl_t *media_orl_info);
int rt_media_orl_deinit(rt_media_orl_t *media_orl_info);
int rt_media_orl_update(rt_media_orl_t *media_orl_info);

#ifdef __cplusplus
}
#endif

#endif
