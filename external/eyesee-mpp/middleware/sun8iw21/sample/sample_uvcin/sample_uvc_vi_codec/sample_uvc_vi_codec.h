#ifndef _SAMPLE_UVC_VI_CODEC_H_
#define _SAMPLE_UVC_VI_CODEC_H_

#include <tsemaphore.h>
#include <tmessage.h>
//#include <mm_common.h>
//#include <mm_comm_uvc.h>
//#include <mm_comm_sys.h>

#define MAX_FILE_PATH_SIZE (256)

typedef enum
{
    SampleState_Invalid,
    SampleState_Idle,
    SampleState_Executing,
} SampleState;

typedef enum {
    SampleMsgType_SetState,
    SampleMsgType_Stop,
} SampleMsgType;

typedef enum {
    PreviewSource_UVC,
    PreviewSource_VIPP,
} PreviewSourceE;

typedef struct SampleUvcViCodecCmdLineParam
{
    char strConfigFilePath[MAX_FILE_PATH_SIZE];
} SampleUvcViCodecCmdLineParam;

typedef struct SampleUvcViCodecConfig
{
    char strUvcDevName[20]; //empty string means disable uvc capture
    enum v4l2_colorspace eUvcColorSpace;
    int nUvcCaptureFrameRate;
    int nUvcCaptureWidth;
    int nUvcCaptureHeight;
    int nUvcCaptureVideoBufCnt;
    double fUvcCaptureMaxFramesizeRatio;
    int nVdecExtraFrameNum;
    PIXEL_FORMAT_E eVdecPixelFormat;
    int nVdecSubRatio; //0:disable, 1:1/2, 2:1/4, 3:1/8
    int nUvcDisplayRotate;
    int nUvcDisplayX;
    int nUvcDisplayY;
    int nUvcDisplayWidth; //0 means disable uvc display.
    int nUvcDisplayHeight;

    enum v4l2_colorspace eIspColorSpace;
    int nIspCaptureFrameRate;
    int nVippDev; //-1 means disable isp-vipp capture.
    PIXEL_FORMAT_E eVippPixelFormat;
    int nVippCaptureWidth;
    int nVippCaptureHeight;
    int nVippBufNum;
    int nSubVippDev;
    PIXEL_FORMAT_E eSubVippPixelFormat;
    int nSubVippCaptureWidth;
    int nSubVippCaptureHeight;
    int nSubVippBufNum;
    int nSubVippDisplayRotate;
    int nSubVippDisplayX;
    int nSubVippDisplayY;
    int nSubVippDisplayWidth; //0 means disable sub vipp display.
    int nSubVippDisplayHeight;

    PreviewSourceE ePreviewSource; //1:preview vipp, 0:preview uvc
    int nPreviewSwitchInterval; //0 means don't switch
    
    PAYLOAD_TYPE_E eVencType; //PT_MAX means disable venc.
    int RcMode; //0:CBR  1:VBR  2:FIXQP
    int vbrOptEn; //0: old vbr bitrate control, 1:new vbr bitrate control
    VENC_RC_PRIORITY eVbrOptRcPriority;
    VENC_QUALITY_LEVEL eVbrOptRcQualityLevel;
    
    int nUvcKeyFrameInterval;
    int nUvcVideoBitrate;
    int nUvcEncodeRotate;

    int nVippKeyFrameInterval;
    int nVippVideoBitrate;
    int nVippEncodeRotate;
    int nVippCropX;
    int nVippCropY;
    int nVippCropWidth; //0 means disable crop
    int nVippCropHeight;
    int nVippEncodeWidth;
    int nVippEncodeHeight;
    bool bVippEncodeSharpEn;
    bool bVippIsp2VeLinkEn;
    bool bVippVe2IspLinkEn;
    bool bVippRegionLinkEnable;
    bool bVippRegionLinkTexDetectEnable;
    bool bVippRegionLinkMotionDetectEnable;
    int nVippRegionLinkMotionDetectInterval;

    char strVencFilePath[MAX_FILE_PATH_SIZE]; //empty string means need not write video file.
    int nVencFileDuration; //unit:s
    int nVencFileNum;
    int nRtspNetType; //RTSP Network type, 0: "lo", 1: "eth0", 2: "br0", 3: "wlan0"
    int nRtspId; // -1 means need not rtsp.

    int nSampleRate;
    int nAiVolume;
    int nMicNum; // 0 means disable audio.
    bool bAiAec;
    bool bAiAns;
    bool bAiAgc;

    PAYLOAD_TYPE_E eAencType; //PT_MAX means disable aenc.
    bool bAencAttachAACHeader;

    char strAencFilePath[MAX_FILE_PATH_SIZE]; //empty string means need not write audio file.
    int nAencFileDuration;
    int nAencFileNum;
    
    int nTestDuration;
} SampleUvcViCodecConfig;

typedef struct VdecDoubleFrameInfoNode
{
    VIDEO_FRAME_INFO_S stMainFrame;  //0: main stream; 1 : sub stream. note: sub stream just for mjpeg vdec now!
    int nMainRefCnt;
    VIDEO_FRAME_INFO_S stSubFrame;
    int nSubRefCnt;
    struct list_head mList;
} VdecDoubleFrameInfoNode;

typedef struct DisplayFrameInfoNode
{
    VIDEO_FRAME_INFO_S stFrame;
    int nRefCnt;
    struct list_head mList;
} DisplayFrameInfoNode;

typedef struct 
{
    char strFilePath[MAX_FILE_PATH_SIZE];
    struct list_head mList;
}FilePathNode;

#define MAX_VDEC_FRAMEPAIR_NUM (4)
#define MAX_DISPLAY_FRAME_NUM (3)
typedef struct SampleUvcViCodecContext
{
    SampleUvcViCodecCmdLineParam stCmdLineParam;
    SampleUvcViCodecConfig stConfig;

    cdx_sem_t stSemExit;
    MPP_SYS_CONF_S stSysconf;

    bool bEnableUvc; //enable uvc capture.
    UVC_CHN nUvcChn;
    UVC_ATTR_S stUvcAttr;
    VDEC_CHN nVdecChn;
    VDEC_CHN_ATTR_S stVdecChnAttr;
    bool bEnableUvcDisplay;
    pthread_mutex_t stVdecFrameLock;
    struct list_head mIdleVdecFramePairList; //VdecDoubleFrameInfoNode*
    struct list_head mUsingVdecFramePairList;
    pthread_t GetUvcVdecFrameThreadId;
    message_queue_t stGetUvcVdecFrameMessageQueue;
    SampleState eGetUvcVdecFrameThreadState;

    bool bEnableIspVipp; //enable isp-vipp capture
    ISP_DEV nIspDev;
    //VI_DEV nViDev;
    VI_CHN nViChn;
    VI_ATTR_S stViAttr;
    //VI_DEV nSubViDev;
    VI_CHN nSubViChn;
    VI_ATTR_S stSubViAttr;
    bool bEnableSubVippDisplay;
    pthread_t GetVippFrameThreadId;
    message_queue_t stGetVippFrameMessageQueue;
    SampleState eGetVippFrameThreadState;

    PreviewSourceE eCurPreviewSource; //1:preview vipp, 0:preview uvc
    pthread_t PreviewSwitchThreadId;
    message_queue_t stPreviewSwitchMessageQueue;
    SampleState ePreviewSwitchThreadState;
    
    VO_DEV nVoDev;
    VO_PUB_ATTR_S stVoPubAttr;
    VO_LAYER nVoLayer;
    VO_VIDEO_LAYER_ATTR_S stUvcLayerAttr;
    VO_VIDEO_LAYER_ATTR_S stVippLayerAttr;
    VO_CHN nVOChn;
    int nG2dDevFd;
    pthread_mutex_t stDisplayFrameLock;
    struct list_head mIdleDisplayFrameList; //DisplayFrameInfoNode*
    struct list_head mUsingDisplayFrameList;

    VENC_CHN nVencChn;
    VENC_CHN_ATTR_S stUvcVencChnAttr;
    VENC_RC_PARAM_S stUvcVencRcParam;
    VENC_CHN_ATTR_S stVippVencChnAttr;
    VENC_RC_PARAM_S stVippVencRcParam;
    VENC_CROP_CFG_S stVippVencCropCfg;
    bool bCreateEncLibFlag;
    bool bEnableVideoEncode;
    pthread_t GetStreamThreadId;
    message_queue_t stGetStreamMessageQueue;
    SampleState eGetStreamThreadState;
    VencHeaderData stSpsPpsInfo;
    FILE *pVencFileFp;
    int nVencFileIndex;
    struct list_head VencFileList;    //FilePathNode
    int64_t VencFileStartPts; //unit:us
    bool bEnableWriteVideoFile;
    bool bEnableRtsp;
    char *pRtspStreamBuf;
    int nRtspStreamBufSize;

    bool bEnableAudio;
    AUDIO_DEV nAIODev;
    AIO_ATTR_S stAiAttr;
    AI_CHN nAiChn;
    //AI_CHN_ATTR_S stAiChnAttr;
    bool bEnableAudioEncode;
    AENC_CHN nAEncChn;
    AENC_CHN_ATTR_S stAEncAttr;
    //pthread_t GetAencStreamThreadId;
    //message_queue_t stGetAencStreamMessageQueue;
    //SampleState eGetAencStreamThreadState;
    FILE *pAencFileFp;
    int nAencFileIndex;
    struct list_head AencFileList;    //FilePathNode
    int64_t AencFileStartPts; //unit:us
    bool bEnableWriteAudioFile;
} SampleUvcViCodecContext;

#endif  /* _SAMPLE_UVC_VI_CODEC_H_ */

