#ifndef __SAMPLE_ENCPP_GDC_OFFLINE_H__
#define __SAMPLE_ENCPP_GDC_OFFLINE_H__

#include <vencoder.h>
#include <plat_type.h>
#include <mm_comm_video.h>

#define MAX_FILE_PATH_SIZE  (256)

typedef struct SampleEncppGdcOfflineCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleEncppGdcOfflineCmdLineParam;

typedef struct SampleEncppGdcOfflineConfig
{
    unsigned int mSrcWidth;
    unsigned int mSrcHeight;
    PIXEL_FORMAT_E mSrcFmt;
    unsigned int mDstWidth;
    unsigned int mDstHeight;
    PIXEL_FORMAT_E mDstFmt;
    BOOL mbGdcMirror;
    eGdcWarpType mGdcWarpType;
    eGdcMountType mGdcMountType;
    char mGdcLDCProLutBin[MAX_FILE_PATH_SIZE];
    char mSrcPic[MAX_FILE_PATH_SIZE];
    char mDstPic[MAX_FILE_PATH_SIZE];
}SampleEncppGdcOfflineConfig;

typedef struct SampleEncppGdcOfflineContext
{
    SampleEncppGdcOfflineConfig mConfigPara;
    SampleEncppGdcOfflineCmdLineParam mCmdLinePara;

    VIDEO_FRAME_INFO_S mSrcFrameInfo;
    VIDEO_FRAME_INFO_S mDstFrameInfo;

    unsigned int* mpGdcLdcProLutData;
    unsigned int mGdcLdcProLutDataLen;
}SampleEncppGdcOfflineContext;

#endif
