#include "aw_ringbuffer.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int aw_ring_buffer_init(ring_buffer_t *ring_buffer, uint32_t buf_size)
{
    ring_buffer->data = malloc(buf_size);
    if (ring_buffer->data == NULL) {
        DOORLOCK_ERR("malloc %d failed\n", buf_size);
        return -1;
    }

    ring_buffer->buffer_size = buf_size;
    ring_buffer->read_pos = 0;
    ring_buffer->write_pos = 0;

    return 0;
}

int aw_ring_buffer_clear(ring_buffer_t *ring_buffer)
{
    if (ring_buffer->data == NULL) {
        DOORLOCK_ERR("Init RingBuffer first!!\n");
        return 0;
    }

    ring_buffer->read_pos = 0;
    ring_buffer->write_pos = 0;

    return 0;
}

int aw_ring_buffer_deinit(ring_buffer_t *ring_buffer)
{
    if (ring_buffer->data) {
        free(ring_buffer->data);
        ring_buffer->data = NULL;
    }

    ring_buffer->read_pos = 0;
    ring_buffer->write_pos = 0;

    return 0;
}

uint32_t aw_ring_buffer_write(ring_buffer_t *ring_buffer, uint8_t *data, uint32_t data_size)
{
    if (ring_buffer->data == NULL) {
        DOORLOCK_ERR("Init RingBuffer first!!\n");
        return 0;
    }
    if (data == NULL) {
        DOORLOCK_ERR("data == NULL!!\n");
        return 0;
    }

    uint32_t copy_write = ring_buffer->write_pos;
    uint32_t copy_read = ring_buffer->read_pos;
    uint32_t writable_size = 0;
    if (copy_read > copy_write) {
        writable_size = copy_read - copy_write - 1;
    } else {
        writable_size = copy_read + ring_buffer->buffer_size - copy_write - 1;
    }

    if (data_size > writable_size) {
        DOORLOCK_DBG("RingBuffer writable size = %d, you intend to write too much data %d\n",
                    writable_size, data_size);
        return 0;
    }
    if (ring_buffer->buffer_size - ring_buffer->write_pos >= data_size) {
        memcpy(ring_buffer->data + ring_buffer->write_pos, data, data_size);
    } else {
        uint32_t tmp_len = ring_buffer->buffer_size - ring_buffer->write_pos;
        memcpy(ring_buffer->data + ring_buffer->write_pos, data, tmp_len);
        memcpy(ring_buffer->data, data + tmp_len, data_size - tmp_len);
    }
    ring_buffer->write_pos = (ring_buffer->write_pos + data_size) % ring_buffer->buffer_size;

    return data_size;
}

uint32_t aw_ring_buffer_read(ring_buffer_t *ring_buffer, uint8_t *data, uint32_t data_size)
{
    if (ring_buffer->data == NULL) {
        DOORLOCK_ERR("Init RingBuffer first!!\n");
        return 0;
    }
    if (data == NULL) {
        DOORLOCK_ERR("data == NULL!!\n");
        return 0;
    }

    uint32_t copy_write = ring_buffer->write_pos;
    uint32_t copy_read = ring_buffer->read_pos;
    uint32_t readable_size = 0;
    if (copy_read > copy_write) {
        readable_size = copy_write + ring_buffer->buffer_size - copy_read;
    } else {
        readable_size = copy_write - copy_read;
    }
    if (data_size > readable_size) {
        // DOORLOCK_DBG("RingBuffer readable size = %d, you intend to read too much data %d\n",
        // readable_size, data_size);
        return 0;
    }
    if (ring_buffer->buffer_size - ring_buffer->read_pos >= data_size) {
        memcpy(data, ring_buffer->data + ring_buffer->read_pos, data_size);
    } else {
        uint32_t tmp_len = ring_buffer->buffer_size - ring_buffer->read_pos;
        memcpy(data, ring_buffer->data + ring_buffer->read_pos, tmp_len);
        memcpy(data + tmp_len, ring_buffer->data, data_size - tmp_len);
    }
    ring_buffer->read_pos = (ring_buffer->read_pos + data_size) % ring_buffer->buffer_size;

    return data_size;
}

uint32_t aw_ring_buffer_peek(ring_buffer_t *ring_buffer, uint8_t *data, uint32_t data_size)
{
    if (ring_buffer->data == NULL) {
        DOORLOCK_ERR("Init RingBuffer first!!\n");
        return 0;
    }
    if (data == NULL) {
        DOORLOCK_ERR("data == NULL!!\n");
        return 0;
    }

    uint32_t copy_write = ring_buffer->write_pos;
    uint32_t copy_read = ring_buffer->read_pos;
    uint32_t readable_size = 0;
    if (copy_read > copy_write) {
        readable_size = copy_write + ring_buffer->buffer_size - copy_read;
    } else {
        readable_size = copy_write - copy_read;
    }
    if (data_size > readable_size) {
        // DOORLOCK_DBG("RingBuffer readable size = %d, you intend to query too much data %d\n",
        // readable_size, data_size);
        return 0;
    }
    if (ring_buffer->buffer_size - ring_buffer->read_pos >= data_size) {
        memcpy(data, ring_buffer->data + ring_buffer->read_pos, data_size);
    } else {
        uint32_t tmp_len = ring_buffer->buffer_size - ring_buffer->read_pos;
        memcpy(data, ring_buffer->data + ring_buffer->read_pos, tmp_len);
        memcpy(data + tmp_len, ring_buffer->data, data_size - tmp_len);
    }

    return data_size;
}

uint32_t aw_ring_buffer_get_writable_size(ring_buffer_t *ring_buffer)
{
    if (ring_buffer->data == NULL) {
        DOORLOCK_ERR("Init RingBuffer first!!\n");
        return 0;
    }

    uint32_t copy_write = ring_buffer->write_pos;
    uint32_t copy_read = ring_buffer->read_pos;
    uint32_t writable_size = 0;

    if (copy_read > copy_write) {
        writable_size = copy_read - copy_write - 1;
    } else {
        writable_size = copy_read + ring_buffer->buffer_size - copy_write - 1;
    }

    return writable_size;
}

uint32_t aw_ring_buffer_get_readable_size(ring_buffer_t *ring_buffer)
{
    if (ring_buffer->data == NULL) {
        DOORLOCK_ERR("Init RingBuffer first!!\n");
        return 0;
    }

    uint32_t copy_write = ring_buffer->write_pos;
    uint32_t copy_read = ring_buffer->read_pos;
    uint32_t readable_size = 0;

    if (copy_read > copy_write) {
        readable_size = copy_write + ring_buffer->buffer_size - copy_read;
    } else {
        readable_size = copy_write - copy_read;
    }

    return readable_size;
}

uint32_t aw_ring_buffer_get_buffer_size(ring_buffer_t *ring_buffer)
{
    if (ring_buffer->data == NULL) {
        DOORLOCK_ERR("Init RingBuffer first!!\n");
        return 0;
    }

    return ring_buffer->buffer_size;
}