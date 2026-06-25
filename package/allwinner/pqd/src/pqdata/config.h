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

#ifndef __PQ_CONFIG_H__
#define __PQ_CONFIG_H__

#ifndef PQ_TCP_PORT
#define PQ_TCP_PORT 5005
#endif

#ifndef PQDATA_FILE_NAME
#define PQDATA_FILE_NAME "/etc/.sunxi_pqdata.bin"
#endif

#define MAX_EPOLL_EVENTS 64

#endif
