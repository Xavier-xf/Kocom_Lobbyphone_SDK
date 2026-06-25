include $(TOPDIR)/rules.mk

PKG_NAME:=libawaisr
PKG_VERSION:=libawaisr-v1
PKG_RELEASE:=1

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)

include $(BUILD_DIR)/package.mk

ifeq ($(PACKAGE_libawaisr),y)
    CFG_LIB_AWAISR := Y
else
    CFG_LIB_AWAISR := N
endif
export CFG_LIB_AWAISR

ifeq ($(CONFIG_TOOLCHAIN_LIBC), "musl")
    LIBAWAISR_TOOLCHAIN_LIBC := musl
else ifeq ($(CONFIG_TOOLCHAIN_LIBC), "glibc")
    LIBAWAISR_TOOLCHAIN_LIBC := glibc
endif
export LIBAWAISR_TOOLCHAIN_LIBC

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=$(PKG_NAME)
  DEPENDS:=+libawion +libawnn_full
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -fr ./sdk $(PKG_BUILD_DIR)/
	$(CP) -fr ./models $(PKG_BUILD_DIR)/
endef

define Build/Compile
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)"  \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/include/libawaisr
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/include/awaisr.h $(1)/usr/include/libawaisr
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/library/$(LIBAWAISR_TOOLCHAIN_LIBC)/libawaisr.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/library/$(LIBAWAISR_TOOLCHAIN_LIBC)/libawaisr.a $(1)/usr/lib/
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk/library/$(LIBAWAISR_TOOLCHAIN_LIBC)/libawaisr.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/models/*.cbin $(1)/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/models/*.lic $(1)/lib/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
