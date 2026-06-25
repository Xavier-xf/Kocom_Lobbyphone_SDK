/* *******************************************************************************
 * Copyright (C), 2017-2025, sunchip Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file menu_config_lua.cpp
 * @brief 璁剧疆鍙傛暟鎺у埗鎺ュ彛
 * @author :fangjianjun
 * @version v1.0
 * @date 2017-03-29
 */

#include "menu_config_lua.h"
#include "common/app_log.h"
#include "window/window.h"
#include "window/user_msg.h"
#include "common/setting_menu_id.h"
#include "device_model/system/rtc.h"

#undef LOG_TAG
#define LOG_TAG "MenuConfigLua"
#define NO_DEBUG   0
#define DEFAULT_VALUE -1
using namespace EyeseeLinux;
using namespace std;

#define MENU_CONFIG_FILE "/tmp/data/menu_config.lua"
#define MENU_CONFIG_FILE_USR "/usr/share/app/sdv/menu_config.lua"
#define MENU_CONFIG_FILE_USR_TEMP "/usr/share/app/sdv/menu_config_temp.lua"


MenuConfigLua::MenuConfigLua()
{
    memset(&menu_cfg_, 0, sizeof(menu_cfg_));
    memset(&usr_temp_menu_cfg_, 0, sizeof(usr_temp_menu_cfg_));
    memset(&data_menu_cfg_, 0, sizeof(data_menu_cfg_));
    if(ChangeMenuConfig() < 0)
    {
        db_msg("[zhb]:[FUN]:%s [LINE]:%d  Do ChangeMenuConfig !\n", __func__, __LINE__);
    }

    lua_cfg_ = new LuaConfig();
    int ret = LoadMenuConfig();
    if (ret) {
        db_msg("[fangjj]:[FUN]:%s [LINE]:%d  Do LoadMenuConfig fail:%d !\n", __func__, __LINE__, ret);
    	}
   //ready to set the system
   UpdateSystemTime(false);
}

MenuConfigLua::~MenuConfigLua()
{
    if (NULL != lua_cfg_) {
        delete lua_cfg_;
    }
}

int MenuConfigLua::SaveMenuAllConfig(void)
{
    int ret = 0;
    int cnt = 0, i = 0;
    char tmp_str[256] = {0};
    std::string str;

    if (NULL == this->lua_cfg_) {
        db_error("[fangjj]:The lua_cfg_ is NULL! error! \n");
        return -1;
    }
    if (!FILE_EXIST(MENU_CONFIG_FILE)) {
        db_warn("[fangjj]:config file %s not exist, copy default from /usr/share/app/sdv", MENU_CONFIG_FILE);
        system("cp -f /usr/share/app/sdv/menu_config.lua /tmp/data/");
    }

    ret = lua_cfg_->LoadFromFile(MENU_CONFIG_FILE);
    if (ret) {
        db_warn("[fangjj]:Load %s failed, copy backup and try again", MENU_CONFIG_FILE);
        system("cp -f /usr/share/app/sdv/menu_config.lua /tmp/data/");
    
        ret = lua_cfg_->LoadFromFile(MENU_CONFIG_FILE);
        if (ret) {
            db_error("[fangjj]:Load %s failed!", MENU_CONFIG_FILE);
            return -1;
        }
    }
    db_msg("[fangjj]:Load %s success", MENU_CONFIG_FILE);

    /*device setting*/
    lua_cfg_->SetIntegerValue("menu.device.resolution.current",  menu_cfg_.record_resolution.current);
    lua_cfg_->SetIntegerValue("menu.device.resolution.count",  menu_cfg_.record_resolution.count);
    lua_cfg_->SetIntegerValue("menu.device.record_time.current",  menu_cfg_.record_time.current);
    lua_cfg_->SetIntegerValue("menu.device.record_time.count",  menu_cfg_.record_time.count);
    lua_cfg_->SetIntegerValue("menu.device.record_loop.current",  menu_cfg_.record_loop_switch.current);
    lua_cfg_->SetIntegerValue("menu.device.record_loop.count",  menu_cfg_.record_loop_switch.count);
    lua_cfg_->SetIntegerValue("menu.device.emerrecordsen.current",  menu_cfg_.record_emerrecordsen.current);
    lua_cfg_->SetIntegerValue("menu.device.emerrecordsen.count",  menu_cfg_.record_emerrecordsen.count);
    lua_cfg_->SetIntegerValue("menu.device.timewatermark.current",  menu_cfg_.record_timewatermark.current);
    lua_cfg_->SetIntegerValue("menu.device.timewatermark.count",  menu_cfg_.record_timewatermark.count);
    lua_cfg_->SetIntegerValue("menu.device.record_sound.current",  menu_cfg_.record_sound.current);
    lua_cfg_->SetIntegerValue("menu.device.record_sound.count",  menu_cfg_.record_sound.count);
    lua_cfg_->SetIntegerValue("menu.device.exposure.current",  menu_cfg_.camera_exposure.current);
    lua_cfg_->SetIntegerValue("menu.device.exposure.count",  menu_cfg_.camera_exposure.count);
    lua_cfg_->SetIntegerValue("menu.device.lightfreq.current",  menu_cfg_.camera_lightfreq.current);
    lua_cfg_->SetIntegerValue("menu.device.lightfreq.count",  menu_cfg_.camera_lightfreq.count);

    /*ADAS setting*/
    lua_cfg_->SetIntegerValue("menu.adas.adas_switch.current",  menu_cfg_.record_adas_switch.current);
    lua_cfg_->SetIntegerValue("menu.adas.adas_switch.count",  menu_cfg_.record_adas_switch.count);
    lua_cfg_->SetIntegerValue("menu.adas.adas_calibration.current",  menu_cfg_.record_adas_calibration.current);
    lua_cfg_->SetIntegerValue("menu.adas.adas_calibration.count",  menu_cfg_.record_adas_calibration.count);

    /*general setting*/
    lua_cfg_->SetIntegerValue("menu.general.autoscreensaver.current",  menu_cfg_.camera_autoscreensaver.current);
    lua_cfg_->SetIntegerValue("menu.general.autoscreensaver.count",  menu_cfg_.camera_autoscreensaver.count);
    lua_cfg_->SetIntegerValue("menu.general.parking_monitory.current",  menu_cfg_.parking_monitory.current);
    lua_cfg_->SetIntegerValue("menu.general.parking_monitory.count",  menu_cfg_.parking_monitory.count);
    lua_cfg_->SetIntegerValue("menu.general.voice_ctrl.current",  menu_cfg_.voice_ctrl.current);
    lua_cfg_->SetIntegerValue("menu.general.voice_ctrl.count",  menu_cfg_.voice_ctrl.count);
    lua_cfg_->SetIntegerValue("menu.general.language.current",  menu_cfg_.device_language.current);
    lua_cfg_->SetIntegerValue("menu.general.language.count",  menu_cfg_.device_language.count);
    lua_cfg_->SetIntegerValue("menu.general.system_volume.current",  menu_cfg_.record_volume_switch.current);
    lua_cfg_->SetIntegerValue("menu.general.system_volume.count",  menu_cfg_.record_volume_switch.count);
    lua_cfg_->SetIntegerValue("menu.general.wifi_switch.current",  menu_cfg_.wifi_switch.current);
    lua_cfg_->SetIntegerValue("menu.general.wifi_switch.count",  menu_cfg_.wifi_switch.count);
    lua_cfg_->SetStringValue("menu.general.systemTimeSave.timeStr",  menu_cfg_.device_systemTime.string1);


    /*system*/
    lua_cfg_->SetStringValue("menu.system.wifi_info.ssid",  menu_cfg_.wifi_info.string1);
    lua_cfg_->SetStringValue("menu.system.wifi_info.password",  menu_cfg_.wifi_info.string2);
    lua_cfg_->SetStringValue("menu.system.version_info",  menu_cfg_.system_version_info.string1);

    /*preview*/
    lua_cfg_->SetIntegerValue("menu.preview.camera.count",  menu_cfg_.camera.count);
    lua_cfg_->SetIntegerValue("menu.preview.camera.current",  menu_cfg_.camera.current);
    lua_cfg_->SetIntegerValue("menu.preview.crop0.count",  menu_cfg_.crop0.count);
    lua_cfg_->SetIntegerValue("menu.preview.crop0.current",  menu_cfg_.crop0.current);
    lua_cfg_->SetIntegerValue("menu.preview.crop1.count",  menu_cfg_.crop1.count);
    lua_cfg_->SetIntegerValue("menu.preview.crop1.current",  menu_cfg_.crop1.current);

    /*reverse line*/
    lua_cfg_->SetIntegerValue("menu.reverseline_id.line_id.count",  menu_cfg_.reverseline_id.count);
    lua_cfg_->SetIntegerValue("menu.reverseline_id.line_id.current",  menu_cfg_.reverseline_id.current);

    ret = lua_cfg_->SyncConfigToFile(MENU_CONFIG_FILE, "menu");
    if (ret < 0) {
    db_error("[fangjj]:Do SyncConfigToFile error! file:%s \n", MENU_CONFIG_FILE);
    return -1;
    }

    return 0;
}

int MenuConfigLua::DefaultMenuConfig(void)
{
    return 0;
}

int MenuConfigLua::SetMenuIndexConfig(int msg, int val)
{
    db_error("[fangjj]:SetMenuIndexConfig:msg[%d], val[%d]", msg, val);
    switch(msg)
    {
        case MSG_SET_VIDEO_RESOULATION:  //ok
            menu_cfg_.record_resolution.current = val;
        break;

        case MSG_SET_RECORD_TIME: //ok
            menu_cfg_.record_time.current =val;
        break;

        case MSG_RECORD_LOOP_SWITCH:
            menu_cfg_.record_loop_switch.current =val;
        break;

        case SETTING_EMER_RECORD_SENSITIVITY:
        case MSG_SET_EMER_RECORD_SENSITIVITY:
            menu_cfg_.record_emerrecordsen.current =val;
        break;

        case MSG_SET_TIMEWATER_MARK:
            menu_cfg_.record_timewatermark.current = val;
        break;

        case MSG_SET_RECORD_VOLUME:
            menu_cfg_.record_sound.current = val;
        break;

        case MSG_SET_CAMERA_EXPOSURE:
            menu_cfg_.camera_exposure.current = val;
        break;

        case MSG_SET_CAMERA_LIGHTSOURCEFREQUENCY:
            menu_cfg_.camera_lightfreq.current = val;
        break;

        case MSG_SET_ADAS_SWITCH:
            menu_cfg_.record_adas_switch.current = val;
        break;

        case MSG_SET_ADAS_CALIBRATION:
            menu_cfg_.record_adas_calibration.current = val;
        break;

        case MSG_SET_AUTO_TIME_SCREENSAVER:
            menu_cfg_.camera_autoscreensaver.current = val;
        break;

        case MSG_RM_LANG_CHANGED:
            menu_cfg_.device_language.current = val;
        break;

        case MSG_SET_PARKING_MONITORY:
            menu_cfg_.parking_monitory.current = val;
        break;

        case MSG_SET_VOICE_CTRL:
            menu_cfg_.voice_ctrl.current = val;
        break;

        case MSG_SET_VOLUME_SELECTION:
            menu_cfg_.record_volume_switch.current = val;
        break;

        case MSG_SET_WIFI_SWITCH:
            menu_cfg_.wifi_switch.current = val;
        break;

        case MSG_SET_PREVIEW_CAMERA:
            menu_cfg_.camera.current = val;
        break;

        case MSG_SET_PREVIEW_CROP0:
            menu_cfg_.crop0.current = val;
        break;

        case MSG_SET_PREVIEW_CROP1:
            menu_cfg_.crop1.current = val;
        break;

        case MSG_SET_REVERSELINEID:
            menu_cfg_.reverseline_id.current = val;
        break;

#if 0
        case MSG_SET_PARKING_MONITORY: //ok
        {
            if(val > 1 || val < 0){
                db_error("Invalid parking monitor val %d,max val is 1 or min val is 0",val);
                return -1;
            }else{
                menu_cfg_.record_parkingmonitor_switch.current = val;
            }
            break;
        }
        case MSG_SET_RECORD_ENCODE_TYPE: //ok
        {
            if(menu_cfg_.record_encodingtype.count - 1 < val){
                db_error("Invalid record type val %d,max val is %d",
                val,menu_cfg_.record_encodingtype.count - 1);
                return -1;
            }else if(val < 0){
                db_error("Invalid record type val %d,min val is 0",val);
                return -1;
            }
            menu_cfg_.record_encodingtype.current =val;
        }
        break;

        case MSG_DEVICE_TIME:
            menu_cfg_.device_datatime.current=val;
        break;
#endif
        default:
            db_msg("[fangjj]:SetMenuIndexConfig:unhandled message: msg[%d], val[%d]", msg, val);
        break;
      }    
      SaveMenuAllConfig();
      return 0; 
} 

 int MenuConfigLua::GetMenuConfig(SunChipMenuConfig &resp)
{
    /*device setting*/
    resp.record_resolution.current = menu_cfg_.record_resolution.current;
    resp.record_time.current = menu_cfg_.record_time.current;
    resp.record_loop_switch.current = menu_cfg_.record_loop_switch.current;
    resp.record_emerrecordsen.current = menu_cfg_.record_emerrecordsen.current;
    resp.record_timewatermark.current = menu_cfg_.record_timewatermark.current;
    resp.record_sound.current = menu_cfg_.record_sound.current;
    resp.camera_exposure.current = menu_cfg_.camera_exposure.current;
    resp.camera_lightfreq.current = menu_cfg_.camera_lightfreq.current;

    /*ADAS setting*/
    resp.record_adas_switch.current = menu_cfg_.record_adas_switch.current;
    resp.record_adas_calibration.current = menu_cfg_.record_adas_calibration.current;

    /*general setting*/
    resp.camera_autoscreensaver.current = menu_cfg_.camera_autoscreensaver.current;
    resp.parking_monitory.current = menu_cfg_.parking_monitory.current;
    resp.voice_ctrl.current = menu_cfg_.voice_ctrl.current;
    resp.device_language.current = menu_cfg_.device_language.current;
    resp.record_volume_switch.current = menu_cfg_.record_volume_switch.current;
    resp.wifi_switch.current = menu_cfg_.wifi_switch.current;

    /*system*/
    strncpy(resp.wifi_info.string1, menu_cfg_.wifi_info.string1, sizeof(menu_cfg_.wifi_info.string1) - 1);
    strncpy(resp.wifi_info.string2, menu_cfg_.wifi_info.string2, sizeof(menu_cfg_.wifi_info.string2) - 1);
    strncpy(resp.system_version_info.string1, menu_cfg_.system_version_info.string1, sizeof(menu_cfg_.system_version_info.string1) - 1);

    /*preview*/
    resp.camera.current = menu_cfg_.camera.current;
    resp.crop0.current = menu_cfg_.crop0.current;
    resp.crop1.current = menu_cfg_.crop1.current;

    /*reverse line*/
    resp.reverseline_id.current = menu_cfg_.reverseline_id.current;
    return 0;
}

int MenuConfigLua::GetMenuIndexConfig(int msg)
{
    int val=0;
    switch(msg)
    {
        case SETTING_RECORD_RESOLUTION:
            val = menu_cfg_.record_resolution.current;
        break;

        case SETTING_RECORD_TIME:
            val = menu_cfg_.record_time.current;
        break;

        case SETTING_RECORD_LOOP_SWITCH:
            val = menu_cfg_.record_loop_switch.current;
        break;

        case SETTING_EMER_RECORD_SENSITIVITY:
            val = menu_cfg_.record_emerrecordsen.current;
        break;

        case SETTING_TIMEWATERMARK:
            val = menu_cfg_.record_timewatermark.current;
        break;

        case SETTING_RECORD_VOLUME_SWITCH:
            db_error("val is %d",menu_cfg_.record_sound.current);
            val = menu_cfg_.record_sound.current;
        break;

        case SETTING_CAMERA_EXPOSURE:
            val = menu_cfg_.camera_exposure.current;
        break;

        case SETTING_CAMERA_LIGHTSOURCEFREQUENCY:
            val = menu_cfg_.camera_lightfreq.current;
        break;

        case  SETTING_ADAS_SWITCH:
            val = menu_cfg_.record_adas_switch.current;
        break;

        case SETTING_ADAS_CALIBRATION:
            val = menu_cfg_.record_adas_calibration.current;
        break;

        case SETTING_CAMERA_AUTOSCREENSAVER:
            val = menu_cfg_.camera_autoscreensaver.current;
        break;

        case SETTING_DEVICE_LANGUAGE:
            val = menu_cfg_.device_language.current;
        break;

        case SETTING_PARKING_MONITORY:
            val = menu_cfg_.parking_monitory.current;
        break;

        case SETTING_VOICE_CTRL:
            val = menu_cfg_.voice_ctrl.current;
        break;

        case SETTING_VOLUME_SELECTION:
            val = menu_cfg_.record_volume_switch.current;
        break;

        case SETTING_WIFI_SWITCH:
            val = menu_cfg_.wifi_switch.current;
        break;

        case SETTING_PREVIEW_CAMERA:
            val = menu_cfg_.camera.current;
        break;

        case SETTING_PREVIEW_CROP0:
            val = menu_cfg_.crop0.current;
        break;

        case SETTING_PREVIEW_CROP1:
            val = menu_cfg_.crop1.current;
        break;

        case SETTINT_REVERSELINEID:
            val = menu_cfg_.reverseline_id.current;
        break;
    }
    return val;

}


int MenuConfigLua::GetMenuIndexCountConfig(int msg)
{
    int val=0;
    switch(msg)
    {
        case SETTING_PREVIEW_CROP0_COUNT:
            val = menu_cfg_.crop0.count;
        break;
        case SETTING_PREVIEW_CROP1_COUNT:
            val = menu_cfg_.crop1.count;
        break;
    }
    return val;
}
int MenuConfigLua::ResetMenuConfig(void)
{

    system("cp -f /usr/share/app/sdv/menu_config.lua /tmp/data/");

    db_msg("[fangjj]:config file %s  reset, copy default from /usr/share/app/sdv", MENU_CONFIG_FILE);
    int ret = LoadMenuConfig();
    if (ret) {
        db_msg("[fangjj]:[FUN]:%s [LINE]:%d  Do Reset LoadMenuConfig fail:%d !\n", __func__, __LINE__, ret);
    }

    return 0;
}
bool MenuConfigLua::IsVersionSame(std::string external_version,std::string local_version)
{
		//V-1.00.15d26_CN_debug
		//V-1.00.15d26_CN
	if(external_version.empty() || local_version.empty())
	{
		db_error("%s  external_version or local_version is empty",__func__);
		return false;
	}
	db_warn("IsNewVersion    external_version : %s  ---  local_version : %s",external_version.c_str(),local_version.c_str());
	//pars external_version
	string::size_type e_rc_start = external_version.rfind("V-");
	if( e_rc_start == string::npos)
	{
		db_warn("invalid fileName:%s",external_version.c_str());
		return false;
	}
	string::size_type e_rc_end = external_version.rfind("d26");
	if( e_rc_end == string::npos)
	{
		db_warn("invalid fileName:%s",external_version.c_str());
		return false;
	}
	string e_str = external_version.substr(e_rc_start+2,e_rc_end-(e_rc_start+2) );
	
	db_warn("pars external_version : %s",e_str.c_str());
	int e_num_first = 0, e_num_second = 0, e_num_third = 0;
	sscanf(e_str.c_str(),"%i.%i.%i",&e_num_first,&e_num_second,&e_num_third);
	db_warn("pars external version value : %d  %2d  %2d",e_num_first,e_num_second,e_num_third);
	
	//pars local version
	string::size_type local_rc_start = local_version.rfind("V-");
	if( local_rc_start == string::npos)
	{
		db_warn("invalid fileName:%s",local_version.c_str());
		return false;
	}
	string::size_type local_rc_end = local_version.rfind("d26");
	if( local_rc_end == string::npos)
	{
		db_warn("invalid fileName:%s",local_version.c_str());
		return false;
	}
	string local_str = local_version.substr(local_rc_start+2,local_rc_end-(local_rc_start+2) );

	db_warn("pars local_version : %s",local_str.c_str());
	int l_num_first = 0, l_num_second = 0, l_num_third = 0;
	sscanf(local_str.c_str(),"%i.%i.%i",&l_num_first,&l_num_second,&l_num_third);
	db_warn("pars local version value : %d  %2d  %2d",l_num_first,l_num_second,l_num_third);

	//cmp the version 
	if(e_num_first != l_num_first)
	{
		return false;
	}
	else //e_num_first == l_num_first
	{	 if(e_num_second !=  l_num_second)
		{
			return false;
		}
		else //e_num_second ==  l_num_second
		{
			if(e_num_third != l_num_third)
			{
				return false;
			}
			else //e_num_third == l_num_third
			{
				return true;
			}
		}
	}
	
	return true;
}

int MenuConfigLua::ChangeMenuConfig()
{
    db_warn(" ready to ChangeMenuConfig-----------------------");
    LuaConfig usr_luacfg, data_luacfg;
    char temp[128]={0};
    int ret = -1;
    if (!FILE_EXIST(MENU_CONFIG_FILE_USR))
    {
        db_warn("config file %s not exist", MENU_CONFIG_FILE_USR);
        return -1;
    }
    snprintf(temp,sizeof(temp),"cp -f %s %s",MENU_CONFIG_FILE_USR,MENU_CONFIG_FILE_USR_TEMP);
    system(temp);

    if (!FILE_EXIST(MENU_CONFIG_FILE_USR_TEMP))
    {
        db_warn("config file %s not exist", MENU_CONFIG_FILE_USR_TEMP);
        return -1;
    }
    ret = usr_luacfg.LoadFromFile(MENU_CONFIG_FILE_USR_TEMP);
    if(ret < 0)
    {
        db_warn("Load %s failed", MENU_CONFIG_FILE_USR_TEMP);
        //remove the menu_config_temp.lua
        memset(temp,0,sizeof(temp));
        snprintf(temp,sizeof(temp),"rm -f %s",MENU_CONFIG_FILE_USR_TEMP);
        system(temp);
        return -1;
    }

    std::string usr_update_str = usr_luacfg.GetStringValue("menu.system.update_menu");
    std::string usr_ver_str = usr_luacfg.GetStringValue("menu.system.version_info");
    db_warn("usr_update_str = %s   usr_ver_str = %s",usr_update_str.c_str(),usr_ver_str.c_str());

    if (!FILE_EXIST(MENU_CONFIG_FILE))
    {
        db_warn("config file %s not exist, copy from data", MENU_CONFIG_FILE);
        memset(temp,0,sizeof(temp));
        snprintf(temp,sizeof(temp),"cp -f %s /tmp/data/",MENU_CONFIG_FILE_USR);
        system(temp);
        //remove the menu_config_temp.lua
        memset(temp,0,sizeof(temp));
        snprintf(temp,sizeof(temp),"rm -f %s",MENU_CONFIG_FILE_USR_TEMP);
        system(temp);
        return 0;
    }
    ret = data_luacfg.LoadFromFile(MENU_CONFIG_FILE);
    if (ret < 0)
    {
        db_warn(" Load %s failed , ready to cp /usr/share/app/sdv/menu_config.lua /tmp/data/", MENU_CONFIG_FILE);
        memset(temp,0,sizeof(temp));
        snprintf(temp,sizeof(temp),"cp -f %s /tmp/data/",MENU_CONFIG_FILE_USR);
        system(temp);
        //remove the menu_config_temp.lua
        memset(temp,0,sizeof(temp));
        snprintf(temp,sizeof(temp),"rm -f %s",MENU_CONFIG_FILE_USR_TEMP);
        system(temp);
        return 0;
    }

    std::string data_ver_str = data_luacfg.GetStringValue("menu.system.version_info");
    if(IsVersionSame(usr_ver_str,data_ver_str) == true)
    {
        db_warn("version is the same , no to do anything");
        //remove the menu_config_temp.lua
        memset(temp,0,sizeof(temp));
        snprintf(temp,sizeof(temp),"rm -f %s",MENU_CONFIG_FILE_USR_TEMP);
        system(temp);
        return 0;
    }
    else
    {
        if( strcmp(usr_update_str.c_str(), "true") == 0 )
        {
            db_warn("update is true ,need to force cover the /tmp/data/menu_config.lua");
            memset(temp,0,sizeof(temp));
            snprintf(temp,sizeof(temp),"cp -f %s /tmp/data/",MENU_CONFIG_FILE_USR);
            system(temp);
            //remove the menu_config_temp.lua
            memset(temp,0,sizeof(temp));
            snprintf(temp,sizeof(temp),"rm -f %s",MENU_CONFIG_FILE_USR_TEMP);
            system(temp);
            return 0;
        }
        else
        {
            if(ComparisonProfile(&usr_luacfg ,&data_luacfg) < 0)
            {
                db_error("error: ComparisonProfile failed");
                return -1;
            }
            //mv menu_config_temp.lua to data/menu_config.lua
            memset(temp,0,sizeof(temp));
            snprintf(temp,sizeof(temp),"mv %s %s",MENU_CONFIG_FILE_USR_TEMP,MENU_CONFIG_FILE);
            system(temp);
            sync();
            return 0;
        }
    }
    //remove the menu_config_temp.lua
    memset(temp,0,sizeof(temp));
    snprintf(temp,sizeof(temp),"rm -f %s",MENU_CONFIG_FILE_USR_TEMP);
    system(temp);
    sync();
    return 0;
}

/******************************************************************
*function : ComparisonProfile(LuaConfig * usr_luacfg , LuaConfig * data_luacfg)
*return : -1 失败, 0 成功
*比较规则如下:
*I.如果usr 下面的count 大于data 下面的current ,
*   就有两种情况:
*	1.usr and data count 相等 ,这种情况直接使用data current.
*	2.usr count > data count ,这种情况也是直接使用data current
*
*II.如果usr count <= data count,这种情况直接使用usr current
*注意事项:
*	如果有添加子选项，建议添加到后面；而且不
*	能顺便调换自选项的顺序
********************************************************************/

int MenuConfigLua::ComparisonProfile(LuaConfig * usr_luacfg , LuaConfig * data_luacfg)
{
    if(usr_luacfg == NULL || data_luacfg == NULL)
    {
        db_error("error: ComparisonProfile usr_luacfg = %p",usr_luacfg,data_luacfg);
        return -1;
    }

    string str;
    int ret = 0;
    //read the usr_luacfg
    db_warn("ready to read out the data menu_config.lua data");
    /*device setting*/
    usr_temp_menu_cfg_.record_resolution.current  =    usr_luacfg->GetIntegerValue("menu.device.resolution.current");
    usr_temp_menu_cfg_.record_resolution.count  =    usr_luacfg->GetIntegerValue("menu.device.resolution.count");
    usr_temp_menu_cfg_.record_time.current  =    usr_luacfg->GetIntegerValue("menu.device.record_time.current");
    usr_temp_menu_cfg_.record_time.count   =   usr_luacfg->GetIntegerValue("menu.device.record_time.count");
    usr_temp_menu_cfg_.record_loop_switch.current  =    usr_luacfg->GetIntegerValue("menu.device.record_loop.current");
    usr_temp_menu_cfg_.record_loop_switch.count  =    usr_luacfg->GetIntegerValue("menu.device.record_loop.count");
    usr_temp_menu_cfg_.record_emerrecordsen.current  =    usr_luacfg->GetIntegerValue("menu.device.emerrecordsen.current");
    usr_temp_menu_cfg_.record_emerrecordsen.count   =   usr_luacfg->GetIntegerValue("menu.device.emerrecordsen.count");
    usr_temp_menu_cfg_.record_timewatermark.current  =    usr_luacfg->GetIntegerValue("menu.device.timewatermark.current");
    usr_temp_menu_cfg_.record_timewatermark.count  =    usr_luacfg->GetIntegerValue("menu.device.timewatermark.count");
    usr_temp_menu_cfg_.record_sound.current  =    usr_luacfg->GetIntegerValue("menu.device.record_sound.current");
    usr_temp_menu_cfg_.record_sound.count   =   usr_luacfg->GetIntegerValue("menu.device.record_sound.count");
    usr_temp_menu_cfg_.camera_exposure.current  =    usr_luacfg->GetIntegerValue("menu.device.exposure.current");
    usr_temp_menu_cfg_.camera_exposure.count  =    usr_luacfg->GetIntegerValue("menu.device.exposure.count");
    usr_temp_menu_cfg_.camera_lightfreq.current  =    usr_luacfg->GetIntegerValue("menu.device.lightfreq.current");
    usr_temp_menu_cfg_.camera_lightfreq.count   =   usr_luacfg->GetIntegerValue("menu.device.lightfreq.count");

    /*ADAS setting*/
    usr_temp_menu_cfg_.record_adas_switch.current  =    usr_luacfg->GetIntegerValue("menu.adas.adas_switch.current");
    usr_temp_menu_cfg_.record_adas_switch.count   =   usr_luacfg->GetIntegerValue("menu.adas.adas_switch.count");
    usr_temp_menu_cfg_.record_adas_calibration.current  =    usr_luacfg->GetIntegerValue("menu.adas.adas_calibration.current");
    usr_temp_menu_cfg_.record_adas_calibration.count   =   usr_luacfg->GetIntegerValue("menu.adas.adas_calibration.count");

    /*general setting*/
    usr_temp_menu_cfg_.camera_autoscreensaver.current  =    usr_luacfg->GetIntegerValue("menu.general.autoscreensaver.current");
    usr_temp_menu_cfg_.camera_autoscreensaver.count   =   usr_luacfg->GetIntegerValue("menu.general.autoscreensaver.count");
    usr_temp_menu_cfg_.device_language.current  =    usr_luacfg->GetIntegerValue("menu.general.language.current");
    usr_temp_menu_cfg_.device_language.count   =   usr_luacfg->GetIntegerValue("menu.general.language.count");
    usr_temp_menu_cfg_.parking_monitory.current  =    usr_luacfg->GetIntegerValue("menu.general.parking_monitory.current");
    usr_temp_menu_cfg_.parking_monitory.count   =   usr_luacfg->GetIntegerValue("menu.general.parking_monitory.count");
    usr_temp_menu_cfg_.voice_ctrl.current  =    usr_luacfg->GetIntegerValue("menu.general.voice_ctrl.current");
    usr_temp_menu_cfg_.voice_ctrl.count   =   usr_luacfg->GetIntegerValue("menu.general.voice_ctrl.count");
    usr_temp_menu_cfg_.record_volume_switch.current  =    usr_luacfg->GetIntegerValue("menu.general.system_volume.current");
    usr_temp_menu_cfg_.record_volume_switch.count   =   usr_luacfg->GetIntegerValue("menu.general.system_volume.count");
    usr_temp_menu_cfg_.wifi_switch.current  =    usr_luacfg->GetIntegerValue("menu.general.wifi_switch.current");
    usr_temp_menu_cfg_.wifi_switch.count   =   usr_luacfg->GetIntegerValue("menu.general.wifi_switch.count");

    str = usr_luacfg->GetStringValue("menu.system.wifi_info.ssid");
    strncpy(usr_temp_menu_cfg_.wifi_info.string1, str.c_str(), sizeof(usr_temp_menu_cfg_.wifi_info.string1) - 1);
    str = usr_luacfg->GetStringValue("menu.system.wifi_info.password");
    strncpy(usr_temp_menu_cfg_.wifi_info.string2, str.c_str(), sizeof(usr_temp_menu_cfg_.wifi_info.string2) - 1);
    str = usr_luacfg->GetStringValue("menu.system.version_info");
    strncpy(usr_temp_menu_cfg_.system_version_info.string1, str.c_str(), sizeof(usr_temp_menu_cfg_.system_version_info.string1) - 1);

    /*preview*/
    usr_temp_menu_cfg_.camera.count  =    usr_luacfg->GetIntegerValue("menu.preview.camera.count");
    usr_temp_menu_cfg_.camera.current  =    usr_luacfg->GetIntegerValue("menu.preview.camera.current");
    usr_temp_menu_cfg_.crop0.count  =    usr_luacfg->GetIntegerValue("menu.preview.crop0.count");
    usr_temp_menu_cfg_.crop0.current  =    usr_luacfg->GetIntegerValue("menu.preview.crop0.current");
    usr_temp_menu_cfg_.crop1.count  =    usr_luacfg->GetIntegerValue("menu.preview.crop1.count");
    usr_temp_menu_cfg_.crop1.current  =    usr_luacfg->GetIntegerValue("menu.preview.crop1.current");

    /*reverse line*/
    usr_temp_menu_cfg_.reverseline_id.count  =    usr_luacfg->GetIntegerValue("menu.reverseline_id.line_id.count");
    usr_temp_menu_cfg_.reverseline_id.current  =    usr_luacfg->GetIntegerValue("menu.reverseline_id.line_id.current");

/*----------------------------------------------------------------------------------------------------------*/
    //read the data_luacfg
    db_warn("ready to read out the data menu_config.lua data");
    /*device setting*/
    data_menu_cfg_.record_resolution.current  =    data_luacfg->GetIntegerValue("menu.device.resolution.current");
    data_menu_cfg_.record_resolution.count  =    data_luacfg->GetIntegerValue("menu.device.resolution.count");
    data_menu_cfg_.record_time.current  =    data_luacfg->GetIntegerValue("menu.device.record_time.current");
    data_menu_cfg_.record_time.count   =   data_luacfg->GetIntegerValue("menu.device.record_time.count");
    data_menu_cfg_.record_loop_switch.current  =    data_luacfg->GetIntegerValue("menu.device.record_loop.current");
    data_menu_cfg_.record_loop_switch.count  =    data_luacfg->GetIntegerValue("menu.device.record_loop.count");
    data_menu_cfg_.record_emerrecordsen.current  =    data_luacfg->GetIntegerValue("menu.device.emerrecordsen.current");
    data_menu_cfg_.record_emerrecordsen.count   =   data_luacfg->GetIntegerValue("menu.device.emerrecordsen.count");
    data_menu_cfg_.record_timewatermark.current  =    data_luacfg->GetIntegerValue("menu.device.timewatermark.current");
    data_menu_cfg_.record_timewatermark.count  =    data_luacfg->GetIntegerValue("menu.device.timewatermark.count");
    data_menu_cfg_.record_sound.current  =    data_luacfg->GetIntegerValue("menu.device.record_sound.current");
    data_menu_cfg_.record_sound.count   =   data_luacfg->GetIntegerValue("menu.device.record_sound.count");
    data_menu_cfg_.camera_exposure.current  =    data_luacfg->GetIntegerValue("menu.device.exposure.current");
    data_menu_cfg_.camera_exposure.count  =    data_luacfg->GetIntegerValue("menu.device.exposure.count");
    data_menu_cfg_.camera_lightfreq.current  =    data_luacfg->GetIntegerValue("menu.device.lightfreq.current");
    data_menu_cfg_.camera_lightfreq.count   =   data_luacfg->GetIntegerValue("menu.device.lightfreq.count");

    /*ADAS setting*/
    data_menu_cfg_.record_adas_switch.current  =    data_luacfg->GetIntegerValue("menu.adas.adas_switch.current");
    data_menu_cfg_.record_adas_switch.count   =   data_luacfg->GetIntegerValue("menu.adas.adas_switch.count");
    data_menu_cfg_.record_adas_calibration.current  =    data_luacfg->GetIntegerValue("menu.adas.adas_calibration.current");
    data_menu_cfg_.record_adas_calibration.count   =   data_luacfg->GetIntegerValue("menu.adas.adas_calibration.count");

    /*general setting*/
    data_menu_cfg_.camera_autoscreensaver.current  =    data_luacfg->GetIntegerValue("menu.general.autoscreensaver.current");
    data_menu_cfg_.camera_autoscreensaver.count   =   data_luacfg->GetIntegerValue("menu.general.autoscreensaver.count");
    data_menu_cfg_.device_language.current  =    data_luacfg->GetIntegerValue("menu.general.language.current");
    data_menu_cfg_.device_language.count   =   data_luacfg->GetIntegerValue("menu.general.language.count");
    data_menu_cfg_.parking_monitory.current  =    data_luacfg->GetIntegerValue("menu.general.parking_monitory.current");
    data_menu_cfg_.parking_monitory.count   =   data_luacfg->GetIntegerValue("menu.general.parking_monitory.count");
    data_menu_cfg_.voice_ctrl.current  =    data_luacfg->GetIntegerValue("menu.general.voice_ctrl.current");
    data_menu_cfg_.voice_ctrl.count   =   data_luacfg->GetIntegerValue("menu.general.voice_ctrl.count");
    data_menu_cfg_.record_volume_switch.current  =    data_luacfg->GetIntegerValue("menu.general.system_volume.current");
    data_menu_cfg_.record_volume_switch.count   =   data_luacfg->GetIntegerValue("menu.general.system_volume.count");
    data_menu_cfg_.wifi_switch.current  =    data_luacfg->GetIntegerValue("menu.general.wifi_switch.current");
    data_menu_cfg_.wifi_switch.count   =   data_luacfg->GetIntegerValue("menu.general.wifi_switch.count");

    str = usr_luacfg->GetStringValue("menu.system.wifi_info.ssid");
    strncpy(usr_temp_menu_cfg_.wifi_info.string1, str.c_str(), sizeof(usr_temp_menu_cfg_.wifi_info.string1) - 1);
    str = usr_luacfg->GetStringValue("menu.system.wifi_info.password");
    strncpy(usr_temp_menu_cfg_.wifi_info.string2, str.c_str(), sizeof(usr_temp_menu_cfg_.wifi_info.string2) - 1);
    str = usr_luacfg->GetStringValue("menu.system.version_info");
    strncpy(usr_temp_menu_cfg_.system_version_info.string1, str.c_str(), sizeof(usr_temp_menu_cfg_.system_version_info.string1) - 1);

    /*preview*/
    data_menu_cfg_.camera.count  =    data_luacfg->GetIntegerValue("menu.preview.camera.count");
    data_menu_cfg_.camera.current  =    data_luacfg->GetIntegerValue("menu.preview.camera.current");
    data_menu_cfg_.crop0.count  =    data_luacfg->GetIntegerValue("menu.preview.crop0.count");
    data_menu_cfg_.crop0.current  =    data_luacfg->GetIntegerValue("menu.preview.crop0.current");
    data_menu_cfg_.crop1.count  =    data_luacfg->GetIntegerValue("menu.preview.crop1.count");
    data_menu_cfg_.crop1.current  =    data_luacfg->GetIntegerValue("menu.preview.crop1.current");

    /*reverse line*/
    data_menu_cfg_.reverseline_id.count  =    data_luacfg->GetIntegerValue("menu.reverseline_id.line_id.count");
    data_menu_cfg_.reverseline_id.current  =    data_luacfg->GetIntegerValue("menu.reverseline_id.line_id.current");

    /*=================================================================================*/
    /*device setting*/
    if(data_menu_cfg_.record_resolution.count  != DEFAULT_VALUE  && data_menu_cfg_.record_resolution.current  != DEFAULT_VALUE)
        if(usr_temp_menu_cfg_.record_resolution.count > data_menu_cfg_.record_resolution.current)
            usr_temp_menu_cfg_.record_resolution.current = data_menu_cfg_.record_resolution.current;
    if(data_menu_cfg_.record_time.count  != DEFAULT_VALUE  && data_menu_cfg_.record_time.current  != DEFAULT_VALUE)
        if(usr_temp_menu_cfg_.record_time.count > data_menu_cfg_.record_time.current)
            usr_temp_menu_cfg_.record_time.current = data_menu_cfg_.record_time.current;
    if(data_menu_cfg_.record_loop_switch.count  != DEFAULT_VALUE  && data_menu_cfg_.record_loop_switch.current  != DEFAULT_VALUE)
        if(usr_temp_menu_cfg_.record_loop_switch.count > data_menu_cfg_.record_loop_switch.current)
            usr_temp_menu_cfg_.record_loop_switch.current = data_menu_cfg_.record_loop_switch.current;
    if(data_menu_cfg_.record_emerrecordsen.count  != DEFAULT_VALUE  && data_menu_cfg_.record_emerrecordsen.current  != DEFAULT_VALUE)
        if(usr_temp_menu_cfg_.record_emerrecordsen.count > data_menu_cfg_.record_emerrecordsen.current)
            usr_temp_menu_cfg_.record_emerrecordsen.current = data_menu_cfg_.record_emerrecordsen.current;
    if(data_menu_cfg_.record_timewatermark.count  != DEFAULT_VALUE  && data_menu_cfg_.record_timewatermark.current  != DEFAULT_VALUE)
        if(usr_temp_menu_cfg_.record_timewatermark.count > data_menu_cfg_.record_timewatermark.current)
            usr_temp_menu_cfg_.record_timewatermark.current = data_menu_cfg_.record_timewatermark.current;
    if(data_menu_cfg_.record_sound.count  != DEFAULT_VALUE  && data_menu_cfg_.record_sound.current  != DEFAULT_VALUE)
        if(usr_temp_menu_cfg_.record_sound.count > data_menu_cfg_.record_sound.current)
            usr_temp_menu_cfg_.record_sound.current = data_menu_cfg_.record_sound.current;
    if(data_menu_cfg_.camera_exposure.count  != DEFAULT_VALUE  && data_menu_cfg_.camera_exposure.current  != DEFAULT_VALUE)
        if(usr_temp_menu_cfg_.camera_exposure.count > data_menu_cfg_.camera_exposure.current)
            usr_temp_menu_cfg_.camera_exposure.current = data_menu_cfg_.camera_exposure.current;
    if(data_menu_cfg_.camera_lightfreq.count  != DEFAULT_VALUE  && data_menu_cfg_.camera_lightfreq.current  != DEFAULT_VALUE)
       if(usr_temp_menu_cfg_.camera_lightfreq.count > data_menu_cfg_.camera_lightfreq.current)
           usr_temp_menu_cfg_.camera_lightfreq.current = data_menu_cfg_.camera_lightfreq.current;

    /*ADAS setting*/
    if(data_menu_cfg_.record_adas_switch.count  != DEFAULT_VALUE  && data_menu_cfg_.record_adas_switch.current  != DEFAULT_VALUE)
       if(usr_temp_menu_cfg_.record_adas_switch.count > data_menu_cfg_.record_adas_switch.current)
           usr_temp_menu_cfg_.record_adas_switch.current = data_menu_cfg_.record_adas_switch.current;
    if(data_menu_cfg_.record_adas_calibration.count  != DEFAULT_VALUE  && data_menu_cfg_.record_adas_calibration.current  != DEFAULT_VALUE)
           if(usr_temp_menu_cfg_.record_adas_calibration.count > data_menu_cfg_.record_adas_calibration.current)
               usr_temp_menu_cfg_.record_adas_calibration.current = data_menu_cfg_.record_adas_calibration.current;

    /*general setting*/
    if(data_menu_cfg_.camera_autoscreensaver.count  != DEFAULT_VALUE  && data_menu_cfg_.camera_autoscreensaver.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.camera_autoscreensaver.count > data_menu_cfg_.camera_autoscreensaver.current)
              usr_temp_menu_cfg_.camera_autoscreensaver.current = data_menu_cfg_.camera_autoscreensaver.current;
    if(data_menu_cfg_.parking_monitory.count  != DEFAULT_VALUE  && data_menu_cfg_.parking_monitory.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.parking_monitory.count > data_menu_cfg_.parking_monitory.current)
              usr_temp_menu_cfg_.parking_monitory.current = data_menu_cfg_.parking_monitory.current;
    if(data_menu_cfg_.voice_ctrl.count  != DEFAULT_VALUE  && data_menu_cfg_.voice_ctrl.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.voice_ctrl.count > data_menu_cfg_.voice_ctrl.current)
              usr_temp_menu_cfg_.voice_ctrl.current = data_menu_cfg_.voice_ctrl.current;
    if(data_menu_cfg_.device_language.count  != DEFAULT_VALUE  && data_menu_cfg_.device_language.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.device_language.count > data_menu_cfg_.device_language.current)
              usr_temp_menu_cfg_.device_language.current = data_menu_cfg_.device_language.current;
    if(data_menu_cfg_.record_volume_switch.count  != DEFAULT_VALUE  && data_menu_cfg_.record_volume_switch.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.record_volume_switch.count > data_menu_cfg_.record_volume_switch.current)
              usr_temp_menu_cfg_.record_volume_switch.current = data_menu_cfg_.record_volume_switch.current;
    if(data_menu_cfg_.wifi_switch.count  != DEFAULT_VALUE  && data_menu_cfg_.wifi_switch.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.wifi_switch.count > data_menu_cfg_.wifi_switch.current)
              usr_temp_menu_cfg_.wifi_switch.current = data_menu_cfg_.wifi_switch.current;

    /*preview*/
    if(data_menu_cfg_.camera.count  != DEFAULT_VALUE  && data_menu_cfg_.camera.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.camera.count > data_menu_cfg_.camera.current)
              usr_temp_menu_cfg_.camera.current = data_menu_cfg_.camera.current;
    if(data_menu_cfg_.crop0.count  != DEFAULT_VALUE  && data_menu_cfg_.crop0.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.crop0.count > data_menu_cfg_.crop0.current)
              usr_temp_menu_cfg_.crop0.current = data_menu_cfg_.crop0.current;
    if(data_menu_cfg_.crop1.count  != DEFAULT_VALUE  && data_menu_cfg_.crop1.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.crop1.count > data_menu_cfg_.crop1.current)
              usr_temp_menu_cfg_.crop1.current = data_menu_cfg_.crop1.current;

    /*reverse line*/
    if(data_menu_cfg_.reverseline_id.count  != DEFAULT_VALUE  && data_menu_cfg_.reverseline_id.current  != DEFAULT_VALUE)
          if(usr_temp_menu_cfg_.reverseline_id.count > data_menu_cfg_.reverseline_id.current)
              usr_temp_menu_cfg_.reverseline_id.current = data_menu_cfg_.reverseline_id.current;

	db_warn("ready to set the data to the usr_luacfg");
	//set to the usr_luacfg
	/*device setting*/
    usr_luacfg->SetIntegerValue("menu.device.resolution.current",  usr_temp_menu_cfg_.record_resolution.current);
    usr_luacfg->SetIntegerValue("menu.device.resolution.count",  usr_temp_menu_cfg_.record_resolution.count);
    usr_luacfg->SetIntegerValue("menu.device.record_time.current",  usr_temp_menu_cfg_.record_time.current);
    usr_luacfg->SetIntegerValue("menu.device.record_time.count",  usr_temp_menu_cfg_.record_time.count);
    usr_luacfg->SetIntegerValue("menu.device.record_loop.current",  usr_temp_menu_cfg_.record_loop_switch.current);
    usr_luacfg->SetIntegerValue("menu.device.record_loop.count",  usr_temp_menu_cfg_.record_loop_switch.count);
    usr_luacfg->SetIntegerValue("menu.device.emerrecordsen.current",  usr_temp_menu_cfg_.record_emerrecordsen.current);
    usr_luacfg->SetIntegerValue("menu.device.emerrecordsen.count",  usr_temp_menu_cfg_.record_emerrecordsen.count);
    usr_luacfg->SetIntegerValue("menu.device.timewatermark.current",  usr_temp_menu_cfg_.record_timewatermark.current);
    usr_luacfg->SetIntegerValue("menu.device.timewatermark.count",  usr_temp_menu_cfg_.record_timewatermark.count);
    usr_luacfg->SetIntegerValue("menu.device.record_sound.current",  usr_temp_menu_cfg_.record_sound.current);
    usr_luacfg->SetIntegerValue("menu.device.record_sound.count",  usr_temp_menu_cfg_.record_sound.count);
    usr_luacfg->SetIntegerValue("menu.device.exposure.current",  usr_temp_menu_cfg_.camera_exposure.current);
    usr_luacfg->SetIntegerValue("menu.device.exposure.count",  usr_temp_menu_cfg_.camera_exposure.count);
    usr_luacfg->SetIntegerValue("menu.device.lightfreq.current",  usr_temp_menu_cfg_.camera_lightfreq.current);
    usr_luacfg->SetIntegerValue("menu.device.lightfreq.count",  usr_temp_menu_cfg_.camera_lightfreq.count);

    /*ADAS setting*/
    usr_luacfg->SetIntegerValue("menu.adas.adas_switch.current",  usr_temp_menu_cfg_.record_adas_switch.current);
    usr_luacfg->SetIntegerValue("menu.adas.adas_switch.count",  usr_temp_menu_cfg_.record_adas_switch.count);
    usr_luacfg->SetIntegerValue("menu.adas.adas_calibration.current",  usr_temp_menu_cfg_.record_adas_calibration.current);
    usr_luacfg->SetIntegerValue("menu.adas.adas_calibration.count",  usr_temp_menu_cfg_.record_adas_calibration.count);

    /*general setting*/
    usr_luacfg->SetIntegerValue("menu.general.autoscreensaver.current",  usr_temp_menu_cfg_.camera_autoscreensaver.current);
    usr_luacfg->SetIntegerValue("menu.general.autoscreensaver.count",  usr_temp_menu_cfg_.camera_autoscreensaver.count);
    usr_luacfg->SetIntegerValue("menu.general.parking_monitory.current",  usr_temp_menu_cfg_.parking_monitory.current);
    usr_luacfg->SetIntegerValue("menu.general.parking_monitory.count",  usr_temp_menu_cfg_.parking_monitory.count);
    usr_luacfg->SetIntegerValue("menu.general.voice_ctrl.current",  usr_temp_menu_cfg_.voice_ctrl.current);
    usr_luacfg->SetIntegerValue("menu.general.voice_ctrl.count",  usr_temp_menu_cfg_.voice_ctrl.count);
    usr_luacfg->SetIntegerValue("menu.general.language.current",  usr_temp_menu_cfg_.device_language.current);
    usr_luacfg->SetIntegerValue("menu.general.language.count",  usr_temp_menu_cfg_.device_language.count);
    usr_luacfg->SetIntegerValue("menu.general.system_volume.current",  usr_temp_menu_cfg_.record_volume_switch.current);
    usr_luacfg->SetIntegerValue("menu.general.system_volume.count",  usr_temp_menu_cfg_.record_volume_switch.count);
    usr_luacfg->SetIntegerValue("menu.general.wifi_switch.current",  usr_temp_menu_cfg_.wifi_switch.current);
    usr_luacfg->SetIntegerValue("menu.general.wifi_switch.count",  usr_temp_menu_cfg_.wifi_switch.count);

    /*preview*/
    usr_luacfg->SetIntegerValue("menu.preview.camera.current",  usr_temp_menu_cfg_.camera.current);
    usr_luacfg->SetIntegerValue("menu.preview.camera.count",  usr_temp_menu_cfg_.camera.count);
    usr_luacfg->SetIntegerValue("menu.preview.crop0.current",  usr_temp_menu_cfg_.crop0.current);
    usr_luacfg->SetIntegerValue("menu.preview.crop0.count",  usr_temp_menu_cfg_.crop0.count);
    usr_luacfg->SetIntegerValue("menu.preview.crop1.current",  usr_temp_menu_cfg_.crop1.current);
    usr_luacfg->SetIntegerValue("menu.preview.crop1.count",  usr_temp_menu_cfg_.crop1.count);

    /*reverse line*/
    usr_luacfg->SetIntegerValue("menu.reverseline_id.line_id.current",  usr_temp_menu_cfg_.reverseline_id.current);
    usr_luacfg->SetIntegerValue("menu.reverseline_id.line_id.count",  usr_temp_menu_cfg_.reverseline_id.count);

    str.clear();//wifi ssid is "" the first time
    str = data_luacfg->GetStringValue("menu.system.wifi_info.ssid");
    strncpy(usr_temp_menu_cfg_.wifi_info.string1, str.c_str(), sizeof(usr_temp_menu_cfg_.wifi_info.string1) - 1);
    str.clear();
    str  = data_luacfg->GetStringValue("menu.system.wifi_info.password");
    if(!str.empty())
        strncpy(usr_temp_menu_cfg_.wifi_info.string2, str.c_str(), sizeof(usr_temp_menu_cfg_.wifi_info.string2) - 1);
    str.clear();
    str = data_luacfg->GetStringValue("menu.system.version_info");
    if(!str.empty())
        strncpy(usr_temp_menu_cfg_.system_version_info.string1, str.c_str(), sizeof(usr_temp_menu_cfg_.system_version_info.string1) - 1);

    db_warn("ready to save the compare result to the usr/--/menu_config_temp.lua");
    ret = usr_luacfg->SyncConfigToFile(MENU_CONFIG_FILE_USR_TEMP, "menu");
    if (ret < 0)
    {
        db_error("Do SyncConfigToFile error! file:%s \n", MENU_CONFIG_FILE_USR_TEMP);
        return ret;
    }
    db_warn("Comparison is ok ");
    return ret;
}

int MenuConfigLua::LoadMenuConfig(void)
{
    int ret = 0;
    int cnt = 0, i = 0;
    char tmp_str[256] = {0};
    std::string str;

    if (NULL == this->lua_cfg_) {
        db_error("[fangjj]:The lua_cfg_ is NULL! error! \n");
        return -1;
    }

    if (!FILE_EXIST(MENU_CONFIG_FILE)) {
        db_warn("config file %s not exist, copy default from /usr/share/app/sdv", MENU_CONFIG_FILE);
        system("cp -f /usr/share/app/sdv/menu_config.lua /tmp/data/");
    }

    ret = lua_cfg_->LoadFromFile(MENU_CONFIG_FILE);
    if (ret) {
        db_warn("Load %s failed, copy backup and try again", MENU_CONFIG_FILE);
        system("cp -f /usr/share/app/sdv/menu_config.lua /tmp/data/");

        ret = lua_cfg_->LoadFromFile(MENU_CONFIG_FILE);
        if (ret) {
            db_error("[fangjj]:Load %s failed!", MENU_CONFIG_FILE);
            return -1;
        }
    }

    /*device setting*/
    menu_cfg_.record_resolution.current = lua_cfg_->GetIntegerValue("menu.device.resolution.current");
    menu_cfg_.record_resolution.count = lua_cfg_->GetIntegerValue("menu.device.resolution.count");
    menu_cfg_.record_time.current = lua_cfg_->GetIntegerValue("menu.device.record_time.current");
    menu_cfg_.record_time.count = lua_cfg_->GetIntegerValue("menu.device.record_time.count");
    menu_cfg_.record_loop_switch.current = lua_cfg_->GetIntegerValue("menu.device.record_loop.current");
    menu_cfg_.record_loop_switch.count = lua_cfg_->GetIntegerValue("menu.device.record_loop.count");
    menu_cfg_.record_emerrecordsen.current  =    lua_cfg_->GetIntegerValue("menu.device.emerrecordsen.current");
    menu_cfg_.record_emerrecordsen.count  =    lua_cfg_->GetIntegerValue("menu.device.emerrecordsen.count");
    menu_cfg_.record_timewatermark.current  =    lua_cfg_->GetIntegerValue("menu.device.timewatermark.current");
    menu_cfg_.record_timewatermark.count  =    lua_cfg_->GetIntegerValue("menu.device.timewatermark.count");
    menu_cfg_.record_sound.current  =    lua_cfg_->GetIntegerValue("menu.device.record_sound.current");
    menu_cfg_.record_sound.count  =    lua_cfg_->GetIntegerValue("menu.device.record_sound.count");
    menu_cfg_.camera_exposure.current  =    lua_cfg_->GetIntegerValue("menu.device.exposure.current");
    menu_cfg_.camera_exposure.count  =    lua_cfg_->GetIntegerValue("menu.device.exposure.count");
    menu_cfg_.camera_lightfreq.current  =    lua_cfg_->GetIntegerValue("menu.device.lightfreq.current");
    menu_cfg_.camera_lightfreq.count  =    lua_cfg_->GetIntegerValue("menu.device.lightfreq.count");

    /*ADAS setting*/
    menu_cfg_.record_adas_switch.current  =    lua_cfg_->GetIntegerValue("menu.adas.adas_switch.current");
    menu_cfg_.record_adas_switch.count  =    lua_cfg_->GetIntegerValue("menu.adas.adas_switch.count");
    menu_cfg_.record_adas_calibration.current  =    lua_cfg_->GetIntegerValue("menu.adas.adas_calibration.current");
    menu_cfg_.record_adas_calibration.count  =    lua_cfg_->GetIntegerValue("menu.adas.adas_calibration.count");

    /*general setting*/
    menu_cfg_.camera_autoscreensaver.current  =    lua_cfg_->GetIntegerValue("menu.general.autoscreensaver.current");
    menu_cfg_.camera_autoscreensaver.count  =    lua_cfg_->GetIntegerValue("menu.general.autoscreensaver.count");
    menu_cfg_.parking_monitory.current  =    lua_cfg_->GetIntegerValue("menu.general.parking_monitory.current");
    menu_cfg_.parking_monitory.count  =    lua_cfg_->GetIntegerValue("menu.general.parking_monitory.count");
    menu_cfg_.voice_ctrl.current  =    lua_cfg_->GetIntegerValue("menu.general.voice_ctrl.current");
    menu_cfg_.voice_ctrl.count  =    lua_cfg_->GetIntegerValue("menu.general.voice_ctrl.count");
    menu_cfg_.device_language.current  =    lua_cfg_->GetIntegerValue("menu.general.language.current");
    menu_cfg_.device_language.count  =    lua_cfg_->GetIntegerValue("menu.general.language.count");
    menu_cfg_.record_volume_switch.current  =    lua_cfg_->GetIntegerValue("menu.general.system_volume.current");
    menu_cfg_.record_volume_switch.count  =    lua_cfg_->GetIntegerValue("menu.general.system_volume.count");
    menu_cfg_.wifi_switch.current  =    lua_cfg_->GetIntegerValue("menu.general.wifi_switch.current");
    menu_cfg_.wifi_switch.count  =    lua_cfg_->GetIntegerValue("menu.general.wifi_switch.count");

    /*preview*/
    menu_cfg_.camera.current  =    lua_cfg_->GetIntegerValue("menu.preview.camera.current");
    menu_cfg_.camera.count  =    lua_cfg_->GetIntegerValue("menu.preview.camera.count");
    menu_cfg_.crop0.current  =    lua_cfg_->GetIntegerValue("menu.preview.crop0.current");
    menu_cfg_.crop0.count  =    lua_cfg_->GetIntegerValue("menu.preview.crop0.count");
    menu_cfg_.crop1.current  =    lua_cfg_->GetIntegerValue("menu.preview.crop1.current");
    menu_cfg_.crop1.count  =    lua_cfg_->GetIntegerValue("menu.preview.crop1.count");

    /*reverse line*/
    menu_cfg_.reverseline_id.current  =    lua_cfg_->GetIntegerValue("menu.reverseline_id.line_id.current");
    menu_cfg_.reverseline_id.count  =    lua_cfg_->GetIntegerValue("menu.reverseline_id.line_id.count");

    str = lua_cfg_->GetStringValue("menu.system.wifi_info.ssid");
    strncpy(menu_cfg_.wifi_info.string1, str.c_str(), sizeof(menu_cfg_.wifi_info.string1) - 1);
    str = lua_cfg_->GetStringValue("menu.system.wifi_info.password");
    strncpy(menu_cfg_.wifi_info.string2, str.c_str(), sizeof(menu_cfg_.wifi_info.string2) - 1);
    str = lua_cfg_->GetStringValue("menu.system.version_info");
    strncpy(menu_cfg_.system_version_info.string1, str.c_str(), sizeof(menu_cfg_.system_version_info.string1) - 1);
    str = lua_cfg_->GetStringValue("menu.general.systemTimeSave.timeStr");
    strncpy(menu_cfg_.device_systemTime.string1, str.c_str(), sizeof(menu_cfg_.device_systemTime.string1) - 1);
    return 0;
}

int MenuConfigLua::SetWifiSsid(std::string str)
{
     strncpy(menu_cfg_.wifi_info.string1, str.c_str(), sizeof(menu_cfg_.wifi_info.string1) - 1);
     lua_cfg_->SetStringValue("menu.system.wifi_info.ssid",  menu_cfg_.wifi_info.string1);
     return 0;
}

int MenuConfigLua::SetWifiPassword(std::string str)
{
     strncpy(menu_cfg_.wifi_info.string2, str.c_str(), sizeof(menu_cfg_.wifi_info.string2) - 1);
     lua_cfg_->SetStringValue("menu.system.wifi_info.password",  menu_cfg_.wifi_info.string2);
     return 0;
}


int MenuConfigLua::UpdateWifiInfo()
{
    if (NULL == this->lua_cfg_)
    {
        db_error("[fangjj]:The lua_cfg_ is NULL! error! \n");
        return -1;
    }

    int ret = lua_cfg_->LoadFromFile(MENU_CONFIG_FILE);
    if (ret)
    {
        db_error("Load %s failed!", MENU_CONFIG_FILE);
        return -1;
    }

    std::string str = lua_cfg_->GetStringValue("menu.system.wifi_info.ssid");
    strncpy(menu_cfg_.wifi_info.string1, str.c_str(), sizeof(menu_cfg_.wifi_info.string1) - 1);

    str = lua_cfg_->GetStringValue("menu.system.wifi_info.password");
    strncpy(menu_cfg_.wifi_info.string2, str.c_str(), sizeof(menu_cfg_.wifi_info.string2) - 1);
    
    return 0;
}
int MenuConfigLua::SetMenuStringConfig(int msg, std::string & str)
{
    switch(msg) {
               /*****switch******/
        case MSG_SHUTDOWN_SAVE_TIME:
		strncpy(menu_cfg_.device_systemTime.string1, str.c_str(), sizeof(menu_cfg_.device_systemTime.string1) - 1); 
	   	break;
          default:
         	 break;
      }    
      SaveMenuAllConfig();
      return 0; 
} 

int MenuConfigLua::UpdateSystemTime(bool flag)
{
    if(!flag)//刚开机的时候
    	system("hwclock -s");//将硬件的时间同步到系统时间
    char buf[32] = {0};
    struct tm * tm=NULL;
    time_t timer;
    timer = time(NULL);
    tm = localtime(&timer);
    snprintf(buf, sizeof(buf),"%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
	db_warn("debug_yxl---> buf = %s",buf);
    //get menu_config.lua time
    if(flag)//shutdown save timestr to menu_config.lua
   	{
		string temp;
		temp = buf;
		SetMenuStringConfig(MSG_SHUTDOWN_SAVE_TIME,temp);
        if(set_date_time(tm) < 0)
		{
			db_error("set system time fail");
			return -1;
	    }
   	}
	else
	{
	     string t_str;
		 t_str.clear();
         t_str = lua_cfg_->GetStringValue("menu.general.systemTimeSave.timeStr");
		 if( t_str.empty() )
		 {
		 	db_warn("get systemTimeSave is null");
			if(set_date_time(tm) < 0)
			{
				db_error("set system time fail");
				return -1;
            }
            system("hwclock -w");//将系统的时间同步到硬件时间
			return 0;
		 }
		
	    if(strncmp(t_str.c_str(),buf,strlen(t_str.c_str()))>=0)//menu_config.lua time >= system time
	    	{
			//2018-07-01 00:00:00
			struct tm tm_last;
			tm_last.tm_year = atoi((t_str.substr(0,4)).c_str()) - 1900;
			tm_last.tm_mon = atoi((t_str.substr(5,2)).c_str())-1;
			tm_last.tm_mday = atoi((t_str.substr(8,2)).c_str());
			tm_last.tm_hour = atoi((t_str.substr(11,2)).c_str());
			tm_last.tm_min = atoi((t_str.substr(14,2)).c_str());
			tm_last.tm_sec = atoi((t_str.substr(17,2)).c_str());
			tm_last.tm_wday = 0;
			tm_last.tm_yday = 0;
			tm_last.tm_isdst = 0;
			db_warn("debug_yxl---> %04d-%02d-%02d %02d:%02d:%02d",tm_last.tm_year+1900, tm_last.tm_mon+1, tm_last.tm_mday, tm_last.tm_hour, tm_last.tm_min, tm_last.tm_sec);
			if(set_date_time(&tm_last) < 0)
			{
				db_error("set system time fail");
				return -1;
	    		}
			system("hwclock -w");//将系统的时间同步到硬件时间
	    	}
	 }
	
	return 0;
}

