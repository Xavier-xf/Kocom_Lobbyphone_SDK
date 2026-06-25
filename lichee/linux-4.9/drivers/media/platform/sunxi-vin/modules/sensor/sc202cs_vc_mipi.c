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
#include <linux/types.h>

#include "camera.h"
#include "sensor_helper.h"

MODULE_AUTHOR("joker");
MODULE_DESCRIPTION("A low-level driver for SC202CS sensors");
MODULE_LICENSE("GPL");

#define MCLK            (24*1000*1000)
#define V4L2_IDENT_SENSOR   0x2355
#define MIPI_SWITCH_EN 1

//define the registers
#define SC202CS_EXP_H_ADDR                    0x3e00
#define SC202CS_EXP_M_ADDR                    0x3e01
#define SC202CS_EXP_L_ADDR                    0x3e02
#define SC202CS_AGAIN_ADDR                    0x3e09
#define SC202CS_DGAIN_ADDR                    0x3e06
#define SC202CS_DGAIN_FINE_ADDR               0x3e07
#define GAIN_STEP_BASE 128 //mean gain min step is 1/128gain

#define ID_REG_HIGH 0x3e07
#define ID_REG_LOW  0x3e08
#define ID_VAL_HIGH 0x80
#define ID_VAL_LOW  0x0

#define I2C_ADDR 0x60

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc202cs_mipi"
#define SENSOR_NAME_2 "sc202cs_mipi_2"


#define SC202CS_1600X1200_15FPS
//#define SC202CS_800X600_30FPS

#define A_I2C_ADDR 0x6c
#define B_I2C_ADDR 0x20
#define GPIO_SWITCH_SENSOR_A_STATUS CSI_GPIO_HIGH
#define GPIO_SWITCH_SENSOR_B_STATUS CSI_GPIO_LOW
static DEFINE_MUTEX(device_mutex);

struct sensor_switch_cfg_t {
	bool sensor_is_detected;
	bool sensor_with_switch_en;
	unsigned int sensor_num;
	unsigned int switch_status;
	unsigned int i2c_addr_container[2];
	unsigned int frame_length_save[2];
	unsigned int exp_val_save[2];
	unsigned int gain_val_save[2];
	bool set_expgain_flags[2];
	bool sensor_detect_flag[2];
	int sensor_hflip_status[2];
	int sensor_vflip_status[2];
	unsigned int vsync_cnt;
};

typedef enum _sensor_switch_choice_type {
	SWITCH_SENSOR_A = 0,
	SWITCH_SENSOR_B = 1,
	SWITCH_SENSOR_MAX,
} sensor_switch_choice_type;

static struct sensor_switch_cfg_t sensor_switch_cfg[2] = {
	[0] = {
		.i2c_addr_container = {A_I2C_ADDR, B_I2C_ADDR},
		.switch_status = 0,
	},
};

/*
 * The default register settings
 */
static struct regval_list sensor_default_regs[] = {

};

#ifdef SC202CS_800X600_30FPS
static struct regval_list sensor_800x600_30_regs[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36ea, 0x0f},
	{0x36eb, 0x24},
	{0x36ed, 0x14},
	{0x36e9, 0x01},
	{0x301f, 0x0c},
	{0x303f, 0x82},
	{0x3208, 0x03},
	{0x3209, 0x20},
	{0x320a, 0x02},
	{0x320b, 0x58},
	{0x320c, 0x07},//hts=1920
	{0x320d, 0x80},
	{0x320e, 0x04},//vts=1250	1920*1250*30=72
	{0x320f, 0xe2},
	{0x3211, 0x02},
	{0x3213, 0x02},
	{0x3215, 0x31},
	{0x3220, 0x01},
	{0x3221, 0x60},
	{0x3248, 0x02},
	{0x3253, 0x0a},
	{0x3301, 0xff},
	{0x3302, 0xff},
	{0x3303, 0x10},
	{0x3306, 0x28},
	{0x3307, 0x02},
	{0x330a, 0x00},
	{0x330b, 0xb0},
	{0x3318, 0x02},
	{0x3320, 0x06},
	{0x3321, 0x02},
	{0x3326, 0x12},
	{0x3327, 0x0e},
	{0x3328, 0x03},
	{0x3329, 0x0f},
	{0x3364, 0x4f},
	{0x33b3, 0x40},
	{0x33f9, 0x2c},
	{0x33fb, 0x38},
	{0x33fc, 0x0f},
	{0x33fd, 0x1f},
	{0x349f, 0x03},
	{0x34a6, 0x01},
	{0x34a7, 0x1f},
	{0x34a8, 0x40},
	{0x34a9, 0x30},
	{0x34ab, 0xa6},
	{0x34ad, 0xa6},
	{0x3622, 0x60},
	{0x3623, 0x40},
	{0x3624, 0x61},
	{0x3625, 0x08},
	{0x3626, 0x03},
	{0x3630, 0xa8},
	{0x3631, 0x84},
	{0x3632, 0x90},
	{0x3633, 0x43},
	{0x3634, 0x09},
	{0x3635, 0x82},
	{0x3636, 0x48},
	{0x3637, 0xe4},
	{0x3641, 0x22},
	{0x3670, 0x0f},
	{0x3674, 0xc0},
	{0x3675, 0xc0},
	{0x3676, 0xc0},
	{0x3677, 0x86},
	{0x3678, 0x88},
	{0x3679, 0x8c},
	{0x367c, 0x01},
	{0x367d, 0x0f},
	{0x367e, 0x01},
	{0x367f, 0x0f},
	{0x3690, 0x63},
	{0x3691, 0x63},
	{0x3692, 0x73},
	{0x369c, 0x01},
	{0x369d, 0x1f},
	{0x369e, 0x8a},
	{0x369f, 0x9e},
	{0x36a0, 0xda},
	{0x36a1, 0x01},
	{0x36a2, 0x03},
	{0x3900, 0x0d},
	{0x3904, 0x04},
	{0x3905, 0x98},
	{0x391b, 0x81},
	{0x391c, 0x10},
	{0x391d, 0x19},
	{0x3933, 0x01},
	{0x3934, 0x82},
	{0x3940, 0x5d},
	{0x3942, 0x01},
	{0x3943, 0x82},
	{0x3949, 0xc8},
	{0x394b, 0x64},
	{0x3952, 0x02},
	{0x3e00, 0x00},
	{0x3e01, 0x03},
	{0x3e02, 0xe0},
	{0x4502, 0x34},
	{0x4509, 0x30},
	{0x450a, 0x71},
	{0x4819, 0x09},
	{0x481b, 0x05},
	{0x481d, 0x13},
	{0x481f, 0x04},
	{0x4821, 0x0a},
	{0x4823, 0x05},
	{0x4825, 0x04},
	{0x4827, 0x05},
	{0x4829, 0x08},
	{0x5000, 0x46},
	{0x5900, 0x01},
	{0x5901, 0x04},
	{0x0100, 0x01},
};
#endif

#ifdef SC202CS_1600X1200_15FPS
static struct regval_list sensor_1600x1200_15_regs[] = {

};
#endif
static struct regval_list sensor_master_regs[] = {

	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36e9, 0x24},
	{0x301f, 0x02},
	{0x300a, 0x40},
	{0x3032, 0xa0},
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

	{0x320e, 0x0a},
	{0x320f, 0x1a},
	{0x4816, 0x61},//vc ch0

	{0x4509, 0x28},
	{0x450d, 0x61},
	{0x0100, 0x01},

};

static struct regval_list sensor_slave_regs[] = {

	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x36e9, 0x80},
	{0x36e9, 0x24},
	{0x301f, 0x03},
	{0x3222, 0x02},
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

	{0x320e, 0x0a},
	{0x320f, 0x1a},

	{0x3224, 0x83},
	{0x3230, 0x00},
	{0x3231, 0x3a},

	{0x322e, 0x09},//vts - RB rowa
	{0x322f, 0xe0},

	{0x4509, 0x28},
	{0x450d, 0x61},

	{0x4816, 0x65},//vc ch1

	{0x0100, 0x01},

};

#if MIPI_SWITCH_EN

static struct sensor_switch_cfg_t *sensor_mipi_switch_get_cfg(struct v4l2_subdev *sd)
{
	struct sensor_switch_cfg_t *sensor_switch_cfg_tmp = NULL;
	if (!sd) {
		sensor_err("sd is NULL!\n");
		return NULL;
	}

	if (strncmp(SENSOR_NAME, sd->name, strlen(SENSOR_NAME)) == 0) {
		sensor_switch_cfg_tmp = &sensor_switch_cfg[0];
		return sensor_switch_cfg_tmp;
	} else {
		sensor_err("It is no vaild sensor_switch_cfg_t\n");
		return NULL;
	}
}

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

#define GPIO_SWITCH_NUM 135 //PE7

static int init_gpio(void)
{
	int ret;

	ret = gpio_request(GPIO_SWITCH_NUM, "mipi_switch");
	if (ret) {
		printk(KERN_ERR "Failed to request GPIO pin\n");
		return ret;
	} else {
		sensor_print("============= request switch gpio success!\n");
	}

	ret = gpio_direction_output(GPIO_SWITCH_NUM, 0);
	if (ret) {
		printk(KERN_ERR "Failed to set GPIO direction\n");
		gpio_free(GPIO_SWITCH_NUM);
		return ret;
	}

	return 0;
}

static void control_gpio(int value)
{
    unsigned int read_data;
    unsigned int write_data;
    int bit_data;

    gpio_set_value(GPIO_SWITCH_NUM, value);
}

static void cleanup_gpio(void)
{
    gpio_free(GPIO_SWITCH_NUM);
}

#endif

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

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;

	return 0;
}

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	struct sensor_info *info = to_state(sd);

	exphigh = (unsigned char) (0x0f & (exp_val >> 16));
	expmid = (unsigned char) (0xff & (exp_val >> 8));
	explow = (unsigned char) (0xf0 & (exp_val << 0));

	sensor_write(sd, SC202CS_EXP_L_ADDR, explow);
	sensor_write(sd, SC202CS_EXP_M_ADDR, expmid);
	sensor_write(sd, SC202CS_EXP_H_ADDR, exphigh);

	sensor_dbg("exp_val = %d\n", exp_val);
	info->exp = exp_val;
	return 0;
}

static int sensor_g_gain(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	data_type ana_gain, dig_gain, fine_gain;

	*value = info->gain;

	sensor_read(sd, SC202CS_AGAIN_ADDR, &ana_gain);
	sensor_read(sd, SC202CS_DGAIN_ADDR, &dig_gain);
	sensor_read(sd, SC202CS_DGAIN_FINE_ADDR, &fine_gain);
	//sensor_print("sensor_g_gain ana_gain:%d, dig_gain:%d, fine_fain:%d", ana_gain, dig_gain, fine_gain);
	return 0;
}

unsigned char analog_Gain_Reg[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f};

static int setSensorGain(struct v4l2_subdev *sd, int gain)
{
	int ana_gain = gain / GAIN_STEP_BASE;
	int dig_Gain;
	int gain_flag = 1;

	if (ana_gain >= 16) {
		gain_flag = 4;
	} else if (ana_gain >= 8) {
		gain_flag = 3;
	} else if (ana_gain >= 4) {
		gain_flag = 2;
	} else if (ana_gain >= 2) {
		gain_flag = 1;
	} else {
		gain_flag = 0;
	}

	sensor_write(sd, SC202CS_AGAIN_ADDR, analog_Gain_Reg[gain_flag]);
	dig_Gain = gain >> gain_flag;
	if (dig_Gain < 2 * GAIN_STEP_BASE) {
		//step1/128
		sensor_write(sd, SC202CS_DGAIN_ADDR, 0x00);
		sensor_write(sd, SC202CS_DGAIN_FINE_ADDR, dig_Gain - GAIN_STEP_BASE + 0x80);
	} else if (dig_Gain < 4 * GAIN_STEP_BASE) {
		//step1/64
		sensor_write(sd, SC202CS_DGAIN_ADDR, 0x01);
		sensor_write(sd, SC202CS_DGAIN_FINE_ADDR, (dig_Gain - GAIN_STEP_BASE * 2) / 2 + 0x80);
	} else {
		sensor_write(sd, SC202CS_DGAIN_ADDR, 0x01);
		sensor_write(sd, SC202CS_DGAIN_FINE_ADDR, 0xfc);
	}
    return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	int tem_gain_val;

	//gain min step is 1/128gain
	tem_gain_val = gain_val * GAIN_STEP_BASE;
	if ((tem_gain_val - tem_gain_val / 16 * 16) > 0) {
		tem_gain_val = tem_gain_val / 16 + 1;
	} else {
		tem_gain_val = tem_gain_val / 16;
	}

	sensor_dbg("%s(), L:%d, gain_val:%d, tem_gain_val:%d\n",
		__func__, __LINE__, gain_val, tem_gain_val);

	setSensorGain(sd, tem_gain_val);
	info->gain = gain_val;
	return 0;
}

static int SC202CS_sensor_vts;
static int sensor_s_exp_gain(struct v4l2_subdev *sd, struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val, isp_id;
	struct sensor_info *info = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct sensor_switch_cfg_t *sensor_switch_config;
	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;
	if (exp_val > info->current_wins->vts << 4) {
		exp_val = info->current_wins->vts << 4;
	}

#if MIPI_SWITCH_EN
	sensor_switch_config = sensor_mipi_switch_get_cfg(sd);
	isp_id = exp_gain->r_gain >> 16; //get isp id
	if (isp_id  ==  1) {
		sensor_switch_config->exp_val_save[SWITCH_SENSOR_A] = exp_val;
		sensor_switch_config->gain_val_save[SWITCH_SENSOR_A] = gain_val;
		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_A] = true;
	} else if (isp_id  ==  2) {
		sensor_switch_config->exp_val_save[SWITCH_SENSOR_B] = exp_val;
		sensor_switch_config->gain_val_save[SWITCH_SENSOR_B] = gain_val;
		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_B] = true;
	} else {
		sensor_err("get isp id is %d\n", exp_gain->r_gain >> 16);
		return -1;
	}
#else
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
#endif
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

	sensor_read(sd, 0x3221, &get_value);
	sensor_print("ready to vflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x60;
		sensor_flip_status |= 0x60;
	} else {
		set_value = get_value & 0x9F;
		sensor_flip_status &= 0x9F;
	}
	sensor_write(sd, 0x3221, set_value);
	usleep_range(8000, 100000);
	sensor_read(sd, 0x3221, &get_value);
	sensor_print("after vflip, regs_data = 0x%x, sensor_flip_status = %d\n", get_value, sensor_flip_status);

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3221, &get_value);
	sensor_print("ready to hflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x06;
		sensor_flip_status |= 0x06;
	} else {
		set_value = get_value & 0xF9;
		sensor_flip_status &= 0xF9;
	}
	sensor_write(sd, 0x3221, set_value);
	usleep_range(80000, 100000);
	sensor_read(sd, 0x3221, &get_value);
	sensor_print("after hflip, regs_data = 0x%x, sensor_flip_status = %d\n", get_value, sensor_flip_status);

	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	*code = MEDIA_BUS_FMT_SBGGR10_1X10;
	return 0;
}

static int sensor_power(struct v4l2_subdev *sd, int on)
{
	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		vin_set_mclk_freq(sd, MCLK);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		usleep_range(10000, 12000);
		break;
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(1000, 1200);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);

		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(1000, 1200);
		printk("==========================sensor reset =====================================\n");
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!do nothing\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);
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
	switch (val) {
	case 0:
		printk("==========================sensor_reset high =====================================\n");
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(10000, 12000);
		break;
	case 1:
		printk("==========================sensor_reset low =====================================\n");
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
		printk("<<<<<ID_VAL_HIGH:0x%x,times_out:%d\n", rdval, times_out);
		usleep_range(200, 220);
		times_out--;
	} while (eRet < 0 && times_out > 0);

	sensor_read(sd, ID_REG_HIGH, &rdval);
	printk("<<<<< ID_VAL_HIGH = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;

	sensor_read(sd, ID_REG_LOW, &rdval);
	printk("<<<<< ID_VAL_LOW = %2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	printk("<<<<< Done!\n");
#endif
	return 0;
}

#if MIPI_SWITCH_EN

static void sensor_switch_change(struct v4l2_subdev *sd)
{
	unsigned int max_frame_length;
	struct sensor_switch_cfg_t *sensor_switch_config;

	sensor_switch_config = sensor_mipi_switch_get_cfg(sd);

	if (GPIO_SWITCH_SENSOR_A_STATUS == sensor_switch_config->switch_status) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
		// sensor_write(sd, 0x0100, 0x01); //sleep disable
		sensor_write(sd, 0x3650, 0x33); //driver max
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
		// sensor_write(sd, 0x0100, 0x00); //sleep enable
		sensor_write(sd, 0x3650, 0x31); //driver min
		sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_B_STATUS;
	} else if (GPIO_SWITCH_SENSOR_B_STATUS == sensor_switch_config->switch_status) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
		// sensor_write(sd, 0x0100, 0x01); //sleep disable
		sensor_write(sd, 0x3650, 0x33); //driver max
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
		// sensor_write(sd, 0x0100, 0x00); //sleep enable
		sensor_write(sd, 0x3650, 0x31); //driver min
		sensor_switch_config->switch_status = GPIO_SWITCH_SENSOR_A_STATUS;
	}

	if (sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_A]) {
		// sensor_print("[exp_gain] A = (%d, %d), B = (%d, %d)\n", sensor_switch_config->exp_val_save[SWITCH_SENSOR_A], sensor_switch_config->gain_val_save[SWITCH_SENSOR_A], sensor_switch_config->exp_val_save[SWITCH_SENSOR_B], sensor_switch_config->gain_val_save[SWITCH_SENSOR_B]);
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
		sensor_s_exp(sd, sensor_switch_config->exp_val_save[SWITCH_SENSOR_A]);
		sensor_s_gain(sd, sensor_switch_config->gain_val_save[SWITCH_SENSOR_A]);
		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_A] = false;
	}
	if (sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_B]) {
		// sensor_print("[exp_gain] A = (%d, %d), B = (%d, %d)\n", sensor_switch_config->exp_val_save[SWITCH_SENSOR_A], sensor_switch_config->gain_val_save[SWITCH_SENSOR_A], sensor_switch_config->exp_val_save[SWITCH_SENSOR_B], sensor_switch_config->gain_val_save[SWITCH_SENSOR_B]);
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_B]);
		sensor_s_exp(sd, sensor_switch_config->exp_val_save[SWITCH_SENSOR_B]);
		sensor_s_gain(sd, sensor_switch_config->gain_val_save[SWITCH_SENSOR_B]);
		sensor_switch_config->set_expgain_flags[SWITCH_SENSOR_B] = false;
	}

}

static void __s_sensor_switch_change_handle(struct work_struct *work)
{
	struct sensor_vysnc_config *sensor_vysnc_cfg = container_of(work, struct sensor_vysnc_config, s_sensor_switch_change_task);
	struct sensor_info *info = container_of(sensor_vysnc_cfg, struct sensor_info, sensor_vysnc_cfg);
	struct v4l2_subdev *sd = &info->sd;

	if (!sd || !sd->entity.use_count)
		return;

	sensor_switch_change(sd);

}

//vsync中断函数，每次中断后切换switch状态
static irqreturn_t sensor_vsync_irq_func(int irq, void *priv)
{
	struct v4l2_subdev *sd = priv;
	struct sensor_info *info = to_state(sd);
	struct sensor_vysnc_config *sensor_vysnc_cfg = &info->sensor_vysnc_cfg;
	struct sensor_switch_cfg_t *sensor_switch_config;
	unsigned long flags;

	sensor_switch_config = sensor_mipi_switch_get_cfg(sd);
	spin_lock_irqsave(&info->slock, flags);

	sensor_switch_config->vsync_cnt++;
	if (sensor_switch_config->vsync_cnt < 5) {
		spin_unlock_irqrestore(&info->slock, flags);
		return IRQ_HANDLED;
	}
/*
	if (GPIO_SWITCH_SENSOR_A_STATUS == sensor_switch_config->switch_status) {
		// sensor_print("========ywj_dg_add1 do irq func,switch change to A!!!\n");
		control_gpio(GPIO_SWITCH_SENSOR_A_STATUS);
	}
	else if(GPIO_SWITCH_SENSOR_B_STATUS == sensor_switch_config->switch_status) {
		// sensor_print("========ywj_dg_add1 do irq func,switch change to B!!!\n");
		control_gpio(GPIO_SWITCH_SENSOR_B_STATUS);
	}
*/

	schedule_work(&sensor_vysnc_cfg->s_sensor_switch_change_task);
	spin_unlock_irqrestore(&info->slock, flags);

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
	char *vsync_gpio_name = "vsync_gpio";

	if (!strcmp(sd->name, SENSOR_NAME)) {
		np = of_find_node_by_name(NULL, node_name0);
		if (np == NULL) {
			sensor_err("SC202CS_mipi can not find the %s node\n", node_name0);
			return -EINVAL;
		} else
			sensor_print("find the %s node\n", node_name0);
	}
	sensor_vysnc_cfg->gpio = of_get_named_gpio_flags(np, vsync_gpio_name, 0, &gc);
	sensor_print("get form %s gpio is %d\n", vsync_gpio_name, sensor_vysnc_cfg->gpio);
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
				IRQF_TRIGGER_FALLING,//IRQF_TRIGGER_FALLING, //| IRQF_TRIGGER_RISING,
				node_name0, sd);
		sensor_print("======== request irq [%d]==============\n", ret);
		disable_irq(sensor_vysnc_cfg->gpio_irq);
	}
	INIT_WORK(&sensor_vysnc_cfg->s_sensor_switch_change_task, __s_sensor_switch_change_handle);
	return 0;
}

static int sensor_init_with_switch(struct v4l2_subdev *sd)
{
	struct sensor_info *info = to_state(sd);
	unsigned int switch_choice = 0;
	unsigned int sensor_num = 0;
	int ret;
	struct sensor_switch_cfg_t *sensor_switch_config;

	sensor_switch_config = sensor_mipi_switch_get_cfg(sd);
	if (!sensor_switch_config) {
		sensor_err("sensor_switch_config is NULL\n");
		return -1;
	}
	for (switch_choice = SWITCH_SENSOR_A; switch_choice < SWITCH_SENSOR_MAX; switch_choice++) {
		sensor_print("sensor_detect 0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[switch_choice]);
		/*Make sure it is a target sensor */
		ret = sensor_detect(sd);
		if (ret) {
			sensor_err("chip found is not an target chip, iic_addr = 0x%x\n",
					sensor_switch_config->i2c_addr_container[switch_choice]);
			sensor_switch_config->sensor_detect_flag[switch_choice] = false;
		} else {
			sensor_print("[0:A, 1:B] detect sensor%d i2c_addr = 0x%x\n", switch_choice, sensor_switch_config->i2c_addr_container[switch_choice]);
			sensor_switch_config->sensor_detect_flag[switch_choice] = true;
			sensor_num++;
		}
	}
	sensor_print("detect sensor num : %d\n", sensor_num);
	if (sensor_num == 0) {
		sensor_switch_config->sensor_is_detected = false;
		return -ENODEV;
	} else if (sensor_num >= 1 && sensor_switch_config->sensor_detect_flag[SWITCH_SENSOR_A]) {
		sensor_switch_config->sensor_with_switch_en = true;
		sensor_print("maybe with mipi switch, so sensor_with_switch_en = %d\n", sensor_switch_config->sensor_with_switch_en);
	} else {
		sensor_switch_config->sensor_with_switch_en = false;
		sensor_print("without mipi switch, so sensor_with_switch_en = %d\n", sensor_switch_config->sensor_with_switch_en);
	}
	sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[SWITCH_SENSOR_A]);
	sensor_switch_config->sensor_is_detected = true;
	if (sensor_num > SWITCH_SENSOR_MAX) {
		sensor_switch_config->sensor_num = SWITCH_SENSOR_MAX;
		sensor_err("sensor_num is %d, which is greater than SWITCH_SENSOR_MAX, please check the config\n", sensor_num);
	} else {
		sensor_switch_config->sensor_num = sensor_num;
	}
	sensor_print("sensor_switch_config->sensor_num = %d\n", sensor_switch_config->sensor_num);

	info->focus_status = 0;
	info->low_speed    = 0;
	info->width	   = 1600;
	info->height   = 1200;
	info->hflip	   = 0;
	info->vflip	   = 0;
	info->gain	   = 0;
	info->exp	   = 0;
	info->tpf.numerator	 = 1;
	info->tpf.denominator	 = 30;

	return 0;
}
#endif

static int set_switch_change;  //0 :auto, 1：sensorA, 2:sennsorB

static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);
	printk("<<<<< sensor_init\n");

#if MIPI_SWITCH_EN
	struct sensor_switch_cfg_t *sensor_switch_config;
	sensor_switch_config = sensor_mipi_switch_get_cfg(sd);
	if (!sensor_switch_config->sensor_is_detected) {
		ret = sensor_init_with_switch(sd);
		if (ret) {
			sensor_err("chip found is not an target chip.\n");
			return ret;
		}
	} else {
		sensor_print("sensor is detected and will not detect repeatedly\n");
	}

	set_switch_change = 0;

	if (set_switch_change == 1) {
		control_gpio(GPIO_SWITCH_SENSOR_A_STATUS);
	} else if (set_switch_change == 2) {
		control_gpio(GPIO_SWITCH_SENSOR_B_STATUS);
	} else {
		control_gpio(GPIO_SWITCH_SENSOR_A_STATUS);
	}
#else
	ret = sensor_detect(sd);
	if (ret) {
		printk("<<<<< chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 1600;
	info->height = 1200;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 30;
#endif
	return 0;
}

static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

	switch (cmd) {
	case SET_SWITCH_STATUS:
		set_switch_change = *((int *)arg);
		if (1 == set_switch_change) {
			// sensor_print("SET_SWITCH_STATUS: %d\n", set_switch_change);
			control_gpio(GPIO_SWITCH_SENSOR_A_STATUS);
		} else if (2 == set_switch_change) {
			// sensor_print("SET_SWITCH_STATUS: %d\n", set_switch_change);
			control_gpio(GPIO_SWITCH_SENSOR_B_STATUS);
		}
	case SET_SENSOR_FRAME_DONE_AVOID_SLEEP:
		if (set_switch_change == 0) {
			ret = *((int *)arg);
			//sensor_print("framedone name=%s ret = %d\n",sd->name,ret);
			if (ret == 1) {
				// sensor_print("tdm rx1 frame done!\n");
				control_gpio(GPIO_SWITCH_SENSOR_B_STATUS);
			}
			if (ret == 2) {
				// sensor_print("tdm rx2 frame done!\n");
				control_gpio(GPIO_SWITCH_SENSOR_A_STATUS);
			}
		}
		break;
	case GET_CURRENT_WIN_CFG:
		if (info->current_wins != NULL) {
			memcpy(arg, info->current_wins, sizeof(struct sensor_win_size));
			ret = 0;
		} else {
			sensor_err("empyt wins!\n");
			ret = -1;
		}
		break;
	case SET_FPS:
		break;
	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
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

static struct sensor_win_size sensor_win_sizes[] = {
#ifdef SC202CS_800X600_30FPS
	{
		.width = 800,
		.height = 600,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1920,
		.vts = 1250,
		.pclk = 72000000,
		.mipi_bps = 297 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = 1125 << 4,
		.gain_min = 1 << 4,
		.gain_max = 1370,
		.regs = sensor_800x600_30_regs,
		.regs_size = ARRAY_SIZE(sensor_800x600_30_regs),
		.set_size = NULL,
	},
#endif

#ifdef SC202CS_1600X1200_15FPS
	{
		.width = 1600,
		.height = 1200,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1900,
		.vts = 2586,
		.pclk = 71250000,
		.mipi_bps = 712.5 * 1000 * 1000,
		.fps_fixed = 15,
		.bin_factor = 1,
		.intg_min = 1 << 4,
		.intg_max = 2586 << 4,
		.gain_min = 1 << 4,
		.gain_max = 16 << 4,
		.regs = sensor_1600x1200_15_regs,
		.regs_size = ARRAY_SIZE(sensor_1600x1200_15_regs),
		.set_size = NULL,
	},
#endif
};

#define N_WIN_SIZES ARRAY_SIZE(sensor_win_sizes)

static int sensor_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
#if MIPI_SWITCH_EN
	cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
#else
	cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
#endif
	return 0;
}

static int sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct sensor_info *info = container_of(ctrl->handler, struct sensor_info, handler);
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
	struct sensor_info *info = container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

	switch (ctrl->id) {
	case V4L2_CID_GAIN:
		return sensor_s_gain(sd, ctrl->val);
		break;
	case V4L2_CID_EXPOSURE:
		return sensor_s_exp(sd, ctrl->val);
		break;
	case V4L2_CID_HFLIP:
		return  sensor_s_hflip(sd, ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		return sensor_s_vflip(sd, ctrl->val);
		break;
	}
	return -EINVAL;
}

static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd  = &info->sd;
	struct sensor_win_size *wsize = info->current_wins;

	int switch_choice;
	struct sensor_switch_cfg_t *sensor_switch_config;
	sensor_switch_config = sensor_mipi_switch_get_cfg(sd);
	sensor_switch_config->vsync_cnt = 0;

#if defined CONFIG_VIN_INIT_MELIS

	if (info->preview_first_flag) {
		info->preview_first_flag = 0;
	} else {
		for (switch_choice = SWITCH_SENSOR_A; switch_choice < 2; switch_choice++) {
			sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[switch_choice]);
			ret = sensor_write_array(sd, sensor_default_regs,
						ARRAY_SIZE(sensor_default_regs));
			if (ret < 0) {
				sensor_err("write sensor_default_regs error\n");
				return ret;
			}

			if (wsize->regs) {
				sensor_print("ready to write regs for i2c_addr:0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
				sensor_write_array(sd, wsize->regs, wsize->regs_size);
			}

			if (switch_choice == SWITCH_SENSOR_A) {
				sensor_print("ready to write master regs for i2c_addr:0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
				ret = sensor_write_array(sd, sensor_master_regs, ARRAY_SIZE(sensor_master_regs));
			} else {
				sensor_print("ready to write slave regs for i2c_addr:0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
				ret = sensor_write_array(sd, sensor_slave_regs, ARRAY_SIZE(sensor_slave_regs));
			}
			sensor_switch_config->frame_length_save[switch_choice] = wsize->vts;
			sensor_switch_config->exp_val_save[switch_choice] = 16;
			sensor_switch_config->gain_val_save[switch_choice] = 16;
		}
	}
#else
		for (switch_choice = SWITCH_SENSOR_A; switch_choice < 2; switch_choice++) {

			sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[switch_choice]);
			ret = sensor_write_array(sd, sensor_default_regs,
						ARRAY_SIZE(sensor_default_regs));
			if (ret < 0) {
				sensor_err("write sensor_default_regs error\n");
				return ret;
			}
			if (wsize->regs) {
				sensor_print("ready to write regs for i2c_addr:0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
				sensor_write_array(sd, wsize->regs, wsize->regs_size);
			}
			if (switch_choice == SWITCH_SENSOR_A) {
				sensor_print("ready to write master regs for i2c_addr:0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
				ret = sensor_write_array(sd, sensor_master_regs, ARRAY_SIZE(sensor_master_regs));
			} else {
				sensor_print("ready to write slave regs for i2c_addr:0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
				ret = sensor_write_array(sd, sensor_slave_regs, ARRAY_SIZE(sensor_slave_regs));
			}
			sensor_switch_config->frame_length_save[switch_choice] = wsize->vts;
			sensor_switch_config->exp_val_save[switch_choice] = 16;
			sensor_switch_config->gain_val_save[switch_choice] = 16;
		}

#endif
	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	sensor_flip_status = 0x0;
	SC202CS_sensor_vts = wsize->vts;
	sensor_print("SC202CS_sensor_vts = %d\n", SC202CS_sensor_vts);
	sensor_print("s_fmt set width = %d, height = %d\n", wsize->width, wsize->height);

	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
int ret;
#if MIPI_SWITCH_EN
	unsigned int switch_choice;
	struct sensor_switch_cfg_t *sensor_switch_config;
	int use_count;
	data_type rdval;

	sensor_switch_config = sensor_mipi_switch_get_cfg(sd);
	use_count = sd->entity.use_count;
	if (on_off && use_count)
		return 0;
	else if (!on_off && --use_count > 0)
		return 0;

	ret = sensor_read(sd, 0x0100, &rdval);

	if (ret != 0)
		return ret;
	for (switch_choice = SWITCH_SENSOR_A; switch_choice < sensor_switch_config->sensor_num; switch_choice++) {
		sensor_i2c_addr_set(sd, sensor_switch_config->i2c_addr_container[switch_choice]);
		if (on_off == STBY_ON)
			ret = sensor_write(sd, 0x0100, 0x01);
		else
			ret = sensor_write(sd, 0x0100, 0x00);
	}
#endif
	return ret;
}


static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_vysnc_config *sensor_vysnc_cfg = &info->sensor_vysnc_cfg;
	struct sensor_switch_cfg_t *sensor_switch_config;
	int ret;
	sensor_print("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable, info->current_wins->width, info->current_wins->height, info->current_wins->fps_fixed, info->fmt->mbus_code);

	if (!enable) {
		ret = sensor_s_sw_stby(sd, 0);
		if (ret < 0)
			sensor_err("soft stby off falied!\n");
		if (sensor_vysnc_cfg->gpio_irq > 0)
			disable_irq(sensor_vysnc_cfg->gpio_irq);
		cancel_work_sync(&sensor_vysnc_cfg->s_sensor_switch_change_task);
		return 0;
	}
	sensor_switch_config = sensor_mipi_switch_get_cfg(sd);
	sensor_reg_init(info);
	if (sensor_vysnc_cfg->gpio_irq > 0) {
		sensor_print("========enable irq func,gio is [%d]==============", sensor_vysnc_cfg->gpio_irq);
		enable_irq(sensor_vysnc_cfg->gpio_irq);
	}
	return 0;
}

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

static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	},
	{
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_16,
		.data_width = CCI_BITS_8,
	},
};

static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;
	struct v4l2_ctrl *ctrl;
	int ret = 0;

	v4l2_ctrl_handler_init(handler, 4);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600, 256 * 1600, 1, 1 * 1600);

	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1, 65536 * 16, 1, 1);

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
	int i = 0;
	printk("=========== client name:%s, slave:0x%x\n",  client->name, client->addr);
	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
		 printk("===========num:%d cci_driver name:%s, client name:%s, slave:0x%x\n", i, cci_drv[i].name, client->name, client->addr);
			if (!strcmp(cci_drv[i].name, client->name))
				break;
		}
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[i]);
	} else {
		cci_dev_probe_helper(sd, client, &sensor_ops, &cci_drv[sensor_dev_id++]);
	}

	sensor_init_controls(sd, &sensor_ctrl_ops);

	mutex_init(&info->lock);

#if MIPI_SWITCH_EN
	sensor_i2c_switch_lock_init();
	init_gpio();
	sensor_vysnc_init(sd);
#endif

	info->fmt = &sensor_formats[0];
	info->fmt_pt = &sensor_formats[0];
	info->win_pt = &sensor_win_sizes[0];
	info->fmt_num = N_FMTS;
	info->win_size_num = N_WIN_SIZES;
	info->sensor_field = V4L2_FIELD_NONE;
	//use CMB_PHYA_OFFSET2 also ok
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;
	//info->combo_mode = CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->time_hs = 0x11;
	info->exp = 0;
	info->gain = 0;
	info->preview_first_flag = 1;
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
	},
	{
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

