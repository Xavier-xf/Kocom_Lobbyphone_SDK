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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "list.h"
#include "cmdqueue.h"

static void cmdq_wait(struct cmdqueue* cmdq)
{
    pthread_mutex_lock(&cmdq->lock);
    if (!list_empty(&cmdq->request_list))
        goto out;
    pthread_cond_wait(&cmdq->notfiy_cond, &cmdq->lock);

out:
    pthread_mutex_unlock(&cmdq->lock);
}

static void queue(struct cmdqueue* cmdq, struct request_base* base)
{
    pthread_mutex_lock(&cmdq->lock);
    list_add(&base->link, &cmdq->request_list);
    pthread_cond_signal(&cmdq->notfiy_cond);
    pthread_mutex_unlock(&cmdq->lock);
}

static struct request_base* dequeue(struct cmdqueue* cmdq)
{
    struct request_base* out = NULL;

    pthread_mutex_lock(&cmdq->lock);
    if (list_empty(&cmdq->request_list))
        goto out;
    out = list_entry(cmdq->request_list.next, struct request_base, link);
    list_del(&out->link);
out:
    pthread_mutex_unlock(&cmdq->lock);
    return out;
}

struct cmdqueue* cmdqueue_create(void)
{
    struct cmdqueue *cmdq = malloc(sizeof(*cmdq));

    pthread_mutex_init(&cmdq->lock, NULL);
    pthread_cond_init(&cmdq->notfiy_cond, NULL);

    INIT_LIST_HEAD(&cmdq->request_list);
    cmdq->wait = cmdq_wait;
    cmdq->queue = queue;
    cmdq->dequeu = dequeue;

    return cmdq;
}

void cmdqueue_destroy(struct cmdqueue *cmdq)
{
    pthread_mutex_destroy(&cmdq->lock);
    pthread_cond_destroy(&cmdq->notfiy_cond);

    free(cmdq);
}

void* cmdqueue_request_alloc(const char* name,
        enum cmdqueue_cmd_type type, size_t size)
{
    struct request_base* out = malloc(size);
    memset(out, 0, size);
    INIT_LIST_HEAD(&out->link);
    out->name = name;
    out->cmd = type;
    return out;
}
