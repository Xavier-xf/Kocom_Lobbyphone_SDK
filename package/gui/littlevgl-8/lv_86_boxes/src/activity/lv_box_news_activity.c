/*
 * lv_box_news_activity.c
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#include "lv_box_news_activity.h"
#include "../common/lv_box_res.h"

void lv_box_news_init(void) {
    news_activity = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(news_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *title_lable = lv_label_create(news_activity);
    lv_obj_set_pos(title_lable, 15, 5);
    lv_obj_add_style(title_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(title_lable, true);
    lv_label_set_text_fmt(title_lable, "#ffffff %s #", "News");

    lv_obj_t *main_cont = lv_obj_create(news_activity);
    lv_obj_set_pos(main_cont, 0, 60);
    lv_obj_set_size(main_cont, 480, 420);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_shadow_width(main_cont, 0, 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x10111a), 0);

    lv_obj_t *bg_img = lv_img_create(news_activity);
    lv_img_set_src(bg_img, "A:/usr/share/lv_86_boxes/message.png");
    lv_obj_center(bg_img);

    lv_obj_t *news_lable = lv_label_create(main_cont);
    lv_obj_align_to(news_lable, bg_img, LV_ALIGN_OUT_BOTTOM_LEFT, 35, 0);
    lv_obj_add_style(news_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(news_lable, true);
    lv_label_set_text_fmt(news_lable, "#ffffff %s #", "No news yet");
}
