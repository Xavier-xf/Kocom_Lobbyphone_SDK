/* SPDX-License-Identifier: GPL-2.0-or-later WITH Linux-syscall-note */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/**
  This file is used to export some structs of vencoder to user space, mainly for rt_media.

  @author eric_wang
  @date 20241011
*/
#ifndef _VENCODER_BASE_H_
#define _VENCODER_BASE_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct VencRect {
	int nLeft;
	int nTop;
	int nWidth;
	int nHeight;
} VencRect;

typedef struct {
    int enable_crop;
    VencRect s_crop_rect;
} CropInfo;

typedef enum eVencEncppScalerRatio {
	VENC_ISP_SCALER_0       = 0, //not scaler
	VENC_ISP_SCALER_EIGHTH  = 1, //scaler 1/8 write back
	VENC_ISP_SCALER_HALF    = 2, //scaler 1/2 write back
	VENC_ISP_SCALER_QUARTER = 3, //scaler 1/4 write back
} eVencEncppScalerRatio;

typedef struct sWbYuvParam {
	unsigned int bEnableWbYuv;
	unsigned int nWbBufferNum;
    unsigned int bEnableCrop;
    VencRect     sWbYuvcrop;
	eVencEncppScalerRatio scalerRatio;
} sWbYuvParam;

typedef struct s3DfilterParam {
	unsigned char enable_3d_filter;
	unsigned char adjust_pix_level_enable; // adjustment of coef pix level enable
	unsigned char smooth_filter_enable; //* 3x3 smooth filter enable
	unsigned char max_pix_diff_th; //* range[0~31]: maximum threshold of pixel difference
	unsigned char max_mad_th; //* range[0~63]: maximum threshold of mad
	unsigned char max_mv_th; //* range[0~63]: maximum threshold of motion vector
	unsigned char min_coef; //* range[0~16]: minimum weight of 3d filter
	unsigned char max_coef; //* range[0~16]: maximum weight of 3d filter,
} s3DfilterParam;

typedef struct VencRegionD3DRegion {
	int pix_x_bgn;
	int pix_x_end;
	int pix_y_bgn;
	int pix_y_end;
	int total_num;
	int zero_mv_num;
	int is_possible_static;
	int is_really_static;
	float zero_mv_rate;
} VencRegionD3DRegion;

typedef struct sVencRegionD3DResult VencRegionD3DResult;
struct sVencRegionD3DResult {
	int total_region_num;
	int static_region_num;
	VencRegionD3DRegion *region;
    VencRegionD3DResult **prev;
    VencRegionD3DResult **next;
};

typedef struct VencRegionD3DParam {
	int en_region_d3d;
	int dis_default_para;
    int result_num;
	int hor_region_num;
	int ver_region_num;
	int hor_expand_num;
	int ver_expand_num;
	int lv_weak_th[9];
	float zero_mv_rate_th[3];
	unsigned char chroma_offset;
	unsigned char static_coef[3]; //* fallow by zero_mv_rate
	unsigned char motion_coef[4]; //* fallow by MoveStatus
} VencRegionD3DParam;

typedef struct VencExtremeD3DParam {
	unsigned char  en_extreme_d3d;
	float          zero_mv_ratio_th;
	s3DfilterParam ex_d3d_param;
} VencExtremeD3DParam;

typedef struct s2DfilterParam {
	unsigned char enable_2d_filter;
	unsigned char filter_strength_uv; //* range[0~255], 0 means close 2d filter, advice: 32
	unsigned char filter_strength_y; //* range[0~255], 0 means close 2d filter, advice: 32
	unsigned char filter_th_uv; //* range[0~15], advice: 2
	unsigned char filter_th_y; //* range[0~15], advice: 2
} s2DfilterParam;

typedef struct VencVe2IspD2DLimit {
	int en_d2d_limit;
	int d2d_level[6];
} VencVe2IspD2DLimit;

typedef struct {
    int dis_default_d2d;
    int d2d_min_lv_th;
    int d2d_max_lv_th;
    int d2d_min_lv_level;
    int d2d_max_lv_level;
    int dis_default_d3d;
    int d3d_min_lv_th;
    int d3d_max_lv_th;
    int d3d_min_lv_level;
    int d3d_max_lv_level;
} VencRotVe2Isp;

typedef struct VencTargetBitsClipParam {
	int dis_default_para;
	int mode;
	int en_gop_clip;
	float gop_bit_ratio_th[3];
	float coef_th[5][2];
} VencTargetBitsClipParam;

typedef enum eGdcPerspFunc {
	Gdc_Persp_Only,
	Gdc_Persp_LDC
} eGdcPerspFunc;

typedef enum eGdcLensDistModel {
	Gdc_DistModel_WideAngle,
	Gdc_DistModel_FishEye
} eGdcLensDistModel;

typedef enum eGdcMountType {
	Gdc_Mount_Top,
	Gdc_Mount_Wall,
	Gdc_Mount_Bottom
} eGdcMountType;

typedef enum eGdcWarpType {
	Gdc_Warp_LDC,
	Gdc_Warp_Pano180,
	Gdc_Warp_Pano360,
	Gdc_Warp_Normal,
	Gdc_Warp_Fish2Wide,
	Gdc_Warp_Perspective,
	Gdc_Warp_BirdsEye,
	Gdc_Warp_User
} eGdcWarpType;

typedef enum {
	CAMERA_ADAPTIVE_STATIC = 0,
	CAMERA_FORCE_STATIC = 1,
	CAMERA_FORCE_MOVING = 2,
	CAMERA_ADAPTIVE_MOVING_AND_STATIC = 3,
	CAMERA_STATUS_NUM
} eCameraStatus;

typedef struct {
	unsigned char bGDC_en;
	eGdcWarpType eWarpMode;
	eGdcMountType eMountMode;
	unsigned char bMirror;
	unsigned int calib_widht;
	unsigned int calib_height;
	float fx;
	float fy;
	float cx;
	float cy;
	float fx_scale;
	float fy_scale;
	float cx_scale;
	float cy_scale;
	eGdcLensDistModel eLensDistModel;
	float distCoef_wide_ra[3];
	float distCoef_wide_ta[2];
	float distCoef_fish_k[4];
	int centerOffsetX;
	int centerOffsetY;
	int rotateAngle;
	int radialDistortCoef;
	int trapezoidDistortCoef;
	int fanDistortCoef;
	int pan;
	int tilt;
	int zoomH;
	int zoomV;
	int scale;
	int innerRadius;
	float roll;
	float pitch;
	float yaw;
	eGdcPerspFunc perspFunc;
	float perspectiveProjMat[9];
	int birdsImg_width;
	int birdsImg_height;
	float mountHeight;
	float roiDist_ahead;
	float roiDist_left;
	float roiDist_right;
	float roiDist_bottom;

	int peaking_en;
	int peaking_clamp;
	int peak_m;
	int th_strong_edge;
	int peak_weights_strength;
} sGdcParam;

#define REGIONLINK_REG_TBL_LENGTH 21
typedef struct sRegionLinkParamDynamic{
	unsigned short mad_cmp_th[3];					 // range[0~4095]
	unsigned short tex_mtb_max_val; 				 // range[0~4095]
	unsigned short motion_mtb_max_val;				 // range[0~4095]
	unsigned short motion_recover_berfore_i_max_val; // range[0~4095]
	unsigned short motion_recover_after_i_max_val;	 // range[0~4095]

	unsigned short tex_mtb_curve[REGIONLINK_REG_TBL_LENGTH];	// range[0~4095]
	unsigned short motion_mtb_curve[REGIONLINK_REG_TBL_LENGTH]; // range[0~4095]
} sRegionLinkParamDynamic;

typedef struct sRegionLinkParamStatic{
	unsigned char region_link_en;
	unsigned char tex_detect_en;
	unsigned char motion_detect_en;
	unsigned char regionLinkAdaptParamEn;
	unsigned char motion_hor_dir_expand_en;
	unsigned char motion_ver_dir_expand_en;
	unsigned char motion_dir_expand_adp_en;
	unsigned char motion_soft_th_en;
	unsigned char motion_recover_en;
	unsigned char motion_recover_grad_en;
	unsigned char temporal_w_i_en;
	unsigned char temporal_w_p_en;
	unsigned char tex_para_select_mode; 	   // 0:default, 1:define
	unsigned char motion_para_select_mode;	   // 0:default, 1:define
	unsigned char tex_ratio_th[2];			   // range[0~100]
	unsigned char motion_hor_l_expand_num;	   // range[0~7]
	unsigned char motion_hor_r_expand_num;	   // range[0~7]
	unsigned char motion_ver_u_expand_num;	   // range[0~7]
	unsigned char motion_ver_d_expand_num;	   // range[0~7]
	unsigned char motion_soft_ratio_th[2];	   // range[0~100], [0]: motion soft low th, [1]: motion soft up th
	unsigned char motion_recover_speed; 	   // range[0~50]
	unsigned char motion_recover_before_i_num; // range[0~idrPeroid-1], idrPeroid-1 < 255
	unsigned char motion_recover_after_i_num;  // range[0~idrPeroid-1], idrPeroid-1 < 255
	unsigned char motion_recover_grad_ratio;   // range[0~100]
	unsigned short texture_weight[3];		   // range[0~256], [0]: flat, [1]: normal, [2]: complex
	unsigned short cur_frame_ratio; 		   // range[0~256]
	unsigned short motion_largemv_th;
} sRegionLinkParamStatic;

typedef struct sRegionLinkParam{
	sRegionLinkParamDynamic mDynamicParam;
	sRegionLinkParamStatic	mStaticParam;
} sRegionLinkParam;

typedef enum VENC_CODING_MODE {
	VENC_FRAME_CODING = 0,
	VENC_FIELD_CODING = 1,
} VENC_CODING_MODE;

typedef enum VENC_CODEC_TYPE {
	VENC_CODEC_H264,
	VENC_CODEC_JPEG,
	VENC_CODEC_H264_VER2,
	VENC_CODEC_H265,
	VENC_CODEC_VP8,
} VENC_CODEC_TYPE;

typedef enum VENC_PIXEL_FMT {
	VENC_PIXEL_YUV420SP,
	VENC_PIXEL_YVU420SP,
	VENC_PIXEL_YUV420P,
	VENC_PIXEL_YVU420P,
	VENC_PIXEL_YUV422SP,
	VENC_PIXEL_YVU422SP,
	VENC_PIXEL_YUV422P,
	VENC_PIXEL_YVU422P,
	VENC_PIXEL_YUYV422,
	VENC_PIXEL_UYVY422,
	VENC_PIXEL_YVYU422, //10
	VENC_PIXEL_VYUY422,
	VENC_PIXEL_ARGB,
	VENC_PIXEL_RGBA,
	VENC_PIXEL_ABGR,
	VENC_PIXEL_BGRA,
	VENC_PIXEL_TILE_32X32,
	VENC_PIXEL_TILE_128X32,
	VENC_PIXEL_AFBC_AW,
	VENC_PIXEL_LBC_AW, //* for v5v200 and newer ic //19
} VENC_PIXEL_FMT;

typedef enum VENC_OUTPUT_FMT {
    VENC_OUTPUT_SAME_AS_INPUT = 0,
    VENC_OUTPUT_YUV420,
    VENC_OUTPUT_YUV444,
    VENC_OUTPUT_YUV422,
} VENC_OUTPUT_FMT;

/**
 * H264 profile types
 */
typedef enum VENC_H264PROFILETYPE {
	VENC_H264ProfileBaseline = 66, /**< Baseline profile */
	VENC_H264ProfileMain     = 77, /**< Main profile */
	VENC_H264ProfileHigh     = 100, /**< High profile */
} VENC_H264PROFILETYPE;

/**
 * H264 level types
 */
typedef enum VENC_H264LEVELTYPE {
	VENC_H264Level1       = 10, /**< Level 1 */
	VENC_H264Level11      = 11, /**< Level 1.1 */
	VENC_H264Level12      = 12, /**< Level 1.2 */
	VENC_H264Level13      = 13, /**< Level 1.3 */
	VENC_H264Level2       = 20, /**< Level 2 */
	VENC_H264Level21      = 21, /**< Level 2.1 */
	VENC_H264Level22      = 22, /**< Level 2.2 */
	VENC_H264Level3       = 30, /**< Level 3 */
	VENC_H264Level31      = 31, /**< Level 3.1 */
	VENC_H264Level32      = 32, /**< Level 3.2 */
	VENC_H264Level4       = 40, /**< Level 4 */
	VENC_H264Level41      = 41, /**< Level 4.1 */
	VENC_H264Level42      = 42, /**< Level 4.2 */
	VENC_H264Level5       = 50, /**< Level 5 */
	VENC_H264Level51      = 51, /**< Level 5.1 */
	VENC_H264Level52      = 52, /**< Level 5.2 */
	VENC_H264LevelDefault = 0
} VENC_H264LEVELTYPE;

typedef struct VencH264ProfileLevel {
	VENC_H264PROFILETYPE nProfile;
	VENC_H264LEVELTYPE nLevel;
} VencH264ProfileLevel;

typedef struct VencQPRange {
	int nMaxqp;
	int nMinqp;
	int nMaxPqp;
	int nMinPqp;
	int nQpInit;
	int bEnMbQpLimit;
} VencQPRange;

/* support 4 ROI region */
typedef struct VencROIConfig {
	int bEnable;
	int index; /* (0~3) */
	int nQPoffset;
	unsigned char roi_abs_flag;
	VencRect sRect;
} VencROIConfig;

typedef struct VencFixQP {
	int bEnable;
	int nIQp;
	int nPQp;
} VencFixQP;

typedef enum {
	AW_CBR   = 0,
	AW_VBR   = 1,
	AW_AVBR  = 2,
	AW_QPMAP = 3,
	AW_FIXQP = 4,
} VENC_RC_MODE;

typedef enum VENC_VIDEO_GOP_MODE {
	AW_NORMALP	= 1, //one p ref frame
	AW_ADVANCE_SINGLE = 2,
	AW_DOUBLEP	= 3,
	AW_SPECIAL_DOUBLE = 4,
	AW_SPECIAL_SMARTP = 5, //double p ref frames and use virtual i frame,but virtual i ref virtual i
	AW_SMARTP	 = 6, //double p ref frames and use virtual i frame
} VENC_VIDEO_GOP_MODE;

typedef enum {
     VENC_H264_NALU_PSLICE = 1,                         /*PSLICE types*/
     VENC_H264_NALU_ISLICE = 5,                         /*ISLICE types*/
     VENC_H264_NALU_SEI    = 6,                         /*SEI types*/
     VENC_H264_NALU_SPS    = 7,                         /*SPS types*/
     VENC_H264_NALU_PPS    = 8,                         /*PPS types*/
     VENC_H264_NALU_IPSLICE = 9,
     VENC_H264_NALU_SVC    = 14,                        /*SVC Prefix types*/
} VENC_H264_NALU_TYPE;

typedef enum {
	 VENC_H265_NALU_PSLICE = 1,                         /*P SLICE types*/
	 VENC_H265_NALU_ISLICE = 19,                        /*I SLICE types*/
     VENC_H265_NALU_VPS    = 32,                        /*VPS types*/
     VENC_H265_NALU_SPS    = 33,                        /*SPS types*/
     VENC_H265_NALU_PPS    = 34,                        /*PPS types*/
     VENC_H265_NALU_SEI    = 39,                        /*SEI types*/
} VENC_H265_NALU_TYPE;

typedef enum {
     VENC_JPEG_PACK_ECS = 5,                            /*ECS types*/
     VENC_JPEG_PACK_APP = 6,                            /*APP types*/
     VENC_JPEG_PACK_VDO = 7,                            /*VDO types*/
     VENC_JPEG_PACK_PIC = 8,                            /*PIC types*/
} VENC_JPEGE_PACK_TYPE;

typedef union {
    VENC_H264_NALU_TYPE    eH264Type;               /*H264E NALU types*/
    VENC_JPEGE_PACK_TYPE   eJpegType;               /*JPEGE pack types*/
    VENC_H265_NALU_TYPE    eH265Type;               /*H264E NALU types*/
} VENC_OUTPUT_PACK_TYPE;

typedef struct {
    VENC_OUTPUT_PACK_TYPE  uType;
    unsigned int           nOffset;
    unsigned int           nLength;
} VencPackInfo;

#define MAX_OUTPUT_PACK_NUM (8)

typedef enum VENC_COLOR_SPACE {
	RESERVED0       = 0,
	VENC_BT709      = 1, // include BT709-5, BT1361, IEC61966-2-1 IEC61966-2-4
	RESERVED1       = 2,
	RESERVED2       = 3,
	VENC_BT470      = 4, // include BT470-6_SystemM,
	VENC_BT601      = 5, // include BT470-6_SystemB, BT601-6_625, BT1358_625, BT1700_625
	VENC_SMPTE_170M = 6, // include SMPTE_170M, BT601-6_525, BT1358_525, BT1700_NTSC
	VENC_SMPTE_240M = 7, // include SMPTE_240M
	VENC_YCC	= 8, // include GENERIC_FILM
	VENC_BT2020     = 9, // include BT2020
	VENC_ST_428     = 10, // include SMPTE_428-1
} VENC_COLOR_SPACE;

typedef enum VENC_VIDEO_FORMAT {
	COMPONENT = 0,
	PAL       = 1,
	NTSC      = 2,
	SECAM     = 3,
	MAC       = 4,
	DEFAULT   = 5,
} VENC_VIDEO_FORMAT;

typedef struct VencJpegVideoSignal {
	VENC_COLOR_SPACE src_colour_primaries;
	VENC_COLOR_SPACE dst_colour_primaries;
} VencJpegVideoSignal;

typedef struct VencH264VideoSignal {
	VENC_VIDEO_FORMAT video_format;

	unsigned char full_range_flag;

	VENC_COLOR_SPACE src_colour_primaries;
	VENC_COLOR_SPACE dst_colour_primaries;
} VencH264VideoSignal;

typedef struct VencH264VideoTiming {
	unsigned long num_units_in_tick;
	unsigned long time_scale;
	unsigned int fixed_frame_rate_flag;

} VencH264VideoTiming;

typedef struct VencCyclicIntraRefresh {
	int bEnable;
	int nBlockNumber;
} VencCyclicIntraRefresh;

typedef struct VencAdvancedRefParam {
	unsigned char bAdvancedRefEn; //advanced ref frame mode, 0:not use , 1:use
	unsigned int nBase; //base frame num
	unsigned int nEnhance; //enhance frame num
	unsigned char bRefBaseEn; //ctrl base frame ref base frame, 0:enable, 1:disable
} VencAdvancedRefParam;

typedef struct VencGopParam {
	unsigned char bUseGopCtrlEn; //use user set gop mode
	VENC_VIDEO_GOP_MODE eGopMode; //gop mode
	unsigned int nVirtualIFrameInterval;
	unsigned int nSpInterval; //user set special p frame ref interval
	VencAdvancedRefParam sRefParam; //user set advanced ref frame mode
} VencGopParam;

typedef enum VENC_SUPERFRAME_MODE {
	VENC_SUPERFRAME_NONE,
	VENC_SUPERFRAME_DISCARD,
	VENC_SUPERFRAME_REENCODE,
} VENC_SUPERFRAME_MODE;

typedef enum VENC_SUPERFRAME_PRIORITY {
	VENC_SUPERF_RPQ_BALANCE = 0,
	VENC_SUPERF_BIT_RATE_FIRST,
	VENC_SUPERF_FRAME_BITS_FIRST,
	VENC_SUPERFE_BUTT,
} VENC_SUPERFRAME_PRIORITY;

typedef struct VencSuperFrameConfig {
	VENC_SUPERFRAME_MODE    eSuperFrameMode;
	unsigned int            nMaxIFrameBits;
	unsigned int            nMaxPFrameBits;
	unsigned int            nMaxBFrameBits;
	VENC_SUPERFRAME_PRIORITY eSuperPriority;
	int                     nMaxRencodeTimes;
	float                   nMaxP2IFrameBitsRatio;
} VencSuperFrameConfig;

typedef struct VencBitRateRange {
	int            bitRateMax;
	int            bitRateMin;
	float          fRangeRatioTh;
	int            nQualityTh;
	int            nQualityStep[2];
	int            nSubQualityDelay;
	int            nAddQualityDelay;
	int            nMinQuality;
	int            nMaxQuality;
} VencBitRateRange;

typedef struct VencH265TimingS {
	//0:stream without timing info; 1:stream with timing info
	unsigned char timing_info_present_flag;
	unsigned int num_units_in_tick; //time_scale/frameRate
	unsigned int time_scale; //1second is  average divided by time_scale
	unsigned int num_ticks_poc_diff_one; //num ticks of diff frame
} VencH265TimingS;

typedef enum VENC_OVERLAY_ARGB_TYPE {
	VENC_OVERLAY_ARGB_MIN = -1,
	VENC_OVERLAY_ARGB8888 = 0,
	VENC_OVERLAY_ARGB4444 = 1,
	VENC_OVERLAY_ARGB1555 = 2,
	VENC_OVERLAY_ARGB_MAX = 3,
} VENC_OVERLAY_ARGB_TYPE;

typedef enum VENC_OVERLAY_TYPE {
	NORMAL_OVERLAY       = 0, //normal overlay
	COVER_OVERLAY	= 1, //use the setting yuv to cover region
	LUMA_REVERSE_OVERLAY = 2, //normal overlay and luma reverse
} VENC_OVERLAY_TYPE;

typedef struct VencOverlayCoverYuvS {
	unsigned char use_cover_yuv_flag; //1:use the cover yuv; 0:transform the argb data to yuv ,then cover
	unsigned char cover_y; //the value of cover y
	unsigned char cover_u; //the value of cover u
	unsigned char cover_v; //the value of cover v
} VencOverlayCoverYuvS;

typedef struct VencH264ConstraintFlag {
	unsigned char                 reserve_zero : 2;
	unsigned char                 constraint_5 : 1;
	unsigned char                 constraint_4 : 1;
	unsigned char                 constraint_3 : 1;
	unsigned char                 constraint_2 : 1;
	unsigned char                 constraint_1 : 1;
	unsigned char                 constraint_0 : 1;
} VencH264ConstraintFlag;

typedef struct {
    unsigned int                  en_force_conf;
    unsigned int                  left_offset;
    unsigned int                  right_offset;
    unsigned int                  top_offset;
    unsigned int                  bottom_offset;
} VencForceConfWin;

typedef struct {
    unsigned char *pBuffer;
    unsigned int  nBufLen;
    unsigned int  nDataLen;
    unsigned int  nType;
} VencSeiData;

typedef struct {
    unsigned int nSeiNum;
    VencSeiData  *pSeiData;
} VencSeiParam;

typedef enum {
    BUF_IDLE,
    BUF_STANDBY,
    BUF_OCCUPY,
} VENC_BUF_STATUS;

typedef struct {
    unsigned char *pBuffer;
    unsigned int  nBufLen;
    unsigned int  nDataLen;
    unsigned int  nFrameRate;
    VENC_BUF_STATUS eStatus;
} VencInsertData;

typedef enum {
    PRODUCT_STATIC_IPC = 0,
    PRODUCT_MOVING_IPC = 1,
    PRODUCT_DOORBELL = 2,
    PRODUCT_CDR = 3,
    PRODUCT_SDV = 4,
    PRODUCT_PROJECTION = 5,
    PRODUCT_UAV = 6,       // Unmanned Aerial Vehicle
    PRODUCT_NUM,
} eVencProductMode;

#define FACTOR_MAX_NUM 8

typedef enum VENC_RC_PRIORITY {
	VENC_RC_RPQ_BALANCE_Q = 0,       // balance bit rate and quality, slightly more focused on quality, default value
	VENC_RC_RPQ_BALANCE_R,           // balance bit rate and quality, slightly more focused on bit rate
	VENC_RC_AVERAGE_BIT_RATE_FIRST,  // Average bitrate is guaranteed first
	VENC_RC_RT_BIT_RATE_FIRST,       // Instantaneous bitrate is guaranteed first
	VENC_RC_BUTT,
} VENC_RC_PRIORITY;

typedef enum VENC_QUALITY_LEVEL {
	VENC_QUALITY_LOW_LEVEL = 0,     // use with VENC_RC_PRIORITY, low peak bitrate and quality, suitable for low bit rate application, default value
	VENC_QUALITY_MIDDLE_LEVEL,      // use with VENC_RC_PRIORITY, middle peak bitrate and quality
	VENC_QUALITY_HIGH_LEVEL,        // use with VENC_RC_PRIORITY, high peak bitrate and quality
	VENC_QUALITY_LEVEL_NUM,
} VENC_QUALITY_LEVEL;

typedef struct InsideClipQpFunc {
	unsigned char closeMbRcEn;       // range[0, 1], when closeMbRcEn = 1, the best quality, default value 1
	unsigned char MoveStatusTh;      // range[0, 4], motion status threshold, use default
	unsigned int  MbQpTightLimitEn;  // range[0, 1], enable macroblock qp narrowing adjustment amplitude, when MbQpTightLimitEn = 1, the best quality, default value 1
	int 		  uAddDeltaQp;       // range[0, 10], limit classify adjustment qp amplitude, the large the value, the best the image quality, use default
	unsigned int  uSliceQpMaxTh;     // range[minqp, maxqp], the smaller the value, the best the image quality, use default
} InsideClipQpFunc;

typedef struct InsideInstaneousBitRatePar {
	int 		  nFactorLevelNum;                  // range[1, 8], the number of adjustment intervals, use default
	float		  ExceedTarBrRatio;                 // range[0.1f, 3.0f], exceed max target bitrate*ExceedTarBrRatio need to save bits, use default
	float		  SmallTarBrRatio;                  // range[0.1f, 3.0f], increase bits if the intantenus bitrate is less than target bitrate*smallTarBrRatio, use default
	float		  recodeExceedTarBrRatio;           // range[0.1f, 3.0f], the smaller the value, the best the image quality, use default
	float		  fgAdjustFactorTh[FACTOR_MAX_NUM]; // range[0.01f, 3.0f], InstaneousBitRate exceed ratio, use default
	float		  fgAdjustFactor[FACTOR_MAX_NUM];   // range[0.01f, 1.0f], target bit adjust ratio, use default
} InsideInstaneousBitRatePar;

typedef struct InsideVbrKeyPar {
	unsigned int		uVbrOptEn;                // range[0, 1], follow the vbr(New) enable, internal automatic setting, use default
	unsigned int		uPrintVbrTraceInfoEn;     // range[0, 1], print vbr trace info enable, default value 0, use default
	unsigned int		uPrintLogInfoEn;          // range[0, 1], print vbr log enable, default value 0, use default
	unsigned int		PrintLogLevel;            // range[0, 1], print vbr log level, default value 0, use default
	int 				PrintRegInfoFrameNum;     // range[0~], print register value of a certain frame, use default
	int 				nIFrameRcInfoNum;         // range[1, 16], number of saved I-frame historical vbr info, use default
	int 				nPFrameRcInfoNum;         // range[1, 16], number of saved P-frame historical vbr info, use default
	int 				nSaveBitQpTh;             // range[minqp, maxqp], no more need to save bit allocation threshold, use default
	int 				nBoostBitQpTh;            // range[minqp, maxqp], no longer increase bit number allocation, use defalut
	int 				nBppLevelNum;             // range[1, 8], total number of bpp level, use default
	int 				nIPSliceQpMaxGap;         // range[1, 15], maximun interval of P-I frame sliceQp, the large the value, the worse the image quality stability
	int 				nMvStaLevelNum;           // range[1, 7], number of integer pixel mv statistical intervals, use default
	int 				nStaMvTh[FACTOR_MAX_NUM]; // range[1, 128], thresholds for each interval of integer pixel mv, use default
	int 				nAvgMvFactor;             // range[1,2^N], N<=10, mv mean precision, use default
	int 				nAvgQpRatio;              // range[0, 16], avgqp mean weight coefficient, use default
	int 				nDeltaQp;                 // range[0, 10], the large the value, the worse the image quality stability, use default
	int 				nMaxHistoryFrameNum[2];   // range[1, maxKeyI - 1], use default
	float				fInitAvgMovingLevel;      // range[0.0f, 0.5f], initial move level, the larger the value, the stronger the move, use defalut
	float				fAvgMoveLevelRatio;       // range[0.0f, 1.0f], AvgMoveLevel mean weight coefficient, use default
	float				fMinRatio[2];             // range[0.0f, 3.0f], fMinRatio[0]:use for I slice,fMinRatio[1]:use for P slice, use default
	float				fMaxRatio[2];             // range[0.0f, 3.0f], fMaxRatio[0]:use for I slice,fMaxRatio[1]:use for P slice, use default
	float				fMinRaioTh[2];            // range[0.0f, 3.0f], fMinRaioTh[0]:use for I slice,fMinRaioTh[1]:use for P slice, use default
	float				fMaxRatioTh[2];           // range[0.0f, 3.0f], fMaxRatioTh[0]:use for I slice,fMaxRatioTh[1]:use for P slice, use default
	float				fBppLevel[2][FACTOR_MAX_NUM];       // range[0.0f, 256.0f], bpp level threshold, use default
	float				fAdjustQpBppTh[2][FACTOR_MAX_NUM];  // range[0.0f, 1.0f], bpp threshold used for adjust sliceqp, use default
	float				fAdjustMaxQp[2][FACTOR_MAX_NUM];    // range[0.0f, 7.0f], adjust maximun of delta qp based on bpp level, use default
	float				fMoveFrameLevelTh;                  // range[0.0f, 0.5f], large motion scene threshold, use default
	float				fStaticFrameLevelTh;                // range[0.0f, 0.5f], static scene threshold, use default
	float				fluctuationFactorTh;                // range[1, 30], the large the value, the worse the image quality stability, default value 10
	unsigned char		uIframeMadThOpt[12];                // range[0, 255], adjust qp based on mad (I slice), ternal automatic setting, use default
	unsigned char		uPframeMadThOpt[12];                // range[0, 255], adjust qp based on mad (P slice), ternal automatic setting, use default
} InsideVbrKeyPar;

typedef struct InsideVbrMosAicOptPar {
	//mosaic opt param
	unsigned int		 uConstantLargeMoveNumTh;   // range[2, maxKeyI - 1], continuous motion frame number threshold, use default
	unsigned int		 uConstantStaticNumTh;      // range[1, maxKeyI - 1], continuous static frame number threshold, use default
	unsigned int		 uMethodSelect;             // range[0, 3], default value 1
													// 0:constand move frame adjust sliceqp;
													// 1:constand move frame and MoveToStatic frame adjust slice qp
													// 2:adjust target bits
													// 3:no opeartion
	unsigned int		 uAdjustSliceQpPeriod;      // range[1, maxKeyI - 1], slice Qp adjust period, use default
	unsigned int		 uSliceQpLimitTh;           // range[minqp, maxqp], adjusrt sliceQp upper limit threshold, use default
	int 				 nMosAicSliceDeltaQp;       // range[-10, 10], sliceQp adjustment range, the smaller the value, the bset the image quality, default value -6
	float				 nScaleDeltaRatio;          // range[0.0f, 3.0f], scale ratio for bit, use default
} InsideVbrMosAicOptPar;

typedef struct InsideVbrMoveToStaticPar {
	unsigned int		 uConstandMoveToStaticNumTh;                // range[1, maxKeyI - 1], not use
	int 				 nMoveToStaticSliceQpDelta[FACTOR_MAX_NUM]; // range[-10,10], adjustment range of sliceQp from different motion level to static scene, use default
																	// the smaller the value, the bset the image quality
	int 				 nMoveToStaticDelayNum[4];                  // range[0, maxKeyI - 1], adjust the sliceQp period frame number for different motion level to statis scene, use default
	float				 fFrameMvLevelTh[FACTOR_MAX_NUM];           // range[0.0f, 1.0f], motion level judgment threshold, use default
} InsideVbrMoveToStaticPar;

typedef struct InsideVbrOptPar {
	InsideVbrKeyPar 		   vbrKeyPar;
	InsideVbrMosAicOptPar	   vbrMosAicOptPar;
	InsideVbrMoveToStaticPar   vbrMoveToStaticPar;
	InsideInstaneousBitRatePar vbrInstaneousBRPar;
	InsideClipQpFunc		   mclipQpOpt;
} InsideVbrOptPar;

typedef struct VencVbrOptParam {
	int 				 nIPratio;               // range[10, 100], the larger the value of nIPratio, the larger the size of IDR, use default
	int 				 nIntraPeriodNumInVbv;   // range[1, 10], use default
	unsigned char		 enable_instaneousBR;    // range[0, 1], Instantaneous rate control strategy enabled
	unsigned int	     max_instaneousBR;       // range[104857, 104857600], unit bps
	unsigned int		 peroid_instaneousBR;    // range[20, 120], as an integer multiple of tha frame rate, use default
	unsigned char		 recodeIsliceQpEn;       // range[0, 1], modify I slice Qp enable
	unsigned int		 uMosAicOptEn;           // range[0, 1], mosaic opt enable
	unsigned int		 uSaveBitRateEn;         // range[0, 1], save bit rate enable
	unsigned int		 uClassifyMadThAdjustEn; // range[0, 1], modify mad threshold enable, use default
	unsigned int		 uMoveToStaticOptEn;     // range[0, 1], motion to still video quality improve enable
	InsideVbrOptPar 	 pVbrOptPar;
	VENC_RC_PRIORITY     sRcPriority;            // range[0, 3], rate control priority
	VENC_QUALITY_LEVEL   eQualityLevel;          // range[0, 2], quality level, the larger the value, the best the image quality
} VencVbrOptParam;

typedef enum {
	VENC_H265ProfileMain      = 1,
	VENC_H265ProfileMain10    = 2,
	VENC_H265ProfileMainStill = 3
} VENC_H265PROFILETYPE;

typedef enum {
	VENC_H265Level1       = 30, /**< Level 1 */
	VENC_H265Level2       = 60, /**< Level 2 */
	VENC_H265Level21      = 63, /**< Level 2.1 */
	VENC_H265Level3       = 90, /**< Level 3 */
	VENC_H265Level31      = 93, /**< Level 3.1 */
	VENC_H265Level4       = 120, /**< Level 4 */
	VENC_H265Level41      = 123, /**< Level 4.1 */
	VENC_H265Level5       = 150, /**< Level 5 */
	VENC_H265Level51      = 153, /**< Level 5.1 */
	VENC_H265Level52      = 156, /**< Level 5.2 */
	VENC_H265Level6       = 180, /**< Level 6 */
	VENC_H265Level61      = 183, /**< Level 6.1 */
	VENC_H265Level62      = 186, /**< Level 6.2 */
	VENC_H265LevelDefault = 0
} VENC_H265LEVELTYPE;

typedef enum {
	VENC_ST_DIS_WDR = 0,
	VENC_ST_EN_WDR  = 1,
	VENC_ST_NONE
} eSensorType;

typedef struct {
	VENC_H265PROFILETYPE nProfile;
	VENC_H265LEVELTYPE nLevel;
} VencH265ProfileLevel;

typedef struct {
	unsigned int uMaxBitRate;
	unsigned int nMovingTh; //range[1,31], 1:all frames are moving,
	//            31:have no moving frame, default: 20
	int nQuality; //range[1,20], 1:worst quality, 20:best quality
	int nIFrmBitsCoef; //range[1, 20], 1:worst quality, 20:best quality
	int nPFrmBitsCoef; //range[1, 50], 1:worst quality, 50:best quality
} VencVbrParam;

typedef struct {
	unsigned char mode_ctrl_en;
	unsigned char *p_map_info;
} VencMBModeCtrl;

typedef struct {
	VENC_RC_MODE eRcMode;
	unsigned char bLowBitrateBeginFlag;
	unsigned char bUseSetMadThrdFlag;
	unsigned char uMadThrdI[12]; //range 0-255
	unsigned char uMadThrdP[12]; //range 0-255
	unsigned char uMadThrdB[12]; //no support
	unsigned int uStatTime; //range [1,10], default:1
	unsigned int uMinIQp;
	int nMaxReEncodeTimes; //default use one time

	VencVbrParam sVbrParam; //valid only at AW_VBR/AW_AVBR
	VencFixQP sFixQp; //valid only at AW_FIXQP
	VencMBModeCtrl sQpMap; //valid only at AW_QPMAP

	unsigned int uRowQpDelta; //no support
	unsigned int uDirectionThrd; //no support
	unsigned int uQpDeltaLevelI; //no support
	unsigned int uQpDeltaLevelP; //no support
	unsigned int uQpDeltaLevelB; //no support
	unsigned int uInputFrmRate; //no support
	unsigned int uOutputFrmRate; //no support
	unsigned int uFluctuateLevel; //no support
	unsigned int uMinIprop; //no support
	unsigned int uMaxIprop; //no support
} VencRcParam;

typedef struct VencH264Param {
	VencH264ProfileLevel sProfileLevel;
	int bEntropyCodingCABAC; /* 0:CAVLC 1:CABAC*/
	VencQPRange sQPRange;
	int nFramerate; /* fps*/
	int nSrcFramerate; /* fps*/
	int nBitrate; /* bps*/
	int nMaxKeyInterval;
	VENC_CODING_MODE nCodingMode;
	VencGopParam sGopParam;
	VencRcParam sRcParam;
} VencH264Param;

typedef struct {
	int idr_period;
	VencH265ProfileLevel sProfileLevel;
	VencQPRange sQPRange;
	int nFramerate; /* fps*/
	int nSrcFramerate; /* fps*/
	int nBitrate; /* bps*/
	int nIntraPeriod;
	int nGopSize;
	int nQPInit; /* qp of first IDR_frame if use rate control */
	VencRcParam sRcParam;
	VencGopParam sGopParam;
} VencH265Param;

typedef struct {
	// all these average value is for mb16x16
	unsigned int avg_mad;
	unsigned int avg_md;
	unsigned int avg_sse;
	unsigned int avg_qp;
	double avg_psnr;
	unsigned char *p_mb_mad_qp_sse;
	unsigned char *p_mb_bin_img;
	unsigned char *p_mb_mv;
} VencMBSumInfo;

typedef struct {
    int pix_x_bgn;
    int pix_x_end;
    int pix_y_bgn;
    int pix_y_end;
    int total_num;
    int intra_num;
    int large_mv_num;
    int small_mv_num;
    int zero_mv_num;
    int large_sad_num;
    int is_motion;
} VencMotionSearchRegion;

typedef struct {
    int total_region_num;
    int motion_region_num;
    VencMotionSearchRegion *region;
} VencMotionSearchResult;

typedef struct {
    int en_motion_search;
    int dis_default_para;
    int hor_region_num;
    int ver_region_num;
    int large_mv_th;
    float large_mv_ratio_th;    // include intra and large mv
    float non_zero_mv_ratio_th; // include intra, large mv and samll mv
    float large_sad_ratio_th;
} VencMotionSearchParam;

typedef void *VideoEncoder;

typedef struct {
	int is_static;
	VideoEncoder *pVideoEnc;
	VENC_CODEC_TYPE codecType;
	int parsingStatic;
	int parsingDynamic;
	int channnel_id;
	unsigned int		nInputWidth;
	unsigned int		nInputHeight;
	unsigned int		nDstWidth;
	unsigned int		nDstHeight;
	VENC_PIXEL_FMT		eInputFormat;
	VENC_OUTPUT_FMT 	eOutputFormat;
	unsigned int		fps;
	VencQPRange qp_range;
	int bit_rate;
	VencH264Param		h264Param;
	unsigned int vbv_size;
	VencForceConfWin conf_win;
	int jpeg_quality;
	VencFixQP			fixQP;
	VencTargetBitsClipParam bits_clip_param;
	VencVbrParam			sVbrParam;
	s2DfilterParam m2DfilterParam;
	s3DfilterParam m3DfilterParam;
	VencRegionD3DParam mRegionD3DParam;
	int mb_rc_level;
	float		 weak_text_th;
	VencH264VideoTiming mH264VideoTiming;
	VencH265TimingS mH265VideoTiming;
} VencParamFromFiles;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* _VENCODER_BASE_H_ */

