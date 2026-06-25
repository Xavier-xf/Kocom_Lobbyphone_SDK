/*
 * lv_box_wifi_item.c
 *
 *  Created on: 2022Äê9ÔÂ27ÈÕ
 *      Author: anruliu
 */

#include "lv_box_wifi_item.h"
#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_box_wifi_item_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_box_wifi_item_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lv_box_wifi_item_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_box_wifi_item_class = {
    .constructor_cb = lv_box_wifi_item_constructor,
    .destructor_cb = lv_box_wifi_item_destructor,
    .width_def = LV_SIZE_CONTENT,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(lv_box_wifi_item_t),
    .base_class = &lv_obj_class
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t* lv_box_wifi_item_create(lv_obj_t *parent) {
    LV_LOG_INFO("begin");
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/*=====================
 * Setter functions
 *====================*/

void lv_box_wifi_item_set_src(lv_obj_t *obj, lv_style_t *name_font_style,
        lv_style_t *bg_style, const char *left_src_img, const char *name) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_wifi_item_t *item = (lv_box_wifi_item_t*) obj;

    lv_obj_update_layout(obj);
    lv_obj_set_size(item->btn, lv_obj_get_width(obj), lv_obj_get_height(obj));

    lv_obj_add_style(item->btn, bg_style, 0);
    lv_obj_add_style(item->name, name_font_style, 0);

    lv_img_set_src(item->left_img, left_src_img);

    lv_label_set_long_mode(item->name, 2);
    lv_obj_set_width(item->name, lv_obj_get_width(obj) - 100);
    lv_label_set_text_fmt(item->name, "%s", name);
    lv_obj_set_style_text_color(item->name, lv_color_white(), 0);

    lv_obj_align(item->left_img, LV_ALIGN_LEFT_MID, 15, 0);
    lv_obj_align_to(item->name, item->left_img, LV_ALIGN_OUT_RIGHT_MID, 15, 0);

    lv_obj_refresh_self_size(obj);

    lv_obj_invalidate(obj);
}

void lv_box_wifi_item_set_name(lv_obj_t *obj, const char *name) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_wifi_item_t *item = (lv_box_wifi_item_t*) obj;
    lv_label_set_text_fmt(item->name, "%s", name);
}

/*=====================
 * Getter functions
 *====================*/

char* lv_box_wifi_item_get_name(lv_obj_t *obj) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_wifi_item_t *item = (lv_box_wifi_item_t*) obj;
    return lv_label_get_text(item->name);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_box_wifi_item_constructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);LV_TRACE_OBJ_CREATE("begin");

    lv_box_wifi_item_t *item = (lv_box_wifi_item_t*) obj;

    item->btn = lv_btn_create(obj);
    item->name = lv_label_create(obj);
    item->left_img = lv_img_create(obj);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item->btn, LV_OBJ_FLAG_EVENT_BUBBLE);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_box_wifi_item_destructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);
    lv_box_wifi_item_t *item = (lv_box_wifi_item_t*) obj;
}
