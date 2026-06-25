include $(TOPDIR)/rules.mk
-include $(INCLUDE_DIR)/package.mk
-include $(INCLUDE_DIR)/version.mk
-include $(BUILD_DIR)/package.mk

PKG_NAME:=wifimanager-v2.0
PKG_VERSION:=0.1
PKG_RELEASE:=1

# VERSION_CODE has value only on Tina5.0 or later
ifneq ($(VERSION_CODE),)
SRC_CODE_DIR := $(LICHEE_CBBPKG_DIR)/allwinner/wireless/wifimanager
TARGET_CFLAGS += -DTINA_5_0
else
SRC_CODE_DIR := ./
endif

PKG_CONFIG_DEPENDS:= \
	CONFIG_WMG_SUPPORT_STA_MODE \
	CONFIG_WMG_SUPPORT_AP_MODE \
	CONFIG_WMG_SUPPORT_MONITOR_MODE \
	CONFIG_WMG_SUPPORT_P2P_MODE \
	CONFIG_WMG_PROTOCOL_SOFTAP \
	CONFIG_WMG_PROTOCOL_BLE \
	CONFIG_WMG_PROTOCOL_XCONFIG \
	CONFIG_WMG_PROTOCOL_SOUNDWAVE

PKG_FILE_DEPENDS += $(SRC_CODE_DIR)

define Package/$(PKG_NAME)/Default
  SUBMENU:=Wireless
  CATEGORY:=Allwinner
  TITLE:=Tina wifimanager-v2.0
endef

define Package/$(PKG_NAME)
  MENU:=1
  DEFAULT:=
  $(call Package/wifimanager-v2.0/Default)
endef

define Package/$(PKG_NAME)/config
source "$(SOURCE)/Config.in"
endef

define Package/$(PKG_NAME)-lib
  $(call Package/wifimanager-v2.0/Default)
  TITLE:=Tina wifimanager-v2.0 lib
  DEPENDS:= wifimanager-v2.0 +libpthread +libnl-tiny +libstdcpp +wirelesscommon +libopenssl \
            +WMG_PROTOCOL_BLE:btmanager-v4.0 \
            +WMG_PROTOCOL_BLE:libjson-c \
            +WMG_PROTOCOL_XCONFIG:libopenssl \
            +WMG_PROTOCOL_SOUNDWAVE:alsa-lib \
            +WMG_PROTOCOL_SOUNDWAVE:libuci
endef

define Package/$(PKG_NAME)-demo
  $(call Package/wifimanager-v2.0/Default)
  TITLE:=Tina wifimanager-v2.0 app demo
  DEPENDS:= wifimanager-v2.0 wifimanager-v2.0-lib +wirelesscommon
endef

define Package/$(PKG_NAME)/description
	Tina wifi manager v2.0
endef

define Package/$(PKG_NAME)-lib/description
	Tina wifimanager-v2.0 lib
endef

define Package/$(PKG_NAME)-demo/description
	Tina wifimanager-v2.0 app demo
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r $(SRC_CODE_DIR)/core $(PKG_BUILD_DIR)/
	$(CP) -r $(SRC_CODE_DIR)/demo $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

TARGET_CFLAGS += $(if $(CONFIG_WMG_DEFAULT_DEBUG_LV_ERROR),-DDEFAULT_DEBUG_LV_ERROR)
TARGET_CFLAGS += $(if $(CONFIG_WMG_DEFAULT_DEBUG_LV_WARN),-DDEFAULT_DEBUG_LV_WARN)
TARGET_CFLAGS += $(if $(CONFIG_WMG_DEFAULT_DEBUG_LV_INFO),-DDEFAULT_DEBUG_LV_INFO)
TARGET_CFLAGS += $(if $(CONFIG_WMG_DEFAULT_DEBUG_LV_DEBUG),-DDEFAULT_DEBUG_LV_DEBUG)
TARGET_CFLAGS += $(if $(CONFIG_WMG_DEFAULT_DEBUG_LV_DUMP),-DDEFAULT_DEBUG_LV_DUMP)
TARGET_CFLAGS += $(if $(CONFIG_WMG_DEFAULT_DEBUG_LV_EXCE),-DDEFAULT_DEBUG_LV_EXCE)

TARGET_CFLAGS += $(if $(CONFIG_WMG_SUPPORT_STA_MODE),-DSUPPORT_STA_MODE)
TARGET_CFLAGS += $(if $(CONFIG_WMG_SUPPORT_AP_MODE),-DSUPPORT_AP_MODE)
TARGET_CFLAGS += $(if $(CONFIG_WMG_SUPPORT_MONITOR_MODE),-DSUPPORT_MONITOR_MODE)
TARGET_CFLAGS += $(if $(CONFIG_WMG_SUPPORT_P2P_MODE),-DSUPPORT_P2P_MODE)

TARGET_CFLAGS += $(if $(CONFIG_USE_GLIBC),-pthread)
TARGET_LDFLAGS += $(if $(CONFIG_USE_GLIBC),-lpthread -lrt -ldl)
TARGET_CFLAGS += $(if $(CONFIG_WMG_SUPPORT_EXPAND),-DSUPPORT_EXPAND)
TARGET_CFLAGS += $(if $(CONFIG_WMG_PLATFORM_LINUX),-DOS_NET_LINUX_OS)
TARGET_CFLAGS += $(if $(CONFIG_WMG_PLATFORM_FREERTOS),-DOS_NET_FREERTOS_OS)
TARGET_CFLAGS += $(if $(CONFIG_WMG_PLATFORM_XRLINK),-DOS_NET_XRLINK_OS)

TARGET_CFLAGS += -DWMG_CONFIG_PATH=\\\"$(CONFIG_WMG_CONFIG_PATH)\\\"

TARGET_CFLAGS += -I $(STAGING_DIR)/usr/include/libnl-tiny
TARGET_LDFLAGS += -lnl-tiny -lwirelesscom

ifneq ($(CONFIG_WMG_PROTOCOL_BLE)$(CONFIG_WMG_PROTOCOL_SOFTAP)$(CONFIG_WMG_PROTOCOL_XCONFIG)$(CONFIG_WMG_PROTOCOL_SOUNDWAVE), )
TARGET_CFLAGS += -DSUPPORT_LINKD
endif

ifeq ($(CONFIG_PACKAGE_wifimanager-v2.0-lib),y)
define Build/Compile/$(PKG_NAME)-lib
	#libwifimg-v2.0.so
	$(MAKE) -C $(PKG_BUILD_DIR)/core/ \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)" \
		all
endef
endif

ifeq ($(CONFIG_PACKAGE_wifimanager-v2.0-demo),y)
define Build/Compile/$(PKG_NAME)-demo
	$(MAKE) -C $(PKG_BUILD_DIR)/demo/ \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)" \
		all
endef
endif

define Build/Compile
	$(Build/Compile/$(PKG_NAME))
	$(Build/Compile/$(PKG_NAME)-lib)
	$(Build/Compile/$(PKG_NAME)-demo)
endef

ifeq ($(CONFIG_PACKAGE_wifimanager-v2.0-lib),y)
define Build/InstallDev/$(PKG_NAME)-lib
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/lib/libwifimg-v2.0.so $(1)/lib
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/include/*.h $(1)/usr/include
endef
endif

ifeq ($(CONFIG_PACKAGE_wifimanager-v2.0-demo),y)
define Build/InstallDev/$(PKG_NAME)-demo
endef
endif

define Build/InstallDev
	$(Build/InstallDev/$(PKG_NAME))
	$(Build/InstallDev/$(PKG_NAME)-lib)
	$(Build/InstallDev/$(PKG_NAME)-demo)
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)$(CONFIG_WMG_CONFIG_PATH)
	$(INSTALL_DIR) $(1)$(CONFIG_WMG_CONFIG_PATH)/wpa_supplicant
	$(INSTALL_DIR) $(1)$(CONFIG_WMG_CONFIG_PATH)/hostapd
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DATA) $(SRC_CODE_DIR)/files/hostapd.conf $(1)$(CONFIG_WMG_CONFIG_PATH)/hostapd
	$(INSTALL_DATA) $(SRC_CODE_DIR)/files/wpa_supplicant.conf $(1)$(CONFIG_WMG_CONFIG_PATH)/wpa_supplicant
	$(INSTALL_DATA) $(SRC_CODE_DIR)/files/wpa_supplicant_p2p.conf $(1)$(CONFIG_WMG_CONFIG_PATH)/wpa_supplicant
	$(INSTALL_DATA) $(SRC_CODE_DIR)/files/wpa_supplicant_src.conf $(1)$(CONFIG_WMG_CONFIG_PATH)/wpa_supplicant
	$(INSTALL_DATA) $(SRC_CODE_DIR)/files/wpa_supplicant_overlay.conf $(1)$(CONFIG_WMG_CONFIG_PATH)/wpa_supplicant
	$(INSTALL_DATA) $(SRC_CODE_DIR)/files/wifi_monitor.conf $(1)$(CONFIG_WMG_CONFIG_PATH)/monitor
	$(INSTALL_BIN) $(SRC_CODE_DIR)/files/udhcpc_wlan0.init $(1)$(CONFIG_WMG_CONFIG_PATH)/udhcpc_wlan0
	$(INSTALL_BIN) $(SRC_CODE_DIR)/files/wifi_start.init $(1)/etc/init.d/wifi_start
	$(INSTALL_BIN) $(SRC_CODE_DIR)/files/udhcpc_p2p-wlan0-0.init $(1)$(CONFIG_WMG_CONFIG_PATH)/udhcpc_p2p-wlan0-0
	$(INSTALL_BIN) $(SRC_CODE_DIR)/files/wifimg.config $(1)$(CONFIG_WMG_CONFIG_PATH)/wifimg.def
	$(INSTALL_BIN) $(SRC_CODE_DIR)/files/wmg_udhcpc6.script $(1)$(CONFIG_WMG_CONFIG_PATH)/wmg_udhcpc6.script
	$(INSTALL_BIN) $(SRC_CODE_DIR)/files/dnsmasq.conf $(1)$(CONFIG_WMG_CONFIG_PATH)/dnsmasq.conf
endef

define Package/$(PKG_NAME)-lib/install
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/lib/libwifimg-v2.0.so $(1)/lib
	$(INSTALL_DATA) $(PKG_INSTALL_DIR)/usr/include/*.h $(1)/usr/include
endef

define Package/$(PKG_NAME)-demo/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_DIR) $(1)/etc/wifi
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/wifi_daemon $(1)/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/wifi $(1)/bin
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
$(eval $(call BuildPackage,$(PKG_NAME)-lib))
$(eval $(call BuildPackage,$(PKG_NAME)-demo))
