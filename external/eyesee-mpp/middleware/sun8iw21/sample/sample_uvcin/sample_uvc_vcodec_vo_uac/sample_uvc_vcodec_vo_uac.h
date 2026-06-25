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
#ifndef _SAMPLE_UVC_VCODEC_VO_UAC_H_
#define _SAMPLE_UVC_VCODEC_VO_UAC_H_

#include <tsemaphore.h>
#include <tmessage.h>
//#include <mm_common.h>
//#include <mm_comm_uvc.h>
//#include <mm_comm_sys.h>

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleUvcVcodecVoUacCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
} SampleUvcVcodecVoUacCmdLineParam;

typedef struct SampleUvcVcodecVoUacConfig
{
    char strDevName[20];
    char strUacDevName[32];
    UVC_CAPTURE_FORMAT ePicFormat;
    int nCaptureVideoBufCnt;
    int nCaptureWidth;
    int nCaptureHeight;
    int nCaptureFrameRate;
    double fCaptureMaxFramesizeRatio;
    int nVdecExtraFrameNum;

    int nDisplayMainX;
    int nDisplayMainY;
    int nDisplayMainWidth;
    int nDisplayMainHeight;

    PAYLOAD_TYPE_E eVencType;
    char vencFilePath[MAX_FILE_PATH_SIZE];
    enum v4l2_colorspace eColorSpace;
    int nKeyFrameInterval;
    eVencProductMode eProductMode;
    int RcMode; //0:CBR  1:VBR  2:FIXQP
    int vbrOptEn; //0: old vbr bitrate control, 1:new vbr bitrate control
    VENC_RC_PRIORITY eVbrOptRcPriority;
    VENC_QUALITY_LEVEL eVbrOptRcQualityLevel;
    int nVideoBitrate;

    int nTestFrameCount;

    bool bUacIn; //in host perspective. so uac_in means audio input in host, host should play audio by speaker.
    int nUacInChnCnt; //host can config pcm channel number to device.
    int nAoVolume;
    bool bUacOut;//in host perspective. so uac_out means audio output in host, host should capture audio by mic.
    int nUacOutPeriodSize;
    int nUacOutStartThresholdMultiple;
    int nSampleRate;
    int nAiPeriodSize;
    int nAiVolume;
    int nMicNum;
    bool bAiAec;
    bool bAiAns;
    bool bAiAgc;
} SampleUvcVcodecVoUacConfig;

typedef struct VdecDoubleFrameInfo
{
    VIDEO_FRAME_INFO_S mMainFrame;  //0: main stream; 1 : sub stream. note: sub stream just for mjpeg vdec now!
    int mMainRefCnt;
    VIDEO_FRAME_INFO_S mSubFrame;
    int mSubRefCnt;
}VdecDoubleFrameInfo;

#define MAX_FRAMEPAIR_ARRAY_SIZE (30)
typedef struct SampleUvcVcodecVoUacContext
{
    SampleUvcVcodecVoUacCmdLineParam stCmdLineParam;
    SampleUvcVcodecVoUacConfig stConfig;
    cdx_sem_t stSemExit;
    UVC_CHN nUvcChn;
    MPP_SYS_CONF_S stSysconf;
    VDEC_CHN nVdecChn;
    VDEC_CHN_ATTR_S stVdecChnAttr;
    pthread_mutex_t stFrameLock;
    int nFrameCounter;
    int nHoldFrameNum;
    VdecDoubleFrameInfo DoubleFrameArray[MAX_FRAMEPAIR_ARRAY_SIZE];
    int nValidDoubleFrameBufferNum;

    bool bEnableDisplay;
    VO_DEV nVoDev;
    int nUILayer;
    VO_LAYER nVoLayer;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    VO_CHN nVOChn;

    bool bEnableEncode;
    VENC_CHN nVencChn;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_RC_PARAM_S stVencRcParam;
    FILE *pVencFileFp;
    pthread_t GetStreamThreadId;
    message_queue_t stGetStreamMessageQueue;

    AUDIO_DEV nAIODev;
    AO_CHN nAoChn;
    cdx_sem_t stSemAoEof;
    AIO_ATTR_S stAiAttr;
    AI_CHN nAiChn;
    AI_CHN_ATTR_S stAiChnAttr;
    pthread_t uacInThreadId;
    message_queue_t stUacInMessageQueue;
    pthread_t uacOutThreadId;
    message_queue_t stUacOutMessageQueue;
} SampleUvcVcodecVoUacContext;

#endif  /* _SAMPLE_UVC_VCODEC_VO_UAC_H_ */

