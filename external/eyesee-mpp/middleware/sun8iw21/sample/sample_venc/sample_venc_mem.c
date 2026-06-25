//#define LOG_NDEBUG 0
#define LOG_TAG "SampleVencMem"

#include <string.h>

//#include <memoryAdapter.h>
//#include "sc_interface.h"
#include "plat_log.h"
//#include "ion_memmanager.h"
#include <ion_mem_alloc.h>

struct SunxiMemOpsS *gpMemOps = NULL;
int venc_MemOpen(void)
{
    gpMemOps = GetMemAdapterOpsS();
    return SunxiMemOpen(gpMemOps);
}

int venc_MemClose(void)
{
    SunxiMemClose(gpMemOps);
    return 0;
}

unsigned char* venc_allocMem(unsigned int size)
{
    IonAllocAttr allocAttr;
    memset(&allocAttr, 0, sizeof(IonAllocAttr));
    allocAttr.nLen = size;
    //allocAttr.mAlign = 0;
    allocAttr.eIonHeapType = IonHeapType_IOMMU;
    allocAttr.bSupportCache = true;
    return (unsigned char*)SunxiMemPallocExtend(gpMemOps, &allocAttr);
}

int venc_freeMem(void *vir_ptr)
{
    SunxiMemPfree(gpMemOps, vir_ptr);
    return 0;
}

unsigned int venc_getPhyAddrByVirAddr(void *vir_ptr)
{
    return (unsigned int)SunxiMemGetPhysicAddressCpu(gpMemOps, vir_ptr);
}

int venc_flushCache(void *vir_ptr, unsigned int size)
{
    SunxiMemFlushCache(gpMemOps, vir_ptr, size);
    return 0;
}
