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

#ifndef _pq_protocol_h_
#define _pq_protocol_h_

#include <stdint.h>

enum pq_package_cmd {
    CMD_HEART_BEAT = 0,
    CMD_GET = 1,
    CMD_SET = 2,
    CMD_SAVE_CONFIG = 3,
    CMD_READ_CONFIG = 4,
};

struct pq_package {
    int magic; /* magic package header: "AWPQ" */
    char cmd;  /* enum pq_package_cmd */
    char type; /* reserved */
    int size;  /* sizeof(struct pq_package) + payload data length */
    char data[0];
} __attribute__((packed));

#define PQ_PACKET_MAGIC 0x51505741 /* magic number "AWPQ" */

/*
 * pq packet id
 */
enum {
    HARDWARE_INFO = 1,
    SINGLE_REG = 2,
    REG_SET = 3,
    GAMMA_LUT = 11,

    ENHANCE = 14,
    COLOR_MATRIX_IN = 15,
    COLOR_MATRIX_OUT = 16,
    FCM_CTRL = 17,
};

/* head of pq_package::data[0] */
struct payload_head {
    int id;
    char data[0];
} __attribute__((packed));

/* generic reply packet */
struct pq_package_reply {
    int id;
    int ret;
} __attribute__((packed));

/* reply platform version */
struct platform_version_packet {
    int id;
    int ic_code;
    union {
        int de_port;
        int de_version;
    };
} __attribute__((packed));

struct register_data {
    uint32_t offset;
    uint32_t value;
} __attribute__((packed));

struct register_packet {
    int id;
    struct register_data reg;
} __attribute__((packed));

struct register_set_packet {
    int id;
    int count;
    struct register_data reg[0];
} __attribute__((packed));

#endif
