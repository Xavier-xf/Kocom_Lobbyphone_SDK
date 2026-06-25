/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    voice_ctrl.h
 * @brief   语音采集识别模块
 * @author  id:826
 * @version v0.3
 * @date    2018-07-16
 */
#pragma once

#include <mutex>
#include <tsemaphore.h>
#include <plat_type.h>
#include <mm_common.h>
#include "common/subject.h"
//#include "common/observer.h"
#include "common/singleton.h"
#include <txz_engine.h>
#include <mpi_ai.h>

namespace EyeseeLinux {

struct AIChannelParam {
    int trackCnt;
    int sampleRate;
    int bitWidth;
    int aiCardType;
    int workmode;
    int soundmode;
    int clksel;
};

typedef struct
{
    cmd_uint16_t cmd_index;
    const char *commands;
}VOICECMDMAP;

static VOICECMDMAP cmd_array[] =
{
    {1,     "小志开始录像"},
    {2,     "小志停止录像"},
    {3,     "小志拍照"},
};

class VoiceCtrl
        :public Singleton<VoiceCtrl>
        , public ISubjectWrap(VoiceCtrl)
//        , public IObserverWrap(VoiceCtrl)
{
    friend class Singleton<VoiceCtrl>;
    public:
        enum LANG_TYPE {
            LANGUAGE_CHINESE = 0,
            LANGUAGE_ENGLISH,
        };

        VoiceCtrl();

        ~VoiceCtrl();

        void Init();

        int AIChannelInit(const AIChannelParam &param);

        int AIChannelDeinit();

        int VoiceCtrlModuleInit();

        int VoiceCtrlModuleDeinit();

		int InitAIPlayer(int ai_card);

		int DeInitAIPlayer();

		int VoiceCtrlModuleSetConfig();

		void RunVoiceCollectionThread();

        static void *VoiceCollectionThread(void *context);

        void GetStringFromVoiceCtrlModule();

        void VoiceCtrlModuleReset();

        MSG_TYPE MapCommandToGUIMessage(VOICECMDMAP *cmd);

        void AnalyzerVoiceCommand(VOICECMDMAP *cmd);
    private:
        int ai_dev_;
        int ai_chn_;
        float confidence_;
        const char* voice_commands_;
        void* txz_handle_;
        static int voicethread_flag;
        cmd_uint16_t cmdindex_;
        AIChannelParam ai_chn_param_;
        AIO_ATTR_S ai_attr_;
        pthread_t voice_collection_thread_id_;
        VOICECMDMAP cmd_map_;
        std::mutex mutex_;
};

}
