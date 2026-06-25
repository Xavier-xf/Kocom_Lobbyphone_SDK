#ifndef _DEMO_VIDEO_MUXER_H_
#define _DEMO_VIDEO_MUXER_H_

#include <pthread.h>

#define LOG_TAG "demo_video_muxer"
#include <aw_util.h>

#include <mm_common.h>
#include <mm_comm_sys.h>
#include <mm_comm_mux.h>
#include <RecorderMode.h>

typedef struct DemoVideoMuxerContext
{
    demo_video_param mparam;

    VideoInputConfig config_0;
    AWVideoInput_SeiAttr stSeiAttr;

    MPP_SYS_CONF_S mSysConf;
    MUX_CHN mMuxChn;
    MUX_CHN_ATTR_S mMuxChnAttr;
    RecordFileDurationPolicy mPolicy;
    int mbMuxChnSetSpsppsFlag;
    pthread_mutex_t mMuxChnLock;
    int mMuxVideoStreamId;
    int mMuxAudioStreamId;

    int max_bitstream_count; //max encoded video frame count.
    int stream_count_0; //count encoded video frame number, for channel0.
    sem_t finish_sem;
    int video_finish_flag; //indicate video frame count is finish.
    uint64_t pre_pts; //record previous video frame pts. unit:us
    int pre_pts_in_seconds; //record last video frame pts diff in seconds.
    int cb_stream_cnt_in_seconds; //the stream count between two seconds.
}DemoVideoMuxerContext;

#endif  /* _DEMO_VIDEO_MUXER_H_ */

