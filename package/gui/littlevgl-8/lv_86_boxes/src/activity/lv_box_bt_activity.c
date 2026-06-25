/*
 * lv_box_bt_activity.c
 *
 *  Created on: 2022Äê10ÔÂ11ÈÕ
 *      Author: anruliu
 */

#include "lv_box_bt_activity.h"
#include "lv_box_setting_activity.h"
#include "../common/lv_box_res.h"
#include "../common/lv_box_res_bt.h"

static bool off_ing;
static bool on_ing;
static bool bt_state;
static bool exit_ok;

static lv_obj_t *state_spinner;
static lv_obj_t *bt_name_lable;

static void* bt_control_proc(void *arg) {
    int ret = -1;
    if (!strcmp(arg, "on")) {
        if (!off_ing) {
            on_ing = true;
            pthread_mutex_lock(&lvgl_mutex);
            if (!exit_ok)
                lv_obj_clear_flag(state_spinner, LV_OBJ_FLAG_HIDDEN);
            pthread_mutex_unlock(&lvgl_mutex);

            ret = lv_box_res_bt_on();

            pthread_mutex_lock(&lvgl_mutex);
            if (!exit_ok) {
                lv_obj_add_flag(state_spinner, LV_OBJ_FLAG_HIDDEN);
                if (ret == 0) {
                    lv_obj_clear_flag(bt_name_lable, LV_OBJ_FLAG_HIDDEN);
                }
            }
            pthread_mutex_unlock(&lvgl_mutex);
            if (ret == 0) {
                bt_state = true;
            } else {
                bt_state = false;
            }
            on_ing = false;
        }
    } else if (!strcmp(arg, "off")) {
        if (!on_ing) {
            off_ing = true;
            pthread_mutex_lock(&lvgl_mutex);
            if (!exit_ok) {
                lv_obj_clear_flag(state_spinner, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(bt_name_lable, LV_OBJ_FLAG_HIDDEN);
            }
            pthread_mutex_unlock(&lvgl_mutex);

            lv_box_res_bt_off();

            pthread_mutex_lock(&lvgl_mutex);
            if (!exit_ok)
                lv_obj_add_flag(state_spinner, LV_OBJ_FLAG_HIDDEN);
            pthread_mutex_unlock(&lvgl_mutex);
            bt_state = false;
            off_ing = false;
        }
    }

    return NULL;
}

static void event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        exit_ok = true;
        lv_scr_load_anim(setting_activity, LV_SCR_LOAD_ANIM_NONE, 200, 100,
                true);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
            pthread_t thread_id;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_create(&thread_id, &attr, bt_control_proc, "on");
            pthread_attr_destroy(&attr);
        } else {
            pthread_t thread_id;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_create(&thread_id, &attr, bt_control_proc, "off");
            pthread_attr_destroy(&attr);
        }
    }
}

void lv_box_bt_init(void) {
    bt_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(bt_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *title_lable = lv_label_create(bt_activity);
    lv_obj_set_pos(title_lable, 15, 5);
    lv_obj_add_style(title_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(title_lable, true);
    lv_label_set_text_fmt(title_lable, "#ffffff %s #", "Bluetooth");

    lv_obj_t *cross_img = lv_img_create(bt_activity);
    lv_obj_center(cross_img);
    lv_img_set_src(cross_img, "A:/usr/share/lv_86_boxes/cross.png");
    lv_obj_align(cross_img, LV_ALIGN_TOP_RIGHT, -15, 15);

    lv_obj_t *click_rect = lv_obj_create(bt_activity);
    lv_obj_remove_style_all(click_rect);
    lv_obj_set_size(click_rect, 60, 60);
    lv_obj_align(click_rect, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(click_rect, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(click_rect, event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *main_cont = lv_obj_create(bt_activity);
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
    //lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW_WRAP);

    lv_obj_t *bt_state_bg = lv_obj_create(main_cont);
    lv_obj_set_size(bt_state_bg, 480, 80);
    lv_obj_add_style(bt_state_bg, &lv_box_res.set_bg_style, 0);

    lv_obj_t *bt_state_name = lv_label_create(bt_state_bg);
    lv_label_set_recolor(bt_state_name, true);
    lv_obj_add_style(bt_state_name, &lv_box_res.style_ft_30, 0);
    lv_label_set_text_fmt(bt_state_name, "#ffffff %s #", "Bt Music");
    lv_obj_align(bt_state_name, LV_ALIGN_LEFT_MID, 15, 0);

    lv_obj_t *bt_state_sw = lv_switch_create(bt_state_bg);
    lv_obj_set_style_bg_color(bt_state_sw, lv_color_hex(0xffa523),
            LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_align(bt_state_sw, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_add_event_cb(bt_state_sw, event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    state_spinner = lv_spinner_create(main_cont, 1000, 60);
    lv_obj_set_size(state_spinner, 60, 60);
    lv_obj_align(state_spinner, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(state_spinner, LV_OBJ_FLAG_HIDDEN);

    bt_name_lable = lv_label_create(main_cont);
    lv_obj_align(bt_name_lable, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_style(bt_name_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(bt_name_lable, true);
    lv_label_set_text_fmt(bt_name_lable, "#ffffff %s #", "LV_86_BOXES_MUSIC");

    exit_ok = false;

    if (bt_state) {
        lv_obj_add_state(bt_state_sw, LV_STATE_CHECKED);
    } else {
        lv_obj_add_flag(bt_name_lable, LV_OBJ_FLAG_HIDDEN);
    }

}
