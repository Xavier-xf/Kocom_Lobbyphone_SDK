#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <aw_list.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define DATA_BUFF_NUM		(16)
#define DATA_BUFF_MAX_LEN	(1024*1024)
#define here printf("%d %s \n", __LINE__, __func__)
typedef struct data_buff_tag {
	size_t data_size;
	unsigned char *data_buff;
	struct list_head list;
} data_buff_t;

typedef void* (*thread_func_t)(void *arg);

static LIST_HEAD(free_data_list);
static LIST_HEAD(write_data_list);
static pthread_t fast_copy_read_thread;
static pthread_t fast_copy_write_thread;
static int write_fd = 0;
static int read_fd = 0;
static sem_t read_sleep_thread_lock;
static sem_t read_start_thread_lock;
static sem_t write_start_thread_lock;
static sem_t write_sleep_thread_lock;
static sem_t free_data_list_lock;
static sem_t write_data_list_lock;
static sem_t copy_finish_lock;
static int fast_copy_exit = 0;

void *read_thread(void *args);
void *write_thread(void *args);
void data_buff_init(void) {
	int index = 0;
	data_buff_t *data_buff = NULL;

	for (index = 0; index < DATA_BUFF_NUM; index++) {
		data_buff = (data_buff_t *)malloc(sizeof(data_buff_t));
		memset(data_buff, 0x00, sizeof(data_buff_t));
		data_buff->data_buff = (unsigned char *)malloc(DATA_BUFF_MAX_LEN);
		INIT_LIST_HEAD(&data_buff->list);
		list_add_tail(&data_buff->list, &free_data_list);
	}
}

void data_buff_deinit(void) {
	int index = 0;
	data_buff_t *data_buff = NULL, *data_buff_tmp = NULL;

	list_for_each_entry_safe(data_buff, data_buff_tmp, &free_data_list, list) {
		list_del(&data_buff->list);
		free(data_buff->data_buff);
		free(data_buff);
	}
}

void *read_thread(void *args)
{
	int fd;
	size_t read_size = 0;
	data_buff_t *data_buff = NULL, *data_buff_tmp = NULL;

wait_fd:
	sem_wait(&read_start_thread_lock);
	if (fast_copy_exit) {
		return ;
	}
    while (1)
    {
    	sem_wait(&free_data_list_lock);
		if (list_empty(&free_data_list)) {
			//thread sleep
			sem_post(&free_data_list_lock);
			sem_wait(&read_sleep_thread_lock);
		} else {
			list_for_each_entry_safe(data_buff, data_buff_tmp, &free_data_list, list) {
				list_del(&data_buff->list);
				sem_post(&free_data_list_lock);
				read_size = read(read_fd, data_buff->data_buff, DATA_BUFF_MAX_LEN);
				data_buff->data_size = read_size;
//				printf("%d %s data_size:%d\n", __LINE__, __func__, data_buff->data_size);
				if (list_empty(&write_data_list)) {
					//wakeup write thread
					sem_wait(&write_data_list_lock);
					list_add_tail(&data_buff->list, &write_data_list);
					sem_post(&write_data_list_lock);
					sem_post(&write_sleep_thread_lock);
				} else {
					sem_wait(&write_data_list_lock);
					list_add_tail(&data_buff->list, &write_data_list);
					sem_post(&write_data_list_lock);
				}
				if (data_buff->data_size < DATA_BUFF_MAX_LEN) {
					close(read_fd);
					goto wait_fd;
				}
				sem_wait(&free_data_list_lock);
			}
			sem_post(&free_data_list_lock);
		}
	}
}

void *write_thread(void *args)
{
	size_t write_size = 0;
	data_buff_t *data_buff = NULL, *data_buff_tmp = NULL;

wait_fd:
	sem_wait(&write_start_thread_lock);
	if (fast_copy_exit) {
		return ;
	}
	while (1)
    {
		sem_wait(&write_data_list_lock);
		if (list_empty(&write_data_list)) {
			//thread sleep
			sem_post(&write_data_list_lock);
			sem_wait(&write_sleep_thread_lock);
		} else {
			list_for_each_entry_safe(data_buff, data_buff_tmp, &write_data_list, list) {
				list_del(&data_buff->list);
				sem_post(&write_data_list_lock);
				write_size = write(write_fd, data_buff->data_buff, data_buff->data_size);
//				printf("%d %s write_size:%d\n", __LINE__, __func__, write_size);
				if (list_empty(&free_data_list)) {
					//wakeup read thread
					sem_wait(&free_data_list_lock);
					list_add_tail(&data_buff->list, &free_data_list);
					sem_post(&free_data_list_lock);
					sem_post(&read_sleep_thread_lock);
				} else {
					sem_wait(&free_data_list_lock);
					list_add_tail(&data_buff->list, &free_data_list);
					sem_post(&free_data_list_lock);
				}
				if (data_buff->data_size != DATA_BUFF_MAX_LEN) {
					//write finish
					close(write_fd);
					fsync(write_fd);
					sem_post(&copy_finish_lock);
					goto wait_fd;
				}
				sem_wait(&write_data_list_lock);
			}
			sem_post(&write_data_list_lock);
		}
	}
}
pthread_t fast_copy_create_thread(unsigned char *name, thread_func_t start)
{
	pthread_t thread = 0;
	pthread_attr_t attr;
	struct sched_param param;

	memset(&param, 0, sizeof(struct sched_param));
	memset(&attr, 0, sizeof(pthread_attr_t));
	param.sched_priority = 0;
	pthread_attr_init(&attr);
	pthread_attr_setschedparam(&attr, &param);
	pthread_attr_setschedparam(&attr, &param);
//	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	pthread_create(&thread, &attr, start, NULL);
	pthread_setname_np(thread, (char *)name);
	return thread;
}

void fast_copy_init(void) {
	data_buff_init();
	sem_init(&read_sleep_thread_lock, 0, 0);
	sem_init(&read_start_thread_lock, 0, 0);
	sem_init(&write_start_thread_lock, 0, 0);
	sem_init(&write_sleep_thread_lock, 0, 0);
	sem_init(&free_data_list_lock, 0, 1);
	sem_init(&write_data_list_lock, 0, 1);
	sem_init(&copy_finish_lock, 0, 0);
	fast_copy_read_thread = fast_copy_create_thread("read_thread", read_thread);
	fast_copy_write_thread = fast_copy_create_thread("write_thread", write_thread);
}

void fast_copy_deinit(void) {
	fast_copy_exit = 1;
	sem_post(&read_start_thread_lock);
	pthread_join(fast_copy_read_thread, NULL);
	sem_post(&write_start_thread_lock);
	pthread_join(fast_copy_write_thread, NULL);
	sem_destroy(&read_sleep_thread_lock);
	sem_destroy(&read_start_thread_lock);
	sem_destroy(&write_start_thread_lock);
	sem_destroy(&write_sleep_thread_lock);
	sem_destroy(&free_data_list_lock);
	sem_destroy(&write_data_list_lock);
	sem_destroy(&copy_finish_lock);
	data_buff_deinit();
}

int fast_copy_param_set(unsigned char *src, unsigned char *dest)
{
	printf("src		:%s\t\n", src);
	printf("dest	:%s\t\n", dest);

    write_fd = open(dest, O_CREAT | O_TRUNC, 0666);
	read_fd = open(src, O_RDONLY, 0666);
	if (write_fd < 0) {
		printf("%s open error!\n", dest);
		return -1;
	}
	if (read_fd < 0) {
		printf("%s open error!\n", src);
		close(write_fd);
		return -1;
	}
	sem_post(&read_start_thread_lock);
	sem_post(&write_start_thread_lock);
	printf("copy start	\t\n");
	sem_wait(&copy_finish_lock);
	printf("copy finish	\t\n");
}

int main(int argc, const char **argv)
{
	printf("argc		:%d\t\n", argc);
	if (argc != 3) {
		printf("param error!\n");
		return -1;
	}
	fast_copy_init();
	fast_copy_param_set(argv[1], argv[2]);
	fast_copy_deinit();
	return 0;
}
