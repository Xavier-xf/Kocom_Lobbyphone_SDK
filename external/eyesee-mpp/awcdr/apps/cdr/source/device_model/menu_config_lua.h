/* *******************************************************************************
 * Copyright (C), 2017-2025, sunchip Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file menu_config_lua.h
 * @brief 设置参数控制接口
 * @author :fangjj
 * @version v1.0
 * @date 2017-03-29
 */
#pragma once


#include "lua/lua_config_parser.h"
#include "lua/lua.hpp"
#include "common/subject.h"
#include "bll_presenter/presenter.h"
#include "bll_presenter/gui_presenter_base.h"
#include "bll_presenter/common_type.h"

#include <map>

typedef struct Param1_{
    int   current;
    int   count;
}Param1;
typedef struct String1_{
    char string1[128];
}String1;

typedef struct String2_{
    char string1[128];
    char string2[128];
}String2;
typedef struct String3_{
    char string1[128];
    char string2[128];
    char string3[128];
}String3;  
typedef struct String4_{
    char string1[128];
    char string2[128];
    char string3[128]; 
    char string4[128];
}String4; 

 typedef struct MenuConfig_{
	/*****switch******/
//    int  switch_record_eis;
    int  switch_record_awmd;
    int  switch_record_drivingmode;
    int  switch_camera_imagerotation;
    int  switch_camera_ledindicator;
    int  switch_camera_timewatermark;
    int  switch_camera_distortioncalibration;
    int  switch_camera_keytone;
  	int  switch_acc_resume;
	int  camB_preview;
    int  switch_wifi;

    /*****record******/

    Param1 record_rear_resolution;
    Param1 record_screen_brightness;
    Param1 record_screen_disturb_mode;
    Param1 record_voice_take_photo;
    Param1 record_4g_network_switch;
    Param1 device_wifi_switch;
    Param1 record_power_on_sound;
    Param1 record_key_sound;
    Param1 record_drivering_report;

    Param1 record_adas_forward_collision_waring;
    Param1 record_adas_lane_shift_reminding;
    Param1 record_watchdog;
    Param1 record_probeprompt;
    Param1 record_speedprompt;
    Param1 record_emerrecord;
    Param1 record_parkingmonitor_switch;
    Param1 record_parkingwarnlamp_switch;
    Param1 record_parkingabnormalmonitory_switch;
    Param1 record_parkingloopabnormalnotice_switch;
    Param1 record_parkingloop_switch;
    Param1 record_parkingloop_resolution;
    Param1 record_encodingtype;
    Param1 record_standby_clock; 
    Param1 record_timelapse; 
    Param1 record_slowmotion; 
    Param1 record_switch_acc;
	
     /*****photo******/
    Param1 photo_resolution;
    Param1 photo_timed;
    Param1 photo_auto;
    Param1 photo_dramashot; 
    
     /*****camera******/
    Param1 camera_whitebalance;
    Param1 camera_timedshutdown; 
         
    String2 wifi_info; 
    String1 system_version_info;
    String1 device_update;
    String1 device_systemTime;
      /*****device******/
    Param1 device_datatime;
    Param1 device_voicestatus;
    String4 device_deviceinfo;



    /*device setting*/
    Param1 record_resolution;
    Param1 record_time;
    Param1 record_loop_switch;
    Param1 record_emerrecordsen; //碰撞灵敏度
    Param1 record_timewatermark;
    Param1 record_sound;
    Param1 camera_exposure;
    Param1 camera_lightfreq;

    /*adas setting*/
    Param1 record_adas_switch;
    Param1 record_adas_calibration;

    /*general setting*/
    Param1 camera_autoscreensaver;
    Param1 device_language;
    Param1 parking_monitory;
    Param1 voice_ctrl;
    Param1 record_volume_switch;
    Param1 wifi_switch;

    /*preview config*/
    Param1 camera;
    Param1 crop0;
    Param1 crop1;

    /*reverse line*/
    Param1 reverseline_id;

}SunChipMenuConfig;

namespace EyeseeLinux {


/**
 * @brief 设置参数控制接口
 */
class MenuConfigLua:public Singleton<MenuConfigLua>
{
    friend class Singleton<MenuConfigLua>;
public:

    int SetMenuIndexConfig(int msg, int val);

    int GetMenuIndexConfig(int msg);

    int GetMenuIndexCountConfig(int msg);

    int SaveMenuAllConfig(void);

    int DefaultMenuConfig(void);
    int GetMenuConfig(SunChipMenuConfig &resp);
    int  ResetMenuConfig(void);
    int  UpdateWifiInfo();
    SunChipMenuConfig menu_cfg_,usr_temp_menu_cfg_,data_menu_cfg_;
    int ChangeMenuConfig();
    int SetMenuStringConfig(int msg, std::string & str);
    int UpdateSystemTime(bool flag);
    bool IsVersionSame(std::string external_version,std::string local_version);
    int ComparisonProfile(LuaConfig * usr_luacfg , LuaConfig * data_luacfg);
    int SetWifiSsid(std::string ssid);
    int SetWifiPassword(std::string pass);

private:
    MenuConfigLua();
    ~MenuConfigLua();
    MenuConfigLua(const MenuConfigLua &o);
    MenuConfigLua &operator=(const MenuConfigLua &o);
    LuaConfig *lua_cfg_,*lua_cfg_temp_;
    int LoadMenuConfig(void);
};

} // namespace EyeseeLinux
