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

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include "debug.h"
#include "socket.h"

struct socket_client {
    int socket;
    int authorized;

    pthread_mutex_t lock;
    pthread_t tid;

    packet_handler_pfn packet_handler;
};

void *socket_poll_thread(void *data)
{
    struct pollfd pollfds[1];
    struct socket_client *client = (struct socket_client *)data;

    pollfds[0].fd = client->socket;
    pollfds[0].events = POLLIN | POLLPRI;

    while (1) {
        int ret = poll(pollfds, 1, 5000);
        if (ret > 0) {
            if (pollfds[0].fd > 0 && pollfds[0].revents & POLLIN) {
                char buf[SIZE];
                int size = read(pollfds[0].fd, buf, SIZE);
                if (client->packet_handler)
                    client->packet_handler(client, buf, size);
            }
        }
    }

    return 0;
}

struct socket_client* socket_client_create(int socket_fd, packet_handler_pfn handler)
{
    struct socket_client* client = malloc(sizeof(*client));
    memset(client, 0, sizeof(*client));

    client->socket = socket_fd;
    client->authorized = 0;
    client->packet_handler = handler;

    pthread_mutex_init(&client->lock, NULL);
    pthread_create(&client->tid, NULL, (void *)socket_poll_thread, (void *)client);

    return client;
}

void socket_client_destroy(struct socket_client *client)
{
    pthread_join(client->tid, NULL);
    close(client->socket);
    free(client);
}

int socket_client_send_packet(struct socket_client *client, const char* buf, int size)
{
    int written = 0;

    pthread_mutex_lock(&client->lock);
    written = write(client->socket, buf, size);
    pthread_mutex_unlock(&client->lock);

    if (written != size) {
        dloge("write socket error: %s", strerror(errno));
        return -1;
    }
    return 0;
}

