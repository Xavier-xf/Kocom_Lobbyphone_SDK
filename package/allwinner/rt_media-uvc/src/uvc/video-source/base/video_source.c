#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/video_source.h"
#include "../../../utils/debug/include/debug.h"

#if VIDEO_SOURCE_RT_MEDIA
extern const struct video_source_ops video_source_rt_media_ops;
#endif
#if VIDEO_SOURCE_MPP
extern const struct video_source_ops video_source_mpp_ops;
#endif
#if VIDEO_SOURCE_COLORBAR
extern const struct video_source_ops video_source_colorbar_ops;
#endif

int video_source_start(struct video_source *video_source, struct video_source_base_config *config)
{
    return video_source->ops->start(video_source, config);
}

int video_source_stop(struct video_source *video_source)
{
    return video_source->ops->stop(video_source);
}

struct video_source_frame *video_source_get_frame(struct video_source *video_source)
{
    return video_source->ops->get_frame(video_source);
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
#if VIDEO_SOURCE_COLORBAR
    case VIDEO_SOURCE_TYPE_COLORBAR:
        video_source->ops = &video_source_colorbar_ops;
        break;
#endif
#if VIDEO_SOURCE_RT_MEDIA
    case VIDEO_SOURCE_TYPE_RT_MEDIA:
        video_source->ops = &video_source_rt_media_ops;
        break;
#elif VIDEO_SOURCE_MPP
    case VIDEO_SOURCE_TYPE_MPP:
        video_source->ops = &video_source_mpp_ops;
        break;
#endif
    default:
        memcpy(&video_source->ops, &video_source_colorbar_ops, sizeof(video_source_colorbar_ops));
        break;
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
