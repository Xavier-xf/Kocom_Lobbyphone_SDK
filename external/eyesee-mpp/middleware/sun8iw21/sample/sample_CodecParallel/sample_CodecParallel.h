#ifndef _SAMPLE_CODECPARALLEL_H_
#define _SAMPLE_CODECPARALLEL_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_vi.h>
#include <mpi_demux.h>
#include <mpi_mux.h>
#include <mpi_venc.h>
#include <mpi_vdec.h>
#include "tmessage.h"

#define MAX_FILE_PATH_SIZE (256)

/* Command Line parameter */
typedef struct SampleCodecParallelCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleCodecParallelCmdLineParam;

/* MPI parameter and configure */
typedef struct ViConfig
{
    int mVippID;
    int mOnlineEnable;
    int mOnlineShareBufNum;
    int mVippBufNum;
    PIXEL_FORMAT_E mPicFormat;
    int mCaptureWidth;
    int mCaptureHeight;
    int mFrameRate;
}ViConfig;

typedef struct ViParam
{
    ISP_DEV mIspDev;
    VI_DEV  mViDev;
    VI_CHN  mViChn;
    ViConfig *mpViCfg;
    void *priv;
}ViParam;

typedef struct VoConfig
{
    int mVoDev;
    int mDisplayX;
    int mDisplayY;
    int mDisplayWidth;
    int mDisplayHeight;
    int mVoLayer;
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;
    PIXEL_FORMAT_E mPicFormat;
}VoConfig;

typedef struct VoParam
{
    VO_DEV mVoDev;
    VO_LAYER mUILayer;
    VO_LAYER mVoLayer;
    VO_CHN mVoChn;
    VoConfig *mpVoCfg;
    void *priv;
}VoParam;

typedef struct VencConfig
{
    VENC_CHN mVencChn;
    int mOnlineEnable;
    int mOnlineShareBufNum;
    int mVideoFrameRate;
    int mVideoBitrate;
    int mVideoWidth;
    int mVideoHeight;
    PAYLOAD_TYPE_E mVideoEncoderType;
    int mVideoRcMode;      // rc_mode for H264/H265 0:CBR  1:VBR  2:FIXQP  3:QPMAP
}VencConfig;

typedef struct VencParam
{
    VENC_CHN mVencChn;
    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    VencConfig *mpVencCfg;
    ViConfig *mpViCfg;
    MEDIA_FILE_FORMAT_E mFileFormat;
    void *priv;
}VencParam;

typedef struct VdecConfig
{
    int mVdecBufSize;
}VdecConfig;

typedef struct VdecParam
{
    VDEC_CHN mVdecChn;
    int mMaxVdecOutputWidth;
    int mMaxVdecOutputHeight;
    int mVideoNum;
    int mDemuxDisableTrack;
    PAYLOAD_TYPE_E mCodecType;
    PIXEL_FORMAT_E mPicFormat;
    int mVdecBufSize;
    void *priv;
}VdecParam;

typedef struct MuxConfig
{
    MUX_CHN mMuxChn;
    char mDestVideoFile[MAX_FILE_PATH_SIZE];
    MEDIA_FILE_FORMAT_E mVideoFileFormat; // mp4, ts
    int mVideoDuration;  // per output media file time len (s)
    int nMaxFileNum;
}MuxConfig;

typedef struct
{
    char strFilePath[MAX_FILE_PATH_SIZE];
    struct list_head mList;
}FilePathNode;

typedef struct MuxParam
{
    //MUX_GRP mMuxGrp;
    MUX_CHN mMuxChn;
    MuxConfig *mpMuxCfg;
    VencConfig *mpVencCfg;
    MEDIA_FILE_FORMAT_E mFileFormat;
    void *priv;
}MuxParam;

typedef struct DemuxConfig
{
    char mSrcVideoFile[MAX_FILE_PATH_SIZE];
}DemuxConfig;

typedef struct DemuxParam
{
    DEMUX_CHN mDmxChn;
    DemuxConfig *mpDmxCfg;
    int mDemuxDisableTrack;
    void *priv;
}DemuxParam;

typedef struct ClockParam
{
    CLOCK_CHN mClockChn;
    void *priv;
}ClockParam;

/* Function parameter */
typedef struct RecordContext
{
    int mEnable;
    ViParam mViPara;
    VencParam mVencPara;
    MuxParam mMuxPara;
    ViConfig mViCfg;
    VencConfig mVencCfg;
    MuxConfig mMuxCfg;
    pthread_t nMsgQueueThreadId;
    message_queue_t stMsgQueue;
    struct list_head stFileList;    //FilePathNode
    int nFileNum;
    void *priv;
}RecordContext;

typedef struct PreviewContext
{
    int mEnable;
    ViParam mViPara;
    VoParam mVoPara;
    ViConfig mViCfg;
    VoConfig mVoCfg;
    void *priv;
}PreviewContext;

typedef struct PlayContext
{
    int mEnable;
    DemuxParam mDmxPara;
    VdecParam mVdecPara;
    VoParam mVoPara;
    ClockParam mClkPara;
    VdecConfig mVdecCfg;
    DemuxConfig mDmxCfg;
    VoConfig mVoCfg;
    void *priv;
    int mLoopEnable;
    pthread_t mMsgQueueThreadId;
    message_queue_t mMsgQueue;
}PlayContext;

typedef enum CodecParallelMsgType
{
    MSGQUEUE_MEDIA_NOTIFY_EOF = 0,

    MSGQUEUE_Rec_NeedSetNextFd = 100,
    MSGQUEUE_Rec_FileDone,

    MSGQUEUE_STOP = 900,
}CodecParallelMsgType;

typedef struct SampleCodecParallelContext
{
    SampleCodecParallelCmdLineParam mCmdLinePara; // configure file
    int mTestDuration;  // unit: s, 0 mean infinite
    cdx_sem_t mSemExit; // used to control the end of the test
    MPP_SYS_CONF_S mSysConf; // config mpp system
    RecordContext mRec; // for record
    RecordContext mSubRec; // for sub record
    PreviewContext mPrew; // for preview
    PlayContext mPlay; // for play
}SampleCodecParallelContext;

#endif  /* _SAMPLE_CODECPARALLEL_H_ */

