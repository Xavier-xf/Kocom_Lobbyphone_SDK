#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <pthread.h>
#include <wpa_ctrl.h>
#include <udhcpc.h>
#include <utils.h>
#include <wifi_log.h>
#include <wifimg.h>
#include <wmg_ap.h>
#include <unistd.h>
#include <linux_ap.h>
#include <linux_common.h>

static wmg_ap_inf_object_t ap_inf_object;

#define AP_SEC_NONE        0
#define AP_SEC_WPA         1
#define AP_SEC_WPA2        2
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define HOSTAPD_TIMEOUT      3000000  // microseconds
#define HOSTAPD_TIMEOUT_STEP  100000  // microseconds

typedef struct {
	wifi_ap_config_t *ap_config;
	bool ap_config_enable;
} global_ap_config_t;

static char global_ssid[SSID_MAX_LEN + 1] = {0};
static char global_psk[PSK_MAX_LEN + 1] = {0};

static wifi_ap_config_t global_wifi_ap_config = {
	.ssid = global_ssid,
	.psk = global_psk,
	.sec = WIFI_SEC_NONE,
	.channel = 0,
	.dev_list = NULL,
	.sta_num = 0,
};

static global_ap_config_t global_ap_config = {
	.ap_config = &global_wifi_ap_config,
	.ap_config_enable = false,
};

static wmg_status_t parse_ap_config(char *reply, wifi_ap_config_t *ap_config)
{
	char *pos = NULL;
	char tmp[SSID_MAX_LEN + 1] = {0};
	char *pch;

	if (reply == NULL || ap_config == NULL) {
		WMG_ERROR("invalid parameters\n");
		return WMG_STATUS_INVALID;
	}

	pos = strstr(reply, "state");
	if (pos != NULL) {
		pos += 6;
		if (strncmp(pos, "ENABLED", 7) != 0) {
			WMG_WARNG("Warning: ap state is inactive\n");
			return WMG_STATUS_FAIL;
		} else {
			WMG_DUMP("ap state is enable\n");
			pch = strtok(reply, "'\n'");
			while (pch != NULL) {
				if (strncmp(pch, "ssid[0]=", 8) == 0) {
					if(strlen(pch + 8) > SSID_MAX_LEN) {
						WMG_DEBUG("ssid is too long:%d",strlen(pch + 8));
						return WMG_STATUS_INVALID;
					}
					strcpy(ap_config->ssid, (pch + 8));
					WMG_DEBUG("%s\n", ap_config->ssid);
				}
				if (strncmp(pch, "channel=", 8) == 0) {
					ap_config->channel = atoi(pch + 8);
					WMG_DEBUG("%d\n", ap_config->channel);
				}
				if (strncmp(pch, "num_sta[0]=", 11) == 0) {
					ap_config->sta_num = atoi(pch + 11);
					WMG_DEBUG("%d\n", ap_config->sta_num);
				}
				pch = strtok(NULL, "'\n'");
			}
			return WMG_STATUS_SUCCESS;
		}
	} else {
		WMG_ERROR("invalid ap config info %s\n", reply);
	}

	return WMG_STATUS_FAIL;
}

static wmg_status_t linux_command_to_hostapd(char const *cmd, char *reply, size_t reply_len)
{
	return command_to_wpad(cmd, reply, reply_len, WIFI_AP);
}

static wmg_status_t linux_ap_set_config(wifi_ap_config_t *ap_config)
{
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};
	int passwd_len = 0;

	if((ap_config == NULL) || (ap_config->ssid == NULL)){
		WMG_ERROR("ap config format is error, ssid can't NULL\n");
		return WMG_STATUS_FAIL;
	}

	WMG_DUMP("set ssid %s\n", ap_config->ssid);
	sprintf(cmd, "SET ssid %s", ap_config->ssid);
	if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
		WMG_ERROR("failed to set ssid '%s', reply %s\n",ap_config->ssid, reply);
		return WMG_STATUS_FAIL;
	}

	if (ap_config->channel < 14) {
		WMG_DUMP("set channel %u\n", ap_config->channel);
		sprintf(cmd, "SET channel %u", ap_config->channel);
		if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
			WMG_ERROR("failed to set channel '%u', reply %s\n", ap_config->channel, reply);
			return WMG_STATUS_FAIL;
		}
	} else {
		WMG_ERROR("invalid channel\n");
		return WMG_STATUS_FAIL;
	}

	switch (ap_config->sec){
		case WIFI_SEC_NONE:
			WMG_DUMP("psk is NULL, set wpa NONE\n");
			sprintf(cmd, "SET wpa %d", AP_SEC_NONE);
			if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
				WMG_ERROR("failed to set wpa AP_SEC_NONE, reply %s\n", reply);
				return WMG_STATUS_FAIL;
			}
			break;
		case WIFI_SEC_WEP:
			sprintf(cmd, "SET wep_default_key 0");
			if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
				WMG_ERROR("failed to set wpa_default_key 0, reply %s\n");
				return WMG_STATUS_FAIL;
			}
			WMG_DUMP("set wep_default_key 0\n");
			passwd_len = strlen(ap_config->psk);
			if((passwd_len == 10) || (passwd_len == 26)) {
				sprintf(cmd, "SET wep_key0 %s", ap_config->psk);
				WMG_DEBUG("set passwd HEX format!\n");
			} else if((passwd_len == 5) || (passwd_len == 13)) {
				sprintf(cmd, "SET wep_key0 \"%s\"", ap_config->psk);
				WMG_DEBUG("The passwd is ASCII format!\n");
			} else {
				WMG_ERROR("The password does not conform to the specification!\n");
				return WMG_STATUS_FAIL;
			}
			if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
				WMG_ERROR("failed to set wep_key0 '%s', reply %s\n", ap_config->psk, reply);
				return WMG_STATUS_FAIL;
			}
			WMG_DUMP("set wep_key0 %s\n", ap_config->psk);
			break;
		case WIFI_SEC_WPA_PSK:
			if (ap_config->psk != NULL) {
				sprintf(cmd, "SET wpa %d", AP_SEC_WPA);
				if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
					WMG_ERROR("failed to set wpa AP_SEC_WPA, reply %s\n", reply);
					return WMG_STATUS_FAIL;
				}
				WMG_DUMP("set wpa AP_SEC_WPA\n");
				sprintf(cmd, "SET wpa_key_mgmt WPA-PSK");
				if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
					WMG_ERROR("failed to set wpa key mgmt WPA-PSK, reply %s\n", reply);
					return WMG_STATUS_FAIL;
				}
				passwd_len = strlen(ap_config->psk);
				if(passwd_len < 8) {
					WMG_ERROR("Password shorter than 8 characters!\n");
					return WMG_STATUS_FAIL;
				}
				sprintf(cmd, "SET rsn_pairwise CCMP");
				if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
					WMG_ERROR("failed to set rsn_pairwise CCMP");
					return WMG_STATUS_FAIL;
				}
				sprintf(cmd, "SET wpa_passphrase %s", ap_config->psk);
				if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
					WMG_ERROR("failed to set psk '%s', reply %s\n",ap_config->psk, reply);
					return WMG_STATUS_FAIL;
				}
				WMG_DUMP("set psk %s\n", ap_config->psk);
			} else {
				WMG_ERROR("psk is NULL, but sec not WIFI_SEC_NONE(WIFI_SEC_WPA_PSK)\n");
				WMG_ERROR("Please check ap config format\n");
				return WMG_STATUS_FAIL;
			}
			break;
		case WIFI_SEC_WPA2_PSK:
			if (ap_config->psk != NULL) {
				sprintf(cmd, "SET wpa %d", AP_SEC_WPA2);
				if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
					WMG_ERROR("failed to set wpa AP_SEC_WPA2, reply %s\n", reply);
					return WMG_STATUS_FAIL;
				}
				WMG_DUMP("set wpa AP_SEC_WPA2\n");
				sprintf(cmd, "SET wpa_key_mgmt WPA-PSK");
				if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
					WMG_ERROR("failed to set wpa key mgmt WPA-PSK, reply %s\n", reply);
					return WMG_STATUS_FAIL;
				}
				passwd_len = strlen(ap_config->psk);
				if(passwd_len < 8) {
					WMG_ERROR("Password shorter than 8 characters!\n");
					return WMG_STATUS_FAIL;
				}
				sprintf(cmd, "SET rsn_pairwise CCMP");
				if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
					WMG_ERROR("failed to set rsn_pairwise CCMP");
					return WMG_STATUS_FAIL;
				}
				sprintf(cmd, "SET wpa_passphrase %s", ap_config->psk);
				if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
					WMG_ERROR("failed to set psk '%s', reply %s\n",ap_config->psk, reply);
					return WMG_STATUS_FAIL;
				}
				WMG_DUMP("set psk %s\n", ap_config->psk);
			} else {
				WMG_ERROR("psk is NULL, but sec not WIFI_SEC_NONE(WIFI_SEC_WPA2_PSK)\n");
				WMG_ERROR("Please check ap config format\n");
				return WMG_STATUS_FAIL;
			}
			break;
		default:
			WMG_ERROR("not support sec\n");
			WMG_ERROR("Please check ap config format\n");
			return WMG_STATUS_FAIL;
			break;
	}

	strcpy(global_ap_config.ap_config->ssid, ap_config->ssid);
	strcpy(global_ap_config.ap_config->psk, ap_config->psk);
	global_ap_config.ap_config->sec = ap_config->sec;
	global_ap_config.ap_config->channel = ap_config->channel;
	global_ap_config.ap_config_enable = true;

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t linux_ap_cmd_enable()
{
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "ENABLE", CMD_MAX_LEN);
	if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
		WMG_ERROR("failed to enable ap, reply %s\n", reply);
		return WMG_STATUS_FAIL;
	}
	return WMG_STATUS_SUCCESS;
}

static int linux_ap_cmd_disable()
{
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "DISABLE", CMD_MAX_LEN);
	if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
		WMG_ERROR("failed to disable ap, reply %s\n", reply);
		return WMG_STATUS_FAIL;
	}
	return WMG_STATUS_SUCCESS;
}

static int linux_ap_cmd_status()
{
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "STATUS", CMD_MAX_LEN);
	if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
		WMG_ERROR("failed to get ap status\n");
		return -1;
	}
	if (strstr(reply, "state=ENABLED") != NULL) {
		WMG_DUMP("ap status is: enable\n");
		return 0;
	} else if (strstr(reply, "state=DISABLED") != NULL) {
		WMG_DUMP("ap status is: disable\n");
		return 1;
	} else {
		WMG_DUMP("ap status is unknowd\n");
		return -1;
	}
}

static wmg_status_t linux_ap_cmd_reload()
{
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "RELOAD", CMD_MAX_LEN);
	if(linux_command_to_hostapd(cmd, reply, sizeof(reply))) {
		WMG_ERROR("failed to reload ap config, reply %s\n", reply);
		return WMG_STATUS_FAIL;
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t wmg_sta_device_add(dev_node_t *list_head, char *bssid)
{
	dev_node_t *new = NULL;
	int i = 0;
	char *pch = NULL;

	if (list_head == NULL) {
		WMG_ERROR("head node is NULL!\n");
		return WMG_STATUS_INVALID;
	}

	new = (dev_node_t *)malloc(sizeof(dev_node_t));
	if(new == NULL) {
		WMG_ERROR("failed to allocate memory for device node\n");
		return WMG_STATUS_FAIL;
	}
	new->next = NULL;
	memset(new->bssid, 0, (sizeof(uint8_t) * 6));
	pch = strtok(bssid, ":");
	for(;((pch != NULL) && (i < 6)); i++) {
		new->bssid[i] = char2uint8(pch);
		pch = strtok(NULL, ":");
	}
	new->next = list_head->next;
	list_head->next = new;

	return WMG_STATUS_SUCCESS;
}

static int wmg_sta_device_remove(dev_node_t *list_head, char *bssid_char)
{
	int ret = -1;
	dev_node_t *pre = list_head;
	dev_node_t *p = list_head->next;
	uint8_t bssid[6];
	int i = 0;
	char *pch;

	pch = strtok(bssid_char, ":");
	for(; (pch != NULL) && (i < 6); i++) {
		bssid[i] = char2uint8(pch);
		pch = strtok(NULL, ":");
	}

	while (p != NULL) {
		if(!memcmp(p->bssid, bssid, (sizeof(uint8_t) * 6))){
			WMG_DUMP("remove device '%s'\n", bssid_char);
			pre->next = p->next;
			free(p);
			ret = 0;
			break;
		}
		pre = p;
		p = p->next;
	}

	if (ret == -1)
		WMG_WARNG("%s is not found, remove device failed\n");

	return ret;
}

static void wmg_sta_device_print(dev_node_t *list_head)
{
	dev_node_t *p = list_head->next;
	int i = 1;

	while (p != NULL) {
		WMG_DUMP("the %dth device is '%x%x:%x%x:%x%x:%x%x:%x%x:%x%x'\n", i,
								(p->bssid[0] >> 4),
								(p->bssid[0] & 0xf),
								(p->bssid[1] >> 4),
								(p->bssid[1] & 0xf),
								(p->bssid[2] >> 4),
								(p->bssid[2] & 0xf),
								(p->bssid[3] >> 4),
								(p->bssid[3] & 0xf),
								(p->bssid[4] >> 4),
								(p->bssid[4] & 0xf),
								(p->bssid[5] >> 4),
								(p->bssid[5] & 0xf));
		p = p->next;
		i++;
	}
}

static void ap_event_notify_to_ap_dev(wifi_ap_event_t event)
{
	if (ap_inf_object.ap_event_cb) {
		ap_inf_object.ap_event_cb(event);
	}
}

static void ap_event_notify(wifi_ap_event_t event)
{
	evt_send(ap_inf_object.ap_event_handle, event);
	ap_event_notify_to_ap_dev(event);
}

static int ap_dispatch_event(const char *event_str, int nread)
{
	int i = 0, event = 0, ret;
	char event_nae[18];
	char cmd[255] = {0}, reply[16] = {0};
	char *nae_start = NULL, *nae_end = NULL;
	char *event_data = NULL;

	if (!event_str || !event_str[0]) {
		WMG_WARNG("hostapd event is NULL!\n");
		return 0;
	}

	WMG_DUMP("receive ap event: '%s'\n", event_str);
	if (strstr(event_str, "AP-ENABLED")) {
		event = WIFI_AP_ENABLED;
		ap_event_notify(event);
	} else if (strstr(event_str, "AP-DISABLED")) {
		event = WIFI_AP_DISABLED;
		ap_event_notify(event);
	} else if (strstr(event_str, "AP-STA-CONNECTED")) {
		event_data = strchr(event_str, ' ');
		if (event_data) {
			event_data++;
			WMG_DUMP("bssid=%s\n", event_data);
			ret = wmg_sta_device_add(ap_inf_object.dev_list, event_data);
			if (ret == 0)
				ap_inf_object.sta_num++;
			wmg_sta_device_print(ap_inf_object.dev_list);
		}
		event = WIFI_AP_STA_CONNECTED;
		ap_event_notify(event);
	} else if (strstr(event_str, "AP-STA-DISCONNECTED")) {
		event_data = strchr(event_str, ' ');
		if (event_data) {
			event_data++;
			ret = wmg_sta_device_remove(ap_inf_object.dev_list, event_data);
			if (ret == 0)
				ap_inf_object.sta_num--;
			wmg_sta_device_print(ap_inf_object.dev_list);
		}
		event = WIFI_AP_STA_DISCONNECTED;
		ap_event_notify(event);
	} else {
		event = WIFI_AP_UNKNOWN;
	}

	return 0;
}

static wmg_status_t linux_ap_enable(wifi_ap_config_t *ap_config)
{
	wmg_status_t ret = WMG_STATUS_FAIL;
	char cmd[255] = {0};

	if(ap_inf_object.ap_enable_flag == WMG_TRUE) {
		WMG_WARNG("ap mode has been enable\n");
		return WMG_STATUS_UNHANDLED;
	}

	init_wpad_para_t hostapd_para = {
		.wifi_mode = WIFI_AP,
		.dispatch_event = ap_dispatch_event,
		.linux_mode_private_data = ap_inf_object.ap_private_data,
	};

	if(init_wpad(hostapd_para)) {
		WMG_ERROR("init hostapd failed\n");
		return WMG_STATUS_FAIL;
	} else {
		if(evt_socket_init(ap_inf_object.ap_event_handle)) {
			WMG_ERROR("failed to initialize linux ap event socket\n");
			deinit_wpad(WIFI_AP);;
		}
		WMG_DEBUG("init hostapd success\n");
		ap_inf_object.ap_enable_flag = WMG_TRUE;
	}

	ap_inf_object.dev_list = (dev_node_t *)malloc(sizeof(dev_node_t));
	if (ap_inf_object.dev_list != NULL) {
		memset(ap_inf_object.dev_list->bssid, 0, (sizeof(uint8_t) * 6));
		ap_inf_object.dev_list->next = NULL;
	} else {
		WMG_ERROR("failed to allocate memory for device list\n");
		return WMG_STATUS_NOMEM;
	}

	ret = linux_ap_cmd_status();
	if(ret == 1){
		WMG_DEBUG("hapd is already disable\n");
	} else if (ret == 0){
		WMG_DEBUG("hapd is already enable, disable now...\n");
		ret = linux_ap_cmd_disable();
		if(ret){
			WMG_ERROR("reload hapd config faile\n");
			return WMG_STATUS_FAIL;
		}
	} else {
		WMG_ERROR("failed to get ap hapd status\n");
		return WMG_STATUS_FAIL;
	}

	ret = linux_ap_set_config(ap_config);
	if(ret){
		WMG_ERROR("failed to set ap hapd config\n");
		return WMG_STATUS_FAIL;
	}

	WMG_DEBUG("Enable ap hapd now...\n");
	ret = linux_ap_cmd_enable();
	if(ret){
		WMG_ERROR("failed to enable ap hapd\n");
		return WMG_STATUS_FAIL;
	}

	sprintf(cmd, "ifconfig %s 192.168.5.1", (get_hapd_ap_iface() == NULL) ? "wlan0" : get_hapd_ap_iface());
	wmg_system(cmd);
	wmg_system("dnsmasq -C /etc/wifi/dnsmasq.conf -l /var/dnsmasq.leases -x /var/dnsmasq.pid");

	if (check_process_is_exist("dnsmasq", 7)) {
		WMG_DEBUG("start dnsmasq success\n");
	} else {
		sleep(1);
		if (!check_process_is_exist("dnsmasq", 7)) {
			WMG_ERROR("start dnsmasq failed\n");
			return WMG_STATUS_FAIL;
		}
	}
	ap_inf_object.ap_enable_flag = WMG_TRUE;

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t linux_ap_disable()
{
	wmg_status_t ret;
	dev_node_t *p = ap_inf_object.dev_list;
	dev_node_t *temp = NULL;
	char cmd[CMD_MAX_LEN + 1] = {0};

	if(ap_inf_object.ap_enable_flag == WMG_FALSE) {
		WMG_WARNG("ap mode has been disable\n");
		return WMG_STATUS_UNHANDLED;
	}

	ret = linux_ap_cmd_status();
	if(ret == 1){
		WMG_DEBUG("hapd is already disable, not need to send disable cmd\n");
	} else {
		ret = linux_ap_cmd_disable();
		if(ret){
			WMG_ERROR("failed to disable ap hapd\n");
			return WMG_STATUS_FAIL;
		}
	}

	WMG_INFO("linux hostapd disable now\n");

	while (p != NULL) {
		temp = p;
		p = p->next;
		free(temp);
	}
	ap_inf_object.sta_num = 0;

	WMG_INFO("linux hostapd stop now\n");
	if (ap_inf_object.ap_enable_flag == WMG_TRUE) {
		if (deinit_wpad(WIFI_AP) == WMG_STATUS_SUCCESS) {
			evt_socket_exit(ap_inf_object.ap_event_handle);
		}
	} else {
		WMG_INFO("linux hostapd already has stop\n");
	}

	sprintf(cmd, "ifconfig %s 0.0.0.0", (get_hapd_ap_iface() == NULL) ? "wlan0" : get_hapd_ap_iface());
	wmg_system(cmd);
	wmg_system("rm -rf /var/lib/misc/");
	wmg_system("pidof dnsmasq | xargs kill -9");

	ap_inf_object.ap_enable_flag = WMG_FALSE;

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t linux_ap_get_config(wifi_ap_config_t *ap_config)
{
	WMG_DEBUG("ap get config\n");

	wmg_status_t ret;
	int count = 0;
	char cmd[CMD_MAX_LEN + 1] = {0};
	char reply[EVENT_BUF_SIZE] = {0};

	cmd[CMD_MAX_LEN] = '\0';
	strncpy(cmd, "STATUS", CMD_MAX_LEN);
	ret = linux_command_to_hostapd(cmd, reply, sizeof(reply));
	if (ret) {
		WMG_ERROR("failed to get ap status\n");
		return WMG_STATUS_FAIL;
	}
	if (strstr(reply, "state=DISABLED") != NULL) {
		WMG_DEBUG("hapd is disabled, please enable first\n");
		return WMG_STATUS_FAIL;
	}

	ret = parse_ap_config(reply, ap_config);
	if (ret) {
		WMG_ERROR("failed to parse ap config\n");
		return WMG_STATUS_FAIL;
	}

	if(global_ap_config.ap_config_enable != true) {
		WMG_ERROR("failed to get ap config psk\n");
		return WMG_STATUS_FAIL;
	} else {
		strcpy(ap_config->psk, global_ap_config.ap_config->psk);
		ap_config->sec = global_ap_config.ap_config->sec;
		ap_config->ip_addr[0] = 192;
		ap_config->ip_addr[1] = 168;
		ap_config->ip_addr[2] = 5;
		ap_config->ip_addr[3] = 1;
		ap_config->gw_addr[0] = 192;
		ap_config->gw_addr[1] = 168;
		ap_config->gw_addr[2] = 5;
		ap_config->gw_addr[3] = 1;
	}

	dev_node_t *p = ap_inf_object.dev_list->next;
	while (p != NULL) {
		memcpy(ap_config->dev_list[count], p->bssid, (sizeof(uint8_t) * 6));
		p = p->next;
		count++;
		if (count >= STA_MAX_NUM)
		break;
	}
	WMG_DUMP("number of connected sta is %d\n", count);

	return WMG_STATUS_SUCCESS;
}

static wmg_status_t linux_platform_extension(int cmd, void* cmd_para,int *erro_code)
{
	switch (cmd) {
		case AP_CMD_ENABLE:
			return linux_ap_enable((wifi_ap_config_t *)cmd_para);
		case AP_CMD_DISABLE:
			return linux_ap_disable();
		case AP_CMD_GET_CONFIG:
			return linux_ap_get_config((wifi_ap_config_t *)cmd_para);
		case AP_CMD_SET_SCAN_PARAM:
			return WMG_STATUS_FAIL;
		case AP_CMD_GET_SCAN_RESULTS:
			WMG_INFO("linux ap mode unsupport get scan results\n");
			return WMG_STATUS_FAIL;
		default:
			return WMG_STATUS_FAIL;
	}
}

static wmg_status_t linux_ap_inf_init(ap_event_cb_t ap_event_cb)
{
	if(ap_inf_object.ap_init_flag == WMG_FALSE) {
		WMG_INFO("linux ap init now\n");
		ap_inf_object.ap_event_handle = (event_handle_t *)malloc(sizeof(event_handle_t));
		if(ap_inf_object.ap_event_handle != NULL) {
			memset(ap_inf_object.ap_event_handle, 0, sizeof(event_handle_t));
			ap_inf_object.ap_event_handle->evt_socket[0] = -1;
			ap_inf_object.ap_event_handle->evt_socket[1] = -1;
			ap_inf_object.ap_event_handle->evt_socket_enable = WMG_FALSE;
		} else {
			WMG_ERROR("failed to allocate memory for linux hapd event_handle\n");
			goto event_erro;
		}
		if(ap_event_cb != NULL){
			ap_inf_object.ap_event_cb = ap_event_cb;
		}
		ap_inf_object.ap_init_flag = WMG_TRUE;
	} else {
		WMG_INFO("linux hostapd already init\n");
	}
	return WMG_STATUS_SUCCESS;

event_erro:
	free(ap_inf_object.dev_list);
	return WMG_STATUS_FAIL;
}

static wmg_status_t linux_ap_inf_deinit(void *p)
{
	if(ap_inf_object.ap_init_flag == WMG_TRUE) {
		if(ap_inf_object.ap_event_handle != NULL) {
			free(ap_inf_object.ap_event_handle);
			ap_inf_object.ap_event_cb = NULL;
		}
		ap_inf_object.ap_init_flag = WMG_FALSE;
	} else {
		WMG_INFO("linux hostapd already init\n");
	}
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t linux_ap_inf_enable()
{
	return WMG_STATUS_SUCCESS;
}

static wmg_status_t linux_ap_inf_disable()
{
	linux_ap_disable();
	return WMG_STATUS_SUCCESS;
}

static wmg_ap_inf_object_t ap_inf_object = {
	.ap_init_flag = WMG_FALSE,
	.ap_enable_flag = WMG_FALSE,
	.sta_num = 0,
	.ap_event_cb = NULL,
	.ap_event_handle = NULL,
	.ap_private_data = NULL,

	.ap_inf_init = linux_ap_inf_init,
	.ap_inf_deinit = linux_ap_inf_deinit,
	.ap_inf_enable = linux_ap_inf_enable,
	.ap_inf_disable = linux_ap_inf_disable,
	.ap_platform_extension = linux_platform_extension,
};

wmg_ap_inf_object_t * ap_linux_inf_object_register(void)
{
	return &ap_inf_object;
}
