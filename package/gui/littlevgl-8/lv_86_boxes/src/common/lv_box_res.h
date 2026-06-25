/*
 * lv_box_res.h
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#ifndef LV_86_BOXES_SRC_COMMON_LV_BOX_RES_H_
#define LV_86_BOXES_SRC_COMMON_LV_BOX_RES_H_

#include "lvgl/lvgl.h"
#include <pthread.h>

typedef struct {
    lv_ft_info_t ft_info_100;
    lv_ft_info_t ft_info_50;
    lv_ft_info_t ft_info_40;
    lv_ft_info_t ft_info_30;
    lv_ft_info_t ft_info_26;
    lv_ft_info_t ft_info_10;

    lv_style_t style_ft_100;
    lv_style_t style_ft_40;
    lv_style_t style_ft_30;
    lv_style_t style_ft_26;
    lv_style_t style_ft_10;

    lv_style_t lamp_bg_style;
    lv_style_t set_bg_style;
    lv_style_t tabview_bg_style;
} lv_box_res_t;

lv_box_res_t lv_box_res;
pthread_mutex_t lvgl_mutex;

void lv_box_res_init(void);

#endif /* LV_86_BOXES_SRC_COMMON_LV_BOX_RES_H_ */
