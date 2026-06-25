
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/debug.h"
#include "sockets/socket.h"
#include "sockets/protocol.h"
#include "hardwares/de20x/fcm/fcm.h"

static char* fcmpacket;
static int total_recv_len = 0;
static int request_size = sizeof(struct pq_package) + sizeof(struct fcm_ctrl_para);

void receive_packet_handler(struct socket_client* client, const char* buf, int size)
{
    char *p = fcmpacket + total_recv_len;
    memcpy(p, buf, size);
    total_recv_len += size;

    printf("--- receive packet size: %d, total_recv_len = %d\n", size, total_recv_len);

    if (total_recv_len >= request_size) {
        struct fcm_ctrl_para* para = (struct fcm_ctrl_para*)(fcmpacket + sizeof(struct pq_package));
        dump_fcm_ctrl_data("fcm from pqd", para);

        struct pq_package* head = (struct pq_package*)fcmpacket;

        printf("+++++ packet size: %d\n", head->size);
        printf("+++++ packet size: %d\n", head->size);
    }
}


static void send_fcm_read_packet(int fd)
{
    int size = sizeof(struct pq_package) + sizeof(struct fcm_ctrl_para);
    char* buf = malloc(size);
    memset(buf, 0, size);

    struct pq_package* head = (struct pq_package*)buf;
    head->magic = PQ_PACKET_MAGIC;
    head->cmd  = 0x01;
    head->type = 0x00;
    head->size = size;

    struct fcm_ctrl_para* fcm = (struct fcm_ctrl_para*)(buf + sizeof(struct pq_package));
    fcm->id = 17;
    fcm->LUT_ID = 4;

    int send = write(fd, buf, size);
    printf("+++ send pack size: %d request size: %d\n", send, size);
    free(buf);
}


int main(int argc, char** argv)
{
    int socket = create_socket_client();
    struct socket_client* client = socket_client_create(socket, receive_packet_handler);

    fcmpacket = malloc(request_size);
    memset(fcmpacket, 0, request_size);

    send_fcm_read_packet(socket);

    while (1) {
        usleep(1000 * 1000);
    }

    return 0;
}

