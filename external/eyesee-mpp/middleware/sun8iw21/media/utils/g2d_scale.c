//#define LOG_NDEBUG 0
#define LOG_TAG "g2d_scale"
#include <utils/plat_log.h>

#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys_linux_ioctl.h>

#include <PIXEL_FORMAT_E_g2d_format_convert.h>
#include "V4l2Camera/sunxi_camera_v2.h"
#include "g2d_scale.h"

/**
  use g2d to convert picture format to dst_format, scale picture size to dst_size, and rotate. But rotate forbid scale.
  e.g. nv12@640x360 -> rgb888@640x360
  We think srcFrameBuffer and dstFrameBuffer of pScaleInfo are all iommu memory that are alloc by ion.

  @return
    0:success
    -1:fail
*/
int ScalePictureByG2d(ScalePictureParam *pScaleInfo, int nG2dFd)
{
    int ret = 0;
    g2d_blt_h blit;
    g2d_fmt_enh eSrcFormat, eDstFormat;
    g2d_color_gmt eSrcG2dColorSpace, eDstG2dColorSpace;

    //check if scale when rotate.
    if (180 == pScaleInfo->nRotate)
    {
        if ((pScaleInfo->mSrcValidRect.Width != pScaleInfo->mDstValidRect.Width)
            || (pScaleInfo->mSrcValidRect.Height != pScaleInfo->mDstValidRect.Height))
        {
            aloge("fatal error! g2d rotate forbid scale![%dx%d][%dx%d]", pScaleInfo->mSrcValidRect.Width,
                pScaleInfo->mSrcValidRect.Height, pScaleInfo->mDstValidRect.Width, pScaleInfo->mDstValidRect.Height);
            return -1;
        }
    }
    else if ((90 == pScaleInfo->nRotate) || (270 == pScaleInfo->nRotate))
    {
        if ((pScaleInfo->mSrcValidRect.Width != pScaleInfo->mDstValidRect.Height)
            || (pScaleInfo->mSrcValidRect.Height != pScaleInfo->mDstValidRect.Width))
        {
            aloge("fatal error! g2d rotate forbid scale![%dx%d][%dx%d]", pScaleInfo->mSrcValidRect.Width,
                pScaleInfo->mSrcValidRect.Height, pScaleInfo->mDstValidRect.Width, pScaleInfo->mDstValidRect.Height);
            return -1;
        }
    }

    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pScaleInfo->eSrcPixFormat, &eSrcFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pScaleInfo->eSrcPixFormat);
        return -1;
    }
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pScaleInfo->eDstPixFormat, &eDstFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pScaleInfo->eDstPixFormat);
        return -1;
    }
    ret = convert_v4l2_colorspace_to_g2d_color_gmt(pScaleInfo->eSrcColorSpace, &eSrcG2dColorSpace);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! v4l2 color space[0x%x] is invalid!", pScaleInfo->eSrcColorSpace);
        return -1;
    }
    ret = convert_v4l2_colorspace_to_g2d_color_gmt(pScaleInfo->eDstColorSpace, &eDstG2dColorSpace);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst v4l2 color space[0x%x] is invalid!", pScaleInfo->eDstColorSpace);
        return -1;
    }

    //config blit
    memset(&blit, 0, sizeof(g2d_blt_h));
    switch (pScaleInfo->nRotate)
    {
    case 0:
        blit.flag_h = G2D_BLT_NONE_H;
        break;
    case 90:
        blit.flag_h = G2D_ROT_90;
        break;
    case 180:
        blit.flag_h = G2D_ROT_180;
        break;
    case 270:
        blit.flag_h = G2D_ROT_270;
        break;
    default:
        aloge("fatal error! rotation[%d] is invalid!", pScaleInfo->nRotate);
        blit.flag_h = G2D_BLT_NONE_H;
        return -1;
    }
    //alogd("src format[0x%x] -> dst format[0x%x]", eSrcFormat, eDstFormat);
    //blit.src_image_h.bbuff = 1;
    //blit.src_image_h.color = 0xff;
    blit.src_image_h.format = eSrcFormat;
    blit.src_image_h.laddr[0] = pScaleInfo->mSrcPhyAddrs[0];
    blit.src_image_h.laddr[1] = pScaleInfo->mSrcPhyAddrs[1];
    blit.src_image_h.laddr[2] = pScaleInfo->mSrcPhyAddrs[2];
    //blit.src_image_h.haddr[] =
    blit.src_image_h.width = pScaleInfo->mSrcPicSize.Width;
    blit.src_image_h.height = pScaleInfo->mSrcPicSize.Height;
    blit.src_image_h.align[0] = 0;
    blit.src_image_h.align[1] = 0;
    blit.src_image_h.align[2] = 0;
    blit.src_image_h.clip_rect.x = pScaleInfo->mSrcValidRect.X;
    blit.src_image_h.clip_rect.y = pScaleInfo->mSrcValidRect.Y;
    blit.src_image_h.clip_rect.w = pScaleInfo->mSrcValidRect.Width;
    blit.src_image_h.clip_rect.h = pScaleInfo->mSrcValidRect.Height;
    blit.src_image_h.gamut = eSrcG2dColorSpace;
    blit.src_image_h.bpremul = 0;
    //blit.src_image_h.alpha = 0xff;
    blit.src_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.src_image_h.fd = -1;
    blit.src_image_h.use_phy_addr = 1;
    //blit.src_image_h.color_range =

    //blit.dst_image_h.bbuff = 1;
    //blit.dst_image_h.color = 0xff;
    blit.dst_image_h.format = eDstFormat;
    blit.dst_image_h.laddr[0] = pScaleInfo->mDstPhyAddrs[0];
    blit.dst_image_h.laddr[1] = pScaleInfo->mDstPhyAddrs[1];
    blit.dst_image_h.laddr[2] = pScaleInfo->mDstPhyAddrs[2];
    //blit.dst_image_h.haddr[] =
    blit.dst_image_h.width = pScaleInfo->mDstPicSize.Width;
    blit.dst_image_h.height = pScaleInfo->mDstPicSize.Height;
    blit.dst_image_h.align[0] = 0;
    blit.dst_image_h.align[1] = 0;
    blit.dst_image_h.align[2] = 0;
    blit.dst_image_h.clip_rect.x = pScaleInfo->mDstValidRect.X;
    blit.dst_image_h.clip_rect.y = pScaleInfo->mDstValidRect.Y;
    blit.dst_image_h.clip_rect.w = pScaleInfo->mDstValidRect.Width;
    blit.dst_image_h.clip_rect.h = pScaleInfo->mDstValidRect.Height;
    blit.dst_image_h.gamut = eDstG2dColorSpace;
    blit.dst_image_h.bpremul = 0;
    //blit.dst_image_h.alpha = 0xff;
    blit.dst_image_h.mode = G2D_PIXEL_ALPHA;   //G2D_PIXEL_ALPHA, G2D_GLOBAL_ALPHA
    blit.dst_image_h.fd = -1;
    blit.dst_image_h.use_phy_addr = 1;
    //blit.dst_image_h.color_range =

    ret = ioctl(nG2dFd, G2D_CMD_BITBLT_H, (unsigned long)&blit);
    if(ret < 0)
    {
        aloge("fatal error! bit-block(image) transfer failed[%d]", ret);
        system("cd /sys/class/sunxi_dump;echo 0x14A8000,0x14A8100 > dump;cat dump");
    }

    return ret;
}

