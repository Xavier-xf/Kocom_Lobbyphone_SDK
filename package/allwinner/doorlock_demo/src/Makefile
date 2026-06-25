include $(TOPDIR)/rules.mk
include $(BUILD_DIR)/kernel.mk
include $(BUILD_DIR)/package.mk

CUR_PATH := .

APP_UVC_SUPPORT := 1
APP_UAC_SUPPORT := 1
APP_MPPAIO_SUPPORT := 0

ifeq ($(TARGET_CHIP), sun8iw12p1)
    MPPCFG_CHIP := AW1721
else ifeq ($(TARGET_CHIP), sun8iw16p1)
    MPPCFG_CHIP := AW1816
else ifeq ($(TARGET_CHIP), sun8iw19p1)
    MPPCFG_CHIP := AW1817
else ifeq ($(TARGET_CHIP), sun8iw21p1)
    MPPCFG_CHIP := AW1886
else
    $(error fatal error! $(TARGET_CHIP) is not support by eyesee-mpp!)
endif
export MPPCFG_CHIP

#config write file method: LINUX_VFS, DIRECT_IO
ifeq ($(CONFIG_mpp_fwrite_method_vfs),y)
    MPPCFG_FILE_SYSTEM := LINUX_VFS
else ifeq ($(CONFIG_mpp_fwrite_method_directIO),y)
    MPPCFG_FILE_SYSTEM := DIRECT_IO
else
    MPPCFG_FILE_SYSTEM := LINUX_VFS
endif

EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
EYESEE_MINIGUI_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-minigui
-include $(EYESEE_MPP_INCLUDE)/middleware/config/mpp_config.mk

TARGET_BIN := doorlock_demo

CFILES = $(CUR_PATH)/main.c \
	$(CUR_PATH)/aw_msg.c \
	$(CUR_PATH)/aw_dev.c \
	$(CUR_PATH)/doorlock_common.c \
	$(CUR_PATH)/aw_alg/aw_alg.c \
	$(CUR_PATH)/aw_alg/aw_alg_pix_kit.c \
	$(CUR_PATH)/aw_alg/vnn_utils.cpp \
	$(CUR_PATH)/aw_rtmedia/aw_rtmedia.c \
	$(CUR_PATH)/aw_components/aw_mem.c \
	$(CUR_PATH)/aw_components/aw_g2d.c \
	$(CUR_PATH)/aw_components/aw_osd.c \
	$(CUR_PATH)/aw_components/char_conversion.c \
	$(CUR_PATH)/aw_components/aw_uart.c \
	$(CUR_PATH)/aw_components/aw_mtd.c \
	$(CUR_PATH)/aw_components/aw_ringbuffer.c \

ifeq ($(APP_UVC_SUPPORT),1)
	CFILES += $(CUR_PATH)/aw_components/aw_uvcout.c
endif

ifeq ($(APP_UAC_SUPPORT),1)
	CFILES += $(CUR_PATH)/aw_components/aw_uacdev.c
	CFILES += $(CUR_PATH)/aw_components/aw_composite.c
ifeq ($(APP_MPPAIO_SUPPORT),1)
	CFILES += $(CUR_PATH)/aw_mpp/aw_mpp.c
	CFILES += $(CUR_PATH)/aw_mpp/aw_mpp_ao.c
	CFILES += $(CUR_PATH)/aw_mpp/aw_mpp_ai.c
else
	CFILES += $(CUR_PATH)/aw_components/aw_sndcard.c
endif
endif

OBJS = $(CFILES:%=%.o) $(C++FILES:%=%.o)
DEPS = $(OBJS:%=%.d)

INCLUDES_DIR := \
	$(CUR_PATH)/ \
	$(CUR_PATH)/aw_components \
	$(CUR_PATH)/aw_mpp \
	$(CUR_PATH)/aw_rtmedia \
	$(CUR_PATH)/aw_alg \
	$(CUR_PATH)/aw_alg/libnna/include \
	$(CUR_PATH)/aw_alg/libpix_facekit/include \
	$(KERNEL_DIR)/drivers/usb/gadget/function \
	$(KERNEL_DIR)/user_headers/include \
	$(STAGING_DIR)/usr/include/ \
	$(STAGING_DIR)/usr/include/rt_media \
	$(STAGING_DIR)/usr/include/eyesee-mpp \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/include/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/include/utils/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/include/media/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/include/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/include/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/include/V4l2Camera/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/include/V4l2Camera/linux/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/isp_tuning/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib/osal/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib/midware/decoding/include/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib/osal/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libcedarc/include/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/include_FsWriter/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/include_stream/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/include_muxer/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/agc_lib/include/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libISE/include/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/include/component/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/system/public/libion/include/ \
	$(STAGING_DIR)/usr/include/eyesee-mpp/system/public/include \
	$(STAGING_DIR)/usr/include/eyesee-mpp/system/public/include/utils

LIBS_DIR := \
	$(CUR_PATH)/aw_alg/libnna/lib \
	$(CUR_PATH)/aw_alg/libpix_facekit/lib \
	$(STAGING_DIR)/usr/lib/rt_media \
	$(STAGING_DIR)/usr/lib/eyesee-mpp \
	$(STAGING_DIR)/usr/lib


INCLUDES = $(foreach n, $(INCLUDES_DIR), -I$(n))
LIBS = $(foreach n, $(LIBS_DIR), -L$(n))

# reduce app size by manual
SHARED_LIBS_NAME = librt_media.so libawion.so
STATIC_LIBS_NAME = liblog.a libVIPlite.a libVIPuser.a libpix_facekit_api.a

ifeq ($(APP_UAC_SUPPORT),1)
	#STATIC_LIBS_NAME += libasound.a
	SHARED_LIBS_NAME += libasound.so
ifeq ($(APP_MPPAIO_SUPPORT),1)
	STATIC_LIBS_NAME += libAec.a libAgc.a libAns.a libaw_mpp.a libmedia_utils.a libResample.a libDrc.a
ifeq ($(CONFIG_mpp_aenc),y)
	STATIC_LIBS_NAME += libcedarx_aencoder.a
	STATIC_LIBS_NAME += libaacenc.a
endif
else

endif
endif

SHARED_LIBS := \
	-lpthread -lglog $(foreach n, $(SHARED_LIBS_NAME), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(n)))))
STATIC_LIBS := \
	$(foreach n, $(STATIC_LIBS_NAME), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n)))))

CEDARX_EXT_CFLAGS += -D__OS_LINUX -DAPP_UVC_SUPPORT=$(APP_UVC_SUPPORT) -DAPP_UAC_SUPPORT=$(APP_UAC_SUPPORT) -DAPP_MPPAIO_SUPPORT=$(APP_MPPAIO_SUPPORT)
CCFLAGS = -c -g -fPIC $(CEDARX_EXT_CFLAGS) $(INCLUDES)
CXXFLAGS = -c -g -fPIC $(CEDARX_EXT_CFLAGS) $(INCLUDES)
STRIP=arm-openwrt-linux-muslgnueabi-strip

LOCAL_BIN_FLAGS := \
	$(LDFLAGS) \
	$(CFLAGS) \
	$(LIBS) \
	-Wl,-Bstatic -Wl,--start-group $(STATIC_LIBS) -Wl,--end-group \
	-Wl,-Bdynamic -Wl,--start-group $(SHARED_LIBS) -Wl,--end-group

.PHONY: all clean

all:$(TARGET_BIN)

$(TARGET_BIN):$(OBJS)
	@echo $(STATIC_LIBS) =======
	@echo $(SHARED_LIBS) -------
	$(CXX) $^ $(LOCAL_BIN_FLAGS) -o $@
	$(STRIP) $(TARGET_BIN)

$(filter %.c.o , $(OBJS)): %.o:%
	$(CC) $(CCFLAGS) -o $@  $< -MD -MP -MF $(@:%=%.d)

$(filter %.cpp.o %.cc.o, $(OBJS)): %.o:%
	$(CXX) $(CXXFLAGS) -o $@  $< -MD -MP -MF $(@:%=%.d)

clean:
	$(RM) -rf $(TARGET_BIN)
	$(RM) -rf $(OBJS)
	$(RM) -rf $(DEPS)
	$(RM) -rf *.o
	$(RM) -rf *.d

-include $(DEPS)
