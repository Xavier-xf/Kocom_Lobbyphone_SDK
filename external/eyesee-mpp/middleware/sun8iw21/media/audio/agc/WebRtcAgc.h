
#ifndef _WEBRTCAGC_H_
#define _WEBRTCAGC_H_

#include <stdbool.h>

#include <mm_comm_aio.h>
#include <alsa_interface.h>
#include <agc_lib.h>

typedef struct WebRtcAgcContext
{
    void *agcInst; //WebRtcAgc handle
    WebRtcAgc_config_t agcConfig;
    short *inBuff;                       // buffer used as internal buffer to store near data for conjunction.
    unsigned int inBuffLen;             // the length of the near buffer, normally is 2 x chunkbytesize.
    unsigned int inBuffDataRemainLen; // the length of the valid data that stored in near buffer.
    short *outBuff;                        // buffer used as internal buffer to store aec produced data for conjunction.
    unsigned int outBuffLen;              // the length of the out buffer, normally is 2 x chunkbytesize.
    unsigned int outBuffDataRemainLen;  // the length of the valid data that stored in out buffer.

    //for debug
    FILE *tmp_pcm_fp_in;
    int tmp_pcm_in_size;
    FILE *tmp_pcm_fp_out;
    int tmp_pcm_out_size;
}WebRtcAgcContext;
WebRtcAgcContext* ConstructWebRtcAgcContext(int nSampleRate, int nChnNum, int nFrameLen, AGC_FLOAT_CONFIG_S *pAgcConfig);
void DestructWebRtcAgcContext(WebRtcAgcContext *pCtx);
int WebRtcAgcProcess(void *cookie, AUDIO_FRAME_S *pFrm);
int WebRtcAgcGetInnerDataInfo(void *cookie, char **ppCacheProcessedData, int *pCacheProcessedDataLen,
    char **ppLeftOriginData, int *pLeftOriginDataLen);

#endif  /* _WEBRTCAGC_H_ */

