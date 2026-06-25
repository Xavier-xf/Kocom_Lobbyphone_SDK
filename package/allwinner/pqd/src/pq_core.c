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
#include <string.h>

#include "cmdqueue.h"
#include "debug.h"
#include "pqal.h"
#include "pqdata/pqdata.h"
#include "sockets/protocol.h"
#include "utils/helper.h"
#include "hardwares/de20x/fcm/fcm.h"

static int pq_core(pq_request_t* request);

int process_pq_request(pq_request_t* req)
{
    if (debug_is_verbose_mode()) dump_packet(req->packet);

    struct pq_package* packet = req->packet;

    switch (packet->cmd) {
        case CMD_HEART_BEAT:
            dlogd("heart beat");
            req->base.ack_callback((const char*)packet, packet->size);
            break;
        case CMD_GET:
        case CMD_SET:
        case CMD_SAVE_CONFIG:
            pq_core(req);
            break;
        case CMD_READ_CONFIG:
            break;
        default:
            dlogw("unknow packet[%d] size=%d", packet->cmd, packet->size);
            break;
    }

    free(req->packet);
    free(req);
    return 0;
}

static struct pqal* pqal_;

static struct pq_package* create_reply_packet(int cmd, int ret)
{
    static struct pq_package* packet = NULL;
    if (!packet) {
        int size = sizeof(struct pq_package) + sizeof(struct pq_package_reply);
        packet = (struct pq_package*)malloc(size);
        packet->magic = PQ_PACKET_MAGIC;
        packet->size = size;
    }
    packet->cmd = cmd;
    struct pq_package_reply* reply = (struct pq_package_reply*)((char*)packet + sizeof(struct pq_package));
    reply->id = 0;
    reply->ret = ret;
    return packet;
}

static int pq_core(pq_request_t* request)
{
    struct pq_package* packet = request->packet;
    struct payload_head* head = (struct payload_head*)packet->data;

    switch (head->id) {
        case HARDWARE_INFO: {
            if (packet->cmd == CMD_GET) {
                struct platform_version_packet* hwinfo = (struct platform_version_packet*)packet->data;
                int id, version;
                pqal_->get_hardware_info(&id, &version);
                hwinfo->ic_code = id;
                hwinfo->de_version = version;
                request->base.ack_callback((const char*)packet, packet->size);
            } else {
                struct platform_version_packet* hwinfo = (struct platform_version_packet*)packet->data;
                int port = hwinfo->de_port;
                pqal_->de_index_switch(port);
                struct pq_package* reply = create_reply_packet(CMD_SET, 0);
                request->base.ack_callback((const char*)reply, reply->size);
            }
        } break;
        case SINGLE_REG:
            if (packet->cmd == CMD_GET) {
                struct register_packet* rdat = (struct register_packet*)packet->data;
                pqal_->get_registers(&rdat->reg, 1);
                request->base.ack_callback((const char*)packet, packet->size);
            } else {
                const struct register_packet* rdat = (const struct register_packet*)packet->data;
                pqal_->set_registers(&rdat->reg, 1);
                struct pq_package* reply = create_reply_packet(CMD_SET, 0);
                request->base.ack_callback((const char*)reply, reply->size);
            }
            break;
        case REG_SET:
            if (packet->cmd == CMD_GET) {
                struct register_set_packet* rdat = (struct register_set_packet*)packet->data;
                pqal_->get_registers(&rdat->reg[0], rdat->count);
                request->base.ack_callback((const char*)packet, packet->size);
            } else {
                const struct register_set_packet* rdat = (const struct register_set_packet*)packet->data;
                pqal_->set_registers(&rdat->reg[0], rdat->count);
                struct pq_package* reply = create_reply_packet(CMD_SET, 0);
                request->base.ack_callback((const char*)reply, reply->size);
            }
            break;
        case GAMMA_LUT:
            if (packet->cmd == CMD_GET) {
                char* data = packet->data;
                int size = packet->size - sizeof(struct pq_package);
                pqal_->get_gamma_lut(data, size);
                request->base.ack_callback((const char*)packet, packet->size);
            } else {
                const char* data = packet->data;
                int size = packet->size - sizeof(struct pq_package);
                pqal_->set_gamma_lut(data, size);
                struct pq_package* reply = create_reply_packet(CMD_SET, 0);
                request->base.ack_callback((const char*)reply, reply->size);
            }
            break;
        case ENHANCE:
            if (packet->cmd == CMD_GET) {
                char* data = packet->data;
                int size = packet->size - sizeof(struct pq_package);
                pqal_->get_color_enhance(data, size);
                request->base.ack_callback((const char*)packet, packet->size);
            } else {
                const char* data = packet->data;
                int size = packet->size - sizeof(struct pq_package);
                pqal_->set_color_enhance(data, size);
                struct pq_package* reply = create_reply_packet(CMD_SET, 0);
                request->base.ack_callback((const char*)reply, reply->size);
            }
            break;
        case COLOR_MATRIX_IN:
        case COLOR_MATRIX_OUT:
            if (packet->cmd == CMD_GET) {
                char* data = packet->data;
                int size = packet->size - sizeof(struct pq_package);
                pqal_->get_color_matrix(data, size);
                request->base.ack_callback((const char*)packet, packet->size);
            } else {
                const char* data = packet->data;
                int size = packet->size - sizeof(struct pq_package);
                pqal_->set_color_matrix(data, size);
                struct pq_package* reply = create_reply_packet(CMD_SET, 0);
                request->base.ack_callback((const char*)reply, reply->size);
            }
            break;
        case FCM_CTRL:
            if (packet->cmd == CMD_GET) {
                char* data = packet->data;
                int size = packet->size - sizeof(struct pq_package);

                /*
                 * we can not convert hardware register data to pqtool fcm packet data,
                 * so find data from the store pqdata.
                 */
                const struct hardware_data* hwdata = pq_hardware_data();
                int index = pqal_->get_packet_store_index(head->id, data, size);
                const void* p;
                int len;
                if (pqdata_find(hwdata->port, head->id, index, &p, &len) == 0) {
                    dlogd("find saved packet data, size=%d request size=%d", len, size);
                    memcpy(data, p, len);
                    if (debug_is_verbose_mode()) {
                        fcm_ctrl_para_t* c = malloc(sizeof(fcm_ctrl_para_t));
                        memcpy(c, p, len);
                        dump_fcm_ctrl_data("pack send to pqtool", c);
                        free(c);
                    }
                } else {
                    dloge("cant not find type=%d index=%d", head->id, index);
                }
                request->base.ack_callback((const char*)packet, packet->size);
            } else {
                const char* data = packet->data;
                int size = packet->size - sizeof(struct pq_package);
                pqal_->set_fcm(data, size);
                struct pq_package* reply = create_reply_packet(CMD_SET, 0);
                request->base.ack_callback((const char*)reply, reply->size);

                /*
                 * backup the fcm packet data,
                 * because we can not convert hardware data to packet data.
                 */
                const struct hardware_data* hwdata = pq_hardware_data();
                int index = pqal_->get_packet_store_index(head->id, data, size);
                pqdata_save(hwdata->port, head->id, index, data, size);
            }
            break;
    }

    if (packet->cmd == CMD_SAVE_CONFIG) {
        const struct hardware_data* hwdata = pq_hardware_data();
        const char* data = packet->data;
        int size = packet->size - sizeof(struct pq_package);
        int index = pqal_->get_packet_store_index(head->id, data, size);
        pqdata_save(hwdata->port, head->id, index, data, size);
        pqdata_store_to_file();
    }

    return 0;
}

static void pq_config_recovery(void)
{
    if (pqdata_init() != 0) {
        dloge("pqdata init failed, skip pq recovery!");
        return;
    }

    struct pqdata* cfgdata = 0;
    int count = pqdata_load(&cfgdata);

    while (count-- > 0) {
        struct pqdata* p = cfgdata++;
        dlogi("pq recovery: DE%d type=%d index=%d size=%d", p->port, p->type, p->index, p->size);

        pqal_->de_index_switch(p->port);

        switch (p->type) {
            case PQ_GAMMA_LUT:
                pqal_->set_gamma_lut(p->data, p->size);
                break;
            case PQ_PEAK_SHARP:
            case PQ_LTI_SHARP:
                break;
            case PQ_COLOR_ENHANCE:
                pqal_->set_color_enhance(p->data, p->size);
                break;
            case PQ_COLOR_MATRIX_IN:
            case PQ_COLOR_MATRIX_OUT:
                pqal_->set_color_matrix(p->data, p->size);
                break;
            case PQ_FCM:
                pqal_->init_fcm_hardware_table(p->data, p->size);
                break;
            default:
                break;
        }
    }

    pqal_->de_index_switch(0);

    if (!cfgdata)
        pqdata_free(cfgdata);
}

int pq_core_init(void)
{
    pqal_ = pq_hardware_init();
    if (pqal_ == NULL) {
        dloge("pq_core_init error!");
        return -1;
    }
    pq_config_recovery();
    return 0;
}

