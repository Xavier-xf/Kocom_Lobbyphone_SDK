#include <utils/plat_log.h>
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
#include <sys/prctl.h>
#include "media/mm_comm_vi.h"
#include "media/mm_comm_region.h"
#include "media/mpi_vi.h"
#include "vo/hwdisplay.h"
#include "log/log_wrapper.h"
#include <ClockCompPortIndex.h>
#include <mpi_videoformat_conversion.h>
#include <utils/VIDEO_FRAME_INFO_S.h>
#include <confparser.h>
#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_isp.h>
#include <mm_comm_venc.h>
#include <mpi_venc.h>
#include <mpi_region.h>
#include <utils/plat_log.h>
#include "sample_facekit_demo.h"
#include "sample_facekit_demo_config.h"
#include "ai_facekit.h"
#include <vip_lite.h>

#define _REGION_ENABLE_ 1
#define ISP_RUN 1
#define _FRAME_RATE_ENABLE_ 1
//#define _TIME_CONSUMING_ 1

SampleFacekitContext *gpSampleFacekitContext = NULL;
AiContext *gpAiContext= NULL;

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleFacekitContext)
    {
        cdx_sem_up(&gpSampleFacekitContext->mSemExit);
    }
}

static int ParseCmdLine(int argc, char **argv, SampleFacekitCmdLineParam *pCmdLinePara)
{
    alogd("path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleFacekitCmdLineParam));
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                aloge("fatal error! use -h to learn how to set parameter!!!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                aloge("fatal error! file path[%s] too long: [%d]>=[%d]!", argv[i], strlen(argv[i]), MAX_FILE_PATH_SIZE);
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], MAX_FILE_PATH_SIZE-1);
            pCmdLinePara->mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
            alogd("CmdLine param:\n"
                "\t-path /home/sample_OnlineVenc.conf\n");
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

static PIXEL_FORMAT_E getPicFormatFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PIXEL_FORMAT_E PicFormat = MM_PIXEL_FORMAT_BUTT;
    char *pStrPixelFormat = (char*)GetConfParaString(pConfParser, key, NULL);

    if (!strcmp(pStrPixelFormat, "nv21"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "yv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "nv12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "yu12"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_2_0x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_2_5x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_1_5x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
    }
    else if (!strcmp(pStrPixelFormat, "aw_lbc_1_0x"))
    {
        PicFormat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
    }
    else
    {
        aloge("fatal error! conf file pic_format is [%s]?", pStrPixelFormat);
        PicFormat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }

    return PicFormat;
}

static void LoadVIPP2VOConfig(int idx, VIPP2VOConfig *pVIPP2VOConfig, CONFPARSER_S *pConfParser)
{
    if(0 == idx)
    {
        pVIPP2VOConfig->mIspDev        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_ISP_DEV, 0);
        pVIPP2VOConfig->mVippDev        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_VIPP_DEV, 0);
        pVIPP2VOConfig->mCaptureWidth  = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_CAPTURE_WIDTH, 0);
        pVIPP2VOConfig->mCaptureHeight = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_CAPTURE_HEIGHT, 0);
        pVIPP2VOConfig->mDisplayX   = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_DISPLAY_X, 0);
        pVIPP2VOConfig->mDisplayY   = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_DISPLAY_Y, 0);
        pVIPP2VOConfig->mDisplayWidth  = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_DISPLAY_WIDTH, 0);
        pVIPP2VOConfig->mDisplayHeight = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_DISPLAY_HEIGHT, 0);
        pVIPP2VOConfig->mLayerNum = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_LAYER_NUM, 0);
		pVIPP2VOConfig->mFrameRate = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_FRAME_RATE, 0);
		pVIPP2VOConfig->mPicFormat = getPicFormatFromConfig(pConfParser, SAMPLE_FACEKIT_KEY_PIC_FORMAT);

    }
    else if(1 == idx)
    {
        pVIPP2VOConfig->mIspDev        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_ISP_DEV2, 0);
        pVIPP2VOConfig->mVippDev        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_VIPP_DEV2, 0);
        pVIPP2VOConfig->mCaptureWidth  = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_CAPTURE_WIDTH2, 0);
        pVIPP2VOConfig->mCaptureHeight = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_CAPTURE_HEIGHT2, 0);
        pVIPP2VOConfig->mDisplayX   = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_DISPLAY_X2, 0);
        pVIPP2VOConfig->mDisplayY   = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_DISPLAY_Y2, 0);
        pVIPP2VOConfig->mDisplayWidth  = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_DISPLAY_WIDTH2, 0);
        pVIPP2VOConfig->mDisplayHeight = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_DISPLAY_HEIGHT2, 0);
        pVIPP2VOConfig->mLayerNum = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_LAYER_NUM2, 0);
		pVIPP2VOConfig->mFrameRate = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_FRAME_RATE2, 0);
		pVIPP2VOConfig->mPicFormat = getPicFormatFromConfig(pConfParser, SAMPLE_FACEKIT_KEY_PIC_FORMAT2);
    }
    else
    {
        aloge("fatal error! need add vipp[%d] config!", idx);
    }
}

static void LoadAiServiceInfoItemConfig(int idx, AiServiceInfoItemConfig *pAiServiceItemInfo, CONFPARSER_S *pConfParser)
{
	char *ptr = NULL;
    if(0 == idx)
    {
        pAiServiceItemInfo->mIspDev        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_ISP_DEV, 0);
        pAiServiceItemInfo->mVippDev        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_VIPP_DEV, 0);
        pAiServiceItemInfo->mCaptureWidth  = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_CAPTURE_WIDTH, 0);
        pAiServiceItemInfo->mCaptureHeight = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_CAPTURE_HEIGHT, 0);
		pAiServiceItemInfo->mSrcFrameRate = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_FRAME_RATE, 0);
		pAiServiceItemInfo->mPicFormat = getPicFormatFromConfig(pConfParser, SAMPLE_FACEKIT_KEY_NPU_PIC_FORMAT);
		ptr = (char*)GetConfParaString(pConfParser, SAMPLE_FACEKIT_KEY_NPU_MODE_FILE, NULL);
        strncpy(pAiServiceItemInfo->mModelFile, ptr, MAX_FILE_PATH_SIZE);
    }
    else if(1 == idx)
    {
        pAiServiceItemInfo->mIspDev        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_ISP_DEV_1, 0);
        pAiServiceItemInfo->mVippDev        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_VIPP_DEV_1, 0);
        pAiServiceItemInfo->mCaptureWidth  = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_CAPTURE_WIDTH_1, 0);
        pAiServiceItemInfo->mCaptureHeight = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_CAPTURE_HEIGHT_1, 0);
		pAiServiceItemInfo->mSrcFrameRate = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_FRAME_RATE_1, 0);
		pAiServiceItemInfo->mPicFormat = getPicFormatFromConfig(pConfParser, SAMPLE_FACEKIT_KEY_NPU_PIC_FORMAT_1);
		ptr = (char*)GetConfParaString(pConfParser, SAMPLE_FACEKIT_KEY_NPU_MODE_FILE_1, NULL);
        strncpy(pAiServiceItemInfo->mModelFile, ptr, MAX_FILE_PATH_SIZE);
    }
    else
    {
        aloge("fatal error! need add vipp[%d] config!", idx);
    }
}

static void LoadAiServiceInfoConfig(int idx, AiServiceInfo *AiServiceInfo, CONFPARSER_S *pConfParser)
{
    if(0 == idx)
    {
        AiServiceInfo->mIspDev              = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_ISP_DEV, 0);
        AiServiceInfo->mChIdx               = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_VIPP_DEV, 0);
        AiServiceInfo->mCaptureHeight       = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_CAPTURE_HEIGHT, 0);
        AiServiceInfo->mCaptureWidth        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_CAPTURE_WIDTH, 0);
    }
    else if(1 == idx)
    {
        AiServiceInfo->mIspDev              = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_ISP_DEV_1, 0);
        AiServiceInfo->mChIdx               = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_VIPP_DEV_1, 0);
        AiServiceInfo->mCaptureHeight       = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_CAPTURE_HEIGHT_1, 0);
        AiServiceInfo->mCaptureWidth        = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_NPU_CAPTURE_WIDTH_1, 0);
    }
    else
    {
        aloge("fatal error! need add vipp[%d] config!", idx);
    }
}

static void LoadAiPersonInfoConfig(PersonInfo *pPersonInfo, CONFPARSER_S *pConfParser)
{
	int i = 0;
	char *ptr = NULL;
	for(i=0;i<MAX_PERSON_NUM;i++)
	{
		if(i==0)
		{
		    ptr = (char*)GetConfParaString(pConfParser, SAMPLE_FACEKIT_KEY_PERSON_RGB_FILE1, NULL);
        	strncpy(pPersonInfo->mPesonInfos[i].mPersonRGBFile, ptr, MAX_FILE_PATH_SIZE);
            ptr = (char*)GetConfParaString(pConfParser, SAMPLE_FACEKIT_KEY_PERSON_IR_FILE1, NULL);
        	strncpy(pPersonInfo->mPesonInfos[i].mPersonIRFile, ptr, MAX_FILE_PATH_SIZE);
			ptr = (char*)GetConfParaString(pConfParser, SAMPLE_FACEKIT_KEY_PERSON_NAME1, NULL);
        	strncpy(pPersonInfo->mPesonInfos[i].mPersonName, ptr, MAX_FILE_PATH_SIZE);
			pPersonInfo->mPesonInfos[i].mPersonId = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_PERSON_ID1, 0);
			pPersonInfo->mPesonInfos[i].mWidht = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_PERSON_WIDTH1, 0);
			pPersonInfo->mPesonInfos[i].mHeight = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_PERSON_HEIGHT1, 0);
			pPersonInfo->mPesonInfos[i].mVaild = 1;
			pPersonInfo->mValidNum++;
		}
		// else if(i==1)
		// {
		//     ptr = (char*)GetConfParaString(pConfParser, SAMPLE_FACEKIT_KEY_PERSON_FILE2, NULL);
        // 	strncpy(pPersonInfo->mPesonInfos[i].mPersonFile, ptr, MAX_FILE_PATH_SIZE);
		// 	ptr = (char*)GetConfParaString(pConfParser, SAMPLE_FACEKIT_KEY_PERSON_NAME2, NULL);
        // 	strncpy(pPersonInfo->mPesonInfos[i].mPersonName, ptr, MAX_FILE_PATH_SIZE);
		// 	pPersonInfo->mPesonInfos[i].mPersonId = GetConfParaInt(pConfParser, SAMPLE_FACEKIT_KEY_PERSON_ID2, 0);
		// 	pPersonInfo->mPesonInfos[i].mVaild = 1;
		// 	pPersonInfo->mValidNum++;
		// }
		else
		{
			aloge("fatal error! need add vipp[%d] config!", i);
			break;
		}
	}
	pPersonInfo->mToatalNum = MAX_PERSON_NUM;
	alogd("mValidNum:%d,mToatalNum:%d\n",pPersonInfo->mValidNum,pPersonInfo->mToatalNum);
}


static ERRORTYPE loadSampleFacekitConfig(SampleFacekitConfig *pConfig, AiServiceInfoConfig *pAiServiceInfoConfig,const char *conf_path)
{
    int ret;
    char *ptr;
	int i;
	CONFPARSER_S stConfParser;

    ret = createConfParser(conf_path, &stConfParser);
    if(ret < 0)
    {
        aloge("load conf fail");
        return FAILURE;
    }
    memset(pConfig, 0, sizeof(SampleFacekitConfig));

    for(i=0;i<VIPP2VO_NUM;i++)
    {
        LoadVIPP2VOConfig(i, &pConfig->mVIPP2VOConfigArray[i], &stConfParser);
    }


	for(i=0;i<NN_CHN_NUM_MAX;i++)
	{
	 	LoadAiServiceInfoItemConfig(i, &pAiServiceInfoConfig->mAiServiceInfoConifgArray[i], &stConfParser);
	}

    for(i=0;i<NN_CHN_NUM_MAX;i++)
    {
        printf("************************************\n");
        printf("Array[%d].mCaptureWidth = %d\n",i,pAiServiceInfoConfig->mAiServiceInfoConifgArray[i].mCaptureWidth);
        printf("Array[%d].mCaptureHeight = %d\n",i,pAiServiceInfoConfig->mAiServiceInfoConifgArray[i].mCaptureHeight);
        printf("Array[%d].mIspDev = %d\n",i,pAiServiceInfoConfig->mAiServiceInfoConifgArray[i].mIspDev);
        printf("Array[%d].mVippDev = %d\n",i,pAiServiceInfoConfig->mAiServiceInfoConifgArray[i].mVippDev);
        printf("************************************\n");
    }

	LoadAiPersonInfoConfig(&pAiServiceInfoConfig->mPersonInfo,&stConfParser);


    char *pStrDispType = (char*)GetConfParaString(&stConfParser, SAMPLE_FACEKIT_KEY_DISP_TYPE, NULL);
    if (!strcmp(pStrDispType, "hdmi"))
    {
        pConfig->mDispType = VO_INTF_HDMI;
        if (pConfig->mVIPP2VOConfigArray[0].mDisplayWidth > 1920)
            pConfig->mDispSync = VO_OUTPUT_3840x2160_30;
        else if (pConfig->mVIPP2VOConfigArray[0].mDisplayWidth > 1280)
            pConfig->mDispSync = VO_OUTPUT_1080P30;
        else
            pConfig->mDispSync = VO_OUTPUT_720P60;
    }
    else if (!strcmp(pStrDispType, "lcd"))
    {
        pConfig->mDispType = VO_INTF_LCD;
        pConfig->mDispSync = VO_OUTPUT_NTSC;
    }
    else if (!strcmp(pStrDispType, "cvbs"))
    {
        pConfig->mDispType = VO_INTF_CVBS;
        pConfig->mDispSync = VO_OUTPUT_NTSC;
    }


    pConfig->mTestDuration = GetConfParaInt(&stConfParser, SAMPLE_FACEKIT_KEY_TEST_DURATION, 0);

    for(i=0;i<VIPP2VO_NUM;i++)
    {
        alogd("vipp[%d]: captureSize[%dx%d], displayArea[%d,%d,%dx%d],layer[%d]",
            pConfig->mVIPP2VOConfigArray[i].mVippDev, pConfig->mVIPP2VOConfigArray[i].mCaptureWidth, pConfig->mVIPP2VOConfigArray[i].mCaptureHeight,
            pConfig->mVIPP2VOConfigArray[i].mDisplayX, pConfig->mVIPP2VOConfigArray[i].mDisplayY, pConfig->mVIPP2VOConfigArray[i].mDisplayWidth, pConfig->mVIPP2VOConfigArray[i].mDisplayHeight,pConfig->mVIPP2VOConfigArray[i].mLayerNum);
    }
    alogd("dispSync[%d], dispType[%d],testDuration[%d]", pConfig->mDispSync, pConfig->mDispType, pConfig->mTestDuration);

    destroyConfParser(&stConfParser);
    return SUCCESS;
}

static ERRORTYPE SampleFacekit_VOCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    ERRORTYPE ret = SUCCESS;
    SampleFacekitContext *pContext = (SampleFacekitContext*)cookie;
    if(MOD_ID_VOU == pChn->mModId)
    {
        alogd("VO callback: VO Layer[%d] chn[%d] event:%d", pChn->mDevId, pChn->mChnId, event);
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                VIDEO_FRAME_INFO_S *pFrameInfo = (VIDEO_FRAME_INFO_S*)pEventData;
                aloge("vo layer[%d] release frame id[0x%x]!", pChn->mDevId, pFrameInfo->mId);
                break;
            }
            case MPP_EVENT_SET_VIDEO_SIZE:
            {
                SIZE_S *pDisplaySize = (SIZE_S*)pEventData;
                alogd("vo layer[%d] report video display size[%dx%d]", pChn->mDevId, pDisplaySize->Width, pDisplaySize->Height);
                break;
            }
            case MPP_EVENT_RENDERING_START:
            {
                alogd("vo layer[%d] report rendering start", pChn->mDevId);
                break;
            }
            default:
            {
                //postEventFromNative(this, event, 0, 0, pEventData);
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                ret = ERR_VO_ILLEGAL_PARAM;
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! why modId[0x%x]?", pChn->mModId);
        ret = FAILURE;
    }
    return ret;
}

static ERRORTYPE SampleFaceKit_MPPCallback(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleFacekitContext *pContext = (SampleFacekitContext*)cookie;
    if(MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                alogd("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! unknown event[0x%x] from channel[0x%x][0x%x][0x%x]!", event, pChn->mModId, pChn->mDevId, pChn->mChnId);
    }
    return SUCCESS;
}



static ERRORTYPE CreateVIPP2VOLink(int idx, SampleFacekitContext *pContext)
{
    ERRORTYPE result = SUCCESS;
    VIPP2VOConfig *pConfig = &pContext->mConfigPara.mVIPP2VOConfigArray[idx];
    VIPP2VOLinkInfo *pLinkInfo = &pContext->mLinkInfoArray[idx];
    if(0 == pConfig->mCaptureWidth || 0 == pConfig->mCaptureHeight)
    {
        alogd("do not need create link for idx[%d]", idx);
        return result;
    }
    /* create vi channel */
	pLinkInfo->mVIChn = 0;
    pLinkInfo->mVIDev = pConfig->mVippDev;
    pLinkInfo->mCaptureWidth = pConfig->mCaptureWidth;
	pLinkInfo->mCaptureHeight = pConfig->mCaptureHeight;
	pLinkInfo->mIspDev = pConfig->mIspDev;
	pLinkInfo->mFrameRate = pConfig->mFrameRate;
	pLinkInfo->mPicFormat = pConfig->mPicFormat;

    alogd("Vipp dev[%d] vir_chn[%d]", pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    ERRORTYPE eRet = AW_MPI_VI_CreateVipp(pLinkInfo->mVIDev);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp:%d failed", pLinkInfo->mVIDev);
        result = FAILURE;
        goto _err0;
    }
    MPPCallbackInfo cbInfo;
    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleFaceKit_MPPCallback;
    eRet = AW_MPI_VI_RegisterCallback(pLinkInfo->mVIDev, &cbInfo);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! vipp[%d] RegisterCallback failed", pLinkInfo->mVIDev);
    }
    VI_ATTR_S stAttr;
    eRet = AW_MPI_VI_GetVippAttr(pLinkInfo->mVIDev, &stAttr);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI GetVippAttr failed");
    }
    memset(&stAttr, 0, sizeof(VI_ATTR_S));
    stAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    stAttr.memtype = V4L2_MEMORY_MMAP;
    stAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pLinkInfo->mPicFormat);
    stAttr.format.field = V4L2_FIELD_NONE;
    stAttr.format.colorspace = V4L2_COLORSPACE_JPEG;
    stAttr.format.width = pLinkInfo->mCaptureWidth;
    stAttr.format.height =pLinkInfo->mCaptureHeight;
    stAttr.nbufs = 5;//5;
    stAttr.nplanes = 2;
    stAttr.drop_frame_num = 0; // drop 2 second video data, default=0
    stAttr.mbEncppEnable = TRUE;
    /* do not use current param, if set to 1, all this configuration will
     * not be used.
     */
    if(0 == idx)
    {
        stAttr.use_current_win = 0;
    }
    if(1 == idx)
    {
        stAttr.use_current_win = 0;
    }
    else
    {
        stAttr.use_current_win = 1;
    }
    stAttr.fps = pLinkInfo->mFrameRate;
    eRet = AW_MPI_VI_SetVippAttr(pLinkInfo->mVIDev, &stAttr);
    if (eRet != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr:%d failed", pLinkInfo->mVIDev);
    }
#if ISP_RUN
    /* open isp */
    AW_MPI_ISP_Run(pLinkInfo->mIspDev);
#endif
    eRet = AW_MPI_VI_CreateVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn, NULL);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!", pLinkInfo->mVIChn);
    }
    eRet = AW_MPI_VI_EnableVipp(pLinkInfo->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!");
        result = FAILURE;
        goto _err1;
    }
#if ISP_RUN
    //if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations must be after stream_on.
    AW_MPI_VI_SetVippMirror(pLinkInfo->mVIDev, 0);//0,1
    AW_MPI_VI_SetVippFlip(pLinkInfo->mVIDev, 0);//0,1
#endif

    /* enable vo layer */
    int hlay0 = 0;
    /*int hwDispChn = 0;
    while(hlay0 < VO_MAX_LAYER_NUM)
    {
        if(SUCCESS == AW_MPI_VO_EnableVideoLayer(hlay0))
        {
            break;
        }
        hwDispChn++;
        hlay0 = HLAY(hwDispChn, 0);
    }
    if(hlay0 >= VO_MAX_LAYER_NUM)
    {
        aloge("fatal error! enable video layer fail!");
    }*/
    hlay0 = pConfig->mLayerNum;
    if(SUCCESS != AW_MPI_VO_EnableVideoLayer(hlay0))
    {
        aloge("fatal error! enable video layer[%d] fail!", hlay0);
        hlay0 = MM_INVALID_LAYER;
        pLinkInfo->mVoLayer = hlay0;
        result = FAILURE;
        goto _err1_5;
    }
    pLinkInfo->mVoLayer = hlay0;
    AW_MPI_VO_GetVideoLayerAttr(pLinkInfo->mVoLayer, &pLinkInfo->mLayerAttr);
    pLinkInfo->mLayerAttr.stDispRect.X = pConfig->mDisplayX;
    pLinkInfo->mLayerAttr.stDispRect.Y = pConfig->mDisplayY;
    pLinkInfo->mLayerAttr.stDispRect.Width = pConfig->mDisplayWidth;
    pLinkInfo->mLayerAttr.stDispRect.Height = pConfig->mDisplayHeight;
    AW_MPI_VO_SetVideoLayerAttr(pLinkInfo->mVoLayer, &pLinkInfo->mLayerAttr);

    /* create vo channel and clock channel.
    (because frame information has 'pts', there is no need clock channel now)
    */
    BOOL bSuccessFlag = FALSE;
    pLinkInfo->mVOChn = 0;
    while(pLinkInfo->mVOChn < VO_MAX_CHN_NUM)
    {
        eRet = AW_MPI_VO_CreateChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);
        if(SUCCESS == eRet)
        {
            bSuccessFlag = TRUE;
            alogd("create vo channel[%d] success!", pLinkInfo->mVOChn);
            break;
        }
        else if(ERR_VO_CHN_NOT_DISABLE == eRet)
        {
            alogd("vo channel[%d] is exist, find next!", pLinkInfo->mVOChn);
            pLinkInfo->mVOChn++;
        }
        else
        {
            aloge("fatal error! create vo channel[%d] ret[0x%x]!", pLinkInfo->mVOChn, eRet);
            break;
        }
    }
    if(FALSE == bSuccessFlag)
    {
        pLinkInfo->mVOChn = MM_INVALID_CHN;
        aloge("fatal error! create vo channel fail!");
        result = FAILURE;
        goto _err2;
    }

    cbInfo.cookie = (void*)pContext;
    cbInfo.callback = (MPPCallbackFuncType)&SampleFacekit_VOCallbackWrapper;
    AW_MPI_VO_RegisterCallback(pLinkInfo->mVoLayer, pLinkInfo->mVOChn, &cbInfo);
    AW_MPI_VO_SetChnDispBufNum(pLinkInfo->mVoLayer, pLinkInfo->mVOChn, 2);
    /* bind clock,vo, viChn
    (because frame information has 'pts', there is no need to bind clock channel now)
    */
    MPP_CHN_S VOChn = {MOD_ID_VOU, pLinkInfo->mVoLayer, pLinkInfo->mVOChn};
    MPP_CHN_S VIChn = {MOD_ID_VIU, pLinkInfo->mVIDev, pLinkInfo->mVIChn};
    AW_MPI_SYS_Bind(&VIChn, &VOChn);

    /* start vo, vi_channel. */
    eRet = AW_MPI_VI_EnableVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! enableVirChn fail!");
        result = FAILURE;
        goto _err3;
    }
    AW_MPI_VO_StartChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);

	alogd("[mVoLayer:%d,mVOChn:%d]start success!!!\n",pLinkInfo->mVoLayer,pLinkInfo->mVOChn);

    return result;

_err3:
    eRet = AW_MPI_SYS_UnBind(&VIChn, &VOChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! ViChn && VoChn SYS_UnBind fail!");
    }
    eRet = AW_MPI_VO_DestroyChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! Vo Disable Chn fail!");
    }
_err2:
    eRet = AW_MPI_VO_DisableVideoLayer(pLinkInfo->mVoLayer);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableVideoLayer fail!");
    }
_err1_5:
    eRet = AW_MPI_VI_DestroyVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VI DestoryVirChn fail!");
    }
#if ISP_RUN
    eRet = AW_MPI_ISP_Stop(pLinkInfo->mIspDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! ISP Stop fail!");
    }
#endif
_err1:
    eRet = AW_MPI_VI_DestroyVipp(pLinkInfo->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VI DestoryVipp fail!");
    }
_err0:
    return result;

}

static ERRORTYPE DestroyVIPP2VOLink(int idx, SampleFacekitContext *pContext)
{
    ERRORTYPE eRet;
    VIPP2VOConfig *pConfig = &pContext->mConfigPara.mVIPP2VOConfigArray[idx];
    VIPP2VOLinkInfo *pLinkInfo = &pContext->mLinkInfoArray[idx];
    if(0 == pConfig->mCaptureWidth || 0 == pConfig->mCaptureHeight)
    {
        alogd("do not need destroy link for idx[%d]", idx);
        return SUCCESS;
    }
    /* stop vo channel, vi channel */
    eRet = AW_MPI_VO_StopChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO StopChn fail!");
    }
    eRet = AW_MPI_VI_DisableVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VI DisableVirChn fail!");
    }
    eRet = AW_MPI_VO_DestroyChn(pLinkInfo->mVoLayer, pLinkInfo->mVOChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
    pLinkInfo->mVOChn = MM_INVALID_CHN;
    /* disable vo layer */
    eRet = AW_MPI_VO_DisableVideoLayer(pLinkInfo->mVoLayer);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
    pLinkInfo->mVoLayer = -1;
    //wait hwdisplay kernel driver processing frame buffer, must guarantee this! Then vdec can free frame buffer.
    usleep(50*1000);

    eRet = AW_MPI_VI_DestroyVirChn(pLinkInfo->mVIDev, pLinkInfo->mVIChn);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VI DestoryVirChn fail!");
    }
#if ISP_RUN
    eRet = AW_MPI_ISP_Stop(pConfig->mIspDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }

#endif
    eRet = AW_MPI_VI_DisableVipp(pLinkInfo->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
    eRet = AW_MPI_VI_DestroyVipp(pLinkInfo->mVIDev);
    if(eRet != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!");
    }
    pLinkInfo->mVIDev = MM_INVALID_DEV;
    pLinkInfo->mVIChn = MM_INVALID_CHN;
    return SUCCESS;
}


static ERRORTYPE CreateNpuVIPPLink(int idx, AiContext *pContext)
{
    ERRORTYPE result = SUCCESS;

    alogd("Enter!\n");

    if (pContext == NULL)
    {
        alogd("parameter is wrong.\n");
        result = FAILURE;
        goto _err0;
    }

    AiServiceInfoItemConfig *pcfg = &pContext->mConfigPara.mAiServiceInfoConifgArray[idx];
    AiServiceInfo *plnk = &pContext->mAiServiceInfo[idx];

	plnk->mChIdx = idx;
    plnk->mVippDev = pcfg->mVippDev;
	plnk->mIspDev = pcfg->mIspDev;
	plnk->mPixelFormat = pcfg->mPicFormat;
	plnk->mCaptureWidth = pcfg->mCaptureWidth;
	plnk->mCaptureHeight = pcfg->mCaptureHeight;
	plnk->mSrcFrameRate = pcfg->mSrcFrameRate;
    plnk->mViChn = 0;

	plnk->mDrawOrlEnable = 1;
	plnk->mDrawOrlSrcWidth = gpSampleFacekitContext->mConfigPara.mVIPP2VOConfigArray[0].mCaptureWidth;
	plnk->mDrawOrlSrcHeight = gpSampleFacekitContext->mConfigPara.mVIPP2VOConfigArray[0].mCaptureHeight;
	plnk->mDrawOrlVipp =  gpSampleFacekitContext->mConfigPara.mVIPP2VOConfigArray[0].mVippDev;
	plnk->mRegionHdlBase = 10;

    alogd("vipp[%d] vir_chn[%d] creating.\n", plnk->mVippDev, plnk->mViChn);

	alogd("mChIdx[%d] mVippDev[%d] mViChn:[%d],mPixelFormat:[%d],mCaptureWidth:[%d],mCaptureHeight:[%d],mSrcFrameRate:[%d]\n"
	,plnk->mChIdx
	,plnk->mVippDev
	,plnk->mViChn
	,plnk->mPixelFormat
	,plnk->mCaptureWidth
	,plnk->mCaptureHeight
	,plnk->mSrcFrameRate);

    // create a VIPP hardware channel.
    result = AW_MPI_VI_CreateVipp(plnk->mVippDev);
    if (result != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI CreateVipp:%d failed\n", plnk->mVippDev);
        result = FAILURE;
        goto _err0;
    }

    MPPCallbackInfo cbinfo;

    memset(&cbinfo, 0x00, sizeof(cbinfo));
    cbinfo.cookie = (void *)pContext;
    cbinfo.callback = (MPPCallbackFuncType)&SampleFaceKit_MPPCallback;
    result = AW_MPI_VI_RegisterCallback(plnk->mVippDev, &cbinfo);
    if (result != SUCCESS)
    {
        aloge("fatal error! vipp[%d] RegisterCallback failed.\n", plnk->mVippDev);
        result = FAILURE;
        goto _err1;
    }

    VI_ATTR_S vi_attr;
    memset(&vi_attr, 0x00, sizeof(vi_attr));
    result = AW_MPI_VI_GetVippAttr(plnk->mVippDev, &vi_attr);
    if (result != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI GetVippAttr failed.\n");
        result = FAILURE;
        goto _err1;
    }

    memset(&vi_attr, 0, sizeof(VI_ATTR_S));
    vi_attr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    vi_attr.memtype = V4L2_MEMORY_MMAP;
    vi_attr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(plnk->mPixelFormat);
    vi_attr.format.field = V4L2_FIELD_NONE;
    vi_attr.format.colorspace = V4L2_COLORSPACE_JPEG;
    vi_attr.format.width = plnk->mCaptureWidth;
    vi_attr.format.height = plnk->mCaptureHeight;
    vi_attr.nbufs = 5;
    vi_attr.nplanes = 2;
    vi_attr.drop_frame_num = 0; // drop 2 second video data, default=0
    vi_attr.use_current_win =0;
    vi_attr.fps = plnk->mSrcFrameRate;
    result = AW_MPI_VI_SetVippAttr(plnk->mVippDev, &vi_attr);
    if (result != SUCCESS)
    {
        aloge("fatal error! AW_MPI_VI SetVippAttr:%d failed.\n", plnk->mVippDev);
        result = FAILURE;
        goto _err1;
    }

    /* open isp */
    AW_MPI_ISP_Run(plnk->mIspDev);

    result = AW_MPI_VI_EnableVipp(plnk->mVippDev);
    if (result != SUCCESS)
    {
        aloge("fatal error! enableVipp fail!\n");
        result = FAILURE;
        goto _err1;
    }

    // Create a virtual channel on specify VIPP.
    result = AW_MPI_VI_CreateVirChn(plnk->mVippDev, plnk->mViChn, NULL);
    if (result != SUCCESS)
    {
        aloge("fatal error! createVirChn[%d] fail!\n", plnk->mViChn);
        result = FAILURE;
        goto _err1;
    }

    // if enable CONFIG_ENABLE_SENSOR_FLIP_OPTION, flip operations must be after stream_on.
    // 设置镜像和翻转
    AW_MPI_VI_SetVippMirror(plnk->mVippDev, 0);//0,1
    AW_MPI_VI_SetVippFlip(plnk->mVippDev, 0);//0,1

	result = AW_MPI_VI_EnableVirChn(plnk->mVippDev, plnk->mViChn);
    if (result != SUCCESS)
    {
        aloge("fatal error! enableVirChn fail!\n");
        result = FAILURE;
        goto _err1;
    }

	alogd("leave!\n");
	return result ;

_err1:
    result = AW_MPI_VI_DestroyVipp(plnk->mVippDev);
    if (result != SUCCESS)
    {
        aloge("fatal error! VI DestoryVipp fail!\n");
    }
_err0:
	 alogd("leave:%u!!!!\n",result);
	 return result ;


}

static int DestroyNpuVIPPLink(int idx, AiContext *pContext)
{
    ERRORTYPE result;

    alogd("enter!\n");

    if (pContext == NULL)
    {
        aloge("parameter is wrong.\n");
        return FAILURE;
    }

    AiServiceInfoItemConfig *pcfg = &pContext->mConfigPara.mAiServiceInfoConifgArray[idx];
    AiServiceInfo *plnk = &pContext->mAiServiceInfo[idx];

    result = AW_MPI_VI_DisableVirChn(plnk->mVippDev, plnk->mViChn);
    if (result != SUCCESS)
    {
        aloge("fatal error! VI DisableVirChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DestroyVirChn(plnk->mVippDev, plnk->mViChn);
    if (result != SUCCESS)
    {
        aloge("fatal error! VI DestoryVirChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_ISP_Stop(plnk->mIspDev);
    if (result != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DisableVipp(plnk->mVippDev);
    if (result != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    result = AW_MPI_VI_DestroyVipp(plnk->mVippDev);
    if (result != SUCCESS)
    {
        aloge("fatal error! VO DisableChn fail!\n");
        return FAILURE;
    }

    plnk->mVippDev = MM_INVALID_DEV;
    plnk->mViChn = MM_INVALID_CHN;

    alogd("leave!\n");
    return SUCCESS;
}


static void paint_object_detect_region_face(BBoxResults_t *res, int ch_idx)
{
    int i = 0;
    RGN_ATTR_S rgnAttr;
    RGN_CHN_ATTR_S rgnChnAttr;

	AiServiceInfo * pAiServiceInfo = &gpAiContext->mAiServiceInfo[ch_idx];

    MPP_CHN_S ViChn = {MOD_ID_VIU, pAiServiceInfo->mDrawOrlVipp, 0};

    alogv("ch_idx=%d, region hdl base=%d, vipp=%d, orl num old=%d cur=%d", ch_idx,
        gpAiContext->region_info[ch_idx].region_hdl_base, pAiServiceInfo->mDrawOrlVipp,
        gpAiContext->region_info[ch_idx].old_num_of_boxes, res->valid_cnt);

    for (i = 0; i < gpAiContext->region_info[ch_idx].old_num_of_boxes; i ++)
    {
        alogv("ch_idx=%d, detach region hdl base=%d, vipp=%d", ch_idx, i + gpAiContext->region_info[ch_idx].region_hdl_base, pAiServiceInfo->mDrawOrlVipp);
        AW_MPI_RGN_DetachFromChn(i + gpAiContext->region_info[ch_idx].region_hdl_base, &ViChn);
        AW_MPI_RGN_Destroy(i + gpAiContext->region_info[ch_idx].region_hdl_base);
    }
    gpAiContext->region_info[ch_idx].old_num_of_boxes = 0;

    for (i = 0; i < res->valid_cnt; i++)
    {
        memset(&rgnAttr, 0x00, sizeof(RGN_ATTR_S));
        rgnAttr.enType = ORL_RGN;
        AW_MPI_RGN_Create(i + gpAiContext->region_info[ch_idx].region_hdl_base, &rgnAttr);

        int left   = res->boxes[i].xmin * pAiServiceInfo->mDrawOrlSrcWidth  / pAiServiceInfo->mCaptureWidth;
        int up     = res->boxes[i].ymin * pAiServiceInfo->mDrawOrlSrcHeight  / pAiServiceInfo->mCaptureHeight;
        int right  = res->boxes[i].xmax * pAiServiceInfo->mDrawOrlSrcWidth   / pAiServiceInfo->mCaptureWidth;
        int bottom = res->boxes[i].ymax * pAiServiceInfo->mDrawOrlSrcHeight  / pAiServiceInfo->mCaptureHeight;


		alogv("left:%d,up:%d,right:%d,bottom:%d\n",left,up,right,bottom);

        memset(&rgnChnAttr, 0x00, sizeof(RGN_CHN_ATTR_S));
        rgnChnAttr.unChnAttr.stOrlChn.stRect.X        = left;
        rgnChnAttr.unChnAttr.stOrlChn.stRect.Y        = up;
        rgnChnAttr.unChnAttr.stOrlChn.stRect.Width    = right - left;
        rgnChnAttr.unChnAttr.stOrlChn.stRect.Height   = bottom - up;
        rgnChnAttr.bShow                              = TRUE;
        rgnChnAttr.enType                             = ORL_RGN;
        rgnChnAttr.unChnAttr.stOrlChn.enAreaType      = AREA_RECT;
		rgnChnAttr.unChnAttr.stOrlChn.mColor		  = res->boxes[i].match?0x00FF00:0xFF0000;
        rgnChnAttr.unChnAttr.stOrlChn.mThick          = 1;
        rgnChnAttr.unChnAttr.stOrlChn.mLayer          = i;
        alogv("ch_idx=%d, attach region hdl=%d, vipp=%d", ch_idx, i + gpAiContext->region_info[ch_idx].region_hdl_base, pAiServiceInfo->mDrawOrlVipp);
        AW_MPI_RGN_AttachToChn(i + gpAiContext->region_info[ch_idx].region_hdl_base, &ViChn, &rgnChnAttr);
    }

    gpAiContext->region_info[ch_idx].old_num_of_boxes = res->valid_cnt;
    return;
}



#if 0
static void *NpuDetFaceThread(void *para)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	float pair_fr_score = 0;
	AiServiceInfo *plnk = (AiServiceInfo*)para;
	VideoFrameBufferSizeInfo vfbsInfo;
	VIDEO_FRAME_INFO_S pFrameInfo;

	BBoxResults_t boxResults;
	pix_image_face_info_t *pFaceInfo = NULL;
	unsigned char *yuv_buffer_face = NULL;

#ifdef  _FRAME_RATE_ENABLE_
    static unsigned long long picture_counter = 0;
    static unsigned long long old_picture_counter = 0;
    static struct timeval start, end;
	static int npu_det_fps = 0;
	signed long long time_sum;
#endif

#ifdef _TIME_CONSUMING_
	struct timeval t1,t2;
	signed long long t_sum;
#endif
	int offset = plnk->mCaptureWidth*plnk->mCaptureHeight;
	int size = plnk->mCaptureWidth*plnk->mCaptureHeight*3/2;


#if 0
	size_t stack_size = 0; //堆栈大小变量
	pthread_attr_t attr; //线程属性结构体变量
	pthread_attr_init(&attr);
	pthread_attr_getstacksize(&attr, &stack_size);
	printf("stack_size = %dB, %dk\n", stack_size, stack_size/1024);
	stack_size = 10*stack_size;
	pthread_attr_setstacksize(&attr, stack_size);
	pthread_attr_getstacksize(&attr, &stack_size);
	printf("stack_size = %dB, %dk\n", stack_size, stack_size/1024);
#endif



	yuv_buffer_face = malloc(size);

	if(NULL == yuv_buffer_face)
	{
		aloge("malloc yuv_buffer_face error!!!!\n");
		goto _err0 ;
	}

	pFaceInfo = (pix_image_face_info_t *) malloc(sizeof(pix_image_face_info_t));

	if(NULL == pFaceInfo)
	{
		aloge("malloc pFaceInfo error!!!!\n");
		goto _err0 ;
	}

	alogd("mChIdx[%d] mVippDev[%d] mViChn:[%d],mPixelFormat:[%d],mCaptureWidth:[%d],mCaptureHeight:[%d],mSrcFrameRate:[%d],sizeof(pix_image_face_info_t):[%d]\n"
	,plnk->mChIdx
	,plnk->mVippDev
	,plnk->mViChn
	,plnk->mPixelFormat
	,plnk->mCaptureWidth
	,plnk->mCaptureHeight
	,plnk->mSrcFrameRate
	,sizeof(pix_image_face_info_t));


    while(!gpAiContext->mAiserviceExit[plnk->mChIdx])
    {
		memset(&pFrameInfo, 0, sizeof(VIDEO_FRAME_INFO_S));
		memset(&vfbsInfo, 0, sizeof(VideoFrameBufferSizeInfo));
		memset(pFaceInfo, 0, sizeof(pix_image_face_info_t));
		//memset(&boxResults, 0, sizeof(BBoxResults_t));

		if(AW_MPI_VI_GetFrame(plnk->mVippDev, plnk->mViChn, &pFrameInfo, 1000) != SUCCESS)
		{
			alogw("get frame failure this chance, re-again.");
			continue;
		}
		if (getVideoFrameBufferSizeInfo(&pFrameInfo, &vfbsInfo) != SUCCESS)
		{
			aloge("fatal error, get pixfmnt failure.");
			continue;
		}

		if (pFrameInfo.VFrame.mpVirAddr[0])
		{
			memcpy(yuv_buffer_face, pFrameInfo.VFrame.mpVirAddr[0], vfbsInfo.mYSize);
		}
		if (pFrameInfo.VFrame.mpVirAddr[1])
		{
			memcpy(yuv_buffer_face + offset, pFrameInfo.VFrame.mpVirAddr[1], vfbsInfo.mUSize);
		}
		if (pFrameInfo.VFrame.mpVirAddr[2])
		{
			aloge("fatal error, the V component should be null.");
			memcpy(yuv_buffer_face + offset + vfbsInfo.mUSize, pFrameInfo.VFrame.mpVirAddr[2], vfbsInfo.mVSize);
		}

		AW_MPI_VI_ReleaseFrame(plnk->mVippDev, plnk->mViChn, &pFrameInfo);

		/*NPU 处理*/
	#ifdef _TIME_CONSUMING_
		gettimeofday(&t1, NULL);
	#endif
		ret = ai_det_face(yuv_buffer_face,pFaceInfo);
	#ifdef _TIME_CONSUMING_
		gettimeofday(&t2, NULL);
		t_sum = ((1000 * 1000 * t2.tv_sec + t2.tv_usec) - (1000 * 1000 * t1.tv_sec + t1.tv_usec));
		alogd("ret:%d,t_sum:%lld\n",ret,t_sum);
	#endif
		if(!ret)//get faceinfo success
		{
			boxResults.valid_cnt = 0;
			k=0;
			for(i = 0; i < pFaceInfo->rgb_coord.face_number; i++)
			{
				for(j=0;j<gpAiContext->mConfigPara.mPersonInfo.mToatalNum;j++)
				{
					if(gpAiContext->mConfigPara.mPersonInfo.mPesonInfos[j].mVaild)
					{
						ret = ai_get_face_score(pFaceInfo->fr_feature[i].fea,gpAiContext->mConfigPara.mPersonInfo.mPesonInfos[j].mPersonFea, &pair_fr_score);
						if(ret)
						{
							alogw("Fail to pix_cal_fea_sim with %d,j=%d\n", ret,j);
							continue;
						}
						if(pair_fr_score > ai_get_threshold())
						{

							if(k<BOX_NUM)
							{
								boxResults.boxes[k].xmin  = (int)pFaceInfo->rgb_coord.face_box[i * API_PER_BOX_ELEMENTS + 0];
								boxResults.boxes[k].ymin  = (int)pFaceInfo->rgb_coord.face_box[i * API_PER_BOX_ELEMENTS + 1];
								boxResults.boxes[k].xmax  = (int)pFaceInfo->rgb_coord.face_box[i * API_PER_BOX_ELEMENTS + 2];
								boxResults.boxes[k].ymax  = (int)pFaceInfo->rgb_coord.face_box[i * API_PER_BOX_ELEMENTS + 3];
								boxResults.boxes[k].match = TRUE;
								boxResults.valid_cnt++;
								k++;
							}
							alogd("!!!!!!!!!!![PersonName:%s,PersonId:%d] match success!!!!!!!!!!\n"
									,gpAiContext->mConfigPara.mPersonInfo.mPesonInfos[j].mPersonName
									,gpAiContext->mConfigPara.mPersonInfo.mPesonInfos[j].mPersonId);
							//break;
						}
					}
				}
			}
			paint_object_detect_region_face(&boxResults,plnk->mChIdx);
		}
		else //can not  get faceinfo success
		{
			boxResults.valid_cnt = 0;
			paint_object_detect_region_face(&boxResults,plnk->mChIdx);
		}
#ifdef  _FRAME_RATE_ENABLE_
		/*计算帧率*/
		gettimeofday(&end, NULL);
        time_sum = ((1000.f * 1000 * end.tv_sec + end.tv_usec) - (1000.f * 1000 * start.tv_sec + start.tv_usec));
        if ((time_sum >= (1000 * 1000)) && (start.tv_sec != 0 || start.tv_usec != 0))
        {
            start = end;
            npu_det_fps = picture_counter - old_picture_counter;
            old_picture_counter = picture_counter;
			alogd("frame no %d.\n", npu_det_fps);
        }
		if (picture_counter == 0)
        {
            gettimeofday(&start, NULL);
            old_picture_counter = 0;
        }
		picture_counter++;
#endif
    }

	boxResults.valid_cnt = 0;
	paint_object_detect_region_face(&boxResults,plnk->mChIdx);

_err0:
    if (yuv_buffer_face)
    {
		free(yuv_buffer_face);
		yuv_buffer_face = NULL;
    }

	if(pFaceInfo)
	{
	    free(pFaceInfo);
        pFaceInfo = NULL;
	}

	return ;
}

#else
static void *NpuDetFaceThread(void *para)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	int k = 0;
	float fPairFrScore = 0;
	AiContext* pAiContext = (AiContext*)para;

	VIDEO_FRAME_INFO_S stFrameInfoRGB;
	VideoFrameBufferSizeInfo stVfbsInfoRGB;
	unsigned char *pYuvBufferFaceRGB = NULL;

	VIDEO_FRAME_INFO_S stFrameInfoIR;
	VideoFrameBufferSizeInfo stVfbsInfoIR;
	unsigned char *pYuvBufferFaceIR = NULL;

	pix_image_face_info_t *pFaceInfo = NULL;
	BBoxResults_t stBoxResults;

#ifdef  _FRAME_RATE_ENABLE_
    static unsigned long long picture_counter = 0;
    static unsigned long long old_picture_counter = 0;
    static struct timeval start, end;
	static int npu_det_fps = 0;
	signed long long time_sum;
#endif

#ifdef _TIME_CONSUMING_
	struct timeval t1,t2;
	signed long long t_sum;
#endif
	int nOffsetRGB = pAiContext->mConfigPara.mAiServiceInfoConifgArray[0].mCaptureHeight  * pAiContext->mConfigPara.mAiServiceInfoConifgArray[0].mCaptureWidth;
	int nSizeRGB =pAiContext->mConfigPara.mAiServiceInfoConifgArray[0].mCaptureHeight  * pAiContext->mConfigPara.mAiServiceInfoConifgArray[0].mCaptureWidth * 3 / 2;

    int nOffsetIR = pAiContext->mConfigPara.mAiServiceInfoConifgArray[1].mCaptureHeight  * pAiContext->mConfigPara.mAiServiceInfoConifgArray[1].mCaptureWidth;
	int nSizeIR = pAiContext->mConfigPara.mAiServiceInfoConifgArray[1].mCaptureHeight  * pAiContext->mConfigPara.mAiServiceInfoConifgArray[1].mCaptureWidth * 3 / 2;

	pYuvBufferFaceRGB = (unsigned char *)malloc(nSizeRGB);
	if(NULL == pYuvBufferFaceRGB)
	{
		aloge("malloc yuv_buffer_face error!!!!\n");
		goto _err0 ;
	}

	pYuvBufferFaceIR = (unsigned char *)malloc(nSizeIR);
	if(NULL == pYuvBufferFaceIR)
	{
		aloge("malloc yuv_buffer_face error!!!!\n");
		goto _err0 ;
	}

    pFaceInfo = (pix_image_face_info_t *)malloc(sizeof(pix_image_face_info_t));
	if(NULL == pFaceInfo)
	{
		aloge("malloc pFaceInfo error!!!!\n");
		goto _err0 ;
	}

    while(!pAiContext->mDetectThreadState)
    {
		memset(&stFrameInfoRGB, 0, sizeof(VIDEO_FRAME_INFO_S));
		memset(&stVfbsInfoRGB, 0, sizeof(VideoFrameBufferSizeInfo));

		memset(&stFrameInfoIR, 0, sizeof(VIDEO_FRAME_INFO_S));
		memset(&stVfbsInfoIR, 0, sizeof(VideoFrameBufferSizeInfo));

		memset(pFaceInfo, 0, sizeof(pix_image_face_info_t));
		//memset(&boxResults, 0, sizeof(BBoxResults_t));

        if(AW_MPI_VI_GetFrame(pAiContext->mConfigPara.mAiServiceInfoConifgArray[0].mVippDev, 0, &stFrameInfoIR, 1000) != SUCCESS)
		{
			alogw("get frame failure this chance, re-again.");
			continue;
		}
        if (getVideoFrameBufferSizeInfo(&stFrameInfoIR, &stVfbsInfoIR) != SUCCESS)
		{
			aloge("fatal error, get pixfmnt failure.");
			continue;
		}

		if(AW_MPI_VI_GetFrame(pAiContext->mConfigPara.mAiServiceInfoConifgArray[1].mVippDev, 0, &stFrameInfoRGB, 1000) != SUCCESS)
		{
			alogw("get frame failure this chance, re-again.");
			continue;
		}
        if (getVideoFrameBufferSizeInfo(&stFrameInfoRGB, &stVfbsInfoRGB) != SUCCESS)
		{
			aloge("fatal error, get pixfmnt failure.");
			continue;
		}

        if (stFrameInfoIR.VFrame.mpVirAddr[0])
		{
			memcpy(pYuvBufferFaceIR, stFrameInfoIR.VFrame.mpVirAddr[0], stVfbsInfoIR.mYSize);
		}
		if (stFrameInfoIR.VFrame.mpVirAddr[1])
		{
			memcpy(pYuvBufferFaceIR + nOffsetIR, stFrameInfoIR.VFrame.mpVirAddr[1], stVfbsInfoIR.mUSize);
		}
		if (stFrameInfoIR.VFrame.mpVirAddr[2])
		{
			aloge("fatal error, the V component should be null.");
			memcpy(pYuvBufferFaceIR + nOffsetIR + stVfbsInfoIR.mUSize, stFrameInfoIR.VFrame.mpVirAddr[2], stVfbsInfoIR.mVSize);
		}

		AW_MPI_VI_ReleaseFrame(pAiContext->mConfigPara.mAiServiceInfoConifgArray[0].mVippDev, 0, &stFrameInfoIR);

		if (stFrameInfoRGB.VFrame.mpVirAddr[0])
		{
			memcpy(pYuvBufferFaceRGB, stFrameInfoRGB.VFrame.mpVirAddr[0], stVfbsInfoRGB.mYSize); // 拷贝 y 分量
		}
		if (stFrameInfoRGB.VFrame.mpVirAddr[1])
		{
			memcpy(pYuvBufferFaceRGB + nOffsetRGB, stFrameInfoRGB.VFrame.mpVirAddr[1], stVfbsInfoRGB.mUSize); // 拷贝 uv 分量
		}
		if (stFrameInfoRGB.VFrame.mpVirAddr[2])
		{
			aloge("fatal error, the V component should be null.");
			memcpy(pYuvBufferFaceRGB + nOffsetRGB + stVfbsInfoRGB.mUSize, stFrameInfoRGB.VFrame.mpVirAddr[2], stVfbsInfoRGB.mVSize);
		}

		AW_MPI_VI_ReleaseFrame(pAiContext->mConfigPara.mAiServiceInfoConifgArray[1].mVippDev, 0, &stFrameInfoRGB);

		/*NPU 处理*/
	#ifdef _TIME_CONSUMING_
		gettimeofday(&t1, NULL);
	#endif
		ret = ai_det_face(pYuvBufferFaceIR, pYuvBufferFaceRGB, pFaceInfo);
	#ifdef _TIME_CONSUMING_
		gettimeofday(&t2, NULL);
		t_sum = ((1000 * 1000 * t2.tv_sec + t2.tv_usec) - (1000 * 1000 * t1.tv_sec + t1.tv_usec));
		alogd("ret:%d,t_sum:%lld\n",ret,t_sum);
	#endif
		if(!ret)//get faceinfo success
		{
			stBoxResults.valid_cnt = 0;
			k=0;
			for(i = 0; i < pFaceInfo->rgb_coord.face_number; i++)
			{
				for(j=0;j<pAiContext->mConfigPara.mPersonInfo.mToatalNum;j++)
				{
					if(pAiContext->mConfigPara.mPersonInfo.mPesonInfos[j].mVaild)
					{
						ret = ai_get_face_score(pFaceInfo->fr_feature[i].fea, pAiContext->mConfigPara.mPersonInfo.mPesonInfos[j].mPersonFea, &fPairFrScore);
                        printf("fPairFrScore = %f, ai_get_threshold = %f\n", fPairFrScore, ai_get_threshold());
                        if(ret)
						{
							alogw("Fail to pix_cal_fea_sim with %d,j=%d\n", ret,j);
							continue;
						}
						if(fPairFrScore > ai_get_threshold())
						{
							if(k<BOX_NUM)
							{
								stBoxResults.boxes[k].xmin  = (int)pFaceInfo->coord.face_box[i * API_PER_BOX_ELEMENTS + 0];
								stBoxResults.boxes[k].ymin  = (int)pFaceInfo->coord.face_box[i * API_PER_BOX_ELEMENTS + 1];
								stBoxResults.boxes[k].xmax  = (int)pFaceInfo->coord.face_box[i * API_PER_BOX_ELEMENTS + 2];
								stBoxResults.boxes[k].ymax  = (int)pFaceInfo->coord.face_box[i * API_PER_BOX_ELEMENTS + 3];
								stBoxResults.boxes[k].match = TRUE;
                                stBoxResults.valid_cnt++;

								k++;
							}
							alogd("!!!!!!!!!!![PersonName:%s,PersonId:%d] match success!!!!!!!!!!\n"
									,pAiContext->mConfigPara.mPersonInfo.mPesonInfos[j].mPersonName
									,pAiContext->mConfigPara.mPersonInfo.mPesonInfos[j].mPersonId);
							//break;
						}
					}
				}
			}
			paint_object_detect_region_face(&stBoxResults,pAiContext->mAiServiceInfo[0].mChIdx);
		}
		else //can not  get faceinfo success
		{
			stBoxResults.valid_cnt = 0;
			paint_object_detect_region_face(&stBoxResults,pAiContext->mAiServiceInfo[0].mChIdx);
		}
#ifdef  _FRAME_RATE_ENABLE_
		/*计算帧率*/
		gettimeofday(&end, NULL);
        time_sum = ((1000.f * 1000 * end.tv_sec + end.tv_usec) - (1000.f * 1000 * start.tv_sec + start.tv_usec));
        if ((time_sum >= (1000 * 1000)) && (start.tv_sec != 0 || start.tv_usec != 0))
        {
            start = end;
            npu_det_fps = picture_counter - old_picture_counter;
            old_picture_counter = picture_counter;
			alogd("frame no %d.\n", npu_det_fps);
        }
		if (picture_counter == 0)
        {
            gettimeofday(&start, NULL);
            old_picture_counter = 0;
        }
		picture_counter++;
#endif
    }

	stBoxResults.valid_cnt = 0;
	paint_object_detect_region_face(&stBoxResults,gpAiContext->mAiServiceInfo[0].mChIdx);

_err0:

	if(pFaceInfo)
	{
	    free(pFaceInfo);
        pFaceInfo = NULL;
	}

    if (pYuvBufferFaceIR)
    {
		free(pYuvBufferFaceIR);
		pYuvBufferFaceIR = NULL;
    }

	if(pYuvBufferFaceRGB)
	{
	    free(pYuvBufferFaceRGB);
        pYuvBufferFaceRGB = NULL;
	}

	return (void*)0;
}

#endif

int LoadSamplePics(AiContext *pAiContext)
{
	int i = 0;
	int ret = -1;
	if(pAiContext == NULL)
	{
		aloge("param error:pAiContext==NULL\n");
	}

	for(i=0;i<pAiContext->mConfigPara.mPersonInfo.mValidNum;i++)
	{
		if(pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mVaild)
		{
			ret = ai_save_face(pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mPersonIRFile
                     ,pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mPersonRGBFile
					 ,pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mWidht
					 ,pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mHeight
					 ,pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mPersonFea
					 ,sizeof(pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mPersonFea));
			if(!ret)
			{
				alogw("[id:%d,name:%s]save success!!!!!\n",pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mPersonId,pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mPersonName);
			}
			else
			{
				pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mVaild = 0;
				alogw("[id:%d,name:%s]save falied!!!!!\n",pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mPersonId,pAiContext->mConfigPara.mPersonInfo.mPesonInfos[i].mPersonName);
			}
		}
	}
	return 0;
}

int main(int argc ,char**argv)
{
	int ret = 0;
	int i = 0;
	SampleFacekitContext *pContext = NULL;
	AiContext *pAiContext = NULL;
	char *pConfigFilePath = NULL;
	unsigned long value;
	MPP_SYS_CONF_S stSysConf;
	VO_PUB_ATTR_S spPubAttr;

	alogd("Sample_Facekit running!\n");
	do
	{
		pContext = (SampleFacekitContext*)malloc(sizeof(SampleFacekitContext));
		if(!pContext)
		{
			ret = -1;
			break;
		}
		pAiContext = (AiContext*)malloc(sizeof(AiContext));
		if(!pAiContext)
		{
			ret = -1;
			break;
		}
		 gpSampleFacekitContext = pContext;
		 memset(pContext, 0, sizeof(SampleFacekitContext));
		 gpAiContext = pAiContext;
		 memset(pAiContext, 0, sizeof(AiContext));
	 	 cdx_sem_init(&pContext->mSemExit, 0);

		 /* register process function for SIGINT, to exit program. */
		 if (signal(SIGINT, handle_exit) == SIG_ERR)
		 {
			 aloge("can't catch SIGSEGV");
		 }

		 if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
		 {
			 aloge("fatal error! command line param is wrong, exit!");
			 ret = -1;
			 break;
		 }

		 if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
		 {
			 pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
		 }
		 else
		 {
			 pConfigFilePath = NULL;
		 }
		 /* parse config file. */
		 if(loadSampleFacekitConfig(&pContext->mConfigPara,&pAiContext->mConfigPara,pConfigFilePath) != SUCCESS)
		 {
			 aloge("fatal error! no config file or parse conf file fail");
			 ret = -1;
			 break;
		 }

		memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
		stSysConf.nAlignWidth = 32;
		AW_MPI_SYS_SetConf(&stSysConf);
		ret = AW_MPI_SYS_Init();
		if (ret < 0)
		{
			 aloge("sys Init failed!");
			 ret = -1;
		 break;
		}
	   	pContext->mVoDev = 0;
	   	AW_MPI_VO_Enable(pContext->mVoDev);
	   	AW_MPI_VO_GetPubAttr(pContext->mVoDev, &spPubAttr);
	   	spPubAttr.enIntfType = pContext->mConfigPara.mDispType;
	   	spPubAttr.enIntfSync = pContext->mConfigPara.mDispSync;
	   	AW_MPI_VO_SetPubAttr(pContext->mVoDev, &spPubAttr);

		ret = vip_init(0);
		if(ret)
		{
			 aloge("vip_init error!\n");
			 ret = -1;
			 break;
		}
		ret = ai_init(pAiContext->mConfigPara.mAiServiceInfoConifgArray[0].mModelFile);
		if(ret)
		{
			 save_bonding();
			 aloge("init error!\n");
			 ret = -1;
			 break;
		}

		/*加载样本图片*/
		LoadSamplePics(pAiContext);

	    for(i=0;i<VIPP2VO_NUM;i++)
	    {
	        CreateVIPP2VOLink(i, pContext);
	    }

		for(i=0;i<NN_CHN_NUM_MAX;i++)
	    {
	        CreateNpuVIPPLink(i, pAiContext);
	    }

        ret = pthread_create(&pAiContext->mDetectThreadId, NULL, NpuDetFaceThread, pAiContext);

	   if(pContext->mConfigPara.mTestDuration > 0)
	   {
		   cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
	   }
	   else
	   {
		   cdx_sem_down(&pContext->mSemExit);
	   }

        pAiContext->mDetectThreadState = 1;
        ret = pthread_join(pAiContext->mDetectThreadId, (void **)&value);
        for(i=0;i<NN_CHN_NUM_MAX;i++)
        {
            DestroyNpuVIPPLink(i, pAiContext);
        }

        ai_deinit();

	   for(i=0;i<VIPP2VO_NUM;i++)
	   {
		   DestroyVIPP2VOLink(i, pContext);
	   }
	   AW_MPI_VO_Disable(pContext->mVoDev);
	   pContext->mVoDev = -1;
	   AW_MPI_SYS_Exit();

	}while(0);

	cdx_sem_deinit(&pContext->mSemExit);
	if(pContext)
	{
		free(pContext);
		//aloge("fatal error! malloc pContext failed! size=%d", sizeof(SampleFacekitContext));
		pContext = NULL;
		gpSampleFacekitContext = NULL;
	}
	if(pAiContext)
	{
		free(pAiContext);
		//aloge("fatal error! malloc pAiContext failed! size=%d", sizeof(AiContext));
		pAiContext = NULL;
		gpAiContext = NULL;
	}


 	return ret;
}

