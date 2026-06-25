/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file playback.h
 * @brief 回放presenter
 * @author id:826
 * @version v0.3
 * @date 2016-08-01
 */
// #define NDEBUG 
#include "bll_presenter/playback.h"
#include "device_model/media/player/video_player.h"
#include "device_model/media/player/jpeg_player.h"
#include "device_model/system/event_manager.h"
#include "device_model/media/media_file_manager.h"
#include "device_model/media/media_file.h"
#include "device_model/storage_manager.h"
#include "lua/lua_config_parser.h"
#include "window/window.h"
#include "window/user_msg.h"
#include "window/playback_window.h"
#include "window/status_bar_window.h"
#include "window/status_bar_bottom_window.h"//add by zhb
#include "window/preview_window.h"
#include "common/utils/utils.h"
#include "common/app_log.h"
#include "application.h"
#include "utils/utils.h"
#include "bll_presenter/audioCtrl.h"
#include "widgets/progress_bar.h" //fix me
#include "window/prompt.h"
#include "window/promptBox.h"
#include "window/bulletCollection.h"
#include "device_model/dialog_status_manager.h"
#include "bll_presenter/screensaver.h"
#include "device_model/menu_config_lua.h"
#include "device_model/system/power_manager.h"


#include <sstream>

#undef LOG_TAG
#define LOG_TAG "PlaybackPresenter"

using namespace EyeseeLinux;
using namespace std;

// #define DESTRUCT_PLAYER_WHEN_DETACH
#define DESTRUCT_PLAYER_EVERYTIME
// #define DO_NOT_DESTRUCT_PLAYER

PlaybackPresenter::PlaybackPresenter()
    : playback_win_(NULL)
    , media_info_parser_(NULL)
    , video_player_(NULL)
    , jpeg_player_(NULL)
    , video_duration_(0)
    , init_flag_(true)
{
    status_ = MODEL_UNINIT;
    storage_status_ = UMOUNT;

   //add by zhb
    this->Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_STATUSBAR_BOTTOM));
    this->Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_STATUSBAR));
    this->Attach(WindowManager::GetInstance()->GetWindow(WINDOWID_PLAYBACK));
    // FIXME: 在这里调用GetWindow来初始化recplay_win_，这可能会导致window无法绑定到presenter
    media_info_parser_ = new EyeseeThumbRetriever();
}

PlaybackPresenter::~PlaybackPresenter()
{
    if (media_info_parser_) {
        delete media_info_parser_;
        media_info_parser_ = NULL;
    }
}

void PlaybackPresenter::GetPicSize(std::string file)
{
    if(media_info_parser_ == NULL)
    {
        db_error("media info parser is NULL");
        return;
    }
    media_info_parser_->setDataSource(file.c_str());
    DEMUX_MEDIA_INFO_S media_info = {0};
    media_info_parser_->getMediaInfo(&media_info);
    if(media_info.mVideoStreamInfo[0].mCodecType == PT_JPEG){
        picsize_.Width = media_info.mVideoStreamInfo[0].mWidth;
        picsize_.Height = media_info.mVideoStreamInfo[0].mHeight;
        db_debug("picsize_.Width %d,picsize_.Height %d",
                picsize_.Width,picsize_.Height);
    }
    media_info_parser_->reset();
}

void PlaybackPresenter::ShowMediaFilePreview()
{
    const FileInfo *file = playback_win_->GetSelectMediaFileInfo();
    int ret = 0;
    if(file){
       if(status_ == MODEL_UNINIT)
           this->DeviceModelInit();
       jpeg_player_->ReleasePic();
       jpeg_player_->Reset();
       video_player_->Reset();
       if(jpeg_player_){
           string filepath;
           if (file->fileType != PHOTO_A && file->fileType != PHOTO_B
                   && file->fileType != FileType_UNKNOWN_TYPE){ //video
               filepath = file->thumb_filepath;
               db_debug("thumb filepath %s",filepath.c_str());
               ret = jpeg_player_->PrepareFile(filepath.c_str());
           }else if((file->fileType == PHOTO_A || file->fileType == PHOTO_B)
                   && (file->fileType != FileType_UNKNOWN_TYPE)){ //photo
               filepath = file->filepath;
               GetPicSize(filepath.c_str());
               ShowProgressBar(false);
               jpeg_player_->SetJpegPicSize(picsize_);
               ret = jpeg_player_->PrepareFile(filepath.c_str(),false);
           }else{  //photo
               db_error("invalid file!!");
               return;
           }

           if (ret == 0)
               jpeg_player_->ShowPic();

           if (file->fileType != PHOTO_A && file->fileType != PHOTO_B
                   && file->fileType != FileType_UNKNOWN_TYPE){
               db_debug("file is video,show start play view");
               PrepareVideoFile();
               ShowVideoStartPlayView(true);
               SetPlayDuration();
           }else{
               db_debug("file is photo");
               ShowVideoStartPlayView(false);
           }
       }else{
           db_error("jpeg player init fail?");
       }
    }else{ //fix me
       db_error("no media file,show no file icon.");
    }
}

void PlaybackPresenter::PrepareVideoFile()
{
    if(video_player_ == NULL){
        db_error("video player not init!!");
        return;
    }
    PlayerState stat = video_player_->GetStatus();
    video_player_->Reset();
    const FileInfo *file = playback_win_->GetSelectMediaFileInfo();
    if(file){
        if(file->fileType != PHOTO_A && file->fileType != PHOTO_B
                && file->fileType != FileType_UNKNOWN_TYPE){
            db_debug("prepare to play file: %s", file->filepath.c_str());
            int ret = video_player_->PreparePlay(file->filepath.c_str());
            if (ret < 0)
                db_error("prepare file fail!!");
        }else{
            db_error("invalid file type %d",file->fileType);
        }
    }else{
        db_error("invalid file!!!");
    }
}

FileType_t PlaybackPresenter::GetMediaFileType()
{
    const FileInfo *file = playback_win_->GetSelectMediaFileInfo();
    if(file){
        return file->fileType;
    }else{
        db_error("invalid file!!!");
        return FileType_UNKNOWN_TYPE;
    }
}

void PlaybackPresenter::StartPlay()
{
    if(video_player_ == NULL){
        db_error("video player not init!!");
        return;
    }
    if (jpeg_player_ != NULL) {     //在显示视频缩略图之后需要将图层内容释放掉,否则会出现绿屏
        jpeg_player_->ReleasePic();
        jpeg_player_->Reset();
    }
    video_player_->Start();
}

void PlaybackPresenter::PausePlay()
{
    if(video_player_ == NULL){
        db_error("video player not init!!");
        return;
    }
    PlayerState stat = video_player_->GetStatus();
    if(stat == PLAYER_STARTED){
        video_player_->Pause();
        db_error("pause play");
    }
}

void PlaybackPresenter::StopPlay()
{
    if(video_player_ == NULL){
       db_error("video player not init!!");
       return;
    }
    video_player_->Stop();
}

void PlaybackPresenter::OnWindowLoaded()
{
    db_debug("window load");
    if(!init_flag_){                 //第一次开机不响应sd接入事件，不获取文件列表
        db_error("init media play");
        system("echo 3 >/proc/sys/vm/drop_caches");
        db_debug("drop caches");
        this->DeviceModelInit();
        playback_win_ = reinterpret_cast<PlaybackWindow *>(WindowManager::GetInstance()->GetWindow(WINDOWID_PLAYBACK));
        playback_win_->SetRefreshFileListFlag(true);
        playback_win_->SetReturnningFlag(false);
        Layer::GetInstance()->Attach(this);
        Layer::GetInstance()->SetLayerAlpha(LAYER_UI, 150);
        if(StorageManager::GetInstance()->IsMounted())
        {
            db_error("sd is mount,show file list");
            playback_win_->SetActiveGraphicView(FRONT_VIEW);
            playback_win_->RefeshFileList(1);
            std::thread([&]{
                ShowMediaFilePreview();
                playback_win_->SetRefreshFileListFlag(false);
            }).detach();
        }else{
            db_error("sd is umount,clear file list");
            playback_win_->SetActiveGraphicView(REAR_VIEW);
            playback_win_->RefeshFileList(0);
            playback_win_->SetRefreshFileListFlag(false);
        }
        MediaFileManager::GetInstance()->Attach(this); //需要等待回放界面资源初始化之后再去订阅消息
        StorageManager::GetInstance()->Attach(this);
    }
}

void PlaybackPresenter::OnWindowDetached()
{
    db_debug("window detach");
    system("echo 3 >/proc/sys/vm/drop_caches");
    db_debug("drop caches");
    if(!init_flag_){
        db_error("deinit media play");
        MediaFileManager::GetInstance()->Detach(this);
        StorageManager *sm = StorageManager::GetInstance();
        sm->Detach(this);
        if (status_ == MODEL_INITED) {
            this->DeviceModelDeInit();
        }
    }
    if(init_flag_) init_flag_ = false;
}

int PlaybackPresenter::DeviceModelInit()
{
    db_debug("playback DeviceModelInit");
    if(status_ == MODEL_UNINIT){
        std::thread video_player_init([&]{
            if (video_player_ == NULL) {
                video_player_ = new VideoPlayer();
                video_player_->SetLoopingMode(false);
            }
        });
        std::thread jpeg_player_init([&]{
            if(jpeg_player_ == NULL) {
                jpeg_player_ = new JpegPlayer();
                jpeg_player_->SetDisplay(HLAY(0, 0));
            }
        });
        video_player_init.join();
        jpeg_player_init.join();
        video_player_->Attach(this);
        jpeg_player_->Attach(this);
        status_ = MODEL_INITED;
    }
    return 0;
}

int PlaybackPresenter::DeviceModelDeInit()
{
     db_debug("playback DeviceModelDeInit");
     Layer::GetInstance()->Detach(this);
     video_player_->Detach(this);
     jpeg_player_->Detach(this);
     std::thread video_player_deinit([&]{
        if (video_player_ != NULL) {
             video_player_->Stop();
             usleep(50*1000);
             video_player_->Reset();
             delete video_player_;
             video_player_ = NULL;
         }
     });
     std::thread jpeg_player_deinit([&]{
        if (jpeg_player_ != NULL) {
            jpeg_player_->ReleasePic();
            jpeg_player_->Reset();
            delete jpeg_player_;
            jpeg_player_ = NULL;
        }
     });
     jpeg_player_deinit.join();
     video_player_deinit.join();
     status_ = MODEL_UNINIT;
     return 0;
}

void PlaybackPresenter::RefreshUnmount()
{
    if (jpeg_player_ != NULL) {
        jpeg_player_->ReleasePic();
        jpeg_player_->Reset();
    }

    if (video_player_) {
        video_player_->Stop();
        usleep(50*1000); //加延时是为了等待stop函数执行完之后释放锁
        video_player_->Reset();
    }

}

int PlaybackPresenter::HandleGUIMessage(int msg, int val, int id)
{
    int ret = 0;
    switch(msg)
    {
        case PLAYBACK_PLAY_SEEK:
        {
            PlaySeekHandler(val * 1000);
        }
        break;

        case MSG_CHANG_STATU_PLAYBACK_TO_PREVIEW: //fix me
        {
            this->Notify((MSG_TYPE)MSG_PLAYBACK_TO_PREIVEW_CHANG_STATUS_BAR_BOTTOM);
        }
        break;

        case PLAYBACK_SHOW_MEDIA_FILE_PREVIEW: //进入回放和切换前摄后拉时只显示预览不自动播放
        {
             this->ShowMediaFilePreview();
        }
        break;

        case PLAYBACK_START_PLAY:
        {
            FileType_t filetype = GetMediaFileType();
            if(filetype != PHOTO_A && filetype != PHOTO_B
                    && filetype != FileType_UNKNOWN_TYPE){
                this->StartPlay();
            }
            else
                db_error("jpeg file or no file,can not start play!!");
        }
        break;

        case PLAYBACK_PAUSE_PLAY:
        {
            int media_file_duration = 0;
            int duration = 0;
            video_player_->GetCurrentFileDuration(media_file_duration);
            duration = video_player_->getCurrentPosition();
            if(media_file_duration - duration < 1000 && val == 1){
                db_error("video residue time %d < 1000, do not response PLAYBACK_PAUSE_PLAY msg",media_file_duration - duration);
                break;
            }
            this->PausePlay();
            this->SetPlayProgress(duration);
        }
        break;

        case PLAYBACK_STOP_PLAY:
        {
            this->StopPlay();
        }
        break;

        case PLAYBACK_START_PLAY_VIEW_CLICK: //切换文件时自动播放
        {
            FileType_t filetype = GetMediaFileType();
            if(filetype != PHOTO_A && filetype != PHOTO_B
                    && filetype != FileType_UNKNOWN_TYPE){
                ShowVideoStartPlayView(false);
                this->PrepareVideoFile();
                SetPlayDuration();
                this->StartPlay();
            }else
                db_debug("jpeg file,can not start play!!");

        }
        break;

        case PLAYBACK_RESET_MEDIA:
        {
            jpeg_player_->ReleasePic();
            jpeg_player_->Reset();
            video_player_->Stop();
            usleep(50*1000);
            video_player_->Reset();
            db_error("reset media end");
        }
        break;
    }
    return ret;
}

int PlaybackPresenter::PlaySeekHandler(int pos)
{
    int ret = 0;

    printf("pos: %dmsec\n", pos);

    int cur_pos = pos;
    int total_duration = video_player_->getDuration();

    if (cur_pos < 0) cur_pos = 0;
    if (cur_pos >= total_duration) cur_pos = total_duration;

    ret = video_player_->Seek(cur_pos);

    int new_pos = video_player_->getCurrentPosition();
    SetPlayProgress(new_pos);
    return ret;
}

// 底层通知回调
void PlaybackPresenter::Update(MSG_TYPE msg, int p_CamID, int p_recordId)
{
    db_debug("msg: %d", msg);

    switch (msg)
    {
        case MSG_STORAGE_UMOUNT:
        {
            storage_status_ = UMOUNT;
            if(playback_win_->GetReturnningFlag()){
                db_warn("return now,do not refresh file list!");
                break;
            }
            playback_win_->SetRefreshFileListFlag(true);
            while(playback_win_->GetViewClickProcFlag());
            if (!StorageManager::GetInstance()->IsMounted())
            {
                db_error("sd ummount");
                RefreshUnmount();
                this->Notify(MSG_PLAYBACK_REFRESH_FILELIST,0);
                this->Notify(MSG_PLAYBACK_SET_ACTIVEMODE,0);
            }
            playback_win_->SetRefreshFileListFlag(false);
        }
        break;

//        case MSG_STORAGE_MOUNTED:
        case MSG_DATABASE_UPDATE_FINISHED:
        {
           playback_win_ = reinterpret_cast<PlaybackWindow *>(WindowManager::GetInstance()->GetWindow(WINDOWID_PLAYBACK));
           if(playback_win_->GetReturnningFlag())
           {
                db_error("playback return now ,ignore MSG_DATABASE_UPDTE_FINISHED msg");
                break;
           }
           playback_win_->SetSdCardMountingFlag(true);
           storage_status_ = MOUNTED;
           if (StorageManager::GetInstance()->IsMounted())
           {
               if (status_ == MODEL_UNINIT) {
                   db_error("player is not init,init player.");
                   this->DeviceModelInit();
               }
               db_error("sd is mount,show file list");
               this->Notify(MSG_PLAYBACK_SET_ACTIVEMODE,0);
               this->Notify(MSG_PLAYBACK_REFRESH_FILELIST,1);
               ShowMediaFilePreview();
           }
           playback_win_->SetSdCardMountingFlag(false);
        }
        break;

        case MSG_VIDEO_PLAY_START:
        {
            this->Notify((MSG_TYPE)MSG_VIDEO_PLAY_START);
        }
        break;

        case MSG_VIDEO_PLAY_PAUSE:
        {
            this->Notify((MSG_TYPE)MSG_VIDEO_PLAY_PAUSE);
        }
        break;

        case MSG_VIDEO_PLAY_STOP:
        {
           this->Notify((MSG_TYPE)MSG_VIDEO_PLAY_STOP);
        }
        break;

        case MSG_VIDEO_PLAY_COMPLETION:
        {
           this->Notify((MSG_TYPE)MSG_VIDEO_PLAY_COMPLETION);
        }
        break;
        case MSG_BACKCARVIDEO_ON:
        {
            MenuConfigLua *mcl=MenuConfigLua::GetInstance();
            mcl->SetMenuIndexConfig(MSG_SET_PREVIEW_CAMERA,1);
            std::thread([=] {
                playback_win_->ReturnPreviewWindow(true);
            }).detach();
        }
        break;
        default:

        break;
    }
}

void PlaybackPresenter::ShowProgressBar(bool show_flag)
{
    this->Notify(MSG_CHANGE_PROGRESSBAR, show_flag);
}

void PlaybackPresenter::ShowVideoStartPlayView(bool show_flag)
{
    this->Notify(MSG_CHANGE_STARTPLAY_VIEW, show_flag);
}

void PlaybackPresenter::SetPlayDuration()
{
    int duration = 0;
    video_player_->GetCurrentFileDuration(duration);
    this->Notify(MSG_VIDEO_PLAY_SET_DURATION, duration);
}

void PlaybackPresenter::SetPlayProgress(int duration)
{
    this->Notify(MSG_VIDEO_PLAY_SET_PROGRESS, duration);
}
