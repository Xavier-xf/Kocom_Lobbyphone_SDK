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
	wmg_printf(MSG_INFO,"NAME:\n\twifi_set_parse_flags\n");
	wmg_printf(MSG_INFO,"DESCRIPTION:\n\tset xr parse flags.\n");
	wmg_printf(MSG_INFO,"USAGE:\n\twifi_set_parse_flags <rx_flags> <tx_flags>\n");
	wmg_printf(MSG_INFO,"--------------------------------------MORE---------------------------------------\n");
	wmg_printf(MSG_INFO,"The way to get help information:\n");
	wmg_printf(MSG_INFO,"\twifi_connect_ap_test --help\n");
	wmg_printf(MSG_INFO,"\twifi_connect_ap_test -h\n");
	wmg_printf(MSG_INFO,"\twifi_connect_ap_test -H\n");
	wmg_printf(MSG_INFO,"---------------------------------------------------------------------------------\n");
}

int main(int argv, char *argc[]){
    int event_label = 0;;
    int rx_flags = 0, tx_flags = 0;
    const aw_wifi_interface_t *p_wifi_interface = NULL;

	if(argv == 2 && (!strcmp(argc[1],"--help") || !strcmp(argc[1], "-h") || !strcmp(argc[1], "-H"))){
		print_help();
		return -1;
	}

	if(argv != 3) {
		print_help();
		return -1;
	}

	wmg_printf(MSG_INFO,"\n*********************************\n");
	wmg_printf(MSG_INFO,"***Start wifi connect ap test\n");
	wmg_printf(MSG_INFO,"*********************************\n");

    event_label = rand();
    p_wifi_interface = aw_wifi_on(wifi_state_handle, event_label);
    if(p_wifi_interface == NULL){
        wmg_printf(MSG_ERROR,"wifi on failed\n");
        return -1;
    }

	tx_flags = atoi(argc[1]);
	rx_flags = atoi(argc[2]);

	p_wifi_interface->set_parse_flags((uint32_t)tx_flags, (uint32_t)rx_flags);

    return 0;
}
