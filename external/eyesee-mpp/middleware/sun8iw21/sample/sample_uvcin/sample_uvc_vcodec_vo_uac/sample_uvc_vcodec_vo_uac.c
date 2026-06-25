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
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_uvc_vcodec_vo_uac"
#include <utils/plat_log.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/prctl.h>

#include <alsa/asoundlib.h>

#include "vo/hwdisplay.h"
#include <confparser.h>
#include <sunxi_camera_v2.h>
#include <media_common_aio.h>
#include <mpi_sys.h>
#include <mpi_uvc.h>
#include <mpi_vdec.h>
#include <mpi_venc.h>
#include <mpi_vo.h>
#include <mpi_ao.h>
#include <mpi_ai.h>

#include "sample_uvc_vcodec_vo_uac.h"
#include "sample_uvc_vcodec_vo_uac_config.h"

#define SOUND_CARD_UAC "hw:UAC1Gadget" //"hw:UAC1Gadget", "hw:Camera", "hw:UVCUAC1"
#define PERIOD_SIZE (960)
#define PERIOD_COUNT (8)
//#define DEBUG_SAVE_UAC_CAPTURE_PCM
//#define DEBUG_SAVE_UAC_CAPTURE_PCM_FILE     "/mnt/extsd/save_uac_capture.pcm"


static SampleUvcVcodecVoUacContext *gpSampleUvcVcodecVoUacContext = NULL;

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleUvcVcodecVoUacContext != NULL)
    {
        cdx_sem_up(&gpSampleUvcVcodecVoUacContext->stSemExit);
    }
}

static int kernel_fwrite(const char *fmt, ...)
{
    FILE * stream = NULL;
    va_list ap;

    stream = fopen("/dev/kmsg", "w");
    if (!stream) {
        fprintf(stderr, "Cannot open:/dev/kmsg\n");
        return -EINVAL;
    }

    va_start(ap, fmt);
    if (vfprintf(stream, fmt, ap) == -1) {
        fprintf(stderr, "[err] vfprintf %s, ret = %d\n", fmt, errno);
        fclose(stream);
        return -EINVAL;
    }
    va_end(ap);

    fclose(stream);

    return 0;
}

int initSampleUvcVcodecVoUacContext(SampleUvcVcodecVoUacContext *pContext)
{
    int ret;
    memset(pContext, 0, sizeof *pContext);
    pContext->nUILayer = HLAY(2, 0);
    pContext->nVoLayer = MM_INVALID_LAYER;
    pContext->nVOChn = MM_INVALID_CHN;
    ret = cdx_sem_init(&pContext->stSemExit, 0);
    if (ret != 0)
    {
        aloge("fatal error! cdx sem init fail:%d", ret);
    }
    ret = pthread_mutex_init(&pContext->stFrameLock, NULL);
	if (ret!=0)
	{
        aloge("fatal error! pthread mutex init fail:%d!", ret);
	}
    int i;
    for(i=0; i< MAX_FRAMEPAIR_ARRAY_SIZE; i++)
    {
        pContext->DoubleFrameArray[i].mMainFrame.mId = -1;
        pContext->DoubleFrameArray[i].mSubFrame.mId = -1;
    }
    ret = message_create(&pContext->stGetStreamMessageQueue);
    if (ret!=0)
	{
        aloge("fatal error! message queue init fail:%d!", ret);
	}
    ret = message_create(&pContext->stUacInMessageQueue);
    if (ret!=0)
	{
        aloge("fatal error! message queue init fail:%d!", ret);
	}
    ret = message_create(&pContext->stUacOutMessageQueue);
    if (ret!=0)
	{
        aloge("fatal error! message queue init fail:%d!", ret);
	}
    cdx_sem_init(&pContext->stSemAoEof, 0);
    pContext->nUvcChn = 0;
    pContext->nVdecChn = 0;
    pContext->nVoDev = 0;
    pContext->nVencChn = 0;
    pContext->nVOChn = 0;
    pContext->nAIODev = 0;
    pContext->nAoChn = 0;
    pContext->nAiChn = 0;
    return 0;
}

int destroySampleUvcVcodecVoUacContext(SampleUvcVcodecVoUacContext *pContext)
{
    int ret;
    cdx_sem_deinit(&pContext->stSemExit);
    ret = pthread_mutex_destroy(&pContext->stFrameLock);
    if (ret != 0)
    {
        aloge("fatal error! pthread mutex destroy fail:%d", ret);
    }
    message_destroy(&pContext->stGetStreamMessageQueue);
    message_destroy(&pContext->stUacInMessageQueue);
    message_destroy(&pContext->stUacOutMessageQueue);
    cdx_sem_deinit(&pContext->stSemAoEof);
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleUvcVcodecVoUacCmdLineParam *pCmdLinePara)
{
    //alogd("sample_region input path is : %s", argv[0]);
    int ret = 0;
    int i = 1;
    memset(pCmdLinePara, 0, sizeof *pCmdLinePara);
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if((++i) >= argc)
            {
                aloge("fatal error!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error!");
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], strlen(argv[i]));
            pCmdLinePara->mConfigFilePath[strlen(argv[i])] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
             alogd("CmdLine param example:\n"
                "\t run -path /mnt/extsd/sample_uvc_vcodec_vo_uac.conf\n");
             ret = 1;
             break;
        }
        else
        {
            alogd("CmdLine param example:\n"
                "\t run -path /mnt/extsd/sample_uvc_vcodec_vo_uac.conf\n");
        }
        ++i;
    }
    return ret;
}

static int LoadSampleUvcVcodecVoUacConfig(SampleUvcVcodecVoUacConfig *pConfig, const char *conf_path)
{
    int ret = 0;
    char *ptr = NULL;
    memset(pConfig, 0, sizeof(SampleUvcVcodecVoUacConfig));
    strcpy(pConfig->strDevName, "/dev/video0");
    strcpy(pConfig->strUacDevName, SOUND_CARD_UAC);
    pConfig->ePicFormat = UVC_MJPEG;
    pConfig->nCaptureVideoBufCnt = 5;
    pConfig->nCaptureWidth = 1280;
    pConfig->nCaptureHeight = 720;
    pConfig->nCaptureFrameRate = 20;
    pConfig->fCaptureMaxFramesizeRatio = 1.0;
    pConfig->nVdecExtraFrameNum = -1;
    pConfig->nDisplayMainX = 0;
    pConfig->nDisplayMainY = 0;
    pConfig->nDisplayMainWidth = 320;
    pConfig->nDisplayMainHeight = 240;
    pConfig->eVencType = PT_H264;
    strcpy(pConfig->vencFilePath, "/mnt/extsd/uvc_host.h264");
    pConfig->eColorSpace = V4L2_COLORSPACE_JPEG;
    pConfig->nKeyFrameInterval = 100;
    pConfig->eProductMode = PRODUCT_DOORBELL;
    pConfig->RcMode = 1;
    pConfig->vbrOptEn = 1;
    pConfig->eVbrOptRcPriority = VENC_RC_RT_BIT_RATE_FIRST;
    pConfig->eVbrOptRcQualityLevel = VENC_QUALITY_LOW_LEVEL;
    pConfig->nVideoBitrate = 2*1024*1024;
    pConfig->nTestFrameCount = 0;
    pConfig->bUacIn = true;
    pConfig->nUacInChnCnt = 1;
    pConfig->nAoVolume = 90;
    pConfig->bUacOut = true;
    pConfig->nUacOutPeriodSize = PERIOD_SIZE;
    pConfig->nUacOutStartThresholdMultiple = 1;
    pConfig->nSampleRate = 16000;
    pConfig->nAiPeriodSize = 320;
    pConfig->nAiVolume = 100;
    pConfig->nMicNum = 1;
    pConfig->bAiAec = true;
    pConfig->bAiAns = true;
    pConfig->bAiAgc = true;
    if(!conf_path)
    {
        alogd("user not set config file. use default test parameter!");
    }
    else
    {
        CONFPARSER_S stConfParser;
        ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("load conf fail!");
            return FAILURE;
        }

        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_DEV_NAME, NULL);
        if(ptr)
        {
            strcpy(pConfig->strDevName, ptr);
        }
        else
        {
            aloge("fatal error! the uvc dev name is error!");
        }
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_UAC_DEV_NAME, NULL);
        if(ptr)
        {
            strcpy(pConfig->strUacDevName, ptr);
        }
        else
        {
            aloge("fatal error! the uac dev name is error!");
        }
        char* pStrPixelFormat = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_PIC_FORMAT, NULL);
        if(!strcmp(pStrPixelFormat, "UVC_MJPEG"))
        {
            pConfig->ePicFormat = UVC_MJPEG;
        }
        else if(!strcmp(pStrPixelFormat, "UVC_H264"))
        {
            pConfig->ePicFormat = UVC_H264;
        }
        else if(!strcmp(pStrPixelFormat, "UVC_YUY2"))
        {
            pConfig->ePicFormat = UVC_YUY2;
        }
        else
        {
            aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
            pConfig->ePicFormat = UVC_YUY2;
        }
        pConfig->nCaptureVideoBufCnt = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_CAPTURE_VIDEOBUFCNT, 0);
        pConfig->nCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_CAPTURE_WIDTH, 640);
        pConfig->nCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_CAPTURE_HEIGHT, 480);
        pConfig->nCaptureFrameRate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_CAPTURE_FRAMERATE, 0);
        pConfig->fCaptureMaxFramesizeRatio = GetConfParaDouble(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_CAPTURE_MAXFRAMESIZE_RATIO, 1.0);
        pConfig->nVdecExtraFrameNum = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_VDEC_EXTRA_FRAME_NUM, 0);
        pConfig->nDisplayMainX = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_DISPLAY_MAIN_X, 0);
        pConfig->nDisplayMainY = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_DISPLAY_MAIN_Y, 0);
        pConfig->nDisplayMainWidth = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_DISPLAY_MAIN_WIDTH, 0);
        pConfig->nDisplayMainHeight = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_DISPLAY_MAIN_HEIGHT, 0);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_VENC_TYPE, NULL);
        if (!strcmp(ptr, "h264"))
        {
            pConfig->eVencType = PT_H264;
        }
        else if (!strcmp(ptr, "h265"))
        {
            pConfig->eVencType = PT_H265;
        }
        else if (!strcmp(ptr, "mjpeg"))
        {
            pConfig->eVencType = PT_MJPEG;
        }
        else
        {
            pConfig->eVencType = PT_MAX;
        }
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_VENC_FILE_PATH, NULL);
        if (ptr != NULL)
        {
            strncpy(pConfig->vencFilePath, ptr, MAX_FILE_PATH_SIZE-1);
        }
        else
        {
            aloge("fatal error! null pointer of venc file path?");
        }
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_COLOR_SPACE, NULL);
        if (!strcmp(ptr, "jpeg"))
        {
            pConfig->eColorSpace = V4L2_COLORSPACE_JPEG;
        }
        else if (!strcmp(ptr, "rec709"))
        {
            pConfig->eColorSpace = V4L2_COLORSPACE_REC709;
        }
        else if (!strcmp(ptr, "rec709_part_range"))
        {
            pConfig->eColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
        }
        else
        {
            pConfig->eColorSpace = V4L2_COLORSPACE_JPEG;
        }
        pConfig->nKeyFrameInterval = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_KEY_FRAME_INTERVAL, 0);
        pConfig->eProductMode = (eVencProductMode)GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_PRODUCT_MODE, 0);
        pConfig->RcMode = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_RC_MODE, 0);
        pConfig->vbrOptEn = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_VBR_OPT_EN, 0);
        pConfig->eVbrOptRcPriority = (VENC_RC_PRIORITY)GetConfParaInt(&stConfParser,
            SAMPLE_UVC_VCODEC_VO_UAC_KEY_VBR_OPT_RC_PRIORITY, 0);
        pConfig->eVbrOptRcQualityLevel = (VENC_QUALITY_LEVEL)GetConfParaInt(&stConfParser,
            SAMPLE_UVC_VCODEC_VO_UAC_KEY_VBR_OPT_RC_QUALITYLEVEL, 0);
        pConfig->nVideoBitrate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_VIDEO_BITRATE, 0);
        pConfig->nTestFrameCount = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_TEST_FRAME_COUNT, 0);
        pConfig->bUacIn = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_UAC_IN, 0);
        pConfig->nUacInChnCnt = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_UAC_IN_CHNS, 0);
        pConfig->nAoVolume = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_AO_VOLUME, 0);
        pConfig->bUacOut = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_UAC_OUT, 0);
        pConfig->nUacOutPeriodSize = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_UAC_OUT_PERIOD_SIZE, 0);
        pConfig->nUacOutStartThresholdMultiple = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_UAC_OUT_START_THRESHOLD_MULTIPLE, 0);
        pConfig->nSampleRate = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_SAMPLE_RATE, 0);
        pConfig->nAiPeriodSize = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_AI_PERIODSIZE, 0);
        pConfig->nAiVolume = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_AI_VOLUME, 0);
        pConfig->nMicNum = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_MIC_NUM, 0);
        pConfig->bAiAec = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_AEC_EN, 0);
        pConfig->bAiAns = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_ANS_EN, 0);
        pConfig->bAiAgc = GetConfParaInt(&stConfParser, SAMPLE_UVC_VCODEC_VO_UAC_KEY_AGC_EN, 0);

        destroyConfParser(&stConfParser);
    }
    alogd("uvcDev:%s, uvcVideoParam[0x%x-%dx%d-%d-%d], uacDev:%s, uacInAudioParam[%d-%d-%d-%d]-%d-%d, localAudioCapParam[%d]",
        pConfig->strDevName, pConfig->ePicFormat, pConfig->nCaptureWidth, pConfig->nCaptureHeight, pConfig->nCaptureFrameRate,
        pConfig->nCaptureVideoBufCnt, pConfig->strUacDevName, pConfig->bUacIn, pConfig->nSampleRate, pConfig->nUacInChnCnt,
        pConfig->nAoVolume, pConfig->nUacOutPeriodSize, pConfig->nUacOutStartThresholdMultiple, pConfig->nAiPeriodSize);
    return 0;
}

/**
 *
 * @return suffix of array, if not find, return -1.
 */
int FindFrameIdInArray(SampleUvcVcodecVoUacContext *pContext, unsigned int nFrameId)
{
    int suffix = -1;
    int matchNum = 0;
    int i;
    for(i=0; i<MAX_FRAMEPAIR_ARRAY_SIZE; i++)
    {
        if(nFrameId == pContext->DoubleFrameArray[i].mMainFrame.mId)
        {
            if(0 == matchNum)
            {
                suffix = i;
            }
            else
            {
                aloge("fatal error! already match num[%d], current suffix[%d], id[%d]", matchNum, i, nFrameId);
            }
            matchNum++;
        }
    }
    return suffix;
}

int releaseVideoFrameToVdecChn(SampleUvcVcodecVoUacContext *pContext, int nFrameId)
{
    ERRORTYPE ret;
    pthread_mutex_lock(&pContext->stFrameLock);
    VdecDoubleFrameInfo *pDbFramePair = NULL;
    int suffix = FindFrameIdInArray(pContext, nFrameId);
    if(suffix >= 0)
    {
        pDbFramePair = &pContext->DoubleFrameArray[suffix];
    }
    else
    {
        aloge("fatal error! why not find frameId[%d]?", nFrameId);
        pthread_mutex_unlock(&pContext->stFrameLock);
        return -1;
    }
    pDbFramePair->mMainRefCnt--;
    if(pDbFramePair->mMainRefCnt < 0)
    {
        aloge("fatal error! mainRefCnt[%d] wrong!", pDbFramePair->mMainRefCnt);
    }
    if(0 == pDbFramePair->mMainRefCnt)
    {
        ret = AW_MPI_VDEC_ReleaseImage(pContext->nVdecChn, &pDbFramePair->mMainFrame);
        if(SUCCESS == ret)
        {
            pContext->nHoldFrameNum--;
            if(pContext->nHoldFrameNum < 0)
            {
                aloge("fatal error! holdframe num[%d] < 0, check code!", pContext->nHoldFrameNum);
            }
        }
        else
        {
            aloge("fatal error! why release frame to vdecVhn[%d] fail[0x%x]?", pContext->nVdecChn, ret);
        }
    }
    pthread_mutex_unlock(&pContext->stFrameLock);
    return 0;
}

static ERRORTYPE SampleUvcVcodecVoUac_MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event,
    void *pEventData)
{
    int result;
    ERRORTYPE ret = SUCCESS;
    SampleUvcVcodecVoUacContext *pContext = (SampleUvcVcodecVoUacContext*)cookie;
    if(MOD_ID_VOU == pChn->mModId)
    {
        if(!(pChn->mDevId == pContext->nVoLayer && pChn->mChnId == pContext->nVOChn))
        {
            aloge("fatal error! voChn[%d-%d] != [%d-%d]", pChn->mDevId, pChn->mChnId, pContext->nVoLayer, pContext->nVOChn);
        }
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                result = releaseVideoFrameToVdecChn(pContext, pVideoFrameInfo->mId);
                if (result != 0)
                {
                    aloge("fatal error! voChn[%d-%d] release frameId[%d] fail:%d", pChn->mDevId, pChn->mChnId, pVideoFrameInfo->mId, result);
                    ret = FAILURE;
                }
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("voChn[%d-%d] report video display size[%dx%d]", pChn->mDevId, pChn->mChnId, pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                aloge("voChn[%d-%d] report rendering start", pChn->mDevId, pChn->mChnId);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x-0x%x-0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN nVEncChn = pChn->mChnId;
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pVideoFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                if(pVideoFrameInfo != NULL)
                {
                    result = releaseVideoFrameToVdecChn(pContext, pVideoFrameInfo->mId);
                    if (result != 0)
                    {
                        aloge("fatal error! vencChn[%d] release frameId[%d] fail:%d", pChn->mChnId, pVideoFrameInfo->mId, result);
                        ret = FAILURE;
                    }
                }
                break;
            }
            default:
            {
                aloge("fatal error! vencChn[%d] receive unknown event:%d", pChn->mChnId, event);
                ret = ERR_VENC_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else if (MOD_ID_AO == pChn->mModId)
    {
        if (pChn->mChnId != pContext->nAoChn)
        {
            aloge("fatal error! AO chnId[%d]!=[%d]", pChn->mChnId, pContext->nAoChn);
        }
        switch(event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("aoChn[%d] notify APP that play complete!", pChn->mChnId);
                cdx_sem_up(&pContext->stSemAoEof);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[%d-%d-%d]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_AO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else if (MOD_ID_AI == pChn->mModId)
    {
        if(pChn->mChnId != pContext->nAiChn)
        {
            aloge("fatal error! AI chnId[%d]!=[%d]", pChn->mChnId, pContext->nAiChn);
        }
        switch(event)
        {
            case MPP_EVENT_CAPTURE_AUDIO_DATA:
            {
                AISendDataInfo *pDataInfo = (AISendDataInfo*)pEventData;
                //alogd("AIChannel transport pcm:%d bytes, ignore:%d, pts:%lldms", pDataInfo->mLen, pDataInfo->mbIgnore, pDataInfo->mPts/1000);
                break;
            }
            default:
            {
                aloge("fatal error! aiChn[%d] receive unknown event[0x%x]!", pChn->mChnId, event);
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! why modId[0x%x]?", pChn->mModId);
        ret = FAILURE;
    }
    return ret;
}

static ERRORTYPE configVdecChnAttr(SampleUvcVcodecVoUacContext *pContext)
{
    memset(&pContext->stVdecChnAttr, 0, sizeof(pContext->stVdecChnAttr));
    PAYLOAD_TYPE_E eUvcEncodeType;
    if (UVC_MJPEG == pContext->stConfig.ePicFormat)
    {
        eUvcEncodeType = PT_MJPEG;
    }
    else if(UVC_H264 == pContext->stConfig.ePicFormat)
    {
        eUvcEncodeType = PT_H264;
    }
    else
    {
        aloge("fatal error! uvc encode type[0x%x] is not compressed type!", pContext->stConfig.ePicFormat);
        eUvcEncodeType = PT_MAX;
    }
    pContext->stVdecChnAttr.mType = eUvcEncodeType;
    pContext->stVdecChnAttr.mBufSize = AWALIGN(pContext->stConfig.nCaptureWidth*pContext->stConfig.nCaptureHeight*3/2,1024);
    pContext->stVdecChnAttr.mPicWidth = 0;
    pContext->stVdecChnAttr.mPicHeight = 0;
    pContext->stVdecChnAttr.mInitRotation = ROTATE_NONE;
    pContext->stVdecChnAttr.mOutputPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pContext->stVdecChnAttr.mVdecVideoAttr.mMode = VIDEO_MODE_FRAME;
    pContext->stVdecChnAttr.mVdecVideoAttr.mSupportBFrame = 1;
    if (pContext->stConfig.nVdecExtraFrameNum >= 0)
    {
        pContext->stVdecChnAttr.bEnableExtraFrameNum = TRUE;
        pContext->stVdecChnAttr.mExtraFrameNum = pContext->stConfig.nVdecExtraFrameNum;
    }
    return SUCCESS;
}

static ERRORTYPE configVencChnAttr(SampleUvcVcodecVoUacContext *pContext)
{
    memset(&pContext->stVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    pContext->stVencChnAttr.VeAttr.Type = pContext->stConfig.eVencType;
    if (PT_H264 == pContext->stVencChnAttr.VeAttr.Type)
    {
        pContext->stVencChnAttr.VeAttr.AttrH264e.BufSize = 0;
        pContext->stVencChnAttr.VeAttr.AttrH264e.mThreshSize = pContext->stConfig.nVideoBitrate/8/pContext->stConfig.nCaptureFrameRate*15;
        pContext->stVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->stVencChnAttr.VeAttr.AttrH264e.Profile = 2;
        pContext->stVencChnAttr.VeAttr.AttrH264e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->stConfig.nCaptureWidth;
        pContext->stVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->stConfig.nCaptureHeight;
        pContext->stVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
    }
    else if (PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
    {
        pContext->stVencChnAttr.VeAttr.AttrH265e.mBufSize = 0;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mThreshSize = pContext->stConfig.nVideoBitrate/8/pContext->stConfig.nCaptureFrameRate*15;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->stConfig.nCaptureWidth;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->stConfig.nCaptureHeight;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
    }
    else if (PT_MJPEG == pContext->stVencChnAttr.VeAttr.Type)
    {
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mBufSize = 0;
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = 0;
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->stConfig.nCaptureWidth;
        pContext->stVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->stConfig.nCaptureHeight;
    }
    pContext->stVencChnAttr.VeAttr.MaxKeyInterval = pContext->stConfig.nKeyFrameInterval;
    //mpi_vdec output width and height are all 32 align, so mpi_venc must config to 32 align
    pContext->stVencChnAttr.VeAttr.SrcPicWidth  = AWALIGN(pContext->stConfig.nCaptureWidth, 32);
    pContext->stVencChnAttr.VeAttr.SrcPicHeight = AWALIGN(pContext->stConfig.nCaptureHeight, 32);
    pContext->stVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->stVencChnAttr.VeAttr.PixelFormat = pContext->stVdecChnAttr.mOutputPixelFormat;
    pContext->stVencChnAttr.VeAttr.mColorSpace = pContext->stConfig.eColorSpace;
    alogd("pixfmt:0x%x, colorSpace:0x%x", pContext->stVencChnAttr.VeAttr.PixelFormat, pContext->stVencChnAttr.VeAttr.mColorSpace);
    pContext->stVencChnAttr.VeAttr.Rotate = ROTATE_NONE;
    pContext->stVencChnAttr.VeAttr.mDropFrameNum = 0;
    pContext->stVencChnAttr.VeAttr.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT;
    alogd("VeRefFrameLbcMode:%d", pContext->stVencChnAttr.VeAttr.mVeRefFrameLbcMode);
    pContext->stVencChnAttr.VeAttr.mVeRecRefBufReduceEnable = 0;
    alogd("VeRecRefBufReduceEnable:%d", pContext->stVencChnAttr.VeAttr.mVeRecRefBufReduceEnable);
    pContext->stVencChnAttr.VeAttr.mVbrOptEnable = pContext->stConfig.vbrOptEn;
    alogd("VbrOptEnable:%d", pContext->stVencChnAttr.VeAttr.mVbrOptEnable);
    if (PT_H264 == pContext->stVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfig.RcMode)
        {
        case 1:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
            pContext->stVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->stConfig.nVideoBitrate;
            pContext->stVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pContext->stConfig.nCaptureFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = pContext->stConfig.nCaptureFrameRate;
            break;
        case 2:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
            pContext->stVencChnAttr.RcAttr.mAttrH264FixQp.mIQp = 35;
            pContext->stVencChnAttr.RcAttr.mAttrH264FixQp.mPQp = 35;
            pContext->stVencChnAttr.RcAttr.mAttrH264FixQp.mSrcFrmRate = pContext->stConfig.nCaptureFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH264FixQp.mDstFrmRate = pContext->stConfig.nCaptureFrameRate;
            break;
        case 0:
        default:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
            pContext->stVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->stConfig.nVideoBitrate;
            pContext->stVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pContext->stConfig.nCaptureFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = pContext->stConfig.nCaptureFrameRate;
            break;
        }
    }
    else if (PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfig.RcMode)
        {
        case 1:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
            pContext->stVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->stConfig.nVideoBitrate;
            pContext->stVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pContext->stConfig.nCaptureFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = pContext->stConfig.nCaptureFrameRate;
            break;
        case 2:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
            pContext->stVencChnAttr.RcAttr.mAttrH265FixQp.mIQp = 35;
            pContext->stVencChnAttr.RcAttr.mAttrH265FixQp.mPQp = 35;
            pContext->stVencChnAttr.RcAttr.mAttrH265FixQp.mSrcFrmRate = pContext->stConfig.nCaptureFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH265FixQp.mDstFrmRate = pContext->stConfig.nCaptureFrameRate;
            break;
        case 0:
        default:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
            pContext->stVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->stConfig.nVideoBitrate;
            pContext->stVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pContext->stConfig.nCaptureFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = pContext->stConfig.nCaptureFrameRate;
            break;
        }
    }
    else if (PT_MJPEG == pContext->stVencChnAttr.VeAttr.Type)
    {
        switch (pContext->stConfig.RcMode)
        {
        case 2:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            break;
        case 0:
        default:
            pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->stConfig.nVideoBitrate;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pContext->stConfig.nCaptureFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = pContext->stConfig.nCaptureFrameRate;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.bitRateMax = (int)((float)pContext->stConfig.nVideoBitrate*1.2);
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.bitRateMin = (int)((float)pContext->stConfig.nVideoBitrate*0.8);
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.fRangeRatioTh = 0.05;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nQualityTh = 85;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nMinQuality = 10;
            pContext->stVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRateRange.nMaxQuality = 100;
            break;
        }
    }
    pContext->stVencChnAttr.RcAttr.mProductMode = pContext->stConfig.eProductMode;
    pContext->stVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->stVencChnAttr.GopAttr.mGopSize = 2;
    pContext->stVencChnAttr.EncppAttr.eEncppSharpSetting = VencEncppSharp_Disable;

    memset(&pContext->stVencRcParam, 0, sizeof(VENC_RC_PARAM_S));
    if (VENC_RC_MODE_H264CBR == pContext->stVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stVencRcParam.ParamH264Cbr.mMaxQp = 45;
        pContext->stVencRcParam.ParamH264Cbr.mMinQp = 25;
        pContext->stVencRcParam.ParamH264Cbr.mMaxPqp = 45;
        pContext->stVencRcParam.ParamH264Cbr.mMinPqp = 25;
        pContext->stVencRcParam.ParamH264Cbr.mQpInit = 37;
        pContext->stVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 1;
    }
    else if (VENC_RC_MODE_H264VBR == pContext->stVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stVencRcParam.ParamH264Vbr.mMaxQp = 45;
        pContext->stVencRcParam.ParamH264Vbr.mMinQp = 25;
        pContext->stVencRcParam.ParamH264Vbr.mMaxPqp = 45;
        pContext->stVencRcParam.ParamH264Vbr.mMinPqp = 25;
        pContext->stVencRcParam.ParamH264Vbr.mQpInit = 37;
        pContext->stVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 1;
        pContext->stVencRcParam.ParamH264Vbr.mMovingTh = 20;
        pContext->stVencRcParam.ParamH264Vbr.mQuality = 5;
        pContext->stVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 15;
        pContext->stVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    }
    else if (VENC_RC_MODE_H265CBR == pContext->stVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stVencRcParam.ParamH265Cbr.mMaxQp = 45;
        pContext->stVencRcParam.ParamH265Cbr.mMinQp = 25;
        pContext->stVencRcParam.ParamH265Cbr.mMaxPqp = 45;
        pContext->stVencRcParam.ParamH265Cbr.mMinPqp = 25;
        pContext->stVencRcParam.ParamH265Cbr.mQpInit = 37;
        pContext->stVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 1;
    }
    else if (VENC_RC_MODE_H265VBR == pContext->stVencChnAttr.RcAttr.mRcMode)
    {
        pContext->stVencRcParam.ParamH265Vbr.mMaxQp = 45;
        pContext->stVencRcParam.ParamH265Vbr.mMinQp = 25;
        pContext->stVencRcParam.ParamH265Vbr.mMaxPqp = 45;
        pContext->stVencRcParam.ParamH265Vbr.mMinPqp = 25;
        pContext->stVencRcParam.ParamH265Vbr.mQpInit = 37;
        pContext->stVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 1;
        pContext->stVencRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->stVencRcParam.ParamH265Vbr.mQuality = 5;
        pContext->stVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 15;
        pContext->stVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }

    return SUCCESS;
}

static void* GetStreamThread(void *pThreadData)
{
    SampleUvcVcodecVoUacContext *pContext = (SampleUvcVcodecVoUacContext *)pThreadData;
    char strThreadName[32];
    sprintf(strThreadName, "GetStreamThd[%d]", pContext->nVencChn);
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);
    alogd("vencChn[%d] GetStream thread run", pContext->nVencChn);
    CompInternalMsgType cmd;
    message_t stMsg;
    ERRORTYPE eRet;
    pContext->pVencFileFp = fopen(pContext->stConfig.vencFilePath, "wb");
    if (pContext->pVencFileFp != NULL)
    {
        aloge("fatal error! open filepath[%s] fail", pContext->stConfig.vencFilePath);
    }
    //set spspps
    VencHeaderData SpsPpsInfo;
    if (PT_H264 == pContext->stConfig.eVencType)
    {
        eRet = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->nVencChn, &SpsPpsInfo);
        if (SUCCESS == eRet)
        {
            if(SpsPpsInfo.nLength > 0)
            {
                fwrite(SpsPpsInfo.pBuffer, 1, SpsPpsInfo.nLength, pContext->pVencFileFp);
            }
        }
        else
        {
            aloge("fatal error! vencChn[%d] get H264SpsPpsInfo failed[0x%x]!", pContext->nVencChn, eRet);
        }
    }
    else if (PT_H265 == pContext->stConfig.eVencType)
    {
        eRet = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->nVencChn, &SpsPpsInfo);
        if (SUCCESS == eRet)
        {
            if (SpsPpsInfo.nLength > 0)
            {
                fwrite(SpsPpsInfo.pBuffer, 1, SpsPpsInfo.nLength, pContext->pVencFileFp);
            }
        }
        else
        {
            aloge("fatal error! vencChn[%d] get H265SpsPpsInfo failed[0x%x]!", pContext->nVencChn, eRet);
        }
    }
    else
    {
        aloge("fatal error! vencChn[%d] other encode type[%d]?", pContext->nVencChn, pContext->stConfig.eVencType);
    }
    VENC_STREAM_S stVencFrame;
    VENC_PACK_S stVencPack;
    stVencFrame.mPackCount = 1;
    stVencFrame.mpPack = &stVencPack;
    while(1)
    {
        if (get_message(&pContext->stGetStreamMessageQueue, &stMsg) == 0)
        {
            cmd = stMsg.command;
            if (Stop == cmd)
            {
                alogd("vencChn[%d] GetStream thread receive stop command", pContext->nVencChn);
                break;
            }
            else
            {
                aloge("fatal error! vencChn[%d] GetStream thread receive unknown cmd:0x%x", pContext->nVencChn, cmd);
            }
        }
        eRet = AW_MPI_VENC_GetStream(pContext->nVencChn, &stVencFrame, 200);
        if (SUCCESS == eRet)
        {
            alogv("Venc stream addr %px-%px-%px, Len %d-%d-%d", stVencFrame.mpPack->mpAddr0, stVencFrame.mpPack->mpAddr1,
                stVencFrame.mpPack->mpAddr2, stVencFrame.mpPack->mLen0, stVencFrame.mpPack->mLen1, stVencFrame.mpPack->mLen2);
            if(stVencFrame.mpPack->mLen0 > 0)
            {
                fwrite(stVencFrame.mpPack->mpAddr0, 1, stVencFrame.mpPack->mLen0, pContext->pVencFileFp);
            }
            if(stVencFrame.mpPack->mLen1 > 0)
            {
                fwrite(stVencFrame.mpPack->mpAddr1, 1, stVencFrame.mpPack->mLen1, pContext->pVencFileFp);
            }
            eRet = AW_MPI_VENC_ReleaseStream(pContext->nVencChn, &stVencFrame);
            if(eRet != SUCCESS)
            {
                aloge("fatal error! vencChn[%d] releaseFrame failed:0x%x", pContext->nVencChn, eRet);
            }
        }
        else
        {
            alogw("Be careful! vencChn[%d] get stream fail:0x%x, maybe timeout", pContext->nVencChn, eRet);
        }
    }
    alogd("vencChn[%d] GetStream thread exit", pContext->nVencChn);
    return (void*)SUCCESS;
}

/**
  In host perspective, uac in means get pcm from uac sound card, and need play pcm locally.
*/
static void *uac1InThread(void *thread_data)
{
    int result = 0;
    int ret;
    ERRORTYPE eRet;
    SampleUvcVcodecVoUacContext *pContext = (SampleUvcVcodecVoUacContext *)thread_data;

    //1. prepare uac pcm handle.
    snd_pcm_info_t *info;
    snd_pcm_t *pUacPcmHandle = NULL;
    int open_mode = 0;
    // open_mode |= SND_PCM_NO_AUTO_RESAMPLE;  // not to used the auto resample
    ret = snd_pcm_open(&pUacPcmHandle, pContext->stConfig.strUacDevName, SND_PCM_STREAM_CAPTURE, open_mode);
    if (ret < 0)
    {
        aloge("fatal error! PCM_handle[%s] open for capture error: %s. Maybe uac not support capture, exit now.",
            pContext->stConfig.strUacDevName, snd_strerror(ret));
        result = -1;
        goto _exit;
    }
    snd_pcm_info_alloca(&info);
    ret = snd_pcm_info(pUacPcmHandle, info);
    if (ret < 0)
    {
        aloge("fatal error! snd_pcm_info error: %s", snd_strerror(ret));
    }

    /* HW params */
    snd_pcm_hw_params_t *params;
    snd_pcm_format_t pcmFormat = SND_PCM_FORMAT_S16_LE;
    snd_pcm_hw_params_alloca(&params);
    ret = snd_pcm_hw_params_any(pUacPcmHandle, params);
    if (ret < 0)
    {
        aloge("fatal error! Broken configuration for this PCM: no configurations available");
    }
    ret = snd_pcm_hw_params_set_access(pUacPcmHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0)
    {
        aloge("fatal error! Access type not available");
    }
    ret = snd_pcm_hw_params_set_format(pUacPcmHandle, params, pcmFormat);
    if (ret < 0)
    {
        aloge("fatal error! Sample format not available");
    }
    ret = snd_pcm_hw_params_set_channels(pUacPcmHandle, params, pContext->stConfig.nUacInChnCnt);
    if (ret < 0)
    {
        aloge("fatal error! Channels count[%d] not available", pContext->stConfig.nUacInChnCnt);
    }
    unsigned int rate = pContext->stConfig.nSampleRate;
    ret = snd_pcm_hw_params_set_rate_near(pUacPcmHandle, params, &rate, NULL);
    if (ret < 0)
    {
        aloge("fatal error! set_rate_near error!");
    }
    if (rate != pContext->stConfig.nSampleRate)
    {
        aloge("fatal error! required sample_rate %d is not supported, use %d instead", pContext->stConfig.nSampleRate, rate);
    }
    snd_pcm_uframes_t periodSize = pContext->stConfig.nUacOutPeriodSize;
    ret = snd_pcm_hw_params_set_period_size_near(pUacPcmHandle, params, &periodSize, NULL);
    if (ret < 0)
    {
        aloge("fatal error! set_period_size_near error!");
    }
    if(periodSize != pContext->stConfig.nUacOutPeriodSize)
    {
        aloge("fatal error! periodSize change:%d->%lu", pContext->stConfig.nUacOutPeriodSize, periodSize);
    }
    snd_pcm_uframes_t bufferSize = periodSize * PERIOD_COUNT;
    ret = snd_pcm_hw_params_set_buffer_size_near(pUacPcmHandle, params, &bufferSize);
    if (ret < 0)
    {
        aloge("fatal error! set_buffer_size_near error!");
    }
    if(bufferSize != periodSize * PERIOD_COUNT)
    {
        aloge("fatal error! bufferSize change:%lu->%lu", periodSize * PERIOD_COUNT, bufferSize);
    }
    ret = snd_pcm_hw_params(pUacPcmHandle, params);
    if (ret < 0)
    {
        aloge("fatal error! Unable to install hw params");
    }

    snd_pcm_uframes_t periodSizeFinal = 0;
    ret = snd_pcm_hw_params_get_period_size(params, &periodSizeFinal, NULL);
    if (0 == ret)
    {
        if (periodSizeFinal != periodSize)
        {
            aloge("fatal error! uac_capture periodSize [%d!=%d]", periodSizeFinal, periodSize);
        }
    }
    else
    {
        aloge("fatal error! get period size fail:%d", ret);
    }
    snd_pcm_uframes_t bufferSizeFinal = 0;
    ret = snd_pcm_hw_params_get_buffer_size(params, &bufferSizeFinal);
    if (0 == ret)
    {
        if (bufferSizeFinal != bufferSize)
        {
            aloge("fatal error! uac_capture bufferSize [%d!=%d]", bufferSizeFinal, bufferSize);
        }
    }
    else
    {
        aloge("fatal error! get buffer size fail:%d", ret);
    }
    int bitsPerSample = snd_pcm_format_physical_width(pcmFormat);
    int significantBitsPerSample = snd_pcm_format_width(pcmFormat);
    int nAlsaFrameBytes = bitsPerSample * pContext->stConfig.nUacInChnCnt / 8;
    int periodBytes = periodSize * nAlsaFrameBytes;
    alogd("----------------ALSA setting, pcm_name:%s, pcm_stream:%d----------------", snd_pcm_name(pUacPcmHandle),
        snd_pcm_stream(pUacPcmHandle));
    alogd(">>Channels:   %4d, BitWidth:  %4d,phsical_w:%4d, SampRate:   %4d", pContext->stConfig.nUacInChnCnt,
        significantBitsPerSample, bitsPerSample, rate);
    alogd(">>ChunkBytes: %4d, ChunkSize: %4d, BufferSize: %4d", periodBytes, periodSize, bufferSize);

    /* SW params */
    snd_pcm_sw_params_t *sw_params;
    snd_pcm_sw_params_alloca(&sw_params);
    ret = snd_pcm_sw_params_current(pUacPcmHandle, sw_params);
    if (ret != 0)
    {
        aloge("fatal error! sw params current fail");
    }
    snd_pcm_sw_params_set_start_threshold(pUacPcmHandle, sw_params, 1);
    snd_pcm_sw_params_set_stop_threshold(pUacPcmHandle, sw_params, bufferSize);
    snd_pcm_sw_params_set_avail_min(pUacPcmHandle, sw_params, periodSize);
    ret = snd_pcm_sw_params(pUacPcmHandle, sw_params);
    if (ret < 0)
    {
        aloge("fatal error! Unable to install sw prams!");
    }

    ret = snd_pcm_prepare(pUacPcmHandle);
    if (ret < 0)
    {
        aloge("fatal error! pcm prepare fail:%d!", ret);
    }

    //2. prepare local mpi_ao
    eRet = AW_MPI_AO_SetDevVolume(pContext->nAIODev, pContext->stConfig.nAoVolume);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! aoDev[%d] set volume[%d] fail:0x%x!", pContext->nAIODev, pContext->stConfig.nAoVolume, eRet);
    }
    eRet = AW_MPI_AO_CreateChn(pContext->nAIODev, pContext->nAoChn);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! create aoChn[%d-%d] fail:0x%x!", pContext->nAIODev, pContext->nAoChn, eRet);
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void *)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleUvcVcodecVoUac_MPPCallbackWrapper;
    AW_MPI_AO_RegisterCallback(pContext->nAIODev, pContext->nAoChn, &cbInfo);
    eRet = AW_MPI_AO_StartChn(pContext->nAIODev, pContext->nAoChn);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! aoChn[%d-%d] start fail:0x%x!", pContext->nAIODev, pContext->nAoChn, eRet);
    }
    cdx_sem_reset(&pContext->stSemAoEof);

    //3. start to read pcm from uac, send pcm to mpi_ao to play.
    int capture_data_len = periodBytes;
    char *capture_data = malloc(capture_data_len);
    if (!capture_data)
    {
        aloge("fatal error! malloc capture buffer fail!");
    }
    message_t stMsg;
    snd_pcm_sframes_t readRet;
    int nReadBytes;
    AUDIO_FRAME_S stAudioFrame;
    memset(&stAudioFrame, 0, sizeof(AUDIO_FRAME_S));
    stAudioFrame.mBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(bitsPerSample);  //key: need to set info for every frm
    stAudioFrame.mSoundmode = (pContext->stConfig.nUacInChnCnt==1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    stAudioFrame.mSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(rate);
    stAudioFrame.mpAddr = (void *)capture_data;
#ifdef DEBUG_SAVE_UAC_CAPTURE_PCM
    FILE *fp = fopen(DEBUG_SAVE_UAC_CAPTURE_PCM_FILE, "wb");
#endif
    while (1)
    {
        if (get_message(&pContext->stUacInMessageQueue, &stMsg) == 0)
        {
            if (Stop == stMsg.command)
            {
                alogd("uacIn thread receive stop command");
                break;
            }
            else
            {
                aloge("fatal error! uacIn thread receive unknown cmd:0x%x", stMsg.command);
            }
        }

        readRet = snd_pcm_readi(pUacPcmHandle, capture_data, periodSize);
        if (readRet >= 0)
        {
            if (readRet < periodSize)
            {
                aloge("fatal error! why read less pcm alsaFrames?[%d<%d]", readRet < periodSize);
            }
            nReadBytes = readRet * nAlsaFrameBytes;
        }
        else
        {
            if (-EAGAIN == readRet)
            {
                aloge("fatal error! alsa eagain:(%s)!", strerror(errno));
                snd_pcm_wait(pUacPcmHandle, 100);
            }
            else if (-EPIPE == readRet)
            {
                aloge("fatal error! alsa_overflow_xrun:(%s)!", strerror(errno));
                snd_pcm_prepare(pUacPcmHandle);
            }
            else if (-ESTRPIPE == readRet)
            {
                aloge("fatal error! need recover:(%s)!", strerror(errno));
                snd_pcm_recover(pUacPcmHandle, readRet, 0);
            }
            else if (readRet < 0)
            {
                aloge("fatal error! other read error:(%s)", snd_strerror(readRet));
            }
            nReadBytes = 0;
        }

        stAudioFrame.mLen = nReadBytes;
        if (stAudioFrame.mLen > 0)
        {
            eRet = AW_MPI_AO_SendFrameSync(pContext->nAIODev, pContext->nAoChn, &stAudioFrame);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! aoChn[%d-%d] send audio frame fail:0x%x", pContext->nAIODev, pContext->nAoChn, eRet);
            }
        #ifdef DEBUG_SAVE_UAC_CAPTURE_PCM
            if (fp)
            {
                fwrite(capture_data, 1, nReadBytes, fp);
            }
        #endif
        }
    }
    AW_MPI_AO_SetStreamEof(pContext->nAIODev, pContext->nAoChn, TRUE, TRUE);
    cdx_sem_down(&pContext->stSemAoEof);

    eRet = AW_MPI_AO_StopChn(pContext->nAIODev, pContext->nAoChn);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! aoChn[%d-%d] stop fail:0x%x!", pContext->nAIODev, pContext->nAoChn, eRet);
    }
    eRet = AW_MPI_AO_DestroyChn(pContext->nAIODev, pContext->nAoChn);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! aoChn[%d-%d] destroy fail:0x%x!", pContext->nAIODev, pContext->nAoChn, eRet);
    }
#ifdef DEBUG_SAVE_UAC_CAPTURE_PCM
    if (fp)
    {
        fclose(fp);
        fp = NULL;
    }
#endif

    //close uac1_card
    if (capture_data)
    {
        free(capture_data);
        capture_data = NULL;
    }
    ret = snd_pcm_close(pUacPcmHandle);
    if (ret < 0)
    {
        aloge("fatal error! pcm close fail:%d!", ret);
    }
    pUacPcmHandle = NULL;
_exit:
    return (void *)result;
}

void config_AIO_ATTR_S_for_AI(AIO_ATTR_S *pAiAttr, SampleUvcVcodecVoUacConfig *pConfig)
{
    memset(pAiAttr, 0, sizeof(AIO_ATTR_S));
    pAiAttr->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(pConfig->nSampleRate);
    pAiAttr->enBitwidth = AUDIO_BIT_WIDTH_16;
    if(pConfig->bAiAec)
    {
        pAiAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
    }
    else
    {
        if(1 == pConfig->nMicNum)
        {
            pAiAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
        }
        else if(2 == pConfig->nMicNum)
        {
            pAiAttr->enSoundmode = AUDIO_SOUND_MODE_STEREO;
        }
        else
        {
            aloge("fatal error! mic num[%d] wrong", pConfig->nMicNum);
            pAiAttr->enSoundmode = AUDIO_SOUND_MODE_MONO;
        }
    }
    pAiAttr->mPtNumPerFrm = pConfig->nAiPeriodSize;
    pAiAttr->mChnCnt = pConfig->nMicNum;
    pAiAttr->mMicNum = pConfig->nMicNum;
    pAiAttr->ai_aec_en = pConfig->bAiAec;
    pAiAttr->aec_delay_ms = 0;
    pAiAttr->mAecNlpMode = 0;
    pAiAttr->ai_ans_en = pConfig->bAiAns;
    pAiAttr->ai_ans_mode = 0;
    pAiAttr->ai_agc_en = pConfig->bAiAgc;
    if (pAiAttr->ai_agc_en)
    {
        pAiAttr->ai_agc_float_cfg.fTargetDb = 0;
        pAiAttr->ai_agc_float_cfg.fMaxGainDb = 30;
    }
}

/**
  In host perspective, uac_out means capture pcm data locally, then send out to uac. So local aec is needed.
*/
static void *uac1OutThread(void *thread_data)
{
    int result = 0;
    int ret;
    ERRORTYPE eRet;
    SampleUvcVcodecVoUacContext *pContext = (SampleUvcVcodecVoUacContext *)thread_data;

    config_AIO_ATTR_S_for_AI(&pContext->stAiAttr, &pContext->stConfig);
    //1. prepare uac pcm handle to play
    snd_pcm_info_t *info;
    snd_pcm_t *pUacPcmHandle = NULL;
    int open_mode = 0;
    // open_mode |= SND_PCM_NO_AUTO_RESAMPLE;  // not to used the auto resample
    ret = snd_pcm_open(&pUacPcmHandle, pContext->stConfig.strUacDevName, SND_PCM_STREAM_PLAYBACK, open_mode);
    if (ret < 0)
    {
        aloge("fatal error! PCM_handle[%s] open for capture error: %s. Maybe uac not support play, exit now.",
            pContext->stConfig.strUacDevName, snd_strerror(ret));
        result = -1;
        goto _err0;
    }
    snd_pcm_info_alloca(&info);
    ret = snd_pcm_info(pUacPcmHandle, info);
    if (ret < 0)
    {
        aloge("fatal error! snd_pcm_info error: %s", snd_strerror(ret));
    }

    /* HW params */
    snd_pcm_hw_params_t *params;
    snd_pcm_format_t pcmFormat = SND_PCM_FORMAT_S16_LE;
    snd_pcm_hw_params_alloca(&params);
    ret = snd_pcm_hw_params_any(pUacPcmHandle, params);
    if (ret < 0)
    {
        aloge("fatal error! Broken configuration for this PCM: no configurations available");
    }
    ret = snd_pcm_hw_params_set_access(pUacPcmHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0)
    {
        aloge("fatal error! Access type not available");
    }
    ret = snd_pcm_hw_params_set_format(pUacPcmHandle, params, pcmFormat);
    if (ret < 0)
    {
        aloge("fatal error! Sample format not available");
    }
    int nOutChnCnt = (AUDIO_SOUND_MODE_MONO==pContext->stAiAttr.enSoundmode)?1:2;
    ret = snd_pcm_hw_params_set_channels(pUacPcmHandle, params, nOutChnCnt);
    if (ret < 0)
    {
        aloge("fatal error! Channels count[%d] not available", nOutChnCnt);
    }
    unsigned int rate = pContext->stConfig.nSampleRate;
    ret = snd_pcm_hw_params_set_rate_near(pUacPcmHandle, params, &rate, NULL);
    if (ret < 0)
    {
        aloge("fatal error! set_rate_near error!");
    }
    if (rate != pContext->stConfig.nSampleRate)
    {
        aloge("fatal error! required sample_rate %d is not supported, use %d instead", pContext->stConfig.nSampleRate, rate);
    }
    snd_pcm_uframes_t periodSize = pContext->stConfig.nUacOutPeriodSize;
    ret = snd_pcm_hw_params_set_period_size_near(pUacPcmHandle, params, &periodSize, NULL);
    if (ret < 0)
    {
        aloge("fatal error! set_period_size_near error!");
    }
    if(periodSize != pContext->stConfig.nUacOutPeriodSize)
    {
        aloge("fatal error! periodSize change:%d->%lu", pContext->stConfig.nUacOutPeriodSize, periodSize);
    }
    snd_pcm_uframes_t bufferSize = periodSize * PERIOD_COUNT;
    ret = snd_pcm_hw_params_set_buffer_size_near(pUacPcmHandle, params, &bufferSize);
    if (ret < 0)
    {
        aloge("fatal error! set_buffer_size_near error!");
    }
    if(bufferSize != periodSize * PERIOD_COUNT)
    {
        aloge("fatal error! bufferSize change:%lu->%lu", periodSize * PERIOD_COUNT, bufferSize);
    }
    ret = snd_pcm_hw_params(pUacPcmHandle, params);
    if (ret < 0)
    {
        aloge("fatal error! Unable to install hw params");
    }

    snd_pcm_uframes_t periodSizeFinal = 0;
    ret = snd_pcm_hw_params_get_period_size(params, &periodSizeFinal, NULL);
    if (0 == ret)
    {
        if (periodSizeFinal != periodSize)
        {
            aloge("fatal error! uac_capture periodSize [%d!=%d]", periodSizeFinal, periodSize);
        }
    }
    else
    {
        aloge("fatal error! get period size fail:%d", ret);
    }
    snd_pcm_uframes_t bufferSizeFinal = 0;
    ret = snd_pcm_hw_params_get_buffer_size(params, &bufferSizeFinal);
    if (0 == ret)
    {
        if (bufferSizeFinal != bufferSize)
        {
            aloge("fatal error! uac_capture bufferSize [%d!=%d]", bufferSizeFinal, bufferSize);
        }
    }
    else
    {
        aloge("fatal error! get buffer size fail:%d", ret);
    }
    int bitsPerSample = snd_pcm_format_physical_width(pcmFormat);
    int significantBitsPerSample = snd_pcm_format_width(pcmFormat);
    int nAlsaFrameBytes = bitsPerSample * nOutChnCnt / 8;
    int periodBytes = periodSize * nAlsaFrameBytes;
    alogd("----------------ALSA setting, pcm_name:%s, pcm_stream:%d----------------", snd_pcm_name(pUacPcmHandle),
        snd_pcm_stream(pUacPcmHandle));
    alogd(">>Channels:   %4d, BitWidth:  %4d,phsical_w:%4d, SampRate:   %4d", nOutChnCnt, significantBitsPerSample,
        bitsPerSample, rate);
    alogd(">>ChunkBytes: %4d, ChunkSize: %4d, BufferSize: %4d", periodBytes, periodSize, bufferSize);

    /* SW params */
    snd_pcm_sw_params_t *sw_params;
    snd_pcm_sw_params_alloca(&sw_params);
    ret = snd_pcm_sw_params_current(pUacPcmHandle, sw_params);
    if (ret != 0)
    {
        aloge("fatal error! sw params current fail");
    }
    snd_pcm_uframes_t boundary = 0;
    snd_pcm_sw_params_get_boundary(sw_params, &boundary);
    alogd("SW play params get: boundary:0x%lx", boundary);
//        snd_pcm_uframes_t silence_size = 0;
//        snd_pcm_sw_params_get_silence_size(sw_params, &silence_size);
//        snd_pcm_uframes_t silence_threshold = 0;
//        snd_pcm_sw_params_get_silence_threshold(sw_params, &silence_size);
    snd_pcm_uframes_t nStartThreshold = periodSize*pContext->stConfig.nUacOutStartThresholdMultiple;
    snd_pcm_sw_params_set_start_threshold(pUacPcmHandle, sw_params, nStartThreshold);
    /* set silence size, in order to fill silence data into ringbuffer */
    snd_pcm_sw_params_set_silence_size(pUacPcmHandle, sw_params, boundary);
    alogd("SW play params set: start_threshold:%ld, silence_size:0x%lx", nStartThreshold, boundary);
    snd_pcm_sw_params_set_stop_threshold(pUacPcmHandle, sw_params, bufferSize);
    snd_pcm_sw_params_set_avail_min(pUacPcmHandle, sw_params, periodSize);
    ret = snd_pcm_sw_params(pUacPcmHandle, sw_params);
    if (ret < 0)
    {
        aloge("fatal error! Unable to install sw prams!");
    }

    ret = snd_pcm_prepare(pUacPcmHandle);
    if (ret < 0)
    {
        aloge("fatal error! pcm prepare fail:%d!", ret);
    }

    //2. prepare local mpi_ai
    AW_MPI_AI_SetPubAttr(pContext->nAIODev, &pContext->stAiAttr);
    AW_MPI_AI_Enable(pContext->nAIODev);
    AW_MPI_AI_SetDevVolume(pContext->nAIODev, pContext->stConfig.nAiVolume);
    memset(&pContext->stAiChnAttr, 0, sizeof(pContext->stAiChnAttr));
    //we need adapt to uac out period size to avoid uac sound card underrun.
    pContext->stAiChnAttr.nFrameSize = pContext->stConfig.nUacOutPeriodSize;
    eRet = AW_MPI_AI_CreateChn(pContext->nAIODev, pContext->nAiChn, &pContext->stAiChnAttr);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! create ai channel[%d-%d] fail:0x%x!", pContext->nAIODev, pContext->nAiChn, eRet);
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void *)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleUvcVcodecVoUac_MPPCallbackWrapper;
    AW_MPI_AI_RegisterCallback(pContext->nAIODev, pContext->nAiChn, &cbInfo);
    eRet = AW_MPI_AI_EnableChn(pContext->nAIODev, pContext->nAiChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! aiChn[%d-%d] enable fail:0x%x!", pContext->nAIODev, pContext->nAiChn, eRet);
    }

    //3. start to read pcm from mpi_ai, send pcm to uac to play.
    message_t stMsg;
    snd_pcm_sframes_t wtRet;
    AUDIO_FRAME_S stAudioFrame;
    memset(&stAudioFrame, 0, sizeof(stAudioFrame));
    while (1)
    {
        if (get_message(&pContext->stUacOutMessageQueue, &stMsg) == 0)
        {
            if (Stop == stMsg.command)
            {
                alogd("uacOut thread receive stop command");
                break;
            }
            else
            {
                aloge("fatal error! uacOut thread receive unknown cmd:0x%x", stMsg.command);
            }
        }

        eRet = AW_MPI_AI_GetFrame(pContext->nAIODev, pContext->nAiChn, &stAudioFrame, NULL, 200);
        if(SUCCESS == eRet)
        {
            if (map_AUDIO_BIT_WIDTH_E_to_BitWidth(stAudioFrame.mBitwidth)*judgeAudioChnNumBySoundMode(stAudioFrame.mSoundmode,
                NULL, NULL) != nAlsaFrameBytes*8)
            {
                aloge("fatal error! alsa frame bytes wrong! [%d-%d] != %d", stAudioFrame.mBitwidth, stAudioFrame.mSoundmode,
                    nAlsaFrameBytes);
            }
            if (stAudioFrame.mLen%nAlsaFrameBytes != 0)
            {
                aloge("fatal error! audio frame len wrong! [%d-%d]", stAudioFrame.mLen, nAlsaFrameBytes);
            }
            int nAlsaFrameNum = stAudioFrame.mLen/nAlsaFrameBytes;
            wtRet = snd_pcm_writei(pUacPcmHandle, stAudioFrame.mpAddr, nAlsaFrameNum);
            if (wtRet >= 0)
            {
                if (wtRet < nAlsaFrameNum)
                {
                    aloge("fatal error! uacOut write less alsaFrames:%d<%d", wtRet, nAlsaFrameNum);
                }
            }
            else
            {
                if (-EAGAIN == wtRet)
                {
                    aloge("fatal error! alsa play eagain:(%s)! pcm_state:%d", strerror(errno), snd_pcm_state(pUacPcmHandle));
                    snd_pcm_wait(pUacPcmHandle, 100);
                }
                else if (-EPIPE == wtRet)
                {
                    aloge("fatal error! alsa play underflow xrun:(%s)! pcm_state:%d", strerror(errno), snd_pcm_state(pUacPcmHandle));
                    snd_pcm_prepare(pUacPcmHandle);
                }
                else if (-EBADFD == wtRet)
                {
                    aloge("fatal error! alsa play badfd:(%s)! pcm_state:%d", strerror(errno), snd_pcm_state(pUacPcmHandle));
                    snd_pcm_prepare(pUacPcmHandle);
                }
                else if (-ESTRPIPE == wtRet)
                {
                    aloge("fatal error! alsa play need recover:(%s)! pcm_state:%d", strerror(errno), snd_pcm_state(pUacPcmHandle));
                    snd_pcm_recover(pUacPcmHandle, wtRet, 0);
                }
                else if (wtRet < 0)
                {
                    aloge("fatal error! alsa play write error! ret:(%d-%s)", wtRet, snd_strerror(wtRet));
                }
            }

            eRet = AW_MPI_AI_ReleaseFrame(pContext->nAIODev, pContext->nAiChn, &stAudioFrame, NULL);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! aiChn[%d-%d] release frame fail:0x%x", pContext->nAIODev, pContext->nAiChn, eRet);
            }
        }
        else if(ERR_AI_BUF_EMPTY == eRet)
        {
            alogw("Be careful! aiChn[%d-%d] getFrame timeout?", pContext->nAIODev, pContext->nAiChn);
        }
        else
        {
            aloge("fatal error! aiChn[%d-%d] getFrame fail[0x%x]", pContext->nAIODev, pContext->nAiChn, eRet);
        }
    }

    //4. close mpi_ai and uac pcm handle.
    eRet = AW_MPI_AI_DisableChn(pContext->nAIODev, pContext->nAiChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! aiChn[%d-%d] disable fail:0x%x!", pContext->nAIODev, pContext->nAiChn, eRet);
    }
    eRet = AW_MPI_AI_DestroyChn(pContext->nAIODev, pContext->nAiChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! aiChn[%d-%d] destroy fail:0x%x!", pContext->nAIODev, pContext->nAiChn);
    }
    eRet = AW_MPI_AI_Disable(pContext->nAIODev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! aiDev[%d] disable fail:0x%x!", pContext->nAIODev, eRet);
    }
    eRet = AW_MPI_AI_ClrPubAttr(pContext->nAIODev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! aiDev[%d] clear pub attr fail:0x%x!", pContext->nAIODev, eRet);
    }
    ret = snd_pcm_drain(pUacPcmHandle);
    if (ret != 0)
    {
        aloge("fatal error! uac pcm drain error:(%d-%s)", ret, snd_strerror(ret));
    }
    ret = snd_pcm_close(pUacPcmHandle);
    if (ret != 0)
    {
        aloge("fatal error! uac pcm close error:(%d-%s)", ret, snd_strerror(ret));
    }
    pUacPcmHandle = NULL;

    return (void *)result;

_err0:
    return (void *)result;
}

int main(int argc, char **argv)
{
    kernel_fwrite("dmesg: app[%s] begin\n", argv[0]);
    int result = 0;
    int ret;
    GLogConfig stGLogConfig =
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_WARN,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);
    aloge("log_init done");

    SampleUvcVcodecVoUacContext *pContext = (SampleUvcVcodecVoUacContext*)malloc(sizeof(SampleUvcVcodecVoUacContext));
    initSampleUvcVcodecVoUacContext(pContext);
    gpSampleUvcVcodecVoUacContext = pContext;

    if(ParseCmdLine(argc, argv, &pContext->stCmdLineParam) != 0)
    {
        result = -1;
        goto _exit;
    }

    char *pConfigFilePath = NULL;
    if(strlen(pContext->stCmdLineParam.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->stCmdLineParam.mConfigFilePath;
    }
    if(LoadSampleUvcVcodecVoUacConfig(&pContext->stConfig, pConfigFilePath) != 0)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    if ((pContext->stConfig.nDisplayMainWidth > 0) && (pContext->stConfig.nDisplayMainHeight > 0))
    {
        pContext->bEnableDisplay = true;
    }
    else
    {
        pContext->bEnableDisplay = false;
    }
    if (pContext->stConfig.eVencType != PT_MAX)
    {
        pContext->bEnableEncode = true;
    }
    else
    {
        pContext->bEnableEncode = false;
    }

    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }

    memset(&pContext->stSysconf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->stSysconf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->stSysconf);
    AW_MPI_SYS_Init();
    //AW_MPI_VDEC_SetVEFreq(MM_INVALID_CHN, 648);

    ERRORTYPE eRet = AW_MPI_UVC_CreateDevice(pContext->stConfig.strDevName);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! uvcDev[%s] can not create, ret:0x%x!", pContext->stConfig.strDevName, eRet);
        result = -1;
        goto _exit1;
    }
    UVC_ATTR_S attr;
    memset(&attr, 0, sizeof attr);
    attr.mPixelformat = pContext->stConfig.ePicFormat;
    attr.mUvcVideo_BufCnt = pContext->stConfig.nCaptureVideoBufCnt;
    attr.mUvcVideo_Width = pContext->stConfig.nCaptureWidth;
    attr.mUvcVideo_Height = pContext->stConfig.nCaptureHeight;
    attr.mUvcVideo_Fps = pContext->stConfig.nCaptureFrameRate;
    attr.nMaxVideoFrameSize = AWALIGN((unsigned int)(pContext->stConfig.nCaptureWidth * pContext->stConfig.nCaptureHeight
        * 3 / 2 * pContext->stConfig.fCaptureMaxFramesizeRatio), 1024);
    eRet = AW_MPI_UVC_SetDeviceAttr(pContext->stConfig.strDevName, &attr);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %s UVC can not set device attr, ret:0x%x", pContext->stConfig.strDevName, eRet);
        result = -1;
        goto _exit2;
    }
    eRet = AW_MPI_UVC_GetDeviceAttr(pContext->stConfig.strDevName, &attr);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC can not get device attr", pContext->stConfig.strDevName);
    }

    eRet = AW_MPI_UVC_CreateVirChn(pContext->stConfig.strDevName, pContext->nUvcChn);
    if(eRet != SUCCESS)
    {
        aloge("error: the %s UVC can not create virchannel[%d]", pContext->stConfig.strDevName, pContext->nUvcChn);
    }

    configVdecChnAttr(pContext);
    eRet = AW_MPI_VDEC_CreateChn(pContext->nVdecChn, &pContext->stVdecChnAttr);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %s uvc can not create vdec chn[%s]", pContext->stConfig.strDevName, pContext->nVdecChn);
    }

    if(pContext->bEnableDisplay)
    {
        AW_MPI_VO_Enable(pContext->nVoDev);
        AW_MPI_VO_AddOutsideVideoLayer(pContext->nUILayer);
        AW_MPI_VO_CloseVideoLayer(pContext->nUILayer);
        VO_PUB_ATTR_S stPubAttr;
        memset(&stPubAttr, 0, sizeof(VO_PUB_ATTR_S));
        AW_MPI_VO_GetPubAttr(pContext->nVoDev, &stPubAttr);
        stPubAttr.enIntfType = VO_INTF_LCD;
        stPubAttr.enIntfSync = VO_OUTPUT_NTSC;
        AW_MPI_VO_SetPubAttr(pContext->nVoDev, &stPubAttr);

        int hlay0 = 0;
        while(hlay0 < VO_MAX_LAYER_NUM)
        {
            if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
            {
                break;
            }
            hlay0+=4;
        }
        if(hlay0 >= VO_MAX_LAYER_NUM)
        {
            aloge("error: enable video layer failed");
        }

        pContext->nVoLayer = hlay0;
        AW_MPI_VO_GetVideoLayerAttr(pContext->nVoLayer, &pContext->stLayerAttr);
        pContext->stLayerAttr.stDispRect.X = pContext->stConfig.nDisplayMainX;
        pContext->stLayerAttr.stDispRect.Y = pContext->stConfig.nDisplayMainY;
        pContext->stLayerAttr.stDispRect.Width = pContext->stConfig.nDisplayMainWidth;
        pContext->stLayerAttr.stDispRect.Height = pContext->stConfig.nDisplayMainHeight;
        AW_MPI_VO_SetVideoLayerAttr(pContext->nVoLayer, &pContext->stLayerAttr);

        eRet = AW_MPI_VO_CreateChn(pContext->nVoLayer, pContext->nVOChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error: create vo channel[%d-%d] failed", pContext->nVoLayer, pContext->nVOChn);
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void *)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleUvcVcodecVoUac_MPPCallbackWrapper;
        AW_MPI_VO_RegisterCallback(pContext->nVoLayer, pContext->nVOChn, &cbInfo);
        AW_MPI_VO_SetChnDispBufNum(pContext->nVoLayer, pContext->nVOChn, 2);
    }
    if (pContext->bEnableEncode)
    {
        configVencChnAttr(pContext);
        eRet = AW_MPI_VENC_CreateChn(pContext->nVencChn, &pContext->stVencChnAttr);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] create fail:0x%x", pContext->nVencChn, eRet);
        }
        eRet = AW_MPI_VENC_SetRcParam(pContext->nVencChn, &pContext->stVencRcParam);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] set rc param fail:0x%x", pContext->nVencChn, eRet);
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void *)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&SampleUvcVcodecVoUac_MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->nVencChn, &cbInfo);
    }

    // note: UvcChn->mDevId is the pointer of char *;
    /*MPP_CHN_S UvcChn = {MOD_ID_UVC, (int)pContext->stConfig.strDevName, pContext->nUvcChn};
    MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, pContext->nVdecChn};
    eRet = AW_MPI_SYS_Bind(&UvcChn, &VdecChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! bind uvc2vdec fail[0x%x]", eRet);
    }*/

    eRet = AW_MPI_UVC_EnableDevice(pContext->stConfig.strDevName);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %s UVC device can not start", pContext->stConfig.strDevName);
    }
    eRet = AW_MPI_UVC_StartRecvPic(pContext->stConfig.strDevName, pContext->nUvcChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error: the %s UVC can not start virchannel[%d]", pContext->stConfig.strDevName, pContext->nUvcChn);
    }
    AW_MPI_VDEC_StartRecvStream(pContext->nVdecChn);

    if (pContext->bEnableDisplay)
    {
        eRet = AW_MPI_VO_StartChn(pContext->nVoLayer, pContext->nVOChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! voChn[%d-%d] start fail", pContext->nVoLayer, pContext->nVOChn);
        }
    }
    if (pContext->bEnableEncode)
    {
        eRet = AW_MPI_VENC_StartRecvPic(pContext->nVencChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] start fail", pContext->nVencChn);
        }
        ret = pthread_create(&pContext->GetStreamThreadId, NULL, GetStreamThread, pContext);
        if(ret != 0)
        {
            aloge("fatal error! create GetStreamThread fail:%d!", ret);
        }
    }

    if (pContext->stConfig.bUacIn)
    {
        ret = pthread_create(&pContext->uacInThreadId, NULL, uac1InThread, (void *)pContext);
        if(ret != 0)
        {
            aloge("fatal error! create uac1InThread fail:%d!", ret);
        }
    }
    if (pContext->stConfig.bUacOut)
    {
        ret = pthread_create(&pContext->uacOutThreadId, NULL, uac1OutThread, (void *)pContext);
        if(ret != 0)
        {
            aloge("fatal error! create uac1OutThread fail:%d!", ret);
        }
    }

    //get frame from vdec, send frame to venc and vo.
    VIDEO_FRAME_INFO_S mainFrameInfo;
    VIDEO_FRAME_INFO_S subFrameInfo;
    VIDEO_FRAME_INFO_S uvcFrameInfo;
    VDEC_STREAM_S stVdecStream;
    memset(&stVdecStream, 0, sizeof(stVdecStream));
    while(1)
    {
        VdecDoubleFrameInfo *pDstDbFrame = NULL;
        if(pContext->stConfig.nTestFrameCount > 0)
        {
            if(pContext->nFrameCounter>=pContext->stConfig.nTestFrameCount)
            {
                alogd("get [%d] frames from vdec, prepare to exit!", pContext->nFrameCounter);
                break;
            }
        }
        if(cdx_sem_get_val(&pContext->stSemExit) > 0)
        {
            alogd("detect user exit signal! prepare to exit!");
            break;
        }

        eRet = AW_MPI_UVC_GetFrame(pContext->stConfig.strDevName, pContext->nUvcChn, &uvcFrameInfo, 200);
        if (SUCCESS == eRet)
        {
            stVdecStream.pAddr = uvcFrameInfo.VFrame.mpVirAddr[0];
            stVdecStream.mLen = uvcFrameInfo.VFrame.mStride[0];
            stVdecStream.mPTS = uvcFrameInfo.VFrame.mpts;
            stVdecStream.mbEndOfFrame = TRUE;
            stVdecStream.mbEndOfStream = FALSE;
            eRet = AW_MPI_VDEC_SendStream(pContext->nVdecChn, &stVdecStream, 200);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! vdecChn[%d] send stream fail:0x%x?", pContext->nVdecChn, eRet);
            }
            eRet = AW_MPI_UVC_ReleaseFrame(pContext->stConfig.strDevName, pContext->nUvcChn, &uvcFrameInfo);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! uvcChn[%s-%d] release frame fail:0x%x?", pContext->stConfig.strDevName, pContext->nUvcChn, eRet);
            }
        }
        else
        {
            aloge("fatal error! uvcChn[%s-%d] get frame fail:0x%x", pContext->stConfig.strDevName, pContext->nUvcChn, eRet);
            continue;
        }
        eRet = AW_MPI_VDEC_GetImage(pContext->nVdecChn, &mainFrameInfo, 500);
        if(SUCCESS == eRet)
        {
            pthread_mutex_lock(&pContext->stFrameLock);
            int suffix = FindFrameIdInArray(pContext, mainFrameInfo.mId);
            if(-1 == suffix)
            {
                if(pContext->nValidDoubleFrameBufferNum >= MAX_FRAMEPAIR_ARRAY_SIZE)
                {
                    aloge("fatal error! frame number [%d] too much!", pContext->nValidDoubleFrameBufferNum);
                }
                pDstDbFrame = &pContext->DoubleFrameArray[pContext->nValidDoubleFrameBufferNum];
                pContext->nValidDoubleFrameBufferNum++;
            }
            else
            {
                pDstDbFrame = &pContext->DoubleFrameArray[suffix];
            }
            if(pDstDbFrame->mMainRefCnt!=0 || pDstDbFrame->mSubRefCnt!=0)
            {
                aloge("fatal error! refCnt[%d-%d] != 0", pDstDbFrame->mMainRefCnt, pDstDbFrame->mSubRefCnt);
            }
            pDstDbFrame->mMainFrame = mainFrameInfo;
            pDstDbFrame->mMainRefCnt = 1;

            pContext->nFrameCounter++;
            pContext->nHoldFrameNum++;
            pthread_mutex_unlock(&pContext->stFrameLock);
        }
        else
        {
            aloge("fatal error! why not get frame from vdec for too long? ret=0x%x", eRet);
            continue;
        }

        if (pContext->bEnableDisplay)
        {
            pthread_mutex_lock(&pContext->stFrameLock);
            pDstDbFrame->mMainRefCnt++;
            pthread_mutex_unlock(&pContext->stFrameLock);
            eRet = AW_MPI_VO_SendFrame(pContext->nVoLayer, pContext->nVOChn, &pDstDbFrame->mMainFrame, 0);
            if(eRet != SUCCESS)
            {
                aloge("fatal error! why send frame to vo fail? voChn[%d-%d], ret[0x%x]", pContext->nVoLayer, pContext->nVOChn, eRet);
                pthread_mutex_lock(&pContext->stFrameLock);
                pDstDbFrame->mMainRefCnt--;
                pthread_mutex_unlock(&pContext->stFrameLock);
            }
        }
        if (pContext->bEnableEncode)
        {
            pthread_mutex_lock(&pContext->stFrameLock);
            pDstDbFrame->mMainRefCnt++;
            pthread_mutex_unlock(&pContext->stFrameLock);
            eRet = AW_MPI_VENC_SendFrame(pContext->nVencChn, &pDstDbFrame->mMainFrame, 0);
            if (eRet != SUCCESS)
            {
                aloge("fatal error! vencChn[%d] send frame fail:0x%x", pContext->nVencChn, eRet);
                pthread_mutex_lock(&pContext->stFrameLock);
                pDstDbFrame->mMainRefCnt--;
                pthread_mutex_unlock(&pContext->stFrameLock);
            }
        }
        ret = releaseVideoFrameToVdecChn(pContext, pDstDbFrame->mMainFrame.mId);
        if (ret != 0)
        {
            aloge("fatal error! release frameId[%d] fail:%d", pDstDbFrame->mMainFrame.mId, ret);
        }
    }

    if (pContext->stConfig.bUacIn)
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = Stop;
        ret = put_message(&pContext->stUacInMessageQueue, &stMsg);
        if (ret != 0)
        {
            aloge("fatal error! put message fail:%d", ret);
        }
        int nUacInThdRet = 0;
        ret = pthread_join(pContext->uacInThreadId, (void**)&nUacInThdRet);
        if (0 == ret)
        {
            alogd("uacIn thread join success. thread ret:%d", nUacInThdRet);
        }
        else
        {
            aloge("fatal error! uacIn thread join fail:%d", ret);
        }
        pContext->uacInThreadId = NULL;
    }
    if (pContext->stConfig.bUacOut)
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = Stop;
        ret = put_message(&pContext->stUacOutMessageQueue, &stMsg);
        if (ret != 0)
        {
            aloge("fatal error! put message fail:%d", ret);
        }
        int nUacOutThdRet = 0;
        ret = pthread_join(pContext->uacOutThreadId, (void**)&nUacOutThdRet);
        if (0 == ret)
        {
            alogd("uacOut thread join success. thread ret:%d", nUacOutThdRet);
        }
        else
        {
            aloge("fatal error! uacOut thread join fail:%d", ret);
        }
        pContext->uacOutThreadId = NULL;
    }

    if (pContext->bEnableDisplay)
    {
        eRet = AW_MPI_VO_StopChn(pContext->nVoLayer, pContext->nVOChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! voChn[%d-%d] stop fail:0x%x", pContext->nVoLayer, pContext->nVOChn, eRet);
        }
    }
    if (pContext->bEnableEncode)
    {
        message_t stMsg;
        InitMessage(&stMsg);
        stMsg.command = Stop;
        ret = put_message(&pContext->stGetStreamMessageQueue, &stMsg);
        if (ret != 0)
        {
            aloge("fatal error! put message fail:%d", ret);
        }
        int nGetStreamThdRet = 0;
        ret = pthread_join(pContext->GetStreamThreadId, (void**)&nGetStreamThdRet);
        if (0 == ret)
        {
            alogd("vencChn[%d] GetStream thread join success. thread ret:%d", pContext->nVencChn, nGetStreamThdRet);
        }
        else
        {
            aloge("fatal error! pthread join fail:%d", ret);
        }
        pContext->GetStreamThreadId = NULL;

        eRet = AW_MPI_VENC_StopRecvPic(pContext->nVencChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] stop fail:0x%x", pContext->nVencChn, eRet);
        }
    }

    eRet = AW_MPI_VDEC_StopRecvStream(pContext->nVdecChn);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! vdecChn[%d] stop fail:0x%x", pContext->nVdecChn, eRet);
    }
    eRet = AW_MPI_UVC_StopRecvPic(pContext->stConfig.strDevName, pContext->nUvcChn);
    if (eRet != SUCCESS)
    {
        aloge("fatal error!uvcChn[%s-%d] stop fail:0x%x", pContext->stConfig.strDevName, pContext->nUvcChn, eRet);
    }

    if (pContext->bEnableDisplay)
    {
        eRet = AW_MPI_VO_DestroyChn(pContext->nVoLayer, pContext->nVOChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! voChn[%d-%d] destroy fail:0x%x", pContext->nVoLayer, pContext->nVOChn, eRet);
        }
        pContext->nVOChn = MM_INVALID_CHN;
    }
    if (pContext->bEnableEncode)
    {
        eRet = AW_MPI_VENC_DestroyChn(pContext->nVencChn);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! vencChn[%d] destroy fail:0x%x", pContext->nVencChn, eRet);
        }
    }
    eRet = AW_MPI_VDEC_DestroyChn(pContext->nVdecChn);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! vdecChn[%d] destroy fail:0x%x", pContext->nVdecChn, eRet);
    }
    pContext->nVdecChn = MM_INVALID_CHN;
    eRet = AW_MPI_UVC_DestroyVirChn(pContext->stConfig.strDevName, pContext->nUvcChn);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! uvcChn[%s-%d] destroy fail:0x%x", pContext->stConfig.strDevName, pContext->nUvcChn, eRet);
    }
    pContext->nUvcChn = MM_INVALID_CHN;

    eRet = AW_MPI_UVC_DisableDevice(pContext->stConfig.strDevName);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! uvcDevice[%s] disable fail:0x%x", pContext->stConfig.strDevName, eRet);
    }
    eRet = AW_MPI_UVC_DestroyDevice(pContext->stConfig.strDevName);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! uvcDevice[%s] destroy fail:0x%x", pContext->stConfig.strDevName, eRet);
    }

    if (pContext->bEnableDisplay)
    {
        eRet = AW_MPI_VO_DisableVideoLayer(pContext->nVoLayer);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! voLayer[%d] disable fail:0x%x", pContext->nVoLayer, eRet);
        }
        pContext->nVoLayer = MM_INVALID_LAYER;
        eRet = AW_MPI_VO_RemoveOutsideVideoLayer(pContext->nUILayer);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! uiLayer[%d] remove fail:0x%x", pContext->nUILayer, eRet);
        }
        /* disable vo dev */
        eRet = AW_MPI_VO_Disable(pContext->nVoDev);
        if (eRet != SUCCESS)
        {
            aloge("fatal error! voDev[%d] disable fail:0x%x", pContext->nVoDev, eRet);
        }
        pContext->nVoDev = MM_INVALID_DEV;
    }

    AW_MPI_SYS_Exit();

    if(pContext->nHoldFrameNum != 0)
    {
        aloge("fatal error! why holdframe num[%d] != 0?", pContext->nHoldFrameNum);
    }
    alogd("vdec buffer number is [%d]", pContext->nValidDoubleFrameBufferNum);
    int i;
    for(i=0;i<pContext->nValidDoubleFrameBufferNum;i++)
    {
        alogd("vdec buffer[%d] id =[%d]", i, pContext->DoubleFrameArray[i].mMainFrame.mId);
    }

    destroySampleUvcVcodecVoUacContext(pContext);
    free(pContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;

_exit2:
    eRet = AW_MPI_UVC_DestroyDevice(pContext->stConfig.strDevName);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! %s UVC Device destroy fail:0x%x", pContext->stConfig.strDevName, eRet);
    }
_exit1:
    AW_MPI_SYS_Exit();
_exit:
    destroySampleUvcVcodecVoUacContext(pContext);
    free(pContext);
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}

