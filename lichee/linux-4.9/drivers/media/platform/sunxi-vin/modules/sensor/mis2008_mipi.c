/*
 * A V4L2 driver for mis2008 Raw cameras.
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

MODULE_AUTHOR("cwm");
MODULE_DESCRIPTION("A low-level driver for mis2008 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR 0x2008	//TBD

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 25

/*
 * The SC2232 i2c address
 */
#define I2C_ADDR 0x60

#define SENSOR_NUM 0x2
#define SENSOR_NAME "mis2008_mipi"
#define SENSOR_NAME_2 "mis2008_mipi_2"

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_1080p15_regs[] = {
	/*  1928*1080@15fps */
	{0x300a, 0x01},
	{0x3006, 0x02},
	{0x3007, 0x00},
	{0x3637, 0x1e},
	{0x3c40, 0x8d},
	{0x3c01, 0x10},
	{0x3c0e, 0xf7},
	{0x3c0f, 0x34},
	{0x3b01, 0x3f},
	{0x3b03, 0x3f},
	{0x3902, 0x01},
	{0x3904, 0x00},
	{0x3903, 0x00},
	{0x3906, 0x1e},
	{0x3905, 0x00},
	{0x3908, 0xb0},
	{0x3907, 0x10},
	{0x390a, 0xff},
	{0x3909, 0x1f},
	{0x390c, 0xc3},
	{0x390b, 0x03},
	{0x390e, 0x40},
	{0x390d, 0x00},
	{0x3910, 0xb0},
	{0x390f, 0x10},
	{0x3912, 0xff},
	{0x3911, 0x1f},
	{0x3919, 0x00},
	{0x3918, 0x00},
	{0x391b, 0xfd},
	{0x391a, 0x00},
	{0x3983, 0x5a},
	{0x3982, 0x00},
	{0x3985, 0x0f},
	{0x3984, 0x00},
	{0x391d, 0x00},
	{0x391c, 0x00},
	{0x391f, 0xa4},
	{0x391e, 0x10},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0xff},
	{0x3922, 0x1f},
	{0x3932, 0x00},
	{0x3931, 0x00},
	{0x3934, 0xd0},
	{0x3933, 0x00},
	{0x393f, 0x61},
	{0x393e, 0x00},
	{0x3941, 0x89},
	{0x3940, 0x00},
	{0x3943, 0x80},
	{0x3942, 0x01},
	{0x3945, 0x10},
	{0x3944, 0x03},
	{0x3925, 0x95},
	{0x3924, 0x00},
	{0x3927, 0x3d},
	{0x3926, 0x03},
	{0x3947, 0xee},
	{0x3946, 0x00},
	{0x3949, 0x9e},
	{0x3948, 0x0f},
	{0x394b, 0x9e},
	{0x394a, 0x03},
	{0x394d, 0x9c},
	{0x394c, 0x00},
	{0x3913, 0x01},
	{0x3915, 0x0f},
	{0x3914, 0x00},
	{0x3917, 0xc3},
	{0x3916, 0x03},
	{0x392a, 0x1e},
	{0x3929, 0x00},
	{0x392c, 0x0f},
	{0x392b, 0x00},
	{0x392e, 0x0f},
	{0x392d, 0x00},
	{0x3930, 0xca},
	{0x392f, 0x03},
	{0x397f, 0x00},
	{0x397e, 0x00},
	{0x3981, 0x40},
	{0x3980, 0x00},
	{0x395d, 0xbe},
	{0x395c, 0x10},
	{0x3962, 0xdc},
	{0x3961, 0x10},
	{0x3977, 0x22},
	{0x3976, 0x00},
	{0x396d, 0x10},
	{0x396c, 0x03},
	{0x396f, 0x10},
	{0x396e, 0x03},
	{0x3971, 0x10},
	{0x3970, 0x03},
	{0x3973, 0x10},
	{0x3972, 0x03},
	{0x3978, 0x00},
	{0x3979, 0x04},
	{0x3012, 0x01},
	{0x3600, 0x13},
	{0x3601, 0x02},
	{0x360f, 0x00},
	{0x360e, 0x00},
	{0x3610, 0x02},
	{0x3707, 0x00},
	{0x3708, 0x40},
	{0x3709, 0x00},
	{0x370a, 0x40},
	{0x370b, 0x00},
	{0x370c, 0x40},
	{0x370d, 0x00},
	{0x370e, 0x40},
	{0x3800, 0x01},
	{0x3a03, 0x00},
	{0x3a08, 0xb4},
	{0x3a1b, 0x54},
	{0x3a1e, 0x80},
	{0x3100, 0x04},
	{0x3101, 0x64},
	{0x3a1c, 0x1f},
	{0x3a0C, 0x04},
	{0x3a0D, 0x12},
	{0x3a0E, 0x15},
	{0x3a0F, 0x18},
	{0x3a10, 0x20},
	{0x3a11, 0x3c},
	{0x3300, 0x1e},
	{0x3301, 0x00},
	{0x3302, 0x02},
	{0x3303, 0x05},
	{0x330b, 0x01},
	{0x330f, 0x07},
	{0x330d, 0x01},
	{0x3011, 0x2b},
	{0x3c20, 0x2b},
	{0x3c21, 0x6b},

	//{0x3201, 0x53},
	//{0x3200, 0x07},

	//{0x3203, 0x00},
	//{0x3202, 0x0a},
	{0x3201, 0x70},
	{0x3200, 0x08},
	{0x3203, 0xae},
	{0x3202, 0x08},

	{0x3205, 0x04},
	{0x3204, 0x00},
	{0x3207, 0x3f},
	{0x3206, 0x04},
	{0x3209, 0x09},
	{0x3208, 0x00},
	{0x320b, 0x88},
	{0x320a, 0x07},
	{0x3b00, 0x07},
	{0x3b01, 0xff},
	{0x3c40, 0x8c},
	{0x3006, 0x00},
	{0x3801, 0x10},
};

static struct regval_list sensor_1080p20_regs[] = {
	/*  1928*1080@20fps */
	{0x300a, 0x01},
	{0x3006, 0x02},
	{0x3007, 0x00},
	{0x3637, 0x1e},
	{0x3c40, 0x8d},
	{0x3c01, 0x10},
	{0x3c0e, 0xf7},
	{0x3c0f, 0x34},
	{0x3b01, 0x3f},
	{0x3b03, 0x3f},
	{0x3902, 0x01},
	{0x3904, 0x00},
	{0x3903, 0x00},
	{0x3906, 0x1e},
	{0x3905, 0x00},
	{0x3908, 0xb0},
	{0x3907, 0x10},
	{0x390a, 0xff},
	{0x3909, 0x1f},
	{0x390c, 0xc3},
	{0x390b, 0x03},
	{0x390e, 0x77},
	{0x390d, 0x00},
	{0x3910, 0xb0},
	{0x390f, 0x10},
	{0x3912, 0xff},
	{0x3911, 0x1f},
	{0x3919, 0x00},
	{0x3918, 0x00},
	{0x391b, 0xfd},
	{0x391a, 0x00},
	{0x3983, 0x5a},
	{0x3982, 0x00},
	{0x3985, 0x0f},
	{0x3984, 0x00},
	{0x391d, 0x00},
	{0x391c, 0x00},
	{0x391f, 0xa4},
	{0x391e, 0x10},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0xff},
	{0x3922, 0x1f},
	{0x3932, 0x00},
	{0x3931, 0x00},
	{0x3934, 0xd0},
	{0x3933, 0x00},
	{0x393f, 0x61},
	{0x393e, 0x00},
	{0x3941, 0x89},
	{0x3940, 0x00},
	{0x3943, 0x16},
	{0x3942, 0x01},
	{0x3945, 0x10},
	{0x3944, 0x03},
	{0x3925, 0x95},
	{0x3924, 0x00},
	{0x3927, 0x3d},
	{0x3926, 0x03},
	{0x3947, 0xee},
	{0x3946, 0x00},
	{0x3949, 0x9e},
	{0x3948, 0x0f},
	{0x394b, 0x9e},
	{0x394a, 0x03},
	{0x394d, 0x9c},
	{0x394c, 0x00},
	{0x3913, 0x01},
	{0x3915, 0x0f},
	{0x3914, 0x00},
	{0x3917, 0xc3},
	{0x3916, 0x03},
	{0x392a, 0x1e},
	{0x3929, 0x00},
	{0x392c, 0x0f},
	{0x392b, 0x00},
	{0x392e, 0x0f},
	{0x392d, 0x00},
	{0x3930, 0xca},
	{0x392f, 0x03},
	{0x397f, 0x00},
	{0x397e, 0x00},
	{0x3981, 0x77},
	{0x3980, 0x00},
	{0x395d, 0xbe},
	{0x395c, 0x10},
	{0x3962, 0xdc},
	{0x3961, 0x10},
	{0x3977, 0x22},
	{0x3976, 0x00},
	{0x396d, 0x10},
	{0x396c, 0x03},
	{0x396f, 0x10},
	{0x396e, 0x03},
	{0x3971, 0x10},
	{0x3970, 0x03},
	{0x3973, 0x10},
	{0x3972, 0x03},
	{0x3978, 0x00},
	{0x3979, 0x04},
	{0x3012, 0x01},
	{0x3600, 0x13},
	{0x3601, 0x02},
	{0x360f, 0x00},
	{0x360e, 0x00},
	{0x3610, 0x02},
	{0x3707, 0x00},
	{0x3708, 0x40},
	{0x3709, 0x00},
	{0x370a, 0x40},
	{0x370b, 0x00},
	{0x370c, 0x40},
	{0x370d, 0x00},
	{0x370e, 0x40},
	{0x3800, 0x01},
	{0x3a03, 0x3f},
	{0x3a08, 0xb4},
	{0x3a1b, 0x54},
	{0x3a1e, 0x00},
	{0x3100, 0x04},
	{0x3101, 0x64},
	{0x3a1c, 0x1f},
	{0x3a0C, 0x04},
	{0x3a0D, 0x12},
	{0x3a0E, 0x15},
	{0x3a0F, 0x18},
	{0x3a10, 0x20},
	{0x3a11, 0x3c},
	{0x3300, 0x1e},
	{0x3301, 0x00},
	{0x3302, 0x02},
	{0x3303, 0x03},
	{0x330b, 0x01},
	{0x330f, 0x07},
	{0x330d, 0x01},
	{0x3011, 0x2b},
	{0x3c20, 0x2b},
	{0x3c21, 0x6b},

	//{0x3201, 0x7e},
	//{0x3200, 0x05},

	//{0x3203, 0x00},
	//{0x3202, 0x0a},

	{0x3201, 0x54},
	{0x3200, 0x06},
	{0x3203, 0xae},
	{0x3202, 0x08},

	{0x3205, 0x04},
	{0x3204, 0x00},
	{0x3207, 0x3f},
	{0x3206, 0x04},
	{0x3209, 0x09},
	{0x3208, 0x00},
	{0x320b, 0x88},
	{0x320a, 0x07},
	{0x3006, 0x00},
};

static struct regval_list sensor_1080p25_regs[] = {
	/*  1928*1080@25fps */
	{0x300a, 0x01},
	{0x3006, 0x02},
	{0x3007, 0x00},
	{0x3637, 0x1e},
	{0x3c40, 0x8d},
	{0x3c01, 0x10},
	{0x3c0e, 0xf7},
	{0x3c0f, 0x34},
	{0x3b01, 0x3f},
	{0x3b03, 0x3f},
	{0x3902, 0x01},
	{0x3904, 0x00},
	{0x3903, 0x00},
	{0x3906, 0x1e},
	{0x3905, 0x00},
	{0x3908, 0xb0},
	{0x3907, 0x10},
	{0x390a, 0xff},
	{0x3909, 0x1f},
	{0x390c, 0xc3},
	{0x390b, 0x03},
	{0x390e, 0x77},
	{0x390d, 0x00},
	{0x3910, 0xb0},
	{0x390f, 0x10},
	{0x3912, 0xff},
	{0x3911, 0x1f},
	{0x3919, 0x00},
	{0x3918, 0x00},
	{0x391b, 0xfd},
	{0x391a, 0x00},
	{0x3983, 0x5a},
	{0x3982, 0x00},
	{0x3985, 0x0f},
	{0x3984, 0x00},
	{0x391d, 0x00},
	{0x391c, 0x00},
	{0x391f, 0xa4},
	{0x391e, 0x10},
	{0x3921, 0xff},
	{0x3920, 0x1f},
	{0x3923, 0xff},
	{0x3922, 0x1f},
	{0x3932, 0x00},
	{0x3931, 0x00},
	{0x3934, 0xd0},
	{0x3933, 0x00},
	{0x393f, 0x61},
	{0x393e, 0x00},
	{0x3941, 0x89},
	{0x3940, 0x00},
	{0x3943, 0x16},
	{0x3942, 0x01},
	{0x3945, 0x10},
	{0x3944, 0x03},
	{0x3925, 0x95},
	{0x3924, 0x00},
	{0x3927, 0x3d},
	{0x3926, 0x03},
	{0x3947, 0xee},
	{0x3946, 0x00},
	{0x3949, 0x9e},
	{0x3948, 0x0f},
	{0x394b, 0x9e},
	{0x394a, 0x03},
	{0x394d, 0x9c},
	{0x394c, 0x00},
	{0x3913, 0x01},
	{0x3915, 0x0f},
	{0x3914, 0x00},
	{0x3917, 0xc3},
	{0x3916, 0x03},
	{0x392a, 0x1e},
	{0x3929, 0x00},
	{0x392c, 0x0f},
	{0x392b, 0x00},
	{0x392e, 0x0f},
	{0x392d, 0x00},
	{0x3930, 0xca},
	{0x392f, 0x03},
	{0x397f, 0x00},
	{0x397e, 0x00},
	{0x3981, 0x77},
	{0x3980, 0x00},
	{0x395d, 0xbe},
	{0x395c, 0x10},
	{0x3962, 0xdc},
	{0x3961, 0x10},
	{0x3977, 0x22},
	{0x3976, 0x00},
	{0x396d, 0x10},
	{0x396c, 0x03},
	{0x396f, 0x10},
	{0x396e, 0x03},
	{0x3971, 0x10},
	{0x3970, 0x03},
	{0x3973, 0x10},
	{0x3972, 0x03},
	{0x3978, 0x00},
	{0x3979, 0x04},
	{0x3012, 0x01},
	{0x3600, 0x13},
	{0x3601, 0x02},
	{0x360f, 0x00},
	{0x360e, 0x00},
	{0x3610, 0x02},
	{0x3707, 0x00},
	{0x3708, 0x40},
	{0x3709, 0x00},
	{0x370a, 0x40},
	{0x370b, 0x00},
	{0x370c, 0x40},
	{0x370d, 0x00},
	{0x370e, 0x40},
	{0x3800, 0x01},
	{0x3a03, 0x3f},
	{0x3a08, 0xb4},
	{0x3a1b, 0x54},
	{0x3a1e, 0x00},
	{0x3100, 0x04},
	{0x3101, 0x64},
	{0x3a1c, 0x1f},
	{0x3a0C, 0x04},
	{0x3a0D, 0x12},
	{0x3a0E, 0x15},
	{0x3a0F, 0x18},
	{0x3a10, 0x20},
	{0x3a11, 0x3c},
	{0x3300, 0x1e},
	{0x3301, 0x00},

	{0x3302, 0x02},
	{0x3303, 0x03},
	{0x330b, 0x01},
	{0x330f, 0x07},
	{0x330d, 0x01},
	{0x3011, 0x2b},
	{0x3c20, 0x2b},
	{0x3c21, 0x6b},

	{0x3201, 0x10},
	{0x3200, 0x05},
	{0x3203, 0xae},
	{0x3202, 0x08},

	{0x3205, 0x04},
	{0x3204, 0x00},
	{0x3207, 0x3f},
	{0x3206, 0x04},
	{0x3209, 0x09},
	{0x3208, 0x00},
	{0x320b, 0x88},
	{0x320a, 0x07},
	{0x3006, 0x00},
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

static int mis2008_sensor_vts;
/*static int mis2008_sensor_svr;*/
static int shutter_delay = 1;
static int shutter_delay_cnt;
static int fps_change_flag;

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	struct sensor_info *info = to_state(sd);
    int tmp_exp_val = exp_val / 16;

    sensor_write(sd, 0x3100, (tmp_exp_val >> 8) & 0xFF);
    sensor_write(sd, 0x3101, (tmp_exp_val & 0xFF));

	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->gain;
//	sensor_dbg("sensor_get_gain = %d\n", info->gain);
	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	data_type again = 0, dgain = 0x80;

	if (gain_val < 2 * 16) {// 2x
		again = (gain_val << 1) & 0x1f;
		dgain = 0x80;
	} else if (gain_val < 4 * 16) {// 4x
		again = (gain_val) & 0x1f;
		again |= 0x20;
		dgain = 0x80;
	} else if (gain_val < 8 * 16) {// 8x
		again = (gain_val >> 1) & 0x1f;
		again |= 0x40;
		dgain = 0x80;
	} else if (gain_val < 16 * 16) {// 16x
		again = (gain_val >> 2) & 0x1f;
		again |= 0x60;
		dgain = 0x80;
	} else if (gain_val < 256 * 16) {// 256x
		again = 0x7f;
		dgain = gain_val >> 1;
	} else {// >256x
		again = 0x7f;
		dgain = 0x7ff;
	}

	if (0 == again) {
		sensor_write(sd, 0x3a02, 0x0b);
	} else {
		sensor_write(sd, 0x3a02, 0x0a);
	}

	sensor_write(sd, 0x3102, (unsigned char)(again & 0xff));
	sensor_write(sd, 0x3700, (unsigned char)((dgain >> 8) & 0xff));
	sensor_write(sd, 0x3701, (unsigned char)(dgain & 0xff));

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

	int shutter = exp_val >> 4;
	int frame_length = 0;
	if (shutter > mis2008_sensor_vts - 4)
		frame_length = shutter + 4;
	else
		frame_length = mis2008_sensor_vts;
	sensor_write(sd, 0x3200, (frame_length >> 8) & 0xFF);
	sensor_write(sd, 0x3201, (frame_length & 0xFF));
	sensor_dbg("sensor_s_exp_gain:shutter %lx frame_length %lx\n", shutter, frame_length);

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

//	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	info->exp = exp_val;
	info->gain = gain_val;

	return 0;
}

static data_type mis2008_flip_status_mipi_1;
static data_type mis2008_flip_status_mipi_2;

static unsigned int mis2008_code_save = MEDIA_BUS_FMT_SGRBG10_1X10;
static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	printk("into set sensor hfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3007, &get_value);
	if (enable)
		set_value = get_value | 0x01;
	else
		set_value = get_value & 0xfe;

	sensor_write(sd, 0x3007, set_value);
	if (!strcmp(sd->name, "mis2008_mipi"))
		mis2008_flip_status_mipi_1 = set_value;
	else if (!strcmp(sd->name, "mis2008_mipi_2"))
		mis2008_flip_status_mipi_2 = set_value;
	printk("sensor_s_hflip mis2008_flip_status_mipi_1 %d mis2008_flip_status_mipi_2 %d\n",
			mis2008_flip_status_mipi_1, mis2008_flip_status_mipi_2);
    return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	printk("into set sensor vfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3007, &get_value);
	if (enable)
		set_value = get_value | 0x02;
	else
		set_value = get_value & 0xfd;

	sensor_write(sd, 0x3007, set_value);
	if (!strcmp(sd->name, "mis2008_mipi")) {
		printk("set mis2008_mipi vflip\n");
		mis2008_flip_status_mipi_1 = set_value;
	} else if (!strcmp(sd->name, "mis2008_mipi_2")) {
		printk("set mis2008_mipi_2 vflip\n");
		mis2008_flip_status_mipi_2 = set_value;
	}
	printk("sensor_s_vflip mis2008_flip_status_mipi_1 %d mis2008_flip_status_mipi_2 %d\n",
			mis2008_flip_status_mipi_1, mis2008_flip_status_mipi_2);
    return 0;

}

#define SENSOR_TEMPER_REGS_HIGH 0x3802
#define SENSOR_TEMPER_REGS_LOW  0x3803

static int sensor_get_temp(struct v4l2_subdev *sd,  struct sensor_temp *temp)
{
	struct sensor_info *info = to_state(sd);

	data_type rdval, temp_value;
	int cnt = 0;

	sensor_read(sd, SENSOR_TEMPER_REGS_HIGH, &rdval);
//	sensor_print("SENSOR_TEMPER_REGS_HIGH value %x\n",rdval);
	rdval |= 0xF00;
	temp_value = rdval;
	sensor_read(sd, SENSOR_TEMPER_REGS_LOW, &rdval);
//	sensor_print("SENSOR_TEMPER_REGS_LOW value %x\n",rdval);
	rdval &= 0x0FF;
	temp_value = temp_value << 8;
	temp_value |= rdval;
	temp_value = (temp_value + 1)*839/100/32*52/100-264; //2069 18.218
//	temp_value += 10;
	temp_value = temp_value * 3;
	temp->temp = temp_value;
//	sensor_print("sensor_get_temp temp %d\n",temp_value);
	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);
	data_type check_value = 0;
	struct sensor_format_struct *sensor_fmt = info->fmt;

	if (!strcmp(sd->name, "mis2008_mipi"))
		check_value = mis2008_flip_status_mipi_1 & 0x3;
	else if (!strcmp(sd->name, "mis2008_mipi_2"))
		check_value = mis2008_flip_status_mipi_2 & 0x3;

	printk("sensor_get_fmt_mbus_core %s check_value %d mis2008_flip_status_mipi_1 %d mis2008_flip_status_mipi_2 %d\n",
			sd->name, check_value, mis2008_flip_status_mipi_1, mis2008_flip_status_mipi_2);

	switch (check_value) {
	case 0x00:
		*code = MEDIA_BUS_FMT_SGRBG10_1X10;
		break;
	case 0x01:
		*code = MEDIA_BUS_FMT_SRGGB10_1X10;
		break;
	case 0x02:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	case 0x03:
		*code = MEDIA_BUS_FMT_SGBRG10_1X10;
		break;
	default:
		*code = info->fmt->mbus_code;
	}
	sensor_fmt->mbus_code = *code;

	return 0;
}

static int sensor_s_fps(struct v4l2_subdev *sd,
			struct sensor_fps *fps)
{
	/*data_type rdval1, rdval2, rdval3;*/
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;

	fps_change_flag = 1;
	/*sensor_write(sd, 0x302d, 1);
	sensor_read(sd, 0x30f8, &rdval1);
	sensor_read(sd, 0x30f9, &rdval2);
	sensor_read(sd, 0x30fa, &rdval3);

	sensor_dbg("sc2232_sensor_svr: %d, vts: %d.\n", sc2232_sensor_svr, (rdval1 | (rdval2<<8) | (rdval3<<16)));*/
	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret = 0;
	data_type rdval;

//	ret = sensor_read(sd, 0x0100, &rdval);
//	if (ret != 0)
//		return ret;
//
//	if (on_off == STBY_ON)
//		ret = sensor_write(sd, 0x0100, rdval&0xfe);
//	else
//		ret = sensor_write(sd, 0x0100, rdval|0x01);
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
		usleep_range(10000, 12000);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(10000, 12000);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(30000, 32000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_dbg("PWR_OFF!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
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
	unsigned int SENSOR_ID = 0;
	data_type rdval;
	int cnt = 0;

	sensor_read(sd, 0x3000, &rdval);
	SENSOR_ID |= (rdval << 8);
	sensor_read(sd, 0x3001, &rdval);
	SENSOR_ID |= (rdval);
	sensor_print("V4L2_IDENT_SENSOR = 0x%x\n", SENSOR_ID);

	while ((SENSOR_ID != V4L2_IDENT_SENSOR) && (cnt < 5)) {
		SENSOR_ID = 0;
		sensor_read(sd, 0x3000, &rdval);
		SENSOR_ID |= (rdval << 8);
		sensor_read(sd, 0x3001, &rdval);
		SENSOR_ID |= (rdval);
		sensor_print("retry = %d, V4L2_IDENT_SENSOR = %x\n",
			cnt, SENSOR_ID);
		cnt++;
		}
	if (SENSOR_ID != V4L2_IDENT_SENSOR)
		return -ENODEV;

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
	info->tpf.denominator = 25;	/* 25fps */

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
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
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
		.mbus_code = MEDIA_BUS_FMT_SGRBG10_1X10,
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
		.width = 1920,
		.height = 1080,
		.hoffset = 0,
		.voffset = 0,
		.hts = 2222,
		.vts = 2160,
		.pclk = 72000000,
		.mipi_bps = 360 * 5000 * 1000,
		.fps_fixed = 15,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (2160 - 1) << 4,
		.gain_min = 1 << 4,
		.gain_max = 256 << 4,
		.regs = sensor_1080p15_regs,
		.regs_size = ARRAY_SIZE(sensor_1080p15_regs),
		.set_size = NULL,
	},
	{
		.width = 1920,
		.height = 1080,
		.hoffset = 0,
		.voffset = 0,
		.hts = 2222,
		.vts = 1620,
		.pclk = 72000000,
		.mipi_bps = 360 * 5000 * 1000,
		.fps_fixed = 20, //20
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (1620 - 1) << 4,
		.gain_min = 1 << 4,
		.gain_max = 256 << 4,
		.regs = sensor_1080p20_regs,
		.regs_size = ARRAY_SIZE(sensor_1080p20_regs),
		.set_size = NULL,
	},
	{
		.width = 1920,
		.height = 1080,
		.hoffset = 0,
		.voffset = 0,
		.hts = 2222,
		.vts = 1296,
		.pclk = 72000000,
		.mipi_bps = 360 * 5000 * 1000,
		.fps_fixed = 25,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = (1296 - 1) << 4,
		.gain_min = 1 << 4,
		.gain_max = 256 << 4,
		.regs = sensor_1080p25_regs,
		.regs_size = ARRAY_SIZE(sensor_1080p25_regs),
		.set_size = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct sensor_info *info = to_state(sd);

	cfg->type = V4L2_MBUS_CSI2;
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE)
		cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
	else
		cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
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
	/*data_type rdval_l, rdval_h;*/
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	data_type check_value = 0;

	ret = sensor_write_array(sd, sensor_default_regs, ARRAY_SIZE(sensor_default_regs));

	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	/*soft reset sensor*/
	data_type rdval;
	sensor_write(sd, 0x3006, 0x01);
	usleep_range(50000, 50000);
	printk("soft reset sensor\n");

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	if (wsize->regs)
		sensor_write_array(sd, wsize->regs, wsize->regs_size);

	if (wsize->set_size)
		wsize->set_size(sd);

    info->width = wsize->width;
    info->height = wsize->height;
    mis2008_sensor_vts = wsize->vts;

	if (!strcmp(sd->name, "mis2008_mipi")) {
		sensor_read(sd, 0x3007, &mis2008_flip_status_mipi_1);//read sensor init flip
		check_value = mis2008_flip_status_mipi_1 & 0x3;
	} else if (!strcmp(sd->name, "mis2008_mipi_2")) {
		sensor_read(sd, 0x3007, &mis2008_flip_status_mipi_2);
		check_value = mis2008_flip_status_mipi_2 & 0x3;
    }

	switch (check_value) {
	case 0x00:
		sensor_dbg("GRBG\n");
		mis2008_code_save = MEDIA_BUS_FMT_SGRBG10_1X10;
		break;
	case 0x01:
		sensor_dbg("RGGB\n");
		mis2008_code_save = MEDIA_BUS_FMT_SRGGB10_1X10;
		break;
	case 0x02:
		sensor_dbg("BGGR\n");
		mis2008_code_save = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	case 0x03:
		sensor_dbg("GBRG\n");
		mis2008_code_save = MEDIA_BUS_FMT_SGBRG10_1X10;
		break;
	default:
		mis2008_code_save = MEDIA_BUS_FMT_SGRBG10_1X10;
	}
	sensor_fmt->mbus_code = mis2008_code_save;
	printk("gg fps s_fmt set width = %d, height = %d pclk %d fps %d vts %x\n", wsize->width,
			wsize->height, wsize->pclk, wsize->fps_fixed, mis2008_sensor_vts);
	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_print("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
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
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE;
	info->time_hs = 0x23;
	info->stream_seq = MIPI_BEFORE_SENSOR;
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
