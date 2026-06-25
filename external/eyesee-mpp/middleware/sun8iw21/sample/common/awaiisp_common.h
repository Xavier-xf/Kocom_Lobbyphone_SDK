#ifndef __AWAIISP_COMMON_H__
#define __AWAIISP_COMMON_H__

#include <awaiisp.h>

#ifdef __cplusplus
       extern "C" {
#endif

#define AWAIISP_COMMON_NUM_MAX  (4)

typedef enum awaiisp_common_switch_case {
    /**
      Specify the aiisp switch case, aiisp and day all use 8bit.
    */
    AWAIISP_COMMON_SWITCH_CASE_AIISP_DAY_ALL_8BIT,
    /**
      Specify the aiisp switch case, aiisp use 8bit but day use 10bit.
    */
    AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT,
    /**
      Specify the aiisp switch case, aiisp and day all use 10bit.
    */
    AWAIISP_COMMON_SWITCH_CASE_AIISP_DAY_ALL_10BIT,
    /**
      Invalid parameter.
    */
    AWAIISP_COMMON_SWITCH_CASE_LAST
} awaiisp_common_switch_case;

typedef struct awaiisp_common_config_param {
    awaiisp_config_param config;
    char *isp_cfg_bin_path;
} awaiisp_common_config_param;

typedef struct awaiisp_common_switch_channel_param {
    int enable;
    int isp;
    awaiisp_switch_param config;
    char *isp_cfg_bin_path;
    int vipp;
    unsigned int drop_frame_num;
    awaiisp_common_switch_case switch_case;
} awaiisp_common_switch_channel_param;

typedef struct awaiisp_common_switch_param {
    awaiisp_common_switch_channel_param channel_param[AWAIISP_COMMON_NUM_MAX];
} awaiisp_common_switch_param;

int awaiisp_common_set_ulimit_fd(int num);
int awaiisp_common_enable(int isp, awaiisp_common_config_param *param);
int awaiisp_common_disable(int isp);
int awaiisp_common_switch_mode(awaiisp_common_switch_param *param);

#ifdef __cplusplus
       }
#endif

#endif
