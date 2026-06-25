#ifndef __AW_MTD_H__
#define __AW_MTD_H__
#include "doorlock_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aw_mtd_s {
    int mtd_fd;
    int mtd_size;
    int mtd_erasesize;
    int mtd_writesize;
    char mtd_name[20];
} aw_mtd_t;

int aw_mtd_open(aw_mtd_t* aw_mtd, char* mtd_name);
void aw_mtd_close(aw_mtd_t* aw_mtd);
int aw_mtd_erase(aw_mtd_t* aw_mtd, int offset, int data_len);
int aw_mtd_read(aw_mtd_t* aw_mtd, int offset, uint8_t* buffer, int data_len);
int aw_mtd_read_uid(aw_mtd_t* aw_mtd, uint8_t* uid);
int aw_mtd_write(aw_mtd_t* aw_mtd, int offset, uint8_t* buffer, int data_len);
int aw_mtd_write_block(aw_mtd_t* aw_mtd, int offset, uint8_t* buffer, int data_len);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
