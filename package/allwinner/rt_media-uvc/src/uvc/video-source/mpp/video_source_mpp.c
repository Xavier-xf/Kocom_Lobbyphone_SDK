#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>

#include <include/media/mm_comm_vi.h>
#include <include/media/mm_comm_venc.h>
#include <include/media/mpi_vi.h>
#include <include/media/mpi_isp.h>
#include <include/media/mpi_venc.h>
#include <include/media/mpi_sys.h>
#include <include/media/mpi_videoformat_conversion.h>

#include "base_list.h"
#include "include/sample_common_venc.h"
#include "../base/include/video_source.h"
#include "../../../utils/debug/include/debug.h"

#define ISP_RUN                                 (1)

#define FRAME_BUF_LEN                           (200*1024)
#define MAX_VIDEO_SOURCE_MPP_CONTEXT_NUM        (16)
#define VE_VBV_CACHE_TIME                       (4)  // unit:seconds, exp:1,2,3,4...

#define STREAM_VIPP_BUF_NUM                     (3)
#define STREAM_ENCODE_ONLINE                    (0)
#define STREAM_ENCODE_ENCPP                     (1)
#define STREAM_VBV_BUF_SIZE                     (2*1024*1024)
#define DUAL_STREAM_ENCODE_TYPE                 (PT_H264)
#define DUAL_STREAM_FRAMERATE                   (15)
#define DUAL_STREAM_WIDTH                       (640)
#define DUAL_STREAM_HEIGHT                      (360)
#define DUAL_STREAM_BITRATE                     (512*1024)

struct video_source_mpp_frame_node {
    struct video_source_frame frame;
    struct list_head list;
};

struct video_source_mpp_frame_manager {
    int list_entry_num;
    pthread_mutex_t list_lock;
    struct list_head idle_list; //list entry: struct video_source_frame
    struct list_head ready_list;
    struct list_head using_list;
};

struct video_source_mpp_stream {
    VI_DEV vipp_dev;
    VI_CHN vipp_chn;
    ISP_DEV isp_dev;
    VENC_CHN ve_chn;

    BOOL capture_thread_exit_flag;
    BOOL capture_thread_running;
    pthread_t capture_thread_trd;

    struct video_source_mpp_frame_manager frame_manager;
    int (*config_frame)(struct video_source_mpp_stream *uvc_stream, struct video_source_frame *frame);

    void *private;
};

struct video_source_mpp_context
{
    struct video_source_mpp_stream stream;
    struct video_source_mpp_stream dual_stream;

    struct video_source_base_config base_config;
    struct video_source_extra_config extra_config;
};

static int mpp_init_cnt;

static unsigned int getSysTickMs()
{
    unsigned int ms = 0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ms = tv.tv_sec*1000 + tv.tv_usec/1000;
    return ms;
}

static int get_isp_id(int vipp)
{
    switch (vipp) {
        case 0:
        case 4:
        case 8:
        case 12:
            return 0;
        case 1:
        case 5:
        case 9:
        case 13:
            return 1;
        default:
            loge("invalid vipp %d!", vipp);
            return 0;
    };
}

/*static void mpp_init()
{
    if (!mpp_init_cnt) {
        logd("mpp sys init");
        MPP_SYS_CONF_S sys_conf;
        memset(&sys_conf, 0, sizeof(MPP_SYS_CONF_S));
        sys_conf.nAlignWidth = 32;
        AW_MPI_SYS_SetConf(&sys_conf);
        AW_MPI_SYS_Init();
    }
    mpp_init_cnt++;
}

static void mpp_deinit()
{
    mpp_init_cnt--;
    if (!mpp_init_cnt) {
        logd("mpp sys exit");
        AW_MPI_SYS_Exit();
    }
    if (mpp_init_cnt < 0)
        mpp_init_cnt = 0;
}*/

static void init_frame_manager(struct video_source_mpp_frame_manager *frame_manager)
{
    INIT_LIST_HEAD(&frame_manager->idle_list);
    INIT_LIST_HEAD(&frame_manager->ready_list);
    INIT_LIST_HEAD(&frame_manager->using_list);
    pthread_mutex_init(&frame_manager->list_lock, NULL);
    for (int i = 0; i < frame_manager->list_entry_num; i++) {
        struct video_source_mpp_frame_node *node = malloc(sizeof(*node)*2);
        if (!node)
            loge("rt-media video source frame node malloc fail!\n");
        memset(node, 0, sizeof(*node));
        node->frame.buf_len = FRAME_BUF_LEN;
        node->frame.buf_vir_addr = malloc(node->frame.buf_len);
        if (!node->frame.buf_vir_addr) {
            loge("malloc buffer len %d fail!\n", node->frame.buf_len);
            node->frame.buf_len = 0;
        } else {
            memset(node->frame.buf_vir_addr, 0, node->frame.buf_len);
            logv("malloc buf %p len %d\n", node->frame.buf_vir_addr, node->frame.buf_len);
        }
        list_add_tail(&node->list, &frame_manager->idle_list);
    }
}

static void deinit_frame_manager(struct video_source_mpp_frame_manager *frame_manager)
{
    struct video_source_mpp_frame_node *node, *tmp;
    list_for_each_entry_safe(node, tmp, &frame_manager->using_list, list) {
        list_del(&node->list);
        if (node->frame.buf_vir_addr) {
            free(node->frame.buf_vir_addr);
            node->frame.buf_vir_addr = NULL;
        }
        if (node) {
            free(node);
            node = NULL;
        }
    }
    list_for_each_entry_safe(node, tmp, &frame_manager->ready_list, list) {
        list_del(&node->list);
        if (node->frame.buf_vir_addr) {
            free(node->frame.buf_vir_addr);
            node->frame.buf_vir_addr = NULL;
        }
        if (node) {
            free(node);
            node = NULL;
        }
    }
    list_for_each_entry_safe(node, tmp, &frame_manager->idle_list, list) {
        list_del(&node->list);
        if (node->frame.buf_vir_addr) {
            free(node->frame.buf_vir_addr);
            node->frame.buf_vir_addr = NULL;
        }
        if (node) {
            free(node);
            node = NULL;
        }
    }
    pthread_mutex_destroy(&frame_manager->list_lock);
}

ERRORTYPE VencStreamCallBack(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    VENC_STREAM_S *pFrame = (VENC_STREAM_S *)pEventData;

    switch (event) {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            break;
        default:
            break;
    }

    return SUCCESS;
}

static int config_mjpeg_frame(struct video_source_mpp_stream *uvc_stream, struct video_source_frame *frame)
{
    VENC_PACK_S venc_pack;
    VENC_STREAM_S venc_stream;
    memset(&venc_pack, 0, sizeof(venc_pack));
    memset(&venc_stream, 0, sizeof(venc_stream));
    venc_stream.mPackCount = 1;
    venc_stream.mpPack = &venc_pack;
    int ret = AW_MPI_VENC_GetStream(uvc_stream->ve_chn, &venc_stream, -1);
    if (ret != SUCCESS)
        return ret;

    unsigned int offset = 0;
    unsigned int src_len = venc_stream.mpPack->mLen0 + venc_stream.mpPack->mLen1 + venc_stream.mpPack->mLen2;
    if (frame->buf_len < src_len) {
        AW_MPI_VENC_ReleaseStream(uvc_stream->ve_chn, &venc_stream);
        return -1;
    }
    if (venc_stream.mpPack->mLen0 && venc_stream.mpPack->mpAddr0) {
        memcpy(frame->buf_vir_addr, venc_stream.mpPack->mpAddr0, venc_stream.mpPack->mLen0);
        offset += venc_stream.mpPack->mLen0;
    }
    if (venc_stream.mpPack->mLen1 && venc_stream.mpPack->mpAddr1) {
        memcpy(frame->buf_vir_addr + offset, venc_stream.mpPack->mpAddr1, venc_stream.mpPack->mLen1);
        offset += venc_stream.mpPack->mLen1;
    }
    if (venc_stream.mpPack->mLen2 && venc_stream.mpPack->mpAddr2) {
        memcpy(frame->buf_vir_addr + offset, venc_stream.mpPack->mpAddr2, venc_stream.mpPack->mLen2);
        offset += venc_stream.mpPack->mLen2;
    }
    frame->data_len = offset;

    AW_MPI_VENC_ReleaseStream(uvc_stream->ve_chn, &venc_stream);

    return 0;
}

/*
  src frame format is nv12, because isp unsupport output yuv422
  dst frame format is yuyv.
  only copy y data, uv need convert!
*/
static int config_yuyv_frame(struct video_source_mpp_stream *uvc_stream, struct video_source_frame *frame)
{
    VIDEO_FRAME_INFO_S video_frame;
    memset(&video_frame, 0, sizeof(video_frame));
    int ret = AW_MPI_VI_GetFrame(uvc_stream->vipp_dev, uvc_stream->vipp_chn, &video_frame, -1);
    if (ret != SUCCESS)
        return ret;
    unsigned int src_len = video_frame.VFrame.mWidth * video_frame.VFrame.mHeight * 3 / 2;
    unsigned int dst_len = video_frame.VFrame.mWidth * video_frame.VFrame.mHeight * 2;

    if (frame->buf_len < src_len) {
        AW_MPI_VI_ReleaseFrame(uvc_stream->vipp_dev, uvc_stream->vipp_chn, &video_frame);
        return -1;
    }
    memcpy(frame->buf_vir_addr, video_frame.VFrame.mpVirAddr[0],
                video_frame.VFrame.mWidth * video_frame.VFrame.mHeight);
    //memcpy(frame->buf_vir_addr, video_frame.VFrame.mpVirAddr[1],
    //            video_frame.VFrame.mWidth * video_frame.VFrame.mHeight / 2);
    frame->data_len = dst_len;
    AW_MPI_VI_ReleaseFrame(uvc_stream->vipp_dev, uvc_stream->vipp_chn, &video_frame);

    return 0;
}

static int config_h264_frame(struct video_source_mpp_stream *uvc_stream, struct video_source_frame *frame)
{
    VENC_PACK_S venc_pack;
    VENC_STREAM_S venc_stream;
    memset(&venc_pack, 0, sizeof(venc_pack));
    memset(&venc_stream, 0, sizeof(venc_stream));
    venc_stream.mPackCount = 1;
    venc_stream.mpPack = &venc_pack;
    int ret = AW_MPI_VENC_GetStream(uvc_stream->ve_chn, &venc_stream, -1);
    if (ret != SUCCESS)
        return ret;

    unsigned int offset = 0;
    unsigned int src_len = venc_stream.mpPack->mLen0 + venc_stream.mpPack->mLen1 + venc_stream.mpPack->mLen2;
    VencHeaderData header;
    if (venc_stream.mpPack->mDataType.enH264EType == H264E_NALU_ISLICE) {
        AW_MPI_VENC_GetH264SpsPpsInfo(uvc_stream->ve_chn, &header);
        src_len += header.nLength;
    }
    if (frame->buf_len < src_len) {
        AW_MPI_VENC_ReleaseStream(uvc_stream->ve_chn, &venc_stream);
        return -1;
    }
    if (venc_stream.mpPack->mDataType.enH264EType == H264E_NALU_ISLICE) {
        memcpy(frame->buf_vir_addr, header.pBuffer, header.nLength);
        offset += header.nLength;
    }
    if (venc_stream.mpPack->mLen0 && venc_stream.mpPack->mpAddr0) {
        memcpy(frame->buf_vir_addr + offset, venc_stream.mpPack->mpAddr0, venc_stream.mpPack->mLen0);
        offset += venc_stream.mpPack->mLen0;
    }
    if (venc_stream.mpPack->mLen1 && venc_stream.mpPack->mpAddr1) {
        memcpy(frame->buf_vir_addr + offset, venc_stream.mpPack->mpAddr1, venc_stream.mpPack->mLen1);
        offset += venc_stream.mpPack->mLen1;
    }
    if (venc_stream.mpPack->mLen2 && venc_stream.mpPack->mpAddr2) {
        memcpy(frame->buf_vir_addr + offset, venc_stream.mpPack->mpAddr2, venc_stream.mpPack->mLen2);
        offset += venc_stream.mpPack->mLen2;
    }
    frame->data_len = offset;

    AW_MPI_VENC_ReleaseStream(uvc_stream->ve_chn, &venc_stream);

    return 0;
}

static int config_nv12_frame(struct video_source_mpp_stream *uvc_stream, struct video_source_frame *frame)
{
    VIDEO_FRAME_INFO_S video_frame;
    memset(&video_frame, 0, sizeof(video_frame));
    int ret = AW_MPI_VI_GetFrame(uvc_stream->vipp_dev, uvc_stream->vipp_chn, &video_frame, -1);
    if (ret != SUCCESS)
        return ret;
    unsigned int src_len = video_frame.VFrame.mWidth * video_frame.VFrame.mHeight * 3 / 2;
    if (frame->buf_len < src_len) {
        AW_MPI_VI_ReleaseFrame(uvc_stream->vipp_dev, uvc_stream->vipp_chn, &video_frame);
        return -1;
    }
    memcpy(frame->buf_vir_addr, video_frame.VFrame.mpVirAddr[0],
                video_frame.VFrame.mWidth * video_frame.VFrame.mHeight);
    memcpy(frame->buf_vir_addr, video_frame.VFrame.mpVirAddr[1],
                video_frame.VFrame.mWidth * video_frame.VFrame.mHeight / 2);
    frame->data_len = src_len;
    ret = AW_MPI_VI_ReleaseFrame(uvc_stream->vipp_dev, uvc_stream->vipp_chn, &video_frame);

    return 0;
}

static void *DualStreamCaptureThread(void *thread_data)
{
    int frame_len = 0;
    ERRORTYPE eRet = SUCCESS;
    VENC_BUF_STATUS venc_buf_status;
    VENC_STREAM_S venc_stream;
    VENC_PACK_S venc_pack;
    VencInsertData venc_insert_data;
    int offset = 0;
    VencHeaderData venc_headerdata;
    char *tmp_buffer = NULL;
    unsigned int tmp_buffer_len = 0;
    struct video_source_mpp_context *context = (struct video_source_mpp_context *)thread_data;

    context->dual_stream.capture_thread_running = TRUE;
    memset(&venc_headerdata, 0, sizeof(VencHeaderData));
    if (DUAL_STREAM_ENCODE_TYPE == PT_H264)
        AW_MPI_VENC_GetH264SpsPpsInfo(context->dual_stream.ve_chn, &venc_headerdata);
    else if (DUAL_STREAM_ENCODE_TYPE == PT_H265)
        AW_MPI_VENC_GetH265SpsPpsInfo(context->dual_stream.ve_chn, &venc_headerdata);
    while (1)
    {
        if (context->stream.capture_thread_exit_flag)
            break;

        eRet = AW_MPI_VENC_GetInsertDataBufStatus(context->stream.ve_chn, &venc_buf_status);
        if (venc_buf_status == BUF_IDLE)
        {
            memset(&venc_pack, 0, sizeof(VENC_PACK_S));
            memset(&venc_stream, 0, sizeof(VENC_STREAM_S));
            venc_stream.mPackCount = 1;
            venc_stream.mpPack = &venc_pack;
            eRet = AW_MPI_VENC_GetStream(context->dual_stream.ve_chn, &venc_stream, 200);
            if (eRet != SUCCESS)
                continue;

            if ((DUAL_STREAM_ENCODE_TYPE == PT_H264)
                && (venc_stream.mpPack[0].mDataType.enH264EType == H264E_NALU_ISLICE))
                frame_len = venc_stream.mpPack[0].mLen0 + venc_stream.mpPack[0].mLen1
                + venc_stream.mpPack[0].mLen2 + venc_headerdata.nLength;
            else if ((DUAL_STREAM_ENCODE_TYPE == PT_H265)
                && (venc_stream.mpPack[0].mDataType.enH265EType == H265E_NALU_ISLICE))
                frame_len = venc_stream.mpPack[0].mLen0 + venc_stream.mpPack[0].mLen1
                + venc_stream.mpPack[0].mLen2 + venc_headerdata.nLength;
            else
                frame_len = venc_stream.mpPack[0].mLen0 + venc_stream.mpPack[0].mLen1 + venc_stream.mpPack[0].mLen2;
            if (tmp_buffer_len < frame_len)
            {
                logd("realloc dual stream tmp buffer! buffer len change %d -> %d!",
                    tmp_buffer_len, frame_len);
                if (tmp_buffer)
                {
                    free(tmp_buffer);
                    tmp_buffer = NULL;
                }
                tmp_buffer = malloc(frame_len);
                if (!tmp_buffer)
                    loge("alloc dual stream tmp buffer fail!");
                tmp_buffer_len = frame_len;
            }
            if (!tmp_buffer)
            {
                loge("fatal error! dual stream tmp buffer is null!");
                AW_MPI_VENC_ReleaseStream(context->dual_stream.ve_chn, &venc_stream);
                continue;
            }
            offset = 0;
            if (((DUAL_STREAM_ENCODE_TYPE == PT_H264) && venc_stream.mpPack[0].mDataType.enH264EType == H264E_NALU_ISLICE)
                || ((DUAL_STREAM_ENCODE_TYPE == PT_H265) && venc_stream.mpPack[0].mDataType.enH265EType == H265E_NALU_ISLICE))
            {
                memcpy(tmp_buffer, venc_headerdata.pBuffer, venc_headerdata.nLength);
                offset += venc_headerdata.nLength;
            }
            if (venc_stream.mpPack[0].mLen0)
            {
                memcpy(tmp_buffer + offset, venc_stream.mpPack[0].mpAddr0, venc_stream.mpPack[0].mLen0);
                offset += venc_stream.mpPack[0].mLen0;
            }
            if (venc_stream.mpPack[0].mLen1)
            {
                memcpy(tmp_buffer + offset, venc_stream.mpPack[0].mpAddr1, venc_stream.mpPack[0].mLen1);
                offset += venc_stream.mpPack[0].mLen1;
            }
            if (venc_stream.mpPack[0].mLen2)
            {
                memcpy(tmp_buffer + offset, venc_stream.mpPack[0].mpAddr2, venc_stream.mpPack[0].mLen2);
            }
            memset(&venc_insert_data, 0, sizeof(VencInsertData));
            venc_insert_data.pBuffer = (unsigned char *)tmp_buffer;
            venc_insert_data.nDataLen = frame_len;
            venc_insert_data.nFrameRate = DUAL_STREAM_FRAMERATE;
            eRet = AW_MPI_VENC_SetInsertData(context->stream.ve_chn, &venc_insert_data);
            if (eRet != SUCCESS)
                logw("venc %d insert data fail! data 0x%p len %d",
                    context->dual_stream.ve_chn, tmp_buffer, frame_len);
            AW_MPI_VENC_ReleaseStream(context->dual_stream.ve_chn, &venc_stream);
        }
    }
    if (tmp_buffer)
        free(tmp_buffer);
    return (void *)NULL;
}

static void *CaptureThread(void *pArg)
{
    int ret = 0;
    struct video_source_mpp_context *context = (struct video_source_mpp_context *)pArg;

    context->stream.capture_thread_running = TRUE;
    logd("get video frame thread running!");

    switch (context->base_config.format) {
    case V4L2_PIX_FMT_MJPEG:
        context->stream.config_frame = config_mjpeg_frame;
        break;

    case V4L2_PIX_FMT_H264:
        context->stream.config_frame = config_h264_frame;
        break;

    case V4L2_PIX_FMT_YUYV:
        context->stream.config_frame = config_yuyv_frame;
        break;

    case V4L2_PIX_FMT_NV12:
        context->stream.config_frame = config_nv12_frame;
        break;
    default:
        loge("unsupport format 0x%x", context->base_config.format);
        return (void *)NULL;
    }

    while(1)
    {
        if (context->stream.capture_thread_exit_flag) {
            logd("capture thread exit!");
            break;
        }

        pthread_mutex_lock(&context->stream.frame_manager.list_lock);
        struct video_source_mpp_frame_node *node =
            list_first_entry_or_null(&context->stream.frame_manager.idle_list,struct video_source_mpp_frame_node, list);
        if (NULL == node) {
            pthread_mutex_unlock(&context->stream.frame_manager.list_lock);
            usleep(10*1000);
            continue;
        }
        ret = context->stream.config_frame(&context->stream, &node->frame);
        if (ret) {
            pthread_mutex_unlock(&context->stream.frame_manager.list_lock);
            continue;
        }
        list_move_tail(&node->list, &context->stream.frame_manager.ready_list);
        pthread_mutex_unlock(&context->stream.frame_manager.list_lock);
    }

    return (void *)NULL;
}

static int createVipp(struct video_source_mpp_context *context, BOOL dual_stream)
{
    int ret = 0;
    ERRORTYPE eRet = SUCCESS;
    VI_DEV vipp_dev = MM_INVALID_DEV;
    VI_CHN vipp_chn = MM_INVALID_CHN;
    ISP_DEV isp_dev = MM_INVALID_DEV;
    VI_ATTR_S stVippAttr;
    PIXEL_FORMAT_E format;
    int width, height, framerate;

    if (dual_stream)
    {
        if (context->extra_config.dual_stream_vipp_dev)
            vipp_dev = context->extra_config.dual_stream_vipp_dev;
        else
            vipp_dev = context->extra_config.vipp + 4;
        isp_dev = get_isp_id(vipp_dev);
        width = DUAL_STREAM_WIDTH;
        height = DUAL_STREAM_HEIGHT;
        format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        framerate = DUAL_STREAM_FRAMERATE;
    }
    else
    {
        vipp_dev = context->extra_config.vipp;
        isp_dev = get_isp_id(vipp_dev);
        width = context->base_config.width;
        height = context->base_config.height;
        if ((context->base_config.format == V4L2_PIX_FMT_MJPEG) || (context->base_config.format == V4L2_PIX_FMT_H264))
            format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        else
            format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        framerate = context->base_config.framerate;
    }

    memset(&stVippAttr, 0, sizeof(stVippAttr));
    if ((STREAM_ENCODE_ONLINE) && (context->base_config.format != V4L2_PIX_FMT_YUYV) && (vipp_dev == 0))
    {
        stVippAttr.mOnlineEnable = 1;
        stVippAttr.mOnlineShareBufNum = 1;
    }
    stVippAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    stVippAttr.memtype = V4L2_MEMORY_MMAP;
    stVippAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(format);
    stVippAttr.format.field = V4L2_FIELD_NONE;
    stVippAttr.format.colorspace = V4L2_COLORSPACE_REC709;
    stVippAttr.format.width = width;
    stVippAttr.format.height = height;
    stVippAttr.fps = framerate;
    stVippAttr.use_current_win = 0;
    stVippAttr.wdr_mode = 0;
    stVippAttr.nbufs = STREAM_VIPP_BUF_NUM;
    stVippAttr.nplanes = 2;
    stVippAttr.drop_frame_num = 0;
    stVippAttr.mbEncppEnable = STREAM_ENCODE_ENCPP;
    logd("pixformat 0x%x size %dx%d framerate %d", stVippAttr.format.pixelformat,
        stVippAttr.format.width, stVippAttr.format.height, stVippAttr.fps);

    eRet = AW_MPI_VI_CreateVipp(vipp_dev);
    if (SUCCESS != eRet)
    {
        ret = -1;
        vipp_dev = MM_INVALID_DEV;
        loge("fatal error! create vipp[%d] fail!", vipp_dev);
        goto _create_vipp_result;
    }
    AW_MPI_VI_SetVippAttr(vipp_dev, &stVippAttr);

#if ISP_RUN
    if (isp_dev >= 0)
    {
        AW_MPI_ISP_Run(isp_dev);
    }
    else
    {
        isp_dev = MM_INVALID_DEV;
    }
#endif

    vipp_chn = 0;
    eRet = AW_MPI_VI_CreateVirChn(vipp_dev, vipp_chn, NULL);
    if (SUCCESS != eRet)
    {
        vipp_chn = MM_INVALID_CHN;
        loge("fatal error! create vir chn[%d] fail!", vipp_chn);
        return -1;
    }

    eRet = AW_MPI_VI_EnableVipp(vipp_dev);
    if (SUCCESS != eRet)
    {
        ret = -1;
        loge("fatal error! enable vipp[%d] fail!", vipp_dev);
        goto _create_vipp_result;
    }

    logd("create vipp[%d] vir vi chn[%d]", vipp_dev, vipp_chn);

_create_vipp_result:
    if (dual_stream)
    {
        context->dual_stream.vipp_dev = vipp_dev;
        context->dual_stream.vipp_chn = vipp_chn;
        context->dual_stream.isp_dev = isp_dev;
    }
    else
    {
        context->stream.vipp_dev = vipp_dev;
        context->stream.vipp_chn = vipp_chn;
        context->stream.isp_dev = isp_dev;
    }
    return ret;
}

static int createVeChn(struct video_source_mpp_context *context, BOOL dual_stream)
{
    ERRORTYPE eRet = SUCCESS;
    VI_DEV vipp_dev;
    VENC_CHN ve_chn;
    VENC_CHN_ATTR_S stVEncChnAttr;
    VENC_RC_PARAM_S stVEncRcParam;
    PAYLOAD_TYPE_E encode_type;
    int width, height, framerate, bitrate;
    PIXEL_FORMAT_E format = MM_PIXEL_FORMAT_BUTT;
    BOOL success_flag = FALSE;

    if (dual_stream)
    {
        if (context->extra_config.dual_stream_vipp_dev)
            vipp_dev = context->extra_config.dual_stream_vipp_dev;
        else
            vipp_dev = context->extra_config.vipp + 4;
        width = DUAL_STREAM_WIDTH;
        height = DUAL_STREAM_HEIGHT;
        encode_type = DUAL_STREAM_ENCODE_TYPE;
        format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        bitrate = DUAL_STREAM_BITRATE;
        framerate = DUAL_STREAM_FRAMERATE;
    }
    else
    {
        vipp_dev = context->extra_config.vipp;
        width = context->base_config.width;
        height = context->base_config.height;
        if (context->base_config.format == V4L2_PIX_FMT_MJPEG) {
            format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            encode_type = PT_MJPEG;
        } else if (context->base_config.format == V4L2_PIX_FMT_H264) {
            format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            encode_type = PT_H264;
        } else {
            loge("invalid format 0x%x", format);
            return -1;
        }
        bitrate = context->extra_config.bitrate * 1024 * 1024;
        framerate = context->base_config.framerate;
    }
    logd("online %d vipp %d encode %d size %dx%d framerate %d bitrate %d format 0x%x",
        STREAM_ENCODE_ONLINE, vipp_dev, encode_type, width, height, framerate, bitrate, format);

    memset(&stVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&stVEncRcParam, 0, sizeof(VENC_RC_PARAM_S));
    if (STREAM_ENCODE_ONLINE && (vipp_dev == 0))
    {
        stVEncChnAttr.VeAttr.mOnlineEnable = 1;
        stVEncChnAttr.VeAttr.mOnlineShareBufNum = 1;
    }
    stVEncChnAttr.VeAttr.Type         = encode_type;
    stVEncChnAttr.VeAttr.mVippID      = context->extra_config.vipp;
    stVEncChnAttr.VeAttr.SrcPicWidth  = width;
    stVEncChnAttr.VeAttr.SrcPicHeight = height;
    stVEncChnAttr.VeAttr.Field        = VIDEO_FIELD_FRAME;
    stVEncChnAttr.VeAttr.PixelFormat  = format;
    stVEncChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_REC709;
    stVEncChnAttr.VeAttr.Rotate       = ROTATE_NONE;
    stVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = 0;
    stVEncChnAttr.EncppAttr.mbEncppEnable = STREAM_ENCODE_ENCPP;
    stVEncChnAttr.RcAttr.mProductMode = PRODUCT_CDR;
    //stVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (PT_H264 == stVEncChnAttr.VeAttr.Type || PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
        if (framerate)
        {
            vbvThreshSize = bitrate/8/framerate*15;
        }
        vbvBufSize = bitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
        logd("vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);
    }
    vbvThreshSize = 200*1024;
    vbvBufSize = STREAM_VBV_BUF_SIZE;

    if(PT_H264 == stVEncChnAttr.VeAttr.Type || PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
#if VE_IPC_PRODUCT_ROTATING
        stVEncRcParam.EnIFrmMbRcMoveStatusEnable = 1;
        stVEncRcParam.EnIFrmMbRcMoveStatus = 0;
#else
        stVEncRcParam.EnIFrmMbRcMoveStatusEnable = 1;
        stVEncRcParam.EnIFrmMbRcMoveStatus = 3;
#endif
    }

    if (PT_H264 == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        stVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        stVEncChnAttr.VeAttr.AttrH264e.PicWidth = width;
        stVEncChnAttr.VeAttr.AttrH264e.PicHeight = height;
        stVEncChnAttr.VeAttr.AttrH264e.mLevel = 0;
        stVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        stVEncChnAttr.VeAttr.AttrH264e.mThreshSize = vbvThreshSize;
        stVEncChnAttr.VeAttr.AttrH264e.BufSize = vbvBufSize;
        stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
        if (VENC_RC_MODE_H264CBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate = bitrate;
            stVEncChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = framerate;
            stVEncChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = framerate;
            stVEncRcParam.ParamH264Cbr.mMaxQp = 45;
            stVEncRcParam.ParamH264Cbr.mMinQp = 10;
            stVEncRcParam.ParamH264Cbr.mMaxPqp = 45;
            stVEncRcParam.ParamH264Cbr.mMinPqp = 10;
            stVEncRcParam.ParamH264Cbr.mQpInit = 35;
            stVEncRcParam.ParamH264Cbr.mbEnMbQpLimit = 1;
        }
        else if (VENC_RC_MODE_H264VBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = bitrate;
            stVEncChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = framerate;
            stVEncChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = framerate;
            stVEncRcParam.ParamH264Vbr.mMaxQp = 45;
            stVEncRcParam.ParamH264Vbr.mMinQp = 25;
            stVEncRcParam.ParamH264Vbr.mMaxPqp = 45;
            stVEncRcParam.ParamH264Vbr.mMinPqp = 25;
            stVEncRcParam.ParamH264Vbr.mQpInit = 37;
#if VE_IPC_PRODUCT_ROTATING
            stVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
            stVEncRcParam.ParamH264Vbr.mQuality = 10;
#else
            stVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 1;
            stVEncRcParam.ParamH264Vbr.mQuality = 1;
#endif
            stVEncRcParam.ParamH264Vbr.mMovingTh = 20;
            stVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
            stVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
        }
    }
    else if (PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        stVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        stVEncChnAttr.VeAttr.AttrH265e.mPicWidth = width;
        stVEncChnAttr.VeAttr.AttrH265e.mPicHeight = height;
        stVEncChnAttr.VeAttr.AttrH265e.mLevel = 0;
        stVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        stVEncChnAttr.VeAttr.AttrH265e.mThreshSize = vbvThreshSize;
        stVEncChnAttr.VeAttr.AttrH265e.mBufSize = vbvBufSize;
        stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        if (VENC_RC_MODE_H265CBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate = bitrate;
            stVEncChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = framerate;
            stVEncChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = framerate;
            stVEncRcParam.ParamH265Cbr.mMaxQp = 45;
            stVEncRcParam.ParamH265Cbr.mMinQp = 10;
            stVEncRcParam.ParamH265Cbr.mMaxPqp = 45;
            stVEncRcParam.ParamH265Cbr.mMinPqp = 10;
            stVEncRcParam.ParamH265Cbr.mQpInit = 35;
            stVEncRcParam.ParamH265Cbr.mbEnMbQpLimit = 1;
        }
        else if (VENC_RC_MODE_H265VBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = bitrate;
            stVEncChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = framerate;
            stVEncChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = framerate;
            stVEncRcParam.ParamH265Vbr.mMaxQp = 45;
            stVEncRcParam.ParamH265Vbr.mMinQp = 25;
            stVEncRcParam.ParamH265Vbr.mMaxPqp = 45;
            stVEncRcParam.ParamH265Vbr.mMinPqp = 25;
            stVEncRcParam.ParamH265Vbr.mQpInit = 37;
#if VE_IPC_PRODUCT_ROTATING
            stVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
            stVEncRcParam.ParamH265Vbr.mQuality = 10;
#else
            stVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 1;
            stVEncRcParam.ParamH265Vbr.mQuality = 1;
#endif
            stVEncRcParam.ParamH265Vbr.mMovingTh = 20;
            stVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
            stVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
        }
    }
    else if(PT_MJPEG == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame  = TRUE;
        stVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth  = width;
        stVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = height;
        stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        stVEncChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = framerate;
        stVEncChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = framerate;
        stVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = bitrate;
    }
    else
    {
        eRet = FAILURE;
        ve_chn = MM_INVALID_CHN;
        loge("fatal error! unsupport vencoder type[%d]!!\n", stVEncChnAttr.VeAttr.Type);
        goto _create_ve_chn_result;
    }

    configBitsClipParam(&stVEncRcParam);

    ve_chn = 0;
    while (1)
    {
        if (ve_chn >= VENC_MAX_CHN_NUM)
        {
            loge("fatal error! create ve chn fail!");
            break;
        }
        eRet = AW_MPI_VENC_CreateChn(ve_chn, &stVEncChnAttr);
        if (eRet == SUCCESS)
        {
            success_flag = TRUE;
            logd("create venc channel[%d] success!", ve_chn);
            break;
        }
        ve_chn++;
    }
    if (!success_flag)
    {
        eRet = FAILURE;
        loge("set venc channle frame rate failed!! venc_chn[%d]\n", ve_chn);
        goto _create_ve_chn_result;
    }

    /* set framerate in AW_MPI_VENC_CreateChn */
    /*VENC_FRAME_RATE_S stVencFrameRate;
    stVencFrameRate.SrcFrmRate = uvc_config->capture_framerate;
    stVencFrameRate.DstFrmRate = framerate;
    eRet = AW_MPI_VENC_SetFrameRate(ve_chn, &stVencFrameRate);
    if (eRet < 0)
    {
        loge("set venc channle frame rate failed!! venc_chn[%d]\n", ve_chn);
        goto _create_ve_chn_result;
    }*/

    if ((PT_H264 == stVEncChnAttr.VeAttr.Type) || (PT_H265 == stVEncChnAttr.VeAttr.Type))
    {
        AW_MPI_VENC_SetRcParam(ve_chn, &stVEncRcParam);
    }
    if (PT_H264 == stVEncChnAttr.VeAttr.Type)
    {
        VENC_PARAM_H264_VUI_S h264_vui;
        memset(&h264_vui, 0, sizeof(VENC_PARAM_H264_VUI_S));
        AW_MPI_VENC_GetH264Vui(ve_chn, &h264_vui);
        h264_vui.VuiTimeInfo.timing_info_present_flag = 1;
        h264_vui.VuiTimeInfo.fixed_frame_rate_flag = 1;
        h264_vui.VuiTimeInfo.num_units_in_tick = 1000;
        h264_vui.VuiTimeInfo.time_scale = h264_vui.VuiTimeInfo.num_units_in_tick * framerate * 2;
        AW_MPI_VENC_SetH264Vui(ve_chn, &h264_vui);
    }
    else if (PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
        VENC_PARAM_H265_VUI_S h265_vui;
        memset(&h265_vui, 0, sizeof(VENC_PARAM_H265_VUI_S));
        AW_MPI_VENC_GetH265Vui(ve_chn, &h265_vui);
        h265_vui.VuiTimeInfo.timing_info_present_flag = 1;
        h265_vui.VuiTimeInfo.num_units_in_tick = 1000;
        /* Notices: the protocol syntax states that h265 does not need to be multiplied by 2. */
        h265_vui.VuiTimeInfo.time_scale = h265_vui.VuiTimeInfo.num_units_in_tick * framerate;
        h265_vui.VuiTimeInfo.num_ticks_poc_diff_one_minus1 = h265_vui.VuiTimeInfo.num_units_in_tick;
        AW_MPI_VENC_SetH265Vui(ve_chn, &h265_vui);
    }

    if (stVEncChnAttr.VeAttr.Type == PT_H264) {
        //setVenc2Dnr(ve_chn);
        //setVenc3Dnr(ve_chn);
        //setVencSuperFrameCfg(ve_chn, bitrate, framerate);
    }

    MPPCallbackInfo cbInfo;
    cbInfo.callback = (MPPCallbackFuncType)&VencStreamCallBack;
    cbInfo.cookie = (void *)context;
    AW_MPI_VENC_RegisterCallback(ve_chn, &cbInfo);
    logd("create ve chn[%d], encoder type[%d]", ve_chn, stVEncChnAttr.VeAttr.Type);
_create_ve_chn_result:
    if (dual_stream)
        context->dual_stream.ve_chn = ve_chn;
    else
        context->stream.ve_chn = ve_chn;
    return eRet;
}

static int CreateCapture(struct video_source_mpp_context *context)
{
    int ret = 0;
    ERRORTYPE eRet = SUCCESS;

    if (context->extra_config.dual_stream && (context->base_config.format == V4L2_PIX_FMT_MJPEG))
    {
        eRet = createVipp(context, TRUE);
        if (eRet)
        {
            loge("fatal error! create vipp fail!");
            goto _destroy_vipp;
        }
        logd("dual stream vipp %d-%d", context->dual_stream.vipp_dev, context->dual_stream.vipp_chn);
    }

    eRet = createVipp(context, FALSE);
    if (eRet)
    {
        loge("fatal error! create vipp fail!");
        goto _destroy_vipp;
    }
    logd("stream vipp %d-%d", context->stream.vipp_dev, context->stream.vipp_chn);

    if ((context->base_config.format == V4L2_PIX_FMT_MJPEG) || (context->base_config.format == V4L2_PIX_FMT_H264))
    {
        if (context->extra_config.dual_stream && (context->base_config.format == V4L2_PIX_FMT_MJPEG))
        {
            eRet = createVeChn(context, TRUE);
            if (eRet)
            {
                loge("fatal error! create ve chn fail!");
                goto _destroy_vipp;
            }
        }

        eRet = createVeChn(context, FALSE);
        if (eRet)
        {
            loge("fatal error! create ve chn fail!");
            goto _destroy_vipp;
        }

        if (context->extra_config.dual_stream && (context->base_config.format == V4L2_PIX_FMT_MJPEG))
        {
            MPP_CHN_S VIChn = {MOD_ID_VIU, context->dual_stream.vipp_dev, context->dual_stream.vipp_chn};
            MPP_CHN_S VEChn = {MOD_ID_VENC, 0, context->dual_stream.ve_chn};
            eRet = AW_MPI_SYS_Bind(&VIChn, &VEChn);
            if (SUCCESS != eRet)
            {
                loge("fatal error1 bind vi and ve fail!");
                goto _destroy_ve;
            }
        }

        MPP_CHN_S VIChn = {MOD_ID_VIU, context->stream.vipp_dev, context->stream.vipp_chn};
        MPP_CHN_S VEChn = {MOD_ID_VENC, 0, context->stream.ve_chn};
        eRet = AW_MPI_SYS_Bind(&VIChn, &VEChn);
        if (SUCCESS != eRet)
        {
            loge("fatal error1 bind vi and ve fail!");
            goto _destroy_ve;
        }
    }
    else
        context->stream.ve_chn = MM_INVALID_CHN;
    logd("initialize vi and venc success, videv[%d],vichn[%d],vencchn[%d],ispdev[%d]\n",
        context->stream.vipp_dev, context->stream.vipp_chn, \
        context->stream.ve_chn, context->stream.isp_dev);

    return 0;
_destroy_ve:
    if (MM_INVALID_CHN != context->stream.ve_chn)
    {
        AW_MPI_VENC_DestroyChn(context->stream.ve_chn);
        context->stream.ve_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_CHN != context->dual_stream.ve_chn)
    {
        AW_MPI_VENC_DestroyChn(context->dual_stream.ve_chn);
        context->dual_stream.ve_chn = MM_INVALID_CHN;
    }
_destroy_vipp:
    if (MM_INVALID_CHN != context->stream.vipp_chn)
    {
        AW_MPI_VI_DestroyVirChn(context->stream.vipp_dev, context->stream.vipp_chn);
        context->stream.vipp_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_DEV != context->stream.isp_dev)
    {
        AW_MPI_ISP_Stop(context->stream.isp_dev);
        context->stream.isp_dev = MM_INVALID_DEV;
    }
    if (MM_INVALID_DEV != context->stream.vipp_dev)
    {
        AW_MPI_VI_DestroyVipp(context->stream.vipp_dev);
        context->stream.vipp_dev = MM_INVALID_DEV;
    }
    if (MM_INVALID_CHN != context->dual_stream.vipp_chn)
    {
        AW_MPI_VI_DestroyVirChn(context->dual_stream.vipp_dev, context->dual_stream.vipp_chn);
        context->dual_stream.vipp_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_DEV != context->dual_stream.isp_dev)
    {
        AW_MPI_ISP_Stop(context->dual_stream.isp_dev);
        context->dual_stream.isp_dev = MM_INVALID_DEV;
    }
    if (MM_INVALID_DEV != context->dual_stream.vipp_dev)
    {
        AW_MPI_VI_DestroyVipp(context->dual_stream.vipp_dev);
        context->dual_stream.vipp_dev = MM_INVALID_DEV;
    }
_sys_exit:
    //AW_MPI_SYS_Exit();
_exit:
    return eRet;
}

static int DestroyCapture(struct video_source_mpp_context *context)
{
    if (MM_INVALID_CHN != context->stream.ve_chn)
    {
        AW_MPI_VENC_DestroyChn(context->stream.ve_chn);
        context->stream.ve_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_CHN != context->dual_stream.ve_chn)
    {
        AW_MPI_VENC_DestroyChn(context->dual_stream.ve_chn);
        context->dual_stream.ve_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_CHN != context->stream.vipp_chn)
    {
        AW_MPI_VI_DestroyVirChn(context->stream.vipp_dev, context->stream.vipp_chn);
        context->stream.vipp_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_CHN != context->dual_stream.vipp_chn)
    {
        AW_MPI_VI_DestroyVirChn(context->dual_stream.vipp_dev, context->dual_stream.vipp_chn);
        context->dual_stream.vipp_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_DEV != context->dual_stream.vipp_dev)
    {
        AW_MPI_VI_DisableVipp(context->dual_stream.vipp_dev);
        #if ISP_RUN
        if (MM_INVALID_DEV != context->dual_stream.isp_dev)
        {
            AW_MPI_ISP_Stop(context->dual_stream.isp_dev);
        }
        #endif
        AW_MPI_VI_DestroyVipp(context->dual_stream.vipp_dev);
        context->dual_stream.vipp_dev = MM_INVALID_DEV;
        context->dual_stream.isp_dev = MM_INVALID_DEV;
    }
    if (MM_INVALID_DEV != context->stream.vipp_dev)
    {
        AW_MPI_VI_DisableVipp(context->stream.vipp_dev);
        #if ISP_RUN
        if (MM_INVALID_DEV != context->stream.isp_dev)
        {
            AW_MPI_ISP_Stop(context->stream.isp_dev);
        }
        #endif
        AW_MPI_VI_DestroyVipp(context->stream.vipp_dev);
        context->stream.vipp_dev = MM_INVALID_DEV;
        context->stream.isp_dev = MM_INVALID_DEV;
    }
    //AW_MPI_SYS_Exit();
    return 0;
}

static int StartCapture(struct video_source_mpp_context *context)
{
    int ret = 0;
    ERRORTYPE eRet = SUCCESS;
    struct video_source_mpp_stream *stream = &context->stream;
    struct video_source_mpp_stream *dual_stream = &context->dual_stream;

    //mpp_init();

    ret = CreateCapture(context);
    if (ret)
        return ret;

    if (MM_INVALID_CHN != dual_stream->vipp_chn)
    {
        eRet = AW_MPI_VI_EnableVirChn(dual_stream->vipp_dev, dual_stream->vipp_chn);
        if (SUCCESS  != eRet)
        {
            loge("fatal error! vipp[%d] enable vir chn[%d]", dual_stream->vipp_dev, dual_stream->vipp_chn);
            return eRet;
        }
    }
    if (MM_INVALID_CHN != stream->vipp_chn)
    {
        logd("stream vipp %d-%d", stream->vipp_dev, stream->vipp_chn);
        eRet = AW_MPI_VI_EnableVirChn(stream->vipp_dev, stream->vipp_chn);
        if (SUCCESS  != eRet)
        {
            loge("fatal error! vipp[%d] enable vir chn[%d]", stream->vipp_dev, stream->vipp_chn);
            return eRet;
        }
    }
    if (context->extra_config.dual_stream && (MM_INVALID_CHN != dual_stream->ve_chn))
    {
        eRet = AW_MPI_VENC_StartRecvPic(dual_stream->ve_chn);
        if (SUCCESS != eRet)
        {
            loge("fatal error! ve chn[%d] start recive picture fail!", dual_stream->ve_chn);
            return eRet;
        }
    }
    if (MM_INVALID_CHN != stream->ve_chn)
    {
        eRet = AW_MPI_VENC_StartRecvPic(stream->ve_chn);
        if (SUCCESS != eRet)
        {
            loge("fatal error! ve chn[%d] start recive picture fail!", stream->ve_chn);
            return eRet;
        }
    }
    if (context->extra_config.dual_stream && (context->base_config.format == V4L2_PIX_FMT_MJPEG))
    {
        dual_stream->capture_thread_exit_flag = FALSE;
        ret = pthread_create(&dual_stream->capture_thread_trd, NULL, DualStreamCaptureThread, context);
        if (ret < 0) {
            loge("caeate GetVideoFrameThread failed!!\n");
            DestroyCapture(context);
            return ret;
        }
    }
    stream->capture_thread_exit_flag = FALSE;
    ret = pthread_create(&stream->capture_thread_trd, NULL, CaptureThread, context);
    if (ret < 0) {
        loge("caeate GetVideoFrameThread failed!!\n");
        DestroyCapture(context);
        return ret;
    }

    return ret;
}

static int StopCapture(struct video_source_mpp_context *context)
{
    struct video_source_mpp_stream *stream = &context->stream;
    struct video_source_mpp_stream *dual_stream = &context->dual_stream;

    if (stream->capture_thread_running)
    {
        stream->capture_thread_exit_flag = TRUE;
        stream->capture_thread_running = FALSE;
        pthread_join(stream->capture_thread_trd, NULL);
    }
    if (dual_stream->capture_thread_running)
    {
        dual_stream->capture_thread_exit_flag = TRUE;
        dual_stream->capture_thread_running = FALSE;
        pthread_join(dual_stream->capture_thread_trd, NULL);
    }

    if (MM_INVALID_CHN != stream->ve_chn)
    {
        AW_MPI_VENC_StopRecvPic(stream->ve_chn);
    }
    if (MM_INVALID_CHN != dual_stream->ve_chn)
    {
        AW_MPI_VENC_StopRecvPic(dual_stream->ve_chn);
    }
    if (MM_INVALID_CHN != stream->vipp_chn)
    {
        AW_MPI_VI_DisableVirChn(stream->vipp_dev, stream->vipp_chn);
    }
    if (MM_INVALID_CHN != dual_stream->vipp_chn)
    {
        AW_MPI_VI_DisableVirChn(dual_stream->vipp_dev, dual_stream->vipp_chn);
    }
    if (MM_INVALID_CHN != stream->ve_chn)
    {
        AW_MPI_VENC_ResetChn(stream->ve_chn);
        //AW_MPI_VENC_DestroyChn(context->stream.ve_chn);
    }
    if (MM_INVALID_CHN != dual_stream->ve_chn)
    {
        AW_MPI_VENC_ResetChn(dual_stream->ve_chn);
        //AW_MPI_VENC_DestroyChn(context->dual_stream.ve_chn);
    }

    DestroyCapture(context);

    //mpp_deinit();

    return 0;
}

static int video_source_mpp_create(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_mpp_context *context = malloc(sizeof(*context));
    if (!context)
        loge("malloc video souce mpp context fail!");
    memset(context, 0, sizeof(*context));
    context->stream.vipp_dev = MM_INVALID_DEV;
    context->stream.vipp_chn = MM_INVALID_CHN;
    context->stream.ve_chn = MM_INVALID_CHN;
    context->dual_stream.vipp_dev = MM_INVALID_DEV;
    context->dual_stream.vipp_chn = MM_INVALID_CHN;
    context->dual_stream.ve_chn = MM_INVALID_CHN;
    video_src->ops_data = (void *)context;

    return 0;
}

static int video_source_mpp_destroy(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_mpp_context *context = (struct video_source_mpp_context *)video_src->ops_data;

    if (!context)
        return 0;
    free(context);
    context = NULL;
    video_src->ops_data = NULL;

    return 0;
}

static int video_source_mpp_start(void *thiz, struct video_source_base_config *config)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_mpp_context *context = (struct video_source_mpp_context *)video_src->ops_data;

    memcpy(&context->base_config, config, sizeof(*config));

    if (context->extra_config.bufs)
        context->stream.frame_manager.list_entry_num = context->extra_config.bufs;
    else
        context->stream.frame_manager.list_entry_num = 3;
    init_frame_manager(&context->stream.frame_manager);
    if (context->extra_config.dual_stream) {
        if (context->extra_config.dual_stream_bufs)
            context->dual_stream.frame_manager.list_entry_num = context->extra_config.dual_stream_bufs;
        else
            context->dual_stream.frame_manager.list_entry_num = 3;
        init_frame_manager(&context->dual_stream.frame_manager);
    }
    StartCapture(context);

    return 0;
}

static int video_source_mpp_stop(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_mpp_context *context = (struct video_source_mpp_context *)video_src->ops_data;

    StopCapture(context);
    deinit_frame_manager(&context->stream.frame_manager);
    if (context->extra_config.dual_stream)
        deinit_frame_manager(&context->dual_stream.frame_manager);

    return 0;
}

static int video_source_mpp_set_extra_config(void *thiz, struct video_source_extra_config *config)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_mpp_context *context = (struct video_source_mpp_context *)video_src->ops_data;

    memcpy(&context->extra_config, config, sizeof(*config));

    return 0;
}

static struct video_source_frame *video_source_mpp_get_frame(void *thiz)
{
    struct video_source_mpp_frame_node *node = NULL;
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_mpp_context *context = (struct video_source_mpp_context *)video_src->ops_data;

    pthread_mutex_lock(&context->stream.frame_manager.list_lock);
    node = list_first_entry_or_null(&context->stream.frame_manager.ready_list, struct video_source_mpp_frame_node, list);
    if (node)
        list_move_tail(&node->list, &context->stream.frame_manager.using_list);
    pthread_mutex_unlock(&context->stream.frame_manager.list_lock);
    if (node)
        return &node->frame;
    else
        return NULL;
}

static int video_source_mpp_release_frame(void *thiz, struct video_source_frame *video_frame)
{
    int find = 0;
    struct video_source_mpp_frame_node *node, *tmp;
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_mpp_context *context = (struct video_source_mpp_context *)video_src->ops_data;

    pthread_mutex_lock(&context->stream.frame_manager.list_lock);
    list_for_each_entry_safe(node, tmp, &context->stream.frame_manager.using_list, list) {
        if (node->frame.buf_vir_addr == video_frame->buf_vir_addr) {
            find = 1;
            list_move_tail(&node->list, &context->stream.frame_manager.idle_list);
        }
    }
    pthread_mutex_unlock(&context->stream.frame_manager.list_lock);
    if (!find)
        loge("frame %p not find!", video_frame->buf_vir_addr);

    return 0;
}

const struct video_source_ops video_source_mpp_ops = {
    .create = video_source_mpp_create,
    .destroy = video_source_mpp_destroy,
    .start = video_source_mpp_start,
    .stop = video_source_mpp_stop,
    .set_extra_config = video_source_mpp_set_extra_config,
    .get_frame = video_source_mpp_get_frame,
    .release_frame = video_source_mpp_release_frame,
};
