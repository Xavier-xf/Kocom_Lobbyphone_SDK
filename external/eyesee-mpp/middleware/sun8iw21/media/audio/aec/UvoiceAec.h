#ifndef _UVOICEAEC_H_
#define _UVOICEAEC_H_

#include <mm_comm_aio.h>
#include <alsa_interface.h>

#define MAX_MIXEDIN_NUM (8)
typedef struct UvoiceAecContext
{
    void *handle_ecnr;
    short *in_buff[MAX_MIXEDIN_NUM];                       // buffer used as internal buffer to store mixed data separately(including MicIn and RefIn). for aec
    unsigned int in_buff_len;             // the length of the in_buffer, normally is 2 x chunkbytesize.
    unsigned int in_buff_data_remain_len; // the length of the valid data that stored in in_buffer.

    short *out_buff;                        // buffer used as internal buffer to store aec produced data for conjunction. for aec
    unsigned int out_buff_len;              // the length of the out buffer, normally is 2 x chunkbytesize.
    unsigned int out_buff_data_remain_len;  // the length of the valid data that stored in out buffer.

    int mMicChnNum;
    int mRefChnNum;
    int mChnLen; //unit:bytes

    //for debug
    float process_duration; //unit:s
    float audio_duration; //unit:s
  
    FILE *tmp_pcm_fp_in; //mixed audio data, MicIn + RefIn.
    int tmp_pcm_in_size;
    FILE *tmp_pcm_fp_out;
    int tmp_pcm_out_size;
}UvoiceAecContext;
UvoiceAecContext* ConstructUvoiceAecContext(AIO_ATTR_S *pAttr, int nFrameBytes, AudioDevCallbackFuncType pAudioDevCallback,
    void *pAudioDevCookie, AUDIO_DEV AudioDevId);
void DestructUvoiceAecContext(UvoiceAecContext *pCtx);
int UvoiceAecProcess(void *cookie, AUDIO_FRAME_S *pFrm, BOOL bSuspendAec);

#endif  /* _UVOICEAEC_H_ */

