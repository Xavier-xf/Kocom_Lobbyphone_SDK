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
#pragma once

#include "common/subject.h"
#include "common/observer.h"
#include "bll_presenter/presenter.h"
#include "bll_presenter/gui_presenter_base.h"
#include "window/window_manager.h"
#include "window/playback_window.h"
#include "media/thumbretriever/EyeseeThumbRetriever.h"
#include <map>

class PlaybackWindow;
class EyeseeThumbRetriever;
namespace EyeseeLinux {

/**
 * @addtogroup BLLPresenter
 * @{
 */

/**
 * @addtogroup RecPlay
 * @{
 */

class VideoPlayer;
class JpegPlayer;
class Window;
class PlaybackPresenter
    : public GUIPresenterBase
    , public IPresenter
    , public ISubjectWrap(PlaybackPresenter)
    , public IObserverWrap(PlaybackPresenter)
{
public:
        PlaybackPresenter();
        ~PlaybackPresenter();

        void OnWindowLoaded();
        void OnWindowDetached();
        int HandleGUIMessage(int msg, int val,int id=0);
        int DeviceModelInit();
        int DeviceModelDeInit();
        void PrepareExit() {}
        void Update(MSG_TYPE msg, int p_CamID=0, int p_recordId=0);
        void RefreshUnmount();
        void ShowMediaFilePreview();
        void GetPicSize(std::string file);
        void PrepareVideoFile();
        FileType_t GetMediaFileType();
        void StartPlay();
        void PausePlay();
        void StopPlay();
        int PlaySeekHandler(int pos);
        void ShowProgressBar(bool show_flag);
        void ShowVideoStartPlayView(bool show_flag);
        void SetPlayDuration();
        void SetPlayProgress(int duration);
private:
        int status_;
        int storage_status_;
        int video_duration_;
        SIZE_S picsize_;
        PlaybackWindow *playback_win_;
        EyeseeThumbRetriever *media_info_parser_;
        VideoPlayer *video_player_;
        JpegPlayer *jpeg_player_;
        bool init_flag_;

}; /* class PlaybackPresenter */

/**  @} */
/**  @} */
} /* EyeseeLinux */
