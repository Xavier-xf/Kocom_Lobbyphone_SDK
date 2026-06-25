#ifndef __AW_RING_BUFFER_H__
#define __AW_RING_BUFFER_H__

#include <stdbool.h>
#include <stdio.h>

#include "doorlock_common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct ring_buffer_s {
    char *data;
    uint32_t buffer_size;
    uint32_t read_pos;
    uint32_t write_pos;
} ring_buffer_t;

int aw_ring_buffer_init(ring_buffer_t *ring_buffer, uint32_t buf_size);
int aw_ring_buffer_clear(ring_buffer_t *ring_buffer);
int aw_ring_buffer_deinit(ring_buffer_t *ring_buffer);
uint32_t aw_ring_buffer_write(ring_buffer_t *ring_buffer, uint8_t *data, uint32_t data_size);
uint32_t aw_ring_buffer_read(ring_buffer_t *ring_buffer, uint8_t *data, uint32_t data_size);
uint32_t aw_ring_buffer_peek(ring_buffer_t *ring_buffer, uint8_t *data, uint32_t data_size);
uint32_t aw_ring_buffer_get_writable_size(ring_buffer_t *ring_buffer);
uint32_t aw_ring_buffer_get_readable_size(ring_buffer_t *ring_buffer);
uint32_t aw_ring_buffer_get_buffer_size(ring_buffer_t *ring_buffer);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
