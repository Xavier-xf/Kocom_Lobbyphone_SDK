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
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "debug.h"
#include "fcm/fcm.h"
#include "hardwares/driver_api.h"
#include "pqal.h"
#include "sockets/protocol.h"

static int get_dispdev_fd(void) {
    static int devfd = -1;
    if (devfd < 0) {
        devfd = open("/dev/disp", O_RDWR);
    }
    return devfd;
}

// ------------------------------------------------------------------------------------------//
//                                 General Register RW                                       //
// ------------------------------------------------------------------------------------------//

static void register_packet_to_ioctl_data(
        const struct register_data* data, int count, struct register_info* out)
{
    for (int i = 0; i < count; i++) {
        out->offset = data->offset;
        out->value  = data->value;
        out++;
        data++;
    }
}

static void register_packet_from_ioctl_data(
        struct register_data* out, int count, const struct register_info* info)
{
    for (int i = 0; i < count; i++) {
        out->offset = info->offset;
        out->value  = info->value;
        out++;
        info++;
    }
}

int de20x_set_registers(const struct register_data* data, int count)
{
    struct register_info* infos = malloc(sizeof(struct register_info) * count);
    register_packet_to_ioctl_data(data, count, infos);

    const struct hardware_data* hwdat = pq_hardware_data();
    unsigned long arg[4] = {0};

    arg[0] = (unsigned long)PQ_SET_REG;
    arg[1] = (unsigned long)hwdat->port;
    arg[2] = (unsigned long)infos;
    arg[3] = (unsigned long)count;

    int ret = ioctl(get_dispdev_fd(), DISP_PQ_PROC, (unsigned long)arg);
    if (ret) {
        dloge("DISP_PQ_PROC PQ_SET_REG failed: %d", ret);
        free(infos);
        return -1;
    }

    free(infos);
    return 0;
}

int de20x_get_registers(struct register_data* data, int count)
{
    struct register_info* infos = malloc(sizeof(struct register_info) * count);

    const struct hardware_data* hwdat = pq_hardware_data();
    unsigned long arg[4] = {0};

    arg[0] = (unsigned long)PQ_GET_REG;
    arg[1] = (unsigned long)hwdat->port;
    arg[2] = (unsigned long)infos;
    arg[3] = (unsigned long)count;

    int ret = ioctl(get_dispdev_fd(), DISP_PQ_PROC, (unsigned long)arg);
    if (ret) {
        dloge("DISP_PQ_PROC PQ_GET_REG failed: %d", ret);
        free(infos);
        return -1;
    }

    register_packet_from_ioctl_data(data, count, infos);
    free(infos);
    return 0;
}

// ------------------------------------------------------------------------------------------//
//                                     Gamma lut                                             //
// ------------------------------------------------------------------------------------------//

static int packet_to_gamma_lut(const char* data, int size, struct hw_gamma_lut* out)
{
    if (size < 256 * 3) {
        dloge("packet size error, convert to hardware gamma lut failed!");
        return -1;
    }

    int i = 0;
    const unsigned char* p = (const unsigned char*)data;
    while (i < 256) {
        out->x[i] = (((uint32_t)p[i]) << 16) | (((uint32_t)p[i + 256]) << 8) | (p[i + 512]);
        i++;
    }
    return 0;
}

static int gamma_lut_to_packet(const struct hw_gamma_lut* lut, char* data)
{
    int i = 0;
    while (i < 256) {
        data[i]       = (char)(lut->x[i] >> 16) & 0xff;
        data[i + 256] = (char)(lut->x[i] >>  8) & 0xff;
        data[i + 512] = (char)(lut->x[i]      ) & 0xff;
        i++;
    }
    return 0;
}

int de20x_set_gamma_lut(const char *data, int size)
{
    const struct payload_head *head = (struct payload_head *)data;
    size -= sizeof(struct payload_head);
    struct hw_gamma_lut lut = {0};
    packet_to_gamma_lut(head->data, size, &lut);

    unsigned long arg[4] = {0};
    int ret = ioctl(get_dispdev_fd(), DISP_LCD_GAMMA_CORRECTION_ENABLE, arg);
    if (ret) {
        dloge("DISP_LCD_GAMMA_CORRECTION_ENABLE failed: %d", ret);
        return -1;
    }

    const struct hardware_data *hwdat = pq_hardware_data();

    arg[0] = (unsigned long)hwdat->port;
    arg[1] = (unsigned long)&lut;
    arg[2] = (unsigned long)sizeof(lut);

    ret = ioctl(get_dispdev_fd(), DISP_LCD_SET_GAMMA_TABLE, (unsigned long)arg);
    if (ret) {
        dloge("DISP_LCD_GAMMA_TABLE failed: %d", ret);
        return -1;
    }
    return 0;
}

int de20x_get_gamma_lut(char *data, int size)
{
    const struct hardware_data *hwdat = pq_hardware_data();
    struct hw_gamma_lut lut = {0};
    unsigned long arg[4] = {0};

    arg[0] = (unsigned long)hwdat->port;
    arg[1] = (unsigned long)&lut;
    arg[2] = (unsigned long)sizeof(lut);

    int ret = ioctl(get_dispdev_fd(), DISP_LCD_GET_GAMMA_TABLE, (unsigned long)arg);
    if (ret) {
        dloge("DISP_LCD_GET_GAMMA_TABLE failed: %d", ret);
        return -1;
    }
    struct payload_head *head = (struct payload_head *)data;
    gamma_lut_to_packet(&lut, head->data);

    return 0;
}

// ------------------------------------------------------------------------------------------//
//                                     Color Enhance                                         //
// ------------------------------------------------------------------------------------------//

/* packet from pqtool */
struct enhance_packet {
    int id;
    char contrast;
    char brightness;
    char saturation;
    char hue;
} __attribute__((packed));

int de20x_set_enhance(const char *data, int size)
{
    const struct enhance_packet *packet = (const struct enhance_packet *)data;
    struct enhance_data enhance;

    enhance.contrast = packet->contrast;
    enhance.brightness = packet->brightness;
    enhance.saturation = packet->saturation;
    enhance.hue = packet->hue;
    enhance.cmd = 0;
    enhance.read = 0;

    const struct hardware_data *hwdat = pq_hardware_data();
    unsigned long arg[4] = {0};
    arg[0] = (unsigned long)PQ_COLOR_MATRIX;
    arg[1] = (unsigned long)hwdat->port;
    arg[2] = (unsigned long)&enhance;

    int ret = ioctl(get_dispdev_fd(), DISP_PQ_PROC, (unsigned long)arg);
    if (ret) {
        dloge("DISP_PQ_PROC PQ_COLOR_MATRIX failed: %d", ret);
        return -1;
    }
    return 0;
}

int de20x_get_enhance(char *data, int size)
{
    const struct hardware_data *hwdat = pq_hardware_data();
    struct enhance_data enhance;

    memset(&enhance, 0, sizeof(enhance));
    enhance.cmd = 0;
    enhance.read = 1;

    unsigned long arg[4] = {0};
    arg[0] = (unsigned long)PQ_COLOR_MATRIX;
    arg[1] = (unsigned long)hwdat->port;
    arg[2] = (unsigned long)&enhance;

    int ret = ioctl(get_dispdev_fd(), DISP_PQ_PROC, (unsigned long)arg);
    if (ret) {
        dloge("DISP_PQ_PROC PQ_COLOR_MATRIX failed: %d", ret);
        return -1;
    }

    struct enhance_packet *packet = (struct enhance_packet *)data;
    packet->contrast = enhance.contrast;
    packet->brightness = enhance.brightness;
    packet->saturation = enhance.saturation;
    packet->hue = enhance.hue;

    return 0;
}

// ------------------------------------------------------------------------------------------//
//                                     Color Matrix                                          //
// ------------------------------------------------------------------------------------------//

/*
 * matrix from pqtool to pqd
 */
struct matrix_packet {
    int id;
    char space;
    char data[0]; /* float * 12 */
} __attribute__((packed));

struct float_matrix {
    float c00;
    float c01;
    float c02;
    float c10;
    float c11;
    float c12;
    float c20;
    float c21;
    float c22;
    float offset0;
    float offset1;
    float offset2;
};

static int packet_to_matrix(const struct float_matrix* in, struct matrix4x4* matrix)
{
    matrix->x00 = ceil((double)(in->c00) * 4096);
    matrix->x01 = ceil((double)(in->c01) * 4096);
    matrix->x02 = ceil((double)(in->c02) * 4096);
    matrix->x03 = ceil((double)(in->offset0) * 4096);

    matrix->x10 = ceil((double)(in->c10) * 4096);
    matrix->x11 = ceil((double)(in->c11) * 4096);
    matrix->x12 = ceil((double)(in->c12) * 4096);
    matrix->x13 = ceil((double)(in->offset1) * 4096);

    matrix->x20 = ceil((double)(in->c20) * 4096);
    matrix->x21 = ceil((double)(in->c21) * 4096);
    matrix->x22 = ceil((double)(in->c22) * 4096);
    matrix->x23 = ceil((double)(in->offset2) * 4096);

    matrix->x30 = 0;
    matrix->x31 = 0;
    matrix->x32 = 0;
    matrix->x33 = 0x00001000;

    return 0;
}

static int matrix_to_packet(const struct matrix4x4* matrix, struct float_matrix* out)
{
    out->c00 = matrix->x00 / 4096.0;
    out->c01 = matrix->x01 / 4096.0;
    out->c02 = matrix->x02 / 4096.0;
    out->c10 = matrix->x10 / 4096.0;
    out->c11 = matrix->x11 / 4096.0;
    out->c12 = matrix->x12 / 4096.0;
    out->c20 = matrix->x20 / 4096.0;
    out->c21 = matrix->x21 / 4096.0;
    out->c22 = matrix->x22 / 4096.0;
    out->offset0 = matrix->x03 / 4096.0;
    out->offset1 = matrix->x13 / 4096.0;
    out->offset2 = matrix->x23 / 4096.0;

    return 0;
}

int de20x_set_color_matrix(const char *data, int size)
{
    const struct matrix_packet *mp = (struct matrix_packet *)data;
    size -= sizeof(struct matrix_packet);
    struct float_matrix fmatrix = {0};
    struct matrix_user mu = {0};

    if (size < sizeof(struct float_matrix)) {
        dloge("matrix packet size=%d, sizeof(struct float_matrix)=%d",
                size, sizeof(struct float_matrix));
        return -1;
    }
    dlogi("matrix packet size=%d, sizeof(struct float_matrix)=%d",
            size, sizeof(struct float_matrix));

    memcpy(&fmatrix, mp->data, sizeof(fmatrix));
    packet_to_matrix(&fmatrix, &mu.matrix);
    mu.cmd  = mp->id == COLOR_MATRIX_IN ? 1 : 2;
    mu.read = 0;
    mu.choice = mp->space;

    const struct hardware_data *hwdat = pq_hardware_data();

    unsigned long arg[4] = {0};
    arg[0] = (unsigned long)PQ_COLOR_MATRIX;
    arg[1] = (unsigned long)hwdat->port;
    arg[2] = (unsigned long)&mu;

    int ret = ioctl(get_dispdev_fd(), DISP_PQ_PROC, (unsigned long)arg);
    if (ret) {
        dloge("DISP_PQ_PROC failed: %d", ret);
        return -1;
    }

    return 0;
}

int de20x_get_color_matrix(char *data, int size)
{
    struct matrix_packet *mp = (struct matrix_packet *)data;
    struct matrix_user mu = {0};
    struct float_matrix fmatrix = {0};
    const struct hardware_data *hwdat = pq_hardware_data();

    mu.cmd = mp->id == COLOR_MATRIX_IN ? 1 : 2;
    mu.read = 1;
    mu.choice = mp->space;

    unsigned long arg[4] = {0};
    arg[0] = (unsigned long)PQ_COLOR_MATRIX;
    arg[1] = (unsigned long)hwdat->port;
    arg[2] = (unsigned long)&mu;

    int ret = ioctl(get_dispdev_fd(), DISP_PQ_PROC, (unsigned long)arg);
    if (ret) {
        dloge("DISP_PQ_PROC failed: %d", ret);
        return -1;
    }

    matrix_to_packet(&mu.matrix, &fmatrix);
    memcpy(mp->data, &fmatrix, sizeof(fmatrix));
    return 0;
}

// ------------------------------------------------------------------------------------------//
//                                          FCM                                              //
// ------------------------------------------------------------------------------------------//

int de20x_get_fcm_packet_index(const char *data, int size)
{
    struct fcm_ctrl_para para;

    if (size < sizeof(para)) {
        dloge("packet size error, size=%d sizeof(para)=%d", size, sizeof(para));
        return -1;
    }
    memcpy(&para, data, sizeof(para));
    return para.LUT_ID;
}

/*
 * cmd: 0 -- update fcm table and enable this table
 *      1 -- update fcm table but DO NOT enable it
 *      2 -- read fcm table from hardware
 */
static int de20x_set_fcm_internal(const char* data, int size, int cmd)
{
    if (size < sizeof(struct fcm_ctrl_para)) {
        dloge("packet size error, size=%d sizeof(para)=%d", size, sizeof(struct fcm_ctrl_para));
        return -1;
    }
    dloge("fcm packet size=%d sizeof(para)=%d", size, sizeof(struct fcm_ctrl_para));

    struct fcm_info* info = malloc(sizeof(*info));
    struct fcm_ctrl_para* para = malloc(sizeof(*para));
    memcpy(para, data, sizeof(*para));
    if (debug_is_verbose_mode()) dump_fcm_ctrl_data("pqtool input", para);

    info->cmd = cmd;
    fcm_convert_pqtool_input(para, &info->fcm_data, debug_is_verbose_mode());

    const struct hardware_data* hwdat = pq_hardware_data();
    unsigned long arg[4] = {0};

    arg[0] = (unsigned long)PQ_FCM;
    arg[1] = (unsigned long)hwdat->port;
    arg[2] = (unsigned long)info;

    int ret = ioctl(get_dispdev_fd(), DISP_PQ_PROC, (unsigned long)arg);
    if (ret) {
        dloge("DISP_PQ_PROC PQ_FCM failed: %d port=%d lut_id=%d", ret, hwdat->port, info->fcm_data.lut_id);
        return -1;
    }

    free(info);
    free(para);

    return 0;
}

int de20x_set_fcm(const char* data, int size, int cmd)
{
    return de20x_set_fcm_internal(data, size, 0);
}

int de20x_init_fcm_hardware_table(const char* data, int size)
{
    return de20x_set_fcm_internal(data, size, 1);
}

int de20x_get_fcm(char *data, int size)
{
    /*
     * we can not convert hardware register data to pqtool fcm packet data,
     * so find data from the store pqdata.
     */
    return 0;
}

