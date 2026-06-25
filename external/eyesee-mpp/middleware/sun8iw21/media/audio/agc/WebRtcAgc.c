#define LOG_NDEBUG 0
#define LOG_TAG "WebRtcAgc"
#include <utils/plat_log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <media_common_aio.h>
#include <agc_lib.h>
#include "WebRtcAgc.h"

//#define AI_HW_AGC_DEBUG_EN

/**
  create WebRtcAgc context.

  @param nFrameLen
    unit: byte. audio frame bytes number.
*/
WebRtcAgcContext* ConstructWebRtcAgcContext(int nSampleRate, int nChnNum, int nFrameLen, AGC_FLOAT_CONFIG_S *pAgcConfig)
{
    int ret;
    WebRtcAgcContext *pCtx = (WebRtcAgcContext*)calloc(1, sizeof(WebRtcAgcContext));
    if(NULL == pCtx)
    {
        aloge("fatal error! malloc fail");
    }
    if(nChnNum != 1)
    {
        aloge("fatal error! WebRtcAgc only support one channel! curChnNum=%d", nChnNum);
        free(pCtx);
        return NULL;
    }
    int nChnLen = nFrameLen/nChnNum;

    ret = aw_WebRtcAgc_Create(&pCtx->agcInst);
    if(ret != 0)
    {
        aloge("fatal error! WebRtcAgc create fail[%d]", ret);
    }

    int minLevel = 0;
    int maxLevel = 255;
    int agcMode = kAgcModeAdaptiveAnalog;
    ret = aw_WebRtcAgc_Init(pCtx->agcInst, minLevel, maxLevel, agcMode, nSampleRate);
    if(ret != 0)
    {
        aloge("fatal error! WebRtcAgc init fail[%d]", ret);
    }

    memset(&pCtx->agcConfig, 0x0, sizeof(WebRtcAgc_config_t));
    pCtx->agcConfig.targetLevelDbfs = -pAgcConfig->fTargetDb;
    pCtx->agcConfig.compressionGaindB = pAgcConfig->fMaxGainDb;
    pCtx->agcConfig.limiterEnable = kAgcTrue;
    aw_WebRtcAgc_set_config(pCtx->agcInst, pCtx->agcConfig);

    pCtx->inBuff = (short *)malloc(nChnLen*2);
    if(NULL == pCtx->inBuff)
    {
        aloge("fatal error! malloc fail:%d", nChnLen*2);
    }
    pCtx->inBuffLen = nChnLen*2;
    pCtx->inBuffDataRemainLen = 0;
    pCtx->outBuff = (short *)malloc(nChnLen*2);
    if(NULL == pCtx->outBuff)
    {
        aloge("fatal error! malloc fail:%d", nChnLen*2);
    }
    pCtx->outBuffLen = nChnLen*2;
    pCtx->outBuffDataRemainLen = 0;

#ifdef AI_HW_AGC_DEBUG_EN
    pCtx->tmp_pcm_fp_in = fopen("/mnt/extsd/tmp_in_agc_pcm", "wb");
    pCtx->tmp_pcm_fp_out = fopen("/mnt/extsd/tmp_out_agc_pcm", "wb");
    if(NULL==pCtx->tmp_pcm_fp_in || NULL==pCtx->tmp_pcm_fp_out)
    {
        aloge("fatal error! agc_debug_file_create_failed");
    }
#endif
    return pCtx;
}
void DestructWebRtcAgcContext(WebRtcAgcContext *pCtx)
{
    int ret;

    if(NULL != pCtx->agcInst)
    {
        ret = aw_WebRtcAgc_Free(pCtx->agcInst);
        if(ret!=0)
        {
            aloge("fatal error! agc free failed");
        }
        pCtx->agcInst = NULL;
    }
    if(pCtx->outBuff != NULL)
    {
        free(pCtx->outBuff);
        pCtx->outBuff = NULL;
    }
    if(NULL != pCtx->inBuff)
    {
        free(pCtx->inBuff);
        pCtx->inBuff = NULL;
    }
  #ifdef AI_HW_AGC_DEBUG_EN
    if(pCtx->tmp_pcm_fp_in != NULL)
    {
        fclose(pCtx->tmp_pcm_fp_in);
        pCtx->tmp_pcm_fp_in = NULL;
    }
    if(pCtx->tmp_pcm_fp_out != NULL)
    {
        fclose(pCtx->tmp_pcm_fp_out);
        pCtx->tmp_pcm_fp_out = NULL;
    }
  #endif
    free(pCtx);
}

/**
  implement of AgcProcessFuncType.
*/
int WebRtcAgcProcess(void *cookie, AUDIO_FRAME_S *pFrm)
{
    int rc;
    int ret;
    if(pFrm->mSoundmode != AUDIO_SOUND_MODE_MONO)
    {
        aloge("fatal error! webRtcAgc only can process one channel, not support soundmode[%d]", pFrm->mSoundmode);
        return 0;
    }
    int nBitWidth = (int)map_AUDIO_BIT_WIDTH_E_to_BitWidth(pFrm->mBitwidth);
    if(nBitWidth != 16)
    {
        aloge("fatal error! bitWidth[%d] must be 16!", nBitWidth);
    }
    int nChnNum = judgeAudioChnNumBySoundMode(pFrm->mSoundmode, NULL, NULL);
    int nAlsaFrameBytes = nBitWidth*nChnNum/8;
    WebRtcAgcContext *pCtx = (WebRtcAgcContext*)cookie;

    if(pCtx->inBuffDataRemainLen + pFrm->mLen <= pCtx->inBuffLen)
    {
        memcpy((char*)pCtx->inBuff + pCtx->inBuffDataRemainLen, (char*)pFrm->mpAddr, pFrm->mLen);
        pCtx->inBuffDataRemainLen += pFrm->mLen;
    }
    else
    {
        aloge("fatal error! in_buff_over_flow:%d-%d-%d", pCtx->inBuffDataRemainLen, pCtx->inBuffLen, pFrm->mLen);
    }

    int frm_size = 160;         // 160 samples as one unit processed by agc library
    short *near_frm_ptr = (short *)pCtx->inBuff;
    short *processed_frm_ptr = (short *)((char*)pCtx->outBuff+pCtx->outBuffDataRemainLen);
    int left = pCtx->inBuffDataRemainLen / nAlsaFrameBytes;
    // start to process
    while(left >= frm_size)
    {
        if((pCtx->outBuffDataRemainLen + frm_size*(nBitWidth/8)) > pCtx->outBuffLen)
        {
            aloge("fatal error! WebRtcAgc out buff not enough![%d-%d]bytes", pCtx->outBuffDataRemainLen, pCtx->outBuffLen);
        }
        int32_t inMicLevel = 0;
        int32_t outMicLevel = 0;
        uint8_t saturationWarning = 0;
        ret = aw_WebRtcAgc_Process(pCtx->agcInst, near_frm_ptr, NULL, frm_size, processed_frm_ptr, NULL, inMicLevel, &outMicLevel,
            0, &saturationWarning);
        if (ret != 0)
        {
            aloge("fatal error! failed in WebRtcAgc_Process");
        }
        if(saturationWarning != 0)
        {
            alogw("Be careful! agc saturation event occured! maybe agc params[%d-%d] too large", pCtx->agcConfig.targetLevelDbfs,
                pCtx->agcConfig.compressionGaindB);
        }
        near_frm_ptr += frm_size;
        processed_frm_ptr += frm_size;
        left -= frm_size;

        pCtx->inBuffDataRemainLen -= frm_size*sizeof(short);
        pCtx->outBuffDataRemainLen += frm_size*(nBitWidth/8);
    }

    // move remaining data in internal buffer to the beginning of the buffer
    if(left > 0)
    {
        memmove((char*)pCtx->inBuff, (char*)near_frm_ptr, pCtx->inBuffDataRemainLen);
    }

    // fetch one valid output frame from output internal buffer, the length of valid frame must equal to chunsize.
    if(pCtx->outBuffDataRemainLen >= pFrm->mLen)
    {
        memcpy((char *)pFrm->mpAddr, (char *)pCtx->outBuff, pFrm->mLen);
        pCtx->outBuffDataRemainLen -= pFrm->mLen;

        if(pCtx->outBuffDataRemainLen > pFrm->mLen) //can be same, but we send one frame one time, so next time to send next frame.
        {
            aloge("fatal error! agc_out_buff_data too long:%d-%d", pFrm->mLen, pCtx->outBuffDataRemainLen);
        }
        memmove((char *)pCtx->outBuff, ((char *)pCtx->outBuff + pFrm->mLen), pCtx->outBuffDataRemainLen);
        rc = 0;
    }
    else
    {
        rc = 1;
    }
    return rc;
}

/**
  implement of AgcGetInnerDataInfoFuncType.
*/
int WebRtcAgcGetInnerDataInfo(void *cookie, char **ppCacheProcessedData, int *pCacheProcessedDataLen,
        char **ppLeftOriginData, int *pLeftOriginDataLen)
{
    WebRtcAgcContext *pCtx = (WebRtcAgcContext*)cookie;
    if(pCtx->outBuffDataRemainLen > 0)
    {
        *ppCacheProcessedData = (char*)pCtx->outBuff;
        *pCacheProcessedDataLen = pCtx->outBuffDataRemainLen;
    }
    else
    {
        *ppCacheProcessedData = NULL;
        *pCacheProcessedDataLen = 0;
    }
    if(pCtx->inBuffDataRemainLen > 0)
    {
        *ppLeftOriginData = (char*)pCtx->inBuff;
        *pLeftOriginDataLen = pCtx->inBuffDataRemainLen;
    }
    else
    {
        *ppLeftOriginData = NULL;
        *pLeftOriginDataLen = 0;
    }
    return 0;
}

