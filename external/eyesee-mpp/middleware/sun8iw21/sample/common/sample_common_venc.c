#include <utils/plat_log.h>
#include "sample_common_venc.h"
#include "media/mpi_isp.h"
#include <mpi_vi.h>

int configVencRateCtrlParam(const VencRateCtrlConfig *pRcConfig, VENC_CHN_ATTR_S *pVencChnAttr, VENC_RC_PARAM_S *pVencRcParam)
{
    int ret = 0;
    if (NULL == pVencChnAttr || NULL == pVencRcParam || NULL == pRcConfig)
    {
        aloge("fatal error! invalid input params! %p, %p, %p", pVencChnAttr, pVencRcParam, pRcConfig);
        return -1;
    }
    if(PT_H264 == pRcConfig->mEncodeType)
    {
        pVencChnAttr->VeAttr.AttrH264e.Profile = 2;
        pVencChnAttr->VeAttr.AttrH264e.bByFrame = TRUE;
        pVencChnAttr->VeAttr.AttrH264e.PicWidth = pRcConfig->mEncodeWidth;
        pVencChnAttr->VeAttr.AttrH264e.PicHeight = pRcConfig->mEncodeHeight;
        pVencChnAttr->VeAttr.AttrH264e.mLevel = 0;
        pVencChnAttr->VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pVencChnAttr->VeAttr.AttrH264e.mThreshSize = pRcConfig->mVbvThreshSize;
        pVencChnAttr->VeAttr.AttrH264e.BufSize = pRcConfig->mVbvBufSize;
        pVencChnAttr->RcAttr.mRcMode = pRcConfig->mRcMode;
        switch (pVencChnAttr->RcAttr.mRcMode)
        {
        case VENC_RC_MODE_H264CBR:
            pVencChnAttr->RcAttr.mAttrH264Cbr.mBitRate = pRcConfig->mEncodeBitrate;
            pVencChnAttr->RcAttr.mAttrH264Cbr.mSrcFrmRate = pRcConfig->mSrcFrameRate;
            pVencChnAttr->RcAttr.mAttrH264Cbr.mDstFrmRate = pRcConfig->mDstFrameRate;
            pVencRcParam->ParamH264Cbr.mMaxQp = pRcConfig->mMaxIQp;
            pVencRcParam->ParamH264Cbr.mMinQp = pRcConfig->mMinIQp;
            pVencRcParam->ParamH264Cbr.mMaxPqp = pRcConfig->mMaxPQp;
            pVencRcParam->ParamH264Cbr.mMinPqp = pRcConfig->mMinPQp;
            pVencRcParam->ParamH264Cbr.mQpInit = pRcConfig->mInitQp;
            pVencRcParam->ParamH264Cbr.mbEnMbQpLimit = pRcConfig->mEnMbQpLimit;
            break;
        case VENC_RC_MODE_H264VBR:
            pVencChnAttr->RcAttr.mAttrH264Vbr.mMaxBitRate = pRcConfig->mEncodeBitrate;
            pVencChnAttr->RcAttr.mAttrH264Vbr.mSrcFrmRate = pRcConfig->mSrcFrameRate;
            pVencChnAttr->RcAttr.mAttrH264Vbr.mDstFrmRate = pRcConfig->mDstFrameRate;
            pVencRcParam->ParamH264Vbr.mMaxQp = pRcConfig->mMaxIQp;
            pVencRcParam->ParamH264Vbr.mMinQp = pRcConfig->mMinIQp;
            pVencRcParam->ParamH264Vbr.mMaxPqp = pRcConfig->mMaxPQp;
            pVencRcParam->ParamH264Vbr.mMinPqp = pRcConfig->mMinPQp;
            pVencRcParam->ParamH264Vbr.mQpInit = pRcConfig->mInitQp;
            pVencRcParam->ParamH264Vbr.mbEnMbQpLimit = pRcConfig->mEnMbQpLimit;
            pVencRcParam->ParamH264Vbr.mQuality = pRcConfig->mQuality;
            pVencRcParam->ParamH264Vbr.mMovingTh = pRcConfig->mMovingTh;
            pVencRcParam->ParamH264Vbr.mIFrmBitsCoef = pRcConfig->mIBitsCoef;
            pVencRcParam->ParamH264Vbr.mPFrmBitsCoef = pRcConfig->mPBitsCoef;
            break;
        case VENC_RC_MODE_H264FIXQP:
            pVencChnAttr->RcAttr.mAttrH264FixQp.mIQp = pRcConfig->mMinIQp;
            pVencChnAttr->RcAttr.mAttrH264FixQp.mPQp = pRcConfig->mMinPQp;
            pVencChnAttr->RcAttr.mAttrH264FixQp.mSrcFrmRate = pRcConfig->mSrcFrameRate;
            pVencChnAttr->RcAttr.mAttrH264FixQp.mDstFrmRate = pRcConfig->mDstFrameRate;
            break;
        default:
            aloge("fatal error! H264 rc mode %d is not support!", pRcConfig->mRcMode);
            ret = -1;
            break;
        }
    }
    else if(PT_H265 == pRcConfig->mEncodeType)
    {
        pVencChnAttr->VeAttr.AttrH265e.mProfile = 0;
        pVencChnAttr->VeAttr.AttrH265e.mbByFrame = TRUE;
        pVencChnAttr->VeAttr.AttrH265e.mPicWidth = pRcConfig->mEncodeWidth;
        pVencChnAttr->VeAttr.AttrH265e.mPicHeight = pRcConfig->mEncodeHeight;
        pVencChnAttr->VeAttr.AttrH265e.mLevel = 0;
        pVencChnAttr->VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pVencChnAttr->VeAttr.AttrH265e.mThreshSize = pRcConfig->mVbvThreshSize;
        pVencChnAttr->VeAttr.AttrH265e.mBufSize = pRcConfig->mVbvBufSize;
        pVencChnAttr->RcAttr.mRcMode = pRcConfig->mRcMode;
        switch (pVencChnAttr->RcAttr.mRcMode)
        {
        case VENC_RC_MODE_H265CBR:
            pVencChnAttr->RcAttr.mAttrH265Cbr.mBitRate = pRcConfig->mEncodeBitrate;
            pVencChnAttr->RcAttr.mAttrH265Cbr.mSrcFrmRate = pRcConfig->mSrcFrameRate;
            pVencChnAttr->RcAttr.mAttrH265Cbr.mDstFrmRate = pRcConfig->mDstFrameRate;
            pVencRcParam->ParamH265Cbr.mMaxQp = pRcConfig->mMaxIQp;
            pVencRcParam->ParamH265Cbr.mMinQp = pRcConfig->mMinIQp;
            pVencRcParam->ParamH265Cbr.mMaxPqp = pRcConfig->mMaxPQp;
            pVencRcParam->ParamH265Cbr.mMinPqp = pRcConfig->mMinPQp;
            pVencRcParam->ParamH265Cbr.mQpInit = pRcConfig->mInitQp;
            pVencRcParam->ParamH265Cbr.mbEnMbQpLimit = pRcConfig->mEnMbQpLimit;
            break;
        case VENC_RC_MODE_H265VBR:
            pVencChnAttr->RcAttr.mAttrH265Vbr.mMaxBitRate = pRcConfig->mEncodeBitrate;
            pVencChnAttr->RcAttr.mAttrH265Vbr.mSrcFrmRate = pRcConfig->mSrcFrameRate;
            pVencChnAttr->RcAttr.mAttrH265Vbr.mDstFrmRate = pRcConfig->mDstFrameRate;
            pVencRcParam->ParamH265Vbr.mMaxQp = pRcConfig->mMaxIQp;
            pVencRcParam->ParamH265Vbr.mMinQp = pRcConfig->mMinIQp;
            pVencRcParam->ParamH265Vbr.mMaxPqp = pRcConfig->mMaxPQp;
            pVencRcParam->ParamH265Vbr.mMinPqp = pRcConfig->mMinPQp;
            pVencRcParam->ParamH265Vbr.mQpInit = pRcConfig->mInitQp;
            pVencRcParam->ParamH265Vbr.mbEnMbQpLimit = pRcConfig->mEnMbQpLimit;
            pVencRcParam->ParamH265Vbr.mQuality = pRcConfig->mQuality;
            pVencRcParam->ParamH265Vbr.mMovingTh = pRcConfig->mMovingTh;
            pVencRcParam->ParamH265Vbr.mIFrmBitsCoef = pRcConfig->mIBitsCoef;
            pVencRcParam->ParamH265Vbr.mPFrmBitsCoef = pRcConfig->mPBitsCoef;
            break;
        case VENC_RC_MODE_H265FIXQP:
            pVencChnAttr->RcAttr.mAttrH265FixQp.mIQp = pRcConfig->mMinIQp;
            pVencChnAttr->RcAttr.mAttrH265FixQp.mPQp = pRcConfig->mMinPQp;
            pVencChnAttr->RcAttr.mAttrH265FixQp.mSrcFrmRate = pRcConfig->mSrcFrameRate;
            pVencChnAttr->RcAttr.mAttrH265FixQp.mDstFrmRate = pRcConfig->mDstFrameRate;
            break;
        default:
            aloge("fatal error! H265 rc mode %d is not support!", pRcConfig->mRcMode);
            ret = -1;
            break;
        }
    }
    else if(PT_MJPEG == pRcConfig->mEncodeType)
    {
        pVencChnAttr->VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pVencChnAttr->VeAttr.AttrMjpeg.mPicWidth= pRcConfig->mEncodeWidth;
        pVencChnAttr->VeAttr.AttrMjpeg.mPicHeight = pRcConfig->mEncodeHeight;
        pVencChnAttr->VeAttr.AttrMjpeg.mThreshSize = pRcConfig->mVbvThreshSize;
        pVencChnAttr->VeAttr.AttrMjpeg.mBufSize = pRcConfig->mVbvBufSize;
        pVencChnAttr->RcAttr.mRcMode = pRcConfig->mRcMode;
        switch (pVencChnAttr->RcAttr.mRcMode)
        {
        case VENC_RC_MODE_MJPEGCBR:
            pVencChnAttr->RcAttr.mAttrMjpegeCbr.mBitRate = pRcConfig->mEncodeBitrate;
            pVencChnAttr->RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pRcConfig->mSrcFrameRate;
            pVencChnAttr->RcAttr.mAttrMjpegeCbr.mDstFrmRate = pRcConfig->mDstFrameRate;
            break;
        case VENC_RC_MODE_MJPEGFIXQP:
            pVencChnAttr->RcAttr.mAttrMjpegeFixQp.mQfactor = 40;
            pVencChnAttr->RcAttr.mAttrMjpegeFixQp.mSrcFrmRate = pRcConfig->mSrcFrameRate;
            pVencChnAttr->RcAttr.mAttrMjpegeFixQp.mDstFrmRate = pRcConfig->mDstFrameRate;
            break;
        default:
            aloge("fatal error! MJPEG rc mode %d is not support!", pRcConfig->mRcMode);
            ret = -1;
            break;
        }
    }

    return ret;
}

void setVenc2Dnr(VENC_CHN mVEncChn)
{
    s2DfilterParam m2DnrPara;
    memset(&m2DnrPara, 0, sizeof(s2DfilterParam));
    m2DnrPara.enable_2d_filter = 1;
    m2DnrPara.filter_strength_y = 127;
    m2DnrPara.filter_strength_uv = 127;
    m2DnrPara.filter_th_y = 11;
    m2DnrPara.filter_th_uv = 7;
    AW_MPI_VENC_Set2DFilter(mVEncChn, &m2DnrPara);
    alogd("VencChn[%d] enable and set 2DFilter param", mVEncChn);
}

void setVenc3Dnr(VENC_CHN mVEncChn)
{
    s3DfilterParam m3DnrPara;
    memset(&m3DnrPara, 0, sizeof(s3DfilterParam));
    m3DnrPara.enable_3d_filter = 1;
    m3DnrPara.adjust_pix_level_enable = 0;
    m3DnrPara.max_pix_diff_th = 6;
#if VE_REGION_D3D_ENABLE
    m3DnrPara.max_mv_th = 5;
    m3DnrPara.max_mad_th = 48;
    m3DnrPara.smooth_filter_enable = 0;
    m3DnrPara.min_coef = 0;
#else
    m3DnrPara.max_mv_th = 8;
    m3DnrPara.max_mad_th = 11;
    m3DnrPara.smooth_filter_enable = 1;
    m3DnrPara.min_coef = 13;
#endif
    m3DnrPara.max_coef = 16;
    AW_MPI_VENC_Set3DFilter(mVEncChn, &m3DnrPara);
    alogd("VencChn[%d] enable and set 3DFilter param", mVEncChn);
}

void setVencSuperFrameCfg(VENC_CHN mVEncChn, int mBitrate, int mFramerate)
{
    VENC_SUPERFRAME_CFG_S mSuperFrmParam;
    memset(&mSuperFrmParam, 0, sizeof(VENC_SUPERFRAME_CFG_S));
#if VE_SUPER_FRM_MODE_REENCODE_ENABLE
    mSuperFrmParam.enSuperFrmMode = SUPERFRM_REENCODE;
#else
    mSuperFrmParam.enSuperFrmMode = SUPERFRM_NONE;
#endif
    mSuperFrmParam.MaxRencodeTimes = 1;
    mSuperFrmParam.MaxP2IFrameBitsRatio = 0.33;
    float cmp_bits = 1.5*1024*1024 / 20;
    float dst_bits = (float)mBitrate / mFramerate;
    float bits_ratio = dst_bits / cmp_bits;
    mSuperFrmParam.SuperIFrmBitsThr = (unsigned int)((8.0*250*1024) * bits_ratio);
    mSuperFrmParam.SuperPFrmBitsThr = mSuperFrmParam.SuperIFrmBitsThr / 3;
    alogd("SuperFrm Mode:%d, MaxRencodeTimes:%d, MaxP2IFrameBitsRatio:%.2f, IBitsThr:%d, PBitsThr:%d",
        mSuperFrmParam.enSuperFrmMode, mSuperFrmParam.MaxRencodeTimes, mSuperFrmParam.MaxP2IFrameBitsRatio,
        mSuperFrmParam.SuperIFrmBitsThr, mSuperFrmParam.SuperPFrmBitsThr);
    AW_MPI_VENC_SetSuperFrameCfg(mVEncChn, &mSuperFrmParam);
}

void setVencRegionD3D(int mVEncChn, int dstWidth, int dstHeight)
{
#if VE_REGION_D3D_ENABLE
    VencRegionD3DParam mRegionD3DParam;
    memset(&mRegionD3DParam, 0, sizeof(VencRegionD3DParam));
    AW_MPI_VENC_GetRegionD3DParam(mVEncChn, &mRegionD3DParam);
    mRegionD3DParam.en_region_d3d = 1;
    mRegionD3DParam.dis_default_para = 1;
    mRegionD3DParam.result_num = 3; // [1, 5], default is 1
    mRegionD3DParam.chroma_offset = 16;
    mRegionD3DParam.zero_mv_rate_th[0] = 93;
    mRegionD3DParam.zero_mv_rate_th[1] = 87;
    mRegionD3DParam.zero_mv_rate_th[2] = 81;
    mRegionD3DParam.hor_region_num = AWALIGN(dstWidth, 16) / 64;
    mRegionD3DParam.ver_region_num = AWALIGN(dstHeight, 16) / 64;
    mRegionD3DParam.hor_expand_num = 2;
    mRegionD3DParam.ver_expand_num = 2;
    mRegionD3DParam.static_coef[0] = 8;
    mRegionD3DParam.static_coef[1] = 9;
    mRegionD3DParam.static_coef[2] = 10;
    mRegionD3DParam.motion_coef[0] = 14;
    mRegionD3DParam.motion_coef[1] = 15;
    mRegionD3DParam.motion_coef[2] = 16;
    mRegionD3DParam.motion_coef[3] = 16;
    alogd("VeChn[%d] set RegionD3D en %d", mVEncChn, mRegionD3DParam.en_region_d3d);
    AW_MPI_VENC_SetRegionD3DParam(mVEncChn, &mRegionD3DParam);
#else
    alogd("VeChn[%d] disable RegionD3D by default.", mVEncChn);
#endif
}

void configBitsClipParam(VENC_RC_PARAM_S *pVEncRcParam)
{
   VencTargetBitsClipParam *pBitsClipParam = &pVEncRcParam->mBitsClipParam;
#if VE_IPC_PRODUCT_ROTATING
    pBitsClipParam->dis_default_para = 1;
    pBitsClipParam->mode = 0;
    pBitsClipParam->coef_th[0][0] = -0.5;
    pBitsClipParam->coef_th[0][1] = -0.2;
    pBitsClipParam->coef_th[1][0] = -0.3;
    pBitsClipParam->coef_th[1][1] = -0.1;
    pBitsClipParam->coef_th[2][0] = -0.3;
    pBitsClipParam->coef_th[2][1] =  0;
    pBitsClipParam->coef_th[3][0] = -0.5;
    pBitsClipParam->coef_th[3][1] =  0.1;
    pBitsClipParam->coef_th[4][0] =  0.4;
    pBitsClipParam->coef_th[4][1] =  0.7;
    pBitsClipParam->en_gop_clip = 0;
    pBitsClipParam->gop_bit_ratio_th[0] = 0;
    pBitsClipParam->gop_bit_ratio_th[1] = 1;
    pBitsClipParam->gop_bit_ratio_th[2] = 2;
#else
    pBitsClipParam->dis_default_para = 1;
    pBitsClipParam->mode = 1;
    pBitsClipParam->coef_th[0][0] = -0.5;
    pBitsClipParam->coef_th[0][1] =  0.2;
    pBitsClipParam->coef_th[1][0] = -0.3;
    pBitsClipParam->coef_th[1][1] =  0.3;
    pBitsClipParam->coef_th[2][0] = -0.3;
    pBitsClipParam->coef_th[2][1] =  0.3;
    pBitsClipParam->coef_th[3][0] = -0.5;
    pBitsClipParam->coef_th[3][1] =  0.5;
    pBitsClipParam->coef_th[4][0] =  0.4;
    pBitsClipParam->coef_th[4][1] =  0.7;
    pBitsClipParam->en_gop_clip = 0;
    pBitsClipParam->gop_bit_ratio_th[0] = 0;
    pBitsClipParam->gop_bit_ratio_th[1] = 1;
    pBitsClipParam->gop_bit_ratio_th[2] = 2;
#endif
    alogd("BitsClipParam: %d %d {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%.2f,%.2f}, {%d,%.2f,%.2f,%.2f}",
        pBitsClipParam->dis_default_para, pBitsClipParam->mode,
        pBitsClipParam->coef_th[0][0], pBitsClipParam->coef_th[0][1],
        pBitsClipParam->coef_th[1][0], pBitsClipParam->coef_th[1][1],
        pBitsClipParam->coef_th[2][0], pBitsClipParam->coef_th[2][1],
        pBitsClipParam->coef_th[3][0], pBitsClipParam->coef_th[3][1],
        pBitsClipParam->coef_th[4][0], pBitsClipParam->coef_th[4][1],
        pBitsClipParam->en_gop_clip, pBitsClipParam->gop_bit_ratio_th[0], pBitsClipParam->gop_bit_ratio_th[1], pBitsClipParam->gop_bit_ratio_th[2]);
}

void configIFrmMbRcMoveStatus(VENC_RC_PARAM_S *pVEncRcParam)
{
#if VE_IPC_PRODUCT_ROTATING
    pVEncRcParam->EnIFrmMbRcMoveStatusEnable = 1;
    pVEncRcParam->EnIFrmMbRcMoveStatus = 0;
#else
    pVEncRcParam->EnIFrmMbRcMoveStatusEnable = 1;
    pVEncRcParam->EnIFrmMbRcMoveStatus = 3;
#endif
}

int setVencLensMovingMaxQp(int mVEncChn, int nLensMovingMaxQp)
{
    alogd("VeChn[%d] set LensMovingMaxQp %d", mVEncChn, nLensMovingMaxQp);
    return AW_MPI_VENC_SetLensMovingMaxQp(mVEncChn, nLensMovingMaxQp);
}

#if 0
int setIsp2VeLinkageParam(Isp2VeLinkageParam *pIsp2Ve)
{
    BOOL bIspAndVeLinkageEnable = pIsp2Ve->mIspAndVeLinkageEnable;
    BOOL bCameraAdaptiveMovingAndStaticEnable = pIsp2Ve->mCameraAdaptiveMovingAndStaticEnable;
    int nVEncChn = pIsp2Ve->mVEncChn;
    int nVipp = pIsp2Ve->mVipp;
    VencIsp2VeParam *pIsp2VeParam = pIsp2Ve->pIsp2VeParam;
    unsigned int nEncppSharpAttenCoefPer = pIsp2Ve->nEncppSharpAttenCoefPer;
    int ret = 0;

    if (pIsp2VeParam)
    {
        sEncppSharpParam *pSharpParam = &pIsp2VeParam->mSharpParam;
        ISP_DEV mIspDev = 0;
        ret = AW_MPI_VI_GetIspDev(nVipp, &mIspDev);
        if (ret)
        {
            aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", nVipp, ret);
            return -1;
        }

        struct enc_VencIsp2VeParam mIsp2VeParam;
        memset(&mIsp2VeParam, 0, sizeof(struct enc_VencIsp2VeParam));
        mIsp2VeParam.AeStatsInfo = (struct isp_ae_stats_s *)&pIsp2VeParam->mIspAeStatus;
        ret = AW_MPI_ISP_GetIsp2VeParam(mIspDev, &mIsp2VeParam);
        if (ret)
        {
            aloge("fatal error, isp[%d] GetIsp2VeParam failed! ret=%d", mIspDev, ret);
            return -1;
        }

        if (mIsp2VeParam.encpp_en)
        {
            VENC_CHN_ATTR_S stVencAttr;
            memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
            AW_MPI_VENC_GetChnAttr(nVEncChn, &stVencAttr);
            if (FALSE == stVencAttr.EncppAttr.mbEncppEnable)
            {
                stVencAttr.EncppAttr.mbEncppEnable = TRUE;
                AW_MPI_VENC_SetChnAttr(nVEncChn, &stVencAttr);
            }
            if (100 != nEncppSharpAttenCoefPer)
            {
                mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ss_blk_stren * nEncppSharpAttenCoefPer / 100;
                mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ss_wht_stren * nEncppSharpAttenCoefPer / 100;
                mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren = mIsp2VeParam.mDynamicSharpCfg.ls_blk_stren * nEncppSharpAttenCoefPer / 100;
                mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren = mIsp2VeParam.mDynamicSharpCfg.ls_wht_stren * nEncppSharpAttenCoefPer / 100;
            }
            memcpy(&pSharpParam->mDynamicParam, &mIsp2VeParam.mDynamicSharpCfg,sizeof(sEncppSharpParamDynamic));
            memcpy(&pSharpParam->mStaticParam, &mIsp2VeParam.mStaticSharpCfg, sizeof(sEncppSharpParamStatic));
        }
        else
        {
            VENC_CHN_ATTR_S stVencAttr;
            memset(&stVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
            AW_MPI_VENC_GetChnAttr(nVEncChn, &stVencAttr);
            if (TRUE == stVencAttr.EncppAttr.mbEncppEnable)
            {
                stVencAttr.EncppAttr.mbEncppEnable = FALSE;
                AW_MPI_VENC_SetChnAttr(nVEncChn, &stVencAttr);
            }
        }

        if (bIspAndVeLinkageEnable)
        {
            pIsp2VeParam->mEnvLv = AW_MPI_ISP_GetEnvLV(mIspDev);
            pIsp2VeParam->mAeWeightLum = AW_MPI_ISP_GetAeWeightLum(mIspDev);
            if (bCameraAdaptiveMovingAndStaticEnable)
            {
                pIsp2VeParam->mEnCameraMove = CAMERA_ADAPTIVE_MOVING_AND_STATIC;
            }
            else
            {
                /**
                Example code:
                static int lens_start_moving_flag = 1; // obtain motor motion status from motor driver.
                if (lens_start_moving_flag)
                {
                    pIsp2VeParam->mEnCameraMove = CAMERA_FORCE_MOVING;
                    lens_start_moving_flag = 0;
                }
                else
                {
                    pIsp2VeParam->mEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                }
                */
                pIsp2VeParam->mEnCameraMove = CAMERA_ADAPTIVE_STATIC;
            }
        }
    }

    return ret;
}

int setVe2IspLinkageParam(Ve2IspLinkageParam *pVe2Isp)
{
    BOOL bIspAndVeLinkageEnable = pVe2Isp->mIspAndVeLinkageEnable;
    int nVEncChn = pVe2Isp->mVEncChn;
    int nVipp = pVe2Isp->mVipp;
    VencVe2IspParam *pVe2IspParam = (VencVe2IspParam *)(pVe2Isp->p2Ve2IspParam);
    int ret = 0;

    if (pVe2IspParam && bIspAndVeLinkageEnable)
    {
        ISP_DEV mIspDev = 0;
        ret = AW_MPI_VI_GetIspDev(nVipp, &mIspDev);
        if (ret)
        {
            aloge("fatal error, vipp[%d] GetIspDev failed! ret=%d", nVipp, ret);
            return -1;
        }
        alogv("update Ve2IspParam, route Isp[%d]-Vipp[%d]-VencChn[%d]", mIspDev, nVipp, nVEncChn);

        alogv("isp[%d] d2d_level=%d, d3d_level=%d, is_overflow=%d", mIspDev, pVe2IspParam->d2d_level, pVe2IspParam->d3d_level, pVe2IspParam->mMovingLevelInfo.is_overflow);
        struct enc_VencVe2IspParam mIspParam;
        memset(&mIspParam, 0, sizeof(struct enc_VencVe2IspParam));
        mIspParam.d2d_level = pVe2IspParam->d2d_level;
        mIspParam.d3d_level = pVe2IspParam->d3d_level;
        memcpy(&mIspParam.mMovingLevelInfo, &pVe2IspParam->mMovingLevelInfo, sizeof(MovingLevelInfo));
        ret = AW_MPI_ISP_SetVe2IspParam(mIspDev, &mIspParam);
        if (ret)
        {
            aloge("fatal error, isp[%d] SetVe2IspParam failed! ret=%d", mIspDev, ret);
            return -1;
        }
    }

    return ret;
}
#endif

