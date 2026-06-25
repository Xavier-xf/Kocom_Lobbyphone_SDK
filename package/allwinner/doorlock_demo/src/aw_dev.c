#include "aw_dev.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"
#include "aw_msg.h"
#include "doorlock_common.h"
#include "aw_rtmedia.h"

#define MTD_AUTH_OFFSET 0
#define MTD_CONFIG_OFFSET (64 * 1024)
#define MTD_USER_DATA_OFFSET (2 * 64 * 1024)

aw_dev_config_t g_default_dev_config = {
    .valid_flag = AW_VALID_FALG,
    .log_level = DOORLOCK_LOG_LEVEL_INFO,
    .face_ae = 1,
    .roi_ae_target0 = 64,  // ir
    .roi_ae_target1 = 0,   // rgb
    .algo_mode = 1,
    .led_switch = 0,
    .flip_ctl = 0,
    .auto_connect = 1,
    .video_uvc = UVC_SUPPORT,
    .video_osd = OSD_SUPPORT,
    .video_orl = ORL_SUPPORT,
    .video_rotate = UVC_ROTATE,
    .video_index = 1,
    .video_ir_index = 0,
    .video_fmt = 0,
    .video_mode = 0,
    .video_fps = SENSOR_FPS,
    .video_width = SENSOR_WIDTH,
    .video_height = SENSOR_HEIGHT,
    .audio_mic = 1,
    .audio_spk = 1,
    .audio_sample_rate = 8000,
    .audio_bit_width = 16,
    .audio_ch_count = 1,
    .audio_volume = 99,
    .audio_aec = 1,
    .baudrate = 115200,
    .usb_type = 0,
};

users_database_t g_users_database_t = {
    .valid_flag = AW_VALID_FALG, .user_count = 0, .latest_id = 0};

int partition_name_to_mtd(char *partition_name, char *mtd_path, int mtd_size)
{
    char *partitions = getenv("partitions");
    char part[100] = {0};

    sprintf(part, "%s@mtdblock", partition_name);
    char *target_part = strstr(partitions, part);
    if (target_part == NULL) {
        DOORLOCK_ERR("no partition[%s] find\n", partition_name);
        return -1;
    }

    int mtd_num = atoi(&target_part[strlen(part)]);
    if (mtd_num > 0) {
        memset(mtd_path, 0, mtd_size);
        sprintf(mtd_path, "/dev/mtd%d", mtd_num);
    } else {
        DOORLOCK_ERR("parse mtd num failed\n");
        return -1;
    }

    return 0;
}

void *msg_read_thread(void *arg)
{
    DOORLOCK_DBG("enter ===>\n");
    aw_dev_t *aw_dev_info = (aw_dev_t *)arg;
    ring_buffer_t *ring_buffer = &aw_dev_info->ring_buffer;
    uint32_t max_packet_len = UART_RINGBUFFER_SIZE / 2;
    uint8_t *packet_cache_data = malloc(max_packet_len);

    if (packet_cache_data == NULL) {
        DOORLOCK_ERR("malloc %d failed\n", max_packet_len);
        return NULL;
    }

    while (aw_dev_info->thread_exit == false) {
        int actual_len = msg_read_packet(aw_dev_info, packet_cache_data, max_packet_len);
        if (actual_len <= 0) {
            usleep(1 * 1000);
        } else {
            DOORLOCK_DBG("uart actual_len = %d", actual_len);
            while (aw_dev_info->thread_exit == false) {
                if (aw_ring_buffer_write(ring_buffer, packet_cache_data, actual_len) == actual_len) {
                    DOORLOCK_DBG("ring buffer write success\n");
                    break;
                }
                usleep(1000);
            }
        }
    }

    if (packet_cache_data) {
        free(packet_cache_data);
        packet_cache_data = NULL;
    }
    aw_dev_info->thread_exit = true;
    DOORLOCK_DBG("exit <===\n");
}

void *msg_handle_thread(void *arg)
{
    DOORLOCK_DBG("enter ===>\n");
    aw_dev_t *aw_dev_info = (aw_dev_t *)arg;
    aw_packet_header_t packet_header;
    memset(&packet_header, 0, sizeof(aw_packet_header_t));
    uint32_t whole_packet_cnt = 0;
    uint32_t max_packet_len = UART_RINGBUFFER_SIZE;
    uint32_t packet_len = 0;
    uint8_t parity_check = 0;
    uint8_t packet_header_len = 0;
    ring_buffer_t *ring_buffer = &aw_dev_info->ring_buffer;

    uint8_t *packet_cache_data = malloc(max_packet_len);
    if (packet_cache_data == NULL) {
        DOORLOCK_ERR("malloc %d failed\n", max_packet_len);
        return NULL;
    }

    packet_header_len = sizeof(aw_packet_header_t);
    while (aw_dev_info->thread_exit == false) {
        // take a peek for head of packet
        usleep(1 * 1000);
        int head_ok = 0;
        if (aw_ring_buffer_peek(ring_buffer, (uint8_t *)&packet_header, packet_header_len) ==
            packet_header_len) {
#if 0
			uint8_t *pBuf = (uint8_t *)&packet_header;
			for(int i = 0; i < sizeof(aw_packet_header_t); i++){
				printf("0x%02x ", pBuf[i]);
			}
			printf("\n========================\n");
#endif
            // DOORLOCK_INFO("SyncWord:%x %x\n", packet_header.sync_word_heb, packet_header.sync_word_leb);
            if (packet_header.sync_word_heb != SYNC_WORD_H &&
                packet_header.sync_word_leb != SYNC_WORD_L) {
                DOORLOCK_INFO("Not match, Header\n");
                usleep(1 * 1000);
                aw_ring_buffer_read(ring_buffer, (uint8_t *)&packet_header, 1);
            } else {
                // DOORLOCK_INFO("Header is OK \n");
                head_ok = 1;
            }
        }
        if (head_ok != 1) {
            usleep(5000);
            continue;
        }

        uint16_t data_len = (packet_header.data_len_heb << 8) | packet_header.data_len_leb;
        packet_len = packet_header_len + data_len + sizeof(aw_packet_crc_t);
        DOORLOCK_INFO("packet_len is %d\n", packet_len);
        if (packet_len > max_packet_len) {
            DOORLOCK_ERR("packet too large %d, cache buffer max size = %d !!!\n", packet_len,
                        max_packet_len);
        }
        if (aw_ring_buffer_read(ring_buffer, packet_cache_data, packet_len) == packet_len) {
            // read whole packet
            DOORLOCK_INFO("Receive whole packet cnt[%d], Len = %d\n", whole_packet_cnt++,
                         packet_len);
        } else {
            usleep(5000);
            continue;
        }

#if 1
        DOORLOCK_INFO("Receive Date[%d]:", packet_len);
        for (int i = 0; i < packet_len; i++) {
            printf("%02x ", packet_cache_data[i]);
        }
        printf("\n");
#endif

        parity_check = get_parity_checksum(packet_cache_data + PARITY_CHECK_OFFSET,
                                           PARITY_CHECK_LENGTH(packet_len));
        if (parity_check != packet_cache_data[packet_len - 1]) {
            DOORLOCK_ERR("Parity check error\n");
            continue;
        }
        switch (packet_header.msg_id) {
        case MID_VERIFY: {
            DOORLOCK_INFO("Recv MID_VERIFY\n");
            msg_verify_data_t *verify =
                (msg_verify_data_t *)(packet_cache_data + packet_header_len);
            DOORLOCK_INFO("MID_VERIFY timeout:%d\n", verify->timeout);
            break;
        }
        case MID_ENROLL: {
            DOORLOCK_INFO("Recv MID_ENROLL\n");
            msg_enroll_data_t *enroll_data =
                (msg_enroll_data_t *)(packet_cache_data + packet_header_len);
            DOORLOCK_INFO("MID_ENROLL admin:%d, timeout:%d, direction:%d\n", enroll_data->admin,
                   enroll_data->timeout, enroll_data->face_direction);
            aw_dev_info->five_point_enroll = 1;
            aw_dev_info->face_direction = enroll_data->face_direction;
            aw_dev_add_user(aw_dev_info, FEATURE_TYPE_FACE, enroll_data->admin,
                            (char *)enroll_data->user_name, enroll_data->timeout);
            break;
        }
        case MID_PALM_ENROLL_ITG: {
            DOORLOCK_INFO("Recv MID_PALM_ENROLL_ITG\n");
            msg_palm_enroll_data_t *enroll_data =
                (msg_palm_enroll_data_t *)(packet_cache_data + packet_header_len);
            DOORLOCK_INFO("MID_PALM_ENROLL_ITG admin:%d, timeout:%d\n", enroll_data->admin,
                   enroll_data->timeout);
            aw_dev_info->five_point_enroll = 0;
            aw_dev_add_user(aw_dev_info, FEATURE_TYPE_PALM, enroll_data->admin,
                            (char *)enroll_data->user_name, enroll_data->timeout);
            break;
        }
        case MID_POWERDOWN: {
            DOORLOCK_INFO("recv MID_POWERDOWN\n");
            // 构造消息回复(测试使用)
            usleep(100 * 1000);
            msg_send_reply(aw_dev_info, MID_POWERDOWN, MR_SUCCESS);
            break;
        }
        default: {
            DOORLOCK_ERR("unsupport msg id %d\n\n", packet_header.msg_id);
            break;
        }
        }
        usleep(5 * 1000);
    }

exit:
    if (packet_cache_data) {
        free(packet_cache_data);
        packet_cache_data = NULL;
    }
    aw_dev_info->thread_exit = true;
    DOORLOCK_DBG("exit <===\n");
}

int aw_dev_init(aw_dev_t *aw_dev_info)
{
    aw_dev_info->hw_info.version = APP_VERSION;

    if (aw_dev_load_chipid(aw_dev_info, &aw_dev_info->hw_info.chipid[0],
                           sizeof(aw_dev_info->hw_info.chipid)) == 0) {
        aw_dev_info->hw_info.chipid_len = sizeof(aw_dev_info->hw_info.chipid);
    }

    if (aw_dev_info->config_info.algo_mode) {
// 正式的方式应该调用aw_dev_load_auth从分区获取
#if 0
		if(aw_dev_load_auth(aw_dev_info, &aw_dev_info->hw_info.auth_key[0], sizeof(aw_dev_info->hw_info.auth_key)) == 0){
			aw_dev_info->hw_info.mAuthKeyLen = sizeof(aw_dev_info->hw_info.auth_key);
		}
#endif
        int ret = aw_dev_load_user(aw_dev_info, &aw_dev_info->users_database);
        if (ret != 0 || aw_dev_info->users_database.valid_flag != AW_VALID_FALG) {
            DOORLOCK_WARN("set users_database default\n");
            memcpy(&aw_dev_info->users_database, &g_users_database_t, sizeof(users_database_t));
        }
    }
    return 0;
}

int aw_dev_pre_init(aw_dev_t *aw_dev_info)
{
    char file_name[100] = {0};

    if (partition_name_to_mtd("doorlock", file_name, sizeof(file_name) - 1) < 0) {
        DOORLOCK_ERR("partition_name_to_mtd failed, [doorlock]\n");
    }

    while (1) {
        if (access(file_name, F_OK) == 0 && access("/dev/sunxi_soc_info", F_OK) == 0 &&
            access("/dev/ion", F_OK) == 0 && access("/dev/vipcore", F_OK) == 0) {
            break;
        }
        DOORLOCK_WARN("waitting for device ...\n");
        usleep(1 * 1000);
    }
    memset(aw_dev_info, 0, sizeof(aw_dev_t));

    int ret = aw_dev_load_config(aw_dev_info, &aw_dev_info->config_info);
    if (ret != 0 || aw_dev_info->config_info.valid_flag != AW_VALID_FALG ||
        !APP_VERSION_MATCH(aw_dev_info->config_info.version, APP_VERSION)) {
        DOORLOCK_WARN("set config to default\n");
        memcpy(&aw_dev_info->config_info, &g_default_dev_config, sizeof(aw_dev_config_t));
    }
    int baudrate = aw_dev_info->config_info.baudrate;
    if (baudrate == 0) {
        baudrate = 115200;
        DOORLOCK_WARN("UART Set Baudrate default to %d\n", baudrate);
    }

    DOORLOCK_INFO("UART Baudrate %d\n", baudrate);
    aw_dev_info->uart_info.baud_rate = baudrate;
    aw_dev_info->uart_info.stop_bit = 1;
    aw_dev_info->uart_info.bit_width = 8;
    aw_dev_info->uart_info.parity = 'N';
    aw_dev_info->uart_info.block_mode = 0;
    aw_uart_open(&aw_dev_info->uart_info, "/dev/ttyS2");

    //doorlock_set_dynamic_log_level(aw_dev_info->config_info.log_level);
    aw_dev_dump_config(&aw_dev_info->config_info);

    pthread_mutex_init(&aw_dev_info->write_mutex, NULL);
    pthread_mutex_init(&aw_dev_info->read_mutex, NULL);

    pthread_mutex_init(&aw_dev_info->register_mutex, NULL);
    aw_dev_info->register_flag = 0;

    aw_dev_info->thread_exit = false;
    aw_dev_info->enroll_type = FEATURE_TYPE_NULL;
    ring_buffer_t *ring_buffer = &aw_dev_info->ring_buffer;
    aw_ring_buffer_init(ring_buffer, UART_RINGBUFFER_SIZE);

    pthread_create(&aw_dev_info->uart_read_thread, NULL, msg_read_thread, (void *)aw_dev_info);
    pthread_setname_np(aw_dev_info->uart_read_thread, "uart msg read");

    pthread_create(&aw_dev_info->uart_handle_thread, NULL, msg_handle_thread, (void *)aw_dev_info);
    pthread_setname_np(aw_dev_info->uart_handle_thread, "uart msg handle");

    msg_send_note(aw_dev_info, NID_READY, MR_SUCCESS);
    DOORLOCK_INFO("== === DEVICE READY === ==\n");

    return 0;
}

int aw_dev_deinit(aw_dev_t *aw_dev_info)
{
    aw_dev_info->thread_exit = true;
    pthread_join(aw_dev_info->uart_read_thread, NULL);
    pthread_join(aw_dev_info->uart_handle_thread, NULL);
    pthread_mutex_destroy(&aw_dev_info->write_mutex);
    pthread_mutex_destroy(&aw_dev_info->read_mutex);
    pthread_mutex_destroy(&aw_dev_info->register_mutex);
    aw_uart_close(&aw_dev_info->uart_info);
    ring_buffer_t *ring_buffer = &aw_dev_info->ring_buffer;
    aw_ring_buffer_deinit(ring_buffer);

    return 0;
}

int aw_dev_save_config(aw_dev_t *aw_dev_info, aw_dev_config_t *config)
{
    uint32_t data_len = sizeof(aw_dev_config_t);
    char file_name[100] = {0};

    if (partition_name_to_mtd("doorlock", file_name, sizeof(file_name) - 1) < 0) {
        DOORLOCK_ERR("partition_name_to_mtd failed, [doorlock]\n");
        return -1;
    }

    aw_mtd_open(&aw_dev_info->mtd_info, file_name);
    aw_mtd_write_block(&aw_dev_info->mtd_info, MTD_CONFIG_OFFSET, (uint8_t *)config, data_len);
    aw_mtd_close(&aw_dev_info->mtd_info);

    return 0;
}

int aw_dev_load_config(aw_dev_t *aw_dev_info, aw_dev_config_t *config)
{
    uint32_t data_len = sizeof(aw_dev_config_t);
    char file_name[100] = {0};

    if (partition_name_to_mtd("doorlock", file_name, sizeof(file_name) - 1) < 0) {
        DOORLOCK_ERR("partition_name_to_mtd failed, [doorlock]\n");
        return -1;
    }

    aw_mtd_open(&aw_dev_info->mtd_info, file_name);
    unsigned char uid_buf[16];
    aw_mtd_read_uid(&aw_dev_info->mtd_info, uid_buf);

    aw_mtd_read(&aw_dev_info->mtd_info, MTD_CONFIG_OFFSET, (uint8_t *)config, data_len);
    aw_mtd_close(&aw_dev_info->mtd_info);

    config->video_rotate = UVC_ROTATE;  // force set  for uvc video
    return 0;
}

void aw_dev_dump_config(aw_dev_config_t *config)
{
#if 0
	DOORLOCK_INFO("Version 0x%x\n", config->version);
	DOORLOCK_INFO("ValidFlag 0x%x\n", config->valid_flag);
	DOORLOCK_INFO("FaceAe %d\n", config->face_ae);
	DOORLOCK_INFO("LogLevel %d\n", config->log_level);
	DOORLOCK_INFO("RoiAeTarget0 %d\n", config->roi_ae_target0);
	DOORLOCK_INFO("RoiAeTarget1 %d\n", config->roi_ae_target1);
	DOORLOCK_INFO("AlgoMode %d\n", config->algo_mode);
	DOORLOCK_INFO("FlipCtl %d\n", config->flip_ctl);
	DOORLOCK_INFO("AutoConnect %d\n", config->auto_connect);
	DOORLOCK_INFO("VideoUvc %d\n", config->video_uvc);
	DOORLOCK_INFO("VideoOsd %d\n", config->video_osd);
	DOORLOCK_INFO("VideoOrl %d\n", config->video_orl);
	DOORLOCK_INFO("VideoRotate %d\n", config->video_rotate);
	DOORLOCK_INFO("VideoIndex %d\n", config->video_index);
	DOORLOCK_INFO("VideoIrIndex %d\n", config->video_ir_index);
	DOORLOCK_INFO("VideoFmt %d\n", config->video_fmt);
	DOORLOCK_INFO("VideoFps %d\n", config->video_fmt);
	DOORLOCK_INFO("VideoWidth %d\n", config->video_width);
	DOORLOCK_INFO("VideoHeight %d\n", config->video_height);
	DOORLOCK_INFO("AudioMic %d\n", config->audio_mic);
	DOORLOCK_INFO("AudioSpk %d\n", config->audio_spk);
	DOORLOCK_INFO("AudioSampleRate %d\n", config->audio_sample_rate);
	DOORLOCK_INFO("AudioBitWidth %d\n", config->audio_bit_width);
	DOORLOCK_INFO("AudioChCount %d\n", config->audio_ch_count);
	DOORLOCK_INFO("AudioVolume %d\n", config->audio_volume);
	DOORLOCK_INFO("UartBaudrate %d\n", config->baudrate);
	DOORLOCK_INFO("UsbType %d\n", config->usb_type);
#endif
}

int aw_dev_save_auth(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len)
{
    char file_name[100] = {0};

    if (partition_name_to_mtd("doorlock", file_name, sizeof(file_name) - 1) < 0) {
        DOORLOCK_ERR("partition_name_to_mtd failed, [doorlock]\n");
        return -1;
    }

    aw_mtd_open(&aw_dev_info->mtd_info, file_name);
    aw_mtd_write_block(&aw_dev_info->mtd_info, MTD_AUTH_OFFSET, (uint8_t *)data, len);
    aw_mtd_close(&aw_dev_info->mtd_info);

    return 0;
}

int aw_dev_load_auth(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len)
{
    char file_name[100] = {0};

    if (partition_name_to_mtd("doorlock", file_name, sizeof(file_name) - 1) < 0) {
        DOORLOCK_ERR("Partition Name To Mtd failed, [doorlock]\n");
        return -1;
    }

    aw_mtd_open(&aw_dev_info->mtd_info, file_name);
    aw_mtd_read(&aw_dev_info->mtd_info, MTD_AUTH_OFFSET, (uint8_t *)data, len);
    aw_mtd_close(&aw_dev_info->mtd_info);

    return 0;
}

int aw_dev_database_adduser(users_database_t *db, unsigned int user_id, uint8_t feature_type,
                            void *feature_data)
{
    if (db->user_count >= MAX_USERS) {
        return -1;
    }

    user_feature_info_t *new_user = &db->users[db->user_count++];
    new_user->user_id = user_id;
    new_user->feature_type = feature_type;

    if (feature_type == FEATURE_TYPE_FACE) {
        new_user->face_data = *(pix_fr_feature_info_t *)feature_data;
    } else if (feature_type == FEATURE_TYPE_PALM) {
        new_user->palm_data = *(pix_pr_feature_info_t *)feature_data;
    }
    return 0;
}

int aw_dev_database_deluser(users_database_t *db, unsigned int user_id)
{
    for (unsigned int i = 0; i < db->user_count; ++i) {
        if (db->users[i].user_id == user_id) {
            for (unsigned int j = i; j < db->user_count - 1; ++j) {
                db->users[j] = db->users[j + 1];
            }
            db->user_count--;
            return 0;
        }
    }

    return -1;
}

int aw_dev_save_user(aw_dev_t *aw_dev_info, users_database_t *db)
{
    char file_name[100] = {0};

    if (partition_name_to_mtd("doorlock", file_name, sizeof(file_name) - 1) < 0) {
        DOORLOCK_ERR("partition_name_to_mtd failed, [doorlock]\n");
        return -1;
    }

    db->valid_flag = AW_VALID_FALG;
    aw_mtd_open(&aw_dev_info->mtd_info, file_name);
    aw_mtd_write_block(&aw_dev_info->mtd_info, MTD_USER_DATA_OFFSET, (uint8_t *)db,
                  sizeof(users_database_t));
    aw_mtd_close(&aw_dev_info->mtd_info);

    return 0;
}

int aw_dev_load_user(aw_dev_t *aw_dev_info, users_database_t *db)
{
    char file_name[100] = {0};

    if (partition_name_to_mtd("doorlock", file_name, sizeof(file_name) - 1) < 0) {
        DOORLOCK_ERR("partition_name_to_mtd failed, [doorlock]\n");
        return -1;
    }

    DOORLOCK_INFO("sizeof(users_database_t) %d\n", sizeof(users_database_t));
    aw_mtd_open(&aw_dev_info->mtd_info, file_name);
    aw_mtd_read(&aw_dev_info->mtd_info, MTD_USER_DATA_OFFSET, (uint8_t *)db, sizeof(users_database_t));
    aw_mtd_close(&aw_dev_info->mtd_info);

    DOORLOCK_INFO("User count = %d\n", db->user_count);
    return 0;
}

int aw_dev_add_user(aw_dev_t *aw_dev_info, uint8_t type, uint8_t admin, char *name, uint8_t timeout)
{
    memset(&aw_dev_info->register_user_info, 0, sizeof(aw_dev_info->register_user_info));
    if (timeout < 5) {
        aw_dev_info->register_timeout = 10;
    } else {
        aw_dev_info->register_timeout = timeout;  // second
    }

    aw_dev_info->register_user_info.admin = admin;
    aw_dev_info->enroll_type = type;
    memcpy(aw_dev_info->register_user_info.name, name, strlen(name));

    pthread_mutex_lock(&aw_dev_info->register_mutex);
    aw_dev_info->register_flag = 1;
    pthread_mutex_unlock(&aw_dev_info->register_mutex);
    DOORLOCK_INFO(" \n");

    return 0;
}

int aw_dev_del_user(aw_dev_t *aw_dev_info, uint8_t id)
{
    users_database_t *database = &aw_dev_info->users_database;
    aw_dev_database_deluser(database, id);
    aw_dev_save_user(aw_dev_info, database);
    system("sync");

    return 0;
}

#define CHECK_SOC_CHIPID 0x04
#define CHECK_SOC_CHIPID_FULL 0x07
int aw_dev_load_chipid(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len)
{
    int sid_fd;
    uint8_t buf[32 + 1] = {0};
    sid_fd = open("/dev/sunxi_soc_info", O_RDONLY);

    if (sid_fd < 0) {
        perror("open sunxi_soc_info failed");
        DOORLOCK_ERR("open /dev/sunxi_soc_info error!\n");
        return -1;
    }

    int ret = ioctl(sid_fd, CHECK_SOC_CHIPID_FULL, buf);
    if (ret) {
        DOORLOCK_ERR("read /dev/sunxi_soc_info error!\n");
        close(sid_fd);
        return -1;
    }

    if (len != 16) {
        DOORLOCK_ERR("data len not right!\n");
    } else {
        for (int i = 0; i < 16; i++) {
            char tmp_str[5] = {'0', 'x', '0', '0', '\0'};
            tmp_str[2] = buf[i * 2];
            tmp_str[3] = buf[i * 2 + 1];
            data[i] = strtol(tmp_str, NULL, 16);
        }
    }
    close(sid_fd);

    return 0;
}

int aw_dev_reboot(aw_dev_t *aw_dev_info)
{
    system("sync");
    system("reboot\n");

    return 0;
}

int aw_dev_set_led(aw_dev_t *aw_dev_info, char led_switch)
{
    return door_lock_set_led(led_switch);
}

int aw_dev_set_videomode(aw_dev_t *aw_dev_info, int video_index, char video_mode)
{
    return door_lock_set_video_mode(video_index, video_mode);
}

int aw_dev_set_flipctl(aw_dev_t *aw_dev_info, char flip_ctl)
{
    return door_lock_set_video_flip(flip_ctl);
}
