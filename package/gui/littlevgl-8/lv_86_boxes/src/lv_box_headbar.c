/*
 * lv_box_headbar.c
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#include "lv_box_headbar.h"

#include "common/lv_box_res.h"
#include "activity/lv_box_home_activity.h"
#include "activity/lv_box_lamp_activity.h"
#include "activity/lv_box_news_activity.h"
#include "activity/lv_box_music_activity.h"
#include "activity/lv_box_alarm_activity.h"
#include "activity/lv_box_setting_activity.h"

static lv_obj_t *headbar_activity;

static void event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *parent = lv_obj_get_parent(headbar_activity);
    char *user_data = (char*) lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        lv_box_music_timer_en(false);
        music_activity_is_open = false;
        lv_obj_scroll_to_y(headbar_activity, 460, LV_ANIM_ON);
        if (!strcmp(user_data, "Home")) {
            if (parent == home_activity) {
                lv_tabview_set_act(tabview, 0, LV_ANIM_ON);
            } else {
                lv_obj_set_parent(headbar_activity, home_activity);
                lv_scr_load_anim(home_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                        100, false);
            }
        } else if (!strcmp(user_data, "Lamp")) {
            if (parent == lamp_activity)
                return;
            lv_obj_set_parent(headbar_activity, lamp_activity);
            lv_scr_load_anim(lamp_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                    100, false);
        } else if (!strcmp(user_data, "News")) {
            if (parent == news_activity)
                return;
            lv_obj_set_parent(headbar_activity, news_activity);
            lv_scr_load_anim(news_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                    100, false);
        } else if (!strcmp(user_data, "Music")) {
            music_activity_is_open = true;
            if (parent == music_activity)
                return;
            lv_obj_set_parent(headbar_activity, music_activity);
            lv_scr_load_anim(music_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                    100, false);
            lv_box_music_timer_en(true);
        } else if (!strcmp(user_data, "Alarm")) {
            if (parent == alarm_activity)
                return;
            lv_obj_set_parent(headbar_activity, alarm_activity);
            lv_scr_load_anim(alarm_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                    100, false);
        } else if (!strcmp(user_data, "Setting")) {
            if (parent == setting_activity)
                return;
            lv_obj_set_parent(headbar_activity, setting_activity);
            lv_scr_load_anim(setting_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                    100, false);
        }
    }
}

void lv_box_headbar_init(lv_obj_t *parent) {
    headbar_activity = lv_obj_create(parent);
    lv_obj_clear_flag(headbar_activity, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(headbar_activity, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_style_all(headbar_activity); /*Make it transparent*/
    lv_obj_set_size(headbar_activity, lv_pct(100), lv_pct(100));
    lv_obj_set_scroll_snap_y(headbar_activity, LV_SCROLL_SNAP_END); /*Snap the children to the center*/

    lv_obj_t *placeholder1 = lv_obj_create(headbar_activity);
    lv_obj_set_y(placeholder1, -20);
    lv_obj_set_size(placeholder1, LV_HOR_RES, LV_VER_RES + 20 * 2);
    lv_obj_clear_flag(placeholder1, LV_OBJ_FLAG_SNAPABLE);
    lv_obj_set_style_bg_opa(placeholder1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(placeholder1, 0, 0);
    lv_obj_set_style_pad_all(placeholder1, 0, 0);
    lv_obj_set_scroll_dir(placeholder1, LV_DIR_VER);

    lv_obj_t *placeholder2 = lv_obj_create(headbar_activity);
    lv_obj_remove_style_all(placeholder2);
    lv_obj_clear_flag(placeholder2, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *placeholder3 = lv_obj_create(headbar_activity);
    lv_obj_remove_style_all(placeholder3);
    lv_obj_clear_flag(placeholder3, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_size(placeholder2, lv_pct(100), LV_VER_RES);
    lv_obj_set_y(placeholder2, 0);

    lv_obj_set_size(placeholder3, lv_pct(100), LV_VER_RES - 2 * 20);
    lv_obj_set_y(placeholder3, LV_VER_RES + 20);

    static lv_coord_t col_dsc[] = { 158, 158, 158, LV_GRID_TEMPLATE_LAST };
    static lv_coord_t row_dsc[] = { 158, 158, 158, LV_GRID_TEMPLATE_LAST };

    /*    Create a container with grid*/
    lv_obj_t *cont = lv_obj_create(placeholder2);
    lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);
    lv_obj_set_style_pad_row(cont, 3, 0);
    lv_obj_set_style_pad_column(cont, 3, 0);
    lv_obj_set_size(cont, 480, 322);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x0c0d16), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_90, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);

    char *png_path[] = { "A:/usr/share/lv_86_boxes/home_setting.png",
            "A:/usr/share/lv_86_boxes/lamp_setting.png",
            "A:/usr/share/lv_86_boxes/news_setting.png",
            "A:/usr/share/lv_86_boxes/music_setting.png",
            "A:/usr/share/lv_86_boxes/alarm_setting.png",
            "A:/usr/share/lv_86_boxes/setting_setting.png" };

    char *png_name[] = { "Home", "Lamp", "News", "Music", "Alarm", "Setting" };

    uint32_t i;
    for (i = 0; i < 6; i++) {
        uint8_t col = i % 3;
        uint8_t row = i / 3;

        lv_obj_t *btn = lv_btn_create(cont);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x33343c), 0);
        lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1,
                LV_GRID_ALIGN_STRETCH, row, 1);
        lv_obj_add_event_cb(btn, event_handler, LV_EVENT_CLICKED, png_name[i]);

        lv_obj_t *item = lv_obj_create(btn);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_pad_all(item, 0, 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_shadow_width(item, 0, 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(item, LV_ALIGN_CENTER, 0, -20);

        lv_obj_t *item_img = lv_img_create(item);
        lv_img_set_src(item_img, png_path[i]);
        lv_obj_center(item_img);

        lv_obj_t *item_label = lv_label_create(item);
        lv_label_set_recolor(item_label, true);
        lv_label_set_text_fmt(item_label, "#b1adbb %s #", png_name[i]);
        lv_obj_add_style(item_label, &lv_box_res.style_ft_26, 0);
        lv_obj_align_to(item_label, item_img, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    }

    lv_obj_update_layout(headbar_activity);
    lv_obj_scroll_to_y(headbar_activity, 460, LV_ANIM_OFF);
}
