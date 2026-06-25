/*
 * lv_box_wifi_activity.h
 *
 *  Created on: 2022Äê9ÔÂ22ÈÕ
 *      Author: anruliu
 */

#ifndef LV_86_BOXES_SRC_ACTIVITY_LV_BOX_WIFI_ACTIVITY_H_
#define LV_86_BOXES_SRC_ACTIVITY_LV_BOX_WIFI_ACTIVITY_H_

#include "lvgl/lvgl.h"

lv_obj_t *wifi_activity;

void lv_box_wifi_init(void);
void lv_box_wifi_connect(const char* password);

#endif /* LV_86_BOXES_SRC_ACTIVITY_LV_BOX_WIFI_ACTIVITY_H_ */
