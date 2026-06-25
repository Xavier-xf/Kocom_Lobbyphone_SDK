/**
  ******************************************************************************
  * @file    h63_dvp.c
  * @author  Panjunwen <panjunwenswc@allwinnertech.com>
  * @brief   A V4L2 driver for H63 Raw cameras
  *
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
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


MODULE_AUTHOR("Clover");
MODULE_DESCRIPTION("A low-level driver for h63 sensors");
MODULE_LICENSE("GPL");


/* Private Constants ---------------------------------------------------------*/
/** @defgroup H63_DVP_Private_Constants H63 DVP Private Constants
  * @brief    H63 DVP Private Constants
  * @{
  */

/**
  * @brief MCLK frequency config macro definition for H63
  */
#define MCLK_FREQ			(24 * 1000 * 1000)

/**
  * @brief VSYNC/HSYNC/PCLK polarity macro definition for H63
  */
#define H63_VREF_POL		(V4L2_MBUS_VSYNC_ACTIVE_LOW)
#define H63_HREF_POL		(V4L2_MBUS_HSYNC_ACTIVE_HIGH)
#define H63_PCLK_POL		(V4L2_MBUS_PCLK_SAMPLE_FALLING)

/**
  * @brief H63 register macro definittion
  */
#define H63_REG_GAIN		(0x00)		/*!< H63 Gain Register			*/
#define H63_REG_EXP_LOW		(0x01)		/*!< H63 Exposure Low  Register	*/
#define H63_REG_EXP_HIGH	(0x02)		/*!< H63 Exposure High Register	*/
#define H63_REG_PIDH		(0x0A)		/*!< H63 PIDH Register			*/
#define H63_REG_PIDL		(0x0B)		/*!< H63 PIDL Register			*/
#define H63_REG_SYS			(0x12)		/*!< H63 SYS Register			*/
#define H63_REG_FrameH_LOW	(0x22)		/*!< H63 FrameH Low Register	*/
#define H63_REG_FrameH_HIGH	(0x23)		/*!< H63 FrameH High Register	*/

#define H63_REG_SAEC0		(0x05)		/*!< H63 SAEC0 Register			*/
#define H63_REG_SFramSt		(0x06)		/*!< H63 SFramSt Register		*/
#define H63_REG_SAEC1		(0x08)		/*!< H63 SAEC1 Register			*/
#define H63_REG_DVP1		(0x0C)		/*!< H63 DVP1 Register			*/
#define H63_REG_DVP2		(0x0D)		/*!< H63 DVP2 Register			*/
#define H63_REG_DVP3		(0x1D)		/*!< H63 DVP3 Register			*/
#define H63_REG_DVP4		(0x1E)		/*!< H63 DVP4 Register			*/
#define H63_REG_FrameW_LOW	(0x20)		/*!< H63 FrameW Low Register	*/
#define H63_REG_FrameW_HIGH	(0x21)		/*!< H63 FrameW High Register	*/
#define H63_REG_Hwin		(0x24)		/*!< H63 Hwin Register			*/
#define H63_REG_Vwin		(0x25)		/*!< H63 Vwin Register			*/
#define H63_REG_HVWin		(0x26)		/*!< H63 HVWin Register			*/
#define H63_REG_HwinSt		(0x27)		/*!< H63 HwinSt Register		*/
#define H63_REG_VwinSt		(0x28)		/*!< H63 VwinSt Register		*/
#define H63_REG_HVWinSt		(0x29)		/*!< H63 HVWinSt Register		*/
#define H63_REG_BLC_TGT		(0x49)		/*!< H63 BLC_TGT Register		*/
#define H63_REG_BLCCtrl		(0x4A)		/*!< H63 BLCCtrl Register		*/
#define H63_REG_BLC_B		(0x4B)		/*!< H63 BLC_B Register			*/
#define H63_REG_BLC_Gb		(0x4C)		/*!< H63 BLC_Gb Register		*/
#define H63_REG_BLC_Gr		(0x4D)		/*!< H63 BLC_Gr Register		*/
#define H63_REG_BLC_R		(0x4E)		/*!< H63 BLC_R Register			*/
#define H63_REG_BLC_H		(0x4F)		/*!< H63 BLC_H Register			*/
#define H63_REG_RAMP3		(0x65)		/*!< H63 RAMP3 Register			*/
#define H63_REG_PWC3		(0x69)		/*!< H63 PWC3 Register			*/
#define H63_REG_PWC4		(0x6A)		/*!< H63 PWC4 Register			*/
#define H63_REG_Mipi1		(0x70)		/*!< H63 Mipi1 Register			*/
#define H63_REG_Mipi2		(0x71)		/*!< H63 Mipi2 Register			*/
#define H63_REG_Mipi3		(0x72)		/*!< H63 Mipi3 Register			*/
#define H63_REG_Mipi4		(0x73)		/*!< H63 Mipi4 Register			*/
#define H63_REG_Mipi5		(0x74)		/*!< H63 Mipi5 Register			*/
#define H63_REG_Mipi6		(0x75)		/*!< H63 Mipi6 Register			*/
#define H63_REG_Mipi7		(0x76)		/*!< H63 Mipi7 Register			*/
#define H63_REG_Mipi8		(0x77)		/*!< H63 Mipi8 Register			*/
#define H63_REG_DigData		(0x80)		/*!< H63 DigData Register		*/

#define H63_ID_VAL_HIGH		(0x08)		/*!< H63 ID Value High			*/
#define H63_ID_VAL_LOW		(0x48)		/*!< H63 ID Value Low			*/

/**
  * @brief H63 sensor numbers and names macro definittion
  */
#define SENSOR_NUM			(2)
#define SENSOR_NAME			"h63_dvp"
#define SENSOR_NAME_2		"h63_dvp_2"
#define SOI_MIRROR_FLIP

/**
  * @}
  */


/* Private Macros ------------------------------------------------------------*/
/** @defgroup H63_DVP_Private_Macros H63 DVP Private Macros
  * @brief    H63 DVP Private Macros
  * @{
  */

/**
  * @brief BIT macro definition
  */
#define BIT(n)						(1 << (n))

/**
  * @}
  */


/* Private Types -------------------------------------------------------------*/
/* Private Variables ---------------------------------------------------------*/
/** @defgroup H63_DVP_Private_Variables H63 DVP Private Variables
  * @brief    H63 DVP Private Variables
  * @{
  */

/**
  * @brief Default register configs
  */
static const struct regval_list sensor_default_regs[] = {
};

/**
  * @brief 720p15 Register configs
  */
static const struct regval_list sensor_720p15_regs[] = {
	{0x12, 0x40},
	{0x48, 0x85},
	{0x48, 0x05},
	{0x0E, 0x11},
	{0x0F, 0x84},
	{0x10, 0x1E},
	{0x11, 0x80},
	{0x57, 0x60},
	{0x58, 0x18},
	{0x61, 0x10},
	{0x46, 0x00},
	{0x0D, 0xA0},
	{0x20, 0x20},
	{0x21, 0x03},
	{0x22, 0xDC},//0xEE
	{0x23, 0x05},//0x02
	{0x24, 0x80},
	{0x25, 0xD0},
	{0x26, 0x22},
	{0x27, 0x8B},
	{0x28, 0x15},
	{0x29, 0x02},
	{0x2A, 0x80},
	{0x2B, 0x12},
	{0x2C, 0x00},
	{0x2D, 0x00},
	{0x2E, 0xBA},
	{0x2F, 0x60},
	{0x41, 0x84},
	{0x42, 0x02},
	{0x47, 0x42},
	{0x76, 0x40},
	{0x77, 0x06},
	{0x80, 0x01},
	{0xAF, 0x22},
	{0x8A, 0x00},
	{0xA6, 0x00},
	{0x8D, 0x49},
	{0xAB, 0x00},
	{0x1D, 0xFF},
	{0x1E, 0x1F},
	{0x6C, 0xC0},
	{0x9E, 0xF8},
	{0x9C, 0xE1},
	{0x3A, 0xAC},
	{0x3B, 0x18},
	{0x3C, 0x5D},
	{0x3D, 0x80},
	{0x3E, 0x6E},
	{0x31, 0x07},
	{0x32, 0x14},
	{0x33, 0x12},
	{0x34, 0x1C},
	{0x35, 0x1C},
	{0x56, 0x12},
	{0x59, 0x20},
	{0x85, 0x14},
	{0x64, 0xD2},
	{0x8F, 0x90},
	{0xA4, 0x87},
	{0xA7, 0x80},
	{0xA9, 0x48},
	{0x45, 0x01},
	{0x5B, 0xA0},
	{0x5C, 0x6C},
	{0x5D, 0x44},
	{0x5E, 0x81},
	{0x63, 0x0F},
	{0x65, 0x12},
	{0x66, 0x43},
	{0x67, 0x79},
	{0x68, 0x04},
	{0x69, 0x78},
	{0x6A, 0x28},
	{0x7A, 0x66},
	{0xA5, 0x03},
	{0x94, 0xC0},
	{0x13, 0x81},
	{0x96, 0x84},
	{0xB7, 0x4A},
	{0x4A, 0x01},
	{0xB5, 0x0C},
	{0xA1, 0x0F},
	{0xA3, 0x40},
	{0xB1, 0x00},
	{0x93, 0x00},
	{0x7E, 0x4C},
	{0x50, 0x02},
	{0x49, 0x10},
	{0x8E, 0x40},
	{0x7F, 0x56},
	{0x0C, 0x00},
	{0xBC, 0x11},
	{0x82, 0x00},
	{0x19, 0x20},
	{0x1F, 0x10},
	{0x1B, 0x4F},
	{0x12, 0x00},
};

/**
  * @brief Here we'll try to encapsulate the changes for just the output video format
  */
static const struct regval_list sensor_fmt_raw[] = {
};


/**
  * @brief Store information about the video data format
  */
static struct sensor_format_struct sensor_formats[] = {
	{
		.desc      = "Raw RGB Bayer",
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
		.regs      = sensor_fmt_raw,
		.regs_size = ARRAY_SIZE(sensor_fmt_raw),
		.bpp       = 1,
	},
};
#define N_FMTS ARRAY_SIZE(sensor_formats)

/**
  * @brief Then there is the issue of window sizes. Try to capture the info here
  */
static struct sensor_win_size sensor_win_sizes[] = {
	{
		.width      = 1280,
		.height     = 720,
		.hoffset    = 0,
		.voffset    = 0,
		.hts    	= 1600,
		.vts    	= 1500,//750
		.pclk       = 36 * 1000 * 1000,
		.mipi_bps   = 180 * 1000 * 1000,
		.fps_fixed  = 15,//30
		.bin_factor = 1,
		.intg_min   = 1 << 4,
		.intg_max   = 1110 << 4,
		.gain_min   = 1 << 4,
		.gain_max   = 15 << 4,
		.regs       = sensor_720p15_regs,
		.regs_size  = ARRAY_SIZE(sensor_720p15_regs),
		.set_size   = NULL,
	},
};
#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))


/**
  * @brief H63 sensor private param definition
  */
static int h63_sensor_vts, sensor_dev_id;

/**
  * @}
  */


/* Private Function Prototypes -----------------------------------------------*/
/* Exported Variables --------------------------------------------------------*/
/* Exported Functions --------------------------------------------------------*/
/* Private Functions ---------------------------------------------------------*/
/** @defgroup H63_DVP_Private_Functions H63 DVP Private Functions
  * @brief    H63 DVP Private Functions
  * @{
  */

/** @defgroup H63_DVP_Private_Functions_Group1 H63 Sensor Config Functions
  * @brief    H63 Sensor Config Functions
  * @{
  */

/** @brief  Sensor get exposure
  * @note   Code for dealing with controls, fill with different sensor module
  *         Different sensor module has different settings here, if not support the follow function ,retrun -EINVAL
  * @param  sd v4l2_subdev struct pointer
  * @param  value exposure value pointer to get back
  * @retval 0 get success
  * @retval other get failed
  */
static int sensor_g_exp(struct v4l2_subdev *sd, s32 *value)
{
	struct sensor_info *info = to_state(sd);

	*value = info->exp;
	sensor_dbg("sensor get exposure = %d\n", info->exp);

	return 0;
}

/** @brief  Sensor set exposure
  * @param  sd v4l2_subdev struct pointer
  * @param  exp_val exposure value to set
  * @retval 0 set success
  * @retval other set failed
  */
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	struct sensor_info *info = to_state(sd);

	info->exp = exp_val;
	sensor_write(sd, H63_REG_EXP_HIGH, ((exp_val / 16) >> 8) & 0xFF);
	sensor_write(sd, H63_REG_EXP_LOW, ((exp_val / 16)) & 0xFF);

	return 0;
}

/** @brief  Sensor get gain
  * @param  sd v4l2_subdev struct pointer
  * @param  value gain value pointer to get back
  * @retval 0 get success
  * @retval other get failed
  */
static int sensor_g_gain(struct v4l2_subdev *sd, s32 *value)
{
	struct sensor_info *info = to_state(sd);

	*value = info->gain;
	sensor_dbg("sensor get gain = %d\n", info->gain);

	return 0;
}

/** @brief  Set Sensor gain
  * @param  sd v4l2_subdev struct pointer
  * @param  gain gain to set
  * @retval 0 set success
  * @retval other set failed
  */
static int setSensorGain(struct v4l2_subdev *sd, int gain)
{
	int again = 0, tmp = 0;
	unsigned char regdata = 0;

	//gain: 16=1X, 32=2X, 48=3X, 64=4X, ......, 240=15X, 256=16X, ......
	again = gain;

	while (again > 31) {
		again >>= 1;
		tmp++;
	}

	if (again > 15) {
		again -= 16;
	}

	regdata = (unsigned char)((tmp << 4) | again);
	sensor_write(sd, H63_REG_GAIN, regdata & 0xFF);

	return 0;
}

/** @brief  Sensor set gain
  * @param  sd v4l2_subdev struct pointer
  * @param  gain_val gain value to set
  * @retval 0 set success
  * @retval other set failed
  */
static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);

	if (gain_val != info->gain) {
		//sensor_dbg("gain_val:%d\n", gain_val);
		setSensorGain(sd, gain_val);
		info->gain = gain_val;
	}

	return 0;
}

/** @brief  Sensor set exposure and gain
  * @param  sd v4l2_subdev struct pointer
  * @param  exp_gain sensor_exp_gain struct pointer
  * @retval 0 set success
  * @retval other set failed
  */
static int sensor_s_exp_gain(struct v4l2_subdev *sd, struct sensor_exp_gain *exp_gain)
{
	int exp_val, gain_val, shutter = 0, frame_length = 0;
	struct sensor_info *info = to_state(sd);

	exp_val  = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;

	if (gain_val < 1 * 16) {
		gain_val = 16;
	}

	if (exp_val > 0xfffff) {
		exp_val = 0xfffff;
	}

	shutter = exp_val >> 4;

	if (shutter > h63_sensor_vts - 4) {
		frame_length = shutter + 4;
	} else {
		frame_length = h63_sensor_vts;
	}

	//Write vts
	sensor_write(sd, H63_REG_FrameH_LOW,  frame_length & 0xff);
	sensor_write(sd, H63_REG_FrameH_HIGH, frame_length >> 8);

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

	info->exp  = exp_val;
	info->gain = gain_val;

	return 0;
}

/** @brief  Sensor set software standby
  * @param  sd v4l2_subdev struct pointer
  * @param  on_off software standby swtich
  * @retval 0 set success
  * @retval other set failed
  */
static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	return 0;
}

/** @brief  Sensor get format mbus core
  * @param  sd v4l2_subdev struct pointer
  * @param  code code value pointer to get back
  * @retval 0 get success
  * @retval other get failed
  */
static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	data_type get_value;
	struct sensor_info *info = to_state(sd);

	sensor_read(sd, H63_REG_SYS, &get_value);

	switch (get_value) {
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

/** @brief  Sensor set horizontal flip
  * @param  sd v4l2_subdev struct pointer
  * @param  enable horizontal flip enable/disable
  * @retval 0 set success
  * @retval other set failed
  */
static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type reg_value;

	if (!(enable == 0 || enable == 1)) {
		return -1;
	}

#ifdef SOI_MIRROR_FLIP
	sensor_read(sd, H63_REG_SYS, &reg_value);

	if (1 == enable) {
		reg_value |= BIT(5);
	} else {
		reg_value &= ~BIT(5);
	}

	sensor_write(sd, H63_REG_SYS, reg_value);
#endif

	return 0;
}

/** @brief  Sensor set vertical flip
  * @param  sd v4l2_subdev struct pointer
  * @param  enable horizontal vertical enable/disable
  * @retval 0 set success
  * @retval other set failed
  */
static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type reg_value;

	if (!(enable == 0 || enable == 1)) {
		return -1;
	}

#ifdef SOI_MIRROR_FLIP
	sensor_read(sd, H63_REG_SYS, &reg_value);

	if (1 == enable) {
		reg_value |= BIT(4);
	} else {
		reg_value &= ~BIT(4);
	}

	sensor_write(sd, H63_REG_SYS, reg_value);
#endif

	return 0;
}

/**
  * @}
  */


/** @defgroup H63_DVP_Private_Functions_Group2 H63 V4l2 Subdev Core Operations Functions
  * @brief    H63 V4l2 Subdev Core Operations Functions
  * @{
  */

/** @brief  Sensor power config
  * @param  sd v4l2_subdev struct pointer
  * @param  on power config
  * @retval 0 success
  * @retval others failed
  */
static int sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;

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
		vin_set_mclk_freq(sd, MCLK_FREQ);
		vin_set_mclk(sd, ON);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		usleep_range(10000, 12000);
		break;

	case PWR_ON:
		sensor_dbg("PWR_ON!\n");
		cci_lock(sd);
		vin_gpio_set_status(sd, PWDN, 1);
		vin_gpio_set_status(sd, RESET, 1);
		vin_gpio_set_status(sd, POWER_EN, 1);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);	// Pull up PWDN pin initially
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);	// Pull down RESET# pin initially
		usleep_range(1000, 1200);
#if 0//sk
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_HIGH);
#endif
		//vin_set_pmu_channel(sd, CMBCSI, ON);
		vin_set_pmu_channel(sd, AVDD, ON);		  // Turn on AVDD
		usleep_range(100, 120);
		vin_set_pmu_channel(sd, DVDD, ON);		  // DVDD is controlled internally
		usleep_range(1000, 1200);
		vin_set_pmu_channel(sd, IOVDD, ON);		 // Turn on DOVDD
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);   // Pull up RESET# pin
		vin_set_mclk(sd, ON);
		usleep_range(1000, 1200);
		vin_set_mclk_freq(sd, MCLK_FREQ);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		usleep_range(1000, 1200);
		vin_gpio_write(sd, PWDN, CSI_GPIO_LOW);
		usleep_range(10000, 12000);
		cci_unlock(sd);
		break;

	case PWR_OFF:
		sensor_dbg("PWR_OFF!do nothing\n");
		break;
#if 0
		cci_lock(sd);
		usleep_range(10000, 12000);				 // > 512 MCLK cycles
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);	// Pull up PWDN pin
		usleep_range(1000, 1200);				   // Add delay
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);	// Pull down RESET# pin
		usleep_range(1000, 1200);				   // Add delay

		vin_set_mclk(sd, OFF);					  // Disable MCLK
		vin_gpio_write(sd, POWER_EN, CSI_GPIO_LOW);

		usleep_range(1000, 1200);				   // Add delay
		vin_set_pmu_channel(sd, IOVDD, OFF);		// Turn off DOVDD
		vin_set_pmu_channel(sd, DVDD, OFF);

		vin_set_pmu_channel(sd, AVDD, OFF);		 // Turn off AVDD
		usleep_range(10000, 12000);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		vin_gpio_set_status(sd, RESET, 0);
		vin_gpio_set_status(sd, PWDN, 0);
		cci_unlock(sd);
		break;
#endif

	default:
		return -EINVAL;
	}

	return 0;
}

/** @brief  Sensor reset config
  * @param  sd v4l2_subdev struct pointer
  * @param  val reset config value
  * @retval 0 success
  * @retval others failed
  */
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

/** @brief  Sensor detect
  * @param  sd v4l2_subdev struct pointer
  * @retval 0 detect success
  * @retval other detect failed
  */
static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rd_val;
	int eRet, times_out = 3;

	//Try to read ID register up to 3 times
	do {
		eRet = sensor_read(sd, H63_REG_PIDH, &rd_val);
		printk("eRet:%d, H63_ID_VAL_HIGH:0x%x, times_out:%d\n", eRet, rd_val, times_out);
		usleep_range(200000, 220000);
		times_out--;
	} while (eRet < 0 && times_out > 0);

	//Read PIDH register and check
	sensor_read(sd, H63_REG_PIDH, &rd_val);
	printk("H63_ID_VAL_HIGH = 0x%02x, Done!\n", rd_val);

	if (rd_val != H63_ID_VAL_HIGH) {
		return -ENODEV;
	}

	//Read PIDL register and check
	sensor_read(sd, H63_REG_PIDL, &rd_val);
	printk("H63_ID_VAL_LOW = 0x%02x, Done!\n", rd_val);

	if (rd_val != H63_ID_VAL_LOW) {
		return -ENODEV;
	}

	printk("H63 sensor detect success.\n");

	return 0;
}

/** @brief  Sensor init
  * @param  sd v4l2_subdev struct pointer
  * @param  val init value config
  * @retval 0 init success
  * @retval other init failed
  */
static int sensor_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	struct sensor_info *info = to_state(sd);

	printk("**********************************\r\n");
	printk("*********h63 sensor init**********\r\n");
	printk("**********************************\r\n");
	sensor_dbg("sensor init\n");

	//Make sure it is a target sensor
	ret = sensor_detect(sd);

	if (ret) {
		sensor_err("chip found is not an target chip.\n");
		return ret;
	}

	info->focus_status = 0;
	info->low_speed = 0;
	info->width = 1280;
	info->height = 720;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 20; //30fps
	info->preview_first_flag = 1;

	return 0;
}

/** @brief  Sensor ioctl
  * @param  sd v4l2_subdev struct pointer
  * @param  cmd command
  * @param  arg argument
  * @retval 0 success
  * @retval others failed
  */
static long sensor_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_info *info = to_state(sd);

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

	case VIDIOC_VIN_SENSOR_EXP_GAIN:
		sensor_s_exp_gain(sd, (struct sensor_exp_gain *)arg);
		break;

	case VIDIOC_VIN_SENSOR_CFG_REQ:
		sensor_cfg_req(sd, (struct sensor_config *)arg);
		break;
#if 0

	case VIDIOC_VIN_SENSOR_SET_FPS:
		ret = sensor_s_fps(sd, (struct sensor_fps *)arg);
		break;

	case VIDIOC_VIN_GET_SENSOR_CODE:
		sensor_get_fmt_mbus_core(sd, (int *)arg);
		break;
#endif

	default:
		return -EINVAL;
	}

	return ret;
}

/**
  * @}
  */


/** @defgroup H63_DVP_Private_Functions_Group3 H63 V4l2 Control Operations Functions
  * @brief    H63 V4l2 Control Operations Functions
  * @{
  */

#if 0
/** @brief  Sensor query control
  * @param  sd v4l2_subdev struct pointer
  * @param  qc v4l2 query control pointer
  * @retval 0 success
  * @retval others failed
  */
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

/** @brief  Sensor get control
  * @param  ctrl v4l2 control pointer
  * @retval 0 success
  * @retval others failed
  */
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

/** @brief  Sensor set control
  * @param  ctrl v4l2 control pointer
  * @retval 0 success
  * @retval others failed
  */
static int sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
#if 0
	struct v4l2_queryctrl qc;
	int ret;
#endif

	struct sensor_info *info = container_of(ctrl->handler, struct sensor_info, handler);
	struct v4l2_subdev *sd = &info->sd;

#if 0
	qc.id = ctrl->id;
	ret = sensor_queryctrl(sd, &qc);

	if (ret < 0) {
		return ret;
	}

	if (ctrl->val < qc.minimum || ctrl->val > qc.maximum) {
		return -ERANGE;
	}

#endif

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

/**
  * @}
  */


/** @defgroup H63_DVP_Private_Functions_Group4 H63 V4l2 Subdev Video Operations Functions
  * @brief    H63 V4l2 Subdev Video Operations Functions
  * @{
  */

/** @brief  Sensor get media bus config
  * @param  sd v4l2_subdev struct pointer
  * @param  cfg v4l2 mbus config pointer
  * @retval 0 success
  * @retval others failed
  */
static int sensor_g_mbus_config(struct v4l2_subdev *sd, struct v4l2_mbus_config *cfg)
{
	struct sensor_info *info = to_state(sd);

	cfg->type = V4L2_MBUS_PARALLEL;
	cfg->flags = V4L2_MBUS_MASTER | H63_VREF_POL | H63_HREF_POL | H63_PCLK_POL;

	return 0;
}

/** @brief  Sensor register init
  * @param  info sensor infomation pointer
  * @retval 0 success
  * @retval others failed
  */
static int sensor_reg_init(struct sensor_info *info)
{
	int ret;
	struct v4l2_subdev *sd = &info->sd;
	struct sensor_format_struct *sensor_fmt = info->fmt;
	struct sensor_win_size *wsize = info->current_wins;

	//Write sensor default register
	sensor_dbg("sensor reg init, ARRAY_SIZE(sensor_default_regs)=%d\n", ARRAY_SIZE(sensor_default_regs));
	ret = sensor_write_array(sd, sensor_default_regs, ARRAY_SIZE(sensor_default_regs));

	if (ret < 0) {
		sensor_err("write sensor_default_regs error\n");
		return ret;
	}

	//Write sensor format register
	sensor_write_array(sd, sensor_fmt->regs, sensor_fmt->regs_size);

	//Write sensor win size register
	sensor_dbg("sensor reg init, wsize=%p, wsize->regs=0x%x, wsize->regs_size=%d\n", wsize, wsize->regs, wsize->regs_size);

	if (wsize->regs) {
		sensor_dbg("%s: start sensor_write_array(wsize->regs)\n", __func__);
		sensor_write_array(sd, wsize->regs, wsize->regs_size);
	}

	if (wsize->set_size) {
		wsize->set_size(sd);
	}

	info->width = wsize->width;
	info->height = wsize->height;
	h63_sensor_vts = wsize->vts;

	sensor_dbg("s_fmt set width = %d, height = %d\n", wsize->width, wsize->height);

	return 0;
}

/** @brief  Sensor start/stop vedio stream
  * @param  sd v4l2_subdev struct pointer
  * @param  enable vedio stream is enable/disable
  * @retval 0 success
  * @retval others failed
  */
static int sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct sensor_info *info = to_state(sd);

	sensor_dbg("%s on = %d, %d*%d fps: %d code: %x\n", __func__, enable,
			   info->current_wins->width, info->current_wins->height,
			   info->current_wins->fps_fixed, info->fmt->mbus_code);

	if (!enable) {
		return 0;
	}

	return sensor_reg_init(info);
}

/**
  * @}
  */


/** @defgroup H63_DVP_Private_Functions_Group5 H63 Sensor V4l2 Operation Struct Definition
  * @brief    H63 Sensor V4l2 Operation Struct Definition
  * @{
  */

/**
  * @brief sensor v4l2 ctrl ops definition
  */
static const struct v4l2_ctrl_ops sensor_ctrl_ops = {
	.g_volatile_ctrl = sensor_g_ctrl,
	.s_ctrl          = sensor_s_ctrl,
	//.queryctrl     = sensor_queryctrl,
};

/**
  * @brief sensor v4l2 subdev core ops definition
  */
static const struct v4l2_subdev_core_ops sensor_core_ops = {
	.reset   = sensor_reset,
	.init    = sensor_init,
	.s_power = sensor_power,
	.ioctl   = sensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = sensor_compat_ioctl32,
#endif
};

/**
  * @brief sensor v4l2 subdev video ops definition
  */
static const struct v4l2_subdev_video_ops sensor_video_ops = {
	.s_parm   = sensor_s_parm,
	.g_parm   = sensor_g_parm,
	.s_stream = sensor_s_stream,
	.g_mbus_config = sensor_g_mbus_config,
};

/**
  * @brief sensor v4l2 subdev pad ops definition
  */
static const struct v4l2_subdev_pad_ops sensor_pad_ops = {
	.enum_mbus_code  = sensor_enum_mbus_code,
	.enum_frame_size = sensor_enum_frame_size,
	.get_fmt = sensor_get_fmt,
	.set_fmt = sensor_set_fmt,
};

/**
  * @brief sensor v4l2 subdev ops definition
  */
static const struct v4l2_subdev_ops sensor_ops = {
	.core  = &sensor_core_ops,
	.video = &sensor_video_ops,
	.pad   = &sensor_pad_ops,
};

/**
  * @}
  */


/** @defgroup H63_DVP_Private_Functions_Group6 H63 Linux Device Driver Model Funciotns
  * @brief    H63 Linux Device Driver Model Funciotns
  * @{
  */

/**
  * @brief sensor cci driver definition
  */
static struct cci_driver cci_drv[] = {
	{
		.name = SENSOR_NAME,
		.addr_width = CCI_BITS_8,
		.data_width = CCI_BITS_8,
	},

	{
		.name = SENSOR_NAME_2,
		.addr_width = CCI_BITS_8,
		.data_width = CCI_BITS_8,
	},
};

/** @brief  Sensor init controls
  * @param  sd v4l2_subdev struct pointer
  * @param  ops v4l2 ctrl ops pointer
  * @retval 0 success
  * @retval others failed
  */
static int sensor_init_controls(struct v4l2_subdev *sd, const struct v4l2_ctrl_ops *ops)
{
	int ret = 0;
	struct v4l2_ctrl *ctrl;
	struct sensor_info *info = to_state(sd);
	struct v4l2_ctrl_handler *handler = &info->handler;

	v4l2_ctrl_handler_init(handler, 4);

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600, 256 * 1600, 1, 1 * 1600);

	if (ctrl != NULL) {
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 1, 65536 * 16, 1, 1);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_HFLIP, 0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_VFLIP, 0, 1, 1, 0);

	if (ctrl != NULL) {
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}

	if (handler->error) {
		ret = handler->error;
		v4l2_ctrl_handler_free(handler);
	}

	sd->ctrl_handler = handler;

	return ret;
}

/** @brief  Sensor probe
  * @param  client i2c client pointer
  * @param  id i2c device id pointer
  * @retval 0 success
  * @retval others failed
  */
static int sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i;
	struct v4l2_subdev *sd;
	struct sensor_info *info;

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);

	if (info == NULL) {
		return -ENOMEM;
	}

	sd = &info->sd;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name)) {
				break;
			}
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
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET3 | MIPI_NORMAL_MODE;  //CMB_PHYA_OFFSET2 | MIPI_NORMAL_MODE
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	info->exp = 0;
	info->gain = 0;

	return 0;
}

/** @brief  Sensor remove
  * @param  client i2c client pointer
  * @retval 0 success
  * @retval others failed
  */
static int sensor_remove(struct i2c_client *client)
{
	int i;
	struct v4l2_subdev *sd;

	if (client) {
		for (i = 0; i < SENSOR_NUM; i++) {
			if (!strcmp(cci_drv[i].name, client->name)) {
				break;
			}
		}

		sd = cci_dev_remove_helper(client, &cci_drv[i]);
	} else {
		sd = cci_dev_remove_helper(client, &cci_drv[sensor_dev_id++]);
	}

	kfree(to_state(sd));

	return 0;
}

/**
  * @brief sensor i2c device id definition
  */
static const struct i2c_device_id sensor_id[] = {
	{SENSOR_NAME, 0},
	{}
};

/**
  * @brief sensor i2c device id 2 definition
  */
static const struct i2c_device_id sensor_id_2[] = {
	{SENSOR_NAME_2, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sensor_id);
MODULE_DEVICE_TABLE(i2c, sensor_id_2);


/**
  * @brief sensor i2c driver definition
  */
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

/** @brief  Init Sensor
  * @param  None
  * @retval 0 success
  * @retval others failed
  */
static __init int init_sensor(void)
{
	int i, ret = 0;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++) {
		ret = cci_dev_init_helper(&sensor_driver[i]);
	}

	return ret;
}

/** @brief  Exit Sensor
  * @param  None
  * @retval 0 success
  * @retval others failed
  */
static __exit void exit_sensor(void)
{
	int i;

	sensor_dev_id = 0;

	for (i = 0; i < SENSOR_NUM; i++) {
		cci_dev_exit_helper(&sensor_driver[i]);
	}
}

module_init(init_sensor);
module_exit(exit_sensor);

/**
  * @}
  */

/**
  * @}
  */


/************************* (C) COPYRIGHT Allwinner *****END OF FILE***********/

