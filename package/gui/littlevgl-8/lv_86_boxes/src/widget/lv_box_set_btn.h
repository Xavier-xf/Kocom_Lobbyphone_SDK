/*
 * lv_box_set_btn.h
 *
 *  Created on: 2022Äê9ÔÂ21ÈÕ
 *      Author: anruliu
 */

#ifndef LV_86_BOXES_SRC_WIDGET_LV_BOX_SET_BTN_H_
#define LV_86_BOXES_SRC_WIDGET_LV_BOX_SET_BTN_H_

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
    lv_obj_t *name;
    lv_obj_t *right_img;
    lv_obj_t *left_img;
} lv_box_set_btn_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a box btn objects
 * @param par pointer to an object, it will be the parent of the new box btn
 * @return pointer to the created box btn
 */
lv_obj_t* lv_box_set_btn_create(lv_obj_t *parent);

/*=====================
 * Setter functions
 *====================*/

void lv_box_set_btn_src(lv_obj_t *obj, lv_style_t *name_font_style,
        lv_style_t *bg_style, const char *left_src_img,
        const char *right_src_img, const char *name);

void lv_box_set_btn_set_name(lv_obj_t *obj, const char *name);

void lv_box_set_btn_right_img_show(lv_obj_t *obj, const bool on);

/*=====================
 * Getter functions
 *====================*/

char* lv_box_set_btn_get_name(lv_obj_t *obj);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LV_86_BOXES_SRC_WIDGET_LV_BOX_SET_BTN_H_ */
