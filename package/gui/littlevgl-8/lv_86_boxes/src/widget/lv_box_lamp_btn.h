/*
 * lv_box_lamp_btn.h
 *
 *  Created on: 2022Äê8ÔÂ8ÈÕ
 *      Author: anruliu
 */

#ifndef LV_86_BOXES_SRC_WIDGET_LV_BOX_LAMP_BTN_H_
#define LV_86_BOXES_SRC_WIDGET_LV_BOX_LAMP_BTN_H_

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
    lv_obj_t *img;
    lv_obj_t *name;
    lv_obj_t *state;

    const void *src_on_img;
    const void *src_off_img;
} lv_box_lamp_btn_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a box btn objects
 * @param par pointer to an object, it will be the parent of the new box btn
 * @return pointer to the created box btn
 */
lv_obj_t* lv_box_lamp_btn_create(lv_obj_t *parent);
void lv_box_lamp_btn_set_state(lv_obj_t *obj, bool checked);

/*=====================
 * Setter functions
 *====================*/

void lv_box_lamp_btn_set_src(lv_obj_t *obj, const char *src_on_img,
        const char *src_off_img, const char *name, bool state,
        lv_style_t *font_style, lv_style_t *bg_style);

/*=====================
 * Getter functions
 *====================*/

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LV_86_BOXES_SRC_WIDGET_LV_BOX_LAMP_BTN_H_ */
