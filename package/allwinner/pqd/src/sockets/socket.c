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

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <poll.h>
#include <errno.h>

#include "debug.h"
#include "socket.h"

int create_socket_server()
{
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int ret = bind(server_socket, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) {
        dloge("bind socket error: %s", strerror(errno));
        return -1;
    }

    ret = listen(server_socket, 5);
    if (ret == -1) {
        dloge("listen socket error: %s", strerror(errno));
        return -1;
    }

    dlogi("socket server start");
    return server_socket;
}

int create_socket_client()
{
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int ret = connect(client_socket, (struct sockaddr *)&addr, sizeof(addr));

    dlogi("connect result ==> %d", ret);
    if (ret < 0) {
        dloge("connect error: %s", strerror(errno));
        return -1;
    }

    return client_socket;
}

int accept_client(int server_socket)
{
    struct sockaddr_in client_addr;
    unsigned int addr_len = sizeof(client_addr);

    dlogi("waiting connect ...");
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
    dlogi("accept success %s", inet_ntoa(client_addr.sin_addr));

    return client_socket;
}

