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

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#define logbufmax 4096
static char buf__[logbufmax];
static pthread_mutex_t lock__ = PTHREAD_MUTEX_INITIALIZER;
static int verbose_print_;
void debug_verbose_mode(int enable)   { verbose_print_ = enable; }
const int debug_is_verbose_mode(void) { return verbose_print_;   }

static inline void __log_vprint(const char* fmt, va_list ap)
{
    pthread_mutex_lock(&lock__);

    time_t t = time(NULL);
    struct tm time;
    localtime_r(&t, &time);

    char* p = buf__;
    char strtime[64];
    strtime[strftime(strtime, sizeof(strtime), "%Y-%m-%d %H:%M:%S", &time)] = '\0';

    int count = sprintf(buf__, "[%s] ", strtime);
    vsnprintf(p+count, logbufmax-count, fmt, ap);
    printf("%s\n", buf__);

    pthread_mutex_unlock(&lock__);
}

void error(const char *format, ...)
{
    va_list list;
    va_start(list, format);
    __log_vprint(format, list);
    va_end(list);
}

void warning(const char *format, ...)
{
    va_list list;
    va_start(list, format);
    __log_vprint(format, list);
    va_end(list);
}

void info(const char *format, ...)
{
    va_list list;
    va_start(list, format);
    __log_vprint(format, list);
    va_end(list);
}

void debug(const char *format, ...)
{
    if (!verbose_print_) return;

    va_list list;
    va_start(list, format);
    __log_vprint(format, list);
    va_end(list);
}

void verbose(const char *format, ...)
{
    if (!verbose_print_) return;

    va_list list;
    va_start(list, format);
    __log_vprint(format, list);
    va_end(list);
}

