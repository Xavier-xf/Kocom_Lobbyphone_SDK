#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <errno.h>
#include <sys/mman.h>

#include <rt_media/AW_VideoInput_API.h>
#include "rgb_ctrl.h"

#include "base_list.h"
#include "../base/include/video_encode.h"
#include "../../utils/include/debug.h"

#define FRAME_BUF_NUM                           (1)
#define FRAME_BUF_LEN                           (1280*720*3/2)
#define MAX_VIDEO_SOURCE_MPP_CONTEXT_NUM        (16)
#define VE_VBV_CACHE_TIME                       (4)  // unit:seconds, exp:1,2,3,4...
#define STREAM_VBV_BUF_SIZE                     (2*1024*1024)
#define STREAM_VBV_THRESH_SIZE                  (100*1024)
#define MAX_VIDEO_SOURCE_RT_MEDIA_CONTEXT_NUM   (16)
#define DEFAULT_MOTION_SEARCH_HOR_NUM           (30)
#define DEFAULT_MOTION_SEARCH_VER_NUM           (17)
#define DEFAULT_MOTION_SEARCH_TOTAL_NUM         (DEFAULT_MOTION_SEARCH_HOR_NUM * DEFAULT_MOTION_SEARCH_VER_NUM)

struct video_encode_rt_media_frame_node {
    struct video_encode_frame frame;
    struct list_head list;
};

struct vbv_buffer_info
{
    int            share_fd;
    int            fd;
    int            bInit;
    void           *virBuf;
    int            size;
    int            handle;
};

struct video_encode_osd_item_node {
    struct video_encode_osd_item item;
    struct list_head list;
};

struct video_encode_rt_media_stream {
    int channel;
    VideoInputConfig config;
    unsigned long long pts;

    void *region_buf;
    void *private;

    struct list_head osd_item_list; //list entry: struct video_encode_rt_media_stream
};

struct video_encode_rt_media_context
{
    struct video_encode_rt_media_stream stream;
    struct video_encode_config base_config;
};

struct video_encode_rt_media_context_map {
    struct video_encode_rt_media_context *context;
};
static struct video_encode_rt_media_context_map g_context_map[16];
static int rt_media_init_cnt;

static unsigned int getSysTickMs()
{
    unsigned int ms = 0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ms = tv.tv_sec*1000 + tv.tv_usec/1000;
    return ms;
}

static void add_one_context_to_map(struct video_encode_rt_media_context *context)
{
    for (int i = 0; i < MAX_VIDEO_SOURCE_RT_MEDIA_CONTEXT_NUM; i++) {
        if (!g_context_map[i].context) {
            g_context_map[i].context = context;
            break;
        }
    }
}

static void remove_one_context_from_map(struct video_encode_rt_media_context *context)
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
        loge("not find context %p channel %d\n", context, context->stream.channel);
}

static struct video_encode_rt_media_context *find_one_context_from_map(int channel)
{
    for (int i = 0; i < MAX_VIDEO_SOURCE_RT_MEDIA_CONTEXT_NUM; i++) {
        if (g_context_map[i].context && (g_context_map[i].context->stream.channel == channel))
            return g_context_map[i].context;
    }
    return NULL;
}

static int start_encode(struct video_encode_rt_media_context *context)
{
    int ret = 0;
    int nEncType = 0;

    VideoInputConfig stVideoInputConfig;
    memset(&stVideoInputConfig, 0, sizeof(VideoInputConfig));
    context->stream.channel = context->base_config.channel;
    stVideoInputConfig.channelId = context->base_config.channel;
    switch (context->base_config.format)
    {
        case VIDEO_ENCODE_STREAM_TYPE_H264:
            stVideoInputConfig.encodeType = RT_VENC_CODEC_H264;
            stVideoInputConfig.bitrate = context->base_config.bitrate;
            stVideoInputConfig.pixelformat = RT_PIXEL_YUV420SP;
            break;
        case VIDEO_ENCODE_STREAM_TYPE_MJPEG:
            stVideoInputConfig.encodeType = RT_VENC_CODEC_JPEG;
            stVideoInputConfig.jpg_quality = 30;
            stVideoInputConfig.jpg_mode = 1;
            stVideoInputConfig.bitrate = context->base_config.bitrate;
            stVideoInputConfig.bit_rate_range.bitRateMin = context->base_config.bitrate;
            stVideoInputConfig.bit_rate_range.bitRateMax = context->base_config.bitrate;
            stVideoInputConfig.bit_rate_range.fRangeRatioTh = 0;
            stVideoInputConfig.bit_rate_range.nQualityTh = 0;
            stVideoInputConfig.bit_rate_range.nQualityStep[0] = 5;
            stVideoInputConfig.bit_rate_range.nQualityStep[1] = 5;
            stVideoInputConfig.bit_rate_range.nSubQualityDelay = 0;
            stVideoInputConfig.bit_rate_range.nAddQualityDelay = 5;
            stVideoInputConfig.bit_rate_range.nMinQuality = 20;
            stVideoInputConfig.bit_rate_range.nMaxQuality = 80;
            stVideoInputConfig.pixelformat = RT_PIXEL_YUV420SP;
            break;
        case VIDEO_ENCODE_STREAM_TYPE_JPEG:
            stVideoInputConfig.encodeType = RT_VENC_CODEC_JPEG;
            stVideoInputConfig.jpg_quality = 80;
            stVideoInputConfig.jpg_mode = 0;
        default:
            loge("unsupport encode type[0x%x], use default MJPEG\n", context->base_config.format);
            return -1;
    }
    stVideoInputConfig.output_mode = OUTPUT_MODE_ENCODE_OUTSIDE_YUV;
    stVideoInputConfig.fps = context->base_config.framerate;
    stVideoInputConfig.gop = context->base_config.framerate;
    stVideoInputConfig.product_mode = PRODUCT_DOORBELL;
    stVideoInputConfig.width = context->base_config.width;
    stVideoInputConfig.height = context->base_config.height;
    stVideoInputConfig.dst_width = context->base_config.width;
    stVideoInputConfig.dst_height = context->base_config.height;
    stVideoInputConfig.enable_isp2ve_linkage = 0;
    stVideoInputConfig.enable_ve2isp_linkage = 0;
    //stVideoInputConfig.debug_sharp = 1;
    logv("encpp:%d, %dx%d -> %dx%d\n", stVideoInputConfig.enable_sharp, stVideoInputConfig.width,
        stVideoInputConfig.height, stVideoInputConfig.dst_width, stVideoInputConfig.dst_height);
    if (stVideoInputConfig.encodeType == RT_VENC_CODEC_H264)
    {
        stVideoInputConfig.mRcMode = AW_VBR;
        stVideoInputConfig.vbr_opt_enable = 1;
        stVideoInputConfig.product_mode = PRODUCT_STATIC_IPC;
        stVideoInputConfig.profile = VENC_H264ProfileMain;
        stVideoInputConfig.level = VENC_H264Level51;
        stVideoInputConfig.qp_range.nMinqp = 25;
        stVideoInputConfig.qp_range.nMaxqp = 45;
        stVideoInputConfig.qp_range.nMinPqp = 25;
        stVideoInputConfig.qp_range.nMaxPqp = 45;
        stVideoInputConfig.qp_range.nQpInit = 37;
        stVideoInputConfig.qp_range.bEnMbQpLimit = 0;
    }
    stVideoInputConfig.vbv_buf_size = STREAM_VBV_BUF_SIZE;
    stVideoInputConfig.vbv_thresh_size = STREAM_VBV_THRESH_SIZE;
    AWVideoInput_Configure(stVideoInputConfig.channelId, &stVideoInputConfig);

    /*if (stVideoInputConfig.encodeType == RT_VENC_CODEC_H264) {
        VencH264VideoTiming timing;
        memset(&timing, 0, sizeof(timing));
        timing.fixed_frame_rate_flag = 1;
        timing.num_units_in_tick = 1000;
        timing.time_scale = timing.num_units_in_tick * stVideoInputConfig.dst_fps * 2;
        AWVideoInput_SetH264VideoTiming(stVideoInputConfig.channelId, &timing);
    } else if (stVideoInputConfig.encodeType == RT_VENC_CODEC_H265) {
        VencH265TimingS timing;
        memset(&timing, 0, sizeof(timing));
        timing.timing_info_present_flag = 1;
        timing.num_units_in_tick = 1000;
        timing.time_scale = timing.num_units_in_tick * stVideoInputConfig.dst_fps;
        timing.num_ticks_poc_diff_one = timing.num_units_in_tick;
        AWVideoInput_SetH265VideoTiming(stVideoInputConfig.channelId, &timing);
    }*/

    AWVideoInput_Start(stVideoInputConfig.channelId, 1);

    VideoChannelInfo stVideoChnInfo;
    memset(&stVideoChnInfo, 0, sizeof(VideoChannelInfo));
    AWVideoInput_GetChannelInfo(stVideoInputConfig.channelId, &stVideoChnInfo);
    logd("chn[%d] encodeType[%d] size[%dx%d] frameRate[%d] bitRate[%d]\n", \
        stVideoChnInfo.mConfig.channelId, stVideoChnInfo.mConfig.encodeType, stVideoChnInfo.mConfig.width, \
        stVideoChnInfo.mConfig.height, stVideoChnInfo.mConfig.fps, stVideoChnInfo.mConfig.bitrate);
    memcpy(&context->stream.config, &stVideoChnInfo, sizeof(stVideoChnInfo));

    VencMotionSearchParam motion_search_param;
    memset(&motion_search_param, 0, sizeof(VencMotionSearchParam));
    motion_search_param.en_motion_search = 1;
    /*motion_search_param.dis_default_para = 1;
    motion_search_param.hor_region_num = MOTION_SEARCH_HOR_NUM;
    motion_search_param.ver_region_num = MOTION_SEARCH_VER_NUM;
    motion_search_param.en_check_mv = 0;
    motion_search_param.en_check_mad = 1;
    motion_search_param.en_morpholog = 0;
    motion_search_param.large_mv_th = 22;
    motion_search_param.large_mad_th = 20;
    motion_search_param.background_weight = 0;
    motion_search_param.large_mv_ratio_th = 12.0f;
    motion_search_param.non_zero_mv_ratio_th = 22.0f;
    motion_search_param.large_mad_ratio_th = 12.0f;*/
    motion_search_param.dis_default_para = 0;
    AWVideoInput_SetMotionSearchParam(stVideoInputConfig.channelId, &motion_search_param);
    context->stream.region_buf = malloc(DEFAULT_MOTION_SEARCH_TOTAL_NUM*sizeof(VencMotionSearchRegion));

    AWVideoInput_SetSharp(stVideoInputConfig.channelId, 1);

    return ret;
}

static int stop_encode(struct video_encode_rt_media_context *context)
{
    struct video_encode_rt_media_stream *stream = &context->stream;

    AWVideoInput_Start(context->base_config.channel, 0);
    AWVideoInput_Destroy(context->base_config.channel);
    if (stream->region_buf)
        free(stream->region_buf);

    return 0;
}

static int video_encode_rt_media_create(void *thiz)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = malloc(sizeof(*context));
    if (!context)
        loge("malloc video souce mpp context fail!");
    memset(context, 0, sizeof(*context));
    context->stream.channel = -1;
    video_encode->ops_data = (void *)context;
    //add_one_context_to_map(context);

    return 0;
}

static int video_encode_rt_media_destroy(void *thiz)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    if (!context)
        return 0;
    //remove_one_context_from_map(context);
    free(context);
    context = NULL;
    video_encode->ops_data = NULL;

    return 0;
}

static int video_encode_rt_media_start(void *thiz, struct video_encode_config *config)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    memcpy(&context->base_config, config, sizeof(*config));
    INIT_LIST_HEAD(&context->stream.osd_item_list);
    start_encode(context);

    return 0;
}

static int video_encode_rt_media_stop(void *thiz)
{
    struct video_encode_osd_item_node *entry, *tmp;
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    stop_encode(context);
    list_for_each_entry_safe(entry, tmp, &context->stream.osd_item_list, list) {
        list_del(&entry->list);
        if (entry->item.data_buf) {
            free(entry->item.data_buf);
            entry->item.data_buf = NULL;
            entry->item.data_size = 0;
        }
        free(entry);
        entry = NULL;
    }

    return 0;
}

static int video_encode_rt_media_put_frame(void *thiz, struct video_encode_frame *frame)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    VideoYuvFrame yuv_frame;
    memset(&yuv_frame, 0, sizeof(yuv_frame));
    yuv_frame.widht = frame->width;
    yuv_frame.height = frame->height;
    yuv_frame.phyAddr[0] = (unsigned char*)frame->phy_addr[0];
    yuv_frame.pts = frame->pts;
    int yuv_size = yuv_frame.widht * yuv_frame.height * 3 / 2;
    int ret = AWVideoInput_SubmitFilledYuvFrame(context->stream.channel, &yuv_frame, yuv_size);

    return ret;
}

static int video_encode_rt_media_get_frame(void *thiz, struct video_encode_frame *stream)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    AWVideoInput_StreamInfo data_info;
    memset(&data_info, 0, sizeof(data_info));
    int ret = AWVideoInput_GetStreamData(context->stream.channel, &data_info);
    if (ret)
        return ret;

    stream->id = data_info.id;
    if (data_info.size0) {
        stream->len[0] = data_info.size0;
        stream->vir_addr[0] = data_info.data0;
    }
    if (data_info.size1) {
        stream->len[1] = data_info.size1;
        stream->vir_addr[1] = data_info.data1;
    }
    if (data_info.size2) {
        stream->len[2] = data_info.size2;
        stream->vir_addr[2] = data_info.data2;
    }
    stream->key = data_info.keyframe_flag;
    stream->pts = data_info.pts;

    return 0;
}

static int video_encode_rt_media_release_frame(void *thiz, struct video_encode_frame *video_frame)
{
    int find = 0;
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    AWVideoInput_StreamInfo data_info;
    memset(&data_info, 0, sizeof(data_info));
    data_info.id = video_frame->id;
    return AWVideoInput_ReturnStreamData(context->stream.channel, &data_info);
}

static void video_encode_rt_media_get_motion_search_result(void *thiz, struct motion_search_result *result)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    VencMotionSearchResult motion_search_result;
    memset(&motion_search_result, 0, sizeof(motion_search_result));
    motion_search_result.region = context->stream.region_buf;
    AWVideoInput_GetMotionSearchResult(context->stream.channel, &motion_search_result);
    for(int i=0; i < motion_search_result.total_region_num; i++) {
        if(motion_search_result.region[i].is_motion) {
            logv("area_%d:[(%d,%d),(%d,%d)]", i,
                motion_search_result.region[i].pix_x_bgn, motion_search_result.region[i].pix_y_bgn,
                motion_search_result.region[i].pix_x_end, motion_search_result.region[i].pix_y_end);
        }
    }
    result->region = (struct motion_search_region *)motion_search_result.region;
    result->total_region_num = motion_search_result.total_region_num;
    result->motion_region_num = motion_search_result.motion_region_num;
}

static int video_encode_rt_media_request_idr(void *thiz)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    return AWVideoInput_SetIFrame(context->stream.channel);
}

static int video_encode_rt_media_pause(void *thiz, int flag)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    return AWVideoInput_Start(context->stream.channel, !flag);
}

static int video_encode_rt_media_get_spspps_info(void *thiz, struct video_encode_spspps_info *info)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    sps_pps_data_info spspps_info;
    memset(&spspps_info, 0, sizeof(spspps_info));
    int ret = AWVideoInput_GetSpsPpsInfo(context->stream.channel, &spspps_info);
    if (ret)
        return ret;
    info->buf = spspps_info.buf;
    info->len = spspps_info.size;
    return 0;
}

static void deep_copy_osd_item(struct video_encode_osd_item *dst, struct video_encode_osd_item *src)
{
    dst->index    = src->index;
    dst->enable   = src->enable;
    dst->x        = src->x;
    dst->y        = src->y;
    dst->w        = src->w;
    dst->h        = src->h;
    dst->osd_type = src->osd_type;
    if (dst->data_size < src->data_size) {
        if (dst->data_buf)
            free(dst->data_buf);
        dst->data_buf = malloc(src->data_size);
        if (!dst->data_buf)
            loge("fatal error! malloc item data buf %d Bytes fail!", src->data_size);
        memset(dst->data_buf, 0, sizeof(src->data_size));
        memcpy(dst->data_buf, src->data_buf, src->data_size);
        dst->data_size = src->data_size;
    } else {
        memcpy(dst->data_buf, src->data_buf, src->data_size);
        dst->data_size = src->data_size;
    }
}

static int video_encode_rt_media_set_osd(void *thiz, void *osd)
{
    struct video_encode_osd_item_node *entry, *tmp;
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    int find = 0;
    struct video_encode_osd_item *item = (struct video_encode_osd_item *)osd;
    list_for_each_entry_safe(entry, tmp, &context->stream.osd_item_list, list) {
        if (entry->item.index == item->index) {
            find = 1;
            break;
        }
    }
    if (find) {
        logv("item %d data %p len %d", item->index, item->data_buf, item->data_size);
        deep_copy_osd_item(&entry->item, item);
    } else {
        struct video_encode_osd_item_node *node = malloc(sizeof(*node));
        if (!node)
            loge("fatal error! malloc osd item node fail!");
        memset(node, 0, sizeof(*node));
        logv("item %d data %p len %d", item->index, item->data_buf, item->data_size);
        deep_copy_osd_item(&node->item, item);
        list_add_tail(&node->list, &context->stream.osd_item_list);
    }
    VideoInputOSD input_osd;
    memset(&input_osd, 0, sizeof(input_osd));
    input_osd.argb_type = VENC_OVERLAY_ARGB8888;
    input_osd.invert_mode = 1;
    input_osd.invert_threshold = 50;
    list_for_each_entry_safe(entry, tmp, &context->stream.osd_item_list, list) {
        if (!entry->item.enable)
            continue;
        input_osd.item_info[input_osd.osd_num].start_x   = entry->item.x;
        input_osd.item_info[input_osd.osd_num].start_y   = entry->item.y;
        input_osd.item_info[input_osd.osd_num].widht     = entry->item.w;
        input_osd.item_info[input_osd.osd_num].height    = entry->item.h;
        input_osd.item_info[input_osd.osd_num].osd_type  = entry->item.osd_type;
        input_osd.item_info[input_osd.osd_num].data_buf  = entry->item.data_buf;
        input_osd.item_info[input_osd.osd_num].data_size = entry->item.data_size;
        input_osd.osd_num++;
    }

    return AWVideoInput_SetOSD(context->stream.channel, &input_osd);
}

static int video_encode_rt_media_set_sharp_param(void *thiz, void *param)
{
    struct video_encode *video_encode = (struct video_encode *)thiz;
    struct video_encode_rt_media_context *context = (struct video_encode_rt_media_context *)video_encode->ops_data;

    return AWVideoInput_SetSharpParam(context->stream.channel, (sEncppSharpParam *)param);
}

const struct video_encode_ops video_encode_rt_media_ops = {
    .create = video_encode_rt_media_create,
    .destroy = video_encode_rt_media_destroy,
    .start = video_encode_rt_media_start,
    .stop = video_encode_rt_media_stop,
    .put_frame = video_encode_rt_media_put_frame,
    .get_frame = video_encode_rt_media_get_frame,
    .release_frame = video_encode_rt_media_release_frame,
    .get_motion_search_result = video_encode_rt_media_get_motion_search_result,
    .request_idr = video_encode_rt_media_request_idr,
    .pause = video_encode_rt_media_pause,
    .get_spspps_info = video_encode_rt_media_get_spspps_info,
    .set_osd = video_encode_rt_media_set_osd,
    .set_sharp_param = video_encode_rt_media_set_sharp_param,
};
