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
#ifdef CONFIG_THERMAL
#include <linux/thermal.h>
#endif

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("hzh");
MODULE_DESCRIPTION("A low-level driver for f37p sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR  0x0841

//define the registers
#define EXP_HIGH		0x02
#define EXP_LOW			0x01
#define GAIN		    0x00
/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0x0A
#define ID_REG_LOW		0x0B
#define ID_VAL_HIGH		0x08
#define ID_VAL_LOW		0x41
#define SENSOR_FRAME_RATE 20

/*
 * The f37p i2c address
 */
#define I2C_ADDR 0x80

#define SENSOR_NUM 0x2
#define SENSOR_NAME "f37p_mipi"
#define SENSOR_NAME_2 "f37p_mipi_2"
#define		_SOI_MIRROR_FLIP
///#define	_SOI_BSUN_OPT
///#define	_SOI_PRECHG_OPT

struct sensor_switch_cfg_t {
	bool sensor_is_detected;
	bool sensor_with_switch_en;
	unsigned int switch_status;
	unsigned int i2c_addr_container[2];
	unsigned int frame_length_save[2];
	unsigned int exp_val_save[2];
	unsigned int gain_val_save[2];
	bool set_expgain_flags[2];
	unsigned int vsync_cnt;
};

#define MIPI_SWITCH_EN 1
#if MIPI_SWITCH_EN

typedef enum _sensor_switch_choice_type {
	SWITCH_SENSOR_A = 0,
	SWITCH_SENSOR_B = 1,
	SWITCH_SENSOR_MAX,
} sensor_switch_choice_type;

#define A_I2C_ADDR 0x80
#define B_I2C_ADDR 0x84
#define C_I2C_ADDR 0x88
#define D_I2C_ADDR 0x8C
#define MIPI_SWITCH_GPIO POWER_EN
#define GPIO_SWITCH_SENSOR_A_STATUS CSI_GPIO_LOW
#define GPIO_SWITCH_SENSOR_B_STATUS CSI_GPIO_HIGH

static struct sensor_switch_cfg_t sensor_switch_cfg[2] = {
	[0] = {
		.i2c_addr_container = {A_I2C_ADDR, B_I2C_ADDR},
	},
	[1] = {
		.i2c_addr_container = {C_I2C_ADDR, D_I2C_ADDR},
	},
};

static DEFINE_MUTEX(device_mutex); /* four sensor use one i2c */
#endif

data_type sensor_flip_status;

#ifdef _SOI_BSUN_OPT
static unsigned char	BSunMode = 0xFF;					//Uninitialized
#endif
#ifdef _SOI_PRECHG_OPT
static unsigned char	PreChgMode = 0xFF;					//Uninitialized
#endif

static unsigned short settleTime = 0xa0;
module_param(settleTime, ushort, 0644);
/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_1080p20_regs[] = {
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x19},
	{0x0F, 0x04},
	{0x10, 0x20},
	{0x11, 0x80},
	{0x46, 0x09},
	{0x47, 0x66},
	{0x0D, 0xF2},
	{0x57, 0x6A},
	{0x58, 0x22},
	{0x5F, 0x41},
	{0x60, 0x24},
	{0xA5, 0xC0},
	{0x20, 0x80},
	{0x21, 0x07},
	{0x22, 0x65},
	{0x23, 0x04},
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0x06},
	{0x28, 0x15},
	{0x29, 0x06},
	{0x2A, 0xFB},
	{0x2B, 0x15},
	{0x2C, 0x02},
	{0x2D, 0x00},
	{0x2E, 0x14},
	{0x2F, 0x04},
	{0x41, 0xC5},
	{0x42, 0x33},
	{0x47, 0x46},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0xAB, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0x6C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x56},
	//{0x74, 0x02},
	{0x74, 0x00},
	{0x78, 0x9D},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x31, 0x10},
	{0x32, 0x18},
	{0x33, 0xE8},
	{0x34, 0x5E},
	{0x35, 0x5E},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0xFF},
	{0x3D, 0xFF},
	{0x3E, 0xFF},
	{0x3F, 0xBB},
	{0x40, 0xFF},
	{0x56, 0x92},
	{0x59, 0xAF},
	{0x5A, 0x47},
	{0x61, 0x18},
	{0x6F, 0x04},
	{0x85, 0x5F},
	{0x8A, 0x44},
	{0x91, 0x13},
	{0x94, 0xE0},
	{0x9B, 0x83},
	{0x9C, 0xE1},
	{0xA4, 0x80},
	{0xA6, 0x22},
	{0xA9, 0x1C},
	{0x5B, 0xE7},
	{0x5C, 0x28},
	{0x5D, 0x67},
	{0x5E, 0x11},
	{0x62, 0x21},
	{0x63, 0x0F},
	{0x64, 0xD0},
	{0x65, 0x02},
	{0x67, 0x49},
	{0x66, 0x00},
	{0x68, 0x00},
	{0x69, 0x72},
	{0x6A, 0x12},
	{0x7A, 0x00},
	{0x82, 0x20},
	{0x8D, 0x47},
	{0x8F, 0x90},
	{0x45, 0x01},
	{0x97, 0x20},
	{0x13, 0x81},
	{0x96, 0x84},
	{0x4A, 0x01},
	{0xB1, 0x00},
	{0xA1, 0x0F},
	{0xBE, 0x00},
	{0x7E, 0x48},
	{0xB5, 0xC0},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x90, 0x00},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x8C, 0xFF},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x19, 0x20},
	{0x1B, 0x4F},
//	{0x12, 0x00},
	{0x00, 0x10},
};

static struct regval_list sensor_1080p15_regs[] = {
#if 0
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x19},
	{0x0F, 0x04},
	{0x10, 0x20},
	{0x11, 0x80},
	{0x46, 0x09},
	{0x47, 0x66},
	{0x0D, 0xF2},
	{0x57, 0x6A},
	{0x58, 0x22},
	{0x5F, 0x41},
	{0x60, 0x24},
	{0xA5, 0xC0},
	{0x20, 0x80},
	{0x21, 0x07},
	{0x22, 0xDC},
	{0x23, 0x05},
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0x06},
	{0x28, 0x15},
	{0x29, 0x06},
	{0x2A, 0xFB},
	{0x2B, 0x15},
	{0x2C, 0x02},
	{0x2D, 0x00},
	{0x2E, 0x14},
	{0x2F, 0x04},
	{0x41, 0xC5},
	{0x42, 0x33},
	{0x47, 0x46},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0xAB, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0x6C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x56},
	//{0x74, 0x02},
	{0x74, 0x00},
	{0x78, 0x9D},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x31, 0x10},
	{0x32, 0x18},
	{0x33, 0xE8},
	{0x34, 0x5E},
	{0x35, 0x5E},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0xFF},
	{0x3D, 0xFF},
	{0x3E, 0xFF},
	{0x3F, 0xBB},
	{0x40, 0xFF},
	{0x56, 0x92},
	{0x59, 0xAF},
	{0x5A, 0x47},
	{0x61, 0x18},
	{0x6F, 0x04},
	{0x85, 0x5F},
	{0x8A, 0x44},
	{0x91, 0x13},
	{0x94, 0xE0},
	{0x9B, 0x83},
	{0x9C, 0xE1},
	{0xA4, 0x80},
	{0xA6, 0x22},
	{0xA9, 0x1C},
	{0x5B, 0xE7},
	{0x5C, 0x28},
	{0x5D, 0x67},
	{0x5E, 0x11},
	{0x62, 0x21},
	{0x63, 0x0F},
	{0x64, 0xD0},
	{0x65, 0x02},
	{0x67, 0x49},
	{0x66, 0x00},
	{0x68, 0x00},
	{0x69, 0x72},
	{0x6A, 0x12},
	{0x7A, 0x00},
	{0x82, 0x20},
	{0x8D, 0x47},
	{0x8F, 0x90},
	{0x45, 0x01},
	{0x97, 0x20},
	{0x13, 0x81},
	{0x96, 0x84},
	{0x4A, 0x01},
	{0xB1, 0x00},
	{0xA1, 0x0F},
	{0xBE, 0x00},
	{0x7E, 0x48},
	{0xB5, 0xC0},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x90, 0x00},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x8C, 0xFF},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x19, 0x20},
	{0x1B, 0x4F},
	{0x12, 0x00},
	{0x00, 0x10},
#endif
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x19},
	{0x0F, 0x04},
	{0x10, 0x1B},
	{0x11, 0x80},
	{0x46, 0x09},
	{0x47, 0x66},
	{0x0D, 0xF2},
	{0x57, 0x6A},
	{0x58, 0x22},
	{0x5F, 0x41},
	{0x60, 0x28},
	{0xA5, 0xC0},
	{0x20, 0x40},
	{0x21, 0x06},
	{0x22, 0x46},
	{0x23, 0x05},
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0x66},
	{0x28, 0x15},
	{0x29, 0x05},
	{0x2A, 0x5B},
	{0x2B, 0x15},
	{0x2C, 0x02},
	{0x2D, 0x00},
	{0x2E, 0x14},
	{0x2F, 0x04},
	{0x41, 0xC5},
	{0x42, 0x33},
	{0x47, 0x46},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0xAB, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x40},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0x6C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x56},
	{0x74, 0x00},
	{0x78, 0x9D},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x31, 0x0C},
	{0x32, 0x11},
	{0x33, 0xDC},
	{0x34, 0x46},
	{0x35, 0x46},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0xFF},
	{0x3D, 0xFF},
	{0x3E, 0xFF},
	{0x3F, 0xBB},
	{0x40, 0xFF},
	{0x56, 0x92},
	{0x59, 0x80},
	{0x5A, 0x47},
	{0x61, 0x18},
	{0x6F, 0x04},
	{0x85, 0x44},
	{0x8A, 0x44},
	{0x91, 0x13},
	{0x94, 0xE0},
	{0x9B, 0x83},
	{0x9C, 0xE1},
	{0xA4, 0x80},
	{0xA6, 0x22},
	{0xA9, 0x1C},
	{0x5B, 0xE7},
	{0x5C, 0x28},
	{0x5D, 0x67},
	{0x5E, 0x11},
	{0x62, 0x21},
	{0x63, 0x0F},
	{0x64, 0xD0},
	{0x65, 0x02},
	{0x67, 0x49},
	{0x66, 0x00},
	{0x68, 0x00},
	{0x69, 0x72},
	{0x6A, 0x12},
	{0x7A, 0x00},
	{0x82, 0x20},
	{0x8D, 0x47},
	{0x8F, 0x90},
	{0x45, 0x01},
	{0x97, 0x20},
	{0x13, 0x81},
	{0x96, 0x84},
	{0x4A, 0x01},
	{0xB1, 0x00},
	{0xA1, 0x0F},
	{0xBE, 0x00},
	{0x7E, 0x48},
	{0xB5, 0xC0},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x90, 0x00},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x8C, 0xFF},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x19, 0x20},
	{0x1B, 0x4F},
//	{0x12, 0x00},
	{0x00, 0x10},
};

static struct regval_list sensor_1080p30_regs[] = {
	{0x12, 0x40},
	{0x48, 0x8A},
	{0x48, 0x0A},
	{0x0E, 0x19},
	{0x0F, 0x04},
	{0x10, 0x24},
	{0x11, 0x80},
	{0x46, 0x09},
	{0x47, 0x66},
	{0x0D, 0xF2},
	{0x57, 0x6A},
	{0x58, 0x22},
	{0x5F, 0x41},
	{0x60, 0x28},
	{0xA5, 0xC0},
	{0x20, 0x00},
	{0x21, 0x05},
	{0x22, 0x65},//30fps:0x65 25fps:0x46
	{0x23, 0x04},//30fps:0x04 25fps:0x05
	{0x24, 0xC0},
	{0x25, 0x38},
	{0x26, 0x43},
	{0x27, 0xC6},
	{0x28, 0x15},
	{0x29, 0x04},
	{0x2A, 0xBB},
	{0x2B, 0x14},
	{0x2C, 0x02},
	{0x2D, 0x00},
	{0x2E, 0x14},
	{0x2F, 0x04},
	{0x41, 0xC5},
	{0x42, 0x33},
	{0x47, 0x46},
	{0x76, 0x60},
	{0x77, 0x09},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0xAB, 0x00},
	{0x1D, 0x00},
	{0x1E, 0x04},
	{0x6C, 0x48},
	{0x9E, 0xF8},
	{0x6E, 0x2C},
	{0x70, 0x6C},
	{0x71, 0x6D},
	{0x72, 0x6A},
	{0x73, 0x56},
	{0x74, 0x00},
	{0x78, 0x9D},
	{0x89, 0x01},
	{0x6B, 0x20},
	{0x86, 0x40},
	{0x31, 0x10},
	{0x32, 0x18},
	{0x33, 0xE8},
	{0x34, 0x5E},
	{0x35, 0x5E},
	{0x3A, 0xAF},
	{0x3B, 0x00},
	{0x3C, 0xFF},
	{0x3D, 0xFF},
	{0x3E, 0xFF},
	{0x3F, 0xBB},
	{0x40, 0xFF},
	{0x56, 0x92},
	{0x59, 0xAF},
	{0x5A, 0x47},
	{0x61, 0x18},
	{0x6F, 0x04},
	{0x85, 0x5F},
	{0x8A, 0x44},
	{0x91, 0x13},
	{0x94, 0xA0},
	{0x9B, 0x83},
	{0x9C, 0xE1},
	{0xA4, 0x80},
	{0xA6, 0x22},
	{0xA9, 0x1C},
	{0x5B, 0xE7},
	{0x5C, 0x28},
	{0x5D, 0x67},
	{0x5E, 0x11},
	{0x62, 0x21},
	{0x63, 0x0F},
	{0x64, 0xD0},
	{0x65, 0x02},
	{0x67, 0x49},
	{0x66, 0x00},
	{0x68, 0x00},
	{0x69, 0x72},
	{0x6A, 0x12},
	{0x7A, 0x00},
	{0x82, 0x20},
	{0x8D, 0x47},
	{0x8F, 0x90},
	{0x45, 0x01},
	{0x97, 0x20},
	{0x13, 0x81},
	{0x96, 0x84},
	{0x4A, 0x01},
	{0xB1, 0x00},
	{0xA1, 0x0F},
	{0xBE, 0x00},
	{0x7E, 0x48},
	{0xB5, 0xC0},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x7F, 0x57},
	{0x90, 0x00},
	{0x7B, 0x4A},
	{0x7C, 0x0C},
	{0x8C, 0xFF},
	{0x8E, 0x00},
	{0x8B, 0x01},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x19, 0x20},
	{0x1B, 0x4F},
//	{0x12, 0x00},
	{0x00, 0x10},
};
/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};

#if MIPI_SWITCH_EN
static void sensor_i2c_switch_lock_init(void)
{
	mutex_init(&device_mutex);
}

static void sensor_i2c_switch_lock_lock(void)
{
	mutex_lock(&device_mutex);
}

static void sensor_i2c_switch_lock_unlock(void)
{
	mutex_unlock(&device_mutex);
}

static int sensor_i2c_addr_get(struct v4l2_subdev *sd, unsigned int *iic_addr)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (client == NULL) {
		sensor_err("client is NULL!!!\n");
		return -1;
	}
	/* i2c addr useful for 7bit */
	*iic_addr = client->addr << 1;

	return 0;
}

static int sensor_i2c_addr_set(struct v4l2_subdev *sd, unsigned int iic_addr)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (client == NULL) {
		sensor_err("client is NULL!!!\n");
		return -1;
	}
	/* i2c addr useful for 7bit */
	client->addr = iic_addr >> 1;

	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val);
static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val);
static int sensor_s_exp_gain_comp(struct v4l2_subdev *sd, struct sensor_mipi_switch_entity *sensor_mipi_switch_info)
{
	struct sensor_info *info = to_state(sd);
	unsigned int comp_ratio = sensor_mipi_switch_info->comp_ratio;
	unsigned int target_exp;
	unsigned int target_gain;

	if (comp_ratio <= 0) {
		sensor_err("comp_ratio = %d <= 0, it will set to default:1000\n", comp_ratio);
		sensor_mipi_switch_info->comp_ratio = 1000;
	}

	target_gain = info->gain * sensor_mipi_switch_info->gain_comp / comp_ratio;
	target_exp = info->exp * sensor_mipi_switch_info->exp_comp / comp_ratio;
	sensor_print("comp = %d, exp = %d, exp_target = %d, gain = %d, target_gain = %d\n", sensor_mipi_switch_info->exp_comp,
		info->exp, target_exp, info->gain, target_gain);

	if (sensor_mipi_switch_info->gain_comp < comp_ratio) {
		if (target_gain < 16) {
			if (target_exp <= 16) {
				sensor_print("target_exp is %d <= min_exp, it will not to set comp!!!\n", target_exp);
			} else {
				sensor_print("target_gain is %d < min_gain, it will set exp comp!!!\n", target_gain);
				sensor_s_exp(sd, target_exp);
				sensor_s_gain(sd, info->gain);
			}
		} else {
			sensor_s_exp(sd, info->exp);
			sensor_s_gain(sd, target_gain);
		}
	} else {
		sensor_s_exp(sd, info->exp);
		sensor_s_gain(sd, target_gain);
	}

	return 0;
}

static int sensor_mipi_get_switch_status(struct v4l2_subdev *sd, unsigned int *mipi_switch_status)
{
	struct sensor_switch_cfg_t *sensor_switch_config;

	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];

	*mipi_switch_status = sensor_switch_config->switch_status;

	return 0;
}

static int sensor_mipi_set_switch_select(struct v4l2_subdev *sd, struct sensor_mipi_switch_entity *sensor_mipi_switch_info)
{
	unsigned int iic_addr;
	struct sensor_switch_cfg_t *sensor_switch_config;

	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];

	if (sensor_mipi_switch_info->mipi_switch_status < SWITCH_SENSOR_A || sensor_mipi_switch_info->mipi_switch_status > SWITCH_SENSOR_MAX) {
		sensor_err("Invaild switch_choice!!!\n");
		return -1;
	}

	switch (sensor_mipi_switch_info->mipi_switch_status) {
	case SWITCH_SENSOR_A:
		sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_A_STATUS;
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
		sensor_s_exp_gain_comp(sd, sensor_mipi_switch_info);
		vin_gpio_write(sd, MIPI_SWITCH_GPIO, GPIO_SWITCH_SENSOR_A_STATUS);
		sensor_mipi_switch_info->time_stamp = ktime_get_ns();
		sensor_i2c_addr_get(sd, &iic_addr);
		sensor_print("----> switch to A, switch_status = %d, iic_addr = 0x%x\n", sensor_switch_config->switch_status, iic_addr);
		break;
	case SWITCH_SENSOR_B:
		sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_B_STATUS;
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
		sensor_s_exp_gain_comp(sd, sensor_mipi_switch_info);
		vin_gpio_write(sd, MIPI_SWITCH_GPIO, GPIO_SWITCH_SENSOR_B_STATUS);
		sensor_mipi_switch_info->time_stamp = ktime_get_ns();
		sensor_i2c_addr_get(sd, &iic_addr);
		sensor_print("----> switch to B, switch_status = %d, iic_addr = 0x%x\n", sensor_switch_config->switch_status, iic_addr);
		break;
	default:
		sensor_err("It is not have this switch_choice:%d, please check choice_type\n", sensor_switch_config->switch_status);
		break;
	}

	return 0;
}

static int sensor_mipi_switch_ctrl(struct v4l2_subdev *sd, struct sensor_mipi_switch_entity *sensor_mipi_switch_info)
{
	if (!sensor_mipi_switch_info) {
		sensor_err("Invaild sensor_mipi_switch_info!!!\n");
		return -1;
	}

	sensor_dbg("switch_ctrl = %d, mipi_switch_status = %d\n",
			sensor_mipi_switch_info->switch_ctrl, sensor_mipi_switch_info->mipi_switch_status);
	switch (sensor_mipi_switch_info->switch_ctrl) {
	case SET_SWITCH:
		sensor_mipi_set_switch_select(sd, sensor_mipi_switch_info);
		break;
	case GET_SWITCH:
		sensor_mipi_get_switch_status(sd, &sensor_mipi_switch_info->mipi_switch_status);
		break;
	default:
		sensor_err("Invaild sensor_mipi_switch_ctrl!!!\n");
		break;
	}

	return 0;
}

static int sensor_mipi_switch_on(struct v4l2_subdev *sd, switch_choice_type switch_choice)
{
	//int ret;
	//ret = vin_gpio_set_status(sd, MIPI_SWITCH_GPIO, 1);
	//sensor_print("mipi_switch tune to on, ret = %d!!!\n", ret);

	return 0;
}

static int sensor_mipi_switch_off(struct v4l2_subdev *sd, switch_choice_type switch_choice)
{
	//int ret = 0;
	struct sensor_switch_cfg_t *sensor_switch_config;

	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];
	sensor_switch_config->sensor_with_switch_en = false;
	sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_A_STATUS;
	sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
	//ret = vin_gpio_set_status(sd, MIPI_SWITCH_GPIO, 0);
	//sensor_print("mipi_switch tune to off, ret = %d!!!\n", ret);
	return 0;
}
#endif

/*
 * Code for dealing with controls.
 * fill with different sensor module
 * different sensor module has different settings here
 * if not support the follow function ,retrun -EINVAL
 */

static int f37p_sensor_vts;

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	//unsigned char explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);
	int		tmp_exp_val;

	tmp_exp_val = exp_val / 16;

	sensor_write(sd, 0x02, (tmp_exp_val >> 8) & 0xFF);
	sensor_write(sd, 0x01, (tmp_exp_val) & 0xFF);

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
	int	again = 0;
	int	tmp = 0;
	unsigned char	regdata = 0;

	/// gain: 16=1X, 32=2X, 48=3X, 64=4X, ......, 240=15X, 256=16X, ......
	again = gain;
	while (again > 31) {
		again = again >> 1;
		tmp++;
	}
	if (again > 15)
		again = again - 16;
	regdata = (unsigned char)((tmp<<4) | again);

	sensor_write(sd, 0x00, regdata & 0xFF);

	return 0;
}
static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	//	printk("f37p sensor gain value is %d\n",gain_val);

	if (gain_val == info->gain) {
		return 0;
	}

	//sensor_print( "gain_val:%d\n", gain_val );
	setSensorGain(sd, gain_val);
	info->gain = gain_val;

#ifdef _SOI_BSUN_OPT
	{
		if (gain_val >= 32) { // When AGain >= 2X
			if (BSunMode != 1) {
				sensor_write(sd, 0xC0, 0x2F);	//Reg2F[5]=0
				sensor_write(sd, 0xC1, 0x44);
				sensor_write(sd, 0xC2, 0x0C);	//Reg0C[6]=0
				sensor_write(sd, 0xC3, 0x00);
				sensor_write(sd, 0xC4, 0x82);	//Reg82[1]=0
				sensor_write(sd, 0xC5, 0x20);
				sensor_write(sd, 0x1F, 0x80);	// Trigger group write.
				BSunMode = 1;
			}
		} else { // When AGain < 2X
			if (BSunMode != 0) {
				sensor_write(sd, 0xC0, 0x2F);	//Reg2F[5]=1
				sensor_write(sd, 0xC1, 0x64);
				sensor_write(sd, 0xC2, 0x0C);	//Reg0C[6]=1
				sensor_write(sd, 0xC3, 0x40);
				sensor_write(sd, 0xC4, 0x82);	//Reg82[1]=1
				sensor_write(sd, 0xC5, 0x22);
				sensor_write(sd, 0x1F, 0x80);	// Trigger group write.
				BSunMode = 0;
			}
		}
	}
#endif
#ifdef _SOI_PRECHG_OPT
	{
		if (gain_val > 16) { // When AGain >= 1X
			if (PreChgMode != 1) {
				sensor_write(sd, 0xC0, 0x8A);
				sensor_write(sd, 0xC1, 0x06);
				sensor_write(sd, 0x1F, 0x80);	// Trigger group write.
				PreChgMode = 1;
			}
		} else { // When AGain = 1X
			if (PreChgMode != 0) {
				sensor_write(sd, 0xC0, 0x8A);
				sensor_write(sd, 0xC1, 0x04);
				sensor_write(sd, 0x1F, 0x80);	// Trigger group write.
				PreChgMode = 0;
			}
		}
	}
#endif

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd, struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	int shutter, frame_length;
	struct sensor_info *info = to_state(sd);
	struct sensor_switch_cfg_t *sensor_switch_config;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < 1 * 16)
		gain_val = 16;

	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	shutter = exp_val >> 4;
	if (shutter > f37p_sensor_vts - 4)
		frame_length = shutter + 4;
	else
		frame_length = f37p_sensor_vts;

#if MIPI_SWITCH_EN
	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];
	if ((exp_gain->r_gain >> 16) % 2 == 0) {
		//sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
		//sensor_print("get isp id is 1, set A sensor\n");
		sensor_switch_config->exp_val_save[SWITCH_SENSOR_A] = exp_val;
		sensor_switch_config->gain_val_save[SWITCH_SENSOR_A] = gain_val;
		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_A] = true;
		sensor_switch_config->frame_length_save[SWITCH_SENSOR_A] = frame_length;
	} else if ((exp_gain->r_gain >> 16) % 2 == 1) {
		//sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
		//sensor_print("get isp id is 2, set B sensor\n");
		sensor_switch_config->exp_val_save[SWITCH_SENSOR_B] = exp_val;
		sensor_switch_config->gain_val_save[SWITCH_SENSOR_B] = gain_val;
		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_B] = true;
		sensor_switch_config->frame_length_save[SWITCH_SENSOR_B] = frame_length;
	} else {
		sensor_err("get isp id is %d\n", exp_gain->r_gain >> 16);
		return -1;
	}
#else
	//sensor_print("frame_length = %d\n", frame_length);
	/* write vts */
	sensor_write(sd, 0x22, frame_length & 0xff);
	sensor_write(sd, 0x23, frame_length >> 8);

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
#endif

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

#if MIPI_SWITCH_EN
static void sensor_switch_change(struct v4l2_subdev *sd)
{
	unsigned int max_frame_length;
	struct sensor_switch_cfg_t *sensor_switch_config;

	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];

#if 1
	if (GPIO_SWITCH_SENSOR_A_STATUS == sensor_switch_config->switch_status) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
		//sensor_write(sd, 0x6C, 0xC4); //Mipi interface power down, drive min
		sensor_write(sd, 0xC0, 0x6C);
		sensor_write(sd, 0xC1, 0xC4);
		sensor_write(sd, 0x1F, 0x80);
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
		//sensor_write(sd, 0x6C, 0x48); //Mipi interface power on, drive max
		sensor_write(sd, 0xC0, 0x6C);
		sensor_write(sd, 0xC1, 0x48);
		sensor_write(sd, 0x1F, 0x80);
		//sensor_print("switch_status = %d\n", sensor_switch_config->switch_status);
		sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_B_STATUS;
	} else if (GPIO_SWITCH_SENSOR_B_STATUS == sensor_switch_config->switch_status) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
		//sensor_write(sd, 0x6C, 0xC4); //Mipi interface power down, drive min
		sensor_write(sd, 0xC0, 0x6C);
		sensor_write(sd, 0xC1, 0xC4);
		sensor_write(sd, 0x1F, 0x80);
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
		//sensor_write(sd, 0x6C, 0x48);  //Mipi interface power on, drive max
		sensor_write(sd, 0xC0, 0x6C);
		sensor_write(sd, 0xC1, 0x48);
		sensor_write(sd, 0x1F, 0x80);
		//sensor_print("switch_status = %d\n", sensor_switch_config->switch_status);
		sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_A_STATUS;
	}
#endif
	max_frame_length = max(sensor_switch_config->frame_length_save[SWITCH_SENSOR_A], sensor_switch_config->frame_length_save[SWITCH_SENSOR_B]);
	sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
	/* write vts */
	sensor_write(sd, 0x22, max_frame_length & 0xff);
	sensor_write(sd, 0x23, max_frame_length >> 8);

	sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
	/* write vts */
	sensor_write(sd, 0x22, max_frame_length & 0xff);
	sensor_write(sd, 0x23, max_frame_length >> 8);

	if (sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_A]) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
		sensor_s_exp(sd, sensor_switch_config->exp_val_save[SWITCH_SENSOR_A]);
		sensor_s_gain(sd, sensor_switch_config->gain_val_save[SWITCH_SENSOR_A]);

		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_A] = false;
	}
	if (sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_B]) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
		sensor_s_exp(sd, sensor_switch_config->exp_val_save[SWITCH_SENSOR_B]);
		sensor_s_gain(sd, sensor_switch_config->gain_val_save[SWITCH_SENSOR_B]);

		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_B] = false;
	}

}
#endif
/*
 *set && get sensor flip
 */
#if 0
static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);
	data_type get_value;
	data_type flip_mirror;

	sensor_read(sd, 0x012, &get_value);
	flip_mirror = (get_value >> 4) & 0x3;
	// bit[5]bit[4] 00:normal 01:flip 10:mirror 11:flip&mirror
	switch (flip_mirror) {
	case 0x00:
		*code = MEDIA_BUS_FMT_SGRBG10_1X10;
		break;
	case 0x01:
		*code = MEDIA_BUS_FMT_SRGGB10_1X10;
		break;
	case 0x10:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	case 0x11:
		*code = MEDIA_BUS_FMT_SGBRG10_1X10;
		break;
	default:
		*code = info->fmt->mbus_code;
	}
	return 0;
}
#endif

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;
	data_type value_12;
	int times_out = 5;
	int eRet;
#if MIPI_SWITCH_EN
	int current_switch_choice;
	unsigned int i2c_addr;
	int switch_choice;
	int ret = 0;
	struct sensor_switch_cfg_t *sensor_switch_config;
#endif

	if (!(enable == 0 || enable == 1))
		return -1;

	get_value = sensor_flip_status;
	//printk("--sensor hfilp set[%d] read value:0x%X --\n", enable, get_value);
	if (enable)
		set_value = get_value | 0x10;
	else
		set_value = get_value & 0xEF;

#if MIPI_SWITCH_EN
	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];
	current_switch_choice = sensor_switch_config->switch_status;
	for (switch_choice = SWITCH_SENSOR_A; switch_choice < SWITCH_SENSOR_MAX; switch_choice++) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[switch_choice]);
		sensor_print("[sensor_s_hflip] enable = %d sensor = 0x%x set_value = %x, ret = %d\n", enable, sensor_switch_config->i2c_addr_container[switch_choice], set_value, ret);
		do {
			/* write repeatly */
			sensor_write(sd, 0x12, set_value);
			eRet = sensor_read(sd, 0x12, &value_12);
			usleep_range(10000, 30000);
			times_out--;
		} while ((value_12 != set_value) && (times_out >= 0));

		if ((times_out < 0) && (value_12 != set_value)) {
			sensor_err("set hflip failed, please set more times!!!\n");
			sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[current_switch_choice]);
			sensor_i2c_addr_get(sd, &i2c_addr);
			sensor_print("current sensor is %d, reset i2c's add  = 0x%x when hflip failed\n", current_switch_choice, i2c_addr);
			return -1;
		} else {
			times_out = 5;
			sensor_print("hflip finish, set_value : 0x%x, value_12 = 0x%x\n",
				set_value, value_12);
		}
	}
	sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[current_switch_choice]);
	sensor_i2c_addr_get(sd, &i2c_addr);
	sensor_print("current sensor is %d, the i2c's add  = 0x%x when finish hflip \n", current_switch_choice, i2c_addr);
#else

	sensor_flip_status = set_value;
	//printk("--set sensor hfilp the value:0x%X \n--", set_value);
	sensor_write(sd, 0x12, set_value);
#endif

	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;
	data_type value_12;
	int times_out = 5;
	int eRet;
#if MIPI_SWITCH_EN
	int current_switch_choice;
	unsigned int i2c_addr;
	int switch_choice;
	int ret = 0;
	struct sensor_switch_cfg_t *sensor_switch_config;
#endif

	if (!(enable == 0 || enable == 1))
		return -1;

	get_value = sensor_flip_status;
	//printk("--sensor vfilp set[%d] read value:0x%X --\n", enable, get_value);
	if (enable)
		set_value = get_value | 0x20;
	else
		set_value = get_value & 0xDF;

#if MIPI_SWITCH_EN
	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];
	current_switch_choice = sensor_switch_config->switch_status;
	for (switch_choice = SWITCH_SENSOR_A; switch_choice < SWITCH_SENSOR_MAX; switch_choice++) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[switch_choice]);

		sensor_print("[sensor_s_vflip] enable = %d sensor = 0x%x set_value = %x, ret = %d\n", enable, sensor_switch_config->i2c_addr_container[switch_choice], set_value, ret);
		do {
			/* write repeatly */
			sensor_write(sd, 0x12, set_value);
			eRet = sensor_read(sd, 0x12, &value_12);

			sensor_print("[V] eRet:%d, value_12 = 0x%x, times_out:%d\n", eRet, value_12, times_out);
			usleep_range(10000, 30000);
			times_out--;
		} while ((value_12 != set_value) && (times_out >= 0));

		if ((times_out < 0) && (value_12 != set_value)) {
			sensor_err("set vflip failed, please set more times!!!\n");
			sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[current_switch_choice]);
			sensor_i2c_addr_get(sd, &i2c_addr);
			sensor_print("current sensor is %d, reset the i2c's add  = 0x%x when vflip failed\n", current_switch_choice, i2c_addr);
			return -1;
		} else {
			times_out = 5;
			sensor_print("vflip finish, set_value : 0x%x, value_12 = 0x%x\n",
				set_value, value_12);
		}
	}
	sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[current_switch_choice]);
	sensor_i2c_addr_get(sd, &i2c_addr);
	sensor_print("current sensor is %d, the i2c's add  = 0x%x when finish vflip \n", current_switch_choice, i2c_addr);
#else

	sensor_flip_status = set_value;
	//printk("--set sensor vfilp the value:0x%X --\n", set_value);
	sensor_write(sd, 0x12, set_value);

#endif
	return 0;

}


/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	struct sensor_switch_cfg_t *sensor_switch_config;

	switch (on) {
	case STBY_ON:
		sensor_print("STBY_ON!\n");
		cci_lock(sd);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_print("STBY_OFF!\n");
		cci_lock(sd);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		usleep_range(10000, 12000);
		break;
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);        /// Pull up PWDN pin initially.
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);        /// Pull down RESET# pin initially.
		usleep_range(1000, 1200);
		#if 0//sk
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
		#endif
		//vin_set_pmu_channel(sd, CMBCSI, ON);
		vin_set_pmu_channel(sd, AVDD, ON);              /// Turn on AVDD.
		usleep_range(100, 120);
		vin_set_pmu_channel(sd, DVDD, ON);             /// DVDD is controlled internally.
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, IOVDD, ON);              /// Turn on DOVDD.
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);	    /// Pull up RESET# pin.
		usleep_range(1000, 1200);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
#if MIPI_SWITCH_EN
		sensor_mipi_switch_on(sd, SWITCH_SENSOR_A);
#endif
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!do nothing\n");
		break;
		cci_lock(sd);

		usleep_range(10000, 12000);					/// > 512 MCLK cycles
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);		/// Pull up PWDN pin.
		usleep_range(1000, 1200);						/// Add delay.
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);		/// Pull down RESET# pin.
		usleep_range(1000, 1200);						/// Add delay.

		vin_set_mclk(sd, OFF);						/// Disable MCLK.
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);

		usleep_range(1000, 1200);						/// Add delay.
#if MIPI_SWITCH_EN
		sensor_mipi_switch_off(sd, SWITCH_SENSOR_A);
		if (strcmp(sd->name, SENSOR_NAME_2))
			sensor_switch_config = &sensor_switch_cfg[0];
		else
			sensor_switch_config = &sensor_switch_cfg[1];
		sensor_switch_config->sensor_is_detected = false;
#endif
		vin_set_pmu_channel(sd, IOVDD, OFF);			/// Turn off DOVDD.
		vin_set_pmu_channel(sd, DVDD, OFF);

		vin_set_pmu_channel(sd, AVDD, OFF);			/// Turn off AVDD.
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
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

	sensor_print("%s: val=%d\n", __func__, val);
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
	data_type rdval;
	int eRet;
	int times_out = 3;

	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (client == NULL) {
		sensor_err("client is NULL!!!\n");
		return -1;
	}
	/* i2c addr useful for 7bit */
	sensor_print("Start detect device I2Caddr = 0x%x\n", client->addr << 1);

	do {
		eRet = sensor_read(sd, ID_REG_HIGH, &rdval);
		sensor_print("f37p_mipi eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval, times_out);
		usleep_range(2000, 2200);
		times_out--;
	} while (eRet < 0 && times_out > 0);

	sensor_read(sd, ID_REG_HIGH, &rdval);
	sensor_print("ID_VAL_HIGH = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;
	sensor_read(sd, ID_REG_LOW, &rdval);
	sensor_print("ID_VAL_LOW = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	sensor_print("Done!\n");
	return 0;
}

#if MIPI_SWITCH_EN
static int sensor_init_with_switch(struct v4l2_subdev *sd)
{
	struct sensor_info *info = to_state(sd);
	unsigned int switch_choice;
	unsigned int sensor_num = 0;
	int ret;
	struct sensor_switch_cfg_t *sensor_switch_config;

	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];

	sensor_print("sensor_init_with_switch\n");
	for (switch_choice = SWITCH_SENSOR_A; switch_choice < SWITCH_SENSOR_MAX; switch_choice++) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[switch_choice]);
		/*Make sure it is a target sensor */
		ret = sensor_detect(sd);
		if (ret) {
			sensor_err("chip found is not an target chip, iic_addr = 0x%x\n",
					sensor_switch_config->i2c_addr_container[switch_choice]);
		} else {
			sensor_print("[0:A, 1:B] detect sensor%d i2c_addr = 0x%x\n", switch_choice, sensor_switch_config->i2c_addr_container[switch_choice]);
			sensor_num++;
		}
	}
	sensor_print("detect sensor num : %d\n", sensor_num);
	if (sensor_num == 0) {
		sensor_switch_config->sensor_is_detected = false;
		return -ENODEV;
	} else if (sensor_num > 1) {
		sensor_switch_config->sensor_with_switch_en = true;
		sensor_print("maybe with mipi switch, so sensor_with_switch_en = %d\n", sensor_switch_config->sensor_with_switch_en);
	} else {
		sensor_switch_config->sensor_with_switch_en = false;
		sensor_print("without mipi switch, so sensor_with_switch_en = %d\n", sensor_switch_config->sensor_with_switch_en);
	}
	sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
	sensor_switch_config->sensor_is_detected = true;

	info->focus_status = 0;
	info->low_speed    = 0;
	info->width	   = 1920;
	info->height	   = 1080;
	info->hflip	   = 0;
	info->vflip	   = 0;
	info->gain	   = 0;
	info->exp	   = 0;
	info->tpf.numerator	 = 1;
	info->tpf.denominator	 = 20;	/* 30fps */
	info->preview_first_flag = 1;

	return 0;
}
#endif


static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);
	struct sensor_switch_cfg_t *sensor_switch_config;

	printk("***********sensor init************\r\n");
	sensor_dbg("sensor_init\n");

#if MIPI_SWITCH_EN
	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];
	if (!sensor_switch_config->sensor_is_detected) {
		ret = sensor_init_with_switch(sd);
	} else {
		sensor_print("sensor is detected and will not detect repeatedly\n");
	}
#else
	/* Make sure it is a target sensor */
	ret = sensor_detect(sd);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}
#endif
	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 1920;
	info->height = 1080;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;
	/* sensor_print("info->settle_time = 0x%x\n", info->settle_time); */
	info->tpf.numerator = 1;
	info->tpf.denominator = 20;	/* 30fps */
	info->preview_first_flag = 1;
	return 0;
}

static int sensor_get_temp(struct v4l2_subdev *sd,  struct sensor_temp *temp)
{
#ifdef CONFIG_THERMAL
	int ret;
	int temp1;
	struct thermal_zone_device *thermal_dev = NULL;

	thermal_dev = thermal_zone_get_zone_by_name("cpu_thermal_zone");
	 if (IS_ERR(thermal_dev)) {
	       sensor_err("thermal_zone_get_zone_by_name fail \n");
	       //ERR_PTR(-EPROBE_DEFER);
	       return -1;
	 } else {
	       ret = thermal_zone_get_temp(thermal_dev, &temp1);
	       if (ret != 0) {
			sensor_err("get temp erro \n");
			return -1;
		   }

		sensor_dbg("thermal temp :%d \n", temp1);
	 }
	 temp->temp = temp1 / 1000;
#else
	temp->temp = 25;
#endif

	return 0;
}
static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);
	struct sensor_switch_cfg_t *sensor_switch_config;

	switch (cmd) {
	case GET_CURRENT_WIN_CFG:
		sensor_dbg("%s: GET_CURRENT_WIN_CFG, info->current_wins=%p\n", __func__, info->current_wins);

		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins, sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empty wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		break;
#if MIPI_SWITCH_EN
	case SET_SWITCH_CHANGE:
		sensor_i2c_switch_lock_lock(); //防止设置寄存器的时候被切换
		sensor_switch_change(sd);
		sensor_i2c_switch_lock_unlock();  //解锁
		break;
	case VIDIOC_VIN_SENSOR_MIPI_SWITCH:
		sensor_i2c_switch_lock_lock(); //防止设置寄存器的时候被切换
		if (strcmp(sd->name, SENSOR_NAME_2))
			sensor_switch_config = &sensor_switch_cfg[0];
		else
			sensor_switch_config = &sensor_switch_cfg[1];
		if (sensor_switch_config->sensor_with_switch_en) {
			sensor_mipi_switch_ctrl(sd, (struct sensor_mipi_switch_entity *)arg);
		} else {
			sensor_err("Without mipi_switch, please check!!!\n");
		}
		sensor_i2c_switch_lock_unlock();  //解锁
		break;
#endif
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
#if MIPI_SWITCH_EN
		sensor_i2c_switch_lock_lock(); //防止设置寄存器的时候被切换
#endif
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
#if MIPI_SWITCH_EN
		sensor_i2c_switch_lock_unlock();  //解锁
#endif
		break;
	//case VIDIOC_VIN_SENSOR_SET_FPS:
//		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
		//break;
	case VIDIOC_VIN_SENSOR_GET_TEMP:
		sensor_get_temp(sd, (struct sensor_temp *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
#if 0
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
#endif
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
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,/*.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,*/
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
		.width      = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 2560,
		.vts        = 1125,
		.pclk       = 86.4*1000*1000,
		.mipi_bps	 = 432*1000*1000,
		.fps_fixed  = 30,
	//	 .if_mode = MIPI_VC_WDR_MODE,
	//	 .wdr_mode = ISP_DOL_WDR_MODE,
		.bin_factor = 1,
		.intg_min   = 1<<4,
		.intg_max   = 1125<<4,
		.gain_min   = 1<<4,
		.gain_max	 = 15<<4,
		.regs = sensor_1080p30_regs,
		.regs_size = ARRAY_SIZE(sensor_1080p30_regs),
		.set_size = NULL,
	},

	{
		.width      = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 3840,
		.vts        = 1125,
		.pclk       = 86.4*1000*1000,
		.mipi_bps	 = 432*1000*1000,
		.fps_fixed  = 20,
		.bin_factor = 1,
		.intg_min   = 1<<4,
		.intg_max   = 1125<<4,
		.gain_min   = 1<<4,
		.gain_max	 = 15<<4,
		.regs = sensor_1080p20_regs,
		.regs_size = ARRAY_SIZE(sensor_1080p20_regs),
		.set_size = NULL,
	},

	{
		.width      = 1920,
		.height     = 1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 3200,
		.vts        = 1350,
		.pclk       = 64.8*1000*1000,
		.mipi_bps	 = 324*1000*1000,
		.fps_fixed  = 15,
		.bin_factor = 1,
		.intg_min   = 1<<4,
		.intg_max   = 1125<<4,
		.gain_min   = 1<<4,
		.gain_max	 = 15<<4,
		.regs = sensor_1080p15_regs,
		.regs_size = ARRAY_SIZE(sensor_1080p15_regs),
		.set_size = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
#if MIPI_SWITCH_EN
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
#else
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#endif

	return 0;
}

#if 0
static int sensor_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	/* Fill in min, max, step and default value for these controls. */
	/* see include/linux/videodev2.h for details */

	switch (qc->id) {
	case V4L2_CID_GAIN:
		return v4l2_ctrl_query_fill(qc, 1 * 16, 256 * 16, 1, 16);
	case V4L2_CID_EXPOSURE:
		return v4l2_ctrl_query_fill(qc, 0, 65535 * 16, 1, 0);
	}
	return -EINVAL;
}
#endif

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info = container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;
	int ret;

#if MIPI_SWITCH_EN
	sensor_i2c_switch_lock_lock(); //防止设置寄存器的时候被切换
#endif

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ret =  sensor_g_gain(sd, &ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret =  sensor_g_exp(sd, &ctrl->val);
		break;
	default:
		ret = -EINVAL;
		break;
	}
#if MIPI_SWITCH_EN
	sensor_i2c_switch_lock_unlock();  //解锁
#endif

	return ret;

}

static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
#if 0
	struct v4l2_queryctrl qc;
	int ret;
#endif

	struct sensor_info *info = container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;
	int ret;

#if MIPI_SWITCH_EN
	sensor_i2c_switch_lock_lock(); //防止设置寄存器的时候被切换
#endif

#if 0
	qc.id = ctrl->id;
	ret = sensor_queryctrl(sd, &qc);

	if (ret < 0)
		return ret;

	if (ctrl->val < qc.minimum || ctrl->val > qc.maximum)
		return -ERANGE;
#endif

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		ret =  sensor_s_gain(sd, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		ret =  sensor_s_exp(sd, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		ret =  sensor_s_hflip(sd, ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		ret =  sensor_s_vflip(sd, ctrl->val);
		break;
	default:
		ret =  -EINVAL;
		break;
	}

#if MIPI_SWITCH_EN
	sensor_i2c_switch_lock_unlock();  //解锁
#endif
	return ret;

}

#if MIPI_SWITCH_EN
static int sensor_reg_init_with_switch(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;
	int switch_choice;
	unsigned int i2c_addr;
	data_type rdval;
	struct sensor_switch_cfg_t *sensor_switch_config;

	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];

	sensor_i2c_switch_lock_lock(); //防止设置寄存器的时候被切换

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		sensor_i2c_switch_lock_unlock();
		return ret;
	}

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);


	sensor_flip_status = 0x20; /// Normal mode (mirror off, flip off)
#if 0
	Reg27 = 0x52; ///
	Reg28 = 0x10; ///
	Reg46 = 0x00; ///
	Reg80 = 0x41; ///
#endif

	sensor_switch_config->vsync_cnt = 0;

	for (switch_choice = SWITCH_SENSOR_A; switch_choice < SWITCH_SENSOR_MAX; switch_choice++) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[switch_choice]);
		sensor_print("ready to write regs for i2c_addr:0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
		if (wsize->regs) {
			sensor_dbg("%s: start sensor_write_array(wsize->regs)\n", __func__);
			sensor_write_array(sd, wsize->regs, wsize->regs_size);
		}
		if (wsize->set_size)
			wsize->set_size(sd);
	}

	sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
	sensor_i2c_addr_get(sd, &i2c_addr);
	sensor_write(sd, 0x1E, 0x00); //Vsync Input
	sensor_write(sd, 0x6C, 0xC4); //Mipi interface power down
	//sensor_write(sd, 0x0C, 0x01); //test mode
	sensor_write(sd, 0x80, 0x81); //slave mode

	sensor_read(sd, 0x89, &rdval);
	sensor_write(sd, 0x89, 0x04); //Virtual Channel 1

	sensor_print("Mipi interface power down & slave mode for i2c_addr:0x%x, VC 0x%x\n", i2c_addr, rdval);

	for (switch_choice = SWITCH_SENSOR_A; switch_choice < SWITCH_SENSOR_MAX; switch_choice++) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[switch_choice]);
		sensor_write(sd, 0x12, 0x00);	//stream on
	}

	sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_A_STATUS;
	sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);

	sensor_i2c_switch_lock_unlock();  //解锁

	info->width = wsize->width;
	info->height = wsize->height;
	f37p_sensor_vts = wsize->vts;
	sensor_dbg("f37p_sensor_vts = %d\n", f37p_sensor_vts);

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width,
			 wsize->height);

	return 0;
}
#endif

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	//data_type rdval_l, rdval_h, value_12;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;

	sensor_dbg("sensor_reg_init, ARRAY_SIZE(sensor_default_regs)=%d\n", ARRAY_SIZE(sensor_default_regs));

	ret = sensor_write_array(sd, sensor_default_regs,
				 ARRAY_SIZE(sensor_default_regs));
	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	sensor_dbg("sensor_reg_init, wsize=%p, wsize->regs=0x%x, wsize->regs_size=%d\n", wsize, wsize->regs, wsize->regs_size);

#if 0
	sensor_print("f37p: sensor%d soft reset start\n");
	eRet = sensor_read(sd, 0x12, &value_12);
	value_12 = 	value_12 | 0X80;
	sensor_write(sd, 0x12, value_12); //soft reset sensor
	usleep_range(50000, 50000);
	value_12 = value_12 & 0X7F;
	sensor_write(sd, 0x12, value_12);
	usleep_range(50000, 50000);
	sensor_print("f37p: sensor%d soft reset end\n");
#endif

	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	sensor_flip_status = 0x20; /// Normal mode (mirror off, flip off)
#if 0
	Reg27 = 0x52; ///
	Reg28 = 0x10; ///
	Reg46 = 0x00; ///
	Reg80 = 0x41; ///
#endif

	if (wsize->regs) {
		sensor_dbg("%s: start sensor_write_array(wsize->regs)\n", __func__);
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
	}
	/* sensor_read_array_test(sd, wsize->regs, wsize->regs_size); */
	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	f37p_sensor_vts = wsize->vts;
	sensor_dbg("f37p_sensor_vts = %d\n", f37p_sensor_vts);

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width, wsize->height);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_vysnc_config *sensor_vysnc_cfg = &info->sensor_vysnc_cfg;
	struct sensor_switch_cfg_t *sensor_switch_config;
	int ret;

	sensor_print("%s on = %d, %d*%d fps: %d code: %x, wdr mode %d\n", __func__, enable,
		     info->current_wins->width, info->current_wins->height,
		     info->current_wins->fps_fixed, info->fmt->mbus_code, info->current_wins->wdr_mode);

	if (!enable) {
		if (sensor_vysnc_cfg->gpio_irq > 0)
			disable_irq(sensor_vysnc_cfg->gpio_irq);
		cancel_work_sync(&sensor_vysnc_cfg->s_sensor_switch_change_task);
		return 0;
	}

#if MIPI_SWITCH_EN
	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];
	if (sensor_switch_config->sensor_with_switch_en) {
		/* detect more than 1 sensor on board, maybe with mipi switch */
		ret = sensor_reg_init_with_switch(info);
		if (sensor_vysnc_cfg->gpio_irq > 0)
			enable_irq(sensor_vysnc_cfg->gpio_irq);
		return ret;
	} else {
		/* only 1 sensor on mipi data */
		return sensor_reg_init(info);
	}
#else
	return sensor_reg_init(info);
#endif

}

/* ----------------------------------------------------------------------- */
static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.g_volatile_ctrl = sensor_g_ctrl,
	.s_ctrl = sensor_s_ctrl,
	.try_ctrl = sensor_try_ctrl,
	//.queryctrl = sensor_queryctrl,
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
		//.addr_width = CCI_BITS_16,
		.addr_width = CCI_BITS_8,
		.data_width = CCI_BITS_8,
	}, {
		.name = SENSOR_NAME_2,
		//.addr_width = CCI_BITS_16,
		.addr_width = CCI_BITS_8,
		.data_width = CCI_BITS_8,
	}
};

#if MIPI_SWITCH_EN
static void __s_sensor_switch_change_handle(struct work_struct *work)
{
	struct sensor_vysnc_config *sensor_vysnc_cfg = container_of(work, struct sensor_vysnc_config, s_sensor_switch_change_task);
	struct sensor_info *info = container_of(sensor_vysnc_cfg, struct sensor_info, sensor_vysnc_cfg);
	struct v4l2_subdev *sd = &info->sd;

	if (!sd || !sd->entity.use_count)
		return;

	sensor_i2c_switch_lock_lock(); //防止设置寄存器的时候被切换
	sensor_switch_change(sd);
	sensor_i2c_switch_lock_unlock();  //解锁
}

static irqreturn_t sensor_vsync_irq_func(int irq, void *priv)
{
	struct v4l2_subdev *sd = priv;
	struct sensor_info *info = to_state(sd);
	struct sensor_vysnc_config *sensor_vysnc_cfg = &info->sensor_vysnc_cfg;
	struct sensor_switch_cfg_t *sensor_switch_config;
	unsigned long flags;

	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_switch_config = &sensor_switch_cfg[0];
	else
		sensor_switch_config = &sensor_switch_cfg[1];

	spin_lock_irqsave(&info->slock, flags);
	sensor_switch_config->vsync_cnt++;
	if (sensor_switch_config->vsync_cnt < 5) {
		spin_unlock_irqrestore(&info->slock, flags);
		return IRQ_HANDLED;
	}
	schedule_work(&sensor_vysnc_cfg->s_sensor_switch_change_task);
	spin_unlock_irqrestore(&info->slock, flags);

	//sensor_print("%s vysnc come\n", sd->name);

	return IRQ_HANDLED;
}

static int sensor_vysnc_init(struct v4l2_subdev *sd)
{
	int ret;
	struct device_node *np = NULL;
	enum of_gpio_flags gc;
	struct sensor_info *info = to_state(sd);
	struct sensor_vysnc_config *sensor_vysnc_cfg = &info->sensor_vysnc_cfg;
	char *node_name0 = "sensor_vsync0";
	char *node_name1 = "sensor_vsync1";
	char *vsync_gpio_name = "vsync_gpio";

	if (strcmp(sd->name, SENSOR_NAME_2)) {
		np = of_find_node_by_name(NULL, node_name0);
		if (np == NULL) {
			sensor_err("can not find the %s node\n", node_name0);
			return -EINVAL;
		} else
			sensor_print("find the %s node\n", node_name0);
	} else {
		np = of_find_node_by_name(NULL, node_name1);
		if (np == NULL) {
			sensor_err("can not find the %s node\n", node_name1);
			return -EINVAL;
		} else
			sensor_print("find the %s node\n", node_name1);
	}

	sensor_vysnc_cfg->gpio = of_get_named_gpio_flags(np, vsync_gpio_name, 0, &gc);
	//sensor_print("get form %s gpio is %d\n", vsync_gpio_name, sensor_vysnc_cfg->gpio);
	if (!gpio_is_valid(sensor_vysnc_cfg->gpio)) {
		sensor_err("fetch %s from device_tree failed\n", vsync_gpio_name);
		return -ENODEV;
	} else {
		ret = gpio_request(sensor_vysnc_cfg->gpio, NULL);
		if (ret < 0) {
			sensor_err("request %s fail!\n", vsync_gpio_name);
			return -1;
		}
		gpio_direction_input(sensor_vysnc_cfg->gpio);

		sensor_vysnc_cfg->gpio_irq = gpio_to_irq(sensor_vysnc_cfg->gpio);
		if (sensor_vysnc_cfg->gpio_irq <= 0) {
			sensor_err("gpio %d get irq err\n", sensor_vysnc_cfg->gpio);
			return -1;
		}
		ret = request_irq(sensor_vysnc_cfg->gpio_irq, sensor_vsync_irq_func,
				IRQF_TRIGGER_FALLING, //| IRQF_TRIGGER_RISING,
				strcmp(sd->name, SENSOR_NAME_2) ? node_name0 : node_name1, sd);
		disable_irq(sensor_vysnc_cfg->gpio_irq);
	}

	INIT_WORK(&sensor_vysnc_cfg->s_sensor_switch_change_task, __s_sensor_switch_change_handle);
	return 0;
}

static void sensor_vysnc_exit(struct v4l2_subdev *sd)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_vysnc_config *sensor_vysnc_cfg = &info->sensor_vysnc_cfg;

	cancel_work_sync(&sensor_vysnc_cfg->s_sensor_switch_change_task);
	if (sensor_vysnc_cfg->gpio_irq > 0) {
		disable_irq(sensor_vysnc_cfg->gpio_irq);
		free_irq(sensor_vysnc_cfg->gpio_irq, sd);
	}

	if (gpio_is_valid(sensor_vysnc_cfg->gpio))
		gpio_free(sensor_vysnc_cfg->gpio);
}
#endif

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

static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	spin_lock_init(&info->slock);

#if MIPI_SWITCH_EN
	if (strcmp(sd->name, SENSOR_NAME_2))
		sensor_i2c_switch_lock_init();
	sensor_vysnc_init(sd);
#endif

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
	// use CMB_PHYA_OFFSET2  also ok
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
	//info->combo_mode = CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->time_hs = 0x20;
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

#if MIPI_SWITCH_EN
	sensor_vysnc_exit(sd);
#endif

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
