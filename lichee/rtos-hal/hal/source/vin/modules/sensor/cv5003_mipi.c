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

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR  0x5003

/*
 * Our nominal (default) frame rate.
 */
#define ID_REG_HIGH		0x3003
#define ID_REG_LOW		0x3002
#define ID_VAL_HIGH		((V4L2_IDENT_SENSOR) >> 8)
#define ID_VAL_LOW		((V4L2_IDENT_SENSOR) & 0xff)
#define SENSOR_FRAME_RATE 15

#define I2C_ADDR 0x6a

#define SENSOR_NUM 0x2
#define SENSOR_NAME "cv5003_mipi"
#define SENSOR_NAME_2 "cv5003_mipi_2"

static int sensor_power_count[2];
static int sensor_stream_count[2];
static struct sensor_exp_gain glb_exp_gain;
static struct sensor_format_struct *current_win[2];
static struct sensor_format_struct *current_switch_win[2];

/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {
};

static struct regval_list sensor_2880_1620p15_regs[] = {
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

	// {0x3000, 0x00},
};

static struct regval_list sensor_1440_810p_100fps_10bit_regs[] = {
	{0x300D, 0x01},  // [0]     OSC_EXTCLK_SEL      = 1(dec:1)
	{0x300E, 0x00},
	{0x30A8, 0x07},
	{0x3808, 0x46},
	{0x380A, 0x02},
	{0x301C, 0x08},
	{0x301D, 0x07},
	{0x301E, 0x00},
	{0x3020, 0x85},
	{0x3021, 0x01},
	{0x3204, 0x40},//BLK LEVEL 64
	{0x3478, 0x00},

	{0x3109, 0x01}, //split gain

	//S810P setting
	{0x3034, 0x02},
	{0x3035, 0x00},
	{0x3036, 0x2A},
	{0x3037, 0x03},
	{0x3038, 0x02},
	{0x3039, 0x00},
	{0x303A, 0xA0},
	{0x303B, 0x05},
	//for V-normal
	{0x31BC, 0x0C},
	{0x31C4, 0x20},
	{0x31C5, 0x00},
	{0x31CA, 0x24},
	{0x31CB, 0x00},
	{0x31C6, 0x5C},
	{0x31C7, 0x06},
	{0x385A, 0x03},
	{0x359F, 0x13},
	{0x395F, 0x13},
	{0x3808, 0xA5},
	{0x3809, 0x00},
	{0x380a, 0x03},
	//divider
	{0x305c, 0x00},
	//FRAME_LENGTH:1710
	{0x301c, 0xAE},
	{0x301d, 0x06},
	{0x301e, 0x00},
	//LINE_LENGTH:386
	{0x3020, 0x82},
	{0x3021, 0x01},
	//MIPI global timing
	//满足min要求的最低配置.
	{0x3420, 0x3f}, //THSPREPARE:48ns; min limit:43; max limt:89.54545454545455
	{0x3422, 0xcf}, //THSZERO   :157ns; min limit:153; max limt:1000
	{0x3424, 0x5f}, //THSTRAIL  :72ns; min limit:67; max limt:1000
	{0x3426, 0x87}, //THSEXIT   :103ns; min limit:100; max limt:1000
	{0x3428, 0x47}, //TLPX      :54ns; min limit:50; max limt:1000

	// {0x3000, 0x00}, //Streaming
};

static struct regval_list sensor_100fps_to_15fps_regs[] = {
	{0x300D, 0x01},
	{0x300E, 0x00},
	{0x30A8, 0x04},
	{0x385A, 0x03},
	{0x3204, 0x40}, //BLK LEVEL 64
	{0x3478, 0x00},

	// {0x3048, 0x08},

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

	// {0x3000, 0x00},
};


//#endif
/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {
};

static int cv5003_sensor_vts;
static int sensor_s_exp(int id, unsigned int exp_val)
{
	unsigned int frame_length = cv5003_sensor_vts;
	data_type frame_length_tmp = 0;

	unsigned int shutter0 = 0;
	unsigned int shutter0_high = 0, shutter0_mid = 0, shutter0_low = 0;

	sensor_read(id, 0x301E, &frame_length_tmp);
	frame_length = frame_length_tmp << 16;
	sensor_read(id, 0x301D, &frame_length_tmp);
	frame_length |= frame_length_tmp << 8;
	sensor_read(id, 0x301C, &frame_length_tmp);
	frame_length |= frame_length_tmp;

	shutter0 = frame_length - (exp_val >> 4) ;
	shutter0 >>= 1;
	shutter0 <<= 1;	// shutter0 = 2n
	if (shutter0 <= 4)
		shutter0 = 4;
	shutter0_low =  (unsigned char)((0x0000ff & shutter0));
	shutter0_mid =  (unsigned char)((0x00ff00 & shutter0) >> 8);
	shutter0_high = (unsigned char)((0x0f0000 & shutter0) >> 16);

	sensor_write(id, 0x3048, shutter0_low);
	sensor_write(id, 0x3049, shutter0_mid);
	sensor_write(id, 0x304A, shutter0_high);
	sensor_dbg("sensor_set_exp = %d %d line Done!\n", exp_val, shutter0);

	return 0;
}

unsigned int Tab_SensorRegToGain[250] =
{
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

static int setSensorGain(int id, int gain)
{
	// function parameter "gain" = sensor real gain * 64
	int i;
	int again;
	int dgain = 64;
	// m_gain =gain;
	for (i = 0; i < 250; i++)
		if ((Tab_SensorRegToGain[i] <= gain)&&(Tab_SensorRegToGain[i+1] >= gain))
			break;
	again = i;

	// Dgain
	dgain = clip_xyd(gain >> 4);

	// (Linear mode): Gain
	// (DOL2 mode L-Frame): Gain
	sensor_write(id, 0x3118, again);			//set again
	sensor_write(id, 0x311C, dgain & 0xff);		//set Dgain
	sensor_write(id, 0x311D, dgain >> 8);		//set Dgain

	return 0;
}

static int sensor_s_gain(int id, int gain_val)
{
	sensor_dbg("gain_val:%d\n", gain_val);
	setSensorGain(id, gain_val * 4);

	return 0;
}

static int sensor_s_exp_gain(int id, struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val;
	int shutter = 0, frame_length = 0;

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
	sensor_write(id, 0x301C, frame_length & 0x000ff);
	sensor_write(id, 0x301D, (frame_length & 0x0ff00) >> 8);
	sensor_write(id, 0x301E, (frame_length & 0xf0000) >> 16);

	sensor_s_exp(id, exp_val);
	sensor_s_gain(id, gain_val);

	glb_exp_gain.exp_val = exp_val;
	glb_exp_gain.gain_val = gain_val;

	printk("gain_val:%d, exp_val:%d\n", gain_val, exp_val);

	return 0;
}


static int sensor_flip_status;
static int sensor_s_vflip(int id, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(id, 0x3028, &get_value);
	sensor_dbg("ready to vflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x02;
	} else {
		set_value = get_value & 0xFD;
	}
	sensor_write(id, 0x3028, set_value);
	sensor_flip_status = set_value;

	return 0;
}

static int sensor_s_hflip(int id, int enable)
{
	data_type get_value;
	data_type set_value;

	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(id, 0x3028, &get_value);
	sensor_dbg("ready to hflip, regs_data = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x01;
	} else {
		set_value = get_value & 0xFE;
	}
	sensor_write(id, 0x3028, set_value);
	sensor_flip_status = set_value;

	return 0;
}


/*
* Stuff that knows about the sensor.
*/
static int sensor_power(int id, int on)
{
	if (on && (sensor_power_count[id])++ > 0)
		return 0;
	else if (!on && (sensor_power_count[id] == 0 || --(sensor_power_count[id]) > 0))
		return 0;

	switch (on) {
	case PWR_ON:
		sensor_dbg("PWR_ON!\n");
		vin_gpio_set_status(id, RESET, 1);
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		hal_usleep(10);
		vin_gpio_write(id, RESET, CSI_GPIO_HIGH);
		hal_usleep(100);
		vin_set_mclk_freq(id, MCLK);
		hal_usleep(3000);
		vin_set_mclk(id, 1);
		hal_usleep(1000);
		break;

	case PWR_OFF:
		sensor_dbg("PWR_OFF! do nothing\n");
		vin_set_mclk(id, 0);
		hal_usleep(1000);
		vin_gpio_set_status(id, RESET, 1);
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		vin_gpio_set_status(id, RESET, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sensor_set_ir(int id, int status)
{
	vin_gpio_set_status(id, IR_CUT0, 1);
	vin_gpio_set_status(id, IR_CUT1, 1);
	vin_gpio_set_status(id, IR_LED, 1);
	switch (status) {
	case IR_DAY:
		vin_gpio_write(id, IR_CUT0, CSI_GPIO_HIGH);
		vin_gpio_write(id, IR_CUT1, CSI_GPIO_LOW);
		vin_gpio_write(id, IR_LED, CSI_GPIO_LOW);
		break;
	case IR_NIGHT:
		vin_gpio_write(id, IR_CUT0, CSI_GPIO_LOW);
		vin_gpio_write(id, IR_CUT1, CSI_GPIO_HIGH);
		vin_gpio_write(id, IR_LED, CSI_GPIO_HIGH);
		break;
	default:
		return -1;
	}
	return 0;
}

#if 0
static int sensor_reset(int id, u32 val)
{

	sensor_dbg("%s: val=%d\n", __func__);
	switch (val) {
	case 0:
		vin_gpio_write(id, RESET, CSI_GPIO_HIGH);
		hal_usleep(1000);
		break;
	case 1:
		vin_gpio_write(id, RESET, CSI_GPIO_LOW);
		hal_usleep(1000);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
#endif

static int sensor_detect(int id)
{
	data_type rdval;
	int eRet;
	int times_out = 3;
	do {
		eRet = sensor_read(id, ID_REG_HIGH, &rdval);
		sensor_dbg("eRet:%d, ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rdval, times_out);
		hal_usleep(200);
		times_out--;
	} while (eRet < 0  &&  times_out > 0);
	//} while (eRet < 0);

	sensor_read(id, ID_REG_HIGH, &rdval);
	sensor_dbg("ID_VAL_HIGH = 0x%2x, Done!\n", rdval);
	if (rdval != ID_VAL_HIGH)
		return -ENODEV;

	sensor_read(id, ID_REG_LOW, &rdval);
	sensor_dbg("ID_VAL_LOW = 0x%2x, Done!\n", rdval);
	if (rdval != ID_VAL_LOW)
		return -ENODEV;

	sensor_dbg("Done!\n");
	return 0;
}

static int sensor_init(int id)
{
	int ret;

	sensor_dbg("sensor_init\n");

	/*Make sure it is a target sensor */
	ret = sensor_detect(id);
	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	return 0;
}

/*
 * Store information about the video data format.
 */
static struct sensor_format_struct sensor_formats[] = {
#if defined CONFIG_ISP_READ_THRESHOLD || defined CONFIG_ISP_ONLY_HARD_LIGHTADC // FULL_SIZE
	// {
	// 	.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
	// 	.width      = 1440,
	// 	.height     = 810,
	// 	.hoffset    = 0,
	// 	.voffset    = 0,
	// 	.hts        = 386,
	// 	.vts        = 1710,
	// 	.pclk       = 66 * 1000 * 1000,
	// 	.mipi_bps   = 1319 * 1000 * 1000,
	// 	.fps_fixed  = 100,
	// 	.bin_factor = 1,
	// 	.intg_min 	= 4 << 4,
	// 	.intg_max 	= (1710 - 8) << 4,
	// 	.gain_min = 1 << 4,
	// 	.gain_max = 128 << 4,
	// 	.offs_h     = 0,
	// 	.offs_v     = 0,
	// 	.regs	    = sensor_1440_810p_100fps_10bit_regs,
	// 	.regs_size  = ARRAY_SIZE(sensor_1440_810p_100fps_10bit_regs),
	// },

	{
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width      = 2880,
		// .height     = 1620,
		.height     = 1616,
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
		.offs_h     = 0,
		.offs_v     = 0,
		.regs	    = sensor_2880_1620p15_regs,
		.regs_size  = ARRAY_SIZE(sensor_2880_1620p15_regs),
	},

#else //CONFIG_ISP_FAST_CONVERGENCE || CONFIG_ISP_HARD_LIGHTADC
	{
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.width      = 1440,
		.height     = 810,
		.hoffset    = 0,
		.voffset    = 0,
		.hts        = 386,
		.vts        = 1710,
		.pclk       = 66 * 1000 * 1000,
		.mipi_bps   = 1319 * 1000 * 1000,
		.fps_fixed  = 100,
		.bin_factor = 1,
		.intg_min 	= 4 << 4,
		.intg_max 	= (1710 - 10) << 4,
		.gain_min = 1 << 4,
		.gain_max = 128 << 4,
		.offs_h     = 0,
		.offs_v     = 0,
		.regs	    = sensor_1440_810p_100fps_10bit_regs,
		.regs_size  = ARRAY_SIZE(sensor_1440_810p_100fps_10bit_regs),
	},
#endif
};

static struct sensor_format_struct *sensor_get_format(int id, int isp_id)
{
#if defined CONFIG_ISP_READ_THRESHOLD || defined CONFIG_ISP_ONLY_HARD_LIGHTADC
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
#else //CONFIG_ISP_FAST_CONVERGENCE || CONFIG_ISP_HARD_LIGHTADC
	if (current_win[id])
		return current_win[id];

	current_win[id] = &sensor_formats[0];
	sensor_print("fine wdr is %d, fps is %d\n", sensor_formats[0].wdr_mode, sensor_formats[0].fps_fixed);
	return &sensor_formats[0];
#endif
}

static struct sensor_format_struct switch_sensor_formats[] = {
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
	{
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.width      = 2880,
		.height     = 1616,
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
		.offs_h     = 0,
		.offs_v     = 0,
		.switch_regs	    = sensor_100fps_to_15fps_regs,
		.switch_regs_size  = ARRAY_SIZE(sensor_100fps_to_15fps_regs),
	},
#endif
};

static struct sensor_format_struct *sensor_get_switch_format(int id, int isp_id)
{
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);
	struct sensor_format_struct *sensor_format = NULL;
	int wdr_on = isp_get_cfg[ispid].sensor_wdr_on;
	int fps = isp_get_cfg[ispid].sensor_get_fps;
	int i;

	if (current_switch_win[id])
		return current_switch_win[id];

	for (i = 0; i < ARRAY_SIZE(switch_sensor_formats); i++) {
		if (switch_sensor_formats[i].wdr_mode == wdr_on) {
			if (switch_sensor_formats[i].fps_fixed == fps) {
				sensor_format = &switch_sensor_formats[i];
				sensor_print("switch fine wdr is %d, fine fps is %d\n", wdr_on, fps);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		for (i = 0; i < ARRAY_SIZE(switch_sensor_formats); i++) {
			if (switch_sensor_formats[i].wdr_mode == wdr_on) {
				sensor_format = &switch_sensor_formats[i];
				isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
				sensor_print("switch fine wdr is %d, use fps is %d\n", wdr_on, sensor_format->fps_fixed);
				goto done;
			}
		}
	}

	if (sensor_format == NULL) {
		sensor_format = &switch_sensor_formats[0];
		isp_get_cfg[ispid].sensor_wdr_on = sensor_format->wdr_mode;
		isp_get_cfg[ispid].sensor_get_fps = sensor_format->fps_fixed;
		sensor_print("switch use wdr is %d, use fps is %d\n", sensor_format->wdr_mode, sensor_format->fps_fixed);
	}

done:
	current_switch_win[id] = sensor_format;
	return sensor_format;
#else
	return NULL;
#endif
}

static int sensor_g_mbus_config(int id, struct v4l2_mbus_config *cfg, struct mbus_framefmt_res *res)
{
	cfg->type  = V4L2_MBUS_CSI2;
	cfg->flags = 0 | V4L2_MBUS_CSI2_2_LANE | V4L2_MBUS_CSI2_CHANNEL_0;
	res->res_time_hs = 0x28;

	return 0;
}

static int sensor_reg_init(int id, int isp_id)
{
	int ret = 0;
	int ispid = clamp(isp_id, 0, ISP_GET_CFG_NUM - 1);
	struct sensor_exp_gain exp_gain;

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

	cv5003_sensor_vts = current_win[id]->vts;
	if (ispid == 0) {
		exp_gain.exp_val = clamp(*((unsigned int *)ISP0_NORFLASH_SAVE + 2), 16, 6928 << 4);
		exp_gain.gain_val = clamp(*((unsigned int *)ISP0_NORFLASH_SAVE + 1), 16, 128 << 4);
	} else {
		exp_gain.exp_val = clamp(*((unsigned int *)ISP1_NORFLASH_SAVE + 2), 16, 6928 << 4);
		exp_gain.gain_val = clamp(*((unsigned int *)ISP1_NORFLASH_SAVE + 1), 16, 128 << 4);
	}

	sensor_print("%s, exp = %d, gain = %d\n", __func__, exp_gain.exp_val, exp_gain.gain_val);
	sensor_s_exp_gain(id, &exp_gain);
	sensor_write(id, 0x3000, 0x0);

	return 0;
}

static int sensor_s_stream(int id, int isp_id, int enable)
{
	if (enable && sensor_stream_count[id]++ > 0)
		return 0;
	else if (!enable && (sensor_stream_count[id] == 0 || --sensor_stream_count[id] > 0))
		return 0;

	sensor_dbg("%s on = %d\n", __func__, enable);

	if (!enable)
		return 0;

	return sensor_reg_init(id, isp_id);
}

static int sensor_s_switch(int id)
{
#if defined CONFIG_ISP_FAST_CONVERGENCE || defined CONFIG_ISP_HARD_LIGHTADC
	struct sensor_exp_gain exp_gain;
	int ret = -1;

	/* cv5003 need to do reset for load new regs */
	vin_gpio_write(id, RESET, CSI_GPIO_LOW);
	hal_usleep(10);
	vin_gpio_write(id, RESET, CSI_GPIO_HIGH);
	hal_usleep(100);

	cv5003_sensor_vts = current_switch_win[id]->vts;
	if (current_switch_win[id]->switch_regs)
		ret = sensor_write_array(id, current_switch_win[id]->switch_regs, current_switch_win[id]->switch_regs_size);
	else
		sensor_err("cannot find 1440_810p100fps to 2880_1620p%dfps reg\n", current_switch_win[id]->fps_fixed);
	if (ret < 0)
		return ret;

	if (glb_exp_gain.exp_val && glb_exp_gain.gain_val) {
		exp_gain.exp_val = glb_exp_gain.exp_val;
		exp_gain.gain_val = glb_exp_gain.gain_val;
	} else {
		exp_gain.exp_val = 1000;
		exp_gain.gain_val = 16;
	}
	sensor_s_exp_gain(id, &exp_gain); /* make switch_regs firstframe  */

	sensor_write(id, 0x3000, 0x0);
	sensor_print("sensor_s_switch & stream on!!!!!!!!!!\n");
#endif
	return 0;
}

static int sensor_test_i2c(int id)
{
	int ret;
	sensor_power(id, PWR_ON);
	ret = sensor_init(id);
	sensor_power(id, PWR_OFF);

	return ret;
}

struct sensor_fuc_core cv5003_core  = {
	.g_mbus_config = sensor_g_mbus_config,
	.sensor_test_i2c = sensor_test_i2c,
	.sensor_power = sensor_power,
	.s_ir_status = sensor_set_ir,
	.s_stream = sensor_s_stream,
	.s_switch = sensor_s_switch,
	.s_exp_gain = sensor_s_exp_gain,
	.sensor_g_format = sensor_get_format,
	.sensor_g_switch_format = sensor_get_switch_format,
};
