#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>

#include <plat_errno.h>
#include <utils/plat_log.h>

#include "uvc.h"
#include "video.h"
#include "sample_FaceTrack.h"
#include "parser_uvc_configfs.h"

extern int gStop;
UvcOutPrivateData *gpUvcOutData;

static int OpenUVCDevice(int device, int *devID)
{
    struct v4l2_capability stCap;
    int iRet;
    int iFd;
    char pcDevName[256];

    sprintf(pcDevName, "/dev/video%d", device);
    alogd("open uvc device[%s]", pcDevName);
    iFd = open(pcDevName, O_RDWR | O_NONBLOCK);
    if (iFd < 0) {
        aloge("open video device failed: device[%s]\n", pcDevName);
        goto open_err;
    }
    iRet = ioctl(iFd, VIDIOC_QUERYCAP, &stCap);
    if (iRet < 0) {
        aloge("unable to query device\n");
        goto query_cap_err;
    }
    alogd("device is %s on bus %s\n", stCap.card, stCap.bus_info);

    *devID = iFd;

    return 0;

query_cap_err:
    close(iFd);
open_err:
    return -1;
}

static void CloseUVCDevice(int device)
{
    close(device);
}

static void UVCFillStreamingControl(SampleUVCDevice *pstUVCDev, struct uvc_streaming_control *pstCtrl, int iFmtIndex, int iFrmIndex)
{
    UvcOutPrivateData *pData = gpUvcOutData;
   // SampleUVCContext *context = (SampleUVCContext *)pstUVCDev->privite_data;
    struct uvc_frame *frame = &pData->configfs_para->format[iFmtIndex].frame[iFrmIndex];

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

static void SubscribeUVCEvent(SampleUVCDevice *pDevice)
{
    struct v4l2_event_subscription sub;

    UVCFillStreamingControl(pDevice, &pDevice->uvc_streaming_probe, 0, 0);
    UVCFillStreamingControl(pDevice, &pDevice->uvc_streaming_commit, 0, 0);
//    UVCFillStreamingControl(pDevice, &pDevice->uvc_streaming_probe, 2, 2);
//    UVCFillStreamingControl(pDevice, &pDevice->uvc_streaming_commit, 2, 2);

    /* subscribe events, for debug, subscribe all events */
    memset(&sub, 0, sizeof(sub));
    sub.type = UVC_EVENT_FIRST;
    ioctl(pDevice->devId, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_CONNECT;
    ioctl(pDevice->devId, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_DISCONNECT;
    ioctl(pDevice->devId, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMON;
    ioctl(pDevice->devId, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_STREAMOFF;
    ioctl(pDevice->devId, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_SETUP;
    ioctl(pDevice->devId, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_DATA;
    ioctl(pDevice->devId, VIDIOC_SUBSCRIBE_EVENT, &sub);
    sub.type = UVC_EVENT_LAST;
    ioctl(pDevice->devId, VIDIOC_SUBSCRIBE_EVENT, &sub);
}

static void UnSubscribeUVCEvent(SampleUVCDevice *pDevice)
{
    struct v4l2_event_subscription sub;

    /*uvc_fill_streaming_control(pstUVCDev, &pstUVCDev->probe, 0, 0);
    uvc_fill_streaming_control(pstUVCDev, &pstUVCDev->commit, 0, 0);*/

    /* subscribe events, for debug, subscribe all events */
    memset(&sub, 0, sizeof(sub));
    sub.type = UVC_EVENT_FIRST;
    ioctl(pDevice->devId, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_CONNECT;
    ioctl(pDevice->devId, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_DISCONNECT;
    ioctl(pDevice->devId, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_STREAMON;
    ioctl(pDevice->devId, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_STREAMOFF;
    ioctl(pDevice->devId, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_SETUP;
    ioctl(pDevice->devId, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_DATA;
    ioctl(pDevice->devId, VIDIOC_UNSUBSCRIBE_EVENT, sub);
    sub.type = UVC_EVENT_LAST;
    ioctl(pDevice->devId, VIDIOC_UNSUBSCRIBE_EVENT, sub);
}

static int DoUVDReqReleaseBufs(SampleUVCDevice *pDevice, int iBufsNum)
{
    int iRet = 0;

    if (iBufsNum > 0) {
        struct v4l2_requestbuffers stReqBufs;
        memset(&stReqBufs, 0, sizeof(struct v4l2_requestbuffers));
        stReqBufs.count  = iBufsNum;
        stReqBufs.memory = V4L2_MEMORY_MMAP;
        stReqBufs.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        iRet = ioctl(pDevice->devId, VIDIOC_REQBUFS, &stReqBufs);
        if (iRet < 0) {
            aloge("VIDIOC_REQBUFS failed!!\n");
            goto reqbufs_err;
        }

        pDevice->frame_buffer = malloc(iBufsNum * sizeof(SampleUVCFrame));

        for (int i = 0; i < iBufsNum; i++) {
            struct v4l2_buffer stBuffer;
            memset(&stBuffer, 0, sizeof(struct v4l2_buffer));
            stBuffer.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            stBuffer.memory = V4L2_MEMORY_MMAP;
            stBuffer.index  = i;
            iRet = ioctl(pDevice->devId, VIDIOC_QUERYBUF, &stBuffer);
            if (iRet < 0) {
                aloge("VIDIOC_QUERYBUF failed!!\n");
            }

            pDevice->frame_buffer[i].pVirAddr = mmap(0, stBuffer.length,
                PROT_READ | PROT_WRITE, MAP_SHARED, pDevice->devId, stBuffer.m.offset);
           pDevice->frame_buffer[i].iBufLen  = stBuffer.length;

            ioctl(pDevice->devId, VIDIOC_QBUF, &stBuffer);
        }

        pDevice->frame_buffer_num = stReqBufs.count;
        alogd("request [%d] buffers success\n", pDevice->frame_buffer_num);
    } else {
        for (int i = 0; i < pDevice->frame_buffer_num; i++) {
            munmap(pDevice->frame_buffer[i].pVirAddr, pDevice->frame_buffer[i].iBufLen);
        }

        free(pDevice->frame_buffer);
    }
reqbufs_err:
    return iRet;
}

static int InitFrameList(int iFrmNum, SampleUVCDevice *pDevice)
{
    unsigned int iFrameSize = 0;
    if (V4L2_PIX_FMT_YUYV == pDevice->format)
    {
        iFrameSize = pDevice->frame->wWidth * pDevice->frame->wHeight * 2;
    }
    else if (V4L2_PIX_FMT_H264 == pDevice->format)
    {
        iFrameSize = pDevice->frame->wWidth * pDevice->frame->wHeight / 10;
        alogd("H264 iFrameSize=%d", iFrameSize);
    }
    else
    {
        iFrameSize = pDevice->frame->wWidth * pDevice->frame->wHeight * 3 / 2 / 3;
    }

    alogd("begin to alloc frame list.\n");
    if (iFrmNum <= 0) {
        aloge("frame list number must bigger than 0!!\n");
        return -1;
    }

    INIT_LIST_HEAD(&pDevice->frame_idle_list);
    INIT_LIST_HEAD(&pDevice->frame_valid_list);
    INIT_LIST_HEAD(&pDevice->frame_used_list);

    SampleUVCOutBuf *pBufTmp;
    pthread_mutex_init(&pDevice->frame_list_lock, NULL);
    for (int i = 0; i < iFrmNum; i++) {
        pBufTmp = malloc(sizeof(SampleUVCOutBuf));
        pBufTmp->pcData = malloc(iFrameSize);
        pBufTmp->iDataBufSize = iFrameSize;
        pBufTmp->iDataSize0 = 0;
        pBufTmp->iDataSize1 = 0;
        pBufTmp->iDataSize2 = 0;
        list_add_tail(&pBufTmp->mList, &pDevice->frame_idle_list);
    }
    return 0;
}

static void DeinitFrameList(SampleUVCDevice *pDevice)
{
    alogd("begin to free frame list.\n");
    SampleUVCOutBuf *pBufTmp;
    SampleUVCOutBuf *pBufTmpNext;
    list_for_each_entry_safe(pBufTmp, pBufTmpNext, &pDevice->frame_used_list, mList)
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
    list_for_each_entry_safe(pBufTmp, pBufTmpNext, &pDevice->frame_idle_list, mList)
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
    list_for_each_entry_safe(pBufTmp, pBufTmpNext, &pDevice->frame_idle_list, mList)
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
    pthread_mutex_destroy(&pDevice->frame_list_lock);
}

static int DoUVCStreamOnOff(SampleUVCDevice *pDevice, int iOn)
{
    int iRet;
    enum v4l2_buf_type stBufType;
    stBufType = V4L2_BUF_TYPE_VIDEO_OUTPUT;

    if (iOn) {
        iRet = InitFrameList(3, pDevice);
        if (iRet)
        {
            aloge("fatal error! init frame list fail!");
            goto _deinit_frame_list;
        }
        iRet = ioctl(pDevice->devId, VIDIOC_STREAMON, &stBufType);
        if (iRet == 0) {
            alogd("begin to streaming\n");
            pDevice->is_streaming_flag = 1;
        }
    } else {
        iRet = ioctl(pDevice->devId, VIDIOC_STREAMOFF, &stBufType);
        if (iRet == 0) {
            alogd("stop to streaming\n");
            pDevice->is_streaming_flag = 0;
        }
        DeinitFrameList(pDevice);
    }
    return iRet;

_deinit_frame_list:
    DeinitFrameList(pDevice);
    return iRet;
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

static int UVCVideoSetFormat(SampleUVCDevice *uvc_device)
{
    int iRet;
    struct v4l2_format stFormat;
    iRet = ioctl(uvc_device->devId, VIDIOC_G_FMT, &stFormat);
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
    iRet = ioctl(uvc_device->devId, VIDIOC_S_FMT, &stFormat);
    if (iRet < 0) {
        aloge("VIDIOC_S_FMT failed!! %s (%d).\n", strerror(errno), errno);
    }

    iRet = ioctl(uvc_device->devId, VIDIOC_G_FMT, &stFormat);
    alogv("width=[%d],height=[%d],sizeimage=[%d], format[%d]\n",
        stFormat.fmt.pix.width, stFormat.fmt.pix.height, stFormat.fmt.pix.sizeimage, stFormat.fmt.pix.pixelformat);

    return iRet;
}


static int DoUVCEventData(SampleUVCDevice *uvc_device, struct uvc_request_data *pstReq)
{
    struct uvc_streaming_control *pstTarget;
    UvcOutPrivateData *pData = gpUvcOutData;

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
        if (pstCtrl->bFormatIndex >= (pData->configfs_para->format_num+1) || \
            pstCtrl->bFrameIndex >= (pData->configfs_para->format[pstCtrl->bFormatIndex - 1].frame_num+1))
        {
            alogw("unsupport format index[%d] frame index[%d], use default format index[1] frame index[1]", \
                pstCtrl->bFormatIndex, pstCtrl->bFrameIndex);
            pstCtrl->bFormatIndex = 1;
            pstCtrl->bFrameIndex = 1;
        }

        uvc_device->format = pData->configfs_para->format[pstCtrl->bFormatIndex - 1].format;
        uvc_device->frame =
            find_uvc_frame(pData->configfs_para, pstCtrl->bFormatIndex, pstCtrl->bFrameIndex);
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

static int DoUVCEventProcess(SampleUVCDevice *pDevice)
{
    int iRet;
    struct v4l2_event stEvent;
    struct uvc_event *pstUVCEvent = (struct uvc_event*)&stEvent.u.data[0];
    struct uvc_request_data stUVCReq;
    memset(&stUVCReq, 0, sizeof(struct uvc_request_data));
    stUVCReq.length = -EL2HLT;

    iRet = ioctl(pDevice->devId, VIDIOC_DQEVENT, &stEvent);
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
            if (pDevice->is_streaming_flag)
            {
                DoUVDReqReleaseBufs(pDevice, 0);
                DoUVCStreamOnOff(pDevice, 0);
            }
            goto qevent_err;
        case UVC_EVENT_STREAMON:
            /*alogd("uvc event stream on.\n");*/
            DoUVDReqReleaseBufs(pDevice, 3);
            DoUVCStreamOnOff(pDevice, 1);
            goto qevent_err;
        case UVC_EVENT_STREAMOFF:
            /*alogd("uvc event stream off.\n");*/
            if (pDevice->is_streaming_flag)
            {
                DoUVDReqReleaseBufs(pDevice, 0);
                DoUVCStreamOnOff(pDevice, 0);
            }
            goto qevent_err;
        case UVC_EVENT_SETUP:
            /*alogd("uvc event setup.\n");*/
            DoUVCEventSetup(pDevice, pstUVCEvent, &stUVCReq);
            break;
        case UVC_EVENT_DATA:
            DoUVCEventData(pDevice, &pstUVCEvent->data);
            /*alogd("uvc event data.\n");*/
            break;

        default: break;
    }

    ioctl(pDevice->devId, UVCIOC_SEND_RESPONSE, &stUVCReq);
qevent_err:
    return iRet;
}

static void check_usb_deivce_status(SampleUVCConfig *demo_config)
{
    int uvc_enable = 0, uvc_bulk_mode = 0, uac_enable = 0;

    uvc_enable = demo_config->uvc_enable;
    uvc_bulk_mode = demo_config->uvc_bulk_mode;
    uac_enable =demo_config->uac_enable;

    if (!uac_enable && uvc_enable && uvc_bulk_mode)
    {
        system("/usr/bin/setusbconfig none");
        alogv("/usr/bin/setusbconfig uvc bulk");
        system("/usr/bin/setusbconfig uvc bulk");
    }
    if (!uac_enable && uvc_enable && !uvc_bulk_mode) //ok
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

int uvcGetVenStream(VENC_STREAM_S *pStream, VencHeaderData *pHeader)
{
    int ret = 0;
    SampleUVCDevice *pDevice = (SampleUVCDevice *)&(gpUvcOutData->uvcDevice);

    if (!pDevice->is_streaming_flag)
        return 0;

    pthread_mutex_lock(&pDevice->frame_list_lock);
    SampleUVCOutBuf *pBuf = list_first_entry_or_null(&pDevice->frame_idle_list, SampleUVCOutBuf, mList);
    if (pBuf != NULL) {
        ret = ConfigUVCBufByVE(pBuf, pStream, pHeader);
        list_move_tail(&pBuf->mList, &pDevice->frame_valid_list);
    }
    pthread_mutex_unlock(&pDevice->frame_list_lock);
    return ret;
}

#define DEBUG_SAVE_SEND_UVC_STREAM_FILE      "./save_stream.raw"
FILE *gfp = NULL;

static inline int DoUVCVideoBufProcess(SampleUVCDevice *uvc_device)
{
    UvcOutPrivateData *pData = gpUvcOutData;
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
    iRet = ioctl(uvc_device->devId, VIDIOC_DQBUF, &stBuf);
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
#if 0
    if (gfp == NULL)
        gfp = fopen(DEBUG_SAVE_SEND_UVC_STREAM_FILE, "ab+");
    if (gfp)
    {
        fwrite(uvc_device->frame_buffer[stBuf.index].pVirAddr, stBuf.bytesused, 1, gfp);
       // fclose(fp);
    }
#endif
    iRet = ioctl(uvc_device->devId, VIDIOC_QBUF, &stBuf);
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

void *sampleFaceTrackUvcOutTask(void *para)
{
    int ret;
    int uvcDevId;

    UvcOutPrivateData *pData = (UvcOutPrivateData *)para;
    UvcOutParaConfig *pUvcConfig = (UvcOutParaConfig *)pData->config;
    SampleUVCDevice *pDevice = (SampleUVCDevice *)&(pData->uvcDevice);
    gpUvcOutData = pData;
    SampleUVCConfig *pconfig = &(pUvcConfig->config);

    check_usb_deivce_status(pconfig);
    pDevice->enable_bulk_mode = pconfig->uvc_bulk_mode;
    pData->configfs_para = init_prarser_uvc_configfs();

    for (int i = 0; i < pData->configfs_para->format_num; i++)
    {
        for (int j = 0; j < pData->configfs_para->format[i].frame_num; j++)
        {
            alogd("support foramt index %d type %d frame index %d video %dx%d", i,
                    pData->configfs_para->format[i].format,
                    pData->configfs_para->format[i].frame[j].bFrameIndex,
                    pData->configfs_para->format[i].frame[j].wWidth,
                    pData->configfs_para->format[i].frame[j].wHeight);
        }
    }

    ret = OpenUVCDevice(pUvcConfig->uvcDev, &uvcDevId);
    if (ret < 0) {
        aloge("open uvc video device failed!!\n");
        goto _exit;
    }
    pDevice->devId = uvcDevId;

    pDevice->is_streaming_flag = 0;

    SubscribeUVCEvent(pDevice);
    fd_set stFdSet;
    FD_ZERO(&stFdSet);
    FD_SET(uvcDevId, &stFdSet);
    while (!gStop) {

        fd_set stErSet = stFdSet;
        fd_set stWrSet = stFdSet;
        struct timeval stTimeVal;
        stTimeVal.tv_sec = 0;
        stTimeVal.tv_usec = 10*1000;

        ret = select(uvcDevId + 1, NULL, &stWrSet, &stErSet, &stTimeVal);
        if (FD_ISSET(uvcDevId, &stErSet)) {
            DoUVCEventProcess(pDevice);
        }
        if (FD_ISSET(uvcDevId, &stWrSet) && pDevice->is_streaming_flag) {
            DoUVCVideoBufProcess(pDevice);
        }
    }
    if (pDevice->is_streaming_flag)
    {
        DoUVCStreamOnOff(pDevice, 0);
    }
    UnSubscribeUVCEvent(pDevice);
    CloseUVCDevice(uvcDevId);

_exit:
    destroy_parser_uvc_configfs(pData->configfs_para);
    return (void *)NULL;

}
