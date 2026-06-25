/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file preview_window.cpp
 * @brief 鍗曡矾棰勮褰曞儚绐楀彛
 * @author id:826
 * @version v0.3
 * @date 2016-11-03
 */

//#define NDEBUG
#include "window/preview_window.h"
#include "window/usb_mode_window.h"
#include "window/playback_window.h"
#include "window/status_bar_bottom_window.h"
#include "window/user_msg.h"
#include "window/dialog.h"
#include "debug/app_log.h"
#include "widgets/graphic_view.h"
#include "resource/resource_manager.h"
#include "widgets/text_view.h"
#include "common/message.h"
#include "common/posix_timer.h"
#include "application.h"
//#include "window/shutdown_window.h"
//#include "bll_presenter/closescreen.h"
#include "bll_presenter/screensaver.h"
#include "bll_presenter/camRecCtrl.h"
#include "window/prompt.h"
#include "device_model/storage_manager.h"
#include "device_model/system/power_manager.h"
#include "device_model/menu_config_lua.h"
#include "common/setting_menu_id.h"
#include <sstream>
#include <thread>
#include "window/promptBox.h"
#include "window/bulletCollection.h"
#include "device_model/version_update_manager.h"
#include "device_model/system/event_manager.h"
#include <signal.h>
#include <pthread.h>

#include "ADASEventUIProc.h"
#include "device_model/system/watchdog.h"
#include "device_model/download_4g_manager.h"
#include "window/newSettingWindow.h"
#include "bll_presenter/audioCtrl.h"
#include "bll_presenter/statusbarsaver.h"

#undef LOG_TAG
#define LOG_TAG "PreviewWindow"
using namespace std;
using namespace EyeseeLinux;

#define KEEPALIVE_TIMER_ID 1000
//#define OTA_ENABLE

IMPLEMENT_DYNCRT_CLASS(PreviewWindow)

void PreviewWindow::keyProc(int keyCode, int isLongPress)
{
    printf("[debug_joson]: keyCode = %d , inLongPress = %d",keyCode,isLongPress);
    static bool isShutdown = false;
    int ret = pthread_mutex_trylock(&proc_lock_);
    if (ret != 0) {
        db_info("key event is handling, ignore this one, keyCode: %d, isLongPress: %d", keyCode, isLongPress);
        return;
    }
    int m_recodTime = 0;
    switch(keyCode)
    {
        case SDV_KEY_LEFT:// button5 setting
            db_warn("[debug_zhb]:this is CDR_KEY_LEFT key code");
            if(isRecordStart)
            {
                ShowPromptBox(PROMPT_BOX_DEVICE_RECORDING,2);
                break;
            }
            if(!isTakepicFinish)
            {
                db_error("take pic not finish");
                break;
            }
            if(win_statu == STATU_PREVIEW)
            {
                win_statu = STATU_PLAYBACK;
                EyeseeLinux::StatusBarSaver::GetInstance()->Pause(true);
                listener_->sendmsg(this, PREVIEW_TO_SETTING_BUTTON, 0);
                listener_->sendmsg(this,PREVIEW_TO_SETTING_NEW_WINDOW,0);
                listener_->sendmsg(this, WM_WINDOW_CHANGE, WINDOWID_SETTING_NEW);
                win_statu = STATU_PREVIEW;
            }
        break;

        case SDV_KEY_MODE:// button3 file locked
        {
            if(isRecordStart)
            {
                m_recodTime = this->GetCurrentRecordTime();
                listener_->sendmsg(this,PREVIEW_EMAGRE_RECORD_CONTROL, m_recodTime);
            }else{
                //should show please start record first
                ShowPromptBox(PROMPT_BOX_LOCK_RECORD_TIP_FILE,2);
            }
            if(!isTakepicFinish){
                db_error("take pic not finish");
                break;
            }
            db_msg("[habo]:this is SDV_KEY_MODE key code win_statu %d",win_statu);
        }
        break;
        case SDV_KEY_OK://button2  pohote
        {
            #ifdef ENABLE_ADAS
            db_error("adas_enable_ %d isRecordStart %d",adas_enable_,isRecordStart);
            if(adas_enable_ && isRecordStart)
            {
                db_error("adas and recording is start,can not take picture!!");
                ShowPromptBox(PROMPT_BOX_NOT_SUPPORT_TAKEPIC,2);
                break;
            }
            #endif
            db_msg("[debug_zhb]:this is cdr_key_right key code");
            if(((win_statu == STATU_PREVIEW ) && isTakepicFinish))
            {
                if(HandlerPromptInfo() == -1)
                {
                    db_msg("[fangjj]: TF ERROR: no tf or tf full \n");
                    break;
                }
            }
            if(isTakepicFinish){
                isTakepicFinish = false;
                AudioCtrl::GetInstance()->PlaySound(AudioCtrl::AUTOPHOTO_SOUND);
                listener_->sendmsg(this,PREVIEW_TAKE_PIC_CONTROL, 0);
            }else{
                db_warn("[debug_zhb]:take pic is not done\n");
            }
        }
        break;
        case SDV_KEY_MENU: //button1 recording
        {
            if(((win_statu == STATU_PREVIEW ) && !isRecordStart))
            {
                if(HandlerPromptInfo() == -1)
                {
                    db_msg("[fangjj]: TF ERROR: no tf or tf full \n");
                    break;
                }
            }
            if(accon_recode_){
                db_error("accon recoding now, ignor this button recording msg");
                break;
            }
            if(isRecordStart == false)
            {
                db_msg("debug_zhb-----------------ready to recording ");
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 1);
//                ShowPromptBox(PROMPT_BOX_RECORDING_START,2);
            }else{
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 0);
                ShowPromptBox(PROMPT_BOX_RECORDING_STOP,2);
            }
        }
        break;
        case SDV_KEY_RIGHT://button4 to playback
        {
            db_msg("[debug_zhb]:this is SDV_KEY_RIGHT key code");
            if(isRecordStart)
            {
                ShowPromptBox(PROMPT_BOX_DEVICE_RECORDING,2);
                break;
            }
            if(!isTakepicFinish){
                db_error("take pic not finish");
                break;
            }
            if(win_statu == STATU_PREVIEW)
            {
                win_statu = STATU_PLAYBACK;
                EyeseeLinux::StatusBarSaver::GetInstance()->Pause(true);
                listener_->sendmsg(this, PREVIEW_GO_PLAYBACK_BUTTON, win_statu);
                listener_->sendmsg(this, WM_WINDOW_CHANGE, WINDOWID_PLAYBACK);
                win_statu = STATU_PREVIEW;
            }
        }
        break;
        case SDV_KEY_AUDIO:
        {
//            if(isRecordStart)
//            {
//                ShowPromptBox(PROMPT_BOX_DEVICE_RECORDING,2);
//                break;
//            }
            MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
            int val = menuconfiglua->GetMenuIndexConfig(SETTING_RECORD_VOLUME_SWITCH);
            db_msg("==========SDV_KEY_AUDIO the val is %d==========",val);
            if(val){
                db_msg("disable auio record");
                listener_->sendmsg(this, PREVIEW_AUDIO_BUTTON , 0);
            }else{
                db_msg("enable auio record");
                listener_->sendmsg(this, PREVIEW_AUDIO_BUTTON , 1);
            }
        }
        break;
        case SDV_KEY_POWER:
        {
            if(isLongPress && !isShutdown)
            {
                db_error("preview window receive power key long press");
                isShutdown = true;
//                shutdown_window_->Show();
                EyeseeLinux::Screensaver::GetInstance()->ForceScreenOnBySdcard();
                listener_->notify(this, MSG_SYSTEM_SHUTDOWN , 0);
            }
        }
        break;
#ifdef ENABLE_ADAS
        case SDV_KEY_ADAS:
        {
            MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
            if(menuconfiglua->GetMenuIndexConfig(SETTING_RECORD_RESOLUTION)){
                ShowPromptBox(PROMPT_BOX_NOT_SUPPORT_ADAS,2);
                break;
            }
            int val = menuconfiglua->GetMenuIndexConfig(SETTING_ADAS_SWITCH);
            if(val){
                listener_->notify(this,PREVIEW_ADAS_ONOFF, 0);
                ShowPromptBox(PROMPT_BOX_ADAS_STOP,2);
                //::InvalidateRect(GetHandle(),NULL, TRUE);
                db_error("refresh window.");
            }else{
                listener_->notify(this,PREVIEW_ADAS_ONOFF, 1);
                ShowPromptBox(PROMPT_BOX_ADAS_START,2);
            }
        }
        break;
#endif
        default:
            db_msg("[debug_jaosn]:this is invild key code");
        break;
    }
    pthread_mutex_unlock(&proc_lock_);
}

#if 0
int PreviewWindow::ShowCamBRecordIcon()
{
    StorageManager *sm = StorageManager::GetInstance();
    int status = StorageManager::GetInstance()->GetStorageStatus();
    if( (status == UMOUNT) || (status == STORAGE_FS_ERROR) || (status == FORMATTING))
    {
        HideCamBRecordIcon();
        return -1;
    }

	int IsShowing = MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_CAMB_PREVIEWING);
	int IsRecording = MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_REAR_RECORD_RESOLUTION);
	if( IsRecording && isRecordStart)
	{
		if( IsShowing )
		{
			if(GetControl("record_icon1") != NULL)
				GetControl("record_icon1")->Hide();

			if( m_record_info1 != NULL)
				m_record_info1->Hide();

			GraphicView::LoadImage(GetControl("record_icon"), "rec_hint");
			GetControl("record_icon")->Show();
			if( m_record_info == NULL)
			{
				m_record_info = reinterpret_cast<TextView *>(GetControl("record_info"));
				m_record_info->SetCaptionColor(0xFFFFFFFF);//0xFF5f6174
				m_record_info->SetBackColor(0xff000000);
			}
			m_record_info->SetCaption("REC");
			m_record_info->Show();
		}
		else
		{
			if(GetControl("record_icon") != NULL)
				GetControl("record_icon")->Hide();

			if( m_record_info != NULL)
				m_record_info->Hide();

			GraphicView::LoadImage(GetControl("record_icon1"), "rec_hint");
			GetControl("record_icon1")->Show();

			if( m_record_info1 == NULL)
			{
				m_record_info1 = reinterpret_cast<TextView *>(GetControl("record_info1"));
				m_record_info1->SetCaptionColor(0xFFFFFFFF);//0xFF5f6174
				m_record_info1->SetBackColor(0xff000000);
			}
			m_record_info1->SetCaption("REC");
			m_record_info1->Show();
		}
	}
	else
		HideCamBRecordIcon();

	return 0;
}

int PreviewWindow::HideCamBRecordIcon()
{
	if(GetControl("record_icon") != NULL)
		GetControl("record_icon")->Hide();

	if(GetControl("record_icon1") != NULL)
		GetControl("record_icon1")->Hide();

	if( m_record_info != NULL)
		m_record_info->Hide();

	if( m_record_info1 != NULL)
		m_record_info1->Hide();

	return 0;
}
#endif

int PreviewWindow::HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam)
{
//      db_error("[debug_jaosn]:the message=%d ; wparam=%u ; lparam = %lu",message,wparam,lparam);
    switch (message) {
        case MSG_CREATE:
            //SetTimer(hwnd, KEEPALIVE_TIMER_ID, 100);
            break;
        case MSG_PAINT:
        {
#if 0
            HDC hdc = ::BeginPaint(hwnd);
            HDC mem_dc = CreateMemDC (1280, 200, 16, MEMDC_FLAG_HWSURFACE | MEMDC_FLAG_SRCALPHA,
                                  0x0000F000, 0x00000F00, 0x000000F0, 0x0000000F);


            SetBrushColor (mem_dc, RGBA2Pixel (mem_dc, 0x00, 0x00, 0x00, 0xA0));
            FillBox (mem_dc, 0, 0, 1280, 200);

            SetBkMode (mem_dc, BM_TRANSPARENT);
            BitBlt (mem_dc, 0, 0, 1280, 200, hdc, 0, 600, 0);

            ::EndPaint(hwnd, hdc);
#endif

#ifdef ENABLE_ADAS
            HDC hdc = 0;
            MenuConfigLua *mcl=MenuConfigLua::GetInstance();
            int preview_camera = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CAMERA);
            if(adas_enable_ && !preview_camera){
                if(!adas_uidraw_->GetFullWarnShowFlag()){
                    //hdc = BeginPaint(hwnd);
                        if(adas_uidraw_ != NULL){
                            pthread_mutex_lock(&adas_lock_);
                            if(MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_ADAS_CALIBRATION)
                                    && alignline_showtime_ < 30){
                                adas_uidraw_->DrawALignLine(hdc);
                            }
                            int preview_crop = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CROP0);
                            CropDirection direction = CropDirection_Center;
                            int y = (preview_crop)*(PREVIEW_MOVE_DISTANCE);
                            if(preview_crop == 0){
                                y = (int)(y * 0.6);
                                direction = CropDirection_UP;
                            }else if(preview_crop == 1){
                                y = (int)(y * 0.5);
                                direction = CropDirection_UP;
                            }else if(preview_crop == 2){
                                direction = CropDirection_Center;
                                y = (int)(y * 0.4);
                            }else if(preview_crop == 3){
                                direction = CropDirection_Down;
                                y = (int)(y * 0.5);
                            }else if(preview_crop == 4){
                                direction = CropDirection_Down;
                                y = (int)(y * 0.6);
                            }
                            adas_uidraw_->DrawRoadLine(hdc,y,direction);
                            adas_uidraw_->DrawCarsInfo(hdc,y,direction);
                            pthread_mutex_unlock(&adas_lock_);
                        }
                    //EndPaint(hwnd, hdc);
                }else {
                    if(!show_fullwarn_flag_){
                        //hdc = BeginPaint(hwnd);
                        adas_uidraw_->DrawLaneWarning(hdc);
                        //EndPaint(hwnd, hdc);
                        show_fullwarn_flag_ = true;
                    }
                    return HELP_ME_OUT;
                }
            }else{
                return HELP_ME_OUT;
            }
#endif
        }
        break;
        case MSG_TIMER:
#if 0
            if(wparam == KEEPALIVE_TIMER_ID)
            {
                WatchDog::GetInstance()->SetFeedTime();
                return HELP_ME_OUT;
            }
#endif
		break;
        case MSG_MOUSE_FLING:
        {  
            int direction = LOSWORD (wparam);
            if (direction == MOUSE_LEFT || direction == MOUSE_RIGHT)
            {
                if(!show_reverseline_flag_)
                    listener_->sendmsg(this,PREVIEW_SWITCH_LAYER, 0);
                if(show_reverseline_flag_)
                {
                    if (direction == MOUSE_LEFT || direction == MOUSE_RIGHT){
                        //db_error("direction %d line_id %d",direction,line_id_);
                        if(line_id_ == REVERSELINES)
                            ShowReverseLines(REVERSELINES_WIDTH, true);
                        else if(line_id_ == REVERSELINES_LOW)
                            ShowReverseLines(REVERSELINES_LOW_WIDTH, true);
                        else if(line_id_ == REVERSELINES_WIDTH)
                            ShowReverseLines(REVERSELINES, true);
                        else if(line_id_ == REVERSELINES_LOW_WIDTH)
                            ShowReverseLines(REVERSELINES_LOW, true);
                        MenuConfigLua::GetInstance()->SetMenuIndexConfig(MSG_SET_REVERSELINEID,line_id_);
                    }
                }
            }else if(direction == MOUSE_UP){
//                listener_->sendmsg(this,PREVIEW_VIEW_UP, 0);
                if(show_reverseline_flag_){
                    //db_error("direction %d line_id %d",direction,line_id_);
                    ShowReverseLines(REVERSELINES, true);
                    MenuConfigLua::GetInstance()->SetMenuIndexConfig(MSG_SET_REVERSELINEID,line_id_);
                }
            }else if(direction == MOUSE_DOWN){
//                listener_->sendmsg(this,PREVIEW_VIEW_DOWN, 0);
                if(show_reverseline_flag_){
                    //db_error("direction %d line_id %d",direction,line_id_);
                    ShowReverseLines(REVERSELINES_LOW, true);
                    MenuConfigLua::GetInstance()->SetMenuIndexConfig(MSG_SET_REVERSELINEID,line_id_);
                }
            }
        }
        break;
        case MSG_LBUTTONDOWN:
        {
            if(prompt_->GetCPUTempHighPromptShowFlag()){
                db_error("Hide CPU Temp High Prompt");
                HidePromptInfo();
            }
        }
        break;
        case MSG_MOUSEMOVE:
        {
            static int ignore = 0;
            if(show_reverseline_flag_)
                break;
            int direction = LOSWORD(wparam) & ~(0x400);
            if (!ignore--) {
                ignore = 3;
            } else {
                break;
            }
            if(direction == MOUSE_DOWN)
                listener_->sendmsg(this,PREVIEW_VIEW_DOWN, 0);
            else if(direction == MOUSE_UP)
                listener_->sendmsg(this,PREVIEW_VIEW_UP, 0);
        }
        break;

        default:
          return ContainerWidget::HandleMessage( hwnd, message, wparam, lparam );
    }
	return HELP_ME_OUT;
}


void PreviewWindow::GetCreateParams(CommonCreateParams& params)
{
    params.style = WS_NONE;
    params.exstyle = WS_EX_NONE;
    params.class_name = " ";
    params.alias      = GetClassName();
}

PreviewWindow::PreviewWindow(IComponent *parent)
        : SystemWindow(parent)
        , win_statu(STATU_PREVIEW)
        , m_nCurrentWin(WINDOWID_PREVIEW)
        , m_download_status(STATUS_DOWNLOAD_INIT)
        , isRecordStart(false)
        , isWifiStarting(false)
        , isTakepicFinish(true)
        , m_bUsbDialogShow(false)
        , m_standby_flag(false)
        , m_stop2down(false)
        , is_start_download(false)
#ifdef ENABLE_ADAS
        , full_showtime_(0)
        , alignline_showtime_(0)
        , show_fullwarn_flag_(false)
        , adas_enable_(false)
        , adas_switch_(false)
        , adas_uidraw_(NULL)
#endif
        , accon_recode_(false)
        , show_reverseline_flag_(false)
        , line_id_(NO_REVERSELINES)
        , prompt_(NULL)
        , prompt_box_(NULL)
        , bullet_collection_(NULL)
        , win_mg(::WindowManager::GetInstance())
        , m_record_info(NULL)
        , m_record_info1(NULL)
        , usb_win_(NULL)
        , rec_hint_icon_(NULL)
{
    db_msg(" ");
    Load();

    //SetWindowBackImage("/usr/share/minigui/res/images/bg.png");
    SetBackColor(0x00000000);

    // alpha is 0, that means set background transparent
    DWORD ctrl_bg_color = 0x00000000;

//    shutdown_window_ = new ShutDownWindow(this);
//    shutdown_window_->Hide();
    prompt_box_ = new PromptBox(this);
    prompt_ = new Prompt(this);

#ifdef SHOW_DEBUG_INFO
    TextView *debug_info_label = static_cast<TextView*>(GetControl("debug_info"));
    debug_info_label->SetCaptionColor(PIXEL_red);
    debug_info_label->SetTextStyle(DT_LEFT| DT_WORDBREAK | DT_EDITCONTROL);
#endif

    bullet_collection_ = new BulletCollection();
    bullet_collection_->initButtonDialog(this);

#ifdef ENABLE_ADAS
    adas_uidraw_ = new ADASEventUIProc();
    pthread_mutex_init(&adas_lock_,NULL);
    memset(&adas_event_,0,sizeof(AW_AI_ADAS_DETECT_R__v2));
    create_timer(this, &adas_uiproc_timer_,ADASUIProcUpdate);
#endif

    pthread_mutex_init(&proc_lock_, NULL);

#ifdef USB_MODE_WINDOW
    usb_win_ = new USBModeWindow(this);
#endif
    rec_hint_icon_ = reinterpret_cast<GraphicView *>(GetControl("rec_hint_icon"));
    GraphicView::LoadImage(rec_hint_icon_, "rec_hint");
    rec_hint_icon_->Hide();
    create_timer(this, &rechint_timer_id_, RecHintTimerProc);
    stop_timer(rechint_timer_id_);

    ShowReverseLines(NO_REVERSELINES,false);\

}

PreviewWindow::~PreviewWindow()
{
    db_msg("destruct");

    if(rec_hint_icon_)
       GraphicView::UnloadImage(rec_hint_icon_);

#ifdef USB_MODE_WINDOW
    if(usb_win_){
        delete usb_win_;
        usb_win_ = NULL;
    }
#endif

    if(bullet_collection_){
        delete bullet_collection_;
        bullet_collection_ = NULL;
    }

    if(prompt_){
        delete prompt_;
        prompt_ = NULL;
    }

    if(prompt_box_){
        delete prompt_box_;
        prompt_box_ = NULL;
    }
    #ifdef ENABLE_ADAS
    pthread_mutex_destroy(&adas_lock_);
    #endif
    pthread_mutex_destroy(&proc_lock_);
}

void PreviewWindow::RecHintTimerProc(union sigval sigval)
{
    prctl(PR_SET_NAME, "UpdateRecHint", 0, 0, 0);

    static bool flag = false;
    PreviewWindow *self = reinterpret_cast<PreviewWindow*>(sigval.sival_ptr);

    if (flag)
        self->rec_hint_icon_->Hide();
    else
        self->rec_hint_icon_->Show();

    flag = !flag;
}

int GetSdcardVersion(std::vector<std::string> &p_FileNameList,std::string path_str = "/mnt/extsd/version/",std::string filter_str = ".img")
{
    StorageManager *sm = StorageManager::GetInstance();
    int status = StorageManager::GetInstance()->GetStorageStatus();
    if( (status == UMOUNT) || (status == STORAGE_FS_ERROR) || (status == FORMATTING))
        return -1;

    char *filepath =(char *)path_str.c_str();
    DIR *sdcard_dir = NULL;
    struct dirent *dirp;
      int ret = -1;
      db_msg("debug_zhb---> scan sdcard path : %s",filepath);
    if((sdcard_dir = opendir(filepath)) == NULL)
    {
       db_error("opendir fail");
       return -1;
    }
    while((dirp = readdir(sdcard_dir)) != NULL)
    {
        if(strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
            continue;
        int size = strlen(dirp->d_name);
        //if(strcmp((dirp->d_name+( size - 4)),".img")!=0)
        if(strcmp((dirp->d_name+( size - strlen(filter_str.c_str()))),filter_str.c_str())!=0)
            continue;
        ret = 0;
       p_FileNameList.push_back(dirp->d_name);
    }
        closedir(sdcard_dir);
    return ret;
}

int GetSdcardBin(std::vector<std::string> &p_FileNameList,std::string path_str,std::string filter_str)
{
	StorageManager *sm = StorageManager::GetInstance();
    int status = StorageManager::GetInstance()->GetStorageStatus();
    if( (status == UMOUNT) || (status == STORAGE_FS_ERROR) || (status == FORMATTING))
		return -1;

    char *filepath =(char *)path_str.c_str();
    DIR *sdcard_dir = NULL;
    struct dirent *dirp;
    int ret = -1;
    db_msg("debug_zhb---> scan sdcard path : %s",filepath);
	if((sdcard_dir = opendir(filepath)) == NULL)
	{
	   db_error("opendir fail");
	   return -1;
	}
	while((dirp = readdir(sdcard_dir)) != NULL)
	{
		if(strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
			continue;
		int size = strlen(dirp->d_name);
		//if(strcmp((dirp->d_name+( size - 4)),".img")!=0)
		if(strcmp((dirp->d_name+( size - strlen(filter_str.c_str()))),filter_str.c_str())!=0)
			continue;
		ret = 0;
	   p_FileNameList.push_back(dirp->d_name);
	}
	    closedir(sdcard_dir);
	return ret;
}

bool PreviewWindow::Md5CheckVersionPacket(string p_path,string md5Code)
{
    FILE *ptr = NULL;
    char buf_ps[128]={0};
    char md5str[128]= {0};
    char temp[128]={0};;
    snprintf(temp,sizeof(temp),"md5sum /mnt/extsd/version/%s",p_path.c_str());
    db_warn("md5check command : %s",temp);
    if((ptr = popen(temp,"r"))!= NULL)
    {
        while(fgets(buf_ps, 128, ptr) !=NULL)
        {
            if(strstr(buf_ps,p_path.c_str())== NULL)
                continue;
            char *saveptr = strstr(buf_ps," ");
            memset(md5str, 0, sizeof(md5str));
            strncpy(md5str, buf_ps, saveptr-buf_ps);
            db_msg("debug_zhb--->md5str = %s ",md5str);
            break;
        }
        pclose(ptr);
    }
    db_msg("debug_zhb--->md5Code = %s ",md5Code.c_str());
    if(strncmp(md5Code.c_str(),md5str,strlen(md5Code.c_str())) == 0)
        return true;

    return false;
}

bool PreviewWindow::IsNewVersion(std::string external_version,std::string local_version)
{
	//升级固件路径为/mnt/extsd/version,固件名称为V533-CDR-32位md5值.img
	//eg:V533-CDR-dc1afd85b49f47377c7e6b62ad0045c3.img
	if(external_version.empty() || local_version.empty())
    {
        db_error("%s  external_version or local_version is empty",__func__);
        return false;
    }
    db_warn("IsNewVersion    external_version : %s ",external_version.c_str());
    db_warn("IsNewVersion     local_version : %s",local_version.c_str());
    //pars external_version
    string::size_type e_rc_start = external_version.rfind("V533-CDR");
    if( e_rc_start == string::npos)
    {
        db_warn("invalid fileName:%s",external_version.c_str());
        return false;
    }

    string::size_type local_rc_start = local_version.rfind("V533-CDR");
    if( local_rc_start == string::npos)
    {
        db_warn("invalid fileName:%s",local_version.c_str());
        return false;
    }

    return true;
}

int PreviewWindow::DetectSdcardNewVersion()
{
    int ret = -1;
    db_warn("debug_zhb-------ready------detecte the new version");
    std::vector<std::string> p_FileNameList;
    p_FileNameList.clear();
    p_FileNameList.shrink_to_fit();
    if(GetSdcardVersion(p_FileNameList) < 0){
        db_warn("get sdcard version info fail");
        return -1;
    }
    if(p_FileNameList.size() > 1)
    {
		db_warn("detect more than 1 img in the /mnt/extsd/version/");
		m_stop2down = true;
		bullet_collection_->setButtonDialogCurrentId(BC_BUTTON_DIALOG_MORE_IMG);
		bullet_collection_->ShowButtonDialog();
		return 0;
	}

    NewSettingWindow *s_win = reinterpret_cast<NewSettingWindow*>(win_mg->GetWindow(WINDOWID_SETTING_NEW));
    string vstr = s_win->GetVersionStr();

    if(IsNewVersion(p_FileNameList[0],vstr) == true)
    {
        string md5_str;
        string::size_type rc_cn_debug = p_FileNameList[0].rfind("V533-CDR-");
        md5_str = p_FileNameList[0].substr(rc_cn_debug+strlen("V533-CDR-"));
        if(md5_str.empty()){
            db_error("md5 str is empty!!");
            return -1;
        }

        char temp[128]={0};
        if(strncpy(temp,md5_str.c_str(),32) == NULL)
        {
            db_warn("strncpy md5 str fail");
            return -1;
        }
        db_warn("get the md5 str form version name : %s",temp);
        md5_str.clear();
        md5_str = temp;

        //check md5
        if(Md5CheckVersionPacket(p_FileNameList[0],md5_str) == false)
        {
            db_warn("check the version  md5  fail");
            m_stop2down = true;
            bullet_collection_->setButtonDialogCurrentId(BC_BUTTON_DIALOG_CHECK_MD5_FAILE);
            bullet_collection_->ShowButtonDialog();
            return 0;
        }
        ret =0;
        db_msg("debug_zhb-------------detecte the new version");
        m_stop2down = true;
        bullet_collection_->setButtonDialogCurrentId(BC_BUTTON_DIALOG_SDCARD_UPDATE_VERSION);
        bullet_collection_->ShowButtonDialog();
    }

	return ret;
}
bool PreviewWindow::IsHighCalssCard()
{
	StorageManager *sm = StorageManager::GetInstance();
	if(sm->IsHighClassCard() == LOW_SPEED)
	{
		db_warn("is low speed card set m_stop2down true");
		m_stop2down = true;
		ShowPromptInfo(PROMPT_TF_LOW_SPEED, 0);
		return false;
	}
	return true;
}

bool PreviewWindow::IsFileSystemError()
{
    StorageManager *sm = StorageManager::GetInstance();
    if (sm->GetStorageStatus() == MOUNTED)
    {
        db_msg("debug_zhb----> filesystem check");
        if(sm->CheckFileSystemError()!= true)
        {
            db_error("checkFileSystemError---");
            return true;
        }
    }
    db_msg("debug_zhb----> filesystem is ok");
    return false;
}

int PreviewWindow::GetCurrentRecordTime()
{
    WindowManager *win_mg_ = WindowManager::GetInstance();
    StatusBarWindow *sbw  = static_cast<StatusBarWindow*>(win_mg_->GetWindow(WINDOWID_STATUSBAR));
    return sbw->GetCurrentRecordTime();
}

void PreviewWindow::RecordStatusTimeUi(bool mstart)
{
    if(mstart){
        isRecordStart = true;
    }else{
        isRecordStart = false;
    }

    WindowManager *win_mg_ = WindowManager::GetInstance();
    StatusBarWindow *sbw  = static_cast<StatusBarWindow*>(win_mg_->GetWindow(WINDOWID_STATUSBAR));
    sbw->RecordStatusTimeUi(mstart);
    sbw->RecHintIcon(mstart);
    RecHintIcon(mstart);
    sbw->SetRecordStatusFlag(mstart);
}

void PreviewWindow::RecHintIcon(bool flag)
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

void PreviewWindow::ResetRecordTime()
{
    WindowManager *win_mg_ = WindowManager::GetInstance();
    StatusBarWindow *sbw  = static_cast<StatusBarWindow*>(win_mg_->GetWindow(WINDOWID_STATUSBAR));
    sbw->ResetRecordTime();
}

string PreviewWindow::GetResourceName()
{
    return string(GetClassName());
}

void PreviewWindow::Update(MSG_TYPE msg, int p_CamID, int p_recordId)
{
    switch ((int)msg)
    {
        case MSG_CLOSE_STANDBY_DIALOG:
        {
            db_warn("by hero *** ------PreviewWindow------------MSG_CLOSE_STANDBY_DIALOG ---");
            EyeseeLinux::Screensaver::GetInstance()->ForceScreenOnBySdcard();
            db_warn("==========PromptShowFlag %d,getPromptId %d,m_prompt_standby_flag_ %d==========",
                    prompt_->GetPromptShowFlag(),prompt_->getPromptId(),prompt_->GetStandbyFlagStatus());
            if(prompt_->GetPromptShowFlag() && prompt_->getPromptId() == PROMPT_STANDBY_MODE
                    && prompt_->GetStandbyFlagStatus() == true)
            {
                prompt_->HidePromptInfo();
                if(EyeseeLinux::EventManager::GetInstance()->mStandbybreak_flag == false)
                {
                    db_warn("========set mStandbybreak_flag true=========");
                    EyeseeLinux::EventManager::GetInstance()->mStandbybreak_flag = true;
                    EyeseeLinux::EventManager::GetInstance()->standby_try_count = 0;
                }
                if(EyeseeLinux::EventManager::GetInstance()->mNotify_acc_off_flag) {
                    db_warn("========reset mNotify_acc_off_flag=========");
                    EyeseeLinux::EventManager::GetInstance()->mNotify_acc_off_flag = false;
                }
                if(prompt_->GetStandbyFlagStatus() == true) {
                    db_warn("prompt stanby flag is true,reset prompt stanby flag");
                    prompt_->SetStandbyFlagStatus(false);
                }
            }
            m_stop2down = false;
        }
        break;
        case MSG_ACCON_HAPPEN:
        {
            db_warn("by hero *** ------PreviewWindow------------MSG_ACCON_HAPPEN ---");
            if( WINDOWID_PREVIEW == win_mg->GetCurrentWinID() && !m_bUsbDialogShow)
            {
                db_debug("hide status bar");
                HideStatusBar();
            }
            EyeseeLinux::Screensaver::GetInstance()->ForceScreenOnBySdcard();
            if(m_standby_flag){
                m_standby_flag = false;
                if(prompt_->GetPromptShowFlag() && prompt_->getPromptId() == PROMPT_STANDBY_MODE)
                {
                    prompt_->HidePromptInfo();
					PowerManager::GetInstance()->SetBrightnessLevel(1);
                    if(EyeseeLinux::EventManager::GetInstance()->mNotify_acc_off_flag) {
                        db_warn("========reset mNotify_acc_off_flag=========");
                        EyeseeLinux::EventManager::GetInstance()->mNotify_acc_off_flag = false;
                    }
                    if(prompt_->GetStandbyFlagStatus() == true) {
                        db_warn("prompt stanby flag is true,reset prompt stanby flag");
                        prompt_->SetStandbyFlagStatus(false);
                    }
                }
                m_stop2down = false;
            }
            int window_id = WindowManager::GetInstance()->GetCurrentWinID();
            if(WINDOWID_PREVIEW == window_id){
                if(!GetRecordStatus() && (HandlerPromptInfo() != -1)){
                    if(!PowerManager::GetInstance()->getUsbconnectStatus()) {
                        usleep(300*1000);
                        if(!PowerManager::GetInstance()->getUsbconnectStatus()){
                            listener_->sendmsg(this, PREVIEW_RECORD_BUTTON, 1);
                        }
                    }
                }
            }else{
                db_error("acc on happen,but current window %d is not preview,do not recorder",window_id);
            }
            if( WINDOWID_PREVIEW == win_mg->GetCurrentWinID() && !m_bUsbDialogShow)
            {
                db_debug("show status bar");
                ShowStatusBar();
            }
        }
        break;
        case MSG_ACCOFF_HAPPEN:
        {
            db_warn("by hero *** ------PreviewWindow------------MSG_ACCOFF_HAPPEN");
//            if(!m_standby_flag)
//                EyeseeLinux::Screensaver::GetInstance()->ForceScreenOnBySdcard();
            if( WINDOWID_PREVIEW == win_mg->GetCurrentWinID() && !m_bUsbDialogShow)
            {
                db_debug("hide status bar");
                HideStatusBar();
            }
            EyeseeLinux::Screensaver::GetInstance()->ForceScreenOnBySdcard();
            m_standby_flag = true;
            //should colse all style dialog window and the show the  PROMPT_STANDBY_MODE
            if(WINDOWID_PLAYBACK == win_mg->GetCurrentWinID())
            {
                PlaybackWindow* playback_win_ = reinterpret_cast<PlaybackWindow *>(win_mg->GetWindow(WINDOWID_PLAYBACK));
                playback_win_->PauseVideoFilePlay();
            }
            if(bullet_collection_->getButtonDialogShowFlag())//close the preview window button dialog
            {
                bullet_collection_->BCDoHide();
                StorageManager::GetInstance()->setMReadOnlyDiskFormatFinish(true);//reset the ready only thread flag
            }
            if(prompt_box_ != NULL){
                if(prompt_box_->GetPromptBoxShowFlag())//close the all window promptbox
                    prompt_box_->HidePromptBox();
            }else{
                db_error("PromptBox_ is null!!!");
            }
            NewSettingWindow*s_win = reinterpret_cast<NewSettingWindow*>(win_mg->GetWindow(WINDOWID_SETTING_NEW));
            s_win->ForceCloseSettingWindowAllDialog();
            m_stop2down = FALSE;
            if(EyeseeLinux::EventManager::GetInstance()->mNotify_acc_on_flag) {
                db_warn("========reset mNotify_acc_on_flag=========");
                EyeseeLinux::EventManager::GetInstance()->mNotify_acc_on_flag = false;
            }
            ShowPromptInfo(PROMPT_STANDBY_MODE, 5,true);//force close the all window prompt
        }
        break;
        case MSG_RECORD_START:
        {
            db_warn("[habo]--->start to record !!!!");
            RecordStatusTimeUi(true);
        }
        break;
        case MSG_RECORD_STOP:
        {
            if(isRecordStart && !m_standby_flag)
            {
                ShowPromptBox(PROMPT_BOX_RECORDING_STOP,2);
            }
            RecordStatusTimeUi(false);
        }break;
        case MSG_RECORD_FILE_DONE:
            ResetRecordTime();
        break;
        case MSG_CAMERA_TAKEPICTURE_ERROR:
            isTakepicFinish = true;
            break;
        case MSG_CAMERA_TAKEPICTURE_FINISHED:
        {
            db_warn("[habo]:MSG_CAMERA_TAKEPICTURE_FINISHED \n");
            if(!isTakepicFinish)
            {
                isTakepicFinish = true;
            }
        }
        break;
        case MSG_STORAGE_UMOUNT:
        {
            db_error("receive MSG_STORAGE_UMOUNT");
            m_stop2down = false;
          //  m_dbisReady = false;
            if(!m_standby_flag)
                EyeseeLinux::Screensaver::GetInstance()->ForceScreenOnBySdcard();

            if( win_mg ->GetCurrentWinID() == WINDOWID_PREVIEW && (bullet_collection_->getButtonDialogShowFlag())){
                db_msg("debug_zhb--->button dialog show ready to hide");
                bullet_collection_->BCDoHide();
                break;
            }

            if(prompt_->GetPromptShowFlag() && (prompt_->getPromptId() ==PROMPT_TF_LOW_SPEED))
            {
                prompt_->HidePromptInfo();
            }

            if(StorageManager::GetInstance()->getFormatFlag() == false && win_mg ->GetCurrentWinID() == WINDOWID_PREVIEW)
            {
                if(isRecordStart){
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 0);
                //sleep(2);
                ShowPromptBox(PROMPT_BOX_TF_OUT,2);
                }
            }
#ifdef USB_MODE_WINDOW
            if(usb_win_->GetVisible()){
                db_debug("resume window when umount at mass storage mode");
                m_bUsbDialogShow = false;
                usb_win_->Hide();
                usb_win_->SetUSBWinMessageReceiveFlag(false);
				win_mg->SetUSBWindowShowFlag(false);
                if( WINDOWID_PREVIEW == win_mg->GetCurrentWinID())
                {
                    db_debug("show status bar");
                    ShowStatusBar();
                }
                ResumeWindow(::WindowManager::GetInstance()->GetCurrentWinID(),WIN_USBMode);
            }
#endif
        }
        break;
        case MSG_DATABASE_UPDATE_FINISHED:
        {
           // m_dbisReady = true;
			usleep(500*1000);
            if(StorageManager::GetInstance()->getFormatFlag() == false && win_mg ->GetCurrentWinID() == WINDOWID_PREVIEW && m_bUsbDialogShow == false)
            {
                ShowPromptBox(PROMPT_BOX_TF_INSERT,2);
               // sleep(2);
            }
            #ifdef OTA_ENABLE
            if(win_mg ->GetCurrentWinID() == WINDOWID_PREVIEW && StorageManager::GetInstance()->getFormatFlag() == false)//only insert sdcard by manual need to detect the sdcard if has the new version.
            {
                if(DetectSdcardNewVersion() >= 0)
                    break;
            }
            #endif
            #if 0
            int status = StorageManager::GetInstance()->GetStorageStatus();
            if(!isRecordStart && ( (status != UMOUNT) && (status != STORAGE_FS_ERROR) ))
            {
                if(HandlerPromptInfo() != -1)
                {
                    if( win_mg ->GetCurrentWinID() == WINDOWID_PREVIEW)
                    {
                        listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 1);
                        ShowPromptBox(PROMPT_BOX_RECORDING_START,2);
                    }
                }
            }
            #endif
            StorageManager::GetInstance()->setFormatFlag(false);//reset the flag
        }
        break;
        case MSG_STORAGE_IS_FULL:
        {
            db_error("sd is full,will stop rec %d",isRecordStart);
            if(isRecordStart){
                ShowPromptBox(PROMPT_BOX_TF_FULL,2);
                sleep(2);
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 0);
            }
        }
        break;
        case MSG_STORAGE_FS_ERROR:
        {
            if(win_mg ->GetCurrentWinID() != WINDOWID_PREVIEW)
                break;

            if(GetRecordStatus())
            {
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON, 0);
                sleep(3);
            }

            if(prompt_box_->GetPromptBoxShowFlag())//close the all window promptbox
                prompt_box_->HidePromptBox();
            m_stop2down = true;
            bullet_collection_->setButtonDialogCurrentId(BC_BUTTON_DIALOG_DD_TF_FS_ERROR);
            bullet_collection_->ShowButtonDialog();
        }
        break;
#ifdef USB_MODE_WINDOW
        case MSG_USB_HOST_CONNECTED:
        {
        db_warn("MSG_USB_HOST_CONNECTED");
            m_bUsbDialogShow = true;
            if(WINDOWID_PLAYBACK == win_mg->GetCurrentWinID())
            {
                //stop playing
                PlaybackWindow *playback_win = reinterpret_cast<PlaybackWindow*>(win_mg->GetWindow(WINDOWID_PLAYBACK));
                playback_win->PauseVideoFilePlay();
                playback_win->SetPlaybackWinMessageReceiveFlag(true);
            }else if(WINDOWID_SETTING_NEW == win_mg->GetCurrentWinID()){
                NewSettingWindow *setting_win = reinterpret_cast<NewSettingWindow*>(win_mg->GetWindow(WINDOWID_SETTING_NEW));
                setting_win->SetNewSettingWinMessageReceiveFlag(true);
            }
            win_mg->GetWindow(win_mg->GetCurrentWinID())->Hide();
            usb_win_->DoShow();
			win_mg->SetUSBWindowShowFlag(true);
        }
        break;
        case MSG_USB_HOST_DETACHED:
        {
            db_warn("habo---> MSG_USB_HOST_DETACHED");
            if( !m_bUsbDialogShow)
                break;
            m_bUsbDialogShow = false;
            usb_win_->Hide();
            usb_win_->SetUSBWinMessageReceiveFlag(false);
			win_mg->SetUSBWindowShowFlag(false);
            if(WINDOWID_PLAYBACK == win_mg->GetCurrentWinID())
            {
                PlaybackWindow *playback_win = reinterpret_cast<PlaybackWindow*>(win_mg->GetWindow(WINDOWID_PLAYBACK));
                playback_win->SetPlaybackWinMessageReceiveFlag(false);

            } else if(WINDOWID_SETTING_NEW == win_mg->GetCurrentWinID()){
                NewSettingWindow *setting_win = reinterpret_cast<NewSettingWindow*>(win_mg->GetWindow(WINDOWID_SETTING_NEW));
                setting_win->SetNewSettingWinMessageReceiveFlag(false);
            }
            EyeseeLinux::StatusBarSaver::GetInstance()->Pause(false);
            if( WINDOWID_PREVIEW == win_mg->GetCurrentWinID())
            {
                ShowStatusBar();
                db_msg("status bar bottom window show");
            }
            ResumeWindow(::WindowManager::GetInstance()->GetCurrentWinID(),WIN_USBMode);
        }
        break;
        case MSG_USB_CHARGING:
//       case MSG_USB_MASS_STORAGE_SD_REMOVE:
        {
            db_warn("habo---> MSG_USB_CHARGING");
            if( WINDOWID_PREVIEW == win_mg->GetCurrentWinID())
            {
                ShowStatusBar();
                db_msg("status bar bottom window show");
            }
            ResumeWindow(::WindowManager::GetInstance()->GetCurrentWinID(),WIN_USBMode);
            m_bUsbDialogShow = false;
            usb_win_->SetUSBWinMessageReceiveFlag(false);
			win_mg->SetUSBWindowShowFlag(false);
            if( WINDOWID_PLAYBACK == win_mg->GetCurrentWinID())
            {
               usleep(500*1000);
               db_msg("set playback ignore message flag false");
               PlaybackWindow *playback_win = reinterpret_cast<PlaybackWindow*>(win_mg->GetWindow(WINDOWID_PLAYBACK));
               playback_win->SetPlaybackWinMessageReceiveFlag(false);
            }else if( WINDOWID_SETTING_NEW == win_mg->GetCurrentWinID()){
               usleep(500*1000);
               db_msg("set playback ignore message flag false");
               NewSettingWindow *setting_win = reinterpret_cast<NewSettingWindow*>(win_mg->GetWindow(WINDOWID_SETTING_NEW));
               setting_win->SetNewSettingWinMessageReceiveFlag(false);
            }
            if(WINDOWID_SETTING_NEW == win_mg->GetCurrentWinID() || WINDOWID_PLAYBACK == win_mg->GetCurrentWinID()){
                HideStatusBar();
                db_msg("status bar bottom window hide");
            }
        }
        break;
       case MSG_USB_MASS_STORAGE:
       {
           db_warn("habo---> MSG_USB_MASS_STORAGE");
           HideStatusBar();
           db_msg("status bar bottom window hide");
           break;
       }
#endif
        case MSG_SOFTAP_DISABLED:
        {
            //db_warn("[debug_jaosn]: MSG_SOFTAP_DISABLED \n");
            isWifiStarting = false;
        }
        break;
#if 0
        case MSG_BATTERY_LOW:
        {
            ShowPromptInfo(PROMPT_BAT_LOW, 2);
            db_msg("isRecordStart %d, isTakepicFinish %d\n",isRecordStart,isTakepicFinish);
            if(isRecordStart)
            {
                db_msg("stop record\n");
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 0);//stop record
            }
            /*
               if(isTakepicFinish)
               {
               db_msg("stop take picture\n");
               listener_->sendmsg(this, PREVIEW_SHOTCUT_BUTTON , 1);//stop take picture
               }
               */
            db_warn("lowpower! ready to shutdown system");
            listener_->notify(this, PREVIEW_LOWPOWER_SHUTDOWN , 0); //close system
        }
        break;
#endif
        case MSG_CAMERA_ON_ERROR:
            ShowPromptInfo(PROMPT_CAMEAR_ERROR, 2);
            if(isRecordStart == true){
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 0);
                ShowPromptBox(PROMPT_BOX_RECORDING_STOP,2);
                usleep(500*1000);
            }
        break;
        case MSG_REINIT_CAMERA_FINISH:
            db_error("receive MSG_REINIT_CAMERA_FINISH");
            if(((win_statu == STATU_PREVIEW ) && !isRecordStart))
            {
               if(HandlerPromptInfo() == -1)
               {
                   db_msg("[fangjj]: TF ERROR: no tf or tf full \n");
                   break;
               }
            }
            if(isRecordStart == false)
            {
               db_msg("debug_zhb-----------------ready to recording ");
               listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 1);
               ShowPromptBox(PROMPT_BOX_RECORDING_START,2);
            }
        break;
        case MSG_WIFI_CLOSE:
        {
            //ResumeWindow(m_nCurrentWin, WIN_WIFI);
//            if( !m_bHdmiConnect )
//            {
//                Closescreen *cs = Closescreen::GetInstance();
//                cs->SetClosescreenEnable(false);
//                cs->Stop();
//            }
//            db_msg("m_nCurrentWin[%d]\n",m_nCurrentWin);
        }
        break;
        case MSG_SET_WIFI_ON:
        {
            db_warn("[debug_jason]: isWifiStarting = %d",isWifiStarting);
            if(isWifiStarting)
                break;
            listener_->sendmsg(this, PREVIEW_WIFI_SWITCH_BUTTON , 1);
            isWifiStarting = true;
            break;
        }
        case MSG_SET_WIFI_OFF:
            db_warn("[debug_jason]: isWifiStarting = %d",isWifiStarting);
            if(!isWifiStarting)
                break;
            listener_->sendmsg(this, PREVIEW_WIFI_SWITCH_BUTTON , 0);
        break;
        case MSG_WIFI_DISABLED:
            db_warn("[debug_jason]:  11 isWifiStarting = %d",isWifiStarting);
            isWifiStarting = false;
        break;
        case MSG_DATABASE_IS_FULL:
            ShowPromptInfo(PROMPT_DATABASE_FULL, 2);
        break;
        case MSG_STORAGE_CAP_NO_SUPPORT:
            ShowPromptInfo(PROMPT_TF_CAP_NO_SUPPORT, 3);
        break;
#ifdef ENABLE_ADAS
        case MSG_ADAS_START:
            adas_enable_ = true;
            alignline_showtime_ = 0;
            set_period_timer(1,0,adas_uiproc_timer_);
        break;
#endif
#ifdef ENABLE_ADAS
        case MSG_ADAS_STOP:
            adas_enable_ = false;
            stop_timer(adas_uiproc_timer_);
        break;
#endif
#ifdef ENABLE_ADAS
        case MSG_ADAS_OPEN_CALIBRATION:
            alignline_showtime_ = 0;
        break;
        case MSG_ADAS_CLOSE_CALIBRATION:
            alignline_showtime_ = 0;
        break;
#endif
        case MSG_VOICE_CTRL_TURNON_SCREEN:
            db_debug("revice screen on msg");
            if(!PowerManager::GetInstance()->IsScreenOn()){
                EyeseeLinux::Screensaver::GetInstance()->ForceReTimerStatus();
            }
        break;
        case MSG_VOICE_CTRL_TURNOFF_SCREEN:
            db_debug("revice screen off msg");
            if(PowerManager::GetInstance()->IsScreenOn()){
                EyeseeLinux::Screensaver::GetInstance()->ForceReTimerStatus();
            }
        break;
        case MSG_VOICE_CTRL_TAKE_PIC:
        {
            db_debug("revice take pic msg");
            if(((win_statu == STATU_PREVIEW ) && isTakepicFinish))
            {
                if(HandlerPromptInfo() == -1)
                {
                    db_msg("[fangjj]: TF ERROR: no tf or tf full \n");
                    break;
                }
            }
            if(isTakepicFinish){
                isTakepicFinish = false;
                AudioCtrl::GetInstance()->PlaySound(AudioCtrl::AUTOPHOTO_SOUND);
                listener_->sendmsg(this,PREVIEW_TAKE_PIC_CONTROL, 0);
            }else{
                db_warn("[debug_zhb]:take pic is not done\n");
            }
        }
        break;

        case MSG_VOICE_CTRL_LOCK_FILE:
        {
            db_debug("revice lock file msg");
            int m_recodTime = 0;
            if(isRecordStart)
            {
                m_recodTime = this->GetCurrentRecordTime();
                listener_->sendmsg(this,PREVIEW_EMAGRE_RECORD_CONTROL, m_recodTime);
            }else{
                //should show please start record first
                ShowPromptBox(PROMPT_BOX_LOCK_RECORD_TIP_FILE,2);
            }
            if(!isTakepicFinish){
                db_error("take pic not finish");
                break;
            }
        }
        case MSG_VOICE_CTRL_TURNON_RECORDERAUDIO:
        {
            db_debug("revice turn on rec audio msg");
//            if(isRecordStart)
//            {
//                ShowPromptBox(PROMPT_BOX_DEVICE_RECORDING,2);
//                break;
//            }
            MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
            int val = menuconfiglua->GetMenuIndexConfig(SETTING_RECORD_VOLUME_SWITCH);
            if(!val){
               db_debug("disable auio record");
               listener_->sendmsg(this, PREVIEW_AUDIO_BUTTON , 1);
            }
        }
        break;
        case MSG_VOICE_CTRL_TURNOFF_RECORDERAUDIO:
        {
            db_debug("revice turn off rec audio msg");
//            if(isRecordStart)
//            {
//                ShowPromptBox(PROMPT_BOX_DEVICE_RECORDING,2);
//                break;
//            }
            MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
            int val = menuconfiglua->GetMenuIndexConfig(SETTING_RECORD_VOLUME_SWITCH);
            if(val){
                db_debug("enable auio record");
               listener_->sendmsg(this, PREVIEW_AUDIO_BUTTON , 0);
            }
        }
        break;
        case MSG_TF_CARD_ERROR:
        {
            db_msg("preview window recive MSG_TF_CARD_ERROR");
            if(isRecordStart)
            {
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 0);
            }
            //提示存储卡异常录像以停止，请格式化卡或者换卡
            ShowPromptInfo(PORMPT_TF_CARD_ERROR,5);
        }
        break;
        case MSG_TF_CARD_WORNING:
        {
            db_msg("preview window recive MSG_TF_CARD_WORNING");
            ShowPromptInfo(PORMPT_TF_CARD_WARNING,3);
        }
        break;
        case MSG_CPU_TEMP_HIGH:
        {
            if(isRecordStart)
            {
                db_error("receive cpu temp high msg,will stop rec");
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 0);
                ShowPromptInfo(PROMPT_CPU_TEMP_HIGH, 0);
            }
        }
        break;
        case SHOW_SHUTDOWN_LOGO:
        {
            if( WINDOWID_PREVIEW == win_mg->GetCurrentWinID())
            {
               HideStatusBar();
            }
            db_error("show shut down logo");
            GraphicView::LoadImage(GetControl("shut_down_logo"), "shut_down");
            GetControl("shut_down_logo")->Show();
        }
        break;
        case MSG_BACKCARVIDEO_ON:
        {
            db_error("receive MSG_BACKCARVIDEO_ON");
#ifdef ENABLE_ADAS
            if(adas_enable_){
                db_debug("adas enable stop timer");
                stop_timer(adas_uiproc_timer_); //显示倒车辅助线后需要将ADAS绘制暂时定时器关掉,否则辅助线会闪
            }
#endif
            if(!reverse_line_icon_->GetVisible()){
               int val = MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTINT_REVERSELINEID);
               ShowReverseLines((ReverseLines)val, true);
            }
            if( WINDOWID_PREVIEW == win_mg->GetCurrentWinID())
            {
                db_debug("hide status bar");
                HideStatusBar();
            }
            show_reverseline_flag_ = true;
        }
        break;
        case MSG_BACKCARVIDEO_OFF:
        case MSG_AHD_REMOVE:
        {
            if(show_reverseline_flag_){
#ifdef ENABLE_ADAS
                if(adas_enable_){
                  db_debug("MSG_BACKCARVIDEO_OFF start timer");
                  set_period_timer(1,0,adas_uiproc_timer_);
                }
#endif
                if(reverse_line_icon_->GetVisible())
                   reverse_line_icon_->Hide();
                if( WINDOWID_PREVIEW == win_mg->GetCurrentWinID())
                {
                    db_debug("show status bar");
                    ShowStatusBar();
                }
                show_reverseline_flag_ = false;
            }
        }
        break;
        default:
        break;
    }
}

void PreviewWindow::HideStatusBar()
{
    WindowManager *win_mg_ = ::WindowManager::GetInstance();
    StatusBarBottomWindow * status_bar_bottom = reinterpret_cast<StatusBarBottomWindow *>(win_mg->GetWindow(WINDOWID_STATUSBAR_BOTTOM));
    status_bar_bottom->DoHide();
    StatusBarWindow * status_bar_top = reinterpret_cast<StatusBarWindow *>(win_mg->GetWindow(WINDOWID_STATUSBAR));
    status_bar_top->DoHide();
    EyeseeLinux::StatusBarSaver::GetInstance()->Pause(true);
}

void PreviewWindow::ShowStatusBar()
{
    StatusBarBottomWindow * status_bar_bottom = reinterpret_cast<StatusBarBottomWindow *>(win_mg->GetWindow(WINDOWID_STATUSBAR_BOTTOM));
    status_bar_bottom->Show();
    StatusBarWindow * status_bar_top = reinterpret_cast<StatusBarWindow *>(win_mg->GetWindow(WINDOWID_STATUSBAR));
    status_bar_top->Show();
    EyeseeLinux::StatusBarSaver::GetInstance()->Pause(false);
}

#ifdef ENABLE_ADAS
void PreviewWindow::UpdateADASEventMsg(AW_AI_ADAS_DETECT_R__v2 *adas_event)
{
    pthread_mutex_lock(&adas_lock_);
    if(adas_event != NULL){
        memset(&adas_event_, 0, sizeof(AW_AI_ADAS_DETECT_R__v2));
        memcpy(&adas_event_,adas_event,sizeof(AW_AI_ADAS_DETECT_R__v2));
    }
    pthread_mutex_unlock(&adas_lock_);
}
#endif

void PreviewWindow::PreInitCtrl(View *ctrl, string &ctrl_name)
{
    ctrl->SetCtrlTransparentStyle(true);

#ifdef SHOW_DEBUG_INFO
    if (ctrl_name == string("debug_info")) {
        ::ExcludeWindowStyle(ctrl->GetHandle(), SS_CENTER);
        ::IncludeWindowStyle(ctrl->GetHandle(), SS_LEFT);
    }
#endif

   if (ctrl_name == "time_label") {// ctrl_name == "rec_time_label"
        ctrl->SetCtrlTransparentStyle(false);
        TextView* time_label = reinterpret_cast<TextView *>(ctrl);
        time_label->SetTextStyle(DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

}

void PreviewWindow::showLockFileUiInfo(int value)
{
    db_warn("showLockFileUiInfo value is %d",value);
    switch(value)
    {
        case 0:
            ShowPromptBox(PROMPT_BOX_UNLOCK_FILE,2);
            break;
        case 1:
            ShowPromptBox(PROMPT_BOX_LOCK_FILE,2);    
            break;
        case 2:
            ShowPromptBox(PROMPT_BOX_LOCK_RECORD_TIP_FILE,2);
            break;
        case 3:
            ShowPromptBox(PROMPT_BOX_FILE_LOCKED,2);
            break;
    }
}

void PreviewWindow::HandleButtonDialogMsg(int val)
{
    switch(bullet_collection_->getButtonDialogCurrentId()){
        case BC_BUTTON_DIALOG_DD_NOTICE:
            db_msg("[debug_zhb]------BC_BUTTON_DIALOG_DD_NOTICE");
            break;
        case BC_BUTTON_DIALOG_DD_NOTICE_FULL:
            db_msg("[debug_zhb]------BC_BUTTON_DIALOG_DD_NOTICE_FULL");
            break;
        case BC_BUTTON_DIALOG_DD_TF_FS_ERROR:
            db_msg("[debug_zhb]------BC_BUTTON_DIALOG_DD_TF_FS_ERROR");
            if(val == 1){
                usleep(300*1000);
                ShowPromptInfo(PROMPT_TF_FORMATTING,0);
                if(StorageManager::GetInstance()->Format() < 0)
                    ShowPromptInfo(PROMPT_TF_FORMAT_FAILED,2,true);
                else
                    ShowPromptInfo(PROMPT_TF_FORMAT_FINISH,2,true);
                }
            m_stop2down = false;
            StorageManager::GetInstance()->setMReadOnlyDiskFormatFinish(true);
            break;
        case BC_BUTTON_DIALOG_SDCARD_UPDATE_VERSION:
            db_msg("[debug_zhb]------BC_BUTTON_DIALOG_SDCARD_UPDATE_VERSION");
            if(val == 1){
                m_stop2down = false;
                listener_->sendmsg(this, PREVIEW_TO_SETTINGWINDOW_UPDATE_VERSION, 0);
                }else{
                        m_stop2down = false;
                        if(!GetRecordStatus() && (HandlerPromptInfo() != -1))
                            listener_->sendmsg(this, PREVIEW_RECORD_BUTTON, 1);
                    }

            break;
        case BC_BUTTON_DIALOG_MORE_IMG:
        case BC_BUTTON_DIALOG_CHECK_MD5_FAILE:
            m_stop2down = false;
            if(!GetRecordStatus() && (HandlerPromptInfo() != -1))
                listener_->sendmsg(this, PREVIEW_RECORD_BUTTON, 1);
            break;
        default:
            if(val == 1){
                db_msg("[debug_zhb]--->default p_camid = 1");
            }else{
                db_msg("[debug_zhb]--->default p_camid = 0");
            }
            break;
        }
    bullet_collection_->setButtonDialogShowFlag(false);
}

void PreviewWindow::ShowPromptInfo(unsigned int prompt_id,unsigned int showtimes,bool m_force)
{
     db_warn("[debug_zhb]-------PreviewWindow-----ShowPromptInfo  prompt_id:%d",prompt_id);
     if(prompt_->GetStandbyFlagStatus()) {
         db_warn("now is the standy mode no finsh,not show prompt");
         return;
     }
     if (prompt_->GetPromptShowFlag()){
         if(prompt_->getPromptId() == PROMPT_STANDBY_MODE){
             db_warn("now is the standy mode show ,shuold not to do anything");
             return ;
         }
         if(!m_force){
             db_warn("if has been show ,should close and then show other");
             return ;
         }else{
             if(WINDOWID_SETTING_NEW == win_mg->GetCurrentWinID()){
                 NewSettingWindow *setting_win = reinterpret_cast<NewSettingWindow*>(win_mg->GetWindow(WINDOWID_SETTING_NEW));
                 if(setting_win->GetNewSettingWinMessageReceiveFlag()){
                     db_error("set new setting win receive flag false");
                     setting_win->SetNewSettingWinMessageReceiveFlag(false);
                 }
             }
             db_warn("force kill the promptinfo dialog and show other dialog");
             prompt_->HidePromptInfo();
         }
     }
//     string bkgnd_bmp ;
//     if(prompt_id >= PROMPT_FULL_SDCARD_FORMAT && prompt_id <=PROMPT_FULL_WIFI_CONNET){
//         bkgnd_bmp = R::get()->GetImagePath("promtp_full_bg");
//         prompt_->SetPosition(0, 120, s_w, s_h-60*2);
//     }else{
//         bkgnd_bmp = R::get()->GetImagePath("bg_transparent");
//
//         prompt_->SetPosition(90, 124, 460, 232);
//     }
//     prompt_->SetWindowBackImage(bkgnd_bmp.c_str());
     prompt_->DoShow();
     prompt_->ShowPromptInfo(prompt_id,showtimes);
}

int PreviewWindow::HandlerPromptInfo(void)
{
    if(IsHighCalssCard() == false)
    {
        db_warn("Detected is low speed card , stop to recording");
        return -1;
    }

    if(m_stop2down){
        db_warn("when power on detect the tf is low speed / fs error/ new version to update stop to going down");
        return -1;
    }
    int ret = 0;
    int status = 0;
    StorageManager *sm = StorageManager::GetInstance();
    // update storage status
    status = sm->GetStorageStatus();
    if (status == MOUNTED) {
        ret = 0;
    } else if (status == STORAGE_DISK_FULL || status == STORAGE_LOOP_COVERAGE) {
        db_warn("[debug_jaosn]:PROMPT_TF_FULL 00");
        ShowPromptInfo(PROMPT_TF_FULL,2);
        ret = -1;
    }else if(status == STORAGE_FS_ERROR){
    	db_warn("[debug_jaosn]:STORAGE_FS_ERROR 11");
        ret = -1;
    }else {
        db_warn("[debug_jaosn]:PROMPT_TF_NULL 22");
        ShowPromptInfo(PROMPT_TF_NULL,3);
        ret = -1;
    }
    return ret;
}

void PreviewWindow::ShowPromptBox(unsigned int promptbox_id,unsigned int showtimes)
{

    db_msg("[debug_zhb]-------PreviewWindow-----ShowPromptBox  promptbox_id:%d",promptbox_id);
    if(prompt_->GetPromptShowFlag() && prompt_->getPromptId() == PROMPT_STANDBY_MODE){
        db_warn(" standby mode prompt has been show ,no to do anything");
        return ;
    }

     if(bullet_collection_->getButtonDialogShowFlag() && (bullet_collection_->getDialogCurrentId() == BC_BUTTON_DIALOG_FORMAT_SDCARD ||
        bullet_collection_->getDialogCurrentId() == BC_BUTTON_DIALOG_DD_TF_FS_ERROR||
        bullet_collection_->getDialogCurrentId() == BC_BUTTON_DIALOG_SDCARD_UPDATE_VERSION ||
        bullet_collection_->getDialogCurrentId() == BC_BUTTON_DIALOG_MORE_IMG ||
        bullet_collection_->getDialogCurrentId() == BC_BUTTON_DIALOG_CHECK_MD5_FAILE)){
            db_warn(" fs/version update/format mode prompt has been show ,no to do anything");
            return ;
     }
     if(promptbox_id >= PROMPT_BOX_RECORD_SOUND_OPEN && promptbox_id <= PROMPT_BOX_TF_OUT){
          if( win_mg ->GetCurrentWinID() == WINDOWID_SETTING_NEW || win_mg ->GetCurrentWinID() == WINDOWID_PLAYBACK)
            {
                db_warn("not need to show the promptbox when window id is not the previewwindow");
                return;
            }
        }
     if(prompt_box_->GetPromptBoxShowFlag()){
        db_warn("promptbox has been show ,no to do anything");
        return;
        }
     int p_len = 0;
     prompt_box_->ShowPromptBox(promptbox_id,showtimes);
     prompt_box_->DoShow();
}

int PreviewWindow::GetRecordStatus()
{
    return isRecordStart;
}

int PreviewWindow::GetWindowStatus()
{
    return win_statu;
}

int PreviewWindow::ResumeWindow(int p_nWinId, CurWinStatus p_Status)
{
    switch( p_nWinId )
    {
        case WINDOWID_PREVIEW:
            break;
    }
    win_mg->GetWindow( p_nWinId)->Show();

    return 0;
}

bool PreviewWindow::IsUsbAttach()
{
    return m_bUsbDialogShow;
}

#ifdef SHOW_DEBUG_INFO
void PreviewWindow::ShowDebugInfo(bool value)
{
    if (value) {
        GetControl("debug_info")->Show();
    } else {
        GetControl("debug_info")->Hide();
    }
}

void PreviewWindow::ClearDebugInfo()
{
    debug_info_.clear();
}

void PreviewWindow::InsertDebugInfo(const string &key, const string &value)
{
    debug_info_.emplace(key, value);

    UpdateDebugInfo();
}

void PreviewWindow::RemoveDebugInfo(const string &key)
{
    auto it = debug_info_.find(key);
    if (it != debug_info_.end()) {
        debug_info_.erase(key);
    }

    UpdateDebugInfo();
}

void PreviewWindow::UpdateDebugInfo()
{
    stringstream ss;

    for (auto str : debug_info_) {
        ss << str.second << "\n";
    }

    UpdateDebugInfo(ss.str());
}

void PreviewWindow::UpdateDebugInfo(const std::string &info)
{
    TextView *debug_info_label = static_cast<TextView*>(GetControl("debug_info"));
    debug_info_label->SetText(info);
}
#endif

void PreviewWindow::VideoStopRecordCtl()
{
    listener_->sendmsg(this, PREVIEW_RECORD_BUTTON , 0);

}

void PreviewWindow::OnLanguageChanged()
{

}

void PreviewWindow::HidePromptInfo()
{
    prompt_->HidePromptInfo();
}

bool PreviewWindow::GetIsRecordStartFlag()
{
    return isRecordStart;
}

#ifdef ENABLE_ADAS
void PreviewWindow::ADASUIProcUpdate(union sigval sigval)
{
    static bool play_sound_flag = false;
    PreviewWindow *preview_win = reinterpret_cast<PreviewWindow *>(sigval.sival_ptr);
    pthread_mutex_lock(&(preview_win->adas_lock_));
    preview_win->adas_uidraw_->UpdateADASEventMsg(&(preview_win->adas_event_));
    pthread_mutex_unlock(&(preview_win->adas_lock_));
    if(preview_win->adas_uidraw_->GetFullWarnShowFlag()){
        if(preview_win->full_showtime_ >= 5){
            db_error("show full warn image time arrived");
            preview_win->full_showtime_ = 0;
            preview_win->show_fullwarn_flag_ = false;
            play_sound_flag = false;
            if(WINDOWID_PLAYBACK == preview_win->win_mg->GetCurrentWinID())
                preview_win->ShowStatusBar();
            if(preview_win->adas_uidraw_->GetFullWarnShowFlag())
                preview_win->adas_uidraw_->SetFullWarnShowFlag(false);
        }else{
            db_error("show full warn image");
            if(WINDOWID_PLAYBACK == preview_win->win_mg->GetCurrentWinID())
                preview_win-> HideStatusBar();
        }
        preview_win->full_showtime_++;
    }
    if(!preview_win->show_fullwarn_flag_) {}
      //::InvalidateRect(preview_win->GetHandle(),NULL, TRUE);
    else {
        if(!play_sound_flag){
            preview_win->adas_uidraw_->ADASPlayWarning();
            play_sound_flag = true;
        }
    }
    if(MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_ADAS_CALIBRATION)){
        preview_win->alignline_showtime_++;
        if(preview_win->alignline_showtime_ >= 30){
//            db_error("alignline showtime arrived,hide alignline");
            preview_win->alignline_showtime_ = 30;
        }
    }
}
#endif

void PreviewWindow::SetAccon_Record_Flag(bool flag)
{
    accon_recode_ = flag;
}

void PreviewWindow::ShowReverseLines(ReverseLines line_id,bool show_flag)
{
    char filepath[128]={0};
    switch(line_id)
    {
        case REVERSELINES:
            snprintf(filepath,sizeof(filepath)-1,"reverse_lines");
        break;

        case REVERSELINES_WIDTH:
            snprintf(filepath,sizeof(filepath)-1,"reverse_lines_width");
        break;

        case REVERSELINES_LOW:
            snprintf(filepath,sizeof(filepath)-1,"reverse_lines_low");
        break;

        case REVERSELINES_LOW_WIDTH:
            snprintf(filepath,sizeof(filepath)-1,"reverse_lines_low_width");
        break;

        default:
            snprintf(filepath,sizeof(filepath)-1,"reverse_lines");
        break;
    }
    db_debug("filepath %s",filepath);
    reverse_line_icon_ = reinterpret_cast<GraphicView *>(GetControl("reverse_line_icon"));
    if(reverse_line_icon_ != NULL){
        GraphicView::UnloadImage(reverse_line_icon_);
        GraphicView::LoadImage(reverse_line_icon_, filepath);
        if(show_flag)
            reverse_line_icon_->Show();
        else
            reverse_line_icon_->Hide();
    }
    line_id_ = line_id;
}
