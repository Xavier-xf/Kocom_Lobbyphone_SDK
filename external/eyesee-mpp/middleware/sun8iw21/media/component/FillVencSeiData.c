/******************************************************************************
  Copyright (C), 2001-2023, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : FillVencSeiData.c
  Version       : Initial Draft
  Author        : Allwinner
  Created       : 2023/04/10
  Last Modified :
  Description   : fill vencoder SEI data function
  Function List :
  History       :
******************************************************************************/
#include <string.h>
#include <stdlib.h>

#include <utils/plat_log.h>
#include "FillVencSeiData.h"

#define VENC_SEI_DATA_INFO_READ_LINE_LEN_MAX        (512)

#define VENC_SEI_CONFIG_LEVEL_MASK_FOR_ISP          (0x1)
#define VENC_SEI_CONFIG_LEVEL_MASK_FOR_VIPP         (0x2)
#define VENC_SEI_CONFIG_LEVEL_MASK_FOR_VE_BASE      (0x4)
#define VENC_SEI_CONFIG_LEVEL_MASK_FOR_VE_ADVANCE   (0x8)

#define VENC_SEI_UPDATE_INTERVAL_MS_DEFAULT         (1000)

#define VENC_SEI_DATA_BUF_SIZE_FOR_ISP              (1024)
#define VENC_SEI_DATA_BUF_SIZE_FOR_VIPP             (3*1024)
#define VENC_SEI_DATA_BUF_SIZE_FOR_VE_BASE          (1024)
#define VENC_SEI_DATA_BUF_SIZE_FOR_VE_ADVANCE       (1024)

#define DEBUGFS_PATH_FOR_ISP                        "/sys/kernel/debug/mpp/isp"
#define DEBUGFS_PATH_FOR_VIPP                       "/sys/kernel/debug/mpp/vi"
#define DEBUGFS_PATH_FOR_VE_BASE                    "/sys/kernel/debug/mpp/ve_base"
#define DEBUGFS_PATH_FOR_VE_ADVANCE                 "/sys/kernel/debug/mpp/ve_advance"

#define VENC_SEI_INFO_EXE_CMD_FOR_ISP               "cat /sys/kernel/debug/mpp/isp"
#define VENC_SEI_INFO_EXE_CMD_FOR_VIPP              "cat /sys/kernel/debug/mpp/vi"
#define VENC_SEI_INFO_EXE_CMD_FOR_VE_BASE           "cat /sys/kernel/debug/mpp/ve_base"
#define VENC_SEI_INFO_EXE_CMD_FOR_VE_ADVANCE        "cat /sys/kernel/debug/mpp/ve_advance"

typedef enum VENC_SEI_DATA_SOURCE
{
    VENC_SEI_DATA_ISP_INFO,
    VENC_SEI_DATA_VIPP_INFO,
    VENC_SEI_DATA_VE_BASE_INFO,
    VENC_SEI_DATA_VE_ADVANCE_INFO,
    VENC_SEI_DATA_INFO_LAST,
} VENC_SEI_DATA_SOURCE;

static void GetExecuteCmdResult(const char *cmd, unsigned char *result, unsigned int size, unsigned int *reallen)
{
    int total_len = 0;
    char line[VENC_SEI_DATA_INFO_READ_LINE_LEN_MAX];
    *reallen = 0;

    FILE *fp = popen(cmd, "r");
    if (NULL != fp)
    {
        // The number of characters in a line should not exceed 512 bytes,
        // otherwise it cannot be obtained.
        while(fgets(line, VENC_SEI_DATA_INFO_READ_LINE_LEN_MAX, fp) != NULL)
        {
            total_len += strlen(line);
            if (total_len > (size - 1))
            {
                alogv("total len: %d is more than buf size %d, discard remain info.", total_len, size);
                break;
            }
            strncat((char*)result, line, size);
            *reallen += strlen(line);
        }
        pclose(fp);
        fp = NULL;
    }
    else
    {
        aloge("fatal error, popen %s error!", cmd);
    }
}

static int GetSeiDataInfo(VIDEOENCDATATYPE* pVideoEncData, unsigned int nSeiNum, unsigned int nBufLen, enum VENC_SEI_DATA_SOURCE eSeiDataSource)
{
    int ret = 0;

    if (VENC_SEI_DATA_NUM_MAX <= nSeiNum)
    {
        aloge("fatal error, VencChn[%d] invalid SeiNum %d >= %d", pVideoEncData->mMppChnInfo.mChnId, nSeiNum, VENC_SEI_DATA_NUM_MAX);
        return -1;
    }
    VencSeiData *pSeiData = &pVideoEncData->mSeiData[nSeiNum];

    if (NULL == pSeiData->pBuffer || 0 == pSeiData->nBufLen || nBufLen != pSeiData->nBufLen)
    {
        if (pSeiData->pBuffer)
        {
            alogd("VencChn[%d] SeiData[%d] source %d maybe buf size change %d -> %d, free old buf.", pVideoEncData->mMppChnInfo.mChnId, nSeiNum, eSeiDataSource, pSeiData->nBufLen, nBufLen);
            free(pSeiData->pBuffer);
            pSeiData->pBuffer = NULL;
            pSeiData->nBufLen = 0;
            pSeiData->nDataLen = 0;
            pSeiData->nType = 0;
        }
        // all SeiData buffer will be free in VideoEncComponentDeInit()
        pSeiData->pBuffer = malloc(nBufLen);
        if (NULL == pSeiData->pBuffer)
        {
            aloge("fatal error, VencChn[%d] malloc SeiData buffer failed! size=%d", pVideoEncData->mMppChnInfo.mChnId, nBufLen);
            return ERR_VENC_NOMEM;
        }
        alogd("VencChn[%d] SeiData[%d] source %d malloc buffer %p, size %d", pVideoEncData->mMppChnInfo.mChnId, nSeiNum, eSeiDataSource, pSeiData->pBuffer, nBufLen);
    }
    memset(pSeiData->pBuffer, 0, nBufLen);
    pSeiData->nBufLen = nBufLen;
    pSeiData->nType = 5;

    unsigned int nDataLen = 0;

    switch (eSeiDataSource)
    {
        case VENC_SEI_DATA_ISP_INFO:
        {
            if (access(DEBUGFS_PATH_FOR_ISP, F_OK | W_OK) == 0)
            {
                GetExecuteCmdResult(VENC_SEI_INFO_EXE_CMD_FOR_ISP, pSeiData->pBuffer, nBufLen, &nDataLen);
            }
            else
            {
                aloge("fatal error, VencChn[%d] %s is not found!", pVideoEncData->mMppChnInfo.mChnId, DEBUGFS_PATH_FOR_ISP);
                ret = -1;
            }
            break;
        }
        case VENC_SEI_DATA_VIPP_INFO:
        {
            if (access(DEBUGFS_PATH_FOR_VIPP, F_OK | W_OK) == 0)
            {
                GetExecuteCmdResult(VENC_SEI_INFO_EXE_CMD_FOR_VIPP, pSeiData->pBuffer, nBufLen, &nDataLen);
            }
            else
            {
                aloge("fatal error, VencChn[%d] %s is not found!", pVideoEncData->mMppChnInfo.mChnId, DEBUGFS_PATH_FOR_VIPP);
                ret = -1;
            }
            break;
        }
        case VENC_SEI_DATA_VE_BASE_INFO:
        {
            if (access(DEBUGFS_PATH_FOR_VE_BASE, F_OK | W_OK) == 0)
            {
                GetExecuteCmdResult(VENC_SEI_INFO_EXE_CMD_FOR_VE_BASE, pSeiData->pBuffer, nBufLen, &nDataLen);
            }
            else
            {
                aloge("fatal error, VencChn[%d] %s is not found!", pVideoEncData->mMppChnInfo.mChnId, DEBUGFS_PATH_FOR_VE_BASE);
                ret = -1;
            }
            break;
        }
        case VENC_SEI_DATA_VE_ADVANCE_INFO:
        {
            if (access(DEBUGFS_PATH_FOR_VE_ADVANCE, F_OK | W_OK) == 0)
            {
                GetExecuteCmdResult(VENC_SEI_INFO_EXE_CMD_FOR_VE_ADVANCE, pSeiData->pBuffer, nBufLen, &nDataLen);
            }
            else
            {
                aloge("fatal error, VencChn[%d] %s is not found!", pVideoEncData->mMppChnInfo.mChnId, DEBUGFS_PATH_FOR_VE_ADVANCE);
                ret = -1;
            }
            break;
        }
        default:
        {
            aloge("fatal error, VencChn[%d] unknown SeiDataSource %d !", pVideoEncData->mMppChnInfo.mChnId, eSeiDataSource);
            ret = -1;
            break;
        }
    }
    if (nBufLen < nDataLen)
    {
        aloge("fatal error, data len %d is more than buf size %d\n", nDataLen, nBufLen);
        nDataLen = nBufLen;
    }
    pSeiData->nDataLen = nDataLen;
    alogv("VencChn[%d] SeiData[%d] source %d buffer %p buflen %d datalen %d type %d", pVideoEncData->mMppChnInfo.mChnId, nSeiNum, eSeiDataSource, pSeiData->pBuffer, pSeiData->nBufLen, pSeiData->nDataLen, pSeiData->nType);

    alogv("VencChn[%d] SeiData[%d] source %d buffer data:\n%s\n", pVideoEncData->mMppChnInfo.mChnId, nSeiNum, eSeiDataSource, pSeiData->pBuffer);

    return ret;
}

void FreeSeiDataBuffer(VIDEOENCDATATYPE* pVideoEncData)
{
    for (int i = 0; i < VENC_SEI_DATA_NUM_MAX; i++)
    {
        if (pVideoEncData->mSeiData[i].pBuffer)
        {
            alogd("VencChn[%d] free SeiData[%d] buffer %p", pVideoEncData->mMppChnInfo.mChnId, i, pVideoEncData->mSeiData[i].pBuffer);
            free(pVideoEncData->mSeiData[i].pBuffer);
            pVideoEncData->mSeiData[i].pBuffer = NULL;
            pVideoEncData->mSeiData[i].nBufLen = 0;
            pVideoEncData->mSeiData[i].nDataLen = 0;
            pVideoEncData->mSeiData[i].nType = 0;
        }
    }
}

extern int MPP_GLOBAL_VENC_SEI_CONFIG_LEVEL;
extern int MPP_GLOBAL_VENC_SEI_UPDATE_INTERVAL;
extern int MPP_GLOBAL_VENC_SEI_DATA_BUFFER_SIZE;

/**
  environment variable:
  /tmp/mpp_venc_sei_config_param
  param format: level,update_interval,sei_data_buf_size
  [level]
    range is [0,15]
    Independent SEI
    0 :disable all
    1 :isp
    2 :vipp
    4 :ve_base
    8 :ve_advance
    Combination SEI:
    3 :isp + vipp
    5 :isp + ve_base
    6 :vipp + ve_base
    7 :isp + vipp + ve_base
    9 :isp + ve_advance
    10:vipp + ve_advance
    11:isp + vipp + ve_advance
    12:ve_base + ve_advance
    13:isp + ve_base + ve_advance
    14:vipp + ve_base + ve_advance
    15:isp + vipp + ve_base + ve_advance
  [update_interval]
    unit is ms, default is 1000ms.
  [sei_data_buf_size]
    range is [0,1048576], default is for every sei define.
*/
int FillSeiDataToVencLib(VIDEOENCDATATYPE* pVideoEncData)
{
    unsigned char isp_flag = 0;
    unsigned char vipp_flag = 0;
    unsigned char ve_base_flag = 0;
    unsigned char ve_advance_flag = 0;
    VencSeiData *pSeiData = NULL;
    enum VENC_SEI_DATA_SOURCE eSeiDataSource = VENC_SEI_DATA_INFO_LAST;
    int usr_sei_config_level = MPP_GLOBAL_VENC_SEI_CONFIG_LEVEL;
    int usr_sei_update_interval = MPP_GLOBAL_VENC_SEI_UPDATE_INTERVAL;
    int usr_sei_data_buf_size = MPP_GLOBAL_VENC_SEI_DATA_BUFFER_SIZE;
    unsigned int SeiUpdateInterval = 0;
    unsigned int SeiDataBufferSize = 0;
    unsigned int SeiNum = 0;
    uint64_t mCurTimeMs = 0;
    int ret = 0;

    if (0 == usr_sei_config_level)
    {
        FreeSeiDataBuffer(pVideoEncData);
        return 0;
    }

    if ((NULL == pVideoEncData->pCedarV) || (0 == pVideoEncData->mbCedarvVideoEncInitFlag))
    {
        alogd("fatal error, VE is not ready! %p, %d", pVideoEncData->pCedarV, pVideoEncData->mbCedarvVideoEncInitFlag);
        return 0;
    }

    if (0 == pVideoEncData->mOutputStreamCnt)
    {
        return 0;
    }

    if (usr_sei_config_level != pVideoEncData->mLastSeiDataConfigLevel)
    {
        FreeSeiDataBuffer(pVideoEncData);
        pVideoEncData->mLastSeiDataConfigLevel = usr_sei_config_level;
    }

    if (0 < usr_sei_update_interval)
    {
        SeiUpdateInterval = usr_sei_update_interval;
    }
    else
    {
        SeiUpdateInterval = VENC_SEI_UPDATE_INTERVAL_MS_DEFAULT;
    }

    mCurTimeMs = GetSysTimeByUsec() / 1000;
    if (0 == pVideoEncData->mLastSetSeiDataTimeMs)
    {
        pVideoEncData->mLastSetSeiDataTimeMs = mCurTimeMs;
    }
    else
    {
        if (pVideoEncData->mLastSetSeiDataTimeMs + SeiUpdateInterval > mCurTimeMs)
        {
            return 0;
        }
    }
    alogv("VencChn[%d] set sei data time(ms) last %llu cur %llu", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mLastSetSeiDataTimeMs, mCurTimeMs);
    pVideoEncData->mLastSetSeiDataTimeMs = mCurTimeMs;

    if (PT_H264 == pVideoEncData->mEncChnAttr.VeAttr.Type || PT_H265 == pVideoEncData->mEncChnAttr.VeAttr.Type)
    {
        alogv("VencChn[%d] usr config level: 0x%x, %d(ms), %d", pVideoEncData->mMppChnInfo.mChnId, usr_sei_config_level, usr_sei_update_interval, usr_sei_data_buf_size);
        isp_flag = usr_sei_config_level & VENC_SEI_CONFIG_LEVEL_MASK_FOR_ISP;
        vipp_flag = usr_sei_config_level & VENC_SEI_CONFIG_LEVEL_MASK_FOR_VIPP;
        ve_base_flag = usr_sei_config_level & VENC_SEI_CONFIG_LEVEL_MASK_FOR_VE_BASE;
        ve_advance_flag = usr_sei_config_level & VENC_SEI_CONFIG_LEVEL_MASK_FOR_VE_ADVANCE;

        if (isp_flag)
        {
            SeiDataBufferSize = VENC_SEI_DATA_BUF_SIZE_FOR_ISP;
            if (0 < usr_sei_data_buf_size)
            {
                SeiDataBufferSize = usr_sei_data_buf_size;
            }
            ret = GetSeiDataInfo(pVideoEncData, SeiNum, SeiDataBufferSize, VENC_SEI_DATA_ISP_INFO);
            if (ret != 0)
            {
                aloge("fatal error! VencChn[%d] get SeiData[%d] failed! ret=%d", pVideoEncData->mMppChnInfo.mChnId, SeiNum, ret);
                goto exit;
            }
            SeiNum += 1;
        }
        if (vipp_flag)
        {
            SeiDataBufferSize = VENC_SEI_DATA_BUF_SIZE_FOR_VIPP;
            if (0 < usr_sei_data_buf_size)
            {
                SeiDataBufferSize = usr_sei_data_buf_size;
            }
            ret = GetSeiDataInfo(pVideoEncData, SeiNum, SeiDataBufferSize, VENC_SEI_DATA_VIPP_INFO);
            if (ret != 0)
            {
                aloge("fatal error! VencChn[%d] get SeiData[%d] failed! ret=%d", pVideoEncData->mMppChnInfo.mChnId, SeiNum, ret);
                goto exit;
            }
            SeiNum += 1;
        }
        if (ve_base_flag)
        {
            SeiDataBufferSize = VENC_SEI_DATA_BUF_SIZE_FOR_VE_BASE;
            if (0 < usr_sei_data_buf_size)
            {
                SeiDataBufferSize = usr_sei_data_buf_size;
            }
            ret = GetSeiDataInfo(pVideoEncData, SeiNum, SeiDataBufferSize, VENC_SEI_DATA_VE_BASE_INFO);
            if (ret != 0)
            {
                aloge("fatal error! VencChn[%d] get SeiData[%d] failed! ret=%d", pVideoEncData->mMppChnInfo.mChnId, SeiNum, ret);
                goto exit;
            }
            SeiNum += 1;
        }
        if (ve_advance_flag)
        {
            SeiDataBufferSize = VENC_SEI_DATA_BUF_SIZE_FOR_VE_ADVANCE;
            if (0 < usr_sei_data_buf_size)
            {
                SeiDataBufferSize = usr_sei_data_buf_size;
            }
            ret = GetSeiDataInfo(pVideoEncData, SeiNum, SeiDataBufferSize, VENC_SEI_DATA_VE_ADVANCE_INFO);
            if (ret != 0)
            {
                aloge("fatal error! VencChn[%d] get SeiData[%d] failed! ret=%d", pVideoEncData->mMppChnInfo.mChnId, SeiNum, ret);
                goto exit;
            }
            SeiNum += 1;
        }

        VencSeiParam stSeiParam;
        memset(&stSeiParam, 0, sizeof(VencSeiParam));
        stSeiParam.nSeiNum = SeiNum;
        stSeiParam.pSeiData = &pVideoEncData->mSeiData[0];
        alogv("VencChn[%d] set SeiNum:%d, SeiData:%p", pVideoEncData->mMppChnInfo.mChnId, stSeiParam.nSeiNum, stSeiParam.pSeiData);
        ret = VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamSeiParam, (void*)&stSeiParam);
        if (ret != 0)
        {
            aloge("fatal error! VencChn[%d] set sei param failed! ret=%d", pVideoEncData->mMppChnInfo.mChnId, ret);
            goto exit;
        }
        alogv("VencChn[%d] set SeiParam success", pVideoEncData->mMppChnInfo.mChnId);
    }
    else
    {
        alogw("fatal error! VencChn[%d] VeType %d does not support this api, only H264/H265 supports it.", pVideoEncData->mMppChnInfo.mChnId, pVideoEncData->mEncChnAttr.VeAttr.Type);
        return ERR_VENC_ILLEGAL_PARAM;
    }

    return 0;

exit:
    FreeSeiDataBuffer(pVideoEncData);
    return -1;
}
