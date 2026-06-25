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

#ifndef _FCM_TYPES_H_
#define _FCM_TYPES_H_

#include <stdint.h>

#define arrary_size(x)    (sizeof(x)/sizeof(x[0]))
#define arrary_size_2D(x) (sizeof(x)/sizeof(x[0][0]))

typedef struct fcm_table {

    float Angle_HUE[28];
    float Angle_SAT[13];
    float Angle_LUM[13];

    float HBH_MaxGain[28];
    float SBH_MaxGain[28];
    float YBH_MaxGain[28];

    float SATHBH_Protect[13];
    float LUMHBH_Protect[13];
    float SATSBH_Protect[13];
    float LUMSBH_Protect[13];
    float SATYBH_Protect[13];
    float LUMYBH_Protect[13];

    int32_t Tone_Enhance_HBH[28];
    int32_t Tone_Enhance_SBH[28];
    int32_t Tone_Enhance_YBH[28];

    int32_t Hue_MaxNum;
    int32_t Sat_MaxNum;
    int32_t Lum_MaxNum;

    int32_t Zero_Angle_Pos;

} fcm_table_t;

typedef struct fcm_ctrl_para {
    int32_t id;
    float Gain_Lev_Sel_HBH;
    float Gain_Lev_Sel_SBH;
    float Gain_Lev_Sel_YBH;
    int32_t HUE_HBH[28];
    int32_t HUE_SBH[28];
    int32_t HUE_YBH[28];
    int32_t SAT_HBH[28][13];
    int32_t SAT_SBH[28][13];
    int32_t SAT_YBH[28][13];
    int32_t LUM_HBH[28][13];
    int32_t LUM_SBH[28][13];
    int32_t LUM_YBH[28][13];
    int32_t LUT_ID;
} fcm_ctrl_para_t;

typedef struct fcm_normal_data {
    float HUE_HBH[28];
    float HUE_SBH[28];
    float HUE_YBH[28];
    float SAT_HBH[28][13];
    float SAT_SBH[28][13];
    float SAT_YBH[28][13];
    float LUM_HBH[28][13];
    float LUM_SBH[28][13];
    float LUM_YBH[28][13];
} fcm_normal_data_t;

#endif

