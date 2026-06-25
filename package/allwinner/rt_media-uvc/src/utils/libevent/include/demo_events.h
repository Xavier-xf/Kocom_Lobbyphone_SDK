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

#ifndef __DEMO_EVENTS_H__
#define __DEMO_EVENTS_H__

#include <stdbool.h>
#include <sys/select.h>

#include "base_list.h"

#define max(a, b) ({				\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	__a > __b ? __a : __b;			\
})

enum demo_event_dispatch_status {
    EVENT_DISPATCH_DEFAULT = 0x0,
    EVENT_DISPATCH_CHECK = 0x100,
    EVENT_DISPATCH_AGAIN = 0x200,
    EVENT_DISPATCH_END   = 0x300,
};

struct demo_events {
    struct list_head events; //event_fd
    bool done;

    int maxfd;
    fd_set rfds; //EVENT_READ
    fd_set wfds; //EVENT_WRITE
    fd_set efds; //EVENT_EXCEPTION

    enum demo_event_dispatch_status dispatch_status;
};

enum demo_event_type {
    EVENT_READ = 1,
    EVENT_WRITE = 2,
    EVENT_EXCEPTION = 4,
};

void demo_events_watch_fd(struct demo_events *events, int fd, enum demo_event_type type,
		     void(*callback)(void *), void *priv);
void demo_events_unwatch_fd(struct demo_events *events, int fd, enum demo_event_type type);

bool demo_events_loop(struct demo_events *events);
void demo_events_stop(struct demo_events *events);

void demo_events_init(struct demo_events *events);
void demo_events_cleanup(struct demo_events *events);

#endif
