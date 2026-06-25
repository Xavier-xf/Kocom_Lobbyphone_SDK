#define LOG_NDEBUG 0
#define LOG_TAG "WebRtcAec"
#include <utils/plat_log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <media_common_aio.h>
#include <aec_lib.h>
#include "WebRtcAec.h"

//#define AI_HW_AEC_DEBUG_EN

WebRtcAecContext* ConstructWebRtcAecContext(AIO_ATTR_S *pAttr, int nFrameBytes)
{
    WebRtcAecContext *pCtx = (WebRtcAecContext*)malloc(sizeof(WebRtcAecContext));
    if(NULL == pCtx)
    {
        aloge("fatal error! malloc fail");
    }
    memset(pCtx, 0, sizeof(*pCtx));

    int nChnNum = 0;
    int nMicChnNum = 0;
    int nRefChnNum = 0;
    AUDIO_SOUND_MODE_E eInitialSoundMode = judgeInitialSoundModeOfAudioInput(pAttr);
    nChnNum = judgeAudioChnNumBySoundMode(eInitialSoundMode, &nMicChnNum, &nRefChnNum);
    int nSampleRate = (int)map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pAttr->enSamplerate);
    int nChnLen = nFrameBytes/nChnNum;
    
    alogv("aec_to_init:%d", nSampleRate);
    int ret = aw_WebRtcAec_Create(&pCtx->aecmInst);
    if(NULL == pCtx->aecmInst || 0 != ret)
    {
        aloge("fatal error! aec_instance_create_fail:%d", ret);
    }
    ret = aw_WebRtcAec_Init(pCtx->aecmInst, nSampleRate, nSampleRate);
    if(0 != ret)
    {
        aloge("fatal error! aec_init_failed:%d", ret);
    }
    AecConfig config;
    memset(&config,0,sizeof(AecConfig));
    config.nlpMode = pAttr->mAecNlpMode;
    ret = aw_WebRtcAec_set_config(pCtx->aecmInst, config);
    if(0 != ret)
    {
        aloge("fatal error! aec_cfg_failed:%d", ret);
    }

    if(NULL == pCtx->near_buff)
    {
        pCtx->near_buff = (short *)malloc(nChnLen*2);
        if(NULL == pCtx->near_buff)
        {
            aloge("fatal error! malloc fail:%d", nChnLen*2);
        }
        pCtx->near_buff_len = nChnLen*2;
        pCtx->near_buff_data_remain_len = 0;
    }
    else
    {
        if(pCtx->near_buff_len != nChnLen*2)
        {
            aloge("fatal error! why aec near buf len wrong?[%d != %d*2]", pCtx->near_buff_len, nChnLen);
        }
    }
    if(NULL == pCtx->ref_buff)
    {
        pCtx->ref_buff = (short *)malloc(nChnLen*2);
        if(NULL == pCtx->ref_buff)
        {
            aloge("fatal error! malloc fail:%d", nChnLen*2);
        }
        pCtx->ref_buff_len = nChnLen*2;
        pCtx->ref_buff_data_remain_len = 0;
    }
    else
    {
        if(pCtx->ref_buff_len != nChnLen*2)
        {
            aloge("fatal error! why aec ref buf len wrong?[%d != %d*2]", pCtx->ref_buff_len, nChnLen);
        }
    }
    if(NULL == pCtx->out_buff)
    {
        pCtx->out_buff = (short *)malloc(nChnLen*2);
        if(NULL == pCtx->out_buff)
        {
            aloge("fatal error! malloc fail:%d", nChnLen*2);
        }
        pCtx->out_buff_len = nChnLen*2;
        pCtx->out_buff_data_remain_len = 0;
    }
    else
    {
        if(pCtx->out_buff_len != nChnLen*2)
        {
            aloge("fatal error! why aec out buf len wrong?[%d != %d*2]", pCtx->out_buff_len, nChnLen);
        }
    }
    pCtx->mRefDelayMs = pAttr->aec_delay_ms;
    pCtx->mbRefSkipFlag = false;

#ifdef AI_HW_AEC_DEBUG_EN
    pCtx->tmp_pcm_fp_in = fopen("/mnt/extsd/tmp_in_ai_pcm", "wb"); 
    pCtx->tmp_pcm_fp_ref = fopen("/mnt/extsd/tmp_ref_ai_pcm", "wb");
    pCtx->tmp_pcm_fp_out = fopen("/mnt/extsd/tmp_out_ai_pcm", "wb");
    if(NULL==pCtx->tmp_pcm_fp_in || NULL==pCtx->tmp_pcm_fp_ref || NULL==pCtx->tmp_pcm_fp_out)
    {
        aloge("fatal error! aec_debug_file_create_failed");
    }
#endif
    return pCtx;
}
void DestructWebRtcAecContext(WebRtcAecContext *pCtx)
{
    int ret;

    if(NULL != pCtx->aecmInst)
    {
        ret = aw_WebRtcAec_Free(pCtx->aecmInst);
        if(0 == ret)
        {
            //aloge("aec_released");
            pCtx->aecmInst = NULL;
        }
        else
        {
            aloge("fatal error! aec_free_failed");
        }
    }
//    if(NULL != pCtx->tmpBuf)
//    {
//        free(pCtx->tmpBuf);
//        pCtx->tmpBuf = NULL;
//    }
    if(NULL != pCtx->out_buff)
    {
        free(pCtx->out_buff);
        pCtx->out_buff = NULL;
    }
    if(NULL != pCtx->ref_buff)
    {
        free(pCtx->ref_buff);
        pCtx->ref_buff = NULL;
    }
    if(NULL != pCtx->near_buff)
    {
        free(pCtx->near_buff);
        pCtx->near_buff = NULL;
    }
  #ifdef AI_HW_AEC_DEBUG_EN
    if(NULL != pCtx->tmp_pcm_fp_in)
    {
        fclose(pCtx->tmp_pcm_fp_in);
        pCtx->tmp_pcm_fp_in = NULL;
    }
    if(NULL != pCtx->tmp_pcm_fp_ref)
    {
        fclose(pCtx->tmp_pcm_fp_ref);
        pCtx->tmp_pcm_fp_ref = NULL;
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
  implement of AecProcessFuncType.
*/
int WebRtcAecProcess(void *cookie, AUDIO_FRAME_S *pFrm, BOOL bSuspendAec)
{
    int rc;
    int ret;

    //webrtc only supports 1chn1ref.
    if(pFrm->mSoundmode != AUDIO_SOUND_MODE_AW_1Chn1Ref)
    {
        aloge("fatal error! WebRtcAec invalid sound mode:%d", pFrm->mSoundmode);
        return -1;
    }
    int nSampleRate = (int)map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pFrm->mSamplerate);
    int nBitWidth = (int)map_AUDIO_BIT_WIDTH_E_to_BitWidth(pFrm->mBitwidth);
    if(nBitWidth != 16)
    {
        aloge("fatal error! bitWidth[%d] must be 16!", nBitWidth);
    }
    int nMicChnNum = 0;
    int nRefChnNum = 0;
    int nChnNum = judgeAudioChnNumBySoundMode(pFrm->mSoundmode, &nMicChnNum, &nRefChnNum);
    int nChnLen = pFrm->mLen/nChnNum; //unit: bytes
    WebRtcAecContext *pCtx = (WebRtcAecContext*)cookie;

    // move data in near buffer and reference buffer to internal buffer for conjunction with remaining data for last process. 
    if(pCtx->near_buff_data_remain_len != pCtx->ref_buff_data_remain_len)
    {
        if(!(pCtx->mRefDelayMs > 0 && pCtx->mbRefSkipFlag))
        {
            aloge("fatal error! capture and ref remain data: %d!=%d bytes", pCtx->near_buff_data_remain_len, pCtx->ref_buff_data_remain_len);
        }
    }
    if(pCtx->near_buff_data_remain_len + nChnLen <= pCtx->near_buff_len)
    {
        short *pNearStart = (short*)((char*)pCtx->near_buff + pCtx->near_buff_data_remain_len);
        short *pRefStart = (short*)((char*)pCtx->ref_buff + pCtx->ref_buff_data_remain_len);
        short *pAudFrameStart = (short*)pFrm->mpAddr;
        int nFrameSize = nChnLen/(nBitWidth/8); //one frame contain the number of alsaFrames.
        for(int i = 0; i < nFrameSize; i++)
        {
            //assume sample length is 16bit.
            pNearStart[i] = pAudFrameStart[2*i];
            pRefStart[i] = pAudFrameStart[2*i + 1];
        }
        pCtx->near_buff_data_remain_len += nFrameSize*(nBitWidth/8);
        pCtx->ref_buff_data_remain_len += nFrameSize*(nBitWidth/8);
    }
    else
    {
        aloge("fatal error! _near_buff_over_flow:%d-%d-%d-%d", pCtx->near_buff_data_remain_len, pCtx->near_buff_len, nChnLen, pFrm->mLen);
    }
    if(pCtx->mRefDelayMs > 0)
    {
        if(false == pCtx->mbRefSkipFlag)
        {
            int nRefAlsaFrameBytes = nBitWidth/8*nRefChnNum;
            int nSkipRefAlsaFrames = (int)((int64_t)nSampleRate*pCtx->mRefDelayMs/1000);
            int nSkipRefBytes = nRefAlsaFrameBytes * nSkipRefAlsaFrames;
            if(nSkipRefBytes <= pCtx->ref_buff_data_remain_len)
            {
                memmove((char*)pCtx->ref_buff, (char*)pCtx->ref_buff + nSkipRefBytes, pCtx->ref_buff_data_remain_len - nSkipRefBytes);
                pCtx->ref_buff_data_remain_len -= nSkipRefBytes;
                alogd("aec refDelay:[%d]ms, skipRefBytes:[%d]bytes", pCtx->mRefDelayMs, nSkipRefBytes);
            }
            else
            {
                aloge("fatal error! aec refDelay:[%d]ms, skipRefBytes:[%d]>[%d], too many! ignore", pCtx->mRefDelayMs, nSkipRefBytes,
                    pCtx->ref_buff_data_remain_len);
            }
            pCtx->mbRefSkipFlag = true;
        }
    }

    int frm_size = 160;         // 160 samples as one unit processed by aec library
    //short tmp_near_buffer[160]; //captureMic data
    //short tmp_far_buffer[160];  //ref data(i.d. daudio0 data)

    short *near_frm_ptr = (short *)pCtx->near_buff;
    short *ref_frm_ptr = (short *)pCtx->ref_buff;
    //short *processed_frm_ptr = (short *)pCtx->tmpBuf;
    short *processed_frm_ptr = (short *)((char*)pCtx->out_buff+pCtx->out_buff_data_remain_len);

    int size = pCtx->near_buff_data_remain_len; //bytes
    if(pCtx->ref_buff_data_remain_len < pCtx->near_buff_data_remain_len)
    {
        size = pCtx->ref_buff_data_remain_len;
    }
    int left = size / sizeof(short); //sample number.

    // start to process
    while(left >= frm_size)
    {
        if (FALSE == bSuspendAec)
        {
            ret = aw_WebRtcAec_BufferFarend(pCtx->aecmInst, ref_frm_ptr, frm_size);
            if(0 != ret)
            {
                aloge("fatal error! aec_insert_far_data_failed:%d-%d", ret, ((aecpc_t*)pCtx->aecmInst)->lastError);
            }

            ret = aw_WebRtcAec_Process(pCtx->aecmInst, near_frm_ptr, NULL, processed_frm_ptr, NULL, frm_size, 0, 0);
            if(0 != ret)
            {
                aloge("aec_process_failed:%d-%d", ret, ((aecpc_t*)pCtx->aecmInst)->lastError);
            }
        }
        else
        {
            memcpy(processed_frm_ptr, near_frm_ptr, frm_size*sizeof(short));
        }

      #ifdef AI_HW_AEC_DEBUG_EN
        if(NULL != pCtx->tmp_pcm_fp_in)
        {
            fwrite(near_frm_ptr, 1, frm_size*sizeof(short), pCtx->tmp_pcm_fp_in);
            pCtx->tmp_pcm_in_size += frm_size*sizeof(short);
        }
        
        if(NULL != pCtx->tmp_pcm_fp_ref)
        {
            fwrite(ref_frm_ptr, 1, frm_size*sizeof(short), pCtx->tmp_pcm_fp_ref);
            pCtx->tmp_pcm_ref_size += frm_size*sizeof(short);
        }

//        aloge("zjx_tbin:%d-%d",pCap->tmp_pcm_in_size,pCap->tmp_pcm_ref_size);
      #endif

        near_frm_ptr += frm_size;
        ref_frm_ptr += frm_size;
        processed_frm_ptr += frm_size;
        left -= frm_size;

        pCtx->near_buff_data_remain_len -= frm_size*sizeof(short);
        pCtx->ref_buff_data_remain_len -= frm_size*sizeof(short);
        pCtx->out_buff_data_remain_len += frm_size*(nBitWidth/8);
    }

    // move remaining data in internal buffer to the beginning of the buffer 
    if(left > 0)
    {
        memmove((char*)pCtx->near_buff, (char*)near_frm_ptr, pCtx->near_buff_data_remain_len);
        memmove((char*)pCtx->ref_buff, (char*)ref_frm_ptr, pCtx->ref_buff_data_remain_len);
    }

    // fetch one valid output frame from output internal buffer, the length of valid frame must equal to chunksize.
    if(pCtx->out_buff_data_remain_len >= nChnLen)
    {
        memcpy((char *)pFrm->mpAddr, (char *)pCtx->out_buff, nChnLen);
        pFrm->mLen = nChnLen;
        pCtx->out_buff_data_remain_len -= nChnLen;
        pFrm->mSoundmode = AUDIO_SOUND_MODE_MONO;

      #ifdef AI_HW_AEC_DEBUG_EN
        if(NULL != pCtx->tmp_pcm_fp_out)
        {
            fwrite(pFrm->mpAddr, 1, pFrm->mLen, pCtx->tmp_pcm_fp_out);
            pCtx->tmp_pcm_out_size += pFrm->mLen;
        }
//        aloge("zjx_tbo:%d-%d-%d",pCap->tmp_pcm_out_size,pFrm->mLen,pCap->mCfg.chunkSize*sizeof(short));
      #endif

        if(pCtx->out_buff_data_remain_len > nChnLen)
        {
            aloge("fatal error! aec_out_buff_data too long:%d-%d", nChnLen, pCtx->out_buff_data_remain_len);
        }
        memmove((char *)pCtx->out_buff, ((char *)pCtx->out_buff+nChnLen), pCtx->out_buff_data_remain_len);
        rc = 0;
    }
    else
    {
        rc = 1;
    }
    return rc;
}

