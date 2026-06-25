#include "aw_g2d.h"

#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <mqueue.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "linux/g2d_driver.h"

#define WITH_MUTEX_LOCK 0

static int g_g2d_handle;

#if WITH_MUTEX_LOCK
static pthread_mutex_t g2d_lock;
#endif
static volatile int g2d_open_cnt = 0;

int aw_g2d_open(void)
{
    int ret = 0;
    DOORLOCK_DBG("g2d open\n");

    if (g2d_open_cnt == 0) {
#if WITH_MUTEX_LOCK
        pthread_mutex_init(&g2d_lock, 0);
        pthread_mutex_lock(&g2d_lock);
#endif
        g_g2d_handle = open("/dev/g2d", O_RDWR, 0);
        if (g_g2d_handle < 0) {
            DOORLOCK_DBG("g2d_open failed, ret = %d\n", g_g2d_handle);
            ret = -1;
#if WITH_MUTEX_LOCK
            pthread_mutex_unlock(&g2d_lock);
            return ret;
#endif
        }
        ret = g_g2d_handle;
#if WITH_MUTEX_LOCK
        pthread_mutex_unlock(&g2d_lock);
#endif
    } else {
        ret = g_g2d_handle;
    }

    g2d_open_cnt++;
    DOORLOCK_DBG("g2d open cnt = %d\n", g2d_open_cnt);
    return ret;
}

int aw_g2d_close()
{
    DOORLOCK_DBG("g2d close\n");
    int ret = 0;
    DOORLOCK_DBG("g2d open cnt = %d\n", g2d_open_cnt);

    if (g2d_open_cnt == 1) {
#if WITH_MUTEX_LOCK
        pthread_mutex_lock(&g2d_lock);
#endif
        close(g_g2d_handle);
#if WITH_MUTEX_LOCK
        pthread_mutex_unlock(&g2d_lock);
        pthread_mutex_destroy(&g2d_lock);
#endif
    }

    if (g2d_open_cnt)
        g2d_open_cnt--;
    return ret;
}

int aw_g2d_copy(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
            uint32_t phy_dst[3], int dst_width, int dst_height)
{
    int ret = 0;

#if WITH_MUTEX_LOCK
    pthread_mutex_lock(&g2d_lock);
#endif
    g2d_blt_h info;
    memset(&info, 0, sizeof(g2d_blt_h));
    info.flag_h = G2D_ROT_0;

    info.dst_image_h.use_phy_addr = 1;
    info.dst_image_h.format = g2d_format;
    info.dst_image_h.width = dst_width;
    info.dst_image_h.height = dst_height;
    info.dst_image_h.clip_rect.w = dst_width;
    info.dst_image_h.clip_rect.h = dst_height;

    memcpy(&info.src_image_h, &info.dst_image_h, sizeof(g2d_image_enh));
    info.src_image_h.width = src_width;
    info.src_image_h.height = src_height;
    info.src_image_h.clip_rect.w = src_width;
    info.src_image_h.clip_rect.h = src_height;

    info.dst_image_h.laddr[0] = (unsigned int)phy_dst[0];
    info.dst_image_h.laddr[1] = (unsigned int)phy_dst[1];
    info.dst_image_h.laddr[2] = (unsigned int)phy_dst[2];
    info.src_image_h.laddr[0] = (unsigned int)phy_src[0];
    info.src_image_h.laddr[1] = (unsigned int)phy_src[1];
    info.src_image_h.laddr[2] = (unsigned int)phy_src[2];

    ret = ioctl(g_g2d_handle, G2D_CMD_BITBLT_H, (unsigned long)&info);
    if (ret != 0) {
        DOORLOCK_DBG("g2d ioctl G2D_CMD_BITBLT_H err = %d\n", ret);
        ret = -1;
    }

#if WITH_MUTEX_LOCK
    pthread_mutex_unlock(&g2d_lock);
#endif
    return ret;
}

int aw_g2d_resize(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
              uint32_t phy_dst[3], int dst_width, int dst_height)
{
    int ret = 0;
#if WITH_MUTEX_LOCK
    pthread_mutex_lock(&g2d_lock);
#endif
    g2d_blt_h info;
    memset(&info, 0, sizeof(g2d_blt_h));
    info.flag_h = G2D_BLT_NONE_H;

    // info.dst_image_h.gamut = G2D_BT601;
    // info.dst_image_h.mode = G2D_PIXEL_ALPHA;
    // info.dst_image_h.fd = -1;
    info.dst_image_h.use_phy_addr = 1;
    info.dst_image_h.format = g2d_format;

    info.dst_image_h.width = dst_width;
    info.dst_image_h.height = dst_height;
    info.dst_image_h.clip_rect.w = dst_width;
    info.dst_image_h.clip_rect.h = dst_height;

    memcpy(&info.src_image_h, &info.dst_image_h, sizeof(g2d_image_enh));
    info.src_image_h.width = src_width;
    info.src_image_h.height = src_height;
    info.src_image_h.clip_rect.w = src_width;
    info.src_image_h.clip_rect.h = src_height;

    info.dst_image_h.laddr[0] = (unsigned int)phy_dst[0];
    info.dst_image_h.laddr[1] = (unsigned int)phy_dst[1];
    info.dst_image_h.laddr[2] = (unsigned int)phy_dst[2];
    info.src_image_h.laddr[0] = (unsigned int)phy_src[0];
    info.src_image_h.laddr[1] = (unsigned int)phy_src[1];
    info.src_image_h.laddr[2] = (unsigned int)phy_src[2];

    ret = ioctl(g_g2d_handle, G2D_CMD_BITBLT_H, (unsigned long)&info);
    if (ret != 0) {
        DOORLOCK_DBG("g2d ioctl G2D_CMD_BITBLT_H err = %d\n", ret);
        ret = -1;
    }
#if WITH_MUTEX_LOCK
    pthread_mutex_unlock(&g2d_lock);
#endif
    return ret;
}

int aw_g2d_rotate(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
              uint32_t phy_dst[3], int dst_width, int dst_height, uint8_t rot_degree)
{
    int ret = 0;

#if WITH_MUTEX_LOCK
    pthread_mutex_lock(&g2d_lock);
#endif
    g2d_blt_h info;
    memset(&info, 0, sizeof(g2d_blt_h));
    info.flag_h = G2D_ROT_90;  // default

    if (rot_degree == ROT_0) {
        info.flag_h = G2D_ROT_0;
    } else if (rot_degree == ROT_90) {
        info.flag_h = G2D_ROT_90;
    } else if (rot_degree == ROT_180) {
        info.flag_h = G2D_ROT_180;
    } else if (rot_degree == ROT_270) {
        info.flag_h = G2D_ROT_270;
    } else if (rot_degree == ROT_H) {
        info.flag_h = G2D_ROT_H;
    } else if (rot_degree == ROT_V) {
        info.flag_h = G2D_ROT_V;
    } else {
        DOORLOCK_ERR("rot_degree = %d, do not support\n", rot_degree);
        goto __exit;
    }

    info.dst_image_h.use_phy_addr = 1;
    info.dst_image_h.format = g2d_format;
    info.dst_image_h.width = dst_width;
    info.dst_image_h.height = dst_height;
    info.dst_image_h.clip_rect.w = dst_width;
    info.dst_image_h.clip_rect.h = dst_height;

    memcpy(&info.src_image_h, &info.dst_image_h, sizeof(g2d_image_enh));
    info.src_image_h.width = src_width;
    info.src_image_h.height = src_height;
    info.src_image_h.clip_rect.w = src_width;
    info.src_image_h.clip_rect.h = src_height;

    info.dst_image_h.laddr[0] = (unsigned int)phy_dst[0];
    info.dst_image_h.laddr[1] = (unsigned int)phy_dst[1];
    info.dst_image_h.laddr[2] = (unsigned int)phy_dst[2];
    info.src_image_h.laddr[0] = (unsigned int)phy_src[0];
    info.src_image_h.laddr[1] = (unsigned int)phy_src[1];
    info.src_image_h.laddr[2] = (unsigned int)phy_src[2];

    ret = ioctl(g_g2d_handle, G2D_CMD_BITBLT_H, (unsigned long)&info);
    if (ret != 0) {
        DOORLOCK_DBG("g2d ioctl G2D_CMD_BITBLT_H err = %d\n", ret);
        ret = -1;
    }

__exit:
#if WITH_MUTEX_LOCK
    pthread_mutex_unlock(&g2d_lock);
#endif
    return ret;
}

int aw_g2d_cut(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
           uint32_t phy_dst[3], int dst_width, int dst_height, int cut_x, int cut_y)
{
    int ret = 0;
#if WITH_MUTEX_LOCK
    pthread_mutex_lock(&g2d_lock);
#endif
    g2d_blt_h info;
    memset(&info, 0, sizeof(g2d_blt_h));
    info.flag_h = G2D_ROT_0;

    info.dst_image_h.use_phy_addr = 1;
    info.dst_image_h.format = g2d_format;
    info.dst_image_h.width = dst_width;
    info.dst_image_h.height = dst_height;
    info.dst_image_h.clip_rect.w = dst_width;
    info.dst_image_h.clip_rect.h = dst_height;
    info.dst_image_h.clip_rect.x = 0;
    info.dst_image_h.clip_rect.y = 0;

    memcpy(&info.src_image_h, &info.dst_image_h, sizeof(g2d_image_enh));
    info.src_image_h.width = src_width;
    info.src_image_h.height = src_height;
    info.src_image_h.clip_rect.w = dst_width;
    info.src_image_h.clip_rect.h = dst_height;
    info.src_image_h.clip_rect.x = cut_x;
    info.src_image_h.clip_rect.y = cut_y;

    info.dst_image_h.laddr[0] = (unsigned int)phy_dst[0];
    info.dst_image_h.laddr[1] = (unsigned int)phy_dst[1];
    info.dst_image_h.laddr[2] = (unsigned int)phy_dst[2];
    info.src_image_h.laddr[0] = (unsigned int)phy_src[0];
    info.src_image_h.laddr[1] = (unsigned int)phy_src[1];
    info.src_image_h.laddr[2] = (unsigned int)phy_src[2];

    ret = ioctl(g_g2d_handle, G2D_CMD_BITBLT_H, (unsigned long)&info);
    if (ret != 0) {
        DOORLOCK_DBG("g2d ioctl G2D_CMD_BITBLT_H err = %d\n", ret);
        ret = -1;
    }

#if WITH_MUTEX_LOCK
    pthread_mutex_unlock(&g2d_lock);
#endif
    return ret;
}

int aw_g2d_paste(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
             uint32_t phy_dst[3], int dst_width, int dst_height, int paste_x, int paste_y)
{
    int ret = 0;
#if WITH_MUTEX_LOCK
    pthread_mutex_lock(&g2d_lock);
#endif
    g2d_blt_h info;
    memset(&info, 0, sizeof(g2d_blt_h));
    // info.flag_h = G2D_ROT_0;
    info.flag_h = G2D_BLT_NONE_H;

    info.dst_image_h.use_phy_addr = 1;
    info.dst_image_h.format = g2d_format;
    info.dst_image_h.width = dst_width;
    info.dst_image_h.height = dst_height;
    info.dst_image_h.clip_rect.w = src_width;
    info.dst_image_h.clip_rect.h = src_height;
    info.dst_image_h.clip_rect.x = paste_x;
    info.dst_image_h.clip_rect.y = paste_y;

    memcpy(&info.src_image_h, &info.dst_image_h, sizeof(g2d_image_enh));
    info.src_image_h.width = src_width;
    info.src_image_h.height = src_height;
    info.src_image_h.clip_rect.w = src_width;
    info.src_image_h.clip_rect.h = src_height;
    info.src_image_h.clip_rect.x = 0;
    info.src_image_h.clip_rect.y = 0;

    info.dst_image_h.laddr[0] = (unsigned int)phy_dst[0];
    info.dst_image_h.laddr[1] = (unsigned int)phy_dst[1];
    info.dst_image_h.laddr[2] = (unsigned int)phy_dst[2];
    info.src_image_h.laddr[0] = (unsigned int)phy_src[0];
    info.src_image_h.laddr[1] = (unsigned int)phy_src[1];
    info.src_image_h.laddr[2] = (unsigned int)phy_src[2];

    ret = ioctl(g_g2d_handle, G2D_CMD_BITBLT_H, (unsigned long)&info);
    if (ret != 0) {
        DOORLOCK_DBG("g2d ioctl G2D_CMD_BITBLT_H err = %d\n", ret);
        ret = -1;
    }

#if WITH_MUTEX_LOCK
    pthread_mutex_unlock(&g2d_lock);
#endif
    return ret;
}

int aw_g2d_convert(g2d_fmt_enh g2d_format_src, uint32_t phy_src[3], int width_src, int height_src,
               g2d_fmt_enh g2d_format_dst, uint32_t phy_dst[3], int width_dst, int height_dst)
{
    int ret = 0;

#if WITH_MUTEX_LOCK
    pthread_mutex_lock(&g2d_lock);
#endif
    g2d_blt_h info;
    memset(&info, 0, sizeof(g2d_blt_h));
    info.flag_h = G2D_BLT_NONE_H;

    info.dst_image_h.use_phy_addr = 1;
    info.dst_image_h.format = g2d_format_dst;
    info.dst_image_h.width = width_dst;
    info.dst_image_h.height = height_dst;
    info.dst_image_h.clip_rect.w = width_dst;
    info.dst_image_h.clip_rect.h = height_dst;

    memcpy(&info.src_image_h, &info.dst_image_h, sizeof(g2d_image_enh));
    info.src_image_h.format = g2d_format_src;
    info.src_image_h.width = width_src;
    info.src_image_h.height = height_src;
    info.src_image_h.clip_rect.w = width_src;
    info.src_image_h.clip_rect.h = height_src;

    info.dst_image_h.laddr[0] = (unsigned int)phy_dst[0];
    info.dst_image_h.laddr[1] = (unsigned int)phy_dst[1];
    info.dst_image_h.laddr[2] = (unsigned int)phy_dst[2];
    info.src_image_h.laddr[0] = (unsigned int)phy_src[0];
    info.src_image_h.laddr[1] = (unsigned int)phy_src[1];
    info.src_image_h.laddr[2] = (unsigned int)phy_src[2];

    ret = ioctl(g_g2d_handle, G2D_CMD_BITBLT_H, (unsigned long)&info);
    if (ret != 0) {
        DOORLOCK_DBG("g2d ioctl G2D_CMD_BITBLT_H err = %d\n", ret);
        ret = -1;
    }
#if WITH_MUTEX_LOCK
    pthread_mutex_unlock(&g2d_lock);
#endif
    return ret;
}

g2d_fmt_enh V4L2_FORMAT_to_G2D_FORMAT(uint32_t v4l2_pix_format)
{
    uint32_t g2d_format = G2D_FORMAT_Y8;  // default

    if (v4l2_pix_format == V4L2_PIX_FMT_NV21 || v4l2_pix_format == V4L2_PIX_FMT_NV21M) {
        g2d_format = G2D_FORMAT_YUV420UVC_U1V1U0V0;
    } else if (v4l2_pix_format == V4L2_PIX_FMT_NV12 ||
               v4l2_pix_format == V4L2_PIX_FMT_NV12M) {
        g2d_format = G2D_FORMAT_YUV420UVC_V1U1V0U0;
    } else if (v4l2_pix_format == V4L2_PIX_FMT_YUV420 ||
               v4l2_pix_format == V4L2_PIX_FMT_YVU420) {
        g2d_format = G2D_FORMAT_YUV420_PLANAR;
    } else if (v4l2_pix_format == V4L2_PIX_FMT_GREY) {
        g2d_format = G2D_FORMAT_Y8;
    }

    return (g2d_fmt_enh)g2d_format;
}
