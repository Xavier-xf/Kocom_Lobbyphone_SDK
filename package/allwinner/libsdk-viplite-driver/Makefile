include $(TOPDIR)/rules.mk

PKG_NAME:=viplite-driver
PKG_VERSION:=vip-1.8.0
PKG_RELEASE:=1

PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)

include $(BUILD_DIR)/package.mk

ifeq ($(CONFIG_TOOLCHAIN_LIBC), "musl")
    LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC := musl
else ifeq ($(CONFIG_TOOLCHAIN_LIBC), "glibc")
    LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC := glibc
endif
export LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  TITLE:=viplite-driver npu-runtime-test
  DEPENDS:=+libstdcpp
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) -fr ./sdk_release $(PKG_BUILD_DIR)/
	$(CP) -fr ./algo_aw $(PKG_BUILD_DIR)/
	$(CP) -fr ./third-party $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/algo_aw/det_demo \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)"  \
		CXXFLAGS="$(TARGET_CXXFLAGS)" \
		CPPFLAGS="$(TARGET_CPPFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)"
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk_release/library/$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC)/libVIPlite.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk_release/library/$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC)/libVIPlite.a $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk_release/library/$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC)/libVIPuser.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk_release/library/$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC)/libVIPuser.a $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/include/awnn.h $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/include/awnn_det.h $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/include/awnn_info.h $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/include/awnn_lib.h $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/include/cross_utils.h $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/include/awnn.h $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/library/$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC)/libawnn.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/library/$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC)/libawnn.a $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk_release/sdk/*.h $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/third-party/facekit/lib/libpix_facekit.a $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/third-party/facekit/inc/*.h $(1)/usr/include/$(PKG_NAME)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/third-party/pixtalks_face_palm_kit/lib/libpix_facekit_api.a $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/third-party/pixtalks_face_palm_kit/lib/libpix_facekit_api.so $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/third-party/pixtalks_face_palm_kit/inc/*.h $(1)/usr/include/$(PKG_NAME)
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/pdet/1.1.0_Beta.nb $(1)/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk_release/library/$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC)/libVIPlite.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sdk_release/library/$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC)/libVIPuser.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/algo_aw/library/$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC)/libawnn.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/third-party/pixtalks_face_palm_kit/lib/libpix_facekit_api.so $(1)/usr/lib/
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/third-party/pixtalks_face_palm_kit/model/pix_face_lazy_v1_0_20230515.bin  $(1)/lib/
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/third-party/pixtalks_face_palm_kit/model/pix_face_v1_5_20230608.bin  $(1)/lib/
	#$(INSTALL_BIN) $(PKG_BUILD_DIR)/third-party/pixtalks_face_palm_kit/model/face_hand_detect_v1_7_20230725.bin  $(1)/lib/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
