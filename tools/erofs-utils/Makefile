include $(TOPDIR)/rules.mk

PKG_NAME:=erofs-utils
PKG_VERSION:=1.2
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://git.kernel.org/pub/scm/linux/kernel/git/xiang/erofs-utils.git/
PKG_MD5SUM:=fff3827a842379002c989941f7071b17

HOST_FIXUP:=autoreconf

HOST_BUILD_PARALLEL:=1

include $(BUILD_DIR)/host-build.mk

HOST_LDFLAGS += -Wl,-rpath=$(STAGING_DIR_HOST)/lib/

HOST_CONFIGURE_ARGS += \
		--prefix=$(STAGING_DIR_HOST) \
		--enable-shared \
		--enable-static \
		--without-uuid \

ifeq ($(HOST_OS),Linux)
 MAKE_STATIC := STATIC=1
endif

define Host/Compile
	$(MAKE) -C $(HOST_BUILD_DIR) $(MAKE_STATIC)
endef

define Host/Install
	$(INSTALL_BIN) $(HOST_BUILD_DIR)/mkfs/mkfs.erofs $(STAGING_DIR_HOST)/bin/
endef

define Host/Clean
	rm -f $(STAGING_DIR_HOST)/bin/mkfs.erofs
endef

$(eval $(call HostBuild))
