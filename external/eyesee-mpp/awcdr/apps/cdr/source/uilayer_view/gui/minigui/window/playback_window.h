/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file playback_window.h
 * @brief 回放界面
 * @author id:826
 * @version v0.3
 * @date 2016-11-03
 */
#pragma once

#include "window/window.h"
#include "window/user_msg.h"
#include "window/window_manager.h"
#include <signal.h>
#include "device_model/media/media_file_manager.h"
#include "widgets/text_view.h"
#include "widgets/list_view.h"
#include "window/sublist.h"
#include "window/promptBox.h"
#include "window/prompt.h"
#include <sys/time.h>

#define PLAYBACK_LISTVIEW_ITEM_H 52
#define LOCK_LISTVIEW_ITEM_H     30

#define FILELISTVIEW_FIRST_COL_W    50
#define FILELISTVIEW_SECOND_COL_W   200
#define FILELISTVIEW_THIRD_COL_W    50

#define PLAYBACK_MSG_BASE (USER_MSG_BASE+15)

enum PlaybackWindowMsg {
    PLAYBACK_SHOW_MEDIA_FILE_PREVIEW = PLAYBACK_MSG_BASE,
    PLAYBACK_START_PLAY_VIEW_CLICK,
    PLAYBACK_PAUSE_PLAY,
    PLAYBACK_START_PLAY,
    PLAYBACK_STOP_PLAY,
    PLAYBACK_PLAY_SHOW_PROGRESSBAR,
    PLAYBACK_PLAY_SEEK,
    PLAYBACK_RESET_MEDIA,
};

enum FileType_t{
    PHOTO_A = 0,
    PHOTO_B,
    VIDEO_A,
    VIDEO_B,
    VIDEO_A_SOS,
    VIDEO_B_SOS,
    VIDEO_A_P,
    VIDEO_B_P,
    FileType_UNKNOWN_TYPE,
};

struct FileInfo{    
    std::string filename;
    std::string filepath;
    std::string thumb_filename;
    std::string thumb_filepath;
    std::string fileCreatTime;//2018-07-01 12:10
    int isHaveThumbJPG;
    FileType_t fileType;
    int index;
    bool lock_flag;
    bool operator == (const FileInfo & obj) const
    {
        return index == obj.index /*&& index == obj.index*/;
    }
};

enum PlayMode_t{
	PLAY_STOP = 0,
	PLAY_DELETE,
};

enum VIEWCLICK_EVENT{
    FRONT_VIEW = 0,
    REAR_VIEW,
    RETURN_VIEW,
    LOCK_VIEW,
    PLAY_VIEW,
    DELETE_VIEW,
    START_PLAY_VIEW,
};

enum {
    LOCK_FILE_VIEW = 0,
    UNLOCK_FILE_VIEW,
    UNLOCK_ALL_VIEW,
};

enum {
    DELETE_FILE_VIEW = 0,
    DELETE_ALL_VIEW,
};

enum FILETYPE{
    FRONT_FILE,
    REAR_FILE,
};

typedef enum{
    PLAYBACKLIST_FIRST_COL = 0,//image
    PLAYBACKLIST_SECOND_COL,//str
    PLAYBACKLIST_THIRD_COL,//image
    PLAYBACKLIST_FOURTH_COL,//
};

class Dialog;

class GraphicView;

class TextView;

class ProgressBar;

class PromptBox;

class BulletCollection;
class PlaybackWindow
        : public SystemWindow {
    DECLARE_DYNCRT_CLASS(PlaybackWindow, Runtime)

    enum PlayerStatus {
        STOPED = 0,
        PLAYING,
        PAUSED,
        COMPLETION,
    };

public:
    PlaybackWindow(IComponent *parent);

    virtual ~PlaybackWindow();

    virtual int OnMouseUp(unsigned int button_status, int x, int y);

    std::string GetResourceName();

    void Update(MSG_TYPE msg, int p_CamID=0, int p_recordId=0);

    int HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam);

    void PreInitCtrl(View *ctrl, std::string &ctrl_name);

    void keyProc(int keyCode, int isLongPress);

    std::string GetVideoCreatTime(const std::string &filename);

    void OnLanguageChanged();

    void SetPreviewButtonStatus();

    NotifyEvent OnClick;

    const FileInfo* GetSelectMediaFileInfo();

    void PauseVideoFilePlay();

    inline void SetPlaybackWinMessageReceiveFlag(bool flag){ignore_message_flag_ = flag;}

    void SetReturnningFlag(bool flag);

    bool GetReturnningFlag();

    bool GetViewClickProcFlag();

    void SetRefreshFileListFlag(bool flag);

    void ReturnPreviewWindow(bool backcarvideo_on_flag);

    void RefeshFileList(int sd_flag);

    void SetActiveGraphicView(VIEWCLICK_EVENT active_view);

    void SetSdCardMountingFlag(bool mount_flag);

private:
    //new
//    GraphicView *rear_button_view_;
//    GraphicView *front_button_view_;
    R* r;
    TextView* rear_button_view_;
    TextView* front_button_view_;
    TextView* lock_file_label_;
    TextView* unlock_file_label_;
    TextView* unlock_all_label_;
    TextView* delete_current_file_label_;
    TextView* delete_all_file_label_;
    TextView* file_info_label_;
    TextView* return_label_label_;
    GraphicView *return_view_;
    GraphicView *playback_view_;
    GraphicView *startplay_button_view_;
    GraphicView *lock_button_view_;
    GraphicView *play_button_view_;
    GraphicView *delete_button_view_;
    ProgressBar *progress_bar_;
    ListView *playback_filelist_view_;
    ListView *lock_list_view_; //保留
    Sublist *lock_list_;  //保留
    std::vector<std::string> filelist_;
    BITMAP front_icon_pic_;
    BITMAP rear_icon_pic_;
    BITMAP photo_icon_pic_;
    BITMAP lock_icon_pic_;
    PlayerStatus player_status_;
    bool lock_list_show_flag_;
    bool delete_list_show_flag_;
    bool ignore_message_flag_;
    bool returnning_flag_;
    std::vector<FileInfo> rearfile_info_;
    std::vector<FileInfo> frontfile_info_;
    int item_select_index_;
    VIEWCLICK_EVENT active_graphic_view_;
    unsigned int file_sum_;
    unsigned int front_file_sum_;
    unsigned int rear_file_sum_;
    timer_t play_timer_id_;
    bool ViewClickProc_flag_;
	int video_duration;
	int progress_move_step;
	int progress_move_step_f;
    bool refresh_filelist_flag_;
    bool sdcard_mounting_flag_;

private:
    void ViewClickProc(View * control);
    void PlayBackClickProc(View *control);
    void PlaybackFileListViewClickProc();
    void LockListViewClickProc(View *control);
    void DeleteListViewClickProc(View *control);
    void ShowLockListTextView();
    void HideLockListTextView();
    void ShowDeleteListTextView();
    void HideDeleteListTextView();
    void ShowVideoStartPlayView();
    void HideVideoStartPlayView();
    void ShowProgressBar();
    void HideProgressBar();
    void RefreshPlayButtonView(bool play_flag);
    int InitMediaFileListViewItem(FILETYPE filetype);
    int InitMediaFileListViewItem(FileInfo *fileinfo);
    void RefreshMediaFileListViewItem(FileInfo *fileinfo);
    void RemoveMediaFile(std::string src_file_path,std::string dest_file_path,
            std::string file_type,int lock_status,bool changesql = true);
    int InitLockListtViewItem(); //保留
    int InitLockListtViewItem(const char *file_icon_path, const char *video_name,
            const char *lock_icon_path,int index); //保留
    void RefreshFileInfo(int current_index,int sum_index,bool clear_flag=false);
    void UpdateMediaFileList(bool update_flag=true);
    void AddMediaFileLockAttribute(FileInfo *fileinfo);
    void RemoveMediaFileLockAttribute(FileInfo *fileinfo);
    void LockSelectMediaFile();
    void UnlockSelectMediaFile(bool unlock_all_flag=false);
    void DeleteSelectMediaFile(bool delete_all_unlock_flag=false);
    void DeleteFile(FileInfo *fileinfo);
    static void PlayProgressUpdate(union sigval sigval);
    void ResetPlayProgress();
    void SetPlayProgress(int msec);
    void SetPlayDuration(int msec);
    void GetMeidaFileInfo();
};
