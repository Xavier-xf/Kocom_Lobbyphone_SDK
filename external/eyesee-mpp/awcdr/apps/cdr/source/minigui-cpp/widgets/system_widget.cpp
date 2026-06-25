/*****************************************************************************
 Copyright (C), 2015, AllwinnerTech. Co., Ltd.
 File name: system_widget.cpp
 Author: yangy@allwinnertech.com
 Version: v1.0
 Date: 2015-11-24
 Description:
    System Widget
 History:
*****************************************************************************/
#define NDEBUG

#include "type/types.h"
#include "widgets/system_widget.h"
#include "debug/app_log.h"

#undef LOG_TAG
#define LOG_TAG "SystemWidget"

using namespace std;

/* A global static KeyMap variable.
 * Through the handler to find out class pointer, so as to call the function
 * HandleMessage.
 * @where: SystemWidget::WindowProc
 */
KeyMap SystemWidget::g_controls_maps_;

SystemWidget::SystemWidget(View *parent)
    : Widget(parent)
{

}

SystemWidget::~SystemWidget()
{
    DestroyWidget();
}

/*****************************************************************************
 Function: SystemWidget::WindowProc
 Description: The old process function of the system control is substituted
    by this WindowProc. To begin with, HandleMessage is called, and later, when
    the value from that equals to HELP_ME_OUT, old window process function will
    be called.
    @attention: there is no HandleMessage here.
    @descendant
 Parameter:
    #hwnd - system widget's handler
 Return:
*****************************************************************************/
/*long int SystemWidget::WindowProc(HWND hwnd, unsigned int message, WPARAM wparam,
                            LPARAM lparam)
{
    int ret = 0;

    View *result = KeyMapSearch(g_controls_maps_, hwnd);

    if (result != NULL) {
        SystemWidget* ctrl = reinterpret_cast<SystemWidget*>(result) ;
        if (ctrl) {
            ret = ctrl->HandleMessage(hwnd, message, wparam, lparam);
            if (ret == HELP_ME_OUT) {
                return ctrl->old_window_proc_(hwnd, message, wparam, lparam);
            }
        }
    }
    //return DefaultControlProc(hwnd, message, wparam, lparam);
}*/

void SystemWidget::WindowProc(lv_event_t *event)
{
    printf("widget123: %p\n", lv_event_get_target(event));
    int ret = 0;
    lv_obj_t *hwnd = lv_event_get_target(event);

    View *result = KeyMapSearch(g_controls_maps_, hwnd);

    if (result != NULL) {
        SystemWidget* ctrl = reinterpret_cast<SystemWidget*>(result) ;
        if (ctrl) {
            ret = ctrl->HandleMessage(hwnd, lv_event_get_code(event), 0, 0);
            if ((ret == HELP_ME_OUT) && ctrl->old_window_proc_) {
                ctrl->old_window_proc_(hwnd, lv_event_get_code(event), 0, 0);
            }
        }
    }

    //return DefaultControlProc(hwnd, message, wparam, lparam);
}

void SystemWidget::DestroyWidget()
{
    g_controls_maps_.clear();
    //::DestroyWindow(handle_);
}

/*****************************************************************************
 Function: SystemWidget::CreateWidget
 Description: create the system widget and replace its window process function
 Parameter: -
 Return: -
*****************************************************************************/
void SystemWidget::CreateWidget()
{
    CommonCreateParams params;
    memset( (void*)(&params), 0, sizeof(params));
    GetCreateParams(params);
    lv_obj_t *parent_handle = parent_->GetHandle();
    printf("-------------CreateWidget %s--%p----\n", params.alias, parent_handle);
    if((parent_handle == HWND_INVALID) || (parent_handle == HWND_NULL)) {
	handle_ = HWND_INVALID;
    } else if (!strcmp(params.alias, "GraphicView")) {
       	handle_ = lv_obj_create(parent_handle);
	lv_obj_t *img_handle_ = lv_img_create(handle_);
	lv_obj_set_align(img_handle_, LV_ALIGN_CENTER);
        printf("-------------CreateWidget %p----\n", handle_);
	lv_obj_add_flag(handle_, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(handle_, LV_OBJ_FLAG_EVENT_BUBBLE);
	lv_obj_set_style_pad_top(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_bottom(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_left(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_right(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(handle_, 0, LV_PART_MAIN);
        lv_obj_clear_flag(handle_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_border_width(handle_, 0, 0);
   	lv_obj_set_style_outline_width(handle_, 0, 0);
	lv_obj_set_style_outline_pad(handle_, 0, 0);
	lv_obj_set_style_shadow_width(handle_, 0, 0);
        //lv_obj_set_style_radius(handle_, 0, 0);
        lv_obj_set_style_bg_color(handle_, lv_color_white(), 0);
	//lv_img_set_zoom(handle_, 256);
	lv_img_set_size_mode(parent_handle, LV_IMG_SIZE_MODE_REAL);
	//lv_label_set_text(handle_, "");
    } else if (!strcmp(params.alias, "TextView")) {
       	handle_ = lv_label_create(parent_handle);
	printf("-------------CreateWidget %p----\n", handle_);
	//lv_obj_set_style_text_align(parent_handle, LV_TEXT_ALIGN_CENTER, 0);
	lv_obj_set_style_text_color(parent_handle, lv_color_white(), 0);
	//lv_label_set_long_mode(handle_, LV_LABEL_LONG_SCROLL_CIRCULAR);
	lv_obj_set_style_pad_top(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_bottom(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_left(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_right(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(handle_, 0, 0);
	lv_label_set_text(handle_, "");
    } else if (!strcmp(params.alias, "ListView")) {
       	handle_ = lv_obj_create(parent_handle);
	lv_obj_set_style_pad_top(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_bottom(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_left(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_right(handle_, 0, LV_PART_MAIN);
        lv_obj_clear_flag(handle_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_top(handle_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(handle_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(handle_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_right(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(handle_, 0, 0);
	lv_obj_set_style_outline_width(handle_, 0, 0);
	lv_obj_set_style_outline_pad(handle_, 0, 0);
	lv_obj_set_style_shadow_width(handle_, 0, 0);
	lv_obj_add_flag(handle_, LV_OBJ_FLAG_EVENT_BUBBLE);
	lv_obj_add_flag(handle_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_clear_flag(handle_, LV_OBJ_FLAG_SCROLL_ELASTIC);
	lv_obj_set_user_data(handle_, (void * )0);
	printf("-------------CreateWidget %p----\n", handle_);
       //lv_label_set_text(handle_, "");
    } else if ((!strcmp(params.alias, "ButtonOK")) || (!strcmp(params.alias, "ButtonCancel"))) {
       	handle_ = lv_btn_create(parent_handle);
	printf("params.id=%d\n", params.id);
	lv_obj_set_user_data(handle_, (void *)params.id);
	printf("params.id=%d %p\n", lv_obj_get_user_data(handle_), handle_);
	lv_obj_set_style_pad_top(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_bottom(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_left(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_right(handle_, 0, LV_PART_MAIN);
        lv_obj_clear_flag(handle_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_top(handle_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(handle_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(handle_, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_right(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(handle_, 1, 0);
	lv_obj_set_style_outline_width(handle_, 0, 0);
	lv_obj_set_style_outline_pad(handle_, 0, 0);
	lv_obj_set_style_shadow_width(handle_, 0, 0);
	lv_obj_set_style_radius(handle_, 0, 0);
	lv_obj_add_flag(handle_, LV_OBJ_FLAG_EVENT_BUBBLE);
	lv_obj_add_flag(handle_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_clear_flag(handle_, LV_OBJ_FLAG_SCROLL_ELASTIC);
	printf("-------------CreateWidget %p----\n", handle_);
       //lv_label_set_text(handle_, "");
    }


    /*handle_ = ::CreateWindowEx(params.class_name,
        " ",
        WS_CHILD | params.style,
        WS_EX_NONE | params.exstyle,
        params.id,
        params.x, params.y, params.w, params.h,
        parent_handle,
        0);*/
    old_window_proc_= NULL;
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID))
        lv_obj_add_event_cb(handle_, WindowProc, LV_EVENT_CLICKED, NULL);
    g_controls_maps_.insert(make_pair(handle_, this));
    //::GetWindowRect( handle_, &bound_rect_ );
}
