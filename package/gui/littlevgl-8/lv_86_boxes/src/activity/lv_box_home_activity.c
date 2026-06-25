/*
 * lv_box_home_activity.c
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#include "lv_box_home_activity.h"
#include "../common/lv_box_res.h"
#include "../widget/lv_box_lamp_btn.h"

void lv_box_home_init(void) {
    home_activity = lv_obj_create(NULL);
    lv_scr_load(home_activity);

    lv_obj_t *img_screen_bg = lv_img_create(home_activity);
    lv_img_set_src(img_screen_bg, "A:/usr/share/lv_86_boxes/screen_bg.png");
    lv_obj_center(img_screen_bg);

    /*Create a Tab view object*/
    tabview = lv_tabview_create(home_activity, LV_DIR_TOP, 0);
    lv_obj_add_style(tabview, &lv_box_res.tabview_bg_style, 0);

    /*Add 3 tabs (the tabs are page (lv_page) and can be scrolled*/
    lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "Tab 1");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "Tab 2");
    lv_obj_t *tab3 = lv_tabview_add_tab(tabview, "Tab 3");

    lv_obj_set_scrollbar_mode(tab1, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollbar_mode(tab2, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollbar_mode(tab3, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *time_label = lv_label_create(tab1);
    lv_obj_add_style(time_label, &lv_box_res.style_ft_100, 0);
    lv_label_set_recolor(time_label, true);
    lv_label_set_text_fmt(time_label, "#ffffff %s #", "07:59");
    lv_obj_set_pos(time_label, 10, 0);

    lv_obj_t *weather_img = lv_img_create(tab1);
    lv_img_set_src(weather_img, "A:/usr/share/lv_86_boxes/weather_sunny.png");
    lv_obj_align_to(weather_img, time_label, LV_ALIGN_OUT_RIGHT_MID, 30, 10);

    lv_obj_t *date_label = lv_label_create(tab1);
    lv_obj_add_style(date_label, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(date_label, true);
    lv_label_set_text_fmt(date_label, "#ffffff %s #", "Sep 23, Wed");
    lv_obj_align_to(date_label, time_label, LV_ALIGN_OUT_BOTTOM_LEFT, 5, 0);

    lv_obj_t *temp_label = lv_label_create(tab1);
    lv_obj_add_style(temp_label, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(temp_label, true);
    lv_label_set_text_fmt(temp_label, "#ffffff %s #", "20\u00b0");
    lv_obj_align_to(temp_label, date_label, LV_ALIGN_OUT_RIGHT_MID, 170, 0);

    lv_obj_t *lamp_btn1 = lv_box_lamp_btn_create(tab1);
    lv_obj_set_size(lamp_btn1, 200, 120);
    lv_obj_align(lamp_btn1, LV_ALIGN_BOTTOM_LEFT, 15, -20);
    lv_box_lamp_btn_set_src(lamp_btn1, "A:/usr/share/lv_86_boxes/lamp_on.png",
            "A:/usr/share/lv_86_boxes/lamp_off.png", "Light 1", true,
            &lv_box_res.style_ft_26, &lv_box_res.lamp_bg_style);

    lv_obj_t *lamp_btn2 = lv_box_lamp_btn_create(tab1);
    lv_obj_set_size(lamp_btn2, 200, 120);
    lv_obj_align_to(lamp_btn2, lamp_btn1, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_box_lamp_btn_set_src(lamp_btn2, "A:/usr/share/lv_86_boxes/lamp_on.png",
            "A:/usr/share/lv_86_boxes/lamp_off.png", "Light 2", false,
            &lv_box_res.style_ft_26, &lv_box_res.lamp_bg_style);

    lv_obj_t *lamp_btn3 = lv_box_lamp_btn_create(tab1);
    lv_obj_set_size(lamp_btn3, 418, 120);
    lv_obj_align_to(lamp_btn3, lamp_btn1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 40);
    lv_box_lamp_btn_set_src(lamp_btn3, "A:/usr/share/lv_86_boxes/lamp_on.png",
            "A:/usr/share/lv_86_boxes/lamp_off.png", "Light 3", false,
            &lv_box_res.style_ft_26, &lv_box_res.lamp_bg_style);

    lv_obj_t *lamp_btn4 = lv_box_lamp_btn_create(tab1);
    lv_obj_set_size(lamp_btn4, 200, 120);
    lv_obj_align_to(lamp_btn4, lamp_btn3, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);
    lv_box_lamp_btn_set_src(lamp_btn4, "A:/usr/share/lv_86_boxes/lamp_on.png",
            "A:/usr/share/lv_86_boxes/lamp_off.png", "Light 4", true,
            &lv_box_res.style_ft_26, &lv_box_res.lamp_bg_style);

    lv_obj_t *lamp_btn5 = lv_box_lamp_btn_create(tab1);
    lv_obj_set_size(lamp_btn5, 200, 120);
    lv_obj_align_to(lamp_btn5, lamp_btn4, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_box_lamp_btn_set_src(lamp_btn5, "A:/usr/share/lv_86_boxes/lamp_on.png",
            "A:/usr/share/lv_86_boxes/lamp_off.png", "Light 5", false,
            &lv_box_res.style_ft_26, &lv_box_res.lamp_bg_style);

    lv_obj_t *qr_code1_img = lv_img_create(tab2);
    lv_img_set_src(qr_code1_img, "A:/usr/share/lv_86_boxes/qr_code1.png");
    lv_obj_center(qr_code1_img);

    lv_obj_t *qr_code_img = lv_img_create(tab3);
    lv_img_set_src(qr_code_img, "A:/usr/share/lv_86_boxes/qr_code.png");
    lv_obj_center(qr_code_img);
}
