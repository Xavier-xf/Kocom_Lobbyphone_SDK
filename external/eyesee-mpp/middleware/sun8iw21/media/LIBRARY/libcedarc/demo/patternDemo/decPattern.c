#include "patternDemo.h"

#define DECODER_FQ (400)

typedef enum DecPatType {
    MJPEG_DEC_PAT            = 0x101,
    MPEG1_DEC_PAT            = 0x102,
    MPEG2_DEC_PAT            = 0x103,
    MPEG4_DEC_PAT            = 0x104,
    MSMPEG4V1_DEC_PAT        = 0x105,
    MSMPEG4V2_DEC_PAT        = 0x106,
    DIVX3_DEC_PAT            = 0x107,
    DIVX4_DEC_PAT            = 0x108,
    DIVX5_DEC_PAT            = 0x109,
    XVID_DEC_PAT             = 0x10a,
    H263_DEC_PAT             = 0x10b,
    SORENSSON_H263_DEC_PAT   = 0x10c,
    RXG2_DEC_PAT             = 0x10d,
    WMV1_DEC_PAT             = 0x10e,
    WMV2_DEC_PAT             = 0x10f,
    WMV3_DEC_PAT             = 0x110,
    VP6_DEC_PAT              = 0x111,
    VP8_DEC_PAT              = 0x112,
    VP9_DEC_PAT              = 0x113,
    RX_DEC_PAT               = 0x114,
    H264_DEC_PAT             = 0x115,
    H265_DEC_PAT             = 0x116,
    AVS_DEC_PAT              = 0x117,
    AVS2_DEC_PAT             = 0x118,
    MAX_DEC_PAT_TYPE         = 0x119
} DecPatType;

typedef struct DecPatCtx {
    MEMOPS_STRUCT   *memops;
    VeOpsS*            veOpsS;
    void*              pVeOpsSelf;

    DecPatType         eDecType;

    PatIntType         eCheckIntType;

    unsigned int       nVeFq;

    unsigned long      nBaseAddr;

    FILE               *fpPattern;
    char               pPatternLine[256];
    char               pPatternPath[256];
} DecPatCtx;

void* DecPatOpen(void* pBaseConfig)
{
    VeBaseCfg *baseCfg = (VeBaseCfg*)pBaseConfig;
    DecPatCtx *patCtx = NULL;

    if (NULL == (patCtx = (DecPatCtx*)MALLOC(sizeof(DecPatCtx))))
    {
        loge("Create DecPatCtx failed!");
        return NULL;
    }

    memset(patCtx, 0, sizeof(DecPatCtx));

    patCtx->veOpsS = baseCfg->veOpsS;
    patCtx->pVeOpsSelf = baseCfg->pVeOpsSelf;
    patCtx->memops = baseCfg->memops;

    patCtx->nBaseAddr = (unsigned long)CdcVeGetGroupRegAddr(baseCfg->veOpsS, baseCfg->pVeOpsSelf, REG_GROUP_VETOP);
    return (void*)patCtx;
}

int DecPatUnInit(void* handle)
{
    int cnt;
    DecPatCtx *patCtx = (DecPatCtx*)handle;

    FCLOSE(patCtx->fpPattern);

    logw("Finish PatternUnInit");
    return 0;
}

int DecPatInit(void* handle)
{
    int ret = 0;
    char path[256] = {0};
    DecPatCtx *patCtx = (DecPatCtx*)handle;

    sprintf(path, "%s/aw_pattern.txt", patCtx->pPatternPath);
    if(NULL == (patCtx->fpPattern = fopen(path, "r")))
    {
        loge("%s fopen failed!", path);
        ret = -1;
        goto PATTERN_INIT_FAILED;
    }

PATTERN_INIT_FAILED:
    if(ret)
    {
        EncPatUnInit(patCtx);
    }
    else
    {
        logw("Finish PatternInit");
    }

    return ret;
}

void DecPatClose(void* handle)
{
    DecPatCtx *patCtx = (DecPatCtx*)handle;

    DecPatUnInit(patCtx);
    FREE(patCtx);

    logw("Finish PatternEncClose");
}

int DecPatWork(void *handle)
{
    DecPatCtx *patCtx = (DecPatCtx*)handle;
    int ret = 0;

    CdcVeReset(patCtx->veOpsS, patCtx->pVeOpsSelf);
    CdcVeEnableVe(patCtx->veOpsS, patCtx->pVeOpsSelf);


    CdcVeReset(patCtx->veOpsS, patCtx->pVeOpsSelf);
    CdcVeDisableVe(patCtx->veOpsS,patCtx->pVeOpsSelf);

    logw("Finish PatternEncEncode");
ENCODE_FAILED:

    return ret;
}

int DecPatGetParameter(void *handle, int indexType, void* param)
{
    return 0;
}

int DecPatSetParameter(void *handle, int indexType, void* param)
{
    int ret = 0;
    DecPatCtx *patCtx = (DecPatCtx*)handle;

    switch(indexType)
    {
        case PAT_IndexParamWorkPath:
        {
            char *path = (char*)param;
            sprintf(patCtx->pPatternPath, "%s", path);
            break;
        }
        case PAT_IndexParamCheckIntType:
        {
            patCtx->eCheckIntType = *((int *)param);
            break;
        }
        case PAT_IndexParamFq:
        {
            patCtx->nVeFq = *((int *)param);
            break;
        }
        default:
        {
            logw("pattern do not support this %d indexType!", indexType);
            ret = -1;
            break;
        }
    }
    return ret;
}


PatWorkDev g_dec_pattern_device = {
    .codecType              = "decoder pattern",
    .open                   = DecPatOpen,
    .init                   = DecPatInit,
    .uninit                 = DecPatUnInit,
    .close                  = DecPatClose,
    .work                   = DecPatWork,
    .GetParameter           = DecPatGetParameter,
    .SetParameter           = DecPatSetParameter,
};

