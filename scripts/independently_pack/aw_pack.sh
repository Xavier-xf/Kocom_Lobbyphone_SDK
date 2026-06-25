#!/bin/bash

usage_help()
{
	echo "------usage-----"
	echo "run ./pack.sh"
	echo "tina image is creat on ./out"
}

aw_pack_dir=
get_aw_pack_dir(){
	cd `dirname $0`
	aw_pack_dir=`pwd`
	echo "aw_pack_dir=${aw_pack_dir}"
	cd -
}

function print_red(){
    echo -e '\033[0;31;1m'
    echo $1
    echo -e '\033[0m'
}

get_aw_pack_dir

if [ ! -d ${aw_pack_dir}/config ]; then
	echo "input erro"
	usage_help
	exit 1
fi

if [ ! -d ${aw_pack_dir}/image ]; then
	echo "input erro"
	usage_help
	exit 1
fi

if [ ! -d ${aw_pack_dir}/other ]; then
	echo "input erro"
	usage_help
	exit 1
fi

rm -rf ${aw_pack_dir}/tmp
rm -rf ${aw_pack_dir}/out

mkdir -p ${aw_pack_dir}/tmp
mkdir -p ${aw_pack_dir}/out

#cp resource
cp -lrf ${aw_pack_dir}/config/* ${aw_pack_dir}/tmp
cp -lrf ${aw_pack_dir}/image/* ${aw_pack_dir}/tmp
cp -lrf ${aw_pack_dir}/other/* ${aw_pack_dir}/tmp

cd ${aw_pack_dir}/tmp

#make new partition mbr
cp sys_partition.fex sys_partition_tmp.fex
sed -i '/^[ \t]*downloadfile/d' sys_partition_tmp.fex
/bin/busybox unix2dos sys_partition_tmp.fex
../tools/script  sys_partition_tmp.fex > /dev/null
../tools/update_mbr sys_partition_tmp.bin 1 sunxi_mbr_tmp.fex > /dev/null
local PART_INDEX
local PART_NAME
local PART_SIZE
local PART_OFFSET
echo "" >> sys_config.fex
echo "[partitions]" >> sys_config.fex
echo "" >> sys_config.fex
# add one partition to sys_config
# PART_NAME="vital"
# PART_SIZE=`parser_mbr sunxi_mbr_tmp.fex get_size_by_name ${PART_NAME}`
# PART_OFFSET=`parser_mbr sunxi_mbr_tmp.fex get_offset_by_name ${PART_NAME}`
# #update to sys_config
# echo "[partitions/vital]" >> ${ROOT_DIR}/image/sys_config.fex
# echo "offset = ${PART_OFFSET}" >> ${ROOT_DIR}/image/sys_config.fex
# echo "size = ${PART_SIZE}" >> ${ROOT_DIR}/image/sys_config.fex

# add all partitions to sys_config
total_num=`../tools/parser_mbr sunxi_mbr_tmp.fex get_total_num`
let total_num--
for PART_INDEX in $( /usr/bin/seq 0 ${total_num} )
do
	PART_NAME=`../tools/parser_mbr sunxi_mbr_tmp.fex get_name_by_index ${PART_INDEX}`
	PART_SIZE=`../tools/parser_mbr sunxi_mbr_tmp.fex get_size_by_index ${PART_INDEX}`
	PART_OFFSET=`../tools/parser_mbr sunxi_mbr_tmp.fex get_offset_by_index ${PART_INDEX}`
	echo "[partitions/${PART_NAME}]" >> sys_config.fex
	echo "offset = ${PART_OFFSET}" >> sys_config.fex
	echo "size = ${PART_SIZE}" >> sys_config.fex
	echo "" >> sys_config.fex
done

rm -f sys_partition_tmp.fex sys_partition_tmp.bin sunxi_mbr_tmp.fex dlinfo.fex

../tools/script sys_partition.fex > /dev/null

#default flash size 32M = 65535 sector ,logical offset = 2016 sector, full_img need to fixup this
../tools/update_mbr sys_partition.bin 1 sunxi_mbr_nor.fex dlinfo.fex 65535 2016 1
if [ $? -ne 0 ]; then
	echo -e "\033[31m----update_mbr_gpt failed , need fixup config/sys_partition.fex----\033[0m"
	exit 1
fi

#creat AW image
../tools/dragon image.cfg sys_partition.fex
if [ $? != 0 ]; then
	echo "make AW image fail !!"
	exit 1
fi

image_name=`ls tina*.img`

cp ${image_name} ../out

cd -
rm -rf ${aw_pack_dir}/tmp

echo "aw pack finish !"

echo "-------------------- aw image file in --------------------"
echo ""
print_red "${aw_pack_dir}/out/${image_name}"


