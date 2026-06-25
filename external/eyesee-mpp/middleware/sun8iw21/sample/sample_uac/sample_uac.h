
#ifndef _SAMPLE_UAC_H_
#define _SAMPLE_UAC_H_

#include <plat_type.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleUACCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleUACCmdLineParam;

typedef struct SampleUACConfig
{
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mFrameSize;
    int mAecEn;
    int mAnsEn;
    int mAnsMode;
    int mAgcEn;
    float agc_float_target_db;
    float agc_float_max_gain_db;
    int enable_uac1_in;
    int enable_uac1_out;
}SampleUACConfig;

typedef struct SampleUACContext
{
    int exit_flag;
    SampleUACCmdLineParam mCmdLinePara;
    SampleUACConfig mConfigPara;
}SampleUACContext;

#endif  /* _SAMPLE_UAC_H_ */
