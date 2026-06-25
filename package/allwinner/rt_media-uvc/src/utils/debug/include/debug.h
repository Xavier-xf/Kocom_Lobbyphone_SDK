#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

enum UVC_DEMO_LOG_LEVEL_TYPE
{
    UVC_DEMO_LOG_VERBOSE = 0,
    UVC_DEMO_LOG_DEBUG   = 1,
    UVC_DEMO_LOG_WARNING = 2,
    UVC_DEMO_LOG_ERROR   = 3,
    UVC_DEMO_LOG_CLOSE   = 4,
};

#ifndef UVC_DEMO_LOG_LEVEL
#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_DEBUG
#endif

#define log_printf(level, string, fmt, arg...) \
    do { \
        if (level >= UVC_DEMO_LOG_LEVEL) \
            printf("[%s]%s, line: %d. " fmt "\n", string, __FUNCTION__, __LINE__, ##arg); \
    } while (0)
#define logv(x, arg...)        log_printf(UVC_DEMO_LOG_VERBOSE, "VER", x, ##arg)
#define logw(x, arg...)        log_printf(UVC_DEMO_LOG_WARNING, "WRN", x, ##arg)
#define loge(x, arg...)        log_printf(UVC_DEMO_LOG_ERROR,   "ERR", x, ##arg)
#define logd(x, arg...)        log_printf(UVC_DEMO_LOG_DEBUG,   "DBG", x, ##arg)

#endif
