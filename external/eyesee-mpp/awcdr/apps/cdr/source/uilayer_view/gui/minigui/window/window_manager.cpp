/*****************************************************************************
 Copyright (C), 2015, AllwinnerTech. Co., Ltd.
 File name: window_manager.cpp
 Author: yangy@allwinnertech.com
 Version: v1.0
 Date: 2015-11-24
 Description:

 History:
*****************************************************************************/

#include "window/window_manager.h"
#include "data/gui.h"
#include "resource/resource_manager.h"
#include "debug/app_log.h"
#include "debug/message_string.h"
#include "application.h"
#include "runtime/runtime.h"
#include "window/ipc_mode_window.h"
#include "window/launcher_window.h"
#include "window/preview_window.h"
#include "window/ipc_mode_window.h"
#include "window/playback_window.h"
#include "window/bind_window.h"
#include "window/setting_handler_window.h"
#include "window/status_bar_window.h"
#include "window/user_msg.h"
#include "bll_presenter/gui_presenter_base.h"
#include "bll_presenter/presenter.h"
#include "bll_presenter/screensaver.h"
//#include "bll_presenter/closescreen.h"
#include "bll_presenter/audioCtrl.h"
#include "device_model/system/power_manager.h"

#include "window/status_bar_bottom_window.h"
#include "window/newSettingWindow.h"
#include "window/setting_window_new.h"


#include <thread>

#undef LOG_TAG
#define LOG_TAG "WindowManager.cpp"

using namespace std;

#if 0
#define pthread_mutex_lock(lock)  do { \
    db_msg("lock"); \
    pthread_mutex_lock(lock); \
    db_msg("locked"); \
} while(0)

#define pthread_mutex_unlock(lock)  do { \
    db_msg("unlock"); \
    pthread_mutex_unlock(lock); \
    db_msg("unlocked"); \
} while(0)
#endif


WindowManager::WindowManager()
    : r_(NULL)
    , app_(NULL)
    , ui_loaded_(false)
    , acc_on_flag_(true)
    , ignor_poweroff_msg_flag_(false)
    , usb_window_show(false)
    , powerkey_msg_flag_(false)
{
    current_winid = WINDOWID_INVALID;
    window_map_.clear();
    win_presenter_map_.clear();
}

Window* WindowManager::CreateWindowById(int window_id)
{
    Window *window = NULL;
    switch(window_id) {
        case WINDOWID_STATUSBAR:
            window = new StatusBarWindow(NULL);
            break;
        case WINDOWID_STATUSBAR_BOTTOM:
            window = new StatusBarBottomWindow(NULL);
            break;
        case WINDOWID_PREVIEW:
            window = new PreviewWindow(NULL);
            break;
        case WINDOWID_PLAYBACK:
            window = new PlaybackWindow(NULL);
            break;
        case WINDOWID_SETTING_NEW://add by zhb
            window = new NewSettingWindow(NULL);
            break;
        default:
            db_error("no window found:%d",window_id);
            return NULL;
    }
    window->SetTag(window_id);
    window->SetWindowCallback(this);
    window_map_.insert(make_pair(window_id, window));

    /* 绑定窗口到presenter */
    map<WindowID, IGUIPresenter*>::iterator iter = win_presenter_map_.find((WindowID)window_id);
    if (iter != win_presenter_map_.end()) {
        IGUIPresenter *presenter = iter->second;
        db_warn("habo---> windowid = %d",window_id);
        presenter->BindGUIWindow(window);
    }

    return window;
}

Window* WindowManager::GetWindow(int window_id)
{
    WindowMap::iterator iter = window_map_.find(window_id);
    if (iter != window_map_.end()) {
        return iter->second;
    } else {
        return CreateWindowById(window_id);
    }
    return NULL;
}

void WindowManager::ChangeWindow(int src_window_id, int dst_window_id,bool preview_init)
{
    Window *src_from = NULL, *dst_from = NULL;

    db_msg("ChangeWindow:src====%d, dst===%d", src_window_id, dst_window_id);
    if (dst_window_id > 0) {
        dst_from = GetWindow(dst_window_id);

        if (dst_from == NULL) {
            db_error("dst_from == NULL, change window failed");
            return;
        }
		current_winid = (WindowID)dst_window_id;
    }

    if (src_window_id > 0) {
        src_from = GetWindow(src_window_id);
        if (src_from == NULL) {
            db_error("src_from == NULL, change window failed");
            return;
        }
    }

    db_debug("src win[%d], dst win[%d]", src_window_id, dst_window_id);

    if (src_from) {
        src_from->Hide();
        map<WindowID, IGUIPresenter*>::iterator s_iter = win_presenter_map_.find((WindowID)src_window_id);
        if (s_iter != win_presenter_map_.end()) {
            IGUIPresenter *src_win_presenter = s_iter->second;
            src_win_presenter->OnWindowDetached();
        }
    }

    if (dst_from) {
         if (dst_window_id == WINDOWID_SETTING_NEW) {
            setenv("MOSE_MOVE_ADJUST", "1", 1);
        } else {
            setenv("MOSE_MOVE_ADJUST", "0", 1);
        }
        dst_from->Show();
        map<WindowID, IGUIPresenter *>::iterator d_iter = win_presenter_map_.find((WindowID) dst_window_id);
        if (d_iter != win_presenter_map_.end()) {
            IGUIPresenter *dst_win_presenter = d_iter->second;
            // std::thread([=] {
                // prctl(PR_SET_NAME, "OnWindowLoaded", 0, 0, 0);
            if(preview_init != true){
                dst_win_presenter->OnWindowLoaded();
            }
            // }).detach();
        }
    }
}

void WindowManager::notify(Window *form, int message, int val)
{
    db_msg("message %d val %d", message, val);
	if( form == NULL)
	{
		db_warn("form is null\n");
		return ;
	}
	
    int win_id = form->GetTag();
    switch(message) {
        case WM_WINDOW_CHANGE:
            if(win_id == WINDOWID_PREVIEW)
            {
                //hide WINDOWID_STATUSBAR_BOTTOM
                ChangeWindow(WINDOWID_STATUSBAR_BOTTOM,-1);
            }else if(win_id == WINDOWID_PLAYBACK)
            {
                // hide WINDOWID_STATUSBAR
                ChangeWindow(WINDOWID_STATUSBAR,-1);
            }
            ChangeWindow(win_id, val);
            return;
        default:
            break;
    }

    thread handlemsg = thread(HandlePostMessage, form, message, val, this);
    handlemsg.detach();
}

int WindowManager::CommonKeyProc(int win_id, int msg, int val)
{
    //db_msg("window id: %d", win_id);
    db_warn("[ghy], ===========window id: %d", win_id);
    bool ignore = false;

    switch (msg) {
        case MSG_KEYUP:
            break;
        case MSG_KEYDOWN:
            if (val != 0) {
                ignore = false;
                if (!PowerManager::GetInstance()->IsScreenOn() &&
                        !EyeseeLinux::Screensaver::GetInstance()->GetHdmiConnect()) {
                    ignore = true;
                }
                if (win_id == WINDOWID_PLAYBACK) {
                    PlaybackWindow *playback_win = static_cast<PlaybackWindow*>(GetWindow(win_id));
//                    if (playback_win->getPlayerStatus() != PlaybackWindow::STOPED) break;
                } else if (win_id == WINDOWID_PREVIEW) {
                    PreviewWindow *preview_win = static_cast<PreviewWindow*>(GetWindow(win_id));
                    int win_status = preview_win->GetWindowStatus();
                    // 拍照音与按键音不能重叠
                    if (win_status == STATU_PHOTO) {
                        if (val == SDV_KEY_OK) break;
                    }
                }
            }
            break;
        case MSG_KEYLONGPRESS:
            if (val == MSG_SYSTEM_SHUTDOWN) {
                auto iter = win_presenter_map_.find(WINDOWID_PREVIEW);
                if (iter != win_presenter_map_.end()) {
                    win_presenter_map_[WINDOWID_PREVIEW]->HandleGUIMessage(MSG_SYSTEM_SHUTDOWN, 1);
                }
            }
            break;
        default:
            break;
    }

    return (ignore?1:0);
}

int WindowManager::sendmsg(Window *form, int msg, int val)
{
	printf("manager\n");
    int ret = 0;
    //db_warn("guohy........  window=======%s", form->wname.c_str());
    int win_id = form->GetTag();

    db_msg("msg %d, val %d", msg, val);
    switch(msg)
    {
        case WM_WINDOW_CHANGE:
            {
		printf("WM_WINDOW_CHANGE\n");
                if(win_id == WINDOWID_PREVIEW)
                {
                    if(val == WINDOWID_PLAYBACK){
                        printf("WINDOWID_PLAYBACK\n");
			ChangeWindow(WINDOWID_STATUSBAR,-1);//hide top statusbar
                        ChangeWindow(WINDOWID_STATUSBAR_BOTTOM,-1);
			printf("WINDOWID_PLAYBACK\n");
//                       ChangeWindow(-1,WINDOWID_STATUSBAR_BOTTOM);//show
                    }
                    if(val == WINDOWID_SETTING_NEW)
                    {
                        db_warn("---------------------------- setting new hide the statuar ");
                        ChangeWindow(WINDOWID_STATUSBAR,-1);//hide
                        ChangeWindow(WINDOWID_STATUSBAR_BOTTOM,-1);//hide bottom statusbar
                    }
                }
                else if(win_id == WINDOWID_PLAYBACK)
                {
                    ChangeWindow(-1,WINDOWID_STATUSBAR);//show 
                    ChangeWindow(-1,WINDOWID_STATUSBAR_BOTTOM);//show
                }else if(win_id == WINDOWID_SETTING_NEW)
                {
                    ChangeWindow(-1,WINDOWID_STATUSBAR);//show
                    ChangeWindow(-1,WINDOWID_STATUSBAR_BOTTOM);//show
                }

                ChangeWindow(win_id, val);
                return 0;
            }
            break;
        case MSG_KEYUP:
        case MSG_KEYDOWN:
        case MSG_KEYLONGPRESS:
            return CommonKeyProc(win_id, msg, val);
        default:
            break;
    }

    map<WindowID, IGUIPresenter*> &presenter_map = win_presenter_map_;
    if (msg > USER_MSG_BASE && msg < WM_BASE) {
        map<WindowID, IGUIPresenter*>::iterator iter = presenter_map.find((WindowID)win_id);
        if (iter != presenter_map.end()) {
            ret = presenter_map[(WindowID)win_id]->HandleGUIMessage(msg, val);
        } else {
            // try parent window;
            db_msg("this window no presenter bind, try send to parent window");
            win_id = form->GetParentWindow()->GetTag();
            iter = presenter_map.find((WindowID)win_id);
            if (iter != presenter_map.end()) {
                ret = presenter_map[(WindowID)win_id]->HandleGUIMessage(msg, val);
            }
        }
    }
    return ret;
}

void WindowManager::Init(WindowID entry_win)
{
    entry_win_ = entry_win;
    app_ = Application::GetApp();
    app_->RegistCallback([&] {
        for (auto presenter : win_presenter_map_) {
            ui_loaded_ = true;
            presenter.second->OnUILoaded();
        }
    });

    this->ChangeWindow(-1, WINDOWID_SETTING_NEW);

	this->ChangeWindow(-1, WINDOWID_STATUSBAR_BOTTOM);
	this->ChangeWindow(-1, WINDOWID_STATUSBAR);
    
    this->ChangeWindow(WINDOWID_SETTING_NEW, WINDOWID_PLAYBACK);
	if(entry_win_ == WINDOWID_PREVIEW){
        //this->ChangeWindow(WINDOWID_PLAYBACK, WINDOWID_BINDING);
		 this->ChangeWindow(WINDOWID_PLAYBACK, entry_win_,true);
		 PlaybackWindow *p_win = reinterpret_cast<PlaybackWindow *>(GetWindow(WINDOWID_PLAYBACK));
		 p_win->SetPreviewButtonStatus();
	}else{
		this->ChangeWindow(WINDOWID_PREVIEW, entry_win_);
		//this->ChangeWindow(WINDOWID_PLAYBACK, entry_win_);
		PlaybackWindow *p_win = reinterpret_cast<PlaybackWindow *>(GetWindow(WINDOWID_PLAYBACK));
		p_win->SetPreviewButtonStatus();
		}

	this->ResetConfigLua();
//	if(entry_win_ == WINDOWID_PREVIEW )
//	{
//		PreviewWindow *p_win = static_cast<PreviewWindow*>(GetWindow(WINDOWID_PREVIEW));
//		p_win->ShowCamBRecordIcon();
//	}
	this->current_winid = entry_win_;
	//this->sendUsbConnectMessage();
	printf("Init1\n");
	DetectSdcardSpeedAndVersion();
	printf("Init2\n");

}
void WindowManager::DetectSdcardSpeedAndVersion()
{
	if(entry_win_ == WINDOWID_PREVIEW){
		PreviewWindow *pre_win = reinterpret_cast<PreviewWindow *>(GetWindow(WINDOWID_PREVIEW));//modify by zhb 2018.04.17
		//if(pre_win->IsFileSystemError())
			if(pre_win->IsHighCalssCard())
				pre_win->DetectSdcardNewVersion();
	}
	
}

void WindowManager::sendUsbConnectMessage()
{
    map<WindowID, IGUIPresenter*>::iterator pre_iter = win_presenter_map_.find(WINDOWID_PREVIEW);
    if (pre_iter != win_presenter_map_.end())
    {
        pre_iter->second->sendUsbConnectMessage();
        db_msg("send usb connect message to WINDOWID_PREVIEW\n");
    }
}

void WindowManager::ResetConfigLua()
{
    db_msg("[fangjj]: ResetConfigLua----");		
    map<WindowID, IGUIPresenter*>::iterator pre_iter = win_presenter_map_.find(WINDOWID_SETTING_NEW);

    if (pre_iter != win_presenter_map_.end()) {
		pre_iter->second->NotifyAll();
     }	
}

void WindowManager::MsgLoop()
{
    Application::GetApp()->Run();
    ui_loaded_ = false;
}

void WindowManager::SetGUIPresenter(const map<WindowID, IGUIPresenter*> &win_presenter_map)
{
    win_presenter_map_ = win_presenter_map;
}

const std::map<WindowID, IGUIPresenter*> WindowManager::GetGuiPresenter()
{
    return win_presenter_map_;
}

void WindowManager::HandlePostMessage(Window *win, int msg, int val, WindowManager *self)
{
       prctl(PR_SET_NAME, "WinMsgPost", 0, 0, 0);
	if( win == NULL || self == NULL )
	{
		db_warn("win/self is null win:%p self:%d",win, self);
		return ;
	}
    WindowID win_id = (WindowID)win->GetTag();
    map<WindowID, IGUIPresenter*> &presenter_map = self->win_presenter_map_;

    if (msg > USER_MSG_BASE && msg < WM_BASE) {
        map<WindowID, IGUIPresenter*>::iterator iter = presenter_map.find(win_id);
        if (iter != presenter_map.end()) {
            lock_guard<mutex> lock(self->post_msg_mutex_);
            presenter_map[win_id]->HandleGUIMessage(msg, val);
        }else{
            db_msg("this window no presenter bind, try send to parent window");
            win_id = (WindowID)win->GetParentWindow()->GetTag();
            iter = presenter_map.find((WindowID)win_id);
            if (iter != presenter_map.end()) {
                presenter_map[(WindowID)win_id]->HandleGUIMessage(msg, val);
            }
        }
    }
}

void WindowManager::DoExit()
{
    // 1. terminate minigui
    app_->Terminate();

    // 2. destruct all window object
    WindowMap::iterator iter;
    for (iter = window_map_.begin(); iter != window_map_.end();) {
        map<WindowID, IGUIPresenter*>::iterator pre_iter = win_presenter_map_.find((WindowID)iter->first);
        if (pre_iter != win_presenter_map_.end()) {
            pre_iter->second->OnWindowDetached();
        }

        delete iter->second;
        window_map_.erase(iter++);
    }

    ui_loaded_ = false;
}

WindowID WindowManager::GetCurrentWinID()
{
    return current_winid;
}

void WindowManager::SetAccOnFlag(bool acc_flag)
{
    acc_on_flag_ = acc_flag;
}

bool WindowManager::GetAccOnFlag()
{
    return acc_on_flag_;
}

void WindowManager::SetIgnorPowerOffMsgFlag(bool ignor_flag)
{
    ignor_poweroff_msg_flag_ = ignor_flag;
}

bool WindowManager::GetIgnorPowerOffMsgFlag()
{
    return ignor_poweroff_msg_flag_;
}

bool WindowManager::GetUSBWindowShowFlag()
{
    return usb_window_show;
}

bool WindowManager::SetUSBWindowShowFlag(bool usb_show_flag)
{
    usb_window_show = usb_show_flag;
}

void WindowManager::SetPowerKeyMsgFlag(bool powerkey_msg_flag)
{
    powerkey_msg_flag_ = powerkey_msg_flag;
}

bool WindowManager::GetPowerKeyMsgFlag()
{
   return powerkey_msg_flag_;
}
