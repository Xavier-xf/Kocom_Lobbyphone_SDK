/** @file
  @brief wrap ion driver of linux-5.4
  @author eric_wang, PDC-PD5
  @date 2024/05/09
  Copyright (C), 2001-2024, Allwinner Tech. Co., Ltd.
*/
#include <sys/ioctl.h>
//out/v853-perf1/compile_dir/target/linux-v853-perf1/linux-4.9.191/user_headers/include/asm-generic/ioctl.h
//                                                   linux-4.9.191 -> lichee/linux-4.9/
//glibc headers will include "user_headers/include/asm-generic/ioctl.h",
//but musl don't, musl will define _IOC/_IOWR self, and will lead to redefinition conflicts.
#ifndef _ASM_GENERIC_IOCTL_H

#ifdef _IOC
#undef _IOC
#endif
#ifdef _IO
#undef _IO
#endif
#ifdef _IOR
#undef _IOR
#endif
#ifdef _IOW
#undef _IOW
#endif
#ifdef _IOWR
#undef _IOWR
#endif

#endif //!_ASM_GENERIC_IOCTL_H

#include <linux/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>

#include "base_list.h"
#include <ion_mem_alloc.h>

#include <linux/ion.h>
#include <linux/dma-buf.h>
#include <linux/cedar_ve_uapi.h>

#define CONFIG_LOG_LEVEL    OPTION_LOG_LEVEL_DETAIL
#define LOG_TAG "ionAlloc_5.4"
#define DEBUG_ION_REF           0
//#define ION_ALLOC_ALIGN         SZ_4k
#define DEV_NAME                "/dev/ion"
#define DEFAULT_IOMMU_DEV_NAME              "/dev/cedar_dev"
//#define ION_IOC_SUNXI_POOL_INFO 10
#define UNUSA_PARAM(param) (void)param

#define loge(fmt, arg...) fprintf(stderr, LOG_TAG":<%s:%d>"fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define logw(fmt, arg...)
#define logd(fmt, arg...)
#define logv(fmt, arg...)

#if DEBUG_ION_REF==1
    int cdx_use_mem = 0;
    typedef struct ION_BUF_NODE_TEST {
        unsigned int addr;
        int size;
    } ion_buf_node_test;

    #define ION_BUF_LEN  50
    ion_buf_node_test ion_buf_nodes_test[ION_BUF_LEN];
#endif

typedef struct BUFFER_NODE
{
    struct list_head i_list;
    IonHeapType eIonHeapType;
    bool bSupportCache;
    unsigned long phy; /*phisical address*/
    unsigned long vir; /*virtual address*/
    unsigned int size; /*buffer size*/
    unsigned int tee;
    unsigned long user_virt;
    int fd;
} buffer_node;

typedef struct ION_ALLOC_CONTEXT
{
    int fd;
    int iommu_dev_fd;
    struct list_head list; /* buffer list for buffer_node */
    int ref_cnt; /* reference count */
} ion_alloc_context;

char iommu_dev_path[32] = DEFAULT_IOMMU_DEV_NAME;

ion_alloc_context *g_ion_alloc_context = NULL;
pthread_mutex_t g_ion_mutex_alloc = PTHREAD_MUTEX_INITIALIZER;

static int tryOpenDev(const char *pDevPath, int nFlags, int nParam3)
{
    int nFd = -1;
    int i;
    for (i = 0; i < 10; i++)
    {
        nFd = open(pDevPath, nFlags, nParam3);
        if (nFd <= 0)
        {
            usleep(50*1000);
        }
        else
        {
            if (i > 0)
            {
                loge("Be careful! try [%d]times fail to open dev[%s]", i, pDevPath);
            }
            break;
        }
    }
    if (nFd < 0)
    {
        loge("fatal error! open dev[%s] fail. error[%d-%s]!", pDevPath, errno, strerror(errno));
    }
    return nFd;
}

/*funciton begin*/
int sunxi_ion_alloc_open()
{
    logd("begin ion_alloc_open\n");
    pthread_mutex_lock(&g_ion_mutex_alloc);
    if (g_ion_alloc_context != NULL)
    {
        logv("ion allocator has already been created\n");
        goto SUCCEED_OUT;
    }

    g_ion_alloc_context = (ion_alloc_context*)malloc(sizeof(ion_alloc_context));
    if (g_ion_alloc_context == NULL)
    {
        loge("create ion allocator failed, out of memory\n");
        goto ERROR_OUT;
    }
    else
    {
        logv("pid:%d, g_alloc_context = %p\n", getpid(), g_ion_alloc_context);
    }
    memset((void*)g_ion_alloc_context, 0, sizeof(ion_alloc_context));
    /* Readonly should be enough. */
    g_ion_alloc_context->fd = tryOpenDev(DEV_NAME, O_RDONLY, 0);
    if (g_ion_alloc_context->fd <= 0)
    {
        loge("open %s failed\n", DEV_NAME);
        goto ERROR_OUT;
    }

    g_ion_alloc_context->iommu_dev_fd = tryOpenDev(iommu_dev_path, O_RDONLY, 0);
    if (g_ion_alloc_context->iommu_dev_fd <= 0)
    {
        loge("open %s failed\n", iommu_dev_path);
        goto ERROR_OUT;
    }

#if DEBUG_ION_REF==1
    cdx_use_mem = 0;
    memset(&ion_buf_nodes_test, sizeof(ion_buf_nodes_test), 0);
    logd("ion_open, cdx_use_mem=[%dByte].", cdx_use_mem);
    ion_alloc_get_total_size();
#endif
    INIT_LIST_HEAD(&g_ion_alloc_context->list);

SUCCEED_OUT:
    g_ion_alloc_context->ref_cnt++;
    pthread_mutex_unlock(&g_ion_mutex_alloc);
    return 0;

ERROR_OUT:
    if (g_ion_alloc_context != NULL && g_ion_alloc_context->fd > 0)
    {
        close(g_ion_alloc_context->fd);
        g_ion_alloc_context->fd = 0;
    }

    if (g_ion_alloc_context != NULL && g_ion_alloc_context->iommu_dev_fd > 0)
    {
        close(g_ion_alloc_context->iommu_dev_fd);
        g_ion_alloc_context->iommu_dev_fd = 0;
    }

    if (g_ion_alloc_context != NULL)
    {
        free(g_ion_alloc_context);
        g_ion_alloc_context = NULL;
    }

    pthread_mutex_unlock(&g_ion_mutex_alloc);
    return -1;
}

static int ion_alloc_pfree_l(buffer_node *pNode)
{
    int ret = 0;
    /*unmap user space*/
    if (munmap((void *)(pNode->user_virt), pNode->size) < 0)
    {
        loge("fatal error! munmap 0x%p, size: %u failed\n", (void*)pNode->vir, pNode->size);
    }

    struct user_iommu_param iommu_param;
    memset(&iommu_param, 0, sizeof(iommu_param));
    iommu_param.fd = pNode->fd;
    ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_FREE_IOMMU_ADDR, &iommu_param);
    if (ret)
    {
        loge("fatal error! FREE_IOMMU_ADDR err, ret %d\n", ret);
    }
//    ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_ENGINE_REL, 0);
//    if (ret)
//    {
//        loge("fatal error! ENGINE_REL failed\n");
//    }

    /*close dma buffer fd*/
    close(pNode->fd);
    pNode->fd = -1;

    return ret;
}

void sunxi_ion_alloc_close()
{
    logv("ion_alloc_close\n");

    pthread_mutex_lock(&g_ion_mutex_alloc);
    if (--g_ion_alloc_context->ref_cnt <= 0)
    {
        logv("pid: %d, release g_ion_alloc_context = %p\n", getpid(), g_ion_alloc_context);
        if (g_ion_alloc_context->ref_cnt < 0)
        {
            loge("fatal error! libawion maybe close more times than open");
        }
        buffer_node *pEntry, *q;
        list_for_each_entry_safe(pEntry, q, &g_ion_alloc_context->list, i_list)
        {
            logv("ion_alloc_close del item phy= 0x%lx vir= 0x%lx, size= %d\n", pEntry->phy, pEntry->vir, pEntry->size);
            ion_alloc_pfree_l(pEntry);
            list_del(&pEntry->i_list);
            free(pEntry);
        }
#if DEBUG_ION_REF==1
        logd("ion_close, cdx_use_mem=[%d MB]", cdx_use_mem/1024/1024);
        ion_alloc_get_total_size();
#endif
        if(g_ion_alloc_context->fd >= 0)
        {
            close(g_ion_alloc_context->fd);
            g_ion_alloc_context->fd = -1;
        }

        if(g_ion_alloc_context->iommu_dev_fd >= 0)
        {
            close(g_ion_alloc_context->iommu_dev_fd);
            g_ion_alloc_context->iommu_dev_fd = -1;
        }
        free(g_ion_alloc_context);
        g_ion_alloc_context = NULL;
    }
    else
    {
        logv("ref cnt: %d > 0, do not free\n", g_ion_alloc_context->ref_cnt);
    }
    pthread_mutex_unlock(&g_ion_mutex_alloc);
#if DEBUG_ION_REF==1
    int i = 0;
    int counter = 0;
    for (i = 0; i < ION_BUF_LEN; i++) {
        if (ion_buf_nodes_test[i].addr != 0 || ion_buf_nodes_test[i].size != 0) {
            loge("ion mem leak????  addr->[0x%x], leak size->[%dByte]", \
                ion_buf_nodes_test[i].addr, ion_buf_nodes_test[i].size);
            counter ++;
        }
    }

    if (counter != 0) {
        loge("my god, have [%d]blocks ion mem leak.!!!!", counter);
    } else {
        logd("well done, no ion mem leak.");
    }
#endif
    return;
}

void* sunxi_ion_alloc_pallocExtend(IonAllocAttr *pAttr)
{
    struct ion_allocation_data alloc_data;
    struct user_iommu_param iommu_param;

    int rest_size = 0;
    unsigned long addr_phy = 0;
    unsigned long addr_vir = 0;
    buffer_node * alloc_buffer = NULL;
    int ret = 0;
    char *cache_flag;
    pthread_mutex_lock(&g_ion_mutex_alloc);

    if (g_ion_alloc_context == NULL)
    {
        loge("fatal error! call ion_alloc_open\n");
        goto ALLOC_OUT;
    }

    if (NULL == pAttr || pAttr->nLen <= 0)
    {
        loge("can not alloc size 0\n");
        goto ALLOC_OUT;
    }

    memset(&alloc_data, 0, sizeof(alloc_data));

    alloc_data.len = (size_t)pAttr->nLen;

    IonHeapType eIonHeapType = pAttr->eIonHeapType;

    if(IonHeapType_IOMMU == eIonHeapType)
    {
        alloc_data.heap_id_mask = ION_HEAP_SYSTEM;
    }
    else if(IonHeapType_POOL == eIonHeapType)
    {
        alloc_data.heap_id_mask = ION_HEAP_CUSTOM_START;
    }
    else
    {
        alloc_data.heap_id_mask = ION_HEAP_DMA_START;
    }
    if(pAttr->bSupportCache)
    {
        alloc_data.flags = ION_FLAG_CACHED;
    }

    ret = ioctl(g_ion_alloc_context->fd, ION_IOC_ALLOC, &alloc_data);
    if (ret < 0)
    {
        loge("fatal error! AW_ION_IOC_NEW_ALLOC error\n");
        goto ALLOC_OUT;
    }

    /* mmap to user */
    addr_vir = (unsigned long)mmap(NULL, alloc_data.len, PROT_READ|PROT_WRITE, MAP_SHARED, alloc_data.fd, 0);

    if ((unsigned long)MAP_FAILED == addr_vir)
    {
        loge("fatal error! mmap err, ret %u\n", (unsigned int)addr_vir);
        addr_vir = 0;
        goto ALLOC_OUT;
    }

    /* get phy address */
    memset(&iommu_param, 0, sizeof(iommu_param));
    iommu_param.fd = alloc_data.fd;
//    ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_ENGINE_REQ, 0);
//    if (ret)
//    {
//        loge("fatal error! ENGINE_REQ err, ret %d\n", ret);
//        addr_phy = 0;
//        addr_vir = 0;
//        goto ALLOC_OUT;
//    }
    ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_GET_IOMMU_ADDR, &iommu_param);
    if (ret)
    {
        loge("fatal error! GET_IOMMU_ADDR err, ret %d\n", ret);
        addr_phy = 0;
        addr_vir = 0;
        goto ALLOC_OUT;
    }
    addr_phy = iommu_param.iommu_addr;

    alloc_buffer = (buffer_node *)malloc(sizeof(buffer_node));
    if (alloc_buffer == NULL)
    {
        loge("fatal error! malloc buffer node failed");

        /* unmmap */
        ret = munmap((void*)addr_vir, alloc_data.len);
        if (ret)
        {
            loge("fatal error! munmap err, ret %d\n", ret);
        }

        ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_FREE_IOMMU_ADDR, &iommu_param);
        if (ret)
        {
            loge("fatal error! FREE_IOMMU_ADDR err, ret %d\n", ret);
        }
//        ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_ENGINE_REL, 0);
//        if (ret)
//        {
//            loge("fatal error! ENGINE_REL failed\n");
//        }

        /* close dmabuf fd */
        close(alloc_data.fd);

        addr_phy = 0;
        addr_vir = 0; /* value of MAP_FAILED is -1, should return 0*/

        goto ALLOC_OUT;
    }
    memset(alloc_buffer, 0, sizeof(*alloc_buffer));
    alloc_buffer->eIonHeapType = eIonHeapType;
    alloc_buffer->bSupportCache = pAttr->bSupportCache;
    alloc_buffer->phy = addr_phy;
    alloc_buffer->vir = addr_vir;
    alloc_buffer->user_virt = addr_vir;
    alloc_buffer->size = pAttr->nLen;
    alloc_buffer->fd = alloc_data.fd;

    list_add_tail(&alloc_buffer->i_list, &g_ion_alloc_context->list);

#if DEBUG_ION_REF==1
    cdx_use_mem += pAttr->nLen;
    int i = 0;
    for (i = 0; i < ION_BUF_LEN; i++) {
        if (ion_buf_nodes_test[i].addr == 0
                && ion_buf_nodes_test[i].size == 0) {
            ion_buf_nodes_test[i].addr = addr_vir;
            ion_buf_nodes_test[i].size = pAttr->nLen;
            break;
        }
    }

    if (i>= ION_BUF_LEN) {
        loge("error, ion buf len is large than [%d]", ION_BUF_LEN);
    }
#endif

ALLOC_OUT:
    pthread_mutex_unlock(&g_ion_mutex_alloc);
    return (void*)addr_vir;
}

/* return virtual address: 0 failed */
void* sunxi_ion_alloc_palloc(int size)
{
    IonAllocAttr stAttr;
    memset(&stAttr, 0, sizeof(stAttr));
    stAttr.nLen = size;
#if defined(CONF_KERNEL_IOMMU)
    stAttr.eIonHeapType = IonHeapType_IOMMU;
#elif defined(CONF_AWION_SIZE_POOL_HEAP)
    stAttr.eIonHeapType = IonHeapType_POOL;
#else
    stAttr.eIonHeapType = IonHeapType_CARVEOUT;
#endif
    stAttr.bSupportCache = true;
    return sunxi_ion_alloc_pallocExtend(&stAttr);
}

void sunxi_ion_alloc_pfree(void * pbuf)
{
    int flag = 0;
    unsigned long addr_vir = (unsigned long)pbuf;
    buffer_node *pEntry, *tmp;
    int ret;

    if (NULL == pbuf)
    {
        loge("can not free NULL buffer\n");
        return;
    }

    pthread_mutex_lock(&g_ion_mutex_alloc);

    if (g_ion_alloc_context == NULL)
    {
        loge("fatal error! call ion_alloc_open before ion_alloc_alloc\n");
        pthread_mutex_unlock(&g_ion_mutex_alloc);
        return;
    }

    list_for_each_entry_safe(pEntry, tmp, &g_ion_alloc_context->list, i_list)
    {
        if (pEntry->vir == addr_vir)
        {
            logv("ion_alloc_free item phy =0x%lx vir =0x%lx, size =%d\n", pEntry->phy, pEntry->vir, pEntry->size);
            ion_alloc_pfree_l(pEntry);
            list_del(&pEntry->i_list);
            free(pEntry);

            flag = 1;

#if DEBUG_ION_REF==1
            int i = 0;
            for ( i = 0; i < ION_BUF_LEN; i++) {
                if (ion_buf_nodes_test[i].addr == addr_vir && ion_buf_nodes_test[i].size > 0) {
                    cdx_use_mem -= ion_buf_nodes_test[i].size;
                    logd("--cdx_use_mem = [%d MB], reduce size->[%d B]",\
                        cdx_use_mem/1024/1024, ion_buf_nodes_test[i].size);
                    ion_buf_nodes_test[i].addr = 0;
                    ion_buf_nodes_test[i].size = 0;

                    break;
                }
            }

            if (i >= ION_BUF_LEN) {
                loge("error, ion buf len is large than [%d]", ION_BUF_LEN);
            }
#endif
            break;
        }
    }

    if (0 == flag)
    {
        loge("fatal error! ion_alloc_free failed, do not find virtual address: 0x%lx\n", addr_vir);
    }

    pthread_mutex_unlock(&g_ion_mutex_alloc);
    return;
}

void* sunxi_ion_alloc_vir2phy_cpu(void * pbuf)
{
    int flag = 0;
    unsigned long addr_vir = (unsigned long)pbuf;
    unsigned long addr_phy = 0;
    buffer_node * tmp;

    if (NULL == pbuf)
    {
        loge("fatal error! can not vir2phy NULL buffer\n");
        return NULL;
    }

    pthread_mutex_lock(&g_ion_mutex_alloc);
    list_for_each_entry(tmp, &g_ion_alloc_context->list, i_list)
    {
        if (addr_vir >= tmp->vir && addr_vir < tmp->vir + tmp->size)
        {
            addr_phy = tmp->phy + addr_vir - tmp->vir;
            logv("ion_alloc_vir2phy phy= 0x%08x vir= 0x%08x\n", addr_phy, addr_vir);
            flag = 1;
            break;
        }
    }
    if (0 == flag)
    {
        loge("fatal error! ion_alloc_vir2phy failed,no virtual address: 0x%lx\n", addr_vir);
    }
    pthread_mutex_unlock(&g_ion_mutex_alloc);

    return (void*)addr_phy;
}

void* sunxi_ion_alloc_phy2vir_cpu(void * pbuf)
{
    int flag = 0;
    unsigned long addr_vir = 0;
    unsigned long addr_phy = (unsigned long)pbuf;
    buffer_node * tmp;

    if (NULL == pbuf)
    {
        loge("fatal error! can not phy2vir NULL buffer\n");
        return NULL;
    }

    pthread_mutex_lock(&g_ion_mutex_alloc);
    list_for_each_entry(tmp, &g_ion_alloc_context->list, i_list)
    {
        if (addr_phy >= tmp->phy && addr_phy < tmp->phy + tmp->size)
        {
            addr_vir = tmp->vir + addr_phy - tmp->phy;
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("fatal error! %s failed,can not find phy adr:0x%lx\n", __FUNCTION__, addr_phy);
    }
    pthread_mutex_unlock(&g_ion_mutex_alloc);

    return (void*)addr_vir;
}

/**
  get buffer_node by virtual address.

  not use lock g_ion_mutex_alloc, so caller will guarantee lock.
*/
static buffer_node* ion_alloc_get_buffer_node(void * pbuf)
{
    int flag = 0;
    unsigned long addr_vir = (unsigned long)pbuf;
    buffer_node *pNode;

    if (NULL == pbuf)
    {
        loge("fatal error! buf is NULL!\n");
        return NULL;
    }

    list_for_each_entry(pNode, &g_ion_alloc_context->list, i_list)
    {
        if (addr_vir >= pNode->vir && addr_vir < pNode->vir + pNode->size)
        {
            logv("ion_alloc_get_bufferFd, fd = 0x%08x vir= 0x%08x\n", pNode->fd, addr_vir);
            flag = 1;
            break;
        }
    }
    if (0 == flag)
    {
        loge("fatal error! find buffer node fail,no virtual address: 0x%lx\n", addr_vir);
        pNode = NULL;
    }

    logv("*** get buffer node: %p, flag = %d\n", pNode, flag);
    return pNode;
}

static int __sunxi_ion_alloc_get_bufferFd_priv(void * pbuf, char lock)
{
    int flag = 0;
    unsigned long addr_vir = (unsigned long)pbuf;
    int buffer_fd = -1;
    buffer_node * tmp;

    if (NULL == pbuf)
    {
        loge("fatal error! can not vir2phy NULL buffer\n");
        return -1;
    }

    if (lock)
    {
        pthread_mutex_lock(&g_ion_mutex_alloc);
    }
    list_for_each_entry(tmp, &g_ion_alloc_context->list, i_list)
    {
        if (addr_vir >= tmp->vir && addr_vir < tmp->vir + tmp->size)
        {
            buffer_fd = tmp->fd;
            logv("ion_alloc_get_bufferFd, fd = 0x%08x vir= 0x%08x\n", buffer_fd, addr_vir);
            flag = 1;
            break;
        }
    }
    if (0 == flag)
    {
        loge("fatal error! ion_alloc_vir2phy failed,no virtual address: 0x%lx\n", addr_vir);
    }
    if (lock)
    {
        pthread_mutex_unlock(&g_ion_mutex_alloc);
    }

    logv("*** get_bufferfd: %d, flag = %d\n", buffer_fd, flag);
    return buffer_fd;
}

int sunxi_ion_alloc_get_bufferFd(void * pbuf)
{
    return __sunxi_ion_alloc_get_bufferFd_priv(pbuf, 1);
}

void* sunxi_ion_alloc_get_viraddr_byFd(int nShareFd)
{
    int flag = 0;
    unsigned long addr_vir = 0;
    buffer_node * tmp = NULL;

    pthread_mutex_lock(&g_ion_mutex_alloc);

    list_for_each_entry(tmp, &g_ion_alloc_context->list, i_list)
    {
        if (nShareFd == tmp->fd)
        {
            addr_vir = tmp->vir;
            flag = 1;
            break;
        }
    }

    if (0 == flag)
    {
        loge("fatal error! ion_alloc_phy2vir failed, do not find nShareFd %d \n", nShareFd);
    }

    pthread_mutex_unlock(&g_ion_mutex_alloc);

    return (void*)addr_vir;
}

void sunxi_ion_alloc_flush_cache(void* startAddr, int size)
{
    int ret;
    pthread_mutex_lock(&g_ion_mutex_alloc);
    buffer_node *pNode = ion_alloc_get_buffer_node(startAddr);
    if(pNode != NULL)
    {
        if(IonHeapType_IOMMU == pNode->eIonHeapType)
        {
            struct cache_range range;
            /* clean and invalid user cache */
            range.start = (uint64_t)startAddr;
            range.end = (uint64_t)startAddr + size;
            ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_FLUSH_CACHE_RANGE, &range);
            if (ret)
            {
                loge("fatal error! ION_IOC_SUNXI_FLUSH_RANGE failed\n");
            }
        }
        else
        {
            int nBufFd = pNode->fd;
            struct dma_buf_sync stBufSync;
            memset(&stBufSync, 0, sizeof(stBufSync));
            stBufSync.flags = DMA_BUF_SYNC_VALID_FLAGS_MASK;
            ret = ioctl(nBufFd, DMA_BUF_IOCTL_SYNC, &stBufSync);
            if (ret != 0)
            {
                loge("fatal error! ion_alloc flush cache failed\n");
            }
        }
    }
    else
    {
        loge("fatal error! virAddr[%p] not found in buffer node list\n", startAddr);
    }
    pthread_mutex_unlock(&g_ion_mutex_alloc);
}

void sunxi_ion_flush_cache_all()
{
    //UNUSA_PARAM(sunxi_ion_flush_cache_all);

    //ioctl(g_ion_alloc_context->fd, ION_IOC_SUNXI_FLUSH_ALL, 0);
}

void* sunxi_ion_alloc_alloc_drm(int size)
{
    loge("fatal error! unsupport ion drm!");
    return NULL;
}

/**
  return total mem size allocated by ion_alloc. unit:KB.
*/
int sunxi_ion_alloc_get_total_size()
{
    int nSize = 0;
    buffer_node *pEntry;
    pthread_mutex_lock(&g_ion_mutex_alloc);
    list_for_each_entry(pEntry, &g_ion_alloc_context->list, i_list)
    {
        nSize += pEntry->size;
    }
    logd("ion_alloc total [%fMB]", (float)nSize/1024/1024);
    pthread_mutex_unlock(&g_ion_mutex_alloc);
    return nSize/1024;
}

int sunxi_ion_alloc_memset(void* buf, int value, size_t n)
{
    memset(buf, value, n);
    return -1;
}

int sunxi_ion_alloc_copy(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

int sunxi_ion_alloc_read(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

int sunxi_ion_alloc_write(void* dst, void* src, size_t n)
{
    memcpy(dst, src, n);
    return -1;
}

int sunxi_ion_alloc_setup()
{
    return -1;
}

int sunxi_ion_alloc_shutdown()
{
    return -1;
}

int sunxi_ion_alloc_set_iommu_dev_path (char *dev_path)
{
    if (strlen(dev_path) > 32)
    {
        loge("dev_path:%s is too long\n", dev_path);
        return -1;
    }
    strncpy(iommu_dev_path, dev_path, 32);
    logd("iommu_dev_path has changed to %s", iommu_dev_path);
    return 0;
}

struct SunxiMemOpsS _allocionMemOpsS =
{
    open:               sunxi_ion_alloc_open,
    close:              sunxi_ion_alloc_close,
    total_size:         sunxi_ion_alloc_get_total_size,
    palloc:             sunxi_ion_alloc_palloc,
    pallocExtend:       sunxi_ion_alloc_pallocExtend,
    pfree:              sunxi_ion_alloc_pfree,
    flush_cache:        sunxi_ion_alloc_flush_cache,
    cpu_get_phyaddr:    sunxi_ion_alloc_vir2phy_cpu,
    cpu_get_viraddr:    sunxi_ion_alloc_phy2vir_cpu,
    mem_set:            sunxi_ion_alloc_memset,
    mem_cpy:            sunxi_ion_alloc_copy,
    mem_read:           sunxi_ion_alloc_read,
    mem_write:          sunxi_ion_alloc_write,
    setup:              sunxi_ion_alloc_setup,
    shutdown:           sunxi_ion_alloc_shutdown,
    palloc_secure:      sunxi_ion_alloc_alloc_drm,
    get_bufferFd:       sunxi_ion_alloc_get_bufferFd,
    get_viraddr_byFd:   sunxi_ion_alloc_get_viraddr_byFd,
    set_iommu_dev_path: sunxi_ion_alloc_set_iommu_dev_path,
    pclose_fd_byViraddr:NULL,
    merge:              NULL,
    unmerge:            NULL
};

struct SunxiMemOpsS* GetMemAdapterOpsS()
{
    logd("*** get __GetIonMemOpsS ***");

    return &_allocionMemOpsS;
}

