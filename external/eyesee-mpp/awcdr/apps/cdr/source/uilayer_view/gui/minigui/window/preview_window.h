				/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file preview_window.h
 * @brief 单路预览录像窗口
 * @author id:826
 * @version v0.3
 * @date 2016-11-03
 */
#pragma once

#include "window/window.h"
#include "window/user_msg.h"
#include <signal.h>
#include <pthread.h>

#include "ADASEventUIProc.h"
#include "device_model/system/led.h"
#include "window/status_bar_window.h"
#include "window/window_manager.h"
#include "window/promptBox.h"

/**
 * 定义每个窗体内部的button/msg
 */
#define PREVIEW_RECORD_BUTTON       (USER_MSG_BASE+1)
#define PREVIEW_SHOTCUT_BUTTON      (USER_MSG_BASE+2)
#define PREVIEW_GO_PLAYBACK_BUTTON  (USER_MSG_BASE+3)
#define PREVIEW_AUDIO_BUTTON        (USER_MSG_BASE+4)
#define PREVIEW_SWITCH_CAM_BUTTON   (USER_MSG_BASE+5)
#define PREVIEW_CONFIRM_FORMAT      (USER_MSG_BASE+6)
#define PREVIEW_CANCEL_FORMAT       (USER_MSG_BASE+7)
#define PREVIEW_SHUTDOWN_BUTTON     (USER_MSG_BASE+8)
#define PREVIEW_WIFI_SWITCH_BUTTON  (USER_MSG_BASE+9)
#define PREVIEW_SET_DIGHTZOOM_BUTTON (USER_MSG_BASE+10)
#define PREVIEW_SET_RECORD_MUTE     (USER_MSG_BASE+11)
#define PREVIEW_BUTTON_DIALOG_HIDE  (USER_MSG_BASE+12)
#define PREVIEW_WIFI_DIALOG_HIDE    (USER_MSG_BASE+13)
#define PREVIEW_USB_DIALOG_HIDE     (USER_MSG_BASE+14)
#define PREVIEW_LOWPOWER_SHUTDOWN   (USER_MSG_BASE+15)
#define PREVIEW_SWITCH_LAYER		(USER_MSG_BASE+16)
#define PREVIEW_TO_SETTING_BUTTON		(USER_MSG_BASE+17)
#define PREVIEW_TO_SETTINGWINDOW_UPDATE_VERSION		(USER_MSG_BASE+18)
#define PREVIEW_TO_SETTINGWINDOW_UPDATE_4G_VERSION		(USER_MSG_BASE+19)
#define PREVIEW_CAMB_PREVIEW_CONTROL (USER_MSG_BASE+20)
#define PREVIEW_TAKE_PIC_CONTROL (USER_MSG_BASE+21)
#define PREVIEW_EMAGRE_RECORD_CONTROL (USER_MSG_BASE+22)
#define PREVIEW_TO_SETTING_NEW_WINDOW (USER_MSG_BASE+23)
#define PREVIEW_TO_PLAYBACK_WINDOW (USER_MSG_BASE+24)
#define PREVIEW_VIEW_UP		       (USER_MSG_BASE+25)
#define PREVIEW_VIEW_DOWN		   (USER_MSG_BASE+26)
#define PREVIEW_ADAS_ONOFF          (USER_MSG_BASE+27)

#define REVERSELINES_PATH   "/usr/share/minigui/res/images"

typedef enum {
    STATUS_DOWNLOAD_INIT,
    STATUS_DOWNLOAD_SUCC,
    STATUS_DOWNLOAD_FAIL,
    STATUS_DOWNLOAD_NETFAIL,
    STATUS_DOWNLOAD_FSUNMOUNT,
    STATUS_DOWNLOAD_FSFAIL
};

typedef enum {
    //WINDOWID_LAUNCHER = 1,
    WIN_DEFAULT = 0,
    WIN_WIFI,
    WIN_Dialog,
    WIN_USBMode,
    WIN_Format,
} CurWinStatus;
enum {
	PREVIEW_BUTTON_DIALOG_FORMAT_SDCARD= 0,
	PREVIEW_BUTTON_DIALOG_DD_NOTICE,
	PREVIEW_BUTTON_DIALOG_SDCARD_UPDATE_VERSION,
};

typedef enum ReverseLines
{
    REVERSELINES = 0,
    REVERSELINES_WIDTH,
    REVERSELINES_LOW,
    REVERSELINES_LOW_WIDTH,
    NO_REVERSELINES,
};

class Dialog;

class GraphicView;

class TextView;


//class ProgressBar;

//class ShutDownWindow;

class Prompt;

class PromptBox;

class BulletCollection;

class StatusBarWindow;

class USBModeWindow;

class ADASEventUIProc;

class PreviewWindow
        : public SystemWindow {
    DECLARE_DYNCRT_CLASS(PreviewWindow, Runtime)

    public:
        PreviewWindow(IComponent *parent);

        virtual ~PreviewWindow();

        std::string GetResourceName();

        void GraphicViewButtonProc(View *control);

        void GetCreateParams(CommonCreateParams &params);

        void Update(MSG_TYPE msg, int p_CamID=0, int p_recordId=0);

        int HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam);

        void PreInitCtrl(View *ctrl, std::string &ctrl_name);

        void keyProc(int keyCode, int isLongPress);

        void ShowPromptInfo(unsigned int prompt_id,unsigned int showtimes=2,bool m_force = false);

        int HandlerPromptInfo(void);

        int GetRecordStatus();

        int GetWindowStatus();

        bool IsUsbAttach();
#ifdef SHOW_DEBUG_INFO
        void ShowDebugInfo(bool value);

        void ClearDebugInfo();

        void InsertDebugInfo(const std::string &key, const std::string &value);

        void RemoveDebugInfo(const std::string &key);

        void UpdateDebugInfo();

        void UpdateDebugInfo(const std::string &info);
#endif
        void OnLanguageChanged();

        void HidePromptInfo();

        void ShowPromptBox(unsigned int promptbox_id,unsigned int showtimes=2);

        void RecordStatusTimeUi(bool mstart);

        void ResetRecordTime();

        int DetectSdcardNewVersion();

        void HandleButtonDialogMsg(int val);

        bool IsHighCalssCard();

        bool IsFileSystemError();

        bool Md5CheckVersionPacket(std::string p_path,std::string md5Code);

        int ShowCamBRecordIcon();

        int HideCamBRecordIcon();

        int GetCurrentRecordTime();

        void showLockFileUiInfo(int value);

        bool GetIsRecordStartFlag();

        void VideoStopRecordCtl();

//        ShutDownWindow *shutdown_window_;

        inline Prompt *GetPromptPoint(){return prompt_;}

#ifdef ENABLE_ADAS
        void UpdateADASEventMsg(AW_AI_ADAS_DETECT_R__v2 *adas_event);
#endif

        void HideStatusBar();

        void ShowStatusBar();

        void SetAccon_Record_Flag(bool flag);

        void ShowReverseLines(ReverseLines line_id,bool show_flag);

    private:
        int ResumeWindow(int p_nWinId, CurWinStatus p_Status);
        bool IsNewVersion(std::string external_version,std::string local_version);
#ifdef ENABLE_ADAS
        static void ADASUIProcUpdate(union sigval sigval);
#endif

        static void RecHintTimerProc(union sigval sigval);

        void RecHintIcon(bool flag);

    private:
        int win_statu;
        int m_nCurrentWin;
        int m_download_status;
        bool isRecordStart;
        bool isWifiStarting;
        bool isTakepicFinish;
        bool m_bUsbDialogShow;
        bool m_standby_flag;
        bool m_stop2down;
        bool is_start_download;
#ifdef ENABLE_ADAS
        int full_showtime_;
        int alignline_showtime_;
        bool show_fullwarn_flag_;
        bool adas_enable_;
        bool adas_switch_;
        ADASEventUIProc *adas_uidraw_;
        timer_t adas_uiproc_timer_;
        AW_AI_ADAS_DETECT_R__v2 adas_event_;
        pthread_mutex_t adas_lock_;
#endif
        bool accon_recode_;
        bool show_reverseline_flag_;
        ReverseLines line_id_;
        pthread_mutex_t proc_lock_;
        Prompt *prompt_;
        PromptBox *prompt_box_;
        BulletCollection * bullet_collection_;
        WindowManager *win_mg;
        TextView *m_record_info;
        TextView *m_record_info1;
        USBModeWindow * usb_win_;
        GraphicView *rec_hint_icon_;
        GraphicView *reverse_line_icon_;
        timer_t rechint_timer_id_;
        std::map<std::string, std::string> debug_info_;
        BITMAP reverselines_image_;
};
