obj-y += irqnest_test.o
obj-y += irqnest_ctx_cmp.o

ifeq ($(CONFIG_IRQNEST_FPU_TEST), y)
obj-$(CONFIG_ARCH_SUN20IW3) += fpu_ctx_riscv.o
endif
obj-$(CONFIG_ARCH_SUN20IW3) += platform/irqnest_e90x.o

subdir-ccflags-y += -I$(srctree)/ekernel/arch/riscv/rv32/include/
subdir-ccflags-y += -I$(srctree)/ekernel/arch/riscv/rv32/e907/clic/
