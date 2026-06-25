#include "patternDemo.h"

extern PatWorkDev g_enc_pattern_device;
extern PatWorkDev g_dec_pattern_device;

int vePatCreat(PatCtx *patCtx)
{
    VeBaseCfg *baseCfg = NULL;
    VeConfig veCfg = {0};
    int ret = 0;

    if(NULL == (baseCfg = (VeBaseCfg*)MALLOC(sizeof(VeBaseCfg))))
    {
        loge("VeBaseCfg malloc failed!");
        ret = -1;
        goto VE_PAT_CREAT_FAILED;
    }
    memset(baseCfg, 0, sizeof(VeBaseCfg));

    baseCfg->memops = MemAdapterGetOpsS();
    if(CdcMemOpen(baseCfg->memops))
    {
        loge("Set up memory runtime environment failed!");
        ret = -2;
        goto VE_PAT_CREAT_FAILED;
    }
    veCfg.nEnableAfbcFlag = 0;
    if(patCtx->ePatType == ENC_PATTERN)
    {
        veCfg.nDecoderFlag = 0;
        veCfg.nEncoderFlag = 1;
        veCfg.nResetVeMode = RESET_VE_NORMAL;
        memcpy(&patCtx->sPatDev, &g_enc_pattern_device, sizeof(PatWorkDev));
    }
    else
    {
        veCfg.nDecoderFlag = 1;
        veCfg.nEncoderFlag = 0;
        veCfg.nResetVeMode = RESET_VE_SPECIAL;
        memcpy(&patCtx->sPatDev, &g_dec_pattern_device, sizeof(PatWorkDev));
    }

    if(NULL == (baseCfg->veOpsS = GetVeOpsS(VE_OPS_TYPE_NORMAL)))
    {
        loge("veOpsS get failed!");
        ret = -3;
        goto VE_PAT_CREAT_FAILED;
    }

    if(NULL == (baseCfg->pVeOpsSelf = CdcVeInit(baseCfg->veOpsS, &veCfg)))
    {
        loge("pVeOpsSelf init failed!");
        ret = -4;
        goto VE_PAT_CREAT_FAILED;
    }

    if(NULL == (patCtx->pPatHandle = patCtx->sPatDev.open(baseCfg)))
    {
        loge("pPatHandle open failed!");
        ret = -5;
        goto VE_PAT_CREAT_FAILED;
    }

VE_PAT_CREAT_FAILED:
    if(ret)
    {
        if(baseCfg)
        {
            if(baseCfg->veOpsS)
                CdcVeRelease(baseCfg->veOpsS, baseCfg->pVeOpsSelf);
            if(baseCfg->memops)
                CdcMemClose(baseCfg->memops);
            FREE(baseCfg);
        }
    }

    patCtx->pBaseCfg = baseCfg;

    return 0;
}

void vePatDestroy(PatCtx *patCtx)
{
    VeBaseCfg *baseCfg = (VeBaseCfg*)patCtx->pBaseCfg;

    patCtx->sPatDev.close(patCtx->pPatHandle);

    CdcVeRelease(baseCfg->veOpsS, baseCfg->pVeOpsSelf);

    CdcMemClose(baseCfg->memops);

    FREE(patCtx->pBaseCfg);

    free(patCtx);
}

PatCtx* PatCreat(PatType patType)
{
    PatCtx *patCtx = NULL;

    if(patType >= MAX_PAT_TYPE)
    {
        loge("patType(%d) is too larger!", patType);
        return NULL;
    }

    if(NULL == (patCtx = (PatCtx*)MALLOC(sizeof(PatCtx))))
    {
        loge("patCtx malloc failed!");
        return NULL;
    }
    memset(patCtx, 0, sizeof(PatCtx));

    patCtx->ePatType = patType;

    if(ENC_PATTERN <= patCtx->ePatType && patCtx->ePatType <= DEC_PATTERN)
    {
        if(vePatCreat(patCtx))
        {
            FREE(patCtx);
            return NULL;
        }
    }

    return patCtx;
}

int PatInit(PatCtx *patCtx)
{
    int ret = 0;

    ret = patCtx->sPatDev.init(patCtx->pPatHandle);

    return ret;
}

int PatWork(PatCtx *patCtx)
{
    int ret = 0;

    ret = patCtx->sPatDev.work(patCtx->pPatHandle);

    return ret;
}

void PatDestroy(PatCtx *patCtx)
{
    if(patCtx == NULL)
    {
        return;
    }

    if(ENC_PATTERN <= patCtx->ePatType && patCtx->ePatType <= DEC_PATTERN)
    {
        vePatDestroy(patCtx);
    }
}

int PatGetParameter(PatCtx *patCtx, PatIdxType indexType, void* param)
{
    int ret = 0;

    if(patCtx->ePatType < ENC_PATTERN || DEC_PATTERN < patCtx->ePatType)
    {
        logw("PatType(%d) is out of the supported range!", patCtx->ePatType);
        return -1;
    }

    if(indexType < MIN_PAT_PARAM_IDX || MAX_PAT_PARAM_IDX < indexType)
    {
        logw("IndexType(%d) is out of the supported range!", indexType);
        return -2;
    }

    ret = patCtx->sPatDev.GetParameter(patCtx->pPatHandle, indexType, param);

    return ret;
}

int PatSetParameter(PatCtx *patCtx, PatIdxType indexType, void* param)
{
    int ret = 0;

    if(patCtx->ePatType < ENC_PATTERN || DEC_PATTERN < patCtx->ePatType)
    {
        logw("PatType(%d) is out of the supported range!", patCtx->ePatType);
        return -1;
    }

    if(indexType < MIN_PAT_PARAM_IDX || MAX_PAT_PARAM_IDX < indexType)
    {
        logw("IndexType(%d) is out of the supported range!", indexType);
        return -2;
    }

    ret = patCtx->sPatDev.SetParameter(patCtx->pPatHandle, indexType, param);

    return ret;
}


void PrintfArgument()
{
    logw("Thu number of Arguments should be 4!");
    logw("argv[1]: pattern path;");
    logw("argv[2]: pattern type;");
    logw("argv[3]: pattern interrupt type;");
}

int main(int argc, char** argv)
{
    int ret = 0;
    int nPatType = ENC_PATTERN;
    int nPatIntType = 0;
    PatCtx *pPatCtx = NULL;

    if(argc != 4)
    {
        PrintfArgument();
        ret = -1;
        goto MAIN_END;
    }

    logd("PatternPath:%s", argv[1]);

    nPatType = CLIP3(MIN_PAT_TYPE, MAX_PAT_TYPE, atoi(argv[2]));
    logd("PatType:%d", nPatType);

    nPatIntType = CLIP3(MIN_INT_TYPE, MAX_PAT_TYPE, atoi(argv[3]));
    logd("PatIntType:%d", nPatIntType);

    if(NULL == (pPatCtx = PatCreat(nPatType)))
    {
        loge("PatCreat failed!");
        ret = -2;
        goto MAIN_END;
    }

    PatSetParameter(pPatCtx, PAT_IndexParamWorkPath, argv[1]);
    PatSetParameter(pPatCtx, PAT_IndexParamCheckIntType, &nPatIntType);

    if(PatInit(pPatCtx))
    {
        loge("PatInit failed!");
        ret = -3;
        goto MAIN_END;
    }

    if(PatWork(pPatCtx))
    {
        loge("PatWork failed!");
        ret = -4;
        goto MAIN_END;
    }


MAIN_END:

    PatDestroy(pPatCtx);

    return ret;
}

