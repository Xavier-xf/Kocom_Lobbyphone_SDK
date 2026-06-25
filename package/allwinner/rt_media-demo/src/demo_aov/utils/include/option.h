#ifndef __OPTION_H__
#define __OPTION_H__

#ifdef __cplusplus
extern "C"{
#endif

#define OPTIONS_SHORT_NAME_MAX_LEN                  32
#define OPTIONS_FULL_NAME_MAX_LEN                   256
#define OPTIONS_HELP_MAX_LEN                        256
#define OPTIONS_FILE_PATH_MAX_LEN                   256

struct aov_demo_options
{
    char short_name[OPTIONS_SHORT_NAME_MAX_LEN];
    char full_name[OPTIONS_FULL_NAME_MAX_LEN];
    char help[OPTIONS_HELP_MAX_LEN];
    void (*set_option)(void *thiz, char *option_param);
};

struct aov_demo_config
{
    int suspend_ms;
    int encode_frame_cnt;
    int ms;
    int mst;
    int pdet;
    int camera_low_pw;
    int record_type;
    int frame_mode;
    int suspend_mode;
    int drop_frames;
    int attach_debug_osd;
    int ongoing_duration;
};

int  parser_cmdline_param(int argc, char *argv[], struct aov_demo_config *config);

#ifdef __cplusplus
}
#endif

#endif
