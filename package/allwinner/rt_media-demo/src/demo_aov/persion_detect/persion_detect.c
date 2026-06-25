#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_DEBUG

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <linux/videodev2.h>

#include "rgb_ctrl.h"
#include "aw_message_queue.h"
#include "media/rt-media/uapi_rt_media.h"
#include "../utils/include/debug.h"
#include "../video_source/base/include/video_source.h"
#include "../video_encode/base/include/video_encode.h"

#include "persion_detect.h"

#define VIDEO_SOURCE_CHANNEL                (4)
#define VIDEO_SOURCE_FRAME_WIDTH            (320)
#define VIDEO_SOURCE_FRAME_HEIGHT           (192)
#define VIDEO_SOURCE_FRAME_RATE             (20)
#define VIDEO_SOURCE_FRAME_PIXELFORMAT      (V4L2_PIX_FMT_NV12)

#define PDET_INPUT_W                        (320)
#define PDET_INPUT_H                        (192)
#define PDET_INPUT_C                        (3)
#define PDET_MODEL_FILE                     "/lib/1.1.0_Beta.nb"

#define DEFAULT_WAIT_DETECT_TIMEOUT         (500) //500ms
#define PERSION_DETECT_TASK_EXIT_MSG        (100)
#define PERSION_DETECT_TASK_START_MSG       (200)
#define PERSION_DETECT_TASK_ASYNC_MSG       (300)

#define VIDEO_MAIN_FRAME_WIDTH              (1280)
#define VIDEO_MAIN_FRAME_HEIGHT             (720)
#define PDET_DEBUG_OSD_START_INDEX          (2)

struct persion_detect_context {
    SamplePdetInfo pdet_info;
    pthread_t persion_detect_task_trd;
    struct video_source *video_source;
    int exit;
    int wait_flag;
    pthread_mutex_t lock;
    pthread_cond_t condition;
    pthread_condattr_t cond_attr;
    int detect_result_update;

    AwRtMessageQueue *msg_queue;
    int osd_index;
    struct persion_detect_config config;
};
static struct persion_detect_context *context;

static unsigned long long GetSysTimeUsMonotonic()
{
    long long curr;
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = ((unsigned long long)(t.tv_sec)*1000000000LL + t.tv_nsec)/1000LL;
    return (unsigned long long)curr;
}

static void draw_debug_osd(
    struct persion_detect_context *context,
    int x1, int y1, int x2, int y2, int lable, float prob,
    unsigned long long timestamp)
{
    int index = PDET_DEBUG_OSD_START_INDEX;
    char string[128] = {0};
    FONT_RGBPIC_S font_pic;
    struct video_encode_osd_item osd_item;
    RGB_PIC_S result_rgb, position_rgb, timestamp_rgb;

    memset(&font_pic, 0, sizeof(font_pic));
    font_pic.font_type = FONT_SIZE_32;
    font_pic.rgb_type = OSD_RGB_32;
    font_pic.enable_bg = 0;
    font_pic.foreground[0] = 0xFF;
    font_pic.foreground[1] = 0xFF;
    font_pic.foreground[2] = 0xFF;
    font_pic.foreground[3] = 0xFF;
    font_pic.background[0] = 0x88;
    font_pic.background[0] = 0x88;
    font_pic.background[0] = 0x88;
    font_pic.background[0] = 0x88;

    sprintf(string, "lable: %d prob: %f", lable, prob);
    memset(&result_rgb, 0, sizeof(result_rgb));
    result_rgb.rgb_type = OSD_RGB_32;
    result_rgb.enable_mosaic = 0;
    create_font_rectangle(string, &font_pic, &result_rgb);
    memset(&osd_item, 0, sizeof(osd_item));
    osd_item.index = index++;
    osd_item.enable = 1;
    osd_item.x = x1;
    osd_item.y = y1;
    osd_item.w = result_rgb.wide;
    osd_item.h = result_rgb.high;
    osd_item.data_buf = (unsigned char *)result_rgb.pic_addr;
    osd_item.data_size = result_rgb.pic_size;
    osd_item.osd_type = LUMA_REVERSE_OVERLAY;
    context->config.callback.set_osd((void *)&osd_item);
    release_rgb_picture(&result_rgb);

    sprintf(string, "%d,%d %d,%d", x1, y1, x2, y2);
    memset(&position_rgb, 0, sizeof(position_rgb));
    position_rgb.rgb_type = OSD_RGB_32;
    position_rgb.enable_mosaic = 0;
    create_font_rectangle(string, &font_pic, &position_rgb);
    memset(&osd_item, 0, sizeof(osd_item));
    osd_item.index = index++;
    osd_item.enable = 1;
    osd_item.x = x1;
    osd_item.y = y1 + (y2-y1)/2;
    osd_item.w = position_rgb.wide;
    osd_item.h = position_rgb.high;
    osd_item.data_buf = (unsigned char *)position_rgb.pic_addr;
    osd_item.data_size = position_rgb.pic_size;
    osd_item.osd_type = LUMA_REVERSE_OVERLAY;
    context->config.callback.set_osd((void *)&osd_item);
    release_rgb_picture(&position_rgb);

    sprintf(string, "%llu", timestamp);
    memset(&timestamp_rgb, 0, sizeof(timestamp_rgb));
    timestamp_rgb.rgb_type = OSD_RGB_32;
    timestamp_rgb.enable_mosaic = 0;
    create_font_rectangle(string, &font_pic, &timestamp_rgb);
    memset(&osd_item, 0, sizeof(osd_item));
    osd_item.index = index++;
    osd_item.enable = 1;
    osd_item.x = x1;
    osd_item.y = y2;
    osd_item.w = timestamp_rgb.wide;
    osd_item.h = timestamp_rgb.high;
    osd_item.data_buf = (unsigned char *)timestamp_rgb.pic_addr;
    osd_item.data_size = timestamp_rgb.pic_size;
    osd_item.osd_type = LUMA_REVERSE_OVERLAY;
    context->config.callback.set_osd((void *)&osd_item);
    release_rgb_picture(&timestamp_rgb);

    context->osd_index = index;
}

static int draw_orl(struct persion_detect_context *context, unsigned long long timestamp)
{
    int i;
    static int cnt;
    RTIspOrl isp_orl;
    int x1, y1, x2, y2;
    Awnn_Result_t *result = &context->pdet_info.detect_result;

    if (result->valid_cnt > 0) {
        memset(&isp_orl, 0, sizeof(RTIspOrl));
        isp_orl.on = 1;
        isp_orl.orl_width = 1;
        isp_orl.orl_cnt = result->valid_cnt;
        for (i = 0; i < result->valid_cnt; i++) {
            x1 = result->boxes[i].xmin * (VIDEO_MAIN_FRAME_WIDTH / PDET_INPUT_W);
            y1 = result->boxes[i].ymin * (VIDEO_MAIN_FRAME_HEIGHT / PDET_INPUT_H);
            x2 = result->boxes[i].xmax * (VIDEO_MAIN_FRAME_WIDTH / PDET_INPUT_W);
            y2 = result->boxes[i].ymax * (VIDEO_MAIN_FRAME_HEIGHT / PDET_INPUT_H);
            isp_orl.orl_win[i].left = x1;
            isp_orl.orl_win[i].top = y1;
            isp_orl.orl_win[i].width = x2 - x1;
            isp_orl.orl_win[i].height = y2 - y1;
            isp_orl.orl_win[i].rgb_orl = 0xff0000 >> ((i % 3) * 8);
            if (context->config.callback.set_orl)
                context->config.callback.set_orl((void *)&isp_orl);
            if (context->config.callback.set_osd)
                draw_debug_osd(context, x1, y1, x2, y2, result->boxes[i].label, result->boxes[i].score, timestamp);
        }
    } else {
        memset(&isp_orl, 0, sizeof(RTIspOrl));
        cnt = 0;
        isp_orl.on = 0;
        isp_orl.orl_cnt = 5;
        isp_orl.orl_width = 1;
        if (context->config.callback.set_orl)
            context->config.callback.set_orl((void *)&isp_orl);
        for (i = context->osd_index; i >= PDET_DEBUG_OSD_START_INDEX; i--) {
            struct video_encode_osd_item osd_item;
            memset(&osd_item, 0, sizeof(osd_item));
            osd_item.index = i;
            osd_item.enable = 0;
            if (context->config.callback.set_osd)
                context->config.callback.set_osd((void *)&osd_item);
        }
    }
    return 0;
}

static void *persion_detect_task(void *arg)
{
    int ret;
    int async_process = 0;
    unsigned long long pdet_start, whole_end;
    struct persion_detect_context *context = (struct persion_detect_context *)arg;

    load_font_file(FONT_SIZE_32);

    while (1) {
        AwRtMessage msg;
        memset(&msg, 0, sizeof(msg));
        ret = aw_message_queue_tryGetMessage(context->msg_queue, &msg, 0);
        if ((ret < 0) && !async_process) {
            aw_message_queue_waitMessage(context->msg_queue, -1);
            continue;
        }

        if (msg.messageId == PERSION_DETECT_TASK_EXIT_MSG) {
            break;
        } else if (msg.messageId == PERSION_DETECT_TASK_ASYNC_MSG) {
            async_process = msg.para0;
            if (!async_process) {
                if (msg.pReply)
                    awrt_sem_up(&msg.pReply->ReplySem);
                continue;
            }
        }

        unsigned long long start = GetSysTimeUsMonotonic();
        struct video_source_frame frame;
        memset(&frame, 0, sizeof(frame));
        ret = video_source_get_frame(context->video_source, &frame);
        if (ret) {
            continue;
        }

        pdet_start = GetSysTimeUsMonotonic();
        SamplePdetInputFrameInfo input_frame;
        memset(&input_frame, 0, sizeof(input_frame));
        input_frame.width = frame.width;
        input_frame.height = frame.height;
        input_frame.pix_format = frame.pix_fmt;
        input_frame.vir_addr[0] = frame.buf_vir_addr;
        input_frame.phy_addr[0] = frame.buf_phy_addr;
        input_frame.vir_addr[1] = frame.buf_vir_addr + (frame.width * frame.height);
        input_frame.phy_addr[1] = frame.buf_phy_addr + (frame.width * frame.height);
        pdet_run(&context->pdet_info, &input_frame);
        logv("pdet run result num %d", context->pdet_info.detect_result.valid_cnt);
        whole_end = GetSysTimeUsMonotonic();

        if (async_process && context->config.callback.notify)
            context->config.callback.notify(&context->pdet_info.detect_result);

        if (context->config.attach_debug_osd)
            draw_orl(context, frame.pts);

        pthread_mutex_lock(&context->lock);
        context->detect_result_update = 1;
        if (context->wait_flag && !async_process) {
            context->wait_flag = 0;
            pthread_cond_signal(&context->condition);
        }
        pthread_mutex_unlock(&context->lock);

        video_source_release_frame(context->video_source, &frame);

        logv("persion detect timeuse %lluus", pdet_start - whole_end);
    }

    unload_font_file(FONT_SIZE_32);

    return (void *)NULL;
}

void persion_detect_start_async(void)
{
    AwRtMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.messageId = PERSION_DETECT_TASK_ASYNC_MSG;
    msg.para0 = 1;
    aw_message_queue_postMessage(context->msg_queue, &msg);
}

void persion_detect_stop_async(void)
{
    int ret;
    AwRtMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.messageId = PERSION_DETECT_TASK_ASYNC_MSG;
    msg.para0 = 0;
    msg.pReply = CreateAwRtMessageReply();
    ret = aw_message_queue_postMessage(context->msg_queue, &msg);
    if (ret != 0) {
        loge("fatal error! stop async fail!");
        return;
    }
    ret = awrt_sem_down_timedwait(&msg.pReply->ReplySem, DEFAULT_WAIT_DETECT_TIMEOUT);
    if (ret != 0) {
        loge("fatal error! stop async wait reply fail!");
    }
    ret = msg.pReply->ReplyResult;
    logv("stop async recive reply ret: 0x%x!", ret);
    DeleteAwRtMessageReply(msg.pReply);
    msg.pReply = NULL;
}

int persion_detect_result_get(Awnn_Result_t *result)
{
    int ret, relative_sec, relative_nsec;
    struct timespec ts;

    _retry:
    pthread_mutex_lock(&context->lock);
    if (context->detect_result_update) {
        memcpy(result, &context->pdet_info.detect_result, sizeof(context->pdet_info.detect_result));
        context->detect_result_update = 0;
    } else {
        context->wait_flag = 1;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        relative_sec = DEFAULT_WAIT_DETECT_TIMEOUT/1000;
        relative_nsec = (DEFAULT_WAIT_DETECT_TIMEOUT%1000)*1000000;
        ts.tv_sec += relative_sec;
        ts.tv_nsec += relative_nsec;
        ts.tv_sec += ts.tv_nsec/(1000*1000*1000);
        ts.tv_nsec = ts.tv_nsec%(1000*1000*1000);
        ret = pthread_cond_timedwait(&context->condition, &context->lock, &ts);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&context->lock);
            loge("wait persion detect timeout!");
            return -1;
        } else if (ret == 0) {
            pthread_mutex_unlock(&context->lock);
            logv("wait persion detect done");
            goto _retry;
        } else {
            pthread_mutex_unlock(&context->lock);
            loge("wait persion detect error!");
            return -1;
        }
    }
    pthread_mutex_unlock(&context->lock);
    return 0;
}

void persion_detect_start(void)
{
    AwRtMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.messageId = PERSION_DETECT_TASK_START_MSG;
    aw_message_queue_postMessage(context->msg_queue, &msg);
}

int persion_detect_init(struct persion_detect_config *config)
{
    int ret;

    context = malloc(sizeof(*context));
    if (!context) {
        loge("persion detect context malloc fail!");
        return -1;
    }
    memset(context, 0, sizeof(*context));
    memcpy(&context->config, config, sizeof(*config));

    context->video_source = video_source_create(VIDEO_SOURCE_TYPE_RT_MEDIA);
    if (!context->video_source) {
        loge("video source create fail!");
        goto _video_source_create_fail;
    }
    struct video_source_extra_config extra_config;
    memset(&extra_config, 0, sizeof(extra_config));
    extra_config.vipp = VIDEO_SOURCE_CHANNEL;
    extra_config.input_fmt = VIDEO_SOURCE_FRAME_PIXELFORMAT;
    extra_config.output_fmt = VIDEO_SOURCE_FRAME_PIXELFORMAT;
    video_source_set_extra_config(context->video_source, &extra_config);
    struct video_source_base_config base_config;
    memset(&base_config, 0, sizeof(base_config));
    base_config.width = VIDEO_SOURCE_FRAME_WIDTH;
    base_config.height = VIDEO_SOURCE_FRAME_HEIGHT;
    base_config.framerate = VIDEO_SOURCE_FRAME_RATE;
    base_config.format = VIDEO_SOURCE_FRAME_PIXELFORMAT;
    video_source_start(context->video_source, &base_config);

	memset(&context->pdet_info, 0, sizeof(SamplePdetInfo));
	context->pdet_info.pdet_conf_thres = 0.3;
	context->pdet_info.pdet_input_w = PDET_INPUT_W;
	context->pdet_info.pdet_input_h = PDET_INPUT_H;
	context->pdet_info.pdet_input_c = PDET_INPUT_C;
    strncpy(context->pdet_info.pdet_model_filename, PDET_MODEL_FILE, strlen(PDET_MODEL_FILE));
	ret = pdet_init(&context->pdet_info);
	if (ret < 0)
	{
		loge("pdet_init failed!");
		goto _pdet_init_fail;
	}

    pthread_condattr_init(&context->cond_attr);
    pthread_condattr_setclock(&context->cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&context->condition, &context->cond_attr);
    pthread_mutex_init(&context->lock, NULL);

    context->msg_queue = aw_message_queue_create(16, "demo_aov_persion_detect");
    if (!context->msg_queue)
        loge("message queue create fail!");

    pthread_create(&context->persion_detect_task_trd, NULL, persion_detect_task, (void *)context);

    return 0;
_pdet_init_fail:
_de_init_fail:
    video_source_stop(context->video_source);
    video_source_destroy(context->video_source);
    context->video_source = NULL;
_video_source_create_fail:
    free(context);
    context = NULL;
    return -1;
}

void persion_detect_destroy(void)
{
    context->exit = 1;
    AwRtMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.messageId = PERSION_DETECT_TASK_EXIT_MSG;
    aw_message_queue_postMessage(context->msg_queue, &msg);
    pthread_join(context->persion_detect_task_trd, NULL);

    pthread_cond_destroy(&context->condition);
    pthread_mutex_destroy(&context->lock);
    pthread_condattr_destroy(&context->cond_attr);

    aw_message_queue_destroy(context->msg_queue);

    pdet_deinit(&context->pdet_info);

    video_source_stop(context->video_source);
    video_source_destroy(context->video_source);

    free(context);
    context = NULL;
}
