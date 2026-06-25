#ifndef _WEBRTCANS_H_
#define _WEBRTCANS_H_

#include <mm_comm_aio.h>

typedef struct WebRtcAnsContext
{
    // for ans common process 
    short *in_buff_ans;                       // buffer used as internal buffer to store near data for conjunction.
    unsigned int in_buff_len_ans;             // the length of the near buffer, normally is 2 x chunkbytesize.
    unsigned int in_buff_data_remain_len_ans; // the length of the valid data that stored in near buffer.

    short *out_buff_ans;                        // buffer used as internal buffer to store aec produced data for conjunction.
    unsigned int out_buff_len_ans;              // the length of the out buffer, normally is 2 x chunkbytesize.
    unsigned int out_buff_data_remain_len_ans;  // the length of the valid data that stored in out buffer.
    short *tmpBuf_ans;
    int tmpBufLen_ans;

    // for ans process
    void *ans_int;
    
    int  filter_state1[6];
    int  filter_state12[6];
    int  Synthesis_state1[6];
    int  Synthesis_state12[6];
}WebRtcAnsContext;
WebRtcAnsContext* ConstructWebRtcAnsContext();
void DestructWebRtcAnsContext(WebRtcAnsContext *pCtx);
int WebRtcAnsProcess(void *cookie, AUDIO_FRAME_S *pFrm, const AIO_ATTR_S *pAttr, BOOL bSuspendAns);

#endif  /* _WEBRTCANS_H_ */

