#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/prctl.h>

#include "ion_mem_alloc.h"
#include "base_list.h"
#include <rt_media/AW_VideoInput_API.h>
#include "../base/include/video_source.h"
#include "../../utils/include/debug.h"

#define FRAME_BUF_NUM                              (1)
#define FRAME_BUF_LEN                              (1280*720*3/2)
#define MAX_VIDEO_SOURCE_RT_MEDIA_CONTEXT_NUM      (16)
#define STREAM_VBV_BUF_SIZE                        (2*1024*1024)
#define STREAM_VBV_THRESH_SIZE                     (100*1024)
#define STREAM_BUF_NUM                             (3)

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
    struct SunxiMemOpsS *mem_ops;
};

struct video_source_rt_media_context
{
    int channel;

    int exit_flag;
    int proc_exit;
    int proc_running;
    pthread_t proc_trd;

    pthread_cond_t condition;
    pthread_mutex_t mutex;

    int pause;
    pthread_mutex_t pause_lock;

    struct video_source_rt_media_frame_manager frame_manager;
    struct video_source_base_config base_config;
    struct video_source_extra_config extra_config;
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
        if (g_context_map[i].context && (g_context_map[i].context->channel == channel))
            return g_context_map[i].context;
    }
    return NULL;
}

static void init_frame_manager(struct video_source_rt_media_frame_manager *frame_manager)
{
    frame_manager->mem_ops = GetMemAdapterOpsS();
    INIT_LIST_HEAD(&frame_manager->idle_list);
    INIT_LIST_HEAD(&frame_manager->ready_list);
    INIT_LIST_HEAD(&frame_manager->using_list);
    sem_init(&frame_manager->sem_wait, 0, 0);
    pthread_mutex_init(&frame_manager->list_lock, NULL);
    for (int i = 0; i < frame_manager->list_entry_num; i++) {
        struct video_source_rt_media_frame_node *node = malloc(sizeof(*node)*2);
        if (!node)
            loge("rt-media video source frame node malloc fail!\n");
        list_add_tail(&node->list, &frame_manager->idle_list);
    }
}

static void deinit_frame_manager(struct video_source_rt_media_frame_manager *frame_manager)
{
    struct video_source_rt_media_frame_node *node, *tmp;
    list_for_each_entry_safe(node, tmp, &frame_manager->using_list, list) {
        list_del(&node->list);
        if (node) {
            free(node);
            node = NULL;
        }
    }
    list_for_each_entry_safe(node, tmp, &frame_manager->ready_list, list) {
        list_del(&node->list);
        if (node) {
            free(node);
            node = NULL;
        }
    }
    list_for_each_entry_safe(node, tmp, &frame_manager->idle_list, list) {
        list_del(&node->list);
        if (node) {
            free(node);
            node = NULL;
        }
    }
    sem_destroy(&frame_manager->sem_wait);
    pthread_mutex_destroy(&frame_manager->list_lock);
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

	pthread_mutex_lock(&context->mutex);
        if (context->pause) {
            pthread_cond_wait(&context->condition, &context->mutex);
        }
	pthread_mutex_unlock(&context->mutex);

        pthread_mutex_lock(&context->frame_manager.list_lock);
        struct video_source_rt_media_frame_node *pBuf = list_first_entry_or_null(&context->frame_manager.idle_list, struct video_source_rt_media_frame_node, list);
        if (NULL != pBuf)
        {
            memset(&stVideoYuvFrame, 0, sizeof(VideoYuvFrame));
            ret = AWVideoInput_GetYuvFrame(context->extra_config.vipp, &stVideoYuvFrame);
            if (0 == ret)
            {
                if (context->base_config.format == V4L2_PIX_FMT_YUYV) {
                    int len = stVideoYuvFrame.widht * stVideoYuvFrame.height;
                    int i = 0, j = 0, k = 0;
                    unsigned char *nv16_y_dat = stVideoYuvFrame.virAddr[0];
                    unsigned char *nv16_c_dat = stVideoYuvFrame.virAddr[1];
                    unsigned char *yuyv_dat = pBuf->frame.buf_vir_addr;
                    for (i = 0; i < len*2; i++) {
                        if (i % 2 == 0)
                            yuyv_dat[i] = nv16_y_dat[j++];
                        else
                            yuyv_dat[i] = nv16_c_dat[k++];
                    }
                    pBuf->frame.data_len = len*2;
                } else if (context->base_config.format == V4L2_PIX_FMT_NV12) {
                    int len = stVideoYuvFrame.widht * stVideoYuvFrame.height;
                    pBuf->frame.width = stVideoYuvFrame.widht;
                    pBuf->frame.height = stVideoYuvFrame.height;
                    memcpy(pBuf->frame.buf_vir_addr, stVideoYuvFrame.virAddr[0], len);
                    memcpy(pBuf->frame.buf_vir_addr + len, stVideoYuvFrame.virAddr[1], len / 2);
                    pBuf->frame.data_len = len*3/2;
                } else
                    loge("invalid foramt 0x%x!", context->base_config.format);
                list_move_tail(&pBuf->list, &context->frame_manager.ready_list);
                AWVideoInput_ReleaseYuvFrame(context->extra_config.vipp, &stVideoYuvFrame);
            }
        }

        pthread_mutex_unlock(&context->frame_manager.list_lock);
    }
    return (void *)NULL;
}

static int StartCapture(struct video_source_rt_media_context *context)
{
    int ret = 0;
    int nEncType = 0;

    context->channel = context->extra_config.vipp;
    VideoInputConfig stVideoInputConfig;
    memset(&stVideoInputConfig, 0, sizeof(VideoInputConfig));
    stVideoInputConfig.channelId = context->extra_config.vipp;
    switch (context->base_config.format)
    {
        case V4L2_PIX_FMT_H264:
            stVideoInputConfig.encodeType = 0;
            stVideoInputConfig.output_mode = OUTPUT_MODE_STREAM;
            stVideoInputConfig.bitrate = context->extra_config.bitrate*1024;
            if ((stVideoInputConfig.channelId == 0) || (stVideoInputConfig.channelId == 1))
	        stVideoInputConfig.pixelformat = RT_PIXEL_LBC_25X;
            else
                stVideoInputConfig.pixelformat = RT_PIXEL_YUV420SP;
            break;
        case V4L2_PIX_FMT_MJPEG:
            stVideoInputConfig.encodeType = 1;
            stVideoInputConfig.jpg_quality = 30;
            stVideoInputConfig.jpg_mode = 1;
            stVideoInputConfig.bitrate = context->extra_config.bitrate*1024;
            stVideoInputConfig.bit_rate_range.bitRateMin = context->extra_config.bitrate*1024;
            stVideoInputConfig.bit_rate_range.bitRateMax = context->extra_config.bitrate*1024;
            stVideoInputConfig.bit_rate_range.fRangeRatioTh = 0;
            stVideoInputConfig.bit_rate_range.nQualityTh = 0;
            stVideoInputConfig.bit_rate_range.nQualityStep[0] = 5;
            stVideoInputConfig.bit_rate_range.nQualityStep[1] = 5;
            stVideoInputConfig.bit_rate_range.nSubQualityDelay = 0;
            stVideoInputConfig.bit_rate_range.nAddQualityDelay = 5;
            stVideoInputConfig.bit_rate_range.nMinQuality = 20;
            stVideoInputConfig.bit_rate_range.nMaxQuality = 80;
            stVideoInputConfig.output_mode = OUTPUT_MODE_STREAM;
            if ((stVideoInputConfig.channelId == 0) || (stVideoInputConfig.channelId == 1))
                stVideoInputConfig.pixelformat = RT_PIXEL_LBC_25X;
            else
                stVideoInputConfig.pixelformat = RT_PIXEL_YUV420SP;
            break;
        case V4L2_PIX_FMT_YUYV:
            stVideoInputConfig.encodeType = 0;
            stVideoInputConfig.output_mode = OUTPUT_MODE_YUV;
            stVideoInputConfig.bitrate = 0;
            stVideoInputConfig.pixelformat = RT_PIXEL_YUV422SP;
            break;
        case V4L2_PIX_FMT_NV12:
            stVideoInputConfig.encodeType = 0;
            stVideoInputConfig.output_mode = OUTPUT_MODE_YUV_GET_DIRECT;
            stVideoInputConfig.bitrate = 0;
            stVideoInputConfig.pixelformat = RT_PIXEL_YUV420SP;
            break;
        case V4L2_PIX_FMT_NV21:
            stVideoInputConfig.encodeType = 0;
            stVideoInputConfig.output_mode = OUTPUT_MODE_YUV_GET_DIRECT;
            stVideoInputConfig.bitrate = 0;
            stVideoInputConfig.pixelformat = RT_PIXEL_YVU420SP;
            break;
        default:
            logw("unsupport encode type[0x%x], use default MJPEG\n", context->base_config.format);
            stVideoInputConfig.encodeType = 1;
            stVideoInputConfig.output_mode = OUTPUT_MODE_STREAM;
            break;
    }
    stVideoInputConfig.fps = context->base_config.framerate;
    stVideoInputConfig.gop = context->base_config.framerate;
    stVideoInputConfig.vin_buf_num = 5;
    stVideoInputConfig.mRcMode = AW_VBR;
    stVideoInputConfig.product_mode = PRODUCT_DOORBELL;
    stVideoInputConfig.drop_frame_num = 0;
    stVideoInputConfig.enable_wdr = 0;
    stVideoInputConfig.enable_overlay = 0;
    stVideoInputConfig.demo_start = 0;
    stVideoInputConfig.enable_sharp = 1;
    stVideoInputConfig.breduce_refrecmem = 0;
    if (stVideoInputConfig.enable_sharp)
    {
        stVideoInputConfig.width = context->base_config.width;
        stVideoInputConfig.height = context->base_config.height;
    }
    else
    {
        stVideoInputConfig.width = context->base_config.width;
        stVideoInputConfig.height = context->base_config.height;
    }
    stVideoInputConfig.dst_width = context->base_config.width;
    stVideoInputConfig.dst_height = context->base_config.height;
    logv("encpp:%d, %dx%d -> %dx%d\n", stVideoInputConfig.enable_sharp, stVideoInputConfig.width,
        stVideoInputConfig.height, stVideoInputConfig.dst_width, stVideoInputConfig.dst_height);
    if (0 == stVideoInputConfig.encodeType && stVideoInputConfig.output_mode == OUTPUT_MODE_STREAM) // h264
    {
        stVideoInputConfig.profile = VENC_H264ProfileMain;
        stVideoInputConfig.level = VENC_H264Level51;
        stVideoInputConfig.qp_range.nMinqp = 25;
        stVideoInputConfig.qp_range.nMaxqp = 45;
        stVideoInputConfig.qp_range.nMinPqp = 25;
        stVideoInputConfig.qp_range.nMaxPqp = 45;
        stVideoInputConfig.qp_range.nQpInit = 37;
        stVideoInputConfig.qp_range.bEnMbQpLimit = 0;
    }
    stVideoInputConfig.enable_ve2isp_linkage = 1;
    stVideoInputConfig.vbv_buf_size = STREAM_VBV_BUF_SIZE;
    stVideoInputConfig.vbv_thresh_size = STREAM_VBV_THRESH_SIZE;
    AWVideoInput_Configure(context->channel, &stVideoInputConfig);

    if (OUTPUT_MODE_STREAM == stVideoInputConfig.output_mode)
    {
        AWVideoInput_CallBack(context->channel, video_stream_cb, 1);
    }

    if (stVideoInputConfig.encodeType == RT_VENC_CODEC_H264
        && stVideoInputConfig.output_mode == OUTPUT_MODE_STREAM) {
        VencH264VideoTiming timing;
        memset(&timing, 0, sizeof(timing));
        timing.fixed_frame_rate_flag = 1;
        timing.num_units_in_tick = 1000;
        timing.time_scale = timing.num_units_in_tick * stVideoInputConfig.dst_fps * 2;
        AWVideoInput_SetH264VideoTiming(context->channel, &timing);
    } else if (stVideoInputConfig.encodeType == RT_VENC_CODEC_H265
        && stVideoInputConfig.output_mode == OUTPUT_MODE_STREAM) {
        VencH265TimingS timing;
        memset(&timing, 0, sizeof(timing));
        timing.timing_info_present_flag = 1;
        timing.num_units_in_tick = 1000;
        timing.time_scale = timing.num_units_in_tick * stVideoInputConfig.dst_fps;
        timing.num_ticks_poc_diff_one = timing.num_units_in_tick;
        AWVideoInput_SetH265VideoTiming(context->channel, &timing);
    }


    AWVideoInput_Start(context->channel, 1);

    VideoChannelInfo stVideoChnInfo;
    memset(&stVideoChnInfo, 0, sizeof(VideoChannelInfo));
    AWVideoInput_GetChannelInfo(context->channel, &stVideoChnInfo);
    logd("chn[%d] encodeType[%d] size[%dx%d] frameRate[%d] bitRate[%d]\n", \
        stVideoChnInfo.mConfig.channelId, stVideoChnInfo.mConfig.encodeType, stVideoChnInfo.mConfig.width, \
        stVideoChnInfo.mConfig.height, stVideoChnInfo.mConfig.fps, stVideoChnInfo.mConfig.bitrate);

    if (OUTPUT_MODE_YUV == stVideoInputConfig.output_mode) {
        context->proc_exit = 0;
    }

    return ret;
}

static int StopCapture(struct video_source_rt_media_context *context)
{
    int ret = 0;

    logd("stop channel %d", context->channel);
    if (context->proc_running) {
        logd("stop channel %d", context->channel);
        context->proc_exit = 1;
        context->proc_running = 0;
    }

    AWVideoInput_Start(context->channel, 0);
    AWVideoInput_Destroy(context->channel);
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
    //add_one_context_to_map(context);
    video_src->ops_data = (void *)context;
    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&context->condition, &cond_attr);
    pthread_mutex_init(&context->mutex, NULL);
    pthread_mutex_init(&context->pause_lock, NULL);
    return 0;
}

static int video_source_rt_media_destroy(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    if (!context)
	    return 0;
    //remove_one_context_from_map(context);

    pthread_mutex_lock(&context->pause_lock);
    if (context->pause) {
        context->pause = 0;
        pthread_mutex_lock(&context->mutex);
        pthread_cond_signal(&context->condition);
        pthread_mutex_unlock(&context->mutex);
    }
    pthread_mutex_unlock(&context->pause_lock);

    pthread_cond_destroy(&context->condition);
    pthread_mutex_destroy(&context->mutex);
    pthread_mutex_destroy(&context->pause_lock);
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
        context->frame_manager.list_entry_num = FRAME_BUF_NUM;
    //init_frame_manager(&context->frame_manager);
    StartCapture(context);
    return 0;
}

static int video_source_rt_media_set_extra_config(void *thiz, struct video_source_extra_config *config)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    memcpy(&context->extra_config, config, sizeof(*config));
    return 0;
}

static int video_source_rt_media_stop(void *thiz)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    if (!context)
        return 0;
    StopCapture(context);
    //deinit_frame_manager(&context->frame_manager);
    return 0;
}

int video_source_rt_media_get_frame(void *thiz, struct video_source_frame *frame)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    VideoYuvFrame stVideoYuvFrame;
    memset(&stVideoYuvFrame, 0, sizeof(VideoYuvFrame));
    int ret = AWVideoInput_GetYuvFrame(context->extra_config.vipp, &stVideoYuvFrame);
    if (ret)
        return -1;

    frame->fd = stVideoYuvFrame.fd;
    frame->width = stVideoYuvFrame.widht;
    frame->height = stVideoYuvFrame.height;
    frame->buf_phy_addr = (unsigned int)stVideoYuvFrame.phyAddr[0];
    frame->buf_vir_addr = stVideoYuvFrame.virAddr[0];
    frame->pts = stVideoYuvFrame.pts;

    return 0;
}

static int video_source_rt_media_release_frame(void *thiz, struct video_source_frame *video_frame)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    VideoYuvFrame stVideoYuvFrame;
    memset(&stVideoYuvFrame, 0, sizeof(VideoYuvFrame));
    stVideoYuvFrame.widht = video_frame->width;
    stVideoYuvFrame.height = video_frame->height;
    stVideoYuvFrame.phyAddr[0] = (unsigned char* )video_frame->buf_phy_addr;
    stVideoYuvFrame.virAddr[0] = (unsigned char *)video_frame->buf_vir_addr;
    AWVideoInput_ReleaseYuvFrame(context->extra_config.vipp, &stVideoYuvFrame);

    return 0;
}

static int video_source_rt_media_get_isp_state(void *thiz, struct video_source_isp_state *isp_state)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    RTIspCtrlAttr isp_ctrl_attr;
    memset(&isp_ctrl_attr, 0, sizeof(isp_ctrl_attr));
    //isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_AE_EV_IDX_STATUS;
    AWVideoInput_GetIspAttrCfg(context->extra_config.vipp, &isp_ctrl_attr);
    //isp_state->ae_state = isp_ctrl_attr.isp_attr_cfg.ae_ev_idx_status;

    return 0;
}

static int video_source_rt_media_pause(void *thiz, int flag)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    if (flag) {
        AWVideoInput_Start(context->extra_config.vipp, 0);
    } else {
        AWVideoInput_Start(context->extra_config.vipp, 1);
    }

    return 0;
}

static int video_source_rt_media_set_camera_lowpw_mode(void *thiz, void *cfg)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    struct sensor_lowpw_cfg *lowpw_cfg = (struct sensor_lowpw_cfg *)cfg;
    return AWVideoInput_SetCameraLowPowerMode(context->extra_config.vipp, lowpw_cfg);
}

static int video_source_rt_media_set_orl(void *thiz, void *orl)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    RTIspOrl *isp_orl = (RTIspOrl *)orl;
    return AWVideoInput_SetIspOrl(context->extra_config.vipp, isp_orl);
}

static int video_source_rt_media_get_sharp_param(void *thiz, void *param)
{
    struct video_source *video_src = (struct video_source *)thiz;
    struct video_source_rt_media_context *context = (struct video_source_rt_media_context *)video_src->ops_data;

    return AWVideoInput_GetSharpParam(context->extra_config.vipp, (sEncppSharpParam *)param);
}

const struct video_source_ops video_source_rt_media_ops = {
    .create = video_source_rt_media_create,
    .destroy = video_source_rt_media_destroy,
    .start = video_source_rt_media_start,
    .stop = video_source_rt_media_stop,
    .set_extra_config = video_source_rt_media_set_extra_config,
    .get_frame = video_source_rt_media_get_frame,
    .release_frame = video_source_rt_media_release_frame,
    .get_isp_state = video_source_rt_media_get_isp_state,
    .pause = video_source_rt_media_pause,
    .set_camera_lowpw_mode = video_source_rt_media_set_camera_lowpw_mode,
    .set_orl = video_source_rt_media_set_orl,
    .get_sharp_param = video_source_rt_media_get_sharp_param,
};
