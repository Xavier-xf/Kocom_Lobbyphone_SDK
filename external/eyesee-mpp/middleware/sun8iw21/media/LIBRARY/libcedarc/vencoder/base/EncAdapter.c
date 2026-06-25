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

#include "cdc_log.h"

#include "EncAdapter.h"

//* use method provide by libVE.so to process ve control methods.
//* use method provide by libMemAdapter.so to process physical continue memory allocation.

//* memory methods.
#ifndef CONFIG_VIDEO_RT_MEDIA
void* __EncAdapterMemPalloc(MEMOPS_STRUCT *memops, int nSize, void *veOps, void *pVeopsSelf)
{
    return memops->palloc(nSize, veOps, pVeopsSelf);
}

void* __EncAdapterMemNoCachePalloc(MEMOPS_STRUCT *memops, int nSize, void *veOps, void *pVeopsSelf)
{
    return memops->palloc_no_cache(nSize, veOps, pVeopsSelf);
}

void __EncAdapterMemPfree(MEMOPS_STRUCT *memops, void* pMem, void *veOps, void *pVeopsSelf)
{
    memops->pfree(pMem, veOps, pVeopsSelf);
}

void __EncAdapterMemFlushCache(MEMOPS_STRUCT *memops, void* pMem, int nSize)
{
    memops->flush_cache(pMem, nSize);
}

void* __EncAdapterMemGetPhysicAddress(MEMOPS_STRUCT *memops, void* pVirtualAddress)
{
    return memops->ve_get_phyaddr(pVirtualAddress);
}

void* __EncAdapterMemGetPhysicAddressCpu(MEMOPS_STRUCT *memops, void* pVirtualAddress)
{
    return memops->cpu_get_phyaddr(pVirtualAddress);
}

void* __EncAdapterMemGetVirtualAddress(MEMOPS_STRUCT *memops, void* pPhysicAddress)
{
    return memops->ve_get_viraddr(pPhysicAddress);
}

unsigned int __EncAdapterMemGetVeAddrOffset(MEMOPS_STRUCT *memops)
{
    return memops->get_ve_addr_offset();
}
#endif

unsigned int EncAdapterGetICVersion(void* nTopBaseAddr)
{
   volatile unsigned int value;
   value = *((unsigned int*)((char *)nTopBaseAddr + 0xf0));
   if(value == 0)
   {
        value = *((unsigned int*)((char *)nTopBaseAddr + 0xe4));
        if(value == 0)
        {
            loge("can not get the ve version ,both 0xf0 and 0xe4 is 0x00000000\n");
            return 0;
        }
        else
            return value;
   }
   else
        return (value>>16);
}

void EncAdapterPrintTopVEReg(void *pBaseAddr, const char *name)
{
    int i;
	volatile int *ptr = (int *)pBaseAddr;

    logw("--------- register of top level ve base:%p -----------",ptr);
    for(i=0;i<16;i++)
    {
        logw("%s-top-reg%02x:%08x %08x %08x %08x",name, i*16,ptr[0],ptr[1],ptr[2],ptr[3]);
        ptr += 4;
    }
    logw("\n");
}

void EncAdapterPrintEncReg(void *pBaseAddr, const char *name)
{
    int i;
	volatile int *ptr = (int *)((unsigned long)pBaseAddr);

    logw("--------- register of ve encoder base:%p -----------",ptr);
    for(i=0;i<16;i++)
    {
        logw("%s-enc-reg%02x:%08x %08x %08x %08x",name, i*16,ptr[0],ptr[1],ptr[2],ptr[3]);
        ptr += 4;
    }
    logw("\n");
}

void EncAdapterPrintIspReg(void *pBaseAddr, const char *name)
{
    CEDARC_UNUSE(__EncAdapterMemGetVirtualAddress);

    int i;
	volatile int *ptr = (int *)((unsigned long)pBaseAddr);

    logw("--------- register of ve isp base:%p -----------",ptr);
    for(i=0;i<16;i++)
    {
        logw("%s-isp-reg%02x:%08x %08x %08x %08x",name, i*16,ptr[0],ptr[1],ptr[2],ptr[3]);
        ptr += 4;
    }
    logw("\n");
}

