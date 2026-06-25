#
# Copyright (C) 2006-2010 OpenWrt.org
# Copyright (C) 2016-2016 tracewong
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(BUILD_DIR)/kernel.mk

ifeq ($(CONFIG_LVGL8_USE_SUNXIFB_DOUBLE_BUFFER),y)
TARGET_CFLAGS+=-DUSE_SUNXIFB_DOUBLE_BUFFER
endif

ifeq ($(CONFIG_LVGL8_USE_SUNXIFB_CACHE),y)
TARGET_CFLAGS+=-DUSE_SUNXIFB_CACHE
endif

ifeq ($(CONFIG_LVGL8_USE_SUNXIFB_G2D),y)
TARGET_CFLAGS+=-DUSE_SUNXIFB_G2D
#TARGET_LDFLAGS+=-luapi
TARGET_LDFLAGS+=-lawion
endif

ifeq ($(CONFIG_LVGL8_USE_SUNXIFB_G2D_ROTATE),y)
TARGET_CFLAGS+=-DUSE_SUNXIFB_G2D_ROTATE
endif

ifeq ($(KERNEL_PATCHVER), $(filter $(KERNEL_PATCHVER), 5.4 5.15))
TARGET_CFLAGS+=-DCONF_G2D_VERSION_NEW
endif

ifeq ($(CONFIG_LVGL8_USE_FREETYPE),y)
TARGET_CFLAGS+=-I$(STAGING_DIR)/usr/include/freetype2
TARGET_LDFLAGS+=-lfreetype
endif

ifeq ($(CONFIG_LVGL8_USE_RLOTTIE),y)
TARGET_LDFLAGS+=-lrlottie -ldl -lpthread -lm
endif

ifeq ($(CONFIG_LVGL8_USE_HARDWARE_JPEGDECODER),y)
TARGET_CFLAGS+=-DUSE_HARDWARE_JPEGDECODER
TARGET_CFLAGS+=-I$(STAGING_DIR)/usr/include/allwinner/include
TARGET_LDFLAGS+=-lvdecoder -lvideoengine -lMemAdapter
endif
