/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Generic Event Handling
 *
 * Copyright (C) 2018 Laurent Pinchart
 *
 * This file comes from the omap3-isp-live project
 * (git://git.ideasonboard.org/omap3-isp-live.git)
 *
 * Copyright (C) 2010-2011 Ideas on board SPRL
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 */

#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#include "include/demo_events.h"
#include "../debug/include/debug.h"

#define SELECT_TIMEOUT		2000		/* in milliseconds */

struct event_fd {
    struct list_head list;

    int fd;
    enum demo_event_type type;
    void (*callback)(void *priv);
    void *priv;
};

void demo_events_watch_fd(struct demo_events *events, int fd, enum demo_event_type type,
		     void(*callback)(void *), void *priv)
{
    struct event_fd *event;

    event = malloc(sizeof *event);
    if (event == NULL)
        return;

    event->fd = fd;
    event->type = type;
    event->callback = callback;
    event->priv = priv;

    switch (event->type) {
    case EVENT_READ:
        FD_SET(fd, &events->rfds);
        break;
    case EVENT_WRITE:
        FD_SET(fd, &events->wfds);
        break;
    case EVENT_EXCEPTION:
        FD_SET(fd, &events->efds);
        break;
    }

    events->maxfd = max(events->maxfd, fd);

    if (events->dispatch_status == EVENT_DISPATCH_CHECK) {
        events->dispatch_status = EVENT_DISPATCH_AGAIN;
        logv("changing event list!");
    }

    list_add_tail(&event->list, &events->events);
    logv("demo_events_watch_fd %p", event);
}

void demo_events_unwatch_fd(struct demo_events *events, int fd, enum demo_event_type type)
{
    struct event_fd *event = NULL;
    struct event_fd *entry, *next;
    int maxfd = 0;

    list_for_each_entry_safe(entry, next, &events->events, list) {
        if (entry->fd == fd && entry->type == type)
            event = entry;
        else
            maxfd = max(maxfd, entry->fd);
    }

    if (event == NULL)
        return;

    switch (event->type) {
    case EVENT_READ:
        FD_CLR(fd, &events->rfds);
        break;
    case EVENT_WRITE:
        FD_CLR(fd, &events->wfds);
        break;
    case EVENT_EXCEPTION:
        FD_CLR(fd, &events->efds);
        break;
    }

    events->maxfd = maxfd;

    if (events->dispatch_status == EVENT_DISPATCH_CHECK) {
        events->dispatch_status = EVENT_DISPATCH_AGAIN;
        logv("changing event list!");
    }

    list_del(&event->list);
    free(event);
    logv("demo_events_unwatch_fd %p", event);
}

static void demo_events_dispatch(struct demo_events *events, const fd_set *rfds,
			    const fd_set *wfds, const fd_set *efds)
{
    struct event_fd *event;
    struct event_fd *next;
    struct event_fd *entry, *next_entry;
    int _continue = 0;
    int redispatch = 0;
    int i;
    int event_processed_num = 0;
    void *event_processed[32] = {0};

_reeach:
    list_for_each_entry_safe(event, next, &events->events, list) {
        if (event_processed_num) {
            for (i = 0; i < event_processed_num; i++) {
                if (event_processed[i] == event) {
                    _continue = 1;
                    logv("event %p processed!", event);
                    break;
                }
            }
        }
        if (_continue) {
            _continue = 0;
            continue;
        }

        if (event->type == EVENT_READ && FD_ISSET(event->fd, rfds)) {
            event->callback(event->priv);
        } else if (event->type == EVENT_WRITE && FD_ISSET(event->fd, wfds)) {
            event_processed[event_processed_num] = event;
            event_processed_num++;
            event->callback(event->priv);
        } else if (event->type == EVENT_EXCEPTION && FD_ISSET(event->fd, efds)) {
            event_processed[event_processed_num] = event;
            event_processed_num++;
            events->dispatch_status = EVENT_DISPATCH_CHECK;
            event->callback(event->priv);
            logv("demo_events_dispatch exception %p", event);
            if (events->dispatch_status == EVENT_DISPATCH_AGAIN) {
                logv("changed event! reeach!");
                events->dispatch_status = EVENT_DISPATCH_DEFAULT;
                goto _reeach;
            }
        }

        /* If the callback stopped events processing, we're done. */
        if (events->done)
            break;
    }
}

bool demo_events_loop(struct demo_events *events)
{
    events->done = false;

    while (!events->done) {
        fd_set rfds;
        fd_set wfds;
        fd_set efds;
        int ret;

        struct timeval stTimeVal;
        stTimeVal.tv_sec = 2;
        stTimeVal.tv_usec = 0;

        rfds = events->rfds;
        wfds = events->wfds;
        efds = events->efds;

        ret = select(events->maxfd + 1, &rfds, &wfds, &efds, &stTimeVal);
        if (ret < 0) {
            /* EINTR means that a signal has been received, continue
             * to the next iteration in that case.
             */
            if (errno == EINTR)
                continue;

            logw("error: select failed with %d\n", errno);
            break;
        }

        demo_events_dispatch(events, &rfds, &wfds, &efds);
    }

    return !events->done;
}

void demo_events_stop(struct demo_events *events)
{
    events->done = true;
}

void demo_events_init(struct demo_events *events)
{
    memset(events, 0, sizeof *events);

    FD_ZERO(&events->rfds);
    FD_ZERO(&events->wfds);
    FD_ZERO(&events->efds);
    events->maxfd = 0;
    INIT_LIST_HEAD(&events->events);
}

void demo_events_cleanup(struct demo_events *events)
{
    while (!list_empty(&events->events)) {
        struct event_fd *event;

        event = list_first_entry(&events->events, typeof(*event), list);
        list_del(&event->list);
        free(event);
    }
}
