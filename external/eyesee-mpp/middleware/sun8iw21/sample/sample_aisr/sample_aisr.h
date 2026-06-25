#ifndef __SAMPLE_AISR_H__
#define __SAMPLE_AISR_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <confparser.h>

#include "mm_comm_sys.h"
#include "mpi_sys.h"
#include "mm_comm_vi.h"
#include "mpi_vi.h"
#include <mpi_isp.h>
#include "vencoder.h"
#include "mpi_venc.h"
#include "mm_comm_video.h"
#include "tsemaphore.h"

#define MAX_FILE_PATH_LEN  (128)

typedef enum H264_PROFILE_E
{
   H264_PROFILE_BASE = 0,
   H264_PROFILE_MAIN,
   H264_PROFILE_HIGH,
} H264_PROFILE_E;

typedef enum H265_PROFILE_E
{
   H265_PROFILE_MAIN = 0,
   H265_PROFILE_MAIN10,
   H265_PROFILE_STI11,
} H265_PROFILE_E;

typedef struct venc_in_frame_s
{
    VIDEO_FRAME_INFO_S  mFrame;
    struct list_head mList;
} VENC_IN_FRAME_S, *PTR_VENC_IN_FRAME_S;

typedef struct CmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_LEN];
} CMDLINEPARAM_S;

typedef struct Aisr_Config
{
    char srcYuvFile[MAX_FILE_PATH_LEN];
    char dstYuvFile[MAX_FILE_PATH_LEN];
    char dstVideoFile[MAX_FILE_PATH_LEN];

    int srcWidth;
    int srcHeight;
    int srcPixFmt;
    int mSrcFrameRate;
    int mViBufferNum;

    int mAisrCropWidth;
    int mAisrCropHeight;
    int mAisrPx;
    int mAisrWidthScale;
    int mAisrHeightScale;
    int mAisrOutputBufNum;
    int mAisr1200WTest;

    int mVippDev;
    int mVeChn;
    int mVencsrcWidth;
    int mVencsrcHeight;
    int mVencdstWidth;
    int mVencdstHeight;

    int mVideoEncoderFmt;
    int mVideoFrameRate;
    int mVideoBitRate;
    int mTestDuration;

    int mVbrOptEnable;

    int mProductMode;
    int mKeyFrameInterval;
    int mRcMode;

    int mEncUseProfile;

    int mVbvBufferSize;  //unit:Byte
    int mVbvThreshSize;  //unit:Byte

    int mEncppEnable;
    int mVeRefFrameLbcMode;
} Aisr_Config_S;

typedef struct SampleAisrSaveBufNode
{
    VIDEO_FRAME_INFO_S mSrcFrameInfo;
    VIDEO_FRAME_INFO_S mDstFrameInfo;
    struct list_head mList;
    int mId;
    int mDataLen;
    unsigned int mDataPhyAddr;
    void *mpDataVirAddr;
} SampleAisrSaveBufNode;

typedef struct CsiSaveBufMgr
{
    struct list_head mFrameIdleList;
    struct list_head mFrameAisrUsingList;
    struct list_head mFrameReadyList;
    struct list_head mFrameVeUsingList;
    pthread_mutex_t mFrameIdleListLock;
    pthread_mutex_t mFrameAisrUsingListLock;
    pthread_mutex_t mFrameReadyListLock;
    pthread_mutex_t mFrameVeUsingListLock;

    struct list_head mInputYuvIdleList;
    struct list_head mInputYuvReadyList;
    struct list_head mInputYuvUsingList;
    pthread_mutex_t mInputYuvIdleListLock;
    pthread_mutex_t mInputYuvReadyListLock;
    pthread_mutex_t mInputYuvUsingListLock;
} CsiSaveBufMgr;

typedef struct SAMPLE_AISR_S
{
    Aisr_Config_S mConfigPara;
    CMDLINEPARAM_S mCmdLinePara;
    cdx_sem_t mSemExit;
    MPP_SYS_CONF_S mSysConf;

    VI_ATTR_S mViAttr;
    ISP_DEV mIspDev;
    VI_DEV mViDev;
    VI_CHN mViChn;
    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    VENC_CHN mVeChn;

    pthread_t mGetStreamThreadId;
    pthread_t mSendEncFrameThreadId;
    pthread_t mCSIFrameThreadId;

    pthread_t mInputYUVThreadId;
    pthread_t mDoAisrThreadId;
    pthread_t mOutputYUVThreadId;
    int mOpt1200wYuv;
    int mInputRead2Eof;
    FILE *mInputFileFp;

    FILE *mOutputFileFp;
    int mExitFlag;
    CsiSaveBufMgr *mpSaveBufMgr;
} SAMPLE_AISR_S;

#endif //#define __SAMPLE_AISR_H__
