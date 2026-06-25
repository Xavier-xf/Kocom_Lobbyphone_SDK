include $(TOPDIR)/rules.mk
include $(BUILD_DIR)/kernel.mk

PKG_NAME:=libawion
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)

PKG_BUILD_DEPENDS := libbaselist

include $(BUILD_DIR)/package.mk

KERNEL_VERSION = CONF_KERNEL_VERSION_3_10

#config the kernel version,which depends on $(KERNEL_PATCHVER)
ifeq ($(KERNEL_PATCHVER),3.4)
    KERNEL_VERSION = CONF_KERNEL_VERSION_3_4
endif

ifeq ($(KERNEL_PATCHVER),3.10)
    KERNEL_VERSION = CONF_KERNEL_VERSION_3_10
endif

ifeq ($(KERNEL_PATCHVER),4.4)
    KERNEL_VERSION = CONF_KERNEL_VERSION_4_4
endif

ifeq ($(KERNEL_PATCHVER),4.9)
    KERNEL_VERSION = CONF_KERNEL_VERSION_4_9
endif

ifeq ($(KERNEL_PATCHVER),5.4)
    KERNEL_VERSION = CONF_KERNEL_VERSION_5_4
endif

export KERNEL_VERSION

#end config the kernel version
TARGET_CFLAGS+=-D$(KERNEL_VERSION)


define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=ion api for multimedia
  MENU:=1
  DEPENDS:=
endef

define Package/$(PKG_NAME)/Default
endef

define Package/$(PKG_NAME)/description
  ion api for allwinner
endef

define Package/$(PKG_NAME)/config
if PACKAGE_$(PKG_NAME)
choice
prompt "choose ion alloc method"
	default AW_ION_ALLOC_IOMMU
	help
	select how to apply for memory
config AW_ION_ALLOC_IOMMU
	bool "aw_ion_alloc_iommu"
config AW_ION_ALLOC_CMA
	bool "aw_ion_alloc_cma"
endchoice
endif
endef

ifeq ($(CONFIG_AW_ION_ALLOC_CMA),y)
TARGET_CFLAGS+=-DCONF_KERNEL_CMA
endif
ifeq ($(CONFIG_AW_ION_ALLOC_IOMMU),y)
TARGET_CFLAGS+=-DCONF_KERNEL_IOMMU
endif

define Build/Prepare
	$(INSTALL_DIR) $(PKG_BUILD_DIR)/
	$(CP) ./src $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/src\
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		LINUX_USER_HEADERS="$(LINUX_DIR)/user_headers" \
		INSTALL_PREFIX="$(PKG_INSTALL_DIR)" \
		all
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include

	$(CP) $(PKG_INSTALL_DIR)/usr/lib/* $(1)/usr/lib/
	$(CP) $(PKG_INSTALL_DIR)/usr/include/* $(1)/usr/include/
endef

ifeq ($(CONFIG_PACKAGE_rt_media),y)
define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/lib/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/*.so $(1)/lib/
endef
else
define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/lib/*.so $(1)/usr/lib/
endef
endif

$(eval $(call BuildPackage,$(PKG_NAME)))
