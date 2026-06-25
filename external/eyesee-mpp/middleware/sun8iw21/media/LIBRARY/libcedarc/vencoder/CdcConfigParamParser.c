#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "CdcConfigParamParser.h"
#include "cdc_log.h"

#define BUF_SIZE 1024

extern void ve_param_set_res(VideoEncoder* pVideoEnc, VencParamFromFiles *p_ve_param);
extern void ve_param_set_pxl_format(VideoEncoder* pVideoEnc, VencParamFromFiles *p_ve_param);

const static struct {
    int        val;
    const char *str;
} VENC_FMT_CONVERSION [] = {
   {0,  "VENC_PIXEL_YUV420SP"   },
   {1,  "VENC_PIXEL_YVU420SP"   },
   {2,  "VENC_PIXEL_YUV420P"    },
   {3,  "VENC_PIXEL_YVU420P"    },
   {4,  "VENC_PIXEL_YUV422SP"   },
   {5,  "VENC_PIXEL_YVU422SP"   },
   {6,  "VENC_PIXEL_YUV422P"    },
   {7,  "VENC_PIXEL_YVU422P"    },
   {8,  "VENC_PIXEL_YUYV422"    },
   {9,  "VENC_PIXEL_UYVY422"    },
   {10, "VENC_PIXEL_YVYU422"    },
   {11, "VENC_PIXEL_VYUY422"    },
   {12, "VENC_PIXEL_ARGB"       },
   {13, "VENC_PIXEL_RGBA"       },
   {14, "VENC_PIXEL_ABGR"       },
   {15, "VENC_PIXEL_BGRA"       },
   {16, "VENC_PIXEL_TILE_32X32" },
   {17, "VENC_PIXEL_TILE_128X32"},
   {18, "VENC_PIXEL_AFBC_AW"    },
   {19, "VENC_PIXEL_LBC_AW"     },
};

static int str2enum (const char *str)
{
     for (unsigned j = 0;  j < sizeof (VENC_FMT_CONVERSION)/sizeof (VENC_FMT_CONVERSION[0]);++j)
         if (!strcmp (str, VENC_FMT_CONVERSION[j].str))
             return VENC_FMT_CONVERSION[j].val;
     logv("no such enum ofr VENC_PIXEL_FMT");
     return -1;
}


static int parse_frame_rate_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "value") == 0)
    {
        ve_param->fps = strtol(attr[i + 1], NULL, 0);
        logd("frame_rate: %d", ve_param->fps);
    }
	return 0;
}

static int parse_bit_rate_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "value") == 0)
    {
        ve_param->bit_rate = strtol(attr[i + 1], NULL, 0);
        logd("bit_rate: %d", ve_param->bit_rate);
    }
	return 0;
}

static int parse_vbv_size_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "value") == 0)
    {
        ve_param->vbv_size = strtol(attr[i + 1], NULL, 0);
        logd("vbv_size: %d", ve_param->vbv_size);
    }
	return 0;
}

static int parse_pixel_format_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "pxl_format") == 0)
    {
        ve_param->eInputFormat = str2enum(attr[i + 1]);
		logd("pxl_format: %d", ve_param->eInputFormat);
	}
    else if (strcmp(attr[i], "out_pxl_format") == 0)
    {
        ve_param->eOutputFormat = strtol(attr[i + 1], NULL, 0);
        logd("out_pxl_format: %d", ve_param->eOutputFormat);
    }
	return 0;
}

static int parse_resolution_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "src_width") == 0)
    {
        ve_param->nInputWidth = strtol(attr[i + 1], NULL, 0);
        logd("src_width: %d", ve_param->nInputWidth);
    }
    else if (strcmp(attr[i], "src_height") == 0)
    {
        ve_param->nInputHeight = strtol(attr[i + 1], NULL, 0);
        logd("src_height: %d", ve_param->nInputHeight);
    }
    else if (strcmp(attr[i], "dst_width") == 0) {
        ve_param->nDstWidth = strtol(attr[i + 1], NULL, 0);
        logd("dst_width: %d", ve_param->nDstWidth);
    }
    else if (strcmp(attr[i], "dst_height") == 0) {
        ve_param->nDstHeight = strtol(attr[i + 1], NULL, 0);
        logd("dst_height: %d", ve_param->nDstHeight);
    }
	return 0;
}

static int parse_qp_range_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "i_qp_min") == 0)
    {
        ve_param->qp_range.nMinqp = strtol(attr[i + 1], NULL, 0);
        logd("i_qp_min: %d", ve_param->qp_range.nMinqp);
    }
    else if (strcmp(attr[i], "i_qp_max") == 0)
    {
        ve_param->qp_range.nMaxqp = strtol(attr[i + 1], NULL, 0);
        logd("i_qp_max: %d", ve_param->qp_range.nMaxqp);
    }
    else if (strcmp(attr[i], "p_qp_min") == 0)
    {
        ve_param->qp_range.nMinPqp = strtol(attr[i + 1], NULL, 0);
        logd("p_qp_min: %d", ve_param->qp_range.nMinPqp);
    }
    else if (strcmp(attr[i], "p_qp_max") == 0)
    {
        ve_param->qp_range.nMaxPqp = strtol(attr[i + 1], NULL, 0);
        logd("p_qp_max: %d", ve_param->qp_range.nMaxPqp);
    }
    else if (strcmp(attr[i], "qp_init") == 0)
    {
        ve_param->qp_range.nQpInit = strtol(attr[i + 1], NULL, 0);
        logd("qp_init: %d", ve_param->qp_range.nQpInit);
    }
    else if (strcmp(attr[i], "mb_qp_limit") == 0)
    {
        ve_param->qp_range.bEnMbQpLimit = strtol(attr[i + 1], NULL, 0);
        logd("mb_qp_limit: %d", ve_param->qp_range.bEnMbQpLimit);
    }
	return 0;
}

static int parse_fix_qp_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "u8_en_fix_qp") == 0)
	{
		ve_param->fixQP.bEnable = strtol(attr[i + 1], NULL, 0);
		logd("u8_en_fix_qp: %d", ve_param->fixQP.bEnable);
	}
    else if (strcmp(attr[i], "s32_fix_i_qp") == 0)
	{
		ve_param->fixQP.nIQp = strtol(attr[i + 1], NULL, 0);
		logd("s32_fix_i_qp: %d", ve_param->fixQP.nIQp);
	}
    else if (strcmp(attr[i], "s32_fix_p_qp") == 0)
	{
		ve_param->fixQP.nPQp = strtol(attr[i + 1], NULL, 0);
		logd("s32_fix_p_qp: %d", ve_param->fixQP.nPQp);
	}
	return 0;
}

static int parse_vbr_param_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "s32_quality") == 0)
	{
		ve_param->sVbrParam.nQuality = strtol(attr[i + 1], NULL, 0);
		logd("nQuality: %d", ve_param->sVbrParam.nQuality);
	}
    else if (strcmp(attr[i], "s32_p_bits_coef") == 0)
	{
		ve_param->sVbrParam.nPFrmBitsCoef = strtol(attr[i + 1], NULL, 0);
		logd("nPFrmBitsCoef: %d", ve_param->sVbrParam.nPFrmBitsCoef);
	}
    else if (strcmp(attr[i], "s32_i_bits_coef") == 0)
	{
		ve_param->sVbrParam.nIFrmBitsCoef = strtol(attr[i + 1], NULL, 0);
		logd("nIFrmBitsCoef: %d", ve_param->sVbrParam.nIFrmBitsCoef);
	}
	return 0;
}

static int parse_h264_video_timing_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "u8_timing_info_present_flag") == 0)
	{
		//ve_param->mH264VideoTiming. = strtol(attr[i + 1], NULL, 0);
		//if (ve_param->fps != 0xffff) logd("vbv_size: %d", ve_param->vbv_size);
		//logd("struct not this mem %s: %ld", attr[i], strtol(attr[i + 1], NULL, 0));
	}
    else if (strcmp(attr[i], "u8_fixed_frame_rate_flag") == 0)
	{
		ve_param->mH264VideoTiming.fixed_frame_rate_flag = strtol(attr[i + 1], NULL, 0);
		logd("fixed_frame_rate_flag: %d", ve_param->mH264VideoTiming.fixed_frame_rate_flag);
	}
    else if (strcmp(attr[i], "u32_num_units_in_tick") == 0)
	{
		ve_param->mH264VideoTiming.num_units_in_tick = strtol(attr[i + 1], NULL, 0);
		logd("num_units_in_tick: %ld", ve_param->mH264VideoTiming.num_units_in_tick);
	}
    else if (strcmp(attr[i], "u32_time_scale") == 0)
	{
		ve_param->mH264VideoTiming.time_scale = strtol(attr[i + 1], NULL, 0);
		logd("time_scale: %ld", ve_param->mH264VideoTiming.time_scale);
	}
	return 0;
}
static int parse_h265_video_timing_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "u32_timing_info_present_flag") == 0)
	{
		ve_param->mH265VideoTiming.timing_info_present_flag = strtol(attr[i + 1], NULL, 0);
		logd("timing_info_present_flag: %d", ve_param->mH265VideoTiming.timing_info_present_flag);
	}
    else if (strcmp(attr[i], "u32_h265_num_units_in_tick") == 0)
	{
		ve_param->mH265VideoTiming.num_units_in_tick = strtol(attr[i + 1], NULL, 0);
		logd("u32_h265_num_units_in_tick: %d", ve_param->mH265VideoTiming.num_units_in_tick);
	}
    else if (strcmp(attr[i], "u32_h265_time_scale") == 0)
	{
		ve_param->mH265VideoTiming.time_scale = strtol(attr[i + 1], NULL, 0);
		logd("u32_h265_time_scale: %d", ve_param->mH265VideoTiming.time_scale);
	}
    else if (strcmp(attr[i], "num_ticks_poc_diff_one_minus1") == 0)
	{
		ve_param->mH265VideoTiming.num_ticks_poc_diff_one = strtol(attr[i + 1], NULL, 0);
		logd("num_ticks_poc_diff_one: %d", ve_param->mH265VideoTiming.num_ticks_poc_diff_one);
	}
	return 0;
}

static int parse_region_d3d_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "u8_en_region_d3d") == 0)
	{
		ve_param->mRegionD3DParam.en_region_d3d = strtol(attr[i + 1], NULL, 0);
		logd("en_region_d3d: %d", ve_param->mRegionD3DParam.en_region_d3d);
	}
    else if (strcmp(attr[i], "u8_dis_default_para") == 0)
	{
		ve_param->mRegionD3DParam.dis_default_para = strtol(attr[i + 1], NULL, 0);
		logd("dis_default_para: %d", ve_param->mRegionD3DParam.dis_default_para);
	}
    else if (strcmp(attr[i], "s32_result_num") == 0)
	{
		ve_param->mRegionD3DParam.result_num = strtol(attr[i + 1], NULL, 0);
		logd("result_num: %d", ve_param->mRegionD3DParam.result_num);
	}
    else if (strcmp(attr[i], "s32_hor_region_num") == 0)
	{
		ve_param->mRegionD3DParam.hor_region_num = strtol(attr[i + 1], NULL, 0);
		logd("hor_region_num: %d", ve_param->mRegionD3DParam.hor_region_num);
	}
    else if (strcmp(attr[i], "s32_ver_region_num") == 0)
	{
		ve_param->mRegionD3DParam.ver_region_num = strtol(attr[i + 1], NULL, 0);
		logd("ver_region_num: %d", ve_param->mRegionD3DParam.ver_region_num);
	}
    else if (strcmp(attr[i], "s32_hor_expand_num") == 0)
	{
		ve_param->mRegionD3DParam.hor_expand_num = strtol(attr[i + 1], NULL, 0);
		logd("hor_expand_num: %d", ve_param->mRegionD3DParam.hor_expand_num);
	}
    else if (strcmp(attr[i], "s32_ver_expand_num") == 0)
	{
		ve_param->mRegionD3DParam.ver_expand_num = strtol(attr[i + 1], NULL, 0);
		logd("ver_expand_num: %d", ve_param->mRegionD3DParam.ver_expand_num);
	}
    else if (strcmp(attr[i], "u8_motion_coef_3") == 0)
	{
		ve_param->mRegionD3DParam.motion_coef[3] = strtol(attr[i + 1], NULL, 0);
		logd("motion_coef[3]: %d", ve_param->mRegionD3DParam.motion_coef[3]);
	}
    else if (strcmp(attr[i], "u8_motion_coef_2") == 0)
	{
		ve_param->mRegionD3DParam.motion_coef[2] = strtol(attr[i + 1], NULL, 0);
		logd("motion_coef[2]: %d", ve_param->mRegionD3DParam.motion_coef[2]);
	}
    else if (strcmp(attr[i], "u8_motion_coef_1") == 0)
	{
		ve_param->mRegionD3DParam.motion_coef[1] = strtol(attr[i + 1], NULL, 0);
		logd("motion_coef[1]: %d", ve_param->mRegionD3DParam.motion_coef[1]);
	}
    else if (strcmp(attr[i], "u8_motion_coef_0") == 0)
	{
		ve_param->mRegionD3DParam.motion_coef[0] = strtol(attr[i + 1], NULL, 0);
		logd("motion_coef[0]: %d", ve_param->mRegionD3DParam.motion_coef[0]);
	}
    else if (strcmp(attr[i], "f32_zero_mv_rate_th_2") == 0)
	{
		ve_param->mRegionD3DParam.zero_mv_rate_th[2] = strtof(attr[i + 1], NULL);
		logd("zero_mv_rate_th[2]: %f", ve_param->mRegionD3DParam.zero_mv_rate_th[2]);
	}
    else if (strcmp(attr[i], "f32_zero_mv_rate_th_1") == 0)
	{
		ve_param->mRegionD3DParam.zero_mv_rate_th[1] = strtof(attr[i + 1], NULL);
		logd("zero_mv_rate_th[1]: %f", ve_param->mRegionD3DParam.zero_mv_rate_th[1]);
	}
    else if (strcmp(attr[i], "f32_zero_mv_rate_th_0") == 0)
	{
		ve_param->mRegionD3DParam.zero_mv_rate_th[0] = strtof(attr[i + 1], NULL);
		logd("zero_mv_rate_th[0]: %f", ve_param->mRegionD3DParam.zero_mv_rate_th[0]);
	}
    else if (strcmp(attr[i], "u8_static_coef_2") == 0)
	{
		ve_param->mRegionD3DParam.static_coef[2] = strtol(attr[i + 1], NULL, 0);
		logd("static_coef[2]: %d", ve_param->mRegionD3DParam.static_coef[2]);
	}
    else if (strcmp(attr[i], "u8_static_coef_1") == 0)
	{
		ve_param->mRegionD3DParam.static_coef[1] = strtol(attr[i + 1], NULL, 0);
		logd("static_coef[1]: %d", ve_param->mRegionD3DParam.static_coef[1]);
	}
    else if (strcmp(attr[i], "u8_static_coef_0") == 0)
	{
		ve_param->mRegionD3DParam.static_coef[0] = strtol(attr[i + 1], NULL, 0);
		logd("static_coef[0]: %d", ve_param->mRegionD3DParam.static_coef[0]);
	}
    else if (strcmp(attr[i], "u8_chroma_offset") == 0)
	{
		ve_param->mRegionD3DParam.chroma_offset = strtol(attr[i + 1], NULL, 0);
		logd("chroma_offset: %d", ve_param->mRegionD3DParam.chroma_offset);
	}
    else if (strcmp(attr[i], "s32_lv_weak_th_8") == 0)
	{
		ve_param->mRegionD3DParam.lv_weak_th[8] = strtol(attr[i + 1], NULL, 0);
		logd("lv_weak_th[8]: %d", ve_param->mRegionD3DParam.lv_weak_th[8]);
	}
    else if (strcmp(attr[i], "s32_lv_weak_th_7") == 0)
	{
		ve_param->mRegionD3DParam.lv_weak_th[7] = strtol(attr[i + 1], NULL, 0);
		logd("lv_weak_th[7]: %d", ve_param->mRegionD3DParam.lv_weak_th[7]);
	}
    else if (strcmp(attr[i], "s32_lv_weak_th_6") == 0)
	{
		ve_param->mRegionD3DParam.lv_weak_th[6] = strtol(attr[i + 1], NULL, 0);
		logd("lv_weak_th[6]: %d", ve_param->mRegionD3DParam.lv_weak_th[6]);
	}
    else if (strcmp(attr[i], "s32_lv_weak_th_5") == 0)
	{
		ve_param->mRegionD3DParam.lv_weak_th[5] = strtol(attr[i + 1], NULL, 0);
		logd("lv_weak_th[5]: %d", ve_param->mRegionD3DParam.lv_weak_th[5]);
	}
    else if (strcmp(attr[i], "s32_lv_weak_th_4") == 0)
	{
		ve_param->mRegionD3DParam.lv_weak_th[4] = strtol(attr[i + 1], NULL, 0);
		logd("lv_weak_th[4]: %d", ve_param->mRegionD3DParam.lv_weak_th[4]);
	}
    else if (strcmp(attr[i], "s32_lv_weak_th_3") == 0)
	{
		ve_param->mRegionD3DParam.lv_weak_th[3] = strtol(attr[i + 1], NULL, 0);
		logd("lv_weak_th[3]: %d", ve_param->mRegionD3DParam.lv_weak_th[3]);
	}
    else if (strcmp(attr[i], "s32_lv_weak_th_2") == 0)
	{
		ve_param->mRegionD3DParam.lv_weak_th[2] = strtol(attr[i + 1], NULL, 0);
		logd("lv_weak_th[2]: %d", ve_param->mRegionD3DParam.lv_weak_th[2]);
	}
    else if (strcmp(attr[i], "s32_lv_weak_th_1") == 0)
	{
		ve_param->mRegionD3DParam.lv_weak_th[1] = strtol(attr[i + 1], NULL, 0);
		logd("lv_weak_th[1]: %d", ve_param->mRegionD3DParam.lv_weak_th[1]);
	}
    else if (strcmp(attr[i], "s32_lv_weak_th_0") == 0)
	{
		ve_param->mRegionD3DParam.lv_weak_th[0] = strtol(attr[i + 1], NULL, 0);
		logd("lv_weak_th[0]: %d", ve_param->mRegionD3DParam.lv_weak_th[0]);
	}
	return 0;
}


static int parse_3d_filter_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "u8_d3d_en") == 0)
	{
		ve_param->m3DfilterParam.enable_3d_filter = strtol(attr[i + 1], NULL, 0);
		logd("enable_3d_filter: %d", ve_param->m3DfilterParam.enable_3d_filter);
	}
    else if (strcmp(attr[i], "u8_d3d_max_mv_th") == 0)
	{
		ve_param->m3DfilterParam.max_mv_th = strtol(attr[i + 1], NULL, 0);
		logd("max_mv_th: %d", ve_param->m3DfilterParam.max_mv_th);
	}
    else if (strcmp(attr[i], "u8_d3d_max_mad_th") == 0)
	{
		ve_param->m3DfilterParam.max_mad_th = strtol(attr[i + 1], NULL, 0);
		logd("max_mad_th: %d", ve_param->m3DfilterParam.max_mad_th);
	}
    else if (strcmp(attr[i], "u8_d3d_pix_level_en") == 0)
	{
		ve_param->m3DfilterParam.adjust_pix_level_enable = strtol(attr[i + 1], NULL, 0);
		logd("adjust_pix_level_enable: %d", ve_param->m3DfilterParam.adjust_pix_level_enable);
	}
    else if (strcmp(attr[i], "u8_d3d_pix_diff_th") == 0)
	{
		ve_param->m3DfilterParam.max_pix_diff_th = strtol(attr[i + 1], NULL, 0);
		logd("max_pix_diff_th: %d", ve_param->m3DfilterParam.max_pix_diff_th);
	}
    else if (strcmp(attr[i], "u8_d3d_smooth_en") == 0)
	{
		ve_param->m3DfilterParam.smooth_filter_enable = strtol(attr[i + 1], NULL, 0);
		logd("smooth_filter_enable: %d", ve_param->m3DfilterParam.smooth_filter_enable);
	}
    else if (strcmp(attr[i], "u8_min_coef") == 0)
	{
		ve_param->m3DfilterParam.min_coef = strtol(attr[i + 1], NULL, 0);
		logd("min_coef: %d", ve_param->m3DfilterParam.min_coef);
	}
    else if (strcmp(attr[i], "u8_max_coef") == 0)
	{
		ve_param->m3DfilterParam.max_coef = strtol(attr[i + 1], NULL, 0);
		logd("max_coef: %d", ve_param->m3DfilterParam.max_coef);
	}
	return 0;
}

static int parse_2d_filter_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "u8_d2d_en") == 0)
	{
		ve_param->m2DfilterParam.enable_2d_filter = strtol(attr[i + 1], NULL, 0);
		logd("enable_2d_filter: %d", ve_param->m2DfilterParam.enable_2d_filter);
	}
    else if (strcmp(attr[i], "u8_d2d_strength_uv") == 0)
	{
		ve_param->m2DfilterParam.filter_strength_uv = strtol(attr[i + 1], NULL, 0);
		logd("filter_strength_uv: %d", ve_param->m2DfilterParam.filter_strength_uv);
	}
    else if (strcmp(attr[i], "u8_d2d_strength_y") == 0)
	{
		ve_param->m2DfilterParam.filter_strength_y = strtol(attr[i + 1], NULL, 0);
		logd("filter_strength_y: %d", ve_param->m2DfilterParam.filter_strength_y);
	}
    else if (strcmp(attr[i], "u8_d2d_th_uv") == 0)
	{
		ve_param->m2DfilterParam.filter_th_uv = strtol(attr[i + 1], NULL, 0);
		logd("filter_th_uv: %d", ve_param->m2DfilterParam.filter_th_uv);
	}
    else if (strcmp(attr[i], "u8_d2d_th_y") == 0)
	{
		ve_param->m2DfilterParam.filter_th_y = strtol(attr[i + 1], NULL, 0);
		logd("filter_th_y: %d", ve_param->m2DfilterParam.filter_th_y);
	}
	return 0;
}

static int parse_target_bits_clip_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "u8_bits_clip_dis_default") == 0)
	{
		ve_param->bits_clip_param.dis_default_para = strtol(attr[i + 1], NULL, 0);
		logd("dis_default_para: %d", ve_param->bits_clip_param.dis_default_para);
	}
    else if (strcmp(attr[i], "s32_bits_clip_mode") == 0)
	{
		ve_param->bits_clip_param.mode = strtol(attr[i + 1], NULL, 0);
		logd("s32_bits_clip_mode: %d", ve_param->bits_clip_param.mode);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_0_0") == 0)
	{
		ve_param->bits_clip_param.coef_th[0][0] = strtof(attr[i + 1], NULL);
		logd("coef_th[0][0]: %f", ve_param->bits_clip_param.coef_th[0][0]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_0_1") == 0)
	{
		ve_param->bits_clip_param.coef_th[0][1] = strtof(attr[i + 1], NULL);
		logd("coef_th[0][1]: %f", ve_param->bits_clip_param.coef_th[0][1]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_1_0") == 0)
	{
		ve_param->bits_clip_param.coef_th[1][0] = strtof(attr[i + 1], NULL);
		logd("coef_th[1][0]: %f", ve_param->bits_clip_param.coef_th[1][0]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_1_1") == 0)
	{
		ve_param->bits_clip_param.coef_th[1][1] = strtof(attr[i + 1], NULL);
		logd("coef_th[1][1]: %f", ve_param->bits_clip_param.coef_th[1][1]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_2_0") == 0)
	{
		ve_param->bits_clip_param.coef_th[2][0] = strtof(attr[i + 1], NULL);
		logd("coef_th[2][0]: %f", ve_param->bits_clip_param.coef_th[2][0]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_2_1") == 0)
	{
		ve_param->bits_clip_param.coef_th[2][1] = strtof(attr[i + 1], NULL);
		logd("coef_th[2][1]: %f", ve_param->bits_clip_param.coef_th[2][1]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_3_0") == 0)
	{
		ve_param->bits_clip_param.coef_th[3][0] = strtof(attr[i + 1], NULL);
		logd("coef_th[3][0]: %f", ve_param->bits_clip_param.coef_th[3][0]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_3_1") == 0)
	{
		ve_param->bits_clip_param.coef_th[3][1] = strtof(attr[i + 1], NULL);
		logd("coef_th[3][1]: %f", ve_param->bits_clip_param.coef_th[3][1]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_4_0") == 0)
	{
		ve_param->bits_clip_param.coef_th[4][0] = strtof(attr[i + 1], NULL);
		logd("coef_th[4][0]: %f", ve_param->bits_clip_param.coef_th[4][0]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_coef_4_1") == 0)
	{
		ve_param->bits_clip_param.coef_th[4][1] = strtof(attr[i + 1], NULL);
		logd("coef_th[4][1]: %f", ve_param->bits_clip_param.coef_th[4][1]);
	}
    else if (strcmp(attr[i], "u8_bits_clip_gop_ratio_en") == 0)
	{
		ve_param->bits_clip_param.en_gop_clip = strtol(attr[i + 1], NULL, 0);
		logd("en_gop_clip: %d", ve_param->bits_clip_param.en_gop_clip);
	}
    else if (strcmp(attr[i], "f32_bits_clip_gop_ratio_th_0") == 0)
	{
		ve_param->bits_clip_param.gop_bit_ratio_th[0] = strtof(attr[i + 1], NULL);
		logd("gop_bit_ratio_th[0]: %f", ve_param->bits_clip_param.gop_bit_ratio_th[0]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_gop_ratio_th_1") == 0)
	{
		ve_param->bits_clip_param.gop_bit_ratio_th[1] = strtof(attr[i + 1], NULL);
		logd("gop_bit_ratio_th[1]: %f", ve_param->bits_clip_param.gop_bit_ratio_th[1]);
	}
    else if (strcmp(attr[i], "f32_bits_clip_gop_ratio_th_2") == 0)
	{
		ve_param->bits_clip_param.gop_bit_ratio_th[2] = strtof(attr[i + 1], NULL);
		logd("gop_bit_ratio_th[2]: %f", ve_param->bits_clip_param.gop_bit_ratio_th[2]);
	}
	return 0;
}

static int parse_force_conf_win_param(VencParamFromFiles *ve_param, const XML_Char **attr, int i)
{
    if (strcmp(attr[i], "u8_en_force_conf") == 0)
	{
		ve_param->conf_win.en_force_conf = strtol(attr[i + 1], NULL, 0);
		logd("u8_en_force_conf: %d", ve_param->conf_win.en_force_conf);
	}
    else if (strcmp(attr[i], "u32_lefet_offset") == 0)
	{
		ve_param->conf_win.left_offset = strtol(attr[i + 1], NULL, 0);
		logd("left_offset: %d", ve_param->conf_win.left_offset);
	}
    else if (strcmp(attr[i], "u32_right_offset") == 0)
	{
		ve_param->conf_win.right_offset = strtol(attr[i + 1], NULL, 0);
		logd("right_offset: %d", ve_param->conf_win.right_offset);
	}
    else if (strcmp(attr[i], "u32_top_offset") == 0)
	{
		ve_param->conf_win.top_offset = strtol(attr[i + 1], NULL, 0);
		logd("top_offset: %d", ve_param->conf_win.top_offset);
	}
    else if (strcmp(attr[i], "u32_bottom_offset") == 0)
	{
		ve_param->conf_win.bottom_offset = strtol(attr[i + 1], NULL, 0);
		logd("bottom_offset: %d", ve_param->conf_win.bottom_offset);
	}
	return 0;
}

static void end_parse_tag(void *data, const XML_Char *tag_name)
{
    VencParamFromFiles *ve_param = (VencParamFromFiles*)data;

    if (strcmp(tag_name, "static") == 0 && ve_param->is_static)
		ve_param->parsingStatic = 0;
	else if (strcmp(tag_name, "dynamic") == 0 && !ve_param->is_static)
		ve_param->parsingDynamic = 0;

}

static int check_status(const XML_Char *name, const XML_Char *value)
{
	if (strcmp(name, "status") == 0 && strcmp(value, "off") == 0) {
		return 0;
	}

	return 1;
}

static void start_parse_tag(void *data, const XML_Char *tag_name,
                      const XML_Char **attr)
{
    VencParamFromFiles *ve_param = (VencParamFromFiles*)data;
    VideoEncoder* pVideoEnc = (VideoEncoder*)ve_param->pVideoEnc;
    static int output_index = 0;

    if (strcmp(tag_name, "static") == 0 && ve_param->is_static) {
		ve_param->parsingStatic = 1;
    }
    else if (strcmp(tag_name, "dynamic") == 0 && !ve_param->is_static) {
		ve_param->parsingDynamic = 1;
    }

	if (ve_param->parsingStatic && ve_param->is_static) {
		if (check_status(attr[0], attr[1]) == 0)
			return ;

		if (strcmp(tag_name, "frame_rate") == 0) {
			for (unsigned int i = 0; attr[i]; i += 2) {
				parse_frame_rate_param(ve_param, attr, i);
			}
			VencSetParameter(pVideoEnc, VENC_IndexParamFramerate, &ve_param->fps);
		}
		else if (strcmp(tag_name, "bit_rate") == 0) {
			for (unsigned int i = 0; attr[i]; i += 2) {
				parse_bit_rate_param(ve_param, attr, i);
			}
			VencSetParameter(pVideoEnc, VENC_IndexParamBitrate, &ve_param->bit_rate);
		}
		else if (strcmp(tag_name, "vbv_size") == 0) {
			for (unsigned int i = 0; attr[i]; i += 2) {
				parse_vbv_size_param(ve_param, attr, i);
			}
			VencSetParameter(pVideoEnc, VENC_IndexParamSetVbvSize, &ve_param->vbv_size);
		}
		else if (strcmp(tag_name, "pixel_format") == 0) {
			for (unsigned int i = 0; attr[i]; i += 2) {
				parse_pixel_format_param(ve_param, attr, i);
			}
			ve_param_set_pxl_format(ve_param->pVideoEnc, ve_param);
		}
		else if (strcmp(tag_name, "resolution") == 0) {
			for (unsigned int i = 0; attr[i]; i += 2) {
				parse_resolution_param(ve_param, attr, i);
			}
			ve_param_set_res(ve_param->pVideoEnc, ve_param);
		}
		else if (strcmp(tag_name, "qp_range") == 0) {
			if (ve_param->codecType == VENC_CODEC_JPEG)
				return ;
			for (unsigned int i = 0; attr[i]; i += 2) {
				parse_qp_range_param(ve_param, attr, i);
			}
			VencSetParameter(pVideoEnc, VENC_IndexParamH264QPRange, &ve_param->qp_range);
		}
	}
	else if (ve_param->parsingDynamic && !ve_param->is_static) {
		if (check_status(attr[0], attr[1]) == 0)
			return ;

		if (strcmp(tag_name, "frame_rate") == 0) {
			for (unsigned int i = 0; attr[i]; i += 2) {
				parse_frame_rate_param(ve_param, attr, i);
			}
			VencSetParameter(pVideoEnc, VENC_IndexParamFramerate, &ve_param->fps);
			return ;
		}
		else if (strcmp(tag_name, "bit_rate") == 0) {
			for (unsigned int i = 0; attr[i]; i += 2) {
				parse_bit_rate_param(ve_param, attr, i);
			}
			VencSetParameter(pVideoEnc, VENC_IndexParamBitrate, &ve_param->bit_rate);
			return ;
		}

		if (ve_param->codecType == VENC_CODEC_JPEG) {
			if (strcmp(tag_name, "s32_jpeg_quality") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					if (strcmp(attr[i], "value") == 0)
					{
						ve_param->jpeg_quality = strtol(attr[i + 1], NULL, 0);
						logd("jpeg_quality: %d", ve_param->jpeg_quality);
					}
				}
				VencSetParameter(pVideoEnc, VENC_IndexParamJpegQuality, &ve_param->jpeg_quality);
			}
		}

		if (ve_param->codecType != VENC_CODEC_JPEG) {
			if (strcmp(tag_name, "force_conf_win") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_force_conf_win_param(ve_param, attr, i);
				}
				VencSetParameter(pVideoEnc, VENC_IndexParamForceConfWin, &ve_param->conf_win);
			}
			else if (strcmp(tag_name, "qp_range") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_qp_range_param(ve_param, attr, i);
				}
				VencSetParameter(pVideoEnc, VENC_IndexParamH264QPRange, &ve_param->qp_range);
			}
			else if (strcmp(tag_name, "fix_qp") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_fix_qp_param(ve_param, attr, i);
				}
				VencSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &ve_param->fixQP);
			}
			else if (strcmp(tag_name, "target_bits_clip") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_target_bits_clip_param(ve_param, attr, i);
				}
				VencSetParameter(pVideoEnc, VENC_IndexParamTargetBitsClipParam, &ve_param->bits_clip_param);
			}
			else if (strcmp(tag_name, "vbr_param") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_vbr_param_param(ve_param, attr, i);
				}
				VencSetParameter(pVideoEnc, VENC_IndexParamSetVbrParam, &ve_param->sVbrParam);
			}
			else if (strcmp(tag_name, "d2d_filter") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_2d_filter_param(ve_param, attr, i);
				}
				VencSetParameter(pVideoEnc, VENC_IndexParam2DFilter, &ve_param->m2DfilterParam);
			}
			else if (strcmp(tag_name, "d3d_filter") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_3d_filter_param(ve_param, attr, i);
				}
				VencSetParameter(pVideoEnc, VENC_IndexParam3DFilterNew, &ve_param->m3DfilterParam);
			}
			else if (strcmp(tag_name, "region_d3d") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_region_d3d_param(ve_param, attr, i);
				}
				VencSetParameter(pVideoEnc, VENC_IndexParamRegionD3DParam, &ve_param->mRegionD3DParam);
			}
			else if (strcmp(tag_name, "mb_rc_level") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					if (strcmp(attr[i], "value") == 0)
					{
						ve_param->mb_rc_level = strtol(attr[i + 1], NULL, 0);
						logd("mb_rc_level: %d", ve_param->mb_rc_level);
					}
				}
				VencSetParameter(pVideoEnc, VENC_IndexParamEnIFrmMbRcMoveStatus, &ve_param->mb_rc_level);
			}
			else if (strcmp(tag_name, "f32_weak_text_th") == 0) {
				for (unsigned int i = 0; attr[i]; i += 2) {
					if (strcmp(attr[i], "f32_weak_text_th") == 0)
					{
						ve_param->weak_text_th = strtof(attr[i + 1], NULL);
						logd("weak_text_th: %f", ve_param->weak_text_th);
					}
				}
				VencSetParameter(pVideoEnc, VENC_IndexParamWeakTextTh, &ve_param->weak_text_th);
			}
			else if (strcmp(tag_name, "h264_video_timing") == 0) {
				if (ve_param->codecType != VENC_CODEC_H264 && ve_param->codecType != VENC_CODEC_H264_VER2) {
					return ;
				}
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_h264_video_timing_param(ve_param, attr, i);
				}
				if (ve_param->mH264VideoTiming.time_scale == 0)
					ve_param->mH264VideoTiming.time_scale = ve_param->mH264VideoTiming.num_units_in_tick*ve_param->fps*2;
				VencSetParameter(pVideoEnc, VENC_IndexParamH264VideoTiming, &ve_param->mH264VideoTiming);
			}
			else if (strcmp(tag_name, "h265_video_timing") == 0) {
				if (ve_param->codecType != VENC_CODEC_H265) {
					return ;
				}
				for (unsigned int i = 0; attr[i]; i += 2) {
					parse_h265_video_timing_param(ve_param, attr, i);
				}
				if (ve_param->mH265VideoTiming.time_scale == 0)
					ve_param->mH265VideoTiming.time_scale = ve_param->mH265VideoTiming.num_units_in_tick*ve_param->fps*2;
				VencSetParameter(pVideoEnc, VENC_IndexParamH265Timing, &ve_param->mH265VideoTiming);
			}
		}
	}

}

int ParserCreateAndParse(XML_Parser parser, VencParamFromFiles *p_ve_param, FILE *xml_file)
{
    void *buf;
    int  bytes_read;

    parser = XML_ParserCreate(NULL);
    if (!parser) {
        loge("Failed to create XML parser");
        return -1;
    }
    XML_SetUserData(parser, p_ve_param);
    XML_SetElementHandler(parser, start_parse_tag, end_parse_tag);

    for (;;) {
        buf = XML_GetBuffer(parser, BUF_SIZE);
        if (buf == NULL) {
            XML_ParserFree(parser);
            return -1;
        }

        bytes_read = fread(buf, 1, BUF_SIZE, xml_file);
        if (bytes_read < 0) {
            XML_ParserFree(parser);
            return -1;
        }

        if (XML_ParseBuffer(parser, bytes_read,
                    bytes_read == 0) == XML_STATUS_ERROR) {
            loge("Error in demo config xml file");
            XML_ParserFree(parser);
            return -1;
        }

        if (bytes_read == 0)
            break;
    }
	XML_ParserFree(parser);
	return 0;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
