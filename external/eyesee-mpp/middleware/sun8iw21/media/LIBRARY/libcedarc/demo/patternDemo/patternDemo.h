#ifndef _PATTERN_DEMO_H_
#define _PATTERN_DEMO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cdc_log.h"
#include "sc_interface.h"
#include "veAdapter.h"
#include "memoryAdapter.h"

#define U_ALIGN(y, x)                          (((x) + ((y)-1)) & ~((y)-1))
#define MAX(a,b)                               (((a) > (b)) ? (a) : (b))
#define MIN(a,b)                               (((a) < (b)) ? (a) : (b))
#define CLIP3(low,high,x)                      MIN(MAX(low,x), high)
#define DEFAULT(para,def,other)                ((para) = (((para) == 0) ? (def) : (other)))
#define READ_PAT_REG(n)                        (*((volatile unsigned int *)(n)))
#define WRITE_PAT_REG(n,c)                     (*((volatile unsigned int *)(n)) = (unsigned int)(c))
#define FREE(buf)                              if((buf)!=NULL){free(buf);(buf)=NULL;}
#define FCLOSE(fp)                             if((fp)!=NULL){fclose((fp));(fp) = NULL;}

#define PatAdapterMemPalloc(nSize)             CdcMemPalloc(_memops, nSize, veOps, pVeopsSelf)
#define PatAdapterMemPfree(pMem)               CdcMemPfree(_memops, pMem, veOps, pVeopsSelf)
#define PatAdapterMemFlushCache(pMem, nSize)   CdcMemFlushCache(_memops, pMem, nSize)
#define PatAdapterMemGetPhyAddr(pVirAddr)      CdcMemGetPhysicAddress(_memops, pVirAddr)
#define PatAdapterMemGetVirAddr(pPhyAddr)      CdcMemGetVirtualAddress(_memops, pPhyAddr)

typedef enum PatType {
    ENC_PATTERN = 0,
    DEC_PATTERN = 1,
    MIN_PAT_TYPE = ENC_PATTERN,
    MAX_PAT_TYPE = DEC_PATTERN,
} PatType;

typedef enum PatIntType {
    INQUIRE_REGISTER = 0,
    WAITING_INTTERRUPT = 1,
    MIN_INT_TYPE = INQUIRE_REGISTER,
    MAX_INT_TYPE = WAITING_INTTERRUPT
} PatIntType;

typedef struct VeBaseCfg {
    MEMOPS_STRUCT *memops;
    VeOpsS*           veOpsS;
    void*             pVeOpsSelf;
} VeBaseCfg;

typedef struct PatWorkDev {
    const char *codecType;
    void*      (*open)(void* pBaseConfig);
    int        (*init)(void *handle);
    int        (*uninit)(void *handle);
    void       (*close)(void *handle);
    int        (*work)(void *handle);
    int        (*GetParameter)(void *handle, int indexType, void* param);
    int        (*SetParameter)(void *handle, int indexType, void* param);
} PatWorkDev;

typedef struct PatCtx {
    PatType    ePatType;
    PatWorkDev sPatDev;
    void       *pBaseCfg;
    void       *pPatHandle;
} PatCtx;

typedef enum PatIdxType {
    /**< reference type: char* */
    PAT_IndexParamWorkPath,
    /**< reference type: int */
    PAT_IndexParamCheckIntType,
    /**< reference type: int */
    PAT_IndexParamFq,
    /**< reference type: int */

    MIN_PAT_PARAM_IDX = PAT_IndexParamWorkPath,
    MAX_PAT_PARAM_IDX = PAT_IndexParamFq,
} PatIdxType;

#endif
