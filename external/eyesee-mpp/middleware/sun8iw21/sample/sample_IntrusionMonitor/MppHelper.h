#ifndef __MPP_HELPER_H__
#define __MPP_HELPER_H__

#include <stdio.h>
#include <plat_type.h>
#include <mm_comm_region.h>
#include "rgb_ctrl.h"

// TODO we will remove type of venc
#include <mpi_venc.h>
// end TODO


#define _CHECK_RET( ret )  do {\
    if( SUCCESS != ret ) {\
        printf("Error: %s: %s at %d\n", __FILE__, __FUNCTION__, __LINE__);\
        return ret;\
    }\
} while(0)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

ERRORTYPE MPP_INIT();
ERRORTYPE MPP_EXIT();
ERRORTYPE MPP_SYS_BIND(MPP_CHN_S* srcChn, MPP_CHN_S* dstChn);
ERRORTYPE MPP_SYS_UNBIND(MPP_CHN_S* srcChn, MPP_CHN_S* dstChn);
ERRORTYPE MPP_HELPER_CREATE_VIPP(MPP_CHN_S viChn, int width, int height, int fps, PIXEL_FORMAT_E format);
ERRORTYPE MPP_HELPER_DESTROY_VIPP(MPP_CHN_S viChn);
ERRORTYPE MPP_HELPER_ENABLE_VIPP(MPP_CHN_S viChn);
ERRORTYPE MPP_HELPER_DISABLE_VIPP(MPP_CHN_S viChn);
ERRORTYPE MPP_HELPER_DESTROY_VO(MPP_CHN_S voChn);
ERRORTYPE MPP_HELPER_CREATE_VENC(MPP_CHN_S veChn, int srcWidth, int srcHeight, int srcFps, int distWidth, int distHeight, int distFps, PAYLOAD_TYPE_E payloadType, int bitRate, PIXEL_FORMAT_E format);
ERRORTYPE MPP_VENC_INIT(MPP_CHN_S *veChn, int srcWidth, int srcHeight, int srcFps, int distWidth, int distHeight, int distFps, PAYLOAD_TYPE_E payloadType, int bitRate, PIXEL_FORMAT_E format);
ERRORTYPE MPP_VENC_UNINIT(MPP_CHN_S *veChn);
ERRORTYPE MPP_HELPER_DESTROY_VENC(MPP_CHN_S veChn);
ERRORTYPE MPP_VO_INIT(MPP_CHN_S *voChn, int x, int y, int width, int height);
ERRORTYPE MPP_VI_INIT(MPP_CHN_S *viChn, int width, int height, int fps, PIXEL_FORMAT_E format);
ERRORTYPE MPP_VI_INIT2(MPP_CHN_S *viChn, int width, int height, int fps, PIXEL_FORMAT_E format);
ERRORTYPE MPP_VO_START(MPP_CHN_S *voChn);
ERRORTYPE MPP_VI_START(MPP_CHN_S *viChn);
ERRORTYPE MPP_VO_UNINIT(MPP_CHN_S *voChn);
ERRORTYPE MPP_VI_UNINIT(MPP_CHN_S *viChn);
ERRORTYPE MPP_VI_STOP(MPP_CHN_S *viChn);
ERRORTYPE MPP_VO_STOP(MPP_CHN_S *voChn);
ERRORTYPE MPP_VI_GET_FRAME(MPP_CHN_S *viChn, VIDEO_FRAME_INFO_S *frameInfo);
ERRORTYPE MPP_VO_SEND_FRAME(MPP_CHN_S *voChn, VIDEO_FRAME_INFO_S *frameInfo);
ERRORTYPE MPP_VEN_SEND_FRAME(MPP_CHN_S *veChn, VIDEO_FRAME_INFO_S *frameInfo);
ERRORTYPE MPP_VI_RELEASE_FRAME(MPP_CHN_S *viChn, VIDEO_FRAME_INFO_S *frameInfo);
ERRORTYPE MPP_VENC_START(MPP_CHN_S *veChn);
ERRORTYPE MPP_VENC_SET_MOTION_PARAM(MPP_CHN_S *veChn, VencMotionSearchParam *pMotionParam);
ERRORTYPE MPP_VENC_GET_MOTION_PARAM(MPP_CHN_S *veChn, VencMotionSearchParam *pMotionParam);
ERRORTYPE MPP_VENC_STOP(MPP_CHN_S *veChn);
ERRORTYPE MPP_HELPER_STOP_RECORD();
ERRORTYPE MPP_VENC_GET_STREAM(MPP_CHN_S *veChn, VENC_STREAM_S *pStream);
ERRORTYPE MPP_VENC_GET_MOTION_RESULT(MPP_CHN_S *veChn, VencMotionSearchResult *pMotionResult);
ERRORTYPE MPP_VENC_RELEASE_STREAM(MPP_CHN_S *veChn, VENC_STREAM_S *pStream);
ERRORTYPE MPP_HELPER_START_NPU(int width, int height, int fps);
ERRORTYPE MPP_HELPER_STOP_NPU();
int MPP_REGION_DESTORY_BITMAP(MPP_CHN_S *pstChn, RGN_HANDLE Handle);
int MPP_REGION_CREATE_BITMAP(MPP_CHN_S *pstChn, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, RGN_HANDLE Handle, int x, int y, RGB_PIC_S *rgb);
int MPP_REGION_DESTORY_RECT(MPP_CHN_S *pstChn, RGN_HANDLE Handle);
int MPP_REGION_CREATE_RECT(MPP_CHN_S *pstChn, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, RGN_HANDLE Handle, int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __MPP_HELPER_H__ */
