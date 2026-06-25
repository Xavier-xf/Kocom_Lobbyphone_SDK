//#define LOG_NDEBUG 0
#define LOG_TAG "sample_region"
#include <utils/plat_log.h>

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include "media/mpi_venc.h"
#include "media/mpi_sys.h"
#include "mm_common.h"
#include "mm_comm_venc.h"
#include "mm_comm_rc.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"

#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <confparser.h>
#include "sample_isposd_config.h"
#include "sample_isposd.h"
#include <BITMAP_S.h>


#define HAVE_H264

static SampleIspOsdContext *gpSampleIspOsdContext = NULL;

int initSampleIspOsdContext(SampleIspOsdContext *pContext)
{
    memset(pContext, 0, sizeof(SampleIspOsdContext));
    cdx_sem_init(&pContext->mSemExit, 0);
    return 0;
}

int destroySampleIspOsdContext(SampleIspOsdContext *pContext)
{
    cdx_sem_deinit(&pContext->mSemExit);
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleIspOsdCmdLineParam *pCmdLinePara)
{
    //alogd("sample_region input path is : %s", argv[0]);
    int ret = 0;
    int i = 1;
    memset(pCmdLinePara, 0, sizeof(SampleIspOsdCmdLineParam));
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if((++i) >= argc)
            {
                aloge("fatal error!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error!");
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], strlen(argv[i]));
            pCmdLinePara->mConfigFilePath[strlen(argv[i])] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
             printf("CmdLine param example:\n"
                "\t run -path /home/sample_vi2vo.conf\n");
             ret = 1;
             break;
        }
        else
        {
             printf("CmdLine param example:\n"                "\t run -path /home/sample_vi2vo.conf\n");
        }
        ++i;
    }
    return ret;
}

static ERRORTYPE loadSampleIspOsdConfig(SampleIspOsdConfig *pConfig, const char *conf_path)
{
    int ret;

    if(NULL == conf_path)
    {
        aloge("fatal error! user not set config file!");
        return -1;
    }

    CONFPARSER_S stConfParser;
    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }

    memset(pConfig, 0, sizeof(SampleIspOsdConfig));

    pConfig->mCaptureWidth = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_CAPTURE_WIDTH, 1920);
    pConfig->mCaptureHeight = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_CAPTURE_HEIGHT, 1080);

    char *pStrPixelFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_REGION_KEY_PIC_FORMAT, NULL);
    if (pStrPixelFormat)
    {
        if(!strcmp(pStrPixelFormat, "yu12"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
        }
        else if(!strcmp(pStrPixelFormat, "yv12"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
        }
        else if(!strcmp(pStrPixelFormat, "nv21"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if(!strcmp(pStrPixelFormat, "nv12"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if (!strcmp(pStrPixelFormat, "aw_lbc_2_5x"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        }
        else if (!strcmp(pStrPixelFormat, "aw_lbc_2_0x"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
        }
        else if (!strcmp(pStrPixelFormat, "aw_lbc_1_5x"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
        }
        else if (!strcmp(pStrPixelFormat, "aw_lbc_1_0x"))
        {
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
        }
        else
        {
            aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
            pConfig->mPicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
    }
    alogd("StrPixelFormat:%s, PicFormat:%d", pStrPixelFormat, pConfig->mPicFormat);

    pConfig->mFrameRate = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_FRAME_RATE, 30);
    pConfig->mBitrate = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_BIT_RATE, 0);
    char *EncoderType = (char*)GetConfParaString(&stConfParser, SAMPLE_REGION_KEY_ENCODERTYPE, NULL);
    if (EncoderType)
    {
        if(!strcmp(EncoderType, "H.264"))
        {
            pConfig->EncoderType = PT_H264;
        }
        else if(!strcmp(EncoderType, "H.265"))
        {
            pConfig->EncoderType = PT_H265;
        }
        else if(!strcmp(EncoderType, "MJPEG"))
        {
            pConfig->EncoderType = PT_MJPEG;
        }
        else
        {
            alogw("unsupported venc type:%p,encoder type turn to H.264!",EncoderType);
            pConfig->EncoderType = PT_H264;
        }
    }

    char *strFormat = (char*)GetConfParaString(&stConfParser, SAMPLE_REGION_KEY_BITMAP_FORMAT, NULL);
    if (strFormat)
    {
        if(!strcmp(strFormat, "ARGB1555"))
        {
            pConfig->mBitmapFormat = MM_PIXEL_FORMAT_RGB_1555;
        }
        else
        {
            pConfig->mBitmapFormat = MM_PIXEL_FORMAT_RGB_8888;
        }
    }

    pConfig->overlay_x = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_OVERLAY_X, 0);
    pConfig->overlay_y = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_OVERLAY_Y, 0);

    pConfig->InvColEn = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_INVCOL_EN, 0);
    pConfig->InvColMode = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_INVCOL_MODE, 0);
    pConfig->InvColLumThresh = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_INVCOL_LUMTHRESH, 0);
    alogd("InvCol En:%d, Mode:%d, LumThresh:%d", pConfig->InvColEn, pConfig->InvColMode, pConfig->InvColLumThresh);

    char *pStr = (char *)GetConfParaString(&stConfParser, SAMPLE_REGION_KEY_OUTPUT_FILE_PATH, NULL);
    if (pStr)
    {
        strncpy(pConfig->OutputFilePath, pStr, MAX_FILE_PATH_SIZE-1);
        pConfig->OutputFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
    }

    pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_REGION_KEY_TEST_DURATION, 0);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(gpSampleIspOsdContext != NULL)
    {
        cdx_sem_up(&gpSampleIspOsdContext->mSemExit);
    }
}

static int Isp_DebugFunc(int IspDevId)
{
#if 0
    static int time_s_cnt = 0;
    static int on_off = 0;

    if(++time_s_cnt >= 4)
    {
        time_s_cnt = 0;

        // test SwitchIspConfig
//        AW_MPI_ISP_SwitchIspConfig(IspDevId, on_off);

        // test Flip
//        AW_MPI_ISP_SetMirror(IspDevId, on_off);
//        AW_MPI_ISP_SetFlip(IspDevId, on_off);
/*
        // test  Exposure
        int exp_mode = 0, exp_value = 0;
        int exp_temp[4] = {2048, 4096, 8192, 16384};
        AW_MPI_ISP_AE_GetMode(IspDevId, &exp_mode);
        alogd("1. exp_mode = %d\n", exp_mode);
        AW_MPI_ISP_AE_SetMode(IspDevId, 1);
        AW_MPI_ISP_AE_GetExposure(IspDevId, &exp_value);
        alogd("1. exp_value = %d\n", exp_value);
        AW_MPI_ISP_AE_SetExposure(IspDevId, exp_temp[on_off]);
*/
        if(++on_off > 3)
            on_off = 0;
    }
#endif

    return 0;
}

static void *Isp_DebugThread(void)
{
    SampleIspOsdContext *pContext = gpSampleIspOsdContext;
    if (!pContext)
    {
        aloge("fatal error! pContext is NULL!");
        return NULL;
    }

    while(FALSE == pContext->mbEncThreadExitFlag)
    {
        Isp_DebugFunc(pContext->mVIDev);
        sleep(1); //1s
    }

    return NULL;
}

static int CreateIspDebugRgb(RGB_PIC_S *ispRgb, int IspDevId)
{
	int exp_line=-1, gain=-1, lv_idx=-1, color_temp=-1;
	int lv_pos[3] = {-1, -1, -1};
	char isp_debug[128];
	double  gain_multiple = -1;

    //   get ISP_Paramer
    AW_MPI_ISP_AE_GetExposureLine(IspDevId, &exp_line);
    AW_MPI_ISP_AE_GetGain(IspDevId, &gain);
    AW_MPI_ISP_AWB_GetCurColorT(IspDevId, &color_temp);
    AW_MPI_ISP_AE_GetEvIdx(IspDevId, &lv_idx);

	lv_pos[1] = lv_idx/25;
	if(lv_pos[1] <= 0)
		lv_pos[0] = 0;
	else
		lv_pos[0] = lv_pos[1] - 1;
	if(lv_pos[1] >= 13)
		lv_pos[2] = 13;
	else
		lv_pos[2] = lv_pos[1] + 1;
	gain_multiple = (double)gain/16;	// 1 2 4 8 16 32 64

	snprintf(isp_debug, sizeof(isp_debug)-1, "exp_line=%d gain=%d(multiple:%.1f) lv=%d(lv_pos:%d, %d, %d) colorT=%d",
                      exp_line, gain, gain_multiple, lv_idx, lv_pos[0], lv_pos[1], lv_pos[2], color_temp);

    FONT_RGBPIC_S font_pic;
    font_pic.font_type     = FONT_SIZE_32;
    font_pic.rgb_type      = OSD_RGB_32;
    font_pic.enable_bg     = 0;
    font_pic.foreground[0] = 0xFF;
    font_pic.foreground[1] = 0xFF;
    font_pic.foreground[2] = 0xFF;
    font_pic.foreground[3] = 0xFF;
    font_pic.background[0] = 0x0;
    font_pic.background[1] = 0x0;
    font_pic.background[2] = 0x0;
    font_pic.background[3] = 0x0;
    ispRgb->enable_mosaic = 0;
    ispRgb->rgb_type      = OSD_RGB_32;
    create_font_rectangle(isp_debug, &font_pic, ispRgb);

    return 0;
}

static INVERT_COLOR_MODE_E converToInvertColorMode(int mode)
{
    INVERT_COLOR_MODE_E InvertColorMode = INVERT_COLOR_BUTT;
    switch(mode)
    {
        case 0:
            InvertColorMode = LESSTHAN_LUM_THRESH;
            break;
        case 1:
            InvertColorMode = MORETHAN_LUM_THRESH;
            break;
        case 2:
            InvertColorMode = LESSTHAN_LUMDIFF_THRESH;
            break;
        case 3:
            InvertColorMode = LESSTHAN_UNIT_LUMDIFF_THRESH;
            break;
        default:
            aloge("fatal error! unknown invert color mode[0x%x]", mode);
            break;
    }
    return InvertColorMode;
}

static void UpdateIspOSD(SampleIspOsdContext *pContext)
{
    if (!pContext)
    {
        aloge("fatal error! pContext is NULL!");
        return;
    }

    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVEChn};

    AW_MPI_RGN_DetachFromChn(pContext->mOverlayHandle, &VeChn);
    AW_MPI_RGN_Destroy(pContext->mOverlayHandle);

    RGN_ATTR_S stRegion;
    memset(&stRegion, 0, sizeof(RGN_ATTR_S));
    stRegion.enType = OVERLAY_RGN;
    stRegion.unAttr.stOverlay.mPixelFmt = MM_PIXEL_FORMAT_RGB_8888;
    stRegion.unAttr.stOverlay.mSize.Width = AWALIGN(pContext->ispDebugRgb.wide, 16);
    stRegion.unAttr.stOverlay.mSize.Height = AWALIGN(pContext->ispDebugRgb.high, 16);
    AW_MPI_RGN_Create(pContext->mOverlayHandle, &stRegion);

    RGN_CHN_ATTR_S stRgnChnAttr;
    memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    stRgnChnAttr.bShow = TRUE;
    stRgnChnAttr.enType = stRegion.enType;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X = AWALIGN(pContext->mConfigPara.overlay_x, 16);
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = AWALIGN(pContext->mConfigPara.overlay_y, 16);
    stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = 0;
    stRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha = 0x5C; // global alpha mode value for ARGB1555
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = pContext->mConfigPara.InvColLumThresh;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = converToInvertColorMode(pContext->mConfigPara.InvColMode);
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = pContext->mConfigPara.InvColEn;
    alogd("InvCol En:%d, Mode:%d, LumThresh:%d", pContext->mConfigPara.InvColEn, pContext->mConfigPara.InvColMode, pContext->mConfigPara.InvColLumThresh);
    AW_MPI_RGN_AttachToChn(pContext->mOverlayHandle, &VeChn, &stRgnChnAttr);
}

static void *IspDebugRbgbUpdateThread(void* argv)
{
    BITMAP_S stBmp;
    int *fps = (int *)argv;
    SampleIspOsdContext *pContext = gpSampleIspOsdContext;
    if (!pContext)
    {
        aloge("fatal error! pContext is NULL!");
        return NULL;
    }
    int fps_ms = 1000/(*fps);
    //alogd("fps_ms: %d ms", fps_ms);
    unsigned int curTimeBmpSize = 0;
    unsigned int lastTimeBmpSize = 0;
    while(FALSE == pContext->mbEncThreadExitFlag)
    {
        CreateIspDebugRgb(&pContext->ispDebugRgb, pContext->mVIDev);
        curTimeBmpSize = pContext->ispDebugRgb.wide * pContext->ispDebugRgb.high * 4;
        if (lastTimeBmpSize != curTimeBmpSize)
        {
            UpdateIspOSD(pContext);
        }
        memset(&stBmp, 0, sizeof(BITMAP_S));
        stBmp.mPixelFormat = MM_PIXEL_FORMAT_RGB_8888;
        stBmp.mWidth  = pContext->ispDebugRgb.wide;
        stBmp.mHeight = pContext->ispDebugRgb.high;
        stBmp.mpData  = pContext->ispDebugRgb.pic_addr;        
        AW_MPI_RGN_SetBitMap(pContext->mOverlayHandle, &stBmp);
        release_rgb_picture(&pContext->ispDebugRgb);
        lastTimeBmpSize = curTimeBmpSize;
        usleep(fps_ms);//ms
    }

    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVEChn};
    AW_MPI_RGN_DetachFromChn(pContext->mOverlayHandle, &VeChn);
    AW_MPI_RGN_Destroy(pContext->mOverlayHandle);
    pContext->mOverlayHandle = MM_INVALID_CHN;

    return NULL;
}

static int IspDebugRgbInit(SampleIspOsdContext *pContext)
{
    int ret;

    ret = load_font_file(FONT_SIZE_32);
    if(ret < 0)
    {
        aloge("load_font_file FONT_SIZE_32 fail! ret:%d\n", ret);
        return -1;
    }

    ret = load_font_file(FONT_SIZE_64);
    if (ret < 0)
    {
        aloge("Do load_font_file FONT_SIZE_64 fail! ret:%d\n", ret);
        return -1;
    }

    CreateIspDebugRgb(&pContext->ispDebugRgb, pContext->mVIDev);

    ret = pthread_create(&pContext->ispDebugThreadId, NULL, Isp_DebugThread, NULL);
    if(ret <0)
    {
        aloge("error: the IspDebugThreadCreate can not be created");
        return -1;
    }else
        alogd("the IspDebugThreadCreate create ok");

    ret = pthread_create(&pContext->ispDebugRgbThreadId, NULL, IspDebugRbgbUpdateThread, (void*)&pContext->mConfigPara.mFrameRate);
    if(ret <0)
    {
        aloge("error: the IspDebugRgbUpdateThread can not be created");
        return -1;
    }else
        alogd("the IspDebugRgbUpdateThread create ok");

    return 0;
}

static int IspDebugRgbDeInit(SampleIspOsdContext *pContext)
{
    int ret;

    pContext->mbEncThreadExitFlag = TRUE;
    int eError = 0;
    pthread_join(pContext->ispDebugRgbThreadId, (void*)&eError);
    pthread_join(pContext->ispDebugThreadId, (void*)&eError);

    return 0;
}

static void *GetEncoderFrameThread(void * pArg)
{
    SampleIspOsdContext *pContext = (SampleIspOsdContext*)pArg;
    int count = 0;
    int eRet = -1;

    VencHeaderData vencheader;
    pContext->mOutputFileFp = fopen(pContext->mConfigPara.OutputFilePath,"wb+");
    if(!pContext->mOutputFileFp)
    {
        aloge("fatal error! can not open the file");
        return NULL;
    }
    alogd("open %s success", pContext->mConfigPara.OutputFilePath);
    
    if(PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
    {
        eRet = AW_MPI_VENC_GetH264SpsPpsInfo(pContext->mVEChn, &vencheader);
        if (SUCCESS == eRet)
        {
            if(vencheader.nLength)
            {
                fwrite(vencheader.pBuffer, vencheader.nLength, 1, pContext->mOutputFileFp);
            }
        }
        else
        {
            aloge("fatal error! AW_MPI_VENC_GetH264SpsPpsInfo failed!\n");
        }
    }
    else if(PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
    {
        eRet = AW_MPI_VENC_GetH265SpsPpsInfo(pContext->mVEChn, &vencheader);
        if (SUCCESS == eRet)
        {
            if(vencheader.nLength)
            {
                fwrite(vencheader.pBuffer, vencheader.nLength, 1, pContext->mOutputFileFp);
            }
        }
        else
        {
            aloge("fatal error! AW_MPI_VENC_GetH265SpsPpsInfo failed!\n");
        }
    }
    
    VENC_STREAM_S VencFrame;
    VENC_PACK_S venc_pack;
    VencFrame.mPackCount = 1;
    VencFrame.mpPack = &venc_pack;

    //save the video
    while(FALSE == pContext->mbEncThreadExitFlag)
    {
        if(AW_MPI_VENC_GetStream(pContext->mVEChn, &VencFrame, 4000) < 0)
        {
            aloge("get first frame failed!\n");
            continue;
        }
        else
        {
            if(VencFrame.mpPack != NULL && VencFrame.mpPack->mLen0)
            {
                fwrite(VencFrame.mpPack->mpAddr0, 1, VencFrame.mpPack->mLen0, pContext->mOutputFileFp);
            }
            if(VencFrame.mpPack != NULL && VencFrame.mpPack->mLen1)
            {
                fwrite(VencFrame.mpPack->mpAddr1, 1, VencFrame.mpPack->mLen1, pContext->mOutputFileFp);
            }

            AW_MPI_VENC_ReleaseStream(pContext->mVEChn, &VencFrame);
            count++;
        }
    }
    alogd("get [%d]encoded frames.", count);

    if (pContext->mOutputFileFp)
    {
        fclose(pContext->mOutputFileFp);
        pContext->mOutputFileFp = NULL;
    }

    return NULL;
}

int main(int argc, char **argv)
{
    int result = 0;
    ERRORTYPE eRet = SUCCESS;

    SampleIspOsdContext *pContext = (SampleIspOsdContext*)malloc(sizeof(SampleIspOsdContext));
    if(NULL == pContext)
    {
        alogd("malloc fail!");
        return -1;
    }
    initSampleIspOsdContext(pContext);
    gpSampleIspOsdContext = pContext;

    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath = NULL;
    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    if(loadSampleIspOsdConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }

    pContext->mISPDev = 0;
    pContext->mVIDev = 0;
    pContext->mVIChn = 0;
    pContext->mVEChn = 0;
    pContext->mOverlayHandle = 0;

    memset(&pContext->mSysConf, 0, sizeof(MPP_SYS_CONF_S));
    pContext->mSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&pContext->mSysConf);
    AW_MPI_SYS_Init();

    eRet = AW_MPI_VI_CreateVipp(pContext->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_VI_CreateVipp failed");
    }

    VI_ATTR_S attr;
    memset(&attr, 0, sizeof(VI_ATTR_S));
    attr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    attr.memtype = V4L2_MEMORY_MMAP;
    attr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pContext->mConfigPara.mPicFormat);
    attr.format.field = V4L2_FIELD_NONE;
    attr.format.colorspace = V4L2_COLORSPACE_REC709_PART_RANGE;
    attr.format.width = pContext->mConfigPara.mCaptureWidth;
    attr.format.height = pContext->mConfigPara.mCaptureHeight;
    attr.nbufs = 5;
    attr.nplanes = 2;
    attr.fps = pContext->mConfigPara.mFrameRate;
    attr.wdr_mode = 0;
    attr.mbEncppEnable = TRUE;    
    eRet = AW_MPI_VI_SetVippAttr(pContext->mVIDev, &attr);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_SetVippAttr failed");
    }

    AW_MPI_ISP_Run(pContext->mISPDev);

    eRet = AW_MPI_VI_EnableVipp(pContext->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_VI_EnableVipp failed");
    }

    eRet = AW_MPI_VI_CreateVirChn(pContext->mVIDev, pContext->mVIChn, NULL);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_VI_CreateVirChn failed ");
    }

    memset(&pContext->mVencChnAttr, 0, sizeof(VENC_CHN_ATTR_S));
    pContext->mVencChnAttr.VeAttr.Type = pContext->mConfigPara.EncoderType;
    pContext->mVencChnAttr.VeAttr.MaxKeyInterval = 100;
    pContext->mVencChnAttr.VeAttr.SrcPicWidth  = pContext->mConfigPara.mCaptureWidth;
    pContext->mVencChnAttr.VeAttr.SrcPicHeight = pContext->mConfigPara.mCaptureHeight;
    pContext->mVencChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pContext->mVencChnAttr.VeAttr.PixelFormat = pContext->mConfigPara.mPicFormat;
    pContext->mVencChnAttr.VeAttr.mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
    pContext->mVencChnAttr.EncppAttr.eEncppSharpSetting = VencEncppSharp_FollowISPConfig;
    pContext->mVencChnAttr.RcAttr.mProductMode = PRODUCT_STATIC_IPC;
    //pContext->mVencRcParam.sensor_type = VENC_ST_EN_WDR;
    int vbvBufSize = 0;
    int vbvThreshSize = 0;
    if (pContext->mConfigPara.mFrameRate)
    {
        vbvThreshSize = pContext->mConfigPara.mBitrate/8/pContext->mConfigPara.mFrameRate*15;
    }
    vbvBufSize = pContext->mConfigPara.mBitrate/8*4 + vbvThreshSize;
    alogd("vbvBufSize: %d, vbvThreshSize: %d", vbvBufSize, vbvThreshSize);

    if (PT_H264 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH264e.BufSize = vbvBufSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mThreshSize = vbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH264e.bByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH264e.Profile = 2;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicWidth  = pContext->mConfigPara.mCaptureWidth;
        pContext->mVencChnAttr.VeAttr.AttrH264e.PicHeight = pContext->mConfigPara.mCaptureHeight;
        pContext->mVencChnAttr.VeAttr.AttrH264e.mbPIntraEnable = TRUE;
        pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
        pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = pContext->mConfigPara.mBitrate;
        pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mSrcFrmRate = pContext->mConfigPara.mFrameRate;
        pContext->mVencChnAttr.RcAttr.mAttrH264Vbr.mDstFrmRate = pContext->mConfigPara.mFrameRate;
        pContext->mVencRcParam.ParamH264Vbr.mMinQp = 10;
        pContext->mVencRcParam.ParamH264Vbr.mMaxQp = 50;
        pContext->mVencRcParam.ParamH264Vbr.mMaxPqp = 50;
        pContext->mVencRcParam.ParamH264Vbr.mMinPqp = 10;
        pContext->mVencRcParam.ParamH264Vbr.mQpInit = 35;
        pContext->mVencRcParam.ParamH264Vbr.mbEnMbQpLimit = 0;
        pContext->mVencRcParam.ParamH264Vbr.mMovingTh = 20;
        pContext->mVencRcParam.ParamH264Vbr.mQuality = 10;
        pContext->mVencRcParam.ParamH264Vbr.mIFrmBitsCoef = 10;
        pContext->mVencRcParam.ParamH264Vbr.mPFrmBitsCoef = 10;
    }
    else if (PT_H265 == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrH265e.mBufSize = vbvBufSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mThreshSize = vbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mProfile = 0;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mLevel = 0; /* set the default value 0 and encoder will adjust automatically. */
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicWidth = pContext->mConfigPara.mCaptureWidth;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mPicHeight = pContext->mConfigPara.mCaptureHeight;
        pContext->mVencChnAttr.VeAttr.AttrH265e.mbPIntraEnable = TRUE;
        pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
        pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = pContext->mConfigPara.mBitrate;
        pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mSrcFrmRate = pContext->mConfigPara.mFrameRate;
        pContext->mVencChnAttr.RcAttr.mAttrH265Vbr.mDstFrmRate = pContext->mConfigPara.mFrameRate;
        pContext->mVencRcParam.ParamH265Vbr.mMinQp = 10;
        pContext->mVencRcParam.ParamH265Vbr.mMaxQp = 50;
        pContext->mVencRcParam.ParamH265Vbr.mMaxPqp = 50;
        pContext->mVencRcParam.ParamH265Vbr.mMinPqp = 10;
        pContext->mVencRcParam.ParamH265Vbr.mQpInit = 35;
        pContext->mVencRcParam.ParamH265Vbr.mbEnMbQpLimit = 0;
        pContext->mVencRcParam.ParamH265Vbr.mMovingTh = 20;
        pContext->mVencRcParam.ParamH265Vbr.mQuality = 10;
        pContext->mVencRcParam.ParamH265Vbr.mIFrmBitsCoef = 10;
        pContext->mVencRcParam.ParamH265Vbr.mPFrmBitsCoef = 10;
    }
    else if (PT_MJPEG == pContext->mVencChnAttr.VeAttr.Type)
    {
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mBufSize = vbvBufSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mThreshSize = vbvThreshSize;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mbByFrame = TRUE;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicWidth = pContext->mConfigPara.mCaptureWidth;
        pContext->mVencChnAttr.VeAttr.AttrMjpeg.mPicHeight = pContext->mConfigPara.mCaptureHeight;
        pContext->mVencChnAttr.RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
        pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = pContext->mConfigPara.mBitrate;
        pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mSrcFrmRate = pContext->mConfigPara.mFrameRate;
        pContext->mVencChnAttr.RcAttr.mAttrMjpegeCbr.mDstFrmRate = pContext->mConfigPara.mFrameRate;
    }

    alogd("venc set Type:%d, Rcmode:%d", pContext->mVencChnAttr.VeAttr.Type, pContext->mVencChnAttr.RcAttr.mRcMode);

    eRet =  AW_MPI_VENC_CreateChn(pContext->mVEChn, &pContext->mVencChnAttr);
    if(eRet != SUCCESS)
    {
        aloge("error:AW_MPI_VENC_CreateChn failed");
    }

    AW_MPI_VENC_SetRcParam(pContext->mVEChn, &pContext->mVencRcParam);

    /*VENC_FRAME_RATE_S stFrameRate;
    stFrameRate.SrcFrmRate = pContext->mConfigPara.mFrameRate;
    stFrameRate.DstFrmRate = pContext->mConfigPara.mFrameRate;
    AW_MPI_VENC_SetFrameRate(pContext->mVEChn, &stFrameRate);*/

    if(pthread_create(&pContext->mEncThreadId, NULL, GetEncoderFrameThread, pContext) < 0)
    {
        aloge("error: the pthread can not be created");
    }
    alogw("the pthread[0x%x] of venc had created", pContext->mEncThreadId);

    MPP_CHN_S ViChn = {MOD_ID_VIU, pContext->mVIDev, pContext->mVIChn};
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pContext->mVEChn};
    AW_MPI_SYS_Bind(&ViChn,&VeChn);
    alogd("bind vipp%d virch%d and ve ch%d", pContext->mVIDev, pContext->mVIChn, pContext->mVEChn);

    AW_MPI_VI_EnableVirChn(pContext->mVIDev, pContext->mVIChn);
    AW_MPI_VENC_StartRecvPic(pContext->mVEChn);

    IspDebugRgbInit(pContext);

    if(pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration * 1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

    pContext->mbEncThreadExitFlag = TRUE;
    int eError = 0;
    pthread_join(pContext->mEncThreadId, (void*)&eError);

    IspDebugRgbDeInit(pContext);

    AW_MPI_VENC_StopRecvPic(pContext->mVEChn);
 	AW_MPI_VI_DisableVirChn(pContext->mVIDev, pContext->mVIChn);
    //AW_MPI_VENC_ResetChn(pContext->mVEChn);
    AW_MPI_VENC_DestroyChn(pContext->mVEChn);
    AW_MPI_VI_DestroyVirChn(pContext->mVIDev, pContext->mVIChn);
    AW_MPI_VI_DisableVipp(pContext->mVIDev);
    AW_MPI_VI_DestroyVipp(pContext->mVIDev);
    pContext->mVIDev = MM_INVALID_DEV;
    pContext->mVIChn = MM_INVALID_CHN;
    pContext->mVEChn = MM_INVALID_CHN;

	AW_MPI_ISP_Stop(pContext->mISPDev);

    //exit mpp system
    AW_MPI_SYS_Exit();

_exit:
    destroySampleIspOsdContext(pContext);
    if (pContext)
    {
        free(pContext);
        pContext = NULL;
    }

    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}

