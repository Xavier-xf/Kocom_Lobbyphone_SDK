#include "sys_linux_ioctl.h"
#define LOG_TAG "sample_uvcout"
#include <utils/plat_log.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/prctl.h>
#include <getopt.h>
#include <signal.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <linux/usb/ch9.h>
#include <linux/g2d_driver.h>

#include "linux/videodev2.h"
#include "linux/usb/video.h"
#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include "media/mpi_venc.h"
#include "media/mpi_sys.h"
#include "mm_common.h"
#include "mm_comm_venc.h"
#include "mm_comm_rc.h"
#include <confparser.h>
#include <utils/PIXEL_FORMAT_E_g2d_format_convert.h>
#include <mpi_videoformat_conversion.h>
#include <cdx_list.h>

#include "include/uvc.h"
#include "sample_uvcout.h"
#include "sample_uvcout_config.h"
#include "../common/awaiisp_common.h"
#include "../common/sample_common_venc.h"

#define SUPPORT_AWAIISP

//#define AWAIISP_SWITCH_NORMAL_MODE  AWAIISP_MODE_NORMAL_GAMMA  // for day 8bit
#define AWAIISP_SWITCH_NORMAL_MODE  AWAIISP_MODE_NORMAL        // for day 10bit
#define DAY_TO_NIGHT_SIGNAL_CNT     30
#define NIGHT_TO_DAY_SIGNAL_CNT     30
#define DAY_TO_NIGHT_THRESHOLD      60
#define NIGHT_TO_DAY_THRESHOLD      200

#define TIME_TEST
#ifdef TIME_TEST
#include "SystemBase.h"
#endif

#define ISP_RUN             (1)
//#define DEBUG_SAVE_SEND_UVC_STREAM
#ifdef DEBUG_SAVE_SEND_UVC_STREAM
#define DEBUG_SAVE_SEND_UVC_STREAM_FILE      "/mnt/extsd/save_stream.raw"
#endif

static int g_bWaitVencDone = 0;
static int g_bSampleExit   = 0;
static SampleUVCContext *g_sample_uvcout_context = NULL;

static unsigned int getSysTickMs()
{
    unsigned int ms = 0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ms = tv.tv_sec*1000 + tv.tv_usec/1000;
    return ms;
}

static int OpenUVCDevice(SampleUVCDevice *uvc_device, SampleUVCConfig *uvc_config)
{
    struct v4l2_capability stCap;
    int iRet;
    int iFd;
    char pcDevName[256];

    sprintf(pcDevName, "/dev/video%d", uvc_config->uvc_dev);
    alogd("open uvc device[%s]", pcDevName);
    iFd = open(pcDevName, O_RDWR | O_NONBLOCK);
    if (iFd < 0) {
        aloge("open video device failed: device[%s] %s\n", pcDevName, strerror(errno));
        goto open_err;
    }
    iRet = ioctl(iFd, VIDIOC_QUERYCAP, &stCap);
    if (iRet < 0) {
        aloge("unable to query device: %s (%d)\n", strerror(errno), errno);
        goto query_cap_err;
    }
    alogd("device is %s on bus %s\n", stCap.card, stCap.bus_info);

    uvc_device->uvc_dev = iFd;

    return 0;

query_cap_err:
    close(iFd);
open_err:
    return -1;
}

static void CloseUVCDevice(SampleUVCDevice *pUVCDev)
{
    close(pUVCDev->uvc_dev);
}

static int OpenG2d(SampleUVCDevice *uvc_device, SampleUVCConfig *uvc_config)
{
    int nFrmSize = 0;

    uvc_device->g2d_dev = open("/dev/g2d", O_RDWR, 0);
    if (uvc_device->g2d_dev < 0)
    {
        aloge("fatal error! open g2d device fail!");
        return -1;
    }
    memset(&uvc_device->g2d_proc_frame, 0, sizeof(VIDEO_FRAME_INFO_S));
    uvc_device->g2d_proc_frame.VFrame.mWidth = uvc_config->capture_width;
    uvc_device->g2d_proc_frame.VFrame.mHeight = uvc_config->capture_height;
    uvc_device->g2d_proc_frame.VFrame.mPixelFormat = MM_PIXEL_FORMAT_YUYV_PACKAGE_422;
    nFrmSize = uvc_device->g2d_proc_frame.VFrame.mWidth * uvc_device->g2d_proc_frame.VFrame.mHeight;
    AW_MPI_SYS_MmzAlloc_Cached(&uvc_device->g2d_proc_frame.VFrame.mPhyAddr[0], \
        &uvc_device->g2d_proc_frame.VFrame.mpVirAddr[0], nFrmSize*2);
    if (0 == uvc_device->g2d_proc_frame.VFrame.mPhyAddr[0] || NULL == uvc_device->g2d_proc_frame.VFrame.mpVirAddr[0])
    {
        aloge("fatal error! alloc g2d proc y data buffer fail!");
        return -1;
    }

    return 0;
}

static int G2dProc(SampleUVCDevice *uvc_device, VIDEO_FRAME_INFO_S *pSrcFrame, VIDEO_FRAME_INFO_S *pDstFrame)
{
    int ret = 0;
    g2d_blt_h blt;
    g2d_fmt_enh eSrcFormat, eDstFormat;
    int nFrmSize = 0;

    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pSrcFrame->VFrame.mPixelFormat, &eSrcFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! src pixel format[0x%x] is invalid!", pSrcFrame->VFrame.mPixelFormat);
        return -1;
    }
    ret = convert_PIXEL_FORMAT_E_to_g2d_fmt_enh(pDstFrame->VFrame.mPixelFormat, &eDstFormat);
    if(ret!=SUCCESS)
    {
        aloge("fatal error! dst pixel format[0x%x] is invalid!", pDstFrame->VFrame.mPixelFormat);
        return -1;
    }

    memset(&blt, 0, sizeof(g2d_blt_h));
    blt.flag_h = G2D_BLT_NONE_H;
    blt.src_image_h.format = eSrcFormat;
    blt.src_image_h.laddr[0] = (unsigned int)pSrcFrame->VFrame.mPhyAddr[0];
    blt.src_image_h.laddr[1] = (unsigned int)pSrcFrame->VFrame.mPhyAddr[1];
    blt.src_image_h.laddr[2] = (unsigned int)pSrcFrame->VFrame.mPhyAddr[2];
    blt.src_image_h.width = pSrcFrame->VFrame.mWidth;
    blt.src_image_h.height = pSrcFrame->VFrame.mHeight;
    blt.src_image_h.align[0] = 0;
    blt.src_image_h.align[1] = 0;
    blt.src_image_h.align[2] = 0;
    blt.src_image_h.clip_rect.x = 0;
    blt.src_image_h.clip_rect.y = 0;
    blt.src_image_h.clip_rect.w = pSrcFrame->VFrame.mWidth;
    blt.src_image_h.clip_rect.h = pSrcFrame->VFrame.mHeight;
    blt.src_image_h.gamut = G2D_BT601;
    blt.src_image_h.bpremul = 0;
    blt.src_image_h.mode = G2D_PIXEL_ALPHA;
    blt.src_image_h.fd = -1;
    blt.src_image_h.use_phy_addr = 1;

    blt.dst_image_h.format = eDstFormat;
    blt.dst_image_h.laddr[0] = (unsigned int)pDstFrame->VFrame.mPhyAddr[0];
    blt.dst_image_h.laddr[1] = (unsigned int)pDstFrame->VFrame.mPhyAddr[1];
    blt.dst_image_h.laddr[2] = (unsigned int)pDstFrame->VFrame.mPhyAddr[2];
    blt.dst_image_h.width = pDstFrame->VFrame.mWidth;
    blt.dst_image_h.height = pDstFrame->VFrame.mHeight;
    blt.dst_image_h.align[0] = 0;
    blt.dst_image_h.align[1] = 0;
    blt.dst_image_h.align[2] = 0;
    blt.dst_image_h.clip_rect.x = 0;
    blt.dst_image_h.clip_rect.y = 0;
    blt.dst_image_h.clip_rect.w = pDstFrame->VFrame.mWidth;
    blt.dst_image_h.clip_rect.h = pDstFrame->VFrame.mHeight;
    blt.dst_image_h.gamut = G2D_BT601;
    blt.dst_image_h.bpremul = 0;
    blt.dst_image_h.mode = G2D_PIXEL_ALPHA;
    blt.dst_image_h.fd = -1;
    blt.dst_image_h.use_phy_addr = 1;

    ret = ioctl(uvc_device->g2d_dev, G2D_CMD_BITBLT_H, (unsigned long)&blt);
    if (ret < 0)
    {
        aloge("fatal error! g2d proc fail!");
        return -1;
    }
    nFrmSize = pDstFrame->VFrame.mWidth * pDstFrame->VFrame.mHeight;
    AW_MPI_SYS_MmzFlushCache(pDstFrame->VFrame.mPhyAddr[0], pDstFrame->VFrame.mpVirAddr[0], nFrmSize*2);

    return 0;
}

static void CloseG2d(SampleUVCDevice *uvc_device)
{
    if (uvc_device->g2d_dev >= 0)
    {
        close(uvc_device->g2d_dev);
        uvc_device->g2d_dev = -1;
    }
    if (uvc_device->g2d_proc_frame.VFrame.mPhyAddr[0] > 0 || NULL != uvc_device->g2d_proc_frame.VFrame.mpVirAddr[0])
    {
        AW_MPI_SYS_MmzFree(uvc_device->g2d_proc_frame.VFrame.mPhyAddr[0], uvc_device->g2d_proc_frame.VFrame.mpVirAddr[0]);
        uvc_device->g2d_proc_frame.VFrame.mPhyAddr[0] = 0;
        uvc_device->g2d_proc_frame.VFrame.mpVirAddr[0] = NULL;
    }
    if (uvc_device->g2d_proc_frame.VFrame.mPhyAddr[1] > 0 || NULL != uvc_device->g2d_proc_frame.VFrame.mpVirAddr[1])
    {
        AW_MPI_SYS_MmzFree(uvc_device->g2d_proc_frame.VFrame.mPhyAddr[1], uvc_device->g2d_proc_frame.VFrame.mpVirAddr[1]);
        uvc_device->g2d_proc_frame.VFrame.mPhyAddr[1] = 0;
        uvc_device->g2d_proc_frame.VFrame.mpVirAddr[1] = NULL;
    }
    if (uvc_device->g2d_proc_frame.VFrame.mPhyAddr[2] > 0 || NULL != uvc_device->g2d_proc_frame.VFrame.mpVirAddr[2])
    {
        AW_MPI_SYS_MmzFree(uvc_device->g2d_proc_frame.VFrame.mPhyAddr[2], uvc_device->g2d_proc_frame.VFrame.mpVirAddr[2]);
        uvc_device->g2d_proc_frame.VFrame.mPhyAddr[2] = 0;
        uvc_device->g2d_proc_frame.VFrame.mpVirAddr[2] = NULL;
    }
}

ERRORTYPE VencStreamCallBack(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    VENC_STREAM_S *pFrame = (VENC_STREAM_S *)pEventData;

    switch (event) {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            if (pFrame != NULL) {
                g_bWaitVencDone = 1;
            }
            break;

        default:
            break;
    }

    return SUCCESS;
}

static inline int DoUVCVideoBufProcess(SampleUVCDevice *uvc_device)
{
    SampleUVCContext *uvc_context = (SampleUVCContext *)uvc_device->privite_data;
    struct v4l2_buffer stBuf;
    int iRet;

    pthread_mutex_lock(&uvc_device->frame_list_lock);
    if (list_empty(&uvc_device->frame_valid_list)) {
//        aloge("valid frame is empty!!\n");
        iRet = -1;
        goto frm_empty;
    }

    SampleUVCOutBuf *pUvcOutBuf;
    pUvcOutBuf = list_first_entry(&uvc_device->frame_valid_list, SampleUVCOutBuf, mList);
    memset(&stBuf, 0, sizeof(struct v4l2_buffer));
    stBuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    stBuf.memory = V4L2_MEMORY_MMAP;
    iRet = ioctl(uvc_device->uvc_dev, VIDIOC_DQBUF, &stBuf);
    if (iRet < 0) {
        aloge("Unable to dequeue buffer: %s (%d).\n", strerror(errno), errno);
        goto qbuf_err;
    }

    /* fill the v4l2 buffer */
    unsigned int data_size = pUvcOutBuf->iDataSize0 + pUvcOutBuf->iDataSize1 + pUvcOutBuf->iDataSize2;
    if (uvc_device->frame_buffer[stBuf.index].iBufLen >= data_size)
    {
        memcpy(uvc_device->frame_buffer[stBuf.index].pVirAddr, pUvcOutBuf->pcData, data_size);
        stBuf.bytesused = data_size;
    }
    else
    {
        aloge("fatal error! buf size %d is less than data size %d", uvc_device->frame_buffer[stBuf.index].iBufLen, data_size);
        stBuf.bytesused = 0;
    }
#ifdef DEBUG_SAVE_SEND_UVC_STREAM
    FILE *fp = fopen(DEBUG_SAVE_SEND_UVC_STREAM_FILE, "ab+");
    if (fp)
    {
        fwrite(uvc_device->frame_buffer[stBuf.index].pVirAddr, stBuf.bytesused, 1, fp);
        fclose(fp);
    }
#endif
    iRet = ioctl(uvc_device->uvc_dev, VIDIOC_QBUF, &stBuf);
    if (iRet < 0) {
        aloge("Unable to requeue buffer: %s (%d).\n", strerror(errno), errno);
        goto dqbuf_err;
    }
qbuf_err:
dqbuf_err:
    pUvcOutBuf->iDataSize0 = 0;
    pUvcOutBuf->iDataSize1 = 0;
    pUvcOutBuf->iDataSize2 = 0;
    memset(pUvcOutBuf->pcData, 0, pUvcOutBuf->iDataBufSize);
    list_move_tail(&pUvcOutBuf->mList, &uvc_device->frame_idle_list);
//  AW_MPI_VENC_ReleaseStream(ve_chn, &pUvcOutBuf->stStream);
frm_empty:
    pthread_mutex_unlock(&uvc_device->frame_list_lock);
    return iRet;
}

static int ConfigUVCBufByVE(SampleUVCOutBuf *pBuf, VENC_STREAM_S *pVencBuf, VencHeaderData *venc_header)
{
    unsigned int offsetLen = 0;
    pBuf->iDataSize0 = 0;
    pBuf->iDataSize1 = 0;
    pBuf->iDataSize2 = 0;

    if (pVencBuf->mpPack[0].mDataType.enH264EType == H264E_NALU_ISLICE)
    {
        pBuf->iDataSize0 += venc_header->nLength;
        memcpy(pBuf->pcData, venc_header->pBuffer, venc_header->nLength);
        offsetLen += venc_header->nLength;
    }

    if (pVencBuf->mpPack != NULL && pVencBuf->mpPack->mpAddr0 && pVencBuf->mpPack->mLen0)
    {
        if (pBuf->iDataBufSize < pVencBuf->mpPack->mLen0)
        {
            aloge("fatal error! buf remain size %d is less than data size0 %d", pBuf->iDataBufSize, pVencBuf->mpPack->mLen0);
            return -1;
        }
        pBuf->iDataSize0 += pVencBuf->mpPack->mLen0;
        memcpy(pBuf->pcData + offsetLen, pVencBuf->mpPack->mpAddr0, pVencBuf->mpPack->mLen0);
        offsetLen += pVencBuf->mpPack->mLen0;
    }
    if (pVencBuf->mpPack != NULL && pVencBuf->mpPack->mpAddr1 && pVencBuf->mpPack->mLen1)
    {
        if (pBuf->iDataBufSize < pVencBuf->mpPack->mLen1 + offsetLen)
        {
            aloge("fatal error! buf remain size %d is less than data size1 %d", pBuf->iDataBufSize - offsetLen, pVencBuf->mpPack->mLen1);
            return -1;
        }
        pBuf->iDataSize1 = pVencBuf->mpPack->mLen1;
        memcpy(pBuf->pcData + offsetLen, pVencBuf->mpPack->mpAddr1, pVencBuf->mpPack->mLen1);
        offsetLen += pVencBuf->mpPack->mLen1;
    }
    if (pVencBuf->mpPack != NULL && pVencBuf->mpPack->mpAddr2 && pVencBuf->mpPack->mLen2)
    {
        if (pBuf->iDataBufSize < pVencBuf->mpPack->mLen2 + offsetLen)
        {
            aloge("fatal error! buf remain size %d is less than data size2 %d", pBuf->iDataBufSize - offsetLen, pVencBuf->mpPack->mLen2);
            return -1;
        }
        pBuf->iDataSize2 = pVencBuf->mpPack->mLen2;
        memcpy(pBuf->pcData + offsetLen, pVencBuf->mpPack->mpAddr2, pVencBuf->mpPack->mLen2);
        offsetLen += pVencBuf->mpPack->mLen2;
    }
    return 0;
}

static int ConfigUVCBufByVI(SampleUVCOutBuf *pBuf, VIDEO_FRAME_INFO_S *pFrmInfo)
{
    unsigned int offsetLen = 0;

    if (pFrmInfo->VFrame.mpVirAddr[0])
    {
        pBuf->iDataSize0 = pFrmInfo->VFrame.mWidth * pFrmInfo->VFrame.mHeight * 2;
        memcpy(pBuf->pcData + offsetLen, pFrmInfo->VFrame.mpVirAddr[0], pBuf->iDataSize0);
        offsetLen += pBuf->iDataSize0;
    }
    if (pFrmInfo->VFrame.mpVirAddr[1])
    {
        pBuf->iDataSize1 = pFrmInfo->VFrame.mWidth * pFrmInfo->VFrame.mHeight / 2;
        memcpy(pBuf->pcData + offsetLen, pFrmInfo->VFrame.mpVirAddr[1], pBuf->iDataSize1);
        offsetLen += pBuf->iDataSize1;
    }
    if (pFrmInfo->VFrame.mpVirAddr[2])
    {
        pBuf->iDataSize2 = pFrmInfo->VFrame.mWidth * pFrmInfo->VFrame.mHeight / 2;
        memcpy(pBuf->pcData + offsetLen, pFrmInfo->VFrame.mpVirAddr[2], pBuf->iDataSize2);
        offsetLen += pBuf->iDataSize2;
    }

    return 0;
}

static void *DualStreamCaptureThread(void *thread_data)
{
    int frame_len = 0;
    ERRORTYPE eRet = SUCCESS;
    VENC_BUF_STATUS venc_buf_status;
    VENC_STREAM_S venc_stream;
    VENC_PACK_S venc_pack;
    VencInsertData venc_insert_data;
    int offset = 0;
    SampleUVCContext *uvc_context = (SampleUVCContext *)thread_data;
    SampleUVCDevice *uvc_device = (SampleUVCDevice *)&uvc_context->uvc_device;
    SampleUVCConfig *uvc_config = (SampleUVCConfig *)&uvc_context->uvc_config;
    VencHeaderData venc_headerdata;

    uvc_device->dual_stream_capture_thread_running = TRUE;
    memset(&venc_headerdata, 0, sizeof(VencHeaderData));
    if (uvc_config->dual_stream_type == PT_H264)
        AW_MPI_VENC_GetH264SpsPpsInfo(uvc_device->dual_stream_ve_chn, &venc_headerdata);
    else if (uvc_config->dual_stream_type == PT_H265)
        AW_MPI_VENC_GetH265SpsPpsInfo(uvc_device->dual_stream_ve_chn, &venc_headerdata);
    while (1)
    {
        if (uvc_device->capture_thread_exit_flag)
            break;

        eRet = AW_MPI_VENC_GetInsertDataBufStatus(uvc_device->ve_chn, &venc_buf_status);
        if (venc_buf_status == BUF_IDLE)
        {
            memset(&venc_pack, 0, sizeof(VENC_PACK_S));
            memset(&venc_stream, 0, sizeof(VENC_STREAM_S));
            venc_stream.mPackCount = 1;
            venc_stream.mpPack = &venc_pack;
            eRet = AW_MPI_VENC_GetStream(uvc_device->dual_stream_ve_chn, &venc_stream, 200);
            if (eRet != SUCCESS)
                continue;

            if (venc_stream.mpPack[0].mDataType.enH264EType == H264E_NALU_ISLICE)
                frame_len = venc_stream.mpPack[0].mLen0 + venc_stream.mpPack[0].mLen1 + venc_stream.mpPack[0].mLen2 + venc_headerdata.nLength;
            else
                frame_len = venc_stream.mpPack[0].mLen0 + venc_stream.mpPack[0].mLen1 + venc_stream.mpPack[0].mLen2;
            if (uvc_device->dual_stream_tmp_buffer_len < frame_len)
            {
                alogd("realloc dual stream tmp buffer! buffer len change %d -> %d!",
                    uvc_device->dual_stream_tmp_buffer_len, frame_len);
                if (uvc_device->dual_stream_tmp_buffer)
                {
                    free(uvc_device->dual_stream_tmp_buffer);
                    uvc_device->dual_stream_tmp_buffer = NULL;
                }
                uvc_device->dual_stream_tmp_buffer = malloc(frame_len);
                if (!uvc_device->dual_stream_tmp_buffer)
                    aloge("alloc dual stream tmp buffer fail!");
                uvc_device->dual_stream_tmp_buffer_len = frame_len;
            }
            if (!uvc_device->dual_stream_tmp_buffer)
            {
                aloge("fatal error! dual stream tmp buffer is null!");
                AW_MPI_VENC_ReleaseStream(uvc_device->dual_stream_ve_chn, &venc_stream);
                continue;
            }
            offset = 0;
            if (venc_stream.mpPack[0].mDataType.enH264EType == H264E_NALU_ISLICE)
            {
                memcpy(uvc_device->dual_stream_tmp_buffer, venc_headerdata.pBuffer, venc_headerdata.nLength);
                offset += venc_headerdata.nLength;
            }
            if (venc_stream.mpPack[0].mLen0)
            {
                memcpy(uvc_device->dual_stream_tmp_buffer + offset, venc_stream.mpPack[0].mpAddr0, venc_stream.mpPack[0].mLen0);
                offset += venc_stream.mpPack[0].mLen0;
            }
            if (venc_stream.mpPack[0].mLen1)
            {
                memcpy(uvc_device->dual_stream_tmp_buffer + offset, venc_stream.mpPack[0].mpAddr1, venc_stream.mpPack[0].mLen1);
                offset += venc_stream.mpPack[0].mLen1;
            }
            if (venc_stream.mpPack[0].mLen2)
            {
                memcpy(uvc_device->dual_stream_tmp_buffer + offset, venc_stream.mpPack[0].mpAddr2, venc_stream.mpPack[0].mLen2);
            }
            memset(&venc_insert_data, 0, sizeof(VencInsertData));
            venc_insert_data.pBuffer = uvc_device->dual_stream_tmp_buffer;
            venc_insert_data.nDataLen = frame_len;
            venc_insert_data.nFrameRate = uvc_config->dual_stream_framerate;
            eRet = AW_MPI_VENC_SetInsertData(uvc_device->ve_chn, &venc_insert_data);
            if (eRet != SUCCESS)
                alogw("venc %d insert data fail! data 0x%p len %d",
                    uvc_device->dual_stream_ve_chn, uvc_device->dual_stream_tmp_buffer, frame_len);
            AW_MPI_VENC_ReleaseStream(uvc_device->dual_stream_ve_chn, &venc_stream);
        }
    }
    return (void *)NULL;
}

static void *CaptureThread(void *pArg)
{
    SampleUVCContext *pContext = (SampleUVCContext *)pArg;
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    SampleUVCConfig *uvc_config = &pContext->uvc_config;
    ERRORTYPE eRet = SUCCESS;
    int ret = 0;
    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    VIDEO_FRAME_INFO_S stVideoFrmameInfo;
    VencHeaderData stVencHeadDat;

    uvc_device->capture_thread_running = TRUE;
    alogd("get video frame thread running!");

    memset(&stVencHeadDat, 0, sizeof(VencHeaderData));
    eRet = AW_MPI_VENC_GetH264SpsPpsInfo(uvc_device->ve_chn, &stVencHeadDat);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! venc get h264 stream header fail!");
    }
    alogd("get h264 spspps info, info len[%d]", stVencHeadDat.nLength);

    awaiisp_mode last_aiisp_mode = AWAIISP_MODE_NPU;
    int mStreamDataCnt = 0;

    while(1)
    {
        if (uvc_device->capture_thread_exit_flag)
        {
            alogd("capture thread exit!");
            break;
        }
        if ((MM_INVALID_CHN != uvc_device->ve_chn) && (PT_BUTT != uvc_config->encode_type))
        {
            memset(&stVencStream, 0, sizeof(VENC_STREAM_S));
            memset(&stVencPack, 0, sizeof(VENC_PACK_S));
            stVencStream.mPackCount = 1;
            stVencStream.mpPack = &stVencPack;
            eRet = AW_MPI_VENC_GetStream(uvc_device->ve_chn, &stVencStream, 4000);
            if (SUCCESS != eRet)
            {
                alogw("get venc frame fail!");
                continue;
            }
        }
        else
        {
            memset(&stVideoFrmameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));
            eRet = AW_MPI_VI_GetFrame(uvc_device->vipp_dev, uvc_device->vipp_chn, &stVideoFrmameInfo, 200);
            if (SUCCESS != eRet)
            {
                alogw("get vi frame fail!");
                continue;
            }
        }

        pthread_mutex_lock(&uvc_device->frame_list_lock);
        SampleUVCOutBuf *pBuf = list_first_entry_or_null(&uvc_device->frame_idle_list, SampleUVCOutBuf, mList);
        if (NULL == pBuf)
        {
            if ((MM_INVALID_CHN != uvc_device->ve_chn) && (PT_BUTT != uvc_config->encode_type))
            {
                AW_MPI_VENC_ReleaseStream(uvc_device->ve_chn, &stVencStream);
            }
            else
            {
                AW_MPI_VI_ReleaseFrame(uvc_device->vipp_dev, uvc_device->vipp_chn, &stVideoFrmameInfo);
            }
            pthread_mutex_unlock(&uvc_device->frame_list_lock);
            continue;
        }
        if ((MM_INVALID_CHN != uvc_device->ve_chn) && (PT_BUTT != uvc_config->encode_type))
        {
            ret = ConfigUVCBufByVE(pBuf, &stVencStream, &stVencHeadDat);
            if (ret)
            {
                AW_MPI_VENC_ReleaseStream(uvc_device->ve_chn, &stVencStream);
                pthread_mutex_unlock(&uvc_device->frame_list_lock);
                continue;
            }
            AW_MPI_VENC_ReleaseStream(uvc_device->ve_chn, &stVencStream);
        }
        else
        {
            ret = G2dProc(uvc_device, &stVideoFrmameInfo, &uvc_device->g2d_proc_frame);
            if (ret)
            {
                aloge("fatal error! g2d proc fail!");
                AW_MPI_VI_ReleaseFrame(uvc_device->vipp_dev, uvc_device->vipp_chn, &stVideoFrmameInfo);
                pthread_mutex_unlock(&uvc_device->frame_list_lock);
                continue;
            }
            ConfigUVCBufByVI(pBuf, &uvc_device->g2d_proc_frame);
            AW_MPI_VI_ReleaseFrame(uvc_device->vipp_dev, uvc_device->vipp_chn, &stVideoFrmameInfo);
        }
        list_move_tail(&pBuf->mList, &uvc_device->frame_valid_list);
        pthread_mutex_unlock(&uvc_device->frame_list_lock);
    }
    pthread_exit(NULL);
}

static void* aiispSwitchThread(void* pThreadData)
{
    SampleUVCContext *pContext = (SampleUVCContext*)pThreadData;
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    SampleUVCConfig *uvc_config = &pContext->uvc_config;
    ISP_DEV nIsp = uvc_device->isp_dev;
    VI_DEV nVipp = uvc_device->vipp_dev;

    char strThreadName[32];
    sprintf(strThreadName, "aiispSwitch%d", nIsp);
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    char *pAiIspCfgBinPath = uvc_config->mAiIspCfgBinPath;
    char *pAiIspCfgBinPath2 = uvc_config->mAiIspCfgBinPath2;
    int nAiIspSwitchInterval = uvc_config->mAiIspSwitchInterval;
    int nFrameRate = uvc_config->capture_framerate;
    int nAiIspSwitchCase = uvc_config->mAiIspSwitchCase;
    int nAiIspSwitchDropFrameNum = uvc_config->mAiIspSwitchDropFrameNum;

    int loop_cnt = 0;
    int interval_ms = 0;
    awaiisp_mode mode = uvc_config->mAiIspMode;
    awaiisp_mode last_aiisp_mode = mode;

    int nAiIspAutoSwitchEnable = 0;
    if (uvc_config->mAiIspAutoSwitchEnable)
    {
        nAiIspAutoSwitchEnable = 1;
    }

    if (nFrameRate)
        interval_ms = 1000 / nFrameRate;
    if (0 == interval_ms)
        interval_ms = 1000;

    int night_to_day_signal_cnt = 0;
    int day_to_night_signal_cnt = 0;
    int env_light_level = 0;

    awaiisp_mode switch_normal_mode = (AWAIISP_COMMON_SWITCH_CASE_AIISP_DAY_ALL_8BIT == nAiIspSwitchCase) ? AWAIISP_MODE_NORMAL_GAMMA : AWAIISP_MODE_NORMAL;
    int nAiIspSwitchReleaseResEnable = uvc_config->mAiIspSwitchReleaseResEnable;

    while (!uvc_device->aiisp_switch_thread_exit_flag)
    {
        if (nAiIspAutoSwitchEnable)
        {
            // switch aiisp by ae param
            env_light_level = AW_MPI_ISP_GetEvLvAdj(nIsp);
            alogv("isp %d, env_light_level:%d, day2nignt:%d, night2day:%d", nIsp, env_light_level, day_to_night_signal_cnt, night_to_day_signal_cnt);

            if (env_light_level < DAY_TO_NIGHT_THRESHOLD)
            {
                if (++day_to_night_signal_cnt >= DAY_TO_NIGHT_SIGNAL_CNT)
                {
                    day_to_night_signal_cnt = 0;
                    mode = AWAIISP_MODE_NPU;
                }
                night_to_day_signal_cnt = 0;
            }
            else if (env_light_level > NIGHT_TO_DAY_THRESHOLD)
            {
                if (++night_to_day_signal_cnt >= NIGHT_TO_DAY_SIGNAL_CNT)
                {
                    night_to_day_signal_cnt = 0;
                    mode = switch_normal_mode;
                }
                day_to_night_signal_cnt = 0;
            }
            else
            {
                night_to_day_signal_cnt = 0;
                day_to_night_signal_cnt = 0;
            }
            interval_ms = 100;
        }
        else
        {
            // for aiisp switch test by manually specifying interval
            if ((nAiIspSwitchInterval) && (0 == (++loop_cnt) % nAiIspSwitchInterval))
            {
                mode = (AWAIISP_MODE_NPU == last_aiisp_mode) ? switch_normal_mode : AWAIISP_MODE_NPU;
            }
        }

        if (last_aiisp_mode != mode)
        {
            if (nAiIspAutoSwitchEnable)
                alogw("isp%d switch mode %d -> %d, env_light_level:%d", nIsp, last_aiisp_mode, mode, env_light_level);
            else
                alogw("isp%d switch mode %d -> %d", nIsp, last_aiisp_mode, mode);

            awaiisp_common_switch_param switch_param;
            memset(&switch_param, 0, sizeof(awaiisp_common_switch_param));
            switch_param.channel_param[0].enable = 1;
            switch_param.channel_param[0].isp = nIsp;
            switch_param.channel_param[0].vipp = nVipp;
            switch_param.channel_param[0].switch_case = nAiIspSwitchCase;
            switch_param.channel_param[0].drop_frame_num = nAiIspSwitchDropFrameNum;
            switch_param.channel_param[0].config.mode = mode;
            if (AWAIISP_MODE_NPU == mode)
            {
                switch_param.channel_param[0].isp_cfg_bin_path = pAiIspCfgBinPath;
                switch_param.channel_param[0].config.release_aiisp_resources = 0;
            }
            else
            {
                switch_param.channel_param[0].isp_cfg_bin_path = pAiIspCfgBinPath2;
                switch_param.channel_param[0].config.release_aiisp_resources = nAiIspSwitchReleaseResEnable;
            }
            awaiisp_common_switch_mode(&switch_param);

            last_aiisp_mode = mode;
        }

        usleep(interval_ms * 1000);
    }

    return NULL;
}

static int InitFrameList(int iFrmNum, SampleUVCContext *pContext)
{
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    SampleUVCConfig *uvc_config = &pContext->uvc_config;
    unsigned int iFrameSize = 0;
    if (V4L2_PIX_FMT_YUYV == uvc_device->format)
    {
        iFrameSize = uvc_device->frame->wWidth * uvc_device->frame->wHeight * 2;
    }
    else if (V4L2_PIX_FMT_H264 == uvc_device->format)
    {
        iFrameSize = uvc_device->frame->wWidth * uvc_device->frame->wHeight / 10;
        alogd("H264 iFrameSize=%d", iFrameSize);
    }
    else
    {
        iFrameSize = uvc_device->frame->wWidth * uvc_device->frame->wHeight * 3 / 2 / 3;
    }

    alogd("begin to alloc frame list.\n");
    if (iFrmNum <= 0) {
        aloge("frame list number must bigger than 0!!\n");
        return -1;
    }

    INIT_LIST_HEAD(&uvc_device->frame_idle_list);
    INIT_LIST_HEAD(&uvc_device->frame_valid_list);
    INIT_LIST_HEAD(&uvc_device->frame_used_list);

    SampleUVCOutBuf *pBufTmp;
    pthread_mutex_init(&uvc_device->frame_list_lock, NULL);
    for (int i = 0; i < iFrmNum; i++) {
        pBufTmp = malloc(sizeof(SampleUVCOutBuf));
        pBufTmp->pcData = malloc(iFrameSize);
        pBufTmp->iDataBufSize = iFrameSize;
        pBufTmp->iDataSize0 = 0;
        pBufTmp->iDataSize1 = 0;
        pBufTmp->iDataSize2 = 0;
        list_add_tail(&pBufTmp->mList, &uvc_device->frame_idle_list);
    }

    return 0;
}

static void FlushFrameList(SampleUVCContext *pContext)
{
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    SampleUVCOutBuf *pBufTmp, *pBufTmpNext;

    if (list_empty(&uvc_device->frame_used_list))
    {
        list_for_each_entry_safe(pBufTmp, pBufTmpNext, &uvc_device->frame_used_list, mList)
        {
            list_move_tail(&pBufTmp->mList, &uvc_device->frame_idle_list);
        }
    }
    if (list_empty(&uvc_device->frame_valid_list))
    {
        list_for_each_entry_safe(pBufTmp, pBufTmpNext, &uvc_device->frame_valid_list, mList)
        {
            list_move_tail(&pBufTmp->mList, &uvc_device->frame_idle_list);
        }
    }
    if (list_empty(&uvc_device->frame_idle_list))
    {
        list_for_each_entry_safe(pBufTmp, pBufTmpNext, &uvc_device->frame_idle_list, mList)
        {
            memset(pBufTmp->pcData, 0, pBufTmp->iDataBufSize);
            pBufTmp->iDataSize0 = 0;
            pBufTmp->iDataSize1 = 0;
            pBufTmp->iDataSize2 = 0;
        }
    }
}

static void DeinitFrameList(SampleUVCContext *pContext)
{
    SampleUVCDevice *uvc_device = &pContext->uvc_device;

    alogd("begin to free frame list.\n");
    SampleUVCOutBuf *pBufTmp;
    SampleUVCOutBuf *pBufTmpNext;
    list_for_each_entry_safe(pBufTmp, pBufTmpNext, &uvc_device->frame_used_list, mList)
    {
        list_del(&pBufTmp->mList);
        if (pBufTmp->pcData)
        {
            free(pBufTmp->pcData);
            pBufTmp->pcData = NULL;
        }
        pBufTmp->iDataBufSize = 0;
        pBufTmp->iDataSize0 = 0;
        pBufTmp->iDataSize1 = 0;
        pBufTmp->iDataSize2 = 0;
        if (pBufTmp)
        {
            free(pBufTmp);
            pBufTmp = NULL;
        }
    }
    list_for_each_entry_safe(pBufTmp, pBufTmpNext, &uvc_device->frame_idle_list, mList)
    {
        list_del(&pBufTmp->mList);
        if (pBufTmp->pcData)
        {
            free(pBufTmp->pcData);
            pBufTmp->pcData = NULL;
        }
        pBufTmp->iDataBufSize = 0;
        pBufTmp->iDataSize0 = 0;
        pBufTmp->iDataSize1 = 0;
        pBufTmp->iDataSize2 = 0;
        if (pBufTmp)
        {
            free(pBufTmp);
            pBufTmp = NULL;
        }
    }
    list_for_each_entry_safe(pBufTmp, pBufTmpNext, &uvc_device->frame_idle_list, mList)
    {
        list_del(&pBufTmp->mList);
        if (pBufTmp->pcData)
        {
            free(pBufTmp->pcData);
            pBufTmp->pcData = NULL;
        }
        pBufTmp->iDataBufSize = 0;
        pBufTmp->iDataSize0 = 0;
        pBufTmp->iDataSize1 = 0;
        pBufTmp->iDataSize2 = 0;
        if (pBufTmp)
        {
            free(pBufTmp);
            pBufTmp = NULL;
        }
    }

    pthread_mutex_destroy(&uvc_device->frame_list_lock);
}

static int createVipp(SampleUVCContext *pContext, BOOL dual_stream)
{
    int ret = 0;
    ERRORTYPE eRet = SUCCESS;
    VI_DEV vipp_dev = MM_INVALID_DEV;
    VI_CHN vipp_chn = MM_INVALID_CHN;
    ISP_DEV isp_dev = MM_INVALID_DEV;
    VI_ATTR_S stVippAttr;
    PIXEL_FORMAT_E format;
    int width, height, framerate;
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    SampleUVCConfig *uvc_config = &pContext->uvc_config;

    if (dual_stream)
    {
        vipp_dev = uvc_config->dual_stream_vipp_dev;
        isp_dev = uvc_config->dual_stream_isp_dev;
        width = uvc_config->dual_stream_width;
        height = uvc_config->dual_stream_height;
        format = uvc_config->dual_stream_capture_format;
        framerate = uvc_config->dual_stream_capture_framerate;
    }
    else
    {
        vipp_dev = uvc_config->vipp_dev;
        isp_dev = uvc_config->isp_dev;
        width = uvc_config->capture_width;
        height = uvc_config->capture_height;
        format = uvc_config->capture_format;
        framerate = uvc_config->capture_framerate;
    }

    memset(&stVippAttr, 0, sizeof(stVippAttr));
    if (uvc_config->enable_encode_online && PT_BUTT != uvc_config->encode_type && vipp_dev == 0)
    {
        stVippAttr.mOnlineEnable = 1;
        stVippAttr.mOnlineShareBufNum = 1;
    }
    stVippAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    stVippAttr.memtype = V4L2_MEMORY_MMAP;
    stVippAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(format);
    stVippAttr.format.field = V4L2_FIELD_NONE;
    stVippAttr.format.colorspace = V4L2_COLORSPACE_REC709;
    stVippAttr.format.width = width;
    stVippAttr.format.height = height;
    stVippAttr.fps = framerate;
    stVippAttr.use_current_win = 0;
    stVippAttr.wdr_mode = 0;
    stVippAttr.nbufs = 3;//5;
    stVippAttr.nplanes = 2;
    stVippAttr.drop_frame_num = 5;
    stVippAttr.mbEncppEnable = uvc_config->enable_encpp;
    if (FALSE == dual_stream && uvc_config->mAiIspEnable)
    {
        stVippAttr.tdm_rxbuf_cnt = uvc_config->mAiIspTdmRxBufNum;
    }
    alogd("pixformat 0x%x size %dx%d framerate %d", stVippAttr.format.pixelformat,
        stVippAttr.format.width, stVippAttr.format.height, stVippAttr.fps);

    eRet = AW_MPI_VI_CreateVipp(vipp_dev);
    if (SUCCESS != eRet)
    {
        ret = -1;
        vipp_dev = MM_INVALID_DEV;
        aloge("fatal error! create vipp[%d] fail!", vipp_dev);
        goto _create_vipp_result;
    }
    AW_MPI_VI_SetVippAttr(vipp_dev, &stVippAttr);

#if ISP_RUN
    if (isp_dev >= 0)
    {
        AW_MPI_ISP_Run(isp_dev);
    }
    else
    {
        isp_dev = MM_INVALID_DEV;
    }
#endif

#ifdef SUPPORT_AWAIISP
    if (FALSE == dual_stream && uvc_config->mAiIspEnable)
    {
        awaiisp_common_config_param common_param;
        memset(&common_param, 0, sizeof(awaiisp_common_config_param));
        strncpy(common_param.config.lut_model_file, uvc_config->mAiIspLutNbgFilePath, AWAIISP_FILE_PATH_MAX);
        strncpy(common_param.config.model_file, uvc_config->mAiIspNbgFilePath, AWAIISP_FILE_PATH_MAX);
        common_param.config.model_version = uvc_config->mAiIspModelVersion;
        common_param.config.width = uvc_config->mAiIspWidth;
        common_param.config.height = uvc_config->mAiIspHeight;
        common_param.config.tdm_rxbuf_cnt = uvc_config->mAiIspTdmRxBufNum;
        common_param.config.reserve0 = uvc_config->mAiIspReserve0;
        common_param.config.reserve1 = uvc_config->mAiIspReserve1;
        common_param.config.reserve2 = uvc_config->mAiIspReserve2;
        common_param.config.npu_ref_buf_reduce_enable = uvc_config->mAiIspNpuRefBufReduceEnable;
        common_param.config.mode = uvc_config->mAiIspMode;
        if (AWAIISP_MODE_NPU == uvc_config->mAiIspMode)
        {
            common_param.isp_cfg_bin_path = uvc_config->mAiIspCfgBinPath;
            common_param.config.unprepared_aiisp_resources_advance = 0;
        }
        else
        {
            common_param.isp_cfg_bin_path = uvc_config->mAiIspCfgBinPath2;
            /**
              decide whether to prepare aiisp resources in advance for certain scenarios.
              If the memory resources are sufficient, it is recommended to prepare aiisp resources in advance.
              If no switching test is conducted, there is no need to prepare.
            */
            if (uvc_config->mAiIspAutoSwitchEnable || uvc_config->mAiIspSwitchInterval)
                common_param.config.unprepared_aiisp_resources_advance = 0;
            else
                common_param.config.unprepared_aiisp_resources_advance = 1;
        }
        eRet = awaiisp_common_enable(isp_dev, &common_param);
        if (SUCCESS != eRet)
        {
            ret = -1;
            aloge("fatal error! awaiisp common enable %d fail!", isp_dev);
            goto _create_vipp_result;
        }

        if (uvc_config->mAiIspAutoSwitchEnable || uvc_config->mAiIspSwitchInterval)
        {
            uvc_device->aiisp_switch_thread_exit_flag = FALSE;
            eRet = pthread_create(&uvc_device->aiisp_switch_thread_trd, NULL, aiispSwitchThread, pContext);
            if (eRet < 0) {
                aloge("caeate GetVideoFrameThread failed!!\n");
            }
        }
    }
#endif
    eRet = AW_MPI_VI_EnableVipp(vipp_dev);
    if (SUCCESS != eRet)
    {
        ret = -1;
        aloge("fatal error! enable vipp[%d] fail!", vipp_dev);
        goto _create_vipp_result;
    }
    vipp_chn = 0;
    eRet = AW_MPI_VI_CreateVirChn(vipp_dev, vipp_chn, NULL);
    if (SUCCESS != eRet)
    {
        vipp_chn = MM_INVALID_CHN;
        aloge("fatal error! create vir chn[%d] fail!", vipp_chn);
        return -1;
    }
    alogd("create vipp[%d] vir vi chn[%d]", vipp_dev, vipp_chn);

_create_vipp_result:
    if (dual_stream)
    {
        uvc_device->dual_stream_vipp_dev = vipp_dev;
        uvc_device->dual_stream_vipp_chn = vipp_chn;
        uvc_device->dual_stream_isp_dev = isp_dev;
    }
    else
    {
        uvc_device->vipp_dev = vipp_dev;
        uvc_device->vipp_chn = vipp_chn;
        uvc_device->isp_dev = isp_dev;
    }
    return ret;
}

static int createVeChn(SampleUVCContext *pContext, BOOL dual_stream)
{
    ERRORTYPE eRet = SUCCESS;
    VI_DEV vipp_dev;
    VENC_CHN ve_chn;
    VENC_CHN_ATTR_S stVEncChnAttr;
    VENC_RC_PARAM_S stVEncRcParam;
    PAYLOAD_TYPE_E encode_type;
    int width, height, framerate, bitrate;
    PIXEL_FORMAT_E pix_format;
    BOOL success_flag = FALSE;
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    SampleUVCConfig *uvc_config = &pContext->uvc_config;

    if (dual_stream)
    {
        vipp_dev = uvc_config->dual_stream_vipp_dev;
        encode_type = uvc_config->dual_stream_type;
        width = uvc_config->dual_stream_width;
        height = uvc_config->dual_stream_height;
        framerate = uvc_config->dual_stream_framerate;
        bitrate = uvc_config->dual_stream_bitrate;
        pix_format = uvc_config->dual_stream_capture_format;
    }
    else
    {
        vipp_dev = uvc_config->vipp_dev;
        encode_type = uvc_config->encode_type;
        width = uvc_config->encode_width;
        height = uvc_config->encode_height;
        framerate = uvc_config->encode_framerate;
        bitrate = uvc_config->encode_bitrate;
        pix_format = uvc_config->capture_format;
    }
    alogd("online %d vipp %d encode %d size %dx%d framerate %d bitrate %d format 0x%x",
        uvc_config->enable_encode_online, vipp_dev, encode_type, width, height, framerate, bitrate, pix_format);

    memset(&stVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&stVEncRcParam, 0, sizeof(VENC_RC_PARAM_S));
    if (uvc_config->enable_encode_online && vipp_dev == 0)
    {
        stVEncChnAttr.VeAttr.mOnlineEnable = 1;
        stVEncChnAttr.VeAttr.mOnlineShareBufNum = 1;
    }
    stVEncChnAttr.VeAttr.Type         = encode_type;
    stVEncChnAttr.VeAttr.SrcPicWidth  = width;
    stVEncChnAttr.VeAttr.SrcPicHeight = height;
    stVEncChnAttr.VeAttr.Field        = VIDEO_FIELD_FRAME;
    stVEncChnAttr.VeAttr.PixelFormat  = pix_format;
    stVEncChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_REC709;
    stVEncChnAttr.VeAttr.Rotate       = ROTATE_NONE;
    stVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = 0;
    stVEncChnAttr.EncppAttr.eEncppSharpSetting = uvc_config->enable_encpp?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;
    stVEncChnAttr.RcAttr.mProductMode = PRODUCT_STATIC_IPC;
    //stVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (PT_H264 == stVEncChnAttr.VeAttr.Type || PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
        if (framerate)
        {
            vbvThreshSize = bitrate/8/framerate*15;
        }
        vbvBufSize = bitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
        alogd("vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);
    }

    if(PT_H264 == stVEncChnAttr.VeAttr.Type || PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
#if VE_IPC_PRODUCT_ROTATING
        stVEncRcParam.EnIFrmMbRcMoveStatusEnable = 1;
        stVEncRcParam.EnIFrmMbRcMoveStatus = 0;
#else
        stVEncRcParam.EnIFrmMbRcMoveStatusEnable = 1;
        stVEncRcParam.EnIFrmMbRcMoveStatus = 3;
#endif
    }

    if (PT_H264 == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        stVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        stVEncChnAttr.VeAttr.AttrH264e.PicWidth = width;
        stVEncChnAttr.VeAttr.AttrH264e.PicHeight = height;
        stVEncChnAttr.VeAttr.AttrH264e.mLevel = 0;
        stVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        stVEncChnAttr.VeAttr.AttrH264e.mThreshSize = vbvThreshSize;
        stVEncChnAttr.VeAttr.AttrH264e.BufSize = vbvBufSize;
        stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        if (VENC_RC_MODE_H264CBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate = bitrate;
            stVEncChnAttr.RcAttr.mAttrH264Cbr.mSrcFrmRate = framerate;
            stVEncChnAttr.RcAttr.mAttrH264Cbr.mDstFrmRate = framerate;
            stVEncRcParam.ParamH264Cbr.mMaxQp = 45;
            stVEncRcParam.ParamH264Cbr.mMinQp = 10;
            stVEncRcParam.ParamH264Cbr.mMaxPqp = 45;
            stVEncRcParam.ParamH264Cbr.mMinPqp = 10;
            stVEncRcParam.ParamH264Cbr.mQpInit = 35;
            stVEncRcParam.ParamH264Cbr.mbEnMbQpLimit = 1;
        }
        else if (VENC_RC_MODE_H264VBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = bitrate;
            stVEncChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = framerate;
            stVEncChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = framerate;
            stVEncRcParam.ParamH264Vbr.mMaxQp = 45;
            stVEncRcParam.ParamH264Vbr.mMinQp = 25;
            stVEncRcParam.ParamH264Vbr.mMaxPqp = 45;
            stVEncRcParam.ParamH264Vbr.mMinPqp = 25;
            stVEncRcParam.ParamH264Vbr.mQpInit = 37;
#if VE_IPC_PRODUCT_ROTATING
            stVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
            stVEncRcParam.ParamH264Vbr.mQuality = 10;
#else
            stVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 1;
            stVEncRcParam.ParamH264Vbr.mQuality = 1;
#endif
            stVEncRcParam.ParamH264Vbr.mMovingTh = 20;
            stVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
            stVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
        }
    }
    else if (PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        stVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        stVEncChnAttr.VeAttr.AttrH265e.mPicWidth = width;
        stVEncChnAttr.VeAttr.AttrH265e.mPicHeight = height;
        stVEncChnAttr.VeAttr.AttrH265e.mLevel = 0;
        stVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        stVEncChnAttr.VeAttr.AttrH265e.mThreshSize = vbvThreshSize;
        stVEncChnAttr.VeAttr.AttrH265e.mBufSize = vbvBufSize;
        stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        if (VENC_RC_MODE_H265CBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate = bitrate;
            stVEncChnAttr.RcAttr.mAttrH265Cbr.mSrcFrmRate = framerate;
            stVEncChnAttr.RcAttr.mAttrH265Cbr.mDstFrmRate = framerate;
            stVEncRcParam.ParamH265Cbr.mMaxQp = 45;
            stVEncRcParam.ParamH265Cbr.mMinQp = 10;
            stVEncRcParam.ParamH265Cbr.mMaxPqp = 45;
            stVEncRcParam.ParamH265Cbr.mMinPqp = 10;
            stVEncRcParam.ParamH265Cbr.mQpInit = 35;
            stVEncRcParam.ParamH265Cbr.mbEnMbQpLimit = 1;
        }
        else if (VENC_RC_MODE_H265VBR == stVEncChnAttr.RcAttr.mRcMode)
        {
            stVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = bitrate;
            stVEncChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = framerate;
            stVEncChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = framerate;
            stVEncRcParam.ParamH265Vbr.mMaxQp = 45;
            stVEncRcParam.ParamH265Vbr.mMinQp = 25;
            stVEncRcParam.ParamH265Vbr.mMaxPqp = 45;
            stVEncRcParam.ParamH265Vbr.mMinPqp = 25;
            stVEncRcParam.ParamH265Vbr.mQpInit = 37;
#if VE_IPC_PRODUCT_ROTATING
            stVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
            stVEncRcParam.ParamH265Vbr.mQuality = 10;
#else
            stVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 1;
            stVEncRcParam.ParamH265Vbr.mQuality = 1;
#endif
            stVEncRcParam.ParamH265Vbr.mMovingTh = 20;
            stVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
            stVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
        }
    }
    else if(PT_MJPEG == stVEncChnAttr.VeAttr.Type)
    {
        stVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame  = TRUE;
        stVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth  = width;
        stVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = height;
        stVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        stVEncChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = framerate;
        stVEncChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = framerate;
        stVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = bitrate;
    }
    else
    {
        eRet = FAILURE;
        ve_chn = MM_INVALID_CHN;
        aloge("fatal error! unsupport vencoder type[%d]!!\n", stVEncChnAttr.VeAttr.Type);
        goto _create_ve_chn_result;
    }

    configBitsClipParam(&stVEncRcParam);

    ve_chn = 0;
    while (1)
    {
        if (ve_chn >= VENC_MAX_CHN_NUM)
        {
            aloge("fatal error! create ve chn fail!");
            break;
        }
        eRet = AW_MPI_VENC_CreateChn(ve_chn, &stVEncChnAttr);
        if (eRet == SUCCESS)
        {
            success_flag = TRUE;
            alogd("create venc channel[%d] success!", ve_chn);
            break;
        }
        ve_chn++;
    }
    if (!success_flag)
    {
        eRet = FAILURE;
        aloge("set venc channle frame rate failed!! venc_chn[%d]\n", ve_chn);
        goto _create_ve_chn_result;
    }

    /* set framerate in AW_MPI_VENC_CreateChn */
    /*VENC_FRAME_RATE_S stVencFrameRate;
    stVencFrameRate.SrcFrmRate = uvc_config->capture_framerate;
    stVencFrameRate.DstFrmRate = framerate;
    eRet = AW_MPI_VENC_SetFrameRate(ve_chn, &stVencFrameRate);
    if (eRet < 0)
    {
        aloge("set venc channle frame rate failed!! venc_chn[%d]\n", ve_chn);
        goto _create_ve_chn_result;
    }*/

    if ((PT_H264 == stVEncChnAttr.VeAttr.Type) || (PT_H265 == stVEncChnAttr.VeAttr.Type))
    {
        AW_MPI_VENC_SetRcParam(ve_chn, &stVEncRcParam);
    }
    if (PT_H264 == stVEncChnAttr.VeAttr.Type)
    {
        VENC_PARAM_H264_VUI_S h264_vui;
        memset(&h264_vui, 0, sizeof(VENC_PARAM_H264_VUI_S));
        AW_MPI_VENC_GetH264Vui(ve_chn, &h264_vui);
        h264_vui.VuiTimeInfo.timing_info_present_flag = 1;
        h264_vui.VuiTimeInfo.fixed_frame_rate_flag = 1;
        h264_vui.VuiTimeInfo.num_units_in_tick = 1000;
        h264_vui.VuiTimeInfo.time_scale = h264_vui.VuiTimeInfo.num_units_in_tick * framerate * 2;
        AW_MPI_VENC_SetH264Vui(ve_chn, &h264_vui);
    }
    else if (PT_H265 == stVEncChnAttr.VeAttr.Type)
    {
        VENC_PARAM_H265_VUI_S h265_vui;
        memset(&h265_vui, 0, sizeof(VENC_PARAM_H265_VUI_S));
        AW_MPI_VENC_GetH265Vui(ve_chn, &h265_vui);
        h265_vui.VuiTimeInfo.timing_info_present_flag = 1;
        h265_vui.VuiTimeInfo.num_units_in_tick = 1000;
        /* Notices: the protocol syntax states that h265 does not need to be multiplied by 2. */
        h265_vui.VuiTimeInfo.time_scale = h265_vui.VuiTimeInfo.num_units_in_tick * framerate;
        h265_vui.VuiTimeInfo.num_ticks_poc_diff_one_minus1 = h265_vui.VuiTimeInfo.num_units_in_tick;
        AW_MPI_VENC_SetH265Vui(ve_chn, &h265_vui);
    }

    setVenc2Dnr(ve_chn);
    setVenc3Dnr(ve_chn);
    setVencSuperFrameCfg(ve_chn, bitrate, framerate);

    MPPCallbackInfo cbInfo;
    cbInfo.callback = (MPPCallbackFuncType)&VencStreamCallBack;
    cbInfo.cookie = (void *)uvc_device;
    AW_MPI_VENC_RegisterCallback(ve_chn, &cbInfo);
    alogd("create ve chn[%d], encoder type[%d]", ve_chn, stVEncChnAttr.VeAttr.Type);
_create_ve_chn_result:
    if (dual_stream)
        uvc_device->dual_stream_ve_chn = ve_chn;
    else
        uvc_device->ve_chn = ve_chn;
    return eRet;
}

static int CreateCaptureThread(SampleUVCContext *pContext)
{
    ERRORTYPE eRet = SUCCESS;
    int ret = 0;
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    SampleUVCConfig *uvc_config = &pContext->uvc_config;

    /*MPP_SYS_CONF_S mSysConf;
    mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&mSysConf);
    eRet = AW_MPI_SYS_Init();
    if (eRet < 0) {
        aloge("AW_MPI_SYS_Init failed!");
        goto _exit;
    }*/

    if (V4L2_PIX_FMT_MJPEG == uvc_device->format)
    {
        uvc_config->capture_format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        uvc_config->encode_type = PT_MJPEG;
    }
    else if (V4L2_PIX_FMT_YUYV == uvc_device->format)
    {
        uvc_config->capture_format = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        uvc_config->encode_type = PT_BUTT;
    }
    else if (V4L2_PIX_FMT_H264 == uvc_device->format)
    {
        uvc_config->capture_format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        uvc_config->encode_type = PT_H264;
    }
    uvc_config->capture_width = uvc_device->frame->wWidth;
    uvc_config->capture_height = uvc_device->frame->wHeight;
    uvc_config->capture_framerate = 10000000 / uvc_device->frame->dwFrameInterval;
    uvc_config->encode_width = uvc_device->frame->wWidth;
    uvc_config->encode_height = uvc_device->frame->wHeight;
    uvc_config->encode_framerate = 10000000 / uvc_device->frame->dwFrameInterval;
    uvc_config->dual_stream_capture_format = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    uvc_config->dual_stream_capture_framerate = 10000000 / uvc_device->frame->dwFrameInterval;

    alogd("capture size[%dx%d], encode size[%dx%d] capture format 0x%x encode type 0x%x", \
        uvc_config->capture_width, uvc_config->capture_height, \
        uvc_config->encode_width, uvc_config->encode_height,
        uvc_config->capture_format, uvc_config->encode_type);

    if (uvc_config->enable_dual_stream && (uvc_device->format == V4L2_PIX_FMT_MJPEG))
    {
        eRet = createVipp(pContext, TRUE);
        if (eRet)
        {
            aloge("fatal error! create vipp fail!");
            goto _destroy_vipp;
        }
    }

    eRet = createVipp(pContext, FALSE);
    if (eRet)
    {
        aloge("fatal error! create vipp fail!");
        goto _destroy_vipp;
    }

    if (PT_BUTT != uvc_config->encode_type)
    {
        if (uvc_config->enable_dual_stream && (uvc_device->format == V4L2_PIX_FMT_MJPEG))
        {
            eRet = createVeChn(pContext, TRUE);
            if (eRet)
            {
                aloge("fatal error! create ve chn fail!");
                goto _destroy_vipp;
            }
        }

        eRet = createVeChn(pContext, FALSE);
        if (eRet)
        {
            aloge("fatal error! create ve chn fail!");
            goto _destroy_vipp;
        }

        if (uvc_config->enable_dual_stream && (uvc_device->format == V4L2_PIX_FMT_MJPEG))
        {
            MPP_CHN_S VIChn = {MOD_ID_VIU, uvc_device->dual_stream_vipp_dev, uvc_device->dual_stream_vipp_chn};
            MPP_CHN_S VEChn = {MOD_ID_VENC, 0, uvc_device->dual_stream_ve_chn};
            eRet = AW_MPI_SYS_Bind(&VIChn, &VEChn);
            if (SUCCESS != eRet)
            {
                aloge("fatal error1 bind vi and ve fail!");
                goto _destroy_ve;
            }
        }

        MPP_CHN_S VIChn = {MOD_ID_VIU, uvc_device->vipp_dev, uvc_device->vipp_chn};
        MPP_CHN_S VEChn = {MOD_ID_VENC, 0, uvc_device->ve_chn};
        eRet = AW_MPI_SYS_Bind(&VIChn, &VEChn);
        if (SUCCESS != eRet)
        {
            aloge("fatal error1 bind vi and ve fail!");
            goto _destroy_ve;
        }
    }
    else
    {
        uvc_device->ve_chn = MM_INVALID_CHN;
        ret = OpenG2d(uvc_device, uvc_config);
        if (ret)
        {
            aloge("fatal error! open g2d device fail!");
            goto _destroy_ve;
        }
    }
    alogd("initialize vi and venc success, videv[%d],vichn[%d],vencchn[%d],ispdev[%d]\n",
        uvc_device->vipp_dev, uvc_device->vipp_chn, \
        uvc_device->ve_chn, uvc_device->isp_dev);

    return 0;
_destroy_ve:
    if (MM_INVALID_CHN != uvc_device->ve_chn)
    {
        AW_MPI_VENC_DestroyChn(uvc_device->ve_chn);
        uvc_device->ve_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_CHN != uvc_device->dual_stream_ve_chn)
    {
        AW_MPI_VENC_DestroyChn(uvc_device->dual_stream_ve_chn);
        uvc_device->dual_stream_ve_chn = MM_INVALID_CHN;
    }
_destroy_vipp:
    if (MM_INVALID_CHN != uvc_device->vipp_chn)
    {
        AW_MPI_VI_DestroyVirChn(uvc_device->vipp_dev, uvc_device->vipp_chn);
        uvc_device->vipp_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_DEV != uvc_device->isp_dev)
    {
        AW_MPI_ISP_Stop(uvc_device->isp_dev);
        uvc_device->isp_dev = MM_INVALID_DEV;
    }
    if (MM_INVALID_DEV != uvc_device->vipp_dev)
    {
        AW_MPI_VI_DestroyVipp(uvc_device->vipp_dev);
        uvc_device->vipp_dev = MM_INVALID_DEV;
    }
    if (MM_INVALID_CHN != uvc_device->dual_stream_vipp_chn)
    {
        AW_MPI_VI_DestroyVirChn(uvc_device->dual_stream_vipp_dev, uvc_device->dual_stream_vipp_chn);
        uvc_device->dual_stream_vipp_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_DEV != uvc_device->dual_stream_isp_dev)
    {
        AW_MPI_ISP_Stop(uvc_device->dual_stream_isp_dev);
        uvc_device->dual_stream_isp_dev = MM_INVALID_DEV;
    }
    if (MM_INVALID_DEV != uvc_device->dual_stream_vipp_dev)
    {
        AW_MPI_VI_DestroyVipp(uvc_device->dual_stream_vipp_dev);
        uvc_device->dual_stream_vipp_dev = MM_INVALID_DEV;
    }
_sys_exit:
    //AW_MPI_SYS_Exit();
_exit:
    return eRet;
}

static int DestroyCaptureThread(SampleUVCContext *pContext)
{
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    if (pContext->uvc_device.g2d_dev >= 0)
    {
        CloseG2d(&pContext->uvc_device);
    }
    if (MM_INVALID_CHN != uvc_device->ve_chn)
    {
        AW_MPI_VENC_DestroyChn(uvc_device->ve_chn);
        uvc_device->ve_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_CHN != uvc_device->dual_stream_ve_chn)
    {
        AW_MPI_VENC_DestroyChn(uvc_device->dual_stream_ve_chn);
        uvc_device->dual_stream_ve_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_CHN != uvc_device->vipp_chn)
    {
        AW_MPI_VI_DestroyVirChn(uvc_device->vipp_dev, uvc_device->vipp_chn);
        uvc_device->vipp_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_CHN != uvc_device->dual_stream_vipp_chn)
    {
        AW_MPI_VI_DestroyVirChn(uvc_device->dual_stream_vipp_dev, uvc_device->dual_stream_vipp_chn);
        uvc_device->dual_stream_vipp_chn = MM_INVALID_CHN;
    }
    if (MM_INVALID_DEV != uvc_device->dual_stream_vipp_dev)
    {
        AW_MPI_VI_DisableVipp(uvc_device->dual_stream_vipp_dev);
        #if ISP_RUN
        if (MM_INVALID_DEV != uvc_device->dual_stream_isp_dev)
        {
            AW_MPI_ISP_Stop(uvc_device->dual_stream_isp_dev);
        }
        #endif
        AW_MPI_VI_DestroyVipp(uvc_device->dual_stream_vipp_dev);
        uvc_device->dual_stream_vipp_dev = MM_INVALID_DEV;
        uvc_device->dual_stream_isp_dev = MM_INVALID_DEV;
    }
    if (MM_INVALID_DEV != uvc_device->vipp_dev)
    {
        AW_MPI_VI_DisableVipp(uvc_device->vipp_dev);
        #if ISP_RUN
        if (MM_INVALID_DEV != uvc_device->isp_dev)
        {
            AW_MPI_ISP_Stop(uvc_device->isp_dev);
        }
        #endif
        AW_MPI_VI_DestroyVipp(uvc_device->vipp_dev);
        uvc_device->vipp_dev = MM_INVALID_DEV;
        uvc_device->isp_dev = MM_INVALID_DEV;
    }
    //AW_MPI_SYS_Exit();
    return 0;
}

static int StartCapture(SampleUVCContext *pContext)
{
    int iRet = 0;
    ERRORTYPE eRet = SUCCESS;
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    SampleUVCConfig *uvc_config = &pContext->uvc_config;
    alogd("vir chn is %d", uvc_device->vipp_chn);

    if (MM_INVALID_CHN != uvc_device->dual_stream_vipp_chn)
    {
        eRet = AW_MPI_VI_EnableVirChn(uvc_device->dual_stream_vipp_dev, uvc_device->dual_stream_vipp_chn);
        if (SUCCESS  != eRet)
        {
            aloge("fatal error! vipp[%d] enable vir chn[%d]", \
                uvc_device->dual_stream_vipp_dev, uvc_device->dual_stream_vipp_chn);
            return eRet;
        }
    }
    if (MM_INVALID_CHN != uvc_device->vipp_chn)
    {
        eRet = AW_MPI_VI_EnableVirChn(uvc_device->vipp_dev, uvc_device->vipp_chn);
        if (SUCCESS  != eRet)
        {
            aloge("fatal error! vipp[%d] enable vir chn[%d]", \
                uvc_device->vipp_dev, uvc_device->vipp_chn);
            return eRet;
        }
    }
    if (uvc_config->enable_dual_stream && MM_INVALID_CHN != uvc_device->dual_stream_ve_chn)
    {
        eRet = AW_MPI_VENC_StartRecvPic(uvc_device->dual_stream_ve_chn);
        if (SUCCESS != eRet)
        {
            aloge("fatal error! ve chn[%d] start recive picture fail!", \
                uvc_device->dual_stream_ve_chn);
            return eRet;
        }
        setVencRegionD3D(uvc_device->dual_stream_ve_chn, uvc_config->dual_stream_width, uvc_config->dual_stream_height);
    }
    if (MM_INVALID_CHN != uvc_device->ve_chn)
    {
        eRet = AW_MPI_VENC_StartRecvPic(uvc_device->ve_chn);
        if (SUCCESS != eRet)
        {
            aloge("fatal error! ve chn[%d] start recive picture fail!", \
                uvc_device->ve_chn);
            return eRet;
        }
        setVencRegionD3D(uvc_device->ve_chn, uvc_config->encode_width, uvc_config->encode_height);
    }
    if (uvc_config->enable_dual_stream && (uvc_device->format == V4L2_PIX_FMT_MJPEG))
    {
        uvc_device->dual_stream_capture_thread_exit_flag = FALSE;
        iRet = pthread_create(&uvc_device->dual_stream_capture_thread_trd, NULL, DualStreamCaptureThread, pContext);
        if (iRet < 0) {
            aloge("caeate GetVideoFrameThread failed!!\n");
            DestroyCaptureThread(pContext);
            return iRet;
        }
    }
    uvc_device->capture_thread_exit_flag = FALSE;
    iRet = pthread_create(&uvc_device->capture_thread_trd, NULL, CaptureThread, pContext);
    if (iRet < 0) {
        aloge("caeate GetVideoFrameThread failed!!\n");
        DestroyCaptureThread(pContext);
        return iRet;
    }

    return iRet;
}

static int StopCapture(SampleUVCContext *pContext)
{
    SampleUVCDevice *uvc_device = &pContext->uvc_device;
    SampleUVCConfig *uvc_config = &pContext->uvc_config;
    if (uvc_device->capture_thread_running)
    {
        uvc_device->capture_thread_exit_flag = TRUE;
        uvc_device->capture_thread_running = FALSE;
        pthread_join(uvc_device->capture_thread_trd, NULL);
    }
    if (uvc_device->dual_stream_capture_thread_running)
    {
        uvc_device->dual_stream_capture_thread_exit_flag = TRUE;
        uvc_device->dual_stream_capture_thread_running = FALSE;
        pthread_join(uvc_device->dual_stream_capture_thread_trd, NULL);
    }
#ifdef SUPPORT_AWAIISP
    if (uvc_config->mAiIspEnable)
    {
        awaiisp_common_disable(uvc_config->isp_dev);
        uvc_device->aiisp_switch_thread_exit_flag = TRUE;
        if (uvc_device->aiisp_switch_thread_trd)
            pthread_join(uvc_device->aiisp_switch_thread_trd, NULL);
    }
#endif
    if (MM_INVALID_CHN != uvc_device->ve_chn)
    {
        AW_MPI_VENC_StopRecvPic(uvc_device->ve_chn);
    }
    if (MM_INVALID_CHN != uvc_device->dual_stream_ve_chn)
    {
        AW_MPI_VENC_StopRecvPic(uvc_device->dual_stream_ve_chn);
    }
    if (MM_INVALID_CHN != uvc_device->vipp_chn)
    {
        AW_MPI_VI_DisableVirChn(uvc_device->vipp_dev, uvc_device->vipp_chn);
    }
    if (MM_INVALID_CHN != uvc_device->dual_stream_vipp_chn)
    {
        AW_MPI_VI_DisableVirChn(uvc_device->dual_stream_vipp_dev, uvc_device->dual_stream_vipp_chn);
    }
    if (MM_INVALID_CHN != uvc_device->ve_chn)
    {
        AW_MPI_VENC_ResetChn(uvc_device->ve_chn);
        //AW_MPI_VENC_DestroyChn(uvc_device->ve_chn);
    }
    if (MM_INVALID_CHN != uvc_device->dual_stream_ve_chn)
    {
        AW_MPI_VENC_ResetChn(uvc_device->dual_stream_ve_chn);
        //AW_MPI_VENC_DestroyChn(uvc_device->dual_stream_ve_chn);
    }

    return 0;
}

static int UVCVideoSetFormat(SampleUVCDevice *uvc_device)
{
    int iRet;
    struct v4l2_format stFormat;
    iRet = ioctl(uvc_device->uvc_dev, VIDIOC_G_FMT, &stFormat);
    alogv("width=[%d],height=[%d],sizeimage=[%d], format[%d]\n",
        stFormat.fmt.pix.width, stFormat.fmt.pix.height, stFormat.fmt.pix.sizeimage, stFormat.fmt.pix.pixelformat);

    alogv("iWidth=[%d],iHeight=[%d],iFormat=[0x%08x]\n",
        uvc_device->frame->wWidth, uvc_device->frame->wHeight, uvc_device->format);

    memset(&stFormat, 0, sizeof(struct v4l2_format));
    stFormat.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    stFormat.fmt.pix.pixelformat = uvc_device->format;
    stFormat.fmt.pix.width       = uvc_device->frame->wWidth;
    stFormat.fmt.pix.height      = uvc_device->frame->wHeight;
    stFormat.fmt.pix.field       = V4L2_FIELD_NONE;
    stFormat.fmt.pix.sizeimage   = uvc_device->frame->dwMaxVideoFrameBufferSize;
    alogv("VIDIOC_S_FMT size[%dx%d] fmt[%d] mjpeg[%d] nv21[%d] yuyv[%d]", \
        stFormat.fmt.pix.width, stFormat.fmt.pix.height, stFormat.fmt.pix.pixelformat, \
        V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_NV21, V4L2_PIX_FMT_YUYV);
    iRet = ioctl(uvc_device->uvc_dev, VIDIOC_S_FMT, &stFormat);
    if (iRet < 0) {
        aloge("VIDIOC_S_FMT failed!! %s (%d).\n", strerror(errno), errno);
    }

    iRet = ioctl(uvc_device->uvc_dev, VIDIOC_G_FMT, &stFormat);
    alogv("width=[%d],height=[%d],sizeimage=[%d], format[%d]\n",
        stFormat.fmt.pix.width, stFormat.fmt.pix.height, stFormat.fmt.pix.sizeimage, stFormat.fmt.pix.pixelformat);

    return iRet;
}

static void UVCFillStreamingControl(SampleUVCDevice *pstUVCDev, struct uvc_streaming_control *pstCtrl, int iFmtIndex, int iFrmIndex)
{
    SampleUVCContext *context = (SampleUVCContext *)pstUVCDev->privite_data;
    struct uvc_frame *frame = &context->configfs_para->format[iFmtIndex].frame[iFrmIndex];

    /* 0: interval fixed
     * 1: keyframe rate fixed
     * 2: Pframe rate fixed
     */
    pstCtrl->bmHint = 0;
    pstCtrl->bFormatIndex    = iFmtIndex + 1;
    pstCtrl->bFrameIndex     = iFrmIndex + 1;
    pstCtrl->dwFrameInterval = frame->dwDefaultFrameInterval;
    pstCtrl->wDelay = 0;
    pstCtrl->dwMaxVideoFrameSize = frame->dwMaxVideoFrameBufferSize;
    pstCtrl->dwMaxPayloadTransferSize = frame->dwMaxVideoFrameBufferSize;
//  pstCtrl->dwClockFrequency    = ;
    pstCtrl->bmFramingInfo = 3; //ignore in JPEG or MJPEG format
    pstCtrl->bPreferedVersion = 1;
    pstCtrl->bMinVersion = 1;
    pstCtrl->bMaxVersion = 1;
}

static void SubscribeUVCEvent(struct SampleUVCDevice *uvc_device)
{
    struct v4l2_event_subscription sub;

    UVCFillStreamingControl(uvc_device, &uvc_device->uvc_streaming_probe, 0, 0);
    UVCFillStreamingControl(uvc_device, &uvc_device->uvc_streaming_commit, 0, 0);

    /* subscribe events, for debug, subscribe all events */
    memset(&sub, 0, sizeof(sub));
    sub.type = UVC_EVENT_FIRST;
    ioctl(uvc_device->uvc_dev, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_CONNECT;
    ioctl(uvc_device->uvc_dev, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_DISCONNECT;
    ioctl(uvc_device->uvc_dev, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMON;
    ioctl(uvc_device->uvc_dev, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMOFF;
    ioctl(uvc_device->uvc_dev, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_SETUP;
    ioctl(uvc_device->uvc_dev, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_DATA;
    ioctl(uvc_device->uvc_dev, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_LAST;
    ioctl(uvc_device->uvc_dev, VIDIOC_SUBSCRIBE_EVENT, &sub);
}

static void UnSubscribeUVCEvent(struct SampleUVCDevice *uvc_device)
{
    struct v4l2_event_subscription sub;

    /*uvc_fill_streaming_control(pstUVCDev, &pstUVCDev->probe, 0, 0);
    uvc_fill_streaming_control(pstUVCDev, &pstUVCDev->commit, 0, 0);*/

    /* subscribe events, for debug, subscribe all events */
    memset(&sub, 0, sizeof(sub));
    sub.type = UVC_EVENT_FIRST;
    ioctl(uvc_device->uvc_dev, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_CONNECT;
    ioctl(uvc_device->uvc_dev, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_DISCONNECT;
    ioctl(uvc_device->uvc_dev, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_STREAMON;
    ioctl(uvc_device->uvc_dev, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_STREAMOFF;
    ioctl(uvc_device->uvc_dev, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_SETUP;
    ioctl(uvc_device->uvc_dev, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_DATA;
    ioctl(uvc_device->uvc_dev, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_LAST;
    ioctl(uvc_device->uvc_dev, VIDIOC_UNSUBSCRIBE_EVENT, sub);
}

static void DoUVCEventSetupClassControl(SampleUVCDevice *uvc_device, struct uvc_event *pstEvent, struct uvc_request_data *pstReq)
{
    uint8_t ucReq = pstEvent->req.bRequest;
    uint8_t ucCtrlSet = pstEvent->req.wValue >> 8;
    int control_data = 0;
    memset(pstReq->data, 0, pstEvent->req.wLength);
    pstReq->length = pstEvent->req.wLength;

    alogv("streaming request (req %s cs %s)\n",
        ucReq == UVC_SET_CUR?"SET_CUR":\
        ucReq == UVC_GET_CUR?"GET_CUR":\
        ucReq == UVC_GET_MIN?"GET_MIN":\
        ucReq == UVC_GET_MAX?"GET_MAX":\
        ucReq == UVC_GET_DEF?"GET_DEF":\
        ucReq == UVC_GET_RES?"GET_RES":\
        ucReq == UVC_GET_LEN?"GET_LEN":\
        ucReq == UVC_GET_INFO?"GET_INFO":"NULL",
        ucCtrlSet == UVC_VS_PROBE_CONTROL?"probe":\
        ucCtrlSet == UVC_VS_COMMIT_CONTROL?"commit":"unknown"
        );

    if(pstEvent->req.wIndex == 0x0200)
    {
        switch(pstEvent->req.bRequest){
            case UVC_SET_CUR:
                uvc_device->control = ucCtrlSet;
                break;
            case UVC_GET_DEF:
            case UVC_GET_CUR:
                switch(pstEvent->req.wValue >> 8)
                {
                    case UVC_PU_BRIGHTNESS_CONTROL:
                        alogv("UVC_PU_BRIGHTNESS_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                        control_data = 0;
                        break;
                    default:
                        aloge("GET_CUR unsupport control field 0x%x", pstEvent->req.wValue);
                        break;
                }
                pstReq->data[0] = control_data & 0x00ff;
                break;
            case UVC_GET_MIN:
                switch(pstEvent->req.wValue >> 8)
                {
                    case UVC_PU_BRIGHTNESS_CONTROL:
                        alogv("UVC_PU_BRIGHTNESS_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                        control_data = 0;
                        break;
                    default:
                        aloge("GET_CUR unsupport control field 0x%x", pstEvent->req.wValue);
                        break;
                }
                pstReq->data[0] = 0x0;
                break;
            case UVC_GET_MAX:
                switch(pstEvent->req.wValue >> 8)
                {
                    case UVC_PU_BRIGHTNESS_CONTROL:
                        alogv("UVC_PU_BRIGHTNESS_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                        control_data = 0;
                        break;
                    default:
                        aloge("GET_CUR unsupport control field 0x%x", pstEvent->req.wValue);
                        break;
                }
                pstReq->data[0] = control_data;
                break;
            case UVC_GET_RES:
                switch(pstEvent->req.wValue >> 8)
                {
                    case UVC_PU_BRIGHTNESS_CONTROL:
                        alogv("UVC_PU_BRIGHTNESS_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                        control_data = 0;
                        break;
                    default:
                        aloge("GET_CUR unsupport control field 0x%x", pstEvent->req.wValue);
                        break;
                }
                pstReq->data[0] = control_data;
                break;
            case UVC_GET_INFO:
                switch(pstEvent->req.wValue >> 8)
                {
                    case UVC_PU_BRIGHTNESS_CONTROL:
                        alogv("UVC_PU_BRIGHTNESS_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL");
                        control_data = 0;
                        break;
                    case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                        alogv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL");
                        control_data = 0;
                        break;
                    default:
                        aloge("GET_CUR unsupport control field 0x%x", pstEvent->req.wValue);
                        break;
                }
                break;
            default:
                break;
        }
    }
}

static int DoUVCEventSetupClassStreaming(SampleUVCDevice *pstUVCDev, struct uvc_event *pstEvent, struct uvc_request_data *pstReq)
{
    struct uvc_streaming_control *pstCtrl;
    uint8_t ucReq = pstEvent->req.bRequest;
    uint8_t ucCtrlSet = pstEvent->req.wValue >> 8;

    alogv("streaming request (req %s cs %02x)\n",
        ucReq == UVC_SET_CUR?"SET_CUR":\
        ucReq == UVC_GET_CUR?"GET_CUR":\
        ucReq == UVC_GET_MIN?"GET_MIN":\
        ucReq == UVC_GET_MAX?"GET_MAX":\
        ucReq == UVC_GET_DEF?"GET_DEF":\
        ucReq == UVC_GET_RES?"GET_RES":\
        ucReq == UVC_GET_LEN?"GET_LEN":\
        ucReq == UVC_GET_INFO?"GET_INFO":"NULL",
        ucCtrlSet == UVC_VS_PROBE_CONTROL?"probe":\
        ucCtrlSet == UVC_VS_COMMIT_CONTROL?"commit":"unknown"
        );

    if (ucCtrlSet != UVC_VS_PROBE_CONTROL && ucCtrlSet != UVC_VS_COMMIT_CONTROL)
        return 0;

    pstCtrl = (struct uvc_streaming_control *)&pstReq->data[0];
    pstReq->length = sizeof(struct uvc_streaming_control);

    switch (ucReq) {
    case UVC_SET_CUR:
        pstUVCDev->control = ucCtrlSet;
        pstReq->length = 34;
        break;

    case UVC_GET_CUR:
        if (ucCtrlSet == UVC_VS_PROBE_CONTROL)
            memcpy(pstCtrl, &pstUVCDev->uvc_streaming_probe, sizeof(struct uvc_streaming_control));
        else
            memcpy(pstCtrl, &pstUVCDev->uvc_streaming_commit, sizeof(struct uvc_streaming_control));
        break;

    case UVC_GET_MIN:
    case UVC_GET_MAX:
    case UVC_GET_DEF:
        UVCFillStreamingControl(pstUVCDev, pstCtrl, 0, 0);
        break;

    case UVC_GET_RES:
        memset(pstCtrl, 0, sizeof(struct uvc_streaming_control));
        break;

    case UVC_GET_LEN:
        pstReq->data[0] = 0x00;
        pstReq->data[1] = 0x22;
        pstReq->length = 2;
        break;

    case UVC_GET_INFO:
        pstReq->data[0] = 0x03;
        pstReq->length = 1;
        break;
    }

    return 0;
}

static int DoUVCEventSetupClass(SampleUVCDevice *pstUVCDev, struct uvc_event *pstEvent, struct uvc_request_data *pstReq)
{
    if ((pstEvent->req.bRequestType & USB_RECIP_MASK) != USB_RECIP_INTERFACE)
        return 0;

    switch (pstEvent->req.wIndex & 0xff) {
    case UVC_INTF_CONTROL:
        DoUVCEventSetupClassControl(pstUVCDev, pstEvent, pstReq);
        break;

    case UVC_INTF_STREAMING:
        DoUVCEventSetupClassStreaming(pstUVCDev, pstEvent, pstReq);
        break;

    default:
        break;
    }

    return 0;
}

static int DoUVCEventSetup(SampleUVCDevice *pstUVCDev, struct uvc_event *pstEvent, struct uvc_request_data *pstReq)
{
    switch(pstEvent->req.bRequestType & USB_TYPE_MASK) {
        /* USB_TYPE_STANDARD: kernel driver will process it */
        case USB_TYPE_STANDARD:
        case USB_TYPE_VENDOR:
            aloge("do not care\n");
            break;
        case USB_TYPE_CLASS:
            DoUVCEventSetupClass(pstUVCDev, pstEvent, pstReq);
            break;
        default: break;
    }

    return 0;
}

static int DoUVDReqReleaseBufs(SampleUVCDevice *pstUVCDev,  int iBufsNum);
static int DoUVCStreamOnOff(SampleUVCDevice *pstUVCDev, int iOn);
static int DoUVCEventData(SampleUVCDevice *uvc_device, struct uvc_request_data *pstReq)
{
    struct uvc_streaming_control *pstTarget;
    SampleUVCContext *context = (SampleUVCContext *)uvc_device->privite_data;

    if ((pstReq->length == 1) || (pstReq->length == 2))
    {
        int control_data = 0;
        if(pstReq->length == 1)
        {
            control_data = pstReq->data[0];
        }
        if(pstReq->length == 2)
        {
            control_data = pstReq->data[0] | (pstReq->data[1] << 8);
        }
        switch(uvc_device->control)
        {
            case UVC_PU_BRIGHTNESS_CONTROL: //0x02 brightness
                alogv("UVC_PU_BRIGHTNESS_CONTROL 0x%x", control_data);
                break;
            case UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                alogv("UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL 0x%x", control_data);
                break;
            case UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL:
                alogv("UVC_CT_EXPOSURE_TIME_RELATIVE_CONTROL 0x%x", control_data);
                break;
            default:
                aloge("unknown, control = 0x%02x\n", uvc_device->control);
                break;
        }
        return 0;
    }

    switch(uvc_device->control) {
        case UVC_VS_PROBE_CONTROL:
            alogv("setting probe control, length = %d\n", pstReq->length);
            pstTarget = &uvc_device->uvc_streaming_probe;
            break;

        case UVC_VS_COMMIT_CONTROL:
            alogv("setting commit control, length = %d\n", pstReq->length);
            pstTarget = &uvc_device->uvc_streaming_commit;
            break;

        default:
            alogv("setting unknown control, length = %d\n", pstReq->length);
            return 0;
    }

    struct uvc_streaming_control *pstCtrl;
    pstCtrl = (struct uvc_streaming_control*)&pstReq->data[0];

    if (uvc_device->enable_bulk_mode) {
	/* Notice: dwMaxPayloadTransferSize can't be 0 */
        pstTarget->bFormatIndex = pstCtrl->bFormatIndex;
        pstTarget->bFrameIndex = pstCtrl->bFrameIndex;
        /* TODO: select dwMaxVideoFrameSize, dwMaxPayloadTransferSize according bFormatIndex, bFrameIndex */
        pstTarget->dwFrameInterval = pstCtrl->dwFrameInterval;
    } else {
        memcpy(pstTarget, pstCtrl, sizeof(struct uvc_streaming_control));
    }

    alogv("pstCtrl->bmHint[%d],pstCtrl->bFormatIndex[%d],pstCtrl->bFrameIndex[%d],pstCtrl->dwFrameInterval[%d],\
        pstCtrl->dwMaxVideoFrameSize[%d],pstCtrl->dwMaxPayloadTransferSize[%d]",
        pstCtrl->bmHint, pstCtrl->bFormatIndex, pstCtrl->bFrameIndex, pstCtrl->dwFrameInterval,
        pstCtrl->dwMaxVideoFrameSize, pstCtrl->dwMaxPayloadTransferSize);
    alogv("MPJPEG[%d], JPEG[%d], YUYV[%d]", V4L2_PIX_FMT_MJPEG, V4L2_PIX_FMT_JPEG, V4L2_PIX_FMT_YUYV);

    if (uvc_device->control == UVC_VS_COMMIT_CONTROL) {
        if (pstCtrl->bFormatIndex >= (context->configfs_para->format_num+1) || \
            pstCtrl->bFrameIndex >= (context->configfs_para->format[pstCtrl->bFormatIndex - 1].frame_num+1))
        {
            alogw("unsupport format index[%d] frame index[%d], use default format index[1] frame index[1]", \
                pstCtrl->bFormatIndex, pstCtrl->bFrameIndex);
            pstCtrl->bFormatIndex = 1;
            pstCtrl->bFrameIndex = 1;
        }

        uvc_device->format = context->configfs_para->format[pstCtrl->bFormatIndex - 1].format;
        uvc_device->frame =
            find_uvc_frame(context->configfs_para, pstCtrl->bFormatIndex, pstCtrl->bFrameIndex);
        UVCVideoSetFormat(uvc_device);
        if (uvc_device->enable_bulk_mode) {
            /* bulk mode, stream on after commit */
	    if (uvc_device->is_streaming_flag) {
	        DoUVDReqReleaseBufs(uvc_device, 0);
		    DoUVCStreamOnOff(uvc_device, 0);
	    }
            DoUVDReqReleaseBufs(uvc_device, 3);
            DoUVCStreamOnOff(uvc_device, 1);
        }
    }

    return 0;
}

static int DoUVDReqReleaseBufs(SampleUVCDevice *uvc_device,  int iBufsNum)
{
    int iRet = 0;

    if (iBufsNum > 0) {
        struct v4l2_requestbuffers stReqBufs;
        memset(&stReqBufs, 0, sizeof(struct v4l2_requestbuffers));
        stReqBufs.count  = iBufsNum;
        stReqBufs.memory = V4L2_MEMORY_MMAP;
        stReqBufs.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        iRet = ioctl(uvc_device->uvc_dev, VIDIOC_REQBUFS, &stReqBufs);
        if (iRet < 0) {
            aloge("VIDIOC_REQBUFS failed!!\n");
            goto reqbufs_err;
        }

        uvc_device->frame_buffer = malloc(iBufsNum * sizeof(SampleUVCFrame));

        for (int i = 0; i < iBufsNum; i++) {
            struct v4l2_buffer stBuffer;
            memset(&stBuffer, 0, sizeof(struct v4l2_buffer));
            stBuffer.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            stBuffer.memory = V4L2_MEMORY_MMAP;
            stBuffer.index  = i;
            iRet = ioctl(uvc_device->uvc_dev, VIDIOC_QUERYBUF, &stBuffer);
            if (iRet < 0) {
                aloge("VIDIOC_QUERYBUF failed!!\n");
            }

            uvc_device->frame_buffer[i].pVirAddr = mmap(0, stBuffer.length,
                PROT_READ | PROT_WRITE, MAP_SHARED, uvc_device->uvc_dev, stBuffer.m.offset);
            uvc_device->frame_buffer[i].iBufLen  = stBuffer.length;

            ioctl(uvc_device->uvc_dev, VIDIOC_QBUF, &stBuffer);
        }

        uvc_device->frame_buffer_num = stReqBufs.count;
        alogd("request [%d] buffers success\n", uvc_device->frame_buffer_num);
    } else {
        for (int i = 0; i < uvc_device->frame_buffer_num; i++) {
            munmap(uvc_device->frame_buffer[i].pVirAddr, uvc_device->frame_buffer[i].iBufLen);
        }

        free(uvc_device->frame_buffer);
    }
reqbufs_err:
    return iRet;
}

static int DoUVCStreamOnOff(SampleUVCDevice *uvc_device, int iOn)
{
    int iRet;
    enum v4l2_buf_type stBufType;
    stBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    SampleUVCContext *pContext = (SampleUVCContext *)uvc_device->privite_data;

    if (iOn) {
        iRet = InitFrameList(3, pContext);
        if (iRet)
        {
            aloge("fatal error! init frame list fail!");
            goto _deinit_frame_list;
        }
        iRet = CreateCaptureThread(pContext);
        if (iRet)
        {
            aloge("fatal error! start capture fail!");
            goto _destroy;
        }
        iRet = StartCapture(pContext);
        if (iRet)
        {
            aloge("fatal error! start capture fail!");
            goto _stop_capture;
        }
        iRet = ioctl(uvc_device->uvc_dev, VIDIOC_STREAMON, &stBufType);
        if (iRet == 0) {
            alogd("begin to streaming\n");
            uvc_device->is_streaming_flag = 1;
        }
    } else {
        iRet = ioctl(uvc_device->uvc_dev, VIDIOC_STREAMOFF, &stBufType);
        if (iRet == 0) {
            alogd("stop to streaming\n");
            uvc_device->is_streaming_flag = 0;
        }
        StopCapture(pContext);
        DestroyCaptureThread(pContext);
        DeinitFrameList(pContext);
    }
    return iRet;
_stop_capture:
    StopCapture(pContext);
_destroy:
    DestroyCaptureThread(pContext);
_deinit_frame_list:
    DeinitFrameList(pContext);
    return iRet;
}

static int DoUVCEventProcess(SampleUVCDevice *uvc_device)
{
    int iRet;
    struct v4l2_event stEvent;
    struct uvc_event *pstUVCEvent = (struct uvc_event*)&stEvent.u.data[0];
    struct uvc_request_data stUVCReq;
    memset(&stUVCReq, 0, sizeof(struct uvc_request_data));
    stUVCReq.length = -EL2HLT;

    iRet = ioctl(uvc_device->uvc_dev, VIDIOC_DQEVENT, &stEvent);
    if (iRet < 0) {
        goto qevent_err;
    }

    alogv("event is %s\n",
        stEvent.type == UVC_EVENT_CONNECT?"first":\
        stEvent.type == UVC_EVENT_DISCONNECT?"disconnect":\
        stEvent.type == UVC_EVENT_STREAMON?"stream on":\
        stEvent.type == UVC_EVENT_STREAMOFF?"stream off":\
        stEvent.type == UVC_EVENT_SETUP?"setup":\
        stEvent.type == UVC_EVENT_DATA?"data":"NULL"
        );

    switch (stEvent.type) {
        case UVC_EVENT_CONNECT:
            /*alogd("uvc event first.\n");*/
            break;
        case UVC_EVENT_DISCONNECT:
            /*alogd("uvc event disconnect.\n");*/
            if (uvc_device->is_streaming_flag)
            {
                DoUVDReqReleaseBufs(uvc_device, 0);
                DoUVCStreamOnOff(uvc_device, 0);
            }
            goto qevent_err;
        case UVC_EVENT_STREAMON:
            /*alogd("uvc event stream on.\n");*/
            DoUVDReqReleaseBufs(uvc_device, 3);
            DoUVCStreamOnOff(uvc_device, 1);
            goto qevent_err;
        case UVC_EVENT_STREAMOFF:
            /*alogd("uvc event stream off.\n");*/
            if (uvc_device->is_streaming_flag)
            {
                DoUVDReqReleaseBufs(uvc_device, 0);
                DoUVCStreamOnOff(uvc_device, 0);
            }
            goto qevent_err;
        case UVC_EVENT_SETUP:
            /*alogd("uvc event setup.\n");*/
            DoUVCEventSetup(uvc_device, pstUVCEvent, &stUVCReq);
            break;
        case UVC_EVENT_DATA:
            DoUVCEventData(uvc_device, &pstUVCEvent->data);
            /*alogd("uvc event data.\n");*/
            break;

        default: break;
    }

    ioctl(uvc_device->uvc_dev, UVCIOC_SEND_RESPONSE, &stUVCReq);
qevent_err:
    return iRet;
}

static void usage(const char *argv0)
{
    printf(
        "\033[33m"
        "exec [-h|--help] [-p|--path]\n"
        "   <-h|--help>: print the help information\n"
        "   <-p|--path>       <args>: point to the configuration file path\n"
        "   <-x|--width>      <args>: set video picture width\n"
        "   <-y|--height>     <args>: set video picture height\n"
        "   <-f|--framerate>  <args>: set the video frame rate\n"
        "   <-b|--bulk>       <args>: Use bulk mode or not[0|1]\n"
        "   <-d|--device>     <args>: uvc video device number[0-3]\n"
        "   <-i|--image>      <args>: MJPEG image\n"
        "\033[0m\n");
}

static ERRORTYPE LoadSampleUVCConfig(SampleUVCConfig *pConfig, const char *conf_path)
{
    int iRet;
    char *pTmp = NULL;
    CONFPARSER_S stConfParser;
    iRet = createConfParser(conf_path, &stConfParser);
    if(iRet < 0)
    {
        iRet = FAILURE;
        alogd("user not set config file. use default test parameter!");
        goto use_default_conf;
    }

    // uvc
    pConfig->uvc_dev = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_UVC_DEVICE, 0);
    pConfig->isp_dev = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_ISP_DEVICE, MM_INVALID_CHN);
    pConfig->vipp_dev = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_VIPP_DEVICE, MM_INVALID_CHN);
    pConfig->encode_bitrate = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_ENCODER_BITRATE, 0);
    pConfig->enable_encode_online = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_ENABLE_ENCODE_ONLINE, 0);
    pConfig->enable_encpp = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_ENABLE_ENCPP, 0);

    // dual stream
    pConfig->enable_dual_stream = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_ENABLE_DUAL_STREAM, 0);
    pConfig->dual_stream_vipp_dev = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_DUAL_STREAM_VIPP_DEV, MM_INVALID_CHN);
    if (pConfig->enable_encode_online && pConfig->dual_stream_vipp_dev != 0)
    {
        iRet = FAILURE;
        alogw("must set dual stream vipp dev 0 when enable encode online!");
        goto use_default_conf;
    }
    pConfig->dual_stream_isp_dev = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_DUAL_STREAM_ISP_DEV, MM_INVALID_CHN);
    pConfig->dual_stream_width = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_DUAL_STREAM_WIDTH, 0);
    pConfig->dual_stream_height = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_DUAL_STREAM_HEIGHT, 0);
    pConfig->dual_stream_framerate = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_DUAL_STREAM_FRAMERATE, 0);
    pConfig->dual_stream_bitrate = GetConfParaInt(&stConfParser, SAMPLE_UVC_KEY_DUAL_STREAM_BITRATE, 0);
    pTmp = (char *)GetConfParaString(&stConfParser, SAMPLE_UVC_KEY_DUAL_STREAM_TYPE, NULL);
    if (!pTmp)
    {
        if (!strcmp(pTmp, "H.264"))
            pConfig->dual_stream_type = PT_H264;
        else if (!strcmp(pTmp, "H.265"))
            pConfig->dual_stream_type = PT_H265;
        else
        {
            alogw("unsupport data type[%s] use default H.264!", pTmp);
            pConfig->dual_stream_type = PT_H264;
        }
    }
    else
    {
        alogw("check conf file! MJPEG insert data type why no set! use default H.264!");
        pConfig->dual_stream_type = PT_H264;
    }
    alogd("enable dual stream %d vipp %d isp %d size %dx%d framerate %d bitrate %d type %d",
        pConfig->enable_dual_stream, pConfig->dual_stream_vipp_dev, pConfig->dual_stream_isp_dev,
        pConfig->dual_stream_width, pConfig->dual_stream_height, pConfig->dual_stream_framerate,
        pConfig->dual_stream_bitrate, pConfig->dual_stream_type);


    // awaiisp
    char *ptr = NULL;
    pConfig->mAiIspNpuRefBufReduceEnable = GetConfParaInt(&stConfParser, CFG_AiIspNpuRefBufReduceEnable, 0);
    pConfig->mAiIspSwitchReleaseResEnable = GetConfParaInt(&stConfParser, CFG_AiIspSwitchReleaseResEnable, 0);
    pConfig->mAiIspEnable = GetConfParaInt(&stConfParser, CFG_AiIspEnable, 0);
    ptr = (char*)GetConfParaString(&stConfParser, CFG_AiIspLutNbgFilePath, NULL);
    strncpy(pConfig->mAiIspLutNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
    ptr = (char*)GetConfParaString(&stConfParser, CFG_AiIspNbgFilePath, NULL);
    strncpy(pConfig->mAiIspNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
    pConfig->mAiIspModelVersion = GetConfParaInt(&stConfParser, CFG_AiIspModelVersion, 0);
    ptr = (char*)GetConfParaString(&stConfParser, CFG_AiIspCfgBinPath, NULL);
    strncpy(pConfig->mAiIspCfgBinPath, ptr, MAX_FILE_PATH_SIZE);
    pConfig->mAiIspWidth = GetConfParaInt(&stConfParser, CFG_AiIspWidth, 0);
    pConfig->mAiIspHeight = GetConfParaInt(&stConfParser, CFG_AiIspHeight, 0);
    pConfig->mAiIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_AiIspTdmRxBufNum, 0);
    pConfig->mAiIspAutoSwitchEnable = GetConfParaInt(&stConfParser, CFG_AiIspAutoSwitchEnable, 0);
    pConfig->mAiIspSwitchInterval = GetConfParaInt(&stConfParser, CFG_AiIspSwitchInterval, 0);
    int nAiIspSwitchIntervalDefault = 50;
    if (pConfig->mAiIspSwitchInterval > 0 && pConfig->mAiIspSwitchInterval < nAiIspSwitchIntervalDefault)
    {
        alogd("ai nr switch interval %d is too small, force to set default %d", pConfig->mAiIspSwitchInterval, nAiIspSwitchIntervalDefault);
        pConfig->mAiIspSwitchInterval = nAiIspSwitchIntervalDefault;
    }
    pConfig->mAiIspSwitchCase = GetConfParaInt(&stConfParser, CFG_AiIspSwitchCase, 0);
    pConfig->mAiIspSwitchDropFrameNum = GetConfParaInt(&stConfParser, CFG_AiIspSwitchDropFrameNum, 0);
    ptr = (char*)GetConfParaString(&stConfParser, CFG_AiIspCfgBinPath2, NULL);
    strncpy(pConfig->mAiIspCfgBinPath2, ptr, MAX_FILE_PATH_SIZE);
    pConfig->mAiIspReserve0 = GetConfParaInt(&stConfParser, CFG_AiIspReserve0, 0);
    pConfig->mAiIspReserve1 = GetConfParaInt(&stConfParser, CFG_AiIspReserve1, 0);
    pConfig->mAiIspReserve2 = GetConfParaInt(&stConfParser, CFG_AiIspReserve2, 0);


    //uac
    pConfig->uac_config.mSampleRate = GetConfParaInt(&stConfParser, SAMPLE_UAC_PCM_SAMPLE_RATE, 0);
    pConfig->uac_config.mBitWidth = GetConfParaInt(&stConfParser, SAMPLE_UAC_PCM_BIT_WIDTH, 0);
    pConfig->uac_config.mChannelCnt = GetConfParaInt(&stConfParser, SAMPLE_UAC_PCM_CHANNEL_CNT, 0);
    pConfig->uac_config.mFrameSize = GetConfParaInt(&stConfParser, SAMPLE_UAC_PCM_FRAME_SIZE, 0);
    pConfig->uac_config.mAecEn = GetConfParaInt(&stConfParser, SAMPLE_UAC_AEC_EN, 0);
    pConfig->uac_config.mAnsEn = GetConfParaInt(&stConfParser, SAMPLE_UAC_ANS_EN, 0);
    pConfig->uac_config.mAnsMode = GetConfParaInt(&stConfParser, SAMPLE_UAC_ANS_MODE, 0);
    pConfig->uac_config.mAgcEn = GetConfParaInt(&stConfParser, SAMPLE_UAC_AGC_EN, 0);
    pConfig->uac_config.agc_float_target_db = (float)GetConfParaDouble(&stConfParser, SAMPLE_UAC_AGC_FLOAT_TARGET_DB, 0);
    pConfig->uac_config.agc_float_max_gain_db = (float)GetConfParaDouble(&stConfParser, SAMPLE_UAC_AGC_FLOAT_MAX_GAIN_DB, 30);
    pConfig->uac_config.enable_uac1_in = GetConfParaInt(&stConfParser, SAMPLE_UAC_ENABLE_UAC_IN, 0);
    pConfig->uac_config.enable_uac1_out = GetConfParaInt(&stConfParser, SAMPLE_UAC_ENABLE_UAC_OUT, 0);
    alogd("mSampleRate[%d], mBitWidth[%d], mChannelCnt[%d], mFrameSize[%d]",
        pConfig->uac_config.mSampleRate, pConfig->uac_config.mBitWidth, pConfig->uac_config.mChannelCnt, pConfig->uac_config.mFrameSize);
    alogd("aec[%d] ans[%d] ans mode[%d] agc[%d] agc gain[%f-%f], enable_uac1_in[%d] enable_uac1_out[%d]", \
        pConfig->uac_config.mAecEn, pConfig->uac_config.mAnsEn, pConfig->uac_config.mAnsMode, pConfig->uac_config.mAgcEn,
        pConfig->uac_config.agc_float_target_db, pConfig->uac_config.agc_float_max_gain_db,
        pConfig->uac_config.enable_uac1_in, pConfig->uac_config.enable_uac1_out);

use_default_conf:
    alogd("UVCDev=%d,CapDev=%d,CapWidth=%d,CapHeight=%d,CapFrmRate=%d,EncBitRate=%d,\
        EncWidth=%d,EncHeight=%d,EncFrmRate=%d\n",
        pConfig->uvc_dev, pConfig->vipp_dev, pConfig->capture_width, pConfig->capture_height, pConfig->capture_framerate,
        pConfig->encode_bitrate, pConfig->encode_width, pConfig->encode_height, pConfig->encode_framerate);

    destroyConfParser(&stConfParser);
    return iRet;
}

static struct option pstLongOptions[] = {
   {"help",        no_argument,       0, 'h'},
   {"bulk",        required_argument, 0, 'b'},
   {"path",        required_argument, 0, 'p'},
   {"width",       required_argument, 0, 'x'},
   {"height",      required_argument, 0, 'y'},
   {"framerate",   required_argument, 0, 'f'},
   {"device",      required_argument, 0, 'd'},
   {0,             0,                 0,  0 }
};

static int ParseCmdLine(int argc, char **argv, SampleUVCContext *pCmdLinePara)
{
    int mRet;
    int iOptIndex = 0;

    memset(pCmdLinePara, 0, sizeof(SampleUVCContext));
    pCmdLinePara->mCmdLinePara.mConfigFilePath[0] = 0;
    while (1) {
        mRet = getopt_long(argc, argv, ":p:b:x:y:f:d:h", pstLongOptions, &iOptIndex);
        if (mRet == -1) {
            break;
        }

        switch (mRet) {
            /* let the "sampleXXX -path sampleXXX.conf" command to be compatible with
             * "sampleXXX -p sampleXXX.conf"
             */
            case 'p':
                if (strcmp("ath", optarg) == 0) {
                    if (NULL == argv[optind]) {
                        usage(argv[0]);
                        goto opt_need_arg;
                    }
                    alogd("path is [%s]\n", argv[optind]);
                    strncpy(pCmdLinePara->mCmdLinePara.mConfigFilePath, argv[optind], sizeof(pCmdLinePara->mCmdLinePara.mConfigFilePath));
                } else {
                    alogd("path is [%s]\n", optarg);
                    strncpy(pCmdLinePara->mCmdLinePara.mConfigFilePath, optarg, sizeof(pCmdLinePara->mCmdLinePara.mConfigFilePath));
                }
                break;
            case 'x':
                alogd("width is [%d]\n", atoi(optarg));
                pCmdLinePara->uvc_config.capture_width = atoi(optarg);
                break;
            case 'y':
                alogd("height is [%d]\n", atoi(optarg));
                pCmdLinePara->uvc_config.capture_height = atoi(optarg);
                break;
            case 'f':
                alogd("frame rate is [%d]\n", atoi(optarg));
                pCmdLinePara->uvc_config.capture_format = atoi(optarg);
                break;
            case 'b':
                alogd("bulk mode is [%d]\n", atoi(optarg));
                pCmdLinePara->uvc_config.uvc_bulk_mode = atoi(optarg);
                break;
            case 'd':
                alogd("device is [%d]\n", atoi(optarg));
                pCmdLinePara->uvc_config.uvc_dev = atoi(optarg);
                break;
            case 'h':
                usage(argv[0]);
                goto print_help_exit;
                break;
            case ':':
                aloge("option \"%s\" need <arg>\n", argv[optind - 1]);
                goto opt_need_arg;
                break;
            case '?':
                if (optind > 2) {
                    break;
                }
                aloge("unknow option \"%s\"\n", argv[optind - 1]);
                usage(argv[0]);
                goto unknow_option;
                break;
            default:
                printf("?? why getopt_long returned character code 0%o ??\n", mRet);
                break;
        }
    }

    return 0;
opt_need_arg:
unknow_option:
print_help_exit:
    return -1;
}

void SignalHandle(int iArg)
{
    alogv("receive exit signal. \n");
    pthread_mutex_lock(&g_sample_uvcout_context->mutex);
    pthread_cond_signal(&g_sample_uvcout_context->condition);
    pthread_mutex_unlock(&g_sample_uvcout_context->mutex);
    g_sample_uvcout_context->exit_flag = 1;
}

static void InitContext(SampleUVCContext *pContext)
{
    pContext->uvc_device.vipp_dev = MM_INVALID_DEV;
    pContext->uvc_device.ve_chn = MM_INVALID_CHN;
    pContext->uvc_device.ve_chn = MM_INVALID_CHN;
    pContext->uvc_device.isp_dev  = MM_INVALID_DEV;
    pContext->uvc_device.dual_stream_vipp_dev = MM_INVALID_DEV;
    pContext->uvc_device.dual_stream_vipp_chn = MM_INVALID_CHN;
    pContext->uvc_device.dual_stream_isp_dev = MM_INVALID_DEV;
    pContext->uvc_device.dual_stream_ve_chn = MM_INVALID_CHN;
    pContext->uvc_device.g2d_dev = -1;
}

static void check_usb_deivce_status(SampleUVCConfig *demo_config)
{
    int uvc_enable = 0, uvc_bulk_mode = 0, uac_enable = 0;
    if (demo_config->uvc_dev >= 0)
        uvc_enable = 1;
    if (demo_config->uvc_bulk_mode)
        uvc_bulk_mode = 1;
    if (demo_config->uac_config.enable_uac1_in || demo_config->uac_config.enable_uac1_out)
        uac_enable = 1;

    if (!uac_enable && uvc_enable && uvc_bulk_mode)
    {
        system("/usr/bin/setusbconfig none");
        alogv("/usr/bin/setusbconfig uvc bulk");
        system("/usr/bin/setusbconfig uvc bulk");
    }
    if (!uac_enable && uvc_enable && !uvc_bulk_mode)
    {
        system("/usr/bin/setusbconfig none");
        alogv("/usr/bin/setusbconfig uvc");
        system("/usr/bin/setusbconfig uvc");
    }
    if (uac_enable && uvc_enable)
    {
        system("/usr/bin/setusbconfig none");
        alogv("/usr/bin/setusbconfig uvc,uac1");
        system("/usr/bin/setusbconfig uvc,uac1");
    }
    if (uac_enable && !uvc_enable)
    {
        system("/usr/bin/setusbconfig none");
        alogv("/usr/bin/setusbconfig uac1");
        system("/usr/bin/setusbconfig uac1");
    }
}

static void *uvc_task_proc(void *thread_data)
{
    int ret;
    SampleUVCContext *context = (SampleUVCContext *)thread_data;
    SampleUVCDevice *uvc_dev = &context->uvc_device;
    uvc_dev->privite_data = (void *)context;

    ret = OpenUVCDevice(uvc_dev, &context->uvc_config);
    if (ret < 0) {
        aloge("open uvc video device failed!!\n");
        goto _exit;
    }
    uvc_dev->is_streaming_flag = 0;
    SubscribeUVCEvent(uvc_dev);
    fd_set stFdSet;
    FD_ZERO(&stFdSet);
    FD_SET(uvc_dev->uvc_dev, &stFdSet);
    while (1) {
        if (context->exit_flag)
            break;

        fd_set stErSet = stFdSet;
        fd_set stWrSet = stFdSet;
        struct timeval stTimeVal;
        stTimeVal.tv_sec = 0;
        stTimeVal.tv_usec = 10*1000;

        ret = select(uvc_dev->uvc_dev + 1, NULL, &stWrSet, &stErSet, &stTimeVal);
        if (FD_ISSET(uvc_dev->uvc_dev, &stErSet))
            DoUVCEventProcess(uvc_dev);
        if (FD_ISSET(uvc_dev->uvc_dev, &stWrSet) && uvc_dev->is_streaming_flag) {
            DoUVCVideoBufProcess(uvc_dev);
        }
    }
    if (uvc_dev->is_streaming_flag)
    {
        DoUVCStreamOnOff(uvc_dev, 0);
    }
    UnSubscribeUVCEvent(uvc_dev);
    CloseUVCDevice(uvc_dev);
_exit:
    return (void *)NULL;
}

int main(int argc, char *argv[])
{
    int result;

    SampleUVCContext stContext;
    memset(&stContext, 0, sizeof(SampleUVCContext));
    InitContext(&stContext);
    g_sample_uvcout_context = &stContext;

    result = ParseCmdLine(argc, argv, &stContext);
    if (result < 0) {
        aloge("parse cmdline error.");
        return -1;
    }
    stContext.uvc_device.enable_bulk_mode = stContext.uvc_config.uvc_bulk_mode;

    /* parse config file. */
    if(LoadSampleUVCConfig(&stContext.uvc_config , stContext.mCmdLinePara.mConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    check_usb_deivce_status(&stContext.uvc_config);

    stContext.configfs_para = init_prarser_uvc_configfs();
    if (!stContext.configfs_para)
        goto _exit;
    for (int i = 0; i < stContext.configfs_para->format_num; i++)
    {
        for (int j = 0; j < stContext.configfs_para->format[i].frame_num; j++)
        {
            alogd("support foramt index %d type %d frame index %d video %dx%d", i,
                stContext.configfs_para->format[i].format,
                stContext.configfs_para->format[i].frame[j].bFrameIndex,
                stContext.configfs_para->format[i].frame[j].wWidth,
                stContext.configfs_para->format[i].frame[j].wHeight);
        }
    }

    pthread_condattr_t cond_attr;
    pthread_condattr_init(&cond_attr);
    pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
    pthread_cond_init(&stContext.condition, &cond_attr);
    pthread_mutex_init(&stContext.mutex, NULL);
    signal(SIGINT, SignalHandle);

    MPP_SYS_CONF_S mSysConf;
    mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&mSysConf);
    result = AW_MPI_SYS_Init();
    if (result) {
        aloge("AW_MPI_SYS_Init failed!");
        goto _exit;
    }

    if (stContext.uvc_config.uac_config.enable_uac1_in || stContext.uvc_config.uac_config.enable_uac1_out)
        uac_enable(&stContext.uvc_config.uac_config);
    if ((stContext.uvc_config.vipp_dev >= 0) && (stContext.uvc_config.uvc_dev >= 0))
        pthread_create(&stContext.uvc_task_trd, NULL, uvc_task_proc, (void *)&stContext);

    pthread_mutex_lock(&stContext.mutex);
    pthread_cond_wait(&stContext.condition, &stContext.mutex);
    pthread_mutex_unlock(&stContext.mutex);

    if ((stContext.uvc_config.vipp_dev >= 0) && (stContext.uvc_config.uvc_dev >= 0))
        pthread_join(stContext.uvc_task_trd, NULL);
    if (stContext.uvc_config.uac_config.enable_uac1_in || stContext.uvc_config.uac_config.enable_uac1_out)
        uac_disable();

    pthread_cond_destroy(&stContext.condition);
    pthread_mutex_destroy(&stContext.mutex);

    if (!stContext.configfs_para)
        destroy_parser_uvc_configfs(stContext.configfs_para);

    AW_MPI_SYS_Exit();
_exit:
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
