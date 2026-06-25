#!/bin/bash
#
# This script is a hook and it is be run by SUPPORT_PACK_OUT_OF_TINA function
#
# You can add commands here for copy some files or resource to aw_pack_src
# directory
#
# Show aw_pack_src directory name, you should copy file to correct directory
#
#./aw_pack_src
#|--aw_pack.sh  #执行此脚本即可在aw_pack_src/out/目录生成固件
#|--config      #打包配置文件
#|--image       #各种镜像文件，可替换，但不能改文件名
#|    |--boot0_nand.fex        #nand介质boot0镜像
#|    |--boot0_sdcard.fex      #SD卡boot0镜像
#|    |--boot0_spinor.fex      #nor介质boot0镜像
#|    |--boot0_spinor.fex      #nor介质boot0镜像
#|    |--boot_package.fex      #nand和SD卡uboot镜像
#|    |--boot_package_nor.fex  #nor介质uboot镜像
#|    |--env.fex               #env环境变量镜像
#|    |--boot.fex              #内核镜像
#|    |--rootfs.fex            #rootfs镜像
#|--other       #打包所需的其他文件, 这里会放模型文件
#|--out         #固件生成目录
#|--tmp         #打包使用的临时目录
#|--tools       #工具
#|--rootfs      #存放rootfs的tar.gz打包,给二次修改使用
#|--lib_aw      #拷贝全志方案的库文件，如多媒体组件eyesempp等,
#		给应用app编译链接使用(没有选择这些库，则可能是空文件).
#|--README      #关于板级方案的一些说明，例如分区布局等等(无说明则没有这个文件)

#
#NOTE: input parameter $1 is a path of aw_pack_src
#

#author: wuguanling@allwinnertech.com

#This script of this project need to copy eyesempp lib to aw_pack_src/lib_aw
if [ -d ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/eyesee-mpp ]; then
	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/eyesee-mpp $1/lib_aw/lib
	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/include/eyesee-mpp  $1/lib_aw/include

	if [ -f ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libz.a ]; then
		cp ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libz.a  $1/lib_aw/lib/eyesee-mpp
	fi

	if [ -f ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libexpat.a ]; then
		cp ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libexpat.a  $1/lib_aw/lib/eyesee-mpp
	fi
fi

#cp person detect lib and include
if [ -d ${LICHEE_PACK_OUT_DIR}/../compile_dir/target/viplite-driver ]; then
	mkdir -p $1/lib_aw/lib/viplite-driver
	mkdir -p $1/lib_aw/include/viplite-driver
	mkdir -p $1/other/models/viplite-driver

	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libVIPlite.a $1/lib_aw/lib/viplite-driver
	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libVIPuser.a $1/lib_aw/lib/viplite-driver


	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libawnn.a  $1/lib_aw/lib/viplite-driver
	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/include/viplite-driver/*.h $1/lib_aw/include/viplite-driver
	#模型文件
	cp -rf  ${LICHEE_PACK_OUT_DIR}/../compile_dir/target/viplite-driver/algo_aw/pdet/*.nb $1/other/models/viplite-driver

	if [ -f ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libpix_facekit.a ]; then
		cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libpix_facekit.a $1/lib_aw/lib/viplite-driver
		cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/include/viplite-driver/*.h $1/lib_aw/include/viplite-driver

	fi

	if [ -f ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libpix_facekit_api.a ]; then
		cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libpix_facekit_api.a $1/lib_aw/lib/viplite-driver
		cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/include/viplite-driver/*.h $1/lib_aw/include/viplite-driver
		#模型文件
		cp -rf  ${LICHEE_PACK_OUT_DIR}/../compile_dir/target/viplite-driver/third-party/pixtalks_face_palm_kit/model/*.bin $1/other/models/viplite-driver
	fi

fi

#npu lib
if [ -d ${LICHEE_PACK_OUT_DIR}/../compile_dir/target/libawnn_full ]; then
	mkdir -p $1/lib_aw/lib/libawnn_full
	mkdir -p $1/lib_aw/include/libawnn_full

	cp -rf ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libawnn_full.a  $1/lib_aw/lib/libawnn_full
	cp -rf ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/include/libawnn/*.h $1/lib_aw/include/libawnn_full

fi

#cp awaiisp lib and include
if [ -d ${LICHEE_PACK_OUT_DIR}/../compile_dir/target/libawaiisp ]; then
	mkdir -p $1/lib_aw/lib/libawaiisp
	mkdir -p $1/lib_aw/include/libawaiisp
	mkdir -p $1/other/models/libawaiisp

	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/lib/libawaiisp.a $1/lib_aw/lib/libawaiisp
	cp -rf  ${LICHEE_PACK_OUT_DIR}/../staging_dir/target/usr/include/libawaiisp/*.h $1/lib_aw/include/libawaiisp
	cp -rf  ${LICHEE_PACK_OUT_DIR}/../compile_dir/target/libawaiisp/models/*.nb $1/other/models/libawaiisp

fi

#list lib
if [ -d ${LICHEE_PACK_OUT_DIR}/../compile_dir/target/libawlist ]; then
	mkdir -p $1/lib_aw/include/libawlist

	cp -rf ${LICHEE_PACK_OUT_DIR}/../compile_dir/target/libawlist/src/include/* $1/lib_aw/include/libawlist
fi


