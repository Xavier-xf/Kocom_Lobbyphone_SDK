#ifndef __RT_AWAIISP_COMMON_H__
#define __RT_AWAIISP_COMMON_H__

#include <awaiisp.h>

#ifdef __cplusplus
       extern "C" {
#endif

#define RT_AWAIISP_COMMON_NUM_MAX  (4)

typedef enum rt_awaiisp_common_switch_case {
    /**
      Specify the aiisp switch case, aiisp and day all use 8bit.
    */
    RT_AWAIISP_COMMON_SWITCH_CASE_AIISP_DAY_ALL_8BIT,
    /**
      Specify the aiisp switch case, aiisp use 8bit but day use 10bit.
    */
    RT_AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT,
    /**
      Specify the aiisp switch case, aiisp and day all use 10bit.
    */
    RT_AWAIISP_COMMON_SWITCH_CASE_AIISP_DAY_ALL_10BIT,
    /**
      Invalid parameter.
    */
    RT_AWAIISP_COMMON_SWITCH_CASE_LAST
} rt_awaiisp_common_switch_case;

typedef struct rt_awaiisp_common_config_param {
    awaiisp_config_param config;
    char *isp_cfg_bin_path;
} rt_awaiisp_common_config_param;

typedef struct rt_awaiisp_common_switch_channel_param {
    int enable;
    int isp;
    awaiisp_switch_param config;
    char *isp_cfg_bin_path;
    int vipp;
    unsigned int drop_frame_num;
    rt_awaiisp_common_switch_case switch_case;
} rt_awaiisp_common_switch_channel_param;

typedef struct rt_awaiisp_common_switch_param {
    rt_awaiisp_common_switch_channel_param channel_param[RT_AWAIISP_COMMON_NUM_MAX];
} rt_awaiisp_common_switch_param;

int rt_awaiisp_common_set_ulimit_fd(int num);
int rt_awaiisp_common_enable(int isp, rt_awaiisp_common_config_param *param);
int rt_awaiisp_common_disable(int isp);
int rt_awaiisp_common_switch_mode(rt_awaiisp_common_switch_param *param);

#ifdef __cplusplus
       }
#endif

#endif
