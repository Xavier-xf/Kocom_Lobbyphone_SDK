/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file status_bar_window.cpp
 * @brief 状态栏窗口
 * @author id:826
 * @version v0.3
 * @date 2016-07-01
 */
//#define NDEBUG

#include "window/playback_window.h"
#include "window/status_bar_window.h"
#include "debug/app_log.h"
#include "resource/resource_manager.h"
#include "widgets/view_container.h"
#include "window/window_manager.h"
#include "common/message.h"
#include "common/posix_timer.h"
#include "common/setting_menu_id.h"
#include "application.h"
#include "bll_presenter/device_setting.h"
#include "device_model/system/power_manager.h"
#include "device_model/menu_config_lua.h"
#include "device_model/storage_manager.h"
#include "device_model/system/event_manager.h"
#include <sstream>
#include <iomanip>
//#define S_WIFI_ICON
//#define S_ADAS_ICON
#define S_GPS_ICON
#define BATTERY_DETECT_TIME 1
#define GPS_DETECT_TIME 1

using namespace std;

IMPLEMENT_DYNCRT_CLASS(StatusBarWindow)

/*****************************************************************************
 Function: ContainerWidget::HandleMessage
 Description: process the messages and notify the children
    @override
 Parameter:
 Return:
*****************************************************************************/
int StatusBarWindow::HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam)
{
    switch ( message ) {
    case MSG_PAINT:
        //db_warn("habo---> statusbartop window  MSG_PAINT !!!");
        return HELP_ME_OUT;
    default:
        return ContainerWidget::HandleMessage( hwnd, message, wparam, lparam );
   }
}

StatusBarWindow::StatusBarWindow(IComponent *parent)
    : SystemWindow(parent)
    , win_status_(STATU_PREVIEW)
    , current_battery_level_(-1)
    , current_gps_signal_level_(-1)
    , record_status_flag_(false)
    , app_connected_flag_(false)
    , record_time_count_(0)
    , statusbar_blackground_(0x66000000)
    , rec_time_label_(NULL)
    , time_label_(NULL)
    , wifi_icon_(NULL)
    , lock_icon_(NULL)
    , tf_icon_(NULL)
    , voice_ctrl_icon_(NULL)
    , voice_icon_(NULL)
    , gps_icon_(NULL)
    , battery_icon_(NULL)
    , rec_hint_icon_(NULL)

{
    wname = "StatusBarWindow";
    Load();
    R::get()->SetLangID(GetModeConfigIndex(SETTING_DEVICE_LANGUAGE));
    SetBackColor(statusbar_blackground_);  // apha b2

    time_label_ = reinterpret_cast<TextView *>(GetControl("time_label"));
    //::SetWindowFont(time_label->GetHandle(),R::get()->GetFontBySize(32));
    // time_label->SetTimeCaption("- - : - -");
    time_label_->SetCaptionColor(0xFFFFFFFF);
    time_label_->SetBackColor(statusbar_blackground_);  //改变字体背景色后需要同步修改字体控件SetBrushColor(hdc, 0x00000000);
    //time_label->SetBackColor(0x96000000);
    //time_label->SetTextStyle(DT_VCENTER|DT_CENTER);

    create_timer(this, &timer_id_data, DateUpdateProc);
    stop_timer(timer_id_data);
    set_period_timer(1, 0, timer_id_data);

    voice_icon_ = reinterpret_cast<GraphicView *>(GetControl("voice_icon"));
    voice_ctrl_icon_ = reinterpret_cast<GraphicView *>(GetControl("voice_ctrl_icon"));
    lock_icon_ = reinterpret_cast<GraphicView *>(GetControl("lock_icon"));
    gps_icon_ = reinterpret_cast<GraphicView *>(GetControl("gps_icon"));
    wifi_icon_ = reinterpret_cast<GraphicView *>(GetControl("wifi_icon"));
    tf_icon_ = reinterpret_cast<GraphicView *>(GetControl("tf_icon"));
    battery_icon_ = reinterpret_cast<GraphicView *>(GetControl("battery_icon"));
    InitRecordTimeUi();
#ifdef S_WIFI_ICON
    ShowWifiIcon(true);
#endif

    ShowLockStatusIcon(true,0);

    ShowSdCardIcon(true);
    ShowVoiceCtrlIcon(true);
    ShowVoiceIcon(true);
#ifdef S_GPS_ICON
    UpdateGpsIconStatus(true,1);
    ThreadCreate(&gps_signal_thread_id_, NULL, StatusBarWindow::GpsSignalDetectThread, this);
#endif

    ThreadCreate(&battery_detect_thread_id_, NULL, StatusBarWindow::BatteryDetectThread, this);
}

StatusBarWindow::~StatusBarWindow()
{
    db_msg("destruct");
    if( battery_detect_thread_id_ > 0 )
        pthread_cancel(battery_detect_thread_id_);
    if( gps_signal_thread_id_ > 0 )
        pthread_cancel(gps_signal_thread_id_);

    ::stop_timer(rechint_timer_id_);
    ::delete_timer(rechint_timer_id_);

    ::stop_timer(timer_id_data);
    ::delete_timer(timer_id_data);

    ::stop_timer(recording_timer_id_);
    ::delete_timer(recording_timer_id_);

    if(rec_hint_icon_)
        GraphicView::UnloadImage(rec_hint_icon_);
    if(battery_icon_)
        GraphicView::UnloadImage(battery_icon_);
    if(gps_icon_)
        GraphicView::UnloadImage(gps_icon_);
    if(voice_icon_)
        GraphicView::UnloadImage(voice_icon_);
    if(voice_ctrl_icon_)
        GraphicView::UnloadImage(voice_ctrl_icon_);
    if(tf_icon_)
        GraphicView::UnloadImage(tf_icon_);
    if(lock_icon_)
        GraphicView::UnloadImage(lock_icon_);
    if(wifi_icon_)
        GraphicView::UnloadImage(wifi_icon_);
}

void StatusBarWindow::DateUpdateProc(union sigval sigval)
{
    char buf[32] = {0};
    struct tm * tm=NULL;
    time_t timer;
    prctl(PR_SET_NAME, "UpdateDateTime", 0, 0, 0);

    timer = time(NULL);
    tm = localtime(&timer);
    snprintf(buf, sizeof(buf),"%04d-%02d-%02d  %02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

    StatusBarWindow *status_bar = reinterpret_cast<StatusBarWindow*>(sigval.sival_ptr);
    static int initbar_count = 0;
    if(status_bar->win_status_ ==  STATU_PREVIEW){
        if(initbar_count <= 4){
            status_bar->time_label_->SetCaption(buf);
            initbar_count ++;
       }else{
           status_bar->time_label_->SetTimeCaption(buf);
       }
    }
    else
        status_bar->time_label_->SetTimeCaption("");
}

void StatusBarWindow::InitRecordTimeUi()
{
    rec_hint_icon_ = reinterpret_cast<GraphicView *>(GetControl("rec_hint_icon"));
    GraphicView::LoadImage(rec_hint_icon_, "rec_hint");
    rec_hint_icon_->Hide();
    rec_time_label_ = reinterpret_cast<TextView *>(GetControl("rec_time_label"));
    rec_time_label_->SetCaptionColor(0xFFFFFFFF);
    //rec_time_label_->SetBackColor(statusbar_blackground_);
    rec_time_label_->SetTimeCaption("00:00");
    rec_time_label_->Hide();
    create_timer(this, &recording_timer_id_,RecordingTimerProc);
    stop_timer(recording_timer_id_);
    create_timer(this, &rechint_timer_id_, RecHintTimerProc);
    stop_timer(rechint_timer_id_);
}

void StatusBarWindow::RecordStatusTimeUi(bool rec_start)
{
    if(rec_start)
    {
        record_time_count_ = 0;
//        if(!rec_hint_icon_->GetVisible())
//            rec_hint_icon_->Show();
//        set_period_timer(1, 0, rechint_timer_id_);
        if(!rec_time_label_->GetVisible())
             rec_time_label_->Show();
        set_period_timer(1, 0, recording_timer_id_);
    }else
    {
//        stop_timer(rechint_timer_id_);
//        rec_hint_icon_->Hide();
        rec_time_label_->SetTimeCaption("");
        rec_time_label_->Hide();
        stop_timer(recording_timer_id_);
    }
}

void StatusBarWindow::ResetRecordTime()
{
    db_warn("mRecordTime: %d, need reset to 0", record_time_count_);
    record_time_count_ = 0;
}

int StatusBarWindow::GetCurrentRecordTime()
{
    return record_time_count_;
}

void StatusBarWindow::RecordingTimerProc(union sigval sigval)//add by zhb
{
    prctl(PR_SET_NAME, "UpdateRecordTime", 0, 0, 0);
    char buf[32] = {0};
    int rec_time = 0;
    StatusBarWindow *status_bar= reinterpret_cast<StatusBarWindow*>(sigval.sival_ptr);
    rec_time =status_bar->record_time_count_++;
    sprintf(buf, "%02d:%02d",rec_time/60%60, rec_time%60);
    status_bar->rec_time_label_->SetTimeCaption(buf);
}

void StatusBarWindow::RecHintTimerProc(union sigval sigval)
{
    prctl(PR_SET_NAME, "UpdateRecHint", 0, 0, 0);

    static bool flag = false;
    StatusBarWindow *self = reinterpret_cast<StatusBarWindow*>(sigval.sival_ptr);

    if (flag)
        self->rec_hint_icon_->Hide();
    else
        self->rec_hint_icon_->Show();

    flag = !flag;
}

void StatusBarWindow::ShowWifiIcon(bool show_flag)
{
	int index =-1;
    if(show_flag)
    {
        index= GetModeConfigIndex(SETTING_WIFI_SWITCH);
        if(index == 1){
            GraphicView::UnloadImage(wifi_icon_);
            GraphicView::LoadImage(wifi_icon_, "status_top_wifi_on");
        }else{
            GraphicView::UnloadImage(wifi_icon_);
            GraphicView::LoadImage(wifi_icon_, "status_top_wifi_off");
        }
        wifi_icon_->Show();
    }
    else
    {
        wifi_icon_->Hide();
    }
}

void StatusBarWindow::ShowAppConnectIcon(bool show_flag)
{
    if(show_flag)
    {
         //load the app connected icon
         GraphicView::UnloadImage(wifi_icon_);
         GraphicView::LoadImage(wifi_icon_,"status_top_app_connect");
         wifi_icon_->Show();
    }else{
         wifi_icon_->Hide();
    }
}

void StatusBarWindow::ShowLockStatusIcon(bool show_flag, int switch_flag)
{
    if(show_flag)
    {
        db_msg("[zhb]:AdasIconHander  index:[%d]\n", switch_flag);
        if(switch_flag == 1){
            GraphicView::UnloadImage(lock_icon_);
            GraphicView::LoadImage(lock_icon_, "status_top_lock"); //on
        }else{
            GraphicView::UnloadImage(lock_icon_);
            GraphicView::LoadImage(lock_icon_, "status_top_unlock");
        }
        lock_icon_->Show();
    }
    else
    {
        lock_icon_->Hide();
    }
}

void StatusBarWindow::ShowSdCardIcon(bool show_flag)
{
    int index =-1;
    if(show_flag)
    {
        StorageManager *sm = StorageManager::GetInstance();
        db_debug("sd status %d",sm->GetStorageStatus());
        if ((sm->GetStorageStatus() == UMOUNT || sm->GetStorageStatus() == STORAGE_FS_ERROR)){
            GraphicView::UnloadImage(tf_icon_);
            GraphicView::LoadImage(tf_icon_,"status_top_sd_off");//off
        }else{
            GraphicView::UnloadImage(tf_icon_);
            GraphicView::LoadImage(tf_icon_, "status_top_sd_on");
        }
        tf_icon_->Show();
    }
    else
    {
        tf_icon_->Hide();
    }
}

void StatusBarWindow::ShowVoiceCtrlIcon(bool show_flag)
{
    int index =-1;
    if(show_flag)
    {
        index= GetModeConfigIndex(SETTING_VOICE_CTRL);
        db_debug("[zhb]:voice ctrl index:[%d]\n", index);
        if(index == 1){
            GraphicView::UnloadImage(voice_ctrl_icon_);
            GraphicView::LoadImage(voice_ctrl_icon_, "status_top_voice_ctrl_on"); //on
        }else{
            GraphicView::UnloadImage(voice_ctrl_icon_);
            GraphicView::LoadImage(voice_ctrl_icon_, "status_top_voice_ctrl_off");
        }
        voice_ctrl_icon_->Show();
    }
    else
    {
        voice_ctrl_icon_->Hide();
    }
}

void StatusBarWindow::ShowVoiceIcon(bool show_flag)
{
    int index =-1;
    if(show_flag)
    {
        index= GetModeConfigIndex(SETTING_RECORD_VOLUME_SWITCH);
        db_msg("[zhb]:VoiceIconHandler  index:[%d]\n", index);
        switch(index)
        {
            case 0:
                GraphicView::UnloadImage(voice_icon_);
                GraphicView::LoadImage(voice_icon_, "status_top_voice_off");
                voice_icon_->Show();
                break;
            case 1:
                GraphicView::UnloadImage(voice_icon_);
                GraphicView::LoadImage(voice_icon_, "status_top_voice_on");
                voice_icon_->Show();
                break;
            default:
                break;
        }
    } else {
        voice_icon_->Hide();
    }
}

void StatusBarWindow::GetCreateParams(CommonCreateParams& params)
{
    params.style = WS_NONE;
    params.exstyle = WS_EX_NONE | WS_EX_TOPMOST;
    params.class_name = " ";
    params.alias      = GetClassName();
}

string StatusBarWindow::GetResourceName()
{
    return string(GetClassName());
}

void StatusBarWindow::DoHide()
{
    WindowManager *wm = WindowManager::GetInstance();
    Window *cur_win = wm->GetWindow(wm->GetCurrentWinID());
    //::SetActiveWindow(cur_win->GetHandle());
    //::EnableWindow(cur_win->GetHandle(), true);
    Widget::Hide();
}

void StatusBarWindow::PreInitCtrl(View *ctrl, string &ctrl_name)
{
    printf("PreInitCtrl\n");
    if (ctrl_name == "time_label" || ctrl_name == "rec_time_label")
    {
        ctrl->SetCtrlTransparentStyle(false);
        TextView* time_label_ = reinterpret_cast<TextView *>(ctrl);
        time_label_->SetTextStyle(DT_VCENTER|DT_CENTER);
    }
    else
        ctrl->SetCtrlTransparentStyle(true);
}

int StatusBarWindow::GetModeConfigIndex(int msg)
{
    int index =-1;
    MenuConfigLua *mfl=MenuConfigLua::GetInstance();
    index = mfl->GetMenuIndexConfig(msg);
    return index;
}

void StatusBarWindow::Update(MSG_TYPE msg, int p_CamID, int p_recordId)
{
    db_msg("handle msg:%d  win_status_ =%d", msg,win_status_);
    StorageManager *sm = StorageManager::GetInstance();
    switch (msg)
    {
        case MSG_RECORD_AUDIO_ON:
        case MSG_RECORD_AUDIO_OFF:
            ShowVoiceIcon(true);
        break;
        case MSG_WIFI_DISABLED:
        case MSG_SOFTAP_DISABLED:
#ifdef S_WIFI_ICON
            ShowWifiIcon(true);
#endif
        break;
        case MSG_SOFTAP_ENABLE:
        case MSG_SOFTAP_ENABLED:
#ifdef S_WIFI_ICON
            ShowWifiIcon(true);
#endif
        break;
        case MSG_RECFILELOCK_ENABLE:
        {
            ShowLockStatusIcon(true,p_recordId);
        }
        break;
        case MSG_STORAGE_UMOUNT:
        case MSG_STORAGE_MOUNTED:
            ShowSdCardIcon(true);
        break;
        case MSG_PLAYBACK_TO_PREIVEW_CHANG_STATUS_BAR_BOTTOM:
        case MSG_CHANG_STATU_PREVIEW:
        {
            db_debug("change to preview window,update status bar window icon.");
            win_status_ = STATU_PREVIEW;
            //show
#ifdef S_WIFI_ICON
            if(app_connected_flag_)
            {
                ShowWifiIcon(false);
                ShowAppConnectIcon(true);
            }else{
                ShowWifiIcon(true);
            }
#endif
            ShowLockStatusIcon(true,p_recordId);
            ShowSdCardIcon(true);
            ShowVoiceCtrlIcon(true);
            ShowVoiceIcon(true);
#ifdef S_GPS_ICON
            UpdateGpsIconStatus(true,4);
#endif
            UpdateBatteryIconStatus(true,6);

            //start system time timer
            StartRecTimer(true);

        }
        break;
        case MSG_PLAY_TO_PLAYBACK_WINDOW:
        case MSG_CHANG_STATU_PLAYBACK:
        {
            db_debug("change to playback window,update status bar window icon.");
            win_status_ = STATU_PLAYBACK;
            //hide
#ifdef S_WIFI_ICON
            ShowWifiIcon(false);
            ShowAppConnectIcon(false);
#endif
            ShowLockStatusIcon(false,p_recordId);
            ShowSdCardIcon(false);
            ShowVoiceCtrlIcon(false);
            ShowVoiceIcon(false);
#ifdef S_GPS_ICON
            UpdateGpsIconStatus(false,4);
#endif
            UpdateBatteryIconStatus(false,6);

            //stop system time timer
            StartRecTimer(false);
        }
        break;
        case MSG_PREVIW_TO_SETTING_CHANGE_STATUS_BAR:
        {
            db_debug("change to setting window,update status bar window icon.");
            win_status_ = STATU_SETTING;
            //hide 
#ifdef S_WIFI_ICON
            ShowWifiIcon(false);
            ShowAppConnectIcon(false);
#endif
            ShowLockStatusIcon(false,p_recordId);
            ShowSdCardIcon(false);
            ShowVoiceCtrlIcon(false);
            ShowVoiceIcon(false);
#ifdef S_GPS_ICON
            UpdateGpsIconStatus(false,4);
#endif
            UpdateBatteryIconStatus(false,6);

            //stop system time timer
            StartRecTimer(false);
        }
        break;
        case MSG_PLAYBACK_TO_PLAY_WINDOW:
            db_msg("[debug_zhb]----MSG_PLAYBACK_TO_PLAY_WINDOW");
            win_status_ = STATU_PLAYBACK;
        break;

        case MSG_APP_IS_CONNECTED:
            db_warn("msg is MSG_APP_IS_CONNECTED should update the icon\n");
            app_connected_flag_ = true;
            ShowWifiIcon(false);
            ShowAppConnectIcon(true);
        break;
        case MSG_APP_IS_DISCONNECTED:
            db_warn("msg is MSG_APP_IS_DISCONNECTED should update the icon\n");
            app_connected_flag_ = false;
            ShowAppConnectIcon(false);
            ShowWifiIcon(true);
        break;
        default:
            break;
    }
}

void StatusBarWindow::RecHintIcon(bool flag)
{
    if(flag){
        if(!rec_hint_icon_->GetVisible())
            rec_hint_icon_->Show();
        set_period_timer(1, 0, rechint_timer_id_);
    } else {
        stop_timer(rechint_timer_id_);
        rec_hint_icon_->Hide();
    }
}

void StatusBarWindow::StartRecTimer(bool flag)
{
    if(flag){
        set_period_timer(1, 0, timer_id_data);
        time_label_->Show();
    }
    else{
        stop_timer(timer_id_data);
        time_label_->Hide();
    }
}

void StatusBarWindow::OnLanguageChanged()
{

}

void StatusBarWindow::GetIndexStringArray(string array_name,  string &result, int index)
{
    StringVector title_str1;
    vector<string>::const_iterator it;

    R::get()->GetStringArray(array_name, title_str1);
    it = title_str1.begin()+index;
    result =(*it).c_str();
}

void StatusBarWindow::GetString(string array_name, string &result)
{
    string title_str1;
    R::get()->GetString(array_name, title_str1);
    result = title_str1;
}

int StatusBarWindow::GetStringArrayIndex(int msg)
{
    int index =-1;
    MenuConfigLua *mfl=MenuConfigLua::GetInstance();
    index = mfl->GetMenuIndexConfig(msg);
    return index;
}

void StatusBarWindow::UpdateGpsIconStatus(bool show_flag,int index)
{
    db_msg("[zhb]:gpsIconHandler  index:[%d]\n", index);

    switch(index)
    {
        case 0:
            GraphicView::UnloadImage(gps_icon_);
            GraphicView::LoadImage(gps_icon_, "status_top_GPS_level_0");
            break;
        case 1:
            GraphicView::UnloadImage(gps_icon_);
            GraphicView::LoadImage(gps_icon_, "status_top_GPS_level_1");
            break;
        case 2:
            GraphicView::UnloadImage(gps_icon_);
            GraphicView::LoadImage(gps_icon_, "status_top_GPS_level_2");
            break;
        case 3:
            GraphicView::UnloadImage(gps_icon_);
            GraphicView::LoadImage(gps_icon_, "status_top_GPS_level_3");
            break;
        default:
            break;
    }
    if(show_flag)
        gps_icon_->Show();
    else
        gps_icon_->Hide();

}

void StatusBarWindow::UpdateBatteryIconStatus(bool show_flag,int levelval)
{
    switch (levelval)
    {
        case 0:
            GraphicView::UnloadImage(battery_icon_);
            GraphicView::LoadImage(battery_icon_, "status_top_battery_level_0");
            break;
        case 1:
            GraphicView::UnloadImage(battery_icon_);
            GraphicView::LoadImage(battery_icon_, "status_top_battery_level_1");
            break;
        case 2:
            GraphicView::UnloadImage(battery_icon_);
            GraphicView::LoadImage(battery_icon_, "status_top_battery_level_2");
            break;
        case 3:
            GraphicView::UnloadImage(battery_icon_);
            GraphicView::LoadImage(battery_icon_, "status_top_battery_level_3");
            break;
        case 4:
            GraphicView::UnloadImage(battery_icon_);
            GraphicView::LoadImage(battery_icon_, "status_top_charging");
            break;
        case 5: 
            GraphicView::UnloadImage(battery_icon_);
            GraphicView::LoadImage(battery_icon_, "status_top_connect_pc");
            break;
        default:
            break;
    }
    if(show_flag)
        battery_icon_->Show();
    else
        battery_icon_->Hide();
}

void* StatusBarWindow::BatteryDetectThread(void *context)
{
    int battery_cap_level = -1;
    int batteryStatus;
    StatusBarWindow *status_bar = reinterpret_cast<StatusBarWindow*>(context);
    PowerManager *pm = PowerManager::GetInstance();
    WindowManager *win_mg = ::WindowManager::GetInstance();
    while(1)
    {
        #if 0
        batteryStatus = pm->GetBatteryStatus();
        if ((pm->getACconnectStatus() == 1) && (batteryStatus > 0)) {//Acc on and battery on
            battery_cap_level = 4;
        } else if ((pm->getACconnectStatus() == 1) && (batteryStatus < 0)){//˵?????ز????ڻ??????ػ?????
            battery_cap_level = 5;
        } 
        #endif
        if (pm->getACconnectStatus() == 1 && !PowerManager::GetInstance()->getUsbconnectStatus())
        {
            battery_cap_level = 4;
        }
        else if(pm->getACconnectStatus() == 1 && PowerManager::GetInstance()->getUsbconnectStatus())//connect the pc
        {
            battery_cap_level = 5;
        }
        else 
        {
            battery_cap_level = pm->GetBatteryLevel();
        }
        if(status_bar->current_battery_level_ != battery_cap_level){
            status_bar->current_battery_level_ = battery_cap_level;
            if((win_mg ->GetCurrentWinID() == WINDOWID_PREVIEW) || (win_mg ->GetCurrentWinID() ==  WINDOWID_INVALID)){
                status_bar->UpdateBatteryIconStatus(true,battery_cap_level);
            }else{
                status_bar->UpdateBatteryIconStatus(false,battery_cap_level);
            }
        }
	 
        sleep(BATTERY_DETECT_TIME);
    }
    return NULL;  
}

void* StatusBarWindow::GpsSignalDetectThread(void * context)
{
    int gps_signal_level = -1;
    StatusBarWindow *p_statusBar = reinterpret_cast<StatusBarWindow*>(context);
    EventManager *even_ = EventManager::GetInstance();
    WindowManager *win_mg = ::WindowManager::GetInstance();

    while(1)
    {
        gps_signal_level = even_->GetGpsSignalLevel();
        //db_warn("[debug_jason]: GpsDetectThread: gps_signal_level = %d ,p_statusBar->m_current_gps_signal_level = %d",gps_signal_level,p_statusBar->m_current_gps_signal_level);
        if(p_statusBar->current_gps_signal_level_ != gps_signal_level)
        {
            p_statusBar->current_gps_signal_level_=gps_signal_level;
            if((win_mg ->GetCurrentWinID() == WINDOWID_PREVIEW) || (win_mg ->GetCurrentWinID() ==  WINDOWID_INVALID)){
                p_statusBar->UpdateGpsIconStatus(true,gps_signal_level);
            }else{
                p_statusBar->UpdateGpsIconStatus(false,gps_signal_level);
            }
        }
        sleep(GPS_DETECT_TIME);
    }
    return NULL;  
}


