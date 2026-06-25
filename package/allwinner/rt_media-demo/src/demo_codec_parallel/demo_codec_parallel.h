#ifndef _DEMO_CODEC_PARALLEL_H_
#define _DEMO_CODEC_PARALLEL_H_

#include <pthread.h>

#include <aw_util.h>

#include <mm_common.h>
#include <mm_comm_sys.h>
#include "mm_comm_vdec.h"
#include "mpi_vdec.h"
#include "mm_comm_vo.h"
#include "mpi_vo.h"
#include "hwdisplay.h"
#include "tmessage.h"

#define MAX_FILE_PATH_SIZE (256)

/* Command Line parameter */
typedef struct DemoCodecParallelCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}DemoCodecParallelCmdLineParam;

typedef struct DemoCodecParallelConfig
{
    // common
    int mTestDuration;

    // record
    int mRecordEnable;
    int mVippID;
    int mOnlineEnable;
    int mOnlineShareBufNum;
    int mVippBufNum;
    int mVippPixelFormat;
    int mVippColorSpace;
    int mVippWidth;
    int mVippHeight;
    int mVippFrameRate;
    int mVideoFrameRate;
    int mVideoBitrate;
    int mVideoWidth;
    int mVideoHeight;
    int mVideoEncoderType;
    int mVideoRcMode;
    char mDestVideoFile[128];
    int mRecordDuration;

    // record sub
    int mSubRecordEnable;
    int mSubVippID;
    int mSubVippBufNum;
    int mSubVippPixelFormat;
    int mSubVippColorSpace;
    int mSubVippWidth;
    int mSubVippHeight;
    int mSubVippFrameRate;
    int mSubVideoFrameRate;
    int mSubVideoBitrate;
    int mSubVideoWidth;
    int mSubVideoHeight;
    int mSubVideoEncoderType;
    int mSubVideoRcMode;
    char mSubDestVideoFile[128];
    int mSubRecordDuration;

	// play
	int mPlayEnable;
    int mVideoDecoderType;
	VDEC_CHN mVdecChn;
	char mSrcVideoFile[128];
    char mSrcVideoLenFile[128];
	int mVdecBufSize;
    int mVdecOutputPixelFormat;
	int mMaxVdecOutputWidth;
	int mMaxVdecOutputHeight;
    int mUILayer;
	VO_DEV mVoDev;
	VO_LAYER mVoLayer;
	VO_CHN mVoChn;
	int mDisplayX;
	int mDisplayY;
	int mDisplayWidth;
	int mDisplayHeight;
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;
	int mPlayFrameRate;
}DemoCodecParallelConfig;

typedef struct DemoCodecParallelStatistics
{
    int max_bitstream_count; //max encoded video frame count.
    int stream_count_0; //count encoded video frame number, for channel0.
    int video_finish_flag; //indicate video frame count is finish.
    uint64_t pre_pts; //record previous video frame pts. unit:us
    int pre_pts_in_seconds; //record last video frame pts diff in seconds.
    int cb_stream_cnt_in_seconds; //the stream count between two seconds.
} DemoCodecParallelStatistics;

typedef struct DemoCodecParallelContext
{
    MPP_SYS_CONF_S mSysConf;
    DemoCodecParallelCmdLineParam mCmdLinePara;
    DemoCodecParallelConfig mConfigPara;
    sem_t mSemExit;
    pthread_t mVdecThreadId;
    BOOL mVdecThreadExitFlag;
    DemoCodecParallelStatistics mStatis;
    DemoCodecParallelStatistics mSubStatis;
    FILE *mOutFile;
    FILE *mSubOutFile;
}DemoCodecParallelContext;

#endif  /* _DEMO_CODEC_PARALLEL_H_ */

