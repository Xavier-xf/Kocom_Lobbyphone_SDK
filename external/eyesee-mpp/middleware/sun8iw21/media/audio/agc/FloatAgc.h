#ifndef _FLOATAGC_H_
#define _FLOATAGC_H_

#include <mm_comm_aio.h>
#include <alsa_interface.h>
#include <agc_float.h>

typedef struct FloatAgcContext
{
    short *ai_agc_float_tmp_buff;  // tmp buffer to store data processed by agc float

    agc_handle *ai_agc_float_handle;  /** agc float handle */
    //pthread_mutex_t mAgcDbGainLock;     // to protect the agc db gain call,when used in two thread asynchronously.

    //for debug
    FILE *tmp_pcm_fp_in;
    int tmp_pcm_in_size;
    FILE *tmp_pcm_fp_out;
    int tmp_pcm_out_size;
}FloatAgcContext;
FloatAgcContext* ConstructFloatAgcContext(int nSampleRate, int nChnNum, int nFrameLen, AGC_FLOAT_CONFIG_S *pAgcConfig);
void DestructFloatAgcContext(FloatAgcContext *pCtx);
int FloatAgcProcess(void *cookie, AUDIO_FRAME_S *pFrm);
int FloatAgcGetInnerDataInfo(void *cookie, char **ppCacheProcessedData, int *pCacheProcessedDataLen,
        char **ppLeftOriginData, int *pLeftOriginDataLen);
#endif  /* _FLOATAGC_H_ */

