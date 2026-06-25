#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/video_encode.h"
#include "../../utils/include/debug.h"

extern const struct video_encode_ops video_encode_rt_media_ops;

int video_encode_start(struct video_encode *video_encode, struct video_encode_config *config)
{
    return video_encode->ops->start(video_encode, config);
}

int video_encode_stop(struct video_encode *video_encode)
{
    return video_encode->ops->stop(video_encode);
}

int video_encode_put_frame(struct video_encode * video_encode, struct video_encode_frame * frame)
{
    return video_encode->ops->put_frame(video_encode, frame);
}

int video_encode_get_frame(struct video_encode *video_encode, struct video_encode_frame *frame)
{
    return video_encode->ops->get_frame(video_encode, frame);
}

int video_encode_release_frame(struct video_encode *video_encode, struct video_encode_frame *frame)
{
    return video_encode->ops->release_frame(video_encode, frame);
}

struct video_encode *video_encode_create(enum video_encode_type video_encode_type)
{
    struct video_encode *video_encode = malloc(sizeof(struct video_encode));
    if (!video_encode)
        loge("create video source fail!\n");
    memset(video_encode, 0, sizeof(*video_encode));

    switch (video_encode_type) {
    case VIDEO_ENCODE_TYPE_RT_MEDIA:
        video_encode->ops = &video_encode_rt_media_ops;
        break;
    default:
        loge("unsupport video_encode_type 0x%x", video_encode_type);
        return NULL;
    };
    video_encode->ops->create((void *)video_encode);

    return video_encode;
}

void video_encode_destroy(struct video_encode *video_encode)
{
    if (!video_encode)
        return;
    if (video_encode->ops) {
        video_encode->ops->destroy((void *)video_encode);
    }
    free(video_encode);
}

void video_encode_get_motion_search_result(struct video_encode *video_encode, struct motion_search_result *result)
{
    return video_encode->ops->get_motion_search_result(video_encode, result);
}

int video_encode_request_idr(struct video_encode *video_encode)
{
    return video_encode->ops->request_idr(video_encode);
}

int video_encode_pause(struct video_encode *video_encode, int flag)
{
    return video_encode->ops->pause(video_encode, flag);
}

int video_encode_get_spspps_info(struct video_encode *video_encode, struct video_encode_spspps_info *info)
{
    return video_encode->ops->get_spspps_info(video_encode, info);
}

int video_encode_set_osd(struct video_encode *video_encode, void *bmp)
{
    return video_encode->ops->set_osd(video_encode, bmp);
}

int video_encode_set_sharp_param(struct video_encode *video_encode, void *param)
{
    return video_encode->ops->set_sharp_param(video_encode, param);
}