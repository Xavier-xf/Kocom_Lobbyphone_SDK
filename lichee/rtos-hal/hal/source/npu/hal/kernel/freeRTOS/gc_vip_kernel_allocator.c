/******************************************************************************\
|* Copyright (c) 2017-2023 by Vivante Corporation.  All Rights Reserved.      *|
|*                                                                            *|
|* The material in this file is confidential and contains trade secrets of    *|
|* of Vivante Corporation.  This is proprietary information owned by Vivante  *|
|* Corporation.  No part of this work may be disclosed, reproduced, copied,   *|
|* transmitted, or used in any way for any purpose, without the express       *|
|* written permission of Vivante Corporation.                                 *|
|*                                                                            *|
\******************************************************************************/
#include "gc_vip_kernel_allocator.h"
#include "gc_vip_kernel_drv.h"
#include <hal_cache.h>
#include <hal_mem.h>

int size_total_allocator = 0;

#if vpmdENABLE_MMU

/******* These functions need to be implemented when MMU is enable ************/

/*
@brief get user space logical address.
@param, context, the contect of kernel space.
@param, node, dynamic allocate node info.
@param, virtual_addr, the VIP's virtual address.
@param, logical, use space logical address.
@param, video memory type see gckvip_video_memory_type_e
*/
vip_status_e gckvip_allocator_get_userlogical(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node,
    vip_uint32_t virtual_addr,
    void **logical,
    vip_uint8_t memory_type
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_int64_t offset = virtual_addr - node->vip_virtual_address;

    if ((node->user_logical != VIP_NULL) && (offset > 0)) {
        *logical = node->user_logical + offset;
    }

    return status;
}

/*
@brief get kernel space logical address.
@param, context, the contect of kernel space.
@param, node, dynamic allocate node info.
@param, virtual_addr, the VIP's virtual address.
@param, logical, kernel space logical address.
@param, video memory type see gckvip_video_memory_type_e
*/
vip_status_e gckvip_allocator_get_kernellogical(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node,
    vip_uint32_t virtual_addr,
    void **logical,
    vip_uint8_t memory_type
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_int64_t offset = virtual_addr - node->vip_virtual_address;

    if ((node->kerl_logical != VIP_NULL) && (offset > 0)) {
        *logical = node->kerl_logical + offset;
    }

    return status;
}

/*
@brief get vip access physical address.
@param, context, the contect of kernel space.
@param, node, dynamic allocate node info.
@param IN vip_virtual, the vitual address of VIP.
@param, physical, VIP access physical address.
@param, video memory type see gckvip_video_memory_type_e
*/
vip_status_e gckvip_allocator_get_vipphysical(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node,
    vip_uint32_t vip_virtual,
    phy_address_t *physical,
    vip_uint8_t memory_type
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_int64_t offset = vip_virtual - node->vip_virtual_address;

    *physical = node->physical_table[0] + offset;

    return status;
}
#endif

/*
@brief convert user's memory to CPU physical. And map to VIP virtual address. for vip_create_buffer_from_handle() API
@param, context, the contect of kernel space.
@param, logical, the logical address(handle) should be wraped.
@param, memory_type The type of this VIP buffer memory. see vip_buffer_memory_type_e.
@param, node, dynamic allocate node info.
*/
vip_status_e gckvip_allocator_wrap_usermemory(
    gckvip_context_t *context,
    vip_ptr logical,
    vip_uint32_t memory_type,
    gckvip_dyn_allocate_node_t *node
    )
{
    vip_status_e status = VIP_SUCCESS;

    PRINTK_D("wrap usermemory logical=0x%"PRPx", size=0x%x, memory_type=0x%x\n",
              logical, node->size, memory_type);

    gcOnError(gckvip_os_allocate_memory(sizeof(phy_address_t), (void **)&node->physical_table));
    gcOnError(gckvip_os_allocate_memory(sizeof(vip_uint32_t), (void **)&node->size_table));

    /* fill physical_table/size_table/physical_num */
    /* physical is equal to logical on RTOS and physical is contiguous */
    node->physical_table[0] = (phy_address_t)logical;
    node->size_table[0] = node->size;
    node->physical_num = 1;

    node->kerl_logical = logical;
    node->user_logical = logical;

    return status;

onError:
    PRINTK_E("failed to wrap user memory, status=%d\n", status);
    if (node->size_table != VIP_NULL) {
       gckvip_os_free_memory(node->size_table);
    }
    if (node->physical_table != VIP_NULL) {
        gckvip_os_free_memory(node->physical_table);
    }

    return status;
}

/*
@brief un-wrap user memory(handle). for vip_create_buffer_from_handle() API
@param context, the contect of kernel space.
@param, node, dynamic allocate node info.
*/
vip_status_e gckvip_allocator_unwrap_usermemory(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node
    )
{
    vip_status_e status = VIP_SUCCESS;

    if (node->size_table != VIP_NULL) {
       gckvip_os_free_memory(node->size_table);
    }
    if (node->physical_table != VIP_NULL) {
        gckvip_os_free_memory(node->physical_table);
    }

    return status;
}

/*
@brief convert cpu's physical to vip physical.
@param IN context, the contect of kernel space.
@param IN physical_table Physical address table. should be wraped for VIP hardware.
@param IN size_table The size of physical memory for each physical_table element.
@param IN physical_num The number of physical table element.
@param, node, dynamic allocate node info.
@param IN alloc_flag the flag of this video memroy. see gckvip_video_mem_alloc_flag_e.
*/
vip_status_e gckvip_allocator_wrap_userphysical(
    gckvip_context_t *context,
    vip_address_t *physical_table,
    vip_uint32_t *size_table,
    vip_uint32_t physical_num,
    gckvip_dyn_allocate_node_t *node,
    vip_uint32_t alloc_flag
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_uint32_t i = 0;

    gcOnError(gckvip_os_allocate_memory(sizeof(phy_address_t) * physical_num, (void**)&node->physical_table));
    gcOnError(gckvip_os_allocate_memory(sizeof(vip_uint32_t) * physical_num, (void**)&node->size_table));

    for (i = 0; i < physical_num; i++) {
        node->physical_table[i] = gckvip_drv_get_vipphysical(physical_table[i]);
        node->size_table[i] = size_table[i];
    }

    node->physical_num = physical_num;
    if (1 == physical_num) {
        node->kerl_logical = (vip_uint8_t *)(physical_table[0]);
        node->user_logical = node->kerl_logical;
    }
    node->mem_flag |= GCVIP_MEM_FLAG_NONE_CACHE;

onError:

    return status;
}

/*
@brief un-wrap user physical.
@param, context, the contect of kernel space.
@param, node, dynamic allocate node info.
*/
vip_status_e gckvip_allocator_unwrap_userphysical(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node
    )
{
    if (node->physical_table != VIP_NULL) {
        gckvip_os_free_memory(node->physical_table);
        node->physical_table = VIP_NULL;
    }

    if (node->size_table != VIP_NULL) {
        gckvip_os_free_memory(node->size_table);
        node->physical_table = VIP_NULL;
    }

    return VIP_SUCCESS;
}

/*
@brief Flush CPU cache for dynamic alloc video memory and wrap user memory.
@param context, the contect of kernel space.
@param node, dynamic allocate node info.
@param physical, the physical address should be flush CPU cache.
@param logical, the logical address should be flush.
@param size, the size of the memory should be flush.
@param type The type of operate cache.
*/
#if vpmdENABLE_FLUSH_CPU_CACHE
vip_status_e gckvip_alloctor_flush_cache(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node,
    phy_address_t physical,
    void *logical,
    vip_uint32_t size,
    gckvip_cache_type_e type
    )
{
    vip_status_e status = VIP_SUCCESS;

    if (node->mem_flag & GCVIP_MEM_FLAG_NONE_CACHE) {
        return VIP_SUCCESS;
    }

    PRINTK_D("allocator flush cache... %s, handle=0x%"PRPx", memory_flag=0x%x,"
             "mem_flag=0x%x, flush_hanlde=0x%"PRPx", type=%s\n",
             (node->mem_flag & GCVIP_MEM_FLAG_CONTIGUOUS) ? "contiguous": "non-contiguous",
             node->alloc_handle, node->mem_flag, node->mem_flag, node->flush_cache_handle,
             (GCKVIP_CACHE_CLEAN == type) ? "CLEAN" : (GCKVIP_CACHE_FLUSH == type) ? "FLUSH" : "INVALID");

    if (node->flush_cache_handle != VIP_NULL) {
        if (node->mem_flag & GCVIP_MEM_FLAG_CONTIGUOUS) {
            switch (type) {
            case GCKVIP_CACHE_CLEAN:
            {
                PRINTK_D("GCKVIP_CACHE_CLEAN\n");
                hal_dcache_clean((unsigned long)physical,(unsigned long)(size));
            }
            break;

            case GCKVIP_CACHE_FLUSH:
            {
                PRINTK_D("GCKVIP_CACHE_FLUSH\n");
                hal_dcache_clean_invalidate((unsigned long)physical,(unsigned long)(size));
            }
            break;

            case GCKVIP_CACHE_INVALID:
            {
                PRINTK_D("GCKVIP_CACHE_INVALID\n");
                hal_dcache_invalidate((unsigned long)physical,(unsigned long)(size));
            }
            break;

            default:
                PRINTK_E("not support this flush cache type=%d\n", type);
                break;
            }
        }
        else {
            /* not supports wrap physical memory flush cache */
            struct sg_table *sgt = (struct sg_table *)node->flush_cache_handle;
            switch (type) {
            case GCKVIP_CACHE_CLEAN:
            {
                hal_dcache_clean((unsigned long)physical,(unsigned long)(size));
            }
            break;

            case GCKVIP_CACHE_FLUSH:
            {
                hal_dcache_clean_invalidate((unsigned long)physical,(unsigned long)(size));
            }
            break;

            case GCKVIP_CACHE_INVALID:
            {
                hal_dcache_invalidate((unsigned long)physical,(unsigned long)(size));
            }
            break;

            default:
                PRINTK_E("not support this flush cache type=%d\n", type);
                break;
            }
        }
    }
    else {
        PRINTK_E("alloctor_flush_cache flush handle is NULL\n");
        status = VIP_ERROR_FAILURE;
    }

    return status;

}
#endif

/*
@brief allocate memory from system.
       get the physical address table each page.
       get size table of eache page
@param context, the contect of kernel space.
@param node, dynamic allocate node.
@param align, the size of alignment for this video memory.
@param flag the flag of this video memroy. see gckvip_video_mem_alloc_flag_e.
@param alloc_memory
*/
static vip_status_e gckvip_allocator_dyn_alloc_ext(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node,
    vip_uint32_t align,
    vip_uint32_t alloc_flag
    )
{
    void *alloc_memory = VIP_NULL;
    vip_status_e status = VIP_SUCCESS;
    size_total_allocator += node->size;
    PRINTK_D("malloc size = %d, size_total_allocator = %d\n", node->size, size_total_allocator);
    alloc_memory = hal_malloc_align(node->size, align);
    PRINTK_D("alloc_memory_allocator = 0x%"PRIx64"\n", (phy_address_t)alloc_memory);
    if (!alloc_memory) {
        return VIP_ERROR_OUT_OF_MEMORY;
    }

    gcOnError(gckvip_os_allocate_memory(sizeof(phy_address_t), (void **)&node->physical_table));
    gcOnError(gckvip_os_allocate_memory(sizeof(vip_uint32_t), (void **)&node->size_table));
    gckvip_os_allocate_memory(sizeof(phy_address_t), (void**)&node->flush_cache_handle);
    if (!node->flush_cache_handle) {
        PRINTK_E("contiguous failed to alloca memory for flush cache handle\n");
        gcGoOnError(VIP_ERROR_IO);
    }

    /* typedef unsigned long long  vip_address_t; */
    node->physical_table[0] = (phy_address_t)alloc_memory;
    /*PRINTK_D("node->physical_table = 0x%"PRIx64"\n", node->physical_table[0]);*/
    node->size_table[0] = node->size;
    node->physical_num = 1;

    node->kerl_logical = alloc_memory;
    node->user_logical = alloc_memory;

    PRINTK_D("dny alloc kernel address=0x%"PRPx", fir_physical=0x%"PRIx64", "
             "user address=0x%"PRPx", size=0x%d, flag=0x%x\n",
             node->kerl_logical, node->physical_table[0],
             node->user_logical, node->size, node->mem_flag);

    return status;

onError:
    PRINTK_E("failed to allocator_dyn_alloc memory, status=%d\n", status);
    if (node->size_table != VIP_NULL) {
       gckvip_os_free_memory(node->size_table);
    }
    if (node->physical_table != VIP_NULL) {
        gckvip_os_free_memory(node->physical_table);
    }
#if vpmdENABLE_FLUSH_CPU_CACHE
    if (node->flush_cache_handle != VIP_NULL) {
        gckvip_os_free_memory((void*)node->flush_cache_handle);
        node->flush_cache_handle = VIP_NULL;
    }
#endif

    return status;
}

static vip_status_e gckvip_allocator_dyn_free_ext(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node
    )
{
    vip_status_e status = VIP_SUCCESS;
    struct page *pages;
    size_t num_pages = 0;

    if ((VIP_NULL == context) || (VIP_NULL == node)) {
        PRINTK_E("failed free contiguous dyn free, parameter is NULL\n");
        return VIP_ERROR_IO;
    }
#if vpmdENABLE_FLUSH_CPU_CACHE
    if (node->flush_cache_handle != VIP_NULL) {
        gckvip_os_free_memory((void*)node->flush_cache_handle);
        node->flush_cache_handle = VIP_NULL;
    }
#endif
    /* free memory */
    hal_free_align((void *)node->kerl_logical);
    node->alloc_handle = VIP_NULL;
    if (node->physical_table != VIP_NULL) {
        gckvip_os_free_memory(node->physical_table);
        node->physical_table = VIP_NULL;
    }
    if (node->size_table != VIP_NULL) {
        gckvip_os_free_memory(node->size_table);
        node->physical_table = VIP_NULL;
    }
    return status;

onError:
    return status;
}

/*
@brief allocate memory from system. get the physical address table each page. get size table of eache page
@param context, the contect of kernel space.
@param node, dynamic allocate noe.
@param align, the size of alignment for this video memory.
@param flag the flag of this video memroy. see gckvip_video_mem_alloc_flag_e.
*/
vip_status_e gckvip_allocator_dyn_alloc(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node,
    vip_uint32_t align,
    vip_uint32_t alloc_flag
    )
{
    vip_status_e status = VIP_ERROR_OUT_OF_RESOURCE;

    /* fill physical_table/size_table/physical_num */
    PRINTK_D("video memory dynamic alloc size=0x%x\n", node->size);
    node->alloc_flag = alloc_flag;
    node->mem_flag = GCVIP_VIDEO_MEM_ALLOC_NONE;
    if (0 == node->size) {
        PRINTK_E("fail to dynamic alloc video memory size is 0\n");
        GCKVIP_DUMP_STACK();
        gcGoOnError(VIP_ERROR_INVALID_ARGUMENTS);
    }

#if vpmdENABLE_MMU
    status = gckvip_allocator_dyn_alloc_ext(context, node, align, alloc_flag);
    if (status != VIP_SUCCESS) {
        PRINTK_E("failed to dynamic alloc video memory size=%dbytes, status=%d\n", node->size, status);
        gcGoOnError(status);
    }
#else
    /* alloca contiguous physical memory, mmu is disabled */
    status = gckvip_allocator_dyn_alloc_ext(context, node, align, alloc_flag);
    if (status != VIP_SUCCESS) {
        PRINTK_E("failed to dynamic alloc video memory size=%dbytes, status=%d\n", node->size, status);
        gcGoOnError(status);
    }
    else {
        node->mem_flag |= GCVIP_MEM_FLAG_CONTIGUOUS;
    }
#endif

onError:
    return status;
}

/*
@brief free a dynamic allocate memory.
@param context, the contect of kernel space.
@param node, dynamic allocate node info.
*/
vip_status_e gckvip_allocator_dyn_free(
    gckvip_context_t *context,
    gckvip_dyn_allocate_node_t *node
    )
{
    vip_status_e status = VIP_ERROR_OUT_OF_RESOURCE;

#if vpmdENABLE_MMU
    status = gckvip_allocator_dyn_free_ext(context, node);
    if (status != VIP_SUCCESS) {
        PRINTK_E("failed to free dyn memory, status=%d\n", status);
    }
#else
    /* contiguous memory */
    status = gckvip_allocator_dyn_free_ext(context, node);
    if (status != VIP_SUCCESS) {
        PRINTK_E("failed to free dyn memory, status=%d\n", status);
    }
#endif

    return status;
}

