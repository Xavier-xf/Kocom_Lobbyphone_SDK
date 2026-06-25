/*
 * lv_box_res_wifi.h
 *
 *  Created on: 2022��9��23��
 *      Author: anruliu
 */

#ifndef LV_86_BOXES_SRC_COMMON_LV_BOX_RES_WIFI_H_
#define LV_86_BOXES_SRC_COMMON_LV_BOX_RES_WIFI_H_

#ifdef LV_USE_LINUX_WIFI2_MODE
#include <wifimg.h>
#else
typedef enum {
    WIFI_SEC_NONE,
    WIFI_SEC_WEP,
    WIFI_SEC_WPA_PSK,
    WIFI_SEC_WPA2_PSK,
    WIFI_SEC_WPA3_PSK,
} wifi_secure_t;

typedef struct {
    char ssid[32 + 1];
    wifi_secure_t key_mgmt;
} wifi_scan_result_t;
#endif

#define RES_LEN          60
int get_scan_results_num;
wifi_scan_result_t scan_res[RES_LEN];

void lv_box_res_wifi_init(void);
void lv_box_res_wifi_deinit(void);
int lv_box_res_wifi_on(void);
void lv_box_res_wifi_off(void);
int lv_box_res_wifi_scan(void);
int lv_box_res_wifi_connect(char *ssid, const char *password, int sec);

#endif /* LV_86_BOXES_SRC_COMMON_LV_BOX_RES_WIFI_H_ */
