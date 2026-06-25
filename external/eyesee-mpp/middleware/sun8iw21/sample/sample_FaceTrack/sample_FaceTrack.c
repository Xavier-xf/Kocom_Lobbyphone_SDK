#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <utils/plat_log.h>
#include <VIDEO_FRAME_INFO_S.h>

#include "awf_detection.h"
#include "confparser.h"
#include "rgb_ctrl.h"
#include "mm_comm_region.h"
#include "MppHelper.h"
#include "sample_FaceTrack.h"

//#define NETWORK_BINARY          "./Facedet_480_288_nv12.nb"
//#define NETWORK_BINARY          "./Facedet_480_288_nv12"
#define NETWORK_BINARY          "./face_detection_v5.0.1_beta"

#define VI_NPU_PIC_WIDTH        640
#define VI_NPU_PIC_HEIGHT       360
#define NPU_DET_PIC_WIDTH       640
#define NPU_DET_PIC_HEIGHT      360
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

#define SAMPLE_FACE_DETECT_MAX_NUM 10
#define MPP_REGION_RECT_NUM  SAMPLE_FACE_DETECT_MAX_NUM

pthread_t gStreamProcessThread;
pthread_t gDetectThread;
pthread_t gUvcOutThread;

SampleFaceTrackContext gSampleFaceTrackContext;

AW_Tracker_Output gTrackOutput;
AW_Box_Base gBoxInfo[SAMPLE_FACE_DETECT_MAX_NUM];

volatile int gStop;
volatile int gKeepRunning = 1;
void sampleFaceTrackRecvSignal(int sig) {
    alogd("Catch ctrl+c signal and exit...\n");
    gKeepRunning = 0;
}

long getCurrentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int npuDetectCallback(unsigned char *yuvBuffer,AWF_Det_Container *detContainer, AWF_Det_Outputs *detOutputs)
{
    AWF_Det_Container *container = detContainer;
    AWF_Det_Outputs *outputs = detOutputs;

    awf_run_det(container, yuvBuffer, outputs);

    for (int j = 0; j < outputs->num; j++)
    {
        int x1 = outputs->faces[j].bbox.tl_x;
        int y1 = outputs->faces[j].bbox.tl_y;
        int x2 = outputs->faces[j].bbox.br_x;
        int y2 = outputs->faces[j].bbox.br_y;
        int s = outputs->faces[j].confid_score;
  //      printf("rect: %d %d %d %d %d.\n", x1, y1, x2, y2, s);

        for (int k = 0; k < NUM_KEYPOINTS; k++)
        {
            int landms_x = outputs->faces[j].ldmk.cx[k];
            int landms_y = outputs->faces[j].ldmk.cy[k];
        }
    }

    return 0;
}

static int trackCallback(AWF_Det_Outputs *input, AW_Tracker_Container *trackContainer,AW_Det_Input *trackInput, AW_Tracker_Output *trackOutput)
{
    int i, j;

    DetectPrivateData *pData =  &(gSampleFaceTrackContext.privateData.detectData);

    AWF_Det_Outputs *res = (AWF_Det_Outputs *)input;
    AW_Tracker_Container *container = (AW_Tracker_Container *)trackContainer;
    AW_Tracker_Output *tracker_output = (AWF_Det_Outputs *)trackOutput;
    AW_Det_Input *det_input = (AW_Det_Input *)trackInput;

    int valid_human_cnt = 0;
    for (j = 0; j < res->num; j++) {

        AW_Box box;
        box.tl_x = res->faces[j].bbox.tl_x;
        box.tl_y = res->faces[j].bbox.tl_y;
        box.br_x = res->faces[j].bbox.br_x;
        box.br_y = res->faces[j].bbox.br_y;

        box.width = box.br_x - box.tl_x;
        box.height = box.br_y - box.tl_y;
        det_input->box_info[valid_human_cnt].label = 0;
        det_input->box_info[valid_human_cnt].confidence = res->faces[j].confid_score;
        det_input->box_info[valid_human_cnt].box = box;
        valid_human_cnt++;
    }

    det_input->num = valid_human_cnt;

    if (det_input->num == 0) {
        tracker_output->num = 0;
        return 0;
    }

    aw_run_tracker(container, det_input, tracker_output);

#if 0
    for (j = 0; j < tracker_output->num; j++) {
        printf("Detect procee: Track ID:%d, classID:%d, x:%d, y:%d, width:%d, height:%d\n",
                tracker_output->box_info[j].track_id,
                tracker_output->box_info[j].label,
                tracker_output->box_info[j].box.tl_x,
                tracker_output->box_info[j].box.tl_y,
                tracker_output->box_info[j].box.width,
                tracker_output->box_info[j].box.height);
    }

#endif
    return 0;
}


RGN_ATTR_S stRegion[MPP_REGION_RECT_NUM];
RGN_CHN_ATTR_S stRgnChnAttr[MPP_REGION_RECT_NUM];

static int regionCreateRect(AW_Tracker_Output *tracker_output, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, int num)
{
    int x, y, w, h;
    int j = 0;

    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;

    StreamProcessPrivateData *pData = &(gSampleFaceTrackContext.privateData.streamProcessData);
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;
    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));


    if (!tracker_output->num)
        return 0;
    if (tracker_output->num > num)
        tracker_output->num = num;

    for (j = 0; j < tracker_output->num; j++) {
        x = tracker_output->box_info[j].box.tl_x * DST_WIDTH / NPU_DET_PIC_WIDTH;
        y = tracker_output->box_info[j].box.tl_y * DST_HEIGHT / NPU_DET_PIC_HEIGHT;
        w = tracker_output->box_info[j].box.width * DST_WIDTH / NPU_DET_PIC_WIDTH;
        h = tracker_output->box_info[j].box.height * DST_HEIGHT / NPU_DET_PIC_HEIGHT;;
       // printf("%s,line:%d,x:%d,y:%d,w:%d,h:%d\n",__func__,__LINE__, x, y, w, h);
        MPP_REGION_CREATE_RECT(&streamProcessViChn, &pStRegion[j], &pStRgnChnAttr[j], j, x, y, w, h);
    }

    return 0;
}

static int regionDestoryRect(AW_Tracker_Output *tracker_output, int num)
{
    int left, right, top, bottom;
    int j = 0;

    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;

    StreamProcessPrivateData *pData = &(gSampleFaceTrackContext.privateData.streamProcessData);
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;

    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    if (!tracker_output->num)
        return 0;
    if (tracker_output->num > num)
        tracker_output->num = num;

    for (j = 0; j < tracker_output->num; j++) {
        MPP_REGION_DESTORY_RECT(&streamProcessViChn, j);
    }
    return 0;

}

static int CreateTracKLable(RGB_PIC_S *rgb, int id)
{
    char lableString[15];
    int ret = 0;

    ret = load_font_file(FONT_SIZE_16);
    if(ret < 0)
    {
        aloge("load_font_file FONT_SIZE fail! ret:%d\n", ret);
        return -1;
    }

    snprintf(lableString, sizeof(lableString)-1, "Track_ID%d", id);

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
    create_font_rectangle(lableString, &fontPic, rgb);

    return 0;
}

static int regionCreateBitMap(AW_Tracker_Output *tracker_output, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, int num)
{
    int x, y, w, h;
    int j = 0;

    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;

    StreamProcessPrivateData *pData = &(gSampleFaceTrackContext.privateData.streamProcessData);
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;
    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));


    if (!tracker_output->num)
        return 0;
    if (tracker_output->num > num)
        tracker_output->num = num;

    for (j = 0; j < tracker_output->num; j++) {
        x = tracker_output->box_info[j].box.tl_x * DST_WIDTH / NPU_DET_PIC_WIDTH;
        y = tracker_output->box_info[j].box.tl_y * DST_HEIGHT / NPU_DET_PIC_HEIGHT;
        w = tracker_output->box_info[j].box.width * DST_WIDTH / NPU_DET_PIC_WIDTH;
        h = tracker_output->box_info[j].box.height * DST_HEIGHT / NPU_DET_PIC_HEIGHT;;

        x = AWALIGN(x, 16);
        y = AWALIGN(y, 16);
        RGB_PIC_S rgb;
        CreateTracKLable(&rgb, tracker_output->box_info[j].track_id);
        MPP_REGION_CREATE_BITMAP(&streamProcessVeChn, &pStRegion[j], &pStRgnChnAttr[j], j, x, y, &rgb);
        release_rgb_picture(&rgb);
    }

    return 0;
}

static int regionDestoryBitMap(AW_Tracker_Output *tracker_output, int num)
{
    int left, right, top, bottom;
    int j = 0;

    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;

    StreamProcessPrivateData *pData = &(gSampleFaceTrackContext.privateData.streamProcessData);
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;

    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    if (!tracker_output->num)
        return 0;
    if (tracker_output->num > num)
        tracker_output->num = num;

    for (j = 0; j < tracker_output->num; j++) {
        MPP_REGION_DESTORY_BITMAP(&streamProcessVeChn, j);
    }

    return 0;
}

static int streamProcessCallback(unsigned char* yBuffer, unsigned char* uBuffer, unsigned char* vBuffer)
{
#if 0
    int left, right, top, bottom;
    AW_Tracker_Output tracker_output;
    int j = 0;

    DetectPrivateData *pData =  &(gSampleFaceTrackContext.privateData.detectData);
    memcpy(&tracker_output, &(pData->trackerOutput), sizeof(AW_Tracker_Output));

    for (j = 0; j < tracker_output.num; j++) {
        left = tracker_output.box_info[j].box.tl_x;
        right = tracker_output.box_info[j].box.tl_x + tracker_output.box_info[j].box.width;
        top = tracker_output.box_info[j].box.tl_y;
        bottom = tracker_output.box_info[j].box.tl_y + tracker_output.box_info[j].box.height;
        // printf("%s,line:%d, left:%d, right:%d, top:%d, bottom:%d\n",__func__,__LINE__,left, right, top, bottom);
        nv12_draw_rect(yBuffer, uBuffer,
                left * DST_WIDTH / NPU_DET_PIC_WIDTH,
                top * DST_HEIGHT / NPU_DET_PIC_HEIGHT,
                right * DST_WIDTH / NPU_DET_PIC_WIDTH,
                bottom * DST_HEIGHT / NPU_DET_PIC_HEIGHT,
                0x96, 0x2C, 0x15 // green
                );
    }
#endif
    return 0;
}

static int ParseCmdLine(int argc, char **argv, SampleFaceTrackCmdLineParam *pCmdLinePara)
{
    alogd("sample_FaceTrack path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleFaceTrackCmdLineParam));
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

static ERRORTYPE loadSampleFaceTrackConfig(SampleFaceTrackContext *pContext, const char *pConf)
{
    int ret = SUCCESS;

    SampleFaceTrackConfig *pConfig = &(pContext->configPara);
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

    /* track config */
    pConfig->detectConfig.viChn = (MPP_CHN_S){16, 8, 0}; //MOD_ID_VIU, 8, 0
    pConfig->detectConfig.viWidth = VI_NPU_PIC_WIDTH;
    pConfig->detectConfig.viHeight = VI_NPU_PIC_HEIGHT;
    pConfig->detectConfig.viFps = VIPP_INPUT_FPS;

    strncpy(pConfig->detectConfig.nbg, NETWORK_BINARY, MAX_FILE_PATH_SIZE);
//    pConfig->detectConfig.memSize = DET_MEM_SIZE;
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

//    pConfig->streamProcessConfig.viChn = (MPP_CHN_S){16, 0, 0}; //MOD_ID_VIU, 0, 0

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

    /* track config */
    pConfig->detectConfig.viChn.mModId = GetConfParaInt(&stConfParser, "detect_vi_mod_id", 0);
    pConfig->detectConfig.viChn.mDevId = GetConfParaInt(&stConfParser, "detect_vi_dev_id", 0);
    pConfig->detectConfig.viChn.mChnId = GetConfParaInt(&stConfParser, "detect_vi_chn_id", 0);

    pConfig->detectConfig.viWidth = GetConfParaInt(&stConfParser, "detect_vi_width", 0);
    pConfig->detectConfig.viHeight = GetConfParaInt(&stConfParser, "detect_vi_height", 0);
    pConfig->detectConfig.viFps = GetConfParaInt(&stConfParser, "detect_vi_framerat", 0);

    //.nbg = NETWORK_BINARY,
    ptr = (char*)GetConfParaString(&stConfParser,"nbg_file_path", NULL);
    strncpy(pConfig->detectConfig.nbg, ptr, MAX_FILE_PATH_SIZE);


   // pConfig->detectConfig.memSize = GetConfParaInt(&stConfParser, "detect_npu_memSize", 0);
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

static ERRORTYPE sampleFaceTrackPrivateDataSet(SampleFaceTrackContext *pContext)
{
    int ret = SUCCESS;

    pContext->privateData.detectData.config = &pContext->configPara.detectConfig;
  //  pContext->privateData.detectData.detectCallback = npuCallback;
    pContext->privateData.detectData.detectCallback = npuDetectCallback;
    pContext->privateData.detectData.trackCallback = trackCallback;

    pContext->privateData.streamProcessData.config = &pContext->configPara.streamProcessConfig;
    if (pContext->enableRecord)
        pContext->privateData.streamProcessData.recordCallback = recordCallback;
    pContext->privateData.streamProcessData.uvcCallback = uvcCallback;
    pContext->privateData.streamProcessData.streamProcessCallback = streamProcessCallback;
    pContext->privateData.streamProcessData.createRect = regionCreateRect;
    pContext->privateData.streamProcessData.destoryRect = regionDestoryRect;
    pContext->privateData.streamProcessData.createLabel = regionCreateBitMap;
    pContext->privateData.streamProcessData.destoryLabel = regionDestoryBitMap;

    pContext->privateData.uvcOutData.config = &pContext->configPara.uvcOutConfig;

    return ret;
}

static ERRORTYPE sampleFaceTrackMPPInit()
{
    return MPP_INIT();
}

static ERRORTYPE sampleFaceTrackMPPUnInit()
{
    return MPP_EXIT();
}

static int NPU_DETECT_INIT(char *nbg, int width, int height, AWF_Det_Container *detContainer, AWF_Det_Outputs *detOutputs)
{
    int ret = 0;
    /* face detect */
    AWF_Det_Container *container = detContainer;
    memset(container, 0, sizeof(AWF_Det_Container));
    AWF_Det_Outputs *outputs = detOutputs;
    memset(outputs, 0, sizeof(AWF_Det_Outputs));

    ret = awf_make_det_container(container, nbg, outputs);
    if (ret == -1) {
        aloge("Fatal error! fail to make face detction container\n");
        return ret;
    }

    float confid_ths = 0.6;
    float nms_ths = 0.45;
    ret = awf_set_det_runtime(container, confid_ths, nms_ths, width, height);
    if (ret == -1) {
        aloge("Fatal error! fail to set det runtime\n");
        return ret;
    }
    return 0;
}

static int OBJECT_TRACK_INIT(AW_Tracker_Container *trackContainer,AW_Det_Input *trackInput, AW_Tracker_Output *trackOutput)
{

    memset(&gTrackOutput, 0, sizeof(AW_Tracker_Output));
    memset(gBoxInfo, 0, SAMPLE_FACE_DETECT_MAX_NUM * sizeof(AW_Box_Base));
    gTrackOutput.box_info = gBoxInfo;

    AW_Tracker_Container *container = trackContainer;
    AW_Tracker_Output *tracker_output = trackOutput;
    AW_Det_Input *det_input = trackInput;

    memset(container, 0, sizeof(AW_Tracker_Container));
    memset(tracker_output, 0, sizeof(AW_Tracker_Output));
    memset(det_input, 0, sizeof(AW_Det_Input));

    int frame_rate = 30;
    int max_loss_frames = 30;
    int flag = aw_make_tracker(container, frame_rate, max_loss_frames, tracker_output);
    if (flag == -1) {
        aloge("Fatal error! fail to make tracker container. \n");
        return -1;
    }

    det_input->box_info = (AW_Box_Base*)calloc(SAMPLE_FACE_DETECT_MAX_NUM, sizeof(AW_Box_Base));
    if (!det_input->box_info) {
        aloge("Fatal error! fail to allocate memory for [det_input->box_info]. \n");
        return -1;
    }

    return 0;
}

static int OBJECT_TRACK_UNINIT(AW_Tracker_Container *trackContainer,AW_Det_Input *trackInput)
{
    AW_Tracker_Container *container = trackContainer;
    AW_Det_Input *det_input = trackInput;

    aw_free_tracker(container);
    free(det_input->box_info);
    det_input->box_info = NULL;
    return 0;
}

static ERRORTYPE sampleFaceTrackDetectInit(void *para)
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

    NPU_DETECT_INIT(pConfig->nbg, pConfig->detWidth, pConfig->detHeight, &pData->detContainer, &pData->detOutputs);

    pthread_mutex_init(&pData->trackerDataLock, NULL);
    OBJECT_TRACK_INIT(&pData->trackContainer, &pData->trackInput, &pData->trackOutput);

    _CHECK_RET(ret);


    return ret;
}

#if 1
static int nv12_draw_point(unsigned char* yBuffer, unsigned char* uvBuffer, int x, int y, int yColor, int uColor, int vColor) {
    if (x < 0 || x >= NPU_DET_PIC_WIDTH) return -1;
    if (y < 0 || y >= NPU_DET_PIC_HEIGHT) return -1;
    yBuffer[y*NPU_DET_PIC_WIDTH+x] = yColor;
    uvBuffer[(y/2)*NPU_DET_PIC_WIDTH+x/2*2] = uColor;
    uvBuffer[(y/2)*NPU_DET_PIC_WIDTH+x/2*2+1] = vColor;
    return 0;
}

static int nv12_draw_rect(unsigned char* yBuffer, unsigned char* uvBuffer, int left, int top, int right, int bottom, int yColor, int uColor, int vColor) {
    int i;
    for (i = left; i <= right; i++) {
        nv12_draw_point(yBuffer, uvBuffer, i, top, yColor, uColor, vColor);
        nv12_draw_point(yBuffer, uvBuffer, i, bottom, yColor, uColor, vColor);
    }
    for (i = top; i <= bottom; i++) {
        nv12_draw_point(yBuffer, uvBuffer, left, i, yColor, uColor, vColor);
        nv12_draw_point(yBuffer, uvBuffer, right, i, yColor, uColor, vColor);
    }
    return 0;
}
#endif

#define DETECT_FRAME_TIME_DEBUG 0
unsigned long detectFrames = 0;
unsigned long detectFramesTimeSum = 0;
unsigned long trackFrames = 0;
unsigned long trackFramesTimeSum = 0;

static FILE* testFp = NULL;
static ERRORTYPE sampleFaceTrackDetectRun(void *para)
{
    ERRORTYPE ret;
    unsigned char *buffer;
    MPP_CHN_S npuViChn;
    DetectPrivateData *pData = (DetectPrivateData *)para;
    DetectParaConfig *pConfig = (DetectParaConfig *)pData->config;

    buffer = pData->buffer;
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

    long start = getCurrentTime();

    if (pData->detectCallback) {
        pData->detectCallback(buffer, &pData->detContainer, &pData->detOutputs);
    }

#if 0
    AWF_Det_Outputs *outputs = &pData->detOutputs;
    int j = 0;
    int x0, y0, x1, y1;
    unsigned char* yBuffer = frameInfo.VFrame.mpVirAddr[0];
    unsigned char* uBuffer = frameInfo.VFrame.mpVirAddr[1];
    for (j = 0; j < outputs->num; j++) {
        x0 = outputs->faces[j].bbox.tl_x;
        y0 = outputs->faces[j].bbox.tl_y;
        x1 = outputs->faces[j].bbox.br_x;
        y1 = outputs->faces[j].bbox.br_y;
        nv12_draw_rect(yBuffer, uBuffer, x0, y0, x1, y1, 0x96, 0x2C, 0x15);
    }
    if (!testFp) testFp = fopen("./test.yuv", "wb");
    if (testFp) {
        fwrite(yBuffer, VI_NPU_PIC_WIDTH * VI_NPU_PIC_HEIGHT, 1, testFp);
        fwrite(uBuffer, VI_NPU_PIC_WIDTH * VI_NPU_PIC_HEIGHT / 2, 1, testFp);
        //fclose(testFp);
    }
#endif
    long end = getCurrentTime();
    long time = end - start;

    detectFrames++;
    detectFramesTimeSum += time;
    if (DETECT_FRAME_TIME_DEBUG && detectFrames == 300) {
        alogd("Face detect time per frame: %ld\n",detectFramesTimeSum/300);
        detectFrames = 0;
        detectFramesTimeSum = 0;
    }

    start = getCurrentTime();
    pthread_mutex_lock(&pData->trackerDataLock);
    if (pData->trackCallback) {
        pData->trackCallback(&pData->detOutputs, &pData->trackContainer, &pData->trackInput, &pData->trackOutput);
    }
#if 0
    static FILE* testFp0 = NULL;
    if (!testFp0) testFp0 = fopen("./test_640_320.yuv", "wb");
    int j = 0;
    int x, y, w, h;
    unsigned char* yBuffer = frameInfo.VFrame.mpVirAddr[0];
    unsigned char* uBuffer = frameInfo.VFrame.mpVirAddr[1];
    for (j = 0; j < trackerOutput.num; j++) {
        x = trackerOutput.box_info[j].box.tl_x; //* DST_WIDTH / NPU_DET_PIC_WIDTH;
        y = trackerOutput.box_info[j].box.tl_y; //* DST_HEIGHT / NPU_DET_PIC_HEIGHT;
        w = trackerOutput.box_info[j].box.width; //* DST_WIDTH / NPU_DET_PIC_WIDTH;
        h = trackerOutput.box_info[j].box.height; //* DST_HEIGHT / NPU_DET_PIC_HEIGHT;;
        printf("%s,line:%d,x:%d,y:%d,w:%d,h:%d\n",__func__,__LINE__, x, y, w, h);
        //        MPP_REGION_CREATE_RECT(&streamProcessViChn, &pStRegion[j], &pStRgnChnAttr[j], j, x, y, w, h);
        nv12_draw_rect(yBuffer, uBuffer, x, y, x+w, y+h, 0x96, 0x2C, 0x15);

        static FILE* testFp = NULL;
        if (!testFp) testFp = fopen("./test.yuv", "wb");
        if (testFp) {
            fwrite(yBuffer, VI_NPU_PIC_WIDTH * VI_NPU_PIC_HEIGHT, 1, testFp);
            fwrite(uBuffer, VI_NPU_PIC_WIDTH * VI_NPU_PIC_HEIGHT / 2, 1, testFp);
            //fclose(testFp);
        }

    }
#endif
    pthread_mutex_unlock(&pData->trackerDataLock);
    end = getCurrentTime();
    time = end - start;
    trackFrames++;
    trackFramesTimeSum += time;
    if (DETECT_FRAME_TIME_DEBUG && trackFrames == 300) {
        alogd("track time per frame: %ld\n",trackFramesTimeSum/300);
        trackFrames = 0;
        trackFramesTimeSum = 0;
    }

    ret = MPP_VI_RELEASE_FRAME(&npuViChn, &frameInfo);
    _CHECK_RET(ret);

    return ret;
}

static int NPU_DETECT_UNINIT(AWF_Det_Container *detContainer, AWF_Det_Outputs *detOutputs)
{
    AWF_Det_Container *container = detContainer;
    AWF_Det_Outputs *outputs = detOutputs;
    awf_free_det_container(container, outputs);
    return 0;
}

static ERRORTYPE sampleFaceTrackDetectUnInit(void *para)
{
    ERRORTYPE ret;
   // unsigned char **buffers;
    unsigned char *buffer;
    MPP_CHN_S npuViChn;

    DetectPrivateData *pData = (DetectPrivateData *)para;
    DetectParaConfig *pConfig = (DetectParaConfig *)pData->config;

    buffer = pData->buffer;
    memcpy(&npuViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    pthread_mutex_destroy(&pData->trackerDataLock);
    NPU_DETECT_UNINIT(&pData->detContainer, &pData->detOutputs);

    OBJECT_TRACK_UNINIT(&pData->trackContainer, &pData->trackInput);

    ret = MPP_VI_STOP(&npuViChn);
    _CHECK_RET(ret);

    ret = MPP_VI_UNINIT(&npuViChn);
    _CHECK_RET(ret);


    return ret;
}

static void *sampleFaceTrackDetectTask(void *para)
{
    ERRORTYPE ret;

    DetectPrivateData *pData = (DetectPrivateData *)para;

    ret = sampleFaceTrackDetectInit(pData);
    if (ret) {
        aloge("Fatal error! sampleFaceTrackDetectInit failed (ret:%d)\n", ret);
        return NULL;
    }

    while (!gStop) {
        ret = sampleFaceTrackDetectRun(pData);
        if (ret)  {
            aloge("Fatal error! sampleFaceTrackDetectRun failed (ret:%d)\n", ret);
            break;
        }
    }

    ret = sampleFaceTrackDetectUnInit(pData);
    if (ret)
        aloge("Fatal error! sampleFaceTrackDetectUnInit failed (ret:%d)\n", ret);

    return NULL;
}


VencHeaderData gVencHeader;
#if 0
ERRORTYPE sampleFaceTrackStreamProcessInit(void *para)
{
    ERRORTYPE ret;
    StreamProcessPrivateData *pData = (StreamProcessPrivateData *)para;
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;
    MPP_CHN_S streamProcessViChn;
    MPP_CHN_S streamProcessVeChn;

    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));


    PIXEL_FORMAT_E format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    ret = MPP_VI_INIT(&streamProcessViChn, pConfig->viWidth, pConfig->viHeight, pConfig->viFps, format);
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
#else
#include "media/mm_comm_vi.h"
ERRORTYPE MPP_VI_INIT2(MPP_CHN_S *viChn, int width, int height, int fps, PIXEL_FORMAT_E format)
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

   // ret = AW_MPI_ISP_Run(MPP_HELPER_ISP_DEV);
    sleep(1);
    ret = AW_MPI_ISP_Run(0);
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

ERRORTYPE sampleFaceTrackStreamProcessInit(void *para)
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
#endif

ERRORTYPE sampleFaceTrackStreamProcessRun(void *para)
{
    ERRORTYPE ret;
    MPP_CHN_S streamProcessVeChn;
    MPP_CHN_S streamProcessViChn;
    AW_Tracker_Output *trackerOutput;

    StreamProcessPrivateData *pData = (StreamProcessPrivateData *)para;
    StreamProcessParaConfig *pConfig = (StreamProcessParaConfig *)pData->config;

    memcpy(&streamProcessVeChn, &(pConfig->veChn), sizeof(MPP_CHN_S));
    memcpy(&streamProcessViChn, &(pConfig->viChn), sizeof(MPP_CHN_S));

    DetectPrivateData *pDetectData =  &(gSampleFaceTrackContext.privateData.detectData);

    pthread_mutex_lock(&pDetectData->trackerDataLock);
    if (pDetectData->trackOutput.num != 0) {
        gTrackOutput.num = pDetectData->trackOutput.num;
        memcpy(gTrackOutput.box_info, pDetectData->trackOutput.box_info, gTrackOutput.num * sizeof(AW_Box_Base));
    } else
        gTrackOutput.num = 0;
    pthread_mutex_unlock(&pDetectData->trackerDataLock);
    trackerOutput = &gTrackOutput;

    if (pData->createRect) {
        pData->createRect(trackerOutput, stRegion, stRgnChnAttr, MPP_REGION_RECT_NUM);
    }

    /* Get vin stream, add track result and send into ven */
    VIDEO_FRAME_INFO_S frameInfo;
    ret = MPP_VI_GET_FRAME(&streamProcessViChn, &frameInfo);
    _CHECK_RET(ret);

    if (pData->destoryRect) {
        pData->destoryRect(trackerOutput, MPP_REGION_RECT_NUM);
    }

    if (pData->streamProcessCallback) {
        pData->streamProcessCallback(frameInfo.VFrame.mpVirAddr[0], frameInfo.VFrame.mpVirAddr[1], frameInfo.VFrame.mpVirAddr[2]);
    }

    if (pData->createLabel) {
        pData->createLabel(trackerOutput, stRegion, stRgnChnAttr, MPP_REGION_RECT_NUM);
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

    if (pData->destoryLabel) {
        pData->destoryLabel(trackerOutput, MPP_REGION_RECT_NUM);
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

ERRORTYPE sampleFaceTrackStreamProcessUnInit(void *para)
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

#define sampleFaceTrackRecordFile "motion.h264"
static void *sampleFaceTrackStreamProcessTask(void *para)
{
    ERRORTYPE ret;
    int i;

    StreamProcessPrivateData *pData = (StreamProcessPrivateData *)para;
    pData->recordFp = fopen(sampleFaceTrackRecordFile, "wb+");

    ret = sampleFaceTrackStreamProcessInit(pData);
    if (ret) {
        aloge("sampleFaceTrackStreamProcessInit failed (ret:%d)\n", ret);
        goto RecordInitERR;
    }

    while (!gStop) {
        ret = sampleFaceTrackStreamProcessRun(pData);
        if (ret) {
            aloge("sampleFaceTrackStreamProcessRun failed (ret:%d)\n", ret);
            break;
        }
    }
  //  if (testFp != NULL)
  //  fclose(testFp);

RecordInitERR:
    if (pData->recordFp)
        fclose(pData->recordFp);

    ret = sampleFaceTrackStreamProcessUnInit(pData);
    if (ret)
        aloge("sampleFaceTrackStreamProcessUnInit failed (ret:%d)\n", ret);

    return NULL;
}

static int sampleFaceTrackStart()
{
    int ret;

    gStop = 0;
    DetectPrivateData *pDetectData;
    pDetectData = &(gSampleFaceTrackContext.privateData.detectData);
    ret = pthread_create(&gDetectThread, NULL, sampleFaceTrackDetectTask, pDetectData);
    if (ret != 0) {
        aloge("Fatal error! Failed to create thread for detect.\n");
        return ret;
    }
    StreamProcessPrivateData *pStreamProcessData;
    pStreamProcessData = &(gSampleFaceTrackContext.privateData.streamProcessData);
    ret = pthread_create(&gStreamProcessThread, NULL, sampleFaceTrackStreamProcessTask, pStreamProcessData);
    if (ret != 0) {
        aloge("Fatal error! Failed to create thread for preview.\n");
        return ret;
    }

    UvcOutPrivateData *pUvcOutData;
    pUvcOutData = &(gSampleFaceTrackContext.privateData.uvcOutData);
    ret = pthread_create(&gUvcOutThread, NULL, sampleFaceTrackUvcOutTask, pUvcOutData);
    if (ret != 0) {
        aloge("Fatal error! Failed to create thread for preview.\n");
        return ret;
    }

    return ret;
}

static int sampleFaceTrackStop()
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
    ret = ParseCmdLine(argc, argv, &gSampleFaceTrackContext.cmdLinePara);
    if(ret) {
        aloge("Fatal error! command line param is wrong, exit!");
        return ret;
    }

    if(strlen(gSampleFaceTrackContext.cmdLinePara.configFilePath) > 0) {
        pConfigFilePath = gSampleFaceTrackContext.cmdLinePara.configFilePath;
    } else {
        pConfigFilePath = NULL;
    }

    ret = loadSampleFaceTrackConfig(&gSampleFaceTrackContext, pConfigFilePath);
    if(ret) {
        aloge("Fatal error! no config file or parse conf file fail");
        return ret;
    }

    ret = sampleFaceTrackPrivateDataSet(&gSampleFaceTrackContext);
    if(ret) {
        aloge("Fatal error! no config file or parse conf file fail");
        return ret;
    }

    ret = sampleFaceTrackMPPInit();
    if (ret) {
        aloge("Fatal error! sample_RegionDetect MPP Init Failed. \n");
        return ret;
    }

    ret = sampleFaceTrackStart();
    if (ret) {
        aloge("Fatal error! sample_RegionDetect start Failed. \n");
        return ret;
    }

    signal(SIGINT, sampleFaceTrackRecvSignal);
    while (gKeepRunning) {
        sleep(1);
    }

    ret = sampleFaceTrackStop();
    if (ret)
        aloge("Fatal error! sample_RegionDetect stop Failed. \n");

    ret = sampleFaceTrackMPPUnInit();
    if (ret)
        aloge("Fatal error! sample_RegionDetect MPP UnInit Failed. \n");

    return ret;
}
