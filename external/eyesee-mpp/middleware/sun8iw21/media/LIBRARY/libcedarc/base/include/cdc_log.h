
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : cdc_log.h
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2015/04/13
*   Comment :
*
*
*/

#ifndef LOG_H
#define LOG_H

#include <cdc_config.h>
#include "UserKernelAdapter.h"

#ifndef LOG_TAG
#define LOG_TAG "cedarc"
#endif

#define DEBUG_TRACK_PROCESSING 0

enum CDC_LOG_LEVEL_TYPE {
    LOG_LEVEL_VERBOSE = 2,
    LOG_LEVEL_DEBUG = 3,
    LOG_LEVEL_INFO = 4,
    LOG_LEVEL_WARNING = 5,
    LOG_LEVEL_ERROR = 6,
};

void cdc_log_set_level(unsigned level);

extern enum CDC_LOG_LEVEL_TYPE CDC_GLOBAL_LOG_LEVEL;

#ifdef __ANDROID__
    #ifdef CONF_OREO_AND_NEWER
    #include <log/log.h>
    #else
    #include <cutils/log.h>
    #endif

#define CDCLOG(level, fmt, arg...)  \
    do { \
        if (level >= CDC_GLOBAL_LOG_LEVEL) \
            LOG_PRI((android_LogPriority)level, LOG_TAG, "<%s:%u>: " fmt, __FUNCTION__, __LINE__, ##arg); \
    } while (0)

#define CC_LOG_ASSERT(e, fmt, arg...)                               \
        LOG_ALWAYS_FATAL_IF(                                        \
                !(e),                                               \
                "<%s:%d>check (%s) failed:" fmt,                    \
                __FUNCTION__, __LINE__, #e, ##arg)                  \

#else

#ifdef CONFIG_VIDEO_RT_MEDIA
#include <linux/kernel.h>
#include <linux/ktime.h>
#else
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#endif

extern const char *CDC_LOG_LEVEL_NAME[];
#define CDCLOG(level, fmt, arg...)  \
    do { \
        if (level >= CDC_GLOBAL_LOG_LEVEL) \
            PRINTF("%s: %s <%s:%u>: " fmt "\n", \
                    CDC_LOG_LEVEL_NAME[level], LOG_TAG, __FUNCTION__, __LINE__, ##arg); \
    } while (0)


#define CC_LOG_ASSERT(e, fmt, arg...)                                       \
                do {                                                        \
                    if (!(e))                                               \
                    {                                                       \
                        loge("check (%s) failed:"fmt, #e, ##arg);           \
                        assert(0);                                          \
                    }                                                       \
                } while (0)

#endif

#define loge(fmt, arg...) CDCLOG(LOG_LEVEL_ERROR, "\033[40;31m" fmt "\033[0m", ##arg)
#define logw(fmt, arg...) CDCLOG(LOG_LEVEL_WARNING, fmt, ##arg)
#define logi(fmt, arg...) CDCLOG(LOG_LEVEL_INFO, fmt, ##arg)
#define logd(fmt, arg...) CDCLOG(LOG_LEVEL_DEBUG, fmt, ##arg)
#define logv(fmt, arg...) CDCLOG(LOG_LEVEL_VERBOSE, fmt, ##arg)

#if DEBUG_TRACK_PROCESSING
#define logs(fmt, arg...) CDCLOG(LOG_LEVEL_WARNING, "flow: "fmt, ##arg)
#else
#define logs(fmt, arg...)
#endif

static inline int64_t getCurrentTime(void)
{
#ifdef CONFIG_VIDEO_RT_MEDIA
    return (long long)ktime_get_ns()/1000;
#else
    struct timeval tv;
    int64_t time;
    gettimeofday(&tv,NULL);
    time = (int64_t)tv.tv_sec*1000000 + tv.tv_usec;
    return time;
#endif
}

#define CEDARC_PRINTF_LINE logd("Run this line")
#define PRINTF_TIME logw("%lld Run this line", getCurrentTime())

#define PRINTF_COST_TIME(name, ttime) do {\
	if (ttime != 0) {\
		int64_t now_time = getCurrentTime();\
		logw("%s cost ttime %lld Run this line", name, now_time - ttime);\
		ttime = now_time;\
	}\
} while (0)

#define CEDARC_UNUSE(param) (void)param  //just for remove compile warning

#define CEDARC_DEBUG (0)

#endif

