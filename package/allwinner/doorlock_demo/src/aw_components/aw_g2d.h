#ifndef __AW_G2D_H__
#define __AW_G2D_H__

#include <stdbool.h>
#include <stdio.h>

#include "linux/g2d_driver.h"
#include "linux/videodev2.h"
#include "doorlock_common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    ROT_0 = 0,
    ROT_90 = 1,
    ROT_180 = 2,
    ROT_270 = 3,
    ROT_H = 4,
    ROT_V = 5,
} aw_rotate;

int aw_g2d_open();
int aw_g2d_close();
int aw_g2d_copy(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
                uint32_t phy_dst[3], int dst_width, int dst_height);
int aw_g2d_resize(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
                  uint32_t phy_dst[3], int dst_width, int dst_height);
// rot_degree: 1(90), 2(180), 3(270), 4(MIRROR), 5(FLIP)
int aw_g2d_rotate(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
                  uint32_t phy_dst[3], int dst_width, int dst_height, uint8_t rot_degree);
int aw_g2d_cut(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
               uint32_t phy_dst[3], int dst_width, int dst_height, int cut_x, int cut_y);
int aw_g2d_paste(g2d_fmt_enh g2d_format, uint32_t phy_src[3], int src_width, int src_height,
                 uint32_t phy_dst[3], int dst_width, int dst_height, int paste_x, int paste_y);
int aw_g2d_convert(g2d_fmt_enh g2d_format_src, uint32_t phy_src[3], int width_src, int height_src,
                   g2d_fmt_enh g2d_format_dst, uint32_t phy_dst[3], int width_dst, int height_dst);

g2d_fmt_enh V4L2_FORMAT_to_G2D_FORMAT(uint32_t v4l2_pix_format);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
