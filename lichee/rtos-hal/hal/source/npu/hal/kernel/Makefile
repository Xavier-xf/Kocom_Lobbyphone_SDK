
ccflags-y += -Iekernel/drivers/rtos-hal/hal/source/npu/hal/inc
ccflags-y += -Iekernel/drivers/rtos-hal/hal/source/npu/sdk/inc
#kernel
ccflags-y += -Iekernel/drivers/rtos-hal/hal/source/npu/hal/kernel
ccflags-y += -Iekernel/drivers/rtos-hal/hal/source/npu/hal/kernel/freeRTOS
ccflags-y += -Iekernel/drivers/rtos-hal/hal/source/npu/hal/kernel/freeRTOS/allwinner
obj-y += gc_vip_hardware.o
obj-y += gc_vip_kernel.o
obj-y += gc_vip_kernel_util.o
obj-y += gc_vip_kernel_pm.o
obj-y += gc_vip_video_memory.o
obj-y += gc_vip_kernel_command.o
obj-y += gc_vip_kernel_mmu.o
obj-y += gc_vip_kernel_heap.o
obj-y += gc_vip_kernel_debug.o

#freeRTOS
obj-y += freeRTOS/gc_vip_kernel_allocator.o
obj-y += freeRTOS/gc_vip_kernel_os.o
obj-y += freeRTOS/gc_vip_kernel_drv.o
#rpmsg
obj-y += freeRTOS/npu_rpmsg.o

#allwinner
#obj-y += freeRTOS/allwinner/gc_vip_kernel_drv_platform.o
#obj-y += freeRTOS/allwinner/gc_vip_kernel_ion.o
