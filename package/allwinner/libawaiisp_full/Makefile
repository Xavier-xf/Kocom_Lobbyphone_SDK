include $(TOPDIR)/rules.mk

PKG_NAME:=libawaiisp_full
PKG_VERSION:=libawaiisp_full-v1
PKG_RELEASE:=1

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)

include $(BUILD_DIR)/package.mk

ifeq ($(PACKAGE_libawaiisp_full),y)
    CFG_LIB_AWAIISP_FULL := Y
else
    CFG_LIB_AWAIISP_FULL := N
endif
export CFG_LIB_AWAIISP_FULL

ifeq ($(CONFIG_TOOLCHAIN_LIBC), "musl")
    LIBAWAIISP_FULL_TOOLCHAIN_LIBC := musl
else ifeq ($(CONFIG_TOOLCHAIN_LIBC), "glibc")
    LIBAWAIISP_FULL_TOOLCHAIN_LIBC := glibc
endif
export LIBAWAIISP_FULL_TOOLCHAIN_LIBC

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=$(PKG_NAME)
  DEPENDS:=+libawion
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -fr ./sdk $(PKG_BUILD_DIR)/
	$(CP) -fr ./demo $(PKG_BUILD_DIR)/
	$(CP) -fr ./models $(PKG_BUILD_DIR)/
endef

#define Build/Compile
#	$(MAKE) -C $(PKG_BUILD_DIR)/demo
#endef
define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/demo \
		MAKE="$(MAKE)" \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)"  \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)" \
		all
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/include/libawaiisp_full
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/include/awaiisp.h $(1)/usr/include/libawaiisp_full
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/library/$(LIBAWAIISP_FULL_TOOLCHAIN_LIBC)/libawaiisp_full.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/library/$(LIBAWAIISP_FULL_TOOLCHAIN_LIBC)/libawaiisp_full.a $(1)/usr/lib/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/library/$(LIBAWAIISP_FULL_TOOLCHAIN_LIBC)/libawaiisp_full.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/models/*.nb $(1)/lib/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
