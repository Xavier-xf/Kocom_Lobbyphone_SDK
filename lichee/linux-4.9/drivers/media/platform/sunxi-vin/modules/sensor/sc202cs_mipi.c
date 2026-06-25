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
MODULE_DESCRIPTION("A low-level driver for SC202CS sensors");
MODULE_LICENSE("GPL");

//define the registers
#define EXP_HIGH		0xff
#define EXP_MID			0x03
#define EXP_LOW			0x04
#define GAIN_HIGH		0xff
#define GAIN_LOW		0x24
/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0xf0
#define ID_REG_LOW		0xf1
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)

#ifdef sensor_dbg
#undef sensor_dbg
#define sensor_dbg(x, arg...) printk(KERN_DEBUG "[%s_info]" x, SENSOR_NAME, ##arg)
#endif

#define SC202CS 1
#if (SC202CS == 1)
#define I2C_ADDR 0x6C
#define V4L2_IDENT_SENSOR 0xeb52
#define SENSOR_WIDTH UXGA_WIDTH
#define SENSOR_HEIGHT UXGA_HEIGHT
#define VTS 1250
#define SENSOR_FRAME_RATE 30
#else
#define I2C_ADDR 0x60
#define V4L2_IDENT_SENSOR 0x0031
#define SENSOR_WIDTH VGA_WIDTH
#define SENSOR_HEIGHT VGA_HEIGHT
#define VTS 0x0aac
#define SENSOR_FRAME_RATE 120
#endif

#define EXPOSURE_MIN 6
#define EXPOSURE_MAX (VTS - 6)
#define EXPOSURE_STEP 1
#define EXPOSURE_DEFAULT 0x0148

#define GAIN_MIN 0x00
#define GAIN_MAX 0xF8
#define GAIN_STEP 1
#define GAIN_DEFAULT 20

#define MCLK              (24*1000*1000)

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc202cs_mipi"
#define SENSOR_NAME_2 "sc202cs_mipi_2"

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

#if (SC202CS == 1)
// 24Minput_720Mbps_1lane_10bit_1600x1200_30fps
static struct regval_list sensor_normal_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36e9, 0x24},
	{0x301f, 0x01},
	{0x3301, 0xff},
	{0x3304, 0x68},
	{0x3306, 0x40},
	{0x3308, 0x08},
	{0x3309, 0xa8},
	{0x330b, 0xb0},
	{0x330c, 0x18},
	{0x330d, 0xff},
	{0x330e, 0x20},
	{0x331e, 0x59},
	{0x331f, 0x99},
	{0x3333, 0x10},
	{0x335e, 0x06},
	{0x335f, 0x08},
	{0x3364, 0x1f},
	{0x337c, 0x02},
	{0x337d, 0x0a},
	{0x338f, 0xa0},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x1f},
	{0x3393, 0xff},
	{0x3394, 0xff},
	{0x3395, 0xff},
	{0x33a2, 0x04},
	{0x33ad, 0x0c},
	{0x33b1, 0x20},
	{0x33b3, 0x38},
	{0x33f9, 0x40},
	{0x33fb, 0x48},
	{0x33fc, 0x0f},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x03},
	{0x34a7, 0x1f},
	{0x34a8, 0x38},
	{0x34a9, 0x30},
	{0x34ab, 0xb0},
	{0x34ad, 0xb0},
	{0x34f8, 0x1f},
	{0x34f9, 0x20},
	{0x3630, 0xa0},
	{0x3631, 0x92},
	{0x3632, 0x64},
	{0x3633, 0x43},
	{0x3637, 0x49},
	{0x363a, 0x85},
	{0x363c, 0x0f},
	{0x3650, 0x31},
	{0x3670, 0x0d},
	{0x3674, 0xc0},
	{0x3675, 0xa0},
	{0x3676, 0xa0},
	{0x3677, 0x92},
	{0x3678, 0x96},
	{0x3679, 0x9a},
	{0x367c, 0x03},
	{0x367d, 0x0f},
	{0x367e, 0x01},
	{0x367f, 0x0f},
	{0x3698, 0x83},
	{0x3699, 0x86},
	{0x369a, 0x8c},
	{0x369b, 0x94},
	{0x36a2, 0x01},
	{0x36a3, 0x03},
	{0x36a4, 0x07},
	{0x36ae, 0x0f},
	{0x36af, 0x1f},
	{0x36bd, 0x22},
	{0x36be, 0x22},
	{0x36bf, 0x22},
	{0x36d0, 0x01},
	{0x370f, 0x02},
	{0x3721, 0x6c},
	{0x3722, 0x8d},
	{0x3725, 0xc5},
	{0x3727, 0x14},
	{0x3728, 0x04},
	{0x37b7, 0x04},
	{0x37b8, 0x04},
	{0x37b9, 0x06},
	{0x37bd, 0x07},
	{0x37be, 0x0f},
	{0x3901, 0x02},
	{0x3903, 0x40},
	{0x3905, 0x8d},
	{0x3907, 0x00},
	{0x3908, 0x41},
	{0x391f, 0x41},
	{0x3933, 0x80},
	{0x3934, 0x02},
	{0x3937, 0x6f},
	{0x393a, 0x01},
	{0x393d, 0x01},
	{0x393e, 0xc0},
	{0x39dd, 0x41},
	{0x3e00, 0x00},
	{0x3e01, 0x4d},
	{0x3e02, 0xc0},
	{0x3e09, 0x00},
	{0x4509, 0x28},
	{0x450d, 0x61},
	{0x0100, 0x01},
};
#else
/* 640x480 RAW 120fps 24MHz */
static struct regval_list sensor_VGA_120fps_2lane_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},

	{0x3000, 0x00},
	{0x3001, 0x00},
	{0x300f, 0x0f},
	{0x3018, 0x33},
	{0x3019, 0x0c},
	{0x301c, 0x78},
	{0x3031, 0x0a},
	{0x3037, 0x20},
	{0x303f, 0x01},
	{0x320c, 0x03}, // hts=878
	{0x320d, 0x6e},
	// 120fps
	{0x320e, 0x02}, // vts=683
	{0x320f, 0xab},
	{0x3252, 0x02},
	{0x3253, 0xa6},

	{0x3220, 0x10},
	{0x3250, 0xc0},
	{0x3251, 0x02},
	{0x3254, 0x02},
	{0x3255, 0x07},
	{0x3304, 0x48},
	{0x3306, 0x38},
	{0x3309, 0x68},
	{0x330b, 0xe0},
	{0x330c, 0x18},
	{0x330f, 0x20},
	{0x3310, 0x10},
	{0x3314, 0x1e},
	{0x3315, 0x38},
	{0x3316, 0x40},
	{0x3317, 0x10},
	{0x3329, 0x34},
	{0x332d, 0x34},
	{0x332f, 0x38},
	{0x3335, 0x3c},
	{0x3344, 0x3c},
	{0x335b, 0x80},
	{0x335f, 0x80},
	{0x3366, 0x06},
	{0x3385, 0x31},
	{0x3387, 0x51},
	{0x3389, 0x01},
	{0x33b1, 0x03},
	{0x33b2, 0x06},
	{0x3621, 0xa4},
	{0x3622, 0x05},
	{0x3624, 0x47},
	{0x3630, 0x46},
	{0x3631, 0x48},
	{0x3633, 0x52},
	{0x3635, 0x18},
	{0x3636, 0x25},
	{0x3637, 0x89},
	{0x3638, 0x0f},
	{0x3639, 0x08},
	{0x363a, 0x00},
	{0x363b, 0x48},
	{0x363c, 0x06},
	{0x363d, 0x00},
	{0x363e, 0xf8},
	{0x3640, 0x00},
	{0x3641, 0x01},
	{0x36e9, 0x00},
	{0x36ea, 0x3b},
	{0x36eb, 0x0e},
	{0x36ec, 0x1e},
	{0x36ed, 0x33},
	{0x36f9, 0x00},
	{0x36fa, 0x3a},
	{0x36fc, 0x01},
	{0x3908, 0x91},
	{0x3d08, 0x01},
	{0x3e01, 0x14},
	{0x3e02, 0x80},
	{0x3e06, 0x0c},
	{0x4418, 0x08},
	{0x4419, 0x8e},
	{0x4500, 0x59},
	{0x4501, 0xc4},
	{0x4603, 0x00},
	{0x4809, 0x01},
	{0x4837, 0x37},
	{0x5011, 0x00},

	{0x0100, 0x01},
	// edlay 10ms
	{0x4418, 0x08},
	{0x4419, 0x8e},

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
	data_type explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);
	/*struct vin_md *vind = dev_get_drvdata(sd->v4l2_dev->dev);*/
	/*struct vin_core *vinc = vind->vinc[0];*/

	if (exp_val > EXPOSURE_MAX << 4)
		exp_val = EXPOSURE_MAX << 4;

	if (exp_val < 16)
		exp_val = 16;

	if (exp_val == info->exp) {
		return 0;
	}

#if (SC202CS == 1)
	exphigh = (unsigned char)(0xf & (exp_val >> 12)); // upper 4 bits
	expmid = (unsigned char)(0xff & (exp_val >> 4));  // middle 7 bits
	explow = (unsigned char)(0xf0 & (exp_val << 4));  // lower 4 bits
	sensor_write(sd, 0x3e02, explow);				  //[7:4]
	sensor_write(sd, 0x3e01, expmid);				  //[7:0]
	sensor_write(sd, 0x3e00, exphigh);				  //[3:0]
#else
	exphigh = (unsigned char)(exp_val >> 8);
	explow = (unsigned char)(exp_val & 0xFF);
	expmid = 0;
	sensor_write(sd, 0x3e01, exphigh);
	sensor_write(sd, 0x3e02, explow);
#endif

	sensor_dbg("%s():%d, exp_val = %d\n", __func__, __LINE__, exp_val);
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
	struct sensor_info *info = to_state(sd);
//sensor_dbg("%s():%d. info:%p, gain_val:%d\n", __func__, __LINE__, info, gain_val);
	if (gain_val == info->gain) {
		return 0;
	}
sensor_dbg("%s():%d. info:%p, gain_val:%d\n", __func__, __LINE__, info, gain_val);
	info->gain = gain_val;

	return 0;

	// if (gain_val < 1 * 16)
	// 	gain_val = 16;
	if (gain_val > 16 * 16 - 1)
		gain_val = 16 * 16 - 1;

	if (gain_val < 32) {
		sensor_write(sd, 0x3314, 0x1e);
		sensor_write(sd, 0x3317, 0x10);
	} else {
		sensor_write(sd, 0x3314, 0x4f);
		sensor_write(sd, 0x3317, 0x0f);
	}

	if (gain_val < 32) {
		sensor_write(sd, 0x3e08, 0x03);
		sensor_write(sd, 0x3e09, gain_val);
	} else if (gain_val >= 32 && gain_val < 64) {
		sensor_write(sd, 0x3e08, 0x07);
		sensor_write(sd, 0x3e09, gain_val >> 1);
	} else if (gain_val >= 64 && gain_val < 128) {
		sensor_write(sd, 0x3e08, 0x0f);
		sensor_write(sd, 0x3e09, gain_val >> 2);
	} else if (gain_val >= 128) {
		sensor_write(sd, 0x3e08, 0x1f);
		sensor_write(sd, 0x3e09, gain_val >> 3);
	}

	sensor_dbg("drv sensor_s_gain(%d)\n", gain_val);
	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
				 struct sensor_exp_gain *exp_gain)
{
	sensor_s_exp(sd, exp_gain->exp_val);
	sensor_s_gain(sd, exp_gain->gain_val);
	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	sensor_dbg("set hfilp=%d\n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	if (enable)
		set_value = get_value | 0x60;
	else
		set_value = get_value & 0x9f;
	sensor_write(sd, 0x3221, set_value);

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	sensor_dbg("set hfilp=%d\n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	if (enable)
		set_value = get_value | 0x06;
	else
		set_value = get_value & 0xf9;
	sensor_write(sd, 0x3221, set_value);

	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	*code = MEDIA_BUS_FMT_SRGGB10_1X10; // gc2053 support change the rgb format by itself
	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	data_type rdval;

	ret = sensor_read(sd, 0x0100, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == STBY_ON)
		ret = sensor_write(sd, 0x0100, rdval & 0xfe);
	else
		ret = sensor_write(sd, 0x0100, rdval | 0x01);

	return ret;
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
		sensor_s_sw_stby(sd, STBY_ON);
		cci_unlock(sd);
		break;

	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		sensor_s_sw_stby(sd, STBY_OFF);
		cci_unlock(sd);
		break;

#if (SC202CS == 1)
	case PWR_ON:
		sensor_dbg("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(30000, 32000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_dbg("PWR_OFF!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, AFVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		vin_gpio_set_status(sd, POWER_EN, 0);
		cci_unlock(sd);
		break;
#else
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);

		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, CAMERAVDD, ON);
		vin_set_pmu_channel(sd, IOVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_set_pmu_channel(sd, AVDD, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		// usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!\n");
		cci_lock(sd);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_set_mclk(sd, OFF);
		vin_set_pmu_channel(sd, CAMERAVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		vin_gpio_set_status(sd, RESET, 0);
		cci_unlock(sd);
		break;
#endif

	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{

	sensor_dbg("%s=%d\n", __func__, val);
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
	/* 快启过程已检测设备则不需要再次检测 */
	int ret = 0;
	unsigned int cnt = 0;
	unsigned int SENSOR_ID = 0;
	data_type rdval;
	for (cnt = 0; cnt < 5; cnt++) {
		ret |= sensor_read(sd, 0x3107, &rdval);
		SENSOR_ID |= (rdval << 8);
		ret |= sensor_read(sd, 0x3108, &rdval);
		SENSOR_ID |= (rdval);
		if (ret != 0)
			return ret;
		sensor_dbg("V4L2_IDENT_SENSOR = 0x%x\n", SENSOR_ID);
		if (SENSOR_ID == V4L2_IDENT_SENSOR)
			break;
	}
	if (cnt == 5)
		return -ENODEV;
	sensor_dbg("Done!\n");
#endif
	return 0;
}
static int m_autoexp;
static int sensor_dev_id;
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
	info->width        = SENSOR_WIDTH;
	info->height       = SENSOR_HEIGHT;
	info->hflip        = 0;
	info->vflip        = 0;
	info->gain         = 0;
	info->exp          = 0;

	info->tpf.numerator      = 1;
	info->tpf.denominator    = SENSOR_FRAME_RATE;
	m_autoexp = 0;
	sensor_dev_id = 0;

 #if defined CONFIG_VIN_INIT_MELIS
 #else
	info->preview_first_flag = 1;
 #endif

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
		if (m_autoexp == V4L2_EXPOSURE_MANUAL) {
			sensor_print("SENSOR_EXP_GAIN continue\n");
			break;
		} else {
			//sensor_dbg("SENSOR_EXP_GAIN set\n");
			ret = sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		}
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
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10, /*.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
		.regs      = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp       = 1
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/*
 * Then there is the issue of window sizes.  Try to capture the info here.
 */
#if (SC202CS == 1)
static struct sensor_win_size sensor_win_sizes[] = {
	{
		// 24Minput_720Mbps_1lane_10bit_1600x1200_30fps
		.width = 1600,	//
		.height = 1200, //
		.hoffset = 0,
		.voffset = 0,
		.hts = 2200,
		.vts = 1250,
		.pclk = 74250000,
		.mipi_bps = 371250000,
		.fps_fixed = 30,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (1250 - 8) << 4,
		.gain_min = 1 << 4,
		.gain_max = 128 << 4,
		.regs = sensor_normal_regs,
		.regs_size = ARRAY_SIZE(sensor_normal_regs),
		.set_size = NULL,
	},
};
#else
static struct sensor_win_size sensor_win_sizes[] = {
	{
		.width = SENSOR_WIDTH,
		.height = SENSOR_HEIGHT,
		.hoffset = 0,
		.voffset = 0,
		.hts = 878,
		.vts = 683,
		.pclk = 72 * 1000 * 1000,
		.mipi_bps = 360 * 1000 * 1000,
		.fps_fixed = SENSOR_FRAME_RATE,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (683 - 6) << 4,
		.gain_min = 1 << 4,
		.gain_max = 1440 << 4,
		.regs = sensor_VGA_120fps_2lane_regs,
		.regs_size = ARRAY_SIZE(sensor_VGA_120fps_2lane_regs),
		.set_size = NULL,
	},
};
#endif
#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
#if (SC202CS == 1)
	cfg->type  = V4L2_MBUS_CSI2;
	cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#else
	cfg->type  = V4L2_MBUS_CSI2;
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
	case V4L2_CID_EXPOSURE_AUTO:
		ctrl->val = m_autoexp;
		return 0;
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
	case V4L2_CID_EXPOSURE_AUTO:
		sensor_dbg("sensor_s_ctrl: V4L2_CID_EXPOSURE_AUTO=%d\n", ctrl->val);
		m_autoexp = ctrl->val;
		return 0;
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
		sensor_dbg("sensor_reg_init: first, break\n");
		info->preview_first_flag = 0;
	} else {
		sensor_dbg("sensor_reg_init: sensor_write_array\n");
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

	exp_gain.exp_val = EXPOSURE_DEFAULT;
	exp_gain.gain_val = GAIN_DEFAULT;
	sensor_s_exp_gain(sd, &exp_gain);

	sensor_dbg("init: w(%d), h(%d), exp(%d), gain(%d), vts(%d)\n",
			   wsize->width, wsize->height, info->exp, info->gain, wsize->vts);


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
		.addr_width = CCI_BITS_16,//CCI_BITS_8,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_16,//CCI_BITS_8,
		.data_width = CCI_BITS_8,
	}
};

static int sensor_init_controls(struct v4l2_subdev *sd,
			const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 5);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN,
							 GAIN_MIN, GAIN_MAX,
							 GAIN_STEP, GAIN_DEFAULT);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE,
							 EXPOSURE_MIN, EXPOSURE_MAX,
							 EXPOSURE_STEP, EXPOSURE_DEFAULT);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_EXPOSURE_AUTO,
						   V4L2_EXPOSURE_APERTURE_PRIORITY, 0,
						   V4L2_EXPOSURE_AUTO);

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

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
	info->exp = 0;
	info->gain = 0;

#if defined CONFIG_VIN_INIT_MELIS
	info->first_power_flag = 1;
	info->preview_first_flag = 1;
#endif
	sensor_print("probe sensor is %x\n", I2C_ADDR);

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
