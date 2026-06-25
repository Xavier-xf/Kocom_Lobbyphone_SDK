#ifndef __SAMPLE_COMMON_VENC_H__
#define __SAMPLE_COMMON_VENC_H__

#include <include/media/mpi_venc.h>

#ifdef __cplusplus
       extern "C" {
#endif

#define VE_VBV_CACHE_TIME                   (4)  // unit:seconds, exp:1,2,3,4...
#define VE_SUPER_FRM_MODE_REENCODE_ENABLE   (0)  // 0:disable reencode, 1:enable reencode
#define VE_IPC_PRODUCT_ROTATING             (0)  // 0:No rotation(qiang ji ...), 1:Rotating(qiu ji, yun tai ...)

typedef struct VencRateCtrlConfig {
    PAYLOAD_TYPE_E mEncodeType;
    VENC_RC_MODE_E mRcMode;
    int mInitQp;
    int mMinIQp;
    int mMaxIQp;
    int mMinPQp;
    int mMaxPQp;
    int mEnMbQpLimit;
    int mMovingTh;
    int mQuality;
    int mPBitsCoef;
    int mIBitsCoef;
    int mEncodeWidth;
    int mEncodeHeight;
    int mSrcFrameRate;
    int mDstFrameRate;
    int mEncodeBitrate;
    unsigned int mVbvThreshSize;
    unsigned int mVbvBufSize;
}VencRateCtrlConfig;

typedef struct Isp2VeLinkageParam {
    BOOL mIspAndVeLinkageEnable;
    BOOL mCameraAdaptiveMovingAndStaticEnable;
    int mVEncChn;
    int mVipp;
    VencIsp2VeParam *pIsp2VeParam;
    unsigned int nEncppSharpAttenCoefPer;
    int mEncppSharpDebugDisable;
} Isp2VeLinkageParam;

typedef struct Ve2IspLinkageParam {
    BOOL mIspAndVeLinkageEnable;
    int mVEncChn;
    int mVipp;
    VencVe2IspParam *p2Ve2IspParam;
} Ve2IspLinkageParam;

int configVencRateCtrlParam(const VencRateCtrlConfig *pRcConfig, VENC_CHN_ATTR_S *pVencChnAttr, VENC_RC_PARAM_S *pVencRcParam);
void setVenc2Dnr(VENC_CHN mVEncChn);
void setVenc3Dnr(VENC_CHN mVEncChn);
void setVencSuperFrameCfg(VENC_CHN mVEncChn, int mBitrate, int mFramerate);
void configBitsClipParam(VENC_RC_PARAM_S *pVEncRcParam);
void configIFrmMbRcMoveStatus(VENC_RC_PARAM_S *pVEncRcParam);
int setVencLensMovingMaxQp(int mVEncChn, int nLensMovingMaxQp);
//int setIsp2VeLinkageParam(Isp2VeLinkageParam *pIsp2Ve);
//int setVe2IspLinkageParam(Ve2IspLinkageParam *pVe2Isp);

#ifdef __cplusplus
       }
#endif

#endif
