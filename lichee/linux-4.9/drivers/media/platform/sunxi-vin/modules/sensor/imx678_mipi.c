/*
 * A V4L2 driver for imx678 Raw cameras.
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
MODULE_DESCRIPTION("A low-level driver for IMX678 sensors");
MODULE_LICENSE("GPL");

#define MCLK              (27*1000*1000)
#define V4L2_IDENT_SENSOR 0x0678

/*
 * Our nominal (default) frame rate.
 */

#define SENSOR_FRAME_RATE 30

/*
 * The IMX678 i2c address
 */
#define I2C_ADDR 0x34

#define SENSOR_NUM 0x1
#define SENSOR_NAME "imx678_mipi"
#define SENSOR_NAME_2 "imx678_mipi_2"

#define DOL_RHS1	141
#define DOL_RATIO	16 //Clear HDR -> 10.66x

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

//IMX678 Initial Setting for 10bit_3840x2160_p30
//All-pixel scan
//CSI-2_4lane
//Clock In 27MHz
//AD:10bit Output:10bit
//891Mbps
//Master Mode
//frame rate 30fps
//Horizontal Clock : 1100
//Vertical Line : 2250
static struct regval_list sensor_10b_3840x2160_p30_regs[] = {
	{0x3000, 0x01},//Standby 0h : Operating 1h : Standby

	{0x3001, 0x00},//REGHOLD
	{0x3002, 0x00},//XMSTA  0h : Master mode
	{0x3014, 0x03},//INCK setting
	{0x3015, 0x05},//mipi bps
	{0x3018, 0x00},//00h: All-pixel mode
	{0x3019, 0x00},//Color filter mode setting 0h: RGB  (Only if using IMX678-AAQR1) 1h: MONO  (Only if using IMX678-AAMR1)
	{0x301A, 0x00},//HDR mode setting 00h : Normal mode 01h: DOL 2 frame mode 02h: DOL 3 frame mode 08h: Clear HDR mode
	{0x301B, 0x00},//ADDMODE[1:0]  0h: Non-binning
	{0x301C, 0x00},//XVS subsampling setting 00h: Disable (Normal) 01h: Enable (Subsampling)
	{0x301E, 0x01},//When DOL mode and Clear HDR mode only 00h: Line Information Output 01h: Virtual Channel Mode
	{0x3020, 0x00},//Horizontal direction
	{0x3021, 0x00},//Vertical direction
	{0x3022, 0x00},//10bit
	{0x3023, 0x00},//10bit
	{0x3028, 0xCA},//VMAX
	{0x3029, 0x08},//VMAX
	{0x302A, 0x00},//VMAX
	{0x302C, 0x4C},//HMAX
	{0x302D, 0x04},//HMAX
	{0x3030, 0x00},//FDG_SEL0
	{0x3031, 0x00},//FDG_SEL1
	{0x3032, 0x00},//FDG_SEL2
	{0x303C, 0x00},//PIX_HST
	{0x303D, 0x00},//PIX_HST
	{0x303E, 0x10},//PIX_HWIDTH
	{0x303F, 0x0F},//PIX_HWIDTH
	{0x3040, 0x03},//LANEMODE 3h: 4lane
	{0x3042, 0x00},//XSIZE_OVERLAP
	{0x3043, 0x00},//XSIZE_OVERLAP
	{0x3044, 0x00},//PIX_VST
	{0x3045, 0x00},//PIX_VST
	{0x3046, 0x84},//PIX_VWIDTH
	{0x3047, 0x08},//PIX_VWIDTH
	{0x3050, 0x03},//SHR0
	{0x3051, 0x00},//SHR0
	{0x3052, 0x00},//SHR0
	{0x3054, 0x0E},//SHR1
	{0x3055, 0x00},//SHR1
	{0x3056, 0x00},//SHR1
	{0x3058, 0x8A},//SHR2
	{0x3059, 0x01},//SHR2
	{0x305A, 0x00},//SHR2
	{0x3060, 0x16},//RHS1
	{0x3061, 0x01},//RHS1
	{0x3062, 0x00},//RHS1
	{0x3064, 0xC4},//RHS2
	{0x3065, 0x0C},//RHS2
	{0x3066, 0x00},//RHS2
	{0x3069, 0x00},//CHDR_GAIN_EN
	{0x306B, 0x00},//00h : Normal mode  04h : Clear HDR mode
	{0x3070, 0x00},//GAIN
	{0x3071, 0x00},//GAIN
	{0x3072, 0x00},//GAIN_1
	{0x3073, 0x00},//GAIN_1
	{0x3074, 0x00},//GAIN_2
	{0x3075, 0x00},//GAIN_2
	{0x3081, 0x00},//EXP_GAIN
	{0x308C, 0x00},//CHDR_DGAIN0_HG
	{0x308D, 0x01},//CHDR_DGAIN0_HG
	{0x3094, 0x00},//CHDR_AGAIN0_LG
	{0x3095, 0x00},//CHDR_AGAIN0_LG
	{0x309C, 0x00},//CHDR_AGAIN0_HG
	{0x309D, 0x00},//CHDR_AGAIN0_HG
	{0x30A4, 0xAA},
	{0x30A6, 0x00},
	{0x30CC, 0x00},
	{0x30CD, 0x00},
	{0x30DC, 0x32},//BLKLEVEL
	{0x30DD, 0x40},//BLKLEVEL
	{0x3400, 0x01},//GAIN_PGC_FIDMD
	{0x3460, 0x22},//Set to "22h"
	{0x355A, 0x64},//When Normal mode only 64h : Normal mode 00h : Clear HDR mode

	{0x3A02, 0x7A},//Set to "7Ah"
	{0x3A10, 0xEC},//Set to "ECh"
	{0x3A12, 0x71},//Set to "71h"
	{0x3A14, 0xDE},//Set to "DEh"
	{0x3A20, 0x2B},//Set to "2Bh"
	{0x3A24, 0x22},//Set to "22h"
	{0x3A25, 0x25},//Set to "25h"
	{0x3A26, 0x2A},//Set to "2Ah"
	{0x3A27, 0x2C},//Set to "2Ch"
	{0x3A28, 0x39},//Set to "39h"
	{0x3A29, 0x38},//Set to "38h"
	{0x3A30, 0x04},//Set to "04h"
	{0x3A31, 0x04},//Set to "04h"
	{0x3A32, 0x03},//Set to "03h"
	{0x3A33, 0x03},//Set to "03h"
	{0x3A34, 0x09},//Set to "09h"
	{0x3A35, 0x06},//Set to "06h"
	{0x3A38, 0xCD},//Set to "CDh"
	{0x3A3A, 0x4C},//Set to "4Ch"
	{0x3A3C, 0xB9},//Set to "B9h"
	{0x3A3E, 0x30},//Set to "30h"
	{0x3A40, 0x2C},//Set to "2Ch"
	{0x3A42, 0x39},//Set to "39h"
	{0x3A4E, 0x00},//Set to "00h"
	{0x3A52, 0x00},//Set to "00h"
	{0x3A56, 0x00},//Set to "00h"
	{0x3A5A, 0x00},//Set to "00h"
	{0x3A5E, 0x00},//Set to "00h"
	{0x3A62, 0x00},//Set to "00h"
	{0x3A64, 0x00},//When Clear HDR mode only 00h : Normal mode 01h : Clear HDR mode
	{0x3A6E, 0xA0},//Set to "A0h"
	{0x3A70, 0x50},//Set to "50h"
	{0x3A8C, 0x04},//Set to "04h"
	{0x3A8D, 0x03},//Set to "03h"
	{0x3A8E, 0x09},//Set to "09h"
	{0x3A90, 0x38},//Set to "38h"
	{0x3A91, 0x42},//Set to "42h"
	{0x3A92, 0x3C},//Set to "3Ch"
	{0x3B0E, 0xF3},//Set to "F3h"
	{0x3B12, 0xE5},//Set to "E5h"
	{0x3B27, 0xC0},//Set to "C0h"
	{0x3B2E, 0xEF},//Set to "EFh"
	{0x3B30, 0x6A},//Set to "6Ah"
	{0x3B32, 0xF6},//Set to "F6h"
	{0x3B36, 0xE1},//Set to "E1h"
	{0x3B3A, 0xE8},//Set to "E8h"
	{0x3B5A, 0x17},//Set to "17h"
	{0x3B5E, 0xEF},//Set to "EFh"
	{0x3B60, 0x6A},//Set to "6Ah"
	{0x3B62, 0xF6},//Set to "F6h"
	{0x3B66, 0xE1},//Set to "E1h"
	{0x3B6A, 0xE8},//Set to "E8h"
	{0x3B88, 0xEC},//Set to "ECh"
	{0x3B8A, 0xED},//Set to "EDh"
	{0x3B94, 0x71},//Set to "71h"
	{0x3B96, 0x72},//Set to "72h"
	{0x3B98, 0xDE},//Set to "DEh"
	{0x3B9A, 0xDF},//Set to "DFh"
	{0x3C0F, 0x06},//Set to "06h"
	{0x3C10, 0x06},//Set to "06h"
	{0x3C11, 0x06},//Set to "06h"
	{0x3C12, 0x06},//Set to "06h"
	{0x3C13, 0x06},//Set to "06h"
	{0x3C18, 0x20},//Set to "20h"
	{0x3C37, 0x10},//When Clear HDR mode only  10h : Normal mode  30h : Clear HDR mode
	{0x3C3A, 0x7A},//Set to "7Ah"
	{0x3C40, 0xF4},//Set to "F4h"
	{0x3C48, 0xE6},//Set to "E6h"
	{0x3C54, 0xCE},//Set to "CEh"
	{0x3C56, 0xD0},//Set to "D0h"
	{0x3C6C, 0x53},//Set to "53h"
	{0x3C6E, 0x55},//Set to "55h"
	{0x3C70, 0xC0},//Set to "C0h"
	{0x3C72, 0xC2},//Set to "C2h"
	{0x3C7E, 0xCE},//Set to "CEh"
	{0x3C8C, 0xCF},//Set to "CFh"
	{0x3C8E, 0xEB},//Set to "EBh"
	{0x3C98, 0x54},//Set to "54h"
	{0x3C9A, 0x70},//Set to "70h"
	{0x3C9C, 0xC1},//Set to "C1h"
	{0x3C9E, 0xDD},//Set to "DDh"
	{0x3CB0, 0x7A},//Set to "7Ah"
	{0x3CB2, 0xBA},//Set to "BAh"
	{0x3CC8, 0xBC},//Set to "BCh"
	{0x3CCA, 0x7C},//Set to "7Ch"
	{0x3CD4, 0xEA},//Set to "EAh"
	{0x3CD5, 0x01},//Set to "01h"
	{0x3CD6, 0x4A},//Set to "4Ah"
	{0x3CD8, 0x00},//Set to "00h"
	{0x3CD9, 0x00},//Set to "00h"
	{0x3CDA, 0xFF},//Set to "FFh"
	{0x3CDB, 0x03},//Set to "03h"
	{0x3CDC, 0x00},//Set to "00h"
	{0x3CDD, 0x00},//Set to "00h"
	{0x3CDE, 0xFF},//Set to "FFh"
	{0x3CDF, 0x03},//Set to "03h"
	{0x3CE4, 0x4C},//Set to "4Ch"
	{0x3CE6, 0xEC},//Set to "ECh"
	{0x3CE7, 0x01},//Set to "01h"
	{0x3CE8, 0xFF},//Set to "FFh"
	{0x3CE9, 0x03},//Set to "03h"
	{0x3CEA, 0x00},//Set to "00h"
	{0x3CEB, 0x00},//Set to "00h"
	{0x3CEC, 0xFF},//Set to "FFh"
	{0x3CED, 0x03},//Set to "03h"
	{0x3CEE, 0x00},//Set to "00h"
	{0x3CEF, 0x00},//Set to "00h"
	{0x3CF2, 0xFF},//When Clear HDR mode only  FFh : Normal mode  78h : Clear HDR mode
	{0x3CF3, 0x03},//When Clear HDR mode only  03h : Normal mode  00h : Clear HDR mode
	{0x3CF4, 0x00},//When Clear HDR mode only  00h : Normal mode  A5h : Clear HDR mode AD10bit  AAh : Clear HDR mode AD12bit

	{0x3E28, 0x82},//Set to "82h"
	{0x3E2A, 0x80},//Set to "80h"
	{0x3E30, 0x85},//Set to "85h"
	{0x3E32, 0x7D},//Set to "7Dh"
	{0x3E5C, 0xCE},//Set to "CEh"
	{0x3E5E, 0xD3},//Set to "D3h"
	{0x3E70, 0x53},//Set to "53h"
	{0x3E72, 0x58},//Set to "58h"
	{0x3E74, 0xC0},//Set to "C0h"
	{0x3E76, 0xC5},//Set to "C5h"
	{0x3E78, 0xC0},//Set to "C0h"
	{0x3E79, 0x01},//Set to "01h"
	{0x3E7A, 0xD4},//Set to "D4h"
	{0x3E7B, 0x01},//Set to "01h"

	{0x3EB4, 0x0B},//0Bh : Normal mode  7Bh : Clear HDR mode
	{0x3EB5, 0x02},//02h : Normal mode  00h : Clear HDR mode
	{0x3EB6, 0x4D},//4Dh : Normal mode  A5h : Clear HDR mode AD10bit  AAh : Clear HDR mode AD12bit
	{0x3EB7, 0x42},//When Clear HDR mode only  42h : Normal mode  40h : Clear HDR mode

	{0x3EEC, 0xF3},//Set to "F3h"
	{0x3EEE, 0xE7},//Set to "E7h"
	{0x3F01, 0x01},//Set to "01h"
	{0x3F24, 0x10},//10h : Normal mode  17h : Clear HDR mode

	{0x3F28, 0x2D},//Set to "2Dh"
	{0x3F2A, 0x2D},//Set to "2Dh"
	{0x3F2C, 0x2D},//Set to "2Dh"
	{0x3F2E, 0x2D},//Set to "2Dh"
	{0x3F30, 0x23},//Set to "23h"
	{0x3F38, 0x2D},//Set to "2Dh"
	{0x3F3A, 0x2D},//Set to "2Dh"
	{0x3F3C, 0x2D},//Set to "2Dh"
	{0x3F3E, 0x28},//Set to "28h"
	{0x3F40, 0x1E},//Set to "1Eh"
	{0x3F48, 0x2D},//Set to "2Dh"
	{0x3F4A, 0x2D},//Set to "2Dh"
	{0x3F4C, 0x00},//00h : Normal mode  2Dh : Clear HDR mode

	{0x4004, 0xE4},//Set to "E4h"
	{0x4006, 0xFF},//Set to "FFh"
	{0x4018, 0x69},//Set to "69h"
	{0x401A, 0x84},//Set to "84h"
	{0x401C, 0xD6},//Set to "D6h"
	{0x401E, 0xF1},//Set to "F1h"
	{0x4038, 0xDE},//Set to "DEh"
	{0x403A, 0x00},//Set to "00h"
	{0x403B, 0x01},//Set to "01h"
	{0x404C, 0x63},//Set to "63h"
	{0x404E, 0x85},//Set to "85h"
	{0x4050, 0xD0},//Set to "D0h"
	{0x4052, 0xF2},//Set to "F2h"
	{0x4108, 0xDD},//Set to "DDh"
	{0x410A, 0xF7},//Set to "F7h"
	{0x411C, 0x62},//Set to "62h"
	{0x411E, 0x7C},//Set to "7Ch"
	{0x4120, 0xCF},//Set to "CFh"
	{0x4122, 0xE9},//Set to "E9h"
	{0x4138, 0xE6},//Set to "E6h"
	{0x413A, 0xF1},//Set to "F1h"
	{0x414C, 0x6B},//Set to "6Bh"
	{0x414E, 0x76},//Set to "76h"
	{0x4150, 0xD8},//Set to "D8h"
	{0x4152, 0xE3},//Set to "E3h"
	{0x417E, 0x03},//Set to "03h"
	{0x417F, 0x01},//Set to "01h"
	{0x4186, 0xE0},//Set to "E0h"
	{0x4190, 0xF3},//Set to "F3h"
	{0x4192, 0xF7},//Set to "F7h"
	{0x419C, 0x78},//Set to "78h"
	{0x419E, 0x7C},//Set to "7Ch"
	{0x41A0, 0xE5},//Set to "E5h"
	{0x41A2, 0xE9},//Set to "E9h"
	{0x41C8, 0xE2},//Set to "E2h"
	{0x41CA, 0xFD},//Set to "FDh"
	{0x41DC, 0x67},//Set to "67h"
	{0x41DE, 0x82},//Set to "82h"
	{0x41E0, 0xD4},//Set to "D4h"
	{0x41E2, 0xEF},//Set to "EFh"
	{0x4200, 0xDE},//Set to "DEh"
	{0x4202, 0xDA},//Set to "DAh"
	{0x4218, 0x63},//Set to "63h"
	{0x421A, 0x5F},//Set to "5Fh"
	{0x421C, 0xD0},//Set to "D0h"
	{0x421E, 0xCC},//Set to "CCh"
	{0x425A, 0x82},//Set to "82h"
	{0x425C, 0xEF},//Set to "EFh"
	{0x4348, 0xFE},//Set to "FEh"
	{0x4349, 0x06},//Set to "06h"
	{0x4352, 0xCE},//Set to "CEh"

	{0x4420, 0x0B},//0Bh : Normal mode  FFh : Clear HDR mode
	{0x4421, 0x02},//02h : Normal mode  03h : Clear HDR mode
	{0x4422, 0x4D},//4Dh : Normal mode  00h : Clear HDR mode
	{0x4423, 0x0A},//0Ah : Normal mode  08h : Clear HDR mode

	{0x4426, 0xF5},//Set to "F5h"
	{0x442A, 0xE7},//Set to "E7h"
	{0x4432, 0xF5},//Set to "F5h"
	{0x4436, 0xE7},//Set to "E7h"
	{0x4466, 0xB4},//Set to "B4h"
	{0x446E, 0x32},//Set to "32h"
	{0x449F, 0x1C},//Set to "1Ch"

	{0x44A4, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44A6, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44A8, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44AA, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44B4, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44B6, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44B8, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44BA, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44C4, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44C6, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44C8, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode

	{0x4506, 0xF3},//Set to "F3h"
	{0x450E, 0xE5},//Set to "E5h"
	{0x4516, 0xF3},//Set to "F3h"
	{0x4522, 0xE5},//Set to "E5h"
	{0x4524, 0xF3},//Set to "F3h"
	{0x452C, 0xE5},//Set to "E5h"
	{0x453C, 0x22},//Set to "22h"

	{0x453D, 0x1B},//1Bh : Normal mode  18h : Clear HDR mode
	{0x453E, 0x1B},//1Bh : Normal mode  18h : Clear HDR mode
	{0x453F, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4540, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4541, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4542, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4543, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4544, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4548, 0x00},//Set to "00h"
	{0x4549, 0x01},//01h : Normal mode  00h : Clear HDR mode
	{0x454A, 0x01},//01h : Normal mode  00h : Clear HDR mode
	{0x454B, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x454C, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x454D, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x454E, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x454F, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x4550, 0x06},//06h : Normal mode  04h : Clear HDR mode

	{0x4554, 0x55},//Set to "55h"
	{0x4555, 0x02},//Set to "02h"
	{0x4556, 0x42},//Set to "42h"
	{0x4557, 0x05},//Set to "05h"
	{0x4558, 0xFD},//Set to "FDh"
	{0x4559, 0x05},//Set to "05h"
	{0x455A, 0x94},//Set to "94h"
	{0x455B, 0x06},//Set to "06h"
	{0x455D, 0x06},//Set to "06h"
	{0x455E, 0x49},//Set to "49h"
	{0x455F, 0x07},//Set to "07h"
	{0x4560, 0x7F},//Set to "7Fh"
	{0x4561, 0x07},//Set to "07h"
	{0x4562, 0xA5},//Set to "A5h"
	{0x4564, 0x55},//Set to "55h"
	{0x4565, 0x02},//Set to "02h"
	{0x4566, 0x42},//Set to "42h"
	{0x4567, 0x05},//Set to "05h"
	{0x4568, 0xFD},//Set to "FDh"
	{0x4569, 0x05},//Set to "05h"
	{0x456A, 0x94},//Set to "94h"
	{0x456B, 0x06},//Set to "06h"
	{0x456D, 0x06},//Set to "06h"
	{0x456E, 0x49},//Set to "49h"
	{0x456F, 0x07},//Set to "07h"
	{0x4572, 0xA5},//Set to "A5h"
	{0x460C, 0x7D},//Set to "7Dh"
	{0x460E, 0xB1},//Set to "B1h"
	{0x4614, 0xA8},//Set to "A8h"
	{0x4616, 0xB2},//Set to "B2h"
	{0x461C, 0x7E},//Set to "7Eh"
	{0x461E, 0xA7},//Set to "A7h"
	{0x4624, 0xA8},//Set to "A8h"
	{0x4626, 0xB2},//Set to "B2h"
	{0x462C, 0x7E},//Set to "7Eh"
	{0x462E, 0x8A},//Set to "8Ah"
	{0x4630, 0x94},//Set to "94h"
	{0x4632, 0xA7},//Set to "A7h"
	{0x4634, 0xFB},//Set to "FBh"
	{0x4636, 0x2F},//Set to "2Fh"
	{0x4638, 0x81},//Set to "81h"
	{0x4639, 0x01},//Set to "01h"
	{0x463A, 0xB5},//Set to "B5h"
	{0x463B, 0x01},//Set to "01h"
	{0x463C, 0x26},//Set to "26h"
	{0x463E, 0x30},//Set to "30h"
	{0x4640, 0xAC},//Set to "ACh"
	{0x4641, 0x01},//Set to "01h"
	{0x4642, 0xB6},//Set to "B6h"
	{0x4643, 0x01},//Set to "01h"
	{0x4644, 0xFC},//Set to "FCh"
	{0x4646, 0x25},//Set to "25h"
	{0x4648, 0x82},//Set to "82h"
	{0x4649, 0x01},//Set to "01h"
	{0x464A, 0xAB},//Set to "ABh"
	{0x464B, 0x01},//Set to "01h"
	{0x464C, 0x26},//Set to "26h"
	{0x464E, 0x30},//Set to "30h"
	{0x4654, 0xFC},//Set to "FCh"
	{0x4656, 0x08},//Set to "08h"
	{0x4658, 0x12},//Set to "12h"
	{0x465A, 0x25},//Set to "25h"
	{0x4662, 0xFC},//Set to "FCh"
	{0x46A2, 0xFB},//Set to "FBh"
	{0x46D6, 0xF3},//Set to "F3h"
	{0x46E6, 0x00},//Set to "00h"
	{0x46E8, 0xFF},//Set to "FFh"
	{0x46E9, 0x03},//Set to "03h"
	{0x46EC, 0x7A},//Set to "7Ah"
	{0x46EE, 0xE5},//Set to "E5h"
	{0x46F4, 0xEE},//Set to "EEh"
	{0x46F6, 0xF2},//Set to "F2h"
	{0x470C, 0xFF},//Set to "FFh"
	{0x470D, 0x03},//Set to "03h"
	{0x470E, 0x00},//Set to "00h"
	{0x4714, 0xE0},//Set to "E0h"
	{0x4716, 0xE4},//Set to "E4h"
	{0x471E, 0xED},//Set to "EDh"
	{0x472E, 0x00},//Set to "00h"
	{0x4730, 0xFF},//Set to "FFh"
	{0x4731, 0x03},//Set to "03h"
	{0x4734, 0x7B},//Set to "7Bh"
	{0x4736, 0xDF},//Set to "DFh"
	{0x4754, 0x7D},//Set to "7Dh"
	{0x4756, 0x8B},//Set to "8Bh"
	{0x4758, 0x93},//Set to "93h"
	{0x475A, 0xB1},//Set to "B1h"
	{0x475C, 0xFB},//Set to "FBh"
	{0x475E, 0x09},//Set to "09h"
	{0x4760, 0x11},//Set to "11h"
	{0x4762, 0x2F},//Set to "2Fh"
	{0x4766, 0xCC},//Set to "CCh"
	{0x4776, 0xCB},//Set to "CBh"
	{0x477E, 0x4A},//Set to "4Ah"
	{0x478E, 0x49},//Set to "49h"
	{0x4794, 0x7C},//Set to "7Ch"
	{0x4796, 0x8F},//Set to "8Fh"
	{0x4798, 0xB3},//Set to "B3h"
	{0x4799, 0x00},//Set to "00h"
	{0x479A, 0xCC},//Set to "CCh"
	{0x479C, 0xC1},//Set to "C1h"
	{0x479E, 0xCB},//Set to "CBh"
	{0x47A4, 0x7D},//Set to "7Dh"
	{0x47A6, 0x8E},//Set to "8Eh"
	{0x47A8, 0xB4},//Set to "B4h"
	{0x47A9, 0x00},//Set to "00h"
	{0x47AA, 0xC0},//Set to "C0h"
	{0x47AC, 0xFA},//Set to "FAh"
	{0x47AE, 0x0D},//Set to "0Dh"
	{0x47B0, 0x31},//Set to "31h"
	{0x47B1, 0x01},//Set to "01h"
	{0x47B2, 0x4A},//Set to "4Ah"
	{0x47B3, 0x01},//Set to "01h"
	{0x47B4, 0x3F},//Set to "3Fh"
	{0x47B6, 0x49},//Set to "49h"
	{0x47BC, 0xFB},//Set to "FBh"
	{0x47BE, 0x0C},//Set to "0Ch"
	{0x47C0, 0x32},//Set to "32h"
	{0x47C1, 0x01},//Set to "01h"
	{0x47C2, 0x3E},//Set to "3Eh"
	{0x47C3, 0x01},//Set to "01h"

	{0x3000, 0x00},//operation
};

#if ClearHdr
//IMX678 Initial Setting for 10bit_3840x2160_p30 Clear HDR mode
//IMX678 ClearHDR
//All-pixel scan
//CSI-2_4lane
//Clock In 27MHz
//AD:10bit Output:10bit
//1440Mbps
//Master Mode
//ClearHDR VC
//frame rate 30fps
//Horizontal Clock : 550
//Vertical Line : 4500
static struct regval_list sensor_10b_3840x2160_p30clearhdr_regs[] = {
	{0x3000, 0x01},//Standby 0h : Operating 1h : Standby

	{0x3001, 0x00},//REGHOLD
	{0x3002, 0x00},//XMSTA  0h : Master mode
	{0x3014, 0x03},//INCK setting
	{0x3015, 0x03},//mipi bps
	{0x3018, 0x00},//00h: All-pixel mode
	{0x3019, 0x00},//Color filter mode setting 0h: RGB  (Only if using IMX678-AAQR1) 1h: MONO  (Only if using IMX678-AAMR1)
	{0x301A, 0x08},//HDR mode setting 00h : Normal mode 01h: DOL 2 frame mode 02h: DOL 3 frame mode 08h: Clear HDR mode
	{0x301B, 0x00},//ADDMODE[1:0]  0h: Non-binning
	{0x301C, 0x00},//XVS subsampling setting 00h: Disable (Normal) 01h: Enable (Subsampling)
	{0x301E, 0x01},//When DOL mode and Clear HDR mode only 00h: Line Information Output 01h: Virtual Channel Mode
	{0x3020, 0x00},//Horizontal direction
	{0x3021, 0x00},//Vertical direction
	{0x3022, 0x00},//10bit
	{0x3023, 0x00},//10bit
	{0x3028, 0x94},//VMAX
	{0x3029, 0x11},//VMAX
	{0x302A, 0x00},//VMAX
	{0x302C, 0x26},//HMAX
	{0x302D, 0x02},//HMAX
	{0x3030, 0x00},//FDG_SEL0
	{0x3031, 0x00},//FDG_SEL1
	{0x3032, 0x00},//FDG_SEL2
	{0x303C, 0x00},//PIX_HST
	{0x303D, 0x00},//PIX_HST
	{0x303E, 0x10},//PIX_HWIDTH
	{0x303F, 0x0F},//PIX_HWIDTH
	{0x3040, 0x03},//LANEMODE 3h: 4lane
	{0x3042, 0x00},//XSIZE_OVERLAP
	{0x3043, 0x00},//XSIZE_OVERLAP
	{0x3044, 0x00},//PIX_VST
	{0x3045, 0x00},//PIX_VST
	{0x3046, 0x84},//PIX_VWIDTH
	{0x3047, 0x08},//PIX_VWIDTH
	{0x3050, 0x06},//SHR0
	{0x3051, 0x00},//SHR0
	{0x3052, 0x00},//SHR0
	{0x3054, 0x0E},//SHR1
	{0x3055, 0x00},//SHR1
	{0x3056, 0x00},//SHR1
	{0x3058, 0x8A},//SHR2
	{0x3059, 0x01},//SHR2
	{0x305A, 0x00},//SHR2
	{0x3060, 0x16},//RHS1
	{0x3061, 0x01},//RHS1
	{0x3062, 0x00},//RHS1
	{0x3064, 0xC4},//RHS2
	{0x3065, 0x0C},//RHS2
	{0x3066, 0x00},//RHS2
	{0x3069, 0x01},//CHDR_GAIN_EN
	{0x306B, 0x04},//00h : Normal mode  04h : Clear HDR mode
	{0x3070, 0x00},//GAIN
	{0x3071, 0x00},//GAIN
	{0x3072, 0x00},//GAIN_1
	{0x3073, 0x00},//GAIN_1
	{0x3074, 0x00},//GAIN_2
	{0x3075, 0x00},//GAIN_2
	{0x3081, 0x02},//EXP_GAIN
	{0x308C, 0x00},//CHDR_DGAIN0_HG
	{0x308D, 0x01},//CHDR_DGAIN0_HG
	{0x3094, 0x00},//CHDR_AGAIN0_LG
	{0x3095, 0x00},//CHDR_AGAIN0_LG
	{0x309C, 0x00},//CHDR_AGAIN0_HG
	{0x309D, 0x00},//CHDR_AGAIN0_HG
	{0x30A4, 0xAA},
	{0x30A6, 0x00},
	{0x30CC, 0x00},
	{0x30CD, 0x00},
	{0x30DC, 0x32},//BLKLEVEL
	{0x30DD, 0x40},//BLKLEVEL
	{0x3400, 0x01},//GAIN_PGC_FIDMD
	{0x3460, 0x22},//Set to "22h"
	{0x355A, 0x00},//When Normal mode only 64h : Normal mode 00h : Clear HDR mode

	{0x3A02, 0x7A},//Set to "7Ah"
	{0x3A10, 0xEC},//Set to "ECh"
	{0x3A12, 0x71},//Set to "71h"
	{0x3A14, 0xDE},//Set to "DEh"
	{0x3A20, 0x2B},//Set to "2Bh"
	{0x3A24, 0x22},//Set to "22h"
	{0x3A25, 0x25},//Set to "25h"
	{0x3A26, 0x2A},//Set to "2Ah"
	{0x3A27, 0x2C},//Set to "2Ch"
	{0x3A28, 0x39},//Set to "39h"
	{0x3A29, 0x38},//Set to "38h"
	{0x3A30, 0x04},//Set to "04h"
	{0x3A31, 0x04},//Set to "04h"
	{0x3A32, 0x03},//Set to "03h"
	{0x3A33, 0x03},//Set to "03h"
	{0x3A34, 0x09},//Set to "09h"
	{0x3A35, 0x06},//Set to "06h"
	{0x3A38, 0xCD},//Set to "CDh"
	{0x3A3A, 0x4C},//Set to "4Ch"
	{0x3A3C, 0xB9},//Set to "B9h"
	{0x3A3E, 0x30},//Set to "30h"
	{0x3A40, 0x2C},//Set to "2Ch"
	{0x3A42, 0x39},//Set to "39h"
	{0x3A4E, 0x00},//Set to "00h"
	{0x3A52, 0x00},//Set to "00h"
	{0x3A56, 0x00},//Set to "00h"
	{0x3A5A, 0x00},//Set to "00h"
	{0x3A5E, 0x00},//Set to "00h"
	{0x3A62, 0x00},//Set to "00h"
	{0x3A64, 0x01},//When Clear HDR mode only 00h : Normal mode 01h : Clear HDR mode
	{0x3A6E, 0xA0},//Set to "A0h"
	{0x3A70, 0x50},//Set to "50h"
	{0x3A8C, 0x04},//Set to "04h"
	{0x3A8D, 0x03},//Set to "03h"
	{0x3A8E, 0x09},//Set to "09h"
	{0x3A90, 0x38},//Set to "38h"
	{0x3A91, 0x42},//Set to "42h"
	{0x3A92, 0x3C},//Set to "3Ch"
	{0x3B0E, 0xF3},//Set to "F3h"
	{0x3B12, 0xE5},//Set to "E5h"
	{0x3B27, 0xC0},//Set to "C0h"
	{0x3B2E, 0xEF},//Set to "EFh"
	{0x3B30, 0x6A},//Set to "6Ah"
	{0x3B32, 0xF6},//Set to "F6h"
	{0x3B36, 0xE1},//Set to "E1h"
	{0x3B3A, 0xE8},//Set to "E8h"
	{0x3B5A, 0x17},//Set to "17h"
	{0x3B5E, 0xEF},//Set to "EFh"
	{0x3B60, 0x6A},//Set to "6Ah"
	{0x3B62, 0xF6},//Set to "F6h"
	{0x3B66, 0xE1},//Set to "E1h"
	{0x3B6A, 0xE8},//Set to "E8h"
	{0x3B88, 0xEC},//Set to "ECh"
	{0x3B8A, 0xED},//Set to "EDh"
	{0x3B94, 0x71},//Set to "71h"
	{0x3B96, 0x72},//Set to "72h"
	{0x3B98, 0xDE},//Set to "DEh"
	{0x3B9A, 0xDF},//Set to "DFh"
	{0x3C0F, 0x06},//Set to "06h"
	{0x3C10, 0x06},//Set to "06h"
	{0x3C11, 0x06},//Set to "06h"
	{0x3C12, 0x06},//Set to "06h"
	{0x3C13, 0x06},//Set to "06h"
	{0x3C18, 0x20},//Set to "20h"
	{0x3C37, 0x30},//When Clear HDR mode only  10h : Normal mode  30h : Clear HDR mode
	{0x3C3A, 0x7A},//Set to "7Ah"
	{0x3C40, 0xF4},//Set to "F4h"
	{0x3C48, 0xE6},//Set to "E6h"
	{0x3C54, 0xCE},//Set to "CEh"
	{0x3C56, 0xD0},//Set to "D0h"
	{0x3C6C, 0x53},//Set to "53h"
	{0x3C6E, 0x55},//Set to "55h"
	{0x3C70, 0xC0},//Set to "C0h"
	{0x3C72, 0xC2},//Set to "C2h"
	{0x3C7E, 0xCE},//Set to "CEh"
	{0x3C8C, 0xCF},//Set to "CFh"
	{0x3C8E, 0xEB},//Set to "EBh"
	{0x3C98, 0x54},//Set to "54h"
	{0x3C9A, 0x70},//Set to "70h"
	{0x3C9C, 0xC1},//Set to "C1h"
	{0x3C9E, 0xDD},//Set to "DDh"
	{0x3CB0, 0x7A},//Set to "7Ah"
	{0x3CB2, 0xBA},//Set to "BAh"
	{0x3CC8, 0xBC},//Set to "BCh"
	{0x3CCA, 0x7C},//Set to "7Ch"
	{0x3CD4, 0xEA},//Set to "EAh"
	{0x3CD5, 0x01},//Set to "01h"
	{0x3CD6, 0x4A},//Set to "4Ah"
	{0x3CD8, 0x00},//Set to "00h"
	{0x3CD9, 0x00},//Set to "00h"
	{0x3CDA, 0xFF},//Set to "FFh"
	{0x3CDB, 0x03},//Set to "03h"
	{0x3CDC, 0x00},//Set to "00h"
	{0x3CDD, 0x00},//Set to "00h"
	{0x3CDE, 0xFF},//Set to "FFh"
	{0x3CDF, 0x03},//Set to "03h"
	{0x3CE4, 0x4C},//Set to "4Ch"
	{0x3CE6, 0xEC},//Set to "ECh"
	{0x3CE7, 0x01},//Set to "01h"
	{0x3CE8, 0xFF},//Set to "FFh"
	{0x3CE9, 0x03},//Set to "03h"
	{0x3CEA, 0x00},//Set to "00h"
	{0x3CEB, 0x00},//Set to "00h"
	{0x3CEC, 0xFF},//Set to "FFh"
	{0x3CED, 0x03},//Set to "03h"
	{0x3CEE, 0x00},//Set to "00h"
	{0x3CEF, 0x00},//Set to "00h"
	{0x3CF2, 0x78},//When Clear HDR mode only  FFh : Normal mode  78h : Clear HDR mode
	{0x3CF3, 0x00},//When Clear HDR mode only  03h : Normal mode  00h : Clear HDR mode
	{0x3CF4, 0xa5},//When Clear HDR mode only  00h : Normal mode  A5h : Clear HDR mode AD10bit  AAh : Clear HDR mode AD12bit

	{0x3E28, 0x82},//Set to "82h"
	{0x3E2A, 0x80},//Set to "80h"
	{0x3E30, 0x85},//Set to "85h"
	{0x3E32, 0x7D},//Set to "7Dh"
	{0x3E5C, 0xCE},//Set to "CEh"
	{0x3E5E, 0xD3},//Set to "D3h"
	{0x3E70, 0x53},//Set to "53h"
	{0x3E72, 0x58},//Set to "58h"
	{0x3E74, 0xC0},//Set to "C0h"
	{0x3E76, 0xC5},//Set to "C5h"
	{0x3E78, 0xC0},//Set to "C0h"
	{0x3E79, 0x01},//Set to "01h"
	{0x3E7A, 0xD4},//Set to "D4h"
	{0x3E7B, 0x01},//Set to "01h"

	{0x3EB4, 0x7B},//0Bh : Normal mode  7Bh : Clear HDR mode
	{0x3EB5, 0x00},//02h : Normal mode  00h : Clear HDR mode
	{0x3EB6, 0xA5},//4Dh : Normal mode  A5h : Clear HDR mode AD10bit  AAh : Clear HDR mode AD12bit
	{0x3EB7, 0x40},//When Clear HDR mode only  42h : Normal mode  40h : Clear HDR mode

	{0x3EEC, 0xF3},//Set to "F3h"
	{0x3EEE, 0xE7},//Set to "E7h"
	{0x3F01, 0x01},//Set to "01h"
	{0x3F24, 0x17},//10h : Normal mode  17h : Clear HDR mode

	{0x3F28, 0x2D},//Set to "2Dh"
	{0x3F2A, 0x2D},//Set to "2Dh"
	{0x3F2C, 0x2D},//Set to "2Dh"
	{0x3F2E, 0x2D},//Set to "2Dh"
	{0x3F30, 0x23},//Set to "23h"
	{0x3F38, 0x2D},//Set to "2Dh"
	{0x3F3A, 0x2D},//Set to "2Dh"
	{0x3F3C, 0x2D},//Set to "2Dh"
	{0x3F3E, 0x28},//Set to "28h"
	{0x3F40, 0x1E},//Set to "1Eh"
	{0x3F48, 0x2D},//Set to "2Dh"
	{0x3F4A, 0x2D},//Set to "2Dh"
	{0x3F4C, 0x2D},//00h : Normal mode  2Dh : Clear HDR mode

	{0x4004, 0xE4},//Set to "E4h"
	{0x4006, 0xFF},//Set to "FFh"
	{0x4018, 0x69},//Set to "69h"
	{0x401A, 0x84},//Set to "84h"
	{0x401C, 0xD6},//Set to "D6h"
	{0x401E, 0xF1},//Set to "F1h"
	{0x4038, 0xDE},//Set to "DEh"
	{0x403A, 0x00},//Set to "00h"
	{0x403B, 0x01},//Set to "01h"
	{0x404C, 0x63},//Set to "63h"
	{0x404E, 0x85},//Set to "85h"
	{0x4050, 0xD0},//Set to "D0h"
	{0x4052, 0xF2},//Set to "F2h"
	{0x4108, 0xDD},//Set to "DDh"
	{0x410A, 0xF7},//Set to "F7h"
	{0x411C, 0x62},//Set to "62h"
	{0x411E, 0x7C},//Set to "7Ch"
	{0x4120, 0xCF},//Set to "CFh"
	{0x4122, 0xE9},//Set to "E9h"
	{0x4138, 0xE6},//Set to "E6h"
	{0x413A, 0xF1},//Set to "F1h"
	{0x414C, 0x6B},//Set to "6Bh"
	{0x414E, 0x76},//Set to "76h"
	{0x4150, 0xD8},//Set to "D8h"
	{0x4152, 0xE3},//Set to "E3h"
	{0x417E, 0x03},//Set to "03h"
	{0x417F, 0x01},//Set to "01h"
	{0x4186, 0xE0},//Set to "E0h"
	{0x4190, 0xF3},//Set to "F3h"
	{0x4192, 0xF7},//Set to "F7h"
	{0x419C, 0x78},//Set to "78h"
	{0x419E, 0x7C},//Set to "7Ch"
	{0x41A0, 0xE5},//Set to "E5h"
	{0x41A2, 0xE9},//Set to "E9h"
	{0x41C8, 0xE2},//Set to "E2h"
	{0x41CA, 0xFD},//Set to "FDh"
	{0x41DC, 0x67},//Set to "67h"
	{0x41DE, 0x82},//Set to "82h"
	{0x41E0, 0xD4},//Set to "D4h"
	{0x41E2, 0xEF},//Set to "EFh"
	{0x4200, 0xDE},//Set to "DEh"
	{0x4202, 0xDA},//Set to "DAh"
	{0x4218, 0x63},//Set to "63h"
	{0x421A, 0x5F},//Set to "5Fh"
	{0x421C, 0xD0},//Set to "D0h"
	{0x421E, 0xCC},//Set to "CCh"
	{0x425A, 0x82},//Set to "82h"
	{0x425C, 0xEF},//Set to "EFh"
	{0x4348, 0xFE},//Set to "FEh"
	{0x4349, 0x06},//Set to "06h"
	{0x4352, 0xCE},//Set to "CEh"

	{0x4420, 0xFF},//0Bh : Normal mode  FFh : Clear HDR mode
	{0x4421, 0x03},//02h : Normal mode  03h : Clear HDR mode
	{0x4422, 0x00},//4Dh : Normal mode  00h : Clear HDR mode
	{0x4423, 0x08},//0Ah : Normal mode  08h : Clear HDR mode

	{0x4426, 0xF5},//Set to "F5h"
	{0x442A, 0xE7},//Set to "E7h"
	{0x4432, 0xF5},//Set to "F5h"
	{0x4436, 0xE7},//Set to "E7h"
	{0x4466, 0xB4},//Set to "B4h"
	{0x446E, 0x32},//Set to "32h"
	{0x449F, 0x1C},//Set to "1Ch"

	{0x44A4, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44A6, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44A8, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44AA, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44B4, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44B6, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44B8, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44BA, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44C4, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44C6, 0x37},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44C8, 0x37},//2Ch : Normal mode  37h : Clear HDR mode

	{0x4506, 0xF3},//Set to "F3h"
	{0x450E, 0xE5},//Set to "E5h"
	{0x4516, 0xF3},//Set to "F3h"
	{0x4522, 0xE5},//Set to "E5h"
	{0x4524, 0xF3},//Set to "F3h"
	{0x452C, 0xE5},//Set to "E5h"
	{0x453C, 0x22},//Set to "22h"

	{0x453D, 0x18},//1Bh : Normal mode  18h : Clear HDR mode
	{0x453E, 0x18},//1Bh : Normal mode  18h : Clear HDR mode
	{0x453F, 0x11},//15h : Normal mode  11h : Clear HDR mode
	{0x4540, 0x11},//15h : Normal mode  11h : Clear HDR mode
	{0x4541, 0x11},//15h : Normal mode  11h : Clear HDR mode
	{0x4542, 0x11},//15h : Normal mode  11h : Clear HDR mode
	{0x4543, 0x11},//15h : Normal mode  11h : Clear HDR mode
	{0x4544, 0x11},//15h : Normal mode  11h : Clear HDR mode
	{0x4548, 0x00},//Set to "00h"
	{0x4549, 0x00},//01h : Normal mode  00h : Clear HDR mode
	{0x454A, 0x00},//01h : Normal mode  00h : Clear HDR mode
	{0x454B, 0x04},//06h : Normal mode  04h : Clear HDR mode
	{0x454C, 0x04},//06h : Normal mode  04h : Clear HDR mode
	{0x454D, 0x04},//06h : Normal mode  04h : Clear HDR mode
	{0x454E, 0x04},//06h : Normal mode  04h : Clear HDR mode
	{0x454F, 0x04},//06h : Normal mode  04h : Clear HDR mode
	{0x4550, 0x04},//06h : Normal mode  04h : Clear HDR mode

	{0x4554, 0x55},//Set to "55h"
	{0x4555, 0x02},//Set to "02h"
	{0x4556, 0x42},//Set to "42h"
	{0x4557, 0x05},//Set to "05h"
	{0x4558, 0xFD},//Set to "FDh"
	{0x4559, 0x05},//Set to "05h"
	{0x455A, 0x94},//Set to "94h"
	{0x455B, 0x06},//Set to "06h"
	{0x455D, 0x06},//Set to "06h"
	{0x455E, 0x49},//Set to "49h"
	{0x455F, 0x07},//Set to "07h"
	{0x4560, 0x7F},//Set to "7Fh"
	{0x4561, 0x07},//Set to "07h"
	{0x4562, 0xA5},//Set to "A5h"
	{0x4564, 0x55},//Set to "55h"
	{0x4565, 0x02},//Set to "02h"
	{0x4566, 0x42},//Set to "42h"
	{0x4567, 0x05},//Set to "05h"
	{0x4568, 0xFD},//Set to "FDh"
	{0x4569, 0x05},//Set to "05h"
	{0x456A, 0x94},//Set to "94h"
	{0x456B, 0x06},//Set to "06h"
	{0x456D, 0x06},//Set to "06h"
	{0x456E, 0x49},//Set to "49h"
	{0x456F, 0x07},//Set to "07h"
	{0x4572, 0xA5},//Set to "A5h"
	{0x460C, 0x7D},//Set to "7Dh"
	{0x460E, 0xB1},//Set to "B1h"
	{0x4614, 0xA8},//Set to "A8h"
	{0x4616, 0xB2},//Set to "B2h"
	{0x461C, 0x7E},//Set to "7Eh"
	{0x461E, 0xA7},//Set to "A7h"
	{0x4624, 0xA8},//Set to "A8h"
	{0x4626, 0xB2},//Set to "B2h"
	{0x462C, 0x7E},//Set to "7Eh"
	{0x462E, 0x8A},//Set to "8Ah"
	{0x4630, 0x94},//Set to "94h"
	{0x4632, 0xA7},//Set to "A7h"
	{0x4634, 0xFB},//Set to "FBh"
	{0x4636, 0x2F},//Set to "2Fh"
	{0x4638, 0x81},//Set to "81h"
	{0x4639, 0x01},//Set to "01h"
	{0x463A, 0xB5},//Set to "B5h"
	{0x463B, 0x01},//Set to "01h"
	{0x463C, 0x26},//Set to "26h"
	{0x463E, 0x30},//Set to "30h"
	{0x4640, 0xAC},//Set to "ACh"
	{0x4641, 0x01},//Set to "01h"
	{0x4642, 0xB6},//Set to "B6h"
	{0x4643, 0x01},//Set to "01h"
	{0x4644, 0xFC},//Set to "FCh"
	{0x4646, 0x25},//Set to "25h"
	{0x4648, 0x82},//Set to "82h"
	{0x4649, 0x01},//Set to "01h"
	{0x464A, 0xAB},//Set to "ABh"
	{0x464B, 0x01},//Set to "01h"
	{0x464C, 0x26},//Set to "26h"
	{0x464E, 0x30},//Set to "30h"
	{0x4654, 0xFC},//Set to "FCh"
	{0x4656, 0x08},//Set to "08h"
	{0x4658, 0x12},//Set to "12h"
	{0x465A, 0x25},//Set to "25h"
	{0x4662, 0xFC},//Set to "FCh"
	{0x46A2, 0xFB},//Set to "FBh"
	{0x46D6, 0xF3},//Set to "F3h"
	{0x46E6, 0x00},//Set to "00h"
	{0x46E8, 0xFF},//Set to "FFh"
	{0x46E9, 0x03},//Set to "03h"
	{0x46EC, 0x7A},//Set to "7Ah"
	{0x46EE, 0xE5},//Set to "E5h"
	{0x46F4, 0xEE},//Set to "EEh"
	{0x46F6, 0xF2},//Set to "F2h"
	{0x470C, 0xFF},//Set to "FFh"
	{0x470D, 0x03},//Set to "03h"
	{0x470E, 0x00},//Set to "00h"
	{0x4714, 0xE0},//Set to "E0h"
	{0x4716, 0xE4},//Set to "E4h"
	{0x471E, 0xED},//Set to "EDh"
	{0x472E, 0x00},//Set to "00h"
	{0x4730, 0xFF},//Set to "FFh"
	{0x4731, 0x03},//Set to "03h"
	{0x4734, 0x7B},//Set to "7Bh"
	{0x4736, 0xDF},//Set to "DFh"
	{0x4754, 0x7D},//Set to "7Dh"
	{0x4756, 0x8B},//Set to "8Bh"
	{0x4758, 0x93},//Set to "93h"
	{0x475A, 0xB1},//Set to "B1h"
	{0x475C, 0xFB},//Set to "FBh"
	{0x475E, 0x09},//Set to "09h"
	{0x4760, 0x11},//Set to "11h"
	{0x4762, 0x2F},//Set to "2Fh"
	{0x4766, 0xCC},//Set to "CCh"
	{0x4776, 0xCB},//Set to "CBh"
	{0x477E, 0x4A},//Set to "4Ah"
	{0x478E, 0x49},//Set to "49h"
	{0x4794, 0x7C},//Set to "7Ch"
	{0x4796, 0x8F},//Set to "8Fh"
	{0x4798, 0xB3},//Set to "B3h"
	{0x4799, 0x00},//Set to "00h"
	{0x479A, 0xCC},//Set to "CCh"
	{0x479C, 0xC1},//Set to "C1h"
	{0x479E, 0xCB},//Set to "CBh"
	{0x47A4, 0x7D},//Set to "7Dh"
	{0x47A6, 0x8E},//Set to "8Eh"
	{0x47A8, 0xB4},//Set to "B4h"
	{0x47A9, 0x00},//Set to "00h"
	{0x47AA, 0xC0},//Set to "C0h"
	{0x47AC, 0xFA},//Set to "FAh"
	{0x47AE, 0x0D},//Set to "0Dh"
	{0x47B0, 0x31},//Set to "31h"
	{0x47B1, 0x01},//Set to "01h"
	{0x47B2, 0x4A},//Set to "4Ah"
	{0x47B3, 0x01},//Set to "01h"
	{0x47B4, 0x3F},//Set to "3Fh"
	{0x47B6, 0x49},//Set to "49h"
	{0x47BC, 0xFB},//Set to "FBh"
	{0x47BE, 0x0C},//Set to "0Ch"
	{0x47C0, 0x32},//Set to "32h"
	{0x47C1, 0x01},//Set to "01h"
	{0x47C2, 0x3E},//Set to "3Eh"
	{0x47C3, 0x01},//Set to "01h"

	{0x3000, 0x00},//operation
};

#else
//IMX678 Initial Setting for 10bit_3840x2160_p30 DOL 2frame mode
//"All-pixel scan
//CSI-2_4lane
//Clock In 27MHz
//AD:10bit Output:10bit
//1440Mbps
//Master Mode
//LCG Mode
//DOL HDR 2frame VC
//frame rate 30fps
//Horizontal Clock : 550
//Vertical Line : 2250
static struct regval_list sensor_10b_3840x2160_p30dol_regs[] = {
	{0x3000, 0x01},//Standby 0h : Operating 1h : Standby

	{0x3001, 0x00},//REGHOLD
	{0x3002, 0x00},//XMSTA  0h : Master mode
	{0x3014, 0x03},//INCK setting
	{0x3015, 0x03},//mipi bps
	{0x3018, 0x00},//00h: All-pixel mode
	{0x3019, 0x00},//Color filter mode setting 0h: RGB  (Only if using IMX678-AAQR1) 1h: MONO  (Only if using IMX678-AAMR1)
	{0x301A, 0x01},//HDR mode setting 00h : Normal mode 01h: DOL 2 frame mode 02h: DOL 3 frame mode 08h: Clear HDR mode
	{0x301B, 0x00},//ADDMODE[1:0]  0h: Non-binning
	{0x301C, 0x01},//XVS subsampling setting 00h: Disable (Normal) 01h: Enable (Subsampling)
	{0x301E, 0x01},//When DOL mode and Clear HDR mode only 00h: Line Information Output 01h: Virtual Channel Mode
	{0x3020, 0x00},//Horizontal direction
	{0x3021, 0x00},//Vertical direction
	{0x3022, 0x00},//10bit
	{0x3023, 0x00},//10bit
	{0x3028, 0xCA},//VMAX
	{0x3029, 0x08},//VMAX
	{0x302A, 0x00},//VMAX
	{0x302C, 0x26},//HMAX
	{0x302D, 0x02},//HMAX
	{0x3030, 0x00},//FDG_SEL0
	{0x3031, 0x00},//FDG_SEL1
	{0x3032, 0x00},//FDG_SEL2
	{0x303C, 0x00},//PIX_HST
	{0x303D, 0x00},//PIX_HST
	{0x303E, 0x10},//PIX_HWIDTH
	{0x303F, 0x0F},//PIX_HWIDTH
	{0x3040, 0x03},//LANEMODE 3h: 4lane
	{0x3042, 0x00},//XSIZE_OVERLAP
	{0x3043, 0x00},//XSIZE_OVERLAP
	{0x3044, 0x00},//PIX_VST
	{0x3045, 0x00},//PIX_VST
	{0x3046, 0x84},//PIX_VWIDTH
	{0x3047, 0x08},//PIX_VWIDTH
	{0x3050, 0xEC},//SHR0
	{0x3051, 0x04},//SHR0
	{0x3052, 0x00},//SHR0
	{0x3054, 0x05},//SHR1
	{0x3055, 0x00},//SHR1
	{0x3056, 0x00},//SHR1
	{0x3058, 0x8A},//SHR2
	{0x3059, 0x01},//SHR2
	{0x305A, 0x00},//SHR2
	{0x3060, DOL_RHS1&0xff},//RHS1
	{0x3061, (DOL_RHS1>>8)&0xff},//RHS1
	{0x3062, (DOL_RHS1>>16)&0xff},//RHS1
	{0x3064, 0xC4},//RHS2
	{0x3065, 0x0C},//RHS2
	{0x3066, 0x00},//RHS2
	{0x3069, 0x00},//CHDR_GAIN_EN
	{0x306B, 0x00},//00h : Normal mode  04h : Clear HDR mode
	{0x3070, 0x00},//GAIN
	{0x3071, 0x00},//GAIN
	{0x3072, 0x00},//GAIN_1
	{0x3073, 0x00},//GAIN_1
	{0x3074, 0x00},//GAIN_2
	{0x3075, 0x00},//GAIN_2
	{0x3081, 0x00},//EXP_GAIN
	{0x308C, 0x00},//CHDR_DGAIN0_HG
	{0x308D, 0x01},//CHDR_DGAIN0_HG
	{0x3094, 0x00},//CHDR_AGAIN0_LG
	{0x3095, 0x00},//CHDR_AGAIN0_LG
	{0x309C, 0x00},//CHDR_AGAIN0_HG
	{0x309D, 0x00},//CHDR_AGAIN0_HG
	{0x30A4, 0xAA},
	{0x30A6, 0x00},
	{0x30CC, 0x00},
	{0x30CD, 0x00},
	{0x30DC, 0x32},//BLKLEVEL
	{0x30DD, 0x40},//BLKLEVEL
	{0x3400, 0x01},//GAIN_PGC_FIDMD
	{0x3460, 0x22},//Set to "22h"
	{0x355A, 0x64},//When Normal mode only 64h : Normal mode 00h : Clear HDR mode

	{0x3A02, 0x7A},//Set to "7Ah"
	{0x3A10, 0xEC},//Set to "ECh"
	{0x3A12, 0x71},//Set to "71h"
	{0x3A14, 0xDE},//Set to "DEh"
	{0x3A20, 0x2B},//Set to "2Bh"
	{0x3A24, 0x22},//Set to "22h"
	{0x3A25, 0x25},//Set to "25h"
	{0x3A26, 0x2A},//Set to "2Ah"
	{0x3A27, 0x2C},//Set to "2Ch"
	{0x3A28, 0x39},//Set to "39h"
	{0x3A29, 0x38},//Set to "38h"
	{0x3A30, 0x04},//Set to "04h"
	{0x3A31, 0x04},//Set to "04h"
	{0x3A32, 0x03},//Set to "03h"
	{0x3A33, 0x03},//Set to "03h"
	{0x3A34, 0x09},//Set to "09h"
	{0x3A35, 0x06},//Set to "06h"
	{0x3A38, 0xCD},//Set to "CDh"
	{0x3A3A, 0x4C},//Set to "4Ch"
	{0x3A3C, 0xB9},//Set to "B9h"
	{0x3A3E, 0x30},//Set to "30h"
	{0x3A40, 0x2C},//Set to "2Ch"
	{0x3A42, 0x39},//Set to "39h"
	{0x3A4E, 0x00},//Set to "00h"
	{0x3A52, 0x00},//Set to "00h"
	{0x3A56, 0x00},//Set to "00h"
	{0x3A5A, 0x00},//Set to "00h"
	{0x3A5E, 0x00},//Set to "00h"
	{0x3A62, 0x00},//Set to "00h"
	{0x3A64, 0x00},//When Clear HDR mode only 00h : Normal mode 01h : Clear HDR mode
	{0x3A6E, 0xA0},//Set to "A0h"
	{0x3A70, 0x50},//Set to "50h"
	{0x3A8C, 0x04},//Set to "04h"
	{0x3A8D, 0x03},//Set to "03h"
	{0x3A8E, 0x09},//Set to "09h"
	{0x3A90, 0x38},//Set to "38h"
	{0x3A91, 0x42},//Set to "42h"
	{0x3A92, 0x3C},//Set to "3Ch"
	{0x3B0E, 0xF3},//Set to "F3h"
	{0x3B12, 0xE5},//Set to "E5h"
	{0x3B27, 0xC0},//Set to "C0h"
	{0x3B2E, 0xEF},//Set to "EFh"
	{0x3B30, 0x6A},//Set to "6Ah"
	{0x3B32, 0xF6},//Set to "F6h"
	{0x3B36, 0xE1},//Set to "E1h"
	{0x3B3A, 0xE8},//Set to "E8h"
	{0x3B5A, 0x17},//Set to "17h"
	{0x3B5E, 0xEF},//Set to "EFh"
	{0x3B60, 0x6A},//Set to "6Ah"
	{0x3B62, 0xF6},//Set to "F6h"
	{0x3B66, 0xE1},//Set to "E1h"
	{0x3B6A, 0xE8},//Set to "E8h"
	{0x3B88, 0xEC},//Set to "ECh"
	{0x3B8A, 0xED},//Set to "EDh"
	{0x3B94, 0x71},//Set to "71h"
	{0x3B96, 0x72},//Set to "72h"
	{0x3B98, 0xDE},//Set to "DEh"
	{0x3B9A, 0xDF},//Set to "DFh"
	{0x3C0F, 0x06},//Set to "06h"
	{0x3C10, 0x06},//Set to "06h"
	{0x3C11, 0x06},//Set to "06h"
	{0x3C12, 0x06},//Set to "06h"
	{0x3C13, 0x06},//Set to "06h"
	{0x3C18, 0x20},//Set to "20h"
	{0x3C37, 0x10},//When Clear HDR mode only  10h : Normal mode  30h : Clear HDR mode
	{0x3C3A, 0x7A},//Set to "7Ah"
	{0x3C40, 0xF4},//Set to "F4h"
	{0x3C48, 0xE6},//Set to "E6h"
	{0x3C54, 0xCE},//Set to "CEh"
	{0x3C56, 0xD0},//Set to "D0h"
	{0x3C6C, 0x53},//Set to "53h"
	{0x3C6E, 0x55},//Set to "55h"
	{0x3C70, 0xC0},//Set to "C0h"
	{0x3C72, 0xC2},//Set to "C2h"
	{0x3C7E, 0xCE},//Set to "CEh"
	{0x3C8C, 0xCF},//Set to "CFh"
	{0x3C8E, 0xEB},//Set to "EBh"
	{0x3C98, 0x54},//Set to "54h"
	{0x3C9A, 0x70},//Set to "70h"
	{0x3C9C, 0xC1},//Set to "C1h"
	{0x3C9E, 0xDD},//Set to "DDh"
	{0x3CB0, 0x7A},//Set to "7Ah"
	{0x3CB2, 0xBA},//Set to "BAh"
	{0x3CC8, 0xBC},//Set to "BCh"
	{0x3CCA, 0x7C},//Set to "7Ch"
	{0x3CD4, 0xEA},//Set to "EAh"
	{0x3CD5, 0x01},//Set to "01h"
	{0x3CD6, 0x4A},//Set to "4Ah"
	{0x3CD8, 0x00},//Set to "00h"
	{0x3CD9, 0x00},//Set to "00h"
	{0x3CDA, 0xFF},//Set to "FFh"
	{0x3CDB, 0x03},//Set to "03h"
	{0x3CDC, 0x00},//Set to "00h"
	{0x3CDD, 0x00},//Set to "00h"
	{0x3CDE, 0xFF},//Set to "FFh"
	{0x3CDF, 0x03},//Set to "03h"
	{0x3CE4, 0x4C},//Set to "4Ch"
	{0x3CE6, 0xEC},//Set to "ECh"
	{0x3CE7, 0x01},//Set to "01h"
	{0x3CE8, 0xFF},//Set to "FFh"
	{0x3CE9, 0x03},//Set to "03h"
	{0x3CEA, 0x00},//Set to "00h"
	{0x3CEB, 0x00},//Set to "00h"
	{0x3CEC, 0xFF},//Set to "FFh"
	{0x3CED, 0x03},//Set to "03h"
	{0x3CEE, 0x00},//Set to "00h"
	{0x3CEF, 0x00},//Set to "00h"
	{0x3CF2, 0xFF},//When Clear HDR mode only  FFh : Normal mode  78h : Clear HDR mode
	{0x3CF3, 0x03},//When Clear HDR mode only  03h : Normal mode  00h : Clear HDR mode
	{0x3CF4, 0x00},//When Clear HDR mode only  00h : Normal mode  A5h : Clear HDR mode AD10bit  AAh : Clear HDR mode AD12bit

	{0x3E28, 0x82},//Set to "82h"
	{0x3E2A, 0x80},//Set to "80h"
	{0x3E30, 0x85},//Set to "85h"
	{0x3E32, 0x7D},//Set to "7Dh"
	{0x3E5C, 0xCE},//Set to "CEh"
	{0x3E5E, 0xD3},//Set to "D3h"
	{0x3E70, 0x53},//Set to "53h"
	{0x3E72, 0x58},//Set to "58h"
	{0x3E74, 0xC0},//Set to "C0h"
	{0x3E76, 0xC5},//Set to "C5h"
	{0x3E78, 0xC0},//Set to "C0h"
	{0x3E79, 0x01},//Set to "01h"
	{0x3E7A, 0xD4},//Set to "D4h"
	{0x3E7B, 0x01},//Set to "01h"

	{0x3EB4, 0x0B},//0Bh : Normal mode  7Bh : Clear HDR mode
	{0x3EB5, 0x02},//02h : Normal mode  00h : Clear HDR mode
	{0x3EB6, 0x4D},//4Dh : Normal mode  A5h : Clear HDR mode AD10bit  AAh : Clear HDR mode AD12bit
	{0x3EB7, 0x42},//When Clear HDR mode only  42h : Normal mode  40h : Clear HDR mode

	{0x3EEC, 0xF3},//Set to "F3h"
	{0x3EEE, 0xE7},//Set to "E7h"
	{0x3F01, 0x01},//Set to "01h"
	{0x3F24, 0x10},//10h : Normal mode  17h : Clear HDR mode

	{0x3F28, 0x2D},//Set to "2Dh"
	{0x3F2A, 0x2D},//Set to "2Dh"
	{0x3F2C, 0x2D},//Set to "2Dh"
	{0x3F2E, 0x2D},//Set to "2Dh"
	{0x3F30, 0x23},//Set to "23h"
	{0x3F38, 0x2D},//Set to "2Dh"
	{0x3F3A, 0x2D},//Set to "2Dh"
	{0x3F3C, 0x2D},//Set to "2Dh"
	{0x3F3E, 0x28},//Set to "28h"
	{0x3F40, 0x1E},//Set to "1Eh"
	{0x3F48, 0x2D},//Set to "2Dh"
	{0x3F4A, 0x2D},//Set to "2Dh"
	{0x3F4C, 0x00},//00h : Normal mode  2Dh : Clear HDR mode

	{0x4004, 0xE4},//Set to "E4h"
	{0x4006, 0xFF},//Set to "FFh"
	{0x4018, 0x69},//Set to "69h"
	{0x401A, 0x84},//Set to "84h"
	{0x401C, 0xD6},//Set to "D6h"
	{0x401E, 0xF1},//Set to "F1h"
	{0x4038, 0xDE},//Set to "DEh"
	{0x403A, 0x00},//Set to "00h"
	{0x403B, 0x01},//Set to "01h"
	{0x404C, 0x63},//Set to "63h"
	{0x404E, 0x85},//Set to "85h"
	{0x4050, 0xD0},//Set to "D0h"
	{0x4052, 0xF2},//Set to "F2h"
	{0x4108, 0xDD},//Set to "DDh"
	{0x410A, 0xF7},//Set to "F7h"
	{0x411C, 0x62},//Set to "62h"
	{0x411E, 0x7C},//Set to "7Ch"
	{0x4120, 0xCF},//Set to "CFh"
	{0x4122, 0xE9},//Set to "E9h"
	{0x4138, 0xE6},//Set to "E6h"
	{0x413A, 0xF1},//Set to "F1h"
	{0x414C, 0x6B},//Set to "6Bh"
	{0x414E, 0x76},//Set to "76h"
	{0x4150, 0xD8},//Set to "D8h"
	{0x4152, 0xE3},//Set to "E3h"
	{0x417E, 0x03},//Set to "03h"
	{0x417F, 0x01},//Set to "01h"
	{0x4186, 0xE0},//Set to "E0h"
	{0x4190, 0xF3},//Set to "F3h"
	{0x4192, 0xF7},//Set to "F7h"
	{0x419C, 0x78},//Set to "78h"
	{0x419E, 0x7C},//Set to "7Ch"
	{0x41A0, 0xE5},//Set to "E5h"
	{0x41A2, 0xE9},//Set to "E9h"
	{0x41C8, 0xE2},//Set to "E2h"
	{0x41CA, 0xFD},//Set to "FDh"
	{0x41DC, 0x67},//Set to "67h"
	{0x41DE, 0x82},//Set to "82h"
	{0x41E0, 0xD4},//Set to "D4h"
	{0x41E2, 0xEF},//Set to "EFh"
	{0x4200, 0xDE},//Set to "DEh"
	{0x4202, 0xDA},//Set to "DAh"
	{0x4218, 0x63},//Set to "63h"
	{0x421A, 0x5F},//Set to "5Fh"
	{0x421C, 0xD0},//Set to "D0h"
	{0x421E, 0xCC},//Set to "CCh"
	{0x425A, 0x82},//Set to "82h"
	{0x425C, 0xEF},//Set to "EFh"
	{0x4348, 0xFE},//Set to "FEh"
	{0x4349, 0x06},//Set to "06h"
	{0x4352, 0xCE},//Set to "CEh"

	{0x4420, 0x0B},//0Bh : Normal mode  FFh : Clear HDR mode
	{0x4421, 0x02},//02h : Normal mode  03h : Clear HDR mode
	{0x4422, 0x4D},//4Dh : Normal mode  00h : Clear HDR mode
	{0x4423, 0x0A},//0Ah : Normal mode  08h : Clear HDR mode

	{0x4426, 0xF5},//Set to "F5h"
	{0x442A, 0xE7},//Set to "E7h"
	{0x4432, 0xF5},//Set to "F5h"
	{0x4436, 0xE7},//Set to "E7h"
	{0x4466, 0xB4},//Set to "B4h"
	{0x446E, 0x32},//Set to "32h"
	{0x449F, 0x1C},//Set to "1Ch"

	{0x44A4, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44A6, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44A8, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44AA, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44B4, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44B6, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44B8, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44BA, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44C4, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44C6, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode
	{0x44C8, 0x2C},//2Ch : Normal mode  37h : Clear HDR mode

	{0x4506, 0xF3},//Set to "F3h"
	{0x450E, 0xE5},//Set to "E5h"
	{0x4516, 0xF3},//Set to "F3h"
	{0x4522, 0xE5},//Set to "E5h"
	{0x4524, 0xF3},//Set to "F3h"
	{0x452C, 0xE5},//Set to "E5h"
	{0x453C, 0x22},//Set to "22h"

	{0x453D, 0x1B},//1Bh : Normal mode  18h : Clear HDR mode
	{0x453E, 0x1B},//1Bh : Normal mode  18h : Clear HDR mode
	{0x453F, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4540, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4541, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4542, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4543, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4544, 0x15},//15h : Normal mode  11h : Clear HDR mode
	{0x4548, 0x00},//Set to "00h"
	{0x4549, 0x01},//01h : Normal mode  00h : Clear HDR mode
	{0x454A, 0x01},//01h : Normal mode  00h : Clear HDR mode
	{0x454B, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x454C, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x454D, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x454E, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x454F, 0x06},//06h : Normal mode  04h : Clear HDR mode
	{0x4550, 0x06},//06h : Normal mode  04h : Clear HDR mode

	{0x4554, 0x55},//Set to "55h"
	{0x4555, 0x02},//Set to "02h"
	{0x4556, 0x42},//Set to "42h"
	{0x4557, 0x05},//Set to "05h"
	{0x4558, 0xFD},//Set to "FDh"
	{0x4559, 0x05},//Set to "05h"
	{0x455A, 0x94},//Set to "94h"
	{0x455B, 0x06},//Set to "06h"
	{0x455D, 0x06},//Set to "06h"
	{0x455E, 0x49},//Set to "49h"
	{0x455F, 0x07},//Set to "07h"
	{0x4560, 0x7F},//Set to "7Fh"
	{0x4561, 0x07},//Set to "07h"
	{0x4562, 0xA5},//Set to "A5h"
	{0x4564, 0x55},//Set to "55h"
	{0x4565, 0x02},//Set to "02h"
	{0x4566, 0x42},//Set to "42h"
	{0x4567, 0x05},//Set to "05h"
	{0x4568, 0xFD},//Set to "FDh"
	{0x4569, 0x05},//Set to "05h"
	{0x456A, 0x94},//Set to "94h"
	{0x456B, 0x06},//Set to "06h"
	{0x456D, 0x06},//Set to "06h"
	{0x456E, 0x49},//Set to "49h"
	{0x456F, 0x07},//Set to "07h"
	{0x4572, 0xA5},//Set to "A5h"
	{0x460C, 0x7D},//Set to "7Dh"
	{0x460E, 0xB1},//Set to "B1h"
	{0x4614, 0xA8},//Set to "A8h"
	{0x4616, 0xB2},//Set to "B2h"
	{0x461C, 0x7E},//Set to "7Eh"
	{0x461E, 0xA7},//Set to "A7h"
	{0x4624, 0xA8},//Set to "A8h"
	{0x4626, 0xB2},//Set to "B2h"
	{0x462C, 0x7E},//Set to "7Eh"
	{0x462E, 0x8A},//Set to "8Ah"
	{0x4630, 0x94},//Set to "94h"
	{0x4632, 0xA7},//Set to "A7h"
	{0x4634, 0xFB},//Set to "FBh"
	{0x4636, 0x2F},//Set to "2Fh"
	{0x4638, 0x81},//Set to "81h"
	{0x4639, 0x01},//Set to "01h"
	{0x463A, 0xB5},//Set to "B5h"
	{0x463B, 0x01},//Set to "01h"
	{0x463C, 0x26},//Set to "26h"
	{0x463E, 0x30},//Set to "30h"
	{0x4640, 0xAC},//Set to "ACh"
	{0x4641, 0x01},//Set to "01h"
	{0x4642, 0xB6},//Set to "B6h"
	{0x4643, 0x01},//Set to "01h"
	{0x4644, 0xFC},//Set to "FCh"
	{0x4646, 0x25},//Set to "25h"
	{0x4648, 0x82},//Set to "82h"
	{0x4649, 0x01},//Set to "01h"
	{0x464A, 0xAB},//Set to "ABh"
	{0x464B, 0x01},//Set to "01h"
	{0x464C, 0x26},//Set to "26h"
	{0x464E, 0x30},//Set to "30h"
	{0x4654, 0xFC},//Set to "FCh"
	{0x4656, 0x08},//Set to "08h"
	{0x4658, 0x12},//Set to "12h"
	{0x465A, 0x25},//Set to "25h"
	{0x4662, 0xFC},//Set to "FCh"
	{0x46A2, 0xFB},//Set to "FBh"
	{0x46D6, 0xF3},//Set to "F3h"
	{0x46E6, 0x00},//Set to "00h"
	{0x46E8, 0xFF},//Set to "FFh"
	{0x46E9, 0x03},//Set to "03h"
	{0x46EC, 0x7A},//Set to "7Ah"
	{0x46EE, 0xE5},//Set to "E5h"
	{0x46F4, 0xEE},//Set to "EEh"
	{0x46F6, 0xF2},//Set to "F2h"
	{0x470C, 0xFF},//Set to "FFh"
	{0x470D, 0x03},//Set to "03h"
	{0x470E, 0x00},//Set to "00h"
	{0x4714, 0xE0},//Set to "E0h"
	{0x4716, 0xE4},//Set to "E4h"
	{0x471E, 0xED},//Set to "EDh"
	{0x472E, 0x00},//Set to "00h"
	{0x4730, 0xFF},//Set to "FFh"
	{0x4731, 0x03},//Set to "03h"
	{0x4734, 0x7B},//Set to "7Bh"
	{0x4736, 0xDF},//Set to "DFh"
	{0x4754, 0x7D},//Set to "7Dh"
	{0x4756, 0x8B},//Set to "8Bh"
	{0x4758, 0x93},//Set to "93h"
	{0x475A, 0xB1},//Set to "B1h"
	{0x475C, 0xFB},//Set to "FBh"
	{0x475E, 0x09},//Set to "09h"
	{0x4760, 0x11},//Set to "11h"
	{0x4762, 0x2F},//Set to "2Fh"
	{0x4766, 0xCC},//Set to "CCh"
	{0x4776, 0xCB},//Set to "CBh"
	{0x477E, 0x4A},//Set to "4Ah"
	{0x478E, 0x49},//Set to "49h"
	{0x4794, 0x7C},//Set to "7Ch"
	{0x4796, 0x8F},//Set to "8Fh"
	{0x4798, 0xB3},//Set to "B3h"
	{0x4799, 0x00},//Set to "00h"
	{0x479A, 0xCC},//Set to "CCh"
	{0x479C, 0xC1},//Set to "C1h"
	{0x479E, 0xCB},//Set to "CBh"
	{0x47A4, 0x7D},//Set to "7Dh"
	{0x47A6, 0x8E},//Set to "8Eh"
	{0x47A8, 0xB4},//Set to "B4h"
	{0x47A9, 0x00},//Set to "00h"
	{0x47AA, 0xC0},//Set to "C0h"
	{0x47AC, 0xFA},//Set to "FAh"
	{0x47AE, 0x0D},//Set to "0Dh"
	{0x47B0, 0x31},//Set to "31h"
	{0x47B1, 0x01},//Set to "01h"
	{0x47B2, 0x4A},//Set to "4Ah"
	{0x47B3, 0x01},//Set to "01h"
	{0x47B4, 0x3F},//Set to "3Fh"
	{0x47B6, 0x49},//Set to "49h"
	{0x47BC, 0xFB},//Set to "FBh"
	{0x47BE, 0x0C},//Set to "0Ch"
	{0x47C0, 0x32},//Set to "32h"
	{0x47C1, 0x01},//Set to "01h"
	{0x47C2, 0x3E},//Set to "3Eh"
	{0x47C3, 0x01},//Set to "01h"

	{0x3000, 0x00},//operation
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

static int imx678_sensor_vts;
static int imx678_sensor_svr;
static int shutter_delay = 1;
static int shutter_delay_cnt;
static int fps_change_flag;

static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	data_type explow, expmid, exphigh;
	int exptime,  exp_val_m;
	struct sensor_info *info = to_state(sd);

#if ClearHdr
		exptime = imx678_sensor_vts - (exp_val >> 8) - 1;
		if (exptime < 3)
			exptime = 3;
		exphigh = (unsigned char)((0x00f0000 & exptime) >> 16);
		expmid =  (unsigned char)((0x000ff00 & exptime) >> 8);
		explow =  (unsigned char)((0x00000ff & exptime));

		// IMX678 SHR0
		sensor_write(sd, 0x3050, explow);
		sensor_write(sd, 0x3051, expmid);
		sensor_write(sd, 0x3052, exphigh);
		sensor_dbg("sensor_set_exp = %d %d line Done!\n", exp_val, exptime);
#else
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
		// LEF
		exptime = (imx678_sensor_vts<<1) - (exp_val>>4);
		if (exptime < DOL_RHS1 + 5) {
			exptime = DOL_RHS1 + 5;
			exp_val = ((imx678_sensor_vts << 1) - exptime) << 4;
		}
		sensor_dbg("long exp_val: %d, exptime: %d\n", exp_val, exptime);

		exphigh	= (unsigned char) ((0x00f0000 & exptime) >> 16);
		expmid	= (unsigned char) ((0x000ff00 & exptime) >> 8);
		explow	= (unsigned char) ((0x00000ff & exptime));

		// IMX678 SHR0
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

		// IMX678 SHR1
		sensor_write(sd, 0x3054, explow);
		sensor_write(sd, 0x3055, expmid);
		sensor_write(sd, 0x3056, exphigh);
	} else {
		exptime = imx678_sensor_vts - (exp_val >> 4) - 1;
		if (exptime < 3)
			exptime = 3;
		exphigh = (unsigned char)((0x00f0000 & exptime) >> 16);
		expmid =  (unsigned char)((0x000ff00 & exptime) >> 8);
		explow =  (unsigned char)((0x00000ff & exptime));

		// IMX678 SHR0
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
	int gain_val_h = gain_val * 6;  //DOL_RATIO/2.666
	int gain_reg_h_d = 256;
	int gain_reg_h = 1024, gain_val_l = 1024;
	if (gain_val < 1 * 16)
		gain_val = 16;
#if ClearHdr
	if (info->isp_wdr_mode == ISP_DOL_WDR_MODE) {
			gain_reg_l = ((gain_val<<11) - 32768) / gain_val;
			if (gain_val_h < 443) {
				gain_reg_h = ((gain_val_h<<11) - 32768) / gain_val_h;
				gain_reg_h_d = 256;
			} else {
				gain_reg_h = 1974;
				gain_reg_h_d = (gain_val_h<<8) / 442;
			}
			sensor_write(sd, 0x308C, gain_reg_h_d&0xff);
			sensor_write(sd, 0x308D, gain_reg_h_d>>8);
			sensor_write(sd, 0x3094, gain_reg_l&0xff);
			sensor_write(sd, 0x3095, gain_reg_l>>8);
			sensor_write(sd, 0x309C, gain_reg_h&0xff);
			sensor_write(sd, 0x309D, gain_reg_h>>8);
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
			//imx678 VMAX[0:19]
			sensor_write(sd, 0x3028, imx678_sensor_vts / (imx678_sensor_svr + 1) & 0xFF);
			sensor_write(sd, 0x3029, imx678_sensor_vts / (imx678_sensor_svr + 1) >> 8 & 0xFF);
			sensor_write(sd, 0x302a, imx678_sensor_vts / (imx678_sensor_svr + 1) >> 16);
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
	imx678_sensor_vts = wsize->pclk/fps->fps/wsize->hts;
	fps_change_flag = 1;
	sensor_write(sd, 0x3001, 1);

	sensor_read(sd, 0x3028, &rdval1);
	sensor_read(sd, 0x3029, &rdval2);
	sensor_read(sd, 0x302a, &rdval3);

	sensor_dbg("imx678_sensor_svr: %d, vts: %d.\n", imx678_sensor_svr, (rdval1 | (rdval2<<8) | (rdval3<<16)));
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
		.width = 3840,
		.height = 2160,
		.hoffset = 0,
		.voffset = 0,
		.hts = 550,
		.vts = 4500,
		.pclk = 74250000 * 16,
		.mipi_bps = 1440 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		//.lp_mode = SENSOR_LP_DISCONTINUOUS,
		.if_mode = MIPI_VC_WDR_MODE,
		.wdr_mode = ISP_DOL_WDR_MODE,
		.intg_min = 1 << 4,
		.intg_max = (4500  - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 16 << 4,
		.regs = sensor_10b_3840x2160_p30clearhdr_regs,
		.regs_size = ARRAY_SIZE(sensor_10b_3840x2160_p30clearhdr_regs),
		.set_size = NULL,
		.top_clk = 432000000,
		.isp_clk = 297000000,
	},
#else
	//DOL HDR
	{
		.width = 3840,
		.height = 2160,
		.hoffset = 0,
		.voffset = 0,
		.hts = 550,
		.vts = 2250,
		.pclk = 74250000,
		.mipi_bps = 1440 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		//.lp_mode = SENSOR_LP_DISCONTINUOUS,
		.if_mode = MIPI_VC_WDR_MODE,
		.wdr_mode = ISP_DOL_WDR_MODE,
		.intg_min = 1 << 4,
		.intg_max = (2250 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 2000 << 4,
		.regs = sensor_10b_3840x2160_p30dol_regs,
		.regs_size = ARRAY_SIZE(sensor_10b_3840x2160_p30dol_regs),
		.set_size = NULL,
		.top_clk = 432000000,
		.isp_clk = 297000000,
	},
#endif

	{
		.width = 3840,
		.height = 2160,
		.hoffset = 0,
		.voffset = 0,
		.hts = 1100,
		.vts = 2250,
		.pclk = 74250000,
		.mipi_bps = 891 * 1000 * 1000,
		.fps_fixed = 30,
		.bin_factor = 1,
		//.lp_mode = SENSOR_LP_DISCONTINUOUS,
		.intg_min = 1 << 4,
		.intg_max = (2250  - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 2000 << 4,
		.regs = sensor_10b_3840x2160_p30_regs,
		.regs_size = ARRAY_SIZE(sensor_10b_3840x2160_p30_regs),
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
	imx678_sensor_vts = wsize->vts;
	sensor_read(sd, 0x300E, &rdval_l);
	sensor_read(sd, 0x300F, &rdval_h);
	imx678_sensor_svr = (rdval_h << 8) | rdval_l;
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
