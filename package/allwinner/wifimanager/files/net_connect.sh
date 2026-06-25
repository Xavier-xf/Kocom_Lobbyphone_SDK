#!/bin/sh

wireless_gateway=0.0.0.0
net_name=wlan0

get_gw() {
	running_num=`ifconfig ${net_name}|grep RUNNING|wc -l`
	if [ ${running_num} -ge 1 ]; then
		wireless_gateway=`ip route show|grep ${net_name}|grep default|awk '{print $3}'`
		return 0
	else
		wireless_gateway=0.0.0.0
		return 1
	fi
}

ping_check(){
	get_gw
	if [ $? -eq 0 ]; then
		ping -c 3 "www.baidu.com"
		if [ $? -eq 0 ]; then
			echo -e "------------ping baidu sucess-----------"
			return 0;
		else
			ping -c 5 $wireless_gateway > /dev/null
			if [ $? -eq 0 ]; then
				echo -e "------------ping gateway $wireless_gateway sucess-----------"
				return 0;
			else
				echo -e "------------ping gateway $wireless_gateway false-----------"
				return 1;
			fi
		fi
	else
		echo -e "--------------$net_name is not running-----------"
		return 1;
	fi
}

while true
do
	ping_check
	if [ $? -eq 0 ]; then
		echo -e "------------net have connect-----------"
		logger -t net_connect.sh " ------------net have connect----------- "
	else
		echo -e "----------net have not connect---------"
		logger -t net_connect.sh " --net have not connect, lock file now-- "
		wifi_lock_file 256
		exit
	fi
	sleep 60
done
