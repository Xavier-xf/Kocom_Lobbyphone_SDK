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

#ifndef _vp_socket_h_
#define _vp_socket_h_

#define PORT 5005
#define SIZE 1024
#define MAX_CLIENTS 10

#include "socket_client.h"

int create_socket_server();
int create_socket_client();
int accept_client(int server_socket);

#endif
