/*
 * lv_box_res_disp.c
 *
 *  Created on: 2022Äê10ÔÂ11ÈÕ
 *      Author: anruliu
 */

#include "lv_box_res_display.h"

#ifdef LV_USE_LINUX_DISPALY

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>

static int disphd;

int lv_box_res_display_init(void) {
    disphd = open("/dev/disp", O_RDWR);
    if (disphd == -1) {
        printf("open display device faild ( %s )", strerror(errno));
        return -1;
    }

    return 0;
}

void lv_box_res_display_deinit(void) {
    close(disphd);
}

int lv_box_res_display_get_brightness(void) {
    unsigned long ioctlParam[4] = { 0 };
    return ioctl(disphd, 0x103, ioctlParam);
}

int lv_box_res_display_set_brightness(unsigned int brightness) {
    unsigned long ioctlParam[4] = { 0 };
    ioctlParam[1] = (unsigned long) brightness;
    return ioctl(disphd, 0x102, ioctlParam);
}
#else
int lv_box_res_display_init(void) {
    return 0;
}

void lv_box_res_display_deinit(void) {

}

int lv_box_res_display_get_brightness(void) {
    return 0;
}

int lv_box_res_display_set_brightness(unsigned int brightness) {
    return 0;
}
#endif
