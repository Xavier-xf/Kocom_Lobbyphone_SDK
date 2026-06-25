/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file playback_window.cpp
 * @brief 回放界面
 * @author id:826
 * @version v0.3
 * @date 2016-11-03
 */
//#define NDEBUG 

#include "window/preview_window.h"
#include "window/playback_window.h"
#include "window/dialog.h"
#include "debug/app_log.h"
#include "widgets/graphic_view.h"
#include "common/style.h"
#include "resource/resource_manager.h"
//#include "widgets/button.h"
//#include "widgets/text_view.h"
#include "widgets/progress_bar.h"
#include "window/window_manager.h"
#include "common/message.h"
#include "common/posix_timer.h"
#include "common/setting_menu_id.h"
#include "application.h"
#include "device_model/system/power_manager.h"
#include "bll_presenter/screensaver.h"
#include "bll_presenter/audioCtrl.h"
#include <thread>
#include "window/promptBox.h"
#include "window/prompt.h"
#include "sublist.h"
#include "device_model/storage_manager.h"
#include "window/bulletCollection.h"
#include "window/status_bar_bottom_window.h"
#include "common/utils/utils.h"
#include "bll_presenter/statusbarsaver.h"

#define FILE_CREATE_TIME 

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "PlaybackWindow"
using namespace std;
IMPLEMENT_DYNCRT_CLASS(PlaybackWindow)

void PlaybackWindow::keyProc(int keyCode, int isLongPress)
{
    switch(keyCode){
        case SDV_KEY_MENU://return 
        {

        }
        break;

        case SDV_KEY_OK://Album previous page ; fast rewind ;previous video 
        {
            
        }
        break;

        case SDV_KEY_MODE://unlock and locked ; Next video ;fast forward 
        {

        }
        break;

        case SDV_KEY_RIGHT://Album next page ;delete video
        {

        }
        break;

        case SDV_KEY_LEFT://start and pause play 
        {
            
        }
        break;

        default:

        break;
    }

}

void PlaybackWindow::SetPreviewButtonStatus()
{
    listener_->sendmsg(this, MSG_CHANG_STATU_PLAYBACK_TO_PREVIEW, 0);
}


//文件列表事件响应函数，点击文件后自动播放
void PlaybackWindow::PlaybackFileListViewClickProc()
{
    if(ignore_message_flag_){
        db_error("usb host connect,ignore message!!!");
        return;
    }
    db_debug("play back file list click...");
    if(lock_list_show_flag_)
       HideLockListTextView();
    if(delete_list_show_flag_)
       HideDeleteListTextView();
    LVITEM item;
    int ret = -1;
    ret = playback_filelist_view_->GetSelectedItem(item);
    if(ret < 0)
    {
        db_error("Get selected item fail !!!");
        return;
    }
    item_select_index_ = item.nItem;
    playback_filelist_view_->SetHilight(item.nItem);
    if(player_status_ == PLAYING){
        db_error("video is playing,stop video play");
        listener_->sendmsg(this, PLAYBACK_STOP_PLAY,0);
    }
    listener_->sendmsg(this, PLAYBACK_SHOW_MEDIA_FILE_PREVIEW,0);
    //listener_->sendmsg(this, PLAYBACK_START_PLAY_VIEW_CLICK,0);
    db_debug("item index %d",item.nItem);
    db_debug("front_file_sum_ %d rear_file_sum_ %d select file index %d",
               front_file_sum_,rear_file_sum_,item_select_index_);
    if(active_graphic_view_ == FRONT_VIEW){
       if(frontfile_info_.size() != 0)
           RefreshFileInfo(item_select_index_,front_file_sum_);
       else
           RefreshFileInfo(item_select_index_,front_file_sum_,true);
    }else if(active_graphic_view_ == REAR_VIEW){
       if(rearfile_info_.size() != 0)
           RefreshFileInfo(item_select_index_,rear_file_sum_);
       else
           RefreshFileInfo(item_select_index_,rear_file_sum_,true);
    }
}

int PlaybackWindow::HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam)
{
    static bool list_click_flag = false;

    switch (message) 
    {
        if(ignore_message_flag_){
            db_error("usb host connect,ignore message!!!");
            return -1;
        }
        case MSG_PAINT: {	
        	break;
        }
            return HELP_ME_OUT;
        case MSG_ERASEBKGND:
            {
#if 0
                HDC hdc = (HDC)wparam;
                const RECT* clip = (const RECT*) lparam;
                BOOL fGetDC = FALSE;
                RECT rcTemp;

                if (hdc == 0) {
                    hdc = GetClientDC (hwnd);
                    fGetDC = TRUE;
                }

                if (clip) {
                    rcTemp = *clip;
                    ScreenToClient (hwnd, &rcTemp.left, &rcTemp.top);
                    ScreenToClient (hwnd, &rcTemp.right, &rcTemp.bottom);
                    IncludeClipRect (hdc, &rcTemp);	
                }
                else
                    GetClientRect (hwnd, &rcTemp);
                SetBrushColor (hdc, RGBA2Pixel (hdc, 0xFF, 0xFF, 0xFF, 0x00));
                FillBox (hdc, rcTemp.left, rcTemp.top, RECTW(rcTemp), RECTH(rcTemp));

                if (fGetDC)
                    ReleaseDC (hdc);
#endif
            }
            break;
            
         case MSG_MOUSE_FLING:
        { 
#if 0
                int direction = LOSWORD (wparam);
                //todo Ablum up down page
                if (direction == MOUSE_LEFT || direction == MOUSE_RIGHT)
                {
                    db_warn("playback window right or left flig !!!");
                     m_flig_rl = true;
                     if(direction == MOUSE_LEFT)
                        this->keyProc(SDV_KEY_OK, LONG_PRESS);
                     else
                        this->keyProc(SDV_KEY_RIGHT, LONG_PRESS);
                }
                else
                    db_warn("please right or left flig  to update Ablum page !!!");

#endif
        }break;
        case MSG_TIMER:
            break;
#if 1
        case LV_EVENT_VALUE_CHANGED://habo
            {
	        listener_->sendmsg(this, PLAYBACK_PLAY_SEEK, lv_slider_get_value(lv_obj_get_child(progress_bar_->GetHandle(), 0)));
                    /*if (direction == MOUSE_RIGHT || direction == MOUSE_LEFT) {
                        listener_->sendmsg(this, progress_bar_->GetTag(), (direction == MOUSE_RIGHT)?progress_move_step:progress_move_step_f);
                        db_error("direction %d",direction);
                    }*/
            }
            break;
#endif

        case LV_EVENT_CLICKED :
              {
#if 0
                  int x = 0,y = 0;
                    x = LOWORD(lparam);
                    y = HIWORD(lparam);
                    db_error("x %d y %d",x,y);
#endif
                    if(StorageManager::GetInstance()->GetStorageStatus() == UMOUNT){
                        db_error("SD Card umount ,ignore list view click !!!");
                        break;
                    }
                    /*ViewClickProc_flag_ = true;
                    PlaybackFileListViewClickProc();*/
                    //int id = LOSWORD(wparam);
                    //int code = HISWORD(wparam);
                    /*if (list_click_flag) {
                        list_click_flag = false;
                        PlaybackFileListViewClickProc();
                    }else if (code == LVN_SELCHANGE) {
                        list_click_flag = true;
                        if(lock_list_show_flag_)
                           HideLockListTextView();
                        if(delete_list_show_flag_)
                           HideDeleteListTextView();
                    }else if(code == LVN_ITEMRDOWN) {
                        list_click_flag = false;
                    }
                    ViewClickProc_flag_ = false;*/
              }
              break;
	case LV_EVENT_REFRESH:
            {
		if(StorageManager::GetInstance()->GetStorageStatus() == UMOUNT){
                        db_error("SD Card umount ,ignore list view click !!!");
                        break;
                }
		PlaybackFileListViewClickProc();
                list_click_flag = true;
                if(lock_list_show_flag_)
                    HideLockListTextView();
                if(delete_list_show_flag_)
                    HideDeleteListTextView();
		ViewClickProc_flag_ = false;
	    }
	    break;
        default:
            break;
    }
    return SystemWindow::HandleMessage(hwnd, message, wparam, lparam);
}


PlaybackWindow::PlaybackWindow(IComponent *parent)
        : SystemWindow(parent)
        , rear_button_view_(NULL)
        , front_button_view_(NULL)
        , lock_file_label_(NULL)
        , unlock_file_label_(NULL)
        , unlock_all_label_(NULL)
        , delete_current_file_label_(NULL)
        , delete_all_file_label_(NULL)
        , file_info_label_(NULL)
        , return_view_(NULL)
        , playback_view_(NULL)
        , startplay_button_view_(NULL)
        , lock_button_view_(NULL)
        , play_button_view_(NULL)
        , delete_button_view_(NULL)
        , progress_bar_(NULL)
        , playback_filelist_view_(NULL)
        , lock_list_view_(NULL)
        , lock_list_(NULL)
        , player_status_(STOPED)
        , lock_list_show_flag_(false)
        , delete_list_show_flag_(false)
        , ignore_message_flag_(false)
        , returnning_flag_(false)
        , item_select_index_(0)
        , active_graphic_view_(FRONT_VIEW)
        , file_sum_(0)
        , front_file_sum_(0)
        , rear_file_sum_(0)
        , ViewClickProc_flag_(false)
        , video_duration(0)
        , progress_move_step(2)
        , progress_move_step_f(-3)
        , refresh_filelist_flag_(false)
        , sdcard_mounting_flag_(false)
{
    wname = "PlaybackWindow";
    printf("load start\n");
    Load();
    printf("load end\n");
//    SetBackColor(0xFFFFFFFF);

#if 0
    rear_button_view_ = reinterpret_cast<GraphicView *>(GetControl("rear_gview"));  //前录按钮
    GraphicView::LoadImage(rear_button_view_, "sw_return");
    rear_button_view_->SetTag(REAR_VIEW);
    rear_button_view_->OnClick.bind(this, &PlaybackWindow::ViewClickProc);
    rear_button_view_->Show();

    front_button_view_ = reinterpret_cast<GraphicView *>(GetControl("front_gview"));  //前录按钮
    GraphicView::LoadImage(front_button_view_, "sw_return");
    front_button_view_->SetTag(FRONT_VIEW);
    front_button_view_->OnClick.bind(this, &PlaybackWindow::ViewClickProc);
    front_button_view_->Show();
#endif
    string lebel_view;
    r = R::get();

    rear_button_view_ = reinterpret_cast<TextView *>(GetControl("rear_gview"));
    rear_button_view_->SetTextStyle(DT_CENTER|DT_BOTTOM);
    rear_button_view_->SetCaptionColor(0xFFFFFFFF);
    rear_button_view_->SetClickable();
    lebel_view.clear();
    r->GetString("ml_playback_rear_filelist", lebel_view);
    rear_button_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    rear_button_view_->SetTag(REAR_VIEW);
    rear_button_view_->TextOnClick.bind(this, &PlaybackWindow::ViewClickProc);

    front_button_view_ = reinterpret_cast<TextView *>(GetControl("front_gview"));
    front_button_view_->SetTextStyle(DT_CENTER|DT_BOTTOM);
    front_button_view_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_playback_front_filelist", lebel_view);
    front_button_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    front_button_view_->SetTag(FRONT_VIEW);
    front_button_view_->TextOnClick.bind(this, &PlaybackWindow::ViewClickProc);
    front_button_view_->SetClickable();


    progress_bar_ = reinterpret_cast<ProgressBar *>(GetControl("progress_bar")); //进度条
    progress_bar_->SetTag(PLAYBACK_PLAY_SEEK);
    //progress_bar_->OnProgressSeek.bind(this, &PlaybackWindow::PlayProgressSeekProc);
    progress_bar_->SetBackColor(COLOR_PROGRESS_BAR_BG);
    progress_bar_->Hide();

    PGBTime_t pgb_time;
    pgb_time.min = 59;
    pgb_time.sec = 59;

    int min = 0;
    int max = pgb_time.sec * 60 + pgb_time.min;
    progress_bar_->SetProgressRange(min, max);
    progress_bar_->SetProgressStep(1);
    progress_bar_->SetProgressSeekValue(0);
    ::create_timer(this, &play_timer_id_, PlayProgressUpdate);

    return_view_ = reinterpret_cast<GraphicView *>(GetControl("return_gview"));  //返回按钮
    GraphicView::LoadImage(return_view_, "return");
    return_view_->SetTag(RETURN_VIEW);
    return_view_->OnClick.bind(this, &PlaybackWindow::ViewClickProc);
    return_view_->Show();
    return_label_label_ = reinterpret_cast<TextView *>(GetControl("return_label"));
    return_label_label_->SetTextStyle(DT_CENTER|DT_TOP);
    return_label_label_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_listview_return", lebel_view);
    return_label_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    return_label_label_->SetTag(RETURN_VIEW);
    return_label_label_->TextOnClick.bind(this, &PlaybackWindow::ViewClickProc);
    return_label_label_->Show();

    playback_view_ = reinterpret_cast<GraphicView *>(GetControl("playback_gview"));  //回放界面
    playback_view_->OnClick.bind(this, &PlaybackWindow::PlayBackClickProc);
    playback_view_->SetBackColor(0x0);
    playback_view_->Show();

    lock_button_view_ = reinterpret_cast<GraphicView *>(GetControl("lock_gview"));  //锁按钮
    GraphicView::LoadImage(lock_button_view_, "playback_lock");
    lock_button_view_->SetTag(LOCK_VIEW);
    lock_button_view_->OnClick.bind(this, &PlaybackWindow::ViewClickProc);
    lock_button_view_->Show();

    play_button_view_ = reinterpret_cast<GraphicView *>(GetControl("play_gview"));  //播放按钮
    GraphicView::LoadImage(play_button_view_, "playback_play");
    play_button_view_->SetTag(PLAY_VIEW);
    play_button_view_->OnClick.bind(this, &PlaybackWindow::ViewClickProc);
    play_button_view_->Show();

    startplay_button_view_= reinterpret_cast<GraphicView *>(GetControl("start_play_gview"));  //回放界面播放按钮
    GraphicView::LoadImage(startplay_button_view_, "start_play");
    startplay_button_view_->SetTag(START_PLAY_VIEW);
    startplay_button_view_->OnClick.bind(this, &PlaybackWindow::ViewClickProc);
    startplay_button_view_->Hide();

    delete_button_view_ = reinterpret_cast<GraphicView *>(GetControl("delete_gview"));  //删除按钮
    GraphicView::LoadImage(delete_button_view_, "playback_delete");
    delete_button_view_->SetTag(DELETE_VIEW);
    delete_button_view_->OnClick.bind(this, &PlaybackWindow::ViewClickProc);
    delete_button_view_->Show();

    playback_filelist_view_ = reinterpret_cast<ListView *> (GetControl("playback_list"));  //文件列表
    playback_filelist_view_->SetBackColor(0xFF2E2E2E);
    playback_filelist_view_->SetHilightColor(0xFF3E3EFF);
    /*SetWindowElementAttr(playback_filelist_view_->GetHandle(), WE_BGC_HIGHLIGHT_ITEM,
                          Pixel2DWORD(HDC_SCREEN, 0xFF3E3EFF));*/

    //init the first listitem cols //fix me
    LVCOLUMN col[3];
    std::vector<LVCOLUMN> columns;
    col[0].pfnCompare = NULL;
    col[0].width = FILELISTVIEW_FIRST_COL_W;
    col[0].colFlags = LVHF_LEFTALIGN;
    col[0].colmFlags = LVCF_LEFTALIGN_IMAGE;
    col[0].pszHeadText = "image";
    col[0].nCols = PLAYBACKLIST_FIRST_COL;
    columns.push_back(col[0]);
    col[1].pfnCompare = NULL;
    col[1].width = FILELISTVIEW_SECOND_COL_W;
    col[1].colFlags = LVCF_CENTERALIGN;
    col[1].colmFlags = LVCF_CENTERALIGN_IMAGE;
    col[1].pszHeadText = "str";
    col[1].nCols =PLAYBACKLIST_SECOND_COL;
    columns.push_back(col[1]);
    col[2].pfnCompare = NULL;
    col[2].width = FILELISTVIEW_THIRD_COL_W;
    col[2].colFlags = LVCF_RIGHTALIGN;
    col[2].colmFlags = LVCF_RIGHTALIGN;
    col[2].pszHeadText = "image";
    col[2].nCols = PLAYBACKLIST_THIRD_COL;
    columns.push_back(col[2]);
    playback_filelist_view_->SetColumns(columns,false);
    //::LoadBitmapFromFile(HDC_SCREEN, &front_icon_pic_, R::get()->GetImagePath("file_video_A").c_str());
    //::LoadBitmapFromFile(HDC_SCREEN, &rear_icon_pic_, R::get()->GetImagePath("file_video_B").c_str());
    //::LoadBitmapFromFile(HDC_SCREEN, &photo_icon_pic_, R::get()->GetImagePath("file_photo").c_str());
    //::LoadBitmapFromFile(HDC_SCREEN, &lock_icon_pic_, R::get()->GetImagePath("file_lock").c_str());

    playback_filelist_view_->Show();

    lock_file_label_ = reinterpret_cast<TextView *>(GetControl("lock_file_label"));
    lock_file_label_->SetTextStyle(DT_LEFT|DT_VCENTER);
    lock_file_label_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_playback_lock_file", lebel_view);
    lock_file_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lock_file_label_->SetTag(LOCK_FILE_VIEW);
    lock_file_label_->TextOnClick.bind(this, &PlaybackWindow::LockListViewClickProc);
    lock_file_label_->SetClickable();
    lock_file_label_->Hide();

    unlock_file_label_ = reinterpret_cast<TextView *>(GetControl("unlock_file_label"));
    unlock_file_label_->SetTextStyle(DT_LEFT|DT_VCENTER);
    unlock_file_label_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_playback_unlock_file", lebel_view);
    unlock_file_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    unlock_file_label_->SetTag(UNLOCK_FILE_VIEW);
    unlock_file_label_->TextOnClick.bind(this, &PlaybackWindow::LockListViewClickProc);
    unlock_file_label_->SetClickable();
    unlock_file_label_->Hide();

    unlock_all_label_ = reinterpret_cast<TextView *>(GetControl("unlock_all_label"));
    unlock_all_label_->SetTextStyle(DT_LEFT|DT_VCENTER);
    unlock_all_label_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_playback_unlock_all_files", lebel_view);
    unlock_all_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    unlock_all_label_->SetTag(UNLOCK_ALL_VIEW);
    unlock_all_label_->TextOnClick.bind(this, &PlaybackWindow::LockListViewClickProc);
    unlock_all_label_->SetClickable();
    unlock_all_label_->Hide();

    delete_current_file_label_ = reinterpret_cast<TextView *>(GetControl("delete_file_label"));
    delete_current_file_label_->SetTextStyle(DT_LEFT|DT_VCENTER);
    delete_current_file_label_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_playback_delete_file", lebel_view);
    delete_current_file_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    delete_current_file_label_->SetTag(DELETE_FILE_VIEW);
    delete_current_file_label_->TextOnClick.bind(this, &PlaybackWindow::DeleteListViewClickProc);
    delete_current_file_label_->SetClickable();
    delete_current_file_label_->Hide();

    delete_all_file_label_ = reinterpret_cast<TextView *>(GetControl("delete_all_label"));
    delete_all_file_label_->SetTextStyle(DT_LEFT|DT_VCENTER);
    delete_all_file_label_->SetCaptionColor(0xFFFFFFFF);
    lebel_view.clear();
    r->GetString("ml_playback_delete_all_files", lebel_view);
    delete_all_file_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    delete_all_file_label_->SetTag(DELETE_ALL_VIEW);
    delete_all_file_label_->TextOnClick.bind(this, &PlaybackWindow::DeleteListViewClickProc);
    delete_all_file_label_->SetClickable();
    delete_all_file_label_->Hide();

    file_info_label_ = reinterpret_cast<TextView *>(GetControl("file_info_label"));
    file_info_label_->SetTextStyle(DT_CENTER|DT_VCENTER);
    file_info_label_->SetCaptionColor(0xFFFFFFFF);
    file_info_label_->SetCaptionEx("file info");
    file_info_label_->Show();

#if 0
    lock_list_ = new Sublist(this);
    lock_list_->OnListItemClick.bind(this, &PlaybackWindow::LockListViewClickProc);
    lock_list_view_ = reinterpret_cast<ListView *> (GetControl("lock_list"));  //设置界面内容
    lock_list_view_->SetBackColor(0xFFFFFFFF);
    SetWindowElementAttr(lock_list_view_->GetHandle(), WE_BGC_HIGHLIGHT_ITEM,
                      Pixel2DWORD(HDC_SCREEN, 0xFFFFFFFF));
    columns.clear();
    col[0].pfnCompare = NULL;
    col[0].width = 120;
    col[0].colFlags = LVHF_CENTERALIGN;
    col[0].colmFlags = LVCF_CENTERALIGN_IMAGE;
    col[0].pszHeadText = "str";
    col[0].nCols = 0;
    columns.push_back(col[0]);
    lock_list_view_->SetColumns(columns,false);
    InitLockListtViewItem();
    lock_list_view_->Hide();
#endif

}

PlaybackWindow::~PlaybackWindow()
{
    db_debug("PlaybackWindow destruct");
    if(return_view_)
        GraphicView::UnloadImage(return_view_);
    if(playback_view_)
        GraphicView::UnloadImage(playback_view_);
    if(startplay_button_view_)
        GraphicView::UnloadImage(startplay_button_view_);
    if(lock_button_view_)
        GraphicView::UnloadImage(lock_button_view_);
    if(play_button_view_)
        GraphicView::UnloadImage(play_button_view_);
    if(delete_button_view_)
        GraphicView::UnloadImage(delete_button_view_);

    //::UnloadBitmap(&front_icon_pic_);
    //::UnloadBitmap(&rear_icon_pic_);
    //::UnloadBitmap(&photo_icon_pic_);
    //::UnloadBitmap(&lock_icon_pic_);

    ::delete_timer(play_timer_id_);
}

string PlaybackWindow::GetResourceName()
{
    return string(GetClassName());
}

void PlaybackWindow::Update(MSG_TYPE msg, int p_CamID, int p_recordId)
{
    printf("handle msg:%d\n", msg);
    switch (msg)
    {
        case MSG_VIDEO_PLAY_START:
            player_status_ = PLAYING;
            HideVideoStartPlayView();
            RefreshPlayButtonView(true);
            set_period_timer(1, 0, play_timer_id_);
            progress_bar_->Show();
        break;

        case MSG_VIDEO_PLAY_PAUSE:
            stop_timer(play_timer_id_);
            player_status_ = PAUSED;
            ShowVideoStartPlayView();
            RefreshPlayButtonView(false);
        break;

        case MSG_VIDEO_PLAY_STOP:
            stop_timer(play_timer_id_);
            player_status_ = STOPED;
            ShowVideoStartPlayView();
            if(StorageManager::GetInstance()->GetStorageStatus() == MOUNTED){
                RefreshPlayButtonView(false);
            }
        break;

        case MSG_VIDEO_PLAY_COMPLETION:
            progress_bar_->SetProgressSeekValue(0);
            stop_timer(play_timer_id_);
            player_status_ = COMPLETION;
            ShowVideoStartPlayView();
            RefreshPlayButtonView(false);
        break;

        case MSG_CHANGE_PROGRESSBAR:
        {
            if(p_CamID)
                ShowProgressBar();
            else
                HideProgressBar();
        }
        break;

        case MSG_CHANGE_STARTPLAY_VIEW:
        {
            if(p_CamID)
                ShowVideoStartPlayView();
            else
                HideVideoStartPlayView();
        }
        break;

        case MSG_VIDEO_PLAY_SET_DURATION:
			video_duration = p_CamID/1000;
			if(video_duration <= 20){
				progress_move_step = 2;
				progress_move_step_f = -3;
			}else if(video_duration <= 50 && video_duration > 20){
				progress_move_step = 4;
				progress_move_step_f = -5;
			}else if(video_duration <= 80 && video_duration > 50){
				progress_move_step = 7;
				progress_move_step_f = -8;
			}else if(video_duration <= 130 && video_duration > 80){
				progress_move_step = 10;
				progress_move_step_f = -11;
			}else if(video_duration > 130){
				progress_move_step = 15;
				progress_move_step_f = -16;

			}
            ResetPlayProgress();
            SetPlayDuration(p_CamID);
        break;

        case MSG_VIDEO_PLAY_SET_PROGRESS:
            SetPlayProgress(p_CamID);
        break;

        case MSG_PLAYBACK_REFRESH_FILELIST:
            if(p_CamID){
                UpdateMediaFileList();
                InitMediaFileListViewItem(FRONT_FILE);
            }else{
                UpdateMediaFileList(false);
                InitMediaFileListViewItem(FRONT_FILE);
                HideVideoStartPlayView();
                RefreshPlayButtonView(false);
                HideProgressBar();
            }
        break;

        case MSG_PLAYBACK_SET_ACTIVEMODE:
            if(p_CamID == 0)
                SetActiveGraphicView(FRONT_VIEW);
            else if(p_CamID == 1)
                SetActiveGraphicView(REAR_VIEW);
        break;
#if 0
        case MSG_APP_IS_CONNECTED:
        {
            if(WindowManager::GetInstance()->GetCurrentWinID() == WINDOWID_SETTING_NEW)
            {
                // back to preview window
                EyeseeLinux::StatusBarSaver::GetInstance()->Pause(false);
                listener_->sendmsg(this, MSG_SETTING_TO_PREVIEW, 1);
                listener_->sendmsg(this, WM_WINDOW_CHANGE, WINDOWID_PREVIEW);
            }
        }
        break;
#endif
        default:

        break;
    }
}

void PlaybackWindow::RefeshFileList(int sd_flag)
{
    if(sd_flag){
        UpdateMediaFileList();
        InitMediaFileListViewItem(FRONT_FILE);
    }else{
        UpdateMediaFileList(false);
        InitMediaFileListViewItem(FRONT_FILE);
        HideVideoStartPlayView();
        RefreshPlayButtonView(false);
        HideProgressBar();
    }

}

void PlaybackWindow::PlayProgressUpdate(union sigval sigval)
{
    PlaybackWindow *self = reinterpret_cast<PlaybackWindow *>(sigval.sival_ptr);
    self->HideVideoStartPlayView();
    self->progress_bar_->UpdateProgressByStep();
}

void PlaybackWindow::ResetPlayProgress()
{
    progress_bar_->SetProgressSeekValue(0);
}

void PlaybackWindow::SetPlayProgress(int msec)
{
    progress_bar_->SetProgressSeekValue(msec / 1000);
    db_msg("the msec is %d, msec/1000:%d", msec, msec/1000);
}

void PlaybackWindow::SetPlayDuration(int msec)
{
    progress_bar_->SetProgressRange(0, msec / 1000);
    db_msg("msec/1000 is %d", msec / 1000);
}

int PlaybackWindow::OnMouseUp(unsigned int button_status, int x, int y)
{
    return 0;
    RECT top_rect, bottom_rect;

    GetControl("return_btn")->GetRect(&top_rect);
    GetControl("gv_voice")->GetRect(&bottom_rect);

    db_info("touch.y: %d, top.y: %d, bottom.y: %d", y, top_rect.top, bottom_rect.top);

    if (y > top_rect.top && y < bottom_rect.top) {
         if (OnClick) OnClick(this);
    }

    return View::OnMouseUp(button_status, x, y);
}

void PlaybackWindow::OnLanguageChanged()
{
    string lebel_view;
    r->GetString("ml_listview_return", lebel_view);
    return_label_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_playback_rear_filelist", lebel_view);
    rear_button_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_playback_front_filelist", lebel_view);
    front_button_view_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_playback_unlock_all_files", lebel_view);
    unlock_all_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_playback_lock_file", lebel_view);
    lock_file_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_playback_unlock_file", lebel_view);
    unlock_file_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_playback_delete_file", lebel_view);
    delete_current_file_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
    lebel_view.clear();
    r->GetString("ml_playback_delete_all_files", lebel_view);
    delete_all_file_label_->SetCaptionEx(const_cast<char*>(lebel_view.c_str()));
}

void PlaybackWindow::PreInitCtrl(View *ctrl, string &ctrl_name)
{
//    if (ctrl_name == "progress_bar")
//        ctrl->SetCtrlTransparentStyle(false);
//    else
//        ctrl->SetCtrlTransparentStyle(true);
}

void PlaybackWindow::ViewClickProc(View *control)
{
    printf("PlaybackWindow::ViewClickProc\n");
    if(ignore_message_flag_){
        db_error("usb host connect,ignore message!!!");
        return;
    }
    int tag = control->GetTag();
    bool refresh_playpreview = false;
    switch(tag)
    {
        case FRONT_VIEW:
        {
	    printf("FRONT_VIEW\n");
            if(lock_list_show_flag_)
                HideLockListTextView();
            if(delete_list_show_flag_)
                HideDeleteListTextView();
            InitMediaFileListViewItem(FRONT_FILE);
            active_graphic_view_ = FRONT_VIEW;
            refresh_playpreview = true;
        }
        break;

        case REAR_VIEW:
        {
	    printf("REAR_VIEW\n");
            if(lock_list_show_flag_)
                HideLockListTextView();
            if(delete_list_show_flag_)
                HideDeleteListTextView();
            InitMediaFileListViewItem(REAR_FILE);
            active_graphic_view_ = REAR_VIEW;
            refresh_playpreview = true;
        }
        break;

        case RETURN_VIEW:
        {
            if(returnning_flag_ || refresh_filelist_flag_ || sdcard_mounting_flag_){
                db_warn("playback window returnning or refresh filelst or sd card mounting now, can not return");
                break;
            }
            returnning_flag_ = true;
            Hide();
            usleep(1000*1000);//等待窗口隐藏使切换窗口时不会出现图标还残留出现在下一个窗口
            if(lock_list_show_flag_)
                HideLockListTextView();
            if(delete_list_show_flag_)
                HideDeleteListTextView();
            if(player_status_ == PLAYING){
                db_error("video is playing,stop video play");
                listener_->sendmsg(this, PLAYBACK_STOP_PLAY,0);
            }
            db_error("return view click");
            listener_->sendmsg(this, MSG_CHANG_STATU_PLAYBACK_TO_PREVIEW, 0);
            listener_->sendmsg(this, WM_WINDOW_CHANGE, WINDOWID_PREVIEW);
            EyeseeLinux::StatusBarSaver::GetInstance()->Pause(false);
        }
        break;

        case LOCK_VIEW:
        {
            if(delete_list_show_flag_)
                HideDeleteListTextView();
            db_error("lock_list_show_flag_ %d",lock_list_show_flag_);
            if(lock_list_show_flag_)
                HideLockListTextView();
            else
                ShowLockListTextView();
        }
        break;

        case PLAY_VIEW:
        {
            if(refresh_filelist_flag_)
                break;
            if(lock_list_show_flag_)
                HideLockListTextView();
            if(delete_list_show_flag_)
                HideDeleteListTextView();
            db_error("player_status_ %d",player_status_);
            if(player_status_ == STOPED || player_status_ == PAUSED ||
                    player_status_ == COMPLETION){
                listener_->sendmsg(this,PLAYBACK_START_PLAY,0);
            }else if(player_status_ == PLAYING){
                listener_->sendmsg(this,PLAYBACK_PAUSE_PLAY,1);
            }
	    printf("PLAY_VIEW...\n");
        }
        break;

        case DELETE_VIEW:
        {
            if(lock_list_show_flag_)
                HideLockListTextView();

            if(delete_list_show_flag_)
                HideDeleteListTextView();
            else
                ShowDeleteListTextView();

        }
        break;

        case START_PLAY_VIEW:
        {
            if(refresh_filelist_flag_)
                break;
            if(lock_list_show_flag_)
                HideLockListTextView();
            if(delete_list_show_flag_)
                HideDeleteListTextView();
            db_error("start play view....");
            if(player_status_ == STOPED || player_status_ == PAUSED ||
                    player_status_ == COMPLETION){
                listener_->sendmsg(this,PLAYBACK_START_PLAY,0);
            }
        }
        break;

        default:

        break;
    }
    if(refresh_playpreview){
        if(player_status_ == PLAYING){
               printf("video is playing,stop video play\n");
               listener_->sendmsg(this, PLAYBACK_STOP_PLAY,0);
        }
        if(active_graphic_view_ == FRONT_VIEW){
            if(!frontfile_info_.empty()){  //
		printf("reset player...FRONT_VIEW\n");
                listener_->sendmsg(this, PLAYBACK_RESET_MEDIA, 0);
                printf("reset player...FRONT_VIEW\n");
                listener_->sendmsg(this, PLAYBACK_SHOW_MEDIA_FILE_PREVIEW,0);
//                listener_->sendmsg(this, MSG_START_PLAY_VIEW_CLICK,0);
            }
        }else if(active_graphic_view_ == REAR_VIEW){
            if(!rearfile_info_.empty()){  //
		printf("reset player...REAR_VIEW\n");
                listener_->sendmsg(this, PLAYBACK_RESET_MEDIA, 0);
                printf("reset player...REAR_VIEW\n");
                listener_->sendmsg(this, PLAYBACK_SHOW_MEDIA_FILE_PREVIEW,0);
//                listener_->sendmsg(this, MSG_START_PLAY_VIEW_CLICK,0);
            }
        }
    }
}

void PlaybackWindow::AddMediaFileLockAttribute(FileInfo *fileinfo)
{
    if(fileinfo->fileType == VIDEO_A || fileinfo->fileType == VIDEO_B){
        string file_name_bak,thum_name_bak;
        string thumb_suffix,video_suffix;
        string src_file_path,dest_file_path;
        string filetype;
        if(fileinfo->fileType == VIDEO_A)
        {
            filetype = "videoA_SOS";
            thumb_suffix = "F_ths_SOS.jpg";
            video_suffix = "F_SOS.ts";
        }else if(fileinfo->fileType == VIDEO_B){
            filetype = "videoB_SOS";
            thumb_suffix = "R_ths_SOS.jpg";
            video_suffix = "R_SOS.ts";
        }
        file_name_bak = fileinfo->filename;
        thum_name_bak = fileinfo->filename;
        file_name_bak = file_name_bak + video_suffix;
//        db_error("file_name_bak %s video_suffix %s",
//                file_name_bak.c_str(),video_suffix.c_str());
        string dts_path;
        if(fileinfo->fileType == VIDEO_A)
            dts_path = DIR_2CAT(MOUNT_PATH, EVENT_DIR_A);
        else if(fileinfo->fileType == VIDEO_B)
            dts_path = DIR_2CAT(MOUNT_PATH, EVENT_DIR_B);
        if(fileinfo->isHaveThumbJPG == 1)
        {
            src_file_path = fileinfo->thumb_filepath;
            fileinfo->thumb_filename = thum_name_bak + thumb_suffix;
            fileinfo->thumb_filepath = dts_path + fileinfo->thumb_filename;
            dest_file_path = fileinfo->thumb_filepath;
//            db_error("src_file_path %s, dest_file_path %s",
//                                src_file_path.c_str(), dest_file_path.c_str());
            RemoveMediaFile(src_file_path, dest_file_path, filetype, 1,false);
        }
        else
            fileinfo->thumb_filename = "";
        src_file_path = fileinfo->filepath;
        fileinfo->filepath = dts_path + file_name_bak;
        dest_file_path = fileinfo->filepath;
        if(fileinfo->fileType == VIDEO_A)
            fileinfo->fileType = VIDEO_A_SOS;
        else if(fileinfo->fileType == VIDEO_B)
            fileinfo->fileType = VIDEO_B_SOS;
        fileinfo->lock_flag = true;
//        db_error("src_file_path %s, dest_file_path %s",
//                src_file_path.c_str(), dest_file_path.c_str());
        RemoveMediaFile(src_file_path, dest_file_path, filetype, 1);
    }else
        db_error("file %s (file type %d)is lock,ignore msg",fileinfo->filepath.c_str(),fileinfo->fileType);
}

void PlaybackWindow::RemoveMediaFileLockAttribute(FileInfo *fileinfo)
{
    if(fileinfo->fileType == VIDEO_A_SOS || fileinfo->fileType == VIDEO_A_P
            || fileinfo->fileType == VIDEO_B_SOS || fileinfo->fileType == VIDEO_B_P){
        string file_name_bak,thum_name_bak;
        string thumb_suffix,video_suffix;
        string src_file_path,dest_file_path;
        string filetype;
        if(fileinfo->fileType == VIDEO_A_SOS || fileinfo->fileType == VIDEO_A_P)
        {
            filetype = "video_A";
            thumb_suffix = "F_ths.jpg";
            video_suffix = "F.ts";
        }else if(fileinfo->fileType == VIDEO_B_SOS || fileinfo->fileType == VIDEO_B_P){
            filetype = "video_B";
            thumb_suffix = "R_ths.jpg";
            video_suffix = "R.ts";
        }
        file_name_bak = fileinfo->filename;
        thum_name_bak = fileinfo->filename;
        file_name_bak = file_name_bak + video_suffix;
        string dts_path;
        if(fileinfo->fileType == VIDEO_A_SOS || fileinfo->fileType == VIDEO_A_P)
            dts_path = DIR_2CAT(MOUNT_PATH, VIDEO_DIR_A);
        else if(fileinfo->fileType == VIDEO_B_SOS || fileinfo->fileType == VIDEO_B_P)
            dts_path = DIR_2CAT(MOUNT_PATH, VIDEO_DIR_B);
        if(fileinfo->isHaveThumbJPG == 1)
        {
            src_file_path = fileinfo->thumb_filepath;
            fileinfo->thumb_filename = thum_name_bak + thumb_suffix;
            fileinfo->thumb_filepath = dts_path + fileinfo->thumb_filename;
            dest_file_path = fileinfo->thumb_filepath;
            RemoveMediaFile(src_file_path, dest_file_path, filetype, 0,false);
        }
        else
            fileinfo->thumb_filename = "";
        src_file_path = fileinfo->filepath;
        fileinfo->filepath = dts_path + file_name_bak;
        dest_file_path = fileinfo->filepath;
        if(fileinfo->fileType == VIDEO_A_SOS || fileinfo->fileType == VIDEO_A_P)
            fileinfo->fileType = VIDEO_A;
        else if(fileinfo->fileType == VIDEO_B_SOS || fileinfo->fileType == VIDEO_B_P)
            fileinfo->fileType = VIDEO_B;
        fileinfo->lock_flag = false;
        RemoveMediaFile(src_file_path, dest_file_path, filetype, 0);
    }else{

//        db_error("file %s (file type %d)is not need lock,ignore msg",fileinfo->filepath.c_str(),fileinfo->fileType);
    }
}

void PlaybackWindow::LockSelectMediaFile()
{
    FileInfo tmp;
    tmp.index = item_select_index_;
    std::vector<FileInfo>::iterator iter;
//    FileInfo *fileinfo;
    if(active_graphic_view_ == FRONT_VIEW){
        iter = find(frontfile_info_.begin(), frontfile_info_.end(), tmp);
        if(iter != frontfile_info_.end())
        {
            db_error("find it %s",iter->filename.c_str());
//            fileinfo = &(*iter);
        }
        else
        {
            db_error("can not find selet file %d",item_select_index_);
            return;
        }
        AddMediaFileLockAttribute(&(*iter));
        RefreshMediaFileListViewItem(&(*iter));
    }else if(active_graphic_view_ == REAR_VIEW){
        iter = find(rearfile_info_.begin(), rearfile_info_.end(), tmp);
        if(iter != rearfile_info_.end())
        {
            db_error("find it %s",iter->filename.c_str());
//            fileinfo = &(*iter);
        }
        else
        {
            db_error("can not find selet file %d",item_select_index_);
            return;
        }
        AddMediaFileLockAttribute(&(*iter));
        RefreshMediaFileListViewItem(&(*iter));
    }
}

void PlaybackWindow::UnlockSelectMediaFile(bool update_all_flag)
{
    FileInfo *fileinfo;
    if(!update_all_flag){
        if(active_graphic_view_ == FRONT_VIEW){
            FileInfo tmp;
            tmp.index = item_select_index_;
            std::vector<FileInfo>::iterator iter;
            iter = find(frontfile_info_.begin(), frontfile_info_.end(), tmp);
            if(iter != frontfile_info_.end())
            {
                db_error("find it %s",iter->filename.c_str());
                fileinfo = &(*iter);
            }
            else
            {
                db_error("can not find selet file %d",item_select_index_);
                return;
            }
            RemoveMediaFileLockAttribute(&(*iter));
            RefreshMediaFileListViewItem(&(*iter));
        }else if(active_graphic_view_ == REAR_VIEW){
            FileInfo tmp;
            tmp.index = item_select_index_;
            std::vector<FileInfo>::iterator iter;
            iter = find(rearfile_info_.begin(), rearfile_info_.end(), tmp);
            if(iter != rearfile_info_.end())
            {
                db_error("find it %s",iter->filename.c_str());
                fileinfo = &(*iter);
            }
            else
            {
                db_error("can not find selet file %d",item_select_index_);
                return;
            }
            RemoveMediaFileLockAttribute(&(*iter));
            RefreshMediaFileListViewItem(&(*iter));
        }
    }else{
        for(auto &iter:frontfile_info_){
            RemoveMediaFileLockAttribute(&iter);
        }
        for(auto &iter:rearfile_info_){
            RemoveMediaFileLockAttribute(&iter);
        }
        InitMediaFileListViewItem(FRONT_FILE);
    }
}

void PlaybackWindow::LockListViewClickProc(View *control)
{
    if(ignore_message_flag_){
        db_error("usb host connect,ignore message!!!");
        return;
    }
    int tag = control->GetTag();
    switch(tag)
    {
        case LOCK_FILE_VIEW:
        {
            LockSelectMediaFile();
        }
        break;

        case UNLOCK_FILE_VIEW:
            UnlockSelectMediaFile();
        break;

        case UNLOCK_ALL_VIEW:
            UnlockSelectMediaFile(true);
        break;
    }
    if(lock_list_show_flag_)
        HideLockListTextView();
}

void PlaybackWindow::DeleteListViewClickProc(View *control)
{
    if(ignore_message_flag_){
        db_error("usb host connect,ignore message!!!");
        return;
    }
    int tag = control->GetTag();
    switch(tag)
    {
        case DELETE_FILE_VIEW:
            DeleteSelectMediaFile();
        break;

        case DELETE_ALL_VIEW:
            DeleteSelectMediaFile(true);
        break;
    }
    if(delete_list_show_flag_)
        HideDeleteListTextView();
}


void PlaybackWindow::DeleteFile(FileInfo *fileinfo)
{
    int ret = MediaFileManager::GetInstance()->RemoveFile(fileinfo->filepath.c_str());
    if(ret < 0)
       db_error("delete file error.");
}

void PlaybackWindow::DeleteSelectMediaFile(bool delete_all_unlock_flag)
{
    FileInfo *fileinfo;
    bool refresh_flag = false;
    if(!delete_all_unlock_flag){
        if(active_graphic_view_ == FRONT_VIEW){
            FileInfo tmp;
            tmp.index = item_select_index_;
            std::vector<FileInfo>::iterator iter;
            iter = find(frontfile_info_.begin(), frontfile_info_.end(), tmp);
            if(iter != frontfile_info_.end())
            {
                db_error("find it %s",iter->filename.c_str());
                fileinfo = &(*iter);
            }
            else
            {
                db_error("can not find selet file %d",item_select_index_);
                return;
            }
            if(iter->lock_flag){
                db_error("select file is lock,can not delete!!");
                PreviewWindow *p_win  = static_cast<PreviewWindow*>(WindowManager::GetInstance()->GetWindow(WINDOWID_PREVIEW));
                if(p_win != NULL)
                    p_win->ShowPromptInfo(PROMPT_DELETE_LOCK_FILE,2);
            }else{
                if(player_status_ == PLAYING){
                    db_error("video is playing,stop video play");
                    listener_->sendmsg(this, PLAYBACK_STOP_PLAY,0);
                }
                DeleteFile(&(*iter));
                db_error("delete file %s",iter->filepath.c_str());
                UpdateMediaFileList();
                InitMediaFileListViewItem(FRONT_FILE);
                refresh_flag = true;
            }
        }else if(active_graphic_view_ == REAR_VIEW){
            FileInfo tmp;
            tmp.index = item_select_index_;
            std::vector<FileInfo>::iterator iter;
            iter = find(rearfile_info_.begin(), rearfile_info_.end(), tmp);
            if(iter != rearfile_info_.end())
            {
                db_error("find it %s",iter->filename.c_str());
                fileinfo = &(*iter);
            }
            else
            {
                db_error("can not find selet file %d",item_select_index_);
                return;
            }
            if(iter->lock_flag){
                db_error("select file is lock,can not delete!!");
                PreviewWindow *p_win  = static_cast<PreviewWindow*>(WindowManager::GetInstance()->GetWindow(WINDOWID_PREVIEW));
                if(p_win != NULL)
                    p_win->ShowPromptInfo(PROMPT_DELETE_LOCK_FILE,2);
            }else{
                if(player_status_ == PLAYING){
                    db_error("video is playing,stop video play");
                    listener_->sendmsg(this, PLAYBACK_STOP_PLAY,0);
                }
                DeleteFile(&(*iter));
                db_error("delete file %s",iter->filepath.c_str());
                UpdateMediaFileList();
                InitMediaFileListViewItem(REAR_FILE);
                refresh_flag = true;
            }
        }
    }else{
        FileInfo tmp;
        tmp.lock_flag = 0;
        for(auto &iter:frontfile_info_){
            if(!iter.lock_flag)
            {
                db_error("find it file name %s",iter.filepath.c_str());
                DeleteFile(&iter);
                refresh_flag = true;
            }
        }
        for(auto &iter:rearfile_info_){
            if(!iter.lock_flag)
            {
                db_error("find it file name %s",iter.filepath.c_str());
                DeleteFile(&iter);
                refresh_flag = true;
            }
        }
        if(refresh_flag){
            if(player_status_ == PLAYING){
                db_error("video is playing,stop video play");
                listener_->sendmsg(this, PLAYBACK_STOP_PLAY,0);
            }
            UpdateMediaFileList();
            if(active_graphic_view_ == FRONT_VIEW)
                InitMediaFileListViewItem(FRONT_FILE);
            else if(active_graphic_view_ == REAR_VIEW)
                InitMediaFileListViewItem(REAR_FILE);
        }

    }

    //文件为空时，释放多媒体资源和video图层
    if(frontfile_info_.size() == 0 && rearfile_info_.size() == 0){
        db_error("video file is empty...");
        listener_->sendmsg(this, PLAYBACK_RESET_MEDIA, 0);
        HideProgressBar();
        HideVideoStartPlayView();
    }else{
        if(refresh_flag)
            listener_->sendmsg(this, PLAYBACK_SHOW_MEDIA_FILE_PREVIEW,0);
    }
}

void PlaybackWindow::ShowLockListTextView()
{
    if(!lock_list_show_flag_)
    {
        lock_file_label_->Show();
        unlock_file_label_->Show();
        unlock_all_label_->Show();
        lock_list_show_flag_ = true;
    }else{
        db_error("lock list show flag is true!!!");
    }
}

void PlaybackWindow::HideLockListTextView()
{
    if(lock_list_show_flag_)
    {
        lock_file_label_->Hide();
        unlock_file_label_->Hide();
        unlock_all_label_->Hide();
        lock_list_show_flag_ = false;
    }else{
        db_error("lock list show flag is false!!!");
    }
}

void PlaybackWindow::ShowDeleteListTextView()
{
    if(!delete_list_show_flag_)
    {
        delete_current_file_label_->Show();
        delete_all_file_label_->Show();
        delete_list_show_flag_ = true;
    }else{
        db_error("delete list show flag is true!!!");
    }
}

void PlaybackWindow::HideDeleteListTextView()
{
    if(delete_list_show_flag_)
    {
        delete_current_file_label_->Hide();
        delete_all_file_label_->Hide();
        delete_list_show_flag_ = false;
    }else{
        db_error("delete list show flag is false!!!");
    }
}

void PlaybackWindow::HideProgressBar()
{
    if(progress_bar_->GetVisible())
        progress_bar_->Hide();
}

void PlaybackWindow::ShowProgressBar()
{
    if(!progress_bar_->GetVisible())
        progress_bar_->Show();
}

void PlaybackWindow::PlayBackClickProc(View *control)
{
    if(ignore_message_flag_){
        db_error("usb host connect,ignore message!!!");
        return;
    }
    if(lock_list_show_flag_)
       HideLockListTextView();
    if(delete_list_show_flag_)
        HideDeleteListTextView();
}

int PlaybackWindow::InitMediaFileListViewItem(FILETYPE filetype)
{
    playback_filelist_view_->RemoveAllItems();
    item_select_index_ = 0;
    if(filetype == FRONT_FILE){
        if(!frontfile_info_.empty()){
            for(auto filelist : frontfile_info_){
                InitMediaFileListViewItem(&filelist);
            }
            db_error("front_file_sum_ %d rear_file_sum_ %d select file index %d",
                    front_file_sum_,rear_file_sum_,item_select_index_);
            RefreshFileInfo(item_select_index_,front_file_sum_);
        }else{
            db_warn("no front file,empty file list.");
            InitMediaFileListViewItem(NULL);
            RefreshFileInfo(item_select_index_,front_file_sum_,true);
        }
    }else if(filetype == REAR_FILE){
        if(!rearfile_info_.empty()){
           for(auto filelist : rearfile_info_){
               InitMediaFileListViewItem(&filelist);
           }
            RefreshFileInfo(item_select_index_,rear_file_sum_);
        }else{
           db_warn("no rear file,empty file list.");
           InitMediaFileListViewItem(NULL);
           RefreshFileInfo(item_select_index_,front_file_sum_,true);
        }
    }
    playback_filelist_view_->SetHilight(0);
    playback_filelist_view_->SelectItem(0);
    return 0;
}

int PlaybackWindow::InitMediaFileListViewItem(FileInfo *fileinfo)
{
    std::string str_data;
    int index_n = 0;
    int current_val = 0;
    //set list item
    LVITEM item;
    item.dwFlags &= ~LVIF_FOLD;
    item.nItemHeight = PLAYBACK_LISTVIEW_ITEM_H;

    //set list sub item
    LVSUBITEM subdata;

    if(fileinfo != NULL){
//        db_error("file name %s",fileinfo->filename.c_str());
//        db_error("file path %s",fileinfo->filepath.c_str());
//        db_error("lock_flag %d",fileinfo->lock_flag);
//        db_error("thumb_filename %s",fileinfo->thumb_filename.c_str());
//        db_error("thumb_filepath %s",fileinfo->thumb_filepath.c_str());
//        db_error("fileCreatTime %s",fileinfo->fileCreatTime.c_str());
//        db_error("isHaveThumJPG %d",fileinfo->isHaveThumbJPG);
//        db_error("fileType %d",fileinfo->fileType);
//        db_error("index %d",fileinfo->index);
//        db_error("lock_flag %d",fileinfo->lock_flag);
	item.nItem = fileinfo->index;
        playback_filelist_view_->AddItem(item);
        StringVector str_text;

        //0 image
        subdata.pszText = const_cast<char*>("");
        subdata.nItem = fileinfo->index;
        subdata.subItem = PLAYBACKLIST_FIRST_COL;
        subdata.flags = LVFLAG_BITMAP;
	std::string path;
        if(fileinfo->fileType == PHOTO_A || fileinfo->fileType == PHOTO_B)
	    path = "S:" + R::get()->GetImagePath("file_photo");
        else if(fileinfo->fileType == VIDEO_A || fileinfo->fileType == VIDEO_A_SOS
                || fileinfo->fileType == VIDEO_A_P)
	    path = "S:" + R::get()->GetImagePath("file_video_A");
        else if(fileinfo->fileType == VIDEO_B || fileinfo->fileType == VIDEO_B_SOS
                || fileinfo->fileType == VIDEO_B_P)
	    path = "S:" + R::get()->GetImagePath("file_video_B");
        subdata.nTextColor = 0x313747;
        subdata.image = path.c_str();
	playback_filelist_view_->FillSubItem(subdata);
        //1 str
        subdata.pszText = const_cast<char*>(fileinfo->filename.c_str());
        subdata.nItem = fileinfo->index;
        subdata.subItem = PLAYBACKLIST_SECOND_COL;
        subdata.flags = 0;
        subdata.image = NULL;
        subdata.nTextColor = 0xFFFFFFFF;
        playback_filelist_view_->FillSubItem(subdata);

        //2 image
        subdata.pszText = const_cast<char*>("");
        subdata.nItem = fileinfo->index;
        subdata.subItem = PLAYBACKLIST_THIRD_COL;
        if(fileinfo->lock_flag){
            subdata.flags = LVFLAG_BITMAP;
	    path = "S:" + R::get()->GetImagePath("file_lock");
            subdata.image = path.c_str();
//            db_error("load lock pic");
        }else{
            subdata.flags = 0;
            subdata.image = NULL;
        }
        subdata.nTextColor = 0x313747;
        playback_filelist_view_->FillSubItem(subdata);
    }else{
        for(int i = 0;i < 5;i++){
            item.nItem = i;
            playback_filelist_view_->AddItem(item);
            StringVector str_text;

            //0 image
            subdata.pszText = const_cast<char*>("");
            subdata.nItem = i;
            subdata.subItem = PLAYBACKLIST_FIRST_COL;
            subdata.flags = 0;
            subdata.nTextColor = 0x313747;
            playback_filelist_view_->FillSubItem(subdata);

            //1 str
            subdata.pszText = const_cast<char*>("");
            subdata.nItem = i;
            subdata.subItem = PLAYBACKLIST_SECOND_COL;
            subdata.flags = 0;
            subdata.image = 0;
            subdata.nTextColor = 0xA4A3A3;
            playback_filelist_view_->FillSubItem(subdata);

            //2 image
            subdata.pszText = const_cast<char*>("");
            subdata.nItem = i;
            subdata.subItem = PLAYBACKLIST_THIRD_COL;
            subdata.flags = 0;
            subdata.image = 0;
            subdata.nTextColor = 0x313747;
            playback_filelist_view_->FillSubItem(subdata);
        }
        listener_->sendmsg(this, PLAYBACK_RESET_MEDIA, 0);//文件为空时，释放多媒体资源和video图层
        HideProgressBar();
        HideVideoStartPlayView();
    }
    return 0;
}

//刷新解锁加锁图标
void PlaybackWindow::RefreshMediaFileListViewItem(FileInfo *fileinfo)
{
//    db_error("file name %s",fileinfo->filename.c_str());
//    db_error("filepath %s",fileinfo->filepath.c_str());
//    db_error("thumb_filename %s",fileinfo->thumb_filename.c_str());
//    db_error("thumb_filepath %s",fileinfo->thumb_filepath.c_str());
//    db_error("fileCreatTime %s",fileinfo->fileCreatTime.c_str());
//    db_error("isHaveThumJPG %d",fileinfo->isHaveThumbJPG);
//    db_error("fileType %d",fileinfo->fileType);
//    db_error("index %d",fileinfo->index);
//    db_error("lock_flag %d",fileinfo->lock_flag);
    std::string str_data;
    int index_n = 0;
    int current_val = 0;
    LVSUBITEM subdata;
    subdata.pszText = const_cast<char*>("");
    subdata.nItem = fileinfo->index;
    subdata.subItem = PLAYBACKLIST_THIRD_COL;
    if(fileinfo->lock_flag){
        subdata.flags = LVFLAG_BITMAP;
	str_data = "S:" + R::get()->GetImagePath("file_lock");
        subdata.image = str_data.c_str();
    }else{
        subdata.flags = 0;
        subdata.image = 0;
    }
    subdata.nTextColor = 0x313747;
    playback_filelist_view_->FillSubItem(subdata);
}

void PlaybackWindow::RemoveMediaFile(string src_file_name,string dest_file_name,string file_type,int lock_status,bool changesql)
{
    char buf[256];
    db_error("src_file_name %s dest_file_name %s",src_file_name.c_str(),dest_file_name.c_str());
    snprintf(buf,sizeof(buf)-1,"mv %s %s",src_file_name.c_str(),dest_file_name.c_str());
    system(buf);
    if(changesql){
    //change the sql
        MediaFileManager::GetInstance()->SetFileInfoByName(src_file_name.c_str(),dest_file_name.c_str(),file_type,lock_status,0);
        db_error("change sql");
    }
}

void PlaybackWindow::ShowVideoStartPlayView()
{
    if(!startplay_button_view_->GetVisible()){
        startplay_button_view_->Show();
    }
}

void PlaybackWindow::HideVideoStartPlayView()
{
    if(startplay_button_view_->GetVisible())
        startplay_button_view_->Hide();
}

void PlaybackWindow::RefreshPlayButtonView(bool playing_flag)
{
    if(playing_flag){
        GraphicView::UnloadImage(play_button_view_);
        GraphicView::LoadImage(play_button_view_, "playback_pause");
    }else{
        GraphicView::UnloadImage(play_button_view_);
        GraphicView::LoadImage(play_button_view_, "playback_play");
    }
    play_button_view_->Show();
}

int PlaybackWindow::InitLockListtViewItem()
{
    lock_list_view_->RemoveAllItems();
    for(int i = 0;i < 3;i++){
        InitLockListtViewItem(NULL,NULL,NULL,i);
    }
    lock_list_view_->SetHilight(0);
    return 0;
}

int PlaybackWindow::InitLockListtViewItem(const char *file_icon_path, const char *video_name,
            const char *lock_icon_path,int index)
{
    std::string str_data;
    int index_n = 0;
    int current_val = 0;
    //set list item
    LVITEM item;
    item.dwFlags &= ~LVIF_FOLD;
    item.nItemHeight = LOCK_LISTVIEW_ITEM_H;

    //set list sub item
    LVSUBITEM subdata;

    item.nItem = index;
    lock_list_view_->AddItem(item);
    StringVector str_text;
    //0 str
    subdata.pszText = "";
    subdata.nItem = index;
    subdata.subItem = 0;
    subdata.flags = 0;
    subdata.image = 0;
    subdata.nTextColor = 0xA4A3A3;
    lock_list_view_->FillSubItem(subdata);
    return 0;
}

void PlaybackWindow::PauseVideoFilePlay()
{
    if(player_status_ == PLAYING || player_status_ == PAUSED)
       listener_->sendmsg(this, PLAYBACK_PAUSE_PLAY, 0);
}

const FileInfo* PlaybackWindow::GetSelectMediaFileInfo()
{
    FileInfo *file = NULL;
    std::vector<FileInfo>::iterator iter;
    FileInfo tmp;
    tmp.index = item_select_index_;
    db_error("active_graphic_view_ %d index %d",active_graphic_view_,item_select_index_);
    if(active_graphic_view_ == FRONT_VIEW){
        iter = find(frontfile_info_.begin(), frontfile_info_.end(), tmp);
        if(iter != frontfile_info_.end())
        {
           db_error("find it %s",iter->filename.c_str());
           file = &(*iter);
        }
        else
        {
           db_error("can not find selet file %d",item_select_index_);
           return NULL;
        }
    }else if(active_graphic_view_ == REAR_VIEW){
        iter = find(rearfile_info_.begin(), rearfile_info_.end(), tmp);
        if(iter != rearfile_info_.end())
        {
           db_error("find it %s %d",iter->filename.c_str());
           file = &(*iter);
        }
        else
        {
           db_error("can not find selet file %d",item_select_index_);
           return NULL;
        }
    }
    return file;
}

void PlaybackWindow::SetActiveGraphicView(VIEWCLICK_EVENT active_view)
{
    active_graphic_view_ = active_view;
}

string PlaybackWindow::GetVideoCreatTime(const std::string &filename)
{
    time_t timep = MediaFileManager::GetInstance()->GetFileTimestampByName(filename);
    if (timep < 0) return "";

    struct tm *tm = localtime(&timep);

    char cDate[128];
    memset(cDate, 0, sizeof(cDate));
    snprintf(cDate, sizeof(cDate)-1,"%02d-%02d",(1 + tm->tm_mon),tm->tm_mday);
    char cTime[128];
    memset(cTime, 0, sizeof(cTime));
    snprintf(cTime, sizeof(cTime)-1, "%s  %02d:%02d", cDate, tm->tm_hour, tm->tm_min);
//    db_msg("ShowVideoCreatTime : %s", cTime);
    return cTime;
}

//遍历文件列表,将前后拉文件分开,填充FileInfo结构体信息
void PlaybackWindow::GetMeidaFileInfo()
{
    int index;
    int front_video_count = -1,rear_video_count = -1;
    string file_bak,thumb_file_path,thumb_file_name;
    std::string type;
    FileInfo fileinfo;
    std::string thumb_suffix;
    if(filelist_.empty())
    {
        db_error("fatal error,file list is empty!!");
        return;
    }
    string::size_type position,suffix;
    for(auto filelist : filelist_)
    {
        file_bak = filelist;
//        db_error("file bak %s,filelist %s",file_bak.c_str(),filelist.c_str());
        fileinfo.filepath = file_bak;
        fileinfo.fileCreatTime= GetVideoCreatTime(file_bak.c_str());
        fileinfo.lock_flag = false;
        type = MediaFileManager::GetInstance()->GetMediaFileType(file_bak.c_str());
//        db_error("type %s",type.c_str());

        if(type == "video_A" || type == "videoA_SOS" || type == "videoA_PARK"
                || type == "photo_A"){
            position  = file_bak.rfind("F/");
            if( position  == string::npos)
            {
                db_error("invalid fileName:%s",fileinfo.filename.c_str());
                continue;
            }
            if(type == "photo_A"){
                fileinfo.fileType = PHOTO_A;
                suffix  = file_bak.find("F.jpg");
                if( suffix  == string::npos)
                {
                    db_error("invalid fileName:%s",fileinfo.filename.c_str());
                    continue;
                }
                thumb_suffix = "F_ths.jpg";
            } else if(type == "video_A"){
                fileinfo.fileType = VIDEO_A;
                suffix  = file_bak.find("F.ts");
                if( suffix  == string::npos)
                {
                    db_error("invalid fileName:%s",fileinfo.filename.c_str());
                    continue;
                }
                thumb_suffix = "F_ths.jpg";
            } else if(type == "videoA_SOS" || type == "videoA_PARK"){
                fileinfo.lock_flag = true;
                if(type == "videoA_SOS"){
                    fileinfo.fileType = VIDEO_A_SOS;
                    suffix  = file_bak.find("F_SOS.ts");
                    thumb_suffix = "F_ths_SOS.jpg";
                } else {  //fix me
                    fileinfo.fileType = VIDEO_A_P;
                    suffix  = file_bak.find("F_PARK.ts");
                    thumb_suffix = "F_PARK_ths.jpg";
                }
                if( suffix  == string::npos)
                {
                    db_error("invalid fileName:%s",fileinfo.filename.c_str());
                    continue;
                }
            }
            fileinfo.filename = file_bak.substr(position+2,suffix-(position+2));
            thumb_file_name = fileinfo.filename;
            thumb_file_name = thumb_file_name + thumb_suffix;
            thumb_file_path = file_bak.substr(0,position+2) + thumb_file_name;
            if(access(thumb_file_path.c_str(), F_OK) == 0){
               fileinfo.thumb_filename = thumb_file_name;
               fileinfo.thumb_filepath = thumb_file_path;
               fileinfo.isHaveThumbJPG = 1;
            }else{
                db_error("thum file %s is not exit!!",thumb_file_path.c_str());
            }
            front_video_count++;
            fileinfo.index = front_video_count;
            frontfile_info_.push_back(fileinfo);
        } else if(type == "video_B" || type == "videoB_SOS" || type == "videoB_PARK" //fix me
                || type == "photo_B"){
            file_bak = filelist;
//            db_error("file bak %s,filelist %s",file_bak.c_str(),filelist.c_str());
            fileinfo.filepath = file_bak;
            fileinfo.fileCreatTime= GetVideoCreatTime(file_bak.c_str());
            fileinfo.lock_flag = false;
            type = MediaFileManager::GetInstance()->GetMediaFileType(file_bak.c_str());
//            db_error("type %s",type.c_str());

            if(type == "video_B" || type == "videoB_SOS" || type == "videoB_PARK"
                    || type == "photo_B"){
                position  = file_bak.rfind("R/");
                if( position  == string::npos)
                {
                    db_error("invalid fileName:%s",fileinfo.filename.c_str());
                    continue;
                }
                if(type == "photo_B"){
                    fileinfo.fileType = PHOTO_B;
                    suffix  = file_bak.find("R.jpg");
                    if( suffix  == string::npos)
                    {
                        db_error("invalid fileName:%s",fileinfo.filename.c_str());
                        continue;
                    }
                    thumb_suffix = "R_ths.jpg";
                } else if(type == "video_B"){
                    fileinfo.fileType = VIDEO_B;
                    suffix  = file_bak.find("R.ts");
                    if( suffix  == string::npos)
                    {
                        db_error("invalid fileName:%s",fileinfo.filename.c_str());
                        continue;
                    }
                    thumb_suffix = "R_ths.jpg";
                } else if(type == "videoB_SOS" || type == "videoB_PARK"){
                    fileinfo.lock_flag = true;
                    if(type == "videoB_SOS"){
                        fileinfo.fileType = VIDEO_B_SOS;
                        suffix  = file_bak.find("R_SOS.ts");
                        thumb_suffix = "R_ths_SOS.jpg";
                    } else {  //fix me
                        fileinfo.fileType = VIDEO_B_P;
                        suffix  = file_bak.find("R_PARK.ts");
                        thumb_suffix = "R_PARK_ths.jpg";
                    }
                    if( suffix  == string::npos)
                    {
                        db_error("invalid fileName:%s",fileinfo.filename.c_str());
                        continue;
                    }
                }
                fileinfo.filename = file_bak.substr(position+2,suffix-(position+2));
                thumb_file_name = fileinfo.filename;
                thumb_file_name = thumb_file_name + thumb_suffix;
                thumb_file_path = file_bak.substr(0,position+2) + thumb_file_name;
                if(access(thumb_file_path.c_str(), F_OK) == 0){
                   fileinfo.thumb_filename = thumb_file_name;
                   fileinfo.thumb_filepath = thumb_file_path;
                   fileinfo.isHaveThumbJPG = 1;
                }else{
//                    db_error("thum file %s is not exit!!",thumb_file_path.c_str());
                }
                rear_video_count++;
                fileinfo.index = rear_video_count;
                rearfile_info_.push_back(fileinfo);
            }
        }
    }
    front_file_sum_ = front_video_count;
    rear_file_sum_ = rear_video_count;
    db_error("file count front:%d rear:%d",front_file_sum_,rear_file_sum_);
}

void PlaybackWindow::RefreshFileInfo(int current_index,int sum_index,bool clear_flag)
{
    char buf[50] = {0};
    if(!clear_flag){
        current_index++;   //note:文件编号是从0开始的,显示文件个数需要从一开始算起
        sum_index++;
        snprintf(buf, sizeof(buf) - 1, "%d/%d", current_index,sum_index);
    }else{
        snprintf(buf, sizeof(buf) - 1, "0/0");
    }
    file_info_label_->SetCaptionEx(const_cast<char*>(buf));
}

void PlaybackWindow::UpdateMediaFileList(bool update_flag)
{
    if(update_flag){
        file_sum_ = MediaFileManager::GetInstance()->GetMediaFileCnt("");
        filelist_.clear();
        filelist_.shrink_to_fit();
        frontfile_info_.clear();
        frontfile_info_.shrink_to_fit();
        rearfile_info_.clear();
        rearfile_info_.shrink_to_fit();
        db_error("sum %d",file_sum_);
        MediaFileManager::GetInstance()->GetMediaFileList(filelist_, 0, file_sum_, false, "");
        GetMeidaFileInfo();
    }else{
        filelist_.clear();
        filelist_.shrink_to_fit();
        frontfile_info_.clear();
        frontfile_info_.shrink_to_fit();
        rearfile_info_.clear();
        rearfile_info_.shrink_to_fit();
        item_select_index_ = 0; //拔卡后需要将index号清除掉
        front_file_sum_ = 0;
        rear_file_sum_ = 0;
        file_sum_ = 0;
    }

#if 0
    std::vector<FileInfo>::iterator iter;
    FileInfo tmp;
    tmp.index = 1;
    iter = find(frontfile_info_.begin(), frontfile_info_.end(), tmp);
    if(iter != frontfile_info_.end())
    {
        db_error("find it %s",iter->filename.c_str());
    }
    else
    {
    }
#endif
}

void PlaybackWindow::SetReturnningFlag(bool flag)
{
    returnning_flag_ = flag;
}

bool PlaybackWindow::GetReturnningFlag()
{
    return returnning_flag_;
}

bool PlaybackWindow::GetViewClickProcFlag()
{
    return ViewClickProc_flag_;
}

void PlaybackWindow::SetRefreshFileListFlag(bool flag)
{
    refresh_filelist_flag_ = flag;
}

void PlaybackWindow::ReturnPreviewWindow(bool backcarvideo_on_flag)
{
    if(returnning_flag_ || refresh_filelist_flag_ || sdcard_mounting_flag_){
        db_warn("playback window returnning or refresh filelst or sd card mounting now, can not return");
        return;
    }
    returnning_flag_ = true;
    if(lock_list_show_flag_)
        HideLockListTextView();
    if(delete_list_show_flag_)
        HideDeleteListTextView();
    if(player_status_ == PLAYING){
        db_error("video is playing,stop video play");
        listener_->sendmsg(this, PLAYBACK_STOP_PLAY,0);
    }
    db_error("return view click");
    listener_->sendmsg(this, MSG_CHANG_STATU_PLAYBACK_TO_PREVIEW, 0);
    listener_->sendmsg(this, WM_WINDOW_CHANGE, WINDOWID_PREVIEW);
    EyeseeLinux::StatusBarSaver::GetInstance()->Pause(false);
    if(backcarvideo_on_flag){
        PreviewWindow *p_win  = static_cast<PreviewWindow*>(WindowManager::GetInstance()->GetWindow(WINDOWID_PREVIEW));
        p_win->Update(MSG_BACKCARVIDEO_ON);
        db_debug("send MSG_BACKCARVIDEO_ON msg");
    }
}

void PlaybackWindow::SetSdCardMountingFlag(bool mount_flag)
{
    sdcard_mounting_flag_ = mount_flag;
}

