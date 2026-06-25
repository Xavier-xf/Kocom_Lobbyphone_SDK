#include "aw_uart.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

int aw_uart_set_opt(int fd, int speed, int bits, char event, int stop)
{
    struct termios newtio, oldtio;

    if (tcgetattr(fd, &oldtio) != 0) {
        DOORLOCK_ERR("tcgetattr error\n");
        return -1;
    }
    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag |= CLOCAL | CREAD;
    newtio.c_cflag &= ~CSIZE;

    switch (bits) {
    case 7:
        newtio.c_cflag |= CS7;
        break;
    case 8:
        newtio.c_cflag |= CS8;
        break;
    }

    switch (event) {
    case 'O':
        newtio.c_cflag |= PARENB;
        newtio.c_cflag |= PARODD;
        newtio.c_iflag |= (INPCK | ISTRIP);
        break;
    case 'E':
        newtio.c_iflag |= (INPCK | ISTRIP);
        newtio.c_cflag |= PARENB;
        newtio.c_cflag &= ~PARODD;
        break;
    case 'N':
        newtio.c_cflag &= ~PARENB;
        break;
    }
    newtio.c_iflag |= IGNPAR;  // ignore parity error
    switch (speed) {
    case 2400:
        cfsetispeed(&newtio, B2400);
        cfsetospeed(&newtio, B2400);
        break;
    case 4800:
        cfsetispeed(&newtio, B4800);
        cfsetospeed(&newtio, B4800);
        break;
    case 9600:
        cfsetispeed(&newtio, B9600);
        cfsetospeed(&newtio, B9600);
        break;
    case 38400:
        cfsetispeed(&newtio, B38400);
        cfsetospeed(&newtio, B38400);
        break;
    case 57600:
        cfsetispeed(&newtio, B57600);
        cfsetospeed(&newtio, B57600);
        break;
    case 115200:
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    case 230400:
        cfsetispeed(&newtio, B230400);
        cfsetospeed(&newtio, B230400);
        break;
    case 460800:
        cfsetispeed(&newtio, B460800);
        cfsetospeed(&newtio, B460800);
        break;
    case 500000:
        cfsetispeed(&newtio, B500000);
        cfsetospeed(&newtio, B500000);
        break;
    case 576000:
        cfsetispeed(&newtio, B576000);
        cfsetospeed(&newtio, B576000);
        break;
    case 921600:
        cfsetispeed(&newtio, B921600);
        cfsetospeed(&newtio, B921600);
        break;
    case 1000000:
        cfsetispeed(&newtio, B1000000);
        cfsetospeed(&newtio, B1000000);
        break;
    case 1152000:
        cfsetispeed(&newtio, B1152000);
        cfsetospeed(&newtio, B1152000);
        break;
    case 1500000:
        cfsetispeed(&newtio, B1500000);
        cfsetospeed(&newtio, B1500000);
        break;
    case 2000000:
        cfsetispeed(&newtio, B2000000);
        cfsetospeed(&newtio, B2000000);
        break;
    case 2500000:
        cfsetispeed(&newtio, B2500000);
        cfsetospeed(&newtio, B2500000);
        break;
    case 3000000:
        cfsetispeed(&newtio, B3000000);
        cfsetospeed(&newtio, B3000000);
        break;
    case 3500000:
        cfsetispeed(&newtio, B3500000);
        cfsetospeed(&newtio, B3500000);
        break;
    case 4000000:
        cfsetispeed(&newtio, B4000000);
        cfsetospeed(&newtio, B4000000);
        break;
    default:
        cfsetispeed(&newtio, B115200);
        cfsetospeed(&newtio, B115200);
        break;
    }
    if (stop == 1)
        newtio.c_cflag &= ~CSTOPB;
    else if (stop == 2)
        newtio.c_cflag |= CSTOPB;

    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 1;
    tcflush(fd, TCIFLUSH);
    if ((tcsetattr(fd, TCSANOW, &newtio)) != 0) {
        DOORLOCK_ERR("tcsetattr error\n");
        return -1;
    }

    return 0;
}

int aw_uart_open(uart_info_t *uart_info, char *uart_name)
{
    if (uart_info->block_mode == 0) {
        uart_info->uart_fd = open(uart_name, O_RDWR | O_NOCTTY | O_NDELAY);  // O_NONBLOCK
    } else {
        uart_info->uart_fd = open(uart_name, O_RDWR | O_NOCTTY);
    }
    if (uart_info->uart_fd == -1) {
        DOORLOCK_ERR("open %s fail\n", uart_name);
        return -1;
    } else {
        DOORLOCK_DBG("open %s sucsses uart_fd = %d\n", uart_name, uart_info->uart_fd);
    }
    aw_uart_set_opt(uart_info->uart_fd, uart_info->baud_rate, uart_info->bit_width,
                    uart_info->parity, uart_info->stop_bit);

    return 0;
}

void aw_uart_close(uart_info_t* uart_info)
{
    if (uart_info->uart_fd > 0)
        close(uart_info->uart_fd);

    uart_info->uart_fd = 0;
}

int aw_uart_set_non_block(uart_info_t* uart_info, bool non_block)
{
    if (uart_info->uart_fd > 0) {
        int flags = fcntl(uart_info->uart_fd, F_GETFL, 0);
        if (flags == -1) {
            DOORLOCK_ERR("fcntl F_GETFL failed\n");
            return -1;
        }
        if (non_block) {
            flags |= O_NONBLOCK;
        } else {
            flags &= (~O_NONBLOCK);
        }
        if (fcntl(uart_info->uart_fd, F_SETFL, flags) == -1) {
            DOORLOCK_ERR("fcntl F_SETFL failed\n");
            return -1;
        }
        if (non_block) {
            uart_info->block_mode = 0;
        } else {
            uart_info->block_mode = 1;
        }
    } else {
        DOORLOCK_ERR("open uart first!\n");
        return -1;
    }

    return 0;
}

int aw_uart_write(uart_info_t* uart_info, uint8_t* buffer, uint32_t data_len)
{
    if (uart_info->uart_fd > 0) {
        if (uart_info->block_mode) {
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(uart_info->uart_fd, &wfds);
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 1000 * 1;
            if (select(uart_info->uart_fd + 1, NULL, &wfds, NULL, &timeout) <= 0) {
                return -1;
            } else {
                return write(uart_info->uart_fd, buffer, data_len);
            }
        } else {
            return write(uart_info->uart_fd, buffer, data_len);
        }
    } else {
        DOORLOCK_ERR("open uart first!\n");
        return -1;
    }

    return 0;
}

int aw_uart_read(uart_info_t* uart_info, uint8_t* buffer, uint32_t data_len)
{
    if (uart_info->uart_fd > 0) {
        if (uart_info->block_mode) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(uart_info->uart_fd, &rfds);
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 1000 * 1;
            if (select(uart_info->uart_fd + 1, &rfds, NULL, NULL, &timeout) <= 0) {
                return -1;
            } else {
                return read(uart_info->uart_fd, buffer, data_len);
            }
        } else {
            return read(uart_info->uart_fd, buffer, data_len);
        }
    } else {
        DOORLOCK_ERR("open uart first!\n");
        return -1;
    }

    return 0;
}
