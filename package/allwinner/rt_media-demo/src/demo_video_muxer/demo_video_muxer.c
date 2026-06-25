/** @file
  演示从rtmedia获取视频编码流，从MPP获取音频编码流，使用mpi_muxer组件封装为mp4文件的功能.

配置：勾选对应配置后，编译时会将其拷贝到/bin目录下。

```
make menuconfig
Allwinner-> rt_media demo selection -> demo_video_muxer
```

参数说明：

```
-n,--encode_frame_num   编码帧数，未考虑pts情况下，250帧大约=10s，eg: -n 250。 param->encoder_num
-f0,--encode_format0    channel0编码格式，0:h264 encoder, 1:jpeg_encoder, 2:h265 encoder，eg: -f0 0。 param->c0_encoder_format
-s0,--srcsize           channel0编码分辨率 1280x720 1920x1080 2560x1440 etc, eg: -s0 2560x1440。 param->c0_src_size, param->c0_src_w, param->c0_src_h
-ds0   channel0         channel0输出分辨率，如果未设置，即等于输入分辨率 1280x720 1920x1080 2560x1440 etc，eg: -ds0 2560x1440。 param->c0_dst_size, param->c0_dst_w, param->c0_dst_h
-vb,--vinbuf            vin buf num.
-vn,--vippnum           channel0使用的vipp编号。e.g.: -vn 0。 param->use_vipp_num
-pf,--pxlformat         channel0的vipp的pixel format。1:RT_PIXEL_YVU420SP, 12: RT_PIXEL_LBC_25X, e.g.:-pf 12。 param->pixelformat
-sp,--sharp             channel0是否开启编码锐化。0:不开启，1:开启。e.g.: -sp 0。 param->enable_sharp
-online,--online_mode   channel0是否开启在线编码。 e.g.: -online 0。 param->bonline_channel
-sbn,--share_buf        channel0在线编码的buf数量。e.g.: -sbn 2。param->share_buf_num
-vbr,--vbr              channnel0是否使用vbr。1:vbr, 0:cbr
-vbr_opt_en,--vbr_opt_en    channel0是否开启新vbr。1:开启, 0:关闭，使用旧vbr。
-b0,--bitrate           channel0的码率。单位bps。 e.g.: -b0 1500000。 param->c0_bitraten
-cs,--colorspace        isp的颜色空间。取值为7:V4L2_COLORSPACE_JPEG, 3:V4L2_COLORSPACE_REC709, 31:V4L2_COLORSPACE_REC709_PART_RANGE
-out,--outputfile       输出文件的路径的前缀。e.g.: /mnt/extsd/stream0_encoder。param->OutputFilePath
```

测试命令示例：

```
./demo_video_muxer -n 150 -s0 1920x1080 -f0 0 -vn 0 -pf 12 -sp 1 -vbr 1 -vbr_opt_en 1 -b0 1500000 -cs 3 -out /mnt/extsd/demo_video_muxer.mp4
./demo_video_muxer -n 150 -s0 1920x1080 -f0 1 -vn 0 -pf 12 -vb 4 -sp 1 -b0 3145728 -cs 3 -out /mnt/extsd/demo_video_muxer_mjpeg.mp4

./demo_video_muxer -n 150 -s0 1280x720 -f0 0 -vn 0 -pf 1 -vb 4 -sp 1 -vbr 1 -vbr_opt_en 1 -b0 1500000 -cs 3 -out /mnt/extsd/demo_video_muxer_offline_nv21.mp4
./demo_video_muxer -n 150 -s0 1280x720 -f0 0 -vn 0 -pf 12 -vb 4 -sp 1 -vbr 1 -vbr_opt_en 1 -b0 1500000 -cs 3 -out /mnt/extsd/demo_video_muxer_offline_lbc.mp4
./demo_video_muxer -n 150 -s0 1280x720 -f0 0 -vn 0 -pf 1 -sp 1 -vbr 1 -vbr_opt_en 1 -b0 1500000 -cs 3 -online 1 -sbn 2 -out /mnt/extsd/demo_video_muxer_online2buf_nv21.mp4
./demo_video_muxer -n 150 -s0 1280x720 -f0 0 -vn 0 -pf 12 -sp 1 -vbr 1 -vbr_opt_en 1 -b0 1500000 -cs 3 -online 1 -sbn 2 -out /mnt/extsd/demo_video_muxer_online2buf_lbc.mp4
```

生成文件：/mnt/extsd/demo_video_muxer.mp4

*/

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

#include "demo_video_muxer.h"

#include <linux/videodev2.h>
#include <media/sunxi_camera_v2.h>

#include <plat_log.h>
#include <aenc_sw_lib.h>
#include <mm_comm_venc.h>
#include <media_common_vcodec.h>
#include <mpi_sys.h>
#include <mpi_mux.h>

#define OUT_PUT_FILE_PREFIX "/mnt/extsd/demo_video_muxer.mp4"
//#define TEST_SEI
//#define TEST_GETYUV
//#define TEST_CATCHJPEG
//#define TEST_SENSOR_A_ROTATE

DemoVideoMuxerContext *gpDemoVideoMuxerContext = NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpDemoVideoMuxerContext)
    {
        sem_post(&gpDemoVideoMuxerContext->finish_sem);
    }
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    DemoVideoMuxerContext *pCtx = (DemoVideoMuxerContext*)cookie;
    ERRORTYPE ret;
    if(MOD_ID_AI == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_CAPTURE_AUDIO_DATA:
            {
                AISendDataInfo * pUserData = (AISendDataInfo *)pEventData;
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else if(MOD_ID_AENC == pChn->mModId)
    {
        alogw("aencChn[%d] not support notify recorder by AEnc with event(%d)", pChn->mChnId, event);
    }
    else if(MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                int nMuxerId = *(int*)pEventData;
                alogd("muxChn[%d-%d] record file done.", pCtx->mMuxChn, nMuxerId);
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                int nMuxerId = *(int*)pEventData;
                alogd("muxChn[%d-%d] need next fd.", pCtx->mMuxChn, nMuxerId);
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
                break;
            }
            case MPP_EVENT_WRITE_DISK_ERROR:
            {
                int muxerId = *(int*)pEventData;
                alogd("MuxerId[%d] write disk error!", muxerId);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x]", event);
                break;
            }
        }
    }
    else
    {
        alogw("unknown mpp module:0x%x", pChn->mModId);
    }
    
    return SUCCESS;
}

static MEDIA_FILE_FORMAT_E checkFileFormat(const char *pFilePath)
{
    MEDIA_FILE_FORMAT_E eFileFormat = MEDIA_FILE_FORMAT_MP4;
    char *ptr = strrchr(pFilePath, '.');
    if(NULL == ptr)
    {
        alogw("Be careful! not find file suffix, so use mp4 as default");
        return eFileFormat;
    }
    ptr += 1;
    if (!strcmp(ptr, "mp4"))
    {
        eFileFormat = MEDIA_FILE_FORMAT_MP4;
    }
    else if (!strcmp(ptr, "ts"))
    {
        eFileFormat = MEDIA_FILE_FORMAT_TS;
    }
    else
    {
        alogw("Unknown file format[%s]! use mp4 as default", ptr);
        eFileFormat = MEDIA_FILE_FORMAT_MP4;
    }
    return eFileFormat;
}

static PAYLOAD_TYPE_E map_RT_VENC_CODEC_TYPE_to_PAYLOAD_TYPE_E(RT_VENC_CODEC_TYPE eRtVencType)
{
    PAYLOAD_TYPE_E ePayloadType;
    switch(eRtVencType)
    {
    case RT_VENC_CODEC_H264:
    {
        ePayloadType = PT_H264;
        break;
    }
    case RT_VENC_CODEC_H265:
    {
        ePayloadType = PT_H265;
        break;
    }
    case RT_VENC_CODEC_JPEG:
    {
        ePayloadType = PT_MJPEG;
        break;
    }
    default:
    {
        aloge("fatal error! unknown RT venc type:%d", eRtVencType);
        ePayloadType = PT_H264;
        break;
    }
    }
    return ePayloadType;
}

static int CreateMuxChn(DemoVideoMuxerContext *pCtx)
{
    ERRORTYPE ret;
    const char *pFilePath = pCtx->mparam.OutputFilePath;
    int nMediaFd = open(pFilePath, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (nMediaFd < 0)
    {
        aloge("fatal error! failed to open %s", pFilePath);
    }
    pCtx->mMuxChn = 0;
    memset(&pCtx->mMuxChnAttr, 0, sizeof(MUX_CHN_ATTR_S));
    pCtx->mMuxChnAttr.mVideoAttrValidNum = 1;
    pCtx->mMuxChnAttr.mVideoAttr[0].mWidth = pCtx->config_0.dst_width;
    pCtx->mMuxChnAttr.mVideoAttr[0].mHeight = pCtx->config_0.dst_height;
    pCtx->mMuxChnAttr.mVideoAttr[0].mVideoFrmRate = pCtx->config_0.fps*1000;
    pCtx->mMuxChnAttr.mVideoAttr[0].mMaxKeyInterval = pCtx->config_0.gop;
    switch(pCtx->config_0.encodeType)
    {
        case 0:
            pCtx->mMuxChnAttr.mVideoAttr[0].mVideoEncodeType = PT_H264;
            break;
        case 2:
            pCtx->mMuxChnAttr.mVideoAttr[0].mVideoEncodeType = PT_H265;
            break;
        case 1:
            pCtx->mMuxChnAttr.mVideoAttr[0].mVideoEncodeType = PT_MJPEG;
            break;
        default:
            aloge("fatal error! use h264");
            pCtx->mMuxChnAttr.mVideoAttr[0].mVideoEncodeType = PT_H264;
            break;
    }
    pCtx->mMuxChnAttr.mVideoAttr[0].mRotateDegree = 0;
    //we can do this, muxer use veChn to link spspps and streamId, muxer don't need to operate veChn.
    //so we can use any number as veChn.
    pCtx->mMuxChnAttr.mVideoAttr[0].mVeChn = pCtx->config_0.channelId;
    pCtx->mMuxChnAttr.mAudioEncodeType = PT_MAX;
    pCtx->mMuxChnAttr.mTextEncodeType = PT_MAX;

    //pCtx->mMuxChnAttr.mMuxerId = 0;
    pCtx->mMuxChnAttr.mMediaFileFormat = checkFileFormat(pFilePath);
    pCtx->mMuxChnAttr.mMaxFileDuration = 0;
    pCtx->mMuxChnAttr.mCallbackOutFlag = FALSE;
    pCtx->mMuxChnAttr.mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pCtx->mMuxChnAttr.mSimpleCacheSize = 64*1024;
    pCtx->mMuxChnAttr.mAddRepairInfo = 0;
    pCtx->mMuxChnAttr.mMaxFrmsTagInterval = 0;
    ret = AW_MPI_MUX_CreateChn(pCtx->mMuxChn, &pCtx->mMuxChnAttr, nMediaFd, 0);
    if(ret != SUCCESS)
    {
        aloge("fatal error! create muxChn[%d] fail ret[0x%x]!", pCtx->mMuxChn, ret);
    }
    close(nMediaFd);
    nMediaFd = -1;
    pCtx->mPolicy = RecordFileDurationPolicy_MinDuration;
    ret = AW_MPI_MUX_SetSwitchFileDurationPolicy(pCtx->mMuxChn, pCtx->mPolicy);
    if(ret != SUCCESS)
    {
        aloge("fatal error! set file duration policy[%d] to mux[%d] fail!", pCtx->mPolicy, pCtx->mMuxChn);
    }
    ret = AW_MPI_MUX_SetVeChnBindStreamId(pCtx->mMuxChn, pCtx->config_0.channelId, pCtx->mMuxVideoStreamId);
    if(ret != SUCCESS)
    {
        aloge("fatal error! set VeChnBindStreamId to muxChn[%d] fail!", pCtx->mMuxChn);
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pCtx;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_MUX_RegisterCallback(pCtx->mMuxChn, &cbInfo);
    return 0;
}

static int SetSpsppsToMuxer(DemoVideoMuxerContext *pContext)
{
    alogd("try to get spspps from rtmedia and set to muxChn");
    if(0 == pContext->config_0.encodeType)
    {
        sps_pps_data_info stSpsPpsInfo;
        int result = AWVideoInput_GetSpsPpsInfo(pContext->config_0.channelId, &stSpsPpsInfo);
        if(0 == result)
        {
            VencHeaderData stH264SpsPpsInfo;
            memset(&stH264SpsPpsInfo,0,sizeof(stH264SpsPpsInfo));
            stH264SpsPpsInfo.pBuffer = stSpsPpsInfo.buf;
            stH264SpsPpsInfo.nLength = stSpsPpsInfo.size;
            alogd("rtmedia channel[%d]: H264SpsPpsInfo.nLength: %d", pContext->config_0.channelId, stH264SpsPpsInfo.nLength);
            AW_MPI_MUX_SetH264SpsPpsInfo(pContext->mMuxChn, pContext->config_0.channelId, &stH264SpsPpsInfo);
        }
        else
        {
            aloge("fatal error! get spspps from rtmedia channel[%d] fail", pContext->config_0.channelId);
        }
    }
    else if(2 == pContext->config_0.encodeType)
    {
        sps_pps_data_info stSpsPpsInfo;
        int result = AWVideoInput_GetSpsPpsInfo(pContext->config_0.channelId, &stSpsPpsInfo);
        if(0 == result)
        {
            VencHeaderData stH265SpsPpsInfo;
            memset(&stH265SpsPpsInfo,0,sizeof(stH265SpsPpsInfo));
            stH265SpsPpsInfo.pBuffer = stSpsPpsInfo.buf;
            stH265SpsPpsInfo.nLength = stSpsPpsInfo.size;
            alogd("rtmedia channel[%d]: H265SpsPpsInfo.nLength: %d", pContext->config_0.channelId, stH265SpsPpsInfo.nLength);
            AW_MPI_MUX_SetH265SpsPpsInfo(pContext->mMuxChn, pContext->config_0.channelId, &stH265SpsPpsInfo);
        }
        else
        {
            aloge("fatal error! get spspps from rtmedia channel[%d] fail", pContext->config_0.channelId);
        }
    }
    return 0;
}

//static VENC_DATA_TYPE_U map_RT_VENC_DATA_TYPE_U_to_VENC_DATA_TYPE_U(RT_VENC_DATA_TYPE_U URTPackType)
//{
//    VENC_DATA_TYPE_U UVencPackType;
//    UVencPackType.enH264EType = (H264E_NALU_TYPE_E)URTPackType.enH264EType;
//    return UVencPackType;
//}

void video_stream_cb(const AWVideoInput_StreamInfo* stream_info)
{
    ERRORTYPE ret;
    DemoVideoMuxerContext *pContext = gpDemoVideoMuxerContext;
    pthread_mutex_lock(&pContext->mMuxChnLock);
    if(0 == pContext->mbMuxChnSetSpsppsFlag)
    {
        SetSpsppsToMuxer(pContext);
        pContext->mbMuxChnSetSpsppsFlag = 1;
        AW_MPI_MUX_StartChn(pContext->mMuxChn);
    }
    pthread_mutex_unlock(&pContext->mMuxChnLock);

    if(stream_info->data0 != NULL && stream_info->size0 > 0 && pContext->stream_count_0 < pContext->max_bitstream_count)
    {
        uint64_t cur_time = get_cur_time_us();

        uint64_t pts = stream_info->pts; //unit:us
        uint64_t pts_in_seconds = pts/1000/1000;

        if(pContext->stream_count_0%15 == 0)
        {
            alogd("*data-len:%p-%d,%p-%d,%p-%d, cnt = %d, kf = %d, pts = %llu us(%llu s), diff = %llu ms,  cur_time = %llu us, time-pts-diff = %llu ms",
                stream_info->data0, stream_info->size0, stream_info->data1, stream_info->size1, stream_info->data2, stream_info->size2,
                pContext->stream_count_0, stream_info->keyframe_flag,
                pts, pts/1000000, (pts - pContext->pre_pts)/1000, cur_time, (cur_time - pts)/1000);
        }

        if(pts_in_seconds != pContext->pre_pts_in_seconds)
        {
            alogd("get video stream, fps = %d", pContext->cb_stream_cnt_in_seconds);
            pContext->pre_pts_in_seconds = pts_in_seconds;
            pContext->cb_stream_cnt_in_seconds = 0;
        }
        pContext->cb_stream_cnt_in_seconds++;

        pContext->pre_pts = pts;

        VENC_STREAM_S stVencStream;
        VENC_PACK_S stVencPack;
        memset(&stVencStream, 0, sizeof(stVencStream));
        memset(&stVencPack, 0, sizeof(stVencPack));
        stVencStream.mPackCount = 1;
        stVencStream.mpPack = &stVencPack;

        stVencStream.mpPack[0].mpAddr0 = stream_info->data0;
        stVencStream.mpPack[0].mLen0 = stream_info->size0;
        if(stream_info->size1 > 0)
        {
            stVencStream.mpPack[0].mpAddr1 = stream_info->data1;
            stVencStream.mpPack[0].mLen1 = stream_info->size1;
        }
        if(stream_info->size2 > 0)
        {
            stVencStream.mpPack[0].mpAddr2 = stream_info->data2;
            stVencStream.mpPack[0].mLen2 = stream_info->size2;
        }
        stVencStream.mpPack[0].mPTS = pts;
        stVencStream.mpPack[0].mbFrameEnd = TRUE;
        if(0 == pContext->config_0.encodeType)
        {
            stVencStream.mpPack[0].mDataType.enH264EType = (stream_info->keyframe_flag) ? H264E_NALU_ISLICE : H264E_NALU_PSLICE;
        }
        else if(2 == pContext->config_0.encodeType)
        {
            stVencStream.mpPack[0].mDataType.enH265EType = (stream_info->keyframe_flag) ? H265E_NALU_ISLICE : H265E_NALU_PSLICE;
        }
        else if (1 == pContext->config_0.encodeType)
        {
            stVencStream.mpPack[0].mDataType.enJPEGEType = JPEGE_PACK_PIC;
        }
        else
        {
            alogw("fatal error! unsupported encodeType:%d", pContext->config_0.encodeType);
        }
        PAYLOAD_TYPE_E ePayloadType = map_RT_VENC_CODEC_TYPE_to_PAYLOAD_TYPE_E(pContext->config_0.encodeType);
        stVencStream.mpPack[0].mDataNum = stream_info->nDataNum;
        for(int i=0; i<stream_info->nDataNum; i++)
        {
            stVencStream.mpPack[0].mPackInfo[i].mPackType = map_VENC_OUTPUT_PACK_TYPE_to_VENC_DATA_TYPE_U(
                stream_info->stPackInfo[i].uType, ePayloadType);
            stVencStream.mpPack[0].mPackInfo[i].mPackOffset = stream_info->stPackInfo[i].nOffset;
            stVencStream.mpPack[0].mPackInfo[i].mPackLength = stream_info->stPackInfo[i].nLength;
            //alogd("frmcnt[%d]:%d/%d:%d-%d-%d", pContext->stream_count_0, i, stream_info->nDataNum,
            //    stVencStream.mpPack[0].mPackInfo[i].mPackType, stVencStream.mpPack[0].mPackInfo[i].mPackOffset,
            //    stVencStream.mpPack[0].mPackInfo[i].mPackLength);
        }
        stVencStream.mSeq = pContext->stream_count_0;
        ret = AW_MPI_MUX_SendVideoStreamSync(pContext->mMuxChn, &stVencStream, pContext->mMuxVideoStreamId);
        if(ret != SUCCESS)
        {
            aloge("fatal error! send video stream to mux fail[0x%x]", ret);
        }
    }
    else
    {
        alogd("stream data error: data = %p, len = %d, streamCount:%d >= %d", stream_info->data0, stream_info->size0,
            pContext->stream_count_0, pContext->max_bitstream_count);
        return;
    }
    pContext->stream_count_0++;
    if(pContext->stream_count_0 >= pContext->max_bitstream_count && !pContext->video_finish_flag)
    {
        pContext->video_finish_flag = 1;
        sem_post(&pContext->finish_sem);
    }
}

/**
  parse and get cmd line arguments.

  @return
    0: success
    -1: fail
*/
static int LoadParam(DemoVideoMuxerContext *pContext, int argc, char** argv)
{
    int ret = 0;
    int i;
    memset(&pContext->mparam, 0, sizeof(demo_video_param));
    pContext->mparam.c0_encoder_format = 0;
    pContext->mparam.pixelformat = RT_PIXEL_LBC_25X;
    pContext->mparam.use_vipp_num = 0;
    strcpy(pContext->mparam.OutputFilePath, OUT_PUT_FILE_PREFIX);
    /******** begin parse the config paramter ********/
    if(argc >= 2)
    {
        for(i = 1; i < argc; i += 2)
        {
            ParseArgument(&pContext->mparam, argv[i], argv[i + 1]);
        }
        check_param(&pContext->mparam);
    }
    else
    {
        aloge("we need more arguments");
        PrintDemoUsage();
        ret = -1;
    }

    return ret;
}

int main(int argc, char** argv)
{
    int result = 0;
    ERRORTYPE ret;
    int rc;
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

    alogd("demo video muxer begin");
    DemoVideoMuxerContext *pContext = calloc(1, sizeof(DemoVideoMuxerContext));
    if(NULL == pContext)
    {
        aloge("fatal error! malloc fail");
    }
    result = pthread_mutex_init(&pContext->mMuxChnLock, NULL);
    if (result!=0)
    {
        aloge("fatal error! mutex init fail");
    }
    sem_init(&pContext->finish_sem, 0, 0);
    pContext->mMuxVideoStreamId = 0;
    pContext->mMuxAudioStreamId = 1;

    gpDemoVideoMuxerContext = pContext;
    result = LoadParam(pContext, argc, argv);
    if(result < 0)
    {
        free(pContext);
        gpDemoVideoMuxerContext = NULL;
        return result;
    }
    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    pContext->stream_count_0 = 0;
    pContext->video_finish_flag = 0;
    pContext->max_bitstream_count = pContext->mparam.encoder_num;
    if(pContext->max_bitstream_count <= 0)
    {
        pContext->max_bitstream_count = SAVE_BITSTREAM_COUNT;
    }

    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();
    
    memset(&pContext->config_0, 0, sizeof(VideoInputConfig));
    pContext->config_0.channelId = pContext->mparam.use_vipp_num;
    pContext->config_0.fps     = 15;
    pContext->config_0.gop     = 30;
    pContext->config_0.mRcMode = pContext->mparam.vbr?AW_VBR:AW_CBR;
    pContext->config_0.vbr_opt_enable = pContext->mparam.vbr_opt_en;
    pContext->config_0.qp_range.nMinqp = 35;
    pContext->config_0.qp_range.nMaxqp = 51;
    pContext->config_0.qp_range.nMinPqp = 35;
    pContext->config_0.qp_range.nMaxPqp = 51;
    pContext->config_0.qp_range.nQpInit = 35;
    pContext->config_0.pixelformat = pContext->mparam.pixelformat;
    pContext->config_0.enable_sharp = pContext->mparam.enable_sharp;
    pContext->config_0.bonline_channel = pContext->mparam.bonline_channel;
    pContext->config_0.share_buf_num = pContext->mparam.share_buf_num;
    pContext->config_0.vin_buf_num = pContext->mparam.vin_buf_num;
    pContext->config_0.breduce_refrecmem = 1;
    pContext->config_0.venc_video_signal.video_format = DEFAULT;
    switch(pContext->mparam.color_space)
    {
        case V4L2_COLORSPACE_JPEG:
        {
            pContext->config_0.venc_video_signal.full_range_flag = 1;
            pContext->config_0.venc_video_signal.src_colour_primaries = VENC_YCC;
            pContext->config_0.venc_video_signal.dst_colour_primaries = VENC_YCC;
            break;
        }
        case V4L2_COLORSPACE_REC709:
        {
            pContext->config_0.venc_video_signal.full_range_flag = 1;
            pContext->config_0.venc_video_signal.src_colour_primaries = VENC_BT709;
            pContext->config_0.venc_video_signal.dst_colour_primaries = VENC_BT709;
            break;
        }
        case V4L2_COLORSPACE_REC709_PART_RANGE:
        {
            pContext->config_0.venc_video_signal.full_range_flag = 0;
            pContext->config_0.venc_video_signal.src_colour_primaries = VENC_BT709;
            pContext->config_0.venc_video_signal.dst_colour_primaries = VENC_BT709;
            break;
        }
        default:
        {
            pContext->config_0.venc_video_signal.full_range_flag = 1;
            pContext->config_0.venc_video_signal.src_colour_primaries = VENC_BT709;
            pContext->config_0.venc_video_signal.dst_colour_primaries = VENC_BT709;
            break;
        }
    }
    pContext->config_0.output_mode = OUTPUT_MODE_STREAM;
    pContext->config_0.width      = pContext->mparam.c0_src_w;
    pContext->config_0.height     = pContext->mparam.c0_src_h;
    pContext->config_0.dst_width      = pContext->mparam.c0_dst_w;
    pContext->config_0.dst_height     = pContext->mparam.c0_dst_h;
    pContext->config_0.bitrate    = pContext->mparam.c0_bitrate;
    pContext->config_0.encodeType = pContext->mparam.c0_encoder_format;
    pContext->config_0.drop_frame_num = 0;
    pContext->config_0.enable_wdr = 0;
    if (pContext->config_0.encodeType == 1)
    {//jpg encode
        alogd("config mjpeg for video stream.");
        pContext->config_0.jpg_mode = 1;
        pContext->config_0.jpg_quality = 80;
        pContext->config_0.bit_rate_range.bitRateMax = pContext->config_0.bitrate*1024*2;
        pContext->config_0.bit_rate_range.bitRateMin = pContext->config_0.bitrate*1024/2;
        pContext->config_0.bit_rate_range.nQualityTh = 80;
        pContext->config_0.bit_rate_range.nMinQuality = 10;
        pContext->config_0.bit_rate_range.nMaxQuality = 100;
    }
    else if(pContext->config_0.encodeType == 0) //h264
    {
        pContext->config_0.profile = VENC_H264ProfileMain;
        pContext->config_0.level   = VENC_H264Level51;
    }
    else if(pContext->config_0.encodeType == 2) //h265
    {
        pContext->config_0.profile = VENC_H265ProfileMain;
        pContext->config_0.level   = VENC_H265LevelDefault;
    }
    pContext->config_0.enable_isp2ve_linkage = 1;
    pContext->config_0.enable_ve2isp_linkage = 1; //only main channel need enable ve2isp
#ifdef TEST_SENSOR_A_ROTATE
    pContext->config_0.dma_stitch_rotate_cfg.rotate_en = 1;
    pContext->config_0.dma_stitch_rotate_cfg.sensor_a_rotate = 90;
#endif
    //AWVideoInput_SetLogLevel(AWRT_LOG_LEVEL_VERBOSE);
    AWVideoInput_Init();
    if(AWVideoInput_Configure(pContext->config_0.channelId, &pContext->config_0))
    {
        aloge("config err, exit!");
        goto _exit;
    }
    CreateMuxChn(pContext);
    AWVideoInput_CallBack(pContext->config_0.channelId, video_stream_cb, 0);

#ifdef TEST_SEI
    pContext->stSeiAttr.eSeiEnableSetting = AWVideoInput_SeiEnable;
    //pContext->stSeiAttr.eSeiEnableSetting = AWVideoInput_SeiDisable;
    //pContext->stSeiAttr.eSeiEnableSetting = AWVideoInput_SeiFollowShellSet;

    pContext->stSeiAttr.nSeiDataTypeFlags |= RTSEIDataType_ISP;
    //pContext->stSeiAttr.nSeiDataTypeFlags |= RTSEIDataType_VIPP;
    pContext->stSeiAttr.nSeiDataTypeFlags |= RTSEIDataType_VENC;
    pContext->stSeiAttr.nFrameIntervalForISPLevel1 = 5;
    pContext->stSeiAttr.nFrameIntervalForISPLevel2 = 20;
    pContext->stSeiAttr.nFrameIntervalForISPLevel3 = 200;
    //pContext->stSeiAttr.nFrameIntervalForVIPP = 20;
    pContext->stSeiAttr.nFrameIntervalForVencLevel1 = 20;
    pContext->stSeiAttr.nFrameIntervalForVencLevel2 = 200;
    result = AWVideoInput_VENC_ConfigSEI(pContext->config_0.channelId, &pContext->stSeiAttr);
    if(result != 0)
    {
        aloge("fatal error! rtChn[%d] config sei fail[%d]", pContext->config_0.channelId, result);
    }
#endif
    AWVideoInput_Start(pContext->config_0.channelId, 1);
    pthread_mutex_lock(&pContext->mMuxChnLock);
    if(0 == pContext->mbMuxChnSetSpsppsFlag)
    {
        SetSpsppsToMuxer(pContext);
        pContext->mbMuxChnSetSpsppsFlag = 1;
        AW_MPI_MUX_StartChn(pContext->mMuxChn);
    }
    pthread_mutex_unlock(&pContext->mMuxChnLock);

#ifdef TEST_GETYUV
    int nFrmIdx = 0;
    int nFrmCnt = 5;
    char strYuvFramePath[128];
    while (nFrmIdx < nFrmCnt)
    {
        sleep(1);
        VideoYuvFrame stYuvFrame;
        rc = AWVideoInput_GetYuvFrame(pContext->config_0.channelId, &stYuvFrame);
        if (0 == rc)
        {
            alogd("AWChannel[%d], YuvFrameIdx[%d]: buf %dx%d, data %dx%d, phyAddr[%p-%p-%p], virAddr[%p-%p-%p]", pContext->config_0.channelId,
                nFrmIdx, stYuvFrame.widht, stYuvFrame.height, pContext->config_0.width, pContext->config_0.height, stYuvFrame.phyAddr[0],
                stYuvFrame.phyAddr[1], stYuvFrame.phyAddr[2], stYuvFrame.virAddr[0], stYuvFrame.virAddr[1], stYuvFrame.virAddr[2]);
            char *pStrFrmFormat;
            switch (pContext->config_0.pixelformat)
            {
            case RT_PIXEL_YUV420SP:
            {
                pStrFrmFormat = "nv12";
                break;
            }
            case RT_PIXEL_YVU420SP:
            {
                pStrFrmFormat = "nv21";
                break;
            }
            default:
            {
                pStrFrmFormat = "unknown";
                break;
            }
            }
            snprintf(strYuvFramePath, sizeof(strYuvFramePath)-1, "/mnt/extsd/frame%d_%dx%d.%s", nFrmIdx, stYuvFrame.widht,
                stYuvFrame.height, pStrFrmFormat);
            FILE* pFrameFile = fopen(strYuvFramePath, "wb");
            if (pFrameFile != NULL)
            {
                fwrite(stYuvFrame.virAddr[0], 1, stYuvFrame.widht*stYuvFrame.height*3/2, pFrameFile);
                fclose(pFrameFile);
                alogd("store to file:%s", strYuvFramePath);
            }
            else
            {
                aloge("fatal error! fopen [%s] fail!", strYuvFramePath);
            }
            rc = AWVideoInput_ReleaseYuvFrame(pContext->config_0.channelId, &stYuvFrame);
            if (rc != 0)
            {
                aloge("fatal error! why AWChannel[%d] release yuv frame fail[%d]?", pContext->config_0.channelId, rc);
            }
        }
        else
        {
            aloge("fatal error! why AWChannel[%d] get yuv frame fail[%d]?", pContext->config_0.channelId, rc);
        }
        nFrmIdx++;
    }
#endif

#ifdef TEST_CATCHJPEG
    int nJpegIdx = 0;
    int nJpegCnt = 5;
    char strJpegPath[128];
    char *pJpegBuf = NULL;
    int nJpegSize = pContext->config_0.width * pContext->config_0.height / 2;
    pJpegBuf = (char *)malloc(nJpegSize);
    if (NULL == pJpegBuf)
    {
        aloge("fatal error! AWChannel[%d] malloc %d bytes fail", pContext->config_0.channelId, nJpegSize);
    }
    catch_jpeg_config stCatchJpegConfig;
    memset(&stCatchJpegConfig, 0, sizeof(stCatchJpegConfig));
    stCatchJpegConfig.channel_id = pContext->config_0.channelId;
    stCatchJpegConfig.width = pContext->config_0.width;
    stCatchJpegConfig.height = pContext->config_0.height;
    stCatchJpegConfig.qp = 80;
    stCatchJpegConfig.rotate_angle = 0;
    while (nJpegIdx < nJpegCnt)
    {
        sleep(1);
        rc = AWVideoInput_CatchJpegConfig(&stCatchJpegConfig);
        if (rc != 0)
        {
            aloge("fatal error! AWChannel[%d] catch jpeg config fail[%d]", stCatchJpegConfig.channel_id, rc);
        }
        int nJpegLen = nJpegSize;
        rc = AWVideoInput_CatchJpeg(pJpegBuf, &nJpegLen, stCatchJpegConfig.channel_id);
        if (0 == rc)
        {
            alogd("AWChannel[%d] JpegIdx[%d]: JpegLen[%d]", pContext->config_0.channelId, nJpegIdx, nJpegLen);
            snprintf(strJpegPath, sizeof(strJpegPath)-1, "/mnt/extsd/pic%d.jpg", nJpegIdx);
            FILE* pJpegFile = fopen(strJpegPath, "wb");
            if (pJpegFile != NULL)
            {
                fwrite(pJpegBuf, 1, nJpegLen, pJpegFile);
                fclose(pJpegFile);
                alogd("store to file:%s", strJpegPath);
            }
            else
            {
                aloge("fatal error! fopen [%s] fail!", strJpegPath);
            }
        }
        else
        {
            aloge("fatal error! why AWChannel[%d] catch jpeg fail[%d]?", pContext->config_0.channelId, rc);
        }
        nJpegIdx++;
    }
    if (pJpegBuf)
    {
        free(pJpegBuf);
        pJpegBuf = NULL;
    }
#endif

    //* wait for finish
    sem_wait(&pContext->finish_sem);
    //AWVideoInput_Start(pContext->config_0.channelId, 0);
    AWVideoInput_Destroy(pContext->config_0.channelId);

    pthread_mutex_lock(&pContext->mMuxChnLock);
    alogd("destroy mux group in main()");
    ret = AW_MPI_MUX_StopChn(pContext->mMuxChn, FALSE);
    if (ret != SUCCESS) 
    {
        alogd("fatal error! stop muxChn[%d] fail!", pContext->mMuxChn);
    }
    ret = AW_MPI_MUX_DestroyChn(pContext->mMuxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! check code.");
    }
    pthread_mutex_unlock(&pContext->mMuxChnLock);

_exit:
    AWVideoInput_DeInit();
    AW_MPI_SYS_Exit();
    sem_destroy(&pContext->finish_sem);
    result = pthread_mutex_destroy(&pContext->mMuxChnLock);
    if (result!=0)
    {
        aloge("fatal error! mutex destroy fail");
    }
    free(pContext);
    alogd("demo_video_muxer finish!");
    log_quit();
    return 0;
}


