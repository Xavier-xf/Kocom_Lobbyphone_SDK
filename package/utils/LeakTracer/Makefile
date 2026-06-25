include $(TOPDIR)/rules.mk

PKG_NAME:=libleaktracer
PKG_VERSION:=1
PKG_RELEASE:=1

PKG_MAINTAINER:=AW-PDC-PD5

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)

include $(BUILD_DIR)/package.mk
include $(BUILD_DIR)/target.mk

define Package/$(PKG_NAME)
#  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=
  TITLE:=LeakTracer for c/c++
endef

define Package/libleaktracer/description
This library is designed to be a nice complement of valgrind and libduma (ex efence) to
detect leak memory. It simply override C/C++ allocation function (new, malloc, ...) to
keep then in a simple list that could be analysed later. The effective allocation is
still done by the underlying libc.
It can be used when valgrind is not ported on your platform, when libduma is using too
much virtual memory (embedded system), or when you're really interested in tracking only
memory leaks.
You may start/stop/dump monitoring allocation from the first allocation, when the program
receive a signal (for ex. SIGUSR1), or when you explicitly call a LeakTracer function.
endef

define Package/$(PKG_NAME)/config
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -r ./src/ $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/src -f tina.mk \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		CXXFLAGS="$(TARGET_CXXFLAGS)" \
		CPPFLAGS="$(TARGET_CPPFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CROSS_COMPILE="$(CONFIG_TOOLCHAIN_PREFIX)" \
		all
endef

define Build/Clean
	if [ -d $(PKG_BUILD_DIR)/src ]; then \
		$(MAKE) -C $(PKG_BUILD_DIR)/src -f tina.mk \
			ARCH="$(TARGET_ARCH)" \
			AR="$(TARGET_AR)" \
			CC="$(TARGET_CC)" \
			CXX="$(TARGET_CXX)" \
			CFLAGS="$(TARGET_CFLAGS)" \
			CXXFLAGS="$(TARGET_CXXFLAGS)" \
			CPPFLAGS="$(TARGET_CPPFLAGS)" \
			LDFLAGS="$(TARGET_LDFLAGS)" \
			CROSS_COMPILE="$(CONFIG_TOOLCHAIN_PREFIX)" \
			clean; \
	else \
		echo "$(PKG_BUILD_DIR)/src is not exist!"; \
	fi
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include/LeakTracer
	$(CP) $(PKG_BUILD_DIR)/src/libleaktracer.a $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/src/libleaktracer/include/* $(1)/usr/include/LeakTracer
endef

define Package/$(PKG_NAME)/install
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
