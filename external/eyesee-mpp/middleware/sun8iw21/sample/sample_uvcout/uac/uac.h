#ifndef __UAC_H__
#define __UAC_H__

struct uac_config
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
};

struct uac_context
{
    int exit_flag;
    pthread_t uac1_in_task_trd, uac1_out_task_trd;
    struct uac_config mConfigPara;
};

int uac_enable(struct uac_config *config);

void uac_disable(void);

#endif  /* _SAMPLE_UAC_H_ */
