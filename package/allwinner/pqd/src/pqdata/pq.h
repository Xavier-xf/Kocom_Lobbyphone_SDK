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

#ifndef __PQ_H__
#define __PQ_H__

enum pq_package_type {
    PQ_REPLY = 0,
    PQ_HARDWARE_DETECT = 1,
    PQ_REGISTER_RW = 2,
    PQ_REGISTER_SET_RW = 3,

    PQ_GAMMA_LUT = 11,
    PQ_PEAK_SHARP = 12,
    PQ_LTI_SHARP = 13,
    PQ_COLOR_ENHANCE = 14,
    PQ_COLOR_MATRIX_IN = 15,
    PQ_COLOR_MATRIX_OUT = 16,
    PQ_FCM = 17,
};

#endif
