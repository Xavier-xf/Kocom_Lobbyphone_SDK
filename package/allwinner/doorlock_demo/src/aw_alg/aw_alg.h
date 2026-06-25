
#ifndef __AW_ALG_H__
#define __AW_ALG_H__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <linux/types.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include "doorlock_common.h"
#include "aw_g2d.h"
#include "aw_mem.h"
#include "vip_lite.h"
#include "vip_lite_common.h"
#include "vnn_utils.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

#define ALG_MAX_NAME_LEN   100
#define ALG_MAX_PATH_LEN   256
#define ALG_SHARE_MEM  1//0:no share, 1:share

typedef struct aw_alg_s {
    unsigned int alg_max_mem_size;
    pthread_mutex_t alg_mutex;
} aw_alg_t;

typedef struct alg_coord_s {
    unsigned int width;
    unsigned int height;
    unsigned int x1;
    unsigned int x2;
    unsigned int y1;
    unsigned int y2;
} alg_coord_t;

int aw_alg_init(aw_alg_t *aw_alg_info);
int aw_alg_deinit(aw_alg_t *aw_alg_info);
int aw_alg_coord_scale(int src_coord, int src_data, int dst_coord);
int aw_alg_coord_rotate(alg_coord_t *src, alg_coord_t *dst, int rotate_angle);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif /* __AW_ALG_H__ */