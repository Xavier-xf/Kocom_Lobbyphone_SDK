Package/ssv6158-firmware = $(call Package/firmware-default,ssv6158 jixian firmware)
define Package/ssv6158-firmware/install
	$(INSTALL_DIR) $(1)/$(FIRMWARE_PATH)
	$(INSTALL_DATA) \
		$(TOPDIR)/package/firmware/linux-firmware/ssv6155/ssv6x5x-wifi.cfg \
		$(1)/$(FIRMWARE_PATH)/
endef
$(eval $(call BuildPackage,ssv6158-firmware))
