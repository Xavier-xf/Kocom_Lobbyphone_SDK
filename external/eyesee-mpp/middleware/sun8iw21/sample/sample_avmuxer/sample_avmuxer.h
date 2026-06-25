
#ifndef _SAMPLE_AVMUXER_H_
#define _SAMPLE_AVMUXER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "tmessage.h"
#include "tsemaphore.h"
#include "mpi_sys.h"
#include "mpi_vi.h"
#include <mpi_isp.h>
#include "mpi_venc.h"
#include "mpi_ai.h"
#include "mpi_aenc.h"
#include "mpi_mux.h"

#define MAX_FILE_PATH_LEN  (128)

typedef enum SampleAVMuxerMsgType
{
    Rec_NeedSetNextFd = 0,
    Rec_FileDone,
    Vi_Timeout,
    MsgQueue_Stop,
}SampleAVMuxerMsgType;

typedef struct SampleAVMuxerCmdLineParam
{
    char strConfigFilePath[MAX_FILE_PATH_LEN];
}SampleAVMuxerCmdLineParam;

typedef struct SampleAVMuxerConfig
{
    char dstVideoFile[MAX_FILE_PATH_LEN];
    bool bAddRepairInfo;
    int nMaxFrmsTagInterval;    //frames interval for repair. unit:us
    int nDstFileMaxCnt;

    int srcWidth;
    int srcHeight;
    int nSrcFrameRate;
    PIXEL_FORMAT_E eSrcPixFmt;
    enum v4l2_colorspace eColorSpace;
    int dstWidth;
    int dstHeight;
    int nViDropFrameNum;
    int nVencDropFrameNum;
    int nViBufferNum;
    int wdr_en;

    int nVippDev;
    int nVeChn;

    PAYLOAD_TYPE_E eVideoEncoderFmt;
    int nVideoFrameRate;
    int nVideoBitRate;
    int nMaxFileDuration; //unit:s
    int nTestDuration;

    eVencProductMode eProductMode;
    int nKeyFrameInterval;
    int nRcMode;
    int nGopMode;
    int nGopSize;
    /**
      use profile
      for h264: 0 -> base; 1 -> main; 2 -> high(suggested);
      for h265: 0 -> main(suggested); 1 -> main10; 2 -> sti11 
    */
    int nEncUseProfile;

    int nVbvBufferSize;  //unit:Byte
    int nVbvThreshSize;  //unit:Byte

    /* crop params */
    bool bCropEnable;
    int nCropRectX;
    int nCropRectY;
    int nCropRectWidth;
    int nCropRectHeight;

    bool bVuiTimingInfoPresentFlag;

    bool bOnlineEnable;
    int nOnlineShareBufNum;

    bool bEncppEnable;

    bool bIspAndVeLinkageEnable;

    VENC_REF_FRAME_LBC_MODE_E eVeRefFrameLbcMode;
    bool bVeRecRefBufReduceEnable;

    bool bVbrOptEnable;

    //sei params
    VencSeiEnableSettingE eSeiEnable;
    bool bSeiDataIsp;
    bool bSeiDataVipp;
    bool bSeiDataVenc;
    int nSeiFrameIntervalIspLevel1;
    int nSeiFrameIntervalIspLevel2;
    int nSeiFrameIntervalIspLevel3;
    int nSeiFrameIntervalVipp;
    int nSeiFrameIntervalVencLevel1;
    int nSeiFrameIntervalVencLevel2;

    //audio params
    int nPcmChnCnt;
    int nPcmBitWidth;
    int nPcmSampleRate;
    int nAiVolume;
    bool bAecEn;
    int nAecNlpMode; // 0,1,2, kAecNlpConservative, kAecNlpModerate, kAecNlpAggressive
    bool bAnsEn;
    int nAnsMode;
    bool bAgcEn;
    int nAgcTargetDb;
    int nAgcMaxGainDb;
    PAYLOAD_TYPE_E eAudioCodecType;
    int nBitrate;
}SampleAVMuxerConfig;

typedef struct
{
    char strFilePath[MAX_FILE_PATH_LEN];
    struct list_head mList;
}FilePathNode;

typedef struct SampleAVMuxerContext
{
    SampleAVMuxerCmdLineParam stCmdLinePara;
    SampleAVMuxerConfig stConfigPara;

    char strDstDir[MAX_FILE_PATH_LEN];    //tail don't contain '/', e.g.,/mnt/extsd/sample_virvi2venc2muxer_Files
    char strFirstFileName[MAX_FILE_PATH_LEN];
    MEDIA_FILE_FORMAT_E eFileFormat;

    cdx_sem_t stSemExit;

    MPP_SYS_CONF_S stSysConf;

    VI_ATTR_S stViAttr;
    ISP_DEV nIspDev;
    VI_DEV nViDev;
    VI_CHN nViChn;

    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_RC_PARAM_S stVencRcParam;
    VENC_CHN nVeChn;

    AUDIO_DEV nAudioDevId;
    AI_CHN nAiChn;
    AIO_ATTR_S stAioAttr;
    AENC_CHN nAEncChn;
    AENC_CHN_ATTR_S stAEncChnAttr;

    MUX_CHN_ATTR_S stMuxChnAttr;
    MUX_CHN nMuxChn;
    struct list_head stMuxerFileList;    //FilePathNode

    pthread_t nMsgQueueThreadId;
    message_queue_t stMsgQueue;

    int nResetCameraCnt;
}SampleAVMuxerContext;

#endif  /* _SAMPLE_AVMUXER_H_ */

