#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_DEBUG
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../debug/include/debug.h"
#include "../option/include/option.h"

static inline void set_vipp_dev(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->vipp_dev = atoi(option_param);
    return;
}

static inline void set_uvc_dev(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uvc_dev = atoi(option_param);
    return;
}

static inline void set_bitrate(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->bitrate = atoi(option_param);
    return;
}

static inline void set_dual_stream(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->dual_stream = atoi(option_param);
    return;
}

static inline void set_dual_stream_vipp_dev(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->dual_stream_vipp_dev = atoi(option_param);
    return;
}

static inline void set_uvc_bulk_mode(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uvc_bulk_mode = atoi(option_param);
    return;
}

static inline void set_enable_aiisp(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->enable_aiisp = atoi(option_param);
    return;
}

static inline void set_aiisp_npu_ref_buf_reduce_enable(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->npu_ref_buf_reduce_enable = atoi(option_param);
    return;
}

static inline void set_tdm_drop_frame(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->tdm_drop_frame = atoi(option_param);
    return;
}

static inline void set_aiisp_switch_case(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->aiisp_switch_case = atoi(option_param);
    return;
}

static inline void set_aiisp_switch_release_res_enable(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->aiisp_switch_release_res_enable = atoi(option_param);
    return;
}

static inline void set_aiisp_mode(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->aiisp_mode = atoi(option_param);
    return;
}

static inline void set_tdm_rxbuf_cnt(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->tdm_rxbuf_cnt = atoi(option_param);
    return;
}

static inline void set_aiisp_auto_switch(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->aiisp_auto_switch = atoi(option_param);
    return;
}

static inline void set_aiisp_switch_interval(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->aiisp_switch_interval = atoi(option_param);
    return;
}

static inline void set_isp_aiisp_bin_path(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    memset(config->isp_aiisp_bin_path, 0, sizeof(config->isp_aiisp_bin_path));
    strncpy(config->isp_aiisp_bin_path, option_param, sizeof(config->isp_aiisp_bin_path) - 1);
    return;
}

static inline void set_isp_day_bin_path(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    memset(config->isp_day_bin_path, 0, sizeof(config->isp_day_bin_path));
    strncpy(config->isp_day_bin_path, option_param, sizeof(config->isp_day_bin_path) - 1);
    return;
}

static inline void set_npu_lut_model_file_path(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    memset(config->npu_lut_model_file_path, 0, sizeof(config->npu_lut_model_file_path));
    strncpy(config->npu_lut_model_file_path, option_param, sizeof(config->npu_lut_model_file_path) - 1);
    return;
}

static inline void set_npu_model_file_path(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    memset(config->npu_model_file_path, 0, sizeof(config->npu_model_file_path));
    strncpy(config->npu_model_file_path, option_param, sizeof(config->npu_model_file_path) - 1);
    return;
}

static inline void set_uac_in(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uac_in = atoi(option_param);
    return;
}

static inline void set_uac_out(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uac_out = atoi(option_param);
    return;
}

static inline void set_uac_sample_rate(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uac_sample_rate = atoi(option_param);
    return;
}

static inline void set_uac_channel(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uac_channel = atoi(option_param);
    return;
}

static inline void set_uac_bitwidth(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uac_bitwidth = atoi(option_param);
    return;
}

static inline void set_uac_aec(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uac_aec = atoi(option_param);
    return;
}

static inline void set_uac_ans(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uac_ans = atoi(option_param);
    return;
}

static inline void set_uac_agc(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->uac_agc = atoi(option_param);
    return;
}

static inline void set_debug_mode(void *thiz, char *option_param)
{
    uvc_demo_config *config = (uvc_demo_config *)thiz;
    config->debug = atoi(option_param);
    return;
}

static const uvc_demo_options uvc_options[] =
{
    {
        .short_name = "-D",
        .full_name  = "--vipp_dev",
        .help       = "select vipp dev. eg: -D 0(means /dev/video0)",
        .set_option = set_vipp_dev,
    },
    {
        .short_name = "-d",
        .full_name  = "--uvc_dev",
        .help       = "select uvc dev. eg: -d 1(means /dev/video1)",
        .set_option = set_uvc_dev,
    },
    {
        .short_name = "-B",
        .full_name  = "--bitrate",
        .help       = "set MJPEG/H264 stream bitrate. eg: -B 5(means bitrate 5Mbps)",
        .set_option = set_bitrate,
    },
    {
        .short_name = "-s",
        .full_name  = "--dual_stream",
        .help       = "enable MJPEG insert H264 stream function. eg: -s 1(means enable dual stream function)",
        .set_option = set_dual_stream,
    },
    {
        .short_name = "-s_vipp_dev",
        .full_name  = "--dual_stream_vipp_dev",
        .help       = "set dual stream use vipp -dev. eg: -s_vipp_dev 4(means dual stream use /dev/video4)",
        .set_option = set_dual_stream_vipp_dev,
    },
    {
        .short_name = "-b",
        .full_name  = "--uvc_bulk_mode",
        .help       = "enable uvc bulk transport mode. eg: -b 1(means enable uvc bulk transport mode)",
        .set_option = set_uvc_bulk_mode,
    },
    {
        .short_name = "-uac_in",
        .full_name  = "--uac_in",
        .help       = "enable uac1 in function. eg: -uac_in 1(means enable uac1 in function)",
        .set_option = set_uac_in,
    },
    {
        .short_name = "-uac_out",
        .full_name  = "--uac_out",
        .help       = "enable uac1 out function. eg: -uac_out 1(means enable uac1 out function)",
        .set_option = set_uac_out,
    },
    {
        .short_name = "-uac_sr",
        .full_name  = "--uac_sample_rate",
        .help       = "set uac1 audio sample rate. eg: -uac_sr 16000(means set uac1 audio sample rate 16000)",
        .set_option = set_uac_sample_rate,
    },
    {
        .short_name = "-uac_ch",
        .full_name  = "--uac_channel",
        .help       = "set uac1 audio channel. eg: -uac_ch 1(means set uac1 audio channel 1)",
        .set_option = set_uac_channel,
    },
    {
        .short_name = "-uac_bw",
        .full_name  = "--uac_bitwidth",
        .help       = "set uac1 audio bitwidth. eg: -uac_bitwidth 16(means set uac1 audio bitwidth 16)",
        .set_option = set_uac_bitwidth,
    },
    {
        .short_name = "-uac_aec",
        .full_name  = "--uac_aec",
        .help       = "enable audio aec function. eg: -uac_aec 1(means enable audio aec functiuon)",
        .set_option = set_uac_aec,
    },
    {
        .short_name = "-uac_agc",
        .full_name  = "--uac_agc",
        .help       = "enable audio agc function. eg: -uac_agc 1(means enable audio agc functiuon)",
        .set_option = set_uac_agc,
    },
    {
        .short_name = "-uac_ans",
        .full_name  = "--uac_ans",
        .help       = "enable audio ans function. eg: -uac_ans 1(means enable audio ans functiuon)",
        .set_option = set_uac_ans,
    },
    {
        .short_name = "-enable_dual_uvc",
        .full_name  = "--enable_dual_uvc",
        .help       = "enable dual uvc device. eg: -enable_dual_uvc(means enable dual uvc device)",
        .set_option = NULL,
    },
    {
        .short_name = "-debug",
        .full_name  = "--debug_mode",
        .help       = "debug mode, send picture. eg: -debug 1(means enable debug mode)",
        .set_option = set_debug_mode,
    },
};
#define UVC_OPTIONS_NUM (sizeof(uvc_options) / sizeof(uvc_demo_options))

int  parser_cmdline_param(int argc, char *argv[], uvc_demo_config *config, uvc_demo_config *dual_config)
{
    int find = 0;
    uvc_demo_config *tmp_config = config;

    if (argc < 2)
    {
        loge("invalid option input -h get help info!");
        return -1;
    }
    for (int i = 1; i < argc; i++)
    {
        if ((!strcmp("-h", argv[i])) || (!strcmp("--help", argv[i])))
        {
            for (int j = 0; j < UVC_OPTIONS_NUM; j++)
                logd("%-18s %-35s %-24s", uvc_options[j].short_name, uvc_options[j].full_name, uvc_options[j].help);
            return -1;
        }
        for (int j = 0; j < UVC_OPTIONS_NUM; j++)
        {
            if ((!strcmp(argv[i], "-enable_dual_uvc")) || (!strcmp(argv[i], "--enable_dual_uvc")))
                tmp_config = dual_config;
            if ((!strcmp(uvc_options[j].short_name, argv[i])) || (!strcmp(uvc_options[j].full_name, argv[i])))
            {
                i++;
                if (!argv[i])
                    break;
                if (uvc_options[j].set_option)
                    uvc_options[j].set_option(tmp_config, argv[i]);
                logv("%s %d", uvc_options[j].short_name, atoi(argv[i]));
                find = 1;
                break;
            }
        }
        if (!find)
        {
            loge("invalid option input -h get help info!");
            return -1;
        }
    }
    return 0;
}
