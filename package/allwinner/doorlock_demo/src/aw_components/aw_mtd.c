#include "aw_mtd.h"

#include <errno.h>
#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

int aw_mtd_open(aw_mtd_t* aw_mtd, char* mtd_name)
{
    aw_mtd->mtd_fd = open(mtd_name, O_RDWR);

    if (aw_mtd->mtd_fd <= 0) {
        DOORLOCK_ERR("open [%s] failed\n", mtd_name);
        return -1;
    } else {
        DOORLOCK_DBG("open [%s] sucsses mtd_fd = %d\n", mtd_name, aw_mtd->mtd_fd);
    }

    struct mtd_info_user mtd_info;
    ioctl(aw_mtd->mtd_fd, MEMGETINFO, &mtd_info);
    DOORLOCK_INFO("mtd_name = [%s], mtd_size = 0x%08x, erasesize = 0x%08x, writesize = 0x%08x\n",
                 mtd_name, mtd_info.size, mtd_info.erasesize, mtd_info.writesize);
    aw_mtd->mtd_size = mtd_info.size;
    aw_mtd->mtd_erasesize = mtd_info.erasesize;
    aw_mtd->mtd_writesize = mtd_info.writesize;
    memset(aw_mtd->mtd_name, 0, sizeof(aw_mtd->mtd_name));
    memcpy(aw_mtd->mtd_name, mtd_name, strlen(mtd_name));
    return 0;
}

void aw_mtd_close(aw_mtd_t* aw_mtd)
{
    DOORLOCK_INFO("mtd_name = [%s]\n", aw_mtd->mtd_name);

    if (aw_mtd->mtd_fd > 0)
        close(aw_mtd->mtd_fd);
    else
        DOORLOCK_ERR("mtd_name = [%s], not opened yet, no need close!\n", aw_mtd->mtd_name);

    aw_mtd->mtd_fd = 0;
}

int aw_mtd_erase(aw_mtd_t* aw_mtd, int offset, int data_len)
{
    if (aw_mtd->mtd_fd <= 0) {
        DOORLOCK_ERR("open mtd first!\n");
        return -1;
    }
    if (offset + data_len > aw_mtd->mtd_size) {
        DOORLOCK_ERR("offset %d len %d bytes, exceed limit mtd_size %d\n", offset, data_len,
                    aw_mtd->mtd_size);
        return -1;
    }

    struct erase_info_user erase_info;
    erase_info.start = offset;
    erase_info.length = data_len;
    int ret = ioctl(aw_mtd->mtd_fd, MEMERASE, &erase_info);
    if (ret != 0) {
        DOORLOCK_ERR("erase mtd failed %d, offset %d, data_len %d!\n", ret, offset, data_len);
    }
    return ret;
}

int aw_mtd_read_uid(aw_mtd_t* aw_mtd, uint8_t* uid)
{
    struct nor_uid get_uid_info;

    int ret = ioctl(aw_mtd->mtd_fd, MTD_NOR_RDUID, &get_uid_info);
    memcpy(uid, get_uid_info.uid_buf, 16);

    {
#if 1
        DOORLOCK_INFO("nor flash uid: ");
        for (int i = 0; i < 16; i++) {
            printf("0x%02x ", get_uid_info.uid_buf[i]);
        }
        printf("\n");
#endif
    }

    return ret;
}

int aw_mtd_read(aw_mtd_t* aw_mtd, int offset, uint8_t* buffer, int data_len)
{
    if (aw_mtd->mtd_fd <= 0) {
        DOORLOCK_ERR("open mtd first!\n");
        return -1;
    }
    if (offset + data_len > aw_mtd->mtd_size) {
        DOORLOCK_ERR("offset %d len %d bytes, exceed limit mtd_size %d\n", offset, data_len,
                    aw_mtd->mtd_size);
        return -1;
    }
    lseek(aw_mtd->mtd_fd, offset, SEEK_SET);
    int ret = read(aw_mtd->mtd_fd, buffer, data_len);
    if (ret != data_len) {
        DOORLOCK_ERR("read mtd failed %d, offset %d, data_len %d!\n", ret, offset, data_len);
    }
    return 0;
}

// need erase first
int aw_mtd_write(aw_mtd_t* aw_mtd, int offset, uint8_t* buffer, int data_len)
{
    if (aw_mtd->mtd_fd <= 0) {
        DOORLOCK_ERR("open mtd first!\n");
        return -1;
    }
    if (offset + data_len > aw_mtd->mtd_size) {
        DOORLOCK_ERR("offset %d len %d bytes, exceed limit mtd_size %d\n", offset, data_len,
                    aw_mtd->mtd_size);
        return -1;
    }
    lseek(aw_mtd->mtd_fd, offset, SEEK_SET);
    int ret = write(aw_mtd->mtd_fd, buffer, data_len);
    if (ret != data_len) {
        DOORLOCK_ERR("write mtd failed %d, offset %d, data_len %d!\n", ret, offset, data_len);
    }
    return 0;
}

// need erase in this funciton
int aw_mtd_write_block(aw_mtd_t* aw_mtd, int offset, uint8_t* buffer, int data_len)
{
    if (aw_mtd->mtd_fd <= 0) {
        DOORLOCK_ERR("open mtd first!\n");
        return -1;
    }
    if (offset + data_len > aw_mtd->mtd_size) {
        DOORLOCK_ERR("offset %d len %d bytes, exceed limit mtd_size %d\n", offset, data_len,
                    aw_mtd->mtd_size);
        return -1;
    }
    uint8_t* cache_data = malloc(aw_mtd->mtd_erasesize);
    if (cache_data == NULL) {
        DOORLOCK_ERR("malloc %d failed\n", aw_mtd->mtd_erasesize);
        return -1;
    }
    int start_offset = offset / aw_mtd->mtd_erasesize * aw_mtd->mtd_erasesize;
    int end_offset = (offset + data_len + aw_mtd->mtd_erasesize - 1) / aw_mtd->mtd_erasesize *
                    aw_mtd->mtd_erasesize;
    int block_cnt = (end_offset - start_offset) / aw_mtd->mtd_erasesize;
    int already_write = 0;

    int head_len = aw_mtd->mtd_erasesize - (offset - start_offset);
    if (head_len > data_len) {
        head_len = data_len;
    }
    for (int i = 0; i < block_cnt; i++) {
        if (i == 0 && offset != start_offset) {
            aw_mtd_read(aw_mtd, start_offset + i * aw_mtd->mtd_erasesize, cache_data,
                    aw_mtd->mtd_erasesize);
            memcpy(cache_data + (offset - start_offset), buffer + already_write, head_len);
            aw_mtd_erase(aw_mtd, start_offset + i * aw_mtd->mtd_erasesize, aw_mtd->mtd_erasesize);
            aw_mtd_write(aw_mtd, start_offset + i * aw_mtd->mtd_erasesize, cache_data,
                     aw_mtd->mtd_erasesize);
            already_write += aw_mtd->mtd_erasesize - (offset - start_offset);
        } else if (i == block_cnt - 1 && (offset + data_len) != end_offset) {
            aw_mtd_read(aw_mtd, start_offset + i * aw_mtd->mtd_erasesize, cache_data,
                    aw_mtd->mtd_erasesize);
            memcpy(cache_data, buffer + already_write, (offset + data_len) % aw_mtd->mtd_erasesize);
            aw_mtd_erase(aw_mtd, start_offset + i * aw_mtd->mtd_erasesize, aw_mtd->mtd_erasesize);
            aw_mtd_write(aw_mtd, start_offset + i * aw_mtd->mtd_erasesize, cache_data,
                     aw_mtd->mtd_erasesize);
            already_write += (offset + data_len) % aw_mtd->mtd_erasesize;
        } else {
            aw_mtd_erase(aw_mtd, start_offset + i * aw_mtd->mtd_erasesize, aw_mtd->mtd_erasesize);
            aw_mtd_write(aw_mtd, start_offset + i * aw_mtd->mtd_erasesize, buffer + already_write,
                     aw_mtd->mtd_erasesize);
            already_write += aw_mtd->mtd_erasesize;
        }
    }

    if (cache_data) {
        free(cache_data);
        cache_data = NULL;
    }
    return 0;
}
