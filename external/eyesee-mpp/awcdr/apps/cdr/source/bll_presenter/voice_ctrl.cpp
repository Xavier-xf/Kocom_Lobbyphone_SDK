/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    voice_ctrl.cpp
 * @brief   语音采集识别模块
 * @author  id:826
 * @version v0.3
 * @date    2018-07-16
 */

#include "voice_ctrl.h"
#include "common/app_log.h"
#include "common/message.h"
#include <string.h>
#include <mm_common.h>
#include <mpi_sys.h>
#include "common/setting_menu_id.h"
#include "device_model/menu_config_lua.h"

#include <map>
using namespace std;
using namespace EyeseeLinux;

int VoiceCtrl::voicethread_flag = 1;

VoiceCtrl::VoiceCtrl()
    : ai_dev_(0)
    , ai_chn_(2)
    , confidence_(0.0)
    , voice_commands_(NULL)
    , txz_handle_(NULL)
{
    memset(&txz_handle_,0,sizeof(AIChannelParam));
    memset(&cmd_map_,0,sizeof(VOICECMDMAP));
    memset(&ai_attr_, 0, sizeof(AIO_ATTR_S));
}

VoiceCtrl::~VoiceCtrl()
{
    db_debug("VoiceCtrl Destructor");
    if (voice_collection_thread_id_ > 0){
        voicethread_flag = 0;
        pthread_join(voice_collection_thread_id_,NULL);
    }
    AIChannelDeinit();
    VoiceCtrlModuleDeinit();
}

int VoiceCtrl::DeInitAIPlayer()
{
    db_debug("DeInitAIPlayer");
    AIChannelDeinit();
    return 0;
}

void VoiceCtrl::Init()
{
    db_debug("voice ctrl init");
    InitAIPlayer(PCM_CARD_TYPE_AUDIOCODEC);
    VoiceCtrlModuleInit();
}

int VoiceCtrl::InitAIPlayer(int ai_card)
{
    ai_chn_param_.trackCnt = 1;
    ai_chn_param_.sampleRate = 16000;
    ai_chn_param_.bitWidth = 16;
    ai_chn_param_.clksel = 0;
    ai_chn_param_.soundmode = AUDIO_SOUND_MODE_MONO;

    switch (ai_card) {
        case 0:
            ai_chn_param_.aiCardType = PCM_CARD_TYPE_AUDIOCODEC;
            break;
        case 1:
            ai_chn_param_.aiCardType = PCM_CARD_TYPE_SNDHDMI;
            break;
        default:
            ai_chn_param_.aiCardType = PCM_CARD_TYPE_AUDIOCODEC;
            break;
    }

    db_debug("param dump: ");
    db_debug("trackCnt: %d, sampleRate: %d, bitwidth: %d, aoCardType: %d",
            ai_chn_param_.trackCnt, ai_chn_param_.sampleRate, ai_chn_param_.bitWidth,
            ai_chn_param_.clksel, ai_chn_param_.soundmode, ai_chn_param_.aiCardType);

    return AIChannelInit(ai_chn_param_);
}

int VoiceCtrl::AIChannelInit(const AIChannelParam &param)
{
    int ret;

    lock_guard<mutex> lock(mutex_);

    ai_attr_.mChnCnt = param.trackCnt;
    ai_attr_.enSamplerate = (AUDIO_SAMPLE_RATE_E)param.sampleRate;
    ai_attr_.enBitwidth = (AUDIO_BIT_WIDTH_E)(param.bitWidth / 8 - 1);
    ai_attr_.mPcmCardId = (PCM_CARD_TYPE_E)param.aiCardType;
    ai_attr_.enWorkmode = (AIO_MODE_E)param.workmode;
    ai_attr_.u32ClkSel = param.clksel;
    db_debug("AIO_ATTR_S: [%d, %d, %d, %d, %d, %d]",
            ai_attr_.mChnCnt, ai_attr_.enSamplerate, ai_attr_.enBitwidth,
            ai_attr_.mPcmCardId, ai_attr_.enWorkmode, ai_attr_.u32ClkSel);

    AW_MPI_AI_SetPubAttr(ai_dev_, &ai_attr_);

    while(ai_chn_ < AIO_MAX_CHN_NUM)
    {
        ret = AW_MPI_AI_CreateChn(ai_dev_, ai_chn_, NULL);
        if(SUCCESS == ret)
        {
            db_debug("create ai channel[%d] success!", ai_chn_);
            break;
        }
        else if (ERR_AO_EXIST == ret)
        {
            db_debug("ai channel[%d] exist, find next!", ai_chn_);
            ai_chn_++;
        }
        else if(ERR_AO_NOT_ENABLED == ret)
        {
            db_error("audio_hw_ai not started!");
            break;
        }
        else
        {
            db_error("create ai channel[%d] fail! ret[0x%x]!", ai_chn_, ret);
            break;
        }
    }

    ret = AW_MPI_AI_EnableChn(ai_dev_, ai_chn_);
    if(SUCCESS != ret)
    {
        // ai_chn_ = MM_INVALID_CHN;
        ai_chn_ = 1; // reset to default
        db_error("fatal error! enable ai channel fail!");
        return -1;
    }

    return 0;
}

int VoiceCtrl::AIChannelDeinit()
{
    lock_guard<mutex> lock(mutex_);
    int ret = 0;
    ret = AW_MPI_AI_DisableChn(ai_dev_, ai_chn_);
    if(SUCCESS != ret)
    {
        db_error("fatal error! disable ai channel fail!");
        return -1;
    }
    ret = AW_MPI_AI_ResetChn(ai_dev_, ai_chn_);
    if(SUCCESS != ret)
    {
        db_error("fatal error! reset ai channel fail!");
        return -1;
    }
    ret = AW_MPI_AI_DestroyChn(ai_dev_, ai_chn_);
    if(SUCCESS != ret)
    {
       db_error("fatal error! destroy ai channel fail!");
       return -1;
    }
    return 0;
}

void VoiceCtrl::RunVoiceCollectionThread()
{
    voicethread_flag = 1;
    pthread_create(&voice_collection_thread_id_,NULL,VoiceCtrl::VoiceCollectionThread,this);
}

int VoiceCtrl::VoiceCtrlModuleInit()
{
    int ret = 0;
#ifdef VOICECTRL_SUPPORT
    int res = txzEngineCheckLicense(NULL, "bbfbc8d1c8b032ee5d9e24d3a5a4ca9cc7af8cf5", "/usr/", "/mnt/extsd/");//A code can only be used once
    if(res != 1)
    {
        db_error("License err ----\n");
        return -1;
    }
    txzEngineCreate(&txz_handle_);
    if (NULL == txz_handle_)
    {
        db_error("txzEngineCreate err \n");
        return -1;
    }
    if (CMD_CODE_NORMAL != txzEngineCmdInit(txz_handle_))
    {
        db_error("txzEngineInit err \n");
        return -1;
    }
    VoiceCtrlModuleSetConfig();
    VoiceCtrlModuleReset();
#endif
    return 0;
}

int VoiceCtrl::VoiceCtrlModuleSetConfig()
{
#if 0
    /* CMD configure */
    cmd_uint16_t cmd_shift_frames = 1; // 1 (0~10)
    cmd_uint16_t cmd_smooth_frames = 1; // 1 (0~30)
    cmd_uint16_t cmd_lock_frames = 1; // 1 (0~100)
    cmd_uint16_t cmd_post_max_frames = 35; // 35 (0~100)
    cmd_float32_t cmd_threshold = 0.50; // 0.70f (0.0f~1.0f)
    //printf("cmd threshod = %f\n", cmd_threshold);

    if (CMD_CODE_NORMAL != txzEngineSetConfig(txz_handle_, cmd_shiftFrames, &cmd_shift_frames)) {
        db_error( "txzEngineSetConfig cmd_shiftFrames error.\n");
        return -1;
    }
    if (CMD_CODE_NORMAL != txzEngineSetConfig(txz_handle_, cmd_smoothFrames, &cmd_smooth_frames)) {
        db_error( "txzEngineSetConfig cmd_smoothFrames error.\n");
        return -1;
    }
    if (CMD_CODE_NORMAL != txzEngineSetConfig(txz_handle_, cmd_lockFrames, &cmd_lock_frames)) {
        db_error( "txzEngineSetConfig cmd_lockFrames error.\n");
        return -1;
    }
    if (CMD_CODE_NORMAL != txzEngineSetConfig(txz_handle_, cmd_postMaxFrames, &cmd_post_max_frames)) {
        db_error( "txzEngineSetConfig cmd_postMaxFrames error.\n");
        return -1;
    }
    if (CMD_CODE_NORMAL != txzEngineSetConfig(txz_handle_, cmd_thresHold, &cmd_threshold)) {
        db_error( "txzEngineSetConfig cmd_threshold error.\n");
        return -1;
    }
#endif
    return 0;
}

void VoiceCtrl::VoiceCtrlModuleReset()
{
#ifdef VOICECTRL_SUPPORT
    txzEngineReset(txz_handle_);
#endif
}

int VoiceCtrl::VoiceCtrlModuleDeinit()
{
#ifdef VOICECTRL_SUPPORT
    txzEngineDesrtoy(&txz_handle_);
#endif
    return 0;
}

void *VoiceCtrl::VoiceCollectionThread(void *context)
{
    prctl(PR_SET_NAME,"VoiceCollectionThread",0,0,0);
    VoiceCtrl *voice_ctrl_ = reinterpret_cast<VoiceCtrl*>(context);
    AUDIO_FRAME_S stAFrame;
    memset(&stAFrame, 0, sizeof(AUDIO_FRAME_S));
    int ret = 0,result = 0;
    int cmd_count = 0,cmd_count_mod = 0;
    void *fram_date_bak = NULL;

    short *pDatabuf = NULL;
    short *pDatabuf_bak = NULL;
    short *pDatabuf_remain = NULL;
    unsigned int near_buff_len;
    near_buff_len = 1024 * voice_ctrl_->ai_chn_param_.bitWidth /8 * voice_ctrl_->ai_attr_.mChnCnt;
    pDatabuf = (short *)malloc(near_buff_len * 2);
    int nWriteLen;

    while(voicethread_flag)
    {
        pDatabuf_bak = pDatabuf;
        ret = AW_MPI_AI_GetFrame(voice_ctrl_->ai_dev_, voice_ctrl_->ai_chn_, &stAFrame, NULL, -1);
        memcpy(pDatabuf + cmd_count_mod/2,stAFrame.mpAddr,stAFrame.mLen);
        cmd_count = (stAFrame.mLen + cmd_count_mod)/320;//每次需要传入320个字节
        cmd_count_mod = (stAFrame.mLen + cmd_count_mod)%320;//取余数，保证所有数据都会传入
        pDatabuf_remain = pDatabuf + 160 * cmd_count;
        for(cmd_count;cmd_count > 0;cmd_count--)
        {
#ifdef VOICECTRL_SUPPORT
           result = txzEngineProcess(voice_ctrl_->txz_handle_,pDatabuf_bak, 160,
                                       &(voice_ctrl_->cmdindex_),&(voice_ctrl_->confidence_));
           pDatabuf_bak = pDatabuf_bak + 160;//地址后移320个字节
           if(result != CMD_CODE_NORMAL)
           {
               db_error("txzEngineProcess error, ret=%d \n",result);
               voice_ctrl_->VoiceCtrlModuleReset();
           }
           else
           {
               if (voice_ctrl_->cmdindex_ > 0)
               {
                   txzEngineGetName(voice_ctrl_->cmdindex_, &(voice_ctrl_->voice_commands_));
                   db_error(" cmdIndex=%d , command:<%s> ,confidence=%f\n",
                           voice_ctrl_->cmdindex_,voice_ctrl_->voice_commands_,voice_ctrl_->confidence_);
                   //cmdIndex=3 , command:<小志拍照> ,confidence=0.518561
                   voice_ctrl_->cmd_map_.cmd_index = voice_ctrl_->cmdindex_;
                   voice_ctrl_->cmd_map_.commands = voice_ctrl_->voice_commands_;
                   voice_ctrl_->AnalyzerVoiceCommand(&voice_ctrl_->cmd_map_);
                   voice_ctrl_->VoiceCtrlModuleReset();
               }
           }
#endif
        }
        memcpy(pDatabuf,pDatabuf_remain,cmd_count_mod);
        if (SUCCESS == ret)
        {
            ret = AW_MPI_AI_ReleaseFrame(voice_ctrl_->ai_dev_, voice_ctrl_->ai_chn_, &stAFrame, NULL);
            if (SUCCESS != ret)
            {
                db_error("release frame to ai fail! ret: %#x", ret);
            }
        }
        else
        {
            db_error("get pcm from ai in block mode fail! ret: %#x", ret);
            break;
        }
    }
    if(pDatabuf != NULL)
        free(pDatabuf);
    return 0;
}

MSG_TYPE VoiceCtrl::MapCommandToGUIMessage(VOICECMDMAP *cmd)
{
    db_error("index %d cmd %s",cmd->cmd_index,cmd->commands);
    MSG_TYPE msg;
    int index = cmd->cmd_index;
    switch(index)
    {
        case 1:
            msg = MSG_VOICE_CTRL_FRONT_PREVIEW;
        break;
        case 2:
            msg = MSG_VOICE_CTRL_REAR_PREVIEW;
        break;
        case 3:
            msg = MSG_VOICE_CTRL_TURNON_SCREEN;
        break;
        case 4:
            msg = MSG_VOICE_CTRL_TURNOFF_SCREEN;
        break;
//        case 7:
//            msg = MSG_VOICE_CTRL_TURNON_WIFI;
//        break;
//        case 8:
//            msg = MSG_VOICE_CTRL_TURNOFF_WIFI;
//        break;
        case 14:
            msg = MSG_VOICE_CTRL_TAKE_PIC;
        break;
        case 15:
            msg = MSG_VOICE_CTRL_LOCK_FILE;
        break;
        case 16:
            msg = MSG_VOICE_CTRL_TURNON_RECORDERAUDIO;
        break;
        case 17:
           msg = MSG_VOICE_CTRL_TURNOFF_RECORDERAUDIO;
        break;
        default:
            db_error("there is no mesg map");
        break;
    }
    return msg;
}

void VoiceCtrl::AnalyzerVoiceCommand(VOICECMDMAP *cmd)
{
    MSG_TYPE msg = MapCommandToGUIMessage(cmd);
    db_error("analyzer voice ctrl msg is %d",msg);
    MenuConfigLua *menuconfiglua = MenuConfigLua::GetInstance();
    int val = menuconfiglua->GetMenuIndexConfig(SETTING_VOICE_CTRL);
    if(val)
        Notify(msg);
    else
        db_error("voice ctrl switch is close");
}

