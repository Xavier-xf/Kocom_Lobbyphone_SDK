/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : audio_hw.c
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/05/25
  Last Modified :
  Description   : mpi functions implement
  Function List :
  History       :
******************************************************************************/

#define LOG_NDEBUG 0
#define LOG_TAG "audio_hw"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <memory.h>
#include <sys/prctl.h>

#include <SystemBase.h>
#include <media_common_aio.h>
#include <audio_hw.h>
#include <ConfigOption.h>

#define PlaybackRateDmix_DefaultSampleRate  (16000) //ref to pcm.PlaybackRateDmix of asound.conf
#define PlaybackRateDmix_DefaultChnCnt      (1)
#define PlaybackRateDmix_DefaultPeriodSize  (960)

#define AICapAlsaFrameNum (960)
#define AOPlayPeriodTime (60) //unit:ms

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
  #if (MPPCFG_AEC_LIB == OPTION_AEC_LIBRARY_WEBRTC)
    #include <WebRtcAec.h>
  #elif(MPPCFG_AEC_LIB == OPTION_AEC_LIBRARY_UVOICE)
    #include <UvoiceAec.h>
  #endif
#endif

#if (MPPCFG_ANS == OPTION_ANS_ENABLE)
  #if (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_WEBRTC)
    #include <WebRtcAns.h>
  #elif(MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_LSTM)
    #include <LstmAns.h>
  #elif(MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_NOSC)
    #include <NoscAns.h>
  #endif
#endif

#if (MPPCFG_AGC == OPTION_AGC_ENABLE)
#if (MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_WEBRTC)
#include <WebRtcAgc.h>
#elif (MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_AGCFLOAT)
#include <FloatAgc.h>
#endif

#endif

#include "cdx_list.h" 

//#define SOUND_CARD  "default:CARD=audiocodec"

// #define SOUND_CARD_AUDIOCODEC   "default"
#define PCM_HANDLE_IDENTIFIER_PLAY   "PlaybackRateDmix"
#define SOUND_CARD_AUDIOCODEC_PLAY   "plug:PlaybackRateDmix"
//#define SOUND_CARD_AUDIOCODEC_CAP   "CaptureMic"    //"hw:0,0"
#define SOUND_CARD_SNDHDMI      "hw:1,0"
//#define PCM_HANDLE_IDENTIFIER_CaptureDouble "CaptureDouble"
#define SOUND_CARD_UAC1         "hw:UAC1Gadget"

#define SOUND_MIXER_AUDIOCODEC   "hw:0"
#define SOUND_MIXER_SNDDAUDIO0   "hw:1"

#define SOUND_CARD_SNDDAUDIo0   "I2STX" //"hw:1,0"

//#define AI_HW_AEC_DEBUG_EN

typedef enum AI_STATES_E
{
    AI_STATE_INVALID = 0,
    AI_STATE_CONFIGURED,
    AI_STATE_STARTED,
} AI_STATES_E;

typedef enum AO_STATES_E
{
    AO_STATE_INVALID = 0,
    AO_STATE_CONFIGURED,
    AO_STATE_STARTED,
} AO_STATES_E;

typedef struct AudioInputDevice
{
    AI_STATES_E mState;

    AIO_ATTR_S mAttr; //user setting.
    PCM_CONFIG_S mCfg; //underlying config of alsaLib.
    AUDIO_TRACK_MODE_E mTrackMode;
    pthread_t mThdId;
    pthread_t mThdLoopId;
    volatile BOOL mThdRunning;

    struct list_head mChnList;
    pthread_mutex_t mChnListLock; 
    pthread_mutex_t mApiCallLock;       // to protect the api call,when used in two thread asynchronously.
    //pthread_mutex_t mAgcDbGainLock;     // to protect the agc db gain call,when used in two thread asynchronously.

    void *mpAecCookie;
    AecProcessFuncType mpAecProcessCallback;
    int aec_valid_frm;                      // flag used to indicate one valid output frame is ready or not.

    char *pCapBuf;  //used to read capture data(i.e. captureMic data)
    //char *pRefBuf;  //used to read ref data(i.e. daudio0 data), when use multi-plugin, it is not used.

    int snd_card_id; 

    //for ans process
    void *mpAnsCookie;
    AnsProcessFuncType mpAnsProcessCallback;
    int ans_valid_frm;                      // flag used to indicate one valid output frame is ready or not.

    //for agc process
    void *mpAgcCookie;
    AgcProcessFuncType mpAgcProcessCallback;
    int agc_valid_frm; //flag used to indicate one valid output frame is ready or not. 1:ready, 0:not

    BOOL mbSuspendAns;    //when in ans enable state, suspend ans and resume ans. 0:resume, 1:suspend.
    BOOL mbSuspendAec;    //when in aec enable state, suspend aec and resume aec. 0:resume, 1:suspend.

    void *cookie;
    AudioDevCallbackFuncType mpAudioDevCallback;
} AudioInputDevice;

typedef struct AudioOutputSubChl_S
{
    AO_STATES_E mState;

    AIO_ATTR_S mAttr;
    PCM_CONFIG_S mCfg;
    AUDIO_TRACK_MODE_E mTrackMode;
    pthread_t mThdId;
    volatile BOOL mThdRunning;
   
	int AoChlId;	// added to indicate id of current ao chl
    int snd_card_id;
} AudioOutputSubChl;
typedef struct AudioOutputDevice
{
    pthread_mutex_t mAOApiCallLock;   // to protect the api call,when used in two thread asynchronously.
    pthread_mutex_t mChnListLock; 
    struct list_head mChnList;
	AudioOutputSubChl PlayChls[MAX_AO_CHLS]; // changed for condition that two ao chl exist
	AO_CHN daudio0_ao_chl;
    /// config runtime arguments of pcm.PlaybackRateDmix of asound.conf
    //int mDmixSampleRate;
    //int mDmixChnNum;
    //int mDmixPeriodSize;
} AudioOutputDevice;

typedef struct AudioHwDevice
{
    BOOL mEnableFlag;
    AIO_MIXER_S mMixer;
    AudioInputDevice mCap;
    AudioOutputDevice mPlay;
} AudioHwDevice;

static AudioHwDevice gAudioHwDev[AIO_DEV_MAX_NUM];

/*
 * Because the mixerElems created by alsa-lib will not exist at init, they will be registered to system after first 
 * snd_pcm_open(), and be available after second snd_pcm_open(), so for all mixerElems available before app call
 * snd_pcm_open(), we call snd_pcm_open() specifically here.
 */
static void MakeAllMixerElemsAvailable()
{
    int err;
    snd_pcm_t *pSndPcmHandle = NULL;
    //int64_t tm1, tm2;
    //tm1 = CDX_GetSysTimeUsMonotonic();
    err = snd_pcm_open(&pSndPcmHandle, SOUND_CARD_AUDIOCODEC_PLAY, SND_PCM_STREAM_PLAYBACK, 0);
    if(0 == err)
    {
        err = snd_pcm_close(pSndPcmHandle);
        if(0 == err)
        {
            alogd("pcm open and close, all MixerElems maybe exist.");
        }
        else
        {
            aloge("fatal error! snd pcm close fail[%d]", err);
        }
    }
    else
    {
        aloge("fatal error! why snd pcm open fail?[%d]", err);
    }
    //tm2 = CDX_GetSysTimeUsMonotonic();
    //alogd("prepare alsa mixer elems, cost [%lld]us", tm2-tm1);
}

static void GeneratePlaybackRateDmixIdentifier(char *pDmixId, int nSize, int nSampleRate, int nChnNum, int nPeriodSize)
{
    pDmixId[nSize-1] = '\0';
    //snprintf(pDmixId, nSize-1, "%s:%d,%d,%d", PCM_HANDLE_IDENTIFIER_PLAY, nSampleRate, nChnNum,nPeriodSize);
    snprintf(pDmixId, nSize-1, "%s", PCM_HANDLE_IDENTIFIER_PLAY);
    //alogd("generate pcm plugin id:%s", pDmixId);
}

/**
  generate audio capture identifer.

  our alsa capture pcm handle identifer naming conversion:
  pcm.Capture<n>Mic:<SampleRate>, <n>: the number of MIC. <SampleRate>: set sample rate to Hardware PCM.
  pcm.Capture<n>MicPlusAec, if enable aec, add PlusAec.

  e.g.:
  pcm.Capture1Mic:16000
  pcm.Capture1MicPlusAec
  pcm.Capture2Mic:16000
  pcm.Capture2MicPlusAec

  @param pCaptureId
    char array to store capture pcm handle identifier.
  @param nMicNum
    the number of microphone.
  @param bAecEnable
    indicate if enable aec.
  @param nSampleRate
    sample rate to be set to Hardware PCM.
*/
static void GenerateCaptureIdentifier(char *pCaptureId, int nSize, int nMicNum, int bAecEnable, int nSampleRate)
{
    pCaptureId[nSize-1] = '\0';
    if(bAecEnable)
    {
        snprintf(pCaptureId, nSize-1, "Capture%dMicPlusAec", nMicNum);
    }
    else
    {
        snprintf(pCaptureId, nSize-1, "Capture%dMic:%d", nMicNum, nSampleRate);
    }
    //alogd("generate capture pcm plugin id:%s", pCaptureId);
}

// 0-cap; 1-play
ERRORTYPE audioHw_Construct(void)
{
    int i;
    int err;
    //make all controls available.
    MakeAllMixerElemsAvailable();

    //memset(&gAudioHwDev, 0, sizeof(AudioHwDevice)*AIO_DEV_MAX_NUM);
    for (i = 0; i < AIO_DEV_MAX_NUM; ++i) {
        AudioHwDevice *pDev = &gAudioHwDev[i];
        if (TRUE == pDev->mEnableFlag) {
            alogw("audio_hw has already been constructed!");
            return SUCCESS;
        }
        memset(pDev, 0, sizeof(AudioHwDevice));
        if(0 == i)
        {
            err = alsaOpenMixer(&pDev->mMixer, SOUND_MIXER_AUDIOCODEC);
            if (err != 0) {
                aloge("AIO device %d open mixer failed, err[%d]!", i, err);
            }
            pDev->mMixer.snd_card_id = 0;

            pDev->mCap.snd_card_id = 0;
            pDev->mCap.mCfg.snd_card_id = 0;

            for(int j=0; j<MAX_AO_CHLS; ++j)
            {
                pDev->mPlay.PlayChls[j].snd_card_id = 0;
                pDev->mPlay.PlayChls[j].mCfg.snd_card_id = 0;
            }
            //pDev->mPlay.mDmixSampleRate = PlaybackRateDmix_DefaultSampleRate;
            //pDev->mPlay.mDmixChnNum = PlaybackRateDmix_DefaultChnCnt;
            //pDev->mPlay.mDmixPeriodSize = PlaybackRateDmix_DefaultPeriodSize;
//            #if (MPPCFG_AEC == OPTION_AEC_ENABLE)
//                alsaMixerSetAudioCodecHubMode(&pDev->mMixer,1);    // to set hub mode for the first snd card
//            #endif
        }
        else if(1 == i)
        {
            err = alsaOpenMixer(&pDev->mMixer, SOUND_MIXER_SNDDAUDIO0);
            if (err != 0) {
                aloge("AIO device %d open mixer failed!", i);
            }
            pDev->mMixer.snd_card_id = 1;

            pDev->mCap.snd_card_id = 1;
            pDev->mCap.mCfg.snd_card_id = 1;

            for(int j=0; j<MAX_AO_CHLS; ++j)
            {
                pDev->mPlay.PlayChls[j].snd_card_id = 1;
                pDev->mPlay.PlayChls[j].mCfg.snd_card_id = 1;
            }
//            #if (MPPCFG_AEC == OPTION_AEC_ENABLE)
//                alsaMixerSetDAudio0HubMode(&pDev->mMixer,1);// to set hub mode for the second snd card
//                alsaMixerSetDAudio0LoopBackEn(&pDev->mMixer,1);// to enable loopback for the second snd card
//            #endif
        } 
        pDev->mCap.mState = AI_STATE_INVALID;
        for(int j=0; j<MAX_AO_CHLS; ++j)
        {
            pDev->mPlay.PlayChls[j].mState = AO_STATE_INVALID;
        }
        pDev->mPlay.daudio0_ao_chl = -1;
        INIT_LIST_HEAD(&pDev->mCap.mChnList);
        INIT_LIST_HEAD(&pDev->mPlay.mChnList); 
        pthread_mutex_init(&pDev->mCap.mApiCallLock, NULL);
        //pthread_mutex_init(&pDev->mCap.mAgcDbGainLock, NULL);
        pthread_mutex_init(&pDev->mPlay.mAOApiCallLock, NULL);
        pthread_mutex_init(&pDev->mPlay.mChnListLock, NULL);
        //INIT_LIST_HEAD(&pPlay->mChnList);
        pDev->mEnableFlag = TRUE;
    }
    return SUCCESS;
}

ERRORTYPE audioHw_Destruct(void)
{
    int i;
    for (i = 0; i < AIO_DEV_MAX_NUM; ++i) {
        AudioHwDevice *pDev = &gAudioHwDev[i];
        if (FALSE == pDev->mEnableFlag) {
            alogw("audio_hw has already been destructed!");
            return SUCCESS;
        }
        if (pDev->mMixer.handle != NULL) 
        {
            for(int j=0; j<MAX_AO_CHLS; ++j)
            {
                if(AO_STATE_STARTED==pDev->mPlay.PlayChls[j].mState)
                {
                    aloge("Why AO still running? chl[%d],playState:%d", j,pDev->mPlay.PlayChls[j].mState);
                }
            }

            if (AI_STATE_STARTED==pDev->mCap.mState) 
            {
                aloge("Why AI still running? CapState:%d ", pDev->mCap.mState);
            }
            alsaCloseMixer(&pDev->mMixer);
        }
        
        pthread_mutex_destroy(&pDev->mPlay.mChnListLock);
        pthread_mutex_destroy(&pDev->mCap.mApiCallLock);
        //pthread_mutex_destroy(&pDev->mCap.mAgcDbGainLock);
        pthread_mutex_destroy(&pDev->mPlay.mAOApiCallLock);

        pDev->mCap.mState = AI_STATE_INVALID;

        for(int j=0; j<MAX_AO_CHLS; ++j)
        {
            pDev->mPlay.PlayChls[j].mState = AO_STATE_INVALID;
        }

        pDev->mEnableFlag = FALSE;
    }
    return SUCCESS;
}

BOOL audioHw_IsDevEnable(AUDIO_DEV AudioDevId)
{
    return gAudioHwDev[AudioDevId].mEnableFlag;
}

/**************************************AI_DEV*****************************************/
ERRORTYPE audioHw_AI_Dev_lock(AUDIO_DEV AudioDevId)
{
    return pthread_mutex_lock(&gAudioHwDev[AudioDevId].mCap.mChnListLock);
}

ERRORTYPE audioHw_AI_Dev_unlock(AUDIO_DEV AudioDevId)
{
    return pthread_mutex_unlock(&gAudioHwDev[AudioDevId].mCap.mChnListLock);
}

ERRORTYPE audioHw_AI_searchChannel_l(AUDIO_DEV AudioDevId, AI_CHN AiChn, AI_CHANNEL_S** pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    ERRORTYPE ret = FAILURE;
    AI_CHANNEL_S *pEntry;
    list_for_each_entry(pEntry, &pCap->mChnList, mList)
    {
        if(pEntry->mId == AiChn) {
            if(pChn) {
                *pChn = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    return ret;
}

ERRORTYPE audioHw_AI_searchChannel(AUDIO_DEV AudioDevId, AI_CHN AiChn, AI_CHANNEL_S** pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    ERRORTYPE ret = FAILURE;
    AI_CHANNEL_S *pEntry;
    pthread_mutex_lock(&pCap->mChnListLock);
    ret = audioHw_AI_searchChannel_l(AudioDevId, AiChn, pChn);
    pthread_mutex_unlock(&pCap->mChnListLock);
    return ret;
}

ERRORTYPE audioHw_AI_AddChannel_l(AUDIO_DEV AudioDevId, AI_CHANNEL_S* pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    list_add_tail(&pChn->mList, &pCap->mChnList);
    struct list_head* pTmp;
    int cnt = 0;
    list_for_each(pTmp, &pCap->mChnList)
        cnt++;
    updateDebugfsByChnCnt(0, cnt);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_AddChannel(AUDIO_DEV AudioDevId, AI_CHANNEL_S* pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    pthread_mutex_lock(&pCap->mChnListLock);
    ERRORTYPE ret = audioHw_AI_AddChannel_l(AudioDevId, pChn);
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_RemoveChannel(AUDIO_DEV AudioDevId, AI_CHANNEL_S* pChn)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    pthread_mutex_lock(&pCap->mChnListLock);
    list_del(&pChn->mList);
    struct list_head* pTmp;
    int cnt = 0;
    list_for_each(pTmp, &pCap->mChnList)
        cnt++;
    updateDebugfsByChnCnt(0, cnt);
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

MM_COMPONENTTYPE *audioHw_AI_GetChnComp(PARAM_IN MPP_CHN_S *pMppChn)
{
    AI_CHANNEL_S *pChn = NULL;
    if (SUCCESS != audioHw_AI_searchChannel(pMppChn->mDevId, pMppChn->mChnId, &pChn)) {
        return NULL;
    }
    return pChn->mpComp;
}

BOOL audioHw_AI_IsDevStarted(AUDIO_DEV AudioDevId)
{
    return (gAudioHwDev[AudioDevId].mCap.mState == AI_STATE_STARTED);
}

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)

static ERRORTYPE audioHw_AI_AecProcess(void *pThreadData, AUDIO_FRAME_S *pFrm)
{
    AudioInputDevice *pCap = (AudioInputDevice*)pThreadData;
    int ret = 0;

//    if(2 != pCap->mCfg.chnCnt)
//    {
//        aloge("fatal error! aec_invalid_chl_number:%d", pCap->mCfg.chnCnt);
//        return FAILURE;
//    }
    ret = pCap->mpAecProcessCallback(pCap->mpAecCookie, pFrm, pCap->mbSuspendAec);
    if(0 == ret)
    {
        pCap->aec_valid_frm = 1;
    }
    else
    {
        if(ret < 0)
        {
            aloge("fatal error! aec process fail:%d", ret);
        }
        pCap->aec_valid_frm = 0;
    }
    return SUCCESS;
}

#endif

#if (MPPCFG_ANS == OPTION_ANS_ENABLE) 
static ERRORTYPE audioHw_AI_AnsProcess(void *pThreadData, AUDIO_FRAME_S *pFrm)
{
    AudioInputDevice *pCap = (AudioInputDevice*)pThreadData;
    int ret = 0; 

//    if(pCap->mAttr.ai_aec_en)
//    {
//        if(2 != pCap->mCfg.chnCnt)
//        {
//            aloge("fatal error! ans_invalid_chl_number:%d", pCap->mCfg.chnCnt);
//            return FAILURE;
//        }
//    }
//    else
//    {
//        if(1 != pCap->mCfg.chnCnt)
//        {
//            aloge("ans_invalid_chl_number:%d", pCap->mCfg.chnCnt);
//            return FAILURE;
//        }
//    }
    ret = pCap->mpAnsProcessCallback(pCap->mpAnsCookie, pFrm, &pCap->mAttr, pCap->mbSuspendAns);
    if(0 == ret)
    {
        pCap->ans_valid_frm = 1;
    }
    else
    {
        if(ret < 0)
        {
            aloge("fatal error! ans process fail:%d", ret);
        }
        pCap->ans_valid_frm = 0;
    }
    return SUCCESS;
}
#endif

#if (MPPCFG_AGC == OPTION_AGC_ENABLE)
static ERRORTYPE audioHw_AI_AgcProcess(void *pThreadData, AUDIO_FRAME_S *pFrm)
{
    AudioInputDevice *pCap = (AudioInputDevice*)pThreadData;
    int ret = pCap->mpAgcProcessCallback(pCap->mpAgcCookie, pFrm);
    if(0 == ret)
    {
        pCap->agc_valid_frm = 1;
    }
    else
    {
        if(ret < 0)
        {
            aloge("fatal error! agc process fail:%d", ret);
        }
        pCap->agc_valid_frm = 0;
    }
    return SUCCESS;
}
#endif

//static void *audioHw_AI_CapLoopThread(void *pThreadData)
//{
//    AudioInputDevice *pCap = (AudioInputDevice*)pThreadData; 
//    AudioInputDevice *pCap_daudio0 = NULL; 
//    
//    pCap_daudio0 = &gAudioHwDev[pCap->snd_card_id+1].mCap;
//    int nRet = alsaReadPcm(&pCap_daudio0->mCfg, pCap->pRefBuf, pCap_daudio0->mCfg.chunkSize);
//    if (nRet != pCap_daudio0->mCfg.chunkSize)
//    {
//        aloge("fatal error! daudio0 fail to read pcm %d bytes-%d-%d", pCap_daudio0->mCfg.chunkBytes, pCap_daudio0->mState, nRet);
//        if(nRet >= 0)
//        {
//            aloge("fatal error! alsa read pcm[%d] is impossible, check code!", nRet);
//        }
//    }
//    return (void*)nRet;
//}

//static int OverflowAudioInputDevice(AudioInputDevice *pCap, char *pCapBuf)
//{
//    // adc driver use default 1024x8 sample, if sampleRate is 16k, cacheTime is 1024/16*8=512ms
//    int nUnit = 100;    //unit:ms
//    int nInterval;
//    int nRet;
//    int n;
//    for(n=0; n<5; n++)
//    {
//        nInterval = nUnit*(1<<n);
//        usleep(nInterval*1000);
//        alogd("snd_card[%d] wait [%d]ms", pCap->snd_card_id, nInterval);
//        nRet = alsaReadPcm(&pCap->mCfg, pCapBuf, pCap->mCfg.chunkSize);
//        if(nRet < 0)
//        {
//            if(nRet != -EPIPE)
//            {
//                aloge("fatal error! why snd_card[%d] read ret[%d] is not -EPIPE?", pCap->snd_card_id, nRet);
//            }
//            break;
//        }
//        else
//        {
//            alogw("Be careful! snd_card[%d] ret[%d] still not -EPIPE.", pCap->snd_card_id, nRet);
//        }
//    }
//    if(n >= 5)
//    {
//        aloge("fatal error! why snd_card[%d] don't meet -EPIPE?", pCap->snd_card_id);
//        return -1;
//    }
//    else
//    {
//        return 0;
//    }
//}
//#define DEBUG_SAVE_DAUDIO0_PCM (1)
//#define DEBUG_SAVE_RAW_PCM (1)

/**
  judge initial sound mode of audio stream that mpi_ai get from alsa.
  pAiAttr->enSoundmode is the final sound mode after being processed.
*/
AUDIO_SOUND_MODE_E judgeInitialSoundModeOfAudioInput(AIO_ATTR_S *pAiAttr)
{
    AUDIO_SOUND_MODE_E eSoundMode;
    if(1==pAiAttr->mChnCnt && !pAiAttr->ai_aec_en)
    {
        eSoundMode = AUDIO_SOUND_MODE_MONO;
    }
    else if(2==pAiAttr->mChnCnt && !pAiAttr->ai_aec_en)
    {
        eSoundMode = AUDIO_SOUND_MODE_STEREO;
    }
    else if(1==pAiAttr->mChnCnt && pAiAttr->ai_aec_en)
    {
        eSoundMode = AUDIO_SOUND_MODE_AW_1Chn1Ref;
    }
    else if(2==pAiAttr->mChnCnt && pAiAttr->ai_aec_en)
    {
        eSoundMode = AUDIO_SOUND_MODE_AW_2Chn1Ref;
    }
    else
    {
        aloge("fatal error! not support mode:%d-%d", pAiAttr->mChnCnt, pAiAttr->ai_aec_en);
        eSoundMode = AUDIO_SOUND_MODE_MONO;
    }
    return eSoundMode;
}

static void *audioHw_AI_CapThread(void *pThreadData)
{
    AudioInputDevice *pCap = (AudioInputDevice*)pThreadData; 
    AudioInputDevice *pCap_daudio0 = NULL; 
    BOOL mSaveFileFlag = 0;
    char *pCapBuf = NULL; 
    //char *pCapBufLoopBack = NULL; 
    ssize_t ret;
    pCap->pCapBuf = (char*)malloc(pCap->mCfg.chunkBytes);
    if (NULL == pCap->pCapBuf)
    {
        aloge("Failed to alloc %d bytes(%s)", pCap->mCfg.chunkBytes, strerror(errno));
    }
    pCapBuf = pCap->pCapBuf; 

    if(pCap->mAttr.ai_aec_en)
    {
//        pCap->pRefBuf = (char *)malloc(pCap->mCfg.chunkBytes);
//        if(NULL == pCap->pRefBuf)
//        {
//            aloge("fatal_error_to_malloc_ref_frm_buff:%d",pCap->mCfg.chunkBytes);
//        } 
//        pCapBufLoopBack = pCap->pRefBuf; 
        pCap_daudio0 = &gAudioHwDev[pCap->snd_card_id+1].mCap; 
    } 
    prctl(PR_SET_NAME, "audioHw_AI_CapThread", 0, 0, 0);
#ifdef DEBUG_SAVE_DAUDIO0_PCM
    FILE *pdaudio0File = fopen("/mnt/extsd/AiDaudio0.pcm", "wb");
#endif
#ifdef DEBUG_SAVE_RAW_PCM
    FILE *prawFile = fopen("/tmp/AiRaw.pcm", "wb");
    if (prawFile)
    {
        alogd("create file AiRaw.pcm to save pcm file");
        mSaveFileFlag = TRUE;
    }
    else
    {
        aloge("create file AiRaw.pcm failed!");
        mSaveFileFlag = FALSE;
    }
#endif

    while (pCap->mThdRunning) 
    {
        ret = alsaReadPcm(&pCap->mCfg, pCapBuf, pCap->mCfg.chunkSize);
        if (ret != pCap->mCfg.chunkSize)
        {
            if(ret >= 0)
            {
                aloge("aec_en[%d] fatal error! mainChn read pcm impossible[%d-%d-%d], check code!", pCap->mAttr.ai_aec_en, ret, pCap->mCfg.chunkSize, pCap->mState);
            }
            else
            {
                if(-EPIPE == ret)
                {
                    aloge("aec_en[%d] fatal error! mainChn xrun happened:[%lld]us", pCap->mAttr.ai_aec_en, CDX_GetSysTimeUsMonotonic());
                }
                else
                {
                    aloge("aec_en[%d] fatal error! mainChn fail to read pcm[%d-%d-%d]", pCap->mAttr.ai_aec_en, ret, pCap->mCfg.chunkSize, pCap->mState);
                }
            }
            usleep(20*1000);    //to wait daudio0 xrun
            continue;
        }
        
        AI_CHANNEL_S *pEntry = NULL;
        pthread_mutex_lock(&pCap->mChnListLock);
        if (list_empty(&pCap->mChnList)) //if no aiChn exist, discard data and continue.
        {
            pthread_mutex_unlock(&pCap->mChnListLock);
            continue;
        } 
        AUDIO_FRAME_S stFrmCap;
        memset(&stFrmCap,0,sizeof(AUDIO_FRAME_S));
        stFrmCap.mSamplerate = pCap->mAttr.enSamplerate;
        stFrmCap.mBitwidth = pCap->mAttr.enBitwidth;
        stFrmCap.mSoundmode = judgeInitialSoundModeOfAudioInput(&pCap->mAttr);
        stFrmCap.mLen = pCap->mCfg.chunkBytes;
        stFrmCap.mpAddr = pCap->pCapBuf;

#ifdef DEBUG_SAVE_RAW_PCM
        if (mSaveFileFlag)
            fwrite(stFrmCap.mpAddr, 1, stFrmCap.mLen, prawFile);
#endif

        if(pCap->mAttr.ai_aec_en && pCap->mAttr.mbBypassAec)
        {
            //if enable aec and bypase aec, then we can't do ans and agc here, because ans and agc will change
            //capture_pcm, and this will lead to aec effect very bad.
        }
        else
        {
          #if (MPPCFG_AEC == OPTION_AEC_ENABLE)
            if(pCap->mAttr.ai_aec_en)
            {
                audioHw_AI_AecProcess(pCap,&stFrmCap);

                if(0 == pCap->aec_valid_frm)
                {
                    pthread_mutex_unlock(&pCap->mChnListLock);
                    continue;
                }
            }
          #endif

          #if (MPPCFG_ANS == OPTION_ANS_ENABLE)
            if(pCap->mAttr.ai_ans_en)
            {
                audioHw_AI_AnsProcess(pCap,&stFrmCap);

                if(0 == pCap->ans_valid_frm)
                {
                    pthread_mutex_unlock(&pCap->mChnListLock);
                    continue;
                }
            }
          #endif

          #if (MPPCFG_AGC == OPTION_AGC_ENABLE)
            if(pCap->mAttr.ai_agc_en)
            {
                audioHw_AI_AgcProcess(pCap, &stFrmCap);

                if(0 == pCap->agc_valid_frm)
                {
                    pthread_mutex_unlock(&pCap->mChnListLock);
                    continue;
                }
            }
          #endif
        }

        int64_t nPeriodInterval = (int64_t)pCap->mCfg.chunkSize*1000*1000/pCap->mCfg.sampleRate; //unit:us
        int64_t nFramePts = CDX_GetSysTimeUsMonotonic() - nPeriodInterval;
        list_for_each_entry(pEntry, &pCap->mChnList, mList)
        {
            AUDIO_FRAME_S frame;
            memset(&frame, 0, sizeof(frame));
            COMP_BUFFERHEADERTYPE bufferHeader;
            memset(&bufferHeader, 0, sizeof(bufferHeader));
            bufferHeader.nOutputPortIndex = AI_CHN_PORT_INDEX_CAP_IN;
            bufferHeader.pOutputPortPrivate = &frame;
            frame.mLen = stFrmCap.mLen;
            frame.mBitwidth = pCap->mAttr.enBitwidth;
            if(0 == pCap->mAttr.mbBypassAec)
            {
                if(stFrmCap.mSoundmode != pCap->mAttr.enSoundmode)
                {
                    aloge("fatal error! check AI AIO_ATTR_S sound mode:%d!=%d", stFrmCap.mSoundmode, pCap->mAttr.enSoundmode);
                }
            }
            frame.mSoundmode = pCap->mAttr.enSoundmode;
            frame.mSamplerate = pCap->mAttr.enSamplerate;
            frame.mpAddr = pCapBuf;
            frame.mTimeStamp = nFramePts;// Note: the pts timestamp will be used by all ai channels
            pEntry->mpComp->EmptyThisBuffer(pEntry->mpComp, &bufferHeader);
        }
        pthread_mutex_unlock(&pCap->mChnListLock);
    }

    alogd("AI_CapThread exit!");
#ifdef DEBUG_SAVE_DAUDIO0_PCM
    fclose(pdaudio0File);
#endif
#ifdef DEBUG_SAVE_RAW_PCM
    if (mSaveFileFlag)
        fclose(prawFile);
#endif

    if(NULL != pCap->pCapBuf)
    {
        free(pCap->pCapBuf);
        pCap->pCapBuf = NULL;
    }
    if(pCap->mAttr.ai_aec_en)
    {
//        if(NULL != pCap->pRefBuf)
//        {
//            free(pCap->pRefBuf);
//            pCap->pRefBuf = NULL;
//        }
    }
    return NULL;
}

ERRORTYPE audioHw_AI_SetCallback(AUDIO_DEV AudioDevId, void *cookie, AudioDevCallbackFuncType pAudioDevCallback)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    pthread_mutex_lock(&pCap->mApiCallLock);
    pCap->cookie = cookie;
    pCap->mpAudioDevCallback = pAudioDevCallback;
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_SetPubAttr(AUDIO_DEV AudioDevId, const AIO_ATTR_S *pstAttr)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    
    pthread_mutex_lock(&pCap->mApiCallLock);
    if (pstAttr == NULL) {
        aloge("pstAttr is NULL!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_ILLEGAL_PARAM;
    }
    if (AI_STATE_INVALID != pCap->mState) {
        alogw("audioHw AI PublicAttr has been set!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return SUCCESS;
    }
    pCap->mAttr = *pstAttr;
    if(0==pCap->mAttr.mMicNum)
    {
        pCap->mAttr.mMicNum = 1;
    }
    //verify enSoundmode and mChnCnt of pCap->mAttr
    if (0 == pCap->mAttr.ai_aec_en)
    {
        AUDIO_SOUND_MODE_E eSoundMode = AUDIO_SOUND_MODE_MONO;
        if (1 == pCap->mAttr.mChnCnt)
        {
            eSoundMode = AUDIO_SOUND_MODE_MONO;
        }
        else if (2 == pCap->mAttr.mChnCnt)
        {
            eSoundMode = AUDIO_SOUND_MODE_STEREO;
        }
        else
        {
            aloge("fatal error! ai chnCnt[%d] not support!", pCap->mAttr.mChnCnt);
        }

        if (pCap->mAttr.enSoundmode != eSoundMode)
        {
            aloge("fatal error! user set SoundMode[%d] is wrong, we correct it to [%d]", pCap->mAttr.enSoundmode, eSoundMode);
            pCap->mAttr.enSoundmode = eSoundMode;
        }
    }

    if(pCap->mAttr.ai_aec_en)
    {
    #if (MPPCFG_AEC == OPTION_AEC_ENABLE)
        AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;
        AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
        //alsaMixerSetAudioCodecHubMode(pMixer,1);    // to set hub mode for the first snd card
        //alsaMixerSetDAudio0HubMode(pMixer_daudio0,1);// to set hub mode for the second snd card
        //alsaMixerSetDAudio0LoopBackEn(pMixer_daudio0,1);// to enable loopback for the second snd card

        alsaMixerSetCapPlaySyncMode(pMixer, 1);
        alsaMixerSetCapPlaySyncMode(pMixer_daudio0, 1);
    #else
        alogw("Be careful! aec is not config, so force disable.");
        pCap->mAttr.ai_aec_en = 0;
    #endif
    }
    else
    {
    #if (MPPCFG_AEC == OPTION_AEC_ENABLE)
        AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;
        AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
        //alsaMixerSetAudioCodecHubMode(pMixer,1);    // to set hub mode for the first snd card
        //alsaMixerSetDAudio0HubMode(pMixer_daudio0,1);// to set hub mode for the second snd card
        //alsaMixerSetDAudio0LoopBackEn(pMixer_daudio0,1);// to enable loopback for the second snd card

        alsaMixerSetCapPlaySyncMode(pMixer, 0);
        alsaMixerSetCapPlaySyncMode(pMixer_daudio0, 0);
    #endif
    }
    pCap->mState = AI_STATE_CONFIGURED;
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_GetPubAttr(AUDIO_DEV AudioDevId, AIO_ATTR_S *pstAttr)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    
    pthread_mutex_lock(&pCap->mApiCallLock);
    if (pstAttr == NULL) {
        aloge("pstAttr is NULL!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_ILLEGAL_PARAM;
    }

    if (pCap->mState == AI_STATE_INVALID) {
        aloge("get attr when attr is not set!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_NOT_PERM;
    }

    *pstAttr = pCap->mAttr;
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_ClrPubAttr(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    
    pthread_mutex_lock(&pCap->mApiCallLock);
    if (pCap->mState == AI_STATE_STARTED) {
        aloge("please clear attr after AI disable!");
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_NOT_PERM;
    }
    memset(&pCap->mAttr, 0, sizeof(AIO_ATTR_S));
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;
    AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
    alsaMixerSetCapPlaySyncMode(pMixer, 0);
    alsaMixerSetCapPlaySyncMode(pMixer_daudio0, 0);
#endif
    pCap->mState = AI_STATE_INVALID;
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_Enable(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer; 
    AudioInputDevice *pCap_daudio0 = NULL;
    AIO_MIXER_S *pMixer_daudio0 = NULL;
    if(pCap->mAttr.ai_aec_en)
    {
        pCap_daudio0 = &gAudioHwDev[AudioDevId+1].mCap;
        pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
    }

    int ret;
    
    pthread_mutex_lock(&pCap->mApiCallLock);

    if (pCap->mState == AI_STATE_INVALID) {
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_NOT_CONFIG;
    }
    if (pCap->mState == AI_STATE_STARTED) {
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return SUCCESS;
    }
    // if 'MIC2 Switch' enable but not disable, then next time when user want to use single mic, two mic's stream will
    // be merge to one channel, it will make app error. So we depend on asound.conf hooks to enable
    // other MICs. Only MIC1 will be enable in alsaOpenMixer().
//    for(int i=0; i<pCap->mAttr.mMicNum; i++)
//    {
//        alsaMixerSetMicXEnable(pMixer, i+1, 1);
//    }
    pCap->mCfg.chnCnt = pCap->mAttr.mChnCnt;
    pCap->mCfg.sampleRate = pCap->mAttr.enSamplerate;

    pCap->mCfg.aec_delay_ms = pCap->mAttr.aec_delay_ms;
    
    if (pCap->mAttr.enBitwidth == AUDIO_BIT_WIDTH_32) {
        pCap->mCfg.format = SND_PCM_FORMAT_S32_LE;
    } else if (pCap->mAttr.enBitwidth == AUDIO_BIT_WIDTH_24) {
        pCap->mCfg.format = SND_PCM_FORMAT_S24_LE;
    } else if (pCap->mAttr.enBitwidth == AUDIO_BIT_WIDTH_16) {
        pCap->mCfg.format = SND_PCM_FORMAT_S16_LE;
    } else if (pCap->mAttr.enBitwidth == AUDIO_BIT_WIDTH_8) {
        pCap->mCfg.format = SND_PCM_FORMAT_S8;
    } else {
        pCap->mCfg.format = SND_PCM_FORMAT_S16_LE;
    }
    pCap->mCfg.bitsPerSample = (pCap->mAttr.enBitwidth+1)*8;
    pCap->mCfg.chunkSize = pCap->mAttr.mPtNumPerFrm;
    if (0 == pCap->mCfg.chunkSize)
    {
        pCap->mCfg.chunkSize = AICapAlsaFrameNum;
    }
    if ((pCap->mCfg.chunkSize != 160) && (pCap->mCfg.chunkSize != 320) && (pCap->mCfg.chunkSize != AICapAlsaFrameNum)
        && (pCap->mCfg.chunkSize != 1024))
    {
        aloge("fatal error! audioDev[%d] capture: not support periodSize[%d], use default[%d]", AudioDevId, pCap->mCfg.chunkSize,
            AICapAlsaFrameNum);
        pCap->mCfg.chunkSize = AICapAlsaFrameNum;
    }

    if(pCap->mAttr.ai_aec_en)
    {
        memcpy(&pCap_daudio0->mAttr,&pCap->mAttr,sizeof(AIO_ATTR_S));
        int tmpCardId = pCap_daudio0->mCfg.snd_card_id;
        memcpy(&pCap_daudio0->mCfg,&pCap->mCfg,sizeof(PCM_CONFIG_S));
        pCap_daudio0->mCfg.snd_card_id = tmpCardId;
    }
//    if(pCap->mAttr.ai_aec_en)
//    {
//        alsaMixerSetCapPlaySyncMode(pMixer,1); 
//    }
    char strCaptureIdentifier[128]; // e.g. pcm.Capture1MicPlusAec
    GenerateCaptureIdentifier(strCaptureIdentifier, sizeof(strCaptureIdentifier), pCap->mAttr.mMicNum, pCap->mAttr.ai_aec_en, pCap->mCfg.sampleRate);
    if(pCap->mAttr.ai_aec_en)
    {
        pCap->mCfg.chnCnt += 1; //daudio0 data will be capture by multi-plugin, so channel count plus 1.
        ret = alsaOpenPcm(&pCap->mCfg, strCaptureIdentifier, 0);
    }
    else
    {
        ret = alsaOpenPcm(&pCap->mCfg, strCaptureIdentifier, 0);
    }
    if (ret != 0)
    {
        aloge("fatal error! open_pcm failed");
        //alsaMixerSetMic2Enable(pMixer,0); // disable  mic2
        //alsaMixerSetCapPlaySyncMode(pMixer,0); 
        pthread_mutex_unlock(&pCap->mApiCallLock); 
        return FAILURE;
    }

    ret = alsaSetPcmParams(&pCap->mCfg);
    if (ret < 0)
    {
        aloge("fatal error! SetPcmParams failed");
        goto ERR_SET_PCM_PARAM;
    }
    alsaPreparePcm(&pCap->mCfg);
    //update mAttr by mCfg
    pCap->mAttr.mPtNumPerFrm = pCap->mCfg.chunkSize;

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    pCap->mCfg.read_pcm_aec = 0;
    if(pCap->mAttr.ai_aec_en)
    {
        pCap->mCfg.read_pcm_aec = 1;
        pCap_daudio0->mCfg.read_pcm_aec = 1;

        pCap->aec_valid_frm = 0;

      #if (MPPCFG_AEC_LIB == OPTION_AEC_LIBRARY_WEBRTC)
        pCap->mpAecCookie = ConstructWebRtcAecContext(&pCap->mAttr, pCap->mCfg.chunkBytes);
        pCap->mpAecProcessCallback = &WebRtcAecProcess;
      #elif (MPPCFG_AEC_LIB == OPTION_AEC_LIBRARY_UVOICE)
        pCap->mpAecCookie = ConstructUvoiceAecContext(&pCap->mAttr, pCap->mCfg.chunkBytes, pCap->mpAudioDevCallback, pCap->cookie, AudioDevId);
        pCap->mpAecProcessCallback = &UvoiceAecProcess;
      #else
        aloge("fatal error! no aec lib!");
        pCap->mpAecCookie = NULL;
        pCap->mpAecProcessCallback = NULL;
      #endif
    }
#endif

#if (MPPCFG_ANS == OPTION_ANS_ENABLE)
    if(pCap->mAttr.ai_ans_en)
    {
        pCap->ans_valid_frm = 0;

      #if (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_WEBRTC)
        pCap->mpAnsCookie = ConstructWebRtcAnsContext();
        pCap->mpAnsProcessCallback = &WebRtcAnsProcess;
      #elif(MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_LSTM)
        pCap->mpAnsCookie = ConstructLstmAnsContext();
        pCap->mpAnsProcessCallback = &LstmAnsProcess;
      #elif(MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_NOSC)
        pCap->mpAnsCookie = ConstructNoscAnsContext();
        pCap->mpAnsProcessCallback = &NoscAnsProcess;
      #else
        aloge("fatal error! no ans lib!");
        pCap->mpAnsCookie = NULL;
        pCap->mpAnsProcessCallback = NULL;
      #endif
    }
#endif

    /*if(pCap->mAttr.ai_agc_en)
    {
        pCap->ai_agc_tmp_buff = (short *)malloc(pCap->mCfg.chunkBytes);
        if(NULL == pCap->ai_agc_tmp_buff)
        {
            aloge("agc_in_ai_error_malloc_tmp_buff_failed:%d",pCap->mCfg.chunkBytes);
            goto ERR_EXIT5;
        }
        pCap->ai_agc_inited = 0;
    }*/

#if (MPPCFG_AGC == OPTION_AGC_ENABLE)
    if(pCap->mAttr.ai_agc_en)
    {
        pCap->agc_valid_frm = 0;

        int nSampleRate = (int)map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pCap->mAttr.enSamplerate);
        int nChnNum = judgeAudioChnNumBySoundMode(pCap->mAttr.enSoundmode, NULL, NULL);
        int nFrameLen = pCap->mCfg.chunkBytes;
      #if (MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_WEBRTC)
        pCap->mpAgcCookie = ConstructWebRtcAgcContext(nSampleRate, nChnNum, nFrameLen, &pCap->mAttr.ai_agc_float_cfg);
        pCap->mpAgcProcessCallback = &WebRtcAgcProcess;
      #elif(MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_AGCFLOAT)
        pCap->mpAgcCookie = ConstructFloatAgcContext(nSampleRate, nChnNum, nFrameLen, &pCap->mAttr.ai_agc_float_cfg);
        pCap->mpAgcProcessCallback = &FloatAgcProcess;
      #else
        aloge("fatal error! no agc lib!");
        pCap->mpAgcCookie = NULL;
        pCap->mpAgcProcessCallback = NULL;
      #endif
    }
#endif

    pthread_mutex_init(&pCap->mChnListLock, NULL);
    //INIT_LIST_HEAD(&pCap->mChnList);
    pCap->mThdRunning = TRUE;
    pthread_create(&pCap->mThdId, NULL, audioHw_AI_CapThread, pCap);

    pCap->mState = AI_STATE_STARTED;
    
    pthread_mutex_unlock(&pCap->mApiCallLock);

    return SUCCESS;
ERR_EXIT5:
#if (MPPCFG_ANS == OPTION_ANS_ENABLE)
    if(pCap->mpAnsCookie)
    {
      #if (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_WEBRTC)
        DestructWebRtcAnsContext((WebRtcAnsContext*)pCap->mpAnsCookie);
      #elif(MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_LSTM)
        DestructLstmAnsContext((LstmAnsContext*)pCap->mpAnsCookie);
      #elif(MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_NOSC)
        DestructNoscAnsContext((NoscAnsContext*)pCap->mpAnsCookie);
      #endif
        pCap->mpAnsCookie = NULL;
    }
#endif
    
ERR_EXIT2: 
ERR_EXIT1: 
ERR_EXIT0:
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    if(pCap->mpAecCookie)
    {
      #if (MPPCFG_AEC_LIB == OPTION_AEC_LIBRARY_WEBRTC)
        DestructWebRtcAecContext((WebRtcAecContext*)pCap->mpAecCookie);
      #endif
        pCap->mpAecCookie = NULL;
    }
#endif
ERR_SET_PCM_PARAM2: 
    if(pCap->mAttr.ai_aec_en)
    {
        //alsaClosePcm(&pCap_daudio0->mCfg, 0);   // 0: cap
        //alsaMixerSetCapPlaySyncMode(pMixer_daudio0,1); 
    }
    
ERR_SET_PCM_PARAM:
    //alsaMixerSetMic2Enable(pMixer,0); // disable  mic2
    alsaClosePcm(&pCap->mCfg, 0);   // 0: cap
    //alsaMixerSetCapPlaySyncMode(pMixer,0); 
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return FAILURE;
}

ERRORTYPE audioHw_AI_Disable(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer; 
    AudioInputDevice *pCap_daudio0 = NULL;
    AIO_MIXER_S *pMixer_daudio0 = NULL;
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    pCap_daudio0 = &gAudioHwDev[AudioDevId+1].mCap;
    pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
#endif

    int ret;
    
    pthread_mutex_lock(&pCap->mApiCallLock);
    if (pCap->mState == AI_STATE_INVALID)
    {
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return ERR_AI_NOT_CONFIG;
    }
    if (pCap->mState != AI_STATE_STARTED)
    {
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return SUCCESS;
    }
    pthread_mutex_lock(&pCap->mChnListLock);
    if (!list_empty(&pCap->mChnList))
    {
        pthread_mutex_unlock(&pCap->mChnListLock);
        pthread_mutex_unlock(&pCap->mApiCallLock);
        return SUCCESS;
    }
    pthread_mutex_unlock(&pCap->mChnListLock);

    pCap->mThdRunning = FALSE;

    pthread_join(pCap->mThdId, (void*) &ret);

    pthread_mutex_destroy(&pCap->mChnListLock);
    
    alsaClosePcm(&pCap->mCfg, 0);   // 0: cap
    pCap->mCfg.read_pcm_aec = 0;

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    if(pCap->mAttr.ai_aec_en)
    {
        //alsaMixerSetCapPlaySyncMode(pMixer,0);
        //alsaClosePcm(&pCap_daudio0->mCfg, 0);   // 0: cap
        pCap_daudio0->mCfg.read_pcm_aec = 0;

        if(pCap->mpAecCookie)
        {
          #if (MPPCFG_AEC_LIB == OPTION_AEC_LIBRARY_WEBRTC)
            DestructWebRtcAecContext((WebRtcAecContext*)pCap->mpAecCookie);
          #elif (MPPCFG_AEC_LIB == OPTION_AEC_LIBRARY_UVOICE)
            DestructUvoiceAecContext((UvoiceAecContext*)pCap->mpAecCookie);
          #endif
            pCap->mpAecCookie = NULL;
        }
    }
#endif

#if (MPPCFG_ANS == OPTION_ANS_ENABLE)
    if(pCap->mAttr.ai_ans_en)
    {
        if(pCap->mpAnsCookie)
        {
          #if (MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_WEBRTC)
            DestructWebRtcAnsContext((WebRtcAnsContext*)pCap->mpAnsCookie);
          #elif(MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_LSTM)
            DestructLstmAnsContext((LstmAnsContext*)pCap->mpAnsCookie);
          #elif(MPPCFG_ANS_LIB == OPTION_ANS_LIBRARY_NOSC)
            DestructNoscAnsContext((NoscAnsContext*)pCap->mpAnsCookie);
          #endif
            pCap->mpAnsCookie = NULL;
        }
    }
#endif

#if (MPPCFG_AGC == OPTION_AGC_ENABLE)
    if(pCap->mAttr.ai_agc_en)
    {
        if(pCap->mpAgcCookie)
        {
          #if (MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_WEBRTC)
            DestructWebRtcAgcContext((WebRtcAgcContext*)pCap->mpAgcCookie);
          #elif(MPPCFG_AGC_LIB == OPTION_AGC_LIBRARY_AGCFLOAT)
            DestructFloatAgcContext((FloatAgcContext*)pCap->mpAgcCookie);
          #endif
            pCap->mpAgcCookie = NULL;
        }
    }
#endif

    pCap->mState = AI_STATE_CONFIGURED;
    pthread_mutex_unlock(&pCap->mApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_SetTrackMode(AUDIO_DEV AudioDevId, AUDIO_TRACK_MODE_E enTrackMode)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    pCap->mTrackMode = enTrackMode;

    return SUCCESS;
}

ERRORTYPE audioHw_AI_GetTrackMode(AUDIO_DEV AudioDevId, AUDIO_TRACK_MODE_E *penTrackMode)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    *penTrackMode = pCap->mTrackMode;

    return SUCCESS;
}

ERRORTYPE audioHw_AI_GetPcmConfig(AUDIO_DEV AudioDevId, PCM_CONFIG_S **ppCfg)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }
    *ppCfg = &pCap->mCfg;
    return SUCCESS;
}

ERRORTYPE audioHw_AI_GetAIOAttr(AUDIO_DEV AudioDevId, AIO_ATTR_S **ppAttr)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }
    *ppAttr = &pCap->mAttr;
    return SUCCESS;
}

ERRORTYPE audioHw_AI_SetAdcDrc(AUDIO_DEV AudioDevId, int enable)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerSetAudioCodecAdcDrc(pMixer, enable);
}

ERRORTYPE audioHw_AI_SetAdcHpf(AUDIO_DEV AudioDevId, int enable) 
{ 
	AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap; AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerSetAudioCodecAdcHpf(pMixer, enable);
}

ERRORTYPE audioHw_AI_SetVolume(AUDIO_DEV AudioDevId, int s32VolumeDb)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerSetVolume(pMixer, 0, s32VolumeDb);
}

ERRORTYPE audioHw_AI_GetVolume(AUDIO_DEV AudioDevId, int *ps32VolumeDb)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerGetVolume(pMixer, 0, (long*)ps32VolumeDb);
}

ERRORTYPE audioHw_AI_SetMute(AUDIO_DEV AudioDevId, int bEnable)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerSetMute(pMixer, 0, bEnable);
}

ERRORTYPE audioHw_AI_GetMute(AUDIO_DEV AudioDevId, int *pbEnable)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    return alsaMixerGetMute(pMixer, 0, pbEnable);
}

/**************************************AO_DEV*****************************************/
ERRORTYPE audioHw_AO_Dev_lock(AUDIO_DEV AudioDevId)
{
    return pthread_mutex_lock(&gAudioHwDev[AudioDevId].mPlay.mChnListLock);
}

ERRORTYPE audioHw_AO_Dev_unlock(AUDIO_DEV AudioDevId)
{
    return pthread_mutex_unlock(&gAudioHwDev[AudioDevId].mPlay.mChnListLock);
}

ERRORTYPE audioHw_AO_searchChannel_l(AUDIO_DEV AudioDevId, AO_CHN AoChn, AO_CHANNEL_S** pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    ERRORTYPE ret = FAILURE;
    AO_CHANNEL_S *pEntry;
    list_for_each_entry(pEntry, &pPlay->mChnList, mList)
    {
        if(pEntry->mId == AoChn) {
            if(pChn) {
                *pChn = pEntry;
            }
            ret = SUCCESS;
            break;
        }
    }
    return ret;
}

ERRORTYPE audioHw_AO_searchChannel(AUDIO_DEV AudioDevId, AO_CHN AoChn, AO_CHANNEL_S** pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    ERRORTYPE ret = FAILURE;
    AO_CHANNEL_S *pEntry;
    pthread_mutex_lock(&pPlay->mChnListLock);
    ret = audioHw_AO_searchChannel_l(AudioDevId, AoChn, pChn);
    pthread_mutex_unlock(&pPlay->mChnListLock);
    return ret;
}

ERRORTYPE audioHw_AO_AddChannel_l(AUDIO_DEV AudioDevId, AO_CHANNEL_S* pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    list_add_tail(&pChn->mList, &pPlay->mChnList);
    struct list_head* pTmp;
    int cnt = 0;
    list_for_each(pTmp, &pPlay->mChnList)
        cnt++;
    updateDebugfsByChnCnt(1, cnt);
    return SUCCESS;
}

ERRORTYPE audioHw_AO_AddChannel(AUDIO_DEV AudioDevId, AO_CHANNEL_S* pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    pthread_mutex_lock(&pPlay->mChnListLock);
    ERRORTYPE ret = audioHw_AO_AddChannel_l(AudioDevId, pChn);
    pthread_mutex_unlock(&pPlay->mChnListLock);
    return ret;
}

ERRORTYPE audioHw_AO_RemoveChannel(AUDIO_DEV AudioDevId, AO_CHANNEL_S* pChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    pthread_mutex_lock(&pPlay->mChnListLock);
    list_del(&pChn->mList);
    struct list_head* pTmp;
    int cnt = 0;
    list_for_each(pTmp, &pPlay->mChnList)
        cnt++;
    updateDebugfsByChnCnt(1, cnt);
    pthread_mutex_unlock(&pPlay->mChnListLock);
    return SUCCESS;
}

MM_COMPONENTTYPE *audioHw_AO_GetChnComp(PARAM_IN MPP_CHN_S *pMppChn)
{
    AO_CHANNEL_S *pChn = NULL;
    if (SUCCESS != audioHw_AO_searchChannel(pMppChn->mDevId, pMppChn->mChnId, &pChn)) {
        return NULL;
    }
    return pChn->mpComp;
}

/**
  set attr of underlying ao output alsa-plugin, i.e. pcm.PlaybackRateDmix of asound.conf.

  @param AudioDevId
    audioDevice
  @param pstAttr
    use pstAttr->enSamplerate, pstAttr->mPtNumPerFrm(i.e. period_size), pstAttr->mChnCnt now.
*/
//ERRORTYPE AudioHw_AO_SetPubAttr(AUDIO_DEV AudioDevId, const AIO_ATTR_S *pstAttr)
//{
//    if (pstAttr == NULL)
//    {
//        aloge("pstAttr is NULL!");
//        return ERR_AO_ILLEGAL_PARAM;
//    }
//    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
//    pthread_mutex_lock(&pPlay->mAOApiCallLock);
//    pPlay->mDmixSampleRate = pstAttr->enSamplerate;
//    pPlay->mDmixChnNum = pstAttr->mChnCnt;
//    pPlay->mDmixPeriodSize = pstAttr->mPtNumPerFrm;
//    pthread_mutex_unlock(&pPlay->mAOApiCallLock);
//    return SUCCESS;
//}
/**
  get attr of underlying ao output alsa-plugin, i.e. pcm.PlaybackRateDmix of asound.conf.

  @param AudioDevId
    audioDevice
  @param pstAttr
    use pstAttr->enSamplerate, pstAttr->mPtNumPerFrm(i.e. period_size), pstAttr->mChnCnt now.
*/
//ERRORTYPE AudioHw_AO_GetPubAttr(AUDIO_DEV AudioDevId, AIO_ATTR_S *pstAttr)
//{
//    if (pstAttr == NULL)
//    {
//        aloge("pstAttr is NULL!");
//        return ERR_AO_ILLEGAL_PARAM;
//    }
//    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
//    pstAttr->enSamplerate = pPlay->mDmixSampleRate;
//    pstAttr->mChnCnt = pPlay->mDmixChnNum;
//    pstAttr->mPtNumPerFrm = pPlay->mDmixPeriodSize;
//    return SUCCESS;
//}
/**
  restore attr of underlying ao output alsa-plugin to default value.
*/
//ERRORTYPE audioHw_AO_ClrPubAttr(AUDIO_DEV AudioDevId)
//{
//    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
//    pthread_mutex_lock(&pPlay->mAOApiCallLock);
//    pPlay->mDmixSampleRate = PlaybackRateDmix_DefaultSampleRate;
//    pPlay->mDmixChnNum = PlaybackRateDmix_DefaultChnCnt;
//    pPlay->mDmixPeriodSize = PlaybackRateDmix_DefaultPeriodSize;
//    pthread_mutex_unlock(&pPlay->mAOApiCallLock);
//    return SUCCESS;
//}


BOOL audioHw_AO_IsDevConfigured(AUDIO_DEV AudioDevId,AO_CHN AoChn)  // specify SndCard and ao chn id
{
    pthread_mutex_lock(&gAudioHwDev[AudioDevId].mPlay.mAOApiCallLock);
    BOOL bFlag = (gAudioHwDev[AudioDevId].mPlay.PlayChls[AoChn].mState == AO_STATE_CONFIGURED);
    pthread_mutex_unlock(&gAudioHwDev[AudioDevId].mPlay.mAOApiCallLock);
    return bFlag;
}

BOOL audioHw_AO_IsDevStarted(AUDIO_DEV AudioDevId, AO_CHN AoChn)
{
    pthread_mutex_lock(&gAudioHwDev[AudioDevId].mPlay.mAOApiCallLock);
    BOOL bFlag = (gAudioHwDev[AudioDevId].mPlay.PlayChls[AoChn].mState == AO_STATE_STARTED);
    pthread_mutex_unlock(&gAudioHwDev[AudioDevId].mPlay.mAOApiCallLock);
    return bFlag;
}
/* set attribute of pcm data from given ao chn */
ERRORTYPE audioHw_AO_SetChnPubAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn, const AIO_ATTR_S *pstAttr)
{
    if (pstAttr == NULL) {
        aloge("pstAttr is NULL!");
        return ERR_AO_ILLEGAL_PARAM;
    }
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    pthread_mutex_lock(&pPlay->mAOApiCallLock);
	aloge("ao_set_attr_chl:%d,stat:%d",AoChn,pPlay->PlayChls[AoChn].mState);

    if (pPlay->PlayChls[AoChn].mState == AO_STATE_CONFIGURED) {
        alogw("Update AoAttr? cur_card:%d -> wanted_card:%d-%d", 
				pPlay->PlayChls[AoChn].mAttr.mPcmCardId, pstAttr->mPcmCardId,AoChn);
    } else if (pPlay->PlayChls[AoChn].mState == AO_STATE_STARTED) {
        alogw("Careful for 2 AoChns at the same time! They must have the same param:%d!",AoChn);
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return SUCCESS;
    } 
    pPlay->PlayChls[AoChn].mAttr = *pstAttr;

/* #if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;
    AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
    alsaMixerSetAudioCodecHubMode(pMixer,1);    // to set hub mode for the first snd card
    alsaMixerSetDAudio0HubMode(pMixer_daudio0,1);// to set hub mode for the second snd card
    alsaMixerSetDAudio0LoopBackEn(pMixer_daudio0,1);// to enable loopback for the second snd card
#endif */
    pPlay->PlayChls[AoChn].mState = AO_STATE_CONFIGURED;
    pthread_mutex_unlock(&pPlay->mAOApiCallLock);
    return SUCCESS;
}
/* get attribute of pcm that come from given ao chn */
ERRORTYPE audioHw_AO_GetChnPubAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn, AIO_ATTR_S *pstAttr)
{
    if (pstAttr == NULL) {
        aloge("pstAttr is NULL!");
        return ERR_AO_ILLEGAL_PARAM;
    }

    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    if (pPlay->PlayChls[AoChn].mState == AO_STATE_INVALID) {
        aloge("get attr when attr is not set!");
        return ERR_AO_NOT_PERM;
    }

    *pstAttr = pPlay->PlayChls[AoChn].mAttr;
    return SUCCESS;
}
/* clear the attribute for specific ao chn */
ERRORTYPE audioHw_AO_ClrChnPubAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    pthread_mutex_lock(&pPlay->mAOApiCallLock);
    if (pPlay->PlayChls[AoChn].mState == AO_STATE_STARTED) {
        aloge("please clear attr after AI disable!");
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return ERR_AO_NOT_PERM;
    }
    memset(&pPlay->PlayChls[AoChn].mAttr, 0, sizeof(AIO_ATTR_S));
    pPlay->PlayChls[AoChn].mState = AO_STATE_INVALID;
    pthread_mutex_unlock(&pPlay->mAOApiCallLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AO_SetTrackMode(AUDIO_DEV AudioDevId, AO_CHN AoChn,AUDIO_TRACK_MODE_E enTrackMode)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }

    pPlay->PlayChls[AoChn].mTrackMode = enTrackMode;

    return SUCCESS;
}

ERRORTYPE audioHw_AO_GetTrackMode(AUDIO_DEV AudioDevId, AO_CHN AoChn,AUDIO_TRACK_MODE_E *penTrackMode)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }

    *penTrackMode = pPlay->PlayChls[AoChn].mTrackMode;

    return SUCCESS;
}

ERRORTYPE audioHw_AO_EnableChn(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay; 
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer; 
	int j = 0; 

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    AudioOutputDevice *pPlay_daudio0 = &gAudioHwDev[AudioDevId+1].mPlay; // related with second snd card
    AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;
#endif

    int ret;

    aloge("ao_enable_chl:%d-%d", AudioDevId, AoChn);

    pthread_mutex_lock(&pPlay->mAOApiCallLock);
    if (pPlay->PlayChls[AoChn].mState == AO_STATE_INVALID) {
        aloge("fatal error! error_state:%d", pPlay->PlayChls[AoChn].mState);
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return ERR_AO_NOT_CONFIG;
    }
    if (pPlay->PlayChls[AoChn].mState == AO_STATE_STARTED) {
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return SUCCESS;
    }
    PCM_CONFIG_S *p_cfg = &pPlay->PlayChls[AoChn].mCfg;
    AIO_ATTR_S *p_attr = &pPlay->PlayChls[AoChn].mAttr;

    p_cfg->chnCnt = p_attr->mChnCnt;
    p_cfg->sampleRate = p_attr->enSamplerate;
    p_cfg->chunkSize = p_cfg->sampleRate*AOPlayPeriodTime/1000;
    p_cfg->nStartThresholdSize = p_attr->nAlsaFrameNumberForStartThreshold;
    switch (p_attr->enBitwidth)
    {
        case AUDIO_BIT_WIDTH_32:
            p_cfg->format = SND_PCM_FORMAT_S32_LE;
            break;
        case AUDIO_BIT_WIDTH_24:
            p_cfg->format = SND_PCM_FORMAT_S24_LE;
            break;
        case AUDIO_BIT_WIDTH_16:
            p_cfg->format = SND_PCM_FORMAT_S16_LE;
            break;
        case AUDIO_BIT_WIDTH_8:
            p_cfg->format = SND_PCM_FORMAT_S8;
            break;
        default :
            p_cfg->format = SND_PCM_FORMAT_S16_LE;
            break;
    }

    p_cfg->bitsPerSample = (p_attr->enBitwidth+1)*8;

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    if(-1 == pPlay_daudio0->daudio0_ao_chl)
    {
        memcpy(&pPlay_daudio0->PlayChls[AoChn].mAttr,&pPlay->PlayChls[AoChn].mAttr,sizeof(AIO_ATTR_S));
        int tmpCardId = pPlay_daudio0->PlayChls[AoChn].mCfg.snd_card_id;
        memcpy(&pPlay_daudio0->PlayChls[AoChn].mCfg,&pPlay->PlayChls[AoChn].mCfg,sizeof(PCM_CONFIG_S)); 
        pPlay_daudio0->PlayChls[AoChn].mCfg.snd_card_id = tmpCardId;
        alogw("params of play_daudio0 chn[%d] [%d-%d-%d] don't matter now. asound.conf make sure same to PlaybackRateDmix",
            AoChn, pPlay_daudio0->PlayChls[AoChn].mCfg.sampleRate, pPlay_daudio0->PlayChls[AoChn].mCfg.chnCnt,
            pPlay_daudio0->PlayChls[AoChn].mCfg.chunkSize);
        //alogw("params of play_daudio0 chn[%d] must follow PlaybackRateDmix in asound.conf! [%d-%d-%d]", AoChn, pPlay->mDmixSampleRate, pPlay->mDmixChnNum, pPlay->mDmixPeriodSize);
//        pPlay_daudio0->PlayChls[AoChn].mAttr.enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(pPlay->mDmixSampleRate);
//        pPlay_daudio0->PlayChls[AoChn].mAttr.mChnCnt = pPlay->mDmixChnNum;
//        pPlay_daudio0->PlayChls[AoChn].mCfg.sampleRate = pPlay->mDmixSampleRate;
//        pPlay_daudio0->PlayChls[AoChn].mCfg.chnCnt = pPlay->mDmixChnNum;
    }
    else
    {
        alogd("daudio0 chn[%d] already exists, keep its value! Ignore aoChn[%d] param!", pPlay_daudio0->daudio0_ao_chl, AoChn);
    }
    for(j=0; j<MAX_AO_CHLS; ++j)
    {
        if(pPlay->PlayChls[j].mState == AO_STATE_STARTED)
        {
            break;
        }
    }
//    if(j == MAX_AO_CHLS)    // only need to set AudioCodecHubMode when the AoChn is the first one
//    {
//        alsaMixerSetAudioCodecHubMode(pMixer,1);    // to set hub mode for the first snd card
//    }
//    else
//    {
//        alogd("Be careful! aoChn[%d] is started, not set AudioCodecHubMode to 1", j);
//    }
#endif
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    if(j == MAX_AO_CHLS)    // only need to set daudio0 when the AoChn is the first one
    {
        //alsaMixerSetDAudio0HubMode(pMixer_daudio0,1);// to set hub mode for the second snd card
        //alsaMixerSetDAudio0LoopBackEn(pMixer_daudio0,1);// to enable loopback for the second snd card
        ret = alsaOpenPcm(&pPlay_daudio0->PlayChls[AoChn].mCfg, SOUND_CARD_SNDDAUDIo0, 1);  // to open the second snd card
        if (ret != 0)
        {
            aloge("fatal error! daudio0 open_pcm failed, ch:%d", AoChn);
            pthread_mutex_unlock(&pPlay->mAOApiCallLock);
            return FAILURE;
        }
        pPlay_daudio0->daudio0_ao_chl = AoChn;      // record the ao chn index to set daudio0
        // "hw:snddaudio0,0" play params must accord to audiocodec playback. alsa-plugins don't take effect in "hw:snddaudio0,0"
        // play, but take effect in "hw:snddaudio0,0" capture. So when use plugin I2SRTX, play params don't matter.
        ret = alsaSetPcmParams(&pPlay_daudio0->PlayChls[AoChn].mCfg);   // to set params for the second snd card
        if (ret < 0)
        {
            aloge("fatal error! daudio0 SetPcmParams failed, ch:%d", AoChn);
            goto ERR_SET_PCM_PARAM2;
        }
        alsaPreparePcm(&pPlay_daudio0->PlayChls[AoChn].mCfg);           // and call the prepare api for the second snd card
        alogd("daudio0 chn:%d prepare pcm done.", pPlay_daudio0->daudio0_ao_chl);
    }
#endif
    //const char *pCardType = (pPlay->PlayChls[AoChn].mAttr.mPcmCardId==PCM_CARD_TYPE_AUDIOCODEC) ? SOUND_CARD_AUDIOCODEC_PLAY:SOUND_CARD_SNDHDMI;
    char *pCardType = NULL;
    char strDmixIdentifier[128]; // e.g., PlaybackRateDmix:16000,1,960
    if(PCM_CARD_TYPE_AUDIOCODEC == pPlay->PlayChls[AoChn].mAttr.mPcmCardId)
    {
        GeneratePlaybackRateDmixIdentifier(strDmixIdentifier, sizeof(strDmixIdentifier), 0, 0, 0);//pPlay->mDmixSampleRate, pPlay->mDmixChnNum, pPlay->mDmixPeriodSize
        pCardType = strDmixIdentifier;
    }
    else if (PCM_CARD_TYPE_SNDHDMI == pPlay->PlayChls[AoChn].mAttr.mPcmCardId)
    {
        pCardType = SOUND_CARD_SNDHDMI;
    }
    else if (PCM_CARD_TYPE_UAC1 == pPlay->PlayChls[AoChn].mAttr.mPcmCardId)
    {
        pCardType = SOUND_CARD_UAC1;
    }
    ret = alsaOpenPcm(&pPlay->PlayChls[AoChn].mCfg, pCardType, 1);
    if (ret != 0)
    {
        aloge("fatal error! aoChn[%d-%d] open_pcm failed", AudioDevId, AoChn);
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return FAILURE;
    }
    ret = alsaSetPcmParams(&pPlay->PlayChls[AoChn].mCfg);
    if (ret < 0)
    {
        aloge("fatal error! aoChn[%d-%d] SetPcmParams failed", AudioDevId, AoChn);
        goto ERR_SET_PCM_PARAM;
    }
    alsaPreparePcm(&pPlay->PlayChls[AoChn].mCfg);           // and call the prepare api for the first snd card

    //pPlay->mThdRunning = TRUE;
    //pthread_create(&pPlay->mThdId, NULL, audioHw_AO_PlayThread, &pPlay);

    pPlay->PlayChls[AoChn].mState = AO_STATE_STARTED;
    pthread_mutex_unlock(&pPlay->mAOApiCallLock);
    return SUCCESS;
    
ERR_SET_PCM_PARAM:
    alsaClosePcm(&pPlay->PlayChls[AoChn].mCfg, 1);  // 1: playback
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
ERR_SET_PCM_PARAM2:
    alsaClosePcm(&pPlay_daudio0->PlayChls[AoChn].mCfg, 1);  // close the second snd card
#endif
    pthread_mutex_unlock(&pPlay->mAOApiCallLock);
    return FAILURE;
}

ERRORTYPE audioHw_AO_DisableChn(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer; 
	int tmp_cnt = 0;

#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    AudioOutputDevice *pPlay_daudio0 = &gAudioHwDev[AudioDevId+1].mPlay; // related with second snd card
    AIO_MIXER_S *pMixer_daudio0 = &gAudioHwDev[AudioDevId+1].mMixer;

#endif
    pthread_mutex_lock(&pPlay->mAOApiCallLock);
    if (pPlay->PlayChls[AoChn].mState == AO_STATE_INVALID) {
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return ERR_AO_NOT_CONFIG;
    }
    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return SUCCESS;
    }

    //pPlay->mThdRunning = FALSE;
    //pthread_join(pPlay->mThdId, (void*) &ret);

    /* if (!list_empty(&pPlay->mChnList)) {
        alogw("When ao_disable, still exist channle in PlayChnList?! list them below:");
        AO_CHANNEL_S *pEntry;
        list_for_each_entry(pEntry, &pPlay->mChnList, mList)
        {
            alogw("AoCardType[%d] AoChn[%d] still run!", pPlay->PlayChls[AoChn].mAttr.mPcmCardId, pEntry->mId);
        }
        pthread_mutex_unlock(&pPlay->mAOApiCallLock);
        return SUCCESS;
    } */

    alogd("close pcm! current AoCardType:[%d-%d]",AoChn, pPlay->PlayChls[AoChn].mAttr.mPcmCardId);
    alsaClosePcm(&pPlay->PlayChls[AoChn].mCfg, 1);  // 1: playback
#if (MPPCFG_AEC == OPTION_AEC_ENABLE)
    for(int j=0; j<MAX_AO_CHLS; ++j)
    {
        if(pPlay->PlayChls[j].mState == AO_STATE_STARTED)
        {
            tmp_cnt++;
        }
    }
    if(1 == tmp_cnt)
    {
        //alsaMixerSetAudioCodecHubMode(pMixer,0);    // to clean hub mode for the first snd card
        //alsaMixerSetDAudio0HubMode(pMixer_daudio0,0);// to clean hub mode for the second snd card
        //alsaMixerSetDAudio0LoopBackEn(pMixer_daudio0,0);// to disable loopback for the second snd card
        alogd("close daudio0 pcm! current AoCardType:[%d-%d], daudio0:[%d-%d]",
            AoChn, pPlay->PlayChls[AoChn].mAttr.mPcmCardId,
            pPlay_daudio0->daudio0_ao_chl, pPlay_daudio0->PlayChls[pPlay_daudio0->daudio0_ao_chl].mAttr.mPcmCardId);
        alsaClosePcm(&pPlay_daudio0->PlayChls[pPlay_daudio0->daudio0_ao_chl].mCfg, 1);  // to close the the second snd card
        pPlay_daudio0->daudio0_ao_chl = -1;
    }
#endif
    pPlay->PlayChls[AoChn].mState = AO_STATE_CONFIGURED;
    pthread_mutex_unlock(&pPlay->mAOApiCallLock);

    return SUCCESS;
}

ERRORTYPE audioHw_AO_SetDacDrc(AUDIO_DEV AudioDevId, int enable)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerSetAudioCodecDacDrc(pMixer, enable);
}

ERRORTYPE audioHw_AO_SetDacHpf(AUDIO_DEV AudioDevId, int enable)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;
	
    /* if (pPlay->mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerSetAudioCodecDacHpf(pMixer, enable);
}


ERRORTYPE audioHw_AO_SetVolume(AUDIO_DEV AudioDevId, int s32VolumeDb)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerSetVolume(pMixer, 1, s32VolumeDb);
}

ERRORTYPE audioHw_AO_GetVolume(AUDIO_DEV AudioDevId, int *ps32VolumeDb)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerGetVolume(pMixer, 1, (long*)ps32VolumeDb);
}

ERRORTYPE audioHw_AO_SetSoftVolume(AUDIO_DEV AudioDevId, int s32Volume)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerSetSoftVolume(pMixer, 1, s32Volume);
}

ERRORTYPE audioHw_AO_GetSoftVolume(AUDIO_DEV AudioDevId, int *ps32Volume)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerGetSoftVolume(pMixer, 1, (long*)ps32Volume);
}

ERRORTYPE audioHw_AO_SetMute(AUDIO_DEV AudioDevId, BOOL bEnable, AUDIO_FADE_S *pstFade)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    return alsaMixerSetMute(pMixer, 1, (int)bEnable);
}

ERRORTYPE audioHw_AO_GetMute(AUDIO_DEV AudioDevId, BOOL *pbEnable, AUDIO_FADE_S *pstFade)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    /* if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    } */

    int MainVolVal;
    alsaMixerGetMute(pMixer, 1, &MainVolVal);
    if (MainVolVal > 0)
        *pbEnable = FALSE;
    else
        *pbEnable = TRUE;

    return SUCCESS;
}

ERRORTYPE audioHw_AO_SetPA(AUDIO_DEV AudioDevId, BOOL bHighLevel)
{
    ERRORTYPE ret;
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    ret = alsaMixerSetPlayBackPA(pMixer, (int)bHighLevel);
    if(0 != ret)
    {
        aloge("fatal error! alsaMixer SetPlayBackPA fail[0x%x]!", ret);
    }
    return ret;
}

ERRORTYPE audioHw_AO_GetPA(AUDIO_DEV AudioDevId, BOOL *pbHighLevel)
{
    ERRORTYPE ret;
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    int bHighLevel = 0;
    ret = alsaMixerGetPlayBackPA(pMixer, &bHighLevel);
    if(0 == ret)
    {
        *pbHighLevel = bHighLevel?TRUE:FALSE;
    }
    else
    {
        aloge("fatal error! alsaMixer GetPlayBackPA fail[0x%x]!", ret);
    }

    return ret;
}

ERRORTYPE audioHw_AO_FillPcmRingBuf(AUDIO_DEV AudioDevId,AO_CHN AoChn, void* pData, int Len)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    size_t frame_cnt = Len / (pPlay->PlayChls[AoChn].mCfg.bitsPerFrame >> 3);
    ssize_t ret;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }
    ret = alsaWritePcm(&pPlay->PlayChls[AoChn].mCfg, pData, frame_cnt);
    if (ret != frame_cnt) {
        aloge("alsaWritePcm error!");
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE audioHw_AO_DrainPcmRingBuf(AUDIO_DEV AudioDevId,AO_CHN AoChn)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    alsaDrainPcm(&pPlay->PlayChls[AoChn].mCfg);

    return SUCCESS;
}

ERRORTYPE audioHw_AO_FeedPcmData(AUDIO_DEV AudioDevId,AO_CHN AoChn, AUDIO_FRAME_S *pFrm)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;
    size_t frame_cnt = pFrm->mLen / (pPlay->PlayChls[AoChn].mCfg.bitsPerFrame >> 3);
    ssize_t ret;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }

    ret = alsaWritePcm(&pPlay->PlayChls[AoChn].mCfg, pFrm->mpAddr, frame_cnt);
    if (ret != frame_cnt) {
        aloge("alsaWritePcm error!");
        return FAILURE;
    }

    return SUCCESS;
}

ERRORTYPE audioHw_AO_GetPcmConfig(AUDIO_DEV AudioDevId,AO_CHN AoChn, PCM_CONFIG_S **ppCfg)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }
    *ppCfg = &pPlay->PlayChls[AoChn].mCfg;
    return SUCCESS;
}

ERRORTYPE audioHw_AO_GetAIOAttr(AUDIO_DEV AudioDevId,AO_CHN AoChn, AIO_ATTR_S **ppAttr)
{
    AudioOutputDevice *pPlay = &gAudioHwDev[AudioDevId].mPlay;

    if (pPlay->PlayChls[AoChn].mState != AO_STATE_STARTED) {
        return ERR_AO_NOT_ENABLED;
    }
    *ppAttr = &pPlay->PlayChls[AoChn].mAttr;
    return SUCCESS;
}

ERRORTYPE audioHw_AI_SuspendAns(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    pthread_mutex_lock(&pCap->mChnListLock);
    if (pCap->mState != AI_STATE_STARTED)
    {
        aloge("fatal error! audioDev[%d] is not started!", AudioDevId);
        pthread_mutex_unlock(&pCap->mChnListLock);
        return ERR_AI_NOT_ENABLED;
    }
    pCap->mbSuspendAns = TRUE;
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}
ERRORTYPE audioHw_AI_ResumeAns(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    pthread_mutex_lock(&pCap->mChnListLock);
    if (pCap->mState != AI_STATE_STARTED)
    {
        aloge("fatal error! audioDev[%d] is not started!", AudioDevId);
        pthread_mutex_unlock(&pCap->mChnListLock);
        return ERR_AI_NOT_ENABLED;
    }
    pCap->mbSuspendAns = FALSE;
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_SuspendAec(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    pthread_mutex_lock(&pCap->mChnListLock);
    if (pCap->mState != AI_STATE_STARTED)
    {
        aloge("fatal error! audioDev[%d] is not started!", AudioDevId);
        pthread_mutex_unlock(&pCap->mChnListLock);
        return ERR_AI_NOT_ENABLED;
    }
    pCap->mbSuspendAec = TRUE;
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

ERRORTYPE audioHw_AI_ResumeAec(AUDIO_DEV AudioDevId)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    pthread_mutex_lock(&pCap->mChnListLock);
    if (pCap->mState != AI_STATE_STARTED)
    {
        aloge("fatal error! audioDev[%d] is not started!", AudioDevId);
        pthread_mutex_unlock(&pCap->mChnListLock);
        return ERR_AI_NOT_ENABLED;
    }
    pCap->mbSuspendAec = FALSE;
    pthread_mutex_unlock(&pCap->mChnListLock);
    return SUCCESS;
}

/*
ERRORTYPE audioHw_AI_SetAgcDb(AUDIO_DEV AudioDevId, float fDbGain)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    //agc_handle *ai_agc_float_hld;
    int ret = -1;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    if (fDbGain < 0 || fDbGain > 95)
    {
        aloge("agc db gain %f invalid", fDbGain);
        return ERR_AI_ILLEGAL_PARAM;
    }

  #if (MPPCFG_AGC == OPTION_AGC_ENABLE)
    pthread_mutex_lock(&pCap->mAgcDbGainLock);
    ret = func_agc_set_maxgain_db((agc_handle *)pCap->ai_agc_float_handle, fDbGain);
    if (ret != 0)
    {
        aloge("agc float set db failed");
        pthread_mutex_unlock(&pCap->mAgcDbGainLock);
        return ERR_AI_ILLEGAL_PARAM;
    }
    else
    {
        pCap->mAttr.ai_agc_float_cfg.fMaxGainDb = fDbGain;
        alogd("agc db gain set:%f", fDbGain);
    }
    pthread_mutex_unlock(&pCap->mAgcDbGainLock);
  #endif
    return 0;
}

ERRORTYPE audioHw_AI_GetAgcDb(AUDIO_DEV AudioDevId, float *pfVolumeDb)
{
    AudioInputDevice *pCap = &gAudioHwDev[AudioDevId].mCap;
    AIO_MIXER_S *pMixer = &gAudioHwDev[AudioDevId].mMixer;

    if (pCap->mState != AI_STATE_STARTED) {
        return ERR_AI_NOT_ENABLED;
    }

    pthread_mutex_lock(&pCap->mAgcDbGainLock);
    *pfVolumeDb = pCap->mAttr.ai_agc_float_cfg.fMaxGainDb;
    pthread_mutex_unlock(&pCap->mAgcDbGainLock);

    alogd("get_ai_agc_db_gain:%f", *pfVolumeDb);
    return 0;
}
*/

