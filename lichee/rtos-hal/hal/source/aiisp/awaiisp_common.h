#ifndef __AWAIISP_COMMON_H__
#define __AWAIISP_COMMON_H__

#ifndef __linux__
#include "./lib/include/awaiisp.h"
#include "../vin/vin_isp/isp_server/isp_server.h"
#endif

/* print level */
#define awaiisp_common_err_print(fmt, arg...)      printf("[AWAIISP_COMMON_ERR]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define awaiisp_common_warn_print(fmt, arg...)     printf("[AWAIISP_COMMON_WRN]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define awaiisp_common_dbg_print(fmt, arg...)      printf("[AWAIISP_COMMON_DBG]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define awaiisp_common_ver_print(fmt, arg...)      //printf("[AWAIISP_COMMON_VER]%s:%u: " fmt "\n", __FUNCTION__, __LINE__, ##arg)

#ifdef __cplusplus
       extern "C" {
#endif

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

typedef struct awaiisp_common_switch_param {
    awaiisp_switch_param config;
#ifdef __linux
    char *isp_cfg_bin_path;
    int vipp;
    unsigned int drop_frame_num;
    awaiisp_common_switch_case switch_case;
#endif
} awaiisp_common_switch_param;

int awaiisp_common_set_ulimit_fd(int num);
int awaiisp_common_enable(int isp, awaiisp_common_config_param *param);
int awaiisp_common_disable(int isp);
int awaiisp_common_switch_mode(int isp, awaiisp_common_switch_param *param);

int awaiisp_common_rpmsg_init(void);
void awaiisp_common_rt_memheap_init(int isp);
int awaiisp_common_start(int isp, short int width, short int height);

#ifdef __cplusplus
       }
#endif

#endif
