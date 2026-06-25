/*
 * lv_box_wifi_activity.c
 *
 *  Created on: 2022Äê9ÔÂ22ÈÕ
 *      Author: anruliu
 */

#include "lv_box_wifi_activity.h"
#include "lv_box_setting_activity.h"
#include "lv_box_connect_wifi_activity.h"
#include "../common/lv_box_res.h"
#include "../common/lv_box_res_wifi.h"
#include "../widget/lv_box_wifi_item.h"
#include "../widget/lv_box_set_btn.h"
#include <pthread.h>
#include <unistd.h>

static lv_obj_t *wifi_item[RES_LEN];
static int scan_run;
static int exit_ok = 1;
static lv_obj_t *wifi_connected_btn;
static lv_obj_t *scan_lable;
static lv_obj_t *main_cont;
static lv_obj_t *scan_spinner;
static lv_obj_t *connect_spinner;
static lv_obj_t *wifi_state_sw;
static char select_ssid[64];
static const char *select_password;
static int select_sec;
static bool wifi_state = false;
static bool connect_state = false;
static bool on_ing;
static bool off_ing;

static void* scan_proc(void *arg) {
    int ret = -1;
    while (1) {
        if (!scan_run) {
            exit_ok = 1;
            pthread_exit((void*) 1);
        }
        if (wifi_state) {
            pthread_mutex_lock(&lvgl_mutex);
            if (scan_run) {
                lv_obj_clear_flag(scan_spinner, LV_OBJ_FLAG_HIDDEN);
            }
            pthread_mutex_unlock(&lvgl_mutex);

            ret = lv_box_res_wifi_scan();

            if (ret == 0 && scan_run) {
                pthread_mutex_lock(&lvgl_mutex);
                lv_obj_add_flag(scan_spinner, LV_OBJ_FLAG_HIDDEN);

                for (int i = 0; i < RES_LEN; i++) {
                    lv_obj_add_flag(wifi_item[i], LV_OBJ_FLAG_HIDDEN);
                }

                for (int i = 0; i < get_scan_results_num; i++) {
                    if (strcmp(scan_res[i].ssid, "")) {
                        lv_box_wifi_item_set_name(wifi_item[i],
                                scan_res[i].ssid);
                        lv_obj_clear_flag(wifi_item[i], LV_OBJ_FLAG_HIDDEN);
                    }
                }

                if (!wifi_state) {
                    for (int i = 0; i < RES_LEN; i++) {
                        lv_obj_add_flag(wifi_item[i], LV_OBJ_FLAG_HIDDEN);
                    }
                }
                pthread_mutex_unlock(&lvgl_mutex);
            }

            sleep(10);
        } else {
            pthread_mutex_lock(&lvgl_mutex);

            if (scan_run && lv_obj_has_state(wifi_state_sw, LV_STATE_CHECKED)
                    && !on_ing) {
                lv_obj_clear_state(wifi_state_sw, LV_STATE_CHECKED);
                lv_obj_add_flag(wifi_connected_btn, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(scan_lable, LV_OBJ_FLAG_HIDDEN);
                for (int i = 0; i < RES_LEN; i++) {
                    lv_obj_add_flag(wifi_item[i], LV_OBJ_FLAG_HIDDEN);
                }
            }

            pthread_mutex_unlock(&lvgl_mutex);

            sleep(1);
        }
    }

    return NULL;
}

static void* wifi_control_proc(void *arg) {
    int ret = -1;
    if (!strcmp(arg, "on")) {
        if (!off_ing) {
            on_ing = true;
            ret = lv_box_res_wifi_on();
            if (ret == 0) {
                wifi_state = true;
            } else {
                wifi_state = false;
            }
            on_ing = false;
        }
    } else if (!strcmp(arg, "off")) {
        if (!on_ing) {
            off_ing = true;
            lv_box_res_wifi_off();
            wifi_state = false;
            connect_state = false;
            off_ing = false;
        }
    } else if (!strcmp(arg, "connect")) {
        ret = lv_box_res_wifi_connect(select_ssid, select_password, select_sec);
        if (ret == 0) {
            pthread_mutex_lock(&lvgl_mutex);
            connect_state = true;
            if (scan_run) {
                lv_obj_add_flag(connect_spinner, LV_OBJ_FLAG_HIDDEN);
                lv_box_set_btn_right_img_show(wifi_connected_btn, true);
            }
            pthread_mutex_unlock(&lvgl_mutex);
        } else {
            pthread_mutex_lock(&lvgl_mutex);
            connect_state = false;
            if (scan_run) {
                lv_obj_add_flag(wifi_connected_btn, LV_OBJ_FLAG_HIDDEN);
                lv_box_set_btn_right_img_show(wifi_connected_btn, false);
            }
            pthread_mutex_unlock(&lvgl_mutex);
        }
    }

    return NULL;
}

static void event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        scan_run = 0;
        lv_scr_load_anim(setting_activity, LV_SCR_LOAD_ANIM_NONE, 200, 100,
                true);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
            pthread_t thread_id;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_create(&thread_id, &attr, wifi_control_proc, "on");
            pthread_attr_destroy(&attr);

            lv_obj_clear_flag(scan_lable, LV_OBJ_FLAG_HIDDEN);
        } else {
            wifi_state = false;
            pthread_t thread_id;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_create(&thread_id, &attr, wifi_control_proc, "off");
            pthread_attr_destroy(&attr);
            lv_obj_add_flag(wifi_connected_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(scan_lable, LV_OBJ_FLAG_HIDDEN);
            for (int i = 0; i < RES_LEN; i++) {
                lv_obj_add_flag(wifi_item[i], LV_OBJ_FLAG_HIDDEN);
            }
        }

    }
}

static void item_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_obj_get_parent(lv_event_get_target(e));

    if (code == LV_EVENT_CLICKED) {
        memcpy(select_ssid, lv_box_wifi_item_get_name(obj),
                sizeof(select_ssid));
        for (int i = 0; i < get_scan_results_num; i++) {
            if (strcmp(scan_res[i].ssid, select_ssid)) {
                select_sec = (int) scan_res[i].key_mgmt;
            }
        }
        lv_box_connect_wifi_init(select_ssid);
        lv_scr_load_anim(connect_wifi_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                100, false);
    }
}

void lv_box_wifi_connect(const char *password) {
    lv_obj_scroll_to_y(main_cont, 0, LV_ANIM_ON);
    lv_box_set_btn_set_name(wifi_connected_btn, select_ssid);
    lv_box_set_btn_right_img_show(wifi_connected_btn, false);
    lv_obj_clear_flag(wifi_connected_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(connect_spinner, LV_OBJ_FLAG_HIDDEN);
    select_password = password;

    pthread_t thread_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread_id, &attr, wifi_control_proc, "connect");
    pthread_attr_destroy(&attr);
}

void lv_box_wifi_init(void) {
    scan_run = 1;
    wifi_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(wifi_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *title_lable = lv_label_create(wifi_activity);
    lv_obj_set_pos(title_lable, 15, 5);
    lv_obj_add_style(title_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(title_lable, true);
    lv_label_set_text_fmt(title_lable, "#ffffff %s #", "Wireless Fidelity");

    lv_obj_t *cross_img = lv_img_create(wifi_activity);
    lv_obj_center(cross_img);
    lv_img_set_src(cross_img, "A:/usr/share/lv_86_boxes/cross.png");
    lv_obj_align(cross_img, LV_ALIGN_TOP_RIGHT, -15, 15);

    lv_obj_t *click_rect = lv_obj_create(wifi_activity);
    lv_obj_remove_style_all(click_rect);
    lv_obj_set_size(click_rect, 60, 60);
    lv_obj_align(click_rect, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(click_rect, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(click_rect, event_cb, LV_EVENT_CLICKED, NULL);

    main_cont = lv_obj_create(wifi_activity);
    lv_obj_set_pos(main_cont, 0, 60);
    lv_obj_set_size(main_cont, 480, 420);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    lv_obj_set_style_pad_top(main_cont, 15, 0);
    lv_obj_set_style_pad_bottom(main_cont, 15, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_shadow_width(main_cont, 0, 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x10111a), 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW_WRAP);

    lv_obj_t *wifi_state_bg = lv_obj_create(main_cont);
    lv_obj_set_size(wifi_state_bg, 480, 80);
    lv_obj_add_style(wifi_state_bg, &lv_box_res.set_bg_style, 0);

    lv_obj_t *wifi_state_name = lv_label_create(wifi_state_bg);
    lv_label_set_recolor(wifi_state_name, true);
    lv_obj_add_style(wifi_state_name, &lv_box_res.style_ft_30, 0);
    lv_label_set_text_fmt(wifi_state_name, "#ffffff %s #", "Wifi");
    lv_obj_align(wifi_state_name, LV_ALIGN_LEFT_MID, 15, 0);

    wifi_state_sw = lv_switch_create(wifi_state_bg);
    lv_obj_set_style_bg_color(wifi_state_sw, lv_color_hex(0xffa523),
            LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_align(wifi_state_sw, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_add_event_cb(wifi_state_sw, event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    wifi_connected_btn = lv_box_set_btn_create(main_cont);
    lv_obj_set_size(wifi_connected_btn, 480, 80);
    lv_box_set_btn_src(wifi_connected_btn, &lv_box_res.style_ft_26,
            &lv_box_res.set_bg_style, "A:/usr/share/lv_86_boxes/wifi.png",
            "A:/usr/share/lv_86_boxes/hook.png", "AwTest");
    //lv_obj_add_event_cb(set_btn, event_cb, LV_EVENT_CLICKED, set_png_name[i]);

    connect_spinner = lv_spinner_create(wifi_connected_btn, 1000, 60);
    lv_obj_set_size(connect_spinner, 25, 25);
    lv_obj_align(connect_spinner, LV_ALIGN_RIGHT_MID, -15, 0);

    scan_lable = lv_label_create(main_cont);
    lv_label_set_recolor(scan_lable, true);
    lv_obj_add_style(scan_lable, &lv_box_res.style_ft_26, 0);
    lv_label_set_text_fmt(scan_lable, "#ffffff %s #", "Select Network");

    scan_spinner = lv_spinner_create(scan_lable, 1000, 60);
    lv_obj_set_size(scan_spinner, 25, 25);
    lv_obj_align_to(scan_spinner, scan_lable, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    for (int i = 0; i < RES_LEN; i++) {
        wifi_item[i] = lv_box_wifi_item_create(main_cont);
        lv_obj_set_size(wifi_item[i], 480, 80);
        lv_box_wifi_item_set_src(wifi_item[i], &lv_box_res.style_ft_26,
                &lv_box_res.set_bg_style, "A:/usr/share/lv_86_boxes/wifi.png",
                "NAME");
        lv_obj_add_flag(wifi_item[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(wifi_item[i], item_event_cb, LV_EVENT_CLICKED,
                NULL);
    }

    if (wifi_state) {
        lv_obj_add_state(wifi_state_sw, LV_STATE_CHECKED);
        if (connect_state) {
            lv_box_set_btn_set_name(wifi_connected_btn, select_ssid);
            lv_box_set_btn_right_img_show(wifi_connected_btn, true);
            lv_obj_add_flag(connect_spinner, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(wifi_connected_btn, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(wifi_connected_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scan_lable, LV_OBJ_FLAG_HIDDEN);
    }

    if (exit_ok) {
        exit_ok = 0;
        pthread_t thread_id;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&thread_id, &attr, scan_proc, NULL);
        pthread_attr_destroy(&attr);
    }

}
