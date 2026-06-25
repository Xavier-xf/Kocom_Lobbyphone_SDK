/*
 * lv_box_set_btn.c
 *
 *  Created on: 2022Äê9ÔÂ21ÈÕ
 *      Author: anruliu
 */

#include "lv_box_set_btn.h"
#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_box_set_btn_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_box_set_btn_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lv_box_set_btn_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_box_set_btn_class = {
    .constructor_cb = lv_box_set_btn_constructor,
    .destructor_cb = lv_box_set_btn_destructor,
    .width_def = LV_SIZE_CONTENT,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(lv_box_set_btn_t),
    .base_class = &lv_obj_class
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t* lv_box_set_btn_create(lv_obj_t *parent) {
    LV_LOG_INFO("begin");
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/*=====================
 * Setter functions
 *====================*/

void lv_box_set_btn_src(lv_obj_t *obj, lv_style_t *name_font_style,
        lv_style_t *bg_style, const char *left_src_img,
        const char *right_src_img, const char *name) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_set_btn_t *btn = (lv_box_set_btn_t*) obj;

    lv_label_set_recolor(btn->name, true);
    lv_obj_update_layout(obj);
    lv_obj_set_size(btn->btn, lv_obj_get_width(obj), lv_obj_get_height(obj));

    lv_obj_add_style(btn->btn, bg_style, 0);
    lv_obj_add_style(btn->name, name_font_style, 0);

    lv_img_set_src(btn->left_img, left_src_img);
    lv_img_set_src(btn->right_img, right_src_img);

    lv_label_set_long_mode(btn->name, 2);
    lv_obj_set_width(btn->name, lv_obj_get_width(obj) - 100);
    lv_label_set_text_fmt(btn->name, "#ffffff %s #", name);

    lv_obj_align(btn->left_img, LV_ALIGN_LEFT_MID, 15, 0);
    lv_obj_align_to(btn->name, btn->left_img, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
    lv_obj_align(btn->right_img, LV_ALIGN_RIGHT_MID, -15, 0);

    lv_obj_refresh_self_size(obj);

    lv_obj_invalidate(obj);
}

void lv_box_set_btn_set_name(lv_obj_t *obj, const char *name) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_set_btn_t *btn = (lv_box_set_btn_t*) obj;
    lv_label_set_text_fmt(btn->name, "#ffffff %s #", name);
}

void lv_box_set_btn_right_img_show(lv_obj_t *obj, const bool on) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_set_btn_t *btn = (lv_box_set_btn_t*) obj;
    if (on) {
        lv_obj_clear_flag(btn->right_img, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(btn->right_img, LV_OBJ_FLAG_HIDDEN);
    }
}

/*=====================
 * Getter functions
 *====================*/

char* lv_box_set_btn_get_name(lv_obj_t *obj) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_set_btn_t *btn = (lv_box_set_btn_t*) obj;
    return lv_label_get_text(btn->name);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_box_set_btn_constructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);LV_TRACE_OBJ_CREATE("begin");

    lv_box_set_btn_t *btn = (lv_box_set_btn_t*) obj;

    btn->btn = lv_btn_create(obj);
    btn->name = lv_label_create(obj);
    btn->left_img = lv_img_create(obj);
    btn->right_img = lv_img_create(obj);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn->btn, LV_OBJ_FLAG_EVENT_BUBBLE);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_box_set_btn_destructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);
    lv_box_set_btn_t *btn = (lv_box_set_btn_t*) obj;
}
