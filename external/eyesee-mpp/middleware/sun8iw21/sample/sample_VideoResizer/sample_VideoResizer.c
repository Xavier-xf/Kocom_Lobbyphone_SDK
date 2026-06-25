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
#define LOG_TAG "sample_VideoResizer"

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include "plat_log.h"
#include <confparser.h>
#include <media_common_vcodec.h>

#include "sample_VideoResizer.h"
#include "sample_VideoResizer_config.h"

static SampleVideoResizerContext *gpSampleVideoResizerContext = NULL;

static int ParseCmdLine(SampleVideoResizerContext *pContext, int argc, char** argv)
{
    int ret = -1;

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
              ret = 0;
              if (strlen(*argv) >= MAX_FILE_PATH_LEN)
              {
                 aloge("fatal error! file path[%s] too long!", *argv);
              }
              strncpy(pContext->stCmdLinePara.strConfigFilePath, *argv, MAX_FILE_PATH_LEN-1);
              pContext->stCmdLinePara.strConfigFilePath[MAX_FILE_PATH_LEN-1] = '\0';
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            alogd("CmdLine param:\n"
                "\t-path /mnt/extsd/sample_VideoResizer.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static ERRORTYPE loadConfigPara(SampleVideoResizerConfig *pConfig, const char *pConfPath)
{
    int ret;
    char *ptr;
    CONFPARSER_S stConfParser;

    strcpy(pConfig->SrcFile, "/mnt/extsd/test.mp4");
    strcpy(pConfig->DstFile, "/mnt/extsd/resize.mp4");
    pConfig->eEncodeType = PT_H264;
    pConfig->nDstWidth = 1920;
    pConfig->nDstHeight = 1080;
    pConfig->eRotation = ROTATE_NONE;
    pConfig->fBitrate = 1;
    pConfig->nKeyFrameInterval = 100;

    if(pConfPath != NULL)
    {
        ret = createConfParser(pConfPath, &stConfParser);
        if (ret < 0)
        {
            aloge("load conf fail");
            return FAILURE;
        }
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_VIDEORESIZER_SRC_FILE, NULL);
        strcpy(pConfig->SrcFile, ptr);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_VIDEORESIZER_DST_FILE, NULL);
        strcpy(pConfig->DstFile, ptr);
        ptr = (char *)GetConfParaString(&stConfParser, SAMPLE_VIDEORESIZER_ENCODE_TYPE, NULL);
        if (ptr != NULL)
        {
            if (!strcmp(ptr, "h264"))
            {
                pConfig->eEncodeType = PT_H264;
            }
            else if (!strcmp(ptr, "h265"))
            {
                pConfig->eEncodeType = PT_H265;
            }
            else
            {
                aloge("fatal error! encoder type[%s] wrong", ptr);
                pConfig->eEncodeType = PT_H264;
            }
        }
        pConfig->nDstWidth = GetConfParaInt(&stConfParser, SAMPLE_VIDEORESIZER_DST_WIDTH, 0);
        pConfig->nDstHeight = GetConfParaInt(&stConfParser, SAMPLE_VIDEORESIZER_DST_HEIGHT, 0);
        int nRotation = GetConfParaInt(&stConfParser, SAMPLE_VIDEORESIZER_ROTATION, 0);
        switch(nRotation)
        {
            case 0:
            {
                pConfig->eRotation = ROTATE_NONE;
                break;
            }
            case 90:
            {
                pConfig->eRotation = ROTATE_90;
                break;
            }
            case 180:
            {
                pConfig->eRotation = ROTATE_180;
                break;
            }
            case 270:
            {
                pConfig->eRotation = ROTATE_270;
                break;
            }
            default:
            {
                pConfig->eRotation = ROTATE_NONE;
                break;
            }
        }
        pConfig->fBitrate = (float)GetConfParaDouble(&stConfParser, SAMPLE_VIDEORESIZER_BITRATE, 0);
        pConfig->nKeyFrameInterval = GetConfParaInt(&stConfParser, SAMPLE_VIDEORESIZER_KEY_FRAME_INTERVAL, 0);
        destroyConfParser(&stConfParser);
        alogd("file[%s-%s], encType[%d], dstSize[%d-%d], eRot[%d], bitRate[%f]Mbps, KeyFrameItl[%d]", pConfig->SrcFile,
            pConfig->DstFile, pConfig->eEncodeType, pConfig->nDstWidth, pConfig->nDstHeight, pConfig->eRotation,
            pConfig->fBitrate, pConfig->nKeyFrameInterval);
    }
    return SUCCESS;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE result = SUCCESS;
    SampleVideoResizerContext *pContext = (SampleVideoResizerContext*)cookie;

    if (pChn->mModId == MOD_ID_DEMUX)
    {
        switch (event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("demuxChn[%d] to end of file", pChn->mChnId);
                pContext->bDmxOverFlag = true;
                if (pContext->nVdecChn >= 0)
                {
                    AW_MPI_VDEC_SetStreamEof(pContext->nVdecChn, TRUE);
                }
                break;
            }
            default:
            {
                alogd("demuxChn[%d] send event:%d", pChn->mChnId, event);
                break;
            }
        }
    }
    else if (pChn->mModId == MOD_ID_VDEC)
    {
        switch (event)
        {
            case MPP_EVENT_NOTIFY_EOF:
            {
                alogd("vdecChn[%d] to the end of file", pChn->mChnId);
                pContext->bVdecOverFlag = true;
                break;
            }
            default:
            {
                alogd("vdecChn[%d] send event:%d", pChn->mChnId, event);
                break;
            }
        }
    }
    else if (pChn->mModId == MOD_ID_VENC)
    {
        switch (event)
        {
            /*case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                result = ERR_VENC_NOT_SUPPORT;
                break;
            }
            case MPP_EVENT_LINKAGE_VE2ISP_PARAM:
            {
                result = ERR_VENC_NOT_SUPPORT;
                break;
            }*/
            default:
            {
                alogd("vencChn[%d] send event:%d, ignore", pChn->mChnId, event);
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! receive unknown chn[%d-%d-%d] event:%d", pChn->mModId, pChn->mDevId, pChn->mChnId, event);
    }
    return result;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleVideoResizerContext)
    {
        gpSampleVideoResizerContext->bOverFlag = true;
    }
}


SampleVideoResizerContext* createSampleVideoResizerContext()
{
    SampleVideoResizerContext *pContext = (SampleVideoResizerContext*)malloc(sizeof(SampleVideoResizerContext));
    if(NULL == pContext)
    {
        aloge("fatal error! malloc fail!");
    }
    memset(pContext, 0, sizeof(SampleVideoResizerContext));
    return pContext;
}

int freeSampleVideoResizerContext(SampleVideoResizerContext *pContext)
{
    if(pContext->pDstFile)
    {
        aloge("fatal error! not close dstFile[%p]", pContext->pDstFile);
        fclose(pContext->pDstFile);
        pContext->pDstFile = NULL;
    }
    return 0;
}

static int configVideoStreamInfoByDEMUX_VIDEO_STREAM_INFO_S(VideoStreamInfo *pStreamInfo, DEMUX_VIDEO_STREAM_INFO_S *pMppVideoStreamInfo)
{
    memset(pStreamInfo, 0, sizeof(VideoStreamInfo));
    pStreamInfo->eCodecFormat = map_PAYLOAD_TYPE_E_to_EVIDEOCODECFORMAT(pMppVideoStreamInfo->mCodecType);
    pStreamInfo->nWidth = pMppVideoStreamInfo->mWidth;
    pStreamInfo->nHeight = pMppVideoStreamInfo->mHeight;
    pStreamInfo->nFrameRate = pMppVideoStreamInfo->mFrameRate;
    pStreamInfo->nCodecSpecificDataLen = pMppVideoStreamInfo->nCodecSpecificDataLen;
    pStreamInfo->pCodecSpecificData = pMppVideoStreamInfo->pCodecSpecificData;
    pStreamInfo->bIsFramePackage = 1;
    return 0;
}

static int configVDEC_STREAM_SByDmxOutBuf(VDEC_STREAM_S *pVdecStream, EncodedStream *pDmxEncodedStream)
{
    memset(pVdecStream, 0, sizeof(VDEC_STREAM_S));
    pVdecStream->pAddr = pDmxEncodedStream->pBuffer;
    pVdecStream->mLen = pDmxEncodedStream->nFilledLen;
    pVdecStream->mPTS = pDmxEncodedStream->nTimeStamp;
    pVdecStream->mbEndOfFrame = (pDmxEncodedStream->nFlags & CEDARV_FLAG_LAST_PART)?TRUE:FALSE;
    return 0;
}

int main(int argc, char** argv)
{
    int result = 0;
    GLogConfig stGLogConfig = 
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
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
    
    alogd("Hello, sample_VideoResizer!");
    SampleVideoResizerContext *pContext = createSampleVideoResizerContext();
    if(NULL == pContext)
    {
        aloge("fatal error! create SampleVideoResizerContext fail");
    }
    gpSampleVideoResizerContext = pContext;
    //parse command line param
    if(ParseCmdLine(pContext, argc, argv) != 0)
    {
        aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto err_out_0;
    }
    char *pConfigFilePath;
    if(strlen(pContext->stCmdLinePara.strConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->stCmdLinePara.strConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }
    //parse config file.
    if(loadConfigPara(&pContext->stConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto err_out_0;
    }
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("fatal error! can't catch SIGSEGV");
    }

    int nSrcFd = open(pContext->stConfigPara.SrcFile, O_RDONLY);
    if(nSrcFd < 0)
    {
        aloge("fatal error: cannot open file[%s]", pContext->stConfigPara.SrcFile);
        goto err_out_0;
    }
    pContext->pDstFile = fopen(pContext->stConfigPara.DstFile, "wb");
    if (NULL == pContext->pDstFile)
    {
        aloge("fatal error: cannot open file[%s]", pContext->stConfigPara.DstFile);
        goto err_out_0;
    }

    ERRORTYPE ret;
    AW_MPI_SYS_SetConf(&pContext->stSysConf);
    AW_MPI_SYS_Init();
    //config demux
    pContext->stDmxChnAttr.mSourceType = SOURCETYPE_FD;
    pContext->stDmxChnAttr.mFd = nSrcFd;
    pContext->stDmxChnAttr.mDemuxDisableTrack = DEMUX_DISABLE_SUBTITLE_TRACK|DEMUX_DISABLE_AUDIO_TRACK;
    pContext->nDmxChn = 0;
    ret = AW_MPI_DEMUX_CreateChn(pContext->nDmxChn, &pContext->stDmxChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! create demux channel fail, ret[0x%x]!", ret);
    }

    close(nSrcFd);
    nSrcFd = -1;

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_DEMUX_RegisterCallback(pContext->nDmxChn, &cbInfo);
    ret = AW_MPI_DEMUX_GetMediaInfo(pContext->nDmxChn, &pContext->stDemuxMediaInfo);
    if (ret != SUCCESS)
    {
        aloge("fatal error! get media info fail[0x%x]!", ret);
    }
    alogd("streamIdx[%d-%d][%d-%d][%d-%d]", pContext->stDemuxMediaInfo.mVideoIndex, pContext->stDemuxMediaInfo.mVideoNum,
        pContext->stDemuxMediaInfo.mAudioIndex, pContext->stDemuxMediaInfo.mAudioNum, pContext->stDemuxMediaInfo.mSubtitleIndex,
        pContext->stDemuxMediaInfo.mSubtitleNum);
    if(pContext->stDemuxMediaInfo.mVideoNum <= 0)
    {
        aloge("fatal error! no video!");
        goto err_out_1;
    }
    DEMUX_VIDEO_STREAM_INFO_S *pMppVideoStreamInfo = &pContext->stDemuxMediaInfo.mVideoStreamInfo[pContext->stDemuxMediaInfo.mVideoIndex];
    alogd("vstrmIdx[%d]:type[%d], size[%dx%d], fps[%f], bitRate[%d-%d]", pContext->stDemuxMediaInfo.mVideoIndex, pMppVideoStreamInfo->mCodecType,
        pMppVideoStreamInfo->mWidth, pMppVideoStreamInfo->mHeight, (float)pMppVideoStreamInfo->mFrameRate/1000, pMppVideoStreamInfo->mAvgBitsRate,
        pMppVideoStreamInfo->mMaxBitsRate);
    //config vdec
    memset(&pContext->stVdecChnAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    pContext->stVdecChnAttr.mType = pMppVideoStreamInfo->mCodecType;
    pContext->stVdecChnAttr.mBufSize = AWALIGN(pMppVideoStreamInfo->mAvgBitsRate*4/8, 1024); //It is enough to set vbvBuffer to contain 4s bitstream.
    pContext->stVdecChnAttr.mOutputPixelFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    pContext->nVdecChn = 0;
    ret = AW_MPI_VDEC_CreateChn(pContext->nVdecChn, &pContext->stVdecChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! create vdec channel[%d] fail[0x%x]!", pContext->nVdecChn, ret);
    }
    AW_MPI_VDEC_RegisterCallback(pContext->nVdecChn, &cbInfo);
    VideoStreamInfo stStreamInfo;
    configVideoStreamInfoByDEMUX_VIDEO_STREAM_INFO_S(&stStreamInfo, pMppVideoStreamInfo);
    AW_MPI_VDEC_SetVideoStreamInfo(pContext->nVdecChn, &stStreamInfo);
    //config venc
    memset(&pContext->stVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    pContext->stVencChnAttr.VeAttr.Type = pContext->stConfigPara.eEncodeType;
    pContext->stVencChnAttr.VeAttr.MaxKeyInterval = pContext->stConfigPara.nKeyFrameInterval;
    pContext->stVencChnAttr.VeAttr.SrcPicWidth  = AWALIGN(pMppVideoStreamInfo->mWidth, 32);
    pContext->stVencChnAttr.VeAttr.SrcPicHeight = AWALIGN(pMppVideoStreamInfo->mHeight, 32);
    pContext->stVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->stVencChnAttr.VeAttr.PixelFormat = pContext->stVdecChnAttr.mOutputPixelFormat;
    pContext->stVencChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_REC709;
    pContext->stVencChnAttr.VeAttr.Rotate = pContext->stConfigPara.eRotation;
    pContext->stVencChnAttr.VeAttr.mVeRefFrameLbcMode = VENC_REF_FRAME_LBC_MODE_DEFAULT;
    pContext->stVencChnAttr.VeAttr.mVeRecRefBufReduceEnable = 0;
    pContext->stVencChnAttr.EncppAttr.eEncppSharpSetting = VencEncppSharp_Disable;
    pContext->stVencChnAttr.RcAttr.mProductMode = PRODUCT_STATIC_IPC;
    if (PT_H264 == pContext->stVencChnAttr.VeAttr.Type)
    {
        int nThreshSize = pContext->stVencChnAttr.VeAttr.SrcPicWidth*pContext->stVencChnAttr.VeAttr.SrcPicHeight;
        pContext->stVencChnAttr.VeAttr.AttrH264e.BufSize = pContext->stConfigPara.fBitrate*1000000*2/8 + nThreshSize;
        pContext->stVencChnAttr.VeAttr.AttrH264e.mThreshSize = nThreshSize;
        pContext->stVencChnAttr.VeAttr.AttrH264e.Profile = 2;
        pContext->stVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->stVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->stConfigPara.nDstWidth;
        pContext->stVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->stConfigPara.nDstHeight;
        pContext->stVencChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_Default; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        pContext->stVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = (unsigned int)(pContext->stConfigPara.fBitrate*1000000);
        pContext->stVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pMppVideoStreamInfo->mFrameRate/1000;
        pContext->stVencChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = pMppVideoStreamInfo->mFrameRate/1000;
        pContext->stVencRcParam.ParamH264Vbr.mMinQp = 25;
        pContext->stVencRcParam.ParamH264Vbr.mMaxQp = 45;
        pContext->stVencRcParam.ParamH264Vbr.mMinPqp = 25;
        pContext->stVencRcParam.ParamH264Vbr.mMaxPqp = 45;
        pContext->stVencRcParam.ParamH264Vbr.mQpInit = 37;
        pContext->stVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 1;
        pContext->stVencRcParam.ParamH264Vbr.mMovingTh = 20;
        pContext->stVencRcParam.ParamH264Vbr.mQuality = 10;
        pContext->stVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
        pContext->stVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    }
    else if (PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
    {
        int nThreshSize = pContext->stVencChnAttr.VeAttr.SrcPicWidth*pContext->stVencChnAttr.VeAttr.SrcPicHeight;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mBufSize = pContext->stConfigPara.fBitrate*1000000*2/8 + nThreshSize;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mThreshSize = nThreshSize;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->stConfigPara.nDstWidth;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->stConfigPara.nDstHeight;
        pContext->stVencChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_Default; /* set the default value 0 and encoder will adjust automatically. */
        pContext->stVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->stVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        pContext->stVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = (unsigned int)(pContext->stConfigPara.fBitrate*1000000);
        pContext->stVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pMppVideoStreamInfo->mFrameRate/1000;
        pContext->stVencChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = pMppVideoStreamInfo->mFrameRate/1000;
        pContext->stVencRcParam.ParamH265Vbr.mMinQp = 25;
        pContext->stVencRcParam.ParamH265Vbr.mMaxQp = 45;
        pContext->stVencRcParam.ParamH265Vbr.mMinPqp = 25;
        pContext->stVencRcParam.ParamH265Vbr.mMaxPqp = 45;
        pContext->stVencRcParam.ParamH265Vbr.mQpInit = 37;
        pContext->stVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 1;
        pContext->stVencRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->stVencRcParam.ParamH265Vbr.mQuality = 10;
        pContext->stVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
        pContext->stVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }
    else
    {
        aloge("fatal error! not support encode type:%d", pContext->stVencChnAttr.VeAttr.Type);
    }
    alogd("venc set Rcmode=%d", pContext->stVencChnAttr.RcAttr.mRcMode);
    pContext->stVencChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pContext->stVencChnAttr.GopAttr.mGopSize = 2;
    pContext->nVencChn = 0;
    ret = AW_MPI_VENC_CreateChn(pContext->nVencChn, &pContext->stVencChnAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! create vencChn[%d] ret[0x%x]!", pContext->nVencChn, ret);
    }
    ret = AW_MPI_VENC_SetRcParam(pContext->nVencChn, &pContext->stVencRcParam);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vencChn[%d] set rcParam fail[0x%x]!", pContext->nVencChn, ret);
    }
    if (PT_H264 == pContext->stVencChnAttr.VeAttr.Type || PT_H265 == pContext->stVencChnAttr.VeAttr.Type)
    {

        int dstWidthAlign = AWALIGN(pMppVideoStreamInfo->mWidth, 16);
        int dstHeightAlign = AWALIGN(pMppVideoStreamInfo->mHeight, 8);
        if (dstWidthAlign != pMppVideoStreamInfo->mWidth || dstHeightAlign != pMppVideoStreamInfo->mHeight)
        {
            VencForceConfWin stConfWin;
            memset(&stConfWin, 0, sizeof(VencForceConfWin));
            stConfWin.en_force_conf = 1;
            stConfWin.left_offset = 0;
            stConfWin.right_offset = dstWidthAlign - pMppVideoStreamInfo->mWidth;
            stConfWin.top_offset = 0;
            stConfWin.bottom_offset = dstHeightAlign - pMppVideoStreamInfo->mHeight;
            alogd("Be careful! set ForceConfWin en %d, left_offset:%d, right_offset:%d, top_offset:%d, bottom_offset:%d, rotate:%d",
                stConfWin.en_force_conf, stConfWin.left_offset, stConfWin.right_offset, stConfWin.top_offset,
                stConfWin.bottom_offset, pContext->stConfigPara.eRotation);
//            ret = AW_MPI_VENC_SetForceConfWin(pContext->nVencChn, &stConfWin);
//            if(ret != SUCCESS)
//            {
//                aloge("fatal error! vencChn[%d] set force confWin fail[0x%x]!", pContext->nVencChn, ret);
//            }
        }
    }
    AW_MPI_VENC_RegisterCallback(pContext->nVencChn, &cbInfo);
    ret = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->nVencChn, &pContext->stSpsPpsInfo);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vencChn[%d] get spspps fail[0x%x]", pContext->nVencChn, ret);
    }

    //bind vdec and venc
    MPP_CHN_S stVdecChn = {MOD_ID_VDEC, 0, pContext->nVdecChn};
    MPP_CHN_S stVencChn = {MOD_ID_VENC, 0, pContext->nVencChn};
    ret = AW_MPI_SYS_Bind(&stVdecChn, &stVencChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! bind vdec&venc fail[0x%x]!", ret);
    }
    //start demux, vdec, venc
    ret = AW_MPI_DEMUX_Start(pContext->nDmxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! start dmxChn[%d] fail[0x%x]!", pContext->nDmxChn, ret);
    }
    ret = AW_MPI_VDEC_StartRecvStream(pContext->nVdecChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! start vdecChn[%d] fail[0x%x]!", pContext->nVdecChn, ret);
    }
    ret = AW_MPI_VENC_StartRecvPic(pContext->nVencChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! start vencChn[%d] fail[0x%x]!", pContext->nVencChn, ret);
    }

    EncodedStream demuxOutBuf;
    VDEC_STREAM_S stVdecStream;
    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    memset(&stVencStream, 0, sizeof(stVencStream));
    memset(&stVencPack, 0, sizeof(stVencPack));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;
    while(1)
    {
        if(pContext->bOverFlag)
        {
            alogd("detect over flag, exit");
            break;
        }
        //1. get stream from demux, send it to vdec.
        ret = AW_MPI_DEMUX_getDmxOutPutBuf(pContext->nDmxChn, &demuxOutBuf, 200);
        if(SUCCESS == ret)
        {
            if(demuxOutBuf.media_type == CDX_PacketVideo)
            {
                configVDEC_STREAM_SByDmxOutBuf(&stVdecStream, &demuxOutBuf);
                ret = AW_MPI_VDEC_SendStream(pContext->nVdecChn, &stVdecStream, 1000);
                if(ret != SUCCESS)
                {
                    aloge("fatal error! vdecChn[%d] send stream fail[0x%x]", pContext->nVdecChn, ret);
                }
            }
            else
            {
                aloge("fatal error! why media_type:%d?", demuxOutBuf.media_type);
            }
            ret = AW_MPI_DEMUX_releaseDmxBuf(pContext->nDmxChn, &demuxOutBuf);
            if (ret != SUCCESS)
            {
                aloge("fatal error! dmxChn[%d] release dmxBuf fail[0x%x]", pContext->nDmxChn, ret);
            }
        }
        else
        {
            alogd("Be careful! dmxChn[%d] get outBuf fail[0x%x-%d]", pContext->nDmxChn, ret, pContext->bDmxOverFlag);
        }
        //2. get stream from venc.
        ret = AW_MPI_VENC_GetStream(pContext->nVencChn, &stVencStream, 100);
        if(SUCCESS == ret)
        {
            size_t nBytesNum;
            bool bKeyFrame = false;
            if(PT_H264 == pContext->stConfigPara.eEncodeType)
            {
                if(H264E_NALU_ISLICE == stVencStream.mpPack[0].mDataType.enH264EType)
                {
                    bKeyFrame = true;
                }
            }
            else if(PT_H265 == pContext->stConfigPara.eEncodeType)
            {
                if(H265E_NALU_ISLICE == stVencStream.mpPack[0].mDataType.enH265EType)
                {
                    bKeyFrame = true;
                }
            }
            if(bKeyFrame)
            {
                nBytesNum = fwrite(pContext->stSpsPpsInfo.pBuffer, 1, pContext->stSpsPpsInfo.nLength, pContext->pDstFile);
                if(nBytesNum != pContext->stSpsPpsInfo.nLength)
                {
                    aloge("fatal error! fwrite fail[%d!=%d]", nBytesNum, pContext->stSpsPpsInfo.nLength);
                }
            }
            if(stVencStream.mpPack[0].mpAddr0 != NULL)
            {
                nBytesNum = fwrite(stVencStream.mpPack[0].mpAddr0, 1, stVencStream.mpPack[0].mLen0, pContext->pDstFile);
                if(nBytesNum != stVencStream.mpPack[0].mLen0)
                {
                    aloge("fatal error! fwrite fail[%d!=%d]", nBytesNum, stVencStream.mpPack[0].mLen0);
                }
            }
            if(stVencStream.mpPack[0].mpAddr1 != NULL)
            {
                nBytesNum = fwrite(stVencStream.mpPack[0].mpAddr1, 1, stVencStream.mpPack[0].mLen1, pContext->pDstFile);
                if(nBytesNum != stVencStream.mpPack[0].mLen1)
                {
                    aloge("fatal error! fwrite fail[%d!=%d]", nBytesNum, stVencStream.mpPack[0].mLen1);
                }
            }
            ret = AW_MPI_VENC_ReleaseStream(pContext->nVencChn, &stVencStream);
            if(ret != SUCCESS)
            {
                aloge("fatal error! vencChn[%d] release stream fail[0x%x]", pContext->nVencChn, ret);
            }
        }
        else
        {
            if(pContext->bVdecOverFlag)
            {
                alogd("set overflag! vencChn[%d] get stream fail[0x%x]", pContext->nVencChn, ret);
                pContext->bOverFlag = true;
            }
            else
            {
                alogd("Be careful! vencChn[%d] get stream fail[0x%x]", pContext->nVencChn, ret);
            }
        }
    }

    ret = AW_MPI_DEMUX_Stop(pContext->nDmxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! dmxChn[%d] stop fail[0x%x]", pContext->nDmxChn, ret);
    }
    ret = AW_MPI_VDEC_StopRecvStream(pContext->nVdecChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vdecChn[%d] stop fail[0x%x]", pContext->nVdecChn, ret);
    }
    ret = AW_MPI_VENC_StopRecvPic(pContext->nVencChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vencChn[%d] stop fail[0x%x]", pContext->nVencChn, ret);
    }
    ret = AW_MPI_VENC_DestroyChn(pContext->nVencChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vencChn[%d] destroy fail[0x%x]", pContext->nVencChn, ret);
    }
    pContext->nVencChn = MM_INVALID_CHN;
    ret = AW_MPI_VDEC_DestroyChn(pContext->nVdecChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! vdecChn[%d] destroy fail[0x%x]", pContext->nVdecChn, ret);
    }
    pContext->nVdecChn = MM_INVALID_CHN;
    ret = AW_MPI_DEMUX_DestroyChn(pContext->nDmxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! dmxChn[%d] destroy fail[0x%x]", pContext->nDmxChn, ret);
    }
    pContext->nDmxChn = MM_INVALID_CHN;

    //exit mpp system
    ret = AW_MPI_SYS_Exit();
    if(ret != SUCCESS)
    {
        aloge("fatal error! aw mpi sys exit fail[0x%x]", ret);
    }
    if(pContext->pDstFile)
    {
        fclose(pContext->pDstFile);
        pContext->pDstFile = NULL;
    }
    freeSampleVideoResizerContext(pContext);
    gpSampleVideoResizerContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;

err_out_1:
    ret = AW_MPI_DEMUX_DestroyChn(pContext->nDmxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! dmxChn[%d] destroy fail[0x%x]", pContext->nDmxChn, ret);
    }
    //exit mpp system
    AW_MPI_SYS_Exit();
    if(pContext->pDstFile)
    {
        fclose(pContext->pDstFile);
        pContext->pDstFile = NULL;
    }
err_out_0:
    freeSampleVideoResizerContext(pContext);
    gpSampleVideoResizerContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    log_quit();
    return result;
}
