#ifndef _SAMPLE_ISPOSD_H_
#define _SAMPLE_ISPOSD_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_region.h>
#include <mm_comm_venc.h>
#include "rgb_ctrl.h"


#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleIspOsdCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleIspOsdCmdLineParam;

typedef struct SampleIspOsdConfig{
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mPicFormat;
    int mFrameRate;
    int mBitrate;
    PAYLOAD_TYPE_E EncoderType;

    PIXEL_FORMAT_E mBitmapFormat;
    int overlay_x;
    int overlay_y;

    int InvColEn;
    int InvColMode;
    int InvColLumThresh;
    
    char OutputFilePath[MAX_FILE_PATH_SIZE];
    int mTestDuration;  //unit:s, 0 mean infinite
}SampleIspOsdConfig;

typedef struct SampleIspOsdContext{
    SampleIspOsdCmdLineParam mCmdLinePara;
    SampleIspOsdConfig mConfigPara;
    cdx_sem_t mSemExit;

    MPP_SYS_CONF_S mSysConf;

    ISP_DEV mISPDev;
    VI_DEV mVIDev;
    VI_CHN mVIChn;

    VENC_CHN mVEChn;
    VENC_CHN_ATTR_S mVencChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    volatile BOOL mbEncThreadExitFlag;
    pthread_t mEncThreadId;
    pthread_t ispDebugThreadId;
    pthread_t ispDebugRgbThreadId;

    RGN_HANDLE mOverlayHandle;
    RGB_PIC_S  ispDebugRgb;

    FILE* mOutputFileFp;
}SampleIspOsdContext;

#endif
