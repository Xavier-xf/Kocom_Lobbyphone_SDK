#define LOG_NDEBUG 0
#define LOG_TAG "LstmAns"
#include <utils/plat_log.h>

#include <stdlib.h>
#include <string.h>

#include <ans_lib.h>
#include <media_common_aio.h>
#include "LstmAns.h"

LstmAnsContext* ConstructLstmAnsContext()
{
    LstmAnsContext *pCtx = (LstmAnsContext*)malloc(sizeof(LstmAnsContext));
    if(NULL == pCtx)
    {
        aloge("fatal error! malloc fail");
    }
    memset(pCtx, 0, sizeof(*pCtx));
    return pCtx;
}
void DestructLstmAnsContext(LstmAnsContext *pCtx)
{
    int ret;
    if(pCtx->ans_int_lstm != NULL && pCtx->ans_state_lstm != NULL)
    {
        ret = Rnn_Process_free(pCtx->ans_state_lstm, pCtx->ans_int_lstm);
        if(0 == ret)
        {
            //aloge("aec_ans_lstm_released");
            pCtx->ans_int_lstm = NULL;
            pCtx->ans_state_lstm = NULL;
        }
        else
        {
            aloge("fatal error! aec_ans_lstm_release_failed");
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
int LstmAnsProcess(void *cookie, AUDIO_FRAME_S *pFrm, const AIO_ATTR_S *pAttr, BOOL bSuspendAns)
{
    int rc;
    int ret;
    if(pAttr->enSoundmode != AUDIO_SOUND_MODE_MONO)
    {
        aloge("fatal error! LstmAns only can process one channel, not support soundmode[%d]", pAttr->enSoundmode);
        return 0;
    }
    LstmAnsContext *pCtx = (LstmAnsContext*)cookie;
    if(NULL == pCtx->ans_int_lstm)
    {
        ret = Rnn_Process_Create(&pCtx->ans_int_lstm, &pCtx->ans_state_lstm);
        if(NULL == pCtx->ans_int_lstm || ret != 0)
        {
            aloge("fatal error! aec_ans_lstm_instance_create_fail:%d", ret);
            return -1;
        }
        unsigned int nSampleRate = map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pAttr->enSamplerate);
        ret = Rnn_Process_init(pCtx->ans_int_lstm, pCtx->ans_state_lstm, nSampleRate);
        if(0 != ret)
        {
            aloge("fatal error! aec_ans_lstm_init_failed:%d", ret);
        } 
    } 

    if(NULL == pCtx->in_buff_ans)
    {
        pCtx->in_buff_ans = (short *)malloc(pFrm->mLen*2);
        if(NULL == pCtx->in_buff_ans)
        {
            aloge("fatal error! malloc fail:%d", pFrm->mLen*2);
        }
        pCtx->in_buff_len_ans = pFrm->mLen*2;
        pCtx->in_buff_data_remain_len_ans = 0;
    }
    else
    {
        if(pCtx->in_buff_len_ans != pFrm->mLen*2)
        {
            aloge("fatal error! why ans in buf len wrong?[%d != %d*2]", pCtx->in_buff_len_ans, pFrm->mLen);
        }
    }

    if(NULL == pCtx->out_buff_ans)
    {
        pCtx->out_buff_ans = (short *)malloc(pFrm->mLen*2);
        if(NULL == pCtx->out_buff_ans)
        {
            aloge("fatal error! malloc fail:%d", pFrm->mLen*2);
        }
        pCtx->out_buff_len_ans = pFrm->mLen*2;
        pCtx->out_buff_data_remain_len_ans = 0;
    }
    else
    {
        if(pCtx->out_buff_len_ans != pFrm->mLen*2)
        {
            aloge("fatal error! why ans out buf len wrong?[%d != %d*2]", pCtx->out_buff_len_ans, pFrm->mLen);
        }
    }
        
    if(NULL == pCtx->tmpBuf_ans)
    {
        pCtx->tmpBuf_ans = (short*)malloc(pFrm->mLen*2);
        if(NULL == pCtx->tmpBuf_ans)
        {
            aloge("fatal error! malloc fail:%d", pFrm->mLen*2);
        }
        pCtx->tmpBufLen_ans = pFrm->mLen*2;
    }
    else
    {
        if(pCtx->tmpBufLen_ans != pFrm->mLen*2)
        {
            aloge("fatal error! why ans out tmp buf len wrong?[%d != %d*2]", pCtx->tmpBufLen_ans, pFrm->mLen);
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
        aloge("fatal error! in_buff_over_flow:%d-%d-%d", pCtx->in_buff_data_remain_len_ans, pCtx->in_buff_len_ans, pFrm->mLen);
    }

    int frm_size = 160;         // 160 samples as one unit processed by aec library     
    short tmp_near_buffer[160];
    short *near_frm_ptr = (short *)pCtx->in_buff_ans; //for in_buff_ans
    short *processed_frm_ptr = (short *)pCtx->tmpBuf_ans; //for tmpBuf_ans, out_buff_data

    int left = pCtx->in_buff_data_remain_len_ans / sizeof(short);

    // start to process
    while(left >= frm_size)
    {
        if(FALSE == bSuspendAns)
        {
            memcpy((char *)tmp_near_buffer, (char *)near_frm_ptr, frm_size*sizeof(short));
            Lstm_process_frame(pCtx->ans_int_lstm, pCtx->ans_state_lstm[0], processed_frm_ptr, tmp_near_buffer);
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
        aloge("fatal error! ans_out_buff_over_flow:%d-%d-%d", pCtx->out_buff_data_remain_len_ans, pCtx->out_buff_len_ans, out_offset);
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

        if(pFrm->mLen != pAttr->mPtNumPerFrm*sizeof(short))
        {
            aloge("fatal error! ans_frameLen wrong:%d != %d", pAttr->mPtNumPerFrm*sizeof(short), pFrm->mLen);
        }
        if(pCtx->out_buff_data_remain_len_ans > pFrm->mLen)
        {
            aloge("fatal error! ans_out_buff_data too long:%d-%d", pFrm->mLen, pCtx->out_buff_data_remain_len_ans);
        }
        memmove((char *)pCtx->out_buff_ans, ((char *)pCtx->out_buff_ans + pFrm->mLen), pCtx->out_buff_data_remain_len_ans);
        rc = 0;
    }
    else
    {
        rc = 1;
    }
    return rc;
}

