/** @file
  演示从rtmedia获取视频编码流，从MPP获取音频编码流，使用mpi_muxer组件封装为mp4文件的功能.

配置：勾选对应配置后，编译时会将其拷贝到/bin目录下。

```
make menuconfig
Allwinner-> rt_media demo selection -> demo_avmuxer
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
-b0,--bitrate           channel0的码率。单位bps。 e.g.: -b0 1500000。 param->c0_bitraten
-cs,--colorspace        isp的颜色空间。取值为7:V4L2_COLORSPACE_JPEG, 3:V4L2_COLORSPACE_REC709, 31:V4L2_COLORSPACE_REC709_PART_RANGE
-out,--outputfile       输出文件的路径的前缀。e.g.: /mnt/extsd/stream0_encoder。param->OutputFilePath
```

测试命令示例：

```
demo_avmuxer -n 150 -s0 1920x1080 -f0 0 -vn 0 -pf 12 -sp 1 -b0 1500000 -cs 3 -out /mnt/extsd/demo_avmuxer.mp4

./demo_avmuxer -n 150 -s0 1920x1080 -f0 1 -vn 0 -pf 12 -vb 4 -sp 1 -b0 3145728 -cs 3 -out /mnt/extsd/demo_avmuxer_mjpeg.mp4

```

生成文件：/mnt/extsd/demo_avmuxer.mp4

*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/time.h>

#include "demo_avmuxer.h"

#include <linux/videodev2.h>
#include <media/sunxi_camera_v2.h>

#include <plat_log.h>
#include <aenc_sw_lib.h>
#include <mm_comm_venc.h>
#include <media_common_aio.h>
#include <media_common_vcodec.h>
#include <mpi_sys.h>
#include <mpi_mux.h>
#include <mpi_ai.h>
#include <mpi_aenc.h>

#define OUT_PUT_FILE_PREFIX "/mnt/extsd/demo_avmuxer.mp4"
//#define TEST_SEI

typedef enum
{
    DemoAvMuxer_Stop = 0,
}DemoAvMuxerMsgType;

DemoAvmuxerContext *gpDemoAvmuxerContext = NULL;

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    DemoAvmuxerContext *pCtx = (DemoAvmuxerContext*)cookie;
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

static int CreateMuxChn(DemoAvmuxerContext *pCtx)
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
    pCtx->mMuxChnAttr.mVideoAttr[0].mVideoEncodeType = map_RT_VENC_CODEC_TYPE_to_PAYLOAD_TYPE_E(pCtx->config_0.encodeType);
    pCtx->mMuxChnAttr.mVideoAttr[0].mRotateDegree = 0;
    //we can do this, muxer use veChn to link spspps and streamId, muxer don't need to operate veChn.
    //so we can use any number as veChn.
    pCtx->mMuxChnAttr.mVideoAttr[0].mVeChn = pCtx->config_0.channelId;
    pCtx->mMuxChnAttr.mChannels = pCtx->nAudioChnNum;
    pCtx->mMuxChnAttr.mBitsPerSample = pCtx->nAudioBitWidth;
    pCtx->mMuxChnAttr.mSamplesPerFrame = pCtx->nAudioSamplesPerFrame;
    pCtx->mMuxChnAttr.mSampleRate = pCtx->nAudioSampleRate;
    pCtx->mMuxChnAttr.mAudioEncodeType = pCtx->eAudioEncodeType;
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
        aloge("fatal error! set file duration policy[%d] to muxChn[%d] fail!", pCtx->mPolicy, pCtx->mMuxChn);
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
static int SetSpsppsToMuxer(DemoAvmuxerContext *pContext)
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
    DemoAvmuxerContext *pContext = gpDemoAvmuxerContext;
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
static int LoadParam(DemoAvmuxerContext *pContext, int argc, char** argv)
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

    //config audio param here
    pContext->nAudioChnNum = 1;
    pContext->nAudioBitWidth = 16;
    pContext->nAudioSamplesPerFrame = MAXDECODESAMPLE;
    pContext->nAudioSampleRate = 16000;
    pContext->eAudioEncodeType = PT_AAC;
    pContext->nAudioBitRate = 32000;
    return ret;
}

static void* StreamDispatchThread(void *pThreadData)
{
    DemoAvmuxerContext *pContext = (DemoAvmuxerContext*)pThreadData;
    int result = 0;
    ERRORTYPE ret;
    message_t stCmdMsg;
    handle_set rdFds;
    prctl(PR_SET_NAME, (unsigned long)"StrmDispatch", 0, 0, 0);

    int nAeChnFd = AW_MPI_AENC_GetHandle(pContext->mAEncChnId);
    int nMaxFd = nAeChnFd;
    int nReadyCnt;

    while(1)
    {
    PROCESS_MESSAGE:
        if (get_message(&pContext->mStreamDispatchCmdQueue, &stCmdMsg) == 0)
        {
            alogv("StreamDispatch Thread receive command:%d", stCmdMsg.command);
            if (DemoAvMuxer_Stop == stCmdMsg.command)
            {
                // Kill thread
                goto EXIT;
            }
            else
            {
                aloge("fatal error! unknown command:%d", stCmdMsg.command);
            }
            //precede to process message
            goto PROCESS_MESSAGE;
        }

        AW_MPI_SYS_HANDLE_ZERO(&rdFds);
        AW_MPI_SYS_HANDLE_SET(nAeChnFd, &rdFds);
        nReadyCnt = AW_MPI_SYS_HANDLE_Select(nMaxFd+1, &rdFds, 200);
        if(nReadyCnt > 0)
        {
            if(AW_MPI_SYS_HANDLE_ISSET(nAeChnFd, &rdFds))
            {
                AUDIO_STREAM_S stAudioStream;
                ret = AW_MPI_AENC_GetStream(pContext->mAEncChnId, &stAudioStream, 0);
                if (SUCCESS == ret)
                {
                    ret = AW_MPI_MUX_SendAudioStreamSync(pContext->mMuxChn, &stAudioStream, pContext->mMuxAudioStreamId);
                    if(ret != SUCCESS)
                    {
                        aloge("fatal error! send audio stream to mux fail[0x%x]", ret);
                    }
                    ret = AW_MPI_AENC_ReleaseStream(pContext->mAEncChnId, &stAudioStream);
                    if(ret != SUCCESS)
                    {
                        aloge("fatal error! why aenc release stream fail[0x%x]?", ret);
                    }
                }
                else
                {
                    aloge("fatal error! aenc get stream fail[0x%x]!", ret);
                }
            }
            else
            {
                aloge("fatal error! IsSet fail?");
            }
        }
        else
        {
            alogd("handle_select timeout! fd cnts:%d", nReadyCnt);
        }
    }

EXIT:
    alogd("StreamDispatch thread exit");
    return (void*)result;
}

int main(int argc, char** argv)
{
    int result = 0;
    ERRORTYPE ret;
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

    alogd("demo avmuxer begin");
    DemoAvmuxerContext *pContext = calloc(1, sizeof(DemoAvmuxerContext));
    if(NULL == pContext)
    {
        aloge("fatal error! malloc fail");
    }
    result = pthread_mutex_init(&pContext->mMuxChnLock, NULL);
    if (result!=0)
    {
        aloge("fatal error! mutex init fail");
    }
    result = message_create(&pContext->mStreamDispatchCmdQueue);
    if(result != 0)
    {
        aloge("fatal error! message create fail!");
    }
    sem_init(&pContext->finish_sem, 0, 0);
    pContext->mMuxVideoStreamId = 0;
    pContext->mMuxAudioStreamId = 1;

    gpDemoAvmuxerContext = pContext;
    result = LoadParam(pContext, argc, argv);
    if(result < 0)
    {
        free(pContext);
        gpDemoAvmuxerContext = NULL;
        return result;
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
    pContext->config_0.mRcMode     = AW_CBR;
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

    AWVideoInput_Init();
    if(AWVideoInput_Configure(pContext->config_0.channelId, &pContext->config_0))
    {
        aloge("config err, exit!");
        goto _exit;
    }
    //prepare audio stream
    pContext->mAIDevId = 0;
    pContext->mAIChnId = 0;
    pContext->mAioAttr.enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(pContext->nAudioSampleRate);
    pContext->mAioAttr.enBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(pContext->nAudioBitWidth);
    pContext->mAioAttr.mChnCnt = pContext->nAudioChnNum;
    pContext->mAioAttr.mMicNum = 1;
    pContext->mAioAttr.ai_aec_en = 0;
    pContext->mAioAttr.mbBypassAec = 0;
    pContext->mAioAttr.ai_ans_en = 0;
    pContext->mAioAttr.ai_agc_en = 0;
    AW_MPI_AI_SetPubAttr(pContext->mAIDevId, &pContext->mAioAttr);
    AW_MPI_AI_Enable(pContext->mAIDevId);
    ret = AW_MPI_AI_CreateChn(pContext->mAIDevId, pContext->mAIChnId, NULL);
    if (ret != SUCCESS)
    {
        aloge("fatal error! create ai chn fail");
    }
    pContext->mAEncChnId = 0;
    pContext->mAEncAttr.AeAttr.Type = pContext->eAudioEncodeType;
    pContext->mAEncAttr.AeAttr.channels = pContext->nAudioChnNum;
    pContext->mAEncAttr.AeAttr.bitsPerSample = pContext->nAudioBitWidth;
    pContext->mAEncAttr.AeAttr.sampleRate = pContext->nAudioSampleRate;
    pContext->mAEncAttr.AeAttr.bitRate = pContext->nAudioBitRate;
    pContext->mAEncAttr.AeAttr.attachAACHeader = 0;
    pContext->mAEncAttr.AeAttr.mInBufSize = 0;
    pContext->mAEncAttr.AeAttr.mOutBufCnt = 0;
    ret = AW_MPI_AENC_CreateChn(pContext->mAEncChnId, &pContext->mAEncAttr);
    if (ret != SUCCESS)
    {
        aloge("fatal error! create aenc chn fail");
    }
    MPP_CHN_S stSrcAiChn = {MOD_ID_AI, pContext->mAIDevId, pContext->mAIChnId};
    MPP_CHN_S stDstAEncChn = {MOD_ID_AENC, 0, pContext->mAEncChnId};
    AW_MPI_SYS_Bind(&stSrcAiChn, &stDstAEncChn);
    //prepare mux chn
    CreateMuxChn(pContext);

    //start to record
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
    AW_MPI_AI_EnableChn(pContext->mAIDevId, pContext->mAIChnId);
    AW_MPI_AENC_StartRecvPcm(pContext->mAEncChnId);
    result = pthread_create(&pContext->StreamDispatchThreadId, NULL, StreamDispatchThread, pContext);
    if(result != 0)
    {
        aloge("fatal error! pthread create fail!");
    }

    //* wait for finish
    sem_wait(&pContext->finish_sem);
    //AWVideoInput_Start(pContext->config_0.channelId, 0);
    AWVideoInput_Destroy(pContext->config_0.channelId);

    message_t stMsg;
    InitMessage(&stMsg);
    stMsg.command = DemoAvMuxer_Stop;
    put_message(&pContext->mStreamDispatchCmdQueue, &stMsg);
    int StreamDispatchResult;
    result = pthread_join(pContext->StreamDispatchThreadId, (void**) &StreamDispatchResult);
    if(result != 0)
    {
        aloge("fatal error! pthread join fail[%d]", result);
    }

    //stop
    ret = AW_MPI_AI_DisableChn(pContext->mAIDevId, pContext->mAIChnId);
    if(ret != SUCCESS)
    {
        aloge("fatal error! ai disable chn fail[0x%x]", ret);
    }
    ret = AW_MPI_AENC_StopRecvPcm(pContext->mAEncChnId);
    if(ret != SUCCESS)
    {
        aloge("fatal error! aenc stop chn fail[0x%x]", ret);
    }
    ret = AW_MPI_MUX_StopChn(pContext->mMuxChn, FALSE);
    if(ret != SUCCESS)
    {
        aloge("fatal error! muxChn stop fail[0x%x]", ret);
    }
    //destroy
    ret = AW_MPI_MUX_DestroyChn(pContext->mMuxChn);
    if(ret != SUCCESS)
    {
        aloge("fatal error! muxChn destroy fail[0x%x]", ret);
    }
    ret = AW_MPI_AENC_DestroyChn(pContext->mAEncChnId);
    if(ret != SUCCESS)
    {
        aloge("fatal error! aenc destroy chn fail[0x%x]", ret);
    }
    ret = AW_MPI_AI_DestroyChn(pContext->mAIDevId, pContext->mAIChnId);
    if(ret != SUCCESS)
    {
        aloge("fatal error! ai destroy chn fail[0x%x]", ret);
    }

_exit:
    AWVideoInput_DeInit();
    AW_MPI_SYS_Exit();
    sem_destroy(&pContext->finish_sem);
    message_destroy(&pContext->mStreamDispatchCmdQueue);
    result = pthread_mutex_destroy(&pContext->mMuxChnLock);
    if (result!=0)
    {
        aloge("fatal error! mutex destroy fail");
    }
    free(pContext);
    alogd("demo_avmuxer finish!");
    log_quit();
    return 0;
}

