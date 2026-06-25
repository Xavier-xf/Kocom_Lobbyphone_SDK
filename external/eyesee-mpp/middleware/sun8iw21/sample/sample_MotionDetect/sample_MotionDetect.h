#ifndef _SAMPLE_MOTIONDETECT_H_
#define _SAMPLE_MOTIONDETECT_H_

#include <plat_type.h>

#define MAX_FILE_PATH_SIZE  (256)

typedef struct SampleMotionDetectCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleMotionDetectCmdLineParam;

typedef struct SampleMotionDetectZoneInfo
{
    int left_up_x;
    int left_up_y;
    int left_bottom_x;
    int left_bottom_y;
    int right_bottom_x;
    int right_bottom_y;
    int right_up_x;
    int right_up_y;
}SampleMotionDetectZoneInfo;

typedef struct SampleMotionDetectConfig
{
    int mEncoderCount;
    int mDevNum;
    PIXEL_FORMAT_E mSrcPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mSrcWidth;
    int mSrcHeight;
    int mSrcFrameRate;
    PAYLOAD_TYPE_E mEncoderType;
    int mDstWidth;
    int mDstHeight;
    int mDstFrameRate;
    int mDstBitRate;
    int mOnlineEnable;
    int mOnlineShareBufNum;
    char mOutputFilePath[MAX_FILE_PATH_SIZE];
    int mIspAndVeLinkageEnable;

    int motionAlarm_on;
    int motionAlarm_useDefaultCfgEnable;
    int motionAlarm_sensitivity;

    int motionAlarm_support_zone;
    int motionAlarm_left_up_x;
    int motionAlarm_left_up_y;
    int motionAlarm_left_bottom_x;
    int motionAlarm_left_bottom_y;
    int motionAlarm_right_bottom_x;
    int motionAlarm_right_bottom_y;
    int motionAlarm_right_up_x;
    int motionAlarm_right_up_y;

    int motionAlarm_HorizontalRegionNum;
    int motionAlarm_VerticalRegionNum;

    int motionAlarm_Threshold_High;
    int motionAlarm_Threshold_MediumHigh;
    int motionAlarm_Threshold_Default;
    int motionAlarm_Threshold_MediumLow;
    int motionAlarm_Threshold_Low;
}SampleMotionDetectConfig;

typedef struct SampleMotionDetectContext
{
    SampleMotionDetectCmdLineParam mCmdLinePara;
    SampleMotionDetectConfig mConfigPara;

    ISP_DEV mIspDev;
    VI_DEV mViDev;
    VI_CHN mViChn;
    VENC_CHN mVeChn;

    VI_ATTR_S mViAttr;
    VENC_CHN_ATTR_S mVEncChnAttr;
    VENC_RC_PARAM_S mVEncRcParam;
    VENC_FRAME_RATE_S mVencFrameRateConfig;

    FILE* mOutputFileFp;

    int mbExitFlag;
    BOOL bMotionSearchEnable;
    VencMotionSearchResult mMotionResult;
}SampleMotionDetectContext;

#endif  /* _SAMPLE_MOTIONDETECT_H_ */

