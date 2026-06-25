
#ifndef _AOWEBRTCAGC_H_
#define _AOWEBRTCAGC_H_

#include <stdbool.h>

#include <mm_comm_aio.h>
#include <alsa_interface.h>
#include <agc_lib.h>
#include "AOAgc.h"

typedef struct AOWebRtcAgc
{
    AOAgc mBase;
    void *agcInst; //WebRtcAgc handle
    WebRtcAgc_config_t agcConfig;
    short *inBuff;                       // buffer used as internal buffer to store near data for conjunction.
    unsigned int inBuffLen;             // the length of the near buffer, normally is 2 x chunkbytesize. unit:byte
    unsigned int inBuffDataRemainLen; // the length of the valid data that stored in near buffer. unit:byte
    short *outBuff;                        // buffer used as internal buffer to store aec produced data for conjunction.
    unsigned int outBuffLen;              // the length of the out buffer, normally is 2 x chunkbytesize.
    unsigned int outBuffDataRemainLen;  // the length of the valid data that stored in out buffer.

    //for debug
    FILE *tmp_pcm_fp_in;
    int tmp_pcm_in_size;
    FILE *tmp_pcm_fp_out;
    int tmp_pcm_out_size;
} AOWebRtcAgc;
AOWebRtcAgc* CreateAOWebRtcAgc(int nSampleRate, int nChnNum, int nBitWidth, int nFrameLen,
    AGC_FLOAT_CONFIG_S *pAgcConfig);
void FreeAOWebRtcAgc(AOWebRtcAgc *pThiz);

#endif  /* _AOWEBRTCAGC_H_ */

