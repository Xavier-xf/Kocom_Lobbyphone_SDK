/*
 * A V4L2 driver for imx662 Raw cameras.
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

MODULE_AUTHOR("myf");
MODULE_DESCRIPTION("A low-level driver for IMX662 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (24*1000*1000)
#define V4L2_IDENT_SENSOR 0x0662

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

/*
 * The IMX662 i2c address
 */
#define I2C_ADDR 0x34

#define SENSOR_NUM 0x1
#define SENSOR_NAME "imx662_mipi"
#define SENSOR_NAME_2 "imx662_mipi_2"

#define DOL_RHS1	81
#define DOL_RATIO	16

/*
*	0: DOL HDR   (sunxi_isp.c  ---->  wdr_cfg.wdr_mode = 0;)
*	1: Clear HDR   (sunxi_isp.c  ---->  wdr_cfg.wdr_mode = 1;)
*/
#define ClearHdr 0
/*
 * The default register settings
 */

static struct regval_list sensor_default_regs[] = {

};

/******************************************************************************
 *
 * IMX662-AAQR All-pixel scan
 * CSI-2_4lane 24MHz
 * AD:12bit Output:12bit
 * 891Mbps
 * Master Mode
 * LCG Mode 30fps
 * Integration Time 33.227ms
 *****************************************************************************/
static struct regval_list sensor_4lane12bit_1080P30_reg[] = {
	{0x3000, 0x01},  // STANDBY
	{0x3001, 0x00},  // REGHOLD
	{0x3002, 0x00},  // master mode
	{0x3014, 0x04},  // INCK_SEL[3:0]
	{0x3015, 0x05},  // DATARATE_SEL[3:0]
	{0x3018, 0x00},  // WINMODE[3:0]
	{0x301A, 0x00},  // WDMODE
	{0x301B, 0x00},  // ADDMODE[1:0]
	{0x301C, 0x00},  // THIN_V_EN
	{0x301E, 0x01},  // VCMODE
	{0x3020, 0x00},  // HREVERSE
	{0x3021, 0x00},  // VREVERSE
	{0x3022, 0x01},  // ADBIT
	{0x3023, 0x01},  // MDBIT
	{0x3028, 0xE2},  // VMAX[19:0]
	{0x3029, 0x04},  // VMAX[19:0]
	{0x302A, 0x00},  // VMAX[19:0]
	{0x302C, 0xBC},  // HMAX[15:0]
	{0x302D, 0x07},  // HMAX[15:0]
	{0x3030, 0x00},  // FDG_SEL0[1:0]
	{0x3031, 0x00},  // FDG_SEL1[1:0]
	{0x3032, 0x00},  // FDG_SEL2[1:0]
	{0x303C, 0x00},  // PIX_HST[12:0]
	{0x303D, 0x00},  // PIX_HST[12:0]
	{0x303E, 0x90},  // PIX_HWIDTH[12:0]
	{0x303F, 0x07},  // PIX_HWIDTH[12:0]
	{0x3040, 0x03},  // LANEMODE[2:0]
	{0x3044, 0x00},  // PIX_VST[11:0]
	{0x3045, 0x00},  // PIX_VST[11:0]
	{0x3046, 0x4C},  // PIX_VWIDTH[11:0]
	{0x3047, 0x04},  // PIX_VWIDTH[11:0]
	{0x3050, 0x04},  // SHR0[19:0]
	{0x3051, 0x00},  // SHR0[19:0]
	{0x3052, 0x00},  // SHR0[19:0]
	{0x3054, 0x0E},  // SHR1[19:0]
	{0x3055, 0x00},  // SHR1[19:0]
	{0x3056, 0x00},  // SHR1[19:0]
	{0x3058, 0x8A},  // SHR2[19:0]
	{0x3059, 0x01},  // SHR2[19:0]
	{0x305A, 0x00},  // SHR2[19:0]
	{0x3060, 0x16},  // RHS1[19:0]
	{0x3061, 0x01},  // RHS1[19:0]
	{0x3062, 0x00},  // RHS1[19:0]
	{0x3064, 0xC4},  // RHS2[19:0]
	{0x3065, 0x0C},  // RHS2[19:0]
	{0x3066, 0x00},  // RHS2[19:0]
	{0x3069, 0x00},  // CHDR_GAIN_EN
	{0x3070, 0x00},  // GAIN[10:0]
	{0x3071, 0x00},  // GAIN[10:0]
	{0x3072, 0x00},  // GAIN_1[10:0]
	{0x3073, 0x00},  // GAIN_1[10:0]
	{0x3074, 0x00},  // GAIN_2[10:0]
	{0x3075, 0x00},  // GAIN_2[10:0]
	{0x3081, 0x00},  // EXP_GAIN
	{0x308C, 0x00},  // CHDR_DGAIN0_HG[15:0]
	{0x308D, 0x01},  // CHDR_DGAIN0_HG[15:0]
	{0x3094, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3095, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3096, 0x00},  // CHDR_AGAIN1[10:0]
	{0x3097, 0x00},  // CHDR_AGAIN1[10:0]
	{0x309C, 0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x309D, 0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x30A4, 0xAA},  // XVSOUTSEL[1:0]
	{0x30A6, 0x00},  // XVS_DRV[1:0]
	{0x30CC, 0x00},  // -
	{0x30CD, 0x00},  // -
	{0x30DC, 0x32},  // BLKLEVEL[11:0]
	{0x30DD, 0x40},  // BLKLEVEL[11:0]
	{0x3400, 0x01},  // GAIN_PGC_FIDMD
	{0x3444, 0xAC},  // -
	{0x3460, 0x21},  // -
	{0x3492, 0x08},  // -
	{0x3A50, 0xFF},  // -
	{0x3A51, 0x03},  // -
	{0x3A52, 0x00},  // -
	{0x3B00, 0x39},  // -
	{0x3B23, 0x2D},  // -
	{0x3B45, 0x04},  // -
	{0x3C0A, 0x1F},  // -
	{0x3C0B, 0x1E},  // -
	{0x3C38, 0x21},  // -
	{0x3C40, 0x06},  // -
	{0x3C44, 0x00},  // -
	{0x3CB6, 0xD8},  // -
	{0x3CC4, 0xDA},  // -
	{0x3E24, 0x79},  // -
	{0x3E2C, 0x15},  // -
	{0x3EDC, 0x2D},  // -
	{0x4498, 0x05},  // -
	{0x449C, 0x19},  // -
	{0x449D, 0x00},  // -
	{0x449E, 0x32},  // -
	{0x449F, 0x01},  // -
	{0x44A0, 0x92},  // -
	{0x44A2, 0x91},  // -
	{0x44A4, 0x8C},  // -
	{0x44A6, 0x87},  // -
	{0x44A8, 0x82},  // -
	{0x44AA, 0x78},  // -
	{0x44AC, 0x6E},  // -
	{0x44AE, 0x69},  // -
	{0x44B0, 0x92},  // -
	{0x44B2, 0x91},  // -
	{0x44B4, 0x8C},  // -
	{0x44B6, 0x87},  // -
	{0x44B8, 0x82},  // -
	{0x44BA, 0x78},  // -
	{0x44BC, 0x6E},  // -
	{0x44BE, 0x69},  // -
	{0x44C0, 0x7F},  // -
	{0x44C1, 0x01},  // -
	{0x44C2, 0x7F},  // -
	{0x44C3, 0x01},  // -
	{0x44C4, 0x7A},  // -
	{0x44C5, 0x01},  // -
	{0x44C6, 0x7A},  // -
	{0x44C7, 0x01},  // -
	{0x44C8, 0x70},  // -
	{0x44C9, 0x01},  // -
	{0x44CA, 0x6B},  // -
	{0x44CB, 0x01},  // -
	{0x44CC, 0x6B},  // -
	{0x44CD, 0x01},  // -
	{0x44CE, 0x5C},  // -
	{0x44CF, 0x01},  // -
	{0x44D0, 0x7F},  // -
	{0x44D1, 0x01},  // -
	{0x44D2, 0x7F},  // -
	{0x44D3, 0x01},  // -
	{0x44D4, 0x7A},  // -
	{0x44D5, 0x01},  // -
	{0x44D6, 0x7A},  // -
	{0x44D7, 0x01},  // -
	{0x44D8, 0x70},  // -
	{0x44D9, 0x01},  // -
	{0x44DA, 0x6B},  // -
	{0x44DB, 0x01},  // -
	{0x44DC, 0x6B},  // -
	{0x44DD, 0x01},  // -
	{0x44DE, 0x5C},  // -
	{0x44DF, 0x01},  // -
	{0x4534, 0x1C},  // -
	{0x4535, 0x03},  // -
	{0x4538, 0x1C},  // -
	{0x4539, 0x1C},  // -
	{0x453A, 0x1C},  // -
	{0x453B, 0x1C},  // -
	{0x453C, 0x1C},  // -
	{0x453D, 0x1C},  // -
	{0x453E, 0x1C},  // -
	{0x453F, 0x1C},  // -
	{0x4540, 0x1C},  // -
	{0x4541, 0x03},  // -
	{0x4542, 0x03},  // -
	{0x4543, 0x03},  // -
	{0x4544, 0x03},  // -
	{0x4545, 0x03},  // -
	{0x4546, 0x03},  // -
	{0x4547, 0x03},  // -
	{0x4548, 0x03},  // -
	{0x4549, 0x03},  // -

	{0x3000, 0x00}, //Standby 1: Standby Start (Sleep) 0: Standby cancel
};

#if ClearHdr
/******************************************************************************
 *
 * IMX662-AAQR All-pixel scan
 * CSI-2_4lane 24MHz
 * AD:12bit Output:12bit
 * 891Mbps Master Mode
 * ClearHDR VC 30fps
 * Integration Time 33.227ms
 * Direct gain(LG):dB Direct gain(HG):12dB
 *****************************************************************************/
static struct regval_list sensor_clearhdr_4lane12bit_1080P30_reg[] = {
	{0x3000, 0x01},  // STANDBY
	{0x3001, 0x00},  // REGHOLD
	{0x3002, 0x00},  //master mode
	{0x3014, 0x04},  // INCK_SEL[3:0]
	{0x3015, 0x05},  // DATARATE_SEL[3:0]
	{0x3018, 0x00},  // WINMODE[3:0]
	{0x301A, 0x08},  // WDMODE
	{0x301B, 0x00},  // ADDMODE[1:0]
	{0x301C, 0x00},  // THIN_V_EN
	{0x301E, 0x01},  // VCMODE
	{0x3020, 0x00},  // HREVERSE
	{0x3021, 0x00},  // VREVERSE
	{0x3022, 0x01},  // ADBIT
	{0x3023, 0x01},  // MDBIT
	{0x3028, 0xC4},  // VMAX[19:0]
	{0x3029, 0x09},  // VMAX[19:0]
	{0x302A, 0x00},  // VMAX[19:0]
	{0x302C, 0xDE},  // HMAX[15:0]
	{0x302D, 0x03},  // HMAX[15:0]
	{0x3030, 0x02},  // FDG_SEL0[1:0]
	{0x3031, 0x00},  // FDG_SEL1[1:0]
	{0x3032, 0x00},  // FDG_SEL2[1:0]
	{0x303C, 0x00},  // PIX_HST[12:0]
	{0x303D, 0x00},  // PIX_HST[12:0]
	{0x303E, 0x90},  // PIX_HWIDTH[12:0]
	{0x303F, 0x07},  // PIX_HWIDTH[12:0]
	{0x3040, 0x03},  // LANEMODE[2:0]
	{0x3044, 0x00},  // PIX_VST[11:0]
	{0x3045, 0x00},  // PIX_VST[11:0]
	{0x3046, 0x4C},  // PIX_VWIDTH[11:0]
	{0x3047, 0x04},  // PIX_VWIDTH[11:0]
	{0x3050, 0x08},  // SHR0[19:0]
	{0x3051, 0x00},  // SHR0[19:0]
	{0x3052, 0x00},  // SHR0[19:0]
	{0x3054, 0x0E},  // SHR1[19:0]
	{0x3055, 0x00},  // SHR1[19:0]
	{0x3056, 0x00},  // SHR1[19:0]
	{0x3058, 0x8A},  // SHR2[19:0]
	{0x3059, 0x01},  // SHR2[19:0]
	{0x305A, 0x00},  // SHR2[19:0]
	{0x3060, 0x16},  // RHS1[19:0]
	{0x3061, 0x01},  // RHS1[19:0]
	{0x3062, 0x00},  // RHS1[19:0]
	{0x3064, 0xC4},  // RHS2[19:0]
	{0x3065, 0x0C},  // RHS2[19:0]
	{0x3066, 0x00},  // RHS2[19:0]
	{0x3069, 0x01},  // CHDR_GAIN_EN
	{0x3070, 0x00},  // GAIN[10:0]
	{0x3071, 0x00},  // GAIN[10:0]
	{0x3072, 0x00},  // GAIN_1[10:0]
	{0x3073, 0x00},  // GAIN_1[10:0]
	{0x3074, 0x00},  // GAIN_2[10:0]
	{0x3075, 0x00},  // GAIN_2[10:0]
	{0x3081, 0x00},  // EXP_GAIN
	{0x308C, 0x00},  // CHDR_DGAIN0_HG[15:0]
	{0x308D, 0x01},  // CHDR_DGAIN0_HG[15:0]
	{0x3094, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3095, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3096, 0x00},  // CHDR_AGAIN1[10:0]
	{0x3097, 0x00},  // CHDR_AGAIN1[10:0]
	{0x309C, 0xFE},  // CHDR_AGAIN0_HG[10:0]
	{0x309D, 0x05},  // CHDR_AGAIN0_HG[10:0]
	{0x30A4, 0xAA},  // XVSOUTSEL[1:0]
	{0x30A6, 0x00},  // XVS_DRV[1:0]
	{0x30CC, 0x00},  // -
	{0x30CD, 0x00},  // -
	{0x30DC, 0x32},  // BLKLEVEL[11:0]
	{0x30DD, 0x40},  // BLKLEVEL[11:0]
	{0x3400, 0x01},  // GAIN_PGC_FIDMD
	{0x3444, 0xAC},  // -
	{0x3460, 0x22},  // -
	{0x3492, 0x08},  // -
	{0x3A50, 0xFF},  // -
	{0x3A51, 0x03},  // -
	{0x3A52, 0x00},  // -
	{0x3B00, 0x39},  // -
	{0x3B23, 0x2D},  // -
	{0x3B45, 0x04},  // -
	{0x3C0A, 0x1F},  // -
	{0x3C0B, 0x1E},  // -
	{0x3C38, 0x21},  // -
	{0x3C40, 0x05},  // -
	{0x3C44, 0x00},  // -
	{0x3CB6, 0xD8},  // -
	{0x3CC4, 0xDA},  // -
	{0x3E24, 0x79},  // -
	{0x3E2C, 0x15},  // -
	{0x3EDC, 0x2D},  // -
	{0x4498, 0x05},  // -
	{0x449C, 0x19},  // -
	{0x449D, 0x00},  // -
	{0x449E, 0x32},  // -
	{0x449F, 0x01},  // -
	{0x44A0, 0x92},  // -
	{0x44A2, 0x91},  // -
	{0x44A4, 0x8C},  // -
	{0x44A6, 0x87},  // -
	{0x44A8, 0x82},  // -
	{0x44AA, 0x78},  // -
	{0x44AC, 0x6E},  // -
	{0x44AE, 0x69},  // -
	{0x44B0, 0x92},  // -
	{0x44B2, 0x91},  // -
	{0x44B4, 0x8C},  // -
	{0x44B6, 0x87},  // -
	{0x44B8, 0x82},  // -
	{0x44BA, 0x78},  // -
	{0x44BC, 0x6E},  // -
	{0x44BE, 0x69},  // -
	{0x44C0, 0x7F},  // -
	{0x44C1, 0x01},  // -
	{0x44C2, 0x7F},  // -
	{0x44C3, 0x01},  // -
	{0x44C4, 0x7A},  // -
	{0x44C5, 0x01},  // -
	{0x44C6, 0x7A},  // -
	{0x44C7, 0x01},  // -
	{0x44C8, 0x70},  // -
	{0x44C9, 0x01},  // -
	{0x44CA, 0x6B},  // -
	{0x44CB, 0x01},  // -
	{0x44CC, 0x6B},  // -
	{0x44CD, 0x01},  // -
	{0x44CE, 0x5C},  // -
	{0x44CF, 0x01},  // -
	{0x44D0, 0x7F},  // -
	{0x44D1, 0x01},  // -
	{0x44D2, 0x7F},  // -
	{0x44D3, 0x01},  // -
	{0x44D4, 0x7A},  // -
	{0x44D5, 0x01},  // -
	{0x44D6, 0x7A},  // -
	{0x44D7, 0x01},  // -
	{0x44D8, 0x70},  // -
	{0x44D9, 0x01},  // -
	{0x44DA, 0x6B},  // -
	{0x44DB, 0x01},  // -
	{0x44DC, 0x6B},  // -
	{0x44DD, 0x01},  // -
	{0x44DE, 0x5C},  // -
	{0x44DF, 0x01},  // -
	{0x4534, 0x1C},  // -
	{0x4535, 0x03},  // -
	{0x4538, 0x1C},  // -
	{0x4539, 0x1C},  // -
	{0x453A, 0x1C},  // -
	{0x453B, 0x1C},  // -
	{0x453C, 0x1C},  // -
	{0x453D, 0x1C},  // -
	{0x453E, 0x1C},  // -
	{0x453F, 0x1C},  // -
	{0x4540, 0x1C},  // -
	{0x4541, 0x03},  // -
	{0x4542, 0x03},  // -
	{0x4543, 0x03},  // -
	{0x4544, 0x03},  // -
	{0x4545, 0x03},  // -
	{0x4546, 0x03},  // -
	{0x4547, 0x03},  // -
	{0x4548, 0x03},  // -
	{0x4549, 0x03},  // -

	{0x3000, 0x00}, //Standby 1: Standby Start (Sleep) 0: Standby cancel
};

#else
/******************************************************************************
 *
 * IMX662-AAQR All-pixel scan
 * CSI-2_4lane 24MHz
 * AD:12bit Output:12bit
 * 891Mbps
 * Master Mode
 * LCG Mode DOL HDR 2frame VC 30fps
 * Integration Time LEF:16ms SEF:1.013ms
 *****************************************************************************/
static struct regval_list sensor_dol_4lane12bit_1080P30_reg[] = {
	{0x3000, 0x01},  // STANDBY
	{0x3001, 0x00},  // REGHOLD
	{0x3002, 0x00},  // master mode
	{0x3014, 0x04},  // INCK_SEL[3:0]
	{0x3015, 0x05},  // DATARATE_SEL[3:0]
	{0x3018, 0x00},  // WINMODE[3:0]
	{0x301A, 0x01},  // WDMODE
	{0x301B, 0x00},  // ADDMODE[1:0]
	{0x301C, 0x01},  // THIN_V_EN
	{0x301E, 0x01},  // VCMODE
	{0x3020, 0x00},  // HREVERSE
	{0x3021, 0x00},  // VREVERSE
	{0x3022, 0x01},  // ADBIT
	{0x3023, 0x01},  // MDBIT
	{0x3028, 0xE2},  // VMAX[19:0]
	{0x3029, 0x04},  // VMAX[19:0]
	{0x302A, 0x00},  // VMAX[19:0]
	{0x302C, 0xDE},  // HMAX[15:0]
	{0x302D, 0x03},  // HMAX[15:0]
	{0x3030, 0x00},  // FDG_SEL0[1:0]
	{0x3031, 0x00},  // FDG_SEL1[1:0]
	{0x3032, 0x00},  // FDG_SEL2[1:0]
	{0x303C, 0x00},  // PIX_HST[12:0]
	{0x303D, 0x00},  // PIX_HST[12:0]
	{0x303E, 0x90},  // PIX_HWIDTH[12:0]
	{0x303F, 0x07},  // PIX_HWIDTH[12:0]
	{0x3040, 0x03},  // LANEMODE[2:0]
	{0x3044, 0x00},  // PIX_VST[11:0]
	{0x3045, 0x00},  // PIX_VST[11:0]
	{0x3046, 0x4C},  // PIX_VWIDTH[11:0]
	{0x3047, 0x04},  // PIX_VWIDTH[11:0]
	{0x3050, 0x14},  // SHR0[19:0]
	{0x3051, 0x05},  // SHR0[19:0]
	{0x3052, 0x00},  // SHR0[19:0]
	{0x3054, 0x05},  // SHR1[19:0]
	{0x3055, 0x00},  // SHR1[19:0]
	{0x3056, 0x00},  // SHR1[19:0]
	{0x3058, 0x8A},  // SHR2[19:0]
	{0x3059, 0x01},  // SHR2[19:0]
	{0x305A, 0x00},  // SHR2[19:0]
	{0x3060, DOL_RHS1&0xff},  // RHS1[19:0]
	{0x3061, (DOL_RHS1>>8)&0xff},  // RHS1[19:0]
	{0x3062, (DOL_RHS1>>16)&0xff},  // RHS1[19:0]
	{0x3064, 0xC4},  // RHS2[19:0]
	{0x3065, 0x0C},  // RHS2[19:0]
	{0x3066, 0x00},  // RHS2[19:0]
	{0x3069, 0x00},  // CHDR_GAIN_EN
	{0x3070, 0x00},  // GAIN[10:0]
	{0x3071, 0x00},  // GAIN[10:0]
	{0x3072, 0x00},  // GAIN_1[10:0]
	{0x3073, 0x00},  // GAIN_1[10:0]
	{0x3074, 0x00},  // GAIN_2[10:0]
	{0x3075, 0x00},  // GAIN_2[10:0]
	{0x3081, 0x00},  // EXP_GAIN
	{0x308C, 0x00},  // CHDR_DGAIN0_HG[15:0]
	{0x308D, 0x01},  // CHDR_DGAIN0_HG[15:0]
	{0x3094, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3095, 0x00},  // CHDR_AGAIN0_LG[10:0]
	{0x3096, 0x00},  // CHDR_AGAIN1[10:0]
	{0x3097, 0x00},  // CHDR_AGAIN1[10:0]
	{0x309C, 0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x309D, 0x00},  // CHDR_AGAIN0_HG[10:0]
	{0x30A4, 0xAA},  // XVSOUTSEL[1:0]
	{0x30A6, 0x00},  // XVS_DRV[1:0]
	{0x30CC, 0x00},  // -
	{0x30CD, 0x00},  // -
	{0x30DC, 0x32},  // BLKLEVEL[11:0]
	{0x30DD, 0x40},  // BLKLEVEL[11:0]
	{0x3400, 0x00},  // GAIN_PGC_FIDMD
	{0x3444, 0xAC},  // -
	{0x3460, 0x21},  // -
	{0x3492, 0x08},  // -
	{0x3A50, 0xFF},  // -
	{0x3A51, 0x03},  // -
	{0x3A52, 0x00},  // -
	{0x3B00, 0x39},  // -
	{0x3B23, 0x2D},  // -
	{0x3B45, 0x04},  // -
	{0x3C0A, 0x1F},  // -
	{0x3C0B, 0x1E},  // -
	{0x3C38, 0x21},  // -
	{0x3C40, 0x06},  // -
	{0x3C44, 0x00},  // -
	{0x3CB6, 0xD8},  // -
	{0x3CC4, 0xDA},  // -
	{0x3E24, 0x79},  // -
	{0x3E2C, 0x15},  // -
	{0x3EDC, 0x2D},  // -
	{0x4498, 0x05},  // -
	{0x449C, 0x19},  // -
	{0x449D, 0x00},  // -
	{0x449E, 0x32},  // -
	{0x449F, 0x01},  // -
	{0x44A0, 0x92},  // -
	{0x44A2, 0x91},  // -
	{0x44A4, 0x8C},  // -
	{0x44A6, 0x87},  // -
	{0x44A8, 0x82},  // -
	{0x44AA, 0x78},  // -
	{0x44AC, 0x6E},  // -
	{0x44AE, 0x69},  // -
	{0x44B0, 0x92},  // -
	{0x44B2, 0x91},  // -
	{0x44B4, 0x8C},  // -
	{0x44B6, 0x87},  // -
	{0x44B8, 0x82},  // -
	{0x44BA, 0x78},  // -
	{0x44BC, 0x6E},  // -
	{0x44BE, 0x69},  // -
	{0x44C0, 0x7F},  // -
	{0x44C1, 0x01},  // -
	{0x44C2, 0x7F},  // -
	{0x44C3, 0x01},  // -
	{0x44C4, 0x7A},  // -
	{0x44C5, 0x01},  // -
	{0x44C6, 0x7A},  // -
	{0x44C7, 0x01},  // -
	{0x44C8, 0x70},  // -
	{0x44C9, 0x01},  // -
	{0x44CA, 0x6B},  // -
	{0x44CB, 0x01},  // -
	{0x44CC, 0x6B},  // -
	{0x44CD, 0x01},  // -
	{0x44CE, 0x5C},  // -
	{0x44CF, 0x01},  // -
	{0x44D0, 0x7F},  // -
	{0x44D1, 0x01},  // -
	{0x44D2, 0x7F},  // -
	{0x44D3, 0x01},  // -
	{0x44D4, 0x7A},  // -
	{0x44D5, 0x01},  // -
	{0x44D6, 0x7A},  // -
	{0x44D7, 0x01},  // -
	{0x44D8, 0x70},  // -
	{0x44D9, 0x01},  // -
	{0x44DA, 0x6B},  // -
	{0x44DB, 0x01},  // -
	{0x44DC, 0x6B},  // -
	{0x44DD, 0x01},  // -
	{0x44DE, 0x5C},  // -
	{0x44DF, 0x01},  // -
	{0x4534, 0x1C},  // -
	{0x4535, 0x03},  // -
	{0x4538, 0x1C},  // -
	{0x4539, 0x1C},  // -
	{0x453A, 0x1C},  // -
	{0x453B, 0x1C},  // -
	{0x453C, 0x1C},  // -
	{0x453D, 0x1C},  // -
	{0x453E, 0x1C},  // -
	{0x453F, 0x1C},  // -
	{0x4540, 0x1C},  // -
	{0x4541, 0x03},  // -
	{0x4542, 0x03},  // -
	{0x4543, 0x03},  // -
	{0x4544, 0x03},  // -
	{0x4545, 0x03},  // -
	{0x4546, 0x03},  // -
	{0x4547, 0x03},  // -
	{0x4548, 0x03},  // -
	{0x4549, 0x03},  // -

	{0x3000, 0x00}, //Standby 1: Standby Start (Sleep) 0: Standby cancel
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
 * if not support the follow function ,retrun -EINVAL
 */

static int sensor_g_exp(struct v4l2_subdev *sd, __s32 *value)
{
	struct sensor_info *info = to_state(sd);
	*value = info->exp;
	sensor_dbg("sensor_get_exposure = %d\n", info->exp);
	return 0;
}

static int imx662_sensor_vts;
static int imx662_sensor_svr;
static int shutter_delay = 1;
static int shutter_delay_cnt;
static int fps_change_flag;

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	int exptime,  exp_val_m;
	struct sensor_info *info = to_state(sd);

#if ClearHdr
		exptime = imx662_sensor_vts - (exp_val >> 8) - 1;
		if (exptime < 4)
			exptime = 4;
		exphigh = (unsigned char)((0x00f0000 & exptime) >> 16);
		expmid =  (unsigned char)((0x000ff00 & exptime) >> 8);
		explow =  (unsigned char)((0x00000ff & exptime));

		// IMX662 SHR0
		sensor_write(sd, 0x3050, explow);
		sensor_write(sd, 0x3051, expmid);
		sensor_write(sd, 0x3052, exphigh);
		sensor_dbg("sensor_set_exp = %d %d line Done!\n", exp_val, exptime);
#else
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		// LEF
		exptime = (imx662_sensor_vts<<1) - (exp_val>>4);
		if (exptime < DOL_RHS1 + 5) {
			exptime = DOL_RHS1 + 5;
			exp_val = ((imx662_sensor_vts << 1) - exptime) << 4;
		}
		sensor_dbg("long exp_val: %d, exptime: %d\n", exp_val, exptime);

		exphigh	= (unsigned char) ((0x00f0000 & exptime) >> 16);
		expmid	= (unsigned char) ((0x000ff00 & exptime) >> 8);
		explow	= (unsigned char) ((0x00000ff & exptime));

		// IMX662 SHR0
		sensor_write(sd, 0x3050, explow);
		sensor_write(sd, 0x3051, expmid);
		sensor_write(sd, 0x3052, exphigh);
		// SEF
		exp_val_m = exp_val / DOL_RATIO;
		if (exp_val_m < 2 * 16)
			exp_val_m = 2 * 16;
		exptime = DOL_RHS1 - (exp_val_m >> 4);
		if (exptime < 5)
			exptime = 5;

		sensor_dbg("short exp_val: %d, exptime: %d\n", exp_val_m, exptime);

		exphigh	= (unsigned char) ((0x00f0000 & exptime) >> 16);
		expmid	= (unsigned char) ((0x000ff00 & exptime) >> 8);
		explow	= (unsigned char) ((0x00000ff & exptime));

		// IMX662 SHR1
		sensor_write(sd, 0x3054, explow);
		sensor_write(sd, 0x3055, expmid);
		sensor_write(sd, 0x3056, exphigh);
	} else {
		exptime = imx662_sensor_vts - (exp_val >> 4) - 1;
		if (exptime < 4)
			exptime = 4;
		exphigh = (unsigned char)((0x00f0000 & exptime) >> 16);
		expmid =  (unsigned char)((0x000ff00 & exptime) >> 8);
		explow =  (unsigned char)((0x00000ff & exptime));

		// IMX662 SHR0
		sensor_write(sd, 0x3050, explow);
		sensor_write(sd, 0x3051, expmid);
		sensor_write(sd, 0x3052, exphigh);
		sensor_dbg("sensor_set_exp = %d %d line Done!\n", exp_val, exptime);
	}
#endif
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

static unsigned char gain2db[497] = {
	0,   2,   3,	 5,   6,   8,	9,  11,  12,  13,  14,	15,  16,  17,
	18,  19,  20,	21,  22,  23,  23,  24,  25,  26,  27,	27,  28,  29,
	29,  30,  31,	31,  32,  32,  33,  34,  34,  35,  35,	36,  36,  37,
	37,  38,  38,	39,  39,  40,  40,  41,  41,  41,  42,	42,  43,  43,
	44,  44,  44,	45,  45,  45,  46,  46,  47,  47,  47,	48,  48,  48,
	49,  49,  49,	50,  50,  50,  51,  51,  51,  52,  52,	52,  52,  53,
	53,  53,  54,	54,  54,  54,  55,  55,  55,  56,  56,	56,  56,  57,
	57,  57,  57,	58,  58,  58,  58,  59,  59,  59,  59,	60,  60,  60,
	60,  60,  61,	61,  61,  61,  62,  62,  62,  62,  62,	63,  63,  63,
	63,  63,  64,	64,  64,  64,  64,  65,  65,  65,  65,	65,  66,  66,
	66,  66,  66,	66,  67,  67,  67,  67,  67,  68,  68,	68,  68,  68,
	68,  69,  69,	69,  69,  69,  69,  70,  70,  70,  70,	70,  70,  71,
	71,  71,  71,	71,  71,  71,  72,  72,  72,  72,  72,	72,  73,  73,
	73,  73,  73,	73,  73,  74,  74,  74,  74,  74,  74,	74,  75,  75,
	75,  75,  75,	75,  75,  75,  76,  76,  76,  76,  76,	76,  76,  77,
	77,  77,  77,	77,  77,  77,  77,  78,  78,  78,  78,	78,  78,  78,
	78,  79,  79,	79,  79,  79,  79,  79,  79,  79,  80,	80,  80,  80,
	80,  80,  80,	80,  80,  81,  81,  81,  81,  81,  81,	81,  81,  81,
	82,  82,  82,	82,  82,  82,  82,  82,  82,  83,  83,	83,  83,  83,
	83,  83,  83,	83,  83,  84,  84,  84,  84,  84,  84,	84,  84,  84,
	84,  85,  85,	85,  85,  85,  85,  85,  85,  85,  85,	86,  86,  86,
	86,  86,  86,	86,  86,  86,  86,  86,  87,  87,  87,	87,  87,  87,
	87,  87,  87,	87,  87,  88,  88,  88,  88,  88,  88,	88,  88,  88,
	88,  88,  88,	89,  89,  89,  89,  89,  89,  89,  89,	89,  89,  89,
	89,  90,  90,	90,  90,  90,  90,  90,  90,  90,  90,	90,  90,  91,
	91,  91,  91,	91,  91,  91,  91,  91,  91,  91,  91,	91,  92,  92,
	92,  92,  92,	92,  92,  92,  92,  92,  92,  92,  92,	93,  93,  93,
	93,  93,  93,	93,  93,  93,  93,  93,  93,  93,  93,	94,  94,  94,
	94,  94,  94,	94,  94,  94,  94,  94,  94,  94,  94,	95,  95,  95,
	95,  95,  95,	95,  95,  95,  95,  95,  95,  95,  95,	95,  96,  96,
	96,  96,  96,	96,  96,  96,  96,  96,  96,  96,  96,	96,  96,  97,
	97,  97,  97,	97,  97,  97,  97,  97,  97,  97,  97,	97,  97,  97,
	97,  98,  98,	98,  98,  98,  98,  98,  98,  98,  98,	98,  98,  98,
	98,  98,  98,	99,  99,  99,  99,  99,  99,  99,  99,	99,  99,  99,
	99,  99,  99,	99,  99,  99, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 100, 100, 100,
};
static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	int gain_val_l, gain_val_l_dg, gain_reg_hg, gain_reg_hg_dg, gain_reg_lg;
	if (gain_val < 1 * 16)
		gain_val = 16;
#if ClearHdr
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		gain_val_l = gain_val * DOL_RATIO * 10 / 58;

		gain_reg_lg = ((2048 * gain_val * 107 / 100) - 32768) / (gain_val * 107 / 100);
		if (gain_val_l <= 32 * 16) {//30db
			gain_reg_hg = ((2048 * gain_val_l * 107 / 100) - 32768) / (gain_val_l * 107 / 100);
			gain_reg_hg_dg = 0x100;
		} else {
			gain_reg_hg = ((2048 * 512 * 107 / 100) - 32768) / (512 * 107 / 100);
			gain_reg_hg_dg = (gain_val_l << 8) / 512;
		}

		sensor_write(sd, 0x308c, gain_reg_hg_dg & 0xff);
		sensor_write(sd, 0x308d, (gain_reg_hg_dg >> 8) & 0xff);
		sensor_write(sd, 0x3094, gain_reg_lg & 0xff);
		sensor_write(sd, 0x3095, (gain_reg_lg >> 8) & 0xff);
		sensor_write(sd, 0x309c, gain_reg_hg & 0xff);
		sensor_write(sd, 0x309d, (gain_reg_hg >> 8) & 0xff);
		sensor_dbg("sensor_set_gain = %d %d, Done!\n", gain_val_l, gain_val);
	} else {
		if (gain_val < 32 * 16) {
			sensor_write(sd, 0x3070, gain2db[gain_val - 16]);
		} else if (gain_val < 1024 * 16) {
			sensor_write(sd, 0x3070, gain2db[(gain_val>>5) - 16] + 100);
		} else {
			sensor_write(sd, 0x3070, gain2db[(gain_val>>10) - 16] + 200);
		}
		sensor_dbg("sensor_set_gain = %d, Done!\n", gain_val);
	}
#else
	if (gain_val < 32 * 16) {
		sensor_write(sd, 0x3070, gain2db[gain_val - 16]);
	} else if (gain_val < 1024 * 16) {
		sensor_write(sd, 0x3070, gain2db[(gain_val>>5) - 16] + 100);
	} else {
		sensor_write(sd, 0x3070, gain2db[(gain_val>>10) - 16] + 200);
	}
	sensor_dbg("sensor_set_gain = %d, Done!\n", gain_val);
#endif
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
	if (exp_val < 16)
		exp_val = 16;
#if 0
	if (fps_change_flag) {
		if (shutter_delay_cnt == shutter_delay) {
			//imx662 VMAX[0:19]
			sensor_write(sd, 0x3028, imx662_sensor_vts / (imx662_sensor_svr + 1) & 0xFF);
			sensor_write(sd, 0x3029, imx662_sensor_vts / (imx662_sensor_svr + 1) >> 8 & 0xFF);
			sensor_write(sd, 0x302a, imx662_sensor_vts / (imx662_sensor_svr + 1) >> 16);
			sensor_write(sd, 0x3001, 0);
			shutter_delay_cnt = 0;
			fps_change_flag = 0;
		} else
			shutter_delay_cnt++;
	}
#endif

	//sensor_write(sd, 0x3001, 0x01);
	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);
	//sensor_write(sd, 0x3001, 0x00);
	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;
}

static int sensor_s_fps(struct v4l2_subdev *sd,
			struct sensor_fps *fps)
{
#if 0
	data_type rdval1, rdval2, rdval3;
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
	imx662_sensor_vts = wsize->pclk/fps->fps/wsize->hts;
	fps_change_flag = 1;
	sensor_write(sd, 0x3001, 1);

	sensor_read(sd, 0x3028, &rdval1);
	sensor_read(sd, 0x3029, &rdval2);
	sensor_read(sd, 0x302a, &rdval3);

	sensor_dbg("imx662_sensor_svr: %d, vts: %d.\n", imx662_sensor_svr, (rdval1 | (rdval2<<8) | (rdval3<<16)));
#endif
	return 0;
}
static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	data_type rdval;

	ret = sensor_read(sd, 0x3000, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == STBY_ON)
		ret = sensor_write(sd, 0x3000, rdval | 0x01);
	else
		ret = sensor_write(sd, 0x3000, rdval & 0xfe);
	return ret;
}

static int sensor_s_stby(struct v4l2_subdev *sd, int *on_off)
{
	return sensor_s_sw_stby(sd, *on_off);
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
		usleep_range(1000, 1200);
		cci_unlock(sd);
		break;
	case STBY_OFF:
		sensor_dbg("STBY_OFF!\n");
		cci_lock(sd);
		usleep_range(1000, 1200);
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
		usleep_range(2000, 2200);
		vin_set_pmu_channel(sd, AVDD, ON);
		vin_set_pmu_channel(sd, DVDD, ON);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(100, 120);
		vin_set_mclk(sd, ON);
		usleep_range(100, 120);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(3000, 3200);
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
		usleep_range(100, 120);
		break;
	case 1:
		vin_gpio_write(sd, RESET, CSI_GPIO_LOW);
		usleep_range(100, 120);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sensor_detect(struct v4l2_subdev *sd)
{
	data_type rdval = 0;
	sensor_read(sd, 0x3008, &rdval);
	sensor_dbg("%s read value is 0x%x\n", __func__, rdval);
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
	info->width = 2592;
	info->height = 1944;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;

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
	case SET_SENSOR_STANDBY:
		sensor_s_stby(sd, (int *)arg);
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
		//.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		.mbus_code = MEDIA_BUS_FMT_SRGGB12_1X12,
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
#if ClearHdr
	//Clear HDR
	{
		.width = 1920,
		.height = 1080,
		.hoffset = 0,
		.voffset = 0,
		.hts = 990,
		.vts = 2500,
		.pclk = 74250000 * 16,
		.mipi_bps = 891 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		//.lp_mode = SENSOR_LP_DISCONTINUOUS,
		.if_mode = MIPI_VC_WDR_MODE,
		.wdr_mode = ISP_DOL_WDR_MODE,
		.intg_min = 1 << 4,
		.intg_max = (2500 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 16 << 4,
		.regs = sensor_clearhdr_4lane12bit_1080P30_reg,
		.regs_size = ARRAY_SIZE(sensor_clearhdr_4lane12bit_1080P30_reg),
		.set_size = NULL,
		.top_clk = 432000000,
		.isp_clk = 297000000,
	},
#else
	//DOL HDR
	{
		.width = 1920,
		.height = 1080,
		.hoffset = 0,
		.voffset = 0,
		.hts = 990,
		.vts = 1250,
		.pclk = 74250000,
		.mipi_bps = 891 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		//.lp_mode = SENSOR_LP_DISCONTINUOUS,
		.if_mode = MIPI_VC_WDR_MODE,
		.wdr_mode = ISP_DOL_WDR_MODE,
		.intg_min = 1 << 4,
		.intg_max = (1250 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 2000 << 4,
		.regs = sensor_dol_4lane12bit_1080P30_reg,
		.regs_size = ARRAY_SIZE(sensor_dol_4lane12bit_1080P30_reg),
		.set_size = NULL,
		.top_clk = 432000000,
		.isp_clk = 297000000,
	},
#endif

	{
		.width = 1920,
		.height = 1080,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1980,
		.vts = 1250,
		.pclk = 74250000,
		.mipi_bps = 891 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		//.lp_mode = SENSOR_LP_DISCONTINUOUS,
		.intg_min = 1 << 4,
		.intg_max = (1250 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 2000 << 4,
		.regs = sensor_4lane12bit_1080P30_reg,
		.regs_size = ARRAY_SIZE(sensor_4lane12bit_1080P30_reg),
		.set_size = NULL,
		.top_clk = 432000000,
		.isp_clk = 297000000,
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
	data_type rdval_l, rdval_h;
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
	imx662_sensor_vts = wsize->vts;
	sensor_read(sd, 0x300E, &rdval_l);
	sensor_read(sd, 0x300F, &rdval_h);
	imx662_sensor_svr = (rdval_h << 8) | rdval_l;
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

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_GAIN, 1 * 1600,
			      256 * 1600, 1, 1 * 1600);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(handler, ops, V4L2_CID_EXPOSURE, 0,
			      65536 * 16, 1, 0);
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
