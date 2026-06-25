/*
 * A V4L2 driver for sc1301cs Raw cameras.
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

#include <linux/proc_fs.h>

MODULE_AUTHOR("myf");
MODULE_DESCRIPTION("A low-level driver for sc1301cs sensors");
MODULE_LICENSE("GPL");

#define MCLK              (27*1000*1000)
#define V4L2_IDENT_SENSOR 0xc628

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

/*
 * The sc1301cs i2c address
 */
#define I2C_ADDR 0x6c

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc1301cs_mipi"
#define SENSOR_NAME_2 "sc1301cs_mipi_2"

struct proc_dir_entry *entry;
static struct v4l2_subdev *g_sd;

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_4208x2368_30fps_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36f9, 0x80},
	//{0x36ea, 0x07},
	{0x36ea, 0x05},

	{0x36eb, 0x0a},
	{0x36ec, 0x0b},
	{0x36ed, 0x24},
	//{0x36fa, 0x05},
	{0x36fa, 0x06},
	//{0x36fb, 0xb2},
	//{0x36fc, 0x10},
	//{0x36fd, 0x14},
	{0x36fd, 0x24},
	//{0x36e9, 0x00},
	{0x36e9, 0x04},
	{0x36f9, 0x04},

	{0x301f, 0x83},
	{0x302d, 0x20},
	{0x3106, 0x01},
	{0x3202, 0x01},
	{0x3203, 0x78},
	{0x3206, 0x0a},
	{0x3207, 0xc7},
	{0x320a, 0x09},
	{0x320b, 0x40},
	{0x320c, 0x07},
	{0x320d, 0x53},

	{0x320e, 0x09},
	{0x320f, 0x60},
	{0x3212, 0x00},
	{0x3213, 0x08},
	{0x325f, 0x42},
	{0x3301, 0x09},
	{0x3306, 0x28},
	{0x330b, 0x88},
	{0x330e, 0x40},
	{0x331e, 0x45},
	{0x331f, 0x65},
	{0x3333, 0x10},
	{0x3347, 0x07},
	{0x335d, 0x60},
	{0x3364, 0x57},
	{0x3367, 0x04},
	{0x3390, 0x01},
	{0x3391, 0x03},
	{0x3392, 0x07},
	{0x3393, 0x08},
	{0x3394, 0x0e},
	{0x3395, 0x1a},
	{0x33ad, 0x30},
	{0x33b1, 0x80},
	{0x33b3, 0x44},
	{0x341e, 0x00},
	{0x34a8, 0x40},
	{0x34a9, 0x20},
	{0x34ab, 0x84},
	{0x34ad, 0x84},
	{0x360f, 0x01},
	{0x3621, 0x68},
	{0x3622, 0xe8},
	{0x3635, 0x20},
	{0x3637, 0x23},
	{0x3638, 0xc7},
	{0x3639, 0xf4},
	{0x363a, 0x82},
	{0x363c, 0xc0},
	{0x363d, 0x40},
	{0x363e, 0x28},
	{0x3670, 0x4a},
	{0x3671, 0xe5},
	{0x3672, 0x83},
	{0x3673, 0x83},
	{0x3674, 0x10},
	{0x3675, 0x36},
	{0x3676, 0x3a},
	{0x367a, 0x03},
	{0x367b, 0x0f},
	{0x367c, 0x03},
	{0x367d, 0x07},
	{0x3690, 0x55},
	{0x3691, 0x65},
	{0x3692, 0x65},
	{0x3699, 0x82},
	{0x369a, 0x9f},
	{0x369b, 0xc0},
	{0x369c, 0x03},
	{0x369d, 0x07},
	{0x36a2, 0x03},
	{0x36a3, 0x07},
	{0x36b1, 0xd8},
	{0x36b2, 0x01},
	{0x3900, 0x1d},
	{0x3902, 0xc5},
	{0x391b, 0x81},
	{0x391c, 0x30},
	{0x391d, 0x19},
	{0x3948, 0x06},
	{0x394a, 0x08},
	{0x3952, 0x02},
	{0x396a, 0x07},
	{0x396c, 0x0e},
	{0x3e00, 0x01},
	{0x3e01, 0x2b},
	{0x3e02, 0x60},
	{0x3e20, 0x00},
	{0x4000, 0x00},
	{0x4001, 0x03},
	{0x4002, 0xc0},
	{0x4003, 0x00},
	{0x4004, 0x00},
	{0x4005, 0x00},
	{0x4006, 0x06},
	{0x4007, 0x24},
	{0x4008, 0x00},
	{0x4009, 0xc8},
	{0x4424, 0x02},
	{0x4506, 0x3c},
	{0x4509, 0x30},
	{0x4800, 0x24},
	{0x4837, 0x26},
	{0x5000, 0x0e},
	{0x5799, 0x06},
	{0x59e0, 0xd8},
	{0x59e1, 0x08},
	{0x59e2, 0x10},
	{0x59e3, 0x08},
	{0x59e4, 0x00},
	{0x59e5, 0x10},
	{0x59e6, 0x08},
	{0x59e7, 0x00},
	{0x59e8, 0x18},
	{0x59e9, 0x0c},
	{0x59ea, 0x04},
	{0x59eb, 0x18},
	{0x59ec, 0x0c},
	{0x59ed, 0x04},
	{0x0100, 0x01},
	{0x302d, 0x00},
};

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

static int sc1301cs_sensor_vts;
//static int sc1301cs_sensor_svr;
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);

	if (exp_val < 16) {
		exp_val = 16;
	}
	exphigh = (unsigned char) (0xf & (exp_val>>15));
	expmid = (unsigned char) (0xff & (exp_val>>7));
	explow = (unsigned char) (0xf0 & (exp_val<<1));

	sensor_write(sd, 0x3e02, explow);
	sensor_write(sd, 0x3e01, expmid);
	sensor_write(sd, 0x3e00, exphigh);
	sensor_dbg("sensor_set_exp = %d line Done!\n", exp_val);

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
	data_type anagain = 0x00;
	data_type gaindiglow = 0x80;
	data_type gaindighigh = 0x00;
	int gain_tmp;

	gain_tmp = gain_val << 3;
	if (gain_val < 32) {// 16 * 2
		anagain = 0x00;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp;
	} else if (gain_val < 64) {//16 * 4
		anagain = 0x01;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp >> 1;
	} else if (gain_val < 128) {//16 * 8
		anagain = 0x03;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp >> 2;
	} else if (gain_val < 256) {//16 * 16
		anagain = 0x07;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp >> 3;
	} else if (gain_val < 512) {//16 * 32
		anagain = 0x0f;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp >> 4;
	} else if (gain_val < 1024) {//16 * 32 * 2
		anagain = 0x1f;
		gaindighigh = 0x00;
		gaindiglow = gain_tmp >> 5;
	} else if (gain_val < 2048) {//16 * 32 * 4
		anagain = 0x1f;
		gaindighigh = 0x01;
		gaindiglow = gain_tmp >> 6;
	} else if (gain_val < 4096) {//16 * 32 * 8
		anagain = 0x1f;
		gaindighigh = 0x03;
		gaindiglow = gain_tmp >> 7;
	} else if (gain_val < 8064) {//16 * 32 * 15.75
		anagain = 0x1f;
		gaindighigh = 0x07;
		gaindiglow = gain_tmp >> 8;
	} else {
		anagain = 0x1f;
		gaindighigh = 0x07;
		gaindiglow = 0xfc;
	}

	sensor_write(sd, 0x3e09, (unsigned char)anagain);
	sensor_write(sd, 0x3e07, (unsigned char)gaindiglow);
	sensor_write(sd, 0x3e06, (unsigned char)gaindighigh);

	sensor_dbg("sensor_set_anagain = %d, 0x%x, 0x%x, 0x%x Done!\n", gain_val, anagain, gaindighigh, gaindiglow);
	//sensor_dbg("digital_gain = 0x%x, 0x%x Done!\n", gaindighigh, gaindiglow);
	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	struct sensor_info *info = to_state(sd);
	int exp_val, gain_val;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;
	if (gain_val < 1 * 16)
		gain_val = 16;
	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	//sensor_write(sd, 0x3812, 0x00);
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
	//sensor_write(sd, 0x3812, 0x30);

	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}


static int sensor_s_fps(struct v4l2_subdev *sd,
			struct sensor_fps *fps)
{
	data_type rdval1, rdval2, rdval3;
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;

	sc1301cs_sensor_vts = wsize->pclk/fps->fps/wsize->hts;
	//fps_change_flag = 1;
	//sensor_write(sd, 0x302d, 1);

	//sensor_read(sd, 0x30f8, &rdval1);
	//sensor_read(sd, 0x30f9, &rdval2);
	//sensor_read(sd, 0x30fa, &rdval3);

	//sensor_dbg("sc1301cs_sensor_svr: %d, vts: %d.\n", sc1301cs_sensor_svr, (rdval1 | (rdval2<<8) | (rdval3<<16)));
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
		ret = sensor_write(sd, 0x0100, rdval&0xfe);
	else
		ret = sensor_write(sd, 0x0100, rdval|0x01);
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
		usleep_range(100, 120);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(2000, 2200);
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
		usleep_range(1000, 1200);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rdval = 0;

	sensor_read(sd, 0x3107, &rdval);
//	if (rdval != (V4L2_IDENT_SENSOR>>8))
//		return -ENODEV;
	sensor_print("0x3107 = 0x%x\n", rdval);
	sensor_read(sd, 0x3108, &rdval);
//	if (rdval != (V4L2_IDENT_SENSOR&0xff))
//		return -ENODEV;
	sensor_print("0x3108 = 0x%x\n", rdval);
	sensor_read(sd, 0x3e03, &rdval);
	sensor_print("0x3e03 = 0x%x\n", rdval);

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
	info->low_speed = 0;
	info->width = 1920;
	info->height = 1080;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 30;	/* 30fps */

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
		ret = 0;
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		ret = sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
	case VIDIOC_VIN_SENSOR_SET_FPS:
		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
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
	{
	.width         = 4208,
	.height        = 2368,
	.hoffset       = 0,
	.voffset       = 0,
	.hts           = 7500,
	.vts           = 2400,
	//.pclk          = 360*1000*1000,
	.pclk          = 540*1000*1000,
	//.mipi_bps      = 840*1000*1000,
	.mipi_bps      = 1260*1000*1000,
	.fps_fixed     = 30,
	.bin_factor    = 1,
	.intg_min      = 1 << 4,
	.intg_max      = (2400 - 5) << 4,
	.gain_min      = 1 << 4,
	.gain_max      = 504 << 4,
	.regs          = sensor_4208x2368_30fps_regs,
	.regs_size     = ARRAY_SIZE(sensor_4208x2368_30fps_regs),
	.set_size      = NULL,
	.top_clk       = 550*1000*1000,
	.isp_clk       = 550*1000*1000,
	 },
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct sensor_info *info = to_state(sd);

	cfg->type = V4L2_MBUS_CSI2;
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE)
		cfg->flags = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
	else
		cfg->flags = 0 | V4L2_MBUS_CSI2_4_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
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

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init\n");
	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	sc1301cs_sensor_vts = wsize->vts;

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width,
		     wsize->height);

	//info->initialized = 1;
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

	v4l2_ctrl_handler_init(handler, 2);

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			      256 * 1600, 1, 1 * 1600);
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

/******************************************************************************
 *
 *****************************************************************************/
/******************************************************************************
 *
 *****************************************************************************/

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
	g_sd = sd;

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
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET1 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	//info->time_hs = 0x02; /* 0x09 */
	info->exp = 0;
	info->gain = 0;
	//info->initialized = 0;

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

	if (entry)
		proc_remove(entry);
}

module_init(init_sensor);
module_exit(exit_sensor);
