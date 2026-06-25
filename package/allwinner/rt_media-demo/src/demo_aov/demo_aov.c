#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_DEBUG

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
#include <pthread.h>

#include <rt_media/AW_VideoInput_API.h>

#include "demo_aov.h"

#include <mpi_sys.h>
#include "rgb_ctrl.h"

#include "utils/include/debug.h"
#include "utils/include/option.h"
#include "utils/include/demo_aov_mux.h"
#include "utils/include/sdcard_manager.h"
#include "persion_detect/persion_detect.h"

#define DEBUG_PRINT_TO_KERNEL               (0)

#define VIDEO_SOURCE_CHANNEL                (0)
#define VIDEO_SOURCE_FRAME_WIDTH            (1280)
#define VIDEO_SOURCE_FRAME_HEIGHT           (720)
#define VIDEO_SOURCE_FRAME_RATE             (20)
#define VIDEO_SOURCE_FRAME_PIXELFORMAT      (V4L2_PIX_FMT_NV12)

#define VIDEO_ENCODE_CHANNEL                (1)
#define VIDEO_ENCODE_TYPE                   (VIDEO_ENCODE_STREAM_TYPE_H264)
#define VIDEO_ENCODE_FRAME_WIDTH            (1280)
#define VIDEO_ENCODE_FRAME_HEIGHT           (720)
#define VIDEO_ENCODE_FRAME_RATE             (20)
#define VIDEO_ENCODE_BITRATE                (5*2024*1024)

#define ONGOING_CAPTURE_DURATION            (200)

#define DEMO_AOV_ENCODE_STREAM_SAVE_PATH    "/tmp"

#define ENABLE_ENCODE                       (1)
#define ENCODE_STREAM_LIST_LEN_THRESHOLD    (1*1024*1024)
#define DEFAULT_RECORD_TYPE                 (RECORD_TYPE_FRAMELOOP)
#define DEFAULT_AOV_RECORD_FILE_SECONDS     (30*60) //30 minutes
#define DEFAULT_ONGOING_RECORD_FILE_SECONDS (10) //10 seconds
#define DEFALUT_MOTION_DETECH_REPEAT_THRESHOLD (5) //5 seconds

#define TIMESTAMP_OSD_INDEX                 (0)
#define TIMESTAMP_DEBUG_OSD_INDEX           (1)

static struct aov_demo_context *g_aov_demo_context = NULL;
static void handler_exit()
{
    int i = 0;

    loge("revice eixt signal!");
    g_aov_demo_context->task_eixt = 1;

    return ;
}

static void gpio_set(int value)
{
    if (value)
        system("echo 0x42000094 0x1ff1ffff > /sys/class/sunxi_dump/write;echo 0x420000A0 0x9000 > /sys/class/sunxi_dump/write");
    else
        system("echo 0x42000094 0x1ff1ffff > /sys/class/sunxi_dump/write;echo 0x420000A0 0x1000 > /sys/class/sunxi_dump/write");
}

static void gpio_clear(void)
{
    system("echo 0x42000094 0x1ff1ffff > /sys/class/sunxi_dump/write;echo 0x420000A0 0x1000 > /sys/class/sunxi_dump/write");
}

static int kernel_fwrite(const char *val_str)
{
    #if DEBUG_PRINT_TO_KERNEL
    FILE *stream = NULL;
    size_t len = 0;

    stream = fopen("/dev/kmsg", "w");
    if (!stream) {
        fprintf(stderr, "Cannot open: /dev/kmsg\n");
        return -EINVAL;
    }
    len = strlen(val_str);
    if (len != fwrite(val_str, 1, len, stream)) {
        fprintf(stderr, "[err] %s --->fwrite size: %d\n", val_str, (int)len);
        fclose(stream);
        return -EINVAL;
    }
    fclose(stream);
    #endif
    return 0;
}

static void suspend(int suspend_mseconds, int camera_low_pw)
{
    char cmd[256] = {0};
    if (suspend_mseconds <= 0)
        return;
    //if (camera_low_pw == CAMERA_POWER_MODE_3V3_KEEP)
    //    system("echo 3 > /sys/class/ae350_standby/standby_ldo_onoff");
    sprintf(cmd, "echo +%d > /sys/class/rtc/rtc0/wakealarm;echo mem > /sys/power/state", suspend_mseconds / 1000);
    //sprintf(cmd, "echo %d > /sys/class/ae350_standby/aov_standby_timer_ms; echo mem > /sys/power/state", suspend_mseconds);
    logv("cmd [%s]", cmd);
    system(cmd);
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

static void record_timestamp( char *lable, unsigned long long timestamp, int dump_to_kmsg)
{
    char string[128] = {0};
    sprintf(string, "%s timestamp %llu\n", lable, timestamp);
    if (dump_to_kmsg)
        kernel_fwrite(string);
    logv("%s", string);
}

static int release_stream(void *stream)
{
    int ret = 0;
    struct video_encode_frame *p = (struct video_encode_frame *)stream;
    ret = video_encode_release_frame(g_aov_demo_context->video_encode, p);
    if (ret) {
        logw("video encode release frame %d pts %llu fail!", p->id, p->pts);
    }
    return ret;
}

static void request_idr_frame(void)
{
    video_encode_request_idr(g_aov_demo_context->video_encode);
}

static int set_orl(void *orl)
{
    return video_source_set_orl(g_aov_demo_context->video_source, orl);
}

static int set_osd(void *osd)
{
    return video_encode_set_osd(g_aov_demo_context->video_encode, osd);
}

static void persion_detect_notify(Awnn_Result_t *result)
{
    for (int i = 0; i < result->valid_cnt; i++) {
        logv("persion detect notify persion %d lable %d prob %f position %d-%d-%d-%d",
            i, result->boxes[i].label, result->boxes[i].score,
            result->boxes[i].xmin, result->boxes[i].ymin,
            result->boxes[i].xmax, result->boxes[i].ymax);
    }
    pthread_mutex_lock(&g_aov_demo_context->lock);
    if ((GetSysTimeUsMonotonic() - g_aov_demo_context->algo_detected_timestamp) >= 2*1000*1000)
        logv("fine! it's secure now!");
    else {
        g_aov_demo_context->algo_detected_timestamp -= 2*1000*1000;
        logd("algo detected timestamp decrease 2 seconds");
    }
    pthread_mutex_unlock(&g_aov_demo_context->lock);
}

static void aov_demo_context_stream_init(struct aov_demo_context *context)
{
    /* yuv source */
    context->video_source = video_source_create(VIDEO_SOURCE_TYPE_RT_MEDIA);
    if (!context->video_source) {
        loge("video source create fail!");
        return;
    }
    struct video_source_extra_config extra_config;
    memset(&extra_config, 0, sizeof(extra_config));
    extra_config.vipp = VIDEO_SOURCE_CHANNEL;
    extra_config.input_fmt = VIDEO_SOURCE_FRAME_PIXELFORMAT;
    extra_config.output_fmt = VIDEO_SOURCE_FRAME_PIXELFORMAT;
    extra_config.camera_low_pw = context->config.camera_low_pw;
    video_source_set_extra_config(context->video_source, &extra_config);
    struct video_source_base_config base_config;
    memset(&base_config, 0, sizeof(base_config));
    base_config.width = VIDEO_SOURCE_FRAME_WIDTH;
    base_config.height = VIDEO_SOURCE_FRAME_HEIGHT;
    base_config.framerate = VIDEO_SOURCE_FRAME_RATE;
    base_config.format = VIDEO_SOURCE_FRAME_PIXELFORMAT;
    video_source_start(context->video_source, &base_config);
    if (context->config.camera_low_pw) {
        struct sensor_lowpw_cfg cfg;
        memset(&cfg, 0, sizeof(cfg));
        cfg.lowpw_en = 1;
        cfg.frame_mode = SENSOR_ONE_FRAME;
        video_source_set_camera_lowpw_mode(context->video_source, &cfg);
    }

    /* video encode */
    #if ENABLE_ENCODE
    context->video_encode = video_encode_create(VIDEO_ENCODE_TYPE_RT_MEDIA);
    if (!context->video_encode) {
        loge("video encode create fail!");
        return;
    }
    struct video_encode_config encode_config;
    memset(&encode_config, 0, sizeof(encode_config));
    encode_config.channel = VIDEO_ENCODE_CHANNEL;
    encode_config.format = VIDEO_ENCODE_TYPE;
    encode_config.framerate = VIDEO_ENCODE_FRAME_RATE;
    encode_config.width = VIDEO_ENCODE_FRAME_WIDTH;
    encode_config.height = VIDEO_ENCODE_FRAME_HEIGHT;
    encode_config.bitrate = VIDEO_ENCODE_BITRATE;
    video_encode_start(context->video_encode, &encode_config);

    struct demo_aov_mux_config config;
    memset(&config, 0, sizeof(config));
    config.width = VIDEO_ENCODE_FRAME_WIDTH;
    config.height = VIDEO_ENCODE_FRAME_HEIGHT;
    config.framerate = VIDEO_ENCODE_FRAME_RATE;
    config.max_key_interval = VIDEO_ENCODE_FRAME_RATE;
    config.file_duration = DEFAULT_AOV_RECORD_FILE_SECONDS;
    config.ongoing_duration = context->config.ongoing_duration;
    config.max_cache_len = ENCODE_STREAM_LIST_LEN_THRESHOLD;
    config.record_type = context->config.record_type;
    config.release_stream_callback = release_stream;
    config.request_idr_frame = request_idr_frame;
    demo_aov_mux_init(&config);
    struct video_encode_spspps_info spspps_info;
    memset(&spspps_info, 0, sizeof(spspps_info));
    video_encode_get_spspps_info(context->video_encode, &spspps_info);
    demo_aov_mux_set_spspps((void *)&spspps_info);
    #endif
}

static void aov_demo_context_stream_deinint(struct aov_demo_context *context)
{
    video_source_stop(context->video_source);
    video_source_destroy(context->video_source);

#if ENABLE_ENCODE
    demo_aov_mux_destroy();
    video_encode_stop(context->video_encode);
    video_encode_destroy(context->video_encode);
#endif
}

static void config_put_encode_frame(struct video_source_frame *src, struct video_encode_frame *dst)
{
    dst->len[0] = src->data_len;
    dst->vir_addr[0] = src->buf_vir_addr;
    dst->phy_addr[0] = src->buf_phy_addr;
    dst->width = src->width;
    dst->height = src->height;
    dst->pts = src->pts;
}

static int check_is_motion(struct aov_demo_context *context)
{
    if (!context->config.ms || context->algo_detected)
        return 0;

    int motion_region_num = 0;
    struct motion_search_result result;
    memset(&result, 0, sizeof(result));
    video_encode_get_motion_search_result(context->video_encode, &result);
    for (int i = 0; i < result.total_region_num; i++) {
        logv("area_%d: is_motion %d [(%d,%d),(%d,%d)]", i, result.region[i].is_motion,
           result.region[i].pix_x_bgn, result.region[i].pix_y_bgn,
           result.region[i].pix_x_end, result.region[i].pix_y_end);
	    if (result.region[i].is_motion)
		    motion_region_num++;
    }
    logd("detect motion region %d!", motion_region_num);
    return motion_region_num >= context->config.mst ? 1 : 0;
}

static int check_is_pdet(struct aov_demo_context *context)
{
    int result = 0;
    if (!context->config.pdet || context->algo_detected)
        return 0;

    Awnn_Result_t detect_result;
    memset(&detect_result, 0, sizeof(detect_result));
    persion_detect_result_get(&detect_result);
    for (int i = 0; i < detect_result.valid_cnt; i++) {
        if (detect_result.boxes[i].score > 0.3)
            result = 1;
    }
    return result;
}

static void draw_timestamp_osd(struct aov_demo_context *context, unsigned long long timestamp)
{
    time_t timep;
    time(&timep);
    char string[256] = {0};

    FONT_RGBPIC_S font_pic;
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

    if (context->config.attach_debug_osd) {
        sprintf(string, "%llu", timestamp);
        RGB_PIC_S debug_rgb_pic;
        memset(&debug_rgb_pic, 0, sizeof(debug_rgb_pic));
        debug_rgb_pic.rgb_type = OSD_RGB_32;
        debug_rgb_pic.enable_mosaic = 0;
        create_font_rectangle(string, &font_pic, &debug_rgb_pic);
        struct video_encode_osd_item osd_item;
        memset(&osd_item, 0, sizeof(osd_item));
        osd_item.index = TIMESTAMP_DEBUG_OSD_INDEX;
        osd_item.enable = 1;
        osd_item.x = 64;
        osd_item.y = 16 + 32;
        osd_item.w = debug_rgb_pic.wide;
        osd_item.h = debug_rgb_pic.high;
        osd_item.data_buf = (unsigned char *)debug_rgb_pic.pic_addr;
        osd_item.data_size = debug_rgb_pic.pic_size;
        osd_item.osd_type = LUMA_REVERSE_OVERLAY;
        int ret = video_encode_set_osd(context->video_encode, (void *)&osd_item);
        if (ret)
            loge("video encode set osd fail!");
        release_rgb_picture(&debug_rgb_pic);
    }

    if (context->algo_detected && ((timep - context->prev_timestamp) < 1))
	    return;

    struct tm *p = localtime(&timep);
    sprintf(string, "%04d/%02d/%02d %02d:%02d:%02d", (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour,
        p->tm_min, p->tm_sec);
    RGB_PIC_S rgb_pic;
    memset(&rgb_pic, 0, sizeof(rgb_pic));
    rgb_pic.rgb_type = OSD_RGB_32;
    rgb_pic.enable_mosaic = 0;
    create_font_rectangle(string, &font_pic, &rgb_pic);
    struct video_encode_osd_item osd_item;
    memset(&osd_item, 0, sizeof(osd_item));
    osd_item.index = TIMESTAMP_OSD_INDEX;
    osd_item.enable = 1;
    osd_item.x = 64;
    osd_item.y = 16;
    osd_item.w = rgb_pic.wide;
    osd_item.h = rgb_pic.high;
    osd_item.data_buf = (unsigned char *)rgb_pic.pic_addr;
    osd_item.data_size = rgb_pic.pic_size;
    osd_item.osd_type = LUMA_REVERSE_OVERLAY;
    int ret = video_encode_set_osd(context->video_encode, (void *)&osd_item);
    if (ret)
        loge("video encode set osd fail!");
    release_rgb_picture(&rgb_pic);
    context->prev_timestamp = timep;
}

static void set_encode_sharp_param(struct aov_demo_context *context)
{
    sEncppSharpParam sharp_param;
    memset(&sharp_param, 0, sizeof(sharp_param));
    video_source_get_sharp_param(context->video_source, (void *)&sharp_param);
    video_encode_set_sharp_param(context->video_encode, (void *)&sharp_param);
}

static int encode_and_mux(struct aov_demo_context *context, struct video_source_frame *yuv_frame)
{
    int ret;
    int get_encode_frame_max_cnt;

    draw_timestamp_osd(context, yuv_frame->pts);
    struct video_encode_frame encode_put_frame;
    memset(&encode_put_frame, 0, sizeof(encode_put_frame));
    config_put_encode_frame(yuv_frame, &encode_put_frame);
    ret = video_encode_put_frame(context->video_encode, &encode_put_frame);
    if (!ret) {
        kernel_fwrite("app put yuv frame to rt-media ve end\n");
        get_encode_frame_max_cnt = 0;
        while (1) {
            if (get_encode_frame_max_cnt >= 5) {
                logw("get encode frame fail!");
                break;
            }
            struct video_encode_frame encode_frame;
            memset(&encode_frame, 0, sizeof(encode_frame));
            ret = video_encode_get_frame(context->video_encode, &encode_frame);
            if (!ret) {
                demo_aov_mux_add_stream((void *)&encode_frame);
                break;
            } else {
                logw("get encode stream fail!");
            }
            get_encode_frame_max_cnt++;
            usleep(10*1000);
        }
    }
    return ret;
}

static int check_grant_to_suspend(struct aov_demo_context *context)
{
    struct sensor_lowpw_cfg cfg;

    if (context->config.suspend_ms <= 0)
        return 0;

    /*#if ENABLE_ENCODE
    if ( check_is_motion(context)) {
        context->algo_detected = 1;
        context->algo_detected_timestamp = GetSysTimeUsMonotonic();
        video_encode_request_idr(context->video_encode);
        if (context->config.camera_low_pw) {
            memset(&cfg, 0, sizeof(cfg));
            cfg.lowpw_en = 1;
            cfg.frame_mode = SENSOR_MULTI_FRAME;
            video_source_set_camera_lowpw_mode(context->video_source, (void *)&cfg);
        }
        if (context->config.pdet)
            persion_detect_start_async();
        logd("motion detected timestamp %llu", context->algo_detected_timestamp);
        grant = 0;
    }
    #endif*/
    if (check_is_pdet(context)) {
        context->algo_detected = 1;
        pthread_mutex_lock(&context->lock);
        context->algo_detected_timestamp = GetSysTimeUsMonotonic();
        pthread_mutex_unlock(&context->lock);
        video_encode_request_idr(context->video_encode);
        if (context->config.camera_low_pw) {
            memset(&cfg, 0, sizeof(cfg));
            cfg.lowpw_en = 1;
            cfg.frame_mode = SENSOR_MULTI_FRAME;
            video_source_set_camera_lowpw_mode(context->video_source, (void *)&cfg);
        }
        if (context->config.pdet)
            persion_detect_start_async();
        logd("persion detected timestamp %llu", context->algo_detected_timestamp);
        kernel_fwrite("app check is pdet done\n");
    }
    if (context->algo_detected) {
        pthread_mutex_lock(&context->lock);
        unsigned long long ongong_duration =
            GetSysTimeUsMonotonic() - context->algo_detected_timestamp;
        pthread_mutex_unlock(&context->lock);
        if ((ongong_duration) / 1000000 >= DEFAULT_ONGOING_RECORD_FILE_SECONDS) {
            context->algo_detected = 0;
            if (context->config.camera_low_pw) {
                memset(&cfg, 0, sizeof(cfg));
                cfg.lowpw_en = 1;
                cfg.frame_mode = SENSOR_ONE_FRAME;
                video_source_set_camera_lowpw_mode(context->video_source, (void *)&cfg);
            }
            if (context->config.pdet)
                persion_detect_stop_async();
            logv("motion detected ongong complete");
        }
    }

    return context->algo_detected ? 0 : 1;
}

static void enter_suspend(struct aov_demo_context *context)
{
    suspend(context->config.suspend_ms, context->config.camera_low_pw);
}

static void wait_mux_complete(struct aov_demo_context *context)
{
    int ret;

    if (!context->algo_detected && (context->config.suspend_ms > 0)) {
        ret = demo_aov_mux_wait_complete(500*100);
        if (ret == ETIMEDOUT) {
            logw("wait mux complete timeout!");
        }
    }
}

static void *task_proc(void *arg)
{
    int ret;
    struct aov_demo_context *aov_demo_context = (struct aov_demo_context *)arg;
    int get_encode_frame_max_cnt;
    char kernel_str[128] = {0};
    unsigned long long process_start, process_end, frame_cnt = 0, suspend_cnt = 0;
    struct video_source_frame yuv_frame;
    int wait_mux_timeout_ms;
    unsigned int drop_frames = aov_demo_context->config.drop_frames;
    int grant_to_suspend = (aov_demo_context->config.suspend_ms > 0) ? 1 : 0;

    load_font_file(FONT_SIZE_32);

    if (aov_demo_context->config.pdet && (aov_demo_context->config.suspend_ms <= 0))
        persion_detect_start_async();

    logv("recive %d frame then exit", aov_demo_context->config.encode_frame_cnt);
    while (1) {
        if (aov_demo_context->task_eixt) {
            logd("task proc exit");
            break;
        }
        if (aov_demo_context->config.encode_frame_cnt && (frame_cnt >= aov_demo_context->config.encode_frame_cnt)) {
            logd("task proc exit");
            break;
        }

        process_start = GetSysTimeUsMonotonic();
        record_timestamp("app process start", process_start, 1);

        /*#if ENABLE_ENCODE
        demo_aov_mux_check_cache(aov_demo_context->algo_detected);
        #endif*/

        if (aov_demo_context->config.pdet && grant_to_suspend)
            persion_detect_start();

        _retry:
        if (aov_demo_context->task_eixt) {
            logd("task proc exit");
            break;
        }
        memset(&yuv_frame, 0, sizeof(yuv_frame));
        ret = video_source_get_frame(aov_demo_context->video_source, &yuv_frame);
        if (ret) {
            logw("video source get frame fail!");
            goto _retry;
        }
        if (drop_frames) {
            drop_frames--;
            set_encode_sharp_param(aov_demo_context);
            video_source_release_frame(aov_demo_context->video_source, &yuv_frame);
            goto _retry;
        }
        record_timestamp("app get yuv frame", GetSysTimeUsMonotonic(), 1);

        #if ENABLE_ENCODE
        encode_and_mux(aov_demo_context, &yuv_frame);
        record_timestamp("app encode yuv frame", GetSysTimeUsMonotonic(), 1);
        #endif

        video_source_release_frame(aov_demo_context->video_source, &yuv_frame);
        record_timestamp("app release yuv frame", GetSysTimeUsMonotonic(), 1);

        grant_to_suspend = check_grant_to_suspend(aov_demo_context);

        #if ENABLE_ENCODE
        wait_mux_complete(aov_demo_context);
        #endif

        if (grant_to_suspend) {
            process_end = GetSysTimeUsMonotonic();
            record_timestamp("app process end", process_end, 1);
            suspend_cnt++;
            logd("suspend cnt %llu app process timeuse %lluus", suspend_cnt, process_end - process_start);
            enter_suspend(aov_demo_context);
        } else
            set_encode_sharp_param(aov_demo_context);
        frame_cnt++;
    }

    if (aov_demo_context->config.pdet && (aov_demo_context->config.suspend_ms <= 0))
        persion_detect_stop_async();

    unload_font_file(FONT_SIZE_32);

    return (void *)NULL;
}

int main(int argc, char *argv[])
{
    kernel_fwrite("demo aov start");

    struct aov_demo_context *aov_demo_context = malloc(sizeof(*aov_demo_context));
    if (!aov_demo_context)
        loge("alloc demo standby obj fail!");
    memset(aov_demo_context, 0, sizeof(*aov_demo_context));
    g_aov_demo_context = aov_demo_context;
    signal(SIGINT, handler_exit);
    pthread_mutex_init(&aov_demo_context->lock, NULL);

    int ret = parser_cmdline_param(argc, argv, &aov_demo_context->config);
    if (ret < 0)
        goto _exit;
    if (!aov_demo_context->config.ongoing_duration)
        aov_demo_context->config.ongoing_duration = DEFAULT_ONGOING_RECORD_FILE_SECONDS;
    logd("ongong record duration %d seconds", aov_demo_context->config.ongoing_duration);

    MPP_SYS_CONF_S sys_conf;
    memset(&sys_conf, 0, sizeof(sys_conf));
    sys_conf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&sys_conf);
    AW_MPI_SYS_Init();
    AWVideoInput_Init();

    aov_demo_context_stream_init(aov_demo_context);
    if (aov_demo_context->config.pdet) {
        unsigned long long init_t = GetSysTimeUsMonotonic();
        struct persion_detect_config config;
        memset(&config, 0, sizeof(config));
        config.callback.set_osd = set_osd;
        config.callback.set_orl = set_orl;
        config.callback.notify = persion_detect_notify;
        config.attach_debug_osd = aov_demo_context->config.attach_debug_osd;
        persion_detect_init(&config);
        unsigned long long init_end = GetSysTimeUsMonotonic();
        logd("persion detect init %llu", init_end - init_t);
    }
    pthread_create(&aov_demo_context->task_trd, NULL, task_proc, (void *)aov_demo_context);

    pthread_join(aov_demo_context->task_trd, NULL);
    if (aov_demo_context->config.pdet)
        persion_detect_destroy();
    aov_demo_context_stream_deinint(aov_demo_context);

    AW_MPI_SYS_Exit();
    AWVideoInput_DeInit();

_exit:
    pthread_mutex_destroy(&aov_demo_context->lock);
    if (aov_demo_context)
        free(aov_demo_context);
    return ret;
}
