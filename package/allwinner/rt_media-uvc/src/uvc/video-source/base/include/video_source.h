/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Abstract video source
 *
 * Copyright (C) 2018 Laurent Pinchart
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */
#ifndef __VIDEO_SOURCE_H__
#define __VIDEO_SOURCE_H__

enum video_source_type
{
    VIDEO_SOURCE_TYPE_COLORBAR = 0,
    VIDEO_SOURCE_TYPE_RT_MEDIA,
    VIDEO_SOURCE_TYPE_MPP,
    VIDEO_SOURCE_TYPE_FILE,
    VIDEO_SOURCE_TYPE_BUTT
};

enum video_source_pixfmt {
    VIDEO_SOURCE_PIXFMT_NV21 = 0,
    VIDEO_SOURCE_PIXFMT_NV12,
    VIDEO_SOURCE_PIXFMT_NV61,
    VIDEO_SOURCE_PIXFMT_NV16,
    VIDEO_SOURCE_PIXFMT_LBC1_0X,
    VIDEO_SOURCE_PIXFMT_LBC1_5X,
    VIDEO_SOURCE_PIXFMT_LBC2_0X,
    VIDEO_SOURCE_PIXFMT_LBC2_5X,
    VIDEO_SOURCE_PIXFMT_BUTT
};

struct video_source_frame {
    void *buf_vir_addr;
    void *buf_phy_addr;
    unsigned int buf_len;
    unsigned int data_len;
};

struct video_source_base_config
{
    int width;
    int height;
    int format; //V4L2_PIX_FMT_H264
    int framerate;
};

struct video_source_extra_config {
    int vipp;
    int src_width;
    int src_height;
    int bitrate; //unit:Mbps
    int bufs;
    enum video_source_pixfmt input_fmt;
    enum video_source_pixfmt output_fmt;

    int dual_stream;
    int dual_stream_bufs;
    int dual_stream_vipp_dev;

    int enable_aiisp;
    int aiisp_mode;
    int tdm_rxbuf_cnt;
    int aiisp_auto_switch;
    int aiisp_switch_interval;
    char isp_aiisp_bin_path[100];
    char isp_day_bin_path[100];
    char npu_lut_model_file_path[100];
    char npu_model_file_path[100];
    int npu_ref_buf_reduce_enable;
    unsigned int tdm_drop_frame;
    int aiisp_switch_case;
    int aiisp_switch_release_res_enable;
};

struct video_source_ops
{
    int (*create)(void *thiz);
    int (*destroy)(void *thiz);
    int (*start)(void *thiz, struct video_source_base_config *base_config);
    int (*set_extra_config)(void *thiz, struct video_source_extra_config *extra_config);
    int (*stop)(void *thiz);
    struct video_source_frame * (*get_frame)(void *thiz);
    int (*release_frame)(void *thiz, struct video_source_frame *video_frame);
};

struct video_source
{
    void *ops_data;
    const struct video_source_ops *ops;
};

struct video_source *video_source_create(enum video_source_type video_source_type);
void video_source_destroy(struct video_source *video_source);
int video_source_start(struct video_source *video_source, struct video_source_base_config *config);
int video_source_set_extra_config(struct video_source *video_source, struct video_source_extra_config *extra_config);
int video_source_stop(struct video_source *video_source);
struct video_source_frame *video_source_get_frame(struct video_source *video_source);
int video_source_release_frame(struct video_source *video_source, struct video_source_frame *frame);

#endif /* __VIDEO_SOURCE_H__ */
