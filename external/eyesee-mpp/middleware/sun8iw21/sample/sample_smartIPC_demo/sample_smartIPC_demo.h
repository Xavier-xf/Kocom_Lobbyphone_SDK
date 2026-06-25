#ifndef _SAMPLE_SMART_IPC_DEMO_H_
#define _SAMPLE_SMART_IPC_DEMO_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_region.h>
#include "rgb_ctrl.h"
#include "tmessage.h"
#include "sample_common_isp.h"

#define MAX_FILE_PATH_SIZE  (256)

#define INVALID_OVERLAY_HANDLE  (-1)

typedef enum SmartIPCMsgType
{
    Vi_Timeout,
    MsgQueue_Stop,
}SmartIPCMsgType;

typedef struct SampleSmartIPCDemoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleSmartIPCDemoCmdLineParam;

typedef struct SampleSmartIPCDemoConfig
{
    // rtsp
    int mRtspNetType;
    // common params
    int mAiIspNpuRefBufReduceEnable;
    int mAiIspSwitchReleaseResEnable;
    int mVeRecRefBufReduceEnable;
    int mStreamBufSize;
    int mProductMode;
    int mVbrOptEnable;
    int mRcMode;
    int mInitQp;
    int mMinIQp;
    int mMaxIQp;
    int mMinPQp;
    int mMaxPQp;
    int mEnMbQpLimit;
    int mMovingTh;
    int mQuality;
    int mPBitsCoef;
    int mIBitsCoef;
    // main stream
    int mMainEnable;
    int mMainRtspID;
    ISP_DEV mMainIsp;
    int mMainIspD3dLbcRatio;
    VI_DEV mMainVipp;
    VI_CHN mMainViChn;
    BOOL mMainLdciUseExtBufEnable;
    VI_DEV mMainLdciVipp;
    int mMainSrcWidth;
    int mMainSrcHeight;
    PIXEL_FORMAT_E mMainPixelFormat;
    int mMainWdrEnable;
    int mMainViBufNum;
    int mMainSrcFrameRate;
    VENC_CHN mMainVEncChn;
    PAYLOAD_TYPE_E mMainEncodeType;
    int mMainEncodeWidth;
    int mMainEncodeHeight;
    int mMainEncodeFrameRate;
    int mMainEncodeBitrate;
    int mMainOnlineEnable;
    int mMainOnlineShareBufNum;
    unsigned char mMainIspTestEnable;
    unsigned int mMainIspTestIntervalMs;
    unsigned char mMainDetectMipiDeskEnable;
    unsigned int  mMainDetectIntervalMs;
    unsigned char mMainMipiChannel;
    BOOL mMainEncppEnable;
    int mMainVeRefFrameLbcMode;
    int mMainKeyFrameInterval;
    char mMainFilePath[MAX_FILE_PATH_SIZE];
    int mMainSaveOneFileDuration;
    int mMainSaveMaxFileCnt;
    char mMainDrawOSDText[MAX_FILE_PATH_SIZE];
    // main tdm raw
    int mMainIspTdmRawProcessType;
    int mMainIspTdmRxBufNum;
    int mMainIspTdmRawProcessFrameCntMin;
    int mMainIspTdmRawProcessFrameCntMax;
    char mMainIspTdmRawFilePath[MAX_FILE_PATH_SIZE];
    // main nn
    BOOL mMainNnEnable;
    int mMainNnNbgType;
    VI_DEV mMainNnVipp;
    int mMainNnViBufNum;
    int mMainNnSrcFrameRate;
    char mMainNnNbgFilePath[MAX_FILE_PATH_SIZE];
    int mMainNnDrawOrlEnable;
    // main aiisp
    BOOL mMainAiIspEnable;
    char mMainAiIspLutNbgFilePath[MAX_FILE_PATH_SIZE];
    char mMainAiIspNbgFilePath[MAX_FILE_PATH_SIZE];
    int mMainAiIspModelVersion;
    char mMainAiIspCfgBinPath[MAX_FILE_PATH_SIZE];
    int mMainAiIspWidth;
    int mMainAiIspHeight;
    int mMainAiIspTdmRxBufNum;
    int mMainAiIspMode;
    int mMainAiIspAutoSwitchEnable;
    int mMainAiIspSwitchInterval;
    int mMainAiIspSwitchCase;
    int mMainAiIspSwitchDropFrameNum;
    char mMainAiIspCfgBinPath2[MAX_FILE_PATH_SIZE];
    int mMainAiIspReserve0;
    int mMainAiIspReserve1;
    int mMainAiIspReserve2;
    // main 2nd stream
    int mMain2ndEnable;
    VI_DEV mMain2ndVipp;
    VI_CHN mMain2ndViChn;
    int mMain2ndSrcWidth;
    int mMain2ndSrcHeight;
    PIXEL_FORMAT_E mMain2ndPixelFormat;
    int mMain2ndViBufNum;
    int mMain2ndSrcFrameRate;
    VENC_CHN mMain2ndVEncChn;
    PAYLOAD_TYPE_E mMain2ndEncodeType;
    int mMain2ndEncodeWidth;
    int mMain2ndEncodeHeight;
    int mMain2ndEncodeFrameRate;
    int mMain2ndEncodeBitrate;
    int mMain2ndEncppSharpAttenCoefPer;
    BOOL mMain2ndEncppEnable;
    int mMain2ndVeRefFrameLbcMode;
    int mMain2ndKeyFrameInterval;
    char mMain2ndFilePath[MAX_FILE_PATH_SIZE];
    int mMain2ndSaveOneFileDuration;
    int mMain2ndSaveMaxFileCnt;
    // sub stream
    int mSubEnable;
    int mSubRtspID;
    ISP_DEV mSubIsp;
    int mSubIspD3dLbcRatio;
    VI_DEV mSubVipp;
    VI_CHN mSubViChn;
    BOOL mSubLdciUseExtBufEnable;
    VI_DEV mSubLdciVipp;
    int mSubSrcWidth;
    int mSubSrcHeight;
    PIXEL_FORMAT_E mSubPixelFormat;
    int mSubWdrEnable;
    int mSubViBufNum;
    int mSubSrcFrameRate;
    VENC_CHN mSubVEncChn;
    PAYLOAD_TYPE_E mSubEncodeType;
    int mSubEncodeWidth;
    int mSubEncodeHeight;
    int mSubEncodeFrameRate;
    int mSubEncodeBitrate;
    int mSubEncppSharpAttenCoefPer;
    BOOL mSubEncppEnable;
    int mSubVeRefFrameLbcMode;
    int mSubKeyFrameInterval;
    char mSubFilePath[MAX_FILE_PATH_SIZE];
    int mSubSaveOneFileDuration;
    int mSubSaveMaxFileCnt;
    char mSubDrawOSDText[MAX_FILE_PATH_SIZE];
    // sub tdm raw
    int mSubIspTdmRawProcessType;
    int mSubIspTdmRxBufNum;
    int mSubIspTdmRawProcessFrameCntMin;
    int mSubIspTdmRawProcessFrameCntMax;
    char mSubIspTdmRawFilePath[MAX_FILE_PATH_SIZE];
    // sub nn
    BOOL mSubNnEnable;
    int mSubNnNbgType;
    VI_DEV mSubNnVipp;
    int mSubNnViBufNum;
    int mSubNnSrcFrameRate;
    char mSubNnNbgFilePath[MAX_FILE_PATH_SIZE];
    int mSubNnDrawOrlEnable;
    // sub aiisp
    BOOL mSubAiIspEnable;
    char mSubAiIspLutNbgFilePath[MAX_FILE_PATH_SIZE];
    char mSubAiIspNbgFilePath[MAX_FILE_PATH_SIZE];
    int mSubAiIspModelVersion;
    char mSubAiIspCfgBinPath[MAX_FILE_PATH_SIZE];
    int mSubAiIspWidth;
    int mSubAiIspHeight;
    int mSubAiIspTdmRxBufNum;
    int mSubAiIspMode;
    int mSubAiIspAutoSwitchEnable;
    int mSubAiIspSwitchInterval;
    int mSubAiIspSwitchCase;
    int mSubAiIspSwitchDropFrameNum;
    char mSubAiIspCfgBinPath2[MAX_FILE_PATH_SIZE];
    int mSubAiIspReserve0;
    int mSubAiIspReserve1;
    int mSubAiIspReserve2;
    // sub 2nd stream
    int mSub2ndEnable;
    VI_DEV mSub2ndVipp;
    VI_CHN mSub2ndViChn;
    int mSub2ndSrcWidth;
    int mSub2ndSrcHeight;
    PIXEL_FORMAT_E mSub2ndPixelFormat;
    int mSub2ndViBufNum;
    int mSub2ndSrcFrameRate;
    VENC_CHN mSub2ndVEncChn;
    PAYLOAD_TYPE_E mSub2ndEncodeType;
    int mSub2ndEncodeWidth;
    int mSub2ndEncodeHeight;
    int mSub2ndEncodeFrameRate;
    int mSub2ndEncodeBitrate;
    int mSub2ndEncppSharpAttenCoefPer;
    BOOL mSub2ndEncppEnable;
    int mSub2ndVeRefFrameLbcMode;
    int mSub2ndKeyFrameInterval;
    char mSub2ndFilePath[MAX_FILE_PATH_SIZE];
    int mSub2ndSaveOneFileDuration;
    int mSub2ndSaveMaxFileCnt;
    // three stream
    int mThreeEnable;
    int mThreeRtspID;
    ISP_DEV mThreeIsp;
    int mThreeIspD3dLbcRatio;
    VI_DEV mThreeVipp;
    VI_CHN mThreeViChn;
    BOOL mThreeLdciUseExtBufEnable;
    VI_DEV mThreeLdciVipp;
    int mThreeSrcWidth;
    int mThreeSrcHeight;
    PIXEL_FORMAT_E mThreePixelFormat;
    int mThreeWdrEnable;
    int mThreeViBufNum;
    int mThreeSrcFrameRate;
    VENC_CHN mThreeVEncChn;
    PAYLOAD_TYPE_E mThreeEncodeType;
    int mThreeEncodeWidth;
    int mThreeEncodeHeight;
    int mThreeEncodeFrameRate;
    int mThreeEncodeBitrate;
    int mThreeOnlineEnable;
    int mThreeOnlineShareBufNum;
    BOOL mThreeEncppEnable;
    int mThreeVeRefFrameLbcMode;
    int mThreeKeyFrameInterval;
    char mThreeFilePath[MAX_FILE_PATH_SIZE];
    int mThreeSaveOneFileDuration;
    int mThreeSaveMaxFileCnt;
    char mThreeDrawOSDText[MAX_FILE_PATH_SIZE];
    // three tdm raw
    int mThreeIspTdmRawProcessType;
    int mThreeIspTdmRxBufNum;
    int mThreeIspTdmRawProcessFrameCntMin;
    int mThreeIspTdmRawProcessFrameCntMax;
    char mThreeIspTdmRawFilePath[MAX_FILE_PATH_SIZE];
    // three nn
    BOOL mThreeNnEnable;
    int mThreeNnNbgType;
    VI_DEV mThreeNnVipp;
    int mThreeNnViBufNum;
    int mThreeNnSrcFrameRate;
    char mThreeNnNbgFilePath[MAX_FILE_PATH_SIZE];
    int mThreeNnDrawOrlEnable;
    // three aiisp
    BOOL mThreeAiIspEnable;
    char mThreeAiIspLutNbgFilePath[MAX_FILE_PATH_SIZE];
    char mThreeAiIspNbgFilePath[MAX_FILE_PATH_SIZE];
    int mThreeAiIspModelVersion;
    char mThreeAiIspCfgBinPath[MAX_FILE_PATH_SIZE];
    int mThreeAiIspWidth;
    int mThreeAiIspHeight;
    int mThreeAiIspTdmRxBufNum;
    int mThreeAiIspMode;
    int mThreeAiIspAutoSwitchEnable;
    int mThreeAiIspSwitchInterval;
    int mThreeAiIspSwitchCase;
    int mThreeAiIspSwitchDropFrameNum;
    char mThreeAiIspCfgBinPath2[MAX_FILE_PATH_SIZE];
    int mThreeAiIspReserve0;
    int mThreeAiIspReserve1;
    int mThreeAiIspReserve2;
    // three 2nd stream
    int mThree2ndEnable;
    VI_DEV mThree2ndVipp;
    VI_CHN mThree2ndViChn;
    int mThree2ndSrcWidth;
    int mThree2ndSrcHeight;
    PIXEL_FORMAT_E mThree2ndPixelFormat;
    int mThree2ndViBufNum;
    int mThree2ndSrcFrameRate;
    VENC_CHN mThree2ndVEncChn;
    PAYLOAD_TYPE_E mThree2ndEncodeType;
    int mThree2ndEncodeWidth;
    int mThree2ndEncodeHeight;
    int mThree2ndEncodeFrameRate;
    int mThree2ndEncodeBitrate;
    int mThree2ndEncppSharpAttenCoefPer;
    BOOL mThree2ndEncppEnable;
    int mThree2ndVeRefFrameLbcMode;
    int mThree2ndKeyFrameInterval;
    char mThree2ndFilePath[MAX_FILE_PATH_SIZE];
    int mThree2ndSaveOneFileDuration;
    int mThree2ndSaveMaxFileCnt;
    // four stream
    int mFourEnable;
    int mFourRtspID;
    ISP_DEV mFourIsp;
    int mFourIspD3dLbcRatio;
    VI_DEV mFourVipp;
    VI_CHN mFourViChn;
    BOOL mFourLdciUseExtBufEnable;
    VI_DEV mFourLdciVipp;
    int mFourSrcWidth;
    int mFourSrcHeight;
    PIXEL_FORMAT_E mFourPixelFormat;
    int mFourWdrEnable;
    int mFourViBufNum;
    int mFourSrcFrameRate;
    VENC_CHN mFourVEncChn;
    PAYLOAD_TYPE_E mFourEncodeType;
    int mFourEncodeWidth;
    int mFourEncodeHeight;
    int mFourEncodeFrameRate;
    int mFourEncodeBitrate;
    int mFourOnlineEnable;
    int mFourOnlineShareBufNum;
    BOOL mFourEncppEnable;
    int mFourVeRefFrameLbcMode;
    int mFourKeyFrameInterval;
    char mFourFilePath[MAX_FILE_PATH_SIZE];
    int mFourSaveOneFileDuration;
    int mFourSaveMaxFileCnt;
    char mFourDrawOSDText[MAX_FILE_PATH_SIZE];
    // four tdm raw
    int mFourIspTdmRawProcessType;
    int mFourIspTdmRxBufNum;
    int mFourIspTdmRawProcessFrameCntMin;
    int mFourIspTdmRawProcessFrameCntMax;
    char mFourIspTdmRawFilePath[MAX_FILE_PATH_SIZE];
    // four nn
    BOOL mFourNnEnable;
    int mFourNnNbgType;
    VI_DEV mFourNnVipp;
    int mFourNnViBufNum;
    int mFourNnSrcFrameRate;
    char mFourNnNbgFilePath[MAX_FILE_PATH_SIZE];
    int mFourNnDrawOrlEnable;
    // four aiisp
    BOOL mFourAiIspEnable;
    char mFourAiIspLutNbgFilePath[MAX_FILE_PATH_SIZE];
    char mFourAiIspNbgFilePath[MAX_FILE_PATH_SIZE];
    int mFourAiIspModelVersion;
    char mFourAiIspCfgBinPath[MAX_FILE_PATH_SIZE];
    int mFourAiIspWidth;
    int mFourAiIspHeight;
    int mFourAiIspTdmRxBufNum;
    int mFourAiIspMode;
    int mFourAiIspAutoSwitchEnable;
    int mFourAiIspSwitchInterval;
    int mFourAiIspSwitchCase;
    int mFourAiIspSwitchDropFrameNum;
    char mFourAiIspCfgBinPath2[MAX_FILE_PATH_SIZE];
    int mFourAiIspReserve0;
    int mFourAiIspReserve1;
    int mFourAiIspReserve2;
    // four 2nd stream
    int mFour2ndEnable;
    VI_DEV mFour2ndVipp;
    VI_CHN mFour2ndViChn;
    int mFour2ndSrcWidth;
    int mFour2ndSrcHeight;
    PIXEL_FORMAT_E mFour2ndPixelFormat;
    int mFour2ndViBufNum;
    int mFour2ndSrcFrameRate;
    VENC_CHN mFour2ndVEncChn;
    PAYLOAD_TYPE_E mFour2ndEncodeType;
    int mFour2ndEncodeWidth;
    int mFour2ndEncodeHeight;
    int mFour2ndEncodeFrameRate;
    int mFour2ndEncodeBitrate;
    int mFour2ndEncppSharpAttenCoefPer;
    BOOL mFour2ndEncppEnable;
    int mFour2ndVeRefFrameLbcMode;
    int mFour2ndKeyFrameInterval;
    char mFour2ndFilePath[MAX_FILE_PATH_SIZE];
    int mFour2ndSaveOneFileDuration;
    int mFour2ndSaveMaxFileCnt;
    // isp and ve linkage
    BOOL mIspAndVeLinkageEnable;
    BOOL mCameraAdaptiveMovingAndStaticEnable;
    int mVencLensMovingMaxQp;
    // wb yuv
    int mWbYuvEnable;
    int mWbYuvBufNum;
    int mWbYuvStartIndex;
    int mWbYuvTotalCnt;
    int mWbYuvStreamChn;
    char mWbYuvFilePath[MAX_FILE_PATH_SIZE];
    // vi timeout
    int mViTimeoutResetDisable;
    int mTestTriggerViTimeout;
    // others
    int mTestDuration;  //unit:s, 0 means infinite.
}SampleSmartIPCDemoConfig;

typedef struct VencStreamContext
{
    pthread_t mStreamThreadId;
    ISP_DEV mIsp;
    VI_DEV mVipp;
    VI_CHN mViChn;
    VI_ATTR_S mViAttr;
    VENC_CHN mVEncChn;
    VENC_CHN_ATTR_S mVEncChnAttr;
    VENC_RC_PARAM_S mVEncRcParam;
    VENC_FRAME_RATE_S mVEncFrameRateConfig;
    int mStreamDataCnt;
    FILE* mFile;
    void *priv;
    int mEncppSharpAttenCoefPer;
    int mRecordHandler;
    int mIspAndVeLinkageEnable;
    int mCameraAdaptiveMovingAndStaticEnable;
    VencHeaderData mSpsPpsInfo;
    IspApiTestCtrlConfig mIspTestCfg;
    DetMipiDeskCtrlConfig mDetMipiDeskCtrlCfg;
}VencStreamContext;

typedef struct VencOsdContext
{
    pthread_t mStreamThreadId;
    RGN_HANDLE mOverlayHandle[VENC_MAX_CHN_NUM];
    PIXEL_FORMAT_E mPixelFormat;
    void *priv;
}VencOsdContext;

typedef struct SampleSmartIPCDemoContext
{
    SampleSmartIPCDemoCmdLineParam mCmdLinePara;
    SampleSmartIPCDemoConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mbExitFlag;

    VencStreamContext mMainStream;
    VencStreamContext mMain2ndStream;
    VencStreamContext mSubStream;
    VencStreamContext mSub2ndStream;
    VencStreamContext mThreeStream;
    VencStreamContext mThree2ndStream;
    VencStreamContext mFourStream;
    VencStreamContext mFour2ndStream;

    RGN_HANDLE mOverlayDrawStreamOSDBase;
    RGB_PIC_S mRgbPic[16];

    pthread_t mWbYuvThreadId;

	pthread_t mMsgQueueThreadId;
	message_queue_t mMsgQueue;

    pthread_t mTestTriggerThreadId;

    pthread_t mAiispThreadId;
}SampleSmartIPCDemoContext;

#endif  /* _SAMPLE_SMART_IPC_DEMO_H_ */

