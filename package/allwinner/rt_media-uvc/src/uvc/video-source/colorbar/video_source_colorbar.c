#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <linux/videodev2.h>

#include "../base/include/video_source.h"
#include "../../../utils/debug/include/debug.h"

#include "include/mjpeg/1920x1080_mjpeg_colorbar.h"
#include "include/mjpeg/1280x720_mjpeg_colorbar.h"
#include "include/mjpeg/640x480_mjpeg_colorbar.h"

#include "include/h264/1920x1080_h264_colorbar.h"
#include "include/h264/1280x720_h264_colorbar.h"

#include "include/yuyv/320x240_yuyv_colorbar.h"
#include "include/nv12/320x240_nv12_colorbar.h"

struct video_source_colorbar_context
{
    struct video_source_frame frame;
};

static int video_source_colorbar_create(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_colorbar_context *context =
        malloc(sizeof(struct video_source_colorbar_context));
    memset(context, 0, sizeof(*context));
    video_src->ops_data = (void *)context;
    return 0;
}

static int video_source_colorbar_destroy(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_colorbar_context *context = (struct video_source_colorbar_context *)video_src->ops_data;
    if (context)
        free(context);
    video_src->ops_data = NULL;
    return 0;
}

static int video_source_colorbar_set_extra_config(void *thiz, struct video_source_extra_config *config)
{
    return 0;
}

static int video_source_colorbar_start(void *thiz, struct video_source_base_config *config)
{
    int find = 0;
    struct colorbar *colorbar = NULL;
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_colorbar_context *context = (struct video_source_colorbar_context *)video_src->ops_data;

    switch (config->format) {
    case V4L2_PIX_FMT_MJPEG:
        if ((config->width == 1920) && (config->height == 1080)) {
            context->frame.buf_vir_addr = __1920x1080_mjpeg_colorbar_jpg;
            context->frame.data_len = __1920x1080_mjpeg_colorbar_jpg_len;
        } else if ((config->width == 1280) && (config->height == 720)) {
            context->frame.buf_vir_addr = __1280x720_mjpeg_colorbar_jpg;
            context->frame.data_len = __1280x720_mjpeg_colorbar_jpg_len;
        } else if ((config->width == 640) && (config->height == 480)) {
            context->frame.buf_vir_addr = __640x480_mjpeg_colorbar_jpg;
            context->frame.data_len = __640x480_mjpeg_colorbar_jpg_len;
        } else
            loge("unsupport frame size %dx%d", config->width, config->height);
        break;
    case V4L2_PIX_FMT_H264:
        if ((config->width == 1920) && (config->height == 1080)) {
            context->frame.buf_vir_addr = __1920x1080_h264_colorbar_h264;
            context->frame.data_len = __1920x1080_h264_colorbar_h264_len;
        } else if ((config->width == 1280) && (config->height == 720)) {
            context->frame.buf_vir_addr = __1280x720_h264_colorbar_h264;
            context->frame.data_len = __1280x720_h264_colorbar_h264_len;
        } else
            loge("unsupport frame size %dx%d", config->width, config->height);
        break;
    case V4L2_PIX_FMT_YUYV:
        if ((config->width == 320) && (config->height == 240)) {
            context->frame.buf_vir_addr = __320x240_yuyv_colorbar_yuv;
            context->frame.data_len = __320x240_yuyv_colorbar_yuv_len;
        } else
            loge("unsupport frame size %dx%d", config->width, config->height);
        break;
    case V4L2_PIX_FMT_NV12:
        if ((config->width == 320) && (config->height == 240)) {
            context->frame.buf_vir_addr = __320x240_nv12_colorbar_yuv;
            context->frame.data_len = __320x240_nv12_colorbar_yuv_len;
        } else
            loge("unsupport frame size %dx%d", config->width, config->height);
        break;
    default:
        loge("unsupport foramt 0x%x", config->format);
        break;
    }

    return 0;
}

static int video_source_colorbar_stop(void *thiz)
{
    return 0;
}

static struct video_source_frame *video_source_colorbar_get_frame(void * thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_colorbar_context *context = (struct video_source_colorbar_context *)video_src->ops_data;
    if (!context) {
        loge("error! context is null!");
        return NULL;
    }

    if (context->frame.data_len && context->frame.buf_vir_addr)
        return &context->frame;
    else
        return NULL;
}

static int video_source_colorbar_release_frame(void *thiz, struct video_source_frame *video_frame)
{
    return 0;
}

const struct video_source_ops video_source_colorbar_ops = {
    .create = video_source_colorbar_create,
    .destroy = video_source_colorbar_destroy,
    .set_extra_config = video_source_colorbar_set_extra_config,
    .start = video_source_colorbar_start,
    .stop = video_source_colorbar_stop,
    .get_frame = video_source_colorbar_get_frame,
    .release_frame = video_source_colorbar_release_frame,
};
