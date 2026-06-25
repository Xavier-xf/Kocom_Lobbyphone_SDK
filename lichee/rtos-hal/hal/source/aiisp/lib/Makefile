ccflags-y += -Iekernel/drivers/rtos-hal/hal/source/npu/sdk/inc
#obj-y += ./awnr_helper.o
#obj-y += ./g2d_process.o
#obj-y += ./awaiisp.o

awaiisp_obj += awnr_helper.o
awaiisp_obj += g2d_process.o
awaiisp_obj += awaiisp.o

TARGET	:= libawaiisp.a

$(TARGET): $(awaiisp_obj)
	$(AR) -rcs $@ $^

obj-y += $(awaiisp_obj)
