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
#include "debug.h"
#include "sockets/protocol.h"

static char dumpstr[4096] = {0};

void dump_packet(struct pq_package *packet)
{
    char *p = dumpstr;

    p += sprintf(p, "\n");
    p += sprintf(p, "magic: 0x%08x\n", packet->magic);
    p += sprintf(p, "cmd  : 0x%02x\n", packet->cmd);
    p += sprintf(p, "size : 0x%08x\n", packet->size);

    for (int i = 0; i < 256 && i < (packet->size - sizeof(struct pq_package)); i++) {
        if ((i & 0xf) == 0)
            p += sprintf(p, "%4d :", i);
        p += sprintf(p, " %02x", (unsigned char)packet->data[i]);
        if ((i & 0xf) == 0xf)
            p += sprintf(p, "\n");
    }
    p += sprintf(p, "\n");

    dlogi("%s", dumpstr);
}

