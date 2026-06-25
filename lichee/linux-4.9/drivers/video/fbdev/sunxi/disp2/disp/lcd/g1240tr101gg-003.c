/*
*	drivers/video/sunxi/disp2/disp/lcd/g1240tr101gg-003.c
*
*	Copyright (c) 2017 Allwinnertech Co., Ltd.
*	Author: lizeyu <lizeyu@allwinnertech.com>
*
*	g1240tr101gg-003 panel driver
*
*	This program is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License version 2 as
*	published by the Free Software Foundation.
*/
#include "g1240tr101gg-003.h"

static void lcd_power_on(u32 sel);
static void lcd_power_off(u32 sel);
static void lcd_bl_open(u32 sel);
static void lcd_bl_close(u32 sel);

static void lcd_panel_init(u32 sel);
static void lcd_panel_exit(u32 sel);

#define panel_reset(sel, val) sunxi_lcd_gpio_set_value(sel, 0, val)
#define DBG_INFO(format, args...)                                              \
	(printk("[G1240TR101GG] LINE:%04d-->%s:" format, __LINE__, __func__,   \
		##args))
#define DBG_ERR(format, args...)                                               \
	(printk("[G1240TR101GG] LINE:%04d-->%s:" format, __LINE__, __func__,   \
		##args))

static void lcd_cfg_panel_info(struct panel_extend_para *info)
{
	u32 i = 0, j = 0;
	u32 items;
	u8 lcd_gamma_tbl[][2] = {
		{ 0, 0 },     { 15, 15 },   { 30, 30 },	  { 45, 45 },
		{ 60, 60 },   { 75, 75 },   { 90, 90 },	  { 105, 105 },
		{ 120, 120 }, { 135, 135 }, { 150, 150 }, { 165, 165 },
		{ 180, 180 }, { 195, 195 }, { 210, 210 }, { 225, 225 },
		{ 240, 240 }, { 255, 255 },
	};

	u32 lcd_cmap_tbl[2][3][4] = {
		{
			{ LCD_CMAP_G0, LCD_CMAP_B1, LCD_CMAP_G2, LCD_CMAP_B3 },
			{ LCD_CMAP_B0, LCD_CMAP_R1, LCD_CMAP_B2, LCD_CMAP_R3 },
			{ LCD_CMAP_R0, LCD_CMAP_G1, LCD_CMAP_R2, LCD_CMAP_G3 },
		},
		{
			{ LCD_CMAP_B3, LCD_CMAP_G2, LCD_CMAP_B1, LCD_CMAP_G0 },
			{ LCD_CMAP_R3, LCD_CMAP_B2, LCD_CMAP_R1, LCD_CMAP_B0 },
			{ LCD_CMAP_G3, LCD_CMAP_R2, LCD_CMAP_G1, LCD_CMAP_R0 },
		},
	};

	items = sizeof(lcd_gamma_tbl) / 2;
	for (i = 0; i < items - 1; i++) {
		u32 num = lcd_gamma_tbl[i + 1][0] - lcd_gamma_tbl[i][0];

		for (j = 0; j < num; j++) {
			u32 value = 0;

			value = lcd_gamma_tbl[i][1] +
				((lcd_gamma_tbl[i + 1][1] -
				  lcd_gamma_tbl[i][1]) *
				 j) / num;
			info->lcd_gamma_tbl[lcd_gamma_tbl[i][0] + j] =
				(value << 16) + (value << 8) + value;
		}
	}
	info->lcd_gamma_tbl[255] = (lcd_gamma_tbl[items - 1][1] << 16) +
				   (lcd_gamma_tbl[items - 1][1] << 8) +
				   lcd_gamma_tbl[items - 1][1];

	memcpy(info->lcd_cmap_tbl, lcd_cmap_tbl, sizeof(lcd_cmap_tbl));
}

static s32 lcd_open_flow(u32 sel)
{
	DBG_INFO("\n");
	LCD_OPEN_FUNC(sel, lcd_power_on, 15);
	LCD_OPEN_FUNC(sel, lcd_panel_init, 120);
	LCD_OPEN_FUNC(sel, sunxi_lcd_tcon_enable, 5);
	LCD_OPEN_FUNC(sel, lcd_bl_open, 0);
	return 0;
}

static s32 lcd_close_flow(u32 sel)
{
	DBG_INFO("\n");
	LCD_CLOSE_FUNC(sel, lcd_bl_close, 0);
	LCD_CLOSE_FUNC(sel, lcd_panel_exit, 120);
	LCD_CLOSE_FUNC(sel, sunxi_lcd_tcon_disable, 0);
	LCD_CLOSE_FUNC(sel, lcd_power_off, 0);

	return 0;
}

static void lcd_power_on(u32 sel)
{
	DBG_INFO("\n");
	sunxi_lcd_pin_cfg(sel, 1);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_power_enable(sel, 0);
	sunxi_lcd_delay_ms(10);
	sunxi_lcd_power_enable(sel, 1);
}

static void lcd_power_off(u32 sel)
{
	DBG_INFO("\n");
	sunxi_lcd_pin_cfg(sel, 0);
	sunxi_lcd_delay_ms(20);
	panel_reset(sel, 0);
	sunxi_lcd_delay_ms(5);
	sunxi_lcd_power_disable(sel, 1);
	sunxi_lcd_delay_ms(5);
	sunxi_lcd_power_disable(sel, 0);
	sunxi_lcd_delay_ms(20);
	sunxi_lcd_gpio_set_value(sel, 1, 0);
}

static void lcd_bl_open(u32 sel)
{
	DBG_INFO("\n");
	sunxi_lcd_pwm_enable(sel);
	sunxi_lcd_backlight_enable(sel);
}

static void lcd_bl_close(u32 sel)
{
	DBG_INFO("\n");
	sunxi_lcd_backlight_disable(sel);
	sunxi_lcd_pwm_disable(sel);
}

#define REGFLAG_DELAY 0xFE
#define REGFLAG_END_OF_TABLE 0xFF // END OF REGISTERS MARKER

struct LCM_setting_table {
	__u8 cmd;
	__u32 count;
	__u8 para_list[64];
};

static struct LCM_setting_table g1240tr101gg_003_initialization_setting[] = {
	{ 0xfe, 1, { 0x00 } },
	{ 0x35, 1, { 0x00 } },
	{ 0x53, 1, { 0x20 } },
	{ 0x51, 1, { 0x5f } },
	{ 0x63, 1, { 0xff } },

	{ 0x2a, 4, { 0x00, 0x00, 0x01, 0xc1 } },
	{ 0x2b, 4, { 0x00, 0x00, 0x02, 0x57 } },

	{ 0x11, 1, {} },
	{ REGFLAG_DELAY, 100, {} },
	{ 0x29, 1, {} },
	{ REGFLAG_DELAY, 40, {} },
	{ 0x51, 1, { 0xff } },

	{ REGFLAG_END_OF_TABLE, REGFLAG_END_OF_TABLE, {} },
};

static void lcd_panel_init(u32 sel)
{
	int index;

	DBG_INFO("\n");
	sunxi_lcd_pin_cfg(sel, 1);
	sunxi_lcd_delay_ms(10);
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(10);
	panel_reset(sel, 0);
	DBG_INFO("panel_reset done\n");
	sunxi_lcd_delay_ms(40);
	panel_reset(sel, 1);
	sunxi_lcd_delay_ms(120);

	sunxi_lcd_dsi_clk_enable(sel);
	sunxi_lcd_delay_ms(10);

	for (index = 0;; ++index) {
		if (g1240tr101gg_003_initialization_setting[index].count ==
		    REGFLAG_END_OF_TABLE) {
			break;
		} else if (g1240tr101gg_003_initialization_setting[index]
				   .count == REGFLAG_DELAY) {
			sunxi_lcd_delay_ms(
				g1240tr101gg_003_initialization_setting[index]
					.para_list[0]);
		} else {
			sunxi_lcd_dsi_gen_write(
				sel,
				g1240tr101gg_003_initialization_setting[index]
					.cmd,
				g1240tr101gg_003_initialization_setting[index]
					.para_list,
				g1240tr101gg_003_initialization_setting[index]
					.count);
		}
	}
	DBG_INFO("panel_cmd ok");
}

static void lcd_panel_exit(u32 sel)
{
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x28);
	sunxi_lcd_delay_ms(200);
	sunxi_lcd_dsi_dcs_write_0para(sel, 0x10);
	sunxi_lcd_delay_ms(100);
	sunxi_lcd_dsi_clk_disable(sel);
}

/*sel: 0:lcd0; 1:lcd1*/
static s32 lcd_user_defined_func(u32 sel, u32 para1, u32 para2, u32 para3)
{
	return 0;
}

static int lcd_rw_info(struct disp_lcd_hs_rw_info *p_info)
{
	if (!p_info)
		return -1;
	p_info->freq = 1;
	p_info->rw_en = true;
	return 0;
}

static s32 lcd_hs_mode_rw(u32 sel)
{
	sunxi_lcd_dsi_dcs_write_1para(sel, 0x51, 0x5f);
	return 0;
}

struct __lcd_panel g1240tr101gg_003_panel = {
	/* panel driver name, must mach the name of
	 * lcd_drv_name in sys_config.fex
	 */
	.name = "g1240tr101gg-003",
	.func = {
		.cfg_panel_info = lcd_cfg_panel_info,
		.cfg_open_flow = lcd_open_flow,
		.cfg_close_flow = lcd_close_flow,
		.lcd_user_defined_func = lcd_user_defined_func,
		.hs_mode_rw = lcd_hs_mode_rw,
		.set_rw_info = lcd_rw_info,
	},
};
