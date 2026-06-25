/******************************************************************************
  Copyright (C), 2001-2022, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : media_debug.h
  Version       : Initial Draft
  Author        : Allwinner PDC-PD5 Team
  Created       : 2022/03/26
  Last Modified :
  Description   : media debug functions implement
  Function List :
  History       :
******************************************************************************/
//#define LOG_NDEBUG 0
#define LOG_TAG "media_debug"
#include <utils/plat_log.h>

#include <mpi_videoformat_conversion.h>

#include <iniparser.h>
#include <media_debug.h>

#define MEDIA_DEBUG_CONF_PATH_LEN    (64)
#define MEDIA_DEBUG_CONF_PATH_CNT    (5)
#define MEDIA_DEBUG_CONF_PATH_1      "/mnt/extsd/media_debug.conf"
#define MEDIA_DEBUG_CONF_PATH_2      "/tmp/sd/media_debug.conf"
#define MEDIA_DEBUG_CONF_PATH_3      "/media/media_debug.conf"
#define MEDIA_DEBUG_CONF_PATH_4      "/data/media_debug.conf"
#define MEDIA_DEBUG_CONF_PATH_5      "/mnt/disk_0/media_debug.conf"
#define MEDIA_DEBUG_ENV_NAME         "MPP_MEDIA_DEBUG_FILE_PATH"

#define MEDIA_DEBUG_VENC_OUT_BITRATE_CHECK_ENV_NAME "MPP_MEDIA_DEBUG_VENC_OUT_BITRATE_CHECK"

#define MPP_VIPP_PARAM_CNT_MAX       (16)
#define MPP_VENC_PARAM_CNT_MAX       (16)
#define MPP_MUX_PARAM_CNT_MAX        (8)

static dictionary *getDictByConfPath(const char *pConfPath)
{
    dictionary *pDict = NULL;
    char strPath[MEDIA_DEBUG_CONF_PATH_LEN];

    if (NULL == pConfPath)
    {
        int is_found = 0;
        // check env first
        char *env = getenv(MEDIA_DEBUG_ENV_NAME);
        alogv("%s=%s", MEDIA_DEBUG_ENV_NAME, env);
        if (env && strlen(env))
        {
            if ((access(env, F_OK)) != -1)
            {
                if (access(env, R_OK) != -1)
                {
                    is_found = 1;
                    snprintf(strPath, sizeof(strPath), "%s", env);
                }
            }
        }
        // if env is not found, check the known path
        if (0 == is_found)
        {
            char strTotalPath[MEDIA_DEBUG_CONF_PATH_CNT][MEDIA_DEBUG_CONF_PATH_LEN] = {
                MEDIA_DEBUG_CONF_PATH_1, MEDIA_DEBUG_CONF_PATH_2, MEDIA_DEBUG_CONF_PATH_3,
                MEDIA_DEBUG_CONF_PATH_4, MEDIA_DEBUG_CONF_PATH_5
            };
            for (int i = 0; i < MEDIA_DEBUG_CONF_PATH_CNT; i++)
            {
                snprintf(strPath, sizeof(strPath), "%s", strTotalPath[i]);
                if ((access(strPath, F_OK)) != -1)
                {
                    if (access(strPath, R_OK) != -1)
                    {
                        is_found = 1;
                        break;
                    }
                }
            }
        }
        if (0 == is_found)
        {
            alogv("The file media_debug.conf was not found!");
            return NULL;
        }
    }
    else
    {
        if ((access(pConfPath, F_OK)) == -1)
        {
            alogd("%s is not exist", pConfPath);
            return NULL;
        }
        if (access(pConfPath, R_OK) == -1)
        {
            alogd("%s can not read", pConfPath);
            return NULL;
        }
        if (MEDIA_DEBUG_CONF_PATH_LEN < strlen(pConfPath))
        {
            alogw("%s len is too long, it should be less %d bytes.", pConfPath, MEDIA_DEBUG_CONF_PATH_LEN);
            return NULL;
        }
        snprintf(strPath, sizeof(strPath), "%s", pConfPath);
    }

    pDict = iniparser_load(strPath);
    if (NULL == pDict)
    {
        aloge("fatal error! load conf %s fail!", strPath);
        return NULL;
    }
    alogw("load %s success", strPath);

    return pDict;
}

#if (MPPCFG_VI == OPTION_VI_ENABLE)
static int findMppVippParamIndex(dictionary *pDict, int mVippDev)
{
    char *str = NULL;
    char paramkey[128];
    int index = -1;

    if (NULL == pDict)
    {
        aloge("fatal error, invalid params! %p", pDict);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "vipp_params:vipp_id");

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VIPP_PARAM_CNT_MAX > i))
        {
            if (mVippDev == (int)(atoi(strPart)))
            {
                index = i;
                alogd("vipp_params index:%d, vipp_id %d", index, mVippDev);
                break;
            }
            strPart = strtok(NULL, ",");
            i++;
        }
    }
    else
    {
        alogw("vipp_params, key vipp_id is not found.");
        index = -1;
    }

    return index;
}

static int findMppVippIntParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    int paramint[MPP_VIPP_PARAM_CNT_MAX];
    int result = 0;

    if ((NULL == pDict) || (MPP_VIPP_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "vipp_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VIPP_PARAM_CNT_MAX > i))
        {
            paramint[i++] = atoi(strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramint[index];
        alogv("vipp_params index:%d, key %s result %d", index, key, result);
    }
    else
    {
        alogv("vipp_params index:%d, key %s is not found.", index, key);
        result = -1;
    }

    return result;
}

static char* findMppVippStringParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    char paramstr[MPP_VIPP_PARAM_CNT_MAX][64];
    char *result = NULL;

    if ((NULL == pDict) || (MPP_VIPP_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return NULL;
    }

    snprintf(paramkey, sizeof(paramkey), "vipp_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VIPP_PARAM_CNT_MAX > i))
        {
            strcpy(paramstr[i++], strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramstr[index];
        alogv("vipp_params index:%d, key %s result %s", index, key, result);
    }
    else
    {
        alogv("vipp_params index:%d, key %s is not found.", index, key);
        result = NULL;
    }

    return result;
}

void MediaDebugLoadMppViParams(viChnManager *pVipp, const char *pConfPath)
{
    char *ptr = NULL;
    dictionary *pDict = NULL;

    if (NULL == pVipp)
    {
        aloge("fatal error, invalid params! %p", pVipp);
        return;
    }

    pDict = getDictByConfPath(pConfPath);
    if (NULL == pDict)
    {
        //aloge("fatal error! get Dict By MediaDebugConfPath fail!");
        return;
    }

    int index = findMppVippParamIndex(pDict, pVipp->vipp_dev_id);
    if (-1 == index)
    {
        alogw("vippDev: %d is not found.", pVipp->vipp_dev_id);
        iniparser_freedict(pDict);
        return;
    }
    int mVippDev = pVipp->vipp_dev_id;

    int mOnlineEnable = findMppVippIntParam(pDict, "online_en", index);
    int mOnlineShareBufNum = findMppVippIntParam(pDict, "online_share_buf_num", index);
    int mViBufferNum = findMppVippIntParam(pDict, "vi_buffer_num", index);
    int mWidth = findMppVippIntParam(pDict, "src_width", index);
    int mHeight = findMppVippIntParam(pDict, "src_height", index);
    int mFrameRate = findMppVippIntParam(pDict, "src_framerate", index);

    int mPixFmt = -1;
    ptr = (char *)findMppVippStringParam(pDict, "pixfmt", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "nv21"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420);
        }
        else if (!strcmp(ptr, "yv12"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YVU_PLANAR_420);
        }
        else if (!strcmp(ptr, "nv12"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420);
        }
        else if (!strcmp(ptr, "yu12"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_PLANAR_420);
        }
        else if (!strcmp(ptr, "lbc_2_0x"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X);
        }
        else if (!strcmp(ptr, "lbc_2_5x"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X);
        }
        else if (!strcmp(ptr, "lbc_1_5x"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X);
        }
        else if (!strcmp(ptr, "lbc_1_0x"))
        {
            mPixFmt = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X);
        }
        else
        {
            mPixFmt = -1;
            alogw("fatal error! wrong src pixfmt:%s", ptr);
        }
    }
    else
    {
        mPixFmt = -1;
    }

    int mColorSpace = -1;
    ptr = (char *)findMppVippStringParam(pDict, "color_space", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "jpeg"))
        {
            mColorSpace = V4L2_COLORSPACE_JPEG;
        }
        else if (!strcmp(ptr, "rec709"))
        {
            mColorSpace = V4L2_COLORSPACE_REC709;
        }
        else if (!strcmp(ptr, "rec709_part_range"))
        {
            mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
        }
        else
        {
            mColorSpace = -1;
            alogw("fatal error! wrong color space:%s", ptr);
        }
    }
    else
    {
        mColorSpace = -1;
    }

    int mViDropFrameNum = findMppVippIntParam(pDict, "drop_frm_num", index);
    int mWdrEn = findMppVippIntParam(pDict, "wdr_en", index);
    int mEncppEnable = findMppVippIntParam(pDict, "encpp_enable", index);

    alogw("**********************************************************");
    alogw("mpp vipp debug mode                                       ");
    alogw("**********************************************************");
    alogw("[params_name]              [user]               [debug]   ");
    alogw("vipp_id                    %-10d           %-10d          ", pVipp->vipp_dev_id, mVippDev);
    if (-1 != mOnlineEnable)
    {
        alogd("online_en                  %-10d           %-10d          ", pVipp->mstAttr.mOnlineEnable, mOnlineEnable);
        pVipp->mstAttr.mOnlineEnable = mOnlineEnable;
    }
    if (-1 != mOnlineShareBufNum)
    {
        alogd("online_share_buf_num       %-10d           %-10d          ", pVipp->mstAttr.mOnlineShareBufNum, mOnlineShareBufNum);
        pVipp->mstAttr.mOnlineShareBufNum = mOnlineShareBufNum;
    }
    if (-1 != mViBufferNum)
    {
        alogd("vi_buffer_num              %-10d           %-10d          ", pVipp->mstAttr.nbufs, mViBufferNum);
        pVipp->mstAttr.nbufs = mViBufferNum;
    }
    if (-1 != mWidth)
    {
        alogd("src_width                  %-10d           %-10d          ", pVipp->mstAttr.format.width, mWidth);
        pVipp->mstAttr.format.width = mWidth;
    }
    if (-1 != mHeight)
    {
        alogd("src_height                 %-10d           %-10d          ", pVipp->mstAttr.format.height, mHeight);
        pVipp->mstAttr.format.height = mHeight;
    }
    if (-1 != mFrameRate)
    {
        alogd("src_framerate              %-10d           %-10d          ", pVipp->mstAttr.fps, mFrameRate);
        pVipp->mstAttr.fps = mFrameRate;
    }
    if (-1 != mPixFmt)
    {
        alogd("pixfmt                     %-10d           %-10d          ", pVipp->mstAttr.format.pixelformat, mPixFmt);
        pVipp->mstAttr.format.pixelformat = mPixFmt;
    }
    if (-1 != mColorSpace)
    {
        alogd("color_space                %-10d           %-10d          ", pVipp->mstAttr.format.colorspace, mColorSpace);
        pVipp->mstAttr.format.colorspace = mColorSpace;
    }
    if (-1 != mViDropFrameNum)
    {
        alogd("drop_frm_num               %-10d           %-10d          ", pVipp->mstAttr.drop_frame_num, mViDropFrameNum);
        pVipp->mstAttr.drop_frame_num = mViDropFrameNum;
    }
    if (-1 != mWdrEn)
    {
        alogd("wdr_en                     %-10d           %-10d          ", pVipp->mstAttr.wdr_mode, mWdrEn);
        pVipp->mstAttr.wdr_mode = mWdrEn;
    }
    if (-1 != mEncppEnable)
    {
        alogd("encpp_enable               %-10d           %-10d          ", pVipp->mstAttr.mbEncppEnable, mEncppEnable);
        pVipp->mstAttr.mbEncppEnable = mEncppEnable;
    }
    alogw("**********************************************************");

    iniparser_freedict(pDict);
}
#endif

#if (MPPCFG_VENC == OPTION_VENC_ENABLE)
static int findMppVencParamIndex(dictionary *pDict, int mVencChn)
{
    char *str = NULL;
    char paramkey[128];
    int index = -1;

    if (NULL == pDict)
    {
        aloge("fatal error, invalid params! %p", pDict);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "venc_params:venc_ch_id");

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VENC_PARAM_CNT_MAX > i))
        {
            if (mVencChn == (int)(atoi(strPart)))
            {
                index = i;
                alogv("venc_params index:%d, venc_ch_id %d", index, mVencChn);
                break;
            }
            strPart = strtok(NULL, ",");
            i++;
        }
    }
    else
    {
        alogw("venc_params, key venc_ch_id is not found.");
        index = -1;
    }

    return index;
}

static int findMppVencIntParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    int paramint[MPP_VENC_PARAM_CNT_MAX];
    int result = 0;

    if ((NULL == pDict) || (MPP_VENC_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "venc_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VENC_PARAM_CNT_MAX > i))
        {
            paramint[i++] = atoi(strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramint[index];
        alogv("venc_params index:%d, key %s result %d", index, key, result);
    }
    else
    {
        alogv("venc_params index:%d, key %s is not found.", index, key);
        result = -1;
    }

    return result;
}

static char* findMppVencStringParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    char paramstr[MPP_VENC_PARAM_CNT_MAX][64];
    char *result = NULL;

    if ((NULL == pDict) || (MPP_VENC_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return NULL;
    }

    snprintf(paramkey, sizeof(paramkey), "venc_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_VENC_PARAM_CNT_MAX > i))
        {
            strcpy(paramstr[i++], strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramstr[index];
        alogv("venc_params index:%d, key %s result %s", index, key, result);
    }
    else
    {
        alogv("venc_params index:%d, key %s is not found.", index, key);
        result = NULL;
    }

    return result;
}

static double findMppVencDoubleParam(dictionary *pDict, const char *key, int index)
{
    char parameterkey[128];

    if ((NULL == pDict) || (MPP_VENC_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return (double)-1.0;
    }

    sprintf(parameterkey, "venc_params:%s", key);
    return iniparser_getdouble(pDict, parameterkey, -1.0);
}

static int getVencOutBitrateStatByKeyFrameInvertalFlag(void)
{
    int flag = 0;
    char *env = getenv(MEDIA_DEBUG_VENC_OUT_BITRATE_CHECK_ENV_NAME);
    alogv("%s=%s", MEDIA_DEBUG_VENC_OUT_BITRATE_CHECK_ENV_NAME, env);
    if (env && strlen(env))
    {
        flag = (int)(atoi(env));
    }
    alogv("flag=%d", flag);

    return flag;
}

void MediaDebugLoadMppVencParams(VIDEOENCDATATYPE *pVideoEncData, const char *pConfPath)
{
    char *ptr = NULL;
    dictionary *pDict = NULL;

    if (NULL == pVideoEncData)
    {
        aloge("fatal error, invalid params! %p", pVideoEncData);
        return;
    }

    pVideoEncData->mbDebug_VencOutBitrateStatByKeyFrameInvertalFlag = getVencOutBitrateStatByKeyFrameInvertalFlag();

    pDict = getDictByConfPath(pConfPath);
    if (NULL == pDict)
    {
        //aloge("fatal error! get Dict By MediaDebugConfPath fail!");
        return;
    }

    int index = findMppVencParamIndex(pDict, pVideoEncData->mMppChnInfo.mChnId);
    if (-1 == index)
    {
        alogw("VencChn: %d is not found.", pVideoEncData->mMppChnInfo.mChnId);
        iniparser_freedict(pDict);
        return;
    }

    int mVeChn = pVideoEncData->mMppChnInfo.mChnId;

    int mVeRecRefBufReduceEnable = findMppVencIntParam(pDict, "rec_ref_buf_reduce_enable", index);
    int mbDebug_IgnoreAllFrame = findMppVencIntParam(pDict, "debug_ignore_all_frame", index);
    int mbDisableVe2Isp = findMppVencIntParam(pDict, "disable_ve2isp", index);
    int mbDisableIsp2Ve = findMppVencIntParam(pDict, "disable_isp2ve", index);

    int mOnlineEnable = findMppVencIntParam(pDict, "online_en", index);
    int mOnlineShareBufNum = findMppVencIntParam(pDict, "online_share_buf_num", index);

    int mVideoEncoderFmt = -1;
    ptr = (char *)findMppVencStringParam(pDict, "video_encoder", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "H264"))
        {
            mVideoEncoderFmt = PT_H264;
        }
        else if (!strcmp(ptr, "H265"))
        {
            mVideoEncoderFmt = PT_H265;
        }
        else if (!strcmp(ptr, "MJPEG"))
        {
            mVideoEncoderFmt = PT_MJPEG;
        }
        else if (!strcmp(ptr, "JPEG"))
        {
            mVideoEncoderFmt = PT_JPEG;
        }
        else
        {
            mVideoEncoderFmt = -1;
            alogw("fatal error! wrong encoder type:%s", ptr);
        }
    }
    else
    {
        mVideoEncoderFmt = -1;
    }

    int mEncUseProfile = findMppVencIntParam(pDict, "profile", index);
    int mEncLevel = findMppVencIntParam(pDict, "level", index);

    int mPixFmt = -1;
    ptr = (char *)findMppVencStringParam(pDict, "pixfmt", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "nv21"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        }
        else if (!strcmp(ptr, "yv12"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YVU_PLANAR_420;
        }
        else if (!strcmp(ptr, "nv12"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420;
        }
        else if (!strcmp(ptr, "yu12"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_PLANAR_420;
        }
        else if (!strcmp(ptr, "lbc_2_0x"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X;
        }
        else if (!strcmp(ptr, "lbc_2_5x"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
        }
        else if (!strcmp(ptr, "lbc_1_5x"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X;
        }
        else if (!strcmp(ptr, "lbc_1_0x"))
        {
            mPixFmt = MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X;
        }
        else
        {
            mPixFmt = -1;
            alogw("fatal error! wrong src pixfmt:%s", ptr);
        }
    }
    else
    {
        mPixFmt = -1;
    }

    int mColorSpace = -1;
    ptr = (char *)findMppVencStringParam(pDict, "color_space", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "jpeg"))
        {
            mColorSpace = V4L2_COLORSPACE_JPEG;
        }
        else if (!strcmp(ptr, "rec709"))
        {
            mColorSpace = V4L2_COLORSPACE_REC709;
        }
        else if (!strcmp(ptr, "rec709_part_range"))
        {
            mColorSpace = V4L2_COLORSPACE_REC709_PART_RANGE;
        }
        else
        {
            mColorSpace = -1;
            alogw("fatal error! wrong color space:%s", ptr);
        }
    }
    else
    {
        mColorSpace = -1;
    }

    int mEncppEnable = findMppVencIntParam(pDict, "encpp_enable", index);
    //int mEncppSharpAttenCoefPer = findMppVencIntParam(pDict, "encppSharpAttenCoefPer", index);

    int mSrcWidth = findMppVencIntParam(pDict, "src_width", index);
    int mSrcHeight = findMppVencIntParam(pDict, "src_height", index);
    if (0 < mEncppEnable)
    {
        if (-1 != mSrcWidth)
            mSrcWidth = AWALIGN(mSrcWidth, 16);
        if (-1 != mSrcHeight)
            mSrcHeight = AWALIGN(mSrcHeight, 16);
    }

    int mVideoWidth = findMppVencIntParam(pDict, "video_width", index);
    int mVideoHeight = findMppVencIntParam(pDict, "video_height", index);

    int mProductMode = findMppVencIntParam(pDict, "product_mode", index);
    int mKeyFrameInterval = findMppVencIntParam(pDict, "key_frame_interval", index);
    int mDropFrameNum = findMppVencIntParam(pDict, "drop_frm_num", index);

    int mInitQp = findMppVencIntParam(pDict, "init_qp", index);
    int mMinIQp = findMppVencIntParam(pDict, "min_i_qp", index);
    int mMaxIQp = findMppVencIntParam(pDict, "max_i_qp", index);
    int mMinPQp = findMppVencIntParam(pDict, "min_p_qp", index);
    int mMaxPQp = findMppVencIntParam(pDict, "max_p_qp", index);
    int mEnMbQpLimit = findMppVencIntParam(pDict, "mb_qp_limit_en", index);
    int mMovingTh = findMppVencIntParam(pDict, "moving_th", index);
    int mQuality = findMppVencIntParam(pDict, "quality", index);
    int mIBitsCoef = findMppVencIntParam(pDict, "i_bits_coef", index);
    int mPBitsCoef = findMppVencIntParam(pDict, "p_bits_coef", index);

    int mSrcFrameRate = findMppVencIntParam(pDict, "src_framerate", index);
    int mVideoFrameRate = findMppVencIntParam(pDict, "video_framerate", index);
    int mVideoBitRate = findMppVencIntParam(pDict, "video_bitrate", index);

    int mPFrameIntraEn = findMppVencIntParam(pDict, "p_frame_intra_en", index);
    int mEnableFastEnc = findMppVencIntParam(pDict, "enable_fast_enc", index);

    int mGopMode = findMppVencIntParam(pDict, "gop_mode", index);
    int mGopSize = findMppVencIntParam(pDict, "gop_size", index);
    
    int m2DnrEnable = findMppVencIntParam(pDict, "2dnr_en", index);
    int m2DnrStrengthY = findMppVencIntParam(pDict, "2dnr_strength_y", index);
    int m2DnrStrengthUV = findMppVencIntParam(pDict, "2dnr_strength_c", index);
    int m2DnrThY = findMppVencIntParam(pDict, "2dnr_threshold_y", index);
    int m2DnrThUV = findMppVencIntParam(pDict, "2dnr_threshold_c", index);

    int m3DnrEnable = findMppVencIntParam(pDict, "3dnr_en", index);
    int m3DnrAdjustPixLevelEnable = findMppVencIntParam(pDict, "3dnr_pix_level_en", index);
    int m3DnrSmoothFilterEnable = findMppVencIntParam(pDict, "3dnr_smooth_en", index);
    int m3DnrMaxPixDiffTh = findMppVencIntParam(pDict, "3dnr_pix_diff_th", index);
    int m3DnrMaxMvTh = findMppVencIntParam(pDict, "3dnr_max_mv_th", index);
    int m3DnrMaxMadTh = findMppVencIntParam(pDict, "3dnr_max_mad_th", index);
    int m3DnrMinCoef = findMppVencIntParam(pDict, "3dnr_min_coef", index);
    int m3DnrMaxCoef = findMppVencIntParam(pDict, "3dnr_max_coef", index);

    int mCropEnable = findMppVencIntParam(pDict, "crop_en", index);
    int mCropRectX = findMppVencIntParam(pDict, "crop_rect_x", index);
    int mCropRectY = findMppVencIntParam(pDict, "crop_rect_y", index);
    int mCropRectWidth = findMppVencIntParam(pDict, "crop_rect_w", index);
    int mCropRectHeight = findMppVencIntParam(pDict, "crop_rect_h", index);

    VENC_SUPERFRAME_CFG_S mSuperFrmParam;
    memset(&mSuperFrmParam, -1, sizeof(VENC_SUPERFRAME_CFG_S));
    int mSuperFrmMode = findMppVencIntParam(pDict, "super_frm_mode", index);
    mSuperFrmParam.enSuperFrmMode = mSuperFrmMode;
    mSuperFrmParam.SuperIFrmBitsThr = findMppVencIntParam(pDict, "super_i_frm_bits_thr", index);
    mSuperFrmParam.SuperPFrmBitsThr = findMppVencIntParam(pDict, "super_p_frm_bits_thr", index);
    mSuperFrmParam.MaxRencodeTimes = findMppVencIntParam(pDict, "max_rencode_times", index);
    mSuperFrmParam.MaxP2IFrameBitsRatio = findMppVencDoubleParam(pDict, "max_p2i_frm_bits_ratio", index);

    int mEncodeRotate = findMppVencIntParam(pDict, "encode_rotate", index);
    switch(mEncodeRotate)
    {
    case 0:
        mEncodeRotate = ROTATE_NONE;
        break;
    case 90:
        mEncodeRotate = ROTATE_90;
        break;
    case 180:
        mEncodeRotate = ROTATE_180;
        break;
    case 270:
        mEncodeRotate = ROTATE_270;
        break;
    default:
        mEncodeRotate = -1;
        break;
    }

    int mHorizonFlipFlag = findMppVencIntParam(pDict, "mirror", index);
    int mColor2Grey = findMppVencIntParam(pDict, "color2grey", index);

    int mVbvBufferSize = findMppVencIntParam(pDict, "vbvBufferSize", index);
    int mVbvThreshSize = findMppVencIntParam(pDict, "vbvThreshSize", index);

    int mVeRefFrameLbcMode = -1;
    ptr = (char *)findMppVencStringParam(pDict, "ve_ref_frame_lbc_mode", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "lbc_disable"))
        {
            mVeRefFrameLbcMode = LBC_MODE_DISABLE;
        }
        else if (!strcmp(ptr, "lbc_1_5x"))
        {
            mVeRefFrameLbcMode = LBC_MODE_1_5X;
        }
        else if (!strcmp(ptr, "lbc_2_0x"))
        {
            mVeRefFrameLbcMode = LBC_MODE_2_0X;
        }
        else if (!strcmp(ptr, "lbc_2_5x"))
        {
            mVeRefFrameLbcMode = LBC_MODE_2_5X;
        }
        else if (!strcmp(ptr, "lbc_1_0x"))
        {
            mVeRefFrameLbcMode = LBC_MODE_NO_LOSSY;
        }
        else
        {
            mVeRefFrameLbcMode = -1;
            alogw("fatal error! wrong ve ref frame lbc mode:%s", ptr);
        }
    }
    else
    {
        mVeRefFrameLbcMode = -1;
    }

    VencTargetBitsClipParam mBitsClipParam;
    memset(&mBitsClipParam, -1, sizeof(VencTargetBitsClipParam));
    mBitsClipParam.dis_default_para = findMppVencIntParam(pDict, "bits_clip_dis_default", index);
    mBitsClipParam.mode = findMppVencIntParam(pDict, "bits_clip_mode", index);
    mBitsClipParam.coef_th[0][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[0][0]", index);
    mBitsClipParam.coef_th[0][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[0][1]", index);
    mBitsClipParam.coef_th[1][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[1][0]", index);
    mBitsClipParam.coef_th[1][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[1][1]", index);
    mBitsClipParam.coef_th[2][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[2][0]", index);
    mBitsClipParam.coef_th[2][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[2][1]", index);
    mBitsClipParam.coef_th[3][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[3][0]", index);
    mBitsClipParam.coef_th[3][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[3][1]", index);
    mBitsClipParam.coef_th[4][0] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[4][0]", index);
    mBitsClipParam.coef_th[4][1] = (float)findMppVencDoubleParam(pDict, "bits_clip_coef[4][1]", index);
    mBitsClipParam.en_gop_clip = findMppVencIntParam(pDict, "en_gop_clip", index);
    mBitsClipParam.gop_bit_ratio_th[0] = (float)findMppVencDoubleParam(pDict, "gop_bit_ratio_th[0]", index);
    mBitsClipParam.gop_bit_ratio_th[1] = (float)findMppVencDoubleParam(pDict, "gop_bit_ratio_th[1]", index);
    mBitsClipParam.gop_bit_ratio_th[2] = (float)findMppVencDoubleParam(pDict, "gop_bit_ratio_th[2]", index);

    int EnIFrmMbRcMoveStatusEnable = findMppVencIntParam(pDict, "en_ifrm_mb_rc_move_status_enable", index);
    int EnIFrmMbRcMoveStatus = findMppVencIntParam(pDict, "en_ifrm_mb_rc_move_status", index);

    int mBitsRatioEnable = findMppVencIntParam(pDict, "i_p_target_bits_ratio_enable", index);
    VencIPTargetBitsRatio mBitsRatio;
    memset(&mBitsRatio, -1, sizeof(VencIPTargetBitsRatio));
    mBitsRatio.nSceneCoef[0] = (float)findMppVencDoubleParam(pDict, "i_p_target_bits_ratio_scene_coef[0]", index);
    mBitsRatio.nSceneCoef[1] = (float)findMppVencDoubleParam(pDict, "i_p_target_bits_ratio_scene_coef[1]", index);
    mBitsRatio.nSceneCoef[2] = (float)findMppVencDoubleParam(pDict, "i_p_target_bits_ratio_scene_coef[2]", index);
    mBitsRatio.nMoveCoef[0] =  (float)findMppVencDoubleParam(pDict, "i_p_target_bits_ratio_move_coef[0]", index);
    mBitsRatio.nMoveCoef[1] =  (float)findMppVencDoubleParam(pDict, "i_p_target_bits_ratio_move_coef[1]", index);
    mBitsRatio.nMoveCoef[2] =  (float)findMppVencDoubleParam(pDict, "i_p_target_bits_ratio_move_coef[2]", index);
    mBitsRatio.nMoveCoef[3] =  (float)findMppVencDoubleParam(pDict, "i_p_target_bits_ratio_move_coef[3]", index);
    mBitsRatio.nMoveCoef[4] =  (float)findMppVencDoubleParam(pDict, "i_p_target_bits_ratio_move_coef[4]", index);

    int mWeakTextureThEnable = findMppVencIntParam(pDict, "en_weak_texture_th", index);
    float mWeakTextureTh = (float)findMppVencDoubleParam(pDict, "weak_texture_th", index);

    int mD3DInIFrmEnable = findMppVencIntParam(pDict, "en_d3d_in_i_frm", index);
    int mTightMbQpEnable = findMppVencIntParam(pDict, "en_tight_mb_qp", index);

    VencExtremeD3DParam mExtremeD3D;
    memset(&mExtremeD3D, -1, sizeof(VencExtremeD3DParam));
    int en_extreme_d3d = findMppVencIntParam(pDict, "en_extreme_d3d", index);
    mExtremeD3D.en_extreme_d3d = en_extreme_d3d;
    mExtremeD3D.zero_mv_ratio_th = (float)findMppVencDoubleParam(pDict, "ex_d3d_zero_mv_ratio_th", index);
    mExtremeD3D.ex_d3d_param.enable_3d_filter = (unsigned char)findMppVencIntParam(pDict, "ex_d3d_enable_3d_filter", index);
    mExtremeD3D.ex_d3d_param.adjust_pix_level_enable = (unsigned char)findMppVencIntParam(pDict, "ex_d3d_adjust_pix_level_enable", index);
    mExtremeD3D.ex_d3d_param.smooth_filter_enable = (unsigned char)findMppVencIntParam(pDict, "ex_d3d_smooth_filter_enable", index);
    mExtremeD3D.ex_d3d_param.max_pix_diff_th = (unsigned char)findMppVencIntParam(pDict, "ex_d3d_max_pix_diff_th", index);
    mExtremeD3D.ex_d3d_param.max_mad_th = (unsigned char)findMppVencIntParam(pDict, "ex_d3d_max_mad_th", index);
    mExtremeD3D.ex_d3d_param.max_mv_th = (unsigned char)findMppVencIntParam(pDict, "ex_d3d_max_mv_th", index);
    mExtremeD3D.ex_d3d_param.min_coef = (unsigned char)findMppVencIntParam(pDict, "ex_d3d_min_coef", index);
    mExtremeD3D.ex_d3d_param.max_coef = (unsigned char)findMppVencIntParam(pDict, "ex_d3d_max_coef", index);

    VencRegionD3DParam mRegionD3DParam;
    memset(&mRegionD3DParam, -1, sizeof(VencRegionD3DParam));
    mRegionD3DParam.en_region_d3d = findMppVencIntParam(pDict, "en_region_d3d", index);
    mRegionD3DParam.dis_default_para = findMppVencIntParam(pDict, "region_d3d_dis_default_para", index);
    mRegionD3DParam.result_num = findMppVencIntParam(pDict, "region_d3d_result_num", index);
    mRegionD3DParam.hor_region_num = findMppVencIntParam(pDict, "region_d3d_hor_region_num", index);
    mRegionD3DParam.ver_region_num = findMppVencIntParam(pDict, "region_d3d_ver_region_num", index);
    mRegionD3DParam.hor_expand_num = findMppVencIntParam(pDict, "region_d3d_hor_expand_num", index);
    mRegionD3DParam.ver_expand_num = findMppVencIntParam(pDict, "region_d3d_ver_expand_num", index);
    mRegionD3DParam.lv_weak_th[0] = findMppVencIntParam(pDict, "region_d3d_lv_weak_th[0]", index);
    mRegionD3DParam.lv_weak_th[1] = findMppVencIntParam(pDict, "region_d3d_lv_weak_th[1]", index);
    mRegionD3DParam.lv_weak_th[2] = findMppVencIntParam(pDict, "region_d3d_lv_weak_th[2]", index);
    mRegionD3DParam.lv_weak_th[3] = findMppVencIntParam(pDict, "region_d3d_lv_weak_th[3]", index);
    mRegionD3DParam.lv_weak_th[4] = findMppVencIntParam(pDict, "region_d3d_lv_weak_th[4]", index);
    mRegionD3DParam.lv_weak_th[5] = findMppVencIntParam(pDict, "region_d3d_lv_weak_th[5]", index);
    mRegionD3DParam.lv_weak_th[6] = findMppVencIntParam(pDict, "region_d3d_lv_weak_th[6]", index);
    mRegionD3DParam.lv_weak_th[7] = findMppVencIntParam(pDict, "region_d3d_lv_weak_th[7]", index);
    mRegionD3DParam.lv_weak_th[8] = findMppVencIntParam(pDict, "region_d3d_lv_weak_th[8]", index);
    mRegionD3DParam.zero_mv_rate_th[0] = (float)findMppVencDoubleParam(pDict, "region_d3d_zero_mv_rate_th[0]", index);
    mRegionD3DParam.zero_mv_rate_th[1] = (float)findMppVencDoubleParam(pDict, "region_d3d_zero_mv_rate_th[1]", index);
    mRegionD3DParam.zero_mv_rate_th[2] = (float)findMppVencDoubleParam(pDict, "region_d3d_zero_mv_rate_th[2]", index);
    mRegionD3DParam.chroma_offset = (unsigned char)findMppVencIntParam(pDict, "region_d3d_chroma_offset", index);
    mRegionD3DParam.static_coef[0] = (unsigned char)findMppVencIntParam(pDict, "region_d3d_static_coef[0]", index);
    mRegionD3DParam.static_coef[1] = (unsigned char)findMppVencIntParam(pDict, "region_d3d_static_coef[1]", index);
    mRegionD3DParam.static_coef[2] = (unsigned char)findMppVencIntParam(pDict, "region_d3d_static_coef[2]", index);
    mRegionD3DParam.motion_coef[0] = (unsigned char)findMppVencIntParam(pDict, "region_d3d_motion_coef[0]", index);
    mRegionD3DParam.motion_coef[1] = (unsigned char)findMppVencIntParam(pDict, "region_d3d_motion_coef[1]", index);
    mRegionD3DParam.motion_coef[2] = (unsigned char)findMppVencIntParam(pDict, "region_d3d_motion_coef[2]", index);
    mRegionD3DParam.motion_coef[3] = (unsigned char)findMppVencIntParam(pDict, "region_d3d_motion_coef[3]", index);

    int mChromaQPOffsetEnable = findMppVencIntParam(pDict, "en_chroma_qp_offset", index);
    int mChromaQPOffset = findMppVencIntParam(pDict, "chroma_qp_offset", index);

    int mH264ConstraintFlagEnable = findMppVencIntParam(pDict, "en_h264_constraint_flag", index);
    VencH264ConstraintFlag mH264ConstraintFlag;
    memset(&mH264ConstraintFlag, -1, sizeof(VencH264ConstraintFlag));
    mH264ConstraintFlag.constraint_0 = (unsigned char)findMppVencIntParam(pDict, "h264_constraint_flag_bit0", index);
    mH264ConstraintFlag.constraint_1 = (unsigned char)findMppVencIntParam(pDict, "h264_constraint_flag_bit1", index);
    mH264ConstraintFlag.constraint_2 = (unsigned char)findMppVencIntParam(pDict, "h264_constraint_flag_bit2", index);
    mH264ConstraintFlag.constraint_3 = (unsigned char)findMppVencIntParam(pDict, "h264_constraint_flag_bit3", index);
    mH264ConstraintFlag.constraint_4 = (unsigned char)findMppVencIntParam(pDict, "h264_constraint_flag_bit4", index);
    mH264ConstraintFlag.constraint_5 = (unsigned char)findMppVencIntParam(pDict, "h264_constraint_flag_bit5", index);

    VencVe2IspD2DLimit mVe2IspD2DLimit;
    memset(&mVe2IspD2DLimit, -1, sizeof(VencVe2IspD2DLimit));
    mVe2IspD2DLimit.en_d2d_limit = findMppVencIntParam(pDict, "en_ve2isp_d2d_limit", index);
    mVe2IspD2DLimit.d2d_level[0] = findMppVencIntParam(pDict, "ve2isp_d2d_limit_d2d_level0", index);
    mVe2IspD2DLimit.d2d_level[1] = findMppVencIntParam(pDict, "ve2isp_d2d_limit_d2d_level1", index);
    mVe2IspD2DLimit.d2d_level[2] = findMppVencIntParam(pDict, "ve2isp_d2d_limit_d2d_level2", index);
    mVe2IspD2DLimit.d2d_level[3] = findMppVencIntParam(pDict, "ve2isp_d2d_limit_d2d_level3", index);
    mVe2IspD2DLimit.d2d_level[4] = findMppVencIntParam(pDict, "ve2isp_d2d_limit_d2d_level4", index);
    mVe2IspD2DLimit.d2d_level[5] = findMppVencIntParam(pDict, "ve2isp_d2d_limit_d2d_level5", index);


    alogw("**********************************************************");
    alogw("mpp venc debug mode                                       ");
    alogw("**********************************************************");
    alogw("[params_name]              [user]               [debug]   ");

    alogw("venc_ch_id                 %-10d           %-10d          ", pVideoEncData->mMppChnInfo.mChnId, mVeChn);

    if (-1 != mVeRecRefBufReduceEnable)
    {
        alogd("rec_ref_buf_reduce_enable  %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mVeRecRefBufReduceEnable, mVeRecRefBufReduceEnable);
        pVideoEncData->mEncChnAttr.VeAttr.mVeRecRefBufReduceEnable = mVeRecRefBufReduceEnable;
    }
    if (-1 != mbDebug_IgnoreAllFrame)
    {
        alogd("debug_ignore_all_frame     %-10d           %-10d          ", pVideoEncData->mbDebug_IgnoreAllFrame, mbDebug_IgnoreAllFrame);
        pVideoEncData->mbDebug_IgnoreAllFrame = mbDebug_IgnoreAllFrame;
    }
    if (-1 != mbDisableIsp2Ve)
    {
        alogd("enable_isp2ve             %-10d           %-10d          ", pVideoEncData->stIspVeLinkAttr.bEnableIsp2Ve, !mbDisableIsp2Ve);
        pVideoEncData->stIspVeLinkAttr.bEnableIsp2Ve = !mbDisableIsp2Ve;
    }
    if (-1 != mbDisableVe2Isp)
    {
        alogd("enable_ve2isp             %-10d           %-10d          ", pVideoEncData->stIspVeLinkAttr.bEnableVe2Isp, !mbDisableVe2Isp);
        pVideoEncData->stIspVeLinkAttr.bEnableVe2Isp = !mbDisableVe2Isp;
    }

    if (-1 != mOnlineEnable)
    {
        alogd("online_en                  %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mOnlineEnable, mOnlineEnable);
        pVideoEncData->mEncChnAttr.VeAttr.mOnlineEnable = mOnlineEnable;
        if (0 < mOnlineEnable)
        {
            alogd("online_share_buf_num       %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mOnlineShareBufNum, mOnlineShareBufNum);
            pVideoEncData->mEncChnAttr.VeAttr.mOnlineShareBufNum = mOnlineShareBufNum;
        }
    }

    if (-1 != mPixFmt)
    {
        alogd("pixfmt                     %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.PixelFormat, mPixFmt);
        pVideoEncData->mEncChnAttr.VeAttr.PixelFormat = mPixFmt;
    }

    if (-1 != mColorSpace)
    {
        alogd("color_space                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mColorSpace, mColorSpace);
        pVideoEncData->mEncChnAttr.VeAttr.mColorSpace = mColorSpace;
    }

    if (-1 != mEncppEnable)
    {
        if (0 == mEncppEnable && pVideoEncData->mEncChnAttr.EncppAttr.eEncppSharpSetting != VencEncppSharp_Disable)
        {
            //pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth = pVideoEncData->mUserEncChnAttr.VeAttr.SrcPicWidth;
            //pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight = pVideoEncData->mUserEncChnAttr.VeAttr.SrcPicHeight;
        }
        if (0 < mEncppEnable)
        {
            pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth = AWALIGN(pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, 16);
            pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight = AWALIGN(pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight, 16);
        }
        alogd("encpp_enable               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.EncppAttr.eEncppSharpSetting, mEncppEnable);
        pVideoEncData->mEncChnAttr.EncppAttr.eEncppSharpSetting = mEncppEnable?VencEncppSharp_FollowISPConfig:VencEncppSharp_Disable;
        pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_ENCPPENABLE;
    }
    /*if (-1 != mEncppSharpAttenCoefPer)
    {
        alogd("encppSharpAttenCoefPer     %-10d           %-10d          ", pVideoEncData->mEncChnAttr.EncppAttr.mEncppSharpAttenCoefPer, mEncppSharpAttenCoefPer);
        pVideoEncData->mEncChnAttr.EncppAttr.mEncppSharpAttenCoefPer = mEncppSharpAttenCoefPer;
    }*/

    if (-1 != mSrcWidth)
    {
        alogd("src_width                  %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, mSrcWidth);
        pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth  = mSrcWidth;
    }
    if (-1 != mSrcHeight)
    {
        alogd("src_height                 %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight, mSrcHeight);
        pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight = mSrcHeight;
    }

    if (-1 != mProductMode)
    {
        alogd("product_mode               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mProductMode, mProductMode);
        pVideoEncData->mEncChnAttr.RcAttr.mProductMode = mProductMode;
    }

    if (-1 != mKeyFrameInterval)
    {
        alogd("key_frame_interval         %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval, mKeyFrameInterval);
        pVideoEncData->mEncChnAttr.VeAttr.MaxKeyInterval = mKeyFrameInterval;
        pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_KEYFRAMEINTERVAL;
    }

    if (-1 != mDropFrameNum)
    {
        alogd("drop_frm_num               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mDropFrameNum, mDropFrameNum);
        pVideoEncData->mEncChnAttr.VeAttr.mDropFrameNum = mDropFrameNum;
    }

    if (-1 != mSrcFrameRate)
    {
        alogd("src_framerate              %-10d           %-10d          ", pVideoEncData->mFrameRateInfo.SrcFrmRate, mSrcFrameRate);
        pVideoEncData->mFrameRateInfo.SrcFrmRate = mSrcFrameRate;
    }
    if (-1 != mVideoFrameRate)
    {
        alogd("dst_framerate              %-10d           %-10d          ", pVideoEncData->mFrameRateInfo.DstFrmRate, mVideoFrameRate);
        pVideoEncData->mFrameRateInfo.DstFrmRate = mVideoFrameRate;
    }

    if (-1 != mVideoEncoderFmt)
    {
        alogd("video_encoder              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.Type, mVideoEncoderFmt);
        pVideoEncData->mEncChnAttr.VeAttr.Type = mVideoEncoderFmt;
    }
    else
    {
        // update video format for other parameters
        mVideoEncoderFmt = pVideoEncData->mEncChnAttr.VeAttr.Type;
    }

    // this configs depends on the video format
    if (PT_JPEG == mVideoEncoderFmt)
    {
        if (-1 == mVideoWidth)
            mVideoWidth = pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicWidth;
        if (-1 == mVideoHeight)
            mVideoHeight = pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicHeight;
        if (-1 == mVbvBufferSize)
            mVbvBufferSize = pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.BufSize;
        if (-1 == mVbvThreshSize)
            mVbvThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.mThreshSize;
    }
    else if (PT_MJPEG == mVideoEncoderFmt)
    {
        if (-1 == mVideoWidth)
            mVideoWidth = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicWidth;
        if (-1 == mVideoHeight)
            mVideoHeight = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicHeight;
        if (-1 == mVbvBufferSize)
            mVbvBufferSize = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mBufSize;
        if (-1 == mVbvThreshSize)
            mVbvThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mThreshSize;
    }
    else if (PT_H264 == mVideoEncoderFmt)
    {
        if (-1 == mEncUseProfile)
            mEncUseProfile = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.Profile;
        if (-1 == mEncLevel)
            mEncLevel = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mLevel;
        if (-1 == mVideoWidth)
            mVideoWidth = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth;
        if (-1 == mVideoHeight)
            mVideoHeight = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight;
        if (-1 == mPFrameIntraEn)
            mPFrameIntraEn = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable;
        if (-1 == mEnableFastEnc)
            mEnableFastEnc = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.FastEncFlag;
        if (-1 == mVbvBufferSize)
            mVbvBufferSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.BufSize;
        if (-1 == mVbvThreshSize)
            mVbvThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mThreshSize;
    }
    else if (PT_H265 == mVideoEncoderFmt)
    {
        if (-1 == mEncUseProfile)
            mEncUseProfile = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mProfile;
        if (-1 == mEncLevel)
            mEncLevel = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mLevel;
        if (-1 == mVideoWidth)
            mVideoWidth = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicWidth;
        if (-1 == mVideoHeight)
            mVideoHeight = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicHeight;
        if (-1 == mPFrameIntraEn)
            mPFrameIntraEn = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable;
        if (-1 == mEnableFastEnc)
            mEnableFastEnc = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mFastEncFlag;
        if (-1 == mVbvBufferSize)
            mVbvBufferSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mBufSize;
        if (-1 == mVbvThreshSize)
            mVbvThreshSize = pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mThreshSize;
    }

    if (PT_JPEG == mVideoEncoderFmt)
    {
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicWidth != mVideoWidth)
        {
            alogd("video_width                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicWidth, mVideoWidth);
            pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicWidth = mVideoWidth;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicHeight != mVideoHeight)
        {
            alogd("video_height               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicHeight, mVideoHeight);
            pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.PicHeight = mVideoHeight;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.BufSize != mVbvBufferSize)
        {
            alogd("vbvBufferSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.BufSize, mVbvBufferSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.BufSize = mVbvBufferSize;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.mThreshSize != mVbvThreshSize)
        {
            alogd("vbvThreshSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.mThreshSize, mVbvThreshSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrJpeg.mThreshSize = mVbvThreshSize;
        }
    }
    else if (PT_MJPEG == mVideoEncoderFmt)
    {
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicWidth != mVideoWidth)
        {
            alogd("video_width                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicWidth, mVideoWidth);
            pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicWidth = mVideoWidth;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicHeight != mVideoHeight)
        {
            alogd("video_height               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicHeight, mVideoHeight);
            pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mPicHeight = mVideoHeight;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mBufSize != mVbvBufferSize)
        {
            alogd("vbvBufferSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mBufSize, mVbvBufferSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mBufSize = mVbvBufferSize;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mThreshSize != mVbvThreshSize)
        {
            alogd("vbvThreshSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mThreshSize, mVbvThreshSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrMjpeg.mThreshSize = mVbvThreshSize;
        }
    }
    else if (PT_H264 == mVideoEncoderFmt)
    {
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.Profile != mEncUseProfile)
        {
            alogd("profile                    %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.Profile, mEncUseProfile);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.Profile = mEncUseProfile;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mLevel != mEncLevel)
        {
            alogd("level                      %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mLevel, mEncLevel);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mLevel = mEncLevel;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth != mVideoWidth)
        {
            alogd("video_width                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth, mVideoWidth);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicWidth = mVideoWidth;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight != mVideoHeight)
        {
            alogd("video_height               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight, mVideoHeight);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.PicHeight = mVideoHeight;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable != mPFrameIntraEn)
        {
            alogd("p_frame_intra_en           %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable, mPFrameIntraEn);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mbPIntraEnable = mPFrameIntraEn;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.FastEncFlag != mEnableFastEnc)
        {
            alogd("enable_fast_enc            %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.FastEncFlag, mEnableFastEnc);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.FastEncFlag = mEnableFastEnc;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.BufSize != mVbvBufferSize)
        {
            alogd("vbvBufferSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.BufSize, mVbvBufferSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.BufSize = mVbvBufferSize;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mThreshSize != mVbvThreshSize)
        {
            alogd("vbvThreshSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mThreshSize, mVbvThreshSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH264e.mThreshSize = mVbvThreshSize;
        }
    }
    else if (PT_H265 == mVideoEncoderFmt)
    {
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mProfile != mEncUseProfile)
        {
            alogd("profile                    %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mProfile, mEncUseProfile);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mProfile = mEncUseProfile;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mLevel != mEncLevel)
        {
            alogd("level                      %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mLevel, mEncLevel);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mLevel = mEncLevel;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicWidth != mVideoWidth)
        {
            alogd("video_width                %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicWidth, mVideoWidth);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicWidth = mVideoWidth;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicHeight != mVideoHeight)
        {
            alogd("video_height               %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicHeight, mVideoHeight);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mPicHeight = mVideoHeight;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable != mPFrameIntraEn)
        {
            alogd("p_frame_intra_en           %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable, mPFrameIntraEn);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mbPIntraEnable = mPFrameIntraEn;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mFastEncFlag != mEnableFastEnc)
        {
            alogd("enable_fast_enc            %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mFastEncFlag, mEnableFastEnc);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mFastEncFlag = mEnableFastEnc;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mBufSize != mVbvBufferSize)
        {
            alogd("vbvBufferSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mBufSize, mVbvBufferSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mBufSize = mVbvBufferSize;
        }
        if (pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mThreshSize != mVbvThreshSize)
        {
            alogd("vbvThreshSize              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mThreshSize, mVbvThreshSize);
            pVideoEncData->mEncChnAttr.VeAttr.AttrH265e.mThreshSize = mVbvThreshSize;
        }
    }

    VENC_RC_MODE_E mRcMode = VENC_RC_MODE_BUTT;
    int rc_mode = findMppVencIntParam(pDict, "rc_mode", index);
    if (-1 == rc_mode)
    {
        // update rc mode for other parameters
        mRcMode = pVideoEncData->mEncChnAttr.RcAttr.mRcMode;
        switch (mRcMode)
        {
        case VENC_RC_MODE_H264CBR:    /* 1 */
        case VENC_RC_MODE_H265CBR:    /* 14 */
        case VENC_RC_MODE_MJPEGCBR:   /* 6 */
            rc_mode = 0;
            break;
        case VENC_RC_MODE_H264VBR:    /* 2 */
        case VENC_RC_MODE_H265VBR:    /* 15 */
            rc_mode = 1;
            break;
        case VENC_RC_MODE_H264FIXQP:  /* 4 */
        case VENC_RC_MODE_H265FIXQP:  /* 17 */
        case VENC_RC_MODE_MJPEGFIXQP: /* 9 */
            rc_mode = 2;
            break;
        case VENC_RC_MODE_H264QPMAP:  /* 5 */
        case VENC_RC_MODE_H265QPMAP:  /* 18 */
            rc_mode = 3;
            break;
        default:
            rc_mode = -1;
            break;
        }
    }

    if (PT_H264 == mVideoEncoderFmt)
    {
        switch (rc_mode)
        {
        case 0:
            mRcMode = VENC_RC_MODE_H264CBR;
            break;
        case 1:
            mRcMode = VENC_RC_MODE_H264VBR;
            break;
        case 2:
            mRcMode = VENC_RC_MODE_H264FIXQP;
            break;
        case 3:
            mRcMode = VENC_RC_MODE_H264QPMAP;
            break;
        default:
            mRcMode = VENC_RC_MODE_BUTT;
            break;
        }
    }
    else if (PT_H265 == mVideoEncoderFmt)
    {
        switch (rc_mode)
        {
        case 0:
            mRcMode = VENC_RC_MODE_H265CBR;
            break;
        case 1:
            mRcMode = VENC_RC_MODE_H265VBR;
            break;
        case 2:
            mRcMode = VENC_RC_MODE_H265FIXQP;
            break;
        case 3:
            mRcMode = VENC_RC_MODE_H265QPMAP;
            break;
        default:
            mRcMode = VENC_RC_MODE_BUTT;
            break;
        }
    }
    else if (PT_MJPEG == mVideoEncoderFmt)
    {
        switch (rc_mode)
        {
        case 0:
            mRcMode = VENC_RC_MODE_MJPEGCBR;
            break;
        case 2:
            mRcMode = VENC_RC_MODE_MJPEGFIXQP;
            break;
        default:
            mRcMode = VENC_RC_MODE_BUTT;
            break;
        }
    }
    else
    {
        mRcMode = VENC_RC_MODE_BUTT;
    }

    if (pVideoEncData->mEncChnAttr.RcAttr.mRcMode != mRcMode)
    {
        alogd("rc_mode                    %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mRcMode, mRcMode);
        pVideoEncData->mEncChnAttr.RcAttr.mRcMode = mRcMode;
    }

    switch (mRcMode)
    {
    case VENC_RC_MODE_H264CBR:    /* 1 */
        if (-1 == mInitQp)
            mInitQp = pVideoEncData->mRcParam.ParamH264Cbr.mQpInit;
        if (-1 == mMinIQp)
            mMinIQp = pVideoEncData->mRcParam.ParamH264Cbr.mMinQp;
        if (-1 == mMaxIQp)
            mMaxIQp = pVideoEncData->mRcParam.ParamH264Cbr.mMaxQp;
        if (-1 == mMinPQp)
            mMinPQp = pVideoEncData->mRcParam.ParamH264Cbr.mMinPqp;
        if (-1 == mMaxPQp)
            mMaxPQp = pVideoEncData->mRcParam.ParamH264Cbr.mMaxPqp;
        if (-1 == mEnMbQpLimit)
            mEnMbQpLimit = pVideoEncData->mRcParam.ParamH264Cbr.mbEnMbQpLimit;
        if (-1 == mVideoBitRate)
            mVideoBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate;
        break;
    case VENC_RC_MODE_H265CBR:    /* 14 */
        if (-1 == mInitQp)
            mInitQp = pVideoEncData->mRcParam.ParamH265Cbr.mQpInit;
        if (-1 == mMinIQp)
            mMinIQp = pVideoEncData->mRcParam.ParamH265Cbr.mMinQp;
        if (-1 == mMaxIQp)
            mMaxIQp = pVideoEncData->mRcParam.ParamH265Cbr.mMaxQp;
        if (-1 == mMinPQp)
            mMinPQp = pVideoEncData->mRcParam.ParamH265Cbr.mMinPqp;
        if (-1 == mMaxPQp)
            mMaxPQp = pVideoEncData->mRcParam.ParamH265Cbr.mMaxPqp;
        if (-1 == mEnMbQpLimit)
            mEnMbQpLimit = pVideoEncData->mRcParam.ParamH265Cbr.mbEnMbQpLimit;
        if (-1 == mVideoBitRate)
            mVideoBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate;
        break;
    case VENC_RC_MODE_MJPEGCBR:   /* 6 */
        if (-1 == mVideoBitRate)
            mVideoBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate;
        break;
    case VENC_RC_MODE_H264VBR:    /* 2 */
        if (-1 == mInitQp)
            mInitQp = pVideoEncData->mRcParam.ParamH264Vbr.mQpInit;
        if (-1 == mMinIQp)
            mMinIQp = pVideoEncData->mRcParam.ParamH264Vbr.mMinQp;
        if (-1 == mMaxIQp)
            mMaxIQp = pVideoEncData->mRcParam.ParamH264Vbr.mMaxQp;
        if (-1 == mMinPQp)
            mMinPQp = pVideoEncData->mRcParam.ParamH264Vbr.mMinPqp;
        if (-1 == mMaxPQp)
            mMaxPQp = pVideoEncData->mRcParam.ParamH264Vbr.mMaxPqp;
        if (-1 == mEnMbQpLimit)
            mEnMbQpLimit = pVideoEncData->mRcParam.ParamH264Vbr.mbEnMbQpLimit;
        if (-1 == mMovingTh)
            mMovingTh = pVideoEncData->mRcParam.ParamH264Vbr.mMovingTh;
        if (-1 == mQuality)
            mQuality = pVideoEncData->mRcParam.ParamH264Vbr.mQuality;
        if (-1 == mIBitsCoef)
            mIBitsCoef = pVideoEncData->mRcParam.ParamH264Vbr.mIFrmBitsCoef;
        if (-1 == mPBitsCoef)
            mPBitsCoef = pVideoEncData->mRcParam.ParamH264Vbr.mPFrmBitsCoef;
        if (-1 == mVideoBitRate)
            mVideoBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate;
        break;
    case VENC_RC_MODE_H265VBR:    /* 15 */
        if (-1 == mInitQp)
            mInitQp = pVideoEncData->mRcParam.ParamH265Vbr.mQpInit;
        if (-1 == mMinIQp)
            mMinIQp = pVideoEncData->mRcParam.ParamH265Vbr.mMinQp;
        if (-1 == mMaxIQp)
            mMaxIQp = pVideoEncData->mRcParam.ParamH265Vbr.mMaxQp;
        if (-1 == mMinPQp)
            mMinPQp = pVideoEncData->mRcParam.ParamH265Vbr.mMinPqp;
        if (-1 == mMaxPQp)
            mMaxPQp = pVideoEncData->mRcParam.ParamH265Vbr.mMaxPqp;
        if (-1 == mEnMbQpLimit)
            mEnMbQpLimit = pVideoEncData->mRcParam.ParamH265Vbr.mbEnMbQpLimit;
        if (-1 == mMovingTh)
            mMovingTh = pVideoEncData->mRcParam.ParamH265Vbr.mMovingTh;
        if (-1 == mQuality)
            mQuality = pVideoEncData->mRcParam.ParamH265Vbr.mQuality;
        if (-1 == mIBitsCoef)
            mIBitsCoef = pVideoEncData->mRcParam.ParamH265Vbr.mIFrmBitsCoef;
        if (-1 == mPBitsCoef)
            mPBitsCoef = pVideoEncData->mRcParam.ParamH265Vbr.mPFrmBitsCoef;
        if (-1 == mVideoBitRate)
            mVideoBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate;
        break;
    case VENC_RC_MODE_H264FIXQP:  /* 4 */
        if (-1 == mMinIQp)
            mMinIQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mIQp;
        if (-1 == mMinPQp)
            mMinPQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mPQp;
        break;
    case VENC_RC_MODE_H265FIXQP:  /* 17 */
        if (-1 == mMinIQp)
            mMinIQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mIQp;
        if (-1 == mMinPQp)
            mMinPQp = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mPQp;
        break;
    case VENC_RC_MODE_MJPEGFIXQP: /* 9 */
        if (-1 == mQuality)
            mQuality = pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor;
        break;
    case VENC_RC_MODE_H264QPMAP:  /* 5 */
        if (-1 == mInitQp)
            mInitQp = pVideoEncData->mRcParam.ParamH264QpMap.mQpInit;
        if (-1 == mMinIQp)
            mMinIQp = pVideoEncData->mRcParam.ParamH264QpMap.mMinQp;
        if (-1 == mMaxIQp)
            mMaxIQp = pVideoEncData->mRcParam.ParamH264QpMap.mMaxQp;
        if (-1 == mMinPQp)
            mMinPQp = pVideoEncData->mRcParam.ParamH264QpMap.mMinPqp;
        if (-1 == mMaxPQp)
            mMaxPQp = pVideoEncData->mRcParam.ParamH264QpMap.mMaxPqp;
        if (-1 == mEnMbQpLimit)
            mEnMbQpLimit = pVideoEncData->mRcParam.ParamH264QpMap.mbEnMbQpLimit;
        if (-1 == mVideoBitRate)
            mVideoBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH264QpMap.mMaxBitRate;
        break;
    case VENC_RC_MODE_H265QPMAP:  /* 18 */
        if (-1 == mInitQp)
            mInitQp = pVideoEncData->mRcParam.ParamH265QpMap.mQpInit;
        if (-1 == mMinIQp)
            mMinIQp = pVideoEncData->mRcParam.ParamH265QpMap.mMinQp;
        if (-1 == mMaxIQp)
            mMaxIQp = pVideoEncData->mRcParam.ParamH265QpMap.mMaxQp;
        if (-1 == mMinPQp)
            mMinPQp = pVideoEncData->mRcParam.ParamH265QpMap.mMinPqp;
        if (-1 == mMaxPQp)
            mMaxPQp = pVideoEncData->mRcParam.ParamH265QpMap.mMaxPqp;
        if (-1 == mEnMbQpLimit)
            mEnMbQpLimit = pVideoEncData->mRcParam.ParamH265QpMap.mbEnMbQpLimit;
        if (-1 == mVideoBitRate)
            mVideoBitRate = pVideoEncData->mEncChnAttr.RcAttr.mAttrH265QpMap.mMaxBitRate;
        break;
    default:
        alogw("fatal error! unknown rc_mode %d", mRcMode);
        break;
    }

    if (-1 != mVideoBitRate)
        pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_BITRATE;

    if (VENC_RC_MODE_H264CBR == mRcMode)
    {
        if (pVideoEncData->mRcParam.ParamH264Cbr.mQpInit != mInitQp)
        {
            alogd("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mQpInit = mInitQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Cbr.mMinQp != mMinIQp)
        {
            alogd("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mMinQp = mMinIQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Cbr.mMaxQp != mMaxIQp)
        {
            alogd("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mMaxQp = mMaxIQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Cbr.mMinPqp != mMinPQp)
        {
            alogd("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mMinPqp = mMinPQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Cbr.mMaxPqp != mMaxPQp)
        {
            alogd("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH264Cbr.mMaxPqp = mMaxPQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Cbr.mbEnMbQpLimit != mEnMbQpLimit)
        {
            alogd("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Cbr.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH264Cbr.mbEnMbQpLimit = mEnMbQpLimit;
        }
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate != mVideoBitRate)
        {
            alogd("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate, mVideoBitRate);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Cbr.mBitRate = mVideoBitRate;
        }
    }
    else if (VENC_RC_MODE_H265CBR == mRcMode)
    {
        if (pVideoEncData->mRcParam.ParamH265Cbr.mQpInit != mInitQp)
        {
            alogd("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mQpInit = mInitQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Cbr.mMinQp != mMinIQp)
        {
            alogd("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mMinQp = mMinIQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Cbr.mMaxQp != mMaxIQp)
        {
            alogd("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mMaxQp = mMaxIQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Cbr.mMinPqp != mMinPQp)
        {
            alogd("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mMinPqp = mMinPQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Cbr.mMaxPqp != mMaxPQp)
        {
            alogd("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH265Cbr.mMaxPqp = mMaxPQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Cbr.mbEnMbQpLimit != mEnMbQpLimit)
        {
            alogd("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Cbr.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH265Cbr.mbEnMbQpLimit = mEnMbQpLimit;
        }
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate != mVideoBitRate)
        {
            alogd("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate, mVideoBitRate);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Cbr.mBitRate = mVideoBitRate;
        }
    }
    else if (VENC_RC_MODE_MJPEGCBR == mRcMode)
    {
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate != mVideoBitRate)
        {
            alogd("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate, mVideoBitRate);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeCbr.mBitRate = mVideoBitRate;
        }
    }
    else if (VENC_RC_MODE_H264VBR == mRcMode)
    {
        if (pVideoEncData->mRcParam.ParamH264Vbr.mQpInit != mInitQp)
        {
            alogd("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mQpInit = mInitQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Vbr.mMinQp != mMinIQp)
        {
            alogd("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mMinQp = mMinIQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Vbr.mMaxQp != mMaxIQp)
        {
            alogd("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mMaxQp = mMaxIQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Vbr.mMinPqp != mMinPQp)
        {
            alogd("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mMinPqp = mMinPQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Vbr.mMaxPqp != mMaxPQp)
        {
            alogd("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH264Vbr.mMaxPqp = mMaxPQp;
        }
        if (pVideoEncData->mRcParam.ParamH264Vbr.mbEnMbQpLimit != mEnMbQpLimit)
        {
            alogd("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH264Vbr.mbEnMbQpLimit = mEnMbQpLimit;
        }
        if (pVideoEncData->mRcParam.ParamH264Vbr.mMovingTh != mMovingTh)
        {
            alogd("moving_th                  %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mMovingTh, mMovingTh);
            pVideoEncData->mRcParam.ParamH264Vbr.mMovingTh = mMovingTh;
        }
        if (pVideoEncData->mRcParam.ParamH264Vbr.mQuality != mQuality)
        {
            alogd("quality                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mQuality, mQuality);
            pVideoEncData->mRcParam.ParamH264Vbr.mQuality = mQuality;
        }
        if (pVideoEncData->mRcParam.ParamH264Vbr.mIFrmBitsCoef != mIBitsCoef)
        {
            alogd("i_bits_coef                %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mIFrmBitsCoef, mIBitsCoef);
            pVideoEncData->mRcParam.ParamH264Vbr.mIFrmBitsCoef = mIBitsCoef;
        }
        if (pVideoEncData->mRcParam.ParamH264Vbr.mPFrmBitsCoef != mPBitsCoef)
        {
            alogd("p_bits_coef                %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264Vbr.mPFrmBitsCoef, mPBitsCoef);
            pVideoEncData->mRcParam.ParamH264Vbr.mPFrmBitsCoef = mPBitsCoef;
        }
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate != mVideoBitRate)
        {
            alogd("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate, mVideoBitRate);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH264Vbr.mMaxBitRate = mVideoBitRate;
        }
    }
    else if (VENC_RC_MODE_H265VBR == mRcMode)
    {
        if (pVideoEncData->mRcParam.ParamH265Vbr.mQpInit != mInitQp)
        {
            alogd("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mQpInit = mInitQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Vbr.mMinQp != mMinIQp)
        {
            alogd("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mMinQp = mMinIQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Vbr.mMaxQp != mMaxIQp)
        {
            alogd("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mMaxQp = mMaxIQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Vbr.mMinPqp != mMinPQp)
        {
            alogd("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mMinPqp = mMinPQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Vbr.mMaxPqp != mMaxPQp)
        {
            alogd("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH265Vbr.mMaxPqp = mMaxPQp;
        }
        if (pVideoEncData->mRcParam.ParamH265Vbr.mbEnMbQpLimit != mEnMbQpLimit)
        {
            alogd("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH265Vbr.mbEnMbQpLimit = mEnMbQpLimit;
        }
        if (pVideoEncData->mRcParam.ParamH265Vbr.mMovingTh != mMovingTh)
        {
            alogd("moving_th                  %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mMovingTh, mMovingTh);
            pVideoEncData->mRcParam.ParamH265Vbr.mMovingTh = mMovingTh;
        }
        if (pVideoEncData->mRcParam.ParamH265Vbr.mQuality != mQuality)
        {
            alogd("quality                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mQuality, mQuality);
            pVideoEncData->mRcParam.ParamH265Vbr.mQuality = mQuality;
        }
        if (pVideoEncData->mRcParam.ParamH265Vbr.mIFrmBitsCoef != mIBitsCoef)
        {
            alogd("i_bits_coef                %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mIFrmBitsCoef, mIBitsCoef);
            pVideoEncData->mRcParam.ParamH265Vbr.mIFrmBitsCoef = mIBitsCoef;
        }
        if (pVideoEncData->mRcParam.ParamH265Vbr.mPFrmBitsCoef != mPBitsCoef)
        {
            alogd("p_bits_coef                %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265Vbr.mPFrmBitsCoef, mPBitsCoef);
            pVideoEncData->mRcParam.ParamH265Vbr.mPFrmBitsCoef = mPBitsCoef;
        }
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate != mVideoBitRate)
        {
            alogd("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate, mVideoBitRate);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH265Vbr.mMaxBitRate = mVideoBitRate;
        }
    }
    else if (VENC_RC_MODE_H264FIXQP == mRcMode)
    {
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mIQp != mMinIQp)
        {
            alogd("i_qp                       %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mIQp, mMinIQp);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mIQp = mMinIQp;
        }
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mPQp != mMinPQp)
        {
            alogd("p_qp                       %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mPQp, mMinPQp);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH264FixQp.mPQp = mMinPQp;
        }
    }
    else if (VENC_RC_MODE_H265FIXQP == mRcMode)
    {
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mIQp != mMinIQp)
        {
            alogd("i_qp                       %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mIQp, mMinIQp);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mIQp = mMinIQp;
        }
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mPQp != mMinPQp)
        {
            alogd("p_qp                       %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mPQp, mMinPQp);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH265FixQp.mPQp = mMinPQp;
        }
    }
    else if (VENC_RC_MODE_MJPEGFIXQP == mRcMode)
    {
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor != mQuality)
        {
            alogd("qfactor                    %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor, mQuality);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrMjpegeFixQp.mQfactor = mQuality;
        }
    }
    else if (VENC_RC_MODE_H264QPMAP == mRcMode)
    {
        if (pVideoEncData->mRcParam.ParamH264QpMap.mQpInit != mInitQp)
        {
            alogd("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mQpInit = mInitQp;
        }
        if (pVideoEncData->mRcParam.ParamH264QpMap.mMinQp != mMinIQp)
        {
            alogd("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mMinQp = mMinIQp;
        }
        if (pVideoEncData->mRcParam.ParamH264QpMap.mMaxQp != mMaxIQp)
        {
            alogd("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mMaxQp = mMaxIQp;
        }
        if (pVideoEncData->mRcParam.ParamH264QpMap.mMinPqp != mMinPQp)
        {
            alogd("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mMinPqp = mMinPQp;
        }
        if (pVideoEncData->mRcParam.ParamH264QpMap.mMaxPqp != mMaxPQp)
        {
            alogd("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH264QpMap.mMaxPqp = mMaxPQp;
        }
        if (pVideoEncData->mRcParam.ParamH264QpMap.mbEnMbQpLimit != mEnMbQpLimit)
        {
            alogd("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH264QpMap.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH264QpMap.mbEnMbQpLimit = mEnMbQpLimit;
        }
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH264QpMap.mMaxBitRate != mVideoBitRate)
        {
            alogd("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH264QpMap.mMaxBitRate, mVideoBitRate);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH264QpMap.mMaxBitRate = mVideoBitRate;
        }
    }
    else if (VENC_RC_MODE_H265QPMAP == mRcMode)
    {
        if (pVideoEncData->mRcParam.ParamH265QpMap.mQpInit != mInitQp)
        {
            alogd("init_qp                    %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mQpInit, mInitQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mQpInit = mInitQp;
        }
        if (pVideoEncData->mRcParam.ParamH265QpMap.mMinQp != mMinIQp)
        {
            alogd("min_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mMinQp, mMinIQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mMinQp = mMinIQp;
        }
        if (pVideoEncData->mRcParam.ParamH265QpMap.mMaxQp != mMaxIQp)
        {
            alogd("max_i_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mMaxQp, mMaxIQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mMaxQp = mMaxIQp;
        }
        if (pVideoEncData->mRcParam.ParamH265QpMap.mMinPqp != mMinPQp)
        {
            alogd("min_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mMinPqp, mMinPQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mMinPqp = mMinPQp;
        }
        if (pVideoEncData->mRcParam.ParamH265QpMap.mMaxPqp != mMaxPQp)
        {
            alogd("max_p_qp                   %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mMaxPqp, mMaxPQp);
            pVideoEncData->mRcParam.ParamH265QpMap.mMaxPqp = mMaxPQp;
        }
        if (pVideoEncData->mRcParam.ParamH265QpMap.mbEnMbQpLimit != mEnMbQpLimit)
        {
            alogd("mb_qp_limit_en             %-10d           %-10d          ", pVideoEncData->mRcParam.ParamH265QpMap.mbEnMbQpLimit, mEnMbQpLimit);
            pVideoEncData->mRcParam.ParamH265QpMap.mbEnMbQpLimit = mEnMbQpLimit;
        }
        if (pVideoEncData->mEncChnAttr.RcAttr.mAttrH265QpMap.mMaxBitRate != mVideoBitRate)
        {
            alogd("video_bitrate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.RcAttr.mAttrH265QpMap.mMaxBitRate, mVideoBitRate);
            pVideoEncData->mEncChnAttr.RcAttr.mAttrH265QpMap.mMaxBitRate = mVideoBitRate;
        }
    }

    if (-1 != mGopMode)
    {
        alogd("gop_mode                   %-10d           %-10d          ", pVideoEncData->mEncChnAttr.GopAttr.enGopMode, mGopMode);
        pVideoEncData->mEncChnAttr.GopAttr.enGopMode = mGopMode;
    }
    if (-1 != mGopSize)
    {
        alogd("gop_size                   %-10d           %-10d          ", pVideoEncData->mEncChnAttr.GopAttr.mGopSize, mGopSize);
        pVideoEncData->mEncChnAttr.GopAttr.mGopSize = mGopSize;
    }

    if ((PT_H264 == mVideoEncoderFmt) || (PT_H265 == mVideoEncoderFmt))
    {
        if (-1 != m2DnrEnable)
        {
            alogd("2dnr_en                    %-10d           %-10d          ", pVideoEncData->m2DfilterParam.enable_2d_filter, m2DnrEnable);
            pVideoEncData->m2DfilterParam.enable_2d_filter = m2DnrEnable;
            if (0 < m2DnrEnable)
            {
                alogd("2dnr_strength_y            %-10d           %-10d          ", pVideoEncData->m2DfilterParam.filter_strength_y, m2DnrStrengthY);
                pVideoEncData->m2DfilterParam.filter_strength_y = m2DnrStrengthY;
                alogd("2dnr_strength_c            %-10d           %-10d          ", pVideoEncData->m2DfilterParam.filter_strength_uv, m2DnrStrengthUV);
                pVideoEncData->m2DfilterParam.filter_strength_uv = m2DnrStrengthUV;
                alogd("2dnr_threshold_y           %-10d           %-10d          ", pVideoEncData->m2DfilterParam.filter_th_y, m2DnrThY);
                pVideoEncData->m2DfilterParam.filter_th_y = m2DnrThY;
                alogd("2dnr_threshold_c           %-10d           %-10d          ", pVideoEncData->m2DfilterParam.filter_th_uv, m2DnrThUV);
                pVideoEncData->m2DfilterParam.filter_th_uv = m2DnrThUV;
                pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_2DNR;
            }
        }
        if (-1 != m3DnrEnable)
        {
            alogd("3dnr_en                    %-10d           %-10d          ", pVideoEncData->m3DfilterParam.enable_3d_filter, m3DnrEnable);
            pVideoEncData->m3DfilterParam.enable_3d_filter = m3DnrEnable;
            if (0 < m3DnrEnable)
            {
                alogd("3dnr_pix_level_en          %-10d           %-10d          ", pVideoEncData->m3DfilterParam.adjust_pix_level_enable, m3DnrAdjustPixLevelEnable);
                pVideoEncData->m3DfilterParam.adjust_pix_level_enable = m3DnrAdjustPixLevelEnable;
                alogd("3dnr_smooth_en             %-10d           %-10d          ", pVideoEncData->m3DfilterParam.smooth_filter_enable, m3DnrSmoothFilterEnable);
                pVideoEncData->m3DfilterParam.smooth_filter_enable = m3DnrSmoothFilterEnable;
                alogd("3dnr_pix_diff_th           %-10d           %-10d          ", pVideoEncData->m3DfilterParam.max_pix_diff_th, m3DnrMaxPixDiffTh);
                pVideoEncData->m3DfilterParam.max_pix_diff_th = m3DnrMaxPixDiffTh;
                alogd("3dnr_max_mv_th             %-10d           %-10d          ", pVideoEncData->m3DfilterParam.max_mv_th, m3DnrMaxMvTh);
                pVideoEncData->m3DfilterParam.max_mv_th = m3DnrMaxMvTh;
                alogd("3dnr_max_mad_th            %-10d           %-10d          ", pVideoEncData->m3DfilterParam.max_mad_th, m3DnrMaxMadTh);
                pVideoEncData->m3DfilterParam.max_mad_th = m3DnrMaxMadTh;
                alogd("3dnr_min_coef              %-10d           %-10d          ", pVideoEncData->m3DfilterParam.min_coef, m3DnrMinCoef);
                pVideoEncData->m3DfilterParam.min_coef = m3DnrMinCoef;
                alogd("3dnr_max_coef              %-10d           %-10d          ", pVideoEncData->m3DfilterParam.max_coef, m3DnrMaxCoef);
                pVideoEncData->m3DfilterParam.max_coef = m3DnrMaxCoef;
                pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_3DNR;
            }
        }
        if (-1 != mColor2Grey)
        {
            alogd("color2grey                 %-10d           %-10d          ", pVideoEncData->mColor2GreyParam.bColor2Grey, mColor2Grey);
            pVideoEncData->mColor2GreyParam.bColor2Grey = mColor2Grey;
            pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_COLOR2GREY;
        }
    }

    if (-1 != mCropEnable)
    {
        alogd("crop_en                    %-10d           %-10d          ", pVideoEncData->mCropCfg.bEnable, mCropEnable);
        pVideoEncData->mCropCfg.bEnable = mCropEnable;
        if (0 < mCropEnable)
        {
            alogd("crop_rect_x                %-10d           %-10d          ", pVideoEncData->mCropCfg.Rect.X, mCropRectX);
            pVideoEncData->mCropCfg.Rect.X = mCropRectX;
            alogd("crop_rect_y                %-10d           %-10d          ", pVideoEncData->mCropCfg.Rect.Y, mCropRectY);
            pVideoEncData->mCropCfg.Rect.Y = mCropRectY;
            alogd("crop_rect_w                %-10d           %-10d          ", pVideoEncData->mCropCfg.Rect.Width, mCropRectWidth);
            pVideoEncData->mCropCfg.Rect.Width = mCropRectWidth;
            alogd("crop_rect_h                %-10d           %-10d          ", pVideoEncData->mCropCfg.Rect.Height, mCropRectHeight);
            pVideoEncData->mCropCfg.Rect.Height = mCropRectHeight;
            pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_CROP;
        }
    }

    if (-1 != mSuperFrmMode)
    {
        alogd("super_frm_mode             %-10d           %-10d          ", pVideoEncData->mSuperFrmParam.enSuperFrmMode, mSuperFrmParam.enSuperFrmMode);
        pVideoEncData->mSuperFrmParam.enSuperFrmMode = mSuperFrmParam.enSuperFrmMode;
        if (0 <= mSuperFrmParam.enSuperFrmMode)
        {
            alogd("super_i_frm_bits_thr       %-10d           %-10d          ", pVideoEncData->mSuperFrmParam.SuperIFrmBitsThr, mSuperFrmParam.SuperIFrmBitsThr);
            pVideoEncData->mSuperFrmParam.SuperIFrmBitsThr = mSuperFrmParam.SuperIFrmBitsThr;
            alogd("super_p_frm_bits_thr       %-10d           %-10d          ", pVideoEncData->mSuperFrmParam.SuperPFrmBitsThr, mSuperFrmParam.SuperPFrmBitsThr);
            pVideoEncData->mSuperFrmParam.SuperPFrmBitsThr = mSuperFrmParam.SuperPFrmBitsThr;
            alogd("super_max_rencode_times    %-10d           %-10d          ", pVideoEncData->mSuperFrmParam.MaxRencodeTimes, mSuperFrmParam.MaxRencodeTimes);
            pVideoEncData->mSuperFrmParam.MaxRencodeTimes = mSuperFrmParam.MaxRencodeTimes;
            alogd("max_p2i_frm_bits_ratio     %-10.2f           %-10.2f          ", pVideoEncData->mSuperFrmParam.MaxP2IFrameBitsRatio, mSuperFrmParam.MaxP2IFrameBitsRatio);
            pVideoEncData->mSuperFrmParam.MaxP2IFrameBitsRatio = mSuperFrmParam.MaxP2IFrameBitsRatio;

            VencSuperFrameConfig stSuperFrameConfig;
            memset(&stSuperFrameConfig, 0, sizeof(VencSuperFrameConfig));
            switch(mSuperFrmParam.enSuperFrmMode)
            {
                case SUPERFRM_NONE:
                    stSuperFrameConfig.eSuperFrameMode = VENC_SUPERFRAME_NONE;
                    break;
                case SUPERFRM_DISCARD:
                    stSuperFrameConfig.eSuperFrameMode = VENC_SUPERFRAME_DISCARD;
                    break;
                case SUPERFRM_REENCODE:
                    stSuperFrameConfig.eSuperFrameMode = VENC_SUPERFRAME_REENCODE;
                    break;
                default:
                    aloge("fatal error! wrong superFrmMode[0x%x]", mSuperFrmParam.enSuperFrmMode);
                    stSuperFrameConfig.eSuperFrameMode = VENC_SUPERFRAME_NONE;
                    break;
            }
            stSuperFrameConfig.nMaxIFrameBits = mSuperFrmParam.SuperIFrmBitsThr;
            stSuperFrameConfig.nMaxPFrameBits = mSuperFrmParam.SuperPFrmBitsThr;
            stSuperFrameConfig.nMaxRencodeTimes = mSuperFrmParam.MaxRencodeTimes;
            stSuperFrameConfig.nMaxP2IFrameBitsRatio = mSuperFrmParam.MaxP2IFrameBitsRatio;
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSuperFrameConfig, (void*)&stSuperFrameConfig);
            pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_SUPERFRM;
        }
    }

    if (-1 != mEncodeRotate)
    {
        alogd("encode_rotate              %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.Rotate, mEncodeRotate);
        pVideoEncData->mEncChnAttr.VeAttr.Rotate = mEncodeRotate;
    }
    if (-1 != mHorizonFlipFlag)
    {
        alogd("mirror                     %-10d           %-10d          ", pVideoEncData->mHorizonFlipFlag, mHorizonFlipFlag);
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamHorizonFlip, (void*)&mHorizonFlipFlag);
        pVideoEncData->mHorizonFlipFlag = mHorizonFlipFlag;
        pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_MIRROR;
    }

    if (-1 != mVeRefFrameLbcMode)
    {
        alogd("ve_ref_frame_lbc_mode      %-10d           %-10d          ", pVideoEncData->mEncChnAttr.VeAttr.mVeRefFrameLbcMode, mVeRefFrameLbcMode);
        pVideoEncData->mEncChnAttr.VeAttr.mVeRefFrameLbcMode = mVeRefFrameLbcMode;
        pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_VEREFFRAMELBCMODE;
    }

    if (-1 != mBitsClipParam.dis_default_para)
    {
        alogd("bits_clip_dis_default      %-10d           %-10d          ", pVideoEncData->mRcParam.mBitsClipParam.dis_default_para, mBitsClipParam.dis_default_para);
        pVideoEncData->mRcParam.mBitsClipParam.dis_default_para = mBitsClipParam.dis_default_para;
        if (0 < mBitsClipParam.dis_default_para)
        {
            alogd("bits_clip_mode             %-10d           %-10d          ", pVideoEncData->mRcParam.mBitsClipParam.mode, mBitsClipParam.mode);
            pVideoEncData->mRcParam.mBitsClipParam.mode = mBitsClipParam.mode;
            alogd("bits_clip_coef[0][0]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[0][0], mBitsClipParam.coef_th[0][0]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[0][0] = mBitsClipParam.coef_th[0][0];
            alogd("bits_clip_coef[0][1]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[0][1], mBitsClipParam.coef_th[0][1]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[0][1] = mBitsClipParam.coef_th[0][1];
            alogd("bits_clip_coef[1][0]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[1][0], mBitsClipParam.coef_th[1][0]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[1][0] = mBitsClipParam.coef_th[1][0];
            alogd("bits_clip_coef[1][1]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[1][1], mBitsClipParam.coef_th[1][1]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[1][1] = mBitsClipParam.coef_th[1][1];
            alogd("bits_clip_coef[2][1]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[2][0], mBitsClipParam.coef_th[2][0]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[2][0] = mBitsClipParam.coef_th[2][0];
            alogd("bits_clip_coef[2][1]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[2][1], mBitsClipParam.coef_th[2][1]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[2][1] = mBitsClipParam.coef_th[2][1];
            alogd("bits_clip_coef[3][1]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[3][0], mBitsClipParam.coef_th[3][0]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[3][0] = mBitsClipParam.coef_th[3][0];
            alogd("bits_clip_coef[3][1]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[3][1], mBitsClipParam.coef_th[3][1]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[3][1] = mBitsClipParam.coef_th[3][1];
            alogd("bits_clip_coef[4][0]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[4][0], mBitsClipParam.coef_th[4][0]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[4][0] = mBitsClipParam.coef_th[4][0];
            alogd("bits_clip_coef[4][1]       %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.coef_th[4][1], mBitsClipParam.coef_th[4][1]);
            pVideoEncData->mRcParam.mBitsClipParam.coef_th[4][1] = mBitsClipParam.coef_th[4][1];
            alogd("en_gop_clip                %-10d           %-10d          ", pVideoEncData->mRcParam.mBitsClipParam.en_gop_clip, mBitsClipParam.en_gop_clip);
            pVideoEncData->mRcParam.mBitsClipParam.en_gop_clip = mBitsClipParam.en_gop_clip;
            alogd("gop_bit_ratio_th[0]        %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.gop_bit_ratio_th[0], mBitsClipParam.gop_bit_ratio_th[0]);
            pVideoEncData->mRcParam.mBitsClipParam.gop_bit_ratio_th[0] = mBitsClipParam.gop_bit_ratio_th[0];
            alogd("gop_bit_ratio_th[1]        %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.gop_bit_ratio_th[1], mBitsClipParam.gop_bit_ratio_th[1]);
            pVideoEncData->mRcParam.mBitsClipParam.gop_bit_ratio_th[1] = mBitsClipParam.gop_bit_ratio_th[1];
            alogd("gop_bit_ratio_th[2]        %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsClipParam.gop_bit_ratio_th[2], mBitsClipParam.gop_bit_ratio_th[2]);
            pVideoEncData->mRcParam.mBitsClipParam.gop_bit_ratio_th[2] = mBitsClipParam.gop_bit_ratio_th[2];
        }
    }

    if (-1 != EnIFrmMbRcMoveStatusEnable)
    {
        alogd("ifrm_mb_rc_move_status_en  %-10d           %-10d          ", pVideoEncData->mRcParam.EnIFrmMbRcMoveStatusEnable, EnIFrmMbRcMoveStatusEnable);
        pVideoEncData->mRcParam.EnIFrmMbRcMoveStatusEnable = EnIFrmMbRcMoveStatusEnable;
        if (0 < EnIFrmMbRcMoveStatusEnable)
        {
            alogd("ifrm_mb_rc_move_status     %-10d           %-10d          ", pVideoEncData->mRcParam.EnIFrmMbRcMoveStatus, EnIFrmMbRcMoveStatus);
            pVideoEncData->mRcParam.EnIFrmMbRcMoveStatus = EnIFrmMbRcMoveStatus;
        }
    }

    if (-1 != mBitsRatioEnable)
    {
        alogd("i_p_target_bits_ratio_en   %-10d           %-10d          ", pVideoEncData->mRcParam.mBitsRatioEnable, mBitsRatioEnable);
        pVideoEncData->mRcParam.mBitsRatioEnable = mBitsRatioEnable;
        if (0 < mBitsRatioEnable)
        {
            alogd("bits_ratio_scene_coef[0]   %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsRatio.nSceneCoef[0], mBitsRatio.nSceneCoef[0]);
            pVideoEncData->mRcParam.mBitsRatio.nSceneCoef[0] = mBitsRatio.nSceneCoef[0];
            alogd("bits_ratio_scene_coef[1]   %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsRatio.nSceneCoef[1], mBitsRatio.nSceneCoef[1]);
            pVideoEncData->mRcParam.mBitsRatio.nSceneCoef[1] = mBitsRatio.nSceneCoef[1];
            alogd("bits_ratio_scene_coef[2]   %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsRatio.nSceneCoef[2], mBitsRatio.nSceneCoef[2]);
            pVideoEncData->mRcParam.mBitsRatio.nSceneCoef[2] = mBitsRatio.nSceneCoef[2];
            alogd("bits_ratio_move_coef[0]    %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[0], mBitsRatio.nMoveCoef[0]);
            pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[0] = mBitsRatio.nMoveCoef[0];
            alogd("bits_ratio_move_coef[1]    %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[1], mBitsRatio.nMoveCoef[1]);
            pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[1] = mBitsRatio.nMoveCoef[1];
            alogd("bits_ratio_move_coef[2]    %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[2], mBitsRatio.nMoveCoef[2]);
            pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[2] = mBitsRatio.nMoveCoef[2];
            alogd("bits_ratio_move_coef[3]    %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[3], mBitsRatio.nMoveCoef[3]);
            pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[3] = mBitsRatio.nMoveCoef[3];
            alogd("bits_ratio_move_coef[4]    %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[4], mBitsRatio.nMoveCoef[4]);
            pVideoEncData->mRcParam.mBitsRatio.nMoveCoef[4] = mBitsRatio.nMoveCoef[4];
        }
    }

    if (-1 != mWeakTextureThEnable)
    {
        alogd("en_weak_texture_th         %-10d           %-10d          ", pVideoEncData->mRcParam.mWeakTextureThEnable, mWeakTextureThEnable);
        pVideoEncData->mRcParam.mWeakTextureThEnable = mWeakTextureThEnable;
        if (0 < mWeakTextureThEnable)
        {
            alogd("weak_texture_th            %-10.2f           %-10.2f          ", pVideoEncData->mRcParam.mWeakTextureTh, mWeakTextureTh);
            pVideoEncData->mRcParam.mWeakTextureTh = mWeakTextureTh;
        }
    }

    if (-1 != mD3DInIFrmEnable)
    {
        alogd("en_d3d_in_i_frm            %-10d           %-10d          ", pVideoEncData->mD3DInIFrmEnable, mD3DInIFrmEnable);
        pVideoEncData->mD3DInIFrmEnable = mD3DInIFrmEnable;
    }
    if (-1 != mTightMbQpEnable)
    {
        alogd("en_tight_mb_qp             %-10d           %-10d          ", pVideoEncData->mTightMbQpEnable, mTightMbQpEnable);
        pVideoEncData->mTightMbQpEnable = mTightMbQpEnable;
    }

    if (-1 != en_extreme_d3d)
    {
        alogd("en_extreme_d3d             %-10d           %-10d          ", pVideoEncData->mExtremeD3D.en_extreme_d3d, mExtremeD3D.en_extreme_d3d);
        if (0 < mExtremeD3D.en_extreme_d3d)
        {
            alogd("ex_d3d_zero_mv_ratio_th    %-10.2f           %-10.2f          ", pVideoEncData->mExtremeD3D.zero_mv_ratio_th, mExtremeD3D.zero_mv_ratio_th);
            alogd("ex_d3d_enable_3d_filter    %-10d           %-10d          ", pVideoEncData->mExtremeD3D.ex_d3d_param.adjust_pix_level_enable, mExtremeD3D.ex_d3d_param.enable_3d_filter);
            alogd("ex_d3d_adjust_pix_level_en %-10d           %-10d          ", pVideoEncData->mExtremeD3D.ex_d3d_param.adjust_pix_level_enable, mExtremeD3D.ex_d3d_param.adjust_pix_level_enable);
            alogd("ex_d3d_smooth_filter_en    %-10d           %-10d          ", pVideoEncData->mExtremeD3D.ex_d3d_param.smooth_filter_enable, mExtremeD3D.ex_d3d_param.smooth_filter_enable);
            alogd("ex_d3d_max_pix_diff_th     %-10d           %-10d          ", pVideoEncData->mExtremeD3D.ex_d3d_param.max_pix_diff_th, mExtremeD3D.ex_d3d_param.max_pix_diff_th);
            alogd("ex_d3d_max_mad_th          %-10d           %-10d          ", pVideoEncData->mExtremeD3D.ex_d3d_param.max_mad_th, mExtremeD3D.ex_d3d_param.max_mad_th);
            alogd("ex_d3d_max_mv_th           %-10d           %-10d          ", pVideoEncData->mExtremeD3D.ex_d3d_param.max_mv_th, mExtremeD3D.ex_d3d_param.max_mv_th);
            alogd("ex_d3d_min_coef            %-10d           %-10d          ", pVideoEncData->mExtremeD3D.ex_d3d_param.min_coef, mExtremeD3D.ex_d3d_param.min_coef);
            alogd("ex_d3d_max_coef            %-10d           %-10d          ", pVideoEncData->mExtremeD3D.ex_d3d_param.max_coef, mExtremeD3D.ex_d3d_param.max_coef);
        }
        memcpy(&pVideoEncData->mExtremeD3D, &mExtremeD3D, sizeof(VencRegionD3DParam));
    }

    if (-1 != mRegionD3DParam.en_region_d3d)
    {
        alogd("en_region_d3d               %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.en_region_d3d, mRegionD3DParam.en_region_d3d);
        if (0 < mRegionD3DParam.en_region_d3d)
        {
            alogd("region_d3d_dis_default_para %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.dis_default_para, mRegionD3DParam.dis_default_para);
            alogd("region_d3d_result_num       %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.result_num, mRegionD3DParam.result_num);
            alogd("region_d3d_hor_region_num   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.hor_region_num, mRegionD3DParam.hor_region_num);
            alogd("region_d3d_ver_region_num   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.ver_region_num, mRegionD3DParam.ver_region_num);
            alogd("region_d3d_hor_expand_num   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.hor_expand_num, mRegionD3DParam.hor_expand_num);
            alogd("region_d3d_ver_expand_num   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.ver_expand_num, mRegionD3DParam.ver_expand_num);
            alogd("region_d3d_lv_weak_th[0]    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.lv_weak_th[0], mRegionD3DParam.lv_weak_th[0]);
            alogd("region_d3d_lv_weak_th[1]    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.lv_weak_th[1], mRegionD3DParam.lv_weak_th[1]);
            alogd("region_d3d_lv_weak_th[2]    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.lv_weak_th[2], mRegionD3DParam.lv_weak_th[2]);
            alogd("region_d3d_lv_weak_th[3]    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.lv_weak_th[3], mRegionD3DParam.lv_weak_th[3]);
            alogd("region_d3d_lv_weak_th[4]    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.lv_weak_th[4], mRegionD3DParam.lv_weak_th[4]);
            alogd("region_d3d_lv_weak_th[5]    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.lv_weak_th[5], mRegionD3DParam.lv_weak_th[5]);
            alogd("region_d3d_lv_weak_th[6]    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.lv_weak_th[6], mRegionD3DParam.lv_weak_th[6]);
            alogd("region_d3d_lv_weak_th[7]    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.lv_weak_th[7], mRegionD3DParam.lv_weak_th[7]);
            alogd("region_d3d_lv_weak_th[8]    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.lv_weak_th[8], mRegionD3DParam.lv_weak_th[8]);
            alogd("region_d3d_zero_mv_rate_th[0] %-10.2f           %-10.2f          ", pVideoEncData->mRegionD3DParam.zero_mv_rate_th[0], mRegionD3DParam.zero_mv_rate_th[0]);
            alogd("region_d3d_zero_mv_rate_th[1] %-10.2f           %-10.2f          ", pVideoEncData->mRegionD3DParam.zero_mv_rate_th[1], mRegionD3DParam.zero_mv_rate_th[1]);
            alogd("region_d3d_zero_mv_rate_th[2] %-10.2f           %-10.2f          ", pVideoEncData->mRegionD3DParam.zero_mv_rate_th[2], mRegionD3DParam.zero_mv_rate_th[2]);
            alogd("region_d3d_chroma_offset    %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.chroma_offset, mRegionD3DParam.chroma_offset);
            alogd("region_d3d_static_coef[0]   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.static_coef[0], mRegionD3DParam.static_coef[0]);
            alogd("region_d3d_static_coef[1]   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.static_coef[1], mRegionD3DParam.static_coef[1]);
            alogd("region_d3d_static_coef[2]   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.static_coef[2], mRegionD3DParam.static_coef[2]);
            alogd("region_d3d_motion_coef[0]   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.motion_coef[0], mRegionD3DParam.motion_coef[0]);
            alogd("region_d3d_motion_coef[1]   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.motion_coef[1], mRegionD3DParam.motion_coef[1]);
            alogd("region_d3d_motion_coef[2]   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.motion_coef[2], mRegionD3DParam.motion_coef[2]);
            alogd("region_d3d_motion_coef[3]   %-10d           %-10d          ", pVideoEncData->mRegionD3DParam.motion_coef[3], mRegionD3DParam.motion_coef[3]);
        }
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamRegionD3DParam, (void*)&mRegionD3DParam);
        memcpy(&pVideoEncData->mRegionD3DParam, &mRegionD3DParam, sizeof(VencRegionD3DParam));
        pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_REGIOND3D;
    }

    if (PT_H264 == mVideoEncoderFmt || PT_H265 == mVideoEncoderFmt)
    {
        if (0 < mChromaQPOffsetEnable)
        {
            alogd("chroma_qp_offset           %-10d           %-10d          ", pVideoEncData->mChromaQPOffset, mChromaQPOffset);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamChromaQPOffset, (void*)&mChromaQPOffset);
            pVideoEncData->mChromaQPOffset = mChromaQPOffset;
            pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_CHROMAQPOFFSET;
        }
    }

    if (PT_H264 == mVideoEncoderFmt)
    {
        if (0 < mH264ConstraintFlagEnable)
        {
            alogd("h264_constraint_flag_bit0  %-10d           %-10d          ", pVideoEncData->mH264ConstraintFlag.constraint_0, mH264ConstraintFlag.constraint_0);
            alogd("h264_constraint_flag_bit1  %-10d           %-10d          ", pVideoEncData->mH264ConstraintFlag.constraint_1, mH264ConstraintFlag.constraint_1);
            alogd("h264_constraint_flag_bit2  %-10d           %-10d          ", pVideoEncData->mH264ConstraintFlag.constraint_2, mH264ConstraintFlag.constraint_2);
            alogd("h264_constraint_flag_bit3  %-10d           %-10d          ", pVideoEncData->mH264ConstraintFlag.constraint_3, mH264ConstraintFlag.constraint_3);
            alogd("h264_constraint_flag_bit4  %-10d           %-10d          ", pVideoEncData->mH264ConstraintFlag.constraint_4, mH264ConstraintFlag.constraint_4);
            alogd("h264_constraint_flag_bit5  %-10d           %-10d          ", pVideoEncData->mH264ConstraintFlag.constraint_5, mH264ConstraintFlag.constraint_5);
            VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamH264ConstraintFlag, (void*)&mH264ConstraintFlag);
            memcpy(&pVideoEncData->mH264ConstraintFlag, &mH264ConstraintFlag, sizeof(VencH264ConstraintFlag));
            pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_H264CONSTRAINTFLAG;
        }
    }

    if (-1 != mVe2IspD2DLimit.en_d2d_limit)
    {
        alogd("en_d2d_limit               %-10d           %-10d          ", pVideoEncData->mVe2IspD2DLimit.en_d2d_limit, mVe2IspD2DLimit.en_d2d_limit);
        if (0 < mVe2IspD2DLimit.en_d2d_limit)
        {
            alogd("d2d_limit_d2d_level0   %-10d           %-10d          ", pVideoEncData->mVe2IspD2DLimit.d2d_level[0], mVe2IspD2DLimit.d2d_level[0]);
            alogd("d2d_limit_d2d_level1   %-10d           %-10d          ", pVideoEncData->mVe2IspD2DLimit.d2d_level[1], mVe2IspD2DLimit.d2d_level[1]);
            alogd("d2d_limit_d2d_level2   %-10d           %-10d          ", pVideoEncData->mVe2IspD2DLimit.d2d_level[2], mVe2IspD2DLimit.d2d_level[2]);
            alogd("d2d_limit_d2d_level3   %-10d           %-10d          ", pVideoEncData->mVe2IspD2DLimit.d2d_level[3], mVe2IspD2DLimit.d2d_level[3]);
            alogd("d2d_limit_d2d_level4   %-10d           %-10d          ", pVideoEncData->mVe2IspD2DLimit.d2d_level[4], mVe2IspD2DLimit.d2d_level[4]);
            alogd("d2d_limit_d2d_level5   %-10d           %-10d          ", pVideoEncData->mVe2IspD2DLimit.d2d_level[5], mVe2IspD2DLimit.d2d_level[5]);
        }
        VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamVe2IspD2DLimit, (void*)&mVe2IspD2DLimit);
        memcpy(&pVideoEncData->mVe2IspD2DLimit, &mVe2IspD2DLimit, sizeof(VencVe2IspD2DLimit));
        pVideoEncData->mbMediaDebugFlag |= MEDIA_DEBUG_VENC_CONFIG_D2DLIMIT;
    }

    alogw("**********************************************************");

    iniparser_freedict(pDict);
}
#endif

#if (MPPCFG_MUXER == OPTION_MUXER_ENABLE)
static int findMppMuxParamIndex(dictionary *pDict, int mMuxGrp)
{
    char *str = NULL;
    char paramkey[128];
    int index = -1;

    if (NULL == pDict)
    {
        aloge("fatal error, invalid params! %p", pDict);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "mux_params:mux_group_id");

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_MUX_PARAM_CNT_MAX > i))
        {
            if (mMuxGrp == (int)(atoi(strPart)))
            {
                index = i;
                alogd("mux_params index:%d, mux_group_id %d", index, mMuxGrp);
                break;
            }
            strPart = strtok(NULL, ",");
            i++;
        }
    }
    else
    {
        alogw("mux_params, key mux_group_id is not found.");
        index = -1;
    }

    return index;
}

static int findMppMuxIntParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    int paramint[MPP_MUX_PARAM_CNT_MAX];
    int result = 0;

    if ((NULL == pDict) || (MPP_MUX_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return -1;
    }

    snprintf(paramkey, sizeof(paramkey), "mux_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_MUX_PARAM_CNT_MAX > i))
        {
            paramint[i++] = atoi(strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramint[index];
        alogv("mux_params index:%d, key %s result %d", index, key, result);
    }
    else
    {
        alogv("mux_params index:%d, key %s is not found.", index, key);
        result = -1;
    }

    return result;
}

static char* findMppMuxStringParam(dictionary *pDict, const char *key, int index)
{
    char *str = NULL;
    char paramkey[128];
    char paramstr[MPP_MUX_PARAM_CNT_MAX][64];
    char *result = NULL;

    if ((NULL == pDict) || (MPP_MUX_PARAM_CNT_MAX <= index))
    {
        aloge("fatal error, invalid params! %p, %d", pDict, index);
        return NULL;
    }

    snprintf(paramkey, sizeof(paramkey), "mux_params:%s", key);

    str = (char *)iniparser_getstring(pDict, paramkey, NULL);
    if (str != NULL)
    {
        int i = 0;
        char *strPart = strtok(str, ",");
        while ((NULL != strPart) && (MPP_MUX_PARAM_CNT_MAX > i))
        {
            strcpy(paramstr[i++], strPart);
            strPart = strtok(NULL, ",");
        }
        result = paramstr[index];
        alogv("mux_params index:%d, key %s result %s", index, key, result);
    }
    else
    {
        alogv("mux_params index:%d, key %s is not found.", index, key);
        result = NULL;
    }

    return result;
}

void MediaDebugLoadMppMuxParams(RECRENDERDATATYPE *pRecRenderData, const char *pConfPath)
{
    char *ptr = NULL;
    dictionary *pDict = NULL;

    if (NULL == pRecRenderData)
    {
        aloge("fatal error, invalid params! %p", pRecRenderData);
        return;
    }

    pDict = getDictByConfPath(pConfPath);
    if (NULL == pDict)
    {
        //aloge("fatal error! get Dict By MediaDebugConfPath fail!");
        return;
    }

    int index = findMppMuxParamIndex(pDict, pRecRenderData->mMppChnInfo.mChnId);
    if (-1 == index)
    {
        alogw("mMuxGrp: %d is not found.", pRecRenderData->mMppChnInfo.mChnId);
        iniparser_freedict(pDict);
        return;
    }
    int mMuxGrp = pRecRenderData->mMppChnInfo.mChnId;
    int mVideoAttrValidNum = 1;//findMppMuxIntParam(pDict, "video_attr_valid_num", index);    

    int mVideoEncoderFmt = -1;
    ptr = (char *)findMppMuxStringParam(pDict, "video_encoder", index);
    if (ptr != NULL)
    {
        alogv("index: %d, found %s", index, ptr);
        if (!strcmp(ptr, "H264"))
        {
            mVideoEncoderFmt = PT_H264;
        }
        else if (!strcmp(ptr, "H265"))
        {
            mVideoEncoderFmt = PT_H265;
        }
        else if (!strcmp(ptr, "MJPEG"))
        {
            mVideoEncoderFmt = PT_MJPEG;
        }
        else if (!strcmp(ptr, "JPEG"))
        {
            mVideoEncoderFmt = PT_JPEG;
        }
        else
        {
            mVideoEncoderFmt = -1;
            alogw("fatal error! wrong encoder type:%s", ptr);
        }
    }
    else
    {
        mVideoEncoderFmt = -1;
    }

    int mVideoWidth = findMppMuxIntParam(pDict, "video_width", index);
    int mVideoHeight = findMppMuxIntParam(pDict, "video_height", index);
    int mVideoFrameRate = findMppMuxIntParam(pDict, "video_framerate", index);
    int mVeChn = findMppMuxIntParam(pDict, "venc_ch_id", index);

    if (pRecRenderData->mChnAttr.mVideoAttrValidNum != mVideoAttrValidNum)
    {
        alogw("update VideoAttrValidNum from %d to %d", pRecRenderData->mChnAttr.mVideoAttrValidNum, mVideoAttrValidNum);
        pRecRenderData->mChnAttr.mVideoAttrValidNum = mVideoAttrValidNum;
    }

    int mVideoInfoIndex = 0;

    alogw("**********************************************************");
    alogw("mpp mux debug mode                                        ");
    alogw("**********************************************************");
    alogw("[params_name]              [user]               [debug]   ");
    alogw("mux_group_id               %-10d           %-10d          ", pRecRenderData->mMppChnInfo.mChnId, mMuxGrp);
    if (-1 != mVideoEncoderFmt)
    {
        alogd("video_encoder              %-10d           %-10d          ", pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mVideoEncodeType, mVideoEncoderFmt);
        pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mVideoEncodeType = mVideoEncoderFmt;
    }
    if (-1 != mVideoWidth)
    {
        alogd("video_width                %-10d           %-10d          ", pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mWidth, mVideoWidth);
        pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mWidth = mVideoWidth;
    }
    if (-1 != mVideoHeight)
    {
        alogd("video_height               %-10d           %-10d          ", pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mHeight, mVideoHeight);
        pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mHeight = mVideoHeight;
    }
    if (-1 != mVideoFrameRate)
    {
        alogd("video_framerate            %-10d           %-10d          ", pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mVideoFrmRate/1000, mVideoFrameRate);
        pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mVideoFrmRate = mVideoFrameRate*1000;
    }
    if (-1 != mVeChn)
    {
        alogd("venc_ch_id                 %-10d           %-10d          ", pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mVeChn, mVeChn);
        pRecRenderData->mChnAttr.mVideoAttr[mVideoInfoIndex].mVeChn = mVeChn;
    }
    alogw("**********************************************************");

    iniparser_freedict(pDict);
}
#endif
