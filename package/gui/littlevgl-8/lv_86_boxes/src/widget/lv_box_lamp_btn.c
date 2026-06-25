/*
 * lv_box_lamp_btn.c
 *
 *  Created on: 2022Äê8ÔÂ8ÈÕ
 *      Author: anruliu
 */

#include "lv_box_lamp_btn.h"
#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_box_lamp_btn_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_box_lamp_btn_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lv_box_lamp_btn_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lv_box_lamp_btn_event(lv_event_t *e);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_box_lamp_btn_class = {
    .constructor_cb = lv_box_lamp_btn_constructor,
    .destructor_cb = lv_box_lamp_btn_destructor,
    .width_def = LV_SIZE_CONTENT,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(lv_box_lamp_btn_t),
    .base_class = &lv_obj_class
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t* lv_box_lamp_btn_create(lv_obj_t *parent) {
    LV_LOG_INFO("begin");
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/*=====================
 * Setter functions
 *====================*/

void lv_box_lamp_btn_set_src(lv_obj_t *obj, const char *src_on_img,
        const char *src_off_img, const char *name, bool state,
        lv_style_t *font_style, lv_style_t *bg_style) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_lamp_btn_t *btn = (lv_box_lamp_btn_t*) obj;

    btn->src_on_img = src_on_img;
    btn->src_off_img = src_off_img;

    lv_label_set_recolor(btn->name, true);
    lv_label_set_recolor(btn->state, true);
    lv_obj_update_layout(obj);
    lv_obj_set_size(btn->btn, lv_obj_get_width(obj), lv_obj_get_height(obj));
    lv_obj_add_style(btn->btn, bg_style, 0);
    lv_obj_add_style(btn->name, font_style, 0);
    lv_obj_add_style(btn->state, font_style, 0);

    if (state) {
        lv_img_set_src(btn->img, btn->src_on_img);
        lv_label_set_text_fmt(btn->state, "#5e5f64 %s #", "ON");
        lv_obj_add_state((lv_obj_t*) btn, LV_STATE_CHECKED);
    } else {
        lv_img_set_src(btn->img, btn->src_off_img);
        lv_label_set_text_fmt(btn->state, "#5e5f64 %s #", "OFF");
        lv_obj_clear_state((lv_obj_t*) btn, LV_STATE_CHECKED);
    }
    lv_label_set_text_fmt(btn->name, "#ffffff %s #", name);

    lv_obj_align(btn->img, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_align_to(btn->name, btn->img, LV_ALIGN_OUT_RIGHT_TOP, 20, -5);
    lv_obj_align_to(btn->state, btn->img, LV_ALIGN_OUT_RIGHT_BOTTOM, 20, 5);

    lv_obj_refresh_self_size(obj);

    lv_obj_invalidate(obj);
}

void lv_box_lamp_btn_set_state(lv_obj_t *obj, bool checked) {
    lv_box_lamp_btn_t *btn = (lv_box_lamp_btn_t*) obj;
    bool chk = lv_obj_get_state(obj) & LV_STATE_CHECKED;
    if (chk != checked) {
        if (checked) {
            lv_obj_add_state((lv_obj_t*) btn, LV_STATE_CHECKED);
            lv_img_set_src(btn->img, btn->src_on_img);
            lv_label_set_text_fmt(btn->state, "#5e5f64 %s #", "ON");
        } else {
            lv_obj_clear_state((lv_obj_t*) btn, LV_STATE_CHECKED);
            lv_img_set_src(btn->img, btn->src_off_img);
            lv_label_set_text_fmt(btn->state, "#5e5f64 %s #", "OFF");
        }
    }
}

/*=====================
 * Getter functions
 *====================*/

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_box_lamp_btn_constructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);LV_TRACE_OBJ_CREATE("begin");

    lv_box_lamp_btn_t *btn = (lv_box_lamp_btn_t*) obj;
    btn->btn = lv_btn_create(obj);
    btn->img = lv_img_create(obj);
    btn->name = lv_label_create(obj);
    btn->state = lv_label_create(obj);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn->btn, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_add_event_cb(btn->btn, lv_box_lamp_btn_event, LV_EVENT_CLICKED,
            NULL);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_box_lamp_btn_destructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);
    lv_box_lamp_btn_t *btn = (lv_box_lamp_btn_t*) obj;
}

static void lv_box_lamp_btn_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_box_lamp_btn_t *btn = (lv_box_lamp_btn_t*) lv_obj_get_parent(
            lv_event_get_target(e));

    if (code == LV_EVENT_CLICKED) {
        bool chk = lv_obj_get_state((lv_obj_t*) btn) & LV_STATE_CHECKED;
        if (chk) {
            lv_obj_clear_state((lv_obj_t*) btn, LV_STATE_CHECKED);
            lv_img_set_src(btn->img, btn->src_off_img);
            lv_label_set_text_fmt(btn->state, "#5e5f64 %s #", "OFF");
        } else {
            lv_obj_add_state((lv_obj_t*) btn, LV_STATE_CHECKED);
            lv_img_set_src(btn->img, btn->src_on_img);
            lv_label_set_text_fmt(btn->state, "#5e5f64 %s #", "ON");
        }
    }
}
