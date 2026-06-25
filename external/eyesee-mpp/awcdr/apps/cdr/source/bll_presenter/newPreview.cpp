/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/*
 * @file newPreview.cpp
 * @author id:826
 * @version v0.3
 * @date 2016-11-03
 */

#include "newPreview.h"
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include "common/app_log.h"
#include "common/posix_timer.h"
#include "window/window_manager.h"
#include "window/prompt.h"
#include "device_model/storage_manager.h"
#include "device_model/system/power_manager.h"
#include "device_model/media/osd_manager.h"
#include "bll_presenter/camRecCtrl.h"
#include "bll_presenter/voice_ctrl.h"
#include "device_model/system/led.h"
#include "window/preview_window.h"
#include "device_model/media/media_file_manager.h"
#include "device_model/dataManager.h"
#include "device_model/system/net/net_manager.h"
#include "device_model/partitionManager.h"
#include "device_model/system/event_manager.h"
#include "device_model/media/camera/camera.h"
#include "device_model/menu_config_lua.h"
#include "device_model/system/gsensor_manager.h"
#include "uilayer_view/gui/minigui/window/status_bar_bottom_window.h"
#include "window/usb_mode_window.h"
#include "device_model/system/net/net_manager.h"
#include "device_model/system/net/wifi_connector.h"
#include "device_model/system/net/softap_controller.h"
#include "bll_presenter/remote/interface/dev_ctrl_adapter.h"
#include <device_model/media/media_file.h>


#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "newPreview.cpp"
#endif

#define USE_CAMA

#define USE_CAMB
#define CAMB_PREVIEW
#define DEINIT_CAMERA   //回放销毁camera

using namespace std;

NewPreview::NewPreview(MainModule *mm)
{
#if 0
	lowpower_shutdown_processing_ = false;
	
	m_MainModule = mm;
	StorageManager::GetInstance()->Attach(this);
	MediaFileManager::GetInstance()->Attach(this);
	PowerManager::GetInstance()->Attach(this);
	NetManager::GetInstance()->Attach(this);
	Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_STATUSBAR_BOTTOM));
	Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_STATUSBAR));
    #ifdef SETTING_WIN_USE
	Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_SETTING));
    #else
    Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_SETTING_NEW));
    #endif
	m_cameraMap.clear();
	m_CamRecMap.clear();
	m_bOsdEnable = true;
//	m_backCameraIsRecording = false;
	m_camBShowFlag = PowerManager::GetInstance()->getAhdInsertOnline();//MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_CAMB_PREVIEWING);
	m_sosRecordisStart = false;
    m_Impact_happen_flag = false;
    isRecordStart = false;
    m_usb_attach_status = false;
    mode_ = USB_MODE_CHARGE;
    win_mg_ = WindowManager::GetInstance();
    m_workMode = PowerManager::GetInstance()->getPowenOnType();
	MediaInit();
#endif
    
	lowpower_shutdown_processing_ = false;
    m_MainModule = mm;
    m_cameraMap.clear();
    m_CamRecMap.clear();
    m_bOsdEnable = true;
    filelock_flag_ = false;
//  m_backCameraIsRecording = false;
#ifdef USE_CAMB
    m_camBShowFlag = PowerManager::GetInstance()->getAhdInsertOnline();//MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_CAMB_PREVIEWING);
#else
    m_camBShowFlag = 0;
#endif
    m_sosRecordisStart = false;
    m_Impact_happen_flag = false;
    isRecordStart = false;
    m_usb_attach_status = false;
    mode_ = USB_MODE_CHARGE;
    m_workMode = PowerManager::GetInstance()->getPowenOnType();
    deinit_flag_ = false;
    record_start_process_flag_ = false;
    camB_created_flag_ = false;
    camB_recinit_fail_flag_ = false;
    take_thumb_flag_ = false;
    cama_park_file_ = NULL;
    camb_park_file_ = NULL;
    if(m_workMode == 0){
         db_msg("newpreview screenoff");
         PowerManager::GetInstance()->ScreenOff();
    }
    MediaInit();
    #ifdef ENABLE_RTSP    
    m_NetManger = NetManager::GetInstance();
    m_RtspServer = RtspServer::GetInstance();
    #endif
    #if 1
    dev_adapter_ = new DeviceAdapter(this);
//    m_httpServer_ = httpServer::GetInstance();
//    m_httpServer_->setAdapter(dev_adapter_);
//    m_httpServer_->start();
//    m_httpServer_->init();
    #endif
#ifdef ENABLE_ADAS
    create_timer(this, &adas_event_timer_,ADASEventUpdate);
    memset(&adas_event_,0,sizeof(AW_AI_ADAS_DETECT_R__v2));
    pthread_mutex_init(&adas_lock_,NULL);
#endif
}

void NewPreview::InitWindow()
{
    db_error("Init window");
    NetManager::GetInstance()->Attach(this);
    StorageManager::GetInstance()->Attach(this);
    MediaFileManager::GetInstance()->Attach(this);
    PowerManager::GetInstance()->Attach(this);
//    httpServer::GetInstance()->Attach(this);
    Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_STATUSBAR_BOTTOM));
    Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_STATUSBAR));
    Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_PLAYBACK));
    Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_SETTING_NEW));
    win_mg_ = WindowManager::GetInstance();
    CreateRecorder(CAM_A, 0);
    #ifdef USE_CAMB
    if(m_camBShowFlag == 1){
        CreateRecorder(CAM_B, 2);
    }
    #endif
    OsdManager::get()->initTimeOsd(m_CamRecMap);
    EventManager::GetInstance()->ActiveDevUevent();
    printf("InitWindow\n");
}

void NewPreview::VoiceCtrlInit(){
#ifdef VOICECTRL_SUPPORT
    VoiceCtrl::GetInstance()->Init();
    VoiceCtrl::GetInstance()->RunVoiceCollectionThread();
    VoiceCtrl::GetInstance()->Attach(this);
#endif

}

NewPreview::~NewPreview()
{
#ifdef ENABLE_ADAS
    pthread_mutex_destroy(&adas_lock_);
#endif
	db_msg("by hero *** ~NewPreview");
}

int NewPreview::MediaInit()
{
    MenuConfigLua *mcl=MenuConfigLua::GetInstance();
    int preview_camera = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CAMERA);
#ifdef USE_CAMB
    if(m_camBShowFlag == 1){
        ViewInfo cam1_rect;
        if(!deinit_flag_ || !camB_created_flag_){
            Camera *cam = CreateCamera(CAM_B);
            if(cam != NULL)
                  cam->Attach(this);
        }
        if(GetCamera(CAM_B) != NULL && GetCamera(CAM_B)->GetCameraStatus() == CAM_DEINITED)
        {
            CameraInitParam init_param;
            GetCamera(CAM_B)->GetCameraInitParam(init_param);
            GetCamera(CAM_B)->InitCamera(init_param);
        }
        GetCamera(CAM_B)->GetCameraDispRect(cam1_rect);
        if(preview_camera == 1){
            GetCamera(CAM_B)->SetCameraDispRect(cam1_rect, 2);
        }else{
            GetCamera(CAM_B)->SetCameraDispRect(cam1_rect, 0);
        }
        GetCamera(CAM_B)->SetCameraWorkMode(m_workMode);
        if(!deinit_flag_)
            StartPreview(CAM_B);
        GetCamera(CAM_B)->SetCameraPreviewRegionSize(CAM_B);
        GetCamera(CAM_B)->ShowPreview();
    }else{
        mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CAMERA,0);
    }
#endif
#ifdef USE_CAMA
        Camera *cam = CreateCamera(CAM_A);
        if(cam != NULL){
            cam->Attach(this);
        }else{
            return -1;
        }
        ViewInfo cam0_rect;
        GetCamera(CAM_A)->GetCameraDispRect(cam0_rect);
        if(preview_camera == 0){
            GetCamera(CAM_A)->SetCameraDispRect(cam0_rect, 2);
        }else{
            GetCamera(CAM_A)->SetCameraDispRect(cam0_rect, 0);
        }
        GetCamera(CAM_A)->SetCameraWorkMode(m_workMode);
        StartPreview(CAM_A);
        GetCamera(CAM_A)->SetCameraPreviewRegionSize(CAM_A);
        GetCamera(CAM_A)->ShowPreview();
        cam = GetCamera(CAM_A);
        if(cam != NULL){
            int freq = MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_CAMERA_LIGHTSOURCEFREQUENCY);
            cam->SetLightFreq(freq);
            db_error("set light freq");
        }
#endif

    return 0;
}

void NewPreview::MediaDeInit()
{
    db_debug("MediaDeInit Now!");
#ifdef USE_CAMA
    DestoryRecorder(CAM_A,0);
    DestoryRecorder(CAM_A,1);
#endif

#ifdef USE_CAMB
    if(!deinit_flag_){
        DestoryRecorder(CAM_B, 2);
        DestoryRecorder(CAM_B, 3);
    }
#endif
#ifdef USE_CAMA
    Camera *cam = GetCamera(CAM_A);
    if(cam != NULL ){
#ifdef ENABLE_ADAS
        cam->StopADAS();
        db_error("stop adas");
#endif
        cam->Detach(this);
    }
    DestoryCamera(CAM_A);
#endif

#ifdef USE_CAMB
    if(!deinit_flag_){
        if(m_camBShowFlag == 1){
            Camera *cam = GetCamera(CAM_B);
            if(cam != NULL ){
               cam->Detach(this);
            }
            DestoryCamera(CAM_B);
        //    VoiceCtrl::GetInstance()->Detach(this);
        //    VoiceCtrl::Destroy();
        }
    }
#endif

}

Camera* NewPreview::CreateCamera(int p_CamId)
{
	if(!CheckCamIdIsValid(p_CamId))
		return NULL;

	Camera *cam = NULL;
	if(!CheckCameraExist(p_CamId))
	{
		switch(p_CamId)
		{
			case CAM_A:
				cam = CameraFactory::GetInstance()->CreateCamera(CAM_NORMAL_0);
				break;
			case CAM_B:
				cam = CameraFactory::GetInstance()->CreateCamera(CAM_UVC_1);
				camB_created_flag_ = true;
				break;
		}
		if( cam != NULL )
		{
			m_cameraMap.insert(make_pair(p_CamId, cam));
			m_MainModule->setCamerMap(m_cameraMap);
		}
	}
	else
	{
        db_error("yxl: camera is exist no need release");
		cam = GetCamera(p_CamId);
	}


	return cam;
}

#ifdef SHOW_DEBUG_INFO
void NewPreview::DebugInfoThread(NewPreview *self)
{
	db_error("creat debug info thread!!!");
    prctl(PR_SET_NAME, "DebugInfoThread", 0, 0, 0);
	EventManager *even_ = EventManager::GetInstance();
    while (true) {

    	self->preview_win_->ClearDebugInfo();
    	FILE *fp = NULL;
		int ret = 0;
		char gps_infomsg_buf[10] = {0};
		char gps_num_buf[10] = {0};
		string res;
		string res1;
		string gngga_buff;
		string gpgsv_buff;
		stringstream ss;
		stringstream ss1;
#if 1
		memset(gps_infomsg_buf, 0, sizeof(gps_infomsg_buf));
		memset(gps_num_buf, 0, sizeof(gps_num_buf));
		int gps_info_msg = 0,gps_num = 0;
		gps_info_msg = even_->GetGPSSignalInfo();
		gps_num = even_->GetGpsSignalLevel();
		
		gngga_buff = even_->GNGGA_string;
		string info = gngga_buff;
		self->preview_win_->InsertDebugInfo("GNGGA:", info);

		gpgsv_buff = even_->GPGSV_string;
		string info1 = gpgsv_buff;
		self->preview_win_->InsertDebugInfo("  GNRMC: ", info1);
		/*
//		db_error("==============gps_info_msg %d,gps_num %d==============",gps_info_msg,gps_num);
		if (gps_info_msg >= 0) {
   		   ss << gps_info_msg;
		   ss >> res;
		   string info = "  GPS count Info: " + res;
		   self->preview_win_->InsertDebugInfo("GPS Signal Info:  ", info);
		}
		if (gps_num >= 0) {
			ss1 << gps_num;
			ss1 >> res1;
			string info = " GPS Signal Level: " + res;
			self->preview_win_->InsertDebugInfo("GPS Signal Level:  ", info);
		}
		*/
#endif
		
        sleep(1);
    }
}
#endif

int NewPreview::DestoryCamera(int p_CamId)
{
    if( !CheckCamIdIsValid(p_CamId) )
        return -1;

    if(CheckCameraExist(p_CamId))
    {
        map<int, Camera*>::iterator iter;
        for(iter = m_cameraMap.begin(); iter != m_cameraMap.end(); iter++)
        {
            if( iter->first == p_CamId ){
                delete iter->second;
                m_cameraMap.erase(iter);
                m_MainModule->setCamerMap(m_cameraMap);
                if(p_CamId == CAM_B)
                    camB_created_flag_ = false;
                return 0;
            }
        }
    }
    return -1;
}


Recorder* NewPreview::CreateRecorder(int p_CamId, int p_record_id)
{
	if( !CheckCamIdIsValid(p_CamId) )
		return NULL;

	Recorder *rec = NULL;
	map<int, Recorder*> mRecGroup;

	if( !CheckRecorderExist(p_CamId, p_record_id) )
	{
		if(CheckCameraExist(p_CamId) )
		{
			switch(p_CamId)
			{
				case CAM_A:
				{
						rec = RecorderFactory::GetInstance()->CreateRecorder(REC_1080P30FPS, GetCamera(p_CamId), 0);
						if( rec != NULL )
						{
							mRecGroup.insert(make_pair(0, rec));
							rec->SetID(0);
                            OsdManager::get()->addCamRecordMap(CAM_A, 0,rec);
						}
						rec = RecorderFactory::GetInstance()->CreateRecorder(REC_M_SUB_CHN, GetCamera(p_CamId), 2);
						if( rec != NULL )
						{
							mRecGroup.insert(make_pair(1, rec));
							rec->SetID(1);
                            OsdManager::get()->addCamRecordMap(CAM_A, 1,rec);
                            #ifdef ENABLE_RTSP
                            rec->SetEncodeDataCallback(NewPreview::EncodeDataCallback, this);
                            #endif
						}
						break;
				}
				case CAM_B:
				{			
						rec = RecorderFactory::GetInstance()->CreateRecorder(REC_U_1080P30FPS, GetCamera(p_CamId), 1);
						if( rec != NULL )
						{
							mRecGroup.insert(make_pair(2, rec));
							rec->SetID(2);
                            OsdManager::get()->addCamRecordMap(CAM_B, 2,rec);
						}

						rec = RecorderFactory::GetInstance()->CreateRecorder(REC_B_SUB_CHN, GetCamera(p_CamId), 3);
						if( rec != NULL )
						{
							mRecGroup.insert(make_pair(3, rec));
							rec->SetID(3);
                            OsdManager::get()->addCamRecordMap(CAM_B, 3,rec);
                            #ifdef ENABLE_RTSP
                            rec->SetEncodeDataCallback(NewPreview::EncodeDataCallback, this);
                            #endif
						}
						break;
				}
			}
			if( rec != NULL )
			{
				m_CamRecMap.insert(make_pair(p_CamId, mRecGroup));
				m_MainModule->setRecoderMap(m_CamRecMap);
			}
		}
	}
	else
	{
        db_error("yxl: record is exist no need release");
		rec = GetRecorder(p_CamId, p_record_id);
	}

	return rec;
}

int NewPreview::DestoryRecorder(int p_CamId, int p_record_id)
{

	if(!CheckCamIdIsValid(p_CamId))
		return -1;
	if(CheckRecorderExist(p_CamId, p_record_id))
	{
		CamRecMap::iterator iter = m_CamRecMap.begin();;
		for(iter = m_CamRecMap.begin(); iter != m_CamRecMap.end(); iter++)
		{
			if( iter->first == p_CamId )
            {
			//map<RecorderType, Recorder*> mRecGroup;
			    map<int, Recorder*>::iterator rec_iter;
			    for (rec_iter = m_CamRecMap[p_CamId].begin();rec_iter != m_CamRecMap[p_CamId].end(); rec_iter++)
			    {
				    if( rec_iter->first == p_record_id ){
					
					delete rec_iter->second;
					m_CamRecMap.erase(iter);
					m_MainModule->setRecoderMap(m_CamRecMap);
                    return 0;
				    }
			    }
			}

			
		}
	}
    return 0;
}

int NewPreview::WifiSOftApDisable()
{
    int ret =0;
    ret = NetManager::GetInstance()->DisableSoftap();
    if(ret < 0){
        db_msg("WifiSOftApDisable filed");
        return ret;
    }

	//by hero ****** wifi led not light
    //LedControl::get()->EnableLed(LedControl::WIFI_LED, false);

    return 0;
}

int NewPreview::WifiSOftApEnable()
{
    string ssid, pwd;
    NetManager::GetInstance()->GetWifiInfo(ssid, pwd);
    int ret =0;
    ret = NetManager::GetInstance()->SwitchToSoftAp(ssid.c_str(),pwd.c_str(),0,0,0);
    if(ret < 0)
    {
        db_msg("WifiSOftApEnable filed");
        return ret;
    }
	//by hero ****** wifi led light
    //LedControl::get()->EnableLed(LedControl::WIFI_LED, true);
    return 0;
}



int NewPreview::StartRecord(int p_CamId, int p_record_id)
{
	if(!CheckCamIdIsValid(p_CamId) )
		return -1;

	if(!CheckRecorderExist(p_CamId, p_record_id))
	{
		db_error("recorder not exist p_CamId:%d p_record_id:%d\n",p_CamId,p_record_id);
		return -1;
	}

	Recorder *rec = GetRecorder(p_CamId, p_record_id);

	return rec->StartRecord();
}

int NewPreview::StopRecord(int p_CamId, int p_record_id)
{
	if(!CheckCamIdIsValid(p_CamId) )
		return -1;

	if(!CheckRecorderExist(p_CamId, p_record_id))
		return -1;

	Recorder *rec = GetRecorder(p_CamId, p_record_id);

	return rec->StopRecord();
}

int NewPreview::TakePicture(int p_CamId)
{
	if( !CheckCamIdIsValid(p_CamId) )
		return -1;

	if( !CheckCameraExist(p_CamId) )
		return -1;

	Camera *cam = GetCamera(p_CamId);
    if(p_CamId == 0){
	    return cam->TakePictureEx(0,false);
    }else{
        return cam->TakePictureEx(1,false);
    }
}

int NewPreview::SwitchDisplay()
{
    if(!CheckCameraExist(CAM_A))
        return -1;

    if(!CheckCameraExist(CAM_B))
        return -1;

    ViewInfo cam0_rect, cam1_rect;
    GetCamera(CAM_A)->GetCameraDispRect(cam0_rect);
    GetCamera(CAM_B)->GetCameraDispRect(cam1_rect);

    db_debug("cam0:rect x %d y %d w %d h %d",cam0_rect.x,cam0_rect.y,cam0_rect.w,cam0_rect.h);
    db_debug("cam1:rect x %d y %d w %d h %d",cam1_rect.x,cam1_rect.y,cam1_rect.w,cam1_rect.h);
    MenuConfigLua *mcl=MenuConfigLua::GetInstance();
    int preview_camera = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CAMERA);
    db_error("preview_camera %d",preview_camera);
    if(preview_camera == 1)
    {
        GetCamera(CAM_A)->SetCameraDispRect(cam1_rect, 2);  //显示前摄
        GetCamera(CAM_B)->SetCameraDispRect(cam0_rect, 0);
        preview_camera = 0;
    }else if(preview_camera == 0){
        GetCamera(CAM_A)->SetCameraDispRect(cam1_rect, 0);  //显示后拉
        GetCamera(CAM_B)->SetCameraDispRect(cam0_rect, 2);
        preview_camera = 1;
    }

//	if( cam0_rect.w > cam1_rect.w || cam0_rect.h > cam1_rect.h)
//	{
//		GetCamera(CAM_A)->SetCameraDispRect(cam1_rect, 2);
//		GetCamera(CAM_B)->SetCameraDispRect(cam0_rect, 0);
//	}
//	else
//	{
//		GetCamera(CAM_A)->SetCameraDispRect(cam1_rect, 0);
//		GetCamera(CAM_B)->SetCameraDispRect(cam0_rect, 2);
//	}
    mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CAMERA,preview_camera);

    return 0;
}

int NewPreview::RestoreDisplay()
{
	if(!CheckCameraExist(CAM_A))
		return -1;

	if(m_camBShowFlag){
        if(!CheckCameraExist(CAM_B))
            return -1;
	}

	ViewInfo cam0_rect;
	cam0_rect.x = 0;
	cam0_rect.y = 0;
	cam0_rect.w = SCREEN_WIDTH;
	cam0_rect.h = SCREEN_HEIGHT;
	GetCamera(CAM_A)->SetCameraDispRect(cam0_rect, 0);
    MenuConfigLua *mcl=MenuConfigLua::GetInstance();
    mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CAMERA,0);

	return 0;
}

int NewPreview::SetCamPreviewRect(int p_CamId,int p_x,int p_y,int p_width,int p_height)
{
	if( !CheckCamIdIsValid(p_CamId) )
		return -1;

	if( !CheckCameraExist(p_CamId) )
		return -1;

	ViewInfo p_info;
	p_info.x = p_x;
	p_info.y = p_y;
	p_info.w = p_width;
	p_info.h = p_height;

	if(p_CamId == 0)
		GetCamera(p_CamId)->SetCameraDispRect(p_info, 2);
	else
		GetCamera(p_CamId)->SetCameraDispRect(p_info, 0);

	return 0;
}

int NewPreview::StartPreview(int p_CamId)
{
	if( !CheckCamIdIsValid(p_CamId) )
		return -1;

	if( !CheckCameraExist(p_CamId) )
		return -1;

	Camera *cam = GetCamera(p_CamId);

	cam->StartPreview();

	return 0;
}

int NewPreview::StopPreview(int p_CamId)
{
	if( !CheckCamIdIsValid(p_CamId) )
		return -1;

	if( !CheckCameraExist(p_CamId) )
		return -1;

	Camera *cam = GetCamera(p_CamId);

	cam->StopPreview();

	return 0;
}


int NewPreview::StopCamera(int camId)
{
    if( !CheckCamIdIsValid(camId) )
		return -1;

	if( !CheckCameraExist(camId) )
		return -1;

	Camera *cam = GetCamera(camId);
	cam->DeinitCamera();
   // cam->Close();
	return 0;
}


void NewPreview::DoSystemShutdown()
{
	 //if start record may be stop recod;
	CamRecCtrl *m_CamRecCtrl = CamRecCtrl::GetInstance();
	m_CamRecCtrl->StopAllRecord();
	int status = StorageManager::GetInstance()->GetStorageStatus();
    if((status == UMOUNT) || (status == STORAGE_FS_ERROR) || (status == FORMATTING))
    {
		db_warn("warning :sd card status is wrong be careful\n");
	}else{
		 AW_MPI_ISP_SetSaveCTX(0);
	}
	 // sync record file data to disk
	sync();
	MenuConfigLua *mcl=MenuConfigLua::GetInstance();
	mcl->UpdateSystemTime(true);
}


void NewPreview::LowPowerShutdownTimerHandler(union sigval sigval)
{
    NewPreview *self = reinterpret_cast<NewPreview *>(sigval.sival_ptr);
    if (PowerManager::GetInstance()->getACconnectStatus()) {
        db_info("ac connected, stop shutdown process");
        stop_timer(self->lowpower_shutdown_timer_id_);
        self->lowpower_shutdown_processing_ = false;
        return;
    }

    self->DoSystemShutdown();
    self->lowpower_shutdown_processing_ = false;
}


int NewPreview::HandleGUIMessage(int p_msg,int p_val,int p_CamId)
{
    int enable_osd = 0;
	switch(p_msg)
	{
		case PREVIEW_RECORD_BUTTON:
		{
			if( !CheckCamIdIsValid(p_CamId) )
				break;

			if( !CheckCameraExist(p_CamId) )
				break;
            PreviewWindow *pre_win = static_cast<PreviewWindow *>(win_mg_->GetWindow(WINDOWID_PREVIEW));

            enable_osd = MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_TIMEWATERMARK);
            if(!p_val && m_bOsdEnable)
            {
                    if(enable_osd){
                        OsdManager::get()->DettchVencRegion(0,0);
                        OsdManager::get()->stopTimeOsd(p_CamId,0);
                    }
                    OsdManager::get()->DettchVencRegion(0,5);
                    if(m_camBShowFlag == 1){
                        if(enable_osd){
                            OsdManager::get()->DettchVencRegion(2,2);
                            OsdManager::get()->stopTimeOsd(p_CamId,2);
                        }
                        OsdManager::get()->DettchVencRegion(2,6);
                    }
            }

			if(p_val)
			{
                record_start_process_flag_ = true;
                int ret = -1;
                if(m_workMode == 0){
                    if(!StorageManager::GetInstance()->CheckParkRecordDirFull()){ //停车监控视频已满,不进行缩略图拍摄
                        db_debug("take park video sub pic");
                        cama_park_file_ = new MediaFile(CAM_A, VIDEO_A_PARK);
                        Recorder *rec = GetRecorder(CAM_A, 0);
                        rec->SetParkFile(cama_park_file_);
                        if(m_camBShowFlag == 1){
                            camb_park_file_ = new MediaFile(CAM_B, VIDEO_B_PARK);
                            rec = GetRecorder(CAM_B, 2);
                            rec->SetParkFile(camb_park_file_);
                        }
                        std::thread([&]{
                            TakeParkVideothumb(CAM_A, cama_park_file_);
                        }).detach();
                        std::thread([&]{
                            if(m_camBShowFlag == 1)
                                TakeParkVideothumb(CAM_B, camb_park_file_);
                        }).detach();
                        usleep(200*1000);
                    }
                }
				ret = StartRecord(p_CamId, 0);
                if(ret == 0)
                {
                     if(p_val && m_bOsdEnable)
                    {
                        OsdManager::get()->AttchlogoVencRegion(0,5);
                        db_debug("enable_osd %d",enable_osd);
                        if(enable_osd){
                            OsdManager::get()->AttchVencRegion(0,0);
                            OsdManager::get()->startTimeOsd(CAM_A,0);
                        }

                    }
                    if(m_workMode != 0)
                        TakePicforVideothumb(p_CamId, 0);
                    if(m_camBShowFlag == 1)
                    {
                        Recorder *rec = GetRecorder(CAM_B, 2);
                        if(rec != NULL && !rec->RecorderIsBusy())
                        {
                            ret = rec->StartRecord();
                            if(ret == 0)
                            {
                                if(p_val && m_bOsdEnable)
                                {
                                    OsdManager::get()->AttchlogoVencRegion(2,6);
                                    if(enable_osd){
                                        OsdManager::get()->AttchVencRegion(2,2);
                                        OsdManager::get()->startTimeOsd(CAM_B,2);
                                    }
                                }
                                if(m_workMode != 0)
                                    TakePicforVideothumb(CAM_B, 2);
                            }else if(ret == -2)
                            {
                                db_msg("R event video dir video record files is full");
                                pre_win->ShowPromptBox(PROMPT_BOX_R_EVENT_DIR_FULL, 3);
                                record_start_process_flag_ = false;
                                break;
                            }else if(ret == -3)
                            {
                                db_msg("Park video dir video record files is full");
                                camB_recinit_fail_flag_ = true;
//                                pre_win->ShowPromptBox(PROMPT_BOX_PARK_DIR_FULL, 3);
                                record_start_process_flag_ = false;
                                break;
                            }else{
                                db_msg("Be careful start R video record filed");
                                record_start_process_flag_ = false;
                                break;
                            }
                        }
                    }
                    pre_win->ShowPromptBox(PROMPT_BOX_RECORDING_START,2);
                    this->Notify((MSG_TYPE)MSG_SET_STATUS_PREVIEW_REC_PLAY);
                }else if(ret == -2)
                {
                    db_msg("F event video dir video record files is full");
                    pre_win->ShowPromptBox(PROMPT_BOX_F_EVENT_DIR_FULL, 3);
                    record_start_process_flag_ = false;
                    break;
                }else if(ret == -3)
                {
                    db_msg("F Park video dir video record files is full");
                    pre_win->ShowPromptBox(PROMPT_BOX_PARK_DIR_FULL, 3);
                    if(m_workMode == 0){	//停车监控
                            db_warn("newPreview is reviced MSG_SYSTEM_POWEROFF 11\n");
                            this->Notify(MSG_SHUTDOWN_SYSTEM);
                    }
                    record_start_process_flag_ = false;
                    break;
                }else  if(ret == RECORDER_SD_FULL){
                    db_error("sd is full,stop rec");
                    pre_win->ShowPromptBox(PROMPT_BOX_TF_FULL, 3);
                    record_start_process_flag_ = false;
                    break;
                }else{
                    db_msg("Be careful start F video record filed");
                    record_start_process_flag_ = false;
                    break;
                }
                record_start_process_flag_ = false;
			}
			else
			{
				StopRecord(p_CamId, 0);
                if(m_camBShowFlag == 1){
				StopRecord(CAM_B, 2);
                }
                this->Notify((MSG_TYPE)MSG_SET_STATUS_PREVIEW_REC_PAUSE);
		    }
            if(m_workMode != 0){
                if(StorageManager::GetInstance()->CheckParkRecordDirFull() && WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_PREVIEW){
                    pre_win->ShowPromptInfo(PROMPT_PARK_DIR_FULL, 4);
                }
            }
            if(filelock_flag_ && PowerManager::GetInstance()->getPowenOnType()!=0){
                this->Notify((MSG_TYPE)MSG_RECFILELOCK_ENABLE,0,0);
                filelock_flag_ = false;
            }
			break;
		}
		case PREVIEW_AUDIO_BUTTON:
		{
            db_error("p_val %d",p_val);
            MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
            menuconfiglua->SetMenuIndexConfig(MSG_SET_RECORD_VOLUME,p_val);
            Recorder *rec_M = GetRecorder(CAM_A, 0);
            if(rec_M != NULL)
            {
                rec_M->SetRecordAudioOnOff(p_val);
                rec_M->SetMute(!p_val);
            }
            StatusBarWindow *status_bar = static_cast<StatusBarWindow *>(win_mg_->GetWindow(WINDOWID_STATUSBAR));
            status_bar->ShowVoiceIcon(true);
		}
        break;
		case PREVIEW_GO_PLAYBACK_BUTTON:
#ifdef DEINIT_CAMERA
		    if(!deinit_flag_)
		        deinit_flag_ = true;
#endif
        break;
		case PREVIEW_TO_SETTINGWINDOW_UPDATE_VERSION:
			this->Notify((MSG_TYPE)MSG_PREVIEW_TO_SETTINGWINDOW_UPDATE_VERSION);
			break;
		case PREVIEW_TO_SETTINGWINDOW_UPDATE_4G_VERSION:
			this->Notify((MSG_TYPE)MSG_PREVIEW_TO_SETTINGWINDOW_UPDATE_4G_VERSION);
			break;
		case PREVIEW_TO_SETTING_BUTTON:
			//change status bar bottom button
			this->Notify((MSG_TYPE)MSG_PREVIW_TO_SETTING_CHANGE_STATUS_BAR_BOTTOM);
			// change status_bar icon status 
			this->Notify((MSG_TYPE)MSG_PREVIW_TO_SETTING_CHANGE_STATUS_BAR);
            break;
        case PREVIEW_TO_SETTING_NEW_WINDOW:
            this->Notify((MSG_TYPE)MSG_PREVIEW_TO_NEWSETTING_WINDOW);
            break;
        case PREVIEW_SWITCH_LAYER:
#ifdef CAMB_PREVIEW
            if(m_camBShowFlag)
                SwitchDisplay();
#endif
#if 0
            PartitionManager::GetInstance()->sunxi_spinor_private_sec_set(FKEY_BINDFLAG, "false");
            PartitionManager::GetInstance()->sunxi_spinor_private_set_flag(0);
#endif
            break;
        case PREVIEW_VIEW_UP:
        case PREVIEW_VIEW_DOWN:
        {
            MenuConfigLua *mcl=MenuConfigLua::GetInstance();
            int preview_camera = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CAMERA);
            int preview_crop,move_count,move_distance,x,y,w,h = 0;
            if(preview_camera == 0){
                preview_crop = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CROP0);
                move_count = mcl->GetMenuIndexCountConfig(SETTING_PREVIEW_CROP0_COUNT);
                CameraInitParam param;
                GetCamera(CAM_A)->GetCameraInitParam(param);
#if 0
                if(p_msg == PREVIEW_VIEW_UP){
                    if(preview_crop >= move_count-1){
                        break;
                    }
                    preview_crop += 1;
                }else if(p_msg == PREVIEW_VIEW_DOWN){
                    if(preview_crop <= 0){
                        break;
                    }
                    preview_crop -= 1;
                }else{
                    break;
                }
                move_distance = PREVIEW_MOVE_DISTANCE;
#endif
#if 0
                y = (preview_crop+1)*(move_distance);
                w = param.sub_venc_size_.Width;
                h = param.sub_venc_size_.Height - move_count*move_distance;
                if(y+h > param.sub_venc_size_.Height){
                   db_error("why y+SCREEN_HEIGHT(%d) > param.sub_venc_size_.Height(%d) ???",y+h,param.sub_venc_size_.Height);
                   break;
                }
#endif
                x = 0;
//                y = preview_crop * move_distance;
                if(p_msg == PREVIEW_VIEW_DOWN){
                    preview_crop -= 40;
                    y = preview_crop;
                    if(y <= 0){
                        preview_crop = 0;
                        y = 0;
                    }
                }else if(p_msg == PREVIEW_VIEW_UP){
                    preview_crop += 40;
                    y = preview_crop;
                    if(y >= 400){
                        preview_crop = 400;
                        y = 400;
                    }
                }
                w = SCREEN_HEIGHT;
                h = SCREEN_WIDTH;
                if(y+h > param.sub_venc_size_.Height){
                    db_error("why preview_crop %d y+h(%d) > param.sub_venc_size_.Height(%d) ???",preview_crop,y+h,param.sub_venc_size_.Height);
                    break;
                }
                GetCamera(CAM_A)->SetCameraUserRegion(x,y,w,h);
                db_debug("PREVIEW_VIEW_DOWN  camera %d ,[X:%d,Y:%d,W:%d,H:%d] , move_distance:%d"
                    ,preview_camera,x,y,w,h,move_distance);
                mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CROP0,preview_crop);
            }else if(preview_camera == 1){
                if(m_camBShowFlag == 1){
                    preview_crop = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CROP1);
                    move_count = mcl->GetMenuIndexCountConfig(SETTING_PREVIEW_CROP1_COUNT);
                    CameraInitParam param;
                    GetCamera(CAM_B)->GetCameraInitParam(param);
#if 0
                    if(p_msg == PREVIEW_VIEW_UP){
                        if(preview_crop >= move_count-1){
                            break;
                        }
                        preview_crop += 1;
                    }else if(p_msg == PREVIEW_VIEW_DOWN){
                        if(preview_crop <= 0){
                            break;
                        }
                        preview_crop -= 1;
                    }else{
                        break;
                    }
                    move_distance = PREVIEW_MOVE_DISTANCE;
#endif
#if 0
                    y = (preview_crop+1)*(move_distance);
                    w = param.sub_venc_size_.Width;
                    h = param.sub_venc_size_.Height - move_count*move_distance;;
                    if(y+h > param.sub_venc_size_.Height){
                        db_error("why y+SCREEN_HEIGHT(%d) > param.sub_venc_size_.Height(%d) ???",y+h,param.sub_venc_size_.Height);
                        break;
                    }
#endif
                    x = 0;
//                    y = preview_crop * move_distance;
                    if(p_msg == PREVIEW_VIEW_DOWN){
                        preview_crop -= 40;
                        y = preview_crop;
                        if(y <= 0){
                            preview_crop = 0;
                            y = 0;
                        }
                    }else if(p_msg == PREVIEW_VIEW_UP){
                        preview_crop += 40;
                        y = preview_crop;
                        if(y >= 400){
                            preview_crop = 400;
                            y = 400;
                        }
                    }
                    w = SCREEN_HEIGHT;
                    h = SCREEN_WIDTH;
                    if(y+h > param.sub_venc_size_.Height){
                        db_error("why preview_crop %d y+h(%d) > param.sub_venc_size_.Height(%d) ???",preview_crop,y+h,param.sub_venc_size_.Height);
                        break;
                    }
                    GetCamera(CAM_B)->SetCameraUserRegion(x,y,w,h);
                    db_debug("PREVIEW_VIEW_DOWN  camera %d ,[X:%d,Y:%d,W:%d,H:%d] , move_distance:%d"
                        ,preview_camera,x,y,w,h,move_distance);
                    mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CROP1,preview_crop);
                }
            }else{
                db_error("why preview camer is %d",preview_camera);
            }
        }
            break;
        case PREVIEW_TAKE_PIC_CONTROL:
        {
            db_warn("[debug_jaosn]: take pic happen");
            TakePicture(0);
            if(m_camBShowFlag == 1)
            {
                TakePicture(1);
            }
        }
            break;
        case PREVIEW_BUTTON_DIALOG_HIDE:
			{
          	  PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
          	 	 db_msg("[debug_zhb]--->PREVIEW_BUTTON_DIALOG_HIDE");
			  pw->HandleButtonDialogMsg(p_val);
			}
           	 break;
		case PREVIEW_SET_RECORD_MUTE:
			GetRecorder(p_CamId, 0)->SetMute(p_val);
			break;
		case PREVIEW_LOWPOWER_SHUTDOWN:
            {
                if (!(this->lowpower_shutdown_processing_)) {
                    this->lowpower_shutdown_processing_ = true;
                    create_timer(this, &(this->lowpower_shutdown_timer_id_), LowPowerShutdownTimerHandler);
                    set_one_shot_timer(5, 0, this->lowpower_shutdown_timer_id_);
                }
            }
            break;
		case PREVIEW_WIFI_SWITCH_BUTTON:{
            int ret = 0;
            if(!p_val){
#ifdef ENABLE_RTSP
                db_warn("wifi disabled, we will stop all record");
                Recorder *rec = GetRecorder(CAM_A, 1);
                if(rec != NULL)
                   rec->StopRecord();
                if(m_camBShowFlag == 1){
                    rec = GetRecorder(CAM_B, 3);
                    if(rec != NULL)
                        rec->StopRecord();
                }
#endif
                ret = WifiSOftApDisable();
                if(ret < 0){
                   db_error("[error]:WifiSOftApDisable filed");
                }

#ifdef ENABLE_RTSP
                DestroyRtspServer();
#endif
            }else{
                ret = WifiSOftApEnable();
                if(ret < 0)
                {
                    //may be close the wifi info tip
                    db_error("[error]:wifi enabled filed");
                    break;
                }
                db_msg("StartRecord is over");
            }
          }
            break;
        case PREVIEW_EMAGRE_RECORD_CONTROL:
            {
                HandleSosRecord(p_val);
                if(filelock_flag_){
                  this->Notify((MSG_TYPE)MSG_RECFILELOCK_ENABLE,0,0);
                  filelock_flag_ = false;
                }else{
                  filelock_flag_ = true;
                  this->Notify((MSG_TYPE)MSG_RECFILELOCK_ENABLE,0,1);
                }
            }
            break;
#ifdef USB_MODE_WINDOW
        case USB_CHARGING:
        {
            db_warn("habo---> USB_CHARGING ---");

            mode_ = USB_MODE_CHARGE;
            m_usb_attach_status = false;
            Notify((MSG_TYPE)MSG_USB_CHARGING);
            
        }
            break;
        case USB_MASS_STORAGE:
        {
            db_warn("habo---> USB_MASS_STORAGE ---");
            mode_ = USB_MODE_MASS_STORAGE;
            if(isRecordStart)
            {
                PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
                pw->VideoStopRecordCtl();
            }
            StorageManager *sm = StorageManager::GetInstance();
            int status = sm->GetStorageStatus();
            if(status == MOUNTED){
                Notify((MSG_TYPE)MSG_USB_MASS_STORAGE);
                StorageManager::GetInstance()->MountToPC();
            }else{
                db_error("sd is remove,can not mount to pc");
//                Notify((MSG_TYPE)MSG_USB_MASS_STORAGE_SD_REMOVE);
            }
        }
            break;
#endif
        case MSG_SYSTEM_SHUTDOWN:
        {
            db_warn("newpreview receive shutdown");
            DoSystemShutdown();
//            MediaDeInit();
            this->Notify((MSG_TYPE)SHOW_SHUTDOWN_LOGO);
            db_warn("media deinit end");
            this->Notify(MSG_SHUTDOWN_SYSTEM);
            break;
        }
        case MSG_STREAM_RECORD_SWITCH:{
            switch(p_val)
            {
               #if 0
                case 0:{
                    db_msg("STREAM_RECORD_SWITCH is %d",p_val);            
                    Recorder *rec = GetRecorder(0, 1);
                    if(rec!= NULL && rec->RecorderIsBusy())
                    {
                        rec->StopRecord();
                    }
                }
                    break;
                case 1:{
                    Recorder *rec1 = GetRecorder(0, 1);
                    if(rec1 != NULL && (!rec1->RecorderIsBusy()))
                    {
                        rec1->StartRecord();
                    }
                }
                    break;
                #endif
            }
        }        
        break;
        case PREVIEW_ADAS_ONOFF:
        {
#ifdef ENABLE_ADAS
            db_error("p_val %d",p_val);
            MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
            menuconfiglua->SetMenuIndexConfig(MSG_SET_ADAS_SWITCH,p_val);
            if(p_val){
                Camera *cam = GetCamera(CAM_A);
                if(cam != NULL){
                    db_error("start adas");
                    cam->StartADAS();
                }
            }else{
                Camera *cam = GetCamera(CAM_A);
                if(cam != NULL){
                    db_error("stop adas");
                    cam->StopADAS();
                }
            }
#endif
        }
        break;
        default:

        break;
	}

	return 0;
}

int NewPreview::HandleSosRecord(int val)
{
   PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
   Recorder *rec = GetRecorder(0, 0);
   #ifdef USE_CAMB
   Recorder *rec1 = GetRecorder(1, 2);
    if(rec != NULL || rec1 != NULL)
   #else
   if(rec != NULL)
   #endif
    {
        if(!m_sosRecordisStart)
        {
            m_sosRecordisStart = true;
            rec->setLockFileFlag(true);
#ifdef USE_CAMB
        if(m_camBShowFlag){
            if(rec1 != NULL)
                rec1->setLockFileFlag(true);
        }
#endif
            //show lock current video file ui
            pw->showLockFileUiInfo(1);
        }else if(m_sosRecordisStart && (m_Impact_happen_flag == false))
        {
            m_sosRecordisStart = false;
            rec->setLockFileFlag(false);
#ifdef USE_CAMB
            if(m_camBShowFlag){
                if(rec1 != NULL)
                    rec1->setLockFileFlag(false);
            }
#endif
            //show unlock and current file ui
            pw->showLockFileUiInfo(0);
        }else if(m_sosRecordisStart && m_Impact_happen_flag){
            db_warn("current file is locked by gsensor impact can't unlock file");
            pw->showLockFileUiInfo(3);
        }
    }
    return 0;
}

void NewPreview::Update(MSG_TYPE p_msg, int p_CamID, int p_recordId)
{
	db_debug("NewPreview recive msg %d",p_msg);
	switch(p_msg)
	{
		case MSG_CAMERA_ENABLE_OSD:
			m_bOsdEnable = true;
			break;
		case MSG_CAMERA_DISABLE_OSD:
			m_bOsdEnable = false;
			break;
		case MSG_RECORD_START:
            if(!isRecordStart)
            {
                isRecordStart = true;
			    Notify(p_msg);
#if 0
                if(m_httpServer_ != NULL){
                m_httpServer_->sendCommdToServer(MSG_RECORD_START,1,"");
                }
#endif
            }
			break;
       case MSG_RECORD_STOP:
           if (isRecordStart) 
            {
                isRecordStart = false;
                Notify(p_msg);
#if 0
                if(m_httpServer_ != NULL){
                m_httpServer_->sendCommdToServer(MSG_RECORD_STOP,0,"");
                }
#endif
            }
			break;
		case MSG_CLOSE_STANDBY_DIALOG:
			{
				PowerManager *pm = PowerManager::GetInstance();
				PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
				if(pw->GetPromptPoint()->GetStandbyFlagStatus()) {
					pm->setStandbyFlag(false);
					Notify(p_msg);
					db_error("send MSG_CLOSE_STANDBY_DIALOG msg");
				}
				break;	
			}
		case MSG_ACCON_HAPPEN:
		{
		    if(PowerManager::GetInstance()->GetEnterPowerOffFlag()){ //即将进入关机,不响应vbus接入的消息
		        db_error("cdr will power off,ignore msg");
		        return;
		    }
            if(!WindowManager::GetInstance()->GetIgnorPowerOffMsgFlag()){
                WindowManager::GetInstance()->SetIgnorPowerOffMsgFlag(true);
                std::thread([&]{//过滤插入电源2s内的误报的power key事件，既插入电源后2s power key事件都不响应
                    sleep(2);
                    WindowManager::GetInstance()->SetIgnorPowerOffMsgFlag(false);
                }).detach();
            }
            WindowManager::GetInstance()->SetAccOnFlag(true);
            PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
            pw->SetAccon_Record_Flag(true);
            if(m_workMode != 0){
                PowerManager *pm = PowerManager::GetInstance();
                pm->setStandbyFlag(false);
                if(isRecordStart){
                    db_msg("recording now ,ignor MSG_ACCON_HAPPEN recorde");
                    pw->SetAccon_Record_Flag(false);
                    pw->Update(MSG_ACCON_HAPPEN);
                    break;
                }
                db_msg("NewPreview send MSG_ACCON_HAPPEN start");
                Notify(p_msg);
                pw->SetAccon_Record_Flag(false);
                db_msg("NewPreview send MSG_ACCON_HAPPEN end");
            }
            if(m_workMode == 0 && StorageManager::GetInstance()->GetStorageStatus() == UMOUNT){
                DoSystemShutdown();
                this->Notify(MSG_SHUTDOWN_SYSTEM);
            }
            break;
		}
        case MSG_ACCOFF_HAPPEN:
	   	{
            if(PowerManager::GetInstance()->GetEnterPowerOffFlag()){
                db_error("cdr will power off,ignore msg");
                return;
            }
            db_msg("NewPreview receive MSG_ACCOFF_HAPPEN");
            WindowManager::GetInstance()->SetAccOnFlag(false);
            if(m_workMode == 0){
                if(StorageManager::GetInstance()->CheckStorageIsOk() == false)
                {
                    //should poweroff tfcard is not ready
                    db_warn("Be careful should power off tf card is not ready\n");
                    PowerManager *pm = PowerManager::GetInstance(); 
                    pm->setStandbyFlag(true);
			        Notify(p_msg);
                }else{
                    db_warn("do nothing current wait park record finished \n");
                }
            }else{
                PowerManager *pm = PowerManager::GetInstance();
                pm->setStandbyFlag(true);
                //pm->ScreenOff();
			    Notify(p_msg);
                if(m_usb_attach_status && mode_ == USB_MODE_MASS_STORAGE)
                {
                    db_msg("acc deconnect ,but usb mode is mass mode ,need to chang mode");
                    this->Notify(MSG_USB_HOST_DETACHED);
                    m_usb_attach_status = false;
                    StorageManager::GetInstance()->UMountFromPC();
                    mode_ = USB_MODE_CHARGE;
                }
			    db_msg("NewPreview send MSG_ACCOFF_HAPPEN end");    
            }
			break;
       	}
        case MSG_STORAGE_MOUNTED:
		{
			Notify(p_msg);
#if 0
            if(m_httpServer_ != NULL)
                m_httpServer_->sendCommdToServer(MSG_STORAGE_MOUNTED,1,"");
#endif
            break;
        }
        case MSG_STORAGE_UMOUNT: 
#if 0
            if(m_httpServer_ != NULL)
                m_httpServer_->sendCommdToServer(MSG_STORAGE_UMOUNT,0,"");            
#endif
            if(mode_ == USB_MODE_MASS_STORAGE){
                mode_ = USB_MODE_CHARGE;
            }
            Notify(p_msg);
        break;
        case MSG_STORAGE_IS_FULL:
            db_error("sd is full,will stop rec");
            Notify(p_msg);
        break;
	   case MSG_PREPARE_TO_SUSPEND:
			Notify(p_msg);
	   		break;
	  case MSG_START_RESUME:
			Notify(p_msg);
			//usleep(200*1000);
			//MediaInit();
			break;
	   case MSG_TAKE_THUMB_VIDEO:
            take_thumb_flag_ = true;//用于关机前拍子图标记
	   		TakePicforVideothumb(p_CamID, p_recordId);
	   		break;
       case MSG_SUB_REC0RD_FILE_DONE:
       {
            if(m_workMode == 0){
                db_warn("newPreview is reviced MSG_SUB_REC0RD_FILE_DONE\n");
                DoSystemShutdown();
                this->Notify(MSG_SHUTDOWN_SYSTEM);
                break;
            }
       }
       break;
	   case MSG_RECORD_FILE_DONE:{
            db_warn("m_camBShowFlag %d camB_recinit_fail_flag_ %d",
                    m_camBShowFlag,camB_recinit_fail_flag_);
            if(m_workMode == 0){
                if(!m_camBShowFlag){
                db_warn("newPreview is reviced MSG_SYSTEM_SHUTDOWN\n");
                DoSystemShutdown();
                this->Notify(MSG_SHUTDOWN_SYSTEM);
                break;
                }else if(m_camBShowFlag && camB_recinit_fail_flag_){  //后拉初始化录像失败,进入关机同时删除后拉停车监控视频缩略图
                    db_warn("camB can not rec,will shutdown\n");
                    if(camb_park_file_ != NULL){
                        db_error("camB can not rec,remove sub pic %s.",
                                camb_park_file_->GetVideoThumbPicFileName().c_str());
                        StorageManager::GetInstance()->RemoveFile(camb_park_file_->GetVideoThumbPicFileName().c_str());
                    }
                    DoSystemShutdown();
                    this->Notify(MSG_SHUTDOWN_SYSTEM);
                    break;
                }
            }
            
            if(m_sosRecordisStart)
            {
               db_warn("MSG_RECORD_FILE_DONE should be reset m_sosRecordisStart false");
               m_sosRecordisStart = false;
               filelock_flag_ = false;
               this->Notify((MSG_TYPE)MSG_RECFILELOCK_ENABLE,0,0);
            }
            if(m_Impact_happen_flag){
                db_warn("MSG_RECORD_FILE_DONE should be reset m_Impact_happen_flag false");
                m_Impact_happen_flag = false;
            }
            //add for motion detect fuction
            Recorder *rec1 = GetRecorder(0, 0);
            if(rec1 != NULL)
                rec1->setRecordMotionFlag(false);
            Notify(p_msg);
	   }
            break;
	   case MSG_STORAGE_CAP_NO_SUPPORT:
			Notify(p_msg);
			break;
	   case MSG_STORAGE_FS_ERROR:
            this->Notify(p_msg);
            break;
	 //  case MSG_BATTERY_FULL:
       case MSG_BATTERY_LOW:
            this->Notify((MSG_TYPE)p_msg);
            break;
	   case MSG_SOFTAP_DISABLED:
            this->Notify((MSG_TYPE)p_msg);
            break;
	   case MSG_UNBIND_SUCCESS:
	   		db_warn("[debug_jaosn]:this is MSG_UNBIND_SUCCESS");
	   		this->Notify(p_msg);
	   		break;
	   case MSG_DELETE_VIDEOFILE:
	   		db_warn("[debug_jaosn]:this is MSG_DELETE_VIDEOFILE");
	   		this->Notify(p_msg);
            break;
        case MSG_ADAS_START:
#ifdef ENABLE_ADAS
            set_period_timer(1,0,adas_event_timer_);
            this->Notify(p_msg, p_CamID);
#endif
        break;
        case MSG_ADAS_STOP:
#ifdef ENABLE_ADAS
            stop_timer(adas_event_timer_);
            this->Notify(p_msg, p_CamID);
#endif
        break;
        case MSG_ADAS_OPEN_CALIBRATION:
#ifdef ENABLE_ADAS
            this->Notify(p_msg, p_CamID);
#endif
       break;
       case MSG_ADAS_CLOSE_CALIBRATION:
#ifdef ENABLE_ADAS
            this->Notify(p_msg, p_CamID);
#endif
       break;
		case MSG_IMPACT_HAPPEN:
		{
          db_warn("NewPreview recive MSG_IMPACT_HAPPEN\n");
          PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
            if(pw->GetIsRecordStartFlag()){
               m_Impact_happen_flag = true;
               this->HandleSosRecord(0);
               filelock_flag_ = true;
               this->Notify((MSG_TYPE)MSG_RECFILELOCK_ENABLE,0,1);
            }else{
               pw->showLockFileUiInfo(2);
            }
        }
		break;
         case MSG_SYSTEM_POWEROFF:
        {
           db_warn("newPreview is reviced MSG_SYSTEM_POWEROFF start");
           GsensorManager::GetInstance()->writeGsensorOdrAxisValue(3);
           GsensorManager::GetInstance()->writeGsensorPowerModeValue(0x15);
            if(record_start_process_flag_){
                db_warn("wait 2s for starting record end");
                sleep(2);
            }
            int enable_osd = MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_TIMEWATERMARK);
            if(enable_osd && m_bOsdEnable){
                OsdManager::get()->DettchVencRegion(0,0);
                OsdManager::get()->stopTimeOsd(CAM_A,0);
            }
            OsdManager::get()->DettchVencRegion(0,5);
            if(m_camBShowFlag == 1){
                if(enable_osd && m_bOsdEnable){
                    OsdManager::get()->DettchVencRegion(2,2);
                    OsdManager::get()->stopTimeOsd(CAM_B,2);
                }
                OsdManager::get()->DettchVencRegion(2,6);
            }
            DoSystemShutdown();
            if(take_thumb_flag_)
            {
                db_debug("[wlw] take tumb pic now ,waite 1s");
                usleep(1000*1000);
            }
            MediaDeInit();
            this->Notify(MSG_SHUTDOWN_SYSTEM);
            db_warn("newPreview is reviced MSG_SYSTEM_POWEROFF end");
        }
            break;
        case MSG_CAMERA_TAKEPICTURE_ERROR:
        case MSG_CAMERA_TAKEPICTURE_FINISHED: 
	        this->Notify(p_msg);
            take_thumb_flag_ = false;
            break;
#ifdef USB_MODE_WINDOW
         case MSG_USB_HOST_CONNECTED:
         {
            if(PowerManager::GetInstance()->GetEnterPowerOffFlag()){
                db_error("cdr will power off,ignore msg");
                return;
            }
            db_warn("habo--->new preiview MSG_USB_HOST_CONNECTED");
            if(!WindowManager::GetInstance()->GetIgnorPowerOffMsgFlag()){
                WindowManager::GetInstance()->SetIgnorPowerOffMsgFlag(true);
                std::thread([&]{//过滤插入电源2s内的误报的power key事件，既插入电源后2s power key事件都不响应
                    sleep(2);
                    WindowManager::GetInstance()->SetIgnorPowerOffMsgFlag(false);
                }).detach();
            }
            if(isRecordStart)
            {
                db_debug("current is take recording !!!");
                //break;
            }
            StorageManager *sm = StorageManager::GetInstance();
            // update storage status
            int status = sm->GetStorageStatus();
            db_msg("sd status is %d",status);
            if(m_usb_attach_status == false && PowerManager::GetInstance()->getUsbconnectStatus() && status == MOUNTED
                && WindowManager::GetInstance()->GetCurrentWinID() != WINDOWID_PLAYBACK)
            {
                this->Notify(p_msg);
                m_usb_attach_status= true;
            }
            else
            {
                db_msg("invalid usb connect message, attach_status[%d], UsbconnectStatus[%d]",m_usb_attach_status,PowerManager::GetInstance()->getUsbconnectStatus());
            }
         }
            break;
        case MSG_USB_HOST_DETACHED:
            if(PowerManager::GetInstance()->GetEnterPowerOffFlag()){
                db_error("cdr will power off,ignore msg");
                return;
            }
            db_warn("habo---> MSG_USB_HOST_DETACHED  usb = %d  acc= %d",!PowerManager::GetInstance()->getUsbconnectStatus(),!PowerManager::GetInstance()->getACconnectStatus());
            if(m_usb_attach_status && !PowerManager::GetInstance()->getUsbconnectStatus() && !PowerManager::GetInstance()->getACconnectStatus())
            {
                db_warn("habo--->1111 MSG_USB_HOST_DETACHED");
                this->Notify(p_msg);
                m_usb_attach_status = false;
                if(mode_ == USB_MODE_MASS_STORAGE)
                    StorageManager::GetInstance()->UMountFromPC();
                if (mode_ == USB_MODE_CHARGE)
                    PowerManager::GetInstance()->ResetUDC();
                mode_ = USB_MODE_CHARGE;
            }
            else
            {
                db_msg("invalid usb disconnect message, attach_status[%d], UsbconnectStatus[%d] ACconnectStatus[%d]",m_usb_attach_status,PowerManager::GetInstance()->getUsbconnectStatus(),PowerManager::GetInstance()->getACconnectStatus());
            }
            
            break;
#endif
#ifdef USE_CAMB
        case MSG_AHD_CONNECT:
       {
           if(PowerManager::GetInstance()->GetEnterPowerOffFlag()){
               db_error("cdr will power off,ignore msg");
               return;
           }
           Camera* cam = NULL;
           int ret = -1;
           PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
#ifdef DEINIT_CAMERA
           db_warn("[debug_jaosn]: newPreview recived MSG_AHD_CONNECT\n");
           if(WindowManager::GetInstance()->GetCurrentWinID() != WINDOWID_PLAYBACK) //回放界面下不响应后拉初始化
           {
               if(!m_camBShowFlag){
                   cam = CreateCamera(CAM_B);
                   CreateRecorder(CAM_B, 2);
                   Recorder *rec = GetRecorder(0, 0);
                   Recorder *rec1 = GetRecorder(1, 2);
                   if(cam != NULL){
                       db_error("set camera B order 2");
//                       cam->SetDispZorder(2);
                       SwitchDisplay();
                   }else {
                       db_error("creat cam B failed!!");
                       m_camBShowFlag = 1;
                       break;
                   }
                   if(cam->GetCameraStatus() == CAM_DEINITED)
                   {
                        db_error("yxl: back camera is deinited should init again");
                        cam->initBackCamera();
                   }
                   StartPreview(CAM_B);
                   cam->SetCameraPreviewRegionSize(CAM_B);
                   if(rec != NULL){
                       if(rec->RecorderIsBusy())
                       {
                            if((rec1 != NULL) && (!rec1->GetRecordStartFlag()))
                            {
                                ret = rec1->StartRecord();
                                if(ret == 0){
                                    TakePicforVideothumb(CAM_B, 2);
                                    OsdManager::get()->AttchlogoVencRegion(2,6);
                                    OsdManager::get()->AttchVencRegion(2,2);
                                    OsdManager::get()->startTimeOsd(CAM_B,2);
                                }else if(ret == -2)
                                {
                                    db_msg("R event video dir video record files is full");
                                    pw->ShowPromptBox(PROMPT_BOX_R_EVENT_DIR_FULL, 3);
                                }else if(ret == -3)
                                {
                                    db_msg("Park video dir video record files is full");
                                    camB_recinit_fail_flag_ = true;
                                    pw->ShowPromptBox(PROMPT_BOX_PARK_DIR_FULL, 3);
                                }else{
                                    db_msg("Be careful start R video record filed");
                                }
                            }
                       }
                   }
                   m_camBShowFlag = 1;
               }
           }else if(WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_PLAYBACK){
               if(!m_camBShowFlag)
                  m_camBShowFlag = 1;
           }
#else
            db_warn("[debug_jaosn]: newPreview recived MSG_AHD_REMOVE\n");
            if(!m_camBShowFlag){
                cam = CreateCamera(CAM_B);
                CreateRecorder(CAM_B, 2);
                Recorder *rec = GetRecorder(0, 0);
                Recorder *rec1 = GetRecorder(1, 2);
                if(cam){
                    db_error("set camera B order 2");
                    cam->SetDispZorder(2);
                }
                StartPreview(CAM_B);
                cam->SetCameraPreviewRegionSize(CAM_B);
                if(rec != NULL){
                    if(rec->RecorderIsBusy())
                    {
                       ret = rec1->StartRecord();
                         if(ret == 0){
                             TakePicforVideothumb(CAM_B, 2);
                             OsdManager::get()->AttchlogoVencRegion(2,6);
                             OsdManager::get()->AttchVencRegion(2,2);
                             OsdManager::get()->startTimeOsd(CAM_B,2);
                         }else if(ret == -2)
                         {
                             db_msg("R event video dir video record files is full");
                             pw->ShowPromptBox(PROMPT_BOX_R_EVENT_DIR_FULL, 3);
                         }else if(ret == -3)
                         {
                             db_msg("Park video dir video record files is full");
                             pw->ShowPromptBox(PROMPT_BOX_PARK_DIR_FULL, 3);
                         }else{
                             db_msg("Be careful start R video record filed");
                         }
                    }
                }
                m_camBShowFlag = 1;
            }
#endif
            MenuConfigLua *mcl=MenuConfigLua::GetInstance();
            mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CAMERA,1);
       }
       break;
       case MSG_AHD_REMOVE:
       {
           if(PowerManager::GetInstance()->GetEnterPowerOffFlag()){
               db_error("cdr will power off,ignore msg");
               return;
           }
#ifdef DEINIT_CAMERA
           if(WindowManager::GetInstance()->GetCurrentWinID() != WINDOWID_PLAYBACK) //回放界面下不响应后拉初始化
           {
               db_warn("[debug_jaosn]: newPreview recived MSG_AHD_REMOVE\n");
                if(m_camBShowFlag){
                    m_camBShowFlag = 0;
                    RestoreDisplay();
                    //stop record
                    Recorder *rec1 = GetRecorder(1, 2);
                    if(rec1 != NULL){
                        if(rec1->RecorderIsBusy()){
                           Camera* cam = GetCamera(CAM_B);
                            while(!cam->GetPhotoFlag())
                            {
                                db_error("yxl: wait take pic is finish");
                                usleep(100*1000);
                            }
                            OsdManager::get()->DettchVencRegion(2,6);
                            OsdManager::get()->DettchVencRegion(2,2);
                            OsdManager::get()->stopTimeOsd(CAM_B,2);
                            rec1->StopRecord();
                        }
                    }
                    //DestoryRecorder(CAM_B,2);
                    StopCamera(CAM_B);
                    //DestoryCamera(CAM_B);
                }
           }else if(WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_PLAYBACK){
               StopCamera(CAM_B);
               if(m_camBShowFlag)
                   m_camBShowFlag = 0;
           }
#else
           db_warn("[debug_jaosn]: newPreview recived MSG_AHD_REMOVE\n");
           if(m_camBShowFlag){
               m_camBShowFlag = 0;
               //stop record
               Recorder *rec1 = GetRecorder(1, 2);
               if(rec1 != NULL){
                   if(rec1->RecorderIsBusy()){
                       OsdManager::get()->DettchVencRegion(2,6);
                       OsdManager::get()->DettchVencRegion(2,2);
                       OsdManager::get()->stopTimeOsd(CAM_B,2);
                       rec1->StopRecord();
                   }
               }
               StopCamera(CAM_B);
               DestoryRecorder(CAM_B,2);
               DestoryCamera(CAM_B);
           }
           RestoreDisplay();
#endif
           this->Notify(p_msg);
       }
       break;
#endif
#if 0
       case MSG_CAMERA_MOTION_HAPPEN:{
        //1.set record mode to pack mode
        Recorder *rec1 = GetRecorder(0, 0);
        if(rec1 != NULL)
            rec1->setRecordMotionFlag(true);
            this->Notify(p_msg);
       }
        break;
#endif
       case MSG_SOFTAP_ENABLED:
       {
            db_error("[debug_jaosn]:MSG_SOFTAP_ENABLED\n");
           // sleep(5);
#ifdef ENABLE_RTSP
            Recorder *rec = NULL;
            rec = GetRecorder(CAM_A, 1);
            if(rec != NULL)
                rec->StartRecord();
#ifdef USE_CAMB
            if(m_camBShowFlag == 1){
                rec = GetRecorder(CAM_B, 3);
                if(rec != NULL)
                    rec->StartRecord();
            }
#endif
            //create rtsp server
            CreateRtspServer();
            //start rtspServer
            RtspServerStart();
            CreateRtspStreamSender();
#endif
       }
       break;
        case MSG_APP_IS_CONNECTED:
        {
             db_warn("MSG_APP_IS_CONNECTED msg");
             Notify(p_msg);
        }
        break;
        case MSG_APP_IS_DISCONNECTED:
        {
            db_warn("MSG_APP_IS_DISCONNECTED msg");
            Notify(p_msg);
        }
        break;
        case MSG_VOICE_CTRL_TURNON_SCREEN:
        case MSG_VOICE_CTRL_TURNOFF_SCREEN:
        {
            db_error("receive MSG_VOICE_CTRL_TURNON_SCREEN or MSG_VOICE_CTRL_TURNOFF_SCREEN msg");
            Notify(p_msg);
        }
        break;
        case MSG_VOICE_CTRL_FRONT_PREVIEW:
        {
            if(WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_PREVIEW){
                MenuConfigLua *mcl=MenuConfigLua::GetInstance();
                int preview_camera = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CAMERA);
                if(preview_camera == 1)
                    SwitchDisplay();
            }
        }
        break;
        case MSG_VOICE_CTRL_REAR_PREVIEW:
        {
            if(WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_PREVIEW){
                MenuConfigLua *mcl=MenuConfigLua::GetInstance();
                int preview_camera = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CAMERA);
                if(preview_camera == 0)
                    SwitchDisplay();
            }
        }
        break;
        case MSG_VOICE_CTRL_LOCK_FILE:
        {
            db_error("lock file %d",filelock_flag_);
            if(!filelock_flag_){
                if(WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_PREVIEW){
                    db_error("receive msg %d",p_msg);
                    Notify(p_msg);
                }
            }
        }
        break;
        case MSG_VOICE_CTRL_TAKE_PIC:
        case MSG_VOICE_CTRL_TURNON_RECORDERAUDIO:
        case MSG_VOICE_CTRL_TURNOFF_RECORDERAUDIO:
        {
            if(WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_PREVIEW){
                db_error("receive msg %d",p_msg);
                Notify(p_msg);
            }
        }
        break;
        case MSG_DATABASE_UPDATE_FINISHED:
        {
            db_error("Revice message MSG_DATABASE_UPDATE_FINISHED");
            if(m_workMode == 0)
                break;
            Notify(p_msg);
        }
        break;
        case MSG_CAMERA_ON_ERROR:
        {
            db_error("receive camera on error");
            Notify(p_msg);
        }
        break;

        case MSG_REINIT_CAMERA_FINISH:
        {
            db_error("MSG_REINIT_CAMERA_FINISH");
            Notify(p_msg);
        }
        break;
        case MSG_BACKCARVIDEO_ON:
        {
            if(WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_PREVIEW && m_camBShowFlag == 1){
                MenuConfigLua *mcl=MenuConfigLua::GetInstance();
                int preview_camera = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CAMERA);
                if(preview_camera == 0)
                    SwitchDisplay();
                Notify(p_msg);
            }
        }
        break;
        case MSG_BACKCARVIDEO_OFF:
        {
            if(WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_PREVIEW && m_camBShowFlag == 1){
                MenuConfigLua *mcl=MenuConfigLua::GetInstance();
                int preview_camera = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CAMERA);
                if(preview_camera == 1)
                    SwitchDisplay();
                Notify(p_msg);
            }
        }
        break;
        case MSG_TF_CARD_WORNING:
        case MSG_TF_CARD_ERROR:
        {
            db_msg("newpreview recived msg TF error id = %d",p_msg);
            //如果是息屏的状态下先亮屏
            if(!PowerManager::GetInstance()->IsScreenOn())
            {
                PowerManager::GetInstance()->ScreenOn();
            }
            //收到应该弹窗提示
            Notify(p_msg);
        }
        break;
        case MSG_CPU_TEMP_HIGH:
        {
            if(isRecordStart){
                if(!PowerManager::GetInstance()->IsScreenOn())
                {
                    PowerManager::GetInstance()->ScreenOn();
                }
                db_error("newpreview receive cpu temp high msg,will stop rec");
                Notify(p_msg);
            }
        }
        break;
        case SHOW_SHUTDOWN_LOGO:
        {
            this->Notify((MSG_TYPE)SHOW_SHUTDOWN_LOGO);
        }
        break;
        default:
        break;
	}
}

#ifdef ENABLE_RTSP

void NewPreview::SendRtspData(const VEncBuffer *frame, Recorder *rec, NewPreview *self)
{
    if (self->m_stream_sender_map.size() == 0) return;
    if (self->m_RtspServer->GetServerStatus() == RtspServer::SERVER_STOPED) return;

    RtspServer::StreamSender *stream_sender = self->m_stream_sender_map[rec];
    MediaStream::FrameDataType frame_type;
   // db_msg("NewPreview SendRtspData 11\n");
    if (frame->stream_type == 0x00) { // video
        if ((*(frame->data + 4) & 0x1F) == 5) { // if is I frame

            // send sps/pps first
            VencHeaderData head_info = {NULL, 0};
            rec->GetVencHeaderData(head_info);

        if (head_info.pBuffer != NULL && head_info.nLength != 0)
           stream_sender->SendVideoData(head_info.pBuffer, head_info.nLength, frame->pts, MediaStream::FRAME_DATA_TYPE_HEADER);
           frame_type = MediaStream::FRAME_DATA_TYPE_I;
        }
        else {
           frame_type = MediaStream::FRAME_DATA_TYPE_P;
        }
       // db_msg("NewPreview SendRtspData data size = %ld ; frame_type = %d\n",frame->data_size,frame_type);
        stream_sender->SendVideoData((unsigned char *) frame->data, frame->data_size, frame->pts,frame_type);
    } else if (frame->stream_type == 0x01) {
        stream_sender->SendAudioData((unsigned char *) frame->data, frame->data_size);
    } else {
       db_msg("stream type: %d", frame->stream_type);
    }
}

static void RtspOnClientConnected(void *context)
{
    db_msg("rtsp client connected, encode I Frame immediately");
    Recorder *recorder = reinterpret_cast<Recorder*>(context);
    recorder->ForceIFrame();
}

void NewPreview::CreateRtspStreamSender()
{
    for(auto iter1 = m_CamRecMap.begin(); iter1 != m_CamRecMap.end(); iter1++)
    {
        Camera *cam = m_cameraMap[iter1->first];
        for(auto iter2 = m_CamRecMap[iter1->first].begin(); iter2 != m_CamRecMap[iter1->first].end(); iter2++)
        {
            if(iter2->first == 1 || iter2->first == 3)
            {
                    Recorder *recorder = m_CamRecMap[iter1->first][iter2->first];
                    std::stringstream ss;
                    RtspServer::StreamSender *stream_sender = NULL;

                    ss << "ch" << cam->GetCameraID() << iter2->first;
                    stream_sender = m_RtspServer->CreateStreamSender(ss.str());
                  //  std::string url_ = stream_sender->streamURL();
                    stream_sender->SetSenderCallback(&RtspOnClientConnected, recorder);
                    db_error("rtsp url: %s", stream_sender->GetUrl().c_str());
                    if (stream_sender != NULL) {
                        m_stream_sender_map.insert(make_pair(recorder, stream_sender));
                    }
            }
        }
    }
}

void NewPreview::EncodeDataCallback(EncodeDataCallbackParam *param)
{
    int ret = 0;
    int sender_type = 0;
    VEncBuffer* frame = NULL;
    Recorder *rec = param->rec_;
   // db_msg("EncodeDataCallback is recived\n");
    switch (param->what_) {
        case MPP_EVENT_ERROR_ENCBUFFER_OVERFLOW:
            // TODO 考虑buffer正在被覆盖的问题
            db_error("send data too slow!!!");
            break;
        default:
            break;
    }

    NewPreview *self = static_cast<NewPreview *>(param->context_);
  //  sender_type = rec->GetStreamSenderType();

    while (1) {

        frame = rec->GetEyeseeRecorder()->getOneBsFrame();
        if (frame == NULL)
            break;

        if (frame->data == NULL) {
            static int cnt = 0;
            rec->GetEyeseeRecorder()->freeOneBsFrame(frame);
            db_error("null data: %d", ++cnt);
            break;
        }
        
       // db_msg("EncodeDataCallback is recived 11\n");
        self->SendRtspData(frame, rec, self);
#if 0
        if (self->mode_ == NORMAL_MODE) {
            if (self->rtsp_flag_ && (sender_type & STREAM_SENDER_RTSP) == STREAM_SENDER_RTSP) {
               
            }
            if (self->tutk_flag_ && (sender_type & STREAM_SENDER_TUTK) == STREAM_SENDER_TUTK) {
                self->SendTutkData(frame, rec, self);
            }
        } else if (self->mode_ == USB_MODE_UVC) {
            if ((sender_type & STREAM_SENDER_UVC) == STREAM_SENDER_UVC) {
                self->PutUVCData(frame, rec, self);
            }
        }
#endif

#ifdef WRITE_RAW_H264_FILE
        self->WriteRawData(frame, rec, self);
#endif

        rec->GetEyeseeRecorder()->freeOneBsFrame(frame);
    }
}


int NewPreview::CreateRtspServer()
{
    string ip;
    //获取ip地址
    m_RtspServer = m_RtspServer->GetInstance();
    if (m_NetManger->GetNetDevIp("wlan0", ip) < 0)
    {
        ip = "0.0.0.0";
        db_warn("no activity net device found, set rtsp server ip to '%s", ip.c_str());
    }
    //创建RTSP server
    m_RtspServer->CreateServer(ip);
    return 0;
}

void* NewPreview::RtspThreadLoop(void *context)
{
    NewPreview *self = reinterpret_cast<NewPreview*>(context);
    prctl(PR_SET_NAME, "RtspThreadLoop", 0, 0, 0);
    db_debug("run rtsp server");
    self->m_RtspServer->Run();
    return NULL;
}


void NewPreview::RtspServerStart()
{
    pthread_t rtsp_thread_id;
    ThreadCreate(&rtsp_thread_id, NULL, NewPreview::RtspThreadLoop, this);
}

void NewPreview::DestroyRtspServer()
{
    if (m_RtspServer == NULL) {
        db_debug("rtsp server already destroyed, no need to stop");
        return;
    }
    m_RtspServer->Stop();
    db_debug("rtsp server stoped");

    map<Recorder*, RtspServer::StreamSender*>::iterator sender_iter;
    for (sender_iter = m_stream_sender_map.begin(); sender_iter != m_stream_sender_map.end(); sender_iter++) {
        delete (sender_iter->second);
    }
    m_stream_sender_map.clear();
    db_debug("delete all stream sender");

    // tempory add a delay for wait rtsp server inner thread exit
    usleep(500*1000);
    RtspServer::Destroy();
    m_RtspServer = NULL;
    db_debug("rtsp server destroyed");
//    system("/tmp/netstat -tln | grep 8554");

}
#endif

int NewPreview::SetRecorderDataCallBack(int p_CamId, int p_record_id)
{
	if(!CheckCamIdIsValid(p_CamId) )
		return -1;

	if(!CheckRecorderExist(p_CamId, p_record_id))
		return -1;

	Recorder *rec = GetRecorder(p_CamId,p_record_id);

//	rec->SetEncodeDataCallback(PreviewPresenter::EncodeDataCallback, this);

	return 0;
}

bool NewPreview::CheckCamIdIsValid(int p_CamId)
{
	if( p_CamId < 0  || p_CamId > 1)
	{
		db_msg("CamId:%d is invalid",p_CamId);
		return false;
	}

	return true;
}

bool NewPreview::CheckCameraExist(int p_CamId)
{
	map<int, Camera*>::iterator iter;
	for(iter = m_cameraMap.begin(); iter != m_cameraMap.end(); iter++)
	{
		if( iter->first == p_CamId ){
		    return true;
		}
	}
	
	db_msg("CamId %d did not create yet!",p_CamId);

	return false;
}

bool NewPreview::CheckRecorderExist(int p_CamId, int p_record_id)
{
	map<int, std::map<int, Recorder *>>::iterator iter;
	for(iter = m_CamRecMap.begin(); iter != m_CamRecMap.end(); iter++ )
	{
		if( iter->first != p_CamId )
			continue;

		map<int, Recorder*>::iterator rec_iter;
		for (rec_iter = m_CamRecMap[p_CamId].begin();rec_iter != m_CamRecMap[p_CamId].end(); rec_iter++)
		{
			if( rec_iter->first == p_record_id ){
			    return true;
			}
		}
	}

	return false;
}

Camera* NewPreview::GetCamera(int p_CamId)
{
	map<int, Camera*>::iterator iter;
	for(iter = m_cameraMap.begin(); iter != m_cameraMap.end(); iter++)
	{
		if( iter->first == p_CamId ){
		return iter->second;
        }
	}

	return NULL;
}

Recorder* NewPreview::GetRecorder(int p_CamId, int p_record_id)
{
	map<int, std::map<int, Recorder *>>::iterator iter;
	for(iter = m_CamRecMap.begin(); iter != m_CamRecMap.end(); iter++ )
	{
		if( iter->first == p_CamId ){

		std::map<int, Recorder*>::iterator rec_iter;
		for (rec_iter = m_CamRecMap[p_CamId].begin();rec_iter != m_CamRecMap[p_CamId].end(); rec_iter++)
		{
			if( rec_iter->first == p_record_id )
			    return rec_iter->second;
		}
		}
	}

	return NULL;
}

void NewPreview::OnWindowLoaded()
{
#if 0
    preview_win_  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
#endif
    db_msg("SetLayerAlpha LAYER_UI 150");
#ifdef DEINIT_CAMERA
    db_debug("deinit_flag_ %d",deinit_flag_);
    if(deinit_flag_){
        MediaInit();
        CreateRecorder(CAM_A, 0);
        #ifdef USE_CAMB
        if(m_camBShowFlag == 1){
            CreateRecorder(CAM_B, 2);
        }
        #endif

        OsdManager::get()->initTimeOsd(m_CamRecMap);
        deinit_flag_ = false;
    }
#endif
    MenuConfigLua *mcl=MenuConfigLua::GetInstance();
    int preview_camera = mcl->GetMenuIndexConfig(SETTING_PREVIEW_CAMERA);
    Layer::GetInstance()->SetLayerAlpha(LAYER_UI, 150);
    Camera *cam = NULL;
    cam = GetCamera(CAM_A);
    ViewInfo cam0_rect,cam1_rect;
    if( cam != NULL )
    {
        cam->ShowPreview();
        cam->GetCameraDispRect(cam0_rect);
        if(preview_camera == 0){
            cam->SetCameraDispRect(cam0_rect, 2);
        }else{
            cam->SetCameraDispRect(cam0_rect, 0);
        }
    }
    if(m_camBShowFlag == 1)
    {
        cam = GetCamera(CAM_B);
        if( cam != NULL )
        {
            cam->ShowPreview();
            cam->GetCameraDispRect(cam1_rect);
            if(preview_camera == 1){
                cam->SetCameraDispRect(cam1_rect, 2);
            }else{
                cam->SetCameraDispRect(cam1_rect, 0);
            }
        }
    }else{
        mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CAMERA,0);
    }

#ifdef SHOW_DEBUG_INFO
    preview_win_->ShowDebugInfo(true);
    debug_info_thread_ = thread(DebugInfoThread, this);
#endif
}

void NewPreview::OnWindowDetached()
{
    Camera *cam = NULL;
    cam = GetCamera(CAM_A);
    if( cam != NULL )
    {
        cam->HidePreview();
    }
#ifdef USE_CAMB
    if(m_camBShowFlag == 1 )
    {
        cam = GetCamera(CAM_B);
        if( cam != NULL ){
            cam->HidePreview();
        }
    }
#endif
#ifdef DEINIT_CAMERA
    db_debug("deinit_flag_ %d",deinit_flag_);
    if(deinit_flag_)
        MediaDeInit();
#endif

#ifdef SHOW_DEBUG_INFO
    if (debug_info_thread_.joinable()) {
        pthread_cancel(debug_info_thread_.native_handle());
        debug_info_thread_.join();
    }
    preview_win_->ShowDebugInfo(false);
#endif
}

void NewPreview::OnUILoaded()
{
	//Layer::GetInstance()->OpenLayer(LAYER_UI);
}

void NewPreview::BindGUIWindow(::Window *win)
{
    Attach(win);
}

int NewPreview::TakePicforVideothumb(int p_CamId, int p_record_id)
{
	if(!CheckCamIdIsValid(p_CamId) )
	{
		db_warn("invalid camera id:%d",p_CamId);
		return -1;
	}

	if(!CheckCameraExist(p_CamId) )
	{
		db_warn("camera id:%d not create yet",p_CamId);
		return -1;
	}
	RecorderParam param;
	Recorder* rec = GetRecorder(p_CamId,p_record_id);
	if( rec == NULL )
	{
		db_warn("get recorder failed camId:%d recorderId:%d",p_CamId);
		return -1;
	}

	rec->GetParam(param);
	Camera *cam = GetCamera(p_CamId);
	if( cam != NULL)
	{
		cam->SetVideoFileForThumb(param.MF);
	    cam->SetPictureMode(TAKE_PICTURE_MODE_FAST);
        if( p_CamId )
        {
            //db_warn("000TakePicforVideothumb :%d",p_CamId);
            cam->TakePictureEx(3, true);
        }
        else{
            //db_warn("111TakePicforVideothumb: %d",p_CamId);
            cam->TakePictureEx(2,true);
        }
		return 0;
	}
	return -1;
}

int NewPreview::RemoteSwitchRecord(int value)
{
   PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
   if(pw->HandlerPromptInfo() < 0)
   {
        db_error("tf card is not ready\n");
        return -1;
   }
    if(value == 1 && isRecordStart)
    {
        db_warn("the main record is start do nothing\n");
        return 0;
    }else if(value == 1 && !isRecordStart)
    {
        db_warn("RemoteSwitchRecord start record\n");
        Recorder *rec1 = GetRecorder(0, 0);
        if(rec1 != NULL && !rec1->RecorderIsBusy()){
		    StartRecord(0, 0);
            TakePicforVideothumb(0, 0);
        }
        if(m_camBShowFlag == 1)
		{
		    Recorder *rec = GetRecorder(1, 2);
			if(rec != NULL && !rec->RecorderIsBusy())
		    {
				rec->StartRecord();
				TakePicforVideothumb(1, 2);
			}
		}
        this->Notify((MSG_TYPE)MSG_SET_STATUS_PREVIEW_REC_PLAY);

        //start osd
        if(m_bOsdEnable){
         OsdManager::get()->AttchlogoVencRegion(0,5);
          OsdManager::get()->AttchVencRegion(0,0);
	     OsdManager::get()->startTimeOsd(CAM_A,0);
          if(m_camBShowFlag == 1){
              OsdManager::get()->AttchlogoVencRegion(2,6);
              OsdManager::get()->AttchVencRegion(2,2);
              OsdManager::get()->startTimeOsd(CAM_B,2);
          }
        }
	}else if(value == 0 && isRecordStart)
    {        
        db_warn("RemoteSwitchRecord stop record\n");
        //stop osd
        if(m_bOsdEnable){
         OsdManager::get()->DettchVencRegion(0,0);                    
         OsdManager::get()->DettchVencRegion(0,5);
         OsdManager::get()->stopTimeOsd(0,0);
         if(m_camBShowFlag == 1){
             OsdManager::get()->DettchVencRegion(2,2);                    
             OsdManager::get()->DettchVencRegion(2,6);
    		 OsdManager::get()->stopTimeOsd(1,2);
         }
        }
        
		StopRecord(0, 0);
       if(m_camBShowFlag == 1){
		StopRecord(0, 2);
       }
       this->Notify((MSG_TYPE)MSG_SET_STATUS_PREVIEW_REC_PAUSE);
	}else{
        db_warn("the main record is aready stoped \n");
    }
    return 0;
}

int NewPreview::RemoteTakePhoto()
{
   db_warn("RemoteTakePhoto happen\n");
   PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
   if(pw->HandlerPromptInfo() < 0)
   {
        db_error("tf card is not ready\n");
        return -1;
   }
    TakePicture(0);
    if(m_camBShowFlag == 1){
         TakePicture(1);
    }
    return 0;
}

int NewPreview::GetCurretRecordTime()
{
    #if 0
    int ret = 0;
    db_error("GetCurretRecordTime get in\n");
    if(isRecordStart){
        PreviewWindow *pw  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
        ret = pw->GetCurrentRecordTime();
        db_error("the current is time %d",ret);
    }else{
        db_error("the record is not start\n");
    }
    #endif
    return 0;
}

void NewPreview::SetFileLockEnable()
{
    this->Notify((MSG_TYPE)MSG_RECFILELOCK_ENABLE,0,1);
}

#ifdef ENABLE_ADAS
void NewPreview::GetADASEventMsg()
{
    Camera *cam = NULL;
    cam = GetCamera(CAM_A);
    if(cam != NULL){
        memset(&adas_event_,0,sizeof(AW_AI_ADAS_DETECT_R__v2));
        cam->GetADASEvent(&adas_event_);
    }
}

void NewPreview::ADASEventUpdate(union sigval sigval)
{
    NewPreview *preview = reinterpret_cast<NewPreview *>(sigval.sival_ptr);
    if(preview != NULL){
        PreviewWindow *preview_win  = static_cast<PreviewWindow*>(preview->win_mg_->GetWindow(WINDOWID_PREVIEW));
        pthread_mutex_lock(&(preview->adas_lock_));
        preview->GetADASEventMsg();
        preview_win->UpdateADASEventMsg(&(preview->adas_event_));
        pthread_mutex_unlock(&(preview->adas_lock_));
//        db_error("car num %d,score %d",
//                preview->adas_event_.nADASOutData_v2.cars.Num,preview->adas_event_.nADASOutData_v2.score);
    }
}
#endif

int NewPreview::TakeParkVideothumb(int p_CamId, MediaFile *park_mediafile)
{
	if(!CheckCamIdIsValid(p_CamId) )
	{
		db_warn("invalid camera id:%d",p_CamId);
		return -1;
	}

	if(!CheckCameraExist(p_CamId) )
	{
		db_warn("camera id:%d not create yet",p_CamId);
		return -1;
	}
	Camera *cam = GetCamera(p_CamId);
	if( cam != NULL)
	{
		cam->SetVideoFileForThumb(park_mediafile);
	    cam->SetPictureMode(TAKE_PICTURE_MODE_FAST);
        if( p_CamId )
        {
            cam->TakePictureEx(3, true);
        }
        else{
            cam->TakePictureEx(2,true);
        }
		return 0;
	}
	return -1;
}
