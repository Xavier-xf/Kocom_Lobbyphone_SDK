/*****************************************************************************
 Copyright (C), 2015, AllwinnerTech. Co., Ltd.
 File name: window_manager.h
 Author: yangy@allwinnertech.com
 Version: v1.0
 Date: 2015-11-24
 Description:

 History:
*****************************************************************************/

#ifndef _WINDOW_MANAGER_H_
#define _WINDOW_MANAGER_H_

#include "window/window.h"
#include "type/types.h"
#include "common/observer.h"
#include "common/singleton.h"

#include "bll_presenter/gui_presenter_base.h"

#include <mutex>
#include <map>
#include <time.h>
#include <signal.h>

typedef enum {
   WINDOWID_INVALID =0,
    //WINDOWID_LAUNCHER = 1,
    WINDOWID_PREVIEW,//1
    WINDOWID_PLAYBACK,//2
    WINDOWID_IPCMODE,
    WINDOWID_STATUSBAR,
    WINDOWID_STATUSBAR_BOTTOM,
    WINDOWID_SETTING_NEW,//9
    WINDOWID_SETTING_LISTBOX_VIEW,
} WindowID;


enum {
    STATU_PREVIEW = 0,
    STATU_PHOTO,
    STATU_SLOWRECOD,
    STATU_PLAYBACK,
    STATU_SETTING,
    STATU_BINDING,
};


class R;
class Application;

class WindowManager
    : public WindowListener
    , public EyeseeLinux::Singleton<WindowManager>
{
    friend class EyeseeLinux::Singleton<WindowManager>;
    public:
        void MsgLoop();
        void Init(WindowID entry_win);
        void notify(Window *form, int msg, int val);
        int sendmsg(Window *form, int msg, int val);
        Window* CreateWindowById(int window_id);
        Window* GetWindow(int window_id);
        void ChangeWindow(int src_window_id, int dst_window_id,bool preview_init = false);
        void SetGUIPresenter(const std::map<WindowID, IGUIPresenter*> &win_presenter_map);
        const std::map<WindowID, IGUIPresenter*> GetGuiPresenter();

        // close and destruct all window resource
        void DoExit();
        WindowID GetCurrentWinID();

        inline bool IsUILoaded() const { return ui_loaded_; }

        void ResetConfigLua();
        void sendUsbConnectMessage();

        // NOTE: 该接口会在所有窗口处理自己的按键事件之前调用,
        // 所以这里面禁止进行任何耗时的操作
        int CommonKeyProc(int win_id, int msg, int val);
	    void DetectSdcardSpeedAndVersion();
        void SetAccOnFlag(bool acc_flag);
        bool GetAccOnFlag();
        void SetIgnorPowerOffMsgFlag(bool ignor_flag);
        bool GetIgnorPowerOffMsgFlag();
		bool SetUSBWindowShowFlag(bool usb_show_flag);
		bool GetUSBWindowShowFlag();
        void SetPowerKeyMsgFlag(bool powerkey_msg_flag);
        bool GetPowerKeyMsgFlag();

    private:
        R *r_;
        WindowMap window_map_;
        std::map<WindowID, IGUIPresenter*> win_presenter_map_;
        Application *app_;
        WindowID entry_win_;
        WindowID current_winid;
        std::mutex post_msg_mutex_;
        bool ui_loaded_;
        bool acc_on_flag_;
        bool ignor_poweroff_msg_flag_;
		bool usb_window_show;
        bool powerkey_msg_flag_;
	timer_t timer_id_;
        WindowManager();
        WindowManager(const WindowManager &o);
        WindowManager &operator=(const WindowManager &o);
        ~WindowManager() {};

        static void HandlePostMessage(Window *form, int msg, int val, WindowManager *self);
};

#endif //_WINDOW_MANAGER_H_
