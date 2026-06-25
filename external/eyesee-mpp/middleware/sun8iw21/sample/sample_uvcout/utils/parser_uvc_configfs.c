#define LOG_TAG "parser_uvc_configfs"
#include <utils/plat_log.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/videodev2.h>

#include "parser_uvc_configfs.h"

#define UVC_CONFIG_FS_PATH				            "/sys/kernel/config/usb_gadget/g1/configs/c.1"
#define UVC_CONFIG_FS_STREAMING			            "/sys/kernel/config/usb_gadget/g1/configs/c.1/uvc.usb0/streaming"
#define UVC_FORMAT_MJPEG				            "mjpeg"
#define UVC_FORMAT_H264					            "h264"
#define UVC_FORMAT_YUYV					            "uncompressed"

#define UVC_FRAME_FRAME_INDEX				        "bFrameIndex"
#define UVC_FRAME_CAPABBILITIES				        "bmCapabilities"
#define UVC_FRAME_DEFAUTL_FRAME_INTERVAL	        "dwDefaultFrameInterval"
#define UVC_FRAME_FRAME_INTERVAL			        "dwFrameInterval"
#define UVC_FRAME_MAX_BITRATE				        "dwMaxBitRate"
#define UVC_FRAME_MAX_VIDEO_FRAME_BUFFER_SIZE		"dwMaxVideoFrameBufferSize"
#define UVC_FRAME_MIN_BITRATE				        "dwMinBitRate"
#define UVC_FRAME_HEIGHT				            "wHeight"
#define UVC_FRAME_WIDTH					            "wWidth"

static int read_node(char *node_path)
{
    char read_value[16] = {0};
    int fd = open(node_path, O_RDONLY);
    if (fd < 0)
    {
        aloge("open node %s fail!", node_path);
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
        alogv("read %s read value %d", node_name, frame->bFrameIndex);
    }
    else if (!strcmp(node_name, UVC_FRAME_CAPABBILITIES))
    {
        frame->bmCapabilities = read_node(node_path);
        alogv("read %s read value %d", node_name, frame->bmCapabilities);
    }
    else if (!strcmp(node_name, UVC_FRAME_DEFAUTL_FRAME_INTERVAL))
    {
        frame->dwDefaultFrameInterval = read_node(node_path);
        alogv("read %s read value %d", node_name, frame->dwDefaultFrameInterval);
    }
    else if (!strcmp(node_name, UVC_FRAME_FRAME_INTERVAL))
    {
        frame->dwFrameInterval = read_node(node_path);
        alogv("read %s read value %d", node_name, frame->dwFrameInterval);
    }
    else if (!strcmp(node_name, UVC_FRAME_MAX_BITRATE))
    {
        frame->dwMaxBitRate = read_node(node_path);
        alogv("read %s read value %d", node_name, frame->dwMaxBitRate);
    }
    else if (!strcmp(node_name, UVC_FRAME_MAX_VIDEO_FRAME_BUFFER_SIZE))
    {
        frame->dwMaxVideoFrameBufferSize = read_node(node_path);
        alogv("read %s read value %d", node_name, frame->dwMaxVideoFrameBufferSize);
    }
    else if (!strcmp(node_name, UVC_FRAME_MIN_BITRATE))
    {
        frame->dwMinBitRate = read_node(node_path);
        alogv("read %s read value %d", node_name, frame->dwMinBitRate);
    }
    else if (!strcmp(node_name, UVC_FRAME_HEIGHT))
    {
        frame->wHeight = read_node(node_path);
        alogv("read %s read value %d", node_name, frame->wHeight);
    }
    else if (!strcmp(node_name, UVC_FRAME_WIDTH))
    {
        frame->wWidth = read_node(node_path);
        alogv("read %s read value %d", node_name, frame->wWidth);
    }
}

static void parser_uvc_frame(char *frame_path, struct uvc_frame *frame)
{
    int index = 0;
    char path[256] = {0};
    DIR *dp = opendir(frame_path);
    if (!dp)
    {
        aloge("opendir %s fail!", frame_path);
        return;
    }
    while (1)
    {
        struct dirent *dirp = readdir(dp);
        if (!dirp)
            break;
        alogv("d_type %d d_name %s\n", dirp->d_type, dirp->d_name);
        if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, "..") || (dirp->d_type != DT_DIR))
            continue;
        sprintf(path, "%s/%s", frame_path, dirp->d_name);
        alogv("%s", path);
        DIR *dp_frame = opendir(path);
        if (!dp_frame)
            break;
        while (1)
        {
            struct dirent *dirp_frame = readdir(dp_frame);
            if (!dirp_frame)
                break;
            alogv("d_type %d d_name %s\n", dirp_frame->d_type, dirp_frame->d_name);
            sprintf(path, "%s/%s/%s", frame_path, dirp->d_name, dirp_frame->d_name);
            alogv("%s", path);
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
        aloge("opendir %s fail!", frame_path);
        return -1;
    }
    while (1)
    {
        struct dirent *dirp = readdir(dp);
        if (!dirp)
            break;
        alogv("d_type %d d_name %s", dirp->d_type, dirp->d_name);
        if (!strcmp(dirp->d_name, ".") || !strcmp(dirp->d_name, "..") || (dirp->d_type != DT_DIR))
            continue;
        frame_cnt++;
    }
    closedir(dp);
    return frame_cnt;
}

static void parser_uvc_format(int *format_num, int *format_support)
{
    int cnt = 0;
    int format = 0;
    char path[256] = {0};

    sprintf(path, "%s/%s", UVC_CONFIG_FS_STREAMING, UVC_FORMAT_MJPEG);
    alogv("%s", path);
    if (!access(path, F_OK))
    {
        cnt++;
        format |= UVC_FORMAT_MJPEG_SUPPORT;
        alogv("good! uvc support MJPEG");
    }
    else
        alogd("uvc unsupport MJPEG");
    sprintf(path, "%s/%s", UVC_CONFIG_FS_STREAMING, UVC_FORMAT_H264);
    alogv("%s", path);
    if (!access(path, F_OK))
    {
        cnt++;
        format |= UVC_FORMAT_H264_SUPPORT;
        alogv("good! uvc support H264");
    }
    else
        alogd("uvc unsupport H264");
    sprintf(path, "%s/%s", UVC_CONFIG_FS_STREAMING, UVC_FORMAT_YUYV);
    alogv("%s", path);
    if (!access(path, F_OK))
    {
        cnt++;
        format |= UVC_FORMAT_YUYV_SUPPORT;
        alogv("good! uvc support YUYV");
    }
    else
        alogd("uvc unsupport YUYV");

    *format_num = cnt;
    *format_support = format;
}

struct uvc_configfs_para *init_prarser_uvc_configfs(void)
{
    int index = 0;
    char path[256] = {0};
    int format_num = 0;
    int format_support = 0;
    int frame_num = 0;
    struct uvc_configfs_para *configfs_para;

    if (access(UVC_CONFIG_FS_PATH, F_OK))
    {
        alogw("%s No such file or directory!", UVC_CONFIG_FS_PATH);
        return NULL;
    }

    parser_uvc_format(&format_num, &format_support);
    if (!format_num || !format_support)
    {
        alogw("uvc fromat num is invalid!");
        return NULL;
    }

    configfs_para = malloc(sizeof(struct uvc_configfs_para));
    if (!configfs_para)
        aloge("configfs para malloc fail!");
    memset(configfs_para, 0, sizeof(struct uvc_configfs_para));
    configfs_para->format_num = format_num;
    configfs_para->format_support = format_support;
    configfs_para->format = malloc(sizeof(struct uvc_format)*configfs_para->format_num);
    if (!configfs_para->format)
        aloge("uvc format malloc fail!");
    memset(configfs_para->format, 0, sizeof(struct uvc_format)*configfs_para->format_num);

    if (configfs_para->format_support & UVC_FORMAT_MJPEG_SUPPORT)
    {
        sprintf(path, "%s/%s/%s", UVC_CONFIG_FS_STREAMING, UVC_FORMAT_MJPEG, "m");
        alogv("%s", path);
        configfs_para->format[index].format = V4L2_PIX_FMT_MJPEG;
        configfs_para->format[index].frame_num = parser_uvc_frame_num(path);
        if (configfs_para->format[index].frame_num > 0)
        {
            configfs_para->format[index].frame =
                malloc(sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            if (!configfs_para->format[index].frame)
                alogw("malloc MJPEG frame fail!");
            memset(configfs_para->format[index].frame,
                0, sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            parser_uvc_frame(path, configfs_para->format[index].frame);
            index++;
        }
    }
    if (configfs_para->format_support & UVC_FORMAT_YUYV_SUPPORT)
    {
        sprintf(path, "%s/%s/%s", UVC_CONFIG_FS_STREAMING, UVC_FORMAT_YUYV, "u");
        alogv("%s", path);
        configfs_para->format[index].format = V4L2_PIX_FMT_YUYV;
        configfs_para->format[index].frame_num = parser_uvc_frame_num(path);
        if (configfs_para->format[index].frame_num > 0)
        {
            configfs_para->format[index].frame =
                malloc(sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            if (!configfs_para->format[index].frame)
                alogw("malloc MJPEG frame fail!");
            memset(configfs_para->format[index].frame,
                0, sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            parser_uvc_frame(path, configfs_para->format[index].frame);
            index++;
        }
    }
    if (configfs_para->format_support & UVC_FORMAT_H264_SUPPORT)
    {
        sprintf(path, "%s/%s/%s", UVC_CONFIG_FS_STREAMING, UVC_FORMAT_H264, "h");
        alogv("%s", path);
        configfs_para->format[index].format = V4L2_PIX_FMT_H264;
        configfs_para->format[index].frame_num = parser_uvc_frame_num(path);
        if (configfs_para->format[index].frame_num > 0)
        {
            configfs_para->format[index].frame =
                malloc(sizeof(struct uvc_frame)*configfs_para->format[index].frame_num);
            if (!configfs_para->format[index].frame)
                alogw("malloc MJPEG frame fail!");
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
        aloge("impassible! don't find frame_index %d frame!", frame_index);
        frame = &format->frame[0];
    }
    return frame;
}
