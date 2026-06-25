/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    status_bar.cpp
 * @brief   状态栏窗口presenter
 * @author  id:826
 * @version v0.3
 * @date    2016-12-26
 */

#include "status_bar.h"

#include "common/app_log.h"
#include "device_model/storage_manager.h"
#include "device_model/system/event_manager.h"
#include "device_model/system/net/net_manager.h"
#include "uilayer_view/gui/minigui/window/window_manager.h"
#include "uilayer_view/gui/minigui/window/status_bar_window.h"

#undef LOG_TAG
#define LOG_TAG "StatusBar"

using namespace EyeseeLinux;
using namespace std;

StatusBarPresenter::StatusBarPresenter()
    : win_mg_(WindowManager::GetInstance())
    , sb_win_(NULL)
{
    em_ = EventManager::GetInstance();
    StorageManager::GetInstance()->Attach(this);
}

StatusBarPresenter::~StatusBarPresenter()
{
    StorageManager::GetInstance()->Detach(this);
}

int StatusBarPresenter::DeviceModelInit()
{
    NetManager::GetInstance()->Attach(this);
    return 0;
}

int StatusBarPresenter::DeviceModelDeInit()
{
    NetManager::GetInstance()->Detach(this);
    return 0;
}

void StatusBarPresenter::OnWindowLoaded()
{
    sb_win_ = reinterpret_cast<StatusBarWindow *>(win_mg_->GetWindow(WINDOWID_STATUSBAR));

    this->DeviceModelInit();

    // init sd status
    int sd_status;
    em_->CheckEvent(EventManager::EVENT_SD, sd_status);

    if (sd_status > 0)
        sb_win_->Update(MSG_STORAGE_MOUNTED);
    else
        sb_win_->Update(MSG_STORAGE_UMOUNT);

    // init bt status
    int bt_status;
    em_->CheckEvent(EventManager::EVENT_BT, bt_status);

    if (bt_status > 0)
        sb_win_->Update(MSG_BLUETOOTH_ENABLE);
    else
        sb_win_->Update(MSG_BLUETOOTH_DISABLE);

    // init usb disk status
    int usb_status;
    em_->CheckEvent(EventManager::EVENT_USB, usb_status);

    if (usb_status > 0)
        sb_win_->Update(MSG_USB_PLUG_IN);
    else
        sb_win_->Update(MSG_USB_PLUG_OUT);

    // init hdmi status
    /*
    int hdmi_status;
    em_->CheckEvent(EventManager::EVENT_HDMI, hdmi_status);

    if (hdmi_status > 0)
        sb_win_->Update(MSG_HDMI_PLUGIN);
    else
        sb_win_->Update(MSG_HDMI_PLUGOUT);

    // init tvout status
    int tvout_status;
    em_->CheckEvent(EventManager::EVENT_TVOUT, tvout_status);

    if (tvout_status > 0)
        sb_win_->Update(MSG_TVOUT_PLUG_IN);
    else
        sb_win_->Update(MSG_TVOUT_PLUG_OUT);
*/
    // init wifi status
    int wifi_status;
    em_->CheckEvent(EventManager::EVENT_WIFI, wifi_status);

    if (wifi_status > 0)
        sb_win_->Update(MSG_WIFI_ENABLED);
    else
        sb_win_->Update(MSG_WIFI_DISABLED);

    // init ethernet status
    // int eth_status;
    // em_->CheckEvent(EventManager::EVENT_ETH, eth_status);

    // if (eth_status > 0)
        // sb_win_->Update(MSG_ETH_CONNECT_LAN);
    // else
        // sb_win_->Update(MSG_ETH_DISCONNECT);

    lock_guard<mutex> lock(msg_mutex_);
    ignore_msg_ = false;
}

void StatusBarPresenter::OnWindowDetached()
{
    lock_guard<mutex> lock(msg_mutex_);
    ignore_msg_ = true;
    this->DeviceModelDeInit();
}

int StatusBarPresenter::HandleGUIMessage(int msg, int val, int id)
{
    return 0;
}

void StatusBarPresenter::BindGUIWindow(::Window *win)
{
    this->Attach(win);
}

void StatusBarPresenter::Update(MSG_TYPE msg, int p_CamID, int p_recordId)
{
    if (msg_mutex_.try_lock() == false) {
        db_warn("maybe presenter is detaching, ignore this msg");
        return;
    }
    if (ignore_msg_) {
        db_warn("presenter has been detached, do not response msg");
        msg_mutex_.unlock();
        return;
    }
    msg_mutex_.unlock();

    //db_msg("msg: %d", msg);
    switch(msg)
    {
        case MSG_STORAGE_MOUNTED:
        {
            sb_win_ = reinterpret_cast<StatusBarWindow *>(win_mg_->GetWindow(WINDOWID_STATUSBAR));
            sb_win_->Update(MSG_STORAGE_MOUNTED);
        }
        break;
    }
    this->Notify(msg);
}

