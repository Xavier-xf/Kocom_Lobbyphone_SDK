#include "lv_box_res_player_int.h"

#ifdef LV_USE_LINUX_TPLAYER

#include <dirent.h>
#include <allwinner/tplayer.h>

#define CEDARX_UNUSE(param) (void)param
#define ISNULL(x) if(!x){return -1;}
#define MUSIC_LIST_DIR "/usr/share/lv_86_boxes/music"

typedef struct PLAYER_CONTEXT_T {
    TPlayer *mTPlayer;
    int mSeekable;
    int mError;
    int mVideoFrameNum;
    bool mPreparedFlag;
    bool mLoopFlag;
    bool mSetLoop;
    bool mCompleteFlag;
    //char              mUrl[64];
    MediaInfo *mMediaInfo;
    sem_t mPreparedSem;
} player_context_t;

static player_context_t player_context;
static list_head_t *media_list;
static bool playing;

//* a callback for tplayer.
static int CallbackForTPlayer(void *pUserData, int msg, int param0,
        void *param1) {
    player_context_t *pPlayer = (player_context_t*) pUserData;

    CEDARX_UNUSE(param1);
    switch (msg) {
    case TPLAYER_NOTIFY_PREPARED: {
        printf("TPLAYER_NOTIFY_PREPARED,has prepared.\n");
        pPlayer->mPreparedFlag = 1;
        sem_post(&pPlayer->mPreparedSem);
        break;
    }
    case TPLAYER_NOTIFY_PLAYBACK_COMPLETE: {
        printf("TPLAYER_NOTIFY_PLAYBACK_COMPLETE\n");

        player_context.mCompleteFlag = 1;

        lv_box_res_music_play_mode(1, 0);
        break;
    }
    case TPLAYER_NOTIFY_SEEK_COMPLETE: {
        printf("TPLAYER_NOTIFY_SEEK_COMPLETE>>>>info: seek ok.\n");
        break;
    }
    case TPLAYER_NOTIFY_MEDIA_ERROR: {
        switch (param0) {
        case TPLAYER_MEDIA_ERROR_UNKNOWN: {
            printf("erro type:TPLAYER_MEDIA_ERROR_UNKNOWN\n");
            break;
        }
        case TPLAYER_MEDIA_ERROR_UNSUPPORTED: {
            printf("erro type:TPLAYER_MEDIA_ERROR_UNSUPPORTED\n");
            break;
        }
        case TPLAYER_MEDIA_ERROR_IO: {
            printf("erro type:TPLAYER_MEDIA_ERROR_IO\n");
            break;
        }
        }
        printf("error: open media source fail.\n");
        break;
    }
    case TPLAYER_NOTIFY_NOT_SEEKABLE: {
        pPlayer->mSeekable = 0;
        printf("info: media source is unseekable.\n");
        break;
    }
    case TPLAYER_NOTIFY_BUFFER_START: {
        printf("have no enough data to play\n");
        break;
    }
    case TPLAYER_NOTIFY_BUFFER_END: {
        printf("have enough data to play again\n");
        break;
    }
    case TPLAYER_NOTIFY_VIDEO_FRAME: {
        //printf("get the decoded video frame\n");
        break;
    }
    case TPLAYER_NOTIFY_AUDIO_FRAME: {
        //printf("get the decoded audio frame\n");
        break;
    }
    case TPLAYER_NOTIFY_SUBTITLE_FRAME: {
        //printf("get the decoded subtitle frame\n");
        break;
    }
    default: {
        printf("warning: unknown callback from Tinaplayer.\n");
        break;
    }
    }
    return 0;
}

int tplayer_init() {
    //* create a player.
    player_context.mTPlayer = TPlayerCreate(AUDIO_PLAYER);
    if (player_context.mTPlayer == NULL) {
        printf("can not create tplayer, quit.\n");
        return -1;
    }
    //* set callback to player.
    TPlayerSetNotifyCallback(player_context.mTPlayer, CallbackForTPlayer,
            (void*) &player_context);

    //set player start status
    player_context.mError = 0;
    player_context.mSeekable = 1;
    player_context.mPreparedFlag = 0;
    player_context.mLoopFlag = 0;
    player_context.mSetLoop = 0;
    player_context.mMediaInfo = NULL;
    player_context.mCompleteFlag = 0;
    sem_init(&player_context.mPreparedSem, 0, 0);

    TPlayerReset(player_context.mTPlayer);
    TPlayerSetDebugFlag(player_context.mTPlayer, 0);
    return 0;
}

int tplayer_exit(void) {
    if (!player_context.mTPlayer) {
        printf("player not init.\n");
        return -1;
    }
    TPlayerReset(player_context.mTPlayer);
    TPlayerDestroy(player_context.mTPlayer);
    player_context.mTPlayer = NULL;
    sem_destroy(&player_context.mPreparedSem);
    return 0;
}

static int semTimedWait(sem_t *sem, int64_t time_ms) {
    int err;

    if (time_ms == -1) {
        err = sem_wait(sem);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += time_ms % 1000 * 1000 * 1000;
        ts.tv_sec += time_ms / 1000 + ts.tv_nsec / (1000 * 1000 * 1000);
        ts.tv_nsec = ts.tv_nsec % (1000 * 1000 * 1000);

        err = sem_timedwait(sem, &ts);
    }

    return err;
}

int tplayer_play_url_first(const char *parth) {
    int waitErr = 0;

    ISNULL(player_context.mTPlayer);
    TPlayerReset(player_context.mTPlayer); //del will cause media source is unseekable
    if (TPlayerSetDataSource(player_context.mTPlayer, parth, NULL) != 0) {
        printf("TPlayerSetDataSource() return fail.\n");
        return -1;
    } else {
        printf("setDataSource end\n");
    }
    player_context.mPreparedFlag = 0;
    if (TPlayerPrepareAsync(player_context.mTPlayer) != 0) {
        printf("TPlayerPrepareAsync() return fail.\n");
        return -1;
    } else {
        printf("prepare\n");
    }
#if 0
	sem_wait(&player_context.mPreparedSem);
#else //tplayer demo
    waitErr = semTimedWait(&player_context.mPreparedSem, 10 * 1000);
    if (waitErr == -1) {
        printf("prepare fail\n");
        return -1;
    }
#endif
    printf("prepared ok\n");
    return 0;
}

int tplayer_play_url(const char *path) {
    int waitErr = 0;

    if (TPlayerReset(player_context.mTPlayer) != 0) {
        printf("TPlayerReset() return fail.\n");
        return -1;
    } else {
        printf("reset the player ok.\n");
        //zwh add from tplayerdemo
        if (player_context.mError == 1) {
            player_context.mError = 0;
        }
        //PowerManagerReleaseWakeLock("tplayerdemo");
    }
    // zwh add from tplayerdemo
    player_context.mSeekable = 1; //* if the media source is not seekable, this flag will be
    //* clear at the TINA_NOTIFY_NOT_SEEKABLE callback.
    //* set url to the tinaplayer.
    if (TPlayerSetDataSource(player_context.mTPlayer, path, NULL) != 0) {
        printf("TPlayerSetDataSource return fail.\n");
        return -1;
    } else {
        printf("setDataSource end\n");
    }

    player_context.mPreparedFlag = 0;
    if (TPlayerPrepareAsync(player_context.mTPlayer) != 0) {
        printf("TPlayerPrepareAsync() return fail.\n");
    } else {
        printf("preparing...\n");
    }
#if 0
	sem_wait(&player_context.mPreparedSem);
#else //tplayer demo
    waitErr = semTimedWait(&player_context.mPreparedSem, 10 * 1000);  //
    if (waitErr == -1) {
        printf("prepare fail\n");
        return -1;
    }
#endif
    printf("prepared ok\n");

    /* printf("TPlayerSetHoldLastPicture()\n"); */
    /* TPlayerSetHoldLastPicture(player_context.mTPlayer, 1); */
    return 0;
}

int tplayer_play(void) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }
    if (TPlayerIsPlaying(player_context.mTPlayer)) {
        printf("already palying!\n");
        return -1;
    }
    player_context.mCompleteFlag = 0;
    return TPlayerStart(player_context.mTPlayer);
}

int tplayer_pause(void) {
    ISNULL(player_context.mTPlayer);
    if (!TPlayerIsPlaying(player_context.mTPlayer)) {
        printf("not playing!\n");
        return -1;
    }
    return TPlayerPause(player_context.mTPlayer);
}

int tplayer_seekto(int nSeekTimeMs) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }

    /*
     if(TPlayerIsPlaying(player_context.mTPlayer)){
     printf("seekto can not at palying state!\n");
     return -1;
     }
     */
    return TPlayerSeekTo(player_context.mTPlayer, nSeekTimeMs);
}

int tplayer_stop(void) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }
    //zwh add
    if (!TPlayerIsPlaying(player_context.mTPlayer)) {
        printf("not playing!\n");
        return -1;
    }

    return TPlayerStop(player_context.mTPlayer);
}

int tplayer_setvolumn(int volumn) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }
    return TPlayerSetVolume(player_context.mTPlayer, volumn);
}

int tplayer_getvolumn(void) {
    ISNULL(player_context.mTPlayer);
    if (!player_context.mPreparedFlag) {
        printf("not prepared!\n");
        return -1;
    }
    return TPlayerGetVolume(player_context.mTPlayer);
}

int tplayer_setlooping(bool bLoop) {
    ISNULL(player_context.mTPlayer);
    return TPlayerSetLooping(player_context.mTPlayer, bLoop);
}

int tplayer_getduration(int *msec) {
    ISNULL(player_context.mTPlayer);
    return TPlayerGetDuration(player_context.mTPlayer, msec);
}

int tplayer_getcurrentpos(int *msec) {
    ISNULL(player_context.mTPlayer);
    return TPlayerGetCurrentPosition(player_context.mTPlayer, msec);
}

int tplayer_getcompletestate(void) {
    return player_context.mCompleteFlag;
}

int tplayer_videodisplayenable(int enable) {
    TPlayerSetVideoDisplay(player_context.mTPlayer, enable);
    return 0;
}

int tplayer_setsrcrect(int x, int y, unsigned int width, unsigned int height) {
    TPlayerSetSrcRect(player_context.mTPlayer, x, y, width, height);
    return 0;
}

int tplayer_setbrightness(unsigned int grade) {
    TPlayerSetBrightness(player_context.mTPlayer, grade);
    return 0;
}

int tplayer_setcontrast(unsigned int grade) {
    TPlayerSetContrast(player_context.mTPlayer, grade);
    return 0;
}

int tplayer_sethue(unsigned int grade) {
    TPlayerSetHue(player_context.mTPlayer, grade);
    return 0;
}

int tplayer_setsaturation(unsigned int grade) {
    TPlayerSetSaturation(player_context.mTPlayer, grade);
    return 0;
}

int tplayer_getplaying(void) {
    ISNULL(player_context.mTPlayer);
    return TPlayerIsPlaying(player_context.mTPlayer);
}

static void* MusicPlayProc(void *arg) {
    tplayer_play_url(media_list->current_node->path);
    tplayer_play();

    return NULL;
}

int lv_box_res_music_init(void) {
    return tplayer_init();
}

int lv_box_res_music_list_init(void) {
    DIR *dp;
    struct dirent *dirp;
    char compete_path[FILE_PATH_MAXT_LEN];
    char compete_name[FILE_NAME_MAXT_LEN];

    media_list = create_list();

    dp = opendir(MUSIC_LIST_DIR);
    if (!dp) {
        printf("open directory %s error\n", MUSIC_LIST_DIR);
        return -1;
    }

    /*Get mp3 file name */
    while ((dirp = readdir(dp))) {
        if (strstr(dirp->d_name, ".mp3")) {
            printf("music filename: %s/%s\n", MUSIC_LIST_DIR, dirp->d_name);
            memset(compete_path, 0, sizeof(compete_path));
            memset(compete_name, 0, sizeof(compete_name));
            snprintf(compete_path, FILE_PATH_MAXT_LEN, "%s/%s", MUSIC_LIST_DIR,
                    dirp->d_name);
            snprintf(compete_name, FILE_NAME_MAXT_LEN, "%s", dirp->d_name);
            list_append(media_list, compete_path, compete_name,
                    MEDIA_F_TYPE_MUSIC);
        }
    }

    closedir(dp);

    /* Initialize the default song, the music play activity can be played immediately */
    tplayer_play_url(media_list->current_node->path);

    return 0;
}

void lv_box_res_music_play_mode(int mode, int index) {
    if (mode == 1) {
        list_get_next_node(media_list);
    } else if (mode == 0) {
        list_get_pre_node(media_list);
    } else if (mode == 2) {
        list_skip_to_index(media_list, index);
    }
    pthread_t thread_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread_id, &attr, MusicPlayProc, NULL);
    pthread_attr_destroy(&attr);

    playing = true;
}

int lv_box_res_music_play(void) {
    playing = true;
    return tplayer_play();
}

int lv_box_res_music_pause(void) {
    playing = false;
    return tplayer_pause();
}

int lv_box_res_music_get_volumn(void) {
    return tplayer_getvolumn();
}

int lv_box_res_music_set_volumn(int volume) {
    return tplayer_setvolumn(volume);
}

void lv_box_res_music_get_percent(double *percent) {
    int curDuration, totalDuration;
    tplayer_getduration(&totalDuration);
    tplayer_getcurrentpos(&curDuration);
    *percent = (double) curDuration / totalDuration;
}

void lv_box_res_music_get_time(char *curTime, char *totalTime) {
    int ret, curDuration, totalDuration, timeMin, timeSec;
    double sec;

    ret = tplayer_getduration(&totalDuration);

    if (!ret) {
        timeMin = totalDuration / 1000 / 60;
        sec = (double) totalDuration / 1000 / 60 - timeMin;
        timeSec = sec * 60;
        sprintf(totalTime, "%02d:%02d", timeMin, timeSec);
    } else {
        strcpy(totalTime, "00:00");
    }

    ret = tplayer_getcurrentpos(&curDuration);
    if (!ret) {
        timeMin = curDuration / 1000 / 60;
        sec = (double) curDuration / 1000 / 60 - timeMin;
        timeSec = sec * 60;
        sprintf(curTime, "%02d:%02d", timeMin, timeSec);
    } else {
        strcpy(curTime, "00:00");
    }
}

list_head_t* lv_box_res_music_get_media_list() {
    return media_list;
}

bool lv_box_res_music_get_playing() {
    return playing;
}

#else

int lv_box_res_music_init(void) {
    return 0;
}

int lv_box_res_music_list_init(void) {
    return 0;
}

void lv_box_res_music_play_mode(int mode, int index) {

}

int lv_box_res_music_play(void) {
    return 0;
}

int lv_box_res_music_pause(void) {
    return 0;
}

int lv_box_res_music_get_volumn(void) {
    return 0;
}

int lv_box_res_music_set_volumn(int volume) {
    return 0;
}

void lv_box_res_music_get_percent(double *percent) {

}

void lv_box_res_music_get_time(char *curTime, char *totalTime) {
    strcpy(totalTime, "00:00");
    strcpy(curTime, "00:00");
}

list_head_t* lv_box_res_music_get_media_list() {
    return NULL;
}

bool lv_box_res_music_get_playing() {
    return false;
}

#endif

