/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _SAMPLE_CDR_DEMO_H_
#define _SAMPLE_CDR_DEMO_H_

#include <mm_common.h>
#include <mm_comm_video.h>
#include <mm_comm_vi.h>
#include <mm_comm_venc.h>
#include <mm_comm_mux.h>
#include <mm_comm_sys.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include "tmessage.h"

#define MAX_FILE_PATH_SIZE (256)
#define MAX_FILE_FORMAT_SIZE (16)

typedef struct SampleCDRDemoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
} SampleCDRDemoCmdLineParam;

typedef enum RecorderState
{
    REC_NOT_PREPARED = 0,
    REC_PREPARED,
    REC_RECORDING,
    REC_STOP,
    REC_ERROR,
} RECSTATE_E;

typedef enum SampleCDRDemoMsgType
{
    Rec_NeedSetNextFd = 0,
    Rec_FileDone,
    MsgQueue_Stop,
} SampleCDRDemoMsgType;

typedef struct
{
    char strFilePath[MAX_FILE_PATH_SIZE];
    struct list_head mList;
} FilePathNode;

typedef struct AudioConfig
{
    int mCaptureSampleRate;
    int mCaptureBitWitdh;
    int mCaptureChannelCnt;
    int mCaptureAnsEn;
    int mCaptureAgcEn;
    int mCaptureAecEn;
    PAYLOAD_TYPE_E mAencType;
    int mAencBitRate;
} AudioConfig;

typedef struct StreamConfig
{
    int mVIDev;
    int mViVirChn;
    int mIspDev;
    int mCapWidth;
    int mCapHeight;
    int mCapFrmRate;
    PIXEL_FORMAT_E mCapFormat;
    int mVIBufNum;
    int mEnableWDR;
    int mViStitchMode;
    int mViStitchIspChannelId;

    int mEncChn;
    int mEncOnlineEnable;
    int mEncOnlineShareBufNum;
    bool bEncIsp2VeEnable;
    bool bEncVe2IspEnable;
    PAYLOAD_TYPE_E mEncType;
    int mEncWidth;
    int mEncHeight;
    int mEncFrmRate;
    int mEncBitRate;
    int mEncRefFrameLbcMode;
    int mVeRecRefBufReduceEnable;
    int mEncRcMode;
    int mProductMode;
    int mVeRxInputBufmultiplexEnable;
} StreamConfig;

typedef struct SampleCDRDemoConfig
{
    StreamConfig mRecorderConfig;
    StreamConfig mPreviewConfig;
    AudioConfig mAudioConfig;

    int mRecorderRecDuration;
    int mRecorderRecFileCnt;
    char mRecorderRecFileFormat[MAX_FILE_FORMAT_SIZE];
    char mRecorderOutFilePath[MAX_FILE_PATH_SIZE];
    char mRecorderCurFileName[MAX_FILE_PATH_SIZE];

    BOOL mPreviewEnable;
    int mPreviewMode; // 0: rtsp; 1: hwdisplay
    int mPreviewRtspId;
    int mPreviewRtspNetType;
    int mPreviewDispX;
    int mPreviewDispY;
    int mPreviewDispWidth;
    int mPreviewDispHeight;
    int mPreviewDispDev;

    int mTakePicture;
    int mTakePicTureVIDev;
    int  mTakePictureViChn;
    int mTakePictureEncChn;
    int mTakePictureOnline;
    int mTakePictureInterval;
    int mTakePictureThumbEnable;
    int nTakePictureFileCnt; //0 means not limited.
    char mTakePictureFile[MAX_FILE_PATH_SIZE];

    BOOL mRegionLinkEnable;
    BOOL mRegionLinkTexDetectEnable;
    BOOL mRegionLinkMotionDetectEnable;
    int mRegionLinkMotionDetectInv;

    int mEncRecRefBufReduceEnable;
    int mProductMode;
    int mEncRcMode;
    int mVeRxInputBufmultiplexEnable;
    int mTestDuration;
} SampleCDRDemoConfig;


typedef struct StreamContext
{
    BOOL mbRecorderValid;
    VI_DEV mVIDev;
    VI_CHN mVIChn;
    ISP_DEV mIspDev;
    VI_ATTR_S mVIAttr;
    ISP_DEV mViStitchIspChannelId;

    VENC_CHN mVeChn;
    VENC_CHN_ATTR_S mVeChnAttr;
    VENC_RC_PARAM_S mVencRcParam;
    // int mEncppSharpAttenCoefPer;

    //MUX_GRP mMuxGrp;
    MUX_CHN mMuxChn;
    //MUX_GRP_ATTR_S mMuxGrpAttr;
    MUX_CHN_ATTR_S mMuxChnAttr;

    VO_DEV mVODev;
    VO_LAYER mUILayer;
    VO_LAYER mVOLayer;
    VO_CHN mVOChn;
    VO_VIDEO_LAYER_ATTR_S mVOLayerAttr;

    AI_CHN mAiChn;
    AUDIO_DEV mAIDevId;
    AIO_ATTR_S mAiChnAttr;
    AENC_CHN mAencChn;
    AENC_CHN_ATTR_S mAencChnAttr;

    BOOL bVencIsp2VeEnable;
    BOOL bVencVe2IspEnable;
    BOOL mRegionLinkEnable;
    BOOL mRegionLinkTexDetectEnable;
    BOOL mRegionLinkMotionDetectEnable;
    int mRegionLinkMotionDetectInv;

    int mCapFrmRate;
    int mEncFrmRate;

    void *priv;

    int mFileIdCounter;
    pthread_mutex_t mFilePathListLock;
    struct list_head mFilePathList; //FilePathNode
} StreamContext;

typedef struct AudioContext
{
    AudioConfig mAudioConfig;
    AI_CHN mAiChn;
    AIO_ATTR_S mAiChnAttr;
    AO_CHN mAoChn;
    int mAiDev;
    int mCaptureTaskExit;
    int mCaptureTaskRunning;
    pthread_t mCaptureTrdId;
    int mAoDev;
    int mPlaybackTaskExit;
    int mPlaybackTaskRunning;
    pthread_t mPlaybackTrdId;
} AudioContext;

typedef struct SampleCDRDemoContext
{
    SampleCDRDemoCmdLineParam mCmdLinePara;
    SampleCDRDemoConfig mConfigPara;
    AudioContext mAudioContext;
    StreamContext mRecorderContext;
    StreamContext mPreviewContext;
    cdx_sem_t mSemExit;
    MPP_SYS_CONF_S mSysConf;

    message_queue_t mMsgQueue;

    pthread_t mMsgQueueThreadId;
    pthread_t mPreviewGetStreamThreadId;
    pthread_t mCSIFrameThreadId;
    struct list_head PicFilePathList; //FilePathNode
    int mbExitFlag;
} SampleCDRDemoContext;

typedef struct SampleCDRDemo_MessageData
{
    StreamContext *pStreamContext;
} SampleCDRDemo_MessageData;

#endif
