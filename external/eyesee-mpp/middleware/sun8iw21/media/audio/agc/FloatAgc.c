#define LOG_NDEBUG 0
#define LOG_TAG "FloatAgc"
#include <utils/plat_log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <SystemBase.h>
#include <mm_comm_aio.h>
#include <media_common_aio.h>

#include <agc_float.h>

#include "FloatAgc.h"

//#define AI_HW_AGC_DEBUG_EN

/**
  create FloatAgc context.

  @param nFrameLen
    unit: byte. audio frame bytes number.
*/
FloatAgcContext* ConstructFloatAgcContext(int nSampleRate, int nChnNum, int nFrameLen, AGC_FLOAT_CONFIG_S *pAgcConfig)
{
    int ret;
    FloatAgcContext *pCtx = NULL;
    alogd("agc_float_to_init param:%f-%f", pAgcConfig->fTargetDb, pAgcConfig->fMaxGainDb);
    if ((pAgcConfig->fTargetDb < -30 || pAgcConfig->fTargetDb > 0)
        || (pAgcConfig->fMaxGainDb < 0 || pAgcConfig->fMaxGainDb > 30))
    {
        aloge("fatal error! agc float init param invalid");
        pCtx = NULL;
    }
    else
    {
        agc_handle *ai_agc_float_hld = func_agc_init(nSampleRate, nChnNum, pAgcConfig->fTargetDb, pAgcConfig->fMaxGainDb);
        if (ai_agc_float_hld == NULL)
        {
            aloge("agc float init failed");
            pCtx = NULL;
        }
        else
        {
            pCtx = (FloatAgcContext*)calloc(1, sizeof(FloatAgcContext));
            if(NULL == pCtx)
            {
                aloge("fatal error! malloc fail");
            }
            pCtx->ai_agc_float_handle = ai_agc_float_hld;
            pCtx->ai_agc_float_tmp_buff = (short *)malloc(nFrameLen);
            if(NULL == pCtx->ai_agc_float_tmp_buff)
            {
                aloge("fatal error! agc_in_ai_error_malloc_tmp_buff_failed:%d", nFrameLen);
            }
//            ret = pthread_mutex_init(&pCtx->mAgcDbGainLock, NULL);
//            if(ret != 0)
//            {
//                aloge("fatal error! pthread mutex init fail:%d", ret);
//            }
        }
    }
    return pCtx;
}
void DestructFloatAgcContext(FloatAgcContext *pCtx)
{
    int ret;
    ret = func_agc_exit((agc_handle *)pCtx->ai_agc_float_handle);
    if(pCtx->ai_agc_float_tmp_buff != NULL)
    {
        free(pCtx->ai_agc_float_tmp_buff);
        pCtx->ai_agc_float_tmp_buff = NULL;
    }
//    ret = pthread_mutex_destroy(&pCtx->mAgcDbGainLock);
//    if(ret != 0)
//    {
//        aloge("fatal error! pthread mutex destroy fail:%d", ret);
//    }

  #ifdef AI_HW_AGC_DEBUG_EN
    if(NULL != pCtx->tmp_pcm_fp_in)
    {
        fclose(pCtx->tmp_pcm_fp_in);
        pCtx->tmp_pcm_fp_in = NULL;
    }
    if(NULL != pCtx->tmp_pcm_fp_out)
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
int FloatAgcProcess(void *cookie, AUDIO_FRAME_S *pFrm)
{
    int ret;
    FloatAgcContext *pCtx = (FloatAgcContext*)cookie;
    //pthread_mutex_lock(&pCtx->mAgcDbGainLock);
    int nBitWidth = map_AUDIO_BIT_WIDTH_E_to_BitWidth(pFrm->mBitwidth);
    int nSampleNum = pFrm->mLen/(nBitWidth/8); //samples in all channels, not alsa frame.
    ret = func_agc_proc(pCtx->ai_agc_float_handle, (short*)pFrm->mpAddr, pCtx->ai_agc_float_tmp_buff, nSampleNum);
    //pthread_mutex_unlock(&pCtx->mAgcDbGainLock);
    if (ret != 0)
    {
        aloge("fatal error! func agc_proc failed");
    }
    else
    {
        memcpy(pFrm->mpAddr, pCtx->ai_agc_float_tmp_buff, pFrm->mLen);
    }
    return 0;
}

/**
  implement of AgcGetInnerDataInfoFuncType.
*/
int FloatAgcGetInnerDataInfo(void *cookie, char **ppCacheProcessedData, int *pCacheProcessedDataLen,
        char **ppLeftOriginData, int *pLeftOriginDataLen)
{
    FloatAgcContext *pCtx = (FloatAgcContext*)cookie;
    *ppCacheProcessedData = NULL;
    *pCacheProcessedDataLen = 0;
    *ppLeftOriginData = NULL;
    *pLeftOriginDataLen = 0;
    return 0;
}

