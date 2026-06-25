#!/bin/sh

#检查vi是否有在工作
check_csi_work()
{
	echo 0x02001C04 > /sys/class/sunxi_dump/dump
	data=$(cat /sys/class/sunxi_dump/dump)

	csi0_clk_enable=$(( ($data >> 31) & 1 ))

	# 判断bit_data的值并更新条件
	if [ $csi0_clk_enable -eq 0 ]; then
		echo "vi not open, please open vi first"
		return 0
	else
		return 1
	fi
}

check_sensor_mipi_clk()
{
echo "=======check sensor mipi clk========="

#检查是否有开vi，如果没开去cat 寄存器则会卡死
check_csi_work

# 检查返回值是否为0
if [ $? -eq 0 ]; then
	return
fi
#==============mipi 0====================
# 初始化变量
count=0
found_3=false
found_5=false
# 循环读取10次
while [ $count -lt 20 ]; do
  # 抓取数据并保存到变量中
	echo 0x058101f0 > /sys/class/sunxi_dump/dump
	data=$(cat /sys/class/sunxi_dump/dump)

	# 提取[19:16]位的数据
	bit_data=$(( ($data >> 16) & 0xF ))

  # 判断bit_data的值并更新条件
  if [ $bit_data -eq 3 ]; then
    found_3=true
  elif [ $bit_data -eq 5 ]; then
    found_5=true
  fi

  # 增加计数器
  count=$((count + 1))

done

# 根据条件输出结果
if $found_3 && $found_5; then
  echo "mipi0 sensor clk : OK, clk_mode: Non-Continuous Mode"
elif $found_3; then
  echo "mipi0 sensor clk : not clk"
elif $found_5; then
  echo "mipi0 sensor clk : OK, clk_mode: Continuous Mode"
else
	if [ $bit_data -eq 4 ]; then
		echo "mipi0 sensor clk : fail,trnds(may be not link sensor)"
	else
		echo "mipi0 sensor clk : check fail(clk status:$bit_data)"
	fi
fi

#==============mipi 1====================
# 初始化变量
count=0
found_3=false
found_5=false
# 循环读取10次
while [ $count -lt 20 ]; do
  # 抓取数据并保存到变量中
	echo 0x058102f0  > /sys/class/sunxi_dump/dump
	data=$(cat /sys/class/sunxi_dump/dump)

	# 提取[19:16]位的数据
	bit_data=$(( ($data >> 16) & 0xF )) 

  # 判断bit_data的值并更新条件
  if [ $bit_data -eq 3 ]; then
    found_3=true
  elif [ $bit_data -eq 5 ]; then
    found_5=true
  fi

  # 增加计数器
  count=$((count + 1))

done

# 根据条件输出结果
if $found_3 && $found_5; then
  echo "mipi1 sensor clk : OK, clk_mode: Non-Continuous Mode"
elif $found_3; then
  echo "mipi1 sensor clk : erro, not clk"
elif $found_5; then
  echo "mipi1 sensor clk : OK, clk_mode: Continuous Mode"
else
	if [ $bit_data -eq 4 ]; then
		echo "mipi1 sensor clk : fail,trnds(may be not link sensor)"
	else
		echo "mipi1 sensor clk : check fail(clk status:$bit_data)"
	fi
fi

}

check_soc_mipi_d0_trnds()
{
echo "=======check mipi d0 trnds========="
#检查是否有开vi，如果没开去cat 寄存器则会卡死
check_csi_work

# 检查返回值是否为0
if [ $? -eq 0 ]; then
	return
fi
#==============mipi 0====================
  # 抓取数据并保存到变量中
	echo 0x058101f0 > /sys/class/sunxi_dump/dump
	data=$(cat /sys/class/sunxi_dump/dump)

	# 提取[19:16]位的数据
	bit_data=$(($data & 0xF ))
	#echo $bit_data

	if [ $bit_data -eq 4 ]; then
		echo "mipi0-d0 trnds: erro (TRNDS)"
	else
		echo "mipi0-d0 trnds: ok (not TRNDS)"
	fi
#==============mipi 1====================
  # 抓取数据并保存到变量中
	echo 0x058102f0 > /sys/class/sunxi_dump/dump
	data=$(cat /sys/class/sunxi_dump/dump)

	# 提取[19:16]位的数据
	bit_data=$(($data & 0xF ))
	#echo $bit_data

	if [ $bit_data -eq 4 ]; then
		echo "mipi1-d0 trnds: erro (TRNDS)"
	else
		echo "mipi1-d0 trnds: ok (not TRNDS)"
	fi
}

check_soc_mipi_int_erro()
{
echo "=======check mipi int erro========="
#检查是否有开vi，如果没开去cat 寄存器则会卡死
check_csi_work

# 检查返回值是否为0
if [ $? -eq 0 ]; then
	return
fi
#==============mipi 0====================
# 抓取数据并保存到变量中
  echo 0x05811118  > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据
  FRAME_SYNC_ERR_INT=$(( ($data >> 7) & 1 ))
  LINE_SYNC_ERR_INT=$(( ($data >> 8) & 1 ))
  ECC_WAN_INT=$(( ($data >> 9) & 1 ))
  ECC_ERR_INT=$(( ($data >> 10) & 1 ))
  CHECKSUM_ERR_INT=$(( ($data >> 11) & 1 ))
  E0T_ERR_INT=$(( ($data >> 12) & 1 ))

  # 初始化错误标志
  error_flag=false

  echo "mipi0:"
  # 检查是否有错误位被置为1
  if [ $FRAME_SYNC_ERR_INT -eq 1 ]; then
    echo "FRAME_SYNC_ERR_INT had erro"
    error_flag=true
  fi
  if [ $LINE_SYNC_ERR_INT -eq 1 ]; then
    echo "LINE_SYNC_ERR_INT had erro"
    error_flag=true
  fi
  if [ $ECC_WAN_INT -eq 1 ]; then
    echo "ECC_WAN_INT had erro"
    error_flag=true
  fi
  if [ $ECC_ERR_INT -eq 1 ]; then
    echo "ECC_ERR_INT had erro (This may cause not stream)"
    error_flag=true
  fi
  if [ $CHECKSUM_ERR_INT -eq 1 ]; then
    echo "CHECKSUM_ERR_INT had erro (This may cause not stream)"
    error_flag=true
  fi
  if [ $E0T_ERR_INT -eq 1 ]; then
    echo "E0T_ERR_INT had erro"
    error_flag=true
  fi
  # 如果没有任何错误，则输出消息
  if [ "$error_flag" = false ]; then
    echo "not erro"
  fi
#==============mipi 1====================
# 抓取数据并保存到变量中
  echo 0x05811518  > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据
  FRAME_SYNC_ERR_INT=$(( ($data >> 7) & 1 ))
  LINE_SYNC_ERR_INT=$(( ($data >> 8) & 1 ))
  ECC_WAN_INT=$(( ($data >> 9) & 1 ))
  ECC_ERR_INT=$(( ($data >> 10) & 1 ))
  CHECKSUM_ERR_INT=$(( ($data >> 11) & 1 ))
  E0T_ERR_INT=$(( ($data >> 12) & 1 ))

  # 初始化错误标志
  error_flag=false

  echo "mipi1:"
  # 检查是否有错误位被置为1
  if [ $FRAME_SYNC_ERR_INT -eq 1 ]; then
    echo "FRAME_SYNC_ERR_INT had erro"
    error_flag=true
  fi
  if [ $LINE_SYNC_ERR_INT -eq 1 ]; then
    echo "LINE_SYNC_ERR_INT had erro"
    error_flag=true
  fi
  if [ $ECC_WAN_INT -eq 1 ]; then
    echo "ECC_WAN_INT had erro"
    error_flag=true
  fi
  if [ $ECC_ERR_INT -eq 1 ]; then
    echo "ECC_ERR_INT had erro (This may cause not stream)"
    error_flag=true
  fi
  if [ $CHECKSUM_ERR_INT -eq 1 ]; then
    echo "CHECKSUM_ERR_INT had erro (This may cause not stream)"
    error_flag=true
  fi
  if [ $E0T_ERR_INT -eq 1 ]; then
    echo "E0T_ERR_INT had erro"
    error_flag=true
  fi
  # 如果没有任何错误，则输出消息
  if [ "$error_flag" = false ]; then
    echo "not erro"
  fi

}

check_dvp_polarity()
{
echo "=======check dvp polarity========="
#检查是否有开vi，如果没开去cat 寄存器则会卡死
check_csi_work

# 检查返回值是否为0
if [ $? -eq 0 ]; then
	return
fi

# 抓取数据并保存到变量中
  echo 0x05822004  > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)

  CLK_POL=$(( ($data >> 16) & 1 ))
  HERF_POL=$(( ($data >> 17) & 1 ))
  VREF_POL=$(( ($data >> 18) & 1 ))

  # 检查极性
  if [ $CLK_POL -eq 1 ]; then
	echo "DVP_CLK_POL: active in falling edge"
  else
	echo "DVP_CLK_POL: active in rising edge"
  fi

   if [ $HERF_POL -eq 1 ]; then
	echo "DVP_HERF_POL: positive"
  else
	echo "DVP_HERF_POL: negative"
  fi

   if [ $VREF_POL -eq 1 ]; then
	echo "DVP_VREF_POL: positive"
  else
	echo "DVP_VREF_POL: negative"
  fi
}

check_sensor_w_h_data()
{
echo "=======check sensor mipi width & height========="

#检查是否有开vi，如果没开去cat 寄存器则会卡死
check_csi_work

# 检查返回值是否为0
if [ $? -eq 0 ]; then
	return
fi

#==============mipi 0====================
# 抓取数据并保存到变量中
  echo 0x05820034  > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据

  # 提取width和high的值
  width=$(( ($data & 0x3FFF) ))
  height=$(( (($data >> 16) & 0x3FFF) ))

  echo "mipi0=> width:$width , height:$height"

#==============mipi 1====================
# 抓取数据并保存到变量中
  echo 0x05821034  > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据

  # 提取width和high的值
  width=$(( ($data & 0x3FFF) ))
  height=$(( (($data >> 16) & 0x3FFF) ))

  echo "mipi1=> width:$width , height:$height"
#============== dvp ====================
# 抓取数据并保存到变量中
  echo 0x05822034  > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据

  # 提取width和high的值
  width=$(( ($data & 0x3FFF) ))
  height=$(( (($data >> 16) & 0x3FFF) ))

  echo "dvp=> width:$width , height:$height"
}

check_dvp_parser_pclk()
{
	pclk_erro=true
echo "=======check dvp pclk cnt========="
#检查是否有开vi，如果没开去cat 寄存器则会卡死
check_csi_work

# 检查返回值是否为0
if [ $? -eq 0 ]; then
	return
fi

# 抓取数据并保存到变量中
  echo 0x05822010  > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据
 last_cnt=$(( (($data >> 28) & 0x7) ))

#==============dvp ====================
 #由于只存3bit 很少， 所以循环读取10次， 有不一样的则表示有变化
 count=0
 while [ $count -lt 5 ]; do
  # 抓取数据并保存到变量中

# 抓取数据并保存到变量中
  echo 0x05822010  > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据
 PCLK_CNT=$(( (($data >> 28) & 0x7) ))
 #echo "pclk_cnt:$PCLK_CNT"

  if [ "$last_cnt" -ne "$PCLK_CNT" ]; then
	pclk_erro=false
	count=20
  fi
    # 增加计数器
  count=$((count + 1))
done

  # 如果没有任何错误，则输出消息
  if [ "$pclk_erro" = false ]; then
    echo "DVP PCLK CNT: OK"
  else
	echo "DVP PCLK CNT: not clk"
  fi

}

check_ve_frame_cnt()
{
	echo "=======check ve ch stream ========="
	 # 初始化错误标志
	 echo "false" > /tmp/ve_erro_flag #使用文件作为标志
	rm /tmp/ve_table1.txt /tmp/ve_table2.txt 2>/dev/null
  # 获取当前的ve_base输出
  output1=$(cat /sys/kernel/debug/mpp/ve_base | grep "Start*")

# 使用循环逐行处理数据
echo "$output1" | while read -r line; do
  # 提取第一列中的数字
  ch_number=$(echo "$line" | sed -n 's/.*Ch\[\([0-9]\+\)\].*/\1/p')

  #提取第三列的值并去掉开头的字符"F"
  col3=$(echo "$line" | awk '{sub(/^F/, "", $3); print $3}')

	echo "$ch_number $col3" >> /tmp/ve_table1.txt
done

	#cat /tmp/ve_table1.txt

	sleep 10  #这里ve 节点有个刷新频率，默认120fps。 所以等待时间需要长一点。

  output1=$(cat /sys/kernel/debug/mpp/ve_base | grep "Start*")

# 使用循环逐行处理数据
echo "$output1" | while read -r line; do
  # 提取第一列中的数字
  ch_number=$(echo "$line" | sed -n 's/.*Ch\[\([0-9]\+\)\].*/\1/p')

  #提取第三列的值并去掉开头的字符"F"
  col3=$(echo "$line" | awk '{sub(/^F/, "", $3); print $3}')

	echo "$ch_number $col3" >> /tmp/ve_table2.txt
done

	table1=$(cat /tmp/ve_table1.txt)
	table2=$(cat /tmp/ve_table2.txt)

# 使用循环逐行处理表1的数据
echo "$table1" | while read -r line1; do
  col1_table1=$(echo "$line1" | awk '{print $1}')
  col2_table1=$(echo "$line1" | awk '{print $2}')

  # 使用循环逐行处理表2的数据
  echo "$table2" | while read -r line2; do
    col1_table2=$(echo "$line2" | awk '{print $1}')
    col2_table2=$(echo "$line2" | awk '{print $2}')

    # 检查是否符合条件
    if [ "$col1_table1" -eq "$col1_table2" ] && [ "$col2_table1" -eq "$col2_table2" ]; then
		echo "true" > /tmp/ve_erro_flag #使用文件作为标志
      echo "ve_ch[$col1_table1] may be not stream"
    fi
  done
done
	ve_erro_flag=$(cat /tmp/ve_erro_flag)
 # 如果没有任何错误，则输出消息
  if [ "$ve_erro_flag" = false ]; then
    echo "ve is ok"
  fi

  rm /tmp/ve_table1.txt /tmp/ve_table2.txt 2>/dev/null
  rm /tmp/ve_erro_flag 2>/dev/null
}

check_vi_frame_cnt()
{
	echo "=======show vi pipe line ========="
	#检查是否有开vi，如果没开去cat 寄存器则会卡死
	check_csi_work

	# 检查返回值是否为0
	if [ $? -eq 0 ]; then
		return
	fi

	#判断是常电还是快启：
	# 读取 /proc/cmdline 的内容并存储到 cmdline 变量中
	cmdline=$(cat /proc/cmdline)

	#快启查看vi0~vi3的节点
	check_vi_start=0
	check_vi_end=4

	#展示输入vi 的通路图：
	#获取vi_start ~ vi_end后面的出帧数, 如果vi_start = vi_end， 则直接star 到末尾
	if [ "$check_vi_start" -eq "$check_vi_end" ]; then
		cmd="cat /sys/kernel/debug/mpp/vi |  awk '/^vi$check_vi_start:/,0' | grep csi"
	else
		cmd="cat /sys/kernel/debug/mpp/vi |  awk '/^vi$check_vi_start:/,/^vi$check_vi_end:/ {print}' | grep \"vipp\""

	fi
	pipeline=$(eval $cmd)
	echo "$pipeline" | while read -r aline; do
		echo $aline
	done

	echo "=======check vi ch stream ========="
	 rm /tmp/vi_table1.txt /tmp/vi_table2.txt 2>/dev/null
	 # 初始化错误标志
	echo "false" > /tmp/vi_erro_flag #使用文件作为标志
	#获取vi_start ~ vi_end后面的出帧数, 如果vi_start = vi_end， 则直接star 到末尾
	if [ "$check_vi_start" -eq "$check_vi_end" ]; then
		cmd="cat /sys/kernel/debug/mpp/vi |  awk '/^vi$check_vi_start:/,0' | grep \"frame =>\""
	else
		cmd="cat /sys/kernel/debug/mpp/vi |  awk '/^vi$check_vi_start:/,/^vi$check_vi_end:/ {print}' | grep \"frame =>\""

	fi
	#echo $cmd
	output1=$(eval $cmd) #执行获取帧数命令

	#把出帧数取出来
# 使用循环逐行处理数据
vi=$check_vi_start
echo "$output1" | while read -r line; do
	# 使用sed和正则表达式提取数字部分
	#echo $line
	cnt_value=$(echo "$line" | sed -n 's/.* cnt: \([0-9]\+\),.*/\1/p')

	#echo cnt:$cnt_value
	#存储vi cnt 到文件
	echo $vi $cnt_value >> /tmp/vi_table1.txt
	vi=$((vi + 1))

done
	sleep 1

	#获取vi后面的出帧数
	output1=$(eval $cmd)
# 使用循环逐行处理数据
vi=$check_vi_start
echo "$output1" | while read -r line; do
	# 使用sed和正则表达式提取数字部分
	cnt_value=$(echo "$line" | sed -n 's/.* cnt: \([0-9]\+\),.*/\1/p')

	#echo cnt:$cnt_value
	#存储vi cnt 到文件
	echo $vi $cnt_value >> /tmp/vi_table2.txt
	vi=$((vi + 1))
done

	table1=$(cat /tmp/vi_table1.txt)
	table2=$(cat /tmp/vi_table2.txt)

# 使用循环逐行处理表1的数据
echo "$table1" | while read -r line1; do
  col1_table1=$(echo "$line1" | awk '{print $1}')
  col2_table1=$(echo "$line1" | awk '{print $2}')

  # 使用循环逐行处理表2的数据
  echo "$table2" | while read -r line2; do
    col1_table2=$(echo "$line2" | awk '{print $1}')
    col2_table2=$(echo "$line2" | awk '{print $2}')

    # 检查是否符合条件
    if [ "$col1_table1" -eq "$col1_table2" ] && [ "$col2_table1" -eq "$col2_table2" ]; then
      echo "vi_ch[$col1_table1] may be not stream"
	  echo "true" > /tmp/vi_erro_flag #使用文件作为标志
    fi
  done
done

  vi_erro_flag=$(cat /tmp/vi_erro_flag)
 # 如果没有任何错误，则输出消息
  if [ "$vi_erro_flag" = false ]; then
    echo "vi stream is ok"
  fi

  #rm /tmp/vi_table1.txt /tmp/vi_table2.txt 2>/dev/null
  #rm /tmp/vi_erro_flag 2>/dev/null
}

check_video_node()
{
	echo "=======check video node ========="
	cd /dev/
	#ls --color=never video* #一些板子可能没有color 选项，会崩溃
	ls video*
	cd - 1>/dev/null
}


check_sensor_i2c_sattus()
{
	echo "=======check sensor i2c sda status========="

	count=0
	#循环查看3个sensor的信息
	while [ $count -lt 3 ]; do
		file_path="/proc/device-tree/soc@03000000/vind@0/sensor@$count/status"
		# 检查文件是否存在
		if [ ! -f "$file_path" ]; then
			count=$((count+1))
			continue
		fi

		cmd="cat /proc/device-tree/soc@03000000/vind@0/sensor@$count/status"
		status=$(eval $cmd)
		#sensor 有开
		if [ "$status" = "okay" ]; then
			cmd="cat /proc/device-tree/soc@03000000/vind@0/sensor@$count/sensor${count}_mname"
			#echo "$cmd"
			sensor_name=$(eval $cmd)
			cmd="cat /proc/device-tree/soc@03000000/vind@0/sensor@$count/sensor${count}_twi_cci_id | hexdump -v -e '/1 \"%d\"' "
			#echo "$cmd"
			i2c=$(eval $cmd)
			#echo "sensor_name:$(eval $cmd) i2c:$i2c"
			#检查i2c的状态
			if [ "$i2c" -eq 000 ]; then
				echo 0x02502020 > /sys/class/sunxi_dump/dump ; i2c_id=0
			elif [ "$i2c" -eq 001 ]; then
				echo 0x02502420 > /sys/class/sunxi_dump/dump ; i2c_id=1
			elif [ "$i2c" -eq 002 ]; then
				echo 0x02502820 > /sys/class/sunxi_dump/dump ; i2c_id=2
			elif [ "$i2c" -eq 003 ]; then
				echo 0x02502c20 > /sys/class/sunxi_dump/dump ; i2c_id=3
			elif [ "$i2c" -eq 004 ]; then
				echo 0x02503020 > /sys/class/sunxi_dump/dump ; i2c_id=4
			else
				echo 0x02502020 > /sys/class/sunxi_dump/dump ; i2c_id=0
			fi
			# 获取/sys/class/sunxi_dump/dump的内容
			dump_value=$(cat /sys/class/sunxi_dump/dump)

			# 使用awk提取第4位数据
			bit_4=$(($dump_value & 0x10))
			# 判断第4位数据并打印相应的消息
			if [ "$bit_4" -eq 0 ]; then
			  echo "$sensor_name i2c-$i2c_id SDA_STATUS: Low，maybe erro"
			else
			  echo "$sensor_name i2c-$i2c_id SDA_STATUS: High, ok"
			fi
		fi
		#计数+1
		count=$((count + 1))
	done
}

check_sensor_mclk()
{
	echo "=======check csi mclk========="
	#检查是否有开vi，如果没开去cat 寄存器则会卡死
	check_csi_work

	# 检查返回值是否为0
	if [ $? -eq 0 ]; then
		return
	fi
#csi0
# 抓取数据并保存到变量中
  echo 0x02001c08   > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据
  csi0_clk_enable=$(( ($data >> 31) & 1 ))
  csi0_clk_src=$(( ($data >> 24) & 7 )) #时钟源
  csi0_clk_n=$(( ($data >> 8) & 2 )) #分配N
  csi0_clk_n=$((1 << $csi0_clk_n))
  #echo "csi0_clk_n:$csi0_clk_n"
  csi0_clk_m=$(( $data  & 8 )) #分配M
  csi0_clk_m=$((csi0_clk_m + 1))

  #找到主时钟
	 # 使用条件语句检查csi0_clk_src的值
	if [ "$csi0_clk_src" -eq 0 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "hosc " | awk '{print $4}')
	elif [ "$csi0_clk_src" -eq 1 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "csix4" | awk '{print $4}')
	elif [ "$csi0_clk_src" -eq 2 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "video0x4" | awk '{print $4}')
	elif [ "$csi0_clk_src" -eq 3 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "periph0x2" | awk '{print $4}')
	else
	  echo "fail"
	fi
	csi0_mclk=$(expr "$src_clk" / "$csi0_clk_n" / "$csi0_clk_m")

#csi1
# 抓取数据并保存到变量中
  echo 0x02001c0c   > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据
  csi1_clk_enable=$(( ($data >> 31) & 1 ))
  csi1_clk_src=$(( ($data >> 24) & 7 )) #时钟源
  csi1_clk_n=$(( ($data >> 8) & 2 )) #分配N
  csi1_clk_n=$((1 << $csi1_clk_n))
  #echo "csi1_clk_n:$csi1_clk_n"
  csi1_clk_m=$(( $data  & 8 )) #分配M
  csi1_clk_m=$((csi1_clk_m + 1))

  #找到主时钟
	 # 使用条件语句检查csi1_clk_src的值
	if [ "$csi1_clk_src" -eq 0 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "hosc " | awk '{print $4}')
	elif [ "$csi1_clk_src" -eq 1 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "csix4" | awk '{print $4}')
	elif [ "$csi1_clk_src" -eq 2 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "video0x4" | awk '{print $4}')
	elif [ "$csi1_clk_src" -eq 3 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "periph0x2" | awk '{print $4}')
	else
	  echo "fail"
	fi
	csi1_mclk=$(expr "$src_clk" / "$csi1_clk_n" / "$csi1_clk_m")
#csi2
# 抓取数据并保存到变量中
  echo 0x02001c10   > /sys/class/sunxi_dump/dump
  data=$(cat /sys/class/sunxi_dump/dump)
 # 提取指定位的数据
  csi2_clk_enable=$(( ($data >> 31) & 1 ))
  csi2_clk_src=$(( ($data >> 24) & 7 )) #时钟源
  csi2_clk_n=$(( ($data >> 8) & 2 )) #分配N
  csi2_clk_n=$((1 << $csi2_clk_n))
  #echo "csi2_clk_n:$csi2_clk_n"
  csi2_clk_m=$(( $data  & 8 )) #分配M
  csi2_clk_m=$((csi2_clk_m + 1))

  #找到主时钟
	 # 使用条件语句检查csi2_clk_src的值
	if [ "$csi2_clk_src" -eq 0 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "hosc " | awk '{print $4}')
	elif [ "$csi2_clk_src" -eq 1 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "csix4" | awk '{print $4}')
	elif [ "$csi2_clk_src" -eq 2 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "video0x4" | awk '{print $4}')
	elif [ "$csi2_clk_src" -eq 3 ]; then
	  src_clk=$(cat /sys/kernel/debug/clk/clk_summary  | grep "periph0x2" | awk '{print $4}')
	else
	  echo "fail"
	fi
	csi2_mclk=$(expr "$src_clk" / "$csi2_clk_n" / "$csi2_clk_m")

	if [ "$csi0_clk_enable" -eq 1 ]; then
		echo "csi0_mclk : NO"
		echo "csi0_mclk_fre: $csi0_mclk"
	else
		echo "csi0_mclk : OFF"
	fi
	if [ "$csi1_clk_enable" -eq 1 ]; then
		echo "csi1_mclk : NO"
		echo "csi1_mclk_fre: $csi0_mclk"
	else
		echo "csi1_mclk : OFF"
	fi
	if [ "$csi2_clk_enable" -eq 1 ]; then
		echo "csi2_mclk : NO"
		echo "csi2_mclk_fre: $csi0_mclk"
	else
		echo "csi2_mclk : OFF"
	fi
}

#这个需要配置脚本
check_sensor_i2c()
{
	echo "=======check sensor i2c test========="

	sensor0="gc2083_mipi"
	sensor1="gc2083_mipi_2"
	sensor2="gc2083_dvp"
	#这里需要配置sensor id 的地址和值，用于测试i2c是否通， 一下信息在sensor驱动都能找到。
	sensor_id_addr0="03f1" #如果是16位地址，则是xxxx， 如8位地址则00xx
	sensor_id_val0="0x83" #如果高位有0，则省了掉
	sensor_id_addr1="03f1" #如果是16位地址，则是xxxx， 如8位地址则00xx
	sensor_id_val1="0x83" #如果高位有0，则省了掉
	sensor_id_addr2="03f1" #如果是16位地址，则是xxxx， 如8位地址则00xx
	sensor_id_val2="0x83" #如果高位有0，则省了掉

	sensor_i2c_node="/sys/devices/"
	cur_path=$(pwd)

	#=======检测sensor0, 不支持需要切页的sensor
	cd "$sensor_i2c_node/$sensor0"
	echo "1" > read_flag
    echo "$sensor_id_addr0"0001 > cci_client
    value=$(cat read_value)
	#echo "$sensor_id_addr0:$value"  #调试
	if [ "$value" = "$sensor_id_val0" ]; then
		echo "sensor0 i2c test :ok"
	else
		echo "sensor0 i2c test :fail"
	fi

	#=======检测sensor1,, 不支持需要切页的sensor
	cd "$sensor_i2c_node/$sensor1"
	echo "1" > read_flag
    echo "$sensor_id_addr1"0001 > cci_client
    value=$(cat read_value)
	#echo "$sensor_id_addr1:$value" #调试
	if [ "$value" = "$sensor_id_val1" ]; then
		echo "sensor1 i2c test :ok"
	else
		echo "sensor1 i2c test :fail"
	fi
	#=======检测sensor2, 不支持需要切页的sensor
	cd "$sensor_i2c_node/$sensor2"
	echo "1" > read_flag
    echo "$sensor_id_addr0"0001 > cci_client
    value=$(cat read_value)
	#echo "$sensor_id_addr0:$value"  #调试
	if [ "$value" = "$sensor_id_val0" ]; then
		echo "sensor2 i2c test :ok"
	else
		echo "sensor2 i2c test :fail"
	fi
	cd $cur_path
echo "==========END============"
}

check_video_node
check_sensor_mipi_clk
check_soc_mipi_d0_trnds
check_soc_mipi_int_erro
check_dvp_polarity
check_dvp_parser_pclk
check_sensor_w_h_data
check_sensor_mclk
check_vi_frame_cnt
#check_ve_frame_cnt  #这个耗时比较长
check_sensor_i2c_sattus
#check_sensor_i2c & ##这个需要进函数配置脚本，把每个sensor 的id 弄清楚,如果sensor mclk 没打开，则这个脚本会卡死。所以放后台执行

echo "==========END============" #放在check_sensor_i2c 执行
