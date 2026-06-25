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
#include "FreeRTOS.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "gc_vip_kernel_port.h"
#include "gc_vip_kernel.h"
#include "gc_vip_kernel_drv.h"
#include "gc_vip_kernel_heap.h"
#include "gc_vip_hardware.h"
#include "printf.h"


#define DEFAULT_AI_TASK_NAME            "NPU TASK"
#define DEFAULT_AI_TASK_STACK_SZ        2000
#define DEFAULT_AI_TASK_PRIO            5 //(configMAX_PRIORITIES <96> 1)
#define NPU_FREERTOS_WAIT_FOREVER       0xFFFFFFFF


typedef struct _gckvip_signal_data
{
    void *wait_signal;
    gckvip_mutex lock;
    volatile vip_bool_e value;
} gckvip_signal_data_t;

/*
@brief Do some initialization works in KERNEL_OS layer.
@param video_mem_size, the size of video memory heap.
*/
vip_status_e gckvip_os_init(
    IN vip_uint32_t video_mem_size
    )
{
    vip_status_e status = VIP_SUCCESS;

    status = gckvip_drv_init(video_mem_size);
    if (status != VIP_SUCCESS) {
        PRINTK_E("failed to drv init\n");
    }

    return status;
}

/*
@brief Do some un-initialization works in KERNEL_OS layer.
*/
vip_status_e gckvip_os_close(void)
{
    vip_status_e status = VIP_SUCCESS;

    status = gckvip_drv_exit();
    if (status != VIP_SUCCESS) {
        PRINTK_E("failed to drv exit\n");
    }

    return status;
}


/*******************************NOTICE***********************************/
/************************************************************************/
/* *********************need to be ported functions**********************/
/************************************************************************/
/************************************************************************/
/************************************************************************/
/*
@brief Allocate system memory in kernel space.
@param video_mem_size, the size of video memory heap.
@param size, Memory size to be allocated.
@param memory, Pointer to a variable that will hold the pointer to the memory.
*/
vip_status_e gckvip_os_allocate_memory(
    IN  vip_uint32_t size,
    OUT void **memory
    )
{
    vip_status_e status = VIP_SUCCESS;
#if vpmdENABLE_SYS_MEMORY_HEAP
    status = gcvip_user_allocate_memory(size, memory);
#else
    if (size > 0) {
        *memory = pvPortMalloc(size);
        if (*memory == VIP_NULL) {
            status = VIP_ERROR_OUT_OF_MEMORY;
        }
    }
    else {
        status = VIP_ERROR_INVALID_ARGUMENTS;
    }
#endif

    return status;
}

/*
@brief Free system memory in kernel space.
@param memory, Memory to be freed.
*/
vip_status_e gckvip_os_free_memory(
    IN void *memory
    )
{
#if vpmdENABLE_SYS_MEMORY_HEAP
    gcvip_user_free_memory(memory);
#else
    if (memory != VIP_NULL) {
        vPortFree(memory);
    }
#endif
    return VIP_SUCCESS;
}

/*
@brief Set memory to zero.
@param memory, Memory to be set to zero.
@param size, the size of memory.
*/
vip_status_e gckvip_os_zero_memory(
    IN void *memory,
    IN vip_uint32_t size
    )
{
    if (!memory) {
        return VIP_ERROR_INVALID_ARGUMENTS;
    }
    memset(memory, 0, size);

    return VIP_SUCCESS;
}

/*
@brief copy src memory to dst memory.
@param dst, Destination memory.
@param src. Source memory
@param size, the size of memory should be copy.
*/
vip_status_e gckvip_os_memcopy(
    IN void *dst,
    IN const void *src,
    IN vip_uint32_t size
    )
{
    if ((VIP_NULL == src) || (VIP_NULL == dst))  {
        PRINTK_E("failed to copy memory, parameter is NULL, size: %d bytes\n", size);
        return VIP_ERROR_INVALID_ARGUMENTS;
    }
    memcpy(dst, src, size);

    return VIP_SUCCESS;
}

/*
@brief Read a hardware register.
@param reg_base, the base address of hardware register.
@param address, read the register address.
@param data, the data of address be read.
*/
vip_status_e gckvip_os_read_reg(
    IN volatile void *reg_base,
    IN vip_uint32_t address,
    OUT vip_uint32_t *data
    )
{
    vip_uint32_t *reg = (vip_uint32_t *)((vip_uint8_t *)reg_base + address);
    *data = *reg;

#if vpmdENABLE_CAPTURE_IN_KERNEL
    {
    gckvip_hardware_info_t info = {0};
    vip_uint32_t i = 0;
    gckvip_drv_get_hardware_info(&info);
    for (i = 0; i < info.core_count; i++) {
        if (reg_base == info.vip_reg[i]) {
            break;
        }
    }
    PRINTK("@[register.read %u 0x%05X 0x%08X]\n", i, address, *data);
    }
#endif

    return VIP_SUCCESS;
}

/*
@brief Write a hardware register.
@param reg_base, the base address of hardware register.
@param address, write the register address.
@param data, the data of address be wrote.
*/
vip_status_e  gckvip_os_write_reg(
    IN volatile void *reg_base,
    IN vip_uint32_t address,
    IN vip_uint32_t data
    )
{
    vip_uint32_t *reg = (vip_uint32_t *)((vip_uint8_t *)reg_base + address);
    *reg = data;

#if vpmdENABLE_CAPTURE_IN_KERNEL
    {
    gckvip_hardware_info_t info = {0};
    vip_uint32_t i = 0;
    gckvip_drv_get_hardware_info(&info);
    for (i = 0; i < info.core_count; i++) {
        if (reg_base == info.vip_reg[i]) {
            break;
        }
    }
    PRINTK("@[register.write %u 0x%05X 0x%08X]\n", i, address, data);
    }
#endif

    return VIP_SUCCESS;
}

/*
@brief Waiting for the hardware interrupt. wait interrupt function depend on the type of RTOS
@param irq_queue, the wait queue of interrupt handle.
@param irq_value, a flag for interrupt.
@param mask, mask data for interrupt.
*/
vip_status_e gckvip_os_wait_interrupt(
    IN void    *irq_queue,
    IN volatile vip_uint32_t* volatile irq_value,
    IN vip_uint32_t time_out,
    IN vip_uint32_t mask
    )
{
    vip_status_e status = VIP_SUCCESS;
    SemaphoreHandle_t *irq_queue_handle = (SemaphoreHandle_t *)irq_queue;
    gcIsNULL(irq_value);

    /*VIV: if multi-task is disabled and vip_wait_network() is long time not calls after vip_trigger_network().
           hardare inference a small command, the interrupt returns quickly, even before the application has
           called the vip_wait_network. */
    if (0 == *irq_value) {
        if (xSemaphoreTake(*irq_queue_handle, time_out) != pdTRUE) {
            PRINTK_E("freeOS xTestSemaphore time out irq_value=0x%x\n", *irq_value);
            return VIP_ERROR_TIMEOUT;
        }
    }
    else {
        PRINTK_I("not wait, may interrupt is returned, irq_value=0x%x\n", *irq_value);
    }

    mask = mask;/* Keep compiler happy. */

onError:
    return status;
}

vip_status_e gckvip_os_wakeup_interrupt(
    IN void *irq_queue
    )
{
    vip_status_e status = VIP_SUCCESS;
    static portBASE_TYPE xHigherPriorityTaskWoken;

    xHigherPriorityTaskWoken = pdFALSE;
    SemaphoreHandle_t *irq_queue_handle = (SemaphoreHandle_t *)irq_queue;

    xSemaphoreGiveFromISR(*irq_queue_handle, &xHigherPriorityTaskWoken);

    return status;
}

/*
@brief Delay execution of the current thread for a number of milliseconds.
@param ms, delay unit ms. Delay to sleep, specified in milliseconds.
*/
void gckvip_os_delay(
    IN vip_uint32_t ms
    )
{
    if (ms > 0) {
        vTaskDelay(ms);
    }
}

/*
@brief Delay execution of the current thread for a number of microseconds.
@param ms, delay unit us. Delay to sleep, specified in microseconds.
*/
void gckvip_os_udelay(
    IN vip_uint32_t us
    )
{
    vip_uint32_t ms = us / 1000 + 1;
    /* please replace vTaskDelay to delay us */
    vTaskDelay(ms);
}

/*
@brief Print string on console.
@param msg, which message to be print.
*/
vip_status_e gckvip_os_print(
    gckvip_log_level level,
    IN const char *message,
    ...
    )
{
    vip_status_e status = VIP_SUCCESS;
    char strbuf[256] = {'\0'};
    (void)level;
    va_list varg;
    va_start (varg, message);
    vsnprintf(strbuf, 256, message, varg);
    va_end(varg);

    iprintf("npu[%x] %s", gckvip_os_get_tid(), strbuf);

    return status;
}

/*
@brief Print string to buffer.
@param, size, the size of msg.
@param msg, which message to be print.
*/
vip_status_e gckvip_os_snprint(
    IN vip_char_t *buffer,
    IN vip_uint32_t size,
    IN const vip_char_t *msg,
    ...
    )
{
    vip_status_e status = VIP_SUCCESS;
    va_list varg;
    va_start (varg, msg);
    vsnprintf(buffer, size, msg, varg);
    va_end(varg);

    return status;
}

/*
@brief get the current system time.
*/
vip_uint64_t  gckvip_os_get_time(void)
{
    return (vip_uint64_t)xTaskGetTickCount();
}

/*
@brief Get process id
*/
vip_uint32_t gckvip_os_get_pid(void)
{
    return 0;
}

/*
@brief Get thread id
*/
vip_uint32_t gckvip_os_get_tid(void)
{
    return 0;
}

/*
@brief Memory barrier function for memory consistency.
*/
vip_status_e gckvip_os_memorybarrier(void)
{
    /* please make sure the memory barrier ASM can work well on you platform */
    __asm__ __volatile__("dsb sy":::"cc");

    return VIP_SUCCESS;
}

#if vpmdENABLE_FLUSH_CPU_CACHE
/*
@brief Flush CPU cache for video memory which allocated from heap.
@param physical, VIP physical address.
        gckvip_drv_get_cpuphysical to convert the physical address should be flush CPU cache.
@param logical, the logical address should be flush. loggical address only for Linux system.
            should be equal to physical address in RTOS?
@param size, the size of the memory should be flush.
@param type The type of operate cache. see gckvip_cache_type_e.
*/
vip_status_e gckvip_os_flush_cache(
    IN phy_address_t physical,
    IN void *logical,
    IN vip_uint32_t size,
    IN vip_uint8_t type
    )
{
    vip_status_e status = VIP_SUCCESS;

    PRINTK_D("flush cache physical=0x%08X, size=0x%08X, type=%d\n", physical, size, type);

    /* please implement flush cpu cache function if video memory heap has enabled cache */
    switch (type) {
        case GCKVIP_CACHE_CLEAN:
        case GCKVIP_CACHE_FLUSH:
        case GCKVIP_CACHE_INVALID:
            vCacheFlushDcacheRange((unsigned long)physical,(unsigned long)(size));
            break;
        default:
            PRINTK_E("not support this flush cache type=%d\n", type);
            break;
    }

    logical = logical;

    return status;
}
#endif

/*
@brief Create a mutex for multiple thread.
@param mutex, create mutex pointer.
*/
vip_status_e gckvip_os_create_mutex(
    OUT gckvip_mutex *mutex
    )
{
    *mutex = xSemaphoreCreateRecursiveMutex();
    if(NULL != *mutex) {
        return VIP_SUCCESS;
    }
    else {
        PRINTK_E("fail to create mutex\n");
        return VIP_ERROR_FAILURE;
    }
}

vip_status_e gckvip_os_lock_mutex(
    IN gckvip_mutex mutex
    )
{
    if (VIP_NULL == mutex) {
        PRINTK_E("fail to lock mutex, parameter is NULL\n");
        return VIP_ERROR_FAILURE;
    }

    if (pdTRUE == xSemaphoreTakeRecursive(mutex, NPU_FREERTOS_WAIT_FOREVER)) {
        return VIP_SUCCESS;
    }
    else {
        PRINTK_E("fail to lock mutex=0x%"PRPx"\n", mutex);
        return VIP_ERROR_FAILURE;
    }
}

vip_status_e gckvip_os_unlock_mutex(
    IN gckvip_mutex mutex
    )
{
    if (VIP_NULL == mutex) {
        PRINTK_E("fail to unlock mutex, parameter is NULL\n");
        return VIP_ERROR_FAILURE;
    }

    if (pdTRUE == xSemaphoreGiveRecursive(mutex)) {
        return VIP_SUCCESS;
    }
    else {
        PRINTK_E("fail to unlock mutex=0x%"PRPx"\n", mutex);
        return VIP_ERROR_FAILURE;
    }
}

/*
@brief Destroy a mutex which create in gckvip_os_create_mutex..
@param mutex, create mutex pointer.
*/
vip_status_e gckvip_os_destroy_mutex(
    IN gckvip_mutex mutex
    )
{
    if (mutex != VIP_NULL) {
        vSemaphoreDelete(mutex);
    }
    return VIP_SUCCESS;
}

#if vpmdENABLE_MULTIPLE_TASK || vpmdPOWER_OFF_TIMEOUT
/*
@brief Create a thread.
@param func, the thread handle function.
@param parm, parameter for this thread.
@param name, the thread name.
@param handle, the handle for thread.
*/
vip_status_e gckvip_os_create_thread(
    IN gckvip_thread_func func,
    IN vip_ptr parm,
    IN vip_char_t *name,
    OUT gckvip_thread *handle
    )
{
    if(pdTRUE == xTaskCreate(
                            (TaskFunction_t) func,
                            DEFAULT_AI_TASK_NAME,
                            DEFAULT_AI_TASK_STACK_SZ,
                            parm,
                            DEFAULT_AI_TASK_PRIO,
                            *handle
                            )) {
        return VIP_SUCCESS;
    }
    else {
        PRINTK_E("fail to create npu thread\n");
        return VIP_ERROR_FAILURE;
    }

}

vip_status_e gckvip_os_destroy_thread(
    IN gckvip_thread handle
    )
{
    if (handle != VIP_NULL) {
        vTaskDelete(handle);
    }
    return VIP_SUCCESS;
}
#endif

/*
@brief Create a atomic.
@param atomic, create atomic pointer.
*/
vip_status_e gckvip_os_create_atomic(
    OUT gckvip_atomic *atomic
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_uint32_t *c_atomic = VIP_NULL;
    gcIsNULL(atomic);

    /* please add atomic implement in here if system supports atomic
        otherwise use below function to instead atmoic */

    status = gckvip_os_allocate_memory(sizeof(vip_uint32_t), (void**)&c_atomic);
    if (status != VIP_SUCCESS) {
        PRINTK_E("fail to allocate memory for atomic\n");
        return VIP_ERROR_IO;
    }

    *c_atomic = 0;
    *atomic = (vip_ptr)c_atomic;

onError:
    return status;
}

/*
@brief Destroy a atomic which create in gckvip_os_create_atomic
@param atomic, create atomic pointer.
*/
vip_status_e gckvip_os_destroy_atomic(
    IN gckvip_atomic atomic
    )
{
    vip_status_e status = VIP_SUCCESS;
    gcIsNULL(atomic);

    gckvip_os_free_memory(atomic);

onError:
    return status;
}

/*
@brief Set the 32-bit value protected by an atomic.
@param atomic, create atomic pointer.
*/
vip_status_e gckvip_os_set_atomic(
    IN gckvip_atomic atomic,
    IN vip_uint32_t value
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_uint32_t *data = (vip_uint32_t*)atomic;
    gcIsNULL(atomic);

    *data = value;

onError:
    return status;
}

/*
@brief Get the 32-bit value protected by an atomic.
@param atomic, create atomic pointer.
*/
vip_uint32_t gckvip_os_get_atomic(
    IN gckvip_atomic atomic
    )
{

    vip_uint32_t value = 0xFFFFFFFE;
    vip_uint32_t *data = (vip_uint32_t*)atomic;
    if (VIP_NULL == atomic) {
        PRINTK_E("get atomic invalid arguments\n");
        return(VIP_ERROR_INVALID_ARGUMENTS);
    }

    value = *data;

    return value;
}

/*
@brief Increase the 32-bit value protected by an atomic.
@param atomic, create atomic pointer.
*/
vip_status_e gckvip_os_inc_atomic(
    IN gckvip_atomic atomic
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_uint32_t *data = (vip_uint32_t*)atomic;
    gcIsNULL(atomic);

    *data += 1;

onError:
    return status;
}

/*
@brief Decrease the 32-bit value protected by an atomic.
@param atomic, create atomic pointer.
*/
vip_status_e gckvip_os_dec_atomic(
    IN gckvip_atomic atomic
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_uint32_t *data = (vip_uint32_t*)atomic;
    gcIsNULL(atomic);

    *data -= 1;

onError:
    return status;
}

#if vpmdENABLE_MULTIPLE_TASK || vpmdENABLE_SUSPEND_RESUME || vpmdPOWER_OFF_TIMEOUT
/*
@brief Create a signal.
@param handle, the handle for signal.
*/
vip_status_e gckvip_os_create_signal(
    IN gckvip_signal *handle
    )
{
    vip_status_e status = VIP_SUCCESS;
    gckvip_signal_data_t *data = VIP_NULL;

    if (handle == VIP_NULL) {
        PRINTK_E("failed to create signal, parameter is NULL\n");
        gcGoOnError(VIP_ERROR_FAILURE);
    }

    gcOnError(gckvip_os_allocate_memory(sizeof(gckvip_signal_data_t), (vip_ptr*)&data));

    /* init wait signal in here. TBD */

    data->value = vip_false_e;

    /* init lock */
    gckvip_os_create_mutex(&data->lock);

    *handle = (gckvip_signal)data;

    return status;

onError:
    if (data != VIP_NULL) {
        gckvip_os_free_memory(data);
    }

    return status;
}

vip_status_e gckvip_os_destroy_signal(
    IN gckvip_signal handle
    )
{
    vip_status_e status = VIP_SUCCESS;
    gckvip_signal_data_t *data = (gckvip_signal_data_t*)handle;

    if (data == VIP_NULL) {
        PRINTK_E("failed to destory signal, parameter is NULL\n");
        gcGoOnError(VIP_ERROR_FAILURE);
    }

    data->value = vip_false_e;
    /* add destroy wait signal function in here. TBD */

    if (data->lock != VIP_NULL) {
        gckvip_os_destroy_mutex(data->lock);
    }
    gckvip_os_free_memory(handle);

onError:
    return status;
}

/*
@brief Waiting for a signal.
@param handle, the handle for signal.
@param timeout, the timeout of waiting signal. unit ms. timeout is 0 or vpmdINFINITE, infinite wait signal.
*/
vip_status_e gckvip_os_wait_signal(
    IN gckvip_signal handle,
    IN vip_uint32_t timeout
    )
{
    vip_status_e status = VIP_SUCCESS;
    gckvip_signal_data_t *data = (gckvip_signal_data_t*)handle;
    vip_int32_t ret = 0;

    gckvip_os_lock_mutex(data->lock);
    if (!data->value) {
        /* Wait for the condition. */
        gckvip_os_unlock_mutex(data->lock);

        if (vpmdINFINITE == timeout) {
            while (1) {
                /* add wait function in here. TBD */
                if (0 == ret) {

                    continue;
                }
                break;
            }
        }
        else {
            /* add wait function in here . TBD */
            if (0 == ret) {
                PRINTK_I("wait signal handle=0x%"PRPx", timeout=%dms\n", handle, timeout);
                status = VIP_ERROR_TIMEOUT;
            }
        }

    }
    else {
        gckvip_os_unlock_mutex(data->lock);
    }

    return status;
}

/*
@brief wake up wait signal.
     state is true, wake up all waiting signal and chane the signal state to SET.
     state is false, change the signal to RESET so the gckvip_os_wait_signal will block until
     the signal state changed to SET. block waiting signal function.
@param handle, the handle for signal.
*/
vip_status_e gckvip_os_set_signal(
    IN gckvip_signal handle,
    IN vip_bool_e state
    )
{
    vip_status_e status = VIP_SUCCESS;
    gckvip_signal_data_t *data = (gckvip_signal_data_t*)handle;

    if (data == VIP_NULL) {
        PRINTK_E("failed to set signal, parameter is NULL\n");
        gcGoOnError(VIP_ERROR_FAILURE);
    }

    gckvip_os_lock_mutex(data->lock);
    if (vip_true_e == state) {
        data->value = vip_true_e;
        /* add wake up all signal in here . TBD */

    }
    else {
        data->value = vip_false_e;
    }
    gckvip_os_unlock_mutex(data->lock);

onError:
    return status;
}
#endif

#if vpmdPOWER_OFF_TIMEOUT
/*
@brief create a system timer callback.
@param func, the callback function of this timer.
@param param, the paramete of the timer.
*/
vip_status_e gckvip_os_create_timer(
    IN gckvip_timer_func func,
    IN vip_ptr param,
    OUT gckvip_timer *timer
    )
{
    return VIP_SUCCESS;
}

vip_status_e gckvip_os_start_timer(
    IN gckvip_timer timer,
    IN vip_uint32_t time_out
    )
{
    return VIP_SUCCESS;
}

vip_status_e gckvip_os_stop_timer(
    IN gckvip_timer timer
    )
{
    return VIP_SUCCESS;
}

/*
@brief destroy a system timer
*/
vip_status_e gckvip_os_destroy_timer(
    IN gckvip_timer timer
    )
{
    return VIP_SUCCESS;
}
#endif
