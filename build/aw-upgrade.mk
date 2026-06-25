#
# Copyright (C) 2006-2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# author:henrisk.ho

define Aw/BuildUpgradeImage/prepare
	#$(1): target_dir #$(2): temp_usr

	#prepare for base system (no include /usr)
	#for procd
	-mv $(1)/usr/lib/libjson-c.so.* $(1)/lib/
	-mv $(1)/usr/bin/readlink $(1)/bin/
	-mv $(1)/usr/bin/basename $(1)/bin/
	-mv $(1)/usr/bin/jshn $(1)/bin/

	#for wifi
	-mv $(1)/usr/sbin/wpa* $(1)/sbin/
	-mv $(1)/usr/sbin/hostapd* $(1)/sbin/
	-mv $(1)/usr/lib/libnl-tiny.so $(1)/lib/

	#for filesystem tools
	-mv $(1)/usr/sbin/mke2fs $(1)/sbin/
	-mv $(1)/usr/sbin/mkfs.ext4 $(1)/sbin/
	-mv $(1)/usr/sbin/mkfs.jffs2 $(1)/sbin/

	-mv $(1)/usr/lib/libext2fs.so* $(1)/lib/
	-mv $(1)/usr/lib/libcom_err.so* $(1)/lib/
	-mv $(1)/usr/lib/libuuid.so* $(1)/lib/
	-mv $(1)/usr/lib/libe2p.so* $(1)/lib/
	-mv $(1)/usr/lib/libz.so* $(1)/lib/

	#for tinymp3player
	-mv $(1)/usr/lib/libmad.so* $(1)/lib/
	-mv $(1)/usr/lib/libasound.so* $(1)/lib/

	#ssl
	-mv $(1)/usr/lib/libssl.so* $(1)/lib/

	#curl
	-mv $(1)/usr/lib/libcurl.so* $(1)/lib/

	#crypto
	-mv $(1)/usr/lib/libcrypto.so* $(1)/lib/

	find $(1)/usr/lib | grep libstdc++.so | xargs -i mv  {} $(1)/lib

	#for link /usr/bin /usr/sbin
	rm -rf $(2)
	mv $(1)/usr $(2)
ifeq (x$(CONFIG_SUNXI_MOVE_KO_TO_USR),xy)
	mkdir -p $(2)/$(MODULES_SUBDIR)
	-mv $(1)/$(MODULES_SUBDIR)/* $(2)/$(MODULES_SUBDIR)
	rm -rf $(1)/$(MODULES_SUBDIR)
	-ln -s /usr/$(MODULES_SUBDIR) $(1)/$(MODULES_SUBDIR)
	ls -lh $(2)/$(MODULES_SUBDIR)
ifeq (x$(CONFIG_PACKAGE_kmod-net-xr806),xy)
	mkdir -p $(2)/lib
	-mv $(1)/bin/wifi $(2)/bin/
	-mv $(1)/bin/wifi_daemon $(2)/bin/
	-mv $(1)/lib/libxrlink.so $(2)/lib/
	-mv $(1)/lib/netifd $(2)/lib/
	-ln -s /usr/bin/wifi $(1)/bin/wifi
	-ln -s /usr/bin/wifi_daemon $(1)/bin/wifi_daemon
	-ln -s /usr/lib/libxrlink.so $(1)/lib/libxrlink.so
	-ln -s /usr/lib/netifd $(1)/lib/netifd
endif
ifeq (x$(CONFIG_PACKAGE_kmod-audio),xy)
	mkdir -p $(2)/lib
	-mv $(1)/lib/libadecoder.so $(2)/lib/
	-mv $(1)/lib/libaencoder.so $(2)/lib/
	-mv $(1)/lib/libaw_aacdec.so $(2)/lib/
	-ln -s /usr/lib/libadecoder.so $(1)/lib/libadecoder.so
	-ln -s /usr/lib/libaencoder.so $(1)/lib/libaencoder.so
	-ln -s /usr/lib/libaw_aacdec.so $(1)/lib/libaw_aacdec.so
endif
endif
ifeq (x$(CONFIG_SUNXI_MOVE_ADB_TO_USR),xy)
	-mv $(1)/bin/adbd $(2)/bin/
	-mv $(1)/bin/adb_shell $(2)/bin/
	-ln -s /usr/bin/adbd $(1)/bin/adbd
	-ln -s /usr/bin/adb_shell $(1)/bin/adb_shell
endif
ifeq (x$(CONFIG_SUNXI_MOVE_UDEV_TO_USR),xy)
	mkdir -p $(2)/lib
	-mv $(1)/sbin/udevd $(2)/sbin/
	-mv $(1)/sbin/udevadm $(2)/sbin/
	-mv $(1)/lib/udev $(2)/lib/
	-ln -s /usr/sbin/udevd $(1)/sbin/udevd
	-ln -s /usr/sbin/udevadm $(1)/sbin/udevadm
	-ln -s /usr/lib/udev $(1)/lib/udev
endif
	mkdir -p $(1)/lib/functions
	mkdir -p $(1)/usr/share
	cp -r $(2)/bin $(1)/usr/
	find $(1)/usr/bin -type f -exec rm {} \;
	-cp $(2)/share/libubox/jshn.sh $(1)/lib/functions/
	-cp -r $(2)/share/* $(1)/usr/share/

endef

define Aw/BuildUpgradeImage/resume
	#$(1): target_dir #$(2): temp_usr
	rm -rf $(1)/usr
	mv $(2) $(1)/usr
endef

define Aw/BuildUpgradeImage/normal-prepare
	#$(1): target_dir #$(2): temp_usr
	$(call Aw/BuildUpgradeImage/prepare,$(1),$(2))
endef

define Aw/BuildUpgradeImage/normal-resume
	#$(1): target_dir #$(2): temp_usr
	$(call Aw/BuildUpgradeImage/resume,$(1),$(2))
endef

ifeq ($(CONFIG_TARGET_AW_OTA_INITRAMFS),y)
define Aw/BuildUpgradeImage/initramfs-prepare
	#$(1): target_dir #$(2): temp_usr
	$(call Aw/BuildUpgradeImage/prepare,$(1),$(2))
endef

define Aw/BuildUpgradeImage/initramfs-resume
	#$(1): target_dir #$(2): temp_usr
	$(call Aw/BuildUpgradeImage/resume,$(1),$(2))
endef
else
define Aw/BuildUpgradeImage/initramfs-prepare
endef

define Aw/BuildUpgradeImage/initramfs-resume
endef
endif
