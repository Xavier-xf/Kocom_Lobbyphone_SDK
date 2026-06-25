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
MODULE_DESCRIPTION("A low-level driver for bf2253l sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR  0x2253

/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0xfc
#define ID_REG_LOW		0xfd
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)
#define SENSOR_FRAME_RATE 20

/*
 * The bf2253l i2c address
 */
#define I2C_ADDR 0xda//0xdc

#define SENSOR_NUM 0x2
#define SENSOR_NAME "bf2253l_mipi"
#define SENSOR_NAME_2 "bf2253l_mipi_2"


#define CONTINUOUS_CLOCK	0

#define SIZE_576X1024		0
#define SIZE_1600X1200		1
/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {
#if 0
	//;==================INI==================
	//Product Ver:VAI01
	//;;Output Detail:
	//;;XCLK:24 MHz
	//;;PCLK:66 MHz
	//;;MipiCLK:660 MHz
	//;;FrameW:1780
	//;;FrameH:1236

	//{0xf2, 0x01},/0xf2[0]:1,reset
	{0xe1, 0x06},
	{0xe2, 0x06},
	{0xe3, 0x0e},
	{0xe4, 0x40},
	{0xe5, 0x67},
	{0xe6, 0x02},
	{0xe8, 0x84},
	{0x01, 0x14},
	{0x03, 0x98},
	{0x27, 0x21},
	{0x29, 0x20},
	{0x59, 0x10},
	{0x5a, 0x10},
	{0x5c, 0x11},
	{0x5d, 0x73},
	{0x6a, 0x2f},//global gain
	{0x6b, 0x07},
	{0x6c, 0xa0},//{0x6b,0x6c}int time
	{0x6f, 0x10},
	{0x70, 0x08},
	{0x71, 0x05},
	{0x72, 0x10},
	{0x73, 0x08},
	{0x74, 0x05},
	{0x75, 0x06},
	{0x76, 0x20},
	{0x77, 0x03},
	{0x78, 0x0e},
	{0x79, 0x08},
#if CONTINUOUS_CLOCK
	{0x7d, 0x0e},
#else
	//{0x7d,0x0f},
#endif
	//slave frame sync
	//{0xe7,0xc9},
	//{0x02,0x64},
	//{0x03,0xaf},

	//stream on
	{0x01, 0x1c},
	{0xe0, 0x00},
	{0x56, 0x10},
	{0x01, 0x14},


	//{0xe0,0x01},//stream off
	//{0x02,0x24},//stream off for slave frame sync
	//{0x03,0x98},//stream off for slave frame sync
#else
	//BF2253L_RAW10_V4.1.2_2022505
	//XCLK:24M;  MIPICLK:660M  PCLK(RAW10):66M
	//行长：1780  帧长：1236
	//Max fps:30fps
	//1608*1208
	{0xe1, 0x06},
	{0xe2, 0x06},
	{0xe3, 0x0e},
	{0xe4, 0x40},
	{0xe5, 0x67},
	{0xe6, 0x02},
	{0xe8, 0x84},
	{0x01, 0x14},
	{0x03, 0x98},
	{0x27, 0x21},
	{0x29, 0x20},
	{0x59, 0x10},
	{0x5a, 0x10},
	{0x5c, 0x11},
	{0x5d, 0x73},
	{0x6a, 0x0f},//global gain
	{0x6b, 0x04},
	{0x6c, 0x59},//{0x6b,0x6c}int time
	{0x6f, 0x10},
	{0x70, 0x08},
	{0x71, 0x05},
	{0x72, 0x10},
	{0x73, 0x08},
	{0x74, 0x05},
	{0x75, 0x06},
	{0x76, 0x20},
	{0x77, 0x03},
	{0x78, 0x0e},
	{0x79, 0x08},
#if CONTINUOUS_CLOCK
	{0x7d, 0x0e},
#else
	//{0x7d,0x0f},
#endif
	//window 1600*1200
	/* {0xca,0x60},
	{0xcb,0x40},
	{0xcc,0x00},
	{0xcd,0x40},
	{0xce,0x00},
	{0xcf,0xB0},

	//power up
	{0x01,0x1c},
	{0xe0,0x00},
	{0x56,0x10},
	{0x01,0x14}, */
#endif
};

static struct regval_list sensor_1600x1200_15_regs[] = {
    /*  1928*1088@15fps */
	{0xca, 0x60},
	{0xcb, 0x40},
	{0xcc, 0x00},
	{0xcd, 0x40},
	{0xce, 0x00},
	{0xcf, 0xB0},

	//power up
	{0x01, 0x1c},
	{0xe0, 0x00},
	{0x56, 0x10},
	{0x01, 0x14},
};

static struct regval_list sensor_576x1024_15_regs[] = {
	{0xca, 0x42},
	{0xcb, 0x40},
	{0xcc, 0x04},
	{0xcd, 0x44},
	{0xce, 0x5c},
	{0xcf, 0x5c},

	//power up
	{0x01, 0x1c},
	{0xe0, 0x00},
	{0x56, 0x10},
	{0x01, 0x14},
};

static struct regval_list sensor_1600x1200_20_regs[] = {
	/*  1928*1088@20fps */
	{0xca, 0x60},
	{0xcb, 0x40},
	{0xcc, 0x00},
	{0xcd, 0x40},
	{0xce, 0x00},
	{0xcf, 0xB0},

	//power up
	{0x01, 0x1c},
	{0xe0, 0x00},
	{0x56, 0x10},
	{0x01, 0x14},
};

static struct regval_list sensor_576x1024_20_regs[] = {
	{0xca, 0x42},
	{0xcb, 0x40},
	{0xcc, 0x04},
	{0xcd, 0x44},
	{0xce, 0x5c},
	{0xcf, 0x5c},

	//power up
	{0x01, 0x1c},
	{0xe0, 0x00},
	{0x56, 0x10},
	{0x01, 0x14},
};

static struct regval_list sensor_1600x1200_25_regs[] = {
	/*  1928*1088@25fps */
	{0xca, 0x60},
	{0xcb, 0x40},
	{0xcc, 0x00},
	{0xcd, 0x40},
	{0xce, 0x00},
	{0xcf, 0xB0},

	//power up
	{0x01, 0x1c},
	{0xe0, 0x00},
	{0x56, 0x10},
	{0x01, 0x14},
};

static struct regval_list sensor_576x1024_25_regs[] = {
	{0xca, 0x42},
	{0xcb, 0x40},
	{0xcc, 0x04},
	{0xcd, 0x44},
	{0xce, 0x5c},
	{0xcf, 0x5c},

	//power up
	{0x01, 0x1c},
	{0xe0, 0x00},
	{0x56, 0x10},
	{0x01, 0x14},
};

static struct regval_list sensor_1600x1200_30_regs[] = {
	{0xca, 0x60},
	{0xcb, 0x40},
	{0xcc, 0x00},
	{0xcd, 0x40},
	{0xce, 0x00},
	{0xcf, 0xB0},

	//power up
	{0x01, 0x1c},
	{0xe0, 0x00},
	{0x56, 0x10},
	{0x01, 0x14},
};

static struct regval_list sensor_576x1024_30_regs[] = {
	{0xca, 0x42},
	{0xcb, 0x40},
	{0xcc, 0x04},
	{0xcd, 0x44},
	{0xce, 0x5c},
	{0xcf, 0x5c},

	//power up
	{0x01, 0x1c},
	{0xe0, 0x00},
	{0x56, 0x10},
	{0x01, 0x14},
};

static struct regval_list sensor_480p120_regs[] = {
	/*system*/

};

#if 0
static struct regval_list sensor_240p120_regs[] = {
    /*  320x240@120fps */

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
#define SENSOR_MAX_GAIN		10856
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

	uint16_t BF2253L_AGC_Param[][2] = {
		 {1024, 15},
		 {1088, 16},
		 {1150, 17},
		 {1214, 18},
		 {1277, 19},
		 {1342, 20},
		 {1403, 21},
		 {1468, 22},
		 {1527, 23},
		 {1594, 24},
		 {1652, 25},
		 {1716, 26},
		 {1775, 27},
		 {1839, 28},
		 {1897, 29},
		 {1960, 30},
		 {2023, 31},
		 {2145, 32},
		 {2273, 33},
		 {2395, 34},
		 {2519, 35},
		 {2641, 36},
		 {2765, 37},
		 {2885, 38},
		 {3006, 39},
		 {3131, 40},
		 {3249, 41},
		 {3373, 42},
		 {3493, 43},
		 {3610, 44},
		 {3730, 45},
		 {3846, 46},
		 {3962, 47},
		 {4206, 48},
		 {4441, 49},
		 {4678, 50},
		 {4906, 51},
		 {5146, 52},
		 {5371, 53},
		 {5601, 54},
		 {5831, 55},
		 {6046, 56},
		 {6259, 57},
		 {6493, 58},
		 {6713, 59},
		 {6928, 60},
		 {7149, 61},
		 {7359, 62},
		 {7564, 63},
		 {7789, 64},
		 {8000, 65},
		 {8216, 66},
		 {8439, 67},
		 {8648, 68},
		 {8859, 69},
		 {9065, 70},
		 {9280, 71},
		 {9482, 72},
		 {9665, 73},
		 {9891, 74},
		 {10073, 75},
		 {10273, 76},
		 {10474, 77},
		 {10663, 78},
		 {10856, 79}
	};

	if (sensor_gain < SENSOR_BASE_GAIN)
		sensor_gain = SENSOR_BASE_GAIN;
	if (sensor_gain > SENSOR_MAX_GAIN)
		sensor_gain = SENSOR_MAX_GAIN;

	uint32_t total_cnt = (sizeof(BF2253L_AGC_Param)/sizeof(BF2253L_AGC_Param[0]));
	for (gain_index = 0; gain_index < total_cnt -1; gain_index++) {
		if ((sensor_gain >= BF2253L_AGC_Param[gain_index][0]) && (sensor_gain <= BF2253L_AGC_Param[gain_index+1][0])) {
			if ((sensor_gain-BF2253L_AGC_Param[gain_index][0]) <= (BF2253L_AGC_Param[gain_index+1][0]-sensor_gain))
				temp_gain = BF2253L_AGC_Param[gain_index][1];
			else
				temp_gain = BF2253L_AGC_Param[gain_index+1][1];
		 break;
		}
	}

	sensor_dbg("BF2253L_AGC_Param[gain_index][1] = 0x%x, temp_gain = 0x%x, sensor_gain = 0x%x, total_cnt = %d\n",
		BF2253L_AGC_Param[gain_index][1], temp_gain, sensor_gain, total_cnt);

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

static int bf2253l_sensor_vts;
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
	if (shutter > bf2253l_sensor_vts - 16) {
		frame_length = shutter + 16;
		if (abs(info->act_fps - (info->current_wins->vts * info->current_wins->fps_fixed / frame_length)) > 2) {
			info->act_fps = info->current_wins->vts * info->current_wins->fps_fixed / frame_length;
			if (info->sensor_fps_chenge_callback)
				info->sensor_fps_chenge_callback(info->act_fps);
			sensor_print("act fps is %d\n", info->act_fps);
		}
	} else {
		frame_length = bf2253l_sensor_vts;
		if (info->act_fps != info->current_wins->fps_fixed) {
			info->act_fps = info->current_wins->fps_fixed;
			if (info->sensor_fps_chenge_callback)
				info->sensor_fps_chenge_callback(info->act_fps);
			sensor_print("act fps_fixed is %d\n", info->act_fps);
		}
	}
#else
	if (shutter > bf2253l_sensor_vts - 16)
		frame_length = shutter + 16;
	else
		frame_length = bf2253l_sensor_vts;
#endif
	sensor_dbg("frame_length = %d\n", frame_length);
	//sensor_write(sd, 0x07, (frame_length-1236) & 0xff);
	//sensor_write(sd, 0x08, (frame_length-1236) >> 8);

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
	*code = MEDIA_BUS_FMT_SBGGR10_1X10; // bf2253l support change the rgb format by itself

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
		vin_gpio_set_status(sd, RESET, 1);
		//vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		//vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		//vin_set_pmu_channel(sd, CMBCSI, ON);
		vin_set_pmu_channel(sd, IOVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
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
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
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
	info->tpf.denominator    = 20;	/* 30fps */
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
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
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
#if SIZE_1600X1200
	{
		.width		= 1600,
		.height 	= 1200,//1080,
		.hoffset	= 0,//0,
		.voffset	= 0,//0,
		.hts		= 1780,
		.vts		= 1236,
		.pclk       = 66040000,
		.mipi_bps   = 661 * 1000 * 1000,
		.fps_fixed	= 15,
		.bin_factor = 1,
		.intg_min	= 1 << 4,
		.intg_max	= (1125 - 16) << 4,
		.gain_min	= 1 << 4,
		.gain_max	= 110 << 4,
		.regs		= sensor_1600x1200_15_regs,
		.regs_size	= ARRAY_SIZE(sensor_1600x1200_15_regs),
		.set_size	= NULL,
	},
	{
		.width		= 1600,
		.height 	= 1200,//1080,
		.hoffset	= 0,//0,
		.voffset	= 0,//0,
		.hts		= 1780,
		.vts		= 1236,
		.pclk       = 66040000,
		.mipi_bps   = 661 * 1000 * 1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1125 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs       = sensor_1600x1200_20_regs,
		.regs_size  = ARRAY_SIZE(sensor_1600x1200_20_regs),
		.set_size   = NULL,
	},
	{
		.width		= 1600,
		.height 	= 1200,//1080,
		.hoffset	= 0,//0,
		.voffset	= 0,//0,
		.hts		= 1780,
		.vts		= 1236,
		.pclk       = 66040000,
		.mipi_bps   = 661 * 1000 * 1000,
		.fps_fixed  = 25,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1125 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs	    = sensor_1600x1200_25_regs,
		.regs_size  = ARRAY_SIZE(sensor_1600x1200_25_regs),
		.set_size   = NULL,
	},
	{
		.width		= 1600,
		.height 	= 1200,//1080,
		.hoffset	= 0,//0,
		.voffset	= 0,//0,
		.hts		= 1780,
		.vts		= 1236,
		.pclk       = 66040000,
		.mipi_bps   = 661 * 1000 * 1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1236 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs       = sensor_1600x1200_30_regs,
		.regs_size  = ARRAY_SIZE(sensor_1600x1200_30_regs),
		.set_size   = NULL,
	},
#endif

#if SIZE_576X1024
	{
		.width		= 576,
		.height 	= 1024,
		.hoffset	= 0,//0,
		.voffset	= 0,//0,
		.hts		= 1780,
		.vts		= 1236,
		.pclk       = 66040000,
		.mipi_bps   = 661 * 1000 * 1000,
		.fps_fixed	= 15,
		.bin_factor = 1,
		.intg_min	= 1 << 4,
		.intg_max	= (1125 - 16) << 4,
		.gain_min	= 1 << 4,
		.gain_max	= 110 << 4,
		.regs		= sensor_576x1024_15_regs,
		.regs_size	= ARRAY_SIZE(sensor_576x1024_15_regs),
		.set_size	= NULL,
	},
	{
		.width		= 576,
		.height 	= 1024,
		.hoffset	= 0,//0,
		.voffset	= 0,//0,
		.hts		= 1780,
		.vts		= 1236,
		.pclk       = 66040000,
		.mipi_bps   = 661 * 1000 * 1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1125 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs       = sensor_576x1024_20_regs,
		.regs_size  = ARRAY_SIZE(sensor_576x1024_20_regs),
		.set_size   = NULL,
	},
	{
		.width		= 576,
		.height 	= 1024,
		.hoffset	= 0,//0,
		.voffset	= 0,//0,
		.hts		= 1780,
		.vts		= 1236,
		.pclk       = 66040000,
		.mipi_bps   = 661 * 1000 * 1000,
		.fps_fixed  = 25,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1125 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs	    = sensor_576x1024_25_regs,
		.regs_size  = ARRAY_SIZE(sensor_576x1024_25_regs),
		.set_size   = NULL,
	},
	{
		.width		= 576,
		.height 	= 1024,
		.hoffset	= 0,//0,
		.voffset	= 0,//0,
		.hts		= 1780,
		.vts		= 1236,
		.pclk       = 66040000,
		.mipi_bps   = 661 * 1000 * 1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (1236 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs       = sensor_576x1024_30_regs,
		.regs_size  = ARRAY_SIZE(sensor_576x1024_30_regs),
		.set_size   = NULL,
	},
#endif

#if 0
	{
		.width      = 640,
		.height     = 480,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 1696,
		.vts        = 525,
		.pclk       = 106848000,
		.mipi_bps   = 648 * 1000 * 1000,
		.fps_fixed  = 120,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (525 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs       = sensor_480p120_regs,
		.regs_size  = ARRAY_SIZE(sensor_480p120_regs),
		.set_size   = NULL,
	},

	{
		.width      = 320,
		.height     = 240,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2234,
		.vts        = 277,
		.pclk       = 74250000,
		.mipi_bps   = 297 * 1000 * 1000,
		.fps_fixed  = 120,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = (277 - 16) << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 110 << 4,
		.regs       = sensor_240p120_regs,
		.regs_size  = ARRAY_SIZE(sensor_240p120_regs),
		.set_size   = NULL,
	},
#endif
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
	sensor_flip_status = 0;
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

	bf2253l_sensor_vts = wsize->vts;
	sensor_dbg("bf2253l_sensor_vts = %d\n", bf2253l_sensor_vts);

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
#if CONTINUOUS_CLOCK
	info->stream_seq = MIPI_BEFORE_SENSOR;
#else
	info->stream_seq = SENSOR_BEFORE_MIPI;
	//info->stream_seq = MIPI_BEFORE_SENSOR;
#endif
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
