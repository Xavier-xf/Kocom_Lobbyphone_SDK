/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file playlist_window.cpp
 * @brief 多媒体回放列表窗口
 * @author id:826
 * @version v0.3
 * @date 2016-11-04
 */
#include "window/newSettingWindow.h"
#include "debug/app_log.h"
#include "widgets/graphic_view.h"
#include "window/window_manager.h"
#include "window/dialog.h"
#include "device_model/menu_config_lua.h"
#include "resource/resource_manager.h"
#include "window/time_setting_window_new.h"
#include "device_model/system/power_manager.h"
#include "bll_presenter/screensaver.h"
#include "bll_presenter/audioCtrl.h"
#include "common/setting_menu_id.h"
#include "widgets/listbox_impl.h"
#include "device_model/storage_manager.h"
#include "device_model/system/event_manager.h"
#include "window/promptBox.h"
#include "window/bulletCollection.h"
#include <pthread.h>
#include <time.h>
#include <sstream>
#include "device_model/version_update_manager.h"
#include "uilayer_view/gui/minigui/window/preview_window.h"
#include "window/prompt.h"
#include "device_model/partitionManager.h"
#include "device_model/download_4g_manager.h"
#include "bll_presenter/device_setting.h"
#include "dd_serv/common_define.h"
#include "widgets/list_view.h"
#include "sublist.h"
#include "info_dialog.h"
#include "bll_presenter/statusbarsaver.h"


#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "NewSettingWindow"

using namespace EyeseeLinux;
using namespace std;

IMPLEMENT_DYNCRT_CLASS(NewSettingWindow)
void NewSettingWindow::keyProc(int keyCode, int isLongPress)
{
#ifdef KEY_INTERACTION
    switch(keyCode){
        case SDV_KEY_MENU:
            if(sub_list_->GetVisible()){
                sub_list_->DoHide();
            }else{
                EyeseeLinux::StatusBarSaver::GetInstance()->Pause(false);
                mItemCurrentIndex_ = 0;
                listener_->sendmsg(this, MSG_SETTING_TO_PREVIEW, 0);
                listener_->sendmsg(this, WM_WINDOW_CHANGE, WINDOWID_PREVIEW);
            }
        break;
        case SDV_KEY_LEFT:
        {
            int ret = 0;
            #if 0
            if(isLongPress == LONG_PRESS){
                PostMsg(NULL, SETTING_OK_KEY_LONG_CLICK, 0);
            } else {
            }
            if (info_dialog_->GetVisible())
                info_dialog_->DoHide();
            #endif
            if(sub_list_->GetVisible()){
                SubListViewClickProc(NULL);
            }
            else{
                ListViewClickProc();
            }

        }
        break;
        case SDV_KEY_OK:
            {
                int ret = 0;
                mItemCurrentIndex_ = list_view_->GetHilight();
                mItemCurrentIndex_--;
                if(mItemCurrentIndex_ < 0)
                {
                    mItemCurrentIndex_ = 0;
                }
                db_msg("habo--> key up mItemCurrentIndex = %d",mItemCurrentIndex_);
                if(sub_list_->GetVisible())
                {
                    mItemSublistIndex_ = sub_list_view_->GetHilight();
                    mItemSublistIndex_--;
                    if(mItemSublistIndex_ < 0)
                    {
                        mItemSublistIndex_ = sub_list_view_->GetItemCont() -1;
                    }
                    db_msg("sub list index %d cont %d",mItemSublistIndex_,sub_list_view_->GetItemCont());
                    ret = sub_list_view_->SelectItem(mItemSublistIndex_);
                }
                else{
                    ret = list_view_->SelectItem(mItemCurrentIndex_);
                }
                if(ret < 0)
                {
                    db_error("SelectItem listview item failed");
                    if(mItemCurrentIndex_ == mItemNum -1)
                        mItemCurrentIndex_ = 0;
                    else
                        mItemCurrentIndex_++;
                }
            }
            break;
        case SDV_KEY_RIGHT:
            {
                int ret = 0;
                mItemCurrentIndex_ = list_view_->GetHilight();
                mItemCurrentIndex_++;
                if(mItemCurrentIndex_ >= mItemNum)
                {
                    mItemCurrentIndex_ = mItemNum;
                }
                db_msg("habo--> key down mItemCurrentIndex = %d",mItemCurrentIndex_);
                if(sub_list_->GetVisible())
                {
                    mItemSublistIndex_ = sub_list_view_->GetHilight();
                    mItemSublistIndex_++;
                    if(mItemSublistIndex_ >= sub_list_view_->GetItemCont())
                    {
                        mItemSublistIndex_ = 0;
                    }
                    db_msg("sub list index %d cont %d",mItemSublistIndex_,sub_list_view_->GetItemCont());
                    ret = sub_list_view_->SelectItem(mItemSublistIndex_);
                }
                else{
                    ret = list_view_->SelectItem(mItemCurrentIndex_);
                }
                if(ret < 0)
                {
                    db_error("SelectItem listview item failed");
                    if(mItemCurrentIndex_ == 0)
                        mItemCurrentIndex_ = mItemNum -1;
                    else
                        mItemCurrentIndex_--;
                }

            }
            break;
    }
#else
    switch(keyCode){
            case SDV_KEY_LEFT:
                break;
            case SDV_KEY_POWER:
                break;
            case SDV_KEY_OK:
                break;
            case SDV_KEY_RIGHT:
                break;
            case SDV_KEY_MENU:
                EyeseeLinux::StatusBarSaver::GetInstance()->Pause(false);
                listener_->sendmsg(this, MSG_SETTING_TO_PREVIEW, 0);
                listener_->sendmsg(this, WM_WINDOW_CHANGE, WINDOWID_PREVIEW);
                break;
            default:
                db_msg("[debug_zhb]:invild keycode");
                break;
        }
#endif
}

int NewSettingWindow::HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
        if(ignore_message_flag_){
            db_error("usb host connect,ignore message!!!");
            return -1;
        }
        case MSG_PAINT:
            return HELP_ME_OUT;
        break;
        case LV_EVENT_REFRESH:
        {
            printf("setting clicked\n");
                ListViewClickProc();
        }
        break;
        case MSG_LBUTTONDOWN:

        break;
        case MSG_LBUTTONUP:

        break;
        case MSG_MOUSEMOVE:

        break;
        default:
        break;
    }
    return SystemWindow::HandleMessage( hwnd, message, wparam, lparam );
}


NewSettingWindow::NewSettingWindow(IComponent *parent)
    : SystemWindow(parent)
    , list_view_(NULL)
    , sub_list_view_(NULL)
    , sub_list_(NULL)
    , win_mg_(::WindowManager::GetInstance())
    , r(R::get())
    , active_mode_(DEVICE_LIST_VIEW)
    , current_active_mode_(DEVICE_LIST_VIEW)
    , timesetting_window_(NULL)
    , info_dialog_(NULL)
    , return_button_view_(NULL)
    , home_button_view_(NULL)
    , view_device_(NULL)
    , view_adas_settings_(NULL)
    , view_general_settings_(NULL)
    , device_label_view_(NULL)
    , adas_label_view_(NULL)
    , general_label_view_(NULL)
//    , listview_background_color_(0xFF2E2E2E)
, listview_background_color_(0xFF2E2E2E)
    , listview_str_background_color_(0xFFFFFFFF)
    , BulletCollection_(NULL)
    , select_index_(-1)
    , device_listview_size_(0)
    , adas_setting_listview_size_(0)
    , general_setting_listview_size_(0)
    , ignore_message_flag_(false)
    , device_info_flag_(false)
    , timesetting_show_flag_(false)
{
    wname = "NewSettingWindow";
    Load();
//    SetBackColor(0xFF000000);
    version_str.clear();

    return_button_view_ = reinterpret_cast<GraphicView *>(GetControl("return_gview"));  //返回按键
    GraphicView::LoadImage(return_button_view_, "return");
    return_button_view_->SetTag(RETURN_VIEW_CLICK);
    return_button_view_->OnClick.bind(this, &NewSettingWindow::ButtonClickProc);
    //return_button_view_->SetBackColor(PIXEL_black);
    return_button_view_->Show();

    home_button_view_ = reinterpret_cast<GraphicView *>(GetControl("home_gview"));  //主页按键
    GraphicView::LoadImage(home_button_view_, "home");
    home_button_view_->SetTag(HOME_VIEW_CLICK);
    home_button_view_->OnClick.bind(this, &NewSettingWindow::ButtonClickProc);
    //home_button_view_->SetBackColor(PIXEL_black);
    home_button_view_->Show();
    
    list_view_ = reinterpret_cast<ListView *> (GetControl("listtable"));  //设置界面内容
    list_view_->SetBackColor(listview_background_color_);
    list_view_->SetHilightColor(0xFF2E2E2E);
    /*SetWindowElementAttr(list_view_->GetHandle(), WE_BGC_HIGHLIGHT_ITEM,
                      Pixel2DWORD(HDC_SCREEN, 0xFF2E2E2E));*/

    /*load the left graphics*/
    view_device_ = reinterpret_cast<GraphicView *>(GetControl("device_gview"));
//    GraphicView::LoadImage(view_device_, "device_view");
    GraphicView::LoadImage(view_device_, "device_view", "device_view_on", GraphicView::NORMAL);
    view_device_->SetTag(DEVICE_VIEW_CLICK);
    view_device_->OnClick.bind(this, &NewSettingWindow::ModelViewClickProc);

    view_adas_settings_ = reinterpret_cast<GraphicView *>(GetControl("alert_settings_gview"));
//    GraphicView::LoadImage(view_alert_settings_, "sw_return");
    GraphicView::LoadImage(view_adas_settings_, "adas_view", "adas_view_on", GraphicView::NORMAL);
    view_adas_settings_->SetTag(ADAS_SETTING_VIEW_CLICK);
    view_adas_settings_->OnClick.bind(this, &NewSettingWindow::ModelViewClickProc);

    view_general_settings_ = reinterpret_cast<GraphicView *>(GetControl("general_settings_gview"));
//    GraphicView::LoadImage(view_general_settings_, "general_view");
    GraphicView::LoadImage(view_general_settings_, "general_view", "general_view_on", GraphicView::NORMAL);
    view_general_settings_->SetTag(GENERAL_SETTING_VIEW_CLICK);
    view_general_settings_->OnClick.bind(this, &NewSettingWindow::ModelViewClickProc);

    string lebel_view;
    device_label_view_ = reinterpret_cast<TextView *>(GetControl("device_label"));
    device_label_view_->SetTextStyle(DT_LEFT|DT_BOTTOM);
    device_label_view_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_listview_mode_device", lebel_view);
    device_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    device_label_view_->SetTag(DEVICE_VIEW_CLICK);
    device_label_view_->TextOnClick.bind(this, &NewSettingWindow::ModelViewClickProc);

    adas_label_view_ = reinterpret_cast<TextView *>(GetControl("alert_label"));
    adas_label_view_->SetTextStyle(DT_LEFT|DT_BOTTOM);
    adas_label_view_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_listview_mode_adas", lebel_view);
    adas_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    adas_label_view_->SetTag(ADAS_SETTING_VIEW_CLICK);
    adas_label_view_->TextOnClick.bind(this, &NewSettingWindow::ModelViewClickProc);

    general_label_view_ = reinterpret_cast<TextView *>(GetControl("general_label"));
    general_label_view_->SetTextStyle(DT_LEFT|DT_BOTTOM);
    general_label_view_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_listview_mode_general", lebel_view);
    general_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    general_label_view_->SetTag(GENERAL_SETTING_VIEW_CLICK);
    general_label_view_->TextOnClick.bind(this, &NewSettingWindow::ModelViewClickProc);

    return_label_view_ = reinterpret_cast<TextView *>(GetControl("return_label"));
    return_label_view_->SetTextStyle(DT_CENTER|DT_TOP);
    return_label_view_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_listview_return", lebel_view);
    return_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    return_label_view_->SetTag(RETURN_VIEW_CLICK);
    return_label_view_->TextOnClick.bind(this, &NewSettingWindow::ButtonClickProc);

    home_label_view_ = reinterpret_cast<TextView *>(GetControl("home_label"));
    home_label_view_->SetTextStyle(DT_CENTER|DT_TOP);
    home_label_view_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_listview_home", lebel_view);
    home_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    home_label_view_->SetTag(HOME_VIEW_CLICK);
    home_label_view_->TextOnClick.bind(this, &NewSettingWindow::ButtonClickProc);

    view_device_->SetState(GraphicView::HIGHLIGHT, true);
    view_device_->Show();
    return_label_view_->Show();
    home_label_view_->Show();
    view_adas_settings_->Show();
    view_general_settings_->Show();

    //init the first listitem cols
    LVCOLUMN col[3];
    std::vector<LVCOLUMN> columns;
    col[0].pfnCompare = NULL;
    col[0].width = LISTVIEW_FIRST_COL_W;
    col[0].colFlags = LVHF_CENTERALIGN;
    col[0].colmFlags = LVCF_CENTERALIGN_IMAGE;
    col[0].pszHeadText = "str";
    col[0].nCols = FIRST_COL;
    columns.push_back(col[0]);
    col[1].pfnCompare = NULL;
    col[1].width = LISTVIEW_SECOND_COL_W;
    col[1].colFlags = LVCF_CENTERALIGN;
    col[1].colmFlags = LVCF_CENTERALIGN_IMAGE;
    col[1].pszHeadText = "str";
    col[1].nCols =SECOND_COL;
    columns.push_back(col[1]);
    col[2].pfnCompare = NULL;
    col[2].width = LISTVIEW_THIRD_COL_W;
    col[2].colFlags = LVCF_RIGHTALIGN;
    col[2].colmFlags = LVCF_RIGHTALIGN_IMAGE;
    col[2].pszHeadText = "image";
    col[2].nCols = THIRD_COL;
    columns.push_back(col[2]);
    list_view_->SetColumns(columns,false);

    device_listview_size_ = sizeof(Device_ListBoxItemData)/sizeof(Device_ListBoxItemData[0]);
    adas_setting_listview_size_ = sizeof(ADAS_Setting_Listview_Itemids)/sizeof(ADAS_Setting_Listview_Itemids[0]);
    general_setting_listview_size_ = sizeof(General_Setting_Listview_Itemids)/sizeof(General_Setting_Listview_Itemids[0]);

    InitListViewItem(DEVICE_LIST_VIEW);
    list_view_->Show();
    //sub list view
    sub_list_ = new Sublist(this);
    sub_list_->OnListItemClick.bind(this, &NewSettingWindow::SubListViewClickProc);
    sub_list_view_ = static_cast<ListView*>(sub_list_->GetControl("sublist_view"));
    sub_list_view_ ->SetBackColor(listview_background_color_);
    sub_list_view_ ->SetHilightColor(0xFF3E3EFF);
    /*SetWindowElementAttr(sub_list_view_->GetHandle(), WE_BGC_HIGHLIGHT_ITEM,
                       Pixel2DWORD(HDC_SCREEN, 0xFF3E3EFF));*/

    //::LoadBitmapFromFile(HDC_SCREEN, &sublist_icon_pic_off, R::get()->GetImagePath("sublist_switch_off").c_str());
    //::LoadBitmapFromFile(HDC_SCREEN, &sublist_icon_pic_on, R::get()->GetImagePath("sublist_switch_on").c_str());
    //::LoadBitmapFromFile(HDC_SCREEN, &list_icon_pic_, R::get()->GetImagePath("switch").c_str());

    //init the time setting windown 
    timesetting_window_ = new TimeSettingWindowNew(this);
    timesetting_window_->OnConfirmClick.bind(this, &NewSettingWindow::DateTimeSettingConfirm);

    //init info dialog
    info_dialog_ = new InfoDialog(this);

    //init the button dialog
    BulletCollection_ = new BulletCollection();
	BulletCollection_->initButtonDialog(this);

    //SystemVersion(false);
}

NewSettingWindow::~NewSettingWindow()
{
    if(BulletCollection_ != NULL)
    {
        delete BulletCollection_;
        BulletCollection_ = NULL;
    }
    if(info_dialog_ != NULL)
    {
        delete info_dialog_;
        info_dialog_ = NULL;
    }
    if(timesetting_window_ != NULL)
    {
        delete timesetting_window_;
        timesetting_window_ = NULL;
    }
    //::UnloadBitmap(&sublist_icon_pic_off);
    //::UnloadBitmap(&sublist_icon_pic_on);
    //::UnloadBitmap(&list_icon_pic_);

    if( sub_list_ != NULL)
    {
        delete sub_list_;
        sub_list_ = NULL;
    }

    if(return_button_view_)
        GraphicView::UnloadImage(return_button_view_);
    if(home_button_view_)
        GraphicView::UnloadImage(home_button_view_);
    if(view_device_)
        GraphicView::UnloadImage(view_device_);
    if(view_adas_settings_)
        GraphicView::UnloadImage(view_adas_settings_);
    if(view_general_settings_)
        GraphicView::UnloadImage(view_general_settings_);
}

void NewSettingWindow::Update(MSG_TYPE msg, int p_CamID, int p_recordId)
{
    db_msg("receive msg:%d", msg);
    switch(msg)
    {
    case MSG_STORAGE_UMOUNT:
        {
            if(BulletCollection_->getButtonDialogShowFlag() && BulletCollection_->getButtonDialogCurrentId() == BC_BUTTON_DIALOG_FORMAT_SDCARD && StorageManager::GetInstance()->getFormatFlag() == false)//close setting window button dialog
                BulletCollection_->BCDoHide();
             if (StorageManager::GetInstance()->GetStorageStatus() != UMOUNT)
                break;
             if(current_active_mode_ == GENERAL_SETTING_LIST_VIEW)
                 UpdateSDcardCap();
        }
        break;
    case MSG_STORAGE_MOUNTED:
        {
            if(current_active_mode_ == GENERAL_SETTING_LIST_VIEW)
                UpdateSDcardCap();
        }break;
    case MSG_PREVIEW_TO_NEWSETTING_WINDOW:
       {
            db_debug("preview to setting window...");
            active_mode_ = DEVICE_LIST_VIEW;
            SetActiveMode(active_mode_);
            RefreshActiveViewIcon();
            this->InitListViewItem(active_mode_);
            StorageManager *sm = StorageManager::GetInstance();
            if(sm->GetStorageStatus() != MOUNTED)
            {
                db_warn("current sdcard is umount !!!");
                break;
            }
       }
        break;
    case MSG_PREVIEW_TO_SETTINGWINDOW_UPDATE_VERSION:
        {
            db_msg("debug_zhb-----new setting------MSG_PREVIEW_TO_SETTINGWINDOW_UPDATE_VERSION");
            listener_->sendmsg(this,MSG_SYSTEM_UPDATE,0);
        }
        break;
    case MSG_APP_IS_CONNECTED:
        {
            if(win_mg_->GetCurrentWinID() == WINDOWID_SETTING_NEW)
            {
                // back to preview window
                EyeseeLinux::StatusBarSaver::GetInstance()->Pause(false);
                listener_->sendmsg(this, MSG_SETTING_TO_PREVIEW, 1);
                listener_->sendmsg(this, WM_WINDOW_CHANGE, WINDOWID_PREVIEW);
            }
        }
        break;
    case MSG_FORMAT_FAILED:
       {
           db_error("receive sd format failed");
           UpdateSDcardCap();
       }
       break;
    default:
        break;
    }

}

void NewSettingWindow::GetCreateParams(CommonCreateParams& params)
{
    params.style = WS_NONE;
    params.exstyle = WS_EX_NONE;// | WS_EX_TRANSPARENT;
    params.class_name = " ";
    params.alias      = GetClassName();
}

std::string NewSettingWindow::GetResourceName()
{
    return std::string(GetClassName());
}

void NewSettingWindow::PreInitCtrl(View *ctrl, std::string &ctrl_name)
{
    if(ctrl_name == std::string("return_gview"))
    {
        ctrl->SetCtrlTransparentStyle(false);
    }
}

void NewSettingWindow::ListViewClickProc()
{
    if(ignore_message_flag_){
        db_error("usb host connect,ignore message!!!");
        return;
    }
    //need to get current the select item 
    LVITEM item;
    int ret = -1;
    ret = list_view_->GetSelectedItem(item);
    if(ret < 0)
    {
        db_error("[Habo]---> get selected item fail !!!");
        select_index_ = -1;
        return;
    }
    select_index_ = item.nItem;
    list_view_->SetHilight(select_index_);
    if(current_active_mode_ == DEVICE_LIST_VIEW){
        switch(Device_Listview_Itemids[item.nItem])
        {
            case SETTING_RECORD_RESOLUTION:
            case SETTING_RECORD_TIME:
            case SETTING_RECORD_LOOP_SWITCH:
            case SETTING_EMER_RECORD_SENSITIVITY:
            case SETTING_TIMEWATERMARK:
            case SETTING_RECORD_VOLUME_SWITCH:
            case SETTING_CAMERA_EXPOSURE:
            case SETTING_CAMERA_LIGHTSOURCEFREQUENCY:
                ShowSubList(item.nItem);

            break;

            default:
                db_error("cat not find item id!!");
            break;
        }
    } else if(current_active_mode_ == ADAS_SETTING_LIST_VIEW) {
        switch(ADAS_Setting_Listview_Itemids[item.nItem])
        {
            case SETTING_ADAS_SWITCH:
            {
                MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
                int val = menuconfiglua->GetMenuIndexConfig(SETTING_RECORD_RESOLUTION);
                if(val){
                    PreviewWindow *p_win  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
                    p_win->ShowPromptInfo(PROMPT_NOT_SUPPORT_ADAS,2);
                    break;
                }else
                    ShowSubList(item.nItem);
            }
            break;
            case SETTING_ADAS_CALIBRATION:
                ShowSubList(item.nItem);
            break;
            default:
                db_error("cat not find item id!!");
            break;
        }
    } else if(current_active_mode_ == GENERAL_SETTING_LIST_VIEW) {
        switch(General_Setting_Listview_Itemids[item.nItem])
        {
            case SETTING_DEVICE_FORMAT:
                ShowFormatScardDialog();
            break;
            case SETTING_CAMERA_AUTOSCREENSAVER:
            case SETTING_DEVICE_LANGUAGE:
            case SETTING_PARKING_MONITORY:
            case SETTING_VOICE_CTRL:
            case SETTING_VOLUME_SELECTION:
                ShowSubList(item.nItem);
            break;
            case SETTING_DEVICE_DATETIME:
                ShowTimeSettingWindow();
            break;
//            case SETTING_WIFI_SWITCH:
//                ShowSubList(item.nItem);
//            break;
            case SETTING_DEVICE_RESETFACTORY:
                ShowResetFactoryDialog();
            break;
            case SETTING_DEVICE_DEVICEINFO:
                ShowDeviceInfoDialog();
            break;
            default:
                db_error("cat not find item id!!");
            break;
        }
    } else {
        db_error("current_active_mode_ %d is invalid!!",current_active_mode_);
    }
}

void NewSettingWindow::SubListViewClickProc(View *control)
{
	printf("SubListViewClickProc\n");
    if(ignore_message_flag_){
        db_error("usb host connect,ignore message!!!");
        return;
    }
    LVITEM main_select_item, sub_select_item;
    int main_row_index = 0 ,sub_row_index = 0 ;
    int ret = -1;
    //hide the sublistview
    usleep(400*1000);
    sub_list_->DoHide();
    ret = sub_list_view_->GetSelectedItem(sub_select_item);

    if (ret < 0) {
        db_error("GetSelectedItem failed, ret[%d]", ret);
        return;
    }

    if (select_index_ < 0) {
        ret = list_view_->GetSelectedItem(main_select_item);
        if (ret < 0) {
            db_error("GetSelectedItem failed, ret[%d]", ret);
            return;
        }
        main_row_index = main_select_item.nItem;
    } else {
        
        main_row_index = select_index_;
    }
    sub_row_index = sub_select_item.nItem;
    //判断这次选择的是否和进来的时候是一致的，如果是则不用刷新一级菜单
    int old_hilight = sub_list_view_->GetHilight();
#ifdef KEY_INTERACTION
    std::string value_str = sub_list_view_->GetItemText(sub_row_index, 0);//获取当前选择的item的字符串
    list_view_->SetItemText(value_str, main_row_index, THIRD_COL);
    list_view_->Refresh();
    //reset the current value
    SetMenuConfig(GetNotifyMessage(listview_item_ids[main_row_index]),sub_row_index);
    listener_->sendmsg(this,GetNotifyMessage(listview_item_ids[main_row_index]),sub_row_index);
#else
    std::string value_str = sub_list_view_->GetItemText(sub_row_index, SECOND_COL);//获取当前选择的item的字符串
    list_view_->SetItemText(value_str, main_row_index, SECOND_COL);
    list_view_->Refresh();
    //reset the current value
    int mesg_index  = 0;
    if(current_active_mode_ == DEVICE_LIST_VIEW) {
        mesg_index = GetNotifyMessage(Device_Listview_Itemids[main_row_index]);
    } else if(current_active_mode_ == ADAS_SETTING_LIST_VIEW) {
        mesg_index = GetNotifyMessage(ADAS_Setting_Listview_Itemids[main_row_index]);
    } else if(current_active_mode_ == GENERAL_SETTING_LIST_VIEW) {
        mesg_index = GetNotifyMessage(General_Setting_Listview_Itemids[main_row_index]);
    }
    SetMenuConfig(mesg_index,sub_row_index);
    listener_->sendmsg(this,mesg_index,sub_row_index);
#endif
}

void NewSettingWindow::ButtonClickProc(View *control)
{
    if(ignore_message_flag_){
        db_error("usb host connect,ignore message!!!");
        return;
    }
    int tag = control->GetTag();
    switch (tag) {
        case RETURN_VIEW_CLICK:
        {
            if(!sub_list_->GetSubListWindowActiveStatus())
            {
                sub_list_->DoHide();
//                AudioCtrl::GetInstance()->PlaySound(AudioCtrl::KEY1_SOUND);
            }else if(device_info_flag_){
                info_dialog_->DoHide();
                device_info_flag_ = false;
            }else if(timesetting_show_flag_){
                timesetting_window_->DoHide();
                timesetting_show_flag_ = false;
            }
            else
            {
                this->keyProc(SDV_KEY_MENU, SHORT_PRESS);
//                AudioCtrl::GetInstance()->PlaySound(AudioCtrl::KEY1_SOUND);
            }
        }
        break;

        case HOME_VIEW_CLICK:
        {
            if(device_info_flag_){
                info_dialog_->DoHide();
                device_info_flag_ = false;
            }else if(timesetting_show_flag_){
                timesetting_window_->DoHide();
                timesetting_show_flag_ = false;
            }
            if(!sub_list_->GetSubListWindowActiveStatus())
               sub_list_->DoHide();
            this->keyProc(SDV_KEY_MENU, SHORT_PRESS);
//            AudioCtrl::GetInstance()->PlaySound(AudioCtrl::KEY1_SOUND);
        }
        break;
    }

}

void NewSettingWindow::SetActiveMode(LIST_VIEW_TYPE mode_type)
{
    current_active_mode_ = mode_type;
}

void NewSettingWindow::ModelViewClickProc(View * control)
{
    if(ignore_message_flag_){
        db_error("usb host connect,ignore message!!!");
        return;
    }
    int tag = control->GetTag();
    switch (tag) {
        case DEVICE_VIEW_CLICK:
            {
//                lock_guard<mutex> lock(view_click_mutex_);
                active_mode_ = DEVICE_LIST_VIEW;
                db_error("current_active_mode_ %d active_mode %d",current_active_mode_,active_mode_);
                if (current_active_mode_ == active_mode_) break;
                SetActiveMode(active_mode_);
                RefreshActiveViewIcon();
                InitListViewItem(active_mode_);
            }
            break;
       case ADAS_SETTING_VIEW_CLICK:
            {
//                lock_guard<mutex> lock(view_click_mutex_);
                active_mode_ = ADAS_SETTING_LIST_VIEW;
                db_error("current_active_mode_ %d active_mode %d",current_active_mode_,active_mode_);
                if (current_active_mode_ == active_mode_) break;
                SetActiveMode(active_mode_);
                RefreshActiveViewIcon();
                InitListViewItem(active_mode_);
            }
            break;
       case GENERAL_SETTING_VIEW_CLICK:
            {
//                lock_guard<mutex> lock(view_click_mutex_);
                active_mode_ = GENERAL_SETTING_LIST_VIEW;
                db_error("current_active_mode_ %d active_mode %d",current_active_mode_,active_mode_);
                if (current_active_mode_ == active_mode_) break;
                SetActiveMode(active_mode_);
                RefreshActiveViewIcon();
                InitListViewItem(active_mode_);
            }
            break;
       default:
            db_warn("wrong tag: %d", tag);
            break;
    }
}

void NewSettingWindow::RefreshActiveViewIcon()
{
    db_debug("refresh view icon...");
    if(current_active_mode_ == DEVICE_LIST_VIEW){
        view_device_->SetState(GraphicView::HIGHLIGHT, true);
        view_adas_settings_->SetState(GraphicView::NORMAL, true);
        view_general_settings_->SetState(GraphicView::NORMAL, true);
        device_label_view_->SetCaptionColor(0x7800FF00);
        adas_label_view_->SetCaptionColor(0xFFFFFFFF);
        general_label_view_->SetCaptionColor(0xFFFFFFFF);
    }else if(current_active_mode_ == ADAS_SETTING_LIST_VIEW){
        view_device_->SetState(GraphicView::NORMAL, true);
        view_adas_settings_->SetState(GraphicView::HIGHLIGHT, true);
        view_general_settings_->SetState(GraphicView::NORMAL, true);
        device_label_view_->SetCaptionColor(0xFFFFFFFF);
        adas_label_view_->SetCaptionColor(0x7800FF00);
        general_label_view_->SetCaptionColor(0xFFFFFFFF);
    }else if(current_active_mode_ == GENERAL_SETTING_LIST_VIEW){
        view_device_->SetState(GraphicView::NORMAL, true);
        view_adas_settings_->SetState(GraphicView::NORMAL, true);
        view_general_settings_->SetState(GraphicView::HIGHLIGHT, true);
        device_label_view_->SetCaptionColor(0xFFFFFFFF);
        adas_label_view_->SetCaptionColor(0xFFFFFFFF);
        general_label_view_->SetCaptionColor(0x7800FF00);
    }
    db_debug("refresh view icon end...");
}

void NewSettingWindow::InitListViewItem(LIST_VIEW_TYPE type)
{
    db_debug("Init list view item");
    if(list_view_ == NULL)
    {
        db_error("fatal error,list_view is null!!!");
        return;
    }
    list_view_->RemoveAllItems();
    if(type == DEVICE_LIST_VIEW){
        for(int i = 0 ; i < device_listview_size_ ; i++){
            InitListViewItem(Device_ListBoxItemData[i],i);
        }
    } else if(type == ADAS_SETTING_VIEW_CLICK) {
        for(int i = 0 ; i < adas_setting_listview_size_ ; i++){
            InitListViewItem(ADAS_Setting_ListBoxItemData[i],i);
        }
    } else if(type == GENERAL_SETTING_LIST_VIEW) {
        for(int i = 0 ; i < general_setting_listview_size_ ; i++){
            InitListViewItem(General_Setting_ListBoxItemData[i],i);
        }
        UpdateSDcardCap();
    }
    list_view_->SetHilight(0);
    db_debug("Init list view item end");
}


void NewSettingWindow::InitListViewItem(const ListViewItem &list,int index_)
{
    InitListViewItem(list.first_text, list.type,index_);
}

int NewSettingWindow::InitListViewItem(const char *first_text,const int  type, int index_)
{
    printf("start to create item\n");

    std::string path;
    std::string str_data;
    int index_n = 0;
    int current_val = 0;
    //set list item
    LVITEM item;
    item.dwFlags &= ~LVIF_FOLD;
    item.nItemHeight = LISTVIEW_ITEM_H;

    //set list sub item
    LVSUBITEM subdata;
    item.nItem = index_;
    list_view_->AddItem(item);
    StringVector str_text;
    //0 str
    r->GetString(std::string(first_text), str_data);
    subdata.pszText = const_cast<char*>(str_data.c_str());
    subdata.nItem = index_;
    subdata.subItem = FIRST_COL;
    subdata.flags = 0;
    subdata.image = NULL;
	subdata.nTextColor = listview_str_background_color_;
    list_view_->FillSubItem(subdata);
    //2 str
    if(type == TYPE_NORMAL){
        if(current_active_mode_ == DEVICE_LIST_VIEW) {
            current_val = GetMenuConfig(Device_Listview_Itemids[index_]);
        } else if(current_active_mode_ == ADAS_SETTING_LIST_VIEW) {
            current_val = GetMenuConfig(ADAS_Setting_Listview_Itemids[index_]);
        } else if(current_active_mode_ == GENERAL_SETTING_LIST_VIEW) {
            current_val = GetMenuConfig(General_Setting_Listview_Itemids[index_]);
        }
        str_text.clear();
        r->GetStringArray(std::string(first_text), str_text);
        subdata.pszText = const_cast<char*>((str_text[current_val]).c_str());
    }else if(type == TYPE_DIALOG || type == TYPE_DIALOG_STR){
        subdata.pszText = "";
    }
        subdata.nItem = index_;
        subdata.subItem = SECOND_COL;
        subdata.flags = 0;
        subdata.image = NULL;
        subdata.nTextColor = listview_str_background_color_;
        list_view_->FillSubItem(subdata);
    //3 image
    subdata.pszText = const_cast<char*>("");
    subdata.nItem = index_;
    subdata.subItem = THIRD_COL;
    if(type == TYPE_NORMAL || type == TYPE_DIALOG){
        subdata.flags = LVFLAG_BITMAP;
        path = "S:" + R::get()->GetImagePath("switch");
        subdata.image = path.c_str();
    }else if(type == TYPE_DIALOG_STR){
        subdata.flags = 0;
        subdata.image = 0;
    }
    subdata.nTextColor = listview_str_background_color_;
    list_view_->FillSubItem(subdata);
    return 0;
}

void NewSettingWindow::ShowSubList(int index_msg)
{
    int hi_idx = 0;
    sub_list_view_->RemoveAllItems();

    string sublist_view_tip;
    r->GetString("ml_sublistview_tip", sublist_view_tip);

    LVCOLUMN col[3];
    std::vector<LVCOLUMN> columns;
    col[0].pfnCompare = NULL;
    col[0].width = SUBLISTVIEW_FIRST_COL_W;
    col[0].colFlags = LVHF_CENTERALIGN;
    col[0].colmFlags = LVCF_CENTERALIGN_IMAGE;
    col[0].pszHeadText = "image";
    col[0].nCols = FIRST_COL;
    columns.push_back(col[0]);
    col[1].pfnCompare = NULL;
    col[1].width = SUBLISTVIEW_SECOND_COL_W;
    col[1].colFlags = LVCF_CENTERALIGN;
    col[1].colmFlags = LVCF_CENTERALIGN_IMAGE;
    col[1].pszHeadText = "str";
    col[1].nCols =SECOND_COL;
    columns.push_back(col[1]);
    col[2].pfnCompare = NULL;
    col[2].width = SUBLISTVIEW_THIRD_COL_W;
    col[2].colFlags = LVCF_RIGHTALIGN;
    col[2].colmFlags = LVCF_RIGHTALIGN_IMAGE;
    col[2].pszHeadText = "str";
    col[2].nCols = THIRD_COL;
    columns.push_back(col[2]);
    sub_list_view_->SetColumns(columns,false);
    /*********set the sublist item***********/
    LVITEM item_sub;
    item_sub.dwFlags &= ~LVIF_FOLD;
    item_sub.nItemHeight = LISTVIEW_ITEM_H;
    LVSUBITEM subdata;
    StringVector sub_value_list;
    sub_value_list.clear();
    //set the sub menu head
    if(current_active_mode_ == DEVICE_LIST_VIEW) {
        r->GetStringArray(std::string(Device_ListBoxItemData[index_msg].first_text), sub_value_list);
        hi_idx = GetMenuConfig(Device_Listview_Itemids[index_msg]);
    } else if(current_active_mode_ == ADAS_SETTING_LIST_VIEW) {
        r->GetStringArray(std::string(ADAS_Setting_ListBoxItemData[index_msg].first_text), sub_value_list);
        hi_idx = GetMenuConfig(ADAS_Setting_Listview_Itemids[index_msg]);
    } else if(current_active_mode_ == GENERAL_SETTING_LIST_VIEW) {
        r->GetStringArray(std::string(General_Setting_ListBoxItemData[index_msg].first_text), sub_value_list);
        hi_idx = GetMenuConfig(General_Setting_Listview_Itemids[index_msg]);
    }

    for(int i = 0; i < sub_value_list.size(); ++i)
    {
        item_sub.nItem = i;
	std::string path;
        printf("create sublist\n");
	sub_list_view_->AddItem(item_sub);
        //image
        subdata.nTextColor = listview_str_background_color_;
        subdata.pszText = const_cast<char*>("");
        subdata.flags   = LVFLAG_BITMAP;
        if(hi_idx == i){
            path = "S:" + R::get()->GetImagePath("sublist_switch_on");
        }else{
            path = "S:" + R::get()->GetImagePath("sublist_switch_off");
        }
	subdata.image = path.c_str();
        subdata.nItem = i;
        subdata.subItem= FIRST_COL;
        sub_list_view_->FillSubItem(subdata);
        //str
        subdata.nTextColor = listview_str_background_color_;
        subdata.pszText = const_cast<char*>((sub_value_list[i]).c_str());
        subdata.flags   = 0;
        subdata.image   = NULL;
        subdata.nItem = i;
        subdata.subItem = SECOND_COL;
        sub_list_view_->FillSubItem(subdata);
        //str
        subdata.nTextColor = listview_str_background_color_;
//        subdata.pszText = const_cast<char*>(sublist_view_tip.c_str());
        subdata.pszText = const_cast<char*>("");
        subdata.flags   = 0;
        subdata.image   = NULL;
        subdata.nItem = i;
        subdata.subItem = THIRD_COL;
        sub_list_view_->FillSubItem(subdata);
    }
    if( sub_list_ != NULL)
        sub_list_->DoShow();
    sub_list_view_->SelectItem(hi_idx);
}


void NewSettingWindow::OnLanguageChanged()
{
    string lebel_view;
    r->GetString("ml_listview_mode_device", lebel_view);
    device_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_listview_mode_adas", lebel_view);
    adas_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_listview_mode_general", lebel_view);
    general_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_listview_return", lebel_view);
    return_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_listview_home", lebel_view);
    home_label_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
}


int NewSettingWindow::GetMenuConfig(int msg)
{
    int val=0;
    MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
    val = menuconfiglua->GetMenuIndexConfig(msg);
    db_debug("GetMenuConfig msg %d val %d",msg,val);
	return val;
}

void NewSettingWindow::SetMenuConfig(int msg,int val)
{
    MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
    menuconfiglua->SetMenuIndexConfig(msg,val);
    db_debug("SetMenuConfig msg %d val %d",msg,val);
}

int NewSettingWindow::GetNotifyMessage(int msgid)
{
    int ret = 0;
    switch(msgid){
        case SETTING_RECORD_RESOLUTION:
            ret = MSG_SET_VIDEO_RESOULATION;
        break;
        case SETTING_RECORD_TIME:
            ret = MSG_SET_RECORD_TIME;
        break;
        case SETTING_RECORD_LOOP_SWITCH:
            ret = MSG_RECORD_LOOP_SWITCH;
        break;
        case SETTING_EMER_RECORD_SENSITIVITY:
            ret = MSG_SET_EMER_RECORD_SENSITIVITY;
        break;
        case SETTING_TIMEWATERMARK:
            ret = MSG_SET_TIMEWATER_MARK;
        break;
        case SETTING_RECORD_VOLUME_SWITCH:
            ret = MSG_SET_RECORD_VOLUME;
        break;
        case SETTING_CAMERA_EXPOSURE:
            ret = MSG_SET_CAMERA_EXPOSURE;
        break;
        case SETTING_CAMERA_LIGHTSOURCEFREQUENCY:
            ret = MSG_SET_CAMERA_LIGHTSOURCEFREQUENCY;
        break;

        case SETTING_ADAS_SWITCH:
            ret = MSG_SET_ADAS_SWITCH;
        break;

        case SETTING_ADAS_CALIBRATION:
            ret = MSG_SET_ADAS_CALIBRATION;
        break;

        case SETTING_CAMERA_AUTOSCREENSAVER:
            ret = MSG_SET_AUTO_TIME_SCREENSAVER;
        break;
        case SETTING_DEVICE_LANGUAGE:
            ret = MSG_RM_LANG_CHANGED;
        break;
        case SETTING_PARKING_MONITORY:
            ret = MSG_SET_PARKING_MONITORY;
        break;
        case SETTING_VOICE_CTRL:
            ret = MSG_SET_VOICE_CTRL;
        break;
        case SETTING_VOLUME_SELECTION:
            ret = MSG_SET_VOLUME_SELECTION;
        break;
        case SETTING_WIFI_SWITCH:
            ret = MSG_SET_WIFI_SWITCH;
        break;
//        case SETTING_PARKING_MONITORY:
//            ret = MSG_SET_PARKING_MONITORY;
//        break;
//        case SETTING_RECORD_ENCODINGTYPE:
//            ret = MSG_SET_RECORD_ENCODE_TYPE;
//        break;
//        case SETTING_DEVICE_TIME:
//            ret = MSG_DEVICE_TIME;
//            break;
//        case SETTING_DEVICE_FORMAT:
//            ret = MSG_DEVICE_FORMAT;
//            break;
//        case SETTING_DEVICE_RESETFACTORY:
//            ret = MSG_DEVICE_RESET_FACTORY;
//            break;
//        case SETTING_DEVICE_SDCARDINFO:
//            ret = MSG_SET_DEVICE_SDCARDINFO;
//            break;
//        case SETTING_DEVICE_VERSIONINFO:
//            ret = MSG_SET_DEVICE_VERSIONINFO;
//            break;
        default:
            ret = -1;
            break;
    }
    return ret;
}


void NewSettingWindow::DoShow()
{
    Window::DoShow();
}

void NewSettingWindow::DoHide()
{
    SetVisible(false);
}

int NewSettingWindow::ShowTimeSettingWindow()
{
    if( timesetting_window_ != NULL)
    {
        timesetting_window_->ShowCurrentDateTime();
        timesetting_window_->Show();
        timesetting_show_flag_ = true;
    }
    return 0;
}
void NewSettingWindow::DateTimeSettingConfirm(View *view, int value)
{
    printf("NewSettingWindow::DateTimeSettingConfirm\n");
    if( timesetting_window_ != NULL)
        timesetting_window_->DoHide();
}

void NewSettingWindow::ShowResetFactoryDialog()
{
    if( BulletCollection_ != NULL)
    {
        BulletCollection_->setButtonDialogCurrentId(BC_BUTTON_DIALOG_RESETFACTORY);
        BulletCollection_->ShowButtonDialog();
    }
}

int NewSettingWindow::ShowFormatScardDialog()
{
    if(!(StorageManager::GetInstance()->GetStorageStatus() != UMOUNT))
    {
        PreviewWindow *p_win  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
        p_win->ShowPromptInfo(PROMPT_TF_NO_EXIST,2);
        return -1;
    }
    if( BulletCollection_ != NULL)
    {

        BulletCollection_->setButtonDialogCurrentId(BC_BUTTON_DIALOG_FORMAT_SDCARD);
        BulletCollection_->ShowButtonDialog();
    }
    return 0;
}

void NewSettingWindow::ShowDeviceInfoDialog()
{
    std::string str_title;
    R::get()->GetString("ml_device_deviceinfo", str_title);
    if( info_dialog_ != NULL)
        info_dialog_->SetInfoTitle(str_title);
    
    std::string str_name,str_make,str_version,wifi_name,wifi_pwd;
    R::get()->GetString("ml_device_deviceinfo_name", str_name);
    R::get()->GetString("ml_device_deviceinfo_make", str_make);
    R::get()->GetString("ml_device_version_current", str_version);
    R::get()->GetString("ml_camera_wifiswitch_ssid", wifi_name);
    R::get()->GetString("ml_camera_wifiswitch_passwd", wifi_pwd);
    ::LuaConfig config;
    config.LoadFromFile("/tmp/data/menu_config.lua");
	std::string version_str = config.GetStringValue("menu.system.version_info");
    std::string str_ssid = config.GetStringValue("menu.system.wifi_info.ssid");
    std::string str_pwd = config.GetStringValue("menu.system.wifi_info.password");
    
    std::stringstream ss;
    ss << "\n" << str_name << "\n";
    ss << "\n" << str_make << "\n";
    ss << "\n" << str_version << " "<< version_str << "\n";
    ss << "\n" << wifi_name << " "<< str_ssid << "\n";
    ss << "\n" << wifi_pwd << " "<< str_pwd << "\n";

    std::string str;
    str = ss.str();
    if( info_dialog_ != NULL)
    {
        info_dialog_->SetInfoText(str);
        info_dialog_->Show();
        device_info_flag_ = true;
    }

}

void NewSettingWindow::ResetUpdate()
{
    db_error("ResetUpdate()");
    active_mode_ = DEVICE_LIST_VIEW;
    SetActiveMode(active_mode_);
    RefreshActiveViewIcon();
    InitListViewItem(active_mode_);
}

void NewSettingWindow::GetSdcardInfo(std::stringstream &info_str)
{
    uint32_t free_, total_;
    StorageManager *sm = StorageManager::GetInstance();
    int status = sm->GetStorageStatus();
    db_debug("sd status %d",status);
    if(status == UMOUNT || status == STORAGE_FS_ERROR  || (status == FORMATTING))
    {
        total_ = 0;
        free_ = 0;
        std::string str_tf;
        r->GetString("ml_no_tf", str_tf);
        db_warn("habo---> str_tf = %s",str_tf.c_str());
        info_str<<str_tf.c_str();
    }
    else
    {
        sm->GetStorageCapacity(&free_, &total_);
        info_str<<free_<<" MB";
    }
    db_warn("[debug_zhb]-----GetSdcardInfo-----info_str = %s",info_str.str().c_str());
}

void NewSettingWindow::UpdateSDcardCap()
{
    if(list_view_ == NULL)
    {
        db_error("ItemData data is null");
        return;
    }
    std::stringstream info_str;
    GetSdcardInfo(info_str);
    for(int i = 0 ; i < general_setting_listview_size_ ; i++){
        if(General_Setting_Listview_Itemids[i] == SETTING_DEVICE_FORMAT)
        {
            db_error("i %d info str %s",i,info_str.str().c_str());
            list_view_->SetItemText(info_str.str().c_str(), i, SECOND_COL);
        }
    }
    list_view_->Refresh();
}

void NewSettingWindow::SystemVersion(bool fset)
{
    ::LuaConfig config;
        config.LoadFromFile("/tmp/data/menu_config.lua");
    if(!fset)
        version_str = config.GetStringValue("menu.system.version_info");
    else
        config.SetStringValue("menu.system.version_info",version_str);
}

void NewSettingWindow::ForceCloseSettingWindowAllDialog()
{
    printf("ForceCloseSettingWindowAllDialog\n");
    if(BulletCollection_->getButtonDialogShowFlag())//close setting window button dialog
        BulletCollection_->BCDoHide();
}

void NewSettingWindow::ReturnPreviewWindow(bool backcarvideo_on_flag)
{
    if(device_info_flag_){
        info_dialog_->DoHide();
        device_info_flag_ = false;
    }
    if(timesetting_show_flag_){
        timesetting_window_->DoHide();
        timesetting_show_flag_ = false;
    }
    if(!sub_list_->GetSubListWindowActiveStatus())
       sub_list_->DoHide();
    BulletCollection_->BCDoHide();
    EyeseeLinux::StatusBarSaver::GetInstance()->Pause(false);
    listener_->sendmsg(this, MSG_SETTING_TO_PREVIEW, 0);
    listener_->sendmsg(this, WM_WINDOW_CHANGE, WINDOWID_PREVIEW);
    //AudioCtrl::GetInstance()->PlaySound(AudioCtrl::KEY1_SOUND);
    if(backcarvideo_on_flag){
        PreviewWindow *p_win  = static_cast<PreviewWindow*>(win_mg_->GetWindow(WINDOWID_PREVIEW));
        p_win->Update(MSG_BACKCARVIDEO_ON);
        db_debug("send MSG_BACKCARVIDEO_ON msg");
    }
}
