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
#include "md5.h"

#define RPBUF_BUFFER_NAME_DEFAULT "rpbuf_demo"
#define RPBUF_BUFFER_LENGTH_DEFAULT 32
#define RPBUF_BUFFER_CNT_DEFAULT 10

static void print_help_msg(void)
{
	printf("\n");
	printf("USAGE:\n");
	printf("  rpbuf_test [OPTIONS]\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("  -d time     : set data sending interval (default: 100 ms)\n");
	printf("  -s          : send test messages\n");
	printf("  -c          : send count (default: 10)\n");
	printf("  -r          : receive messages\n");
	printf("  -t time     : specifies the time of receive messagess, unit:ms\n");
	printf("  -I ID       : specify rpbuf ctrl ID (default: 0)\n");
	printf("  -N NAME     : specify buffer name (default: \"%s\")\n",
			RPBUF_BUFFER_NAME_DEFAULT);
	printf("  -L LENGTH   : specify buffer length (default: %d bytes)\n",
			RPBUF_BUFFER_LENGTH_DEFAULT);
	printf("\n");
	printf("e.g.\n");
	printf("  rpbuf_test -L 0x1000 -c 10 -s    : send 10 test data, size=0x1000\n");
	printf("  rpbuf_test -L 0x1000 -r          : receive test data forever, size=0x1000\n");
	printf("  rpbuf_test -L 0x1000 -r -t 1000  : receive test data 1 second, size=0x1000\n");
	printf("\n");
}

static void delay(uint32_t t)
{
	struct timeval d;

	d.tv_sec = t / 1000;
	d.tv_usec = (t % 1000) * 1000;

	select(0, NULL, NULL, NULL, &d);
}

static void mkdata(uint8_t *buffer, int size)
{
	int i;
	int data_len = size - 16;
	uint32_t *pdate = (uint32_t *)buffer;

	srand((int)time(NULL));

	/* generate random data */
	for (i = 0; i < (data_len / 4); i++)
		pdate[i] = (uint32_t)rand();

	md5_digest(buffer, data_len, &buffer[data_len]);

	if (data_len > 16)
		data_len = 16;

	printf("data:");
	for (i = 0; i < data_len; i++)
		printf("%02x", buffer[i]);
	printf("... [md5:");
	for (i = 0; i < 16; i++)
		printf("%02x", buffer[size - 16 + i]);
	printf("]\n\n");
}

static int checkdata(uint8_t *buffer, int size)
{
	int i;
	int data_len = size - 16;
	uint8_t digest[16];

	md5_digest(buffer, data_len, digest);

	if (data_len > 16)
		data_len = 16;

	printf("data:");
	for (i = 0; i < data_len; i++)
		printf("%02x", buffer[i]);
	printf("... check:");
	for (i = 0; i < 16; i++) {
		printf("%02x", buffer[size - 16 + i]);
		if (buffer[size - 16 + i] != digest[i])
			break;
	}

	if (i != 16) {
		printf(" failed\n\n");
		for (i = 0; i < 16; i++)
			printf("%02x", buffer[size - 16 + i]);
		printf(" <-> cur:");
		for (i = 0; i < 16; i++)
			printf("%02x", digest[i]);
		return 0;
	} else {
		printf(" success\n\n");
		return 1;
	}
}

static long long get_currenttime()
{
	struct timeval time;

	gettimeofday(&time, NULL);

	return (time.tv_sec * 1000000 + time.tv_usec);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int c;
	int cnt = RPBUF_BUFFER_CNT_DEFAULT;

	int do_send = 0;
	int do_recv = 0;

	int ctrl_id = 0;
	const char *name = RPBUF_BUFFER_NAME_DEFAULT;
	size_t len = RPBUF_BUFFER_LENGTH_DEFAULT;
	uint32_t offset = 0;
	uint32_t t = 100;
	int recv_time = -1;
	int recv_forever = 1;

	uint8_t *recv_buf;
	uint32_t data_len;

	rpbuf_buffer_t *buffer;
	void *buf_addr;
	int i;
	long long_tmp;
	long long start_usec, end_usec;
	float delta_usec;

	if (argc <= 1) {
		print_help_msg();
		ret = -1;
		goto out;
	}

	while ((c = getopt(argc, argv, "hd:sc:rt:I:N:L:")) != -1) {
		switch (c) {
		case 'h':
			print_help_msg();
			ret = 0;
			goto out;
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
		case 's':
			do_send = 1;
			break;
		case 'c':
			long_tmp = strtol(optarg, NULL, 0);
			if (long_tmp == LONG_MIN || long_tmp == LONG_MAX) {
				printf("Invalid input %s.\n", optarg);
				print_help_msg();
				ret = -EINVAL;
				goto out;
			}
			cnt = long_tmp;
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
		default:
			printf("Invalid option: -%c\n", c);
			print_help_msg();
			ret = -1;
			goto out;
		}
	}

	if (len < 16) {
		printf("len to short,automatic adjust to 32.\n");
		len = 32;
	}

	if (len % 4 != 0) {
		printf("len need to align to 4 bytes.\n");
		goto out;
	}

	buffer = rpbuf_alloc_buffer(ctrl_id, name, len);
	if (!buffer) {
		printf("rpbuf_alloc_buffer for buffer %s (len: %d) failed\n", name, len);
		ret = -1;
		goto out;
	}
	buf_addr = rpbuf_buffer_addr(buffer);

	if (do_send) {
		while (cnt--) {
			mkdata(buf_addr, len);
			start_usec = get_currenttime();
			ret = rpbuf_transmit_buffer(buffer, offset, len);
			if (ret < 0) {
				printf("transmit buffer %s error\n", name);
				goto free_buffer;
			}
			end_usec = get_currenttime();
			delta_usec = end_usec - start_usec;
			/* ping:rpbuf kernel framework layer transmit time */
			printf("ping: %fms\n", delta_usec / 1000);
			printf("bandwidth: N/AMbps\n");
			delay(t);
		}
	} else if (do_recv) {
		recv_buf = malloc(len);
		if (!recv_buf) {
			printf("no memory\n");
			ret = -ENOMEM;
			goto free_buffer;
		}

		if (!recv_forever)
			printf("will wait data %d msecond.\n", recv_time);
		while (1) {
			start_usec = get_currenttime();
			ret = rpbuf_receive_buffer(buffer, &offset, &data_len, recv_time);
			if (ret < 0) {
				if (ret != -ETIMEDOUT)
					printf("receive buffer %s error, ret=%d\n", name, ret);
				else
					printf("receive buffer %s timeout\n", name);
				break;
			}
			end_usec = get_currenttime();
			delta_usec = end_usec - start_usec;
			/* ping:rpbuf kernel framework layer transmit time */
			printf("ping: %fms\n", delta_usec / 1000);
			if (!recv_forever)
				recv_time = ret;

			memcpy(recv_buf, buf_addr, len);
			end_usec = get_currenttime();
			delta_usec = end_usec - start_usec;
			printf("bandwidth: %fMbps\n", len * 8 / delta_usec);
			checkdata(recv_buf, len);
		}
		free(recv_buf);
	}

	ret = 0;
free_buffer:
	rpbuf_free_buffer(buffer);
out:
	return ret;
}
