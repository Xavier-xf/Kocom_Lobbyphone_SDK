#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <utils/plat_log.h>
#include <VIDEO_FRAME_INFO_S.h>

#include "aw_person_detection.h"
#include "confparser.h"
#include "rgb_ctrl.h"
#include "mm_comm_region.h"
#include "MppHelper.h"
#include "sample_IntrusionMonitor.h"

#define NETWORK_BINARY          "./3.0.8"
#define VI_NPU_PIC_WIDTH        384
#define VI_NPU_PIC_HEIGHT       224
#define NPU_DET_PIC_WIDTH       384
#define NPU_DET_PIC_HEIGHT      224
#define DET_THRESH              0.6
#define VIPP_INPUT_FPS          30

#define SRC_WIDTH               1280
#define SRC_HEIGHT              720
#define SRC_FRAME_RATE          30
#define DST_WIDTH               1280
#define DST_HEIGHT              720
#define DST_FRAME_RATE          30
#define DST_ENCODER_TYPE        PT_H264
//#define DST_BIT_RATE 2000000
#define DST_BIT_RATE            8000000

#define SAMPLE_DETECT_MAX_NUM 10
#define MPP_REGION_RECT_NUM  SAMPLE_DETECT_MAX_NUM

pthread_t gStreamProcessThread;
pthread_t gDetectThread;
pthread_t gUvcOutThread;

SampleIntrusionMonitorContext gSampleIntrusionMonitorContext;

volatile int gStop;
volatile int gKeepRunning = 1;
void sampleIntrusionMonitorRecvSignal(int sig) {
    alogd("Catch ctrl+c signal and exit...\n");
    gKeepRunning = 0;
}

#define DISPLAY_WIDTH 1280
#define DISPLAY_HEIGHT 720

static int nv12_draw_point(unsigned char* yBuffer, unsigned char* uvBuffer, int width,
        int height, int x, int y, int yColor, int uColor, int vColor)
{
    if (x < 0 || x >= width) return -1;
    if (y < 0 || y >= height) return -1;
    yBuffer[y*width+x] = yColor;
    uvBuffer[(y/2)*width+x/2*2] = uColor;
    uvBuffer[(y/2)*width+x/2*2+1] = vColor;
    return 0;
}

// NV12 draw red line
static int nv12_draw_line(unsigned char* yBuffer, unsigned char* uvBuffer, int width,
        int height, int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1, sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2;

    while (nv12_draw_point(yBuffer, uvBuffer, width, height, x0, y0, 0x4C, 0x55, 0xFF), x0 != x1 || y0 != y1) {
        int e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
    return 0;
}

static int createMonitorRegion(void *region, int width, int height, unsigned char* yBuffer,
        unsigned char* uBuffer, unsigned char* vBuffer)
{
    MonitorRegion *pRegion = region;
    unsigned char* uvBuffer = uBuffer;

    if (pRegion->mode == CROSS_WORK_MODE_LINE) {
        nv12_draw_line(yBuffer, uvBuffer, width, height,
                pRegion->settings.lines[0].x0,
                pRegion->settings.lines[0].y0,
                pRegion->settings.lines[0].x1,
                pRegion->settings.lines[0].y1
                );
    } else if (pRegion->mode == CROSS_WORK_MODE_RECT){
        for (int j = 0; j < 4; j++)
            nv12_draw_line(yBuffer, uvBuffer, width, height,
                    pRegion->settings.rects[0].x[j],
                    pRegion->settings.rects[0].y[j],
                    pRegion->settings.rects[0].x[(j + 1) % 4],
                    pRegion->settings.rects[0].y[(j + 1) % 4]
                    );
    } else {
        aloge("Fatal error! Failed to create monitor region.\n");
    }
    return 0;
}

static int nv12_draw_rect(unsigned char* yBuffer, unsigned char* uvBuffer,int width, int height,
        int left, int top, int right, int bottom, int yColor, int uColor, int vColor)
{
    int i;
    for (i = left; i <= right; i++) {
        nv12_draw_point(yBuffer, uvBuffer, width, height, i, top, yColor, uColor, vColor);
        nv12_draw_point(yBuffer, uvBuffer, width, height, i, bottom, yColor, uColor, vColor);
    }
    for (i = top; i <= bottom; i++) {
        nv12_draw_point(yBuffer, uvBuffer, width, height, left, i, yColor, uColor, vColor);
        nv12_draw_point(yBuffer, uvBuffer, width, height, right, i, yColor, uColor, vColor);
    }
    return 0;
}

static int createMonitorTargetRect(AW_PDet_Output *resultNorm, int width, int height,
        unsigned char* yBuffer,unsigned char* uBuffer, unsigned char* vBuffer)
{
    int j = 0;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    AW_PDet_Output *result = resultNorm;
    unsigned char* uvBuffer = uBuffer;

    for (j = 0; j < result->num; j++) {

        x0 = result->person[j].bbox.tl_x;
        y0 = result->person[j].bbox.tl_y;
        x1 = result->person[j].bbox.br_x;
        y1 = result->person[j].bbox.br_y;
        nv12_draw_rect(yBuffer, uvBuffer, width, height, x0, y0, x1, y1, 0x96, 0x2C, 0x15);  //green

    }
    return 0;
}

static int npuDetectCallback(unsigned char **yuvBuffer,AW_PDet_Container *detContainer, AW_PDet_Output *detOutputs)
{
    AW_PDet_Container *container = detContainer;
    AW_PDet_Output *outputs = detOutputs;

    aw_run_person_detection(container, yuvBuffer, outputs);

    for (int j = 0; j < outputs->num; j++)
    {
        if (outputs->person[j].label == 1) {
            printf("Detect person id %d: cls %d, prob %f, rect [%f, %f, %f, %f]\n", j,
                    outputs->person[j].label,
                    outputs->person[j].score,
                    outputs->person[j].bbox.tl_x,
                    outputs->person[j].bbox.tl_y,
                    outputs->person[j].bbox.br_x,
                    outputs->person[j].bbox.br_y);
        }
    }

    return 0;
}

FILE *testFp = NULL;

RGN_ATTR_S stRegion[MPP_REGION_RECT_NUM];
RGN_CHN_ATTR_S stRgnChnAttr[MPP_REGION_RECT_NUM];

static int regionCreateRect(AW_PDet_Output *detOutputs, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, int num)
{
    int x, y, w, h;
    int j = 0;

    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;

    StreamProcessPrivateData *pData = &(gSampleIntrusionMonitorContext.privateData.streamProcessData);
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;
    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    AW_PDet_Output *outputs = detOutputs;

    if (!outputs->num)
        return 0;
    if (outputs->num > num)
        outputs->num = num;

    for (j = 0; j < outputs->num; j++) {
        x = outputs->person[j].bbox.tl_x; //* pConfig->venWidth;
        y = outputs->person[j].bbox.tl_y; //* pConfig->venHeight;
        w = outputs->person[j].bbox.br_x - outputs->person[j].bbox.tl_x;
        h = outputs->person[j].bbox.br_y - outputs->person[j].bbox.tl_y;
   //     printf("draw rect: x:%d, y:%d, w:%d, h:%d\n", x, y, w, h);

        MPP_REGION_CREATE_RECT(&streamProcessViChn, &pStRegion[j], &pStRgnChnAttr[j], j, x, y, w, h);
    }

    return 0;
}

static int regionDestoryRect(AW_PDet_Output *detOutputs, int num)
{
    int left, right, top, bottom;
    int j = 0;

    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;

    StreamProcessPrivateData *pData = &(gSampleIntrusionMonitorContext.privateData.streamProcessData);
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;

    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    AW_PDet_Output *outputs = detOutputs;

    if (!outputs->num)
        return 0;
    if (outputs->num > num)
        outputs->num = num;

    for (j = 0; j < outputs->num; j++) {
        MPP_REGION_DESTORY_RECT(&streamProcessViChn, j);
    }

    return 0;
}

static int CreateLable(RGB_PIC_S *rgb, char *str)
{
    int ret = 0;

    ret = load_font_file(FONT_SIZE_16);
    if(ret < 0)
    {
        aloge("load_font_file FONT_SIZE fail! ret:%d\n", ret);
        return -1;
    }

    FONT_RGBPIC_S fontPic;
    fontPic.font_type     = FONT_SIZE_16;
    fontPic.rgb_type      = OSD_RGB_32;
    fontPic.enable_bg     = 0;
    fontPic.foreground[0] = 0xFF;
    fontPic.foreground[1] = 0xFF;
    fontPic.foreground[2] = 0xFF;
    fontPic.foreground[3] = 0xFF;
    fontPic.background[0] = 0x0;
    fontPic.background[1] = 0x0;
    fontPic.background[2] = 0x0;
    fontPic.background[3] = 0x0;
    rgb->enable_mosaic = 0;
    rgb->rgb_type      = OSD_RGB_32;
    create_font_rectangle(str, &fontPic, rgb);

    return 0;
}

static int regionCreateBitMap(AW_PDet_Output *detOutputs, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, int num)
{
    int x, y, w, h;
    int j = 0;
    char lableString[20];


    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;

    StreamProcessPrivateData *pData = &(gSampleIntrusionMonitorContext.privateData.streamProcessData);
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;
    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    AW_PDet_Output *outputs = detOutputs;

    if (!outputs->num)
        return 0;
    if (outputs->num > num)
        outputs->num = num;

    for (j = 0; j < outputs->num; j++) {
        x = outputs->person[j].bbox.tl_x;
        y = outputs->person[j].bbox.tl_y;
        x = AWALIGN(x, 16);
        y = AWALIGN(y, 16);

        snprintf(lableString, sizeof(lableString)-1, "score:%f", outputs->person[j].score);
        RGB_PIC_S rgb;
        CreateLable(&rgb, lableString);
        MPP_REGION_CREATE_BITMAP(&streamProcessVeChn, &pStRegion[j], &pStRgnChnAttr[j], j, x, y, &rgb);
        release_rgb_picture(&rgb);
    }

    return 0;
}

static int regionDestoryBitMap(AW_PDet_Output *detOutputs, int num)
{
    int left, right, top, bottom;
    int j = 0;

    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;

    StreamProcessPrivateData *pData = &(gSampleIntrusionMonitorContext.privateData.streamProcessData);
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;

    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    AW_PDet_Output *outputs = detOutputs;

    if (!outputs->num)
        return 0;
    if (outputs->num > num)
        outputs->num = num;

    for (j = 0; j < outputs->num; j++) {
        MPP_REGION_DESTORY_BITMAP(&streamProcessVeChn, j);
    }

    return 0;
}

int crossProduct(int x0, int y0, int x1, int y1, int x2, int y2) {
    return (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
}

int isPointInsideQuadrilateral(int x, int y, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
    int cp1 = crossProduct(x0, y0, x1, y1, x, y);
    int cp2 = crossProduct(x1, y1, x2, y2, x, y);
    int cp3 = crossProduct(x2, y2, x3, y3, x, y);
    int cp4 = crossProduct(x3, y3, x0, y0, x, y);

    if ((cp1 >= 0 && cp2 >= 0 && cp3 >= 0 && cp4 >= 0) || (cp1 <= 0 && cp2 <= 0 && cp3 <= 0 && cp4 <= 0)) {
        return 1;
    }
    return 0;
}

static int streamProcessCallback(unsigned char* yBuffer, unsigned char* uBuffer, unsigned char* vBuffer)
{
    return 0;
}

#define pixelNorm(x, i, o)            (x * o / i)

static int targetPositionDataNorm(int iw, int ih, int ow, int oh, AW_PDet_Output *result, AW_PDet_Output *resultNorm)
{
    int j = 0;
    int x0 =0, y0 = 0, x1 = 0, y1 = 0;
    int x0N =0, y0N = 0, x1N = 0, y1N = 0;

    if (result->num > SAMPLE_DETECT_MAX_NUM)
        result->num = SAMPLE_DETECT_MAX_NUM;

    resultNorm->num =  result->num;

    for (j = 0; j < result->num; j++) {
        // AW_Box box;
        x0 = result->person[j].bbox.tl_x * iw;
        y0 = result->person[j].bbox.tl_y * ih;
        x1 = result->person[j].bbox.br_x * iw;
        y1 = result->person[j].bbox.br_y * ih;

        x0N = pixelNorm(x0, iw, ow);
        y0N = pixelNorm(y0, ih, oh);
        x1N = pixelNorm(x1, iw, ow);
        y1N = pixelNorm(y1, ih, oh);

        resultNorm->person[j].bbox.tl_x = x0N;
        resultNorm->person[j].bbox.tl_y = y0N;
        resultNorm->person[j].bbox.br_x = x1N;
        resultNorm->person[j].bbox.br_y = y1N;

        resultNorm->person[j].label = result->person[j].label;
        resultNorm->person[j].score = result->person[j].score;

        if (resultNorm->person[j].label == 1) {
            printf("Detect person id %d: cls %d, prob %f, rect [%f, %f, %f, %f]\n", j,
                    resultNorm->person[j].label,
                    resultNorm->person[j].score,
                    resultNorm->person[j].bbox.tl_x,
                    resultNorm->person[j].bbox.tl_y,
                    resultNorm->person[j].bbox.br_x,
                    resultNorm->person[j].bbox.br_y);
        }
    }
    return 0;
}

int Position(int x0, int y0, int x1, int y1, int x, int y)
{
    int cross_product = (x1 - x0) * (y - y0) - (y1 - y0) * (x - x0);

    if (cross_product > 0) {
        return 1; //left
    } else if (cross_product < 0) {
        return -1; //right
    } else {
        return 0; //inline
    }
}

static int checkMonitorTargetPosition(AW_PDet_Output *resultNorm,  void *regionNorm)
{

    AW_PDet_Output *result = resultNorm;
    MonitorRegion *region  = regionNorm;
    int x = 0, y = 0, tl_x = 0, tl_y = 0, br_x = 0, br_y = 0;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0;
    int inside = 0;
    int cross = 0;
    int valid_cnt = 0;
    int j = 0;

    if (region->mode == CROSS_WORK_MODE_LINE) {
        x0 = region->settings.lines[0].x0;
        y0 = region->settings.lines[0].y0;
        x1 = region->settings.lines[0].x1;
        y1 = region->settings.lines[0].y1;
    } else if (region->mode == CROSS_WORK_MODE_RECT) {
        x0 = region->settings.rects[0].x[0];
        x1 = region->settings.rects[0].x[1];
        x2 = region->settings.rects[0].x[2];
        x3 = region->settings.rects[0].x[3];

        y0 = region->settings.rects[0].y[0];
        y1 = region->settings.rects[0].y[1];
        y2 = region->settings.rects[0].y[2];
        y3 = region->settings.rects[0].y[3];
    }

    for (j = 0; j < result->num; j++) {
       // AW_Box box;
        tl_x = result->person[j].bbox.tl_x;
        tl_y = result->person[j].bbox.tl_y;
        br_x = result->person[j].bbox.br_x;
        br_y = result->person[j].bbox.br_y;

        x =  (tl_x + br_x) / 2;
        y =  (tl_y + br_y) / 2;

        if (region->mode == CROSS_WORK_MODE_LINE) {
           if (Position(x0, y0, x1, y1, x, y) > 0)
                cross = 1;
            else
                cross = 0;
        } else if (region->mode == CROSS_WORK_MODE_RECT) {
            inside = isPointInsideQuadrilateral(x, y, x0, y0, x1, y1, x2, y2, x3, y3);
        }
        if (inside || cross) {
            if (valid_cnt != j)
                memcpy(&result->person[valid_cnt], &result->person[j], sizeof(AW_Person));
            valid_cnt++;
        }

    }
    result->num = valid_cnt;

    return valid_cnt;
}

static int ParseCmdLine(int argc, char **argv, SampleIntrusionMonitorCmdLineParam *pCmdLinePara)
{
    alogd("sample_FaceTrack path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleIntrusionMonitorCmdLineParam));
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                aloge("Fatal error! use -h to learn how to set parameter!!!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("Fatal error! file path[%s] too long: [%d]>=[%d]!", argv[i], strlen(argv[i]), MAX_FILE_PATH_SIZE);
            }
            strncpy(pCmdLinePara->configFilePath, argv[i], MAX_FILE_PATH_SIZE-1);
            pCmdLinePara->configFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
            printf("CmdLine param example:\n"
                    "\t run -path /home/sample_FaceTrack.conf\n");
            ret = 1;
            break;
        }
        else
        {
            alogd("ignore invalid CmdLine param:[%s], type -h to get how to set parameter!", argv[i]);
        }
        i++;
    }
    return ret;
}

static ERRORTYPE loadSampleIntrusionMonitorConfig(SampleIntrusionMonitorContext *pContext, const char *pConf)
{
    int ret = SUCCESS;

    SampleIntrusionMonitorConfig *pConfig = &(pContext->configPara);
    pContext->enableRecord = 0;

    /* venc stream config */
    pConfig->streamProcessConfig.viChn = (MPP_CHN_S){16, 0, 0}; //MOD_ID_VIU, 0, 0
    pConfig->streamProcessConfig.viWidth = SRC_WIDTH;
    pConfig->streamProcessConfig.viHeight = SRC_HEIGHT;
    pConfig->streamProcessConfig.viFps = SRC_FRAME_RATE;
    pConfig->streamProcessConfig.veChn = (MPP_CHN_S){8, 0, 0}; //MOD_ID_VENC, 0, 0

    pConfig->streamProcessConfig.venWidth = SRC_WIDTH;
    pConfig->streamProcessConfig.venHeight = SRC_HEIGHT;
    pConfig->streamProcessConfig.venFps = DST_FRAME_RATE;
    pConfig->streamProcessConfig.bitRate = DST_BIT_RATE;
    pConfig->streamProcessConfig.venType = DST_ENCODER_TYPE;

    pConfig->streamProcessConfig.region.enable = 1;
    pConfig->streamProcessConfig.region.mode = CROSS_WORK_MODE_RECT;
    pConfig->streamProcessConfig.region.settings.rects[0].x[0] = 520;
    pConfig->streamProcessConfig.region.settings.rects[0].y[0] = 301;
    pConfig->streamProcessConfig.region.settings.rects[0].x[1] = 385;
    pConfig->streamProcessConfig.region.settings.rects[0].y[1] = 821;
    pConfig->streamProcessConfig.region.settings.rects[0].x[2] = 1382;
    pConfig->streamProcessConfig.region.settings.rects[0].y[2] = 896;
    pConfig->streamProcessConfig.region.settings.rects[0].x[3] = 1680;
    pConfig->streamProcessConfig.region.settings.rects[0].y[3] = 250;

    pConfig->detectConfig.viChn = (MPP_CHN_S){16, 8, 0}; //MOD_ID_VIU, 8, 0
    pConfig->detectConfig.viWidth = VI_NPU_PIC_WIDTH;
    pConfig->detectConfig.viHeight = VI_NPU_PIC_HEIGHT;
    pConfig->detectConfig.viFps = VIPP_INPUT_FPS;
    strncpy(pConfig->detectConfig.nbg, NETWORK_BINARY, MAX_FILE_PATH_SIZE);
    pConfig->detectConfig.detWidth = NPU_DET_PIC_WIDTH;
    pConfig->detectConfig.detHeight = NPU_DET_PIC_HEIGHT;

    /*uvc config*/
    pConfig->uvcOutConfig.uvcDev = 2;
    pConfig->uvcOutConfig.config.uvc_enable = 1;
    pConfig->uvcOutConfig.config.uvc_bulk_mode = 0;
    pConfig->uvcOutConfig.config.uac_enable = 0;

    if(!pConf)
    {
        alogd("user not set config file. use default test parameter!");
        return ret;
    }

    CONFPARSER_S stConfParser;
    ret = createConfParser(pConf, &stConfParser);

    pConfig->streamProcessConfig.viChn.mModId = GetConfParaInt(&stConfParser, "stream_vi_mod_id", 0);
    pConfig->streamProcessConfig.viChn.mDevId = GetConfParaInt(&stConfParser, "stream_vi_dev_id", 0);
    pConfig->streamProcessConfig.viChn.mChnId = GetConfParaInt(&stConfParser, "stream_vi_chn_id", 0);
    pConfig->streamProcessConfig.viWidth = GetConfParaInt(&stConfParser, "stream_vi_width", 0);
    pConfig->streamProcessConfig.viHeight = GetConfParaInt(&stConfParser, "stream_vi_height", 0);
    pConfig->streamProcessConfig.viFps = GetConfParaInt(&stConfParser, "stream_vi_framerat", 0);

    pConfig->streamProcessConfig.veChn.mModId = GetConfParaInt(&stConfParser, "stream_ven_mod_id", 0);
    pConfig->streamProcessConfig.veChn.mDevId = GetConfParaInt(&stConfParser, "stream_ven_dev_id", 0);
    pConfig->streamProcessConfig.veChn.mChnId = GetConfParaInt(&stConfParser, "stream_ven_chn_id", 0);
    pConfig->streamProcessConfig.venWidth = GetConfParaInt(&stConfParser, "stream_ven_width", 0);
    pConfig->streamProcessConfig.venHeight = GetConfParaInt(&stConfParser, "stream_ven_height", 0);
    pConfig->streamProcessConfig.venFps = GetConfParaInt(&stConfParser, "stream_ven_framerat", 0);
    pConfig->streamProcessConfig.bitRate = GetConfParaInt(&stConfParser, "stream_ven_encode_bitrate", 0);

    char *ptr = NULL;
    ptr = (char *)GetConfParaString(&stConfParser, "stream_ven_encode_type", NULL);
    if (!ptr)
    {
        if (!strcmp(ptr, "H.264"))
            pConfig->streamProcessConfig.venType = PT_H264;
        else if (!strcmp(ptr, "H.265"))
            pConfig->streamProcessConfig.venType = PT_H265;
        else
        {
            alogw("unsupport data type[%s] use default H.264!", ptr);

            pConfig->streamProcessConfig.venType = PT_H264;
        }
    }
    else
    {
        alogw("check conf file! MJPEG insert data type why no set! use default H.264!");
        pConfig->streamProcessConfig.venType = PT_H264;
    }


    //pConfig->streamProcessConfig.region.enable = GetConfParaInt(&stConfParser, "monitor_enable", 0);
    pConfig->streamProcessConfig.region.enable = 1;
    if (pConfig->streamProcessConfig.region.enable) {
        pConfig->streamProcessConfig.region.mode = GetConfParaInt(&stConfParser, "monitor_mode", 0);
        if (pConfig->streamProcessConfig.region.mode == CROSS_WORK_MODE_LINE) {
            pConfig->streamProcessConfig.region.settings.lines[0].x0 = GetConfParaInt(&stConfParser, "line0_x0", 0);
            pConfig->streamProcessConfig.region.settings.lines[0].y0 = GetConfParaInt(&stConfParser, "line0_y0", 0);
            pConfig->streamProcessConfig.region.settings.lines[0].x1 = GetConfParaInt(&stConfParser, "line0_x1", 0);
            pConfig->streamProcessConfig.region.settings.lines[0].y1 = GetConfParaInt(&stConfParser, "line0_y1", 0);

        } else if (pConfig->streamProcessConfig.region.mode == CROSS_WORK_MODE_RECT) {

            pConfig->streamProcessConfig.region.settings.rects[0].x[0] = GetConfParaInt(&stConfParser, "rect0_x0", 0);
            pConfig->streamProcessConfig.region.settings.rects[0].y[0] = GetConfParaInt(&stConfParser, "rect0_y0", 0);
            pConfig->streamProcessConfig.region.settings.rects[0].x[1] = GetConfParaInt(&stConfParser, "rect0_x1", 0);
            pConfig->streamProcessConfig.region.settings.rects[0].y[1] = GetConfParaInt(&stConfParser, "rect0_y1", 0);
            pConfig->streamProcessConfig.region.settings.rects[0].x[2] = GetConfParaInt(&stConfParser, "rect0_x2", 0);
            pConfig->streamProcessConfig.region.settings.rects[0].y[2] = GetConfParaInt(&stConfParser, "rect0_y2", 0);
            pConfig->streamProcessConfig.region.settings.rects[0].x[3] = GetConfParaInt(&stConfParser, "rect0_x3", 0);
            pConfig->streamProcessConfig.region.settings.rects[0].y[3] = GetConfParaInt(&stConfParser, "rect0_y3", 0);
        } else {
            aloge("Fatal error! monitor mode is wrong, use default mode (CROSS_WORK_MODE_RECT).\n");
        }
    }

    /* track config */
    pConfig->detectConfig.viChn.mModId = GetConfParaInt(&stConfParser, "detect_vi_mod_id", 0);
    pConfig->detectConfig.viChn.mDevId = GetConfParaInt(&stConfParser, "detect_vi_dev_id", 0);
    pConfig->detectConfig.viChn.mChnId = GetConfParaInt(&stConfParser, "detect_vi_chn_id", 0);

    pConfig->detectConfig.viWidth = GetConfParaInt(&stConfParser, "detect_vi_width", 0);
    pConfig->detectConfig.viHeight = GetConfParaInt(&stConfParser, "detect_vi_height", 0);
    pConfig->detectConfig.viFps = GetConfParaInt(&stConfParser, "detect_vi_framerat", 0);

    ptr = (char*)GetConfParaString(&stConfParser,"nbg_file_path", NULL);
    strncpy(pConfig->detectConfig.nbg, ptr, MAX_FILE_PATH_SIZE);


    pConfig->detectConfig.detWidth = GetConfParaInt(&stConfParser, "detect_npu_width", 0);
    pConfig->detectConfig.detHeight = GetConfParaInt(&stConfParser, "detect_npu_height", 0);

    /*uvc config*/
    pConfig->uvcOutConfig.uvcDev = GetConfParaInt(&stConfParser, "uvc_dev", 0);

    return SUCCESS;
}

static int recordCallback(unsigned char *addr, unsigned int len, FILE* fp)
{

    FILE* recordFp = fp;
    if (!recordFp) {
        aloge("Write file failed.\n");
        return -1;
    }

    fwrite(addr, len, 1, recordFp);

    return 0;
}

extern int uvcGetVenStream(VENC_STREAM_S *pStream, VencHeaderData *pHeader);
static int uvcCallback(VENC_STREAM_S *pStream, VencHeaderData *pHeader)
{
    uvcGetVenStream(pStream, pHeader);
    return 0;
}

static ERRORTYPE sampleIntrusionMonitorPrivateDataSet(SampleIntrusionMonitorContext *pContext)
{
    int ret = SUCCESS;

    pContext->privateData.detectData.context = pContext;
    pContext->privateData.detectData.config = &pContext->configPara.detectConfig;
    pContext->privateData.detectData.detectCallback = npuDetectCallback;

    pContext->privateData.streamProcessData.context = pContext;
    pContext->privateData.streamProcessData.config = &pContext->configPara.streamProcessConfig;
    if (pContext->enableRecord)
        pContext->privateData.streamProcessData.recordCallback = recordCallback;
    pContext->privateData.streamProcessData.uvcCallback = uvcCallback;
    pContext->privateData.streamProcessData.streamProcessCallback = streamProcessCallback;
    pContext->privateData.streamProcessData.createRect = regionCreateRect;
    pContext->privateData.streamProcessData.destoryRect = regionDestoryRect;
    pContext->privateData.streamProcessData.createLabel = regionCreateBitMap;
    pContext->privateData.streamProcessData.destoryLabel = regionDestoryBitMap;

    pContext->privateData.streamProcessData.createMonitorRegion = createMonitorRegion;

    StreamProcessParaConfig *pConfig = pContext->privateData.streamProcessData.config;

    MonitorRegion *regionNorm = &pContext->privateData.streamProcessData.regionNorm;
    if (pConfig->region.enable) {
        regionNorm->enable = pConfig->region.enable;
        regionNorm->mode = pConfig->region.mode;
        if (regionNorm->mode == CROSS_WORK_MODE_LINE) {
            regionNorm->settings.lines[0].x0 = pConfig->region.settings.lines[0].x0 * pConfig->viWidth / 1920;
            regionNorm->settings.lines[0].y0 = pConfig->region.settings.lines[0].y0 * pConfig->viHeight / 1080;
            regionNorm->settings.lines[0].x1 = pConfig->region.settings.lines[0].x1 * pConfig->viWidth / 1920;
            regionNorm->settings.lines[0].y1 = pConfig->region.settings.lines[0].y1 * pConfig->viHeight / 1080;
        } else if(regionNorm->mode == CROSS_WORK_MODE_RECT) {
            regionNorm->settings.rects[0].x[0] = pConfig->region.settings.rects[0].x[0] * pConfig->viWidth / 1920;
            regionNorm->settings.rects[0].y[0] = pConfig->region.settings.rects[0].y[0] * pConfig->viHeight / 1080;
            regionNorm->settings.rects[0].x[1] = pConfig->region.settings.rects[0].x[1] * pConfig->viWidth / 1920;
            regionNorm->settings.rects[0].y[1] = pConfig->region.settings.rects[0].y[1] * pConfig->viHeight / 1080;
            regionNorm->settings.rects[0].x[2] = pConfig->region.settings.rects[0].x[2] * pConfig->viWidth / 1920;
            regionNorm->settings.rects[0].y[2] = pConfig->region.settings.rects[0].y[2] * pConfig->viHeight / 1080;
            regionNorm->settings.rects[0].x[3] = pConfig->region.settings.rects[0].x[3] * pConfig->viWidth / 1920;
            regionNorm->settings.rects[0].y[3] = pConfig->region.settings.rects[0].y[3] * pConfig->viHeight / 1080;

        } else {
            aloge("Fatal error! monitor mode is wrong, use default mode (CROSS_WORK_MODE_RECT).\n");
        }
    }

    pContext->privateData.uvcOutData.context = pContext;
    pContext->privateData.uvcOutData.config = &pContext->configPara.uvcOutConfig;

    return ret;
}

static ERRORTYPE sampleIntrusionMonitorMPPInit()
{
    return MPP_INIT();
}

static ERRORTYPE sampleIntrusionMonitorMPPUnInit()
{
    return MPP_EXIT();
}

static int NPU_DETECT_INIT(char *nbg, int width, int height, AW_PDet_Container *detContainer, AW_PDet_Output *detOutputs, AW_PDet_Output *detOutputsNorm)
{
    int ret = 0;
    AW_PDet_Container *container = detContainer;
    memset(container, 0, sizeof(AW_PDet_Container));
    AW_PDet_Output *outputs = detOutputs;
    memset(outputs, 0, sizeof(AW_PDet_Output));

    float threshold = 0.3;
    ret = aw_make_person_detection_container(container, nbg, width, height, threshold, outputs);
    if (ret == -1) {
        aloge("Fatal error! fail to aw_make_person_detect_container\n");
        return ret;
    }

    detOutputsNorm->num = SAMPLE_DETECT_MAX_NUM;
    int detSize = detOutputsNorm->num * sizeof(AW_Person);
    detOutputsNorm->person = malloc(detSize);
    if (detOutputsNorm->person == NULL)
        return -1;

    return 0;
}

static ERRORTYPE sampleIntrusionMonitorDetectInit(void *para)
{
    ERRORTYPE ret;
    MPP_CHN_S npuViChn;

    DetectPrivateData *pData = (DetectPrivateData *)para;
    DetectParaConfig *pConfig = (DetectParaConfig *)pData->config;

    pData->buffer = malloc(pConfig->detWidth * pConfig->detHeight * 3 / 2);
    if (pData->buffer == NULL)
        return FAILURE;

    memcpy(&npuViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    PIXEL_FORMAT_E format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    ret = MPP_VI_INIT(&npuViChn, pConfig->viWidth, pConfig->viHeight, pConfig->viFps, format);
    _CHECK_RET(ret);
    ret = MPP_VI_START(&npuViChn);
    _CHECK_RET(ret);

    NPU_DETECT_INIT(pConfig->nbg, pConfig->detWidth, pConfig->detHeight, &pData->detContainer, &pData->detOutputs,&pData->detOutputsNorm);

    pthread_mutex_init(&pData->detectDataLock, NULL);
    _CHECK_RET(ret);

    return ret;
}

#if 0
#include <sys/time.h>
double getCurrentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}
#endif

static ERRORTYPE sampleIntrusionMonitorDetectRun(void *para)
{
    ERRORTYPE ret;
    unsigned char *buffers[2];
    unsigned char *buffer;
    MPP_CHN_S npuViChn;
    DetectPrivateData *pData = (DetectPrivateData *)para;
    DetectParaConfig *pConfig = (DetectParaConfig *)pData->config;

    buffer = pData->buffer;
    buffers[0] = pData->buffer;
    buffers[1] = pData->buffer + pConfig->detWidth * pConfig->detHeight;
    memcpy(&npuViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    VIDEO_FRAME_INFO_S frameInfo;
    VideoFrameBufferSizeInfo vfbsInfo;
    ret = MPP_VI_GET_FRAME(&npuViChn, &frameInfo);
    _CHECK_RET(ret);

    memset(&vfbsInfo, 0, sizeof(VideoFrameBufferSizeInfo));
    getVideoFrameBufferSizeInfo(&frameInfo, &vfbsInfo);

    //YUV
    int offset = 0;
    if (frameInfo.VFrame.mpVirAddr[0]) {
        memcpy(buffer, frameInfo.VFrame.mpVirAddr[0], vfbsInfo.mYSize);
        offset += vfbsInfo.mYSize;
    }
    if (frameInfo.VFrame.mpVirAddr[1]) {
        memcpy(buffer + offset, frameInfo.VFrame.mpVirAddr[1], vfbsInfo.mUSize);
        offset += vfbsInfo.mUSize;
    }
    if (frameInfo.VFrame.mpVirAddr[2])
        memcpy(buffer + offset, frameInfo.VFrame.mpVirAddr[1], vfbsInfo.mVSize);

//    double start = getCurrentTime();
    if (pData->detectCallback) {
            pData->detectCallback(buffers, &pData->detContainer, &pData->detOutputs);
    }
//    double end = getCurrentTime();
//    double time = end - start;
//    printf("%s,line:%d,time:%ld\n",__func__,__LINE__,time);

    SampleIntrusionMonitorContext *pContext = (SampleIntrusionMonitorContext*)(pData->context);
    StreamProcessPrivateData* pStreamProcessData = &(pContext->privateData.streamProcessData);
    StreamProcessParaConfig *pStreamProcessConfig = (StreamProcessParaConfig *)pStreamProcessData->config;
    pthread_mutex_lock(&pData->detectDataLock);
    targetPositionDataNorm(pConfig->detWidth, pConfig->detHeight, pStreamProcessConfig->venWidth, pStreamProcessConfig->venHeight, &pData->detOutputs, &pData->detOutputsNorm);
    pthread_mutex_unlock(&pData->detectDataLock);

    ret = MPP_VI_RELEASE_FRAME(&npuViChn, &frameInfo);
    _CHECK_RET(ret);

#if 0
    AW_PDet_Output *outputs = &pData->detOutputs;
    int j = 0;
    int x0, y0, x1, y1;
    unsigned char* yBuffer = frameInfo.VFrame.mpVirAddr[0];
    unsigned char* uBuffer = frameInfo.VFrame.mpVirAddr[1];
    for (j = 0; j < outputs->num; j++) {
        x0 = outputs->person[j].bbox.tl_x * pConfig->detWidth;
        y0 = outputs->person[j].bbox.tl_y * pConfig->detHeight;
        x1 = outputs->person[j].bbox.br_x * pConfig->detWidth;
        y1 = outputs->person[j].bbox.br_y * pConfig->detHeight;
        nv12_draw_rect(yBuffer, uBuffer, pConfig->detWidth, pConfig->detHeight, x0, y0, x1, y1, 0x96, 0x2C, 0x15);
    }
    if (!testFp) testFp = fopen("./test.yuv", "wb");
    if (testFp) {
        fwrite(yBuffer, pConfig->detWidth * pConfig->detHeight, 1, testFp);
        fwrite(uBuffer, pConfig->detWidth * pConfig->detHeight / 2, 1, testFp);
        //fclose(testFp);
    }
#endif
    return ret;
}

static int NPU_DETECT_UNINIT(AW_PDet_Container *detContainer, AW_PDet_Output *detOutputs, AW_PDet_Output *detOutputsNorm)
{
    AW_PDet_Container *container = detContainer;
    AW_PDet_Output *outputs = detOutputs;
    aw_free_person_detection_container(container, outputs);

    if (detOutputsNorm->person != NULL) {
        free(detOutputsNorm->person);
        detOutputsNorm->person = NULL;
    }
    return 0;
}
static ERRORTYPE sampleIntrusionMonitorDetectUnInit(void *para)
{
    ERRORTYPE ret;
    unsigned char *buffer;
    MPP_CHN_S npuViChn;

    DetectPrivateData *pData = (DetectPrivateData *)para;
    DetectParaConfig *pConfig = (DetectParaConfig *)pData->config;

    buffer = pData->buffer;
    memcpy(&npuViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    pthread_mutex_destroy(&pData->detectDataLock);
    NPU_DETECT_UNINIT(&pData->detContainer, &pData->detOutputs, &pData->detOutputsNorm);

    ret = MPP_VI_STOP(&npuViChn);
    _CHECK_RET(ret);

    ret = MPP_VI_UNINIT(&npuViChn);
    _CHECK_RET(ret);

    return ret;
}

static void *sampleIntrusionMonitorDetectTask(void *para)
{
    ERRORTYPE ret;

    DetectPrivateData *pData = (DetectPrivateData *)para;

    ret = sampleIntrusionMonitorDetectInit(pData);
    if (ret) {
        aloge("Fatal error! sampleIntrusionMonitorDetectInit failed (ret:%d)\n", ret);
        return NULL;
    }

    while (!gStop) {
        ret = sampleIntrusionMonitorDetectRun(pData);
        if (ret)  {
            aloge("Fatal error! sampleIntrusionMonitorDetectRun failed (ret:%d)\n", ret);
            break;
        }
    }

    ret = sampleIntrusionMonitorDetectUnInit(pData);
    if (ret)
        aloge("Fatal error! sampleIntrusionMonitorDetectUnInit failed (ret:%d)\n", ret);

    return NULL;
}

VencHeaderData gVencHeader;

ERRORTYPE sampleIntrusionMonitorStreamProcessInit(void *para)
{
    ERRORTYPE ret;
    StreamProcessPrivateData *pData = (StreamProcessPrivateData *)para;
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;
    MPP_CHN_S streamProcessViChn;
    MPP_CHN_S streamProcessVeChn;

    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));


    PIXEL_FORMAT_E format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    ret = MPP_VI_INIT2(&streamProcessViChn, pConfig->viWidth, pConfig->viHeight, pConfig->viFps, format);
    _CHECK_RET(ret);

    int srcWidth = pConfig->viWidth;
    int srcHeight = pConfig->viHeight;
    int srcFps = pConfig->viFps ;
    int dstWidth = pConfig->venWidth ;
    int dstHeight = pConfig->venHeight ;
    int dstFps = pConfig->venFps;
    int dstEncoderType = pConfig->venType;
    int dstBitRate = pConfig->bitRate;

    ret = MPP_VENC_INIT(&streamProcessVeChn, srcWidth, srcHeight, srcFps, dstWidth, dstHeight, dstFps, dstEncoderType, dstBitRate, format);
    _CHECK_RET(ret);

    //    ret = MPP_SYS_BIND(&streamProcessViChn, &streamProcessVeChn);
    //    _CHECK_RET(ret);

    ret = MPP_VI_START(&streamProcessViChn);
    _CHECK_RET(ret);

    ret = MPP_VENC_START(&streamProcessVeChn);
    _CHECK_RET(ret);

    VencHeaderData header;
    if (PT_H264 == DST_ENCODER_TYPE) {
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(streamProcessVeChn.mChnId, &header);
    } else if (PT_H265 == DST_ENCODER_TYPE) {
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(streamProcessVeChn.mChnId, &header);
    } else {
        ret = -1;
    }

    if (ret == SUCCESS && header.nLength) {
        if (pData->recordCallback)
            pData->recordCallback(header.pBuffer, header.nLength, pData->recordFp);
    } else {
        return ret;
    }
    memcpy(&gVencHeader, &header, sizeof(VencHeaderData));

    return ret;
}

ERRORTYPE sampleIntrusionMonitorStreamProcessRun(void *para)
{
    ERRORTYPE ret;
    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;

    StreamProcessPrivateData *pData = (StreamProcessPrivateData *)para;
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;

    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    SampleIntrusionMonitorContext *pContext = (SampleIntrusionMonitorContext*)(pData->context);
    DetectPrivateData *pDetectData = &(pContext->privateData.detectData);
    AW_PDet_Output dOutputs;
    dOutputs.person = malloc(MPP_REGION_RECT_NUM * sizeof(AW_Person));
    if (dOutputs.person == NULL)
        return -1;

    pthread_mutex_lock(&pDetectData->detectDataLock);
    dOutputs.num = pDetectData->detOutputsNorm.num;
    if (dOutputs.num > MPP_REGION_RECT_NUM)
        dOutputs.num = MPP_REGION_RECT_NUM;
    memcpy(dOutputs.person, pDetectData->detOutputsNorm.person, dOutputs.num * sizeof(AW_Person));
    pthread_mutex_unlock(&pDetectData->detectDataLock);

    int position = checkMonitorTargetPosition(&dOutputs, &pData->regionNorm);

    if (position && pData->createRect) {
        pData->createRect(&dOutputs, stRegion, stRgnChnAttr, MPP_REGION_RECT_NUM);
    }

    /* Get vin stream */
    VIDEO_FRAME_INFO_S frameInfo;
    ret = MPP_VI_GET_FRAME(&streamProcessViChn, &frameInfo);
    _CHECK_RET(ret);

    if (position && pData->destoryRect) {
        pData->destoryRect(&dOutputs, MPP_REGION_RECT_NUM);
    }

    if (pData->createMonitorRegion) {
        pData->createMonitorRegion(&pData->regionNorm, pConfig->venWidth, pConfig->venHeight,
                frameInfo.VFrame.mpVirAddr[0], frameInfo.VFrame.mpVirAddr[1], frameInfo.VFrame.mpVirAddr[2]);
    }

#if 0
    if (position)
        createMonitorTargetRect(&dOutputs, pConfig->venWidth, pConfig->venHeight,
                frameInfo.VFrame.mpVirAddr[0], frameInfo.VFrame.mpVirAddr[1], frameInfo.VFrame.mpVirAddr[2]);
#endif

    if (pData->streamProcessCallback) {
        pData->streamProcessCallback(frameInfo.VFrame.mpVirAddr[0], frameInfo.VFrame.mpVirAddr[1],
                frameInfo.VFrame.mpVirAddr[2]);
    }
    if (position && pData->createLabel) {
        pData->createLabel(&dOutputs, stRegion, stRgnChnAttr, MPP_REGION_RECT_NUM);
    }
    ret = MPP_VEN_SEND_FRAME(&streamProcessVeChn, &frameInfo);
    _CHECK_RET(ret);

    ret = MPP_VI_RELEASE_FRAME(&streamProcessViChn, &frameInfo);
    _CHECK_RET(ret);

    /* Get vin stream */
    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    memset(&stVencStream, 0, sizeof(VENC_STREAM_S));
    memset(&stVencPack, 0, sizeof(VENC_PACK_S));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;

    VENC_STREAM_S *stream =  &stVencStream;

    ret = MPP_VENC_GET_STREAM(&streamProcessVeChn, stream);
    if (ret != 0) {
        printf("Get venc stream failed!");
        return ret;
    }

    if (position && pData->destoryLabel) {
        pData->destoryLabel(&dOutputs, MPP_REGION_RECT_NUM);
    }

    if (dOutputs.person != NULL) {
        free(dOutputs.person);
        dOutputs.person = NULL;
    }

    /* record stream */
    if (pData->recordCallback) {
        if (stream->mpPack != NULL && stream->mpPack->mLen0) {
            pData->recordCallback(stream->mpPack->mpAddr0, stream->mpPack->mLen0, pData->recordFp);
        }
        if (stream->mpPack != NULL && stream->mpPack->mLen1) {
            pData->recordCallback(stream->mpPack->mpAddr1, stream->mpPack->mLen1, pData->recordFp);
        }
    }

    /* stream model */
    if (pData->uvcCallback) {

        VencHeaderData *pHeader = &gVencHeader;

        if (pHeader->nLength) {
            pData->uvcCallback(stream, pHeader);
        }
    }

    ret = MPP_VENC_RELEASE_STREAM(&streamProcessVeChn, stream);
    if (SUCCESS != ret) {
        printf("Release stream failed.");
    }

    return ret;
}

ERRORTYPE sampleIntrusionMonitorStreamProcessUnInit(void *para)
{
    ERRORTYPE ret;

    StreamProcessPrivateData *pData = (StreamProcessPrivateData *)para;
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;
    MPP_CHN_S streamProcessViChn;
    MPP_CHN_S streamProcessVeChn;

    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));

    ret = MPP_VENC_STOP(&streamProcessVeChn);
    _CHECK_RET(ret);

    ret = MPP_VI_STOP(&streamProcessViChn);
    _CHECK_RET(ret);

//    ret = MPP_SYS_UNBIND(&streamProcessViChn, &streamProcessVeChn);
//    _CHECK_RET(ret);

    ret = MPP_VI_UNINIT(&streamProcessViChn);
    _CHECK_RET(ret);

    ret = MPP_VENC_UNINIT(&streamProcessVeChn);
    _CHECK_RET(ret);

    return ret;
}

#define sampleIntrusionMonitorRecordFile "motion.h264"
static void *sampleIntrusionMonitorStreamProcessTask(void *para)
{
    ERRORTYPE ret;
    int i;

    StreamProcessPrivateData *pData = (StreamProcessPrivateData *)para;
    pData->recordFp = fopen(sampleIntrusionMonitorRecordFile, "wb+");

    ret = sampleIntrusionMonitorStreamProcessInit(pData);
    if (ret) {
        aloge("sampleIntrusionMonitorStreamProcessInit failed (ret:%d)\n", ret);
        goto RecordInitERR;
    }

    while (!gStop) {
        ret = sampleIntrusionMonitorStreamProcessRun(pData);
        if (ret) {
            aloge("sampleIntrusionMonitorStreamProcessRun failed (ret:%d)\n", ret);
            break;
        }
    }
  //  if (testFp != NULL)
  //  fclose(testFp);

RecordInitERR:
    if (pData->recordFp)
        fclose(pData->recordFp);

    ret = sampleIntrusionMonitorStreamProcessUnInit(pData);
    if (ret)
        aloge("sampleIntrusionMonitorStreamProcessUnInit failed (ret:%d)\n", ret);

    return NULL;
}

static int sampleIntrusionMonitorStart()
{
    int ret;

    gStop = 0;

    DetectPrivateData *pDetectData;
    pDetectData = &(gSampleIntrusionMonitorContext.privateData.detectData);
    ret = pthread_create(&gDetectThread, NULL, sampleIntrusionMonitorDetectTask, pDetectData);
    if (ret != 0) {
        aloge("Fatal error! Failed to create thread for detect.\n");
        return ret;
    }

    StreamProcessPrivateData *pStreamProcessData;
    pStreamProcessData = &(gSampleIntrusionMonitorContext.privateData.streamProcessData);
    ret = pthread_create(&gStreamProcessThread, NULL, sampleIntrusionMonitorStreamProcessTask, pStreamProcessData);
    if (ret != 0) {
        aloge("Fatal error! Failed to create thread for preview.\n");
        return ret;
    }

    UvcOutPrivateData *pUvcOutData;
    pUvcOutData = &(gSampleIntrusionMonitorContext.privateData.uvcOutData);
    ret = pthread_create(&gUvcOutThread, NULL, sampleIntrusionMonitorUvcOutTask, pUvcOutData);
    if (ret != 0) {
        aloge("Fatal error! Failed to create thread for preview.\n");
        return ret;
    }

    return ret;
}

static int sampleIntrusionMonitorStop()
{
    unsigned long value;

    gStop = 1;
    sleep(3);
    pthread_join(gUvcOutThread, (void **)&value);
    pthread_join(gDetectThread, (void **)&value);
    pthread_join(gStreamProcessThread, (void **)&value);
    return 0;
}

int main(int argc, char *argv[]) {

    int ret = 0;
    char *pConfigFilePath;

    /* parse command line param */
    ret = ParseCmdLine(argc, argv, &gSampleIntrusionMonitorContext.cmdLinePara);
    if(ret) {
        aloge("Fatal error! command line param is wrong, exit!");
        return ret;
    }

    if(strlen(gSampleIntrusionMonitorContext.cmdLinePara.configFilePath) > 0) {
        pConfigFilePath = gSampleIntrusionMonitorContext.cmdLinePara.configFilePath;
    } else {
        pConfigFilePath = NULL;
    }

    ret = loadSampleIntrusionMonitorConfig(&gSampleIntrusionMonitorContext, pConfigFilePath);
    if(ret) {
        aloge("Fatal error! no config file or parse conf file fail");
        return ret;
    }

    ret = sampleIntrusionMonitorPrivateDataSet(&gSampleIntrusionMonitorContext);
    if(ret) {
        aloge("Fatal error! no config file or parse conf file fail");
        return ret;
    }

    ret = sampleIntrusionMonitorMPPInit();
    if (ret) {
        aloge("Fatal error! sample_RegionDetect MPP Init Failed. \n");
        return ret;
    }

    ret = sampleIntrusionMonitorStart();
    if (ret) {
        aloge("Fatal error! sample_RegionDetect start Failed. \n");
        return ret;
    }

    signal(SIGINT, sampleIntrusionMonitorRecvSignal);
    while (gKeepRunning) {
        sleep(1);
    }

    ret = sampleIntrusionMonitorStop();
    if (ret)
        aloge("Fatal error! sample_RegionDetect stop Failed. \n");

    ret = sampleIntrusionMonitorMPPUnInit();
    if (ret)
        aloge("Fatal error! sample_RegionDetect MPP UnInit Failed. \n");

    return ret;
}
