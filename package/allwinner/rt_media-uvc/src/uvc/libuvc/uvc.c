/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * UVC protocol handling
 *
 * Copyright (C) 2010-2018 Laurent Pinchart
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */
#include "../../utils/sys/include/sys_linux_ioctl.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/usb/ch9.h>
#include <linux/usb/video.h>
#include <linux/usb/uapi_uvc.h>

#include "include/v4l2.h"
#include "include/tools.h"
#include "include/stream.h"
#include "../../utils/debug/include/debug.h"
#include "../../utils/libevent/include/demo_events.h"
#include "../../utils/configfs_parser/include/configfs_parser.h"

struct uvc_device
{
    struct v4l2_device *vdev;

    struct uvc_stream *stream;
    struct uvc_configfs_para *fc;

    struct uvc_streaming_control probe;
    struct uvc_streaming_control commit;

    int control; //UVC_VS_PROBE_CONTROL, UVC_VS_COMMIT_CONTROL

    unsigned int fcc;
    unsigned int width;
    unsigned int height;
    unsigned int maxsize;

    int bulk_mode;
    //uvc_control_callback uvc_control_callback;
};

struct uvc_device *uvc_open(const char *devname, struct uvc_stream *stream)
{
    struct uvc_device *dev;

    dev = malloc(sizeof *dev);
    if (dev == NULL)
        return NULL;

    memset(dev, 0, sizeof *dev);
    dev->stream = stream;

    dev->vdev = v4l2_open(devname);
    if (dev->vdev == NULL) {
        free(dev);
        return NULL;
    }
    logv("uvc %s fd %d\n", devname, dev->vdev->fd);

    return dev;
}

void uvc_close(struct uvc_device *dev)
{
    if (uvc_stream_is_streaming(dev->stream))
        uvc_stream_enable(dev->stream, 0);

    v4l2_close(dev->vdev);
    dev->vdev = NULL;

    free(dev);
}

/* ---------------------------------------------------------------------------
 * Request processing
 */

static void
uvc_fill_streaming_control(struct uvc_device *dev,
			   struct uvc_streaming_control *ctrl,
			   int iformat, int iframe, unsigned int ival)
{
    struct uvc_format *format;
    struct uvc_frame *frame;
    unsigned int i;

    if (iformat == -1)
        iformat = dev->fc->format_num;
    format = &dev->fc->format[iformat - 1];

    if (iframe == -1)
        iframe = format->frame_num;
    frame  = &format->frame[iframe - 1];

    memset(ctrl, 0, sizeof *ctrl);
    ctrl->bmHint = 1;
    ctrl->bFormatIndex = iformat;
    ctrl->bFrameIndex = iframe;
    ctrl->dwFrameInterval = frame->dwDefaultFrameInterval;
    ctrl->wDelay = 0;
    ctrl->dwMaxVideoFrameSize = frame->dwMaxVideoFrameBufferSize;
    ctrl->dwMaxPayloadTransferSize = frame->dwMaxVideoFrameBufferSize;
    ctrl->bmFramingInfo = 3;
    ctrl->bPreferedVersion = 1;
    ctrl->bMaxVersion = 1;
    ctrl->bMinVersion = 1;
}

static void
uvc_events_process_standard(struct uvc_device *dev,
			    const struct usb_ctrlrequest *ctrl,
			    struct uvc_request_data *resp)
{
    logv("standard request");
    (void)dev;
    (void)ctrl;
    (void)resp;
}

static void
uvc_events_process_control(struct uvc_device *dev, const struct usb_ctrlrequest *ctrl,
			   struct uvc_request_data *resp)
{
    int control_data;
    uint8_t cs = ctrl->bRequest;
    uint8_t req = ctrl->wValue >> 8;

    logv("control request (req %02x cs %02x index 0x%x request length %d)", req, cs, ctrl->wIndex, resp->length);
    (void)dev;
    (void)resp;

    /*if (dev->uvc_control_callback)
        dev->uvc_control_callback(req, cs, &control_data);*/

    if (ctrl->wIndex != 0x0200)
        return;

    resp->length = ctrl->wLength;
    switch(req){
        case UVC_SET_CUR:
            dev->control = cs;
            break;
        case UVC_GET_DEF:
        case UVC_GET_CUR:
            switch(cs) {
                case UVC_PU_BRIGHTNESS_CONTROL:
                    logv("UVC_PU_BRIGHTNESS_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                    control_data = 0;
                    break;

                default:
                    loge("GET_CUR unsupport control field 0x%x", cs);
                    break;
            }
            resp->data[0] = control_data & 0x00ff;
            break;
        case UVC_GET_MIN:
            switch(cs) {
                case UVC_PU_BRIGHTNESS_CONTROL:
                    logv("UVC_PU_BRIGHTNESS_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                    control_data = 0;
                    break;

                default:
                    loge("GET_CUR unsupport control field 0x%x", cs);
                    break;
            }
            resp->data[0] = 0x0;
            break;
        case UVC_GET_MAX:
            switch(cs) {
                case UVC_PU_BRIGHTNESS_CONTROL:
                    logv("UVC_PU_BRIGHTNESS_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                    control_data = 0;
                    break;

                default:
                    loge("GET_CUR unsupport control field 0x%x", cs);
                    break;
            }
            resp->data[0] = control_data;
            break;
        case UVC_GET_RES:
            switch(cs) {
                case UVC_PU_BRIGHTNESS_CONTROL:
                    logv("UVC_PU_BRIGHTNESS_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                    control_data = 0;
                    break;

                default:
                    loge("GET_CUR unsupport control field 0x%x", cs);
                    break;
            }
            resp->data[0] = control_data;
            break;
        case UVC_GET_INFO:
            switch(cs) {
                case UVC_PU_BRIGHTNESS_CONTROL:
                    logv("UVC_PU_BRIGHTNESS_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                    control_data = 0;
                    break;

                case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                    logv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                    control_data = 0;
                    break;

                default:
                    loge("GET_CUR unsupport control field 0x%x", cs);
                    break;
            }
            break;
        default:
            break;
    }
}

static void
uvc_events_process_streaming(struct uvc_device *dev, uint8_t req, uint8_t cs,
			     struct uvc_request_data *resp)
{
    struct uvc_streaming_control *ctrl;

    logv("streaming request (req %02x cs %02x)", req, cs);

    if (cs != UVC_VS_PROBE_CONTROL && cs != UVC_VS_COMMIT_CONTROL)
        return;

    ctrl = (struct uvc_streaming_control *)&resp->data;
    resp->length = sizeof *ctrl;

    switch (req) {
    case UVC_SET_CUR:
        dev->control = cs;
        resp->length = 34;
        break;

    case UVC_GET_CUR:
        if (cs == UVC_VS_PROBE_CONTROL)
            memcpy(ctrl, &dev->probe, sizeof *ctrl);
        else
            memcpy(ctrl, &dev->commit, sizeof *ctrl);
        break;

    case UVC_GET_MIN:
    case UVC_GET_MAX:
    case UVC_GET_DEF:
        if (req == UVC_GET_MAX)
            uvc_fill_streaming_control(dev, ctrl, -1, -1, UINT_MAX);
        else
            uvc_fill_streaming_control(dev, ctrl, 1, 1, 0);
        break;

    case UVC_GET_RES:
        memset(ctrl, 0, sizeof *ctrl);
        break;

    case UVC_GET_LEN:
        resp->data[0] = 0x00;
        resp->data[1] = 0x22;
        resp->length = 2;
        break;

    case UVC_GET_INFO:
        resp->data[0] = 0x03;
        resp->length = 1;
        break;
    }
}

static void
uvc_events_process_class(struct uvc_device *dev,
			 const struct usb_ctrlrequest *ctrl,
			 struct uvc_request_data *resp)
{
    unsigned int interface = ctrl->wIndex & 0xff;

    if ((ctrl->bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
        return;

    if (interface == UVC_INTF_CONTROL)
        //uvc_events_process_control(dev, ctrl->bRequest, ctrl->wValue >> 8, resp);
        uvc_events_process_control(dev, ctrl, resp);
    else if (interface == UVC_INTF_STREAMING)
        uvc_events_process_streaming(dev, ctrl->bRequest, ctrl->wValue >> 8, resp);
}

static void
uvc_events_process_setup(struct uvc_device *dev,
            const struct usb_ctrlrequest *ctrl,
            struct uvc_request_data *resp)
{
    dev->control = 0;

    logv("bRequestType %02x bRequest %02x wValue %04x wIndex %04x "
        "wLength %04x", ctrl->bRequestType, ctrl->bRequest,
        ctrl->wValue, ctrl->wIndex, ctrl->wLength);

    switch (ctrl->bRequestType & USB_TYPE_MASK) {
    case USB_TYPE_STANDARD:
        uvc_events_process_standard(dev, ctrl, resp);
        break;

    case USB_TYPE_CLASS:
        uvc_events_process_class(dev, ctrl, resp);
        break;

    default:
        break;
	}
}

static void
uvc_events_process_data(struct uvc_device *dev,
			const struct uvc_request_data *data)
{
    const struct uvc_streaming_control *ctrl =
        (const struct uvc_streaming_control *)&data->data;
    struct uvc_streaming_control *target;

    if ((data->length == 1) || (data->length == 2)) {
        int control_data = 0;
        if (data->length == 1)
            control_data = data->data[0];
        if (data->length == 2)
            control_data = data->data[0] | (data->data[1] << 8);

        switch (dev->control) {
        case UVC_PU_BRIGHTNESS_CONTROL: //0x02 brightness
            logv("UVC_PU_BRIGHTNESS_CONTROL 0x%x", control_data);
            break;

        case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
            logv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL 0x%x", control_data);
            break;

        case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
            logv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL 0x%x", control_data);
            break;

        default:
            loge("unknown, control = 0x%02x\n", dev->control);
            break;
        }
        return;
    }

    switch (dev->control) {
    case UVC_VS_PROBE_CONTROL:
        logv("setting probe control, length = %d", data->length);
        target = &dev->probe;
        break;

    case UVC_VS_COMMIT_CONTROL:
        logv("setting commit control, length = %d", data->length);
        target = &dev->commit;
        break;

    default:
        logv("setting unknown control, length = %d", data->length);
        return;
    }

    uvc_fill_streaming_control(dev, target, ctrl->bFormatIndex,
                        ctrl->bFrameIndex, ctrl->dwFrameInterval);
    if (!dev->bulk_mode)
        target->dwMaxPayloadTransferSize = 1024;

    if (dev->control == UVC_VS_COMMIT_CONTROL) {
        struct v4l2_pix_format pixfmt;
        unsigned int fps;

        struct uvc_format *format;
        struct uvc_frame *frame;
        format = &dev->fc->format[target->bFormatIndex - 1];
        frame = find_uvc_frame(dev->fc, target->bFormatIndex, target->bFrameIndex);

        dev->width = frame->wWidth;
        dev->height = frame->wHeight;
        dev->vdev->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

        uvc_stream_set_format(dev->stream, format, frame);

        if (dev->bulk_mode) {
            if (uvc_stream_is_streaming(dev->stream))
                uvc_stream_enable(dev->stream, 0);
            uvc_stream_enable(dev->stream, 1);
        }
    }
}

static void uvc_events_process(void *d)
{
    struct uvc_device *dev = d;
    struct v4l2_event v4l2_event;
    const struct uvc_event *uvc_event = (void *)&v4l2_event.u.data;
    struct uvc_request_data resp;
    int ret;

    ret = ioctl(dev->vdev->fd, VIDIOC_DQEVENT, &v4l2_event);
    if (ret < 0) {
        logv("VIDIOC_DQEVENT failed: %s (%d)", strerror(errno),
            errno);
        return;
    }

    memset(&resp, 0, sizeof resp);
    resp.length = -EL2HLT;

    switch (v4l2_event.type) {
    case UVC_EVENT_CONNECT:
    case UVC_EVENT_DISCONNECT:
        if (uvc_stream_is_streaming(dev->stream) && !dev->bulk_mode)
            uvc_stream_enable(dev->stream, 0);
        return;

    case UVC_EVENT_SETUP:
        uvc_events_process_setup(dev, &uvc_event->req, &resp);
        break;

    case UVC_EVENT_DATA:
        uvc_events_process_data(dev, &uvc_event->data);
        return;

    case UVC_EVENT_STREAMON:
        if (!uvc_stream_is_streaming(dev->stream) && !dev->bulk_mode)
            uvc_stream_enable(dev->stream, 1);
        return;

    case UVC_EVENT_STREAMOFF:
        if (uvc_stream_is_streaming(dev->stream) && !dev->bulk_mode)
            uvc_stream_enable(dev->stream, 0);
        return;
    }

    ret = ioctl(dev->vdev->fd, UVCIOC_SEND_RESPONSE, &resp);
    if (ret < 0) {
        logv("UVCIOC_SEND_RESPONSE failed: %s (%d)",
            strerror(errno), errno);
        return;
    }
}

/* ---------------------------------------------------------------------------
 * Initialization and setup
 */

void uvc_events_init(struct uvc_device *dev, struct demo_events *events)
{
    struct v4l2_event_subscription sub;

    /* Default to the minimum values. */
    uvc_fill_streaming_control(dev, &dev->probe, 1, 1, 0);
    uvc_fill_streaming_control(dev, &dev->commit, 1, 1, 0);

    memset(&sub, 0, sizeof sub);
    sub.type = UVC_EVENT_SETUP;
    ioctl(dev->vdev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_DATA;
    ioctl(dev->vdev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMON;
    ioctl(dev->vdev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMOFF;
    ioctl(dev->vdev->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);

    demo_events_watch_fd(events, dev->vdev->fd, EVENT_EXCEPTION,
            uvc_events_process, dev);
}

void uvc_set_config(struct uvc_device *dev, struct uvc_configfs_para *fc)
{
    /* FIXME: The maximum size should be specified per format and frame. */
    dev->maxsize = 0;
    dev->fc = fc;
}

int uvc_set_format(struct uvc_device *dev, struct v4l2_pix_format *format)
{
    return v4l2_set_format(dev->vdev, format);
}

int uvc_get_format(struct uvc_device *dev, struct v4l2_pix_format *format)
{
    return v4l2_get_format(dev->vdev, format);
}


struct v4l2_device *uvc_v4l2_device(struct uvc_device *dev)
{
    /*
     * TODO: The V4L2 device shouldn't be exposed. We should replace this
     * with an abstract video sink class when one will be avaiilable.
     */
    return dev->vdev;
}

void uvc_set_bulk_mode(struct uvc_device *dev, int bulk_mode)
{
    dev->bulk_mode = bulk_mode;
}
