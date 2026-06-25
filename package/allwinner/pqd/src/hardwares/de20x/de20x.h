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

#ifndef _pq_de20x_h_
#define _pq_de20x_h_

/* register_data define if protocol.h */
struct register_data;

int de20x_set_registers(const struct register_data* data, int count);
int de20x_get_registers(struct register_data* data, int count);

int de20x_set_gamma_lut(const char* data, int size);
int de20x_get_gamma_lut(char* data, int size);

int de20x_set_enhance(const char *data, int size);
int de20x_get_enhance(char *data, int size);

int de20x_set_color_matrix(const char *data, int size);
int de20x_get_color_matrix(char *data, int size);

int de20x_get_fcm_packet_index(const char *data, int size);
int de20x_set_fcm(const char *data, int size);
int de20x_get_fcm(char *data, int size);
int de20x_init_fcm_hardware_table(const char *data, int size);

#endif

