-include $(TOPDIR)/rules.mk
-include $(INCLUDE_DIR)/package.mk
-include $(BUILD_DIR)/package.mk

target = libwifimg-v2.0.so

INCLUDES        += -I ./include
INCLUDES        += -I ./include/os
ifeq ($(CONFIG_WMG_PLATFORM_LINUX), y)
INCLUDES        += -I ./include/os/linux
INCLUDES        += -I src/os/linux/linkd
endif
ifeq ($(CONFIG_WMG_PLATFORM_XRLINK), y)
INCLUDES        += -I ./include/os/xrlink
endif
INCLUDES        += -I $(CONFIG_PREFIX)/usr/include

SRCS += src/wifimg.c
SRCS += src/wmg_common.c
SRCS += src/log/wifi_log.c
SRCS += src/config/wmg_get_config.c
ifeq ($(CONFIG_WMG_SUPPORT_EXPAND), y)
SRCS += src/expand_cmd.c
endif
ifeq ($(CONFIG_WMG_PLATFORM_XRLINK), y)
SRCS += src/os/xrlink/msg_netlink.c
SRCS += src/os/xrlink/xrlink_common.c
ifeq ($(CONFIG_WMG_SUPPORT_EXPAND), y)
SRCS += src/os/xrlink/expand/xrlink_expand_cmd.c
endif
else
ifeq ($(CONFIG_WMG_SUPPORT_EXPAND), y)
SRCS += src/os/linux/expand/linux_expand_cmd.c
endif
endif

ifeq ($(CONFIG_WMG_SUPPORT_STA_MODE), y)
SRCS += src/wmg_sta.c
ifeq ($(CONFIG_WMG_PLATFORM_LINUX), y)
SRCS += src/os/linux/linux_get_config.c
SRCS += src/os/linux/udhcpc.c
SRCS += src/os/linux/event.c
SRCS += src/os/linux/linux_sta.c
SRCS += src/os/linux/linux_common.c
SRCS += src/os/linux/scan.c
SRCS += src/os/linux/utils.c
SRCS += src/os/linux/linux_get_config.c
endif
ifeq ($(CONFIG_WMG_PLATFORM_XRLINK), y)
SRCS += src/os/xrlink/xrlink_sta.c
endif
endif

ifneq ($(CONFIG_WMG_PROTOCOL_BLE)$(CONFIG_WMG_PROTOCOL_SOFTAP)$(CONFIG_WMG_PROTOCOL_XCONFIG)$(CONFIG_WMG_PROTOCOL_SOUNDWAVE), )
SRCS += src/linkd.c
endif

ifeq ($(CONFIG_WMG_SUPPORT_AP_MODE), y)
SRCS += src/wmg_ap.c
ifeq ($(CONFIG_WMG_PLATFORM_LINUX), y)
SRCS += src/os/linux/linux_ap.c
endif
ifeq ($(CONFIG_WMG_PLATFORM_XRLINK), y)
SRCS += src/os/xrlink/xrlink_ap.c
endif
endif

ifeq ($(CONFIG_WMG_SUPPORT_MONITOR_MODE), y)
SRCS += src/wmg_monitor.c
ifeq ($(CONFIG_WMG_PLATFORM_LINUX), y)
SRCS += src/os/linux/linux_monitor.c
endif
ifeq ($(CONFIG_WMG_PLATFORM_XRLINK), y)
SRCS += src/os/xrlink/xrlink_monitor.c
endif
endif

ifeq ($(CONFIG_WMG_SUPPORT_P2P_MODE), y)
SRCS += src/wmg_p2p.c
ifeq ($(CONFIG_WMG_PLATFORM_LINUX), y)
SRCS += src/os/linux/linux_p2p.c
endif
endif

ifdef CONFIG_WMG_PROTOCOL_SOFTAP
SRCS += src/os/linux/linkd/linkd_softap/linkd_softap.c
endif

ifdef CONFIG_WMG_PROTOCOL_BLE
SRCS += src/os/linux/linkd/linkd_ble/linkd_ble.c
LIBS += -lbtmg
LIBS += -lbluetooth
LIBS += -lbluetooth-internal
LIBS += -lglib-2.0 -lsbc -ljson-c -lgio-2.0 -lgobject-2.0 -lasound -lbtmg -ldl -lm -lresolv
LIBS += -lshared-mainloop
LIBS += -ldbus-1
endif

ifdef CONFIG_WMG_PROTOCOL_XCONFIG
INCLUDES += -I src/os/linux/linkd/lib/lib_xconfig
SRCS += src/os/linux/linkd/linkd_xconfig/linkd_xconfig.c
LIBS += -lcrypto
ifeq ($(findstring aarch64, $(CONFIG_ARCH)), aarch64)
    SRCS += ./src/os/linux/linkd/lib/lib_xconfig/libdecode_64.a
else
    ifeq ($(findstring riscv, $(CONFIG_ARCH)), riscv)
        SRCS += ./src/os/linux/linkd/lib/lib_xconfig/libdecode_riscv.a
    else
        SRCS += ./src/os/linux/linkd/lib/lib_xconfig/libdecode.a
    endif
endif
endif

ifdef CONFIG_WMG_PROTOCOL_SOUNDWAVE
INCLUDES += -I src/os/linux/linkd/lib/lib_soundwave
LIBS += -lasound -luci
SRCS += src/os/linux/linkd/linkd_soundwave/linkd_soundwave.c
ifeq ($(CONFIG_LIBC), "glibc")
    LIBS += -lm
ifeq ($(findstring aarch64, $(CONFIG_ARCH)), aarch64)
    ADTLIB=./src/os/linux/linkd/lib/lib_soundwave/glibc/aarch64/libADT.a
else
    ifeq ($(findstring riscv, $(CONFIG_ARCH)), riscv)
        ADTLIB=./src/os/linux/linkd/lib/lib_soundwave/glibc/riscv/libADT.a
    else
        ADTLIB=./src/os/linux/linkd/lib/lib_soundwave/glibc/arm/libADT.a
    endif
endif
else
ifeq ($(findstring aarch64, $(CONFIG_ARCH)), aarch64)
    ADTLIB=./src/os/linux/linkd/lib/lib_soundwave/musl/aarch64/libADT.a
else
    ifeq ($(findstring riscv, $(CONFIG_ARCH)), riscv)
        ADTLIB=./src/os/linux/linkd/lib/lib_soundwave/musl/riscv/libADT.a
    else
        ADTLIB=./src/os/linux/linkd/lib/lib_soundwave/musl/arm/libADT.a
    endif
endif
endif
endif

LOCAL_CFLAGS = -D_GNU_SOURCE=1

install:
	-@mkdir -p $(CONFIG_PREFIX)/usr/lib
	-@mkdir -p $(CONFIG_PREFIX)/usr/include
	@cp ./include/*.h $(CONFIG_PREFIX)/usr/include

#libwifimg-v2.0.so
#ifdef CONFIG_LINUX_5_4 || CONFIG_LINUX_4_9
ifeq ($(findstring aarch64, $(CONFIG_ARCH)), aarch64)
    SRCS += ./src/os/linux/lib/libwpa_client_64.a
else
    ifeq ($(CONFIG_SOFT_FLOAT), y)
        SRCS += ./src/os/linux/lib/libwpa_client_softft.a
    else
         ifeq ($(findstring riscv, $(CONFIG_ARCH)), riscv)
              SRCS += ./src/os/linux/lib/libwpa_client_riscv.a
         else
              SRCS += ./src/os/linux/lib/libwpa_client.a
         endif
    endif
endif
#endif
$(target): $(SRCS) $(ADTLIB)
	$(CC) -fPIC -shared $(INCLUDES) $(LOCAL_CFLAGS) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LIBS)
	@cp $@ $(CONFIG_PREFIX)/usr/lib

all: install $(target)
clean:
	rm -rf $(target)
