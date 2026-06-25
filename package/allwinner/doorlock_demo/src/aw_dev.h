#ifndef __AW_DEV_H__
#define __AW_DEV_H__
#include <semaphore.h>

#include "doorlock_common.h"
#include "aw_alg_pix_kit.h"
#include "aw_mtd.h"
#include "aw_ringbuffer.h"
#include "aw_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AW_CHIPID_LEN 16
#define MAX_USERS 20  // 数据库最多支持20个用户
#define AW_VALID_FALG 0xCC99
#define UART_RINGBUFFER_SIZE 4 * 1024 * 4

typedef struct aw_dev_config_s {
    unsigned int version;       ///< APP_VERSION
    unsigned short valid_flag;  ///< Aw_VALID_FALG
    unsigned char log_level;
    unsigned char face_ae;
    unsigned char roi_ae_target0;
    unsigned char roi_ae_target1;
    unsigned char algo_mode;       ///< 0:off, 1:on
    unsigned char led_switch;      ///< 0:off, 1:on
    unsigned char flip_ctl;        ///< ///< 0:none, 1:hflip, 2:vflip, 3:h&v flip
    unsigned char auto_connect;    ///< 0:No Auto Connect, 1:Bootup Auto Connect
    unsigned char video_uvc;       ///< 0:Disable, 1:Enable
    unsigned char video_osd;       ///< 0:Disable, 1:Enable
    unsigned char video_orl;       ///< 0:Disable, 1:Enable
    unsigned char video_rotate;    ///< 0:0 degree, 1:90 degree, 2:180 degree, 3:270 degree
    unsigned char video_index;     ///< 0:Left, 1:Right
    unsigned char video_ir_index;  ///< 0:Left, 1:Right, 2:Both, 3:None, 4:Default
    unsigned char video_fmt;       ///< bit0:MJPEG, bit1:YUYV, bit2:H264
    unsigned char video_mode;      ///< 0:RGB, 1:IR
    unsigned short video_fps;
    unsigned short video_width;
    unsigned short video_height;
    unsigned char audio_mic;           ///< 0:Disable, 1:Enable
    unsigned char audio_spk;           ///< 0:Disable, 1:Enable
    unsigned short audio_sample_rate;  ///< Sample Rate
    unsigned char audio_bit_width;     ///< Bit Width
    unsigned char audio_ch_count;      ///< Channel Count
    unsigned short audio_volume;       ///< Volume
    unsigned char audio_aec;           ///< AudioAec
    unsigned int baudrate;             ///< Baudrate
    unsigned int usb_type;  ///< 0:iso full speed, 1: iso high speed, 2: bulk full speed, 3: bulk high speed
} __attribute__((packed)) aw_dev_config_t;

typedef struct aw_dev_hw_s {
    unsigned int version;
    unsigned char auth_ok;
    unsigned short chipid_len;
    unsigned char chipid[AW_CHIPID_LEN];
    unsigned short algoid_len;
    unsigned char algoid[PIX_HARDWARE_INFO_BYTES];
    unsigned short authkey_len;
    unsigned char authkey[PIX_AUTH_KEY_BYTES];
} __attribute__((packed)) aw_dev_hw_t;

// 每个用户的特征信息，包括用户ID和特征类型
typedef struct user_feature_info_s {
    uint8_t user_id;  // 用户ID
    uint8_t admin;
    char name[32];
    uint8_t feature_type;  // 特征类型（人脸或手掌）
    union {
        pix_fr_feature_info_t face_data;  // 人脸特征数据
        pix_pr_feature_info_t palm_data;  // 手掌特征数据
    };
} user_feature_info_t;

// 数据库结构体，管理多个用户信息
typedef struct users_database_s {
    unsigned short valid_flag;             // 用来校验分区中是否有数据
    unsigned int latest_id;                // ID 逐一递增
    unsigned int user_count;               // 表示有多少个用户
    user_feature_info_t users[MAX_USERS];  // 存储多个用户的特征数据
} users_database_t;

typedef struct aw_dev_s {
    uart_info_t uart_info;
    ring_buffer_t ring_buffer;
    aw_mtd_t mtd_info;
    int thread_exit;
    pthread_t uart_read_thread;
    pthread_t uart_handle_thread;
    pthread_mutex_t write_mutex;
    pthread_mutex_t read_mutex;
    aw_dev_hw_t hw_info;
    aw_dev_config_t config_info;
    users_database_t users_database;
    uint8_t enroll_type;
    uint8_t register_timeout;  // second
    uint8_t five_point_enroll;
    uint8_t face_direction;
    pthread_mutex_t register_mutex;
    int register_flag;
    user_feature_info_t register_user_info;
} aw_dev_t;

void aw_dev_dump_config(aw_dev_config_t *config_data);
int partition_name_to_mtd(char *partition_name, char *mtd_path, int mtd_size);
int aw_dev_init(aw_dev_t *aw_dev_info);
int aw_dev_pre_init(aw_dev_t *aw_dev_info);
int aw_dev_deinit(aw_dev_t *aw_dev_info);
int aw_dev_save_config(aw_dev_t *aw_dev_info, aw_dev_config_t *config);
int aw_dev_load_config(aw_dev_t *aw_dev_info, aw_dev_config_t *config);
int aw_dev_save_auth(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len);
int aw_dev_load_auth(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len);
int aw_dev_database_adduser(users_database_t *db, unsigned int user_id, uint8_t feature_type,
                            void *feature_data);
int aw_dev_database_deluser(users_database_t *db, unsigned int user_id);
int aw_dev_save_user(aw_dev_t *aw_dev_info, users_database_t *db);
int aw_dev_load_user(aw_dev_t *aw_dev_info, users_database_t *db);
int aw_dev_add_user(aw_dev_t *aw_dev_info, uint8_t type, uint8_t admin, char *name,
                    uint8_t timeout);
int aw_dev_del_user(aw_dev_t *aw_dev_info, uint8_t id);
int aw_dev_load_chipid(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len);
int aw_dev_reboot(aw_dev_t *aw_dev_info);
int aw_dev_set_led(aw_dev_t *aw_dev_info, char led_switch);
int aw_dev_set_videomode(aw_dev_t *aw_dev_info, int video_index, char video_mode);
int aw_dev_set_flipctl(aw_dev_t *aw_dev_info, char flip_ctl);
#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif /* __AW_DEV_H__ */
