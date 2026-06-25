/*
 * lv_box_lamp_activity.c
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#include "lv_box_lamp_activity.h"
#include "../widget/lv_box_lamp_btn.h"
#include "../common/lv_box_res.h"
#include <stdio.h>

static lv_obj_t *lamp_btn[8];

static void event_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_obj_get_parent(lv_event_get_target(e));
    bool chk = lv_obj_get_state(btn) & LV_STATE_CHECKED;
    for (int i = 0; i < 8; i++) {
        lv_box_lamp_btn_set_state(lamp_btn[i], chk);
    }
}

void lv_box_lamp_init(void) {
    lamp_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(lamp_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *title_label = lv_label_create(lamp_activity);
    lv_obj_set_pos(title_label, 15, 5);
    lv_obj_add_style(title_label, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(title_label, true);
    lv_label_set_text_fmt(title_label, "#ffffff %s #", "Lamp");

    lv_obj_t *main_cont = lv_obj_create(lamp_activity);
    lv_obj_set_pos(main_cont, 0, 60);
    lv_obj_set_size(main_cont, 480, 420);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 15, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_shadow_width(main_cont, 0, 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x10111a), 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_ROW_WRAP);

    lv_obj_t *all_lamp_btn = lv_box_lamp_btn_create(main_cont);
    lv_obj_set_size(all_lamp_btn, 450, 120);
    lv_box_lamp_btn_set_src(all_lamp_btn,
            "A:/usr/share/lv_86_boxes/lamp_on.png",
            "A:/usr/share/lv_86_boxes/lamp_off.png", "All", true,
            &lv_box_res.style_ft_26, &lv_box_res.lamp_bg_style);
    lv_obj_add_event_cb(all_lamp_btn, event_cb, LV_EVENT_CLICKED, NULL);

    for (int i = 0; i < 8; i++) {
        char str[8];
        sprintf(str, "Light %d", i + 1);
        lamp_btn[i] = lv_box_lamp_btn_create(main_cont);
        lv_obj_set_size(lamp_btn[i], 217, 120);
        bool state = i % 2 == 0 ? true : false;
        lv_box_lamp_btn_set_src(lamp_btn[i],
                "A:/usr/share/lv_86_boxes/lamp_on.png",
                "A:/usr/share/lv_86_boxes/lamp_off.png", str, state,
                &lv_box_res.style_ft_26, &lv_box_res.lamp_bg_style);
    }

}
