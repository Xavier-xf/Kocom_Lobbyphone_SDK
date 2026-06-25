/*
 * lv_box_res.c
 *
 *  Created on: 2022Äê9ÔÂ9ÈÕ
 *      Author: anruliu
 */

#include "lv_box_res.h"
#include "lv_box_res_wifi.h"
#include "lv_box_res_player_int.h"
#include "lv_box_res_display.h"
#include "lv_box_res_bt.h"

static void lv_box_res_ft_init(void) {
    /*FreeType uses C standard file system, so no driver letter is required.*/
    lv_box_res.ft_info_100.name =
            "/usr/share/lv_86_boxes/AlibabaSans-Regular.otf";
    lv_box_res.ft_info_100.weight = 100;
    lv_box_res.ft_info_100.style = FT_FONT_STYLE_NORMAL;
    if (!lv_ft_font_init(&lv_box_res.ft_info_100)) {
        LV_LOG_ERROR("create failed.");
    }

    lv_box_res.ft_info_50.name =
            "/usr/share/lv_86_boxes/AlibabaSans-Regular.otf";
    lv_box_res.ft_info_50.weight = 50;
    lv_box_res.ft_info_50.style = FT_FONT_STYLE_NORMAL;
    if (!lv_ft_font_init(&lv_box_res.ft_info_50)) {
        LV_LOG_ERROR("create failed.");
    }

    lv_box_res.ft_info_40.name =
            "/usr/share/lv_86_boxes/AlibabaSans-Regular.otf";
    lv_box_res.ft_info_40.weight = 40;
    lv_box_res.ft_info_40.style = FT_FONT_STYLE_NORMAL;
    if (!lv_ft_font_init(&lv_box_res.ft_info_40)) {
        LV_LOG_ERROR("create failed.");
    }

    lv_box_res.ft_info_30.name =
            "/usr/share/lv_86_boxes/AlibabaSans-Regular.otf";
    lv_box_res.ft_info_30.weight = 30;
    lv_box_res.ft_info_30.style = FT_FONT_STYLE_NORMAL;
    if (!lv_ft_font_init(&lv_box_res.ft_info_30)) {
        LV_LOG_ERROR("create failed.");
    }

    lv_box_res.ft_info_26.name =
            "/usr/share/lv_86_boxes/AlibabaSans-Regular.otf";
    lv_box_res.ft_info_26.weight = 26;
    lv_box_res.ft_info_26.style = FT_FONT_STYLE_NORMAL;
    if (!lv_ft_font_init(&lv_box_res.ft_info_26)) {
        LV_LOG_ERROR("create failed.");
    }

    lv_box_res.ft_info_10.name =
            "/usr/share/lv_86_boxes/AlibabaSans-Regular.otf";
    lv_box_res.ft_info_10.weight = 16;
    lv_box_res.ft_info_10.style = FT_FONT_STYLE_NORMAL;
    if (!lv_ft_font_init(&lv_box_res.ft_info_10)) {
        LV_LOG_ERROR("create failed.");
    }
}

static void lv_box_res_style_init(void) {
    lv_style_init(&lv_box_res.style_ft_100);
    lv_style_set_text_font(&lv_box_res.style_ft_100,
            lv_box_res.ft_info_100.font);

    lv_style_init(&lv_box_res.style_ft_40);
    lv_style_set_text_font(&lv_box_res.style_ft_40, lv_box_res.ft_info_40.font);

    lv_style_init(&lv_box_res.style_ft_30);
    lv_style_set_text_font(&lv_box_res.style_ft_30, lv_box_res.ft_info_30.font);

    lv_style_init(&lv_box_res.style_ft_26);
    lv_style_set_text_font(&lv_box_res.style_ft_26, lv_box_res.ft_info_26.font);

    lv_style_init(&lv_box_res.style_ft_10);
    lv_style_set_text_font(&lv_box_res.style_ft_10, lv_box_res.ft_info_10.font);

    lv_style_init(&lv_box_res.lamp_bg_style);
    lv_style_set_radius(&lv_box_res.lamp_bg_style, 10);
    lv_style_set_border_width(&lv_box_res.lamp_bg_style, 0);
    lv_style_set_pad_all(&lv_box_res.lamp_bg_style, 0);
    lv_style_set_shadow_width(&lv_box_res.lamp_bg_style, 0);
    lv_style_set_bg_opa(&lv_box_res.lamp_bg_style, LV_OPA_10);
    lv_style_set_bg_color(&lv_box_res.lamp_bg_style,
            lv_palette_main(LV_PALETTE_GREY));

    lv_style_init(&lv_box_res.tabview_bg_style);
    lv_style_set_bg_opa(&lv_box_res.tabview_bg_style, LV_OPA_0);

    lv_style_init(&lv_box_res.set_bg_style);
    lv_style_set_radius(&lv_box_res.set_bg_style, 0);
    lv_style_set_border_width(&lv_box_res.set_bg_style, 0);
    lv_style_set_pad_all(&lv_box_res.set_bg_style, 0);
    lv_style_set_shadow_width(&lv_box_res.set_bg_style, 0);
    lv_style_set_bg_opa(&lv_box_res.set_bg_style, LV_OPA_10);
    lv_style_set_bg_color(&lv_box_res.set_bg_style,
            lv_palette_main(LV_PALETTE_GREY));
}

void lv_box_res_init(void) {
    pthread_mutex_init(&lvgl_mutex, NULL);

    lv_box_res_ft_init();
    lv_box_res_style_init();
    lv_box_res_wifi_init();
    lv_box_res_bt_init();
    lv_box_res_music_init();
    lv_box_res_music_list_init();
    lv_box_res_display_init();
}
