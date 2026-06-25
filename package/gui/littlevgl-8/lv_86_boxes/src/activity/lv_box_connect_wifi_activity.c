/*
 * lv_box_connect_wifi_activity.c
 *
 *  Created on: 2022Äê9ÔÂ26ÈÕ
 *      Author: anruliu
 */

#include "lv_box_connect_wifi_activity.h"
#include "lv_box_wifi_activity.h"
#include "../common/lv_box_res.h"

static lv_obj_t *ta;

static void event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    char *data = (char*) lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED) {
        if (!strcmp(data, "cancel")) {
            lv_scr_load_anim(wifi_activity, LV_SCR_LOAD_ANIM_NONE, 200, 100,
                    true);
        } else if (!strcmp(data, "connect")) {
            lv_box_wifi_connect(lv_textarea_get_text(ta));
            lv_scr_load_anim(wifi_activity, LV_SCR_LOAD_ANIM_NONE, 200, 100,
                    true);
        }
    }
}

void lv_box_connect_wifi_init(char *ssid) {
    connect_wifi_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(connect_wifi_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *cancel_btn = lv_btn_create(connect_wifi_activity);
    lv_obj_remove_style_all(cancel_btn);
    lv_obj_set_pos(cancel_btn, 15, 5);
    lv_obj_add_event_cb(cancel_btn, event_cb, LV_EVENT_CLICKED, "cancel");

    lv_obj_t *cancel_lable = lv_label_create(cancel_btn);
    lv_obj_center(cancel_lable);
    lv_obj_add_style(cancel_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(cancel_lable, true);
    lv_label_set_text_fmt(cancel_lable, "#ffffff %s #", "Cancel");

    lv_obj_t *connect_btn = lv_btn_create(connect_wifi_activity);
    lv_obj_remove_style_all(connect_btn);
    lv_obj_align(connect_btn, LV_ALIGN_TOP_RIGHT, -15, 5);

    lv_obj_add_event_cb(connect_btn, event_cb, LV_EVENT_CLICKED, "connect");
    lv_obj_t *connect_lable = lv_label_create(connect_btn);
    lv_obj_center(connect_lable);
    lv_obj_add_style(connect_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(connect_lable, true);
    lv_label_set_text_fmt(connect_lable, "#ffa523 %s #", "Connect");

    lv_obj_t *main_cont = lv_obj_create(connect_wifi_activity);
    lv_obj_set_pos(main_cont, 0, 60);
    lv_obj_set_size(main_cont, 480, 420);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 15, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_shadow_width(main_cont, 0, 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x10111a), 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *tisp_lable = lv_label_create(main_cont);
    lv_obj_add_style(tisp_lable, &lv_box_res.style_ft_26, 0);
    lv_label_set_recolor(tisp_lable, true);
    lv_label_set_text_fmt(tisp_lable, "#ffffff %s #", "Please enter password");

    lv_obj_t *ssid_lable = lv_label_create(main_cont);
    lv_obj_add_style(ssid_lable, &lv_box_res.style_ft_30, 0);
    lv_obj_set_style_text_color(ssid_lable, lv_color_white(), 0);
    lv_label_set_text_fmt(ssid_lable, "%s", ssid);
    lv_obj_set_width(ssid_lable, 450);
    lv_label_set_long_mode(ssid_lable, 2);

    /*Create a text area. The keyboard will write here*/
    ta = lv_textarea_create(main_cont);
    //lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_ALL, kb);
    lv_textarea_set_placeholder_text(ta, "Please enter password");
    lv_textarea_set_password_mode(ta, true);
    lv_obj_set_size(ta, 450, 50);

    /*Create a keyboard*/
    lv_obj_t *kb = lv_keyboard_create(connect_wifi_activity);
    lv_obj_set_size(kb, LV_HOR_RES, LV_VER_RES / 2);

    lv_keyboard_set_textarea(kb, ta);

}
