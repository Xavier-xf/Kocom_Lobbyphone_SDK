#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_DEBUG

#include "utils/sys/include/sys_linux_ioctl.h"
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#if VIDEO_SOURCE_MPP
#include <include/media/mpi_sys.h>
#elif VIDEO_SOURCE_RT_MEDIA
#include <rt_media/AW_VideoInput_API.h>
#endif

#include "uac/include/uac.h"
#include "utils/debug/include/debug.h"
#include "uvc/libuvc/include/stream.h"
#include "utils/option/include/option.h"
#include "utils/libevent/include/demo_events.h"
#include "uvc/video-source/base/include/video_source.h"
#include "utils/configfs_parser/include/configfs_parser.h"

struct uvc_demo_device {
    int index;
    uvc_demo_config config;
    struct uvc_stream *stream;
    struct uvc_configfs_para *fc;
    struct video_source *video_src;
};

struct uvc_demo_context {
    struct demo_events events;

    struct uvc_demo_device device;
    struct uvc_demo_device dual_device;
};

static struct demo_events *sigint_events;

static void print_support_format(struct uvc_configfs_para *fc)
{
    for (int i = 0; i < fc->format_num; i++)
    {
        for (int j = 0; j < fc->format[i].frame_num; j++)
        {
            logd("support foramtIndex:%d, type:0x%x, frameIndex:%d, videoSize:%dx%d, frameInterval:%d-%d x100ns, bitRate:%d-%d, others:%d,%d",
                i, fc->format[i].format,fc->format[i].frame[j].bFrameIndex, fc->format[i].frame[j].wWidth,
                fc->format[i].frame[j].wHeight, fc->format[i].frame[j].dwDefaultFrameInterval, fc->format[i].frame[j].dwFrameInterval,
                fc->format[i].frame[j].dwMinBitRate, fc->format[i].frame[j].dwMaxBitRate, fc->format[i].frame[j].bmCapabilities,
                fc->format[i].frame[j].dwMaxVideoFrameBufferSize);
        }
    }
}

static void print_option(uvc_demo_config *config)
{
    logd("vipp %d uvc %d bitrate %d dual_stream %d dual_stream_vipp %d uvc_bulk %d",
        config->vipp_dev, config->uvc_dev, config->bitrate, config->dual_stream,
        config->dual_stream_vipp_dev, config->uvc_bulk_mode);
    logd("uac_in %d uac_out %d uac_sr %d uac_ch %d uac_bw %d uac_aec %d uac_agc %d uac_ans %d",
        config->uac_in, config->uac_out, config->uac_sample_rate, config->uac_channel, config->bitrate,
        config->uac_aec, config->uac_agc, config->uac_ans);
}

static void sigint_handler(int signal)
{
	/* Stop the main loop when the user presses CTRL-C */
	demo_events_stop(sigint_events);
}

static void config_extra_config(uvc_demo_config *config, struct video_source_extra_config *extra_config)
{
    extra_config->vipp = config->vipp_dev;
    if (config->bitrate)
        extra_config->bitrate = config->bitrate;
    else {
        logw("set bitrate %d invalid! use default!", config->bitrate);
        extra_config->bitrate = 8;
    }

    if (config->dual_stream) {
        extra_config->dual_stream = config->dual_stream;
        extra_config->dual_stream_vipp_dev = config->dual_stream_vipp_dev;
    }

    if (config->enable_aiisp) {
        extra_config->enable_aiisp = config->enable_aiisp;
        extra_config->aiisp_mode = config->aiisp_mode;
        extra_config->tdm_rxbuf_cnt = config->tdm_rxbuf_cnt;
        extra_config->aiisp_auto_switch = config->aiisp_auto_switch;
        extra_config->aiisp_switch_interval = config->aiisp_switch_interval;
        strncpy(extra_config->isp_aiisp_bin_path,
            extra_config->isp_aiisp_bin_path, strlen(extra_config->isp_aiisp_bin_path));
        strncpy(extra_config->isp_day_bin_path,
            extra_config->isp_day_bin_path, strlen(extra_config->isp_day_bin_path));
        strncpy(extra_config->npu_lut_model_file_path,
            extra_config->npu_lut_model_file_path, strlen(extra_config->npu_lut_model_file_path));
        strncpy(extra_config->npu_model_file_path,
            extra_config->npu_model_file_path, strlen(extra_config->npu_model_file_path));
        extra_config->npu_ref_buf_reduce_enable = config->npu_ref_buf_reduce_enable;
        extra_config->tdm_drop_frame = config->tdm_drop_frame;
        extra_config->aiisp_switch_case = config->aiisp_switch_case;
        extra_config->aiisp_switch_release_res_enable = config->aiisp_switch_release_res_enable;
    }
}

static int uvc_device_start(struct uvc_demo_device *device, struct demo_events *events)
{
    char uvc_dev[64] = {0};

    if (device->config.debug)
#if VIDEO_SOURCE_COLORBAR
        device->video_src = video_source_create(VIDEO_SOURCE_TYPE_COLORBAR);
#else
        logw("video source colorbar isn't be enabled!");
#endif
#if VIDEO_SOURCE_RT_MEDIA
    else
        device->video_src = video_source_create(VIDEO_SOURCE_TYPE_RT_MEDIA);
#elif VIDEO_SOURCE_MPP
    else
        device->video_src = video_source_create(VIDEO_SOURCE_TYPE_MPP);
#endif
    if (!device->video_src) {
        loge("create video source fail!");
        return -1;
    }
    struct video_source_extra_config config;
    memset(&config, 0, sizeof(config));
    config_extra_config(&device->config, &config);
    video_source_set_extra_config(device->video_src, &config);
    sprintf(uvc_dev, "/dev/video%d", device->config.uvc_dev);
    device->stream = uvc_stream_new(uvc_dev);
    if (!device->stream) {
        loge("alloc uvc stream fail!");
        return -1;
    }
    uvc_stream_set_event_handler(device->stream, events);
    uvc_stream_set_video_source(device->stream, device->video_src);
    if (device->config.uvc_bulk_mode)
        uvc_stream_set_bulk_mode(device->stream, device->config.uvc_bulk_mode);
    uvc_stream_init_uvc(device->stream, device->fc);

    return 0;
}

static void uvc_device_stop(struct uvc_demo_device *device)
{
    uvc_stream_delete(device->stream);
    video_source_destroy(device->video_src);
}

int main(int argc, char *argv[])
{
    int ret = 0;
    struct demo_events events;
    struct uvc_demo_context context;
    struct uvc_demo_device *device, *dual_device;

    memset(&context, 0, sizeof(context));
    device = &context.device;
    device->index = 0;
    device->config.uvc_dev = -1;

    dual_device = &context.dual_device;
    dual_device->index = 1;
    dual_device->config.uvc_dev = -1;

    ret = parser_cmdline_param(argc, argv, &device->config, &dual_device->config);
    if (ret)
        goto _exit;
    if (device->config.debug || dual_device->config.debug) {
        device->config.debug = 1;
        dual_device->config.debug = 1;
    }

    device->fc = init_parser_uvc_configfs(device->index);
    if (!device->fc)
        goto _exit;
    print_support_format(device->fc);

    if (dual_device->config.uvc_dev >= 0) {
        dual_device->fc = init_parser_uvc_configfs(dual_device->index);
        if (!dual_device->fc)
            goto _free_fc;
        print_support_format(dual_device->fc);
    }

    demo_events_init(&events);

    sigint_events = &events;
    signal(SIGINT, sigint_handler);

#if VIDEO_SOURCE_MPP
    MPP_SYS_CONF_S sys_conf;
    memset(&sys_conf, 0, sizeof(sys_conf));
    sys_conf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&sys_conf);
    AW_MPI_SYS_Init();
#elif VIDEO_SOURCE_RT_MEDIA
    AWVideoInput_Init();
#endif

    if (device->config.uvc_dev >= 0) {
        ret = uvc_device_start(device, &events);
        if (ret) {
            loge("uvc device %d start fail!", device->config.uvc_dev);
            goto _events_cleanup;
        }
    }
    if (dual_device->config.uvc_dev >= 0) {
        ret = uvc_device_start(dual_device, &events);
        if (ret) {
            loge("uvc device %d start fail!", dual_device->config.uvc_dev);
            goto _uvc_stop;
        }
    }

    if (device->config.uac_in || device->config.uac_out) {
        uac_enable(&device->config);
    } else if (dual_device->config.uac_in || dual_device->config.uac_out) {
        uac_enable(&dual_device->config);
    }

    demo_events_loop(&events);

    if (device->config.uac_in || device->config.uac_out)
        uac_disable();
    else if (dual_device->config.uac_in || dual_device->config.uac_out)
        uac_disable();

_uvc_stop:
    if (device->config.uvc_dev >= 0)
        uvc_device_stop(device);
    if (dual_device->config.uvc_dev >= 0)
        uvc_device_stop(dual_device);

_events_cleanup:
#if VIDEO_SOURCE_MPP
    AW_MPI_SYS_Exit();
#elif VIDEO_SOURCE_RT_MEDIA
    AWVideoInput_DeInit();
#endif
    demo_events_cleanup(&events);
    if (dual_device->fc)
        destroy_parser_uvc_configfs(dual_device->fc);
_free_fc:
    destroy_parser_uvc_configfs(device->fc);
_exit:
    return ret;
}
