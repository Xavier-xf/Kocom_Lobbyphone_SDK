/*
 * lv_box_music_activity.h
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#ifndef LV_86_BOXES_SRC_ACTIVITY_LV_BOX_MUSIC_ACTIVITY_H_
#define LV_86_BOXES_SRC_ACTIVITY_LV_BOX_MUSIC_ACTIVITY_H_

#include "lvgl/lvgl.h"

lv_obj_t *music_activity;
bool music_activity_is_open;

void lv_box_music_init(void);
void lv_box_music_timer_en(bool en);

#endif /* LV_86_BOXES_SRC_ACTIVITY_LV_BOX_MUSIC_ACTIVITY_H_ */
