#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_DEBUG

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "include/option.h"
#include "include/debug.h"

static inline void set_suspend_seconds(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->suspend_ms = atoi(option_param) * 1000;
    return;
}

static inline void set_encode_frame_cnt(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->encode_frame_cnt = atoi(option_param);
    return;
}

static inline void set_ms(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->ms = atoi(option_param);
    return;
}

static inline void set_mst(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->mst = atoi(option_param);
    return;
}

static inline void set_pdet(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->pdet = atoi(option_param);
    return;
}

static inline void set_camera_low_pw(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->camera_low_pw = atoi(option_param);
    return;
}

static inline void set_record_type(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->record_type = atoi(option_param);
    return;
}

/*static inline void set_frame_mode(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->frame_mode = atoi(option_param);
    return;
}*/

/*static inline void set_suspend_mode(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->suspend_mode = atoi(option_param);
    return;
}*/

static inline void set_drop_frames(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->drop_frames = atoi(option_param);
    return;
}

static inline void set_attach_debug_osd(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->attach_debug_osd = atoi(option_param);
    return;
}

static inline void set_ongoing_duration(void *thiz, char *option_param)
{
    struct aov_demo_config *config = (struct aov_demo_config *)thiz;
    config->ongoing_duration = atoi(option_param);
    return;
}

static const struct aov_demo_options options[] =
{
    {
        .short_name = "-s",
        .full_name  = "--suspend_seconds",
        .help       = "set suspend seconds. eg: -s 5(means suspend 5 seconds)",
        .set_option = set_suspend_seconds,
    },
    {
        .short_name = "-n",
        .full_name  = "--encode_frame_cnt",
        .help       = "set encode frame max cnt. eg: -n 100(means encode 100 frames then exit)",
        .set_option = set_encode_frame_cnt,
    },
    /*{
        .short_name = "-ms",
        .full_name  = "--motion_search",
        .help       = "set motion search detect. eg: -ms 1(means enable motion search detect)",
        .set_option = set_ms,
    },*/
    {
        .short_name = "-mst",
        .full_name  = "--motion_search_threshord",
        .help       = "set motion search detect. eg: -ms 1(means enable motion search detect)",
        .set_option = set_mst,
    },
    {
        .short_name = "-pdet",
        .full_name  = "--persion_detect",
        .help       = "set persion detect. eg: -pdet 1(means enable persion detect)",
        .set_option = set_pdet,
    },
    {
        .short_name = "-cam_low_pw",
        .full_name  = "--camera_low_power",
        .help       = "set camera low power mode. eg: -cam_low_pw 1(means enable camera low power mode)",
        .set_option = set_camera_low_pw,
    },
    {
        .short_name = "-rec_type",
        .full_name  = "--record_type",
        .help       = "set record type. eg: -rec_type 0(means set record type frameloop. 0: frameloop 1: timelapse)",
        .set_option = set_record_type,
    },
    /*{
        .short_name = "-frame_mode",
        .full_name  = "--frame_mode",
        .help       = "set frame mode. eg: -frame_mode 0(means set frame mode multi frame. 0: multi frame 1: one frame)",
        .set_option = set_frame_mode,
    },
    {
        .short_name = "-suspend_mode",
        .full_name  = "--suspend_mode",
        .help       = "set suspend mode. eg: -suspend_mode 0(means process and suspend ms. 0: process and suspned ms 1: total suspend ms)",
        .set_option = set_suspend_mode,
    },*/
    {
        .short_name = "-dp",
        .full_name  = "--drop_frames",
        .help       = "set drop frames. eg: -dp 20(means set set drop 20 frames.)",
        .set_option = set_drop_frames,
    },
    {
        .short_name = "-ado",
        .full_name  = "--attach_debug_osd",
        .help       = "attach debug osd. eg: -ado 1(means enable attach debug osd. 0: disable(default) 1: enable)",
        .set_option = set_attach_debug_osd,
    },
    {
        .short_name = "-ogd",
        .full_name  = "--ongoing_duration",
        .help       = "set ongoing record duration unit second. eg: -ogd 10(means set ongoing record duration 10 seconds.)",
        .set_option = set_ongoing_duration,
    },
};
#define OPTIONS_NUM (sizeof(options) / sizeof(options[0]))

int  parser_cmdline_param(int argc, char *argv[], struct aov_demo_config *config)
{
    int find = 0;

    if (argc < 2)
    {
        loge("invalid option input -h get help info!");
        return -1;
    }
    for (int i = 1; i < argc; i++)
    {
        if ((!strcmp("-h", argv[i])) || (!strcmp("--help", argv[i])))
        {
            for (int j = 0; j < OPTIONS_NUM; j++)
                logd("%-12s %-24s %-24s", options[j].short_name, options[j].full_name, options[j].help);
            return -1;
        }
        for (int j = 0; j < OPTIONS_NUM; j++)
        {
            if ((!strcmp(options[j].short_name, argv[i])) || (!strcmp(options[j].full_name, argv[i])))
            {
                i++;
                if (!argv[i])
                    break;
                if (options[j].set_option)
                    options[j].set_option(config, argv[i]);
                logd("%s %d", options[j].short_name, atoi(argv[i]));
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
