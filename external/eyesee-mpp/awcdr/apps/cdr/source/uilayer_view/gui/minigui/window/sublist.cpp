/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    sublist.cpp
 * @brief   子列表（三级列表）
 * @author  sh
 * @version v1.0
 * @date    2018-01-02
 */
#include "sublist.h"
#include "debug/app_log.h"
#include "widgets/text_view.h"
#include "resource/resource_manager.h"
#include "widgets/view_container.h"
#include "window/window_manager.h"
#include "common/message.h"
#include "common/posix_timer.h"
#include "application.h"
#include "widgets/list_view.h"
//#include "bll_presenter/audioCtrl.h"
//#include "bll_presenter/screensaver.h"
#include "window/window.h"
#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "Sublist"

using namespace std;
using namespace EyeseeLinux;

IMPLEMENT_DYNCRT_CLASS(Sublist)

void Sublist::keyProc(int keyCode, int isLongPress)
{
#ifdef KEY_INTERACTION
	 switch(keyCode){
        case SDV_KEY_LEFT:
        case SDV_KEY_MENU:
        case SDV_KEY_OK:
        case SDV_KEY_RIGHT:
            Window *form = WindowManager::GetInstance()->GetWindow(WINDOWID_SETTING_NEW);
            form->keyProc(keyCode,0);
        break;
    }
#endif
}


int Sublist::HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam)
{
    switch ( message ) {
        case LV_EVENT_CLICKED:
            {
                //int id = LOSWORD(wparam);
                //nt code = HISWORD(wparam);
                //if (code == LVN_CLICKED) {
                    if (OnListItemClick)
                        OnListItemClick(this);
                //}
            }
            break;
        default:
            break;
    }

    return ContainerWidget::HandleMessage( hwnd, message, wparam, lparam );
}

Sublist::Sublist(IComponent *parent)
    : SystemWindow(parent)
    ,m_hide(true)
{
    wname = "Sublist";
    Load();
    SetBackColor(0xFF000000);
}

Sublist::~Sublist()
{

}

void Sublist::GetCreateParams(CommonCreateParams& params)
{
    params.style = WS_NONE;
    params.exstyle = WS_EX_NONE|WS_EX_TOPMOST;
    params.class_name = " ";
    params.alias      = GetClassName();
}

string Sublist::GetResourceName()
{
    return string(GetClassName());
}

void Sublist::PreInitCtrl(View *ctrl, std::string &ctrl_name)
{

}


void Sublist::DoShow()
{
    Window::DoShow();
    // ::EnableWindow(parent_->GetHandle(), false);
    /*if ((parent_->GetHandle() != HWND_INVALID) && (parent_->GetHandle() != HWND_NULL)) {
        lv_obj_clear_flag(parent_->GetHandle(), LV_OBJ_FLAG_HIDDEN);
    }*/
    m_hide = false;
}

void Sublist::DoHide()
{
    SetVisible(false);
    //printf("Sublist DoHide\n");
    /*if ((parent_->GetHandle() != HWND_INVALID) && (parent_->GetHandle() != HWND_NULL)) {
        lv_obj_add_flag(parent_->GetHandle(), LV_OBJ_FLAG_HIDDEN);
    }*/
    // ::EnableWindow(parent_->GetHandle(), true);
    //if (parent_->GetVisible()) {
        //::SetActiveWindow(parent_->GetHandle());
    m_hide = true;
   // }
}
