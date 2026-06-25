/*
 * lv_box_alarm_item.h
 *
 *  Created on: 2022Äê9ÔÂ14ÈÕ
 *      Author: anruliu
 */

#ifndef LV_86_BOXES_SRC_WIDGET_LV_BOX_ALARM_ITEM_H_
#define LV_86_BOXES_SRC_WIDGET_LV_BOX_ALARM_ITEM_H_

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl/src/lv_conf_internal.h"
#include "lvgl/src/core/lv_obj.h"

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/*Data of box_btn*/
typedef struct {
    lv_obj_t obj;

    lv_obj_t *btn;
    lv_obj_t *tiem;
    lv_obj_t *des;
    lv_obj_t *loop;
    lv_obj_t *sw;
} lv_box_alarm_item_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a box btn objects
 * @param par pointer to an object, it will be the parent of the new box btn
 * @return pointer to the created box btn
 */
lv_obj_t* lv_box_alarm_item_create(lv_obj_t *parent);

/*=====================
 * Setter functions
 *====================*/

void lv_box_alarm_item_set_src(lv_obj_t *obj, lv_style_t *time_font_style,
        lv_style_t *des_font_style, lv_style_t *loop_font_style,
        lv_style_t *bg_style, bool state);

/*=====================
 * Getter functions
 *====================*/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LV_86_BOXES_SRC_WIDGET_LV_BOX_ALARM_ITEM_H_ */
