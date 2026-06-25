/*
 * lv_box_alarm_item.c
 *
 *  Created on: 2022Äê9ÔÂ14ÈÕ
 *      Author: anruliu
 */

#include "lv_box_alarm_item.h"
#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_box_alarm_item_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_box_alarm_item_constructor(const lv_obj_class_t *class_p, lv_obj_t *obj);
static void lv_box_alarm_item_destructor(const lv_obj_class_t *class_p, lv_obj_t *obj);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_box_alarm_item_class = {
    .constructor_cb = lv_box_alarm_item_constructor,
    .destructor_cb = lv_box_alarm_item_destructor,
    .width_def = LV_SIZE_CONTENT,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(lv_box_alarm_item_t),
    .base_class = &lv_obj_class
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t* lv_box_alarm_item_create(lv_obj_t *parent) {
    LV_LOG_INFO("begin");
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/*=====================
 * Setter functions
 *====================*/

void lv_box_alarm_item_set_src(lv_obj_t *obj, lv_style_t *time_font_style,
        lv_style_t *des_font_style, lv_style_t *loop_font_style,
        lv_style_t *bg_style, bool state) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_alarm_item_t *item = (lv_box_alarm_item_t*) obj;

    lv_label_set_recolor(item->tiem, true);
    lv_label_set_recolor(item->des, true);
    lv_label_set_recolor(item->loop, true);
    lv_obj_update_layout(obj);
    lv_obj_set_size(item->btn, lv_obj_get_width(obj), lv_obj_get_height(obj));

    if (state)
        lv_obj_add_state(item->sw, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(item->sw, lv_color_hex(0xffa523),
            LV_PART_INDICATOR | LV_STATE_CHECKED);

    lv_obj_add_style(item->btn, bg_style, 0);
    lv_obj_add_style(item->tiem, time_font_style, 0);
    lv_obj_add_style(item->des, des_font_style, 0);
    lv_obj_add_style(item->loop, loop_font_style, 0);

    lv_label_set_text_fmt(item->tiem, "#ffffff %s #", "07:00");
    lv_label_set_text_fmt(item->des, "#ffffff %s #", "Get up");
    lv_label_set_text_fmt(item->loop, "#ffffff %s #", "Working day");

    lv_obj_set_pos(item->tiem, 15, 0);

    lv_obj_align(item->sw, LV_ALIGN_RIGHT_MID, -15, 0);
    lv_obj_align_to(item->des, item->tiem, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_align_to(item->loop, item->tiem, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    lv_obj_refresh_self_size(obj);

    lv_obj_invalidate(obj);
}

/*=====================
 * Getter functions
 *====================*/

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_box_alarm_item_constructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);LV_TRACE_OBJ_CREATE("begin");

    lv_box_alarm_item_t *item = (lv_box_alarm_item_t*) obj;

    item->btn = lv_btn_create(obj);
    item->tiem = lv_label_create(obj);
    item->des = lv_label_create(obj);
    item->loop = lv_label_create(obj);
    item->sw = lv_switch_create(obj);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item->btn, LV_OBJ_FLAG_EVENT_BUBBLE);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_box_alarm_item_destructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);
    lv_box_alarm_item_t *item = (lv_box_alarm_item_t*) obj;
}
