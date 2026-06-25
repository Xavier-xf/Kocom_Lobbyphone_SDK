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

#ifndef _fcm_hardware_h_
#define _fcm_hardware_h_

#include <stdint.h>

typedef struct fcm_hardware_data {
    unsigned char name[32];
    uint32_t lut_id;

    int32_t hbh_hue[28];
    int32_t sbh_hue[28];
    int32_t ybh_hue[28];

    int32_t angle_hue[28];
    int32_t angle_sat[13];
    int32_t angle_lum[13];

    int32_t hbh_sat[364];
    int32_t sbh_sat[364];
    int32_t ybh_sat[364];

    int32_t hbh_lum[364];
    int32_t sbh_lum[364];
    int32_t ybh_lum[364];
} fcm_hardware_data_t;

#endif

