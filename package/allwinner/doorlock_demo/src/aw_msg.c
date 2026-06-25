#include "aw_msg.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int msg_write_packet(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len)
{
    int ret = 0;
    int tmp_len = 0;
    uint32_t write_len = 0;
    uint32_t packet_len = 1024;

    uart_info_t *uart_info = &aw_dev_info->uart_info;

    while (write_len < len) {
        if (len - write_len > packet_len) {
            packet_len = 1024;
        } else {
            packet_len = len - write_len;
        }

        tmp_len = aw_uart_write(uart_info, &data[write_len], packet_len);
        if (tmp_len > 0) {
            write_len += tmp_len;
            DOORLOCK_DBG("len = %d, write_len = %d, tmp_len = %d, packet_len = %d\n", len,
                         write_len, tmp_len, packet_len);
        } else {
            usleep(1 * 1000);
        }
    }

    return write_len;
}

int msg_read_packet(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len)
{
    int ret = 0;
    uart_info_t *uart_info = &aw_dev_info->uart_info;

    ret = aw_uart_read(uart_info, data, len);

    return ret;
}

uint8_t get_parity_checksum(uint8_t *pack, uint16_t pack_len)
{
    unsigned short i;
    unsigned char check_sum = 0;

    for (i = 0; i < pack_len; i++) {
        check_sum ^= *pack++;
    }
    return check_sum;
}

msg_reply_verify_data_t g_verify_reply_data;
void msg_prepare_verify_reply_data(uint16_t usr_id, char *name, uint8_t admin, uint8_t id_type)
{
    g_verify_reply_data.user_id_leb = (uint8_t)usr_id;
    g_verify_reply_data.user_id_heb = (uint8_t)(usr_id >> 8);
    g_verify_reply_data.admin = admin;
    g_verify_reply_data.id_type = id_type;

    if (name != NULL) {
        strncpy((char *)g_verify_reply_data.user_name, name, USER_NAME_SIZE - 1);
        g_verify_reply_data.user_name[USER_NAME_SIZE - 1] = '\0';
    }
}

msg_reply_enroll_data_t g_enroll_reply_data;
void msg_prepare_enroll_reply_data(uint16_t usr_id, uint8_t face_direction, uint8_t enroll_state)
{
    g_enroll_reply_data.user_id_leb = (uint8_t)usr_id;
    g_enroll_reply_data.user_id_heb = (uint8_t)(usr_id >> 8);
    g_enroll_reply_data.face_direction = face_direction;
    g_enroll_reply_data.enroll_state = enroll_state;
}

msg_note_face_data_t g_note_face_data;
void msg_prepare_note_face_data(int16_t state, int16_t left, int16_t top, int16_t right,
                                int16_t bottom, int16_t yaw, int16_t pitch, int16_t roll)
{
    g_note_face_data.state = state;
    g_note_face_data.left = left;
    g_note_face_data.top = top;
    g_note_face_data.right = right;
    g_note_face_data.bottom = bottom;
    g_note_face_data.yaw = yaw;
    g_note_face_data.pitch = pitch;
    g_note_face_data.roll = roll;
}

int msg_send_reply(aw_dev_t *aw_dev_info, uint8_t mid, uint8_t reply_result)
{
    int ret = 0;
    int i;
    unsigned int reply_len = 0;

    pthread_mutex_lock(&aw_dev_info->write_mutex);
    /* 获取reply data len*/
    switch (mid) {
    case MID_RESET:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_VERIFY:
        if (reply_result == MR_SUCCESS)
            reply_len = sizeof(msg_reply_verify_data_t) + sizeof(msg_reply_data_t);
        else
            reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_ENROLL:
        if (reply_result == MR_SUCCESS)
            reply_len = sizeof(msg_reply_enroll_data_t) + sizeof(msg_reply_data_t);
        else
            reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_PALM_ENROLL_ITG:
        if (reply_result == MR_SUCCESS)
            reply_len = sizeof(msg_reply_enroll_data_t) + sizeof(msg_reply_data_t);
        else
            reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_ENROLL_ITG:
        if (reply_result == MR_SUCCESS)
            reply_len = sizeof(msg_reply_enroll_data_t) + sizeof(msg_reply_data_t);
        else
            reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_DELUSER:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_DELALL:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_FACERESET:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_START_OTA:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_CONFIG_BAUDRATE:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_SET_RELEASE_ENC_KEY:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_SET_DEBUG_ENC_KEY:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_OTA_HEADER:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_OTA_PACKET:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_SET_THRESHOLD_LEVEL:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_POWERDOWN:
        reply_len = sizeof(msg_reply_data_t);
        break;

    case MID_DEMOMODE:
        reply_len = sizeof(msg_reply_data_t);
        break;

    default:
        DOORLOCK_INFO("not support mid:%d\n", mid);
        break;
    }

    unsigned int msg_buf_len = sizeof(msg_t) + reply_len;
    msg_t *send_msg = (msg_t *)malloc(msg_buf_len);
    memset(send_msg, 0, msg_buf_len);

    send_msg->sync_word_heb = SYNC_WORD_H;
    send_msg->sync_word_leb = SYNC_WORD_L;
    send_msg->mid = MID_REPLY;
    send_msg->size_leb = (uint8_t)reply_len;
    send_msg->size_heb = (uint8_t)(reply_len >> 8);
    msg_reply_data_t *reply_data = (msg_reply_data_t *)send_msg->data;
    reply_data->mid = mid;
    reply_data->result = reply_result;

    switch (mid) {
    case MID_RESET:
        DOORLOCK_INFO("MID_RESET\n");
        break;

    case MID_VERIFY: {
        DOORLOCK_INFO("MID_VERIFY\n");
        if (reply_result == MR_SUCCESS) {
            msg_reply_verify_data_t *verify_reply_data =
                (msg_reply_verify_data_t *)reply_data->data;
            memcpy(verify_reply_data, &g_verify_reply_data, sizeof(msg_reply_verify_data_t));
        }
        break;
    }
    case MID_ENROLL: {
        DOORLOCK_INFO("MID_ENROLL\n");
        if (reply_result == MR_SUCCESS) {
            msg_reply_enroll_data_t *enroll_reply_data =
                (msg_reply_enroll_data_t *)reply_data->data;
            memcpy(enroll_reply_data, &g_enroll_reply_data, sizeof(msg_reply_enroll_data_t));
        }
        break;
    }
    case MID_PALM_ENROLL_ITG: {
        DOORLOCK_INFO("MID_PALM_ENROLL_ITG\n");
        if (reply_result == MR_SUCCESS) {
            msg_reply_enroll_data_t *enroll_reply_data =
                (msg_reply_enroll_data_t *)reply_data->data;
            memcpy(enroll_reply_data, &g_enroll_reply_data, sizeof(msg_reply_enroll_data_t));
        }
        break;
    }
    case MID_DELUSER:
        DOORLOCK_INFO("MID_DELUSER\n");
        break;
    case MID_DELALL:
        DOORLOCK_INFO("MID_DELALL\n");
        break;
    case MID_POWERDOWN:
        DOORLOCK_INFO("MID_POWERDOWN\n");
        break;
    default:
        DOORLOCK_INFO("not support mid:%d\n", mid);
        break;
    }
#if 0
    // no check sun print
    DOORLOCK_INFO("Before check sun, send_msg[%d]:", msg_buf_len);
    for (i = 0; i < msg_buf_len - 1; i++) {
        printf("%02x ", ((uint8_t *)send_msg)[i]);
    }
    printf("\n");
#endif
    int check_sum = get_parity_checksum((uint8_t *)send_msg + PARITY_CHECK_OFFSET,
                                        PARITY_CHECK_LENGTH(msg_buf_len));
    uint8_t *parity_check = ((uint8_t *)send_msg) + (msg_buf_len - 1);
    *parity_check = check_sum;
    // after check sun print
    DOORLOCK_INFO("After check sun, send_msg[%d]:", msg_buf_len);
    for (i = 0; i < msg_buf_len; i++) {
        printf("%02x ", ((uint8_t *)send_msg)[i]);
    }
    printf("\n");
    ret = msg_write_packet(aw_dev_info, (uint8_t *)send_msg, msg_buf_len);
    if (ret < 0) {
        DOORLOCK_ERR("msg_write_packet Error\n");
    }

    if (send_msg) {
        free(send_msg);
        send_msg = NULL;
    }
    pthread_mutex_unlock(&aw_dev_info->write_mutex);

    return ret;
}

int msg_send_note(aw_dev_t *aw_dev_info, uint8_t nid, uint8_t nid_result)
{
    int ret = 0;
    int i;
    unsigned int note_len;

    pthread_mutex_lock(&aw_dev_info->write_mutex);
    if (nid == NID_FACE_STATE)
        note_len = sizeof(msg_note_data_t) + sizeof(msg_note_face_data_t);
    else if (nid == NID_OTA_DONE)
        note_len = sizeof(msg_note_data_t) + 1;
    else
        note_len = sizeof(msg_note_data_t) + 0;

    unsigned int msg_buf_len = sizeof(msg_t) + note_len;
    msg_t *send_msg = (msg_t *)malloc(msg_buf_len);
    memset(send_msg, 0, msg_buf_len);

    send_msg->sync_word_heb = SYNC_WORD_H;
    send_msg->sync_word_leb = SYNC_WORD_L;
    send_msg->mid = MID_NOTE;
    send_msg->size_leb = (uint8_t)note_len;
    send_msg->size_heb = (uint8_t)(note_len >> 8);
    msg_note_data_t *note_data = (msg_note_data_t *)send_msg->data;
    note_data->nid = nid;

    switch (nid) {
    case NID_READY:
        DOORLOCK_INFO("NID_READY\n");
        break;

    case NID_FACE_STATE: {
        DOORLOCK_INFO("NID_FACE_STATE\n");
        msg_note_face_data_t *note_face = (msg_note_face_data_t *)note_data->data;
        memset(note_face, 0, sizeof(msg_note_face_data_t));
        note_face->state = nid_result;
        break;
    }
    case NID_UNKNOWNERROR:
        DOORLOCK_INFO("NID_UNKNOWNERROR\n");
        break;
    case NID_OTA_DONE:
        note_data->data[0] = nid_result;
        DOORLOCK_INFO("NID_OTA_DONE\n");
        break;
    case NID_AUTHORIZATION:
        DOORLOCK_INFO("NID_AUTHORIZATION\n");
        break;
    default:
        DOORLOCK_INFO("not support nid %d\n", nid);
        free(send_msg);
        send_msg = NULL;
        return -1;
    }

#if 0
    // no check sun print
    DOORLOCK_INFO("no check sun, send_msg[%d]:", msg_buf_len);
    for (i = 0; i < msg_buf_len - 1; i++) {
        printf("%02x ", ((uint8_t *)send_msg)[i]);
    }
    printf("\n");
#endif
    int check_sum = get_parity_checksum((uint8_t *)send_msg + PARITY_CHECK_OFFSET,
                                        PARITY_CHECK_LENGTH(msg_buf_len));
    uint8_t *parity_check = ((uint8_t *)send_msg) + (msg_buf_len - 1);
    *parity_check = check_sum;

// after check sun print
#if 0
    DOORLOCK_INFO("check sun, send_msg[%d]:", msg_buf_len);
    for (i = 0; i < msg_buf_len; i++) {
        printf("%02x ", ((uint8_t *)send_msg)[i]);
    }
    printf("\n");
#endif

    ret = msg_write_packet(aw_dev_info, (uint8_t *)send_msg, msg_buf_len);
    if (ret < 0) {
        DOORLOCK_ERR("msg_write_packet Error!!!\n");
    }

    if (send_msg) {
        free(send_msg);
        send_msg = NULL;
    }
    pthread_mutex_unlock(&aw_dev_info->write_mutex);

    return ret;
}
