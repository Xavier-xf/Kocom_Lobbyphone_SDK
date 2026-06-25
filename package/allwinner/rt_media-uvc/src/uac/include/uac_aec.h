#ifndef __UAC_AEC_H__
#define __UAC_AEC_H__

#include <stdio.h>
#include <stdbool.h>

typedef struct UAC_AEC_FRAME_S
{
    unsigned int mSamplerate;   /* sample rate, extended for ai&ao*/
    unsigned int mBitwidth;     /*audio frame bitwidth*/
    //unsigned int mSoundmode;    /*audio frame momo or stereo mode, used by ai&ao,adec*/
    void                *mpAddr;
    unsigned long long  mTimeStamp;                /*audio frame timestamp, unit:us*/
    unsigned int        mSeq;                      /*audio frame seq*/
    unsigned int        mLen;                      /*data lenth per channel in frame*/
    unsigned int        mId;
} UAC_AEC_FRAME_S;

typedef struct WebRtcAecContext
{
    void *aecmInst; //WebRtcAec handle
    short *near_buff;                       // buffer used as internal buffer to store near data for conjunction. for aec
    unsigned int near_buff_len;             // the length of the near buffer, normally is 2 x chunkbytesize.
    unsigned int near_buff_data_remain_len; // the length of the valid data that stored in near buffer.
    short *ref_buff;                        // buffer used as internal buffer to store reference data for conjunction. for aec
    unsigned int ref_buff_len;              // the length of the ref buffer, normally is 2 x chunkbytesize.
    unsigned int ref_buff_data_remain_len;  // the length of the valid data that stored in ref buffer.

    short *out_buff;                        // buffer used as internal buffer to store aec produced data for conjunction. for aec
    unsigned int out_buff_len;              // the length of the out buffer, normally is 2 x chunkbytesize.
    unsigned int out_buff_data_remain_len;  // the length of the valid data that stored in out buffer.
    //short *tmpBuf;  //used to store aec produced data temporarily, then copy to out_buff.
    //int tmpBufLen;

    int mRefDelayMs; //ref pcm delay time because of auto conversion in I2SRX. user set it.
    bool mbRefSkipFlag;  //indicate if skip ref pcm data of delayTime is done.

    FILE *tmp_pcm_fp_in;
    int tmp_pcm_in_size;
    FILE *tmp_pcm_fp_ref;
    int tmp_pcm_ref_size;
    FILE *tmp_pcm_fp_out;
    int tmp_pcm_out_size;
}WebRtcAecContext;
WebRtcAecContext* ConstructWebRtcAecContext(unsigned int samplerate, int aec_mode, int aec_delayms, int nFrameBytes);
void DestructWebRtcAecContext(WebRtcAecContext *pCtx);
int WebRtcAecProcess(void *cookie, UAC_AEC_FRAME_S *pFrm, int bSuspendAec);

#endif  /* _WEBRTCAEC_H_ */
