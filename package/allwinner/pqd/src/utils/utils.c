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

#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "debug.h"

int get_hardware_id(void) {
#define HW_SYS_INFO "/sys/class/sunxi_info/sys_info"

    int ret = -1;
    off_t off_end;
    char *t_buff = NULL, *buf = NULL, ne[10];
    int fd = -1;

    fd = open(HW_SYS_INFO, O_RDONLY);
    if (fd < 0) {
        dloge("open '%s' err %d.", HW_SYS_INFO, fd);
        return -1;
    } else {
        off_end = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);
        if (0 != lseek(fd, 0, SEEK_CUR)) {
            dloge("lseek '%s' err.", HW_SYS_INFO);
            close(fd);
            return -1;
        }
    }
    t_buff = malloc(sizeof(char) * off_end);
    if (t_buff == NULL) {
        dloge("malloc failed!");
        close(fd);
        return -1;
    }
    if (read(fd, t_buff, off_end) <= 0) {
        goto ret;
    }

    buf = strstr(t_buff, "sunxi_batchno");
    if (buf == NULL) {
        dloge("sunxi can find sunxi_batchno.");
        goto ret;
    }
    buf = strstr(buf, "0x");
    if (buf == NULL) {
        dloge("sunxi can find 0x.");
        goto ret;
    }
    memcpy(ne, buf + 2, 4);
    ne[4] = '\0';
    ret = atoi(ne);

ret:
    dlogi("ic hardware: %x", ret);
    free(t_buff);
    close(fd);
    return ret;
}

