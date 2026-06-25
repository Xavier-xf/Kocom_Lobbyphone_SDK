#!/bin/bash

PATH_SH=$(cd "$(dirname "$0")";pwd)

test_dir="/mnt/sdcard"

threadB="a b c d e f g h i j k"
threadC="1 2 3 4 5 6 7 8 9 L !"
threadD="X Y Z R M L M N O P Q"
threadA="A B C D E F G H I J K"

timer=$1

function threadA_stress() {
	echo start to threadA_stress:$1
	for i in $(seq 1 $1)
	do
	echo start to threadA_stress done:$i
	for ch in $threadA
	do
		echo "${test_dir}/threadA${ch}"
		falloc_test "${test_dir}/threadA${ch}" 1048576 131072 524288 ${ch}

		if [ $? -ne 0 ]; then
			echo "threadA falloc_test failed"
			exit 1
		fi
	done
	done
}
function threadB_stress() {
	echo start to threadB_stress:$1
	for i in $(seq 1 $1)
	do
	echo start to threadB_stress done:$i
	for ch in $threadB
	do
		falloc_test "${test_dir}/threadB${ch}" 1048576 131072 1048576 ${ch}
		if [ $? -ne 0 ]; then
			echo "threadB falloc_test failed"
			exit 1
		fi
	done
	done
}
function threadC_stress() {
	echo start to threadC_stress:$1
	for i in $(seq 1 $1)
	do
	echo start to threadC_stress done:$i
	for ch in $threadC
	do
		falloc_test "${test_dir}/threadC${ch}" 2097152 131072 524288 ${ch}
		if [ $? -ne 0 ]; then
			echo "threadC falloc_test failed"
			exit 1
		fi
	done
	done
}
function threadD_stress() {
	echo start to threadD_stress:$1
	for i in $(seq 1 $1)
	do
	echo start to threadD_stress done:$i
	for ch in $threadD
	do
		falloc_test "${test_dir}/threadD${ch}" 2097152 131072 2097152 ${ch}
		if [ $? -ne 0 ]; then
			echo "threadD falloc_test failed"
			exit 1
		fi
	done
	done
}

ch=a
echo "${test_dir}/threadA${ch}"

threadA_stress $timer &
threadB_stress $timer &
threadC_stress $timer &
threadD_stress $timer &
