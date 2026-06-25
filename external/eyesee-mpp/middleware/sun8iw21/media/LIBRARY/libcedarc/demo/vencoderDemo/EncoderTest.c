/*
 * Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
 * All rights reserved.
 *
 * File : EncoderTest.c
 * Description : EncoderTest
 * History :
 *
 */
#define LOG_TAG "demo"

#include "cdc_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vencoder.h"
#include "EncAdapter.h"
#include <sys/time.h>
#include <time.h>
#include <memoryAdapter.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>

#include "avtimer.h"
#include "veAdapter.h"
#include "CdcUtil.h"

#define ENCODER_MAX_NUM (5)
#define DEMO_FILE_NAME_LEN 256
#define USE_H265_ENC
#define ROI_NUM 4
#define NO_READ_WRITE 0
#define SAVE_AWSP     0
#define SETUP_VIDEO_TIME_INFO (0) //* time info about fps
//#define YU12_NV12
//#define USE_AFBC_INPUT
//#define YU12_NV21
//#define VBR

//#define USE_SVC
#define USE_VIDEO_SIGNAL
//#define USE_ASPECT_RATIO
//#define USE_SUPER_FRAME

//#define GET_MB_INFO
//#define SET_MB_INFO
//#define SET_SMART
//#define DETECT_MOTION

#define SET_ROI_PARAM			0
#define ENABLE_GET_WRITE_BACK_YUV (0)
#define ENABLE_SET_REC_LBC_MODE_TEST (0)
#define ENABLE_PAGE_BUF_MODE (1)
#define SHOW_PTS_INFO (0)
#define ENABLE_2D_FLITER (0)
#define ENABLE_3D_FLITER (0)
#define ENABLE_SUPER_FRAME (0)
#define ENABLE_DROP_FRAME_NUM (0)
#define ENABLE_TARGET_BITS_RATIO (0)
#define ENABLE_BITS_CLIP (0)
#define ENABLE_WEAK_TEXT_TH (0)
#define ENABLE_REGION_D3D (0)
#define ENABLE_VE2ISP_D2D (0)
#define ENABLE_SEI (0)
#define ENABLE_JPEG_INSERT_DATA (0)
#define WB_BUF_NUM (10)

//* test the function of overlay/lbc/afbc by  "./demoVencoder -overlay/-afbc/-lbc"
#if 0
#define TEST_OVERLAY_FUNC (0)
#define USE_AFBC_INPUT
#define USE_LBC_INPUT
#define USE_LBC_LOSSY_COM_EN_2x (0)
#define USE_LBC_LOSSY_COM_EN_2_5x (0)
#endif

#define ALIGN_XXB(y, x) (((x) + ((y)-1)) & ~((y)-1))
#define ALIGN_16B(x) (((x) + (15)) & ~(15))
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define FREE(buf) if((buf)!=NULL){free(buf);(buf)=NULL;}
#define my_printf() logd("func:%s, line:%d\n", __func__, __LINE__)

#define DEMO_INPUT_BUFFER_NUM (3)


typedef struct ptsDebugInfo
{
    long long curPts;
    long long prePts;
    long long maxPts;
    long long minPts;
}ptsDebugInfo;

typedef struct InputBufferInfo InputBufferInfo;

struct InputBufferInfo
{
    VencInputBuffer     inputbuffer;
    InputBufferInfo*    next;
};

typedef struct InputBufferMgr
{
    InputBufferInfo   buffer_node[DEMO_INPUT_BUFFER_NUM];
    InputBufferInfo*  valid_quene;
    InputBufferInfo*  empty_quene;
}InputBufferMgr;

static void enqueue(InputBufferInfo** pp_head, InputBufferInfo* p)
{
    InputBufferInfo* cur;

    cur = *pp_head;

    if (cur == NULL)
    {
        *pp_head = p;
        p->next  = NULL;
        return;
    }
    else
    {
        while(cur->next != NULL)
            cur = cur->next;

        cur->next = p;
        p->next   = NULL;

        return;
    }
}

static InputBufferInfo* dequeue(InputBufferInfo** pp_head)
{
    InputBufferInfo* head;

    head = *pp_head;

    if (head == NULL)
    {
        return NULL;
    }
    else
    {
        *pp_head = head->next;
        head->next = NULL;
        return head;
    }
}

//extern int gettimeofday(struct timeval *tv, struct timezone *tz);
static long long GetNowUs()
{
    long long curr;
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = ((long long)(t.tv_sec)*1000000000LL + t.tv_nsec)/1000LL;
    return curr;
}

typedef struct {
    FILE_STRUCT          *wbyuv_file;
    unsigned char *yuvBuf;
    unsigned int   yuvSize;
}WbYuvFuncInfo;

typedef struct {
    int     argc;
    char**  argv;
    int nChannel;
    int nTotalChannelNum;
    int bOnlineMode;
    int bOnlineChannel;
    InputBufferMgr mInputBufMgr;
    VencBaseConfig baseConfig;
    VideoEncoder* pVideoEnc;
    VencMBModeCtrl  mMBModeCtrl;
    VencMBInfo      mMBInfo;
    WbYuvFuncInfo   mWbYuvFuncInfo;
}encoder_Context;

typedef struct {
    EXIFInfo                exifinfo;
    int                     quality;
    int                     jpeg_mode;
    VencJpegVideoSignal     vs;
    int                     jpeg_biteRate;
    int                     jpeg_frameRate;
    VencBitRateRange        bitRateRange;
    VencOverlayInfoS        sOverlayInfo;
    VencCopyROIConfig       pRoiConfig;
}jpeg_func_t;

typedef struct {
    VencHeaderData          sps_pps_data;
    VencH264Param           h264Param;
    VencMBModeCtrl          h264MBMode;
    VencMBInfo              MBInfo;
    VencFixQP           fixQP;
    VencSuperFrameConfig    sSuperFrameCfg;
    VencH264SVCSkip         SVCSkip; // set SVC and skip_frame
    VencH264AspectRatio     sAspectRatio;
    VencH264VideoSignal     sVideoSignal;
    VencCyclicIntraRefresh  sIntraRefresh;
    VencROIConfig           sRoiConfig[ROI_NUM];
    VeProcSet               sVeProcInfo;
    VencOverlayInfoS        sOverlayInfo;
    VencSmartFun            sH264Smart;
}h264_func_t;

typedef struct {
    VencH265Param               h265Param;
    VencH265GopStruct           h265Gop;
    VencHVS                     h265Hvs;
    VencH265TendRatioCoef       h265Trc;
    VencSmartFun                h265Smart;
    VencMBModeCtrl              h265MBMode;
    VencMBInfo                  MBInfo;
    VencFixQP               fixQP;
    VencSuperFrameConfig        sSuperFrameCfg;
    VencH264SVCSkip             SVCSkip; // set SVC and skip_frame
    VencH264AspectRatio         sAspectRatio;
    VencH264VideoSignal         sVideoSignal;
    VencCyclicIntraRefresh      sIntraRefresh;
    VencROIConfig               sRoiConfig[ROI_NUM];
    VencAlterFrameRateInfo sAlterFrameRateInfo;
    int                         h265_rc_frame_total;
    VeProcSet               sVeProcInfo;
    VencOverlayInfoS        sOverlayInfo;
}h265_func_t;


typedef struct {
    char             intput_file[256];
    char             output_file[256];
    char             reference_file[256];
    char             overlay_file[256];
    char             log_file[256];
    int              compare_flag;
    int              log_flag;
    int              compare_result;

    unsigned int  encode_frame_num;
    unsigned int  encode_format;

    unsigned int src_size;
    unsigned int dst_size;

    unsigned int src_width;
    unsigned int src_height;
    unsigned int dst_width;
    unsigned int dst_height;

    unsigned int thumb_scaler_factor;

    unsigned int vbv_size;

    unsigned int is_night;
    unsigned int sensor_type;

    int frequency;
    int bit_rate;
    int frame_rate;
    int maxKeyFrame;
    int mb_rc_level;
    unsigned int test_cycle;
    unsigned int test_overlay_flag;
    unsigned int test_lbc;
    unsigned int test_afbc;
    unsigned int limit_encode_speed; //* limit encoder speed by framerate
    unsigned int i_qp_min;
    unsigned int i_qp_max;
    unsigned int p_qp_min;
    unsigned int p_qp_max;
    unsigned int qp_init;
    unsigned int mb_qp_limit;
    float        weak_text_th;

    VencVe2IspD2DLimit ve2isp_d2d;

    jpeg_func_t jpeg_func;
    h264_func_t h264_func;
    h265_func_t h265_func;
    unsigned int nChannel;
    VencCopyROIConfig pRoiConfig;
    unsigned int pixel_format;
    unsigned int output_format;
    unsigned int extend_flag;
    unsigned int nShare_buf_num;

	//VencGdcParam mGdcParam;
	unsigned int bEnableGdc;
    unsigned int bEnableSharp;
	unsigned int rotate;

    unsigned int rec_lbc_mode; //*0: disable, 1:1.5x , 2: 2.0x, 3: 2.5x, 4: no_lossy

    VencForceConfWin conf_win;
    unsigned int en_crop;
    unsigned int crop_l;
    unsigned int crop_t;
    unsigned int crop_w;
    unsigned int crop_h;

    unsigned int sei_num;

    VencProductModeInfo mProductMode;
    VencTargetBitsClipParam mBitsClip;
    VencIPTargetBitsRatio mTargetBits;
    VencSuperFrameConfig mSuperFrame;
    VencRegionD3DParam mRegionD3DParam;
    s3DfilterParam m3DfilterParam;
    s2DfilterParam m2DfilterParam;
    VencH264VideoSignal mVideoSignal;
    unsigned int               mVbrOptEn;
    VencVbrOptParam            mVbrOptParam;
    unsigned int  bvcu;
}encode_param_t;

typedef enum {
    INPUT,
    HELP,
    ENCODE_FRAME_NUM,
    ENCODE_FORMAT,
    OUTPUT,
    SRC_SIZE,
    DST_SIZE,
    THUMB_FACTOR,
    COMPARE_FILE,
    LOG_FILE,
    FREQUENCY,
    BIT_RATE,
    FRM_RATE,
    VBV_SIZE,
    GOP_SIZE,
    KEY_FRM_PERIOD,
    GOP_MODE,
    RC_MODE,
    PRODUCT_MODE,
    IS_NIGHT,
    SENSOR_TYPE,
    MOVING_TH,
    QUALITY,
    JPEG_MODE,
    TEST_CYCLE,
    TEST_OVERLAY,
    TEST_OVERLAY_IN,
    TEST_LBC,
    TEST_AFBC,
    LIMIT_SPEED,
    I_QP_MIN,
    I_QP_MAX,
    P_QP_MIN,
    P_QP_MAX,
    QP_INIT,
    MB_QP_LIMIT,
    ENCODER_NUM,
    ONLINE_MODE,
    FIXEL_FORMAT,
    EXTEND_FLAG,
    SHARE_BUF_NUM,
    GDC_MODE,
    ENABLE_GDC,
    ENABLE_SR,
    ENABLE_SHARP,
    ROTATE,
    REC_LBC_MODE,
    REC_LBC_NO_LOSSY,
    FORCE_CONF_WIN,
    ENABLE_CROP,
    CROP_LEFT,
    CROP_TOP,
    CROP_WIDTH,
    CROP_HEIGHT,
    SCENE,
    MOVE,
    SUPER,
    BITS_CLIP,
    MB_RC_LEVEL,
    D3D,
    REGION_D3D,
    D2D,
    WEAK_TEXT_TH,
    VE2ISP_D2D,
    SEI_NUM,
    VIDEO_SIGNAL,
    INSTANEOUS_BITRATE,
    VBR_MODE_OPTNEW,
    BPP_LEVEL,
    BPP_ADJUSTQPTH,
    BPP_ADJUSTMAXQP,
    SAVE_BITRATE,
    MOSAIC_OPT,
    MOVETOSTATIC_OPT,
    STATISTIC_FLAG,
    FRAME_BITRATE,
    FRAME_MOVESTATUS,
    FRAME_MAD_TH,
	VCU,
    INVALID
}ARGUMENT_T;

typedef struct {
    char Short[16];
    char Name[128];
    ARGUMENT_T argument;
    char Description[512];
}argument_t;

static const argument_t ArgumentMapping[] =
{
    { "-h",  "--help",    HELP,
        "Print this help" },
    { "-i",  "--input",   INPUT,
        "Input file path" },
    { "-n",  "--encode_frame_num",   ENCODE_FRAME_NUM,
        "After encoder n frames, encoder stop" },
    { "-f",  "--encode_format",  ENCODE_FORMAT,
        "0:h264 encoder, 1:jpeg_encoder, 3:h265 encoder, 5: encpp" },
    { "-o",  "--output",  OUTPUT,
        "output file path" },
    { "-s",  "--srcsize",  SRC_SIZE,
        "src_size,can be 1920x1080 or 2160,1080,720,480,288" },
    { "-d",  "--dstsize",  DST_SIZE,
        "dst_size,can be 1920x1080 or 2160,1080,720,480,288" },
    { "-tf", "--thumb_factor", THUMB_FACTOR,
        "thumb_factr: 0:1/1  1:1/8  2:1/2  3:1/4"},
    { "-c",  "--compare",  COMPARE_FILE,
        "compare file:reference file path" },
    { "-q",  "--frequency", FREQUENCY,
        "frequency: the frequency of video engine"},
    { "-b",  "--bitrate",  BIT_RATE,
        "bitRate:bps" },
    { "-r",  "--framerate",  FRM_RATE,
        "framerate:fps" },
    { "-v",  "--vbv_size",  VBV_SIZE,
        "vbv_size:byte" },
    { "-gs",  "--gop_size",  GOP_SIZE,
        "Gop Size:" },
    { "-kp",  "--key_frm_period",  KEY_FRM_PERIOD,
        "Key Frame Period" },
    { "-gm",  "--gop_mode",  GOP_MODE,
        "Gop Mode" },
    { "-rm",  "--rc_mode",  RC_MODE,
        "RC Mode" },
    { "-pm",  "--product_mode",  PRODUCT_MODE,
        "Product Mode" },
    { "-in",  "--is_night",  IS_NIGHT,
        "Is Night" },
    { "-st",  "--sensor_type",  SENSOR_TYPE,
        "Sensor Type" },
    { "-mt",  "--moving_th",  MOVING_TH,
        "Moving Th" },
    { "-qua",  "--quality",  QUALITY,
        "Quality" },
    { "-jm",  "--jpeg_mode",  JPEG_MODE,
        "Jpeg Mode" },
    { "-t",  "--test_cycle",   TEST_CYCLE,
        "Cycle num of testing" },
    { "-l",  "--logfile",  LOG_FILE,
        "Log file path" },
    { "-overlay",  "--overlay",  TEST_OVERLAY,
        "add the overlay function" },
    { "-overlay_in",  "--overlay_int",  TEST_OVERLAY_IN,
        "overlay intput data file" },
    { "-lbc",  "--lbc ",  TEST_LBC,
        "1: no lossy lbc, 2: lossy_2x, 3: lossy_2.5x" },
    { "-afbc",  "--afbc",  TEST_AFBC,
        "1: test afbc input data format" },
    { "-limit_sp",  "--limit speed",  LIMIT_SPEED,
        "1: limit encoder speed by framerate" },
    { "-i_qp_min",  "--i qp min",  I_QP_MIN,
        "set the i_qp_min value" },
    { "-i_qp_max",  "--i qp max",  I_QP_MAX,
        "set the i_qp_max value" },
    { "-p_qp_min",  "--i qp min",  P_QP_MIN,
        "set the p_qp_min value" },
    { "-p_qp_max",  "--i qp max",  P_QP_MAX,
        "set the p_qp_max value" },
    { "-qp_init",  "--qp init",  QP_INIT,
        "set the qp_init value" },
    { "-mb_qp_limit",  "--mb_qp_limit",  MB_QP_LIMIT,
        "set the enable mb_qp_limit" },
    { "-enc_num",  "--encoder num",  ENCODER_NUM,
        "create multi encoder" },
    { "-online",  "--online mode",  ONLINE_MODE,
        "online mode" },
    { "-pformat",  "--piexl format",  FIXEL_FORMAT,
        "--piexl format" },
    { "-ext_flag",  "--ext_flag",  EXTEND_FLAG,
        "--ext_flag" },
    { "-share_num",  "--share buf num",  SHARE_BUF_NUM,
        "--share buf num" },
    { "-gdc_mode",  "--gdc_mode",  GDC_MODE,
        "--gdc_mode" },
    { "-en_gdc",  "--en_gdc",  ENABLE_GDC,
        "--en_gdc" },
    { "-en_sr",  "--en_sr",  ENABLE_SR,
        "--en_sr" },
    { "-en_sharp",  "--enable_sharp",  ENABLE_SHARP,
        "--enable_sharp" },
    { "-rotate",  "--rotate",  ROTATE,
        "--rotate" },
    { "-rec_lbc_mode",  "--rec_lbc_mode",  REC_LBC_MODE,
        "--0: disable, 1:1.5x , 2: 2.0x, 3: 2.5x, 4: no_lossy" },
    { "-conf_win",  "--conf_win",  FORCE_CONF_WIN,
        "-----------" },
    { "-en_crop",  "--en_crop",  ENABLE_CROP,
        "--0: disable, 1: enable" },
    { "-crop_l",  "--crop_left",  CROP_LEFT,
        "-----------" },
    { "-crop_t",  "--crop_top",  CROP_TOP,
        "-----------" },
    { "-crop_w",  "--crop_widht",  CROP_WIDTH,
        "-----------" },
    { "-crop_h",  "--crop_height",  CROP_HEIGHT,
        "-----------" },
    { "-scene",  "--scene_ratio",  SCENE,
        "-----------" },
    { "-move",  "--movee_ratio",  MOVE,
        "-----------" },
    { "-super",  "--super_frame",  SUPER,
        "-----------" },
    { "-clip",  "--bits_clip",  BITS_CLIP,
        "-----------" },
    { "-level",  "--mb_rc_level",  MB_RC_LEVEL,
        "-----------" },
    { "-d3d",  "--3d_filter",  D3D,
        "-----------" },
    { "-region_d3d",  "--region_3d_filter",  REGION_D3D,
        "-----------" },
    { "-d2d",  "--2d_filter",  D2D,
        "-----------" },
    { "-text",  "--protect_weak_text_th",  WEAK_TEXT_TH,
        "-----------" },
    { "-v2i_d2d",  "--ve2isp_d2d",  VE2ISP_D2D,
        "-----------" },
    { "-sei",  "--sei_num",  SEI_NUM,
        "-----------" },
    { "-vs",  "--video_signal",  VIDEO_SIGNAL,
        "-----------" },
    { "-maxBitrate",  "--instaneous_bitrate",  INSTANEOUS_BITRATE,
        "-----------" },
    { "-vbrmodeOpt",  "--vbr_mode_optnew",  VBR_MODE_OPTNEW,
        "-----------" },
    { "-BppLevel",  "--bpp_level",  BPP_LEVEL,
        "-----------" },
    { "-BppAdjustQpTh",  "--bpp_adjustQpTh",  BPP_ADJUSTQPTH,
        "-----------" },
    { "-BppAdjustMaxQp",  "--bpp_adjustMaxQp",  BPP_ADJUSTMAXQP,
        "-----------" },
    { "-saveBitrate",  "--save_bitrate",  SAVE_BITRATE,
        "-----------" },
    { "-mosAicOpt",    "--mosaic_opt",  MOSAIC_OPT,
        "-----------" },
    { "-mtsOp",    "--movetostatic_opt",  MOVETOSTATIC_OPT,
        "-----------" },
    { "-staFlag",    "--statistic_Flag",  STATISTIC_FLAG,
        "-----------" },
    { "-FMaxBit",  "--frame_bitrate",  FRAME_BITRATE,
        "-----------" },
    { "-Fmostus",  "--frame_movestatus",  FRAME_MOVESTATUS,
        "-----------" },
    { "-FmadTh",  "--mad_th",  FRAME_MAD_TH,
        "-----------" },
	{ "-vcu",  "--vcu",  VCU,
		"-----------" },
};

typedef struct {
    unsigned int width;
    unsigned int height;
    unsigned int width_aligh16;
    unsigned int height_aligh16;
    unsigned char* argb_addr;
    unsigned int size;
}BitMapInfoS;
BitMapInfoS bit_map_info[13];//= {{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0}};

int yu12_nv12(unsigned int width, unsigned int height, unsigned char *addr_uv,
          unsigned char *addr_tmp_uv, unsigned int fmt)
{
    unsigned int i, chroma_bytes;
    unsigned char *u_addr = NULL;
    unsigned char *v_addr = NULL;
    unsigned char *tmp_addr = NULL;

    if(fmt == VENC_CODEC_H264)
    {
        chroma_bytes = width * ALIGN_XXB(16, height) / 4;
    }
    else
    {
        chroma_bytes = width * height / 4;
    }

    u_addr = addr_uv;
    v_addr = addr_uv + chroma_bytes;
    tmp_addr = addr_tmp_uv;

    for(i=0; i<chroma_bytes; i++)
    {
        *(tmp_addr++) = *(u_addr++);
        *(tmp_addr++) = *(v_addr++);
    }

    memcpy(addr_uv, addr_tmp_uv, chroma_bytes*2);

    return 0;
}

int yu12_nv21(unsigned int width, unsigned int height, unsigned char *addr_uv,
          unsigned char *addr_tmp_uv, unsigned int fmt)
{
    unsigned int i, chroma_bytes;
    unsigned char *u_addr = NULL;
    unsigned char *v_addr = NULL;
    unsigned char *tmp_addr = NULL;

    if(fmt == VENC_CODEC_H264)
    {
        chroma_bytes = width * ALIGN_XXB(16, height) / 4;
    }
    else
    {
        chroma_bytes = width * height / 4;
    }


    u_addr = addr_uv;
    v_addr = addr_uv + chroma_bytes;
    tmp_addr = addr_tmp_uv;

    for(i=0; i<chroma_bytes; i++)
    {
        *(tmp_addr++) = *(v_addr++);
        *(tmp_addr++) = *(u_addr++);
    }

    memcpy(addr_uv, addr_tmp_uv, chroma_bytes*2);

    return 0;
}


ARGUMENT_T GetArgument(char *name)
{
    int i = 0;
    int num = sizeof(ArgumentMapping) / sizeof(argument_t);
    while(i < num)
    {
        logv("input_name:%s, i:%d, name:%s, short:%s, argument:%d, num:%d\n",
                                                    name,
                                                    i,
                                                    ArgumentMapping[i].Name,
                                                    ArgumentMapping[i].Short,
                                                    ArgumentMapping[i].argument,
                                                    num);
        if((0 == strcmp(ArgumentMapping[i].Name, name)) ||
            ((0 == strcmp(ArgumentMapping[i].Short, name)) &&
             (0 != strcmp(ArgumentMapping[i].Short, "--"))))
        {
            return ArgumentMapping[i].argument;
        }
        i++;
    }
    return INVALID;
}

static void PrintDemoUsage(void)
{
    int i = 0;
    int num = sizeof(ArgumentMapping) / sizeof(argument_t);
    printf("Usage:");
    while(i < num)
    {
        printf("%-12s %-32s %s", ArgumentMapping[i].Short, ArgumentMapping[i].Name,
                ArgumentMapping[i].Description);
        printf("\n");
        i++;
    }
}

void ParseArgument(encode_param_t *encode_param, char *argument, char *value)
{
    ARGUMENT_T arg;
    unsigned int ui_val = 0;

    arg = GetArgument(argument);

    switch(arg)
    {
        case HELP:
            PrintDemoUsage();
            exit(-1);
        case INPUT:
            memset(encode_param->intput_file, 0, sizeof(encode_param->intput_file));
            sscanf(value, "%255s", encode_param->intput_file);
            printf("get input file: %s\n", encode_param->intput_file);
            break;
        case ENCODE_FRAME_NUM:
            sscanf(value, "%32u", &encode_param->encode_frame_num);
            printf("encode:%u frames\n", encode_param->encode_frame_num);
            break;
        case ENCODE_FORMAT:
            sscanf(value, "%32u", &encode_param->encode_format);
            printf("encode_format:%u 0:h264,1:jpeg,3:h265, 5:encpp\n", encode_param->encode_format);
            break;
		case OUTPUT: {
			char out_path[256] = {0};
			memset(encode_param->output_file, 0, sizeof(encode_param->output_file));
			sscanf(value, "%255s", out_path);
			sprintf(encode_param->output_file, "%s", out_path);
			printf("get output file: %s\n", encode_param->output_file);
			break;
		}
        case SRC_SIZE:
            if(strlen(value) >= 5)
            {
                sscanf(value, "%32ux%32u", &encode_param->src_width,&encode_param->src_height);
            }
            else
            {
                sscanf(value, "%32u", &encode_param->src_size);
                if(encode_param->src_size == 1080)
                {
                    encode_param->src_width = 1920;
                    encode_param->src_height = 1080;
                }
                else if(encode_param->src_size == 1088)
                {
                    encode_param->src_width = 1920;
                    encode_param->src_height = 1088;
                }
                else if(encode_param->src_size == 720)
                {
                    encode_param->src_width = 1280;
                    encode_param->src_height = 720;
                }
                else if(encode_param->src_size == 480)
                {
                    encode_param->src_width = 640;
                    encode_param->src_height = 480;
                }
                else if(encode_param->src_size == 2160)
                {
                    encode_param->src_width = 3840;
                    encode_param->src_height = 2160;
                }
                else if(encode_param->src_size == 3456)
                {
                    encode_param->src_width = 3840;
                    encode_param->src_height = 2160;
                }
                else if(encode_param->src_size == 288)
                {
                    encode_param->src_width = 352;
                    encode_param->src_height = 288;
                }
                else if(encode_param->src_size == 1344)
                {
                    encode_param->src_width = 1344;
                    encode_param->src_height = 768;
                }
                else if(encode_param->src_size == 1536)
                {
                    encode_param->src_width = 1536;
                    encode_param->src_height = 864;
                }
                else
                {
                    encode_param->src_width = 1280;
                    encode_param->src_height = 720;
                    logw("encoder demo only support the size 1080p,720p,480p, \
                     now use the default size 720p\n");
                }
            }
            printf("get src_size: %ux%u\n", encode_param->src_width,encode_param->src_height);
            break;
        case DST_SIZE:
            if(strlen(value) >= 5)
            {
                sscanf(value, "%32ux%32u", &encode_param->dst_width,&encode_param->dst_height);
            }
            else
            {
                sscanf(value, "%32u", &encode_param->dst_size);
                if(encode_param->dst_size == 1080)
                {
                    encode_param->dst_width = 1920;
                    encode_param->dst_height = 1080;
                }
                else if(encode_param->dst_size == 1088)
                {
                    encode_param->dst_width = 1920;
                    encode_param->dst_height = 1088;
                }
                else if(encode_param->dst_size == 1344)
                {
                    encode_param->dst_width = 1344;
                    encode_param->dst_height = 756;
                }
                else if(encode_param->dst_size == 1536)
                {
                    encode_param->dst_width = 1536;
                    encode_param->dst_height = 864;
                }
                else if(encode_param->dst_size == 720)
                {
                    encode_param->dst_width = 1280;
                    encode_param->dst_height = 720;
                }
                else if(encode_param->dst_size == 480)
                {
                    encode_param->dst_width = 640;
                    encode_param->dst_height = 480;
                }
                else if(encode_param->dst_size == 2160)
                {
                    encode_param->dst_width = 3840;
                    encode_param->dst_height = 2160;
                }
                else if(encode_param->dst_size == 288)
                {
                    encode_param->dst_width = 352;
                    encode_param->dst_height = 288;
                }
                else
                {
                    encode_param->dst_width = 1280;
                    encode_param->dst_height = 720;
                    logw("encoder demo only support the size 1080p,720p,480p,\
                     now use the default size 720p\n");
                }
            }
            printf("get dst_size: %ux%u\n", encode_param->dst_width,encode_param->dst_height);
            break;
        case THUMB_FACTOR:
            sscanf(value, "%32d", &encode_param->thumb_scaler_factor);
            printf("thumb_scaler_factor: %d\n", encode_param->thumb_scaler_factor);
            break;
        case COMPARE_FILE:
            memset(encode_param->reference_file, 0, sizeof(encode_param->reference_file));
            sscanf(value, "%255s", encode_param->reference_file);
            encode_param->compare_flag = 1;
            printf("get reference file: %s\n", encode_param->reference_file);
            break;
        case FREQUENCY:
            sscanf(value, "%32d", &encode_param->frequency);
            printf("frequency: %d\n", encode_param->frequency);
            break;
        case BIT_RATE:
            sscanf(value, "%32d", &encode_param->bit_rate);
            printf("bit rate: %d\n", encode_param->bit_rate);
            break;
        case FRM_RATE:
            sscanf(value, "%32d", &encode_param->frame_rate);
            printf("frm rate: %d\n", encode_param->frame_rate);
            break;
        case VBV_SIZE:
            sscanf(value, "%32d", &encode_param->vbv_size);
            printf("vbv size: %dbyte\n", encode_param->vbv_size);
            break;
        case GOP_SIZE:
            sscanf(value, "%32d", &ui_val);
            encode_param->h265_func.h265Param.nGopSize = ui_val;
            printf("gop size: %d\n", ui_val);
            break;
        case KEY_FRM_PERIOD:
            sscanf(value, "%32d", &ui_val);
            encode_param->h264_func.h264Param.nMaxKeyInterval = ui_val;
            encode_param->h265_func.h265Param.idr_period = ui_val;
            printf("intra period: %d\n", ui_val);
            break;
        case GOP_MODE:
            sscanf(value, "%32d", &ui_val);
            if(ui_val != 1 && ui_val != 2)
            {
                ui_val = 1;
            }
            encode_param->h264_func.h264Param.sGopParam.eGopMode = ui_val;
            encode_param->h265_func.h265Param.sGopParam.eGopMode = ui_val;
            printf("gop mode: %d 1:NormalP 2:DoubleP\n", ui_val);
            break;
        case RC_MODE:
            sscanf(value, "%32d", &ui_val);
            if(ui_val == 5)
            {
                encode_param->mVbrOptEn = 1;
                ui_val = 1;
            }
            else if(ui_val != 0 && ui_val != 1 && ui_val != 4)
            {
                ui_val = 0;
            }
            encode_param->h264_func.h264Param.sRcParam.eRcMode = ui_val;
            encode_param->h265_func.h265Param.sRcParam.eRcMode = ui_val;
            printf("rc mode: %d 0:CBR 1:VBR 4:FIX_QP 5:NEW_VBR\n", ui_val);
            break;
        case MOVING_TH:
            sscanf(value, "%32d", &ui_val);
            if(ui_val < 1 || 31 < ui_val)
            {
                ui_val = 20;
            }
            encode_param->h264_func.h264Param.sRcParam.sVbrParam.nMovingTh = ui_val;
            encode_param->h265_func.h265Param.sRcParam.sVbrParam.nMovingTh = ui_val;
            printf("moving th: %d\n", ui_val);
            break;
        case QUALITY:
            sscanf(value, "%32d", &ui_val);
            if(encode_param->encode_format == 1)
            {
                if(ui_val < 1 || 100 < ui_val)
                {
                    ui_val = 80;
                }
                encode_param->jpeg_func.quality = ui_val;
            }
            else
            {
                if(ui_val < 1 || 20 < ui_val)
                {
                    ui_val = 10;
                }
                encode_param->h264_func.h264Param.sRcParam.sVbrParam.nQuality = ui_val;
                encode_param->h265_func.h265Param.sRcParam.sVbrParam.nQuality = ui_val;
            }
            printf("quality: %d\n", ui_val);
            break;
        case JPEG_MODE:
            sscanf(value, "%32d", &ui_val);
            if(ui_val < 0 || 1 < ui_val)
            {
                ui_val = 0;
            }
            encode_param->jpeg_func.jpeg_mode = ui_val;
            printf("jpeg_mode: %d\n", ui_val);
            break;
        case PRODUCT_MODE:
            sscanf(value, "%32d", &encode_param->mProductMode.eProductMode);
            if(encode_param->mProductMode.eProductMode < PRODUCT_STATIC_IPC
                || PRODUCT_NUM <= encode_param->mProductMode.eProductMode)
            {
                encode_param->mProductMode.eProductMode = PRODUCT_STATIC_IPC;
            }
            printf("product mode: %d\n", encode_param->mProductMode.eProductMode);
            break;
        case IS_NIGHT:
            sscanf(value, "%32d", &encode_param->is_night);
            if(encode_param->is_night != 0 && encode_param->is_night != 1)
            {
                encode_param->is_night = 0;
            }
            printf("is night case: %d\n", encode_param->is_night);
            break;
        case SENSOR_TYPE:
            sscanf(value, "%32d", &encode_param->sensor_type);
            if(encode_param->sensor_type != 0 && encode_param->sensor_type != 1)
            {
                encode_param->sensor_type = 1;
            }
            printf("sensor type: %d 0:DIS_WDR 1:EN_WDR\n", encode_param->sensor_type);
            break;
        case LOG_FILE:
            memset(encode_param->log_file, 0, sizeof(encode_param->log_file));
            sscanf(value, "%255s", encode_param->log_file);
            encode_param->log_flag = 1;
            printf("get log file: %s\n", encode_param->log_file);
            break;
        case TEST_CYCLE:
            sscanf(value, "%32u", &encode_param->test_cycle);
            printf("test cycle: %u\n", encode_param->test_cycle);
            break;
        case TEST_OVERLAY:
            sscanf(value, "%32u", &encode_param->test_overlay_flag);
            printf("test overlay flag: %u\n", encode_param->test_overlay_flag);
            break;
        case TEST_OVERLAY_IN:
            memset(encode_param->overlay_file, 0, sizeof(encode_param->overlay_file));
            sscanf(value, "%255s", encode_param->overlay_file);
            printf("get overlay file: %s\n", encode_param->overlay_file);
            break;
        case TEST_LBC:
            sscanf(value, "%32u", &encode_param->test_lbc);
            printf("test lbc: %u\n", encode_param->test_lbc);
            break;
        case TEST_AFBC:
            sscanf(value, "%32u", &encode_param->test_afbc);
            printf("test afbc: %u\n", encode_param->test_afbc);
            break;
        case LIMIT_SPEED:
            sscanf(value, "%32u", &encode_param->limit_encode_speed);
            printf("limit speed: %u\n", encode_param->limit_encode_speed);
            break;
        case I_QP_MIN:
            sscanf(value, "%32u", &encode_param->i_qp_min);
            encode_param->h264_func.fixQP.nIQp = encode_param->i_qp_min;
            printf("i_qp_min: %u\n", encode_param->i_qp_min);
            break;
        case I_QP_MAX:
            sscanf(value, "%32u", &encode_param->i_qp_max);
            printf("i_qp_max: %u\n", encode_param->i_qp_max);
            break;
        case P_QP_MIN:
            sscanf(value, "%32u", &encode_param->p_qp_min);
            encode_param->h264_func.fixQP.nPQp = encode_param->p_qp_min;
            printf("p_qp_min: %u\n", encode_param->p_qp_min);
            break;
        case P_QP_MAX:
            sscanf(value, "%32u", &encode_param->p_qp_max);
            printf("p_qp_max: %u\n", encode_param->p_qp_max);
            break;
        case QP_INIT:
            sscanf(value, "%32u", &encode_param->qp_init);
            printf("qp_init: %u\n", encode_param->qp_init);
            break;
        case MB_QP_LIMIT:
            sscanf(value, "%32u", &encode_param->mb_qp_limit);
            printf("mb_qp_limit: %u\n", encode_param->mb_qp_limit);
            break;
        case FIXEL_FORMAT:
            sscanf(value, "%32u", &encode_param->pixel_format);
            printf("pixel_format: %u\n", encode_param->pixel_format);
            break;
        case EXTEND_FLAG:
            sscanf(value, "%32u", &encode_param->extend_flag);
            printf("extend_flag: %u\n", encode_param->extend_flag);
            break;
        case SHARE_BUF_NUM:
            sscanf(value, "%32u", &encode_param->nShare_buf_num);
            printf("nShare_buf_num: %u\n", encode_param->nShare_buf_num);
            break;
        case ENABLE_GDC:
            sscanf(value, "%32u", &encode_param->bEnableGdc);
            printf("enable_gdc: %u\n", encode_param->bEnableGdc);
            break;
#if 0
        case GDC_MODE:
            sscanf(value, "%32u", &encode_param->mGdcParam.gdcMode);
            printf("gdc_mode: %u\n", encode_param->mGdcParam.gdcMode);
            break;
        case ENABLE_SR:
            sscanf(value, "%32u", &encode_param->mGdcParam.bEnableSr);
            printf("enable_sr: %u\n", encode_param->mGdcParam.bEnableSr);
            break;
#endif
        case ENABLE_SHARP:
            sscanf(value, "%32u", &encode_param->bEnableSharp);
            printf("enable_sharp: %u\n", encode_param->bEnableSharp);
            break;
        case ROTATE:
            sscanf(value, "%32u", &encode_param->rotate);
            printf("rotate: %u\n", encode_param->rotate);
            break;
        case REC_LBC_MODE:
            sscanf(value, "%32u", &encode_param->rec_lbc_mode);
            printf("rec_lbc_mode: %u\n", encode_param->rec_lbc_mode);
            break;
        case FORCE_CONF_WIN:
        {
            sscanf(value, "%32u,%32u,%32u,%32u,%32u",
                &encode_param->conf_win.en_force_conf,
                &encode_param->conf_win.left_offset,
                &encode_param->conf_win.right_offset,
                &encode_param->conf_win.top_offset,
                &encode_param->conf_win.bottom_offset);
            printf("force_conf_win: %u, %u, %u, %u, %u\n",
                encode_param->conf_win.en_force_conf,
                encode_param->conf_win.left_offset,
                encode_param->conf_win.right_offset,
                encode_param->conf_win.top_offset,
                encode_param->conf_win.bottom_offset);
            break;
        }
        case ENABLE_CROP:
            sscanf(value, "%32u", &encode_param->en_crop);
            printf("en_crop: %u\n", encode_param->en_crop);
            break;
        case CROP_LEFT:
            sscanf(value, "%32u", &encode_param->crop_l);
            printf("crop_l: %u\n", encode_param->crop_l);
            break;
        case CROP_TOP:
            sscanf(value, "%32u", &encode_param->crop_t);
            printf("crop_t: %u\n", encode_param->crop_t);
            break;
        case CROP_WIDTH:
            sscanf(value, "%32u", &encode_param->crop_w);
            printf("crop_w: %u\n", encode_param->crop_w);
            break;
        case CROP_HEIGHT:
            sscanf(value, "%32u", &encode_param->crop_h);
            printf("crop_h: %u\n", encode_param->crop_h);
            break;
        case SCENE:
        {
            float *scene = encode_param->mTargetBits.nSceneCoef;
            sscanf(value, "%f,%f,%f", &scene[0], &scene[1], &scene[2]);
            printf("scene: %f %f %f\n", scene[0], scene[1], scene[2]);
            break;
        }
        case MOVE:
        {
            float *move = encode_param->mTargetBits.nMoveCoef;
            sscanf(value, "%f,%f,%f,%f,%f", &move[0], &move[1], &move[2], &move[3], &move[4]);
            printf("move: %f %f %f %f %f\n", move[0], move[1], move[2], move[3], move[4]);
            break;
        }
        case SUPER:
        {
            VencSuperFrameConfig *super = &encode_param->mSuperFrame;
            sscanf(value, "%d,%d,%d,%d,%f", (int *)&super->eSuperFrameMode, &super->nMaxIFrameBits,
                &super->nMaxPFrameBits, &super->nMaxRencodeTimes, &super->nMaxP2IFrameBitsRatio);
            super->nMaxIFrameBits <<= 3;
            super->nMaxPFrameBits <<= 3;
            printf("super: %d %d %d %d %f\n", super->eSuperFrameMode, super->nMaxIFrameBits>>3,
                super->nMaxPFrameBits>>3, super->nMaxRencodeTimes, super->nMaxP2IFrameBitsRatio);
            break;
        }
        case BITS_CLIP:
        {
            VencTargetBitsClipParam *clip = &encode_param->mBitsClip;
            sscanf(value, "%d,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f", &clip->mode, &clip->coef_th[0][0], &clip->coef_th[0][1],
                &clip->coef_th[1][0], &clip->coef_th[1][1], &clip->coef_th[2][0], &clip->coef_th[2][1],
                &clip->coef_th[3][0], &clip->coef_th[3][1], &clip->coef_th[4][0], &clip->coef_th[4][1]);
            printf("bits_clip:%d, {%f,%f},{%f,%f},{%f,%f},{%f,%f},{%f,%f}\n", clip->mode, clip->coef_th[0][0], clip->coef_th[0][1],
                clip->coef_th[1][0], clip->coef_th[1][1], clip->coef_th[2][0], clip->coef_th[2][1],
                clip->coef_th[3][0], clip->coef_th[3][1], clip->coef_th[4][0], clip->coef_th[4][1]);
            break;
        }
        case MB_RC_LEVEL:
        {
            sscanf(value, "%d", &encode_param->mb_rc_level);
            printf("mb_rc_level:%d\n", encode_param->mb_rc_level);
            break;
        }
        case D3D:
        {
            s3DfilterParam *d3d = &encode_param->m3DfilterParam;
            int val[8] = {0};
            sscanf(value, "%d,%d,%d,%d,%d,%d,%d,%d", &val[0], &val[1],
                &val[2], &val[3], &val[4], &val[5], &val[6], &val[7]);
            d3d->enable_3d_filter = (unsigned char)val[0];
            d3d->adjust_pix_level_enable = (unsigned char)val[1];
            d3d->smooth_filter_enable = (unsigned char)val[2];
            d3d->max_pix_diff_th = (unsigned char)val[3];
            d3d->max_mad_th = (unsigned char)val[4];
            d3d->max_mv_th = (unsigned char)val[5];
            d3d->max_coef = (unsigned char)val[6];
            d3d->min_coef = (unsigned char)val[7];
            printf("*********** 3DFilter ***********\n");
            printf("enable_3d_filter: %d\n", d3d->enable_3d_filter);
            printf("adjust_pix_level_enable: %d\n", d3d->adjust_pix_level_enable);
            printf("smooth_filter_enable: %d\n", d3d->smooth_filter_enable);
            printf("max_pix_diff_th: %d\n", d3d->max_pix_diff_th);
            printf("max_mad_th: %d\n", d3d->max_mad_th);
            printf("max_mv_th: %d\n", d3d->max_mv_th);
            printf("max_coef: %d\n", d3d->max_coef);
            printf("min_coef: %d\n", d3d->min_coef);
            printf("*********** 3DFilter ***********\n\n");
            break;
        }
        case REGION_D3D:
        {
            VencRegionD3DParam *rdp = &encode_param->mRegionD3DParam;
            int val[15] = {0};
            sscanf(value, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%f,%f,%f", &val[0], &val[1], &val[2], &val[3],
                &val[4], &val[5], &val[6], &val[7], &val[8], &val[9], &val[10], &val[11], &val[12], &val[13], &val[14],
                &rdp->zero_mv_rate_th[0], &rdp->zero_mv_rate_th[1], &rdp->zero_mv_rate_th[2]);
            rdp->en_region_d3d = val[0];
            rdp->dis_default_para = val[1];
            rdp->result_num = val[2];
            rdp->hor_region_num = val[3];
            rdp->ver_region_num = val[4];
            rdp->hor_expand_num = val[5];
            rdp->ver_expand_num = val[6];
            rdp->chroma_offset = val[7];
            rdp->static_coef[0] = (float)val[8];
            rdp->static_coef[1] = (float)val[9];
            rdp->static_coef[2] = (float)val[10];
            rdp->motion_coef[0] = (float)val[11];
            rdp->motion_coef[1] = (float)val[12];
            rdp->motion_coef[2] = (float)val[13];
            rdp->motion_coef[3] = (float)val[14];

            printf("*********** Region3DFilter ***********\n");
            printf("en_region_d3d: %d\n", rdp->en_region_d3d);
            printf("dis_default_para: %d\n", rdp->dis_default_para);
            printf("result_num: %d\n", rdp->result_num);
            printf("hor_region_num: %d\n", rdp->hor_region_num);
            printf("ver_region_num: %d\n", rdp->ver_region_num);
            printf("hor_expand_num: %d\n", rdp->hor_expand_num);
            printf("ver_expand_num: %d\n", rdp->ver_expand_num);
            printf("chroma_offset: %d\n", rdp->chroma_offset);
            printf("static_coef: {%d, %d, %d}\n",
                rdp->static_coef[0], rdp->static_coef[1], rdp->static_coef[2]);
            printf("motion_coef: {%d, %d, %d, %d}\n",
                rdp->motion_coef[0], rdp->motion_coef[1], rdp->motion_coef[2], rdp->motion_coef[3]);
            printf("zero_mv_rate_th: {%.2f, %.2f, %.2f}\n",
                rdp->zero_mv_rate_th[0], rdp->zero_mv_rate_th[1], rdp->zero_mv_rate_th[2]);
            printf("*********** Region3DFilter ***********\n\n");
            break;
        }
        case D2D:
        {
            s2DfilterParam *d2d = &encode_param->m2DfilterParam;
            int val[5] = {0};
            sscanf(value, "%d,%d,%d,%d,%d", &val[0], &val[1], &val[2], &val[3], &val[4]);
            d2d->enable_2d_filter = (unsigned char)val[0];
            d2d->filter_strength_uv = (unsigned char)val[1];
            d2d->filter_strength_y = (unsigned char)val[2];
            d2d->filter_th_uv = (unsigned char)val[3];
            d2d->filter_th_y = (unsigned char)val[4];
            printf("*********** 2DFilter ***********\n");
            printf("enable_2d_filter: %d\n", d2d->enable_2d_filter);
            printf("filter_strength_uv: %d\n", d2d->filter_strength_uv);
            printf("filter_strength_y: %d\n", d2d->filter_strength_y);
            printf("filter_th_uv: %d\n", d2d->filter_th_uv);
            printf("filter_th_y: %d\n", d2d->filter_th_y);
            printf("*********** 2DFilter ***********\n\n");
            break;
        }
        case WEAK_TEXT_TH:
        {
            sscanf(value, "%f", &encode_param->weak_text_th);
            printf("weak_text_th: %.2f\n", encode_param->weak_text_th);
            break;
        }
        case VE2ISP_D2D:
        {
            sscanf(value, "%d,%d,%d,%d,%d,%d,%d", &encode_param->ve2isp_d2d.en_d2d_limit,
                &encode_param->ve2isp_d2d.d2d_level[0],
                &encode_param->ve2isp_d2d.d2d_level[1],
                &encode_param->ve2isp_d2d.d2d_level[2],
                &encode_param->ve2isp_d2d.d2d_level[3],
                &encode_param->ve2isp_d2d.d2d_level[4],
                &encode_param->ve2isp_d2d.d2d_level[5]);
            printf("ve2isp_d2d(%d): {%d, %d, %d, %d, %d, %d}\n",
                encode_param->ve2isp_d2d.en_d2d_limit,
                encode_param->ve2isp_d2d.d2d_level[0],
                encode_param->ve2isp_d2d.d2d_level[1],
                encode_param->ve2isp_d2d.d2d_level[2],
                encode_param->ve2isp_d2d.d2d_level[3],
                encode_param->ve2isp_d2d.d2d_level[4],
                encode_param->ve2isp_d2d.d2d_level[5]);
            break;
        }
        case SEI_NUM:
        {
            sscanf(value, "%d", &encode_param->sei_num);
            printf("sei_num: %d", encode_param->sei_num);
            break;
        }
        case VIDEO_SIGNAL:
        {
            int full_range_flag = 0;
            int video_format = 0;
            int dst_colour_primaries = 0;
            sscanf(value, "%d,%d,%d", &video_format, &full_range_flag, &dst_colour_primaries);
            encode_param->mVideoSignal.video_format = video_format;
            encode_param->mVideoSignal.full_range_flag = (unsigned char)full_range_flag;
            encode_param->mVideoSignal.dst_colour_primaries = dst_colour_primaries;
            encode_param->mVideoSignal.src_colour_primaries = encode_param->mVideoSignal.dst_colour_primaries;
            printf("video_signal: %d, %d, %d, %d\n",
                encode_param->mVideoSignal.video_format,
                encode_param->mVideoSignal.full_range_flag,
                encode_param->mVideoSignal.src_colour_primaries,
                encode_param->mVideoSignal.dst_colour_primaries);
            break;
        }
        case INSTANEOUS_BITRATE:
        {
            int i;
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
            InsideVbrOptPar *pVbrMode = &pVbrOptParam->pVbrOptPar;
            InsideInstaneousBitRatePar *instaneousBR = &pVbrMode->vbrInstaneousBRPar;
			int val[5] = {0}, tmpmaxBitrate = 0;
            float fval[5] = {0.0f};
            float fFactorTh[8] = {0.0f}, fFactor[8] = {0.0f};

			sscanf(value, "%d,%d,%d,%d,%f,%f,%f,%d,\
				%f,%f,%f,%f,%f,%f,%f,%f,\
				%f,%f,%f,%f,%f,%f,%f,%f",
				&val[0], &val[1], &tmpmaxBitrate, &val[2], &fval[1], &fval[2], &fval[3], &val[3],\
				&fFactorTh[0], &fFactorTh[1], &fFactorTh[2], &fFactorTh[3],\
				&fFactorTh[4], &fFactorTh[5], &fFactorTh[6], &fFactorTh[7],\
				&fFactor[0], &fFactor[1], &fFactor[2], &fFactor[3],\
				&fFactor[4], &fFactor[5], &fFactor[6], &fFactor[7]);

            pVbrOptParam->enable_instaneousBR = (unsigned char)val[0];
            pVbrOptParam->recodeIsliceQpEn = (unsigned char)val[1];
			pVbrOptParam->max_instaneousBR = (unsigned int)tmpmaxBitrate;
			pVbrOptParam->peroid_instaneousBR = (unsigned int)val[2];
            instaneousBR->ExceedTarBrRatio = (float)fval[1];
            instaneousBR->SmallTarBrRatio = (float)fval[2];
            instaneousBR->recodeExceedTarBrRatio = (float)fval[3];
            instaneousBR->nFactorLevelNum = (unsigned int)val[3];
            for(i = 0; i < instaneousBR->nFactorLevelNum; i++)
            {
                instaneousBR->fgAdjustFactorTh[i] = (float)fFactorTh[i];
                instaneousBR->fgAdjustFactor[i] = (float)fFactor[i];
            }
            printf("*********** vbr instantaneous bitrate ***********\n");
            printf("enable_instaneousBR: %d\n", pVbrOptParam->enable_instaneousBR);
            printf("recodeIsliceQpEn: %d\n", pVbrOptParam->recodeIsliceQpEn);
			printf("max_instaneousBR: %d\n", pVbrOptParam->max_instaneousBR);
            printf("peroid_instaneousBR: %d\n", pVbrOptParam->peroid_instaneousBR);
            printf("ExceedTarBrRatio: %3.2f\n", instaneousBR->ExceedTarBrRatio);
            printf("SmallTarBrRatio: %3.2f\n",  instaneousBR->SmallTarBrRatio);
            printf("recodeExceedTarBrRatio: %3.2f\n",  instaneousBR->recodeExceedTarBrRatio);
            printf("FactorLevelNum: %d\n", instaneousBR->nFactorLevelNum);
            printf("AdjustFactorTh: [%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f]\n",
                                    instaneousBR->fgAdjustFactorTh[0],instaneousBR->fgAdjustFactorTh[1],
                                    instaneousBR->fgAdjustFactorTh[2],instaneousBR->fgAdjustFactorTh[3],
                                    instaneousBR->fgAdjustFactorTh[4],instaneousBR->fgAdjustFactorTh[5],
                                    instaneousBR->fgAdjustFactorTh[6],instaneousBR->fgAdjustFactorTh[7]);
            printf("AdjustFactor: [%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f]\n",
                                    instaneousBR->fgAdjustFactor[0],instaneousBR->fgAdjustFactor[1],
                                    instaneousBR->fgAdjustFactor[2],instaneousBR->fgAdjustFactor[3],
                                    instaneousBR->fgAdjustFactor[4],instaneousBR->fgAdjustFactor[5],
                                    instaneousBR->fgAdjustFactor[6],instaneousBR->fgAdjustFactor[7]);
            printf("*********** vbr instantaneous bitrate ***********\n");
            break;
        }
        case VBR_MODE_OPTNEW://VBR_MODE_OPT_PARAM
        {
            int i;
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
            InsideVbrOptPar *pVbrMode = &pVbrOptParam->pVbrOptPar;
            InsideInstaneousBitRatePar *instaneousBR = &pVbrMode->vbrInstaneousBRPar;
            InsideVbrKeyPar *pKeyParam = &pVbrMode->vbrKeyPar;
            int val[24] = {0}, MvTh[8] = {0};
            float fval[16] = {0.0f};

            sscanf(value,
                "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\
                %d,%d,%d,%d,%d,%d,\
                %d,%d,%d,%d,%d,%d,%d,%d,\
                %f,%f,%f,%f,%f,%f,\
                %f,%f,%f,%f,%d,%d",
                    &val[0],  &val[1],  &val[2],  &val[3],  &val[4],  &val[5],  &val[6],  &val[7],  &val[8],  &val[9],\
                    &val[10], &val[11], &val[16], &val[17], &val[18],&val[19],\
                    &MvTh[0],  &MvTh[1],  &MvTh[2],  &MvTh[3],  &MvTh[4],  &MvTh[5],  &MvTh[6],  &MvTh[7],\
                    &fval[0],  &fval[1],  &fval[2],  &fval[3],  &fval[4],  &fval[5],\
                    &fval[6],  &fval[7],  &fval[8],  &fval[9],&val[20],&val[21]);
            pKeyParam->uVbrOptEn                = (unsigned int)val[0];
            pKeyParam->uPrintVbrTraceInfoEn     = (unsigned int)val[1];
            pKeyParam->uPrintLogInfoEn          = (unsigned int)val[2];
            pKeyParam->PrintRegInfoFrameNum     = (int)val[4];
            pVbrOptParam->nIPratio                 = (int)val[5];
            pVbrOptParam->nIntraPeriodNumInVbv     = (int)val[6];
            pKeyParam->nIFrameRcInfoNum         = (int)val[7];
            pKeyParam->nPFrameRcInfoNum         = (int)val[8];
            pKeyParam->nSaveBitQpTh             = (int)val[9];
            pKeyParam->nBoostBitQpTh            = (int)val[10];
            pKeyParam->nBppLevelNum             = (int)val[11];
            pKeyParam->nIPSliceQpMaxGap         = (int)val[16];
            pKeyParam->nMvStaLevelNum           = (int)val[17];
            pKeyParam->nAvgMvFactor             = (int)val[18];
            pKeyParam->nAvgQpRatio              = (int)val[19];
            for(i = 0; i < 8; i++)
                 pKeyParam->nStaMvTh[i]              = (int)MvTh[i];
            pKeyParam->fInitAvgMovingLevel   = (float)fval[0];
            pKeyParam->fAvgMoveLevelRatio    = (float)fval[1];
            pKeyParam->fMinRatio[0]          = (float)fval[2];
            pKeyParam->fMinRatio[1]          = (float)fval[3];
            pKeyParam->fMaxRatio[0]          = (float)fval[4];
            pKeyParam->fMaxRatio[1]          = (float)fval[5];
            pKeyParam->fMinRaioTh[0]         = (float)fval[6];
            pKeyParam->fMinRaioTh[1]         = (float)fval[7];
            pKeyParam->fMaxRatioTh[0]        = (float)fval[8];
            pKeyParam->fMaxRatioTh[1]        = (float)fval[9];

            pKeyParam->nMaxHistoryFrameNum[0] = (int)val[20];
            pKeyParam->nMaxHistoryFrameNum[1] = (int)val[21];
            printf("*********** vbr opt param ***********\n");
            printf("VbrOptEn: %d\n", pKeyParam->uVbrOptEn);
            printf("PrintVbrTraceInfoEn: %d, PrintLogInfoEn: %d, PrintRegInfoFrameNum: %d\n",
                pKeyParam->uPrintVbrTraceInfoEn,pKeyParam->uPrintLogInfoEn,pKeyParam->PrintRegInfoFrameNum);
            printf("IPratio: %d,IntraPeriodNumInVbv: %d\n", pVbrOptParam->nIPratio, pVbrOptParam->nIntraPeriodNumInVbv);
            printf("IFrameRcInfoNum: %d,PFrameRcInfoNum: %d\n", pKeyParam->nIFrameRcInfoNum, pKeyParam->nPFrameRcInfoNum);
            printf("SaveBitQpTh: %d,BoostBitQpTh: %d\n", pKeyParam->nSaveBitQpTh, pKeyParam->nBoostBitQpTh);
            printf("BppLevelNum: %d,IPSliceQpMaxGap: %d\n", pKeyParam->nBppLevelNum,pKeyParam->nIPSliceQpMaxGap);
            printf("MvStaLevelNum: %d,AvgMvFactor: %d,AvgQpRatio: %d\n", pKeyParam->nMvStaLevelNum, pKeyParam->nAvgMvFactor, pKeyParam->nAvgQpRatio);
            printf("StaMvTh: [%d,%d,%d,%d,%d,%d,%d,%d]\n", pKeyParam->nStaMvTh[0],pKeyParam->nStaMvTh[1],pKeyParam->nStaMvTh[2],pKeyParam->nStaMvTh[3],
                                                        pKeyParam->nStaMvTh[4],pKeyParam->nStaMvTh[5],pKeyParam->nStaMvTh[6],pKeyParam->nStaMvTh[7]);
            printf("InitAvgMovingLevel: %3.2f,AvgMoveLevelRatio: %3.2f\n",
                                                        pKeyParam->fInitAvgMovingLevel, pKeyParam->fAvgMoveLevelRatio);
            printf("MinRatio: [%3.2f,%3.2f]\n", pKeyParam->fMinRatio[0], pKeyParam->fMinRatio[1]);
            printf("MaxRatio: [%3.2f,%3.2f]\n", pKeyParam->fMaxRatio[0], pKeyParam->fMaxRatio[1]);
            printf("MinRaioTh: [%3.2f,%3.2f]\n", pKeyParam->fMinRaioTh[0], pKeyParam->fMinRaioTh[1]);
            printf("MaxRatioTh: [%3.2f,%3.2f]\n", pKeyParam->fMaxRatioTh[0], pKeyParam->fMaxRatioTh[1]);
            printf("*********** vbr opt param ***********\n");
            break;
        }
        case BPP_LEVEL:
        {
            int i, j;
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
            InsideVbrOptPar *pVbrMode = &pVbrOptParam->pVbrOptPar;
            InsideVbrKeyPar *pKeyParam = &pVbrMode->vbrKeyPar;
            float BppLevel[16] = {0.0f};
            sscanf(value, "%f,%f,%f,%f,%f,%f,%f,%f,\
                    %f,%f,%f,%f,%f,%f,%f,%f",
                    &BppLevel[0],  &BppLevel[1],  &BppLevel[2],  &BppLevel[3],\
                    &BppLevel[4],  &BppLevel[5],  &BppLevel[6],  &BppLevel[7],\
                    &BppLevel[8],  &BppLevel[9],  &BppLevel[10], &BppLevel[11],\
                    &BppLevel[12], &BppLevel[13], &BppLevel[14], &BppLevel[15]);
            for(j = 0; j < 2; j++){
                for(i = 0; i < 8; i++)
                {
                    pKeyParam->fBppLevel[j][i] = (float)BppLevel[j * 8 + i];
                }
            }
            printf("*********** vbr opt bpp level ***********\n");
            for(i = 0; i < 2; i++){
                if(i == 0)
                    printf("I slice ");
                else
                    printf("P slice ");
                printf("BppLevel: [%4.3f,%4.3f,%4.3f,%4.3f,%4.3f,%4.3f,%4.3f,%4.3f]\n",
                                pKeyParam->fBppLevel[i][0],pKeyParam->fBppLevel[i][1],pKeyParam->fBppLevel[i][2],pKeyParam->fBppLevel[i][3],
                                pKeyParam->fBppLevel[i][4],pKeyParam->fBppLevel[i][5],pKeyParam->fBppLevel[i][6],pKeyParam->fBppLevel[i][7]);
            }
            printf("*********** vbr opt bpp level ***********\n");
            break;
        }
        case BPP_ADJUSTQPTH:
        {
            int i, j;
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
            InsideVbrOptPar *pVbrMode = &pVbrOptParam->pVbrOptPar;
            InsideVbrKeyPar *pKeyParam = &pVbrMode->vbrKeyPar;
            float AdjustQpBppTh[16] = {0.0f};
            sscanf(value, "%f,%f,%f,%f,%f,%f,%f,%f,\
                    %f,%f,%f,%f,%f,%f,%f,%f",
                    &AdjustQpBppTh[0],  &AdjustQpBppTh[1],  &AdjustQpBppTh[2],  &AdjustQpBppTh[3],\
                    &AdjustQpBppTh[4],  &AdjustQpBppTh[5],  &AdjustQpBppTh[6],  &AdjustQpBppTh[7],\
                    &AdjustQpBppTh[8],  &AdjustQpBppTh[9],  &AdjustQpBppTh[10], &AdjustQpBppTh[11],\
                    &AdjustQpBppTh[12], &AdjustQpBppTh[13], &AdjustQpBppTh[14], &AdjustQpBppTh[15]);
            for(j = 0; j < 2; j++){
                for(i = 0; i < 8; i++)
                {
                    pKeyParam->fAdjustQpBppTh[j][i] = (float)AdjustQpBppTh[j * 8 + i];
                }
            }
            printf("*********** vbr opt Adjust Qp BppTh ***********\n");
            for(i = 0; i < 2; i++){
                if(i == 0)
                    printf("I slice ");
                else
                    printf("P slice ");
                printf("AdjustQpBppTh: [%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f]\n",
                            pKeyParam->fAdjustQpBppTh[i][0],pKeyParam->fAdjustQpBppTh[i][1],pKeyParam->fAdjustQpBppTh[i][2],pKeyParam->fAdjustQpBppTh[i][3],
                            pKeyParam->fAdjustQpBppTh[i][4],pKeyParam->fAdjustQpBppTh[i][5],pKeyParam->fAdjustQpBppTh[i][6],pKeyParam->fAdjustQpBppTh[i][7]);
            }
            printf("*********** vbr opt Adjust Qp BppTh ***********\n");
            break;
        }
        case BPP_ADJUSTMAXQP:
        {
            int i, j;
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
            InsideVbrOptPar *pVbrMode = &pVbrOptParam->pVbrOptPar;
            InsideVbrKeyPar *pKeyParam = &pVbrMode->vbrKeyPar;
            float AdjustMaxQp[16] = {0.0f};
            sscanf(value, "%f,%f,%f,%f,%f,%f,%f,%f,\
                    %f,%f,%f,%f,%f,%f,%f,%f",
                    &AdjustMaxQp[0],  &AdjustMaxQp[1],  &AdjustMaxQp[2],  &AdjustMaxQp[3],\
                    &AdjustMaxQp[4],  &AdjustMaxQp[5],  &AdjustMaxQp[6],  &AdjustMaxQp[7],\
                    &AdjustMaxQp[8],  &AdjustMaxQp[9],  &AdjustMaxQp[10], &AdjustMaxQp[11],\
                    &AdjustMaxQp[12], &AdjustMaxQp[13], &AdjustMaxQp[14], &AdjustMaxQp[15]);
            for(j = 0; j < 2; j++){
                for(i = 0; i < 8; i++)
                {
                    pKeyParam->fAdjustMaxQp[j][i] = (float)AdjustMaxQp[j * 8 + i];
                }
            }
            printf("*********** vbr opt Adjust Qp BppTh ***********\n");
            for(i = 0; i < 2; i++){
                if(i == 0)
                    printf("I slice ");
                else
                    printf("P slice ");
                printf("AdjustMaxQp: [%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f]\n",
                        pKeyParam->fAdjustMaxQp[i][0],pKeyParam->fAdjustMaxQp[i][1],pKeyParam->fAdjustMaxQp[i][2],pKeyParam->fAdjustMaxQp[i][3],
                        pKeyParam->fAdjustMaxQp[i][4],pKeyParam->fAdjustMaxQp[i][5],pKeyParam->fAdjustMaxQp[i][6],pKeyParam->fAdjustMaxQp[i][7]);
            }
            printf("*********** vbr opt Adjust Qp BppTh ***********\n");
            break;
        }
        case SAVE_BITRATE:
        {
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
            InsideVbrOptPar *pVbrMode = &pVbrOptParam->pVbrOptPar;
            InsideVbrKeyPar *pKeyParam = &pVbrMode->vbrKeyPar;
            int val[4] = {0}, madTh[12] = {0};
            float fval[3] = {0.0f};
            sscanf(value, "%d,%d,%f,%f,%f",
                  &val[0], &val[1],&fval[0],&fval[1],&fval[2]);
            pVbrOptParam->uSaveBitRateEn       = (unsigned int)val[0];
            pKeyParam->nDeltaQp             = (int)val[1];
            pKeyParam->fMoveFrameLevelTh    = (float)fval[0];
            pKeyParam->fStaticFrameLevelTh  = (float)fval[1];
            pKeyParam->fluctuationFactorTh  = (float)fval[2];
            printf("*********** save bitrate ***********\n");
            printf("SaveBitRateEn: %d, DeltaQp: %d\n",
                pVbrOptParam->uSaveBitRateEn, pKeyParam->nDeltaQp);
            printf("MoveFrameLevelTh: %3.2f, SmallMvLevelRatioTh: %3.2f, FlutuationFactorTh: %3.2f\n",
                    pKeyParam->fMoveFrameLevelTh, pKeyParam->fStaticFrameLevelTh, pKeyParam->fluctuationFactorTh);
            printf("*********** save bitrate ***********\n");
            break;
        }
        case MOSAIC_OPT:
        {
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
            InsideVbrOptPar *pVbrMode = &pVbrOptParam->pVbrOptPar;
            InsideVbrKeyPar *pKeyParam = &pVbrMode->vbrKeyPar;
            InsideVbrMosAicOptPar *pMosAic = &pVbrMode->vbrMosAicOptPar;
            InsideVbrMoveToStaticPar *pMoveToStatic = &pVbrMode->vbrMoveToStaticPar;
            int val[17] = {0};
            float fVal = 0.0f;
            sscanf(value, "%d,%d,%d,%d,%d,%d,%d,%d,%f,",
                  &val[0], &val[1], &val[2],&val[3],&val[4],\
                  &val[5], &val[6], &val[7],&fVal);
            pVbrOptParam->uMosAicOptEn            = (unsigned int)val[0];
            pMosAic->uConstantLargeMoveNumTh = (unsigned int)val[1];
            pMosAic->uConstantStaticNumTh    = (unsigned int)val[2];
            pMosAic->uMethodSelect           = (unsigned int)val[3];
            pMosAic->nMosAicSliceDeltaQp     = (int)val[4];
            pMosAic->uAdjustSliceQpPeriod    = (unsigned int)val[5];
            pMosAic->uSliceQpLimitTh         = (unsigned int)val[6];
            pMosAic->nScaleDeltaRatio        = (float)fVal;
            pMoveToStatic->uConstandMoveToStaticNumTh = (unsigned int)val[7];

            printf("*********** mosaic opt param ***********\n");
            printf("MosAicOptEn: %d\n", pVbrOptParam->uMosAicOptEn);
            printf("ConstantStaticNumTh: %d, ConstantLargeMoveNumTh: %d\n",
                pMosAic->uConstantStaticNumTh, pMosAic->uConstantLargeMoveNumTh);
            printf("MethodSelect: %d, MosAicSliceDeltaQp: %d, ScaleDeltaRatio: %f\n",
                pMosAic->uMethodSelect, pMosAic->nMosAicSliceDeltaQp,pMosAic->nScaleDeltaRatio);
            printf("uSliceQpLimitTh: %d, uAdjustSliceQpPeriod: %d,uConstandMoveToStaticNumTh: %d\n",
                pMosAic->uSliceQpLimitTh, pMosAic->uAdjustSliceQpPeriod,pMoveToStatic->uConstandMoveToStaticNumTh);
            printf("*********** mosaic opt param ***********\n");
            break;
        }
        case MOVETOSTATIC_OPT:
        {
            int i;
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
            InsideVbrOptPar *pVbrMode = &pVbrOptParam->pVbrOptPar;
            InsideVbrMoveToStaticPar *pMoveToStatic = &pVbrMode->vbrMoveToStaticPar;
            float ratio[8] = {0.0f};
            int val[16] = {0};
            sscanf(value, "%d,%f,%f,%f,%f,%f,%f,%f,%f,\
                  %d,%d,%d,%d,%d,%d,\
                  %d,%d,%d,%d,%d,%d",
                  &val[0], &ratio[0], &ratio[1], &ratio[2],&ratio[3],&ratio[4], &ratio[5], &ratio[6], &ratio[7],\
                  &val[3], &val[4], &val[5], &val[6], &val[7], &val[8],\
                  &val[9], &val[10],&val[11],&val[12],&val[13], &val[14]);
            pVbrOptParam->uMoveToStaticOptEn = (unsigned int)val[0];
            for(i = 0; i < 8; i++)
            {
                pMoveToStatic->fFrameMvLevelTh[i] = (float)ratio[i];
                pMoveToStatic->nMoveToStaticSliceQpDelta[i] = (int)val[i + 3];
            }
            for(i = 0; i < 4; i++)
            {
                pMoveToStatic->nMoveToStaticDelayNum[i] = (int)val[i + 11];
            }
            printf("*********** movetostatic param ***********\n");
            printf("MoveToStaticOpt: %d\n",pVbrOptParam->uMoveToStaticOptEn);
            printf("FrameMvLevelTh[%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f,%3.2f]\n",
                    pMoveToStatic->fFrameMvLevelTh[0],pMoveToStatic->fFrameMvLevelTh[1],\
                    pMoveToStatic->fFrameMvLevelTh[2],pMoveToStatic->fFrameMvLevelTh[3],\
                    pMoveToStatic->fFrameMvLevelTh[4],pMoveToStatic->fFrameMvLevelTh[5],\
                    pMoveToStatic->fFrameMvLevelTh[6],pMoveToStatic->fFrameMvLevelTh[7]);
            printf("MoveToStaticSliceQpDelta[%d,%d,%d,%d,%d,%d,%d,%d]\n",
                    pMoveToStatic->nMoveToStaticSliceQpDelta[0],pMoveToStatic->nMoveToStaticSliceQpDelta[1],\
                    pMoveToStatic->nMoveToStaticSliceQpDelta[2],pMoveToStatic->nMoveToStaticSliceQpDelta[3],\
                    pMoveToStatic->nMoveToStaticSliceQpDelta[4],pMoveToStatic->nMoveToStaticSliceQpDelta[5],\
                    pMoveToStatic->nMoveToStaticSliceQpDelta[6],pMoveToStatic->nMoveToStaticSliceQpDelta[7]);
            printf("MoveToStaticDelayNum[%d,%d,%d,%d]\n",
                    pMoveToStatic->nMoveToStaticDelayNum[0],pMoveToStatic->nMoveToStaticDelayNum[1],\
                    pMoveToStatic->nMoveToStaticDelayNum[2],pMoveToStatic->nMoveToStaticDelayNum[3]);
            printf("*********** movetostatic param ***********\n");
            break;
        }
        case FRAME_BITRATE:
        {
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
			unsigned int val = 0;
			sscanf(value, "%d,", &val);
			pVbrOptParam->max_instaneousBR            = (unsigned int)val;
			printf("max_instaneousBR: %d\n", pVbrOptParam->max_instaneousBR);
			break;
        }
		case VCU:
		{
			sscanf(value, "%d,", &encode_param->bvcu);
			printf("bvcu: %d\n", encode_param->bvcu);
			break;
		}
        case FRAME_MAD_TH:
        {
            int i;
            VencVbrOptParam *pVbrOptParam = &encode_param->mVbrOptParam;
            InsideVbrOptPar *pVbrMode = &pVbrOptParam->pVbrOptPar;
            InsideVbrKeyPar *pKeyParam = &pVbrMode->vbrKeyPar;
            InsideClipQpFunc *pClipQpFunc = &pVbrMode->mclipQpOpt;
            int valI[12] = {0},valP[12] = {0};
            int val2[5] = {0};
            int ClassifyMadThOptEn = 0;
            sscanf(value, "%d,%d,%d,%d,%d,%d,%d,\
                           %d,%d,%d,%d,%d,%d,\
                           %d,%d,%d,%d,%d,%d,\
                           %d,%d,%d,%d,%d,%d,\
                           %d,%d,%d,%d,%d",\
                           &ClassifyMadThOptEn,
                           &valI[0], &valI[1], &valI[2], &valI[3], &valI[4],&valI[5], \
                           &valI[6], &valI[7], &valI[8], &valI[9], &valI[10],&valI[11],\
                           &valP[0], &valP[1], &valP[2], &valP[3], &valP[4], &valP[5], \
                           &valP[6], &valP[7], &valP[8], &valP[9], &valP[10],&valP[11],\
                           &val2[0],&val2[1],&val2[2],&val2[3],&val2[4]);
            pVbrOptParam->uClassifyMadThAdjustEn = (unsigned int)ClassifyMadThOptEn;
            for(i = 0; i < 12; i++)
            {
                pKeyParam->uIframeMadThOpt[i]            = (unsigned char)valI[i];
                pKeyParam->uPframeMadThOpt[i]            = (unsigned char)valP[i];
            }
            pClipQpFunc->closeMbRcEn          = (unsigned int)val2[0];
            pClipQpFunc->MoveStatusTh         = (unsigned int)val2[1];
            pClipQpFunc->MbQpTightLimitEn     = (unsigned int)val2[2];
            pClipQpFunc->uAddDeltaQp          = (int)val2[3];
            pClipQpFunc->uSliceQpMaxTh        = (unsigned int)val2[4];
            printf("*********** frame madTh param ***********\n");
            printf("ClassifyMadThAdjustEn: %d\n",pVbrOptParam->uClassifyMadThAdjustEn);
            printf("I_FrameMadTh: [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]\n",
                pKeyParam->uIframeMadThOpt[0],pKeyParam->uIframeMadThOpt[1],pKeyParam->uIframeMadThOpt[2],pKeyParam->uIframeMadThOpt[3],\
                pKeyParam->uIframeMadThOpt[4],pKeyParam->uIframeMadThOpt[5],pKeyParam->uIframeMadThOpt[6],pKeyParam->uIframeMadThOpt[7],\
                pKeyParam->uIframeMadThOpt[8],pKeyParam->uIframeMadThOpt[9],pKeyParam->uIframeMadThOpt[10],pKeyParam->uIframeMadThOpt[11]);
            printf("P_FrameMadTh: [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]\n",
                pKeyParam->uPframeMadThOpt[0],pKeyParam->uPframeMadThOpt[1],pKeyParam->uPframeMadThOpt[2],pKeyParam->uPframeMadThOpt[3],\
                pKeyParam->uPframeMadThOpt[4],pKeyParam->uPframeMadThOpt[5],pKeyParam->uPframeMadThOpt[6],pKeyParam->uPframeMadThOpt[7],\
                pKeyParam->uPframeMadThOpt[8],pKeyParam->uPframeMadThOpt[9],pKeyParam->uPframeMadThOpt[10],pKeyParam->uPframeMadThOpt[11]);
            printf("uclipQpFunc: [%d,%d,%d,%d,%d]\n",
                pClipQpFunc->closeMbRcEn, pClipQpFunc->MoveStatusTh,\
                pClipQpFunc->MbQpTightLimitEn, pClipQpFunc->uAddDeltaQp,\
                pClipQpFunc->uSliceQpMaxTh);
            printf("*********** frame madTh param ***********\n");
            break;
        }

        case INVALID:
        default:
            logd("unknowed argument :  %s\n", argument);
            break;
    }
}

int SeekPrefixNAL(char* begin)
{
    unsigned int i;
    char* pchar = begin;
    char isPrefixNAL = 1;
    char NAL[4] = {0x00, 0x00, 0x00, 0x01};

    if(!pchar)
    {
        return -1;
    }

    for(i=0; i<4; i++)
    {
        if(pchar[i] != NAL[i])
        {
            isPrefixNAL = 0;
            break;
        }
    }
    if(isPrefixNAL == 1)
    {
        isPrefixNAL = 0;
        char PrefixNAL[3] = {0x6e, 0x4e, 0x0e};
        for(i=0; i<3; i++)
        {
            if(pchar[4] == PrefixNAL[i])
            {
                isPrefixNAL = 1;
                break;
            }
        }
    }
    // read temporal_id
    if(isPrefixNAL == 1)
    {
        char TemporalID = pchar[7];
        TemporalID >>= 5;
        return TemporalID;
    }

    return -1;
}

void init_jpeg_exif(EXIFInfo *exifinfo)
{
    exifinfo->ThumbWidth = 320;
    exifinfo->ThumbHeight = 240;

    strcpy((char*)exifinfo->CameraMake,        "allwinner make test");
    strcpy((char*)exifinfo->CameraModel,        "allwinner model test");
    strcpy((char*)exifinfo->DateTime,         "2014:02:21 10:54:05");
    strcpy((char*)exifinfo->gpsProcessingMethod,  "allwinner gps");

    exifinfo->Orientation = 0;

    exifinfo->ExposureTime.num = 2;
    exifinfo->ExposureTime.den = 1000;

    exifinfo->ExposureProgram = EXPOSURE_PROGRAM_MANUAL;

    exifinfo->FNumber.num = 20;
    exifinfo->FNumber.den = 10;
    exifinfo->ISOSpeed = 50;

    exifinfo->ExposureBiasValue.num= -4;
    exifinfo->ExposureBiasValue.den= 1;

    exifinfo->MeteringMode = METERING_MODE_PARTIAL;

    exifinfo->LightSource = LIGHT_SOURCE_SUNLIGHT;

    exifinfo->FlashUsed = FLASH_NO_FUNCTION;

    exifinfo->FocalLength.num = 1400;
    exifinfo->FocalLength.den = 100;

    exifinfo->DigitalZoomRatio.num = 4;
    exifinfo->DigitalZoomRatio.den = 1;
    exifinfo->resolution_x.num = 12;
    exifinfo->resolution_x.den = 1;
    exifinfo->resolution_y.num = 12;
    exifinfo->resolution_y.den = 1;


    exifinfo->WhiteBalance = 1;
    exifinfo->ExposureMode = EXPOSURE_MODE_MANUAL;

    exifinfo->FocalLengthIn35mmFilm = 1;
    exifinfo->Contrast = CONTRAST_SOFT;
    exifinfo->Saturation = SATRATION_LOW;
    exifinfo->Sharpness = SHARPNESS_SOFT;

    exifinfo->enableGpsInfo = 1;

    exifinfo->gps_latitude = 23.2368;
    exifinfo->gps_longitude = 24.3244;
    exifinfo->gps_altitude = 1234.5;

    exifinfo->gps_timestamp = (long)time(NULL);

    strcpy((char*)exifinfo->CameraSerialNum,  "123456789");
    strcpy((char*)exifinfo->ImageName,  "exif-name-test");
    strcpy((char*)exifinfo->ImageDescription,  "exif-descriptor-test");
}

void init_h265_gop(VencH265GopStruct *h265Gop)
{
    h265Gop->gop_size = 8;
    h265Gop->intra_period = 16;

    //h265Gop->use_lt_ref_flag = 1;
    if(h265Gop->use_lt_ref_flag)
    {
        h265Gop->max_num_ref_pics = 2;
        h265Gop->num_ref_idx_l0_default_active = 2;
        h265Gop->num_ref_idx_l1_default_active = 2;
        h265Gop->use_sps_rps_flag = 0;
    }
    else
    {
        h265Gop->max_num_ref_pics = 1;
        h265Gop->num_ref_idx_l0_default_active = 1;
        h265Gop->num_ref_idx_l1_default_active = 1;
        h265Gop->use_sps_rps_flag = 1;
    }
    //1:user config the reference info; 0:encoder config the reference info
    h265Gop->custom_rps_flag = 0;
}

void init_mb_mode(VencMBModeCtrl *pMBMode, unsigned int dst_width, unsigned int dst_height)
{
    unsigned int mb_num;
    unsigned int j;
    mb_num = (ALIGN_XXB(16, dst_width) >> 4)
                * (ALIGN_XXB(16, dst_height) >> 4);

    logd("set mb mode");
    if(pMBMode->p_map_info == NULL)
    {
        pMBMode->p_map_info = MALLOC(sizeof(VencMBModeCtrlInfo) * mb_num);
        pMBMode->mode_ctrl_en = 1;
    }

    #if 1
    int mb_en = 0;
    int mb_skip_flag = 0;
    int mb_qp = 0;
    for (j = 0; j < mb_num / 2; j++)
    {
        mb_en = 1;
        mb_skip_flag = 0;
        mb_qp = 22;
        pMBMode->p_map_info[j] = (unsigned char)((mb_en << 7) | (mb_skip_flag << 6) | mb_qp);
    }
    for (; j < mb_num; j++)
    {
        mb_en = 1;
        mb_skip_flag = 0;
        mb_qp = 32;
        pMBMode->p_map_info[j] = (unsigned char)((mb_en << 7) | (mb_skip_flag << 6) | mb_qp);
    }
    #endif
}

void init_mb_info(VencMBInfo *MBInfo, encode_param_t *encode_param)
{
    if(encode_param->encode_format == VENC_CODEC_H265)
    {
        MBInfo->num_mb = (ALIGN_XXB(32, encode_param->dst_width) *
                            ALIGN_XXB(32, encode_param->dst_height)) >> 10;
    }
    else
    {
        MBInfo->num_mb = (ALIGN_XXB(16, encode_param->dst_width) *
                            ALIGN_XXB(16, encode_param->dst_height)) >> 8;
    }
    MBInfo->p_para = (VencMBInfoPara *)MALLOC(sizeof(VencMBInfoPara) * MBInfo->num_mb);
    if(MBInfo->p_para == NULL)
    {
        loge("malloc MBInfo->p_para error\n");
        return;
    }
    logd("mb_num:%d, mb_info_queue_addr:%p\n", MBInfo->num_mb, MBInfo->p_para);
}


void init_fix_qp(VencFixQP *fixQP, int bEnable)
{
    fixQP->bEnable = bEnable;
    if(fixQP->nIQp < 1 || 51 < fixQP->nIQp)
    {
        fixQP->nIQp = 35;
    }
    if(fixQP->nPQp < 1 || 51 < fixQP->nPQp)
    {
        fixQP->nPQp = 35;
    }
}

void init_super_frame_cfg(VencSuperFrameConfig *sSuperFrameCfg)
{
    sSuperFrameCfg->eSuperFrameMode = VENC_SUPERFRAME_NONE;
    sSuperFrameCfg->nMaxIFrameBits = 30000*8;
    sSuperFrameCfg->nMaxPFrameBits = 15000*8;
}

void init_svc_skip(VencH264SVCSkip *SVCSkip)
{
    SVCSkip->nTemporalSVC = T_LAYER_4;
    switch(SVCSkip->nTemporalSVC)
    {
        case T_LAYER_4:
            SVCSkip->nSkipFrame = SKIP_8;
            break;
        case T_LAYER_3:
            SVCSkip->nSkipFrame = SKIP_4;
            break;
        case T_LAYER_2:
            SVCSkip->nSkipFrame = SKIP_2;
            break;
        default:
            SVCSkip->nSkipFrame = NO_SKIP;
            break;
    }
}

void init_aspect_ratio(VencH264AspectRatio *sAspectRatio)
{
    sAspectRatio->aspect_ratio_idc = 255;
    sAspectRatio->sar_width = 4;
    sAspectRatio->sar_height = 3;
}

void init_video_signal(VencH264VideoSignal *sVideoSignal)
{
    sVideoSignal->video_format = 5;
    sVideoSignal->src_colour_primaries = 0;
    sVideoSignal->dst_colour_primaries = 1;
}

void init_intra_refresh(VencCyclicIntraRefresh *sIntraRefresh)
{
    sIntraRefresh->bEnable = 1;
    sIntraRefresh->nBlockNumber = 10;
}

void init_roi(VencROIConfig *sRoiConfig)
{
    sRoiConfig[0].bEnable = 1;
    sRoiConfig[0].index = 0;
    sRoiConfig[0].nQPoffset = 10;
    sRoiConfig[0].sRect.nLeft = 0;
    sRoiConfig[0].sRect.nTop = 0;
    sRoiConfig[0].sRect.nWidth = 1280;
    sRoiConfig[0].sRect.nHeight = 320;

    sRoiConfig[1].bEnable = 1;
    sRoiConfig[1].index = 1;
    sRoiConfig[1].nQPoffset = 10;
    sRoiConfig[1].sRect.nLeft = 320;
    sRoiConfig[1].sRect.nTop = 180;
    sRoiConfig[1].sRect.nWidth = 320;
    sRoiConfig[1].sRect.nHeight = 180;

    sRoiConfig[2].bEnable = 1;
    sRoiConfig[2].index = 2;
    sRoiConfig[2].nQPoffset = 10;
    sRoiConfig[2].sRect.nLeft = 320;
    sRoiConfig[2].sRect.nTop = 180;
    sRoiConfig[2].sRect.nWidth = 320;
    sRoiConfig[2].sRect.nHeight = 180;

    sRoiConfig[3].bEnable = 1;
    sRoiConfig[3].index = 3;
    sRoiConfig[3].nQPoffset = 10;
    sRoiConfig[3].sRect.nLeft = 320;
    sRoiConfig[3].sRect.nTop = 180;
    sRoiConfig[3].sRect.nWidth = 320;
    sRoiConfig[3].sRect.nHeight = 180;
}

void init_alter_frame_rate_info(VencAlterFrameRateInfo *pAlterFrameRateInfo)
{
    memset(pAlterFrameRateInfo, 0 , sizeof(VencAlterFrameRateInfo));
    pAlterFrameRateInfo->bEnable = 1;
    pAlterFrameRateInfo->bUseUserSetRoiInfo = 1;
    pAlterFrameRateInfo->sRoiBgFrameRate.nSrcFrameRate = 25;
    pAlterFrameRateInfo->sRoiBgFrameRate.nDstFrameRate = 5;

    pAlterFrameRateInfo->roi_param[0].bEnable = 1;
    pAlterFrameRateInfo->roi_param[0].index = 0;
    pAlterFrameRateInfo->roi_param[0].nQPoffset = 10;
    pAlterFrameRateInfo->roi_param[0].roi_abs_flag = 1;
    pAlterFrameRateInfo->roi_param[0].sRect.nLeft = 0;
    pAlterFrameRateInfo->roi_param[0].sRect.nTop = 0;
    pAlterFrameRateInfo->roi_param[0].sRect.nWidth = 320;
    pAlterFrameRateInfo->roi_param[0].sRect.nHeight = 320;

    pAlterFrameRateInfo->roi_param[1].bEnable = 1;
    pAlterFrameRateInfo->roi_param[1].index = 0;
    pAlterFrameRateInfo->roi_param[1].nQPoffset = 10;
    pAlterFrameRateInfo->roi_param[1].roi_abs_flag = 1;
    pAlterFrameRateInfo->roi_param[1].sRect.nLeft = 320;
    pAlterFrameRateInfo->roi_param[1].sRect.nTop = 320;
    pAlterFrameRateInfo->roi_param[1].sRect.nWidth = 320;
    pAlterFrameRateInfo->roi_param[1].sRect.nHeight = 320;
}

void init_enc_proc_info(VeProcSet *ve_proc_set)
{
    ve_proc_set->bProcEnable = 1;
    ve_proc_set->nProcFreq = 3;
}

void init_overlay_info(VencOverlayInfoS *pOverlayInfo, encode_param_t *encode_param)
{
    int i;
    unsigned char num_bitMap = 4;
    BitMapInfoS* pBitMapInfo;
    unsigned int time_id_list[19];
    unsigned int start_mb_x;
    unsigned int start_mb_y;

    memset(pOverlayInfo, 0, sizeof(VencOverlayInfoS));

#if 0
    char filename[64];
    int ret;
    for(i = 0; i < num_bitMap; i++)
    {
        FILE_STRUCT* icon_hdle = NULL;
        int width  = 0;
        int height = 0;

        sprintf(filename, "%s%d.bmp", "/mnt/libcedarc/bitmap/icon_720p_",i);

        icon_hdle   = fopen(filename, "r");
        if (icon_hdle == NULL) {
            printf("get wartermark %s error\n", filename);
            return;
        }

        //get watermark picture size
        fseek(icon_hdle, 18, SEEK_SET);
        fread(&width, 1, 4, icon_hdle);
        fread(&height, 1, 4, icon_hdle);

        fseek(icon_hdle, 54, SEEK_SET);

        bit_map_info[i].argb_addr = NULL;
        bit_map_info[i].width = 0;
        bit_map_info[i].height = 0;

        bit_map_info[i].width = width;
        bit_map_info[i].height = height*(-1);

        bit_map_info[i].width_aligh16 = ALIGN_XXB(16, bit_map_info[i].width);
        bit_map_info[i].height_aligh16 = ALIGN_XXB(16, bit_map_info[i].height);
        if(bit_map_info[i].argb_addr == NULL) {
            bit_map_info[i].argb_addr =
            (unsigned char*)MALLOC(bit_map_info[i].width_aligh16*bit_map_info[i].height_aligh16*4);

            if(bit_map_info[i].argb_addr == NULL)
            {
                loge("malloc bit_map_info[%d].argb_addr fail\n", i);
                return;
            }
        }
        logd("bitMap[%d] size[%d,%d], size_align16[%d, %d], argb_addr:%p\n", i,
                                    bit_map_info[i].width,
                                    bit_map_info[i].height,
                                    bit_map_info[i].width_aligh16,
                                    bit_map_info[i].height_aligh16,
                                    bit_map_info[i].argb_addr);

        ret = fread(bit_map_info[i].argb_addr, 1,
            bit_map_info[i].width*bit_map_info[i].height*4, icon_hdle);
        if(ret != bit_map_info[i].width*bit_map_info[i].height*4)
            loge("read bitMap[%d] error, ret value:%d\n", i, ret);

        bit_map_info[i].size = bit_map_info[i].width_aligh16 * bit_map_info[i].height_aligh16 * 4;

        if (icon_hdle) {
            fclose(icon_hdle);
            icon_hdle = NULL;
        }
    }

    //time 2017-04-27 18:28:26
    time_id_list[0] = 2;
    time_id_list[1] = 0;
    time_id_list[2] = 1;
    time_id_list[3] = 7;
    time_id_list[4] = 11;
    time_id_list[5] = 0;
    time_id_list[6] = 4;
    time_id_list[7] = 11;
    time_id_list[8] = 2;
    time_id_list[9] = 7;
    time_id_list[10] = 10;
    time_id_list[11] = 1;
    time_id_list[12] = 8;
    time_id_list[13] = 12;
    time_id_list[14] = 2;
    time_id_list[15] = 8;
    time_id_list[16] = 12;
    time_id_list[17] = 2;
    time_id_list[18] = 6;

    logd("pOverlayInfo:%p\n", pOverlayInfo);
    pOverlayInfo->blk_num = 19;
#else
        FILE_STRUCT* icon_hdle = NULL;
        int width  = 160;
        int height = 16;
        int pix_size = 4;
        memset(time_id_list, 0 ,sizeof(time_id_list));

        icon_hdle = fopen(encode_param->overlay_file, "rb");
        logd("icon_hdle = %p",icon_hdle);
        if (icon_hdle == NULL) {
            logd("get icon_hdle error\n");
            return;
        }

        pOverlayInfo->argb_type = VENC_OVERLAY_ARGB1555;
        pix_size = pOverlayInfo->argb_type == VENC_OVERLAY_ARGB8888 ? 4 : 2;

        for(i = 0; i < num_bitMap; i++)
        {
            bit_map_info[i].argb_addr = NULL;
            bit_map_info[i].width = width;
            bit_map_info[i].height = height;

            bit_map_info[i].width_aligh16 = ALIGN_XXB(16, bit_map_info[i].width);
            bit_map_info[i].height_aligh16 = ALIGN_XXB(16, bit_map_info[i].height);
            if(bit_map_info[i].argb_addr == NULL) {
                bit_map_info[i].argb_addr =
            (unsigned char*)MALLOC(bit_map_info[i].width_aligh16 * bit_map_info[i].height_aligh16 * pix_size);

                if(bit_map_info[i].argb_addr == NULL)
                {
                    loge("malloc bit_map_info[%d].argb_addr fail\n", i);
                    if (icon_hdle) {
                        fclose(icon_hdle);
                        icon_hdle = NULL;
                    }

                    return;
                }
            }
            logd("bitMap[%d] size[%d,%d], size_align16[%d, %d], argb_addr:%p\n", i,
                                                        bit_map_info[i].width,
                                                        bit_map_info[i].height,
                                                        bit_map_info[i].width_aligh16,
                                                        bit_map_info[i].height_aligh16,
                                                        bit_map_info[i].argb_addr);

            int ret;
            int rd_len = bit_map_info[i].width * bit_map_info[i].height * pix_size;
             ret = fread(bit_map_info[i].argb_addr, 1, rd_len, icon_hdle);
            if(ret != rd_len)
            loge("read bitMap[%d] error, ret value:%d\n", i, ret);

            bit_map_info[i].size = rd_len;
            fseek(icon_hdle, 0, SEEK_SET);
        }
        if (icon_hdle) {
            fclose(icon_hdle);
            icon_hdle = NULL;
        }
#endif

        pOverlayInfo->blk_num = num_bitMap;
        logd("blk_num:%d, argb_type:%d\n", pOverlayInfo->blk_num, pOverlayInfo->argb_type);
        pOverlayInfo->invert_threshold = 200;
        pOverlayInfo->invert_mode = 3;

        start_mb_x = 0;
        start_mb_y = 0;
        for(i=0; i<pOverlayInfo->blk_num; i++)
        {
            //id = time_id_list[i];
            //pBitMapInfo = &bit_map_info[id];
            pBitMapInfo = &bit_map_info[i];

            pOverlayInfo->overlayHeaderList[i].start_mb_x = start_mb_x;
            pOverlayInfo->overlayHeaderList[i].start_mb_y = start_mb_y;
            pOverlayInfo->overlayHeaderList[i].end_mb_x = start_mb_x
                                        + (pBitMapInfo->width_aligh16 / 16 - 1);
            pOverlayInfo->overlayHeaderList[i].end_mb_y = start_mb_y
                                        + (pBitMapInfo->height_aligh16 / 16 -1);

            pOverlayInfo->overlayHeaderList[i].extra_alpha_flag = 0;
            pOverlayInfo->overlayHeaderList[i].extra_alpha = 8;
            if(i%3 == 0)
                pOverlayInfo->overlayHeaderList[i].overlay_type = LUMA_REVERSE_OVERLAY;
            else if(i%2 == 0 && i!=0)
                pOverlayInfo->overlayHeaderList[i].overlay_type = COVER_OVERLAY;
            else
                pOverlayInfo->overlayHeaderList[i].overlay_type = NORMAL_OVERLAY;

            pOverlayInfo->overlayHeaderList[i].overlay_type = LUMA_REVERSE_OVERLAY;
            pOverlayInfo->overlayHeaderList[i].reverse_unit_mb_w_minus1 = 0;
            pOverlayInfo->overlayHeaderList[i].reverse_unit_mb_h_minus1 = 0;
            pOverlayInfo->overlayHeaderList[i].bforce_reverse_flag = 0;

            if(pOverlayInfo->overlayHeaderList[i].overlay_type == COVER_OVERLAY)
            {
                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_y = 0xff;
                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_u = 0xff;
                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_v = 0xff;
            }

            //pOverlayInfo->overlayHeaderList[i].bforce_reverse_flag = 1;

            pOverlayInfo->overlayHeaderList[i].overlay_blk_addr = pBitMapInfo->argb_addr;
            pOverlayInfo->overlayHeaderList[i].bitmap_size = pBitMapInfo->size;

            logv("blk_%d[%d], start_mb[%d,%d], end_mb[%d,%d],extra_alpha_flag:%d, extra_alpha:%d\n",
                                i,
                                time_id_list[i],
                                pOverlayInfo->overlayHeaderList[i].start_mb_x,
                                pOverlayInfo->overlayHeaderList[i].start_mb_y,
                                pOverlayInfo->overlayHeaderList[i].end_mb_x,
                                pOverlayInfo->overlayHeaderList[i].end_mb_y,
                                pOverlayInfo->overlayHeaderList[i].extra_alpha_flag,
                                pOverlayInfo->overlayHeaderList[i].extra_alpha);
            logv("overlay_type:%d, cover_yuv[%d,%d,%d], overlay_blk_addr:%p, bitmap_size:%d\n",
                                pOverlayInfo->overlayHeaderList[i].overlay_type,
                                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_y,
                                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_u,
                                pOverlayInfo->overlayHeaderList[i].cover_yuv.cover_v,
                                pOverlayInfo->overlayHeaderList[i].overlay_blk_addr,
                                pOverlayInfo->overlayHeaderList[i].bitmap_size);
            //if(i != 5)
            {
                start_mb_x += pBitMapInfo->width_aligh16 / 16;
                start_mb_y += pBitMapInfo->height_aligh16 / 16;
            }
        }

    return;
}

typedef struct {
    MEMOPS_STRUCT *memops;
    VEOPS_STRUCT *veOpsS;
    void *pVeOpsSelf;
} Encpp_Ops_Set;

int init_Roi_config(VencBaseConfig *baseConfig, VencCopyROIConfig*pRoiConfig, Encpp_Ops_Set *p_ops_set, int b_encpp_func)
{
    unsigned int i, j, idx, num, nyLen;
    MEMOPS_STRUCT *_memops = NULL;
    void *veOps = NULL;
    void *pVeopsSelf = NULL;

    if (b_encpp_func) {
        _memops = p_ops_set->memops;
        veOps = (void *)p_ops_set->veOpsS;
        pVeopsSelf = p_ops_set->pVeOpsSelf;
    } else {
        _memops = baseConfig->memops;
        veOps = (void *)baseConfig->veOpsS;
        pVeopsSelf = baseConfig->pVeOpsSelf;
    }

    nyLen = 0;
    num = 16;
    pRoiConfig->bEnable = 1;
    pRoiConfig->num = num;

    for(i=0; i<4; i++)
    {
        for(j=0; j<4; j++)
        {
            idx = i*4 + j;
            pRoiConfig->sRect[idx].nTop = 270*i;
            pRoiConfig->sRect[idx].nLeft = 430*j;
            pRoiConfig->sRect[idx].nWidth= 320;
            pRoiConfig->sRect[idx].nHeight = 240;
            //nyLen += ((640+15)&(~15))*((480+15)&(~15));
            nyLen += ALIGN_16B(pRoiConfig->sRect[idx].nWidth) * ALIGN_16B(pRoiConfig->sRect[idx].nHeight);
        }
    }
    pRoiConfig->size = nyLen * 3 / 2;
    pRoiConfig->pRoiYAddrVir = (unsigned char*)EncAdapterMemPalloc(nyLen);
    if(pRoiConfig->pRoiYAddrVir == NULL)
    {
        loge("error:palloc roi Y buffer error,return -1");
        return -1;
    }
    memset(pRoiConfig->pRoiYAddrVir, 0x80, nyLen);
    EncAdapterMemFlushCache(pRoiConfig->pRoiYAddrVir, nyLen);
    pRoiConfig->pRoiYAddrPhy =
			(unsigned long)EncAdapterMemGetPhysicAddress(pRoiConfig->pRoiYAddrVir);

    pRoiConfig->pRoiCAddrVir = (unsigned char*)EncAdapterMemPalloc(nyLen/2);
    if(pRoiConfig->pRoiCAddrVir == NULL)
    {
        loge("error:palloc roi C buffer error,return -1");
        EncAdapterMemPfree(pRoiConfig->pRoiYAddrVir);
        pRoiConfig->pRoiYAddrVir= NULL;
        return -1;
    }
    memset(pRoiConfig->pRoiCAddrVir, 0x80, nyLen/2);
    EncAdapterMemFlushCache(pRoiConfig->pRoiCAddrVir, nyLen/2);
    pRoiConfig->pRoiCAddrPhy =
			(unsigned long)EncAdapterMemGetPhysicAddress(pRoiConfig->pRoiCAddrVir);

    logd("pRoiConfig->size=%d", pRoiConfig->size);
    logd("pRoiYAddrVir=%p, pRoiCAddrVir=%p", pRoiConfig->pRoiYAddrVir, pRoiConfig->pRoiCAddrVir);
    logd("pRoiYAddrPhy=%lx, pRoiCAddrPhy=%lx", pRoiConfig->pRoiYAddrPhy, pRoiConfig->pRoiCAddrPhy);
    return 0;
}



void init_jpeg_rate_ctrl(jpeg_func_t *jpeg_func)
{
    jpeg_func->jpeg_biteRate = 12*1024*1024;
    jpeg_func->jpeg_frameRate = 30;
    jpeg_func->bitRateRange.bitRateMax = 14*1024*1024;
    jpeg_func->bitRateRange.bitRateMin = 10*1024*1024;
}

int initH264Func(encode_param_t *encode_param)
{
    h264_func_t *h264_func = &encode_param->h264_func;

    //init h264Param
#ifdef SET_MB_INFO
    h264_func->h264Param.sRcParam.eRcMode = AW_QPMAP;
#endif
    h264_func->h264Param.bEntropyCodingCABAC = 1;
    h264_func->h264Param.nBitrate = encode_param->bit_rate;
    h264_func->h264Param.nFramerate = encode_param->frame_rate;
    h264_func->h264Param.nCodingMode = VENC_FRAME_CODING;
    h264_func->h264Param.sProfileLevel.nProfile = VENC_H264ProfileHigh;
    h264_func->h264Param.sProfileLevel.nLevel = VENC_H264Level51;
     if(h264_func->h264Param.nMaxKeyInterval == 0)
    {
        h264_func->h264Param.nMaxKeyInterval = 30;
    }

    h264_func->h264Param.sQPRange.bEnMbQpLimit = encode_param->mb_qp_limit;
    if(0 < encode_param->i_qp_min && encode_param->i_qp_max < 52
        && encode_param->i_qp_min < encode_param->i_qp_max)
    {
        h264_func->h264Param.sQPRange.nMinqp = encode_param->i_qp_min;
        h264_func->h264Param.sQPRange.nMaxqp = encode_param->i_qp_max;

    }
    else
    {
        h264_func->h264Param.sQPRange.nMinqp = 10;
        h264_func->h264Param.sQPRange.nMaxqp = 50;
    }

    if(0 < encode_param->p_qp_min && encode_param->p_qp_max < 52
        && encode_param->p_qp_min < encode_param->p_qp_max)
    {
        h264_func->h264Param.sQPRange.nMinPqp = encode_param->p_qp_min;
        h264_func->h264Param.sQPRange.nMaxPqp = encode_param->p_qp_max;

    }
    else
    {
        h264_func->h264Param.sQPRange.nMinPqp = 10;
        h264_func->h264Param.sQPRange.nMaxPqp = 50;
    }

    if(0 < encode_param->qp_init && encode_param->qp_init < 52)
    {
        h264_func->h264Param.sQPRange.nQpInit = encode_param->qp_init;
    }
    else
    {
        h264_func->h264Param.sQPRange.nQpInit = 30;
    }

    //h264_func->h264Param.bLongRefEnable = 1;
    //h264_func->h264Param.nLongRefPoc = 0;

#if 0
    h264_func->sH264Smart.img_bin_en = 1;
    h264_func->sH264Smart.img_bin_th = 27;
    h264_func->sH264Smart.shift_bits = 2;
    h264_func->sH264Smart.smart_fun_en = 1;
#endif

    //init VencH264FixQP
    init_fix_qp(&h264_func->fixQP, h264_func->h264Param.sRcParam.eRcMode == AW_FIXQP);

    //init VencSuperFrameConfig
    init_super_frame_cfg(&h264_func->sSuperFrameCfg);

    //init VencH264SVCSkip
    init_svc_skip(&h264_func->SVCSkip);

    //init VencH264AspectRatio
    init_aspect_ratio(&h264_func->sAspectRatio);

    //init VencH264AspectRatio
    init_video_signal(&h264_func->sVideoSignal);

    //init CyclicIntraRefresh
    init_intra_refresh(&h264_func->sIntraRefresh);

    //init VencROIConfig
    init_roi(h264_func->sRoiConfig);

    //init proc info
    init_enc_proc_info(&h264_func->sVeProcInfo);

    //init VencOverlayConfig
    if(encode_param->test_overlay_flag)
    {
        init_overlay_info(&h264_func->sOverlayInfo, encode_param);
    }

    return 0;
}

int initH265Func(encode_param_t *encode_param)
{
    h265_func_t *h265_func = &encode_param->h265_func;

#ifdef SET_MB_INFO
    h265_func->h265Param.sRcParam.eRcMode = AW_QPMAP;
#endif
    h265_func->h265Param.nBitrate = encode_param->bit_rate;
    h265_func->h265Param.nFramerate = encode_param->frame_rate;
    h265_func->h265Param.sProfileLevel.nProfile = VENC_H265ProfileMain;
    h265_func->h265Param.sProfileLevel.nLevel = VENC_H265Level41;
    h265_func->h265Param.nQPInit = 30;
    if(h265_func->h265Param.idr_period == 0)
    {
        h265_func->h265Param.idr_period = 30;
    }
    if(h265_func->h265Param.nGopSize == 0)
    {
        h265_func->h265Param.nGopSize = h265_func->h265Param.idr_period;
    }
    h265_func->h265Param.nIntraPeriod = h265_func->h265Param.idr_period;

    h265_func->h265Param.sQPRange.bEnMbQpLimit = encode_param->mb_qp_limit;
    if(0 < encode_param->i_qp_min && encode_param->i_qp_max < 52
        && encode_param->i_qp_min < encode_param->i_qp_max)
    {
        h265_func->h265Param.sQPRange.nMaxqp = encode_param->i_qp_max;
        h265_func->h265Param.sQPRange.nMinqp = encode_param->i_qp_min;
    }
    else
    {
        h265_func->h265Param.sQPRange.nMaxqp = 52;
        h265_func->h265Param.sQPRange.nMinqp = 10;
    }

    if(0 < encode_param->p_qp_min && encode_param->p_qp_max < 52
        && encode_param->p_qp_min < encode_param->p_qp_max)
    {
        h265_func->h265Param.sQPRange.nMinPqp = encode_param->p_qp_min;
        h265_func->h265Param.sQPRange.nMaxPqp = encode_param->p_qp_max;

    }
    else
    {
        h265_func->h265Param.sQPRange.nMinPqp = 10;
        h265_func->h265Param.sQPRange.nMaxPqp = 50;
    }

    if(0 < encode_param->qp_init && encode_param->qp_init < 52)
    {
        h265_func->h265Param.sQPRange.nQpInit = encode_param->qp_init;
    }
    else
    {
        h265_func->h265Param.sQPRange.nQpInit = 30;
    }

    //h265_func->h265Param.bLongTermRef = 1;

#if 0
    h265_func->h265Hvs.hvs_en = 1;
    h265_func->h265Hvs.th_dir = 24;
    h265_func->h265Hvs.th_coef_shift = 4;

    h265_func->h265Trc.inter_tend = 63;
    h265_func->h265Trc.skip_tend = 3;
    h265_func->h265Trc.merge_tend = 0;

    h265_func->h265Smart.img_bin_en = 1;
    h265_func->h265Smart.img_bin_th = 27;
    h265_func->h265Smart.shift_bits = 2;
    h265_func->h265Smart.smart_fun_en = 1;
#endif

    h265_func->h265_rc_frame_total = 20*h265_func->h265Param.nGopSize;

    //init H265Gop
    //init_h265_gop(&h265_func->h265Gop);

    //init VencH264FixQP
    init_fix_qp(&h265_func->fixQP, h265_func->h265Param.sRcParam.eRcMode == AW_FIXQP);

    //init VencSuperFrameConfig
    init_super_frame_cfg(&h265_func->sSuperFrameCfg);

    //init VencH264SVCSkip
    init_svc_skip(&h265_func->SVCSkip);

    //init VencH264AspectRatio
    init_aspect_ratio(&h265_func->sAspectRatio);

    //init VencH264AspectRatio
    init_video_signal(&h265_func->sVideoSignal);

    //init CyclicIntraRefresh
    init_intra_refresh(&h265_func->sIntraRefresh);

    //init VencROIConfig
    init_roi(h265_func->sRoiConfig);

    //init alter frameRate info
    init_alter_frame_rate_info(&h265_func->sAlterFrameRateInfo);

    //init proc info
    init_enc_proc_info(&h265_func->sVeProcInfo);

    //init VencOverlayConfig
    if(encode_param->test_overlay_flag)
    {
        init_overlay_info(&h265_func->sOverlayInfo, encode_param);
    }

    return 0;
}

int initJpegFunc(jpeg_func_t *jpeg_func, encode_param_t *encode_param)
{
    if(jpeg_func->quality < 1 || 100 < jpeg_func->quality)
        jpeg_func->quality = 80;
    if(0 == jpeg_func->jpeg_mode)
    {
        init_jpeg_exif(&jpeg_func->exifinfo);
    }
    else if(1 == jpeg_func->jpeg_mode)
    {
        init_jpeg_rate_ctrl(jpeg_func);
    }
    else
    {
        loge("encoder do not support the jpeg_mode:%d\n", jpeg_func->jpeg_mode);
        return -1;
    }

    //init VencOverlayConfig
    if(encode_param->test_overlay_flag)
    {
        init_overlay_info(&jpeg_func->sOverlayInfo, encode_param);
    }

    return 0;

}

static int initGdcFunc(sGdcParam *pGdcParam)
{
    pGdcParam->bGDC_en = 1;
    pGdcParam->eWarpMode = Gdc_Warp_LDC;
    pGdcParam->eMountMode = Gdc_Mount_Wall;
    pGdcParam->bMirror = 0;
    pGdcParam->calib_widht  = 3264;
    pGdcParam->calib_height = 2448;

    pGdcParam->fx = 2417.19;
    pGdcParam->fy = 2408.43;
    pGdcParam->cx = 1631.50;
    pGdcParam->cy = 1223.50;
    pGdcParam->fx_scale = 2161.82;
    pGdcParam->fy_scale = 2153.99;
    pGdcParam->cx_scale = 1631.50;
    pGdcParam->cy_scale = 1223.50;

    pGdcParam->eLensDistModel = Gdc_DistModel_FishEye;

    pGdcParam->distCoef_wide_ra[0] = -0.3849;
    pGdcParam->distCoef_wide_ra[1] = 0.1567;
    pGdcParam->distCoef_wide_ra[2] = -0.0030;
    pGdcParam->distCoef_wide_ta[0] = -0.00005;
    pGdcParam->distCoef_wide_ta[1] = 0.0016;

    pGdcParam->distCoef_fish_k[0]  = -0.0024;
    pGdcParam->distCoef_fish_k[1]  = 0.141;
    pGdcParam->distCoef_fish_k[2]  = -0.3;
    pGdcParam->distCoef_fish_k[3]  = 0.2328;

    pGdcParam->centerOffsetX         =      0;
    pGdcParam->centerOffsetY         =      0;
    pGdcParam->rotateAngle           =      0;     //[0,360]
    pGdcParam->radialDistortCoef     =      0;     //[-255,255]
    pGdcParam->trapezoidDistortCoef  =      0;     //[-255,255]
    pGdcParam->fanDistortCoef        =      0;     //[-255,255]
    pGdcParam->pan                   =      0;     //pano360:[0,360]; others:[-90,90]
    pGdcParam->tilt                  =      0;     //[-90,90]
    pGdcParam->zoomH                 =      100;   //[0,100]
    pGdcParam->zoomV                 =      100;   //[0,100]
    pGdcParam->scale                 =      100;   //[0,100]
    pGdcParam->innerRadius           =      0;     //[0,width/2]
    pGdcParam->roll                  =      0;     //[-90,90]
    pGdcParam->pitch                 =      0;     //[-90,90]
    pGdcParam->yaw                   =      0;     //[-90,90]

    pGdcParam->perspFunc             =    Gdc_Persp_Only;
    pGdcParam->perspectiveProjMat[0] =    1.0;
    pGdcParam->perspectiveProjMat[1] =    0.0;
    pGdcParam->perspectiveProjMat[2] =    0.0;
    pGdcParam->perspectiveProjMat[3] =    0.0;
    pGdcParam->perspectiveProjMat[4] =    1.0;
    pGdcParam->perspectiveProjMat[5] =    0.0;
    pGdcParam->perspectiveProjMat[6] =    0.0;
    pGdcParam->perspectiveProjMat[7] =    0.0;
    pGdcParam->perspectiveProjMat[8] =    1.0;

    pGdcParam->mountHeight           =      0.85; //meters
    pGdcParam->roiDist_ahead         =      4.5;  //meters
    pGdcParam->roiDist_left          =     -1.5;  //meters
    pGdcParam->roiDist_right         =      1.5;  //meters
    pGdcParam->roiDist_bottom        =      0.65; //meters

    pGdcParam->peaking_en            =      1;    //0/1
    pGdcParam->peaking_clamp         =      1;    //0/1
    pGdcParam->peak_m                =     16;    //[0,63]
    pGdcParam->th_strong_edge        =      6;    //[0,15]
    pGdcParam->peak_weights_strength =      2;    //[0,15]

    if(pGdcParam->eWarpMode == Gdc_Warp_LDC)
    {
        pGdcParam->birdsImg_width    = 768;
        pGdcParam->birdsImg_height   = 1080;
    }

    return 0;
}

static int initSharpFunc(    sEncppSharpParamDynamic *pSharpParamDynamic, sEncppSharpParamStatic  *pSharpParamStatic)
{
    pSharpParamDynamic->ss_ns_lw  = 0;           //[0,255];
    pSharpParamDynamic->ss_ns_hi  = 0;           //[0,255];
    pSharpParamDynamic->ls_ns_lw  = 0;           //[0,255];
    pSharpParamDynamic->ls_ns_hi  = 0;           //[0,255];
    pSharpParamDynamic->ss_lw_cor = 0;           //[0,255];
    pSharpParamDynamic->ss_hi_cor = 0;           //[0,255];
    pSharpParamDynamic->ls_lw_cor = 0;           //[0,255];
    pSharpParamDynamic->ls_hi_cor = 0;           //[0,255];
    pSharpParamDynamic->ss_blk_stren = 256;      //[0,4095];
    pSharpParamDynamic->ss_wht_stren = 256;      //[0,4095];
    pSharpParamDynamic->ls_blk_stren = 256;      //[0,4095];
    pSharpParamDynamic->ls_wht_stren = 256;      //[0,4095];
    pSharpParamDynamic->wht_clp_para = 256;       //[0,1023];
    pSharpParamDynamic->blk_clp_para = 256;       //[0,1023];
    pSharpParamDynamic->ss_avg_smth  = 0;         //[0,255];
    pSharpParamDynamic->hfr_mf_blk_stren  = 0;    //[0,4095];
    pSharpParamDynamic->hfr_hf_blk_stren  = 0;    //[0,4095];
    pSharpParamDynamic->hfr_hf_wht_clp  = 32;     //[0,255];
    pSharpParamDynamic->hfr_hf_cor_ratio  = 0;    //[0,255];
    pSharpParamDynamic->hfr_mf_mix_ratio  = 390;  //[0,1023];
    pSharpParamDynamic->ss_dir_smth  = 0;       //[0,16];
    pSharpParamDynamic->wht_clp_slp  = 16;        //[0,63];
    pSharpParamDynamic->blk_clp_slp  = 8;         //[0,63];
    pSharpParamDynamic->max_clp_ratio  = 64;        //[0,255];
    pSharpParamDynamic->hfr_hf_blk_clp  = 32;     //[0,255];
    pSharpParamDynamic->hfr_smth_ratio  = 0;      //[0,32];
    pSharpParamDynamic->dir_smth[0] = 0;         //[0,16];
    pSharpParamDynamic->dir_smth[1] = 0;         //[0,16];
    pSharpParamDynamic->dir_smth[2] = 0;         //[0,16];
    pSharpParamDynamic->dir_smth[3] = 0;         //[0,16];
    pSharpParamStatic->ss_shp_ratio = 0;           //[0,255];
    pSharpParamStatic->ls_shp_ratio = 0;           //[0,255];
    pSharpParamStatic->ss_dir_ratio = 98;          //[0,1023];
    pSharpParamStatic->ls_dir_ratio = 90;          //[0,1023];
    pSharpParamStatic->ss_crc_stren = 128;        //[0,1023];
    pSharpParamStatic->ss_crc_min   = 16;         //[0,255];
    pSharpParamDynamic->hfr_mf_blk_clp  = 32;     //[0,255];
    pSharpParamDynamic->hfr_mf_wht_clp  = 32;     //[0,255];
    pSharpParamDynamic->hfr_hf_wht_stren = 0;    //[0,4095];
    pSharpParamDynamic->hfr_mf_wht_stren = 0;    //[0,4095];
    pSharpParamDynamic->hfr_mf_cor_ratio = 0;    //[0,255];
    pSharpParamDynamic->hfr_hf_mix_ratio = 390;  //[0,1023];
    pSharpParamDynamic->hfr_hf_mix_min_ratio = 0;   //[0,255];
    pSharpParamDynamic->hfr_mf_mix_min_ratio = 0;   //[0,255];

    pSharpParamStatic->sharp_ss_value[0]=384;
    pSharpParamStatic->sharp_ss_value[1]=416;
    pSharpParamStatic->sharp_ss_value[2]=471;
    pSharpParamStatic->sharp_ss_value[3]=477;
    pSharpParamStatic->sharp_ss_value[4]=443;
    pSharpParamStatic->sharp_ss_value[5]=409;
    pSharpParamStatic->sharp_ss_value[6]=374;
    pSharpParamStatic->sharp_ss_value[7]=340;
    pSharpParamStatic->sharp_ss_value[8]=306;
    pSharpParamStatic->sharp_ss_value[9]=272;
    pSharpParamStatic->sharp_ss_value[10]=237;
    pSharpParamStatic->sharp_ss_value[11]=203;
    pSharpParamStatic->sharp_ss_value[12]=169;
    pSharpParamStatic->sharp_ss_value[13]=134;
    pSharpParamStatic->sharp_ss_value[14]=100;
    pSharpParamStatic->sharp_ss_value[15]=66;
    pSharpParamStatic->sharp_ss_value[16]=41;
    pSharpParamStatic->sharp_ss_value[17]=32;
    pSharpParamStatic->sharp_ss_value[18]=32;
    pSharpParamStatic->sharp_ss_value[19]=32;
    pSharpParamStatic->sharp_ss_value[20]=32;
    pSharpParamStatic->sharp_ss_value[21]=32;
    pSharpParamStatic->sharp_ss_value[22]=32;
    pSharpParamStatic->sharp_ss_value[23]=32;
    pSharpParamStatic->sharp_ss_value[24]=32;
    pSharpParamStatic->sharp_ss_value[25]=32;
    pSharpParamStatic->sharp_ss_value[26]=32;
    pSharpParamStatic->sharp_ss_value[27]=32;
    pSharpParamStatic->sharp_ss_value[28]=32;
    pSharpParamStatic->sharp_ss_value[29]=32;
    pSharpParamStatic->sharp_ss_value[30]=32;
    pSharpParamStatic->sharp_ss_value[31]=32;
    pSharpParamStatic->sharp_ss_value[32]=32;

    pSharpParamStatic->sharp_ls_value[0]=384;
    pSharpParamStatic->sharp_ls_value[1]=395;
    pSharpParamStatic->sharp_ls_value[2]=427;
    pSharpParamStatic->sharp_ls_value[3]=470;
    pSharpParamStatic->sharp_ls_value[4]=478;
    pSharpParamStatic->sharp_ls_value[5]=416;
    pSharpParamStatic->sharp_ls_value[6]=320;
    pSharpParamStatic->sharp_ls_value[7]=224;
    pSharpParamStatic->sharp_ls_value[8]=152;
    pSharpParamStatic->sharp_ls_value[9]=128;
    pSharpParamStatic->sharp_ls_value[10]=128;
    pSharpParamStatic->sharp_ls_value[11]=128;
    pSharpParamStatic->sharp_ls_value[12]=128;
    pSharpParamStatic->sharp_ls_value[13]=128;
    pSharpParamStatic->sharp_ls_value[14]=128;
    pSharpParamStatic->sharp_ls_value[15]=128;
    pSharpParamStatic->sharp_ls_value[16]=128;
    pSharpParamStatic->sharp_ls_value[17]=128;
    pSharpParamStatic->sharp_ls_value[18]=128;
    pSharpParamStatic->sharp_ls_value[19]=128;
    pSharpParamStatic->sharp_ls_value[20]=128;
    pSharpParamStatic->sharp_ls_value[21]=128;
    pSharpParamStatic->sharp_ls_value[22]=128;
    pSharpParamStatic->sharp_ls_value[23]=128;
    pSharpParamStatic->sharp_ls_value[24]=128;
    pSharpParamStatic->sharp_ls_value[25]=128;
    pSharpParamStatic->sharp_ls_value[26]=128;
    pSharpParamStatic->sharp_ls_value[27]=128;
    pSharpParamStatic->sharp_ls_value[28]=128;
    pSharpParamStatic->sharp_ls_value[29]=128;
    pSharpParamStatic->sharp_ls_value[30]=128;
    pSharpParamStatic->sharp_ls_value[31]=128;
    pSharpParamStatic->sharp_ls_value[32]=128;

    pSharpParamStatic->sharp_hsv[0]=218;
    pSharpParamStatic->sharp_hsv[1]=206;
    pSharpParamStatic->sharp_hsv[2]=214;
    pSharpParamStatic->sharp_hsv[3]=247;
    pSharpParamStatic->sharp_hsv[4]=282;
    pSharpParamStatic->sharp_hsv[5]=299;
    pSharpParamStatic->sharp_hsv[6]=308;
    pSharpParamStatic->sharp_hsv[7]=316;
    pSharpParamStatic->sharp_hsv[8]=325;
    pSharpParamStatic->sharp_hsv[9]=333;
    pSharpParamStatic->sharp_hsv[10]=342;
    pSharpParamStatic->sharp_hsv[11]=350;
    pSharpParamStatic->sharp_hsv[12]=359;
    pSharpParamStatic->sharp_hsv[13]=367;
    pSharpParamStatic->sharp_hsv[14]=376;
    pSharpParamStatic->sharp_hsv[15]=380;
    pSharpParamStatic->sharp_hsv[16]=375;
    pSharpParamStatic->sharp_hsv[17]=366;
    pSharpParamStatic->sharp_hsv[18]=358;
    pSharpParamStatic->sharp_hsv[19]=349;
    pSharpParamStatic->sharp_hsv[20]=341;
    pSharpParamStatic->sharp_hsv[21]=332;
    pSharpParamStatic->sharp_hsv[22]=324;
    pSharpParamStatic->sharp_hsv[23]=315;
    pSharpParamStatic->sharp_hsv[24]=307;
    pSharpParamStatic->sharp_hsv[25]=298;
    pSharpParamStatic->sharp_hsv[26]=290;
    pSharpParamStatic->sharp_hsv[27]=281;
    pSharpParamStatic->sharp_hsv[28]=273;
    pSharpParamStatic->sharp_hsv[29]=264;
    pSharpParamStatic->sharp_hsv[30]=258;
    pSharpParamStatic->sharp_hsv[31]=256;
    pSharpParamStatic->sharp_hsv[32]=256;
    pSharpParamStatic->sharp_hsv[33]=256;
    pSharpParamStatic->sharp_hsv[34]=256;
    pSharpParamStatic->sharp_hsv[35]=256;
    pSharpParamStatic->sharp_hsv[36]=256;
    pSharpParamStatic->sharp_hsv[37]=256;
    pSharpParamStatic->sharp_hsv[38]=256;
    pSharpParamStatic->sharp_hsv[39]=256;
    pSharpParamStatic->sharp_hsv[40]=256;
    pSharpParamStatic->sharp_hsv[41]=256;
    pSharpParamStatic->sharp_hsv[42]=256;
    pSharpParamStatic->sharp_hsv[43]=256;
    pSharpParamStatic->sharp_hsv[44]=256;
    pSharpParamStatic->sharp_hsv[45]=256;

    pSharpParamDynamic->sharp_edge_lum[0]=128;
    pSharpParamDynamic->sharp_edge_lum[1]=144;
    pSharpParamDynamic->sharp_edge_lum[2]=160;
    pSharpParamDynamic->sharp_edge_lum[3]=176;
    pSharpParamDynamic->sharp_edge_lum[4]=192;
    pSharpParamDynamic->sharp_edge_lum[5]=208;
    pSharpParamDynamic->sharp_edge_lum[6]=224;
    pSharpParamDynamic->sharp_edge_lum[7]=240;
    pSharpParamDynamic->sharp_edge_lum[8]=256;
    pSharpParamDynamic->sharp_edge_lum[9]=256;
    pSharpParamDynamic->sharp_edge_lum[10]=256;
    pSharpParamDynamic->sharp_edge_lum[11]=256;
    pSharpParamDynamic->sharp_edge_lum[12]=256;
    pSharpParamDynamic->sharp_edge_lum[13]=256;
    pSharpParamDynamic->sharp_edge_lum[14]=256;
    pSharpParamDynamic->sharp_edge_lum[15]=256;
    pSharpParamDynamic->sharp_edge_lum[16]=256;
    pSharpParamDynamic->sharp_edge_lum[17]=256;
    pSharpParamDynamic->sharp_edge_lum[18]=256;
    pSharpParamDynamic->sharp_edge_lum[19]=256;
    pSharpParamDynamic->sharp_edge_lum[20]=256;
    pSharpParamDynamic->sharp_edge_lum[21]=256;
    pSharpParamDynamic->sharp_edge_lum[22]=256;
    pSharpParamDynamic->sharp_edge_lum[23]=256;
    pSharpParamDynamic->sharp_edge_lum[24]=256;
    pSharpParamDynamic->sharp_edge_lum[25]=256;
    pSharpParamDynamic->sharp_edge_lum[26]=256;
    pSharpParamDynamic->sharp_edge_lum[27]=256;
    pSharpParamDynamic->sharp_edge_lum[28]=256;
    pSharpParamDynamic->sharp_edge_lum[29]=256;
    pSharpParamDynamic->sharp_edge_lum[30]=256;
    pSharpParamDynamic->sharp_edge_lum[31]=256;
    pSharpParamDynamic->sharp_edge_lum[32]=256;

#if 0
    pSharpParamDynamic->roi_num = 0; //<=8
    pSharpParamDynamic->roi_item[0].x      = 40;
    pSharpParamDynamic->roi_item[0].y      = 59;
    pSharpParamDynamic->roi_item[0].width  = 32;
    pSharpParamDynamic->roi_item[0].height = 32;


    pSharpParam->roi_item[1].x      = 40;
    pSharpParam->roi_item[1].y      = 59;
    pSharpParam->roi_item[1].width  = 32;
    pSharpParam->roi_item[1].height = 32;

    pSharpParam->roi_item[2].x      = 40;
    pSharpParam->roi_item[2].y      = 59;
    pSharpParam->roi_item[2].width  = 32;
    pSharpParam->roi_item[2].height = 32;

    pSharpParam->roi_item[3].x      = 40;
    pSharpParam->roi_item[3].y      = 59;
    pSharpParam->roi_item[3].width  = 32;
    pSharpParam->roi_item[3].height = 32;

    pSharpParam->roi_item[4].x      = 40;
    pSharpParam->roi_item[4].y      = 59;
    pSharpParam->roi_item[4].width  = 32;
    pSharpParam->roi_item[4].height = 32;

    pSharpParam->roi_item[5].x      = 40;
    pSharpParam->roi_item[5].y      = 59;
    pSharpParam->roi_item[5].width  = 32;
    pSharpParam->roi_item[5].height = 32;

    pSharpParam->roi_item[6].x      = 40;
    pSharpParam->roi_item[6].y      = 59;
    pSharpParam->roi_item[6].width  = 32;
    pSharpParam->roi_item[6].height = 32;

    pSharpParam->roi_item[7].x      = 40;
    pSharpParam->roi_item[7].y      = 59;
    pSharpParam->roi_item[7].width  = 32;
    pSharpParam->roi_item[7].height = 32;
#endif

    return 0;
}

int setEncParam(encoder_Context* pEncContext, encode_param_t *encode_param)
{
    VideoEncoder *pVideoEnc = pEncContext->pVideoEnc;
    int result = 0;
    VeProcSet mProcSet;
	unsigned int vbv_size = 8*1024*1024;

    if(encode_param->encode_format == VENC_CODEC_JPEG)
    {
        vbv_size = MAX(encode_param->dst_size*2, vbv_size);
    }

    if(encode_param->encode_format != VENC_CODEC_JPEG)
    {
        if(encode_param->h264_func.h264Param.sRcParam.eRcMode == AW_VBR
            || encode_param->h265_func.h265Param.sRcParam.eRcMode == AW_VBR)
        {
            VencSetParameter(pVideoEnc, VENC_IndexParamVbrOptEnable, &encode_param->mVbrOptEn);
            if(encode_param->mVbrOptEn)
            {
                VencSetParameter(pVideoEnc, VENC_IndexParamVbrOptParam, &encode_param->mVbrOptParam);
            }
        }
        encode_param->mProductMode.nDstWidth = encode_param->dst_width;
        encode_param->mProductMode.nDstHeight = encode_param->dst_height;
        encode_param->mProductMode.nFrameRate = encode_param->frame_rate;
        encode_param->mProductMode.nBitrate = encode_param->bit_rate;
        VencSetParameter(pVideoEnc, VENC_IndexParamProductCase, &encode_param->mProductMode);
    }

    mProcSet.bProcEnable = 1;
    mProcSet.nProcFreq = 30;
    mProcSet.nStatisBitRateTime = 1000;
    mProcSet.nStatisFrRateTime  = 1000;
    VencSetParameter(pVideoEnc, VENC_IndexParamProcSet, &mProcSet);

    if(!encode_param->conf_win.en_force_conf
        && (encode_param->dst_width != ALIGN_XXB(16, encode_param->dst_width)
        || encode_param->dst_height != ALIGN_XXB(16, encode_param->dst_height)))
    {
        encode_param->conf_win.en_force_conf = 1;
        encode_param->conf_win.left_offset = 0;
        encode_param->conf_win.right_offset = ALIGN_XXB(16, encode_param->dst_width) - encode_param->dst_width;
        encode_param->conf_win.top_offset = 0;
        encode_param->conf_win.bottom_offset = ALIGN_XXB(16, encode_param->dst_height) - encode_param->dst_height;
    }
    VencSetParameter(pVideoEnc, VENC_IndexParamForceConfWin, &encode_param->conf_win);

#if ENABLE_DROP_FRAME_NUM
    unsigned int nDropFrameNum = 10;
    VencSetParameter(pVideoEnc, VENC_IndexParamDropFrame, &nDropFrameNum);
#endif

#if ENABLE_3D_FLITER
    if(encode_param->encode_format == VENC_CODEC_H265 || encode_param->encode_format == VENC_CODEC_H264)
    {
        s3DfilterParam m3DfilterParam;
        m3DfilterParam.enable_3d_filter =        1;
        m3DfilterParam.adjust_pix_level_enable = 0;
        m3DfilterParam.smooth_filter_enable =    0;
        m3DfilterParam.max_pix_diff_th =         6;
        m3DfilterParam.max_mad_th =              48;
        m3DfilterParam.max_mv_th  =              5;
        m3DfilterParam.max_coef =                16;
        m3DfilterParam.min_coef =                0;
        if(encode_param->m3DfilterParam.max_mad_th == 0)
            encode_param->m3DfilterParam.max_mad_th = m3DfilterParam.max_mad_th;
        if(encode_param->m3DfilterParam.max_mv_th == 0)
            encode_param->m3DfilterParam.max_mv_th = m3DfilterParam.max_mv_th;
        VencSetParameter(pVideoEnc, VENC_IndexParam3DFilterNew, &encode_param->m3DfilterParam);
    }

  #if ENABLE_REGION_D3D
    if(encode_param->encode_format == VENC_CODEC_H265 || encode_param->encode_format == VENC_CODEC_H264)
    {
        VencRegionD3DParam rdp = {0};
        rdp.en_region_d3d = 1;
        rdp.dis_default_para = 1;
        rdp.result_num = 3;
        rdp.hor_region_num = 15;
        rdp.ver_region_num = 8;
        rdp.hor_expand_num = 1;
        rdp.ver_expand_num = 1;
        rdp.chroma_offset = 1;
        rdp.static_coef[0] = 5;
        rdp.static_coef[1] = 6;
        rdp.static_coef[2] = 7;
        rdp.motion_coef[0] = 13;
        rdp.motion_coef[1] = 14;
        rdp.motion_coef[2] = 15;
        rdp.motion_coef[3] = 16;
        rdp.zero_mv_rate_th[0] = 96;
        rdp.zero_mv_rate_th[1] = 93;
        rdp.zero_mv_rate_th[2] = 90;
        VencSetParameter(pVideoEnc, VENC_IndexParamRegionD3DParam, &encode_param->mRegionD3DParam);
    }
  #endif
#endif

#if 0
    int bEnD3DInIFrm = 0;
    VencSetParameter(pVideoEnc, VENC_IndexParamEnD3DInIFrm, &bEnD3DInIFrm);

    int bEnTightMbQp = 0;
    VencSetParameter(pVideoEnc, VENC_IndexParamEnTightMbQp, &bEnTightMbQp);

    VencExtremeD3DParam sExD3D = {0};
    VencSetParameter(pVideoEnc, VENC_IndexParamSetExtremeD3D, &sExD3D);
#endif

#if ENABLE_2D_FLITER
    if(encode_param->encode_format == VENC_CODEC_H265 || encode_param->encode_format == VENC_CODEC_H264)
    {
        s2DfilterParam m2DfilterParam;
        m2DfilterParam.enable_2d_filter = 0;
        m2DfilterParam.filter_strength_uv = 127;
        m2DfilterParam.filter_strength_y  = 127;
        m2DfilterParam.filter_th_uv       = 7;
        m2DfilterParam.filter_th_y        = 11;
        VencSetParameter(pVideoEnc, VENC_IndexParam2DFilter, &encode_param->m2DfilterParam);
    }
#endif

#if ENABLE_SUPER_FRAME
    VencSetParameter(pVideoEnc, VENC_IndexParamSuperFrameConfig, &encode_param->mSuperFrame);
#endif

#if ENABLE_TARGET_BITS_RATIO
    int cnt;
    VencIPTargetBitsRatio mBitsRatio = {0};
    VencGetParameter(pVideoEnc, VENC_IndexParamIPTargetBitsRatio, &mBitsRatio);
    for(cnt = 0; cnt < 3; cnt++)
    {
        if(encode_param->mTargetBits.nSceneCoef[cnt] > 0)
        {
            mBitsRatio.nSceneCoef[cnt] = encode_param->mTargetBits.nSceneCoef[cnt];
        }
    }
    for(cnt = 0; cnt < 5; cnt++)
    {
        if(encode_param->mTargetBits.nMoveCoef[cnt] > 0)
        {
            mBitsRatio.nMoveCoef[cnt] = encode_param->mTargetBits.nMoveCoef[cnt];
        }
    }
    VencSetParameter(pVideoEnc, VENC_IndexParamIPTargetBitsRatio, &mBitsRatio);
#endif

#if ENABLE_WEAK_TEXT_TH
    VencSetParameter(pVideoEnc, VENC_IndexParamWeakTextTh, &encode_param->weak_text_th);
#endif

#if ENABLE_BITS_CLIP
    encode_param->mBitsClip.dis_default_para = 1;
    VencSetParameter(pVideoEnc, VENC_IndexParamTargetBitsClipParam, &encode_param->mBitsClip);
#endif

    VencSetParameter(pVideoEnc, VENC_IndexParamEnIFrmMbRcMoveStatus, &encode_param->mb_rc_level);

#if ENABLE_VE2ISP_D2D
    VencSetParameter(pVideoEnc, VENC_IndexParamVe2IspD2DLimit, &encode_param->ve2isp_d2d);
#endif
    if(encode_param->bEnableGdc == 1)
    {
        sGdcParam mGdcParam;
        memset(&mGdcParam, 0, sizeof(sGdcParam));
        initGdcFunc(&mGdcParam);
        VencSetParameter(pVideoEnc, VENC_IndexParamGdcConfig, &mGdcParam);
    }
    if(encode_param->bEnableSharp == 1)
    {
        sEncppSharpParam mSharpParam;
        memset(&mSharpParam, 0, sizeof(sEncppSharpParam));

        initSharpFunc(&mSharpParam.mDynamicParam, &mSharpParam.mStaticParam);
        unsigned int enableSharp = 1;

        VencSetParameter(pVideoEnc, VENC_IndexParamSharpConfig, &mSharpParam);
        VencSetParameter(pVideoEnc, VENC_IndexParamEnableEncppSharp, &enableSharp);
    }

#if SETUP_VIDEO_TIME_INFO
    unsigned int fps = 30;
    VencH264VideoTiming mTiming;
    mTiming.fixed_frame_rate_flag  = 0;
    mTiming.num_units_in_tick = 1000;
    mTiming.time_scale = mTiming.num_units_in_tick*fps*2;

    VencSetParameter(pVideoEnc, VENC_IndexParamH264VideoTiming, &mTiming);
#endif

	if(encode_param->vbv_size)
		vbv_size = encode_param->vbv_size;

    logd("encode_param->rotate = %d, vbv_size = %d", encode_param->rotate, vbv_size);

    VencSetParameter(pVideoEnc, VENC_IndexParamRotation, &encode_param->rotate);

#ifdef SET_MB_INFO
    //init VencMBModeCtrl
    init_mb_mode(&pEncContext->mMBModeCtrl, encode_param->dst_width, encode_param->dst_height);
#endif

#ifdef GET_MB_INFO
    //init VencMBInfo
    init_mb_info(&pEncContext->mMBInfo, encode_param);
#endif

    if(encode_param->encode_format == VENC_CODEC_JPEG)
    {
        result = initJpegFunc(&encode_param->jpeg_func, encode_param);
        if(result)
        {
            loge("initJpegFunc error, return \n");
            return -1;
        }

        if(1 == encode_param->jpeg_func.jpeg_mode)
        {
            VencSetParameter(pVideoEnc, VENC_IndexParamJpegEncMode, &encode_param->jpeg_func.jpeg_mode);
            VencSetParameter(pVideoEnc, VENC_IndexParamBitrate, &encode_param->jpeg_func.jpeg_biteRate);
            VencSetParameter(pVideoEnc, VENC_IndexParamFramerate, &encode_param->jpeg_func.jpeg_frameRate);
            VencSetParameter(pVideoEnc, VENC_IndexParamSetBitRateRange, &encode_param->jpeg_func.bitRateRange);
            VencSetParameter(pVideoEnc, VENC_IndexParamJpegQuality, &encode_param->jpeg_func.quality);
            VencSetParameter(pVideoEnc, VENC_IndexParamSetVbvSize, &vbv_size);
        }
        else
        {
            VencSetParameter(pVideoEnc, VENC_IndexParamSetVbvSize, &vbv_size);
            VencSetParameter(pVideoEnc, VENC_IndexParamJpegQuality, &encode_param->jpeg_func.quality);
            VencSetParameter(pVideoEnc, VENC_IndexParamJpegExifInfo, &encode_param->jpeg_func.exifinfo);
        }

        if(encode_param->jpeg_func.pRoiConfig.bEnable == 1)
        {
            VencSetParameter(pVideoEnc, VENC_IndexParamRoi, &encode_param->jpeg_func.pRoiConfig);
        }

        if(encode_param->test_overlay_flag == 1)
        {
            VencSetParameter(pVideoEnc, VENC_IndexParamSetOverlay, &encode_param->jpeg_func.sOverlayInfo);
        }

#ifdef USE_VIDEO_SIGNAL
        VencJpegVideoSignal mVideoSignal = {VENC_BT709, VENC_BT709};
        if(encode_param->mVideoSignal.dst_colour_primaries != 0)
        {
            mVideoSignal.src_colour_primaries = encode_param->mVideoSignal.src_colour_primaries;
            mVideoSignal.dst_colour_primaries = encode_param->mVideoSignal.dst_colour_primaries;
        }
        VencSetParameter(pVideoEnc, VENC_IndexParamJpegVideoSignal, &mVideoSignal);
#endif
    }
    else if(encode_param->encode_format == VENC_CODEC_H264)
    {
        result = initH264Func(encode_param);
        if(result)
        {
            loge("initH264Func error, return \n");
            return -1;
        }

        VencSetParameter(pVideoEnc, VENC_IndexParamIsNightCaseFlag, &encode_param->is_night);

        VencSetParameter(pVideoEnc, VENC_IndexParamSetVbvSize, &vbv_size);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264Param, &encode_param->h264_func.h264Param);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &encode_param->h264_func.fixQP);
        if(encode_param->test_overlay_flag == 1)
        {
            VencSetParameter(pVideoEnc, VENC_IndexParamSetOverlay, &encode_param->h264_func.sOverlayInfo);
        }

        VencSetParameter(pVideoEnc, VENC_IndexParamProcSet, &encode_param->h264_func.sVeProcInfo);

#ifdef USE_VIDEO_SIGNAL
        VencH264VideoSignal mVencH264VideoSignal = {DEFAULT, 1, VENC_BT709, VENC_BT709};
        if(encode_param->mVideoSignal.dst_colour_primaries != 0)
        {
            memcpy(&mVencH264VideoSignal, &encode_param->mVideoSignal, sizeof(VencH264VideoSignal));
        }
        VencSetParameter(pVideoEnc, VENC_IndexParamH264VideoSignal, &mVencH264VideoSignal);
#endif

#ifdef DETECT_MOTION
        MotionParam mMotionPara;
        mMotionPara.nMaxNumStaticFrame = 4;
        mMotionPara.nMotionDetectEnable = 1;
        mMotionPara.nMotionDetectRatio = 0;
        mMotionPara.nMV64x64Ratio = 0.01;
        mMotionPara.nMVXTh = 6;
        mMotionPara.nMVYTh = 6;
        mMotionPara.nStaticBitsRatio = 0.2;
        mMotionPara.nStaticDetectRatio = 2;
        VencSetParameter(pVideoEnc, VENC_IndexParamMotionDetectStatus,
                                                &mMotionPara);
#endif

#ifdef DETECT_MOTION
        MotionParam mMotionPara;
        mMotionPara.nMaxNumStaticFrame = 4;
        mMotionPara.nMotionDetectEnable = 1;
        mMotionPara.nMotionDetectRatio = 0;
        mMotionPara.nMV64x64Ratio = 0.01;
        mMotionPara.nMVXTh = 6;
        mMotionPara.nMVYTh = 6;
        mMotionPara.nStaticBitsRatio = 0.2;
        mMotionPara.nStaticDetectRatio = 2;

        VencSetParameter(pVideoEnc, VENC_IndexParamMotionDetectStatus,
                                                &mMotionPara);
#endif

        //int tmptmp = 60;
        //VencSetParameter(pVideoEnc, VENC_IndexParamVirtualIFrame, &tmptmp);

#ifdef GET_MB_INFO
        VencSetParameter(pVideoEnc, VENC_IndexParamMBInfoOutput, &pEncContext->mMBInfo);
#endif


#if 0
        unsigned char value = 1;
        //set the specify func
        VencSetParameter(pVideoEnc, VENC_IndexParamH264SVCSkip, &h264_func.SVCSkip);
        value = 0;
        VencSetParameter(pVideoEnc, VENC_IndexParamIfilter, &value);
        value = 0; //degree
        VencSetParameter(pVideoEnc, VENC_IndexParamRotation, &value);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &h264_func.fixQP);
        VencSetParameter(pVideoEnc,
            VENC_IndexParamH264CyclicIntraRefresh, &h264_func.sIntraRefresh);
        value = 720/4;
        VencSetParameter(pVideoEnc, VENC_IndexParamSliceHeight, &value);
        value = 0;
        VencSetParameter(pVideoEnc, VENC_IndexParamSetPSkip, &value);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264AspectRatio, &h264_func.sAspectRatio);
        value = 0;
        VencSetParameter(pVideoEnc, VENC_IndexParamFastEnc, &value);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264VideoSignal, &h264_func.sVideoSignal);
        VencSetParameter(pVideoEnc, VENC_IndexParamSuperFrameConfig, &h264_func.sSuperFrameCfg);
#endif
    }
    else if(encode_param->encode_format == VENC_CODEC_H265)
    {
        result = initH265Func(encode_param);
        if(result)
        {
            loge("initH265Func error, return \n");
            return -1;
        }
        VencSetParameter(pVideoEnc, VENC_IndexParamIsNightCaseFlag, &encode_param->is_night);

        VencSetParameter(pVideoEnc, VENC_IndexParamSetVbvSize, &vbv_size);
        VencSetParameter(pVideoEnc, VENC_IndexParamH265Param, &encode_param->h265_func.h265Param);

        unsigned int value = 1;
        if(encode_param->test_overlay_flag == 1)
        {
        VencSetParameter(pVideoEnc, VENC_IndexParamSetOverlay, &encode_param->h265_func.sOverlayInfo);
        }
        //VencSetParameter(pVideoEnc,
        //VENC_IndexParamAlterFrame, &h265_func.sAlterFrameRateInfo);
        VencSetParameter(pVideoEnc, VENC_IndexParamChannelNum, &value);
        VencSetParameter(pVideoEnc, VENC_IndexParamProcSet, &encode_param->h265_func.sVeProcInfo);

#ifdef USE_VIDEO_SIGNAL
        VencH264VideoSignal mVencH264VideoSignal = {DEFAULT, 1, VENC_BT709, VENC_BT709};
        if(encode_param->mVideoSignal.video_format != 0)
        {
            memcpy(&mVencH264VideoSignal, &encode_param->mVideoSignal, sizeof(VencH264VideoSignal));
        }
        VencSetParameter(pVideoEnc, VENC_IndexParamVUIVideoSignal, &mVencH264VideoSignal);
#endif

        //VencSetParameter(pVideoEnc, VENC_IndexParamVirtualIFrame, &encode_param->frame_rate);
        //value = 0;
        //VencSetParameter(pVideoEnc, VENC_IndexParamPFrameIntraEn, &value);
        //value = 1;
        //VencSetParameter(pVideoEnc, VENC_IndexParamEncodeTimeEn, &value);
        //VencSetParameter(pVideoEnc,
        //VENC_IndexParamH265ToalFramesNum,  &h265_func.h265_rc_frame_total);
        //VencSetParameter(pVideoEnc, VENC_IndexParamH265Gop, &h265_func.h265Gop);

        //VencSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &h265_func.sRoiConfig[0]);
        VencSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &encode_param->h265_func.fixQP);
        //VencSetParameter(pVideoEnc, VENC_IndexParamH265HVS, &h265_func.h265Hvs);
        //VencSetParameter(pVideoEnc, VENC_IndexParamH265TendRatioCoef, &h265_func.h265Trc);
#ifdef GET_MB_INFO
        VencSetParameter(pVideoEnc, VENC_IndexParamMBInfoOutput, &pEncContext->mMBInfo);
#endif
    }
    return 0;
}

void setMbMode(encoder_Context* pEncContext, encode_param_t *encode_param)
{
    VideoEncoder *pVideoEnc = pEncContext->pVideoEnc;
    if(encode_param->encode_format == VENC_CODEC_H264 || encode_param->encode_format == VENC_CODEC_H265)
    {
        if(pEncContext->mMBModeCtrl.mode_ctrl_en == 1)
            VencSetParameter(pVideoEnc, VENC_IndexParamMBModeCtrl, &pEncContext->mMBModeCtrl);
    }
}

void getMbMinfo(encoder_Context* pEncContext)
{
    VencMBInfo *pMBInfo = &pEncContext->mMBInfo;

    logv("pMBInfo = %p, p_para = %p", pMBInfo, pMBInfo->p_para);

    if(pMBInfo == NULL || pMBInfo->p_para == NULL)
        return ;

    unsigned int i;
    for(i = 0; i < pMBInfo->num_mb; i++)
    {
        if(i == 4)
        logd("No.%d MB: mad=%d, qp=%d, sse=%d, psnr=%f\n",i,pMBInfo->p_para[i].mb_mad,
        pMBInfo->p_para[i].mb_qp, pMBInfo->p_para[i].mb_sse, pMBInfo->p_para[i].mb_psnr);
    }
}

void releaseMb(encode_param_t *encode_param)
{
    VencMBInfo *pMBInfo;
    VencMBModeCtrl *pMBMode;
    if(encode_param->encode_format == VENC_CODEC_H264 && encode_param->h264_func.h264MBMode.mode_ctrl_en)
    {
        pMBInfo = &encode_param->h264_func.MBInfo;
        pMBMode = &encode_param->h264_func.h264MBMode;
    }
    else if(encode_param->encode_format == VENC_CODEC_H265 && encode_param->h265_func.h265MBMode.mode_ctrl_en)
    {
        pMBInfo = &encode_param->h264_func.MBInfo;
        pMBMode = &encode_param->h265_func.h265MBMode;
    }
    else
        return;

    if(pMBInfo->p_para)
        FREE(pMBInfo->p_para);
    if(pMBMode->p_map_info)
        FREE(pMBMode->p_map_info);
}

static int saveJpegPic(VencOutputBuffer *pOutputBuffer, const char* out_path,int nTestNum)
{
    char name[128];

    sprintf(name, "%s_%d.jpg",out_path, nTestNum);

    FILE_STRUCT *out_file = fopen(name, "wb");

    if(out_file == NULL)
    {
        logw("open file failed : %s", name);
        return -1;
    }

    fwrite(pOutputBuffer->pData0, 1, pOutputBuffer->nSize0, out_file);

    if(pOutputBuffer->nSize1)
    {
       fwrite(pOutputBuffer->pData1, 1, pOutputBuffer->nSize1, out_file);
    }

    if(pOutputBuffer->nSize2)
    {
       fwrite(pOutputBuffer->pData2, 1, pOutputBuffer->nSize2, out_file);
    }

    fclose(out_file);
    return 0;
}

static int initWbYuv(WbYuvFuncInfo *pWbYuvFuncInfo, encode_param_t *encode_param, VideoEncoder* pVideoEnc)
{
    sWbYuvParam mWbYuvParam;
    memset(&mWbYuvParam, 0, sizeof(sWbYuvParam));
    mWbYuvParam.bEnableWbYuv = 1;
    mWbYuvParam.nWbBufferNum = 3;
    mWbYuvParam.scalerRatio  = VENC_ISP_SCALER_0;
    mWbYuvParam.bEnableCrop  = 0;
    mWbYuvParam.sWbYuvcrop.nHeight  = 640;
    mWbYuvParam.sWbYuvcrop.nWidth  = 640;
    //center point alignment
    mWbYuvParam.sWbYuvcrop.nTop = (ALIGN_XXB(16, encode_param->dst_height)/2 - mWbYuvParam.sWbYuvcrop.nHeight/2);
    mWbYuvParam.sWbYuvcrop.nLeft = (ALIGN_XXB(16, encode_param->dst_width)/2 - mWbYuvParam.sWbYuvcrop.nWidth/2);

    VencSetParameter(pVideoEnc, VENC_IndexParamEnableWbYuv, &mWbYuvParam);

    unsigned int ThumbScale = 0;

    if(mWbYuvParam.scalerRatio == VENC_ISP_SCALER_0)
        ThumbScale = 0;
    else if(mWbYuvParam.scalerRatio == VENC_ISP_SCALER_HALF)
        ThumbScale = 1;
    else if(mWbYuvParam.scalerRatio == VENC_ISP_SCALER_QUARTER)
        ThumbScale = 2;
    else if(mWbYuvParam.scalerRatio == VENC_ISP_SCALER_EIGHTH)
        ThumbScale = 3;

    unsigned int nAlignW = ALIGN_XXB(16, encode_param->dst_width) >> ThumbScale;
    unsigned int nAlignH = ALIGN_XXB(16, encode_param->dst_height) >> ThumbScale;

    if(mWbYuvParam.bEnableCrop)
    {
        nAlignW = ALIGN_XXB(16, mWbYuvParam.sWbYuvcrop.nWidth);
        nAlignH = ALIGN_XXB(16, mWbYuvParam.sWbYuvcrop.nHeight);
    }
    pWbYuvFuncInfo->yuvSize  = nAlignW*nAlignH*3/2;
    logd("%d %d %d", nAlignW, nAlignH, pWbYuvFuncInfo->yuvSize);
    pWbYuvFuncInfo->yuvBuf = CALLOC(1, pWbYuvFuncInfo->yuvSize);
    if(pWbYuvFuncInfo->yuvBuf == NULL)
    {
        loge("malloc failed, size = %d", pWbYuvFuncInfo->yuvSize);
        return -1;
    }
    if(pWbYuvFuncInfo->wbyuv_file)
    {
        return 0;
    }
    char wb_yuv_name[128];
    sprintf(wb_yuv_name, "/mnt/extsd/v853/wb_%dx%d_nv12.yuv",nAlignW, nAlignH);
    pWbYuvFuncInfo->wbyuv_file = fopen(wb_yuv_name, "wb");
    if(pWbYuvFuncInfo->wbyuv_file == NULL)
    {
        logw("open file failed : %s", wb_yuv_name);
    }

    return 0;
}

static void deInitWbYuv(WbYuvFuncInfo *pWbYuvFuncInfo)
{
    if(pWbYuvFuncInfo->wbyuv_file)
        fclose(pWbYuvFuncInfo->wbyuv_file);

    if(pWbYuvFuncInfo->yuvBuf)
        FREE(pWbYuvFuncInfo->yuvBuf);
}

static int saveWbYuv(encoder_Context* pEncContext, encode_param_t *encode_param)
{
    int result;
    VencThumbInfo mThumbInfo;
    WbYuvFuncInfo *pWbYuvFuncInfo = &pEncContext->mWbYuvFuncInfo;

    memset(&mThumbInfo, 0, sizeof(VencThumbInfo));

    mThumbInfo.pThumbBuf    = pWbYuvFuncInfo->yuvBuf;
    mThumbInfo.nThumbSize   = pWbYuvFuncInfo->yuvSize;
    mThumbInfo.bWriteToFile = 0;
    mThumbInfo.fp           = NULL;

    logd("yuvBuf = %p, yuvSize = %d, file = %p",
        pWbYuvFuncInfo->yuvBuf, pWbYuvFuncInfo->yuvSize, pWbYuvFuncInfo->wbyuv_file);

    result = VencGetParameter(pEncContext->pVideoEnc, VENC_IndexParamGetThumbYUV, &mThumbInfo);
    if(result != 0)
    {
        logd("wbyuv valid list is null");
        return -1;
    }
    if(pEncContext->mWbYuvFuncInfo.wbyuv_file)
    {
        fwrite(pWbYuvFuncInfo->yuvBuf, 1, pWbYuvFuncInfo->yuvSize, pWbYuvFuncInfo->wbyuv_file);
    }
    else
        logd("wbyuv_file[%p] is null", pWbYuvFuncInfo->wbyuv_file);

    return 0;
}

static int EventHandler(VideoEncoder* pEncoder, void* pAppData, VencEventType eEvent,
                             unsigned int nData1, unsigned int nData2, void* pEventData)
{
    encoder_Context* pEncContext = (encoder_Context*)pAppData;
    logv("event = %d", eEvent);
    switch(eEvent)
    {
        case VencEvent_UpdateMbModeInfo:
        {
            VencMBModeCtrl *pMbModeCtl = (VencMBModeCtrl *)pEventData;
            //* update mb_mode here
            init_mb_mode(&pEncContext->mMBModeCtrl, pEncContext->baseConfig.nDstWidth, pEncContext->baseConfig.nDstHeight);

            memcpy(pMbModeCtl, &pEncContext->mMBModeCtrl, sizeof(VencMBModeCtrl));
            logv("**mode_ctrl_en = %d", pMbModeCtl->mode_ctrl_en);
            break;
        }
        case VencEvent_UpdateMbStatInfo:
        {
            getMbMinfo(pEncContext);
            break;
        }
        case VencEvent_UpdateIspToVeParam:
        {
            VencIsp2VeParam *pIsp2VeParam = (VencIsp2VeParam *)pEventData;

            if (pIsp2VeParam)
            {
                pIsp2VeParam->mEnCameraMove = CAMERA_ADAPTIVE_MOVING_AND_STATIC;
                pIsp2VeParam->mEnvLv = 1100;
                pIsp2VeParam->mAeWeightLum = 50;
            }
            else
            {
               logw("pEventData is NULL");
            }
            break;
        }
        default:
            logv("not support the event: %d", eEvent);
            break;
    }

    return 0;
}

static int InputBufferDone(VideoEncoder* pEncoder,    void* pAppData,
                          VencCbInputBufferDoneInfo* pBufferDoneInfo)
{
    encoder_Context* pEncContext = (encoder_Context*)pAppData;
    InputBufferInfo *pInputBufInfo = NULL;
    pInputBufInfo = dequeue(&pEncContext->mInputBufMgr.empty_quene);
    if(pInputBufInfo == NULL)
    {
        loge("error: dequeue empty_queue failed");
        return -1;
    }
    memcpy(&pInputBufInfo->inputbuffer, pBufferDoneInfo->pInputBuffer, sizeof(VencInputBuffer));
    enqueue(&pEncContext->mInputBufMgr.valid_quene, pInputBufInfo);
    return 0;
}

static void showPtsInfo(ptsDebugInfo* pPtsInfo)
{
    long long diffPts = 0;
    if(pPtsInfo->prePts != 0 )
      diffPts = pPtsInfo->curPts - pPtsInfo->prePts;

    if(diffPts > pPtsInfo->maxPts)
      pPtsInfo->maxPts = diffPts;

    if(diffPts < pPtsInfo->minPts || pPtsInfo->minPts == 0)
      pPtsInfo->minPts = diffPts;

    if(diffPts > 40*1000)
      logd("bit pts = %lld", diffPts/1000);

    logd("curPts = %lld ms, diffPts = %lld ms, maxPts = %lld ms, minPts = %lld ms",
        pPtsInfo->curPts/1000, diffPts/1000, pPtsInfo->maxPts/1000, pPtsInfo->minPts/1000);

    pPtsInfo->prePts = pPtsInfo->curPts;

}

static int readYuvFile(FILE *fp, unsigned char *dst, int stride, int width, int height)
{
    int ret = 0;

    if(stride == width)
    {
        ret = fread(dst, 1, width*height, fp);
    }
    else
    {
        int h = 0, len = 0;
        unsigned char *line = dst;
        for(h = 0; h < height; h++)
        {
            len = fread(line, 1, width, fp);
            ret += len;
            line += stride;
            if(len != width)
                break;
        }
    }
    return ret;
}

static int testEncppFunction(encode_param_t* pEncodeParam)
{
    int bTestCropFlag        = 0;
    int bTestScaleFlag       = 0;
    int bTestRotateAngle     = 0;
    int bTestOverlayerFlag   = pEncodeParam->test_overlay_flag;
    int bTestHorizonflipFlag = 0;
    int nColorspaceYuv2Yuv   = 0;
    int nColorspaceRgb2Yuv   = 0;
    int nTotalTestFrmNum     = pEncodeParam->encode_frame_num;
    int nTotalOutputBufNum   = MIN(WB_BUF_NUM, pEncodeParam->encode_frame_num);

    VideoEncoderEncpp *pEncpp = NULL;
    MEMOPS_STRUCT *_memops = NULL;
    VencEncppBufferInfo mInBuffer = {0};
    VencEncppBufferInfo *pOutBuffer = NULL;
    VEOPS_STRUCT*           veOps = NULL;
    void*             pVeopsSelf = NULL;
    Encpp_Ops_Set s_ops_set;
    int i = 0, frm_cnt = 0, ret = 0;

    FILE_STRUCT *in_file = NULL;
    FILE_STRUCT *out_file = NULL;
    char *input_path = NULL;
    char *output_path = NULL;
    VencEncppFuncParam mIspFunction;
    VencRect mCropInfo;
    VencOverlayInfoS sOverlayInfo;
    int nOutBufferSize;

    sGdcParam *pGdcParam = NULL;
    sEncppSharpParam *pSharpParam = NULL;

    memset(&mIspFunction, 0, sizeof(VencEncppFuncParam));
    memset(&mCropInfo, 0, sizeof(VencRect));
    memset(&sOverlayInfo, 0, sizeof(VencOverlayInfoS));


    input_path = pEncodeParam->intput_file;
    output_path = pEncodeParam->output_file;

    mInBuffer.colorFormat = pEncodeParam->pixel_format;
    mInBuffer.nStride  = ALIGN_XXB(16, pEncodeParam->src_width);
    mInBuffer.nWidth   = ALIGN_XXB(16, pEncodeParam->src_width);
    mInBuffer.nHeight  = ALIGN_XXB(16, pEncodeParam->src_height);

    mIspFunction.RoiConfig.bEnable = 0;
    in_file = fopen(input_path, "rb");
    if(in_file == NULL)
    {
        loge("open in_file fail\n");
        ret = -1;
        goto EXIT;
    }
    out_file = fopen(output_path, "wb");
    if(out_file == NULL)
    {
        loge("open out_file fail\n");
        ret = -1;
        goto EXIT;
    }

    //* get mem ops
    _memops = CDCGetMemOps();
    if(_memops == NULL)
    {
        loge("CDCGetMemOps failed");
        ret = -1;
        goto EXIT;
    }
    CDCMemOpen(_memops);

    //* get ve ops
    CDCGetVeOpsS(veOps, VE_OPS_TYPE_NORMAL);
    if(veOps == NULL)
    {
        loge("get ve ops failed , type = %d",VE_OPS_TYPE_NORMAL);
        ret = -1;
        goto EXIT;
    }

    if(NULL == (pOutBuffer = (VencEncppBufferInfo*)MALLOC(nTotalTestFrmNum*sizeof(VencEncppBufferInfo))))
    {
        loge("pOutBuffer malloc fialed!");
        ret = -1;
        goto EXIT;
    }
    for(i = 0; i < nTotalOutputBufNum; i++)
    {
        memset(&pOutBuffer[i], 0, sizeof(VencEncppBufferInfo));
        pOutBuffer[i].nStride = ALIGN_XXB(16, pEncodeParam->dst_width);
        pOutBuffer[i].nWidth  = ALIGN_XXB(16, pEncodeParam->dst_width);
        pOutBuffer[i].nHeight = ALIGN_XXB(16, pEncodeParam->dst_height);
    }

    VeConfig mVeConfig;
    memset(&mVeConfig, 0, sizeof(VeConfig));
    mVeConfig.nDecoderFlag = 0;
    mVeConfig.nEncoderFlag = 1;
    mVeConfig.nEnableAfbcFlag = 0;
    mVeConfig.nFormat = 0;
    mVeConfig.nWidth = 0;
    mVeConfig.nResetVeMode = 0;
    pVeopsSelf = CDCVeInit(veOps, &mVeConfig);
    if(pVeopsSelf == NULL)
    {
        loge("init ve ops failed");
        ret = -1;
        goto EXIT;
    }
    s_ops_set.memops = _memops;
    s_ops_set.veOpsS = veOps;
    s_ops_set.pVeOpsSelf = pVeopsSelf;

    pEncpp = VencEncppCreate(0);
    if(pEncpp == NULL)
    {
        loge("VideoEncIspCreate failed");
        ret = -1;
        goto EXIT;
    }
    //* palloc in and out buffer
    int nInBufferSize = mInBuffer.nStride * mInBuffer.nHeight*3/2;
    mInBuffer.pAddrVirY = EncAdapterMemPalloc(nInBufferSize);
    if(mInBuffer.pAddrVirY == NULL)
    {
        loge("palloc failed , size = %d",nInBufferSize);
        ret = -1;
        goto EXIT;
    }
    mInBuffer.pAddrVirC = mInBuffer.pAddrVirY + mInBuffer.nStride * mInBuffer.nHeight;
    mInBuffer.pAddrPhyY = EncAdapterMemGetPhysicAddress(mInBuffer.pAddrVirY);
    mInBuffer.pAddrPhyC0 = mInBuffer.pAddrPhyY + mInBuffer.nStride * mInBuffer.nHeight;
    mInBuffer.pAddrPhyC1 = mInBuffer.pAddrPhyC0 + mInBuffer.nStride * mInBuffer.nHeight / 4;
    if(mIspFunction.RoiConfig.bEnable == 0)
    {
        for(i = 0; i < nTotalOutputBufNum; i++)
        {
            nOutBufferSize = pOutBuffer[i].nWidth*pOutBuffer[i].nHeight*3/2;
            pOutBuffer[i].pAddrVirY = EncAdapterMemPalloc(nOutBufferSize);
            if(pOutBuffer[i].pAddrVirY == NULL)
            {
                loge("pOutBuffer[%d] palloc failed , size = %d", i, nOutBufferSize);
                ret = -1;
                goto EXIT;
            }
            pOutBuffer[i].pAddrVirC = pOutBuffer[i].pAddrVirY + pOutBuffer[i].nWidth*pOutBuffer[i].nHeight;
            pOutBuffer[i].pAddrPhyY = EncAdapterMemGetPhysicAddress(pOutBuffer[i].pAddrVirY);
            pOutBuffer[i].pAddrPhyC0 = pOutBuffer[i].pAddrPhyY + pOutBuffer[i].nWidth*pOutBuffer[i].nHeight;
            pOutBuffer[i].pAddrPhyC1 = pOutBuffer[i].pAddrPhyC0;
        }
    }

    //* read input data
    unsigned int nDataSizeY = pEncodeParam->src_width*pEncodeParam->src_height;
    unsigned int nDataSizeC = pEncodeParam->src_width*pEncodeParam->src_height/4;
    unsigned int nBufrSizeY = mInBuffer.nStride*mInBuffer.nHeight;
    unsigned int nBufSizeC  = mInBuffer.nStride*mInBuffer.nHeight/4;

    mIspFunction.bScaleFlag       = bTestScaleFlag;
    mIspFunction.nThumbScaleFactor = pEncodeParam->thumb_scaler_factor;
    mIspFunction.nRotateAngle     = bTestRotateAngle;
    mIspFunction.bHorizonflipFlag = bTestHorizonflipFlag;
    mIspFunction.bCropFlag        = pEncodeParam->en_crop;
    mIspFunction.bOverlayerFlag   = bTestOverlayerFlag;
    mIspFunction.nColorSpaceYuv2Yuv = nColorspaceYuv2Yuv;
    mIspFunction.nColorSpaceRgb2Yuv = nColorspaceRgb2Yuv;

    if(mIspFunction.bOverlayerFlag == 1)
    {
        init_overlay_info(&sOverlayInfo, pEncodeParam);
        mIspFunction.pOverlayerInfo = &sOverlayInfo;
    }

	if (pEncodeParam->bEnableGdc == 1) {
		pGdcParam = MALLOC(sizeof(sGdcParam));
		initGdcFunc(pGdcParam);
		mIspFunction.bEnableGdcFlag = 1;
		mIspFunction.pGdcParam = pGdcParam;
	}

	if (pEncodeParam->bEnableSharp == 1) {
		pSharpParam = MALLOC(sizeof(sEncppSharpParam));
		memset(pSharpParam, 0, sizeof(sEncppSharpParam));

		initSharpFunc(&pSharpParam->mDynamicParam, &pSharpParam->mStaticParam);
		mIspFunction.bEnableSharpFlag = 1;
		mIspFunction.pSharpParam = pSharpParam;
	}

    for(frm_cnt = 0; frm_cnt < nTotalTestFrmNum; frm_cnt++, i++)
    {
        int size = 0;
        int readCnt = 0;
READ_DATA_FROM_FILE:

        readCnt++;
        if(readCnt > 3)
        {
            loge("not enought data in the file");
            return -1;
        }

        if(i >= nTotalOutputBufNum)
        {
            i = 0;
        }

        printf("F%d testEncppFunction\n", frm_cnt);

        size = readYuvFile(in_file, mInBuffer.pAddrVirY, mInBuffer.nStride, pEncodeParam->src_width, pEncodeParam->src_height);
        if(mInBuffer.colorFormat == VENC_PIXEL_YUV420P || mInBuffer.colorFormat == VENC_PIXEL_YVU420P)
        {
            //* read c0 data
            size += readYuvFile(in_file, mInBuffer.pAddrVirC, mInBuffer.nStride/2,
                pEncodeParam->src_width/2, pEncodeParam->src_height/2);
            //* read c1 data
            size += readYuvFile(in_file, mInBuffer.pAddrVirC + nBufSizeC, mInBuffer.nStride/2,
                pEncodeParam->src_width/2, pEncodeParam->src_height/2);
        }
        else if(mInBuffer.colorFormat == VENC_PIXEL_YUV420SP || mInBuffer.colorFormat == VENC_PIXEL_YVU420SP)
        {
            size += readYuvFile(in_file, mInBuffer.pAddrVirC, mInBuffer.nStride,
                pEncodeParam->src_width, pEncodeParam->src_height/2);
        }
        else
        {
            loge("not support the format[%d] now", mInBuffer.colorFormat);
            return -1;
        }

        if(size != (pEncodeParam->src_width * pEncodeParam->src_height * 3/2))
        {
            fseek(in_file, 0L, SEEK_SET);
            goto READ_DATA_FROM_FILE;
        }
        CdcMemFlushCache(_memops, mInBuffer.pAddrVirY, nInBufferSize);

        //* call function
        if(mIspFunction.RoiConfig.bEnable)
            init_Roi_config(NULL, &mIspFunction.RoiConfig, &s_ops_set, 1);

        if(mIspFunction.nRotateAngle == 1 || mIspFunction.nRotateAngle == 3)
        {
            unsigned int tmpSize = pOutBuffer[i].nWidth;
            pOutBuffer[i].nWidth  = pOutBuffer[i].nHeight;
            pOutBuffer[i].nHeight = tmpSize;
        }
        if(mIspFunction.bCropFlag == 1)
        {
            mCropInfo.nLeft = 800;
            mCropInfo.nTop  = 800;
            mCropInfo.nWidth = 128;//mInBuffer.nWidth - 128;
            mCropInfo.nHeight = 128;//mInBuffer.nHeight - 128;
            mIspFunction.pCropInfo = &mCropInfo;
        }

        VencEncppFunction(pEncpp, &mInBuffer, &pOutBuffer[i], &mIspFunction);

        //* save data
        if(mIspFunction.RoiConfig.bEnable == 0)
        {
            CdcMemFlushCache(_memops, pOutBuffer[i].pAddrVirY, nOutBufferSize);
            fwrite(pOutBuffer[i].pAddrVirY, 1, nOutBufferSize, out_file);
        }

        if(mIspFunction.RoiConfig.bEnable)
        {
            int idx;
            int nLen = 0;
            for(idx=0; idx<mIspFunction.RoiConfig.num; idx++)
            {
                FILE *Roi_file;
                char name[128];
                if (idx == 0) {
                    sprintf(name, "/mnt/roi_test/all_Roi_%d.yuv", idx);
                    Roi_file = fopen(name, "wb");
                    fwrite(mIspFunction.RoiConfig.pRoiYAddrVir, 1, mIspFunction.RoiConfig.size, Roi_file);
                    fclose(Roi_file);
                }
                sprintf(name, "/mnt/roi_test/Roi_%d.yuv", idx);
                Roi_file = fopen(name, "wb");
                if(Roi_file == NULL)
                {
                        loge("open out_file fail\n");
                        goto EXIT;
                }

                int yLen = ALIGN_16B(mIspFunction.RoiConfig.sRect[idx].nWidth)\
                 * ALIGN_16B(mIspFunction.RoiConfig.sRect[idx].nHeight);
                EncAdapterMemFlushCache(mIspFunction.RoiConfig.pRoiYAddrVir + nLen, yLen);
                EncAdapterMemFlushCache(mIspFunction.RoiConfig.pRoiCAddrVir + nLen/2, yLen/2);
                //CdcMemFlushCache(memops, mIspFunction.RoiConfig.pRoiYAddrVir + nLen, yLen);
                fwrite(mIspFunction.RoiConfig.pRoiYAddrVir + nLen, 1, yLen, Roi_file);
                fwrite(mIspFunction.RoiConfig.pRoiCAddrVir + nLen/2, 1, yLen/2, Roi_file);
                fclose(Roi_file);
                nLen += yLen;
            }
        }
    }
EXIT:

    //* free
    if(in_file)
        fclose(in_file);

    if(out_file)
        fclose(out_file);

    if(mIspFunction.RoiConfig.bEnable)
    {
        if(mIspFunction.RoiConfig.pRoiYAddrVir != NULL)
        {
            EncAdapterMemPfree(mIspFunction.RoiConfig.pRoiYAddrVir);
            mIspFunction.RoiConfig.pRoiYAddrVir = NULL;
        }

        if(mIspFunction.RoiConfig.pRoiCAddrVir != NULL)
        {
            EncAdapterMemPfree(mIspFunction.RoiConfig.pRoiCAddrVir);
            mIspFunction.RoiConfig.pRoiCAddrVir = NULL;
        }
    }

    if(veOps)
    {
        if(_memops)
        {
            if(mInBuffer.pAddrVirY)
                EncAdapterMemPfree(mInBuffer.pAddrVirY);
            if(pOutBuffer)
            {
                for(i = 0; i < nTotalOutputBufNum; i++)
                {
                    if(pOutBuffer[i].pAddrVirY)
                        EncAdapterMemPfree(pOutBuffer[i].pAddrVirY);
                }
                free(pOutBuffer);
                pOutBuffer = NULL;
            }
        }
        CDCVeRelease(veOps,pVeopsSelf);
    }

    if(_memops)
        CDCMemClose(_memops);

    if(pEncpp)
        VencEncppDestroy(pEncpp);
	if (pGdcParam)
		FREE(pGdcParam);
	if (pSharpParam)
		FREE(pSharpParam);
    return ret;
}

void* ChannelThread(void* param)
{
    encoder_Context* pEncContext = (encoder_Context*)param;

    VencBaseConfig baseConfig;
    VencAllocateBufferParam bufferParam;
    VideoEncoder* pVideoEnc = NULL;
    VencInputBuffer inputBuffer;
    VencInputBuffer *pInputBuf = NULL;
    InputBufferInfo *pInputBufInfo = NULL;
    VencOutputBuffer outputBuffer;
    VencHeaderData sps_pps_data;
    unsigned char *uv_tmp_buffer = NULL;
    unsigned int afbc_header_size;
    ptsDebugInfo mPtsInfo;
    VencSeiInfo sei_info = {0};
    VencSeiParam sei_para = {0};

    memset(&mPtsInfo, 0, sizeof(ptsDebugInfo));

    int seek_end = 0;
    int file_frm_num = 0;
    int is_back_cnt = 0;
    int cur_pos = 0;

    int result = 0;
    int i = 0;
    long long pts = 0;
    unsigned char yu12_nv12_flag = 0;
    unsigned char yu12_nv21_flag = 0;
    char logcat_buf[1024];

    FILE_STRUCT *in_file = NULL;
    FILE_STRUCT *out_file = NULL;
    FILE_STRUCT *reference_file = NULL;
    FILE_STRUCT *log_file = NULL;
    char *input_path = NULL;
    char *output_path = NULL;
    char *reference_path = NULL;
    char *log_path = NULL;
    char *reference_buffer = NULL;
    char *log_buffer = logcat_buf;
    int log_len = 0;
    int value;

    unsigned int m = 0;
    unsigned int cycle_num = 1;

    int bHadStartTimeFlag = 0;
    long long nFirstEncodePicTime = 0;
    long long curSysTime  = 0;

    int64_t nCurPts = 0;
    int64_t nDuration = 0;

    VencCopyROIConfig pRoiConfig;
    memset(&pRoiConfig, 0, sizeof(VencCopyROIConfig));

    double pre_psnr = 0;
    AvTimer* pAvTimer = AvTimerCreate();

    VencInsertData insert_data = {NULL, JPEG_MAX_SEG_LEN*3, JPEG_MAX_SEG_LEN*3, 20, BUF_IDLE};

    for(m=0; m<cycle_num; m++)
    {
		StaticTImeInfo stat_demo_time;
		memset(&stat_demo_time, 0, sizeof(StaticTImeInfo));
		snprintf(stat_demo_time.stat_name, sizeof(stat_demo_time.stat_name), "demo ch %d", pEncContext->nChannel);

        VencMBSumInfo sMbSumInfo;
        memset(&sMbSumInfo, 0, sizeof(VencMBSumInfo));
        unsigned long long sum_sse = 0;
        unsigned long long min_sse = 0;
        unsigned long long max_sse = 0;
        unsigned long long avr_sse = 0;
        unsigned int       min_sse_frame = 0;
        unsigned int       max_sse_frame = 0;
        unsigned long long avr_sse_period = 0;

        /******** begin set the default encode param ********/
        encode_param_t    encode_param;
        memset(&encode_param, 0, sizeof(encode_param));
        encode_param.src_width = 1280;
        encode_param.src_height = 736;
        encode_param.dst_width = 1280;
        encode_param.dst_height = 736;
        encode_param.bit_rate = 2*1024*1024;
        encode_param.frame_rate = 30;
        encode_param.maxKeyFrame = 30;
        encode_param.encode_format = VENC_CODEC_H264;
        encode_param.encode_frame_num = 100;
        encode_param.test_cycle = 1;
        encode_param.nChannel = pEncContext->nChannel;
        strcpy((char*)encode_param.intput_file,        "/data/camera/720p-30zhen.yuv");
        strcpy((char*)encode_param.output_file,        "/data/camera/720p.264");
        strcpy((char*)encode_param.reference_file,
        "/mnt/bsp_ve_test/reference_data/reference.jpg");

        if(encode_param.dst_width == 3840)
            encode_param.bit_rate = 20*1024*1024;
        else if(encode_param.dst_width == 1920)
            encode_param.bit_rate = 10*1024*1024;
        else if(encode_param.dst_width ==1280)
            encode_param.bit_rate = 6*1024*1024;//6*1024*1024;
        else if(encode_param.dst_width == 640)
            encode_param.bit_rate = 2*1024*1024;
        else if(encode_param.dst_width == 288)
            encode_param.bit_rate = 1*1024*1024;
        else
            encode_param.bit_rate = 4*1024*1024;
        /******** end set the default encode param ********/


        /******** begin parse the config paramter ********/
        if(pEncContext->argc >= 2)
        {
            printf("******************************\n");
            for(i = 1; i < (int)pEncContext->argc; i += 2)
            {
                ParseArgument(&encode_param, pEncContext->argv[i], pEncContext->argv[i + 1]);
            }
            printf("******************************\n");
        }
        else
        {
            printf(" we need more arguments \n");
            PrintDemoUsage();
            return 0;
        }
        /******** end parse the config paramter ********/

        if(encode_param.encode_format == 5)
        {
            testEncppFunction(&encode_param);
            goto DEMO_END;
        }
        encode_param.src_size = encode_param.src_width * encode_param.src_height;
        encode_param.dst_size = encode_param.dst_width * encode_param.dst_height;

        nDuration = 1000/encode_param.frame_rate;

        /******** begin open input , output and reference file ********/
        input_path = encode_param.intput_file;
        output_path = encode_param.output_file;
        log_path = encode_param.log_file;
        cycle_num = encode_param.test_cycle;

        in_file = fopen(input_path, "rb");
        if(in_file == NULL)
        {
            loge("open in_file fail\n");
            goto out;
        }
        fseek(in_file, 0L, SEEK_END);
        file_frm_num = ftell(in_file);
        fseek(in_file, 0L, SEEK_SET);
        file_frm_num /= (encode_param.src_width * encode_param.src_height * 3/2);
        seek_end = (file_frm_num - 1) * (encode_param.src_width * encode_param.src_height * 3/2);

        if(pEncContext->nTotalChannelNum > 1)
        {
            char tmp_out_path[64] = {0};

            sprintf(tmp_out_path, "%s_%d",output_path, pEncContext->nChannel);

            logd("out_file = %s, tmp_out_file = %s, channel = %d, ",
                output_path, tmp_out_path, pEncContext->nChannel);

            out_file = fopen(tmp_out_path, "wb");
        }
        else
            out_file = fopen(output_path, "wb");

        if(out_file == NULL)
        {
            loge("open out_file fail\n");
            fclose(in_file);
            goto out;
        }

        log_file = fopen(log_path, "ab");
        if(log_file == NULL)
        {
            loge("open log_file fail\n");
        }

        if(encode_param.compare_flag)
        {
            reference_path = encode_param.reference_file;
            reference_file = fopen(reference_path, "r");
            if(reference_file == NULL)
            {
                loge("open reference_file fail\n");
                goto out;
            }

            reference_buffer = (char*)MALLOC(1*1024*1024);
            if(reference_buffer == NULL)
            {
                loge("malloc reference_buffer error\n");
                goto out;
            }
        }
        /******** end open input , output and reference file ********/

        /******** begin set baseConfig param********/
        memset(&baseConfig, 0 ,sizeof(VencBaseConfig));
        memset(&bufferParam, 0 ,sizeof(VencAllocateBufferParam));
        baseConfig.memops = CDCGetMemOps();
        if (baseConfig.memops == NULL)
        {
            printf("CDCGetMemOps failed\n");
            goto out;
        }
        CDCMemOpen(baseConfig.memops);
        baseConfig.nInputWidth= ALIGN_XXB(16, encode_param.src_width);
        baseConfig.nInputHeight = ALIGN_XXB(16, encode_param.src_height);
        baseConfig.nStride = ALIGN_XXB(16, encode_param.src_width);
        baseConfig.nDstWidth = ALIGN_XXB(16, encode_param.dst_width);
        baseConfig.nDstHeight = ALIGN_XXB(16, encode_param.dst_height);
        baseConfig.bEncH264Nalu = 0;
        /*
            * the format of yuv file is yuv420p, but the old ic only support the yuv420sp,
            * so use the func yu12_nv12() to config all the format.
            */
        //baseConfig.eInputFormat = VENC_PIXEL_YUV420SP;
        baseConfig.eInputFormat = encode_param.pixel_format;
        baseConfig.eOutputFormat = encode_param.output_format;
        baseConfig.extend_flag  = encode_param.extend_flag;
        baseConfig.nOnlineShareBufNum  = encode_param.nShare_buf_num;
		baseConfig.bVcuOn = encode_param.bvcu;
		//baseConfig.mGdcParam    = encode_param.mGdcParam;
		baseConfig.rec_lbc_mode = encode_param.rec_lbc_mode;

        logd("****eInputFormat = %d, extend_flag = %d, nOnlineShareBufNum = %d ",
        		baseConfig.eInputFormat, baseConfig.extend_flag,
        		baseConfig.nOnlineShareBufNum );

        logd("size of sharp param: %d, %d", sizeof(sEncppSharpParamDynamic), sizeof(sEncppSharpParamStatic));

        if(baseConfig.eInputFormat == VENC_PIXEL_YUV420P)
        {
            #ifdef YU12_NV12
                    baseConfig.eInputFormat = VENC_PIXEL_YUV420SP;
                    yu12_nv12_flag = 1;
            #endif

            #ifdef YU12_NV21
                baseConfig.eInputFormat = VENC_PIXEL_YVU420SP;
                yu12_nv21_flag = 1;
            #endif
        }
        //* ve require 16-align
        int nAlignW = ALIGN_XXB(16, baseConfig.nInputWidth);
        int nAlignH = ALIGN_XXB(16, baseConfig.nInputHeight);

        if(baseConfig.eInputFormat == VENC_PIXEL_YUYV422
           || baseConfig.eInputFormat == VENC_PIXEL_UYVY422)
        {
            bufferParam.nSizeY = nAlignW*nAlignH*2;
            bufferParam.nSizeC = 0;
        }
        else if (baseConfig.eInputFormat == VENC_PIXEL_ARGB
            || baseConfig.eInputFormat == VENC_PIXEL_RGBA
            || baseConfig.eInputFormat == VENC_PIXEL_ABGR
            || baseConfig.eInputFormat == VENC_PIXEL_BGRA)
        {
            bufferParam.nSizeY = nAlignW * nAlignH * 4;
            bufferParam.nSizeC = 0;
            logd("nSizeY=%u, nAlignW=%d, nAlignH=%d", bufferParam.nSizeY, nAlignW, nAlignH);
        }
        else
        {
            bufferParam.nSizeY = nAlignW*nAlignH + 64;
            bufferParam.nSizeC = nAlignW*nAlignH/2 + 64;
        }

        if(encode_param.test_afbc == 1)
        {
            afbc_header_size = ((baseConfig.nInputWidth +127)>>7)*((baseConfig.nInputHeight+31)>>5)*96;
            logd("size_y:%x, size_c:%x, afbc_header:%x\n",
                                    bufferParam.nSizeY,
                                    bufferParam.nSizeC,
                                    afbc_header_size);
            bufferParam.nSizeY += afbc_header_size + bufferParam.nSizeC;
            bufferParam.nSizeC = 0;
            logd("afbc buffer size:%x\n", bufferParam.nSizeY);

            baseConfig.eInputFormat = VENC_PIXEL_AFBC_AW;
        }

	    #if 0
	//just test
        if(pEncContext->bOnlineMode == 0) {
		encode_param.test_lbc = 0;
	}
	    #endif

        unsigned int lbc_ext_size = 0;
        if(encode_param.test_lbc != 0)
        {
            int y_stride = 0;
            int yc_stride = 0;
            int bit_depth = 8;
            int com_ratio_even = 0;
            int com_ratio_odd = 0;
            int pic_width_32align = (baseConfig.nInputWidth + 31) & ~31;
            int pic_height = baseConfig.nInputHeight;

            if(encode_param.test_lbc == 1)
                baseConfig.bLbcLossyComEnFlag1_5x = 1;
            else if(encode_param.test_lbc == 2)
                baseConfig.bLbcLossyComEnFlag2x = 1;
            else if(encode_param.test_lbc == 3)
                baseConfig.bLbcLossyComEnFlag2_5x = 1;

            if(baseConfig.bLbcLossyComEnFlag1_5x == 1)
            {
                com_ratio_even = 670;
                com_ratio_odd = 658;
                y_stride = ((com_ratio_even * pic_width_32align * bit_depth / 1000 +511) & (~511)) >> 3;
                yc_stride = ((com_ratio_odd * pic_width_32align * bit_depth / 500 + 511) & (~511)) >> 3;
            }
            else if(baseConfig.bLbcLossyComEnFlag2x == 1)
            {
                com_ratio_even = 600;
                com_ratio_odd = 450;
                y_stride = ((com_ratio_even * pic_width_32align * bit_depth / 1000 +511) & (~511)) >> 3;
                yc_stride = ((com_ratio_odd * pic_width_32align * bit_depth / 500 + 511) & (~511)) >> 3;
            }
            else if(baseConfig.bLbcLossyComEnFlag2_5x == 1)
            {
                com_ratio_even = 440;
                com_ratio_odd = 380;
                y_stride = ((com_ratio_even * pic_width_32align * bit_depth / 1000 +511) & (~511)) >> 3;
                yc_stride = ((com_ratio_odd * pic_width_32align * bit_depth / 500 + 511) & (~511)) >> 3;
            }
            else
            {
                y_stride = ((pic_width_32align * bit_depth + pic_width_32align / 16 * 2 + 511) & (~511)) >> 3;
                yc_stride = ((pic_width_32align * bit_depth * 2 + pic_width_32align / 16 * 4 + 511) & (~511)) >> 3;
            }


            int total_stream_len = (y_stride + yc_stride) * pic_height / 2;

            //* add more 1KB to fix ve-lbc-error
            lbc_ext_size = 1*1024;
            total_stream_len += lbc_ext_size;

            logd("LBC in buf:com_ratio: %d, %d, w32alin = %d, pic_height = %d, \
                y_s = %d, yc_s = %d, total_len = %d,\n",
                 com_ratio_even, com_ratio_odd,
                 pic_width_32align, pic_height,
                 y_stride, yc_stride, total_stream_len);

            bufferParam.nSizeY = total_stream_len;
            bufferParam.nSizeC = 0;
            baseConfig.eInputFormat = VENC_PIXEL_LBC_AW;
        }
        /******** end set baseConfig param********/

        //create encoder
        logd("encode_param.encode_format:%d\n", encode_param.encode_format);
        pVideoEnc = VencCreate(encode_param.encode_format);

        pEncContext->pVideoEnc = pVideoEnc;
        //set enc parameter
        result = setEncParam(pEncContext ,&encode_param);
        if(result)
        {
            loge("setEncParam error, return");
            goto out;
        }

        baseConfig.bOnlineMode    = pEncContext->bOnlineMode;
        baseConfig.bOnlineChannel = pEncContext->bOnlineChannel;
		baseConfig.nChannel = encode_param.nChannel;
#if ENABLE_PAGE_BUF_MODE
        int pagebuf_param = 1;
        VencSetParameter(pVideoEnc, VENC_IndexParamEnableRecRefBufReduceFunc, &pagebuf_param);
#endif
        VencInit(pVideoEnc, &baseConfig);
        logd("bEnableGdc = %d, bEnableSharp = %d",
            encode_param.bEnableGdc, encode_param.bEnableSharp);

        memcpy(&pEncContext->baseConfig, &baseConfig, sizeof(VencBaseConfig));

#if ENABLE_GET_WRITE_BACK_YUV
        initWbYuv(&pEncContext->mWbYuvFuncInfo, &encode_param, pVideoEnc);
#endif

#if SET_ROI_PARAM
		for(i = 0; i < ROI_NUM; i++)
		{
			if(encode_param.encode_format == VENC_CODEC_H264)
			{
				VencSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &encode_param.h264_func.sRoiConfig[i]);
			}
			else if(encode_param.encode_format == VENC_CODEC_H265)
			{
				VencSetParameter(pVideoEnc, VENC_IndexParamROIConfig, &encode_param.h265_func.sRoiConfig[i]);
			}
		}
#endif
        if((pRoiConfig.bEnable==1) && \
            (encode_param.encode_format == VENC_CODEC_H264 || \
            encode_param.encode_format == VENC_CODEC_JPEG || \
            encode_param.encode_format == VENC_CODEC_H265))
        {
            init_Roi_config(&baseConfig, &pRoiConfig, NULL, 0);
            VencSetParameter(pVideoEnc, VENC_IndexParamRoi, &pRoiConfig);
        }

#if ENABLE_SEI
        sei_para.nSeiNum = encode_param.sei_num;
        sei_para.pSeiData = (VencSeiData*)MALLOC(sei_para.nSeiNum*sizeof(VencSeiData));

        if(sei_para.nSeiNum > 0)
        {
            sei_info.nInfoBitFlags |= VencInfoType_1Times;
            sei_para.pSeiData[0].pBuffer = (unsigned char*)MALLOC(sizeof(VencSeiInfo1Times));
            sei_para.pSeiData[0].nBufLen = sizeof(VencSeiInfo1Times);
            sei_para.pSeiData[0].nDataLen = sizeof(VencSeiInfo1Times);
            sei_para.pSeiData[0].nType = 5;
        }
        if(sei_para.nSeiNum > 1)
        {
            sei_info.nInfoBitFlags |= VencInfoType_10Sec;
            sei_para.pSeiData[1].pBuffer = (unsigned char*)MALLOC(sizeof(VencSeiInfo10Sec));
            sei_para.pSeiData[1].nBufLen = sizeof(VencSeiInfo10Sec);
            sei_para.pSeiData[1].nDataLen = sizeof(VencSeiInfo10Sec);
            sei_para.pSeiData[1].nType = 5;
        }
        if(sei_para.nSeiNum > 2)
        {
            sei_info.nInfoBitFlags |= VencInfoType_1Sec;
            sei_para.pSeiData[2].pBuffer = (unsigned char*)MALLOC(sizeof(VencSeiInfo1Sec));
            sei_para.pSeiData[2].nBufLen = sizeof(VencSeiInfo1Sec);
            sei_para.pSeiData[2].nDataLen = sizeof(VencSeiInfo1Sec);
            sei_para.pSeiData[2].nType = 5;
        }
#endif
        if(encode_param.frequency > 0)
            VencSetFreq(pVideoEnc, encode_param.frequency);

        if(encode_param.encode_format == VENC_CODEC_H264 || \
        encode_param.encode_format == VENC_CODEC_H265)
        {
            unsigned int head_num = 0;
            if(encode_param.encode_format == VENC_CODEC_H264)
            {
                VencGetParameter(pVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
                unsigned char value = 1;
                //VencGetParameter(pVideoEnc, VENC_IndexParamAllParams, &value);
            }
            else if(encode_param.encode_format == VENC_CODEC_H265)
            {
                VencGetParameter(pVideoEnc, VENC_IndexParamH265Header, &sps_pps_data);
                unsigned char value = 1;
                //VencGetParameter(pVideoEnc, VENC_IndexParamAllParams, &value);
            }

        #if SAVE_AWSP
            char mask[16] = "awspcialstream";
            logd(" make special stream save special data size: %d ", sps_pps_data.nLength);
            fwrite(mask, 14, sizeof(char), out_file);
            value = 0x115;
            fwrite(&value, 1, sizeof(int), out_file);
            value = 1280;
            fwrite(&value, 1, sizeof(int), out_file);
            value = 736;
            fwrite(&value , 1, sizeof(int), out_file);
            value = 0;
            fwrite(&value, 1, sizeof(int), out_file);
            fwrite(&sps_pps_data.nLength, 1, sizeof(int), out_file);
        #endif
            fwrite(sps_pps_data.pBuffer, 1, sps_pps_data.nLength, out_file);
            logd("sps_pps_data.nLength: %d", sps_pps_data.nLength);
            //for(head_num=0; head_num<sps_pps_data.nLength; head_num++)
                //logd("the sps_pps :%02x\n", *(sps_pps_data.pBuffer+head_num));

            if(encode_param.compare_flag)
            {
                result = fread(reference_buffer, 1, sps_pps_data.nLength, reference_file);
                if(result != (int)sps_pps_data.nLength)
                {
                    loge("read reference_file sps info error\n");
                    goto out;
                }

                for(i=0; i<(int)sps_pps_data.nLength; i++)
                {
                    if(sps_pps_data.pBuffer[i] != reference_buffer[i])
                    {
                        loge("the sps %d byte is not same, ref[%02x], cur[%02x]\n",
                                                            i,
                                                            reference_buffer[i],
                                                            sps_pps_data.pBuffer[i]);
                        goto out;
                    }
                }
            }
        }

        if(baseConfig.bOnlineChannel == 0)
        {
            for(i = 0; i < DEMO_INPUT_BUFFER_NUM; i++)
            {
                VencAllocateInputBuf(pVideoEnc, &bufferParam, &pEncContext->mInputBufMgr.buffer_node[i].inputbuffer);
                enqueue(&pEncContext->mInputBufMgr.valid_quene, &pEncContext->mInputBufMgr.buffer_node[i]);
            }
        }

        VencCbType vencCallBack;
        vencCallBack.EventHandler = EventHandler;
        if(baseConfig.bOnlineChannel == 0)
            vencCallBack.InputBufferDone = InputBufferDone;

        VencSetCallbacks(pVideoEnc, &vencCallBack, pEncContext);

        VencStart(pVideoEnc);

        if(yu12_nv12_flag || yu12_nv21_flag)
        {
            uv_tmp_buffer = (unsigned char*)MALLOC(baseConfig.nInputWidth
                                            * baseConfig.nInputHeight/2);
            if(uv_tmp_buffer == NULL)
            {
                loge("malloc uv_tmp_buffer fail\n");
                goto out;
            }
        }

#ifdef USE_SVC
        // used for throw frame test with SVC
        int TemporalLayer = -1;
        char p9bytes[9] = {0};
#endif

        unsigned int testNumber = 0;
        while(testNumber < encode_param.encode_frame_num)
        {
			set_start_time(&stat_demo_time);
            if(baseConfig.bOnlineChannel == 0)
            {
                pInputBufInfo = dequeue(&pEncContext->mInputBufMgr.valid_quene);
                logv("get input buf, pInputBufInfo = %p", pInputBufInfo);
                if(pInputBufInfo == NULL)
                {
                    logv(" get input buffer failed, ret = %d", result);
                    USLEEP(10*1000);
                    continue;
                }
                pInputBuf = &pInputBufInfo->inputbuffer;

                {
#if ENABLE_JPEG_INSERT_DATA
                    if(testNumber < 256 && encode_param.encode_format == VENC_CODEC_JPEG)
                    {
                        if(insert_data.pBuffer == NULL)
                        {
                            insert_data.pBuffer = (unsigned char *)MALLOC(insert_data.nBufLen);
                        }
                        if(insert_data.pBuffer)
                        {
                            VencGetParameter(pVideoEnc, VENC_IndexParamInsertDataBufStatus, &insert_data.eStatus);
                            if(insert_data.eStatus == BUF_IDLE)
                            {
                                memset(insert_data.pBuffer, (unsigned char)testNumber, JPEG_MAX_SEG_LEN*3);
                                VencSetParameter(pVideoEnc, VENC_IndexParamInsertData, &insert_data);
                            }
                        }
                    }
#endif
#if NO_READ_WRITE
                    if(testNumber == 0)
#endif
                    {
                        if(encode_param.test_afbc == 1
                           || encode_param.test_lbc != 0
                           || baseConfig.eInputFormat == VENC_PIXEL_YUYV422
                           || baseConfig.eInputFormat == VENC_PIXEL_UYVY422)
                        {
                            unsigned int size1;
                            unsigned int lbc_size = bufferParam.nSizeY - lbc_ext_size;

                            size1 = fread(pInputBuf->pAddrVirY, 1, lbc_size, in_file);
                            if(size1 != lbc_size)
                            {
                                fseek(in_file, 0L, SEEK_SET);
                                size1 = fread(pInputBuf->pAddrVirY, 1, lbc_size, in_file);
                            }
                        }
                        else if (baseConfig.eInputFormat == VENC_PIXEL_ARGB
                            || baseConfig.eInputFormat == VENC_PIXEL_RGBA
                            || baseConfig.eInputFormat == VENC_PIXEL_ABGR
                            || baseConfig.eInputFormat == VENC_PIXEL_BGRA)
                        {
                            unsigned int size1;

                            size1 = fread(pInputBuf->pAddrVirY, 1, bufferParam.nSizeY, in_file);
                            if(size1 != bufferParam.nSizeY)
                            {
                                fseek(in_file, 0L, SEEK_SET);
                                size1 = fread(pInputBuf->pAddrVirY, 1, bufferParam.nSizeY, in_file);
                            }
                        }
                        else
                        {
	                        int size1, size2, ysize, csize, total_size;
                            int width_al = ALIGN_XXB(16, encode_param.src_width);
                            int height_al = ALIGN_XXB(16, encode_param.src_height);

                            ysize = encode_param.src_width * encode_param.src_height;
                            csize = ysize / 4;
                            total_size = ysize + 2*csize;

                            if(cur_pos == seek_end)
                            {
                                is_back_cnt = 1;
                            }
                            if(is_back_cnt)
                            {
                                fseek(in_file, -2*total_size, SEEK_CUR);
                                cur_pos -= 2 * total_size;
                            }
                            if(cur_pos == 0)
                            {
                                is_back_cnt = 0;
                            }

                            if(width_al != encode_param.src_width || height_al != encode_param.src_height)
                            {
                                unsigned char *pY, *pC;
                                int w, h, cnt;
                                int cwidth = encode_param.src_width >> 1;
                                int cheight = encode_param.src_height >> 1;
                                int cwidth_al = width_al >> 1;
                                int cheight_al = height_al >> 1;
                                int yoffset = width_al - encode_param.src_width;
                                int coffset = yoffset >> 1;

                                pY = pInputBuf->pAddrVirY;
                                if(width_al != encode_param.src_width)
                                {
                                    for(h = 0; h < encode_param.src_height; h++)
                                    {
                                        fread(pY, 1, encode_param.src_width, in_file);
                                        memset(&pY[encode_param.src_width], pY[encode_param.src_width-1], yoffset);
                                        pY += width_al;
                                    }
                                }
                                else
                                {
                                    fread(pY, 1, ysize, in_file);
                                    pY += ysize;
                                }
                                for(h = encode_param.src_height; h < height_al; h++)
                                {
                                    memcpy(pY, &pY[-width_al], width_al);
                                    pY += width_al;
                                }

                                for(cnt = 0; cnt < 2; cnt++)
                                {
                                    pC = pInputBuf->pAddrVirC + cnt * cwidth_al * cheight_al;
                                    if(cwidth_al != cwidth)
                                    {
                                        for(h = 0; h < cheight; h++)
                                        {
                                            fread(pC, 1, cwidth, in_file);
                                            memset(&pC[cwidth], pC[cwidth-1], coffset);
                                            pC += cwidth_al;
                                        }
                                    }
                                    else
                                    {
                                        fread(pC, 1, csize, in_file);
                                        pC += csize;
                                    }
                                    for(h = cheight; h < cheight_al; h++)
                                    {
                                        memcpy(pC, &pC[-cwidth_al], cwidth_al);
                                        pC += cwidth_al;
                                    }
                                }

                                cur_pos += total_size;

                                if(yu12_nv12_flag)
    	                        {
    	                            yu12_nv12(width_al, height_al,
    	                                 pInputBuf->pAddrVirC, uv_tmp_buffer, encode_param.encode_format);
    	                        }
    	                        else if(yu12_nv21_flag)
    	                        {
    	                           yu12_nv21(width_al, height_al,
    	                                 pInputBuf->pAddrVirC, uv_tmp_buffer, encode_param.encode_format);
    	                        }

                                if(testNumber < 4 && 0)
                                {
                                    char path[256] = {0};
                                    sprintf(path, "/mnt/extsd/wgj/test_%dx%d.yuv", width_al, height_al);
                                    FILE_STRUCT *fpTest = fopen(path, "ab+");
                                    fwrite(pInputBuf->pAddrVirY, 1, width_al*height_al, fpTest);
                                    fwrite(pInputBuf->pAddrVirC, 1, cwidth_al*cheight_al*2, fpTest);
                                    fclose(fpTest);
                                }
                            }
                            else
                            {
    	                        size1 = fread(pInputBuf->pAddrVirY, 1,
    	                            baseConfig.nInputWidth*baseConfig.nInputHeight, in_file);
    	                        size2 = fread(pInputBuf->pAddrVirC, 1,
    	                            baseConfig.nInputWidth*baseConfig.nInputHeight/4, in_file);
    	                        size2 += fread(pInputBuf->pAddrVirC+csize, 1,
    	                            baseConfig.nInputWidth*baseConfig.nInputHeight/4, in_file);

    	                        if((size1!= baseConfig.nInputWidth*baseConfig.nInputHeight)
    	                            || (size2!= baseConfig.nInputWidth*baseConfig.nInputHeight/2))
    	                        {
                                    logw("reach end of in_file, size1:%d, size2:%d", size1, size2);
    	                            fseek(in_file, 0L, SEEK_SET);
    	                            size1 = fread(pInputBuf->pAddrVirY, 1,
    	                                     baseConfig.nInputWidth*baseConfig.nInputHeight, in_file);
    	                            size2 = fread(pInputBuf->pAddrVirC, 1,
    	                                     baseConfig.nInputWidth*baseConfig.nInputHeight/2, in_file);
    	                        }
                                cur_pos += total_size;

    	                        if(yu12_nv12_flag)
    	                        {
    	                            yu12_nv12(baseConfig.nInputWidth, baseConfig.nInputHeight,
    	                                 pInputBuf->pAddrVirC, uv_tmp_buffer, encode_param.encode_format);
    	                        }
    	                        else if(yu12_nv21_flag)
    	                        {
    	                           yu12_nv21(baseConfig.nInputWidth, baseConfig.nInputHeight,
    	                                 pInputBuf->pAddrVirC, uv_tmp_buffer, encode_param.encode_format);
    	                        }
                            }
                        }
                    }
                }

                if(encode_param.en_crop == 1)
                {
                    pInputBuf->bEnableCorp = 1;
                    pInputBuf->sCropInfo.nLeft =  encode_param.crop_l;
                    pInputBuf->sCropInfo.nTop  =  encode_param.crop_t;
                    pInputBuf->sCropInfo.nWidth  =  encode_param.crop_w;
                    pInputBuf->sCropInfo.nHeight =  encode_param.crop_h;
                }
                pts += US_PER_S/encode_param.frame_rate;
                pInputBuf->nPts = pts;
                pInputBuf->bNeedFlushCache = 1;
                VencQueueInputBuf(pVideoEnc, pInputBuf);

                enqueue(&pEncContext->mInputBufMgr.empty_quene, pInputBufInfo);
            }

#ifdef SET_MB_INFO
            setMbMode(pEncContext, &encode_param);
#endif

#ifdef SET_SMART
            if(testNumber == 0)
            {
                if(encode_param.encode_format == VENC_CODEC_H264)
                {
                    VencSetParameter(pVideoEnc, VENC_IndexParamSmartFuntion,
                                                            &h264_func.sH264Smart);
                }
                else if(encode_param.encode_format == VENC_CODEC_H265)
                {
                    VencSetParameter(pVideoEnc, VENC_IndexParamSmartFuntion,
                                                            &h265_func.h265Smart);
                }
            }
#endif

#if ENABLE_SEI
            if(sei_para.nSeiNum > 0
                && testNumber % 10 == 0
                && (encode_param.encode_format == VENC_CODEC_H264 || encode_param.encode_format == VENC_CODEC_H265))
            {
                VencGetParameter(pVideoEnc, VENC_IndexParamSeiInfo, &sei_info);
                memcpy(sei_para.pSeiData[0].pBuffer, sei_info.mInfo1Times, sizeof(VencSeiInfo1Times));
                if(sei_para.nSeiNum > 1)
                    memcpy(sei_para.pSeiData[1].pBuffer, sei_info.mInfo10Sec, sizeof(VencSeiInfo10Sec));
                if(sei_para.nSeiNum > 2)
                    memcpy(sei_para.pSeiData[2].pBuffer, sei_info.mInfo1Sec, sizeof(VencSeiInfo1Sec));
                VencSetParameter(pVideoEnc, VENC_IndexParamSeiParam, &sei_para);
            }
#endif

            if(encode_param.limit_encode_speed == 1)
            {
                if(bHadStartTimeFlag == 0)
                {
                    bHadStartTimeFlag = 1;
                    pAvTimer->SetTime(pAvTimer, nCurPts);
                    pAvTimer->Start(pAvTimer);

                    nFirstEncodePicTime = GetNowUs();
                }
                else
                {
                    nCurPts += (nDuration*1000);
                    int64_t nCurTime = pAvTimer->GetTime(pAvTimer);

                    int64_t nTimeDiff = nCurPts - nCurTime;
                    int64_t nWaitTimeMs = 0;
                    if(nTimeDiff > 5*1000)
                    {
                        nWaitTimeMs = nTimeDiff/1000;
                        USLEEP(nWaitTimeMs*1000);
                    }
                    logv("** encoder: nCurPts = %lld, nWaitTimeMs = %lld",nCurPts, nWaitTimeMs);

                }

                if((testNumber+1)%30 == 0)
                {
                    curSysTime = GetNowUs();
                    long long curTotalTime = curSysTime - nFirstEncodePicTime;
                    logd("The 1st encoder:encodeFrameCout=%d,costTime=%.2f s,renderFps=%.2f",
                         (testNumber+1), (double)curTotalTime/1000/1000,
                         (double)(testNumber+1)*1000*1000/curTotalTime);
                }
            }

            result = VencDequeueOutputBuf(pVideoEnc, &outputBuffer);

            logv("get bitstream, ret = %d", result);
            if(result != 0)
            {
                logv("get stream failed, ret = %d", result);
                USLEEP(10*1000);
                continue;
            }
#if SHOW_PTS_INFO
            mPtsInfo.curPts = outputBuffer.nPts;
            showPtsInfo(&mPtsInfo);
#endif

#ifdef USE_SUPER_FRAME
            if((sSuperFrameCfg.eSuperFrameMode==VENC_SUPERFRAME_DISCARD) && (result==-1))
            {
                logd("VENC_SUPERFRAME_DISCARD: discard frame %d\n",testNumber);
                continue;
            }
#endif
            if(result == -1)
            {
                goto out;
            }

#ifdef USE_SVC
            // used for throw frame test with SVC
            memcpy(p9bytes, outputBuffer.pData0, 9);
            TemporalLayer = SeekPrefixNAL(p9bytes);

            switch(TemporalLayer)
            {

                case 3:
                case 2:
                case 1:
                    logv("just write the PrefixNAL\n");
                    fwrite(outputBuffer.pData0, 1, 9, out_file);
                    break;

                default:
                    logv("\nTemporalLayer=%d,  testNumber=%d\n", TemporalLayer, testNumber);
                    fwrite(outputBuffer.pData0, 1, outputBuffer.nSize0, out_file);
                    //fwrite(outputBuffer.pData0+9, 1, outputBuffer.nSize0-9, out_file);
                    if(outputBuffer.nSize1)
                    {
                        fwrite(outputBuffer.pData1, 1, outputBuffer.nSize1, out_file);
                    }
                    break;
            }
#else

#if NO_READ_WRITE

#else
        #if SAVE_AWSP
            char mask[6] = "awsp";
            logd(" make special stream save data %d,%d ",outputBuffer.nSize0 , outputBuffer.nSize1);
            fwrite(mask, 4, sizeof(char), out_file);
            value =0;
            fwrite(&value, 1, sizeof(int), out_file);
            value = outputBuffer.nSize0 + outputBuffer.nSize1;
            fwrite(&value, 1, sizeof(int), out_file);
           int64_t value = 0;
            fwrite(&value, 1, sizeof(int64_t), out_file);
        #endif

            if(encode_param.encode_format == VENC_CODEC_JPEG && 0 == encode_param.jpeg_func.jpeg_mode)
            {
                saveJpegPic(&outputBuffer, encode_param.output_file, testNumber);
            }
            else
            {
                fwrite(outputBuffer.pData0, 1, outputBuffer.nSize0, out_file);

                if(outputBuffer.nSize1)
                {
                   fwrite(outputBuffer.pData1, 1, outputBuffer.nSize1, out_file);
                }
            }

           if(pRoiConfig.bEnable)
            {
                   int idx;
                   int nLen = 0;
                   for(idx=0; idx<pRoiConfig.num; idx++)
                   {
                        FILE_STRUCT *Roi_file;
                        char name[128];
                        sprintf(name, "/mnt/test/Roi_%d_%d.yuv", testNumber, idx);
                        Roi_file = fopen(name, "wb");
                        if(Roi_file == NULL)
                        {
                             loge("open out_file fail\n");
                             goto out;
                        }

                        MEMOPS_STRUCT *_memops = baseConfig.memops;
                        void *veOps = (void *)baseConfig.veOpsS;
                        void *pVeopsSelf = baseConfig.pVeOpsSelf;
                        CEDARC_UNUSE(_memops);
                        CEDARC_UNUSE(veOps);
	                  CEDARC_UNUSE(pVeopsSelf);
                         int yLen = pRoiConfig.sRect[idx].nWidth*pRoiConfig.sRect[idx].nHeight;
                         EncAdapterMemFlushCache(pRoiConfig.pRoiYAddrVir +nLen, yLen);
	                   EncAdapterMemFlushCache(pRoiConfig.pRoiCAddrVir+nLen/2 , yLen/2);

	                   fwrite(pRoiConfig.pRoiYAddrVir+nLen, 1, yLen, Roi_file);
	                   fwrite(pRoiConfig.pRoiCAddrVir+nLen/2, 1, yLen/2, Roi_file);
                         fclose(Roi_file);
                         nLen += yLen;
                    }
          }

        #endif
#endif

            if(encode_param.compare_flag)
            {
                result = fread(reference_buffer, 1, outputBuffer.nSize0, reference_file);
                if(result != (int)outputBuffer.nSize0)
                {
                    loge("read reference_file error\n");
                    goto out;
                }

                for(i=0; i<(int)outputBuffer.nSize0; i++)
                {
                    if(outputBuffer.pData0[i] != reference_buffer[i])
                    {
                        loge("the %d frame's ref_data_%d[%02x] and cur_data_%d[%02x] is not same\n",
                                                testNumber,
                                                i,
                                                reference_buffer[i],
                                                i,
                                                outputBuffer.pData0[i]);
                        goto out;
                    }
                }

                result = fread(reference_buffer, 1, outputBuffer.nSize1, reference_file);
                if(result != (int)outputBuffer.nSize1)
                {
                    loge("read reference_file error\n");
                    goto out;
                }

                for(i=0; i<(int)outputBuffer.nSize1; i++)
                {
                    if((outputBuffer.pData1)[i] != reference_buffer[i])
                    {
                        loge("the %d frame's data1 %d byte is not same\n", testNumber, i);
                        goto out;
                    }
                }
            }

            VencQueueOutputBuf(pVideoEnc, &outputBuffer);
			set_end_time_and_compute(&stat_demo_time);
#if ENABLE_GET_WRITE_BACK_YUV

            if(testNumber < 10 || testNumber > 15)
            {
                saveWbYuv(pEncContext, &encode_param);
            }
            if(testNumber == 10)
            {
                sWbYuvParam mWbYuvParam;
                memset(&mWbYuvParam, 0, sizeof(sWbYuvParam));
                mWbYuvParam.bEnableWbYuv = 0;
                VencSetParameter(pVideoEnc, VENC_IndexParamEnableWbYuv, &mWbYuvParam);
                pEncContext->mWbYuvFuncInfo.yuvSize = 0;
                if(pEncContext->mWbYuvFuncInfo.yuvBuf)
                    FREE(pEncContext->mWbYuvFuncInfo.yuvBuf);
            }
            if(testNumber == 13)
            {
                initWbYuv(&pEncContext->mWbYuvFuncInfo, &encode_param, pVideoEnc);
            }
#endif

#if ENABLE_SET_REC_LBC_MODE_TEST

            if(testNumber == 15)
            {
                eVeLbcMode param = LBC_MODE_2_0X;
                VencSetParameter(pVideoEnc, VENC_IndexParamSetRecRefLbcMode, &param);
            }
            if(testNumber == 55)
            {
                eVeLbcMode param = LBC_MODE_1_5X;
                VencSetParameter(pVideoEnc, VENC_IndexParamSetRecRefLbcMode, &param);
            }

#endif
            testNumber++;
            if(testNumber % 20 == 0)
            {
                logd("had encoder num = %d", testNumber);
            }

            if(encode_param.bEnableSharp == 1)
            {
                unsigned int enableSharp = 0;
                #if 0
                if(testNumber == 40)
                {
                    enableSharp = 1;
                    VencSetParameter(pVideoEnc, VENC_IndexParamEnableEncppSharp, &enableSharp);
                }
                else if(testNumber == 80)
                {
                    enableSharp = 1;
                    VencSetParameter(pVideoEnc, VENC_IndexParamEnableEncppSharp, &enableSharp);
                }
                #else
                enableSharp = 1;
                VencSetParameter(pVideoEnc, VENC_IndexParamEnableEncppSharp, &enableSharp);
                #endif
            }
        }
		print_stat_info(&stat_demo_time, 1);
        if(encode_param.compare_flag)
        {
            encode_param.compare_result = 1;
            logd("the compare result is ok\n");
        }

        printf("output file is saved:%s\n",encode_param.output_file);

    out:

		AvTimerDestroy(pAvTimer);

        if(pRoiConfig.bEnable)
        {
             MEMOPS_STRUCT *_memops = baseConfig.memops;
             void *veOps = (void *)baseConfig.veOpsS;
             void *pVeopsSelf = baseConfig.pVeOpsSelf;
             CEDARC_UNUSE(_memops);
             CEDARC_UNUSE(veOps);
             CEDARC_UNUSE(pVeopsSelf);
             if(pRoiConfig.pRoiYAddrVir != NULL)
             {
                  EncAdapterMemPfree(pRoiConfig.pRoiYAddrVir);
                  pRoiConfig.pRoiYAddrVir = NULL;
             }

             if(pRoiConfig.pRoiCAddrVir != NULL)
             {
                  EncAdapterMemPfree(pRoiConfig.pRoiCAddrVir);
                  pRoiConfig.pRoiCAddrVir = NULL;
             }
        }

        if(pVideoEnc)
        {
            VencDestroy(pVideoEnc);
        }
        pVideoEnc = NULL;

        if(out_file)
            fclose(out_file);
        if(in_file)
            fclose(in_file);
        if(uv_tmp_buffer)
            FREE(uv_tmp_buffer);
        if(baseConfig.memops)
        {
            CDCMemClose(baseConfig.memops);
        }

        releaseMb(&encode_param);

#if ENABLE_GET_WRITE_BACK_YUV
        deInitWbYuv(&pEncContext->mWbYuvFuncInfo);
#endif

        if(encode_param.compare_flag)
        {
            if(reference_buffer)
                FREE(reference_buffer);
            if(reference_file)
                fclose(reference_file);

            printf("the %u cycle test, freq:%d\n", m, encode_param.frequency);

            if(encode_param.compare_result)
            {
                printf("encoder:ve_freq[%dMHz],the compare result is ok\n",encode_param.frequency);
                if(log_file)
                {
                    log_len = sprintf(log_buffer + log_len,             \
                        "encoder: ve_freq[%dMHz], the compare result is ok\n", \
                        encode_param.frequency);
                    fwrite(&logcat_buf, 1, log_len, log_file);
                    fclose(log_file);
                }
            }
            else
            {
                printf("encoder: ve_freq[%dMHz], the compare result is fail\n",\
                    encode_param.frequency);
                if(log_file)
                {
                    log_len = sprintf(log_buffer + log_len,             \
                        "encoder: ve_freq[%dMHz], the compare result is fail\n", \
                        encode_param.frequency);
                    fwrite(&logcat_buf, 1, log_len, log_file);
                    fclose(log_file);
                }
                goto DEMO_END;
            }
        }

    }
DEMO_END:
    for(i=0; i<13; i++)
    {
        if(bit_map_info[i].argb_addr)
            FREE(bit_map_info[i].argb_addr);
    }

    if(insert_data.pBuffer)
    {
        FREE(insert_data.pBuffer);
    }

    if(sei_para.pSeiData)
    {
        FREE(sei_para.pSeiData[0].pBuffer);
        FREE(sei_para.pSeiData[1].pBuffer);
        FREE(sei_para.pSeiData[2].pBuffer);
        FREE(sei_para.pSeiData);
    }

    return 0;
}

int main(int argc, char** argv)
{
    THREAD_STRUCT tchanelEncoder[ENCODER_MAX_NUM];
    encoder_Context* pEncContext[ENCODER_MAX_NUM] = {NULL};
    int nRet = 0;
    int i = 0;
    int nEncoderNum = 1;
    int bOnline_mode = 0;

    //* get encoder num
    if(argc >= 2)
    {
        for(i = 1; i < argc; i += 2)
        {
            ARGUMENT_T arg;
            arg = GetArgument(argv[i]);
            switch(arg)
            {
                case HELP:
                    PrintDemoUsage();
                    exit(-1);
                case ENCODER_NUM:
                    sscanf(argv[i + 1], "%d", &nEncoderNum);
                    break;
                case ONLINE_MODE:
                    sscanf(argv[i + 1], "%d", &bOnline_mode);
                    break;
                default:
                    break;
            }
        }
    }
    else
    {
        logd(" we need more arguments ");
        PrintDemoUsage();
        return 0;
    }

    if(nEncoderNum <= 0)
        nEncoderNum = 1;
    else if(nEncoderNum > ENCODER_MAX_NUM)
        nEncoderNum = ENCODER_MAX_NUM;

    logd("nEncoderNum = %d, bOnline_mode = %d",nEncoderNum, bOnline_mode);

    for(i = 0; i < nEncoderNum; i++)
    {
        pEncContext[i] = CALLOC(sizeof(encoder_Context), 1);
        if(pEncContext[i] == NULL)
        {
            loge("malloc for pEncContext failed");
            goto OUT;
        }
        pEncContext[i]->argc = argc;
        pEncContext[i]->argv = argv;
        pEncContext[i]->nChannel = i;
        pEncContext[i]->nTotalChannelNum = nEncoderNum;
    }

    for(i = 0; i < nEncoderNum; i++)
    {
        pEncContext[i]->bOnlineMode    = 1;
        pEncContext[i]->bOnlineChannel = bOnline_mode;

        pthread_create(&tchanelEncoder[i], NULL, ChannelThread, (void*)(pEncContext[i]));
    }

    for(i = 0; i < nEncoderNum; i++)
    {
        pthread_join(tchanelEncoder[i], (void**)&nRet);
    }

OUT:

    for(i = 0; i < nEncoderNum; i++)
    {
        if(pEncContext[i])
            FREE(pEncContext[i]);
    }

    return 0;
}

