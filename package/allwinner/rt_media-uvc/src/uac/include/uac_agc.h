#ifndef __UAC_AGC__
#define __UAC_AGC__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <media/LIBRARY/agc_lib/include/agc_lib.h>

typedef struct UAC_AGC_FRAME_S
{
    unsigned int mSamplerate;   /* sample rate, extended for ai&ao*/
    unsigned int mBitwidth;     /*audio frame bitwidth*/
    //unsigned int mSoundmode;    /*audio frame momo or stereo mode, used by ai&ao,adec*/
    void                *mpAddr;
    unsigned long long  mTimeStamp;                /*audio frame timestamp, unit:us*/
    unsigned int        mSeq;                      /*audio frame seq*/
    unsigned int        mLen;                      /*data lenth per channel in frame*/
    unsigned int        mId;
} UAC_AGC_FRAME_S;

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
WebRtcAgcContext* ConstructWebRtcAgcContext(int nSampleRate, int nChnNum, int nFrameLen, float target_db,
	float max_gain_db);
void DestructWebRtcAgcContext(WebRtcAgcContext *pCtx);
int WebRtcAgcProcess(void *cookie, UAC_AGC_FRAME_S *pFrm);
int WebRtcAgcGetInnerDataInfo(void *cookie, char **ppCacheProcessedData, int *pCacheProcessedDataLen,
    char **ppLeftOriginData, int *pLeftOriginDataLen);

#endif  /* _WEBRTCAGC_H_ */
