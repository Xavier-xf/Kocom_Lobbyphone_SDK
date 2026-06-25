
#define LOG_TAG "demo_codec_parallel"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>
#include <signal.h>

//#include <linux/videodev2.h>
//#include <media/sunxi_camera_v2.h>
#include <AW_VideoInput_API.h>
#include <plat_log.h>
#include <mm_comm_venc.h>
#include <mpi_sys.h>
#include <confparser.h>

#include "demo_codec_parallel.h"
#include "demo_codec_parallel_config.h"

#define SUPPORT_VENC        1
#define SUPPORT_VENC_SUB    1
#define SUPPORT_VDEC        1

DemoCodecParallelContext *gpDemoCodecParallelContext = NULL;

static int parseCmdLine(DemoCodecParallelContext *pContext, int argc, char** argv)
{
    int ret = -1;

    alogd("argc=%d", argc);

    if (argc != 3)
    {
        printf("CmdLine param:\n"
                "\t-path ./sample_CodecParallel.conf\n");
        return -1;
    }

    while (*argv)
    {
       if (!strcmp(*argv, "-path"))
       {
          argv++;
          if (*argv)
          {
                ret = 0;
                if (strlen(*argv) >= MAX_FILE_PATH_SIZE)
                {
                    aloge("fatal error! file path[%s] too long:!", *argv);
                }

                if (pContext)
                {
                    strncpy(pContext->mCmdLinePara.mConfigFilePath, *argv, MAX_FILE_PATH_SIZE-1);
                    pContext->mCmdLinePara.mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
                }
          }
       }
       else if(!strcmp(*argv, "-h"))
       {
            printf("CmdLine param:\n"
                "\t-path ./sample_CodecParallel.conf\n");
            break;
       }
       else if (*argv)
       {
          argv++;
       }
    }

    return ret;
}

static RT_PIXELFORMAT_TYPE getRTPicFormatFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    RT_PIXELFORMAT_TYPE PicFormat = MM_PIXEL_FORMAT_BUTT;
    char *pStrPixelFormat = (char*)GetConfParaString(pConfParser, key, NULL);

    if (!strcmp(pStrPixelFormat, "yu12"))
    {
        PicFormat = RT_PIXEL_YUV420P;
    }
    else if (!strcmp(pStrPixelFormat, "yv12"))
    {
        PicFormat = RT_PIXEL_YVU420P;
    }
    else if (!strcmp(pStrPixelFormat, "nv21"))
    {
        PicFormat = RT_PIXEL_YVU420SP;
    }
    else if (!strcmp(pStrPixelFormat, "nv12"))
    {
        PicFormat = RT_PIXEL_YUV420SP;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_2_0x"))
    {
        PicFormat = RT_PIXEL_LBC_2X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_2_5x"))
    {
        PicFormat = RT_PIXEL_LBC_25X;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        PicFormat = RT_PIXEL_YVU420SP;
    }

    return PicFormat;
}

static int getRTVideoTypeFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    int EncType = 0;
    char *ptr = (char *)GetConfParaString(pConfParser, key, NULL);
    if(!strcmp(ptr, "H.264"))
    {
        EncType = 0;
    }
    else if(!strcmp(ptr, "MJPEG"))
    {
        EncType = 1;
    }
    else if(!strcmp(ptr, "H.265"))
    {
        EncType = 2;
    }
    else
    {
        aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
        EncType = 0;
    }

    return EncType;
}

static PIXEL_FORMAT_E getPicFormatFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PIXEL_FORMAT_E PicFormat = MM_PIXEL_FORMAT_BUTT;
    char *pStrPixelFormat = (char*)GetConfParaString(pConfParser, key, NULL);

    if (!strcmp(pStrPixelFormat, "yu12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "yv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv21"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv21s"))
    {
        PicFormat = MM_PIXEL_FORMAT_AW_NV21S;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }

    return PicFormat;
}

static PAYLOAD_TYPE_E getVideoTypeFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PAYLOAD_TYPE_E EncType = PT_BUTT;
    char *ptr = (char *)GetConfParaString(pConfParser, key, NULL);
    if(!strcmp(ptr, "H.264"))
    {
        EncType = PT_H264;
    }
    else if(!strcmp(ptr, "H.265"))
    {
        EncType = PT_H265;
    }
    else if(!strcmp(ptr, "MJPEG"))
    {
        EncType = PT_MJPEG;
    }
    else if(!strcmp(ptr, "JPEG"))
    {
        EncType = PT_JPEG;
    }
    else
    {
        aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
        EncType = PT_H264;
    }

    return EncType;
}

static void convertStrDispType(char *pStrDispType, int DisplayWidth, VO_INTF_TYPE_E *pDispType, VO_INTF_SYNC_E *pDispSync)
{
    if (!strcmp(pStrDispType, "hdmi"))
    {
        *pDispType = VO_INTF_HDMI;
        if (DisplayWidth > 1920)
            *pDispSync = VO_OUTPUT_3840x2160_30;
        else if (DisplayWidth > 1280)
            *pDispSync = VO_OUTPUT_1080P30;
        else
            *pDispSync = VO_OUTPUT_720P60;
    }
    else if (!strcmp(pStrDispType, "lcd"))
    {
        *pDispType = VO_INTF_LCD;
        *pDispSync = VO_OUTPUT_NTSC;
    }
    else if (!strcmp(pStrDispType, "cvbs"))
    {
        *pDispType = VO_INTF_CVBS;
        *pDispSync = VO_OUTPUT_NTSC;
    }
}

static ERRORTYPE loadConfigPara(DemoCodecParallelContext *pContext, const char *conf_path)
{
	char *ptr = NULL;
    if (NULL == pContext)
    {
        aloge("pContext is NULL!");
        return FAILURE;
    }

    if (NULL == conf_path)
    {
        aloge("user not set config file!");
        return FAILURE;
    }

    CONFPARSER_S stConfParser;
    if (createConfParser(conf_path, &stConfParser) != 0)
    {
        aloge("load conf fail!");
        return FAILURE;
    }

    /* common configure */
    pContext->mConfigPara.mTestDuration = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_TEST_DURATION, 0);
    alogd("testDuration[%d]s", pContext->mConfigPara.mTestDuration);

    /* record configure */
    pContext->mConfigPara.mRecordEnable = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_ENABLE, 0);
    pContext->mConfigPara.mVippID = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIPP_ID, 0);
    pContext->mConfigPara.mOnlineEnable = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_ONLINE_EN, 0);
    pContext->mConfigPara.mOnlineShareBufNum = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_ONLINE_SHARE_BUF_NUM, 0);
    pContext->mConfigPara.mVippBufNum = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIPP_BUF_NUM, 0);
    pContext->mConfigPara.mVippPixelFormat = getRTPicFormatFromConfig(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIDEO_PIXFORMAT);
    pContext->mConfigPara.mVippColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
    pContext->mConfigPara.mVippWidth = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIPP_CAPTURE_WIDTH, 0);
    pContext->mConfigPara.mVippHeight = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIPP_CAPTURE_HEIGHT, 0);
    pContext->mConfigPara.mVippFrameRate = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIPP_FRAME_RATE, 0);
    pContext->mConfigPara.mVideoFrameRate = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIDEO_FRAME_RATE, 0);
    pContext->mConfigPara.mVideoBitrate = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIDEO_BITRATE, 0);
    pContext->mConfigPara.mVideoWidth = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIDEO_WIDTH, 0);
    pContext->mConfigPara.mVideoHeight = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIDEO_HEIGHT, 0);
    pContext->mConfigPara.mVideoEncoderType = getRTVideoTypeFromConfig(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIDEO_ENCODER_TYPE);
    pContext->mConfigPara.mVideoRcMode = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIDEO_RATE_CTRL_MODE, 0);
    ptr = (char *)GetConfParaString(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_DEST_FILE_STR, NULL);
    strncpy(pContext->mConfigPara.mDestVideoFile, ptr, MAX_FILE_PATH_SIZE);
    pContext->mConfigPara.mRecordDuration = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_VIDEO_DURATION, 0);

    /* record sub configure */
    pContext->mConfigPara.mSubRecordEnable = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_ENABLE, 0);
    pContext->mConfigPara.mSubVippID = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIPP_ID, 0);
    pContext->mConfigPara.mSubVippBufNum = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIPP_BUF_NUM, 0);
    pContext->mConfigPara.mSubVippPixelFormat = getRTPicFormatFromConfig(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIDEO_PIXFORMAT);
    pContext->mConfigPara.mSubVippColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
    pContext->mConfigPara.mSubVippWidth = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIPP_CAPTURE_WIDTH, 0);
    pContext->mConfigPara.mSubVippHeight = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIPP_CAPTURE_HEIGHT, 0);
    pContext->mConfigPara.mSubVippFrameRate = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIPP_FRAME_RATE, 0);
    pContext->mConfigPara.mSubVideoFrameRate = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIDEO_FRAME_RATE, 0);
    pContext->mConfigPara.mSubVideoBitrate = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIDEO_BITRATE, 0);
    pContext->mConfigPara.mSubVideoWidth = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIDEO_WIDTH, 0);
    pContext->mConfigPara.mSubVideoHeight = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIDEO_HEIGHT, 0);
    pContext->mConfigPara.mSubVideoEncoderType = getRTVideoTypeFromConfig(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIDEO_ENCODER_TYPE);
    pContext->mConfigPara.mSubVideoRcMode = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIDEO_RATE_CTRL_MODE, 0);
    ptr = (char *)GetConfParaString(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_DEST_FILE_STR, NULL);
    strncpy(pContext->mConfigPara.mSubDestVideoFile, ptr, MAX_FILE_PATH_SIZE);
    pContext->mConfigPara.mSubRecordDuration = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_RECORD_SUB_VIDEO_DURATION, 0);

    /* play configure */
    pContext->mConfigPara.mPlayEnable = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_ENABLE, 0);
    pContext->mConfigPara.mVdecChn = 0;
	pContext->mConfigPara.mVideoDecoderType = getVideoTypeFromConfig(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VIDEO_DECODER_TYPE);
    ptr = (char *)GetConfParaString(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_SRC_FILE_STR, NULL);
    strncpy(pContext->mConfigPara.mSrcVideoFile, ptr, MAX_FILE_PATH_SIZE);
	ptr = (char *)GetConfParaString(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_SRC_LEN_FILE_STR, NULL);
    strncpy(pContext->mConfigPara.mSrcVideoLenFile, ptr, MAX_FILE_PATH_SIZE);
    pContext->mConfigPara.mMaxVdecOutputWidth = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VDEC_OUTPUT_WIDTH, 0);
    pContext->mConfigPara.mMaxVdecOutputHeight = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VDEC_OUTPUT_HEIGHT, 0);
    pContext->mConfigPara.mVdecBufSize = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VDEC_BUF_SIZE, 0);
    pContext->mConfigPara.mVdecOutputPixelFormat = getPicFormatFromConfig(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VIDEO_PIXFORMAT);
    pContext->mConfigPara.mUILayer = HLAY(2, 0);
    pContext->mConfigPara.mVoDev = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VO_DEV_ID, 0);
    pContext->mConfigPara.mVoChn = 0;
    pContext->mConfigPara.mDisplayX = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VO_DISPLAY_X, 0);
    pContext->mConfigPara.mDisplayY = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VO_DISPLAY_Y, 0);
    pContext->mConfigPara.mDisplayWidth = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VO_DISPLAY_WIDTH, 0);
    pContext->mConfigPara.mDisplayHeight = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VO_DISPLAY_HEIGHT, 0);
    pContext->mConfigPara.mVoLayer = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VO_LAYER_NUM, 0);
    ptr = (char*)GetConfParaString(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_VO_DISP_TYPE, NULL);
    convertStrDispType(ptr, pContext->mConfigPara.mDisplayWidth, &pContext->mConfigPara.mDispType, &pContext->mConfigPara.mDispSync);
    pContext->mConfigPara.mPlayFrameRate = GetConfParaInt(&stConfParser, DEMO_CODEC_PARALLEL_PLAY_FRAME_RATE, 0);

    destroyConfParser(&stConfParser);

    return SUCCESS;
}

static void video_stream_cb(const AWVideoInput_StreamInfo* stream_info)
{
    ERRORTYPE ret;
    DemoCodecParallelContext *pContext = gpDemoCodecParallelContext;

    int channel_id = stream_info->channel_id;
    DemoCodecParallelStatistics *pStatis = NULL;
    FILE **pOutFile = NULL;
    char *pDestVideoFile = NULL;

    if (channel_id == pContext->mConfigPara.mVippID)
    {
        pStatis = &pContext->mStatis;
        pOutFile = &pContext->mOutFile;
        pDestVideoFile = pContext->mConfigPara.mDestVideoFile;
    }
    else if (channel_id == pContext->mConfigPara.mSubVippID)
    {
        pStatis = &pContext->mSubStatis;
        pOutFile = &pContext->mSubOutFile;
        pDestVideoFile = pContext->mConfigPara.mSubDestVideoFile;
    }
    else
    {
        aloge("fatal error, invalid channel_id %d", channel_id);
        return;
    }

    if (stream_info->data0 != NULL && stream_info->size0 > 0 && pStatis->stream_count_0 < pStatis->max_bitstream_count)
    {
        uint64_t cur_time = get_cur_time_us();

        uint64_t pts = stream_info->pts; //unit:us
        uint64_t pts_in_seconds = pts/1000/1000;

        if (pStatis->stream_count_0%15 == 0)
        {
            alogd("*ch%d data-len:%p-%d,%p-%d,%p-%d, cnt = %d, kf = %d, pts = %llu us(%llu s), diff = %llu ms,  cur_time = %llu us, time-pts-diff = %llu ms",
                    channel_id, stream_info->data0, stream_info->size0, stream_info->data1, stream_info->size1, stream_info->data2, stream_info->size2,
                    pStatis->stream_count_0, stream_info->keyframe_flag,
                    pts, pts/1000000, (pts - pStatis->pre_pts)/1000, cur_time, (cur_time - pts)/1000);
        }

        if(pts_in_seconds > 0xFFFFFFFF) //0xFFFFFFFF is unsigned int.
        {
            aloge("fatal error! stream_data.pts[%llu]us,[%llu]s, too large!", pts, pts_in_seconds);
        }
        if (pts_in_seconds != pStatis->pre_pts_in_seconds)
        {
            alogd("ch%d get video stream, fps = %d,[%lld-%d]s", channel_id, pStatis->cb_stream_cnt_in_seconds, pts_in_seconds, pStatis->pre_pts_in_seconds);
            pStatis->pre_pts_in_seconds = pts_in_seconds;
            pStatis->cb_stream_cnt_in_seconds = 0;
        }
        pStatis->cb_stream_cnt_in_seconds++;

        pStatis->pre_pts = pts;

        if (NULL == *pOutFile)
        {
            FILE *fp = fopen(pDestVideoFile, "wb");
            if (NULL == fp)
            {
                aloge("fatal error, fopen file %s failed!", pDestVideoFile);
            }
            else
            {
                *pOutFile = fp;
                alogd("%s open success, fp:%p", pDestVideoFile, fp);
            }
        }
        if (*pOutFile)
        {
            if (stream_info->b_insert_sps_pps && stream_info->sps_pps_size && stream_info->sps_pps_buf)
                fwrite(stream_info->sps_pps_buf, 1, stream_info->sps_pps_size, *pOutFile);

            fwrite(stream_info->data0, 1, stream_info->size0, *pOutFile);

            if (stream_info->size1)
                fwrite(stream_info->data1, 1, stream_info->size1, *pOutFile);
            if (stream_info->size2)
                fwrite(stream_info->data2, 1, stream_info->size2, *pOutFile);
        }
    }
    else
    {
        alogv("ch%d stream data = %p, len = %d, streamCount:%d >= %d", channel_id, stream_info->data0, stream_info->size0, pStatis->stream_count_0, pStatis->max_bitstream_count);
        //return;
    }
    pStatis->stream_count_0++;
    if (pStatis->stream_count_0 >= pStatis->max_bitstream_count && !pStatis->video_finish_flag)
    {
        pStatis->video_finish_flag = 1;
        alogd("ch%d set video_finish_flag = 1", channel_id);
        sem_post(&pContext->mSemExit);
    }
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    DemoCodecParallelContext *pContext = (DemoCodecParallelContext *)cookie;

    if (pChn->mModId == MOD_ID_VDEC)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("vdec to the end of file");
            if (pContext->mConfigPara.mVoChn >= 0)
            {
                AW_MPI_VO_SetStreamEof(pContext->mConfigPara.mVoLayer, pContext->mConfigPara.mVoChn, 1);
            }
            //sem_post(&pContext->mSemExit);
            break;

        default:
            break;
        }
    }
    else if (pChn->mModId == MOD_ID_VOU)
    {
        switch (event)
        {
        case MPP_EVENT_NOTIFY_EOF:
            alogd("vo to the end of file");
            sem_post(&pContext->mSemExit);
            break;

        case MPP_EVENT_RENDERING_START:
            alogd("vo start to rendering");
            break;

        default:
            break;
        }
    }

    return SUCCESS;
}

static int aw_vdec_init()
{
    int ret = 0;
    DemoCodecParallelContext *pContext = gpDemoCodecParallelContext;

    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    // prepare vdec
    VDEC_CHN mVDecChn = pContext->mConfigPara.mVdecChn;
    VDEC_CHN_ATTR_S mVDecAttr;
    memset(&mVDecAttr, 0, sizeof(VDEC_CHN_ATTR_S));
    mVDecAttr.mType      = pContext->mConfigPara.mVideoDecoderType;
    mVDecAttr.mOutputPixelFormat = pContext->mConfigPara.mVdecOutputPixelFormat;
    mVDecAttr.mPicWidth  = pContext->mConfigPara.mMaxVdecOutputWidth;
    mVDecAttr.mPicHeight = pContext->mConfigPara.mMaxVdecOutputHeight;
    mVDecAttr.mBufSize = pContext->mConfigPara.mVdecBufSize;
    alogd("vdec dev %d, set type: %d, pixel: %d, %dx%d, bufsize: %d", mVDecChn, mVDecAttr.mType,
        mVDecAttr.mOutputPixelFormat, mVDecAttr.mPicWidth, mVDecAttr.mPicHeight, mVDecAttr.mBufSize);
    ret = AW_MPI_VDEC_CreateChn(mVDecChn, &mVDecAttr);
    if(SUCCESS != ret)
    {
        aloge("fatal error! create VDec channel fail!");
    }

    // prepare vo
    int mUILayer = pContext->mConfigPara.mUILayer;
    VO_DEV mVoDev = pContext->mConfigPara.mVoDev;
    VO_LAYER mVoLayer = pContext->mConfigPara.mVoLayer;
    VO_CHN mVoChn = pContext->mConfigPara.mVoChn;
    VO_VIDEO_LAYER_ATTR_S mVoLayerAttr;
    VO_CHN_ATTR_S mVoChnAttr;

    AW_MPI_VO_Enable(mVoDev);
    AW_MPI_VO_AddOutsideVideoLayer(mUILayer);
    AW_MPI_VO_CloseVideoLayer(mUILayer);//close ui layer.
    VO_PUB_ATTR_S spPubAttr;
    AW_MPI_VO_GetPubAttr(mVoDev, &spPubAttr);
    spPubAttr.enIntfType = VO_INTF_LCD;
    spPubAttr.enIntfSync = VO_OUTPUT_NTSC;
    AW_MPI_VO_SetPubAttr(mVoDev, &spPubAttr);

   //enable vo layer
    int hlay0 = 0;
    while (hlay0 < VO_MAX_LAYER_NUM)
    {
        if (SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
        {
            alogd("vo layer %d enable success", hlay0);
            break;
        }
        hlay0++;
    }

    if (hlay0 >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
        mVoLayer = MM_INVALID_DEV;
        AW_MPI_VO_RemoveOutsideVideoLayer(mUILayer);
        AW_MPI_VO_Disable(mVoDev);
        return FAILURE;
    }

    mVoLayer = hlay0;
    pContext->mConfigPara.mVoLayer = mVoLayer;

    AW_MPI_VO_GetVideoLayerAttr(mVoLayer, &mVoLayerAttr);

    mVoLayerAttr.stDispRect.X = pContext->mConfigPara.mDisplayX;
    mVoLayerAttr.stDispRect.Y = pContext->mConfigPara.mDisplayY;
    mVoLayerAttr.stDispRect.Width = pContext->mConfigPara.mDisplayWidth;
    mVoLayerAttr.stDispRect.Height = pContext->mConfigPara.mDisplayHeight;
    mVoLayerAttr.enPixFormat = pContext->mConfigPara.mVdecOutputPixelFormat;
    alogd("vo layer %d, set x:%d y:%d w:%d h:%d", mVoLayer, mVoLayerAttr.stDispRect.X,
        mVoLayerAttr.stDispRect.Y, mVoLayerAttr.stDispRect.Width, mVoLayerAttr.stDispRect.Height);
    AW_MPI_VO_SetVideoLayerAttr(mVoLayer, &mVoLayerAttr);

    ret = AW_MPI_VO_CreateChn(mVoLayer, mVoChn);
    if (SUCCESS != ret)
    {
        aloge("fatal error! create vo channel fail!");
    }

    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VO_RegisterCallback(mVoLayer, mVoChn, &cbInfo);

    AW_MPI_VO_SetChnDispBufNum(mVoLayer, mVoChn, 2);

    // bind vdec vo
    MPP_CHN_S VdecChn = {MOD_ID_VDEC, 0, mVDecChn};
    MPP_CHN_S VoChn = {MOD_ID_VOU, mVoLayer, mVoChn};
    AW_MPI_SYS_Bind(&VdecChn, &VoChn);

    //start
    AW_MPI_VDEC_StartRecvStream(mVDecChn);
    AW_MPI_VO_StartChn(mVoLayer, mVoChn);

    return 0;
}


static int aw_vdec_exit()
{
    DemoCodecParallelContext *pContext = gpDemoCodecParallelContext;

    VDEC_CHN mVDecChn = pContext->mConfigPara.mVdecChn;
    int mUILayer = pContext->mConfigPara.mUILayer;
    VO_DEV mVoDev = pContext->mConfigPara.mVoDev;
    VO_LAYER mVoLayer = pContext->mConfigPara.mVoLayer;
    VO_CHN mVoChn = pContext->mConfigPara.mVoChn;

    AW_MPI_VO_StopChn(mVoLayer, mVoChn);
    AW_MPI_VDEC_StopRecvStream(mVDecChn);

    AW_MPI_VO_DestroyChn(mVoLayer, mVoChn);
    AW_MPI_VO_DisableVideoLayer(mVoLayer);
    AW_MPI_VO_RemoveOutsideVideoLayer(mUILayer);
    AW_MPI_VO_Disable(mVoDev);

    AW_MPI_VDEC_DestroyChn(mVDecChn);

    AW_MPI_SYS_Exit();

    return 0;
}

static int aw_set_display_video_stream(char *buf, int length)
{
    int ret = 0;
    DemoCodecParallelContext *pContext = gpDemoCodecParallelContext;
    VDEC_CHN mVDecChn = pContext->mConfigPara.mVdecChn;

    /*ret = AW_MPI_VDEC_ReopenVideoEngine(mVDecChn);
    if (ret != SUCCESS)
    {
        aloge("reopen ve failed?!");
    }*/

    VDEC_STREAM_S nStreamInfo;
    memset(&nStreamInfo, 0, sizeof(nStreamInfo));
    nStreamInfo.pAddr = (unsigned char*)buf;
    nStreamInfo.mLen = length;
    nStreamInfo.mbEndOfFrame = 1;
    nStreamInfo.mbEndOfStream = 0;
    ret = AW_MPI_VDEC_SendStream(mVDecChn, &nStreamInfo, 100);
    if (ret != SUCCESS)
    {
        alogw("send stream with 100ms timeout fail?! Maybe Vbs is full!");
        return -1;
    }

    return 0;
}

static void* picture_decoder(DemoCodecParallelContext *pContext)
{
    char *jpeg_buf = NULL;
    char SrcPath[64] = {0};
    sprintf(SrcPath, "%s", pContext->mConfigPara.mSrcVideoFile);
    FILE *fp = fopen(SrcPath, "rb");
    if(!fp)
    {
        alogw("can't open jpeg file[%s], exit loop!", SrcPath);
        goto exit;
    }
    fseek(fp, 0, SEEK_END);
    int jpeg_len = ftell(fp);
    rewind(fp);
    jpeg_buf = malloc(jpeg_len);
    if(!jpeg_buf)
    {
        aloge("malloc failed?!");
        goto exit;
    }
    memset(jpeg_buf, 0, jpeg_len);
    fread(jpeg_buf, 1, jpeg_len, fp);
    alogd("read file(len:%d,path:%s) and ready sendframe", jpeg_len, SrcPath);

    int interval_ms = 0;
    if (0 == pContext->mConfigPara.mPlayFrameRate)
    {
        interval_ms = 50;
        alogw("PlayFrameRate = 0, use default interval %d ms", interval_ms);
    }
    else
    {
        interval_ms = 1000 / pContext->mConfigPara.mPlayFrameRate;
    }

    while(0 == pContext->mVdecThreadExitFlag)
    {
        while(1)
        {
            VDEC_STREAM_S nStreamInfo;
            memset(&nStreamInfo, 0, sizeof(nStreamInfo));
            nStreamInfo.pAddr = (unsigned char*)jpeg_buf;
            nStreamInfo.mLen = jpeg_len;
            nStreamInfo.mbEndOfFrame = 1;
            nStreamInfo.mbEndOfStream = 0;
            int ret = AW_MPI_VDEC_SendStream(pContext->mConfigPara.mVdecChn, &nStreamInfo, 100);
            if (ret != SUCCESS)
            {
                alogw("send stream with 100ms timeout fail?! Maybe Vbs is full!");
            }
            else
            {
                alogv("set is ok, %p %d", nStreamInfo.pAddr, nStreamInfo.mLen);
                break;
            }
        }

        usleep(interval_ms * 1000);
    }

exit:
    if (jpeg_buf)
    {
        free(jpeg_buf);
        jpeg_buf = NULL;
    }
    if (fp)
    {
        fclose(fp);
        fp = NULL;
    }

    return NULL;
}

static void* stream_decoder(DemoCodecParallelContext *pContext)
{
    char *pLenStr = NULL;
    FILE *fp_bs = fopen(pContext->mConfigPara.mSrcVideoFile, "rb");
    FILE *fp_sz = fopen(pContext->mConfigPara.mSrcVideoLenFile, "rb");
    if (NULL == fp_bs || NULL == fp_sz)
    {
        aloge("fopen fail! BitStreamFile: %p, %s, LenFile: %p, %s",
            fp_bs, pContext->mConfigPara.mSrcVideoFile, fp_sz, pContext->mConfigPara.mSrcVideoLenFile);
        goto exit;
    }
    fseek(fp_sz, 0, SEEK_END);
    int nLenFileSize = ftell(fp_sz);
    fseek(fp_sz, 0, SEEK_SET);
    pLenStr = malloc(nLenFileSize);
    if (NULL == pLenStr)
    {
        aloge("fatal error! malloc pLenStr fail! size=%d", nLenFileSize);
        goto exit;
    }
    memset(pLenStr, 0, nLenFileSize);
    fread(pLenStr, 1, nLenFileSize, fp_sz);
    char *endptr0 = pLenStr;
    char *endptr1 = pLenStr + nLenFileSize;

    VDEC_STREAM_S nStreamInfo;
    memset(&nStreamInfo, 0, sizeof(nStreamInfo));

    int stream_buf_len = 500*1024;
    nStreamInfo.pAddr = malloc(stream_buf_len);
    if (NULL == nStreamInfo.pAddr)
    {
        aloge("fatal error, malloc failed! size=%d", stream_buf_len);
        goto exit;
    }
    memset(nStreamInfo.pAddr, 0, stream_buf_len);
    alogd("malloc stream buf %p, size %d", nStreamInfo.pAddr, stream_buf_len);

    int interval_ms = 0;
    if (0 == pContext->mConfigPara.mPlayFrameRate)
    {
        interval_ms = 50;
        alogw("PlayFrameRate = 0, use default interval %d ms", interval_ms);
    }
    else
    {
        interval_ms = 1000 / pContext->mConfigPara.mPlayFrameRate;
    }

    while(0 == pContext->mVdecThreadExitFlag)
    {
        int pkt_sz = strtol(endptr0, &endptr1, 10);
        endptr0 += 8;//endptr1;
        if (0 == pkt_sz)
        {
            alogw("pkt_sz=0, exit loop!");
            break;
        }

        if ((stream_buf_len) && (stream_buf_len < pkt_sz))
        {
            if (nStreamInfo.pAddr)
            {
                alogd("free stream buf %p", nStreamInfo.pAddr);
                free(nStreamInfo.pAddr);
                nStreamInfo.pAddr = NULL;
            }
            stream_buf_len = pkt_sz;
        }
        if (NULL == nStreamInfo.pAddr)
        {
            nStreamInfo.pAddr = malloc(pkt_sz);
            if (NULL == nStreamInfo.pAddr)
            {
                aloge("fatal error, malloc failed! size=%d", pkt_sz);
                break;
            }
            memset(nStreamInfo.pAddr, 0, pkt_sz);
            alogd("malloc stream buf %p, size %d", nStreamInfo.pAddr, pkt_sz);
        }

        int rd_cnt = fread(nStreamInfo.pAddr, 1, pkt_sz, fp_bs);
        if (rd_cnt != pkt_sz)
        {
            aloge("error happen! pkt_sz=%d, rd_cnt=%d", pkt_sz, rd_cnt);
            break;
        }
        else
        {
            nStreamInfo.mLen = rd_cnt;
            alogv("read vbs packet! rd_cnt=%d", rd_cnt);
        }

        while(1)
        {
            nStreamInfo.mbEndOfFrame = 1;
            nStreamInfo.mbEndOfStream = 0;
            int ret = AW_MPI_VDEC_SendStream(pContext->mConfigPara.mVdecChn, &nStreamInfo, 100);
            if (ret != SUCCESS)
            {
                alogw("send stream with 100ms timeout fail?! Maybe Vbs is full!");
            }
            else
            {
                alogv("set is ok, %p %d", nStreamInfo.pAddr, nStreamInfo.mLen);
                break;
            }
        }

        usleep(interval_ms * 1000);
    }

exit:
    if (pLenStr)
    {
        free(pLenStr);
        pLenStr = NULL;
    }
    if (fp_bs)
    {
        fclose(fp_bs);
        fp_bs = NULL;
    }
    if (fp_sz)
    {
        fclose(fp_sz);
        fp_sz = NULL;
    }
    if (nStreamInfo.pAddr)
    {
        free(nStreamInfo.pAddr);
        nStreamInfo.pAddr = NULL;
    }

    return NULL;
}

static void* video_decoder_thread(void* param)
{
    DemoCodecParallelContext *pContext = (DemoCodecParallelContext *)param;

    aw_vdec_init();

    if (PT_JPEG == pContext->mConfigPara.mVideoDecoderType)
    {
        picture_decoder(pContext);
    }
    else if (PT_H264 == pContext->mConfigPara.mVideoDecoderType || PT_MJPEG == pContext->mConfigPara.mVideoDecoderType)
    {
        stream_decoder(pContext);
    }
    else
    {
        aloge("fatal error, video decoder type %d is unsupport!", pContext->mConfigPara.mVideoDecoderType);
    }

    aw_vdec_exit();

    return NULL;
}

static void configureRecord(DemoCodecParallelContext *pContext, VideoInputConfig *pconfig)
{
    int channel_id = pContext->mConfigPara.mVippID;

    pContext->mStatis.stream_count_0 = 0;
    pContext->mStatis.video_finish_flag = 0;
    pContext->mStatis.max_bitstream_count = pContext->mConfigPara.mRecordDuration * pContext->mConfigPara.mVideoFrameRate;
    alogd("ch%d max_bitstream_count=%d", channel_id, pContext->mStatis.max_bitstream_count);
    if(pContext->mStatis.max_bitstream_count <= 0)
    {
        pContext->mStatis.max_bitstream_count = 100;
    }

    pconfig->channelId = channel_id;
    pconfig->fps       = pContext->mConfigPara.mVippFrameRate;
    pconfig->gop       = 100;
    pconfig->mRcMode       = AW_VBR;
    pconfig->vbr_opt_enable = 1;
    pconfig->qp_range.nMinqp = 25;
    pconfig->qp_range.nMaxqp = 45;
    pconfig->qp_range.nMinPqp = 25;
    pconfig->qp_range.nMaxPqp = 45;
    pconfig->qp_range.nQpInit = 37;
    pconfig->pixelformat = pContext->mConfigPara.mVippPixelFormat;
    pconfig->enable_sharp = 1;
    pconfig->bonline_channel = pContext->mConfigPara.mOnlineEnable;
    pconfig->share_buf_num = pContext->mConfigPara.mOnlineShareBufNum;
    pconfig->vin_buf_num = pContext->mConfigPara.mVippBufNum;
    pconfig->breduce_refrecmem = 1;
    pconfig->venc_video_signal.video_format = DEFAULT;
    switch(pContext->mConfigPara.mVippColorSpace) {
    case V4L2_COLORSPACE_JPEG:
    {
        pconfig->venc_video_signal.full_range_flag = 1;
        pconfig->venc_video_signal.src_colour_primaries = VENC_YCC;
        pconfig->venc_video_signal.dst_colour_primaries = VENC_YCC;
        break;
    }
    case V4L2_COLORSPACE_REC709:
    {
        pconfig->venc_video_signal.full_range_flag = 1;
        pconfig->venc_video_signal.src_colour_primaries = VENC_BT709;
        pconfig->venc_video_signal.dst_colour_primaries = VENC_BT709;
        break;
    }
    case V4L2_COLORSPACE_REC709_PART_RANGE:
    {
        pconfig->venc_video_signal.full_range_flag = 0;
        pconfig->venc_video_signal.src_colour_primaries = VENC_BT709;
        pconfig->venc_video_signal.dst_colour_primaries = VENC_BT709;
        break;
    }
    default:
    {
        pconfig->venc_video_signal.full_range_flag = 1;
        pconfig->venc_video_signal.src_colour_primaries = VENC_BT709;
        pconfig->venc_video_signal.dst_colour_primaries = VENC_BT709;
        break;
    }
    }
    pconfig->output_mode = OUTPUT_MODE_STREAM;
    pconfig->width      = pContext->mConfigPara.mVippWidth;
    pconfig->height     = pContext->mConfigPara.mVippHeight;
    pconfig->dst_width      = pContext->mConfigPara.mVideoWidth;
    pconfig->dst_height     = pContext->mConfigPara.mVideoHeight;
    pconfig->bitrate    = pContext->mConfigPara.mVideoBitrate;
    pconfig->encodeType = pContext->mConfigPara.mVideoEncoderType;
    pconfig->drop_frame_num = 0;
    pconfig->enable_wdr = 0;
    if(pconfig->encodeType == 0) //h264
    {
        pconfig->profile = VENC_H264ProfileMain;
        pconfig->level   = VENC_H264Level51;
    }
    else if (pconfig->encodeType == 1) //mjpeg
    {
        aloge("fatal error! not support mjpeg! check code!");
    }
    else if(pconfig->encodeType == 2) //h265
    {
        pconfig->profile = VENC_H265ProfileMain;
        pconfig->level   = VENC_H265LevelDefault;
    }
	return;
}

static void configureSubRecord(DemoCodecParallelContext *pContext, VideoInputConfig *pconfig)
{
    int channel_id = pContext->mConfigPara.mSubVippID;

    pContext->mSubStatis.stream_count_0 = 0;
    pContext->mSubStatis.video_finish_flag = 0;
    pContext->mSubStatis.max_bitstream_count = pContext->mConfigPara.mSubRecordDuration * pContext->mConfigPara.mSubVideoFrameRate;
    alogd("ch%d max_bitstream_count=%d", channel_id, pContext->mSubStatis.max_bitstream_count);
    if(pContext->mSubStatis.max_bitstream_count <= 0)
    {
        pContext->mSubStatis.max_bitstream_count = 100;
    }

    pconfig->channelId = channel_id;
    pconfig->fps       = pContext->mConfigPara.mSubVippFrameRate;
    pconfig->gop       = 100;
    pconfig->mRcMode   = AW_VBR;
    pconfig->vbr_opt_enable = 1;
    pconfig->qp_range.nMinqp = 25;
    pconfig->qp_range.nMaxqp = 45;
    pconfig->qp_range.nMinPqp = 25;
    pconfig->qp_range.nMaxPqp = 45;
    pconfig->qp_range.nQpInit = 37;
    pconfig->pixelformat = pContext->mConfigPara.mSubVippPixelFormat;
    pconfig->enable_sharp = 1;
    pconfig->bonline_channel = 0;
    pconfig->share_buf_num = 0;
    pconfig->vin_buf_num = pContext->mConfigPara.mSubVippBufNum;
    pconfig->breduce_refrecmem = 1;
    pconfig->venc_video_signal.video_format = DEFAULT;
    switch(pContext->mConfigPara.mSubVippColorSpace) {
    case V4L2_COLORSPACE_JPEG:
    {
        pconfig->venc_video_signal.full_range_flag = 1;
        pconfig->venc_video_signal.src_colour_primaries = VENC_YCC;
        pconfig->venc_video_signal.dst_colour_primaries = VENC_YCC;
        break;
    }
    case V4L2_COLORSPACE_REC709:
    {
        pconfig->venc_video_signal.full_range_flag = 1;
        pconfig->venc_video_signal.src_colour_primaries = VENC_BT709;
        pconfig->venc_video_signal.dst_colour_primaries = VENC_BT709;
        break;
    }
    case V4L2_COLORSPACE_REC709_PART_RANGE:
    {
        pconfig->venc_video_signal.full_range_flag = 0;
        pconfig->venc_video_signal.src_colour_primaries = VENC_BT709;
        pconfig->venc_video_signal.dst_colour_primaries = VENC_BT709;
        break;
    }
    default:
    {
        pconfig->venc_video_signal.full_range_flag = 1;
        pconfig->venc_video_signal.src_colour_primaries = VENC_BT709;
        pconfig->venc_video_signal.dst_colour_primaries = VENC_BT709;
        break;
    }
    }
    pconfig->output_mode = OUTPUT_MODE_STREAM;
    pconfig->width      = pContext->mConfigPara.mSubVippWidth;
    pconfig->height     = pContext->mConfigPara.mSubVippHeight;
    pconfig->dst_width      = pContext->mConfigPara.mSubVideoWidth;
    pconfig->dst_height     = pContext->mConfigPara.mSubVideoHeight;
    pconfig->bitrate    = pContext->mConfigPara.mSubVideoBitrate;
    pconfig->encodeType = pContext->mConfigPara.mSubVideoEncoderType;
    pconfig->drop_frame_num = 0;
    pconfig->enable_wdr = 0;
    if(pconfig->encodeType == 0) //h264
    {
        pconfig->profile = VENC_H264ProfileMain;
        pconfig->level   = VENC_H264Level51;
    }
    else if (pconfig->encodeType == 1) //mjpeg
    {
        aloge("fatal error! not support mjpeg! check code!");
    }
    else if(pconfig->encodeType == 2) //h265
    {
        pconfig->profile = VENC_H265ProfileMain;
        pconfig->level   = VENC_H265LevelDefault;
    }
	return;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");

    if(NULL != gpDemoCodecParallelContext)
    {
        sem_post(&gpDemoCodecParallelContext->mSemExit);
    }
}

int main(int argc, char** argv)
{
    int result = 0;
    ERRORTYPE ret;
    GLogConfig stGLogConfig =
    {
        .FLAGS_logtostderr = 0,
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

    alogd("demo_codec_parallel begin");
    DemoCodecParallelContext *pContext = calloc(1, sizeof(DemoCodecParallelContext));
    if(NULL == pContext)
    {
        aloge("fatal error! malloc fail");
    }
    gpDemoCodecParallelContext = pContext;

    sem_init(&pContext->mSemExit, 0, 0);

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    /* parse command line param */
    char *pConfigFilePath = NULL;
    if (parseCmdLine(pContext, argc, argv) != SUCCESS)
    {
        aloge("fatal error! parse cmd line fail");
        result = -1;
        goto err_out_0;
    }
    pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;

    /* parse config file */
    if (loadConfigPara(pContext, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto err_out_0;
    }

    // rt-media
    AWVideoInput_Init();
#if SUPPORT_VENC
    VideoInputConfig config_0;
    if (pContext->mConfigPara.mRecordEnable)
    {
        memset(&config_0, 0, sizeof(VideoInputConfig));
        configureRecord(pContext, &config_0);
        if (AWVideoInput_Configure(config_0.channelId, &config_0))
        {
            aloge("fatal error, config err, exit!");
        }
        AWVideoInput_CallBack(config_0.channelId, video_stream_cb, 1);
        AWVideoInput_SetEncAndDecCase(config_0.channelId, pContext->mConfigPara.mPlayEnable);
        AWVideoInput_Start(config_0.channelId, 1);
    }
#endif
#if SUPPORT_VENC_SUB
    VideoInputConfig config_1;
    if (pContext->mConfigPara.mSubRecordEnable)
    {
        memset(&config_1, 0, sizeof(VideoInputConfig));
        configureSubRecord(pContext, &config_1);
        if (AWVideoInput_Configure(config_1.channelId, &config_1))
        {
            aloge("fatal error, config err, exit!");
        }
        AWVideoInput_CallBack(config_1.channelId, video_stream_cb, 1);
        AWVideoInput_SetEncAndDecCase(config_1.channelId, pContext->mConfigPara.mPlayEnable);
        AWVideoInput_Start(config_1.channelId, 1);
    }
#endif

#if SUPPORT_VDEC
    if (pContext->mConfigPara.mPlayEnable)
    {
        pContext->mVdecThreadExitFlag = 0;
        result = pthread_create(&pContext->mVdecThreadId, NULL, video_decoder_thread, (void*)pContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }
#endif

    if (pContext->mConfigPara.mTestDuration > 0)
    {
        alogd("The test time is %d s, continues ...", pContext->mConfigPara.mTestDuration);
        int64_t time_ms = pContext->mConfigPara.mTestDuration*1000;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += time_ms % 1000 * 1000 * 1000;
        ts.tv_sec += time_ms / 1000 + ts.tv_nsec / (1000 * 1000 * 1000);
        ts.tv_nsec = ts.tv_nsec % (1000*1000*1000);
        sem_timedwait(&pContext->mSemExit, &ts);
        alogd("The test time is up, end the test.");
    }
    else
    {
        alogd("No test time is specified, you need to pass 'ctrl+c' to exit the test.");
        sem_wait(&pContext->mSemExit);
    }

#if SUPPORT_VDEC
    if (pContext->mConfigPara.mPlayEnable)
    {
        pContext->mVdecThreadExitFlag = 1;
        alogd("set VdecThreadExitFlag = 1, wait thread exit");
        pthread_join(pContext->mVdecThreadId, NULL);
    }
#endif

#if SUPPORT_VENC
    if (pContext->mConfigPara.mRecordEnable)
    {
        alogd("stop chn%d", config_0.channelId);
        AWVideoInput_Destroy(config_0.channelId);
        if (pContext->mOutFile)
        {
            fclose(pContext->mOutFile);
            pContext->mOutFile = NULL;
        }
    }
#endif
#if SUPPORT_VENC_SUB
    if (pContext->mConfigPara.mSubRecordEnable)
    {
        alogd("stop chn%d", config_1.channelId);
        AWVideoInput_Destroy(config_1.channelId);
        if (pContext->mSubOutFile)
        {
            fclose(pContext->mSubOutFile);
            pContext->mSubOutFile = NULL;
        }
    }
#endif
    AWVideoInput_DeInit();

err_out_0:
    sem_destroy(&pContext->mSemExit);
    if (pContext)
    {
        free(pContext);
        pContext = NULL;
        gpDemoCodecParallelContext = NULL;
    }
    alogd("demo_codec_parallel finish!");
    log_quit();
    return result;
}

