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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "fcm_types.h"
#include "fcm_hardware.h"

static float gain_level_from_selection(float selection)
{
    return selection;
}

static int rotate_arrary(int32_t* arrary, int len, int zero_pos)
{
    if (zero_pos >= len)
        return -EINVAL;

    int32_t* tmp_arrary = (int32_t*)malloc(sizeof(int32_t)*len);
    if (tmp_arrary == NULL)
        return -ENOMEM;

    for (int i = 0; i < len-zero_pos; i++)
        tmp_arrary[i] = arrary[i+zero_pos];

    int offset = len - zero_pos;
    for (int i = 0; i < zero_pos; i++)
        tmp_arrary[i+offset] = arrary[i];

    memcpy(arrary, tmp_arrary, sizeof(int32_t)*len);

    free(tmp_arrary);
    return 0;
}

static int rotate_2d_arrary(int32_t* arrary, int row, int colum, int zero_pos)
{
    if (zero_pos > row)
        return -EINVAL;

    int size = sizeof(int32_t) * row * colum;
    int32_t* tmp_arrary = (int32_t*)malloc(size);
    if (tmp_arrary == NULL)
        return -ENOMEM;

    int32_t *src, *dst;
    int arrarysize = sizeof(int32_t) * colum;
    for (int i = 0; i < row - zero_pos; i++) {
        src = arrary + (i + zero_pos)*colum;
        dst = tmp_arrary + i*colum;
        memcpy(dst, src, arrarysize);
    }

    int offset = row - zero_pos;
    for (int i = 0; i < zero_pos; i++) {
        src = arrary + i*colum;
        dst = tmp_arrary + (i + offset)*colum;
        memcpy(dst, src, arrarysize);
    }

    memcpy(arrary, tmp_arrary, size);

    free(tmp_arrary);
    return 0;
}

static int rotate_float_arrary(float* arrary, int len, int zero_pos)
{
    if (zero_pos >= len)
        return -EINVAL;

    float* tmp_arrary = (float*)malloc(sizeof(float)*len);
    if (tmp_arrary == NULL)
        return -ENOMEM;

    for (int i = 0; i < len-zero_pos; i++)
        tmp_arrary[i] = arrary[i+zero_pos];

    int offset = len - zero_pos;
    for (int i = 0; i < zero_pos; i++)
        tmp_arrary[i+offset] = arrary[i];

    memcpy(arrary, tmp_arrary, sizeof(float)*len);

    free(tmp_arrary);
    return 0;
}

static void arrary_dot_product(float* a, const float* b, int len)
{
    for (int i = 0; i < len; i++) {
        *a = (*a) * (*b);
        a++;
        b++;
    }
}

static void arrary_scale(float* a, const float b, int len)
{
    for (int i = 0; i < len; i++) {
        *a = (*a) * (b);
        a++;
    }
}

/*
 * a[row][column] * b[column]
 *
 */
static void arrary_dot_product_2D(float* a, int row, int column, const float* b)
{
    for (int i = 0; i < row; i++) {
        float* head = a + (i * column);
        arrary_dot_product(head, b, column);
    }
}

static const float HUE_RANGE = 200.f;
static const float SAT_RANGE =  50.f;
static const float LUM_RANGE =  50.f;

static void normal_arrary(const int32_t* in, float* out, float range, int len)
{
    for (int i = 0; i < len; i++) {
        *out = (float)(*in) / range;
        in++;
        out++;
    }
}

#define normal_2d_arrary(in, out, range)                 \
    do {                                                 \
        int row, column;                                 \
        row = sizeof(in) / sizeof(in[0]);                \
        column = sizeof(in[0]) / sizeof(in[0][0]);       \
        for (int i = 0; i < row; i++) {                  \
            normal_arrary(in[i], out[i], range, column); \
        }                                                \
    } while (0)

static void normal_fcm_ctrl_data(const fcm_ctrl_para_t* para, fcm_normal_data_t *out)
{
    /* HUE */
    normal_arrary(para->HUE_HBH, out->HUE_HBH, HUE_RANGE, arrary_size(para->HUE_HBH));
    normal_arrary(para->HUE_SBH, out->HUE_SBH, HUE_RANGE, arrary_size(para->HUE_SBH));
    normal_arrary(para->HUE_YBH, out->HUE_YBH, HUE_RANGE, arrary_size(para->HUE_YBH));

    /* SAT */
    normal_2d_arrary(para->SAT_HBH, out->SAT_HBH, SAT_RANGE);
    normal_2d_arrary(para->SAT_SBH, out->SAT_SBH, SAT_RANGE);
    normal_2d_arrary(para->SAT_YBH, out->SAT_YBH, SAT_RANGE);

    /* LUM */
    normal_2d_arrary(para->LUM_HBH, out->LUM_HBH, LUM_RANGE);
    normal_2d_arrary(para->LUM_SBH, out->LUM_SBH, LUM_RANGE);
    normal_2d_arrary(para->LUM_YBH, out->LUM_YBH, LUM_RANGE);
}

static void dump_float_arrary(char* prefix, const float* arrary, int len)
{
    printf("%s[%d]:\n{ ", prefix, len);
    for (int i = 0; i < len; i++)
        printf("%5.2f, ", arrary[i]);
    printf("}\n");
}

static void dump_arrary(char* prefix, const int* arrary, int len)
{
    printf("%s[%d]:\n{ ", prefix, len);
    for (int i = 0; i < len; i++) {
        printf("%5d, ", arrary[i]);
    }
    printf("}\n");
}

static void dump_2d_float_arrary(const char* prefix, const float* d, int r, int c)
{
    printf("%s\n:", prefix);
    printf("----------------------------------------------------------------\n");

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            printf("%5.2f, ", *d);
            d++;
        }
        printf("\n");
    }
    printf("\n----------------------------------------------------------------\n");
}

#define PRINT_ARRARY(prefix, x) \
    dump_arrary(prefix, x, sizeof(x)/sizeof(x[0]))

#define PRINT_FLOAT_ARRARY(prefix, x) \
    dump_float_arrary(prefix, x, sizeof(x)/sizeof(x[0]))

static void dump_fcm_table(const char* headline, const fcm_table_t* table)
{
    printf("---------------------------------------------------\n");
    printf("[B] ---> %s <---\n", headline);

    PRINT_FLOAT_ARRARY("Angle_HUE", table->Angle_HUE);
    PRINT_FLOAT_ARRARY("Angle_SAT", table->Angle_SAT);
    PRINT_FLOAT_ARRARY("Angle_LUM", table->Angle_LUM);

    PRINT_FLOAT_ARRARY("HBH_MaxGain", table->HBH_MaxGain);
    PRINT_FLOAT_ARRARY("SBH_MaxGain", table->SBH_MaxGain);
    PRINT_FLOAT_ARRARY("YBH_MaxGain", table->YBH_MaxGain);

    PRINT_FLOAT_ARRARY("SATHBH_Protect", table->SATHBH_Protect);
    PRINT_FLOAT_ARRARY("LUMHBH_Protect", table->LUMHBH_Protect);
    PRINT_FLOAT_ARRARY("SATSBH_Protect", table->SATSBH_Protect);
    PRINT_FLOAT_ARRARY("LUMSBH_Protect", table->LUMSBH_Protect);
    PRINT_FLOAT_ARRARY("SATYBH_Protect", table->SATYBH_Protect);
    PRINT_FLOAT_ARRARY("LUMYBH_Protect", table->LUMYBH_Protect);

    PRINT_ARRARY("Tone_Enhance_HBH", table->Tone_Enhance_HBH);
    PRINT_ARRARY("Tone_Enhance_SBH", table->Tone_Enhance_SBH);
    PRINT_ARRARY("Tone_Enhance_YBH", table->Tone_Enhance_YBH);

    printf("Hue_MaxNum  : %d\n", table->Hue_MaxNum);
    printf("Sat_MaxNum  : %d\n", table->Sat_MaxNum);
    printf("Lum_MaxNum  : %d\n", table->Lum_MaxNum);
    printf("len-zero_pos: %d\n", table->Zero_Angle_Pos);

    printf("[E] ---> %s <---\n", headline);
    printf("---------------------------------------------------\n");
}

static void dump_normal_data(const char* headline, const fcm_normal_data_t* d)
{
    printf("---------------------------------------------------\n");
    printf("[B] ---> %s <---\n", headline);

    PRINT_FLOAT_ARRARY("HUE_HBH", d->HUE_HBH);
    PRINT_FLOAT_ARRARY("HUE_SBH", d->HUE_SBH);
    PRINT_FLOAT_ARRARY("HUE_YBH", d->HUE_YBH);

    dump_2d_float_arrary("SAT_HBH", &d->SAT_HBH[0][0], 28, 13);
    dump_2d_float_arrary("SAT_SBH", &d->SAT_SBH[0][0], 28, 13);
    dump_2d_float_arrary("SAT_YBH", &d->SAT_YBH[0][0], 28, 13);

    dump_2d_float_arrary("LUM_HBH", &d->LUM_HBH[0][0], 28, 13);
    dump_2d_float_arrary("LUM_SBH", &d->LUM_SBH[0][0], 28, 13);
    dump_2d_float_arrary("LUM_YBH", &d->LUM_YBH[0][0], 28, 13);

    printf("[E] ---> %s <---\n", headline);
    printf("---------------------------------------------------\n");
}

extern const fcm_table_t constants_fcm_table;
static fcm_table_t *fcm_const;

int fcm_init()
{
    if (fcm_const)
        return 0;
    fcm_const = (fcm_table_t*)malloc(sizeof(*fcm_const));
    if (!fcm_const)
        return -ENOMEM;
    memcpy(fcm_const, &constants_fcm_table, sizeof(*fcm_const));

    rotate_float_arrary(fcm_const->Angle_HUE,   arrary_size(fcm_const->Angle_HUE  ), fcm_const->Zero_Angle_Pos);
    rotate_float_arrary(fcm_const->HBH_MaxGain, arrary_size(fcm_const->HBH_MaxGain), fcm_const->Zero_Angle_Pos);
    rotate_float_arrary(fcm_const->SBH_MaxGain, arrary_size(fcm_const->SBH_MaxGain), fcm_const->Zero_Angle_Pos);
    rotate_float_arrary(fcm_const->YBH_MaxGain, arrary_size(fcm_const->YBH_MaxGain), fcm_const->Zero_Angle_Pos);

    rotate_arrary(fcm_const->Tone_Enhance_HBH, arrary_size(fcm_const->Tone_Enhance_HBH), fcm_const->Zero_Angle_Pos);
    rotate_arrary(fcm_const->Tone_Enhance_SBH, arrary_size(fcm_const->Tone_Enhance_SBH), fcm_const->Zero_Angle_Pos);
    rotate_arrary(fcm_const->Tone_Enhance_YBH, arrary_size(fcm_const->Tone_Enhance_YBH), fcm_const->Zero_Angle_Pos);

    return 0;
}

void calculate_res_hue(const fcm_table_t* cs, const fcm_ctrl_para_t* para, fcm_normal_data_t* n)
{
    float gain[3] = {0.0f, 0.0f, 0.0f};

    gain[0] = gain_level_from_selection(para->Gain_Lev_Sel_HBH);
    gain[1] = gain_level_from_selection(para->Gain_Lev_Sel_SBH);
    gain[2] = gain_level_from_selection(para->Gain_Lev_Sel_YBH);

    arrary_dot_product(n->HUE_HBH, cs->HBH_MaxGain, arrary_size(n->HUE_HBH));
    arrary_dot_product(n->HUE_SBH, cs->SBH_MaxGain, arrary_size(n->HUE_SBH));
    arrary_dot_product(n->HUE_YBH, cs->YBH_MaxGain, arrary_size(n->HUE_YBH));

    arrary_scale(n->HUE_HBH, gain[0], arrary_size(n->HUE_HBH));
    arrary_scale(n->HUE_SBH, gain[1], arrary_size(n->HUE_SBH));
    arrary_scale(n->HUE_YBH, gain[2], arrary_size(n->HUE_YBH));
}

void calculate_res_sat_lum(const fcm_table_t* cs, fcm_normal_data_t* n)
{
    int row    = sizeof(n->SAT_HBH   ) / sizeof(n->SAT_HBH[0]   );
    int column = sizeof(n->SAT_HBH[0]) / sizeof(n->SAT_HBH[0][0]);

    arrary_dot_product_2D((float *)n->SAT_HBH, row, column, cs->SATHBH_Protect);
    arrary_dot_product_2D((float *)n->SAT_SBH, row, column, cs->SATSBH_Protect);
    arrary_dot_product_2D((float *)n->SAT_YBH, row, column, cs->SATYBH_Protect);

    arrary_dot_product_2D((float *)n->LUM_HBH, row, column, cs->LUMHBH_Protect);
    arrary_dot_product_2D((float *)n->LUM_SBH, row, column, cs->LUMSBH_Protect);
    arrary_dot_product_2D((float *)n->LUM_YBH, row, column, cs->LUMYBH_Protect);
}

#define FLOAT_TO_INT32(x) ((x) >= 0 ? (int32_t)((x) + 0.5f) : (int32_t)((x)-0.5f))

static inline void float_arrary_to_int32(const float* a, int len, int32_t* b)
{
    for (int i = 0; i < len; i++) {
        *b = FLOAT_TO_INT32(*a);
        a++;
        b++;
    }
}

#define HUE_MAP_TO_INT32(in, scale, out)                 \
    do {                                                 \
        arrary_scale(in, scale, arrary_size(in));        \
        float_arrary_to_int32(in, arrary_size(in), out); \
    } while (0)

#define ANGLE_MAP_TO_INT32 HUE_MAP_TO_INT32

#define SAT_LUM_MAP_TO_INT32(in, out)                               \
    do {                                                            \
        arrary_scale((float*)in, 127, arrary_size_2D(in));          \
        float_arrary_to_int32((float*)in, arrary_size_2D(in), out); \
    } while (0)

/* transform the result to hardware data */
void generate_hardware_data(const fcm_table_t* t, const fcm_normal_data_t* n, fcm_hardware_data_t* out)
{
    fcm_table_t* tt = malloc(sizeof(*tt)); memcpy(tt, t, sizeof(*tt));
    fcm_normal_data_t* tn = malloc(sizeof(*tn)); memcpy(tn, n, sizeof(*tn));

    // HW_HUE_HBH
    float s = (float)(tt->Hue_MaxNum) / 360.0f;

    HUE_MAP_TO_INT32(tn->HUE_HBH, s,              out->hbh_hue);
    HUE_MAP_TO_INT32(tn->HUE_SBH, tt->Sat_MaxNum, out->sbh_hue);
    HUE_MAP_TO_INT32(tn->HUE_YBH, tt->Lum_MaxNum, out->ybh_hue);

    ANGLE_MAP_TO_INT32(tt->Angle_HUE, s,              out->angle_hue);
    ANGLE_MAP_TO_INT32(tt->Angle_SAT, tt->Sat_MaxNum, out->angle_sat);
    ANGLE_MAP_TO_INT32(tt->Angle_LUM, tt->Lum_MaxNum, out->angle_lum);

    SAT_LUM_MAP_TO_INT32(tn->SAT_HBH, out->hbh_sat);
    SAT_LUM_MAP_TO_INT32(tn->LUM_HBH, out->hbh_lum);

    SAT_LUM_MAP_TO_INT32(tn->SAT_SBH, out->sbh_sat);
    SAT_LUM_MAP_TO_INT32(tn->LUM_SBH, out->sbh_lum);

    SAT_LUM_MAP_TO_INT32(tn->SAT_YBH, out->ybh_sat);
    SAT_LUM_MAP_TO_INT32(tn->LUM_YBH, out->ybh_lum);

    free(tt);
    free(tn);
}

static void dump_hw_1d_arrary(char* prefix, const int32_t* arrary, int len)
{
    printf("%s[%d] = \n", prefix, len);
    for (int i = 0; i < len; i++) {
        printf("%5d, ", arrary[i]);
    }
    printf("\n----------------------------------------------------------------\n");
}

static void dump_hw_2d_arrary(const char* prefix, const int32_t* d, int r, int c)
{
    printf("%s[%d][%d]\n", prefix, r, c);

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            printf("%5d, ", *d);
            d++;
        }
        printf("\n");
    }
    printf("\n----------------------------------------------------------------\n");
}

static void dump_hardware_data(const char* headline, const fcm_hardware_data_t* h)
{
    printf("---------------------------------------------------\n");
    printf("[B] ---> %s <---\n", headline);

    printf("LUT ID: %d\n", h->lut_id);
    dump_hw_1d_arrary("hbh_hue", h->hbh_hue, arrary_size(h->hbh_hue));
    dump_hw_1d_arrary("sbh_hue", h->sbh_hue, arrary_size(h->sbh_hue));
    dump_hw_1d_arrary("ybh_hue", h->ybh_hue, arrary_size(h->ybh_hue));

    dump_hw_1d_arrary("angle_hue", h->angle_hue, arrary_size(h->angle_hue));
    dump_hw_1d_arrary("angle_sat", h->angle_sat, arrary_size(h->angle_sat));
    dump_hw_1d_arrary("angle_lum", h->angle_lum, arrary_size(h->angle_lum));

    dump_hw_2d_arrary("hbh_sat", h->hbh_sat, 28, 13);
    dump_hw_2d_arrary("sbh_sat", h->sbh_sat, 28, 13);
    dump_hw_2d_arrary("ybh_sat", h->ybh_sat, 28, 13);

    dump_hw_2d_arrary("hbh_lum", h->hbh_lum, 28, 13);
    dump_hw_2d_arrary("sbh_lum", h->sbh_lum, 28, 13);
    dump_hw_2d_arrary("ybh_lum", h->ybh_lum, 28, 13);

    printf("[B] ---> %s <---\n", headline);
    printf("---------------------------------------------------\n");
}

void dump_fcm_ctrl_data(const char* headline, const fcm_ctrl_para_t* c)
{
    printf("---------------------------------------------------\n");
    printf("[B] ---> %s <---\n", headline);

    printf("Gain HBH: %5.2f\n", c->Gain_Lev_Sel_HBH);
    printf("Gain SBH: %5.2f\n", c->Gain_Lev_Sel_SBH);
    printf("Gain YBH: %5.2f\n", c->Gain_Lev_Sel_YBH);
    dump_hw_1d_arrary("HUE_HBH", c->HUE_HBH, arrary_size(c->HUE_HBH));
    dump_hw_1d_arrary("HUE_SBH", c->HUE_SBH, arrary_size(c->HUE_SBH));
    dump_hw_1d_arrary("HUE_YBH", c->HUE_YBH, arrary_size(c->HUE_YBH));

    dump_hw_2d_arrary("SAT_HBH", (int32_t*)c->SAT_HBH, 28, 13);
    dump_hw_2d_arrary("SAT_SBH", (int32_t*)c->SAT_SBH, 28, 13);
    dump_hw_2d_arrary("SAT_YBH", (int32_t*)c->SAT_YBH, 28, 13);

    dump_hw_2d_arrary("LUM_HBH", (int32_t*)c->LUM_HBH, 28, 13);
    dump_hw_2d_arrary("LUM_SBH", (int32_t*)c->LUM_SBH, 28, 13);
    dump_hw_2d_arrary("LUM_YBH", (int32_t*)c->LUM_YBH, 28, 13);

    printf("[E] ---> %s <---\n", headline);
    printf("---------------------------------------------------\n");
}

#define ROTATE_2D_ARRARY_(a, pos)                 \
    do {                                          \
        int r, c;                                 \
        r = sizeof(a) / sizeof(a[0]);             \
        c = sizeof(a[0]) / sizeof(a[0][0]);       \
        rotate_2d_arrary((int32_t*)a, r, c, pos); \
    } while (0)

void rotate_fcm_ctrl_data(fcm_ctrl_para_t* dat)
{
    rotate_arrary(dat->HUE_HBH, arrary_size(dat->HUE_HBH), fcm_const->Zero_Angle_Pos);
    rotate_arrary(dat->HUE_SBH, arrary_size(dat->HUE_SBH), fcm_const->Zero_Angle_Pos);
    rotate_arrary(dat->HUE_YBH, arrary_size(dat->HUE_YBH), fcm_const->Zero_Angle_Pos);

    ROTATE_2D_ARRARY_(dat->SAT_HBH, fcm_const->Zero_Angle_Pos);
    ROTATE_2D_ARRARY_(dat->SAT_SBH, fcm_const->Zero_Angle_Pos);
    ROTATE_2D_ARRARY_(dat->SAT_YBH, fcm_const->Zero_Angle_Pos);

    ROTATE_2D_ARRARY_(dat->LUM_HBH, fcm_const->Zero_Angle_Pos);
    ROTATE_2D_ARRARY_(dat->LUM_SBH, fcm_const->Zero_Angle_Pos);
    ROTATE_2D_ARRARY_(dat->LUM_YBH, fcm_const->Zero_Angle_Pos);
}

int fcm_convert_pqtool_input(fcm_ctrl_para_t* para, fcm_hardware_data_t* hwdat, int verbose)
{
    fcm_normal_data_t* normal = (fcm_normal_data_t *)malloc(sizeof(*normal));
    if (!normal) {
        return -ENOMEM;
    }
    memset(normal, 0, sizeof(*normal));

    if (verbose) dump_fcm_table("fixed fcm constants", &constants_fcm_table);

    // init fcm constants table and rotate the Angle Hue/MaxGain arrary
    // 4 & 5
    fcm_init();
    if (verbose) dump_fcm_table("fcm constants after rotate", fcm_const);

    // 6. rotate input HUE/SAT/LUM
    if (verbose) dump_fcm_ctrl_data("ctrl para from pqtool", para);
    rotate_fcm_ctrl_data(para);
    if (verbose) dump_fcm_ctrl_data("ctrl para after rotate", para);

    // 7. normal HUE/SAT/LUM
    normal_fcm_ctrl_data(para, normal);
    if (verbose) dump_normal_data("ctrl para after normal", normal);

    // 8. Res_HUE_HBH/Res_HUE_SBH/Res_HUE_YBH
    calculate_res_hue(fcm_const, para, normal);

    // 9. Res_SAT_xxx/Res_LUM_xxx
    calculate_res_sat_lum(fcm_const, normal);

    if (verbose) dump_normal_data("Res_xxx", normal);

    // 10. HW data
    generate_hardware_data(fcm_const, normal, hwdat);
    hwdat->lut_id = para->LUT_ID;
    if (verbose) dump_hardware_data("hardware data to driver", hwdat);

    return 0;
}

