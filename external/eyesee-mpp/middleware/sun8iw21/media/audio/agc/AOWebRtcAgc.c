#define LOG_NDEBUG 0
#define LOG_TAG "WebRtcAgc"
#include <utils/plat_log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <media_common_aio.h>
#include <agc_lib.h>
#include "AOWebRtcAgc.h"

//#define AI_HW_AGC_DEBUG_EN

/**
  process input pcm data, use processed pcm data to replace input pcm data. Output all processed pcm data as much as
  possible. If drain flag is true, all input original pcm data is output too.

  @return
    0: success
    -1: fail
*/
static int AOWebRtcAgcProcess(AOAgc *pBase, AUDIO_FRAME_S *pFrm, bool bDrainFlag)
{
    int rc = 0;
    int ret;
    int i;
    AOWebRtcAgc *pThiz = (AOWebRtcAgc*)pBase;
    if ((pFrm->mSoundmode != AUDIO_SOUND_MODE_MONO) && (pFrm->mSoundmode != AUDIO_SOUND_MODE_STEREO))
    {
        aloge("fatal error! webRtcAgc only can process one channel, not support soundmode[%d]", pFrm->mSoundmode);
        return -1;
    }
    int nBitWidth = (int)map_AUDIO_BIT_WIDTH_E_to_BitWidth(pFrm->mBitwidth);
    if(nBitWidth != 16)
    {
        aloge("fatal error! bitWidth[%d] must be 16!", nBitWidth);
    }
    if(nBitWidth != pThiz->mBase.mBitWidth)
    {
        aloge("fatal error! bitWidth unmatch[%d!=%d]!", nBitWidth, pThiz->mBase.mBitWidth);
    }
    int nChnNum = judgeAudioChnNumBySoundMode(pFrm->mSoundmode, NULL, NULL);
    if(nChnNum != pThiz->mBase.mChnNum)
    {
        aloge("fatal error! audio channel number unmatch[%d!=%d]!", nChnNum, pThiz->mBase.mChnNum);
    }
    int nSampleRate = map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pFrm->mSamplerate);
    if(nSampleRate != pThiz->mBase.mSampleRate)
    {
        aloge("fatal error! sample rate unmatch[%d!=%d]!", nSampleRate, pThiz->mBase.mSampleRate);
    }

    int nAlsaFrameBytes = nBitWidth*nChnNum/8;

    if (pThiz->inBuffDataRemainLen + pFrm->mLen > pThiz->inBuffLen)
    {
        aloge("fatal error! in_buff_over_flow:%d-%d-%d, need extend memory", pThiz->inBuffDataRemainLen, pThiz->inBuffLen,
            pFrm->mLen);
        unsigned int nNewLen = pThiz->inBuffDataRemainLen + pFrm->mLen;
        pThiz->inBuff = (short *)realloc((void *)pThiz->inBuff, nNewLen);
        if (NULL == pThiz->inBuff)
        {
            aloge("fatal error! realloc fail!");
        }
        pThiz->inBuffLen = nNewLen;
    }
    memcpy((char*)pThiz->inBuff + pThiz->inBuffDataRemainLen, (char*)pFrm->mpAddr, pFrm->mLen);
    pThiz->inBuffDataRemainLen += pFrm->mLen;

    if (pThiz->inBuffDataRemainLen + pThiz->outBuffDataRemainLen > pThiz->outBuffLen)
    {
        aloge("fatal error! out_buff_over_flow:%d-%d-%d, need extend memory", pThiz->inBuffDataRemainLen,
            pThiz->outBuffDataRemainLen, pThiz->outBuffLen);
        unsigned int nNewLen = pThiz->inBuffDataRemainLen + pThiz->outBuffDataRemainLen;
        pThiz->outBuff = (short *)realloc((void *)pThiz->outBuff, nNewLen);
        if (NULL == pThiz->outBuff)
        {
            aloge("fatal error! realloc fail!");
        }
        pThiz->outBuffLen = nNewLen;
    }

    int frm_size = 160;         // 160 samples as one unit processed by agc library
    short *near_frm_ptr = (short *)pThiz->inBuff;
    short *processed_frm_ptr = (short *)((char*)pThiz->outBuff+pThiz->outBuffDataRemainLen);
    int left = pThiz->inBuffDataRemainLen / nAlsaFrameBytes * nChnNum;
    // start to process
    while(left >= frm_size * nChnNum)
    {
        for (i=0; i<nChnNum; i++)
        {
            if((pThiz->outBuffDataRemainLen + frm_size*(nBitWidth/8)) > pThiz->outBuffLen)
            {
                aloge("fatal error! WebRtcAgc out buff not enough![%d-%d]bytes", pThiz->outBuffDataRemainLen, pThiz->outBuffLen);
            }
            int32_t inMicLevel = 0;
            int32_t outMicLevel = 0;
            uint8_t saturationWarning = 0;
            ret = aw_WebRtcAgc_Process(pThiz->agcInst, near_frm_ptr, NULL, frm_size, processed_frm_ptr, NULL, inMicLevel, &outMicLevel,
                0, &saturationWarning);
            if (ret != 0)
            {
                aloge("fatal error! failed in WebRtcAgc_Process");
            }
            if(saturationWarning != 0)
            {
                alogw("Be careful! agc saturation event occured! maybe agc params[%d-%d] too large", pThiz->agcConfig.targetLevelDbfs,
                    pThiz->agcConfig.compressionGaindB);
            }
            near_frm_ptr += frm_size;
            processed_frm_ptr += frm_size;
            left -= frm_size;

            pThiz->inBuffDataRemainLen -= frm_size*sizeof(short);
            pThiz->outBuffDataRemainLen += frm_size*(nBitWidth/8);
        }
    }

    // move remaining data in internal buffer to the beginning of the buffer
    if(left > 0)
    {
        memmove((char*)pThiz->inBuff, (char*)near_frm_ptr, pThiz->inBuffDataRemainLen);
    }

    // fetch all valid output pcm data from output internal buffer, the length of valid pcm data maybe larger than
    // pFrm->nBufSize, maybe smaller than pFrm->nBufSize. If drain flag is true, need copy left inputPcmData.
    unsigned int nNewFrameLen = pThiz->outBuffDataRemainLen;
    if (bDrainFlag)
    {
        nNewFrameLen += pThiz->inBuffDataRemainLen;
    }
    if (nNewFrameLen > pFrm->nBufSize)
    {
        alogw("Be careful! outputPcmLen[%d] > frameBufSize[%d], need extend!", nNewFrameLen, pFrm->nBufSize);
        pFrm->mpAddr = (void *)realloc((void *)pFrm->mpAddr, nNewFrameLen);
        if (NULL == pFrm->mpAddr)
        {
            aloge("fatal error! realloc fail!");
        }
        pFrm->nBufSize = nNewFrameLen;
    }
    memcpy((char *)pFrm->mpAddr, (char *)pThiz->outBuff, pThiz->outBuffDataRemainLen);
    if (bDrainFlag)
    {
        alogd("drainFlag is set, [%d]bytes input pcm data is not prcoessed and output", pThiz->inBuffDataRemainLen);
        memcpy((char *)pFrm->mpAddr + pThiz->outBuffDataRemainLen, (char *)pThiz->inBuff, pThiz->inBuffDataRemainLen);
        pThiz->inBuffDataRemainLen = 0;
    }
    pThiz->outBuffDataRemainLen = 0;
    if (nNewFrameLen <= pFrm->mLen)
    {
        //alogd("outputPcmLen[%d] <= inputPcmLen[%d]", nNewFrameLen, pFrm->mLen);
    }
    else
    {
        //alogd("outputPcmLen[%d] > inputPcmLen[%d]", nNewFrameLen, pFrm->mLen);
    }
    pFrm->mLen = nNewFrameLen;

    return rc;
}

/**
  clear all internal input pcm data.
*/
static int AOWebRtcAgcClearData(AOAgc *pBase)
{
    AOWebRtcAgc *pThiz = (AOWebRtcAgc*)pBase;
    if (pThiz->inBuffDataRemainLen > 0)
    {
        alogd("Be careful! clear [%d]bytes inputPcm", pThiz->inBuffDataRemainLen);
        pThiz->inBuffDataRemainLen = 0;
    }
    if (pThiz->outBuffDataRemainLen > 0)
    {
        aloge("fatal error! outPcm should be empty! clear [%d]bytes outPcm", pThiz->outBuffDataRemainLen);
        pThiz->outBuffDataRemainLen = 0;
    }
    return 0;
}

static int AOWebRtcAgcGetInputPcmData(AOAgc *pBase, char **ppInputPcmData, int *pInputPcmDataLen)
{
    AOWebRtcAgc *pThiz = (AOWebRtcAgc*)pBase;
    *ppInputPcmData = (char *)pThiz->inBuff;
    *pInputPcmDataLen = pThiz->inBuffDataRemainLen;
    if (pThiz->outBuffDataRemainLen != 0)
    {
        aloge("fatal error! why agc out remain dataLen[%d] != 0?", pThiz->outBuffDataRemainLen);
    }
    return 0;
}

static int AOWebRtcAgcUpdateConfig(AOAgc *pBase, AGC_FLOAT_CONFIG_S *pAgcConfig)
{
    AOWebRtcAgc *pThiz = (AOWebRtcAgc*)pBase;
    pThiz->agcConfig.targetLevelDbfs = -pAgcConfig->fTargetDb;
    pThiz->agcConfig.compressionGaindB = pAgcConfig->fMaxGainDb;
    pThiz->agcConfig.limiterEnable = kAgcTrue;
    int ret = aw_WebRtcAgc_set_config(pThiz->agcInst, pThiz->agcConfig);
    if (ret != 0)
    {
        aloge("fatal error! webRtcAgc set config fail[%d]", ret);
    }
    return ret;
}

static void DeleteAOWebRtcAgc(AOAgc *pBase)
{
    int ret;
    AOWebRtcAgc *pThiz = (AOWebRtcAgc *)pBase;
    if(NULL != pThiz->agcInst)
    {
        ret = aw_WebRtcAgc_Free(pThiz->agcInst);
        if(ret!=0)
        {
            aloge("fatal error! agc free failed");
        }
        pThiz->agcInst = NULL;
    }
    if(pThiz->outBuff != NULL)
    {
        free(pThiz->outBuff);
        pThiz->outBuff = NULL;
    }
    if(NULL != pThiz->inBuff)
    {
        free(pThiz->inBuff);
        pThiz->inBuff = NULL;
    }
  #ifdef AI_HW_AGC_DEBUG_EN
    if(pThiz->tmp_pcm_fp_in != NULL)
    {
        fclose(pThiz->tmp_pcm_fp_in);
        pThiz->tmp_pcm_fp_in = NULL;
    }
    if(pThiz->tmp_pcm_fp_out != NULL)
    {
        fclose(pThiz->tmp_pcm_fp_out);
        pThiz->tmp_pcm_fp_out = NULL;
    }
  #endif
    free(pThiz);
}

/**
  create WebRtcAgc context.

  @param nFrameLen
    unit: byte. audio frame bytes number.
*/
AOWebRtcAgc* CreateAOWebRtcAgc(int nSampleRate, int nChnNum, int nBitWidth, int nFrameLen,
    AGC_FLOAT_CONFIG_S *pAgcConfig)
{
    int ret;
    AOWebRtcAgc *pThiz = (AOWebRtcAgc*)calloc(1, sizeof(AOWebRtcAgc));
    if(NULL == pThiz)
    {
        aloge("fatal error! malloc fail");
    }
    if(nChnNum != 1)
    {
//        aloge("fatal error! WebRtcAgc only support one channel! curChnNum=%d", nChnNum);
//        free(pThiz);
//        return NULL;
        alogw("Be careful! WebRtcAgc only support one channel! curChnNum=%d, have a try", nChnNum);
    }
    pThiz->mBase.mSampleRate = nSampleRate;
    pThiz->mBase.mChnNum = nChnNum;
    pThiz->mBase.mBitWidth = nBitWidth;
    pThiz->mBase.mpAOAgcProcess = AOWebRtcAgcProcess;
    pThiz->mBase.mpAOAgcClearData = AOWebRtcAgcClearData;
    pThiz->mBase.mpAOAgcGetInputPcmData = AOWebRtcAgcGetInputPcmData;
    pThiz->mBase.mpAOAgcUpdateConfig = AOWebRtcAgcUpdateConfig;
    pThiz->mBase.mpAOAgcDelete = DeleteAOWebRtcAgc;

    int nMinFrameLen = nChnNum*nBitWidth/16*1024;
    if (nFrameLen < nMinFrameLen)
    {
        nFrameLen = nMinFrameLen;
    }
    //int nChnLen = nFrameLen/nChnNum;

    ret = aw_WebRtcAgc_Create(&pThiz->agcInst);
    if(ret != 0)
    {
        aloge("fatal error! WebRtcAgc create fail[%d]", ret);
    }

    int minLevel = 0;
    int maxLevel = 255;
    int agcMode = kAgcModeAdaptiveAnalog;
    ret = aw_WebRtcAgc_Init(pThiz->agcInst, minLevel, maxLevel, agcMode, nSampleRate);
    if(ret != 0)
    {
        aloge("fatal error! WebRtcAgc init fail[%d]", ret);
    }

    memset(&pThiz->agcConfig, 0x0, sizeof(WebRtcAgc_config_t));
    pThiz->agcConfig.targetLevelDbfs = -pAgcConfig->fTargetDb;
    pThiz->agcConfig.compressionGaindB = pAgcConfig->fMaxGainDb;
    pThiz->agcConfig.limiterEnable = kAgcTrue;
    aw_WebRtcAgc_set_config(pThiz->agcInst, pThiz->agcConfig);

    pThiz->inBuff = (short *)malloc(nFrameLen*2);
    if(NULL == pThiz->inBuff)
    {
        aloge("fatal error! malloc fail:%d", nFrameLen*2);
    }
    pThiz->inBuffLen = nFrameLen*2;
    pThiz->inBuffDataRemainLen = 0;
    pThiz->outBuff = (short *)malloc(nFrameLen*2);
    if(NULL == pThiz->outBuff)
    {
        aloge("fatal error! malloc fail:%d", nFrameLen*2);
    }
    pThiz->outBuffLen = nFrameLen*2;
    pThiz->outBuffDataRemainLen = 0;

#ifdef AI_HW_AGC_DEBUG_EN
    pThiz->tmp_pcm_fp_in = fopen("/mnt/extsd/tmp_in_agc_pcm", "wb");
    pThiz->tmp_pcm_fp_out = fopen("/mnt/extsd/tmp_out_agc_pcm", "wb");
    if(NULL==pThiz->tmp_pcm_fp_in || NULL==pThiz->tmp_pcm_fp_out)
    {
        aloge("fatal error! agc_debug_file_create_failed");
    }
#endif
    return pThiz;
}

