/*
 * Copyright (C) 2008-2015 Allwinner Technology Co. Ltd.
 * Author: Ning Fang <fangning@allwinnertech.com>
 *         Caoyuan Yang <yangcaoyuan@allwinnertech.com>
 *
 * This software is confidential and proprietary and may be used
 * only as expressly authorized by a licensing agreement from
 * Softwinner Products.
 *
 * The entire notice above must be reproduced on all copies
 * and should not be removed.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef _ENC_ADAPTER_H
#define _ENC_ADAPTER_H

//#include "memoryAdapter.h"
//#include "secureMemoryAdapter.h"
#include <cdc_config.h>
#ifdef CONFIG_VIDEO_RT_MEDIA
#include "mem_interface.h"
#include "ve_interface.h"
#else
#include <sc_interface.h>
#endif

int   EncAdapterLockVideoEngine(void);

void  EncAdapterUnLockVideoEngine(void);

void  EncAdapterVeReset(void);

int   EncAdapterVeWaitInterrupt(void);

void* EncAdapterVeGetBaseAddress(void);

void EncAdapterEnableEncoder(void);

void EncAdapterDisableEncoder(void);

void EncAdapterResetEncoder(void);

void EncAdapterInitPerformance(int nMode);

void EncAdapterUninitPerformance(int nMode);

unsigned int EncAdapterGetICVersion(void* pTopBaseAddr);

void EncAdapterPrintTopVEReg(void *pBaseAddr, const char *name);

void EncAdapterPrintEncReg(void *pBaseAddr, const char *name);

void EncAdapterPrintIspReg(void *pBaseAddr, const char *name);

unsigned int EncAdapterGetVeAddrOffset(void);

void* __EncAdapterMemPalloc(MEMOPS_STRUCT *memops, int nSize, void *veOps, void *pVeopsSelf);

void* __EncAdapterMemNoCachePalloc(MEMOPS_STRUCT *memops, int nSize, void *veOps, void *pVeopsSelf);

void __EncAdapterMemPfree(MEMOPS_STRUCT *memops, void* pMem, void *veOps, void *pVeopsSelf);

void __EncAdapterMemFlushCache(MEMOPS_STRUCT *memops, void* pMem, int nSize);

void* __EncAdapterMemGetPhysicAddress(MEMOPS_STRUCT *memops, void* pVirtualAddress);

void* __EncAdapterMemGetPhysicAddressCpu(MEMOPS_STRUCT *memops, void* pVirtualAddress);

void* __EncAdapterMemGetVirtualAddress(MEMOPS_STRUCT *memops, void* pPhysicAddress);

unsigned int __EncAdapterMemGetVeAddrOffset(MEMOPS_STRUCT *memops);

#ifdef CONFIG_VIDEO_RT_MEDIA

#define CDCGetVeOpsS(veOpsS, type) do {\
   VeConfig mVeConfig;\
   memset(&mVeConfig, 0, sizeof(VeConfig));\
   mVeConfig.nDecoderFlag = 0;\
   mVeConfig.nEncoderFlag = 1;\
   mVeConfig.bJustRunIspFlag = 0;\
   veOpsS = ve_create(type, mVeConfig);\
} while(0)
#define CDCVeInit(veOpsS, p_mVeConfig) (void *)(-1)
#define CDCVeRelease(veOpsS, pVeOpsSelf) ve_destory(VE_OPS_TYPE_NORMAL, veOpsS)
#define CDCVeGetGroupRegAddr(veops, p, nGroupId) ve_get_group_reg_addr(veops, nGroupId)
#define CDCVeEnableVe(veops, p) ve_enable_ve(veops)
#define CDCVeDisableVe(veops, p) ve_disable_ve(veops)
#define CDCVeWaitInterrupt(veops, p) ve_wait_interrupt(veops)
#define CDCVeReset(veops, p) ve_reset(veops)
#define CDCVeResetForce(veops, p) ve_force_reset(veops)
#define CDCGetCsiOnlineInfo(veops, p, pCsiOnlineInfo) ve_get_csi_online_info(veops, pCsiOnlineInfo)
#define CDCVeSetProcInfo(veops, p, p_ch_proc_info) ve_set_proc_info(veops, p_ch_proc_info)
#define CDCVeStopProcInfo(veops, p, p_ch_proc_info) ve_stop_proc_info(veops, p_ch_proc_info)
#define CDCVeAllocPageBuf(veops, p, page_buf) CdcVeAllocPageBuf(veops, page_buf)
#define CDCVeRecPageBuf(veops, p, page_buf) CdcVeRecPageBuf(veops, page_buf)
#define CDCVeFreePageBuf(veops, p, page_buf) CdcVeFreePageBuf(veops, page_buf)
#define CDCVeLock(veops, pVeOpsSelf) ve_lock(veops)
#define CDCVeUnLock(veops, pVeOpsSelf) ve_unlock(veops)
#define CDCVeSetOnlineChannel(veops, pVeOpsSelf, bIsOnlineChannel) CdcVeSetOnlineChannel(veops, bIsOnlineChannel)
#define CDCVeSetSpeed(veops, pVeOpsSelf, nVeFreq) ve_set_ve_freq(veops, nVeFreq)
#define CDCVeSetDdrMode(veops, pVeOpsSelf, nDdrType) ve_set_ddr_mode(veops, nDdrType)
#define CDCVeSetMode(veops, pVeOpsSelf, nMode) CdcVeMode(veops, nMode)

#define CDCSetLbcParameter(veops, p, nLbcMode, nWidht, bEnablePageBuf) ve_set_lbc_parameter(veops, nLbcMode, nWidht)

#define CDCGetMemOps() mem_create(MEM_TYPE_ION)
#define CDCMemOpen(memops) 0
#define CDCMemClose(memops) mem_destory(MEM_TYPE_ION, memops)
#define EncAdapterMemPalloc(nSize) cdc_mem_palloc(_memops, nSize)
#define EncAdapterNoCacheMemPalloc(size) cdc_mem_palloc_no_cache(_memops, size)
#define CDCIonGetMemType(memops) cdc_mem_get_flag(memops)
#define CDCVeGetDramType(veops, p) 0
#if 0//debug used
#define EncAdapterMemPfree(_memops, pMem) do{\
       logi("call pfree 0x%p", pMem);\
       cdc_mem_pfree(_memops, pMem);\
   }while(0)
#else
#define EncAdapterMemPfree(pMem) cdc_mem_pfree(_memops, pMem)
#endif

#if 0//debug used
#define EncAdapterMemFlushCache(pMem, nSize) do{\
       logw("call flush _memops %p pMem %p nSize %d", _memops, pMem, nSize);\
       cdc_mem_flush_cache(_memops, pMem, nSize);\
       logw("end call flush _memops %p pMem %p nSize %d", _memops, pMem, nSize);\
   }while(0)
#else
#define EncAdapterMemFlushCache(pMem, nSize) cdc_mem_flush_cache(_memops, pMem, nSize)
#endif

#define EncAdapterMemGetPhysicAddress(pVirtualAddress) cdc_mem_get_phy(_memops, pVirtualAddress)
#define EncAdapterGetVeAddrOffset() 0
#define EncAdapterMemGetPhysicAddressCpu(pVirtualAddress) cdc_mem_get_phy(_memops, pVirtualAddress)
#define EncAdapterShareFd(p, vir) cdc_mem_share_fd(p, vir)
#define CdcMemCopy(m, x, y, n) memcpy(x, y, n)


#else//not CONFIG_VIDEO_RT_MEDIA

#define CDCGetVeOpsS(veOpsS, type) veOpsS = GetVeOpsS(type)
#define CDCVeInit(veOpsS, p_mVeConfig) CdcVeInit(veOpsS, p_mVeConfig)
#define CDCVeRelease(veOpsS, pVeOpsSelf) CdcVeRelease(veOpsS,pVeOpsSelf)
#define CDCVeGetGroupRegAddr(veops, p, nGroupId) CdcVeGetGroupRegAddr(veops, p, nGroupId)
#define CDCVeEnableVe(veops, p) CdcVeEnableVe(veops, p)
#define CDCVeDisableVe(veops, p) CdcVeDisableVe(veops, p)
#define CDCVeWaitInterrupt(veops, p) CdcVeWaitInterrupt(veops, p)
#define CDCVeReset(veops, p) CdcVeReset(veops, p)
#define CDCVeResetForce(veops, p) CdcVeResetForce(veops, p)
#define CDCGetCsiOnlineInfo(veops, p, pCsiOnlineInfo) CdcGetCsiOnlineInfo(veops, p, pCsiOnlineInfo)
#define CDCVeSetProcInfo(veops, p, p_ch_proc_info) CdcVeSetProcInfo(veops, p, p_ch_proc_info)
#define CDCVeStopProcInfo(veops, p, p_ch_proc_info) CdcVeStopProcInfo(veops, p, p_ch_proc_info)
#define CDCVeAllocPageBuf(veops, p, page_buf) CdcVeAllocPageBuf(veops, p, page_buf)
#define CDCVeRecPageBuf(veops, p, page_buf) CdcVeRecPageBuf(veops, p, page_buf)
#define CDCVeFreePageBuf(veops, p, page_buf) CdcVeFreePageBuf(veops, p, page_buf)
#define CDCVeLock(veops, pVeOpsSelf) CdcVeLock(veops, pVeOpsSelf)
#define CDCVeUnLock(veops, pVeOpsSelf) CdcVeUnLock(veops, pVeOpsSelf)
#define CDCVeSetOnlineChannel(veops, pVeOpsSelf, bIsOnlineChannel) CdcVeSetOnlineChannel(veops, pVeOpsSelf, bIsOnlineChannel)
#define CDCVeSetSpeed(veops, pVeOpsSelf, nVeFreq) CdcVeSetSpeed(veops, pVeOpsSelf, nVeFreq)
#define CDCVeSetDdrMode(veops, pVeOpsSelf, nDdrType) CdcVeSetDdrMode(veops, pVeOpsSelf, nDdrType)
#define CDCVeSetMode(veops, pVeOpsSelf, nMode) CdcVeMode(veops, pVeOpsSelf, nMode)

#define CDCSetLbcParameter(veops, p, nLbcMode, nWidht, bEnablePageBuf) CdcSetLbcParameter(veops, p, nLbcMode, nWidht)

#define CDCGetMemOps() MemAdapterGetOpsS()
#define CDCMemOpen(memops) CdcMemOpen(memops)
#define CDCMemClose(memops) CdcMemClose(memops)
#define EncAdapterMemPalloc(nSize) __EncAdapterMemPalloc(_memops, nSize, veOps, pVeopsSelf)
#define EncAdapterNoCacheMemPalloc(nSize) __EncAdapterMemNoCachePalloc(_memops, nSize, veOps, pVeopsSelf)
#define CDCIonGetMemType(memops) CdcIonGetMemType()
#define CDCVeGetDramType(veops, p) CdcVeGetDramType(veops, p)
#if 0//debug used
#define EncAdapterMemPfree(pMem) do{\
       logw("call pfree");\
       __EncAdapterMemPfree(_memops, pMem, veOps, pVeopsSelf);\
       logw("call pfree end");\
   }while(0)
#else
#define EncAdapterMemPfree(pMem) __EncAdapterMemPfree(_memops, pMem, veOps, pVeopsSelf)
#endif
#if 0//debug used
#define EncAdapterMemFlushCache(pMem, nSize) do{\
       logw("call flush _memops %p pMem %p nSize %d", _memops, pMem, nSize);\
       __EncAdapterMemFlushCache(_memops, pMem, nSize);\
       logw("end call flush _memops %p pMem %p nSize %d", _memops, pMem, nSize);\
   }while(0)
#else
#define EncAdapterMemFlushCache(pMem, nSize) __EncAdapterMemFlushCache(_memops, pMem, nSize)
#endif

#define EncAdapterGetVeAddrOffset() __EncAdapterMemGetVeAddrOffset(_memops)
#define EncAdapterMemGetPhysicAddress(pVirtualAddress) __EncAdapterMemGetPhysicAddress(_memops, pVirtualAddress)
#define EncAdapterMemGetPhysicAddressCpu(pVirtualAddress) __EncAdapterMemGetPhysicAddressCpu(_memops, pVirtualAddress)
#define EncAdapterMemGetVirtualAddress(pPhysicAddress) __EncAdapterMemGetVirtualAddress(_memops, pPhysicAddress)
#define EncAdapterShareFd(p, vir) -1


#endif//CONFIG_VIDEO_RT_MEDIA

#endif //_ENC_ADAPTER_H

#ifdef __cplusplus
}
#endif /* __cplusplus */
