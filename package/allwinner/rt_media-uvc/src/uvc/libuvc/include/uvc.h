/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * UVC protocol handling
 *
 * Copyright (C) 2010-2018 Laurent Pinchart
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */

#ifndef __UVC_H__
#define __UVC_H__

typedef void (*uvc_control_callback)(int control, int control_type, int *control_data);

struct demo_events;
struct v4l2_device;
struct uvc_device;
struct uvc_configfs_para;
struct uvc_stream;
struct v4l2_pix_format;

struct uvc_device *uvc_open(const char *devname, struct uvc_stream *stream);
void uvc_close(struct uvc_device *dev);
void uvc_events_init(struct uvc_device *dev, struct demo_events *events);
void uvc_set_config(struct uvc_device *dev, struct uvc_configfs_para *fc);
int uvc_set_format(struct uvc_device *dev, struct v4l2_pix_format *format);
int uvc_get_format(struct uvc_device *dev, struct v4l2_pix_format *format);
struct v4l2_device *uvc_v4l2_device(struct uvc_device *dev);
void uvc_set_bulk_mode(struct uvc_device *dev, int bulk_mode);

#endif /* __UVC_H__ */
