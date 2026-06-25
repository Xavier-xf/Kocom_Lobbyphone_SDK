/*
 * A V4L2 driver for imx675 Raw cameras.
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
MODULE_DESCRIPTION("A low-level driver for IMX675 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (74250000)
#define V4L2_IDENT_SENSOR 0x0675

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

/*
 * The IMX675 i2c address
 */
#define I2C_ADDR 0x34

#define SENSOR_NUM 0x1
#define SENSOR_NAME "imx675_mipi"
#define SENSOR_NAME_2 "imx675_mipi_2"

#define DOL_RHS1	141
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

//IMX675 Initial Setting for 10bit_2592x1944_p30
//All-pixel scan
//CSI-2_4lane
//Clock In 74.25MHz
//AD:10bit Output:10bit
//594Mbps
//Master Mode
//frame rate 30fps
//Horizontal Clock : 1100
//Vertical Line : 2250
static struct regval_list sensor_10b_2592x1944_p30_regs[] = {
	{0x3000, 0x01},
	{0x3002, 0x00},
	{0x3014, 0x00},
	{0x3015, 0x07},
	{0x3018, 0x00},
	{0x301A, 0x00},
	{0x301B, 0x00},
	{0x301C, 0x00},
	{0x301E, 0x01},
	{0x3020, 0x00},
	{0x3021, 0x00},
	{0x3022, 0x00},
	{0x3023, 0x00},
	{0x3028, 0xCA},
	{0x3029, 0x08},
	{0x302A, 0x00},
	{0x302C, 0x4C},
	{0x302D, 0x04},
	{0x3030, 0x00},
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x303C, 0x00},
	{0x303D, 0x00},
	{0x303E, 0x30},
	{0x303F, 0x0A},
	{0x3040, 0x03},
	{0x3044, 0x00},
	{0x3045, 0x00},
	{0x3046, 0xAC},
	{0x3047, 0x07},
	{0x304C, 0x00},
	{0x304D, 0x00},
	{0x3050, 0x04},
	{0x3051, 0x00},
	{0x3052, 0x00},
	{0x3054, 0x93},
	{0x3055, 0x00},
	{0x3056, 0x00},
	{0x3058, 0x53},
	{0x3059, 0x00},
	{0x305A, 0x00},
	{0x3060, 0x95},
	{0x3061, 0x00},
	{0x3062, 0x00},
	{0x3064, 0x56},
	{0x3065, 0x00},
	{0x3066, 0x00},
	{0x3070, 0x00},	//GAIN_0[0:7],
	{0x3071, 0x00},	//GAIN_0[8:10]
	{0x3072, 0x00},	//GAIN_1[0:7]
	{0x3073, 0x00},	//GAIN_1[8:11]
	{0x3074, 0x00},	//GAIN_2[0:7]
	{0x3075, 0x00},	//GAIN_2[8:11]
	{0x30A4, 0xAA},
	{0x30A6, 0x00},
	{0x30CC, 0x00},
	{0x30CD, 0x00},
	{0x30CE, 0x02},
	{0x30DC, 0x32},	//BLKLEVEL[0:7]
	{0x30DD, 0x40},	//BLKLEVEL[8:9]
	{0x310C, 0x01},	//Set to 01h
	{0x3130, 0x01},	//Set to 01h
	{0x3148, 0x00},	//Set to 00h
	{0x315E, 0x10},	//Set to 10h
	{0x3400, 0x01},	//GAIN_PGC_FIDMD, 0:Clear HDR
	{0x3460, 0x22},	//Set to 22h
	{0x347B, 0x02},	//Set to 02h
	{0x3492, 0x08},	//Set to 08h
	{0x3890, 0x08},	//HFR_EN, Set to 8h
	{0x3891, 0x00},	//HFR_PIX_VST[0:7]
	{0x3893, 0x00},	//HFR_PIX_VST[8:11]
	{0x3B1D, 0x17},	//Set to 17h
	{0x3B44, 0x3F},	//Set to 3Fh
	{0x3B60, 0x03},	//Set to 03h
	{0x3C03, 0x04},	//Set to 04h
	{0x3C04, 0x04},	//Set to 04h
	{0x3C0A, 0x03},	//Set to 03h
	{0x3C0B, 0x03},	//Set to 03h
	{0x3C0C, 0x03},	//Set to 03h
	{0x3C0D, 0x03},	//Set to 03h
	{0x3C0E, 0x03},	//Set to 03h
	{0x3C0F, 0x03},	//Set to 03h
	{0x3C30, 0x73},	//Set to 73h
	{0x3C3C, 0x20},	//Set to 20h
	{0x3C44, 0x06},
	{0x3C7C, 0xB9},	//Set to B9h
	{0x3C7D, 0x01},	//Set to 01h
	{0x3C7E, 0xB7},	//Set to B7h
	{0x3C7F, 0x01},	//Set to 01h
	{0x3CB0, 0x00},	//Set to 00h
	{0x3CB2, 0xFF},	//Set to FFh
	{0x3CB3, 0x03},	//Set to 03h
	{0x3CB4, 0xFF},	//Set to FFh
	{0x3CB5, 0x03},	//Set to 03h
	{0x3CBA, 0xFF},	//Set to FFh
	{0x3CBB, 0x03},	//Set to 03h
	{0x3CC0, 0xFF},	//Set to FFh
	{0x3CC1, 0x03},	//Set to 03h
	{0x3CC2, 0x00},	//Set to 00h
	{0x3CC6, 0xFF},	//Set to FFh
	{0x3CC7, 0x03},	//Set to 03h
	{0x3CC8, 0xFF},	//Set to FFh
	{0x3CC9, 0x03},	//Set to 03h
	{0x3E00, 0x1E},	//Set to 1Eh
	{0x3E02, 0x04},	//Set to 04h
	{0x3E03, 0x00},	//Set to 00h
	{0x3E20, 0x04},	//Set to 04h
	{0x3E21, 0x00},	//Set to 00h
	{0x3E22, 0x1E},	//Set to 1Eh
	{0x3E24, 0xBA},	//Set to BAh
	{0x3E72, 0x85},	//Set to 85h
	{0x3E76, 0x0C},	//Set to 0Ch
	{0x3E77, 0x01},	//Set to 01h
	{0x3E7A, 0x85},	//Set to 85h
	{0x3E7E, 0x1F},	//Set to 1Fh
	{0x3E82, 0xA6},	//Set to A6h
	{0x3E86, 0x2D},	//Set to 2Dh
	{0x3EE2, 0x33},	//Set to 33h
	{0x3EE3, 0x03},	//Set to 03h
	{0x4490, 0x07},	//Set to 07h
	{0x4494, 0x19},	//Set to 19h
	{0x4495, 0x00},	//Set to 00h
	{0x4496, 0xBB},	//Set to BBh
	{0x4497, 0x00},	//Set to 00h
	{0x4498, 0x55},
	{0x449A, 0x50},	//Set to 50h
	{0x449C, 0x50},	//Set to 50h
	{0x449E, 0x50},	//Set to 50h
	{0x44A0, 0x3C},	//Set to 3Ch
	{0x44A2, 0x19},	//Set to 19h
	{0x44A4, 0x19},	//Set to 19h
	{0x44A6, 0x19},	//Set to 19h
	{0x44A8, 0x4B},
	{0x44AA, 0x4B},
	{0x44AC, 0x4B},
	{0x44AE, 0x4B},
	{0x44B0, 0x3C},
	{0x44B2, 0x19},	//Set to 19h
	{0x44B4, 0x19},	//Set to 19h
	{0x44B6, 0x19},	//Set to 19h
	{0x44B8, 0x4B},
	{0x44BA, 0x4B},
	{0x44BC, 0x4B},
	{0x44BE, 0x4B},
	{0x44C0, 0x3C},
	{0x44C2, 0x19},	//Set to 19h
	{0x44C4, 0x19},	//Set to 19h
	{0x44C6, 0x19},	//Set to 19h
	{0x44C8, 0xF0},	//Set to F0h
	{0x44CA, 0xEB},	//Set to EBh
	{0x44CC, 0xEB},	//Set to EBh
	{0x44CE, 0xE6},	//Set to E6h
	{0x44D0, 0xE6},	//Set to E6h
	{0x44D2, 0xBB},	//Set to BBh
	{0x44D4, 0xBB},	//Set to BBh
	{0x44D6, 0xBB},	//Set to BBh
	{0x44D8, 0xE6},	//Set to E6h
	{0x44DA, 0xE6},	//Set to E6h
	{0x44DC, 0xE6},	//Set to E6h
	{0x44DE, 0xE6},	//Set to E6h
	{0x44E0, 0xE6},	//Set to E6h
	{0x44E2, 0xBB},	//Set to BBh
	{0x44E4, 0xBB},	//Set to BBh
	{0x44E6, 0xBB},	//Set to BBh
	{0x44E8, 0xE6},	//Set to E6h
	{0x44EA, 0xE6},	//Set to E6h
	{0x44EC, 0xE6},	//Set to E6h
	{0x44EE, 0xE6},	//Set to E6h
	{0x44F0, 0xE6},	//Set to E6h
	{0x44F2, 0xBB},	//Set to BBh
	{0x44F4, 0xBB},	//Set to BBh
	{0x44F6, 0xBB},	//Set to BBh
	{0x4538, 0x15},	//Set to 15h
	{0x4539, 0x15},	//Set to 15h
	{0x453A, 0x15},	//Set to 15h
	{0x4544, 0x15},	//Set to 15h
	{0x4545, 0x15},	//Set to 15h
	{0x4546, 0x15},	//Set to 15h
	{0x4550, 0x10},	//Set to 10h
	{0x4551, 0x10},	//Set to 10h
	{0x4552, 0x10},	//Set to 10h
	{0x4553, 0x10},	//Set to 10h
	{0x4554, 0x10},	//Set to 10h
	{0x4555, 0x10},	//Set to 10h
	{0x4556, 0x10},	//Set to 10h
	{0x4557, 0x10},	//Set to 10h
	{0x4558, 0x10},	//Set to 10h
	{0x455C, 0x10},	//Set to 10h
	{0x455D, 0x10},	//Set to 10h
	{0x455E, 0x10},	//Set to 10h
	{0x455F, 0x10},	//Set to 10h
	{0x4560, 0x10},	//Set to 10h
	{0x4561, 0x10},	//Set to 10h
	{0x4562, 0x10},	//Set to 10h
	{0x4563, 0x10},	//Set to 10h
	{0x4564, 0x10},	//Set to 10h
	{0x4569, 0x01},	//Set to 01h
	{0x456A, 0x01},	//Set to 01h
	{0x456B, 0x06},	//Set to 06h
	{0x456C, 0x06},	//Set to 06h
	{0x456D, 0x06},	//Set to 06h
	{0x456E, 0x06},	//Set to 06h
	{0x456F, 0x06},	//Set to 06h
	{0x4570, 0x06},	//Set to 06h
	{0x3000, 0x00} //Operation
};

#if ClearHdr
//IMX675 Initial Setting for 10bit_2592x1944_p30 Clear HDR mode
//IMX675 ClearHDR
//All-pixel scan
//CSI-2_4lane
//Clock In 74.25MHz
//AD:10bit Output:10bit
//1188Mbps
//Master Mode
//ClearHDR VC
//frame rate 30fps
//Horizontal Clock : 550
//Vertical Line : 4500
static struct regval_list sensor_10b_2592x1944_p30clearhdr_regs[] = {
	{0x3000, 0x01},	//Standby, 01h : Standby
	//{0x3001, 0x00},	//REGHOLD
	{0x3002, 0x00},	//Master Mode, 00h : Master Mode
	{0x3014, 0x00},	//CLKIN, 00h : 74.25MHz
	{0x3015, 0x04},	//DATARATE, 04h : 1188MBPS
	{0x3018, 0x00},	//WINMODE, 00h : All pixel mode
	{0x301A, 0x08},	//WDMODE, 08h : Clear HDR Mode
	{0x301B, 0x00},	//ADDMode, 00h : Non binning mode
	{0x301C, 0x00},	//THIN_V_EN, 00h : subsampling disable
	{0x301E, 0x01},	//VCMODE, 01h : Virtual Channel Mode
	{0x3020, 0x00},	//HREVERSE, 00h : Normal
	{0x3021, 0x00},	//VREVERSE, 00h : Normal
	{0x3022, 0x00},	//ADBIT, 00h : AD 10bit
	{0x3023, 0x00},	//MDBIT, 00h : 10bit Output
	{0x3028, 0x94},	//VMAX[0:7], 4500 = 0x1194
	{0x3029, 0x11},	//VMAX[8:15]
	{0x302A, 0x00},	//VMAX[16:19]
	{0x302C, 0x26},	//HMAX[0:7], 550 = 0x0226
	{0x302D, 0x02},	//HMAX[8:15]
	{0x3030, 0x02},	//FDG_SEL0[0:1], 02h : Clear HDR Mode
	{0x3031, 0x00},	//FDG_SEL1[0:1]
	{0x3032, 0x00},	//FDG_SEL2[0:1]
	{0x303C, 0x00},	//PIX_HST[0:7], 0000h : Start Position
	{0x303D, 0x00},	//PIX_HST[8:12]
	{0x303E, 0x30},	//PIX_HWIDTH[0:7] 0A30h : 2608
	{0x303F, 0x0A},	//PIX_HWIDTH[8:12]
	{0x3040, 0x03},	//LANEMODE[0:2], 3h : 4Lane
	{0x3044, 0x00},	//PIX_VST[0:7], 00h : Start position In window Cropping mode
	{0x3045, 0x00},	//PIX_VST[8:11]
	{0x3046, 0xAC},	//PIX_HWIDTH[0:7], 07ACh : 1964
	{0x3047, 0x07},	//PIX_HWIDTH[8:11]
	{0x304C, 0x1e},	//GAIN_HG0[0:7]
	{0x304D, 0x00},	//GAIN_HG0[8:11]
	{0x3050, 0x08},	//SHR0[0:7], 0x08, More than 8 => Clear HDR Mode
	{0x3051, 0x00},	//SHR0[8:15]
	{0x3052, 0x00},	//SHR0[16:19]
	{0x3054, 0x93},	//SHR1[0:7]
	{0x3055, 0x00},	//SHR1[8:15]
	{0x3056, 0x00},	//SHR1[16:19]
	{0x3058, 0x53},	//SHR2[0:7]
	{0x3059, 0x00},	//SHR2[8:15]
	{0x305A, 0x00},	//SHR2[16:19]
	{0x3060, 0x95},	//RHS1[0:7], Readout timing setting of SEF1
	{0x3061, 0x00},	//RHS1[8:15]
	{0x3062, 0x00},	//SHR2[16:19]
	{0x3064, 0x56},	//RHS2[0:7], Readout timing setting of SEF2
	{0x3065, 0x00},	//RHS2[8:15]
	{0x3066, 0x00},	//RHS2[16:19]
	{0x3070, 0x00},	//GAIN_0[0:7],
	{0x3071, 0x00},	//GAIN_0[8:10]
	{0x3072, 0x00},	//GAIN_1[0:7]
	{0x3073, 0x00},	//GAIN_1[8:11]
	{0x3074, 0x00},	//GAIN_2[0:7]
	{0x3075, 0x00},	//GAIN_2[8:11]
	{0x30A4, 0xAA},	//XVSOUTSEL / XHSOUTSEL, AAh : XVS=VSYNC, XHS=HSYNC
	{0x30A6, 0x00},	//XVS_DRV / XHS_DRV, 00h : XVS:Output, XHS:Output
	{0x30CC, 0x00},	//XVSLNG, XVS pulse width setting
	{0x30CD, 0x00},	//XHSLNG, HHS pulse width setting
	{0x30CE, 0x02},	//Set to 02h
	{0x30DC, 0x32},	//BLKLEVEL[0:7]
	{0x30DD, 0x40},	//BLKLEVEL[8:9]
	{0x310C, 0x01},	//Set to 01h
	{0x3130, 0x01},	//Set to 01h
	{0x3148, 0x00},	//Set to 00h
	{0x315E, 0x10},	//Set to 10h
	{0x3400, 0x01},	//GAIN_PGC_FIDMD, 0:Clear HDR
	{0x3460, 0x22},	//Set to 22h
	{0x347B, 0x02},	//Set to 02h
	{0x3492, 0x08},	//Set to 08h
	{0x3890, 0x08},	//HFR_EN, Set to 8h
	{0x3891, 0x00},	//HFR_PIX_VST[0:7]
	{0x3893, 0x00},	//HFR_PIX_VST[8:11]
	{0x3B1D, 0x17},	//Set to 17h
	{0x3B44, 0x3F},	//Set to 3Fh
	{0x3B60, 0x03},	//Set to 03h
	{0x3C03, 0x04},	//Set to 04h
	{0x3C04, 0x04},	//Set to 04h
	{0x3C0A, 0x03},	//Set to 03h
	{0x3C0B, 0x03},	//Set to 03h
	{0x3C0C, 0x03},	//Set to 03h
	{0x3C0D, 0x03},	//Set to 03h
	{0x3C0E, 0x03},	//Set to 03h
	{0x3C0F, 0x03},	//Set to 03h
	{0x3C30, 0x73},	//Set to 73h
	{0x3C3C, 0x20},	//Set to 20h
	{0x3C44, 0x05},	//Set to 05h, //Set to 05h, for clear HDR mode
	{0x3C7C, 0xB9},	//Set to B9h
	{0x3C7D, 0x01},	//Set to 01h
	{0x3C7E, 0xB7},	//Set to B7h
	{0x3C7F, 0x01},	//Set to 01h
	{0x3CB0, 0x00},	//Set to 00h
	{0x3CB2, 0xFF},	//Set to FFh
	{0x3CB3, 0x03},	//Set to 03h
	{0x3CB4, 0xFF},	//Set to FFh
	{0x3CB5, 0x03},	//Set to 03h
	{0x3CBA, 0xFF},	//Set to FFh
	{0x3CBB, 0x03},	//Set to 03h
	{0x3CC0, 0xFF},	//Set to FFh
	{0x3CC1, 0x03},	//Set to 03h
	{0x3CC2, 0x00},	//Set to 00h
	{0x3CC6, 0xFF},	//Set to FFh
	{0x3CC7, 0x03},	//Set to 03h
	{0x3CC8, 0xFF},	//Set to FFh
	{0x3CC9, 0x03},	//Set to 03h
	{0x3E00, 0x1E},	//Set to 1Eh
	{0x3E02, 0x04},	//Set to 04h
	{0x3E03, 0x00},	//Set to 00h
	{0x3E20, 0x04},	//Set to 04h
	{0x3E21, 0x00},	//Set to 00h
	{0x3E22, 0x1E},	//Set to 1Eh
	{0x3E24, 0xBA},	//Set to BAh
	{0x3E72, 0x85},	//Set to 85h
	{0x3E76, 0x0C},	//Set to 0Ch
	{0x3E77, 0x01},	//Set to 01h
	{0x3E7A, 0x85},	//Set to 85h
	{0x3E7E, 0x1F},	//Set to 1Fh
	{0x3E82, 0xA6},	//Set to A6h
	{0x3E86, 0x2D},	//Set to 2Dh
	{0x3EE2, 0x33},	//Set to 33h
	{0x3EE3, 0x03},	//Set to 03h
	{0x4490, 0x07},	//Set to 07h
	{0x4494, 0x19},	//Set to 19h
	{0x4495, 0x00},	//Set to 00h
	{0x4496, 0xBB},	//Set to BBh
	{0x4497, 0x00},	//Set to 00h
	{0x4498, 0x5A},	//Set to 5Ah
	{0x449A, 0x50},	//Set to 50h
	{0x449C, 0x50},	//Set to 50h
	{0x449E, 0x50},	//Set to 50h
	{0x44A0, 0x3C},	//Set to 3Ch
	{0x44A2, 0x19},	//Set to 19h
	{0x44A4, 0x19},	//Set to 19h
	{0x44A6, 0x19},	//Set to 19h
	{0x44A8, 0x50},	//Set to 50h
	{0x44AA, 0x50},	//Set to 50h
	{0x44AC, 0x50},	//Set to 50h
	{0x44AE, 0x46},	//Set to 46h
	{0x44B0, 0x46},	//Set to 46h
	{0x44B2, 0x19},	//Set to 19h
	{0x44B4, 0x19},	//Set to 19h
	{0x44B6, 0x19},	//Set to 19h
	{0x44B8, 0x50},	//Set to 50h
	{0x44BA, 0x50},	//Set to 50h
	{0x44BC, 0x50},	//Set to 50h
	{0x44BE, 0x46},	//Set to 46h
	{0x44C0, 0x46},	//Set to 46h
	{0x44C2, 0x19},	//Set to 19h
	{0x44C4, 0x19},	//Set to 19h
	{0x44C6, 0x19},	//Set to 19h
	{0x44C8, 0xF0},	//Set to F0h
	{0x44CA, 0xEB},	//Set to EBh
	{0x44CC, 0xEB},	//Set to EBh
	{0x44CE, 0xE6},	//Set to E6h
	{0x44D0, 0xE6},	//Set to E6h
	{0x44D2, 0xBB},	//Set to BBh
	{0x44D4, 0xBB},	//Set to BBh
	{0x44D6, 0xBB},	//Set to BBh
	{0x44D8, 0xE6},	//Set to E6h
	{0x44DA, 0xE6},	//Set to E6h
	{0x44DC, 0xE6},	//Set to E6h
	{0x44DE, 0xE6},	//Set to E6h
	{0x44E0, 0xE6},	//Set to E6h
	{0x44E2, 0xBB},	//Set to BBh
	{0x44E4, 0xBB},	//Set to BBh
	{0x44E6, 0xBB},	//Set to BBh
	{0x44E8, 0xE6},	//Set to E6h
	{0x44EA, 0xE6},	//Set to E6h
	{0x44EC, 0xE6},	//Set to E6h
	{0x44EE, 0xE6},	//Set to E6h
	{0x44F0, 0xE6},	//Set to E6h
	{0x44F2, 0xBB},	//Set to BBh
	{0x44F4, 0xBB},	//Set to BBh
	{0x44F6, 0xBB},	//Set to BBh
	{0x4538, 0x15},	//Set to 15h
	{0x4539, 0x15},	//Set to 15h
	{0x453A, 0x15},	//Set to 15h
	{0x4544, 0x15},	//Set to 15h
	{0x4545, 0x15},	//Set to 15h
	{0x4546, 0x15},	//Set to 15h
	{0x4550, 0x10},	//Set to 10h
	{0x4551, 0x10},	//Set to 10h
	{0x4552, 0x10},	//Set to 10h
	{0x4553, 0x10},	//Set to 10h
	{0x4554, 0x10},	//Set to 10h
	{0x4555, 0x10},	//Set to 10h
	{0x4556, 0x10},	//Set to 10h
	{0x4557, 0x10},	//Set to 10h
	{0x4558, 0x10},	//Set to 10h
	{0x455C, 0x10},	//Set to 10h
	{0x455D, 0x10},	//Set to 10h
	{0x455E, 0x10},	//Set to 10h
	{0x455F, 0x10},	//Set to 10h
	{0x4560, 0x10},	//Set to 10h
	{0x4561, 0x10},	//Set to 10h
	{0x4562, 0x10},	//Set to 10h
	{0x4563, 0x10},	//Set to 10h
	{0x4564, 0x10},	//Set to 10h
	{0x4569, 0x00},	//Set to 00h
	{0x456A, 0x00},	//Set to 00h
	{0x456B, 0x04},	//Set to 04h
	{0x456C, 0x04},	//Set to 04h
	{0x456D, 0x04},	//Set to 04h
	{0x456E, 0x04},	//Set to 04h
	{0x456F, 0x04},	//Set to 04h
	{0x4570, 0x04},	//Set to 04h
	{0x3000, 0x00} //Operation
};

#else
//IMX675 Initial Setting for 10bit_2592x1944_p30 DOL 2frame mode
//"All-pixel scan
//CSI-2_4lane
//Clock In 74.25MHz
//AD:10bit Output:10bit
//1188Mbps
//Master Mode
//LCG Mode
//DOL HDR 2frame VC
//frame rate 30fps
//Horizontal Clock : 550
//Vertical Line : 2250
static struct regval_list sensor_10b_2592x1944_p30dol_regs[] = {
	{0x3000, 0x01},	//Standby, 01h : Standby
	//{0x3001, 0x00},	//REGHOLD
	{0x3002, 0x00},	//Master Mode, 00h : Master Mode
	{0x3014, 0x00},	//CLKIN, 00h : 74.25MHz
	{0x3015, 0x04},	//DATARATE, 04h : 1188MBPS
	{0x3018, 0x00},	//WINMODE, 00h : All pixel mode
	{0x301A, 0x01},	//WDMODE, 01h : DOL 2 frame Mode
	{0x301B, 0x00},	//ADDMode, 00h : Non binning mode
	{0x301C, 0x01},	//THIN_V_EN, 01h : subsampling Enable
	{0x301E, 0x01},	//VCMODE, 01h : Virtual Channel Mode
	{0x3020, 0x00},	//HREVERSE, 00h : Normal
	{0x3021, 0x00},	//VREVERSE, 00h : Normal
	{0x3022, 0x00},	//ADBIT, 00h : AD 10bit
	{0x3023, 0x00},	//MDBIT, 00h : 10bit Output
	{0x3028, 0xCA},	//VMAX[0:7], 2250 = 0x08CA
	{0x3029, 0x08},	//VMAX[8:15]
	{0x302A, 0x00},	//VMAX[16:19]
	{0x302C, 0x26},	//HMAX[0:7], 550 = 0x0226
	{0x302D, 0x02},	//HMAX[8:15]
	{0x3030, 0x00},	//FDG_SEL0[0:1], 02h : DOL2F Mode
	{0x3031, 0x00},	//FDG_SEL1[0:1]
	{0x3032, 0x00},	//FDG_SEL2[0:1]
	{0x303C, 0x00},	//PIX_HST[0:7], 0000h : Start Position
	{0x303D, 0x00},	//PIX_HST[8:12]
	{0x303E, 0x30},	//PIX_HWIDTH[0:7] 0A30h : 2608
	{0x303F, 0x0A},	//PIX_HWIDTH[8:12]
	{0x3040, 0x03},	//LANEMODE[0:2], 3h : 4Lane
	{0x3044, 0x00},	//PIX_VST[0:7], 00h : Start position In window Cropping mode
	{0x3045, 0x00},	//PIX_VST[8:11]
	{0x3046, 0xAC},	//PIX_HWIDTH[0:7], 07ACh : 1964
	{0x3047, 0x07},	//PIX_HWIDTH[8:11]
	{0x304C, 0x00},	//GAIN_HG0[0:7]
	{0x304D, 0x00},	//GAIN_HG0[8:11]
	{0x3050, 0x74},	//SHR0[0:7], 574h : 1396
	{0x3051, 0x05},	//SHR0[8:15]
	{0x3052, 0x00},	//SHR0[16:19]
	{0x3054, 0x05},	//SHR1[0:7]
	{0x3055, 0x00},	//SHR1[8:15]
	{0x3056, 0x00},	//SHR1[16:19]
	{0x3058, 0x53},	//SHR2[0:7]
	{0x3059, 0x00},	//SHR2[8:15]
	{0x305A, 0x00},	//SHR2[16:19]
	{0x3060, DOL_RHS1&0xff},      //RHS1[0:7], Readout timing setting of SEF1
	{0x3061, (DOL_RHS1>>8)&0xff}, //RHS1[8:15]
	{0x3062, 0x00},	//SHR2[16:19]
	{0x3064, 0x56},	//RHS2[0:7], Readout timing setting of SEF2
	{0x3065, 0x00},	//RHS2[8:15]
	{0x3066, 0x00},	//RHS2[16:19]
	{0x3070, 0x00},	//GAIN_0[0:7],
	{0x3071, 0x00},	//GAIN_0[8:10]
	{0x3072, 0x00},	//GAIN_1[0:7]
	{0x3073, 0x00},	//GAIN_1[8:11]
	{0x3074, 0x00},	//GAIN_2[0:7]
	{0x3075, 0x00},	//GAIN_2[8:11]
	{0x30A4, 0xAA},	//XVSOUTSEL / XHSOUTSEL, AAh : XVS=VSYNC, XHS=HSYNC
	{0x30A6, 0x00},	//XVS_DRV / XHS_DRV, 00h : XVS:Output, XHS:Output
	{0x30CC, 0x00},	//XVSLNG, XVS pulse width setting
	{0x30CD, 0x00},	//XHSLNG, HHS pulse width setting
	{0x30CE, 0x02},	//Set to 02h
	{0x30DC, 0x32},	//BLKLEVEL[0:7]
	{0x30DD, 0x40},	//BLKLEVEL[8:9]
	{0x310C, 0x01},	//Set to 01h
	{0x3130, 0x01},	//Set to 01h
	{0x3148, 0x00},	//Set to 00h
	{0x315E, 0x10},	//Set to 10h
	{0x3400, 0x01},	//GAIN_PGC_FIDMD, 0:Clear HDR
	{0x3460, 0x22},	//Set to 22h
	{0x347B, 0x02},	//Set to 02h
	{0x3492, 0x08},	//Set to 08h
	{0x3890, 0x08},	//HFR_EN, Set to 8h
	{0x3891, 0x00},	//HFR_PIX_VST[0:7]
	{0x3893, 0x00},	//HFR_PIX_VST[8:11]
	{0x3B1D, 0x17},	//Set to 17h
	{0x3B44, 0x3F},	//Set to 3Fh
	{0x3B60, 0x03},	//Set to 03h
	{0x3C03, 0x04},	//Set to 04h
	{0x3C04, 0x04},	//Set to 04h
	{0x3C0A, 0x03},	//Set to 03h
	{0x3C0B, 0x03},	//Set to 03h
	{0x3C0C, 0x03},	//Set to 03h
	{0x3C0D, 0x03},	//Set to 03h
	{0x3C0E, 0x03},	//Set to 03h
	{0x3C0F, 0x03},	//Set to 03h
	{0x3C30, 0x73},	//Set to 73h
	{0x3C3C, 0x20},	//Set to 20h
	{0x3C44, 0x06},	//Set to 06h, for normal mode
	{0x3C7C, 0xB9},	//Set to B9h
	{0x3C7D, 0x01},	//Set to 01h
	{0x3C7E, 0xB7},	//Set to B7h
	{0x3C7F, 0x01},	//Set to 01h
	{0x3CB0, 0x00},	//Set to 00h
	{0x3CB2, 0xFF},	//Set to FFh
	{0x3CB3, 0x03},	//Set to 03h
	{0x3CB4, 0xFF},	//Set to FFh
	{0x3CB5, 0x03},	//Set to 03h
	{0x3CBA, 0xFF},	//Set to FFh
	{0x3CBB, 0x03},	//Set to 03h
	{0x3CC0, 0xFF},	//Set to FFh
	{0x3CC1, 0x03},	//Set to 03h
	{0x3CC2, 0x00},	//Set to 00h
	{0x3CC6, 0xFF},	//Set to FFh
	{0x3CC7, 0x03},	//Set to 03h
	{0x3CC8, 0xFF},	//Set to FFh
	{0x3CC9, 0x03},	//Set to 03h
	{0x3E00, 0x1E},	//Set to 1Eh
	{0x3E02, 0x04},	//Set to 04h
	{0x3E03, 0x00},	//Set to 00h
	{0x3E20, 0x04},	//Set to 04h
	{0x3E21, 0x00},	//Set to 00h
	{0x3E22, 0x1E},	//Set to 1Eh
	{0x3E24, 0xBA},	//Set to BAh
	{0x3E72, 0x85},	//Set to 85h
	{0x3E76, 0x0C},	//Set to 0Ch
	{0x3E77, 0x01},	//Set to 01h
	{0x3E7A, 0x85},	//Set to 85h
	{0x3E7E, 0x1F},	//Set to 1Fh
	{0x3E82, 0xA6},	//Set to A6h
	{0x3E86, 0x2D},	//Set to 2Dh
	{0x3EE2, 0x33},	//Set to 33h
	{0x3EE3, 0x03},	//Set to 03h
	{0x4490, 0x07},	//Set to 07h
	{0x4494, 0x19},	//Set to 19h
	{0x4495, 0x00},	//Set to 00h
	{0x4496, 0xBB},	//Set to BBh
	{0x4497, 0x00},	//Set to 00h
	{0x4498, 0x55},	//Set to 55h
	{0x449A, 0x50},	//Set to 50h
	{0x449C, 0x50},	//Set to 50h
	{0x449E, 0x50},	//Set to 50h
	{0x44A0, 0x3C},	//Set to 3Ch
	{0x44A2, 0x19},	//Set to 19h
	{0x44A4, 0x19},	//Set to 19h
	{0x44A6, 0x19},	//Set to 19h
	{0x44A8, 0x4B},	//Set to 4Bh
	{0x44AA, 0x4B},	//Set to 4Bh
	{0x44AC, 0x4B},	//Set to 4Bh
	{0x44AE, 0x4B},	//Set to 4Bh
	{0x44B0, 0x3C},	//Set to 3Ch
	{0x44B2, 0x19},	//Set to 19h
	{0x44B4, 0x19},	//Set to 19h
	{0x44B6, 0x19},	//Set to 19h
	{0x44B8, 0x4B},	//Set to 4Bh
	{0x44BA, 0x4B},	//Set to 4Bh
	{0x44BC, 0x4B},	//Set to 4Bh
	{0x44BE, 0x4B},	//Set to 4Bh
	{0x44C0, 0x3C},	//Set to 3Ch
	{0x44C2, 0x19},	//Set to 19h
	{0x44C4, 0x19},	//Set to 19h
	{0x44C6, 0x19},	//Set to 19h
	{0x44C8, 0xF0},	//Set to F0h
	{0x44CA, 0xEB},	//Set to EBh
	{0x44CC, 0xEB},	//Set to EBh
	{0x44CE, 0xE6},	//Set to E6h
	{0x44D0, 0xE6},	//Set to E6h
	{0x44D2, 0xBB},	//Set to BBh
	{0x44D4, 0xBB},	//Set to BBh
	{0x44D6, 0xBB},	//Set to BBh
	{0x44D8, 0xE6},	//Set to E6h
	{0x44DA, 0xE6},	//Set to E6h
	{0x44DC, 0xE6},	//Set to E6h
	{0x44DE, 0xE6},	//Set to E6h
	{0x44E0, 0xE6},	//Set to E6h
	{0x44E2, 0xBB},	//Set to BBh
	{0x44E4, 0xBB},	//Set to BBh
	{0x44E6, 0xBB},	//Set to BBh
	{0x44E8, 0xE6},	//Set to E6h
	{0x44EA, 0xE6},	//Set to E6h
	{0x44EC, 0xE6},	//Set to E6h
	{0x44EE, 0xE6},	//Set to E6h
	{0x44F0, 0xE6},	//Set to E6h
	{0x44F2, 0xBB},	//Set to BBh
	{0x44F4, 0xBB},	//Set to BBh
	{0x44F6, 0xBB},	//Set to BBh
	{0x4538, 0x15},	//Set to 15h
	{0x4539, 0x15},	//Set to 15h
	{0x453A, 0x15},	//Set to 15h
	{0x4544, 0x15},	//Set to 15h
	{0x4545, 0x15},	//Set to 15h
	{0x4546, 0x15},	//Set to 15h
	{0x4550, 0x10},	//Set to 10h
	{0x4551, 0x10},	//Set to 10h
	{0x4552, 0x10},	//Set to 10h
	{0x4553, 0x10},	//Set to 10h
	{0x4554, 0x10},	//Set to 10h
	{0x4555, 0x10},	//Set to 10h
	{0x4556, 0x10},	//Set to 10h
	{0x4557, 0x10},	//Set to 10h
	{0x4558, 0x10},	//Set to 10h
	{0x455C, 0x10},	//Set to 10h
	{0x455D, 0x10},	//Set to 10h
	{0x455E, 0x10},	//Set to 10h
	{0x455F, 0x10},	//Set to 10h
	{0x4560, 0x10},	//Set to 10h
	{0x4561, 0x10},	//Set to 10h
	{0x4562, 0x10},	//Set to 10h
	{0x4563, 0x10},	//Set to 10h
	{0x4564, 0x10},	//Set to 10h
	{0x4569, 0x01},	//Set to 01h
	{0x456A, 0x01},	//Set to 01h
	{0x456B, 0x06},	//Set to 06h
	{0x456C, 0x06},	//Set to 06h
	{0x456D, 0x06},	//Set to 06h
	{0x456E, 0x06},	//Set to 06h
	{0x456F, 0x06},	//Set to 06h
	{0x4570, 0x06},	//Set to 06h
	{0x3000, 0x00} //Operation
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

static int imx675_sensor_vts;
static int imx675_sensor_svr;
static int shutter_delay = 1;
static int shutter_delay_cnt;
static int fps_change_flag;

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	int exptime,  exp_val_m;
	struct sensor_info *info = to_state(sd);

#if ClearHdr
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		exptime = imx675_sensor_vts - (exp_val >> 8) - 1;
		if (exptime < 8)
			exptime = 8;
		exphigh = (unsigned char)((0x00f0000 & exptime) >> 16);
		expmid =  (unsigned char)((0x000ff00 & exptime) >> 8);
		explow =  (unsigned char)((0x00000ff & exptime));

		// IMX675 SHR0
		sensor_write(sd, 0x3050, explow);
		sensor_write(sd, 0x3051, expmid);
		sensor_write(sd, 0x3052, exphigh);
	} else {
		exptime = imx675_sensor_vts - (exp_val >> 4) - 1;
		if (exptime < 4)
			exptime = 4;
		exphigh = (unsigned char)((0x00f0000 & exptime) >> 16);
		expmid =  (unsigned char)((0x000ff00 & exptime) >> 8);
		explow =  (unsigned char)((0x00000ff & exptime));

		// IMX675 SHR0
		sensor_write(sd, 0x3050, explow);
		sensor_write(sd, 0x3051, expmid);
		sensor_write(sd, 0x3052, exphigh);
	}
	sensor_dbg("sensor_set_exp = %d %d line Done!\n", exp_val, exptime);
#else
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		// LEF
		exptime = (imx675_sensor_vts<<1) - (exp_val>>4);
		if (exptime < DOL_RHS1 + 5) {
			exptime = DOL_RHS1 + 5;
			exp_val = ((imx675_sensor_vts << 1) - exptime) << 4;
		}
		sensor_dbg("long exp_val: %d, exptime: %d\n", exp_val, exptime);

		exphigh	= (unsigned char) ((0x00f0000 & exptime) >> 16);
		expmid	= (unsigned char) ((0x000ff00 & exptime) >> 8);
		explow	= (unsigned char) ((0x00000ff & exptime));

		// IMX675 SHR0
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

		// IMX675 SHR1
		sensor_write(sd, 0x3054, explow);
		sensor_write(sd, 0x3055, expmid);
		sensor_write(sd, 0x3056, exphigh);
	} else {
		exptime = imx675_sensor_vts - (exp_val >> 4) - 1;
		if (exptime < 4)
			exptime = 4;
		exphigh = (unsigned char)((0x00f0000 & exptime) >> 16);
		expmid =  (unsigned char)((0x000ff00 & exptime) >> 8);
		explow =  (unsigned char)((0x00000ff & exptime));

		// IMX675 SHR0
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
	int gain_val_l;
	if (gain_val < 1 * 16)
		gain_val = 16;
#if ClearHdr
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		gain_val_l = gain_val * DOL_RATIO * 10 / 26;

		//GAIN_HG0
		if (gain_val_l < 32 * 16) {
			sensor_write(sd, 0x304c, gain2db[gain_val_l - 16]);
		} else if (gain_val_l < 1024 * 16) {
			sensor_write(sd, 0x304c, gain2db[(gain_val_l>>5) - 16] + 100);
		} else {
			sensor_write(sd, 0x304c, gain2db[(gain_val_l>>10) - 16] + 200);
		}
		//GAIN_0
		if (gain_val < 32 * 16) {
			sensor_write(sd, 0x3070, gain2db[gain_val - 16]);
		} else if (gain_val < 1024 * 16) {
			sensor_write(sd, 0x3070, gain2db[(gain_val>>5) - 16] + 100);
		} else {
			sensor_write(sd, 0x3070, gain2db[(gain_val>>10) - 16] + 200);
		}
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
			//imx675 VMAX[0:19]
			sensor_write(sd, 0x3028, imx675_sensor_vts / (imx675_sensor_svr + 1) & 0xFF);
			sensor_write(sd, 0x3029, imx675_sensor_vts / (imx675_sensor_svr + 1) >> 8 & 0xFF);
			sensor_write(sd, 0x302a, imx675_sensor_vts / (imx675_sensor_svr + 1) >> 16);
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
	imx675_sensor_vts = wsize->pclk/fps->fps/wsize->hts;
	fps_change_flag = 1;
	sensor_write(sd, 0x3001, 1);

	sensor_read(sd, 0x3028, &rdval1);
	sensor_read(sd, 0x3029, &rdval2);
	sensor_read(sd, 0x302a, &rdval3);

	sensor_dbg("imx675_sensor_svr: %d, vts: %d.\n", imx675_sensor_svr, (rdval1 | (rdval2<<8) | (rdval3<<16)));
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
		.mbus_code = MEDIA_BUS_FMT_SRGGB10_1X10,
		//.mbus_code = MEDIA_BUS_FMT_SRGGB12_1X12,
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
		.width = 2592,
		.height = 1944,
		.hoffset = 0,
		.voffset = 0,
		.hts = 550,
		.vts = 4500,
		.pclk = 74250000 * 16,
		.mipi_bps = 1188 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		//.lp_mode = SENSOR_LP_DISCONTINUOUS,
		.if_mode = MIPI_VC_WDR_MODE,
		.wdr_mode = ISP_DOL_WDR_MODE,
		.intg_min = 1 << 4,
		.intg_max = (4500  - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 2000 << 4,
		.vipp_hoff = 16,
		.vipp_voff = 252,
		.vipp_w = 2560,
		.vipp_h = 1440,
		.regs = sensor_10b_2592x1944_p30clearhdr_regs,
		.regs_size = ARRAY_SIZE(sensor_10b_2592x1944_p30clearhdr_regs),
		.set_size = NULL,
		.top_clk = 432000000,
		.isp_clk = 297000000,
	},
#else
	//DOL HDR
	{
		.width = 2592,
		.height = 1944,
		.hoffset = 0,
		.voffset = 0,
		.hts = 550,
		.vts = 2250,
		.pclk = 74250000,
		.mipi_bps = 1188 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		//.lp_mode = SENSOR_LP_DISCONTINUOUS,
		.if_mode = MIPI_VC_WDR_MODE,
		.wdr_mode = ISP_DOL_WDR_MODE,
		.intg_min = 1 << 4,
		.intg_max = (2250 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 2000 << 4,
		.vipp_hoff = 16,
		.vipp_voff = 252,
		.vipp_w = 2560,
		.vipp_h = 1440,
		.regs = sensor_10b_2592x1944_p30dol_regs,
		.regs_size = ARRAY_SIZE(sensor_10b_2592x1944_p30dol_regs),
		.set_size = NULL,
		.top_clk = 432000000,
		.isp_clk = 297000000,
	},
#endif

	{
		.width = 2592,
		.height = 1944,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1100,
		.vts = 2250,
		.pclk = 74250000,
		.mipi_bps = 594 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		//.lp_mode = SENSOR_LP_DISCONTINUOUS,
		.intg_min = 1 << 4,
		.intg_max = (2250  - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 2000 << 4,
		.vipp_hoff = 16,
		.vipp_voff = 252,
		.vipp_w = 2560,
		.vipp_h = 1440,
		.regs = sensor_10b_2592x1944_p30_regs,
		.regs_size = ARRAY_SIZE(sensor_10b_2592x1944_p30_regs),
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
	imx675_sensor_vts = wsize->vts;
	sensor_read(sd, 0x300E, &rdval_l);
	sensor_read(sd, 0x300F, &rdval_h);
	imx675_sensor_svr = (rdval_h << 8) | rdval_l;
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
