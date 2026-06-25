#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "media/LIBRARY/ans_lib/include/ans_lib.h"
#include "include/uac_ans.h"
#include "../utils/debug/include/debug.h"

extern void aw_WebRtcSpl_AnalysisQMF(const int16_t* in_data,
                           int16_t* low_band,
                           int16_t* high_band,
                           int32_t* filter_state1,
                           int32_t* filter_state2);
extern void aw_WebRtcSpl_SynthesisQMF(const int16_t* low_band,
                            const int16_t* high_band,
                            int16_t* out_data,
                            int32_t* filter_state1,
                            int32_t* filter_state2);

WebRtcAnsContext* ConstructWebRtcAnsContext()
{
    WebRtcAnsContext *pCtx = (WebRtcAnsContext*)malloc(sizeof(WebRtcAnsContext));
    if(NULL == pCtx)
    {
        loge("fatal error! malloc fail");
    }
    memset(pCtx, 0, sizeof(*pCtx));
    return pCtx;
}
void DestructWebRtcAnsContext(WebRtcAnsContext *pCtx)
{
    int ret;
    if(pCtx->ans_int != NULL)
    {
        ret = aw_WebRtcNs_Free(pCtx->ans_int);
        if(0 == ret)
        {
            //alogd("aec_ans_released");
            pCtx->ans_int = NULL;
        }
        else
        {
            loge("fatal error! aec_ans_release_failed");
        }
    }

    if(pCtx->tmpBuf_ans != NULL)
    {
        free(pCtx->tmpBuf_ans);
        pCtx->tmpBuf_ans = NULL;
    }
    if(NULL != pCtx->out_buff_ans)
    {
        free(pCtx->out_buff_ans);
        pCtx->out_buff_ans = NULL;
    }
    if(NULL != pCtx->in_buff_ans)
    {
        free(pCtx->in_buff_ans);
        pCtx->in_buff_ans = NULL;
    }
    free(pCtx);
}

/**
  implement of AnsProcessFuncType.
*/
int WebRtcAnsProcess(
    void *cookie,
    UAC_ANS_FRAME_S *pFrm,
    unsigned int samplerate,
    unsigned int bits_perframe,
    int ans_mode,
    int bSuspendAns)
{
    int rc;
    int ret;
    /*if(pAttr->enSoundmode != AUDIO_SOUND_MODE_MONO)
    {
        loge("fatal error! webRtcAns only can process one channel, not support soundmode[%d]", pAttr->enSoundmode);
        return 0;
    }*/
    WebRtcAnsContext *pCtx = (WebRtcAnsContext*)cookie;
    if(NULL == pCtx->ans_int)
    {
        ret = aw_WebRtcNs_Create(&pCtx->ans_int);  // instance
        if(NULL == pCtx->ans_int || 0 != ret)
        {
            loge("fatal error! aec_ans_instance_create_fail:%d", ret);
            return -1;
        }
        int nSampleRate = samplerate;
        ret = aw_WebRtcNs_Init(pCtx->ans_int, nSampleRate); //32000, pAttr->enSamplerate
        if(0 != ret)
        {
            loge("fatal error! aec_ans_init_failed:%d",ret);
        }

        ret = aw_WebRtcNs_set_policy(pCtx->ans_int, ans_mode);
        if(0 != ret)
        {
            loge("fatal error! aec_ans_cfg_failed");
        }

        memset(pCtx->filter_state1,0,sizeof(pCtx->filter_state1));
        memset(pCtx->filter_state12,0,sizeof(pCtx->filter_state12));
        memset(pCtx->Synthesis_state1,0,sizeof(pCtx->Synthesis_state1));
        memset(pCtx->Synthesis_state12,0,sizeof(pCtx->Synthesis_state12));
    }

    if(NULL == pCtx->in_buff_ans)
    {
        pCtx->in_buff_ans = (short *)malloc(pFrm->mLen*2);
        if(NULL == pCtx->in_buff_ans)
        {
            loge("fatal error! malloc fail:%d", pFrm->mLen*2);
        }
        pCtx->in_buff_len_ans = pFrm->mLen*2;
        pCtx->in_buff_data_remain_len_ans = 0;
    }
    else
    {
        if(pCtx->in_buff_len_ans != pFrm->mLen*2)
        {
            loge("fatal error! why ans in buf len wrong?[%d != %d*2]", pCtx->in_buff_len_ans, pFrm->mLen);
        }
    }

    if(NULL == pCtx->out_buff_ans)
    {
        pCtx->out_buff_ans = (short *)malloc(pFrm->mLen*2);
        if(NULL == pCtx->out_buff_ans)
        {
            loge("fatal error! malloc fail:%d", pFrm->mLen*2);
        }
        pCtx->out_buff_len_ans = pFrm->mLen*2;
        pCtx->out_buff_data_remain_len_ans = 0;
    }
    else
    {
        if(pCtx->out_buff_len_ans != pFrm->mLen*2)
        {
            loge("fatal error! why ans out buf len wrong?[%d != %d*2]", pCtx->out_buff_len_ans, pFrm->mLen);
        }
    }

    if(NULL == pCtx->tmpBuf_ans)
    {
        pCtx->tmpBuf_ans = (short*)malloc(pFrm->mLen*2);
        if(NULL == pCtx->tmpBuf_ans)
        {
            loge("fatal error! malloc fail:%d", pFrm->mLen*2);
        }
        pCtx->tmpBufLen_ans = pFrm->mLen*2;
    }
    else
    {
        if(pCtx->tmpBufLen_ans != pFrm->mLen*2)
        {
            loge("fatal error! why ans out tmp buf len wrong?[%d != %d*2]", pCtx->tmpBufLen_ans, pFrm->mLen);
        }
    }
    //memset(pCtx->tmpBuf_ans, 0, pCtx->tmpBufLen_ans);


    // move data in near buffer and reference buffer to internal buffer for conjunction with remaining data for last process.
    if(pCtx->in_buff_data_remain_len_ans + pFrm->mLen <= pCtx->in_buff_len_ans)
    {
        memcpy((char*)pCtx->in_buff_ans + pCtx->in_buff_data_remain_len_ans, (char*)pFrm->mpAddr, pFrm->mLen);
        pCtx->in_buff_data_remain_len_ans += pFrm->mLen;
    }
    else
    {
        loge("fatal error! in_buff_over_flow:%d-%d-%d", pCtx->in_buff_data_remain_len_ans, pCtx->in_buff_len_ans, pFrm->mLen);
    }

    int frm_size = 320;         // 160 samples as one unit processed by aec library, we use two units.
    short tmp_near_buffer[320]; //for in_buff_ans

    short tmp_ans_shInL[160] = {0};
    short tmp_ans_shInH[160] = {0};
    short tmp_ans_shOutL[160] = {0};
    short tmp_ans_shOutH[160] = {0};
    short *near_frm_ptr = (short *)pCtx->in_buff_ans; //for in_buff_ans
    short *processed_frm_ptr = (short *)pCtx->tmpBuf_ans; //for tmpBuf_ans, out_buff_data

    int left = pCtx->in_buff_data_remain_len_ans / sizeof(short);

    // start to process
    while(left >= frm_size)
    {
        if(!bSuspendAns)
        {
            memcpy((char *)tmp_near_buffer, (char *)near_frm_ptr, frm_size*sizeof(short));
            aw_WebRtcSpl_AnalysisQMF(tmp_near_buffer, tmp_ans_shInL, tmp_ans_shInH, pCtx->filter_state1, pCtx->filter_state12);
            if (0 == aw_WebRtcNs_Process(pCtx->ans_int, tmp_ans_shInL, tmp_ans_shInH, tmp_ans_shOutL, tmp_ans_shOutH))
            {
                aw_WebRtcSpl_SynthesisQMF(tmp_ans_shOutL, tmp_ans_shOutH, processed_frm_ptr, pCtx->Synthesis_state1, pCtx->Synthesis_state12);
            }
        }
        else
        {
            memcpy((char*)processed_frm_ptr, (char *)near_frm_ptr, frm_size*sizeof(short));
        }

        near_frm_ptr += frm_size;
        processed_frm_ptr += frm_size;
        left -= frm_size;

        pCtx->in_buff_data_remain_len_ans -= frm_size*sizeof(short);
    }

    // move remaining data in internal buffer to the beginning of the buffer
    if(left > 0)
    {
        memmove((char*)pCtx->in_buff_ans, (char*)near_frm_ptr, pCtx->in_buff_data_remain_len_ans);
    }

    unsigned int out_offset = (unsigned int)processed_frm_ptr - (unsigned int)pCtx->tmpBuf_ans;

    if(out_offset + pCtx->out_buff_data_remain_len_ans > pCtx->out_buff_len_ans)
    {
        loge("fatal error! ans_out_buff_over_flow:%d-%d-%d", pCtx->out_buff_data_remain_len_ans, pCtx->out_buff_len_ans, out_offset);
    }
    else
    {
        memcpy((char *)pCtx->out_buff_ans + pCtx->out_buff_data_remain_len_ans, (char *)pCtx->tmpBuf_ans, out_offset);
        pCtx->out_buff_data_remain_len_ans += out_offset;
    }

    // fetch one valid output frame from output internal buffer, the length of valid frame must equal to chunsize.
    if(pCtx->out_buff_data_remain_len_ans >= pFrm->mLen)
    {
        memcpy((char *)pFrm->mpAddr, (char *)pCtx->out_buff_ans, pFrm->mLen);
        pCtx->out_buff_data_remain_len_ans -= pFrm->mLen;

        if(pFrm->mLen != bits_perframe*sizeof(short))
        {
            loge("fatal error! ans_frameLen wrong:%d != %d", bits_perframe*sizeof(short), pFrm->mLen);
        }
        if(pCtx->out_buff_data_remain_len_ans > pFrm->mLen)
        {
            loge("fatal error! ans_out_buff_data too long:%d-%d", pFrm->mLen, pCtx->out_buff_data_remain_len_ans);
        }
        if(pCtx->out_buff_data_remain_len_ans > 0)
        {
            memmove((char *)pCtx->out_buff_ans, ((char *)pCtx->out_buff_ans + pFrm->mLen), pCtx->out_buff_data_remain_len_ans);
        }
        rc = 0;
    }
    else
    {
        rc = 1;
    }
    return rc;
}
