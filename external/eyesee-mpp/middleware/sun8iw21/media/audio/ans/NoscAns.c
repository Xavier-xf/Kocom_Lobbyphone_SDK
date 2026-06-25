#define LOG_NDEBUG 0
#define LOG_TAG "NoscAns"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>

#include <media_common_aio.h>
#include <nosc.h>

#include "NoscAns.h"

NoscAnsContext* ConstructNoscAnsContext()
{
    NoscAnsContext *pCtx = (NoscAnsContext*)malloc(sizeof(NoscAnsContext));
    if(NULL == pCtx)
    {
        aloge("fatal error! malloc fail");
    }
    memset(pCtx, 0, sizeof(*pCtx));
  #ifdef AI_ANS_DEBUG_EN
    pCtx->fin = fopen("/mnt/extsd/ans_in.pcm", "wb"); 
    pCtx->fout = fopen("/mnt/extsd/ans_out.pcm", "wb");
    if(NULL==pCtx->fin || NULL==pCtx->fout)
    {
        aloge("fatal error! nosc ans debug file create failed");
    }
  #endif

    return pCtx;
}
void DestructNoscAnsContext(NoscAnsContext *pCtx)
{
    int ret;
    if(pCtx->nosc != NULL)
    {
        NOSCdestroy(pCtx->nosc);
        pCtx->nosc = NULL;
    }

  #ifdef AI_ANS_DEBUG_EN
    if(pCtx->fin)
    {
        fclose(pCtx->fin);
        pCtx->fin = NULL;
    }
    if(pCtx->fout)
    {
        fclose(pCtx->fout);
        pCtx->fout = NULL;
    }
  #endif
    free(pCtx);
}

/**
  In Nosc lib, need indicate stationarity of noise.
*/
static sta_t map_ansMode_to_sta_t(int nAnsMode)
{
    sta_t nonstationary;
    switch(nAnsMode)
    {
        case 0:
        {
            nonstationary = STATIONAL;
            break;
        }
        case 1:
        {
            nonstationary = LOW_NONSTATIONAL;
            break;
        }
        case 2:
        {
            nonstationary = MEDIUM_NONSTATIONAL;
            break;
        }
        case 3:
        {
            nonstationary = HIGH_NONSTATIONAL;
            break;
        }
        default:
        {
            aloge("fatal error! not support ansMode[%d]", nAnsMode);
            nonstationary = STATIONAL;
            break;
        }
    }
    return nonstationary;
}

/**
  implement of AnsProcessFuncType.
*/
int NoscAnsProcess(void *cookie, AUDIO_FRAME_S *pFrm, const AIO_ATTR_S *pAttr, BOOL bSuspendAns)
{
    int rc;
    int ret;
    if(pAttr->enSoundmode != AUDIO_SOUND_MODE_MONO && pAttr->enSoundmode != AUDIO_SOUND_MODE_STEREO)
    {
        aloge("fatal error! NoscAns not support soundmode[%d]", pAttr->enSoundmode);
        return 0;
    }
    NoscAnsContext *pCtx = (NoscAnsContext*)cookie;
    if(NULL == pCtx->nosc)
    {
        int nSampleRate = (int)map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pAttr->enSamplerate);
        int nChnNum = judgeAudioChnNumBySoundMode(pAttr->enSoundmode, NULL, NULL);
        sta_t eNonstat = map_ansMode_to_sta_t(pAttr->ai_ans_mode);

        memset(&pCtx->mNoscInitParams, 0, sizeof(pCtx->mNoscInitParams));
        pCtx->mNoscInitParams.sampling_rate = nSampleRate;
        pCtx->mNoscInitParams.channum = nChnNum;
        pCtx->mNoscInitParams.max_suppression = -30;
        pCtx->mNoscInitParams.overlap_percent = OVERLAP_SEVENTY_FIVE;
        pCtx->mNoscInitParams.nonstat = eNonstat; //LOW_NONSTATIONAL is recommended.
        pCtx->nosc = NOSCinit(&pCtx->mNoscInitParams);
        if(NULL == pCtx->nosc)
        {
            aloge("fatal error! ans nosc init fail");
            return 0;
        }
    }
    int nSampleRate = (int)map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pFrm->mSamplerate);
    int nChnNum = judgeAudioChnNumBySoundMode(pFrm->mSoundmode, NULL, NULL);
    int nBitWidth = (int)map_AUDIO_BIT_WIDTH_E_to_BitWidth(pFrm->mBitwidth);
    if(nBitWidth != 16)
    {
        aloge("fatal error! bitWidth[%d] must be 16!", nBitWidth);
    }
    if(nSampleRate != pCtx->mNoscInitParams.sampling_rate || nChnNum != pCtx->mNoscInitParams.channum)
    {
        aloge("fatal error! audio param wrong![%d-%d]!=[%d-%d]", nSampleRate, nChnNum, pCtx->mNoscInitParams.sampling_rate, pCtx->mNoscInitParams.channum);
    }
    int nSampleNum = pFrm->mLen/(nBitWidth/8);
    if(nSampleNum%nChnNum != 0)
    {
        aloge("fatal error! check audio frame info:%d-%d", nSampleNum, nChnNum);
    }

  #ifdef AI_ANS_DEBUG_EN
    fwrite(pFrm->mpAddr, 1, pFrm->mLen, pCtx->fin);
  #endif

    NOSCdec(pCtx->nosc, (short*)pFrm->mpAddr, nSampleNum);

  #ifdef AI_ANS_DEBUG_EN
    fwrite(pFrm->mpAddr, 1, pFrm->mLen, pCtx->fout);
  #endif

    return 0;
}

