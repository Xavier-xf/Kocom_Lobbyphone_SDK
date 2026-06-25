#include "UserKernelAdapter.h"
#include "cdc_log.h"
#ifdef CONFIG_VIDEO_RT_MEDIA
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/random.h>

int kernel_write_file(unsigned char *data0, int size0, unsigned char *data1, int size1, char *file_name)
{
    mm_segment_t old_fs;
    int ret = 0;
    FILE_STRUCT *fp = NULL;

    old_fs = get_fs();
    set_fs(KERNEL_DS);

    fp = filp_open(file_name, O_CREAT|O_RDWR, 0600);
    if (IS_ERR(fp)) {
        loge("open file failed");
        fp = NULL;
        set_fs(old_fs);
        return -1;
    }

    if (data0 && size0 > 0) {
        ret = vfs_write(fp, (char __user *)data0, size0, &fp->f_pos);
    }
    if (data1 && size1 > 0) {
        ret = vfs_write(fp, (char __user *)data1, size1, &fp->f_pos);
    }
	filp_close(fp, NULL);
    fp = NULL;
    set_fs(old_fs);
    return 0;
}

int kernel_mutex_init(struct mutex *p_mutex)
{
    mutex_init(p_mutex);
    return 0;
}

int get_random_number(void)
{
    unsigned int random;
    get_random_bytes(&random, sizeof(random));
    return (int)random;
}

#else
#include <stdio.h>
#include <time.h>

int user_write_file(unsigned char *data0, int size0, unsigned char *data1, int size1, char *file_name)
{
    FILE_STRUCT *fp = NULL;

    fp = fopen(file_name, "wb");

    if(fp == NULL) {
        logw("open thumb data file failed: %s",file_name);
        return -1;
    }
    if (data0 && size0 > 0)
        fwrite(data0, size0, 1, fp);
    if (data1 && size1 > 0)
        fwrite(data1, size1, 1, fp);
    fclose(fp);
    fp = NULL;
    return 0;
}

int user_write_file_fd(unsigned char *data0, int size0, unsigned char *data1, int size1, FILE_STRUCT *fp)
{
    if(fp == NULL) {
        logw("open thumb data file failed: %s", fp);
        return -1;
    }
    if (data0 && size0 > 0)
        fwrite(data0, size0, 1, fp);
    if (data1 && size1 > 0)
        fwrite(data1, size1, 1, fp);
    return 0;
}

int get_random_number(void)
{
    int random;
    long long curr;
    struct timespec t;

    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    curr = ((long long)(t.tv_sec)*1000000000LL + t.tv_nsec)/1000LL;

    srand(curr);
    random = rand() % 255;
    return random;
}

#endif
