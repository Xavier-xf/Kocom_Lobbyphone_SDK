#include "patternDemo.h"

#define ENCODER_FQ (400)

typedef enum EncPatType {
    H264_ENC_PAT,
    JPEG_ENC_PAT,
    VP8_ENC_PAT,
    H265_ENC_PAT,
    ENCPP_PAT,
} EncPatType;

typedef struct EncPatFileList {
    unsigned int  nFileIdx;
    unsigned int  bIsSram;
    unsigned int  nRegIdx;
    unsigned int  nRshBits;
    char          pName[64];
} EncPatFileList;

typedef struct EncPatIonBuf {
    unsigned int  bIsSram;
    unsigned int  nRegIdx;
    unsigned int  nRshBits;
    unsigned int  nDataLen;
    unsigned int  nBufLen;
    unsigned long nPhyAddr;
    unsigned char *pVirAddr;
    unsigned char *pCmpAddr;
    char          pName[64];
} EncPatIonBuf;

typedef struct EncPatCtx {
    MEMOPS_STRUCT   *memops;
    VeOpsS*            veOpsS;
    void*              pVeOpsSelf;

    EncPatType         eEncType;

    unsigned int       nSrcWidth;
    unsigned int       nSrcHeight;
    unsigned int       nDstWidth;
    unsigned int       nDstHeight;
    unsigned int       nScHorRatio;
    unsigned int       nScVerRatio;
    unsigned int       nRotAngle;
    unsigned int       bOverlay;
    unsigned int       bScaler;

    unsigned int       nDstWidth16Align;
    unsigned int       nDstHeight16Align;
    unsigned int       nDstWidthMb;
    unsigned int       nDstHeightMb;

    unsigned long      nBaseAddr;

    unsigned int       nBsBufLen;
    unsigned long      nBsBgnPhyAddr;
    unsigned long      nBsEndPhyAddr;
    unsigned char      *pBsBgnVirAddr;
    unsigned long      nBsOffset;
    unsigned long      nBsValidSize;

    unsigned int       nOlInvertFalgBufLen;
    unsigned long      nOlInvertFlagPhyAddr;
    unsigned char      *pOlInvertFlagVirAddr;

    unsigned int       nLineRcBufLen;
    unsigned long      nLineRcPhyAddr;
    unsigned char      *pLineRcVirAddr;

    unsigned int       nMbInfoBufLen;
    unsigned long      nMbInfoPhyAddr;
    unsigned char      *pMbInfoVirAddr;

    unsigned int       nDeblockBufLen;
    unsigned long      nDeblockPhyAddr;
    unsigned char      *pDeblockVirAddr;

    int                nDumpInBufNum;
    EncPatIonBuf       *pDumpInList;

    int                nDumpOutBufNum;
    EncPatIonBuf       *pDumpOutList;

    PatIntType         eCheckIntType;

    unsigned int       nVeFq;

    FILE               *fpPattern;
    char               pPatternLine[256];
    char               pPatternPath[256];
} EncPatCtx;

static const EncPatFileList gEncDumpInFileList[] =
{
    { 0x0F, 0,    0xBFC,  8, "sw_qp_info.dat" },
    { 0x16, 0,    0xA78,  0, "sw_frame_luma.dat" },
    { 0x17, 0,    0xA7C,  0, "sw_frame_cb.dat" },
    { 0x18, 0,    0xA80,  0, "sw_frame_cr.dat" },
    { 0x22, 0xBE0, 0x408, 8, "sw_frame7_y.dat" },           // ref_qsub_data
    { 0x23, 0xBE0, 0x40C, 8, "sw_frame7_c.dat" },           // ref_qsub_info
    { 0x25, 0xBE0, 0x410, 8, "sw_frame8_c.dat" },           // ref
    { 0x5D, 0xBE0, 0x418, 8, "sw_3d_coef_in.dat" },
    { 0x86, 0,    0xA44,  8, "sw_ol_link.dat" },
    { 0x88, 0,    0xA48,  8, "sw_ol_data.dat" },
    { 0x8C, 0,    0xBBC,  8, "sw_ctu_rc_coef_input.dat" },
    { 0xB7, 0,    0xBFC,  8, "sw_ctumode_forceset_input.dat" },
};

static const EncPatFileList gEncDumpOutFileList[] =
{
    { 0x00, 0,    0xB80,  8, "sw_bitstream.dat" },
    { 0x20, 0xBE0, 0x400, 8, "sw_frame6_y.dat" },           // rec_qsub_data
    { 0x21, 0xBE0, 0x404, 8, "sw_frame6_c.dat" },           // rec_qsub_info
    { 0x24, 0xBE0, 0x414, 8, "sw_frame8_y.dat" },           // rec
    { 0x36, 0,    0xB60, 8, "sw_frame17_y.dat" },           // mv_info
    { 0x5E, 0xBE0, 0x41C, 8, "sw_3d_coef_out.dat" },
    { 0x5F, 0,    0xBBC, 8, "sw_ctu_rc_mad_output.dat" },
    { 0x81, 0,    0xA70, 0, "sw_thumb_luma.dat" },
    { 0x82, 0,    0xA74, 0, "sw_thumb_cb.dat" },
    { 0xB6, 0,    0xBF8, 8, "sw_img_binary_output.dat" },
    { 0xB8, 0,    0xB3C, 8, "sw_qp_mad_sse_output.dat" },
};

static int encPatCheckDumpOutListIdx(unsigned int nIdx, EncPatIonBuf *pBuf)
{
    int cnt = 0, ret = 0;
    int num = sizeof(gEncDumpOutFileList) / sizeof(EncPatFileList);

    for(cnt = 0; cnt < num; cnt++)
    {
        if(nIdx == gEncDumpOutFileList[cnt].nFileIdx)
        {
            pBuf->bIsSram = gEncDumpOutFileList[cnt].bIsSram;
            pBuf->nRegIdx = gEncDumpOutFileList[cnt].nRegIdx;
            pBuf->nRshBits = gEncDumpOutFileList[cnt].nRshBits;
            sprintf(pBuf->pName, "%s", gEncDumpOutFileList[cnt].pName);
            break;
        }
    }

    ret = cnt >= num ? -1 : cnt;

    return ret;
}

static int encPatCheckDumpInListIdx(unsigned int nIdx, EncPatIonBuf *pBuf)
{
    int cnt = 0, ret = 0;
    int num = sizeof(gEncDumpInFileList) / sizeof(EncPatFileList);

    for(cnt = 0; cnt < num; cnt++)
    {
        if(nIdx == gEncDumpInFileList[cnt].nFileIdx)
        {
            pBuf->bIsSram = gEncDumpInFileList[cnt].bIsSram;
            pBuf->nRegIdx = gEncDumpInFileList[cnt].nRegIdx;
            pBuf->nRshBits = gEncDumpInFileList[cnt].nRshBits;
            sprintf(pBuf->pName, "%s", gEncDumpInFileList[cnt].pName);
            break;
        }
    }

    ret = cnt >= num ? -1 : cnt;

    return ret;
}

static int encPatInitBuf(EncPatCtx *patCtx, EncPatIonBuf *pBuf, int bIsDumpOut)
{
    int ret = 0;
    FILE *fp = NULL;
    char path[128] = {0};
    MEMOPS_STRUCT *_memops = patCtx->memops;
    void *veOps = (void *)patCtx->veOpsS;
    void *pVeopsSelf = patCtx->pVeOpsSelf;

    sprintf(path, "%s/%s", patCtx->pPatternPath, pBuf->pName);
    if(NULL == (fp = fopen(path, "rb")))
    {
        loge("%s fopen failed!", path);
        ret = -1;
        goto BUF_INIT_FAILED;
    }

    fseek(fp, 0, SEEK_END);
    if(0 == (pBuf->nDataLen = ftell(fp)))
    {
        loge("%s len is 0", path);
        ret = -1;
        goto BUF_INIT_FAILED;
    }
    fseek(fp, 0, SEEK_SET);

    if(pBuf->nRegIdx == 0xB80)
    {
        pBuf->nBufLen = patCtx->nBsBufLen;
        pBuf->pVirAddr = patCtx->pBsBgnVirAddr + (patCtx->nBsOffset >> 3);
        pBuf->nPhyAddr = (patCtx->nBsBgnPhyAddr << 8) + (patCtx->nBsOffset >> 3);
    }
    else if(pBuf->nRegIdx == 0xBBC)
    {
        pBuf->nBufLen = pBuf->nDataLen;
        pBuf->pVirAddr = patCtx->pLineRcVirAddr;
        pBuf->nPhyAddr = patCtx->nLineRcPhyAddr << 8;
        if(!bIsDumpOut)
        {
            pBuf->pVirAddr += 512;
            pBuf->nPhyAddr += 512;
        }
    }
    else
    {
        pBuf->nBufLen = U_ALIGN(4096, pBuf->nDataLen);
        if(NULL == (pBuf->pVirAddr = (unsigned char*)PatAdapterMemPalloc(pBuf->nBufLen)))
        {
            loge("%s pVirAddr malloc failed!", pBuf->pName);
            ret = -1;
            goto BUF_INIT_FAILED;
        }
        pBuf->nPhyAddr = (unsigned long)PatAdapterMemGetPhyAddr(pBuf->pVirAddr);
    }

    if(bIsDumpOut)
    {
        if(NULL == (pBuf->pCmpAddr = (unsigned char*)MALLOC(pBuf->nDataLen)))
        {
            loge("%s pCmpAddr malloc failed!", pBuf->pName);
            ret = -1;
            goto BUF_INIT_FAILED;
        }
        fread(pBuf->pCmpAddr, 1, pBuf->nDataLen, fp);
        memset(pBuf->pVirAddr, 0, pBuf->nDataLen);
    }
    else
    {
        fread(pBuf->pVirAddr, 1, pBuf->nDataLen, fp);
    }
    PatAdapterMemFlushCache(pBuf->pVirAddr, pBuf->nDataLen);

BUF_INIT_FAILED:
    if(ret)
    {
        FREE(pBuf->pCmpAddr);
        if(pBuf->pVirAddr)
        {
            PatAdapterMemPfree(pBuf->pVirAddr);
            pBuf->pVirAddr = NULL;
        }
        pBuf->nDataLen = 0;
        pBuf->nBufLen = 0;
    }
    FCLOSE(fp);

    return 0;
}

static void encPatReleaseBuf(EncPatCtx *patCtx, EncPatIonBuf *pBuf, char *pName)
{
    MEMOPS_STRUCT *_memops = patCtx->memops;
    void *veOps = (void *)patCtx->veOpsS;
    void *pVeopsSelf = patCtx->pVeOpsSelf;

    if(pName)
    {
        logd("Release %s", pName);
    }

    FREE(pBuf->pCmpAddr);

    if(pBuf->pVirAddr)
    {
        if(pBuf->nRegIdx != 0xB80 && pBuf->nRegIdx != 0xBBC)
        {
            PatAdapterMemPfree(pBuf->pVirAddr);
        }
        pBuf->pVirAddr = NULL;
    }
    pBuf->nPhyAddr = 0;
    pBuf->nDataLen = 0;
    pBuf->nBufLen = 0;
}

static void encPatReleaseMemory(EncPatCtx *patCtx)
{
    int nInCnt = 0, nOutCnt = 0;
    int nInNum = patCtx->nDumpInBufNum, nOutNum = patCtx->nDumpOutBufNum;
    char pBufName[64] = {0};
    MEMOPS_STRUCT *_memops = patCtx->memops;
    void *veOps = (void *)patCtx->veOpsS;
    void *pVeopsSelf = patCtx->pVeOpsSelf;

    if(patCtx->pDumpInList)
    {
        for(nInCnt = 0; nInCnt < nInNum; nInCnt++)
        {
            sprintf(pBufName, "pDumpInList[%d]", nInCnt);
            encPatReleaseBuf(patCtx, &patCtx->pDumpInList[nInCnt], pBufName);
        }
        FREE(patCtx->pDumpInList);
    }

    if(patCtx->pDumpOutList)
    {
        for(nOutCnt = 0; nOutCnt < nOutNum; nOutCnt++)
        {
            sprintf(pBufName, "pDumpOutList[%d]", nOutCnt);
            encPatReleaseBuf(patCtx, &patCtx->pDumpOutList[nOutCnt], pBufName);
        }
        FREE(patCtx->pDumpOutList);
    }

    if(patCtx->pOlInvertFlagVirAddr)
    {
        PatAdapterMemPfree(patCtx->pOlInvertFlagVirAddr);
        patCtx->pOlInvertFlagVirAddr = NULL;
        patCtx->nOlInvertFlagPhyAddr = 0;
        patCtx->nOlInvertFalgBufLen = 0;
    }

    if(patCtx->pMbInfoVirAddr)
    {
        PatAdapterMemPfree(patCtx->pMbInfoVirAddr);
        patCtx->pMbInfoVirAddr = NULL;
        patCtx->nMbInfoPhyAddr = 0;
        patCtx->nMbInfoBufLen = 0;
    }

    if(patCtx->pDeblockVirAddr)
    {
        PatAdapterMemPfree(patCtx->pDeblockVirAddr);
        patCtx->pDeblockVirAddr = NULL;
        patCtx->nDeblockPhyAddr = 0;
        patCtx->nDeblockBufLen = 0;
    }

    if(patCtx->pLineRcVirAddr)
    {
        PatAdapterMemPfree(patCtx->pLineRcVirAddr);
        patCtx->pLineRcVirAddr = NULL;
        patCtx->nLineRcPhyAddr = 0;
        patCtx->nLineRcBufLen = 0;
    }

    if(patCtx->pBsBgnVirAddr)
    {
        PatAdapterMemPfree(patCtx->pBsBgnVirAddr);
        patCtx->pBsBgnVirAddr = NULL;
        patCtx->nBsBgnPhyAddr = 0;
        patCtx->nBsEndPhyAddr = 0;
        patCtx->nBsBufLen = 0;
    }
}

static int encPatInitMemory(EncPatCtx *patCtx)
{
    int ret = 0;
    int nInCnt = 0, nOutCnt = 0;
    int nInNum = patCtx->nDumpInBufNum, nOutNum = patCtx->nDumpOutBufNum;
    unsigned long dwtmp1 = 0, dwtmp2 = 0, dwtmp3 = 0, dwtmp4;
    unsigned long operation = 0;
    unsigned int  finish_flag = 0;
    MEMOPS_STRUCT *_memops = patCtx->memops;
    void *veOps = (void *)patCtx->veOpsS;
    void *pVeopsSelf = patCtx->pVeOpsSelf;
    int line_cnt = 0;

    if(nInNum <= 0)
    {
        loge("DumpInNum is %d !", nInNum);
        ret = -1;
        goto MEMORY_INIT_FAILED;
    }
    if(NULL == (patCtx->pDumpInList = (EncPatIonBuf*)MALLOC(nInNum*sizeof(EncPatIonBuf))))
    {
        loge("pDumpInList malloc failed!");
        ret = -1;
        goto MEMORY_INIT_FAILED;
    }

    if(nOutNum <= 0)
    {
        loge("DumpOutNum is %d !", nOutNum);
        ret = -1;
        goto MEMORY_INIT_FAILED;
    }
    if(NULL == (patCtx->pDumpOutList = (EncPatIonBuf*)MALLOC(nOutNum*sizeof(EncPatIonBuf))))
    {
        loge("pDumpOutList malloc failed!");
        ret = -1;
        goto MEMORY_INIT_FAILED;
    }

    if(patCtx->eEncType != ENCPP_PAT)
    {
        patCtx->nBsBufLen = U_ALIGN(256, (patCtx->nBsEndPhyAddr << 8) + 1) - (patCtx->nBsBgnPhyAddr << 8);
        patCtx->nBsBufLen = U_ALIGN(4096, patCtx->nBsBufLen);
        if(NULL == (patCtx->pBsBgnVirAddr = (unsigned char*)PatAdapterMemPalloc(patCtx->nBsBufLen)))
        {
            loge("pBsBgnVirAddr malloc failed!");
        }
        memset(patCtx->pBsBgnVirAddr, 0, patCtx->nBsBufLen);
        PatAdapterMemFlushCache(patCtx->pBsBgnVirAddr, patCtx->nBsBufLen);
        patCtx->nBsBgnPhyAddr = (unsigned long)PatAdapterMemGetPhyAddr(patCtx->pBsBgnVirAddr);
        patCtx->nBsEndPhyAddr = patCtx->nBsBgnPhyAddr + patCtx->nBsBufLen - 1;
        patCtx->nBsBgnPhyAddr >>= 8;
        patCtx->nBsEndPhyAddr >>= 8;
        logw("nBs in (%p, 0x%08x) malloc %d bytes",
            patCtx->pBsBgnVirAddr, patCtx->nBsBgnPhyAddr, patCtx->nBsBufLen);
    }

    if(patCtx->eEncType == H264_ENC_PAT || patCtx->eEncType == H265_ENC_PAT)
    {
        patCtx->nLineRcBufLen = (patCtx->nDstHeightMb - 1) * 12 + 512;
        patCtx->nLineRcBufLen = U_ALIGN(4096, patCtx->nLineRcBufLen);
        if(NULL == (patCtx->pLineRcVirAddr = (unsigned char*)PatAdapterMemPalloc(patCtx->nLineRcBufLen)))
        {
            loge("pLineRcVirAddr malloc failed!");
            ret = -1;
            goto MEMORY_INIT_FAILED;
        }
        memset(patCtx->pLineRcVirAddr, 0, patCtx->nLineRcBufLen);
        PatAdapterMemFlushCache(patCtx->pLineRcVirAddr, patCtx->nLineRcBufLen);
        patCtx->nLineRcPhyAddr = (unsigned long)PatAdapterMemGetPhyAddr(patCtx->pLineRcVirAddr);
        patCtx->nLineRcPhyAddr >>= 8;
        logw("nLineRc in (%p, 0x%08x) malloc %d bytes",
            patCtx->pLineRcVirAddr, patCtx->nLineRcPhyAddr, patCtx->nLineRcBufLen);

        patCtx->nMbInfoBufLen = patCtx->nDstWidth16Align * 8;
        patCtx->nMbInfoBufLen = U_ALIGN(4096, patCtx->nMbInfoBufLen);
        if(NULL == (patCtx->pMbInfoVirAddr = (unsigned char*)PatAdapterMemPalloc(patCtx->nMbInfoBufLen)))
        {
            loge("pMbInfoVirAddr malloc failed!");
            ret = -1;
            goto MEMORY_INIT_FAILED;
        }
        memset(patCtx->pMbInfoVirAddr, 0, patCtx->nMbInfoBufLen);
        PatAdapterMemFlushCache(patCtx->pMbInfoVirAddr, patCtx->nMbInfoBufLen);
        patCtx->nMbInfoPhyAddr = (unsigned long)PatAdapterMemGetPhyAddr(patCtx->pMbInfoVirAddr);
        patCtx->nMbInfoPhyAddr >>= 8;
        logw("nMbInfo in (%p, 0x%08x) malloc %d bytes",
            patCtx->pMbInfoVirAddr, patCtx->nMbInfoPhyAddr, patCtx->nMbInfoBufLen);

        patCtx->nDeblockBufLen = patCtx->nDstWidth16Align * 8;
        patCtx->nDeblockBufLen = U_ALIGN(4096, patCtx->nDeblockBufLen);
        if(NULL == (patCtx->pDeblockVirAddr = (unsigned char*)PatAdapterMemPalloc(patCtx->nDeblockBufLen)))
        {
            loge("pDeblockVirAddr malloc failed!");
            goto MEMORY_INIT_FAILED;
        }
        memset(patCtx->pDeblockVirAddr, 0, patCtx->nDeblockBufLen);
        PatAdapterMemFlushCache(patCtx->pDeblockVirAddr, patCtx->nDeblockBufLen);
        patCtx->nDeblockPhyAddr = (unsigned long)PatAdapterMemGetPhyAddr(patCtx->pDeblockVirAddr);
        patCtx->nDeblockPhyAddr >>= 8;
        logw("nDeblock in (%p, 0x%08x) malloc %d bytes",
            patCtx->pDeblockVirAddr, patCtx->nDeblockPhyAddr, patCtx->nDeblockBufLen);
    }

    if(patCtx->bOverlay)
    {
        patCtx->nOlInvertFalgBufLen = patCtx->nDstWidthMb * 32;
        patCtx->nOlInvertFalgBufLen = U_ALIGN(4096, patCtx->nOlInvertFalgBufLen);
        if(NULL == (patCtx->pOlInvertFlagVirAddr = (unsigned char*)PatAdapterMemPalloc(patCtx->nOlInvertFalgBufLen)))
        {
            loge("pOlInvertFlagVirAddr malloc failed!");
            ret = -1;
            goto MEMORY_INIT_FAILED;
        }
        memset(patCtx->pOlInvertFlagVirAddr, 0, patCtx->nOlInvertFalgBufLen);
        PatAdapterMemFlushCache(patCtx->pOlInvertFlagVirAddr, patCtx->nOlInvertFalgBufLen);
        patCtx->nOlInvertFlagPhyAddr = (unsigned long)PatAdapterMemGetPhyAddr(patCtx->pOlInvertFlagVirAddr);
        patCtx->nOlInvertFlagPhyAddr >>= 8;
        logw("OlInvertFlag in (%p, 0x%08x) malloc %d bytes",
            patCtx->pOlInvertFlagVirAddr, patCtx->nOlInvertFlagPhyAddr, patCtx->nOlInvertFalgBufLen);
    }

    fseek(patCtx->fpPattern, 0, SEEK_SET);
    do
    {
        fgets(patCtx->pPatternLine, 256, patCtx->fpPattern);
        if(4 != sscanf(patCtx->pPatternLine, "%08x%08x%08x%08x", &dwtmp1, &dwtmp2, &dwtmp3, &dwtmp4))
        {
            loge("sscanf pattern line failed!");
            break;
        }
        operation = (dwtmp1 & 0xFF000000) >> 24;
        switch(operation)
        {
            case 0x01:
            {
                int idx = 0;

                if(dwtmp1 & 0x00F00000)
                {
                    idx = encPatCheckDumpOutListIdx(dwtmp2, &patCtx->pDumpOutList[nOutCnt]);
                    if(idx < 0)
                    {
                        loge("Can not find DumpOutFile(0x%x)", dwtmp2);
                    }
                    else
                    {
                        encPatInitBuf(patCtx, &patCtx->pDumpOutList[nOutCnt], 1);
                        logw("%s in (%p, 0x%08x) DataLen:%d, BufLen:%d", patCtx->pDumpOutList[nOutCnt].pName,
                            patCtx->pDumpOutList[nOutCnt].pVirAddr, patCtx->pDumpOutList[nOutCnt].nPhyAddr,
                            patCtx->pDumpOutList[nOutCnt].nDataLen, patCtx->pDumpOutList[nOutCnt].nBufLen);
                        nOutCnt++;
                    }
                }
                else
                {
                    idx = encPatCheckDumpInListIdx(dwtmp2, &patCtx->pDumpInList[nInCnt]);
                    if(idx < 0)
                    {
                        loge("Can not find DumpInFile(0x%x)", dwtmp2);
                    }
                    else
                    {
                        encPatInitBuf(patCtx, &patCtx->pDumpInList[nInCnt], 0);
                        logw("%s in (%p, 0x%08x) DatLen:%d, BufLen:%d", patCtx->pDumpInList[nInCnt].pName,
                            patCtx->pDumpInList[nInCnt].pVirAddr, patCtx->pDumpInList[nInCnt].nPhyAddr,
                            patCtx->pDumpInList[nInCnt].nDataLen, patCtx->pDumpInList[nInCnt].nBufLen);
                        nInCnt++;
                    }
                }
                break;
            }
            case 0x0F:
            case 0xFF:
            {
                patCtx->nDumpInBufNum = nInCnt;
                patCtx->nDumpOutBufNum = nOutCnt;
                logd("Finish init dump buffer, In(%d), Out(%d) !", patCtx->nDumpInBufNum, patCtx->nDumpOutBufNum);
                finish_flag = 1;
                break;
            }
            default:
            {
                break;
            }
        }
        line_cnt++;
    } while(!finish_flag && !ret);
    fseek(patCtx->fpPattern, 0, SEEK_SET);

MEMORY_INIT_FAILED:
    if(ret)
    {
        encPatReleaseMemory(patCtx);
    }

    return ret;
}

static void encPatCheckDumpInfo(EncPatCtx *patCtx)
{
    unsigned long dwtmp1 = 0, dwtmp2 = 0, dwtmp3 = 0, dwtmp4;
    unsigned long operation = 0;
    unsigned int  finish_flag = 0, ret = 0;
    int line_cnt = 0;

    fseek(patCtx->fpPattern, 0, SEEK_SET);
    do
    {
        fgets(patCtx->pPatternLine, 256, patCtx->fpPattern);
        if(4 != sscanf(patCtx->pPatternLine, "%08x%08x%08x%08x", &dwtmp1, &dwtmp2, &dwtmp3, &dwtmp4))
        {
            loge("sscanf pattern line failed!");
            break;
        }
        operation = (dwtmp1 & 0xFF000000) >> 24;
        switch(operation)
        {
            case 0x00:
            {
                unsigned int reg_idx = dwtmp3 & 0x00000FFF;

                if(reg_idx == 0x000 && ((dwtmp4 >> 6) & 0x1) == 1)
                {
                    patCtx->eEncType = ENCPP_PAT;
                }
                else if(reg_idx == 0xA00)
                {
                    patCtx->nSrcWidth = ((dwtmp4 >> 16) & 0x7FF) << 3;
                    patCtx->nSrcHeight = (dwtmp4 & 0x7FF) << 3;
                }
                else if(reg_idx == 0xA08)
                {
                    patCtx->nRotAngle = (dwtmp4 >> 20) & 0x3;
                    patCtx->bOverlay = (dwtmp4 >> 18) & 0x1;
                    patCtx->bScaler = (dwtmp4 >> 16) & 0x1;
                }
                else if(reg_idx == 0xA38)
                {
                    patCtx->nScHorRatio = dwtmp4 & 0x7FFF;
                    patCtx->nScVerRatio = (dwtmp4 >> 16) & 0x7FFF;
                }
                else if((reg_idx == 0xA0C && (dwtmp4 & 0x1) == 0x1) || (reg_idx == 0xB18 && (dwtmp4 & 0xF) == 0x8))
                {
                    patCtx->eEncType = (reg_idx == 0xA0C) ? ENCPP_PAT : ((dwtmp4 >> 16) & 0x3);
                }
                else if(reg_idx == 0xB80)
                {
                    patCtx->nBsBgnPhyAddr = dwtmp4;
                }
                else if(reg_idx == 0xB84)
                {
                    patCtx->nBsEndPhyAddr = dwtmp4;
                }
                else if(reg_idx == 0xB88)
                {
                    patCtx->nBsOffset = dwtmp4;
                }
                else if(reg_idx == 0xB8C)
                {
                    patCtx->nBsValidSize = dwtmp4;
                }
                break;
            }
            case 0x01:
            {
                if(dwtmp1 & 0x00F00000)
                    patCtx->nDumpOutBufNum++;
                else
                    patCtx->nDumpInBufNum++;
                break;
            }
            case 0x0F:
            case 0xFF:
            {
                logd("Finish check dump buffer, In(%d), Out(%d) !", patCtx->nDumpInBufNum, patCtx->nDumpOutBufNum);
                finish_flag = 1;
                break;
            }
            default:
            {
                break;
            }
        }
        line_cnt++;
    } while(!finish_flag && !ret);
    fseek(patCtx->fpPattern, 0, SEEK_SET);
}

static void encPatWriteAddrToReg(EncPatCtx *patCtx)
{
    int nInCnt = 0, nOutCnt = 0;
    unsigned long base = patCtx->nBaseAddr;

    for(nInCnt = 0; nInCnt < patCtx->nDumpInBufNum; nInCnt++)
    {
        unsigned long bIsSram = patCtx->pDumpInList[nInCnt].bIsSram;
        unsigned long nRegIdx = patCtx->pDumpInList[nInCnt].nRegIdx;
        unsigned int nAddr = patCtx->pDumpInList[nInCnt].nPhyAddr >> patCtx->pDumpInList[nInCnt].nRshBits;
        if(bIsSram)
        {
            WRITE_PAT_REG(base + bIsSram, nRegIdx);
            WRITE_PAT_REG(base + bIsSram + 4, nAddr);
            logv("SramIn%03X(%03x): %08x", bIsSram, nRegIdx, patCtx->pDumpInList[nInCnt].nPhyAddr >> 8);
        }
        else if(nRegIdx != 0xBBC)
        {
            WRITE_PAT_REG(base + nRegIdx, nAddr);
        }
    }

    for(nOutCnt = 0; nOutCnt < patCtx->nDumpOutBufNum; nOutCnt++)
    {
        unsigned long bIsSram = patCtx->pDumpOutList[nOutCnt].bIsSram;
        unsigned long nRegIdx = patCtx->pDumpOutList[nOutCnt].nRegIdx;
        unsigned int nAddr = patCtx->pDumpOutList[nOutCnt].nPhyAddr >> patCtx->pDumpOutList[nOutCnt].nRshBits;
        if(bIsSram)
        {
            WRITE_PAT_REG(base + bIsSram, nRegIdx);
            WRITE_PAT_REG(base + bIsSram + 4, nAddr);
            logv("SramOut%03X(%03x): %08x", bIsSram, nRegIdx, patCtx->pDumpOutList[nOutCnt].nPhyAddr >> 8);
        }
        else if(nRegIdx != 0xB80 && nRegIdx != 0xBBC)
        {
            WRITE_PAT_REG(base + nRegIdx, nAddr);
        }
    }

    if(patCtx->bOverlay)
    {
        WRITE_PAT_REG(base + 0xA54, patCtx->nOlInvertFlagPhyAddr);
    }

    if(patCtx->eEncType == H264_ENC_PAT || patCtx->eEncType == H265_ENC_PAT)
    {
        WRITE_PAT_REG(base + 0xBBC, patCtx->nLineRcPhyAddr);
        WRITE_PAT_REG(base + 0xBC0, patCtx->nMbInfoPhyAddr);
        WRITE_PAT_REG(base + 0xBC4, patCtx->nDeblockPhyAddr);
    }
}

static void encPatClearEncStatus(EncPatCtx *patCtx)
{
    unsigned int encoder_status = READ_PAT_REG(patCtx->nBaseAddr + 0xB1C);
    unsigned int encpp_status = READ_PAT_REG(patCtx->nBaseAddr + 0xA10);

    WRITE_PAT_REG(patCtx->nBaseAddr + 0xB1C, encoder_status);
    WRITE_PAT_REG(patCtx->nBaseAddr + 0xA10, encpp_status);
}

static void encPatPrintReg(void* pAddr, const char* name)
{
    int i;
    volatile int *ptr = (int*)pAddr;

    logw("--------- register of %s:%p -----------", name, ptr);
    for(i=0;i<16;i++)
    {
        logw("%s-%02x:%08x %08x %08x %08x", name, i*16,ptr[0],ptr[1],ptr[2],ptr[3]);
        ptr += 4;
    }
    logw("\n");
}

static int encPatWriteReg(EncPatCtx *patCtx)
{
    unsigned long dwtmp1 = 0, dwtmp2 = 0, dwtmp3 = 0, dwtmp4;
    unsigned long operation = 0;
    unsigned int  finish_flag = 0, cdc_trig_flag = 0;
    int ret = 0;
    int line_cnt = 0;

    fseek(patCtx->fpPattern, 0, SEEK_SET);
    do
    {
        fgets(patCtx->pPatternLine, 256, patCtx->fpPattern);
        if(4 != sscanf(patCtx->pPatternLine, "%08x%08x%08x%08x", &dwtmp1, &dwtmp2, &dwtmp3, &dwtmp4))
        {
            loge("sscanf pattern line failed!");
            break;
        }
        logv("line(%d): %08x %08x %08x %08x", line_cnt, dwtmp1, dwtmp2, dwtmp3, dwtmp4);
        operation = (dwtmp1 & 0xFF000000) >> 24;
        switch(operation)
        {
            case 0x00:
            {
                unsigned int offset = dwtmp3 & 0x00000FFF;
                unsigned int data = dwtmp4;
                unsigned int base = patCtx->nBaseAddr;
                unsigned int addr = base + offset;

                if(dwtmp1 & 0x00F00000)
                {
                    data = READ_PAT_REG(addr);
                }
                else
                {
                    if(offset == 0xB80)
                    {
                        WRITE_PAT_REG(addr, patCtx->nBsBgnPhyAddr);
                    }
                    else if(offset == 0xB84)
                    {
                        WRITE_PAT_REG(addr, patCtx->nBsEndPhyAddr);
                    }
                    else if((offset == 0xA0C && (data & 0x1) == 0x1) || (offset == 0xB18 && (data & 0xF) == 0x8))
                    {
                        encPatWriteAddrToReg(patCtx);
                        if(0)
                        {
                            encPatPrintReg((void *)patCtx->nBaseAddr, "before-top");
                            encPatPrintReg((void *)patCtx->nBaseAddr+0xA00, "before-encpp");
                            encPatPrintReg((void *)patCtx->nBaseAddr+0xB00, "before-encoder");
                        }
                        WRITE_PAT_REG(addr, data);

                        if(patCtx->eCheckIntType == WAITING_INTTERRUPT)
                        {
                            unsigned int wait_ret = CdcVeWaitInterrupt(patCtx->veOpsS, patCtx->pVeOpsSelf);
                            if(wait_ret == VE_INT_RESULT_TYPE_TIMEOUT)
                            {
                                encPatPrintReg((void *)patCtx->nBaseAddr, "timeout-top");
                                encPatPrintReg((void *)patCtx->nBaseAddr+0xA00, "timeout-encpp");
                                encPatPrintReg((void *)patCtx->nBaseAddr+0xB00, "timeout-encpp");
                                CdcVeResetForce(patCtx->veOpsS, patCtx->pVeOpsSelf);
                                ret = -1;
                            }
                            if(0)
                            {
                                encPatPrintReg((void *)patCtx->nBaseAddr, "after-top");
                                encPatPrintReg((void *)patCtx->nBaseAddr+0xA00, "after-encpp");
                                encPatPrintReg((void *)patCtx->nBaseAddr+0xB00, "after-encpp");
                            }
                            encPatClearEncStatus(patCtx);
                            cdc_trig_flag = 1;
                        }
                        else
                        {
                            usleep(100000);
                        }
                    }
                    else
                    {
                        WRITE_PAT_REG(addr, data);
                    }
                }
                break;
            }
            case 0x02:
            {
                if(!cdc_trig_flag)
                {
                    int cnt = 0, is_true = 0;
                    unsigned int offset = dwtmp3 & 0x00000FFF;
                    unsigned int data = dwtmp4;
                    unsigned int base = patCtx->nBaseAddr;
                    unsigned int addr = base + offset;

                    for(cnt = 0; cnt < 10000; cnt++)
                    {
                        data = READ_PAT_REG(addr);
                        data &= dwtmp2;
                        if(data == dwtmp4)
                        {
                            is_true = 1;
                            break;
                        }
                    }
                    if(!is_true)
                    {
                        loge("Line(%d), Reg%3X is 0x%08x but not 0x%08x!", line_cnt, offset, data, dwtmp4);
                        ret = -1;
                    }
                }
                break;
            }
            case 0xF0:
            {
                if(patCtx->eCheckIntType == INQUIRE_REGISTER)
                {
                    unsigned long delay = dwtmp4 * patCtx->nVeFq;
                    usleep(delay);
                }
                break;
            }
            case 0x0F:
            case 0xFF:
            {
                logd("Finish read and write register!");
                finish_flag = 1;
                break;
            }
            default:
            {
                break;
            }
        }

        if(ret)
        {
            break;
        }
        line_cnt++;
    } while(!finish_flag && !ret);
    fseek(patCtx->fpPattern, 0, SEEK_SET);

    return ret;
}

static int encPatCompareBuf(EncPatCtx *patCtx, EncPatIonBuf *pBuf)
{
    int ret = 0;
    MEMOPS_STRUCT *_memops = patCtx->memops;
    void *veOps = (void *)patCtx->veOpsS;
    void *pVeopsSelf = patCtx->pVeOpsSelf;

    PatAdapterMemFlushCache(pBuf->pVirAddr, pBuf->nDataLen);
    ret = memcmp(pBuf->pVirAddr, pBuf->pCmpAddr, pBuf->nDataLen);
    if(ret)
    {
        char path[256] = {0};
        FILE *fpHw = NULL;

        sprintf(path, "%s/h%s", patCtx->pPatternPath, &pBuf->pName[1]);
        fpHw = fopen(path, "wb");
        fwrite(pBuf->pVirAddr, 1, pBuf->nDataLen, fpHw);
        FCLOSE(fpHw);
        loge("%s compare failed!", pBuf->pName);
    }
    else
    {
        logw("%s compare sucess!", pBuf->pName);
    }

    return ret;
}

static int encPatCompare(EncPatCtx *patCtx)
{
    int ret = 0;
    int nOutCnt = 0;

    for(nOutCnt = 0; nOutCnt < patCtx->nDumpOutBufNum; nOutCnt++)
    {
        ret |= encPatCompareBuf(patCtx, &patCtx->pDumpOutList[nOutCnt]);
    }

    if(ret)
    {
        encPatPrintReg((void *)patCtx->nBaseAddr, "compare-top");
        encPatPrintReg((void *)patCtx->nBaseAddr+0xA00, "compare-encpp");
        encPatPrintReg((void *)patCtx->nBaseAddr+0xB00, "compare-encoder");
    }

    return ret;
}

void* EncPatOpen(void* pBaseConfig)
{
    VeBaseCfg *baseCfg = (VeBaseCfg*)pBaseConfig;
    EncPatCtx *patCtx = NULL;

    if(NULL == (patCtx = (EncPatCtx*)MALLOC(sizeof(EncPatCtx))))
    {
        loge("Create EncPatCtx failed!");
        return NULL;
    }
    memset(patCtx, 0, sizeof(EncPatCtx));

    patCtx->veOpsS = baseCfg->veOpsS;
    patCtx->pVeOpsSelf = baseCfg->pVeOpsSelf;
    patCtx->memops = baseCfg->memops;

    patCtx->nBaseAddr = (unsigned long)CdcVeGetGroupRegAddr(baseCfg->veOpsS, baseCfg->pVeOpsSelf, REG_GROUP_VETOP);

    return (void*)patCtx;
}

int EncPatUnInit(void* handle)
{
    int cnt;
    EncPatCtx *patCtx = (EncPatCtx*)handle;

    encPatReleaseMemory(patCtx);

    FCLOSE(patCtx->fpPattern);

    logw("Finish PatternUnInit");
    return 0;
}

int EncPatInit(void* handle)
{
    int ret = 0;
    char path[256] = {0};
    EncPatCtx *patCtx = (EncPatCtx*)handle;

    sprintf(path, "%s/aw_pattern.txt", patCtx->pPatternPath);
    if(NULL == (patCtx->fpPattern = fopen(path, "r")))
    {
        loge("%s fopen failed!", path);
        ret = -1;
        goto PATTERN_INIT_FAILED;
    }

    encPatCheckDumpInfo(patCtx);

    patCtx->nVeFq = ENCODER_FQ;

    if(patCtx->nRotAngle == 1 || patCtx->nRotAngle == 3)
    {
        if(patCtx->bScaler)
        {
            patCtx->nDstWidth = U_ALIGN(16, (patCtx->nSrcHeight << 12) / patCtx->nScHorRatio);
            patCtx->nDstHeight = U_ALIGN(8, (patCtx->nSrcWidth << 12) / patCtx->nScVerRatio);
        }
        else
        {
            patCtx->nDstWidth = patCtx->nSrcHeight;
            patCtx->nDstHeight = patCtx->nSrcWidth;
        }
    }
    else
    {
        if(patCtx->bScaler)
        {
            patCtx->nDstWidth = U_ALIGN(16, (patCtx->nSrcWidth << 12) / patCtx->nScHorRatio);
            patCtx->nDstHeight = U_ALIGN(8, (patCtx->nSrcHeight << 12) / patCtx->nScVerRatio);
        }
        else
        {
            patCtx->nDstWidth = patCtx->nSrcWidth;
            patCtx->nDstHeight = patCtx->nSrcHeight;
        }
    }
    patCtx->nDstWidth16Align = U_ALIGN(16, patCtx->nDstWidth);
    patCtx->nDstHeight16Align = U_ALIGN(16, patCtx->nDstHeight);
    patCtx->nDstWidthMb = patCtx->nDstWidth16Align >> 4;
    patCtx->nDstHeightMb = patCtx->nDstHeight16Align >> 4;
    DEFAULT(patCtx->nBsValidSize, 0x04000000, patCtx->nBsValidSize);

    logw("%dx%d->%dx%d, EncType:%d, DumpIn:%d, DumpOut:%d",
        patCtx->nSrcWidth, patCtx->nSrcHeight, patCtx->nDstWidth, patCtx->nDstHeight,
        patCtx->eEncType, patCtx->nDumpInBufNum, patCtx->nDumpOutBufNum);
    if(encPatInitMemory(patCtx))
    {
        loge("patternInitMemory failed!");
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

void EncPatClose(void* handle)
{
    EncPatCtx *patCtx = (EncPatCtx*)handle;

    EncPatUnInit(patCtx);
    FREE(patCtx);

    logw("Finish PatternEncClose");
}

int EncPatWork(void *handle)
{
    EncPatCtx *patCtx = (EncPatCtx*)handle;
    int ret = 0;

    CdcVeReset(patCtx->veOpsS, patCtx->pVeOpsSelf);
    CdcVeEnableVe(patCtx->veOpsS, patCtx->pVeOpsSelf);

    if(encPatWriteReg(patCtx))
    {
        loge("WrtieReg failed!");
        ret = -1;
        goto ENCODE_FAILED;
    }

    CdcVeReset(patCtx->veOpsS, patCtx->pVeOpsSelf);
    CdcVeDisableVe(patCtx->veOpsS,patCtx->pVeOpsSelf);

    if(encPatCompare(patCtx))
    {
        loge("Compare failed!");
        ret = -1;
        goto ENCODE_FAILED;
    }

    logw("Finish PatternEncEncode");
ENCODE_FAILED:

    return ret;
}

int EncPatGetParameter(void *handle, int indexType, void* param)
{
    return 0;
}

int EncPatSetParameter(void *handle, int indexType, void* param)
{
    int ret = 0;
    EncPatCtx *patCtx = (EncPatCtx*)handle;

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

PatWorkDev g_enc_pattern_device = {
    .codecType              = "encoder pattern",
    .open                   = EncPatOpen,
    .init                   = EncPatInit,
    .uninit                 = EncPatUnInit,
    .close                  = EncPatClose,
    .work                   = EncPatWork,
    .GetParameter           = EncPatGetParameter,
    .SetParameter           = EncPatSetParameter,
};
