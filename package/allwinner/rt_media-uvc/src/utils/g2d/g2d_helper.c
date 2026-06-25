#include "../sys/include/sys_linux_ioctl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "../debug/include/debug.h"
#include "include/g2d_helper.h"

int g2d_open(void)
{
    int nG2DFd = -1;

    nG2DFd = open(G2D_DEVICE, O_RDWR, 0);
    if (nG2DFd < 0)
    {
        loge("fatal error! open g2d device fail!");
    }

    return nG2DFd;
}

int g2d_close(int nG2DFd)
{
    close(nG2DFd);
}

int g2d_proc(int nG2DFd, G2D_FRAME_INFO_S * pSrcFrame, G2D_FRAME_INFO_S * pDstFrame)
{
    int ret = 0;
    g2d_blt_h blt;
    int nFrmSize = 0;

    memset(&blt, 0, sizeof(g2d_blt_h));
    blt.flag_h = G2D_BLT_NONE_H;
    blt.src_image_h.format = pSrcFrame->mPixFormat;
    blt.src_image_h.laddr[0] = (unsigned int)pSrcFrame->mPhyAddr[0];
    blt.src_image_h.laddr[1] = (unsigned int)pSrcFrame->mPhyAddr[1];
    blt.src_image_h.laddr[2] = (unsigned int)pSrcFrame->mPhyAddr[2];
    blt.src_image_h.width = pSrcFrame->mWidth;
    blt.src_image_h.height = pSrcFrame->mHeight;
    blt.src_image_h.align[0] = 0;
    blt.src_image_h.align[1] = 0;
    blt.src_image_h.align[2] = 0;
    blt.src_image_h.clip_rect.x = 0;
    blt.src_image_h.clip_rect.y = 0;
    blt.src_image_h.clip_rect.w = pSrcFrame->mWidth;
    blt.src_image_h.clip_rect.h = pSrcFrame->mHeight;
    blt.src_image_h.gamut = G2D_BT601;
    blt.src_image_h.bpremul = 0;
    blt.src_image_h.mode = G2D_PIXEL_ALPHA;
    blt.src_image_h.fd = -1;
    blt.src_image_h.use_phy_addr = 1;

    blt.dst_image_h.format = pDstFrame->mPixFormat;
    blt.dst_image_h.laddr[0] = (unsigned int)pDstFrame->mPhyAddr[0];
    blt.dst_image_h.laddr[1] = (unsigned int)pDstFrame->mPhyAddr[1];
    blt.dst_image_h.laddr[2] = (unsigned int)pDstFrame->mPhyAddr[2];
    blt.dst_image_h.width = pDstFrame->mWidth;
    blt.dst_image_h.height = pDstFrame->mHeight;
    blt.dst_image_h.align[0] = 0;
    blt.dst_image_h.align[1] = 0;
    blt.dst_image_h.align[2] = 0;
    blt.dst_image_h.clip_rect.x = 0;
    blt.dst_image_h.clip_rect.y = 0;
    blt.dst_image_h.clip_rect.w = pDstFrame->mWidth;
    blt.dst_image_h.clip_rect.h = pDstFrame->mHeight;
    blt.dst_image_h.gamut = G2D_BT601;
    blt.dst_image_h.bpremul = 0;
    blt.dst_image_h.mode = G2D_PIXEL_ALPHA;
    blt.dst_image_h.fd = -1;
    blt.dst_image_h.use_phy_addr = 1;

    ret = ioctl(nG2DFd, G2D_CMD_BITBLT_H, (unsigned long)&blt);
    if (ret < 0)
    {
        loge("fatal error! g2d proc fail!");
        return -1;
    }

    return ret;
}
