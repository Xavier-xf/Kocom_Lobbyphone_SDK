
#ifndef _SAMPLE_VDEC_H_
#define _SAMPLE_VDEC_H_

#include <plat_type.h>
#include <tsemaphore.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleVDecCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleVDecCmdLineParam;

typedef struct SampleVDecConfig
{
    char mYuvFilePath[MAX_FILE_PATH_SIZE];  //dst file
    int mJpegNum;
    char mJpegOneFilePath[MAX_FILE_PATH_SIZE]; //jpeg src file for one file
    char mJpegMultiFilePath[MAX_FILE_PATH_SIZE]; //jpeg src file for multi files
    char mH264VbsPath[MAX_FILE_PATH_SIZE];  //h264 vbs file
    char mH264LenPath[MAX_FILE_PATH_SIZE];  //h264 len file
    char mH265VbsPath[MAX_FILE_PATH_SIZE];  //h265 vbs file
    char mH265LenPath[MAX_FILE_PATH_SIZE];  //h265 len file
    PAYLOAD_TYPE_E mType;
    int mOutputPixelFormat;
    int mVdecBufSize;
    int mVideoWidth; //video stream frame size
    int mVideoHeight;
    int mVdecMaxWidth; //vdec max output frame size.
    int mVdecMaxHeight;
}SampleVDecConfig;

typedef struct SampleVDecContext
{
    SampleVDecCmdLineParam mCmdLinePara;
    SampleVDecConfig mConfigPara;

    FILE *mFpSrcFile;   //src
    FILE *mFpDstFile;   //dst

    MPP_SYS_CONF_S mSysConf;
    VDEC_CHN mVDecChn;
    VDEC_CHN_ATTR_S mVDecAttr;
}SampleVDecContext;

#endif  /* _SAMPLE_VDEC_H_ */

