#include "lvgl/lvgl.h"
#include "lv_drivers/display/sunxifb.h"
#include "lv_drivers/indev/evdev.h"
#include "lv_box_headbar.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common/lv_box_res.h"
#include "activity/lv_box_home_activity.h"
#include "activity/lv_box_lamp_activity.h"
#include "activity/lv_box_news_activity.h"
#include "activity/lv_box_music_activity.h"
#include "activity/lv_box_alarm_activity.h"
#include "activity/lv_box_setting_activity.h"

int main(int argc, char *argv[]) {
    lv_disp_drv_t disp_drv;
    lv_disp_draw_buf_t disp_buf;
    lv_indev_drv_t indev_drv;
    uint32_t rotated = LV_DISP_ROT_NONE;

    lv_disp_drv_init(&disp_drv);

    if (argc >= 2 && atoi(argv[1]) >= 0 && atoi(argv[1]) <= 4) {
        rotated = atoi(argv[1]);
#ifndef USE_SUNXIFB_G2D_ROTATE
        if (rotated != LV_DISP_ROT_NONE)
            disp_drv.sw_rotate = 1;
#endif
    }

    /*LittlevGL init*/
    lv_init();

    /*Linux frame buffer device init*/
    sunxifb_init(rotated);

    /*A buffer for LittlevGL to draw the screen's content*/
    static uint32_t width, height;
    sunxifb_get_sizes(&width, &height);
    width = 480;
    height = 480;

    static lv_color_t *buf;
    buf = (lv_color_t*) sunxifb_alloc(width * height * sizeof(lv_color_t),
            "lv_g2d_test");

    if (buf == NULL) {
        sunxifb_exit();
        printf("malloc draw buffer fail\n");
        return 0;
    }

    /*Initialize a descriptor for the buffer*/
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, width * height);

    /*Initialize and register a display driver*/
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sunxifb_flush;
    disp_drv.hor_res = width;
    disp_drv.ver_res = height;
    disp_drv.rotated = rotated;
    disp_drv.screen_transp = 0;
    lv_disp_drv_register(&disp_drv);

    evdev_init();
    lv_indev_drv_init(&indev_drv); /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_POINTER; /*See below.*/
    indev_drv.read_cb = evdev_read; /*See below.*/
    /*Register the driver in LVGL and save the created input device object*/
    lv_indev_t *evdev_indev = lv_indev_drv_register(&indev_drv);

    lv_box_res_init();

    lv_box_home_init();

    lv_box_lamp_init();

    lv_box_news_init();

    lv_box_music_init();

    lv_box_alarm_init();

    lv_box_setting_init();

    lv_box_headbar_init(home_activity);

    /*Handle LitlevGL tasks (tickless mode)*/
    while (1) {
        pthread_mutex_lock(&lvgl_mutex);
        lv_task_handler();
        pthread_mutex_unlock(&lvgl_mutex);
        usleep(1000);
    }

    /*sunxifb_free((void**) &buf, "lv_g2d_test");*/
    /*sunxifb_exit();*/
    return 0;
}

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t custom_tick_get(void) {
    static uint64_t start_ms = 0;
    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = ((uint64_t) tv_start.tv_sec * 1000000
                + (uint64_t) tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = ((uint64_t) tv_now.tv_sec * 1000000 + (uint64_t) tv_now.tv_usec)
            / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
