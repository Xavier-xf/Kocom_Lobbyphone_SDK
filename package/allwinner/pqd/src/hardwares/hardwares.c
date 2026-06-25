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

#include <stdlib.h>

#include "debug.h"
#include "pqal.h"
#include "utils.h"
#include "de20x/de20x.h"
#include "pqdata/pq.h"

static struct pqal* aw1855_pq_impl_create(void);
static struct pqal* aw1886_pq_impl_create(void);

struct hardware_table {
    int code;
    int version;
    struct pqal* (*hardware_impl_create)(void);
};

static struct hardware_table hardwares_[] = {
        {1855, 201, aw1855_pq_impl_create},
        {1886, 201, aw1886_pq_impl_create},

        /* add new hardware impl here */

        /* DO NOT DELETE !!! */
        {-1, -1, NULL},
};

static struct hardware_data pq_hardware_data_;

struct pqal* pq_hardware_init(void)
{
    int soc = get_hardware_id();
    if (soc < 0) {
        dloge("get_hardware_id() failed, use default soc hardware impl [%d]", hardwares_[0].code);
        soc = hardwares_[0].code;
    }

    int i = 0;
    while (hardwares_[i].code != -1 && hardwares_[i].code != soc) {
        i++;
    }
    struct hardware_table* hw = &hardwares_[i];

    if (hw->hardware_impl_create) {
        pq_hardware_data_.code = hw->code;
        pq_hardware_data_.version = hw->version;
        pq_hardware_data_.port = 0;
        pq_hardware_data_.apis = hw->hardware_impl_create();
    }

    return pq_hardware_data_.apis;
}

const struct hardware_data* pq_hardware_data(void) {
    return &pq_hardware_data_;
}

static int default_get_hardware_info(int* id, int* version)
{
    *id = pq_hardware_data_.code;
    *version = pq_hardware_data_.version;
    return 0;
}

static int default_de_index_switch(int port)
{
    pq_hardware_data_.port = port;
    return 0;
}

static int default_get_packet_store_index(int type, const char* data, int size)
{
    switch (type) {
        case PQ_GAMMA_LUT:
        case PQ_PEAK_SHARP:
        case PQ_LTI_SHARP:
        case PQ_COLOR_ENHANCE:
        case PQ_COLOR_MATRIX_IN:
        case PQ_COLOR_MATRIX_OUT:
            return 0;
        case PQ_FCM:
            return de20x_get_fcm_packet_index(data, size);
        default:
            return 0;
    }
}

int unsupport_set_function(const char* data, int size) { return 0; }
int unsupport_get_function(char* data, int size) { return 0; }

static struct pqal* aw1855_pq_impl_create(void)
{
    static struct pqal aw1855_pqal = {
        .get_hardware_info = default_get_hardware_info,
        .de_index_switch = default_de_index_switch,

        .get_packet_store_index = default_get_packet_store_index,

        .set_registers = de20x_set_registers,
        .get_registers = de20x_get_registers,

        /* gamma */
        .set_gamma_lut = de20x_set_gamma_lut,
        .get_gamma_lut = de20x_get_gamma_lut,

        /* enhance */
        .set_color_enhance = de20x_set_enhance,
        .get_color_enhance = de20x_get_enhance,

        /* matrix */
        .set_color_matrix = de20x_set_color_matrix,
        .get_color_matrix = de20x_get_color_matrix,

        /* fcm */
        .set_fcm = unsupport_set_function,
        .get_fcm = unsupport_get_function,
        .init_fcm_hardware_table = unsupport_set_function,
    };
    return &aw1855_pqal;
}

static struct pqal* aw1886_pq_impl_create(void)
{
    static struct pqal aw1886_pqal = {
        .get_hardware_info = default_get_hardware_info,
        .de_index_switch = default_de_index_switch,

        .get_packet_store_index = default_get_packet_store_index,

        .set_registers = de20x_set_registers,
        .get_registers = de20x_get_registers,

        /* gamma */
        .set_gamma_lut = de20x_set_gamma_lut,
        .get_gamma_lut = de20x_get_gamma_lut,

        /* enhance */
        .set_color_enhance = de20x_set_enhance,
        .get_color_enhance = de20x_get_enhance,

        /* matrix */
        .set_color_matrix = de20x_set_color_matrix,
        .get_color_matrix = de20x_get_color_matrix,

        /* fcm */
        .set_fcm = de20x_set_fcm,
        .get_fcm = de20x_get_fcm,
        .init_fcm_hardware_table = de20x_init_fcm_hardware_table,
    };
    return &aw1886_pqal;
}

