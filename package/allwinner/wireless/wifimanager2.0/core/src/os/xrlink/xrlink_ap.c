/*
 * Copyright (C) 2022 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of XRADIO TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <log_core.h>
#include <os_net_sync_notify.h>
#include <xr_proto.h>
#include <wifimg.h>
#include <xrlink_common.h>
#include <os_net_thread.h>
#include <api_action.h>
#include <wifimg_glue.h>
#include <wmg_ap.h>
#include <wifi_log.h>

typedef struct {
	snfy_handle_t *dev_event_snfy_handle;
	snfy_handle_t *ap_status_snfy_handle;
	wifi_vend_cb_t vend_cb;
} xrlink_ap_private_data_t;

static wmg_ap_inf_object_t ap_inf_object;
static xrlink_ap_private_data_t xrlink_ap_private_data;

static void event_notify_to_ap_dev(wifi_ap_event_t event)
{
	if (ap_inf_object.ap_event_cb) {
		ap_inf_object.ap_event_cb(event);
	}
}

static void dev_event_notify(char *data)
{
	snfy_ready(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->dev_event_snfy_handle, data);
}

static void* get_dev_event(uint16_t *type)
{
	char *data = NULL;
	data = snfy_await(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->dev_event_snfy_handle,
				WAIT_DEV_NORMOL_TO_MS);

	if(data != NULL) {
		xr_cfg_payload_t *payload = (xr_cfg_payload_t*) data;
		*type = payload->type;
		return payload->param;
	}
	return data;
}

static void ap_status_notify(uint8_t *data)
{
	snfy_ready(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->ap_status_snfy_handle, data);
}

static xr_wifi_dev_status_t *get_ap_status(void)
{
	return (xr_wifi_dev_status_t *)snfy_await(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->ap_status_snfy_handle,
		WAIT_DEV_NORMOL_TO_MS);
}

static int xr_wifi_ap_cb(char *data,uint32_t len)
{
	xr_cfg_payload_t *payload = (xr_cfg_payload_t*) data;
	xr_wifi_sta_cn_event_t *cn_event;
	xr_wifi_raw_data_t *raw_data;

	switch(payload->type) {
		case XR_WIFI_DEV_AP_STATE:
			ap_status_notify(payload->param);
			break;
		case XR_WIFI_DEV_IND_RAW_DATA:
			raw_data = (xr_wifi_raw_data_t *) payload->param;
			if(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->vend_cb) {
				((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->vend_cb(raw_data->data, raw_data->len);
			}
			break;
		default:
			dev_event_notify(data);
			break;
	}

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t glue_ap_inf_init(ap_event_cb_t ap_event_cb)
{
	wmg_status_t status = WMG_STATUS_SUCCESS;

	ap_inf_object.ap_event_cb = ap_event_cb;

	((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->dev_event_snfy_handle = snfy_new();
	if(!(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->dev_event_snfy_handle)) {
		WMG_ERROR("dev event snfy_new failed\n");
		return WMG_STATUS_FAIL;
	}

	((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->ap_status_snfy_handle = snfy_new();
	if(!(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->ap_status_snfy_handle)) {
		snfy_free(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->dev_event_snfy_handle);
		WMG_ERROR("dev status snfy_new failed\n");
		return WMG_STATUS_FAIL;
	}

	xr_wifi_init((xr_wifi_msg_cb_t)xr_wifi_ap_cb, WIFI_AP);

	return status;
}

static wmg_status_t glue_ap_inf_deinit(void)
{
	wmg_status_t status = WMG_STATUS_SUCCESS;

	xr_wifi_deinit();

	if(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->dev_event_snfy_handle) {
		snfy_free(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->dev_event_snfy_handle);
	}
	if(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->ap_status_snfy_handle) {
		snfy_free(((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->ap_status_snfy_handle);
	}

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t glue_ap_inf_enable()
{
	wmg_status_t status = WMG_STATUS_SUCCESS;
	xr_wifi_dev_status_t *dev_status;
	wifi_mode_t mode = WIFI_AP;

	return xr_wifi_on(mode);
}

static wmg_status_t glue_ap_inf_disable()
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	xr_wifi_dev_status_t *dev_status;
	wifi_mode_t mode = WIFI_AP;

	return xr_wifi_off(mode);
}

static wmg_status_t glue_ap_enable(wifi_ap_config_t *ap_config)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	uint16_t dev_event_type = XR_WIFI_DEV_ID_MAX;

	xr_wifi_ap_config_t *cfg = (xr_wifi_ap_config_t *)malloc(sizeof(xr_wifi_ap_config_t));
	memset(cfg, 0, sizeof(xr_wifi_ap_config_t));

	cfg->channel = ap_config->channel;
	cfg->sec = ap_config->sec;

	memcpy(cfg->ssid, ap_config->ssid, strlen(ap_config->ssid));
	if (cfg->sec) {
		memcpy(cfg->pwd, ap_config->psk, strlen(ap_config->psk));
	}

	status = xrlink_send_cmd(sizeof(xr_wifi_ap_config_t), XR_WIFI_HOST_AP_ENABLE, cfg);

	xr_wifi_ap_state_t *ap_state = get_ap_status();
	if(ap_state) {
		WMG_DEBUG("enable ap state:%d\n", ap_state->state);
	} else {
		status = OS_NET_STATUS_FAILED;
	}

	free(cfg);

	return status;
}

static wmg_status_t glue_ap_disable()
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;

	status = xrlink_send_cmd(0, XR_WIFI_HOST_AP_DISABLE, NULL);

	return status;
}

static wmg_status_t glue_ap_get_config(wifi_ap_config_t *ap_config)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	uint16_t dev_event_type = XR_WIFI_DEV_ID_MAX;
	int i;
	char mac[18 + 1] = {0};
	xr_wifi_ap_sta_info_t *ap_sta_info;

	status = xrlink_send_cmd(0, XR_WIFI_HOST_AP_GET_INF, NULL);
	xr_wifi_ap_info_t *info = get_dev_event(&dev_event_type);
	if(dev_event_type == XR_WIFI_DEV_AP_INF) {
		ap_config->channel = info->channel;
		ap_config->sta_num = info->sta_num;
		WMG_DEBUG("sta number:%d\n", info->sta_num);
		ap_config->sec = info->sec;
		memcpy(ap_config->ssid, info->ssid, strlen(info->ssid));
		memcpy(ap_config->psk, info->pwd, strlen(info->pwd));
		ap_sta_info = (xr_wifi_ap_sta_info_t *) info->dev_list;
		WMG_DEBUG("sta number:%d\n", info->sta_num);
		for(i = 0; i < info->sta_num; i++) {
			sprintf(mac,"%02X:%02X:%02X:%02X:%02X:%02X%c",
				ap_sta_info[i].addr[0],ap_sta_info[i].addr[1],ap_sta_info[i].addr[2],
				ap_sta_info[i].addr[3],ap_sta_info[i].addr[4],ap_sta_info[i].addr[5],'\0');
			WMG_DEBUG("mac addres:%s\n", mac);
			memcpy(ap_config->dev_list[i], mac, 18);
		}
	} else {
		status = OS_NET_STATUS_FAILED;
	}
	return status;
}

static wmg_status_t glue_ap_get_scan_results(get_scan_results_para_t *sta_scan_results_para)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	xr_wifi_scan_result_t *scan_result = NULL;
	xr_wifi_scan_info_t *scan_info = NULL;
	int i;
	char bssid[18 + 1];
	uint16_t dev_event_type = XR_WIFI_DEV_ID_MAX;

	status = xrlink_send_cmd(0, XR_WIFI_HOST_GET_SCAN_RES, NULL);
	scan_result = (xr_wifi_scan_result_t *)get_dev_event(&dev_event_type);
	if (dev_event_type == XR_WIFI_DEV_SCAN_RES) {
		scan_info = (xr_wifi_scan_info_t *)scan_result->ap_info;
		*(sta_scan_results_para->bss_num) = scan_result->num;
		for (i = 0; i < *(sta_scan_results_para->bss_num); i++) {
			sta_scan_results_para->scan_results[i].freq = scan_info[i].freq;
			sta_scan_results_para->scan_results[i].rssi = scan_info[i].rssi;
			sta_scan_results_para->scan_results[i].key_mgmt = scan_info[i].key_mgmt;

			sprintf(bssid,"%02X:%02X:%02X:%02X:%02X:%02X%c",
				scan_info[i].bssid[0], scan_info[i].bssid[1], scan_info[i].bssid[2],
				scan_info[i].bssid[3], scan_info[i].bssid[4], scan_info[i].bssid[5], '\0');

			memcpy(sta_scan_results_para->scan_results[i].bssid, bssid, 18);
			memcpy(sta_scan_results_para->scan_results[i].ssid, scan_info[i].ssid, strlen(scan_info[i].ssid));
		}
	} else {
		WMG_ERROR("get dev event is not wifi dev scan results\n");
		status = OS_NET_STATUS_FAILED;
	}

	return status;
}

static wmg_status_t glue_ap_vendor_send_data(uint8_t *data, uint32_t len)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	xr_wifi_raw_data_t *raw = (xr_wifi_raw_data_t *)malloc(sizeof(xr_wifi_raw_data_t) + len);
	if(NULL == raw) {
		status = WMG_STATUS_NOMEM;
		return status;
	}

	raw->len = len;

	memcpy(raw->data, data, len);

	status = xrlink_send_cmd(sizeof(xr_wifi_raw_data_t) + len, XR_WIFI_HOST_SEND_RAW, raw);

	free(raw);
	return status;
}

static wmg_status_t glue_ap_vendor_register_rx_cb(wifi_vend_cb_t vend_cb)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	((xrlink_ap_private_data_t *)ap_inf_object.ap_private_data)->vend_cb = vend_cb;
	if(vend_cb) {
		status = WMG_STATUS_SUCCESS;
	} else {
		status = WMG_STATUS_INVALID;
	}
	return status;
}

static wmg_status_t glue_platform_extension(int cmd, void* cmd_para,int *erro_code)
{
	switch (cmd) {
		case AP_CMD_ENABLE:
			return glue_ap_enable((wifi_ap_config_t *)cmd_para);
		case AP_CMD_DISABLE:
			return glue_ap_disable();
		case AP_CMD_GET_CONFIG:
			return glue_ap_get_config((wifi_ap_config_t *)cmd_para);
		case AP_CMD_SET_SCAN_PARAM:
			return WMG_STATUS_FAIL;
		case AP_CMD_GET_SCAN_RESULTS:
			{
				get_scan_results_para_t *ap_scan_results_para = (get_scan_results_para_t *)cmd_para;
				return glue_ap_get_scan_results(ap_scan_results_para);
			}
		case AP_CMD_VENDOR_SEND_DATA:
			{
				vendor_send_data_para_t *vendor_send_data_para = (vendor_send_data_para_t *)cmd_para;
				return glue_ap_vendor_send_data(vendor_send_data_para->data, vendor_send_data_para->len);
			}
		case AP_CMD_VENDOR_REGISTER_RX_CB:
				return glue_ap_vendor_register_rx_cb((wifi_vend_cb_t)cmd_para);
		default:
		return WMG_STATUS_FAIL;
	}
	return WMG_STATUS_FAIL;
}

static xrlink_ap_private_data_t xrlink_ap_private_data = {
	.dev_event_snfy_handle = NULL,
	.ap_status_snfy_handle = NULL,
	.vend_cb = NULL,
};

static wmg_ap_inf_object_t ap_inf_object = {
	.ap_init_flag = WMG_FALSE,
	.ap_event_cb = NULL,
	.ap_private_data = &xrlink_ap_private_data,

	.ap_inf_init = glue_ap_inf_init,
	.ap_inf_deinit = glue_ap_inf_deinit,
	.ap_inf_enable = glue_ap_inf_enable,
	.ap_inf_disable = glue_ap_inf_disable,
	.ap_platform_extension = glue_platform_extension,
};

wmg_ap_inf_object_t* ap_xrlink_inf_object_register(void)
{
	return &ap_inf_object;
}
