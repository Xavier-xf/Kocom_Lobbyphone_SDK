#ifndef __AW_UART_H__
#define __AW_UART_H__
#include "doorlock_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uart_info_s {
    int uart_fd;
    int baud_rate;
    int bit_width;
    int stop_bit;
    char parity;     // O:odd, E:even, N:no
    int block_mode;  // 0: non_block, 1:block
} uart_info_t;

int aw_uart_set_opt(int fd, int speed, int bits, char event, int stop);
int aw_uart_open(uart_info_t *uart_info, char *uart_name);
void aw_uart_close(uart_info_t* uart_info);
int aw_uart_set_non_block(uart_info_t* uart_info, bool non_block);
int aw_uart_write(uart_info_t* uart_info, uint8_t* buffer, uint32_t data_len);
int aw_uart_read(uart_info_t* uart_info, uint8_t* buffer, uint32_t data_len);

#ifdef __cplusplus
}
#endif

#endif
