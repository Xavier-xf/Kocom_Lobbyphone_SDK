/*
 * Copyright (C) 2017 XRADIO TECHNOLOGY CO., LTD. All rights reserved.
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
#include <stdlib.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wifimg.h>
#include <wifimg_glue.h>
#include <os_net_sync_notify.h>
#include <xrlink_common.h>
#include <wifi_log.h>
#include <expand_cmd.h>

wmg_status_t wmg_xrlink_send_expand_cmd_shell(char *expand_cmd, void *expand_cb)
{
	system(expand_cmd);
}

wmg_status_t wmg_xrlink_send_expand_cmd_gmac(char *expand_cmd, void *expand_cb)
{
	char ifname[10] = {0};
	uint8_t *mac_addr = (uint8_t *)expand_cb;

	strcpy(ifname, expand_cmd);
	if(ifname == NULL) {
		WMG_ERROR("ifname is NULL\n");
		return WMG_STATUS_FAIL;
	}

	xr_wifi_mac_info_t *mac;

	xrlink_send_cmd(0, XR_WIFI_HOST_GET_MAC, NULL);
	mac = (xr_wifi_mac_info_t *)xr_get_dev_event_status(XR_WIFI_DEV_MAC);
	if(mac != NULL) {
		memcpy(mac_addr, mac->mac, 6);
		return WMG_STATUS_SUCCESS;
	}

	return WMG_STATUS_FAIL;
}

wmg_status_t wmg_xrlink_send_expand_cmd_smac(char *expand_cmd, void *expand_cb)
{
	wmg_status_t status = WMG_STATUS_UNHANDLED;
	uint8_t mac_addr[6] = {0};
	char ifname[10] = {0};
	char *pch;
	int i;

	pch = strtok(expand_cmd, ":");
	strcpy(ifname, pch);
	pch = strtok(NULL, ":");
	pch++;
	for(i = 0;(pch != NULL) && (i < 6); i++){
		mac_addr[i] = char2uint8(pch);
		pch = strtok(NULL, ":");
	}

	if(i != 6) {
		WMG_ERROR("%s: mac address format is incorrect\n", expand_cmd);
		return WMG_STATUS_FAIL;
	}

	xr_wifi_mac_info_t *mi = (xr_wifi_mac_info_t *)malloc(sizeof(xr_wifi_mac_info_t));

	memset(mi, 0, sizeof(xr_wifi_mac_info_t));
	memcpy(mi->mac, mac_addr, 6);
	memcpy(mi->ifname, ifname, 5);
	status = xrlink_send_cmd(sizeof(xr_wifi_mac_info_t), XR_WIFI_HOST_SET_MAC, mi);
	free(mi);
	return status;
}

wmg_status_t wmg_xrlink_send_expand_cmd(char *expand_cmd, void *expand_cb)
{
	WMG_DEBUG("xrlink get exp cmd: %s\n", expand_cmd);
	if(!strncmp(expand_cmd, "shell:", 6)) {
		return wmg_xrlink_send_expand_cmd_shell((expand_cmd + 7), expand_cb);
	} else if(!strncmp(expand_cmd, "gmac:", 5)) {
		return wmg_xrlink_send_expand_cmd_gmac((expand_cmd + 6), expand_cb);
	} else if(!strncmp(expand_cmd, "smac:", 5)) {
		return wmg_xrlink_send_expand_cmd_smac((expand_cmd + 6), expand_cb);
	} else {
		WMG_ERROR("unspport xrlink expand_cmd: %s\n", expand_cmd);
	}
	return WMG_STATUS_FAIL;
}
