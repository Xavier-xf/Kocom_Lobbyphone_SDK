#!/bin/bash

pwd=$1

if [ "$pwd" == "" ];then
    pwd=$PWD
fi

# generate boot script use for launch app
cat > ${pwd}/S01app << EOF
#!/bin/sh

MODULES_DIR="/lib/modules/\`uname -r\`"

mkdir -p /tmp/event/

echo 50 > /proc/sys/vm/dirty_writeback_centisecs
echo 50 > /proc/sys/vm/dirty_expire_centisecs
echo 5 > /proc/sys/vm/dirty_background_ratio
echo 10 > /proc/sys/vm/dirty_ratio
echo 1280 > /proc/sys/vm/extra_free_kbytes
echo 4096 > /proc/sys/vm/min_free_kbytes
echo 3 > /proc/sys/vm/drop_caches

echo 1 > /proc/sys/vm/overcommit_memory

export FB_SYNC='1'
# default value, will be overwrited by application
export TERM='xterm'
export LD_HWCAP_MASK='0x00000000'
export TSLIB_TSEVENTTYPE=H3600
export TSLIB_CONSOLEDEVICE=none
export TSLIB_FBDEVICE=/dev/fb0
export TSLIB_TSDEVICE=/dev/input/event0
export TSLIB_CALIBFILE=/etc/pointercal
export TSLIB_CONFFILE=/etc/ts.conf
export TSLIB_PLUGINDIR=/usr/lib/ts

if [ ! -e "/tmp/data" ];then
    if [ -d "/data" ];then
        ln -s /data /tmp/data
    fi
fi

start() {
    ulimit -c unlimited

    #if [ -z "\$(grep '\<vfat\>' /proc/mounts)" ]; then
    #    ! /bin/mount -t vfat /dev/mmcblk0 /mnt/extsd 2>/dev/null &&
    #    /bin/mount -t vfat /dev/mmcblk0p1 /mnt/extsd
    #fi

    if [ ! -z "\$(grep '\<vfat\>' /proc/mounts)" ]; then
        /usr/bin/gettimems "start app"
        if [ -e "/overlay/.firstboot" ];then
            /usr/bin/sdvcam &
            rm /overlay/.firstboot
            rm /mnt/extsd/isp*.bin
        else
                /usr/bin/sdvcam &
        fi
    else
        /usr/bin/sdvcam &
    fi

#    /usr/bin/webapp &
    /usr/bin/log_guardian &

 #   hwclock -s
 #   if [ "\$(date +%Y)" -lt "2018" ];then
 #      date 2018-07-01
 #       hwclock -w
 #   fi
}

stop() {
    hwclock -w

    pid=\$(ps | awk -v partten="sdvcam" '{if (\$5~partten) print \$1}')
    kill -9 \$pid

    sleep 1
}

case "\$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart|reload)
        stop
        start
        ;;
    *)
        echo "Usage: \$0 {start|stop|restart}"
        exit 1
esac

exit \$?

EOF
chmod 755 ${pwd}/S01app

# generate network config script
#cat > $TARGET_OUT/target/etc/init.d/auto_config_network << EOF


#MAC_ADDR="\`cat /proc/cpuinfo | awk '\$1=="Serial" {print \$3}' | sed 's/../&:/g' | cut -c16-29\`"

#ifconfig eth0 hw ether "00:\$MAC_ADDR"
#ifconfig lo 127.0.0.1
#udhcpc -b -R &

#EOF
#chmod 755 $TARGET_OUT/target/etc/init.d/auto_config_network

# update /etc/shadow to generate root password
password="admin"
shadow=$(echo $password | $pwd/mkpasswd -s)
#sed -i "s%^root::%root:$shadow:%" $TARGET_OUT/target/etc/shadow
