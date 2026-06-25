#!/bin/sh

#
# only support v85X fastboot
#
kernel=0
rootfs=0
extend=0
firmware_path="null"
url_path="null"

dec2hex()
{
	printf "%x" $1
}

# update_partition_flag && boot_flag default -- 0xff,
# value  : 1 -- system  0 -- system_backup
# bit 3~7: reserve
# bit 2  ：extend分区，/usr directory,boot0 not use,system use
# bit 1  : rootfs: 1:system  0:system_backup
# bit 0  : kernel, 1:system  0:system_backup
udpate_system_nor_flash()
{
	dd if=/dev/mtdblock0 of=/tmp/boot0_flag skip=61439 ibs=1 bs=1 count=1 1>/dev/null 2>&1
	boot_flag=0x`xxd /tmp/boot0_flag | awk -F ' ' '{print $2}'`
	echo "system update_flag:$update_flag"
	case $update_flag in
		1)
			update_partition_flag=$((boot_flag&1))
			echo "update_partition_flag:$update_partition_flag"
			if [ $update_partition_flag == 1 ]; then
				update_partition_flag=$((boot_flag&0xfe))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernel partition B, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelB
				else
					echo "need input -i or -u Specify the upgrade path"

				fi
			else
				update_partition_flag=$((boot_flag|0x01))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernel partition A, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelA
				else
					echo "need input -i or -u Specify the upgrade path"

				fi

			fi
			;;
		2)
			update_partition_flag=$((boot_flag&2))
			echo "update_partition_flag:$update_partition_flag"
			if [ $update_partition_flag == 2 ]; then
				update_partition_flag=$((boot_flag&0xfd))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update rootfs partition B, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,rootfsB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,rootfsB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi
			else
				update_partition_flag=$((boot_flag|0x02))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update rootfs partition A, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,rootfsA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,rootfsA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			fi

			;;
		3)
			echo "update kernel rootfs"
			update_partition_flag=$((boot_flag&3))
			echo "update_partition_flag:$update_partition_flag"
			if [ $update_partition_flag == 0 ]; then
				update_partition_flag=$((boot_flag|0x3))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelA_rootfsA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelA_rootfsA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelA_rootfsA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi
			elif [ $update_partition_flag == 1 ]; then
				update_partition_flag=$((boot_flag&0xfc|0x02))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelB_rootfsA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelB_rootfsA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelB_rootfsA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			elif [ $update_partition_flag == 2 ]; then
				update_partition_flag=$((boot_flag&0xfc|0x01))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelA_rootfsB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelA_rootfsB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelA_rootfsB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			else
				update_partition_flag=$((boot_flag&0xfc))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelB_rootfsB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelB_rootfsB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelB_rootfsB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi
			fi

			;;
		4)
			echo "update extend"
			update_partition_flag=$((boot_flag&4))
			echo "update_partition_flag:$update_partition_flag"
			if [ $update_partition_flag == 4 ]; then
				update_partition_flag=$((boot_flag&0xfb))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update extend partition B, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,extendB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,extendB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi
			else
				update_partition_flag=$((boot_flag|0x04))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update extend partition A, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,extendA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,extendA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			fi

			;;
		5)
			echo "update kernel extend"
			update_partition_flag=$((boot_flag&5))
			echo "update_partition_flag:$update_partition_flag"
			if [ $update_partition_flag == 0 ] || [ $update_partition_flag == 2 ]; then
				update_partition_flag=$((boot_flag|0x5))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelA_extendA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelA_extendA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelA_extendA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi
			elif [ $update_partition_flag == 1 ] || [ $update_partition_flag == 3 ]; then
				update_partition_flag=$((boot_flag&0xfa|0x04))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelB_extendA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelB_extendA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelB_extendA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			elif [ $update_partition_flag == 4 ] || [ $update_partition_flag == 6 ]; then
				update_partition_flag=$((boot_flag&0xfa|0x01))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelA_extendB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelA_extendB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelA_extendB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			else
				update_partition_flag=$((boot_flag&0xfa))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelB_extendB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelB_extendB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelB_extendB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi
			fi

			;;
		6)
			echo "update rootfs extend"
			update_partition_flag=$((boot_flag&6))
			echo "update_partition_flag:$update_partition_flag"
			if [ $update_partition_flag == 0 ] || [ $update_partition_flag == 1 ]; then
				update_partition_flag=$((boot_flag|0x6))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update rootfsA_extendA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,rootfsA_extendA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,rootfsA_extendA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi
			elif [ $update_partition_flag == 4 ] || [ $update_partition_flag == 5 ]; then
				update_partition_flag=$((boot_flag&0xf9|0x02))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update rootfsA_extendB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,rootfsA_extendB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,rootfsA_extendB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			elif [ $update_partition_flag == 2 ] || [ $update_partition_flag == 3 ]; then
				update_partition_flag=$((boot_flag&0xf9|0x04))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update rootfsB_extendA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,rootfsB_extendA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,rootfsB_extendA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			else
				update_partition_flag=$((boot_flag&0xf9))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update rootfsB_extendB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,rootfsB_extendB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,rootfsB_extendB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi
			fi
			;;
		7)
			echo "udpate kernel rootfs extend"
			update_partition_flag=$((boot_flag&7))
			echo "update_partition_flag:$update_partition_flag"
			if [ $update_partition_flag == 0 ]; then
				update_partition_flag=$((boot_flag|0x7))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelA_rootfsA_extendA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelA_rootfsA_extendA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelA_rootfsA_extendA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi
			elif [ $update_partition_flag == 1 ]; then
				update_partition_flag=$((boot_flag&0xf8|0x06))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "update kernelB_rootfsA_extendA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelB_rootfsA_extendA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelB_rootfsA_extendA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			elif [ $update_partition_flag == 2 ]; then
				update_partition_flag=$((boot_flag&0xf8|0x05))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "kernelA_rootfsB_extendA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelA_rootfsB_extendA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelA_rootfsB_extendA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			elif [ $update_partition_flag == 3 ]; then
				update_partition_flag=$((boot_flag&0xf8|0x04))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "kernelB_rootfsB_extendA, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelB_rootfsB_extendA
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelB_rootfsB_extendA
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			elif [ $update_partition_flag == 4 ]; then
				update_partition_flag=$((boot_flag&0xf8|0x03))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "kernelA_rootfsA_extendB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelA_rootfsA_extendB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelA_rootfsA_extendB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			elif [ $update_partition_flag == 5 ]; then
				update_partition_flag=$((boot_flag&0xf8|0x02))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "kernelB_rootfsA_extendB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelB_rootfsA_extendB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelB_rootfsA_extendB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi


			elif [ $update_partition_flag == 6 ]; then
				update_partition_flag=$((boot_flag&0xf8|0x01))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "kernelA_rootfsB_extendB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelA_rootfsB_extendB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelA_rootfsB_extendB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi


			else
				update_partition_flag=$((boot_flag&0xf8))
				update_partition_flag=$(dec2hex $update_partition_flag)
				fw_setenv update_partition_flag $update_partition_flag
				echo "after update_partition_flag:$update_partition_flag"
				echo "kernelB_rootfsB_extendB, firmware_path:$firmware_path, url_path:$url_path"
				if [ $firmware_path != null ]; then
					echo "update from firmware_path"
					swupdate_cmd.sh -i $firmware_path -e stable,kernelB_rootfsB_extendB
				elif [ $url_path != null ]; then
					echo "update from url"
					swupdate_cmd.sh -d -u$url_path -e stable,kernelB_rootfsB_extendB
				else
					echo "need input -i or -u Specify the upgrade path"
				fi

			fi
			;;
		*)
			echo "Need to pass in with parameters"
			echo "-k not argument update kernel partition"
			echo "-r not argument update rootfs partition"
			echo "-e not argument update extend(/usr) partition"
			echo "-i need argument firmware path"
			echo "-u need argument url path "
			;;
	esac
}

get_flash_type()
{
	case "$(sed 's/ /\n/g' /proc/cmdline | awk -F= '/^root=/{print $2}')" in
		/dev/mmcblk*)
			echo "emmc"
			;;
		/dev/nand*|/dev/ubiblock*)
			echo nand
			;;
		/dev/mtdblock*)
			echo nor
			;;
	esac
}

switch_system()
{
	local flash_type=$(get_flash_type)
	echo "flash_type:$flash_type" > /dev/console
	case "${flash_type}" in
		nor)
			udpate_system_nor_flash
		;;
		emmc)
		#to do
		;;
		nand)
		#to do
		;;
	esac
}

while getopts ":krehi:u:" opt; do
	case $opt in
		"k")
			kernel=1
			;;
		"r")
			rootfs=2
			;;
		"e")
			extend=4
			;;
		"h")
			echo "-k not argument update kernel partition"
			echo "-r not argument update rootfs partition"
			echo "-e not argument update extend(/usr) partition"
			echo "-i need argument firmware path"
			echo "-u need argument url path "
			;;
		"i")
			echo "input  firmware path:$OPTARG"
			firmware_path=$OPTARG
			;;
		"u")
			echo "input url path:$OPTARG"
			url_path=$OPTARG
			;;
		":")
			echo "Option -$OPTARG requires an argument."
			exit 1
			;;
		?)
			echo "Invalid option: -$OPTARG index:$OPTIND"
			echo "-k not argument update kernel partition"
			echo "-r not argument update rootfs partition"
			echo "-e not argument update extend(/usr) partition"
			echo "-i need argument firmware path"
			echo "-u need argument url path "
			;;
	esac
done

update_flag=`expr $kernel + $rootfs + $extend`
fw_setenv update_flag $update_flag

switch_system


