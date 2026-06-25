/*
 * A V4L2 driver for ov8856_mipi Raw cameras.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <linux/io.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("lwj");
MODULE_DESCRIPTION("A low-level driver for OV8856 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)//def 24
#define V4L2_IDENT_SENSOR  0x885a

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

/*
 * The OV2718 i2c address
 */
#define I2C_ADDR 0x6C

#define SENSOR_NUM 0x2
#define SENSOR_NAME "ov8856_mipi"
#define SENSOR_NAME_2 "ov8856_mipi_2"

#define FULL_SIZE

#define EEPROM_WRITE_ID 0xA0

#define AW_OTP_OFFSET 0x0000

#define AW_OTP_AWB_Start 0x0009
#define AW_OTP_MSC_Start 0x0011
#define AW_OTP_DPC_Start 0x0c13
#define AW_OTP_Check_Start 0x0c1d
#define AW_OTP_All_CheckSum 0x0c21
#define AW_OTP_AF_Start 0x0c23
#define AW_OTP_AF_CheckSum 0x0c27

#define OTP_SIZE (0x0c27 + 1)
static int sensor_otp_enable; /* 0: init, -1: disable, 1: enable */
static unsigned char sensor_otp_info[OTP_SIZE];
#define LSC_INFO_SIZE (16 * 16 * 3 * 2)
#define AWB_INFO_SIZE 8
#define AF_INFO_SIZE 2
static unsigned short sensor_otp_lscinfo[LSC_INFO_SIZE + AWB_INFO_SIZE + AF_INFO_SIZE];
/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {
};

//5M 2LANE 1.2Gb
static struct regval_list sensor_5M_regs[] = {
	{0x0100, 0x00},
	{0x0302, 0x35},
	{0x0303, 0x00},
	{0x031e, 0x0c},
	{0x3000, 0x00},
	{0x300e, 0x00},
	{0x3010, 0x00},
	{0x3015, 0x84},
	{0x3018, 0x32},
	{0x3033, 0x24},
	{0x3500, 0x00},

	{0x3501, 0x9a},
	{0x3502, 0x20},

	{0x3503, 0x08},
	{0x3505, 0x83},
	{0x3508, 0x01},
	{0x3509, 0x80},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x02},
	{0x3512, 0x00},
	{0x3600, 0x72},
	{0x3601, 0x40},
	{0x3602, 0x30},
	{0x3610, 0xc5},
	{0x3611, 0x58},
	{0x3612, 0x5c},
	{0x3613, 0x5a},
	{0x3614, 0x60},
	{0x3628, 0xff},
	{0x3629, 0xff},
	{0x362a, 0xff},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x10},
	{0x3663, 0x08},
	{0x3669, 0x34},

	{0x366e, 0x10},

	{0x3706, 0x86},
	{0x370b, 0x7e},

	{0x3714, 0x23},

	{0x3730, 0x12},
	{0x3733, 0x10},
	{0x3764, 0x00},
	{0x3765, 0x00},
	{0x3769, 0x62},
	{0x376a, 0x2a},
	{0x376b, 0x30},
	{0x3780, 0x00},
	{0x3781, 0x24},
	{0x3782, 0x00},
	{0x3783, 0x23},
	{0x3798, 0x2f},
	{0x37a1, 0x60},
	{0x37a8, 0x6a},
	{0x37ab, 0x3f},

	{0x37c2, 0x04},

	{0x37c3, 0xf1},
	{0x37c9, 0x80},
	{0x37cb, 0x03},
	{0x37cc, 0x0a},
	{0x37cd, 0x16},
	{0x37ce, 0x1f},

	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},//12
	{0x3804, 0x0c},//3295; =>3264-2592 = 672
	{0x3805, 0xdf},
	{0x3806, 0x09},//2467; =>2448-1944 = 504
	{0x3807, 0xa3},
	{0x3808, 0x0a},// 0x0cc0,3264
	{0x3809, 0x20},
	{0x380a, 0x07},// 0x0990,2448
	{0x380b, 0x98},
	{0x380c, 0x07},//hts, 0x078c
	{0x380d, 0x8c},
	{0x380e, 0x07},//vts, 0x09b2
	{0x380f, 0xba},
	{0x3810, 0x00},//H win offset 16
	{0x3811, 0x10},
	{0x3812, 0x00},//V win offset 4
	{0x3813, 0x04},
	{0x3814, 0x01},

	{0x3815, 0x01},
	{0x3816, 0x00},
	{0x3817, 0x00},
	{0x3818, 0x00},
	{0x3819, 0x00},

	{0x3820, 0x86},
	{0x3821, 0x40},

	{0x382a, 0x01},

	{0x382b, 0x01},
	{0x3830, 0x06},
	{0x3836, 0x02},
	{0x3862, 0x04},
	{0x3863, 0x08},
	{0x3cc0, 0x33},
	{0x3d85, 0x17},
	{0x3d8c, 0x73},
	{0x3d8d, 0xde},
	{0x4001, 0xe0},
	{0x4003, 0x40},
	{0x4008, 0x00},

	{0x4009, 0x0b},

	{0x400f, 0x80},
	{0x4010, 0xf0},
	{0x4011, 0xff},
	{0x4012, 0x02},
	{0x4013, 0x01},
	{0x4014, 0x01},
	{0x4015, 0x01},
	{0x4042, 0x00},
	{0x4043, 0x80},
	{0x4044, 0x00},
	{0x4045, 0x80},
	{0x4046, 0x00},
	{0x4047, 0x80},
	{0x4048, 0x00},
	{0x4049, 0x80},
	{0x4041, 0x03},
	{0x404c, 0x20},
	{0x404d, 0x00},
	{0x404e, 0x20},
	{0x4203, 0x80},
	{0x4307, 0x30},
	{0x4317, 0x00},
	{0x4503, 0x08},
	{0x4601, 0x80},
	{0x4816, 0x53},
	{0x481b, 0x58},
	{0x481f, 0x27},
	{0x4837, 0x0c},
	{0x5000, 0x77},
	{0x5001, 0x0a},
	{0x5004, 0x04},
	{0x502e, 0x03},
	{0x5030, 0x41},
	{0x5795, 0x02},
	{0x5796, 0x20},
	{0x5797, 0x20},
	{0x5798, 0xd5},
	{0x5799, 0xd5},
	{0x579a, 0x00},
	{0x579b, 0x50},
	{0x579c, 0x00},
	{0x579d, 0x2c},
	{0x579e, 0x0c},
	{0x579f, 0x40},
	{0x57a0, 0x09},
	{0x57a1, 0x40},
	{0x5780, 0x14},
	{0x5781, 0x0f},
	{0x5782, 0x44},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x01},
	{0x5786, 0x00},
	{0x5787, 0x04},
	{0x5788, 0x02},
	{0x5789, 0x0f},
	{0x578a, 0xfd},
	{0x578b, 0xf5},
	{0x578c, 0xf5},
	{0x578d, 0x03},
	{0x578e, 0x08},
	{0x578f, 0x0c},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x52},
	{0x5794, 0xa3},
	{0x5a08, 0x02},
	{0x5b00, 0x02},
	{0x5b01, 0x10},
	{0x5b02, 0x03},
	{0x5b03, 0xcf},
	{0x5b05, 0x6c},
	{0x5e00, 0x00},
	{0x0100, 0x01},

	/*{0xfff9,0x030e},
	{0x0020,0x0021},
	{0xfff9,0x030e},
	{0x0018,0x0013},
	{0xfff9,0x030e},
	{0x0016,0x305f},*/
};

//8M 4LANE,720M
static struct regval_list sensor_4lane_full_regs[] = {
	{0x0103, 0x01},
	{0x0302, 0x3c},
	{0x0303, 0x01},
	{0x3000, 0x00},
	{0x300e, 0x00},
	{0x3010, 0x00},
	{0x3015, 0x84},
	{0x3018, 0x72},
	{0x3033, 0x24},
	{0x3500, 0x00},
	{0x3501, 0x9a},
	{0x3502, 0x20},
	{0x3503, 0x08},
	{0x3505, 0x83},
	{0x3508, 0x01},
	{0x3509, 0x80},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x02},
	{0x3512, 0x00},
	{0x3600, 0x72},
	{0x3601, 0x40},
	{0x3602, 0x30},
	{0x3610, 0xc5},
	{0x3611, 0x58},
	{0x3612, 0x5c},
	{0x3613, 0x5a},
	{0x3614, 0x60},
	{0x3628, 0xff},
	{0x3629, 0xff},
	{0x362a, 0xff},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x10},
	{0x3663, 0x08},
	{0x3669, 0x34},
	{0x366e, 0x10},
	{0x3733, 0x10},
	{0x3764, 0x00},
	{0x3765, 0x00},
	{0x3769, 0x42},
	{0x376a, 0x2a},
	{0x376b, 0x36},
	{0x3780, 0x00},
	{0x3781, 0x24},
	{0x3782, 0x00},
	{0x3783, 0x23},
	{0x3798, 0x2f},
	{0x37a1, 0x60},
	{0x37a8, 0x6a},
	{0x37ab, 0x3f},
	{0x37c2, 0x04},
	{0x37c3, 0xf1},
	{0x37c9, 0x80},
	{0x37cb, 0x03},
	{0x37cc, 0x0a},
	{0x37cd, 0x16},
	{0x37ce, 0x1f},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x07},
	{0x380d, 0x8c},
	{0x380e, 0x09},
	{0x380f, 0xb2},
	{0x3810, 0x00},
	{0x3811, 0x10},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x00},
	{0x3817, 0x00},
	{0x3818, 0x00},
	{0x3819, 0x00},
	{0x3820, 0x86},
	{0x3821, 0x40},
	{0x382a, 0x01},
	{0x382b, 0x01},
	{0x3830, 0x06},
	{0x3836, 0x02},
	{0x3862, 0x04},
	{0x3863, 0x08},
	{0x3cc0, 0x33},
	{0x3d85, 0x17},
	{0x3d8c, 0x73},
	{0x3d8d, 0xde},
	{0x4001, 0xe0},
	{0x4003, 0x40},
	{0x4008, 0x00},
	{0x4009, 0x0b},
	{0x400f, 0x80},
	{0x4010, 0xf0},
	{0x4011, 0xff},
	{0x4012, 0x02},
	{0x4013, 0x01},
	{0x4014, 0x01},
	{0x4015, 0x01},
	{0x4042, 0x00},
	{0x4043, 0x80},
	{0x4044, 0x00},
	{0x4045, 0x80},
	{0x4046, 0x00},
	{0x4047, 0x80},
	{0x4048, 0x00},
	{0x4049, 0x80},
	{0x4041, 0x03},
	{0x404c, 0x20},
	{0x404d, 0x00},
	{0x404e, 0x80},
	{0x4203, 0x80},
	{0x4307, 0x30},
	{0x4317, 0x00},
	{0x4503, 0x08},
	{0x4601, 0x80},
	{0x4816, 0x53},
	{0x481f, 0x27},
	{0x4837, 0x16},
	{0x5000, 0x77},
	{0x5001, 0x04},//def 0x0a
	{0x5004, 0x00},
	{0x502e, 0x00},
	{0x5030, 0x41},
	{0x5795, 0x12},
	{0x5796, 0x20},
	{0x5797, 0x20},
	{0x5798, 0xd5},
	{0x5799, 0xd5},
	{0x579a, 0x00},
	{0x579b, 0x50},
	{0x579c, 0x00},
	{0x579d, 0x2c},
	{0x579e, 0x0c},
	{0x579f, 0x40},
	{0x57a0, 0x09},
	{0x57a1, 0x40},
	{0x5780, 0x14},
	{0x5781, 0x0f},
	{0x5782, 0x44},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x01},
	{0x5786, 0x00},
	{0x5787, 0x04},
	{0x5788, 0x02},
	{0x5789, 0x0f},
	{0x578a, 0xfd},
	{0x578b, 0xf5},
	{0x578c, 0xf5},
	{0x578d, 0x03},
	{0x578e, 0x08},
	{0x578f, 0x0c},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x52},
	{0x5794, 0xa3},
	{0x5a08, 0x02},
	{0x5b00, 0x02},
	{0x5b01, 0x10},
	{0x5b02, 0x03},
	{0x5b03, 0xcf},
	{0x5b05, 0x6c},
	{0x5e00, 0x00},
	{0x0100, 0x01},
};

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
 * if not support the follow function ,retrun -EINVAL
 */

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int ov8856_mipi_sensor_vts;
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	unsigned char explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);

	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	exphigh = (unsigned char) ((0x0f0000&exp_val)>>16);
	expmid	= (unsigned char) ((0x00ff00&exp_val)>>8);
	explow	= (unsigned char) (0x0000ff&exp_val);

	sensor_write(sd, 0x3502, explow);
	sensor_write(sd, 0x3501, expmid);
	sensor_write(sd, 0x3500, exphigh);

	info->exp = exp_val;
	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	unsigned char gainlow = 0;
	unsigned char gainhigh = 0;
	unsigned char digi_gainlow = 0;
	unsigned char digi_gainhigh = 0;
	int digi_gain = 0, ana_gain = 0;
	struct sensor_info *info = to_state(sd);

	digi_gain = gain_val;
	ana_gain  = gain_val;

	if (gain_val <= 1 * 16) {
		ana_gain  = 16;
		digi_gain = 0x400;
	} else if (gain_val > 1 * 16 && gain_val <= 16 * 16 - 1) {
		digi_gain = 0x400;
		ana_gain  = gain_val;
	} else if (gain_val > 16 * 16 - 1 && gain_val <= 32 * 16 - 1) {
		digi_gain = (gain_val - 255 + 16) * 4 + 1024;
		ana_gain = 16 * 16 - 1;
	} else if (gain_val > 32 * 16 - 1 && gain_val <= 64 * 16 - 1) {
		digi_gain = (gain_val - 511 + 16) * 2 + 2048;
		ana_gain = 16 * 16 - 1;
	} else {
		digi_gain = 3072;
		ana_gain = 16 * 16 - 1;
	}

	ana_gain *= 8;
	gainlow = (unsigned char)(ana_gain & 0xff);
	gainhigh = (unsigned char)((ana_gain >> 8) & 0x1f);
	digi_gainlow = (unsigned char)(digi_gain & 0x3f);
	digi_gainhigh = (unsigned char)((digi_gain >> 6) & 0xff);
	sensor_write(sd, 0x3509, gainlow);
	sensor_write(sd, 0x3508, gainhigh);
	sensor_write(sd, 0x350b, digi_gainlow);
	sensor_write(sd, 0x350a, digi_gainhigh);
	info->gain = gain_val;

	return 0;
}

static int tmp_exp_val = -1, tmp_gain_val = -1;
//int frame_length_saved = 0;
static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	int frame_length, shutter;
	static int exp_val, gain_val;
	struct sensor_info *info = to_state(sd);

	/*if(tmp_exp_val == exp_gain->exp_val && tmp_gain_val == exp_gain->gain_val)
	{
		return 0;
	}else
	{
		tmp_exp_val  = exp_gain->exp_val;
		tmp_gain_val = exp_gain->gain_val;
	}*/

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	/*shutter = exp_val / 16;
	if(shutter  > ov8856_mipi_sensor_vts - 4)
		frame_length = shutter + 4;
	else
		frame_length = ov8856_mipi_sensor_vts;*/

	sensor_write(sd, 0x3208, 0x00);//enter group write
	/*if(frame_length != frame_length_saved){
		sensor_write(sd, 0x380f, (frame_length & 0xff));
		sensor_write(sd, 0x380e, (frame_length >> 8));
		frame_length_saved = frame_length;
	}*/
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
	sensor_write(sd, 0x3208, 0x10);//end group write
	sensor_write(sd, 0x3208, 0xa0);//init group write

	info->gain = gain_val;
	info->exp = exp_val;
	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
//	*code = MEDIA_BUS_FMT_SBGGR10_1X10;
	*code = MEDIA_BUS_FMT_SRGGB10_1X10;

	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	data_type rdval;

	ret = sensor_read(sd, 0x0100, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == STBY_ON) {//sw stby on
		ret = sensor_write(sd, 0x0100, rdval&0xfe);
	} else {//sw stby off
		ret = sensor_write(sd, 0x0100, rdval|0x01);
	}
	return ret;
}

/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		ret = sensor_s_sw_stby(sd, STBY_ON);
		if (ret < 0)
			sensor_err("soft stby falied!\n");
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		usleep_range(10000, 12000);
		ret = sensor_s_sw_stby(sd, STBY_OFF);
		if (ret < 0)
			sensor_err("soft stby off falied!\n");
		cci_unlock(sd);
		break;
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);
#if 0
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		//power supply
		vin_set_pmu_channel(sd, IOVDD, ON);
		//add by chao
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, AVDD, ON);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AFVDD, ON);
		//add by chao
		usleep_range(7000, 8000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);

		//reset on io
		usleep_range(20000, 22000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		//active mclk before power on
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
#endif
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_set_pmu_channel(sd, AFVDD, ON);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		//vin_set_pmu_channel(sd, CAMERAVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(5000, 6000);
		vin_set_pmu_channel(sd, DVDD, ON);

		usleep_range(11000, 13000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);

		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!\n");
		cci_lock(sd);
#if 0
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_set_mclk(sd, OFF);

		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);

		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		vin_gpio_set_status(sd, POWER_EN, 0);
#endif
		vin_set_mclk(sd, OFF);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		vin_set_pmu_channel(sd, DVDD, OFF);
		usleep_range(5000, 6000);
		vin_set_pmu_channel(sd, AVDD, OFF);
		//vin_set_pmu_channel(sd, CAMERAVDD, OFF);
		usleep_range(5000, 6000);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);

		cci_unlock(sd);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	switch (val) {
	case 0:
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rdval = 0;

	sensor_read(sd, 0x300A, &rdval);
	sensor_print("%s read 0x300A value is 0x%x\n", __func__, rdval);
	sensor_read(sd, 0x300B, &rdval);
	sensor_print("%s read 0x300B value is 0x%x\n", __func__, rdval);
	sensor_read(sd, 0x300C, &rdval);
	sensor_print("%s read 0x300C value is 0x%x\n", __func__, rdval);
	return 0;
}

static int sensor_read_block_otp_a16_d8(struct v4l2_subdev *sd, unsigned char dev_addr, unsigned short eeprom_addr, unsigned char *buf, int len)
{
#if 0
	unsigned char reg[3];
	struct i2c_msg msg[2];
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!sd || !buf)
		return -1;

	reg[0] = (eeprom_addr & 0xff00) >> 8;
	reg[1] = eeprom_addr & 0xff;
	reg[2] = 0xee;

	msg[0].addr = dev_addr >> 1;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = reg;

	msg[1].addr = dev_addr >> 1;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;

	return (i2c_transfer(client->adapter, msg, 2) == 2);
#else
	unsigned char reg[3];
	struct i2c_msg msg[2];
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, ret = 0, read_addr;

	if (!sd || !buf)
		return -1;

	for (i = 0; i < len; i++) {
		read_addr = eeprom_addr + i;
		reg[0] = (read_addr & 0xff00) >> 8;
		reg[1] = read_addr & 0xff;
		reg[2] = 0xee;

		msg[0].addr = dev_addr >> 1;
		msg[0].flags = 0;
		msg[0].len = 2;
		msg[0].buf = reg;

		msg[1].addr = dev_addr >> 1;
		msg[1].flags = I2C_M_RD;
		msg[1].len = 1;
		msg[1].buf = buf + i;

		ret = i2c_transfer(client->adapter, msg, 2);
		if (ret >= 0) {
			ret = 0;
		} else {
			sensor_err("read otp reg [0x%x] error.\n", read_addr);
			ret = -1;
			break;
		}
	}

	return ret;
#endif
}

static int check_otp_checksum(unsigned char *data, int len)
{
	unsigned int i, Data_Check = 0, Check_Sum = 0, AF_Check_Sum = 0;
	int ret = 0;

	if (!data || len <= 0)
		return -1;

	for (i = 0; i < AW_OTP_All_CheckSum; i++) {
		Check_Sum += data[i];
	}
	Check_Sum = Check_Sum % 255 + 1;

	Data_Check = data[AW_OTP_Check_Start] + data[AW_OTP_Check_Start + 1] + data[AW_OTP_Check_Start + 2];
	sensor_print("Data_Check: 0x%x 0x%x 0x%x 0x%x\n", data[0x0c1d], data[0x0c1e], data[0x0c1f], data[0x0c20]);
	if (Data_Check != data[AW_OTP_Check_Start + 3]) {
		ret = -1;
		sensor_err("Data_Check error. Data_Check = 0x%x, [0x0c20] = 0x%x\n", Data_Check, data[AW_OTP_Check_Start + 3]);
	}

	if (Check_Sum != data[AW_OTP_All_CheckSum]) {
		ret = -1;
		sensor_err("Check_Sum error. Check_Sum = 0x%x, [0x0c21] = 0x%x\n", Check_Sum, data[AW_OTP_All_CheckSum]);
	}

	if (data[0x0c22] == 0x1) {
		sensor_print("AF_OTP enable\n");
		AF_Check_Sum = data[AW_OTP_AF_Start] + data[AW_OTP_AF_Start + 1] + data[AW_OTP_AF_Start + 2] + data[AW_OTP_AF_Start + 3];
		AF_Check_Sum = AF_Check_Sum % 255 + 1;

		if (AF_Check_Sum != data[AW_OTP_AF_CheckSum]) {
			ret = -1;
			sensor_err("AF_Check_Sum error. AF_Check_Sum = 0x%x, [0x0c27] = 0x%x\n", AF_Check_Sum, data[AW_OTP_AF_CheckSum]);
		}
	} else {
		sensor_print("AF_OTP disable\n");
	}

	if (ret < 0)
		sensor_err("sensor otp info checksum error\n");

	return ret;
}

static void conver_Otp2AwInfo(unsigned char *sensor_otp_src,
			      unsigned short *aw_info_dst)
{
	unsigned char *src;
	unsigned short *dst;
	unsigned int i;
	unsigned short val, high, low;

	/* Get MSC table */
	src = &sensor_otp_src[AW_OTP_MSC_Start];
	dst = &aw_info_dst[0];
	for (i = 0; i < LSC_INFO_SIZE; i++) {
		high = *src;
		src++;
		low = *src;
		src++;
		val = (high << 8) + low;
		*dst = val;
		dst++;
	}

	/* Get AWB Data */
	src = &sensor_otp_src[AW_OTP_AWB_Start];
	dst = &aw_info_dst[LSC_INFO_SIZE];
	for (i = 0; i < AWB_INFO_SIZE; i++) {
		*dst = *src;
		dst++;
		src++;
	}

	/* Get AF Data */
	src = &sensor_otp_src[AW_OTP_AF_Start];
	dst = &aw_info_dst[LSC_INFO_SIZE + AWB_INFO_SIZE];
	if (sensor_otp_src[0x0c22] == 0x1) {
		for (i = 0; i < AF_INFO_SIZE; i++) {
			high = *src;
			src++;
			low = *src;
			src++;
			val = (high << 8) + low;
			*dst = val;
			dst++;
		}
	} else {
		for (i = 0; i < AF_INFO_SIZE; i++) {
			*dst = 0;
			dst++;
		}
	}
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret, i, j, tmp;
	data_type mirror = 0, flip = 0;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 3264;
	info->height = 2448;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 30;	/* 30fps */

	if (sensor_otp_enable == 0) {
		memset(sensor_otp_info, 0, sizeof(sensor_otp_info));
		ret = sensor_read_block_otp_a16_d8(sd, EEPROM_WRITE_ID,
						   AW_OTP_OFFSET,
						   sensor_otp_info,
						   sizeof(sensor_otp_info));

		if (ret < 0 || sensor_otp_info[0x0000] != 0x01 ||
			sensor_otp_info[0x0001] != 0xff || sensor_otp_info[0x0002] != 0x00 ||
			sensor_otp_info[0x0003] != 0x0b || sensor_otp_info[0x0004] != 0x01) {
			sensor_otp_enable = -1;
			sensor_print("sensor_otp_disable ret=%d [0x0000]=%x [0x0001]=%x [0x0002]=%x [0x0003]=%x [0x0004]=%x\n",
				     ret, sensor_otp_info[0x0000], sensor_otp_info[0x0001], sensor_otp_info[0x0002],
					 sensor_otp_info[0x0003], sensor_otp_info[0x0004]);
			return 0;
		}
		sensor_otp_enable = 1;
		ret = check_otp_checksum(sensor_otp_info,
					 sizeof(sensor_otp_info));
		if (ret < 0)
			sensor_otp_enable = -1;
		else
			sensor_print("sensor ckeck OTP success\n");

		conver_Otp2AwInfo(&sensor_otp_info[0], &sensor_otp_lscinfo[0]);

		//sensor_read(sd, 0x3820, &flip);
		//sensor_read(sd, 0x3821, &mirror);
		//if ((mirror & 0x6) == 0x6) {//sensor mirror
		//	sensor_print("Set OTP table mirror\n");
		//	for (i = 0; i < 16; i++) {
		//		for (j = 0; j < 16; j++) {
		//			//R
		//			tmp = sensor_otp_lscinfo[i*16+j];
		//			sensor_otp_lscinfo[i*16+j] = sensor_otp_lscinfo[i*16+(15-j)+768];
		//			sensor_otp_lscinfo[i*16+(15-j)+768] = tmp;
		//			//G
		//			tmp = sensor_otp_lscinfo[i*16+j+256];
		//			sensor_otp_lscinfo[i*16+j+256] = sensor_otp_lscinfo[i*16+(15-j)+768+256];
		//			sensor_otp_lscinfo[i*16+(15-j)+768+256] = tmp;
		//			//B
		//			tmp = sensor_otp_lscinfo[i*16+j+512];
		//			sensor_otp_lscinfo[i*16+j+512] = sensor_otp_lscinfo[i*16+(15-j)+768+512];
		//			sensor_otp_lscinfo[i*16+(15-j)+768+512] = tmp;
		//		}
		//	}
		//}
		//if ((flip & 0x6) == 0x6) {//sensor flip
		//	sensor_print("Set OTP table flip\n");
			for (i = 0; i < 8; i++) {
				for (j = 0; j < 16; j++) {
					//R
					tmp = sensor_otp_lscinfo[i*16+j];
					sensor_otp_lscinfo[i*16+j] = sensor_otp_lscinfo[(15-i)*16+j];
					sensor_otp_lscinfo[(15-i)*16+j] = tmp;
					tmp = sensor_otp_lscinfo[i*16+j+768];
					sensor_otp_lscinfo[i*16+j+768] = sensor_otp_lscinfo[(15-i)*16+j+768];
					sensor_otp_lscinfo[(15-i)*16+j+768] = tmp;
					//G
					tmp = sensor_otp_lscinfo[i*16+j+256];
					sensor_otp_lscinfo[i*16+j+256] = sensor_otp_lscinfo[(15-i)*16+j+256];
					sensor_otp_lscinfo[(15-i)*16+j+256] = tmp;
					tmp = sensor_otp_lscinfo[i*16+j+256+768];
					sensor_otp_lscinfo[i*16+j+256+768] = sensor_otp_lscinfo[(15-i)*16+j+256+768];
					sensor_otp_lscinfo[(15-i)*16+j+256+768] = tmp;
					//B
					tmp = sensor_otp_lscinfo[i*16+j+512];
					sensor_otp_lscinfo[i*16+j+512] = sensor_otp_lscinfo[(15-i)*16+j+512];
					sensor_otp_lscinfo[(15-i)*16+j+512] = tmp;
					tmp = sensor_otp_lscinfo[i*16+j+512+768];
					sensor_otp_lscinfo[i*16+j+512+768] = sensor_otp_lscinfo[(15-i)*16+j+512+768];
					sensor_otp_lscinfo[(15-i)*16+j+512+768] = tmp;
				}
			}
		//}

		//debug infomation
		printk("--- awb otp ----\n");
		for (i = 0; i < AWB_INFO_SIZE; i++) {
			printk("%d \n", sensor_otp_lscinfo[LSC_INFO_SIZE + i]);
		}
		printk("---- msc ----");
		for (i = 0; i < LSC_INFO_SIZE; i++) {
			if (i % 16 == 0)
				printk("\n");
			printk("%4d, ", sensor_otp_lscinfo[i]);
		}
		printk("\n");
		printk("---- af ----\n");
		printk("%d  %d\n", sensor_otp_lscinfo[LSC_INFO_SIZE + AWB_INFO_SIZE], sensor_otp_lscinfo[LSC_INFO_SIZE + AWB_INFO_SIZE + 1]);
	}
	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins, sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		ret = 0;
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		ret = sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_GET_SENSOR_CODE:
		//sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
	case VIDIOC_VIN_ACT_INIT:
		ret = actuator_init(sd, (struct actuator_para *)arg);
		break;
	case VIDIOC_VIN_ACT_SET_CODE:
		ret = actuator_set_code(sd, (struct actuator_ctrl *)arg);
		break;
	case VIDIOC_VIN_FLASH_EN:
		flash_en(sd, (struct flash_para *)arg);
		ret = 0;
		break;
	case VIDIOC_VIN_GET_SENSOR_OTP_INFO:
		if (sensor_otp_enable == 1) {
			memcpy(arg, sensor_otp_lscinfo, sizeof(sensor_otp_lscinfo));
			sensor_print("user copy otp info success\n");
		} else {
			ret = -EFAULT;
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
	{
		.desc = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.regs = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {
#ifdef FULL_SIZE
	/* 8M 4Lane*/
	{
		.width	    = 3264,
		.height     = 2448,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 1932,
		.vts        = 2482,
		.pclk       = 144*1000*1000,
		.mipi_bps   = 360*1000*1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 2<<4,
		.intg_max   = (2482-4)<<4,
		.gain_min   = 1<<4,
		.gain_max   = 256<<4,
		.regs       = sensor_4lane_full_regs,
		.regs_size  = ARRAY_SIZE(sensor_4lane_full_regs),
		.set_size   = NULL,
	},
#else
	/*same as: sc500cs_mipi 5M 2592x1944*/
	{
		.width	    = 2592,
		.height     = 1944,
		.hoffset    = 0,
		.voffset    = 0,
		.hts	    = 1932,
		.vts	    = 1978,
		.pclk	    = 144*1000*1000,
		.mipi_bps   = 1272*1000*1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 2<<4,
		.intg_max   = (1978-4)<<4,
		.gain_min   = 1<<4,
		.gain_max   = 256<<4,
		.regs	    = sensor_5M_regs,
		.regs_size  = ARRAY_SIZE(sensor_5M_regs),
		.set_size   = NULL,
	},
#endif
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;

#ifdef FULL_SIZE
	cfg->flags = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#else
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#endif

	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_g_gain(sd, &ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_g_exp(sd, &ctrl->val);
	}
	return -EINVAL;
}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info =
			container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->val);
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->val);
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	struct sensor_exp_gain exp_gain;

	//data_type get_value;
	//int i;

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	/*for(i = 0; i< ARRAY_SIZE(sensor_default_regs); i++)
	{
		sensor_read(sd, sensor_default_regs[i].addr, &get_value);
		pr_err("sensor_read_array reg = 0x%x data = 0x%x \n",sensor_default_regs[i].addr,get_value);
	}*/

	sensor_dbg("sensor_reg_init\n");

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	ov8856_mipi_sensor_vts = wsize->vts;

	/*exp_gain.exp_val = 16000;
	exp_gain.gain_val = 32;
	sensor_s_exp_gain(sd, &exp_gain);*/

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width, wsize->height);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_dbg("%s on = %d, %d*%d %x\n", __func__, enable,
		     info->current_wins->width,
		     info->current_wins->height, info->fmt->mbus_code);

	if (!enable)
		return 0;

	return sensor_reg_init(info);
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.g_volatile_ctrl = sensor_g_ctrl,
	.s_ctrl = sensor_s_ctrl,
	.try_ctrl = sensor_try_ctrl,
};

static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.reset = sensor_reset,
	.init = sensor_init,
	.s_power = sensor_power,
	.ioctl = sensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_parm = sensor_s_parm,
	.g_parm = sensor_g_parm,
	.s_stream = sensor_s_stream,
	.g_mbus_config = sensor_g_mbus_config,
};

static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.enum_mbus_code = sensor_enum_mbus_code,
	.enum_frame_size = sensor_enum_frame_size,
	.get_fmt = sensor_get_fmt,
	.set_fmt = sensor_set_fmt,
};

static const struct v4l2_subdev_ops sensor_ops = {
	.core = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad = &sensor_pad_ops,
};

/* ----------------------------------------------------------------------- */
static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	}
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 4);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 16,
			      2816 * 16, 1, 1 * 16);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	/*param： handler v4l2所属设备指针，ops 操作函数指针，V4L2_CID_EXPOSURE 唯一标识ID，1 min, 65536 * 16 max, 1 default value */
	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1,
			      65536 * 16, 1, 1);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

static int sensor_dev_id;

static int sensor_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int i;

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[i]);
	} else {
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[sensor_dev_id++]);
	}

	sensor_init_controls(sd, &sensor_ctrl_ops);

	mutex_init(&info->lock);

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->combo_mode = CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE;
	//info->time_hs = 0x55;
	info->af_first_flag = 1;
	info->exp = 0;
	info->gain = 0;

	return 0;
}
static int sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd;
	int i;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		sd = cci_dev_remove_helper(client, &cci_drv[i]);
	} else {
		sd = cci_dev_remove_helper(client, &cci_drv[sensor_dev_id++]);
	}

	kfree(to_state(sd));
	return 0;
}

static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

static const struct i2c_device_id sensor_id_2[] = {
	{SENSOR_NAME_2, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);
MODULE_DEVICE_TABLE(i2c, sensor_id_2);

static struct i2c_driver sensor_driver[] = {
	{
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id,
	}, {
		.driver = {
			   .owner = THIS_MODULE,
			   .name = SENSOR_NAME_2,
			   },
		.probe = sensor_probe,
		.remove = sensor_remove,
		.id_table = sensor_id_2,
	},
};

static __init int init_sensor(void)
{
	int i, ret = 0;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		ret = cci_dev_init_helper(&sensor_driver[i]);

	return ret;
}

static __exit void exit_sensor(void)
{
	int i;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++)
		cci_dev_exit_helper(&sensor_driver[i]);
}

module_init(init_sensor);
module_exit(exit_sensor);
