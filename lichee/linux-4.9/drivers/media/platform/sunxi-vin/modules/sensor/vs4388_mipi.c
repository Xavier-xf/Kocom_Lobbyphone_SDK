/*
 * A V4L2 driver for sc4336 Raw cameras.
 *
 * Copyright (c) 2021 by Allwinnertech Co., Ltd.  http://www.allwinnertech.com
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

MODULE_AUTHOR("lwj");
MODULE_DESCRIPTION("A low-level driver for vs4388 sensors");
MODULE_LICENSE("GPL");

#define MCLK				(24 * 1000 * 1000)
#define V4L2_IDENT_SENSOR	0x4388
#define SENSOR_HOR_VER_CFG0_REG1 1

/*
 * The vs4388 i2c address
 */
#define I2C_ADDR			0x6c      //0x60   0x64

#define SENSOR_NUM			0x1
#define SENSOR_NAME			"vs4388_mipi"

static int vs4388_sensor_vts;

#ifdef CONFIG_SENSOR_VS4388_8BIT_MIPI
#define RAW8 1 //raw8 select
#else
#define RAW8 0 //raw10 select
#endif

/*
 * The default register settings
 */
static struct regval_list sensor_default_regs[] = {

};

static struct regval_list sensor_2560x1440p10_regs[] = {
	{0x4601, 0x40},
	{0x5001, 0xc1},
	{0x5000, 0x69},
	{0x3810, 0x05},
	{0x3811, 0xb2},
	{0x3812, 0x11},
	{0x3813, 0x5e},//vts :4446
	{0x3809, 0x00},
	{0x380b, 0x06},
	{0x4003, 0x40},
	{0x4004, 0x03},
	{0x4000, 0x0b},
	{0x4001, 0x40},
	{0x4007, 0x68},
	{0x400c, 0x02},
	{0x400d, 0x07},
	{0x400e, 0x32},
	{0x2050, 0x00},
	{0x2051, 0x05},
	{0x2052, 0x06},
	{0x4104, 0x01},
	{0x4101, 0x0c},
	{0x2148, 0xf0},
	{0x2149, 0x7f},
	{0x2150, 0xe0},
	{0x2151, 0x3f},
	{0x2153, 0x80},
	{0x2154, 0xff},
	{0x2155, 0x01},
	{0x2158, 0xc0},
	{0x2159, 0x1f},
	{0x215a, 0xf8},
	{0x215b, 0x0f},
	{0x215c, 0x1f},
	{0x215d, 0xfc},
	{0x215e, 0x00},
	{0x1111, 0xc8},
	{0x1112, 0x97},
	{0x2138, 0x7c},
	{0x2168, 0x1f},
	{0x2169, 0x00},
	{0x1020, 0x16},
	{0x103a, 0x10},
	{0x103b, 0x10},
	{0x103c, 0x10},
	{0x1020, 0x18},
	{0x21d4, 0xff},
	{0x21d5, 0x07},
	{0x21b8, 0xff},
	{0x21b9, 0x0f},
	{0x202d, 0x00},
	{0x202e, 0x00},
	{0x202f, 0x00},
	{0x219f, 0x7f},
	{0x2199, 0x7f},
	{0x103f, 0x06},
	{0x103e, 0x05},
	{0x103d, 0x05},
	{0x1021, 0x05},
	{0x2030, 0x27},
	{0x2031, 0x0f},
	{0x1101, 0x02},
	{0x1100, 0x28},
	{0x201f, 0x2a},
	{0x202c, 0x03},
	{0x1018, 0x00},
	{0x1019, 0x08},
	{0x101a, 0x0b},
	{0x101b, 0x0c},
	{0x1025, 0xf7},
	{0x1024, 0x4e},
	{0x2012, 0x0b},
	{0x2013, 0x0d},
	{0x2016, 0x0f},
	{0x2017, 0x10},
	{0x201c, 0x13},
	{0x201d, 0x31},
	{0x201a, 0x2b},
	{0x2023, 0x36},
	{0x2024, 0x20},
	{0x2025, 0x06},
	{0x2027, 0x06},
	{0x2028, 0x0f},
	{0x202a, 0xbd},
	{0x202b, 0xaa},
	{0x2026, 0x3a},
	{0x2021, 0xb0},
	{0x2003, 0x10},
	{0x2006, 0x00},
	{0x2007, 0x20},
	{0x2008, 0x00},
	{0x2009, 0x00},
	{0x200a, 0x11},
#if RAW8
	{0x3017, 0x24},
	{0x4305, 0x6a},
#else
	{0x3017, 0x04},
	{0x4305, 0x2b},
#endif
	{0x3511, 0x0c},
	{0x3520, 0x06},
	{0x3521, 0x03},
	{0x3522, 0x03},
	{0x3512, 0x12},
	{0x3513, 0x30},
	{0x3514, 0x54},
	{0x3515, 0x06},
	{0x1113, 0x01},
	{0x1028, 0x32},
	{0x1141, 0x40},
	{0x1010, 0x0b},
	{0x1011, 0x16},
	{0x1012, 0x29},
	{0x1013, 0x3f},
	{0x1014, 0x06},
	{0x1015, 0x04},
	{0x1016, 0x02},
	{0x1017, 0x00},
	{0x101c, 0x8c},
	{0x101d, 0x8c},
	{0x101e, 0x0c},
	{0x101f, 0x0c},
	{0x2032, 0x1f},
	{0x2033, 0x11},
	{0x2034, 0x13},
	{0x2035, 0x1d},
	{0x2036, 0x2a},
	{0x2037, 0x31},
	{0x1110, 0x1a},
	{0x1200, 0x1b},
	{0x1201, 0x5d},
	{0x1202, 0x15},
	{0x1203, 0x44},
	{0x1204, 0x01},
#if RAW8
	{0x1205, 0x16},
#else
	{0x1205, 0x1b},
#endif
	{0x1206, 0x5d},
	{0x1207, 0x05},
	{0x3500, 0x05},
	{0x3501, 0xc0},
	{0x3509, 0x10},
	{0x1221, 0xe3},
	{0x3816, 0x10},//flip
	{0x380b, 0x01},//flip
	{0x3809, 0x00},//flip
	{0x4101, 0x0c},//flip
	{0x0100, 0x01},
};

static struct regval_list sensor_2560x1440p30_regs[] = {
	{0x4601, 0x40},
	{0x5001, 0xc1},
	{0x5000, 0x69},
	{0x3810, 0x05},
	{0x3811, 0xb2},
	{0x3812, 0x05},
	{0x3813, 0xca},//vts :1482
	{0x3809, 0x00},
	{0x380b, 0x06},
	{0x4003, 0x40},
	{0x4004, 0x03},
	{0x4000, 0x0b},
	{0x4001, 0x40},
	{0x4007, 0x68},
	{0x400c, 0x02},
	{0x400d, 0x07},
	{0x400e, 0x32},
	{0x2050, 0x00},
	{0x2051, 0x05},
	{0x2052, 0x06},
	{0x4104, 0x01},
	{0x4101, 0x0c},
	{0x2148, 0xf0},
	{0x2149, 0x7f},
	{0x2150, 0xe0},
	{0x2151, 0x3f},
	{0x2153, 0x80},
	{0x2154, 0xff},
	{0x2155, 0x01},
	{0x2158, 0xc0},
	{0x2159, 0x1f},
	{0x215a, 0xf8},
	{0x215b, 0x0f},
	{0x215c, 0x1f},
	{0x215d, 0xfc},
	{0x215e, 0x00},
	{0x1111, 0xc8},
	{0x1112, 0x97},
	{0x2138, 0x7c},
	{0x2168, 0x1f},
	{0x2169, 0x00},
	{0x1020, 0x16},
	{0x103a, 0x10},
	{0x103b, 0x10},
	{0x103c, 0x10},
	{0x1020, 0x18},
	{0x21d4, 0xff},
	{0x21d5, 0x07},
	{0x21b8, 0xff},
	{0x21b9, 0x0f},
	{0x202d, 0x00},
	{0x202e, 0x00},
	{0x202f, 0x00},
	{0x219f, 0x7f},
	{0x2199, 0x7f},
	{0x103f, 0x06},
	{0x103e, 0x05},
	{0x103d, 0x05},
	{0x1021, 0x05},
	{0x2030, 0x27},
	{0x2031, 0x0f},
	{0x1101, 0x02},
	{0x1100, 0x28},
	{0x201f, 0x2a},
	{0x202c, 0x03},
	{0x1018, 0x00},
	{0x1019, 0x08},
	{0x101a, 0x0b},
	{0x101b, 0x0c},
	{0x1025, 0xf7},
	{0x1024, 0x4e},
	{0x2012, 0x0b},
	{0x2013, 0x0d},
	{0x2016, 0x0f},
	{0x2017, 0x10},
	{0x201c, 0x13},
	{0x201d, 0x31},
	{0x201a, 0x2b},
	{0x2023, 0x36},
	{0x2024, 0x20},
	{0x2025, 0x06},
	{0x2027, 0x06},
	{0x2028, 0x0f},
	{0x202a, 0xbd},
	{0x202b, 0xaa},
	{0x2026, 0x3a},
	{0x2021, 0xb0},
	{0x2003, 0x10},
	{0x2006, 0x00},
	{0x2007, 0x20},
	{0x2008, 0x00},
	{0x2009, 0x00},
	{0x200a, 0x11},
#if RAW8
	{0x3017, 0x24},
	{0x4305, 0x6a},
#else
	{0x3017, 0x04},
	{0x4305, 0x2b},
#endif
	{0x3511, 0x0c},
	{0x3520, 0x06},
	{0x3521, 0x03},
	{0x3522, 0x03},
	{0x3512, 0x12},
	{0x3513, 0x30},
	{0x3514, 0x54},
	{0x3515, 0x06},
	{0x1113, 0x01},
	{0x1028, 0x32},
	{0x1141, 0x40},
	{0x1010, 0x0b},
	{0x1011, 0x16},
	{0x1012, 0x29},
	{0x1013, 0x3f},
	{0x1014, 0x06},
	{0x1015, 0x04},
	{0x1016, 0x02},
	{0x1017, 0x00},
	{0x101c, 0x8c},
	{0x101d, 0x8c},
	{0x101e, 0x0c},
	{0x101f, 0x0c},
	{0x2032, 0x1f},
	{0x2033, 0x11},
	{0x2034, 0x13},
	{0x2035, 0x1d},
	{0x2036, 0x2a},
	{0x2037, 0x31},
	{0x1110, 0x1a},
	{0x1200, 0x1b},
	{0x1201, 0x5d},
	{0x1202, 0x15},
	{0x1203, 0x44},
	{0x1204, 0x01},
#if RAW8
	{0x1205, 0x16},
#else
	{0x1205, 0x1b},
#endif
	{0x1206, 0x5d},
	{0x1207, 0x05},
	{0x3500, 0x05},
	{0x3501, 0xc0},
	{0x3509, 0x10},
	{0x1221, 0xe3},
	{0x3816, 0x10},//flip
	{0x380b, 0x01},//flip
	{0x3809, 0x00},//flip
	{0x4101, 0x0c},//flip
	{0x0100, 0x01},
};

static struct regval_list sensor_2560x1440p10_regs_raw8[] = {
	{0x4601, 0x40},
	{0x5001, 0xc1},
	{0x5000, 0x69},
	{0x3810, 0x05},
	{0x3811, 0xb2},
	{0x3812, 0x11},
	{0x3813, 0x5e},//vts :4446
	{0x3809, 0x00},
	{0x380b, 0x06},
	{0x4003, 0x40},
	{0x4004, 0x03},
	{0x4000, 0x0b},
	{0x4001, 0x40},
	{0x4007, 0x68},
	{0x400c, 0x02},
	{0x400d, 0x07},
	{0x400e, 0x32},
	{0x2050, 0x00},
	{0x2051, 0x05},
	{0x2052, 0x06},
	{0x4104, 0x01},
	{0x4101, 0x0c},
	{0x2148, 0xf0},
	{0x2149, 0x7f},
	{0x2150, 0xe0},
	{0x2151, 0x3f},
	{0x2153, 0x80},
	{0x2154, 0xff},
	{0x2155, 0x01},
	{0x2158, 0xc0},
	{0x2159, 0x1f},
	{0x215a, 0xf8},
	{0x215b, 0x0f},
	{0x215c, 0x1f},
	{0x215d, 0xfc},
	{0x215e, 0x00},
	{0x1111, 0xc8},
	{0x1112, 0x97},
	{0x2138, 0x7c},
	{0x2168, 0x1f},
	{0x2169, 0x00},
	{0x1020, 0x16},
	{0x103a, 0x10},
	{0x103b, 0x10},
	{0x103c, 0x10},
	{0x1020, 0x18},
	{0x21d4, 0xff},
	{0x21d5, 0x07},
	{0x21b8, 0xff},
	{0x21b9, 0x0f},
	{0x202d, 0x00},
	{0x202e, 0x00},
	{0x202f, 0x00},
	{0x219f, 0x7f},
	{0x2199, 0x7f},
	{0x103f, 0x06},
	{0x103e, 0x05},
	{0x103d, 0x05},
	{0x1021, 0x05},
	{0x2030, 0x27},
	{0x2031, 0x0f},
	{0x1101, 0x02},
	{0x1100, 0x28},
	{0x201f, 0x2a},
	{0x202c, 0x03},
	{0x1018, 0x00},
	{0x1019, 0x08},
	{0x101a, 0x0b},
	{0x101b, 0x0c},
	{0x1025, 0xf7},
	{0x1024, 0x4e},
	{0x2012, 0x0b},
	{0x2013, 0x0d},
	{0x2016, 0x0f},
	{0x2017, 0x10},
	{0x201c, 0x13},
	{0x201d, 0x31},
	{0x201a, 0x2b},
	{0x2023, 0x36},
	{0x2024, 0x20},
	{0x2025, 0x06},
	{0x2027, 0x06},
	{0x2028, 0x0f},
	{0x202a, 0xbd},
	{0x202b, 0xaa},
	{0x2026, 0x3a},
	{0x2021, 0xb0},
	{0x2003, 0x10},
	{0x2006, 0x00},
	{0x2007, 0x20},
	{0x2008, 0x00},
	{0x2009, 0x00},
	{0x200a, 0x11},
	{0x3017, 0x24},
	{0x4305, 0x6a},
	{0x3511, 0x0c},
	{0x3520, 0x06},
	{0x3521, 0x03},
	{0x3522, 0x03},
	{0x3512, 0x12},
	{0x3513, 0x30},
	{0x3514, 0x54},
	{0x3515, 0x06},
	{0x1113, 0x01},
	{0x1028, 0x32},
	{0x1141, 0x40},
	{0x1010, 0x0b},
	{0x1011, 0x16},
	{0x1012, 0x29},
	{0x1013, 0x3f},
	{0x1014, 0x06},
	{0x1015, 0x04},
	{0x1016, 0x02},
	{0x1017, 0x00},
	{0x101c, 0x8c},
	{0x101d, 0x8c},
	{0x101e, 0x0c},
	{0x101f, 0x0c},
	{0x2032, 0x1f},
	{0x2033, 0x11},
	{0x2034, 0x13},
	{0x2035, 0x1d},
	{0x2036, 0x2a},
	{0x2037, 0x31},
	{0x1110, 0x1a},
	{0x1200, 0x1b},
	{0x1201, 0x5d},
	{0x1202, 0x15},
	{0x1203, 0x44},
	{0x1204, 0x01},
	{0x1205, 0x16},
	{0x1206, 0x5d},
	{0x1207, 0x05},
	{0x3500, 0x05},
	{0x3501, 0xc0},
	{0x3509, 0x10},
	{0x1221, 0xe3},
	{0x3816, 0x10},//flip
	{0x380b, 0x01},//flip
	{0x3809, 0x00},//flip
	{0x4101, 0x0c},//flip
	{0x0100, 0x01},
};

static struct regval_list sensor_2560x1440p30_regs_raw10[] = {
	{0x4601, 0x40},
	{0x5001, 0xc1},
	{0x5000, 0x69},
	{0x3810, 0x05},
	{0x3811, 0xb2},
	{0x3812, 0x05},
	{0x3813, 0xca},//vts :1482
	{0x3809, 0x00},
	{0x380b, 0x06},
	{0x4003, 0x40},
	{0x4004, 0x03},
	{0x4000, 0x0b},
	{0x4001, 0x40},
	{0x4007, 0x68},
	{0x400c, 0x02},
	{0x400d, 0x07},
	{0x400e, 0x32},
	{0x2050, 0x00},
	{0x2051, 0x05},
	{0x2052, 0x06},
	{0x4104, 0x01},
	{0x4101, 0x0c},
	{0x2148, 0xf0},
	{0x2149, 0x7f},
	{0x2150, 0xe0},
	{0x2151, 0x3f},
	{0x2153, 0x80},
	{0x2154, 0xff},
	{0x2155, 0x01},
	{0x2158, 0xc0},
	{0x2159, 0x1f},
	{0x215a, 0xf8},
	{0x215b, 0x0f},
	{0x215c, 0x1f},
	{0x215d, 0xfc},
	{0x215e, 0x00},
	{0x1111, 0xc8},
	{0x1112, 0x97},
	{0x2138, 0x7c},
	{0x2168, 0x1f},
	{0x2169, 0x00},
	{0x1020, 0x16},
	{0x103a, 0x10},
	{0x103b, 0x10},
	{0x103c, 0x10},
	{0x1020, 0x18},
	{0x21d4, 0xff},
	{0x21d5, 0x07},
	{0x21b8, 0xff},
	{0x21b9, 0x0f},
	{0x202d, 0x00},
	{0x202e, 0x00},
	{0x202f, 0x00},
	{0x219f, 0x7f},
	{0x2199, 0x7f},
	{0x103f, 0x06},
	{0x103e, 0x05},
	{0x103d, 0x05},
	{0x1021, 0x05},
	{0x2030, 0x27},
	{0x2031, 0x0f},
	{0x1101, 0x02},
	{0x1100, 0x28},
	{0x201f, 0x2a},
	{0x202c, 0x03},
	{0x1018, 0x00},
	{0x1019, 0x08},
	{0x101a, 0x0b},
	{0x101b, 0x0c},
	{0x1025, 0xf7},
	{0x1024, 0x4e},
	{0x2012, 0x0b},
	{0x2013, 0x0d},
	{0x2016, 0x0f},
	{0x2017, 0x10},
	{0x201c, 0x13},
	{0x201d, 0x31},
	{0x201a, 0x2b},
	{0x2023, 0x36},
	{0x2024, 0x20},
	{0x2025, 0x06},
	{0x2027, 0x06},
	{0x2028, 0x0f},
	{0x202a, 0xbd},
	{0x202b, 0xaa},
	{0x2026, 0x3a},
	{0x2021, 0xb0},
	{0x2003, 0x10},
	{0x2006, 0x00},
	{0x2007, 0x20},
	{0x2008, 0x00},
	{0x2009, 0x00},
	{0x200a, 0x11},
	{0x3017, 0x04},
	{0x4305, 0x2b},
	{0x3511, 0x0c},
	{0x3520, 0x06},
	{0x3521, 0x03},
	{0x3522, 0x03},
	{0x3512, 0x12},
	{0x3513, 0x30},
	{0x3514, 0x54},
	{0x3515, 0x06},
	{0x1113, 0x01},
	{0x1028, 0x32},
	{0x1141, 0x40},
	{0x1010, 0x0b},
	{0x1011, 0x16},
	{0x1012, 0x29},
	{0x1013, 0x3f},
	{0x1014, 0x06},
	{0x1015, 0x04},
	{0x1016, 0x02},
	{0x1017, 0x00},
	{0x101c, 0x8c},
	{0x101d, 0x8c},
	{0x101e, 0x0c},
	{0x101f, 0x0c},
	{0x2032, 0x1f},
	{0x2033, 0x11},
	{0x2034, 0x13},
	{0x2035, 0x1d},
	{0x2036, 0x2a},
	{0x2037, 0x31},
	{0x1110, 0x1a},
	{0x1200, 0x1b},
	{0x1201, 0x5d},
	{0x1202, 0x15},
	{0x1203, 0x44},
	{0x1204, 0x01},
	{0x1205, 0x1b},
	{0x1206, 0x5d},
	{0x1207, 0x05},
	{0x3500, 0x05},
	{0x3501, 0xc0},
	{0x3509, 0x10},
	{0x1221, 0xe3},
	{0x3816, 0x10},//flip
	{0x380b, 0x01},//flip
	{0x3809, 0x00},//flip
	{0x4101, 0x0c},//flip
	{0x0100, 0x01},
};

/*
 * Here we'll try to encapsulate the changes for just the output
 * video format.
 *
 */

static struct regval_list sensor_fmt_raw[] = {

};

static int sensor_g_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
	data_type frame_length = 0, act_vts = 0;

	sensor_read(sd, 0x3812, &frame_length);
	act_vts = frame_length << 8;
	sensor_read(sd, 0x3813, &frame_length);
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

static int vs4388_sensor_vts;
static int vs4388_fps_change_flag;
static int sensor_s_exp(struct v4l2_subdev *sd, unsigned int exp_val)
{
	struct sensor_info *info = to_state(sd);
	int tmp_exp_val = exp_val / 16;

	sensor_dbg("exp_val:%d\n", exp_val);
	sensor_write(sd, 0x3500, (tmp_exp_val >> 8) & 0xFF);
	sensor_write(sd, 0x3501, (tmp_exp_val & 0xFF));

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

static int sensor_s_gain(struct v4l2_subdev *sd, int gain_val)
{
	struct sensor_info *info = to_state(sd);
	sensor_dbg("gain_val:%d\n", gain_val);
	sensor_write(sd, 0x3508, (gain_val >> 8) & 0xFF);
	sensor_write(sd, 0x3509, (gain_val & 0xFF));
	info->gain = gain_val;

	return 0;
}

static int sensor_s_exp_gain(struct v4l2_subdev *sd,
			     struct sensor_exp_gain *exp_gain)
{
	struct sensor_info *info = to_state(sd);
	int shutter, frame_length;
	int exp_val, gain_val;
//	data_type read_high = 0, read_low = 0;

	exp_val = exp_gain->exp_val;
	gain_val = exp_gain->gain_val;
	if (gain_val < 1 * 16)
		gain_val = 16;
	if (exp_val > 0xfffff)
		exp_val = 0xfffff;

	if (!vs4388_fps_change_flag) {
		shutter = exp_val >> 4;
		if (shutter > vs4388_sensor_vts - 4) {//12
			frame_length = shutter + 4;
		} else
			frame_length = vs4388_sensor_vts;
		sensor_write(sd, 0x3813, (frame_length & 0xff));
		sensor_write(sd, 0x3812, (frame_length >> 8));

	}

	sensor_s_exp(sd, exp_val);
	sensor_s_gain(sd, gain_val);

	sensor_dbg("sensor_set_gain exp = %d, %d Done!\n", gain_val, exp_val);

	info->exp = exp_val;
	info->gain = gain_val;
	return 0;

}

static int sensor_s_fps(struct v4l2_subdev *sd, struct sensor_fps *fps)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_win_size *wsize = info->current_wins;
//	data_type read_high = 0, read_low = 0;
	int vs4388_sensor_target_vts = 0;

	vs4388_fps_change_flag = 1;
	vs4388_sensor_target_vts = wsize->pclk/fps->fps/wsize->hts;

	if (vs4388_sensor_target_vts <= wsize->vts) { // the max fps = 20fps
		vs4388_sensor_target_vts = wsize->vts;
	} else if (vs4388_sensor_target_vts >= (wsize->pclk/wsize->hts)) { // the min fps = 1fps
		vs4388_sensor_target_vts = (wsize->pclk/wsize->hts) - 8;
	}

	vs4388_sensor_vts = vs4388_sensor_target_vts;
	sensor_dbg("target_fps = %d, vs4388_sensor_target_vts = %d, 0x3812 = 0x%x, 0x3813 = 0x%x\n", fps->fps,
		vs4388_sensor_target_vts, vs4388_sensor_target_vts >> 8, vs4388_sensor_target_vts & 0xff);
	sensor_write(sd, 0x3813, (vs4388_sensor_target_vts & 0xff));
	sensor_write(sd, 0x3812, (vs4388_sensor_target_vts >> 8));
	vs4388_fps_change_flag = 0;

	return 0;
}

static int sensor_s_hflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;
	data_type Reg3809;
	data_type Reg4101;
	printk("into set sensor hfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3816, &get_value);// bit[5]:mirror  bit[4]:flip
	sensor_read(sd, 0x3809, &Reg3809);
	sensor_dbg("sensor_s_hflip -- 0x3816 = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x20;
		Reg3809 = (Reg3809 & 0xFE)|0x01; //奇数
		Reg4101 = 0x0e;
	} else {
		set_value = get_value & (~0x20);
		Reg3809 = Reg3809 & 0xFE; //偶数
		Reg4101 = 0x0c;
	}

	sensor_write(sd, 0x3816, set_value);
	sensor_write(sd, 0x3809, Reg3809);
	sensor_write(sd, 0x4101, Reg4101);
	return 0;
}

static int sensor_s_vflip(struct v4l2_subdev *sd, int enable)
{
	data_type get_value;
	data_type set_value;
	data_type Reg380b;
	data_type Reg4101;

	printk("into set sensor vfilp the value:%d \n", enable);
	if (!(enable == 0 || enable == 1))
		return -1;

	sensor_read(sd, 0x3816, &get_value);// bit[5]:mirror  bit[4]:flip
	sensor_read(sd, 0x380b, &Reg380b);
	sensor_dbg("sensor_s_vflip -- 0x3816 = 0x%x\n", get_value);

	if (enable) {
		set_value = get_value | 0x10;
		Reg380b = (Reg380b & 0xFE)|0x01; //奇数
		Reg4101 = 0x0c;
	} else {
		set_value = get_value & (~0x10);
		Reg380b = Reg380b & 0xFE; //偶数
		Reg4101 = 0x0c;
	}

	sensor_write(sd, 0x3816, set_value);
	sensor_write(sd, 0x380b, Reg380b);
	sensor_write(sd, 0x4101, Reg4101);
	return 0;

}

/*
 *set && get sensor flip
 */
 static int sensor_get_fmt_mbus_core(struct v4l2_subdev *sd, int *code)
{
	struct sensor_info *info = to_state(sd);
	data_type get_value;

	sensor_read(sd, 0x3816, &get_value);
	sensor_dbg("-- read value:0x%X --\n", get_value);
	switch (get_value & 0x30) {
	case 0x00:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--BGGR hfilp set read value:0x%X --\n", get_value &  0x66);
		break;
	case 0x10:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--GBRG hfilp set read value:0x%X --\n", get_value & 0x66);
		break;
	case 0x20:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--GRBG hfilp set read value:0x%X --\n", get_value & 0x66);
		break;
	case 0x30:
		*code = MEDIA_BUS_FMT_SBGGR10_1X10;
//		printk("--RGGB hfilp set read value:0x%X --\n", get_value & 0x66);
		break;
	default:
		*code = info->fmt->mbus_code;
	}
	return 0;
}

static int sensor_s_sw_stby(struct v4l2_subdev *sd, int on_off)
{
	int ret;
	data_type rdval;

	ret = sensor_read(sd, 0x0100, &rdval);
	if (ret != 0)
		return ret;

	if (on_off == STBY_ON)
		ret = sensor_write(sd, 0x0100, rdval&0xfe);
	else
		ret = sensor_write(sd, 0x0100, rdval|0x01);
	return ret;
}

static int sensor_get_temp(struct v4l2_subdev *sd,
				struct sensor_temp *temp)
{

	return 0;
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
		usleep_range(100, 120);
		vin_gpio_write(sd, RESET, CSI_GPIO_HIGH);
		vin_gpio_write(sd, PWDN, CSI_GPIO_HIGH);
		usleep_range(5000, 7000);
		vin_set_mclk(sd, ON);
		usleep_range(5000, 7000);
		vin_set_mclk_freq(sd, MCLK);
		usleep_range(5000, 7000);
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
		sensor_read(sd, 0x3000, &rdval);
		SENSOR_ID |= (rdval << 8);
		sensor_read(sd, 0x3000, &rdval);
		SENSOR_ID |= (rdval);
		sensor_print("retry = %d, V4L2_IDENT_SENSOR = %x\n",
			cnt, SENSOR_ID);
		cnt++;
		}
	if (SENSOR_ID != V4L2_IDENT_SENSOR)
		return -ENODEV;

	return 0;
}

static int sensor_g_output_bit_width(struct v4l2_subdev *sd, unsigned char *bit_width)
{
	data_type rdval;

	sensor_read(sd, 0x3017, &rdval);
	if (rdval & 0x20)
		*bit_width = 8;
	else
		*bit_width = 10;

	return 0;
}

static int sensor_s_output_bit_width
		(struct v4l2_subdev *sd, enum set_bit_width *set_output_bit_width)
{
	struct sensor_info *info = to_state(sd);
	struct sensor_exp_gain exp_gain;

	if (*set_output_bit_width == B10_TO_B8) {
		sensor_write(sd, 0x0100, 0x00);
		sensor_write_array(sd, sensor_2560x1440p10_regs_raw8,
					ARRAY_SIZE(sensor_2560x1440p10_regs_raw8));
		if (info->exp && info->gain) {
			exp_gain.exp_val = info->exp;
			exp_gain.gain_val = info->gain;
			sensor_s_exp_gain(sd, &exp_gain);
		}
		sensor_write(sd, 0x0100, 0x01);
		pr_err("sensor set to 8bit\n");
	} else if (*set_output_bit_width == B8_TO_B10) {
		sensor_write(sd, 0x0100, 0x00);
		sensor_write_array(sd, sensor_2560x1440p30_regs_raw10,
					ARRAY_SIZE(sensor_2560x1440p30_regs_raw10));
		if (info->exp && info->gain) {
			exp_gain.exp_val = info->exp;
			exp_gain.gain_val = info->gain;
			sensor_s_exp_gain(sd, &exp_gain);
		}
		sensor_write(sd, 0x0100, 0x01);
		pr_err("sensor set to 10bit\n");
	} else if (*set_output_bit_width == BX_TO_CLOSE) {
		sensor_write(sd, 0x0101, 0x01);
		usleep_range(1000, 1200);
		pr_err("sensor reset\n");
	}

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
	info->width = 2560;
	info->height = 1440;
	info->hflip = 0;
	info->vflip = 0;
	info->gain = 0;
	info->exp = 0;

	info->tpf.numerator = 1;
	info->tpf.denominator = 30;	/* 25fps */

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
	case VIDIOC_VIN_SENSOR_GET_FPS:
		ret = sensor_g_fps(sd, (struct sensor_fps *)arg);
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
	case VIDIOC_VIN_SENSOR_GET_TEMP:
		ret = sensor_get_temp(sd, (struct sensor_temp *)arg);
		break;
	case GET_SENSOR_OUTPUT_BIT_WIDTH:
		ret = sensor_g_output_bit_width(sd, (unsigned char *)arg);
		break;
	case SET_SENSOR_OUTPUT_BIT_WIDTH:
		ret = sensor_s_output_bit_width(sd, (enum set_bit_width *)arg);
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
#if RAW8
		.mbus_code = MEDIA_BUS_FMT_SBGGR8_1X8,
#else
		.mbus_code = MEDIA_BUS_FMT_SBGGR10_1X10,
#endif
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
		.width = 2560,
		.height = 1440,
		.hoffset = 0,
		.voffset = 0,
		.hts = 2916,
		.vts = 4446,
		.pclk = 129600000,
		.mipi_bps = 324000000,
		.fps_fixed = 10,
		.bin_factor = 1,
		.intg_min = 2 << 4,//1
		.intg_max = (4446 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 64 << 4,
		.regs = sensor_2560x1440p10_regs,
		.regs_size = ARRAY_SIZE(sensor_2560x1440p10_regs),
		.set_size = NULL,
	},

	{
		.width = 2560,
		.height = 1440,
		.hoffset = 0,
		.voffset = 0,
		.hts = 2916,
		.vts = 1482,
		.pclk = 129600000,
		.mipi_bps = 324000000,
		.fps_fixed = 30,
		.bin_factor = 1,
		.intg_min = 2 << 4,//1
		.intg_max = (1482 - 4) << 4,
		.gain_min = 1 << 4,
		.gain_max = 64 << 4,
		.regs = sensor_2560x1440p30_regs,
		.regs_size = ARRAY_SIZE(sensor_2560x1440p30_regs),
		.set_size = NULL,
	},
};

#define N_WIN_SIZES (ARRAY_SIZE(sensor_win_sizes))

static int sensor_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
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
	data_type get_value = 0;

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
	vs4388_sensor_vts = wsize->vts;

	sensor_read(sd, 0x3816, &get_value);

	sensor_print("s_fmt set width = %d, height = %d, 0x3221 = 0x%x\n", wsize->width,
		     wsize->height, get_value);

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
	info->combo_mode = CMB_TERMINAL_RES | CMB_PHYA_OFFSET1 | MIPI_NORMAL_MODE;
	info->stream_seq = MIPI_BEFORE_SENSOR;
	info->af_first_flag = 1;
	//info->time_hs = 0x90;
	//info->deskew = 0x20;
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

MODULE_DEVICE_TABLE(i2c, sensor_id);

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
