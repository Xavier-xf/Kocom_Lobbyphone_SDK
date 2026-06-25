#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/video_source.h"
#include "../../utils/include/debug.h"

extern const struct video_source_ops video_source_rt_media_ops;

int video_source_start(struct video_source *video_source, struct video_source_base_config *config)
{
    return video_source->ops->start(video_source, config);
}

int video_source_stop(struct video_source *video_source)
{
    return video_source->ops->stop(video_source);
}

int video_source_get_frame(struct video_source *video_source, struct video_source_frame *frame)
{
    return video_source->ops->get_frame(video_source, frame);
}

int video_source_release_frame(struct video_source *video_source, struct video_source_frame *frame)
{
    return video_source->ops->release_frame(video_source, frame);
}

struct video_source *video_source_create(enum video_source_type video_source_type)
{
    struct video_source *video_source = malloc(sizeof(struct video_source));
    if (!video_source)
        loge("create video source fail!\n");
    memset(video_source, 0, sizeof(*video_source));

    switch (video_source_type) {
    case VIDEO_SOURCE_TYPE_RT_MEDIA:
        video_source->ops = &video_source_rt_media_ops;
        break;
    default:
        loge("unsupport video_source_type 0x%x", video_source_type);
        return NULL;
    };
    video_source->ops->create((void *)video_source);

    return video_source;
}

void video_source_destroy(struct video_source *video_source)
{
    if (!video_source)
        return;
    if (video_source->ops) {
        video_source->ops->destroy((void *)video_source);
    }
    free(video_source);
}

int video_source_set_extra_config(struct video_source *video_source, struct video_source_extra_config *extra_config)
{
    return video_source->ops->set_extra_config(video_source, extra_config);
}

int video_source_get_isp_state(struct video_source *video_source, struct video_source_isp_state *isp_state)
{
    return video_source->ops->get_isp_state(video_source, isp_state);
}

int video_source_pause(struct video_source *video_source, int flag)
{
    return video_source->ops->pause(video_source, flag);
}

int video_source_set_camera_lowpw_mode(struct video_source *video_source, void *cfg)
{
    return video_source->ops->set_camera_lowpw_mode(video_source, cfg);
}

int video_source_set_orl(struct video_source *video_source, void *orl)
{
    return video_source->ops->set_orl(video_source, orl);
}

int video_source_get_sharp_param(struct video_source *video_source, void *param)
{
    return video_source->ops->get_sharp_param(video_source, param);
}
