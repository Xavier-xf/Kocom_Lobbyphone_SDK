#include "main.h"

#include <asm/types.h>
#include <bits/alltypes.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "aw_dev.h"
#include "aw_msg.h"
#include "aw_g2d.h"
#include "aw_osd.h"
#include "aw_rtmedia.h"
#include "aw_alg.h"
#include "aw_alg_pix_kit.h"
#if APP_MPPAIO_SUPPORT
#define MPPAIO_SUPPORT		(1 | UAC_SUPPORT)
#define SNDCARD_SUPPORT 	0
#include "aw_mpp.h"
#include "aw_mpp_ai.h"
#include "aw_mpp_ao.h"
#else
#define MPPAIO_SUPPORT		0
#define SNDCARD_SUPPORT 	(1 | UAC_SUPPORT)
#include "aw_sndcard.h"
#endif

#if UVC_SUPPORT
#include "aw_uvcout.h"
#endif

#if UAC_SUPPORT
#include "aw_uacdev.h"
#endif

#if (UVC_SUPPORT | UAC_SUPPORT)
#include "aw_composite.h"
#endif

typedef struct algo_info_s {
    int algo_mode;
    int algo_state;
    int rotate;
    aw_alg_t aw_alg;
    alg_pix_kit_t alg_pix_info;
} algo_info_t;

typedef struct doorlock_context_s {
    int uvc_vi_dev;
    int uvc_dual_transfer;
    double cpu_usage;
    double mem_usage;
    thermal_zone_t thermal_zone[THERMAL_ZONE_NUM];

    algo_info_t algo_info;
    aw_mem_info_t algo_mem_info[2];
    pthread_t algo_detect_thread;
    pthread_t algo_image_threads[2];
    frm_manager_t algo_frms_info[2];
    rt_media_chn_t rt_media_algo_chns[2];  // for ir and rgb

    pthread_t uvc_yuv_thread;
    rt_media_chn_t rt_media_chns[2];  // for uvc

#if OSD_SUPPORT
    rt_media_osd_t rt_media_osds[2];
    rt_media_osd_chn_t rt_media_osd_chns[2];
#endif
#if ORL_SUPPORT
    rt_media_orl_t rt_media_orl_infos[2];
#endif
    aw_mem_info_t rotate_mem_info;
    uint32_t input_video_v4l2_format;  // FOR SENSOR INPUT

#if UVC_SUPPORT
    uvc_out_t uvc_out_info;
#endif

#if UAC_SUPPORT
#if SNDCARD_SUPPORT
	pthread_t snd_card_mic_thread;
	pthread_t snd_card_spk_thread;
	snd_card_t snd_card_info;
#endif
#if MPPAIO_SUPPORT
    mpp_ai_s mpp_ai_info;
    mpp_ai_chn_s mpp_ai_chn_info;
    mpp_ao_t mpp_ao_info;
    mpp_ao_chn_t mpp_ao_chn_info;
#endif
    uac_dev_t uac_dev_info;
#endif

#if (UVC_SUPPORT | UAC_SUPPORT)
    composite_dev_t composite_info;
#endif
    aw_dev_t dev_info;
} doorlock_context_t;

static bool g_exit_flag = false;
static char *g_store_dir = NULL;
static int g_ai_chn = 0;
static int g_audio_ch_cnt = 1;
static int g_audio_sample_rate = 8000;
static int g_audio_bit_width = 16;
static int g_audio_volume = 99;
static int g_audio_period_size = 1024;
static doorlock_context_t *g_context_info = NULL;
pix_fr_feature_info_t g_last_feature;
int g_five_face_register_count = 0;
int g_five_face_ready_direction = 0;

#if UVC_SUPPORT
#if (UVC_ROTATE == 1 || UVC_ROTATE == 3)
static uvc_frame_info_t app_mjpeg_frame_info[] = {
    {1, UVC_SENSOR_WIDTH, UVC_SENSOR_HEIGHT, SENSOR_FPS, UVC_ROTATE},

};
static uvc_frame_info_t app_yuy2_frame_info[] = {
    {1, SENSOR_HEIGHT, SENSOR_WIDTH, SENSOR_FPS, UVC_ROTATE},
    {2, SENSOR_HEIGHT * 2, SENSOR_WIDTH, SENSOR_FPS, UVC_ROTATE},
};
static uvc_frame_info_t app_h264_frame_info[] = {
    {1, SENSOR_HEIGHT, SENSOR_WIDTH, SENSOR_FPS, UVC_ROTATE},
};
static uvc_frame_info_t app_nv12_frame_info[] = {
    {1, SENSOR_HEIGHT, SENSOR_WIDTH, SENSOR_FPS, UVC_ROTATE},
    {2, SENSOR_HEIGHT * 2, SENSOR_WIDTH, SENSOR_FPS, UVC_ROTATE},
};
#else
static uvc_frame_info_t app_mjpeg_frame_info[] = {
    {1, UVC_SENSOR_WIDTH, UVC_SENSOR_HEIGHT, SENSOR_FPS, UVC_ROTATE},
};
static uvc_frame_info_t app_yuy2_frame_info[] = {
    {1, SENSOR_WIDTH, SENSOR_HEIGHT, SENSOR_FPS, UVC_ROTATE},
    {2, SENSOR_WIDTH, SENSOR_HEIGHT * 2, SENSOR_FPS, UVC_ROTATE},
};
static uvc_frame_info_t app_h264_frame_info[] = {
    {1, SENSOR_WIDTH, SENSOR_HEIGHT, SENSOR_FPS, UVC_ROTATE},
};
static uvc_frame_info_t app_nv12_frame_info[] = {
    {1, SENSOR_WIDTH, SENSOR_HEIGHT, SENSOR_FPS, UVC_ROTATE},
    {2, SENSOR_WIDTH, SENSOR_HEIGHT * 2, SENSOR_FPS, UVC_ROTATE},
};
#endif

static uvc_format_info_t app_uvcout_format_data[] = {
    {1, V4L2_PIX_FMT_MJPEG, 1, sizeof(app_mjpeg_frame_info) / sizeof(uvc_frame_info_t),
     app_mjpeg_frame_info},
    {2, V4L2_PIX_FMT_YUYV, 1, sizeof(app_yuy2_frame_info) / sizeof(uvc_frame_info_t),
     app_yuy2_frame_info},
};
#endif

int set_video_flip(int devIndex, int flip_ctrl, int rotate)
{
    int new_flip_ctrl = 0;

    if (rotate == 0 || rotate == 2) {
        new_flip_ctrl = flip_ctrl;
    } else {
        // exchange h and v
        if (flip_ctrl & 0x01) {
            new_flip_ctrl |= 0x02;
        }
        if (flip_ctrl & 0x02) {
            new_flip_ctrl |= 0x01;
        }
    }
    DOORLOCK_DBG("new_flip_ctrl = %d\n", new_flip_ctrl);
    rt_media_chn_set_flip_ctrl(devIndex, new_flip_ctrl);
}

int door_lock_read_thermal(void)
{
    for (int i = 0; i < THERMAL_ZONE_NUM; i++) {
        thermal_zone_t *thermal_zone = &g_context_info->thermal_zone[i];
        char fileName[256] = {0};
        memset(fileName, 0, sizeof(fileName));
        sprintf(fileName, "/sys/class/thermal/thermal_zone%d/temp", i);
        thermal_zone->temp = doorlock_get_int(fileName, 0);

        memset(fileName, 0, sizeof(fileName));
        sprintf(fileName, "/sys/class/thermal/thermal_zone%d/type", i);
        // cpu_thermal_zone / npu_thermal_zone / ve_thermal_zone
        doorlock_get_str(fileName, thermal_zone->type, sizeof(thermal_zone->type));
        for (int j = 0; j < sizeof(thermal_zone->type); j++) {
            if (thermal_zone->type[j] == '_' || thermal_zone->type[j] == '/') {
                thermal_zone->type[j] = '\0';
            }
        }
    }
}

int door_lock_save_ae(int index)
{
    int ret = rt_media_isp_save_ae(index % 2);

    return ret;
}

int door_lock_set_video_flip(int flip_ctrl)
{
    aw_dev_t *aw_dev_info = &g_context_info->dev_info;
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;
    dev_config->flip_ctl = flip_ctrl;

    DOORLOCK_DBG("flip_ctrl = %d\n", flip_ctrl);
    for (int i = 0; i < 2; i++) {
        set_video_flip(g_context_info->rt_media_algo_chns[i].vi_dev, flip_ctrl,
                       g_context_info->dev_info.config_info.video_rotate);
        set_video_flip(g_context_info->rt_media_chns[i].vi_dev, flip_ctrl,
                       g_context_info->dev_info.config_info.video_rotate);
    }

    return 0;
}

int door_lock_set_video_mode(int index, int mode)
{
    aw_dev_t *aw_dev_info = &g_context_info->dev_info;
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;
    dev_config->video_mode = mode;

    DOORLOCK_INFO("index = %d, mode = %d\n", index, mode);
    index = index % 2;
    rt_media_chn_set_ir_mode(g_context_info->rt_media_algo_chns[index].vi_dev, 0);
    rt_media_chn_set_ir_mode(g_context_info->rt_media_chns[index].vi_dev, 0);
    if (mode) {
        rt_media_chn_set_ir_mode(g_context_info->rt_media_algo_chns[index].vi_dev, mode);
        rt_media_chn_set_ir_mode(g_context_info->rt_media_chns[index].vi_dev, mode);
    }

    return 0;
}

int door_lock_select_uvc_video(int videoIndex)
{
    int videv = videoIndex % 2;
#if !UVC_DYNAMIC_SWITCH_SUPPORT
    return -1;
#endif
    g_context_info->uvc_vi_dev = UVC_VIDEO_BASE + videv;
    return 0;
}

int door_lock_set_led(int led_ctrl)
{
    char *file_name = "/sys/class/pwm/pwmchip0/";

    if (access(file_name, F_OK) == 0) {
        file_name = "/sys/class/pwm/pwmchip0/pwm7";
        if (access(file_name, F_OK) != 0) {
            doorlock_set_str("/sys/class/pwm/pwmchip0/export", "7");
            usleep(2);
        }
        doorlock_set_str("/sys/class/pwm/pwmchip0/pwm7/enable", "0");
        doorlock_set_str("/sys/class/pwm/pwmchip0/pwm7/period", "33000");
        if (led_ctrl) {
            doorlock_set_str("/sys/class/pwm/pwmchip0/pwm7/duty_cycle", "17000");
        } else {
            doorlock_set_str("/sys/class/pwm/pwmchip0/pwm7/duty_cycle", "33000");
        }
        doorlock_set_str("/sys/class/pwm/pwmchip0/pwm7/polarity", "inversed");
        doorlock_set_str("/sys/class/pwm/pwmchip0/pwm7/enable", "1");
    }

    return 0;
}

int door_lock_set_roi_ae(int index, uint8_t target, int x1, int y1, int x2, int y2)
{
    aw_dev_t *aw_dev_info = &g_context_info->dev_info;
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;
    index = index % 2;

    int res_w = ALGO_IMAGE_WIDTH;
    int res_h = ALGO_IMAGE_HEIGHT;
    int coord_x = 0;
    int coord_y = 0;
    int coord_w = 0;
    int coord_h = 0;
#if (APP_ROTATE == 1)  // rotate 90 degree back
    coord_x = y1;
    coord_y = res_w - x2;
    coord_w = y2 - y1;
    coord_h = x2 - x1;
#elif (APP_ROTATE == 3)  // rotate 270 degree back
    coord_x = res_h - y2;
    coord_y = x1;
    coord_w = y2 - y1;
    coord_h = x2 - x1;
#elif (APP_ROTATE == 2)  // rotate 180 degree back
    coord_x = res_w - x2;
    coord_y = res_h - y2;
    coord_w = x2 - x1;
    coord_h = y2 - y1;
#else
    coord_x = x1;
    coord_y = y1;
    coord_w = x2 - x1;
    coord_h = y2 - y1;
#endif

    int width = res_w;
    int height = res_h;
#if (APP_ROTATE == 1 || APP_ROTATE == 3)
    width = res_h;
    height = res_w;
#endif

    rt_media_isp_set_local_exparea_force(g_context_info->rt_media_algo_chns[index].vi_dev, width, height,
                                   coord_x, coord_y, coord_x + coord_w, coord_y + coord_h, target);
    rt_media_isp_set_local_exparea_force(g_context_info->rt_media_chns[index].vi_dev, width, height,
                                   coord_x, coord_y, coord_x + coord_w, coord_y + coord_h, target);

    return 0;
}

int door_lock_get_jpg_data(uint32_t index, uint32_t width, uint32_t height, uint8_t *data,
                           uint32_t max_data_len)
{
    static uint32_t capture_cnt[2] = {0, 0};
    uint32_t jpeg_width = width;
    uint32_t jpeg_height = height;
    int jpeg_len = 0;

    if (index > 2) {
        index = 1;
    }

    if (max_data_len == 0 || data == NULL) {
        DOORLOCK_ERR("data = %d, max_data_len = %d\n", data, max_data_len);
        return 0;
    }
    jpeg_len = rt_media_chn_get_jpeg_data(&g_context_info->rt_media_algo_chns[index], data, max_data_len,
                                     jpeg_width, jpeg_height, 50, APP_ROTATE * 90);
#if 0  // for debug
	if(jpeg_len != 0){
		char file_name[200];
		memset(file_name, 0, sizeof(file_name));
		sprintf(file_name, "/tmp/capture_%d_%d_%d_%d.jpg", index, width, height, capture_cnt[index]);
		FILE *dump_file = fopen(file_name, "wb");
		if(dump_file){
			fwrite(data, 1, jpeg_len, dump_file);
			fclose(dump_file);
			capture_cnt[index]++;
		}else{
			DOORLOCK_ERR("file open [%s] failed\n", file_name);
		}
	}
#endif
    return jpeg_len;
}

int door_lock_save_isp_param(int channel)
{
    //struct isp_autoflash_config isp_auto_flash;
    //AWVideoInput_SaveIspParamToFlash(channel/*, &isp_auto_flash*/);

    return 0;
}

int door_lock_set_audio_volume(int volume)  // volume:0-100
{
#if UAC_SUPPORT
    uac_dev_t *uac_dev_info = &g_context_info->uac_dev_info;
#if MPPAIO_SUPPORT
    if (uac_dev_info->mic) {
        mpp_ai_s *pMppAiInfo = &g_context_info->mpp_ai_info;
        AW_MPI_AI_SetDevVolume(pMppAiInfo->ai_dev, volume);
    }

    if (uac_dev_info->spk) {
        mpp_ao_t *mpp_ao_info = &g_context_info->mpp_ao_info;
        AW_MPI_AO_SetDevVolume(mpp_ao_info->ao_dev, volume);
    }
#endif
#endif
    return 0;
}

int algo_init(algo_info_t *algo_info)
{
    int ret = 0;

    if (access("/dev/by-name/data", F_OK) == 0) {
#if 0
		//system("/bin/mount -t jffs2 /dev/by-name/data /data\n");
		//system("chown root:root -R /data\n");
#else
        system("/bin/mount -t squashfs -o loop /dev/by-name/data /data\n");
#endif
    }

    aw_dev_t *aw_dev_info = &g_context_info->dev_info;
    algo_info->aw_alg.alg_max_mem_size = ALGO_IMAGE_SIZE;  //
    ret = aw_alg_init(&algo_info->aw_alg);
    if (ret != 0) {
        DOORLOCK_ERR("aw_alg_init ret = %d\n\n", ret);
        ret = -1;
        goto exit;
    }

    algo_info->alg_pix_info.aw_alg_info = &algo_info->aw_alg;
    algo_info->alg_pix_info.detect_model_path =
        (char *)"/data/models/face_hand_detect_v1_12_20241202.bin";
    algo_info->alg_pix_info.face_model_path =
        (char *)"/data/models/face_model_v1_6_20241202.bin";
    algo_info->alg_pix_info.face_lazy_model_path =
        (char *)"/data/models/pix_face_lazy_v1_0_20230515.bin";
    algo_info->alg_pix_info.palm_model_path =
        (char *)"/data/models/hand_model_v2_15_20240411.bin";
    // memcpy(algo_info->alg_pix_info.auth_key, aw_dev_info->hw_info.auth_key,
    // sizeof(aw_dev_info->hw_info.auth_key));
    uint8_t hardware_info[PIX_HARDWARE_INFO_BYTES] = {0};
    ret = pix_get_hardware_info(hardware_info, PIX_HARDWARE_INFO_BYTES);
    DOORLOCK_INFO("Pix hardware info is:\n");
    for (uint32_t i = 0; i < PIX_HARDWARE_INFO_BYTES; i++){
        printf("0x%02x,", hardware_info[i]);
    }
    printf("\n");
    if (ret == SDK_CODE_OK) {
        memcpy(aw_dev_info->hw_info.algoid, hardware_info, sizeof(hardware_info));
        aw_dev_info->hw_info.algoid_len = sizeof(hardware_info);
        DOORLOCK_DBG("pix_get_hardware_info ok, len = %d\n", sizeof(hardware_info));
    } else {
        DOORLOCK_ERR("pix_get_hardware_info failed, ret = %d\n", ret);
        aw_dev_info->hw_info.algoid_len = 0;
        ret = -1;
        goto exit;
    }

    ret = alg_pix_init(&algo_info->alg_pix_info);
    if (ret == 0) {
        aw_dev_info->hw_info.auth_ok = 1;
    } else {
        aw_dev_info->hw_info.auth_ok = 0;
    }

exit:
    return ret;
}

int algo_deinit(algo_info_t *algo_info)
{
    int ret = 0;

    ret = alg_pix_deinit(&algo_info->alg_pix_info);
    ret = aw_alg_deinit(&algo_info->aw_alg);

    return ret;
}

int algo_detect(algo_info_t *algo_info, uint8_t *image_vir0, uint8_t *image_phy0, uint32_t width0,
                uint32_t height0, uint32_t v4l2_fmt0, uint8_t *image_vir1, uint8_t *image_phy1,
                uint32_t width1, uint32_t height1, uint32_t v4l2_fmt1)
{
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;
    static uint32_t both_frame_cnt = 0;
    static uint32_t register_frame_cnt = 0;

    both_frame_cnt++;
    if ((both_frame_cnt % 300) == 0) {
        DOORLOCK_INFO("[%d x %d] [%d x %d]\n", width0, height0, width1, height1);
    }

    aw_dev_t *aw_dev_info = &g_context_info->dev_info;

#if UVC_SUPPORT
    uvc_out_t *uvc_out_info = &g_context_info->uvc_out_info;
    int tmp_width = uvc_out_info->cap_width;
    int tmp_height = uvc_out_info->cap_height;
    if (g_context_info->uvc_dual_transfer == 1) {
        tmp_width = tmp_width / 2;
    }

    if (g_context_info->uvc_dual_transfer == 2) {
        tmp_height = tmp_height / 2;
    }

    if (uvc_out_info->rotate == 1 || uvc_out_info->rotate == 3) {
        int tmp_data = tmp_width;
        tmp_width = tmp_height;
        tmp_height = tmp_data;
    }

    if (!uvc_out_info->is_resource_ok) {
        tmp_width = SENSOR_WIDTH;
        tmp_height = SENSOR_HEIGHT;
    }

#if (UVC_ROTATE == 1 || UVC_ROTATE == 3)
    float x_scale = (float)tmp_height / width0;
    float y_scale = (float)tmp_width / height0;
#else
    float x_scale = (float)tmp_width / width0;
    float y_scale = (float)tmp_height / height0;
#endif
#endif  // UVC_SUPPORT

    int uvc_index = g_context_info->uvc_vi_dev % 2;
#if ORL_SUPPORT
    rt_media_orl_t *rt_media_orl_info = &g_context_info->rt_media_orl_infos[uvc_index];
    if (orl_enable) {
        rt_media_orl_info->line_width = 1;
        rt_media_orl_info->num = 0;
    }
#endif  // ORL_SUPPORT

#if OSD_SUPPORT
    rt_media_osd_chn_t *rt_media_osd_chn_info = &g_context_info->rt_media_osd_chns[uvc_index];
#endif

    static uint32_t last_face_num = 0;
    {
        uint8_t pix_detect_num = 0;
        uint8_t pix_detect_type = -1;
        static uint8_t register_flag = 0;
        alg_pix_result_t pix_result = {0};
        algo_info->alg_pix_info.yuv_phy_addrs[0] = image_phy0;
        algo_info->alg_pix_info.yuv_vir_addrs[0] = image_vir0;
        algo_info->alg_pix_info.yuv_widths[0] = width0;
        algo_info->alg_pix_info.yuv_heights[0] = height0;
        algo_info->alg_pix_info.yuv_formats[0] = v4l2_fmt0;
        algo_info->alg_pix_info.yuv_phy_addrs[1] = image_phy1;
        algo_info->alg_pix_info.yuv_vir_addrs[1] = image_vir1;
        algo_info->alg_pix_info.yuv_widths[1] = width1;
        algo_info->alg_pix_info.yuv_heights[1] = height1;
        algo_info->alg_pix_info.yuv_formats[1] = v4l2_fmt1;

        pthread_mutex_lock(&aw_dev_info->register_mutex);
        if (aw_dev_info->register_flag) {
            aw_dev_info->register_flag = 0;
            register_flag = 1;
            register_frame_cnt = 0;
        }
        pthread_mutex_unlock(&aw_dev_info->register_mutex);

        int ret = alg_pix_detect(&algo_info->alg_pix_info, register_flag, &pix_result,
                               &pix_detect_num, &pix_detect_type);

        if (ret != 0 || pix_detect_num <= 0) {
            goto detect_end;
        }

        DOORLOCK_INFO("pix_detect_num:%d\n", pix_detect_num);

        //识别逻辑，数据库有数据和register_flag为0才做识别处理
        if (aw_dev_info->users_database.user_count > 0 && register_flag == 0) {
            uint8_t feature_type = 0;
            users_database_t *users_database = &aw_dev_info->users_database;
            DOORLOCK_INFO("Database User Count:%d\n", users_database->user_count);
            for (int m = 0; m < users_database->user_count; m++) {
                DOORLOCK_INFO("User[%d]:feature_type:%d\n", m, feature_type);
                feature_type = users_database->users[m].feature_type;
                if (feature_type == FEATURE_TYPE_FACE) {
                    for (int j = 0; j < pix_detect_num; j++) {
                        pix_fr_feature_info_t *feature0 = NULL;
                        pix_fr_feature_info_t *feature1 = NULL;
                        feature0 = &pix_result.face_result_list[j].fr_feature;
                        feature1 = &users_database->users[m].face_data;
                        if (alg_pix_face_compare(&algo_info->alg_pix_info, feature0, feature1,
                                              &pix_result.face_result_list[j].fr_score) == 0) {
                            DOORLOCK_INFO("ID[%d] face detected, fr_score %f\n",
                                   users_database->users[m].user_id,
                                   pix_result.face_result_list[j].fr_score);
                            // 通知门锁板认证通过
                            msg_prepare_verify_reply_data(users_database->users[m].user_id,
                                                          users_database->users[m].name,
                                                          users_database->users[m].admin, VERIFY_TYPE_FACE);
                            msg_send_reply(aw_dev_info, MID_VERIFY, MR_SUCCESS);
                        }
                    }
                } else if (feature_type == FEATURE_TYPE_PALM) {
                    pix_pr_feature_info_t *feature0 = NULL;
                    pix_pr_feature_info_t *feature1 = NULL;
                    feature0 = &pix_result.palm_result.pr_feature;
                    feature1 = &users_database->users[m].palm_data;
                    if (alg_pix_palm_compare(&algo_info->alg_pix_info, feature0, feature1,
                                          &pix_result.palm_result.pr_score) == 0) {
                        DOORLOCK_INFO("ID[%d] palm detected, pr_score %f\n",
                               users_database->users[m].user_id, pix_result.palm_result.pr_score);
                        // 通知门锁板认证通过
                        msg_prepare_verify_reply_data(users_database->users[m].user_id,
                                                      users_database->users[m].name,
                                                      users_database->users[m].admin, VERIFY_TYPE_PALM);
                        msg_send_reply(aw_dev_info, MID_VERIFY, MR_SUCCESS);
                        break;
                    }
                }
            }
        }

        //注册逻辑，register_flag为1表示在录入
        if (register_flag == 1) {
            if (aw_dev_info->enroll_type == FEATURE_TYPE_FACE &&
                pix_detect_type == FEATURE_TYPE_FACE) {
                DOORLOCK_INFO("-------enroll face----------\n");
                if (pix_result.face_result_list[0].liveness >=
                    pix_result.face_result_list[0].liveness_threshold) {
                    register_flag = 0;
                    DOORLOCK_INFO("five_face_register_count:%d\n", g_five_face_register_count);
                    if (aw_dev_info->five_point_enroll) {
                        g_five_face_ready_direction += aw_dev_info->face_direction;
                        memcpy(&g_last_feature, &pix_result.face_result_list[0].fr_feature,
                               sizeof(pix_fr_feature_info_t));

                        if (g_five_face_register_count == 0) {
                            msg_prepare_enroll_reply_data(0xffff, g_five_face_ready_direction, 0);
                            msg_send_reply(aw_dev_info, MID_ENROLL, MR_SUCCESS);
                        }

                        if (g_five_face_register_count > 0 && g_five_face_register_count < 4) {
                            // 与前一个角度的人脸数据对比
                            if (alg_pix_face_compare(&algo_info->alg_pix_info,
                                                  &pix_result.face_result_list[0].fr_feature,
                                                  &g_last_feature,
                                                  &pix_result.face_result_list[0].fr_score) == 0) {
                                msg_prepare_enroll_reply_data(0xffff, g_five_face_ready_direction,
                                                              ENROLL_FACE_UNFINISH);
                                msg_send_reply(aw_dev_info, MID_ENROLL, MR_SUCCESS);
                            }
                        }

                        if (g_five_face_register_count == 4) {
                            if (alg_pix_face_compare(&algo_info->alg_pix_info,
                                                  &pix_result.face_result_list[0].fr_feature,
                                                  &g_last_feature,
                                                  &pix_result.face_result_list[0].fr_score) == 0) {
                                // 需要分配一个ID号，并回复人脸录入完成
                                unsigned int user_id = aw_dev_info->users_database.latest_id;
                                DOORLOCK_INFO("new user id:%d\n", user_id);
                                msg_prepare_enroll_reply_data(user_id, g_five_face_ready_direction,
                                                              ENROLL_FACE_FINISH);
                                msg_send_reply(aw_dev_info, MID_ENROLL, MR_SUCCESS);
                                users_database_t *database = &aw_dev_info->users_database;
                                // 添加成功，数据库存储到Flash
                                memcpy(&aw_dev_info->register_user_info.face_data,
                                       &pix_result.face_result_list[0].fr_feature,
                                       sizeof(pix_fr_feature_info_t));
                                aw_dev_info->users_database.latest_id++;
                                aw_dev_database_adduser(database, user_id, FEATURE_TYPE_FACE,
                                                        &pix_result.face_result_list[0].fr_feature);
                                aw_dev_save_user(aw_dev_info, database);
                                DOORLOCK_INFO("register face success\n");
                                aw_dev_info->enroll_type = FEATURE_TYPE_NULL;
                                register_frame_cnt = 0;
                                system("sync");
                            }
                        }
                    }
                    g_five_face_register_count++;
                }
            } else if (aw_dev_info->enroll_type == FEATURE_TYPE_PALM &&
                       pix_detect_type == FEATURE_TYPE_PALM) {
                DOORLOCK_INFO("-------enroll palm----------\n");
                unsigned int user_id = aw_dev_info->users_database.latest_id;
                register_flag = 0;
                DOORLOCK_INFO("new user id:%d\n", user_id);
                msg_prepare_enroll_reply_data(user_id, 0, ENROLL_PALM_FINISH);
                msg_send_reply(aw_dev_info, MID_PALM_ENROLL_ITG, MR_SUCCESS);
                users_database_t *database = &aw_dev_info->users_database;
                // 添加成功，数据库存储到Flash
                memcpy(&aw_dev_info->register_user_info.palm_data,
                        &pix_result.palm_result.pr_feature, sizeof(pix_pr_feature_info_t));
                aw_dev_info->users_database.latest_id++;
                aw_dev_database_adduser(database, user_id, FEATURE_TYPE_PALM,
                                        &pix_result.palm_result.pr_feature);
                aw_dev_save_user(aw_dev_info, database);
                DOORLOCK_INFO("register palm success\n");
                aw_dev_info->enroll_type = FEATURE_TYPE_NULL;
                register_frame_cnt = 0;
                system("sync");
            }
        }

#if FACEAE_SUPPORT
        if (dev_config->face_ae) {
            if (pix_detect_num) {
                if (dev_config->roi_ae_target0) {
                    uint8_t forceValue = dev_config->roi_ae_target0;
                    door_lock_set_roi_ae(0, forceValue, pix_result.face_result_list[0].x1,
                                         pix_result.face_result_list[0].y1,
                                         pix_result.face_result_list[0].x2,
                                         pix_result.face_result_list[0].y2);
                }
                if (dev_config->roi_ae_target1) {
                    uint8_t forceValue = dev_config->roi_ae_target1;
                    door_lock_set_roi_ae(1, forceValue, pix_result.face_result_list[0].x1,
                                         pix_result.face_result_list[0].y1,
                                         pix_result.face_result_list[0].x2,
                                         pix_result.face_result_list[0].y2);
                }
            }
            if (pix_detect_num == 0 && last_face_num != 0) {
                if (dev_config->roi_ae_target0) {
                    door_lock_set_roi_ae(0, 0, 0, 0, ALGO_IMAGE_WIDTH - 1, ALGO_IMAGE_HEIGHT - 1);
                }
                if (dev_config->roi_ae_target1) {
                    door_lock_set_roi_ae(1, 0, 0, 0, ALGO_IMAGE_WIDTH - 1, ALGO_IMAGE_HEIGHT - 1);
                }
            }
        }
#endif
        last_face_num = pix_detect_num;

#if UVC_SUPPORT
#if ORL_SUPPORT
        if (orl_enable) {
            for (int i = 0; i < pix_detect_num; i++) {
                if (rt_media_orl_info->num + i >= MAX_ISP_ORL_NUM) {
                    continue;
                }
                alg_coord_t src_coord, dst_coord;
                src_coord.width = ALGO_IMAGE_WIDTH;
                src_coord.height = ALGO_IMAGE_HEIGHT;
                src_coord.x1 = pix_result.face_result_list[i].x1;
                src_coord.y1 = pix_result.face_result_list[i].y1;
                src_coord.x2 = pix_result.face_result_list[i].x2;
                src_coord.y2 = pix_result.face_result_list[i].y2;
                aw_alg_coord_rotate(&src_coord, &dst_coord, 360 - APP_ROTATE * 90);  // rotate back
                rt_media_orl_info->pos_x[rt_media_orl_info->num + i] = dst_coord.x1;
                rt_media_orl_info->pos_y[rt_media_orl_info->num + i] = dst_coord.y1;
                rt_media_orl_info->width[rt_media_orl_info->num + i] =
                    dst_coord.x2 - dst_coord.x1;
                rt_media_orl_info->height[rt_media_orl_info->num + i] =
                    dst_coord.y2 - dst_coord.y1;
                // SCALE
                rt_media_orl_info->pos_x[rt_media_orl_info->num + i] =
                    rt_media_orl_info->pos_x[rt_media_orl_info->num + i] * x_scale;
                rt_media_orl_info->pos_y[rt_media_orl_info->num + i] =
                    rt_media_orl_info->pos_y[rt_media_orl_info->num + i] * y_scale;
                rt_media_orl_info->width[rt_media_orl_info->num + i] =
                    rt_media_orl_info->width[rt_media_orl_info->num + i] * x_scale;
                rt_media_orl_info->height[rt_media_orl_info->num + i] =
                    rt_media_orl_info->height[rt_media_orl_info->num + i] * y_scale;

                if (pix_result.face_result_list[i].liveness > pix_get_liveness_threshold()) {
                    rt_media_orl_info->color_rgb[rt_media_orl_info->num + i] = 0x00ff00;  // GREEN
                } else {
                    rt_media_orl_info->color_rgb[rt_media_orl_info->num + i] = 0xff0000;  // RED
                }
            }
            rt_media_orl_info->num += pix_detect_num;
        }
#endif
#endif
//检测注册是否超时
detect_end:
        if (register_flag) {
            register_frame_cnt++;
            if (register_frame_cnt >= aw_dev_info->register_timeout * 30) {
                DOORLOCK_ERR("ENROLL Timeout!\n");
                register_flag = 0;
                if (aw_dev_info->enroll_type == FEATURE_TYPE_FACE) {
                    g_five_face_register_count = 0;
                    msg_send_reply(aw_dev_info, MID_ENROLL, MR_FAILED4_TIMEOUT);
                } else if (aw_dev_info->enroll_type == FEATURE_TYPE_PALM) {
                    msg_send_reply(aw_dev_info, MID_PALM_ENROLL_ITG, MR_FAILED4_TIMEOUT);
                }
            }
        }
    }

#if UVC_SUPPORT
#if ORL_SUPPORT
    if (uvc_out_info->is_resource_ok && orl_enable) {
        if (uvc_out_info->cap_width <= 800) {
            rt_media_orl_info->line_width = 1;
        }
        if (rt_media_orl_info->num >= MAX_ISP_ORL_NUM) {
            DOORLOCK_ERR("rt_media_orl_info->num = %d, MAX_ISP_ORL_NUM = %d\n",
                        rt_media_orl_info->num, MAX_ISP_ORL_NUM);
            rt_media_orl_info->num = MAX_ISP_ORL_NUM - 1;
        }
        rt_media_orl_update(rt_media_orl_info);
    }
#endif
#if 0  // OSD_SUPPORT
    if (uvc_out_info->is_resource_ok && dev_config->video_osd) {
        rt_media_osd_chn_update(rt_media_osd_chn_info);
    }
#endif
#endif
}

static void *algo_get_yuv_thread(void *arg)
{
    int i = (*(int *)arg);
    DOORLOCK_INFO("algo_get_yuv_thread start[%d]  %lld us\n", i, get_cur_time_us());
    int ret = 0;
    VideoYuvFrame video_yuv_frame;
    aw_dev_t *aw_dev_info = &g_context_info->dev_info;
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;

    g_context_info->rt_media_algo_chns[i].v4l2_format_type = V4L2_PIX_FMT_NV21;
    g_context_info->rt_media_algo_chns[i].vi_dev = ALGO_VIDEO_BASE + i;
    g_context_info->rt_media_algo_chns[i].input_rt_format = RT_PIXEL_YVU420SP;
    g_context_info->rt_media_algo_chns[i].src_width = SENSOR_WIDTH;
    g_context_info->rt_media_algo_chns[i].src_height = SENSOR_HEIGHT;
    g_context_info->rt_media_algo_chns[i].dst_width = SENSOR_WIDTH;
    g_context_info->rt_media_algo_chns[i].dst_height = SENSOR_HEIGHT;
    g_context_info->rt_media_algo_chns[i].src_fps = SENSOR_FPS;
    g_context_info->rt_media_algo_chns[i].dst_fps = SENSOR_FPS;
    g_context_info->rt_media_algo_chns[i].bitrate = 0xffffffff;
    g_context_info->rt_media_algo_chns[i].enc_quality = 99;
    // g_context_info->rt_media_algo_chns[i].rc_mode = 0;//0:CBR, 1:VBR, 2:FIXQP, 3:QPMAP
    g_context_info->rt_media_algo_chns[i].chn = ALGO_VIDEO_BASE + i;
    g_context_info->rt_media_algo_chns[i].thread_attr = NULL;

    rt_media_chn_init(&g_context_info->rt_media_algo_chns[i]);
    rt_media_chn_start(&g_context_info->rt_media_algo_chns[i]);
    // DOORLOCK_INFO("algo_get_yuv_thread after rt_media_chn_start[%d]  %lld us\n", i,  get_cur_time_us());

    DOORLOCK_INFO("algo after init video[%d]  %lld us\n", i, get_cur_time_us());

    uint32_t frame_cnt = 0;
    while (1) {
        if (g_exit_flag) {
            break;
        }

        memset(&video_yuv_frame, 0, sizeof(VideoYuvFrame));
        ret = rt_media_chn_request_yuv_data(&g_context_info->rt_media_algo_chns[i], &video_yuv_frame);
        if (0 == ret) {
            if (frame_cnt == 0) {
                DOORLOCK_INFO("algo yuv get first video[%d]  %lld us\n", i, get_cur_time_us());
            }
            frame_cnt++;
            uint8_t *image_vir = 0;
            uint8_t *image_phy = 0;
            uint32_t phy_src[3];
            uint32_t phy_dst[3];

            frm_manager_t *frm_manager = &g_context_info->algo_frms_info[i];
            frame_mem_t *buf_tmp = NULL;
            if (frm_manager->prefetch_first_idle_frame)
                ret = frm_manager->prefetch_first_idle_frame(frm_manager, &buf_tmp);

            if (buf_tmp == NULL || ret < 0) {
                rt_media_chn_return_yuv_data(&g_context_info->rt_media_algo_chns[i], &video_yuv_frame);
                usleep(10 * 1000);
                continue;
            }

            image_vir = buf_tmp->mem_info.mem_vir;
            image_phy = (uint8_t *)buf_tmp->mem_info.mem_phy;

            phy_src[0] = (uint32_t)video_yuv_frame.phyAddr[0];
            phy_src[1] = (uint32_t)video_yuv_frame.phyAddr[1];
            phy_src[2] = (uint32_t)video_yuv_frame.phyAddr[2];
            phy_dst[0] = (uint32_t)image_phy;
            phy_dst[1] = (uint32_t)image_phy + video_yuv_frame.widht * video_yuv_frame.height;
            phy_dst[2] = 0;

#if (APP_ROTATE == 1 || APP_ROTATE == 3)
            aw_g2d_rotate(V4L2_FORMAT_to_G2D_FORMAT(g_context_info->input_video_v4l2_format),
                      phy_src, video_yuv_frame.widht, video_yuv_frame.height, phy_dst,
                      video_yuv_frame.height, video_yuv_frame.widht, APP_ROTATE);
#elif (APP_ROTATE == 2)
            aw_g2d_rotate(V4L2_FORMAT_to_G2D_FORMAT(g_context_info->input_video_v4l2_format),
                      phy_src, video_yuv_frame.widht, video_yuv_frame.height, phy_dst,
                      video_yuv_frame.widht, video_yuv_frame.height, APP_ROTATE);
#else
            aw_g2d_copy(V4L2_FORMAT_to_G2D_FORMAT(g_context_info->input_video_v4l2_format),
                    phy_src, video_yuv_frame.widht, video_yuv_frame.height, phy_dst,
                    video_yuv_frame.widht, video_yuv_frame.height);
#endif

            rt_media_chn_return_yuv_data(&g_context_info->rt_media_algo_chns[i], &video_yuv_frame);
            if (frm_manager->first_idle_to_using_frame)
                frm_manager->first_idle_to_using_frame(frm_manager, buf_tmp);
        }

        usleep(100 * 1000);
    }

    rt_media_chn_stop(&g_context_info->rt_media_algo_chns[i]);
    rt_media_chn_deinit(&g_context_info->rt_media_algo_chns[i]);
}

#define REDUCE_MEM 0
static void *algo_detect_thread(void *arg)
{
    DOORLOCK_INFO("algo_detect_thread start  %lld us\n", get_cur_time_us());
    int ret = 0;
    int got_frame[2] = {0, 0};
    uint32_t frame_cnt[2] = {0, 0};
    uint32_t both_frame_cnt = 0;
    uint8_t *image_vir[2] = {0};
    uint8_t *image_phy[2] = {0};
    uint32_t image_width[2] = {0};
    uint32_t image_height[2] = {0};
    frame_mem_t *buf_mem_node[2] = {0};
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;

    aw_dev_init(&g_context_info->dev_info);

    DOORLOCK_INFO("algo before init npu %lld us\n", get_cur_time_us());
    if (algo_init(&g_context_info->algo_info) != 0) {
        DOORLOCK_ERR("algo_init failed\n");
        g_context_info->algo_info.algo_state = 1;
        return NULL;
    }
    DOORLOCK_INFO("algo after init npu %lld us\n", get_cur_time_us());
    g_context_info->algo_info.algo_state = 1;

    for (int i = 0; i < 2; i++) {
        image_width[i] = ALGO_IMAGE_WIDTH;
        image_height[i] = ALGO_IMAGE_HEIGHT;
#if !REDUCE_MEM
        aw_mem_info_t *algo_mem = &g_context_info->algo_mem_info[i];
        algo_mem->mem_size = ALGO_IMAGE_SIZE;  // yuv420
        algo_mem->mem_cache = 0;
        aw_mem_malloc(algo_mem);
        // image_vir[i] = malloc(ALGO_IMAGE_SIZE);
        image_vir[i] = algo_mem->mem_vir;
        if (image_vir[i] == NULL) {
            DOORLOCK_ERR("malloc %d failed\n", ALGO_IMAGE_SIZE);
            return NULL;
        }
        image_phy[i] = (uint8_t *)algo_mem->mem_phy;
#endif
    }

    while (1) {
        if (g_exit_flag) {
            break;
        }
        for (int i = 0; i < 2; i++) {
            if (got_frame[i]) {
                usleep(5 * 1000);
                continue;
            }
            frm_manager_t *frm_manager = &g_context_info->algo_frms_info[i];
            frame_mem_t *buf_tmp = NULL;
            if (frm_manager->prefetch_first_using_frame)
                ret = frm_manager->prefetch_first_using_frame(frm_manager, &buf_tmp);
            if (buf_tmp == NULL || ret < 0) {
                // DOORLOCK_DBG("frm_manager->prefetch_first_using_frame fail\n");
                usleep(5 * 1000);
                continue;
            }

            if (0 == ret) {
#if !REDUCE_MEM
                memcpy(image_vir[i], buf_tmp->mem_info.mem_vir, ALGO_IMAGE_SIZE);
#else
                image_vir[i] = buf_tmp->mem_info.mem_vir;
                image_phy[i] = buf_tmp->mem_info.mem_phy;
#endif
                buf_mem_node[i] = buf_tmp;
                got_frame[i] = 1;
                if (frame_cnt[i] == 0) {
                    DOORLOCK_INFO("algo detect get first video[%d] %lld us\n", i, get_cur_time_us());
                }
                frame_cnt[i]++;
            }
        }

        if (got_frame[0] && got_frame[1]) {
            if (both_frame_cnt == 0) {
                DOORLOCK_INFO("algo before detect first frame %lld us\n", get_cur_time_us());
            }
            got_frame[0] = 0;
            got_frame[1] = 0;
            if ((dev_config->video_ir_index % 2) == 0) {
                algo_detect(&g_context_info->algo_info, image_vir[0], image_phy[0], image_width[0],
                            image_height[0], V4L2_PIX_FMT_NV21, image_vir[1], image_phy[1],
                            image_width[1], image_height[1], V4L2_PIX_FMT_NV21);
            } else {
                algo_detect(&g_context_info->algo_info, image_vir[1], image_phy[1], image_width[1],
                            image_height[1], V4L2_PIX_FMT_NV21, image_vir[0], image_phy[0],
                            image_width[0], image_height[0], V4L2_PIX_FMT_NV21);
            }

            if (both_frame_cnt == 0) {
                DOORLOCK_INFO("algo after detect first frame %lld us\n", get_cur_time_us());
            }
            both_frame_cnt++;

            for (int i = 0; i < 2; i++) {
                frm_manager_t *frm_manager = &g_context_info->algo_frms_info[i];
                frame_mem_t *buf_tmp = buf_mem_node[i];
                if (frm_manager->first_using_to_idle_frame) {
                    frm_manager->first_using_to_idle_frame(frm_manager, buf_tmp);
                }
            }
        }
        usleep(100 * 1000);
    }

#if !REDUCE_MEM
    for (int i = 0; i < 2; i++) {
        aw_mem_info_t *algo_mem = &g_context_info->algo_mem_info[i];
        aw_mem_free(algo_mem);
    }
#endif
    algo_deinit(&g_context_info->algo_info);
}

#if UVC_SUPPORT
static uint64_t last_timestamp_us[2] = {0, 0};
static uint64_t cur_timestamp_us[2] = {0, 0};
static uint32_t act_fps[2] = {0, 0};
static uint32_t active_cnt[2] = {0, 0};
#define FPS_ACTIVE_CNT SENSOR_FPS  // 30

static void *get_yuv_frame_thread(void *arg)
{
    int ret = 0;
    VideoYuvFrame video_yuv_frame;
    uvc_out_t *uvc_out_info = &g_context_info->uvc_out_info;
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;
    int dual_transfer = 0;  // 0:no dual, 1:left,right, 2:up,down

    DOORLOCK_DBG("enter ===>\n");
    dual_transfer = g_context_info->uvc_dual_transfer;
    DOORLOCK_INFO("dual_transfer = %d\n", dual_transfer);
    aw_mem_info_t convert_mem;

    if (dual_transfer) {
        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
            convert_mem.mem_size =
                ALIGN_16B(uvc_out_info->cap_width) * ALIGN_16B(uvc_out_info->cap_height) / 2 * 2;
        } else if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
            // convert_mem.mem_size =
            // ALIGN_16B(uvc_out_info->cap_width)*ALIGN_16B(uvc_out_info->cap_height)/2*3/2;
            convert_mem.mem_size =
                ALIGN_16B(uvc_out_info->cap_width) * ALIGN_16B(uvc_out_info->cap_height) / 2 * 2;
        }
        convert_mem.mem_cache = 0;
        aw_mem_malloc(&convert_mem);
    }
    uint32_t g2d_dst_fmt = G2D_FORMAT_IYUV422_V0Y1U0Y0;
    if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
        g2d_dst_fmt = G2D_FORMAT_YUV420UVC_V1U1V0U0;
    }
    int got_frame[2] = {0, 0};

    font_info_t font_info;
    font_info_t *font = &font_info;
    draw_font_init(font, 32, "/usr/bin/");
    char time_str[256] = {0};
    color_yuv_t ft_nv21 = {0x00, 0x00, 0x00};
    color_yuv_t bk_nv21 = {0x00, 0x00, 0x00};
    COLOR_RGB_TO_YUV(255, 0, 0, &ft_nv21.y, &ft_nv21.u, &ft_nv21.v);

    while (1) {
        if (g_exit_flag || uvc_out_info->is_resource_ok == 0) {
            DOORLOCK_INFO("time to exit\n");
            break;
        }
        for (int i = 0; i < 2; i++) {
            int uvc_index = g_context_info->uvc_vi_dev % 2;

#if !UVC_DYNAMIC_SWITCH_SUPPORT
            if ((uvc_index % 2) != i && dual_transfer == 0) {
                continue;
            }
#endif
            memset(&video_yuv_frame, 0, sizeof(VideoYuvFrame));
            ret = rt_media_chn_request_yuv_data(&g_context_info->rt_media_chns[i], &video_yuv_frame);
            if (0 == ret) {
                if ((active_cnt[i] % FPS_ACTIVE_CNT) == 0) {
                    cur_timestamp_us[i] = get_cur_time_us();
                    if (last_timestamp_us[i])
                        act_fps[i] =
                            1000 * 1000 /
                            ((cur_timestamp_us[i] - last_timestamp_us[i]) / FPS_ACTIVE_CNT);
                    last_timestamp_us[i] = cur_timestamp_us[i];
                }
                active_cnt[i]++;
                frm_manager_t *frm_manager = &uvc_out_info->frm_manager;
                frame_mem_t *buf_tmp = NULL;
                int ret = 0;
                if (frm_manager->prefetch_first_idle_frame)
                    ret = frm_manager->prefetch_first_idle_frame(frm_manager, &buf_tmp);
                if (buf_tmp == NULL || ret < 0) {
                    rt_media_chn_return_yuv_data(&g_context_info->rt_media_chns[i], &video_yuv_frame);
                    usleep(10 * 1000);
                    continue;
                }

#if UVC_DYNAMIC_SWITCH_SUPPORT
                if ((uvc_index % 2) != i && dual_transfer == 0) {
                    rt_media_chn_return_yuv_data(&g_context_info->rt_media_chns[i], &video_yuv_frame);
                    usleep(10 * 1000);
                    continue;
                }
#endif
                uint32_t phy_src[3];
                uint32_t phy_dst[3];
                phy_src[0] = (uint32_t)video_yuv_frame.phyAddr[0];
                phy_src[1] = (uint32_t)video_yuv_frame.phyAddr[1];
                phy_src[2] = (uint32_t)video_yuv_frame.phyAddr[2];

#if UVC_ROTATE
                aw_mem_info_t *mem_rotate = &g_context_info->rotate_mem_info;
                if (uvc_out_info->rotate) {
                    phy_dst[0] = mem_rotate->mem_phy;
                    phy_dst[1] =
                        mem_rotate->mem_phy + video_yuv_frame.widht * video_yuv_frame.height;
                    phy_dst[2] = 0;
                    if (uvc_out_info->rotate == 1 || uvc_out_info->rotate == 3) {
                        aw_g2d_rotate(V4L2_FORMAT_to_G2D_FORMAT(
                                      g_context_info->input_video_v4l2_format),
                                  phy_src, video_yuv_frame.widht, video_yuv_frame.height, phy_dst,
                                  video_yuv_frame.height, video_yuv_frame.widht,
                                  uvc_out_info->rotate);
                    } else if (uvc_out_info->rotate == 2) {
                        aw_g2d_rotate(V4L2_FORMAT_to_G2D_FORMAT(
                                      g_context_info->input_video_v4l2_format),
                                  phy_src, video_yuv_frame.widht, video_yuv_frame.height, phy_dst,
                                  video_yuv_frame.widht, video_yuv_frame.height,
                                  uvc_out_info->rotate);
                    }

                    phy_src[0] = mem_rotate->mem_phy;
                    phy_src[1] =
                        mem_rotate->mem_phy + video_yuv_frame.widht * video_yuv_frame.height;
                    phy_src[2] = 0;
#if OSD_SUPPORT
                    if (dev_config->video_osd) {
                        memset(time_str, 0, sizeof(time_str));
                        char tmp_str[256] = {0};
                        if (1) {
                            memset(tmp_str, 0, sizeof(tmp_str));
                            time_t now = time(0);
                            struct tm *p_tm = gmtime(&now);
                            sprintf(tmp_str, "%4d-%02d-%02d %02d:%02d:%02d", p_tm->tm_year + 1900,
                                    p_tm->tm_mon + 1, p_tm->tm_mday, p_tm->tm_hour, p_tm->tm_min,
                                    p_tm->tm_sec);
                            strcat(time_str, tmp_str);
                        }
                        if (1) {
                            memset(tmp_str, 0, sizeof(tmp_str));
                            sprintf(tmp_str, "\nCpuUsage:%.2lf%%", g_context_info->cpu_usage);
                            strcat(time_str, tmp_str);
                        }
                        if (1) {
                            memset(tmp_str, 0, sizeof(tmp_str));
                            sprintf(tmp_str, "\nMemUsage:%.2lf%%", g_context_info->mem_usage);
                            strcat(time_str, tmp_str);
                        }
                        if (1) {
                            memset(tmp_str, 0, sizeof(tmp_str));
                            sprintf(tmp_str, "\nVi[%d]Fps:%d->%d\n",
                                    g_context_info->rt_media_chns[i].vi_dev,
                                    AWVideoInput_GetFps(g_context_info->rt_media_chns[i].vi_dev),
                                    act_fps[i]);
                            strcat(time_str, tmp_str);
                        }
                        if (1) {
                            door_lock_read_thermal();
                            for (int zone = 0; zone < THERMAL_ZONE_NUM; zone++) {
                                thermal_zone_t *thermal_zone = &g_context_info->thermal_zone[zone];
                                if (strlen(thermal_zone->type)) {
                                    sprintf(tmp_str, "%s:%d ", thermal_zone->type,
                                            (thermal_zone->temp + 500) / 1000);
                                    strcat(time_str, tmp_str);
                                }
                            }
                        }
                        if (uvc_out_info->rotate == 1 || uvc_out_info->rotate == 3) {
                            draw_text_utf8_nv21(mem_rotate->mem_vir,
                                             mem_rotate->mem_vir +
                                                 video_yuv_frame.widht * video_yuv_frame.height,
                                             video_yuv_frame.height, video_yuv_frame.widht, time_str,
                                             16, 16, font, ft_nv21, bk_nv21);
                        } else {
                            draw_text_utf8_nv21(mem_rotate->mem_vir,
                                             mem_rotate->mem_vir +
                                                 video_yuv_frame.widht * video_yuv_frame.height,
                                             video_yuv_frame.widht, video_yuv_frame.height, time_str,
                                             16, 16, font, ft_nv21, bk_nv21);
                        }
                    }
#endif
                } else
#endif
                {
#if OSD_SUPPORT
                    if (dev_config->video_osd) {
                        memset(time_str, 0, sizeof(time_str));
                        char tmp_str[256] = {0};
                        if (1) {
                            memset(tmp_str, 0, sizeof(tmp_str));
                            time_t now = time(0);
                            struct tm *p_tm = gmtime(&now);
                            sprintf(tmp_str, "%4d-%02d-%02d %02d:%02d:%02d", p_tm->tm_year + 1900,
                                    p_tm->tm_mon + 1, p_tm->tm_mday, p_tm->tm_hour, p_tm->tm_min,
                                    p_tm->tm_sec);
                            strcat(time_str, tmp_str);
                        }
                        if (1) {
                            memset(tmp_str, 0, sizeof(tmp_str));
                            sprintf(tmp_str, "\nCpuUsage:%.2lf%%", g_context_info->cpu_usage);
                            strcat(time_str, tmp_str);
                        }
                        if (1) {
                            memset(tmp_str, 0, sizeof(tmp_str));
                            sprintf(tmp_str, "\nMemUsage:%.2lf%%", g_context_info->mem_usage);
                            strcat(time_str, tmp_str);
                        }
                        if (1) {
                            memset(tmp_str, 0, sizeof(tmp_str));
                            sprintf(tmp_str, "\nVi[%d]Fps:%d->%d\n",
                                    g_context_info->rt_media_chns[i].vi_dev,
                                    AWVideoInput_GetFps(g_context_info->rt_media_chns[i].vi_dev),
                                    act_fps[i]);
                            strcat(time_str, tmp_str);
                        }
                        if (1) {
                            door_lock_read_thermal();
                            for (int zone = 0; zone < THERMAL_ZONE_NUM; zone++) {
                                thermal_zone_t *thermal_zone = &g_context_info->thermal_zone[zone];
                                if (strlen(thermal_zone->type)) {
                                    sprintf(tmp_str, "%s:%d ", thermal_zone->type,
                                            (thermal_zone->temp + 500) / 1000);
                                    strcat(time_str, tmp_str);
                                }
                            }
                        }
                        draw_text_utf8_nv21(video_yuv_frame.virAddr[0], video_yuv_frame.virAddr[1],
                                         video_yuv_frame.widht, video_yuv_frame.height, time_str, 16,
                                         16, font, ft_nv21, bk_nv21);
                    }
#endif
                }

                if (dual_transfer == 0) {
                    phy_dst[0] = (uint32_t)buf_tmp->mem_info.mem_phy;
                    phy_dst[1] = (uint32_t)buf_tmp->mem_info.mem_phy;
                    phy_dst[2] = (uint32_t)buf_tmp->mem_info.mem_phy;
                    // G2D_FORMAT_YUV422_PLANAR, G2D_FORMAT_YUV420UVC_V1U1V0U0,
                    // G2D_FORMAT_YUV420UVC_U1V1U0V0, G2D_FORMAT_IYUV422_Y1U0Y0V0,
                    // G2D_FORMAT_IYUV422_Y1V0Y0U0
                    if (uvc_out_info->rotate == 1 || uvc_out_info->rotate == 3) {
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
                            aw_g2d_convert(V4L2_FORMAT_to_G2D_FORMAT(
                                           g_context_info->input_video_v4l2_format),
                                       phy_src, video_yuv_frame.height, video_yuv_frame.widht,
                                       G2D_FORMAT_IYUV422_V0Y1U0Y0, phy_dst,
                                       uvc_out_info->cap_width, uvc_out_info->cap_height);
                        }
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                            phy_dst[1] = (uint32_t)buf_tmp->mem_info.mem_phy +
                                         uvc_out_info->cap_width * uvc_out_info->cap_height;
                            phy_dst[2] = 0;
                            //aw_g2d_copy(V4L2_FORMAT_to_G2D_FORMAT(g_context_info->input_video_v4l2_format), phy_src, video_yuv_frame.height, \
                            video_yuv_frame.widht, phy_dst, uvc_out_info->cap_width, uvc_out_info->cap_height);
                            aw_g2d_convert(V4L2_FORMAT_to_G2D_FORMAT(
                                           g_context_info->input_video_v4l2_format),
                                       phy_src, video_yuv_frame.height, video_yuv_frame.widht,
                                       G2D_FORMAT_YUV420UVC_V1U1V0U0, phy_dst,
                                       uvc_out_info->cap_width, uvc_out_info->cap_height);
                        }
                    } else {
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
                            aw_g2d_convert(V4L2_FORMAT_to_G2D_FORMAT(
                                           g_context_info->input_video_v4l2_format),
                                       phy_src, video_yuv_frame.widht, video_yuv_frame.height,
                                       G2D_FORMAT_IYUV422_V0Y1U0Y0, phy_dst,
                                       uvc_out_info->cap_width, uvc_out_info->cap_height);
                        }
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                            phy_dst[1] = (uint32_t)buf_tmp->mem_info.mem_phy +
                                         uvc_out_info->cap_width * uvc_out_info->cap_height;
                            phy_dst[2] = 0;
                            //aw_g2d_copy(V4L2_FORMAT_to_G2D_FORMAT(g_context_info->input_video_v4l2_format), phy_src, video_yuv_frame.height, \
                                video_yuv_frame.widht, phy_dst, uvc_out_info->cap_width, uvc_out_info->cap_height);
                            aw_g2d_convert(V4L2_FORMAT_to_G2D_FORMAT(
                                           g_context_info->input_video_v4l2_format),
                                       phy_src, video_yuv_frame.widht, video_yuv_frame.height,
                                       G2D_FORMAT_YUV420UVC_V1U1V0U0, phy_dst,
                                       uvc_out_info->cap_width, uvc_out_info->cap_height);
                        }
                    }
                    rt_media_chn_return_yuv_data(&g_context_info->rt_media_chns[i], &video_yuv_frame);
                } else {
                    phy_dst[0] = (uint32_t)convert_mem.mem_phy;
                    phy_dst[1] = (uint32_t)convert_mem.mem_phy;
                    phy_dst[2] = (uint32_t)convert_mem.mem_phy;
                    if (uvc_out_info->rotate == 1 || uvc_out_info->rotate == 3) {
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
                            aw_g2d_convert(V4L2_FORMAT_to_G2D_FORMAT(
                                           g_context_info->input_video_v4l2_format),
                                       phy_src, video_yuv_frame.height, video_yuv_frame.widht,
                                       G2D_FORMAT_IYUV422_V0Y1U0Y0, phy_dst,
                                       uvc_out_info->cap_width / 2,
                                       uvc_out_info->cap_height);  // dual_transfer == 1
                        }
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                            phy_dst[1] = (uint32_t)convert_mem.mem_phy +
                                         uvc_out_info->cap_width * uvc_out_info->cap_height / 2;
                            phy_dst[2] = 0;
                            aw_g2d_convert(V4L2_FORMAT_to_G2D_FORMAT(
                                           g_context_info->input_video_v4l2_format),
                                       phy_src, video_yuv_frame.height, video_yuv_frame.widht,
                                       G2D_FORMAT_YUV420UVC_V1U1V0U0, phy_dst,
                                       uvc_out_info->cap_width / 2,
                                       uvc_out_info->cap_height);  // dual_transfer == 1
                        }
                    } else {
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
                            aw_g2d_convert(V4L2_FORMAT_to_G2D_FORMAT(
                                           g_context_info->input_video_v4l2_format),
                                       phy_src, video_yuv_frame.widht, video_yuv_frame.height,
                                       G2D_FORMAT_IYUV422_V0Y1U0Y0, phy_dst,
                                       uvc_out_info->cap_width,
                                       uvc_out_info->cap_height / 2);  // dual_transfer == 2
                        }
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                            phy_dst[1] = (uint32_t)convert_mem.mem_phy +
                                         uvc_out_info->cap_width * uvc_out_info->cap_height / 2;
                            phy_dst[2] = 0;
                            aw_g2d_convert(V4L2_FORMAT_to_G2D_FORMAT(
                                           g_context_info->input_video_v4l2_format),
                                       phy_src, video_yuv_frame.widht, video_yuv_frame.height,
                                       G2D_FORMAT_YUV420UVC_V1U1V0U0, phy_dst,
                                       uvc_out_info->cap_width,
                                       uvc_out_info->cap_height / 2);  // dual_transfer == 2
                        }
                    }
                    rt_media_chn_return_yuv_data(&g_context_info->rt_media_chns[i], &video_yuv_frame);

                    phy_src[0] = (uint32_t)convert_mem.mem_phy;
                    phy_src[1] = (uint32_t)convert_mem.mem_phy;
                    phy_src[2] = (uint32_t)convert_mem.mem_phy;
                    phy_dst[0] = (uint32_t)buf_tmp->mem_info.mem_phy;
                    phy_dst[1] = (uint32_t)buf_tmp->mem_info.mem_phy;
                    phy_dst[2] = (uint32_t)buf_tmp->mem_info.mem_phy;
                    if (dual_transfer == 1) {
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
                            aw_g2d_paste(G2D_FORMAT_IYUV422_V0Y1U0Y0, phy_src,
                                     uvc_out_info->cap_width / 2, uvc_out_info->cap_height, phy_dst,
                                     uvc_out_info->cap_width, uvc_out_info->cap_height,
                                     uvc_out_info->cap_width / 2 * i, 0);
                        }
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                            phy_src[1] = (uint32_t)convert_mem.mem_phy +
                                         uvc_out_info->cap_width * uvc_out_info->cap_height / 2;
                            phy_src[2] = 0;
                            phy_dst[1] = (uint32_t)buf_tmp->mem_info.mem_phy +
                                         uvc_out_info->cap_width * uvc_out_info->cap_height;
                            phy_dst[2] = 0;
                            aw_g2d_paste(G2D_FORMAT_YUV420UVC_V1U1V0U0, phy_src,
                                     uvc_out_info->cap_width / 2, uvc_out_info->cap_height, phy_dst,
                                     uvc_out_info->cap_width, uvc_out_info->cap_height,
                                     uvc_out_info->cap_width / 2 * i, 0);
                        }
                    } else if (dual_transfer == 2) {
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV) {
                            aw_g2d_paste(G2D_FORMAT_IYUV422_V0Y1U0Y0, phy_src, uvc_out_info->cap_width,
                                     uvc_out_info->cap_height / 2, phy_dst, uvc_out_info->cap_width,
                                     uvc_out_info->cap_height, 0, uvc_out_info->cap_height / 2 * i);
                        }
                        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                            phy_src[1] = (uint32_t)convert_mem.mem_phy +
                                         uvc_out_info->cap_width * uvc_out_info->cap_height / 2;
                            phy_src[2] = 0;
                            phy_dst[1] = (uint32_t)buf_tmp->mem_info.mem_phy +
                                         uvc_out_info->cap_width * uvc_out_info->cap_height;
                            phy_dst[2] = 0;
                            aw_g2d_paste(G2D_FORMAT_YUV420UVC_V1U1V0U0, phy_src,
                                     uvc_out_info->cap_width, uvc_out_info->cap_height / 2, phy_dst,
                                     uvc_out_info->cap_width, uvc_out_info->cap_height, 0,
                                     uvc_out_info->cap_height / 2 * i);
                        }
                    }
                    got_frame[i] = 1;
                }

                if (dual_transfer == 0 || (dual_transfer && got_frame[0] && got_frame[1])) {
                    got_frame[0] = 0;
                    got_frame[1] = 0;
                    buf_tmp->cur_mem_size = uvc_out_info->cap_width * uvc_out_info->cap_height * 2;
                    if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                        buf_tmp->cur_mem_size =
                            uvc_out_info->cap_width * uvc_out_info->cap_height * 3 / 2;
                    }
                    if (frm_manager->first_idle_to_using_frame)
                        frm_manager->first_idle_to_using_frame(frm_manager, buf_tmp);
                }
            }
            usleep(10 * 1000);
        }
        usleep(100 * 1000);
    }
    if (dual_transfer) {
        aw_mem_free(&convert_mem);
    }

    DOORLOCK_DBG("exit <===\n");
}

static void uvc_rtmedia_exit_callback0(int channel)
{
    rt_media_chn_t *rt_media_chn = &g_context_info->rt_media_chns[0];
    DOORLOCK_WARN("[%d] rtmedia chn exit callback, chn_state = %d\n", rt_media_chn->chn,
                 rt_media_chn->chn_state);
}
static void uvc_rtmedia_exit_callback1(int channel)
{
    rt_media_chn_t *rt_media_chn = &g_context_info->rt_media_chns[1];
    DOORLOCK_WARN("[%d] rtmedia chn exit callback, chn_state = %d\n", rt_media_chn->chn,
                 rt_media_chn->chn_state);
}

static void uvc_rtmedia_stream_callback0(const AWVideoInput_StreamInfo *stream_info)
{
    int keyframe = stream_info->keyframe_flag;
    int len = stream_info->size0 + stream_info->size1 + stream_info->size2;
    uint8_t *data = NULL;
    // DOORLOCK_DBG("enc data len = 0x%x, keyframe = %d, header len = %d\n", len, keyframe,
    // stream_info->sps_pps_size);

    static uint32_t frame_cnt = 0;
    int uvc_index = g_context_info->uvc_vi_dev % 2;
    if (uvc_index == 1) {
        return;
    }

    if ((active_cnt[0] % FPS_ACTIVE_CNT) == 0) {
        cur_timestamp_us[0] = get_cur_time_us();
        if (last_timestamp_us[0])
            act_fps[0] =
                1000 * 1000 / ((cur_timestamp_us[0] - last_timestamp_us[0]) / FPS_ACTIVE_CNT);

        last_timestamp_us[0] = cur_timestamp_us[0];
    }
    active_cnt[0]++;

    uvc_out_t *uvc_out_info = &g_context_info->uvc_out_info;
    if (uvc_out_info->format_v4l2 != V4L2_PIX_FMT_YUYV &&
        uvc_out_info->format_v4l2 != V4L2_PIX_FMT_NV12) {
        frm_manager_t *frm_manager = &uvc_out_info->frm_manager;
        frame_mem_t *buf_tmp = NULL;
        int ret = 0;
        if (frm_manager->prefetch_first_idle_frame)
            frm_manager->prefetch_first_idle_frame(frm_manager, &buf_tmp);
        if (buf_tmp == NULL || ret < 0) {
            DOORLOCK_DBG("frm_manager->prefetch_first_idle_frame failed\n");
            // usleep(10*1000);
            return;
        }
        if (stream_info->sps_pps_size > 0 && stream_info->sps_pps_buf &&
            stream_info->keyframe_flag && uvc_out_info->format_v4l2 == V4L2_PIX_FMT_H264) {
            len += stream_info->sps_pps_size;
        }
        if (len > uvc_out_info->max_frame_size) {
            DOORLOCK_ERR(
                "video data too big %d, max_frame_size = %d. size0 %d, size1 %d, size2 %d, "
                "sps_pps_size %d\n",
                len, uvc_out_info->max_frame_size, stream_info->size0, stream_info->size1,
                stream_info->size2, stream_info->sps_pps_size);
            return;
        }

        int data_len = 0;
        if (stream_info->sps_pps_size > 0 && stream_info->sps_pps_buf &&
            stream_info->keyframe_flag && uvc_out_info->format_v4l2 == V4L2_PIX_FMT_H264) {
            memcpy(buf_tmp->mem_info.mem_vir + data_len, stream_info->sps_pps_buf,
                   stream_info->sps_pps_size);
            data_len += stream_info->sps_pps_size;
        }

        data = stream_info->data0;
        memcpy(buf_tmp->mem_info.mem_vir + data_len, data, stream_info->size0);
        data_len += stream_info->size0;

        data = stream_info->data1;
        memcpy(buf_tmp->mem_info.mem_vir + data_len, data, stream_info->size1);
        data_len += stream_info->size1;

        data = stream_info->data2;
        memcpy(buf_tmp->mem_info.mem_vir + data_len, data, stream_info->size2);
        data_len += stream_info->size2;

        buf_tmp->cur_mem_size = data_len;

        if (frm_manager->first_idle_to_using_frame) {
            if (frame_cnt == 0) {
                DOORLOCK_INFO("[0] send to uvc first frame %lld us\n", get_cur_time_us());
            }
            frm_manager->first_idle_to_using_frame(frm_manager, buf_tmp);
        }
    }
    frame_cnt++;
}

static void uvc_rtmedia_stream_callback1(const AWVideoInput_StreamInfo *stream_info)
{
    int keyframe = stream_info->keyframe_flag;
    int len = stream_info->size0 + stream_info->size1 + stream_info->size2;
    uint8_t *data = NULL;
    // DOORLOCK_DBG("enc data len = 0x%x, keyframe = %d, header len = %d\n", len, keyframe,
    // stream_info->sps_pps_size);
    static uint32_t frame_cnt = 0;
    int uvc_index = g_context_info->uvc_vi_dev % 2;

    if (uvc_index == 0) {
        return;
    }

    if ((active_cnt[1] % FPS_ACTIVE_CNT) == 0) {
        cur_timestamp_us[1] = get_cur_time_us();
        if (last_timestamp_us[1])
            act_fps[1] =
                1000 * 1000 / ((cur_timestamp_us[1] - last_timestamp_us[1]) / FPS_ACTIVE_CNT);

        last_timestamp_us[1] = cur_timestamp_us[1];
    }
    active_cnt[1]++;

    uvc_out_t *uvc_out_info = &g_context_info->uvc_out_info;
    if (uvc_out_info->format_v4l2 != V4L2_PIX_FMT_YUYV &&
        uvc_out_info->format_v4l2 != V4L2_PIX_FMT_NV12) {
        frm_manager_t *frm_manager = &uvc_out_info->frm_manager;
        frame_mem_t *buf_tmp = NULL;
        int ret = 0;
        if (frm_manager->prefetch_first_idle_frame)
            frm_manager->prefetch_first_idle_frame(frm_manager, &buf_tmp);
        if (buf_tmp == NULL || ret < 0) {
            DOORLOCK_DBG("prefetch_first_idle_frame failed\n");
            // usleep(10*1000);
            return;
        }

        if (stream_info->sps_pps_size > 0 && stream_info->sps_pps_buf &&
            stream_info->keyframe_flag) {
            len += stream_info->sps_pps_size;
        }
        if (len > uvc_out_info->max_frame_size) {
            DOORLOCK_ERR(
                "video data too big %d, max_frame_size = %d. size0 %d, size1 %d, size2 %d, "
                "sps_pps_size %d\n",
                len, uvc_out_info->max_frame_size, stream_info->size0, stream_info->size1,
                stream_info->size2, stream_info->sps_pps_size);
            return;
        }
        int data_len = 0;
        if (stream_info->sps_pps_size > 0 && stream_info->sps_pps_buf &&
            stream_info->keyframe_flag) {
            memcpy(buf_tmp->mem_info.mem_vir + data_len, stream_info->sps_pps_buf,
                   stream_info->sps_pps_size);
            data_len += stream_info->sps_pps_size;
        }
        data = stream_info->data0;
        memcpy(buf_tmp->mem_info.mem_vir + data_len, data, stream_info->size0);
        data_len += stream_info->size0;

        data = stream_info->data1;
        memcpy(buf_tmp->mem_info.mem_vir + data_len, data, stream_info->size1);
        data_len += stream_info->size1;

        data = stream_info->data2;
        memcpy(buf_tmp->mem_info.mem_vir + data_len, data, stream_info->size2);
        data_len += stream_info->size2;

        buf_tmp->cur_mem_size = data_len;

        if (frm_manager->first_idle_to_using_frame) {
            if (frame_cnt == 0) {
                DOORLOCK_DBG("[1] send to uvc first frame %lld us\n", get_cur_time_us());
            }
            frm_manager->first_idle_to_using_frame(frm_manager, buf_tmp);
        }
    }
    frame_cnt++;
}

void uvc_timer(int sig)
{
    static int capture_flag = 1;
    static cpu_occupy_t cpu_occupy_last = {0};
    cpu_occupy_t cpu_occupy_cur;

    doorlock_get_cpu_occupy_info(&cpu_occupy_cur);
    g_context_info->cpu_usage = doorlock_calc_cpu_occupy_info(&cpu_occupy_last, &cpu_occupy_cur);
    memcpy(&cpu_occupy_last, &cpu_occupy_cur, sizeof(cpu_occupy_t));

    mem_occupy_t mem_info;
    doorlock_get_mem_occupy_info(&mem_info);
    g_context_info->mem_usage = mem_info.usage;

    uvc_out_t *uvc_out_info = &g_context_info->uvc_out_info;
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;
    if (SIGALRM == sig && (uvc_out_info->is_resource_ok || uvc_out_info->is_streaming)) {
        DOORLOCK_DBG("timer\n");
        int uvc_index = g_context_info->uvc_vi_dev % 2;

#if 0
		if(capture_flag == 1 && (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV || uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12)){
			uint32_t jpegWidth = g_context_info->uvc_out_info.cap_width;
			uint32_t jpegHeight = g_context_info->uvc_out_info.cap_height;
			uint32_t max_data_len = jpegWidth*jpegHeight;
			uint8_t *data = (uint8_t *)malloc(max_data_len);
			if(data){
				int jpegLen = 0;
				if(g_context_info->uvc_out_info.rotate == 1 || g_context_info->uvc_out_info.rotate == 3){
					jpegLen = rt_media_chn_get_jpeg_data(&g_context_info->rt_media_chns[uvc_index], data, max_data_len, jpegWidth, jpegHeight, 99, g_context_info->uvc_out_info.rotate*90);
				}else{
					jpegLen = rt_media_chn_get_jpeg_data(&g_context_info->rt_media_chns[uvc_index], data, max_data_len, jpegHeight, jpegWidth, 99, g_context_info->uvc_out_info.rotate*90);
				}

				if(jpegLen != 0){
					FILE *dump_file = fopen("/tmp/capture.jpg", "wb");
					if(dump_file){
						fwrite(data, 1, jpegLen, dump_file);
						fclose(dump_file);
					}else{
						DOORLOCK_ERR("file open [capture.jpg] failed\n");
					}
				}
				free(data);
			}else{
				DOORLOCK_ERR("malloc %d failed\n", max_data_len);
			}
			//capture_flag = 1;
		}
#endif

#if OSD_SUPPORT
        if (uvc_out_info->format_v4l2 != V4L2_PIX_FMT_YUYV &&
            uvc_out_info->format_v4l2 != V4L2_PIX_FMT_NV12 && dev_config->video_osd) {
            rt_media_osd_chn_t *rt_media_osd_chn_info = &g_context_info->rt_media_osd_chns[uvc_index];
            memset(rt_media_osd_chn_info->argb_data, 0,
                   rt_media_osd_chn_info->width * rt_media_osd_chn_info->height * 4);
            // memset(rt_media_osd_chn_info->argb_data, 0, rt_media_osd_chn_info->width*96*4);
            if (dev_config->video_osd) {
                rt_media_osd_chn_info->show = 1;
                font_info_t font_info;
                font_info_t *font = &font_info;
                draw_font_init(font, 32, "/usr/bin/");
                char time_str[256] = {0};
                char tmp_str[256] = {0};
                if (1) {
                    memset(tmp_str, 0, sizeof(tmp_str));
                    time_t now = time(0);
                    struct tm *p_tm = gmtime(&now);
                    sprintf(tmp_str, "%4d-%02d-%02d %02d:%02d:%02d", p_tm->tm_year + 1900,
                            p_tm->tm_mon + 1, p_tm->tm_mday, p_tm->tm_hour, p_tm->tm_min,
                            p_tm->tm_sec);
                    strcat(time_str, tmp_str);
                }
                if (1) {
                    memset(tmp_str, 0, sizeof(tmp_str));
                    sprintf(tmp_str, "\nCpuUsage:%.2lf%%", g_context_info->cpu_usage);
                    strcat(time_str, tmp_str);
                }
                if (1) {
                    memset(tmp_str, 0, sizeof(tmp_str));
                    sprintf(tmp_str, "\nMemUsage:%.2lf%%", g_context_info->mem_usage);
                    strcat(time_str, tmp_str);
                }
                if (1) {
                    memset(tmp_str, 0, sizeof(tmp_str));
                    sprintf(tmp_str, "\nVi[%d]Fps:%d->%d\n", g_context_info->uvc_vi_dev,
                            AWVideoInput_GetFps(g_context_info->uvc_vi_dev), act_fps[uvc_index]);
                    strcat(time_str, tmp_str);
                }
                if (1) {
                    door_lock_read_thermal();
                    for (int zone = 0; zone < THERMAL_ZONE_NUM; zone++) {
                        thermal_zone_t *thermal_zone = &g_context_info->thermal_zone[zone];
                        if (strlen(thermal_zone->type)) {
                            sprintf(tmp_str, "%s:%d ", thermal_zone->type,
                                    (thermal_zone->temp + 500) / 1000);
                            strcat(time_str, tmp_str);
                        }
                    }
                }
                draw_text_utf8(rt_media_osd_chn_info->argb_data, 4, rt_media_osd_chn_info->width,
                             rt_media_osd_chn_info->height, time_str, 0, 0, font,
                             RGBA(255, 255, 255, 255), RGBA(0, 0, 0, 0));
            } else {
                rt_media_osd_chn_info->show = 0;
            }
            rt_media_osd_chn_update(rt_media_osd_chn_info);
#if 0
            FILE *dump_file = fopen("./argb.raw", "wb");
            if (dump_file) {
                fwrite(rt_media_osd_chn_info->argb_data, 1, rt_media_osd_chn_info->width*rt_media_osd_chn_info->height*4, dump_file);
                fclose(dump_file);
            } else {
                DOORLOCK_ERR("open file failed\n");
            }
#endif
        }
#endif
    }
    if (uvc_out_info->is_resource_ok || uvc_out_info->is_streaming) {
        alarm(1);  // continue set the timer
    }
}

static int start_rtmedia_uvc_process(uvc_out_t *uvc_out_info)
{
    DOORLOCK_INFO("format_v4l2 = 0x%x, rotate = %d\n", uvc_out_info->format_v4l2,
                 uvc_out_info->rotate);
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;
#if UVC_ROTATE
    if (uvc_out_info->rotate) {
        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV ||
            uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
            aw_mem_info_t *rotate_mem = &g_context_info->rotate_mem_info;
            rotate_mem->mem_size =
                ALIGN_16B(uvc_out_info->cap_width) * ALIGN_16B(uvc_out_info->cap_height) * 2;
            rotate_mem->mem_cache = 0;
            aw_mem_malloc(rotate_mem);
        }
    }
#endif
    for (int i = 0; i < 2; i++) {
        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_H264) {
            g_context_info->rt_media_chns[i].v4l2_format_type = V4L2_PIX_FMT_H264;
        } else if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_MJPEG) {
            g_context_info->rt_media_chns[i].v4l2_format_type = V4L2_PIX_FMT_MJPEG;
        } else if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_JPEG) {
            g_context_info->rt_media_chns[i].v4l2_format_type = V4L2_PIX_FMT_JPEG;
        } else if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV ||
                   uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
            g_context_info->rt_media_chns[i].v4l2_format_type =
                g_context_info->input_video_v4l2_format;
        }

        int dual_transfer = 0;  // 0:no dual, 1:left,right, 2:up,down
        int tmp_width = uvc_out_info->cap_width;
        int tmp_height = uvc_out_info->cap_height;

        if (uvc_out_info->rotate == 1 || uvc_out_info->rotate == 3) {
            tmp_width = uvc_out_info->cap_height;
            tmp_height = uvc_out_info->cap_width;
            if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV ||
                uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_YVU420SP;  // for rotate
            } else {
#if VENC_INPUT_YUV422
                g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_YVU420SP;  // for rotate
#else
                g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_YVU420SP;  // for rotate
#endif
            }
#if (SENSOR_WIDTH > SENSOR_HEIGHT)
            if (uvc_out_info->cap_width > uvc_out_info->cap_height) {
                dual_transfer = 1;
                tmp_width = uvc_out_info->cap_height;
                tmp_height = uvc_out_info->cap_width / 2;
            }
#endif
        } else {
            if (uvc_out_info->rotate == 0) {
                if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV ||
                    uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                    // for rotate
                    g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_YVU420SP;
                } else {
#if VENC_INPUT_YUV422
                    g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_YUV422SP;
#else
                    g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_LBC_25X;
                    // for rotate
                    //g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_YVU420SP;
#endif
                }
            } else {
                if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV ||
                    uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
                    // for rotate
                    g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_YVU420SP;
                } else {
#if VENC_INPUT_YUV422
                    // for rotate
                    g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_YUV422SP;
#else
                    //g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_YVU420SP;  // for rotate
					g_context_info->rt_media_chns[i].input_rt_format = RT_PIXEL_LBC_25X;
#endif
                }
            }
#if (SENSOR_WIDTH > SENSOR_HEIGHT)
            if (uvc_out_info->cap_width < uvc_out_info->cap_height) {
                dual_transfer = 2;
                tmp_height = uvc_out_info->cap_height / 2;
            }
#endif
        }

        g_context_info->uvc_dual_transfer = dual_transfer;
        DOORLOCK_INFO("dual_transfer = %d\n", dual_transfer);

#if !UVC_DYNAMIC_SWITCH_SUPPORT
        if (i != (dev_config->video_index % 2) && !dual_transfer) {
            continue;
        }
#endif

        // if(uvc_out_info->format_v4l2 != V4L2_PIX_FMT_YUYV && uvc_out_info->format_v4l2 !=
        // V4L2_PIX_FMT_NV12)
        {
            g_context_info->rt_media_chns[i].vi_dev = UVC_VIDEO_BASE + i;
            g_context_info->rt_media_chns[i].src_width = tmp_width;
            g_context_info->rt_media_chns[i].src_height = tmp_height;
            g_context_info->rt_media_chns[i].dst_width = tmp_width;
            g_context_info->rt_media_chns[i].dst_height = tmp_height;
            g_context_info->rt_media_chns[i].src_fps = uvc_out_info->frame_rate;
            g_context_info->rt_media_chns[i].dst_fps = uvc_out_info->frame_rate;
            g_context_info->rt_media_chns[i].bitrate = tmp_width * tmp_height * 2 * 8 *
                                                        uvc_out_info->frame_rate / 20 /
                                                        1024;   // for mjpeg, in Kb
            g_context_info->rt_media_chns[i].enc_quality = 99;  // for jpg
            // g_context_info->rt_media_chns[i].mEncUseProfile = 1;
            // g_context_info->rt_media_chns[i].rc_mode = 0;//0:CBR, 1:VBR, 2:FIXQP, 3:QPMAP
            g_context_info->rt_media_chns[i].chn = UVC_VIDEO_BASE + i;
            g_context_info->rt_media_chns[i].thread_attr = NULL;
            if (i == 0) {
                g_context_info->rt_media_chns[i].stream_callback = uvc_rtmedia_stream_callback0;
                g_context_info->rt_media_chns[i].exit_callback = uvc_rtmedia_exit_callback0;
            } else {
                g_context_info->rt_media_chns[i].stream_callback = uvc_rtmedia_stream_callback1;
                g_context_info->rt_media_chns[i].exit_callback = uvc_rtmedia_exit_callback1;
            }
        }
        g_context_info->rt_media_chns[i].drop_frame_num = 2;  // uvc drop first n frames
        memset(&g_context_info->rt_media_chns[i].chn_attr, 0, sizeof(VideoInputConfig));

        rt_media_chn_init(&g_context_info->rt_media_chns[i]);
        if (uvc_out_info->rotate == 1 || uvc_out_info->rotate == 3) {
            AWVideoInput_SetRotate(g_context_info->rt_media_chns[i].vi_dev,
                                   uvc_out_info->rotate * 90);
        } else if (uvc_out_info->rotate == 2) {
            AWVideoInput_SetRotate(g_context_info->rt_media_chns[i].vi_dev,
                                   uvc_out_info->rotate * 90);
            // flip + mirror ??
        } else {
            AWVideoInput_SetRotate(g_context_info->rt_media_chns[i].vi_dev, 0);
        }

        rt_media_chn_start(&g_context_info->rt_media_chns[i]);
        last_timestamp_us[i] = get_cur_time_us();
        active_cnt[i] = 0;

#if (SENSOR_CONFIG_BY_FLASH == 0)
        set_video_flip(g_context_info->rt_media_chns[i].vi_dev, dev_config->flip_ctl,
                       uvc_out_info->rotate);

        int ir_flag = 0;
        if (dev_config->video_ir_index < 2) {
            if (((g_context_info->rt_media_chns[i].vi_dev % 2) ==
                 (dev_config->video_ir_index % 2)) ||
                dev_config->video_mode) {
                ir_flag = 1;
            } else {
                ir_flag = 0;
            }
        } else if (dev_config->video_ir_index == 2) {
            ir_flag = 1;
        } else if (dev_config->video_ir_index == 4) {
            ir_flag = 255;  // default
        } else {
            ir_flag = 0;
        }
        if (dev_config->video_ir_index != 4) {
            rt_media_chn_set_ir_mode(g_context_info->rt_media_chns[i].vi_dev, 0);
            if (ir_flag) {
                rt_media_chn_set_ir_mode(g_context_info->rt_media_chns[i].vi_dev, ir_flag);
            }
        }
        // DOORLOCK_INFO("RtMediaChn vi_dev = %d, ir_flag = %d\n",
        // g_context_info->rt_media_chns[i].vi_dev, ir_flag);
        DOORLOCK_INFO("vi_dev = %d, video_ir_index = %d, ir_flag = %d, video_mode = %d\n",
                     g_context_info->rt_media_chns[i].vi_dev, dev_config->video_ir_index, ir_flag,
                     dev_config->video_mode);
#endif

#if OSD_SUPPORT
        if (uvc_out_info->format_v4l2 != V4L2_PIX_FMT_YUYV &&
            uvc_out_info->format_v4l2 != V4L2_PIX_FMT_NV12 && dev_config->video_osd) {
            g_context_info->rt_media_osds[i].vi_dev = UVC_VIDEO_BASE + i;
            rt_media_osd_init(&g_context_info->rt_media_osds[i]);
            rt_media_osd_chn_t *rt_media_osd_chn_info = &g_context_info->rt_media_osd_chns[i];
            memset(rt_media_osd_chn_info, 0, sizeof(rt_media_osd_chn_t));
            rt_media_osd_chn_info->media_osd = &g_context_info->rt_media_osds[i];
            rt_media_osd_chn_info->chn = 0;
            if (dev_config->video_osd) {
                rt_media_osd_chn_info->show = 1;
            } else {
                rt_media_osd_chn_info->show = 0;
            }
            rt_media_osd_chn_info->pos_x = 16;
            rt_media_osd_chn_info->pos_y = 16;
            rt_media_osd_chn_info->width = ALIGN_16B(360);
            rt_media_osd_chn_info->height = ALIGN_16B(360);
#if 0
			rt_media_osd_chn_info->osd_type = 1;
			//COLOR_RGB_TO_YUV(255, 0, 0, &rt_media_osd_chn_info->color_y, &rt_media_osd_chn_info->color_u, &rt_media_osd_chn_info->color_v);
			rt_media_osd_chn_info->color_rgb = 0xff0000;
			rt_media_osd_chn_init(rt_media_osd_chn_info);
#else
            rt_media_osd_chn_info->osd_type = 0;
            rt_media_osd_chn_info->overlay_argb_type = OVERLAY_ARGB8888;
            rt_media_osd_chn_info->argb_data =
                malloc(rt_media_osd_chn_info->width * rt_media_osd_chn_info->height * 4);
            memset(rt_media_osd_chn_info->argb_data, 0,
                   rt_media_osd_chn_info->width * rt_media_osd_chn_info->height * 4);
            //draw_rect(rt_media_osd_chn_info->argb_data, 4, rt_media_osd_chn_info->width, rt_media_osd_chn_info->height, \
                0, 0, rt_media_osd_chn_info->width, rt_media_osd_chn_info->height, 0xffffffff/*ABGR(255, 255, 255, 255)*/, 2);

            rt_media_osd_chn_init(rt_media_osd_chn_info);
#endif
        }
#endif

#if ORL_SUPPORT
        if (orl_enable) {
            rt_media_orl_t *rt_media_orl_info = &g_context_info->rt_media_orl_infos[i];
            rt_media_orl_info->vi_dev = UVC_VIDEO_BASE + i;
            rt_media_orl_info->line_width = 1;
            rt_media_orl_info->num = 1;
            rt_media_orl_info->pos_x[0] = tmp_width / 4;
            rt_media_orl_info->pos_y[0] = tmp_height / 4;
            rt_media_orl_info->width[0] = tmp_width / 2;
            rt_media_orl_info->height[0] = tmp_height / 2;
            rt_media_orl_info->color_rgb[0] = 0xff0000;
            rt_media_orl_init(rt_media_orl_info);
        }
#endif
    }

    uvc_out_info->is_resource_ok = 1;

    if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV ||
        uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
        pthread_create(&g_context_info->uvc_yuv_thread, NULL, get_yuv_frame_thread,
                       (void *)g_context_info);
        pthread_setname_np(g_context_info->uvc_yuv_thread, "get_yuv_frame");
    }
    signal(SIGALRM, uvc_timer);
    uvc_timer(SIGALRM);

    return 0;
}

static int stop_rtmedia_uvc_process(uvc_out_t *uvc_out_info)
{
    DOORLOCK_INFO("------\n");
    uvc_out_info->is_resource_ok = 0;
    alarm(0);

    door_lock_save_ae(0);
    door_lock_save_ae(1);

    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;
    if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV ||
        uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
        pthread_join(g_context_info->uvc_yuv_thread, NULL);
    }
    for (int i = 0; i < 2; i++) {
#if !UVC_DYNAMIC_SWITCH_SUPPORT
        int dual_transfer = g_context_info->uvc_dual_transfer;
        if (i != (dev_config->video_index % 2) && !dual_transfer) {
            continue;
        }
#endif
        rt_media_chn_stop(&g_context_info->rt_media_chns[i]);
#if ORL_SUPPORT
        if (orl_enable) {
            rt_media_orl_deinit(&g_context_info->rt_media_orl_infos[i]);
        }
#endif

#if OSD_SUPPORT
        if (uvc_out_info->format_v4l2 != V4L2_PIX_FMT_YUYV &&
            uvc_out_info->format_v4l2 != V4L2_PIX_FMT_NV12 && dev_config->video_osd) {
            rt_media_osd_chn_deinit(&g_context_info->rt_media_osd_chns[i]);
            rt_media_osd_deinit(&g_context_info->rt_media_osds[i]);
            rt_media_osd_chn_t *rt_media_osd_chn_info = &g_context_info->rt_media_osd_chns[i];
            if (rt_media_osd_chn_info->argb_data) {
                free(rt_media_osd_chn_info->argb_data);
                rt_media_osd_chn_info->argb_data = NULL;
            }
        }
#endif

        rt_media_chn_deinit(&g_context_info->rt_media_chns[i]);
    }
#if UVC_ROTATE
    if (uvc_out_info->rotate) {
        if (uvc_out_info->format_v4l2 == V4L2_PIX_FMT_YUYV ||
            uvc_out_info->format_v4l2 == V4L2_PIX_FMT_NV12) {
            aw_mem_info_t *rotate_mem = &g_context_info->rotate_mem_info;
            aw_mem_free(rotate_mem);
        }
    }
#endif
    return 0;
}
#endif

#if UAC_SUPPORT
void *snd_card_mic_thread(void *arg)
{
    DOORLOCK_DBG("enter ===>\n");
    int data_size = g_context_info->uac_dev_info.max_mic_frame_size;
    uint8_t *mic_buffer = (uint8_t *)malloc(data_size);
    if (mic_buffer == NULL) {
        DOORLOCK_ERR("malloc %d failed\n", data_size);
        return NULL;
    }

    uac_dev_t *uac_dev_info = &g_context_info->uac_dev_info;
    static int transfer_len = 0;
    char file_name[256];
    memset(file_name, 0, sizeof(file_name));
    if (g_store_dir) {
        sprintf(file_name, "%s/mic_%d-%d-%d.pcm", g_store_dir,
        g_audio_bit_width, g_audio_ch_cnt, g_audio_sample_rate);
    }
    while (uac_dev_info->mic_is_resource_ok) {
        if (uac_dev_info->mic_is_streaming == 0) {
            usleep(10*1000);
            continue;
        }
        int cnt = data_size;

#if SNDCARD_SUPPORT
        cnt = snd_card_read_mic_frame(&g_context_info->snd_card_info, mic_buffer, data_size);
#endif
        if (g_store_dir) {
            FILE *dump_file = fopen(file_name, "rb");
            if (dump_file) {
                fseek(dump_file, transfer_len, SEEK_SET);
                fread(mic_buffer, 1, cnt, dump_file);
                transfer_len += cnt;
                if (feof(dump_file)) {
                    DOORLOCK_INFO("mic file ends, loop again\n");
                    transfer_len = 0;
                }
            fclose(dump_file);
            } else {
                DOORLOCK_ERR("fopen %s failed\n", file_name);
            }
        }

        frm_manager_t *frm_manager = &uac_dev_info->mic_frm_manager;
        frame_mem_t *buf_tmp = NULL;
        int ret = 0;
        if (frm_manager->prefetch_first_idle_frame)
            ret = frm_manager->prefetch_first_idle_frame(frm_manager, &buf_tmp);
        if (buf_tmp == NULL || ret < 0) {
            usleep(2*1000);
            continue;
        }
        if (cnt <= 0) {
            DOORLOCK_ERR("Capture mic data size %d is not right\n", cnt);
            usleep(2*1000);
            continue;
        }
        if (cnt <= buf_tmp->mem_info.mem_size) {
            memcpy(buf_tmp->mem_info.mem_vir, mic_buffer, cnt);
            buf_tmp->cur_mem_size = cnt;
        } else {
            DOORLOCK_ERR("Capture mic data size %d is bigger than mem size %d\n", cnt, buf_tmp->mem_info.mem_size);
            usleep(2*1000);
            continue;
        }
        if (frm_manager->first_idle_to_using_frame)
            frm_manager->first_idle_to_using_frame(frm_manager, buf_tmp);
            usleep(2*1000);
    }

    if (mic_buffer) {
        free(mic_buffer);
    }
    DOORLOCK_DBG("exit <===\n");
}

void *snd_card_spk_thread(void *arg)
{
    DOORLOCK_DBG("enter ===>\n");
    int data_size = g_context_info->uac_dev_info.max_mic_frame_size;
    uint8_t *spk_buffer = NULL;

    uac_dev_t *uac_dev_info = &g_context_info->uac_dev_info;

    char file_name[256];
    memset(file_name, 0, sizeof(file_name));
    if (g_store_dir) {
        sprintf(file_name, "%s/spk_%d-%d-%d.pcm", g_store_dir,
                    g_audio_bit_width, g_audio_ch_cnt, g_audio_sample_rate);
    }
    while (uac_dev_info->spk_is_resource_ok) {
        if (uac_dev_info->spk_is_streaming == 0) {
            usleep(10*1000);
            continue;
        }
        frm_manager_t *frm_manager = &uac_dev_info->spk_frm_manager;
        frame_mem_t *buf_tmp = NULL;
        int ret = 0;
        if (frm_manager->prefetch_first_using_frame)
            ret = frm_manager->prefetch_first_using_frame(frm_manager, &buf_tmp);
        if (buf_tmp == NULL || ret < 0) {
            usleep(2*1000);
            continue;
        }

        spk_buffer = buf_tmp->mem_info.mem_vir;
        data_size = buf_tmp->cur_mem_size;

        if (g_store_dir) {
            FILE *dump_file = fopen(file_name, "ab");
            if (dump_file) {
                fwrite(spk_buffer, 1, data_size, dump_file);
                fclose(dump_file);
            } else {
                DOORLOCK_ERR("fopen %s failed\n", file_name);
            }
        } else {
#if SNDCARD_SUPPORT
        int cnt = snd_card_write_spk_frame(&g_context_info->snd_card_info, spk_buffer, data_size);
#endif
        }
        frm_manager->first_using_to_idle_frame(frm_manager, buf_tmp);
        usleep(2*1000);
    }
    //if (spk_buffer) {
        free(spk_buffer);
    //}
    DOORLOCK_DBG("exit <===\n");
}
#endif


#if UVC_SUPPORT
static int uvc_resource_on_off(uvc_out_t *uvc_out_info, bool on_off)
{
    int ret = 0;
    // DOORLOCK_DBG("uvc_resource_on_off[%d] %lld us\n", on_off, get_cur_time_us());
    DOORLOCK_DBG("enter ===>[%d]\n", on_off);
    if (on_off) {
        if (uvc_out_info->is_resource_ok == 1) {
            DOORLOCK_WARN("already on!!!\n");
            ret = 1;
            goto exit;
        }

        ret = start_rtmedia_uvc_process(uvc_out_info);
        if (ret < 0) {
            DOORLOCK_ERR("start_rtmedia_uvc_process failed!!\n");
            goto exit;
        }
        uvc_out_info->is_resource_ok = 1;
    } else {
        if (uvc_out_info->is_resource_ok == 0) {
            DOORLOCK_WARN("already off!!!\n");
            ret = 1;
            goto exit;
        }

        // stop and destroy venc
        stop_rtmedia_uvc_process(uvc_out_info);
        uvc_out_info->is_resource_ok = 0;
    }
exit:
    DOORLOCK_DBG("exit <===[%d]\n", on_off);
    return ret;
}
#endif

#if UAC_SUPPORT
#if MPPAIO_SUPPORT
static void uac_mpp_ai_user_callback(mpp_ai_chn_s *mpp_ai_chn_info, AUDIO_FRAME_S *frame_info)
{
    static unsigned int frame_cnt = 0;
    uac_dev_t *uac_dev_info = &g_context_info->uac_dev_info;
    mpp_ai_s *pMppAiInfo = mpp_ai_chn_info->mpp_ai;
    mpp_ai_chn_info->pcm_size += frame_info->mLen;

    int ret = 0;
    frm_manager_t *frm_manager = &uac_dev_info->mic_frm_manager;
    frame_mem_t *buf_tmp = NULL;
    if (frm_manager->prefetch_first_idle_frame)
        ret = frm_manager->prefetch_first_idle_frame(frm_manager, &buf_tmp);
    if (buf_tmp == NULL || ret < 0) {
        // DOORLOCK_DBG("prefetch_first_idle_frame fail\n");
        return;
    }

    if (g_store_dir) {
        static int transfer_len = 0;
        char ai_file_name[256];
        memset(ai_file_name, 0, sizeof(ai_file_name));
        sprintf(ai_file_name, "%s/mic_%d-%d-%d-%d.pcm", g_store_dir,
                pMppAiInfo->aio_config.channel_cnt, pMppAiInfo->aio_config.bit_width,
                pMppAiInfo->aio_config.sample_rate, pMppAiInfo->ai_volume);
        FILE *fd = fopen(ai_file_name, "rb");
        if (fd != NULL) {
            fseek(fd, transfer_len, SEEK_SET);
            fread(frame_info->mpAddr, 1, frame_info->mLen, fd);
            fseek(fd, 0, SEEK_END);
            int fileLen = ftell(fd);
            transfer_len += frame_info->mLen;
            if (transfer_len >= fileLen) {
                transfer_len = 0;
            }
            fclose(fd);
        } else {
            // DOORLOCK_ERR("uac_mpp_ai_user_callback open [%s] failed\n", ai_file_name);
        }
    }

    if (frame_info->mLen <= buf_tmp->mem_info.mem_size) {
        memcpy(buf_tmp->mem_info.mem_vir, frame_info->mpAddr, frame_info->mLen);
        buf_tmp->cur_mem_size = frame_info->mLen;
    } else {
        DOORLOCK_ERR("Capture mic data size %d is bigger than mem size %d\n", frame_info->mLen,
                    buf_tmp->mem_info.mem_size);
        return;
    }
    if (frame_cnt == 0) {
        DOORLOCK_INFO("mic send first frame, %d, %lld us\n", buf_tmp->cur_mem_size, get_cur_time_us());
    }
    frame_cnt++;

    if (frm_manager->first_idle_to_using_frame)
        frm_manager->first_idle_to_using_frame(frm_manager, buf_tmp);

    return;
}

static void uac_mpp_ao_user_callback(mpp_ao_chn_t *mpp_ao_chn_info)
{
    static unsigned int frame_cnt = 0;
    mpp_ao_t *mpp_ao_info = mpp_ao_chn_info->mpp_ao;
    uac_dev_t *uac_dev_info = &g_context_info->uac_dev_info;
    int ret = 0;
    uint8_t *data = NULL;
    frm_manager_t *frm_manager = &uac_dev_info->spk_frm_manager;
    frame_mem_t *buf_tmp = NULL;

    if (frm_manager->prefetch_first_using_frame)
        ret = frm_manager->prefetch_first_using_frame(frm_manager, &buf_tmp);
    if (buf_tmp == NULL || ret < 0) {
        // DOORLOCK_DBG("frm_manager->prefetch_first_using_frame fail\n");
        return;
    }
    if (g_store_dir) {
        char ai_file_name[256];
        memset(ai_file_name, 0, sizeof(ai_file_name));
        sprintf(ai_file_name, "%s/spk_%d-%d-%d-%d.pcm", g_store_dir,
                mpp_ao_info->aio_config.channel_cnt, mpp_ao_info->aio_config.bit_width,
                mpp_ao_info->aio_config.sample_rate, mpp_ao_info->ao_volume);
        FILE *fd = fopen(ai_file_name, "ab");
        if (fd != NULL) {
            fwrite(buf_tmp->mem_info.mem_vir, 1, buf_tmp->cur_mem_size, fd);
            fclose(fd);
        } else {
            DOORLOCK_ERR("uac_mpp_ao_user_callback open [%s] failed\n", ai_file_name);
        }
    }

    AUDIO_FRAME_S audio_frame_info;
    mpp_config_audio_frame(frame_cnt, &audio_frame_info, mpp_ao_info->aio_config.sample_rate,
                        mpp_ao_info->aio_config.bit_width, mpp_ao_info->aio_config.channel_cnt);
    audio_frame_info.mpAddr = buf_tmp->mem_info.mem_vir;
    audio_frame_info.mLen = buf_tmp->cur_mem_size;
    // DOORLOCK_DBG("mpp_send_ao_audio_frame %d\n", buf_tmp->cur_mem_size);
    mpp_send_ao_audio_frame(mpp_ao_chn_info, &audio_frame_info, 0);

    if (frame_cnt == 0) {
        DOORLOCK_INFO("spk frame [%d] send ao, size %d, %lld us\n", frame_cnt, buf_tmp->cur_mem_size,
               get_cur_time_us());
    }
    frame_cnt++;
    frm_manager->first_using_to_idle_frame(frm_manager, buf_tmp);

    return;
}
#endif

static int start_mpp_uac_mic_process(uac_dev_t *uac_dev_info, bool on_off)
{
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;
    if (on_off) {
#if SNDCARD_SUPPORT
        system("/bin/setaudioconfig mic");

        //if(g_context_info->snd_card_info.mic)
        snd_card_open_mic(&g_context_info->snd_card_info);
        snd_card_set_mic_volume(&g_context_info->snd_card_info, dev_config->audio_volume);
        pthread_create(&g_context_info->snd_card_mic_thread, NULL, snd_card_mic_thread, (void *)g_context_info);
        pthread_setname_np(g_context_info->snd_card_mic_thread, "SndCardMic");
#endif
#if MPPAIO_SUPPORT
        g_context_info->mpp_ai_info.ai_dev = 0;
        g_context_info->mpp_ai_info.ai_volume = g_audio_volume;
        g_context_info->mpp_ai_info.aio_config.channel_cnt = g_audio_ch_cnt;
        g_context_info->mpp_ai_info.aio_config.sample_rate = g_audio_sample_rate;  // 16000;//
        g_context_info->mpp_ai_info.aio_config.bit_width = g_audio_bit_width;
        g_context_info->mpp_ai_info.aio_config.ai_aec_en = dev_config->audio_aec;

        g_context_info->mpp_ai_info.aio_config.ai_ans_en = 1;
        g_context_info->mpp_ai_info.aio_config.ai_ans_mode = 2;
        g_context_info->mpp_ai_info.aio_config.ai_agc_en = 1;
        g_context_info->mpp_ai_info.aio_config.ai_agc_gain = 10;

        g_context_info->mpp_ai_info.aio_config.frame_size = g_audio_period_size;

        mpp_ai_init(&g_context_info->mpp_ai_info);
        g_context_info->mpp_ai_chn_info.mpp_ai = &g_context_info->mpp_ai_info;
        g_context_info->mpp_ai_chn_info.bind = 0;
        g_context_info->mpp_ai_chn_info.ai_chn = g_ai_chn;

        g_context_info->mpp_ai_chn_info.thread_attr = NULL;

        g_context_info->mpp_ai_chn_info.user_thread = NULL;
        g_context_info->mpp_ai_chn_info.user_callback = uac_mpp_ai_user_callback;
        mpp_ai_chn_init(&g_context_info->mpp_ai_chn_info);
        mpp_ai_chn_start(&g_context_info->mpp_ai_chn_info);
        mpp_ai_chn_thread_init(&g_context_info->mpp_ai_chn_info);
#endif
    } else {
#if SNDCARD_SUPPORT
        //pthread_cancel(g_context_info->snd_card_mic_thread);
        pthread_join(g_context_info->snd_card_mic_thread, NULL);
        //if(g_context_info->snd_card_info.mic)
        snd_card_close_mic(&g_context_info->snd_card_info);
#endif
#if MPPAIO_SUPPORT
        mpp_ai_chn_thread_deInit(&g_context_info->mpp_ai_chn_info);

        mpp_ai_chn_stop(&g_context_info->mpp_ai_chn_info);
        mpp_ai_chn_deinit(&g_context_info->mpp_ai_chn_info);

        mpp_ai_deinit(&g_context_info->mpp_ai_info);
#endif
    }
    return 0;
}

static int uac_mic_resource_on_off(uac_dev_t *uac_dev_info, bool on_off)
{
    int ret;
    DOORLOCK_DBG("enter ===>[%d]\n", on_off);

    if (on_off) {
        if (uac_dev_info->mic_is_resource_ok == 1) {
            DOORLOCK_ERR("mic already on!!!\n");
            ret = 1;
            goto exit;
        }

        ret = start_mpp_uac_mic_process(uac_dev_info, 1);
        if (ret < 0) {
            DOORLOCK_ERR("mic start processing failed!!\n");
        }
        uac_dev_info->mic_is_resource_ok = 1;

    } else {
        if (uac_dev_info->mic_is_resource_ok == 0) {
            DOORLOCK_ERR("mic already off!!!\n");
            ret = 1;
            goto exit;
        }
        // stop and destroy venc
        start_mpp_uac_mic_process(uac_dev_info, 0);

        uac_dev_info->mic_is_resource_ok = 0;
    }

exit:
    DOORLOCK_DBG("exit <===[%d]\n", on_off);
    return ret;
}

static int start_mpp_uac_spk_process(uac_dev_t *uac_dev_info, bool on_off)
{
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;

#if SNDCARD_SUPPORT
        system("/bin/setaudioconfig spk");
        //if(g_context_info->snd_card_info.spk)
        snd_card_open_spk(&g_context_info->snd_card_info);
        snd_card_set_spk_volume(&g_context_info->snd_card_info, dev_config->audio_volume);
        pthread_create(&g_context_info->snd_card_spk_thread, NULL, snd_card_spk_thread, (void *)g_context_info);
        pthread_setname_np(g_context_info->snd_card_spk_thread, "snd_card_spk_thread");
#endif
    if (on_off) {
#if MPPAIO_SUPPORT
        g_context_info->mpp_ao_info.ao_dev = 0;
        g_context_info->mpp_ao_info.ao_volume = g_audio_volume;
        g_context_info->mpp_ao_info.aio_config.channel_cnt = g_audio_ch_cnt;
        g_context_info->mpp_ao_info.aio_config.sample_rate = g_audio_sample_rate;  // 16000;//
        g_context_info->mpp_ao_info.aio_config.bit_width = g_audio_bit_width;
        g_context_info->mpp_ao_info.aio_config.ai_aec_en = dev_config->audio_aec;

        g_context_info->mpp_ao_info.aio_config.ai_ans_en = 1;
        g_context_info->mpp_ao_info.aio_config.ai_ans_mode = 2;
        g_context_info->mpp_ao_info.aio_config.ai_agc_en = 1;
        g_context_info->mpp_ao_info.aio_config.ai_agc_gain = 10;

        g_context_info->mpp_ao_info.aio_config.frame_size = g_audio_period_size;

        mpp_ao_init(&g_context_info->mpp_ao_info);
        g_context_info->mpp_ao_chn_info.ao_gain = 20;
        g_context_info->mpp_ao_chn_info.mpp_ao = &g_context_info->mpp_ao_info;
        // g_context_info->mpp_ao_chn_info.bind = 0;
        g_context_info->mpp_ao_chn_info.ao_chn = g_ai_chn;

        g_context_info->mpp_ao_chn_info.thread_attr = NULL;

        g_context_info->mpp_ao_chn_info.user_thread = NULL;
        g_context_info->mpp_ao_chn_info.user_callback = uac_mpp_ao_user_callback;
        mpp_ao_chn_init(&g_context_info->mpp_ao_chn_info);
        mpp_ao_chn_start(&g_context_info->mpp_ao_chn_info);
        mpp_ao_chn_thread_init(&g_context_info->mpp_ao_chn_info);
#endif
    } else {
#if SNDCARD_SUPPORT
        //pthread_cancel(g_context_info->snd_card_spk_thread);
        pthread_join(g_context_info->snd_card_spk_thread, NULL);
        //if(g_context_info->snd_card_info.spk)
        snd_card_close_spk(&g_context_info->snd_card_info);
#endif
#if MPPAIO_SUPPORT
        mpp_ao_chn_thread_deinit(&g_context_info->mpp_ao_chn_info);
        mpp_ao_chn_stop(&g_context_info->mpp_ao_chn_info);
        mpp_ao_chn_deinit(&g_context_info->mpp_ao_chn_info);
        mpp_ao_deinit(&g_context_info->mpp_ao_info);
#endif
    }
    return 0;
}

static int uac_spk_resource_on_off(uac_dev_t *uac_dev_info, bool on_off)
{
    int ret;
    DOORLOCK_DBG("enter ===>[%d]\n", on_off);

    if (on_off) {
        if (uac_dev_info->spk_is_resource_ok == 1) {
            DOORLOCK_ERR("spk already on!!!\n");
            ret = 1;
            goto exit;
        }

        ret = start_mpp_uac_spk_process(uac_dev_info, 1);
        if (ret < 0) {
            DOORLOCK_ERR("spk start processing failed!!\n");
        }
        uac_dev_info->spk_is_resource_ok = 1;

    } else {
        if (uac_dev_info->spk_is_resource_ok == 0) {
            DOORLOCK_ERR("spk already off!!!\n");
            ret = 1;
            goto exit;
        }
        // stop and destroy venc
        start_mpp_uac_spk_process(uac_dev_info, 0);
        uac_dev_info->spk_is_resource_ok = 0;
    }
exit:
    DOORLOCK_DBG("exit <===[%d]\n", on_off);
    return ret;
}

static int uac_dev_on_off(uac_dev_t *uac_dev_info, bool on_off)
{
    int ret = 0;
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;

    DOORLOCK_DBG("enter ===>[%d]\n", on_off);
    if (uac_dev_info->fast_connect) {
        DOORLOCK_WARN("wait for uvc ready\n");
#if UVC_SUPPORT
        while (dev_config->video_uvc && !g_context_info->uvc_out_info.is_resource_ok) {
            usleep(10 * 1000);
        }
#endif
        DOORLOCK_WARN("wait for algo ready\n");
        while (dev_config->algo_mode && !g_context_info->algo_info.algo_state) {
            usleep(10 * 1000);
        }
        DOORLOCK_WARN("uvc can work now\n");
    }

#if MPPAIO_SUPPORT
    if (on_off) {
        DOORLOCK_INFO("before mpp_init %lld us\n", get_cur_time_us());
        mpp_init();
        DOORLOCK_INFO("after mpp_init %lld us\n", get_cur_time_us());
    } else {
        DOORLOCK_INFO("before mpp_deinit %lld us\n", get_cur_time_us());
        mpp_deinit();
        DOORLOCK_INFO("after mpp_deinit %lld us\n", get_cur_time_us());
    }
#endif
exit:
    DOORLOCK_DBG("exit <===[%d]\n", on_off);
    return ret;
}
#endif

int main(int argc, char *argv[])
{
    int ret = 0;
    int uvc_enable;
    int spk_enable;
    int mic_enable;
    int orl_enable;
    int videv;

    videv = 1;
    uvc_enable = 1;
    spk_enable = 1;
    mic_enable = 1;
    orl_enable = 0;
    g_audio_bit_width = 16;
    g_audio_ch_cnt = 1;
    g_audio_sample_rate = 8000;
    g_store_dir = NULL;
    g_exit_flag = false;
    doorlock_context_t sample_context_info;
    g_context_info = &sample_context_info;

    memset(g_context_info, 0, sizeof(doorlock_context_t));
    aw_dev_pre_init(&g_context_info->dev_info);
    aw_g2d_open();
    aw_mem_open();
    rt_media_init();
    DOORLOCK_INFO("after rt_media_init %lld us\n", get_cur_time_us());
    aw_dev_config_t *dev_config = &g_context_info->dev_info.config_info;

    if (dev_config->valid_flag == AW_VALID_FALG) {
        dev_config->video_orl = orl_enable;
        videv = dev_config->video_index;
        uvc_enable = dev_config->video_uvc;
        spk_enable = dev_config->audio_spk;
        mic_enable = dev_config->audio_mic;
        g_audio_bit_width = dev_config->audio_bit_width;
        g_audio_ch_cnt = dev_config->audio_ch_count;
        g_audio_sample_rate = dev_config->audio_sample_rate;
        g_audio_volume = dev_config->audio_volume;

        if (dev_config->led_switch) {
            door_lock_set_led(1);
        } else {
            door_lock_set_led(0);
        }
    }

    g_context_info->algo_info.rotate = APP_ROTATE;

    for (int i = 0; i < 2; i++) {
        frm_manager_t *frm_manager = &g_context_info->algo_frms_info[i];
        frm_manager->frm_node_cnt = 2;
        frm_manager->frm_node_memsize = ALGO_IMAGE_SIZE;
        frm_manager_init(frm_manager);
    }
    DOORLOCK_INFO("algo thread before create %lld us\n", get_cur_time_us());

    int index[2] = {0, 1};
    for (int i = 0; i < 2; i++) {
        char thread_name[20] = {0};
        memset(thread_name, 0, sizeof(thread_name));
        sprintf(thread_name, "algo_get_yuv_%d", i);
        pthread_create(&g_context_info->algo_image_threads[i], NULL, algo_get_yuv_thread,
                           (void *)&index[i]);
        pthread_setname_np(g_context_info->algo_image_threads[i], thread_name);
    }

    pthread_create(&g_context_info->algo_detect_thread, NULL, algo_detect_thread,
                       (void *)g_context_info);
    pthread_setname_np(g_context_info->algo_detect_thread, "algo_detect");

    g_context_info->uvc_vi_dev = UVC_VIDEO_BASE + (videv % 2);
    g_context_info->input_video_v4l2_format = V4L2_PIX_FMT_NV21;

#if UVC_SUPPORT
    sprintf(g_context_info->uvc_out_info.uvc_dev_name, "/dev/video2");
    g_context_info->uvc_out_info.frm_manager.frm_node_cnt = 2;
    g_context_info->uvc_out_info.resource_on_off = uvc_resource_on_off;
    g_context_info->uvc_out_info.default_format_index = 1;
    g_context_info->uvc_out_info.default_frame_index = 1;
    g_context_info->uvc_out_info.uvcout_format_frame_info = app_uvcout_format_data;
    g_context_info->uvc_out_info.uvcout_format_cnt =
        sizeof(app_uvcout_format_data) / sizeof(uvc_format_info_t);
    g_context_info->uvc_out_info.fast_connect = dev_config->auto_connect;
#endif

    DOORLOCK_INFO("uvc %d, uac_spk %d, uac_mic %d, %lld us\n", uvc_enable, spk_enable, mic_enable,
           get_cur_time_us());

#if UVC_SUPPORT
    if (uvc_enable) {
        if (dev_config->usb_type & 0x01) {
            g_context_info->uvc_out_info.usb_speed = 1;
        }
        if (dev_config->usb_type & 0x02) {
            g_context_info->uvc_out_info.bulk_mode = 1;
        }
        uvc_out_init(&g_context_info->uvc_out_info);
    }
#endif
    DOORLOCK_INFO("after uvc init, %lld us\n", get_cur_time_us());

#if UAC_SUPPORT
    g_context_info->uac_dev_info.dev_on_off = uac_dev_on_off;
#if MPPAIO_SUPPORT
    g_audio_period_size = 320;
    if (g_context_info->uac_dev_info.dev_on_off == NULL) {
        DOORLOCK_INFO("before mpp init %lld us\n", get_cur_time_us());
        mpp_init();
        DOORLOCK_INFO("after mpp init %lld us\n", get_cur_time_us());
    }
#endif

    if (spk_enable || mic_enable) {
        sprintf(g_context_info->uac_dev_info.uac_devname, "UAC1Gadget");
        g_context_info->uac_dev_info.mic_bitwidth = g_audio_bit_width;
        g_context_info->uac_dev_info.mic_channel_cnt = g_audio_ch_cnt;
        g_context_info->uac_dev_info.mic_sample_rate = g_audio_sample_rate;

        g_context_info->uac_dev_info.spk_bitwidth = g_audio_bit_width;
        g_context_info->uac_dev_info.spk_channel_cnt = g_audio_ch_cnt;
        g_context_info->uac_dev_info.spk_sample_rate = g_audio_sample_rate;
#if SNDCARD_SUPPORT
        sprintf(g_context_info->snd_card_info.snd_devname, "audiocodec");
        g_context_info->snd_card_info.mic_bitwidth = g_audio_bit_width;
        g_context_info->snd_card_info.mic_channel_cnt = g_audio_ch_cnt;
        g_context_info->snd_card_info.mic_sample_rate = g_audio_sample_rate;

        g_context_info->snd_card_info.spk_bitwidth = g_audio_bit_width;
        g_context_info->snd_card_info.spk_channel_cnt = g_audio_ch_cnt;
        g_context_info->snd_card_info.spk_sample_rate = g_audio_sample_rate;

        g_context_info->snd_card_info.spk = spk_enable;
        g_context_info->snd_card_info.mic = mic_enable;

        g_audio_period_size = 160 * g_audio_sample_rate / 8000;

        g_context_info->snd_card_info.mic_alsa_cfg.period_size = g_audio_period_size;
        g_context_info->snd_card_info.spk_alsa_cfg.period_size = g_audio_period_size;
        snd_card_init(&g_context_info->snd_card_info);
#endif
    }

    if (spk_enable) {
        g_context_info->uac_dev_info.spk = 1;
#if SNDCARD_SUPPORT
        g_context_info->uac_dev_info.max_mic_frame_size = g_audio_period_size*(g_audio_sample_rate/8000)*g_audio_ch_cnt*g_audio_bit_width/8;
#endif
        g_context_info->uac_dev_info.max_spk_frame_size =
            g_audio_period_size * g_audio_ch_cnt * g_audio_bit_width / 8;
        g_context_info->uac_dev_info.spk_resource_on_off = uac_spk_resource_on_off;
    }
    if (mic_enable) {
        g_context_info->uac_dev_info.mic = 1;
#if SNDCARD_SUPPORT
        g_context_info->uac_dev_info.max_mic_frame_size = g_audio_period_size*(g_audio_sample_rate/8000)*g_audio_ch_cnt*g_audio_bit_width/8;
#endif
        g_context_info->uac_dev_info.max_mic_frame_size =
            g_audio_period_size * g_audio_ch_cnt * g_audio_bit_width / 8;
        g_context_info->uac_dev_info.mic_resource_on_off = uac_mic_resource_on_off;
    }

    if (spk_enable || mic_enable) {
        g_context_info->uac_dev_info.fast_connect = dev_config->auto_connect;  //
        g_context_info->uac_dev_info.mic_nonblock = 1;
        g_context_info->uac_dev_info.spk_nonblock = 0;
        g_context_info->uac_dev_info.mic_alsa_cfg.period_size = g_audio_period_size;
        g_context_info->uac_dev_info.spk_alsa_cfg.period_size = g_audio_period_size;
        if (dev_config->usb_type & 0x01) {
            g_context_info->uac_dev_info.usb_speed = 1;
        }
        uac_dev_init(&g_context_info->uac_dev_info);
    }
#endif
    DOORLOCK_INFO("after uac init, %lld us\n", get_cur_time_us());

#if (UVC_SUPPORT | UAC_SUPPORT)
    g_context_info->composite_info.usb_vid =
        0x8880 | (uvc_enable << 2) | (spk_enable << 1) | (mic_enable << 0);
    g_context_info->composite_info.usb_pid = 0x1234;
    if (uvc_enable) {
        g_context_info->composite_info.uvc_out_info = &g_context_info->uvc_out_info;
    }
    if (spk_enable || mic_enable) {
        uint8_t PID_H = ((g_audio_bit_width / 8) << 4) + g_audio_ch_cnt;
        uint8_t PID_L = g_audio_sample_rate / 8000;
        // DOORLOCK_INFO("PID_H = 0x%02x, PID_L = 0x%02x\n", PID_H, PID_L);
        g_context_info->composite_info.usb_pid = (PID_H << 8) + (PID_L << 0);
        g_context_info->composite_info.uac_dev_info = &g_context_info->uac_dev_info;
    }

    DOORLOCK_INFO("USB VID = 0x%04x, PID = 0x%04x\n", g_context_info->composite_info.usb_vid,
                 g_context_info->composite_info.usb_pid);
    g_context_info->composite_info.uvc = uvc_enable;
    g_context_info->composite_info.uac = mic_enable | spk_enable;
    g_context_info->composite_info.fast_connect = dev_config->auto_connect;  // 0;
    composite_dev_init(&g_context_info->composite_info);
    DOORLOCK_INFO("after composite init, %lld us\n", get_cur_time_us());
    composite_dev_connect(&g_context_info->composite_info);
    DOORLOCK_INFO("after composite connect, %lld us\n", get_cur_time_us());
    if (uvc_enable) {
        uvc_out_run(&g_context_info->uvc_out_info);
    }
    if (spk_enable || mic_enable) {
        uac_dev_run(&g_context_info->uac_dev_info);
    }
#endif

    while (g_exit_flag == false) {
        usleep(100 * 1000);
    }

#if (UVC_SUPPORT | UAC_SUPPORT)
    composite_dev_disconnect(&g_context_info->composite_info);
    composite_dev_deinit(&g_context_info->composite_info);
#endif

#if UAC_SUPPORT
    if (spk_enable || mic_enable) {
        uac_dev_deinit(&g_context_info->uac_dev_info);
#if SNDCARD_SUPPORT
        snd_card_deinit(&g_context_info->snd_card_info);
#endif
        if (g_context_info->uac_dev_info.dev_on_off == NULL) {
#if MPPAIO_SUPPORT
            mpp_deinit();
#endif
        }
    }
#endif

#if UVC_SUPPORT
    if (uvc_enable) {
        uvc_out_deinit(&g_context_info->uvc_out_info);
    }
#endif

    pthread_join(g_context_info->algo_detect_thread, NULL);
    for (int i = 0; i < 2; i++) {
        frm_manager_t *frm_manager = &g_context_info->algo_frms_info[i];
        frm_manager_deinit(frm_manager);
        pthread_join(g_context_info->algo_image_threads[i], NULL);
    }

    aw_dev_deinit(&g_context_info->dev_info);
    rt_media_deinit();
    aw_g2d_close();
    aw_mem_close();
    DOORLOCK_INFO("rt_media-doorlock exit\n");
    return ret;
}
