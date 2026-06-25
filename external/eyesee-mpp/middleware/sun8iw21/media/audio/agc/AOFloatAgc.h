#ifndef _AOFLOATAGC_H_
#define _AOFLOATAGC_H_

#include <mm_comm_aio.h>
#include <alsa_interface.h>
#include <agc_float.h>
#include "AOAgc.h"

typedef struct AOFloatAgc
{
    AOAgc mBase;
    short *ai_agc_float_tmp_buff;  // tmp buffer to store data processed by agc float
    unsigned int nTmpBuffLen;

    agc_handle *ai_agc_float_handle;  /** agc float handle */
    //pthread_mutex_t mAgcDbGainLock;     // to protect the agc db gain call,when used in two thread asynchronously.

    //for debug
    FILE *tmp_pcm_fp_in;
    int tmp_pcm_in_size;
    FILE *tmp_pcm_fp_out;
    int tmp_pcm_out_size;
}AOFloatAgc;
AOFloatAgc *CreateAOFloatAgc(int nSampleRate, int nChnNum, int nBitWidth, int nFrameLen,
    AGC_FLOAT_CONFIG_S *pAgcConfig);
void FreeAOFloatAgc(AOFloatAgc *pThiz);

#endif  /* _AOFLOATAGC_H_ */

