/*
 * lv_box_res_wifi.c
 *
 *  Created on: 2022Äê9ÔÂ23ÈÕ
 *      Author: anruliu
 */

#include "lv_box_res_wifi.h"

#ifdef LV_USE_LINUX_WIFI2_MODE

#include <wifi_log.h>
#include <string.h>

#define BAND_NOME     0
#define BAND_2_4G     1
#define BAND_5G       2

static uint32_t freq_to_channel(uint32_t freq) {
    int band;
    uint32_t channel = 0;
    if ((freq <= 4980) && (freq >= 4910)) {
        band = BAND_5G;
    } else if ((freq >= 2407) && (freq <= 2484)) {
        band = BAND_2_4G;
    } else {
        band = BAND_NOME;
    }
    switch (band) {
    case BAND_2_4G:
        if (freq == 2484) {
            channel = 14;
        } else if (freq == 2407) {
            channel = 0;
        } else if ((freq <= 2472) && (freq > 2407)) {
            if (((freq - 2407) % 5) == 0) {
                channel = ((freq - 2407) / 5);
            } else {
                channel = 1000;
            }
        } else {
            channel = 1000;
        }
        break;
    case BAND_5G:
        if (((freq - 4000) % 5) == 0) {
            channel = ((freq - 4000) / 5);
        } else {
            channel = 1000;
        }
        break;
    case BAND_NOME:
    default:
        channel = 1000;
        break;
    }
    return channel;
}

static char* key_mgmt_to_char(char *key_mgmt_buf, wifi_secure_t key_mgmt) {
    switch (key_mgmt) {
    case WIFI_SEC_WEP:
        strncpy(key_mgmt_buf, "WEP", 3);
        break;
    case WIFI_SEC_WPA_PSK:
        strncpy(key_mgmt_buf, "WPA_PSK", 7);
        break;
    case WIFI_SEC_WPA2_PSK:
        strncpy(key_mgmt_buf, "WPA2_PSK", 8);
        break;
    case WIFI_SEC_WPA3_PSK:
        strncpy(key_mgmt_buf, "WPA3_PSK", 8);
        break;
    case WIFI_SEC_NONE:
    default:
        strncpy(key_mgmt_buf, "NONE", 4);
        break;
    }
    return key_mgmt_buf;
}

static void wifi_msg_cb(wifi_msg_data_t *msg) {
    char msg_cb_arg[64] = "NULL";
    char *msg_cb_arg_p;
    //msg cb test, need to pass static parameters
    if (msg->private_data) {
        msg_cb_arg_p = (char*) msg->private_data;
    } else {
        msg_cb_arg_p = msg_cb_arg;
    }
    switch (msg->id) {
    case WIFI_MSG_ID_DEV_STATUS:
        switch (msg->data.d_status) {
        case WLAN_STATUS_DOWN:
            WMG_INFO("***** wifi msg cb: dev status down *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WLAN_STATUS_UP:
            WMG_INFO("***** wifi msg cb: dev status up *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        default:
            WMG_INFO("***** wifi msg cb: dev status unknow *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        }
        break;
    case WIFI_MSG_ID_STA_CN_EVENT:
        switch (msg->data.event) {
        case WIFI_DISCONNECTED:
            WMG_INFO("***** wifi msg cb: sta event disconnect *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_SCAN_STARTED:
            WMG_INFO("***** wifi msg cb: sta event scan started *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_SCAN_FAILED:
            WMG_INFO("***** wifi msg cb: sta event scan failed *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_SCAN_RESULTS:
            WMG_INFO("***** wifi msg cb: sta event scan results *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_NETWORK_NOT_FOUND:
            WMG_INFO("***** wifi msg cb: sta event network not found *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_PASSWORD_INCORRECT:
            WMG_INFO("***** wifi msg cb: sta event password incorrect *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_AUTHENTIACATION:
            WMG_INFO("***** wifi msg cb: sta event authentiacation *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_AUTH_REJECT:
            WMG_INFO("***** wifi msg cb: sta event auth reject *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_ASSOCIATING:
            WMG_INFO("***** wifi msg cb: sta event associating *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_ASSOC_REJECT:
            WMG_INFO("***** wifi msg cb: sta event assoc reject *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_ASSOCIATED:
            WMG_INFO("***** wifi msg cb: sta event associated *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_4WAY_HANDSHAKE:
            WMG_INFO("***** wifi msg cb: sta event 4 way handshake *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_GROUNP_HANDSHAKE:
            WMG_INFO("***** wifi msg cb: sta event grounp handshake *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_GROUNP_HANDSHAKE_DONE:
            WMG_INFO("***** wifi msg cb: sta event handshake done *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_CONNECTED:
            WMG_INFO("***** wifi msg cb: sta event connected *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_CONNECT_TIMEOUT:
            WMG_INFO("***** wifi msg cb: sta event connect timeout *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_DEAUTH:
            WMG_INFO("***** wifi msg cb: sta event deauth *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_DHCP_START:
            WMG_INFO("***** wifi msg cb: sta event dhcp start *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_DHCP_TIMEOUT:
            WMG_INFO("***** wifi msg cb: sta event dhcp timeout *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_DHCP_SUCCESS:
            WMG_INFO("***** wifi msg cb: sta event dhcp success *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_TERMINATING:
            WMG_INFO("***** wifi msg cb: sta event terminating *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_UNKNOWN:
        default:
            WMG_INFO("***** wifi msg cb: sta event unknow *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        }
        break;
    case WIFI_MSG_ID_STA_STATE_CHANGE:
        switch (msg->data.state) {
        case WIFI_STA_IDLE:
            WMG_INFO("***** wifi msg cb: sta state idle *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_STA_CONNECTING:
            WMG_INFO("***** wifi msg cb: sta state connecting *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_STA_CONNECTED:
            WMG_INFO("***** wifi msg cb: sta state connected *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_STA_OBTAINING_IP:
            WMG_INFO("***** wifi msg cb: sta state obtaining ip *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_STA_NET_CONNECTED:
            WMG_INFO("***** wifi msg cb: sta state net connected *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_STA_DHCP_TIMEOUT:
            WMG_INFO("***** wifi msg cb: sta state dhcp timeout *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_STA_DISCONNECTING:
            WMG_INFO("***** wifi msg cb: sta state disconnecting *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_STA_DISCONNECTED:
            WMG_INFO("***** wifi msg cb: sta state disconnected *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        default:
            WMG_INFO("***** wifi msg cb: sta state unknow *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        }
        break;
    case WIFI_MSG_ID_AP_CN_EVENT:
        switch (msg->data.ap_event) {
        case WIFI_AP_ENABLED:
            WMG_INFO("***** wifi msg cb: ap enable *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_AP_DISABLED:
            WMG_INFO("***** wifi msg cb: ap disable *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_AP_STA_DISCONNECTED:
            WMG_INFO("***** wifi msg cb: ap sta disconnected *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_AP_STA_CONNECTED:
            WMG_INFO("***** wifi msg cb: ap sta connected *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_AP_UNKNOWN:
        default:
            WMG_INFO("***** wifi msg cb: ap event unknow *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        }
        break;
    case WIFI_MSG_ID_MONITOR:
        switch (msg->data.mon_state) {
        case WIFI_MONITOR_DISABLE:
            WMG_INFO("***** wifi msg cb: monitor disable *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        case WIFI_MONITOR_ENABLE:
            WMG_INFO("***** wifi msg cb: monitor enable *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            break;
        default:
            WMG_INFO("***** wifi msg cb: monitor frame *****\n");
            WMG_INFO("***** arg:%s *****\n", msg_cb_arg_p);
            WMG_INFO("***** wifi msg cb: monitor data channel %d *****\n",
                    msg->data.frame->channel);
            WMG_INFO("***** wifi msg cb: monitor data len %d *****\n",
                    msg->data.frame->len);
            break;
        }
        break;
    default:
        break;
    }
}

void lv_box_res_wifi_init(void) {
    wmg_set_debug_level(3);
    wifimanager_init();
}

void lv_box_res_wifi_deinit(void) {
    wifimanager_deinit();
}

int lv_box_res_wifi_on(void) {
    int ret = -1;
    static char sta_msg_cb_char[64] = "sta mode msg cb arg test";

    ret = wifi_on(WIFI_STATION);
    if (ret == 0) {
        ret = wifi_register_msg_cb(wifi_msg_cb, (void*) sta_msg_cb_char);
        if (ret) {
            WMG_INFO("register msg cb faile\n");
        }
        return 0;
    }

    return ret;
}

void lv_box_res_wifi_off(void) {
    wifi_off();
}

int lv_box_res_wifi_scan(void) {
    char scan_res_char[12] = { 0 };

    int i, bss_num = 0, ret = 0;
    ret = wifi_get_scan_results(scan_res, &bss_num, RES_LEN);
    if (ret == 0) {
        get_scan_results_num = (RES_LEN > bss_num ? bss_num : RES_LEN);
        for (i = 0; i < get_scan_results_num; i++) {
            memset(scan_res_char, 0, 12);
            /*            WMG_INFO(
             "bss[%02d]: bssid=%s  ssid=%s  channel=%d(freq=%d)  rssi=%d  sec=%s\n",
             i, scan_res[i].bssid, scan_res[i].ssid,
             freq_to_channel(scan_res[i].freq), scan_res[i].freq,
             scan_res[i].rssi,
             key_mgmt_to_char(scan_res_char, scan_res[i].key_mgmt));*/
        }
        WMG_INFO(
                "===Wi-Fi scan successful, total %d ap(buff size: %d) time %4f ms===\n",
                bss_num, RES_LEN, 0);
        return 0;
    } else {
        get_scan_results_num = 0;
        WMG_WARNG("===Wi-Fi scan failed, time %4f ms===\n", 0);
        return -1;
    }
}

int lv_box_res_wifi_connect(char *ssid, const char *password, int sec) {
    int ret = -1;
    wifi_sta_cn_para_t cn_para;

    cn_para.sec = (wifi_secure_t) sec;
    cn_para.ssid = ssid;
    cn_para.password = password;
    cn_para.fast_connect = 0;

    ret = wifi_sta_connect(&cn_para);

    if (ret == 0) {
        WMG_INFO("===Wi-Fi connect use sec(%d)===\n", cn_para.sec);
        WMG_INFO("===Wi-Fi connect successful,time %4f ms===\n", 0);
        return 0;
    }

    return ret;
}
#else

void lv_box_res_wifi_init(void) {
}

void lv_box_res_wifi_deinit(void) {
}

int lv_box_res_wifi_on(void) {
    return -1;
}

void lv_box_res_wifi_off(void) {
}

int lv_box_res_wifi_scan(void) {
    return 0;
}

int lv_box_res_wifi_connect(char *ssid, const char *password, int sec) {
    return 0;
}

#endif
