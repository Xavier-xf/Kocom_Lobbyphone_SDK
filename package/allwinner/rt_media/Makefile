############################################## # OpenWrt Makefile for eyesee-mpp-system
#
#
# Most of the variables used here are defined in
# the include directives below. We just need to
# specify a basic description of the package,
# where to build our program, where to find
# the source files, and where to install the
# compiled program on the router.
#
# Be very careful of spacing in this file.
# Indents should be tabs, not spaces, and
# there should be no trailing whitespace in
# lines that are not commented.
#
##############################################
include $(TOPDIR)/rules.mk
include $(BUILD_DIR)/kernel.mk

# Name and release number of this package
PKG_NAME:=rt_media
PKG_VERSION:=1.0
PKG_RELEASE:=1

# This specifies the directory where we're going to build the program.
# The root build directory, $(BUILD_DIR), is by default the build_mipsel
# directory in your OpenWrt SDK directory
PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)
SRC_COMPILE_DIR := $(PKG_BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)
SRC_CODE_DIR := $(TOPDIR)/external/fast-user-adapter/rt_media

# This specifies the directory where file depends. when files in it update, increasing compile will be enable.
PKG_FILE_DEPENDS:= $(SRC_CODE_DIR)

PKG_BUILD_DEPENDS := libbaselist

include $(BUILD_DIR)/package.mk

ifeq ($(CONFIG_TOOLCHAIN_LIBC), "musl")
    RT_MEDIA_TOOLCHAIN_LIBC := musl
else ifeq ($(CONFIG_TOOLCHAIN_LIBC), "glibc")
    RT_MEDIA_TOOLCHAIN_LIBC := glibc
endif
export RT_MEDIA_TOOLCHAIN_LIBC

ifeq ($(CONFIG_rt_media_support_venc_param_debug),y)
    RT_MEDIA_SUPPORT_VENC_PARAM_DEBUG := Y
else
    RT_MEDIA_SUPPORT_VENC_PARAM_DEBUG := N
endif
export RT_MEDIA_SUPPORT_VENC_PARAM_DEBUG

export CONFIG_rt_media_demo_compile_static
export CONFIG_rt_media_demo_log_use_printf
export CONFIG_rt_media_demo_log_use_glog

# Specify package information for this program.
# The variables defined here should be self explanatory.
# If you are running Kamikaze, delete the DESCRIPTION
# variable below and uncomment the Kamikaze define
# directive for the description below
define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Allwinner
  SUBMENU:=rt_media
  TITLE:=rt_media for Tina Linux
  DEPENDS:=+libawion +libglogCWrapper +rt_media_support_venc_param_debug:libexpat
endef

define Package/$(PKG_NAME)-AWVideoInputDemo
  SECTION:=utils
  CATEGORY:=Allwinner
  SUBMENU:=rt_media
  TITLE:=demos for AWVideoInput
  DEPENDS:=$(PKG_NAME) +rt_media_demo_face_recognition:viplite-driver
endef

define Package/$(PKG_NAME)/config
source "$(SOURCE)/Config.in"
endef

define Package/$(PKG_NAME)-AWVideoInputDemo/config
source "$(SOURCE)/Config-demo.in"
endef

# Uncomment portion below for Kamikaze and delete DESCRIPTION variable above
define Package/$(PKG_NAME)/description
  fast user adapter
endef

define Package/$(PKG_NAME)-AWVideoInputDemo/description
  rt_media-AWVideoInputDemo contains all samples designed to test AWVideoInput and to show how to use AWVideoInput.
endef

# Specify what needs to be done to prepare for building the package.
# In our case, we need to copy the source files to the build directory.
# This is NOT the default.  The default uses the PKG_SOURCE_URL and the
# PKG_SOURCE which is not defined here to download the source from the web.
# In order to just build a simple program that we have just written, it is
# much easier to do it this way.

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	if [ -d $(SRC_COMPILE_DIR) ]; then \
		rm -rf $(SRC_COMPILE_DIR); \
	fi
	ln -s $(SRC_CODE_DIR) $(SRC_COMPILE_DIR)
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(SRC_COMPILE_DIR)/api_adapter \
		CC="$(TARGET_CC)" AR="$(TARGET_AR)"  \
		CXX="$(TARGET_CXX)" \
		CFLAGS0="$(TARGET_CFLAGS)" \
		CXXFLAGS0="$(TARGET_CXXFLAGS)" \
		LINUX_USER_HEADERS="$(LINUX_DIR)/user_headers"

ifeq ($(CONFIG_rt_media_demo),y)
	$(MAKE) -C $(SRC_COMPILE_DIR)/demo \
			CC="$(TARGET_CC)" \
			CXX="$(TARGET_CXX)" \
			CFLAGS0="$(TARGET_CFLAGS)" \
			CXXFLAGS0="$(TARGET_CXXFLAGS)" \
			LINUX_USER_HEADERS="$(LINUX_DIR)/user_headers";
endif
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/rt_media
	$(INSTALL_DIR) $(1)/usr/include/rt_media
	if [ -d $(SRC_COMPILE_DIR)/api_adapter/InstallDev ]; then \
		$(CP) -r $(SRC_COMPILE_DIR)/api_adapter/InstallDev/* $(1); \
	else \
		echo "fatal error! why $(SRC_COMPILE_DIR)/api_adapter/InstallDev not exist?"; \
	fi
endef

define Build/Clean
	if [ -d $(SRC_COMPILE_DIR) ]; then \
		$(MAKE) -C $(SRC_COMPILE_DIR)/api_adapter clean; \
		$(MAKE) -C $(SRC_COMPILE_DIR)/demo clean; \
	else \
		echo "$(SRC_COMPILE_DIR) is not exist!"; \
	fi
endef

# We do not need to define Build/Configure or Build/Compile directives
# The defaults are appropriate for compiling a simple program such as this one

# Specify where and how to install the program. Since we only have one file,
# the helloworld executable, install it by copying it to the /bin directory on
# the router. The $(1) variable represents the root directory on the router running
# OpenWrt. The $(INSTALL_DIR) variable contains a command to prepare the install
# directory if it does not already exist.  Likewise $(INSTALL_BIN) contains the
# command to copy the binary file from its current location (in our case the build
# directory) to the install directory.
define Package/$(PKG_NAME)/install
	if [ -d $(SRC_COMPILE_DIR)/api_adapter/install ]; then \
		$(CP) -r $(SRC_COMPILE_DIR)/api_adapter/install/* $(1); \
	else \
		echo "fatal error! why $(SRC_COMPILE_DIR)/api_adapter/install not exist?"; \
	fi
endef

define Package/$(PKG_NAME)-AWVideoInputDemo/install
	$(INSTALL_DIR) $(1)/bin

	if [ "$(CONFIG_rt_media_demo_video_in)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_video_in $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_video_in_3chn)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_video_in_3chn $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_roi)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_roi $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_force_i_frame)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_force_i_frame $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_motion_search)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_motion_search $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_encoder)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_encoder $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_get_yuv)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_get_yuv $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_osd)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_osd $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_orl)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_orl $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_reset_size)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_reset_size $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_gdc)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_gdc $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_isp_api)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_isp_api $(1)/bin; \
	fi
	if [ "$(CONFIG_rt_media_demo_face_recognition)" = "y" ]; then \
		$(CP) $(SRC_COMPILE_DIR)/demo/demo_face_recognition $(1)/bin; \
	fi
endef

# This line executes the necessary commands to compile our program.
# The above define directives specify all the information needed, but this
# line calls BuildPackage which in turn actually uses this information to
# build a package.

$(eval $(call BuildPackage,$(PKG_NAME)))
$(eval $(call BuildPackage,$(PKG_NAME)-AWVideoInputDemo))
