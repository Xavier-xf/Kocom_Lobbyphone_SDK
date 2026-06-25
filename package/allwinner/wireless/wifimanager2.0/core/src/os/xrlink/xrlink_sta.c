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
#include <wmg_sta.h>
#include <wifi_log.h>

typedef struct {
	snfy_handle_t *sta_event_snfy_handle;
	wifi_sta_event_t event;
	snfy_handle_t *dev_event_snfy_handle;
	wifi_vend_cb_t vend_cb;
} xrlink_sta_private_data_t;


static wmg_sta_inf_object_t sta_inf_object;
static xrlink_sta_private_data_t xrlink_sta_private_data;

static void event_notify_to_sta_dev(wifi_sta_event_t event)
{
	if (sta_inf_object.sta_event_cb) {
		sta_inf_object.sta_event_cb(event);
	}
}

static void sta_event_notify(wifi_sta_event_t event)
{
	((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->event = event;
	snfy_ready(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->sta_event_snfy_handle,
			&(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->event));
	event_notify_to_sta_dev(event);
}

static wifi_sta_event_t get_sta_event(void)
{
	wifi_sta_event_t *sta_event = NULL;
	sta_event = snfy_await(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->sta_event_snfy_handle,
			WAIT_DEV_CONNECT_TO_MS);
	if(sta_event != NULL) {
		return *sta_event;
	} else {
		return WIFI_UNKNOWN;
	}
}

static void dev_event_notify(char *data)
{
	snfy_ready(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->dev_event_snfy_handle, data);
}

static void* get_dev_event(uint16_t *type)
{
	char *data = NULL;
	data = snfy_await(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->dev_event_snfy_handle,
				WAIT_DEV_NORMOL_TO_MS);

	if(data != NULL) {
		xr_cfg_payload_t *payload = (xr_cfg_payload_t*) data;
		*type = payload->type;
		return payload->param;
	}
	return data;
}

static int xr_wifi_sta_cb(char *data,uint32_t len)
{
	xr_cfg_payload_t *payload = (xr_cfg_payload_t*) data;
	xr_wifi_sta_cn_event_t *cn_event;
	xr_wifi_raw_data_t *raw_data;

	WMG_DEBUG("payload type:%d\n",payload->type);
	if(payload->type == XR_WIFI_DEV_STA_CN_EV) {
		cn_event = (xr_wifi_sta_cn_event_t *) payload->param;
		WMG_DEBUG("wifi sta event:%d\n", cn_event->event);
		switch(cn_event->event) {
			case XR_WIFI_DISCONNECTED:
				sta_event_notify(WIFI_DISCONNECTED);
				break;
			case XR_WIFI_SCAN_STARTED:
				event_notify_to_sta_dev(WIFI_SCAN_STARTED);
				break;
			case XR_WIFI_SCAN_FAILED:
				event_notify_to_sta_dev(WIFI_SCAN_FAILED);
				break;
			case XR_WIFI_SCAN_RESULTS:
				event_notify_to_sta_dev(WIFI_SCAN_RESULTS);
				break;
			case XR_WIFI_NETWORK_NOT_FOUND:
				sta_event_notify(WIFI_NETWORK_NOT_FOUND);
				break;
			case XR_WIFI_PASSWORD_INCORRECT:
				sta_event_notify(WIFI_PASSWORD_INCORRECT);
				break;
			case XR_WIFI_AUTHENTIACATION:
				event_notify_to_sta_dev(WIFI_AUTHENTIACATION);
				break;
			case XR_WIFI_AUTH_REJECT:
				sta_event_notify(WIFI_AUTH_REJECT);
				break;
			case XR_WIFI_ASSOCIATING:
				event_notify_to_sta_dev(WIFI_ASSOCIATING);
				break;
			case XR_WIFI_ASSOC_REJECT:
				sta_event_notify(WIFI_ASSOC_REJECT);
				break;
			case XR_WIFI_ASSOCIATED:
				event_notify_to_sta_dev(WIFI_ASSOCIATED);
				break;
			case XR_WIFI_4WAY_HANDSHAKE:
				event_notify_to_sta_dev(WIFI_4WAY_HANDSHAKE);
				break;
			case XR_WIFI_GROUNP_HANDSHAKE:
				event_notify_to_sta_dev(WIFI_GROUNP_HANDSHAKE);
				break;
			case XR_WIFI_GROUNP_HANDSHAKE_DONE:
				event_notify_to_sta_dev(WIFI_GROUNP_HANDSHAKE_DONE);
				break;
			case XR_WIFI_CONNECTED:
				event_notify_to_sta_dev(WIFI_CONNECTED);
				break;
			case XR_WIFI_CONNECT_TIMEOUT:
				sta_event_notify(WIFI_CONNECT_TIMEOUT);
				break;
			case XR_WIFI_DEAUTH:
				sta_event_notify(WIFI_DEAUTH);
				break;
			case XR_WIFI_DHCP_START:
				event_notify_to_sta_dev(WIFI_DHCP_START);
				break;
			case XR_WIFI_DHCP_TIMEOUT:
				sta_event_notify(WIFI_DHCP_TIMEOUT);
				break;
			case XR_WIFI_DHCP_SUCCESS:
				sta_event_notify(WIFI_DHCP_SUCCESS);
				break;
			case XR_WIFI_TERMINATING:
				sta_event_notify(WIFI_TERMINATING);
				break;
			case XR_WIFI_UNKNOWN:
				event_notify_to_sta_dev(WIFI_UNKNOWN);
				break;
		}
	} else if(payload->type == XR_WIFI_DEV_IND_RAW_DATA) {
		raw_data = (xr_wifi_raw_data_t *) payload->param;
		if(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->vend_cb) {
			((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->vend_cb(raw_data->data, raw_data->len);
		}
	} else {
		dev_event_notify(data);
	}

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t glue_sta_inf_init(sta_event_cb_t sta_event_cb)
{
	wmg_status_t status = WMG_STATUS_SUCCESS;

	sta_inf_object.sta_event_cb = sta_event_cb;

	((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->sta_event_snfy_handle = snfy_new();
	if(!(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->sta_event_snfy_handle)) {
		WMG_ERROR("sta event snfy_new failed\n");
		return WMG_STATUS_FAIL;
	}

	((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->dev_event_snfy_handle = snfy_new();
	if(!(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->dev_event_snfy_handle)) {
		snfy_free(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->sta_event_snfy_handle);
		WMG_ERROR("dev event snfy_new failed\n");
		return WMG_STATUS_FAIL;
	}

	xr_wifi_init(xr_wifi_sta_cb, WIFI_STATION);

	return status;
}

static wmg_status_t glue_sta_inf_deinit(void)
{
	wmg_status_t status = WMG_STATUS_SUCCESS;

	xr_wifi_deinit();

	if(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->sta_event_snfy_handle) {
		snfy_free(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->sta_event_snfy_handle);
		((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->event = XR_WIFI_DEV_ID_MAX;
	}
	if(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->dev_event_snfy_handle) {
		snfy_free(((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->dev_event_snfy_handle);
	}

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t glue_sta_inf_enable()
{
	wmg_status_t status = WMG_STATUS_SUCCESS;
	xr_wifi_dev_status_t *dev_status;
	wifi_mode_t mode = WIFI_STATION;

	return xr_wifi_on(mode);
}

static wmg_status_t glue_sta_inf_disable()
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	xr_wifi_dev_status_t *dev_status;
	wifi_mode_t mode = WIFI_STATION;

	return xr_wifi_off(mode);
}

static wmg_status_t glue_sta_connect(wifi_sta_cn_para_t *cn)
{
	wmg_status_t status = OS_NET_STATUS_FAILED;
	xr_wifi_sta_cn_t *con = malloc(sizeof(xr_wifi_sta_cn_t));
	wifi_sta_event_t sta_event;
	int connect_result = 0;

	if(cn->ssid == NULL) {
		WMG_ERROR("xrlink os unsupport bssid connect\n");
		event_notify_to_sta_dev(WIFI_DISCONNECTED);
		free(con);
		return WMG_STATUS_UNSUPPORTED;
	}
	memset(con, 0, sizeof(xr_wifi_sta_cn_t));
	con->sec = cn->sec;
	con->fast_connect = cn->fast_connect;
	memcpy(con->ssid, cn->ssid, strlen(cn->ssid));
	con->ssid_len = strlen(cn->ssid);
	memcpy(con->pwd, cn->password, strlen(cn->password));
	con->pwd_len = strlen(cn->password);
	WMG_DEBUG("[wifi_sta_connect] cn->ssid len=%d, %s\n",
			strlen(cn->ssid), cn->ssid);
	WMG_DEBUG("[wifi_sta_connect] cn->password len=%d, %s\n",
			strlen(cn->password), cn->password);

	status = xrlink_send_cmd(sizeof(xr_wifi_sta_cn_t), XR_WIFI_HOST_STA_CONNECT, con);
	while(connect_result == 0) {
		sta_event = get_sta_event();
		WMG_DEBUG("get sta event %d\n", sta_event);
		switch(sta_event) {
			case WIFI_DISCONNECTED:
			case WIFI_NETWORK_NOT_FOUND:
			case WIFI_PASSWORD_INCORRECT:
			case WIFI_AUTH_REJECT:
			case WIFI_ASSOC_REJECT:
			case WIFI_CONNECT_TIMEOUT:
			case WIFI_DEAUTH:
			case WIFI_DHCP_TIMEOUT:
			case WIFI_TERMINATING:
			case WIFI_UNKNOWN:
			case WIFI_DHCP_SUCCESS:
				connect_result = 1;
			break;
		}
	};

	if(sta_event != WIFI_DHCP_SUCCESS){
		status = OS_NET_STATUS_FAILED;
	}

	free(con);
	return status;
}

static wmg_status_t glue_sta_disconnect(void)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;

	status = xrlink_send_cmd(0, XR_WIFI_HOST_STA_DISCONNECT, NULL);

	return status;
}

static wmg_status_t glue_sta_auto_reconnect(wmg_bool_t enable)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;

	uint8_t cfg_mode = enable;

	status = xrlink_send_cmd(sizeof(xr_sta_auto_reconnect_t),
			XR_WIFI_HOST_STA_AUTO_RECONNECT, &cfg_mode);

	return status;
}

static wmg_status_t glue_sta_get_info(wifi_sta_info_t *sta_info)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	xr_wifi_sta_info_t *info;
	uint16_t dev_event_type = XR_WIFI_DEV_ID_MAX;

	status = xrlink_send_cmd(0, XR_WIFI_HOST_STA_GET_INFO, NULL);

	WMG_DEBUG("dev event type:%d\n", dev_event_type);
	info = (xr_wifi_sta_info_t *)get_dev_event(&dev_event_type);
	if (dev_event_type == XR_WIFI_DEV_STA_INFO) {
		sta_info->id = info->id;
		sta_info->freq = info->freq;
		sta_info->sec = info->sec;
		memcpy(sta_info->bssid, info->bssid, 6);
		memcpy(sta_info->ssid, info->ssid, strlen(info->ssid));
		memcpy(sta_info->mac_addr, info->mac_addr, 6);
		memcpy(sta_info->ip_addr, info->ip_addr, 4);
	} else {
		WMG_ERROR("get dev event is not wifi dev sta info\n");
		status = OS_NET_STATUS_FAILED;
	}

	return status;
}

static wmg_status_t glue_sta_list_networks(wifi_sta_list_t *sta_list)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	WMG_DEBUG("list network not support\n");
	return status;
}

static wmg_status_t glue_sta_remove_networks(char *ssid)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	WMG_DEBUG("remove network not support\n");
	return status;
}

static wmg_status_t glue_sta_get_scan_results(get_scan_results_para_t *sta_scan_results_para)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	xr_wifi_scan_result_t *scan_result = NULL;
	xr_wifi_scan_info_t *scan_info = NULL;
	int i;
	uint16_t dev_event_type = XR_WIFI_DEV_ID_MAX;

	status = xrlink_send_cmd(0, XR_WIFI_HOST_GET_SCAN_RES, NULL);
	scan_result = (xr_wifi_scan_result_t *)get_dev_event(&dev_event_type);

	WMG_DEBUG("dev event type:%d\n", dev_event_type);

	if (dev_event_type == XR_WIFI_DEV_SCAN_RES) {
		scan_info = (xr_wifi_scan_info_t *)scan_result->ap_info;
		*(sta_scan_results_para->bss_num) = scan_result->num;
		for (i = 0; i < *(sta_scan_results_para->bss_num); i++) {
			sta_scan_results_para->scan_results[i].freq = (uint32_t)scan_info[i].freq;
			sta_scan_results_para->scan_results[i].rssi = scan_info[i].rssi;
			sta_scan_results_para->scan_results[i].key_mgmt = scan_info[i].key_mgmt;

			memcpy(sta_scan_results_para->scan_results[i].bssid, scan_info[i].bssid, 6);
			if(scan_info[i].ssid_len < SSID_MAX_LEN) {
				memcpy(sta_scan_results_para->scan_results[i].ssid, scan_info[i].ssid, scan_info[i].ssid_len);
				sta_scan_results_para->scan_results[i].ssid[scan_info[i].ssid_len] = '\0';
			} else {
				WMG_WARNG("xrlink get ssid too long(%d)\n", scan_info[i].ssid_len);
			}
		}
	} else {
		WMG_ERROR("get dev event is not wifi dev scan results\n");
		status = OS_NET_STATUS_FAILED;
	}

	return status;
}

static int glue_sta_vendor_send_data(uint8_t *data, uint32_t len)
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

static int glue_sta_vendor_register_rx_cb(wifi_vend_cb_t vend_cb)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	((xrlink_sta_private_data_t *)sta_inf_object.sta_private_data)->vend_cb = vend_cb;
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
		case STA_CMD_CONNECT:
			return glue_sta_connect((wifi_sta_cn_para_t *)cmd_para);
		case STA_CMD_DISCONNECT:
			return glue_sta_disconnect();
		case STA_CMD_SET_AUTO_RECONN:
			{
				wmg_bool_t *enable = (wmg_bool_t *)cmd_para;
				return glue_sta_auto_reconnect(*enable);
			}
		case STA_CMD_GET_INFO:
			return glue_sta_get_info((wifi_sta_info_t *)cmd_para);
		case STA_CMD_LIST_NETWORKS:
			return glue_sta_list_networks((wifi_sta_list_t *)cmd_para);
		case STA_CMD_REMOVE_NETWORKS:
			return glue_sta_remove_networks((char *)cmd_para);
		case STA_CMD_GET_SCAN_RESULTS:
			{
				get_scan_results_para_t *sta_scan_results_para = (get_scan_results_para_t *)cmd_para;
				return glue_sta_get_scan_results(sta_scan_results_para);
			}
		case STA_CMD_VENDOR_SEND_DATA:
			{
				vendor_send_data_para_t *vendor_send_data_para = (vendor_send_data_para_t *)cmd_para;
				return glue_sta_vendor_send_data(vendor_send_data_para->data, vendor_send_data_para->len);
			}
		case STA_CMD_VENDOR_REGISTER_RX_CB:
				return glue_sta_vendor_register_rx_cb((wifi_vend_cb_t)cmd_para);
		default:
		return WMG_STATUS_FAIL;
	}
	return WMG_STATUS_FAIL;
}

static xrlink_sta_private_data_t xrlink_sta_private_data = {
	.sta_event_snfy_handle = NULL,
	.event = WIFI_UNKNOWN,
	.dev_event_snfy_handle = NULL,
	.vend_cb = NULL,
};

static wmg_sta_inf_object_t sta_inf_object = {
	.sta_init_flag = WMG_FALSE,
	.sta_auto_reconn = WMG_FALSE,
	.sta_event_cb = NULL,
	.sta_private_data = &xrlink_sta_private_data,

	.sta_inf_init = glue_sta_inf_init,
	.sta_inf_deinit = glue_sta_inf_deinit,
	.sta_inf_enable = glue_sta_inf_enable,
	.sta_inf_disable = glue_sta_inf_disable,
	.sta_platform_extension = glue_platform_extension,
};

wmg_sta_inf_object_t* sta_xrlink_inf_object_register(void)
{
	return &sta_inf_object;
}
