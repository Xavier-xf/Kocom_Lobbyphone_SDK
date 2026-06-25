/*
 * lv_box_res_bt.c
 *
 *  Created on: 2022Äê10ÔÂ11ÈÕ
 *      Author: anruliu
 */

#include "lv_box_res_bt.h"

#ifdef LV_USE_LINUX_BT4_MODE

#include <bt_log.h>
#include <bt_manager.h>
#include <bt_dev_list.h>
#include <stdio.h>

static int song_playing_pos;
static int song_playing_len;
static int is_background = 1;

extern btmg_profile_info_t *bt_pro_info;
static dev_list_t *discovered_devices;
static btmg_callback_t *bt_callback;

static const char* _hfp_event_to_string(btmg_hfp_even_t event) {
    switch (event) {
    case BTMG_HFP_CONNECT:
        return "HFP_CONNECT";
    case BTMG_HFP_CONNECT_LOST:
        return "HFP_CONNECT_LOST";

    case BTMG_HFP_CIND:
        return "HFP_CIND";
    case BTMG_HFP_CIEV:
        return "HFP_CIEV";
    case BTMG_HFP_RING:
        return "HFP_RING";
    case BTMG_HFP_CLIP:
        return "HFP_CLIP";
    case BTMG_HFP_BSIR:
        return "HFP_BSIR";
    case BTMG_HFP_BVRA:
        return "HFP_BVRA";
    case BTMG_HFP_CCWA:
        return "HFP_CCWA";
    case BTMG_HFP_CHLD:
        return "HFP_CHLD";
    case BTMG_HFP_VGM:
        return "HFP_VGM";
    case BTMG_HFP_VGS:
        return "HFP_VGS";
    case BTMG_HFP_BINP:
        return "HFP_BINP";
    case BTMG_HFP_BTRH:
        return "HFP_BTRH";
    case BTMG_HFP_CNUM:
        return "HFP_CNUM";
    case BTMG_HFP_COPS:
        return "HFP_COPS";
    case BTMG_HFP_CMEE:
        return "HFP_CMEE";
    case BTMG_HFP_CLCC:
        return "HFP_CLCC";
    case BTMG_HFP_UNAT:
        return "HFP_UNAT";
    case BTMG_HFP_OK:
        return "HFP_OK";
    case BTMG_HFP_ERROR:
        return "HFP_ERROR";
    case BTMG_HFP_BCS:
        return "HFP_BCS";
    }

    return NULL;
}

static void bt_test_adapter_status_cb(btmg_adapter_state_t status) {
    char bt_addr[18] = { 0 };
    char bt_name_buf[64] = { 0 };
    char bt_name[64] = { 0 };

    if (status == BTMG_ADAPTER_OFF) {
        BTMG_INFO("BT is off");
    } else if (status == BTMG_ADAPTER_ON) {
        BTMG_INFO("BT is ON");
        bt_manager_get_adapter_address(bt_addr);
        bt_manager_set_adapter_name("LV_86_BOXES_MUSIC");
        if (is_background)
            bt_manager_agent_set_io_capability(BTMG_IO_CAP_NOINPUTNOOUTPUT);
        else
            bt_manager_agent_set_io_capability(BTMG_IO_CAP_KEYBOARDDISPLAY);
        bt_manager_set_scan_mode(BTMG_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
    } else if (status == BTMG_ADAPTER_TURNING_ON) {
        BTMG_INFO("bt is turnning on.");
    } else if (status == BTMG_ADAPTER_TURNING_OFF) {
        BTMG_INFO("bt is turnning off.");
    }
}

static void bt_test_scan_status_cb(btmg_scan_state_t status) {
    if (status == BTMG_SCAN_STARTED) {
        BTMG_INFO("start scanning.");
    } else if (status == BTMG_SCAN_STOPPED) {
        BTMG_INFO("scanning stop.");
    }
}

static void bt_test_dev_add_cb(btmg_bt_device_t *device) {
    dev_node_t *dev_node = NULL;

    BTMG_INFO("address:%s, name:%s, class:%d, icon:%s, rssi:%d",
            device->remote_address, device->remote_name, device->r_class,
            device->icon, device->rssi);

    dev_node = btmg_dev_list_find_device(discovered_devices,
            device->remote_address);
    if (dev_node != NULL) {
        return;
    }
    btmg_dev_list_add_device(discovered_devices, device->remote_name,
            device->remote_address, 0);
}

static void bt_test_dev_remove_cb(btmg_bt_device_t *device) {
    dev_node_t *dev_node = NULL;

    BTMG_INFO("address:%s,name:%s", device->remote_address,
            device->remote_name);

    btmg_dev_list_remove_device(discovered_devices, device->remote_address);
}

static void bt_test_update_rssi_cb(const char *address, int rssi) {
    dev_node_t *dev_node = NULL;

    dev_node = btmg_dev_list_find_device(discovered_devices, address);
    if (dev_node != NULL) {
        BTMG_INFO("address:%s,name:%s,rssi:%d", dev_node->dev_addr,
                dev_node->dev_name, rssi);
        return;
    }
}

static void bt_test_bond_state_cb(btmg_bond_state_t state, const char *bd_addr) {
    dev_node_t *dev_discovered_node = NULL;

    BTMG_DEBUG("bonded device state:%d, addr:%s", state, bd_addr);

    dev_discovered_node = btmg_dev_list_find_device(discovered_devices,
            bd_addr);
    if (state == BTMG_BOND_STATE_BONDED) {
        if (dev_discovered_node != NULL) {
            btmg_dev_list_remove_device(discovered_devices, bd_addr);
        }
        BTMG_INFO("Pairing state for %s is BONDED", bd_addr);
    } else if (state == BTMG_BOND_STATE_BONDING) {
        BTMG_INFO("Pairing state for %s is BONDEDING", bd_addr);
    }
}

#define BUFFER_SIZE 17
static void bt_test_pair_ask(const char *prompt, char *buffer) {
    printf("%s", prompt);
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL)
        fprintf(stdout, "\ncmd fgets error\n");
}

void bt_test_agent_request_pincode_cb(void *handle, char *device) {
    char buffer[BUFFER_SIZE] = { 0 };

    fprintf(stdout, "AGENT:(%s)Request pincode\n", device);
    bt_test_pair_ask("Enter PIN Code: ", buffer);
    bt_manager_agent_send_pincode(handle, buffer);
}

void bt_test_agent_display_pincode_cb(char *device, char *pincode) {
    BTMG_INFO("AGENT: display pincode:%s", pincode);
}

void bt_test_agent_request_passkey_cb(void *handle, char *device) {
    unsigned int passkey;

    passkey = (unsigned int) rand() % 1000000;
    BTMG_INFO(
            "AGENT: request passkey:%d, \
            please enter it on the device %s",
            passkey, device);
    bt_manager_agent_send_passkey(handle, passkey);
}

void bt_test_agent_display_passkey_cb(char *device, unsigned int passkey,
        unsigned int entered) {
    BTMG_INFO("AGENT: display passkey:%06u, device:%s, enter:%u", passkey,
            device, entered);
}

void bt_test_agent_request_confirm_passkey_cb(void *handle, char *device,
        unsigned int passkey) {
    BTMG_INFO("AGENT: %s request confirmation,passkey:%06u", device, passkey);
    bt_manager_agent_pair_send_empty_response(handle);
}

void bt_test_agent_request_authorize_cb(void *handle, char *device) {
    BTMG_INFO("AGENT: %s request authorization", device);
    bt_manager_agent_pair_send_empty_response(handle);
}

void bt_test_agent_authorize_service_cb(void *handle, char *device, char *uuid) {
    BTMG_INFO("AGENT: %s Authorize Service %s", device, uuid);
    bt_manager_agent_pair_send_empty_response(handle);
}

static void bt_test_a2dp_sink_connection_state_cb(const char *bd_addr,
        btmg_a2dp_sink_connection_state_t state) {
    if (state == BTMG_A2DP_SINK_DISCONNECTED) {
        BTMG_INFO("A2DP sink disconnected with device: %s", bd_addr);
        bt_manager_set_scan_mode(BTMG_SCAN_MODE_CONNECTABLE_DISCOVERABLE);
    } else if (state == BTMG_A2DP_SINK_CONNECTING) {
        BTMG_INFO("A2DP sink connecting with device: %s", bd_addr);
    } else if (state == BTMG_A2DP_SINK_CONNECTED) {
        BTMG_INFO("A2DP sink connected with device: %s", bd_addr);
    } else if (state == BTMG_A2DP_SINK_DISCONNECTING) {
        BTMG_INFO("A2DP sink disconnecting with device: %s", bd_addr);
    }
}

static void bt_test_a2dp_sink_audio_state_cb(const char *bd_addr,
        btmg_a2dp_sink_audio_state_t state) {
    if (state == BTMG_A2DP_SINK_AUDIO_SUSPENDED) {
        BTMG_INFO("A2DP sink audio suspended with device: %s", bd_addr);
    } else if (state == BTMG_A2DP_SINK_AUDIO_STOPPED) {
        BTMG_INFO("A2DP sink audio stopped with device: %s", bd_addr);
    } else if (state == BTMG_A2DP_SINK_AUDIO_STARTED) {
        BTMG_INFO("A2DP sink audio started with device: %s", bd_addr);
    }
}

static void bt_test_avrcp_play_state_cb(const char *bd_addr,
        btmg_avrcp_play_state_t state) {
    if (state == BTMG_AVRCP_PLAYSTATE_STOPPED) {
        BTMG_INFO("BT playing music stopped with device: %s", bd_addr);
    } else if (state == BTMG_AVRCP_PLAYSTATE_PLAYING) {
        BTMG_INFO("BT palying music playing with device: %s", bd_addr);
    } else if (state == BTMG_AVRCP_PLAYSTATE_PAUSED) {
        BTMG_INFO("BT palying music paused with device: %s", bd_addr);
    } else if (state == BTMG_AVRCP_PLAYSTATE_FWD_SEEK) {
        BTMG_INFO("BT palying music FWD SEEK with device: %s", bd_addr);
    } else if (state == BTMG_AVRCP_PLAYSTATE_REV_SEEK) {
        BTMG_INFO("BT palying music REV SEEK with device: %s", bd_addr);
    } else if (state == BTMG_AVRCP_PLAYSTATE_FORWARD) {
        BTMG_INFO("BT palying music forward with device: %s", bd_addr);
    } else if (state == BTMG_AVRCP_PLAYSTATE_BACKWARD) {
        BTMG_INFO("BT palying music backward with device: %s", bd_addr);
    } else if (state == BTMG_AVRCP_PLAYSTATE_ERROR) {
        BTMG_INFO("BT palying music ERROR with device: %s", bd_addr);
    }
}
static void bt_test_avrcp_track_changed_cb(const char *bd_addr,
        btmg_track_info_t track_info) {
    BTMG_DEBUG("BT playing device: %s", bd_addr);
    BTMG_INFO("BT playing music title: %s", track_info.title);
    BTMG_INFO("BT playing music artist: %s", track_info.artist);
    BTMG_INFO("BT playing music album: %s", track_info.album);
    BTMG_DEBUG("BT playing music track number: %s", track_info.track_num);
    BTMG_DEBUG("BT playing music total number of tracks: %s",
            track_info.num_tracks);
    BTMG_DEBUG("BT playing music genre: %s", track_info.genre);
    BTMG_DEBUG("BT playing music duration: %s", track_info.duration);
}

static void bt_test_avrcp_audio_volume_cb(const char *bd_addr,
        unsigned int volume) {
    BTMG_INFO("AVRCP audio volume:%s : %d", bd_addr, volume);
}

static void bt_test_avrcp_play_position_cb(const char *bd_addr, int song_len,
        int song_pos) {
    if (song_playing_pos > song_pos && song_playing_len != song_len) {
        BTMG_INFO("AVRCP playing song switched");
    }
    song_playing_len = song_len;
    song_playing_pos = song_pos;
    BTMG_DEBUG("%s,playing song len:%d,position:%d", bd_addr, song_len,
            song_pos);
}

static void bt_test_hfp_hf_connection_state_cb(const char *bd_addr,
        btmg_hfp_hf_connection_state_t state) {
    if (state == BTMG_HFP_HF_DISCONNECTED) {
        BTMG_INFO("hfp hf disconnected with device: %s", bd_addr);
    } else if (state == BTMG_HFP_HF_CONNECTING) {
        BTMG_INFO("hfp hf connecting with device: %s", bd_addr);
    } else if (state == BTMG_HFP_HF_CONNECTED) {
        BTMG_INFO("hfp hf connected with device: %s", bd_addr);
    } else if (state == BTMG_HFP_HF_DISCONNECTING) {
        BTMG_INFO("hfp hf disconnecting with device: %s", bd_addr);
    } else if (state == BTMG_HFP_HF_CONNECT_FAILED) {
        BTMG_INFO("hfp hf connect with device: %s failed!", bd_addr);
    } else if (state == BTMG_HFP_HF_DISCONNEC_FAILED) {
        BTMG_INFO("hfp hf disconnect with device: %s failed!", bd_addr);
    }
}

static void bt_test_hfp_event_cb(btmg_hfp_even_t event, char *data) {
    BTMG_INFO("event:%s, value:%s", _hfp_event_to_string(event), data);
}

static int _bt_init(int profile) {
    btmg_set_log_file_path("/tmp/btmg.log");
    if (bt_manager_preinit(&bt_callback) != 0) {
        printf("bt preinit failed!\n");
        return -1;
    }

    if (profile == 0) {
        bt_manager_set_default_profile(true);
    } else {
        bt_manager_enable_profile(profile);
    }
    bt_callback->btmg_adapter_cb.adapter_state_cb = bt_test_adapter_status_cb;

    bt_callback->btmg_gap_cb.gap_scan_status_cb = bt_test_scan_status_cb;
    bt_callback->btmg_gap_cb.gap_device_add_cb = bt_test_dev_add_cb;
    bt_callback->btmg_gap_cb.gap_device_remove_cb = bt_test_dev_remove_cb;
    bt_callback->btmg_gap_cb.gap_update_rssi_cb = bt_test_update_rssi_cb;
    bt_callback->btmg_gap_cb.gap_bond_state_cb = bt_test_bond_state_cb;
    /* bt security callback setting.*/
    bt_callback->btmg_agent_cb.agent_request_pincode =
            bt_test_agent_request_pincode_cb;
    bt_callback->btmg_agent_cb.agent_display_pincode =
            bt_test_agent_display_pincode_cb;
    bt_callback->btmg_agent_cb.agent_request_passkey =
            bt_test_agent_request_passkey_cb;
    bt_callback->btmg_agent_cb.agent_display_passkey =
            bt_test_agent_display_passkey_cb;
    bt_callback->btmg_agent_cb.agent_request_confirm_passkey =
            bt_test_agent_request_confirm_passkey_cb;
    bt_callback->btmg_agent_cb.agent_request_authorize =
            bt_test_agent_request_authorize_cb;
    bt_callback->btmg_agent_cb.agent_authorize_service =
            bt_test_agent_authorize_service_cb;
    /* bt a2dp sink callback*/
    if (bt_pro_info->is_a2dp_sink_enabled) {
        bt_callback->btmg_a2dp_sink_cb.a2dp_sink_connection_state_cb =
                bt_test_a2dp_sink_connection_state_cb;
        bt_callback->btmg_a2dp_sink_cb.a2dp_sink_audio_state_cb =
                bt_test_a2dp_sink_audio_state_cb;
    }

    /* bt avrcp callback*/
    if (bt_pro_info->is_a2dp_sink_enabled
            || bt_pro_info->is_a2dp_source_enabled) {
        bt_callback->btmg_avrcp_cb.avrcp_play_state_cb =
                bt_test_avrcp_play_state_cb;
        bt_callback->btmg_avrcp_cb.avrcp_play_position_cb =
                bt_test_avrcp_play_position_cb;
        bt_callback->btmg_avrcp_cb.avrcp_track_changed_cb =
                bt_test_avrcp_track_changed_cb;
        bt_callback->btmg_avrcp_cb.avrcp_audio_volume_cb =
                bt_test_avrcp_audio_volume_cb;
    }
    /* bt hfp callback*/
    if (bt_pro_info->is_hfp_hf_enabled) {
        bt_callback->btmg_hfp_hf_cb.hfp_hf_connection_state_cb =
                bt_test_hfp_hf_connection_state_cb;
        bt_callback->btmg_hfp_hf_cb.hfp_hf_event_cb = bt_test_hfp_event_cb;
    }

    if (bt_manager_init(bt_callback) != 0) {
        printf("bt manager init failed.\n");
        return -1;
    }

    discovered_devices = btmg_dev_list_new();
    if (discovered_devices == NULL)
        goto Failed;

    return 0;
    Failed: bt_manager_deinit(bt_callback);
    return -1;
}

int lv_box_res_bt_init(void) {
    return _bt_init(0);
}

int lv_box_res_bt_deinit(void) {
    return bt_manager_deinit(bt_callback);
}

int lv_box_res_bt_on(void) {
    return bt_manager_enable(true);
}

int lv_box_res_bt_off(void) {
    return bt_manager_enable(false);
}
#else
int lv_box_res_bt_init(void) {
    return 0;
}

int lv_box_res_bt_deinit(void) {
    return 0;
}

int lv_box_res_bt_on(void) {
    return 0;
}

int lv_box_res_bt_off(void) {
    return 0;
}
#endif
