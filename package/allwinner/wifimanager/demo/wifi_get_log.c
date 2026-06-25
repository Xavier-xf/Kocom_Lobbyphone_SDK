#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <wifi_intf.h>
#include <pthread.h>
#include "wmg_debug.h"
#include "wifi_udhcpc.h"

static void wifi_state_handle(struct Manager *w, int event_label)
{
    wmg_printf(MSG_DEBUG,"event_label 0x%x\n", event_label);

    switch(w->StaEvt.state)
    {
		 case CONNECTING:
		 {
			 wmg_printf(MSG_INFO,"Connecting to the network(%s)......\n",w->ssid);
			 break;
		 }
		 case CONNECTED:
		 {
			 wmg_printf(MSG_INFO,"Connected to the AP(%s)\n",w->ssid);
			 start_udhcpc();
			 break;
		 }

		 case OBTAINING_IP:
		 {
			 wmg_printf(MSG_INFO,"Getting ip address(%s)......\n",w->ssid);
			 break;
		 }

		 case NETWORK_CONNECTED:
		 {
			 wmg_printf(MSG_DEBUG,"Successful network connection(%s)\n",w->ssid);
			 break;
		 }
		case DISCONNECTED:
		{
		    wmg_printf(MSG_ERROR,"Disconnected,the reason:%s\n",wmg_event_txt(w->StaEvt.event));
		    break;
		}
    }
}
void print_help(){
	wmg_printf(MSG_INFO,"---------------------------------------------------------------------------------\n");
	wmg_printf(MSG_INFO,"NAME:\n\twifi_get_log\n");
	wmg_printf(MSG_INFO,"DESCRIPTION:\n\tget log file.\n");
	wmg_printf(MSG_INFO,"USAGE:\n\twifi_get_log <last/lock>\n");
	wmg_printf(MSG_INFO,"PARAMS:\n\tlast   : get last file\n");
	wmg_printf(MSG_INFO,"\tlock : get lock file\n");
	wmg_printf(MSG_INFO,"--------------------------------------MORE---------------------------------------\n");
	wmg_printf(MSG_INFO,"The way to get help information:\n");
	wmg_printf(MSG_INFO,"\twifi_connect_ap_test --help\n");
	wmg_printf(MSG_INFO,"\twifi_connect_ap_test -h\n");
	wmg_printf(MSG_INFO,"\twifi_connect_ap_test -H\n");
	wmg_printf(MSG_INFO,"---------------------------------------------------------------------------------\n");
}


int printfile(char* filename)
{
	FILE *fp = NULL;
	char str[1024];
	fp = fopen(filename,"r");
	if(fp == NULL) {
		wmg_printf(MSG_ERROR,"open file(%s) fail\n", filename);
		return -1;
	}
	while(fgets(str, 1024, fp) != NULL) {
		printf("%s",str);
	}
	fclose(fp);
	return 0;
}

#define LASTLOG_FLAG 0
#define LOCKLOG_FLAG 1
int main(int argv, char *argc[]){
    int ret = 0, len = 0, file_flag = 0;
    int event_label = 0;;
    char log_file_path[256] = {0};
    const aw_wifi_interface_t *p_wifi_interface = NULL;
    int file_num = 0;

	if(argv == 2 && (!strcmp(argc[1],"--help") || !strcmp(argc[1], "-h") || !strcmp(argc[1], "-H"))){
		print_help();
		return -1;
	}

	if(argv != 2){
		print_help();
		return -1;
	}

	wmg_printf(MSG_INFO,"\n*********************************\n");
	wmg_printf(MSG_INFO,"***Start wifi get log\n");
	wmg_printf(MSG_INFO,"*********************************\n");

    event_label = rand();
    p_wifi_interface = aw_wifi_on(wifi_state_handle, event_label);
    if(p_wifi_interface == NULL){
        printf("wifi on failed\n");
        return -1;
    }

	if(!strcmp(argc[1],"last")){
		file_flag = LASTLOG_FLAG;
	} else if(!strcmp(argc[1],"lock")){
		file_flag = LOCKLOG_FLAG;
	} else {
		wmg_printf(MSG_ERROR,"unsupport log file type\n");
		return -1;
	}

	file_num = p_wifi_interface->get_log_file(file_flag, log_file_path);
	if(file_flag == LASTLOG_FLAG) {
		if(file_num == 2) {
			wmg_printf(MSG_INFO,"get last log file: %s %s.old\n", log_file_path, log_file_path);
			printfile(log_file_path);
		} else if(file_num == 1) {
			wmg_printf(MSG_INFO,"get last log file: %s\n", log_file_path);
			printfile(log_file_path);
		} else {
			wmg_printf(MSG_INFO,"Have not last log file\n");
		}
	} else {
		if(file_num != 0) {
			wmg_printf(MSG_INFO,"get lock log file: %s\n", log_file_path);
			printfile(log_file_path);
		} else {
			wmg_printf(MSG_INFO,"Have not lock log file\n");
		}
	}
	return 0;
}
