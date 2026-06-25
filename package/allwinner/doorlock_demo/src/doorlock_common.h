#ifndef __DOORLOCK_COMMON_H__
#define __DOORLOCK_COMMON_H__

#include <asm/types.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint64_t get_cur_time_us()
{
    uint64_t curr;
    struct timespec t;

    memset(&t, 0, sizeof(struct timespec));
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = t.tv_sec;
    curr = curr * 1000000 + (t.tv_nsec / 1000);
    return curr;
}

#define DOORLOCK_COLOR_LOG 1

#ifndef ALIGN
#define ALIGN(x, a) ((a) * (((x) + (a) - 1) / (a)))
#endif

#ifndef ALIGN_16B
#define ALIGN_16B(x) (((x) + (15)) & ~(15))
#endif

#ifndef ALIGN_32B
#define ALIGN_32B(x) (((x) + (31)) & ~(31))
#endif

#ifndef DOORLOCK_ALIGN
#define DOORLOCK_ALIGN(x, align) (((x) + (align)) & ~(align))
#endif

// remove path
#define __FILE_NAME__ strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__

typedef enum {
    DOORLOCK_LOG_LEVEL_DBG = 5,
    DOORLOCK_LOG_LEVEL_INFO = 4,
    DOORLOCK_LOG_LEVEL_WARN = 3,
    DOORLOCK_LOG_LEVEL_ERR = 2,
    DOORLOCK_LOG_LEVEL_PRINT = 1,
} DOORLOCK_LOG_LEVEL;

#define DOORLOCK_DEFAULT_LOG_LEVEL DOORLOCK_LOG_LEVEL_INFO

#define DOORLOCK_LIGHT "1"
#define DOORLOCK_DARK "0"

#define DOORLOCK_FG "3"
#define DOORLOCK_BG "4"

#define DOORLOCK_BLACK "0"
#define DOORLOCK_RED "1"
#define DOORLOCK_GREEN "2"
#define DOORLOCK_YELLOW "3"
#define DOORLOCK_BLUE "4"
#define DOORLOCK_PURPLE "5"
#define DOORLOCK_CYAN "6"
#define DOORLOCK_WRITE "7"

#define DOORLOCK_FG_COLOR(color) DOORLOCK_FG color
#define DOORLOCK_BG_COLOR(color) DOORLOCK_BG color

#if DOORLOCK_COLOR_LOG
#define DOORLOCK_FMT_DEFAULT "\033[0m"
#define DOORLOCK_FMT_COLOR_FG(light, color) "\033[" light ";" DOORLOCK_FG_COLOR(color) "m"
#define DOORLOCK_FMT_COLOR_BG(fg, color) \
    "\033[" DOORLOCK_BG_COLOR(color) ";" DOORLOCK_FG_COLOR(fg) "m"
#else
#define DOORLOCK_FMT_DEFAULT
#define DOORLOCK_FMT_COLOR_FG(light, color)
#define DOORLOCK_FMT_COLOR_BG(fg, color)
#endif

extern int g_doorlock_log_level;
extern char g_doorlock_log_path[256];
extern char g_doorlock_log_buf[1024 * 10];
int doorlock_log_file_config(char *file_path);
int doorlock_log_tag_config(char *tag);
int doorlock_set_dynamic_log_level(int level);
int doorlock_get_dynamic_log_level(int *level);

#define DOORLOCK_LOG_COLOR_PRINT \
    DOORLOCK_FMT_COLOR_BG(DOORLOCK_BLACK, DOORLOCK_WRITE) "DOORLOCK_PRINT " DOORLOCK_FMT_DEFAULT
#define DOORLOCK_LOG_COLOR_DBG \
    DOORLOCK_FMT_COLOR_BG(DOORLOCK_BLACK, DOORLOCK_BLUE) "DOORLOCK_DBG  " DOORLOCK_FMT_DEFAULT
#define DOORLOCK_LOG_COLOR_INFO \
    DOORLOCK_FMT_COLOR_BG(DOORLOCK_BLACK, DOORLOCK_WRITE) "DOORLOCK_INFO " DOORLOCK_FMT_DEFAULT
#define DOORLOCK_LOG_COLOR_WARN \
    DOORLOCK_FMT_COLOR_BG(DOORLOCK_BLACK, DOORLOCK_YELLOW) "DOORLOCK_WARN " DOORLOCK_FMT_DEFAULT
#define DOORLOCK_LOG_COLOR_ERR \
    DOORLOCK_FMT_COLOR_BG(DOORLOCK_BLACK, DOORLOCK_RED) "DOORLOCK_ERR  " DOORLOCK_FMT_DEFAULT

#define __DOORLOCK_LOG_WRITE(LOG_BUF, LOG_FILE_PATH)           \
    do {                                                       \
        if (strlen(LOG_FILE_PATH) > 0) {                       \
            FILE *log_file = fopen(LOG_FILE_PATH, "a+");       \
            if (log_file != NULL) {                            \
                fseek(log_file, 0, SEEK_END);                  \
                fwrite(LOG_BUF, 1, strlen(LOG_BUF), log_file); \
                fclose(log_file);                              \
            }                                                  \
        }                                                      \
    } while (0)

#define __DOORLOCK_PRINT(format, args...)                                                   \
    do {                                                                                    \
        char log_buf[1024 * 10];                                                             \
        memset(log_buf, 0, sizeof(log_buf));                                                  \
        sprintf(log_buf, "%s %s:%04d-->%s:" format, DOORLOCK_LOG_COLOR_PRINT, \
                __FILE_NAME__, __LINE__, __func__, ##args);             \
        printf(log_buf);                                                                     \
        __DOORLOCK_LOG_WRITE(log_buf, g_doorlock_log_path);                                  \
    } while (0)

#define __DOORLOCK_DBG(format, args...)                                                   \
    do {                                                                                  \
        char log_buf[1024 * 10];                                                           \
        memset(log_buf, 0, sizeof(log_buf));                                                \
        sprintf(log_buf, "%s %s:%04d-->%s:" format, DOORLOCK_LOG_COLOR_DBG, \
                __FILE_NAME__, __LINE__, __func__, ##args);           \
        printf(log_buf);                                                                   \
        __DOORLOCK_LOG_WRITE(log_buf, g_doorlock_log_path);                                \
    } while (0)

#define __DOORLOCK_INFO(format, args...)                                                   \
    do {                                                                                   \
        char log_buf[1024 * 10];                                                            \
        memset(log_buf, 0, sizeof(log_buf));                                                 \
        sprintf(log_buf, "%s %s:%04d-->%s:" format, DOORLOCK_LOG_COLOR_INFO, \
                __FILE_NAME__, __LINE__, __func__, ##args);            \
        printf(log_buf);                                                                    \
        __DOORLOCK_LOG_WRITE(log_buf, g_doorlock_log_path);                                 \
    } while (0)

#define __DOORLOCK_WARN(format, args...)                                                   \
    do {                                                                                   \
        char log_buf[1024 * 10];                                                            \
        memset(log_buf, 0, sizeof(log_buf));                                                 \
        sprintf(log_buf, "%s %s:%04d-->%s:" format, DOORLOCK_LOG_COLOR_WARN, \
                __FILE_NAME__, __LINE__, __func__, ##args);            \
        printf(log_buf);                                                                    \
        __DOORLOCK_LOG_WRITE(log_buf, g_doorlock_log_path);                                 \
    } while (0)

#define __DOORLOCK_ERR(format, args...)                                                   \
    do {                                                                                  \
        char log_buf[1024 * 10];                                                           \
        memset(log_buf, 0, sizeof(log_buf));                                                \
        sprintf(log_buf, "%s %s:%04d-->%s:" format, DOORLOCK_LOG_COLOR_ERR, \
                __FILE_NAME__, __LINE__, __func__, ##args);           \
        printf(log_buf);                                                                   \
        __DOORLOCK_LOG_WRITE(log_buf, g_doorlock_log_path);                                \
    } while (0)

#define DOORLOCK_DBG(format, args...)                         \
    do {                                                      \
        if (g_doorlock_log_level >= DOORLOCK_LOG_LEVEL_DBG) { \
            __DOORLOCK_DBG(format, ##args);                   \
        }                                                     \
    } while (0)

#define DOORLOCK_INFO(format, args...)                         \
    do {                                                       \
        if (g_doorlock_log_level >= DOORLOCK_LOG_LEVEL_INFO) { \
            __DOORLOCK_INFO(format, ##args);                   \
        }                                                      \
    } while (0)

#define DOORLOCK_WARN(format, args...)                         \
    do {                                                       \
        if (g_doorlock_log_level >= DOORLOCK_LOG_LEVEL_WARN) { \
            __DOORLOCK_WARN(format, ##args);                   \
        }                                                      \
    } while (0)

#define DOORLOCK_ERR(format, args...)                         \
    do {                                                      \
        if (g_doorlock_log_level >= DOORLOCK_LOG_LEVEL_ERR) { \
            __DOORLOCK_ERR(format, ##args);                   \
        }                                                     \
    } while (0)

#define DOORLOCK_PRINT(format, args...)                         \
    do {                                                        \
        if (g_doorlock_log_level >= DOORLOCK_LOG_LEVEL_PRINT) { \
            __DOORLOCK_PRINT(format, ##args);                   \
        }                                                       \
    } while (0)

int doorlock_get_timestr(char *buf, int buf_size, char *fmt);
void doorlock_set_str(char *file_name, char *str_buf);
int doorlock_get_int(char *file_name, int default_int);
void doorlock_get_str(char *file_name, char *str_buf, uint32_t max_buf_len);

extern int pthread_setname_np(pthread_t thread, const char *name);
extern int ioctl(int, int, ...);

typedef struct cpu_occupy_s {
    char name[20];
    unsigned int user;
    unsigned int nice;
    unsigned int system;
    unsigned int idle;
    unsigned int iowait;
    unsigned int irq;
    unsigned int softirq;
} cpu_occupy_t;

typedef struct mem_occupy_s {
    unsigned int total;      // kB
    unsigned int free;       // kB
    unsigned int avaliable;  // kB
    double usage;
} mem_occupy_t;

typedef struct thermal_zone_s {
    char type[50];
    uint32_t temp;
} __attribute__((packed)) thermal_zone_t;

#define THERMAL_ZONE_NUM 3
typedef struct thermal_zones_s {
    thermal_zone_t thermals[THERMAL_ZONE_NUM];
} __attribute__((packed)) thermal_zones_t;

extern int doorlock_get_cpu_occupy_info(cpu_occupy_t *cpu_occupy);
extern double doorlock_calc_cpu_occupy_info(cpu_occupy_t *o, cpu_occupy_t *n);
extern int doorlock_get_mem_occupy_info(mem_occupy_t *mem_info);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
