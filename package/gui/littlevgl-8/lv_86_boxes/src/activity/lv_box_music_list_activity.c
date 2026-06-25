/*
 * lv_box_music_list_activity.c
 *
 *  Created on: 2022Äê10ÔÂ10ÈÕ
 *      Author: anruliu
 */

#include "lv_box_music_list_activity.h"
#include "lv_box_music_activity.h"
#include "../widget/lv_box_music_item.h"
#include "../common/lv_box_res.h"
#include "../common/lv_box_res_player_int.h"

static lv_obj_t *music_item[64];

static void event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    char *user_data = (char*) lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        if (!strcmp(user_data, "Croos")) {
            if (lv_box_res_music_get_playing()) {
                lv_box_music_timer_en(true);
            }
            lv_scr_load_anim(music_activity, LV_SCR_LOAD_ANIM_NONE, 200, 100,
                    true);
        } else if (!strcmp(user_data, "Item")) {
            list_head_t *my_media_list = lv_box_res_music_get_media_list();
            for (int i = 0; i < list_get_total_num(my_media_list); i++) {
                if (lv_box_music_item_get_play(music_item[i])) {
                    lv_box_music_item_set_play(music_item[i], false);
                }

                if (lv_obj_get_parent(obj) == music_item[i]) {
                    lv_box_music_item_set_play(lv_obj_get_parent(obj), true);
                    lv_box_res_music_play_mode(2, i + 1);
                }
            }
        }
    }
}

void lv_box_music_list_init(void) {
    music_list_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(music_list_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *title_lable = lv_label_create(music_list_activity);
    lv_obj_set_pos(title_lable, 15, 5);
    lv_obj_add_style(title_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(title_lable, true);
    lv_label_set_text_fmt(title_lable, "#ffffff %s #", "Music list");

    lv_obj_t *cross_img = lv_img_create(music_list_activity);
    lv_obj_center(cross_img);
    lv_img_set_src(cross_img, "A:/usr/share/lv_86_boxes/cross.png");
    lv_obj_align(cross_img, LV_ALIGN_TOP_RIGHT, -15, 15);

    lv_obj_t *click_rect = lv_obj_create(music_list_activity);
    lv_obj_remove_style_all(click_rect);
    lv_obj_set_size(click_rect, 60, 60);
    lv_obj_align(click_rect, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(click_rect, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(click_rect, event_cb, LV_EVENT_CLICKED, "Croos");

    lv_obj_t *main_cont = lv_obj_create(music_list_activity);
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
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);

    list_head_t *my_media_list = lv_box_res_music_get_media_list();
    int cur_index = list_get_cur_index(my_media_list);
    list_skip_to_head(my_media_list);

    static char *anim_png_path[] = {
            "A:/usr/share/lv_86_boxes/muic2_playing01.png",
            "A:/usr/share/lv_86_boxes/muic2_playing02.png",
            "A:/usr/share/lv_86_boxes/muic2_playing03.png",
            "A:/usr/share/lv_86_boxes/muic2_playing04.png" };

    for (int i = 0; i < list_get_total_num(my_media_list); i++) {
        music_item[i] = lv_box_music_item_create(main_cont);
        lv_obj_set_size(music_item[i], 480, 80);
        lv_box_music_item_set_src(music_item[i], &lv_box_res.style_ft_26,
                &lv_box_res.set_bg_style, anim_png_path,
                my_media_list->current_node->name);
        lv_obj_add_event_cb(music_item[i], event_cb, LV_EVENT_CLICKED, "Item");
        list_get_next_node(my_media_list);
    }

    if (lv_box_res_music_get_playing()) {
        lv_box_music_item_set_play(music_item[cur_index - 1], true);
    }

    list_skip_to_index(my_media_list, cur_index);

}
