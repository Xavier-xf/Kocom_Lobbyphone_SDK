/*
 * Copyright (C) 2022  Allwinnertech
 * Author: yajianz <yajianz@allwinnertech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _pq_driver_api_h_
#define _pq_driver_api_h_

#include "de20x/fcm/fcm_hardware.h"

enum {
    DISP_LCD_SET_GAMMA_TABLE = 0x10b,
    DISP_LCD_GAMMA_CORRECTION_ENABLE = 0x10c,
    DISP_LCD_GAMMA_CORRECTION_DISABLE = 0x10d,

    /* --- pq --- */
    DISP_PQ_PROC = 0x500,
    DISP_LCD_GET_GAMMA_TABLE = 0x501,
};

/*
 * DISP_PQ_PROC sub command
 */
enum {
    PQ_SET_REG = 0x1,
    PQ_GET_REG = 0x2,
    PQ_ENABLE = 0x3,
    PQ_COLOR_MATRIX = 0x4,
    PQ_FCM = 0x5,
};

struct register_info {
    uint32_t offset;
    uint32_t value;
};

/*
 * gamma lut from userspace to kernel
 */
struct hw_gamma_lut {
    uint32_t x[256];
};

/* DISP_PQ_PROC ioctl enhance data struct */
struct enhance_data {
    int cmd;
    int read;
    int contrast;
    int brightness;
    int saturation;
    int hue;
};

/*
 * color matrix
 */
struct matrix4x4 {
    int64_t x00;
    int64_t x01;
    int64_t x02;
    int64_t x03;
    int64_t x10;
    int64_t x11;
    int64_t x12;
    int64_t x13;
    int64_t x20;
    int64_t x21;
    int64_t x22;
    int64_t x23;
    int64_t x30;
    int64_t x31;
    int64_t x32;
    int64_t x33;
};

/*
 * matrix config from userspace to kernel driver
 */
struct matrix_user {
    int cmd;
    int read;
    int choice;
    struct matrix4x4 matrix;
};

struct fcm_info {
    int cmd; /* 0: write to driver 1: read from driver */
    fcm_hardware_data_t fcm_data;
};

#endif
