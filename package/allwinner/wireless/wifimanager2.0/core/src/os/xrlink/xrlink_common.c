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


#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <wifi_log.h>
#include <os_net_utils.h>
#include <os_net_sync_notify.h>
#include <xrlink_common.h>
#include <msg_netlink.h>
#include <xr_proto.h>
#include <netdb.h>

#define STA_MSG_CB_NUM       0
#define AP_MSG_CB_NUM        1
#define MONITOR_MSG_CB_NUM   2

uint8_t char2uint8(char* trs)
{
	uint8_t ret = 0;
	uint8_t tmp_ret[2] = {0};
	int i = 0;
	for(; i < 2; i++) {
		switch (*(trs + i)) {
			case '0' :
				tmp_ret[i] = 0x0;
				break;
			case '1' :
				tmp_ret[i] = 0x1;
				break;
			case '2' :
				tmp_ret[i] = 0x2;
				break;
			case '3' :
				tmp_ret[i] = 0x3;
				break;
			case '4' :
				tmp_ret[i] = 0x4;
				break;
			case '5' :
				tmp_ret[i] = 0x5;
				break;
			case '6' :
				tmp_ret[i] = 0x6;
				break;
			case '7' :
				tmp_ret[i] = 0x7;
				break;
			case '8' :
				tmp_ret[i] = 0x8;
				break;
			case '9' :
				tmp_ret[i] = 0x9;
				break;
			case 'a' :
			case 'A' :
				tmp_ret[i] = 0xa;
				break;
			case 'b' :
			case 'B' :
				tmp_ret[i] = 0xb;
				break;
			case 'c' :
			case 'C' :
				tmp_ret[i] = 0xc;
				break;
			case 'd' :
			case 'D' :
				tmp_ret[i] = 0xd;
				break;
			case 'e' :
			case 'E' :
				tmp_ret[i] = 0xe;
				break;
			case 'f' :
			case 'F' :
				tmp_ret[i] = 0xf;
		break;
		}
	}
	ret = ((tmp_ret[0] << 4) | tmp_ret[1]);
	return ret;
}

typedef struct {
	int init_flag;
	uint8_t current_mode_bitmap;
	nlmsg_priv_t *xr_nlmsg;
	snfy_handle_t *dev_snfy_handle;
	xr_wifi_msg_cb_t wifi_msg_cb[3];
} xrlink_common_object_t;

static xrlink_common_object_t xrlink_common_object;

#define U16_F "hu"

#define DATA_LEN_ALIGNMENT 4

#define XRNL_MSG_STATE_CK()                         \
    do {                                            \
		if(NULL == xrlink_common_object.xr_nlmsg) {                      \
			WMG_ERROR("xrnl msg is not ready\n");   \
			return OS_NET_STATUS_NOT_READY;         \
		}                                           \
	} while(0)


static xr_cfg_payload_t *xr_wifi_cfg_payload_calloc(uint32_t len,uint16_t type,void *param)
{
	xr_cfg_payload_t *cfg_payload = NULL;
	int tot_len;
	tot_len = CMD_HEAD_LEN + len;
	tot_len +=DATA_LEN_ALIGNMENT - tot_len%DATA_LEN_ALIGNMENT;

	WMG_DEBUG("header len:%d,payload len:%d,tot_len:%d\n",CMD_HEAD_LEN,len,tot_len);
	cfg_payload = (xr_cfg_payload_t *) malloc(tot_len);

	cfg_payload->type = type;
	cfg_payload->len = len;
	if(param)
		memcpy(cfg_payload->param,param,len);
	return cfg_payload;
}

static void xr_wifi_cfg_payload_free(xr_cfg_payload_t *cfg_payload)
{
	if(cfg_payload) {
		free(cfg_payload);
	}
}

os_net_status_t xrlink_send_cmd(uint32_t len,cfg_host_op_t type,void *param)
{
	os_net_status_t status;
	XRNL_MSG_STATE_CK();

	xr_cfg_payload_t *cfg_payload = xr_wifi_cfg_payload_calloc(len, type, param);
	if(!cfg_payload) {
		return OS_NET_STATUS_NOMEM;
	}

	status = nlmsg_send(xrlink_common_object.xr_nlmsg,(char*)cfg_payload,cfg_payload->len + sizeof(xr_cfg_payload_t));
	if(status != OS_NET_STATUS_OK) {
		WMG_ERROR("nlmsg send failed\n");
	}

	xr_wifi_cfg_payload_free(cfg_payload);
	return status;
}

static int get_net_ip(const char *if_name, char *ip, int *len, int *vflag)
{
	struct ifaddrs *ifAddrStruct = NULL, *pifaddr = NULL;
	void *tmpAddrPtr = NULL;

	*vflag = 0;
	getifaddrs(&ifAddrStruct);
	pifaddr = ifAddrStruct;
	while (pifaddr != NULL) {
		if (NULL == pifaddr->ifa_addr) {
			pifaddr=pifaddr->ifa_next;
			continue;
		} else if (pifaddr->ifa_addr->sa_family == AF_INET) { // check it is IP4
			tmpAddrPtr = &((struct sockaddr_in *)pifaddr->ifa_addr)->sin_addr;
			if (strcmp(pifaddr->ifa_name, if_name) == 0) {
				inet_ntop(AF_INET, tmpAddrPtr, ip, INET_ADDRSTRLEN);
				*vflag = 4;
				break;
			}
		} else if (pifaddr->ifa_addr->sa_family == AF_INET6) { // check it is IP6
			// is a valid IP6 Address
			tmpAddrPtr = &((struct sockaddr_in *)pifaddr->ifa_addr)->sin_addr;
			if (strcmp(pifaddr->ifa_name, if_name) == 0) {
				inet_ntop(AF_INET6, tmpAddrPtr, ip, INET6_ADDRSTRLEN);
				*vflag=6;
				break;
			}
		}
		pifaddr=pifaddr->ifa_next;
	}

	if (ifAddrStruct != NULL) {
		freeifaddrs(ifAddrStruct);
	}

	return 0;
}

static void xr_wifi_config_ip_address(void *arg)
{
	char config_ip[64];
	char ip_addr[16];
	char netmask[16];
	char gw[16];
	char dns[16];

	int len = 0, vflag = 0;
	char ipaddr[INET6_ADDRSTRLEN];

	xr_wifi_sta_ip_info_t *ip_info = (xr_wifi_sta_ip_info_t *)arg;

	sprintf(ip_addr, "%"U16_F".%"U16_F".%"U16_F".%"U16_F"%c",
			(unsigned short)ip_info->ip_addr[0], (unsigned short)ip_info->ip_addr[1],
			(unsigned short)ip_info->ip_addr[2], (unsigned short)ip_info->ip_addr[3], '\0');

	sprintf(netmask, "%"U16_F".%"U16_F".%"U16_F".%"U16_F"%c",
			(unsigned short)ip_info->netmask[0], (unsigned short)ip_info->netmask[1],
			(unsigned short)ip_info->netmask[2], (unsigned short)ip_info->netmask[3], '\0');

	sprintf(gw, "%"U16_F".%"U16_F".%"U16_F".%"U16_F"%c",
			(unsigned short)ip_info->gw[0], (unsigned short)ip_info->gw[1],
			(unsigned short)ip_info->gw[2], (unsigned short)ip_info->gw[3], '\0');

	sprintf(dns, "%"U16_F".%"U16_F".%"U16_F".%"U16_F"%c",
			(unsigned short)ip_info->dns[0], (unsigned short)ip_info->dns[1],
			(unsigned short)ip_info->dns[2], (unsigned short)ip_info->dns[3], '\0');
	WMG_DEBUG("ip address:%s\n", ip_addr);
	WMG_DEBUG("netmask:%s\n", netmask);
	WMG_DEBUG("gw:%s\n", gw);
	WMG_DEBUG("dns:%s\n", dns);

	sprintf(config_ip,"ifconfig wlan0 %s netmask %s%c",
			ip_addr,netmask,'\0');
	WMG_DEBUG("config ip:%s\n", config_ip);
	system(config_ip);

	sprintf(config_ip,"route add default gw %s%c",gw,'\0');

	WMG_DEBUG("config gw:%s\n", config_ip);
	system(config_ip);

	sprintf(config_ip,"echo nameserver %s > /etc/resolv.conf%c",dns,'\0');
	WMG_DEBUG("config dns:%s\n", config_ip);
	system(config_ip);
}

static void callback_msg(uint8_t mode_bitmap, char *data, uint32_t len){
	if(mode_bitmap & WIFI_STATION) {
		if(xrlink_common_object.wifi_msg_cb[STA_MSG_CB_NUM] != NULL) {
			xrlink_common_object.wifi_msg_cb[STA_MSG_CB_NUM](data, len);
		}
	}
	if(mode_bitmap & WIFI_AP) {
		if(xrlink_common_object.wifi_msg_cb[AP_MSG_CB_NUM] != NULL) {
			xrlink_common_object.wifi_msg_cb[AP_MSG_CB_NUM](data, len);
		}
	}
	if(mode_bitmap & WIFI_MONITOR) {
		if(xrlink_common_object.wifi_msg_cb[MONITOR_MSG_CB_NUM] != NULL) {
			xrlink_common_object.wifi_msg_cb[MONITOR_MSG_CB_NUM](data, len);
		}
	}
}

static void xr_wifi_recv_cb(char *data,uint32_t len)
{
	xr_cfg_payload_t *payload = (xr_cfg_payload_t*) data;
	xr_wifi_sta_cn_event_t *cn_event;
	xr_wifi_raw_data_t *raw_data;
	xr_wifi_dev_status_t *d_status;

	WMG_DEBUG("dev ind msg type:%d\n", payload->type);

	switch(payload->type) {
		case XR_WIFI_DEV_IND_IP_INFO: //16
			xr_wifi_config_ip_address(payload->param);
			break;
		case XR_WIFI_DEV_MAC://6
			snfy_ready(xrlink_common_object.dev_snfy_handle, data);
			break;
		default:
			callback_msg(xrlink_common_object.current_mode_bitmap, data, len);
			break;
	}
}

os_net_status_t xr_wifi_init(xr_wifi_msg_cb_t wifi_msg_cb, wifi_mode_t mode)
{
	if(!xrlink_common_object.init_flag) {
		xrlink_common_object.xr_nlmsg = nlmsg_init(xr_wifi_recv_cb);
		if(!xrlink_common_object.xr_nlmsg) {
			return OS_NET_STATUS_FAILED;
		}
		xrlink_common_object.dev_snfy_handle = snfy_new();
		if(!(xrlink_common_object.dev_snfy_handle)) {
			nlmsg_deinit(xrlink_common_object.xr_nlmsg);
			return OS_NET_STATUS_FAILED;
		}
		xrlink_common_object.init_flag = 1;
	}
	if(mode & WIFI_STATION) {
		xrlink_common_object.wifi_msg_cb[STA_MSG_CB_NUM] = wifi_msg_cb;
	} else if(mode & WIFI_AP) {
		xrlink_common_object.wifi_msg_cb[AP_MSG_CB_NUM] = wifi_msg_cb;
	} else if(mode & WIFI_MONITOR) {
		xrlink_common_object.wifi_msg_cb[MONITOR_MSG_CB_NUM] = wifi_msg_cb;
	}
	return OS_NET_STATUS_OK;
}

os_net_status_t xr_wifi_deinit(void)
{
	if(xrlink_common_object.init_flag) {
		WMG_DEBUG("[xr_wifi_off] nlmsg_deinit\r\n");
		nlmsg_deinit(xrlink_common_object.xr_nlmsg);
		if(xrlink_common_object.dev_snfy_handle) {
			snfy_free(xrlink_common_object.dev_snfy_handle);
		}
		xrlink_common_object.init_flag = 0;
		xrlink_common_object.wifi_msg_cb[STA_MSG_CB_NUM] = NULL;
		xrlink_common_object.wifi_msg_cb[AP_MSG_CB_NUM] = NULL;
		xrlink_common_object.wifi_msg_cb[MONITOR_MSG_CB_NUM] = NULL;
	}
	return OS_NET_STATUS_OK;
}

os_net_status_t xr_wifi_on(wifi_mode_t mode)
{
	xr_wifi_mode_t xr_mode = XR_WIFI_MODE_UNKNOWN;
	os_net_status_t status;
	if(mode & WIFI_STATION) {
		xr_mode = XR_WIFI_STATION;
	} else if (mode & WIFI_AP) {
		xr_mode = XR_WIFI_AP;
	} else if (mode & WIFI_MONITOR) {
		xr_mode = XR_WIFI_MONITOR;
	}
	status = xrlink_send_cmd(sizeof(xr_wifi_mode_t), XR_WIFI_HOST_ON, &xr_mode);
	if(status == OS_NET_STATUS_OK) {
		xrlink_common_object.current_mode_bitmap |= mode;
	}
	return status;
}

os_net_status_t xr_wifi_off(wifi_mode_t mode)
{
	os_net_status_t status = OS_NET_STATUS_OK;
	xrlink_common_object.current_mode_bitmap &= (~mode);

	if(!xrlink_common_object.current_mode_bitmap) {
		status = xrlink_send_cmd(0, XR_WIFI_HOST_OFF, NULL);
		if(status != OS_NET_STATUS_OK) {
			xrlink_common_object.current_mode_bitmap |= mode;
		}
	}
	return status;
}

void *xr_get_dev_event_status(uint16_t type)
{
	char *data = NULL;
	data = snfy_await(xrlink_common_object.dev_snfy_handle, WAIT_DEV_NORMOL_TO_MS);

	if(data != NULL) {
		xr_cfg_payload_t *payload = (xr_cfg_payload_t*) data;
		if(type == payload->type) {
			return payload->param;
		}
	}
	return NULL;
}

static xrlink_common_object_t xrlink_common_object = {
	.init_flag = 0,
	.current_mode_bitmap = 0,
	.xr_nlmsg = NULL,
	.dev_snfy_handle = NULL,
	.wifi_msg_cb = {NULL, NULL, NULL},
};
