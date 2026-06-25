#
# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This file is executed by build/envsetup.sh, and can use anything
# defined in envsetup.sh.
#
# In particular, you can add lunch options with the add_lunch_combo
# function: add_lunch_combo generic-eng

function mkmpp
{
    local curDir=$(pwd)
    croot
    make package/allwinner/eyesee-mpp/external/compile $@
    make package/allwinner/eyesee-mpp/system/public/compile $@
    make package/allwinner/eyesee-mpp/middleware/compile $@
	if [ "x$?" == "x0" ];then
		echo mpp_install
		make package/install $@
	else echo mpp_compile_failed,skip_install;
	fi
    cd ${curDir}
}

: <<'COMMENTBLOCK'
function mkapp
{
    local curDir=$(pwd)
    croot
    make package/allwinner/eyesee-mpp/framework/compile $@
	if [ "x$?" == "x0" ];then
		make package/allwinner/eyesee-mpp/custom_aw/compile $@
		if [ "x$?" == "x0" ];then
			make package/install $@
		else echo app_compile_filed;
		fi
	else echo frmw_compile_failed;
	fi

    cd ${curDir}
}
COMMENTBLOCK

function mkall()
{
    local curDir=$(pwd)
    croot
    make $@
    cd ${curDir}
}

function cleanmpp()
{
    local curDir=$(pwd)
    croot
    make package/allwinner/eyesee-mpp/middleware/clean $@
    make package/allwinner/eyesee-mpp/system/public/clean $@
    make package/allwinner/eyesee-mpp/external/clean $@
    cd ${curDir}
}

function cleanapp
{
    local curDir=$(pwd)
    croot
    make package/allwinner/eyesee-mpp/framework/clean $@
    cd ${curDir}
}

function cleanall()
{
    local curDir=$(pwd)
    croot
    make package/clean
    make target/clean
    make distclean
    ckernel && rm -rf user_headers
    cd ${curDir}
}


cmpp_p ()
{
    local curDir=$(pwd);
    croot;
    cd package/allwinner/eyesee-mpp/middleware
}

cmpp_s ()
{
    croot;
    cd external/eyesee-mpp/middleware/sun8iw21
}

cfw_p ()
{
    croot;
    cd package/allwinner/eyesee-mpp/framework
}

cfw_s ()
{
    croot;
    cd external/eyesee-mpp/framework/sun8iw21
}

cisp ()
{
    cmpp_s;
    cd ./media/LIBRARY/libisp
}

mkfrmw ()
{
    local curDir=$(pwd);
    croot;
    make package/allwinner/eyesee-mpp/framework/compile $@;
    cd ${curDir}
}


function switch_8m()
{
   local T=${TINA_TOP}
    [ -z "$T" ] \
        && echo "Couldn't locate the top of the tree.  Try setting TOP." \
        && return
    [ -z "${TARGET_BOARD}" ] && "Please lunch your combo firstly" && return


    read -p "[warning] Do you want to overwrite the current configuration with the 8M spinor configuration？ [yes/no]: " answer


    answer=$(echo $answer | tr '[:upper:]' '[:lower:]')

    # check input
    if [[ $answer == "yes" || $answer == "y" ]]; then
	#This scrpit will rewite some configs file with 8M configs
	#
	#may change the config file below
	#
	#defconfig
	#config-4.9
	#moudules.mk
	#boot_package_nor.fex
	#uboot_board.dts
	#board.dts

	#defconfig
	if [ -f ${TINA_TOP}/target/allwinner/${TARGET_BOARD}/defconfig_for_8M ]; then

		cp ${TINA_TOP}/target/allwinner/${TARGET_BOARD}/defconfig_for_8M ${TINA_TOP}/target/allwinner/${TARGET_BOARD}/defconfig -v
	fi

	#config-4.9
	if [ -f ${LICHEE_BOARD_CONFIG_DIR}/linux/config-4.9_for_8M ]; then

		cp ${LICHEE_BOARD_CONFIG_DIR}/linux/config-4.9_for_8M ${LICHEE_BOARD_CONFIG_DIR}/linux/config-4.9 -v
	fi

	#boot_package_nor.cfg
	if [ -f ${LICHEE_BOARD_CONFIG_DIR}/linux/boot_package_nor_for_8M.cfg ]; then

		cp ${LICHEE_BOARD_CONFIG_DIR}/linux/boot_package_nor_for_8M.cfg ${LICHEE_BOARD_CONFIG_DIR}/linux/boot_package_nor.cfg -v
	fi

	#uboot-board.dts
	if [ -f ${LICHEE_BOARD_CONFIG_DIR}/uboot-board_for_8M.dts ]; then
		cp ${LICHEE_BOARD_CONFIG_DIR}/uboot-board_for_8M.dts ${LICHEE_BOARD_CONFIG_DIR}/uboot-board.dts -v
	fi

	#board.dts
	if [ -f ${LICHEE_BOARD_CONFIG_DIR}/board_for_8M.dts ]; then
		cp ${LICHEE_BOARD_CONFIG_DIR}/board_for_8M.dts ${LICHEE_BOARD_CONFIG_DIR}/board.dts -v
	fi

	#sys_partition_nor.fex
	if [ -f ${LICHEE_BOARD_CONFIG_DIR}/linux/sys_partition_nor_for_8M.fex ]; then

		cp ${LICHEE_BOARD_CONFIG_DIR}/linux/sys_partition_nor_for_8M.fex ${LICHEE_BOARD_CONFIG_DIR}/linux/sys_partition_nor.fex -v
	fi


    elif [[ $answer == "no" || $answer == "n" ]]; then
        echo "overwriting the current configuration with the 8M spinor configuration fail"
        return

    else
        echo "input erro，please input yes/y or no/n。"
        return
    fi


}
