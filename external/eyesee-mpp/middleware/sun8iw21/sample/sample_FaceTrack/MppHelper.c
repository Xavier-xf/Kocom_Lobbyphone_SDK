#include <string.h>

#include <VIDEO_FRAME_INFO_S.h>
#include <mpi_sys.h>
#include <mpi_isp.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_vi.h>
#include <mpi_vo.h>
#include <utils/plat_log.h>
#include "rgb_ctrl.h"

#include "MppHelper.h"

#define MPP_HELPER_ISP_DEV 0
#define MPP_HELPER_VO_DEV 0

ERRORTYPE MPP_INIT()
{
    MPP_SYS_CONF_S stSysConf;

    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);

    return AW_MPI_SYS_Init();
}

ERRORTYPE MPP_EXIT()
{
    return AW_MPI_SYS_Exit();
}

ERRORTYPE MPP_SYS_BIND(MPP_CHN_S* srcChn, MPP_CHN_S* dstChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_SYS_Bind(srcChn, dstChn);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_SYS_UNBIND(MPP_CHN_S* srcChn, MPP_CHN_S* dstChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_SYS_UnBind(srcChn, dstChn);
    _CHECK_RET(ret);

    return ret;
}

static ERRORTYPE voCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    if (MOD_ID_VOU == pChn->mModId) {
    } else {
        ret = FAILURE;
    }

    return ret;
}

ERRORTYPE MPP_HELPER_DESTROY_VO(MPP_CHN_S voChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VO_DestroyChn(voChn.mDevId, voChn.mChnId);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_DisableVideoLayer(voChn.mDevId);
    _CHECK_RET(ret);

    sleep(1);

    return ret;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    //    SAMPLE_VENC_PARA_S *pVencPara = (SAMPLE_VENC_PARA_S *)cookie;
    VIDEO_FRAME_INFO_S *pFrame = (VIDEO_FRAME_INFO_S *)pEventData;

#if 0
    switch (event)
    {
        case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            if (pFrame != NULL)
            {
                pthread_mutex_lock(&pVencPara->mInBuf_Q.mUseListLock);
                if (!list_empty(&pVencPara->mInBuf_Q.mUseList))
                {
                    IN_FRAME_NODE_S *pEntry, *pTmp;
                    list_for_each_entry_safe(pEntry, pTmp, &pVencPara->mInBuf_Q.mUseList, mList)
                    {
                        if (pEntry->mFrame.mId == pFrame->mId)
                        {
                            pthread_mutex_lock(&pVencPara->mInBuf_Q.mIdleListLock);
                            list_move_tail(&pEntry->mList, &pVencPara->mInBuf_Q.mIdleList);
                            pthread_mutex_unlock(&pVencPara->mInBuf_Q.mIdleListLock);
                            break;
                        }
                    }
                }
                pthread_mutex_unlock(&pVencPara->mInBuf_Q.mUseListLock);
            }
            break;

        default:
            break;
    }
#endif

    return SUCCESS;
}

ERRORTYPE MPP_VENC_INIT(MPP_CHN_S *veChn, int srcWidth, int srcHeight, int srcFps, int distWidth, int distHeight, int distFps, PAYLOAD_TYPE_E payloadType, int bitRate, PIXEL_FORMAT_E format)
{
    ERRORTYPE ret;
    VENC_CHN_ATTR_S mVEncChnAttr;
    VENC_RC_PARAM_S mVEncRcParam;
    VENC_FRAME_RATE_S mVencFrameRateConfig;

    /* venc chn attr */
    memset(&mVEncChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    //mVEncChnAttr.VeAttr.mOnlineEnable = 0;
    //mVEncChnAttr.VeAttr.mOnlineShareBufNum = 2;
    mVEncChnAttr.VeAttr.Type = payloadType; // PT_H264, PT_H265, PT_MJPEG
    mVEncChnAttr.VeAttr.MaxKeyInterval = distFps;
    mVEncChnAttr.VeAttr.SrcPicWidth = srcWidth;
    mVEncChnAttr.VeAttr.SrcPicHeight = srcHeight;
    mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    mVEncChnAttr.VeAttr.PixelFormat = format; // NV12
    mVEncChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_JPEG;
    mVEncChnAttr.RcAttr.mProductMode = PRODUCT_STATIC_IPC;
    //mVEncRcParam.sensor_type = VENC_ST_EN_WDR;
    if (PT_H264 == mVEncChnAttr.VeAttr.Type) {
        mVEncChnAttr.VeAttr.AttrH264e.Profile = 1;
        mVEncChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        mVEncChnAttr.VeAttr.AttrH264e.PicWidth = distWidth;
        mVEncChnAttr.VeAttr.AttrH264e.PicHeight = distHeight;
        mVEncChnAttr.VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
        mVEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
        mVEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = bitRate;
        mVEncChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = srcFps;
        mVEncChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = distFps;
        mVEncRcParam.ParamH264Vbr.mMaxQp = 45;
        mVEncRcParam.ParamH264Vbr.mMinQp = 1;
        mVEncRcParam.ParamH264Vbr.mMaxPqp = 45;
        mVEncRcParam.ParamH264Vbr.mMinPqp = 1;
        mVEncRcParam.ParamH264Vbr.mQpInit = 30;
        mVEncRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
        mVEncRcParam.ParamH264Vbr.mMovingTh = 20;
        mVEncRcParam.ParamH264Vbr.mQuality = 5;
        mVEncRcParam.ParamH264Vbr.mIFrmBitsCoef = 15;
        mVEncRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    } else if(PT_H265 == mVEncChnAttr.VeAttr.Type) {
        mVEncChnAttr.VeAttr.AttrH265e.mProfile = 0;
        mVEncChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        mVEncChnAttr.VeAttr.AttrH265e.mPicWidth = distWidth;
        mVEncChnAttr.VeAttr.AttrH265e.mPicHeight = distHeight;
        mVEncChnAttr.VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
        mVEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        mVEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = bitRate;
        mVEncChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = srcFps;
        mVEncChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = distFps;
        mVEncRcParam.ParamH265Vbr.mMaxQp = 51;
        mVEncRcParam.ParamH265Vbr.mMinQp = 1;
        mVEncRcParam.ParamH265Vbr.mMaxPqp = 51;
        mVEncRcParam.ParamH265Vbr.mMinPqp = 1;
        mVEncRcParam.ParamH265Vbr.mQpInit = 30;
        mVEncRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        mVEncRcParam.ParamH265Vbr.mMovingTh = 20;
        mVEncRcParam.ParamH265Vbr.mQuality = 5;
        mVEncRcParam.ParamH265Vbr.mIFrmBitsCoef = 15;
        mVEncRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    } else if(PT_MJPEG == mVEncChnAttr.VeAttr.Type) {
        mVEncChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        mVEncChnAttr.VeAttr.AttrMjpeg.mPicWidth= distWidth;
        mVEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = distHeight;
        mVEncChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = bitRate;
        mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = srcFps;
        mVEncChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = distFps;
    }
    mVencFrameRateConfig.SrcFrmRate = srcFps;
    mVencFrameRateConfig.DstFrmRate = distFps;

    ret = AW_MPI_VENC_CreateChn(veChn->mChnId, &mVEncChnAttr);
    _CHECK_RET(ret);

    ret = AW_MPI_VENC_SetRcParam(veChn->mChnId, &mVEncRcParam);
    _CHECK_RET(ret);

    /*ret = AW_MPI_VENC_SetFrameRate(veChn.mChnId, &mVencFrameRateConfig);
    _CHECK_RET(ret);*/

    // 3D降噪
    s3DfilterParam m3DnrPara;
    memset(&m3DnrPara, 0, sizeof(s3DfilterParam));
    m3DnrPara.enable_3d_filter = 1;
    m3DnrPara.adjust_pix_level_enable = 0;
    m3DnrPara.smooth_filter_enable = 1;
    m3DnrPara.max_pix_diff_th = 6;
    m3DnrPara.max_mv_th = 2;
    m3DnrPara.max_mad_th = 11;
    m3DnrPara.min_coef = 14;
    m3DnrPara.max_coef = 16;
    ret = AW_MPI_VENC_Set3DFilter(veChn->mChnId, &m3DnrPara);
    _CHECK_RET(ret);

    MPPCallbackInfo cbInfo;
 //   cbInfo.cookie = (void*)pVencPara;
    cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
    AW_MPI_VENC_RegisterCallback(veChn->mChnId, &cbInfo);

    return ret;
}

ERRORTYPE MPP_VENC_UNINIT(MPP_CHN_S *veChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_ResetChn(veChn->mChnId);
    _CHECK_RET(ret);

    ret = AW_MPI_VENC_DestroyChn(veChn->mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_HELPER_DESTROY_VENC(MPP_CHN_S veChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_ResetChn(veChn.mChnId);
    _CHECK_RET(ret);

    ret = AW_MPI_VENC_DestroyChn(veChn.mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VO_INIT(MPP_CHN_S *voChn, int x, int y, int width, int height)
{
    ERRORTYPE ret = SUCCESS;

    VO_PUB_ATTR_S pub_attr;
    VO_VIDEO_LAYER_ATTR_S layerAttr;
    MPPCallbackInfo cbInfo;
    VO_LAYER uiLayer = 8;

    memset(&pub_attr, 0, sizeof(pub_attr));
    memset(&layerAttr, 0, sizeof(layerAttr));
    memset(&cbInfo, 0, sizeof(cbInfo));

    ret = AW_MPI_VO_Enable(MPP_HELPER_VO_DEV);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_AddOutsideVideoLayer(uiLayer);
    _CHECK_RET(ret);
    ret = AW_MPI_VO_CloseVideoLayer(uiLayer); /* close ui layer. */
    _CHECK_RET(ret);

    ret = AW_MPI_VO_GetPubAttr(MPP_HELPER_VO_DEV, &pub_attr);
    _CHECK_RET(ret);
    pub_attr.enIntfType = VO_INTF_LCD;
    pub_attr.enIntfSync = VO_OUTPUT_NTSC;
    ret = AW_MPI_VO_SetPubAttr(MPP_HELPER_VO_DEV, &pub_attr);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_EnableVideoLayer(voChn->mDevId);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_GetVideoLayerAttr(voChn->mDevId, &layerAttr);
    _CHECK_RET(ret);
    layerAttr.stDispRect.X = x;
    layerAttr.stDispRect.Y = y;
    layerAttr.stDispRect.Width = width;
    layerAttr.stDispRect.Height = height;
    ret = AW_MPI_VO_SetVideoLayerAttr(voChn->mDevId, &layerAttr);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_CreateChn(voChn->mDevId, voChn->mChnId);
    _CHECK_RET(ret);

    cbInfo.callback = (MPPCallbackFuncType)&voCallbackWrapper;
    ret = AW_MPI_VO_RegisterCallback(voChn->mDevId, voChn->mChnId, &cbInfo);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_SetChnDispBufNum(voChn->mDevId, voChn->mChnId, 2);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_SetVideoLayerPriority(voChn->mDevId, 11);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VI_INIT(MPP_CHN_S *viChn, int width, int height, int fps, PIXEL_FORMAT_E format)
{
    ERRORTYPE ret;
    VI_ATTR_S viAttr;

    ret = AW_MPI_VI_CreateVipp(viChn->mDevId);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_GetVippAttr(viChn->mDevId, &viAttr);
    _CHECK_RET(ret);

    memset(&viAttr, 0, sizeof(VI_ATTR_S));
    viAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    viAttr.memtype = V4L2_MEMORY_MMAP;
    viAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(format);
    viAttr.format.field = V4L2_FIELD_NONE;
    viAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    viAttr.format.width = width;
    viAttr.format.height = height;
    viAttr.fps = fps;
    viAttr.use_current_win = viChn->mDevId > 0 ? 1 : 0;
    viAttr.nbufs = 5;
    viAttr.nplanes = 2;
    viAttr.drop_frame_num = 0;

    ret = AW_MPI_VI_SetVippAttr(viChn->mDevId, &viAttr);
    _CHECK_RET(ret);

    ret = AW_MPI_ISP_Run(MPP_HELPER_ISP_DEV);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_CreateVirChn(viChn->mDevId, viChn->mChnId, NULL);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_EnableVipp(viChn->mDevId);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_SetVippMirror(viChn->mDevId, 0);//0,1
    _CHECK_RET(ret);

    ret = AW_MPI_VI_SetVippFlip(viChn->mDevId, 0);//0,1
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VO_START(MPP_CHN_S *voChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VO_StartChn(voChn->mDevId, voChn->mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VI_START(MPP_CHN_S *viChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VI_EnableVirChn(viChn->mDevId, viChn->mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VO_UNINIT(MPP_CHN_S *voChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VO_DestroyChn(voChn->mDevId, voChn->mChnId);
    _CHECK_RET(ret);

    ret = AW_MPI_VO_DisableVideoLayer(voChn->mDevId);
    _CHECK_RET(ret);

    sleep(1);

    AW_MPI_VO_Disable(MPP_HELPER_VO_DEV);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VI_UNINIT(MPP_CHN_S *viChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_ISP_Stop(MPP_HELPER_ISP_DEV);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_DestroyVirChn(viChn->mDevId, viChn->mChnId);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_DisableVipp(viChn->mDevId);
    _CHECK_RET(ret);

    ret = AW_MPI_VI_DestroyVipp(viChn->mDevId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VI_STOP(MPP_CHN_S *viChn)
{
    ERRORTYPE ret;
    ret = AW_MPI_VI_DisableVirChn(viChn->mDevId, viChn->mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VO_STOP(MPP_CHN_S *voChn)
{
    ERRORTYPE ret;
    ret = AW_MPI_VO_StopChn(voChn->mDevId, voChn->mChnId);
    _CHECK_RET(ret);
}

ERRORTYPE MPP_VI_GET_FRAME(MPP_CHN_S *viChn, VIDEO_FRAME_INFO_S *frameInfo)
{
    ERRORTYPE ret;

    ret = AW_MPI_VI_GetFrame(viChn->mDevId, 0, frameInfo, 500);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VO_SEND_FRAME(MPP_CHN_S *voChn, VIDEO_FRAME_INFO_S *frameInfo)
{
    ERRORTYPE ret;

    ret = AW_MPI_VO_SendFrame(voChn->mDevId, voChn->mChnId, frameInfo, 0);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VEN_SEND_FRAME(MPP_CHN_S *veChn, VIDEO_FRAME_INFO_S *frameInfo)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_SendFrame(veChn->mChnId, frameInfo, 0);
    _CHECK_RET(ret);

    return ret;
}


ERRORTYPE MPP_VI_RELEASE_FRAME(MPP_CHN_S *viChn, VIDEO_FRAME_INFO_S *frameInfo)
{
    ERRORTYPE ret;

    ret = AW_MPI_VI_ReleaseFrame(viChn->mDevId, 0, frameInfo);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VENC_START(MPP_CHN_S *veChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_StartRecvPic(veChn->mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VENC_SET_MOTION_PARAM(MPP_CHN_S *veChn, VencMotionSearchParam *pMotionParam)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_SetMotionSearchParam(veChn->mChnId, pMotionParam);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VENC_GET_MOTION_PARAM(MPP_CHN_S *veChn, VencMotionSearchParam *pMotionParam)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_GetMotionSearchParam(veChn->mChnId, pMotionParam);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VENC_STOP(MPP_CHN_S *veChn)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_StopRecvPic(veChn->mChnId);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VENC_GET_STREAM(MPP_CHN_S *veChn, VENC_STREAM_S *pStream)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_GetStream(veChn->mChnId, pStream, 4000);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VENC_GET_MOTION_RESULT(MPP_CHN_S *veChn, VencMotionSearchResult *pMotionResult)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_GetMotionSearchResult(veChn->mChnId, pMotionResult);
    _CHECK_RET(ret);

    return ret;
}

ERRORTYPE MPP_VENC_RELEASE_STREAM(MPP_CHN_S *veChn, VENC_STREAM_S *pStream)
{
    ERRORTYPE ret;

    ret = AW_MPI_VENC_ReleaseStream(veChn->mChnId, pStream);
    _CHECK_RET(ret);

    return ret;
}

int MPP_REGION_CREATE_RECT(MPP_CHN_S *pstChn, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, RGN_HANDLE Handle, int x, int y, int w, int h)
{
    memset(pStRegion, 0, sizeof(RGN_ATTR_S));
    memset(pStRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    pStRegion->enType = ORL_RGN;
    RGN_HANDLE mOrlHandle = Handle;
    AW_MPI_RGN_Create(mOrlHandle, pStRegion);

    pStRgnChnAttr->bShow = TRUE;
    pStRgnChnAttr->enType = pStRegion->enType;
    pStRgnChnAttr->unChnAttr.stOrlChn.enAreaType = AREA_RECT;
    pStRgnChnAttr->unChnAttr.stOrlChn.stRect.X = x;
    pStRgnChnAttr->unChnAttr.stOrlChn.stRect.Y = y;
    pStRgnChnAttr->unChnAttr.stOrlChn.stRect.Width = w;
    pStRgnChnAttr->unChnAttr.stOrlChn.stRect.Height = h;
    pStRgnChnAttr->unChnAttr.stOrlChn.mColor = 0x0000FF00;
    pStRgnChnAttr->unChnAttr.stOrlChn.mThick = 1;
    pStRgnChnAttr->unChnAttr.stOrlChn.mLayer = 0;
    AW_MPI_RGN_AttachToChn(mOrlHandle, pstChn, pStRgnChnAttr);
    return 0;
}

int MPP_REGION_DESTORY_RECT(MPP_CHN_S *pstChn, RGN_HANDLE Handle)
{
    RGN_HANDLE mOrlHandle = Handle;

    AW_MPI_RGN_DetachFromChn(mOrlHandle, pstChn);
    AW_MPI_RGN_Destroy(mOrlHandle);

    return 0;
}

int MPP_REGION_CREATE_BITMAP(MPP_CHN_S *pstChn, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, RGN_HANDLE Handle, int x, int y, RGB_PIC_S *rgb)
{
    memset(pStRegion, 0, sizeof(RGN_ATTR_S));
    memset(pStRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));

    pStRegion->enType = OVERLAY_RGN;
    pStRegion->unAttr.stOverlay.mPixelFmt = MM_PIXEL_FORMAT_RGB_8888;
    pStRegion->unAttr.stOverlay.mSize.Width = rgb->wide;
    pStRegion->unAttr.stOverlay.mSize.Height = rgb->high;

    RGN_HANDLE mOverlayHandle = Handle;
    AW_MPI_RGN_Create(mOverlayHandle, pStRegion);

    BITMAP_S stBmp;
    int nSize = 0;
    memset(&stBmp, 0, sizeof(BITMAP_S));
    stBmp.mPixelFormat = pStRegion->unAttr.stOverlay.mPixelFmt;

    stBmp.mWidth = rgb->wide;
    stBmp.mHeight = rgb->high;
    stBmp.mpData  = rgb->pic_addr;
    AW_MPI_RGN_SetBitMap(mOverlayHandle, &stBmp);

    pStRgnChnAttr->bShow = TRUE;
    pStRgnChnAttr->enType = pStRegion->enType;
    pStRgnChnAttr->unChnAttr.stOverlayChn.stPoint.X = x;
    pStRgnChnAttr->unChnAttr.stOverlayChn.stPoint.Y = y - stBmp.mHeight;
    pStRgnChnAttr->unChnAttr.stOverlayChn.mLayer = 0;
    pStRgnChnAttr->unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
    pStRgnChnAttr->unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
    pStRgnChnAttr->unChnAttr.stOverlayChn.stInvertColor.mLumThresh = 60;
    pStRgnChnAttr->unChnAttr.stOverlayChn.stInvertColor.enChgMod = 3;
    pStRgnChnAttr->unChnAttr.stOverlayChn.stInvertColor.bInvColEn = 1;
//    alogd("overlay attach to ve");
    AW_MPI_RGN_AttachToChn(mOverlayHandle, pstChn, pStRgnChnAttr);
 //   alogd("overlay attach to ve done");

    return 0;
}

int MPP_REGION_DESTORY_BITMAP(MPP_CHN_S *pstChn, RGN_HANDLE Handle)
{
    RGN_HANDLE mOverlayHandle = Handle;

    AW_MPI_RGN_DetachFromChn(mOverlayHandle, pstChn);
    AW_MPI_RGN_Destroy(mOverlayHandle);

    return 0;
}
