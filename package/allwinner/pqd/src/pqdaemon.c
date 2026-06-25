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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/debug.h"
#include "pqdata/config.h"
#include "sockets/protocol.h"
#include "sockets/socket.h"
#include "utils/cmdqueue.h"
#include "utils/helper.h"

static int is_verbose_mode(int argc, char** argv);
static void socket_server_init();
static void socket_server_destroy();

extern int pq_core_init(void);
extern int process_pq_request(pq_request_t* req);

/*
 * All command request add to this queue will be
 * handle by the main thread.
 */
struct cmdqueue* cmdqueue_;

int main(int argc, char** argv)
{
    debug_verbose_mode(is_verbose_mode(argc, argv));

    /*
     * init hardware relate info,
     * and try to recovery pq config from storage file.
     */
    pq_core_init();

    cmdqueue_ = cmdqueue_create();

    socket_server_init();

    while (1) {
        cmdqueue_->wait(cmdqueue_);
        struct request_base* req = cmdqueue_->dequeu(cmdqueue_);
        if (!req) continue;

        switch (req->cmd) {
            case CMDQUEUE_PQ_REQUEST:
                process_pq_request((pq_request_t*)req);
                break;
            default:
                dlogw("unknow request [%s] cmd=%d", req->name, req->cmd);
                free(req);
                break;
        }
    }

    return 0;
}

/* socket packet receive state */
enum { RECV_IDLE, RECV_START, RECV_WAIT_PACKET };

#define RECV_BUF_SIZE (2048)

static struct server {
    pthread_t tid;
    int exit;

    int epollfd;
    int server_socketfd;
    int client;

    /* receiving packet info */
    int recv_state;
    int remaining_data_lenght;
    struct pq_package* packet;

    /* raw receive buf */
    char* recvbuf;
    int recv_len;
    int read_pos;
} server_;

static inline void set_fd_nonblock(int fd) {
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

static int epoll_add_fd(int epollfd, int fd) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        dloge("epoll_ctl EPOLL_CTL_ADD error!");
        return -1;
    }
    return 0;
}

static inline void epoll_remove_fd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

const int NO_CLIENT = -1;
static void socket_connect_handle(struct server *sdat);
static int socket_packet_handle(struct server *sdat);

static void *socket_server_thread(void *data)
{
    int socket_server = create_socket_server();
    if (socket_server < 0) {
        dloge("socket server init error");
        exit(1);
    }
    set_fd_nonblock(socket_server);

    dlogi("server init ok.");
    dlogi("wait for connection ...");

    struct server* sdat = (struct server*)data;
    sdat->server_socketfd = socket_server;
    sdat->client = NO_CLIENT;
    sdat->recv_state = RECV_IDLE;
    sdat->recvbuf = malloc(RECV_BUF_SIZE);
    sdat->recv_len = sdat->read_pos = 0;
    sdat->packet = NULL;
    sdat->remaining_data_lenght = 0;

    sdat->epollfd = epoll_create(1);
    if (sdat->epollfd < 0) {
        dloge("epoll_create failed.");
        return NULL;
    }

    /*
     * add server socket fd to epoll loop,
     * so that we can be notify on incoming connections.
     */
    epoll_add_fd(sdat->epollfd, sdat->server_socketfd);

    while (!sdat->exit) {
        struct epoll_event events[MAX_EPOLL_EVENTS] = {0};
        int count = epoll_wait(sdat->epollfd, events, MAX_EPOLL_EVENTS, -1);

        for (int i = 0; i < count; i++) {
            int eventfd = events[i].data.fd;
            uint32_t evs = events[i].events;

            if (((evs & EPOLLERR) || (evs & EPOLLHUP)) && !(evs & EPOLLIN)) {
                /* An error has occured on this fd */
                epoll_remove_fd(sdat->epollfd, eventfd);
                dlogw("epoll fd error, remove and close it!");
                if (eventfd == sdat->client) {
                    sdat->client = NO_CLIENT;
                    dlogw("close client socket connect!");
                }
                continue;
            }

            if (eventfd == sdat->server_socketfd) {
                /* new connection from remote */
                socket_connect_handle(sdat);
            } else if (eventfd == sdat->client) {
                /* incoming packets handle */
                if (socket_packet_handle(sdat) == 0) {
                    epoll_remove_fd(sdat->epollfd, eventfd);
                    sdat->client = NO_CLIENT;
                    dlogw("client socket close!");
                }
            }
        }
    }

    close(socket_server);
    dlogi("socket server die!");

    return "goodby";
}

static void socket_connect_handle(struct server *sdat)
{
    int client_fd = accept_client(sdat->server_socketfd);
    if (client_fd < 0) {
        dloge("accept error ?!!!");
        return;
    }

    if (sdat->client == NO_CLIENT) {
        set_fd_nonblock(client_fd);
        if (epoll_add_fd(sdat->epollfd, client_fd) == 0) {
            sdat->client = client_fd;
            return;
        }
    } else {
        dlogw("only support one client currently!");
    }
    close(client_fd);
}

static inline void trim_recvbuf(struct server *sdat) {
    if (sdat->read_pos == 0)
        return;
    dlogd("read position: %d, size=%d", sdat->read_pos, sdat->recv_len);
    for (int i = 0; i < sdat->recv_len; i++) {
        sdat->recvbuf[i] = sdat->recvbuf[sdat->read_pos];
        sdat->read_pos++;
    }
    sdat->read_pos = 0;
}

static inline void packet_info_reset(struct server *sdat) {
    sdat->packet = NULL;
    sdat->remaining_data_lenght = 0;
}

static inline void server_recv_info_reset(struct server *sdat) {
    sdat->recv_state = RECV_IDLE;
    sdat->remaining_data_lenght = 0;
    if (sdat->packet) {
        free(sdat->packet);
        sdat->packet = NULL;
    }
    sdat->recv_len = 0;
    sdat->read_pos = 0;
}

static int socket_packet_send(const char* buf, int size)
{
    dump_packet((struct pq_package*)buf);

    int sendcount = 0;

    while (sendcount < size) {
        int remaining = size - sendcount;
        int ret = send(server_.client, buf + sendcount, remaining, 0);

        if (ret == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            return -1;
        }
        sendcount += ret;
    }
    dlogi("send socket packet len: %d", sendcount);

    return sendcount;
}

static void cmdqueue_add_packet(struct pq_package* packet)
{
    pq_request_t* req = alloc_request(pq_request_t, CMDQUEUE_PQ_REQUEST);
    req->base.ack_callback = socket_packet_send;
    req->packet = packet;
    cmdqueue_->queue(cmdqueue_, &req->base);
}

static int socket_packet_handle(struct server* sdat)
{
    dlogd("start read position: %d, size=%d", sdat->read_pos, sdat->recv_len);
    char* p = sdat->recvbuf + sdat->recv_len;
    size_t count = read(sdat->client, p, (RECV_BUF_SIZE - sdat->recv_len));

    if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        /* no any data read from socket */
        return -1;
    } else if (count == 0 || count == -1) {
        /* socket disconnect, we should close this socket */
        server_recv_info_reset(sdat);
        return 0;
    } else {
        sdat->recv_len += count;
    }

    dlogd("receive socket packet: %d", count);

parse_data:
    switch (sdat->recv_state) {
        case RECV_IDLE:
            /* search the magic header */
            while (sdat->recv_len >= sizeof(struct pq_package)) {
                char* h = sdat->recvbuf + sdat->read_pos;
                if (h[0] == 'A' && h[1] == 'W' && h[2] == 'P' && h[3] == 'Q') {
                    sdat->recv_state = RECV_START;
                    break;
                }
                sdat->read_pos++;
                sdat->recv_len--;
            }

            if (sdat->recv_len < sizeof(struct pq_package)) {
                /* not a complete packet */
                trim_recvbuf(sdat);
            }
            break;
        case RECV_START: {
            struct pq_package header = {0};
            char* h = sdat->recvbuf + sdat->read_pos;
            memcpy(&header, h, sizeof(header));
            if (header.magic != PQ_PACKET_MAGIC) {
                /* shoule not happen! */
                dlogw("packet header corruption, retry search the header!");
                sdat->recv_state = RECV_IDLE;
                break;
            }

            sdat->packet = calloc(1, header.size);
            int copysize = (header.size <= sdat->recv_len) ? header.size : sdat->recv_len;
            memcpy(sdat->packet, h, copysize);
            sdat->recv_len -= copysize;
            sdat->read_pos += copysize;

            if (copysize == header.size) {
                /* great! we have a complete packet */
                cmdqueue_add_packet(sdat->packet);
                /* clean up */
                packet_info_reset(sdat);
                sdat->recv_state = RECV_IDLE;
                break;
            }

            /* not a complete packet, wait for remaining data */
            sdat->remaining_data_lenght = header.size - copysize;
            sdat->recv_state = RECV_WAIT_PACKET;
            break;
        }
        case RECV_WAIT_PACKET: {
            int copysize = (sdat->remaining_data_lenght <= sdat->recv_len)
                    ? sdat->remaining_data_lenght
                    : sdat->recv_len;
            char* s = sdat->recvbuf + sdat->read_pos;
            char* d = (char*)sdat->packet + (sdat->packet->size - sdat->remaining_data_lenght);
            memcpy(d, s, copysize);
            sdat->recv_len -= copysize;
            sdat->read_pos += copysize;

            if (copysize == sdat->remaining_data_lenght) {
                /* great! we have a complete packet */
                cmdqueue_add_packet(sdat->packet);
                /* clean up */
                packet_info_reset(sdat);
                sdat->recv_state = RECV_IDLE;
                break;
            }

            /* not a complete packet, wait for remaining data */
            sdat->remaining_data_lenght -= copysize;
            sdat->recv_state = RECV_WAIT_PACKET;
            break;
        }
        default:
            dlogw("invalid packet receive state!");
            break;
    }

    if (sdat->recv_len >= sizeof(struct pq_package))
        goto parse_data;

    trim_recvbuf(sdat);

    return count;
}

static void socket_server_init() {
    server_.exit = 0;
    pthread_create(&server_.tid, NULL, (void *)socket_server_thread, (void *)&server_);
}

static void socket_server_destroy() {
    server_.exit = 1;
    pthread_join(server_.tid, NULL);
}

static int is_verbose_mode(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0)
        return 1;
    return 0;
}

