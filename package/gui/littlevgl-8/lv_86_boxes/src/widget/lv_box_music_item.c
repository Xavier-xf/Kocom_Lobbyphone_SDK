/*
 * lv_box_music_item.c
 *
 *  Created on: 2022Äê10ÔÂ10ÈÕ
 *      Author: anruliu
 */

#include "lv_box_music_item.h"
#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_box_music_item_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_box_music_item_constructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj);
static void lv_box_music_item_destructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj);
static void lv_box_music_item_anim_timer(lv_timer_t *timer);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_box_music_item_class = {
    .constructor_cb = lv_box_music_item_constructor,
    .destructor_cb = lv_box_music_item_destructor,
    .width_def = LV_SIZE_CONTENT,
    .height_def = LV_SIZE_CONTENT,
    .instance_size = sizeof(lv_box_music_item_t),
    .base_class = &lv_obj_class
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t* lv_box_music_item_create(lv_obj_t *parent) {
    LV_LOG_INFO("begin");
    lv_obj_t *obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

/*=====================
 * Setter functions
 *====================*/

void lv_box_music_item_set_src(lv_obj_t *obj, lv_style_t *name_font_style,
        lv_style_t *bg_style, char **anim_img, const char *name) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_music_item_t *item = (lv_box_music_item_t*) obj;

    item->anim_img = anim_img;

    lv_obj_update_layout(obj);
    lv_obj_set_size(item->btn, lv_obj_get_width(obj), lv_obj_get_height(obj));

    lv_obj_add_style(item->btn, bg_style, 0);
    lv_obj_add_style(item->name, name_font_style, 0);

    lv_label_set_long_mode(item->name, 2);
    lv_obj_set_width(item->name, lv_obj_get_width(obj) - 100);
    lv_label_set_text_fmt(item->name, "%s", name);
    lv_obj_set_style_text_color(item->name, lv_color_white(), 0);

    lv_obj_align(item->right_img, LV_ALIGN_RIGHT_MID, -30, 0);
    lv_obj_align(item->name, LV_ALIGN_LEFT_MID, 15, 0);

    lv_obj_add_flag(item->right_img, LV_OBJ_FLAG_HIDDEN);

    lv_obj_refresh_self_size(obj);

    lv_obj_invalidate(obj);
}

void lv_box_music_item_set_play(lv_obj_t *obj, bool mode) {
    LV_ASSERT_OBJ(obj, MY_CLASS);

    lv_box_music_item_t *item = (lv_box_music_item_t*) obj;

    if (item->mode == mode)
        return;

    item->mode = mode;

    if (mode) {
        if (!item->timer)
            item->timer = lv_timer_create(lv_box_music_item_anim_timer, 100,
                    item);
        lv_obj_clear_flag(item->right_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(item->name, lv_color_make(0xff, 0x8a, 0x00),
                0);
    } else {
        if (item->timer) {
            lv_timer_del(item->timer);
            item->timer = NULL;
        }
        lv_obj_add_flag(item->right_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(item->name, lv_color_white(), 0);
    }
}

/*=====================
 * Getter functions
 *====================*/

bool lv_box_music_item_get_play(lv_obj_t *obj) {
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_box_music_item_t *item = (lv_box_music_item_t*) obj;
    return item->mode;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_box_music_item_constructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);LV_TRACE_OBJ_CREATE("begin");

    lv_box_music_item_t *item = (lv_box_music_item_t*) obj;

    item->btn = lv_btn_create(obj);
    item->name = lv_label_create(obj);
    item->right_img = lv_img_create(obj);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(item->btn, LV_OBJ_FLAG_EVENT_BUBBLE);

    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_box_music_item_destructor(const lv_obj_class_t *class_p,
        lv_obj_t *obj) {
    LV_UNUSED(class_p);
    lv_box_music_item_t *item = (lv_box_music_item_t*) obj;

    if (item->timer) {
        lv_timer_del(item->timer);
        item->timer = NULL;
    }
}

static void lv_box_music_item_anim_timer(lv_timer_t *timer) {
    static int index;
    lv_box_music_item_t *item = (lv_box_music_item_t*) timer->user_data;

    index = index >= 3 ? 0 : ++index;
    if (item->anim_img[index])
        lv_img_set_src(item->right_img, item->anim_img[index]);
}
