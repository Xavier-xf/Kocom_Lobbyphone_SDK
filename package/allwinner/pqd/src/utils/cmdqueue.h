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

#ifndef _PQ_CMDQUEUE_H_
#define _PQ_CMDQUEUE_H_

#include <pthread.h>
#include "list_t.h"

enum cmdqueue_cmd_type {
    INVALID,

    /* pq control package */
    CMDQUEUE_PQ_REQUEST,
};

typedef int(*ack_pfn)(const char* buf, int size);

struct request_base {
    const char* name;
    list_t link;
    enum cmdqueue_cmd_type cmd;
    ack_pfn ack_callback;
};

struct cmdqueue {
    pthread_mutex_t lock;
    pthread_cond_t notfiy_cond;

    list_t request_list;

    /* wait for next request */
    void (*wait)(struct cmdqueue*);

    void (*queue )(struct cmdqueue*, struct request_base*);
    struct request_base*(*dequeu)(struct cmdqueue*);
};

/* client connect request */
struct pq_package;
typedef struct pq_request {
    struct request_base base;
    struct pq_package* packet;
} pq_request_t;

#define alloc_request(type, cmd) \
    (type*)cmdqueue_request_alloc(#cmd, cmd, sizeof(type))

struct cmdqueue* cmdqueue_create(void);
void cmdqueue_destroy(struct cmdqueue *cmdq);
void* cmdqueue_request_alloc(const char* name, enum cmdqueue_cmd_type type, size_t size);

#endif
