#ifndef _SAMPLE_UVOICEAEC_H_
#define _SAMPLE_UVOICEAEC_H_

#include <stdbool.h>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_clock.h>
//#include <aec_lib.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleUvoiceAecCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleUvoiceAecCmdLineParam;

typedef struct SampleUvoiceAecConfig
{
    char mPcmInPath[MAX_FILE_PATH_SIZE];
    char mPcmRefPath[MAX_FILE_PATH_SIZE];
    char mPcmOutPath[MAX_FILE_PATH_SIZE];
    int mSampleRate;
    int mChannelCnt;
    int mBitWidth;
    int mRefPcmSkipMs;    //unit:ms, skip several time ref pcm data, it means advancing ref pcm.
}SampleUvoiceAecConfig;

typedef struct SampleUvoiceAecContext
{
    SampleUvoiceAecCmdLineParam mCmdLinePara;
    SampleUvoiceAecConfig mConfigPara;

    FILE *mFpPcmInFile;
    FILE *mFpPcmRefFile;
    FILE *mFpPcmOutFile;

    void *mpAecHdl; //UvoiceAec handle

    short *near_buff;                       // buffer used as internal buffer to store near data for conjunction. for aec
    unsigned int near_buff_len;             // the length of the near buffer, normally is 2 x chunkbytesize.
    short *ref_buff;                        // buffer used as internal buffer to store reference data for conjunction. for aec
    unsigned int ref_buff_len;              // the length of the ref buffer, normally is 2 x chunkbytesize.
    short *out_buff;                        // buffer used as internal buffer to store aec produced data for conjunction. for aec
    unsigned int out_buff_len;              // the length of the out buffer, normally is 2 x chunkbytesize.
}SampleUvoiceAecContext;
SampleUvoiceAecContext* createSampleUvoiceAecContext();
int freeSampleUvoiceAecContext(SampleUvoiceAecContext *pContext);

#endif  /* _SAMPLE_UVOICEAEC_H_ */

