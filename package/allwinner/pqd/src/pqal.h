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

#ifndef _pq_hardware_h_
#define _pq_hardware_h_

#include <stdint.h>

/* register_data define if protocol.h */
struct register_data;

struct pqal {
    /*
     * Get hardware platform info,
     *  id: the soc vendor code, e.g. 1855 (in decimal)
     *  version: DE(display engine) ip version, e.g. 201 (in decimal)
     */
    int (*get_hardware_info)(int* id, int* version);

    /*
     * switch the pq target de port:
     *  port == 0 --> DE0
     *  port == 1 --> DE1
     */
    int (*de_index_switch)(int port);

    /*
     * some hardware requires multiple sets of configuration data,
     * we need an index to identify each packet in the same type.
     *
     *  e.g. fcm have 10 sets of configuration data!
     */
    int (*get_packet_store_index)(int type, const char* data, int size);

    /* general register rw */
    int (*set_registers)(const struct register_data* data, int count);
    int (*get_registers)(struct register_data* data, int count);

    /* gamma */
    int (*set_gamma_lut)(const char* data, int size);
    int (*get_gamma_lut)(char* data, int size);

    /* color enhance */
    int (*set_color_enhance)(const char* data, int size);
    int (*get_color_enhance)(char* data, int size);

    /* color matrix */
    int (*set_color_matrix)(const char* data, int size);
    int (*get_color_matrix)(char* data, int size);

    /* fcm */
    int (*set_fcm)(const char* data, int size);
    int (*get_fcm)(char* data, int size);
    int (*init_fcm_hardware_table)(const char* data, int size);
};

struct hardware_data {
    int code;    /* vendor ic code, e.g. 1855 */
    int version; /* DE ip version,  e.g. 201  */
    int port;    /* current DE port index     */
    struct pqal* apis;
};

struct pqal* pq_hardware_init(void);
const struct hardware_data* pq_hardware_data(void);

#endif
