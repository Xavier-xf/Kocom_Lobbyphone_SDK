/*
 * Copyright (C) 2021 [allwinnertech]
 *
 * Author: yajianz <yajianz@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/debug.h"
#include "sockets/socket.h"
#include "sockets/protocol.h"

static char serialnum = 1;

static char random_byte() {
    return rand() % 255;
}

void send_pq_packet(int fd, int id, int packet_length)
{
    char buf[1024] = {0};

    struct pq_package packet;
    packet.magic = PQ_PACKET_MAGIC;
    packet.cmd  = 0x03;
    packet.type = 0x00;
    packet.size = sizeof(struct pq_package) + packet_length;

    memcpy(buf, &packet, sizeof(struct pq_package));
    struct payload_head* head = (struct payload_head*)((char*)buf + sizeof(struct pq_package));
    head->id = id;

    write(fd, buf, packet.size);
}

void send_test_packet(int fd, int dummy_prefix, int dummy_subfix, int packet_length, int total_packet)
{
    char buf[1024] = {0};

    struct pq_package packet;
    packet.magic = PQ_PACKET_MAGIC;
    packet.cmd  = 0x01;
    packet.type = 0x00;
    packet.size = sizeof(struct pq_package);

    char *p = buf;
    for (int i = 0; i < total_packet; i++) {
        for (int j = 0; j < dummy_prefix; j++) *p++ = random_byte();
        packet.size = sizeof(struct pq_package) + packet_length;
        memcpy(p, &packet, sizeof(struct pq_package));
        p += sizeof(struct pq_package);
        *p = 0x5A;
        *(p+1) = serialnum++;
        *(p+packet_length-1) = 0xA5;
        p += packet_length;
        for (int k = 0; k < dummy_subfix; k++) *p++ = random_byte();
    }

    write(fd, buf, (p - buf));
}

#define TEST_PACKET(p, s, l, t)                                                        \
    do {                                                                               \
        getchar();                                                                     \
        printf("packet[%d]: dummy %d %d size: %d total: %d\n", serialnum, p, s, l, t); \
        send_test_packet(socket, p, s, l, t);                                          \
    } while (0)

void send_split_packet(int fd, int dummy_prefix, int dummy_subfix, int packet_length, int split)
{
    char buf[1024] = {0};

    struct pq_package packet;
    packet.magic = PQ_PACKET_MAGIC;
    packet.cmd  = 0x01;
    packet.type = 0x00;
    packet.size = sizeof(struct pq_package) + packet_length;

    int split_len = packet_length / split;
    int remaining_len = packet_length - split_len;

    char *p = buf;
    for (int j = 0; j < dummy_prefix; j++) *p++ = random_byte();
    memcpy(p, &packet, sizeof(struct pq_package));
    p += sizeof(struct pq_package);
    *p = 0x5A;
    *(p+1) = serialnum++;
    p += split_len;
    write(fd, buf, (p - buf));
    usleep(1000 * 1000);

    int last_packet = 0;
    while (remaining_len > 0) {
        int size = 0;
        if (remaining_len > split_len) {
            size = split_len;
            remaining_len -= size;
        } else {
            last_packet = 1;
            size = remaining_len;
            remaining_len -= size;
        }

        char *start = p;
        p += size;

        if (last_packet) {
            *(p-1) = 0xA5;
            for (int k = 0; k < dummy_subfix; k++) *p++ = random_byte();
        }
        write(fd, start, (p - start));
        usleep(1000 * 1000);
    }
}

#define TEST_SPLIT_PACKET(p, s, l, x)                                                        \
    do {                                                                                     \
        getchar();                                                                           \
        printf("split packet[%d]: dummy %d %d size: %d, split=%d\n", serialnum, p, s, l, x); \
        send_split_packet(socket, p, s, l, x);                                               \
    } while (0)

#define TEST_PQ_PACKET(id, l)          \
    do {                               \
        getchar();                     \
        printf("packet[%d]\n", l);     \
        send_pq_packet(socket, id, l); \
    } while (0)

int main(int argc, char** argv)
{
    int socket = create_socket_client();

#if 0
    TEST_PACKET(0, 0, 16, 1);
    TEST_PACKET(0, 0, 16, 2);

    TEST_PACKET(0, 0, 17, 1);
    TEST_PACKET(0, 0, 17, 2);

    TEST_PACKET(3, 0, 16, 1);
    TEST_PACKET(4, 0, 16, 1);
    TEST_PACKET(7, 0, 16, 1);

    TEST_PACKET(3, 3, 16, 1);
    TEST_PACKET(0, 3, 32, 1);

    printf("\nsplit packet test start:\n\n");

    TEST_SPLIT_PACKET(0, 0, 15, 2);
    TEST_SPLIT_PACKET(0, 0, 32, 2);
    TEST_SPLIT_PACKET(0, 0, 48, 2);

    TEST_SPLIT_PACKET(3, 0, 48, 2);
    TEST_SPLIT_PACKET(3, 3, 48, 2);

    TEST_SPLIT_PACKET(3, 3, 47, 3);
#endif

    TEST_PQ_PACKET(11, 512);
    TEST_PQ_PACKET(14, 255);
    TEST_PQ_PACKET(15, 320);
    TEST_PQ_PACKET(16, 511);

    getchar();
    return 0;
}

