/*
 * lv_box_music_activity.c
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#include "lv_box_music_activity.h"
#include "lv_box_music_list_activity.h"
#include "../common/lv_box_res.h"
#include "../common/lv_box_res_player_int.h"
#include "../common/lv_box_res_mid_list.h"

static lv_timer_t *timer;
static lv_obj_t *anim_img;
static lv_obj_t *play_imgbtn;
static lv_obj_t *time_bar;
static lv_obj_t *music_lable;
static lv_obj_t *curtime_lable;
static lv_obj_t *totaltime_lable;
static lv_obj_t *music_img;

static char *anim_png_path[] = { "A:/usr/share/lv_86_boxes/muic2_playing01.png",
        "A:/usr/share/lv_86_boxes/muic2_playing02.png",
        "A:/usr/share/lv_86_boxes/muic2_playing03.png",
        "A:/usr/share/lv_86_boxes/muic2_playing04.png" };

static void anim_timer(lv_timer_t *timer) {
    static int index;
    static int16_t angle;
    double percent;
    char totaltime[8], curtime[8];

    index = index >= 3 ? 0 : ++index;
    angle = angle >= 3600 ? 0: 100 + angle;
    lv_img_set_src(anim_img, anim_png_path[index]);
    lv_img_set_angle(music_img, angle);

    lv_box_res_music_get_percent(&percent);
    lv_box_res_music_get_time(curtime, totaltime);

    lv_bar_set_value(time_bar, percent * 100, LV_ANIM_OFF);

    if (lv_box_res_music_get_media_list())
        lv_label_set_text_fmt(music_lable, "#ffffff %s #",
                lv_box_res_music_get_media_list()->current_node->name);
    lv_label_set_text_fmt(curtime_lable, "#ffffff %s #", curtime);
    lv_label_set_text_fmt(totaltime_lable, "#ffffff %s #", totaltime);
}

static void event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    char *user_data = (char*) lv_event_get_user_data(e);

    if (!strcmp(user_data, "play")) {
        if (lv_obj_has_state(obj, LV_STATE_CHECKED)) {
            lv_box_res_music_play();
            lv_timer_resume(timer);
        } else {
            lv_box_res_music_pause();
            lv_timer_pause(timer);
        }
    } else if (!strcmp(user_data, "pre")) {
        lv_obj_add_state(play_imgbtn, LV_STATE_CHECKED);
        lv_obj_invalidate(play_imgbtn);
        lv_timer_resume(timer);
        lv_box_res_music_play_mode(0, 0);
    } else if (!strcmp(user_data, "next")) {
        lv_obj_add_state(play_imgbtn, LV_STATE_CHECKED);
        lv_obj_invalidate(play_imgbtn);
        lv_timer_resume(timer);
        lv_box_res_music_play_mode(1, 0);
    } else if (!strcmp(user_data, "volume_down")) {
        int volume;
        volume = lv_box_res_music_get_volumn();
        if (volume >= 2) {
            volume = volume - 2;
            lv_box_res_music_set_volumn(volume);
        }
    } else if (!strcmp(user_data, "volume_up")) {
        int volume;
        volume = lv_box_res_music_get_volumn();
        if (volume <= 40) {
            volume = volume + 2;
            lv_box_res_music_set_volumn(volume);
        }
    } else if (!strcmp(user_data, "list")) {
        lv_box_music_list_init();
        lv_scr_load_anim(music_list_activity, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200,
                100, false);
    }
}

void lv_box_music_timer_en(bool en) {
    if (en) {
        if (lv_box_res_music_get_playing()) {
            lv_timer_resume(timer);
            lv_obj_add_state(play_imgbtn, LV_STATE_CHECKED);
            lv_obj_invalidate(play_imgbtn);
        }
    } else {
        if (music_activity_is_open) {
            lv_timer_pause(timer);
            lv_obj_clear_state(play_imgbtn, LV_STATE_CHECKED);
            lv_obj_invalidate(play_imgbtn);
        }
    }
}

void lv_box_music_init(void) {
    music_activity = lv_obj_create(NULL);
    music_activity_is_open = false;
    lv_obj_set_style_bg_color(music_activity, lv_color_hex(0x10111a), 0);

    lv_obj_t *title_lable = lv_label_create(music_activity);
    lv_obj_set_pos(title_lable, 15, 5);
    lv_obj_add_style(title_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_recolor(title_lable, true);
    lv_label_set_text_fmt(title_lable, "#ffffff %s #", "Music");

    anim_img = lv_img_create(music_activity);
    lv_img_set_src(anim_img, anim_png_path[0]);
    lv_obj_align(anim_img, LV_ALIGN_TOP_RIGHT, -10, 10);

    lv_obj_t *main_cont = lv_obj_create(music_activity);
    lv_obj_set_pos(main_cont, 0, 60);
    lv_obj_set_size(main_cont, 480, 420);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_shadow_width(main_cont, 0, 0);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(main_cont, lv_color_hex(0x10111a), 0);

    music_lable = lv_label_create(main_cont);
    lv_label_set_long_mode(music_lable, 2);
    lv_obj_set_width(music_lable, 340);
    lv_label_set_recolor(music_lable, true);
    lv_obj_add_style(music_lable, &lv_box_res.style_ft_30, 0);
    lv_label_set_text_fmt(music_lable, "#ffffff %s #", "Not Music Play");
    lv_obj_align(music_lable, LV_ALIGN_CENTER, 40, -160);

    music_img = lv_img_create(main_cont);
    lv_img_set_src(music_img, "A:/usr/share/lv_86_boxes/music.png");
    lv_obj_align_to(music_img, music_lable, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    /*Create an image button*/
    play_imgbtn = lv_imgbtn_create(main_cont);
    lv_imgbtn_set_src(play_imgbtn, LV_IMGBTN_STATE_RELEASED, NULL,
            "A:/usr/share/lv_86_boxes/music_play.png", NULL);
    lv_imgbtn_set_src(play_imgbtn, LV_IMGBTN_STATE_CHECKED_RELEASED, NULL,
            "A:/usr/share/lv_86_boxes/music_stop.png", NULL);
    lv_obj_add_flag(play_imgbtn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_width(play_imgbtn, 80);
    lv_obj_align(play_imgbtn, LV_ALIGN_CENTER, 0, -30);
    lv_obj_add_event_cb(play_imgbtn, event_cb, LV_EVENT_CLICKED, "play");

    lv_obj_t *pre_imgbtn = lv_imgbtn_create(main_cont);
    lv_imgbtn_set_src(pre_imgbtn, LV_IMGBTN_STATE_RELEASED, NULL,
            "A:/usr/share/lv_86_boxes/music_back_nol.png", NULL);
    lv_imgbtn_set_src(pre_imgbtn, LV_IMGBTN_STATE_PRESSED, NULL,
            "A:/usr/share/lv_86_boxes/music_back_press.png", NULL);
    lv_obj_set_width(pre_imgbtn, 64);
    lv_obj_align_to(pre_imgbtn, play_imgbtn, LV_ALIGN_OUT_LEFT_MID, -22, 0);
    lv_obj_add_event_cb(pre_imgbtn, event_cb, LV_EVENT_CLICKED, "pre");

    lv_obj_t *next_imgbtn = lv_imgbtn_create(main_cont);
    lv_imgbtn_set_src(next_imgbtn, LV_IMGBTN_STATE_RELEASED, NULL,
            "A:/usr/share/lv_86_boxes/music_next_nol.png", NULL);
    lv_imgbtn_set_src(next_imgbtn, LV_IMGBTN_STATE_PRESSED, NULL,
            "A:/usr/share/lv_86_boxes/music_next_press.png", NULL);
    lv_obj_set_width(next_imgbtn, 64);
    lv_obj_align_to(next_imgbtn, play_imgbtn, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
    lv_obj_add_event_cb(next_imgbtn, event_cb, LV_EVENT_CLICKED, "next");

    lv_obj_t *volume_imgbtn1 = lv_imgbtn_create(main_cont);
    lv_imgbtn_set_src(volume_imgbtn1, LV_IMGBTN_STATE_RELEASED, NULL,
            "A:/usr/share/lv_86_boxes/music_volume-_nol.png", NULL);
    lv_imgbtn_set_src(volume_imgbtn1, LV_IMGBTN_STATE_PRESSED, NULL,
            "A:/usr/share/lv_86_boxes/music_volume-_press.png", NULL);
    lv_obj_set_width(volume_imgbtn1, 48);
    lv_obj_align_to(volume_imgbtn1, pre_imgbtn, LV_ALIGN_OUT_BOTTOM_LEFT, -60,
            40);
    lv_obj_add_event_cb(volume_imgbtn1, event_cb, LV_EVENT_CLICKED,
            "volume_down");

    lv_obj_t *mode_imgbtn = lv_imgbtn_create(main_cont);
    lv_imgbtn_set_src(mode_imgbtn, LV_IMGBTN_STATE_RELEASED, NULL,
            "A:/usr/share/lv_86_boxes/music_mode01.png", NULL);
    lv_imgbtn_set_src(mode_imgbtn, LV_IMGBTN_STATE_CHECKED_RELEASED, NULL,
            "A:/usr/share/lv_86_boxes/music_mode02.png", NULL);
    lv_obj_add_flag(mode_imgbtn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_width(mode_imgbtn, 48);
    lv_obj_align_to(mode_imgbtn, volume_imgbtn1, LV_ALIGN_OUT_RIGHT_MID, 50, 0);
    lv_obj_add_event_cb(mode_imgbtn, event_cb, LV_EVENT_CLICKED, "mode");

    lv_obj_t *volume_imgbtn2 = lv_imgbtn_create(main_cont);
    lv_imgbtn_set_src(volume_imgbtn2, LV_IMGBTN_STATE_RELEASED, NULL,
            "A:/usr/share/lv_86_boxes/music_volume+_nol.png", NULL);
    lv_imgbtn_set_src(volume_imgbtn2, LV_IMGBTN_STATE_PRESSED, NULL,
            "A:/usr/share/lv_86_boxes/music_volume+_press.png", NULL);
    lv_obj_set_width(volume_imgbtn2, 48);
    lv_obj_align_to(volume_imgbtn2, next_imgbtn, LV_ALIGN_OUT_BOTTOM_RIGHT, 60,
            40);
    lv_obj_add_event_cb(volume_imgbtn2, event_cb, LV_EVENT_CLICKED,
            "volume_up");

    lv_obj_t *list_imgbtn = lv_imgbtn_create(main_cont);
    lv_imgbtn_set_src(list_imgbtn, LV_IMGBTN_STATE_RELEASED, NULL,
            "A:/usr/share/lv_86_boxes/music_list_nol.png", NULL);
    lv_imgbtn_set_src(list_imgbtn, LV_IMGBTN_STATE_PRESSED, NULL,
            "A:/usr/share/lv_86_boxes/music_list_press.png", NULL);
    lv_obj_set_width(list_imgbtn, 48);
    lv_obj_align_to(list_imgbtn, volume_imgbtn2, LV_ALIGN_OUT_LEFT_MID, -50, 0);
    lv_obj_add_event_cb(list_imgbtn, event_cb, LV_EVENT_CLICKED, "list");

    time_bar = lv_bar_create(main_cont);
    lv_obj_set_size(time_bar, 360, 10);
    lv_obj_align_to(time_bar, volume_imgbtn1, LV_ALIGN_BOTTOM_LEFT, 0, 40);
    lv_bar_set_value(time_bar, 0, LV_ANIM_OFF);

    curtime_lable = lv_label_create(main_cont);
    lv_label_set_recolor(curtime_lable, true);
    lv_obj_add_style(curtime_lable, &lv_box_res.style_ft_10, 0);
    lv_label_set_text_fmt(curtime_lable, "#ffffff %s #", "00:00");
    lv_obj_align_to(curtime_lable, time_bar, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    totaltime_lable = lv_label_create(main_cont);
    lv_label_set_recolor(totaltime_lable, true);
    lv_obj_add_style(totaltime_lable, &lv_box_res.style_ft_10, 0);
    lv_label_set_text_fmt(totaltime_lable, "#ffffff %s #", "00:00");
    lv_obj_align_to(totaltime_lable, time_bar, LV_ALIGN_OUT_BOTTOM_RIGHT, 4, 0);

    timer = lv_timer_create(anim_timer, 100, NULL);
    lv_timer_pause(timer);
}
