#ifndef __OPTION_H__
#define __OPTION_H__

#ifdef __cplusplus
extern "C"{
#endif

#define UVC_OPTIONS_MAX_LEN              128
#define UVC_OPTIONS_HELP_MAX_LEN         256
#define UVC_OPTIONS_FILE_PATH_MAX_LEN    100

typedef struct uvc_demo_options
{
    char short_name[UVC_OPTIONS_MAX_LEN];
    char full_name[UVC_OPTIONS_MAX_LEN];
    char help[UVC_OPTIONS_HELP_MAX_LEN];
    void (*set_option)(void *thiz, char *option_param);
}uvc_demo_options;

typedef struct uvc_demo_config
{
    int vipp_dev;
    int uvc_dev;
    int bitrate; //unit:Mbps
    int dual_stream;
    int dual_stream_vipp_dev;
    int uvc_bulk_mode;
    int enable_aiisp;
    int aiisp_mode;
    int tdm_rxbuf_cnt;
    int aiisp_auto_switch;
    int aiisp_switch_interval;
    char isp_aiisp_bin_path[UVC_OPTIONS_FILE_PATH_MAX_LEN];
    char isp_day_bin_path[UVC_OPTIONS_FILE_PATH_MAX_LEN];
    char npu_lut_model_file_path[UVC_OPTIONS_FILE_PATH_MAX_LEN];
    char npu_model_file_path[UVC_OPTIONS_FILE_PATH_MAX_LEN];
    int npu_ref_buf_reduce_enable;
    unsigned int tdm_drop_frame;
    int aiisp_switch_case;
    int aiisp_switch_release_res_enable;
    int uac_in; //in host perspective. so uac_in means audio output in device, device should capture audio by mic.
    int uac_out;//in host perspective. so uac_out means audio input in device, device should play data by speaker.
    int uac_sample_rate;
    int uac_channel;
    int uac_bitwidth;
    int uac_aec;
    int uac_agc;
    int uac_ans;
    int debug;
}uvc_demo_config;

int parser_cmdline_param(int argc, char *argv[], uvc_demo_config *config, uvc_demo_config *dual_config);

#ifdef __cplusplus
}
#endif

#endif
