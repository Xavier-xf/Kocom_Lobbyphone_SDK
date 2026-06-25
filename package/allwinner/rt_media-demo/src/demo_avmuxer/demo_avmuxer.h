
#ifndef _DEMO_AVMUXER_H_
#define _DEMO_AVMUXER_H_

#include <pthread.h>

#define LOG_TAG "demo_avmuxer"
#include <aw_util.h>

#include <mm_common.h>
#include <mm_comm_sys.h>
#include <mm_comm_mux.h>
#include <mm_comm_aio.h>
#include <mm_comm_aenc.h>
#include <tmessage.h>
#include <RecorderMode.h>

#define MAX_FILE_PATH_LEN  (128)

typedef struct 
{
    char strFilePath[MAX_FILE_PATH_LEN];
    struct list_head mList;
}FilePathNode;

typedef struct DemoAvmuxerContext
{
    demo_video_param mparam;
    
    VideoInputConfig config_0;
    AWVideoInput_SeiAttr stSeiAttr;
    int nAudioChnNum;
    int nAudioBitWidth;
    int nAudioSamplesPerFrame;
    int nAudioSampleRate;
    PAYLOAD_TYPE_E eAudioEncodeType;
    int nAudioBitRate; //bps

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAIDevId;
    AI_CHN mAIChnId;
    AIO_ATTR_S mAioAttr;
    AENC_CHN mAEncChnId;
    AENC_CHN_ATTR_S mAEncAttr;

    MUX_CHN mMuxChn;
    MUX_CHN_ATTR_S mMuxChnAttr;
    RecordFileDurationPolicy mPolicy;
    int mbMuxChnSetSpsppsFlag;
    pthread_mutex_t mMuxChnLock;
    int mMuxVideoStreamId;
    int mMuxAudioStreamId;

    pthread_t StreamDispatchThreadId;
    message_queue_t  mStreamDispatchCmdQueue;

    int max_bitstream_count; //max encoded video frame count.
    int stream_count_0; //count encoded video frame number, for channel0.
    sem_t finish_sem;
    int video_finish_flag; //indicate video frame count is finish.
    uint64_t pre_pts; //record previous video frame pts. unit:us
    int pre_pts_in_seconds; //record last video frame pts diff in seconds.
    int cb_stream_cnt_in_seconds; //the stream count between two seconds.
}DemoAvmuxerContext;

#endif  /* _DEMO_AVMUXER_H_ */

