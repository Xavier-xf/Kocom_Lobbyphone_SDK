/******************************************************************************
  Copyright (C), 2020-2030, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     :
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2024/1/22
  Last Modified :
  Description   :
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "sample_muxer_multi_stream"
#include "plat_log.h"

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <confparser.h>
#include <mpi_videoformat_conversion.h>
#include <mpi_sys.h>
#include <mpi_vi.h>
#include <mpi_venc.h>
#include <mpi_mux.h>
#include <mpi_isp.h>
#include <vo/hwdisplay.h>
#include <mpi_vo.h>
#include <cdx_list.h>

#include "../common/sample_common_venc.h"
#include "sample_muxer_multi_stream.h"
#include "sample_muxer_multi_stream_config.h"

#define FILE_EXIST(PATH)   (access(PATH, F_OK) == 0)
#define DEFAULT_SIMPLE_CACHE_SIZE_VFS       (64*1024)

static struct sample_muxer_multi_stream_context *g_context = NULL;
//static int gMuxerIdCounter = 0;
static void handle_exit()
{
    alogd("user want to exit!");
    if(g_context)
    {
        cdx_sem_up(&g_context->sem_exit);
    }
}

static int ParseCmdLine(struct sample_muxer_multi_stream_context *context, int argc, char **argv)
{
    alogd("sample_multi_vi2venc2muxer:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
    int i=1;
    memset(&context->cmdline, 0, sizeof(context->cmdline));
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
            else
            {
                strcpy(context->cmdline.config_file, argv[i]);
            }
        }
        else if(!strcmp(argv[i], "-h"))
        {
            alogd("CmdLine param:\n"
                "\t-path /mnt/extsd/sample_multi_vi2venc2muxer.conf");
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

static void judgeCaptureFormat(char *pFormatConf, PIXEL_FORMAT_E *pCapFromat)
{
    if (!strcmp(pFormatConf, "nv21"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    }
    else if (!strcmp(pFormatConf, "yv12"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YVU_PLANAR_420;
    }
    else if (!strcmp(pFormatConf, "nv12"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    }
    else if (!strcmp(pFormatConf, "yu12"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_PLANAR_420;
    }
    else if (!strcmp(pFormatConf, "aw_afbc"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_AFBC;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_2_0x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_2_5x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_1_5x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
    }
    else if (!strcmp(pFormatConf, "aw_lbc_1_0x"))
    {
        *pCapFromat = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
    }
    else
    {
        *pCapFromat = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        aloge("fatal error! wrong src pixfmt:%s use default nv21", pFormatConf);
    }
}

static void judgeEncoderType(char *pFormatConf, PAYLOAD_TYPE_E *pEncoderType)
{
    if (!strcmp(pFormatConf, "H.264"))
    {
        *pEncoderType = PT_H264;
    }
    else if (!strcmp(pFormatConf, "H.265"))
    {
        *pEncoderType = PT_H265;
    }
    else if (!strcmp(pFormatConf, "MJPEG"))
    {
        *pEncoderType = PT_MJPEG;
    }
    else
    {
        aloge("fatal error! unsupport encoder type[%s] use default H.264", pFormatConf);
        *pEncoderType = PT_H264;
    }
}

static char *parserKeyCfg(char *pKeyCfg, int nKey)
{
    static char keyCfg[MAX_FILE_PATH_SIZE] = {0};

    if (pKeyCfg)
    {
        sprintf(keyCfg, "stream_%d_%s", nKey, pKeyCfg);
        return keyCfg;
    }

    aloge("fatal error! key cfg is null key num[%d]", nKey);
    return NULL;
}

static int loadSampleRecordConfig(struct sample_muxer_multi_stream_context *context, const char *conf_path)
{
    int ret = 0;
    struct sample_muxer_multi_stream_config *config;
    char *pTmp = NULL;

    if(conf_path != NULL)
    {
        CONFPARSER_S stConf;
        ret = createConfParser(conf_path, &stConf);
        if (ret < 0)
        {
            aloge("load conf fail");
            return ret;
        }

        context->video_file_max_cnt = GetConfParaInt(&stConf, SAMPLE_MUXER_MULTI_STREAM_KEY_VIDEO_FILE_MAX_CNT, 0);
        context->video_file_duration = GetConfParaInt(&stConf, SAMPLE_MUXER_MULTI_STREAM_KEY_VIDEO_FILE_MAX_DURATION, 0);
        context->test_duration = GetConfParaInt(&stConf, SAMPLE_MUXER_MULTI_STREAM_KEY_TEST_DURATION, 0);
        pTmp = (char *)GetConfParaString(&stConf, SAMPLE_MUXER_MULTI_STREAM_KEY_VIDEO_DST_FILE, NULL);
        if (pTmp)
            strncpy(context->video_dst_file, pTmp, strlen(pTmp));
        context->stream_num = GetConfParaInt(&stConf, SAMPLE_MUXER_MULTI_STREAM_KEY_STREAM_NUM, 0);
        if (context->stream_num > MAX_STREAM_NUM)
            context->stream_num = MAX_STREAM_NUM;

        for(int i = 0; i < context->stream_num; i++)
        {
            config = &context->stream[i].stream_config;

            config->vi_dev = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_VI_DEV, i), 0);
            config->isp_dev = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ISP_DEV, i), 0);
            config->cap_width = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_CAP_WIDTH, i), 0);
            config->cap_height = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_CAP_HEIGHT, i), 0);
            config->cap_framerate = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_CAP_FRAMERATE, i), 0);
            pTmp = (char *)GetConfParaString(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_CAP_FORMAT, i), NULL);
            judgeCaptureFormat(pTmp, &config->cap_format);
            config->vi_bufnum = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_VI_BUFNUM, i), 0);
            config->enable_wdr = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENABLE_WDR, i), 0);
            config->enc_online_enable = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_ONLINE, i), 0);
            config->enc_online_share_bufnum = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_ONLINE_SHARE_BUFNUM, i), 0);
            pTmp = (char *)GetConfParaString(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_TYPE, i), NULL);
            judgeEncoderType(pTmp, &config->enc_type);
            config->enc_width = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_WIDTH, i), 0);
            config->enc_height = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_HEIGHT, i), 0);
            config->enc_framerate = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_FRAMERATE, i), 0);
            config->enc_bitrate = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_BITRATE, i), 0);
            config->enc_rcmode = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_RCMODE, i), 0);
            config->encpp_enable = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENCPP_ENABLE, i), 0);
            config->enc_ve_ref_frame_lbc = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_VE_REF_FRAME_LBC_MODE, i), 0);
            config->enc_key_framerate = GetConfParaInt(&stConf, parserKeyCfg(SAMPLE_MUXER_MULTI_STREAM_KEY_ENC_KEY_FRAME_INTERVAL, i), 0);
        }

        destroyConfParser(&stConf);
    }

    for (int i = 0; i < context->stream_num; i++)
    {
        config = &context->stream[i].stream_config;
        alogd("stream[%d] vi dev[%d] capture size[%d-%d] format[%d] rate[%d] vi buf num[%d] wdr[%d]",
            i, config->vi_dev, config->cap_width, config->cap_height, config->cap_format,
            config->cap_framerate, config->vi_bufnum, config->enable_wdr);
        alogd("stream[%d] encodr type[%d] size[%d-%d] frmRate[%d] bitRate[%d] rcMode[%d]",
            i, config->enc_type, config->enc_width, config->enc_height,
            config->enc_framerate, config->enc_bitrate, config->enc_rcmode);
    }
    alogd("test duration %ds, video file max cnt %d, video dst file %s, max duration %d",
        context->test_duration, context->video_file_max_cnt, context->video_dst_file, context->video_file_duration);

    return SUCCESS;
}

static int generate_file_name_by_id(char *pNameBuf, struct sample_muxer_multi_stream_context *context, int id)
{
    char tmpBuf[MAX_FILE_PATH_SIZE] = {0};
    char *pFileName;
    char *ptr =     ptr = strrchr(context->video_dst_file, '.');
    if(ptr != NULL)
    {
        strncpy(tmpBuf, context->video_dst_file, ptr-context->video_dst_file);
        sprintf(pNameBuf, "%s_%d%s", tmpBuf, id, ptr);
    }
    else
    {
        sprintf(pNameBuf, "%s_%d", context->video_dst_file, id);
    }
    return 0;
}

static int setNextFileToMuxer(struct sample_muxer_multi_stream_context *context, char* path, int64_t fallocateLength, int muxChn)
{
    int result = 0;
    ERRORTYPE ret;
    if(path != NULL)
    {
        int fd = open(path, O_RDWR | O_CREAT, 0666);
        if (fd < 0)
        {
            aloge("fatal error! fail to open %s", path);
            return -1;
        }

        if (context->mux_chn == muxChn)
        {
            ret = AW_MPI_MUX_SwitchFd(context->mux_chn, fd, (int)fallocateLength);
            if(ret != SUCCESS)
            {
                aloge("fatal error! muxChn[%d] switch fd[%d] fail[0x%x]!", context->mux_chn, fd, ret);
                result = -1;
            }
        }
        else
        {
            aloge("fatal error! muxChn is not match:[0x%x!=0x%x]", context->mux_chn, muxChn);
            result = -1;
        }

        close(fd);

        return result;
    }
    else
    {
        return -1;
    }
}

static ERRORTYPE MPPCallbackWrapper(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    int ret;
    ERRORTYPE eRet = SUCCESS;

    if (MOD_ID_VENC == pChn->mModId)
    {
        struct sample_muxer_multi_stream_stream *stream = (struct sample_muxer_multi_stream_stream *)cookie;
        VENC_CHN mVEncChn = pChn->mChnId;
        switch(event)
        {
            case MPP_EVENT_RELEASE_VIDEO_BUFFER:
            {
                break;
            }
            case MPP_EVENT_VENC_TIMEOUT:
            {
                uint64_t framePts = *(uint64_t*)pEventData;
                alogw("Be careful! detect encode timeout, pts[%lld]us", framePts);
                break;
            }
            case MPP_EVENT_VENC_BUFFER_FULL:
            {
                alogw("Be careful! detect venc buffer full");
                break;
            }
            /*case MPP_EVENT_LINKAGE_ISP2VE_PARAM:
            {
                Isp2VeLinkageParam stIsp2Ve;
                memset(&stIsp2Ve, 0, sizeof(Isp2VeLinkageParam));
                stIsp2Ve.mIspAndVeLinkageEnable = stream->stream_config.isp_ve_linkage;
                stIsp2Ve.mCameraAdaptiveMovingAndStaticEnable = 0;
                stIsp2Ve.mVEncChn = mVEncChn;
                stIsp2Ve.mVipp = stream->vi_dev;
                stIsp2Ve.pIsp2VeParam = (VencIsp2VeParam *)pEventData;
                stIsp2Ve.nEncppSharpAttenCoefPer = stream->stream_config.encpp_enable;
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
                stVe2Isp.mIspAndVeLinkageEnable = stream->stream_config.isp_ve_linkage;
                stVe2Isp.mVEncChn = mVEncChn;
                stVe2Isp.mVipp = stream->vi_dev;
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
                pExtraParam->eEnCameraMove = CAMERA_ADAPTIVE_STATIC;
                break;
            }
            default:
            {
                alogv("fatal error! unknown event[%d]", event);
                break;
            }
        }
    }
    else if(MOD_ID_MUX == pChn->mModId)
    {
        struct sample_muxer_multi_stream_context *context = (struct sample_muxer_multi_stream_context *)cookie;
        switch(event)
        {
            case MPP_EVENT_RECORD_DONE:
            {
                message_t stCmdMsg;
                InitMessage(&stCmdMsg);
                stCmdMsg.command = MPP_EVENT_RECORD_DONE;
                stCmdMsg.para0 = *(int*)pEventData;
                putMessageWithData(&context->msg_queue, &stCmdMsg);
                break;
            }
            case MPP_EVENT_NEED_NEXT_FD:
            {
                message_t stMsgCmd;
                InitMessage(&stMsgCmd);
                stMsgCmd.command = MPP_EVENT_NEED_NEXT_FD;
                stMsgCmd.para0 = *(int *)pEventData;
                putMessageWithData(&context->msg_queue, &stMsgCmd);
                break;
            }
            case MPP_EVENT_BSFRAME_AVAILABLE:
            {
                alogd("mux bs frame available");
                break;
            }
            default:
            {
                aloge("fatal error! unknown event[0x%x]", event);
                break;
            }
        }
    }
    else
    {
         aloge("fatal error! unknown chn[%d,%d,%d]", pChn->mModId, pChn->mDevId, pChn->mChnId);
    }

    return eRet;
}


static void configViAttr(struct sample_muxer_multi_stream_config *config, VI_ATTR_S *vi_attr)
{
    if (config->enc_online_enable)
    {
        vi_attr->mOnlineEnable = 1;
        vi_attr->mOnlineShareBufNum = config->enc_online_share_bufnum;
    }
    vi_attr->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    vi_attr->memtype = V4L2_MEMORY_MMAP;
    vi_attr->format.pixelformat = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(config->cap_format);
    vi_attr->format.field = V4L2_FIELD_NONE;
    vi_attr->format.colorspace = V4L2_COLORSPACE_JPEG;
    vi_attr->format.width = config->cap_width;
    vi_attr->format.height = config->cap_height;
    vi_attr->nbufs = config->vi_bufnum;
    vi_attr->nplanes = 2;
    vi_attr->fps = config->cap_framerate;
    vi_attr->use_current_win = 0;
    vi_attr->wdr_mode = config->enable_wdr;
    vi_attr->capturemode = V4L2_MODE_VIDEO;
    vi_attr->drop_frame_num = 0;
    vi_attr->mbEncppEnable = TRUE;
}

static int createVipp(struct sample_muxer_multi_stream_stream *stream)
{
    ERRORTYPE eRet = SUCCESS;
    struct sample_muxer_multi_stream_config *config = &stream->stream_config;

    stream->vi_dev = config->vi_dev;
    eRet = AW_MPI_VI_CreateVipp(stream->vi_dev);
    if (SUCCESS != eRet)
    {
        stream->vi_dev = MM_INVALID_DEV;
        aloge("fatal error! vi dev[%d] create fail!", stream->vi_dev);
        return -1;
    }

    memset(&stream->vi_attr, 0, sizeof(VI_ATTR_S));
    AW_MPI_VI_GetVippAttr(stream->vi_dev, &stream->vi_attr);
    configViAttr(config, &stream->vi_attr);
    eRet = AW_MPI_VI_SetVippAttr(stream->vi_dev, &stream->vi_attr);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! vi dev[%d] set attr fail!", stream->vi_dev);
        return -1;
    }

    stream->isp_dev = config->isp_dev;
    if (stream->isp_dev >= 0)
    {
        eRet = AW_MPI_ISP_Run(stream->isp_dev);
        if (SUCCESS != eRet)
        {
            aloge("fatal error! ISP[%d] init fail!", stream->isp_dev);
            return -1;
        }
    }

    eRet = AW_MPI_VI_EnableVipp(stream->vi_dev);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! vi dev[%d] enable fail!", stream->vi_dev);
        return -1;
    }

    stream->vi_chn = 0;
    eRet = AW_MPI_VI_CreateVirChn(stream->vi_dev, stream->vi_chn, NULL);
    if (SUCCESS != eRet)
    {
        aloge("fatal error! vi chn[%d] create fail!", stream->vi_chn);
        return -1;
    }

    alogd("create vi dev[%d] vi chn[%d] success!", stream->vi_dev, stream->vi_chn);
    return 0;
}

static int configVencChnAttr(struct sample_muxer_multi_stream_config *config, VENC_CHN_ATTR_S *ve_chn_attr, VENC_RC_PARAM_S *ve_rc_param)
{
    ve_chn_attr->VeAttr.Type = config->enc_type;
    if (config->enc_online_enable)
    {
        ve_chn_attr->VeAttr.mOnlineEnable = config->enc_online_enable;
        ve_chn_attr->VeAttr.mOnlineShareBufNum = config->enc_online_share_bufnum;
    }
    switch(ve_chn_attr->VeAttr.Type)
    {
        case PT_H264:
        {
            ve_chn_attr->VeAttr.AttrH264e.mThreshSize = AWALIGN((config->enc_width*config->enc_height*3/2)/3, 1024);
            ve_chn_attr->VeAttr.AttrH264e.BufSize = AWALIGN(config->enc_bitrate*4/8 + ve_chn_attr->VeAttr.AttrH264e.mThreshSize, 1024);
            ve_chn_attr->VeAttr.AttrH264e.Profile = 2;//0:base 1:main 2:high
            ve_chn_attr->VeAttr.AttrH264e.bByFrame = TRUE;
            ve_chn_attr->VeAttr.AttrH264e.PicWidth  = config->enc_width;
            ve_chn_attr->VeAttr.AttrH264e.PicHeight = config->enc_height;
            ve_chn_attr->VeAttr.AttrH264e.mLevel = H264_LEVEL_51;
            ve_chn_attr->VeAttr.AttrH264e.FastEncFlag = FALSE;
            ve_chn_attr->VeAttr.AttrH264e.IQpOffset = 0;
            ve_chn_attr->VeAttr.AttrH264e.mbPIntraEnable = TRUE;
            break;
        }
        case PT_H265:
        {
            ve_chn_attr->VeAttr.AttrH265e.mThreshSize = AWALIGN((config->enc_width*config->enc_height*3/2)/3, 1024);
            ve_chn_attr->VeAttr.AttrH265e.mBufSize = AWALIGN(config->enc_bitrate*4/8 + ve_chn_attr->VeAttr.AttrH264e.mThreshSize, 1024);
            ve_chn_attr->VeAttr.AttrH265e.mProfile = 0; //0:main 1:main10 2:sti11
            ve_chn_attr->VeAttr.AttrH265e.mbByFrame = TRUE;
            ve_chn_attr->VeAttr.AttrH265e.mPicWidth = config->enc_width;
            ve_chn_attr->VeAttr.AttrH265e.mPicHeight = config->enc_height;
            ve_chn_attr->VeAttr.AttrH265e.mLevel = H265_LEVEL_62;
            ve_chn_attr->VeAttr.AttrH265e.mFastEncFlag = FALSE;
            ve_chn_attr->VeAttr.AttrH265e.IQpOffset = 0;
            ve_chn_attr->VeAttr.AttrH265e.mbPIntraEnable = TRUE;
            break;
        }
        case PT_MJPEG:
        {
            ve_chn_attr->VeAttr.AttrMjpeg.mbByFrame = TRUE;
            ve_chn_attr->VeAttr.AttrMjpeg.mPicWidth = config->enc_width;
            ve_chn_attr->VeAttr.AttrMjpeg.mPicHeight = config->enc_height;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", ve_chn_attr->VeAttr.Type);
            break;
        }
    }
    ve_chn_attr->VeAttr.SrcPicWidth = config->cap_width;
    ve_chn_attr->VeAttr.SrcPicHeight = config->cap_height;
    ve_chn_attr->VeAttr.Field = VIDEO_FIELD_FRAME;
    ve_chn_attr->VeAttr.PixelFormat = config->cap_format;
    ve_chn_attr->VeAttr.mColorSpace = V4L2_COLORSPACE_JPEG;
    ve_chn_attr->VeAttr.Rotate = ROTATE_NONE;

    ve_chn_attr->EncppAttr.eEncppSharpSetting = VencEncppSharp_FollowISPConfig;

    switch(ve_chn_attr->VeAttr.Type)
    {
        case PT_H264:
        {
            switch (config->enc_rcmode)
            {
                case 1:
                {
                    ve_chn_attr->RcAttr.mRcMode = VENC_RC_MODE_H264VBR;
                    ve_chn_attr->RcAttr.mAttrH264Vbr.mMaxBitRate = config->enc_bitrate;
                    ve_rc_param->ParamH264Vbr.mMaxQp = 51;
                    ve_rc_param->ParamH264Vbr.mMinQp = 10;
                    ve_rc_param->ParamH264Vbr.mMaxPqp = 50;
                    ve_rc_param->ParamH264Vbr.mMinPqp = 10;
                    ve_rc_param->ParamH264Vbr.mQpInit = 38;
                    ve_rc_param->ParamH264Vbr.mMovingTh = 20;
                    ve_rc_param->ParamH264Vbr.mQuality = 10;
                    break;
                }
                case 2:
                {
                    ve_chn_attr->RcAttr.mRcMode = VENC_RC_MODE_H264FIXQP;
                    ve_chn_attr->RcAttr.mAttrH264FixQp.mIQp = 28;
                    ve_chn_attr->RcAttr.mAttrH264FixQp.mPQp = 28;
                    break;
                }
                case 3:
                {
                    ve_chn_attr->RcAttr.mRcMode = VENC_RC_MODE_H264ABR;
                    ve_chn_attr->RcAttr.mAttrH264Abr.mMaxBitRate = config->enc_bitrate;
                    ve_chn_attr->RcAttr.mAttrH264Abr.mRatioChangeQp = 85;
                    ve_chn_attr->RcAttr.mAttrH264Abr.mQuality = 8;
                    ve_chn_attr->RcAttr.mAttrH264Abr.mMinIQp = 20;
                    ve_chn_attr->RcAttr.mAttrH264Abr.mMaxQp = 51;
                    ve_chn_attr->RcAttr.mAttrH264Abr.mMinQp = 10;
                    break;
                }
                case 0:
                default:
                {
                    ve_chn_attr->RcAttr.mRcMode = VENC_RC_MODE_H264CBR;
                    ve_chn_attr->RcAttr.mAttrH264Cbr.mBitRate = config->enc_bitrate;
                    ve_rc_param->ParamH264Cbr.mMaxQp = 51;
                    ve_rc_param->ParamH264Cbr.mMinQp = 10;
                    ve_rc_param->ParamH264Cbr.mMaxPqp = 50;
                    ve_rc_param->ParamH264Cbr.mMinPqp = 10;
                    ve_rc_param->ParamH264Cbr.mQpInit = 38;
                    break;
                }
            }
            break;
        }
        case PT_H265:
        {
            switch (config->enc_rcmode)
            {
                case 1:
                {
                    ve_chn_attr->RcAttr.mRcMode = VENC_RC_MODE_H265VBR;
                    ve_chn_attr->RcAttr.mAttrH265Vbr.mMaxBitRate = config->enc_bitrate;
                    ve_rc_param->ParamH265Vbr.mMaxQp = 51;
                    ve_rc_param->ParamH265Vbr.mMinQp = 10;
                    ve_rc_param->ParamH265Vbr.mMaxPqp = 50;
                    ve_rc_param->ParamH265Vbr.mMinPqp = 10;
                    ve_rc_param->ParamH265Vbr.mQpInit = 38;
                    ve_rc_param->ParamH265Vbr.mMovingTh = 20;
                    ve_rc_param->ParamH265Vbr.mQuality = 10;
                    break;
                }
                case 2:
                {
                    ve_chn_attr->RcAttr.mRcMode = VENC_RC_MODE_H265FIXQP;
                    ve_chn_attr->RcAttr.mAttrH265FixQp.mIQp = 28;
                    ve_chn_attr->RcAttr.mAttrH265FixQp.mPQp = 28;
                    break;
                }
                case 3:
                {
                    ve_chn_attr->RcAttr.mRcMode = VENC_RC_MODE_H265ABR;
                    ve_chn_attr->RcAttr.mAttrH265Abr.mMaxBitRate = config->enc_bitrate;
                    ve_chn_attr->RcAttr.mAttrH265Abr.mRatioChangeQp = 85;
                    ve_chn_attr->RcAttr.mAttrH265Abr.mQuality = 8;
                    ve_chn_attr->RcAttr.mAttrH265Abr.mMinIQp = 20;
                    ve_chn_attr->RcAttr.mAttrH265Abr.mMaxQp = 51;
                    ve_chn_attr->RcAttr.mAttrH265Abr.mMinQp = 10;
                    break;
                }
                case 0:
                default:
                {
                    ve_chn_attr->RcAttr.mRcMode = VENC_RC_MODE_H265CBR;
                    ve_chn_attr->RcAttr.mAttrH265Cbr.mBitRate = config->enc_bitrate;
                    ve_rc_param->ParamH265Cbr.mMaxQp = 51;
                    ve_rc_param->ParamH265Cbr.mMinQp = 1;
                    ve_rc_param->ParamH265Cbr.mMaxPqp = 50;
                    ve_rc_param->ParamH265Cbr.mMinPqp = 10;
                    ve_rc_param->ParamH265Cbr.mQpInit = 38;
                    break;
                }
            }
            break;
        }
        case PT_MJPEG:
        {
            if(config->enc_rcmode != 0)
            {
                aloge("fatal error! mjpeg don't support rcMode[%d]!", config->enc_bitrate);
            }
            ve_chn_attr->RcAttr.mRcMode = VENC_RC_MODE_MJPEGCBR;
            ve_chn_attr->RcAttr.mAttrMjpegeCbr.mBitRate = config->enc_bitrate;
            break;
        }
        default:
        {
            aloge("fatal error! not support encode type[%d], check code!", ve_chn_attr->VeAttr.Type);
            break;
        }
    }
    ve_chn_attr->GopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    alogd("venc ste Rcmode=%d", ve_chn_attr->RcAttr.mRcMode);

    return 0;
}

static int createVencChn(struct sample_muxer_multi_stream_stream *stream)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;
    struct sample_muxer_multi_stream_config *config = &stream->stream_config;

    memset(&stream->ve_chn_attr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&stream->ve_rc_param, 0, sizeof(VENC_RC_PARAM_S));
    configVencChnAttr(config, &stream->ve_chn_attr, &stream->ve_rc_param);
    stream->ve_chn = 0;
    while (stream->ve_chn < VENC_MAX_CHN_NUM)
    {
        ret = AW_MPI_VENC_CreateChn(stream->ve_chn, &stream->ve_chn_attr);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create venc channel[%d] success!", stream->ve_chn);
            break;
        }
        else if (ERR_VENC_EXIST == ret)
        {
            //alogd("venc channel[%d] is exist, find next!", stream->ve_chn);
            stream->ve_chn++;
        }
        else
        {
            aloge("fatal error! create venc channel[%d] ret[0x%x], find next!", stream->ve_chn, ret);
            stream->ve_chn++;
        }
    }

    if (nSuccessFlag == FALSE)
    {
        stream->ve_chn = MM_INVALID_CHN;
        aloge("fatal error! create venc channel fail!");
        return -1;
    }
    else
    {
        AW_MPI_VENC_SetRcParam(stream->ve_chn, &stream->ve_rc_param);
        VENC_FRAME_RATE_S stFrameRate;
        stFrameRate.SrcFrmRate = config->cap_framerate;
        stFrameRate.DstFrmRate = config->enc_framerate;
        alogd("set srcFrameRate:%d, venc framerate:%d", stFrameRate.SrcFrmRate, stFrameRate.DstFrmRate);
        ret = AW_MPI_VENC_SetFrameRate(stream->ve_chn, &stFrameRate);
        if(ret != SUCCESS)
        {
            aloge("fatal error! venc set framerate fail[0x%x]!", ret);
        }
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)stream;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_VENC_RegisterCallback(stream->ve_chn, &cbInfo);

        VENC_IspVeLinkAttr stIspVeLinkAttr;
        memset(&stIspVeLinkAttr, 0, sizeof(VENC_IspVeLinkAttr));
        stIspVeLinkAttr.bEnableIsp2Ve = TRUE;
        stIspVeLinkAttr.bEnableVe2Isp = FALSE;
        stIspVeLinkAttr.nVipp = stream->vi_dev;
        AW_MPI_VENC_EnableIspVeLink(stream->ve_chn, &stIspVeLinkAttr);
        alogd("VencChn[%d] ispVeLink:%d-%d-%d", stream->ve_chn, stIspVeLinkAttr.bEnableIsp2Ve, stIspVeLinkAttr.bEnableVe2Isp,
            stIspVeLinkAttr.nVipp);

        alogd("create venc chn[%d] success!", stream->ve_chn);
        return result;
    }
}

static int sample_muxer_multi_stream_create(struct sample_muxer_multi_stream_stream *stream)
{
    int result = 0;
    ERRORTYPE eRet = SUCCESS;

    if (!stream->stream_valid)
        return 0;

    if (createVipp(stream) != 0)
    {
        aloge("fatal eorror! create vipp fail!");
        return -1;
    }
    if (createVencChn(stream) != 0)
    {
        aloge("fatal error! create venc fail!");
        return -1;
    }
    if (stream->stream_config.enc_type == PT_H264)
        AW_MPI_VENC_GetH264SpsPpsInfo(stream->ve_chn, &stream->ve_spspps_info);
    else if (stream->stream_config.enc_type == PT_H265)
        AW_MPI_VENC_GetH265SpsPpsInfo(stream->ve_chn, &stream->ve_spspps_info);
    MPP_CHN_S ViChn = {MOD_ID_VIU, stream->vi_dev, stream->vi_chn};
    MPP_CHN_S VeChn = {MOD_ID_VENC, 0, stream->ve_chn};
    eRet = AW_MPI_SYS_Bind(&ViChn, &VeChn);
    if(eRet!=SUCCESS)
    {
        aloge("fatal error! bind vi[%d-%d] ve chn[%d] fail!",
            stream->vi_dev, stream->vi_chn, stream->ve_chn);
        return -1;
    }

    return result;
}

static int sample_muxer_multi_stream_start(struct sample_muxer_multi_stream_stream *stream)
{
    if (!stream->stream_valid)
        return 0;

    int result = 0;
    ERRORTYPE ret = SUCCESS;
    ret = AW_MPI_VI_EnableVirChn(stream->vi_dev, stream->vi_chn);
    if (ret != SUCCESS)
    {
        aloge("fatal error! viChn[%d,%d] enable error[0x%x]!", stream->vi_dev, stream->vi_chn, ret);
    }
    if (stream->ve_chn >= 0)
    {
        ret = AW_MPI_VENC_StartRecvPic(stream->ve_chn);
        if (ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] start error[0x%x]!", stream->ve_chn, ret);
        }
    }

    return result;
}
static int sample_muxer_multi_stream_stop(struct sample_muxer_multi_stream_stream *stream)
{
    ERRORTYPE ret = SUCCESS;

    if (!stream->stream_valid)
        return 0;

    if (stream->vi_chn >= 0)
    {
        ret = AW_MPI_VI_DisableVirChn(stream->vi_dev, stream->vi_chn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d]chn[%d] disabled fail[0x%x]", stream->vi_dev, stream->vi_chn, ret);
        }
    }
    if (stream->ve_chn >= 0)
    {
        ret = AW_MPI_VENC_StopRecvPic(stream->ve_chn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] stop fail[0x%x]", stream->ve_chn, ret);
        }
    }

    return 0;
}

static int sample_muxer_multi_stream_destroy(struct sample_muxer_multi_stream_stream *stream)
{
    ERRORTYPE ret = SUCCESS;

    if (stream->ve_chn >= 0)
    {
        ret = AW_MPI_VENC_ResetChn(stream->ve_chn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] stop fail[0x%x]", stream->ve_chn, ret);
        }
        ret = AW_MPI_VENC_DestroyChn(stream->ve_chn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! veChn[%d] destroy fail[0x%x]", stream->ve_chn, ret);
        }
        stream->ve_chn = MM_INVALID_CHN;
    }
    if (stream->vi_chn >= 0)
    {
        ret = AW_MPI_VI_DestroyVirChn(stream->vi_dev, stream->vi_chn);
        if(ret != SUCCESS)
        {
            aloge("fatal error! vipp[%d]Chn[%d] stop fail[0x%x]", stream->vi_dev, stream->vi_chn, ret);
        }
        stream->ve_chn = MM_INVALID_CHN;
    }
    if (stream->vi_dev >= 0)
    {
        ret = AW_MPI_VI_DisableVipp(stream->vi_dev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vi dev[%d] disable fail!", stream->vi_dev);
        }
    }
    if (stream->isp_dev >= 0)
    {
        ret = AW_MPI_ISP_Stop(stream->isp_dev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! isp[%d] stop fail!", stream->isp_dev);
        }
    }
    if (stream->vi_dev >= 0)
    {
        ret = AW_MPI_VI_DestroyVipp(stream->vi_dev);
        if (ret != SUCCESS)
        {
            aloge("fatal error! vi dev[%d] destroy fail!", stream->vi_dev);
        }
    }
    return 0;
}

static void sample_muxer_multi_stream_init(struct sample_muxer_multi_stream_context *context)
{
    for (int i = 0; i < context->stream_num; i++)
    {
        struct sample_muxer_multi_stream_stream *stream = &context->stream[i];
        struct sample_muxer_multi_stream_config *config = &stream->stream_config;
        if ((config->vi_dev < 0) || (config->cap_width <= 0) || (config->cap_height <= 0)
            || (config->enc_width <= 0) || (config->enc_height <= 0))
            continue;
        stream->stream_valid = 1;
        stream->vi_dev = MM_INVALID_DEV;
        stream->isp_dev = MM_INVALID_DEV;
        stream->vi_chn = MM_INVALID_CHN;
        stream->ve_chn = MM_INVALID_CHN;
    }
}

static MEDIA_FILE_FORMAT_E getFileFormatByName(char *pFilePath)
{
    MEDIA_FILE_FORMAT_E eFileFormat = MEDIA_FILE_FORMAT_MP4;
    char *pFileName;
    char *pFileNameExtend;
    char *ptr = strrchr(pFilePath, '/');
    if(ptr != NULL)
    {
        pFileName = ptr+1;
    }
    else
    {
        pFileName = pFilePath;
    }
    ptr = strrchr(pFileName, '.');
    if(ptr != NULL)
    {
        pFileNameExtend = ptr + 1;
    }
    else
    {
        pFileNameExtend = NULL;
    }
    if(pFileNameExtend)
    {
        if(!strcmp(pFileNameExtend, "mp4"))
        {
            eFileFormat = MEDIA_FILE_FORMAT_MP4;
        }
        else if(!strcmp(pFileNameExtend, "ts"))
        {
            eFileFormat = MEDIA_FILE_FORMAT_TS;
        }
        else
        {
            alogw("Be careful! unknown file format:%d, default to mp4", eFileFormat);
            eFileFormat = MEDIA_FILE_FORMAT_MP4;
        }
    }
    else
    {
        alogw("Be careful! extend name is not exist, default to mp4");
        eFileFormat = MEDIA_FILE_FORMAT_MP4;
    }
    return eFileFormat;
}

static void configMuxChnAttr(struct sample_muxer_multi_stream_context *context, MUX_CHN_ATTR_S *mux_chn_attr)
{
    int stream_num = 0;

    for (int i = 0; i < context->stream_num; i++)
    {
        struct sample_muxer_multi_stream_stream *stream = &context->stream[i];
        struct sample_muxer_multi_stream_config *config = &stream->stream_config;
        if (!stream->stream_valid)
            continue;
        mux_chn_attr->mVideoAttr[stream_num].mWidth = config->enc_width;
        mux_chn_attr->mVideoAttr[stream_num].mHeight = config->enc_height;
        mux_chn_attr->mVideoAttr[stream_num].mVideoFrmRate = config->enc_framerate*1000;
        mux_chn_attr->mVideoAttr[stream_num].mVideoEncodeType = config->enc_type;
        mux_chn_attr->mVideoAttr[stream_num].mVeChn = stream->ve_chn;
        mux_chn_attr->mAudioEncodeType = PT_MAX;
        mux_chn_attr->mTextEncodeType = PT_MAX;
        stream_num++;
    }
    mux_chn_attr->mVideoAttrValidNum = stream_num;
    //mux_chn_attr->mMuxerId = 0;
    mux_chn_attr->mMediaFileFormat = getFileFormatByName(context->video_dst_file);
    mux_chn_attr->mMaxFileDuration = context->video_file_duration*1000;
    mux_chn_attr->mMaxFileSizeBytes = 0;
    mux_chn_attr->mCallbackOutFlag = FALSE;
    mux_chn_attr->mFsWriteMode = FSWRITEMODE_SIMPLECACHE;
    mux_chn_attr->mSimpleCacheSize = DEFAULT_SIMPLE_CACHE_SIZE_VFS;
}

static int createMuxChn(struct sample_muxer_multi_stream_context *context)
{
    int result = 0;
    ERRORTYPE ret;
    BOOL nSuccessFlag = FALSE;

    memset(&context->mux_chn_attr, 0, sizeof(MUX_CHN_ATTR_S));
    configMuxChnAttr(context, &context->mux_chn_attr);

    char video_file[MAX_FILE_PATH_SIZE];
    generate_file_name_by_id(video_file, context, context->video_file_cnt);
    int nFd = open(video_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (nFd < 0)
    {
        aloge("fatal error! Failed to open %s", context->video_dst_file);
        return -1;
    }

    context->mux_chn = 0;
    nSuccessFlag = FALSE;
    while (context->mux_chn < MUX_MAX_CHN_NUM)
    {
        ret = AW_MPI_MUX_CreateChn(context->mux_chn, &context->mux_chn_attr, nFd, 0);
        if (SUCCESS == ret)
        {
            nSuccessFlag = TRUE;
            alogd("create muxChn[%d] success!", context->mux_chn);
            break;
        }
        else if(ERR_MUX_EXIST == ret)
        {
            context->mux_chn++;
        }
        else
        {
            aloge("fatal error! create mux chn fail[0x%x]!", ret);
            context->mux_chn++;
        }
    }
    if (FALSE == nSuccessFlag)
    {
        context->mux_chn = MM_INVALID_CHN;
        aloge("fatal error! create muxChannel fail!");
        result = -1;
    }
    else
    {
        alogd("create mux chn[%d] file format[%d]", context->mux_chn, context->mux_chn_attr.mMediaFileFormat);
        MPPCallbackInfo cbInfo;
        cbInfo.cookie = (void*)context;
        cbInfo.callback = (MPPCallbackFuncType)&MPPCallbackWrapper;
        AW_MPI_MUX_RegisterCallback(context->mux_chn, &cbInfo);
        result = 0;
    }

    for (int i = 0; i < context->stream_num; i++)
    {
        struct sample_muxer_multi_stream_stream *stream = &context->stream[i];
        if (!stream->stream_valid)
            continue;
        if (stream->ve_chn >= 0)
        {
            MPP_CHN_S src_chn = {MOD_ID_VENC, 0, stream->ve_chn};
            MPP_CHN_S dst_chn = {MOD_ID_MUX, 0, context->mux_chn};
            AW_MPI_SYS_Bind(&src_chn, &dst_chn);
            if (stream->stream_config.enc_type == PT_H264)
                AW_MPI_MUX_SetH264SpsPpsInfo(context->mux_chn, stream->ve_chn, &stream->ve_spspps_info);
            else if (stream->stream_config.enc_type == PT_H265)
                AW_MPI_MUX_SetH265SpsPpsInfo(context->mux_chn, stream->ve_chn, &stream->ve_spspps_info);
        }
    }

    if(nFd >= 0)
    {
        close(nFd);
        nFd = -1;
    }
    return result;
}

void *MsgQueueThread(void *pThreadData)
{
    struct sample_muxer_multi_stream_context *context = (struct sample_muxer_multi_stream_context *)pThreadData;
    message_t stCmdMsg;
    int nCmdPara;

    alogd("message queue thread start.");
    while (1)
    {
        if (0 == get_message(&context->msg_queue, &stCmdMsg))
        {
            nCmdPara = stCmdMsg.para0;
            switch (stCmdMsg.command)
            {
                case MPP_EVENT_NEED_NEXT_FD:
                {
                    int muxChn = nCmdPara;
                    char fileName[MAX_FILE_PATH_SIZE] = {0};
//                    if (muxerId != context->mux_chn_attr.mMuxerId)
//                        alogd("fatal error! muxerId is not match:[0x%x!=0x%x]",
//                                muxerId, context->mux_chn_attr.mMuxerId);
                    context->video_file_cnt++;
                    char video_file[MAX_FILE_PATH_SIZE];
                    generate_file_name_by_id(video_file, context, context->video_file_cnt);
                    alogd("set next file %s", video_file);
                    setNextFileToMuxer(context, video_file, 0, muxChn);
                    break;
                }
                case MPP_EVENT_RECORD_DONE:
                {
                    if (context->video_file_cnt >= context->video_file_max_cnt)
                    {
                        char video_file[MAX_FILE_PATH_SIZE];
                        int video_file_id = context->video_file_cnt - context->video_file_max_cnt - 1;
                        generate_file_name_by_id(video_file, context, video_file_id);
                        int ret = remove(video_file);
                        if (ret)
                            alogw("remove file %s fail!", video_file);
                        alogd("remove file %s success!", video_file);
                    }
                    break;
                }
                case MsgQueue_Stop:
                {
                    goto _Exit;
                    break;
                }
                default :
                {
                    break;
                }
            }
        }
        else
        {
            TMessage_WaitQueueNotEmpty(&context->msg_queue, 0);
        }
    }

_Exit:
    alogd("message queue thread stop.");
    return NULL;
}


int main(int argc, char *argv[])
{
    int result = 0;
    ERRORTYPE eRet = SUCCESS;
    message_t stCmdMsg;
    GLogConfig stGLogConfig =
    {
        .FLAGS_logtostderr = 1,
        .FLAGS_colorlogtostderr = 1,
        .FLAGS_stderrthreshold = _GLOG_INFO,
        .FLAGS_minloglevel = _GLOG_INFO,
        .FLAGS_logbuflevel = -1,
        .FLAGS_logbufsecs = 0,
        .FLAGS_max_log_size = 1,
        .FLAGS_stop_logging_if_full_disk = 1,
    };
    strcpy(stGLogConfig.LogDir, "/tmp/log");
    strcpy(stGLogConfig.InfoLogFileNameBase, "LOG-");
    strcpy(stGLogConfig.LogFileNameExtension, "IPC-");
    log_init(argv[0], &stGLogConfig);

    struct sample_muxer_multi_stream_context *context = malloc(sizeof(struct sample_muxer_multi_stream_context));
    if (!context)
    {
        result = -1;
        aloge("sample_muxer_multi_stream_context alloc fail!");
        goto _exit;
    }
    memset(context, 0, sizeof(struct sample_muxer_multi_stream_context));
    cdx_sem_init(&context->sem_exit, 0);
    if (message_create(&context->msg_queue) < 0)
    {
        aloge("fatal error! create message queue fail!");
        goto _free_context;
    }
    pthread_create(&context->msg_queue_trd, NULL, MsgQueueThread, (void *)context);
    g_context = context;

    char *config_file;
    if(ParseCmdLine(context, argc, argv) != 0)
    {
        result = -1;
        goto _free_context;
    }
    if(strlen(context->cmdline.config_file) > 0)
        config_file = context->cmdline.config_file;
    else
        config_file = NULL;
    if(loadSampleRecordConfig(context, config_file) != 0)
    {
        aloge("fatal error! no config file or parse conf file fail");
        result = -1;
        goto _free_context;
    }

    if (signal(SIGINT, handle_exit) == SIG_ERR)
        aloge("fatal error! can't catch SIGSEGV");

    MPP_SYS_CONF_S mpp_sys_conf;
    memset(&mpp_sys_conf, 0, sizeof(MPP_SYS_CONF_S));
    mpp_sys_conf.nAlignWidth = 32;
    AW_MPI_SYS_SetConf(&mpp_sys_conf);
    AW_MPI_SYS_Init();

    sample_muxer_multi_stream_init(context);

    for (int i = 0; i < context->stream_num; i++)
        sample_muxer_multi_stream_create(&context->stream[i]);
    createMuxChn(context);

    AW_MPI_MUX_StartChn(context->mux_chn);
    for (int i = 0; i < context->stream_num; i++)
        sample_muxer_multi_stream_start(&context->stream[i]);

    if (context->test_duration > 0)
        cdx_sem_down_timedwait(&context->sem_exit, context->test_duration*1000);
    else
        cdx_sem_down(&context->sem_exit);

    for (int i = 0; i < context->stream_num; i++)
        sample_muxer_multi_stream_stop(&context->stream[i]);
    AW_MPI_MUX_StopChn(context->mux_chn, FALSE);
    AW_MPI_MUX_DestroyChn(context->mux_chn);
    for (int i = 0; i < context->stream_num; i++)
        sample_muxer_multi_stream_destroy(&context->stream[i]);

    AW_MPI_SYS_Exit();
    memset(&stCmdMsg, 0, sizeof(message_t));
    stCmdMsg.command = MsgQueue_Stop;
    put_message(&context->msg_queue, &stCmdMsg);
    pthread_join(context->msg_queue_trd, NULL);
    message_destroy(&context->msg_queue);
_free_context:
    cdx_sem_deinit(&context->sem_exit);
    free(context);
_exit:
    log_quit();
    alogd("%s test result: %s", argv[0], ((0 == result) ? "success" : "fail"));
    return result;
}
