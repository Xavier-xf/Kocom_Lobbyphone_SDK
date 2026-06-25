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

#ifndef _socket_client_h_
#define _socket_client_h_

#include <stdlib.h>

struct socket_client;
typedef void(*packet_handler_pfn)(
        struct socket_client* client, const char* buf, int size);

struct socket_client* socket_client_create(int socket_fd, packet_handler_pfn handler);
void socket_client_destroy(struct socket_client *client);
int socket_client_send_packet(struct socket_client *client, const char* buf, int size);

#endif

