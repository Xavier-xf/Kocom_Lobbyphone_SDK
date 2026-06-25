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

MODULE_AUTHOR("hzh");
MODULE_DESCRIPTION("A low-level driver for bf2257cs sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR  0x2257

/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0xfc
#define ID_REG_LOW		0xfd
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)
#define SENSOR_FRAME_RATE 30

/*
 * The bf2257cs i2c address
 */
#define I2C_ADDR 0xdc//0xdc

#define SENSOR_NUM 0x2
#define SENSOR_NAME "bf2257cs_mipi"
#define SENSOR_NAME_2 "bf2257cs_mipi_2"

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};



static struct regval_list sensor_1600x1200_30_regs[] = {
	//BF2257_RAW10_MIPI_2M_XCLK24M_PCLK67.2M_max30fps_V3.1.1_20221018_phone
	//XCLK:24M;  MIPICLK:672M  PCLK(RAW10):67.2M
	//行长：1792  帧长：1250
	//Max fps:30fps
	//1600*1200

	{0xf2, 0x01},
	{0x00, 0x01},
	{0x02, 0xb7},
	{0x1e, 0x04},
	{0x24, 0x60},
	{0xe0, 0x00},
	{0xe1, 0x01},//延时2ns
	{0xe2, 0x08},
	{0xe5, 0xe3},
	{0xe6, 0x60},//ADC range
	{0xe7, 0x33},
	{0xe8, 0x12},
	{0xe9, 0x89},
	{0xea, 0x87},
	{0xeb, 0x80},//高频写0x80，低频写0x84
	{0xec, 0x91},
	{0xed, 0x60},

	//MIPICLK:672M
	{0xe3, 0x78},
	{0xe4, 0xe0},

	//行长，帧长1792*1250
	{0x06, 0x10},
	{0x07, 0x00},
	{0x0b, 0x80},
	{0x0c, 0x03},

	//Black target
	{0x59, 0x40},
	{0x5a, 0x40},
	{0x5b, 0x40},
	{0x5c, 0x40},

	//MIPI Setting
	{0x70, 0x08},
	{0x71, 0x07},
	{0x72, 0x12},
	{0x73, 0x09},
	{0x74, 0x08},
	{0x75, 0x06},
	{0x76, 0x20},
	{0x77, 0x02},
	{0x78, 0x10},
	{0x79, 0x09},
	{0x7a, 0x00},
	{0x7b, 0x00},
	{0x7c, 0x00},
	{0x7d, 0x0f},

	//Window 1600*1200：从中心取
	{0xca, 0x60},
	{0xcb, 0x40},
	{0xcc, 0x04},
	{0xcd, 0x44},
	{0xce, 0x04},
	{0xcf, 0xb4},

	{0x6a, 0x0f},//Global gain
	{0x6b, 0x04},
	{0x6c, 0xe1},//{0x6b,0x6c}:int_time
	{0x6d, 0x0f},

	//Stream on
	{0x01, 0x4b},
	{0xf3, 0x00},
	{0xd0, 0x00},
	{0x01, 0x43},
};

static struct regval_list sensor_480p120_regs[] = {
	/*system*/

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
 * if not support the follow function , retrun -EINVAL
 */

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	struct sensor_info *info = to_state(sd);
	int tmp_exp_val = exp_val / 16;

	sensor_dbg("exp_val:%d\n", exp_val);
	sensor_write(sd, 0x6b, (tmp_exp_val >> 8) & 0xFF);
	sensor_write(sd, 0x6c, (tmp_exp_val & 0xFF));
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

static int setSensorGain(struct v4l2_subdev *sd, int gain)
{
#define ISP_BASE_GAIN		16
#define SENSOR_BASE_GAIN	0X400
#define SENSOR_MAX_GAIN		8192
	int i;
	data_type temperature_value;
	uint32_t temp_gain = 0;
	int32_t gain_index;
	int isp_gain = gain;
	int sensor_gain = 0;

	sensor_gain = isp_gain < ISP_BASE_GAIN ? ISP_BASE_GAIN : isp_gain;
	sensor_gain = sensor_gain * SENSOR_BASE_GAIN / ISP_BASE_GAIN;

	if (SENSOR_MAX_GAIN < sensor_gain)
		sensor_gain = SENSOR_MAX_GAIN;

	uint16_t BF2257CS_AGC_Param[][2] = {
		{1.0000*1024, 15},
		{1.0625*1024, 16},
		{1.1250*1024, 17},
		{1.1875*1024, 18},
		{1.2500*1024, 19},
		{1.3125*1024, 20},
		{1.3750*1024, 21},
		{1.4375*1024, 22},
		{1.5000*1024, 23},
		{1.5625*1024, 24},
		{1.6250*1024, 25},
		{1.6875*1024, 26},
		{1.7500*1024, 27},
		{1.8125*1024, 28},
		{1.8750*1024, 29},
		{1.9375*1024, 30},
		{2.0000*1024, 31},
		{2.0625*1024, 32},
		{2.1250*1024, 33},
		{2.1875*1024, 34},
		{2.2500*1024, 35},
		{2.3125*1024, 36},
		{2.3750*1024, 37},
		{2.4375*1024, 38},
		{2.5000*1024, 39},
		{2.5625*1024, 40},
		{2.6250*1024, 41},
		{2.6875*1024, 42},
		{2.7500*1024, 43},
		{2.8125*1024, 44},
		{2.8750*1024, 45},
		{2.9375*1024, 46},
		{3.0000*1024, 47},
		{3.0625*1024, 48},
		{3.1250*1024, 49},
		{3.1875*1024, 50},
		{3.2500*1024, 51},
		{3.3125*1024, 52},
		{3.3750*1024, 53},
		{3.4375*1024, 54},
		{3.5000*1024, 55},
		{3.5625*1024, 56},
		{3.6250*1024, 57},
		{3.6875*1024, 58},
		{3.7500*1024, 59},
		{3.8125*1024, 60},
		{3.8750*1024, 61},
		{3.9375*1024, 62},
		{4.0000*1024, 63},
		{4.0625*1024, 64},
		{4.1250*1024, 65},
		{4.1875*1024, 66},
		{4.2500*1024, 67},
		{4.3125*1024, 68},
		{4.3750*1024, 69},
		{4.4375*1024, 70},
		{4.5000*1024, 71},
		{4.5625*1024, 72},
		{4.6250*1024, 73},
		{4.6875*1024, 74},
		{4.7500*1024, 75},
		{4.8125*1024, 76},
		{4.8750*1024, 77},
		{4.9375*1024, 78},
		{5.0000*1024, 79},
		{5.0625*1024, 80},
		{5.1250*1024, 81},
		{5.1875*1024, 82},
		{5.2500*1024, 83},
		{5.3125*1024, 84},
		{5.3750*1024, 85},
		{5.4375*1024, 86},
		{5.5000*1024, 87},
		{5.5625*1024, 88},
		{5.6250*1024, 89},
		{5.6875*1024, 90},
		{5.7500*1024, 91},
		{5.8125*1024, 92},
		{5.8750*1024, 93},
		{5.9375*1024, 94},
		{6.0000*1024, 95},
		{6.0625*1024, 96},
		{6.1250*1024, 97},
		{6.1875*1024, 98},
		{6.2500*1024, 99},
		{6.3125*1024, 100},
		{6.3750*1024, 101},
		{6.4375*1024, 102},
		{6.5000*1024, 103},
		{6.5625*1024, 104},
		{6.6250*1024, 105},
		{6.6875*1024, 106},
		{6.7500*1024, 107},
		{6.8125*1024, 108},
		{6.8750*1024, 109},
		{6.9375*1024, 110},
		{7.0000*1024, 111},
		{7.0625*1024, 112},
		{7.1250*1024, 113},
		{7.1875*1024, 114},
		{7.2500*1024, 115},
		{7.3125*1024, 116},
		{7.3750*1024, 117},
		{7.4375*1024, 118},
		{7.5000*1024, 119},
		{7.5625*1024, 120},
		{7.6250*1024, 121},
		{7.6875*1024, 122},
		{7.7500*1024, 123},
		{7.8125*1024, 124},
		{7.8750*1024, 125},
		{7.9375*1024, 126},
		{8.0000*1024, 127},

	};

	if (sensor_gain < SENSOR_BASE_GAIN)
		sensor_gain = SENSOR_BASE_GAIN;
	if (sensor_gain > SENSOR_MAX_GAIN)
		sensor_gain = SENSOR_MAX_GAIN;

	uint32_t total_cnt = (sizeof(BF2257CS_AGC_Param)/sizeof(BF2257CS_AGC_Param[0]));
	for (gain_index = 0; gain_index < total_cnt -1; gain_index++) {
		if ((sensor_gain >= BF2257CS_AGC_Param[gain_index][0]) && (sensor_gain <= BF2257CS_AGC_Param[gain_index+1][0])) {
			if ((sensor_gain-BF2257CS_AGC_Param[gain_index][0]) <= (BF2257CS_AGC_Param[gain_index+1][0]-sensor_gain))
				temp_gain = BF2257CS_AGC_Param[gain_index][1];
			else
				temp_gain = BF2257CS_AGC_Param[gain_index+1][1];
			break;
		}
	}

	sensor_dbg("BF2257CS_AGC_Param[gain_index][1] = 0x%x, temp_gain = 0x%x, sensor_gain = 0x%x, total_cnt = %d\n",
			BF2257CS_AGC_Param[gain_index][1], temp_gain, sensor_gain, total_cnt);

	sensor_write(sd, 0x6a, temp_gain);

	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);

	if (gain_val == info->gain) {
		return 0;
	}

	sensor_dbg("gain_val:%d\n", gain_val);
	setSensorGain(sd, gain_val);

	info->gain = gain_val;

	return 0;
}

static int bf2257cs_sensor_vts;
static int sensor_s_exp_gain(struct v4l2_subdev *sd,
		struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	int shutter = 0, frame_length = 0;
	struct sensor_info *info = to_state(sd);

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < (1 * 16)) {
		gain_val = 16;
	}

	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	shutter = exp_val >> 4;
#ifdef CONFIG_VIDEO_SUNXI_VIN_SPECIAL
	if (shutter > bf2257cs_sensor_vts - 16) {
		frame_length = shutter + 16;
		if (abs(info->act_fps - (info->current_wins->vts * info->current_wins->fps_fixed / frame_length)) > 2) {
			info->act_fps = info->current_wins->vts * info->current_wins->fps_fixed / frame_length;
			if (info->sensor_fps_chenge_callback)
				info->sensor_fps_chenge_callback(info->act_fps);
			sensor_print("act fps is %d\n", info->act_fps);
		}
	} else {
		frame_length = bf2257cs_sensor_vts;
		if (info->act_fps != info->current_wins->fps_fixed) {
			info->act_fps = info->current_wins->fps_fixed;
			if (info->sensor_fps_chenge_callback)
				info->sensor_fps_chenge_callback(info->act_fps);
			sensor_print("act fps_fixed is %d\n", info->act_fps);
		}
	}
#else
	if (shutter > bf2257cs_sensor_vts - 16)
		frame_length = shutter + 16;
	else
		frame_length = bf2257cs_sensor_vts;
#endif
	sensor_dbg("frame_length = %d\n", frame_length);
	//sensor_write(sd, 0x07, (frame_length - bf2257cs_sensor_vts) & 0xff);
	//sensor_write(sd, 0x08, (frame_length - bf2257cs_sensor_vts) >> 8);

	//sensor_write(sd, 0x22, frame_length & 0xff);
	//sensor_write(sd, 0x23, frame_length >> 8);
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

static int sensor_flip_status;
static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	//sensor_read(sd, 0x00, &get_value);
	get_value = sensor_flip_status;
	sensor_dbg("ready to vflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x04;
		sensor_flip_status |= 0x04;
	} else {
		set_value = get_value & 0xFB;
		sensor_flip_status &= 0xFB;
	}
	sensor_write(sd, 0x00, set_value);
	//usleep_range(80000, 100000);
	//sensor_read(sd, 0x00, &get_value);
	//sensor_dbg("after vflip, regs_data = 0x%x, sensor_flip_status = %d\n",
	//			get_value, sensor_flip_status);

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	//sensor_read(sd, 0x00, &get_value);
	get_value = sensor_flip_status;
	sensor_dbg("ready to hflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x08;
		sensor_flip_status |= 0x08;
	} else {
		set_value = get_value & 0xF7;
		sensor_flip_status &= 0xF7;
	}
	sensor_write(sd, 0x00, set_value);
	//usleep_range(80000, 100000);
	//sensor_read(sd, 0x00, &get_value);
	//sensor_dbg("after hflip, regs_data = 0x%x, sensor_flip_status = %d\n",
	//			get_value, sensor_flip_status);

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
	*code = MEDIA_BUS_FMT_SBGGR10_1X10; // bf2257cs support change the rgb format by itself

	return 0;
}


/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		cci_unlock(sd);
		break;

	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;

	case PWR_ON:
		sensor_dbg("PWR_ON!\n");
		cci_lock(sd);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(1000, 1200);
		vin_gpio_set_status(sd, PWDN, 1);
		//vin_gpio_set_status(sd, RESET, 1);
		//vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		//vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);

		vin_set_pmu_channel(sd, IOVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		//vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		//usleep_range(10000, 12000);
		cci_unlock(sd);
	break;

	case PWR_OFF:
		sensor_dbg("PWR_OFF!do nothing\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
		//vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		//vin_set_pmu_channel(sd, CMBCSI, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		//vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		cci_unlock(sd);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{

	sensor_dbg("%s: val=%d\n", __func__);
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
#if !defined CONFIG_VIN_INIT_MELIS
	data_type rdval;
	int eRet;
	int times_out = 3;
	do {
		eRet = sensor_read(sd, ID_REG_HIGH, &rdval);
		sensor_dbg("eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval, times_out);
		usleep_range(200, 220);
		times_out--;
	} while (eRet < 0  &&  times_out > 0);

	sensor_read(sd, ID_REG_HIGH, &rdval);
	sensor_dbg("ID_VAL_HIGH = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;

	sensor_read(sd, ID_REG_LOW, &rdval);
	sensor_dbg("ID_VAL_LOW = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	sensor_dbg("Done!\n");
#endif
	return 0;
}

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed    = 0;
	info->width        = 1600;
	info->height       = 1200;
	info->hflip        = 0;
	info->vflip        = 0;
	info->gain         = 0;
	info->exp          = 0;

	info->tpf.numerator      = 1;
	info->tpf.denominator    = 30;	/* 30fps */
	sensor_flip_status = 0;
	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins,
					sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
		//	case VIDIOC_VIN_SENSOR_SET_FPS:
		//		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
		//		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
	case VIDIOC_VIN_SET_IR:
		sensor_set_ir(sd, (struct ir_switch *)arg);
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
		.desc      = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, /*.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
		.regs      = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp       = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */

static struct sensor_win_size sensor_win_sizes[] = {


	{
		.width		= 1600,
		.height 	= 1200,//1080,
		.hoffset	= 0,//0,
		.voffset	= 0,//0,
		.hts		= 1796,
		.vts		= 1250,
		.pclk       = 67.2 * 1000 * 1000,
		.mipi_bps   = 67.2 * 1000 * 1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1250 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs       = sensor_1600x1200_30_regs,
		.regs_size  = ARRAY_SIZE(sensor_1600x1200_30_regs),
		.set_size   = NULL,
	},

};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
		struct v4l2_mbus_config *cfg)
{
	//struct sensor_info *info = to_state(sd);

	cfg->type  = V4L2_MBUS_CSI2;
	cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;

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
	case V4L2_CID_HFLIP:
		return sensor_s_hflip(sd, ctrl->val);
	case V4L2_CID_VFLIP:
		return sensor_s_vflip(sd, ctrl->val);
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	__maybe_unused struct sensor_exp_gain exp_gain;

	ret = sensor_write_array(sd, sensor_default_regs,
			ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

#if defined CONFIG_VIN_INIT_MELIS
	if (info->preview_first_flag) {
		info->preview_first_flag = 0;
	} else {
		if (wsize->regs)
			sensor_write_array(sd, wsize->regs, wsize->regs_size);
		if (info->exp && info->gain) {
			exp_gain.exp_val = info->exp;
			exp_gain.gain_val = info->gain;
		} else {
			exp_gain.exp_val = 6000;
			exp_gain.gain_val = 32;
		}
		sensor_s_exp_gain(sd, &exp_gain);
	}
#else
	if (wsize->regs) {
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
	}
#endif

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	data_type get_value = 0;
	sensor_read(sd, 0x00, &get_value);
	sensor_flip_status = get_value;
	bf2257cs_sensor_vts = wsize->vts;
	sensor_dbg("bf2257cs_sensor_vts = %d\n", bf2257cs_sensor_vts);

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width,
			wsize->height);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_dbg("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
			info->current_wins->width, info->current_wins->height,
			info->current_wins->fps_fixed, info->fmt->mbus_code);

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
		.addr_width = CCI_BITS_8,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_8,
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

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			256 * 1600, 1, 1 * 1600);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1,
			65536 * 16, 1, 1);

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

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
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
	//info->combo_mode = CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->preview_first_flag = 1;
	info->exp = 0;
	info->gain = 0;
	info->first_power_flag = 1;
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

#ifdef CONFIG_SUNXI_FASTBOOT
subsys_initcall_sync(init_sensor);
#else
module_init(init_sensor);
#endif
module_exit(exit_sensor);
