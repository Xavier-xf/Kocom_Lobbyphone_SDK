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

#include "fcm.c"

extern const fcm_table_t constants_fcm_table;


fcm_ctrl_para_t test_data;

#define INIT_ARRARY(a, l, d)      \
    for (int i = 0; i < l; i++) { \
        if (i & 1)                \
            a[i] = d * 2 + i;     \
        else                      \
            a[i] = d + i;         \
    }

#define INIT_ARRARY_2D(a, r, l, d)         \
    for (int i = 0; i < r; i++) {          \
        for (int j = 0; j < l; j++) {      \
            int s = ((i + j) & 1) ? 2 : 1; \
            a[i][j] = (d * s + i)/2;       \
        }                                  \
    }

static void init_test_data(fcm_ctrl_para_t* data) {
    data->Gain_Lev_Sel_HBH = 2;
    data->Gain_Lev_Sel_SBH = 2;
    data->Gain_Lev_Sel_YBH = 2;

    INIT_ARRARY(data->HUE_HBH, 28, 100);
    INIT_ARRARY(data->HUE_SBH, 28, -100);
    INIT_ARRARY(data->HUE_YBH, 28, 100);

    INIT_ARRARY_2D(data->SAT_HBH, 28, 13,  25);
    INIT_ARRARY_2D(data->SAT_SBH, 28, 13, -25);
    INIT_ARRARY_2D(data->SAT_YBH, 28, 13,  25);

    INIT_ARRARY_2D(data->LUM_HBH, 28, 13, -25);
    INIT_ARRARY_2D(data->LUM_SBH, 28, 13,  25);
    INIT_ARRARY_2D(data->LUM_YBH, 28, 13, -25);
}

fcm_ctrl_para_t testDat2 = {
        17, /* fcm packet */
        3,
        3,
        3,
        {148, -165, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {170, -180, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {174, -136, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

#define INIT_ARRARY_2D_VALUE(a, r, l, v) \
    for (int i = 0; i < r; i++) {        \
        for (int j = 0; j < l; j++) {    \
            a[i][j] = v;                 \
        }                                \
    }

static void init_test_data2(fcm_ctrl_para_t* data)
{
    memcpy(data, &testDat2, sizeof(*data));

    INIT_ARRARY_2D_VALUE(data->SAT_HBH, 28, 13, 50);
    INIT_ARRARY_2D_VALUE(data->SAT_SBH, 28, 13, 50);
    INIT_ARRARY_2D_VALUE(data->SAT_YBH, 28, 13, 50);

    INIT_ARRARY_2D_VALUE(data->LUM_HBH, 28, 13, 42);
    INIT_ARRARY_2D_VALUE(data->LUM_SBH, 28, 13, 42);
    INIT_ARRARY_2D_VALUE(data->LUM_YBH, 28, 13, 42);
}

int main(int argc, char** argv)
{
    init_test_data2(&test_data);

    if (argc > 1 && strcmp(argv[1], "legacy") == 0) {
        printf("Legacy mode:\n\n");
        dump_fcm_table("constants table", &constants_fcm_table);

        fcm_init();
        dump_fcm_table("rotate constants table", fcm_const);

        fcm_normal_data_t normal;
        normal_fcm_ctrl_data(&test_data, &normal);
        dump_normal_data("normal fcm ctrl para", &normal);

        calculate_res_hue(fcm_const, &test_data, &normal);
        calculate_res_sat_lum(fcm_const, &normal);

        dump_normal_data("Res_xxx", &normal);

        struct fcm_hardware_data hw;
        generate_hardware_data(fcm_const, &normal, &hw);
        dump_hardware_data("hardware data to driver", &hw);
    } else {
        printf("Release mode:\n\n");
        struct fcm_hardware_data hwdat;
        fcm_convert_pqtool_input(&test_data, &hwdat, 1);
    }

    return 0;
}
