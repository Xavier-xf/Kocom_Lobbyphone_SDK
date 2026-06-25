/*
* Copyright (c) 2008-2022 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : veCoreScheduler.c
* Description :
* History :
*   Author  : wangxiwang <wangxiwang@allwinnertech.com>
*   Date    : 2022/07/13
*   Comment :
*/
/* Notice: It's video engine driver API, Don't modify it in user space. */
#ifndef _VE_CORE_SCHEDULER_H_
#define _VE_CORE_SCHEDULER_H_

#include "vencoder.h"
#include "FrameBufferManager.h"
#include "CdcUtil.h"
//#include "venc_common.h"
// #include "h264enc.h"

#define ONLINE_SHOULD_WAIT -4

typedef void* VeCoreScheduler;

typedef struct VcsChannelRegConfNode VcsChannelRegConfNode;

typedef struct {
    unsigned output_read_info  : 1;  //*[0]
    unsigned reset_ve_core     : 1;  //*[1]
    unsigned revered           : 30; //*[2:31]
}VcuReg_FrameCtrlCfg;

typedef struct VcuFinishGroupStatus {
    unsigned int     finishGroupId;
    unsigned int     finishFrameNum;
    unsigned int     gotTotalFrameNum;
    unsigned int     groupResult;
	long long        int_start;
	long long        int_end;
    long long        int_time;
    unsigned int     ve_status;
	unsigned int     currCtuX;
	unsigned int     currCtuY;
    VENC_RESULT_TYPE enc_result;
} VcuFinishGroupStatus;

//* lenght of the struct is 4*8 byte
//* donot change the order of number
typedef struct {
    unsigned int        input_cfg_msb_addr;
    unsigned int        input_cfg_lsb_addr;
    unsigned int        input_cfg_size;//*(32bit word), buffer must 64byte-align
    VcuReg_FrameCtrlCfg frame_ctrl_cfg;
    unsigned int        output_info_msb_addr;
    unsigned int        output_info_lsb_addr;
    unsigned int        reserverd;
    unsigned int        frame_limit_cycle;
}VcuRegFrameConfInfo;

typedef struct VcsRegFrameBufInfo
{
    unsigned char* pInBufVirAddr;
    unsigned int   nInBufPhyAddr;
    unsigned int   nInBufSize;

    unsigned char* pInBufVirAddrTmp;
    unsigned int   nInBufSizeTmp;

    unsigned char* pOutBufVirAddr;
    unsigned int   nOutBufPhyAddr;
    unsigned int   nOutBufSize;
}VcsRegFrameBufInfo;

#define VCS_MAX_UPDATE_REG_CONF_NUM (2)

typedef enum {
	VCU_P_SLICE = 0,
	VCU_B_SLICE = 1,
	VCU_I_SLICE = 2,
} VCU_CODE_TYPE;

typedef struct VcsUpdateRegConfInfo
{
    unsigned char*      pRegConfBuf;
    unsigned int        nRegConfBufSize;
    unsigned int        nFrameLimitCycle;
    VcuReg_FrameCtrlCfg mFrameCtrlCfg;
    VCU_CODE_TYPE       nCodingType;   //* for h264/h265, I_SLICE or P_SLICE
    int                 bIsMainFrame;  //* for jpeg, 1: main frame, 0: thumb frame;
    int                 bHadThumbFrameRegConf; //* for jpeg, 1: continue encode thumb Frame after encode main frame
}VcsUpdateRegConfInfo;

typedef struct VcsUpdateRegConfParam
{
    VcsUpdateRegConfInfo mUpdateInfo[VCS_MAX_UPDATE_REG_CONF_NUM];
    unsigned int         nUpdateNum;
}VcsUpdateRegConfParam;

typedef struct VcsChannelRegConf
{
    unsigned int channelId;
    unsigned int nCodecType;//* VENC_CODEC_JPEG, VENC_CODEC_H264_VER2, VENC_CODEC_H265
    unsigned int nChannel;
    unsigned int sensor_id;
    unsigned int bk_id;
    int          bOnlineChannel;
    void        *ispCtx;
    void        *pIspinfo;

    unsigned int        bHoldByVeHw;
             int        nRegUpdateCnt;
             int        bReUseRegConf; //* use the same frame-reg-conf
    VCU_CODE_TYPE       nCodingType;   //* for h264/h265, I_SLICE or P_SLICE
             int        bIsMainFrame;  //* for jpeg, 1: main frame, 0: thumb frame;
             int        bHadThumbFrameRegConf; //* for jpeg, 1: continue encode thumb Frame after encode main frame

    VcsRegFrameBufInfo  mBufInfo;
    VcuRegFrameConfInfo mFrameConf;
    void*               pChannelCxt;
    VencInputBuffer    *pInputBuffer;
    unsigned int enc_sram_val[8];
    int                 bValidFlag;
}VcsChannelRegConf;

struct VcsChannelRegConfNode
{
    VcsChannelRegConf         chRegConf;
    VcsChannelRegConfNode*    next;
};

typedef struct VcsRegConfGroup
{
    VcsChannelRegConfNode *groupQueue;

    unsigned int           groupBufPhyAddr; //* palloc no cache phy buffer
    unsigned char*         groupBufVirAddr;
    unsigned int           groupBufSize;

    unsigned int           nFrameConfNum;
             int           veCoreId;
             int           groupId;
    unsigned int           bHoldByHwFlag; //* 1: the group had set to hardware(ve-core)

    unsigned int           bOnlineS0B0Enable;
    unsigned int           bOnlineS0B1Enable;
    unsigned int           bOnlineS1B0Enable;
    unsigned int           bOnlineS1B1Enable;
    unsigned int           bOnlineChannel;
}VcsRegConfGroup;


typedef struct EncodeDoneInfo
{
    VcsChannelRegConf *pRegConf;
    VencInputBuffer   *pInputBuffer;
    unsigned int       bFrameErrorFlag;
    unsigned int       bFrameDropFlag;
	long long          int_start;
	long long          int_end;
    long long          int_time;
	unsigned int stm_len;
    VENC_RESULT_TYPE enc_result;
    unsigned long vcs_out_encpp_addr;
    unsigned long vcs_out_enc_addr;
} EncodeDoneInfo;

struct VcsCreateInfo {
    unsigned int bOnlineChannel;
    unsigned int bVcuAutoMode;
    // unsigned int nFrameNumInGroup;
    unsigned int nCodecType;//* VENC_CODEC_JPEG, VENC_CODEC_H264_VER2, VENC_CODEC_H265
    unsigned int nChannel;
    unsigned int sensor_id;
    unsigned int bk_id;
    unsigned int bEnableMultiOnlineSensor;
    unsigned int bEnableImageStitching;
    void *ispCtx;
    void *pIspinfo;
	unsigned int *pTotalCpuBufSize;
	unsigned int *pTotalIonBufSize;
    unsigned int extend_flag;
    int frameRate;
};

typedef struct VcsCbTypeEnc
{
    int (*inputbufferDone)(
        void* pPrivateData,
        int   nResult,
        VencInputBuffer   *pInputbuffer);

    int (*checkIsReadyToDoEncode)(
        void* pPrivateData);

} VcsCbTypeEnc;

typedef struct VcsCbTypeSubEnc
{
    int (*checkBitstreamBufIsFull)(
        void* pPrivateData);

    int (*updateEncodeParamImmediately)(
        void* pPrivateData,
        VcsChannelRegConf *pRegConf,
        VencInputBuffer   *pInputbuffer);

    int (*encodeDone)(
        void* pPrivateData,
        EncodeDoneInfo* pEncodeDoneInfo);

} VcsCbTypeSubEnc;

VeCoreScheduler* vcsCreate(struct VcsCreateInfo *info);
int vcsInit(VeCoreScheduler* pVcs);
int vcsStart(VeCoreScheduler* pVcs, int bStart);
int vcsDestroy(VeCoreScheduler* pVcs);
int vcsUpdateRegConfToUse(VeCoreScheduler* pVcs, VcsChannelRegConf *pRegConf,
                       unsigned char* pReplaceRegConfBuf, unsigned int nReplaceRegConfBufSize);
//* ���������������¼Ĵ������ú����Ϸ��ش˺�������
int vcsUpdateRegConfToTmp(VeCoreScheduler* pVcs, VcsUpdateRegConfParam *pRegConfParam);
int vcsSetCallbackEnc(VeCoreScheduler* pVcs, VcsCbTypeEnc* pCallback, void* pPrivateData);

int vcsSetCallbackSubEnc(VeCoreScheduler* pVcs, VcsCbTypeSubEnc* pCallback, void* pPrivateData);

//*�������߱��룺 ���� input framebufferģ��Ĺ��������vcs��vcsÿ�ν��б����ǣ��������߱��룬��Ҫ��ѯ�Ƿ���
//*�µ� input framebuffer������У��Ž��б���
int vcsSetFbm(VeCoreScheduler* pVcs, FrameBufferManager* pFBM);

int vcsSetFrameRegBufSize(VeCoreScheduler* pVcs, unsigned int nInFrameRegBufSize, unsigned int nOutFrameRegBufSize);

int vcsNotifyReadyToEncode(VeCoreScheduler* pVcs);

#endif

