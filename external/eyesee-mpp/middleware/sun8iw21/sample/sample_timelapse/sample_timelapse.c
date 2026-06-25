/******************************************************************************
  Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner BU3-PD2 Team
  Created       : 2016/11/4
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "SampleTimeLapse"

#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "plat_log.h"
#include <mm_common.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_isp.h>

#include "sample_timelapse.h"
#include "sample_timelapse_conf.h"


#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (4*1024)


static SampleTimelapseContext *gpSampleTimelapseContext = NULL;


static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleTimelapseContext!=NULL)
    {
        cdx_sem_up(&gpSampleTimelapseContext->mSemExit);
    }
}

ERRORTYPE initSampleTimelapseConfig(SampleTimelapseConfig *pConfig)
{
    pConfig->mVippIndex = 0;
    pConfig->mVippWidth = 1920;
    pConfig->mVippHeight = 1080;
    pConfig->mVippFrameRate = 30;
    pConfig->mTimelapse = 1000000;
    pConfig->mVideoFrameRate = 30;
    pConfig->mVideoDuration = 4;
    pConfig->mVideoBitrate = 12;
    strcpy(pConfig->mVideoFilePath, "/mnt/extsd/timelapse.mp4");
    return SUCCESS;
}
SampleTimelapseContext* constructSampleTimelapseContext()
{
    SampleTimelapseContext *pContext = malloc(sizeof(SampleTimelapseContext));
    memset(pContext, 0, sizeof(SampleTimelapseContext));
    cdx_sem_init(&pContext->mSemExit, 0);
    return pContext;
}

void destructSampleTimelapseContext(SampleTimelapseContext *pContext)
{
    if(pContext)
    {
        cdx_sem_deinit(&pContext->mSemExit);
        free(pContext);
    }
}

static ERRORTYPE parseCmdLine(SampleTimelapseContext *pContext, int argc, char** argv)
{
    ERRORTYPE ret = FAILURE;

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = SUCCESS;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long:!", *argv);
              }

              strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path /home/sample_timelapse.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SampleTimelapseContext *pContext, const char *conf_path)
{
    int ret;
    char *ptr = NULL;
    CONFPARSER_S stConf;
    if(NULL==conf_path || 0==strlen(conf_path))
    {
        alogd("user do not set config file. use default test parameter!");
        initSampleTimelapseConfig(&pContext->mConfigPara);
        return SUCCESS;
    }
    
    ret = createConfParser(conf_path, &stConf);
    if (ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

    pContext->mConfigPara.mVippIndex = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_VIPP_INDEX, 0);
    pContext->mConfigPara.mVippWidth = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_VIPP_WIDTH, 0);
    pContext->mConfigPara.mVippHeight = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_VIPP_HEIGHT, 0);
    pContext->mConfigPara.mVippFrameRate = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_VIPP_FRAME_RATE, 0);
    pContext->mConfigPara.mTimelapse = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_TIMELAPSE, 0);
    pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_VIDEO_FRAME_RATE, 0);
    pContext->mConfigPara.mVideoDuration = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_VIDEO_DURATION, 0);
    pContext->mConfigPara.mVideoBitrate = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_VIDEO_BITRATE, 0);
    char *pVideoFilePath = (char*)GetConfParaString(&stConf, SAMPLE_TIMELAPSE_KEY_VIDEO_FILE_PATH, NULL); 
    strcpy(pContext->mConfigPara.mVideoFilePath, pVideoFilePath);
    pContext->mConfigPara.mOnlineEnable = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_ONLINE_EN, 0);
    pContext->mConfigPara.mOnlineShareBufNum = GetConfParaInt(&stConf, SAMPLE_TIMELAPSE_KEY_ONLINE_SHARE_BUF_NUM, 0);
    if (pContext->mConfigPara.mOnlineEnable && pContext->mConfigPara.mTimelapse)
    {
        int new_fps = 1000000 / pContext->mConfigPara.mTimelapse;
        alogd("update vipp fps %d -> %d", pContext->mConfigPara.mVippFrameRate, new_fps);
        pContext->mConfigPara.mVippFrameRate = new_fps;
    }

    ptr = (char *)GetConfParaString(&stConf, SAMPLE_TIMELAPSE_KEY_PIXFMT, NULL);
    if (ptr != NULL)
    {
        if (!strcmp(ptr, "nv21"))
        {
            pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if (!strcmp(ptr, "yv12"))
        {
            pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YVU_PLANAR_420;
        }
        else if (!strcmp(ptr, "nv12"))
        {
            pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if (!strcmp(ptr, "yu12"))
        {
            pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_PLANAR_420;
        }
        else if (!strcmp(ptr, "aw_lbc_2_5x"))
        {
            pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        }
        else if (!strcmp(ptr, "aw_lbc_2_0x"))
        {
            pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
        }
        else if (!strcmp(ptr, "aw_lbc_1_5x"))
        {
            pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
        }
        else if (!strcmp(ptr, "aw_lbc_1_0x"))
        {
            pContext->mConfigPara.srcPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
        }
        else
        {
            aloge("fatal error! wrong src pixfmt:%s", ptr);
            alogw("use the default pixfmt %d", pContext->mConfigPara.srcPixFmt);
        }
    }

    ptr = (char *)GetConfParaString(&stConf, SAMPLE_TIMELAPSE_KEY_VIDEO_ENCODER, NULL);
    if (ptr != NULL)
    {
        if (!strcmp(ptr, "H.264"))
        {
            pContext->mConfigPara.mVideoEncoderFmt = PT_H264;
            alogd("H.264");
        }
        else if (!strcmp(ptr, "H.265"))
        {
            pContext->mConfigPara.mVideoEncoderFmt = PT_H265;
            alogd("H.265");
        }
        else if (!strcmp(ptr, "MJPEG"))
        {
            pContext->mConfigPara.mVideoEncoderFmt = PT_MJPEG;
            alogd("MJPEG");
        }
        else
        {
            aloge("error conf encoder type");
        }
    }

    destroyConfParser(&stConf);
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleTimelapseContext *pContext = (SampleTimelapseContext *)cookie;

    if(MOD_ID_VENC == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if(MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                int muxChn = *(int*)pEventData;
                alogd("file done, muxChn=%d", muxChn);
                cdx_sem_up(&pContext->mSemExit);
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                alogd("don't set next fd, we only want one file.");
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return SUCCESS;
}

int main(int argc, char** argv)
{
    int result = -1;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 25,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "SDV-");
    log_init(argv[0], &stGLogConfig);

    SampleTimelapseContext *pContext = constructSampleTimelapseContext();
    gpSampleTimelapseContext = pContext;
    if (parseCmdLine(pContext, argc, argv) != SUCCESS)
    {
        aloge("parse cmdline fail");
        return -1;
    }

    if (loadConfigPara(pContext, pContext->mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("load config file fail");
        return -1;
    }

    //register process function for SIGINT, to exit program.
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    ERRORTYPE ret;

    //create vi channel
    pContext->mIspDev = 0;
    pContext->mViDev = pContext->mConfigPara.mVippIndex;
    pContext->mViChn = 0;
    ret = AW_MPI_VI_CreateVipp(pContext->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp failed");
    }

    memset(&pContext->mViAttr, 0, sizeof(VI_ATTR_S));
    pContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.srcPixFmt);
    pContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709;
    pContext->mViAttr.format.width = pContext->mConfigPara.mVippWidth;
    pContext->mViAttr.format.height = pContext->mConfigPara.mVippHeight;
    pContext->mViAttr.nbufs = 3;//5;
    alogd("use %d v4l2 buffers!!!", pContext->mViAttr.nbufs);
    pContext->mViAttr.nplanes = 2;
    pContext->mViAttr.fps = pContext->mConfigPara.mVippFrameRate;
    pContext->mViAttr.mbEncppEnable = TRUE;
    pContext->mViAttr.mOnlineEnable = pContext->mConfigPara.mOnlineEnable;
    pContext->mViAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;
    ret = AW_MPI_VI_SetVippAttr(pContext->mViDev, &pContext->mViAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr failed");
    }
    AW_MPI_ISP_Run(pContext->mIspDev);
    ret = AW_MPI_VI_EnableVipp(pContext->mViDev);
    if (ret != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
    }
    ret = AW_MPI_VI_CreateVirChn(pContext->mViDev, pContext->mViChn, NULL);
    if (ret != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pContext->mViChn);
    }

    //create venc channel
    memset(&pContext->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    pContext->mVencChnAttr.VeAttr.Type = pContext->mConfigPara.mVideoEncoderFmt;
    pContext->mVencChnAttr.VeAttr.MaxKeyInterval = pContext->mConfigPara.mVideoFrameRate;
    pContext->mVencChnAttr.VeAttr.SrcPicWidth = pContext->mConfigPara.mVippWidth;
    pContext->mVencChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.mVippHeight;
    pContext->mVencChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.srcPixFmt;
    pContext->mVencChnAttr.VeAttr.mColorSpace = pContext->mViAttr.format.colorspace;
    pContext->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mVencChnAttr.EncppAttr.eEncppSharpSetting = VencEncppSharp_FollowISPConfig;
    pContext->mVencChnAttr.VeAttr.mVeRefFrameLbcMode =VENC_REF_FRAME_LBC_MODE_NO_LOSSY;

    pContext->mVencChnAttr.RcAttr.mProductMode = PRODUCT_CDR;
    //pContext->mVencRcParam.sensor_type = VENC_ST_EN_WDR;

    pContext->mVencChnAttr.VeAttr.mOnlineEnable = pContext->mConfigPara.mOnlineEnable;
    pContext->mVencChnAttr.VeAttr.mOnlineShareBufNum = pContext->mConfigPara.mOnlineShareBufNum;

    if(PT_H264==pContext->mConfigPara.mVideoEncoderFmt)
    {
        pContext->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH264e.Profile = 2;//0:base 1:main 2:high
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->mConfigPara.mVippWidth;
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.mVippHeight;

        pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
        pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mBitRate = pContext->mConfigPara.mVideoBitrate*1024*1024;
        pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = pContext->mConfigPara.mVippFrameRate;
        pContext->mVencChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
        pContext->mVencRcParam.ParamH264Cbr.mMaxQp = 50;
        pContext->mVencRcParam.ParamH264Cbr.mMinQp = 10;
        pContext->mVencRcParam.ParamH264Cbr.mMaxPqp = 50;
        pContext->mVencRcParam.ParamH264Cbr.mMinPqp = 10;
        pContext->mVencRcParam.ParamH264Cbr.mQpInit = 35;
        pContext->mVencRcParam.ParamH264Cbr.mbEnMbQpLimit = 0;
    }
    else if(PT_H265==pContext->mConfigPara.mVideoEncoderFmt)
    {
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicWidth  = pContext->mConfigPara.mVippWidth;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.mVippHeight;

        pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
        pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mBitRate = pContext->mConfigPara.mVideoBitrate*1024*1024;
        pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = pContext->mConfigPara.mVippFrameRate;
        pContext->mVencChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = pContext->mConfigPara.mVideoFrameRate;
        pContext->mVencRcParam.ParamH265Cbr.mMaxQp = 50;
        pContext->mVencRcParam.ParamH265Cbr.mMinQp = 10;
        pContext->mVencRcParam.ParamH265Cbr.mMaxPqp = 50;
        pContext->mVencRcParam.ParamH265Cbr.mMinPqp = 10;
        pContext->mVencRcParam.ParamH265Cbr.mQpInit = 35;
        pContext->mVencRcParam.ParamH265Cbr.mbEnMbQpLimit = 0;
    }
    else
    {
        aloge("fatal error! video encode type[%d] need be support!", pContext->mConfigPara.mVideoEncoderFmt);
    }
    alogd("venc ste Rcmode=%d", pContext->mVencChnAttr.RcAttr.mRcMode);
    BOOL nSuccessFlag = FALSE;
    pContext->mVeChn = 0;
    while (pContext->mVeChn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(pContext->mVeChn, &pContext->mVencChnAttr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", pContext->mVeChn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            alogd("venc channel[%d] is exist, find next!", pContext->mVeChn);
            pContext->mVeChn++;
        }
        else
        {
            alogd("create venc channel[%d] ret[0x%x], find next!", pContext->mVeChn, ret);
            pContext->mVeChn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        pContext->mVeChn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return FAILURE;
    }

    AW_MPI_VENC_SetRcParam(pContext->mVeChn, &pContext->mVencRcParam);

    /* set framerate in AW_MPI_VENC_CreateChn */
    /*VENC_FRAME_RATE_S stFrameRate;
    stFrameRate.SrcFrmRate = pContext->mConfigPara.mVippFrameRate;
    stFrameRate.DstFrmRate = pContext->mConfigPara.mVideoFrameRate;
    alogd("set venc framerate:%d", stFrameRate.DstFrmRate);
    AW_MPI_VENC_SetFrameRate(pContext->mVeChn, &stFrameRate);*/

    alogd("set venc timelapse:%dus", pContext->mConfigPara.mTimelapse);
    //AW_MPI_VENC_SetTimeLapse(pContext->mVeChn, (int64_t)50*1000*3);
    AW_MPI_VENC_SetTimeLapse(pContext->mVeChn, pContext->mConfigPara.mTimelapse);
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pContext->mVeChn, &cbInfo);
    }

    //create muxer group and muxer channel.
    nSuccessFlag = FALSE;
    memset(&pContext->mMuxChnAttr, 0, sizeof(MUX_CHN_ATTR_S));
    pContext->mMuxChnAttr.mVideoAttrValidNum = 1;
    pContext->mMuxChnAttr.mVideoAttr[0].mVideoEncodeType = pContext->mConfigPara.mVideoEncoderFmt;
    pContext->mMuxChnAttr.mVideoAttr[0].mWidth = pContext->mConfigPara.mVippWidth;
    pContext->mMuxChnAttr.mVideoAttr[0].mHeight = pContext->mConfigPara.mVippHeight;
    pContext->mMuxChnAttr.mVideoAttr[0].mVideoFrmRate = pContext->mConfigPara.mVideoFrameRate*1000;
    pContext->mMuxChnAttr.mAudioEncodeType = PT_MAX;
    pContext->mMuxChnAttr.mTextEncodeType = PT_MAX;

    //pContext->mMuxChnAttr.mMuxerId = 0;
    pContext->mMuxChnAttr.mMediaFileFormat = MEDIA_FILE_FORMAT_MP4;
    pContext->mMuxChnAttr.mMaxFileDuration = pContext->mConfigPara.mVideoDuration *1000;
    pContext->mMuxChnAttr.mCallbackOutFlag = FALSE;
    pContext->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pContext->mMuxChnAttr.mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
    int nOutputFd = open(pContext->mConfigPara.mVideoFilePath, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (nOutputFd < 0)
    {
        aloge("Failed to open %s", pContext->mConfigPara.mVideoFilePath);
        return FAILURE;
    }
    nSuccessFlag = FALSE;
    pContext->mMuxChn = 0;
    while (pContext->mMuxChn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(pContext->mMuxChn, &pContext->mMuxChnAttr, nOutputFd, 0);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create mux channel[%d] success!", pContext->mMuxChn);
            break;
        }
        else if (ERR_MUX_EXIST == ret)
        {
            alogd("mux channel[%d] is exist, find next!", pContext->mMuxChn);
            pContext->mMuxChn++;
        }
        else
        {
            aloge("fatal error! create mux channel[%d] fail ret[0x%x], find next!", pContext->mMuxChn, ret);
            pContext->mMuxChn++;
        }
    }
    close(nOutputFd);
    if (FALSE==nSuccessFlag)
    {
        aloge("fatal error! create mux channel fail!");
        pContext->mMuxChn = MM_INVALID_CHN;
        return FAILURE;
    }
    else
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(pContext->mMuxChn, &cbInfo);
    }
    //set spspps
    if (PT_H264 == pContext->mConfigPara.mVideoEncoderFmt)
    {
        VencHeaderData H264SpsPpsInfo;
        memset(&H264SpsPpsInfo, 0, sizeof(VencHeaderData));
        AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVeChn, &H264SpsPpsInfo);
        AW_MPI_MUX_SetH264SpsPpsInfo(pContext->mMuxChn, pContext->mVeChn, &H264SpsPpsInfo);
    }
    else if (PT_H265 == pContext->mConfigPara.mVideoEncoderFmt)
    {
        VencHeaderData H265SpsPpsInfo;
        memset(&H265SpsPpsInfo, 0, sizeof(VencHeaderData));
        AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVeChn, &H265SpsPpsInfo);
        AW_MPI_MUX_SetH265SpsPpsInfo(pContext->mMuxChn, pContext->mVeChn, &H265SpsPpsInfo);
    }
    else
    {
        aloge("fatal error! unsupport video encode type[%d]", pContext->mConfigPara.mVideoEncoderFmt);
    }

    //bind component
    if ((pContext->mViDev >= 0 && pContext->mViChn >= 0) && pContext->mVeChn >= 0)
    {
        MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mViDev, pContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
    }
    if (pContext->mVeChn >= 0 && pContext->mMuxChn >= 0)
    {
        MPP_CHN_S MuxChn = {MOD_ID_MUX, 0, pContext->mMuxChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVeChn};
        AW_MPI_SYS_Bind(&VeChn, &MuxChn);
    }
        
    //start record.
    alogd("start");
    ret = AW_MPI_VI_EnableVirChn(pContext->mViDev, pContext->mViChn);
    if (ret != SUCCESS)
    {
        alogd("VI enable error!");
        return FAILURE;
    }

    if (pContext->mVeChn >= 0)
    {
        AW_MPI_VENC_StartRecvPic(pContext->mVeChn);
    }

    if (pContext->mMuxChn >= 0)
    {
        AW_MPI_MUX_StartChn(pContext->mMuxChn);
    }

//    sleep(15);
//    alogd("set venc timelapse:%dus", pContext->mConfigPara.mTimelapse);
//    AW_MPI_VENC_SetTimeLapse(pContext->mVeChn, pContext->mConfigPara.mTimelapse);

    if (pContext->mConfigPara.mOnlineEnable && pContext->mConfigPara.mTimelapse)
    {
        int sensor_fps = 0;
        AW_MPI_ISP_GetSensorFps(pContext->mIspDev, &sensor_fps);
        if (pContext->mConfigPara.mVippFrameRate != sensor_fps)
        {
            alogw("fatal error! framerate: %d sensor_fps: %d", pContext->mConfigPara.mVippFrameRate, sensor_fps);
        }
    }

    //wait to finish.
    if (pContext->mConfigPara.mVideoDuration > 0)
    {
        cdx_sem_down(&pContext->mSemExit);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    //stop record
    alogd("stop");
    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DisableVirChn(pContext->mViDev, pContext->mViChn);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("stop venc");
        AW_MPI_VENC_StopRecvPic(pContext->mVeChn);
    }

    if (pContext->mMuxChn >= 0)
    {
        alogd("stop mux chn");
        AW_MPI_MUX_StopChn(pContext->mMuxChn, FALSE);
    }

    if (pContext->mViChn >= 0)
    {
        AW_MPI_VI_DestroyVirChn(pContext->mViDev, pContext->mViChn);
        AW_MPI_VI_DisableVipp(pContext->mViDev);
        AW_MPI_VI_DestroyVipp(pContext->mViDev);
        AW_MPI_ISP_Stop(pContext->mIspDev);
    }

    if (pContext->mVeChn >= 0)
    {
        alogd("destory venc");
        AW_MPI_VENC_ResetChn(pContext->mVeChn);
        AW_MPI_VENC_DestroyChn(pContext->mVeChn);
        pContext->mVeChn = MM_INVALID_CHN;
    }

    if (pContext->mMuxChn >= 0)
    {
        alogd("destory mux chn");
        AW_MPI_MUX_DestroyChn(pContext->mMuxChn);
        pContext->mMuxChn = MM_INVALID_CHN;
    }

    result = 0;

    alogd("start to free res");
    AW_MPI_SYS_Exit();

    destructSampleTimelapseContext(pContext);
    pContext = NULL;
    gpSampleTimelapseContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
