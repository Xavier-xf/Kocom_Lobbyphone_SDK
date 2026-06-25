/*
 * lv_box_display_activity.c
 *
 *  Created on: 2022Äê10ÔÂ11ÈÕ
 *      Author: anruliu
 */

#include "lv_box_display_activity.h"
#include "lv_box_setting_activity.h"
#include "../common/lv_box_res.h"
#include "../common/lv_box_res_display.h"

int map_brightness(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        lv_scr_load_anim(setting_activity, LV_SCR_LOAD_ANIM_NONE, 200, 100,
                true);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        lv_box_res_display_set_brightness(
                map_brightness(lv_slider_get_value(obj), 0, 100, 25, 255));
    }
}

void lv_box_display_init(void) {
    display_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(display_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *title_lable = lv_label_create(display_activity);
    lv_obj_set_pos(title_lable, 15, 5);
    lv_obj_add_style(title_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(title_lable, true);
    lv_label_set_text_fmt(title_lable, "#ffffff %s #", "Display");

    lv_obj_t *cross_img = lv_img_create(display_activity);
    lv_obj_center(cross_img);
    lv_img_set_src(cross_img, "A:/usr/share/lv_86_boxes/cross.png");
    lv_obj_align(cross_img, LV_ALIGN_TOP_RIGHT, -15, 15);

    lv_obj_t *click_rect = lv_obj_create(display_activity);
    lv_obj_remove_style_all(click_rect);
    lv_obj_set_size(click_rect, 60, 60);
    lv_obj_align(click_rect, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(click_rect, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(click_rect, event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *main_cont = lv_obj_create(display_activity);
    lv_obj_set_pos(main_cont, 0, 60);
    lv_obj_set_size(main_cont, 480, 420);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 15, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_shadow_width(main_cont, 0, 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x10111a), 0);
    //lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW_WRAP);

    /*Create a slider in the center of the display*/
    lv_obj_t *slider = lv_slider_create(main_cont);
    lv_obj_set_size(slider, 450, 10);
    lv_slider_set_value(slider,
            map_brightness(lv_box_res_display_get_brightness(), 25, 255, 0,
                    100), LV_ANIM_ON);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(slider, event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
