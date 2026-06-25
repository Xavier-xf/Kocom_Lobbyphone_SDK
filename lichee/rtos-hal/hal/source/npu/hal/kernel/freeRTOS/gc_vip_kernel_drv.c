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
//#include <string.h>
/* #include "FreeRTOS.h"*/
#include "gc_vip_kernel_drv.h"
#include <gc_vip_hardware.h>
#include <gc_vip_kernel.h>
#include <gc_vip_common.h>
#include <gc_vip_platform_config.h>

#include <hal_sem.h>
#include <hal_interrupt.h>
#include <hal_clk.h>
#include <openamp/sunxi_helper/openamp.h>
/* #include <hal_reset.h> */

/* #include "gicv2.h"
   #include "task.h"*/

/*#define npuInt_PRI (((uint32_t)configUNIQUE_INTERRUPT_PRIORITIES) - 5UL)*/


typedef struct _gckvip_driver_t {
    volatile void *vip_reg[vpmdCORE_COUNT]; /* Mapped cpu virtual address for register memory base */
    vip_int32_t irq_line[vpmdCORE_COUNT];
    vip_int32_t irq_enabled[vpmdCORE_COUNT];
    vip_int32_t initialize;
    volatile vip_uint32_t irq_values[vpmdCORE_COUNT];
	/* SemaphoreHandle_t xIntSemaphore[vpmdCORE_COUNT]; */
    hal_sem_t xIntSemaphore[vpmdCORE_COUNT];

    hal_clk_t npu_mclk;
    hal_clk_t npu_pclk;
    /* u32  reset_id;
    hal_clk_id_t mbus_clk_id;
    struct reset_control *reset; */
    hal_clk_id_t npu_mclk_id, npu_pclk_id;
    unsigned int mod_clk;
    unsigned long real_rate;

#if vpmdENABLE_VIDEO_MEMORY_HEAP
    void *cpu_virtual;         /* Mapped cpu virtual address for the VIP memory. */
    phy_address_t cpu_physical; /* CPU physical adddress for the VIP memory */
    phy_address_t vip_physical; /* VIP physical address which VIP would issue to access */
    vip_uint32_t vip_memsize;
#endif
#if vpmdENABLE_SYS_MEMORY_HEAP
    vip_uint32_t sys_heap_size; /* the size of system heap size */
#endif
#if vpmdENABLE_RESERVE_PHYSICAL
    phy_address_t  reserve_physical_base;
    vip_uint32_t   reserve_physical_size;
#endif

    vip_uint32_t power_status[vpmdCORE_COUNT];  /* power on or off */
    vip_uint32_t core_count;
    vip_uint32_t core_fscale_percent;
} gckvip_driver_t;


static gckvip_driver_t kdriver;

#define LOOP_CORE_START                                  \
{  vip_uint32_t core = 0;                                \
   for(core = 0 ; core < kdriver.core_count; core++){

#define LOOP_CORE_END }}


#if 0
#if !vpmdENABLE_POLLING
/*
@brief interrupt handle function.
*/
#define IRQ_HANDLER(core)                                                                     \
  static void irq_handler_core##core(void) {                                                  \
    vip_uint32_t value = 0;                                                                   \
    vip_uint32_t core_index = core;                                                           \
    gckvip_os_read_reg(kdriver.vip_reg[core_index], 0x00010, &value);                     \
    if (value != 0x00) {                                                                      \
        kdriver.irq_values[core_index] = value;                                                \
        gckvip_os_wakeup_interrupt(&kdriver.xIntSemaphore[core_index]);                       \
      }                                                                                       \
  }

/*
@brief rgister interrupt
*/
#define IRQ_REGISTER(core)                                                                                 \
  if(!kdriver.irq_enabled[core]){                                                                          \
    plat_gic_irq_register(kdriver.irq_line[core], npuInt_PRI, 1, (InterruptHandler)irq_handler_core##core); \
    kdriver.irq_enabled[core] = 1;                                                                         \
  }

/* core 0 irq handle function */
IRQ_HANDLER(0)
#endif
#endif


/* ALLWINNER Melis IRQ FUNC */
#if !vpmdENABLE_POLLING
/*
@brief interrupt handle function.
*/

static hal_irqreturn_t npu_isr(void *data) {
    vip_uint32_t value = 0;
    gckvip_os_read_reg(kdriver.vip_reg[0], 0x00010, &value);
    if (value != 0x00) {
        kdriver.irq_values[0] = value;
        /*gckvip_os_wakeup_interrupt(kdriver.xIntSemaphore[0]);*/
        hal_sem_post(kdriver.xIntSemaphore[0]);
    }
    return 0;
}

#endif

/*
@brief convert CPU physical to VIP physical.
@param cpu_physical. the physical address of CPU domain.
*/
vip_address_t gckvip_drv_get_vipphysical(
    vip_address_t cpu_physical
    )
{
    vip_address_t vip_physical = cpu_physical;

    return vip_physical;
}

/*
@brief Get all hardware basic information.
*/
vip_status_e gckvip_drv_get_hardware_info(
    gckvip_hardware_info_t *info
    )
{
    vip_status_e status = VIP_SUCCESS;

    info->max_core_count = vpmdCORE_COUNT;
    info->core_count = vpmdCORE_COUNT;
    info->core_fscale_percent = kdriver.core_fscale_percent;
    info->axi_sram_base = AXI_SRAM_BASE_ADDRESS;
    info->vip_sram_base = VIP_SRAM_BASE_ADDRESS;

    {
      vip_uint32_t core = 0;
      for (core = 0; core < vpmdCORE_COUNT; core++) {
          kdriver.irq_values[core] = 0;
          info->vip_reg[core] = (void *)kdriver.vip_reg[core];
          info->irq_queue[core] = kdriver.xIntSemaphore[core];
          info->irq_value[core] = (vip_uint32_t *)&kdriver.irq_values[core];
          info->device_core_number[core] = LOGIC_DEVICES_WITH_CORE[core];
          info->vip_sram_size[core] = VIP_SRAM_SIZE[core];
          info->axi_sram_size[core] = AXI_SRAM_SIZE[core];
      }
    }

#if vpmdENABLE_VIDEO_MEMORY_HEAP
    info->cpu_virtual = kdriver.cpu_virtual;
    info->cpu_virtual_kernel = kdriver.cpu_virtual;
    info->vip_physical = kdriver.vip_physical;
    info->vip_memsize = kdriver.vip_memsize;

    if ((kdriver.cpu_virtual == VIP_NULL)  || (kdriver.vip_physical == 0) || (kdriver.vip_memsize == 0)) {
        PRINTK("vipcore, video memory is NULL\n");
        status = VIP_ERROR_IO;
    }
#endif

#if vpmdENABLE_SYS_MEMORY_HEAP
    info->sys_heap_size = kdriver.sys_heap_size;
#endif

#if vpmdENABLE_RESERVE_PHYSICAL
    info->reserve_phy_base = kdriver.reserve_physical_base;
    info->reserve_phy_size = kdriver.reserve_physical_size;
#endif

    return status;
}

/*
@brief Inint power and clock.
*/
vip_status_e gckvip_drv_init_power_clk(
    vip_uint32_t core
    )
{
    vip_status_e status = VIP_SUCCESS;
    /* hal_reset_type_t reset_type = HAL_SUNXI_RESET; */
    hal_clk_type_t clk_type = HAL_SUNXI_CCU;
    unsigned long rate;

    /* init clock. */
    /* kdriver.reset_id = ; */
    if (!kdriver.npu_mclk) {
        kdriver.npu_mclk_id = HAL_CLK_PERIPH_NPU;
        kdriver.npu_pclk_id = HAL_CLK_PLL_NPUX4;
        kdriver.mod_clk = 600000000;/* freq */

        /* get clock and reset. */
        /* in v853, we don't need to rst clk */
        /* kdriver.reset = hal_reset_control_get(reset_type, kdriver.reset_id);
        hal_reset_control_deassert(kdriver.reset);
        hal_reset_control_put(kdriver.reset); */
        kdriver.npu_mclk = hal_clock_get(clk_type, kdriver.npu_mclk_id);
        kdriver.npu_pclk = hal_clock_get(clk_type, kdriver.npu_pclk_id);

        /* set pclk rate */
        if (hal_clk_set_rate(kdriver.npu_pclk, kdriver.mod_clk)) {
            PRINTK("set npu pclk rate error!\n");
            return -1;
        }
        /* check npu pclk rate */
        kdriver.real_rate = hal_clk_get_rate(kdriver.npu_pclk);
        PRINTK("Want set npu pclk rate(%d) real(%ld)\n", kdriver.mod_clk, kdriver.real_rate);

        /* set mclk parent */
        if (hal_clk_set_parent(kdriver.npu_mclk, kdriver.npu_pclk)) {
            PRINTK("set npu mclk parent failed!\n");
            return -1;
        }

        /* set mclk rate */
        if (hal_clk_set_rate(kdriver.npu_mclk, kdriver.mod_clk)) {
            PRINTK("set npu mlck rate error!\n");
            return -1;
        }
        /* check npu mclk rate */
        kdriver.real_rate = hal_clk_get_rate(kdriver.npu_mclk);
        PRINTK("Want set npu mclk rate(%d) real(%ld)\n", kdriver.mod_clk, kdriver.real_rate);
    }

    return status;
}

/*
@brief Set power on/off and clock on/off
@param state, power status. refer to gckvip_power_status_e.
*/
vip_status_e gckvip_drv_set_power_clk(
    vip_uint32_t core,
    vip_uint32_t state
    )
{
    vip_status_e status = VIP_SUCCESS;

    if (state == GCKVIP_POWER_ON) {
        if (state != kdriver.power_status[core]) {
            /* power/clock on here */
            /* enable mclk */
            if (kdriver.npu_pclk) {
                PRINTK("Start to enable npu pclk!\n");
			    if (hal_clock_enable(kdriver.npu_pclk)) {
			    	PRINTK("NPU pclk clock enable error\n");
			    	return -1;
			    }
		    } else {
		    	PRINTK("NPU pclk clock is null\n");
		    	return -1;
		    }
            if (kdriver.npu_mclk) {
                PRINTK("Start to enable npu mclk!\n");
			    if (hal_clock_enable(kdriver.npu_mclk)) {
			    	PRINTK("NPU mclk clock enable error\n");
			    	return -1;
			    }
		    } else {
		    	PRINTK("NPU mclk clock is null\n");
		    	return -1;
		    }

            kdriver.power_status[core] = GCKVIP_POWER_ON;
        }
    }
    else if (state == GCKVIP_POWER_OFF) {
        if (state != kdriver.power_status[core]) {
            /* power/clock off here */
            /* disable mclk */
            PRINTK("Start to disable npu mclk!\n");
		    if (kdriver.npu_mclk) {
		    	hal_clock_disable(kdriver.npu_mclk);
		    } else {
		    	PRINTK("NPU mclk clock is null\n");
		    	return -1;
		    }
		    if (kdriver.npu_pclk) {
		    	hal_clock_disable(kdriver.npu_pclk);
		    } else {
		    	PRINTK("NPU pclk clock is null\n");
		    	return -1;
		    }
            kdriver.power_status[core] = GCKVIP_POWER_OFF;
        }
    }
    else {
        PRINTK("vipcore, no this state=%d in set power clk\n", state);
        status = VIP_ERROR_FAILURE;
    }

    return status;
}

/*
@brief do some initialize in this function.
@param, vip_memsizem, the size of video memory heap.
*/
vip_status_e gckvip_drv_init(
    vip_uint32_t vip_memsize
    )
{
    vip_status_e status = VIP_SUCCESS;
    kdriver.initialize = 0;
    kdriver.core_count = vpmdCORE_COUNT;
    kdriver.core_fscale_percent = 100;/* default full clock */

    /* power on VIP */
    LOOP_CORE_START
    status = gckvip_drv_init_power_clk(core);
    if (status != VIP_SUCCESS) {
        PRINTK("vipcore, failed to init power and clk.\n");
        goto exit;
    }
    status = gckvip_drv_set_power_clk(core, GCKVIP_POWER_ON);
    if (status != VIP_SUCCESS) {
        PRINTK("vipcore, failed to power on\n");
        goto exit;
    }
    LOOP_CORE_END

    LOOP_CORE_START
    kdriver.irq_line[core] = IRQ_LINE_NUMBER[core];
    kdriver.vip_reg[core] = AHB_REGISTER_BASE_ADDRESS[core];
    LOOP_CORE_END

#if vpmdENABLE_VIDEO_MEMORY_HEAP
    if(!kdriver.cpu_virtual) {
        if (vip_memsize == 0) {
		kdriver.vip_memsize = VIDEO_MEMORY_HEAP_SIZE;
	} else {
		kdriver.vip_memsize = vip_memsize;
	}
        kdriver.cpu_physical = VIDEO_MEMORY_HEAP_BASE_ADDRESS;
        kdriver.vip_physical = gckvip_drv_get_vipphysical(kdriver.cpu_physical);
        kdriver.cpu_virtual  = (void*)kdriver.cpu_physical;
    }
    PRINTK("gck vip_drv_init, video memory heap base: 0x%"PRIx64", size: 0x%08X\n",
            kdriver.vip_physical, kdriver.vip_memsize);
#endif
#if vpmdENABLE_SYS_MEMORY_HEAP
    kdriver.sys_heap_size = SYSTEM_MEMORY_HEAP_SIZE;
#endif

#if !vpmdENABLE_POLLING
    PRINTK("core_0, request irqline=%d, name=vipcore.\n", kdriver.irq_line[0]);
    /* register core 0 irq */
    if(!kdriver.irq_enabled[0]){
        int ret = 0;
        vip_uint32_t npu_irq = 0;
        ret = hal_request_irq(kdriver.irq_line[0], npu_isr, "vipcore", NULL);
        PRINTK_D("ret = %d\n", ret);
        if (ret < 0)
            PRINTK("vipcore, request_irq failed line=%d\n", kdriver.irq_line[0]);
        else if (ret >= 0)
            PRINTK("vipcore, request_irq success line=%d\n", kdriver.irq_line[0]);
        hal_enable_irq(kdriver.irq_line[0]);
        kdriver.irq_enabled[0] = 1;
        /* gckvip_os_write_reg((volatile void *)0x30800000, 0x1144, 0x1fc00100); */
        gckvip_os_read_reg((volatile void *)0x30800000, 0x1144, &npu_irq);
        PRINTK_D("REG:****RISCV npu_irq = 0x%x******\n", npu_irq);
    }
#endif
    LOOP_CORE_START
    if(!kdriver.initialize){
        /*kdriver.xIntSemaphore[core] = xSemaphoreCreateBinary();*/
        kdriver.xIntSemaphore[core] = hal_sem_create(1);
        PRINTK_D("core = %d, kdriver.xIntSemaphore=0x%x\n", core, kdriver.xIntSemaphore[core]);
        if(kdriver.xIntSemaphore[core] == NULL)
        {
            PRINTK("gck vip_drv_init xIntSemaphore fail\n");
            goto exit;
        }
    }
    LOOP_CORE_END

    kdriver.initialize = 1;

exit:
    return status;
}

/*
@brief do some un-initialize in this function.
*/
vip_status_e gckvip_drv_exit(void)
{
    vip_status_e status = VIP_SUCCESS;

    LOOP_CORE_START
    if(kdriver.irq_enabled[core]){
        /* plat_gic_irq_unregister(kdriver.irq_line[core]); */
		hal_disable_irq(kdriver.irq_line[core]);
        hal_free_irq(kdriver.irq_line[core]);
    }

    if(kdriver.xIntSemaphore[core]){
        /*vSemaphoreDelete(kdriver.xIntSemaphore[core]);*/
        hal_sem_delete(kdriver.xIntSemaphore[core]);
        kdriver.initialize = 0;
    }

    status = gckvip_drv_set_power_clk(core, GCKVIP_POWER_OFF);
    if (status != VIP_SUCCESS) {
        PRINTK("vipcore, failed to power off\n");
    }
    LOOP_CORE_END

    gckvip_os_zero_memory(&kdriver, sizeof(gckvip_driver_t));

    return status;
}
