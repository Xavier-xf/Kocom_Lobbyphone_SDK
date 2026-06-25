declare -A board_name_map=(
	["v851s_fastboot"]="v851-e907-perf2-board"
	["v837s_fastboot"]="v837s-e907-perf2-board"
	["v851se_fastboot"]="v851se-e907-perf2-board"
	["v851s_fastboot_dual_camera"]="v851-e907-perf2-dual-camera-board"
	["v851se_fastboot_dual_camera"]="v851se-e907-perf2-dual-camera-board"
	["v851s_fastboot_mpp"]="v851-e907-perf2-board"
	["v851s_fastboot_mpp_dual_camera"]="v851-e907-perf2-dual-camera-board"
	["v851s_fastboot_sc1721v"]="v851-e907-sc1721v-board"
	["v853s_fastboot"]="v853s-e907-perf4-board"
	["v853s_fastboot_dual_camera"]="v853s-e907-perf4-dual-camera-board"
	["v853s_fastboot_three_camera"]="v853s-e907-perf4-three-camera-board"
	["v853s_fastboot_mpp"]="v853s-e907-perf4-board"
	["v853s_fastboot_mpp_dual_camera"]="v853s-e907-perf4-dual-camera-board"
	["r853_scanp"]="v853-e907-ver1-board"
	["v853_perf1"]="v853-e907-ver1-board"
	["v853_fastboot"]="v853-e907-perf1-board"
	["v853_fastboot_emmc"]="v853-e907-perf4-board"
	["v853s_perf1"]="v853s-e907-perf1-board"
	["v853s_fastboot_emmc"]="v853s-e907-perf4-board"
	["v851s3_fastboot"]="v851s3-e907-perf2-board"
	["v851s_fastboot_sc1730v1_dual"]="v851-e907-sc1730v1-dual-board"
	["v851s3_fastboot_aiisp"]="v851s3-e907-aiisp-board"
	["v851s3_fastboot_melis_aiisp"]="v851s3-e907-melis-aiisp_board"
	["v853_fastboot_aov"]="v853-e907-perf1-aov-board"
)

function get_melis_board_name() {
	for key in $(echo ${!board_name_map[*]})
	do
		if [ $TARGET_PRODUCT == $key ]; then
			echo ${board_name_map[$TARGET_PRODUCT]}
		fi
	done
}

function update_melis_fw() {
	local T=$(gettop)
	local melis_board=$(get_melis_board_name)
	local melis_root=$T/lichee/melis-v3.0
	local src=$T/lichee/melis-v3.0/source/ekernel/arch/boot/zImage
	local dst_prefix=$T/device/config/chips/${TARGET_PLATFORM}/configs
	local dst_plan=${TARGET_BOARD##*-}

	if [ -z "${melis_board}" ]; then
		echo "Melis: Invalid Melis Board name."
		return
	fi

	if [ ! -e $src ]; then
		echo "Melis: melis30.elf not exist, skip it."
		return
	fi

	if [ -e $dst ]; then
		dst_time=`stat -c %Y $dst`
		src_time=`stat -c %Y $src`
		time_change=$((src_time - dst_time))

		if [ $time_change -le 0 ]; then
			return
		fi
	fi

	# copying fw to configs directory
	if [ -e "$dst_prefix/$dst_plan/riscv.fex" ]; then
		echo "COPY    ekernel/arch/boot/zImage ----> ${dst_plan}/riscv.fex"
		cp -f ${src} ${dst_prefix}/${dst_plan}/riscv.fex 2> /dev/null
		# strip melis firmware
		cd ${melis_root}
		./source/tools/scripts/melis-build.sh ${melis_board} strip ${dst_prefix}/${dst_plan}/riscv.fex
		cd -
	else
		echo "COPY    ekernel/arch/boot/zImage ----> default/riscv.fex"
		cp -f ${src} ${dst_prefix}/default/riscv.fex 2> /dev/null
		# strip melis firmware
		cd ${melis_root}
		./source/tools/scripts/melis-build.sh ${melis_board} strip ${dst_prefix}/default/riscv.fex
		cd -
	fi
}

function mmelis() {
	local T=$(gettop)
	local melis_board=$(get_melis_board_name)
	local melis_path=$T/lichee/melis-v3.0
	local pwd=`pwd`

	if [ "$melis_board" ]; then
		cd $melis_path
		./source/tools/scripts/melis-build.sh $melis_board $@
		if [ "$1" == "oldconfig" ]; then
			cp -ravf source/.config source/projects/$melis_board/configs/defconfig
		fi
		cd $pwd
		update_melis_fw
	else
		echo "melis_board not define, this board not defined in build/expand_melis.sh"
	fi
}

function cmelis() {
	local T=$(gettop)
	local melis_path=$T/lichee/melis-v3.0

	if [ "$melis_path" ]; then
		cd $melis_path
	else
		echo "$melis_path not exist."
	fi
}

function make_melis() {
	local dst_prefix=$T/device/config/chips/${TARGET_PLATFORM}/configs
	local dst_plan=${TARGET_BOARD##*-}
	local dst=${dst_prefix}/default/riscv.fex

	if [ -e "$dst_prefix/$dst_plan/riscv.fex" ]; then
		dst=$dst_prefix/$dst_plan/riscv.fex
	fi

	MELIS_NEWEST_FILE=`get_newest_file $T/lichee/melis-v3.0/source`
	MELIS_REBUILD_FLAG=`echo "$MELIS_REBUILD_FLAG $TARGET_BOARD $dst" | md5sum | awk '{print ".newest-"$1".patch"}'`

	if [ ! -f $T/lichee/melis-v3.0/source/$MELIS_REBUILD_FLAG ]; then
		mmelis $@

		rm -rf $T/lichee/melis-v3.0/source/.newest-*.patch
		MELIS_NEWEST_FILE=`get_newest_file $T/lichee/melis-v3.0/source`
		MELIS_REBUILD_FLAG=`echo "$MELIS_REBUILD_FLAG $TARGET_BOARD" | md5sum | awk '{print ".newest-"$1".patch"}'`
		touch $T/lichee/melis-v3.0/source/$MELIS_REBUILD_FLAG
	else
		echo -e "no need rebuild melis, the newest file is: \n\t$MELIS_NEWEST_FILE"
	fi
}
