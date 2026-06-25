
/*
 * mipi subdev driver module
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-subdev.h>
#include <linux/debugfs.h>
#include <linux/mpp.h>
#include "bsp_mipi_csi.h"
#include "combo_rx/combo_rx_reg.h"
#include "combo_rx/combo_rx_reg_i.h"
#include "combo_csi/combo_csi_reg.h"
#include "sunxi_mipi.h"
#include "../platform/platform_cfg.h"
#include "../vin-video/vin_core.h"
#define MIPI_MODULE_NAME "vin_mipi"

#define IS_FLAG(x, y) (((x)&(y)) == y)

/* #define MIPI_IRQ_USE */

struct mipi_dev *glb_mipi[VIN_MAX_MIPI];

#define MIPI_DEBUGFS_BUF_SIZE 768
struct mipi_debugfs_buffer {
	size_t count;
	char data[MIPI_DEBUGFS_BUF_SIZE * VIN_MAX_DEV];
};

extern struct dentry *vi_debugfs_root;
struct dentry *mipi_debugfs_root, *mipi_node;
size_t mipi_status_size[VIN_MAX_DEV];
size_t mipi_status_size_sum;

#ifdef CONFIG_ENABLE_MIPI_TRNDS_RECOVER
struct mipi_check_trnds_s {
	struct task_struct *trnds_thread;
	int thread_run;
	int mipi_en[VIN_MAX_MIPI];
	struct mutex mipi_mutex[VIN_MAX_MIPI];
};

static struct mipi_check_trnds_s mipi_check_trnds;

static int mipi_check_trnds_func(void *data)
{
	struct mipi_check_trnds_s *mipi_date = (struct mipi_check_trnds_s *)data;
	unsigned int read_data, read_control_val;
	int bit_data;
	int i, ret;

	vin_err("enable mipi trnds status revocery \n");
	/*init mutex*/
	for (i = 0 ; i < VIN_MAX_MIPI; i++) {
		mutex_init(&mipi_date->mipi_mutex[i]);
	}

	/*map soc addr*/
#ifdef CONFIG_ARCH_SUN8IW21P1

#define MIPI_BASE 0x05810100
#define ADDR_BASE_OFFSET 0x100
#define CSI_CLK_ADDR 0x02001C04

#else
	vin_err("This chip not support MIPI_TRNDS_RECOVER func \n");
	mipi_date->thread_run = 0;
	ret = -ENOMEM;;
	goto ERR_MIPI_MAP;
#endif

	void *mipi_base[VIN_MAX_MIPI];
	void *csi_clk_addr;
	for (i = 0; i < VIN_MAX_MIPI; i++) {
		mipi_base[i] = ioremap(MIPI_BASE + i * ADDR_BASE_OFFSET, 0xf0);
		if (!mipi_base[i]) {
			vin_err("Failed to map mipi base address in mipi driver\n");
			mipi_date->thread_run = 0;
			ret = -ENOMEM;;
			goto ERR_MIPI_MAP;
		}
	}

	csi_clk_addr = ioremap(CSI_CLK_ADDR, 0x4);
	if (!csi_clk_addr) {
		vin_err("Failed to map csi_clk_addr address in mipi driver\n");
		mipi_date->thread_run = 0;
		ret = -ENOMEM;
		goto ERR_CSI_MAP;
	}
	while (mipi_date->thread_run) {
		/*check csi clk was be open, if not,dont read mipi addr*/
		read_data = readl(csi_clk_addr);
		bit_data = (read_data >> 31) & 1;
		if (bit_data == 0)  //not open csi clk
			continue;

		/*check mipi trnds*/
		for (i = 0; i < VIN_MAX_MIPI; i++) {
			mutex_lock(&mipi_date->mipi_mutex[i]);/*get mutex lock*/
			if (mipi_base[i] && mipi_date->mipi_en[i]) {
				read_data = readl(mipi_base[i] + 0xf0);
				bit_data = read_data & 0xf;  //d0
				if (bit_data == 4) {
					vin_err("Warning, mipi%d d0 was be \
					  trnds, now recovery mipi%d\n", i, i);
					read_control_val =
						   readl(mipi_base[i] + 0x00);
					writel(0x0, mipi_base[i] + 0x00);
					writel(read_control_val,
							mipi_base[i] + 0x00);
					mutex_unlock(&mipi_date->mipi_mutex[i]);/*unlock mutex lock*/
					continue;
				}

				bit_data = (read_data  >> 4) & 0xf;  //d1
				if (bit_data == 4) {
					vin_err("Warning, mipi%d d1 was be \
					  trnds, now recovery mipi%d\n", i, i);
					read_control_val =
						   readl(mipi_base[i] + 0x00);
					writel(0x0, mipi_base[i] + 0x00);
					writel(read_control_val,
							mipi_base[i] + 0x00);
					mutex_unlock(&mipi_date->mipi_mutex[i]);/*unlock mutex lock*/
					continue;
				}

				bit_data = (read_data  >> 16) & 0xf;  //clk
				if (bit_data == 4) {
					vin_err("Warning, mipi%d clk was be \
					  trnds, now recovery mipi%d\n", i, i);
					read_control_val =
						   readl(mipi_base[i] + 0x00);
					writel(0x0, mipi_base[i] + 0x00);
					writel(read_control_val, mipi_base[i] + 0x00);
					mutex_unlock(&mipi_date->mipi_mutex[i]);/*unlock mutex lock*/
					continue;
				}
			}
			mutex_unlock(&mipi_date->mipi_mutex[i]);/*unlock mutex lock*/
		}
		msleep(33); // 15fps
	} //End of while

	ret = 0;

	/*free map & mutex*/
	if (csi_clk_addr)
		iounmap(csi_clk_addr);

ERR_CSI_MAP:
	for (i = 0; i < VIN_MAX_MIPI; i++) {
		if (mipi_base[i]) {
			iounmap(mipi_base[i]);
		}
	}

ERR_MIPI_MAP:
	for (i = 0; i < VIN_MAX_MIPI; i++) {
		 mutex_destroy(&mipi_date->mipi_mutex[i]);
	}

	return ret;
}
#endif

static void mipi_set_st_time(int id, unsigned int time)
{
	struct mipi_dev *mipi;

	if (id >= VIN_MAX_MIPI) {
		vin_print("mipi id error,set it less than %d\n", VIN_MAX_MIPI);
		return;
	}

	vin_print("Set mipi%d settle time as 0x%x\n", id, time);
	mipi = glb_mipi[id];
	if (mipi) {
		mipi->settle_time = time;
		if (mipi->subdev.entity.stream_count > 0) {
#if defined CONFIG_ARCH_SUN8IW16P1
			cmb_rx_mipi_stl_time(mipi->id, time);
#elif defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
			cmb_phy0_s2p_dly(mipi->id, time);
#else
			mipi_dphy_cfg_1data(mipi->id, 0x75, time);
#endif
		}
	}
}

static ssize_t mipi_settle_time_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mipi_dev *mipi;
	int i, cnt = 0;

	for (i = 0; i < VIN_MAX_MIPI; i++) {
		mipi = glb_mipi[i];
		if (mipi)
			cnt += sprintf(buf + cnt, "mipi%d settle time = 0x%x\n",
			mipi->id, mipi->settle_time ? mipi->settle_time : mipi->time_hs);
	}
	return cnt;
}

static ssize_t mipi_settle_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	int mipi_id;

	val = simple_strtoul(buf, NULL, 16);
	mipi_id = val >> 8;
	mipi_set_st_time(mipi_id, val & 0xff);
	return count;
}

static DEVICE_ATTR(settle_time, S_IRUGO | S_IWUSR | S_IWGRP,
		   mipi_settle_time_show, mipi_settle_time_store);

static struct combo_format sunxi_mipi_formats[] = {
	{
		.code = MEDIA_BUS_FMT_SBGGR8_1X8,
		.bit_width = RAW8,
	}, {
		.code = MEDIA_BUS_FMT_SGBRG8_1X8,
		.bit_width = RAW8,
	}, {
		.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		.bit_width = RAW8,
	}, {
		.code = MEDIA_BUS_FMT_SRGGB8_1X8,
		.bit_width = RAW8,
	}, {
		.code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.bit_width = RAW10,
	}, {
		.code = MEDIA_BUS_FMT_SGBRG10_1X10,
		.bit_width = RAW10,
	}, {
		.code = MEDIA_BUS_FMT_SGRBG10_1X10,
		.bit_width = RAW10,
	}, {
		.code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.bit_width = RAW10,
	}, {
		.code = MEDIA_BUS_FMT_SBGGR12_1X12,
		.bit_width = RAW12,
	}, {
		.code = MEDIA_BUS_FMT_SGBRG12_1X12,
		.bit_width = RAW12,
	}, {
		.code = MEDIA_BUS_FMT_SGRBG12_1X12,
		.bit_width = RAW12,
	}, {
		.code = MEDIA_BUS_FMT_SRGGB12_1X12,
		.bit_width = RAW12,
	}, {
		.code = MEDIA_BUS_FMT_UYVY8_2X8,
		.bit_width = YUV8,
		.yuv_seq = UYVY,
	}, {
		.code = MEDIA_BUS_FMT_VYUY8_2X8,
		.bit_width = YUV8,
		.yuv_seq = VYUY,
	}, {
		.code = MEDIA_BUS_FMT_YUYV8_2X8,
		.bit_width = YUV8,
		.yuv_seq = YUYV,
	}, {
		.code = MEDIA_BUS_FMT_YVYU8_2X8,
		.bit_width = YUV8,
		.yuv_seq = YVYU,
	}, {
		.code = MEDIA_BUS_FMT_UYVY8_1X16,
		.bit_width = YUV8,
		.yuv_seq = UYVY,
	}, {
		.code = MEDIA_BUS_FMT_VYUY8_1X16,
		.bit_width = YUV8,
		.yuv_seq = VYUY,
	}, {
		.code = MEDIA_BUS_FMT_YUYV8_1X16,
		.bit_width = YUV8,
		.yuv_seq = YUYV,
	}, {
		.code = MEDIA_BUS_FMT_YVYU8_1X16,
		.bit_width = YUV8,
		.yuv_seq = YVYU,
	}, {
		.code = MEDIA_BUS_FMT_UYVY10_2X10,
		.bit_width = YUV10,
		.yuv_seq = UYVY,
	}, {
		.code = MEDIA_BUS_FMT_VYUY10_2X10,
		.bit_width = YUV10,
		.yuv_seq = VYUY,
	}, {
		.code = MEDIA_BUS_FMT_YUYV10_2X10,
		.bit_width = YUV10,
		.yuv_seq = YUYV,
	}, {
		.code = MEDIA_BUS_FMT_YVYU10_2X10,
		.bit_width = YUV10,
		.yuv_seq = YVYU,
	}, {
		.code = MEDIA_BUS_FMT_RGB888_1X24,
		.bit_width = RAW8,
	}, {
		.code = MEDIA_BUS_FMT_BGR888_1X24,
		.bit_width = RAW8,
	}, {
		.code = MEDIA_BUS_FMT_GBR888_1X24,
		.bit_width = RAW8,
	}
};

static enum pkt_fmt get_pkt_fmt(u32 code)
{
	switch (code) {
	case MEDIA_BUS_FMT_RGB565_2X8_BE:
		return MIPI_RGB565;
	case MEDIA_BUS_FMT_RGB888_1X24:
	case MEDIA_BUS_FMT_BGR888_1X24:
	case MEDIA_BUS_FMT_GBR888_1X24:
		return MIPI_RGB888;
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
	case MEDIA_BUS_FMT_UYVY8_2X8:
	case MEDIA_BUS_FMT_VYUY8_2X8:
	case MEDIA_BUS_FMT_YUYV8_2X8:
	case MEDIA_BUS_FMT_YVYU8_2X8:
		return MIPI_YUV422;
	case MEDIA_BUS_FMT_UYVY10_2X10:
	case MEDIA_BUS_FMT_VYUY10_2X10:
	case MEDIA_BUS_FMT_YUYV10_2X10:
	case MEDIA_BUS_FMT_YVYU10_2X10:
		return MIPI_YUV422_10;
	case MEDIA_BUS_FMT_SBGGR8_1X8:
	case MEDIA_BUS_FMT_SGBRG8_1X8:
	case MEDIA_BUS_FMT_SGRBG8_1X8:
	case MEDIA_BUS_FMT_SRGGB8_1X8:
		return MIPI_RAW8;
	case MEDIA_BUS_FMT_SBGGR10_1X10:
	case MEDIA_BUS_FMT_SGBRG10_1X10:
	case MEDIA_BUS_FMT_SGRBG10_1X10:
	case MEDIA_BUS_FMT_SRGGB10_1X10:
		return MIPI_RAW10;
	case MEDIA_BUS_FMT_SBGGR12_1X12:
	case MEDIA_BUS_FMT_SGBRG12_1X12:
	case MEDIA_BUS_FMT_SGRBG12_1X12:
	case MEDIA_BUS_FMT_SRGGB12_1X12:
		return MIPI_RAW12;
	default:
		return MIPI_RAW8;
	}
}

static unsigned int data_formats_type(u32 code)
{
	switch (code) {
	case MIPI_RAW8:
	case MIPI_RAW12:
		return RAW;
	case MIPI_YUV422:
	case MIPI_YUV422_10:
		return YUV;
	case MIPI_RGB565:
	case MIPI_RGB888:
		return RGB;
	default:
		return RAW;
	}
}

#if defined CONFIG_ARCH_SUN8IW21P1
static int __mcsi_pin_config(struct mipi_dev *dev, int enable)
{
#ifndef FPGA_VER
	char pinctrl_names[20] = "";

	if (!IS_ERR_OR_NULL(dev->pctrl))
		devm_pinctrl_put(dev->pctrl);

	switch (enable) {
	case 1:
		sprintf(pinctrl_names, "mipi%d-default", dev->id);
		break;
	case 0:
		sprintf(pinctrl_names, "mipi%d-sleep", dev->id);
		break;
	default:
		return -1;
	}

	dev->pctrl = devm_pinctrl_get_select(&dev->pdev->dev, pinctrl_names);
	if (IS_ERR_OR_NULL(dev->pctrl)) {
		vin_err("mipi%d request pinctrl handle failed!\n", dev->id);
		return -EINVAL;
	}
	usleep_range(100, 120);

	/* 4lane needs init mipiA/B pins */
	if (dev->cmb_csi_cfg.phy_link_mode == ONE_4LANE) {
		if (enable == 1)
			strcpy(pinctrl_names, "mipi1-4lane-default");
		else
			strcpy(pinctrl_names, "mipi1-4lane-sleep");

		dev->pctrl = devm_pinctrl_get_select(&dev->pdev->dev, pinctrl_names);
		if (IS_ERR_OR_NULL(dev->pctrl)) {
			vin_err("mipib 4lane request pinctrl handle failed!\n");
			return -EINVAL;
		}
		usleep_range(100, 120);
	}
#endif
	return 0;
}

static int __mcsi_pin_release(struct mipi_dev *dev)
{
#ifndef FPGA_VER
	if (!IS_ERR_OR_NULL(dev->pctrl))
		devm_pinctrl_put(dev->pctrl);
#endif
	return 0;
}
#endif

#if defined CONFIG_ARCH_SUN8IW16P1
void combo_rx_mipi_init(struct v4l2_subdev *sd)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);
	struct mipi_ctr mipi_ctr;
	struct mipi_lane_map mipi_map;

	memset(&mipi_ctr, 0, sizeof(mipi_ctr));
	mipi_ctr.mipi_lane_num = mipi->cmb_cfg.mipi_ln;
	mipi_ctr.mipi_msb_lsb_sel = 1;

	if (mipi->cmb_mode == MIPI_NORMAL_MODE) {
		mipi_ctr.mipi_wdr_mode_sel = 0;
	} else if (mipi->cmb_mode == MIPI_VC_WDR_MODE) {
		mipi_ctr.mipi_wdr_mode_sel = 0;
		mipi_ctr.mipi_open_multi_ch = 1;
		mipi_ctr.mipi_ch0_height = mipi->format.height;
		mipi_ctr.mipi_ch1_height = mipi->format.height;
		mipi_ctr.mipi_ch2_height = mipi->format.height;
		mipi_ctr.mipi_ch3_height = mipi->format.height;
	} else if (mipi->cmb_mode == MIPI_DOL_WDR_MODE) {
		mipi_ctr.mipi_wdr_mode_sel = 2;
	}

	mipi_map.mipi_lane0 = MIPI_IN_L0_USE_PAD_LANE0;
	mipi_map.mipi_lane1 = MIPI_IN_L1_USE_PAD_LANE1;
	mipi_map.mipi_lane2 = MIPI_IN_L2_USE_PAD_LANE2;
	mipi_map.mipi_lane3 = MIPI_IN_L3_USE_PAD_LANE3;

	cmb_rx_mode_sel(mipi->id, D_PHY);
	cmb_rx_app_pixel_out(mipi->id, TWO_PIXEL);
	cmb_rx_mipi_stl_time(mipi->id, mipi->time_hs);
	cmb_rx_mipi_ctr(mipi->id, &mipi_ctr);
	cmb_rx_mipi_dphy_mapping(mipi->id, &mipi_map);
}

void combo_rx_sub_lvds_init(struct v4l2_subdev *sd)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);
	struct lvds_ctr lvds_ctr;

	memset(&lvds_ctr, 0, sizeof(lvds_ctr));
	lvds_ctr.lvds_bit_width = mipi->cmb_fmt->bit_width;
	lvds_ctr.lvds_lane_num = mipi->cmb_cfg.lvds_ln;

	if (mipi->cmb_mode == LVDS_NORMAL_MODE) {
		lvds_ctr.lvds_line_code_mode = 1;
	} else if (mipi->cmb_mode == LVDS_4CODE_WDR_MODE) {
		lvds_ctr.lvds_line_code_mode = mipi->wdr_cfg.line_code_mode;

		lvds_ctr.lvds_wdr_lbl_sel = 1;
		lvds_ctr.lvds_sync_code_line_cnt = mipi->wdr_cfg.line_cnt;

		lvds_ctr.lvds_code_mask = mipi->wdr_cfg.code_mask;
		lvds_ctr.lvds_wdr_fid_mode_sel = mipi->wdr_cfg.wdr_fid_mode_sel;
		lvds_ctr.lvds_wdr_fid_map_en = mipi->wdr_cfg.wdr_fid_map_en;
		lvds_ctr.lvds_wdr_fid0_map_sel = mipi->wdr_cfg.wdr_fid0_map_sel;
		lvds_ctr.lvds_wdr_fid1_map_sel = mipi->wdr_cfg.wdr_fid1_map_sel;
		lvds_ctr.lvds_wdr_fid2_map_sel = mipi->wdr_cfg.wdr_fid2_map_sel;
		lvds_ctr.lvds_wdr_fid3_map_sel = mipi->wdr_cfg.wdr_fid3_map_sel;

		lvds_ctr.lvds_wdr_en_multi_ch = mipi->wdr_cfg.wdr_en_multi_ch;
		lvds_ctr.lvds_wdr_ch0_height = mipi->wdr_cfg.wdr_ch0_height;
		lvds_ctr.lvds_wdr_ch1_height = mipi->wdr_cfg.wdr_ch1_height;
		lvds_ctr.lvds_wdr_ch2_height = mipi->wdr_cfg.wdr_ch2_height;
		lvds_ctr.lvds_wdr_ch3_height = mipi->wdr_cfg.wdr_ch3_height;
	} else if (mipi->cmb_mode == LVDS_5CODE_WDR_MODE) {
		lvds_ctr.lvds_line_code_mode = mipi->wdr_cfg.line_code_mode;
		lvds_ctr.lvds_wdr_lbl_sel = 2;
		lvds_ctr.lvds_sync_code_line_cnt = mipi->wdr_cfg.line_cnt;

		lvds_ctr.lvds_code_mask = mipi->wdr_cfg.code_mask;
	}

	cmb_rx_mode_sel(mipi->id, SUB_LVDS);
	cmb_rx_app_pixel_out(mipi->id, ONE_PIXEL);
	cmb_rx_lvds_ctr(mipi->id, &lvds_ctr);
	cmb_rx_lvds_mapping(mipi->id, &mipi->lvds_map);
	cmb_rx_lvds_sync_code(mipi->id, &mipi->sync_code);
}

void combo_rx_hispi_init(struct v4l2_subdev *sd)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);
	struct lvds_ctr lvds_ctr;
	struct hispi_ctr hispi_ctr;

	memset(&hispi_ctr, 0, sizeof(hispi_ctr));
	memset(&lvds_ctr, 0, sizeof(lvds_ctr));
	lvds_ctr.lvds_bit_width = mipi->cmb_fmt->bit_width;
	lvds_ctr.lvds_lane_num = mipi->cmb_cfg.lvds_ln;

	if (mipi->cmb_mode == HISPI_NORMAL_MODE) {
		lvds_ctr.lvds_line_code_mode = 0;
		lvds_ctr.lvds_pix_lsb = 1;

		hispi_ctr.hispi_normal = 1;
		hispi_ctr.hispi_trans_mode = PACKETIZED_SP;
	} else if (mipi->cmb_mode == HISPI_WDR_MODE) {
		lvds_ctr.lvds_line_code_mode = mipi->wdr_cfg.line_code_mode;

		lvds_ctr.lvds_wdr_lbl_sel = 1;

		lvds_ctr.lvds_pix_lsb = mipi->wdr_cfg.pix_lsb;
		lvds_ctr.lvds_sync_code_line_cnt = mipi->wdr_cfg.line_cnt;

		lvds_ctr.lvds_wdr_fid_mode_sel = mipi->wdr_cfg.wdr_fid_mode_sel;
		lvds_ctr.lvds_wdr_fid_map_en = mipi->wdr_cfg.wdr_fid_map_en;
		lvds_ctr.lvds_wdr_fid0_map_sel = mipi->wdr_cfg.wdr_fid0_map_sel;
		lvds_ctr.lvds_wdr_fid1_map_sel = mipi->wdr_cfg.wdr_fid1_map_sel;
		lvds_ctr.lvds_wdr_fid2_map_sel = mipi->wdr_cfg.wdr_fid2_map_sel;
		lvds_ctr.lvds_wdr_fid3_map_sel = mipi->wdr_cfg.wdr_fid3_map_sel;

		hispi_ctr.hispi_normal = 1;
		hispi_ctr.hispi_trans_mode = PACKETIZED_SP;
		hispi_ctr.hispi_wdr_en = 1;
		hispi_ctr.hispi_wdr_sof_fild = mipi->wdr_cfg.wdr_sof_fild;
		hispi_ctr.hispi_wdr_eof_fild = mipi->wdr_cfg.wdr_eof_fild;
		hispi_ctr.hispi_code_mask = mipi->wdr_cfg.code_mask;
	}

	cmb_rx_mode_sel(mipi->id, SUB_LVDS);
	cmb_rx_app_pixel_out(mipi->id, ONE_PIXEL);
	cmb_rx_lvds_ctr(mipi->id, &lvds_ctr);
	cmb_rx_lvds_mapping(mipi->id, &mipi->lvds_map);
	cmb_rx_lvds_sync_code(mipi->id, &mipi->sync_code);

	cmb_rx_hispi_ctr(mipi->id, &hispi_ctr);
}

void combo_rx_init(struct v4l2_subdev *sd)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);

	/*comnbo rx phya init*/
	cmb_rx_phya_config(mipi->id);

	if (mipi->terminal_resistance) {
		vin_log(VIN_LOG_MIPI, "open combo terminal resitance!\n");
		cmb_rx_te_auto_disable(mipi->id, 1);
		cmb_rx_phya_a_d0_en(mipi->id, 1);
		cmb_rx_phya_b_d0_en(mipi->id, 1);
		cmb_rx_phya_c_d0_en(mipi->id, 1);
		cmb_rx_phya_a_d1_en(mipi->id, 1);
		cmb_rx_phya_b_d1_en(mipi->id, 1);
		cmb_rx_phya_c_d1_en(mipi->id, 1);
		cmb_rx_phya_a_d2_en(mipi->id, 1);
		cmb_rx_phya_b_d2_en(mipi->id, 1);
		cmb_rx_phya_c_d2_en(mipi->id, 1);
		cmb_rx_phya_a_d3_en(mipi->id, 1);
		cmb_rx_phya_b_d3_en(mipi->id, 1);
		cmb_rx_phya_c_d3_en(mipi->id, 1);
		cmb_rx_phya_a_ck_en(mipi->id, 1);
		cmb_rx_phya_b_ck_en(mipi->id, 1);
		cmb_rx_phya_c_ck_en(mipi->id, 1);
	}
	cmb_rx_phya_signal_dly_en(mipi->id, 1);
	cmb_rx_phya_offset(mipi->id, mipi->pyha_offset);

	switch (mipi->if_type) {
	case V4L2_MBUS_PARALLEL:
	case V4L2_MBUS_BT656:
		cmb_rx_mode_sel(mipi->id, CMOS);
		cmb_rx_app_pixel_out(mipi->id, ONE_PIXEL);
		break;
	case V4L2_MBUS_CSI2:
		cmb_rx_phya_ck_mode(mipi->id, 0);
		combo_rx_mipi_init(sd);
		break;
	case V4L2_MBUS_SUBLVDS:
		cmb_rx_phya_ck_mode(mipi->id, 1);
		combo_rx_sub_lvds_init(sd);
		break;
	case V4L2_MBUS_HISPI:
		cmb_rx_phya_ck_mode(mipi->id, 0);
		combo_rx_hispi_init(sd);
		break;
	default:
		combo_rx_mipi_init(sd);
		break;
	}

	cmb_rx_enable(mipi->id);
}
#elif defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
void cmb_phy_init(struct mipi_dev *mipi)
{
	cmb_phy_lane_num_en(mipi->id + mipi->phy_offset, mipi->cmb_csi_cfg.phy_lane_cfg);
	cmb_phy0_work_mode(mipi->id + mipi->phy_offset, 0);
	cmb_phy0_ofscal_cfg(mipi->id + mipi->phy_offset);
	//cmb_phy_deskew_en(mipi->id + mipi->phy_offset, mipi->cmb_csi_cfg.phy_lane_cfg);
	cmb_term_ctl(mipi->id + mipi->phy_offset, mipi->cmb_csi_cfg.phy_lane_cfg);
	cmb_hs_ctl(mipi->id + mipi->phy_offset, mipi->cmb_csi_cfg.phy_lane_cfg);
	cmb_s2p_ctl(mipi->id + mipi->phy_offset, mipi->time_hs, mipi->cmb_csi_cfg.phy_lane_cfg);
	cmb_mipirx_ctl(mipi->id + mipi->phy_offset, mipi->cmb_csi_cfg.phy_lane_cfg);
	cmb_phy0_en(mipi->id + mipi->phy_offset, 1);
	cmb_phy0_freq_en(mipi->id, 1);
	cmb_phy_deskew1_cfg(mipi->id + mipi->phy_offset, mipi->deskew, mipi->cmb_mode == MIPI_VC_WDR_MODE ? true : false);
}

static void combo_csi_mipi_init(struct v4l2_subdev *sd)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf = &mipi->format;
	int i;

	mipi->cmb_csi_cfg.phy_lane_cfg.phy_laneck_en = CK_1LANE;
	mipi->cmb_csi_cfg.phy_lane_cfg.phy_mipi_lpck_en = LPCK_1LANE;
	mipi->cmb_csi_cfg.phy_lane_cfg.phy_termck_en = TERMCK_CLOSE;
	mipi->cmb_csi_cfg.phy_lane_cfg.phy_termdt_en = TERMDT_CLOSE;
	mipi->cmb_csi_cfg.phy_lane_cfg.phy_s2p_en = S2PDT_CLOSE;
	mipi->cmb_csi_cfg.phy_lane_cfg.phy_hsck_en = HSCK_CLOSE;
	mipi->cmb_csi_cfg.phy_lane_cfg.phy_hsdt_en = HSDT_CLOSE;

	cmb_phy_init(mipi);

	/* V853 4lane needs cfg link mode */
	if (mipi->cmb_csi_cfg.phy_link_mode == ONE_4LANE) {
		mipi->cmb_csi_cfg.phy_lane_cfg.phy_mipi_lpck_en = LPCK_CLOSE;
		mipi->phy_offset = 1;
		cmb_phy_link_mode_set(ONE_4LANE);
		cmb_phy_init(mipi);
		mipi->phy_offset = 0;
	}

	for (i = 0; i < mipi->cmb_csi_cfg.lane_num; i++)
		mipi->cmb_csi_cfg.mipi_lane[i] = cmb_port_set_lane_map(mipi->id, i);

	for (i = 0; i < mipi->cmb_csi_cfg.total_rx_ch; i++) {
		mipi->cmb_csi_cfg.mipi_datatype[i] = get_pkt_fmt(mipi->format.code);
		mipi->cmb_csi_cfg.vc[i] = i;
		mipi->cmb_csi_cfg.field[i] = mf->field;
		cmb_port_mipi_ch_trigger_en(mipi->id, i, 1);
#ifdef MIPI_IRQ_USE
		cmb_port_int_enable(mipi->id, i, MIPI_FRAME_SYNC_ERR | MIPI_LINE_SYNC_ERR |
				MIPI_ECC_WRN | MIPI_ECC_ERR | MIPI_CHKSUM_ERR |	MIPI_EOT_ERR);
#endif
	}

	cmb_port_lane_num(mipi->id, mipi->cmb_csi_cfg.lane_num);
	cmb_port_out_num(mipi->id, mipi->cmb_csi_cfg.pix_num);
	if (mipi->cmb_mode == MIPI_DOL_WDR_MODE)
		cmb_port_out_chnum(mipi->id, 0);
	else
		cmb_port_out_chnum(mipi->id, mipi->cmb_csi_cfg.total_rx_ch - 1);
	cmb_port_lane_map(mipi->id, mipi->cmb_csi_cfg.mipi_lane);
	cmb_port_mipi_cfg(mipi->id, mipi->cmb_fmt->yuv_seq);
	cmb_port_set_mipi_datatype(mipi->id, &mipi->cmb_csi_cfg);
	cmb_port_mipi_set_field(mipi->id, mipi->cmb_csi_cfg.total_rx_ch, &mipi->cmb_csi_cfg);
	if (mipi->cmb_mode == MIPI_DOL_WDR_MODE)
		cmb_port_set_mipi_wdr(mipi->id, 0, 2);
	cmb_port_enable(mipi->id);
}

void combo_csi_init(struct v4l2_subdev *sd)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);

	switch (mipi->if_type) {
	case V4L2_MBUS_PARALLEL:
	case V4L2_MBUS_BT656:
		break;
	case V4L2_MBUS_CSI2:
		combo_csi_mipi_init(sd);
		break;
	case V4L2_MBUS_SUBLVDS:
		break;
	case V4L2_MBUS_HISPI:
		break;
	default:
		break;
	}
}
#endif

static int sunxi_mipi_subdev_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf = &mipi->format;
	struct mbus_framefmt_res *res = (void *)mf->reserved;

	int i;
	__maybe_unused int cur_phy_link;

	mipi->csi2_cfg.bps = res->res_mipi_bps;
	mipi->csi2_cfg.auto_check_bps = 0;
	mipi->csi2_cfg.dphy_freq = DPHY_CLK;

	for (i = 0; i < mipi->csi2_cfg.total_rx_ch; i++) {
		mipi->csi2_fmt.packet_fmt[i] = get_pkt_fmt(mf->code);
		mipi->csi2_fmt.field[i] = mf->field;
		mipi->csi2_fmt.vc[i] = i;
	}

	mipi->csi2_fmt.fmt_type = data_formats_type(get_pkt_fmt(mf->code));
	mipi->cmb_mode = res->res_combo_mode & 0xf;
	mipi->terminal_resistance = res->res_combo_mode & CMB_TERMINAL_RES;
	mipi->pyha_offset = (res->res_combo_mode & 0x70) >> 4;
	if (mipi->settle_time > 0) {
		mipi->time_hs = mipi->settle_time;
	} else if (res->res_time_hs)
		mipi->time_hs = res->res_time_hs;
	else {
#if defined CONFIG_ARCH_SUN8IW16P1
		mipi->time_hs = 0x30;
#else
		mipi->time_hs = 0x28;
#endif
	}
	if (res->res_deskew)
		mipi->deskew = res->res_deskew;

#if defined CONFIG_ARCH_SUN8IW21P1
	__mcsi_pin_config(mipi, enable);
	cmb_phy_link_mode_get(&cur_phy_link);
	if (mipi->id == 1 && cur_phy_link == ONE_4LANE)
		vin_err("PORT0 is 4 lanes, PORT1 cannot work! \n");
#endif

	if (enable) {
		mipi->stream_flag = 1;
#if defined CONFIG_ARCH_SUN8IW16P1
		combo_rx_init(sd);
#elif defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
		combo_csi_init(sd);
#else
		bsp_mipi_csi_dphy_init(mipi->id);
		mipi_dphy_cfg_1data(mipi->id, 0x75,
			mipi->settle_time ? mipi->settle_time : 0xa0);
		bsp_mipi_csi_set_para(mipi->id, &mipi->csi2_cfg);
		bsp_mipi_csi_set_fmt(mipi->id, mipi->csi2_cfg.total_rx_ch,
				     &mipi->csi2_fmt);
		if (mipi->cmb_mode == MIPI_DOL_WDR_MODE)
			bsp_mipi_csi_set_dol(mipi->id, 0, 2);
		/*for dphy clock async*/
		bsp_mipi_csi_dphy_disable(mipi->id, mipi->sensor_flags);
		bsp_mipi_csi_dphy_enable(mipi->id, mipi->sensor_flags);
		bsp_mipi_csi_protocol_enable(mipi->id);
#endif
#ifdef CONFIG_ENABLE_MIPI_TRNDS_RECOVER
		/*enable check thread*/
		if (!mipi_check_trnds.thread_run) {
			mipi_check_trnds.thread_run = 1;
			mipi_check_trnds.trnds_thread = kthread_run(mipi_check_trnds_func, &mipi_check_trnds, "polling_thread");
			if (IS_ERR(mipi_check_trnds.trnds_thread)) {
				vin_err("Failed to create polling thread\n");
				mipi_check_trnds.thread_run = 0;
			}
		}
		/*mark mipi num need to check*/
		mipi_check_trnds.mipi_en[mipi->id] = 1;
#endif
	} else {
#ifdef CONFIG_ENABLE_MIPI_TRNDS_RECOVER
		if (mipi_check_trnds.thread_run) {
			/*get mutex lock, make sure check was be disable*/
			mutex_lock(&mipi_check_trnds.mipi_mutex[mipi->id]);
			/*make sure check was be disable*/
			mipi_check_trnds.mipi_en[mipi->id] = 0;
			mutex_unlock(&mipi_check_trnds.mipi_mutex[mipi->id]);

			/*check need to close thread*/
			for (i = 0 ; i < VIN_MAX_MIPI; i++) {
				if (mipi_check_trnds.mipi_en[i] != 0)
					break;
			}
			if (i == VIN_MAX_MIPI) { //all mipi was be close, need close thread
				mipi_check_trnds.thread_run = 0;
				kthread_stop(mipi_check_trnds.trnds_thread); //Will block until the thread return
			}
		}
#endif
		mipi->stream_flag = 0;
		mipi->cmb_csi_cfg.phy_link_mode = TWO_2LANE;
#if defined CONFIG_ARCH_SUN8IW16P1
		cmb_rx_disable(mipi->id);
#elif defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
		mipi->cmb_csi_cfg.pix_num = ONE_DATA;
#ifdef MIPI_IRQ_USE
		for (i = 0; i < mipi->cmb_csi_cfg.total_rx_ch; i++) {
			cmb_port_int_disable(mipi->id, i, MIPI_INT_ALL);
			cmb_port_int_clear_status(mipi->id, i, MIPI_INT_ALL);
		}
#endif
		cmb_port_disable(mipi->id);
		cmb_phy0_en(mipi->id, 0);
		cmb_phy0_freq_en(mipi->id, 0);
#else
		bsp_mipi_csi_dphy_disable(mipi->id, mipi->sensor_flags);
		bsp_mipi_csi_protocol_disable(mipi->id);
		bsp_mipi_csi_dphy_exit(mipi->id);
#endif
	}

	vin_log(VIN_LOG_FMT, "%s%d %s, lane_num %d, code: %x field: %d\n",
		mipi->if_name, mipi->id, enable ? "stream on" : "stream off",
		mipi->cmb_cfg.lane_num, mf->code, mf->field);

	return 0;
}

static int sunxi_mipi_subdev_get_fmt(struct v4l2_subdev *sd,
				     struct v4l2_subdev_pad_config *cfg,
				     struct v4l2_subdev_format *fmt)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf;

	mf = &mipi->format;
	if (!mf)
		return -EINVAL;

	mutex_lock(&mipi->subdev_lock);
	fmt->format = *mf;
	mutex_unlock(&mipi->subdev_lock);
	return 0;
}

static struct combo_format *__mipi_try_format(struct v4l2_mbus_framefmt *mf)
{
	struct combo_format *mipi_fmt = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(sunxi_mipi_formats); i++)
		if (mf->code == sunxi_mipi_formats[i].code)
			mipi_fmt = &sunxi_mipi_formats[i];

	if (mipi_fmt == NULL)
		mipi_fmt = &sunxi_mipi_formats[0];

	mf->code = mipi_fmt->code;

	return mipi_fmt;
}

static int sunxi_mipi_subdev_set_fmt(struct v4l2_subdev *sd,
				     struct v4l2_subdev_pad_config *cfg,
				     struct v4l2_subdev_format *fmt)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf;
	struct combo_format *mipi_fmt;

	vin_log(VIN_LOG_FMT, "%s %d*%d %x %d\n", __func__,
		fmt->format.width, fmt->format.height,
		fmt->format.code, fmt->format.field);

	mf = &mipi->format;

	if (fmt->pad == MIPI_PAD_SOURCE) {
		if (mf) {
			mutex_lock(&mipi->subdev_lock);
			fmt->format = *mf;
			mutex_unlock(&mipi->subdev_lock);
		}
		return 0;
	}

	mipi_fmt = __mipi_try_format(&fmt->format);
	if (mf) {
		mutex_lock(&mipi->subdev_lock);
		*mf = fmt->format;
		if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
			mipi->cmb_fmt = mipi_fmt;
		mutex_unlock(&mipi->subdev_lock);
	}

	return 0;
}

static int sunxi_mipi_s_mbus_config(struct v4l2_subdev *sd,
				    const struct v4l2_mbus_config *cfg)
{
	struct mipi_dev *mipi = v4l2_get_subdevdata(sd);

	if (cfg->type == V4L2_MBUS_CSI2) {
		mipi->if_type = V4L2_MBUS_CSI2;
		memcpy(mipi->if_name, "mipi", sizeof("mipi"));
		if (IS_FLAG(cfg->flags, V4L2_MBUS_CSI2_4_LANE)) {
			mipi->csi2_cfg.lane_num = 4;
			mipi->cmb_cfg.lane_num = 4;
			mipi->cmb_cfg.mipi_ln = MIPI_4LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_lanedt_en = DT_4LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_mipi_lpdt_en = LPDT_4LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_deskew_en = DK_4LANE;
			mipi->cmb_csi_cfg.lane_num = 4;
#if defined CONFIG_ARCH_SUN8IW21P1
			mipi->cmb_csi_cfg.phy_link_mode = ONE_4LANE;
			if (mipi->id)
				vin_err("PORT1 supports a maximum of 2lane!\n");
			if (glb_mipi[1]->stream_flag)
				vin_err("PORT1 in using, PORT0 cannot 4lane!\n");
#endif
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_CSI2_3_LANE)) {
			mipi->csi2_cfg.lane_num = 3;
			mipi->cmb_cfg.lane_num = 3;
			mipi->cmb_cfg.mipi_ln = MIPI_3LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_lanedt_en = DT_3LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_mipi_lpdt_en = LPDT_3LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_deskew_en = DK_3LANE;
			mipi->cmb_csi_cfg.lane_num = 3;
#if defined CONFIG_ARCH_SUN8IW21P1
			mipi->cmb_csi_cfg.phy_link_mode = ONE_4LANE;
			if (mipi->id)
				vin_err("PORT1 supports a maximum of 2lane!\n");
			if (glb_mipi[1]->stream_flag)
				vin_err("PORT1 in using, PORT0 cannot 3lane!\n");
#endif
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_CSI2_2_LANE)) {
			mipi->csi2_cfg.lane_num = 2;
			mipi->cmb_cfg.lane_num = 2;
			mipi->cmb_cfg.mipi_ln = MIPI_2LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_lanedt_en = DT_2LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_mipi_lpdt_en = LPDT_2LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_deskew_en = DK_2LANE;
			mipi->cmb_csi_cfg.lane_num = 2;
		} else {
			mipi->cmb_cfg.lane_num = 1;
			mipi->csi2_cfg.lane_num = 1;
			mipi->cmb_cfg.mipi_ln = MIPI_1LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_lanedt_en = DT_1LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_mipi_lpdt_en = LPDT_1LANE;
			mipi->cmb_csi_cfg.phy_lane_cfg.phy_deskew_en = DK_1LANE;
			mipi->cmb_csi_cfg.lane_num = 1;
		}
	} else if (cfg->type == V4L2_MBUS_SUBLVDS) {
		mipi->if_type = V4L2_MBUS_SUBLVDS;
		memcpy(mipi->if_name, "sublvds", sizeof("sublvds"));
		if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_12_LANE)) {
			mipi->cmb_cfg.lane_num = 12;
			mipi->cmb_cfg.lvds_ln = LVDS_12LANE;
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_10_LANE)) {
			mipi->cmb_cfg.lane_num = 10;
			mipi->cmb_cfg.lvds_ln = LVDS_10LANE;
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_8_LANE)) {
			mipi->cmb_cfg.lane_num = 8;
			mipi->cmb_cfg.lvds_ln = LVDS_8LANE;
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_4_LANE)) {
			mipi->cmb_cfg.lane_num = 4;
			mipi->cmb_cfg.lvds_ln = LVDS_4LANE;
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_2_LANE)) {
			mipi->cmb_cfg.lane_num = 2;
			mipi->cmb_cfg.lvds_ln = LVDS_2LANE;
		} else {
			mipi->cmb_cfg.lane_num = 8;
			mipi->cmb_cfg.lvds_ln = LVDS_8LANE;
		}
	} else if (cfg->type == V4L2_MBUS_HISPI) {
		mipi->if_type = V4L2_MBUS_HISPI;
		memcpy(mipi->if_name, "hispi", sizeof("hispi"));
		if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_12_LANE)) {
			mipi->cmb_cfg.lane_num = 12;
			mipi->cmb_cfg.lvds_ln = LVDS_12LANE;
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_10_LANE)) {
			mipi->cmb_cfg.lane_num = 10;
			mipi->cmb_cfg.lvds_ln = LVDS_10LANE;
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_8_LANE)) {
			mipi->cmb_cfg.lane_num = 8;
			mipi->cmb_cfg.lvds_ln = LVDS_8LANE;
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_4_LANE)) {
			mipi->cmb_cfg.lane_num = 4;
			mipi->cmb_cfg.lvds_ln = LVDS_4LANE;
		} else if (IS_FLAG(cfg->flags, V4L2_MBUS_SUBLVDS_2_LANE)) {
			mipi->cmb_cfg.lane_num = 2;
			mipi->cmb_cfg.lvds_ln = LVDS_2LANE;
		} else {
			mipi->cmb_cfg.lane_num = 4;
			mipi->cmb_cfg.lvds_ln = LVDS_4LANE;
		}
	} else {
		memcpy(mipi->if_name, "combo_parallel", sizeof("combo_parallel"));
		mipi->if_type = V4L2_MBUS_PARALLEL;
	}

	mipi->csi2_cfg.total_rx_ch = 0;
	mipi->cmb_csi_cfg.total_rx_ch = 0;
	if (IS_FLAG(cfg->flags, V4L2_MBUS_CSI2_CHANNEL_0)) {
		mipi->csi2_cfg.total_rx_ch++;
		mipi->cmb_csi_cfg.total_rx_ch++;
	}
	if (IS_FLAG(cfg->flags, V4L2_MBUS_CSI2_CHANNEL_1)) {
		mipi->csi2_cfg.total_rx_ch++;
		mipi->cmb_csi_cfg.total_rx_ch++;
	}
	if (IS_FLAG(cfg->flags, V4L2_MBUS_CSI2_CHANNEL_2)) {
		mipi->csi2_cfg.total_rx_ch++;
		mipi->cmb_csi_cfg.total_rx_ch++;
	}
	if (IS_FLAG(cfg->flags, V4L2_MBUS_CSI2_CHANNEL_3)) {
		mipi->csi2_cfg.total_rx_ch++;
		mipi->cmb_csi_cfg.total_rx_ch++;
	}

	return 0;
}

static const struct v4l2_subdev_video_ops sunxi_mipi_subdev_video_ops = {
	.s_stream = sunxi_mipi_subdev_s_stream,
	.s_mbus_config = sunxi_mipi_s_mbus_config,
};

static const struct v4l2_subdev_pad_ops sunxi_mipi_subdev_pad_ops = {
	.get_fmt = sunxi_mipi_subdev_get_fmt,
	.set_fmt = sunxi_mipi_subdev_set_fmt,
};

static struct v4l2_subdev_ops sunxi_mipi_subdev_ops = {
	.video = &sunxi_mipi_subdev_video_ops,
	.pad = &sunxi_mipi_subdev_pad_ops,
};

static int __mipi_init_subdev(struct mipi_dev *mipi)
{
	struct v4l2_subdev *sd = &mipi->subdev;
	int ret;

	mutex_init(&mipi->subdev_lock);
	v4l2_subdev_init(sd, &sunxi_mipi_subdev_ops);
	sd->grp_id = VIN_GRP_ID_MIPI;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(sd->name, sizeof(sd->name), "sunxi_mipi.%u", mipi->id);
	v4l2_set_subdevdata(sd, mipi);

	/*sd->entity->ops = &isp_media_ops;*/
	mipi->mipi_pads[MIPI_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	mipi->mipi_pads[MIPI_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_IO_V4L;

	ret = media_entity_pads_init(&sd->entity, MIPI_PAD_NUM, mipi->mipi_pads);
	if (ret < 0)
		return ret;

	return 0;
}

#ifdef MIPI_IRQ_USE
#if defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
static irqreturn_t mipi_isr(int irq, void *priv)
{
	struct mipi_dev *mipi = (struct mipi_dev *)priv;
	struct mipi_int_status status[VIN_MAX_MIPI][MAX_MIPI_CH];
	unsigned int i, j;
	unsigned long flags;

	if (!mipi->subdev.entity.stream_count) {
		return IRQ_HANDLED;
	}

	for (i = 0; i < VIN_MAX_MIPI; i++) {
		for (j = 0; j < MAX_MIPI_CH; j++) {
			memset(&status[i][j], 0, sizeof(struct mipi_int_status));
			cmb_port_int_status_get(i, j, &status[i][j]);
		}
	}

	spin_lock_irqsave(&mipi->slock, flags);

	for (i = 0; i < VIN_MAX_MIPI; i++) {
		for (j = 0; j < MAX_MIPI_CH; j++) {
			if (status[i][j].mask) {
				if (status[i][j].frame_sync_err) {
					cmb_port_int_clear_status(i, j, MIPI_FRAME_SYNC_ERR);
					vin_err("mipi%d vc%d frame sync err\n", i, j);
				}
				if (status[i][j].line_sync_err) {
					cmb_port_int_clear_status(i, j, MIPI_LINE_SYNC_ERR);
					vin_err("mipi%d vc%d line sync err\n", i, j);
				}
				if (status[i][j].ecc_warn) {
					cmb_port_int_clear_status(i, j, MIPI_ECC_WRN);
					vin_err("mipi%d vc%d ecc warn\n", i, j);
				}
				if (status[i][j].ecc_err) {
					cmb_port_int_clear_status(i, j, MIPI_ECC_ERR);
					vin_err("mipi%d vc%d ecc err\n", i, j);
				}
				if (status[i][j].ecc_err) {
					cmb_port_int_clear_status(i, j, MIPI_CHKSUM_ERR);
					vin_err("mipi%d vc%d checksum err\n", i, j);
				}
				if (status[i][j].ecc_err) {
					cmb_port_int_clear_status(i, j, MIPI_EOT_ERR);
					vin_err("mipi%d vc%d EOT err\n", i, j);
				}
#if 0
				if (status[i][j].frame_start_sync) {
					cmb_port_int_clear_status(i, j, MIPI_FRAME_START_SYNC);
					vin_print("mipi%d vc%d frame start\n", i, j);
				}
				if (status[i][j].frame_end_sync) {
					cmb_port_int_clear_status(i, j, MIPI_FRAME_END_SYNC);
					vin_print("mipi%d vc%d frame end\n", i, j);
				}
#endif
			}
		}
	}

	spin_unlock_irqrestore(&mipi->slock, flags);

	return IRQ_HANDLED;
}
#endif
#endif

static int mipi_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mipi_dev *mipi = NULL;
	int ret = 0;

	if (np == NULL) {
		vin_err("MIPI failed to get of node\n");
		return -ENODEV;
	}

	mipi = kzalloc(sizeof(struct mipi_dev), GFP_KERNEL);
	if (!mipi) {
		ret = -ENOMEM;
		goto ekzalloc;
	}

	of_property_read_u32(np, "device_id", &pdev->id);
	if (pdev->id < 0) {
		vin_err("MIPI failed to get device id\n");
		ret = -EINVAL;
		goto freedev;
	}

	mipi->base = of_iomap(np, 0);
	if (!mipi->base) {
		ret = -EIO;
		goto freedev;
	}
	mipi->id = pdev->id;
	mipi->pdev = pdev;

#if defined CONFIG_ARCH_SUN8IW16P1
	cmb_rx_set_base_addr(mipi->id, (unsigned long)mipi->base);
#elif defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
	cmb_csi_set_phy_base_addr(mipi->id, (unsigned long)mipi->base);
	mipi->port_base = of_iomap(np, 1);
	if (!mipi->port_base) {
		ret = -EIO;
		goto freedev;
	}
	cmb_csi_set_port_base_addr(mipi->id, (unsigned long)mipi->port_base);
#else
	bsp_mipi_csi_set_base_addr(mipi->id, (unsigned long)mipi->base);
	bsp_mipi_dphy_set_base_addr(mipi->id, (unsigned long)mipi->base + 0x1000);
#endif

	ret = __mipi_init_subdev(mipi);
	if (ret < 0) {
		vin_err("mipi init error!\n");
		goto unmap;
	}

#ifdef MIPI_IRQ_USE
#if defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
	if (mipi->id == 0) {
		mipi->irq = irq_of_parse_and_map(np, 0);
		if (mipi->irq <= 0) {
			vin_err("failed to get mipi IRQ resource\n");
			goto unmap;
		}
		ret = request_irq(mipi->irq, mipi_isr, IRQF_SHARED, mipi->pdev->name, mipi);
		if (ret) {
			vin_err("mipi%d request irq failed\n", mipi->id);
			goto unmap;
		}
	}
#endif
#endif

	spin_lock_init(&mipi->slock);

	platform_set_drvdata(pdev, mipi);
	glb_mipi[mipi->id] = mipi;
	ret = device_create_file(&pdev->dev, &dev_attr_settle_time);
	if (ret) {
		vin_err("mipi settle time node register fail\n");
		return ret;
	}
	vin_log(VIN_LOG_MIPI, "mipi%d probe end!\n", mipi->id);
	return 0;

unmap:
	iounmap(mipi->base);
freedev:
	kfree(mipi);
ekzalloc:
	vin_err("mipi probe err!\n");
	return ret;
}

static int mipi_remove(struct platform_device *pdev)
{
	struct mipi_dev *mipi = platform_get_drvdata(pdev);
	struct v4l2_subdev *sd = &mipi->subdev;

	platform_set_drvdata(pdev, NULL);
	v4l2_set_subdevdata(sd, NULL);
#if defined CONFIG_ARCH_SUN8IW21P1
	__mcsi_pin_release(mipi);
#endif
	if (mipi->base)
		iounmap(mipi->base);
	if (mipi->irq > 0)
		free_irq(mipi->irq, mipi);
	media_entity_cleanup(&mipi->subdev.entity);
	kfree(mipi);
	device_remove_file(&pdev->dev, &dev_attr_settle_time);
	return 0;
}

static size_t phy_common_status_dump(char *buf, size_t size)
{
	size_t count = 0;

#if defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
	int cur_phy_link;

	cmb_phy_link_mode_get(&cur_phy_link);
	switch (cur_phy_link) {
	case TWO_2LANE:
		count += scnprintf(buf + count, size - count,
				"phy_link_mode:\t 0, (2x2-lane links)\n");
		break;
	case ONE_4LANE:
		count += scnprintf(buf + count, size - count,
				"phy_link_mode:\t 1, (1x4-lane link)\n");
		break;
	default:
		count += scnprintf(buf + count, size - count,
				"phy_link_mode:\t 0x%x (unrecognized link mode)\n", cur_phy_link);
		break;
	}
	count += scnprintf(buf + count, size - count, "\n");
#endif

	return count;
}

static size_t phy_digital_status_dump(struct mipi_dev *mipi, char *buf, size_t size)
{
	size_t count = 0;

#if defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
	struct vin_md *vind = dev_get_drvdata(mipi->subdev.v4l2_dev->dev);
	unsigned int phy_en_status;
	unsigned int phy_lanedt_en_status;
	unsigned int phy_laneck_en_status;
	unsigned int phy_deskew_laneck0;
	unsigned int phy0_s2p_dly;
	unsigned int dphy_clk;
	unsigned int phy_freq_cnt;
	unsigned int mipi_bps;
	unsigned int phy_sta_d0;
	unsigned int phy_sta_d1;
	unsigned int phy_sta_ck0;

	count += scnprintf(buf + count, size - count, "[mipi%u]\n", mipi->id);

	// 0x0000: PHY Control Register (PHY_EN, PHY_LANEDT_EN, PHY_LANECK_EN)
	phy_en_status = cmb_phy_en_status_get(mipi->id);
	count += scnprintf(buf + count, size - count, "phy_en:\t\t %s\n",
					phy_en_status ? "enabled" : "disabled");
	if (!phy_en_status) {
		count += scnprintf(buf + count, size - count, "\n");
		return count;
	}

	phy_lanedt_en_status = cmb_phy_lanedt_en_status_get(mipi->id);
	switch (phy_lanedt_en_status) {
	case 0:
		count += scnprintf(buf + count, size - count, "data_lane_en:\t disabled\n");
		break;
	case DT_1LANE:
		count += scnprintf(buf + count, size - count,
					"data_lane_en:\t 0x%x (lane0 enabled)\n", phy_lanedt_en_status);
		break;
	case DT_2LANE:
		count += scnprintf(buf + count, size - count,
					"data_lane_en:\t 0x%x (lane0/1 enabled)\n", phy_lanedt_en_status);
		break;
	default:
		count += scnprintf(buf + count, size - count,
					"data_lane_en:\t 0x%x (invalid value)\n", phy_lanedt_en_status);
		break;
	}

	phy_laneck_en_status = cmb_phy_laneck_en_status_get(mipi->id);
	count += scnprintf(buf + count, size - count,
			"clk_lane_en:\t %s\n", phy_laneck_en_status ? "enabled" : "disabled");

	// 0x0018: deskew
	phy_deskew_laneck0 = cmb_phy_deskew1_cfg_get(mipi->id);
	count += scnprintf(buf + count, size - count, "deskew:\t\t 0x%x\n", phy_deskew_laneck0);

	// 0x0028: settle_time
	phy0_s2p_dly = cmb_phy0_s2p_dly_get(mipi->id);
	count += scnprintf(buf + count, size - count, "settle_time:\t 0x%x\n", phy0_s2p_dly);

	// 0x0034: PHY Frequency Counter Register (mipi_bps: Mbps)
	dphy_clk = clk_get_rate(vind->clk[VIN_TOP_CLK].clock) / 1000000;
	phy_freq_cnt = cmb_phy_freq_cnt_get(mipi->id);
	mipi_bps = 1000 * dphy_clk * 8 / phy_freq_cnt;
	count += scnprintf(buf + count, size - count, "mipi_bps:\t %u Mbps\n", mipi_bps);

	// 0x00f0: PHY Debug Register (PHY clock lane0 status, PHYC data lane0/1 status)
	phy_sta_d0 = cmb_phy_sta_d0_get(mipi->id);
	if (0x5 == phy_sta_d0)
		count += scnprintf(buf + count, size - count, "data_lane0:\t 0x%x (HS_S, ok)\n", phy_sta_d0);
	else if (0x3 == phy_sta_d0)
		count += scnprintf(buf + count, size - count, "data_lane0:\t 0x%x (LP11S, ok)\n", phy_sta_d0);
	else
		count += scnprintf(buf + count, size - count, "data_lane0:\t 0x%x (error)\n", phy_sta_d0);

	phy_sta_d1 = cmb_phy_sta_d1_get(mipi->id);
	if (0x5 == phy_sta_d1)
		count += scnprintf(buf + count, size - count, "data_lane1:\t 0x%x (HS_S, ok)\n", phy_sta_d1);
	else if (0x3 == phy_sta_d1)
		count += scnprintf(buf + count, size - count, "data_lane1:\t 0x%x (LP11S, ok)\n", phy_sta_d1);
	else
		count += scnprintf(buf + count, size - count, "data_lane1:\t 0x%x (error)\n", phy_sta_d1);

	phy_sta_ck0 = cmb_phy_sta_ck0_get(mipi->id);
	if ((mipi->id & 1) && cmb_port_lane_num_get(mipi->id - 1) == 4) {
		if (0x0 == phy_sta_ck0) {
			count += scnprintf(buf + count, size - count,
				"clk_lane:\t 0x%x (mipi%d used for 4_lane mode, clk_lane is the same with mipi%d)\n",
				phy_sta_ck0, mipi->id, mipi->id - 1);
		} else {
			count += scnprintf(buf + count, size - count, "clk_lane:\t 0x%x (error)\n", phy_sta_ck0);
		}
	} else {
		if (0x5 == phy_sta_ck0)
			count += scnprintf(buf + count, size - count, "clk_lane:\t 0x%x (HS_S, ok)\n", phy_sta_ck0);
		else if (0x3 == phy_sta_ck0)
			count += scnprintf(buf + count, size - count, "clk_lane:\t 0x%x (LP11S, ok)\n", phy_sta_ck0);
		else
			count += scnprintf(buf + count, size - count, "clk_lane:\t 0x%x (error)\n", phy_sta_ck0);
	}

	count += scnprintf(buf + count, size - count, "\n");
#endif

	return count;
}

static size_t port_payload_status_dump(struct mipi_dev *mipi, char *buf, size_t size)
{
	size_t count = 0;

#if defined CONFIG_ARCH_SUN50IW10P1 || defined CONFIG_ARCH_SUN8IW21P1
	int ch_i = 0;
	unsigned int port_en_status;
	unsigned int port_lane_num;
	unsigned int port_channel_num;
	unsigned int port_out_num;
	unsigned int port_wdr_mode;
	unsigned int unpack_enable;
	unsigned int yuv_seq;
	unsigned int mipi_ch_field;
	unsigned int int_pending_status;
	char int_pd_err_desc[256] = "";
	char int_pd_arr[13][64] = {
		"[Packet Header update interrupt lost]",
		"[Packet Footer detected interrupt lost]",
		"[Frame Start synchronization interrupt lost]",
		"[Frame End synchronization interrupt lost]",
		"[Line Start synchronization interrupt]",
		"[Line End synchronization interrupt]",
		"[Embedded data interrupt]",
		"[Frame synchronization error]",
		"[Line synchronization error]",
		"[ECC warning]",
		"[ECC error]",
		"[Checksum error]"
	};
	int int_pd_i = 0;
	unsigned int mipi_ch_cur_dt;
	char mipi_ch_cur_dt_desc[32] = "";
	unsigned int mipi_ch_cur_vc;

	count += scnprintf(buf + count, size - count, "[mipi%u]\n", mipi->id);

	// 0x0000: PORT Control Register (port_en, port_lane_num, port_channel_num, port_out_num)
	port_en_status = cmb_port_en_status_get(mipi->id);
	count += scnprintf(buf + count, size - count, "port_en:\t %s\n",
					port_en_status ? "enabled" : "disabled");
	if (!port_en_status) {
		count += scnprintf(buf + count, size - count, "\n");
		return count;
	}

	port_lane_num = cmb_port_lane_num_get(mipi->id);
	count += scnprintf(buf + count, size - count, "lane_num:\t %d lane\n", port_lane_num);

	port_channel_num = cmb_port_channel_num_get(mipi->id);
	count += scnprintf(buf + count, size - count, "channel_num:\t %d %s\n",
					port_channel_num + 1, port_channel_num ? "channels" : "channel");

	port_out_num = cmb_port_out_num_get(mipi->id);
	count += scnprintf(buf + count, size - count, "out_data_num:\t %s\n",
					port_out_num ? "2 data" : "1 data");

	// 0x000c: PORT WDR Mode Register
	port_wdr_mode = cmb_port_wdr_mode_get(mipi->id);
	switch (port_wdr_mode) {
	case 0:
		count += scnprintf(buf + count, size - count, "wdr_mode:\t normal\n");
		break;
	case 2:
		count += scnprintf(buf + count, size - count, "wdr_mode:\t WDR\n");
		break;
	default:
		count += scnprintf(buf + count, size - count, "wdr_mode:\t reserved\n");
		break;
	}

	// 0x0100: PORT MIPI Configuration Register (unpack_en, yuv_seq)
	unpack_enable = cmb_port_mipi_unpack_en_status_get(mipi->id);
	count += scnprintf(buf + count, size - count, "unpack_en:\t %s\n",
				unpack_enable ? "enabled" : "disabled");
	yuv_seq = cmb_port_mipi_yuv_seq_get(mipi->id);
	switch (yuv_seq) {
	case YUYV:
		count += scnprintf(buf + count, size - count, "yuv_seq:\t YUYV\n");
		break;
	case YVYU:
		count += scnprintf(buf + count, size - count, "yuv_seq:\t YVYU\n");
		break;
	case UYVY:
		count += scnprintf(buf + count, size - count, "yuv_seq:\t UYVY\n");
		break;
	case VYUY:
		count += scnprintf(buf + count, size - count, "yuv_seq:\t VYUY\n");
		break;
	default:
		break;
	}

	for (ch_i = 0; ch_i <= port_channel_num; ch_i++) {
		if (port_channel_num > 0)
			count += scnprintf(buf + count, size - count, "[mipi%u_channel%u]\n", mipi->id, ch_i);

		// 0x0110: PORT MIPI Channel0 Data Type Trigger Register (mipi_src_is_filed)
		mipi_ch_field = cmb_port_mipi_ch_field_get(mipi->id, ch_i);
		count += scnprintf(buf + count, size - count, "ch_field:\t %s\n",
					mipi_ch_field ? "interlaced" : "progressive");

		// 0x0114: PORT MIPI Channel0 interrupt Enable Register

		// 0x0118: PORT MIPI Channel0 interrupt Pending Register
		int_pending_status = cmb_port_int_status_get(mipi->id, ch_i, NULL);
		cmb_port_int_clear_status(mipi->id, ch_i, MIPI_INT_ALL);

		if (0xf == int_pending_status || 0x4f == int_pending_status
			|| 0x3f == int_pending_status || 0x7f == int_pending_status) {
			count += scnprintf(buf + count, size - count,
				"int_pending:\t 0x%x (ok)\n", int_pending_status);
		} else {
			count += scnprintf(buf + count, size - count,
				"int_pending:\t 0x%x (error), error details as follow:\n", int_pending_status);

			// check necessary interrupt flag
			for (int_pd_i = 0; int_pd_i <= 3; int_pd_i++) {
				if (!(int_pending_status & (1 << int_pd_i)))
					strcat(int_pd_err_desc, int_pd_arr[int_pd_i]);
			}
			if (strcmp(int_pd_err_desc, "")) {
				count += scnprintf(buf + count, size - count, "\t\t |------ %s\n", int_pd_err_desc);
				memset(int_pd_err_desc, 0, sizeof(int_pd_err_desc));
			}

			// check interrupt flag error
			for (int_pd_i = 4; int_pd_i <= 12; int_pd_i++) {
				if (int_pending_status & (1 << int_pd_i))
					strcat(int_pd_err_desc, int_pd_arr[int_pd_i]);
			}
			if (strcmp(int_pd_err_desc, ""))
				count += scnprintf(buf + count, size - count, "\t\t |------ %s\n", int_pd_err_desc);
		}

		// 0x011c: PORT MIPI Channel0 Packet Header Register (port_mipi_cur_dt, mipi_cur_vc)
		mipi_ch_cur_dt = cmb_port_mipi_ch_cur_dt_get(mipi->id, ch_i);
		switch (mipi_ch_cur_dt) {
		case MIPI_FS:
			strcpy(mipi_ch_cur_dt_desc, "Frame Start");
			break;
		case MIPI_FE:
			strcpy(mipi_ch_cur_dt_desc, "Frame End");
			break;
		case MIPI_LS:
			strcpy(mipi_ch_cur_dt_desc, "Line Start");
			break;
		case MIPI_LE:
			strcpy(mipi_ch_cur_dt_desc, "Line End");
			break;
		case MIPI_SDAT0:
		case MIPI_SDAT1:
		case MIPI_SDAT2:
		case MIPI_SDAT3:
		case MIPI_SDAT4:
		case MIPI_SDAT5:
		case MIPI_SDAT6:
		case MIPI_SDAT7:
			strcpy(mipi_ch_cur_dt_desc, "Special Data Type");
			break;

		case MIPI_BLK:
			strcpy(mipi_ch_cur_dt_desc, "Blanking");
			break;

		case MIPI_EMBD:
			strcpy(mipi_ch_cur_dt_desc, "Embedded");
			break;
		case MIPI_YUV420:
			strcpy(mipi_ch_cur_dt_desc, "YUV420 8-bit");
			break;
		case MIPI_YUV420_10:
			strcpy(mipi_ch_cur_dt_desc, "YUV420 10-bit");
			break;
		case MIPI_YUV420_CSP:
			strcpy(mipi_ch_cur_dt_desc, "YUV420 8-bit (CSP)");
			break;
		case MIPI_YUV420_CSP_10:
			strcpy(mipi_ch_cur_dt_desc, "YUV420 10-bit (CSP)");
			break;
		case MIPI_YUV422:
			strcpy(mipi_ch_cur_dt_desc, "YUV422 8-bit");
			break;
		case MIPI_YUV422_10:
			strcpy(mipi_ch_cur_dt_desc, "YUV422 10-bit");
			break;
		case MIPI_RGB565:
			strcpy(mipi_ch_cur_dt_desc, "RGB565");
			break;
		case MIPI_RGB888:
			strcpy(mipi_ch_cur_dt_desc, "RGB888");
			break;
		case MIPI_RAW8:
			strcpy(mipi_ch_cur_dt_desc, "RAW8");
			break;
		case MIPI_RAW10:
			strcpy(mipi_ch_cur_dt_desc, "RAW10");
			break;
		case MIPI_RAW12:
			strcpy(mipi_ch_cur_dt_desc, "RAW12");
			break;
		case MIPI_USR_DAT0:
		case MIPI_USR_DAT1:
		case MIPI_USR_DAT2:
		case MIPI_USR_DAT3:
		case MIPI_USR_DAT4:
		case MIPI_USR_DAT5:
		case MIPI_USR_DAT6:
		case MIPI_USR_DAT7:
			strcpy(mipi_ch_cur_dt_desc, "User Data Type");
			break;
		default:
			strcpy(mipi_ch_cur_dt_desc, "Unrecognized Data Type");
			break;
		}

		count += scnprintf(buf + count, size - count, "cur_data_type:\t 0x%x (%s)\n",
					mipi_ch_cur_dt, mipi_ch_cur_dt_desc);

		mipi_ch_cur_vc = cmb_port_mipi_ch_cur_vc_get(mipi->id, ch_i);
		count += scnprintf(buf + count, size - count, "vir_ch:\t\t %u\n", mipi_ch_cur_vc);
	}

	count += scnprintf(buf + count, size - count, "\n");
#endif

	return count;
}

static int mipi_debugfs_open(struct inode *inode, struct file *file)
{
	struct mipi_debugfs_buffer *buf;
	struct mipi_dev *mipi;
	int i = 0;

	buf = kmalloc(sizeof(*buf), GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	/* ensure mipi has stream on */
	for (i = 0; i < VIN_MAX_MIPI; i++) {
		mipi = glb_mipi[i];
		if (mipi->subdev.entity.stream_count)
			break;
	}
	if (i >= VIN_MAX_MIPI) {
		vin_warn("No mipi stream on, can not dump any mipi register, please ensure mipi has stream on.\n");

		buf->count = 0;
		file->private_data = buf;
		return 0;
	}

	mipi_status_size_sum = 0;

	mipi_status_size_sum += scnprintf(buf->data + mipi_status_size_sum,
							sizeof(buf->data) - mipi_status_size_sum,
							"*************** MIPI Register Status List **************\n");

	/* PHY COMMON Register (PHY Top Control Register) */
	mipi_status_size_sum += scnprintf(buf->data + mipi_status_size_sum,
							sizeof(buf->data) - mipi_status_size_sum,
							"------------------ PHY COMMON Register -----------------\n");
	mipi_status_size_sum += phy_common_status_dump(buf->data + mipi_status_size_sum,
					sizeof(buf->data) - mipi_status_size_sum);

	/* PHY Digital Layer Register */
	mipi_status_size_sum += scnprintf(buf->data + mipi_status_size_sum,
							sizeof(buf->data) - mipi_status_size_sum,
							"-------------- PHY Digital Layer Register --------------\n");
	for (i = 0; i < VIN_MAX_MIPI; i++) {
		mipi = glb_mipi[i];
		if (mipi != NULL) {
			mipi_status_size[i] = phy_digital_status_dump(mipi, buf->data + mipi_status_size_sum,
						sizeof(buf->data) - mipi_status_size_sum);
			mipi_status_size_sum += mipi_status_size[i];
		}
	}

	/* PORT Payload Layer Register*/
	mipi_status_size_sum += scnprintf(buf->data + mipi_status_size_sum,
							sizeof(buf->data) - mipi_status_size_sum,
							"-------------- PORT Payload Layer Register -------------\n");
	for (i = 0; i < VIN_MAX_MIPI; i++) {
		mipi = glb_mipi[i];
		if (mipi != NULL) {
			mipi_status_size[i] = port_payload_status_dump(mipi, buf->data + mipi_status_size_sum,
						sizeof(buf->data) - mipi_status_size_sum);
			mipi_status_size_sum += mipi_status_size[i];
		}
	}

	mipi_status_size_sum += scnprintf(buf->data + mipi_status_size_sum,
							sizeof(buf->data) - mipi_status_size_sum,
							"********************************************************\n\n");

	buf->count = mipi_status_size_sum;
	file->private_data = buf;

	return 0;
}

static ssize_t mipi_debugfs_read(struct file *file, char __user *user_buf,
				      size_t nbytes, loff_t *ppos)
{
	struct mipi_debugfs_buffer *buf = file->private_data;

	return simple_read_from_buffer(user_buf, nbytes, ppos, buf->data,
				       buf->count);
}

static int mipi_debugfs_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	file->private_data = NULL;

	return 0;
}

static const struct file_operations mipi_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = mipi_debugfs_open,
	.llseek = no_llseek,
	.read = mipi_debugfs_read,
	.release = mipi_debugfs_release,
};

#if defined CONFIG_DEBUG_FS
int sunxi_mipi_debug_register_driver(void)
{
#if defined CONFIG_SUNXI_MPP
	mipi_debugfs_root = debugfs_mpp_root;
#else
	mipi_debugfs_root = vi_debugfs_root;
#endif

	if (NULL == mipi_debugfs_root) {
		vin_err("debugfs_mpp_root is NULL!\n");
		return -ENOENT;
	}

	mipi_node = debugfs_create_file("mipi", 0444, mipi_debugfs_root,
				   NULL, &mipi_debugfs_fops);
	if (IS_ERR_OR_NULL(mipi_node)) {
		vin_err("Unable to create mipi debugfs status file.\n");
		mipi_debugfs_root = NULL;
		return -ENODEV;
	}

	return 0;
}
#else
int sunxi_mipi_debug_register_driver(void)
{
	return 0;
}
#endif

#if defined CONFIG_DEBUG_FS
void sunxi_mipi_debug_unregister_driver(void)
{
	if (mipi_node == NULL)
		return;
	debugfs_remove_recursive(mipi_node);
	mipi_debugfs_root = NULL;
	mipi_node = NULL;
}
#else
void sunxi_mipi_debug_unregister_driver(void)
{
	return;
}
#endif

static const struct of_device_id sunxi_mipi_match[] = {
	{.compatible = "allwinner,sunxi-mipi",},
	{},
};

static struct platform_driver mipi_platform_driver = {
	.probe = mipi_probe,
	.remove = mipi_remove,
	.driver = {
		.name = MIPI_MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sunxi_mipi_match,
	}
};

void sunxi_combo_set_sync_code(struct v4l2_subdev *sd,
		struct combo_sync_code *sync)
{
	struct mipi_dev *combo = v4l2_get_subdevdata(sd);

	memset(&combo->sync_code, 0, sizeof(combo->sync_code));
	combo->sync_code = *sync;
}

void sunxi_combo_set_lane_map(struct v4l2_subdev *sd,
		struct combo_lane_map *map)
{
	struct mipi_dev *combo = v4l2_get_subdevdata(sd);

	memset(&combo->lvds_map, 0, sizeof(combo->lvds_map));
	combo->lvds_map = *map;
}

void sunxi_combo_wdr_config(struct v4l2_subdev *sd,
		struct combo_wdr_cfg *wdr)
{
	struct mipi_dev *combo = v4l2_get_subdevdata(sd);

	memset(&combo->wdr_cfg, 0, sizeof(combo->wdr_cfg));
	combo->wdr_cfg = *wdr;
}

struct v4l2_subdev *sunxi_mipi_get_subdev(int id)
{
	if (id < VIN_MAX_MIPI && glb_mipi[id])
		return &glb_mipi[id]->subdev;
	else
		return NULL;
}

int sunxi_mipi_platform_register(void)
{
	return platform_driver_register(&mipi_platform_driver);
}

void sunxi_mipi_platform_unregister(void)
{
	platform_driver_unregister(&mipi_platform_driver);
	vin_log(VIN_LOG_MIPI, "mipi_exit end\n");
}
