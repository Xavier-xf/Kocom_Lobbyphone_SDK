/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * UVC stream handling
 *
 * Copyright (C) 2010-2018 Laurent Pinchart
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "include/uvc.h"
#include "include/v4l2.h"
#include "include/stream.h"
#include "include/video_buffers.h"
#include "../../utils/debug/include/debug.h"
#include "../../utils/libevent/include/demo_events.h"
#include "../video-source/base/include/video_source.h"
#include "../../utils/configfs_parser/include/configfs_parser.h"

/*
 * struct uvc_stream - Representation of a UVC stream
 * @src: video source
 * @uvc: UVC V4L2 output device
 * @events: struct events containing event information
 */
struct uvc_stream
{
    struct video_source *src;
    struct uvc_device *uvc;

    struct demo_events *events;

    struct uvc_format *format;
    struct uvc_frame *frame;
    int is_streaming;
};

/* ---------------------------------------------------------------------------
 * Video streaming
 */

//static void uvc_stream_source_process(void *d, struct video_source *src,
//				      struct video_buffer *buffer)
//{
//	struct uvc_stream *stream = d;
//	struct v4l2_device *sink = uvc_v4l2_device(stream->uvc);
//
//	v4l2_queue_buffer(sink, buffer);
//}

static unsigned int frame_cnt = 0;
static void uvc_stream_uvc_process(void *d)
{
    struct uvc_stream *stream = d;
    struct v4l2_device *sink = uvc_v4l2_device(stream->uvc);
    struct video_buffer buf;
    int delay_ms;
    int ret;

    struct video_source_frame *frame = video_source_get_frame(stream->src);
    if (!frame)
        return;

    ret = v4l2_dequeue_buffer(sink, &buf);
    if (ret < 0) {
        goto _release_src_frame;
    }

    if (sink->buffers.buffers[buf.index].size >= frame->data_len) {
        memcpy(sink->buffers.buffers[buf.index].mem, frame->buf_vir_addr, frame->data_len);
        buf.bytesused = frame->data_len;
    } else {
        buf.bytesused = 0;
    }

    ret = v4l2_queue_buffer(sink, &buf);

_release_src_frame:
    video_source_release_frame(stream->src, frame);

    /*
     *  because usb transport so fast, this cause uvc_stream_uvc_process called very frequent,
     *  cpu usag is high. so we delay at userspace.
     */
    delay_ms = (1000 / (10000000 / stream->frame->dwFrameInterval)) - 10;
    usleep(delay_ms * 1000);
}

static int uvc_stream_start(struct uvc_stream *stream)
{
    struct v4l2_device *sink = uvc_v4l2_device(stream->uvc);
    struct video_buffer_set *buffers = NULL;
    int ret;

    logv("Starting video stream.\n");

    struct video_source_base_config config;
    memset(&config, 0, sizeof(config));
    config.width = stream->frame->wWidth;
    config.height = stream->frame->wHeight;
    config.format = stream->format->format;
    config.framerate = 10000000 / stream->frame->dwFrameInterval;
    video_source_start(stream->src, &config);

    sink->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ret = v4l2_alloc_buffers(sink, V4L2_MEMORY_MMAP, 3);
    if (ret < 0) {
        loge("Failed to allocate sink buffers: %s (%d)",
            strerror(-ret), -ret);
        goto error_free_source;
    }

    ret = v4l2_mmap_buffers(sink);
    if (ret < 0) {
        loge("Failed to import buffers on sink: %s (%d)",
            strerror(-ret), -ret);
        goto error_free_sink;
    }

    /* Start the source and sink. */
    v4l2_stream_on(sink);
    demo_events_watch_fd(stream->events, sink->fd, EVENT_WRITE,
                    uvc_stream_uvc_process, stream);
    logv("uvc fd %d start streaming!", sink->fd);
    stream->is_streaming = 1;
    return 0;

error_free_sink:
    v4l2_free_buffers(sink);
error_free_source:
    if (buffers)
        video_buffer_set_delete(buffers);
    return ret;
}

static int uvc_stream_stop(struct uvc_stream *stream)
{
    struct v4l2_device *sink = uvc_v4l2_device(stream->uvc);

    logv("Stopping video stream.\n");

    v4l2_stream_off(sink);
    video_source_stop(stream->src);

    v4l2_free_buffers(sink);
    demo_events_unwatch_fd(stream->events, sink->fd, EVENT_WRITE);
    stream->is_streaming = 0;
    logv("uvc streaming stop\n");
    return 0;
}

void uvc_stream_enable(struct uvc_stream *stream, int enable)
{
    if (enable)
        uvc_stream_start(stream);
    else
        uvc_stream_stop(stream);
}

int uvc_stream_set_format(struct uvc_stream *stream,
                const struct uvc_format *format, const struct uvc_frame *frame)
{
    struct v4l2_pix_format fmt;
    int ret;

    logv("Setting format to 0x%08x %dx%d %d",
        format->format, frame->wWidth, frame->wHeight, frame->dwMaxVideoFrameBufferSize);

    stream->format = (struct uvc_format *)format;
    stream->frame = (struct uvc_frame *)frame;

    memset(&fmt, 0, sizeof fmt);
    fmt.width = frame->wWidth;
    fmt.height = frame->wHeight;
    fmt.pixelformat = format->format;
    fmt.field = V4L2_FIELD_NONE;
    fmt.sizeimage = frame->dwMaxVideoFrameBufferSize;
    logd("set fmt size %dx%d sizeimage %d", fmt.width, fmt.height, fmt.sizeimage);

    ret = uvc_set_format(stream->uvc, &fmt);
    if (ret < 0)
        return ret;

    uvc_get_format(stream->uvc, &fmt);
    logv("get foramt %dx%d sizeimage %d format %d", fmt.width, fmt.height, fmt.sizeimage, fmt.pixelformat);

    return ret;
}

int uvc_stream_set_frame_rate(struct uvc_stream *stream, unsigned int fps)
{
    logv("Setting frame rate to %u fps", fps);
    return 0;
}

/* ---------------------------------------------------------------------------
 * Stream handling
 */

struct uvc_stream *uvc_stream_new(const char *uvc_device)
{
    struct uvc_stream *stream;

    stream = malloc(sizeof(*stream));
    if (stream == NULL)
        return NULL;

    memset(stream, 0, sizeof(*stream));

    stream->uvc = uvc_open(uvc_device, stream);
    if (stream->uvc == NULL)
        goto error;

    return stream;

    error:
    free(stream);
    return NULL;
}

void uvc_stream_delete(struct uvc_stream *stream)
{
    if (stream == NULL)
        return;

    uvc_close(stream->uvc);
    free(stream);
}

void uvc_stream_init_uvc(struct uvc_stream *stream,
			 struct uvc_configfs_para *fc)
{
    uvc_set_config(stream->uvc, fc);
    uvc_events_init(stream->uvc, stream->events);
}

void uvc_stream_set_event_handler(struct uvc_stream *stream,
				  struct demo_events *events)
{
    stream->events = events;
}

void uvc_stream_set_video_source(struct uvc_stream *stream,
				 struct video_source *src)
{
    stream->src = src;
}

int uvc_stream_is_streaming(struct uvc_stream *stream)
{
    return stream->is_streaming;
}

void uvc_stream_set_bulk_mode(struct uvc_stream *stream, int bulk_mode)
{
    uvc_set_bulk_mode(stream->uvc, bulk_mode);
}
