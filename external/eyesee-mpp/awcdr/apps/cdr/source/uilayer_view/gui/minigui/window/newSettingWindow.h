/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file preview_window.h
 * @brief 单路预览录像窗口
 * @author id:826
 * @version v0.3
 * @date 2016-11-04
 */

#pragma once


#include "window/window.h"
#include "window/user_msg.h"
#include "widgets/graphic_view.h"
#include "widgets/text_view.h"
#include "window/window_manager.h"
#include <time.h>
#include <signal.h>
#include <mutex>

#define LISTVIEW_FIRST_COL_W    150
#define LISTVIEW_SECOND_COL_W   250
#define LISTVIEW_THIRD_COL_W    100

#define SUBLISTVIEW_FIRST_COL_W   300
#define SUBLISTVIEW_SECOND_COL_W  200
#define SUBLISTVIEW_THIRD_COL_W   200

#define SETTING_BUTTON_DIALOG       (USER_MSG_BASE+100)

//预留32后面的宽度

typedef enum{
    FIRST_COL = 0,//image
    SECOND_COL,//str
    THIRD_COL,//str
    FOURTH_COL,//image
};

enum {
   DEVICE_VIEW_CLICK,
   ADAS_SETTING_VIEW_CLICK,
   GENERAL_SETTING_VIEW_CLICK,
};

enum {
    RETURN_VIEW_CLICK,
    HOME_VIEW_CLICK,
};

enum LIST_VIEW_TYPE{
   DEVICE_LIST_VIEW,
   ADAS_SETTING_LIST_VIEW,
   GENERAL_SETTING_LIST_VIEW,
};

#define LISTVIEW_ITEM_H 80

class ListView;
class WindowManager;
class Dialog;
class Button;
class GraphicView;
class TimeSettingWindowNew;
class PromptBox;
class Sublist;
class InfoDialog;
class BulletCollection;
class PreviewWindow;    

class NewSettingWindow
    : public SystemWindow
{
    DECLARE_DYNCRT_CLASS(NewSettingWindow, Runtime)

public:
        NewSettingWindow(IComponent *parent);

        virtual ~NewSettingWindow();

        void Update(MSG_TYPE msg, int p_CamID=0, int p_recordId=0);

        void ResetUpdate();
private:
        std::string GetResourceName();

        void GetCreateParams(CommonCreateParams &params);

        int HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam);

        void PreInitCtrl(View *ctrl, std::string &ctrl_name);

        void DoShow();

        void DoHide();

        void ListViewClickProc();
        
        void SubListViewClickProc(View *control);

        void ButtonClickProc(View *control);

        void SetActiveMode(LIST_VIEW_TYPE mode_type);

        void ModelViewClickProc(View * control);

        void keyProc(int keyCode, int isLongPress);

        void OnLanguageChanged();
        
        int GetMenuConfig(int msg);
        
        void SetMenuConfig(int msg,int val);
        
        void InitListViewItem(LIST_VIEW_TYPE type);
        
        void InitListViewItem(const ListViewItem &list,int index_);
        
        int InitListViewItem(const char *text,const int type,int index_);

        void ShowSubList(int index_msg);

        int GetNotifyMessage(int msgid);

        int ShowTimeSettingWindow();

        void ShowDeviceInfoDialog();

        int ShowFormatScardDialog();

        void ShowResetFactoryDialog();

        void ShowPromptBox(unsigned int promptbox_id,unsigned int showtimes);

        void DateTimeSettingConfirm(View *view, int value);

        void GetSdcardInfo(std::stringstream &info_str);

        void SystemVersion(bool fset);

        void UpdateSDcardCap();

        void RefreshActiveViewIcon();

public:
        std::string GetVersionStr(){return version_str;}

        void ForceCloseSettingWindowAllDialog();

        BulletCollection *BulletCollection_;

        inline void SetNewSettingWinMessageReceiveFlag(bool flag){ignore_message_flag_ = flag;}
        inline bool GetNewSettingWinMessageReceiveFlag(){return ignore_message_flag_;}

        void ReturnPreviewWindow(bool backcarvideo_on_flag);

        bool ignore_message_flag_;
private:
    ListView *list_view_;
    ListView *sub_list_view_;
    Sublist *sub_list_;
    WindowManager *win_mg_;
    R* r;
    LIST_VIEW_TYPE active_mode_;
    LIST_VIEW_TYPE current_active_mode_;
    TimeSettingWindowNew *timesetting_window_;
    InfoDialog *info_dialog_;
    GraphicView *return_button_view_;
    GraphicView *home_button_view_;
    GraphicView  *view_device_;
    GraphicView  *view_adas_settings_;
    GraphicView  *view_general_settings_;
    TextView* device_label_view_;
    TextView* adas_label_view_;
    TextView* general_label_view_;
    TextView* return_label_view_;
    TextView* home_label_view_;
    std::mutex view_click_mutex_;
    std::string version_str;
	DWORD listview_background_color_;
	DWORD listview_str_background_color_;
    int select_index_;
    int device_listview_size_;
    int adas_setting_listview_size_;
    int general_setting_listview_size_;
    BITMAP sublist_icon_pic_off;
    BITMAP sublist_icon_pic_on;
    BITMAP list_icon_pic_;
    bool device_info_flag_;
    bool timesetting_show_flag_;
};
