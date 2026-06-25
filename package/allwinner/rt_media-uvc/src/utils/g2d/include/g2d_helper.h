#ifndef __G2D_HELPER_H__
#define __G2D_HELPER_H__

#include <stdbool.h>
#include <linux/g2d_driver.h>

#define G2D_DEVICE      "/dev/g2d"

typedef struct G2D_FRAME_INFO_S
{
    unsigned int mPixFormat;
    unsigned int mWidth;
    unsigned int mHeight;
    unsigned long mPhyAddr[3];
    void *mpVirAddr[3];
}G2D_FRAME_INFO_S;

int g2d_open(void);

int g2d_proc(int nG2DFd, G2D_FRAME_INFO_S *pSrcFrame, G2D_FRAME_INFO_S *pDstFrame);

int g2d_close(int nG2DFd);

#endif
