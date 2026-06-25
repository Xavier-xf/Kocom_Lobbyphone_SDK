/*
 * lv_box_setting_activity.c
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#include "lv_box_setting_activity.h"
#include "lv_box_wifi_activity.h"
#include "lv_box_bt_activity.h"
#include "lv_box_display_activity.h"
#include "../common/lv_box_res.h"
#include "../widget/lv_box_set_btn.h"

static void event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    char *user_date = (char*) lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED) {
        if (!strcmp(user_date, "Wireless Fidelity")) {
            lv_box_wifi_init();
            lv_scr_load_anim(wifi_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                    100, false);
        } else if (!strcmp(user_date, "Bluetooth")) {
            lv_box_bt_init();
            lv_scr_load_anim(bt_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 100,
                    false);
        } else if (!strcmp(user_date, "Display")) {
            lv_box_display_init();
            lv_scr_load_anim(display_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                    100, false);
        }
    }
}

void lv_box_setting_init(void) {
    setting_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(setting_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *title_lable = lv_label_create(setting_activity);
    lv_obj_set_pos(title_lable, 15, 5);
    lv_obj_add_style(title_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(title_lable, true);
    lv_label_set_text_fmt(title_lable, "#ffffff %s #", "Setting");

    lv_obj_t *main_cont = lv_obj_create(setting_activity);
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

    char *set_png_path[] = { "A:/usr/share/lv_86_boxes/set_wifi.png",
            "A:/usr/share/lv_86_boxes/set_bt.png",
            "A:/usr/share/lv_86_boxes/set_add_devices.png",
            "A:/usr/share/lv_86_boxes/set_music.png",
            "A:/usr/share/lv_86_boxes/set_voice.png",
            "A:/usr/share/lv_86_boxes/set_account.png",
            "A:/usr/share/lv_86_boxes/set_display.png",
            "A:/usr/share/lv_86_boxes/set_customize.png",
            "A:/usr/share/lv_86_boxes/set_sound.png" };

    char *set_png_name[] = { "Wireless Fidelity", "Bluetooth", "Add device",
            "Music", "Voice", "Account", "Display", "Customize", "Sound" };

    for (int i = 0; i < 9; i++) {
        if (i == 3 || i == 5 || i == 6) {
            lv_obj_t *line = lv_obj_create(main_cont);
            lv_obj_remove_style_all(line);
            lv_obj_set_size(line, 480, 5);
        }

        lv_obj_t *set_btn = lv_box_set_btn_create(main_cont);
        lv_obj_set_size(set_btn, 480, 80);
        lv_box_set_btn_src(set_btn, &lv_box_res.style_ft_30,
                &lv_box_res.set_bg_style, set_png_path[i],
                "A:/usr/share/lv_86_boxes/right_arrow.png", set_png_name[i]);
        lv_obj_add_event_cb(set_btn, event_cb, LV_EVENT_CLICKED,
                set_png_name[i]);
    }
}
