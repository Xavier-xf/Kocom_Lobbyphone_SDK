#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/prctl.h>

#include <ion_mem_alloc.h>
#include <AW_VideoInput_API.h>

#include "base_list.h"
#include "include/rt_awaiisp_common.h"
#include "include/video_source_rt_media.h"
#include "../../../utils/debug/include/debug.h"
#include "../../../utils/option/include/option.h"
#include "../../../utils/g2d/include/g2d_helper.h"
#include "../../video-source/base/include/video_source.h"

#define UVC_STREAM_VBV_BUF_SIZE 2*1024*1024
#define FRAME_BUF_LEN       500*1024
#define MAX_VIDEO_SOURCE_RT_MEDIA_CONTEXT_NUM      16

#define UVC_REQ_BUF_NUM     (5)

/* These two macros can only open one */
#define UVC_SUPPORT_AWAIISP (1)
#define UVC_SUPPORT_MELIS_AWAIISP  (0)

#if UVC_SUPPORT_AWAIISP
#define UVC_AWAIISP_DEV_ID             0
#define UVC_AWAIISP_CFG_BIN_PATH       ""
#define UVC_AWAIISP_LUT_NBG_FILE_PATH  "/lib/preproc_1088.nb"
#define UVC_AWAIISP_NBG_FILE_PATH      "/lib/FW100A12W0S00.nb"
#define UVC_AWAIISP_CFG_BIN_PATH2      ""

#define UVC_DAY_TO_NIGHT_SIGNAL_CNT  30
#define UVC_NIGHT_TO_DAY_SIGNAL_CNT  30
#define UVC_DAY_TO_NIGHT_THRESHOLD   60
#define UVC_NIGHT_TO_DAY_THRESHOLD   200
#endif

#define UVC_STREAM_BUF_NUM  (3)
#define DUAL_STREAM_BUF_NUM (10)

#if UVC_SUPPORT_AWAIISP
#define UVC_FRAMERATE       (10)
#else
#define UVC_FRAMERATE       (30)
#endif

#define DUAL_STREAM_H264_FRAMERATE   (15)
#define DUAL_STREAM_H264_WIDTH       (640)
#define DUAL_STREAM_H264_HEIGHT      (360)

#define SAVE_UVC_STREAM  (0)
#define SAVE_DUAL_STREAM (0)

#ifndef AWALIGN
#define AWALIGN(x,a)  (((x)+(a)-1)&(~((a)-1)))
#endif

static FILE *out_file_0;
#if SAVE_DUAL_STREAM
static FILE *out_file_0;
static FILE *out_file_1;
#endif

struct video_source_rt_media_frame_node {
    struct video_source_frame frame;
    struct list_head list;
};

struct video_source_rt_media_frame_manager {
    int wait_flag;
    sem_t sem_wait;
    int list_entry_num;
    pthread_mutex_t list_lock;
    struct list_head idle_list; //list entry: struct video_source_frame
    struct list_head ready_list;
    struct list_head using_list;
};

struct video_source_rt_media_dual_stream_context {
    int channel;

    int proc_exit;
    int proc_running;
    pthread_t proc_trd;

    struct video_source_rt_media_frame_manager frame_manager;
};

struct video_source_rt_media_context
{
    int channel;

    int g2d_fd;
    G2D_FRAME_INFO_S g2d_proc_frm;

    int exit_flag;
    int proc_exit;
    int proc_running;
    pthread_t proc_trd;

    int last_aiisp_mode;
    int enable_aiisp;
    int mbAiispThreadExitFlag;
    pthread_t aiisp_thread;

    struct video_source_rt_media_frame_manager frame_manager;

    struct video_source_rt_media_dual_stream_context dual_stream;

    struct video_source_base_config base_config;
    struct video_source_extra_config extra_config;

    struct SunxiMemOpsS *memops;
};

struct video_source_rt_media_context_map {
    struct video_source_rt_media_context *context;
};
static struct video_source_rt_media_context_map g_context_map[16];
static int rt_media_init_cnt;

static void rt_media_init()
{
    if (!rt_media_init_cnt) {
        logd("rt media init");
        AWVideoInput_Init();
    }
    rt_media_init_cnt++;
}

static void rt_media_deinit()
{
    rt_media_init_cnt--;
    if (!rt_media_init_cnt) {
        logd("rt media deinit");
        AWVideoInput_DeInit();
    }
    if (rt_media_init_cnt < 0)
        rt_media_init_cnt = 0;
}

static void add_one_context_to_map(struct video_source_rt_media_context *context)
{
    for (int i = 0; i < MAX_VIDEO_SOURCE_RT_MEDIA_CONTEXT_NUM; i++) {
        if (!g_context_map[i].context) {
            g_context_map[i].context = context;
            break;
        }
    }
}

static void remove_one_context_from_map(struct video_source_rt_media_context *context)
{
    int find = 0;
    if (!context) {
        loge("context is null!\n");
        return;
    }
    for (int i = 0; i < MAX_VIDEO_SOURCE_RT_MEDIA_CONTEXT_NUM; i++) {
        if (g_context_map[i].context == context) {
            g_context_map[i].context = NULL;
            find = 1;
            break;
        }
    }
    if (!find)
        loge("not find context %p channel %d\n", context, context->channel);
}

static struct video_source_rt_media_context *find_one_context_from_map(int channel)
{
    for (int i = 0; i < MAX_VIDEO_SOURCE_RT_MEDIA_CONTEXT_NUM; i++) {
        if ((g_context_map[i].context && (g_context_map[i].context->channel == channel))
            || (g_context_map[i].context && (g_context_map[i].context->dual_stream.channel == channel)))
            return g_context_map[i].context;
    }
    return NULL;
}

static void init_frame_manager(struct video_source_rt_media_frame_manager *frame_manager)
{
    INIT_LIST_HEAD(&frame_manager->idle_list);
    INIT_LIST_HEAD(&frame_manager->ready_list);
    INIT_LIST_HEAD(&frame_manager->using_list);
    sem_init(&frame_manager->sem_wait, 0, 0);
    pthread_mutex_init(&frame_manager->list_lock, NULL);
    for (int i = 0; i < frame_manager->list_entry_num; i++) {
        struct video_source_rt_media_frame_node *node = malloc(sizeof(*node)*2);
        if (!node)
            loge("rt-media video source frame node malloc fail!\n");
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

static void deinit_frame_manager(struct video_source_rt_media_frame_manager *frame_manager)
{
    struct video_source_rt_media_frame_node *node, *tmp;
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
    sem_destroy(&frame_manager->sem_wait);
    pthread_mutex_destroy(&frame_manager->list_lock);
}

static void *dual_stream_thread(void *pThreadData)
{
    int ret = 0;
    VencInsertData InsertData;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)pThreadData;
    struct video_source_rt_media_dual_stream_context *stream = &context->dual_stream;

    char ThreadName[32];
    sprintf(ThreadName, "DualStream_%d", context->extra_config.dual_stream_vipp_dev);
    prctl(PR_SET_NAME, (unsigned long)ThreadName, 0, 0, 0);

    //usleep(1000*1000);
    //logd("start ch%d", context->extra_config.dual_stream_vipp_dev);
    //AWVideoInput_Start(context->extra_config.dual_stream_vipp_dev, 1);

    stream->proc_running = 1;
    while (1)
    {
        if (context->exit_flag || stream->proc_exit)
            break;

        pthread_mutex_lock(&stream->frame_manager.list_lock);

        if (list_empty(&stream->frame_manager.ready_list)) {
            pthread_mutex_unlock(&stream->frame_manager.list_lock);
            usleep(10*1000);
            continue;
        }

        struct video_source_rt_media_frame_node *buf;
        buf = list_first_entry(&stream->frame_manager.ready_list, struct video_source_rt_media_frame_node, list);
        if (buf)
        {
            unsigned int data_size = buf->frame.data_len;
            if (RT_MAX_INSERT_FRAME_LEN < data_size)
            {
                logw("data_size %d is more than max_frame_len %d, drop this frame!",
                    data_size, RT_MAX_INSERT_FRAME_LEN);
                buf->frame.data_len = 0;
                list_move_tail(&buf->list, &stream->frame_manager.idle_list);
                pthread_mutex_unlock(&stream->frame_manager.list_lock);
                continue;
            }

            VENC_BUF_STATUS eStatus = BUF_IDLE;
            ret = AWVideoInput_GetInsertDataBufStatus(context->extra_config.vipp, &eStatus);
            if (0 == ret)
            {
                if (BUF_IDLE == eStatus)
                {
                    memset(&InsertData, 0, sizeof(VencInsertData));
                    InsertData.pBuffer = buf->frame.buf_vir_addr;
                    InsertData.nDataLen = data_size;
                    InsertData.nFrameRate = DUAL_STREAM_H264_FRAMERATE;
                    logv("status:%d, buf:%p, len:%d, framerate:%d", InsertData.eStatus,
                        InsertData.pBuffer,InsertData.nDataLen, InsertData.nFrameRate);
                    ret = AWVideoInput_SetInsertData(context->extra_config.vipp, &InsertData);
                    if (0 == ret)
                    {
                        logv("insert is OK");
                        buf->frame.data_len = 0;
                        memset(buf->frame.buf_vir_addr, 0, buf->frame.data_len);
                        list_move_tail(&buf->list, &stream->frame_manager.idle_list);
                    }
                    else
                    {
                        logv("AWVideoInput_SetInsertData failed!");
                    }
                }
                else
                {
                    logv("eStatus:%d is not idle!", eStatus);
                    pthread_mutex_unlock(&stream->frame_manager.list_lock);
                    continue;
                }
            }
            else
            {
                loge("AWVideoInput_GetInsertDataBufStatus failed!");
            }
        }
        else
        {
            logw("buf is NULL!");
        }

        pthread_mutex_unlock(&stream->frame_manager.list_lock);
    }
}

static void dual_stream_video_stream_cb(const AWVideoInput_StreamInfo* stream_info)
{
#if SAVE_DUAL_STREAM
    if(NULL == out_file_1)
    {
        out_file_1 = fopen("/mnt/extsd/uvc_dual_stream.h264", "wb");
        if (NULL == out_file_1)
            loge("fopen /mnt/extsd/uvc_dual_stream.h264 fail");
        else
            logd("/mnt/extsd/uvc_dual_stream.h264 open success");
    }
    if(out_file_1)
    {
        if (stream_info->b_insert_sps_pps && stream_info->sps_pps_size && stream_info->sps_pps_buf)
        {
            fwrite(stream_info->sps_pps_buf, 1, stream_info->sps_pps_size, out_file_1);
            logv("b_insert_sps_pps:%d, sps_pps_buf:%p, sps_pps_size:%d", stream_info->b_insert_sps_pps,
                stream_info->sps_pps_buf, stream_info->sps_pps_size);
            logv("%p %d, %p %d, %p %d", stream_info->data0, stream_info->size0, stream_info->data1,
                stream_info->size1, stream_info->data2, stream_info->size2);
        }
        fwrite(stream_info->data0, 1, stream_info->size0, out_file_1);
        if (stream_info->size1)
            fwrite(stream_info->data1, 1, stream_info->size1, out_file_1);
        if (stream_info->size2)
            fwrite(stream_info->data2, 1, stream_info->size2, out_file_1);
    }
#endif

    struct video_source_rt_media_context *context = find_one_context_from_map(stream_info->channel_id);
    if (!context) {
        loge("channel %d not find context!", stream_info->channel_id);
        return;
    }
    struct video_source_rt_media_dual_stream_context *stream = &context->dual_stream;
    pthread_mutex_lock(&stream->frame_manager.list_lock);
    struct video_source_rt_media_frame_node *pBuf =
        list_first_entry_or_null(&stream->frame_manager.idle_list, struct video_source_rt_media_frame_node, list);
    if (NULL != pBuf)
    {
        if (pBuf->frame.buf_len >= stream_info->size0 + stream_info->size1 + stream_info->size2)
        {
            int data_len = 0;
            int offset = 0;
            if (stream_info->sps_pps_buf && stream_info->sps_pps_size
                && (stream_info->sps_pps_size<=pBuf->frame.buf_len))
            {
                logv("recive sps_pps[%p] len[%d] keyframe[%d]",
                    pBuf->frame.buf_vir_addr,stream_info->sps_pps_size, stream_info->keyframe_flag);
                memcpy(pBuf->frame.buf_vir_addr, stream_info->sps_pps_buf, stream_info->sps_pps_size);
                offset = stream_info->sps_pps_size;
                data_len += stream_info->sps_pps_size;
            }
            if (stream_info->data0 && stream_info->size0 && (offset+stream_info->size0<=pBuf->frame.buf_len))
            {
                memcpy(pBuf->frame.buf_vir_addr + offset, stream_info->data0, stream_info->size0);
                offset += stream_info->size0;
                data_len += stream_info->size0;
            }
            if (stream_info->data1 && stream_info->size1 && (offset+stream_info->size1<=pBuf->frame.buf_len))
            {
                memcpy(pBuf->frame.buf_vir_addr + offset, stream_info->data1, stream_info->size1);
                offset += stream_info->size1;
                data_len += stream_info->size1;
            }
            if (stream_info->data2 && stream_info->size2 && (offset+stream_info->size2<=pBuf->frame.buf_len))
            {
                memcpy(pBuf->frame.buf_vir_addr + offset, stream_info->data2, stream_info->size2);
                data_len += stream_info->size2;
            }
            pBuf->frame.data_len = data_len;
            logv("recive frame[%p] len[%d] keyframe[%d]", pBuf->frame.buf_vir_addr,
                offset + pBuf->frame.data_len, stream_info->keyframe_flag);
            list_move_tail(&pBuf->list, &stream->frame_manager.ready_list);
        }
        else
        {
            logw("fatal error! buf_len %d < data size %d",
                pBuf->frame.data_len, stream_info->size0 + stream_info->size1 + stream_info->size2);
        }
    }
    else
    {
        logw("pBuf is NULL!");
    }
    pthread_mutex_unlock(&stream->frame_manager.list_lock);
}

static int DualStreamStartCapture(struct video_source_rt_media_context *context)
{
    if (!context->extra_config.dual_stream_vipp_dev)
        context->extra_config.dual_stream_vipp_dev = context->extra_config.vipp + 4;

    context->dual_stream.channel = context->extra_config.dual_stream_vipp_dev;
    VideoInputConfig stVideoInputConfig;
    memset(&stVideoInputConfig, 0, sizeof(VideoInputConfig));
    stVideoInputConfig.channelId = context->extra_config.dual_stream_vipp_dev;
    stVideoInputConfig.encodeType = 0;
    stVideoInputConfig.output_mode = OUTPUT_MODE_STREAM;
    stVideoInputConfig.bitrate = 1*1024; //kbps
    stVideoInputConfig.pixelformat = RT_PIXEL_LBC_25X;
    stVideoInputConfig.width = DUAL_STREAM_H264_WIDTH;
    stVideoInputConfig.height = DUAL_STREAM_H264_HEIGHT;
    stVideoInputConfig.dst_width = DUAL_STREAM_H264_WIDTH;
    stVideoInputConfig.dst_height = DUAL_STREAM_H264_HEIGHT;
    stVideoInputConfig.fps = UVC_FRAMERATE;
    stVideoInputConfig.dst_fps = DUAL_STREAM_H264_FRAMERATE;
    stVideoInputConfig.gop = DUAL_STREAM_H264_FRAMERATE * 2;
    stVideoInputConfig.mRcMode = AW_VBR;
    stVideoInputConfig.product_mode = PRODUCT_STATIC_IPC;
    stVideoInputConfig.drop_frame_num = 0;
    stVideoInputConfig.enable_wdr = 0;
    stVideoInputConfig.enable_overlay = 0;
    stVideoInputConfig.demo_start = 0;
    stVideoInputConfig.enable_sharp = 1;
    stVideoInputConfig.breduce_refrecmem = 1;
    stVideoInputConfig.profile = VENC_H264ProfileMain;
    stVideoInputConfig.level = VENC_H264Level51;
    stVideoInputConfig.qp_range.nMaxqp = 50;
    stVideoInputConfig.qp_range.nMinqp = 30;
    stVideoInputConfig.qp_range.nMaxPqp = 50;
    stVideoInputConfig.qp_range.nMinPqp = 30;
    stVideoInputConfig.qp_range.nQpInit = 40;
    stVideoInputConfig.qp_range.bEnMbQpLimit = 0;
    logv("h264 src %dx%d dst %dx%d fps:%d, dst_fps:%d, gop:%d, bitrate:%d \n", stVideoInputConfig.width, stVideoInputConfig.height,
        stVideoInputConfig.dst_width, stVideoInputConfig.dst_height, stVideoInputConfig.fps, stVideoInputConfig.dst_fps, stVideoInputConfig.gop, stVideoInputConfig.bitrate);
    AWVideoInput_Configure(context->extra_config.dual_stream_vipp_dev, &stVideoInputConfig);

    VencSuperFrameConfig mSuperConfig;
    memset(&mSuperConfig, 0, sizeof(mSuperConfig));
    mSuperConfig.eSuperFrameMode = VENC_SUPERFRAME_NONE;
    mSuperConfig.nMaxRencodeTimes = 0;
    mSuperConfig.nMaxIFrameBits  = 150*1024*8; //* 150 KB
    mSuperConfig.nMaxPFrameBits  = mSuperConfig.nMaxIFrameBits / 3;
    mSuperConfig.nMaxP2IFrameBitsRatio = 0.33;
    AWVideoInput_SetSuperFrameParam(context->extra_config.dual_stream_vipp_dev, &mSuperConfig);

    s3DfilterParam m3DnrPara;
    memset(&m3DnrPara, 0, sizeof(m3DnrPara));
    m3DnrPara.enable_3d_filter = 1;
    m3DnrPara.adjust_pix_level_enable = 0;
    m3DnrPara.max_pix_diff_th = 6;
    m3DnrPara.max_mv_th = 5;
    m3DnrPara.max_mad_th = 48;
    m3DnrPara.smooth_filter_enable = 0;
    m3DnrPara.min_coef = 0;
    m3DnrPara.max_coef = 16;
    AWVideoInput_Set3dNR(context->extra_config.dual_stream_vipp_dev, &m3DnrPara);

    VencRegionD3DParam mRegonD3DParam;
    memset(&mRegonD3DParam, 0, sizeof(mRegonD3DParam));
    mRegonD3DParam.en_region_d3d = 1;
    mRegonD3DParam.dis_default_para = 1;
    mRegonD3DParam.result_num = 1; // [1, 5], default is 1
    mRegonD3DParam.chroma_offset = 16;
    mRegonD3DParam.zero_mv_rate_th[0] = 93;
    mRegonD3DParam.zero_mv_rate_th[1] = 87;
    mRegonD3DParam.zero_mv_rate_th[2] = 81;
    mRegonD3DParam.hor_region_num = AWALIGN(DUAL_STREAM_H264_WIDTH, 16) / 64;
    mRegonD3DParam.ver_region_num = AWALIGN(DUAL_STREAM_H264_HEIGHT, 16) / 64;
    mRegonD3DParam.hor_expand_num = 2;
    mRegonD3DParam.ver_expand_num = 2;
    mRegonD3DParam.static_coef[0] = 5;
    mRegonD3DParam.static_coef[1] = 6;
    mRegonD3DParam.static_coef[2] = 7;
    mRegonD3DParam.motion_coef[0] = 13;
    mRegonD3DParam.motion_coef[1] = 14;
    mRegonD3DParam.motion_coef[2] = 15;
    mRegonD3DParam.motion_coef[3] = 16;
    AWVideoInput_SetRegionD3DParam(context->extra_config.dual_stream_vipp_dev, &mRegonD3DParam);

    AWVideoInput_CallBack(context->extra_config.dual_stream_vipp_dev, dual_stream_video_stream_cb, 1);
    AWVideoInput_Start(context->extra_config.dual_stream_vipp_dev, 1);

    context->dual_stream.proc_exit = 0;
    pthread_create(&context->dual_stream.proc_trd, NULL, dual_stream_thread, (void *)context);
}

static void DualStreamStopCapture(struct video_source_rt_media_context *context)
{
    if (context->dual_stream.proc_running)
    {
        context->dual_stream.proc_exit = 1;
        context->dual_stream.proc_running = 0;
        pthread_join(context->dual_stream.proc_trd, NULL);
    }

    AWVideoInput_Start(context->extra_config.dual_stream_vipp_dev, 0);
    logd("stop channel %d", context->extra_config.dual_stream_vipp_dev);
}

static void video_stream_cb(const AWVideoInput_StreamInfo* stream_info)
{
#if SAVE_UVC_STREAM
    if(NULL == out_file_0)
    {
        out_file_0 = fopen("/mnt/extsd/uvc_stream.raw", "wb");
        if (NULL == out_file_0)
            loge("fopen /mnt/extsd/uvc_stream.raw fail\n");
        else
            logd("/mnt/extsd/uvc_stream.raw open success\n");
    }
    if(out_file_0)
    {
        if (stream_info->b_insert_sps_pps && stream_info->sps_pps_size && stream_info->sps_pps_buf)
            fwrite(stream_info->sps_pps_buf, 1, stream_info->sps_pps_size, out_file_0);

        fwrite(stream_info->data0, 1, stream_info->size0, out_file_0);

        if (stream_info->size1)
            fwrite(stream_info->data1, 1, stream_info->size1, out_file_0);
        if (stream_info->size2)
            fwrite(stream_info->data2, 1, stream_info->size2, out_file_0);
    }
#endif
    int data_len = 0, offset = 0;
    struct video_source_rt_media_context *context = find_one_context_from_map(stream_info->channel_id);
    if (!context) {
        loge("not find channel %d!\n", stream_info->channel_id);
        return;
    }
    pthread_mutex_lock(&context->frame_manager.list_lock);
    struct video_source_rt_media_frame_node *node =
        list_first_entry_or_null(&context->frame_manager.idle_list, struct video_source_rt_media_frame_node, list);
    if (node)
    {
        if (node->frame.buf_len >= stream_info->size0 + stream_info->size1 + stream_info->size2)
        {
            if (context->base_config.format == V4L2_PIX_FMT_H264) {
                if (stream_info->keyframe_flag && stream_info->sps_pps_buf && stream_info->sps_pps_buf) {
                    memcpy(node->frame.buf_vir_addr, stream_info->sps_pps_buf, stream_info->sps_pps_size);
                    offset += stream_info->sps_pps_size;
                    data_len += stream_info->sps_pps_size;
                }
            }
            if (stream_info->data0 && stream_info->size0)
            {
                memcpy(node->frame.buf_vir_addr + offset, stream_info->data0, stream_info->size0);
                offset += stream_info->size0;
                data_len += stream_info->size0;
            }
            if (stream_info->data1 && stream_info->size1)
            {
                memcpy(node->frame.buf_vir_addr + offset, stream_info->data1, stream_info->size1);
                offset += stream_info->size1;
                data_len += stream_info->size1;
            }
            if (stream_info->data2 && stream_info->size2)
            {
                memcpy(node->frame.buf_vir_addr + offset, stream_info->data2, stream_info->size2);
                data_len += stream_info->size2;
            }
            node->frame.data_len = data_len;
            logv("recive frame[%p] len[%d] keyframe[%d]\n",
                node->frame.buf_vir_addr, node->frame.buf_len, stream_info->keyframe_flag);
            list_move_tail(&node->list, &context->frame_manager.ready_list);
        }
        else
        {
            logw("fatal error! buf_len %d < data size %d\n",
                node->frame.buf_len, stream_info->size0 + stream_info->size1 + stream_info->size2);
        }
    }
    else
    {
        //logw("pBuf is NULL!\n");
    }
    pthread_mutex_unlock(&context->frame_manager.list_lock);
}

static void *get_yuv_frame_thread(void *pThreadData)
{
    int ret = 0;
    VideoYuvFrame stVideoYuvFrame;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)pThreadData;

    char ThreadName[32];
    sprintf(ThreadName, "getYUV_%d", context->extra_config.vipp);
    prctl(PR_SET_NAME, (unsigned long)ThreadName, 0, 0, 0);

    context->proc_running = 1;
    while (1)
    {
        if (context->exit_flag || context->proc_exit)
            break;

        pthread_mutex_lock(&context->frame_manager.list_lock);
        struct video_source_rt_media_frame_node *pBuf = list_first_entry_or_null(&context->frame_manager.idle_list, struct video_source_rt_media_frame_node, list);
        if (NULL != pBuf)
        {
            memset(&stVideoYuvFrame, 0, sizeof(VideoYuvFrame));
            ret = AWVideoInput_GetYuvFrame(context->extra_config.vipp, &stVideoYuvFrame);
            if (0 == ret)
            {
                if ((context->base_config.format == V4L2_PIX_FMT_YUYV)) {
                    G2D_FRAME_INFO_S stG2DSrcFrm, stG2DDstFrm;
                    memset(&stG2DSrcFrm, 0, sizeof(G2D_FRAME_INFO_S));
                    stG2DSrcFrm.mPixFormat = G2D_FORMAT_YUV420UVC_U1V1U0V0;
                    stG2DSrcFrm.mWidth = stVideoYuvFrame.widht;
                    stG2DSrcFrm.mHeight = stVideoYuvFrame.height;
                    stG2DSrcFrm.mpVirAddr[0] = stVideoYuvFrame.virAddr[0];
                    stG2DSrcFrm.mpVirAddr[1] = stVideoYuvFrame.virAddr[1];
                    stG2DSrcFrm.mpVirAddr[2] = stVideoYuvFrame.virAddr[2];
                    stG2DSrcFrm.mPhyAddr[0] = (unsigned long)(stVideoYuvFrame.phyAddr[0]);
                    stG2DSrcFrm.mPhyAddr[1] = (unsigned long)(stVideoYuvFrame.phyAddr[1]);
                    stG2DSrcFrm.mPhyAddr[2] = (unsigned long)(stVideoYuvFrame.phyAddr[2]);
                    ret = g2d_proc(context->g2d_fd, &stG2DSrcFrm, &context->g2d_proc_frm);
                    if (0 == ret)
                    {
                        int nFrmLen = context->g2d_proc_frm.mWidth*context->g2d_proc_frm.mHeight*2;
                        pBuf->frame.data_len = nFrmLen;
                        memcpy(pBuf->frame.buf_vir_addr, context->g2d_proc_frm.mpVirAddr[0], nFrmLen);
                        list_move_tail(&pBuf->list, &context->frame_manager.ready_list);
                    }
                } else if (context->base_config.format == V4L2_PIX_FMT_NV12) {
                    int nFrmLen = stVideoYuvFrame.widht * stVideoYuvFrame.height;
                    memcpy(pBuf->frame.buf_vir_addr, stVideoYuvFrame.virAddr[0], nFrmLen);
                    memcpy(pBuf->frame.buf_vir_addr + nFrmLen, stVideoYuvFrame.virAddr[1], nFrmLen / 2);
                    pBuf->frame.data_len = nFrmLen * 3 / 2;
                    list_move_tail(&pBuf->list, &context->frame_manager.ready_list);
                }
                AWVideoInput_ReleaseYuvFrame(context->extra_config.vipp, &stVideoYuvFrame);
            }
        }
        pthread_mutex_unlock(&context->frame_manager.list_lock);
    }
}

#if UVC_SUPPORT_AWAIISP
static void* aiisp_switch_thread(void* param)
{
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)param;
    int channel_id = context->extra_config.vipp;
    int loop_cnt = 0;
    int interval_ms = 0;
    awaiisp_mode mode = context->extra_config.aiisp_mode;
    awaiisp_mode last_aiisp_mode = mode;

    if (UVC_FRAMERATE)
        interval_ms = 1000 / UVC_FRAMERATE;
    if (0 == interval_ms)
        interval_ms = 1000;

    int night_to_day_signal_cnt = 0;
    int day_to_night_signal_cnt = 0;
    int env_light_level = 0;

    awaiisp_mode switch_normal_mode =
        (RT_AWAIISP_COMMON_SWITCH_CASE_AIISP_DAY_ALL_8BIT == context->extra_config.aiisp_switch_case)
        ? AWAIISP_MODE_NORMAL_GAMMA : AWAIISP_MODE_NORMAL;
    int nAiIspSwitchReleaseResEnable = context->extra_config.aiisp_switch_release_res_enable;

    while (context->exit_flag)
    {
        if (context->extra_config.aiisp_auto_switch)
        {
            // switch aiisp by ae param
            RTIspCtrlAttr isp_ctrl_attr;
            isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_AE_EV_LV_ADJ;
            AWVideoInput_GetIspAttrCfg(channel_id, &isp_ctrl_attr);
            env_light_level = isp_ctrl_attr.isp_attr_cfg.ae_ev_lv_adj;
            //logw("channel %d, env_light_level:%d, day2nignt:%d, night2day:%d", channel_id, env_light_level, day_to_night_signal_cnt, night_to_day_signal_cnt);

            if (env_light_level < UVC_DAY_TO_NIGHT_THRESHOLD)
            {
                if (++day_to_night_signal_cnt >= UVC_DAY_TO_NIGHT_SIGNAL_CNT)
                {
                    day_to_night_signal_cnt = 0;
                    mode = AWAIISP_MODE_NPU;
                }
                night_to_day_signal_cnt = 0;
            }
            else if (env_light_level > UVC_NIGHT_TO_DAY_THRESHOLD)
            {
                if (++night_to_day_signal_cnt >= UVC_NIGHT_TO_DAY_SIGNAL_CNT)
                {
                    night_to_day_signal_cnt = 0;
                    mode = switch_normal_mode;
                }
                day_to_night_signal_cnt = 0;
            }
            else
            {
                night_to_day_signal_cnt = 0;
                day_to_night_signal_cnt = 0;
            }
            interval_ms = 100;
        }
        else
        {
            // for aiisp switch test by manually specifying interval
            if ((context->extra_config.aiisp_switch_interval) && (0 == (++loop_cnt) % context->extra_config.aiisp_switch_interval))
            {
                mode = (AWAIISP_MODE_NPU == last_aiisp_mode) ? switch_normal_mode : AWAIISP_MODE_NPU;
            }
        }

        if (last_aiisp_mode != mode)
        {
            if (context->extra_config.aiisp_auto_switch)
                logw("isp%d switch mode %d -> %d, env_light_level:%d", UVC_AWAIISP_DEV_ID, last_aiisp_mode, mode, env_light_level);
            else
                logw("isp%d switch mode %d -> %d", UVC_AWAIISP_DEV_ID, last_aiisp_mode, mode);

            rt_awaiisp_common_switch_param switch_param;
            memset(&switch_param, 0, sizeof(rt_awaiisp_common_switch_param));
            switch_param.channel_param[0].enable = 1;
            switch_param.channel_param[0].isp = UVC_AWAIISP_DEV_ID;
            switch_param.channel_param[0].config.mode = mode;
            switch_param.channel_param[0].vipp = channel_id;
            switch_param.channel_param[0].switch_case = context->extra_config.aiisp_switch_case;
            switch_param.channel_param[0].drop_frame_num = context->extra_config.tdm_drop_frame;
            logd("isp%d switch mode %d channel %d switch case %d drop frame %d", switch_param.channel_param[0].isp,
                switch_param.channel_param[0].config.mode, switch_param.channel_param[0].vipp, switch_param.channel_param[0].switch_case,
                switch_param.channel_param[0].drop_frame_num);
            if (AWAIISP_MODE_NPU == mode)
            {
                switch_param.channel_param[0].isp_cfg_bin_path = context->extra_config.isp_aiisp_bin_path;
                switch_param.channel_param[0].config.release_aiisp_resources = 0;
            }
            else
            {
                switch_param.channel_param[0].isp_cfg_bin_path = context->extra_config.isp_day_bin_path;
                switch_param.channel_param[0].config.release_aiisp_resources = nAiIspSwitchReleaseResEnable;
            }
            rt_awaiisp_common_switch_mode(&switch_param);

            last_aiisp_mode = mode;
        }

        usleep(interval_ms * 1000);
    }

    return NULL;
}
#endif

#if UVC_SUPPORT_MELIS_AWAIISP
void enable_aiisp(int vipp_dev)
{
	RT_AWAIISP_CONFIG awaiisp_common_config;

	logd("AWVideoInput_EnableAWAiIsp\n");
	memset(&awaiisp_common_config, 0, sizeof(awaiisp_common_config));
	awaiisp_common_config.width = 1920;
	awaiisp_common_config.height = 1088;
	awaiisp_common_config.tdm_rxbuf_cnt = 5;
	awaiisp_common_config.ion_mem_open = 0;
	awaiisp_common_config.mode = 0;//AWAIISP_MODE_NPU
	awaiisp_common_config.unprepared_aiisp_resources_advance = 0;
	awaiisp_common_config.npu_ref_buf_reduce_enable = 0;
	AWVideoInput_EnableAWAiIsp(vipp_dev, &awaiisp_common_config);
}

void disable_aiisp(int vipp_dev)
{
	RT_AWAIISP_CONFIG awaiisp_common_config;

	logd("AWVideoInput_DisableAWAiIsp\n");
	AWVideoInput_DisableAWAiIsp(vipp_dev);
}
#endif

static int StartCapture(struct video_source_rt_media_context *context)
{
    int ret = 0;
    int nEncType = 0;

    if ((context->extra_config.dual_stream) && (context->base_config.format != V4L2_PIX_FMT_MJPEG)) {
        logw("DualStreamPkg only for MJPEG, %d is not support, reset mEnableDualStreamPkg=0",
            context->base_config.format);
        context->extra_config.dual_stream = 0;
    }

    //rt_media_init();

    context->channel = context->extra_config.vipp;
    VideoInputConfig stVideoInputConfig;
    memset(&stVideoInputConfig, 0, sizeof(VideoInputConfig));
    stVideoInputConfig.channelId = context->extra_config.vipp;
    switch (context->base_config.format)
    {
        case V4L2_PIX_FMT_H264:
            stVideoInputConfig.encodeType = 0;
            stVideoInputConfig.output_mode = OUTPUT_MODE_STREAM;
            stVideoInputConfig.bitrate = context->extra_config.bitrate*1024*1024;
            stVideoInputConfig.pixelformat = RT_PIXEL_LBC_25X;
            stVideoInputConfig.mRcMode = AW_VBR;
            stVideoInputConfig.vbr_opt_enable = 1;
            stVideoInputConfig.profile = VENC_H264ProfileMain;
            stVideoInputConfig.level = VENC_H264Level51;
            stVideoInputConfig.qp_range.nMinqp = 25;
            stVideoInputConfig.qp_range.nMaxqp = 45;
            stVideoInputConfig.qp_range.nMinPqp = 25;
            stVideoInputConfig.qp_range.nMaxPqp = 45;
            stVideoInputConfig.qp_range.nQpInit = 37;
            stVideoInputConfig.qp_range.bEnMbQpLimit = 0;
            stVideoInputConfig.product_mode = PRODUCT_STATIC_IPC;
            break;
        case V4L2_PIX_FMT_MJPEG:
            stVideoInputConfig.encodeType = 1;
            stVideoInputConfig.jpg_quality = 30;
            stVideoInputConfig.jpg_mode = 1;
            stVideoInputConfig.bitrate = context->extra_config.bitrate*1024*1024;
            stVideoInputConfig.bit_rate_range.bitRateMin = context->extra_config.bitrate*1024*1024;
            stVideoInputConfig.bit_rate_range.bitRateMax = context->extra_config.bitrate*1024*1024;
	    stVideoInputConfig.bit_rate_range.fRangeRatioTh = 0;
	    stVideoInputConfig.bit_rate_range.nQualityTh = 0;
	    stVideoInputConfig.bit_rate_range.nQualityStep[0] = 5;
	    stVideoInputConfig.bit_rate_range.nQualityStep[1] = 5;
	    stVideoInputConfig.bit_rate_range.nSubQualityDelay = 0;
	    stVideoInputConfig.bit_rate_range.nAddQualityDelay = 5;
	    stVideoInputConfig.bit_rate_range.nMinQuality = 20;
	    stVideoInputConfig.bit_rate_range.nMaxQuality = 80;
            stVideoInputConfig.output_mode = OUTPUT_MODE_STREAM;
            stVideoInputConfig.pixelformat = RT_PIXEL_LBC_25X;
	    stVideoInputConfig.product_mode = PRODUCT_CDR;
            break;
        case V4L2_PIX_FMT_YUYV:
            stVideoInputConfig.encodeType = 0;
            stVideoInputConfig.output_mode = OUTPUT_MODE_YUV;
            stVideoInputConfig.bitrate = 0;
            stVideoInputConfig.pixelformat = RT_PIXEL_YUV420SP;
            break;
	case V4L2_PIX_FMT_NV12:
            stVideoInputConfig.encodeType = 0;
            stVideoInputConfig.output_mode = OUTPUT_MODE_YUV;
            stVideoInputConfig.bitrate = 0;
            stVideoInputConfig.pixelformat = RT_PIXEL_YUV420SP;
            break;
        default:
            logw("unsupport encode type[0x%x], use default MJPEG\n", context->base_config.format);
            stVideoInputConfig.encodeType = 1;
            stVideoInputConfig.output_mode = OUTPUT_MODE_STREAM;
            break;
    }
#if UVC_SUPPORT_AWAIISP
    int frame_rate;
    if (context->extra_config.enable_aiisp)
        frame_rate = UVC_FRAMERATE;
    else
        frame_rate = context->base_config.framerate;
#else
    int frame_rate = context->base_config.framerate;
#endif
    stVideoInputConfig.fps = frame_rate;
    stVideoInputConfig.gop = context->base_config.framerate;
    stVideoInputConfig.drop_frame_num = 0;
    stVideoInputConfig.enable_wdr = 0;
    stVideoInputConfig.enable_overlay = 0;
    stVideoInputConfig.demo_start = 0;
    stVideoInputConfig.enable_sharp = 1;
    stVideoInputConfig.breduce_refrecmem = 0;
    stVideoInputConfig.width = context->base_config.width;
    stVideoInputConfig.height = context->base_config.height;
    stVideoInputConfig.dst_width = context->base_config.width;
    stVideoInputConfig.dst_height = context->base_config.height;
    logv("encpp:%d, %dx%d -> %dx%d\n", stVideoInputConfig.enable_sharp, stVideoInputConfig.width,
        stVideoInputConfig.height, stVideoInputConfig.dst_width, stVideoInputConfig.dst_height);
    stVideoInputConfig.enable_ve2isp_linkage = 1;
    stVideoInputConfig.enable_isp2ve_linkage = 1;
    stVideoInputConfig.vbv_buf_size = UVC_STREAM_VBV_BUF_SIZE;
    stVideoInputConfig.vbv_thresh_size = UVC_STREAM_VBV_BUF_SIZE / 3;
    stVideoInputConfig.venc_video_signal.full_range_flag = 1;
    stVideoInputConfig.venc_video_signal.src_colour_primaries = VENC_BT709;
    stVideoInputConfig.venc_video_signal.dst_colour_primaries = VENC_BT709;
    if (context->extra_config.enable_aiisp) {
        stVideoInputConfig.enable_aiisp = context->extra_config.enable_aiisp;
        stVideoInputConfig.tdm_rxbuf_cnt = context->extra_config.tdm_rxbuf_cnt;
    }
    logd("enable aiisp %d tdm buf cnt %d", stVideoInputConfig.enable_aiisp, stVideoInputConfig.tdm_rxbuf_cnt);
    AWVideoInput_Configure(context->channel, &stVideoInputConfig);

    if (OUTPUT_MODE_STREAM == stVideoInputConfig.output_mode)
    {
        AWVideoInput_CallBack(context->channel, video_stream_cb, 1);
    }

    AWVideoInput_Start(context->channel, 1);

#if UVC_SUPPORT_MELIS_AWAIISP
    enable_aiisp(context->extra_config.vipp);
#endif

#if UVC_SUPPORT_AWAIISP
	if (context->extra_config.enable_aiisp)
	{
            if (context->extra_config.npu_ref_buf_reduce_enable) {
                // must set ulimit fd first if enable aiisp buf reduce.
                rt_awaiisp_common_set_ulimit_fd(4096);
		logw("rt_awaiisp_common_set_ulimit_fd to 4096 ");
            }
	    if (strlen(context->extra_config.isp_aiisp_bin_path) == 0) {
		    strncpy(context->extra_config.isp_aiisp_bin_path, UVC_AWAIISP_CFG_BIN_PATH, UVC_OPTIONS_FILE_PATH_MAX_LEN);
	    }
	    if (strlen(context->extra_config.isp_day_bin_path) == 0) {
		    strncpy(context->extra_config.isp_day_bin_path, UVC_AWAIISP_CFG_BIN_PATH2, UVC_OPTIONS_FILE_PATH_MAX_LEN);
	    }
	    if (strlen(context->extra_config.npu_lut_model_file_path) == 0) {
		    strncpy(context->extra_config.npu_lut_model_file_path, UVC_AWAIISP_LUT_NBG_FILE_PATH, UVC_OPTIONS_FILE_PATH_MAX_LEN);
	    }
	    if (strlen(context->extra_config.npu_model_file_path) == 0) {
		    strncpy(context->extra_config.npu_model_file_path, UVC_AWAIISP_NBG_FILE_PATH, UVC_OPTIONS_FILE_PATH_MAX_LEN);
	    }
	    rt_awaiisp_common_config_param common_param;
	    memset(&common_param, 0, sizeof(rt_awaiisp_common_config_param));
	    common_param.isp_cfg_bin_path = (AWAIISP_MODE_NPU == context->extra_config.aiisp_mode) ? context->extra_config.isp_aiisp_bin_path: context->extra_config.isp_day_bin_path;
	    strncpy(common_param.config.lut_model_file, context->extra_config.npu_lut_model_file_path, AWAIISP_FILE_PATH_MAX);
	    strncpy(common_param.config.model_file, context->extra_config.npu_model_file_path, AWAIISP_FILE_PATH_MAX);
	    common_param.config.width = stVideoInputConfig.width;
	    common_param.config.height = stVideoInputConfig.height;
	    common_param.config.tdm_rxbuf_cnt = stVideoInputConfig.tdm_rxbuf_cnt;
	    common_param.config.npu_ref_buf_reduce_enable = context->extra_config.npu_ref_buf_reduce_enable;
	    common_param.config.mode = context->extra_config.aiisp_mode;
	    if (AWAIISP_MODE_NPU == context->extra_config.aiisp_mode)
	    {
		common_param.config.unprepared_aiisp_resources_advance = 0;
	    }
	    else
	    {
		/**
		  decide whether to prepare aiisp resources in advance for certain scenarios.
		  If the memory resources are sufficient, it is recommended to prepare aiisp resources in advance.
		  If no switching test is conducted, there is no need to prepare.
		*/
		if (context->extra_config.aiisp_auto_switch || context->extra_config.aiisp_switch_interval)
		    common_param.config.unprepared_aiisp_resources_advance = 0;
		else
		    common_param.config.unprepared_aiisp_resources_advance = 1;
	    }
	    rt_awaiisp_common_enable(UVC_AWAIISP_DEV_ID, &common_param);

	    if (context->extra_config.aiisp_auto_switch || context->extra_config.aiisp_switch_interval)
	    {
		context->aiisp_thread = 0;
		pthread_create(&context->aiisp_thread, NULL, aiisp_switch_thread, (void*)context);
	    }
	}
#endif

    VideoChannelInfo stVideoChnInfo;
    memset(&stVideoChnInfo, 0, sizeof(VideoChannelInfo));
    AWVideoInput_GetChannelInfo(context->channel, &stVideoChnInfo);
    logd("chn[%d] encodeType[%d] size[%dx%d] frameRate[%d] bitRate[%d]\n", \
        stVideoChnInfo.mConfig.channelId, stVideoChnInfo.mConfig.encodeType, stVideoChnInfo.mConfig.width, \
        stVideoChnInfo.mConfig.height, stVideoChnInfo.mConfig.fps, stVideoChnInfo.mConfig.bitrate);

    if ((context->base_config.format == V4L2_PIX_FMT_YUYV) || (context->base_config.format == V4L2_PIX_FMT_NV12))
    {
        if (context->base_config.format == V4L2_PIX_FMT_YUYV) {
            context->memops = GetMemAdapterOpsS();
            ret = SunxiMemOpen(context->memops);
            if (ret) {
                    loge("fatal error! sunxi mem open fail[%d]!", ret);
            }
            int nFrmLen = context->base_config.width*context->base_config.height*2;
            context->g2d_proc_frm.mPixFormat = G2D_FORMAT_IYUV422_U0Y1V0Y0;
            context->g2d_proc_frm.mWidth = context->base_config.width;
            context->g2d_proc_frm.mHeight = context->base_config.height;
            context->g2d_proc_frm.mpVirAddr[0] = SunxiMemPalloc(context->memops, nFrmLen);
            context->g2d_proc_frm.mPhyAddr[0] =
            (unsigned long)(SunxiMemGetPhysicAddressCpu(context->memops, context->g2d_proc_frm.mpVirAddr[0]));
            context->g2d_fd = g2d_open();
        }
        context->proc_exit = 0;
        pthread_create(&context->proc_trd, NULL, get_yuv_frame_thread, (void *)context);
    }

    return ret;
}

static int StopCapture(struct video_source_rt_media_context *context)
{
    int ret = 0;

    logd("stop channel %d", context->channel);
    if ((context->base_config.format == V4L2_PIX_FMT_YUYV) || (context->base_config.format == V4L2_PIX_FMT_NV12))
    {
        context->proc_exit = 1;
        context->proc_running = 0;
        pthread_join(context->proc_trd, NULL);
        if (context->base_config.format == V4L2_PIX_FMT_YUYV) {
            logd("stop channel %d", context->channel);
            SunxiMemPfree(context->memops, context->g2d_proc_frm.mpVirAddr[0]);
            context->g2d_proc_frm.mpVirAddr[0] = NULL;
            context->g2d_proc_frm.mPhyAddr[0] = 0;
            SunxiMemClose(context->memops);
            g2d_close(context->g2d_fd);
        }
    }

#if UVC_SUPPORT_AWAIISP
    if (context->extra_config.enable_aiisp)
    {
        logd("stop channel %d", context->channel);
        context->extra_config.enable_aiisp = 0;
        rt_awaiisp_common_disable(UVC_AWAIISP_DEV_ID);
        if(context->aiisp_thread != 0)
            pthread_join(context->aiisp_thread, (void**)&ret);
    }
#endif

#if UVC_SUPPORT_MELIS_AWAIISP
    disable_aiisp(context->extra_config.vipp);
#endif

    AWVideoInput_Start(context->channel, 0);
    AWVideoInput_Destroy(context->channel);
    //rt_media_deinit();
#if SAVE_UVC_STREAM
    if (out_file_0) {
        fclose(out_file_0);
        out_file_0 = NULL;
    }
    if (out_file_1) {
        fclose(out_file_1);
        out_file_1 = NULL;
    }
#endif
    return 0;
}

static int video_source_rt_media_create(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = malloc(sizeof(*context));
    if (!context)
        loge("rt-media video source context malloc fail!\n");
    memset(context, 0, sizeof(*context));
    context->channel = -1;
    context->dual_stream.channel = -1;
    add_one_context_to_map(context);
    video_src->ops_data = (void *)context;
}

static int video_source_rt_media_destroy(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    if (!context)
	    return 0;
    remove_one_context_from_map(context);
    free(context);
    context = NULL;
    return 0;
}

static int video_source_rt_media_start(void *thiz, struct video_source_base_config *config)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    memcpy(&context->base_config, config, sizeof(*config));

    if (context->extra_config.bufs)
        context->frame_manager.list_entry_num = context->extra_config.bufs;
    else
        context->frame_manager.list_entry_num = 3;
    init_frame_manager(&context->frame_manager);
    StartCapture(context);
    if (context->extra_config.dual_stream) {
        if (context->extra_config.dual_stream_bufs)
            context->dual_stream.frame_manager.list_entry_num = context->extra_config.dual_stream_bufs;
        else
            context->dual_stream.frame_manager.list_entry_num = 3;
        init_frame_manager(&context->dual_stream.frame_manager);
        DualStreamStartCapture(context);
    }
}

static int video_source_rt_media_set_extra_config(void *thiz, struct video_source_extra_config *config)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    memcpy(&context->extra_config, config, sizeof(*config));
}

static int video_source_rt_media_stop(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    if (!context)
        return 0;
    if (context->extra_config.dual_stream) {
        DualStreamStopCapture(context);
        deinit_frame_manager(&context->dual_stream.frame_manager);
    }
    StopCapture(context);
    deinit_frame_manager(&context->frame_manager);
}

static struct video_source_frame *video_source_rt_media_get_frame(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;
    struct video_source_rt_media_frame_node *node = NULL;

    pthread_mutex_lock(&context->frame_manager.list_lock);
    node = list_first_entry_or_null(&context->frame_manager.ready_list, struct video_source_rt_media_frame_node, list);
    if (node)
        list_move_tail(&node->list, &context->frame_manager.using_list);
    pthread_mutex_unlock(&context->frame_manager.list_lock);
    if (node)
        return &node->frame;
    else
        return NULL;
}

static int video_source_rt_media_release_frame(void *thiz, struct video_source_frame *video_frame)
{
    int find = 0;
    struct video_source_rt_media_frame_node *node, *tmp;
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    pthread_mutex_lock(&context->frame_manager.list_lock);
    list_for_each_entry_safe(node, tmp, &context->frame_manager.using_list, list) {
        if (node->frame.buf_vir_addr == video_frame->buf_vir_addr) {
            list_move_tail(&node->list, &context->frame_manager.idle_list);
            find = 1;
        }
    }
    if (!find)
        loge("frame %p not find!\n", video_frame->buf_vir_addr);
    pthread_mutex_unlock(&context->frame_manager.list_lock);
}

const struct video_source_ops video_source_rt_media_ops = {
    .create = video_source_rt_media_create,
    .destroy = video_source_rt_media_destroy,
    .start = video_source_rt_media_start,
    .stop = video_source_rt_media_stop,
    .set_extra_config = video_source_rt_media_set_extra_config,
    .get_frame = video_source_rt_media_get_frame,
    .release_frame = video_source_rt_media_release_frame,
};
