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

#ifndef __PQ_DATA_H__
#define __PQ_DATA_H__

#include "pq.h"

int pqdata_init(void);
int pqdata_save(int port, enum pq_package_type type, int index, const void* data, int size);
int pqdata_find(int port, enum pq_package_type type, int index, const void** out, int* len);
int pqdata_store_to_file(void);

struct pqdata {
    int port; /* which DE0/1 this data will apply to */
    enum pq_package_type type;
    int index; /* index of this data in the same type */
    int size;
    const void* data;
};

int pqdata_load(struct pqdata **out);
void pqdata_free(struct pqdata *p);

#endif
