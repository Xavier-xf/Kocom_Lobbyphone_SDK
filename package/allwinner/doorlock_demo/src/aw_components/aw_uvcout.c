#include "aw_uvcout.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define VENC_BUF_SIZE(W, H) ((W) * (H))
uvc_out_t *g_uvcout_info = NULL;

static uvc_frame_info_t default_mjpeg_frame_info[] = {
    {1, 1920, 1080, 30},
    {2, 1280, 720, 30},
    {3, 640, 480, 30},
    {4, 640, 360, 30},
};
static uvc_frame_info_t default_yuy2_frame_info[] = {
    {1, 1280, 720, 10},
    {2, 640, 480, 30},
    {3, 640, 360, 30},
};
static uvc_frame_info_t default_nv12_frame_info[] = {
    {1, 1280, 720, 10},
    {2, 640, 480, 30},
    {3, 640, 360, 30},
};
static uvc_frame_info_t default_h264_frame_info[] = {
    {1, 1920, 1080, 30},
    {2, 1280, 720, 30},
    {3, 640, 480, 30},
    {4, 640, 360, 30},
};

static uvc_format_info_t default_uvcout_format_data[] = {
    {1, V4L2_PIX_FMT_MJPEG, 1, sizeof(default_mjpeg_frame_info) / sizeof(uvc_frame_info_t),
     default_mjpeg_frame_info},
    {2, V4L2_PIX_FMT_YUYV, 2, sizeof(default_yuy2_frame_info) / sizeof(uvc_frame_info_t),
     default_yuy2_frame_info},
    {3, V4L2_PIX_FMT_H264, 1, sizeof(default_h264_frame_info) / sizeof(uvc_frame_info_t),
     default_h264_frame_info},
    {4, V4L2_PIX_FMT_NV12, 2, sizeof(default_nv12_frame_info) / sizeof(uvc_frame_info_t),
     default_nv12_frame_info},
};

void gen_set_uvc_function_script(uvc_out_t *uvcout_info)
{
    //  ==> /tmp/set_uvc_function.sh
    // uvc_out_t *uvcout_info = g_uvcout_info;
    char base_dir[256];
    char shell_cmd_buf[1024];
    uvc_format_info_t *format_info;
    uvc_frame_info_t *frame_info;
    int frame_num = 0;
    int format, frame;

    FILE *fp = fopen("/tmp/set_uvc_function.sh", "w+");
    if (fp == NULL) {
        DOORLOCK_ERR("fopen /tmp/set_uvc_function.sh failed\n");
        return;
    }

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    if (uvcout_info->usb_speed) {
        sprintf(shell_cmd_buf, "echo 1 > /sys/kernel/config/usb_gadget/g1/speedUSB \n");
    } else {
        sprintf(shell_cmd_buf, "echo 0 > /sys/kernel/config/usb_gadget/g1/speedUSB \n");
    }
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    if (uvcout_info->usb_speed) {
        sprintf(shell_cmd_buf, "echo 0x0200 > /sys/kernel/config/usb_gadget/g1/bcdUSB \n");
    } else {
        sprintf(shell_cmd_buf, "echo 0x0110 > /sys/kernel/config/usb_gadget/g1/bcdUSB \n");
    }
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf, "mkdir /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0 \n");
    // fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "echo %d > /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming_maxpacket \n",
            uvcout_info->uvc_max_streaming_size);
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "mkdir /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/header/h \n");
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    for (format = 0; format < uvcout_info->uvcout_format_cnt; format++) {
        DOORLOCK_DBG("format = %d ------------ \n", format);
        format_info = &uvcout_info->uvcout_format_frame_info[format];
        if (format_info == NULL) {
            DOORLOCK_ERR("format_info = NULL\n");
            continue;
        }
        frame = 0;
        for (frame = 0; frame < format_info->frame_cnt; frame++) {
            DOORLOCK_DBG("frame = %d ------------ \n", frame);
            frame_info = &format_info->frames[frame];
            if (frame_info == NULL) {
                DOORLOCK_ERR("frame_info = NULL\n");
                continue;
            }

            uint32_t interval = 1000 * 1000 * 10 / frame_info->fps;  // 100ns
            memset(base_dir, 0, sizeof(base_dir));
            if (format_info->format == V4L2_PIX_FMT_MJPEG) {
                sprintf(base_dir,
                        "/sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/mjpeg/m");
            } else if (format_info->format == V4L2_PIX_FMT_YUYV) {
                sprintf(
                    base_dir,
                    "/sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/uncompressed/u");
            } else if (format_info->format == V4L2_PIX_FMT_NV12) {
                sprintf(base_dir,
                        "/sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/nv12/n");
            } else if (format_info->format == V4L2_PIX_FMT_H264) {
                sprintf(base_dir,
                        "/sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/h264/h");
            }
            memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
            sprintf(shell_cmd_buf, "mkdir -p %s/%d_%d \n", base_dir, frame_info->width,
                    frame_info->height);
            fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

            memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
            sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/wWidth \n", frame_info->width, base_dir,
                    frame_info->width, frame_info->height);
            fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

            memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
            sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/wHeight \n", frame_info->height, base_dir,
                    frame_info->width, frame_info->height);
            fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

            memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
            sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwFrameInterval \n", interval, base_dir,
                    frame_info->width, frame_info->height);
            fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

            memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
            sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwDefaultFrameInterval \n", interval,
                    base_dir, frame_info->width, frame_info->height);
            fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

            memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
            if (format_info->format == V4L2_PIX_FMT_MJPEG) {
                sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMinBitRate \n",
                        frame_info->width * frame_info->height * frame_info->fps * 8, base_dir,
                        frame_info->width, frame_info->height);
            } else if (format_info->format == V4L2_PIX_FMT_YUYV) {
                sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMinBitRate \n",
                        frame_info->width * frame_info->height * 2 * 8 * frame_info->fps, base_dir,
                        frame_info->width, frame_info->height);
            } else if (format_info->format == V4L2_PIX_FMT_NV12) {
                sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMinBitRate \n",
                        frame_info->width * frame_info->height * 3 / 2 * 8 * frame_info->fps,
                        base_dir, frame_info->width, frame_info->height);
                // sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMinBitRate \n",
                // frame_info->width*frame_info->height*2*8*frame_info->fps, base_dir,
                // frame_info->width, frame_info->height);
            }
            fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

            memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
            if (format_info->format == V4L2_PIX_FMT_MJPEG) {
                sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMaxBitRate \n",
                        frame_info->width * frame_info->height * frame_info->fps * 8 * 2, base_dir,
                        frame_info->width, frame_info->height);
            } else if (format_info->format == V4L2_PIX_FMT_YUYV) {
                sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMaxBitRate \n",
                        frame_info->width * frame_info->height * 2 * 8 * frame_info->fps, base_dir,
                        frame_info->width, frame_info->height);
            } else if (format_info->format == V4L2_PIX_FMT_NV12) {
                sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMaxBitRate \n",
                        frame_info->width * frame_info->height * 3 / 2 * 8 * frame_info->fps,
                        base_dir, frame_info->width, frame_info->height);
                // sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMaxBitRate \n",
                // frame_info->width*frame_info->height*2*8*frame_info->fps, base_dir,
                // frame_info->width, frame_info->height);
            }
            fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

            memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
            if (format_info->format == V4L2_PIX_FMT_H264) {
                sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMaxVideoFrameBufferSize \n",
                        frame_info->width * frame_info->height * 2, base_dir, frame_info->width,
                        frame_info->height);
            } else {
                sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/dwMaxVideoFrameBufferSize \n",
                        frame_info->width * frame_info->height * 2, base_dir, frame_info->width,
                        frame_info->height);
            }
            fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

            memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
            sprintf(shell_cmd_buf, "echo %d > %s/%d_%d/bFrameIndex \n", frame + 1, base_dir,
                    frame_info->width, frame_info->height);
            fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
        }
        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        if (format_info->default_frame_index) {
            sprintf(shell_cmd_buf, "echo %d > %s/bDefaultFrameIndex \n",
                    format_info->default_frame_index, base_dir);
        }
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(
            shell_cmd_buf,
            "ln -s %s /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/header/h/ \n",
            base_dir);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
    }

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "ln -s /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/header/h/ "
            "/sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/class/fs \n");
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "ln -s /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/header/h/ "
            "/sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/streaming/class/hs \n");
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "mkdir /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/control/header/h \n");
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "ln -s /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/control/header/h/ "
            "/sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/control/class/fs/ \n");
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "ln -s /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/control/header/h/ "
            "/sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/control/class/ss/ \n");
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
    sprintf(shell_cmd_buf,
            "ln -s /sys/kernel/config/usb_gadget/g1/functions/uvc.usb0/ "
            "/sys/kernel/config/usb_gadget/g1/configs/c.1/uvc.usb0 \n");
    fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);

    if (uvcout_info->usb_vid) {
        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf, "echo 0x%04x > /sys/kernel/config/usb_gadget/g1/idVendor \n",
                uvcout_info->usb_vid);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
    }

    if (uvcout_info->usb_pid) {
        memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));
        sprintf(shell_cmd_buf, "echo 0x%04x > /sys/kernel/config/usb_gadget/g1/idProduct \n",
                uvcout_info->usb_pid);
        fwrite(shell_cmd_buf, 1, strlen(shell_cmd_buf), fp);
    }

    fclose(fp);
}

static void uvc_fill_streaming_control(struct uvc_streaming_control *stream_ctrl, int format, int frame)
{
    unsigned int frames;
    const uvc_format_info_t *format_info;
    const uvc_frame_info_t *frame_info;

    DOORLOCK_DBG("format %d, frame %d \n", format, frame);
    if (format < 0) format = g_uvcout_info->uvcout_format_cnt + format;
    if (format < 0 || format >= g_uvcout_info->uvcout_format_cnt) {
        DOORLOCK_ERR("format %d error\n", format);
        return;
    }
    DOORLOCK_DBG("final format %d\n", format);
    format_info = &g_uvcout_info->uvcout_format_frame_info[format];

    frames = format_info->frame_cnt;
    if (frame < 0) frame = frames + frame;
    if (frame < 0 || frame >= (int)frames) {
        DOORLOCK_ERR("frame %d error\n", frame);
        return;
    }
    DOORLOCK_DBG("final frame %d\n", frame);
    frame_info = &format_info->frames[frame];

    memset(stream_ctrl, 0, sizeof(struct uvc_streaming_control));

    /* 0: interval fixed
     * 1: keyframe rate fixed
     * 2: Pframe rate fixed
     */
    stream_ctrl->bmHint = 1;
    stream_ctrl->bFormatIndex = format + 1;
    stream_ctrl->bFrameIndex = frame + 1;
    stream_ctrl->dwFrameInterval = 1000 * 1000 * 10 / frame_info->fps;  // 100ns

    switch (format_info->format) {
    case V4L2_PIX_FMT_YUYV:
        stream_ctrl->dwMaxVideoFrameSize = frame_info->width * frame_info->height * 2;
        stream_ctrl->dwMaxPayloadTransferSize =
            g_uvcout_info->uvc_max_payload_size /* - UVC_PAYLOAD_HEADER_LEN*/;
        break;
    case V4L2_PIX_FMT_NV12:
        // stream_ctrl->dwMaxVideoFrameSize = frame_info->width * frame_info->height * 2;
        stream_ctrl->dwMaxVideoFrameSize = frame_info->width * frame_info->height * 3 / 2;
        stream_ctrl->dwMaxPayloadTransferSize = g_uvcout_info->uvc_max_payload_size /* - UVC_PAYLOAD_HEADER_LEN*/;
        break;
    case V4L2_PIX_FMT_MJPEG:
        stream_ctrl->dwMaxVideoFrameSize = VENC_BUF_SIZE(frame_info->width, frame_info->height);
        // fix bug when under android system, cannot preview video
        stream_ctrl->dwMaxPayloadTransferSize = g_uvcout_info->uvc_max_payload_size /* - UVC_PAYLOAD_HEADER_LEN*/;
        break;
    case V4L2_PIX_FMT_H264:
        stream_ctrl->dwMaxVideoFrameSize = VENC_BUF_SIZE(frame_info->width, frame_info->height);
        // fix bug when under android system, cannot preview video
        stream_ctrl->dwMaxPayloadTransferSize = g_uvcout_info->uvc_max_payload_size /* - UVC_PAYLOAD_HEADER_LEN*/;
        break;
    }
}

static int g_power_line_freq = 0;
static void uvc_event_setup_class_control(struct usb_ctrlrequest *crq,
                                        struct uvc_request_data *req_data)
{
    uint8_t uvc_req = crq->bRequest;
    uint8_t uvc_ctrl_set = crq->wValue >> 8;
    int control_data = 0;

    DOORLOCK_DBG("control request (req %02x cs %02x) wValue %04x wIndex %04x wLength %04x\n", uvc_req,
                uvc_ctrl_set, crq->wValue, crq->wIndex, crq->wLength);
    // req_data->data[0]
    // req_data->length = 0;
    memset(req_data->data, 0, crq->wLength);
    req_data->length = crq->wLength;

    /*
    brightness -126   126
    contrast   0    64
    saturation -256   512
    sharpness   0     1000
    hue         0     255
    gain        0     65535
    */

    if (crq->wIndex == 0x0100) {
        switch (crq->wValue >> 8) {
            switch (crq->bRequest) {
            case UVC_SET_CUR:
                break;
            case UVC_GET_DEF:
                break;
            case UVC_GET_CUR:
                break;
            case UVC_GET_MIN:
                break;
            case UVC_GET_MAX:
                break;
            case UVC_GET_RES:
                break;
            case UVC_GET_INFO:
                break;
            }
        }
    } else if (crq->wIndex == 0x0200) {
        switch (crq->bRequest) {
        case UVC_SET_CUR:
            break;
        case UVC_GET_DEF:
        case UVC_GET_CUR:
            switch (crq->wValue >> 8) {
            case UVC_PU_BACKLIGHT_COMPENSATION_CONTROL:  // 0x01
                break;
            case UVC_PU_BRIGHTNESS_CONTROL:  // 0x02 brightness
                // AW_MPI_ISP_GetBrightness(stContextUVC.mUVCInfo.iIspDev, &control_data);
                control_data += 126;
                break;
            case UVC_PU_CONTRAST_CONTROL:  // 0x03 contrast
                // AW_MPI_ISP_GetContrast(stContextUVC.mUVCInfo.iIspDev, &control_data);
                break;
            case UVC_PU_GAIN_CONTROL:  // 0x04 gain
                // AW_MPI_ISP_AE_GetGain(stContextUVC.mUVCInfo.iIspDev, &control_data);
                break;
            case UVC_PU_POWER_LINE_FREQUENCY_CONTROL:
                if (g_power_line_freq == 1) {
                    control_data += 1;  // 50hz
                } else {
                    control_data += 2;  // 60hz
                }
                break;
            case UVC_PU_HUE_CONTROL:  // 0x06 hue
                // AW_MPI_ISP_GetHue(stContextUVC.mUVCInfo.iIspDev, &control_data);
                break;
            case UVC_PU_SATURATION_CONTROL:  // 0x07 saturation
                control_data += 256;
                break;
            case UVC_PU_SHARPNESS_CONTROL:  // 0x08 sharpness
                // AW_MPI_ISP_GetSharpness(stContextUVC.mUVCInfo.iIspDev, &control_data);
                break;
            case UVC_PU_GAMMA_CONTROL:  // 0x09 gamma
                break;
            case UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL:  // 0x0a White Balance Temperature
                break;
            case UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL:  // 0x0b White Balance Temperature
                                                                 // Auto
                break;
            case UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL:  // 0x0c
                break;
            case UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL:  // 0x0d
                break;
            case UVC_PU_DIGITAL_MULTIPLIER_CONTROL:  // 0x0e
                break;
            case UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL:  // 0x0f
                break;
            case UVC_PU_HUE_AUTO_CONTROL:  // 0x10
                break;
            case UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL:  // 0x1
                break;
            case UVC_PU_ANALOG_LOCK_STATUS_CONTROL:  // 0x12
                break;
            }
            req_data->data[0] = control_data & 0x00ff;
            req_data->data[1] = (control_data >> 8) & 0x00ff;
            break;
        case UVC_GET_MIN:
            req_data->data[0] = 0x0;
            switch (crq->wValue >> 8) {
            case UVC_PU_POWER_LINE_FREQUENCY_CONTROL:
                req_data->data[0] = 0x1;
                break;
            }
            break;
        case UVC_GET_MAX:
            switch (crq->wValue >> 8) {
            case UVC_PU_BACKLIGHT_COMPENSATION_CONTROL:  // 0x01
                break;
            case UVC_PU_BRIGHTNESS_CONTROL:  // 0x02 brightness
                control_data = 126 + 126;
                break;
            case UVC_PU_CONTRAST_CONTROL:  // 0x03 contrast
                control_data = 0 + 64;
                break;
            case UVC_PU_GAIN_CONTROL:  // 0x04 gain
                control_data = 0 + 65535;
                break;
            case UVC_PU_POWER_LINE_FREQUENCY_CONTROL:
                control_data = 2;
                break;
            case UVC_PU_HUE_CONTROL:  // 0x06 hue
                control_data = 0 + 255;
                break;
            case UVC_PU_SATURATION_CONTROL:  // 0x07 saturation
                control_data = 256 + 512;
                break;
            case UVC_PU_SHARPNESS_CONTROL:  // 0x08 sharpness
                control_data = 0 + 1000;
                break;
            case UVC_PU_GAMMA_CONTROL:  // 0x09 gamma
                break;
            case UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL:  // 0x0a White Balance Temperature
                break;
            case UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL:  // 0x0b White Balance Temperature
                                                                 // Auto
                break;
            case UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL:  // 0x0c
                break;
            case UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL:  // 0x0d
                break;
            case UVC_PU_DIGITAL_MULTIPLIER_CONTROL:  // 0x0e
                break;
            case UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL:  // 0x0f
                break;
            case UVC_PU_HUE_AUTO_CONTROL:  // 0x10
                break;
            case UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL:  // 0x1
                break;
            case UVC_PU_ANALOG_LOCK_STATUS_CONTROL:  // 0x12
                break;
            }
            req_data->data[0] = control_data & 0x00ff;
            req_data->data[1] = (control_data >> 8) & 0x00ff;
            break;
        case UVC_GET_RES:
            req_data->data[0] = 1;
            break;
        case UVC_GET_INFO:
            req_data->data[0] = 0x03;
            /*
            D0  1=Supports GET value requestsCapability
            D1  1=Supports SET value requestsCapability
            D2  1=Disabled due to automatic mode(under device control)State
            D3  1=Autoupdate ControlCapability
            D4  1=Asynchronous ControlCapability
            D5  1=Disabled due to incompatibility with Commit state.State
            D7..D6保留，置为0
            */
            break;
        default:
            break;
        }
    }
    // must!!! in case control interface get def and set cur command with dummy data
    g_uvcout_info->ctrl_interface_set_cur = uvc_ctrl_set;
}

static int uvc_dump_streaming_control_data(struct uvc_streaming_control *stream_ctrl)
{
    int i = 0;
    uint8_t *buf_data = (uint8_t *)stream_ctrl;

    if (0) {
        DOORLOCK_INFO("\n");
        for (i = 0; i < sizeof(struct uvc_streaming_control); i++) {
            printf("%02x ", buf_data[i]);
        }
        printf("\n-----------------------------------------------------------------\n");
    }
    return 0;
}

static int uvc_video_resouce_process(int on)
{
    DOORLOCK_DBG("enter ===>on:%d\n", on);
    uvc_out_t *uvcout_info = g_uvcout_info;

    pthread_mutex_lock(&g_uvcout_info->uvcout_lock);
    if (on) {
        DOORLOCK_INFO("time to create UVC Resource\n");
        if (uvcout_info->is_resource_ok == 0) {
#if !UVC_TEST
            if (uvcout_info->resource_on_off) {
                uvcout_info->frm_manager.frm_node_memsize = uvcout_info->max_frame_size;
                DOORLOCK_INFO("frm_node_memsize = %d, frm_node_cnt = %d\n",
                             uvcout_info->frm_manager.frm_node_memsize,
                             uvcout_info->frm_manager.frm_node_cnt);
                frm_manager_init(&uvcout_info->frm_manager);
                uvcout_info->resource_on_off(uvcout_info, 1);
            } else {
                DOORLOCK_ERR("resource_on_off = NULL\n");
            }
#endif
        } else {
            DOORLOCK_WARN("UVC Resource already created\n");
        }

    } else {
        DOORLOCK_INFO("time to destroy UVC Resource\n");
        if (uvcout_info->is_resource_ok == 1) {
#if !UVC_TEST
            if (uvcout_info->resource_on_off) {
                uvcout_info->resource_on_off(uvcout_info, 0);
                frm_manager_deinit(&uvcout_info->frm_manager);
            } else {
                DOORLOCK_ERR("resource_on_off = NULL\n");
            }
#endif
        } else {
            DOORLOCK_WARN("UVC Resource already destroyed\n");
        }
    }
    pthread_mutex_unlock(&g_uvcout_info->uvcout_lock);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

static int uvc_request_buffers(int bufs_num)
{
    int ret;
    uvc_out_t *uvcout_info = g_uvcout_info;

    DOORLOCK_DBG("enter ===>\n");

    if (bufs_num > 0) {
        struct v4l2_requestbuffers req_buffs;
        memset(&req_buffs, 0, sizeof(struct v4l2_requestbuffers));
        req_buffs.count = bufs_num;
        req_buffs.memory = V4L2_MEMORY_MMAP;
        req_buffs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_REQBUFS, &req_buffs);
        if (ret < 0) {
            DOORLOCK_ERR("VIDIOC_REQBUFS failed %d, errno = %d, %s !!\n", ret, errno,
                        strerror(errno));
            goto reqbufs_err;
        }

        uvcout_info->uvc_frames = malloc(bufs_num * sizeof(uvc_frame_t));
        if (uvcout_info->uvc_frames == NULL) {
            DOORLOCK_ERR("malloc %d failed\n", bufs_num * sizeof(uvc_frame_t));
            goto reqbufs_err;
        }

        for (int i = 0; i < bufs_num; i++) {
            struct v4l2_buffer buffer;
            memset(&buffer, 0, sizeof(struct v4l2_buffer));
            buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;
            buffer.bytesused = 0;
            ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_QUERYBUF, &buffer);
            if (ret < 0) {
                DOORLOCK_ERR("VIDIOC_QUERYBUF failed!!\n");
            }

            uvcout_info->uvc_frames[i].vir_addr =
                (void *)mmap(0, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                             uvcout_info->uvcout_fd, buffer.m.offset);
            uvcout_info->uvc_frames[i].buf_len = buffer.length;
            uvcout_info->uvc_frames[i].phy_addr = (void *)buffer.m.offset;
            ioctl(uvcout_info->uvcout_fd, VIDIOC_QBUF, &buffer);

            DOORLOCK_INFO("[%d]VIDIOC_QUERYBUF virAddr=0x%08x, len=%d, phyAddr=0x%08x\n", i,
                         uvcout_info->uvc_frames[i].vir_addr, uvcout_info->uvc_frames[i].buf_len,
                         uvcout_info->uvc_frames[i].phy_addr);
            if (uvcout_info->uvc_frames[i].vir_addr == NULL) {
                DOORLOCK_ERR("mmap failed, vir_addr NULL, offset = 0x%x, length = %d!!\n",
                            buffer.m.offset, buffer.length);
            }
            memset(uvcout_info->uvc_frames[i].vir_addr, 128,
                   uvcout_info->uvc_frames[i].buf_len);  // default image

        }
        uvcout_info->bufs_num = bufs_num;
        // DOORLOCK_DBG("request [%d] buffers success\n", uvcout_info->bufs_num);
    }

reqbufs_err:
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

static int uvc_release_buffers(void)
{
    int ret = 0;
    uvc_out_t *uvcout_info = g_uvcout_info;

    if (uvcout_info->uvc_frames) {
        for (int i = 0; i < uvcout_info->bufs_num; i++) {
            munmap(uvcout_info->uvc_frames[i].vir_addr, uvcout_info->uvc_frames[i].buf_len);
        }
        free(uvcout_info->uvc_frames);
        uvcout_info->uvc_frames = NULL;
    }

    for (int i = 0; i < uvcout_info->bufs_num; i++) {
        struct v4l2_requestbuffers req_buffs;
        memset(&req_buffs, 0, sizeof(struct v4l2_requestbuffers));
        req_buffs.count = 0;
        req_buffs.memory = V4L2_MEMORY_MMAP;
        req_buffs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_REQBUFS, &req_buffs);
        if (ret < 0) {
            DOORLOCK_ERR("VIDIOC_REQBUFS failed %d, errno = %d, %s !!\n", ret, errno,
                        strerror(errno));
        } else {
            DOORLOCK_INFO("VIDIOC_REQBUFS ok, %d !!\n", i);
            break;
        }
        usleep(10 * 1000);
    }

    return ret;
}

static int uvc_event_stream_on(int on)
{
    int ret = 0;
    uvc_out_t *uvcout_info = g_uvcout_info;
    enum v4l2_buf_type buf_type;
    buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    DOORLOCK_DBG("enter ===>on:%d\n", on);
    pthread_mutex_lock(&g_uvcout_info->uvcout_lock);
    if (on) {
        if (g_uvcout_info->is_streaming == 0) {
            uvc_request_buffers(USB_FRAME_NUM);
            ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_STREAMON, &buf_type);
            if (ret == 0) {
                DOORLOCK_INFO("begin to streaming\n");
                g_uvcout_info->is_streaming = 1;
            } else {
                DOORLOCK_ERR("VIDIOC_STREAMON failed\n");
            }
        } else {
            DOORLOCK_INFO("already begin streaming\n");
        }
    } else {
        if (g_uvcout_info->is_streaming == 1) {
            g_uvcout_info->is_streaming = 0;
            ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_STREAMOFF, &buf_type);
            if (ret == 0) {
                DOORLOCK_INFO("stop to streaming\n");
            } else {
                DOORLOCK_ERR("VIDIOC_STREAMOFF failed\n");
            }
            uvc_release_buffers();
        } else {
            DOORLOCK_INFO("already stop streaming\n");
        }
    }
    pthread_mutex_unlock(&g_uvcout_info->uvcout_lock);
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

static int uvc_stream_on_off(int on_off)
{
    DOORLOCK_DBG("enter ===>on_off %d\n", on_off);
    DOORLOCK_INFO("uvc_stream_on_off [%d] %lld us\n", on_off, get_cur_time_us());
    if (on_off) {
        uvc_video_resouce_process(1);
        uvc_event_stream_on(1);
    } else {
        if (!g_uvcout_info->fast_connect) {
            uvc_video_resouce_process(0);
        }
        uvc_event_stream_on(0);
    }

    DOORLOCK_DBG("exit <===\n");
    return 0;
}

static int uvc_video_set_format(void)
{
    int ret;
    struct v4l2_format format;
    uvc_out_t *uvcout_info = g_uvcout_info;
    ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_G_FMT, &format);

    DOORLOCK_DBG("width=[%d],height=[%d],sizeimage=[0x%x], format[0x%x]\n", format.fmt.pix.width,
                format.fmt.pix.height, format.fmt.pix.sizeimage, format.fmt.pix.pixelformat);

    DOORLOCK_INFO("cap_width=[%d],cap_height=[%d],format_v4l2=[0x%08x]\n", uvcout_info->cap_width,
                 uvcout_info->cap_height, uvcout_info->format_v4l2);

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.pixelformat = uvcout_info->format_v4l2;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;

    format.fmt.pix.width = uvcout_info->cap_width;
    format.fmt.pix.height = uvcout_info->cap_height;
    format.fmt.pix.field = V4L2_FIELD_NONE;  // V4L2_FIELD_INTERLACED//
    // format.fmt.pix.field       = V4L2_FIELD_ANY;
    if (uvcout_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
        format.fmt.pix.sizeimage = uvcout_info->cap_width * uvcout_info->cap_height * 2;
    } else if (uvcout_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
        format.fmt.pix.sizeimage = uvcout_info->cap_width * uvcout_info->cap_height * 3 / 2;
        // format.fmt.pix.sizeimage = uvcout_info->cap_width * uvcout_info->cap_height * 2;
    } else {
        format.fmt.pix.sizeimage = VENC_BUF_SIZE(uvcout_info->cap_width, uvcout_info->cap_height);
    }

    DOORLOCK_INFO(
        "VIDIOC_S_FMT size[%dx%d] fmt[0x%x] mjpeg[0x%x] h264[0x%x] yuyv[0x%x] nv12[0x%x]\n",
        format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.pixelformat,
        V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_H264, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_NV12);
    ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_S_FMT, &format);
    if (ret < 0) {
        DOORLOCK_ERR("VIDIOC_S_FMT failed!! %s (%d).\n", strerror(errno), errno);
    }

    ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_G_FMT, &format);
    DOORLOCK_INFO("VIDIOC_G_FMT width=[%d],height=[%d],sizeimage=[%d], format[0x%x]\n",
                 format.fmt.pix.width, format.fmt.pix.height, format.fmt.pix.sizeimage,
                 format.fmt.pix.pixelformat);

    return ret;
}

static int uvc_event_setup_class_streaming(struct usb_ctrlrequest *crq,
                                         struct uvc_request_data *req_data)
{
    struct uvc_streaming_control *stream_ctrl;
    struct uvc_streaming_control *last_ctrl;
    uint8_t uvc_req = crq->bRequest;
    uint8_t uvc_ctrl_set = crq->wValue >> 8;

    // DOORLOCK_DBG("streaming request (req %02x cs %02x)\n", uvc_req, uvc_ctrl_set);

    if (uvc_ctrl_set != UVC_VS_PROBE_CONTROL && uvc_ctrl_set != UVC_VS_COMMIT_CONTROL)
        return 0;

    stream_ctrl = (struct uvc_streaming_control *)&req_data->data[0];

    if (uvc_ctrl_set == UVC_VS_PROBE_CONTROL) {
        last_ctrl = &g_uvcout_info->streaming_probe;
    } else {
        last_ctrl = &g_uvcout_info->streaming_commit;
    }
    req_data->length = crq->wLength;  // sizeof(struct uvc_streaming_control);

    switch (uvc_req) {
    case UVC_SET_CUR:
        // DOORLOCK_DBG("UVC_SET_CUR\n");
        g_uvcout_info->ctrl_interface_set_cur = uvc_ctrl_set;
        break;
    case UVC_GET_CUR:
        // DOORLOCK_DBG("UVC_GET_CUR\n");
        memcpy(stream_ctrl, last_ctrl, sizeof(struct uvc_streaming_control));
        if (stream_ctrl->dwMaxPayloadTransferSize == 0) {
            stream_ctrl->dwMaxPayloadTransferSize =
                g_uvcout_info->uvc_max_payload_size /* - UVC_PAYLOAD_HEADER_LEN*/;
            // stream_ctrl->dwMaxPayloadTransferSize = frame_info->width * frame_info->height;
            uvc_fill_streaming_control(stream_ctrl, last_ctrl->bFormatIndex - 1,
                                    last_ctrl->bFrameIndex - 1);
            DOORLOCK_DBG("Modify dwMaxPayloadTransferSize from 0 to %d!!!\n",
                        stream_ctrl->dwMaxPayloadTransferSize);  // fix bug for mac system no video.
        }
        uvc_dump_streaming_control_data(stream_ctrl);
        break;
    case UVC_GET_MIN:
        // DOORLOCK_DBG("UVC_GET_MIN\n");
        // uvc_fill_streaming_control(stream_ctrl, last_ctrl->bFormatIndex-1, -1);
        uvc_fill_streaming_control(stream_ctrl, last_ctrl->bFormatIndex - 1,
                                last_ctrl->bFrameIndex - 1);
        uvc_dump_streaming_control_data(stream_ctrl);
        break;
    case UVC_GET_MAX:
        // DOORLOCK_DBG("UVC_GET_MAX\n");
        // uvc_fill_streaming_control(stream_ctrl, last_ctrl->bFormatIndex-1, 0);
        uvc_fill_streaming_control(stream_ctrl, last_ctrl->bFormatIndex - 1,
                                last_ctrl->bFrameIndex - 1);
        uvc_dump_streaming_control_data(stream_ctrl);
        break;
    case UVC_GET_DEF: {
        // DOORLOCK_DBG("UVC_GET_DEF\n");
        uvc_format_info_t *format_info =
            &g_uvcout_info->uvcout_format_frame_info[last_ctrl->bFormatIndex - 1];
        uvc_fill_streaming_control(stream_ctrl, last_ctrl->bFormatIndex - 1,
                                g_uvcout_info->default_frame_index);
        uvc_dump_streaming_control_data(stream_ctrl);
        break;
    }
    case UVC_GET_RES:
        // DOORLOCK_DBG("UVC_GET_RES\n");
        memset(stream_ctrl, 0, sizeof(struct uvc_streaming_control));
        break;
    case UVC_GET_LEN:
        // DOORLOCK_DBG("UVC_GET_LEN\n");
        req_data->data[0] = 0x00;
        req_data->data[1] = req_data->length;
        req_data->length = 2;
        break;
    case UVC_GET_INFO:
        // DOORLOCK_DBG("UVC_GET_INFO\n");
        req_data->data[0] = 0x03;
        req_data->length = 1;
        break;
    }

    return 0;
}

static int uvc_event_data_out(struct usb_ctrlrequest *crq, struct uvc_request_data *req_data)
{
    struct uvc_streaming_control *target_control;
    const uvc_format_info_t *format_info;
    const uvc_frame_info_t *frame_info;
    unsigned int fps;
    unsigned int format_index, frame_index;
    unsigned int frames;

    // DOORLOCK_DBG("iCtrlSetCur = 0x%02x, data length=%d\n", g_uvcout_info->ctrl_interface_set_cur,
    // req_data->length);
    if (req_data->length == 2 || req_data->length == 1) {
        int control_data = 0;
        if (req_data->length == 1) {
            // DOORLOCK_DBG("data: 0x%02x\n", req_data->data[0]);
            control_data = req_data->data[0];
        }
        if (req_data->length == 2) {
            // DOORLOCK_DBG("data: 0x%02x, 0x%02x\n", req_data->data[0], req_data->data[1]);
            control_data = req_data->data[0] + (req_data->data[1] * 256);
        }
        switch (g_uvcout_info->ctrl_interface_set_cur) {
            /*
            brightness -126   126
            contrast   0    64
            saturation -256   512
            sharpness   0     1000
            hue         0     255
            gain        0     65535
            */
        case UVC_PU_BACKLIGHT_COMPENSATION_CONTROL:  // 0x01
            break;
        case UVC_PU_BRIGHTNESS_CONTROL:  // 0x02 brightness
            // AW_MPI_ISP_SetBrightness(stContextUVC.mUVCInfo.iIspDev, control_data - 126);
            break;
        case UVC_PU_CONTRAST_CONTROL:  // 0x03 contrast
            // AW_MPI_ISP_SetContrast(stContextUVC.mUVCInfo.iIspDev, control_data);
            break;
        case UVC_PU_GAIN_CONTROL:  // 0x04 gain
            // AW_MPI_ISP_AE_SetGain(stContextUVC.mUVCInfo.iIspDev, control_data);
            break;
        case UVC_PU_POWER_LINE_FREQUENCY_CONTROL:
            g_power_line_freq = control_data & 0x00ff;
            // AW_MPI_ISP_SetFlicker(stContextUVC.mUVCInfo.iIspDev, g_power_line_freq);//
            // [0:disable,1:50,2:60,3:auto]
            break;
        case UVC_PU_HUE_CONTROL:  // 0x06 hue
            // AW_MPI_ISP_SetHue(stContextUVC.mUVCInfo.iIspDev, control_data);
            break;
        case UVC_PU_SATURATION_CONTROL:  // 0x07 saturation
            // AW_MPI_ISP_SetSaturation(stContextUVC.mUVCInfo.iIspDev, control_data - 256);
            break;
        case UVC_PU_SHARPNESS_CONTROL:  // 0x08 sharpness
            // AW_MPI_ISP_SetSharpness(stContextUVC.mUVCInfo.iIspDev, control_data);
            break;
        case UVC_PU_GAMMA_CONTROL:  // 0x09 gamma
            break;
        case UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL:  // 0x0a White Balance Temperature
            break;
        case UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL:  // 0x0b White Balance Temperature Auto
            break;
        case UVC_PU_WHITE_BALANCE_COMPONENT_CONTROL:  // 0x0c
            break;
        case UVC_PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL:  // 0x0d
            break;
        case UVC_PU_DIGITAL_MULTIPLIER_CONTROL:  // 0x0e
            break;
        case UVC_PU_DIGITAL_MULTIPLIER_LIMIT_CONTROL:  // 0x0f
            break;
        case UVC_PU_HUE_AUTO_CONTROL:  // 0x10
            break;
        case UVC_PU_ANALOG_VIDEO_STANDARD_CONTROL:  // 0x11
            break;
        case UVC_PU_ANALOG_LOCK_STATUS_CONTROL:  // 0x12
            break;
        default:
            DOORLOCK_ERR("unknown, ctrl_interface_set_cur = 0x%02x\n", g_uvcout_info->ctrl_interface_set_cur);
            break;
        }
        return 0;
    }

    if (req_data->length != 26 && req_data->length != sizeof(struct uvc_streaming_control)) {
        DOORLOCK_ERR("data length=%d, not right!!!\n", req_data->length);
        return 0;
    }

    switch (g_uvcout_info->ctrl_interface_set_cur) {
    case UVC_VS_PROBE_CONTROL:
        DOORLOCK_DBG("setting probe control, length = %d\n", req_data->length);
        target_control = &g_uvcout_info->streaming_probe;
        break;

    case UVC_VS_COMMIT_CONTROL:
        DOORLOCK_DBG("setting commit control, length = %d\n", req_data->length);
        target_control = &g_uvcout_info->streaming_commit;
        break;

    default:
        DOORLOCK_ERR("setting unknown control, length = %d\n", req_data->length);
        return 0;
    }

    struct uvc_streaming_control *stream_ctrl;
    stream_ctrl = (struct uvc_streaming_control *)&req_data->data[0];
    //DOORLOCK_DBG("bmHint[%d], bFormatIndex[%d], bFrameIndex[%d], dwFrameInterval[%d], \
        dwMaxVideoFrameSize[%d], dwMaxPayloadTransferSize[%d]", stream_ctrl->bmHint, stream_ctrl->bFormatIndex, \
        stream_ctrl->bFrameIndex, stream_ctrl->dwFrameInterval, stream_ctrl->dwMaxVideoFrameSize, stream_ctrl->dwMaxPayloadTransferSize);

    uvc_dump_streaming_control_data(stream_ctrl);
    // format_index = clamp((unsigned int)stream_ctrl->bFormatIndex, 1U, (unsigned
    // int)ARRAY_SIZE(uvcout_format_data));
    format_index = (unsigned int)stream_ctrl->bFormatIndex;
    format_info = &g_uvcout_info->uvcout_format_frame_info[format_index - 1];

    frames = format_info->frame_cnt;

    // frame_index = clamp((unsigned int)stream_ctrl->bFrameIndex, 1U, frames);
    frame_index = (unsigned int)stream_ctrl->bFrameIndex;
    frame_info = &format_info->frames[frame_index - 1];
    fps = frame_info->fps;

    memcpy(target_control, stream_ctrl, sizeof(struct uvc_streaming_control));

    if (g_uvcout_info->ctrl_interface_set_cur == UVC_VS_COMMIT_CONTROL) {
        DOORLOCK_INFO(
            "UVC_VS_COMMIT_CONTROL, FORMAT = 0x%08x, format_index = %d, frame_index = %d, width = "
            "%d, height = %d\n",
            format_info->format, format_index, frame_index, frame_info->width, frame_info->height);

        int old_format_index = g_uvcout_info->format_index;
        int old_frame_index = g_uvcout_info->frame_index;

        DOORLOCK_INFO("old format index[%d], frame index[%d]\n", old_format_index, old_frame_index);
        DOORLOCK_INFO("new format index[%d], frame index[%d]\n", stream_ctrl->bFormatIndex,
                     stream_ctrl->bFrameIndex);

#if !UVC_TEST
        // in bulk transter mode, there is no set interface (stream on/off) cmd at all.
        if (g_uvcout_info->bulk_mode) {
            uvc_stream_on_off(0);
        }

        // when fast connect enabled, we must only destroy resource here!!!
        if (g_uvcout_info->fast_connect) {
            if ((old_format_index != stream_ctrl->bFormatIndex) ||
                (old_frame_index != stream_ctrl->bFrameIndex)) {
                uvc_video_resouce_process(0);
            }
        }
#endif

        g_uvcout_info->format_v4l2 = format_info->format;  // V4L2_PIX_FMT_YUYV;
        g_uvcout_info->format_index = stream_ctrl->bFormatIndex;
        g_uvcout_info->frame_index = stream_ctrl->bFrameIndex;
        g_uvcout_info->frame_rate = frame_info->fps;  //
        g_uvcout_info->cap_width = frame_info->width;
        g_uvcout_info->cap_height = frame_info->height;
        g_uvcout_info->rotate = frame_info->rotate_flag;
        if (g_uvcout_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
            g_uvcout_info->max_frame_size =
                ALIGN_16B(frame_info->width) * ALIGN_16B(frame_info->height) * 2;
            DOORLOCK_INFO("V4L2_PIX_FMT_YUYV !!!\n");
        } else if (g_uvcout_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
            g_uvcout_info->max_frame_size =
                ALIGN_16B(frame_info->width) * ALIGN_16B(frame_info->height) * 3 / 2;
            // g_uvcout_info->max_frame_size =
            // ALIGN_16B(frame_info->width)*ALIGN_16B(frame_info->height)*2;
            DOORLOCK_INFO("V4L2_PIX_FMT_NV12 !!!\n");
        } else if (g_uvcout_info->format_v4l2 == V4L2_PIX_FMT_MJPEG) {
            g_uvcout_info->max_frame_size =
                ALIGN_16B(frame_info->width) * ALIGN_16B(frame_info->height);  // VENC_BUF_SIZE
            DOORLOCK_INFO("V4L2_PIX_FMT_MJPEG !!!\n");
        } else if (g_uvcout_info->format_v4l2 == V4L2_PIX_FMT_H264) {
            g_uvcout_info->max_frame_size =
                ALIGN_16B(frame_info->width) * ALIGN_16B(frame_info->height);
            DOORLOCK_INFO("V4L2_PIX_FMT_H264 !!!\n");
        }

#if !UVC_TEST
        uvc_video_set_format();
        // in bulk transter mode, there is no set interface (stream on/off) cmd at all.
        if (g_uvcout_info->bulk_mode) {
            uvc_stream_on_off(1);
        }
#endif

#if 0  //! UVC_TEST
       // create resource when in fast connect mode
		if(g_uvcout_info->fast_connect) {
			if((old_format_index != stream_ctrl->bFormatIndex) || (old_frame_index != stream_ctrl->bFrameIndex)) {
                uvc_video_resouce_process(1);
			}
		}
#endif
    }

    return 0;
}

int uvc_transfer_video_frame(uvc_out_t *uvcout_info, uint8_t *data_buf, uint32_t data_size)
{
    int ret = 0;
#if !UVC_TEST
    pthread_mutex_lock(&uvcout_info->uvcout_lock);
    if ((uvcout_info->uvcout_fd != 0) && (uvcout_info->is_streaming != false)) {
        // DOORLOCK_DBG("data_size = %d\n", data_size);
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(struct v4l2_buffer));
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buffer.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_DQBUF, &buffer);
        if (ret < 0) {
            DOORLOCK_DBG("Unable to dequeue buffer: %s (%d).\n", strerror(errno), errno);
            pthread_mutex_unlock(&uvcout_info->uvcout_lock);
            return -1;
        }

        if (uvcout_info->uvc_frames[buffer.index].buf_len < data_size) {
            DOORLOCK_ERR("data_size %d, buf_len = %d, buffer.index = %d\n", data_size,
                        uvcout_info->uvc_frames[buffer.index].buf_len, buffer.index);
            data_size = uvcout_info->uvc_frames[buffer.index].buf_len;
        }
        /* fill the v4l2 buffer */
        memcpy(uvcout_info->uvc_frames[buffer.index].vir_addr, data_buf, data_size);
        buffer.bytesused = data_size;
        int loop_cnt = 0;

loop:
        ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_QBUF, &buffer);
        if (ret < 0) {
            DOORLOCK_ERR("[%d] Unable to queue buffer: %s (%d).\n", loop_cnt, strerror(errno),
                        errno);
            if (loop_cnt < 5) {
                loop_cnt++;
                usleep(10 * 1000);
                goto loop;
            }
        }
    }
#endif
    pthread_mutex_unlock(&uvcout_info->uvcout_lock);

    return ret;
}

static int uvc_consume_video_frame(uvc_out_t *uvcout_info)
{
    int ret = 0;
#if !UVC_TEST
    uint8_t *data_buf = NULL;
    frm_manager_t *frm_manager = &uvcout_info->frm_manager;
    frame_mem_t *buf_tmp = NULL;

    ret = frm_manager->prefetch_first_using_frame(frm_manager, &buf_tmp);
    if (buf_tmp == NULL || ret < 0) {
        // DOORLOCK_DBG("frm_manager->prefetch_first_using_frame fail\n");
        return -1;
    }

    if (buf_tmp->cur_mem_size > 0 && buf_tmp->cur_mem_size <= uvcout_info->max_frame_size) {
        ret = uvc_transfer_video_frame(uvcout_info, buf_tmp->mem_info.mem_vir, buf_tmp->cur_mem_size);
    } else {
        DOORLOCK_ERR("cur size = %d, max size = %d\n", buf_tmp->cur_mem_size,
                    uvcout_info->max_frame_size);
        ret = -1;
    }
    if (ret == 0) {
        frm_manager->first_using_to_idle_frame(frm_manager, buf_tmp);
    }
#endif
    return ret;
}

static int uvc_video_buf_process(uvc_out_t *uvcout_info)
{
    return uvc_consume_video_frame(uvcout_info);
}

static int uvc_event_setup_class(struct uvc_event *event, struct uvc_request_data *req_data)
{
    if ((event->req.bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
        return 0;

    switch (event->req.wIndex & 0xff) {
    case UVC_INTF_CONTROL:
        DOORLOCK_DBG("*******************************intfctonrol\n");
        uvc_event_setup_class_control(&event->req, req_data);
        break;

    case UVC_INTF_STREAMING:
        DOORLOCK_DBG("*******************************intfstreaming\n");
        uvc_event_setup_class_streaming(&event->req, req_data);
        break;

    default:
        break;
    }

    return 0;
}

static int uvc_event_setup(struct uvc_event *event, struct uvc_request_data *req_data)
{
    switch (event->req.bRequestType & USB_TYPE_MASK) {
    /* USB_TYPE_STANDARD: kernel driver will process it */
    case USB_TYPE_STANDARD:
    case USB_TYPE_VENDOR:
        DOORLOCK_ERR("do not care\n");
        break;
    case USB_TYPE_CLASS:
        uvc_event_setup_class(event, req_data);
        break;

    default:
        break;
    }

    return 0;
}

static int uvc_event_process(uvc_out_t *uvcout_info)
{
    int ret;
    pthread_t tid = 0;
    int on_off = 0;
    struct v4l2_event event;
    struct uvc_event *uvcevent = (struct uvc_event *)&event.u.data[0];
    struct uvc_request_data uvc_req;
    memset(&uvc_req, 0, sizeof(struct uvc_request_data));
    uvc_req.length = -EL2HLT;

    ret = ioctl(uvcout_info->uvcout_fd, VIDIOC_DQEVENT, &event);
    if (ret < 0) {
        // DOORLOCK_ERR("queue event failed!!\n");
        goto event_err;
    }

    DOORLOCK_DBG("event is 0x%x\n", event.type);
    switch (event.type) {
    case UVC_EVENT_CONNECT:
        DOORLOCK_DBG("uvc event connect.\n");
        break;
    case UVC_EVENT_DISCONNECT:
        DOORLOCK_DBG("uvc event disconnect.\n");
        break;
    case UVC_EVENT_STREAMON:
        DOORLOCK_WARN("uvc event stream on.\n");
        uvc_stream_on_off(1);
        goto event_err;
    case UVC_EVENT_STREAMOFF:
        DOORLOCK_WARN("uvc event stream off.\n");
        uvc_stream_on_off(0);
        goto event_err;
    case UVC_EVENT_SETUP:
        DOORLOCK_DBG("uvc event setup.\n");
        uvc_event_setup(uvcevent, &uvc_req);
        break;
    case UVC_EVENT_DATA:
        DOORLOCK_DBG("uvc event data.\n");
        uvc_event_data_out(&uvcevent->req, &uvcevent->data);
        break;

    default:
        break;
    }

    ioctl(uvcout_info->uvcout_fd, UVCIOC_SEND_RESPONSE, &uvc_req);

event_err:
    return ret;
}

static int uvc_subscribe(uvc_out_t *uvcout_info)
{
    DOORLOCK_DBG("enter ===>\n");
    struct v4l2_capability capability;

    if (ioctl(uvcout_info->uvcout_fd, VIDIOC_QUERYCAP, &capability) < 0) {
        DOORLOCK_ERR("unable to query uvc device: %s (%d)\n", strerror(errno), errno);
        return -1;
    }
    DOORLOCK_DBG("device is %s on bus %s\n", capability.card, capability.bus_info);
    struct v4l2_event_subscription subscription;
    /* subscribe events */
    memset(&subscription, 0, sizeof subscription);
    subscription.type = UVC_EVENT_FIRST;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_SUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_CONNECT;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_SUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_DISCONNECT;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_SUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_STREAMON;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_SUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_STREAMOFF;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_SUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_SETUP;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_SUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_DATA;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_SUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_LAST;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_SUBSCRIBE_EVENT, &subscription);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

static int uvc_un_subscribe(uvc_out_t *uvcout_info)
{
    DOORLOCK_DBG("enter ===>\n");
    struct v4l2_event_subscription subscription;

    memset(&subscription, 0, sizeof subscription);
    subscription.type = UVC_EVENT_FIRST;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_UNSUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_CONNECT;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_UNSUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_DISCONNECT;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_UNSUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_STREAMON;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_UNSUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_STREAMOFF;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_UNSUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_SETUP;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_UNSUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_DATA;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_UNSUBSCRIBE_EVENT, &subscription);
    subscription.type = UVC_EVENT_LAST;
    ioctl(uvcout_info->uvcout_fd, VIDIOC_UNSUBSCRIBE_EVENT, &subscription);

    DOORLOCK_DBG("exit <===\n");
    return 0;
}

static int uvc_find_dev_name(char *name_buf, uint32_t name_max_size)
{
    FILE *stream =
        popen("ls -l /sys/class/video4linux |grep \"udc.*gadget\"|awk '{print $9}'", "r");
    if (stream == NULL) {
        DOORLOCK_ERR("popen failed !!!\n");
        return -1;
    }
    memset(name_buf, 0, name_max_size);
    fread(name_buf, sizeof(char), name_max_size - 1, stream);
    pclose(stream);

    if (memcmp(name_buf, "video", 5) == 0) {
        if (name_buf[strlen(name_buf) - 1] == '\n' || name_buf[strlen(name_buf) - 1] == '\r') {
            name_buf[strlen(name_buf) - 1] = 0;
        }
        DOORLOCK_INFO("find target uvc video name:[%s] !!!\n", name_buf);
        return 0;
    }
    return -1;
}

static void *fast_connect_thread(void *arg)
{
    DOORLOCK_DBG("enter ===>\n");
    uvc_out_t *uvcout_info = (uvc_out_t *)arg;

    uvc_out_fast_setup(uvcout_info);
    DOORLOCK_DBG("exit <===\n");
}

static void *uvc_out_thread(void *arg)
{
    DOORLOCK_DBG("enter ===>\n");
    uvc_out_t *uvcout_info = (uvc_out_t *)arg;
    int ret = 0;

    // cat /sys/class/video4linux/video1/name  ==> sunxi_usb_udc
    // ls -l /sys/class/video4linux |grep "udc.*gadget"|awk '{print $9}'  ==> video1
    if (strlen(uvcout_info->uvc_dev_name) == 0) {
        while (1) {
            char tmp_name[256];
            if (uvc_find_dev_name(tmp_name, sizeof(tmp_name)) == 0) {
                sprintf(uvcout_info->uvc_dev_name, "/dev/%s", tmp_name);
                break;
            }
            if (uvcout_info->force_exit) {
                DOORLOCK_INFO("time to force exit UVC\n");
                goto exit;
            }
            usleep(1 * 1000);
        }
    }

    while (1) {
        if (uvcout_info->force_exit) {
            DOORLOCK_INFO("time to force exit UVC\n");
            goto exit;
        }
        uvcout_info->uvcout_fd = open(uvcout_info->uvc_dev_name, O_RDWR | O_NONBLOCK);
        if (uvcout_info->uvcout_fd < 0) {
            //DOORLOCK_ERR("open video device failed: device[%s]\n", uvcout_info->uvc_dev_name);
            //goto exit;
        } else if (uvcout_info->uvcout_fd > 0) {
            DOORLOCK_INFO("open video device [%s] success\n", uvcout_info->uvc_dev_name);
            break;
        }
        usleep(1 * 1000);
    }

    uvc_subscribe(uvcout_info);

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(uvcout_info->uvcout_fd, &fds);

    // DOORLOCK_INFO("uvc_out_thread start loop, %lld us\n", get_cur_time_us());
    while (1) {
        fd_set write_fds = fds;
        fd_set except_fds = fds;
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1 * 1000;

        if (uvcout_info->force_exit) {
            DOORLOCK_INFO("time to force exit UVC\n");
            uvc_stream_on_off(0);
            break;
        }

        ret = select(uvcout_info->uvcout_fd + 1, NULL, &write_fds, &except_fds, &timeout);
        if (ret == 0) {
            continue;
        }
        if (FD_ISSET(uvcout_info->uvcout_fd, &except_fds)) {
            uvc_event_process(uvcout_info);
        }
        if (FD_ISSET(uvcout_info->uvcout_fd, &write_fds) && uvcout_info->is_streaming &&
            (uvcout_info->is_resource_ok == 1)) {
            uvc_video_buf_process(uvcout_info);
        }
        usleep(2 * 1000);
    }

    uvc_un_subscribe(uvcout_info);
    if (uvcout_info->uvcout_fd) {
        close(uvcout_info->uvcout_fd);
        uvcout_info->uvcout_fd = 0;
    }

exit:
    DOORLOCK_DBG("exit <===\n");
}

int uvc_out_init(uvc_out_t *uvcout_info)
{
    DOORLOCK_DBG("enter ===>\n");
    int ret = 0;
    g_uvcout_info = uvcout_info;
    if (uvcout_info->uvcout_format_frame_info == NULL) {
        uvcout_info->uvcout_format_frame_info = default_uvcout_format_data;
        uvcout_info->uvcout_format_cnt =
            sizeof(default_uvcout_format_data) / sizeof(uvc_format_info_t);
    }

    uvcout_info->is_streaming = false;
    uvcout_info->force_exit = false;
    uvcout_info->is_resource_ok = false;
    uvcout_info->ctrl_interface_set_cur = 0;
    uvcout_info->tansfer_frame_cnt = 0;

    if (uvcout_info->bulk_mode) {
        if (uvcout_info->usb_speed) {
            uvcout_info->uvc_max_streaming_size = 512;
            uvcout_info->uvc_max_payload_size = 512;
        } else {
            uvcout_info->uvc_max_streaming_size = 64;
            uvcout_info->uvc_max_payload_size = 64;
        }
    } else {
        if (uvcout_info->usb_speed) {
            uvcout_info->uvc_max_streaming_size = 2048;
        } else {
            uvcout_info->uvc_max_streaming_size = 512;
        }
        uvcout_info->uvc_max_payload_size = uvcout_info->uvc_max_streaming_size;
    }
    DOORLOCK_WARN(
        "UVC bulk_mode = %d, usb_speed = %d, uvc_max_payload_size = %d, uvc_max_streaming_size = "
        "%d\n",
        uvcout_info->bulk_mode, uvcout_info->usb_speed, uvcout_info->uvc_max_payload_size,
        uvcout_info->uvc_max_streaming_size);

    pthread_mutex_init(&uvcout_info->uvcout_lock, NULL);
    aw_mem_open();
    DOORLOCK_INFO("uvc dev name [%s]\n", uvcout_info->uvc_dev_name);

    gen_set_uvc_function_script(uvcout_info);

    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int uvc_out_run(uvc_out_t *uvcout_info)
{
    DOORLOCK_DBG("enter ===>\n");
    int ret = pthread_create(&uvcout_info->uvcout_thread, NULL, uvc_out_thread, (void *)uvcout_info);

    if (ret != 0) {
        DOORLOCK_ERR("pthread_create failed ret[%d].\n", ret);
        uvcout_info->uvcout_thread = 0;
    } else {
        char thread_name[50];
        memset(thread_name, 0, sizeof(thread_name));
        sprintf(thread_name, "uvc_out_thread");
        pthread_setname_np(uvcout_info->uvcout_thread, thread_name);
    }

#if !UVC_TEST
    if (uvcout_info->fast_connect) {
        pthread_t tid;
        DOORLOCK_DBG("fast connect thread create\n");
        int ret = pthread_create(&tid, NULL, fast_connect_thread, (void *)uvcout_info);
        if (ret != 0) {
            DOORLOCK_ERR("pthread_create failed ret[%d].\n", ret);
            tid = 0;
        } else {
            char thread_name[50];
            memset(thread_name, 0, sizeof(thread_name));
            sprintf(thread_name, "uvc_fast_connect_thread");
            pthread_setname_np(tid, thread_name);
        }
    }
#endif
    DOORLOCK_DBG("exit <===\n");
}

int uvc_out_deinit(uvc_out_t *uvcout_info)
{
    DOORLOCK_DBG("enter ===>\n");
    if (uvcout_info->resource_on_off) {
        uvcout_info->resource_on_off(g_uvcout_info, 0);
    }
    if (uvcout_info->force_exit == false) {
        uvcout_info->force_exit = true;
        if (uvcout_info->uvcout_thread) {
            pthread_join(uvcout_info->uvcout_thread, NULL);
            uvcout_info->uvcout_thread = 0;
        }
    }

    aw_mem_close();
    pthread_mutex_destroy(&uvcout_info->uvcout_lock);
    g_uvcout_info = NULL;
    DOORLOCK_DBG("exit <===\n");

    return 0;
}

int uvc_out_connect(uvc_out_t *uvcout_info)
{
    DOORLOCK_DBG("enter ===>\n");
    if (uvcout_info->bulk_mode) {
        system("/bin/setusbconfig uvc bulk");
    } else {
        system("/bin/setusbconfig uvc");
    }
    // system("cat /sys/devices/platform/soc/usbc0/usb_device");
    DOORLOCK_DBG("exit <===\n");

    return 0;
}

int uvc_out_disconnect(uvc_out_t *uvcout_info)
{
    DOORLOCK_DBG("enter ===>\n");
    system("/bin/setusbconfig none");
    DOORLOCK_DBG("exit <===\n");

    return 0;
}

int uvc_out_force_exit(uvc_out_t *uvcout_info)
{
    DOORLOCK_DBG("enter ===>\n");
    if (uvcout_info->force_exit == false) {
        uvcout_info->force_exit = true;
        if (uvcout_info->uvcout_thread) {
            pthread_join(uvcout_info->uvcout_thread, NULL);
            uvcout_info->uvcout_thread;
        }
    }
    // uvc_out_disconnect(uvcout_info);
    // uvc_out_deinit(uvcout_info);
    DOORLOCK_DBG("exit <===\n");

    return 0;
}

int uvc_out_fast_setup(uvc_out_t *uvcout_info)
{
    DOORLOCK_DBG("enter ===>\n");
    unsigned int format_index = 1;
    unsigned int frame_index = 1;
    const uvc_format_info_t *format_info;
    const uvc_frame_info_t *frame_info;
    unsigned int fps;
    unsigned int frames;

    format_index = g_uvcout_info->default_format_index;
    frame_index = g_uvcout_info->default_frame_index;
    format_info = &g_uvcout_info->uvcout_format_frame_info[format_index - 1];
    frames = format_info->frame_cnt;
    frame_info = &format_info->frames[frame_index - 1];
    fps = frame_info->fps;

    DOORLOCK_INFO("uvc_out_fast_setup\n");
    DOORLOCK_INFO("FORMAT = 0x%08x, format_index = %d, frame_index = %d, width = %d, height = %d\n",
                 format_info->format, format_index, frame_index, frame_info->width,
                 frame_info->height);
    g_uvcout_info->format_v4l2 = format_info->format;  // V4L2_PIX_FMT_YUYV;
    g_uvcout_info->format_index = format_index;
    g_uvcout_info->frame_index = frame_index;
    g_uvcout_info->frame_rate = frame_info->fps;
    g_uvcout_info->cap_width = frame_info->width;
    g_uvcout_info->cap_height = frame_info->height;
    g_uvcout_info->rotate = frame_info->rotate_flag;
    if (g_uvcout_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
        g_uvcout_info->max_frame_size =
            ALIGN_16B(frame_info->width) * ALIGN_16B(frame_info->height) * 2;
        DOORLOCK_INFO("V4L2_PIX_FMT_YUYV !!!\n");
    } else if (g_uvcout_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
        g_uvcout_info->max_frame_size =
            ALIGN_16B(frame_info->width) * ALIGN_16B(frame_info->height) * 3 / 2;
        DOORLOCK_INFO("V4L2_PIX_FMT_NV12 !!!\n");
    } else if (g_uvcout_info->format_v4l2 == V4L2_PIX_FMT_MJPEG) {
        g_uvcout_info->max_frame_size =
            ALIGN_16B(frame_info->width) * ALIGN_16B(frame_info->height);
        DOORLOCK_INFO("V4L2_PIX_FMT_MJPEG !!!\n");
    } else if (g_uvcout_info->format_v4l2 == V4L2_PIX_FMT_H264) {
        g_uvcout_info->max_frame_size =
            ALIGN_16B(frame_info->width) * ALIGN_16B(frame_info->height);
        DOORLOCK_INFO("V4L2_PIX_FMT_H264 !!!\n");
    }

    uvc_video_resouce_process(1);

    DOORLOCK_DBG("exit <===\n");

    return 0;
}
