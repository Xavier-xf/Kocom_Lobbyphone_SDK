/*
 * lv_box_alarm_activity.c
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#include "lv_box_alarm_activity.h"
#include "lv_box_add_alarm_activity.h"
#include "../common/lv_box_res.h"
#include "../widget/lv_box_alarm_item.h"

static void event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_box_add_alarm_init();
        lv_scr_load_anim(add_alarm_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                100, false);
    }
}

void lv_box_alarm_init(void) {
    alarm_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(alarm_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *title_lable = lv_label_create(alarm_activity);
    lv_obj_set_pos(title_lable, 15, 5);
    lv_obj_add_style(title_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(title_lable, true);
    lv_label_set_text_fmt(title_lable, "#ffffff %s #", "Alarm");

    lv_obj_t *add_img = lv_img_create(alarm_activity);
    lv_obj_center(add_img);
    lv_img_set_src(add_img, "A:/usr/share/lv_86_boxes/add.png");
    lv_obj_align(add_img, LV_ALIGN_TOP_RIGHT, -15, 15);

    lv_obj_t *click_rect =  lv_obj_create(alarm_activity);
    lv_obj_remove_style_all(click_rect);
    lv_obj_set_size(click_rect, 60, 60);
    lv_obj_align(click_rect, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(click_rect, LV_OBJ_FLAG_CLICKABLE);
    //lv_obj_add_event_cb(click_rect, event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *main_cont = lv_obj_create(alarm_activity);
    lv_obj_set_pos(main_cont, 0, 60);
    lv_obj_set_size(main_cont, 480, 420);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 15, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_shadow_width(main_cont, 0, 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x10111a), 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *alarm_item = lv_box_alarm_item_create(main_cont);
        lv_obj_set_size(alarm_item, 450, 120);
        bool state = i % 2 == 0 ? true : false;
        lv_box_alarm_item_set_src(alarm_item, &lv_box_res.style_ft_40,
                &lv_box_res.style_ft_26, &lv_box_res.style_ft_26,
                &lv_box_res.lamp_bg_style, state);
        lv_obj_add_event_cb(alarm_item, event_cb, LV_EVENT_CLICKED, NULL);
    }

}
