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

#ifndef _FCM_CONFIG_H_
#define _FCM_CONFIG_H_

#ifdef FMC_10BIT_ENABLE
#define HUE_MAX_NUM 16383
#define SAT_MAX_NUM 4095
#define LUM_MAX_NUM 1023
#else
#define HUE_MAX_NUM 4095
#define SAT_MAX_NUM 1023
#define LUM_MAX_NUM 255
#endif

#endif
