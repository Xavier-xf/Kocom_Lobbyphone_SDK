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

#include <errno.h>
#include <stdbool.h>
#include "ion_alloc.h"
#include "base_list.h"
#include <ion_mem_alloc.h>

#include <linux/ion_uapi.h>
#include <linux/sunxi_ion_uapi.h>
#include <linux/cedar_ve_uapi.h>

#define CONFIG_LOG_LEVEL    OPTION_LOG_LEVEL_DETAIL
#define LOG_TAG "ionAlloc"
#define DEBUG_ION_REF           0
#define ION_ALLOC_ALIGN         SZ_4k
#define DEV_NAME                "/dev/ion"
#define DEFAULT_IOMMU_DEV_NAME              "/dev/cedar_dev"
#define ION_IOC_SUNXI_POOL_INFO 10
#define UNUSA_PARAM(param) (void)param

#define loge(fmt, arg...) fprintf(stderr, fmt "\n", ##arg)
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

struct sunxi_pool_info {
        unsigned int total; /*unit kb*/
        unsigned int free_kb;  /* size kb */
        unsigned int free_mb;  /* size mb */
};

typedef struct BUFFER_NODE {
    struct list_head i_list;
    IonHeapType eIonHeapType;
    bool bSupportCache;
    unsigned long phy; /*phisical address*/
    unsigned long vir; /*virtual address*/
    unsigned int size; /*buffer size*/
    unsigned int tee;
    unsigned long user_virt;
    struct ion_fd_data fd_data;
    unsigned int fd_closed;
} buffer_node;

typedef struct ION_ALLOC_CONTEXT {
    int fd;
    int iommu_dev_fd;
    struct list_head list; /* buffer list for buffer_node */
    int ref_cnt; /* reference count */
} ion_alloc_context;

//struct user_iommu_param {
//    int fd;
//    unsigned int iommu_addr;
//};

//struct user_merge_iommu_param {
//    unsigned int iommu_addr;
//    int num;
//    int fds[1024];
//};

char iommu_dev_path[32] = DEFAULT_IOMMU_DEV_NAME;

ion_alloc_context *g_ion_alloc_context = NULL;
pthread_mutex_t g_ion_mutex_alloc = PTHREAD_MUTEX_INITIALIZER;

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
    g_ion_alloc_context->fd = open(DEV_NAME, O_RDONLY, 0);
    if (g_ion_alloc_context->fd <= 0)
    {
        loge("open %s failed\n", DEV_NAME);
        goto ERROR_OUT;
    }

    g_ion_alloc_context->iommu_dev_fd = open(iommu_dev_path, O_RDONLY, 0);
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

    if(pNode->eIonHeapType == IonHeapType_IOMMU)
    {
        struct user_iommu_param iommu_param;
        memset(&iommu_param, 0, sizeof(iommu_param));
        iommu_param.fd = pNode->fd_data.fd;
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
    }

    if (0 == pNode->fd_closed)
    {
        /*close dma buffer fd*/
        close(pNode->fd_data.fd);
        pNode->fd_data.fd = -1;
    }
    struct ion_handle_data handle_data;
    /* free buffer */
    handle_data.handle = pNode->fd_data.handle;
    ret = ioctl(g_ion_alloc_context->fd, ION_IOC_FREE, &handle_data);
    if (ret)
    {
        loge("fatal error! ION_IOC_FREE failed\n");
    }
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
    struct ion_fd_data fd_data;
    struct ion_handle_data handle_data;
    struct ion_custom_data custom_data;
    sunxi_phys_data   phys_data;
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
    memset(&fd_data, 0, sizeof(fd_data));

    alloc_data.len = (size_t)pAttr->nLen;
    alloc_data.align = (size_t)ION_ALLOC_ALIGN;

    IonHeapType eIonHeapType = pAttr->eIonHeapType;

    if(IonHeapType_IOMMU == eIonHeapType)
    {
        alloc_data.heap_id_mask = ION_HEAP_SYSTEM_MASK | ION_HEAP_CARVEOUT_MASK;
    }
    else
    {
        alloc_data.heap_id_mask = ION_HEAP_TYPE_DMA_MASK | ION_HEAP_CARVEOUT_MASK;
    }
    if(pAttr->bSupportCache)
    {
        alloc_data.flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
    }

    ret = ioctl(g_ion_alloc_context->fd, ION_IOC_ALLOC, &alloc_data);
    if (ret)
    {
        loge("fatal error! ION_IOC_ALLOC error\n");
        goto ALLOC_OUT;
    }

    /* get dmabuf fd */
    fd_data.handle = alloc_data.handle;
    ret = ioctl(g_ion_alloc_context->fd, ION_IOC_MAP, &fd_data);
    if (ret)
    {
        loge("fatal error! ION_IOC_MAP err, ret %d, dmabuf fd 0x%08x\n", ret, (unsigned int)fd_data.fd);
        goto ALLOC_OUT;
    }

    /* mmap to user */
    addr_vir = (unsigned long)mmap(NULL, alloc_data.len, PROT_READ|PROT_WRITE, MAP_SHARED, fd_data.fd, 0);

    if ((unsigned long)MAP_FAILED == addr_vir)
    {
        loge("fatal error! mmap err, ret %u\n", (unsigned int)addr_vir);
        addr_vir = 0;
        goto ALLOC_OUT;
    }

    /* get phy address */
    if(IonHeapType_IOMMU == eIonHeapType)
    {
        memset(&iommu_param, 0, sizeof(iommu_param));
        iommu_param.fd = fd_data.fd;
//        ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_ENGINE_REQ, 0);
//        if (ret)
//        {
//            loge("fatal error! ENGINE_REQ err, ret %d\n", ret);
//            addr_phy = 0;
//            addr_vir = 0;
//            goto ALLOC_OUT;
//        }
        ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_GET_IOMMU_ADDR, &iommu_param);
        if (ret)
        {
            loge("fatal error! GET_IOMMU_ADDR err, ret %d\n", ret);
            addr_phy = 0;
            addr_vir = 0;
            goto ALLOC_OUT;
        }
        addr_phy = iommu_param.iommu_addr;
    }
    else
    {
        memset(&phys_data, 0, sizeof(phys_data));
        phys_data.handle = alloc_data.handle;
        phys_data.size = pAttr->nLen;
        custom_data.cmd = ION_IOC_SUNXI_PHYS_ADDR;
        custom_data.arg = (unsigned long)&phys_data;
        ret = ioctl(g_ion_alloc_context->fd, ION_IOC_CUSTOM, &custom_data);
        if (ret)
        {
            loge("fatal error! ION_IOC_CUSTOM err, ret %d\n", ret);
            addr_phy = 0;
            addr_vir = 0;
            goto ALLOC_OUT;
        }
        addr_phy = phys_data.phys_addr;
    }

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

        if(IonHeapType_IOMMU == eIonHeapType)
        {
            ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_FREE_IOMMU_ADDR, &iommu_param);
            if (ret)
            {
                loge("fatal error! FREE_IOMMU_ADDR err, ret %d\n", ret);
            }
//            ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_ENGINE_REL, 0);
//            if (ret)
//            {
//                loge("fatal error! ENGINE_REL failed\n");
//            }
        }

        /* close dmabuf fd */
        close(fd_data.fd);

        /* free buffer */
        handle_data.handle = alloc_data.handle;
        ret = ioctl(g_ion_alloc_context->fd, ION_IOC_FREE, &handle_data);
        if(ret)
        {
            loge("fatal error! ION_IOC_FREE err, ret %d\n", ret);
        }

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
    alloc_buffer->fd_data.handle = fd_data.handle;
    alloc_buffer->fd_data.fd = fd_data.fd;
    alloc_buffer->fd_closed = 0;

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
    struct ion_handle_data handle_data;

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
            buffer_fd = tmp->fd_data.fd;
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
        if (nShareFd == tmp->fd_data.fd)
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

#if !defined(CONF_KERNEL_VERSION_3_4)
void sunxi_ion_alloc_flush_cache(void* startAddr, int size)
{
    struct sunxi_cache_range range;
    int ret;

    /* clean and invalid user cache */
    range.start = (long)startAddr;
    range.end = (long)startAddr + size;

    ret = ioctl(g_ion_alloc_context->fd, ION_IOC_SUNXI_FLUSH_RANGE, &range);
    if (ret)
    {
        loge("fatal error! ION_IOC_SUNXI_FLUSH_RANGE failed\n");
    }

    return;
}
#else
void sunxi_ion_alloc_flush_cache(void* startAddr, int size)
{
    struct sunxi_cache_range range;
    struct ion_custom_data custom_data;
    int ret;

    /* clean and invalid user cache */
    range.start = (unsigned long)startAddr;
    range.end = (unsigned long)startAddr + size;

    custom_data.cmd = ION_IOC_SUNXI_FLUSH_RANGE;
    custom_data.arg = (unsigned long)&range;

    ret = ioctl(g_ion_alloc_context->fd, ION_IOC_CUSTOM, &custom_data);
    if (ret)
        loge("ION_IOC_CUSTOM failed\n");

    return;
}
#endif

void sunxi_ion_flush_cache_all()
{
    UNUSA_PARAM(sunxi_ion_flush_cache_all);

    ioctl(g_ion_alloc_context->fd, ION_IOC_SUNXI_FLUSH_ALL, 0);
}

void* sunxi_ion_alloc_alloc_drm(int size)
{
    struct ion_allocation_data alloc_data;
    struct ion_fd_data fd_data;
    struct ion_handle_data handle_data;
    struct ion_custom_data custom_data;
    sunxi_phys_data   phys_data, tee_data;

    int rest_size = 0;
    unsigned long addr_phy = 0;
    unsigned long addr_vir = 0;
    unsigned long addr_tee = 0;
    buffer_node * alloc_buffer = NULL;
    int ret = 0;

    pthread_mutex_lock(&g_ion_mutex_alloc);

    if (g_ion_alloc_context == NULL)
    {
        loge("fatal error! should call ion_alloc_open\n");
        goto ALLOC_OUT;
    }

    if (size <= 0)
    {
        loge("fatal error! can not alloc size 0\n");
        goto ALLOC_OUT;
    }

    /*alloc buffer*/
    alloc_data.len = size;
    alloc_data.align = ION_ALLOC_ALIGN;
    alloc_data.heap_id_mask = (1 << ION_HEAP_TYPE_SECURE);
    alloc_data.flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
    ret = ioctl(g_ion_alloc_context->fd, ION_IOC_ALLOC, &alloc_data);
    if (ret)
    {
        loge("fatal error! ION_IOC_ALLOC error%s\n", strerror(errno));
        goto ALLOC_OUT;
    }

    /* get dmabuf fd */
    fd_data.handle = alloc_data.handle;
    ret = ioctl(g_ion_alloc_context->fd, ION_IOC_MAP, &fd_data);
    if (ret)
    {
        loge("fatal error! ION_IOC_MAP err, ret %d, dmabuf fd 0x%08x\n", ret, (unsigned int)fd_data.fd);
        goto ALLOC_OUT;
    }

    /* mmap to user */
    addr_vir = (unsigned long)mmap(NULL, alloc_data.len, PROT_READ|PROT_WRITE, MAP_SHARED, fd_data.fd, 0);
    if ((unsigned long)MAP_FAILED == addr_vir)
    {
        addr_vir = 0;
        goto ALLOC_OUT;
    }

    /* get phy address */
    memset(&phys_data, 0, sizeof(phys_data));
    phys_data.handle = alloc_data.handle;
    phys_data.size = size;
    custom_data.cmd = ION_IOC_SUNXI_PHYS_ADDR;
    custom_data.arg = (unsigned long)&phys_data;

    ret = ioctl(g_ion_alloc_context->fd, ION_IOC_CUSTOM, &custom_data);
    if (ret)
    {
        loge("fatal error! ION_IOC_CUSTOM err, ret %d\n", ret);
        addr_phy = 0;
        addr_vir = 0;
        goto ALLOC_OUT;
    }

    addr_phy = phys_data.phys_addr;
#if(ADJUST_ADDRESS_FOR_SECURE_OS_OPTEE)
    memset(&tee_data, 0, sizeof(tee_data));
    tee_data.handle = alloc_data.handle;
    tee_data.size = size;
    custom_data.cmd = ION_IOC_SUNXI_TEE_ADDR;
    custom_data.arg = (unsigned long)&tee_data;
    ret = ioctl(g_ion_alloc_context->fd, AW_MEM_ION_IOC_CUSTOM, &custom_data);
    if (ret)
    {
        loge("fatal error! ION_IOC_CUSTOM err, ret %d\n", ret);
        addr_phy = 0;
        addr_vir = 0;
        goto ALLOC_OUT;
    }
    addr_tee = tee_data.phys_addr;
#else
    addr_tee = addr_vir;
#endif
    alloc_buffer = (buffer_node *)malloc(sizeof(buffer_node));
    if (alloc_buffer == NULL)
    {
        loge("fatal error! malloc buffer node failed");

        /* unmmap */
        ret = munmap((void*)addr_vir, alloc_data.len);
        if(ret)
        {
            loge("fatal error! munmap err, ret %d\n", ret);
        }

        /* close dmabuf fd */
        close(fd_data.fd);

        /* free buffer */
        handle_data.handle = alloc_data.handle;
        ret = ioctl(g_ion_alloc_context->fd, ION_IOC_FREE, &handle_data);
        if (ret)
        {
            loge("fatal error! ION_IOC_FREE err, ret %d\n", ret);
        }
        addr_phy = 0;
        addr_vir = 0;

        goto ALLOC_OUT;
    }

    alloc_buffer->eIonHeapType = IonHeapType_CARVEOUT;
    alloc_buffer->bSupportCache = true;
    alloc_buffer->size = size;
    alloc_buffer->phy = addr_phy;
    alloc_buffer->user_virt = addr_vir;
    alloc_buffer->vir = addr_tee;
    alloc_buffer->tee = addr_tee;
    alloc_buffer->fd_data.handle = fd_data.handle;
    alloc_buffer->fd_data.fd = fd_data.fd;
    list_add_tail(&alloc_buffer->i_list, &g_ion_alloc_context->list);

    ALLOC_OUT:

    pthread_mutex_unlock(&g_ion_mutex_alloc);

    return (void*)addr_tee;
}

struct sunxi_pool_info binfo = {
    .total   = 0,
    .free_kb  = 0,
    .free_mb = 0,
};

/*return total meminfo with MB */
int sunxi_ion_alloc_get_total_size()
{
    int ret = 0;
    int ion_fd = open(DEV_NAME, O_WRONLY);
    struct ion_custom_data cdata;

    if (ion_fd < 0) {
        loge("open ion dev failed, cannot get ion mem.");
        goto err;
    }

    cdata.cmd = ION_IOC_SUNXI_POOL_INFO;
    cdata.arg = (unsigned long)&binfo;
    ret = ioctl(ion_fd,ION_IOC_CUSTOM, &cdata);
    if (ret < 0) {
        loge("Failed to ioctl ion device, errno:%s\n", strerror(errno));
        goto err;
    }

    logd(" ion dev get free pool [%d MB], total [%d MB]\n",
            binfo.free_mb, binfo.total / 1024);
    ret = binfo.total;
err:
    if (ion_fd > 0)
        close(ion_fd);

    return ret;
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

void sunxi_ion_alloc_close_fd_byViraddr(void * pbuf)
{
    int flag = 0;
    unsigned long addr_vir = (unsigned long)pbuf;
    buffer_node * tmp;
    int ret;
    struct ion_handle_data handle_data;

    if (0 == pbuf)
    {
        loge("fatal error! can not free NULL buffer\n");
        return;
    }

    pthread_mutex_lock(&g_ion_mutex_alloc);

    if (g_ion_alloc_context == NULL)
    {
        loge("fatal error! call ion_alloc_open before ion_alloc_alloc\n");
        pthread_mutex_unlock(&g_ion_mutex_alloc);
        return ;
    }

    list_for_each_entry(tmp, &g_ion_alloc_context->list, i_list)
    {
        if (tmp->vir == addr_vir)
        {
            logv("ion_alloc_free item phy =0x%lx vir =0x%lx, size =%d, aw_fd=%d\n", tmp->phy, tmp->vir, tmp->size, tmp->fd_data.fd);

            /*close dma buffer fd*/
            close(tmp->fd_data.fd);
            //tmp->fd_data.fd = -1;
            tmp->fd_closed = 1;

            break;
        }
    }

    pthread_mutex_unlock(&g_ion_mutex_alloc);
    return ;
}

void* sunxi_ion_alloc_merge(int num, void *virt[])
{
    int ret = 0;
    int i = 0;
    unsigned int addr_phy = 0;

    pthread_mutex_lock(&g_ion_mutex_alloc);

//    ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_ENGINE_REQ, 0);
//    if (ret < 0)
//    {
//        loge("fatal error! AW_MEM_ENGINE_REQ failed \n");
//        goto EXIT;
//    }
    struct user_merge_iommu_param stUserMergeIommuBuf;
    memset(&stUserMergeIommuBuf, 0, sizeof(struct user_merge_iommu_param));
    stUserMergeIommuBuf.num = num;
    for (i = 0; i < num; i++)
    {
        stUserMergeIommuBuf.fds[i] = __sunxi_ion_alloc_get_bufferFd_priv(virt[i], 0);
    }
    logd("AW_MEM_MERGE_IOMMU_ADDR=0x%x, num=%d\n", IOCTL_MERGE_IOMMU_ADDR, num);
    ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_MERGE_IOMMU_ADDR, &stUserMergeIommuBuf);
    if (ret < 0)
    {
        loge("fatal error! AW_MEM_MERGE_IOMMU_ADDR failed ret=%d\n", ret);
//        ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_ENGINE_REL, 0);
//        if (ret < 0)
//        {
//            loge("fatal error! AW_MEM_ENGINE_REL failed \n");
//        }
        goto EXIT;
    }
    addr_phy = stUserMergeIommuBuf.iommu_addr;

EXIT:
    pthread_mutex_unlock(&g_ion_mutex_alloc);
    return (void*)addr_phy;
}

int sunxi_ion_alloc_unmerge(void *phy)
{
    int ret = 0;

    pthread_mutex_lock(&g_ion_mutex_alloc);

    struct user_merge_iommu_param stUserMergeIommuBuf;
    memset(&stUserMergeIommuBuf, 0, sizeof(struct user_merge_iommu_param));
    stUserMergeIommuBuf.iommu_addr = (unsigned int)phy;
    ret = ioctl(g_ion_alloc_context->iommu_dev_fd, IOCTL_UNMERGE_IOMMU_ADDR, &stUserMergeIommuBuf);
    if (ret < 0)
    {
        loge("fatal error! AW_MEM_UNMERGE_IOMMU_ADDR failed \n");
    }

    pthread_mutex_unlock(&g_ion_mutex_alloc);

    return ret;
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
    pclose_fd_byViraddr:sunxi_ion_alloc_close_fd_byViraddr,
    merge:              sunxi_ion_alloc_merge,
    unmerge:            sunxi_ion_alloc_unmerge
};

struct SunxiMemOpsS* GetMemAdapterOpsS()
{
    logd("*** get __GetIonMemOpsS ***");

    return &_allocionMemOpsS;
}
