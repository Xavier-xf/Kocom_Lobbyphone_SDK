#define LOG_NDEBUG 0
#define LOG_TAG "WebRtcAec"
#include <utils/plat_log.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <SystemBase.h>
#include <mm_comm_aio.h>
#include <media_common_aio.h>
#include <uvoice_ecnr_api.h>
#include <uvoice_license.h>

#include "UvoiceAec.h"

//#define AI_HW_AEC_DEBUG_EN

static UvEcnr_InputMode judgeUvEcnr_InputMode(AUDIO_SOUND_MODE_E eSoundmode)
{
    UvEcnr_InputMode eUvMode;
    if(AUDIO_SOUND_MODE_AW_1Chn1Ref == eSoundmode)
    {
        eUvMode = uvoice_ecnr_mode_1mic1ref;
    }
    else if(AUDIO_SOUND_MODE_AW_2Chn1Ref == eSoundmode)
    {
        eUvMode = uvoice_ecnr_mode_2mic1ref;
    }
    else
    {
        aloge("fatal error! not support sound mode:%d", eSoundmode);
        eUvMode = uvoice_ecnr_mode_1mic1ref;
    }
    return eUvMode;
}

static AUDIO_DEV sgUvAudioDevId;
static AudioDevCallbackFuncType sgUvAudioDevCallback;
static void *sgUvAudioDevCookie;

/**
  get server return string to take it to generate license file.
  shell command is:
  curl -k -s --data-binary $body https://srv01.51asr.com:8007/asrsn_active2
*/
static const char* uvoice_auth_cb(const char* body)
{
    //alogd("Body is:%s", body);
    UvoiceServerHandshake stHandshake;
    memset(&stHandshake, 0, sizeof(stHandshake));
    stHandshake.pPostBody = body;
    ERRORTYPE ret = sgUvAudioDevCallback(sgUvAudioDevCookie, sgUvAudioDevId, AudioDevEvent_GetUvoiceServerResponse, 0, (void*)&stHandshake);
    if(SUCCESS == ret)
    {
        return strdup(stHandshake.response);
    }
    else
    {
        aloge("fatal error! get uvoice server response fail[0x%x]", ret);
        return "";
    }
}

UvoiceAecContext* ConstructUvoiceAecContext(AIO_ATTR_S *pAttr, int nFrameBytes, AudioDevCallbackFuncType pAudioDevCallback,
    void *pAudioDevCookie, AUDIO_DEV AudioDevId)
{
    sgUvAudioDevId = AudioDevId;
    sgUvAudioDevCallback = pAudioDevCallback;
    sgUvAudioDevCookie = pAudioDevCookie;

    UvoiceLicenseParam stUvLicenseParam;
    memset(&stUvLicenseParam, 0, sizeof(stUvLicenseParam));
    if(pAudioDevCallback)
    {
        pAudioDevCallback(pAudioDevCookie, AudioDevId, AudioDevEvent_GetUvoiceLicenseParam, 0, (void*)&stUvLicenseParam);
    }
    else
    {
        aloge("fatal error! audioDev[%d] is not set callback!", AudioDevId);
    }

    int nChnNum = 0;
    int nMicChnNum = 0;
    int nRefChnNum = 0;
    AUDIO_SOUND_MODE_E eInitialSoundMode = judgeInitialSoundModeOfAudioInput(pAttr);
    nChnNum = judgeAudioChnNumBySoundMode(eInitialSoundMode, &nMicChnNum, &nRefChnNum);
    if(nChnNum > MAX_MICIN_NUM || nMicChnNum > MAX_MICIN_NUM || nRefChnNum > MAX_REFIN_NUM)
    {
        aloge("fatal error! audio channel number overflow, check code! [%d-%d-%d]", nChnNum, nMicChnNum, nRefChnNum);
        return NULL;
    }
    int nSampleRate = (int)map_AUDIO_SAMPLE_RATE_E_to_SampleRate(pAttr->enSamplerate);
    int nChnLen = nFrameBytes/nChnNum;

    int i;
    uvoice_ecnr_status st;
    UvoiceAecContext *pCtx = (UvoiceAecContext*)malloc(sizeof(UvoiceAecContext));
    if(NULL == pCtx)
    {
        aloge("fatal error! malloc fail");
    }
    memset(pCtx, 0, sizeof(*pCtx));

    UvEcnr_InputMode eInputMode = judgeUvEcnr_InputMode(eInitialSoundMode);
    UvEcnr_OutputMode eOutputMode = uvoice_ecnr_mode_phone;
    pCtx->mMicChnNum = nMicChnNum;
    pCtx->mRefChnNum = nRefChnNum;
    pCtx->mChnLen = nChnLen;
    uv_activate_param stParam;
    memset(&stParam, 0, sizeof(stParam));
    stParam.license = stUvLicenseParam.license; //UvoiceLicenseCode;
    stParam.license_path = stUvLicenseParam.license_path; //UvoiceLicenseFilePath;
    stParam.uuid = stUvLicenseParam.uuid; //UvoiceUUID;
    stParam.activate_type = 0;
    stParam.auth_cb = uvoice_auth_cb;
    int64_t tm0 = CDX_GetSysTimeUsMonotonic()/1000;
    pCtx->handle_ecnr = UvEcnr_Create(eInputMode, eOutputMode, nSampleRate, &stParam);
    const char* version = UvEcnr_GetVersion();
    alogd("[UVOICE] ecnr version: %s, license: %s-%s-%s, param:%d-%d-%d", version, stUvLicenseParam.license,
        stUvLicenseParam.license_path, stUvLicenseParam.uuid, eInputMode, eOutputMode, nSampleRate);

    // 不适用云端授权调用API，process在半小时后将返回错误信息，处理输出为全0
    //handle_ecnr = UvEcnr_Create((UvEcnr_InputMode)in_mode,(UvEcnr_OutputMode)out_mode,sample_rate, NULL);
    // 授权失败时也会返回空句柄，注意看LOG
    if (NULL == pCtx->handle_ecnr)
    {
        aloge("fatal error! UvEcnr create fail!");
        free(pCtx);
        return NULL;
    }
    int64_t tm1 = CDX_GetSysTimeUsMonotonic()/1000;
    st = UvEcnr_Init(pCtx->handle_ecnr);
    if(st != UV_ECNR_OK)
    {
        aloge("fatal error! [UVOICE] ECNR init error:%d!", st);
        UvEcnr_Destroy(pCtx->handle_ecnr);
        pCtx->handle_ecnr = NULL;
        free(pCtx);
        return NULL;
    }
    int64_t tm2 = CDX_GetSysTimeUsMonotonic()/1000;
    alogd("UvEncr init cost:[%lld-%lld]ms", tm1-tm0, tm2-tm1);

    //malloc memory to buffer.
    for(i=0; i<nChnNum; i++)
    {
        pCtx->in_buff[i] = (short *)malloc(nChnLen*2);
        if(NULL == pCtx->in_buff[i])
        {
            aloge("fatal error! malloc fail:%d", nChnLen*2);
        }
    }
    pCtx->in_buff_len = nChnLen*2;
    pCtx->in_buff_data_remain_len = 0;
    pCtx->out_buff = (short *)malloc(nChnLen*2);
    if(NULL == pCtx->out_buff)
    {
        aloge("fatal error! malloc fail:%d", nChnLen*2);
    }
    pCtx->out_buff_len = nChnLen*2;
    pCtx->out_buff_data_remain_len = 0;

#ifdef AI_HW_AEC_DEBUG_EN
    pCtx->tmp_pcm_fp_in = fopen("/mnt/extsd/tmp_in_ai.pcm", "wb");
    pCtx->tmp_pcm_fp_out = fopen("/mnt/extsd/tmp_out_ai.pcm", "wb");
    if(NULL==pCtx->tmp_pcm_fp_in || NULL==pCtx->tmp_pcm_fp_out)
    {
        aloge("fatal error! aec_debug_file_create_failed");
    }
#endif
    return pCtx;
}
void DestructUvoiceAecContext(UvoiceAecContext *pCtx)
{
    int ret;
    if(pCtx->audio_duration > 0)
    {
        alogd("total RTF: %.4f (%.2f / %.2f)", pCtx->process_duration/pCtx->audio_duration, pCtx->process_duration, pCtx->audio_duration);
    }

    if(NULL != pCtx->handle_ecnr)
    {
        UvEcnr_Destroy(pCtx->handle_ecnr);
        pCtx->handle_ecnr = NULL;
    }
    for(int i=0; i<MAX_MIXEDIN_NUM; i++)
    {
        if(pCtx->in_buff[i])
        {
            free(pCtx->in_buff[i]);
            pCtx->in_buff[i] = NULL;
        }
    }
    if(NULL != pCtx->out_buff)
    {
        free(pCtx->out_buff);
        pCtx->out_buff = NULL;
    }
  #ifdef AI_HW_AEC_DEBUG_EN
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
  implement of AecProcessFuncType.
*/
int UvoiceAecProcess(void *cookie, AUDIO_FRAME_S *pFrm, BOOL bSuspendAec)
{
    int rc;
    int ret;
    uvoice_ecnr_status st;
    int i;
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
    UvoiceAecContext *pCtx = (UvoiceAecContext*)cookie;
    if(NULL == pCtx->handle_ecnr)
    {
        aloge("fatal error! UvEcnr create fail!");
        return -1;
    }

    if(nMicChnNum != pCtx->mMicChnNum || nRefChnNum != pCtx->mRefChnNum || nChnLen != pCtx->mChnLen)
    {
        aloge("fatal error! audio channel param change:[%d-%d-%d] != [%d-%d-%d]", nMicChnNum, nRefChnNum, nChnLen,
            pCtx->mMicChnNum, pCtx->mRefChnNum, pCtx->mChnLen);
    }

    if(pCtx->in_buff_len != nChnLen*2)
    {
        aloge("fatal error! why aec in buf len wrong?[%d != %d*2]", pCtx->in_buff_len, nChnLen);
    }
    if(pCtx->out_buff_len != nChnLen*2)
    {
        aloge("fatal error! why aec out buf len wrong?[%d != %d*2]", pCtx->out_buff_len, nChnLen);
    }

    // move data in near buffer and reference buffer to internal buffer for conjunction with remaining data for last process. 
    if(pCtx->in_buff_data_remain_len + nChnLen <= pCtx->in_buff_len)
    {
        int nFrameSize = nChnLen/(nBitWidth/8); //one frame contain the number of alsaFrames.
        for(i = 0; i < nFrameSize; i++)
        {
            //assume sample length is 16bit.
            for(int nChnIdx = 0; nChnIdx < nChnNum; nChnIdx++)
            {
                *(short*)((char*)pCtx->in_buff[nChnIdx]+pCtx->in_buff_data_remain_len) = ((short*)pFrm->mpAddr)[nChnNum*i+nChnIdx];
            }
            pCtx->in_buff_data_remain_len+=(nBitWidth/8);
        }
      #ifdef AI_HW_AEC_DEBUG_EN
        if(NULL != pCtx->tmp_pcm_fp_in)
        {
            fwrite(pFrm->mpAddr, 1, pFrm->mLen, pCtx->tmp_pcm_fp_in);
            pCtx->tmp_pcm_in_size += pFrm->mLen;
        }
      #endif
    }
    else
    {
        aloge("fatal error! in_buff_over_flow:%d-%d-%d-%d", pCtx->in_buff_data_remain_len, pCtx->in_buff_len, nChnLen, pFrm->mLen);
    }

    int frm_size = nSampleRate/100; // 10ms as one unit! when sampleRate is 16000, 160 samples as one unit processed by aec library
    int left = pCtx->in_buff_data_remain_len / (nBitWidth/8); //sample number.
    int nProcessedSampleNum = 0;
    while(left >= frm_size)
    {
        if (FALSE == bSuspendAec)
        {
            uv_ecnr_audio_buf audioBuf;
            for(i = 0; i < nMicChnNum; i++)
            {
                audioBuf.audioin[i] = pCtx->in_buff[i] + nProcessedSampleNum;
            }
            for(i = 0; i < nRefChnNum; i++)
            {
                audioBuf.audioref[i] = pCtx->in_buff[i+nMicChnNum] + nProcessedSampleNum;
            }
            short *pOutAudio = NULL;

            int64_t tm0 = CDX_GetSysTimeUsMonotonic();
            pCtx->audio_duration += (float)frm_size/nSampleRate;
            st = UvEcnr_Process(pCtx->handle_ecnr, &audioBuf, &pOutAudio);
            int64_t tm1 = CDX_GetSysTimeUsMonotonic();
            pCtx->process_duration += (float)(tm1 - tm0) / 1000000.0f;
            if(UV_ECNR_OK == st)
            {
                memcpy((void*)((char*)pCtx->out_buff + pCtx->out_buff_data_remain_len), (void*)pOutAudio, frm_size*(nBitWidth/8));
            }
            else
            {
                if(UV_ECNR_VERIFY_ERROR == st)
                {
                    aloge("fatal error! ecnr license time passed.");
                }
                aloge("fatal error! process wav failed.");
            }
//            if(((int)pCtx->audio_duration)%10 == 0)
//            {
//                alogd("RTF: %.4f (%.2f / %.2f)", pCtx->process_duration/pCtx->audio_duration, pCtx->process_duration, pCtx->audio_duration);
//            }
        }
        else
        {
            memcpy((void*)((char*)pCtx->out_buff + pCtx->out_buff_data_remain_len), (void*)(pCtx->in_buff[0] + nProcessedSampleNum), frm_size*(nBitWidth/8));
        }
        nProcessedSampleNum += frm_size;
        left -= frm_size;
        pCtx->in_buff_data_remain_len -= frm_size*(nBitWidth/8);
        pCtx->out_buff_data_remain_len += frm_size*(nBitWidth/8);
    }
    // move remaining data in internal buffer to the beginning of the buffer 
    if(left > 0)
    {
        for(i = 0; i < nChnNum; i++)
        {
            memmove((void*)pCtx->in_buff[i], (void*)(pCtx->in_buff[i] + nProcessedSampleNum), pCtx->in_buff_data_remain_len);
        }
    }
    // fetch one valid output frame from output internal buffer, the length of valid frame must equal to chunsize.
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

