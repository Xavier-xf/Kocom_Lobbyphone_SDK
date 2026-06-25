/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file device_setting.cpp
 * @brief 设置界面presenter
 * @author id:826
 * @version v0.3
 * @date 2016-09-19
 */
//#define NDEBUG
#include "bll_presenter/device_setting.h"
#include "bll_presenter/screensaver.h"
#include "bll_presenter/autoshutdown.h"
#include "bll_presenter/audioCtrl.h"
#include "device_model/system/net/wifi_connector.h"
#include "device_model/system/net/softap_controller.h"
#include "device_model/system/net/net_manager.h"
#include "device_model/storage_manager.h"
#include "window/user_msg.h"
#include "common/setting_menu_id.h"
#include "uilayer_view/gui/minigui/window/status_bar_window.h"
#include "uilayer_view/gui/minigui/window/status_bar_bottom_window.h"
#include "uilayer_view/gui/minigui/window/preview_window.h"
#include "uilayer_view/gui/minigui/window/prompt.h"
#include "common/app_log.h"
#include "application.h"
#include "lua/lua_config_parser.h"
#include "device_model/media/camera/camera.h"
#include "device_model/media/recorder/recorder.h"
#include "device_model/media/camera/camera_factory.h"
#include "device_model/media/recorder/recorder.h"
#include "device_model/media/recorder/recorder_factory.h"
#include "minigui-cpp/resource/resource_manager.h"
#include "device_model/menu_config_lua.h"
#include "window/setting_handler_window.h"
#include "device_model/system/led.h"
#include "device_model/media/osd_manager.h"
#include "window/newSettingWindow.h"
#include "uilayer_view/gui/minigui/window/promptBox.h"
#include "device_model/system/power_manager.h"
#include "device_model/system/event_manager.h"
#include "window/bulletCollection.h"
#include "device_model/system/gsensor_manager.h"
#include "device_model/dialog_status_manager.h"
#include "device_model/system/watchdog.h"
#include "dd_serv/common_define.h"
#include "device_model/version_update_manager.h"
#include "bll_presenter/statusbarsaver.h"


#undef LOG_TAG
#define LOG_TAG "DeviceSetting"

using namespace EyeseeLinux;
using namespace std;

DeviceSettingPresenter::DeviceSettingPresenter(MainModule *mm)
    : status_(MODEL_UNINIT)
    , win_mg_(::WindowManager::GetInstance())
    , net_manager_(NetManager::GetInstance())
    , softap_on_(false)
    , isStreamRecodFlag(false)
{
    pthread_mutex_init(&model_lock_, NULL);
    StatusBarWindow *status_bar = static_cast<StatusBarWindow *>(win_mg_->GetWindow(WINDOWID_STATUSBAR));
    this->Attach(status_bar);
    StatusBarBottomWindow *status_bottom_bar = static_cast<StatusBarBottomWindow *>(win_mg_->GetWindow(WINDOWID_STATUSBAR_BOTTOM));
    this->Attach(status_bottom_bar);
	//StorageManager::GetInstance()->Attach(this);
    mainModule_ = mm;

}

DeviceSettingPresenter::~DeviceSettingPresenter()
{
    StatusBarWindow *status_bar = static_cast<StatusBarWindow *>(win_mg_->GetWindow(WINDOWID_STATUSBAR));
    this->Detach(status_bar);
    StatusBarBottomWindow *status_bottom_bar = static_cast<StatusBarBottomWindow *>(win_mg_->GetWindow(WINDOWID_STATUSBAR_BOTTOM));
    this->Detach(status_bottom_bar);
}

void DeviceSettingPresenter::OnWindowLoaded()
{
    db_msg("window load");

    if (status_ != MODEL_INITED) {
        this->DeviceModelInit();
    }

    lock_guard<mutex> lock(msg_mutex_);
    ignore_msg_ = false;
}

void DeviceSettingPresenter::OnWindowDetached()
{
    db_msg("window detach");

    lock_guard<mutex> lock(msg_mutex_);
    ignore_msg_ = true;

    if (status_ == MODEL_INITED) {
        this->DeviceModelDeInit();
    }
}

int DeviceSettingPresenter::WifiSwitch(int val)
{
    PreviewWindow *pre_win = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
    if( val )
        pre_win->Update((MSG_TYPE)MSG_SET_WIFI_ON);
    else
        pre_win->Update((MSG_TYPE)MSG_SET_WIFI_OFF);

    return 0;
}

int DeviceSettingPresenter::SoftAPSwitch(int val)
{
    return 0;
}

static int GetBeepToneConfig(int idx)
{
    int beep_map[] {100, 83, 60};
    return beep_map[idx];
}

int DeviceSettingPresenter::FormatStorage()
{
	this->Notify((MSG_TYPE)MSG_FORMAT_START);
    int ret = StorageManager::GetInstance()->Format();
    if( ret != 0 )
    {
        db_msg("format sd card failed\n");
    }
    else
    {
        db_msg("format sd card success\n");
    }

    sleep(1);
    this->Notify((MSG_TYPE)MSG_FORMAT_FINISH);
    return 0;
}

int DeviceSettingPresenter::GetDeviceInfo(int msgid)
{
    return 0;
}

int DeviceSettingPresenter::GetWifiInfo(int msgid)
{
   vector<string> wifi_info;
    string info,title_str,str_ssid,str_passw;
    NetManager::GetInstance()->GetWifiInfo(str_ssid, str_passw);


    ::LuaConfig config;
    config.LoadFromFile("/data/menu_config.lua");

    R::get()->GetString("ml_camera_wifiswitch_ssid", title_str);
    info = title_str;
    info += str_ssid;
    wifi_info.push_back(info);

    R::get()->GetString("ml_camera_wifiswitch_passwd", title_str);
    info = title_str;
    info += str_passw;
    wifi_info.push_back(info);

    R::get()->GetString("ml_wifi_connect_info", title_str);
    return 0;
}
int DeviceSettingPresenter::GetWifiAppDownloadQRcode(int msgid)
{
   db_msg("[debug_zhb]----GetWifiAppDownloadQRcode------");
   vector<string> wifi_app_download;
    string info,title_str;


    R::get()->GetString("ml_wifi_app_download_qr_code", title_str);
    info = title_str;
    wifi_app_download.push_back(info);

    return 0;
}

int DeviceSettingPresenter::GetTrafficInfo(int msgid)
{
   db_msg("[debug_zhb]----GetTrafficInfo------");
   vector<string> traffic_info;
    string info,title_str;

    R::get()->GetString("ml_traffic_surplus", title_str);
    info = title_str;
    traffic_info.push_back(info);

    R::get()->GetString("ml_traffic_total", title_str);
    info = title_str;
    traffic_info.push_back(info);

    R::get()->GetString("ml_traffic_info_query", title_str);
    return 0;
}
int DeviceSettingPresenter::GetFlowRechargeQRcode(int msgid)
{
   db_msg("[debug_zhb]----GetFlowRechargeQRcode------");
   vector<string> wifi_info;
    string info,title_str;

    R::get()->GetString("ml_scan_qr_code_recharge", title_str);
    info = title_str;
    wifi_info.push_back(info);

    return 0;
}
int DeviceSettingPresenter::GetSimInfo(int msgid)
{
   db_msg("[debug_zhb]----GetSimInfo------");
   vector<string> wifi_info;
    string info,title_str;

    R::get()->GetString("ml_sim_card_id", title_str);
    info = title_str;
    wifi_info.push_back(info);

    R::get()->GetString("ml_sim_network_business", title_str);
    info = title_str;
    wifi_info.push_back(info);

    R::get()->GetString("ml_sim_info_query", title_str);

    return 0;
}

int DeviceSettingPresenter::GetSubMenuData(int index)
{
    int hieght = 0;
    vector<string> submenu_info;
    string info;
    submenu_info.clear();

    //win->ShowSubmenu(hieght,submenu_info);
    return 0;
}
void DeviceSettingPresenter::ShowLevelBar(int levelbar_id)
{
   db_msg("[debug_zhb]----ShowLevelBar------");
}

Recorder* DeviceSettingPresenter::getRecorder(int p_CamId, int p_recorder_id)
{
    m_CamRecMap = mainModule_->getRecoderMap();
    if(m_CamRecMap.size() == 0){
        db_error("[debug_joson]: camera rec map is empty\n");
        return NULL;
    }

    CamRecMap::iterator cam_rec_iter;
    cam_rec_iter = m_CamRecMap.find(p_CamId);
    if(cam_rec_iter == m_CamRecMap.end())
    {
        db_error("[debug_joson]: first can't find the recoder\n");
        return NULL;
    }

	std::map<int, Recorder *>::iterator rec_iter;
	for(rec_iter = m_CamRecMap[p_CamId].begin(); rec_iter != m_CamRecMap[p_CamId].end(); rec_iter++)
	{	
		if( p_recorder_id != rec_iter->first)
			continue;

		return rec_iter->second;
	}

    return NULL;
}

Camera* DeviceSettingPresenter::getCamera(int p_CamId)
{
    m_CamMap = mainModule_->getCamerMap();
    if(m_CamMap.size() == 0)
    {
        db_error("[debug_joson]: camera_map size is empty\n");
        return NULL;
    }

    CameraMap::iterator cam_iter;
    cam_iter = m_CamMap.find(p_CamId);
    if(cam_iter == m_CamMap.end())
    {
        db_error("[debug_joson]: realy can't find the camera\n");
        return NULL;
    }

    return cam_iter->second;
}

void DeviceSettingPresenter::SetVideoResoulation(int p_CamId, int val)
{
    db_msg("[debug_jaosn]:####SetVideoResoulation val = %d####",val);
    Camera *cam = getCamera(p_CamId);
    if(cam == NULL)
        return;
    //if stream record is start should stop frist
    Recorder *rec0;
    rec0 = getRecorder(0,1);
    if(rec0 != NULL && rec0->RecorderIsBusy())
    {
        db_error("the streamRecord is start should stop first\n");
        isStreamRecodFlag = true; 
        rec0->StopRecord();
        usleep(200*1000);
    }
    
    cam->SetVideoResolution(val);

    if(rec0 != NULL && isStreamRecodFlag)
    {   
        usleep(200*1000);     
        db_error("the streamRecord is stop should start first\n");
        isStreamRecodFlag = false;
        rec0->StartRecord();
    }

}

void DeviceSettingPresenter::SetRearVideoResoulation(int p_CamId, int val)
{
    db_msg("[debug_zhb]:####SetRearVideoResoulation val = %d####",val);
    Camera *cam = getCamera(p_CamId);
    if(cam == NULL)
        return;

    bool previewing = cam->IsPreviewing();
    cam->SetVideoResolution(val);
    if( previewing )
        cam->ShowPreview();
}
int DeviceSettingPresenter::SetScreenBrightnessLevel(int val)
{
	db_msg("[debug_zhb]----SetScreenBrightnessLevel = %d",val);
	return  PowerManager::GetInstance()->SetBrightnessLevel(val);
}
void DeviceSettingPresenter::SetScreenDisturbMode(int val)
{
	db_msg("[debug_zhb]----SetScreenDisturbMode = %d",val);

}
void DeviceSettingPresenter::SetVoiceTakePhotoSwitch(int val)
{
	db_msg("[debug_zhb]----SetVoiceTakePhotoSwitch = %d",val);

}
void DeviceSettingPresenter::SetVolumeSelection(int val)
{
	db_msg("[debug_zhb]----SetVolumeSelection = %d",val);
    AudioCtrl::GetInstance()->SetBeepToneVolume(GetBeepToneConfig(val));
}

void DeviceSettingPresenter::SetPowerOnVoiceSwitch(int val)
{
	db_msg("[debug_zhb]----SetPowerOnVoiceSwitch = %d",val);
	char buf[64]={0};
	snprintf(buf,sizeof(buf),"echo %d > /data/power_on_voice.txt",val);
	system (buf);
}
void DeviceSettingPresenter::SetKeyPressVoiceSwitch(int val)
{
	db_msg("[debug_zhb]----SetKeyPressVoiceSwitch = %d",val);

}
void DeviceSettingPresenter::SetDriveringReportSwitch(int val)
{
	db_msg("[debug_zhb]----SetDriveringReportSwitch = %d",val);

}
void DeviceSettingPresenter::Set4GNetWorkSwitch(int val)
{
	db_msg("[debug_zhb]----Set4GNetWorkSwitch = %d",val);

}

void DeviceSettingPresenter::SetAccSwitch(int val)
{
	if(val){
		db_msg("[debug_zhb]----open acc");
		
		}else{
		db_msg("[debug_zhb]----close acc");
			}

}

void DeviceSettingPresenter::SetStandbyClockSwitch(int val)
{

	db_msg("[debug_zhb]----SetStandbyClockSwitch --- val = %d",val);
}

void DeviceSettingPresenter::SetAdasLaneShiftReminding(int val)
{
  #if 0
	if(val)
	{
		db_msg("[debug_zhb]----open adas SetAdasLaneShiftReminding");
		Uber_Control::GetInstance()->setUberADAS_LDW_Enable(true);
	}
	else
	{
		db_msg("[debug_zhb]----close adas SetAdasLaneShiftReminding");
		Uber_Control::GetInstance()->setUberADAS_LDW_Enable(false);
	}
#endif
}
void DeviceSettingPresenter::SetAdasForwardCollisionWaring(int val)
{
 #if 0
	if(val)
	{
		db_msg("[debug_zhb]----open adas SetAdasForwardCollisionWaring");
		Uber_Control::GetInstance()->setUberADAS_FCW_Enable(true);
	}
	else
	{
		db_msg("[debug_zhb]----close adas SetAdasForwardCollisionWaring");
		Uber_Control::GetInstance()->setUberADAS_FCW_Enable(false);
	}
 #endif
}


void DeviceSettingPresenter::SetWatchDogSwitch(int val)
{
	if(val == 0){
		db_msg("[debug_zhb]----open watch dog all switch");

		}else if(val == 1){
				db_msg("[debug_zhb]----noly open watch dog probe prompt switch");
			}else if(val == 2){
					db_msg("[debug_zhb]----noly open watch dog speeding prompt switch");
				}else{
					db_msg("[debug_zhb]----invalid parameter");
					}

}
void DeviceSettingPresenter::SetWatchProbePrompt(int val)
{

	db_msg("[debug_zhb]--- SetWatchProbePrompt---val = %d",val);

}
void DeviceSettingPresenter::SetWatchSpeedPrompt(int val)
{

	db_msg("[debug_zhb]--- SetWatchSpeedPrompt---val = %d",val);

}

void DeviceSettingPresenter::SetEmerRecordSwitch(int val)
{
	if(val){
		db_msg("[debug_zhb]----open SetEmerRecordSwitch");
		
		}else{
		db_msg("[debug_zhb]----close SetEmerRecordSwitch");
			}
}

void DeviceSettingPresenter::SetEmerRecordSensitivity(int p_CamId, int val) //碰撞灵敏度
{
	GsensorManager* gs =  GsensorManager::GetInstance();
	gs->writeImpactHappenLevel(val) ;
}

void DeviceSettingPresenter::SetParkingImpactSensitivity(int p_CamId, int val) //停车监控开关
{
	GsensorManager* gs =  GsensorManager::GetInstance();
	switch(val)
	{
	    case 0:
	        val = 3;
	    break;
	    case 1:
	        val = 0;
	    break;
	}
	gs->writeParkingSensibility(val);
}

void DeviceSettingPresenter::SetParkingWarnLampStatus(int val)
{
	db_msg("[debug_zhb]----SetParkingWarnLampStatus = %d",val);
}
void DeviceSettingPresenter::SetParkingAbnormalMonitoryStatus(int val)
{
	db_msg("[debug_zhb]----SetParkingAbnormalMonitoryStatus = %d",val);
}
void DeviceSettingPresenter::SetParkingAbnormalNoticeStatus(int val)
{
	db_msg("[debug_zhb]----SetParkingAbnormalNoticeStatus = %d",val);
}


void DeviceSettingPresenter::SetParkingLoopRecordStatus(int val)
{
	db_msg("[debug_zhb]----SetParkingLoopRecordStatus = %d",val);
}
void DeviceSettingPresenter::SetParkingLoopResolution(int val)
{
	db_msg("[debug_zhb]----SetParkingLoopResolution = %d",val);
}


int DeviceSettingPresenter::GetVideoResolution(int p_CamId, Size &p_size)
{
    Camera *cam = getCamera(p_CamId);
    if( cam == NULL )
        return -1;

    cam->GetVideoResolution(p_size);

    return 0;
}

void DeviceSettingPresenter::SetEncoderType(int p_CamId, int val)
{
    db_msg("[debug_jaosn]:####SetVideoEncoder val = %d####",val);
    Recorder *rec0 = getRecorder(0,0);
    if(rec0 == NULL)
        return;
    rec0->SetVideoEncoderType(val);
    Recorder *rec1 = getRecorder(1,2);
    if(rec1 == NULL)
        return;
    rec1->SetVideoEncoderType(val);
}


void DeviceSettingPresenter::SetVideoRecordTime(int val)
{
    db_msg("[debug_jaosn]:####SetVideoRecordTime val = %d####",val);
    Recorder *rec0 = getRecorder(0,0);
    if(rec0 == NULL)
        return;
    rec0->SetVideoClcRecordTime(val);
    Recorder *rec1 = getRecorder(1,2);
    if(rec1 == NULL)
        return;
    rec1->SetVideoClcRecordTime(val);
}

void DeviceSettingPresenter::SetEncodeSize(int p_CamId, int val)
{
    Recorder *rec = getRecorder(0,0);
    if(rec == NULL)
    	return;
	rec->SetRecordEncodeSize(val);
}

void DeviceSettingPresenter::SetPicResolution(int p_CamId, int val)
{
    db_msg("[debug_jaosn]:####SetPicResolution val = %d####",val);

	Camera *Cam = getCamera(p_CamId);
    if(Cam == NULL)
        return;
    Cam->SetPicResolution(val);

	Cam = getCamera(CAM_UVC_1);
	if(Cam == NULL)
		return;
	Cam->SetPicResolution(val);
}

int DeviceSettingPresenter::GetPicResolution(int p_CamId, Size &p_size)
{
    Camera *cam = getCamera(p_CamId);
    if( cam == NULL)
        return -1;

    cam->GetPicResolution(p_size);

    return 0;
}

int DeviceSettingPresenter::SetSlowCameraResloution(int p_CamId, int val)
{
    Camera *cam = getCamera(p_CamId);
    if( cam == NULL)
        return -1;

    bool previewing = cam->IsPreviewing();
    cam->SetSlowVideoResloution(val);
    if( previewing )
        cam->ShowPreview();
    return 0;
}

void DeviceSettingPresenter::SetRecordAudioOnOff(int p_CamId, int val)
{
    db_warn("[debug_jaosn]:####SetRecordAudioOnOff p_CamId %d val = %d####",p_CamId,val);
    Recorder *rec = NULL;
    if(p_CamId == 0) {
		rec = getRecorder(p_CamId,0); //front
		if(rec == NULL)
			return;
    }else if(p_CamId == 1){
		rec = getRecorder(p_CamId,2); //back
		if(rec == NULL)
			return;
    }
    #if 0
    int current_status_ = rec->GetStatus();
    if(current_status_ == RECORDER_RECORDING) {
    	rec->SetMute(!val);
    }
    #endif
    rec->SetRecordAudioOnOff(val);
}

void DeviceSettingPresenter::SetExposureValue(int p_CamId, int val)
{
    db_msg("[debug_jaosn]:####SetExposureValue val = %d####",val);
    Camera *cam = getCamera(p_CamId);
    if(cam == NULL)
        return;
    cam->SetExposureValue(val);
}

void DeviceSettingPresenter::SetADASSwitch(int val)
{
#ifdef ENABLE_ADAS
    Camera *cam = getCamera(0);
    if(cam == NULL){
       db_error("fatal error,camera is null!!");
       return;
    }
    if(val == 1)
        cam->StartADAS();
    else
        cam->StopADAS();
#endif
}

void DeviceSettingPresenter::SetADASCalibration(int val)
{
#ifdef ENABLE_ADAS
    Camera *cam = getCamera(0);
    if(cam == NULL){
       db_error("fatal error,camera is null!!");
       return;
    }
    if(val == 1)
        cam->OpenADASCalibration();
    else
        cam->CloseADASCalibration();
#endif
}

void DeviceSettingPresenter::SetWhiteBalance(int p_CamId, int val)
{
    db_msg("[debug_jaosn]:####SetWhiteBalance val = %d####",val);
    Camera *cam = getCamera(p_CamId);
    if(cam == NULL)
        return;
    cam->SetWhiteBalance(val);
}

void DeviceSettingPresenter::SetLedSwitch(int val)
{
    db_msg("[debug_jaosn]:####LedSwitch val = %d####",val);
    LedControl *led_ctrl = LedControl::get();
	//by hero ****** close led ctrl
    //led_ctrl->SetLedMainSwitch(val);
    if (softap_on_) {
        //led_ctrl->EnableLed(LedControl::WIFI_LED, val);
        //by hero ****** wifi led ctrl
    }
}

void DeviceSettingPresenter::SetLightFreq(int p_CamId, int val)
{
    db_error("[debug_jaosn]:####SetLightFreq val = %d####",val);
    Camera *cam = getCamera(0);
    if(cam == NULL)
       return;
    cam->SetLightFreq(val);
}


void DeviceSettingPresenter::SetTimeWaterMark(int p_CamId, int val)
{
    db_msg("[debug_jaosn]:####SetTimeWaterMark val = %d####",val);
	Camera *cam	= getCamera(p_CamId);
    if(cam == NULL)
        return;

    if(val)
    {
        cam->EnableOSD();
    }
    else
    {
        cam->DisableOSD();
    }
}

void DeviceSettingPresenter::ResetDevice()
{
    db_msg("[fangjj]:####ResetDevice####");
    int langval=0;
    MenuConfigLua *mcl=MenuConfigLua::GetInstance();
    NewSettingWindow *win = static_cast<NewSettingWindow *>(win_mg_->GetWindow(WINDOWID_SETTING_NEW));

    R *mRObj = R::get();
    mcl->ResetMenuConfig();
    UpdateAllSetting(true);
    win->ResetUpdate();//menu_ui update
    win_mg_->GetWindow(WINDOWID_PREVIEW)->OnLanguageChanged();
    win_mg_->GetWindow(WINDOWID_PLAYBACK)->OnLanguageChanged();
    win_mg_->GetWindow(WINDOWID_SETTING_NEW)->OnLanguageChanged();
}

void DeviceSettingPresenter::SetDeviceDateTime()
{
   db_msg("[debug_jaosn]:####SetDeviceDateTime####");
}


void DeviceSettingPresenter::SwitchDistortionCalibration(int val)
{
   db_msg("[debug_jaosn]:####SwitchDistortionCalibration val is %d####",val);
}

void DeviceSettingPresenter::HideInfoDialog()
{
}


void DeviceSettingPresenter::showButtonDialog()
{
}

void DeviceSettingPresenter::HandleButtonDialogMsg(int val)
{
    NewSettingWindow *win = static_cast<NewSettingWindow *>(win_mg_->GetWindow(WINDOWID_SETTING_NEW));
    PreviewWindow*pw = static_cast<PreviewWindow *>(win_mg_->GetWindow(WINDOWID_PREVIEW));
    switch(win->BulletCollection_->getButtonDialogCurrentId()){
        case BC_BUTTON_DIALOG_RESETFACTORY:
        {
            db_msg("[debug_zhb]--HandleButtonDialogMsg------BUTTON_DIALOG_RESETFACTORY");
            db_msg("[debug_zhb]--ready to reset factory ...");
            if(val == 1){
                DialogStatusManager::GetInstance()->setMDialogEventFinish(false);
                usleep(300*1000);//wait laster dialog hide
                pw->ShowPromptInfo(PROMPT_RESET_FACTORY_ING,0);
                win->BulletCollection_->ShowButtonDialogSettingButtonStatus(MSG_SET_SHOW_UP_DWON_BUTTON);
                //pw->VideoRecordDetect(false);//if resetfactory before is record should stop recording and the reset
                ResetDevice();
                //pw->VideoRecordDetect(true);
                pw->ShowPromptInfo(PROMPT_RESET_FACTORY_FINISH,2,true);
                DialogStatusManager::GetInstance()->setMDialogEventFinish(true);
            }else{
                win->BulletCollection_->ShowButtonDialogSettingButtonStatus(MSG_SET_HIDE_UP_DWON_BUTTON);
            }
            db_msg("[debug_zhb]--finish the  reset factory ...");
        }
        break;
        case BC_BUTTON_DIALOG_FORMAT_SDCARD:    //button are confirm and cancel , info_text
        {
            if(val == 1){
                win->Update((MSG_TYPE)MSG_FORMAT_START);
                DialogStatusManager::GetInstance()->setMDialogEventFinish(false);
                usleep(500*1000);
                pw->ShowPromptInfo(PROMPT_TF_FORMATTING,0);
                win->BulletCollection_->ShowButtonDialogSettingButtonStatus(MSG_SET_HIDE_UP_DWON_BUTTON);
               // pw->VideoRecordDetect(false);//if format before is record should stop recording and the format
                if(StorageManager::GetInstance()->Format() < 0)
                {
                    db_error("format failed");
                    pw->ShowPromptInfo(PROMPT_TF_FORMAT_FAILED,2,true);//force to kill the front dialog and show self
                    DialogStatusManager::GetInstance()->setMDialogEventFinish(true);
                    win->Update((MSG_TYPE)MSG_FORMAT_FAILED);
                }
                else
                {
                    pw->ShowPromptInfo(PROMPT_TF_FORMAT_FINISH,2,true);//force to kill the front dialog and show self
                    DialogStatusManager::GetInstance()->setMDialogEventFinish(true);
                    win->Update((MSG_TYPE)MSG_FORMAT_FINISH);
                }
            }else{
                win->BulletCollection_->ShowButtonDialogSettingButtonStatus(MSG_SET_HIDE_UP_DWON_BUTTON);
                win->Update((MSG_TYPE)MSG_CANCLE_DIALG);
            }
        }
        break;
        default:
            break;
    }
    win->BulletCollection_->setButtonDialogShowFlag(false);
}

int DeviceSettingPresenter::stopSystemRecords(void)
{
    Recorder *rec;
    rec =getRecorder(0,0);
    if( rec != NULL)
        rec->StopRecord();

    rec =getRecorder(0,1);
    if( rec != NULL)
        rec->StopRecord();

    rec =getRecorder(1,2);
    if( rec != NULL)
        rec->StopRecord();

	rec =getRecorder(1,3);
    if( rec != NULL)
        rec->StopRecord();

#ifdef SUB_RECORD_SUPPORT
    rec =getRecorder(REC_S_SUB_CHN);
    if( rec != NULL)
        rec->StopRecord();
#endif
	Camera *cam = getCamera(0);
    if( cam != NULL )
        cam->DeinitCamera();

    cam = getCamera(1);
    if( cam != NULL )
        cam->DeinitCamera();

	WatchDog *dog = WatchDog::GetInstance();
    dog->StopWatchDog();

    return 0;
}

int DeviceSettingPresenter::exeOtaUpdate(int update_type)
{
    char temp[512] = {0};
    db_error("exeOtaUpdate update_type:%d", update_type);

    stopSystemRecords();

    if(update_type == TYPE_UPDATE_NET) {
        snprintf(temp,sizeof(temp),"/etc/chroot_ota.sh %s/%s/",MOUNT_PATH,VERSION_DIR_NET);
        system(temp);
        db_error("exeOtaUpdate update_success, reboot...");
        system("reboot -f");
        db_error("error: ota_update fail ready to again");
    }else if(update_type == TYPE_UPDATE_CARD){
        snprintf(temp,sizeof(temp),"/etc/chroot_ota.sh %s/",MOUNT_PATH);
        system(temp);
        db_error("exeOtaUpdate update_success, reboot...");
        system("reboot -f");
        db_error("error: ota_update fail ready to again");
    }

    return 0;
}

int DeviceSettingPresenter::HandleGUIMessage(int msg, int val,int id)
{
    db_warn("----------> setting device  msg[%d], val[%d]", msg, val);
    switch(msg) 
    {
        case MSG_SET_VIDEO_RESOULATION:
        {
            db_warn("[habo]---> MSG_SET_VIDEO_RESOULATION");
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set video resoulation failed!");
                return -1;
            }
            SetVideoResoulation(0,val);
            SetEncodeSize(0,val);
            if(val == 1){
                db_error("set MSG_SET_ADAS_SWITCH val %d",val);
                int ret = SetMenuConfig(MSG_SET_ADAS_SWITCH,0);
                if(ret < 0){
                    db_error("set adas failed!");
                    return -1;
                }
                SetADASSwitch(0);
            }
            int freq = MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_CAMERA_LIGHTSOURCEFREQUENCY);
            SetLightFreq(0,freq);
            db_error("set light freq");
        }
        break;
        case MSG_SET_CAMERA_EXPOSURE:
        {
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set camera exposure failed!");
                return -1;
            }
            SetExposureValue(0,val);

        }break;
        case MSG_SET_VOLUME_SELECTION:
        {
            db_warn("[habo]---> MSG_SET_VOLUME_SELECTION");
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set record volume failed!");
                return -1;
            }
            SetVolumeSelection(val);
        }break;
        case MSG_SET_CAMERA_LIGHTSOURCEFREQUENCY:
        {
            db_warn("MSG_SET_CAMERA_LIGHTSOURCEFREQUENCY");
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set camera light source frequency failed!");
                return -1;
            }
            SetLightFreq(0,val);
        }break;
        case MSG_SET_PARKING_MONITORY:
        {
            db_warn("MSG_SET_PARKING_MONITORY %d",val);
#if 1
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set parking monitory failed!");
                return -1;
            }
            SetParkingImpactSensitivity(0,val);
#endif
        }break;
        case MSG_SET_EMER_RECORD_SENSITIVITY:
        {
            db_warn("[habo]---> MSG_SET_EMER_RECORD_SENSITIVITY %d",val);
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set record sensitivity failed!");
                return -1;
            }
            SetEmerRecordSensitivity(id, val);            
        }break;
        case MSG_SET_RECORD_ENCODE_TYPE:
        {
            db_warn("[habo]---> MSG_SET_RECORD_ENCODE_TYPE");
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set record type failed!");
                return -1;
            }
            SetEncoderType(0,val);
        }break;
        case MSG_SET_RECORD_TIME:
        {
            db_warn("[habo]---> MSG_SET_RECORD_TIME");
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set record time failed!");
                return -1;
            }
            SetVideoRecordTime(val);            
        }break;
        case MSG_SET_AUTO_TIME_SCREENSAVER:
        {
            db_warn("[habo]---> MSG_SET_AUTO_TIME_SCREENSAVER");
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set screensave failed!");
                return -1;
            }
            SetAutoScreensaver(val);            
        }break;
        case MSG_RM_LANG_CHANGED:
        {
            db_warn("[habo]---> MSG_RM_LANG_CHANGED");
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set language failed!");
                return -1;
            }
            R *mRObj = R::get(); 
            mRObj->SetLangID(val);
            //下面设置相关窗口的字符词变换
#ifndef SETTING_WIN_USE
            NewSettingWindow *win = static_cast<NewSettingWindow *>(win_mg_->GetWindow(WINDOWID_SETTING_NEW));
            win->ResetUpdate();
#endif
            win_mg_->GetWindow(WINDOWID_SETTING_NEW)->OnLanguageChanged();
            win_mg_->GetWindow(WINDOWID_PLAYBACK)->OnLanguageChanged();

        }break;
        case SETTING_BUTTON_DIALOG:
        {
             HandleButtonDialogMsg(val);
        }
        break;
        case MSG_SET_RECORD_VOLUME:
        {
             db_error("[debug_zhb]-----MSG_SET_RECORD_VOLUME---val = %d",val);
             int ret = SetMenuConfig(msg,val);
             if(ret < 0){
                 db_error("set record volume failed!");
                 return -1;
             }
             db_error("set record volume by app,update voice icon");
             SetRecordAudioOnOff(0,val);
             StatusBarWindow *status_bar = static_cast<StatusBarWindow *>(win_mg_->GetWindow(WINDOWID_STATUSBAR));
             status_bar->ShowVoiceIcon(true);
        }break;
        case MSG_SET_WIFI_SWITCH:
        {
            db_error("[debug_zhb]-----MSG_SET_WIFI_SWITCH---val = %d",val);
            WifiSwitch(val);
            SetMenuConfig(msg,val);
        }break;
#if 0
		case MSG_SET_MOTION_DETECT:
		{
		    db_error("[debug_zhb]-----MSG_SET_MOTION_DETECT---val = %d",val);
		    int ret = SetMenuConfig(msg, val);
		    if(ret < 0){
                db_error("set motion detect failed!");
                return -1;
            }
            EnableMotionDetect(0,val);
			break;
		}
#endif
		case MSG_SET_ADAS_SWITCH:
		{
#ifdef ENABLE_ADAS
		    db_error("set MSG_SET_ADAS_SWITCH val %d",val);
		    int ret = SetMenuConfig(msg,val);
            if(ret < 0){
               db_error("set record type failed!");
               return -1;
            }
		    SetADASSwitch(val);
#endif
		}
		break;
		case MSG_SET_ADAS_CALIBRATION:
		{
#ifdef ENABLE_ADAS
		    db_error("set MSG_SET_ADAS_CALIBRATION val %d",val);
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
              db_error("set record type failed!");
              return -1;
            }
            SetADASCalibration(val);
#endif
        }
        break;
        case MSG_SETTING_TO_PREVIEW:
            this->Notify((MSG_TYPE)MSG_PLAYBACK_TO_PREIVEW_CHANG_STATUS_BAR_BOTTOM);
        break;
        case MSG_SET_VOICE_CTRL:
        {
            db_error("set MSG_SET_VOICE_CTRL");
            int ret = SetMenuConfig(msg,val);
            if(ret < 0){
                db_error("set voice ctrl failed!");
                return -1;
            }
            StatusBarWindow *status_bar = static_cast<StatusBarWindow *>(win_mg_->GetWindow(WINDOWID_STATUSBAR));
            status_bar->ShowVoiceCtrlIcon(true);
        }
        break;
        case MSG_SYSTEM_UPDATE:
        {
            db_debug("ota update now!!!");
            exeOtaUpdate(TYPE_UPDATE_CARD);
        }
        break;
        default:
            break;
    }
    
    return 0;
}

int DeviceSettingPresenter::EnableMotionDetect(int p_nCamid,bool p_bEnable)
{
    Camera *cam = getCamera(p_nCamid);
    if(cam != NULL)
    {
        if(p_bEnable)
            cam->startMotionDetect();
        else
            cam->stopMotionDetect();
    }
    return 0;
}



int DeviceSettingPresenter::GetMenuConfig(int msg)
{
       int val=0;
       MenuConfigLua *menuconfiglua=MenuConfigLua::GetInstance();
	val = menuconfiglua->GetMenuIndexConfig(msg);
       db_msg("[fangjj]:GetMenuConfig:msg[%d], val[%d]", msg, val);
	return val;
}

int DeviceSettingPresenter::SetMenuConfig(int msg, int val)
{
      MenuConfigLua *menuconfiglua=MenuConfigLua::GetInstance();
      db_msg("[fangjj]:SetMenuConfig:msg[%d], val[%d]", msg, val);
      int ret = menuconfiglua->SetMenuIndexConfig(msg,val);
      return ret;
}

int DeviceSettingPresenter::SetIspModuleOnOff(int p_CamId, int val)
{
    Camera *cam = getCamera(p_CamId);
    if(cam == NULL){
        return -1;
        }
    //cam->ISPModuleOnOff(val);
    return 0;
}

void DeviceSettingPresenter::SetAutoScreensaver(int val)
{
	Screensaver *ss = Screensaver::GetInstance();
	db_msg("SetAutoScreensaver:val[%d]",val);
	switch(val)
	{
		case AUTOSCREENSAVER_10SEC:
			ss->SetDelayTime(10);
			ss->SetScreensaverEnable(true);
			ss->Start();
			break;
		case AUTOSCREENSAVER_30SEC:
			ss->SetDelayTime(30);
			ss->SetScreensaverEnable(true);
			ss->Start();
			break;
		case AUTOSCREENSAVER_60SEC:
			ss->SetDelayTime(60);
			ss->SetScreensaverEnable(true);
			ss->Start();
			break;
		case AUTOSCREENSAVER_OFF:
			ss->SetScreensaverEnable(false);
			ss->Stop();
			break;
		default:
		break;
	}
}

void DeviceSettingPresenter::SetTimedShutdown(int val)
{
	Autoshutdown *ss = Autoshutdown::GetInstance();
	db_msg("SetTimedShutdown:val[%d]", val);
	switch(val)
	{
		case AUTOSHUTDOWN_OFF:
			ss->Stop();
			ss->SetAutoshutdownEnable(false);
			break;
		case AUTOSHUTDOWN_3MIN:
			ss->SetDelayTime(3*60);
			ss->SetAutoshutdownEnable(true);
			ss->Start();
			break;
		case AUTOSHUTDOWN_5MIN:
			ss->SetDelayTime(5*60);
			ss->SetAutoshutdownEnable(true);
			ss->Start();
			break;
		case AUTOSHUTDOWN_10MIN:
			ss->SetDelayTime(10*60);
			ss->SetAutoshutdownEnable(true);
			ss->Start();
			break;
		default:
		break;
	}

}
void DeviceSettingPresenter::SetAutoStatusBarSaver(bool enable_flag)
{
	StatusBarSaver *sbs = StatusBarSaver::GetInstance();
	sbs->SetDelayTime(5);
	if(enable_flag){
        sbs->SetStatusBarSaverEnable(enable_flag);
        sbs->Start();
	} else {
	    sbs->SetStatusBarSaverEnable(enable_flag);
	    sbs->Stop();
	}
}


void DeviceSettingPresenter::SettingHandlerWindowUpdateLabel()
{

}

void DeviceSettingPresenter::SetCameraPreview(int val)
{
    Camera *cam0 = getCamera(CAM_A);
    Camera *cam1 = getCamera(CAM_B);
    if( cam0 != NULL )
    {
        ViewInfo cam0_rect;
        cam0->SetCameraPreviewRegionSize(CAM_A);
        cam0->GetCameraDispRect(cam0_rect);
        if(val == 0){
            cam0->SetCameraDispRect(cam0_rect, 2);
        }else{
            cam0->SetCameraDispRect(cam0_rect, 0);
        }
    }
    if( cam1 != NULL )
    {
        ViewInfo cam1_rect;
        cam1->SetCameraPreviewRegionSize(CAM_B);
        cam1->GetCameraDispRect(cam1_rect);
        if(val == 1){
            cam1->SetCameraDispRect(cam1_rect, 2);
        }else{
            cam1->SetCameraDispRect(cam1_rect, 0);
        }
    }
}

void DeviceSettingPresenter::UpdateAllSetting(bool update_video_setting)
{
    int val=0;
    db_msg("[fangjj]:--------------NotifyAll------------begin \n");

    /*device setting*/
    val= GetMenuConfig(SETTING_RECORD_RESOLUTION);
    SetEncodeSize(0, val);

    val= GetMenuConfig(SETTING_RECORD_TIME);
    SetVideoRecordTime(val);

//    val= GetMenuConfig(SETTING_RECORD_LOOP_SWITCH);
//    SetRecordLoop(val);

    val = GetMenuConfig(SETTING_EMER_RECORD_SENSITIVITY);
    SetEmerRecordSensitivity(0, val);

//    val = GetMenuConfig(SETTING_TIMEWATERMARK);
//    SetRecordTimeWaterMark(0, val);

//    val= GetMenuConfig(SETTING_RECORD_VOLUME_SWITCH);
//    WifiSwitch(val);

    val= GetMenuConfig(SETTING_CAMERA_EXPOSURE);
    SetExposureValue(0, val);

#ifdef ENABLE_ADAS
    val = GetMenuConfig(SETTING_ADAS_SWITCH);
    SetADASSwitch(val);
#endif

    val = GetMenuConfig(SETTING_CAMERA_LIGHTSOURCEFREQUENCY);
    //SetLightFreq(0, val);

    val = GetMenuConfig(SETTING_VOLUME_SELECTION);
    SetVolumeSelection(val);

//    val= GetMenuConfig(SETTING_RECORD_ENCODINGTYPE);
    SetEncoderType(0,1);

    val = GetMenuConfig(SETTING_PARKING_MONITORY);
    SetParkingImpactSensitivity(0,val);

    val= GetMenuConfig(SETTING_CAMERA_AUTOSCREENSAVER);
    SetAutoScreensaver(val);

    SetAutoStatusBarSaver(true);
    /*****device******/
    val =GetMenuConfig(SETTING_DEVICE_LANGUAGE);
    R *mRObj = R::get();
    mRObj->SetLangID(val);

    win_mg_->GetWindow(WINDOWID_STATUSBAR)->OnLanguageChanged();
    win_mg_->GetWindow(WINDOWID_PREVIEW)->OnLanguageChanged();
    win_mg_->GetWindow(WINDOWID_PLAYBACK)->OnLanguageChanged();

    val= GetMenuConfig(SETTING_WIFI_SWITCH);
    WifiSwitch(val);

    val= GetMenuConfig(SETTING_PREVIEW_CAMERA);
    SetCameraPreview(val);

//    val =GetMenuConfig(SETTING_MOTION_DETECT);
//    EnableMotionDetect(0,val);
    db_msg("--------------NotifyAll------------ over\n");

}

void DeviceSettingPresenter::NotifyAll()
{
    UpdateAllSetting(false);
}

void DeviceSettingPresenter::BindGUIWindow(::Window *win)
{
    this->Attach(win);
}

int DeviceSettingPresenter::DeviceModelInit()
{
    db_msg("device setting presenter device model init");
     StorageManager::GetInstance()->Attach(this);
    //NetManager::GetInstance()->Attach(this);
    m_CamMap = mainModule_->getCamerMap();
    m_CamRecMap = mainModule_->getRecoderMap();

    // TODO: update ui status
    status_ = MODEL_INITED;
    return 0;
}

int DeviceSettingPresenter::DeviceModelDeInit()
{
	StorageManager::GetInstance()->Detach(this);
    // NetManager::GetInstance()->Detach(this);
    status_ = MODEL_UNINIT;
    m_CamMap.clear();
    m_CamRecMap.clear();
    return 0;
}

// 等待底层通知回调
void DeviceSettingPresenter::Update(MSG_TYPE msg, int p_CamID, int p_recordId)
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

    switch (msg) {
        case MSG_SOFTAP_SWITCH_DONE:
            softap_on_ = true;
            break;
        case MSG_SOFTAP_DISABLED:
            softap_on_ = false;
            break;
        case MSG_WIFI_ENABLED:
            break;
        case MSG_WIFI_SCAN_END: 
            break;
	    case MSG_STORAGE_UMOUNT:
	    case MSG_STORAGE_MOUNTED:
		{
	    }
		break;
	    case MSG_TO_PREVIEW_WINDOW:
		{

        }
		return;
        case MSG_BACKCARVIDEO_ON:
        {
            MenuConfigLua *mcl=MenuConfigLua::GetInstance();
            mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CAMERA,1);
            Camera *camA = NULL;
            Camera *camB = NULL;
            camA = getCamera(CAM_A);
            camB = getCamera(CAM_B);
            ViewInfo cam0_rect,cam1_rect;
            if( camA != NULL && camB != NULL)
            {
                camA->GetCameraDispRect(cam0_rect);
                camB->GetCameraDispRect(cam1_rect);
                camB->SetCameraDispRect(cam1_rect, 2);
                camA->SetCameraDispRect(cam1_rect, 0);
            }else{
                mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CAMERA,0);
            }
            NewSettingWindow *win = static_cast<NewSettingWindow*>(win_mg_->GetWindow(WINDOWID_SETTING_NEW));
            std::thread([=] {
                win->ReturnPreviewWindow(true);
            }).detach();
        }
        break;
        case MSG_AHD_CONNECT:
        {
            NewSettingWindow *win = static_cast<NewSettingWindow*>(win_mg_->GetWindow(WINDOWID_SETTING_NEW));
            std::thread([=] {
                win->ReturnPreviewWindow(false);
            }).detach();
        }
        break;
        default:
            break;
    }

    this->Notify(msg);
}

int DeviceSettingPresenter::StartCamBRecord(int p_nEnable)
{
	Recorder *rec = getRecorder(CAM_B,2);
	if(rec != NULL)
	{
		if(p_nEnable)
		{
			Recorder *rec_A = getRecorder(CAM_A, 0);
			if( (rec_A == NULL) || (rec_A->RecorderIsBusy() == false))
			{
				db_warn("cam A not start record yet, so could not start cam B");
				return 0;
			}

			if(!rec->RecorderIsBusy() )
			{
				rec->StartRecord();
				RecorderParam param;
				rec->GetParam(param);
				Camera *cam = getCamera(CAM_B);
				if(cam != NULL)
				{
					cam->SetVideoFileForThumb(param.MF);
					cam->SetPictureMode(TAKE_PICTURE_MODE_FAST);
					cam->TakePictureEx(2, true);							
				}
			}
		}
		else
		{
			if( rec->RecorderIsBusy() )
				rec->StopRecord();
		}
	}

	return 0;
}
