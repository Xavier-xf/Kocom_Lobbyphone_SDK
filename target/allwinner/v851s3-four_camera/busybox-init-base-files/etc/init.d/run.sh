#!/bin/sh

echo "------------run run.sh file-----------------"
insmod /lib/modules/4.9.191/8188fu.ko
cat /sys/devices/platform/soc/usbc0/usb_host
mkdir /var/run
mkdir /var/run/wpa_supplicant


