#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
	off_t offset = 0;
	size_t data_size, buf_size, size, data_count;
	char *data_buf = NULL;
	int i, count, j, has_file = 0;
	char data;
	char filenameA[100], filenameB[100];
	char *filename = NULL, *filebackup = NULL;
	int fd = -1;

	if (argc != 6) {
		fprintf(stderr, "Usage: %s filename filesize bufsize datacount value\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	size = atoi(argv[2]);
	buf_size = atoi(argv[3]);
	data_count =  atoi(argv[4]);
	data =  argv[5][0];

	sprintf(filenameA, "%sA", argv[1]);
	sprintf(filenameB, "%sB", argv[1]);
	if (access(filenameA, F_OK) != -1) {
		filename = filenameA;
		filebackup = filenameB;
		has_file = 1;
	} else if (access(filenameB, F_OK) != -1) {
		filename = filenameB;
		has_file = 1;
		filebackup = filenameA;
	} else {
		filename = filenameA;
		filebackup = filenameA;
		has_file = 0;
	}

	data_buf = (char *)malloc(buf_size);
	if (data_buf == NULL) {
		printf("malloc\n");
		goto fail_malloc;
	}

	if (has_file) {
		fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd == -1) {
			printf("open %s\n", filename);
			goto fail_open;
		}

		count = ((data_count + buf_size - 1) / buf_size);
		offset = 0;
		for (i = 0; i < count; i++) {
			data_size = (i == (count - 1)) ? (data_count - i * buf_size) : buf_size;

			if (lseek(fd, offset, SEEK_SET) == -1) {
				printf("faild to lseek to read\n");
				goto fail_lseek;
			}

			if (read(fd, data_buf, data_size) == -1) {
				printf("read\n");
				goto fail_read;
			}

			for (j = 0; j < data_size; j++) {
				if (data != data_buf[j]) {
					printf("compare (%d != %d)\n", data, data_buf[j]);
					goto fail_compare;
				}
			}
			offset += data_size;
		}
		close(fd);

		if (truncate(filename, 0) == -1) {
			printf("fail truncate %s\n", filename);
			goto fail_rename;
		}

		if (rename(filename, filebackup) != 0) {
			printf("fail rename old:%s--new:%s\n", filename, filebackup);
			goto fail_rename;
		}
		filename = filebackup;
	}

	fd = open(filename, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		printf("fail open backupfile: %s\n", filename);
		goto fail_open;
	}

	if (!has_file) {
		if (fallocate(fd, 0x1, 0, size) == -1) {
			printf("fallocate\n");
			goto fail_fallocate;
		}
	}

	count = ((data_count + buf_size - 1) / buf_size);
	offset = 0;
	for (i = 0; i < count; i++) {
		memset(data_buf, data, buf_size);
		data_size = (i == (count - 1)) ? (data_count - i * buf_size) : buf_size;

		if (lseek(fd, offset, SEEK_SET) == -1) {
			printf("faild to lseek to write\n");
			goto fail_lseek;
		}

		if (write(fd, data_buf, data_size) == -1) {
			printf("faild to write\n");
			goto fail_write;
		}

		usleep(50000);

		offset += data_size;
	}

	close(fd);
	free(data_buf);
	return 0;

fail_write:
fail_fallocate:
fail_read:
fail_compare:
fail_lseek:
	close(fd);
fail_rename:
fail_open:
	free(data_buf);
fail_malloc:
	exit(EXIT_FAILURE);
}
