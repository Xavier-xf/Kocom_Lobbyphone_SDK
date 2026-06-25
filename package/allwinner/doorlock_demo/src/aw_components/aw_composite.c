#include "aw_composite.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "doorlock_common.h"
#include "aw_uacdev.h"
#include "aw_uvcout.h"

composite_dev_t *g_composite_dev_info = NULL;

static void *composite_dev_thread(void *arg)
{
    DOORLOCK_DBG("enter ===>\n");
    composite_dev_t *composite_dev_info = (composite_dev_t *)arg;
    int exit_flag = false;

    while (1) {
        if (composite_dev_info->force_exit) {
            exit_flag = true;
        }

        usleep(10 * 1000);
    }
    DOORLOCK_DBG("exit <===\n");
}

int composite_dev_init(composite_dev_t *composite_dev_info)
{
    // do not malloc in the callback function, it's called from usb irq !!!
    composite_dev_info->force_exit = false;
    pthread_mutex_init(&composite_dev_info->composite_dev_lock, NULL);

    // composite_dev_info->composite_dev_fd = open("/dev/composite_dev", O_RDWR);
    // ioctl(composite_dev_info->composite_dev_fd, COMPOSITE_EVENT_USER_CONFIG,
    // &composite_dev_info->mUserConfig);
    g_composite_dev_info = composite_dev_info;

    // pthread_create(&composite_dev_info->composite_dev_thread, NULL, composite_dev_thread, (void
    // *)composite_dev_info);
}

int composite_dev_deinit(composite_dev_t *composite_dev_info)
{
    composite_dev_info->force_exit = true;
    // if(composite_dev_info->composite_dev_thread){
    //	pthread_join(composite_dev_info->composite_dev_thread, NULL);
    // }
#if 0
	if(composite_dev_info->uvc){
		uvc_out_t *uvc_out_info = composite_dev_info->uvc_out_info;
		if(uvc_out_info){
			uvc_out_info->force_exit = true;
		}
	}
	if(composite_dev_info->uac){
		uac_dev_t *uac_dev_info = composite_dev_info->uac_dev_info;
		if(uac_dev_info){
			uac_dev_info->force_exit = true;
		}
	}
#endif

    // composite_dev_info->composite_dev_fd = NULL;
    pthread_mutex_destroy(&composite_dev_info->composite_dev_lock);
    g_composite_dev_info = NULL;
    return 0;
}

int composite_dev_connect(composite_dev_t *composite_dev_info)
{
    if (!composite_dev_info) {
        DOORLOCK_ERR("composite_dev_info NULL\n");
        return -1;
    }

    uvc_out_t *uvc_out_info = composite_dev_info->uvc_out_info;
    if (composite_dev_info->uvc) {
        if (!uvc_out_info) {
            DOORLOCK_WARN("uvc_out_info NULL\n");
        } else {
            // DOORLOCK_WARN("UVC bulk_mode = %d\n", uvc_out_info->bulk_mode);
        }
    }
    // ioctl(composite_dev_info->composite_dev_fd, COMPOSITE_EVENT_CONNECT, NULL);
    DOORLOCK_DBG("enter ===>\n");
    char shell_cmd_buf[1024] = {0};
    memset(shell_cmd_buf, 0, sizeof(shell_cmd_buf));

    if (composite_dev_info->uvc && composite_dev_info->uac) {
        if (composite_dev_info->usb_vid || composite_dev_info->usb_pid) {
            if (uvc_out_info && uvc_out_info->bulk_mode) {
                sprintf(shell_cmd_buf, "/bin/setusbconfig uvc,uac1 0x%04x 0x%04x bulk",
                        composite_dev_info->usb_vid, composite_dev_info->usb_pid);
            } else {
                sprintf(shell_cmd_buf, "/bin/setusbconfig uvc,uac1 0x%04x 0x%04x",
                        composite_dev_info->usb_vid, composite_dev_info->usb_pid);
            }
        } else {
            if (uvc_out_info && uvc_out_info->bulk_mode) {
                sprintf(shell_cmd_buf, "/bin/setusbconfig uvc,uac1 bulk");
            } else {
                sprintf(shell_cmd_buf, "/bin/setusbconfig uvc,uac1");
            }
        }
        system(shell_cmd_buf);
    } else if (!composite_dev_info->uvc && composite_dev_info->uac) {
        if (composite_dev_info->usb_vid || composite_dev_info->usb_pid) {
            sprintf(shell_cmd_buf, "/bin/setusbconfig uac1 0x%04x 0x%04x",
                    composite_dev_info->usb_vid, composite_dev_info->usb_pid);
        } else {
            sprintf(shell_cmd_buf, "/bin/setusbconfig uac1");
        }
        system(shell_cmd_buf);
    } else if (composite_dev_info->uvc && !composite_dev_info->uac) {
        if (composite_dev_info->usb_vid || composite_dev_info->usb_pid) {
            if (uvc_out_info && uvc_out_info->bulk_mode) {
                sprintf(shell_cmd_buf, "/bin/setusbconfig uvc 0x%04x 0x%04x bulk",
                        composite_dev_info->usb_vid, composite_dev_info->usb_pid);
            } else {
                sprintf(shell_cmd_buf, "/bin/setusbconfig uvc 0x%04x 0x%04x",
                        composite_dev_info->usb_vid, composite_dev_info->usb_pid);
            }
        } else {
            if (uvc_out_info && uvc_out_info->bulk_mode) {
                sprintf(shell_cmd_buf, "/bin/setusbconfig uvc bulk");
            } else {
                sprintf(shell_cmd_buf, "/bin/setusbconfig uvc");
            }
        }
        system(shell_cmd_buf);
    } else {
        // system("/bin/setusbconfig none");
        DOORLOCK_INFO("why do nothing ?? !!\n");
    }

    // system("cat /sys/devices/platform/soc/usbc0/usb_device");
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int composite_dev_disconnect(composite_dev_t *composite_dev_info)
{
    // ioctl(composite_dev_info->composite_dev_fd, COMPOSITE_EVENT_DISCONNECT, NULL);
    DOORLOCK_DBG("enter ===>\n");
    system("/bin/setusbconfig none");
    DOORLOCK_DBG("exit <===\n");
}
