/******************************************************************************
  Copyright (C), 2001-2022, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : sample_smartIPC_demo.c
  Version       : Initial Draft
  Author        : Allwinner
  Created       : 2022/5/12
  Last Modified :
  Description   : Demonstrate Smart IPC scenarios
  Function List :
  History       :
******************************************************************************/

//#define LOG_NDEBUG 0
#define LOG_TAG "sample_smartIPC_demo"

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
#include <sys/prctl.h>
#include <sys/time.h>

#include "media/mm_comm_vi.h"
#include "media/mpi_vi.h"
#include "media/mpi_isp.h"
#include "media/mpi_venc.h"
#include "media/mpi_sys.h"
#include "mm_common.h"
#include "mm_comm_venc.h"
#include "mm_comm_rc.h"
#include <mpi_videoformat_conversion.h>
#include <SystemBase.h>
#include <confparser.h>
#include <utils/VIDEO_FRAME_INFO_S.h>

#include "sample_smartIPC_demo.h"
#include "sample_smartIPC_demo_config.h"
#include <awnn.h>
#include "../common/rtsp_server.h"
#include "../common/record.h"
#include "../common/aiservice.h"
#include "../common/awaiisp_common.h"
#include "../common/sample_common_venc.h"
#include "../common/gtm_ldci_common.h"
#include "../common/tdm_raw_process.h"

//#define ENABLE_VENC_ADVANCED_PARAM
#define SUPPORT_RTSP_TEST
//#define SUPPORT_STREAM_OSD_TEST
#define SUPPORT_SAVE_STREAM
#define SUPPORT_AI_SERVICE
#define SUPPORT_AWAIISP

#define DAY_TO_NIGHT_SIGNAL_CNT     30
#define NIGHT_TO_DAY_SIGNAL_CNT     30
#define DAY_TO_NIGHT_THRESHOLD      60
#define NIGHT_TO_DAY_THRESHOLD      200

static SampleSmartIPCDemoContext *gpSampleSmartIPCDemoContext = NULL;

static unsigned int getSysTickMs()
{
    unsigned int ms = 0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ms = tv.tv_sec*1000 + tv.tv_usec/1000;
    return ms;
}

static void handle_exit(int signo)
{
    alogd("user want to exit!");
    if(NULL != gpSampleSmartIPCDemoContext)
    {
        cdx_sem_up(&gpSampleSmartIPCDemoContext->mSemExit);
    }
}

static int ParseCmdLine(int argc, char **argv, SampleSmartIPCDemoCmdLineParam *pCmdLinePara)
{
    alogd("path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(pCmdLinePara, 0, sizeof(SampleSmartIPCDemoCmdLineParam));
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

static PAYLOAD_TYPE_E getEncoderTypeFromConfig(CONFPARSER_S *pConfParser, const char *key)
{
    PAYLOAD_TYPE_E EncType = PT_BUTT;
    char *ptr = (char *)GetConfParaString(pConfParser, key, NULL);
    if(!strcmp(ptr, "H.264"))
    {
        EncType = PT_H264;
    }
    else if(!strcmp(ptr, "H.265"))
    {
        EncType = PT_H265;
    }
    else
    {
        aloge("fatal error! conf file encoder[%s] is unsupported", ptr);
        EncType = PT_H264;
    }

    return EncType;
}

static VENC_RC_MODE_E getRcModeFromConfig(int rc_mode, PAYLOAD_TYPE_E eEncType)
{
    VENC_RC_MODE_E eRcMode = VENC_RC_MODE_BUTT;

    if (PT_H264 == eEncType)
    {
        switch (rc_mode)
        {
        case 0:
            eRcMode = VENC_RC_MODE_H264CBR;
            break;
        case 1:
            eRcMode = VENC_RC_MODE_H264VBR;
            break;
        case 2:
            eRcMode = VENC_RC_MODE_H264FIXQP;
            break;
        default:
            aloge("not support! use default cbr mode");
            eRcMode = VENC_RC_MODE_H264CBR;
            break;
        }
    }
    else if (PT_H265 == eEncType)
    {
        switch (rc_mode)
        {
        case 0:
            eRcMode = VENC_RC_MODE_H265CBR;
            break;
        case 1:
            eRcMode = VENC_RC_MODE_H265VBR;
            break;
        case 2:
            eRcMode = VENC_RC_MODE_H265FIXQP;
            break;
        default:
            aloge("not support! use default cbr mode");
            eRcMode = VENC_RC_MODE_H265CBR;
            break;
        }
    }
    else if (PT_MJPEG == eEncType)
    {
        switch (rc_mode)
        {
        case 0:
            eRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        case 2:
            eRcMode = VENC_RC_MODE_MJPEGFIXQP;
            break;
        default:
            aloge("not support! use default cbr mode");
            eRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        }
    }

    return eRcMode;
}

static ERRORTYPE loadSampleConfig(SampleSmartIPCDemoConfig *pConfig, const char *conf_path)
{
    if (NULL == pConfig)
    {
        aloge("fatal error, pConfig is NULL!");
        return FAILURE;
    }

    if (NULL != conf_path)
    {
        char *ptr = NULL;
        CONFPARSER_S stConfParser;
        int ret = createConfParser(conf_path, &stConfParser);
        if(ret < 0)
        {
            aloge("fatal error, load conf failed!");
            return FAILURE;
        }

        // rtsp
        pConfig->mRtspNetType = GetConfParaInt(&stConfParser, CFG_RtspNetType, 0);
        // common params
        pConfig->mStreamBufSize = GetConfParaInt(&stConfParser, CFG_StreamBufSize, 0);
        pConfig->mVeRecRefBufReduceEnable = GetConfParaInt(&stConfParser, CFG_VeRecRefBufReduceEnable, 0);
        pConfig->mAiIspNpuRefBufReduceEnable = GetConfParaInt(&stConfParser, CFG_AiIspNpuRefBufReduceEnable, 0);
        pConfig->mAiIspSwitchReleaseResEnable = GetConfParaInt(&stConfParser, CFG_AiIspSwitchReleaseResEnable, 0);
        pConfig->mProductMode = GetConfParaInt(&stConfParser, CFG_ProductMode, 0);
        pConfig->mVbrOptEnable = GetConfParaInt(&stConfParser, CFG_VBR_OPT_ENABLE, 0);
        pConfig->mRcMode = GetConfParaInt(&stConfParser, CFG_RC_MODE, 0);
        pConfig->mInitQp = GetConfParaInt(&stConfParser, CFG_INIT_QP, 0);
        pConfig->mMinIQp = GetConfParaInt(&stConfParser, CFG_MIN_I_QP, 0);
        pConfig->mMaxIQp = GetConfParaInt(&stConfParser, CFG_MAX_I_QP, 0);
        pConfig->mMinPQp = GetConfParaInt(&stConfParser, CFG_MIN_P_QP, 0);
        pConfig->mMaxPQp = GetConfParaInt(&stConfParser, CFG_MAX_P_QP, 0);
        pConfig->mEnMbQpLimit = GetConfParaInt(&stConfParser, CFG_MB_QP_LIMIT, 0);
        pConfig->mMovingTh = GetConfParaInt(&stConfParser, CFG_MOVING_TH, 0);
        pConfig->mQuality = GetConfParaInt(&stConfParser, CFG_QUALITY, 0);
        pConfig->mPBitsCoef = GetConfParaInt(&stConfParser, CFG_P_BITS_COEF, 0);
        pConfig->mIBitsCoef = GetConfParaInt(&stConfParser, CFG_I_BITS_COEF, 0);
        // isp and ve linkage
        pConfig->mIspAndVeLinkageEnable = GetConfParaInt(&stConfParser, CFG_IspAndVeLinkageEnable, 0);
        pConfig->mCameraAdaptiveMovingAndStaticEnable = GetConfParaInt(&stConfParser, CFG_CameraAdaptiveMovingAndStaticEnable, 0);
        pConfig->mVencLensMovingMaxQp = GetConfParaInt(&stConfParser, CFG_VencLensMovingMaxQp, 0);
        // wb yuv
        pConfig->mWbYuvEnable = GetConfParaInt(&stConfParser, CFG_WbYuvEnable, 0);
        pConfig->mWbYuvBufNum = GetConfParaInt(&stConfParser, CFG_WbYuvBufNum, 0);
        pConfig->mWbYuvStartIndex = GetConfParaInt(&stConfParser, CFG_WbYuvStartIndex, 0);
        pConfig->mWbYuvTotalCnt = GetConfParaInt(&stConfParser, CFG_WbYuvTotalCnt, 0);
        pConfig->mWbYuvStreamChn = GetConfParaInt(&stConfParser, CFG_WbYuvStreamChannel, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_WbYuvFilePath, NULL);
        strncpy(pConfig->mWbYuvFilePath, ptr, MAX_FILE_PATH_SIZE);
        // test code
        pConfig->mViTimeoutResetDisable = GetConfParaInt(&stConfParser, CFG_ViTimeoutResetDisable, 0);
        pConfig->mTestTriggerViTimeout = GetConfParaInt(&stConfParser, CFG_TestTriggerViTimeout, 0);
        // others
        pConfig->mTestDuration = GetConfParaInt(&stConfParser, CFG_TestDuration, 0);
        // main stream
        pConfig->mMainEnable = GetConfParaInt(&stConfParser, CFG_MainEnable, 0);
        pConfig->mMainRtspID = GetConfParaInt(&stConfParser, CFG_MainRtspID, 0);
        pConfig->mMainIsp = GetConfParaInt(&stConfParser, CFG_MainIsp, 0);
        pConfig->mMainIspD3dLbcRatio = GetConfParaInt(&stConfParser, CFG_MainIspD3dLbcRatio, 0);
        pConfig->mMainVipp = GetConfParaInt(&stConfParser, CFG_MainVipp, 0);
        pConfig->mMainViChn = GetConfParaInt(&stConfParser, CFG_MainViChn, 0);
        pConfig->mMainLdciUseExtBufEnable = GetConfParaInt(&stConfParser, CFG_MainLdciUseExtBufEnable, 0);
        pConfig->mMainLdciVipp = GetConfParaInt(&stConfParser, CFG_MainLdciVipp, 0);
        pConfig->mMainSrcWidth = GetConfParaInt(&stConfParser, CFG_MainSrcWidth, 0);
        pConfig->mMainSrcHeight = GetConfParaInt(&stConfParser, CFG_MainSrcHeight, 0);
        pConfig->mMainPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_MainPixelFormat);
        pConfig->mMainWdrEnable = GetConfParaInt(&stConfParser, CFG_MainWdrEnable, 0);
        pConfig->mMainViBufNum = GetConfParaInt(&stConfParser, CFG_MainViBufNum, 0);
        pConfig->mMainSrcFrameRate = GetConfParaInt(&stConfParser, CFG_MainSrcFrameRate, 0);
        pConfig->mMainVEncChn = GetConfParaInt(&stConfParser, CFG_MainVEncChn, 0);
        pConfig->mMainEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_MainEncodeType);
        pConfig->mMainEncodeWidth = GetConfParaInt(&stConfParser, CFG_MainEncodeWidth, 0);
        pConfig->mMainEncodeHeight = GetConfParaInt(&stConfParser, CFG_MainEncodeHeight, 0);
        pConfig->mMainEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_MainEncodeFrameRate, 0);
        pConfig->mMainEncodeBitrate = GetConfParaInt(&stConfParser, CFG_MainEncodeBitrate, 0);
        pConfig->mMainOnlineEnable = GetConfParaInt(&stConfParser, CFG_MainOnlineEnable, 0);
        pConfig->mMainOnlineShareBufNum = GetConfParaInt(&stConfParser, CFG_MainOnlineShareBufNum, 0);
        pConfig->mMainEncppEnable = GetConfParaInt(&stConfParser, CFG_MainEncppEnable, 0);
        pConfig->mMainVeRefFrameLbcMode = GetConfParaInt(&stConfParser, CFG_MainVeRefFrameLbcMode, 0);
        pConfig->mMainKeyFrameInterval = GetConfParaInt(&stConfParser, CFG_MainKeyFrameInterval, 0);
        pConfig->mMainIspTestEnable = GetConfParaInt(&stConfParser, CFG_MainIspTestEnable, 0);
        pConfig->mMainIspTestIntervalMs = GetConfParaInt(&stConfParser, CFG_MainIspTestIntervalMs, 0);
        pConfig->mMainDetectMipiDeskEnable = GetConfParaInt(&stConfParser, CFG_MainDetectMipiEnable, 0);
        pConfig->mMainDetectIntervalMs = GetConfParaInt(&stConfParser, CFG_MainDetectMipiIntrervMs, 0);
        pConfig->mMainMipiChannel = GetConfParaInt(&stConfParser, CFG_MainDetectMipiChannel, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainFilePath, NULL);
        strncpy(pConfig->mMainFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMainSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_MainSaveOneFileDuration, 0);
        pConfig->mMainSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_MainSaveMaxFileCnt, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainDrawOSDText, NULL);
        strncpy(pConfig->mMainDrawOSDText, ptr, MAX_FILE_PATH_SIZE);
        // main tdm raw
        pConfig->mMainIspTdmRawProcessType = GetConfParaInt(&stConfParser, CFG_MainIspTdmRawProcessType, -1);
        pConfig->mMainIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_MainIspTdmRxBufNum, 5);
        pConfig->mMainIspTdmRawProcessFrameCntMin = GetConfParaInt(&stConfParser, CFG_MainIspTdmRawProcessFrameCntMin, 0);
        pConfig->mMainIspTdmRawProcessFrameCntMax = GetConfParaInt(&stConfParser, CFG_MainIspTdmRawProcessFrameCntMax, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainIspTdmRawFilePath, NULL);
        strncpy(pConfig->mMainIspTdmRawFilePath, ptr, MAX_FILE_PATH_SIZE);
        // main nn
        pConfig->mMainNnEnable = GetConfParaInt(&stConfParser, CFG_MainNnEnable, 0);
        pConfig->mMainNnNbgType = GetConfParaInt(&stConfParser, CFG_MainNnNbgType, 0);
        pConfig->mMainNnVipp = GetConfParaInt(&stConfParser, CFG_MainNnVipp, 0);
        pConfig->mMainNnViBufNum = GetConfParaInt(&stConfParser, CFG_MainNnViBufNum, 0);
        pConfig->mMainNnSrcFrameRate = GetConfParaInt(&stConfParser, CFG_MainNnSrcFrameRate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainNnNbgFilePath, NULL);
        strncpy(pConfig->mMainNnNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMainNnDrawOrlEnable = GetConfParaInt(&stConfParser, CFG_MainNnDrawOrlEnable, 0);
        // main aiisp
        pConfig->mMainAiIspEnable = GetConfParaInt(&stConfParser, CFG_MainAiIspEnable, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainAiIspLutNbgFilePath, NULL);
        strncpy(pConfig->mMainAiIspLutNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainAiIspNbgFilePath, NULL);
        strncpy(pConfig->mMainAiIspNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMainAiIspModelVersion = GetConfParaInt(&stConfParser, CFG_MainAiIspModelVersion, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainAiIspCfgBinPath, NULL);
        strncpy(pConfig->mMainAiIspCfgBinPath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMainAiIspWidth = GetConfParaInt(&stConfParser, CFG_MainAiIspWidth, 0);
        pConfig->mMainAiIspHeight = GetConfParaInt(&stConfParser, CFG_MainAiIspHeight, 0);
        pConfig->mMainAiIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_MainAiIspTdmRxBufNum, 0);
        pConfig->mMainAiIspMode = GetConfParaInt(&stConfParser, CFG_MainAiIspMode, 0);
        pConfig->mMainAiIspAutoSwitchEnable = GetConfParaInt(&stConfParser, CFG_MainAiIspAutoSwitchEnable, 0);
        pConfig->mMainAiIspSwitchInterval = GetConfParaInt(&stConfParser, CFG_MainAiIspSwitchInterval, 0);
        int nMainAiIspSwitchIntervalDefault = pConfig->mMainEncodeFrameRate * 5;
        if (pConfig->mMainAiIspSwitchInterval > 0 && pConfig->mMainAiIspSwitchInterval < nMainAiIspSwitchIntervalDefault)
        {
            alogd("main aiisp switch interval %d is too small, force to set default %d", pConfig->mMainAiIspSwitchInterval, nMainAiIspSwitchIntervalDefault);
            pConfig->mMainAiIspSwitchInterval = nMainAiIspSwitchIntervalDefault;
        }
        pConfig->mMainAiIspSwitchCase = GetConfParaInt(&stConfParser, CFG_MainAiIspSwitchCase, 0);
        pConfig->mMainAiIspSwitchDropFrameNum = GetConfParaInt(&stConfParser, CFG_MainAiIspSwitchDropFrameNum, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_MainAiIspCfgBinPath2, NULL);
        strncpy(pConfig->mMainAiIspCfgBinPath2, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMainAiIspReserve0 = GetConfParaInt(&stConfParser, CFG_MainAiIspReserve0, 0);
        pConfig->mMainAiIspReserve1 = GetConfParaInt(&stConfParser, CFG_MainAiIspReserve1, 0);
        pConfig->mMainAiIspReserve2 = GetConfParaInt(&stConfParser, CFG_MainAiIspReserve2, 0);
        // main 2nd stream
        pConfig->mMain2ndEnable = GetConfParaInt(&stConfParser, CFG_Main2ndEnable, 0);
        pConfig->mMain2ndVipp = GetConfParaInt(&stConfParser, CFG_Main2ndVipp, 0);
        pConfig->mMain2ndViChn = GetConfParaInt(&stConfParser, CFG_Main2ndViChn, 0);
        pConfig->mMain2ndSrcWidth = GetConfParaInt(&stConfParser, CFG_Main2ndSrcWidth, 0);
        pConfig->mMain2ndSrcHeight = GetConfParaInt(&stConfParser, CFG_Main2ndSrcHeight, 0);
        pConfig->mMain2ndPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_Main2ndPixelFormat);
        pConfig->mMain2ndViBufNum = GetConfParaInt(&stConfParser, CFG_Main2ndViBufNum, 0);
        pConfig->mMain2ndSrcFrameRate = GetConfParaInt(&stConfParser, CFG_Main2ndSrcFrameRate, 0);
        pConfig->mMain2ndVEncChn = GetConfParaInt(&stConfParser, CFG_Main2ndVEncChn, 0);
        pConfig->mMain2ndEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_Main2ndEncodeType);
        pConfig->mMain2ndEncodeWidth = GetConfParaInt(&stConfParser, CFG_Main2ndEncodeWidth, 0);
        pConfig->mMain2ndEncodeHeight = GetConfParaInt(&stConfParser, CFG_Main2ndEncodeHeight, 0);
        pConfig->mMain2ndEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_Main2ndEncodeFrameRate, 0);
        pConfig->mMain2ndEncodeBitrate = GetConfParaInt(&stConfParser, CFG_Main2ndEncodeBitrate, 0);
        pConfig->mMain2ndEncppSharpAttenCoefPer = 100 * pConfig->mMain2ndEncodeWidth / pConfig->mMainEncodeWidth;
        pConfig->mMain2ndEncppEnable = GetConfParaInt(&stConfParser, CFG_Main2ndEncppEnable, 0);
        pConfig->mMain2ndVeRefFrameLbcMode = GetConfParaInt(&stConfParser, CFG_Main2ndVeRefFrameLbcMode, 0);
        pConfig->mMain2ndKeyFrameInterval = GetConfParaInt(&stConfParser, CFG_Main2ndKeyFrameInterval, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_Main2ndFilePath, NULL);
        strncpy(pConfig->mMain2ndFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mMain2ndSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_Main2ndSaveOneFileDuration, 0);
        pConfig->mMain2ndSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_Main2ndSaveMaxFileCnt, 0);
        // sub stream
        pConfig->mSubEnable = GetConfParaInt(&stConfParser, CFG_SubEnable, 0);
        pConfig->mSubRtspID = GetConfParaInt(&stConfParser, CFG_SubRtspID, 0);
        pConfig->mSubIsp = GetConfParaInt(&stConfParser, CFG_SubIsp, 0);
        pConfig->mSubIspD3dLbcRatio = GetConfParaInt(&stConfParser, CFG_SubIspD3dLbcRatio, 0);
        pConfig->mSubVipp = GetConfParaInt(&stConfParser, CFG_SubVipp, 0);
        pConfig->mSubLdciUseExtBufEnable = GetConfParaInt(&stConfParser, CFG_SubLdciUseExtBufEnable, 0);
        pConfig->mSubLdciVipp = GetConfParaInt(&stConfParser, CFG_SubLdciVipp, 0);
        pConfig->mSubSrcWidth = GetConfParaInt(&stConfParser, CFG_SubSrcWidth, 0);
        pConfig->mSubSrcHeight = GetConfParaInt(&stConfParser, CFG_SubSrcHeight, 0);
        pConfig->mSubPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_SubPixelFormat);
        pConfig->mSubWdrEnable = GetConfParaInt(&stConfParser, CFG_SubWdrEnable, 0);
        pConfig->mSubViBufNum = GetConfParaInt(&stConfParser, CFG_SubViBufNum, 0);
        pConfig->mSubSrcFrameRate = GetConfParaInt(&stConfParser, CFG_SubSrcFrameRate, 0);
        pConfig->mSubViChn = GetConfParaInt(&stConfParser, CFG_SubViChn, 0);
        pConfig->mSubVEncChn = GetConfParaInt(&stConfParser, CFG_SubVEncChn, 0);
        pConfig->mSubEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_SubEncodeType);
        pConfig->mSubEncodeWidth = GetConfParaInt(&stConfParser, CFG_SubEncodeWidth, 0);
        pConfig->mSubEncodeHeight = GetConfParaInt(&stConfParser, CFG_SubEncodeHeight, 0);
        pConfig->mSubEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_SubEncodeFrameRate, 0);
        pConfig->mSubEncodeBitrate = GetConfParaInt(&stConfParser, CFG_SubEncodeBitrate, 0);
        pConfig->mSubEncppSharpAttenCoefPer = 100 * pConfig->mSubEncodeWidth / pConfig->mMainEncodeWidth;
        pConfig->mSubEncppEnable = GetConfParaInt(&stConfParser, CFG_SubEncppEnable, 0);
        pConfig->mSubVeRefFrameLbcMode = GetConfParaInt(&stConfParser, CFG_SubVeRefFrameLbcMode, 0);
        pConfig->mSubKeyFrameInterval = GetConfParaInt(&stConfParser, CFG_SubKeyFrameInterval, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubFilePath, NULL);
        strncpy(pConfig->mSubFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_SubSaveOneFileDuration, 0);
        pConfig->mSubSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_SubSaveMaxFileCnt, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubDrawOSDText, NULL);
        strncpy(pConfig->mSubDrawOSDText, ptr, MAX_FILE_PATH_SIZE);
        // sub tdm raw
        pConfig->mSubIspTdmRawProcessType = GetConfParaInt(&stConfParser, CFG_SubIspTdmRawProcessType, -1);
        pConfig->mSubIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_SubIspTdmRxBufNum, 5);
        pConfig->mSubIspTdmRawProcessFrameCntMin = GetConfParaInt(&stConfParser, CFG_SubIspTdmRawProcessFrameCntMin, 0);
        pConfig->mSubIspTdmRawProcessFrameCntMax = GetConfParaInt(&stConfParser, CFG_SubIspTdmRawProcessFrameCntMax, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubIspTdmRawFilePath, NULL);
        strncpy(pConfig->mSubIspTdmRawFilePath, ptr, MAX_FILE_PATH_SIZE);
        // sub nn
        pConfig->mSubNnEnable = GetConfParaInt(&stConfParser, CFG_SubNnEnable, 0);
        pConfig->mSubNnNbgType = GetConfParaInt(&stConfParser, CFG_SubNnNbgType, 0);
        pConfig->mSubNnVipp = GetConfParaInt(&stConfParser, CFG_SubNnVipp, 0);
        pConfig->mSubNnViBufNum = GetConfParaInt(&stConfParser, CFG_SubNnViBufNum, 0);
        pConfig->mSubNnSrcFrameRate = GetConfParaInt(&stConfParser, CFG_SubNnSrcFrameRate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubNnNbgFilePath, NULL);
        strncpy(pConfig->mSubNnNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubNnDrawOrlEnable = GetConfParaInt(&stConfParser, CFG_SubNnDrawOrlEnable, 0);
        // sub aiisp
        pConfig->mSubAiIspEnable = GetConfParaInt(&stConfParser, CFG_SubAiIspEnable, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubAiIspLutNbgFilePath, NULL);
        strncpy(pConfig->mSubAiIspLutNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubAiIspNbgFilePath, NULL);
        strncpy(pConfig->mSubAiIspNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubAiIspModelVersion = GetConfParaInt(&stConfParser, CFG_SubAiIspModelVersion, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubAiIspCfgBinPath, NULL);
        strncpy(pConfig->mSubAiIspCfgBinPath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubAiIspWidth = GetConfParaInt(&stConfParser, CFG_SubAiIspWidth, 0);
        pConfig->mSubAiIspHeight = GetConfParaInt(&stConfParser, CFG_SubAiIspHeight, 0);
        pConfig->mSubAiIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_SubAiIspTdmRxBufNum, 0);
        pConfig->mSubAiIspMode = GetConfParaInt(&stConfParser, CFG_SubAiIspMode, 0);
        pConfig->mSubAiIspAutoSwitchEnable = GetConfParaInt(&stConfParser, CFG_SubAiIspAutoSwitchEnable, 0);
        pConfig->mSubAiIspSwitchInterval = GetConfParaInt(&stConfParser, CFG_SubAiIspSwitchInterval, 0);
        int nSubAiIspSwitchIntervalDefault = pConfig->mSubEncodeFrameRate * 5;
        if (pConfig->mSubAiIspSwitchInterval > 0 && pConfig->mSubAiIspSwitchInterval < nSubAiIspSwitchIntervalDefault)
        {
            alogd("sub aiisp switch interval %d is too small, force to set default %d", pConfig->mSubAiIspSwitchInterval, nSubAiIspSwitchIntervalDefault);
            pConfig->mSubAiIspSwitchInterval = nSubAiIspSwitchIntervalDefault;
        }
        pConfig->mSubAiIspSwitchCase = GetConfParaInt(&stConfParser, CFG_SubAiIspSwitchCase, 0);
        pConfig->mSubAiIspSwitchDropFrameNum = GetConfParaInt(&stConfParser, CFG_SubAiIspSwitchDropFrameNum, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_SubAiIspCfgBinPath2, NULL);
        strncpy(pConfig->mSubAiIspCfgBinPath2, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSubAiIspReserve0 = GetConfParaInt(&stConfParser, CFG_SubAiIspReserve0, 0);
        pConfig->mSubAiIspReserve1 = GetConfParaInt(&stConfParser, CFG_SubAiIspReserve1, 0);
        pConfig->mSubAiIspReserve2 = GetConfParaInt(&stConfParser, CFG_SubAiIspReserve2, 0);
        // sub 2nd stream
        pConfig->mSub2ndEnable = GetConfParaInt(&stConfParser, CFG_Sub2ndEnable, 0);
        pConfig->mSub2ndVipp = GetConfParaInt(&stConfParser, CFG_Sub2ndVipp, 0);
        pConfig->mSub2ndViChn = GetConfParaInt(&stConfParser, CFG_Sub2ndViChn, 0);
        pConfig->mSub2ndSrcWidth = GetConfParaInt(&stConfParser, CFG_Sub2ndSrcWidth, 0);
        pConfig->mSub2ndSrcHeight = GetConfParaInt(&stConfParser, CFG_Sub2ndSrcHeight, 0);
        pConfig->mSub2ndPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_Sub2ndPixelFormat);
        pConfig->mSub2ndViBufNum = GetConfParaInt(&stConfParser, CFG_Sub2ndViBufNum, 0);
        pConfig->mSub2ndSrcFrameRate = GetConfParaInt(&stConfParser, CFG_Sub2ndSrcFrameRate, 0);
        pConfig->mSub2ndVEncChn = GetConfParaInt(&stConfParser, CFG_Sub2ndVEncChn, 0);
        pConfig->mSub2ndEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_Sub2ndEncodeType);
        pConfig->mSub2ndEncodeWidth = GetConfParaInt(&stConfParser, CFG_Sub2ndEncodeWidth, 0);
        pConfig->mSub2ndEncodeHeight = GetConfParaInt(&stConfParser, CFG_Sub2ndEncodeHeight, 0);
        pConfig->mSub2ndEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_Sub2ndEncodeFrameRate, 0);
        pConfig->mSub2ndEncodeBitrate = GetConfParaInt(&stConfParser, CFG_Sub2ndEncodeBitrate, 0);
        pConfig->mSub2ndEncppSharpAttenCoefPer = 100 * pConfig->mSub2ndEncodeWidth / pConfig->mMainEncodeWidth;
        pConfig->mSub2ndEncppEnable = GetConfParaInt(&stConfParser, CFG_Sub2ndEncppEnable, 0);
        pConfig->mSub2ndVeRefFrameLbcMode = GetConfParaInt(&stConfParser, CFG_Sub2ndVeRefFrameLbcMode, 0);
        pConfig->mSub2ndKeyFrameInterval = GetConfParaInt(&stConfParser, CFG_Sub2ndKeyFrameInterval, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_Sub2ndFilePath, NULL);
        strncpy(pConfig->mSub2ndFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mSub2ndSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_Sub2ndSaveOneFileDuration, 0);
        pConfig->mSub2ndSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_Sub2ndSaveMaxFileCnt, 0);
        // three stream
        pConfig->mThreeEnable = GetConfParaInt(&stConfParser, CFG_ThreeEnable, 0);
        pConfig->mThreeRtspID = GetConfParaInt(&stConfParser, CFG_ThreeRtspID, 0);
        pConfig->mThreeIsp = GetConfParaInt(&stConfParser, CFG_ThreeIsp, 0);
        pConfig->mThreeIspD3dLbcRatio = GetConfParaInt(&stConfParser, CFG_ThreeIspD3dLbcRatio, 0);
        pConfig->mThreeVipp = GetConfParaInt(&stConfParser, CFG_ThreeVipp, 0);
        pConfig->mThreeViChn = GetConfParaInt(&stConfParser, CFG_ThreeViChn, 0);
        pConfig->mThreeLdciUseExtBufEnable = GetConfParaInt(&stConfParser, CFG_ThreeLdciUseExtBufEnable, 0);
        pConfig->mThreeLdciVipp = GetConfParaInt(&stConfParser, CFG_ThreeLdciVipp, 0);
        pConfig->mThreeSrcWidth = GetConfParaInt(&stConfParser, CFG_ThreeSrcWidth, 0);
        pConfig->mThreeSrcHeight = GetConfParaInt(&stConfParser, CFG_ThreeSrcHeight, 0);
        pConfig->mThreePixelFormat = getPicFormatFromConfig(&stConfParser, CFG_ThreePixelFormat);
        pConfig->mThreeWdrEnable = GetConfParaInt(&stConfParser, CFG_ThreeWdrEnable, 0);
        pConfig->mThreeViBufNum = GetConfParaInt(&stConfParser, CFG_ThreeViBufNum, 0);
        pConfig->mThreeSrcFrameRate = GetConfParaInt(&stConfParser, CFG_ThreeSrcFrameRate, 0);
        pConfig->mThreeVEncChn = GetConfParaInt(&stConfParser, CFG_ThreeVEncChn, 0);
        pConfig->mThreeEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_ThreeEncodeType);
        pConfig->mThreeEncodeWidth = GetConfParaInt(&stConfParser, CFG_ThreeEncodeWidth, 0);
        pConfig->mThreeEncodeHeight = GetConfParaInt(&stConfParser, CFG_ThreeEncodeHeight, 0);
        pConfig->mThreeEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_ThreeEncodeFrameRate, 0);
        pConfig->mThreeEncodeBitrate = GetConfParaInt(&stConfParser, CFG_ThreeEncodeBitrate, 0);
        pConfig->mThreeOnlineEnable = GetConfParaInt(&stConfParser, CFG_ThreeOnlineEnable, 0);
        pConfig->mThreeOnlineShareBufNum = GetConfParaInt(&stConfParser, CFG_ThreeOnlineShareBufNum, 0);
        pConfig->mThreeEncppEnable = GetConfParaInt(&stConfParser, CFG_ThreeEncppEnable, 0);
        pConfig->mThreeVeRefFrameLbcMode = GetConfParaInt(&stConfParser, CFG_ThreeVeRefFrameLbcMode, 0);
        pConfig->mThreeKeyFrameInterval = GetConfParaInt(&stConfParser, CFG_ThreeKeyFrameInterval, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_ThreeFilePath, NULL);
        strncpy(pConfig->mThreeFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mThreeSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_ThreeSaveOneFileDuration, 0);
        pConfig->mThreeSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_ThreeSaveMaxFileCnt, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_ThreeDrawOSDText, NULL);
        strncpy(pConfig->mThreeDrawOSDText, ptr, MAX_FILE_PATH_SIZE);
        // three tdm raw
        pConfig->mThreeIspTdmRawProcessType = GetConfParaInt(&stConfParser, CFG_ThreeIspTdmRawProcessType, -1);
        pConfig->mThreeIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_ThreeIspTdmRxBufNum, 5);
        pConfig->mThreeIspTdmRawProcessFrameCntMin = GetConfParaInt(&stConfParser, CFG_ThreeIspTdmRawProcessFrameCntMin, 0);
        pConfig->mThreeIspTdmRawProcessFrameCntMax = GetConfParaInt(&stConfParser, CFG_ThreeIspTdmRawProcessFrameCntMax, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_ThreeIspTdmRawFilePath, NULL);
        strncpy(pConfig->mThreeIspTdmRawFilePath, ptr, MAX_FILE_PATH_SIZE);
        // three nn
        pConfig->mThreeNnEnable = GetConfParaInt(&stConfParser, CFG_ThreeNnEnable, 0);
        pConfig->mThreeNnNbgType = GetConfParaInt(&stConfParser, CFG_ThreeNnNbgType, 0);
        pConfig->mThreeNnVipp = GetConfParaInt(&stConfParser, CFG_ThreeNnVipp, 0);
        pConfig->mThreeNnViBufNum = GetConfParaInt(&stConfParser, CFG_ThreeNnViBufNum, 0);
        pConfig->mThreeNnSrcFrameRate = GetConfParaInt(&stConfParser, CFG_ThreeNnSrcFrameRate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_ThreeNnNbgFilePath, NULL);
        strncpy(pConfig->mThreeNnNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mThreeNnDrawOrlEnable = GetConfParaInt(&stConfParser, CFG_ThreeNnDrawOrlEnable, 0);
        // three aiisp
        pConfig->mThreeAiIspEnable = GetConfParaInt(&stConfParser, CFG_ThreeAiIspEnable, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_ThreeAiIspLutNbgFilePath, NULL);
        strncpy(pConfig->mThreeAiIspLutNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_ThreeAiIspNbgFilePath, NULL);
        strncpy(pConfig->mThreeAiIspNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mThreeAiIspModelVersion = GetConfParaInt(&stConfParser, CFG_ThreeAiIspModelVersion, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_ThreeAiIspCfgBinPath, NULL);
        strncpy(pConfig->mThreeAiIspCfgBinPath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mThreeAiIspWidth = GetConfParaInt(&stConfParser, CFG_ThreeAiIspWidth, 0);
        pConfig->mThreeAiIspHeight = GetConfParaInt(&stConfParser, CFG_ThreeAiIspHeight, 0);
        pConfig->mThreeAiIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_ThreeAiIspTdmRxBufNum, 0);
        pConfig->mThreeAiIspMode = GetConfParaInt(&stConfParser, CFG_ThreeAiIspMode, 0);
        pConfig->mThreeAiIspAutoSwitchEnable = GetConfParaInt(&stConfParser, CFG_ThreeAiIspAutoSwitchEnable, 0);
        pConfig->mThreeAiIspSwitchInterval = GetConfParaInt(&stConfParser, CFG_ThreeAiIspSwitchInterval, 0);
        int nThreeAiIspSwitchIntervalDefault = pConfig->mThreeEncodeFrameRate * 5;
        if (pConfig->mThreeAiIspSwitchInterval > 0 && pConfig->mThreeAiIspSwitchInterval < nThreeAiIspSwitchIntervalDefault)
        {
            alogd("three aiisp switch interval %d is too small, force to set default %d", pConfig->mThreeAiIspSwitchInterval, nThreeAiIspSwitchIntervalDefault);
            pConfig->mThreeAiIspSwitchInterval = nThreeAiIspSwitchIntervalDefault;
        }
        pConfig->mThreeAiIspSwitchCase = GetConfParaInt(&stConfParser, CFG_ThreeAiIspSwitchCase, 0);
        pConfig->mThreeAiIspSwitchDropFrameNum = GetConfParaInt(&stConfParser, CFG_ThreeAiIspSwitchDropFrameNum, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_ThreeAiIspCfgBinPath2, NULL);
        strncpy(pConfig->mThreeAiIspCfgBinPath2, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mThreeAiIspReserve0 = GetConfParaInt(&stConfParser, CFG_ThreeAiIspReserve0, 0);
        pConfig->mThreeAiIspReserve1 = GetConfParaInt(&stConfParser, CFG_ThreeAiIspReserve1, 0);
        pConfig->mThreeAiIspReserve2 = GetConfParaInt(&stConfParser, CFG_ThreeAiIspReserve2, 0);
        // three 2nd stream
        pConfig->mThree2ndEnable = GetConfParaInt(&stConfParser, CFG_Three2ndEnable, 0);
        pConfig->mThree2ndVipp = GetConfParaInt(&stConfParser, CFG_Three2ndVipp, 0);
        pConfig->mThree2ndViChn = GetConfParaInt(&stConfParser, CFG_Three2ndViChn, 0);
        pConfig->mThree2ndSrcWidth = GetConfParaInt(&stConfParser, CFG_Three2ndSrcWidth, 0);
        pConfig->mThree2ndSrcHeight = GetConfParaInt(&stConfParser, CFG_Three2ndSrcHeight, 0);
        pConfig->mThree2ndPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_Three2ndPixelFormat);
        pConfig->mThree2ndViBufNum = GetConfParaInt(&stConfParser, CFG_Three2ndViBufNum, 0);
        pConfig->mThree2ndSrcFrameRate = GetConfParaInt(&stConfParser, CFG_Three2ndSrcFrameRate, 0);
        pConfig->mThree2ndVEncChn = GetConfParaInt(&stConfParser, CFG_Three2ndVEncChn, 0);
        pConfig->mThree2ndEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_Three2ndEncodeType);
        pConfig->mThree2ndEncodeWidth = GetConfParaInt(&stConfParser, CFG_Three2ndEncodeWidth, 0);
        pConfig->mThree2ndEncodeHeight = GetConfParaInt(&stConfParser, CFG_Three2ndEncodeHeight, 0);
        pConfig->mThree2ndEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_Three2ndEncodeFrameRate, 0);
        pConfig->mThree2ndEncodeBitrate = GetConfParaInt(&stConfParser, CFG_Three2ndEncodeBitrate, 0);
        pConfig->mThree2ndEncppSharpAttenCoefPer = 100 * pConfig->mThree2ndEncodeWidth / pConfig->mThreeEncodeWidth;
        pConfig->mThree2ndEncppEnable = GetConfParaInt(&stConfParser, CFG_Three2ndEncppEnable, 0);
        pConfig->mThree2ndVeRefFrameLbcMode = GetConfParaInt(&stConfParser, CFG_Three2ndVeRefFrameLbcMode, 0);
        pConfig->mThree2ndKeyFrameInterval = GetConfParaInt(&stConfParser, CFG_Three2ndKeyFrameInterval, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_Three2ndFilePath, NULL);
        strncpy(pConfig->mThree2ndFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mThree2ndSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_Three2ndSaveOneFileDuration, 0);
        pConfig->mThree2ndSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_Three2ndSaveMaxFileCnt, 0);
        // four stream
        pConfig->mFourEnable = GetConfParaInt(&stConfParser, CFG_FourEnable, 0);
        pConfig->mFourRtspID = GetConfParaInt(&stConfParser, CFG_FourRtspID, 0);
        pConfig->mFourIsp = GetConfParaInt(&stConfParser, CFG_FourIsp, 0);
        pConfig->mFourIspD3dLbcRatio = GetConfParaInt(&stConfParser, CFG_FourIspD3dLbcRatio, 0);
        pConfig->mFourVipp = GetConfParaInt(&stConfParser, CFG_FourVipp, 0);
        pConfig->mFourViChn = GetConfParaInt(&stConfParser, CFG_FourViChn, 0);
        pConfig->mFourLdciUseExtBufEnable = GetConfParaInt(&stConfParser, CFG_FourLdciUseExtBufEnable, 0);
        pConfig->mFourLdciVipp = GetConfParaInt(&stConfParser, CFG_FourLdciVipp, 0);
        pConfig->mFourSrcWidth = GetConfParaInt(&stConfParser, CFG_FourSrcWidth, 0);
        pConfig->mFourSrcHeight = GetConfParaInt(&stConfParser, CFG_FourSrcHeight, 0);
        pConfig->mFourPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_FourPixelFormat);
        pConfig->mFourWdrEnable = GetConfParaInt(&stConfParser, CFG_FourWdrEnable, 0);
        pConfig->mFourViBufNum = GetConfParaInt(&stConfParser, CFG_FourViBufNum, 0);
        pConfig->mFourSrcFrameRate = GetConfParaInt(&stConfParser, CFG_FourSrcFrameRate, 0);
        pConfig->mFourVEncChn = GetConfParaInt(&stConfParser, CFG_FourVEncChn, 0);
        pConfig->mFourEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_FourEncodeType);
        pConfig->mFourEncodeWidth = GetConfParaInt(&stConfParser, CFG_FourEncodeWidth, 0);
        pConfig->mFourEncodeHeight = GetConfParaInt(&stConfParser, CFG_FourEncodeHeight, 0);
        pConfig->mFourEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_FourEncodeFrameRate, 0);
        pConfig->mFourEncodeBitrate = GetConfParaInt(&stConfParser, CFG_FourEncodeBitrate, 0);
        pConfig->mFourOnlineEnable = GetConfParaInt(&stConfParser, CFG_FourOnlineEnable, 0);
        pConfig->mFourOnlineShareBufNum = GetConfParaInt(&stConfParser, CFG_FourOnlineShareBufNum, 0);
        pConfig->mFourEncppEnable = GetConfParaInt(&stConfParser, CFG_FourEncppEnable, 0);
        pConfig->mFourVeRefFrameLbcMode = GetConfParaInt(&stConfParser, CFG_FourVeRefFrameLbcMode, 0);
        pConfig->mFourKeyFrameInterval = GetConfParaInt(&stConfParser, CFG_FourKeyFrameInterval, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_FourFilePath, NULL);
        strncpy(pConfig->mFourFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mFourSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_FourSaveOneFileDuration, 0);
        pConfig->mFourSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_FourSaveMaxFileCnt, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_FourDrawOSDText, NULL);
        strncpy(pConfig->mFourDrawOSDText, ptr, MAX_FILE_PATH_SIZE);
        // four tdm raw
        pConfig->mFourIspTdmRawProcessType = GetConfParaInt(&stConfParser, CFG_FourIspTdmRawProcessType, -1);
        pConfig->mFourIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_FourIspTdmRxBufNum, 5);
        pConfig->mFourIspTdmRawProcessFrameCntMin = GetConfParaInt(&stConfParser, CFG_FourIspTdmRawProcessFrameCntMin, 0);
        pConfig->mFourIspTdmRawProcessFrameCntMax = GetConfParaInt(&stConfParser, CFG_FourIspTdmRawProcessFrameCntMax, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_FourIspTdmRawFilePath, NULL);
        strncpy(pConfig->mFourIspTdmRawFilePath, ptr, MAX_FILE_PATH_SIZE);
        // four nn
        pConfig->mFourNnEnable = GetConfParaInt(&stConfParser, CFG_FourNnEnable, 0);
        pConfig->mFourNnNbgType = GetConfParaInt(&stConfParser, CFG_FourNnNbgType, 0);
        pConfig->mFourNnVipp = GetConfParaInt(&stConfParser, CFG_FourNnVipp, 0);
        pConfig->mFourNnViBufNum = GetConfParaInt(&stConfParser, CFG_FourNnViBufNum, 0);
        pConfig->mFourNnSrcFrameRate = GetConfParaInt(&stConfParser, CFG_FourNnSrcFrameRate, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_FourNnNbgFilePath, NULL);
        strncpy(pConfig->mFourNnNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mFourNnDrawOrlEnable = GetConfParaInt(&stConfParser, CFG_FourNnDrawOrlEnable, 0);
        // four aiisp
        pConfig->mFourAiIspEnable = GetConfParaInt(&stConfParser, CFG_FourAiIspEnable, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_FourAiIspLutNbgFilePath, NULL);
        strncpy(pConfig->mFourAiIspLutNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_FourAiIspNbgFilePath, NULL);
        strncpy(pConfig->mFourAiIspNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mFourAiIspModelVersion = GetConfParaInt(&stConfParser, CFG_FourAiIspModelVersion, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_FourAiIspCfgBinPath, NULL);
        strncpy(pConfig->mFourAiIspCfgBinPath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mFourAiIspWidth = GetConfParaInt(&stConfParser, CFG_FourAiIspWidth, 0);
        pConfig->mFourAiIspHeight = GetConfParaInt(&stConfParser, CFG_FourAiIspHeight, 0);
        pConfig->mFourAiIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_FourAiIspTdmRxBufNum, 0);
        pConfig->mFourAiIspMode = GetConfParaInt(&stConfParser, CFG_FourAiIspMode, 0);
        pConfig->mFourAiIspAutoSwitchEnable = GetConfParaInt(&stConfParser, CFG_FourAiIspAutoSwitchEnable, 0);
        pConfig->mFourAiIspSwitchInterval = GetConfParaInt(&stConfParser, CFG_FourAiIspSwitchInterval, 0);
        int nFourAiIspSwitchIntervalDefault = pConfig->mFourEncodeFrameRate * 5;
        if (pConfig->mFourAiIspSwitchInterval > 0 && pConfig->mFourAiIspSwitchInterval < nFourAiIspSwitchIntervalDefault)
        {
            alogd("four aiisp switch interval %d is too small, force to set default %d", pConfig->mFourAiIspSwitchInterval, nFourAiIspSwitchIntervalDefault);
            pConfig->mFourAiIspSwitchInterval = nFourAiIspSwitchIntervalDefault;
        }
        pConfig->mFourAiIspSwitchCase = GetConfParaInt(&stConfParser, CFG_FourAiIspSwitchCase, 0);
        pConfig->mFourAiIspSwitchDropFrameNum = GetConfParaInt(&stConfParser, CFG_FourAiIspSwitchDropFrameNum, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_FourAiIspCfgBinPath2, NULL);
        strncpy(pConfig->mFourAiIspCfgBinPath2, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mFourAiIspReserve0 = GetConfParaInt(&stConfParser, CFG_FourAiIspReserve0, 0);
        pConfig->mFourAiIspReserve1 = GetConfParaInt(&stConfParser, CFG_FourAiIspReserve1, 0);
        pConfig->mFourAiIspReserve2 = GetConfParaInt(&stConfParser, CFG_FourAiIspReserve2, 0);
        // four 2nd stream
        pConfig->mFour2ndEnable = GetConfParaInt(&stConfParser, CFG_Four2ndEnable, 0);
        pConfig->mFour2ndVipp = GetConfParaInt(&stConfParser, CFG_Four2ndVipp, 0);
        pConfig->mFour2ndViChn = GetConfParaInt(&stConfParser, CFG_Four2ndViChn, 0);
        pConfig->mFour2ndSrcWidth = GetConfParaInt(&stConfParser, CFG_Four2ndSrcWidth, 0);
        pConfig->mFour2ndSrcHeight = GetConfParaInt(&stConfParser, CFG_Four2ndSrcHeight, 0);
        pConfig->mFour2ndPixelFormat = getPicFormatFromConfig(&stConfParser, CFG_Four2ndPixelFormat);
        pConfig->mFour2ndViBufNum = GetConfParaInt(&stConfParser, CFG_Four2ndViBufNum, 0);
        pConfig->mFour2ndSrcFrameRate = GetConfParaInt(&stConfParser, CFG_Four2ndSrcFrameRate, 0);
        pConfig->mFour2ndVEncChn = GetConfParaInt(&stConfParser, CFG_Four2ndVEncChn, 0);
        pConfig->mFour2ndEncodeType = getEncoderTypeFromConfig(&stConfParser, CFG_Four2ndEncodeType);
        pConfig->mFour2ndEncodeWidth = GetConfParaInt(&stConfParser, CFG_Four2ndEncodeWidth, 0);
        pConfig->mFour2ndEncodeHeight = GetConfParaInt(&stConfParser, CFG_Four2ndEncodeHeight, 0);
        pConfig->mFour2ndEncodeFrameRate = GetConfParaInt(&stConfParser, CFG_Four2ndEncodeFrameRate, 0);
        pConfig->mFour2ndEncodeBitrate = GetConfParaInt(&stConfParser, CFG_Four2ndEncodeBitrate, 0);
        pConfig->mFour2ndEncppSharpAttenCoefPer = 100 * pConfig->mFour2ndEncodeWidth / pConfig->mFourEncodeWidth;
        pConfig->mFour2ndEncppEnable = GetConfParaInt(&stConfParser, CFG_Four2ndEncppEnable, 0);
        pConfig->mFour2ndVeRefFrameLbcMode = GetConfParaInt(&stConfParser, CFG_Four2ndVeRefFrameLbcMode, 0);
        pConfig->mFour2ndKeyFrameInterval = GetConfParaInt(&stConfParser, CFG_Four2ndKeyFrameInterval, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_Four2ndFilePath, NULL);
        strncpy(pConfig->mFour2ndFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mFour2ndSaveOneFileDuration = GetConfParaInt(&stConfParser, CFG_Four2ndSaveOneFileDuration, 0);
        pConfig->mFour2ndSaveMaxFileCnt = GetConfParaInt(&stConfParser, CFG_Four2ndSaveMaxFileCnt, 0);

        alogd("MainEn:%d, Main2ndEn:%d, SubEn:%d, Sub2ndEn:%d, IspAndVeLinkageEn:%d, AdaptEn:%d, LensMoveMaxQp:%d, WbYuvEn:%d, TestDuration:%d",
            pConfig->mMainEnable, pConfig->mMain2ndEnable, pConfig->mSubEnable, pConfig->mSub2ndEnable,
            pConfig->mIspAndVeLinkageEnable, pConfig->mCameraAdaptiveMovingAndStaticEnable, pConfig->mVencLensMovingMaxQp,
            pConfig->mWbYuvEnable, pConfig->mTestDuration);

        destroyConfParser(&stConfParser);
    }
    else
    {
        alogw("user not set config file, use default configs.");
    }

    if (-1 != pConfig->mMainRtspID && pConfig->mMainRtspID == pConfig->mSubRtspID)
    {
        aloge("fatal error, same MainRtspID:%d, SubRtspID:%d", pConfig->mMainRtspID, pConfig->mSubRtspID);
        return FAILURE;
    }

    return SUCCESS;
}

static VencStreamContext *getStreamContext(SampleSmartIPCDemoContext *pContext, VENC_CHN mVEncChn)
{
    VencStreamContext *pStreamContext = NULL;

    if (mVEncChn == pContext->mMainStream.mVEncChn)
    {
        pStreamContext = &pContext->mMainStream;
    }
    else if (mVEncChn == pContext->mMain2ndStream.mVEncChn)
    {
        pStreamContext = &pContext->mMain2ndStream;
    }
    else if (mVEncChn == pContext->mSubStream.mVEncChn)
    {
        pStreamContext = &pContext->mSubStream;
    }
    else if (mVEncChn == pContext->mSub2ndStream.mVEncChn)
    {
        pStreamContext = &pContext->mSub2ndStream;
    }
    else if (mVEncChn == pContext->mThreeStream.mVEncChn)
    {
        pStreamContext = &pContext->mThreeStream;
    }
    else if (mVEncChn == pContext->mThree2ndStream.mVEncChn)
    {
        pStreamContext = &pContext->mThree2ndStream;
    }
    else if (mVEncChn == pContext->mFourStream.mVEncChn)
    {
        pStreamContext = &pContext->mFourStream;
    }
    else if (mVEncChn == pContext->mFour2ndStream.mVEncChn)
    {
        pStreamContext = &pContext->mFour2ndStream;
    }
    else
    {
        aloge("fatal error! VencChn[%d] is not match, set pStreamContext = NULL!", mVEncChn);
        pStreamContext = NULL;
    }

    return pStreamContext;
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)cookie;
    ERRORTYPE ret = 0;

    if (MOD_ID_VIU == pChn->mModId)
    {
        switch(event)
        {
            case MPP_EVENT_VI_TIMEOUT:
            {
                aloge("receive vi timeout. vipp:%d, chn:%d", pChn->mDevId, pChn->mChnId);
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                stCmdMsg.command = Vi_Timeout;
                stCmdMsg.para0 = pChn->mDevId;
                putMessageWithData(&pContext->mMsgQueue, &stCmdMsg);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    else if (MOD_ID_VENC == pChn->mModId)
    {
        VENC_CHN mVEncChn = pChn->mChnId;

        VencStreamContext *pStreamContext = getStreamContext(pContext, mVEncChn);
        if (NULL == pStreamContext)
        {
            aloge("fatal error! VenChn[%d] pStreamContext is NULL!", mVEncChn);
            return -1;
        }

        switch(event)
        {
            /*case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                Isp2VeLinkageParam stIsp2Ve;
                memset(&stIsp2Ve, 0, sizeof(Isp2VeLinkageParam));
                stIsp2Ve.mIspAndVeLinkageEnable = pStreamContext->mIspAndVeLinkageEnable;
                stIsp2Ve.mCameraAdaptiveMovingAndStaticEnable = pStreamContext->mCameraAdaptiveMovingAndStaticEnable;
                stIsp2Ve.mVEncChn = mVEncChn;
                stIsp2Ve.mVipp = pStreamContext->mVipp;
                stIsp2Ve.pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                stIsp2Ve.nEncppSharpAttenCoefPer = pStreamContext->mEncppSharpAttenCoefPer;
                int ret = setIsp2VeLinkageParam(&stIsp2Ve);
                if (ret)
                {
                    aloge("fatal error, VEncChn[%d] set Isp2VeLinkageParam failed! ret=%d", mVEncChn, ret);
                    return -1;
                }
                break;
            }
            case MPP_EVENT_LINKAGE_VE2ISP_PARAM:
            {
                Ve2IspLinkageParam stVe2Isp;
                memset(&stVe2Isp, 0, sizeof(Ve2IspLinkageParam));
                stVe2Isp.mIspAndVeLinkageEnable = pStreamContext->mIspAndVeLinkageEnable;
                stVe2Isp.mVEncChn = mVEncChn;
                stVe2Isp.mVipp = pStreamContext->mVipp;
                stVe2Isp.p2Ve2IspParam = (VencVe2IspParam *)pEventData;
                int ret = setVe2IspLinkageParam(&stVe2Isp);
                if (ret)
                {
                    aloge("fatal error, VEncChn[%d] set Ve2IspLinkageParam failed! ret=%d", mVEncChn, ret);
                    return -1;
                }
                break;
            }*/
            case MPP_EVENT_LINKAGE_ISP2VE_PARAM_EXTRA:
            {
                VENC_Isp2VeExtraParam *pExtraParam = (VENC_Isp2VeExtraParam *)pEventData;
                if (pContext->mConfigPara.mCameraAdaptiveMovingAndStaticEnable)
                {
                    pExtraParam->eEnCameraMove = CAMERA_ADAPTIVE_MOVING_AND_STATIC;
                }
                else
                {
                    pExtraParam->eEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }	

    return SUCCESS;
}

static void configMainStream(VencStreamContext *pStreamContext, SampleSmartIPCDemoConfig *pConfig)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (!pStreamContext || !pConfig)
    {
        aloge("fatal error! invalid input params! %p,%p", pStreamContext, pConfig);
    }

    pStreamContext->mIsp = pConfig->mMainIsp;
    pStreamContext->mVipp = pConfig->mMainVipp;
    if (pConfig->mMainOnlineEnable && (0 != pStreamContext->mVipp))
    {
        aloge("fatal error! main vipp %d is wrong, only vipp0 support online.", pStreamContext->mVipp);
    }

    pStreamContext->mViChn = pConfig->mMainViChn; //0;
    pStreamContext->mVEncChn = pConfig->mMainVEncChn; //0;

    if (pConfig->mMainOnlineEnable)
    {
        pStreamContext->mViAttr.mOnlineEnable = 1;
        pStreamContext->mViAttr.mOnlineShareBufNum = pConfig->mMainOnlineShareBufNum;//BK_TWO_BUFFER;
        pStreamContext->mVEncChnAttr.VeAttr.mOnlineEnable = 1;
        pStreamContext->mVEncChnAttr.VeAttr.mOnlineShareBufNum = pConfig->mMainOnlineShareBufNum;//BK_TWO_BUFFER;
    }
    alogd("main vipp%d ve_online_en:%d, dma_buf_num:%d, venc ch%d OnlineEnable:%d, OnlineShareBufNum:%d",
        pStreamContext->mVipp, pStreamContext->mViAttr.mOnlineEnable, pStreamContext->mViAttr.mOnlineShareBufNum,
        pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.VeAttr.mOnlineEnable, pStreamContext->mVEncChnAttr.VeAttr.mOnlineShareBufNum);

    pStreamContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pStreamContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pStreamContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pConfig->mMainPixelFormat);
    pStreamContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pStreamContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pStreamContext->mViAttr.format.width = pConfig->mMainSrcWidth;
    pStreamContext->mViAttr.format.height = pConfig->mMainSrcHeight;
    pStreamContext->mViAttr.fps = pConfig->mMainSrcFrameRate;
    pStreamContext->mViAttr.use_current_win = 0;
    pStreamContext->mViAttr.nbufs = pConfig->mMainViBufNum;
    pStreamContext->mViAttr.nplanes = 2;
    pStreamContext->mViAttr.wdr_mode = pConfig->mMainWdrEnable;
    pStreamContext->mViAttr.drop_frame_num = 0;
    if (pConfig->mMainAiIspEnable)
    {
        pStreamContext->mViAttr.tdm_rxbuf_cnt = pConfig->mMainAiIspTdmRxBufNum;
    }
    else if (-1 != pConfig->mMainIspTdmRawProcessType)
    {
        pStreamContext->mViAttr.tdm_rxbuf_cnt = pConfig->mMainIspTdmRxBufNum;
    }

    pStreamContext->mVEncChnAttr.VeAttr.Type = pConfig->mMainEncodeType;
    pStreamContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pConfig->mMainKeyFrameInterval;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicWidth = pConfig->mMainSrcWidth;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicHeight = pConfig->mMainSrcHeight;
    pStreamContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pStreamContext->mVEncChnAttr.VeAttr.PixelFormat = pConfig->mMainPixelFormat;
    pStreamContext->mVEncChnAttr.VeAttr.mColorSpace = pStreamContext->mViAttr.format.colorspace;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = pConfig->mMainVeRefFrameLbcMode;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = pConfig->mVeRecRefBufReduceEnable;

    pStreamContext->mVEncChnAttr.VeAttr.mVbrOptEnable = pConfig->mVbrOptEnable;

    pStreamContext->mVEncChnAttr.RcAttr.mProductMode = pConfig->mProductMode;
    //pStreamContext->mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pStreamContext->mEncppSharpAttenCoefPer = 100;
    alogd("main EncppSharpAttenCoefPer: %d%%", pStreamContext->mEncppSharpAttenCoefPer);
    pStreamContext->mIspAndVeLinkageEnable = pConfig->mIspAndVeLinkageEnable;
    pStreamContext->mCameraAdaptiveMovingAndStaticEnable = pConfig->mCameraAdaptiveMovingAndStaticEnable;

    pStreamContext->mViAttr.mbEncppEnable = pConfig->mMainEncppEnable;
    pStreamContext->mVEncChnAttr.EncppAttr.eEncppSharpSetting = pConfig->mMainEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    if (pConfig->mMainSrcFrameRate)
    {
        vbvThreshSize = pConfig->mMainEncodeBitrate/8/pConfig->mMainSrcFrameRate*15;
    }
    vbvBufSize = pConfig->mMainEncodeBitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("main vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    VencRateCtrlConfig stRcConfig;
    memset(&stRcConfig, 0, sizeof(VencRateCtrlConfig));
    stRcConfig.mEncodeType = pConfig->mMainEncodeType;
    stRcConfig.mEncodeWidth = pConfig->mMainEncodeWidth;
    stRcConfig.mEncodeHeight = pConfig->mMainEncodeHeight;
    stRcConfig.mSrcFrameRate = pConfig->mMainSrcFrameRate;
    stRcConfig.mDstFrameRate = pConfig->mMainEncodeFrameRate;
    stRcConfig.mEncodeBitrate = pConfig->mMainEncodeBitrate;
    stRcConfig.mRcMode = getRcModeFromConfig(pConfig->mRcMode, pConfig->mMainEncodeType);
    stRcConfig.mMaxIQp = pConfig->mMaxIQp;
    stRcConfig.mMinIQp = pConfig->mMinIQp;
    stRcConfig.mMaxPQp = pConfig->mMaxPQp;
    stRcConfig.mMinPQp = pConfig->mMinPQp;
    stRcConfig.mInitQp = pConfig->mInitQp;
    stRcConfig.mEnMbQpLimit = pConfig->mEnMbQpLimit;
    stRcConfig.mQuality = pConfig->mQuality;
    stRcConfig.mMovingTh = pConfig->mMovingTh;
    stRcConfig.mIBitsCoef = pConfig->mIBitsCoef;
    stRcConfig.mPBitsCoef = pConfig->mPBitsCoef;
    stRcConfig.mVbvBufSize = vbvBufSize;
    stRcConfig.mVbvThreshSize = vbvThreshSize;
    configVencRateCtrlParam(&stRcConfig, &pStreamContext->mVEncChnAttr, &pStreamContext->mVEncRcParam);

    pStreamContext->mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pStreamContext->mVEncChnAttr.GopAttr.mGopSize = 2;

    pStreamContext->mVEncFrameRateConfig.SrcFrmRate = pConfig->mMainSrcFrameRate;
    pStreamContext->mVEncFrameRateConfig.DstFrmRate = pConfig->mMainEncodeFrameRate;

#ifdef ENABLE_VENC_ADVANCED_PARAM
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type || PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        configIFrmMbRcMoveStatus(&pStreamContext->mVEncRcParam);
        configBitsClipParam(&pStreamContext->mVEncRcParam);
    }
#endif
}

static void configMain2ndStream(VencStreamContext *pStreamContext, SampleSmartIPCDemoConfig *pConfig)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (!pStreamContext || !pConfig)
    {
        aloge("fatal error! invalid input params! %p,%p", pStreamContext, pConfig);
    }

    pStreamContext->mIsp = pConfig->mMainIsp;
    pStreamContext->mVipp = pConfig->mMain2ndVipp;
    pStreamContext->mViChn = pConfig->mMain2ndViChn;
    pStreamContext->mVEncChn = pConfig->mMain2ndVEncChn;
    pStreamContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pStreamContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pStreamContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pConfig->mMain2ndPixelFormat);
    pStreamContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pStreamContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pStreamContext->mViAttr.format.width = pConfig->mMain2ndSrcWidth;
    pStreamContext->mViAttr.format.height = pConfig->mMain2ndSrcHeight;
    pStreamContext->mViAttr.fps = pConfig->mMain2ndSrcFrameRate;
    pStreamContext->mViAttr.use_current_win = 1;
    pStreamContext->mViAttr.nbufs = pConfig->mMain2ndViBufNum;
    pStreamContext->mViAttr.nplanes = 2;
    pStreamContext->mViAttr.wdr_mode = 0;
    pStreamContext->mViAttr.drop_frame_num = 0;

    pStreamContext->mVEncChnAttr.VeAttr.Type = pConfig->mMain2ndEncodeType;
    pStreamContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pConfig->mMain2ndKeyFrameInterval;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicWidth = pConfig->mMain2ndSrcWidth;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicHeight = pConfig->mMain2ndSrcHeight;
    pStreamContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pStreamContext->mVEncChnAttr.VeAttr.PixelFormat = pConfig->mMain2ndPixelFormat;
    pStreamContext->mVEncChnAttr.VeAttr.mColorSpace = pStreamContext->mViAttr.format.colorspace;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = pConfig->mMain2ndVeRefFrameLbcMode;;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = pConfig->mVeRecRefBufReduceEnable;

    pStreamContext->mVEncChnAttr.VeAttr.mVbrOptEnable = pConfig->mVbrOptEnable;

    pStreamContext->mVEncChnAttr.RcAttr.mProductMode = pConfig->mProductMode;
    //pStreamContext->mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pStreamContext->mEncppSharpAttenCoefPer = pConfig->mMain2ndEncppSharpAttenCoefPer;
    alogd("main2nd EncppSharpAttenCoefPer: %d%%", pStreamContext->mEncppSharpAttenCoefPer);
    pStreamContext->mIspAndVeLinkageEnable = pConfig->mIspAndVeLinkageEnable;
    pStreamContext->mCameraAdaptiveMovingAndStaticEnable = pConfig->mCameraAdaptiveMovingAndStaticEnable;

    pStreamContext->mViAttr.mbEncppEnable = pConfig->mMain2ndEncppEnable;
    pStreamContext->mVEncChnAttr.EncppAttr.eEncppSharpSetting = pConfig->mMain2ndEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    if (pConfig->mMain2ndSrcFrameRate)
    {
        vbvThreshSize = pConfig->mMain2ndEncodeBitrate/8/pConfig->mMain2ndSrcFrameRate*15;
    }
    vbvBufSize = pConfig->mMain2ndEncodeBitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("main2nd vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    VencRateCtrlConfig stRcConfig;
    memset(&stRcConfig, 0, sizeof(VencRateCtrlConfig));
    stRcConfig.mEncodeType = pConfig->mMain2ndEncodeType;
    stRcConfig.mEncodeWidth = pConfig->mMain2ndEncodeWidth;
    stRcConfig.mEncodeHeight = pConfig->mMain2ndEncodeHeight;
    stRcConfig.mSrcFrameRate = pConfig->mMain2ndSrcFrameRate;
    stRcConfig.mDstFrameRate = pConfig->mMain2ndEncodeFrameRate;
    stRcConfig.mEncodeBitrate = pConfig->mMain2ndEncodeBitrate;
    stRcConfig.mRcMode = getRcModeFromConfig(pConfig->mRcMode, pConfig->mMain2ndEncodeType);
    stRcConfig.mMaxIQp = pConfig->mMaxIQp;
    stRcConfig.mMinIQp = pConfig->mMinIQp;
    stRcConfig.mMaxPQp = pConfig->mMaxPQp;
    stRcConfig.mMinPQp = pConfig->mMinPQp;
    stRcConfig.mInitQp = pConfig->mInitQp;
    stRcConfig.mEnMbQpLimit = pConfig->mEnMbQpLimit;
    stRcConfig.mQuality = pConfig->mQuality;
    stRcConfig.mMovingTh = pConfig->mMovingTh;
    stRcConfig.mIBitsCoef = pConfig->mIBitsCoef;
    stRcConfig.mPBitsCoef = pConfig->mPBitsCoef;
    stRcConfig.mVbvBufSize = vbvBufSize;
    stRcConfig.mVbvThreshSize = vbvThreshSize;
    configVencRateCtrlParam(&stRcConfig, &pStreamContext->mVEncChnAttr, &pStreamContext->mVEncRcParam);

    pStreamContext->mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pStreamContext->mVEncChnAttr.GopAttr.mGopSize = 2;
    pStreamContext->mVEncFrameRateConfig.SrcFrmRate = pConfig->mMain2ndSrcFrameRate;
    pStreamContext->mVEncFrameRateConfig.DstFrmRate = pConfig->mMain2ndEncodeFrameRate;

#ifdef ENABLE_VENC_ADVANCED_PARAM
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type || PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        configIFrmMbRcMoveStatus(&pStreamContext->mVEncRcParam);
        configBitsClipParam(&pStreamContext->mVEncRcParam);
    }
#endif
}

static void configSubStream(VencStreamContext *pStreamContext, SampleSmartIPCDemoConfig *pConfig)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (!pStreamContext || !pConfig)
    {
        aloge("fatal error! invalid input params! %p,%p", pStreamContext, pConfig);
    }

    pStreamContext->mIsp = pConfig->mSubIsp;
    pStreamContext->mVipp = pConfig->mSubVipp;
    pStreamContext->mViChn = pConfig->mSubViChn;
    pStreamContext->mVEncChn = pConfig->mSubVEncChn;
    pStreamContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pStreamContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pStreamContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pConfig->mSubPixelFormat);
    pStreamContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pStreamContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pStreamContext->mViAttr.format.width = pConfig->mSubSrcWidth;
    pStreamContext->mViAttr.format.height = pConfig->mSubSrcHeight;
    pStreamContext->mViAttr.fps = pConfig->mSubSrcFrameRate;
    pStreamContext->mViAttr.use_current_win = 1;
    pStreamContext->mViAttr.nbufs = pConfig->mSubViBufNum;
    pStreamContext->mViAttr.nplanes = 2;
    pStreamContext->mViAttr.wdr_mode = pConfig->mSubWdrEnable;
    pStreamContext->mViAttr.drop_frame_num = 0;
    if (pConfig->mSubAiIspEnable)
    {
        pStreamContext->mViAttr.tdm_rxbuf_cnt = pConfig->mSubAiIspTdmRxBufNum;
    }
    else if (-1 != pConfig->mSubIspTdmRawProcessType)
    {
        pStreamContext->mViAttr.tdm_rxbuf_cnt = pConfig->mSubIspTdmRxBufNum;
    }

    pStreamContext->mVEncChnAttr.VeAttr.Type = pConfig->mSubEncodeType;
    pStreamContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pConfig->mSubKeyFrameInterval;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicWidth = pConfig->mSubSrcWidth;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicHeight = pConfig->mSubSrcHeight;
    pStreamContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pStreamContext->mVEncChnAttr.VeAttr.PixelFormat = pConfig->mSubPixelFormat;
    pStreamContext->mVEncChnAttr.VeAttr.mColorSpace = pStreamContext->mViAttr.format.colorspace;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = pConfig->mSubVeRefFrameLbcMode;;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = pConfig->mVeRecRefBufReduceEnable;

    pStreamContext->mVEncChnAttr.VeAttr.mVbrOptEnable = pConfig->mVbrOptEnable;

    pStreamContext->mVEncChnAttr.RcAttr.mProductMode = pConfig->mProductMode;
    //pStreamContext->mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pStreamContext->mEncppSharpAttenCoefPer = pConfig->mSubEncppSharpAttenCoefPer;
    alogd("sub EncppSharpAttenCoefPer: %d%%", pStreamContext->mEncppSharpAttenCoefPer);
    pStreamContext->mIspAndVeLinkageEnable = pConfig->mIspAndVeLinkageEnable;
    pStreamContext->mCameraAdaptiveMovingAndStaticEnable = pConfig->mCameraAdaptiveMovingAndStaticEnable;

    pStreamContext->mViAttr.mbEncppEnable = pConfig->mSubEncppEnable;
    pStreamContext->mVEncChnAttr.EncppAttr.eEncppSharpSetting = pConfig->mSubEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    if (pConfig->mSubSrcFrameRate)
    {
        vbvThreshSize = pConfig->mSubEncodeBitrate/8/pConfig->mSubSrcFrameRate*15;
    }
    vbvBufSize = pConfig->mSubEncodeBitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("sub vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    VencRateCtrlConfig stRcConfig;
    memset(&stRcConfig, 0, sizeof(VencRateCtrlConfig));
    stRcConfig.mEncodeType = pConfig->mSubEncodeType;
    stRcConfig.mEncodeWidth = pConfig->mSubEncodeWidth;
    stRcConfig.mEncodeHeight = pConfig->mSubEncodeHeight;
    stRcConfig.mSrcFrameRate = pConfig->mSubSrcFrameRate;
    stRcConfig.mDstFrameRate = pConfig->mSubEncodeFrameRate;
    stRcConfig.mEncodeBitrate = pConfig->mSubEncodeBitrate;
    stRcConfig.mRcMode = getRcModeFromConfig(pConfig->mRcMode, pConfig->mSubEncodeType);
    stRcConfig.mMaxIQp = pConfig->mMaxIQp;
    stRcConfig.mMinIQp = pConfig->mMinIQp;
    stRcConfig.mMaxPQp = pConfig->mMaxPQp;
    stRcConfig.mMinPQp = pConfig->mMinPQp;
    stRcConfig.mInitQp = pConfig->mInitQp;
    stRcConfig.mEnMbQpLimit = pConfig->mEnMbQpLimit;
    stRcConfig.mQuality = pConfig->mQuality;
    stRcConfig.mMovingTh = pConfig->mMovingTh;
    stRcConfig.mIBitsCoef = pConfig->mIBitsCoef;
    stRcConfig.mPBitsCoef = pConfig->mPBitsCoef;
    stRcConfig.mVbvBufSize = vbvBufSize;
    stRcConfig.mVbvThreshSize = vbvThreshSize;
    configVencRateCtrlParam(&stRcConfig, &pStreamContext->mVEncChnAttr, &pStreamContext->mVEncRcParam);

    pStreamContext->mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pStreamContext->mVEncChnAttr.GopAttr.mGopSize = 2;
    pStreamContext->mVEncFrameRateConfig.SrcFrmRate = pConfig->mSubSrcFrameRate;
    pStreamContext->mVEncFrameRateConfig.DstFrmRate = pConfig->mSubEncodeFrameRate;

#ifdef ENABLE_VENC_ADVANCED_PARAM
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type || PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        configIFrmMbRcMoveStatus(&pStreamContext->mVEncRcParam);
        configBitsClipParam(&pStreamContext->mVEncRcParam);
    }
#endif
}

static void configSub2ndStream(VencStreamContext *pStreamContext, SampleSmartIPCDemoConfig *pConfig)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (!pStreamContext || !pConfig)
    {
        aloge("fatal error! invalid input params! %p,%p", pStreamContext, pConfig);
    }

    pStreamContext->mIsp = pConfig->mSubIsp;
    pStreamContext->mVipp = pConfig->mSub2ndVipp;
    pStreamContext->mViChn = pConfig->mSub2ndViChn;
    pStreamContext->mVEncChn = pConfig->mSub2ndVEncChn;
    pStreamContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pStreamContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pStreamContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pConfig->mSub2ndPixelFormat);
    pStreamContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pStreamContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pStreamContext->mViAttr.format.width = pConfig->mSub2ndSrcWidth;
    pStreamContext->mViAttr.format.height = pConfig->mSub2ndSrcHeight;
    pStreamContext->mViAttr.fps = pConfig->mSub2ndSrcFrameRate;
    pStreamContext->mViAttr.use_current_win = 1;
    pStreamContext->mViAttr.nbufs = pConfig->mSub2ndViBufNum;
    pStreamContext->mViAttr.nplanes = 2;
    pStreamContext->mViAttr.wdr_mode = 0;
    pStreamContext->mViAttr.drop_frame_num = 0;

    pStreamContext->mVEncChnAttr.VeAttr.Type = pConfig->mSub2ndEncodeType;
    pStreamContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pConfig->mSub2ndKeyFrameInterval;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicWidth = pConfig->mSub2ndSrcWidth;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicHeight = pConfig->mSub2ndSrcHeight;
    pStreamContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pStreamContext->mVEncChnAttr.VeAttr.PixelFormat = pConfig->mSub2ndPixelFormat;
    pStreamContext->mVEncChnAttr.VeAttr.mColorSpace = pStreamContext->mViAttr.format.colorspace;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = pConfig->mSub2ndVeRefFrameLbcMode;;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = pConfig->mVeRecRefBufReduceEnable;

    pStreamContext->mVEncChnAttr.VeAttr.mVbrOptEnable = pConfig->mVbrOptEnable;

    pStreamContext->mVEncChnAttr.RcAttr.mProductMode = pConfig->mProductMode;
    //pStreamContext->mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pStreamContext->mEncppSharpAttenCoefPer = pConfig->mSub2ndEncppSharpAttenCoefPer;
    alogd("sub2nd EncppSharpAttenCoefPer: %d%%", pStreamContext->mEncppSharpAttenCoefPer);
    pStreamContext->mIspAndVeLinkageEnable = pConfig->mIspAndVeLinkageEnable;
    pStreamContext->mCameraAdaptiveMovingAndStaticEnable = pConfig->mCameraAdaptiveMovingAndStaticEnable;

    pStreamContext->mViAttr.mbEncppEnable = pConfig->mSub2ndEncppEnable;
    pStreamContext->mVEncChnAttr.EncppAttr.eEncppSharpSetting = pConfig->mSub2ndEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    if (pConfig->mSub2ndSrcFrameRate)
    {
        vbvThreshSize = pConfig->mSub2ndEncodeBitrate/8/pConfig->mSub2ndSrcFrameRate*15;
    }
    vbvBufSize = pConfig->mSub2ndEncodeBitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("sub2nd vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    VencRateCtrlConfig stRcConfig;
    memset(&stRcConfig, 0, sizeof(VencRateCtrlConfig));
    stRcConfig.mEncodeType = pConfig->mSub2ndEncodeType;
    stRcConfig.mEncodeWidth = pConfig->mSub2ndEncodeWidth;
    stRcConfig.mEncodeHeight = pConfig->mSub2ndEncodeHeight;
    stRcConfig.mSrcFrameRate = pConfig->mSub2ndSrcFrameRate;
    stRcConfig.mDstFrameRate = pConfig->mSub2ndEncodeFrameRate;
    stRcConfig.mEncodeBitrate = pConfig->mSub2ndEncodeBitrate;
    stRcConfig.mRcMode = getRcModeFromConfig(pConfig->mRcMode, pConfig->mSub2ndEncodeType);
    stRcConfig.mMaxIQp = pConfig->mMaxIQp;
    stRcConfig.mMinIQp = pConfig->mMinIQp;
    stRcConfig.mMaxPQp = pConfig->mMaxPQp;
    stRcConfig.mMinPQp = pConfig->mMinPQp;
    stRcConfig.mInitQp = pConfig->mInitQp;
    stRcConfig.mEnMbQpLimit = pConfig->mEnMbQpLimit;
    stRcConfig.mQuality = pConfig->mQuality;
    stRcConfig.mMovingTh = pConfig->mMovingTh;
    stRcConfig.mIBitsCoef = pConfig->mIBitsCoef;
    stRcConfig.mPBitsCoef = pConfig->mPBitsCoef;
    stRcConfig.mVbvBufSize = vbvBufSize;
    stRcConfig.mVbvThreshSize = vbvThreshSize;
    configVencRateCtrlParam(&stRcConfig, &pStreamContext->mVEncChnAttr, &pStreamContext->mVEncRcParam);

    pStreamContext->mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pStreamContext->mVEncChnAttr.GopAttr.mGopSize = 2;
    pStreamContext->mVEncFrameRateConfig.SrcFrmRate = pConfig->mSub2ndSrcFrameRate;
    pStreamContext->mVEncFrameRateConfig.DstFrmRate = pConfig->mSub2ndEncodeFrameRate;

#ifdef ENABLE_VENC_ADVANCED_PARAM
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type || PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        configIFrmMbRcMoveStatus(&pStreamContext->mVEncRcParam);
        configBitsClipParam(&pStreamContext->mVEncRcParam);
    }
#endif
}

static void configThreeStream(VencStreamContext *pStreamContext, SampleSmartIPCDemoConfig *pConfig)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (!pStreamContext || !pConfig)
    {
        aloge("fatal error! invalid input params! %p,%p", pStreamContext, pConfig);
    }

    pStreamContext->mIsp = pConfig->mThreeIsp;
    pStreamContext->mVipp = pConfig->mThreeVipp;
    if (pConfig->mThreeOnlineEnable && (0 != pStreamContext->mVipp))
    {
        aloge("fatal error! main vipp %d is wrong, only vipp0 support online.", pStreamContext->mVipp);
    }

    pStreamContext->mViChn = pConfig->mThreeViChn; //0;
    pStreamContext->mVEncChn = pConfig->mThreeVEncChn; //0;

    if (pConfig->mThreeOnlineEnable)
    {
        pStreamContext->mViAttr.mOnlineEnable = 1;
        pStreamContext->mViAttr.mOnlineShareBufNum = pConfig->mThreeOnlineShareBufNum;//BK_TWO_BUFFER;
        pStreamContext->mVEncChnAttr.VeAttr.mOnlineEnable = 1;
        pStreamContext->mVEncChnAttr.VeAttr.mOnlineShareBufNum = pConfig->mThreeOnlineShareBufNum;//BK_TWO_BUFFER;
    }
    alogd("main vipp%d ve_online_en:%d, dma_buf_num:%d, venc ch%d OnlineEnable:%d, OnlineShareBufNum:%d",
        pStreamContext->mVipp, pStreamContext->mViAttr.mOnlineEnable, pStreamContext->mViAttr.mOnlineShareBufNum,
        pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.VeAttr.mOnlineEnable, pStreamContext->mVEncChnAttr.VeAttr.mOnlineShareBufNum);

    pStreamContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pStreamContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pStreamContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pConfig->mThreePixelFormat);
    pStreamContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pStreamContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pStreamContext->mViAttr.format.width = pConfig->mThreeSrcWidth;
    pStreamContext->mViAttr.format.height = pConfig->mThreeSrcHeight;
    pStreamContext->mViAttr.fps = pConfig->mThreeSrcFrameRate;
    pStreamContext->mViAttr.use_current_win = 0;
    pStreamContext->mViAttr.nbufs = pConfig->mThreeViBufNum;
    pStreamContext->mViAttr.nplanes = 2;
    pStreamContext->mViAttr.wdr_mode = pConfig->mThreeWdrEnable;
    pStreamContext->mViAttr.drop_frame_num = 0;
    if (pConfig->mThreeAiIspEnable)
    {
        pStreamContext->mViAttr.tdm_rxbuf_cnt = pConfig->mThreeAiIspTdmRxBufNum;
    }
    else if (-1 != pConfig->mThreeIspTdmRawProcessType)
    {
        pStreamContext->mViAttr.tdm_rxbuf_cnt = pConfig->mThreeIspTdmRxBufNum;
    }

    pStreamContext->mVEncChnAttr.VeAttr.Type = pConfig->mThreeEncodeType;
    pStreamContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pConfig->mThreeKeyFrameInterval;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicWidth = pConfig->mThreeSrcWidth;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicHeight = pConfig->mThreeSrcHeight;
    pStreamContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pStreamContext->mVEncChnAttr.VeAttr.PixelFormat = pConfig->mThreePixelFormat;
    pStreamContext->mVEncChnAttr.VeAttr.mColorSpace = pStreamContext->mViAttr.format.colorspace;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = pConfig->mThreeVeRefFrameLbcMode;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = pConfig->mVeRecRefBufReduceEnable;

    pStreamContext->mVEncChnAttr.VeAttr.mVbrOptEnable = pConfig->mVbrOptEnable;

    pStreamContext->mVEncChnAttr.RcAttr.mProductMode = pConfig->mProductMode;
    //pStreamContext->mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pStreamContext->mEncppSharpAttenCoefPer = 100;
    alogd("main EncppSharpAttenCoefPer: %d%%", pStreamContext->mEncppSharpAttenCoefPer);
    pStreamContext->mIspAndVeLinkageEnable = pConfig->mIspAndVeLinkageEnable;
    pStreamContext->mCameraAdaptiveMovingAndStaticEnable = pConfig->mCameraAdaptiveMovingAndStaticEnable;

    pStreamContext->mViAttr.mbEncppEnable = pConfig->mThreeEncppEnable;
    pStreamContext->mVEncChnAttr.EncppAttr.eEncppSharpSetting = pConfig->mThreeEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    if (pConfig->mThreeSrcFrameRate)
    {
        vbvThreshSize = pConfig->mThreeEncodeBitrate/8/pConfig->mThreeSrcFrameRate*15;
    }
    vbvBufSize = pConfig->mThreeEncodeBitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("main vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    VencRateCtrlConfig stRcConfig;
    memset(&stRcConfig, 0, sizeof(VencRateCtrlConfig));
    stRcConfig.mEncodeType = pConfig->mThreeEncodeType;
    stRcConfig.mEncodeWidth = pConfig->mThreeEncodeWidth;
    stRcConfig.mEncodeHeight = pConfig->mThreeEncodeHeight;
    stRcConfig.mSrcFrameRate = pConfig->mThreeSrcFrameRate;
    stRcConfig.mDstFrameRate = pConfig->mThreeEncodeFrameRate;
    stRcConfig.mEncodeBitrate = pConfig->mThreeEncodeBitrate;
    stRcConfig.mRcMode = getRcModeFromConfig(pConfig->mRcMode, pConfig->mThreeEncodeType);
    stRcConfig.mMaxIQp = pConfig->mMaxIQp;
    stRcConfig.mMinIQp = pConfig->mMinIQp;
    stRcConfig.mMaxPQp = pConfig->mMaxPQp;
    stRcConfig.mMinPQp = pConfig->mMinPQp;
    stRcConfig.mInitQp = pConfig->mInitQp;
    stRcConfig.mEnMbQpLimit = pConfig->mEnMbQpLimit;
    stRcConfig.mQuality = pConfig->mQuality;
    stRcConfig.mMovingTh = pConfig->mMovingTh;
    stRcConfig.mIBitsCoef = pConfig->mIBitsCoef;
    stRcConfig.mPBitsCoef = pConfig->mPBitsCoef;
    stRcConfig.mVbvBufSize = vbvBufSize;
    stRcConfig.mVbvThreshSize = vbvThreshSize;
    configVencRateCtrlParam(&stRcConfig, &pStreamContext->mVEncChnAttr, &pStreamContext->mVEncRcParam);

    pStreamContext->mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pStreamContext->mVEncChnAttr.GopAttr.mGopSize = 2;

    pStreamContext->mVEncFrameRateConfig.SrcFrmRate = pConfig->mThreeSrcFrameRate;
    pStreamContext->mVEncFrameRateConfig.DstFrmRate = pConfig->mThreeEncodeFrameRate;

#ifdef ENABLE_VENC_ADVANCED_PARAM
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type || PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        configIFrmMbRcMoveStatus(&pStreamContext->mVEncRcParam);
        configBitsClipParam(&pStreamContext->mVEncRcParam);
    }
#endif
}

static void configThree2ndStream(VencStreamContext *pStreamContext, SampleSmartIPCDemoConfig *pConfig)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (!pStreamContext || !pConfig)
    {
        aloge("fatal error! invalid input params! %p,%p", pStreamContext, pConfig);
    }

    pStreamContext->mIsp = pConfig->mThreeIsp;
    pStreamContext->mVipp = pConfig->mThree2ndVipp;
    pStreamContext->mViChn = pConfig->mThree2ndViChn;
    pStreamContext->mVEncChn = pConfig->mThree2ndVEncChn;
    pStreamContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pStreamContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pStreamContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pConfig->mThree2ndPixelFormat);
    pStreamContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pStreamContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pStreamContext->mViAttr.format.width = pConfig->mThree2ndSrcWidth;
    pStreamContext->mViAttr.format.height = pConfig->mThree2ndSrcHeight;
    pStreamContext->mViAttr.fps = pConfig->mThree2ndSrcFrameRate;
    pStreamContext->mViAttr.use_current_win = 1;
    pStreamContext->mViAttr.nbufs = pConfig->mThree2ndViBufNum;
    pStreamContext->mViAttr.nplanes = 2;
    pStreamContext->mViAttr.wdr_mode = 0;
    pStreamContext->mViAttr.drop_frame_num = 0;

    pStreamContext->mVEncChnAttr.VeAttr.Type = pConfig->mThree2ndEncodeType;
    pStreamContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pConfig->mThree2ndKeyFrameInterval;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicWidth = pConfig->mThree2ndSrcWidth;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicHeight = pConfig->mThree2ndSrcHeight;
    pStreamContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pStreamContext->mVEncChnAttr.VeAttr.PixelFormat = pConfig->mThree2ndPixelFormat;
    pStreamContext->mVEncChnAttr.VeAttr.mColorSpace = pStreamContext->mViAttr.format.colorspace;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = pConfig->mThree2ndVeRefFrameLbcMode;;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = pConfig->mVeRecRefBufReduceEnable;

    pStreamContext->mVEncChnAttr.VeAttr.mVbrOptEnable = pConfig->mVbrOptEnable;

    pStreamContext->mVEncChnAttr.RcAttr.mProductMode = pConfig->mProductMode;
    //pStreamContext->mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pStreamContext->mEncppSharpAttenCoefPer = pConfig->mThree2ndEncppSharpAttenCoefPer;
    alogd("main2nd EncppSharpAttenCoefPer: %d%%", pStreamContext->mEncppSharpAttenCoefPer);
    pStreamContext->mIspAndVeLinkageEnable = pConfig->mIspAndVeLinkageEnable;
    pStreamContext->mCameraAdaptiveMovingAndStaticEnable = pConfig->mCameraAdaptiveMovingAndStaticEnable;

    pStreamContext->mViAttr.mbEncppEnable = pConfig->mThree2ndEncppEnable;
    pStreamContext->mVEncChnAttr.EncppAttr.eEncppSharpSetting = pConfig->mThree2ndEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    if (pConfig->mThree2ndSrcFrameRate)
    {
        vbvThreshSize = pConfig->mThree2ndEncodeBitrate/8/pConfig->mThree2ndSrcFrameRate*15;
    }
    vbvBufSize = pConfig->mThree2ndEncodeBitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("main2nd vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    VencRateCtrlConfig stRcConfig;
    memset(&stRcConfig, 0, sizeof(VencRateCtrlConfig));
    stRcConfig.mEncodeType = pConfig->mThree2ndEncodeType;
    stRcConfig.mEncodeWidth = pConfig->mThree2ndEncodeWidth;
    stRcConfig.mEncodeHeight = pConfig->mThree2ndEncodeHeight;
    stRcConfig.mSrcFrameRate = pConfig->mThree2ndSrcFrameRate;
    stRcConfig.mDstFrameRate = pConfig->mThree2ndEncodeFrameRate;
    stRcConfig.mEncodeBitrate = pConfig->mThree2ndEncodeBitrate;
    stRcConfig.mRcMode = getRcModeFromConfig(pConfig->mRcMode, pConfig->mThree2ndEncodeType);
    stRcConfig.mMaxIQp = pConfig->mMaxIQp;
    stRcConfig.mMinIQp = pConfig->mMinIQp;
    stRcConfig.mMaxPQp = pConfig->mMaxPQp;
    stRcConfig.mMinPQp = pConfig->mMinPQp;
    stRcConfig.mInitQp = pConfig->mInitQp;
    stRcConfig.mEnMbQpLimit = pConfig->mEnMbQpLimit;
    stRcConfig.mQuality = pConfig->mQuality;
    stRcConfig.mMovingTh = pConfig->mMovingTh;
    stRcConfig.mIBitsCoef = pConfig->mIBitsCoef;
    stRcConfig.mPBitsCoef = pConfig->mPBitsCoef;
    stRcConfig.mVbvBufSize = vbvBufSize;
    stRcConfig.mVbvThreshSize = vbvThreshSize;
    configVencRateCtrlParam(&stRcConfig, &pStreamContext->mVEncChnAttr, &pStreamContext->mVEncRcParam);

    pStreamContext->mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pStreamContext->mVEncChnAttr.GopAttr.mGopSize = 2;
    pStreamContext->mVEncFrameRateConfig.SrcFrmRate = pConfig->mThree2ndSrcFrameRate;
    pStreamContext->mVEncFrameRateConfig.DstFrmRate = pConfig->mThree2ndEncodeFrameRate;

#ifdef ENABLE_VENC_ADVANCED_PARAM
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type || PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        configIFrmMbRcMoveStatus(&pStreamContext->mVEncRcParam);
        configBitsClipParam(&pStreamContext->mVEncRcParam);
    }
#endif
}

static void configFourStream(VencStreamContext *pStreamContext, SampleSmartIPCDemoConfig *pConfig)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (!pStreamContext || !pConfig)
    {
        aloge("fatal error! invalid input params! %p,%p", pStreamContext, pConfig);
    }

    pStreamContext->mIsp = pConfig->mFourIsp;
    pStreamContext->mVipp = pConfig->mFourVipp;
    if (pConfig->mFourOnlineEnable && (0 != pStreamContext->mVipp))
    {
        aloge("fatal error! main vipp %d is wrong, only vipp0 support online.", pStreamContext->mVipp);
    }

    pStreamContext->mViChn = pConfig->mFourViChn; //0;
    pStreamContext->mVEncChn = pConfig->mFourVEncChn; //0;

    if (pConfig->mFourOnlineEnable)
    {
        pStreamContext->mViAttr.mOnlineEnable = 1;
        pStreamContext->mViAttr.mOnlineShareBufNum = pConfig->mFourOnlineShareBufNum;//BK_TWO_BUFFER;
        pStreamContext->mVEncChnAttr.VeAttr.mOnlineEnable = 1;
        pStreamContext->mVEncChnAttr.VeAttr.mOnlineShareBufNum = pConfig->mFourOnlineShareBufNum;//BK_TWO_BUFFER;
    }
    alogd("main vipp%d ve_online_en:%d, dma_buf_num:%d, venc ch%d OnlineEnable:%d, OnlineShareBufNum:%d",
        pStreamContext->mVipp, pStreamContext->mViAttr.mOnlineEnable, pStreamContext->mViAttr.mOnlineShareBufNum,
        pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.VeAttr.mOnlineEnable, pStreamContext->mVEncChnAttr.VeAttr.mOnlineShareBufNum);

    pStreamContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pStreamContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pStreamContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pConfig->mFourPixelFormat);
    pStreamContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pStreamContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pStreamContext->mViAttr.format.width = pConfig->mFourSrcWidth;
    pStreamContext->mViAttr.format.height = pConfig->mFourSrcHeight;
    pStreamContext->mViAttr.fps = pConfig->mFourSrcFrameRate;
    pStreamContext->mViAttr.use_current_win = 0;
    pStreamContext->mViAttr.nbufs = pConfig->mFourViBufNum;
    pStreamContext->mViAttr.nplanes = 2;
    pStreamContext->mViAttr.wdr_mode = pConfig->mFourWdrEnable;
    pStreamContext->mViAttr.drop_frame_num = 0;
    if (pConfig->mFourAiIspEnable)
    {
        pStreamContext->mViAttr.tdm_rxbuf_cnt = pConfig->mFourAiIspTdmRxBufNum;
    }
    else if (-1 != pConfig->mFourIspTdmRawProcessType)
    {
        pStreamContext->mViAttr.tdm_rxbuf_cnt = pConfig->mFourIspTdmRxBufNum;
    }

    pStreamContext->mVEncChnAttr.VeAttr.Type = pConfig->mFourEncodeType;
    pStreamContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pConfig->mFourKeyFrameInterval;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicWidth = pConfig->mFourSrcWidth;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicHeight = pConfig->mFourSrcHeight;
    pStreamContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pStreamContext->mVEncChnAttr.VeAttr.PixelFormat = pConfig->mFourPixelFormat;
    pStreamContext->mVEncChnAttr.VeAttr.mColorSpace = pStreamContext->mViAttr.format.colorspace;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = pConfig->mFourVeRefFrameLbcMode;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = pConfig->mVeRecRefBufReduceEnable;

    pStreamContext->mVEncChnAttr.VeAttr.mVbrOptEnable = pConfig->mVbrOptEnable;

    pStreamContext->mVEncChnAttr.RcAttr.mProductMode = pConfig->mProductMode;
    //pStreamContext->mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pStreamContext->mEncppSharpAttenCoefPer = 100;
    alogd("main EncppSharpAttenCoefPer: %d%%", pStreamContext->mEncppSharpAttenCoefPer);
    pStreamContext->mIspAndVeLinkageEnable = pConfig->mIspAndVeLinkageEnable;
    pStreamContext->mCameraAdaptiveMovingAndStaticEnable = pConfig->mCameraAdaptiveMovingAndStaticEnable;

    pStreamContext->mViAttr.mbEncppEnable = pConfig->mFourEncppEnable;
    pStreamContext->mVEncChnAttr.EncppAttr.eEncppSharpSetting = pConfig->mFourEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    if (pConfig->mFourSrcFrameRate)
    {
        vbvThreshSize = pConfig->mFourEncodeBitrate/8/pConfig->mFourSrcFrameRate*15;
    }
    vbvBufSize = pConfig->mFourEncodeBitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("main vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    VencRateCtrlConfig stRcConfig;
    memset(&stRcConfig, 0, sizeof(VencRateCtrlConfig));
    stRcConfig.mEncodeType = pConfig->mFourEncodeType;
    stRcConfig.mEncodeWidth = pConfig->mFourEncodeWidth;
    stRcConfig.mEncodeHeight = pConfig->mFourEncodeHeight;
    stRcConfig.mSrcFrameRate = pConfig->mFourSrcFrameRate;
    stRcConfig.mDstFrameRate = pConfig->mFourEncodeFrameRate;
    stRcConfig.mEncodeBitrate = pConfig->mFourEncodeBitrate;
    stRcConfig.mRcMode = getRcModeFromConfig(pConfig->mRcMode, pConfig->mFourEncodeType);
    stRcConfig.mMaxIQp = pConfig->mMaxIQp;
    stRcConfig.mMinIQp = pConfig->mMinIQp;
    stRcConfig.mMaxPQp = pConfig->mMaxPQp;
    stRcConfig.mMinPQp = pConfig->mMinPQp;
    stRcConfig.mInitQp = pConfig->mInitQp;
    stRcConfig.mEnMbQpLimit = pConfig->mEnMbQpLimit;
    stRcConfig.mQuality = pConfig->mQuality;
    stRcConfig.mMovingTh = pConfig->mMovingTh;
    stRcConfig.mIBitsCoef = pConfig->mIBitsCoef;
    stRcConfig.mPBitsCoef = pConfig->mPBitsCoef;
    stRcConfig.mVbvBufSize = vbvBufSize;
    stRcConfig.mVbvThreshSize = vbvThreshSize;
    configVencRateCtrlParam(&stRcConfig, &pStreamContext->mVEncChnAttr, &pStreamContext->mVEncRcParam);

    pStreamContext->mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pStreamContext->mVEncChnAttr.GopAttr.mGopSize = 2;

    pStreamContext->mVEncFrameRateConfig.SrcFrmRate = pConfig->mFourSrcFrameRate;
    pStreamContext->mVEncFrameRateConfig.DstFrmRate = pConfig->mFourEncodeFrameRate;

#ifdef ENABLE_VENC_ADVANCED_PARAM
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type || PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        configIFrmMbRcMoveStatus(&pStreamContext->mVEncRcParam);
        configBitsClipParam(&pStreamContext->mVEncRcParam);
    }
#endif
}

static void configFour2ndStream(VencStreamContext *pStreamContext, SampleSmartIPCDemoConfig *pConfig)
{
    unsigned int vbvThreshSize = 0;
    unsigned int vbvBufSize = 0;
    if (!pStreamContext || !pConfig)
    {
        aloge("fatal error! invalid input params! %p,%p", pStreamContext, pConfig);
    }

    pStreamContext->mIsp = pConfig->mFourIsp;
    pStreamContext->mVipp = pConfig->mFour2ndVipp;
    pStreamContext->mViChn = pConfig->mFour2ndViChn;
    pStreamContext->mVEncChn = pConfig->mFour2ndVEncChn;
    pStreamContext->mViAttr.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pStreamContext->mViAttr.memtype = V4L2_MEMORY_MMAP;
    pStreamContext->mViAttr.format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pConfig->mFour2ndPixelFormat);
    pStreamContext->mViAttr.format.field = V4L2_FIELD_NONE;
    pStreamContext->mViAttr.format.colorspace = V4L2_COLORSPACE_REC709; //V4L2_COLORSPACE_REC709_PART_RANGE;
    pStreamContext->mViAttr.format.width = pConfig->mFour2ndSrcWidth;
    pStreamContext->mViAttr.format.height = pConfig->mFour2ndSrcHeight;
    pStreamContext->mViAttr.fps = pConfig->mFour2ndSrcFrameRate;
    pStreamContext->mViAttr.use_current_win = 1;
    pStreamContext->mViAttr.nbufs = pConfig->mFour2ndViBufNum;
    pStreamContext->mViAttr.nplanes = 2;
    pStreamContext->mViAttr.wdr_mode = 0;
    pStreamContext->mViAttr.drop_frame_num = 0;

    pStreamContext->mVEncChnAttr.VeAttr.Type = pConfig->mFour2ndEncodeType;
    pStreamContext->mVEncChnAttr.VeAttr.MaxKeyInterval = pConfig->mFour2ndKeyFrameInterval;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicWidth = pConfig->mFour2ndSrcWidth;
    pStreamContext->mVEncChnAttr.VeAttr.SrcPicHeight = pConfig->mFour2ndSrcHeight;
    pStreamContext->mVEncChnAttr.VeAttr.Field = VIDEO_FIELD_FRAME;
    pStreamContext->mVEncChnAttr.VeAttr.PixelFormat = pConfig->mFour2ndPixelFormat;
    pStreamContext->mVEncChnAttr.VeAttr.mColorSpace = pStreamContext->mViAttr.format.colorspace;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRefFrameLbcMode = pConfig->mFour2ndVeRefFrameLbcMode;;
    pStreamContext->mVEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = pConfig->mVeRecRefBufReduceEnable;

    pStreamContext->mVEncChnAttr.VeAttr.mVbrOptEnable = pConfig->mVbrOptEnable;

    pStreamContext->mVEncChnAttr.RcAttr.mProductMode = pConfig->mProductMode;
    //pStreamContext->mVEncRcParam.sensor_type = VENC_ST_EN_WDR;

    pStreamContext->mEncppSharpAttenCoefPer = pConfig->mFour2ndEncppSharpAttenCoefPer;
    alogd("main2nd EncppSharpAttenCoefPer: %d%%", pStreamContext->mEncppSharpAttenCoefPer);
    pStreamContext->mIspAndVeLinkageEnable = pConfig->mIspAndVeLinkageEnable;
    pStreamContext->mCameraAdaptiveMovingAndStaticEnable = pConfig->mCameraAdaptiveMovingAndStaticEnable;

    pStreamContext->mViAttr.mbEncppEnable = pConfig->mFour2ndEncppEnable;
    pStreamContext->mVEncChnAttr.EncppAttr.eEncppSharpSetting = pConfig->mFour2ndEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;

    if (pConfig->mFour2ndSrcFrameRate)
    {
        vbvThreshSize = pConfig->mFour2ndEncodeBitrate/8/pConfig->mFour2ndSrcFrameRate*15;
    }
    vbvBufSize = pConfig->mFour2ndEncodeBitrate/8*VE_VBV_CACHE_TIME + vbvThreshSize;
    alogd("main2nd vbvThreshSize: %d, vbvBufSize: %d", vbvThreshSize, vbvBufSize);

    VencRateCtrlConfig stRcConfig;
    memset(&stRcConfig, 0, sizeof(VencRateCtrlConfig));
    stRcConfig.mEncodeType = pConfig->mFour2ndEncodeType;
    stRcConfig.mEncodeWidth = pConfig->mFour2ndEncodeWidth;
    stRcConfig.mEncodeHeight = pConfig->mFour2ndEncodeHeight;
    stRcConfig.mSrcFrameRate = pConfig->mFour2ndSrcFrameRate;
    stRcConfig.mDstFrameRate = pConfig->mFour2ndEncodeFrameRate;
    stRcConfig.mEncodeBitrate = pConfig->mFour2ndEncodeBitrate;
    stRcConfig.mRcMode = getRcModeFromConfig(pConfig->mRcMode, pConfig->mFour2ndEncodeType);
    stRcConfig.mMaxIQp = pConfig->mMaxIQp;
    stRcConfig.mMinIQp = pConfig->mMinIQp;
    stRcConfig.mMaxPQp = pConfig->mMaxPQp;
    stRcConfig.mMinPQp = pConfig->mMinPQp;
    stRcConfig.mInitQp = pConfig->mInitQp;
    stRcConfig.mEnMbQpLimit = pConfig->mEnMbQpLimit;
    stRcConfig.mQuality = pConfig->mQuality;
    stRcConfig.mMovingTh = pConfig->mMovingTh;
    stRcConfig.mIBitsCoef = pConfig->mIBitsCoef;
    stRcConfig.mPBitsCoef = pConfig->mPBitsCoef;
    stRcConfig.mVbvBufSize = vbvBufSize;
    stRcConfig.mVbvThreshSize = vbvThreshSize;
    configVencRateCtrlParam(&stRcConfig, &pStreamContext->mVEncChnAttr, &pStreamContext->mVEncRcParam);

    pStreamContext->mVEncChnAttr.GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    pStreamContext->mVEncChnAttr.GopAttr.mGopSize = 2;
    pStreamContext->mVEncFrameRateConfig.SrcFrmRate = pConfig->mFour2ndSrcFrameRate;
    pStreamContext->mVEncFrameRateConfig.DstFrmRate = pConfig->mFour2ndEncodeFrameRate;

#ifdef ENABLE_VENC_ADVANCED_PARAM
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type || PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        configIFrmMbRcMoveStatus(&pStreamContext->mVEncRcParam);
        configBitsClipParam(&pStreamContext->mVEncRcParam);
    }
#endif
}

static void configAiService(SampleSmartIPCDemoConfig *pConfig, ai_service_attr_t *pNnAttr)
{
    // main
    if (pConfig->mMainEnable)
    {
        pNnAttr->ch_info[0].ch_idx = 0;
        if (0 == pConfig->mMainNnNbgType)
        {
            pNnAttr->ch_info[0].nbg_type = AWNN_DET_POST_HUMANOID_1;
            pNnAttr->ch_info[0].src_width = NN_HUMAN_SRC_WIDTH;
            pNnAttr->ch_info[0].src_height = NN_HUMAN_SRC_HEIGHT;
            pNnAttr->ch_info[0].thresh = 0.25;
        }
        else if (1 == pConfig->mMainNnNbgType)
        {
            pNnAttr->ch_info[0].nbg_type = AWNN_DET_POST_FACE_1;
            pNnAttr->ch_info[0].src_width = NN_FACE_SRC_WIDTH;
            pNnAttr->ch_info[0].src_height = NN_FACE_SRC_HEIGHT;
            pNnAttr->ch_info[0].thresh = 0.60;
        }
        else
        {
            pNnAttr->ch_info[0].nbg_type = -1;
        }
        pNnAttr->ch_info[0].isp = pConfig->mMainIsp;
        pNnAttr->ch_info[0].vipp = pConfig->mMainNnVipp;
        pNnAttr->ch_info[0].viChn = 0;
        pNnAttr->ch_info[0].pixel_format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pNnAttr->ch_info[0].vi_buf_num = pConfig->mMainNnViBufNum;
        pNnAttr->ch_info[0].src_frame_rate = pConfig->mMainNnSrcFrameRate;
        strncpy(pNnAttr->ch_info[0].model_file, pConfig->mMainNnNbgFilePath, NN_NBG_MAX_FILE_PATH_SIZE);
        pNnAttr->ch_info[0].draw_orl_enable = pConfig->mMainNnDrawOrlEnable;
        pNnAttr->ch_info[0].draw_orl_vipp = pConfig->mMainVipp;
        pNnAttr->ch_info[0].draw_orl_src_width = pConfig->mMainSrcWidth;
        pNnAttr->ch_info[0].draw_orl_src_height = pConfig->mMainSrcHeight;
        pNnAttr->ch_info[0].region_hdl_base = 100;
    }
    else
    {
        pNnAttr->ch_info[0].ch_idx = 0;
        pNnAttr->ch_info[0].nbg_type = -1;
    }
    // sub
    if (pConfig->mSubEnable)
    {
        pNnAttr->ch_info[1].ch_idx = 1;
        if (0 == pConfig->mSubNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_DET_POST_HUMANOID_1;
            pNnAttr->ch_info[1].src_width = NN_HUMAN_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_HUMAN_SRC_HEIGHT;
            pNnAttr->ch_info[1].thresh = 0.25;
        }
        else if (1 == pConfig->mSubNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_DET_POST_FACE_1;
            pNnAttr->ch_info[1].src_width = NN_FACE_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_FACE_SRC_HEIGHT;
            pNnAttr->ch_info[1].thresh = 0.60;
        }
        else
        {
            pNnAttr->ch_info[1].nbg_type = -1;
        }
        pNnAttr->ch_info[1].isp = pConfig->mSubIsp;
        pNnAttr->ch_info[1].vipp = pConfig->mSubNnVipp;
        pNnAttr->ch_info[1].viChn = 0;
        pNnAttr->ch_info[1].pixel_format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pNnAttr->ch_info[1].vi_buf_num = pConfig->mSubNnViBufNum;
        pNnAttr->ch_info[1].src_frame_rate = pConfig->mSubNnSrcFrameRate;
        strncpy(pNnAttr->ch_info[1].model_file, pConfig->mSubNnNbgFilePath, NN_NBG_MAX_FILE_PATH_SIZE);
        pNnAttr->ch_info[1].draw_orl_enable = pConfig->mSubNnDrawOrlEnable;
        pNnAttr->ch_info[1].draw_orl_vipp = pConfig->mSubVipp;
        pNnAttr->ch_info[1].draw_orl_src_width = pConfig->mSubSrcWidth;
        pNnAttr->ch_info[1].draw_orl_src_height = pConfig->mSubSrcHeight;
        pNnAttr->ch_info[1].region_hdl_base = 200;
    }
    // three
    if (pConfig->mThreeEnable)
    {
        pNnAttr->ch_info[1].ch_idx = 1;
        if (0 == pConfig->mThreeNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_DET_POST_HUMANOID_1;
            pNnAttr->ch_info[1].src_width = NN_HUMAN_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_HUMAN_SRC_HEIGHT;
            pNnAttr->ch_info[1].thresh = 0.25;
        }
        else if (1 == pConfig->mThreeNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_DET_POST_FACE_1;
            pNnAttr->ch_info[1].src_width = NN_FACE_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_FACE_SRC_HEIGHT;
            pNnAttr->ch_info[1].thresh = 0.60;
        }
        else
        {
            pNnAttr->ch_info[1].nbg_type = -1;
        }
        pNnAttr->ch_info[1].isp = pConfig->mThreeIsp;
        pNnAttr->ch_info[1].vipp = pConfig->mThreeNnVipp;
        pNnAttr->ch_info[1].viChn = 0;
        pNnAttr->ch_info[1].pixel_format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pNnAttr->ch_info[1].vi_buf_num = pConfig->mThreeNnViBufNum;
        pNnAttr->ch_info[1].src_frame_rate = pConfig->mThreeNnSrcFrameRate;
        strncpy(pNnAttr->ch_info[1].model_file, pConfig->mThreeNnNbgFilePath, NN_NBG_MAX_FILE_PATH_SIZE);
        pNnAttr->ch_info[1].draw_orl_enable = pConfig->mThreeNnDrawOrlEnable;
        pNnAttr->ch_info[1].draw_orl_vipp = pConfig->mThreeVipp;
        pNnAttr->ch_info[1].draw_orl_src_width = pConfig->mThreeSrcWidth;
        pNnAttr->ch_info[1].draw_orl_src_height = pConfig->mThreeSrcHeight;
        pNnAttr->ch_info[1].region_hdl_base = 200;
    }
    // four
    if (pConfig->mFourEnable)
    {
        pNnAttr->ch_info[1].ch_idx = 1;
        if (0 == pConfig->mFourNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_DET_POST_HUMANOID_1;
            pNnAttr->ch_info[1].src_width = NN_HUMAN_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_HUMAN_SRC_HEIGHT;
            pNnAttr->ch_info[1].thresh = 0.25;
        }
        else if (1 == pConfig->mFourNnNbgType)
        {
            pNnAttr->ch_info[1].nbg_type = AWNN_DET_POST_FACE_1;
            pNnAttr->ch_info[1].src_width = NN_FACE_SRC_WIDTH;
            pNnAttr->ch_info[1].src_height = NN_FACE_SRC_HEIGHT;
            pNnAttr->ch_info[1].thresh = 0.60;
        }
        else
        {
            pNnAttr->ch_info[1].nbg_type = -1;
        }
        pNnAttr->ch_info[1].isp = pConfig->mFourIsp;
        pNnAttr->ch_info[1].vipp = pConfig->mFourNnVipp;
        pNnAttr->ch_info[1].viChn = 0;
        pNnAttr->ch_info[1].pixel_format = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        pNnAttr->ch_info[1].vi_buf_num = pConfig->mFourNnViBufNum;
        pNnAttr->ch_info[1].src_frame_rate = pConfig->mFourNnSrcFrameRate;
        strncpy(pNnAttr->ch_info[1].model_file, pConfig->mFourNnNbgFilePath, NN_NBG_MAX_FILE_PATH_SIZE);
        pNnAttr->ch_info[1].draw_orl_enable = pConfig->mFourNnDrawOrlEnable;
        pNnAttr->ch_info[1].draw_orl_vipp = pConfig->mFourVipp;
        pNnAttr->ch_info[1].draw_orl_src_width = pConfig->mFourSrcWidth;
        pNnAttr->ch_info[1].draw_orl_src_height = pConfig->mFourSrcHeight;
        pNnAttr->ch_info[1].region_hdl_base = 200;
    }
    else
    {
        pNnAttr->ch_info[1].ch_idx = 1;
        pNnAttr->ch_info[1].nbg_type = -1;
    }
}

static void* getWbYuvThread(void *pThreadData)
{
    int result = 0;
    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)pThreadData;
    VencStreamContext *pStreamContext = NULL;

    if (pContext->mConfigPara.mMainVEncChn == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mMainStream;
    }
    else if (pContext->mConfigPara.mMain2ndVEncChn == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mMain2ndStream;
    }
    else if (pContext->mConfigPara.mSubVEncChn == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mSubStream;
    }
    else if (pContext->mConfigPara.mSub2ndVEncChn == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mSub2ndStream;
    }
    else if (pContext->mConfigPara.mThreeVEncChn == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mThreeStream;
    }
    else if (pContext->mConfigPara.mThree2ndVEncChn == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mThree2ndStream;
    }
    else if (pContext->mConfigPara.mFourVEncChn == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mFourStream;
    }
    else if (pContext->mConfigPara.mFour2ndVEncChn == pContext->mConfigPara.mWbYuvStreamChn)
    {
        pStreamContext = &pContext->mFour2ndStream;
    }
    else
    {
        aloge("fatal error, WbYuvStreamChn:%d is not support for wb yuv!", pContext->mConfigPara.mWbYuvStreamChn);
        return NULL;
    }

    FILE* fp_wb_yuv = fopen(pContext->mConfigPara.mWbYuvFilePath, "wb");
    if (NULL == fp_wb_yuv)
    {
        aloge("fatal error! why open file[%s] fail? errno is %d", pContext->mConfigPara.mWbYuvFilePath, errno);
        return NULL;
    }

    unsigned int wbYuvCnt = 0;
    unsigned int preCnt = 0;
    unsigned mHadEnableWbYuv = 0;
    unsigned int wb_wdith  = 0;
    unsigned int wb_height = 0;

    if (PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        wb_wdith  = pStreamContext->mVEncChnAttr.VeAttr.AttrH264e.PicWidth;
        wb_height = pStreamContext->mVEncChnAttr.VeAttr.AttrH264e.PicHeight;
    }
    else if (PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        wb_wdith  = pStreamContext->mVEncChnAttr.VeAttr.AttrH265e.mPicWidth;
        wb_height = pStreamContext->mVEncChnAttr.VeAttr.AttrH265e.mPicHeight;
    }
    else
    {
        aloge("fatal error, Type:%d is not support for wb yuv!", pStreamContext->mVEncChnAttr.VeAttr.Type);
        if (fp_wb_yuv)
        {
            fclose(fp_wb_yuv);
            fp_wb_yuv = NULL;
        }
        return NULL;
    }
    wb_wdith  = AWALIGN(wb_wdith, 16);
    wb_height = AWALIGN(wb_height, 16);

    while (!pContext->mbExitFlag && wbYuvCnt < pContext->mConfigPara.mWbYuvTotalCnt)
    {
        if (pStreamContext->mStreamDataCnt < pContext->mConfigPara.mWbYuvStartIndex)
        {
            usleep(10*1000);
            continue;
        }
        else
        {
            if (mHadEnableWbYuv == 0)
            {
                mHadEnableWbYuv = 1;
                sWbYuvParam mWbYuvParam;
                memset(&mWbYuvParam, 0, sizeof(sWbYuvParam));
                mWbYuvParam.bEnableWbYuv = 1;
                mWbYuvParam.nWbBufferNum = pContext->mConfigPara.mWbYuvBufNum;
                mWbYuvParam.scalerRatio  = VENC_ISP_SCALER_0;
                result = AW_MPI_VENC_SetWbYuv(pStreamContext->mVEncChn, &mWbYuvParam);
                if (result)
                {
                    aloge("fatal error, VencChn[%d] SetWbYuv failed! ret:%d", pStreamContext->mVEncChn, result);
                }
            }
        }

        if (fp_wb_yuv)
        {
            VencThumbInfo mThumbInfo;
            memset(&mThumbInfo, 0, sizeof(VencThumbInfo));
            mThumbInfo.bWriteToFile = 1;
            mThumbInfo.fp = fp_wb_yuv;
            int startTime = getSysTickMs();
            result = AW_MPI_VENC_GetWbYuv(pStreamContext->mVEncChn, &mThumbInfo);
            int endTime = getSysTickMs();
            if (result == 0)
            {
                wbYuvCnt++;
                alogd("get wb yuv[%dx%d], curWbCnt = %d, saveTotalCnt = %d, time = %d ms, encodeCnt = %d, diffCnt = %d",
                    wb_wdith, wb_height, wbYuvCnt, pContext->mConfigPara.mWbYuvTotalCnt,
                    endTime - startTime, mThumbInfo.nEncodeCnt, (mThumbInfo.nEncodeCnt - preCnt));

                preCnt = mThumbInfo.nEncodeCnt;
            }
            else
            {
                usleep(10*1000);
            }
        }
        else
        {
            alogw("exit thread: fp_wb_yuv = %p", fp_wb_yuv);
        }
    }

    if (fp_wb_yuv)
    {
        fclose(fp_wb_yuv);
        fp_wb_yuv = NULL;
    }

    alogd("exit");

    return (void*)result;
}

void requestKeyFrameFunc(int nVeChn)
{
    int ret = AW_MPI_VENC_RequestIDR(nVeChn, TRUE);
    if (ret)
    {
        aloge("fatal error! VeChn[%d] req IDR failed, ret=%d", nVeChn, ret);
    }
    else
    {
        alogd("***** VeChn[%d] req IDR *****", nVeChn);
    }
}

static void* getVencStreamThread(void *pThreadData)
{
    VencStreamContext *pStreamContext = (VencStreamContext*)pThreadData;
    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)pStreamContext->priv;
    char strThreadName[32];
    sprintf(strThreadName, "venc%d-stream", pStreamContext->mVEncChn);
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    ERRORTYPE ret = SUCCESS;
    int result = 0;
    unsigned int nStreamLen = 0;

    VENC_STREAM_S stVencStream;
    VENC_PACK_S stVencPack;
    memset(&stVencStream, 0, sizeof(stVencStream));
    memset(&stVencPack, 0, sizeof(stVencPack));
    stVencStream.mPackCount = 1;
    stVencStream.mpPack = &stVencPack;

#ifdef SUPPORT_SAVE_STREAM
    if (pStreamContext->mRecordHandler >= 0)
    {
        RECORD_CONFIG_S mRecConfig;
        memset(&mRecConfig, 0, sizeof(RECORD_CONFIG_S));
        mRecConfig.mVeChn = pStreamContext->mVEncChn;
        mRecConfig.mRecordHandler = pStreamContext->mRecordHandler;
        if (pContext->mConfigPara.mMainVEncChn == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mMainSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mMainSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mMainFilePath);
        }
        else if (pContext->mConfigPara.mMain2ndVEncChn == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mMain2ndSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mMain2ndSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mMain2ndFilePath);
        }
        else if (pContext->mConfigPara.mSubVEncChn == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mSubSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mSubSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mSubFilePath);
        }
        else if (pContext->mConfigPara.mSub2ndVEncChn == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mSub2ndSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mSub2ndSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mSub2ndFilePath);
        }
        else if (pContext->mConfigPara.mThreeVEncChn == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mThreeSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mThreeSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mThreeFilePath);
        }
        else if (pContext->mConfigPara.mThree2ndVEncChn == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mThree2ndSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mThree2ndSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mThree2ndFilePath);
        }
        else if (pContext->mConfigPara.mFourVEncChn == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mFourSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mFourSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mFourFilePath);
        }
        else if (pContext->mConfigPara.mFour2ndVEncChn == pStreamContext->mVEncChn)
        {
            mRecConfig.mMaxFileCnt = pContext->mConfigPara.mFour2ndSaveMaxFileCnt;
            mRecConfig.mMaxFileDuration = pContext->mConfigPara.mFour2ndSaveOneFileDuration*1000*1000;
            snprintf(mRecConfig.mFilePath, MAX_RECORD_FILE_PATH_LEN, "%s", pContext->mConfigPara.mFour2ndFilePath);
        }
        else
        {
            aloge("fatal error! invalid venc chn %d", pStreamContext->mVEncChn);
        }
        mRecConfig.requestKeyFrame = requestKeyFrameFunc;
        record_set_config(&mRecConfig);
    }
#endif

    int nEncodeWidth = 0;
    int nEncodeHeight = 0;
    unsigned int buf_size = 0;
    unsigned char *stream_buf = NULL;
    int rtsp_test_enable = 0;
    if (pContext->mConfigPara.mMainVEncChn == pStreamContext->mVEncChn)
    {
        nEncodeWidth = pContext->mConfigPara.mMainEncodeWidth;
        nEncodeHeight = pContext->mConfigPara.mMainEncodeHeight;
        if (pContext->mConfigPara.mMainRtspID >= 0)
            rtsp_test_enable = 1;
    }
    else if (pContext->mConfigPara.mMain2ndVEncChn == pStreamContext->mVEncChn)
    {
        nEncodeWidth = pContext->mConfigPara.mMain2ndEncodeWidth;
        nEncodeHeight = pContext->mConfigPara.mMain2ndEncodeHeight;
    }
    else if (pContext->mConfigPara.mSubVEncChn == pStreamContext->mVEncChn)
    {
        nEncodeWidth = pContext->mConfigPara.mSubEncodeWidth;
        nEncodeHeight = pContext->mConfigPara.mSubEncodeHeight;
        if (pContext->mConfigPara.mSubRtspID >= 0)
            rtsp_test_enable = 1;
    }
    else if (pContext->mConfigPara.mSub2ndVEncChn == pStreamContext->mVEncChn)
    {
        nEncodeWidth = pContext->mConfigPara.mSub2ndEncodeWidth;
        nEncodeHeight = pContext->mConfigPara.mSub2ndEncodeHeight;
    }
    else if (pContext->mConfigPara.mThreeVEncChn == pStreamContext->mVEncChn)
    {
        nEncodeWidth = pContext->mConfigPara.mThreeEncodeWidth;
        nEncodeHeight = pContext->mConfigPara.mThreeEncodeHeight;
        if (pContext->mConfigPara.mThreeRtspID >= 0)
            rtsp_test_enable = 1;
    }
    else if (pContext->mConfigPara.mThree2ndVEncChn == pStreamContext->mVEncChn)
    {
        nEncodeWidth = pContext->mConfigPara.mThree2ndEncodeWidth;
        nEncodeHeight = pContext->mConfigPara.mThree2ndEncodeHeight;
    }
    else if (pContext->mConfigPara.mFourVEncChn == pStreamContext->mVEncChn)
    {
        nEncodeWidth = pContext->mConfigPara.mFourEncodeWidth;
        nEncodeHeight = pContext->mConfigPara.mFourEncodeHeight;
        if (pContext->mConfigPara.mFourRtspID >= 0)
            rtsp_test_enable = 1;
    }
    else if (pContext->mConfigPara.mFour2ndVEncChn == pStreamContext->mVEncChn)
    {
        nEncodeWidth = pContext->mConfigPara.mFour2ndEncodeWidth;
        nEncodeHeight = pContext->mConfigPara.mFour2ndEncodeHeight;
    }
    else
    {
        aloge("fatal error! invalid venc chn %d", pStreamContext->mVEncChn);
        return NULL;
    }

    if (0 == pContext->mConfigPara.mStreamBufSize)
    {
        buf_size = nEncodeWidth * nEncodeHeight * 3 / 2;
    }
    else
    {
        buf_size = pContext->mConfigPara.mStreamBufSize;
    }
    if (0 == buf_size)
    {
        aloge("fatal error! VencChn[%d] buf_size is 0.", pStreamContext->mVEncChn);
        return NULL;
    }
    stream_buf = (unsigned char *)malloc(buf_size);
    if (NULL == stream_buf)
    {
        aloge("malloc stream_buf failed, size=%d", buf_size);
        return NULL;
    }
    memset(stream_buf, 0, buf_size);
    alogd("VencChn[%d] stream_buf:%p, size=%d", pStreamContext->mVEncChn, stream_buf, buf_size);

#ifdef SUPPORT_RTSP_TEST
    if (rtsp_test_enable)
    {
        if (pContext->mConfigPara.mMainVEncChn == pStreamContext->mVEncChn)
        {
            rtsp_start(pContext->mConfigPara.mMainRtspID);
        }
        else if (pContext->mConfigPara.mSubVEncChn == pStreamContext->mVEncChn)
        {
            rtsp_start(pContext->mConfigPara.mSubRtspID);
        }
        else if (pContext->mConfigPara.mThreeVEncChn == pStreamContext->mVEncChn)
        {
            rtsp_start(pContext->mConfigPara.mThreeRtspID);
        }
        else if (pContext->mConfigPara.mFourVEncChn == pStreamContext->mVEncChn)
        {
            rtsp_start(pContext->mConfigPara.mFourRtspID);
        }
    }
#endif

    int mActualFrameRate = 0;
    int mTempFrameCnt = 0;
    int mTempCurrentTime = 0;
    int mTempLastTime = 0;
    pStreamContext->mStreamDataCnt = 0;

    while (!pContext->mbExitFlag)
    {
        memset(stVencStream.mpPack, 0, sizeof(VENC_PACK_S));
        ret = AW_MPI_VENC_GetStream(pStreamContext->mVEncChn, &stVencStream, 4000);
        if(SUCCESS == ret)
        {
            pStreamContext->mStreamDataCnt++;
            nStreamLen = stVencStream.mpPack[0].mLen0 + stVencStream.mpPack[0].mLen1 + stVencStream.mpPack[0].mLen2;
            if (nStreamLen <= 0)
            {
                aloge("fatal error! VencStream length error,[%d,%d,%d]!", stVencStream.mpPack[0].mLen0, stVencStream.mpPack[0].mLen1, stVencStream.mpPack[0].mLen2);
            }

            // check actual framerate.
            mTempFrameCnt++;
            if (0 == mTempLastTime)
            {
                mTempLastTime = getSysTickMs();
            }
            else
            {
                mTempCurrentTime = getSysTickMs();
                if (mTempCurrentTime >= mTempLastTime + 1000)
                {
                    mActualFrameRate = mTempFrameCnt;
                    if (mActualFrameRate < pStreamContext->mVEncFrameRateConfig.DstFrmRate)
                    {
                        int sensor_fps = 0;
                        AW_MPI_ISP_GetSensorFps(pStreamContext->mIsp, &sensor_fps);
                        alogv("sensor_fps:%d, venc fps:%d", sensor_fps, pStreamContext->mVEncFrameRateConfig.DstFrmRate);
                        if (sensor_fps == pStreamContext->mVEncFrameRateConfig.DstFrmRate)
                        {
                            alogw("VencChn[%d], actualFrameRate %d < dstFrmRate %d", pStreamContext->mVEncChn, mActualFrameRate, pStreamContext->mVEncFrameRateConfig.DstFrmRate);
                        }
                    }
                    mTempLastTime = mTempCurrentTime;
                    mTempFrameCnt = 0;
                }
            }

            uint64_t pts = stVencStream.mpPack->mPTS;
            int len = 0;
            char keyframe = 0;
            char stream_buf_empty = 0;

            if (stVencStream.mpPack != NULL && stVencStream.mpPack->mLen0 > 0)
            {
                if (PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                {
                    if (H264E_NALU_ISLICE == stVencStream.mpPack->mDataType.enH264EType)
                    {
                        if (NULL == pStreamContext->mSpsPpsInfo.pBuffer)
                        {
                            alogd("SpsPpsInfo.pBuffer = NULL!!\n");
                        }
                        if (buf_size >= pStreamContext->mSpsPpsInfo.nLength)
                        {
                            /* Get sps/pps first */
                            memcpy(stream_buf, pStreamContext->mSpsPpsInfo.pBuffer, pStreamContext->mSpsPpsInfo.nLength);
                            len += pStreamContext->mSpsPpsInfo.nLength;
                        }
                        else
                        {
                            stream_buf_empty = 1;
                            aloge("fatal error! VeChn[%d] stream buf size %d is too small, h264 spspps len=%d !", pStreamContext->mVEncChn, buf_size, pStreamContext->mSpsPpsInfo.nLength);
                        }
                        keyframe = 1;
                        alogv("***** VeChn[%d] H264 got I frame, Seq %d *****", pStreamContext->mVEncChn, stVencStream.mSeq);
                    }
                    else
                    {
                        alogv("VeChn[%d] H264 got P frame, Seq %d", pStreamContext->mVEncChn, stVencStream.mSeq);
                    }
                }
                else if (PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                {
                    if (H265E_NALU_ISLICE == stVencStream.mpPack->mDataType.enH265EType)
                    {
                        if (NULL == pStreamContext->mSpsPpsInfo.pBuffer)
                        {
                            alogd("SpsPpsInfo.pBuffer = NULL!!\n");
                        }
                        if (buf_size >= pStreamContext->mSpsPpsInfo.nLength)
                        {
                            /* Get sps/pps first */
                            memcpy(stream_buf, pStreamContext->mSpsPpsInfo.pBuffer, pStreamContext->mSpsPpsInfo.nLength);
                            len += pStreamContext->mSpsPpsInfo.nLength;
                        }
                        else
                        {
                            stream_buf_empty = 1;
                            aloge("fatal error! VeChn[%d] stream buf size %d is too small, h265 spspps len=%d !", pStreamContext->mVEncChn, buf_size, pStreamContext->mSpsPpsInfo.nLength);
                        }
                        keyframe = 1;
                        alogv("***** VeChn[%d] H265 got I frame, Seq %d *****", pStreamContext->mVEncChn, stVencStream.mSeq);
                    }
                    else
                    {
                        alogv("VeChn[%d] H265 got P frame, Seq %d", pStreamContext->mVEncChn, stVencStream.mSeq);
                    }
                }
                else
                {
                    aloge("fatal error! vencType:0x%x is wrong!", pStreamContext->mVEncChnAttr.VeAttr.Type);
                }

                if (buf_size >= (len + stVencStream.mpPack->mLen0))
                {
                    memcpy(stream_buf + len, stVencStream.mpPack->mpAddr0, stVencStream.mpPack->mLen0);
                    len += stVencStream.mpPack->mLen0;
                }
                else
                {
                    stream_buf_empty = 1;
                    aloge("fatal error! VeChn[%d] stream buf size %d is too small, len=%d !", pStreamContext->mVEncChn, buf_size, len + stVencStream.mpPack->mLen0);
                }

                if (stVencStream.mpPack->mLen1 > 0)
                {
                    if (buf_size >= (len + stVencStream.mpPack->mLen1))
                    {
                        memcpy(stream_buf + len, stVencStream.mpPack->mpAddr1, stVencStream.mpPack->mLen1);
                        len += stVencStream.mpPack->mLen1;
                    }
                    else
                    {
                        stream_buf_empty = 1;
                        aloge("fatal error! VeChn[%d] stream buf size %d is too small, len=%d !", pStreamContext->mVEncChn, buf_size, len + stVencStream.mpPack->mLen1);
                    }
                }
                if (stVencStream.mpPack->mLen2 > 0)
                {
                    if (buf_size >= (len + stVencStream.mpPack->mLen2))
                    {
                        memcpy(stream_buf + len, stVencStream.mpPack->mpAddr2, stVencStream.mpPack->mLen2);
                        len += stVencStream.mpPack->mLen2;
                    }
                    else
                    {
                        stream_buf_empty = 1;
                        aloge("fatal error! VeChn[%d] stream buf size %d is too small, len=%d !", pStreamContext->mVEncChn, buf_size, len + stVencStream.mpPack->mLen2);
                    }
                }
            }

            ret = AW_MPI_VENC_ReleaseStream(pStreamContext->mVEncChn, &stVencStream);
            if(ret != SUCCESS)
            {
                aloge("fatal error! venc_chn[%d] releaseStream fail", pStreamContext->mVEncChn);
            }

            if ((stream_buf != NULL) && len > 0 && (0 == stream_buf_empty))
            {
#ifdef SUPPORT_SAVE_STREAM
                if (pStreamContext->mRecordHandler >= 0)
                {
                    RECORD_FRAME_S mRecFrm;
                    memset(&mRecFrm, 0, sizeof(RECORD_FRAME_S));
                    mRecFrm.mpFrm = stream_buf;
                    mRecFrm.mFrmSize = len;
                    mRecFrm.mFrameType = (1 == keyframe) ? RECORD_FRAME_TYPE_I : RECORD_FRAME_TYPE_P;
                    mRecFrm.mPts = pts;
                    //alogd("record[%d] FrameType=%d", pStreamContext->mRecordHandler, mRecFrm.mFrameType);
                    int start_time = getSysTickMs();
                    record_send_data(pStreamContext->mRecordHandler, &mRecFrm);
                    int end_time = getSysTickMs();
                    if (100 < end_time - start_time)
                    {
                        alogw("venc_chn[%d] record_send_data cost %dms > 100ms", pStreamContext->mVEncChn, end_time - start_time);
                    }
                }
#endif

#ifdef SUPPORT_RTSP_TEST
                if (rtsp_test_enable)
                {
                    /* get current fps and set to rtsp to avoid rtsp checkDurationTime warn */
                    int sensor_fps = 0;
                    AW_MPI_ISP_GetSensorFps(pStreamContext->mIsp, &sensor_fps);

                    RtspSendDataParam stRtspParam;
                    memset(&stRtspParam, 0, sizeof(RtspSendDataParam));
                    stRtspParam.buf = stream_buf;
                    stRtspParam.size = len;
                    stRtspParam.frame_type = (1 == keyframe) ? RTSP_FRAME_DATA_TYPE_I : RTSP_FRAME_DATA_TYPE_P;
                    stRtspParam.pts = pts;
                    stRtspParam.frame_rate = sensor_fps;
                    alogv("VencChn[%d] rtsp id[%d] RtspParam %p %d %d %lld, %dfps",
                        pStreamContext->mVEncChn, pContext->mConfigPara.mMainRtspID,
                        stRtspParam.buf, stRtspParam.size, stRtspParam.frame_type, stRtspParam.pts, stRtspParam.frame_rate);

                    int start_time = getSysTickMs();
                    if (pContext->mConfigPara.mMainVEncChn == pStreamContext->mVEncChn)
                    {
                        rtsp_sendData(pContext->mConfigPara.mMainRtspID, &stRtspParam);
                    }
                    else if (pContext->mConfigPara.mSubVEncChn == pStreamContext->mVEncChn)
                    {
                        rtsp_sendData(pContext->mConfigPara.mSubRtspID, &stRtspParam);
                    }
                    else if (pContext->mConfigPara.mThreeVEncChn == pStreamContext->mVEncChn)
                    {
                        rtsp_sendData(pContext->mConfigPara.mThreeRtspID, &stRtspParam);
                    }
                    else if (pContext->mConfigPara.mFourVEncChn == pStreamContext->mVEncChn)
                    {
                        rtsp_sendData(pContext->mConfigPara.mFourRtspID, &stRtspParam);
                    }
                    int end_time = getSysTickMs();
                    if (100 < end_time - start_time)
                    {
                        alogw("venc_chn[%d] rtsp_sendData cost %dms > 100ms", pStreamContext->mVEncChn, end_time - start_time);
                    }
                }
#endif
            }
        }
        else
        {
            alogw("fatal error! vencChn[%d] getStream failed! check code! ret=0x%x", pStreamContext->mVEncChn, ret);
            continue;
        }
    }

    if (stream_buf)
    {
        free(stream_buf);
        stream_buf = NULL;
    }

    alogd("exit");

    return (void*)result;
}

#ifdef SUPPORT_AWAIISP
static void* aiispSwitchThread(void* pThreadData)
{
    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)pThreadData;

    char strThreadName[32];
    sprintf(strThreadName, "aiispSwitch");
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    int nIsp = 0;
    int nAiIspSwitchInterval = 0;
    int nFrameRate = 0;
    int nAiIspMode = 0;
    int nAiIspSwitchCase = 0;
    int nAiIspSwitchDropFrameNum = 0;

    /* default parameters are taken in order */
    if (pContext->mConfigPara.mMainEnable)
    {
        nIsp = pContext->mConfigPara.mMainIsp;
        nAiIspSwitchInterval = pContext->mConfigPara.mMainAiIspSwitchInterval;
        nFrameRate = pContext->mConfigPara.mMainSrcFrameRate;
        nAiIspMode = pContext->mConfigPara.mMainAiIspMode;
        nAiIspSwitchCase = pContext->mConfigPara.mMainAiIspSwitchCase;
        nAiIspSwitchDropFrameNum = pContext->mConfigPara.mMainAiIspSwitchDropFrameNum;
    }
    if (pContext->mConfigPara.mSubEnable)
    {
        nIsp = pContext->mConfigPara.mSubIsp;
        nAiIspSwitchInterval = pContext->mConfigPara.mSubAiIspSwitchInterval;
        nFrameRate = pContext->mConfigPara.mSubSrcFrameRate;
        nAiIspMode = pContext->mConfigPara.mSubAiIspMode;
        nAiIspSwitchCase = pContext->mConfigPara.mSubAiIspSwitchCase;
        nAiIspSwitchDropFrameNum = pContext->mConfigPara.mSubAiIspSwitchDropFrameNum;
    }
    if (pContext->mConfigPara.mThreeEnable)
    {
        nIsp = pContext->mConfigPara.mThreeIsp;
        nAiIspSwitchInterval = pContext->mConfigPara.mThreeAiIspSwitchInterval;
        nFrameRate = pContext->mConfigPara.mThreeSrcFrameRate;
        nAiIspMode = pContext->mConfigPara.mThreeAiIspMode;
        nAiIspSwitchCase = pContext->mConfigPara.mThreeAiIspSwitchCase;
        nAiIspSwitchDropFrameNum = pContext->mConfigPara.mThreeAiIspSwitchDropFrameNum;
    }
    if (pContext->mConfigPara.mFourEnable)
    {
        nIsp = pContext->mConfigPara.mFourIsp;
        nAiIspSwitchInterval = pContext->mConfigPara.mFourAiIspSwitchInterval;
        nFrameRate = pContext->mConfigPara.mFourSrcFrameRate;
        nAiIspMode = pContext->mConfigPara.mFourAiIspMode;
        nAiIspSwitchCase = pContext->mConfigPara.mFourAiIspSwitchCase;
        nAiIspSwitchDropFrameNum = pContext->mConfigPara.mFourAiIspSwitchDropFrameNum;
    }

    int loop_cnt = 0;
    int interval_ms = 0;
    awaiisp_mode mode = nAiIspMode;
    awaiisp_mode last_aiisp_mode = mode;

    int nAiIspAutoSwitchEnable = 0;
    if (pContext->mConfigPara.mMainAiIspAutoSwitchEnable || pContext->mConfigPara.mSubAiIspAutoSwitchEnable ||
        pContext->mConfigPara.mThreeAiIspAutoSwitchEnable || pContext->mConfigPara.mFourAiIspAutoSwitchEnable)
    {
        nAiIspAutoSwitchEnable = 1;
    }

    if (nFrameRate)
        interval_ms = 1000 / nFrameRate;
    if (0 == interval_ms)
        interval_ms = 1000;

    int night_to_day_signal_cnt = 0;
    int day_to_night_signal_cnt = 0;
    int env_light_level = 0;

    awaiisp_mode switch_normal_mode = (AWAIISP_COMMON_SWITCH_CASE_AIISP_DAY_ALL_8BIT == nAiIspSwitchCase) ? AWAIISP_MODE_NORMAL_GAMMA : AWAIISP_MODE_NORMAL;
    int nAiIspSwitchReleaseResEnable = pContext->mConfigPara.mAiIspSwitchReleaseResEnable;

    while (!pContext->mbExitFlag)
    {
        if (nAiIspAutoSwitchEnable)
        {
            // switch aiisp by ae param
            for (int i = 0; i < AWAIISP_COMMON_NUM_MAX; i++)
            {
                env_light_level = AW_MPI_ISP_GetEvLvAdj(nIsp);
            }
            alogv("isp %d, env_light_level:%d, day2nignt:%d, night2day:%d", nIsp, env_light_level, day_to_night_signal_cnt, night_to_day_signal_cnt);

            if (env_light_level < DAY_TO_NIGHT_THRESHOLD)
            {
                if (++day_to_night_signal_cnt >= DAY_TO_NIGHT_SIGNAL_CNT)
                {
                    day_to_night_signal_cnt = 0;
                    mode = AWAIISP_MODE_NPU;
                }
                night_to_day_signal_cnt = 0;
            }
            else if (env_light_level > NIGHT_TO_DAY_THRESHOLD)
            {
                if (++night_to_day_signal_cnt >= NIGHT_TO_DAY_SIGNAL_CNT)
                {
                    night_to_day_signal_cnt = 0;
                    mode = switch_normal_mode;
                }
                day_to_night_signal_cnt = 0;
            }
            else
            {
                night_to_day_signal_cnt = 0;
                day_to_night_signal_cnt = 0;
            }
            interval_ms = 100;
        }
        else
        {
            // for aiisp switch test by manually specifying interval
            if ((nAiIspSwitchInterval) && (0 == (++loop_cnt) % nAiIspSwitchInterval))
            {
                mode = (AWAIISP_MODE_NPU == last_aiisp_mode) ? switch_normal_mode : AWAIISP_MODE_NPU;
            }
        }

        if (last_aiisp_mode != mode)
        {
            if (nAiIspAutoSwitchEnable)
                alogw("switch mode %d -> %d, env_light_level:%d", last_aiisp_mode, mode, env_light_level);
            else
                alogw("switch mode %d -> %d", last_aiisp_mode, mode);

            awaiisp_common_switch_param switch_param;
            memset(&switch_param, 0, sizeof(awaiisp_common_switch_param));

            int channel_index = 0;
            if (pContext->mConfigPara.mMainEnable)
            {
                if (channel_index >= AWAIISP_COMMON_NUM_MAX)
                {
                    aloge("fatal error! channel_index %d >= %d", channel_index, AWAIISP_COMMON_NUM_MAX);
                    return (void*)(-1);
                }
                switch_param.channel_param[channel_index].enable = 1;
                switch_param.channel_param[channel_index].isp = pContext->mConfigPara.mMainIsp;
                switch_param.channel_param[channel_index].vipp = pContext->mConfigPara.mMainVipp;
                switch_param.channel_param[channel_index].switch_case = pContext->mConfigPara.mMainAiIspSwitchCase;
                switch_param.channel_param[channel_index].drop_frame_num = pContext->mConfigPara.mMainAiIspSwitchDropFrameNum;
                switch_param.channel_param[channel_index].config.mode = mode;
                if (AWAIISP_MODE_NPU == switch_param.channel_param[channel_index].config.mode)
                {
                    switch_param.channel_param[channel_index].isp_cfg_bin_path = pContext->mConfigPara.mMainAiIspCfgBinPath;
                    switch_param.channel_param[channel_index].config.release_aiisp_resources = 0;
                }
                else
                {
                    switch_param.channel_param[channel_index].isp_cfg_bin_path = pContext->mConfigPara.mMainAiIspCfgBinPath2;
                    switch_param.channel_param[channel_index].config.release_aiisp_resources = nAiIspSwitchReleaseResEnable;
                }
                channel_index++;
            }
            if (pContext->mConfigPara.mSubEnable)
            {
                if (channel_index >= AWAIISP_COMMON_NUM_MAX)
                {
                    aloge("fatal error! channel_index %d >= %d", channel_index, AWAIISP_COMMON_NUM_MAX);
                    return (void*)(-1);
                }
                switch_param.channel_param[channel_index].enable = 1;
                switch_param.channel_param[channel_index].isp = pContext->mConfigPara.mSubIsp;
                switch_param.channel_param[channel_index].vipp = pContext->mConfigPara.mSubVipp;
                switch_param.channel_param[channel_index].switch_case = pContext->mConfigPara.mSubAiIspSwitchCase;
                switch_param.channel_param[channel_index].drop_frame_num = pContext->mConfigPara.mSubAiIspSwitchDropFrameNum;
                switch_param.channel_param[channel_index].config.mode = mode;
                if (AWAIISP_MODE_NPU == switch_param.channel_param[channel_index].config.mode)
                {
                    switch_param.channel_param[channel_index].isp_cfg_bin_path = pContext->mConfigPara.mSubAiIspCfgBinPath;
                    switch_param.channel_param[channel_index].config.release_aiisp_resources = 0;
                }
                else
                {
                    switch_param.channel_param[channel_index].isp_cfg_bin_path = pContext->mConfigPara.mSubAiIspCfgBinPath2;
                    switch_param.channel_param[channel_index].config.release_aiisp_resources = nAiIspSwitchReleaseResEnable;
                }
                channel_index++;
            }
            if (pContext->mConfigPara.mThreeEnable)
            {
                if (channel_index >= AWAIISP_COMMON_NUM_MAX)
                {
                    aloge("fatal error! channel_index %d >= %d", channel_index, AWAIISP_COMMON_NUM_MAX);
                    return (void*)(-1);
                }
                switch_param.channel_param[channel_index].enable = 1;
                switch_param.channel_param[channel_index].isp = pContext->mConfigPara.mThreeIsp;
                switch_param.channel_param[channel_index].vipp = pContext->mConfigPara.mThreeVipp;
                switch_param.channel_param[channel_index].switch_case = pContext->mConfigPara.mThreeAiIspSwitchCase;
                switch_param.channel_param[channel_index].drop_frame_num = pContext->mConfigPara.mThreeAiIspSwitchDropFrameNum;
                switch_param.channel_param[channel_index].config.mode = mode;
                if (AWAIISP_MODE_NPU == switch_param.channel_param[channel_index].config.mode)
                {
                    switch_param.channel_param[channel_index].isp_cfg_bin_path = pContext->mConfigPara.mThreeAiIspCfgBinPath;
                    switch_param.channel_param[channel_index].config.release_aiisp_resources = 0;
                }
                else
                {
                    switch_param.channel_param[channel_index].isp_cfg_bin_path = pContext->mConfigPara.mThreeAiIspCfgBinPath2;
                    switch_param.channel_param[channel_index].config.release_aiisp_resources = nAiIspSwitchReleaseResEnable;
                }
                channel_index++;
            }
            if (pContext->mConfigPara.mFourEnable)
            {
                if (channel_index >= AWAIISP_COMMON_NUM_MAX)
                {
                    aloge("fatal error! channel_index %d >= %d", channel_index, AWAIISP_COMMON_NUM_MAX);
                    return (void*)(-1);
                }
                switch_param.channel_param[channel_index].enable = 1;
                switch_param.channel_param[channel_index].isp = pContext->mConfigPara.mFourIsp;
                switch_param.channel_param[channel_index].vipp = pContext->mConfigPara.mFourVipp;
                switch_param.channel_param[channel_index].switch_case = pContext->mConfigPara.mFourAiIspSwitchCase;
                switch_param.channel_param[channel_index].drop_frame_num = pContext->mConfigPara.mFourAiIspSwitchDropFrameNum;
                switch_param.channel_param[channel_index].config.mode = mode;
                if (AWAIISP_MODE_NPU == switch_param.channel_param[channel_index].config.mode)
                {
                    switch_param.channel_param[channel_index].isp_cfg_bin_path = pContext->mConfigPara.mFourAiIspCfgBinPath;
                    switch_param.channel_param[channel_index].config.release_aiisp_resources = 0;
                }
                else
                {
                    switch_param.channel_param[channel_index].isp_cfg_bin_path = pContext->mConfigPara.mFourAiIspCfgBinPath2;
                    switch_param.channel_param[channel_index].config.release_aiisp_resources = nAiIspSwitchReleaseResEnable;
                }
                channel_index++;
            }
            awaiisp_common_switch_mode(&switch_param);

            last_aiisp_mode = mode;
        }

        usleep(interval_ms * 1000);
    }

    return NULL;
}
#endif

static void getVencSpsPpsInfo(VencStreamContext *pStreamContext)
{
    int ret = 0;
    memset(&pStreamContext->mSpsPpsInfo, 0, sizeof(VencHeaderData));
    if(PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        ret = AW_MPI_VENC_GetH264SpsPpsInfo(pStreamContext->mVEncChn, &pStreamContext->mSpsPpsInfo);
        if(ret != SUCCESS)
        {
            aloge("fatal error! get spspps fail[0x%x]!", ret);
        }
    }
    else if(PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
    {
        ret = AW_MPI_VENC_GetH265SpsPpsInfo(pStreamContext->mVEncChn, &pStreamContext->mSpsPpsInfo);
        if(ret != SUCCESS)
        {
            aloge("fatal error! get spspps fail[0x%x]!", ret);
        }
    }
    else
    {
        aloge("fatal error! vencType:0x%x is wrong!", pStreamContext->mVEncChnAttr.VeAttr.Type);
    }
}

static void DrawStreamOSD(SampleSmartIPCDemoContext *pContext, int mVeChn, char *text, int x, int y, int idx)
{
#ifdef SUPPORT_STREAM_OSD_TEST
    RGN_ATTR_S stRegion;
    BITMAP_S stBitmap;
    RGN_CHN_ATTR_S stRgnChnAttr;
    int overlay_x = 0;
    int overlay_y = 0;
    int ret = 0;
    FONT_SIZE_TYPE font_size = FONT_SIZE_32;

    overlay_x = x;
    overlay_y = y;
    overlay_x = AWALIGN(overlay_x, 16);
    overlay_y = AWALIGN(overlay_y, 16);
    alogd("StreamOSD[%d] coordinate(%d,%d), font_size=%d", idx, overlay_x, overlay_y, font_size);

    ret = load_font_file(font_size);
    if (ret < 0)
    {
        aloge("load_font_file %d fail! ret:%d\n", ret, font_size);
    }

    FONT_RGBPIC_S font_pic;
    memset(&font_pic, 0, sizeof(FONT_RGBPIC_S));
    font_pic.font_type     = font_size;
    font_pic.rgb_type      = OSD_RGB_32;
    font_pic.enable_bg     = 0;
    font_pic.foreground[0] = 0x6;
    font_pic.foreground[1] = 0x1;
    font_pic.foreground[2] = 0xFF;
    font_pic.foreground[3] = 0xFF;
    font_pic.background[0] = 0x21;
    font_pic.background[1] = 0x21;
    font_pic.background[2] = 0x21;
    font_pic.background[3] = 0x11;

    memset(&pContext->mRgbPic[idx], 0, sizeof(RGB_PIC_S));
    pContext->mRgbPic[idx].enable_mosaic = 0;
    pContext->mRgbPic[idx].rgb_type      = OSD_RGB_32;
    create_font_rectangle(text, &font_pic, &pContext->mRgbPic[idx]);

    memset(&stRegion, 0, sizeof(RGN_ATTR_S));
    stRegion.enType = OVERLAY_RGN;
    stRegion.unAttr.stOverlay.mPixelFmt = MM_PIXEL_FORMAT_RGB_8888;
    stRegion.unAttr.stOverlay.mSize.Width = pContext->mRgbPic[idx].wide;
    stRegion.unAttr.stOverlay.mSize.Height = pContext->mRgbPic[idx].high;
    AW_MPI_RGN_Create(pContext->mOverlayDrawStreamOSDBase + idx, &stRegion);

    memset(&stBitmap, 0, sizeof(BITMAP_S));
    stBitmap.mPixelFormat = stRegion.unAttr.stOverlay.mPixelFmt;
    stBitmap.mWidth = stRegion.unAttr.stOverlay.mSize.Width;
    stBitmap.mHeight = stRegion.unAttr.stOverlay.mSize.Height;
    stBitmap.mpData  = pContext->mRgbPic[idx].pic_addr;
    AW_MPI_RGN_SetBitMap(pContext->mOverlayDrawStreamOSDBase + idx, &stBitmap);

    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, mVeChn};
    memset(&stRgnChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    stRgnChnAttr.bShow = TRUE;
    stRgnChnAttr.enType = stRegion.enType;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.X = overlay_x;
    stRgnChnAttr.unChnAttr.stOverlayChn.stPoint.Y = overlay_y;
    stRgnChnAttr.unChnAttr.stOverlayChn.mLayer = 0;
    stRgnChnAttr.unChnAttr.stOverlayChn.mFgAlpha = 0x40; // global alpha mode value for ARGB1555
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Width = 16;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.Height = 16;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.mLumThresh = 60;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUMDIFF_THRESH;
    stRgnChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = TRUE; // OSD反色
    AW_MPI_RGN_AttachToChn(pContext->mOverlayDrawStreamOSDBase + idx, &VeChn, &stRgnChnAttr);
#endif
}

static void DestroyStreamOSD(SampleSmartIPCDemoContext *pContext, int mVeChn, int idx)
{
#ifdef SUPPORT_STREAM_OSD_TEST
    if (idx >= 16)
    {
        aloge("fatal error! invalid idx %d", idx);
        return;
    }
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, mVeChn};
    release_rgb_picture(&pContext->mRgbPic[idx]);
    AW_MPI_RGN_DetachFromChn(pContext->mOverlayDrawStreamOSDBase + idx, &VeChn);
    AW_MPI_RGN_Destroy(pContext->mOverlayDrawStreamOSDBase + idx);
#endif
}

static ERRORTYPE resetCamera(SampleSmartIPCDemoContext *pContext, int vipp, BOOL enable)
{
    VencStreamContext *pStreamContext = NULL;
    int mVipp = -1;
    int mIsp = -1;
    int mViChn = -1;
    int mVEncChn = -1;
    VI_ATTR_S mViAttr;
    int AIChnFlag = 0;

    alogd("enter, vipp%d", vipp);

    if (vipp == pContext->mMainStream.mVipp)
    {
        alogd("MainStream");
        pStreamContext = &pContext->mMainStream;
        mVipp = pStreamContext->mVipp;
        mIsp = pStreamContext->mIsp;
        mViChn = pStreamContext->mViChn;
        mVEncChn = pStreamContext->mVEncChn;
    }
    else if (vipp == pContext->mMain2ndStream.mVipp)
    {
        alogd("Main2ndStream");
        pStreamContext = &pContext->mMain2ndStream;
        mVipp = pStreamContext->mVipp;
        mIsp = pStreamContext->mIsp;
        mViChn = pStreamContext->mViChn;
        mVEncChn = pStreamContext->mVEncChn;
    }
    else if (vipp == pContext->mSubStream.mVipp)
    {
        alogd("SubStream");
        pStreamContext = &pContext->mSubStream;
        mVipp = pStreamContext->mVipp;
        mIsp = pStreamContext->mIsp;
        mViChn = pStreamContext->mViChn;
        mVEncChn = pStreamContext->mVEncChn;
    }
    else if (vipp == pContext->mSub2ndStream.mVipp)
    {
        alogd("Sub2ndStream");
        pStreamContext = &pContext->mSub2ndStream;
        mVipp = pStreamContext->mVipp;
        mIsp = pStreamContext->mIsp;
        mViChn = pStreamContext->mViChn;
        mVEncChn = pStreamContext->mVEncChn;
    }
    else if (vipp == pContext->mThreeStream.mVipp)
    {
        alogd("ThreeStream");
        pStreamContext = &pContext->mThreeStream;
        mVipp = pStreamContext->mVipp;
        mIsp = pStreamContext->mIsp;
        mViChn = pStreamContext->mViChn;
        mVEncChn = pStreamContext->mVEncChn;
    }
    else if (vipp == pContext->mThree2ndStream.mVipp)
    {
        alogd("Three2ndStream");
        pStreamContext = &pContext->mThree2ndStream;
        mVipp = pStreamContext->mVipp;
        mIsp = pStreamContext->mIsp;
        mViChn = pStreamContext->mViChn;
        mVEncChn = pStreamContext->mVEncChn;
    }
    else if (vipp == pContext->mFourStream.mVipp)
    {
        alogd("FourStream");
        pStreamContext = &pContext->mFourStream;
        mVipp = pStreamContext->mVipp;
        mIsp = pStreamContext->mIsp;
        mViChn = pStreamContext->mViChn;
        mVEncChn = pStreamContext->mVEncChn;
    }
    else if (vipp == pContext->mFour2ndStream.mVipp)
    {
        alogd("Four2ndStream");
        pStreamContext = &pContext->mFour2ndStream;
        mVipp = pStreamContext->mVipp;
        mIsp = pStreamContext->mIsp;
        mViChn = pStreamContext->mViChn;
        mVEncChn = pStreamContext->mVEncChn;
    }
#ifdef SUPPORT_AI_SERVICE
    else if (vipp == pContext->mConfigPara.mMainNnVipp && pContext->mConfigPara.mMainNnEnable)
    {
        alogd("MainNn");
        mVipp = pContext->mConfigPara.mMainNnVipp;
        mIsp = pContext->mConfigPara.mMainIsp;
        mViChn = 0;
        AIChnFlag = 1;
    }
    else if (vipp == pContext->mConfigPara.mSubNnVipp && pContext->mConfigPara.mSubNnEnable)
    {
        alogd("SubNn");
        mVipp = pContext->mConfigPara.mSubNnVipp;
        mIsp = pContext->mConfigPara.mSubIsp;
        mViChn = 0;
        AIChnFlag = 1;
    }
    else if (vipp == pContext->mConfigPara.mThreeNnVipp && pContext->mConfigPara.mThreeNnEnable)
    {
        alogd("ThreeNn");
        mVipp = pContext->mConfigPara.mThreeNnVipp;
        mIsp = pContext->mConfigPara.mThreeIsp;
        mViChn = 0;
        AIChnFlag = 1;
    }
    else if (vipp == pContext->mConfigPara.mFourNnVipp && pContext->mConfigPara.mFourNnEnable)
    {
        alogd("FourNn");
        mVipp = pContext->mConfigPara.mFourNnVipp;
        mIsp = pContext->mConfigPara.mFourIsp;
        mViChn = 0;
        AIChnFlag = 1;
    }
#endif
    else
    {
        alogd("invalid vipp %d", vipp);
        return -1;
    }

    if (FALSE == enable)
    {
        if (0 == AIChnFlag)
        {
            alogd("vipp%d viChn%d stop vi&venc", mVipp, mViChn);
            AW_MPI_VI_DisableVirChn(mVipp, mViChn);
            if (-1 != mVEncChn)
            {
                AW_MPI_VENC_StopRecvPic(mVEncChn);
                //AW_MPI_VENC_DestroyEncoder(mVEncChn);
                //AW_MPI_VENC_ResetChn(mVEncChn);
                MPP_CHN_S ViChn = {MOD_ID_VIU, mVipp, mViChn};
                MPP_CHN_S VeChn = {MOD_ID_VENC, 0, mVEncChn};
                AW_MPI_SYS_UnBind(&ViChn, &VeChn);
            }
            AW_MPI_VI_DestroyVirChn(mVipp, mViChn);
            AW_MPI_VI_DisableVipp(mVipp);
            AW_MPI_ISP_Stop(mIsp);
            AW_MPI_VI_DestroyVipp(mVipp);
        }
#ifdef SUPPORT_AI_SERVICE
        else
        {
            alogd("vipp%d viChn%d stop ai_service", mVipp, mViChn);
            ai_service_stop();
        }
#endif
    }
    else
    {
        if (0 == AIChnFlag)
        {
            alogd("vipp%d viChn%d prepare vi&venc", mVipp, mViChn);
            AW_MPI_VI_CreateVipp(mVipp);
            AW_MPI_VI_SetVippAttr(mVipp, &pStreamContext->mViAttr);
            AW_MPI_ISP_Run(mIsp);
            AW_MPI_VI_CreateVirChn(mVipp, mViChn, NULL);
            MPPCallbackInfo cbInfo;
            cbInfo.cookie = (void*)pContext;
            cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
            AW_MPI_VI_RegisterCallback(mVipp, &cbInfo);
            AW_MPI_VI_EnableVipp(mVipp);
            if (-1 != mVEncChn)
            {
                MPP_CHN_S ViChn = {MOD_ID_VIU, mVipp, mViChn};
                MPP_CHN_S VeChn = {MOD_ID_VENC, 0, mVEncChn};
                AW_MPI_SYS_Bind(&ViChn, &VeChn);
            }
            alogd("vipp%d start vi&venc", mVipp);
            AW_MPI_VI_EnableVirChn(mVipp, mViChn);
            if (-1 != mVEncChn)
            {
                AW_MPI_VENC_StartRecvPic(mVEncChn);
            }
        }
#ifdef SUPPORT_AI_SERVICE
        else
        {
            alogd("vipp%d viChn%d start ai_service", mVipp, mViChn);
            ai_service_attr_t aiservice_attr;
            memset(&aiservice_attr, 0, sizeof(ai_service_attr_t));
            configAiService(&pContext->mConfigPara, &aiservice_attr);
            ai_service_start(&aiservice_attr);
        }
#endif
    }

    alogd("ok, vipp%d", mVipp);

    return SUCCESS;
}

static void *MsgQueueThread(void *pThreadData)
{
    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)pThreadData;
    message_t stCmdMsg;
    SmartIPCMsgType cmd;
    int nCmdPara;

    alogd("msg queue thread start run!");
    while (1)
    {
        if (0 == get_message(&pContext->mMsgQueue, &stCmdMsg))
        {
            cmd = stCmdMsg.command;
            nCmdPara = stCmdMsg.para0;

            switch (cmd)
            {
                case Vi_Timeout:
                {
                    if (0 == pContext->mConfigPara.mViTimeoutResetDisable)
                    {
                        int vipp = nCmdPara;
                        alogd("vipp[%d] got Vi Timeout, reset Camera", vipp);
                        if (pContext->mConfigPara.mMainEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mMainVipp, FALSE);
                        }
                        if (pContext->mConfigPara.mMain2ndEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mMain2ndVipp, FALSE);
                        }
                        if (-1 != pContext->mConfigPara.mMainNnNbgType || -1 != pContext->mConfigPara.mSubNnNbgType ||
                            -1 != pContext->mConfigPara.mThreeNnNbgType || -1 != pContext->mConfigPara.mFourNnNbgType)
                        {
                            // reset ai_service together
                            resetCamera(pContext, pContext->mConfigPara.mMainNnVipp, FALSE);
                        }
                        if (pContext->mConfigPara.mSubEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mSubVipp, FALSE);
                        }
                        if (pContext->mConfigPara.mSub2ndEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mSub2ndVipp, FALSE);
                        }
                        if (pContext->mConfigPara.mThreeEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mThreeVipp, FALSE);
                        }
                        if (pContext->mConfigPara.mThree2ndEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mThree2ndVipp, FALSE);
                        }
                        if (pContext->mConfigPara.mFourEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mFourVipp, FALSE);
                        }
                        if (pContext->mConfigPara.mFour2ndEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mFour2ndVipp, FALSE);
                        }
                        usleep(100*1000);
                        if (pContext->mConfigPara.mMainEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mMainVipp, TRUE);
                        }
                        if (pContext->mConfigPara.mMain2ndEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mMain2ndVipp, TRUE);
                        }
                        if (-1 != pContext->mConfigPara.mMainNnNbgType || -1 != pContext->mConfigPara.mSubNnNbgType ||
                            -1 != pContext->mConfigPara.mThreeNnNbgType || -1 != pContext->mConfigPara.mFourNnNbgType)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mMainNnVipp, TRUE);
                        }
                        if (pContext->mConfigPara.mSubEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mSubVipp, TRUE);
                        }
                        if (pContext->mConfigPara.mSub2ndEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mSub2ndVipp, TRUE);
                        }
                        if (pContext->mConfigPara.mThreeEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mThreeVipp, TRUE);
                        }
                        if (pContext->mConfigPara.mThree2ndEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mThree2ndVipp, TRUE);
                        }
                        if (pContext->mConfigPara.mFourEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mFourVipp, TRUE);
                        }
                        if (pContext->mConfigPara.mFour2ndEnable)
                        {
                            resetCamera(pContext, pContext->mConfigPara.mFour2ndVipp, TRUE);
                        }
                        alogd("vipp[%d] got Vi Timeout, reset Camera done", vipp);
                    }
                    break;
                }
                case MsgQueue_Stop:
                {
                    goto _Exit;
                }
                default:
                {
                    break;
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&pContext->mMsgQueue, 0);
        }
    }
_Exit:
    alogd("msg queue thread exit!");
    return NULL;
}

// just for stress test
static void *testTriggerThread(void *pThreadData)
{
    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)pThreadData;
    int vipp = 0;
    int interval_ms = 1000;

    char strThreadName[32];
    sprintf(strThreadName, "testTrigger");
    prctl(PR_SET_NAME, (unsigned long)strThreadName, 0, 0, 0);

    if (pContext->mConfigPara.mTestTriggerViTimeout > 0)
    {
        interval_ms = pContext->mConfigPara.mTestTriggerViTimeout;
    }

    alogd("test thread start run!");
    while (!pContext->mbExitFlag)
    {
        usleep(interval_ms*1000);
        alogd("test vi timeout. vipp:%d, interval_ms:%d", vipp, interval_ms);

        message_t stCmdMsg;
        InitMessage(&stCmdMsg);
        stCmdMsg.command = Vi_Timeout;
        stCmdMsg.para0 = vipp;
        putMessageWithData(&pContext->mMsgQueue, &stCmdMsg);
    }

    alogd("test thread exit!");
    return NULL;
}

static int prepare(SampleSmartIPCDemoContext *pContext)
{
    int ret = 0;

    if (pContext->mConfigPara.mMainEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mMainStream;
        pStreamContext->priv = (void*)pContext;
        configMainStream(pStreamContext, &pContext->mConfigPara);
        AW_MPI_VI_CreateVipp(pStreamContext->mVipp);
        AW_MPI_VI_SetVippAttr(pStreamContext->mVipp, &pStreamContext->mViAttr);

        //AW_MPI_VI_SetVippMirror(pStreamContext->mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pStreamContext->mVipp, 1);

        if (100 <= pContext->mConfigPara.mMainIspD3dLbcRatio && 400 >= pContext->mConfigPara.mMainIspD3dLbcRatio)
        {
            alogd("Isp[%d] set D3dLbcRatio %d", pStreamContext->mIsp, pContext->mConfigPara.mMainIspD3dLbcRatio);
            AW_MPI_ISP_SetD3dLbcRatio(pStreamContext->mIsp, pContext->mConfigPara.mMainIspD3dLbcRatio);
        }

        AW_MPI_ISP_Run(pStreamContext->mIsp);
        if (pContext->mConfigPara.mMainLdciUseExtBufEnable)
        {
            gtm_ldci_common_config_param ldci_param;
            memset(&ldci_param, 0, sizeof(gtm_ldci_common_config_param));
            ldci_param.ldci_vipp = pContext->mConfigPara.mMainLdciVipp;
            ldci_param.ldci_fps = pContext->mConfigPara.mMainSrcFrameRate;
            gtm_ldci_common_open(pStreamContext->mIsp, &ldci_param);
        }
#ifdef SUPPORT_AWAIISP
        /* awaiisp_common_enable() must be called after all vipps are created. */
        if (pContext->mConfigPara.mMainAiIspEnable)
        {
            awaiisp_common_config_param common_param;
            memset(&common_param, 0, sizeof(awaiisp_common_config_param));
            strncpy(common_param.config.lut_model_file, pContext->mConfigPara.mMainAiIspLutNbgFilePath, AWAIISP_FILE_PATH_MAX);
            strncpy(common_param.config.model_file, pContext->mConfigPara.mMainAiIspNbgFilePath, AWAIISP_FILE_PATH_MAX);
            common_param.config.model_version = pContext->mConfigPara.mMainAiIspModelVersion;
            common_param.config.width = pContext->mConfigPara.mMainAiIspWidth;
            common_param.config.height = pContext->mConfigPara.mMainAiIspHeight;
            common_param.config.tdm_rxbuf_cnt = pContext->mConfigPara.mMainAiIspTdmRxBufNum;
            common_param.config.reserve0 = pContext->mConfigPara.mMainAiIspReserve0;
            common_param.config.reserve1 = pContext->mConfigPara.mMainAiIspReserve1;
            common_param.config.reserve2 = pContext->mConfigPara.mMainAiIspReserve2;
            common_param.config.npu_ref_buf_reduce_enable = pContext->mConfigPara.mAiIspNpuRefBufReduceEnable;
            common_param.config.mode = pContext->mConfigPara.mMainAiIspMode;
            if (AWAIISP_MODE_NPU == pContext->mConfigPara.mMainAiIspMode)
            {
                common_param.isp_cfg_bin_path = pContext->mConfigPara.mMainAiIspCfgBinPath;
                common_param.config.unprepared_aiisp_resources_advance = 0;
            }
            else
            {
                common_param.isp_cfg_bin_path = pContext->mConfigPara.mMainAiIspCfgBinPath2;
                if (pContext->mConfigPara.mMainAiIspAutoSwitchEnable || pContext->mConfigPara.mMainAiIspSwitchInterval)
                    common_param.config.unprepared_aiisp_resources_advance = 0;
                else
                    common_param.config.unprepared_aiisp_resources_advance = 1;
            }
            awaiisp_common_enable(pStreamContext->mIsp, &common_param);
        }
        else
#endif
        {
            if (-1 != pContext->mConfigPara.mMainIspTdmRawProcessType)
            {
                tdm_raw_process_config_param tdm_config_param;
                memset(&tdm_config_param, 0, sizeof(tdm_raw_process_config_param));
                tdm_config_param.type = pContext->mConfigPara.mMainIspTdmRawProcessType;
                tdm_config_param.width = pContext->mConfigPara.mMainAiIspWidth;
                tdm_config_param.height = pContext->mConfigPara.mMainAiIspHeight;
                tdm_config_param.frame_cnt_min = pContext->mConfigPara.mMainIspTdmRawProcessFrameCntMin;
                tdm_config_param.frame_cnt_max = pContext->mConfigPara.mMainIspTdmRawProcessFrameCntMax;
                strncpy(tdm_config_param.tdm_raw_file_path, pContext->mConfigPara.mMainIspTdmRawFilePath, TDM_RAW_PROCESS_FILE_PATH_MAX_LEN);
                tdm_raw_process_open(pStreamContext->mIsp, &tdm_config_param);
                tdm_raw_process_start(pStreamContext->mIsp);
            }
        }

        AW_MPI_VI_EnableVipp(pStreamContext->mVipp);

#ifdef SUPPORT_SAVE_STREAM
        if (strlen(pContext->mConfigPara.mMainFilePath) > 0)
        {
            pStreamContext->mRecordHandler = pStreamContext->mVEncChn;
        }
#endif

        AW_MPI_VI_CreateVirChn(pStreamContext->mVipp, pStreamContext->mViChn, NULL);
        AW_MPI_VENC_CreateChn(pStreamContext->mVEncChn, &pStreamContext->mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pStreamContext->mVEncChn, &pStreamContext->mVEncRcParam);

#ifdef ENABLE_VENC_ADVANCED_PARAM
        /* set framerate in AW_MPI_VENC_CreateChn */
        //AW_MPI_VENC_SetFrameRate(pStreamContext->mVEncChn, &pStreamContext->mVEncFrameRateConfig);
        setVenc2Dnr(pStreamContext->mVEncChn);
        setVenc3Dnr(pStreamContext->mVEncChn);
        setVencSuperFrameCfg(pStreamContext->mVEncChn, pContext->mConfigPara.mMainEncodeBitrate, pContext->mConfigPara.mMainEncodeFrameRate);
#else
        alogd("VencChn[%d] use the default venc params for product mode %d", pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.RcAttr.mProductMode);
#endif

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pStreamContext->mVEncChn, &cbInfo);
        AW_MPI_VI_RegisterCallback(pStreamContext->mVipp, &cbInfo);

        if (pStreamContext->mIspAndVeLinkageEnable)
        {
            VENC_IspVeLinkAttr stIspVeLinkAttr;
            memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            stIspVeLinkAttr.bEnableVe2Isp = TRUE; //main camera main stream enable Ve2isp
            stIspVeLinkAttr.nVipp = pStreamContext->mVipp;
            AW_MPI_VENC_EnableIspVeLink(pStreamContext->mVEncChn, &stIspVeLinkAttr);
            alogd("VencChn[%d] ispVeLink:%d-%d-%d", pStreamContext->mVEncChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
                stIspVeLinkAttr.nVipp);
        }

        if (pStreamContext->mCameraAdaptiveMovingAndStaticEnable)
        {
            setVencLensMovingMaxQp(pStreamContext->mVEncChn, pContext->mConfigPara.mVencLensMovingMaxQp);
        }

        MPP_CHN_S ViChn = {MOD_ID_VIU, pStreamContext->mVipp, pStreamContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pStreamContext->mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
        alogd("mainStream vipp:%d viChn:%d veChn:%d", pStreamContext->mVipp, pStreamContext->mViChn, pStreamContext->mVEncChn);
    }
    if (pContext->mConfigPara.mMain2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mMain2ndStream;
        pStreamContext->priv = (void*)pContext;
        configMain2ndStream(pStreamContext, &pContext->mConfigPara);
        AW_MPI_VI_CreateVipp(pStreamContext->mVipp);
        AW_MPI_VI_SetVippAttr(pStreamContext->mVipp, &pStreamContext->mViAttr);

        //AW_MPI_VI_SetVippMirror(pStreamContext->mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pStreamContext->mVipp, 1);

        AW_MPI_VI_EnableVipp(pStreamContext->mVipp);

#ifdef SUPPORT_SAVE_STREAM
        if (strlen(pContext->mConfigPara.mMain2ndFilePath) > 0)
        {
            pStreamContext->mRecordHandler = pStreamContext->mVEncChn;
        }
#endif

        AW_MPI_VI_CreateVirChn(pStreamContext->mVipp, pStreamContext->mViChn, NULL);
        AW_MPI_VENC_CreateChn(pStreamContext->mVEncChn, &pStreamContext->mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pStreamContext->mVEncChn, &pStreamContext->mVEncRcParam);

#ifdef ENABLE_VENC_ADVANCED_PARAM
        /* set framerate in AW_MPI_VENC_CreateChn */
        //AW_MPI_VENC_SetFrameRate(pStreamContext->mVEncChn, &pStreamContext->mVEncFrameRateConfig);
        setVenc2Dnr(pStreamContext->mVEncChn);
        setVenc3Dnr(pStreamContext->mVEncChn);
        setVencSuperFrameCfg(pStreamContext->mVEncChn, pContext->mConfigPara.mMain2ndEncodeBitrate, pContext->mConfigPara.mMain2ndEncodeFrameRate);
#else
        alogd("VencChn[%d] use the default venc params for product mode %d", pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.RcAttr.mProductMode);
#endif

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pStreamContext->mVEncChn, &cbInfo);
        AW_MPI_VI_RegisterCallback(pStreamContext->mVipp, &cbInfo);

        if (pStreamContext->mIspAndVeLinkageEnable)
        {
            VENC_IspVeLinkAttr stIspVeLinkAttr;
            memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            stIspVeLinkAttr.bEnableVe2Isp = FALSE;  //main camera sub stream disable Ve2isp
            stIspVeLinkAttr.nVipp = pStreamContext->mVipp;
            AW_MPI_VENC_EnableIspVeLink(pStreamContext->mVEncChn, &stIspVeLinkAttr);
            alogd("VencChn[%d] ispVeLink:%d-%d-%d", pStreamContext->mVEncChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
                stIspVeLinkAttr.nVipp);
        }

        if (pStreamContext->mCameraAdaptiveMovingAndStaticEnable)
        {
            setVencLensMovingMaxQp(pStreamContext->mVEncChn, pContext->mConfigPara.mVencLensMovingMaxQp);
        }

        MPP_CHN_S ViChn = {MOD_ID_VIU, pStreamContext->mVipp, pStreamContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pStreamContext->mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
        alogd("main2ndStream vipp:%d viChn:%d veChn:%d", pStreamContext->mVipp, pStreamContext->mViChn, pStreamContext->mVEncChn);
    }
    if (pContext->mConfigPara.mSubEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mSubStream;
        pStreamContext->priv = (void*)pContext;
        configSubStream(pStreamContext, &pContext->mConfigPara);
        AW_MPI_VI_CreateVipp(pStreamContext->mVipp);
        AW_MPI_VI_SetVippAttr(pStreamContext->mVipp, &pStreamContext->mViAttr);

        if (100 <= pContext->mConfigPara.mSubIspD3dLbcRatio && 400 >= pContext->mConfigPara.mSubIspD3dLbcRatio)
        {
            alogd("Isp[%d] set D3dLbcRatio %d", pStreamContext->mIsp, pContext->mConfigPara.mSubIspD3dLbcRatio);
            AW_MPI_ISP_SetD3dLbcRatio(pStreamContext->mIsp, pContext->mConfigPara.mSubIspD3dLbcRatio);
        }

        //AW_MPI_VI_SetVippMirror(pStreamContext->mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pStreamContext->mVipp, 1);

        AW_MPI_ISP_Run(pStreamContext->mIsp);
        if (pContext->mConfigPara.mSubLdciUseExtBufEnable)
        {
            gtm_ldci_common_config_param ldci_param;
            memset(&ldci_param, 0, sizeof(gtm_ldci_common_config_param));
            ldci_param.ldci_vipp = pContext->mConfigPara.mSubLdciVipp;
            ldci_param.ldci_fps = pContext->mConfigPara.mSubSrcFrameRate;
            gtm_ldci_common_open(pStreamContext->mIsp, &ldci_param);
        }
#ifdef SUPPORT_AWAIISP
        if (pContext->mConfigPara.mSubAiIspEnable)
        {
            awaiisp_common_config_param common_param;
            memset(&common_param, 0, sizeof(awaiisp_common_config_param));
            strncpy(common_param.config.lut_model_file, pContext->mConfigPara.mSubAiIspLutNbgFilePath, AWAIISP_FILE_PATH_MAX);
            strncpy(common_param.config.model_file, pContext->mConfigPara.mSubAiIspNbgFilePath, AWAIISP_FILE_PATH_MAX);
            common_param.config.model_version = pContext->mConfigPara.mSubAiIspModelVersion;
            common_param.config.width = pContext->mConfigPara.mSubAiIspWidth;
            common_param.config.height = pContext->mConfigPara.mSubAiIspHeight;
            common_param.config.tdm_rxbuf_cnt = pContext->mConfigPara.mSubAiIspTdmRxBufNum;
            common_param.config.reserve0 = pContext->mConfigPara.mSubAiIspReserve0;
            common_param.config.reserve1 = pContext->mConfigPara.mSubAiIspReserve1;
            common_param.config.reserve2 = pContext->mConfigPara.mSubAiIspReserve2;
            common_param.config.npu_ref_buf_reduce_enable = pContext->mConfigPara.mAiIspNpuRefBufReduceEnable;
            common_param.config.mode = pContext->mConfigPara.mSubAiIspMode;
            if (AWAIISP_MODE_NPU == pContext->mConfigPara.mSubAiIspMode)
            {
                common_param.isp_cfg_bin_path = pContext->mConfigPara.mSubAiIspCfgBinPath;
                common_param.config.unprepared_aiisp_resources_advance = 0;
            }
            else
            {
                common_param.isp_cfg_bin_path = pContext->mConfigPara.mSubAiIspCfgBinPath2;
                if (pContext->mConfigPara.mSubAiIspAutoSwitchEnable || pContext->mConfigPara.mSubAiIspSwitchInterval)
                    common_param.config.unprepared_aiisp_resources_advance = 0;
                else
                    common_param.config.unprepared_aiisp_resources_advance = 1;
            }
            awaiisp_common_enable(pStreamContext->mIsp, &common_param);
        }
        else
#endif
        {
            if (-1 != pContext->mConfigPara.mSubIspTdmRawProcessType)
            {
                tdm_raw_process_config_param tdm_config_param;
                memset(&tdm_config_param, 0, sizeof(tdm_raw_process_config_param));
                tdm_config_param.type = pContext->mConfigPara.mSubIspTdmRawProcessType;
                tdm_config_param.width = pContext->mConfigPara.mSubAiIspWidth;
                tdm_config_param.height = pContext->mConfigPara.mSubAiIspHeight;
                tdm_config_param.frame_cnt_min = pContext->mConfigPara.mSubIspTdmRawProcessFrameCntMin;
                tdm_config_param.frame_cnt_max = pContext->mConfigPara.mSubIspTdmRawProcessFrameCntMax;
                strncpy(tdm_config_param.tdm_raw_file_path, pContext->mConfigPara.mSubIspTdmRawFilePath, TDM_RAW_PROCESS_FILE_PATH_MAX_LEN);
                tdm_raw_process_open(pStreamContext->mIsp, &tdm_config_param);
                tdm_raw_process_start(pStreamContext->mIsp);
            }
        }

        AW_MPI_VI_EnableVipp(pStreamContext->mVipp);

#ifdef SUPPORT_SAVE_STREAM
        if (strlen(pContext->mConfigPara.mSubFilePath) > 0)
        {
            pStreamContext->mRecordHandler = pStreamContext->mVEncChn;
        }
#endif

        AW_MPI_VI_CreateVirChn(pStreamContext->mVipp, pStreamContext->mViChn, NULL);
        AW_MPI_VENC_CreateChn(pStreamContext->mVEncChn, &pStreamContext->mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pStreamContext->mVEncChn, &pStreamContext->mVEncRcParam);

#ifdef ENABLE_VENC_ADVANCED_PARAM
        /* set framerate in AW_MPI_VENC_CreateChn */
        //AW_MPI_VENC_SetFrameRate(pStreamContext->mVEncChn, &pStreamContext->mVEncFrameRateConfig);
        setVenc2Dnr(pStreamContext->mVEncChn);
        setVenc3Dnr(pStreamContext->mVEncChn);
        setVencSuperFrameCfg(pStreamContext->mVEncChn, pContext->mConfigPara.mSubEncodeBitrate, pContext->mConfigPara.mSubEncodeFrameRate);
#else
        alogd("VencChn[%d] use the default venc params for product mode %d", pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.RcAttr.mProductMode);
#endif

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pStreamContext->mVEncChn, &cbInfo);
        AW_MPI_VI_RegisterCallback(pStreamContext->mVipp, &cbInfo);

        if (pStreamContext->mIspAndVeLinkageEnable)
        {
            VENC_IspVeLinkAttr stIspVeLinkAttr;
            memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            stIspVeLinkAttr.bEnableVe2Isp = TRUE; //sub camera main stream enable Ve2isp
            stIspVeLinkAttr.nVipp = pStreamContext->mVipp;
            AW_MPI_VENC_EnableIspVeLink(pStreamContext->mVEncChn, &stIspVeLinkAttr);
            alogd("VencChn[%d] ispVeLink:%d-%d-%d", pStreamContext->mVEncChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
                stIspVeLinkAttr.nVipp);
        }

        if (pStreamContext->mCameraAdaptiveMovingAndStaticEnable)
        {
            setVencLensMovingMaxQp(pStreamContext->mVEncChn, pContext->mConfigPara.mVencLensMovingMaxQp);
        }

        MPP_CHN_S ViChn = {MOD_ID_VIU, pStreamContext->mVipp, pStreamContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pStreamContext->mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
        alogd("subStream vipp:%d viChn:%d veChn:%d", pStreamContext->mVipp, pStreamContext->mViChn, pStreamContext->mVEncChn);
    }
    if (pContext->mConfigPara.mSub2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mSub2ndStream;
        pStreamContext->priv = (void*)pContext;
        configSub2ndStream(pStreamContext, &pContext->mConfigPara);
        AW_MPI_VI_CreateVipp(pStreamContext->mVipp);
        AW_MPI_VI_SetVippAttr(pStreamContext->mVipp, &pStreamContext->mViAttr);

        //AW_MPI_VI_SetVippMirror(pStreamContext->mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pStreamContext->mVipp, 1);

        AW_MPI_VI_EnableVipp(pStreamContext->mVipp);

#ifndef SUPPORT_SAVE_STREAM
        if (strlen(pContext->mConfigPara.mSub2ndFilePath) > 0)
        {
            pStreamContext->mFile = fopen(pContext->mConfigPara.mSub2ndFilePath, "wb");
            if(NULL == pStreamContext->mFile)
            {
                aloge("fatal error! why open file[%s] fail? errno is %d", pContext->mConfigPara.mSub2ndFilePath, errno);
            }
        }
        else
        {
            pStreamContext->mFile = NULL;
        }
#else
        if (strlen(pContext->mConfigPara.mSub2ndFilePath) > 0)
        {
            pStreamContext->mRecordHandler = pStreamContext->mVEncChn;
        }
#endif

        AW_MPI_VI_CreateVirChn(pStreamContext->mVipp, pStreamContext->mViChn, NULL);
        AW_MPI_VENC_CreateChn(pStreamContext->mVEncChn, &pStreamContext->mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pStreamContext->mVEncChn, &pStreamContext->mVEncRcParam);

#ifdef ENABLE_VENC_ADVANCED_PARAM
        /* set framerate in AW_MPI_VENC_CreateChn */
        //AW_MPI_VENC_SetFrameRate(pStreamContext->mVEncChn, &pStreamContext->mVEncFrameRateConfig);
        setVenc2Dnr(pStreamContext->mVEncChn);
        setVenc3Dnr(pStreamContext->mVEncChn);
        setVencSuperFrameCfg(pStreamContext->mVEncChn, pContext->mConfigPara.mSub2ndEncodeBitrate, pContext->mConfigPara.mSub2ndEncodeFrameRate);
#else
        alogd("VencChn[%d] use the default venc params for product mode %d", pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.RcAttr.mProductMode);
#endif

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pStreamContext->mVEncChn, &cbInfo);
        AW_MPI_VI_RegisterCallback(pStreamContext->mVipp, &cbInfo);

        if (pStreamContext->mIspAndVeLinkageEnable)
        {
            VENC_IspVeLinkAttr stIspVeLinkAttr;
            memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            stIspVeLinkAttr.bEnableVe2Isp = FALSE; //sub camera sub stream disable Ve2isp
            stIspVeLinkAttr.nVipp = pStreamContext->mVipp;
            AW_MPI_VENC_EnableIspVeLink(pStreamContext->mVEncChn, &stIspVeLinkAttr);
            alogd("VencChn[%d] ispVeLink:%d-%d-%d", pStreamContext->mVEncChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
                stIspVeLinkAttr.nVipp);
        }

        if (pStreamContext->mCameraAdaptiveMovingAndStaticEnable)
        {
            setVencLensMovingMaxQp(pStreamContext->mVEncChn, pContext->mConfigPara.mVencLensMovingMaxQp);
        }

        MPP_CHN_S ViChn = {MOD_ID_VIU, pStreamContext->mVipp, pStreamContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pStreamContext->mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
        alogd("sub2ndStream vipp:%d viChn:%d veChn:%d", pStreamContext->mVipp, pStreamContext->mViChn, pStreamContext->mVEncChn);
    }
    if (pContext->mConfigPara.mThreeEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mThreeStream;
        pStreamContext->priv = (void*)pContext;
        configThreeStream(pStreamContext, &pContext->mConfigPara);
        AW_MPI_VI_CreateVipp(pStreamContext->mVipp);
        AW_MPI_VI_SetVippAttr(pStreamContext->mVipp, &pStreamContext->mViAttr);

        if (100 <= pContext->mConfigPara.mThreeIspD3dLbcRatio && 400 >= pContext->mConfigPara.mThreeIspD3dLbcRatio)
        {
            alogd("Isp[%d] set D3dLbcRatio %d", pStreamContext->mIsp, pContext->mConfigPara.mThreeIspD3dLbcRatio);
            AW_MPI_ISP_SetD3dLbcRatio(pStreamContext->mIsp, pContext->mConfigPara.mThreeIspD3dLbcRatio);
        }

        //AW_MPI_VI_SetVippMirror(pStreamContext->mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pStreamContext->mVipp, 1);

        AW_MPI_ISP_Run(pStreamContext->mIsp);
        if (pContext->mConfigPara.mThreeLdciUseExtBufEnable)
        {
            gtm_ldci_common_config_param ldci_param;
            memset(&ldci_param, 0, sizeof(gtm_ldci_common_config_param));
            ldci_param.ldci_vipp = pContext->mConfigPara.mThreeLdciVipp;
            ldci_param.ldci_fps = pContext->mConfigPara.mThreeSrcFrameRate;
            gtm_ldci_common_open(pStreamContext->mIsp, &ldci_param);
        }
#ifdef SUPPORT_AWAIISP
        if (pContext->mConfigPara.mThreeAiIspEnable)
        {
            awaiisp_common_config_param common_param;
            memset(&common_param, 0, sizeof(awaiisp_common_config_param));
            strncpy(common_param.config.lut_model_file, pContext->mConfigPara.mThreeAiIspLutNbgFilePath, AWAIISP_FILE_PATH_MAX);
            strncpy(common_param.config.model_file, pContext->mConfigPara.mThreeAiIspNbgFilePath, AWAIISP_FILE_PATH_MAX);
            common_param.config.model_version = pContext->mConfigPara.mThreeAiIspModelVersion;
            common_param.config.width = pContext->mConfigPara.mThreeAiIspWidth;
            common_param.config.height = pContext->mConfigPara.mThreeAiIspHeight;
            common_param.config.tdm_rxbuf_cnt = pContext->mConfigPara.mThreeAiIspTdmRxBufNum;
            common_param.config.reserve0 = pContext->mConfigPara.mThreeAiIspReserve0;
            common_param.config.reserve1 = pContext->mConfigPara.mThreeAiIspReserve1;
            common_param.config.reserve2 = pContext->mConfigPara.mThreeAiIspReserve2;
            common_param.config.npu_ref_buf_reduce_enable = pContext->mConfigPara.mAiIspNpuRefBufReduceEnable;
            common_param.config.mode = pContext->mConfigPara.mThreeAiIspMode;
            if (AWAIISP_MODE_NPU == pContext->mConfigPara.mThreeAiIspMode)
            {
                common_param.isp_cfg_bin_path = pContext->mConfigPara.mThreeAiIspCfgBinPath;
                common_param.config.unprepared_aiisp_resources_advance = 0;
            }
            else
            {
                common_param.isp_cfg_bin_path = pContext->mConfigPara.mThreeAiIspCfgBinPath2;
                if (pContext->mConfigPara.mThreeAiIspAutoSwitchEnable || pContext->mConfigPara.mThreeAiIspSwitchInterval)
                    common_param.config.unprepared_aiisp_resources_advance = 0;
                else
                    common_param.config.unprepared_aiisp_resources_advance = 1;
            }
            awaiisp_common_enable(pStreamContext->mIsp, &common_param);
        }
        else
#endif
        {
            if (-1 != pContext->mConfigPara.mThreeIspTdmRawProcessType)
            {
                tdm_raw_process_config_param tdm_config_param;
                memset(&tdm_config_param, 0, sizeof(tdm_raw_process_config_param));
                tdm_config_param.type = pContext->mConfigPara.mThreeIspTdmRawProcessType;
                tdm_config_param.width = pContext->mConfigPara.mThreeAiIspWidth;
                tdm_config_param.height = pContext->mConfigPara.mThreeAiIspHeight;
                tdm_config_param.frame_cnt_min = pContext->mConfigPara.mThreeIspTdmRawProcessFrameCntMin;
                tdm_config_param.frame_cnt_max = pContext->mConfigPara.mThreeIspTdmRawProcessFrameCntMax;
                strncpy(tdm_config_param.tdm_raw_file_path, pContext->mConfigPara.mThreeIspTdmRawFilePath, TDM_RAW_PROCESS_FILE_PATH_MAX_LEN);
                tdm_raw_process_open(pStreamContext->mIsp, &tdm_config_param);
                tdm_raw_process_start(pStreamContext->mIsp);
            }
        }

        AW_MPI_VI_EnableVipp(pStreamContext->mVipp);

#ifdef SUPPORT_SAVE_STREAM
        if (strlen(pContext->mConfigPara.mThreeFilePath) > 0)
        {
            pStreamContext->mRecordHandler = pStreamContext->mVEncChn;
        }
#endif

        AW_MPI_VI_CreateVirChn(pStreamContext->mVipp, pStreamContext->mViChn, NULL);
        AW_MPI_VENC_CreateChn(pStreamContext->mVEncChn, &pStreamContext->mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pStreamContext->mVEncChn, &pStreamContext->mVEncRcParam);

#ifdef ENABLE_VENC_ADVANCED_PARAM
        /* set framerate in AW_MPI_VENC_CreateChn */
        //AW_MPI_VENC_SetFrameRate(pStreamContext->mVEncChn, &pStreamContext->mVEncFrameRateConfig);
        setVenc2Dnr(pStreamContext->mVEncChn);
        setVenc3Dnr(pStreamContext->mVEncChn);
        setVencSuperFrameCfg(pStreamContext->mVEncChn, pContext->mConfigPara.mThreeEncodeBitrate, pContext->mConfigPara.mThreeEncodeFrameRate);
#else
        alogd("VencChn[%d] use the default venc params for product mode %d", pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.RcAttr.mProductMode);
#endif

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pStreamContext->mVEncChn, &cbInfo);
        AW_MPI_VI_RegisterCallback(pStreamContext->mVipp, &cbInfo);

        if (pStreamContext->mIspAndVeLinkageEnable)
        {
            VENC_IspVeLinkAttr stIspVeLinkAttr;
            memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            stIspVeLinkAttr.bEnableVe2Isp = TRUE; //3rd camera main stream enable Ve2isp
            stIspVeLinkAttr.nVipp = pStreamContext->mVipp;
            AW_MPI_VENC_EnableIspVeLink(pStreamContext->mVEncChn, &stIspVeLinkAttr);
            alogd("VencChn[%d] ispVeLink:%d-%d-%d", pStreamContext->mVEncChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
                stIspVeLinkAttr.nVipp);
        }

        if (pStreamContext->mCameraAdaptiveMovingAndStaticEnable)
        {
            setVencLensMovingMaxQp(pStreamContext->mVEncChn, pContext->mConfigPara.mVencLensMovingMaxQp);
        }

        MPP_CHN_S ViChn = {MOD_ID_VIU, pStreamContext->mVipp, pStreamContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pStreamContext->mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
        alogd("subStream vipp:%d viChn:%d veChn:%d", pStreamContext->mVipp, pStreamContext->mViChn, pStreamContext->mVEncChn);
    }
    if (pContext->mConfigPara.mThree2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mThree2ndStream;
        pStreamContext->priv = (void*)pContext;
        configThree2ndStream(pStreamContext, &pContext->mConfigPara);
        AW_MPI_VI_CreateVipp(pStreamContext->mVipp);
        AW_MPI_VI_SetVippAttr(pStreamContext->mVipp, &pStreamContext->mViAttr);

        //AW_MPI_VI_SetVippMirror(pStreamContext->mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pStreamContext->mVipp, 1);

        AW_MPI_VI_EnableVipp(pStreamContext->mVipp);

#ifndef SUPPORT_SAVE_STREAM
        if (strlen(pContext->mConfigPara.mThree2ndFilePath) > 0)
        {
            pStreamContext->mFile = fopen(pContext->mConfigPara.mThree2ndFilePath, "wb");
            if(NULL == pStreamContext->mFile)
            {
                aloge("fatal error! why open file[%s] fail? errno is %d", pContext->mConfigPara.mThree2ndFilePath, errno);
            }
        }
        else
        {
            pStreamContext->mFile = NULL;
        }
#else
        if (strlen(pContext->mConfigPara.mThree2ndFilePath) > 0)
        {
            pStreamContext->mRecordHandler = pStreamContext->mVEncChn;
        }
#endif

        AW_MPI_VI_CreateVirChn(pStreamContext->mVipp, pStreamContext->mViChn, NULL);
        AW_MPI_VENC_CreateChn(pStreamContext->mVEncChn, &pStreamContext->mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pStreamContext->mVEncChn, &pStreamContext->mVEncRcParam);

#ifdef ENABLE_VENC_ADVANCED_PARAM
        /* set framerate in AW_MPI_VENC_CreateChn */
        //AW_MPI_VENC_SetFrameRate(pStreamContext->mVEncChn, &pStreamContext->mVEncFrameRateConfig);
        setVenc2Dnr(pStreamContext->mVEncChn);
        setVenc3Dnr(pStreamContext->mVEncChn);
        setVencSuperFrameCfg(pStreamContext->mVEncChn, pContext->mConfigPara.mThree2ndEncodeBitrate, pContext->mConfigPara.mThree2ndEncodeFrameRate);
#else
        alogd("VencChn[%d] use the default venc params for product mode %d", pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.RcAttr.mProductMode);
#endif

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pStreamContext->mVEncChn, &cbInfo);
        AW_MPI_VI_RegisterCallback(pStreamContext->mVipp, &cbInfo);

        if (pStreamContext->mIspAndVeLinkageEnable)
        {
            VENC_IspVeLinkAttr stIspVeLinkAttr;
            memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            stIspVeLinkAttr.bEnableVe2Isp = FALSE; //3rd camera sub stream disable Ve2isp
            stIspVeLinkAttr.nVipp = pStreamContext->mVipp;
            AW_MPI_VENC_EnableIspVeLink(pStreamContext->mVEncChn, &stIspVeLinkAttr);
            alogd("VencChn[%d] ispVeLink:%d-%d-%d", pStreamContext->mVEncChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
                stIspVeLinkAttr.nVipp);
        }

        if (pStreamContext->mCameraAdaptiveMovingAndStaticEnable)
        {
            setVencLensMovingMaxQp(pStreamContext->mVEncChn, pContext->mConfigPara.mVencLensMovingMaxQp);
        }

        MPP_CHN_S ViChn = {MOD_ID_VIU, pStreamContext->mVipp, pStreamContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pStreamContext->mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
        alogd("sub2ndStream vipp:%d viChn:%d veChn:%d", pStreamContext->mVipp, pStreamContext->mViChn, pStreamContext->mVEncChn);
    }
    if (pContext->mConfigPara.mFourEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mFourStream;
        pStreamContext->priv = (void*)pContext;
        configFourStream(pStreamContext, &pContext->mConfigPara);
        AW_MPI_VI_CreateVipp(pStreamContext->mVipp);
        AW_MPI_VI_SetVippAttr(pStreamContext->mVipp, &pStreamContext->mViAttr);

        if (100 <= pContext->mConfigPara.mFourIspD3dLbcRatio && 400 >= pContext->mConfigPara.mFourIspD3dLbcRatio)
        {
            alogd("Isp[%d] set D3dLbcRatio %d", pStreamContext->mIsp, pContext->mConfigPara.mFourIspD3dLbcRatio);
            AW_MPI_ISP_SetD3dLbcRatio(pStreamContext->mIsp, pContext->mConfigPara.mFourIspD3dLbcRatio);
        }

        //AW_MPI_VI_SetVippMirror(pStreamContext->mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pStreamContext->mVipp, 1);

        AW_MPI_ISP_Run(pStreamContext->mIsp);
        if (pContext->mConfigPara.mFourLdciUseExtBufEnable)
        {
            gtm_ldci_common_config_param ldci_param;
            memset(&ldci_param, 0, sizeof(gtm_ldci_common_config_param));
            ldci_param.ldci_vipp = pContext->mConfigPara.mFourLdciVipp;
            ldci_param.ldci_fps = pContext->mConfigPara.mFourSrcFrameRate;
            gtm_ldci_common_open(pStreamContext->mIsp, &ldci_param);
        }
#ifdef SUPPORT_AWAIISP
        if (pContext->mConfigPara.mFourAiIspEnable)
        {
            awaiisp_common_config_param common_param;
            memset(&common_param, 0, sizeof(awaiisp_common_config_param));
            strncpy(common_param.config.lut_model_file, pContext->mConfigPara.mFourAiIspLutNbgFilePath, AWAIISP_FILE_PATH_MAX);
            strncpy(common_param.config.model_file, pContext->mConfigPara.mFourAiIspNbgFilePath, AWAIISP_FILE_PATH_MAX);
            common_param.config.model_version = pContext->mConfigPara.mFourAiIspModelVersion;
            common_param.config.width = pContext->mConfigPara.mFourAiIspWidth;
            common_param.config.height = pContext->mConfigPara.mFourAiIspHeight;
            common_param.config.tdm_rxbuf_cnt = pContext->mConfigPara.mFourAiIspTdmRxBufNum;
            common_param.config.reserve0 = pContext->mConfigPara.mFourAiIspReserve0;
            common_param.config.reserve1 = pContext->mConfigPara.mFourAiIspReserve1;
            common_param.config.reserve2 = pContext->mConfigPara.mFourAiIspReserve2;
            common_param.config.npu_ref_buf_reduce_enable = pContext->mConfigPara.mAiIspNpuRefBufReduceEnable;
            common_param.config.mode = pContext->mConfigPara.mFourAiIspMode;
            if (AWAIISP_MODE_NPU == pContext->mConfigPara.mFourAiIspMode)
            {
                common_param.isp_cfg_bin_path = pContext->mConfigPara.mFourAiIspCfgBinPath;
                common_param.config.unprepared_aiisp_resources_advance = 0;
            }
            else
            {
                common_param.isp_cfg_bin_path = pContext->mConfigPara.mFourAiIspCfgBinPath2;
                if (pContext->mConfigPara.mFourAiIspAutoSwitchEnable || pContext->mConfigPara.mFourAiIspSwitchInterval)
                    common_param.config.unprepared_aiisp_resources_advance = 0;
                else
                    common_param.config.unprepared_aiisp_resources_advance = 1;
            }
            awaiisp_common_enable(pStreamContext->mIsp, &common_param);
        }
        else
#endif
        {
            if (-1 != pContext->mConfigPara.mFourIspTdmRawProcessType)
            {
                tdm_raw_process_config_param tdm_config_param;
                memset(&tdm_config_param, 0, sizeof(tdm_raw_process_config_param));
                tdm_config_param.type = pContext->mConfigPara.mFourIspTdmRawProcessType;
                tdm_config_param.width = pContext->mConfigPara.mFourAiIspWidth;
                tdm_config_param.height = pContext->mConfigPara.mFourAiIspHeight;
                tdm_config_param.frame_cnt_min = pContext->mConfigPara.mFourIspTdmRawProcessFrameCntMin;
                tdm_config_param.frame_cnt_max = pContext->mConfigPara.mFourIspTdmRawProcessFrameCntMax;
                strncpy(tdm_config_param.tdm_raw_file_path, pContext->mConfigPara.mFourIspTdmRawFilePath, TDM_RAW_PROCESS_FILE_PATH_MAX_LEN);
                tdm_raw_process_open(pStreamContext->mIsp, &tdm_config_param);
                tdm_raw_process_start(pStreamContext->mIsp);
            }
        }

        AW_MPI_VI_EnableVipp(pStreamContext->mVipp);

#ifdef SUPPORT_SAVE_STREAM
        if (strlen(pContext->mConfigPara.mFourFilePath) > 0)
        {
            pStreamContext->mRecordHandler = pStreamContext->mVEncChn;
        }
#endif

        AW_MPI_VI_CreateVirChn(pStreamContext->mVipp, pStreamContext->mViChn, NULL);
        AW_MPI_VENC_CreateChn(pStreamContext->mVEncChn, &pStreamContext->mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pStreamContext->mVEncChn, &pStreamContext->mVEncRcParam);

#ifdef ENABLE_VENC_ADVANCED_PARAM
        /* set framerate in AW_MPI_VENC_CreateChn */
        //AW_MPI_VENC_SetFrameRate(pStreamContext->mVEncChn, &pStreamContext->mVEncFrameRateConfig);
        setVenc2Dnr(pStreamContext->mVEncChn);
        setVenc3Dnr(pStreamContext->mVEncChn);
        setVencSuperFrameCfg(pStreamContext->mVEncChn, pContext->mConfigPara.mFourEncodeBitrate, pContext->mConfigPara.mFourEncodeFrameRate);
#else
        alogd("VencChn[%d] use the default venc params for product mode %d", pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.RcAttr.mProductMode);
#endif

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pStreamContext->mVEncChn, &cbInfo);
        AW_MPI_VI_RegisterCallback(pStreamContext->mVipp, &cbInfo);

        if (pStreamContext->mIspAndVeLinkageEnable)
        {
            VENC_IspVeLinkAttr stIspVeLinkAttr;
            memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            stIspVeLinkAttr.bEnableVe2Isp = TRUE; //4th camera main stream enable Ve2isp
            stIspVeLinkAttr.nVipp = pStreamContext->mVipp;
            AW_MPI_VENC_EnableIspVeLink(pStreamContext->mVEncChn, &stIspVeLinkAttr);
            alogd("VencChn[%d] ispVeLink:%d-%d-%d", pStreamContext->mVEncChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
                stIspVeLinkAttr.nVipp);
        }

        if (pStreamContext->mCameraAdaptiveMovingAndStaticEnable)
        {
            setVencLensMovingMaxQp(pStreamContext->mVEncChn, pContext->mConfigPara.mVencLensMovingMaxQp);
        }

        MPP_CHN_S ViChn = {MOD_ID_VIU, pStreamContext->mVipp, pStreamContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pStreamContext->mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
        alogd("subStream vipp:%d viChn:%d veChn:%d", pStreamContext->mVipp, pStreamContext->mViChn, pStreamContext->mVEncChn);
    }
    if (pContext->mConfigPara.mFour2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mFour2ndStream;
        pStreamContext->priv = (void*)pContext;
        configFour2ndStream(pStreamContext, &pContext->mConfigPara);
        AW_MPI_VI_CreateVipp(pStreamContext->mVipp);
        AW_MPI_VI_SetVippAttr(pStreamContext->mVipp, &pStreamContext->mViAttr);

        //AW_MPI_VI_SetVippMirror(pStreamContext->mVipp, 0);
        //AW_MPI_VI_SetVippFlip(pStreamContext->mVipp, 1);

        AW_MPI_VI_EnableVipp(pStreamContext->mVipp);

#ifndef SUPPORT_SAVE_STREAM
        if (strlen(pContext->mConfigPara.mFour2ndFilePath) > 0)
        {
            pStreamContext->mFile = fopen(pContext->mConfigPara.mFour2ndFilePath, "wb");
            if(NULL == pStreamContext->mFile)
            {
                aloge("fatal error! why open file[%s] fail? errno is %d", pContext->mConfigPara.mFour2ndFilePath, errno);
            }
        }
        else
        {
            pStreamContext->mFile = NULL;
        }
#else
        if (strlen(pContext->mConfigPara.mFour2ndFilePath) > 0)
        {
            pStreamContext->mRecordHandler = pStreamContext->mVEncChn;
        }
#endif

        AW_MPI_VI_CreateVirChn(pStreamContext->mVipp, pStreamContext->mViChn, NULL);
        AW_MPI_VENC_CreateChn(pStreamContext->mVEncChn, &pStreamContext->mVEncChnAttr);
        AW_MPI_VENC_SetRcParam(pStreamContext->mVEncChn, &pStreamContext->mVEncRcParam);

#ifdef ENABLE_VENC_ADVANCED_PARAM
        /* set framerate in AW_MPI_VENC_CreateChn */
        //AW_MPI_VENC_SetFrameRate(pStreamContext->mVEncChn, &pStreamContext->mVEncFrameRateConfig);
        setVenc2Dnr(pStreamContext->mVEncChn);
        setVenc3Dnr(pStreamContext->mVEncChn);
        setVencSuperFrameCfg(pStreamContext->mVEncChn, pContext->mConfigPara.mFour2ndEncodeBitrate, pContext->mConfigPara.mFour2ndEncodeFrameRate);
#else
        alogd("VencChn[%d] use the default venc params for product mode %d", pStreamContext->mVEncChn, pStreamContext->mVEncChnAttr.RcAttr.mProductMode);
#endif

        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(pStreamContext->mVEncChn, &cbInfo);
        AW_MPI_VI_RegisterCallback(pStreamContext->mVipp, &cbInfo);

        if (pStreamContext->mIspAndVeLinkageEnable)
        {
            VENC_IspVeLinkAttr stIspVeLinkAttr;
            memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
            stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
            stIspVeLinkAttr.bEnableVe2Isp = FALSE; //4th camera sub stream disable Ve2isp
            stIspVeLinkAttr.nVipp = pStreamContext->mVipp;
            AW_MPI_VENC_EnableIspVeLink(pStreamContext->mVEncChn, &stIspVeLinkAttr);
            alogd("VencChn[%d] ispVeLink:%d-%d-%d", pStreamContext->mVEncChn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
                stIspVeLinkAttr.nVipp);
        }

        if (pStreamContext->mCameraAdaptiveMovingAndStaticEnable)
        {
            setVencLensMovingMaxQp(pStreamContext->mVEncChn, pContext->mConfigPara.mVencLensMovingMaxQp);
        }

        MPP_CHN_S ViChn = {MOD_ID_VIU, pStreamContext->mVipp, pStreamContext->mViChn};
        MPP_CHN_S VeChn = {MOD_ID_VENC, 0, pStreamContext->mVEncChn};
        AW_MPI_SYS_Bind(&ViChn, &VeChn);
        alogd("sub2ndStream vipp:%d viChn:%d veChn:%d", pStreamContext->mVipp, pStreamContext->mViChn, pStreamContext->mVEncChn);
    }

    return ret;
}

static int start(SampleSmartIPCDemoContext *pContext)
{
    int result = 0;

    if (pContext->mConfigPara.mMainEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mMainStream;

        AW_MPI_VI_EnableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);

        if (pContext->mConfigPara.mMainIspTestEnable) {
            if (pContext->mConfigPara.mMainIspTestIntervalMs == 0) {
                alogw("mIspTestIntervalMs is 0, it will set to 1000\n");
                pContext->mConfigPara.mMainIspTestIntervalMs = 1000;
            }
            memset(&pStreamContext->mIspTestCfg, 0, sizeof(IspApiTestCtrlConfig));
            pStreamContext->mIspTestCfg.mTetstIspChannelId = pStreamContext->mIsp;
            pStreamContext->mIspTestCfg.mTestIntervalMs = pContext->mConfigPara.mMainIspTestIntervalMs;
            pStreamContext->mIspTestCfg.mIspApiTask.mtaskSchedule = TEST_ISP_ALL;
            pStreamContext->mIspTestCfg.mTestAeCfg.mRes.Width = pStreamContext->mViAttr.format.width;
            pStreamContext->mIspTestCfg.mTestAeCfg.mRes.Height = pStreamContext->mViAttr.format.height;
            ispApiTestInit(&pStreamContext->mIspTestCfg);
        }

        if (pContext->mConfigPara.mMainDetectMipiDeskEnable) {
            memset(&pStreamContext->mDetMipiDeskCtrlCfg, 0, sizeof(DetMipiDeskCtrlConfig));
            pStreamContext->mDetMipiDeskCtrlCfg.mDetectIntervalMs = pContext->mConfigPara.mMainDetectIntervalMs;
            pStreamContext->mDetMipiDeskCtrlCfg.mdetMipiDeskEnable =pContext->mConfigPara.mMainDetectMipiDeskEnable;
            if (pContext->mConfigPara.mMainMipiChannel) {
                pStreamContext->mDetMipiDeskCtrlCfg.mMipiDeskCfg.mPayLoadAddr = MIPIB_PAYLOAD_REGS;
            } else {
                pStreamContext->mDetMipiDeskCtrlCfg.mMipiDeskCfg.mPayLoadAddr = MIPIA_PAYLOAD_REGS;
            }
            pStreamContext->mDetMipiDeskCtrlCfg.mMipiDeskCfg.mPhyDeskValueMax = MIPI_PHY_DESKEW_CLK_DLY_MAX;
            detectMipiDeskTestInit(&pStreamContext->mDetMipiDeskCtrlCfg);
        }

        getVencSpsPpsInfo(pStreamContext);
#ifdef ENABLE_VENC_ADVANCED_PARAM
        // RegionD3D must be set after get sps pps or start venc
        setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mMainEncodeWidth, pContext->mConfigPara.mMainEncodeHeight);
#endif
        AW_MPI_VENC_StartRecvPic(pStreamContext->mVEncChn);
        //setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mMainEncodeWidth, pContext->mConfigPara.mMainEncodeHeight);

        if (pContext->mConfigPara.mMainDrawOSDText)
        {
            DrawStreamOSD(pContext, pStreamContext->mVEncChn, pContext->mConfigPara.mMainDrawOSDText, 16, 32, 0);
        }

#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mMainRtspID >= 0)
        {
            RtspServerAttr rtsp_attr;
            memset(&rtsp_attr, 0, sizeof(RtspServerAttr));
            rtsp_attr.net_type = pContext->mConfigPara.mRtspNetType;
        
            if (PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H264;
            else if (PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H265;
            else
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_LAST;

            rtsp_attr.frame_rate = pContext->mConfigPara.mMainEncodeFrameRate;

            result = rtsp_open(pContext->mConfigPara.mMainRtspID, &rtsp_attr);
            if (result)
            {
                aloge("Do rtsp_open fail! ret:%d \n", result);
                return -1;
            }
        }
#endif
        result = pthread_create(&pStreamContext->mStreamThreadId, NULL, getVencStreamThread, (void*)pStreamContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
        if (pContext->mConfigPara.mMainLdciUseExtBufEnable)
        {
            gtm_ldci_common_start(pStreamContext->mIsp);
        }
    }
    if (pContext->mConfigPara.mMain2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mMain2ndStream;
        AW_MPI_VI_EnableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);

        getVencSpsPpsInfo(pStreamContext);
#ifdef ENABLE_VENC_ADVANCED_PARAM
        // RegionD3D must be set after get sps pps or start venc
        setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mMain2ndEncodeWidth, pContext->mConfigPara.mMain2ndEncodeHeight);
#endif
        AW_MPI_VENC_StartRecvPic(pStreamContext->mVEncChn);
        //setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mMain2ndEncodeWidth, pContext->mConfigPara.mMain2ndEncodeHeight);

        result = pthread_create(&pStreamContext->mStreamThreadId, NULL, getVencStreamThread, (void*)pStreamContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

    if (pContext->mConfigPara.mSubEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mSubStream;
        AW_MPI_VI_EnableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);

        getVencSpsPpsInfo(pStreamContext);
#ifdef ENABLE_VENC_ADVANCED_PARAM
        // RegionD3D must be set after get sps pps or start venc
        setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mSubEncodeWidth, pContext->mConfigPara.mSubEncodeHeight);
#endif
        AW_MPI_VENC_StartRecvPic(pStreamContext->mVEncChn);
        //setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mSubEncodeWidth, pContext->mConfigPara.mSubEncodeHeight);

        if (pContext->mConfigPara.mSubDrawOSDText)
        {
            DrawStreamOSD(pContext, pStreamContext->mVEncChn, pContext->mConfigPara.mSubDrawOSDText, 16, 32, 1);
        }

#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mSubRtspID >= 0)
        {
            RtspServerAttr rtsp_attr;
            memset(&rtsp_attr, 0, sizeof(RtspServerAttr));
            rtsp_attr.net_type = pContext->mConfigPara.mRtspNetType;
        
            if (PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H264;
            else if (PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H265;
            else
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_LAST;

            rtsp_attr.frame_rate = pContext->mConfigPara.mSubEncodeFrameRate;

            result = rtsp_open(pContext->mConfigPara.mSubRtspID, &rtsp_attr);
            if (result)
            {
                aloge("Do rtsp_open fail! ret:%d \n", result);
                return -1;
            }
        }
#endif
        result = pthread_create(&pStreamContext->mStreamThreadId, NULL, getVencStreamThread, (void*)pStreamContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
        if (pContext->mConfigPara.mSubLdciUseExtBufEnable)
        {
            gtm_ldci_common_start(pStreamContext->mIsp);
        }
    }
    if (pContext->mConfigPara.mSub2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mSub2ndStream;
        AW_MPI_VI_EnableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);

        getVencSpsPpsInfo(pStreamContext);
#ifdef ENABLE_VENC_ADVANCED_PARAM
        // RegionD3D must be set after get sps pps or start venc
        setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mSub2ndEncodeWidth, pContext->mConfigPara.mSub2ndEncodeHeight);
#endif
        AW_MPI_VENC_StartRecvPic(pStreamContext->mVEncChn);
        //setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mSub2ndEncodeWidth, pContext->mConfigPara.mSub2ndEncodeHeight);

        result = pthread_create(&pStreamContext->mStreamThreadId, NULL, getVencStreamThread, (void*)pStreamContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

    if (pContext->mConfigPara.mThreeEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mThreeStream;
        AW_MPI_VI_EnableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);

        getVencSpsPpsInfo(pStreamContext);
#ifdef ENABLE_VENC_ADVANCED_PARAM
        // RegionD3D must be set after get sps pps or start venc
        setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mThreeEncodeWidth, pContext->mConfigPara.mThreeEncodeHeight);
#endif
        AW_MPI_VENC_StartRecvPic(pStreamContext->mVEncChn);
        //setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mThreeEncodeWidth, pContext->mConfigPara.mThreeEncodeHeight);

        if (pContext->mConfigPara.mThreeDrawOSDText)
        {
            DrawStreamOSD(pContext, pStreamContext->mVEncChn, pContext->mConfigPara.mThreeDrawOSDText, 16, 32, 1);
        }

#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mThreeRtspID >= 0)
        {
            RtspServerAttr rtsp_attr;
            memset(&rtsp_attr, 0, sizeof(RtspServerAttr));
            rtsp_attr.net_type = pContext->mConfigPara.mRtspNetType;
        
            if (PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H264;
            else if (PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H265;
            else
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_LAST;

            rtsp_attr.frame_rate = pContext->mConfigPara.mThreeEncodeFrameRate;

            result = rtsp_open(pContext->mConfigPara.mThreeRtspID, &rtsp_attr);
            if (result)
            {
                aloge("Do rtsp_open fail! ret:%d \n", result);
                return -1;
            }
        }
#endif
        result = pthread_create(&pStreamContext->mStreamThreadId, NULL, getVencStreamThread, (void*)pStreamContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
        if (pContext->mConfigPara.mThreeLdciUseExtBufEnable)
        {
            gtm_ldci_common_start(pStreamContext->mIsp);
        }
    }
    if (pContext->mConfigPara.mThree2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mThree2ndStream;
        AW_MPI_VI_EnableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);

        getVencSpsPpsInfo(pStreamContext);
#ifdef ENABLE_VENC_ADVANCED_PARAM
        // RegionD3D must be set after get sps pps or start venc
        setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mThree2ndEncodeWidth, pContext->mConfigPara.mThree2ndEncodeHeight);
#endif
        AW_MPI_VENC_StartRecvPic(pStreamContext->mVEncChn);
        //setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mThree2ndEncodeWidth, pContext->mConfigPara.mThree2ndEncodeHeight);

        result = pthread_create(&pStreamContext->mStreamThreadId, NULL, getVencStreamThread, (void*)pStreamContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

    if (pContext->mConfigPara.mFourEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mFourStream;
        AW_MPI_VI_EnableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);

        getVencSpsPpsInfo(pStreamContext);
#ifdef ENABLE_VENC_ADVANCED_PARAM
        // RegionD3D must be set after get sps pps or start venc
        setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mFourEncodeWidth, pContext->mConfigPara.mFourEncodeHeight);
#endif
        AW_MPI_VENC_StartRecvPic(pStreamContext->mVEncChn);
        //setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mFourEncodeWidth, pContext->mConfigPara.mFourEncodeHeight);

        if (pContext->mConfigPara.mFourDrawOSDText)
        {
            DrawStreamOSD(pContext, pStreamContext->mVEncChn, pContext->mConfigPara.mFourDrawOSDText, 16, 32, 1);
        }

#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mFourRtspID >= 0)
        {
            RtspServerAttr rtsp_attr;
            memset(&rtsp_attr, 0, sizeof(RtspServerAttr));
            rtsp_attr.net_type = pContext->mConfigPara.mRtspNetType;
        
            if (PT_H264 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H264;
            else if (PT_H265 == pStreamContext->mVEncChnAttr.VeAttr.Type)
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_H265;
            else
                rtsp_attr.video_type = RTSP_VIDEO_TYPE_LAST;

            rtsp_attr.frame_rate = pContext->mConfigPara.mFourEncodeFrameRate;

            result = rtsp_open(pContext->mConfigPara.mFourRtspID, &rtsp_attr);
            if (result)
            {
                aloge("Do rtsp_open fail! ret:%d \n", result);
                return -1;
            }
        }
#endif
        result = pthread_create(&pStreamContext->mStreamThreadId, NULL, getVencStreamThread, (void*)pStreamContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
        if (pContext->mConfigPara.mFourLdciUseExtBufEnable)
        {
            gtm_ldci_common_start(pStreamContext->mIsp);
        }
    }
    if (pContext->mConfigPara.mFour2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mFour2ndStream;
        AW_MPI_VI_EnableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);

        getVencSpsPpsInfo(pStreamContext);
#ifdef ENABLE_VENC_ADVANCED_PARAM
        // RegionD3D must be set after get sps pps or start venc
        setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mFour2ndEncodeWidth, pContext->mConfigPara.mFour2ndEncodeHeight);
#endif
        AW_MPI_VENC_StartRecvPic(pStreamContext->mVEncChn);
        //setVencRegionD3D(pStreamContext->mVEncChn, pContext->mConfigPara.mFour2ndEncodeWidth, pContext->mConfigPara.mFour2ndEncodeHeight);

        result = pthread_create(&pStreamContext->mStreamThreadId, NULL, getVencStreamThread, (void*)pStreamContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

#ifdef SUPPORT_AWAIISP
    if (pContext->mConfigPara.mMainAiIspEnable || pContext->mConfigPara.mSubAiIspEnable ||
        pContext->mConfigPara.mThreeAiIspEnable || pContext->mConfigPara.mFourAiIspEnable)
    {
        if (pContext->mConfigPara.mMainAiIspAutoSwitchEnable || pContext->mConfigPara.mMainAiIspSwitchInterval ||
            pContext->mConfigPara.mSubAiIspAutoSwitchEnable || pContext->mConfigPara.mSubAiIspSwitchInterval ||
            pContext->mConfigPara.mThreeAiIspAutoSwitchEnable || pContext->mConfigPara.mThreeAiIspSwitchInterval ||
            pContext->mConfigPara.mFourAiIspAutoSwitchEnable || pContext->mConfigPara.mFourAiIspSwitchInterval)
        {
            result = pthread_create(&pContext->mAiispThreadId, NULL, aiispSwitchThread, (void*)pContext);
            if (result != 0)
            {
                aloge("fatal error! pthread create fail[%d]", result);
            }
        }
    }
#endif

    return result;
}

static int stop(SampleSmartIPCDemoContext *pContext)
{
    int ret = 0;
    void *pRetVal = NULL;

    if (pContext->mConfigPara.mMainEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mMainStream;
        pthread_join(pStreamContext->mStreamThreadId, &pRetVal);
        alogd("mainStream pRetVal=%p", pRetVal);
#ifdef SUPPORT_AWAIISP
        if (pContext->mConfigPara.mMainAiIspEnable)
        {
            awaiisp_common_disable(pStreamContext->mIsp);
        }
        else
#endif
        {
            if (-1 != pContext->mConfigPara.mMainIspTdmRawProcessType)
            {
                tdm_raw_process_stop(pStreamContext->mIsp);
                tdm_raw_process_close(pStreamContext->mIsp);
            }
        }

        AW_MPI_VI_DisableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        AW_MPI_VENC_StopRecvPic(pStreamContext->mVEncChn);
        AW_MPI_VENC_ResetChn(pStreamContext->mVEncChn);
        AW_MPI_VENC_DestroyChn(pStreamContext->mVEncChn);
        AW_MPI_VI_DestroyVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        if(pStreamContext->mFile)
        {
            fclose(pStreamContext->mFile);
            pStreamContext->mFile = NULL;
        }
        if (pContext->mConfigPara.mMainDrawOSDText)
        {
            DestroyStreamOSD(pContext, pStreamContext->mVEncChn, 0);
        }
        if (pContext->mConfigPara.mMainLdciUseExtBufEnable)
        {
            gtm_ldci_common_stop(pStreamContext->mIsp);
        }
    }
    if (pContext->mConfigPara.mMain2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mMain2ndStream;
        pthread_join(pStreamContext->mStreamThreadId, &pRetVal);
        alogd("main2ndStream pRetVal=%p", pRetVal);
        AW_MPI_VI_DisableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        AW_MPI_VENC_StopRecvPic(pStreamContext->mVEncChn);
        AW_MPI_VENC_ResetChn(pStreamContext->mVEncChn);
        AW_MPI_VENC_DestroyChn(pStreamContext->mVEncChn);
        AW_MPI_VI_DestroyVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        if(pStreamContext->mFile)
        {
            fclose(pStreamContext->mFile);
            pStreamContext->mFile = NULL;
        }
    }
    if (pContext->mConfigPara.mSubEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mSubStream;
        pthread_join(pStreamContext->mStreamThreadId, &pRetVal);
        alogd("subStream pRetVal=%p", pRetVal);
#ifdef SUPPORT_AWAIISP
        if (pContext->mConfigPara.mSubAiIspEnable)
        {
            awaiisp_common_disable(pStreamContext->mIsp);
        }
        else
#endif
        {
            if (-1 != pContext->mConfigPara.mSubIspTdmRawProcessType)
            {
                tdm_raw_process_stop(pStreamContext->mIsp);
                tdm_raw_process_close(pStreamContext->mIsp);
            }
        }

        AW_MPI_VI_DisableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        AW_MPI_VENC_StopRecvPic(pStreamContext->mVEncChn);
        AW_MPI_VENC_ResetChn(pStreamContext->mVEncChn);
        AW_MPI_VENC_DestroyChn(pStreamContext->mVEncChn);
        AW_MPI_VI_DestroyVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        if(pStreamContext->mFile)
        {
            fclose(pStreamContext->mFile);
            pStreamContext->mFile = NULL;
        }
        if (pContext->mConfigPara.mSubDrawOSDText)
        {
            DestroyStreamOSD(pContext, pStreamContext->mVEncChn, 1);
        }
        if (pContext->mConfigPara.mSubLdciUseExtBufEnable)
        {
            gtm_ldci_common_stop(pStreamContext->mIsp);
        }
    }
    if (pContext->mConfigPara.mSub2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mSub2ndStream;
        pthread_join(pStreamContext->mStreamThreadId, &pRetVal);
        alogd("sub2ndStream pRetVal=%p", pRetVal);
        AW_MPI_VI_DisableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        AW_MPI_VENC_StopRecvPic(pStreamContext->mVEncChn);
        AW_MPI_VENC_ResetChn(pStreamContext->mVEncChn);
        AW_MPI_VENC_DestroyChn(pStreamContext->mVEncChn);
        AW_MPI_VI_DestroyVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        if(pStreamContext->mFile)
        {
            fclose(pStreamContext->mFile);
            pStreamContext->mFile = NULL;
        }
    }
    if (pContext->mConfigPara.mThreeEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mThreeStream;
        pthread_join(pStreamContext->mStreamThreadId, &pRetVal);
        alogd("subStream pRetVal=%p", pRetVal);
#ifdef SUPPORT_AWAIISP
        if (pContext->mConfigPara.mThreeAiIspEnable)
        {
            awaiisp_common_disable(pStreamContext->mIsp);
        }
        else
#endif
        {
            if (-1 != pContext->mConfigPara.mThreeIspTdmRawProcessType)
            {
                tdm_raw_process_stop(pStreamContext->mIsp);
                tdm_raw_process_close(pStreamContext->mIsp);
            }
        }

        AW_MPI_VI_DisableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        AW_MPI_VENC_StopRecvPic(pStreamContext->mVEncChn);
        AW_MPI_VENC_ResetChn(pStreamContext->mVEncChn);
        AW_MPI_VENC_DestroyChn(pStreamContext->mVEncChn);
        AW_MPI_VI_DestroyVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        if(pStreamContext->mFile)
        {
            fclose(pStreamContext->mFile);
            pStreamContext->mFile = NULL;
        }
        if (pContext->mConfigPara.mThreeDrawOSDText)
        {
            DestroyStreamOSD(pContext, pStreamContext->mVEncChn, 1);
        }
        if (pContext->mConfigPara.mThreeLdciUseExtBufEnable)
        {
            gtm_ldci_common_stop(pStreamContext->mIsp);
        }
    }
    if (pContext->mConfigPara.mThree2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mThree2ndStream;
        pthread_join(pStreamContext->mStreamThreadId, &pRetVal);
        alogd("sub2ndStream pRetVal=%p", pRetVal);
        AW_MPI_VI_DisableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        AW_MPI_VENC_StopRecvPic(pStreamContext->mVEncChn);
        AW_MPI_VENC_ResetChn(pStreamContext->mVEncChn);
        AW_MPI_VENC_DestroyChn(pStreamContext->mVEncChn);
        AW_MPI_VI_DestroyVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        if(pStreamContext->mFile)
        {
            fclose(pStreamContext->mFile);
            pStreamContext->mFile = NULL;
        }
    }
    if (pContext->mConfigPara.mFourEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mFourStream;
        pthread_join(pStreamContext->mStreamThreadId, &pRetVal);
        alogd("subStream pRetVal=%p", pRetVal);
#ifdef SUPPORT_AWAIISP
        if (pContext->mConfigPara.mFourAiIspEnable)
        {
            awaiisp_common_disable(pStreamContext->mIsp);
        }
        else
#endif
        {
            if (-1 != pContext->mConfigPara.mFourIspTdmRawProcessType)
            {
                tdm_raw_process_stop(pStreamContext->mIsp);
                tdm_raw_process_close(pStreamContext->mIsp);
            }
        }

        AW_MPI_VI_DisableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        AW_MPI_VENC_StopRecvPic(pStreamContext->mVEncChn);
        AW_MPI_VENC_ResetChn(pStreamContext->mVEncChn);
        AW_MPI_VENC_DestroyChn(pStreamContext->mVEncChn);
        AW_MPI_VI_DestroyVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        if(pStreamContext->mFile)
        {
            fclose(pStreamContext->mFile);
            pStreamContext->mFile = NULL;
        }
        if (pContext->mConfigPara.mFourDrawOSDText)
        {
            DestroyStreamOSD(pContext, pStreamContext->mVEncChn, 1);
        }
        if (pContext->mConfigPara.mFourLdciUseExtBufEnable)
        {
            gtm_ldci_common_stop(pStreamContext->mIsp);
        }
    }
    if (pContext->mConfigPara.mFour2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mFour2ndStream;
        pthread_join(pStreamContext->mStreamThreadId, &pRetVal);
        alogd("sub2ndStream pRetVal=%p", pRetVal);
        AW_MPI_VI_DisableVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        AW_MPI_VENC_StopRecvPic(pStreamContext->mVEncChn);
        AW_MPI_VENC_ResetChn(pStreamContext->mVEncChn);
        AW_MPI_VENC_DestroyChn(pStreamContext->mVEncChn);
        AW_MPI_VI_DestroyVirChn(pStreamContext->mVipp, pStreamContext->mViChn);
        if(pStreamContext->mFile)
        {
            fclose(pStreamContext->mFile);
            pStreamContext->mFile = NULL;
        }
    }

#ifdef SUPPORT_AWAIISP
    if (pContext->mConfigPara.mMainAiIspEnable || pContext->mConfigPara.mSubAiIspEnable ||
        pContext->mConfigPara.mThreeAiIspEnable || pContext->mConfigPara.mFourAiIspEnable)
    {
        if (pContext->mAiispThreadId)
            pthread_join(pContext->mAiispThreadId, &pRetVal);
    }
#endif

    return ret;
}

static int destroy(SampleSmartIPCDemoContext *pContext)
{
    int ret = 0;

    if (pContext->mConfigPara.mMainEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mMainStream;

        if (pContext->mConfigPara.mMainIspTestEnable) {
            ispApiTestExit(&pStreamContext->mIspTestCfg);
        }

        if (pContext->mConfigPara.mMainDetectMipiDeskEnable) {
            detectMipiDeskTestExit(&pStreamContext->mDetMipiDeskCtrlCfg);
        }

        AW_MPI_VI_DisableVipp(pStreamContext->mVipp);
        AW_MPI_ISP_Stop(pStreamContext->mIsp);
        AW_MPI_VI_DestroyVipp(pStreamContext->mVipp);
        if (pContext->mConfigPara.mMainLdciUseExtBufEnable)
        {
            gtm_ldci_common_close(pStreamContext->mIsp);
        }
#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mMainRtspID >= 0)
        {
            rtsp_stop(pContext->mConfigPara.mMainRtspID);
            rtsp_close(pContext->mConfigPara.mMainRtspID);
        }
#endif
    }
    if (pContext->mConfigPara.mMain2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mMain2ndStream;
        AW_MPI_VI_DisableVipp(pStreamContext->mVipp);
        AW_MPI_ISP_Stop(pStreamContext->mIsp);
        AW_MPI_VI_DestroyVipp(pStreamContext->mVipp);
    }
    if (pContext->mConfigPara.mSubEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mSubStream;
        AW_MPI_VI_DisableVipp(pStreamContext->mVipp);
        AW_MPI_ISP_Stop(pStreamContext->mIsp);
        AW_MPI_VI_DestroyVipp(pStreamContext->mVipp);
        if (pContext->mConfigPara.mSubLdciUseExtBufEnable)
        {
            gtm_ldci_common_close(pStreamContext->mIsp);
        }
#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mSubRtspID >= 0)
        {
            rtsp_stop(pContext->mConfigPara.mSubRtspID);
            rtsp_close(pContext->mConfigPara.mSubRtspID);
        }
#endif
    }
    if (pContext->mConfigPara.mSub2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mSub2ndStream;
        AW_MPI_VI_DisableVipp(pStreamContext->mVipp);
        AW_MPI_ISP_Stop(pStreamContext->mIsp);
        AW_MPI_VI_DestroyVipp(pStreamContext->mVipp);
    }
    if (pContext->mConfigPara.mThreeEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mThreeStream;
        AW_MPI_VI_DisableVipp(pStreamContext->mVipp);
        AW_MPI_ISP_Stop(pStreamContext->mIsp);
        AW_MPI_VI_DestroyVipp(pStreamContext->mVipp);
        if (pContext->mConfigPara.mThreeLdciUseExtBufEnable)
        {
            gtm_ldci_common_close(pStreamContext->mIsp);
        }
#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mThreeRtspID >= 0)
        {
            rtsp_stop(pContext->mConfigPara.mThreeRtspID);
            rtsp_close(pContext->mConfigPara.mThreeRtspID);
        }
#endif
    }
    if (pContext->mConfigPara.mThree2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mThree2ndStream;
        AW_MPI_VI_DisableVipp(pStreamContext->mVipp);
        AW_MPI_ISP_Stop(pStreamContext->mIsp);
        AW_MPI_VI_DestroyVipp(pStreamContext->mVipp);
    }
    if (pContext->mConfigPara.mFourEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mFourStream;
        AW_MPI_VI_DisableVipp(pStreamContext->mVipp);
        AW_MPI_ISP_Stop(pStreamContext->mIsp);
        AW_MPI_VI_DestroyVipp(pStreamContext->mVipp);
        if (pContext->mConfigPara.mFourLdciUseExtBufEnable)
        {
            gtm_ldci_common_close(pStreamContext->mIsp);
        }
#ifdef SUPPORT_RTSP_TEST
        if (pContext->mConfigPara.mFourRtspID >= 0)
        {
            rtsp_stop(pContext->mConfigPara.mFourRtspID);
            rtsp_close(pContext->mConfigPara.mFourRtspID);
        }
#endif
    }
    if (pContext->mConfigPara.mFour2ndEnable)
    {
        VencStreamContext *pStreamContext = &pContext->mFour2ndStream;
        AW_MPI_VI_DisableVipp(pStreamContext->mVipp);
        AW_MPI_ISP_Stop(pStreamContext->mIsp);
        AW_MPI_VI_DestroyVipp(pStreamContext->mVipp);
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int result = 0;
    ERRORTYPE ret;

    SampleSmartIPCDemoContext *pContext = (SampleSmartIPCDemoContext*)malloc(sizeof(SampleSmartIPCDemoContext));
    if (NULL == pContext)
    {
        aloge("fatal error! malloc pContext failed! size=%d", sizeof(SampleSmartIPCDemoContext));
        return -1;
    }
    gpSampleSmartIPCDemoContext = pContext;
    memset(pContext, 0, sizeof(SampleSmartIPCDemoContext));
    cdx_sem_init(&pContext->mSemExit, 0);

    /* register process function for SIGINT, to exit program. */
    if (signal(SIGINT, handle_exit) == SIG_ERR)
    {
        aloge("can't catch SIGSEGV");
    }

    if (message_create(&pContext->mMsgQueue) < 0)
    {
        aloge("message create fail!");
        goto _exit;
    }

    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        aloge("fatal error! command line param is wrong, exit!");
        result = -1;
        goto _exit;
    }
    char *pConfigFilePath;
    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }
    /* parse config file. */
    if(loadSampleConfig(&pContext->mConfigPara, pConfigFilePath) != SUCCESS)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _exit;
    }
    system("cat /proc/meminfo | grep Committed_AS");

    if (pContext->mConfigPara.mAiIspNpuRefBufReduceEnable)
    {
        awaiisp_common_set_ulimit_fd(4096);
    }

    // prepare
    MPP_SYS_CONF_S stSysConf;
    memset(&stSysConf, 0, sizeof(MPP_SYS_CONF_S));
    stSysConf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&stSysConf);
    ret = AW_MPI_SYS_Init();
    if (ret < 0)
    {
        aloge("fatal error! sys Init failed! ret=%d", ret);
        goto _exit;
    }

#ifdef SUPPORT_SAVE_STREAM
    record_init();
    pContext->mMainStream.mRecordHandler = -1;
    pContext->mMain2ndStream.mRecordHandler = -1;
    pContext->mSubStream.mRecordHandler = -1;
    pContext->mSub2ndStream.mRecordHandler = -1;
    pContext->mThreeStream.mRecordHandler = -1;
    pContext->mThree2ndStream.mRecordHandler = -1;
    pContext->mFourStream.mRecordHandler = -1;
    pContext->mFour2ndStream.mRecordHandler = -1;
#endif

    pContext->mOverlayDrawStreamOSDBase = 100;

    prepare(pContext);

    //create msg queue thread
    result = pthread_create(&pContext->mMsgQueueThreadId, NULL, MsgQueueThread, pContext);
    if (result != 0)
    {
        aloge("fatal error! pthread create fail[%d]", result);
    }

    // start
    start(pContext);

#ifdef SUPPORT_AI_SERVICE
    // ai service
    if (pContext->mConfigPara.mMainNnEnable)
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VI_RegisterCallback(pContext->mConfigPara.mMainNnVipp, &cbInfo);
    }
    if (pContext->mConfigPara.mSubNnEnable)
    {
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)pContext;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        if (pContext->mConfigPara.mMainNnVipp != pContext->mConfigPara.mSubNnVipp)
        {
            AW_MPI_VI_RegisterCallback(pContext->mConfigPara.mSubNnVipp, &cbInfo);
        }
    }
    if (pContext->mConfigPara.mMainNnEnable || pContext->mConfigPara.mSubNnEnable)
    {
        ai_service_attr_t aiservice_attr;
        memset(&aiservice_attr, 0, sizeof(ai_service_attr_t));
        configAiService(&pContext->mConfigPara, &aiservice_attr);
        ai_service_start(&aiservice_attr);
    }
#endif

    // wb yuv
    if (pContext->mConfigPara.mWbYuvEnable)
    {
        result = pthread_create(&pContext->mWbYuvThreadId, NULL, getWbYuvThread, (void*)pContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

    // test trigger code
    if (pContext->mConfigPara.mTestTriggerViTimeout)
    {
        result = pthread_create(&pContext->mTestTriggerThreadId, NULL, testTriggerThread, (void*)pContext);
        if (result != 0)
        {
            aloge("fatal error! pthread create fail[%d]", result);
        }
    }

    // wait for test end
    if (pContext->mConfigPara.mTestDuration > 0)
    {
        cdx_sem_down_timedwait(&pContext->mSemExit, pContext->mConfigPara.mTestDuration*1000);
    }
    else
    {
        cdx_sem_down(&pContext->mSemExit);
    }

#ifdef SUPPORT_AI_SERVICE
    if (pContext->mConfigPara.mMainNnEnable || pContext->mConfigPara.mSubNnEnable)
    {
        ai_service_stop();
    }
#endif

    pContext->mbExitFlag = 1;

    message_t stMsgCmd;
    stMsgCmd.command = MsgQueue_Stop;
    put_message(&pContext->mMsgQueue, &stMsgCmd);
    pthread_join(pContext->mMsgQueueThreadId, NULL);

    void *pRetVal = NULL;

    if (pContext->mConfigPara.mTestTriggerViTimeout)
    {
        pthread_join(pContext->mTestTriggerThreadId, &pRetVal);
        alogd("test pRetVal=%p", pRetVal);
    }

    if (pContext->mConfigPara.mWbYuvEnable)
    {
        pthread_join(pContext->mWbYuvThreadId, &pRetVal);
        alogd("WbYuv pRetVal=%p", pRetVal);
    }

    // stop
    stop(pContext);

    // deinit
    destroy(pContext);

#ifdef SUPPORT_SAVE_STREAM
    record_exit();
#endif

    ret = AW_MPI_SYS_Exit();
    if (ret != SUCCESS)
    {
        aloge("fatal error! sys exit failed!");
    }

_exit:
    cdx_sem_deinit(&pContext->mSemExit);
    message_destroy(&pContext->mMsgQueue);
    if(pContext!=NULL)
    {
        free(pContext);
        pContext = NULL;
    }
    gpSampleSmartIPCDemoContext = NULL;
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
