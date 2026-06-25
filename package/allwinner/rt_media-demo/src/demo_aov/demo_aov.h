#ifndef __DEMO_AOV_H__
#define __DEMO_AOV_H__

#include <base_list.h>

#include "utils/include/option.h"
#include "utils/include/demo_aov_mux.h"
#include "video_source/base/include/video_source.h"
#include "video_encode/base/include/video_encode.h"

#ifdef __cplusplus
extern "C"{
#endif

#define DEFAULT_SUSPEND_SECONDS     (5)

enum CAMERA_POWER_MODE {
    CAMERA_POWER_MODE_3V3_KEEP = 1,
    CAMERA_POWER_MODE_3V3_DOWN,
};

struct aov_demo_context
{
    int task_eixt;
    pthread_t task_trd;

    struct aov_demo_config config;
    struct video_source *video_source;
    struct video_encode *video_encode;

    int algo_detected;
    unsigned long long algo_detected_timestamp;
    time_t prev_timestamp;

    pthread_mutex_t lock;
};

#ifdef __cplusplus
}
#endif

#endif
