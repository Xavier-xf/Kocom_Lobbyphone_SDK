#include "aw_alg.h"

static unsigned int g_ref_cnt = 0;
static int g_alg_err = 0;

int aw_alg_init(aw_alg_t *aw_alg_info)
{
    int ret = 0;
    DOORLOCK_DBG("enter ===>\n");

    if (aw_alg_info->alg_max_mem_size == 0) {
        DOORLOCK_ERR("alg_max_mem_size must be valid\n");
        DOORLOCK_DBG("exit <===\n");
        return -1;
    }

    g_ref_cnt++;
    if (g_ref_cnt != 1) {
        goto already_init;
    }

    pthread_mutex_init(&aw_alg_info->alg_mutex, NULL);
    pthread_mutex_lock(&aw_alg_info->alg_mutex);

    aw_g2d_open();
    aw_mem_open();

    // LWJTODO
    int npu_memory_bytes = 2 * 1024 * 1024;
    vip_status_e status = vip_init(npu_memory_bytes);
    if (status != VIP_SUCCESS) {
        DOORLOCK_ERR("vip_init failed, %d\n", status);
        ret = -1;
    }
    g_alg_err = ret;
    pthread_mutex_unlock(&aw_alg_info->alg_mutex);

already_init:
    if (ret != 0) {
        g_ref_cnt = 0;
        aw_mem_close();
        aw_g2d_close();
        pthread_mutex_destroy(&aw_alg_info->alg_mutex);
    }
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int aw_alg_deinit(aw_alg_t *aw_alg_info)
{
    int ret = 0;
    DOORLOCK_DBG("enter ===>\n");
    if (g_alg_err != 0) {
        goto alg_init_err;
    }
    if (g_ref_cnt > 0) {
        g_ref_cnt--;
    }
    if (g_ref_cnt != 0) {
        goto still_ref;
    }
    pthread_mutex_lock(&aw_alg_info->alg_mutex);

    vip_status_e status = vip_destroy();
    if (status != VIP_SUCCESS) {
        DOORLOCK_ERR("vip_destroy failed, %d\n", status);
        ret = -1;
    }

    aw_g2d_close();
    aw_mem_close();
    pthread_mutex_unlock(&aw_alg_info->alg_mutex);
    pthread_mutex_destroy(&aw_alg_info->alg_mutex);

still_ref:
alg_init_err:
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int aw_alg_coord_scale(int src_coord, int src_data, int dst_coord)
{
    int dst_data = 0;

    dst_data = src_data * dst_coord / src_coord;

    return dst_data;
}

int aw_alg_coord_rotate(alg_coord_t *src, alg_coord_t *dst, int rotate_angle)
{
    rotate_angle = rotate_angle % 360;

    if (rotate_angle < 0) {
        rotate_angle = 360 + rotate_angle;
    }
    if (rotate_angle == 90) {
        dst->width = src->height;
        dst->height = src->width;
        dst->x1 = src->height - src->y2;
        dst->y1 = src->x1;
        dst->x2 = src->height - src->y1;
        dst->y2 = src->x2;
    } else if (rotate_angle == 180) {
        dst->width = src->width;
        dst->height = src->height;
        dst->x1 = src->width - src->x2;
        dst->y1 = src->height - src->y2;
        dst->x2 = src->width - src->x1;
        dst->y2 = src->height - src->y1;
    } else if (rotate_angle == 270) {
        dst->width = src->height;
        dst->height = src->width;
        dst->x1 = src->y1;
        dst->y1 = src->width - src->x2;
        dst->x2 = src->y2;
        dst->y2 = src->width - src->x1;
    } else {
        dst->width = src->width;
        dst->height = src->height;
        dst->x1 = src->x1;
        dst->y1 = src->y1;
        dst->x2 = src->x2;
        dst->y2 = src->y2;
    }
}
