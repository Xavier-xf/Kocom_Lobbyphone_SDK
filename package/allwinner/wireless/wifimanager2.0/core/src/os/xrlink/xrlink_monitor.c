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
#include <wmg_monitor.h>
#include <wifi_log.h>

typedef struct {
	snfy_handle_t *dev_event_snfy_handle;
	snfy_handle_t *monitor_status_snfy_handle;
	wifi_vend_cb_t vend_cb;
} xrlink_monitor_private_data_t;

static wmg_monitor_inf_object_t monitor_inf_object;
static xrlink_monitor_private_data_t xrlink_monitor_private_data;

static void dev_event_notify(char *data)
{
	snfy_ready(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->dev_event_snfy_handle, data);
}

static void* get_dev_event(uint16_t *type)
{
	char *data = NULL;
	data = snfy_await(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->dev_event_snfy_handle,
				WAIT_DEV_NORMOL_TO_MS);

	if(data != NULL) {
		xr_cfg_payload_t *payload = (xr_cfg_payload_t*) data;
		*type = payload->type;
		return payload->param;
	}
	return data;
}

static void monitor_status_notify(uint8_t *data)
{
	snfy_ready(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->monitor_status_snfy_handle, data);
}

static xr_wifi_dev_status_t *get_monitor_status(void)
{
	return (xr_wifi_dev_status_t *)snfy_await(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->monitor_status_snfy_handle,
		WAIT_DEV_NORMOL_TO_MS);
}

static int xr_wifi_monitor_cb(char *data,uint32_t len)
{
	xr_cfg_payload_t *payload = (xr_cfg_payload_t*) data;
	xr_wifi_sta_cn_event_t *cn_event;
	xr_wifi_raw_data_t *raw_data;

	switch(payload->type) {
		case XR_WIFI_DEV_IND_RAW_DATA:
			raw_data = (xr_wifi_raw_data_t *) payload->param;
			if(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->vend_cb) {
				((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->vend_cb(raw_data->data, raw_data->len);
			}
			break;
		default:
			dev_event_notify(data);
			break;
	}

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t glue_monitor_inf_init(monitor_data_frame_cb_t monitor_data_frame_cb)
{
	wmg_status_t status = WMG_STATUS_SUCCESS;

	monitor_inf_object.monitor_data_frame_cb = monitor_data_frame_cb;

	((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->dev_event_snfy_handle = snfy_new();
	if(!(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->dev_event_snfy_handle)) {
		WMG_ERROR("dev event snfy_new failed\n");
		return WMG_STATUS_FAIL;
	}

	((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->monitor_status_snfy_handle = snfy_new();
	if(!(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->monitor_status_snfy_handle)) {
		snfy_free(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->dev_event_snfy_handle);
		WMG_ERROR("dev status snfy_new failed\n");
		return WMG_STATUS_FAIL;
	}

	xr_wifi_init(xr_wifi_monitor_cb, WIFI_MONITOR);

	return status;
}

static wmg_status_t glue_monitor_inf_deinit(void)
{
	wmg_status_t status = WMG_STATUS_SUCCESS;

	xr_wifi_deinit();

	if(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->dev_event_snfy_handle) {
		snfy_free(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->dev_event_snfy_handle);
	}
	if(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->monitor_status_snfy_handle) {
		snfy_free(((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->monitor_status_snfy_handle);
	}

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t glue_monitor_inf_enable()
{
	wmg_status_t status = WMG_STATUS_SUCCESS;
	xr_wifi_dev_status_t *dev_status;
	wifi_mode_t mode = WIFI_MONITOR;

	return xr_wifi_on(mode);
}

static wmg_status_t glue_monitor_inf_disable()
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	xr_wifi_dev_status_t *dev_status;
	wifi_mode_t mode = WIFI_MONITOR;

	return xr_wifi_off(mode);
}

static wmg_status_t glue_monitor_enable(uint8_t channel)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	xr_wifi_monitor_enable_t mon_arg;

	mon_arg.channel = channel;
	status = xrlink_send_cmd(sizeof(xr_wifi_monitor_enable_t), XR_WIFI_HOST_MONITOR_ENABLE, NULL);

	return status;
}

static wmg_status_t glue_monitor_disable()
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;

	status = xrlink_send_cmd(0, XR_WIFI_HOST_MONITOR_DISABLE, NULL);

	return status;
}

static wmg_status_t glue_monitor_set_channel(uint8_t channel)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;

	return status;
}

static int glue_monitor_vendor_send_data(uint8_t *data, uint32_t len)
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

static int glue_monitor_vendor_register_rx_cb(wifi_vend_cb_t vend_cb)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	((xrlink_monitor_private_data_t *)monitor_inf_object.monitor_private_data)->vend_cb = vend_cb;
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
		case MONITOR_CMD_ENABLE:
			return glue_monitor_enable(*(uint8_t *)cmd_para);
		case MONITOR_CMD_DISABLE:
			return glue_monitor_disable();
		case MONITOR_CMD_SET_CHANNL:
			return glue_monitor_set_channel(*(uint8_t *)cmd_para);
		case MONITOR_CMD_VENDOR_SEND_DATA:
			{
				vendor_send_data_para_t *vendor_send_data_para = (vendor_send_data_para_t *)cmd_para;
				return glue_monitor_vendor_send_data(vendor_send_data_para->data, vendor_send_data_para->len);
			}
		case MONITOR_CMD_VENDOR_REGISTER_RX_CB:
				return glue_monitor_vendor_register_rx_cb((wifi_vend_cb_t)cmd_para);
		default:
		return WMG_STATUS_FAIL;
	}
	return WMG_STATUS_FAIL;
}

static xrlink_monitor_private_data_t xrlink_monitor_private_data = {
	.dev_event_snfy_handle = NULL,
	.monitor_status_snfy_handle = NULL,
	.vend_cb = NULL,
};

static wmg_monitor_inf_object_t monitor_inf_object = {
	.monitor_init_flag = WMG_FALSE,
	.monitor_enable = WMG_FALSE,
	.monitor_state = NULL,
	.monitor_data_frame_cb = NULL,
	.monitor_channel = 255,
	.monitor_private_data = &xrlink_monitor_private_data,

	.monitor_inf_init = glue_monitor_inf_init,
	.monitor_inf_deinit = glue_monitor_inf_deinit,
	.monitor_inf_enable = glue_monitor_inf_enable,
	.monitor_inf_disable = glue_monitor_inf_disable,
	.monitor_platform_extension = glue_platform_extension,
};

wmg_monitor_inf_object_t* monitor_xrlink_inf_object_register(void)
{
	return &monitor_inf_object;
}
