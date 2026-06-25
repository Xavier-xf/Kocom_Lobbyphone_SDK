/*
 * lv_box_add_alarm_activity.c
 *
 *  Created on: 2022Äê9ÔÂ15ÈÕ
 *      Author: anruliu
 */

#include "lv_box_add_alarm_activity.h"
#include "lv_box_alarm_activity.h"
#include "../common/lv_box_res.h"
#include "../widget/lv_box_alarm_set_btn.h"

static lv_style_t roller_style;

static void event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_style_reset(&roller_style);
        lv_scr_load_anim(alarm_activity, LV_SCR_LOAD_ANIM_NONE, 200, 100, true);
    }
}

static void mask_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    static int16_t mask_top_id = -1;
    static int16_t mask_bottom_id = -1;

    if (code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_MASKED);

    } else if (code == LV_EVENT_DRAW_MAIN_BEGIN) {
        /* add mask */
        const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
        lv_coord_t line_space = lv_obj_get_style_text_line_space(obj,
                LV_PART_MAIN);
        lv_coord_t font_h = lv_font_get_line_height(font);

        lv_area_t roller_coords;
        lv_obj_get_coords(obj, &roller_coords);

        lv_area_t rect_area;
        rect_area.x1 = roller_coords.x1;
        rect_area.x2 = roller_coords.x2;
        rect_area.y1 = roller_coords.y1;
        rect_area.y2 = roller_coords.y1
                + (lv_obj_get_height(obj) - font_h - line_space) / 2;

        lv_draw_mask_fade_param_t *fade_mask_top = lv_mem_buf_get(
                sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_top, &rect_area, LV_OPA_TRANSP,
                rect_area.y1, LV_OPA_COVER, rect_area.y2);
        mask_top_id = lv_draw_mask_add(fade_mask_top, NULL);

        rect_area.y1 = rect_area.y2 + font_h + line_space - 1;
        rect_area.y2 = roller_coords.y2;

        lv_draw_mask_fade_param_t *fade_mask_bottom = lv_mem_buf_get(
                sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_bottom, &rect_area, LV_OPA_COVER,
                rect_area.y1, LV_OPA_TRANSP, rect_area.y2);
        mask_bottom_id = lv_draw_mask_add(fade_mask_bottom, NULL);

    } else if (code == LV_EVENT_DRAW_POST_END) {
        lv_draw_mask_fade_param_t *fade_mask_top = lv_draw_mask_remove_id(
                mask_top_id);
        lv_draw_mask_fade_param_t *fade_mask_bottom = lv_draw_mask_remove_id(
                mask_bottom_id);
        lv_mem_buf_release(fade_mask_top);
        lv_mem_buf_release(fade_mask_bottom);
        mask_top_id = -1;
        mask_bottom_id = -1;
    }
}

void lv_box_add_alarm_init(void) {
    add_alarm_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(add_alarm_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *cancel_btn = lv_btn_create(add_alarm_activity);
    lv_obj_remove_style_all(cancel_btn);
    lv_obj_set_pos(cancel_btn, 15, 5);
    lv_obj_add_event_cb(cancel_btn, event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_lable = lv_label_create(cancel_btn);
    lv_obj_center(cancel_lable);
    lv_obj_add_style(cancel_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(cancel_lable, true);
    lv_label_set_text_fmt(cancel_lable, "#ffffff %s #", "Cancel");

    lv_obj_t *save_btn = lv_btn_create(add_alarm_activity);
    lv_obj_remove_style_all(save_btn);
    lv_obj_align(save_btn, LV_ALIGN_TOP_RIGHT, -15, 5);

    lv_obj_add_event_cb(save_btn, event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_lable = lv_label_create(save_btn);
    lv_obj_center(save_lable);
    lv_obj_add_style(save_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(save_lable, true);
    lv_label_set_text_fmt(save_lable, "#ffa523 %s #", "Save");

    lv_obj_t *main_cont = lv_obj_create(add_alarm_activity);
    lv_obj_set_pos(main_cont, 0, 60);
    lv_obj_set_size(main_cont, 480, 420);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_shadow_width(main_cont, 0, 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x10111a), 0);
    //lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    lv_style_init(&roller_style);
    lv_style_set_bg_color(&roller_style, lv_color_black());
    lv_style_set_text_color(&roller_style, lv_color_white());
    lv_style_set_border_width(&roller_style, 0);
    lv_style_set_pad_all(&roller_style, 0);
    lv_obj_add_style(add_alarm_activity, &roller_style, 0);
    lv_style_set_text_font(&roller_style, lv_box_res.ft_info_50.font);

    lv_obj_t *hour_roller = lv_roller_create(main_cont);
    lv_obj_set_size(hour_roller, 240, 360);
    lv_obj_add_style(hour_roller, &roller_style, 0);
    lv_obj_set_style_bg_opa(hour_roller, LV_OPA_TRANSP, LV_PART_SELECTED);

    char *hour = "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n11\n12\n13\n14\n15\n16\n17\n"
            "18\n19\n20\n21\n22\n23";

    lv_roller_set_options(hour_roller, hour, LV_ROLLER_MODE_NORMAL);

    lv_obj_align_to(hour_roller, cancel_btn, LV_ALIGN_OUT_BOTTOM_LEFT, -15, 15);
    lv_roller_set_visible_row_count(hour_roller, 3);
    lv_obj_add_event_cb(hour_roller, mask_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *minute_roller = lv_roller_create(main_cont);
    lv_obj_set_size(minute_roller, 240, 360);
    lv_obj_add_style(minute_roller, &roller_style, 0);
    lv_obj_set_style_bg_opa(minute_roller, LV_OPA_TRANSP, LV_PART_SELECTED);

    char *minute =
            "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n11\n12\n13\n14\n15\n16\n17\n18\n"
                    "19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n"
                    "34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n"
                    "49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59";

    lv_roller_set_options(minute_roller, minute, LV_ROLLER_MODE_NORMAL);

    lv_obj_align_to(minute_roller, save_btn, LV_ALIGN_OUT_BOTTOM_RIGHT, 15, 15);
    lv_roller_set_visible_row_count(minute_roller, 3);
    lv_obj_add_event_cb(minute_roller, mask_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *alarm_set_btn1 = lv_box_alarm_set_btn_create(main_cont);
    lv_obj_set_size(alarm_set_btn1, 480, 80);
    lv_box_alarm_set_btn_src(alarm_set_btn1, &lv_box_res.style_ft_30,
            &lv_box_res.style_ft_30, &lv_box_res.set_bg_style,
            "A:/usr/share/lv_86_boxes/right_arrow.png", "Loop", "Never");
    lv_obj_align_to(alarm_set_btn1, hour_roller, LV_ALIGN_OUT_BOTTOM_LEFT, 0,
            15);

    lv_obj_t *alarm_set_btn2 = lv_box_alarm_set_btn_create(main_cont);
    lv_obj_set_size(alarm_set_btn2, 480, 80);
    lv_box_alarm_set_btn_src(alarm_set_btn2, &lv_box_res.style_ft_30,
            &lv_box_res.style_ft_30, &lv_box_res.set_bg_style,
            "A:/usr/share/lv_86_boxes/right_arrow.png", "Ring", "Crisp");
    lv_obj_align_to(alarm_set_btn2, alarm_set_btn1, LV_ALIGN_OUT_BOTTOM_LEFT, 0,
            10);

    lv_obj_t *alarm_set_btn3 = lv_box_alarm_set_btn_create(main_cont);
    lv_obj_set_size(alarm_set_btn3, 480, 80);
    lv_box_alarm_set_btn_src(alarm_set_btn3, &lv_box_res.style_ft_30,
            &lv_box_res.style_ft_30, &lv_box_res.set_bg_style,
            "A:/usr/share/lv_86_boxes/right_arrow.png", "Remark", "Get up");
    lv_obj_align_to(alarm_set_btn3, alarm_set_btn2, LV_ALIGN_OUT_BOTTOM_LEFT, 0,
            10);

}
