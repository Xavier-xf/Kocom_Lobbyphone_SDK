#ifndef _WEBRTCAEC_H_
#define _WEBRTCAEC_H_

#include <stdbool.h>

#include <mm_comm_aio.h>
#include <alsa_interface.h>

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
WebRtcAecContext* ConstructWebRtcAecContext(AIO_ATTR_S *pAttr, int nFrameBytes);
void DestructWebRtcAecContext(WebRtcAecContext *pCtx);
int WebRtcAecProcess(void *cookie, AUDIO_FRAME_S *pFrm, BOOL bSuspendAec);

#endif  /* _WEBRTCAEC_H_ */

