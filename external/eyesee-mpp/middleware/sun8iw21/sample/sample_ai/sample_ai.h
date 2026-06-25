
#ifndef _SAMPLE_AI_H_
#define _SAMPLE_AI_H_

#include <plat_type.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleAICmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleAICmdLineParam;

typedef struct SampleAIConfig
{
    char mPcmFilePath[MAX_FILE_PATH_SIZE];
    int mSampleRate;
    int mMicNum;
    int mChannelCnt;
    int mBitWidth;
    int mFrameSize;
    int mCapDuraSec;
    int ai_gain;
    int mAiAnsEn;
    int mAiAnsMode;
    int mAiAgcEn;
    float mAiAgcTargetDb;
    float mAiAgcMaxGainDb;
}SampleAIConfig;

typedef struct SampleAIContext
{
    SampleAICmdLineParam mCmdLinePara;
    SampleAIConfig mConfigPara;

    FILE *mFpPcmFile;
    int mPcmSize;

    MPP_SYS_CONF_S mSysConf;
    AUDIO_DEV mAIDev;
    AI_CHN mAIChn;
    AI_CHN_ATTR_S mAIChnAttr;
    AIO_ATTR_S mAIOAttr;
}SampleAIContext;

#endif  /* _SAMPLE_AI_H_ */

