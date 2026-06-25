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

enum video_source_stream_type {
    VIDEO_SOURCE_STREAM_TYPE_YUV = 0,
    VIDEO_SOURCE_STREAM_TYPE_H264,
    VIDEO_SOURCE_STREAM_TYPE_MJPEG,
    VIDEO_SOURCE_STREAM_TYPE_BUTT
};

struct video_source_isp_state {
    int ae_state;
    int awb_state;
    int af_state;
};

struct video_source_frame {
    int fd;
    int width;
    int height;
    enum video_source_pixfmt pix_fmt;
    void *buf_vir_addr;
    unsigned int buf_phy_addr;
    unsigned int buf_len;
    unsigned int data_len;
    unsigned long long pts;
};

struct video_source_base_config
{
    int width;
    int height;
    int format;
    int framerate;
};

struct video_source_extra_config {
    int vipp;
    int src_width;
    int src_height;
    int bitrate;
    int bufs;
    enum video_source_pixfmt input_fmt;
    enum video_source_pixfmt output_fmt;

    int dual_stream;
    int dual_stream_bufs;
    int dual_stream_vipp_dev;

    int camera_low_pw;
};

struct video_source_ops
{
    int (*create)(void *thiz);
    int (*destroy)(void *thiz);
    int (*start)(void *thiz, struct video_source_base_config *base_config);
    int (*set_extra_config)(void *thiz, struct video_source_extra_config *extra_config);
    int (*stop)(void *thiz);
    int (*get_frame)(void *thiz, struct video_source_frame *video_frame);
    int (*release_frame)(void *thiz, struct video_source_frame *video_frame);
    int (*get_isp_state)(void *thiz, struct video_source_isp_state *isp_state);
    int (*pause)(void *thiz, int flag);
    int (*set_camera_lowpw_mode)(void *thiz, void *cfg);
    int (*set_orl)(void *thiz, void *orl);
    int (*get_sharp_param)(void *thiz, void *param);
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
int video_source_get_frame(struct video_source *video_source, struct video_source_frame *frame);
int video_source_release_frame(struct video_source *video_source, struct video_source_frame *frame);
int video_source_get_isp_state(struct video_source *video_source, struct video_source_isp_state *isp_state);
int video_source_pause(struct video_source *video_souce, int flag);
int video_source_set_camera_lowpw_mode(struct video_source *video_souce, void *cfg);
int video_source_set_orl(struct video_source *video_souce, void *orl);
int video_source_get_sharp_param(struct video_source *video_source, void *param);

#endif /* __VIDEO_SOURCE_H__ */
