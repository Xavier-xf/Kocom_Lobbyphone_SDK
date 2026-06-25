#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <linux/videodev2.h>

#include "debug.h"
#include "video_source.h"

static void *get_frame(void *arg)
{
    unsigned int frame_cnt = 0;
    struct video_source *video_src = (struct video_source *)arg;

    while (1) {
        if (frame_cnt >= 20)
            break;

        struct video_source_frame *frame = video_source_get_frame(video_src);
        if (!frame) {
            usleep(20*1000);
            continue;
        }

        /*char jpeg[128] = {0};
        sprintf(jpeg, "/mnt/extsd/jpeg/%05d.jpg", frame_cnt);
        FILE *fp = fopen(jpeg, "wb");
        if (fp) {
            fwrite(frame->buf_vir_addr, frame->data_len, 1, fp);
            fclose(fp);
            frame_cnt++;
        } else
            printf("open file %s fail!\n", jpeg);*/

        printf("video src %p get frame addr %p data len %d\n",
            video_src, frame->buf_vir_addr, frame->data_len);
        frame_cnt++;

        video_source_release_frame(video_src, frame);
    }
}

int main(int argc, char *argv[])
{
    struct video_source_base_config base_config;
    struct video_source_extra_config extra_config;
    struct video_source *video_src = NULL, *video_src_dual = NULL;

    video_src = video_source_create(VIDEO_SOURCE_TYPE_RT_MEDIA);
    memset(&extra_config, 0, sizeof(extra_config));
    extra_config.vipp = 0;
    extra_config.bitrate = 10;
    extra_config.input_fmt = VIDEO_SOURCE_PIXFMT_NV21;
    extra_config.output_fmt = VIDEO_SOURCE_PIXFMT_NV21;
    extra_config.dual_stream = 1;
    extra_config.dual_stream_vipp_dev = 4;
    extra_config.dual_stream_bufs = 3;
    video_source_set_extra_config(video_src, &extra_config);
    memset(&base_config, 0, sizeof(base_config));
    base_config.width = 640;
    base_config.height = 360;
    base_config.framerate = 20;
    base_config.format = V4L2_PIX_FMT_MJPEG;
    loge("channel %d start", extra_config.vipp);
    video_source_start(video_src, &base_config);

    video_src_dual = video_source_create(VIDEO_SOURCE_TYPE_RT_MEDIA);
    memset(&extra_config, 0, sizeof(extra_config));
    extra_config.vipp = 1;
    extra_config.bitrate = 10;
    extra_config.input_fmt = VIDEO_SOURCE_PIXFMT_NV21;
    extra_config.output_fmt = VIDEO_SOURCE_PIXFMT_NV21;
    extra_config.dual_stream = 1;
    extra_config.dual_stream_vipp_dev = 5;
    extra_config.dual_stream_bufs = 3;
    video_source_set_extra_config(video_src_dual, &extra_config);
    memset(&base_config, 0, sizeof(base_config));
    base_config.width = 640;
    base_config.height = 360;
    base_config.framerate = 20;
    base_config.format = V4L2_PIX_FMT_MJPEG;
    loge("channel %d start", extra_config.vipp);
    video_source_start(video_src_dual, &base_config);

    pthread_t video_src_trd, video_src_dual_trd;
    pthread_create(&video_src_trd, NULL, get_frame, (void *)video_src);
    pthread_create(&video_src_dual_trd, NULL, get_frame, (void *)video_src_dual);
    pthread_join(video_src_trd, NULL);
    pthread_join(video_src_dual_trd, NULL);

    video_source_stop(video_src);
    video_source_destroy(video_src);
    video_source_stop(video_src_dual);
    video_source_destroy(video_src_dual);

    return 0;
}
