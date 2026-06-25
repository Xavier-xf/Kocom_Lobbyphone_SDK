/*****************************************************************************
 Copyright (C), 2015, AllwinnerTech. Co., Ltd.
 File name: widgets.cpp
 Author: yangy@allwinnertech.com
 Version: v1.0
 Date: 2015-11-24
 Description:
    widget will in actual set its visibility, background color and caption etc.
 History:
*****************************************************************************/

#define NDEBUG

#include "widgets/widgets.h"

#include "debug/app_log.h"

Widget::Widget(View* parent)
    : View(parent)
    , handle_(HWND_NULL)
    , option_style_(0)
{
}

Widget::~Widget()
{
    DestroyWidget();
}

/*****************************************************************************
 Function: Widget::GetCreateParams
 Description: call the child in actual
        @descendant
 Parameter:
    CommonCreateParams - fill it for creating widget
 Return: -
*****************************************************************************/
void Widget::GetCreateParams(CommonCreateParams& params)
{

}

void Widget::SetOptionStyle(const DWORD &style)
{
    option_style_ = style;
}

void Widget::GetOptionStyle(DWORD &style)
{
    style = option_style_;
}


void Widget::DestroyWidget()
{
    //::DestroyWindow(handle_);
    handle_ = HWND_INVALID;
}

/*****************************************************************************
 Function: Widget::HandleMessage
 Description: @called by Widget::WindowProc for further processing
    @descendant
    @override
 Parameter:
    #hwnd - the widget itself
 Return:
    the result of View::HandleMessage
*****************************************************************************/
int Widget::HandleMessage(HWND hwnd, int message, WPARAM wparam,
                                LPARAM lparam)
{
    switch (message)
    {
//      case MSG_CHAR:
//          return OnKeyPress((DWORD)wparam);
//      case MSG_KEYUP:
//          return OnKeyUp((DWORD)wparam);
//      case MSG_KEYDOWN:
//          return OnKeyDown((DWORD)wparam);
//      case MSG_SETFOCUS:
//          DoSetFocus(true);
//          if( OnEnter )
//              OnEnter(this);
//          return HELP_ME_OUT;
//      case MSG_KILLFOCUS:
//          if( hwnd == GetHandle() )
//          {
//              DoSetFocus(false);
//              if( OnLevel )
//                  OnLevel(this);
//          }
//          return HELP_ME_OUT;
     default:
         break;
    }

    return View::HandleMessage(hwnd, message, wparam, lparam);
}

/*****************************************************************************
 Function: Widget::WindowProc
 Description:
    Default process is essential for the creation of Widget.
    The function HandleMessage will be called if widget is existed.
    @attention: HandleMessage can be override
 Parameter:
    #hwnd - the widget itself
 Return:
    the result of HandleMessage
*****************************************************************************/

void Widget::WindowProc(lv_event_t *event)
{
    Widget *widget = (Widget *)lv_obj_get_user_data(lv_event_get_current_target(event));
    if (widget) {
        widget->HandleMessage((HWND)lv_event_get_current_target(event), lv_event_get_code(event), 0, 0);
    }
}
/*****************************************************************************
 Function: Widget::Refresh
 Description: refresh the widget completely
 Parameter: -
 Return: -
*****************************************************************************/
void Widget::Refresh()
{
    //::InvalidateRect(GetHandle(), 0, TRUE);
}

/*****************************************************************************
 Function: Widget::SetBackColor
 Description: set background color of the widget
    @override
 Parameter:
    #new_value - new background color, the format is 32bit ARGB
 Return: -
*****************************************************************************/
void Widget::SetBackColor(DWORD new_value)
{
    //SetWindowBkColor(GetHandle(), new_value);
	if ((GetHandle() != HWND_INVALID) && (GetHandle() != HWND_NULL)) {
            lv_obj_set_style_bg_color(GetHandle(), lv_color_hex(new_value), 0);
	    lv_obj_set_style_bg_opa(GetHandle(), new_value >> 24, 0);
	    printf("SetBackColor success %p,%x \n", GetHandle(), new_value);
	}
        back_color_ = new_value;
}
/*****************************************************************************
 Function: Widget::SetBackColor(HWND hWnd,DWORD new_value)
 Description: set background color of the widget
    @override
 Parameter:
    #new_value - new background color, the format is 32bit ARGB
 Return: -
*****************************************************************************/

void Widget::SetBackColor(lv_obj_t *hWnd,DWORD new_value)
{
    //SetWindowBkColor(hWnd, new_value);
    back_color_ = new_value;
    printf("back_color_ = %x", back_color_);
    if ((hWnd != HWND_INVALID) && (hWnd != HWND_NULL)) {
        lv_obj_set_style_bg_color(hWnd, lv_color_hex(new_value), 0);
        lv_obj_set_style_bg_opa(GetHandle(), new_value >> 24, 0);
    } 
}

/*****************************************************************************
 Function: Widget::SetVisible
 Description: set the visibility of the widget
    @override
 Parameter:
    #new_val - true:  show the widget
               false: hide the widget
 Return: -
*****************************************************************************/
void Widget::SetVisible(bool new_val)
{
    if (new_val) {
        if (!parent_ || parent_->GetVisible())
        Show();
    } else {
        Hide();
    }
}

/*****************************************************************************
 Function: Widget::Show
 Description:
    @override
 Parameter: -
 Return: -
*****************************************************************************/
void Widget::Show()
{
    //::ShowWindow(GetHandle(), SW_SHOWNORMAL);
    printf("Show:%p\n", GetHandle());
    if ((GetHandle() != HWND_INVALID) && (GetHandle() != HWND_NULL)) {
        lv_obj_clear_flag(GetHandle(), LV_OBJ_FLAG_HIDDEN);
        //lv_obj_move_foreground(GetHandle());
    }
            View::Show();
}

/*****************************************************************************
 Function: Widget::Hide
 Description:
    @override
 Parameter: -
 Return: -
*****************************************************************************/
void Widget::Hide()
{
    //::ShowWindow(GetHandle(), SW_HIDE);
    if ((GetHandle() != HWND_INVALID) && (GetHandle() != HWND_NULL)) {
        lv_obj_add_flag(GetHandle(), LV_OBJ_FLAG_HIDDEN);
    }
    View::Hide();
}

/*****************************************************************************
 Function: Widget::SetCaption
 Description: replacing the caption will refresh the widget entirely
 Parameter:
    #new_caption - the text must be ecoded as UTF8
 Return:
*****************************************************************************/
void Widget::SetCaption(const char* new_caption)
{
    if ((GetHandle() != HWND_INVALID) && (GetHandle() != HWND_NULL)) {
        lv_obj_t *text = lv_label_create(GetHandle());
        lv_obj_set_align(text, LV_ALIGN_CENTER);
	lv_label_set_text(text, new_caption);
    }

    //::SetWindowText(GetHandle(), new_caption);
}

void Widget::GetCaption(char *pString, int pStringLen)
{
    //::GetWindowText(GetHandle(),pString,pStringLen);
}

/*****************************************************************************
 Function: Widget::CreateWidget
 Description: According to the obtained parameters to create a new widget
 Parameter: -
 Return: -
*****************************************************************************/
void Widget::CreateWidget()
{

    lv_obj_t *parent_handle;
    CommonCreateParams params;
    WNDCLASS wc;
    
    memset((void*)(&params), 0, sizeof(params));

    GetCreateParams(params);
    printf("create widget: %s\n", params.alias);
 
    if (parent_) {
	parent_handle = parent_->GetHandle();
	printf("parent exist\n");

    } else {
	printf("parent desktop\n");
	parent_handle = HWND_INVALID;

    }

    handle_  = HWND_NULL;
    if (!strcmp(params.alias, "ProgressBar")) {
        handle_ = lv_obj_create(parent_handle);
	printf("handle_ = %p\n", handle_);
	lv_obj_clear_flag(handle_, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_clear_flag(handle_, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_t *slider_handle_ = lv_slider_create(handle_);
	lv_obj_set_style_pad_top(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_bottom(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_left(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_right(handle_, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(handle_, 0, 0);
	lv_obj_set_style_outline_width(handle_, 0, 0);
	lv_obj_set_style_outline_pad(handle_, 0, 0);
	lv_obj_set_style_shadow_width(handle_, 0, 0);
	lv_obj_add_flag(handle_, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_set_flex_flow(handle_, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(handle_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);



	//lv_obj_set_height(slider_handle_, lv_pct(50));
	//lv_obj_set_align(slider_handle_, LV_ALIGN_LEFT_MID);
	lv_obj_set_style_bg_opa(slider_handle_, 255, LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider_handle_, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider_handle_, lv_color_hex(0xFF00CDD1), LV_PART_INDICATOR);
	lv_obj_set_style_bg_opa(slider_handle_, 0, LV_PART_KNOB);
	lv_obj_add_flag(slider_handle_, LV_OBJ_FLAG_EVENT_BUBBLE);
	lv_obj_set_style_border_width(slider_handle_, 0, 0);
	lv_obj_set_style_outline_width(slider_handle_, 0, 0);
	lv_obj_set_style_outline_pad(slider_handle_, 0, 0);
	lv_obj_set_style_shadow_width(slider_handle_, 0, 0);

	lv_obj_add_event_cb(handle_, WindowProc, LV_EVENT_VALUE_CHANGED, NULL);
	lv_obj_add_event_cb(handle_, WindowProc, LV_EVENT_INSERT, NULL);
	lv_obj_add_event_cb(handle_, WindowProc, LV_EVENT_READY, NULL);
	lv_obj_t *text = lv_label_create(handle_);
	lv_label_set_text(text, "");
	lv_obj_set_style_text_color(text, lv_color_white(), 0);
	//lv_obj_set_align(text, LV_ALIGN_RIGHT_MID);
	lv_obj_t *text_total = lv_label_create(handle_);
	lv_label_set_text(text_total, "00:00");
	lv_obj_set_style_text_color(text_total, lv_color_white(), 0);
	printf("slider_handle_ = %p\n", slider_handle_);
    }
    lv_obj_set_user_data(handle_, this);
    printf("this1 = %p\n", this);
    /*
    if (::GetWindowClassInfo(&wc) == false)
    {
        //::RegisterWindowClass(&wc);
    }*/

    /*handle_ = CreateWindowEx(
    params.class_name,
    "",
    WS_CHILD | params.style,
    WS_EX_NONE | params.exstyle,
    params.id,
    params.x,
    params.y,
    params.w,
    params.h,
    parent_handle, (DWORD)this);
    SetBackColor(0xFF000000);*/
    //::GetWindowRect( handle_, &bound_rect_);
}

/*****************************************************************************
 Function: Widget::GetHandle
 Description: get widget's handler
    @override
 Parameter: -
 Return: the handler of the widget
*****************************************************************************/
lv_obj_t *Widget::GetHandle()
{

    if (handle_ == HWND_NULL) {
	CreateWidget();
    }
    return  handle_;
}

CustomWidget::CustomWidget(View *parent)
    : Widget(parent)
{

}

CustomWidget::~CustomWidget()
{

}


/*****************************************************************************
 Function: CustomWidget::HandleMessage
 Description: process the message if be necessary
    @override
 Parameter:
    #hwnd - widget's handler
 Return:
    the result of Widget::HandleMessage
*****************************************************************************/
int CustomWidget::HandleMessage(HWND hwnd, int message, WPARAM wparam,
                                    LPARAM lparam)
{
    HDC hdc;
    RECT rc;
    switch (message)
    {
//      case MSG_PAINT:
//          hdc = ::BeginPaint( m_hHandle );
//          ::GetClientRect( m_hHandle, &rc );
//          Paint( hdc, &rc );
//          ::EndPaint( m_hHandle, hdc );
//          return DO_IT_MYSELF;
        default:
            return Widget::HandleMessage(hwnd, message, wparam, lparam );
    }
}


