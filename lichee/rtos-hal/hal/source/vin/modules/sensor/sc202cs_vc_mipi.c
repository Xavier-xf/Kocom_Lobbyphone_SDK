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

#define MCLK            (24*1000*1000)
#define V4L2_IDENT_SENSOR   0xeb52

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
#define ID_VAL_LOW  0x00

#define I2C_ADDR 0x6C

#define SENSOR_NUM 0x2
#define SENSOR_NAME "sc202cs_mipi"
#define SENSOR_NAME_2 "sc202cs_mipi_2"

#if 1
#define SC2356_1600X1200_15FPS
#else
#define SC2356_800X600_30FPS
#endif

static int sensor_power_count[2];
static int sensor_stream_count[2];
static struct sensor_format_struct *current_win[2];

/*
 * The default register settings
 */
#define A_I2C_ADDR 0x6c
#define B_I2C_ADDR 0x20

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

static struct regval_list sensor_default_regs[] = {

};

#ifdef SC2356_800X600_30FPS
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
	{0x3e01, 0x4d},
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

#ifdef SC2356_1600X1200_15FPS
static struct regval_list sensor_1600x1200_30_regs[] = {

};
#endif
static struct regval_list sensor_master_regs[] = {

	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36e9,0x24},
	{0x301f,0x02},
	{0x300a,0x40},
	{0x3032,0xa0},
	{0x3301,0xff},
	{0x3304,0x68},
	{0x3306,0x40},
	{0x3308,0x08},
	{0x3309,0xa8},
	{0x330b,0xb0},
	{0x330c,0x18},
	{0x330d,0xff},
	{0x330e,0x20},
	{0x331e,0x59},
	{0x331f,0x99},
	{0x3333,0x10},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x1f},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x338f,0xa0},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x1f},
	{0x3393,0xff},
	{0x3394,0xff},
	{0x3395,0xff},
	{0x33a2,0x04},
	{0x33ad,0x0c},
	{0x33b1,0x20},
	{0x33b3,0x38},
	{0x33f9,0x40},
	{0x33fb,0x48},
	{0x33fc,0x0f},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x03},
	{0x34a7,0x1f},
	{0x34a8,0x38},
	{0x34a9,0x30},
	{0x34ab,0xb0},
	{0x34ad,0xb0},
	{0x34f8,0x1f},
	{0x34f9,0x20},
	{0x3630,0xa0},
	{0x3631,0x92},
	{0x3632,0x64},
	{0x3633,0x43},
	{0x3637,0x49},
	{0x363a,0x85},
	{0x363c,0x0f},
	{0x3650,0x31},
	{0x3670,0x0d},
	{0x3674,0xc0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x92},
	{0x3678,0x96},
	{0x3679,0x9a},
	{0x367c,0x03},
	{0x367d,0x0f},
	{0x367e,0x01},
	{0x367f,0x0f},
	{0x3698,0x83},
	{0x3699,0x86},
	{0x369a,0x8c},
	{0x369b,0x94},
	{0x36a2,0x01},
	{0x36a3,0x03},
	{0x36a4,0x07},
	{0x36ae,0x0f},
	{0x36af,0x1f},
	{0x36bd,0x22},
	{0x36be,0x22},
	{0x36bf,0x22},
	{0x36d0,0x01},
	{0x370f,0x02},
	{0x3721,0x6c},
	{0x3722,0x8d},
	{0x3725,0xc5},
	{0x3727,0x14},
	{0x3728,0x04},
	{0x37b7,0x04},
	{0x37b8,0x04},
	{0x37b9,0x06},
	{0x37bd,0x07},
	{0x37be,0x0f},
	{0x3901,0x02},
	{0x3903,0x40},
	{0x3905,0x8d},
	{0x3907,0x00},
	{0x3908,0x41},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0x02},
	{0x3937,0x6f},
	{0x393a,0x01},
	{0x393d,0x01},
	{0x393e,0xc0},
	{0x39dd,0x41},
	{0x3e00,0x00},
	{0x3e01,0x4d},
	{0x3e02,0xc0},
	{0x3e09,0x00},

	{0x320e,0x0a},
	{0x320f,0x1a},
	{0x4816,0x61},//vc ch0

	{0x4509,0x28},
	{0x450d,0x61},
	{0x0100,0x01},

};

static struct regval_list sensor_slave_regs[] = {

	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x36e9,0x24},
	{0x301f,0x03},
	{0x3222,0x02},
	{0x3301,0xff},
	{0x3304,0x68},
	{0x3306,0x40},
	{0x3308,0x08},
	{0x3309,0xa8},
	{0x330b,0xb0},
	{0x330c,0x18},
	{0x330d,0xff},
	{0x330e,0x20},
	{0x331e,0x59},
	{0x331f,0x99},
	{0x3333,0x10},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x1f},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x338f,0xa0},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x1f},
	{0x3393,0xff},
	{0x3394,0xff},
	{0x3395,0xff},
	{0x33a2,0x04},
	{0x33ad,0x0c},
	{0x33b1,0x20},
	{0x33b3,0x38},
	{0x33f9,0x40},
	{0x33fb,0x48},
	{0x33fc,0x0f},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x03},
	{0x34a7,0x1f},
	{0x34a8,0x38},
	{0x34a9,0x30},
	{0x34ab,0xb0},
	{0x34ad,0xb0},
	{0x34f8,0x1f},
	{0x34f9,0x20},
	{0x3630,0xa0},
	{0x3631,0x92},
	{0x3632,0x64},
	{0x3633,0x43},
	{0x3637,0x49},
	{0x363a,0x85},
	{0x363c,0x0f},
	{0x3650,0x31},
	{0x3670,0x0d},
	{0x3674,0xc0},
	{0x3675,0xa0},
	{0x3676,0xa0},
	{0x3677,0x92},
	{0x3678,0x96},
	{0x3679,0x9a},
	{0x367c,0x03},
	{0x367d,0x0f},
	{0x367e,0x01},
	{0x367f,0x0f},
	{0x3698,0x83},
	{0x3699,0x86},
	{0x369a,0x8c},
	{0x369b,0x94},
	{0x36a2,0x01},
	{0x36a3,0x03},
	{0x36a4,0x07},
	{0x36ae,0x0f},
	{0x36af,0x1f},
	{0x36bd,0x22},
	{0x36be,0x22},
	{0x36bf,0x22},
	{0x36d0,0x01},
	{0x370f,0x02},
	{0x3721,0x6c},
	{0x3722,0x8d},
	{0x3725,0xc5},
	{0x3727,0x14},
	{0x3728,0x04},
	{0x37b7,0x04},
	{0x37b8,0x04},
	{0x37b9,0x06},
	{0x37bd,0x07},
	{0x37be,0x0f},
	{0x3901,0x02},
	{0x3903,0x40},
	{0x3905,0x8d},
	{0x3907,0x00},
	{0x3908,0x41},
	{0x391f,0x41},
	{0x3933,0x80},
	{0x3934,0x02},
	{0x3937,0x6f},
	{0x393a,0x01},
	{0x393d,0x01},
	{0x393e,0xc0},
	{0x39dd,0x41},
	{0x3e00,0x00},
	{0x3e01,0x4d},
	{0x3e02,0xc0},
	{0x3e09,0x00},
	
	{0x320e,0x0a},
	{0x320f,0x1a},
	
	{0x3224,0x83},
	{0x3230,0x00},
	{0x3231,0x3a},
	
	{0x322e,0x09},//vts - RB rowa
	{0x322f,0xe0},
	
	{0x4509,0x28},
	{0x450d,0x61},

	{0x4816,0x65},//vc ch1

	{0x0100,0x01},

};


/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct sensor_switch_cfg_t* sensor_mipi_switch_get_cfg(int id)
{
	return (&sensor_switch_cfg[0]);

}

static struct regval_list sensor_fmt_raw[] = {

};

static int sensor_s_exp(int id, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;

	exphigh = (unsigned char) (0x0f & (exp_val >> 16));
	expmid = (unsigned char) (0xff & (exp_val >> 8));
	explow = (unsigned char) (0xf0 & (exp_val << 0));

	sensor_write(id, SC202CS_EXP_L_ADDR, explow);
	sensor_write(id, SC202CS_EXP_M_ADDR, expmid);
	sensor_write(id, SC202CS_EXP_H_ADDR, exphigh);

	return 0;
}


#define GPIO_SWITCH_NUM 135 //PE7
static int init_gpio(void) {
   	hal_gpio_set_direction(GPIOE(7), GPIO_DIRECTION_OUTPUT);
    return 0;
}

static void control_gpio(int value) {
    hal_gpio_set_data(GPIOE(7), value);
}

unsigned char analog_Gain_Reg[] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f};

static int setSensorGain(int id, int gain)
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

	sensor_write(id, SC202CS_AGAIN_ADDR, analog_Gain_Reg[gain_flag]);
	dig_Gain = gain >> gain_flag;
	if (dig_Gain < 2 * GAIN_STEP_BASE) {
		//step1/128
		sensor_write(id, SC202CS_DGAIN_ADDR, 0x00);
		sensor_write(id, SC202CS_DGAIN_FINE_ADDR, dig_Gain - GAIN_STEP_BASE + 0x80);
	} else if (dig_Gain < 4 * GAIN_STEP_BASE) {
		//step1/64
		sensor_write(id, SC202CS_DGAIN_ADDR, 0x01);
		sensor_write(id, SC202CS_DGAIN_FINE_ADDR, (dig_Gain - GAIN_STEP_BASE * 2) / 2 + 0x80);
	} else {
		sensor_write(id, SC202CS_DGAIN_ADDR, 0x01);
		sensor_write(id, SC202CS_DGAIN_FINE_ADDR, 0xfc);
	}
    return 0;
}

static int sensor_s_gain(int id, int gain_val)
{
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

	setSensorGain(id, tem_gain_val);
	return 0;
}

static int sensor_s_exp_gain(int id, struct sensor_exp_gain *exp_gain)
{
	int exp_val,gain_val;
	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;
	if (exp_val > 2586 ){
		exp_val = 2586;
	}
	//sensor_print("sensor_s_exp_gain exp:%d gain:%d\n", exp_gain->gain_val, exp_gain->exp_val);
	sensor_s_exp(id, exp_val);
	sensor_s_gain(id, gain_val);

    return 0;
}

static int sensor_power(int id, int on)
{
	if (on && (sensor_power_count[id])++ > 0) {
		return 0;
	} else if (!on && (sensor_power_count[id] == 0 || --(sensor_power_count[id]) > 0)) {
		return 0;
	}
	switch(on){
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		vin_set_mclk(id, 0);
		hal_usleep(1000);
		vin_set_mclk_freq(id, MCLK);
		vin_set_mclk(id, 1);
		hal_usleep(1000);
		vin_gpio_set_status(id, PWDN, 1);
		vin_gpio_set_status(id, RESET, 1);

		vin_gpio_write(id, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		hal_usleep(1000);
		printk("==========================sensor reset =====================================\n");
		hal_usleep(10000);
		vin_gpio_write(id, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(id, RESET, CSI_GPIO_HIGH);
		hal_usleep(10000);
	    break;
	case PWR_OFF:
		sensor_print("PWR_OFF!do nothing\n");
		vin_set_mclk(id, 0);
		hal_usleep(10000);
		vin_gpio_set_status(id, PWDN, 1);
		vin_gpio_set_status(id, RESET, 1);
		vin_gpio_write(id, PWDN, CSI_GPIO_LOW);
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_detect(int id){
	data_type rdval;
	int eRet;
	int times_out = 3;

	do{
		eRet = sensor_read(id, ID_REG_HIGH, &rdval);
		sensor_print("eRet:%d, ID_VAL_HIGH:0x%x,times_out:%d\n", eRet, rdval, times_out);
		hal_usleep(200);
		times_out--;
	}while(eRet < 0 && times_out > 0);
	sensor_read(id, ID_REG_HIGH, &rdval);
	sensor_print("ID_VAL_HIGH = %2x, Done!\n", rdval);
	if(rdval != ID_VAL_HIGH)
		return -ENODEV;
	sensor_read(id, ID_REG_LOW, &rdval);
	sensor_print("ID_VAL_LOW = %2x, Done!\n", rdval);
	if(rdval != ID_VAL_LOW)
		return -ENODEV;

	return 0;
}

static int sensor_init(int id)
{
	int ret;

	sensor_detect("sensor_init\n");
	ret = sensor_detect(id);
	if(ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	init_gpio();
	control_gpio(1);

	return 0;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
#ifdef SC2356_800X600_30FPS
	{
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, /*.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
		.width      = 800,
		.height     = 600,//1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 1920,
		.vts        = 1250,
		.pclk       = 72000000,
		.mipi_bps   = 297 * 1000 * 1000,
		.fps_fixed  = 30,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = 1125 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 16 << 4,
		.offs_h     = 0,
		.offs_v     = 0,
		.regs	    = sensor_800x600_30_regs,
		.regs_size  = ARRAY_SIZE(sensor_800x600_30_regs),
	},
#endif

#ifdef SC2356_1600X1200_15FPS
	{
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, /*.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10, */
		.width      = 1600,
		.height     = 1200,//1080,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 1920,
		.vts        = 2586,
		.pclk       = 71250000,
		.mipi_bps   = 712.5 * 1000 * 1000,
		.fps_fixed  = 15,
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = 2586 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 16 << 4,
		.offs_h     = 0,
		.offs_v     = 0,
		.regs	    = sensor_1600x1200_30_regs,
		.regs_size  = ARRAY_SIZE(sensor_1600x1200_30_regs),
	},
#endif
};

static struct sensor_format_struct *sensor_get_format(int id, int isp_id)
{
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
}

static int sensor_g_mbus_config(int id, struct v4l2_mbus_config *cfg, struct mbus_framefmt_res *res)
{
	cfg->type  = V4L2_MBUS_CSI2;
	cfg->flags = 0 | V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0 | V4L2_MBUS_CSI2_CHANNEL_1;
	res->res_time_hs = 0x11;
	return 0;
}

static int sensor_reg_init_vc(int id, int isp_id)
{
	int ret;
	struct sensor_exp_gain exp_gain;
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);
	int switch_choice;
	struct sensor_switch_cfg_t *sensor_switch_config;
	sensor_switch_config = sensor_mipi_switch_get_cfg(id);
	sensor_switch_config->vsync_cnt = 0;

	for (switch_choice = SWITCH_SENSOR_A; switch_choice < 2; switch_choice++)
	{
		sensor_set_twi_addr(id,sensor_switch_config->i2c_addr_container[switch_choice]);
		ret = sensor_write_array(id, sensor_default_regs, ARRAY_SIZE(sensor_default_regs));
		if (ret < 0) {
			sensor_err("write sensor_default_regs error\n");
			return ret;
		}
		if (current_win[id]->regs)
			ret = sensor_write_array(id, current_win[id]->regs, current_win[id]->regs_size);
		if (ret < 0)
			return ret;

		if(switch_choice == SWITCH_SENSOR_A){
			sensor_print("ready to write master regs for i2c_addr:0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
			ret = sensor_write_array(id, sensor_master_regs, ARRAY_SIZE(sensor_master_regs));
		}
		else
		{
			sensor_print("ready to write slave regs for i2c_addr:0x%x\n", sensor_switch_config->i2c_addr_container[switch_choice]);
			ret = sensor_write_array(id, sensor_slave_regs, ARRAY_SIZE(sensor_slave_regs));
		}
	}
sensor_set_twi_addr(id,sensor_switch_config->i2c_addr_container[0]);
#ifndef CONFIG_ISP_READ_THRESHOLD
	exp_gain.exp_val = 1088;
	exp_gain.gain_val = 16;
#else
	if (ispid == 0) {
		exp_gain.exp_val = clamp(*((unsigned int *)ISP0_NORFLASH_SAVE + 2), 16, 1125 << 4);
		exp_gain.gain_val = clamp(*((unsigned int *)ISP0_NORFLASH_SAVE + 1), 16, 110 << 4);
		printk("isp0 read gain is %d,exp is %d \n",*((unsigned int *)ISP0_NORFLASH_SAVE + 1),*((unsigned int *)ISP0_NORFLASH_SAVE + 2));
	} else {
		exp_gain.exp_val = clamp(*((unsigned int *)ISP1_NORFLASH_SAVE + 2), 16, 1125 << 4);
		exp_gain.gain_val = clamp(*((unsigned int *)ISP1_NORFLASH_SAVE + 1), 16, 110 << 4);
		printk("isp1 read gain is %d,exp is %d \n",*((unsigned int *)ISP1_NORFLASH_SAVE + 1),*((unsigned int *)ISP1_NORFLASH_SAVE + 2));
	}
		printk("<<<exp = %d, gain = %d function = %s, line=%d \n file=%s \n", exp_gain.exp_val, exp_gain.gain_val,__func__,__LINE__,__FILE__);		
#endif
	sensor_s_exp_gain(id, &exp_gain);

	return 0;
}

static int sensor_reg_init(int id, int isp_id)
{
	sensor_dbg("step sensor_reg_init\n");
	int ret;
	struct sensor_exp_gain exp_gain;
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);

	sensor_print("sensor ready to write reg!\n");
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

#ifndef CONFIG_ISP_READ_THRESHOLD
	exp_gain.exp_val = 1088;
	exp_gain.gain_val = 16;
#else
	if (ispid == 1) {
		exp_gain.exp_val = clamp(*((unsigned int *)ISP1_NORFLASH_SAVE + 2), 16, 1125 << 4);
		exp_gain.gain_val = clamp(*((unsigned int *)ISP1_NORFLASH_SAVE + 1), 16, 110 << 4);
		printk("isp1 read gain is %d,exp is %d \n",*((unsigned int *)ISP1_NORFLASH_SAVE + 1),*((unsigned int *)ISP1_NORFLASH_SAVE + 2));
	} else {
		exp_gain.exp_val = clamp(*((unsigned int *)ISP2_NORFLASH_SAVE + 2), 16, 1125 << 4);
		exp_gain.gain_val = clamp(*((unsigned int *)ISP2_NORFLASH_SAVE + 1), 16, 110 << 4);
		printk("isp2 read gain is %d,exp is %d \n",*((unsigned int *)ISP2_NORFLASH_SAVE + 1),*((unsigned int *)ISP2_NORFLASH_SAVE + 2));
	}
		printk("<<<exp = %d, gain = %d function = %s, line=%d \n file=%s \n", exp_gain.exp_val, exp_gain.gain_val,__func__,__LINE__,__FILE__);		
#endif
	sensor_s_exp_gain(id, &exp_gain);

	return 0;
}

static int sensor_init_with_switch(int id)
{
	unsigned int switch_choice = 0;
	unsigned int sensor_num = 0;
	int ret;
	int iic_addr;
	struct sensor_switch_cfg_t *sensor_switch_config;

	sensor_switch_config = sensor_mipi_switch_get_cfg(id);
	if (!sensor_switch_config) {
		sensor_err("sensor_switch_config is NULL\n");
		return -1;
	}
	for (switch_choice = 0; switch_choice < 2; switch_choice++) {
		sensor_set_twi_addr(id,sensor_switch_config->i2c_addr_container[switch_choice]);
		sensor_print("Get i2c addr is 0x%x\n",sensor_switch_config->i2c_addr_container[switch_choice]);

		ret = sensor_detect(id);
		if (ret) {
			sensor_print("chip found is not an target chip, iic_addr = 0x%x\n",
					sensor_switch_config->i2c_addr_container[switch_choice]);
			sensor_switch_config->sensor_detect_flag[switch_choice] = false;
		} else {
			sensor_print("[0:A, 1:B] detect sensor%d i2c_addr = 0x%x\n", switch_choice, sensor_switch_config->i2c_addr_container[switch_choice]);
			sensor_switch_config->sensor_detect_flag[switch_choice] = true;
			sensor_num++;
		}
	}
	sensor_print("detect sensor num : %d\n", sensor_num);
	return sensor_num;
}


static int sensor_s_stream(int id, int isp_id, int enable)
{
	int sensor_num;
	sensor_dbg("step sensor_s_stream\n");
	if (enable && sensor_stream_count[id]++ > 0)
		return 0;
	else if (!enable && (sensor_stream_count[id] == 0 || --sensor_stream_count[id] > 0))
		return 0;

	if (!enable)
		return 0;

	sensor_num = sensor_init_with_switch(id);

	if (sensor_num == 2){
		return sensor_reg_init_vc(id, isp_id);
	}
	else{
		return sensor_reg_init(id, isp_id);
	}
}

static int sensor_test_i2c(int id)
{
	int ret;
	sensor_power(id, PWR_ON);
	ret = sensor_init(id);
	sensor_power(id, PWR_OFF);

	return ret;
}

struct sensor_fuc_core sc202cs_vc_core  = {
	.g_mbus_config = sensor_g_mbus_config,
	.sensor_test_i2c = sensor_test_i2c,
	.sensor_power = sensor_power,
	.s_stream = sensor_s_stream,
	.s_exp_gain = sensor_s_exp_gain,
	.sensor_g_format = sensor_get_format,
};
