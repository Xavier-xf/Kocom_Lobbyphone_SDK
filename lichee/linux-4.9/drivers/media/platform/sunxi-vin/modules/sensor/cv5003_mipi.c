/*
 * A V4L2 driver for Raw cameras.
 *
 * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
 *
 * Authors:  Gu Cheng <zhaowei@allwinnertech.com>
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

MODULE_AUTHOR("gc");
MODULE_DESCRIPTION("A low-level driver for CV5003 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24 * 1000 * 1000)
#define V4L2_IDENT_SENSOR  0x5002

//define the registers
#define EXP_HIGH		0xff
#define EXP_MID			0x03
#define EXP_LOW			0x04
#define GAIN_HIGH		0xff
#define GAIN_LOW		0x24
/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0x3003
#define ID_REG_LOW		0x3002
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)
#define SENSOR_FRAME_RATE 20

#define RAW12 	0 // raw12 or raw10 select

/*
 * The CV5003 i2c address
 */
#define I2C_ADDR 0x6a

#define SENSOR_NUM 0x2
#define SENSOR_NAME "cv5003_mipi"
#define SENSOR_NAME_2 "cv5003_mipi_2"

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

// CV5003_linear_2880_1620_15fps_AD12_2lane_840Mbps_VTS6928_virtualHTS405_PCLK420Mhz_MCLK24Mhz.cleaned
static struct regval_list sensor_2880_1620_15_regs[] = {
	{0x300D, 0x01},
	{0x300E, 0x00},
	{0x30A8, 0x04},
	{0x385A, 0x03},
	{0x3204, 0x40}, //BLK LEVEL 64
	{0x3478, 0x00},

	{0x3048, 0x08},

	{0x3109, 0x01}, //split gain

	{0x3134, 0x01},
	{0x326c, 0xdc},
	{0x326e, 0x24},
	{0x3154, 0x11},
	{0x31BC, 0x40},
	{0x31D5, 0x03},
	{0x31D6, 0x04},
	{0x3164, 0x09},

	{0x351F, 0x32},
	{0x3527, 0x05},
	{0x38AD, 0x10},

	{0x3638, 0x03},
	{0x3639, 0x00},

	{0x385A, 0x03},
	{0x359F, 0x13},

	{0x301C, 0x10},
	{0x301D, 0x1b},

	{0x3000, 0x00},
};

static struct regval_list sensor_2880_1620_30_regs[] = {
	{0x300D, 0x01},
	{0x300E, 0x00},
	{0x30A8, 0x04},
	{0x385A, 0x03},
	{0x3204, 0x40}, //BLK LEVEL 64
	{0x3478, 0x00},

	{0x3048, 0x08},

	{0x3109, 0x01}, //split gain

	{0x3134, 0x01},
	{0x326c, 0xdc},
	{0x326e, 0x24},
	{0x3154, 0x11},
	{0x31BC, 0x40},
	{0x31D5, 0x03},
	{0x31D6, 0x04},
	{0x3164, 0x09},

	{0x351F, 0x32},
	{0x3527, 0x05},
	{0x38AD, 0x10},

	{0x3638, 0x03},
	{0x3639, 0x00},

	{0x385A, 0x03},
	{0x359F, 0x13},

	{0x3000, 0x00},
};

// CV5001_linear_2880_1620_16p66667fps_AD12_2lane_792Mbps_VTS1650_virtualHTS1800_PCLK49P5Mhz_MCLK24Mhz.cleaned
static struct regval_list sensor_2880_1620_16p6667_regs[] = {
	{0x300D, 0x01},  // [0]     OSC_EXTCLK_SEL      = 1(dec:1)
	{0x300E, 0x00},  // [0]     AO_XSTB_DCO         = 0(dec:0)
	{0x30A8, 0x04},  // [3:0]   PRESET_SEL          = 1(dec:1)
	{0x3204, 0x40},//BLK LEVEL 64
	{0x385a, 0x0f},//VPI DEF07
	{0x359f, 0x15},// ADR fwc4000
	{0x3478, 0x00},//EBD OFF
	{0x310c, 0x5e},//模拟增益 28.2db
	//OB漏光策略
	{0x3134, 0x01},
	{0x326c, 0xdc},
	{0x326e, 0x24},
	//低功耗
	{0x3631, 0x01},
	{0x3632, 0x55},
	{0x3633, 0xF5},
	{0x3639, 0x01},
	{0x363A, 0x55},
	{0x363B, 0xF5},
	{0x3635, 0x00},
	{0x3634, 0x00},
	{0x380A, 0x02},
	{0x3109, 0x01}, //split gain
	{0x301C, 0x10},
	{0x301D, 0x1B},
	{0x301E, 0x00},
	//{0x3000,0x00},
};

static struct regval_list sensor_1440_810p_120fps_10bit_regs[] = {
	{0x300D, 0x01},  // [0]     OSC_EXTCLK_SEL      = 1(dec:1)
	{0x300E, 0x00},
	{0x30A8, 0x07},

	{0x3808, 0x46},
	{0x380A, 0x02},
	{0x301C, 0x08},  // [19:0]  FRAME_LENGTH
	{0x301D, 0x07},
	{0x301E, 0x00},
	{0x3020, 0x85},  // [15:0]  LINE_LENGTH
	{0x3021, 0x01},

	{0x3204, 0x40},//BLK LEVEL 64
	{0x3478, 0x00},

	//S810P setting
	{0x3034, 0x02},  // [10:0]  Y_DCROP_STA         = 002(dec:2)
	{0x3035, 0x00},
	{0x3036, 0x2A},  // [10:0]  Y_DCROP_WIDTH       = 32A(dec:810)
	{0x3037, 0x03},
	{0x3038, 0x02},  // [11:0]  X_CROP_STA          = 002(dec:2)
	{0x3039, 0x00},
	{0x303A, 0xA0},  // [11:0]  X_CROP_WIDTH        = 5A0(dec:1440)
	{0x303B, 0x05},
	//for V-normal
	{0x31BC, 0x0C},//MAN[2][3]
	{0x31C4, 0x20},//AR3_ST1
	{0x31C5, 0x00},//AR3_ST1
	{0x31CA, 0x24},//AR3_ST2
	{0x31CB, 0x00},//AR3_ST2
	{0x31C6, 0x5C},//AR3_WID=(2+810+2)x2
	{0x31C7, 0x06},//AR3_WID
	//for V-Filp
	//0x31C4, 0xD0,//AR3_ST1(1640x2=3280)
	//0x31C5, 0x0C,//AR3_ST1
	//0x31CA, 0xD4,//AR3_ST2(1642x2=3284)
	//0x31CB, 0x0C,//AR3_ST2
	{0x3109, 0x01}, //split gain
	{0x385A, 0x00},
	{0x38AD, 0x10},
	{0x3668, 0x02},
	{0x3639, 0x00},
	{0x351F, 0x32},
	{0x3527, 0x05},
};

// cv5001 2lane 10bit 810P120
// binning to 810P for fastAE mode
// static struct regval_list sensor_2lane10bit_810P120_fastAE_regs[] = {
// 	{0X3162,0X01},
// 	{0x3020,0x04},
// 	{0x3024,0x01},
// 	{0x3025,0x01},
// 	{0x3028,0x90},
// 	{0x3029,0x06},
// 	{0x302A,0x00},
// 	{0x302C,0xE1},
// 	{0x302D,0x02},
// 	{0x3030,0x00},
// 	{0x3035,0x00},
// 	{0x3036,0x00},
// 	{0x3040,0x01},
// 	{0x3044,0x00},
// 	{0x3045,0x00},
// 	{0x3046,0x2A},
// 	{0x3047,0x03},
// 	{0x3048,0x04},
// 	{0x3049,0x00},
// 	{0x304A,0x40},
// 	{0x304B,0x0B},
// 	{0x3054,0x00},
// 	{0x3055,0x00},
// 	{0x3056,0x30},
// 	{0x3057,0x03},
// 	{0x3304,0x01},
// 	{0x3305,0x02},

// 	{0x3401,0x01},
// 	{0x3418,0x97},
// 	{0x3419,0x00},
// 	{0x341C,0x47},
// 	{0x341D,0x00},
// 	{0x341E,0x4F},
// 	{0x341F,0x01},
// 	{0x3422,0x97},
// 	{0x3423,0x00},
// 	{0x3424,0x47},
// 	{0x3425,0x00},
// 	{0x3426,0x7F},
// 	{0x3427,0x00},
// 	{0x3428,0x3F},
// 	{0x3429,0x00},
// 	{0x3400,0x11},  // Discontinuous clock
// 	{0x343C,0x03},
// 	{0x343E,0x00},
// 	{0x3807,0x80},
// 	{0x3908,0x63},
// 	{0x3909,0x00},
// 	{0x3930,0x00},
// 	{0x32A3,0x00},
// 	{0x3348,0x00},
// 	{0x3000,0x00}
// };

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

static int sensor_g_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
	data_type frame_length = 0;
	unsigned int act_vts = 0;

	sensor_read(sd, 0x301E, &frame_length);
	act_vts = frame_length << 16;
	sensor_read(sd, 0x301D, &frame_length);
	act_vts |= frame_length << 8;
	sensor_read(sd, 0x301C, &frame_length);
	act_vts |= frame_length;

	fps->fps = wsize->pclk / (wsize->hts * act_vts);
	sensor_dbg("fps = %d\n", fps->fps);

	return 0;
}

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
	unsigned int frame_length = 0;
	data_type frame_length_tmp = 0;

	unsigned int shutter0 = 0;
	unsigned int shutter0_high = 0, shutter0_mid = 0, shutter0_low = 0;

	sensor_read(sd, 0x301E, &frame_length_tmp);
	frame_length = frame_length_tmp << 16;
	sensor_read(sd, 0x301D, &frame_length_tmp);
	frame_length |= frame_length_tmp << 8;
	sensor_read(sd, 0x301C, &frame_length_tmp);
	frame_length |= frame_length_tmp;

	shutter0 = frame_length - (exp_val >> 4) ;
	shutter0 >>= 1;
	shutter0 <<= 1;	// shutter0 = 2n
	if (shutter0 <= 4)
		shutter0 = 4;
	shutter0_low =  (unsigned char)((0x0000ff & shutter0));
	shutter0_mid =  (unsigned char)((0x00ff00 & shutter0) >> 8);
	shutter0_high = (unsigned char)((0x0f0000 & shutter0) >> 16);

	sensor_write(sd, 0x3048, shutter0_low);
	sensor_write(sd, 0x3049, shutter0_mid);
	sensor_write(sd, 0x304A, shutter0_high);
	sensor_dbg("frame_length = %d, sensor_set_exp = %d, shutter0 =  %d !\n", frame_length, exp_val, shutter0);
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

unsigned int Tab_SensorRegToGain[250] = {
	64, 64, 64, 64, 65,
	65, 65, 65, 66, 66,
	66, 66, 67, 67, 67,
	67, 68, 68, 68, 69,
	69, 69, 70, 70, 70,
	70, 71, 71, 71, 72,
	72, 72, 73, 73, 73,
	74, 74, 74, 75, 75,
	75, 76, 76, 76, 77,
	77, 78, 78, 78, 79,
	79, 79, 80, 80, 81,
	81, 81, 82, 82, 83,
	83, 84, 84, 84, 85,
	85, 86, 86, 87, 87,
	88, 88, 89, 89, 90,
	90, 91, 91, 92, 92,
	93, 93, 94, 94, 95,
	95, 96, 96, 97, 98,
	98, 99, 99, 100, 101,
	101, 102, 103, 103, 104,
	105, 105, 106, 107, 107,
	108, 109, 109, 110, 111,
	112, 113, 113, 114, 115,
	116, 117, 117, 118, 119,
	120, 121, 122, 123, 124,
	125, 126, 126, 128, 129,
	130, 131, 132, 133, 134,
	135, 136, 137, 138, 140,
	141, 142, 143, 144, 146,
	147, 148, 150, 151, 153,
	154, 156, 157, 159, 160,
	162, 163, 165, 167, 168,
	170, 172, 174, 176, 178,
	180, 182, 184, 186, 188,
	190, 192, 195, 197, 199,
	202, 204, 207, 210, 212,
	215, 218, 221, 224, 227,
	230, 234, 237, 240, 244,
	248, 252, 256, 260, 264,
	268, 273, 277, 282, 287,
	292, 297, 303, 309, 315,
	321, 327, 334, 341, 348,
	356, 364, 372, 380, 390,
	399, 409, 420, 431, 442,
	455, 468, 481, 496, 512,
	528, 546, 564, 585, 606,
	630, 655, 682, 712, 744,
	780, 819, 862, 910, 963,
	1024, 1092, 1170, 1260, 1365,
	1489, 1638, 1820, 2048, 0XFFFF
};

static int clip_xyd(int gain)
{
	int ret_gain = 0;

	if (gain < 64) {
		ret_gain = 64;
	} else if (gain > 2005) {
		ret_gain = 2005;
	} else {
		ret_gain = gain;
	}
	return ret_gain;
}

static int setSensorGain(struct v4l2_subdev *sd, int gain)
{
	// function parameter "gain" = sensor real gain * 64
	//
	struct sensor_info *info = to_state(sd);
	int i;
	int again;
	int dgain = 64;

	// m_gain =gain;
	for (i = 0; i < 250; i++) {
		if ((Tab_SensorRegToGain[i] <= gain) && (Tab_SensorRegToGain[i + 1] >= gain))
			break;
	}
	again = i;

	// Dgain
	dgain = clip_xyd(gain >> 4);

	//(Linear mode): Gain
	//(DOL2 mode L-Frame): Gain
	sensor_write(sd, 0x3118, again);			//set again
	sensor_write(sd, 0x311c, dgain & 0xff);		//set Dgain
	sensor_write(sd, 0x311d, dgain >> 8);		//set Dgain

	// DOL2 mode M-Frame: Gain
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		sensor_write(sd, 0x3155, again);			//set again //shaokc. need update cv5003 reg address @ DOL2 mode
		sensor_write(sd, 0x314E, dgain & 0xff);		//set Dgain //shaokc. need update cv5003 reg address @ DOL2 mode
		sensor_write(sd, 0x314F, dgain >> 8);		//set Dgain //shaokc. need update cv5003 reg address @ DOL2 mode
	}

	return 0;
}

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);

	if (gain_val == info->gain) {
		return 0;
	}

	sensor_dbg("gain_val:%d\n", gain_val);
	setSensorGain(sd, gain_val * 4);

	info->gain = gain_val;

	return 0;
}

static int cv5003_sensor_vts;
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

	if (exp_val < (4 * 16)) {
		exp_val = 4 * 16;
	}

	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	shutter = exp_val >> 4;
	if (shutter > cv5003_sensor_vts - 10)
		frame_length = shutter + 10;
	else
		frame_length = cv5003_sensor_vts;
	sensor_dbg("frame_length = %d\n", frame_length);
	sensor_write(sd, 0x301C, frame_length & 0xff);
	sensor_write(sd, 0x301D, frame_length >> 8);

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
	info->exp = exp_val;
	info->gain = gain_val;

	return 0;
}

static data_type sensor_flip_status;
static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3028, &get_value);
	sensor_dbg("ready to vflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x02;
	} else {
		set_value = get_value & 0xFD;
	}
	sensor_write(sd, 0x3028, set_value);
	sensor_flip_status = set_value;

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3028, &get_value);
	sensor_dbg("ready to hflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x01;
	} else {
		set_value = get_value & 0xFE;
	}
	sensor_write(sd, 0x3028, set_value);
	sensor_flip_status = set_value;

	return 0;
}

static int sensor_g_flip(struct v4l2_subdev *sd, struct sensor_flip *flip)
{
#ifdef CONFIG_ENABLE_SENSOR_FLIP_OPTION
	if (sensor_flip_status & 0x01)
		flip->hflip = 1;
	else
		flip->hflip = 0;

	if (sensor_flip_status & 0x02)
		flip->vflip = 1;
	else
		flip->vflip = 0;
#else
	flip->hflip = 0;
	flip->vflip = 0;
#endif
	return 0;
}

static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);
	data_type get_value = 0, check_value = 0;

	sensor_read(sd, 0x3028, &get_value);
	check_value = get_value & 0x03;
	check_value = sensor_flip_status & 0x3;
	sensor_print("0x3028 = 0x%x, check_value = 0x%x\n", get_value, check_value);
	switch (check_value) {
	case 0x00:
		sensor_dbg("RGGB\n");
		*code = MEDIA_BUS_FMT_SRGGB10_1X10;
		break;
	case 0x01:
		sensor_dbg("GRBG\n");
		*code = MEDIA_BUS_FMT_SGRBG10_1X10;
		break;
	case 0x02:
		sensor_dbg("GBRG\n");
		*code = MEDIA_BUS_FMT_SGBRG10_1X10;
		break;
	case 0x03:
		sensor_dbg("BGGR\n");
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
		break;
	default:
		 *code = info->fmt->mbus_code;
	}

	return 0;
}

/*
 * Stuff that knows about the sensor.
 */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	//int ret = 0;

	switch (on) {
	case STBY_ON:
		sensor_dbg("STBY_ON!\n");
		cci_lock(sd);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		cci_unlock(sd);
		break;
	case PWR_ON:
		sensor_print("PWR_ON!\n");
		cci_lock(sd);

		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);

		/*
		vin_set_pmu_channel(sd, IOVDD, ON);
		usleep_range(10, 20);
		vin_set_pmu_channel(sd, DVDD, ON);
		usleep_range(10, 20);
		vin_set_pmu_channel(sd, AVDD, ON);
		*/

		usleep_range(10, 20);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);

		usleep_range(100, 120);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(3000, 3200);
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);

		cci_unlock(sd);
		break;
	case PWR_OFF:
		sensor_print("PWR_OFF!\n");
		cci_lock(sd);
		vin_set_mclk(sd, OFF);

		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);

		/*
		vin_set_pmu_channel(sd, AVDD, OFF);
		vin_set_pmu_channel(sd, IOVDD, OFF);
		vin_set_pmu_channel(sd, DVDD, OFF);
		*/

		vin_gpio_set_status(sd, RESET, 0);
		cci_unlock(sd);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_reset(struct v4l2_subdev *sd, u32 val)
{
	sensor_dbg("%s: val=%d\n", __func__, val);
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
		sensor_print("eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval, times_out);
		usleep_range(200, 220);
		times_out--;
	} while (eRet < 0 && times_out > 0);

	sensor_read(sd, ID_REG_HIGH, &rdval);
	sensor_print("ID_VAL_HIGH = 0x%2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;

	sensor_read(sd, ID_REG_LOW, &rdval);
	sensor_print("ID_VAL_LOW = 0x%2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	sensor_print("Done!\n");
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
	info->width        = 2880;
	info->height       = 1620;
	info->hflip        = 0;
	info->vflip        = 0;
	info->gain         = 0;
	info->exp          = 0;

	info->tpf.numerator      = 1;
	info->tpf.denominator    = 15;	/* 15fps */
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
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;
//	case VIDIOC_VIN_SENSOR_SET_FPS:
//		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
//		break;
	case VIDIOC_VIN_SENSOR_GET_FPS:
		ret = sensor_g_fps(sd, (struct sensor_fps *)arg);
		break;
	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
	case VIDIOC_VIN_SET_IR:
		sensor_set_ir(sd, (struct ir_switch *)arg);
		break;
	case VIDIOC_VIN_SENSOR_GET_FLIP:
		sensor_g_flip(sd, (struct sensor_flip *)arg);
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
#if RAW12
		.mbus_code = MEDIA_BUS_FMT_SRGGB12_1X12,
#else
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
#endif
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
		.width = 2880,
		//.height = 1620,
		.height = 1616,
		.hoffset = 0,
		.voffset = 0,
		.hts = 405,
		.vts = 3464,
		.pclk = 42087600,
		.mipi_bps = 840 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		.intg_min = 4 << 4,
		.intg_max = (3464 - 10) << 4,
		.gain_min = 1 << 4,
		.gain_max = 256 << 4,
		.regs = sensor_2880_1620_30_regs,
		.regs_size = ARRAY_SIZE(sensor_2880_1620_30_regs),
		.set_size = NULL,
	 },

	{
		.width = 2880,
		//.height = 1620,
		.height = 1616,
		.hoffset = 0,
		.voffset = 0,
		.hts = 405,
		.vts = 6928,
		.pclk = 42087600,
		.mipi_bps = 840 * 1000 * 1000,
		.fps_fixed = 15,
		.bin_factor = 1,
		.intg_min = 4 << 4,
		.intg_max = (6928 - 10) << 4,
		.gain_min = 1 << 4,
		.gain_max = 128 << 4,
		.regs = sensor_2880_1620_15_regs,
		.regs_size = ARRAY_SIZE(sensor_2880_1620_15_regs),
		.set_size = NULL,
	 },

	{
		.width = 2880,
		.height = 1620,
		.hoffset = 0,
		.voffset = 0,
		.hts = 900,
		.vts = 3300,
		.pclk = 49500000,
		.mipi_bps = 792 * 1000 * 1000,
		.fps_fixed = 16,   // 16.66667
		.bin_factor = 1,
		.intg_min = 4 << 4,
		.intg_max = (1650 - 8) << 4,
		.gain_min = 1 << 4,
		.gain_max = 256 << 4,
		.regs = sensor_2880_1620_16p6667_regs,
		.regs_size = ARRAY_SIZE(sensor_2880_1620_16p6667_regs),
		.set_size = NULL,
		.top_clk = 330*1000*1000,
		.isp_clk = 330*1000*1000,
	 },

	{
		.width      = 1440,
		.height     = 810,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 737,
		.vts        = 1680,
		.pclk       = 148579200,
		.mipi_bps   = 1180 * 1000 * 1000,
		.fps_fixed  = 120,
		.bin_factor = 1,
		.intg_min 	= 4 << 4,
		.intg_max 	= (1680 - 8) << 4,
		.gain_min 	= 1 << 4,
		.gain_max 	= 256 << 4,
		.gain_max = 256 << 4,
		.regs = sensor_1440_810p_120fps_10bit_regs,
		.regs_size = ARRAY_SIZE(sensor_1440_810p_120fps_10bit_regs),
		.set_size = NULL,
		.top_clk = 330 * 1000 * 1000,
		.isp_clk = 330 * 1000 * 1000,
	 },
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	//struct sensor_info *info = to_state(sd);

	cfg->type  = V4L2_MBUS_CSI2;
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
		//sensor_s_exp_gain(sd, &exp_gain);
	}
#else
	if (wsize->regs) {
		printk("sensor_write_array wsize->regs...\n");
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
	}
#endif

	if (wsize->set_size)
		wsize->set_size(sd);

	info->width = wsize->width;
	info->height = wsize->height;
	cv5003_sensor_vts = wsize->vts;

	sensor_read(sd, 0x3028, &sensor_flip_status);
	sensor_print("sensor_flip_status = %d\n", sensor_flip_status);
	sensor_dbg("s_fmt set width = %d, height = %d, vts = \n", wsize->width, wsize->height, cv5003_sensor_vts);

	return 0;
}

static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_dbg("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
			 info->current_wins->width, info->current_wins->height,
			 info->current_wins->fps_fixed, info->fmt->mbus_code);
	printk("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
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
