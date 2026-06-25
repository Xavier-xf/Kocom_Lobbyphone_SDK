/*
 * lv_box_music_item.h
 *
 *  Created on: 2022Äê10ÔÂ10ÈÕ
 *      Author: anruliu
 */

#ifndef LV_86_BOXES_SRC_WIDGET_LV_BOX_MUSIC_ITEM_H_
#define LV_86_BOXES_SRC_WIDGET_LV_BOX_MUSIC_ITEM_H_

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
    lv_timer_t *timer;
    char **anim_img;
    bool mode;
} lv_box_music_item_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a box btn objects
 * @param par pointer to an object, it will be the parent of the new box btn
 * @return pointer to the created box btn
 */
lv_obj_t* lv_box_music_item_create(lv_obj_t *parent);

/*=====================
 * Setter functions
 *====================*/

void lv_box_music_item_set_src(lv_obj_t *obj, lv_style_t *name_font_style,
        lv_style_t *bg_style, char **anim_img, const char *name);

void lv_box_music_item_set_play(lv_obj_t *obj, bool mode);

/*=====================
 * Getter functions
 *====================*/

bool lv_box_music_item_get_play(lv_obj_t *obj);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* LV_86_BOXES_SRC_WIDGET_LV_BOX_MUSIC_ITEM_H_ */
