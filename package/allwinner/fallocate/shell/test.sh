#!/bin/bash

PATH_SH=$(cd "$(dirname "$0")";pwd)

threadA="A B C D E F G H I J K"

for i in $(seq 1 $1)
do
	echo $i
done
