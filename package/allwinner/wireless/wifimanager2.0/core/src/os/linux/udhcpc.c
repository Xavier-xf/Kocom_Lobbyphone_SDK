#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <linux/event.h>
#include <linux/linux_sta.h>
#include <linux/linux_common.h>
#include <wifi_log.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <udhcpc.h>

#define IP_SIZE     16

static int get_ipv4(const char *inf, char *ip)
{
	int sd;
	struct sockaddr_in sin;
	struct ifreq ifr;

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(-1 == sd){
		WMG_ERROR("socket error: %s\n", strerror(errno));
		return -1;
	}

	strncpy(ifr.ifr_name, inf, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = 0;

	// if error: No such device
	if(ioctl(sd, SIOCGIFADDR, &ifr) < 0) {
		WMG_ERROR("ioctl error: %s\n", strerror(errno));
		close(sd);
		return -1;
	}

	memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
	snprintf(ip, IP_SIZE, "%s", inet_ntoa(sin.sin_addr));

	close(sd);
	return 0;
}

#define IPV6_LEN 128
static int get_ipv6(const char *inf, char *ipv6, int ip_no)
{
	FILE *f;
	int scope, prefix, i = 0;
	unsigned char _ipv6[16] = {0};
	char inf_name[IFNAMSIZ] = {0};
	char dname[IFNAMSIZ] = {0};
	char address[INET6_ADDRSTRLEN];

	f = fopen("/proc/net/if_inet6", "r");
	if (f == NULL) {
		return -1;
	}

	while((19 == fscanf(f,
		" %2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx\
		%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx \
		%*x %2hhx %2hhx %*x %6s",
		&_ipv6[0], &_ipv6[1], &_ipv6[2], &_ipv6[3],
		&_ipv6[4], &_ipv6[5], &_ipv6[6], &_ipv6[7],
		&_ipv6[8], &_ipv6[9], &_ipv6[10], &_ipv6[11],
		&_ipv6[12], &_ipv6[13], &_ipv6[14], &_ipv6[15],
		&prefix, &scope, inf_name)) && (i < ip_no)) {
		if(!strcmp(inf_name, inf)) {
			if(inet_ntop(AF_INET6, _ipv6, (ipv6 + (i * IPV6_LEN)), IPV6_LEN) != NULL) {
				WMG_DEBUG("%s(%d): %s\n", inf_name, i, (ipv6 + (i * IPV6_LEN)));
				i++;
			} else {
				continue;
			}
		}
	}
	fclose(f);

	return i;
}

/*
 * @check_ip: 01: check if ipv4 exists
 *            10: check if ipv6 exists
 *            11: check if ipv4 and ipv6 are exists
 * @return: 01: check that ipv4 exists
 *          10: check that ipv6 exists
 *          11: check that ipv4 and ipv6 are exists
 */
uint8_t is_ip_exist(char *inf, uint8_t check_ip)
{
	uint8_t vflag = 0;
	char ipaddr[INET6_ADDRSTRLEN];
	char ipv6addr[512] = {0};
	int ipv6_ret = 0, i = 0;

	if(check_ip & IPV4_ADDR_BM){
		if(!get_ipv4(inf, ipaddr)) {
			WMG_DEBUG("%s: get ipv4: %s\n", inf, ipaddr);
			vflag |= IPV4_ADDR_BM;
		} else {
			WMG_DEBUG("%s: can't get ipv4\n", inf);
		}
	}

	if(check_ip & IPV6_ADDR_BM){
		ipv6_ret = get_ipv6(inf, ipv6addr, 4);
		if(ipv6_ret > 0) {
			for(; i < ipv6_ret; i++) {
				if(strstr((ipv6addr + i * IPV6_LEN), "fe80")) {
					WMG_DEBUG("%s: get local link ipv6: %s\n", inf, ipv6addr + i * IPV6_LEN);
				} else {
					WMG_DEBUG("%s: get global ipv6: %s\n", inf, ipv6addr + i * IPV6_LEN);
					vflag |= IPV6_ADDR_BM;
				}
			}
		} else {
			WMG_DEBUG("%s: can't get ipv6\n", inf);
		}
	}

	return vflag;
}

wmg_status_t start_udhcpc(char *inf, uint8_t start_ip_type)
{
	int times = 0;
	char ipaddr[INET6_ADDRSTRLEN];
	char cmd[256] = {0};

	if(inf == NULL) {
		WMG_ERROR("start udhcpc inf is NULL");
		return WMG_STATUS_INVALID;
	}

	/* start udhcpc */
	if((start_ip_type == 0) || (start_ip_type & IPV4_ADDR_BM)){
		sprintf(cmd, "udhcpc -i %s -S -t 5 -T 7 -b -q", inf);
		wmg_system(cmd);
	}

	/* start udhcpc6 */
	if((start_ip_type == 0) || (start_ip_type & IPV6_ADDR_BM)){
		sprintf(cmd, "udhcpc6 -i %s -S -t 5 -T 7 -b -q -s /etc/wifi/wmg_udhcpc6.script", inf);
		wmg_system(cmd);
	}

	/* check ip exist */
	if(start_ip_type == 0) {
		while((is_ip_exist(inf, IPV4_ADDR_BM || IPV6_ADDR_BM ) != 0) && (times < 50)) {
			usleep(200000);
			times++;
		}
	} else {
		while((is_ip_exist(inf, start_ip_type) != start_ip_type) && (times < 50)) {
			usleep(200000);
			times++;
		}
	}

	if(times >= 50) {
		WMG_ERROR("udhcpc %s timeout\n",inf);
		return WMG_STATUS_FAIL;
	}
	return WMG_STATUS_SUCCESS;
}

void stop_udhcpc(char *inf, uint8_t stop_ip_type)
{
	char cmd[256] = {0};
	if((stop_ip_type == 0) || (stop_ip_type & IPV4_ADDR_BM)){
		if(stop_linux_process("udhcpc")) {
			WMG_WARNG("stop udhcpc fail\n");
		}
		sprintf(cmd, "ifconfig %s 0.0.0.0", inf);
		wmg_system(cmd);
	}
	if((stop_ip_type == 0) || (stop_ip_type & IPV6_ADDR_BM)){
		if(stop_linux_process("udhcpc6")) {
			WMG_WARNG("stop udhcpc6 fail\n");
		}
	}
}
