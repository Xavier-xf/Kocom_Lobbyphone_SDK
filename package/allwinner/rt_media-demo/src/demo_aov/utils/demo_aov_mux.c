#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_DEBUG
#include "include/debug.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <mm_comm_venc.h>
#include <mm_comm_mux.h>
#include <mpi_venc.h>
#include <mpi_mux.h>
#include "aw_message_queue.h"

#include "include/demo_aov_mux.h"
#include "../video_encode/base/include/video_encode.h"

#define TASK_EXIT_MSG        (100)
#define TASK_START_MSG       (200)

enum demo_aov_mux_mode {
    DEMO_AOV_MUX_MODE_NORMAL, //normal mux
    DEMO_AOV_MUX_MODE_DETECRD, //deteced mux
    DEMO_AOV_MUX_MODE_SWITCH_FILE, //switch file mux
};

struct stream_node {
    struct video_encode_frame stream;
    struct list_head list;
};

struct demo_aov_mux_context {
    struct list_head stream_idle_list;
    struct list_head stream_ready_list;
    struct list_head stream_using_list;
    pthread_mutex_t list_lock;
    unsigned long long stream_list_len;
    unsigned int list_nodes;
    unsigned long long base_pts;
    unsigned long long prev_pts;
    unsigned long long cur_pts;
    unsigned long long stream_count;
    int muxer_init;
    MUX_CHN mux_chn;
    MUX_CHN_ATTR_S mux_chn_attr;

    int exit;
    int need_wait;
    int wait_flag;
    pthread_mutex_t lock;
    pthread_cond_t condition;
    pthread_condattr_t cond_attr;

    AwRtMessageQueue *msg_queue;
    pthread_t task_proc_trd;

    struct demo_aov_mux_config config;
    struct video_encode_spspps_info spspps_info;
};

static struct demo_aov_mux_context *g_context;

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    int ret;
    ERRORTYPE eRet = SUCCESS;

    if(MOD_ID_MUX == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                logd("revice record file done");
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                logv("need next fd");
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                logd("mux bs frame available");
                break;
            }
            case MPP_EVENT_MUX_FORCE_I_FRAME:
            {
                logv("need force I frame!");
                break;
            };
            default:
            {
                loge("fatal error! unknown event[0x%x]", event);
                break;
            }
        }
    }
    else
    {
         loge("fatal error! unknown chn[%d,%d,%d]", pChn->mModId, pChn->mDevId, pChn->mChnId);
    }

    return eRet;
}

static void configMuxChnAttr(MUX_CHN_ATTR_S *pMuxChnAttr)
{
    pMuxChnAttr->mVideoAttrValidNum = 1;
    pMuxChnAttr->mVideoAttr[0].mWidth = g_context->config.width;
    pMuxChnAttr->mVideoAttr[0].mHeight = g_context->config.height;;
    pMuxChnAttr->mVideoAttr[0].mVideoFrmRate = g_context->config.framerate*1000;
    pMuxChnAttr->mVideoAttr[0].mMaxKeyInterval = g_context->config.framerate;
    pMuxChnAttr->mVideoAttr[0].mVideoEncodeType = PT_H264;
    pMuxChnAttr->mVideoAttr[0].mVeChn = 0;
    pMuxChnAttr->mAudioEncodeType = PT_MAX;
    pMuxChnAttr->mTextEncodeType = PT_MAX;

    pMuxChnAttr->mMediaFileFormat = MEDIA_FILE_FORMAT_MP4;
    pMuxChnAttr->mMaxFileDuration = g_context->config.file_duration * 1000; //senconds to mseconds
    pMuxChnAttr->mMaxFileSizeBytes = 0;
    pMuxChnAttr->mCallbackOutFlag = FALSE;
    pMuxChnAttr->mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    pMuxChnAttr->mSimpleCacheSize = 64*1024;
}

static unsigned long long GetSysTimeUsMonotonic()
{
    long long curr;
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = ((unsigned long long)(t.tv_sec)*1000000000LL + t.tv_nsec)/1000LL;
    return (unsigned long long)curr;
}

static int createMuxChn(struct demo_aov_mux_context *context)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    memset(&context->mux_chn_attr, 0, sizeof(context->mux_chn_attr));
    configMuxChnAttr(&context->mux_chn_attr);

    char file[256] = {0};
    sprintf(file, "/mnt/extsd/v821_aov_%llu.mp4", GetSysTimeUsMonotonic());
    int nFd = open(file, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (nFd < 0)
    {
        loge("fatal error! Failed to open %s", file);
        return -1;
    }
    logd("record file %s", file);

    context->mux_chn = 0;
    nSuccessFlag = FALSE;
    while (context->mux_chn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(context->mux_chn, &context->mux_chn_attr, nFd, 0);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            logd("create muxChn[%d] success!", context->mux_chn);
            break;
        }
        else if(ERR_MUX_EXIST == ret)
        {
            context->mux_chn++;
        }
        else
        {
            loge("fatal error! create mux chn fail[0x%x]!", ret);
            context->mux_chn++;
        }
    }
    if (FALSE == nSuccessFlag)
    {
        context->mux_chn = MM_INVALID_CHN;
        loge("fatal error! create muxChannel fail!");
        result = -1;
    }
    else
    {
        logd("create mux chn[%d] file format[%d]", context->mux_chn, context->mux_chn_attr.mMediaFileFormat);
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)context;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(context->mux_chn, &cbInfo);
        result = 0;
    }
    if(nFd >= 0)
    {
        close(nFd);
        nFd = -1;
    }

    AW_MPI_MUX_SetSwitchFileDurationPolicy(context->mux_chn, RecordFileDurationPolicy_MinDuration);
    AW_MPI_MUX_SetVeChnBindStreamId(context->mux_chn, 0, 0);
    if (g_context->spspps_info.buf && g_context->spspps_info.len) {
        VencHeaderData data;
        memset(&data, 0, sizeof(data));
        data.pBuffer = g_context->spspps_info.buf;
        data.nLength = g_context->spspps_info.len;
        AW_MPI_MUX_SetH264SpsPpsInfo(context->mux_chn, 0, &data);
        AW_MPI_MUX_StartChn(context->mux_chn);
    }
    return result;
}

static int destroyMuxChn(struct demo_aov_mux_context *context)
{
    AW_MPI_MUX_StopChn(context->mux_chn, 0);
    AW_MPI_MUX_DestroyChn(context->mux_chn);

    return 0;
}

static int sendto_muxer(struct demo_aov_mux_context * context, struct video_encode_frame * src, enum RECORD_TYPE record_type)
{
    VENC_PACK_S pack;
    VENC_STREAM_S venc_stream;
    memset(&venc_stream, 0, sizeof(venc_stream));
    memset(&pack, 0, sizeof(pack));
    venc_stream.mPackCount = 1;
    venc_stream.mpPack = &pack;
    venc_stream.mpPack[0].mpAddr0 = src->vir_addr[0];
    venc_stream.mpPack[0].mpAddr1 = src->vir_addr[1];
    venc_stream.mpPack[0].mpAddr2 = src->vir_addr[2];
    venc_stream.mpPack[0].mLen0 = src->len[0];
    venc_stream.mpPack[0].mLen1 = src->len[1];
    venc_stream.mpPack[0].mLen2 = src->len[2];
    venc_stream.mpPack[0].mDataType.enH264EType = src->key ? VENC_H264_NALU_ISLICE : VENC_H264_NALU_PSLICE;
    venc_stream.mpPack[0].mPTS = src->pts;
    venc_stream.mpPack[0].mbFrameEnd = TRUE;
    venc_stream.mSeq = context->stream_count;
    switch (record_type) {
    case RECORD_TYPE_TIMELAPSE:
        venc_stream.mpPack[0].mPTS = context->stream_count * (1000000 / context->config.framerate);
        break;

    case RECORD_TYPE_FRAMELOOP:
    case RECORD_TYPE_NORMAL:
    default:
        venc_stream.mpPack[0].mPTS = src->pts;
        break;
    };
    context->stream_count++;
    int ret = AW_MPI_MUX_SendVideoStreamSync(context->mux_chn, &venc_stream, 0);
    if (ret != SUCCESS)
        loge("send video stream sync fail!");
    return 0;
}

static void init_stream_list(struct demo_aov_mux_context *context)
{
    INIT_LIST_HEAD(&context->stream_idle_list);
    INIT_LIST_HEAD(&context->stream_ready_list);
    INIT_LIST_HEAD(&context->stream_using_list);
    pthread_mutex_init(&context->list_lock, NULL);
}

static void destroy_stream_list(struct demo_aov_mux_context *context)
{
    struct stream_node *entry, *tmp;
    unsigned int stream_num = 0;
    pthread_mutex_lock(&context->list_lock);
    list_for_each_entry_safe(entry, tmp, &context->stream_idle_list, list) {
        stream_num++;
        list_del(&entry->list);
        free(entry);
    }
    pthread_mutex_unlock(&context->list_lock);
    pthread_mutex_destroy(&context->list_lock);
    loge("stream list node num %d", stream_num);
}

static int add_stream(struct demo_aov_mux_context *context, struct video_encode_frame *stream)
{
    pthread_mutex_lock(&context->list_lock);
    /*if (!context->base_pts && !stream->key) {
        logw("first stream isn't key frame!");
        pthread_mutex_unlock(&context->list_lock);
        context->config.request_idr_frame();
        context->config.release_stream_callback((void *)stream);
        return -1;
    }*/
    if (list_empty(&context->stream_idle_list)) {
        struct stream_node *node = malloc(sizeof(*node));
        if (!node) {
            loge("alloc stream node fail!");
            context->config.release_stream_callback((void *)stream);
            pthread_mutex_unlock(&context->list_lock);
            return -1;
        }
        memset(node, 0, sizeof(*node));
        list_add_tail(&node->list, &context->stream_idle_list);
    }
    struct stream_node *entry =
        list_first_entry_or_null(&context->stream_idle_list, struct stream_node, list);
    memcpy(&entry->stream, stream, sizeof(*stream));
    if (!context->base_pts)
        context->base_pts = entry->stream.pts;
    context->list_nodes++;
    context->cur_pts = entry->stream.pts;
    context->stream_list_len += (entry->stream.len[0] + entry->stream.len[1] + entry->stream.len[2]);
    list_move_tail(&entry->list, &context->stream_ready_list);

    if (context->stream_list_len >= context->config.max_cache_len) {
            AwRtMessage msg;
        memset(&msg, 0, sizeof(msg));
        msg.messageId = TASK_START_MSG;
        msg.para0 = 0;
        aw_message_queue_postMessage(g_context->msg_queue, &msg);
    }
    pthread_mutex_unlock(&context->list_lock);
    return 0;
}

static int remove_stream_and_mux(struct demo_aov_mux_context *context, int mux_mode)
{
    int ret;
    struct stream_node *entry, *tmp;
    unsigned int stream_num = 0;
    unsigned int stream_len = 0;
    enum RECORD_TYPE record_file_type = context->config.record_type; //default frameloop

    /*if (detected)
        record_file_type = RECORD_TYPE_NORMAL;*/

    unsigned long long start = GetSysTimeUsMonotonic();
    pthread_mutex_lock(&context->list_lock);
    list_for_each_entry_safe(entry, tmp, &context->stream_ready_list, list) {
        list_move_tail(&entry->list, &context->stream_using_list);
    }
    context->list_nodes = 0;
    context->stream_list_len = 0;
    if (mux_mode == DEMO_AOV_MUX_MODE_SWITCH_FILE) {
        context->base_pts = 0;
        context->prev_pts = 0;
        context->cur_pts = 0;
        context->stream_count = 0;
        context->config.request_idr_frame();
    }
    pthread_mutex_unlock(&context->list_lock);
    list_for_each_entry_safe(entry, tmp, &context->stream_using_list, list) {
        stream_num++;
        stream_len += (entry->stream.len[0] + entry->stream.len[1] + entry->stream.len[2]);
        if (!context->stream_count && !entry->stream.key) {
            logw("first stream isn't key frame!");
        } else {
            sendto_muxer(context, &entry->stream, record_file_type);
        }
        context->config.release_stream_callback((void *)&entry->stream);
    }
    pthread_mutex_lock(&context->list_lock);
    list_for_each_entry_safe(entry, tmp, &context->stream_using_list, list) {
        list_move_tail(&entry->list, &context->stream_idle_list);
    }
    pthread_mutex_unlock(&context->list_lock);
    logd("record type %d stream list node num %d stream len %d mux timeuse %llu",
        record_file_type, stream_num, stream_len, GetSysTimeUsMonotonic()-start);
    return 0;
}

static int get_stream_duration(struct demo_aov_mux_context *context)
{
    pthread_mutex_lock(&context->list_lock);
    int duration = (context->cur_pts - context->base_pts) / 1000000; //us to seconds
    logv("cur_pts %llu base_pts %llu", context->cur_pts, context->base_pts);
    pthread_mutex_unlock(&context->list_lock);
    return duration;
}

static int get_stream_list_len(struct demo_aov_mux_context *context)
{
    pthread_mutex_lock(&context->list_lock);
    int len = context->stream_list_len;
    pthread_mutex_unlock(&context->list_lock);
    return len;
}

static void switch_record_file(struct demo_aov_mux_context *context)
{
    destroyMuxChn(context);
    createMuxChn(context);
}

static void write_stream_to_sdcard(struct demo_aov_mux_context *context, int mux_mode)
{
    if ((get_stream_duration(context) >= context->config.file_duration) && (mux_mode == DEMO_AOV_MUX_MODE_NORMAL)) {
        logv("stream durauon %d", get_stream_duration(context));
        remove_stream_and_mux(context, DEMO_AOV_MUX_MODE_SWITCH_FILE);
        switch_record_file(context);
        context->config.request_idr_frame();
    }

    if ((get_stream_list_len(context) >= context->config.max_cache_len)/* || detected*/)
        remove_stream_and_mux(context, mux_mode);
}

static void *task_proc(void *arg)
{
    int ret;
    int mux_mode;
    struct demo_aov_mux_context *context = (struct demo_aov_mux_context *)arg;

    createMuxChn(g_context);

    while (1) {
        AwRtMessage msg;
        memset(&msg, 0, sizeof(msg));
        ret = aw_message_queue_getMessage(context->msg_queue, &msg);
        if (ret != 0) {
            aw_message_queue_waitMessage(context->msg_queue, 500);
            continue;
        }

        if (msg.messageId == TASK_EXIT_MSG) {
            break;
        }

        pthread_mutex_lock(&g_context->lock);
        g_context->need_wait = 1;
        pthread_mutex_unlock(&g_context->lock);
        if (msg.para0)
            mux_mode = DEMO_AOV_MUX_MODE_DETECRD;
        else
            mux_mode = DEMO_AOV_MUX_MODE_NORMAL;
        write_stream_to_sdcard(context, mux_mode);
        pthread_mutex_lock(&context->lock);
        context->need_wait = 0;
        if (context->wait_flag) {
            context->wait_flag = 0;
            pthread_cond_signal(&context->condition);
        }
        context->wait_flag = 0;
        pthread_mutex_unlock(&context->lock);
    }

    remove_stream_and_mux(g_context, 0);
    destroyMuxChn(context);

    return (void *)NULL;
}

int demo_aov_mux_init(struct demo_aov_mux_config *config)
{
    g_context = malloc(sizeof(*g_context));
    if (!g_context) {
        loge("fatal error! alloc demo aov context fail!");
        return -1;
    }
    memset(g_context, 0, sizeof(*g_context));
    memcpy(&g_context->config, config, sizeof(*config));
    logd("size %dx%d framerate %d key %d file %d ongoing %d cache %d type %d cb %p",
        g_context->config.width, g_context->config.height, g_context->config.framerate,
        g_context->config.max_key_interval, g_context->config.file_duration,
        g_context->config.ongoing_duration, g_context->config.max_cache_len,
        g_context->config.record_type, g_context->config.release_stream_callback);

    init_stream_list(g_context);

    pthread_condattr_init(&g_context->cond_attr);
    pthread_condattr_setclock(&g_context->cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&g_context->condition, &g_context->cond_attr);
    pthread_mutex_init(&g_context->lock, NULL);

    g_context->msg_queue = aw_message_queue_create(16, "demo_aov_persion_detect");
    if (!g_context->msg_queue)
        loge("message queue create fail!");

    pthread_create(&g_context->task_proc_trd, NULL, task_proc, (void *)g_context);

    return 0;
}

int demo_aov_mux_destroy(void)
{
    if (!g_context)
        return 0;

    AwRtMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.messageId = TASK_EXIT_MSG;
    aw_message_queue_postMessage(g_context->msg_queue, &msg);
    pthread_join(g_context->task_proc_trd, NULL);

    aw_message_queue_destroy(g_context->msg_queue);

    pthread_cond_destroy(&g_context->condition);
    pthread_mutex_destroy(&g_context->lock);
    pthread_condattr_destroy(&g_context->cond_attr);

    destroy_stream_list(g_context);

    if (g_context->spspps_info.buf)
        free(g_context->spspps_info.buf);

    free(g_context);
    return 0;
}

int demo_aov_mux_add_stream(void *stream)
{
    struct video_encode_frame *p = (struct video_encode_frame *)stream;

    return add_stream(g_context, p);
}

void demo_aov_mux_check_cache(int algo_detected)
{
    AwRtMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.messageId = TASK_START_MSG;
    msg.para0 = algo_detected;
    aw_message_queue_postMessage(g_context->msg_queue, &msg);
}

int demo_aov_mux_wait_complete(int timeout)
{
    int ret = 0, relative_sec, relative_nsec;
    struct timespec ts;

    pthread_mutex_lock(&g_context->lock);
    if (g_context->need_wait) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        relative_sec = timeout / 1000;
        relative_nsec = (timeout % 1000) * 1000000;
        ts.tv_sec += relative_sec;
        ts.tv_nsec += relative_nsec;
        ts.tv_sec += ts.tv_nsec/(1000 * 1000 * 1000);
        ts.tv_nsec = ts.tv_nsec%(1000 * 1000 * 1000);
        g_context->wait_flag = 1;
        ret = pthread_cond_timedwait(&g_context->condition, &g_context->lock, &ts);
    }
    pthread_mutex_unlock(&g_context->lock);
    return ret;
}

void demo_aov_mux_set_spspps(void *info)
{
    struct video_encode_spspps_info *p = (struct video_encode_spspps_info *)info;
    g_context->spspps_info.len = p->len;
    g_context->spspps_info.buf = malloc(g_context->spspps_info.len);
    if (!g_context->spspps_info.buf)
        loge("fatal error! spspps info buf alloc fail!");
    memset(g_context->spspps_info.buf, 0, g_context->spspps_info.len);
    memcpy(g_context->spspps_info.buf, p->buf, p->len);
}

void demo_aov_mux_dump_cache_info(void)
{
    pthread_mutex_lock(&g_context->lock);
    loge("mux cache nodes %u base_pts %llu current_pts %llu len %llu", g_context->list_nodes, g_context->base_pts,
        g_context->cur_pts, g_context->stream_list_len);
    pthread_mutex_unlock(&g_context->lock);
}
