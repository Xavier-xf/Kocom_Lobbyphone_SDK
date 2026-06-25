#ifndef __PARSER_UVC_CONFIGFS_H__
#define __PARSER_UVC_CONFIGFS_H__

#define UVC_FORMAT_MJPEG_SUPPORT    (1)
#define UVC_FORMAT_H264_SUPPORT     (1 << 1)
#define UVC_FORMAT_YUYV_SUPPORT     (1 << 2)
#define UVC_FORMAT_NV12_SUPPORT     (1 << 3)

struct uvc_frame
{
    int bFrameIndex;
    int bmCapabilities;
    int dwDefaultFrameInterval;
    int dwFrameInterval;
    int dwMaxBitRate;
    int dwMaxVideoFrameBufferSize;
    int dwMinBitRate;
    int wHeight;
    int wWidth;
};

struct uvc_format
{
    int format; //V4L2_PIX_FMT_H264
    int frame_num;
    struct uvc_frame *frame;
};

struct uvc_configfs_para
{
    int format_num;
    int format_support;     //bit0 MJPEG, bit1 H264, bit2 YUYV, bit3 NV12
    struct uvc_format *format;
};

struct uvc_configfs_para *init_parser_uvc_configfs(int uvc);

void destroy_parser_uvc_configfs(struct uvc_configfs_para *para);

struct uvc_frame *find_uvc_frame(struct uvc_configfs_para *para, int format_index, int frame_index);

#endif