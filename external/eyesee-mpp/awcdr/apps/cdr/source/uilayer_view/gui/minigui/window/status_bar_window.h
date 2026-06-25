/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file status_bar_window.h
 * @brief 状态栏窗口
 * @author id:826
 * @version v0.3
 * @date 2016-07-01
 */
#pragma once


#include "window/window.h"
#include "window/user_msg.h"
#include "widgets/graphic_view.h"
#include <time.h>
#include <signal.h>
#include "widgets/text_view.h"

class StatusBarWindow : public SystemWindow
{
    DECLARE_DYNCRT_CLASS(StatusBarWindow, Runtime)
    public:
        StatusBarWindow(IComponent *parent);
        virtual ~StatusBarWindow();
        std::string GetResourceName();
        void GetCreateParams(CommonCreateParams& params);
        void PreInitCtrl(View *ctrl, std::string &ctrl_name);
        int HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam);
        void Update(MSG_TYPE msg, int p_CamID=0, int p_recordId=0);
        static void DateUpdateProc(union sigval sigval);
        void GetIndexStringArray(std::string array_name,  std::string &result, int index);
        void GetString(std::string array_name, std::string &result);
        int GetModeConfigIndex(int msg);
        void InitRecordTimeUi();
        void RecordStatusTimeUi(bool mstart);
        int GetStringArrayIndex(int msg);
        void UpdateBatteryIconStatus(bool sb,int levelval);
        void UpdateGpsIconStatus(bool sb,int levelval);
        void OnLanguageChanged();
        static void* BatteryDetectThread(void *context);
        static void* GpsSignalDetectThread(void *context);
        void ShowVoiceIcon(bool sv);
        void StartRecTimer(bool flag);
        void ShowWifiIcon(bool sw);
        void ShowLockStatusIcon(bool sw, int switch_flag);
        void ShowVoiceCtrlIcon(bool sw);
        void ShowSdCardIcon(bool sw);
        static void RecHintTimerProc(union sigval sigval);
        void SetRecordStatusFlag(bool flag){record_status_flag_ = flag;}
        static void RecordingTimerProc(union sigval sigval);
        void ResetRecordTime();
        int GetCurrentRecordTime();
        void ShowAppConnectIcon(bool value);
        void DoHide();
        void RecHintIcon(bool flag);
    private:
        timer_t timer_id_data;
        timer_t rechint_timer_id_;
        timer_t recording_timer_id_;	
        int win_status_;
        int current_battery_level_;
        int current_gps_signal_level_;
        pthread_t battery_detect_thread_id_;
        pthread_t gps_signal_thread_id_;
        bool record_status_flag_;
        bool app_connected_flag_;
        int record_time_count_;
        unsigned long statusbar_blackground_;
        TextView* rec_time_label_;
        TextView* time_label_;
        GraphicView *wifi_icon_;
        GraphicView *lock_icon_;
        GraphicView *tf_icon_;
        GraphicView *voice_ctrl_icon_;
        GraphicView *voice_icon_;
        GraphicView *gps_icon_;
        GraphicView *battery_icon_;
        GraphicView *rec_hint_icon_;
};
