#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/videodev2.h>

#include "../debug/include/debug.h"
#include "include/configfs_parser.h"

#define UVC_CONFIGFS_PATH                       "/sys/kernel/config/usb_gadget/g1/configs/c.1"
#define UVC0_CONFIGFS_STREAMING                 "/sys/kernel/config/usb_gadget/g1/configs/c.1/uvc.usb0/streaming"
#define UVC1_CONFIGFS_STREAMING                 "/sys/kernel/config/usb_gadget/g1/configs/c.1/uvc.usb1/streaming"
#define UVC_FORMAT_MJPEG                        "mjpeg/m"
#define UVC_FORMAT_H264                         "h264/h"
#define UVC_FORMAT_YUYV                         "uncompressed/u"
#define UVC_FORMAT_NV12                         "nv12/nv12"

#define UVC_FRAME_FRAME_INDEX                   "bFrameIndex"
#define UVC_FRAME_CAPABBILITIES                 "bmCapabilities"
#define UVC_FRAME_DEFAUTL_FRAME_INTERVAL        "dwDefaultFrameInterval"
#define UVC_FRAME_FRAME_INTERVAL                "dwFrameInterval"
#define UVC_FRAME_MAX_BITRATE                   "dwMaxBitRate"
#define UVC_FRAME_MAX_VIDEO_FRAME_BUFFER_SIZE   "dwMaxVideoFrameBufferSize"
#define UVC_FRAME_MIN_BITRATE                   "dwMinBitRate"
#define UVC_FRAME_HEIGHT                        "wHeight"
#define UVC_FRAME_WIDTH                         "wWidth"

/* reference bsp/drivers/usb/gadget/legacy/webcam.c */
static struct uvc_configfs_para g_webcam_config = {
    .format_num = 4,
    .format_support = 0xf,
    .format = (struct uvc_format[]) {
        {
            .format = V4L2_PIX_FMT_MJPEG,
            .frame_num = 1,
            .frame = (struct uvc_frame []) {
                {
                    .bFrameIndex               = 1,
                    .bmCapabilities            = 0,
                    .dwDefaultFrameInterval    = 500000,
                    .dwFrameInterval           = 500000,
                    .dwMaxVideoFrameBufferSize = 100000,
                    .dwMinBitRate              = 10485760,
                    .wHeight                   = 480,
                    .wWidth                    = 640,
                },
            }, /* mjpeg frame */
        }, /* mjpeg format */
        {
            .format = V4L2_PIX_FMT_YUYV,
            .frame_num = 1,
            .frame = (struct uvc_frame []) {
                {
                    .bFrameIndex               = 1,
                    .bmCapabilities            = 0,
                    .dwDefaultFrameInterval    = 500000,
                    .dwFrameInterval           = 500000,
                    .dwMaxVideoFrameBufferSize = 153600,
                    .dwMinBitRate              = 3072000,
                    .wHeight                   = 240,
                    .wWidth                    = 320,
                },
            }, /* yuyv frame */
        }, /* yuyv format */
        {
            .format = V4L2_PIX_FMT_H264,
            .frame_num = 1,
            .frame = (struct uvc_frame []) {
                {
                    .bFrameIndex               = 1,
                    .bmCapabilities            = 0,
                    .dwDefaultFrameInterval    = 500000,
                    .dwFrameInterval           = 500000,
                    .dwMaxVideoFrameBufferSize = 100000,
                    .dwMinBitRate              = 10485760,
                    .wHeight                   = 720,
                    .wWidth                    = 1280,
                },
            }, /* h264 frame */
        }, /* h264 format */
        {
            .format = V4L2_PIX_FMT_NV12,
            .frame_num = 1,
            .frame = (struct uvc_frame []) {
                {
                    .bFrameIndex               = 1,
                    .bmCapabilities            = 0,
                    .dwDefaultFrameInterval    = 500000,
                    .dwFrameInterval           = 500000,
                    .dwMaxVideoFrameBufferSize = 115200,
                    .dwMinBitRate              = 2304000,
                    .wHeight                   = 240,
                    .wWidth                    = 320,
                },
            }, /* nv12 frame */
        }, /* nv12 format */
    }, /* format */
}; /* g_webcam_config */

static int read_node(char *node_path)
{
    char read_value[16] = {0};
    int fd = open(node_path, O_RDONLY);
    if (fd < 0)
    {
        loge("open node %s fail!", node_path);
        return -1;
    }
    read(fd, &read_value, sizeof(read_value));
    close(fd);
    return atoi(read_value);
}

static void parser_uvc_para(char *node_path, char *node_name, struct uvc_frame *frame)
{
    if (!strcmp(node_name, UVC_FRAME_FRAME_INDEX))
    {
        frame->bFrameIndex = read_node(node_path);
        logv("read %s read value %d", node_name, frame->bFrameIndex);
    }
    else if (!strcmp(node_name, UVC_FRAME_CAPABBILITIES))
    {
        frame->bmCapabilities = read_node(node_path);
        logv("read %s read value %d", node_name, frame->bmCapabilities);
    }
    else if (!strcmp(node_name, UVC_FRAME_DEFAUTL_FRAME_INTERVAL))
    {
        frame->dwDefaultFrameInterval = read_node(node_path);
        logv("read %s read value %d", node_name, frame->dwDefaultFrameInterval);
    }
    else if (!strcmp(node_name, UVC_FRAME_FRAME_INTERVAL))
    {
        frame->dwFrameInterval = read_node(node_path);
        logv("read %s read value %d", node_name, frame->dwFrameInterval);
    }
    else if (!strcmp(node_name, UVC_FRAME_MAX_BITRATE))
    {
        frame->dwMaxBitRate = read_node(node_path);
        logv("read %s read value %d", node_name, frame->dwMaxBitRate);
    }
    else if (!strcmp(node_name, UVC_FRAME_MAX_VIDEO_FRAME_BUFFER_SIZE))
    {
        frame->dwMaxVideoFrameBufferSize = read_node(node_path);
        logv("read %s read value %d", node_name, frame->dwMaxVideoFrameBufferSize);
    }
    else if (!strcmp(node_name, UVC_FRAME_MIN_BITRATE))
    {
        frame->dwMinBitRate = read_node(node_path);
        logv("read %s read value %d", node_name, frame->dwMinBitRate);
    }
    else if (!strcmp(node_name, UVC_FRAME_HEIGHT))
    {
        frame->wHeight = read_node(node_path);
        logv("read %s read value %d", node_name, frame->wHeight);
    }
    else if (!strcmp(node_name, UVC_FRAME_WIDTH))
    {
        frame->wWidth = read_node(node_path);
        logv("read %s read value %d", node_name, frame->wWidth);
    }
    else
    {
        if (strcmp(node_name, ".") && strcmp(node_name, ".."))
        {
            logd("ignore read %s", node_name);
        }
    }
}

static void parser_uvc_frame(char *frame_path, struct uvc_frame *frame)
{
    int index = 0;
    char path[1024] = {0};
    DIR *dp = opendir(frame_path);
    if (!dp)
    {
        loge("opendir %s fail!", frame_path);
        return;
    }
    while (1)
    {
        struct dirent *dirp = readdir(dp);
        if (!dirp)
            break;
        logv("d_type %d d_name %s", dirp->d_type, dirp->d_name);
        if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, "..") || (dirp->d_type != DT_DIR))
            continue;
        sprintf(path, "%s/%s", frame_path, dirp->d_name);
        logv("%s", path);
        DIR *dp_frame = opendir(path);
        if (!dp_frame)
            break;
        while (1)
        {
            struct dirent *dirp_frame = readdir(dp_frame);
            if (!dirp_frame)
                break;
            logv("d_type %d d_name %s", dirp_frame->d_type, dirp_frame->d_name);
            sprintf(path, "%s/%s/%s", frame_path, dirp->d_name, dirp_frame->d_name);
            logv("%s", path);
            parser_uvc_para(path, dirp_frame->d_name, &frame[index]);
        }
        closedir(dp_frame);
        index++;
    }
    closedir(dp);
}

static int parser_uvc_frame_num(char *frame_path)
{
    int frame_cnt = 0;
    DIR *dp = opendir(frame_path);
    if (!dp)
    {
        loge("opendir %s fail!", frame_path);
        return -1;
    }
    while (1)
    {
        struct dirent *dirp = readdir(dp);
        if (!dirp)
            break;
        logv("d_type %d d_name %s", dirp->d_type, dirp->d_name);
        if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, "..") || (dirp->d_type != DT_DIR))
            continue;
        frame_cnt++;
    }
    closedir(dp);
    return frame_cnt;
}

static void parser_uvc_format(char *path, int *format_num, int *format_support)
{
    int cnt = 0;
    int format = 0;
    char configfs_path[256] = {0};

    sprintf(configfs_path, "%s/%s", path, UVC_FORMAT_MJPEG);
    logv("%s", configfs_path);
    if (!access(configfs_path, F_OK))
    {
        cnt++;
        format |= UVC_FORMAT_MJPEG_SUPPORT;
        logv("good! uvc support MJPEG");
    }
    else {
        logv("uvc unsupport MJPEG");
    }
    sprintf(configfs_path, "%s/%s", path, UVC_FORMAT_H264);
    logv("%s", configfs_path);
    if (!access(configfs_path, F_OK))
    {
        cnt++;
        format |= UVC_FORMAT_H264_SUPPORT;
        logv("good! uvc support H264");
    }
    else {
        logv("uvc unsupport H264");
    }
    sprintf(configfs_path, "%s/%s", path, UVC_FORMAT_YUYV);
    logv("%s", configfs_path);
    if (!access(configfs_path, F_OK))
    {
        cnt++;
        format |= UVC_FORMAT_YUYV_SUPPORT;
        logv("good! uvc support YUYV");
    }
    else {
        logv("uvc unsupport YUYV");
    }
    sprintf(configfs_path, "%s/%s", path, UVC_FORMAT_NV12);
    logv("%s", configfs_path);
    if (!access(configfs_path, F_OK))
    {
        cnt++;
        format |= UVC_FORMAT_NV12_SUPPORT;
        logv("good! uvc support NV12");
    }
    else {
        logv("uvc unsupport NV12");
    }

    *format_num = cnt;
    *format_support = format;
}

static void parser_legacy_g_webcam(struct uvc_configfs_para *configfs_para)
{
    configfs_para->format_num = g_webcam_config.format_num;
    configfs_para->format_support = g_webcam_config.format_support;
    configfs_para->format =
        malloc(sizeof(*g_webcam_config.format)*g_webcam_config.format_num);
    memcpy(configfs_para->format,
        g_webcam_config.format, sizeof(*g_webcam_config.format)*g_webcam_config.format_num);
    for (int i = 0; i < configfs_para->format_num; i++) {
        configfs_para->format[i].format = g_webcam_config.format[i].format;
        configfs_para->format[i].frame_num = g_webcam_config.format[i].frame_num;
        configfs_para->format[i].frame =
            malloc(sizeof(*g_webcam_config.format[i].frame)*g_webcam_config.format[i].frame_num);
        memcpy(configfs_para->format[i].frame, g_webcam_config.format[i].frame,
            sizeof(*g_webcam_config.format[i].frame)*g_webcam_config.format[i].frame_num);
    }
}
struct uvc_configfs_para *init_parser_uvc_configfs(int uvc)
{
    int index = 0;
    char path[256] = {0};
    int format_num = 0;
    int format_support = 0;
    int frame_num = 0;
    struct uvc_configfs_para *configfs_para;
    char *uvc_configfs_streaming_path;

    configfs_para = malloc(sizeof(struct uvc_configfs_para));
    if (!configfs_para)
        logw("configfs para malloc fail!");
    memset(configfs_para, 0, sizeof(struct uvc_configfs_para));

    if (access(UVC_CONFIGFS_PATH, F_OK))
    {
        loge("%s No such file or directory! we use default webcam config.", UVC_CONFIGFS_PATH);
        parser_legacy_g_webcam(configfs_para);
        return configfs_para;
    }

    if (uvc == 0)
        uvc_configfs_streaming_path = UVC0_CONFIGFS_STREAMING;
    else if (uvc == 1)
        uvc_configfs_streaming_path = UVC1_CONFIGFS_STREAMING;
    else {
        loge("do not support more then two uvc device! use default uvc device 0");
        uvc_configfs_streaming_path = UVC0_CONFIGFS_STREAMING;
    }
    parser_uvc_format(uvc_configfs_streaming_path, &format_num, &format_support);
    if (!format_num || !format_support)
    {
        loge("uvc configfs parser fail! we use default webcam config");
        parser_legacy_g_webcam(configfs_para);
        return configfs_para;
    }

    configfs_para->format_num = format_num;
    configfs_para->format_support = format_support;
    configfs_para->format = malloc(sizeof(struct uvc_format)*configfs_para->format_num);
    if (!configfs_para->format)
        logw("uvc format malloc fail!");
    loge("malloc format %p, format_num:%d, format_support:0x%x", configfs_para->format, configfs_para->format_num,
        configfs_para->format_support);
    memset(configfs_para->format, 0, sizeof(struct uvc_format)*configfs_para->format_num);

    if (configfs_para->format_support & UVC_FORMAT_MJPEG_SUPPORT)
    {
        sprintf(path, "%s/%s", uvc_configfs_streaming_path, UVC_FORMAT_MJPEG);
        logv("%s", path);
        configfs_para->format[index].format = V4L2_PIX_FMT_MJPEG;
        configfs_para->format[index].frame_num = parser_uvc_frame_num(path);
        if (configfs_para->format[index].frame_num > 0)
        {
            configfs_para->format[index].frame =
                malloc(sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            if (!configfs_para->format[index].frame)
                logw("malloc MJPEG frame fail!");
            memset(configfs_para->format[index].frame,
                0, sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            parser_uvc_frame(path, configfs_para->format[index].frame);
            index++;
        }
    }
    if (configfs_para->format_support & UVC_FORMAT_YUYV_SUPPORT)
    {
        sprintf(path, "%s/%s", uvc_configfs_streaming_path, UVC_FORMAT_YUYV);
        logv("%s", path);
        configfs_para->format[index].format = V4L2_PIX_FMT_YUYV;
        configfs_para->format[index].frame_num = parser_uvc_frame_num(path);
        if (configfs_para->format[index].frame_num > 0)
        {
            configfs_para->format[index].frame =
                malloc(sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            if (!configfs_para->format[index].frame)
                logw("malloc MJPEG frame fail!");
            memset(configfs_para->format[index].frame,
                0, sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            parser_uvc_frame(path, configfs_para->format[index].frame);
            index++;
        }
    }
    if (configfs_para->format_support & UVC_FORMAT_H264_SUPPORT)
    {
        sprintf(path, "%s/%s", uvc_configfs_streaming_path, UVC_FORMAT_H264);
        logv("%s", path);
        configfs_para->format[index].format = V4L2_PIX_FMT_H264;
        configfs_para->format[index].frame_num = parser_uvc_frame_num(path);
        if (configfs_para->format[index].frame_num > 0)
        {
            configfs_para->format[index].frame =
                malloc(sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            if (!configfs_para->format[index].frame)
                logw("malloc MJPEG frame fail!");
            memset(configfs_para->format[index].frame,
                0, sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            parser_uvc_frame(path, configfs_para->format[index].frame);
            index++;
        }
    }
    if (configfs_para->format_support & UVC_FORMAT_NV12_SUPPORT)
    {
        sprintf(path, "%s/%s", uvc_configfs_streaming_path, UVC_FORMAT_NV12);
        logv("%s", path);
        configfs_para->format[index].format = V4L2_PIX_FMT_NV12;
        configfs_para->format[index].frame_num = parser_uvc_frame_num(path);
        if (configfs_para->format[index].frame_num > 0)
        {
            configfs_para->format[index].frame =
                malloc(sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            if (!configfs_para->format[index].frame)
                logw("malloc MJPEG frame fail!");
            memset(configfs_para->format[index].frame,
                0, sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            parser_uvc_frame(path, configfs_para->format[index].frame);
            index++;
        }
    }
    return configfs_para;
}

void destroy_parser_uvc_configfs(struct uvc_configfs_para *para)
{
    if (para)
    {
        if (para->format && para->format_num)
        {
            for (int i = 0; i < para->format_num; i++)
            {
                if (para->format[i].frame)
                {
                    free(para->format[i].frame);
                    para->format[i].frame = NULL;
                }
            }
            free(para->format);
            para->format = NULL;
        }
        free(para);
    }
}

struct uvc_frame *find_uvc_frame(struct uvc_configfs_para *para, int format_index, int frame_index)
{
    int find = 0;
    struct uvc_frame *frame = NULL;
    struct uvc_format *format = &para->format[format_index-1];
    for (int i = 0; i < format->frame_num; i++)
    {
        if (format->frame[i].bFrameIndex == frame_index)
        {
            find = 1;
            frame = &format->frame[i];
            break;
        }
    }
    if (!find)
    {
        printf("impassible! don't find frame_index %d frame!", frame_index);
        frame = &format->frame[0];
    }
    return frame;
}
