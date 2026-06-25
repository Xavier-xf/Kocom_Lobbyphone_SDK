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

#include "AOFloatAgc.h"

//#define AI_HW_AGC_DEBUG_EN

/**
  process input pcm. floatAgc can process arbitrary pcm data length, so it will left none inner data.
*/
static int AOFloatAgcProcess(AOAgc *pBase, AUDIO_FRAME_S *pFrm, bool bDrainFlag)
{
    int ret;
    AOFloatAgc *pThiz = (AOFloatAgc*)pBase;
    //pthread_mutex_lock(&pThiz->mAgcDbGainLock);
    int nBitWidth = map_AUDIO_BIT_WIDTH_E_to_BitWidth(pFrm->mBitwidth);
    if (nBitWidth != 16)
    {
        aloge("fatal error! bitWidth[%d] must be 16!", nBitWidth);
    }
    if (nBitWidth != pThiz->mBase.mBitWidth)
    {
        aloge("fatal error! bitWidth unmatch[%d!=%d]!", nBitWidth, pThiz->mBase.mBitWidth);
    }
    if (pFrm->mLen > pThiz->nTmpBuffLen)
    {
        aloge("fatal error! outBuf overflow:%d-%d-%d, need extend memory", pThiz->nTmpBuffLen, pFrm->mLen);
        pThiz->ai_agc_float_tmp_buff = (short *)realloc((void *)pThiz->ai_agc_float_tmp_buff, pFrm->mLen);
        if (NULL == pThiz->ai_agc_float_tmp_buff)
        {
            aloge("fatal error! realloc fail!");
        }
        pThiz->nTmpBuffLen = pFrm->mLen;
    }
    int nSampleNum = pFrm->mLen/(nBitWidth/8); //samples in all channels, not alsa frame.
    ret = func_agc_proc(pThiz->ai_agc_float_handle, (short*)pFrm->mpAddr, pThiz->ai_agc_float_tmp_buff, nSampleNum);
    //pthread_mutex_unlock(&pThiz->mAgcDbGainLock);
    if (ret != 0)
    {
        aloge("fatal error! func agc_proc failed");
    }
    else
    {
        memcpy(pFrm->mpAddr, pThiz->ai_agc_float_tmp_buff, pFrm->mLen);
    }
    return 0;
}

static int AOFloatAgcClearData(AOAgc *pBase)
{
    AOFloatAgc *pThiz = (AOFloatAgc*)pBase;
    return 0;
}

static int AOFloatAgcGetInputPcmData(AOAgc *pBase, char **ppInputPcmData, int *pInputPcmDataLen)
{
    AOWebRtcAgc *pThiz = (AOWebRtcAgc*)pBase;
    *ppInputPcmData = NULL;
    *pInputPcmDataLen = 0;
    return 0;
}

static int AOFloatAgcUpdateConfig(AOAgc *pBase, AGC_FLOAT_CONFIG_S *pAgcConfig)
{
    int ret;
    AOFloatAgc *pThiz = (AOFloatAgc*)pBase;
    ret = func_agc_set_target_db(pThiz->ai_agc_float_handle, pAgcConfig->fTargetDb);
    if (ret != 0)
    {
        aloge("fatal error! floatAgc set targetDb[%f] fail[%d]", pAgcConfig->fTargetDb, ret);
    }
    ret = func_agc_set_maxgain_db(pThiz->ai_agc_float_handle, pAgcConfig->fMaxGainDb);
    if (ret != 0)
    {
        aloge("fatal error! floatAgc set maxGainDb[%f] fail[%d]", pAgcConfig->fMaxGainDb, ret);
    }
    return ret;
}

static void DeleteAOFloatAgc(AOAgc *pBase)
{
    int ret;
    AOFloatAgc *pThiz = (AOFloatAgc *)pBase;
    ret = func_agc_exit((agc_handle *)pThiz->ai_agc_float_handle);
    if(pThiz->ai_agc_float_tmp_buff != NULL)
    {
        free(pThiz->ai_agc_float_tmp_buff);
        pThiz->ai_agc_float_tmp_buff = NULL;
    }
//    ret = pthread_mutex_destroy(&pThiz->mAgcDbGainLock);
//    if(ret != 0)
//    {
//        aloge("fatal error! pthread mutex destroy fail:%d", ret);
//    }

  #ifdef AI_HW_AGC_DEBUG_EN
    if(NULL != pThiz->tmp_pcm_fp_in)
    {
        fclose(pThiz->tmp_pcm_fp_in);
        pThiz->tmp_pcm_fp_in = NULL;
    }
    if(NULL != pThiz->tmp_pcm_fp_out)
    {
        fclose(pThiz->tmp_pcm_fp_out);
        pThiz->tmp_pcm_fp_out = NULL;
    }
  #endif
    free(pThiz);
}

/**
  create FloatAgc context.

  @param nFrameLen
    unit: byte. audio frame bytes number.
*/
AOFloatAgc *CreateAOFloatAgc(int nSampleRate, int nChnNum, int nBitWidth, int nFrameLen, AGC_FLOAT_CONFIG_S *pAgcConfig)
{
    int ret;
    AOFloatAgc *pThiz = NULL;
    alogd("agc_float_to_init param:%f-%f", pAgcConfig->fTargetDb, pAgcConfig->fMaxGainDb);
    if ((pAgcConfig->fTargetDb < -30 || pAgcConfig->fTargetDb > 0)
        || (pAgcConfig->fMaxGainDb < 0 || pAgcConfig->fMaxGainDb > 30))
    {
        aloge("fatal error! agc float init param invalid");
        pThiz = NULL;
    }
    else
    {
        agc_handle *ai_agc_float_hld = func_agc_init(nSampleRate, nChnNum, pAgcConfig->fTargetDb, pAgcConfig->fMaxGainDb);
        if (ai_agc_float_hld == NULL)
        {
            aloge("fatal error! agc float init failed");
            pThiz = NULL;
        }
        else
        {
            pThiz = (FloatAgcContext*)calloc(1, sizeof(FloatAgcContext));
            if(NULL == pThiz)
            {
                aloge("fatal error! malloc fail");
            }
            pThiz->mBase.mSampleRate = nSampleRate;
            pThiz->mBase.mChnNum = nChnNum;
            pThiz->mBase.mBitWidth = nBitWidth;
            pThiz->mBase.mpAOAgcProcess = AOFloatAgcProcess;
            pThiz->mBase.mpAOAgcClearData = AOFloatAgcClearData;
            pThiz->mBase.mpAOAgcGetInputPcmData = AOFloatAgcGetInputPcmData;
            pThiz->mBase.mpAOAgcUpdateConfig = AOFloatAgcUpdateConfig;
            pThiz->mBase.mpAOAgcDelete = DeleteAOFloatAgc;

            pThiz->ai_agc_float_handle = ai_agc_float_hld;
            pThiz->ai_agc_float_tmp_buff = (short *)malloc(nFrameLen);
            pThiz->nTmpBuffLen = nFrameLen;
            if(NULL == pThiz->ai_agc_float_tmp_buff)
            {
                aloge("fatal error! agc_in_ai_error_malloc_tmp_buff_failed:%d", nFrameLen);
            }
//            ret = pthread_mutex_init(&pThiz->mAgcDbGainLock, NULL);
//            if(ret != 0)
//            {
//                aloge("fatal error! pthread mutex init fail:%d", ret);
//            }
        }
    }
    return pThiz;
}

