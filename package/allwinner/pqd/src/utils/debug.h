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

#ifndef _pq_debug_h_
#define _pq_debug_h_

#define DLOG(method, mark, format, ...) \
    method(mark "%s: " format, __FUNCTION__, ##__VA_ARGS__)

#define dloge(format, ...) DLOG(error,   "<E> ", format, ##__VA_ARGS__)
#define dlogd(format, ...) DLOG(debug,   "<D> ", format, ##__VA_ARGS__)
#define dlogw(format, ...) DLOG(warning, "<W> ", format, ##__VA_ARGS__)
#define dlogi(format, ...) DLOG(info,    "<I> ", format, ##__VA_ARGS__)
#define dlogv(format, ...) DLOG(verbose, "<V> ", format, ##__VA_ARGS__)

void   error(const char *format, ...);
void warning(const char *format, ...);
void    info(const char *format, ...);
void   debug(const char *format, ...);
void verbose(const char *format, ...);

void debug_verbose_mode(int enable);
const int debug_is_verbose_mode(void);

#endif

