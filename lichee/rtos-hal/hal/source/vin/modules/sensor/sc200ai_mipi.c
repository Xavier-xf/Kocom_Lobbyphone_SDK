/*
 * A V4L2 driver for Raw cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Zhao Wei <zhaowei@allwinnertech.com>
 *    Liang WeiJie <liangweijie@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <hal_timer.h>

#include "../../vin_mipi/combo_common.h"
#include "camera.h"
#include "../../utility/sunxi_camera_v2.h"
#include "../../utility/media-bus-format.h"
#include "../../utility/vin_supply.h"

#define MCLK              (27*1000*1000)
#define V4L2_IDENT_SENSOR 0xcb1c

/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0x3107
#define ID_REG_LOW		0x3108
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)
#define SENSOR_FRAME_RATE 30

#define HDR_RATIO 8

/*
 * The SC200AI i2c address
 */
#define I2C_ADDR 0x60

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc200ai_mipi"
#define SENSOR_NAME_2 "sc200ai_mipi_2"

static int sensor_power_count[2];
static int sensor_stream_count[2];
static struct sensor_format_struct *current_win[2];
static struct sensor_format_struct *current_switch_win[2];

#ifdef CONFIG_SENSOR_SC200AI_8BIT_MIPI
#define RAW8 1 //raw8 select
#else
#define RAW8 0 //raw10 select
#endif

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

#if defined CONFIG_ISP_READ_THRESHOLD || defined CONFIG_ISP_ONLY_HARD_LIGHTADC//FULL_SIZE
//24M 30fps normal
static struct regval_list sensor_1080p10_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x320e, 0x0d},  //vts=3375
	{0x320f, 0x2f},
#if RAW8
	{0x301f, 0x82},
	{0x3031, 0x08},
	{0x3037, 0x00},
#else// RAW10
	{0x301f, 0x7f},
#endif
	{0x3243, 0x01},
	{0x3248, 0x02},
	{0x3249, 0x09},
	{0x3253, 0x08},
	{0x3271, 0x0a},
	{0x3301, 0x20},
	{0x3304, 0x40},
	{0x3306, 0x32},
	{0x330b, 0x88},
	{0x330f, 0x02},
	{0x331e, 0x39},
	{0x3333, 0x10},
	{0x3621, 0xe8},
	{0x3622, 0x16},
	{0x3637, 0x1b},
	{0x363a, 0x1f},
	{0x363b, 0xc6},
	{0x363c, 0x0e},
	{0x3670, 0x0a},
	{0x3674, 0x82},
	{0x3675, 0x76},
	{0x3676, 0x78},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x33},
	{0x3692, 0x44},
	{0x369c, 0x40},
	{0x369d, 0x48},
#if RAW8
	{0x36ea, 0x35},
	{0x36eb, 0x0b},
	{0x36ec, 0x1a},
	{0x36ed, 0x14},
	{0x36fa, 0x35},
	{0x36fb, 0x00},
	{0x36fc, 0x10},
	{0x36fd, 0x17},
#endif
	{0x3901, 0x02},
	{0x3904, 0x04},

#if RAW8
#ifdef BLC_8// RAW8-blc_8
	{0x3908, 0x20},
#else// RAW8-blc_16
	{0x3908, 0x41},
#endif
#else// RAW10-blc_65
	{0x3908, 0x41},
#endif

#if RAW8
	{0x391d, 0x24},
#else// RAW10
	{0x391d, 0x14},
#endif
	{0x391f, 0x18},
	{0x3e01, 0x8c},
	{0x3e02, 0x20},
	{0x3e16, 0x00},
	{0x3e17, 0x80},
	{0x3f09, 0x48},
#if !RAW8
	{0x4800, 0x44},// RAW10
#endif
	{0x5787, 0x10},
	{0x5788, 0x06},
	{0x578a, 0x10},
	{0x578b, 0x06},
	{0x5790, 0x10},
	{0x5791, 0x10},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x10},
	{0x5795, 0x00},
	{0x5799, 0x00},
	{0x57c7, 0x10},
	{0x57c8, 0x06},
	{0x57ca, 0x10},
	{0x57cb, 0x06},
	{0x57d1, 0x10},
	{0x57d4, 0x10},
	{0x57d9, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x59ee, 0xa0},
	{0x59ef, 0x08},
	{0x59f4, 0x18},
	{0x59f5, 0x10},
	{0x59f6, 0x0c},
	{0x59f7, 0x10},
	{0x59f8, 0x06},
	{0x59f9, 0x02},
	{0x59fa, 0x18},
	{0x59fb, 0x10},
	{0x59fc, 0x0c},
	{0x59fd, 0x10},
	{0x59fe, 0x04},
	{0x59ff, 0x02},
#if RAW8
	{0x36e9, 0x54},
	{0x36f9, 0x53},
#else// RAW10
	{0x36e9, 0x20},
	{0x36f9, 0x27},
#endif
	{0x0100, 0x01},
};

//24M 30fps normal
static struct regval_list sensor_1080p20_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x320e, 0x06},  //vts=1687
	{0x320f, 0x97},
#if RAW8
	{0x301f, 0x82},
	{0x3031, 0x08},
	{0x3037, 0x00},
#else// RAW10
	{0x301f, 0x7f},
#endif
	{0x3243, 0x01},
	{0x3248, 0x02},
	{0x3249, 0x09},
	{0x3253, 0x08},
	{0x3271, 0x0a},
	{0x3301, 0x20},
	{0x3304, 0x40},
	{0x3306, 0x32},
	{0x330b, 0x88},
	{0x330f, 0x02},
	{0x331e, 0x39},
	{0x3333, 0x10},
	{0x3621, 0xe8},
	{0x3622, 0x16},
	{0x3637, 0x1b},
	{0x363a, 0x1f},
	{0x363b, 0xc6},
	{0x363c, 0x0e},
	{0x3670, 0x0a},
	{0x3674, 0x82},
	{0x3675, 0x76},
	{0x3676, 0x78},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x33},
	{0x3692, 0x44},
	{0x369c, 0x40},
	{0x369d, 0x48},
#if RAW8
	{0x36ea, 0x35},
	{0x36eb, 0x0b},
	{0x36ec, 0x1a},
	{0x36ed, 0x14},
	{0x36fa, 0x35},
	{0x36fb, 0x00},
	{0x36fc, 0x10},
	{0x36fd, 0x17},
#endif
	{0x3901, 0x02},
	{0x3904, 0x04},

#if RAW8
#ifdef BLC_8// RAW8-blc_8
	{0x3908, 0x20},
#else// RAW8-blc_16
	{0x3908, 0x41},
#endif
#else// RAW10-blc_65
	{0x3908, 0x41},
#endif

#if RAW8
	{0x391d, 0x24},
#else// RAW10
	{0x391d, 0x14},
#endif
	{0x391f, 0x18},
	{0x3e01, 0x8c},
	{0x3e02, 0x20},
	{0x3e16, 0x00},
	{0x3e17, 0x80},
	{0x3f09, 0x48},
#if !RAW8
	{0x4800, 0x44},// RAW10
#endif
	{0x5787, 0x10},
	{0x5788, 0x06},
	{0x578a, 0x10},
	{0x578b, 0x06},
	{0x5790, 0x10},
	{0x5791, 0x10},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x10},
	{0x5795, 0x00},
	{0x5799, 0x00},
	{0x57c7, 0x10},
	{0x57c8, 0x06},
	{0x57ca, 0x10},
	{0x57cb, 0x06},
	{0x57d1, 0x10},
	{0x57d4, 0x10},
	{0x57d9, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x59ee, 0xa0},
	{0x59ef, 0x08},
	{0x59f4, 0x18},
	{0x59f5, 0x10},
	{0x59f6, 0x0c},
	{0x59f7, 0x10},
	{0x59f8, 0x06},
	{0x59f9, 0x02},
	{0x59fa, 0x18},
	{0x59fb, 0x10},
	{0x59fc, 0x0c},
	{0x59fd, 0x10},
	{0x59fe, 0x04},
	{0x59ff, 0x02},
#if RAW8
	{0x36e9, 0x54},
	{0x36f9, 0x53},
#else// RAW10
	{0x36e9, 0x20},
	{0x36f9, 0x27},
#endif
	{0x0100, 0x01},
};

#else //CONFIG_ISP_FAST_CONVERGENCE || CONFIG_ISP_HARD_LIGHTADC
//24M 30fps normal
static struct regval_list sensor_1080p20_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	{0x320e, 0x06},  //vts=1687
	{0x320f, 0x97},
#if RAW8
	{0x301f, 0x82},
	{0x3031, 0x08},
	{0x3037, 0x00},
#else// RAW10
	{0x301f, 0x7f},
#endif
	{0x3243, 0x01},
	{0x3248, 0x02},
	{0x3249, 0x09},
	{0x3253, 0x08},
	{0x3271, 0x0a},
	{0x3301, 0x20},
	{0x3304, 0x40},
	{0x3306, 0x32},
	{0x330b, 0x88},
	{0x330f, 0x02},
	{0x331e, 0x39},
	{0x3333, 0x10},
	{0x3621, 0xe8},
	{0x3622, 0x16},
	{0x3637, 0x1b},
	{0x363a, 0x1f},
	{0x363b, 0xc6},
	{0x363c, 0x0e},
	{0x3670, 0x0a},
	{0x3674, 0x82},
	{0x3675, 0x76},
	{0x3676, 0x78},
	{0x367c, 0x48},
	{0x367d, 0x58},
	{0x3690, 0x34},
	{0x3691, 0x33},
	{0x3692, 0x44},
	{0x369c, 0x40},
	{0x369d, 0x48},
#if RAW8
	{0x36ea, 0x35},
	{0x36eb, 0x0b},
	{0x36ec, 0x1a},
	{0x36ed, 0x14},
	{0x36fa, 0x35},
	{0x36fb, 0x00},
	{0x36fc, 0x10},
	{0x36fd, 0x17},
#endif
	{0x3901, 0x02},
	{0x3904, 0x04},

#if RAW8
#ifdef BLC_8// RAW8-blc_8
	{0x3908, 0x20},
#else// RAW8-blc_16
	{0x3908, 0x41},
#endif
#else// RAW10-blc_65
	{0x3908, 0x41},
#endif

#if RAW8
	{0x391d, 0x24},
#else// RAW10
	{0x391d, 0x14},
#endif
	{0x391f, 0x18},
	{0x3e01, 0x8c},
	{0x3e02, 0x20},
	{0x3e16, 0x00},
	{0x3e17, 0x80},
	{0x3f09, 0x48},
#if !RAW8
	{0x4800, 0x44},// RAW10
#endif
	{0x5787, 0x10},
	{0x5788, 0x06},
	{0x578a, 0x10},
	{0x578b, 0x06},
	{0x5790, 0x10},
	{0x5791, 0x10},
	{0x5792, 0x00},
	{0x5793, 0x10},
	{0x5794, 0x10},
	{0x5795, 0x00},
	{0x5799, 0x00},
	{0x57c7, 0x10},
	{0x57c8, 0x06},
	{0x57ca, 0x10},
	{0x57cb, 0x06},
	{0x57d1, 0x10},
	{0x57d4, 0x10},
	{0x57d9, 0x00},
	{0x59e0, 0x60},
	{0x59e1, 0x08},
	{0x59e2, 0x3f},
	{0x59e3, 0x18},
	{0x59e4, 0x18},
	{0x59e5, 0x3f},
	{0x59e6, 0x06},
	{0x59e7, 0x02},
	{0x59e8, 0x38},
	{0x59e9, 0x10},
	{0x59ea, 0x0c},
	{0x59eb, 0x10},
	{0x59ec, 0x04},
	{0x59ed, 0x02},
	{0x59ee, 0xa0},
	{0x59ef, 0x08},
	{0x59f4, 0x18},
	{0x59f5, 0x10},
	{0x59f6, 0x0c},
	{0x59f7, 0x10},
	{0x59f8, 0x06},
	{0x59f9, 0x02},
	{0x59fa, 0x18},
	{0x59fb, 0x10},
	{0x59fc, 0x0c},
	{0x59fd, 0x10},
	{0x59fe, 0x04},
	{0x59ff, 0x02},
#if RAW8
	{0x36e9, 0x54},
	{0x36f9, 0x53},
#else// RAW10
	{0x36e9, 0x20},
	{0x36f9, 0x27},
#endif
	{0x0100, 0x01},
};

#endif
/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};


/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function , retrun -EINVAL
 */
#if 0
static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}
static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}
#endif

static int sensor_s_exp(int id, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;

	if (exp_val < 16)
		exp_val = 16;
	exphigh = (unsigned char) (0xf & (exp_val>>15));
	expmid = (unsigned char) (0xff & (exp_val>>7));
	explow = (unsigned char) (0xf0 & (exp_val<<1));

	sensor_write(id, 0x3e02, explow);
	sensor_write(id, 0x3e01, expmid);
	sensor_write(id, 0x3e00, exphigh);
	sensor_dbg("sensor_set_exp = %d line Done!\n", exp_val);

	return 0;
}

static int sensor_s_gain(int id, int gain_val)
{
	data_type gainlow = 0;
	data_type gainhigh = 0;
	data_type gaindiglow = 0x80;
	data_type gaindighigh = 0x00;

	int gainana = gain_val << 2;

	if (gainana < 2 * 64) {  // 2*64
		gainhigh = 0x03;
		gainlow = gainana;
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana <= 217) {// 3.4 * 64 =217.6
		gainhigh = 0x07;
		gainlow = gainana >> 1;
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana <= 435) { // 6.8 * 64=435.2
		gainhigh = 0x23;
		gainlow = gainana * 10 / (34 * 1);
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana <= 870) { // 13.6 * 64=870.4
		gainhigh = 0x27;
		gainlow = gainana * 10 / (34 * 2);
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana <= 1740) { // 27.2 * 64 =1740.8
		gainhigh = 0x2F;
		gainlow = gainana * 10 / (34 * 4);
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana < 3455) { // 53.975 * 64 =3454.4
		gainhigh = 0x3F;
		gainlow = gainana * 10 / (34 * 8);
		gaindighigh = 0x00;
		gaindiglow = 0x80;
	} else if (gainana < 2 * 3455) {//53.975 * 64 digital_gain:*2
		gainhigh = 0x3F;
		gainlow = 0x7F;
		gaindighigh = 0x00;
		gaindiglow = 128 * gainana * 10 / (34 * 8 * 1) / 127;

	} else {
		gainhigh = 0x3F;
		gainlow = 0x7F;
		gaindighigh = 0x00;
		gaindiglow = 0xFE;
	}

	sensor_write(id, 0x3e09, (unsigned char)gainlow);
	sensor_write(id, 0x3e08, (unsigned char)gainhigh);
	sensor_write(id, 0x3e07, (unsigned char)gaindiglow);
	sensor_write(id, 0x3e06, (unsigned char)gaindighigh);

	sensor_dbg("sensor_set_anagain = %d, 0x%x, 0x%x Done!\n", gain_val, gainhigh, gainlow);
	sensor_dbg("digital_gain = 0x%x, 0x%x Done!\n", gaindighigh, gaindiglow);

	return 0;
}

static int DPC_frame_cnt;
static int DPC_Flag = 1;
static int fps_change_flag;
static int shutter_delay = 1;
static int shutter_delay_cnt;
static int sc200ai_sensor_vts;
static int sensor_s_exp_gain(int id, struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	unsigned short R = 0;
	data_type R_high, R_low;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < (1 * 16)) {
		gain_val = 16;
	}

	if (exp_val > 0xfffff)
		exp_val = 0xfffff;
	if (fps_change_flag) {
		if (shutter_delay_cnt == shutter_delay) {
			shutter_delay_cnt = 0;
			fps_change_flag = 0;
		} else
			shutter_delay_cnt++;
	}

	/////////////////////////////////////////////////////
	/////high-temp DPC logic/////
	if (++DPC_frame_cnt >= 3) {
		DPC_frame_cnt = 0;
		sensor_read(id, 0x3974, &R_high);
		sensor_read(id, 0x3975, &R_low);
		R = (R_high << 8) | R_low;

		if (R > 0x2040 || ((gain_val / 16) >= 53)) {
			if (R > 0x2040) {
				if (DPC_Flag != 2) {
					sensor_write(id, 0x3812, 0x00);
					sensor_write(id, 0x5787, 0x00);
					sensor_write(id, 0x5788, 0x00);
					sensor_write(id, 0x5790, 0x00);
					sensor_write(id, 0x5791, 0x00);
					sensor_write(id, 0x5799, 0x07);
					sensor_write(id, 0x3812, 0x30);
					DPC_Flag = 2;
				}
			} else {
				if (DPC_Flag != 3) {
					sensor_write(id, 0x3812, 0x00);
					sensor_write(id, 0x5787, 0x10);
					sensor_write(id, 0x5788, 0x06);
					sensor_write(id, 0x5790, 0x10);
					sensor_write(id, 0x5791, 0x10);
					sensor_write(id, 0x5799, 0x07);
					sensor_write(id, 0x3812, 0x30);
					DPC_Flag = 3;
				}
			}
		} else if (R < 0x2030 && ((gain_val / 16) < 53) && 1 != DPC_Flag) {
			sensor_write(id, 0x3812, 0x00);
			sensor_write(id, 0x5787, 0x10);
			sensor_write(id, 0x5788, 0x06);
			sensor_write(id, 0x5790, 0x10);
			sensor_write(id, 0x5791, 0x10);
			sensor_write(id, 0x5799, 0x00);
			sensor_write(id, 0x3812, 0x30);
			DPC_Flag = 1;
		}
	}

	//sensor_write(sd, 0x3812, 0x00);
	sensor_s_exp(id, exp_val);
	sensor_s_gain(id, gain_val);
	//sensor_write(sd, 0x3812, 0x30);

	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	return 0;
}

#if 0
static int sensor_flip_status;
static int sensor_s_vflip(int id, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(id, 0x17, &get_value);
	sensor_dbg("ready to vflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x02;
		sensor_flip_status |= 0x02;
	} else {
		set_value = get_value & 0xFD;
		sensor_flip_status &= 0xFD;
	}
	sensor_write(id, 0x17, set_value);
	usleep_range(80000, 100000);
	sensor_read(id, 0x17, &get_value);
	sensor_dbg("after vflip, regs_data = 0x%x, sensor_flip_status = %d\n",
				get_value, sensor_flip_status);

	return 0;
}

static int sensor_s_hflip(int id, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(id, 0x17, &get_value);
	sensor_dbg("ready to hflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x01;
		sensor_flip_status |= 0x01;
	} else {
		set_value = get_value & 0xFE;
		sensor_flip_status &= 0xFE;
	}
	sensor_write(id, 0x17, set_value);
	usleep_range(80000, 100000);
	sensor_read(id, 0x17, &get_value);
	sensor_dbg("after hflip, regs_data = 0x%x, sensor_flip_status = %d\n",
				get_value, sensor_flip_status);

	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
//	struct sensor_info *info = to_state(sd);
//	data_type get_value = 0, check_value = 0;

//	sensor_read(sd, 0x17, &get_value);
//	check_value = get_value & 0x03;
//	check_value = sensor_flip_status & 0x3;
//	sensor_dbg("0x17 = 0x%x, check_value = 0x%x\n", get_value, check_value);

//	switch (check_value) {
//	case 0x00:
//		sensor_dbg("RGGB\n");
//		*code = MEDIA_BUS_FMT_SRGGB10_1X10;
//		break;
//	case 0x01:
//		sensor_dbg("GRBG\n");
//		*code = MEDIA_BUS_FMT_SGRBG10_1X10;
//		break;
//	case 0x02:
//		sensor_dbg("GBRG\n");
//		*code = MEDIA_BUS_FMT_SGBRG10_1X10;
//		break;
//	case 0x03:
//		sensor_dbg("BGGR\n");
//		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		break;
//	default:
//		 *code = info->fmt->mbus_code;
//	}
	*code = MEDIA_BUS_FMT_SRGGB10_1X10; // gc2053 support change the rgb format by itself

	return 0;
}
#endif

/*
* Stuff that knows about the sensor.
*/
static int sensor_power(int id, int on)
{
	int ret = 0;

	switch (on) {
	case PWR_ON:
		sensor_dbg("PWR_ON!\n");
		vin_gpio_set_status(id, PWDN, 1);
		vin_gpio_set_status(id, RESET, 1);
		//vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(id, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		hal_usleep(10000);
		vin_gpio_write(id, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(id, RESET, CSI_GPIO_HIGH);
		hal_usleep(10000);
		vin_set_mclk(id, 1);
		hal_usleep(10000);
		vin_set_mclk_freq(id, MCLK);
		hal_usleep(30000);

		break;
	case PWR_OFF:
		sensor_dbg("PWR_OFF!do nothing\n");
		hal_usleep(1000);
		vin_gpio_set_status(id, PWDN, 1);
		vin_gpio_set_status(id, RESET, 1);
		vin_gpio_write(id, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		vin_set_mclk(id, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_set_ir(int id, int status)
{
	vin_gpio_set_status(id, IR_CUT0, 1);
	// vin_gpio_set_status(id, IR_CUT1, 1);
	vin_gpio_set_status(id, IR_LED, 1);
	sensor_print("ir_cut status:%d\n", status);
	switch (status) {
	case IR_DAY:
		vin_gpio_write(id, IR_CUT0, CSI_GPIO_LOW);
		// vin_gpio_write(id, IR_CUT1, CSI_GPIO_HIGH);
		// vin_gpio_write(id, IR_CUT0, CSI_GPIO_LOW);
		// vin_gpio_write(id, IR_CUT1, CSI_GPIO_LOW);
		vin_gpio_write(id, IR_LED, CSI_GPIO_LOW);
		break;
	case IR_NIGHT:
		vin_gpio_write(id, IR_CUT0, CSI_GPIO_HIGH);
		// vin_gpio_write(id, IR_CUT1, CSI_GPIO_LOW);
		// vin_gpio_write(id, IR_CUT0, CSI_GPIO_LOW);
		// vin_gpio_write(id, IR_CUT1, CSI_GPIO_LOW);
		vin_gpio_write(id, IR_LED, CSI_GPIO_HIGH);
		break;
	default:
		return -1;
	}
	return 0;
}

#if 0
static int sensor_reset(int id, u32 val)
{

	sensor_dbg("%s: val=%d\n", __func__);
	switch (val) {
	case 0:
		vin_gpio_write(id, RESET, CSI_GPIO_HIGH);
		hal_usleep(1000);
		break;
	case 1:
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		hal_usleep(1000);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif

static int sensor_detect(int id)
{
	data_type rdval;
	/*int eRet;
	int times_out = 3;
	do {
		eRet = sensor_read(id, ID_REG_HIGH, &rdval);
		sensor_dbg("eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval, times_out);
		hal_usleep(200);
		times_out--;
	} while (eRet < 0  &&  times_out > 0);*/

	sensor_read(id, ID_REG_HIGH, &rdval);
	sensor_dbg("ID_VAL_HIGH = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;

	sensor_read(id, ID_REG_LOW, &rdval);
	sensor_dbg("ID_VAL_LOW = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	sensor_dbg("Done!\n");
	return 0;
}

static int sensor_init(int id)
{
	int ret;

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(id);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	return 0;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
#if defined CONFIG_ISP_READ_THRESHOLD || defined CONFIG_ISP_ONLY_HARD_LIGHTADC // FULL_SIZE
	{
#if RAW8
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR8_1X8,
#else//RAW10
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR10_1X10,
#endif
		.width 		= 1920,
		.height 	= 1080,
		.hoffset 	= 0,
		.voffset 	= 0,
		.hts 		= 2200,
		.vts 		= 3375,
		.pclk 		= 74250000,
#if RAW8
		.mipi_bps 	= 297000000,
#else//RAW10
		.mipi_bps 	= 371250000,
#endif
		.fps_fixed 	= 10,
		.bin_factor = 1,
		.intg_min 	= 1 << 4,
		.intg_max 	= (1125 - 8) << 4,
		.gain_min 	= 1 << 4,
		.gain_max 	= 1713 << 4,
		.regs 		= sensor_1080p10_regs,
		.regs_size 	= ARRAY_SIZE(sensor_1080p10_regs),
	},

	{
#if RAW8
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR8_1X8,
#else//RAW10
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR10_1X10,
#endif
		.width 		= 1920,
		.height 	= 1080,
		.hoffset 	= 0,
		.voffset 	= 0,
		.hts 		= 2200,
		.vts 		= 1687,
		.pclk 		= 74250000,
#if RAW8
		.mipi_bps 	= 297000000,
#else//RAW10
		.mipi_bps 	= 371250000,
#endif
		.fps_fixed 	= 20,
		.bin_factor = 1,
		.intg_min 	= 1 << 4,
		.intg_max 	= (1125 - 8) << 4,
		.gain_min 	= 1 << 4,
		.gain_max 	= 1713 << 4,
		.regs 		= sensor_1080p20_regs,
		.regs_size 	= ARRAY_SIZE(sensor_1080p20_regs),
	},

#else //CONFIG_ISP_FAST_CONVERGENCE || CONFIG_ISP_HARD_LIGHTADC
	{
#if RAW8
		.mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8,
#else//RAW10
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
#endif
		.width 		= 1920,
		.height 	= 1080,
		.hoffset 	= 0,
		.voffset 	= 0,
		.hts 		= 2200,
		.vts 		= 1687,
		.pclk 		= 74250000,
#if RAW8
		.mipi_bps 	= 297000000,
#else//RAW10
		.mipi_bps 	= 371250000,
#endif
		.fps_fixed 	= 20,
		.bin_factor = 1,
		.intg_min 	= 1 << 4,
		.intg_max 	= (1125 - 8) << 4,
		.gain_min	= 1 << 4,
		.gain_max 	= 1713 << 4,
		.regs 		= sensor_1080p20_regs,
		.regs_size 	= ARRAY_SIZE(sensor_1080p20_regs),
	}

	{
#if RAW8
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR8_1X8,
#else//RAW10
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR10_1X10,
#endif
		.width 		= 1920,
		.height 	= 1080,
		.hoffset 	= 0,
		.voffset 	= 0,
		.hts 		= 2200,
		.vts 		= 3375,
		.pclk 		= 74250000,
#if RAW8
		.mipi_bps 	= 297000000,
#else//RAW10
		.mipi_bps 	= 371250000,
#endif
		.fps_fixed 	= 10,
		.bin_factor = 1,
		.intg_min 	= 1 << 4,
		.intg_max 	= (1125 - 8) << 4,
		.gain_min 	= 1 << 4,
		.gain_max 	= 1713 << 4,
		.regs 		= sensor_1080p10_regs,
		.regs_size 	= ARRAY_SIZE(sensor_1080p10_regs),
	},
#endif
};

static struct sensor_format_struct *sensor_get_format(int id, int isp_id)
{
#if defined CONFIG_ISP_READ_THRESHOLD || defined CONFIG_ISP_ONLY_HARD_LIGHTADC
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);
	struct sensor_format_struct *sensor_format = NULL;
	int wdr_on = isp_get_cfg[ispid].sensor_wdr_on;
	int fps = isp_get_cfg[ispid].sensor_get_fps;
	int i;

	if (current_win[id])
		return current_win[id];

	for (i = 0; i < ARRAY_SIZE(sensor_formats); i++) {
		if (sensor_formats[i].wdr_mode == wdr_on) {
			if (sensor_formats[i].fps_fixed == fps) {
				sensor_format = &sensor_formats[i];
				sensor_print("fine wdr is %d, fine fps is %d\n", wdr_on, fps);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		for (i = 0; i < ARRAY_SIZE(sensor_formats); i++) {
			if (sensor_formats[i].wdr_mode == wdr_on) {
				sensor_format = &sensor_formats[i];
				isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
				sensor_print("fine wdr is %d, use fps is %d\n", wdr_on, sensor_format->fps_fixed);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		sensor_format = &sensor_formats[0];
		isp_get_cfg[ispid].sensor_wdr_on = sensor_format->wdr_mode;
		isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
		sensor_print("use wdr is %d, use fps is %d\n", sensor_format->wdr_mode, sensor_format->fps_fixed);
	}

done:
	current_win[id] = sensor_format;
	return sensor_format;
#else //CONFIG_ISP_FAST_CONVERGENCE || CONFIG_ISP_HARD_LIGHTADC
	if (current_win[id])
		return current_win[id];

	current_win[id] = &sensor_formats[0];
	sensor_print("fine wdr is %d, fps is %d\n", sensor_formats[0].wdr_mode, sensor_formats[0].fps_fixed);
	return &sensor_formats[0];
#endif
}

static struct sensor_format_struct switch_sensor_formats[] = {
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
	{
#if RAW8
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR8_1X8,
#else//RAW10
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR10_1X10,
#endif
		.width 		= 1920,
		.height 	= 1080,
		.hoffset 	= 0,
		.voffset 	= 0,
		.hts 		= 2200,
		.vts 		= 1687,
		.pclk 		= 74250000,
#if RAW8
		.mipi_bps 	= 297000000,
#else//RAW10
		.mipi_bps 	= 371250000,
#endif
		.fps_fixed 	= 20,
		.bin_factor = 1,
		.intg_min 	= 1 << 4,
		.intg_max 	= (1125 - 8) << 4,
		.gain_min 	= 1 << 4,
		.gain_max 	= 1713 << 4,
		.regs 		= sensor_1080p20_regs,
		.regs_size 	= ARRAY_SIZE(sensor_1080p20_regs),
	}

	{
#if RAW8
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR8_1X8,
#else//RAW10
		.mbus_code 	= MEDIA_BUS_FMT_SBGGR10_1X10,
#endif
		.width 		= 1920,
		.height 	= 1080,
		.hoffset 	= 0,
		.voffset 	= 0,
		.hts 		= 2200,
		.vts 		= 3375,
		.pclk 		= 74250000,
#if RAW8
		.mipi_bps 	= 297000000,
#else//RAW10
		.mipi_bps 	= 371250000,
#endif
		.fps_fixed 	= 10,
		.bin_factor = 1,
		.intg_min 	= 1 << 4,
		.intg_max 	= (1125 - 8) << 4,
		.gain_min 	= 1 << 4,
		.gain_max 	= 1713 << 4,
		.regs 		= sensor_1080p10_regs,
		.regs_size 	= ARRAY_SIZE(sensor_1080p10_regs),
	},
#endif
};

static struct sensor_format_struct *sensor_get_switch_format(int id, int isp_id)
{
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);
	struct sensor_format_struct *sensor_format = NULL;
	int wdr_on = isp_get_cfg[ispid].sensor_wdr_on;
	int fps = isp_get_cfg[ispid].sensor_get_fps;
	int i;

	if (current_switch_win[id])
		return current_switch_win[id];

	for (i = 0; i < ARRAY_SIZE(switch_sensor_formats); i++) {
		if (switch_sensor_formats[i].wdr_mode == wdr_on) {
			if (switch_sensor_formats[i].fps_fixed == fps) {
				sensor_format = &switch_sensor_formats[i];
				sensor_print("switch fine wdr is %d, fine fps is %d\n", wdr_on, fps);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		for (i = 0; i < ARRAY_SIZE(switch_sensor_formats); i++) {
			if (switch_sensor_formats[i].wdr_mode == wdr_on) {
				sensor_format = &switch_sensor_formats[i];
				isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
				sensor_print("switch fine wdr is %d, use fps is %d\n", wdr_on, sensor_format->fps_fixed);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		sensor_format = &switch_sensor_formats[0];
		isp_get_cfg[ispid].sensor_wdr_on = sensor_format->wdr_mode;
		isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
		sensor_print("switch use wdr is %d, use fps is %d\n", sensor_format->wdr_mode, sensor_format->fps_fixed);
	}

done:
	current_switch_win[id] = sensor_format;
	return sensor_format;
#else
	return NULL;
#endif
}

static int sensor_g_mbus_config(int id, struct v4l2_mbus_config *cfg, struct mbus_framefmt_res *res)
{
	//struct sensor_info *info = to_state(sd);

	cfg->type  = V4L2_MBUS_CSI2;
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
	res->res_time_hs = 0x28;

	return 0;
}

static int sensor_reg_init(int id, int isp_id)
{
	int ret = 0;
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);
	struct sensor_exp_gain exp_gain;

	ret = sensor_write_array(id, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	if (current_win[id]->regs)
		ret = sensor_write_array(id, current_win[id]->regs, current_win[id]->regs_size);
	if (ret < 0)
		return ret;

	sc200ai_sensor_vts = current_win[id]->vts;

	return 0;
}

static int sensor_s_stream(int id, int isp_id, int enable)
{
	if (enable && sensor_stream_count[id]++ > 0)
		return 0;
	else if (!enable && (sensor_stream_count[id] == 0 || --sensor_stream_count[id] > 0))
		return 0;

	sensor_dbg("%s on = %d, 2560*1440 fps: 15\n", __func__, enable);

	if (!enable)
		return 0;

	return sensor_reg_init(id, isp_id);
}

static int sensor_s_switch(int id)
{
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
	struct sensor_exp_gain exp_gain;
	int ret = -1;
	temperature_ctrl = 1;
	sc200ai_sensor_vts = current_switch_win[id]->vts;
	if (current_switch_win[id]->switch_regs)
		ret = sensor_write_array(id, current_switch_win[id]->switch_regs, current_switch_win[id]->switch_regs_size);
	else
		sensor_err("cannot find 480p120fps to 1080p%dfps reg\n", current_switch_win[id]->fps_fixed);
	if (ret < 0)
		return ret;33333333
#endif
	return 0;
}

static int sensor_test_i2c(int id)
{
	int ret;
	sensor_power(id, PWR_ON);
	ret = sensor_init(id);
	sensor_power(id, PWR_OFF);

	return ret;
}

struct sensor_fuc_core sc200ai_core  = {
	.g_mbus_config = sensor_g_mbus_config,
	.sensor_test_i2c = sensor_test_i2c,
	.sensor_power = sensor_power,
	.s_ir_status = sensor_set_ir,
	.s_stream = sensor_s_stream,
	.s_switch = sensor_s_switch,
	.s_exp_gain = sensor_s_exp_gain,
	.sensor_g_format = sensor_get_format,
	.sensor_g_switch_format = sensor_get_switch_format,
};