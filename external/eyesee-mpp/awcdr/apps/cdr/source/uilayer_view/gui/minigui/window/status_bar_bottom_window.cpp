/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file status_bar_window.cpp
 * @brief 状态栏窗口
 * @author id:690
 * @version v0.3
 * @date 2017-01-17
 */
//#define NDEBUG

#include "window/preview_window.h"
#include "window/status_bar_bottom_window.h"
#include "debug/app_log.h"
#include "widgets/text_view.h"
#include "resource/resource_manager.h"
#include "widgets/view_container.h"
#include "window/window_manager.h"
#include "common/message.h"
#include "common/posix_timer.h"
#include "common/setting_menu_id.h"
#include "application.h"
#include "bll_presenter/device_setting.h"
#include "device_model/menu_config_lua.h"
#include "bll_presenter/audioCtrl.h"

#include <sstream>

using namespace std;
using namespace EyeseeLinux;

IMPLEMENT_DYNCRT_CLASS(StatusBarBottomWindow)
/*****************************************************************************
Function: ContainerWidget::HandleMessage
Description: process the messages and notify the children
@override
Parameter:
Return:
 *****************************************************************************/
int StatusBarBottomWindow::HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam)
{
    //db_error("[debug_jaosn]:StatusBarBottomWindow %d",message);
    switch ( message ) {
        case MSG_PAINT:
           //db_warn("habo---> statusbarbottom window  MSG_PAINT !!!");
            return HELP_ME_OUT;
        default:
            return ContainerWidget::HandleMessage( hwnd, message, wparam, lparam );
    }
}

StatusBarBottomWindow::StatusBarBottomWindow(IComponent *parent)
: SystemWindow(parent)
{
    wname = "StatusBarBottomWindow";
    Load();
    SetBackColor(0x66000000);

    record_button_view_ = reinterpret_cast<GraphicView *>(GetControl("button1_icon"));
    GraphicView::LoadImage(record_button_view_, "status_bar_bottom_rec");
    photo_button_view_ = reinterpret_cast<GraphicView *>(GetControl("button2_icon"));
    GraphicView::LoadImage(photo_button_view_, "status_bar_bottom_photo");
    lock_button_view_ = reinterpret_cast<GraphicView *>(GetControl("button3_icon"));
    GraphicView::LoadImage(lock_button_view_, "status_bar_bottom_lock");
    playback_button_view_ = reinterpret_cast<GraphicView *>(GetControl("button4_icon"));
    GraphicView::LoadImage(playback_button_view_, "status_bar_bottom_playback");
    setting_button_view_ = reinterpret_cast<GraphicView *>(GetControl("button5_icon"));
    GraphicView::LoadImage(setting_button_view_, "status_bar_bottom_setting");
    adas_button_view_ = reinterpret_cast<GraphicView *>(GetControl("button6_icon"));
    GraphicView::LoadImage(adas_button_view_, "status_bar_bottom_adas");
    record_audio_button_view_ = reinterpret_cast<GraphicView *>(GetControl("button7_icon"));
    GraphicView::LoadImage(record_audio_button_view_, "status_bar_bottom_mic");

    PreviewWindownButtonStatus(true);
    InitPreviewButtonPos();
}

StatusBarBottomWindow::~StatusBarBottomWindow()
{
    db_msg("destruct");
    if(record_button_view_)
       GraphicView::UnloadImage(record_button_view_);
    if(photo_button_view_)
       GraphicView::UnloadImage(photo_button_view_);
    if(lock_button_view_)
       GraphicView::UnloadImage(lock_button_view_);
    if(playback_button_view_)
       GraphicView::UnloadImage(playback_button_view_);
    if(setting_button_view_)
       GraphicView::UnloadImage(setting_button_view_);
    if(adas_button_view_)
       GraphicView::UnloadImage(adas_button_view_);
    if(record_audio_button_view_)
       GraphicView::UnloadImage(record_audio_button_view_);
}

void StatusBarBottomWindow::InitPreviewButtonPos()
{
    RECT icon_rect;
    char icon_name[128]={0};
    for(int i = 0; i< PrviewButtonPosLen ; i++)
    {
        snprintf(icon_name,sizeof(icon_name)-1,"button%d_icon",i+1);
        GetControl(icon_name)->GetRect(&icon_rect);
        PreviewButtonPos[i].x1 = icon_rect.left;
        PreviewButtonPos[i].y1 = icon_rect.top;
        PreviewButtonPos[i].x2 = icon_rect.right;
        PreviewButtonPos[i].y2 = icon_rect.bottom;
    }
}

void StatusBarBottomWindow::GetPreviewButtonPos(struct buttonPos *bps,int len)
{
    for(int i = 0; i< len ; i++)
    {
        bps[i].x1 = PreviewButtonPos[i].x1;
        bps[i].y1 = PreviewButtonPos[i].y1;
        bps[i].x2 = PreviewButtonPos[i].x2;
        bps[i].y2 = PreviewButtonPos[i].y2;
    }
}

void StatusBarBottomWindow::DoHide()
{
    WindowManager *wm = WindowManager::GetInstance();
    Window *cur_win = wm->GetWindow(wm->GetCurrentWinID());
    //::SetActiveWindow(cur_win->GetHandle());
    //::EnableWindow(cur_win->GetHandle(), true);
    printf("DoHide\n");
    Widget::Hide();
}

void StatusBarBottomWindow::PreviewWindownButtonStatus(bool on_off)
{
    if(on_off)
    {
        record_button_view_->Show();
        photo_button_view_->Show();
        lock_button_view_->Show();
        playback_button_view_->Show();
        setting_button_view_->Show();
        adas_button_view_->Show();
        record_audio_button_view_->Show();
    }
    else
    {
        record_button_view_->Hide();
        photo_button_view_->Hide();
        lock_button_view_->Hide();
        playback_button_view_->Hide();
        setting_button_view_->Hide();
        adas_button_view_->Hide();
        record_audio_button_view_->Hide();
    }
}

void StatusBarBottomWindow::GetCreateParams(CommonCreateParams& params)
{
    params.style = WS_NONE;
    params.exstyle = WS_EX_NONE | WS_EX_TOPMOST;
    // params.exstyle = WS_EX_NONE ;
    params.class_name = " ";
    params.alias      = GetClassName();
}

string StatusBarBottomWindow::GetResourceName()
{
    return string(GetClassName());
}

void StatusBarBottomWindow::PreInitCtrl(View *ctrl, string &ctrl_name)
{

    ctrl->SetCtrlTransparentStyle(true);
}

void StatusBarBottomWindow::Update(MSG_TYPE msg,int p_CamID, int p_recordId)
{
    string zoom_str;
    db_msg("handle msg:%d", msg);
    switch ((int)msg) {
        case MSG_CHANG_STATU_PREVIEW:
           PreviewWindownButtonStatus(true);
            break;
        case MSG_CHANG_STATU_PLAYBACK:
            PreviewWindownButtonStatus(false);
            break;
//        case MSG_PREVIW_TO_SETTING_CHANGE_STATUS_BAR_BOTTOM:
//            break;
        case MSG_PLAYBACK_TO_PREIVEW_CHANG_STATUS_BAR_BOTTOM:
            PreviewWindownButtonStatus(true);
            break;
        default:
            break;
    }
}



