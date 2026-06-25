#include <stdio.h>
#include <hal_timer.h>
#include <openamp/sunxi_helper/openamp.h>

extern int csi_init(int argc, const char **argv);
extern int msh_exec(char *cmd, int length);

int app_entry(void *param)
{
#ifdef CONFIG_DRIVERS_VIN
	int ret;

	ret = csi_init(0, NULL);
	if (ret) {
		rpmsg_notify("rt-media", NULL, 0);
		printf("csi init fail!\n");
	}
#ifdef CONFIG_RPMSG_NOTIFY
	rpmsg_notify("twi1", NULL, 0);
	rpmsg_notify("tdm0", NULL, 0);
	rpmsg_notify("isp0", NULL, 0);
	rpmsg_notify("scaler0", NULL, 0);
	rpmsg_notify("scaler4", NULL, 0);
	rpmsg_notify("scaler8", NULL, 0);
	rpmsg_notify("scaler12", NULL, 0);
	rpmsg_notify("vinc0", NULL, 0);
	rpmsg_notify("vinc4", NULL, 0);
	rpmsg_notify("vinc8", NULL, 0);
	rpmsg_notify("vinc12", NULL, 0);
#endif
#else
#ifdef CONFIG_RPMSG_NOTIFY
	hal_msleep(200);
	rpmsg_notify("rt-media", NULL, 0);
#endif
#endif
	//msh_exec("dmesg", strlen("dmesg"));
    return 0;
}
