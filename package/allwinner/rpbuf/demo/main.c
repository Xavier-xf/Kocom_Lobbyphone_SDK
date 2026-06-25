#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <librpbuf.h>

#define RPBUF_BUFFER_NAME_DEFAULT "rpbuf_demo"
#define RPBUF_BUFFER_LENGTH_DEFAULT 32

rpbuf_buffer_t *buffer;
uint8_t *recv_buf;
const char *name;

static void print_help_msg(void)
{
	printf("\n");
	printf("USAGE:\n");
	printf("  rpbuf_demo [OPTIONS]\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("  -s MESSAGE  : send MESSAGE\n");
	printf("  -d time     : delay free buffer (default: 100 ms)\n");
	printf("  -r          : receive messages\n");
	printf("  -t time     : specifies the time of receive messagess, unit:ms\n");
	printf("  -I ID       : specify rpbuf ctrl ID (default: 0)\n");
	printf("  -N NAME     : specify buffer name (default: \"%s\")\n",
			RPBUF_BUFFER_NAME_DEFAULT);
	printf("  -L LENGTH   : specify buffer length (default: %d bytes)\n",
			RPBUF_BUFFER_LENGTH_DEFAULT);
	printf("  -O OFFSET   : specify offset in buffer to store data (default: 0)\n");
	printf("\n");
	printf("e.g.\n");
	printf("  rpbuf_demo -s \"hello\" : send string \"hello\"\n");
	printf("  rpbuf_demo -r           : receive data forever\n");
	printf("  rpbuf_demo -r -t 1000   : receive data 1000 msecond\n");
	printf("\n");
}

static void delay(uint32_t t)
{
	int ret;
	struct timespec req, rem;

	req.tv_sec = t / 1000;
	req.tv_nsec = (t % 1000) * 1000 * 1000;

	while (1) {
		ret = nanosleep(&req, &rem);
		/* timeout */
		if (ret == 0)
			return;
		/* interrupted by a signal */
		req.tv_sec = rem.tv_sec;
		req.tv_nsec = rem.tv_nsec;
	}
}

static void buffer_rx_handler(int num)
{
	int ret, i;
	void *buf_addr;
	unsigned int data_len = 0, offset = 0;

	buf_addr = rpbuf_buffer_addr(buffer);

	ret = rpbuf_receive_buffer(buffer, &offset, &data_len, 0);
	if (ret < 0) {
		if (ret != -ETIMEDOUT)
			printf("receive buffer %s error, ret=%d\n", name, ret);
		else
			printf("receive buffer %s timeout\n", name);
	}

	memcpy(recv_buf, buf_addr + offset, data_len);

	printf("Data received (offset: %u, len: %u):\n", offset, data_len);
	for (i = 0; i < data_len; ++i) {
		printf(" 0x%x", recv_buf[i]);
	}
	printf("\n");
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int c;

	int do_send = 0;
	int do_recv = 0;

	int ctrl_id = 0;
	size_t len = RPBUF_BUFFER_LENGTH_DEFAULT;
	unsigned int offset = 0;
	uint32_t t = 100;
	int recv_time = -1;
	int recv_forever = 1;

	const char *data_send = NULL;
	unsigned int data_len;

	void *buf_addr;
	int i;
	long long_tmp;

	name = RPBUF_BUFFER_NAME_DEFAULT;

	if (argc <= 1) {
		print_help_msg();
		ret = -1;
		goto out;
	}

	while ((c = getopt(argc, argv, "hd:s:rt:I:N:L:O:")) != -1) {
		switch (c) {
		case 'h':
			print_help_msg();
			ret = 0;
			goto out;
		case 's':
			do_send = 1;
			data_send = optarg;
			break;
		case 'd':
			long_tmp = strtol(optarg, NULL, 0);
			if (long_tmp == LONG_MIN || long_tmp == LONG_MAX) {
				printf("Invalid input %s.\n", optarg);
				print_help_msg();
				ret = -EINVAL;
				goto out;
			}
			t = long_tmp;
			break;
		case 'r':
			do_recv = 1;
			break;
		case 't':
			recv_forever = 0;
			long_tmp = strtol(optarg, NULL, 0);
			if (long_tmp == LONG_MIN || long_tmp == LONG_MAX) {
				printf("Invalid input:%s.\n", optarg);
				print_help_msg();
				ret = -EINVAL;
				goto out;
			}
			recv_time = long_tmp;
			break;
		case 'I':
			long_tmp = strtol(optarg, NULL, 0);
			if (long_tmp == LONG_MIN || long_tmp == LONG_MAX) {
				printf("Invalid input %s.\n", optarg);
				print_help_msg();
				ret = -EINVAL;
				goto out;
			}
			ctrl_id = long_tmp;
			break;
		case 'N':
			name = optarg;
			break;
		case 'L':
			long_tmp = strtol(optarg, NULL, 0);
			if (long_tmp == LONG_MIN || long_tmp == LONG_MAX) {
				printf("Invalid input %s.\n", optarg);
				print_help_msg();
				ret = -EINVAL;
				goto out;
			}
			len = long_tmp;
			break;
		case 'O':
			long_tmp = strtol(optarg, NULL, 0);
			if (long_tmp == LONG_MIN || long_tmp == LONG_MAX) {
				printf("Invalid input %s.\n", optarg);
				print_help_msg();
				ret = -EINVAL;
				goto out;
			}
			offset = long_tmp;
			break;
		default:
			printf("Invalid option: -%c\n", c);
			print_help_msg();
			ret = -1;
			goto out;
		}
	}

	buffer = rpbuf_alloc_buffer(ctrl_id, name, len);
	if (!buffer) {
		printf("rpbuf_alloc_buffer for buffer %s (len: %zd) failed\n", name, len);
		ret = -1;
		goto out;
	}
	buf_addr = rpbuf_buffer_addr(buffer);

	if (do_send) {
		data_len = strlen(data_send);
		memcpy(buf_addr + offset, data_send, data_len);

		ret = rpbuf_transmit_buffer(buffer, offset, data_len);
		if (ret < 0) {
			printf("transmit buffer %s error\n", name);
			goto free_buffer;
		}
	}

	if (do_recv) {
		recv_buf = malloc(len);
		if (!recv_buf) {
			printf("no memory\n");
			ret = -ENOMEM;
			goto free_buffer;
		}

		if (!recv_forever)
			printf("will wait data %d msecond.\n", recv_time);

		rpbuf_install_rx_signal(buffer, buffer_rx_handler);
		if (recv_forever) {
			while (1)
				delay(recv_time);
		} else
			delay(recv_time);
		rpbuf_remove_rx_signal(buffer);
		free(recv_buf);
	}
	/*
	* TODO:
	*   We should ensure that remote won't use the buffer any more before
	*   we free it. Here we just delay() roughly. Use a more appropriate
	*   way in the future.
	*/
	delay(t);
	ret = 0;
free_buffer:
	rpbuf_free_buffer(buffer);
out:
	return ret;
}
