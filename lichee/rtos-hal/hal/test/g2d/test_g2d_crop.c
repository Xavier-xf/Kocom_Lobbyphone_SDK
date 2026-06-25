#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <hal_mem.h>
#include <hal_cmd.h>

#include "../source/g2d_rcq/g2d_driver.h"
extern int sunxi_g2d_control(int cmd, void *arg);
extern int sunxi_g2d_close(void);
extern int sunxi_g2d_open(void);
extern int g2d_probe(void);
void hal_dcache_clean(unsigned long vaddr_start, unsigned long size);

#define FRAME_TO_BE_PROCESS 1
struct test_info_t {
	int task_id;
	struct mixer_para info[FRAME_TO_BE_PROCESS];
};
struct test_info_t test_info;

static void g2d_fix_para_rcq(int i, struct test_info_t test_info)
{
}

static void g2d_test_initialize_task_para(int src_addr, int dst_addr,
				int src_image_width, int src_image_height,
				int src_crop_x, int src_crop_y,
				int src_crop_width, int src_crop_height,
				int dst_image_width, int dst_image_height,
				int dst_crop_x, int dst_crop_y,
				int dst_crop_width, int dst_crop_height)
{
    int i = 0;

    test_info.info[0].flag_h = G2D_BLT_NONE_H;
    test_info.info[0].op_flag = OP_BITBLT;
    test_info.info[0].src_image_h.bbuff = 1;
    test_info.info[0].src_image_h.use_phy_addr = 1;
    test_info.info[0].src_image_h.laddr[0] = (int) src_addr;
    test_info.info[0].src_image_h.format = G2D_FORMAT_Y8;
    test_info.info[0].src_image_h.width = src_image_width;
    test_info.info[0].src_image_h.height = src_image_height;
    test_info.info[0].src_image_h.clip_rect.x = src_crop_x;
    test_info.info[0].src_image_h.clip_rect.y = src_crop_y;
    test_info.info[0].src_image_h.clip_rect.w = src_crop_width;
    test_info.info[0].src_image_h.clip_rect.h = src_crop_height;
    test_info.info[0].src_image_h.color = 0x008080;
    test_info.info[0].src_image_h.mode = G2D_PIXEL_ALPHA;
    test_info.info[0].src_image_h.alpha = 0xff;
    test_info.info[0].src_image_h.align[0] = 0;
    test_info.info[0].src_image_h.align[1] = 0;
    test_info.info[0].src_image_h.align[2] = 0;

    test_info.info[0].dst_image_h.bbuff = 1;
    test_info.info[0].dst_image_h.use_phy_addr = 1;
    test_info.info[0].dst_image_h.laddr[0] = (int) dst_addr;
    test_info.info[0].dst_image_h.format = G2D_FORMAT_Y8;
    test_info.info[0].dst_image_h.width = dst_image_width;
    test_info.info[0].dst_image_h.height = dst_image_height;
    test_info.info[0].dst_image_h.clip_rect.x = 0;
    test_info.info[0].dst_image_h.clip_rect.y = 0;
    test_info.info[0].dst_image_h.clip_rect.w = dst_crop_width;
    test_info.info[0].dst_image_h.clip_rect.h = dst_crop_height;
    test_info.info[0].dst_image_h.color = 0x008080;
    test_info.info[0].dst_image_h.mode = G2D_PIXEL_ALPHA;
    test_info.info[0].dst_image_h.alpha = 0xff;
    test_info.info[0].dst_image_h.align[0] = 0;
    test_info.info[0].dst_image_h.align[1] = 0;
    test_info.info[0].dst_image_h.align[2] = 0;

    for (i = 0; i < (FRAME_TO_BE_PROCESS); i++) {
        memcpy(&test_info.info[i], &test_info.info[0],
               sizeof(struct mixer_para));
	g2d_fix_para_rcq(i, test_info);
    }
    printf("create_g2d_task_para ok\n");
}

static int g2d_mixer_task(void)
{
	unsigned long arg[6];
	arg[0] = (unsigned long)test_info.info;
	arg[1] = (unsigned long)FRAME_TO_BE_PROCESS;
	if (sunxi_g2d_control(G2D_CMD_MIXER_TASK, (arg)) < 0) {
		printf("G2D_CMD_MIXER_TASK failure!\n");
		return -1;
	} else
		printf("G2D_CMD_MIXER_TASK success: task_id=%d\n" , test_info.task_id);
	return 0;
}

static int create_g2d_task(void)
{
	unsigned long arg[6];
	arg[0] = (unsigned long)test_info.info;
	arg[1] = (unsigned long)FRAME_TO_BE_PROCESS;
	if ((test_info.task_id = sunxi_g2d_control(G2D_CMD_CREATE_TASK, (arg))) < 0) {
		printf("G2D_CMD_CREATE_TASK failure!\n");
		return -1;
	} else
		printf("G2D_CMD_CREATE_TASK success: task_id=%d\n" , test_info.task_id);
	return 0;

}

static int apply_g2d_task(void)
{
	unsigned long arg[6];
	arg[0] = (unsigned long)test_info.task_id;
	if(sunxi_g2d_control(G2D_CMD_TASK_APPLY, (arg)) < 0) {
		printf("[%d][%s][%s]G2D_CMD_TASK_APPLY failure!\n", __LINE__,
		      __FILE__, __FUNCTION__);
		return -1;
	} else
		printf("G2D_CMD_TASK_APPLY success: task_id=%d\n" , test_info.task_id);
	return 0;
}

static int destory_g2d_task(void)
{
	unsigned long arg[6];
	arg[0] = (unsigned long)test_info.task_id;
	if(sunxi_g2d_control(G2D_CMD_TASK_DESTROY, (arg)) < 0) {
		printf("[%d][%s][%s]G2D_CMD_TASK_DESTROY failure!\n", __LINE__,
		       __FILE__, __FUNCTION__);
		return -1;
	} else
		printf("G2D_CMD_TASK_DESTROY success: task_id=%d\n" , test_info.task_id);
	return 0;
}

static int cmd_g2d_test_crop(int argc, const char **argv)
{
	int ret;
	void *buf1 = NULL,*buf2 = NULL;
	int src_addr, dst_addr;
	char *temp;
	int src_image_width=64, src_image_height=32;
	int src_size = src_image_width * src_image_height;
	int src_crop_x=0, src_crop_y=0, src_crop_width=src_image_width, src_crop_height=src_image_height;
	int dst_image_width=64, dst_image_height=32;
	int dst_crop_x=0, dst_crop_y=0, dst_crop_width=dst_image_width, dst_crop_height=dst_image_height;
	int dst_size = dst_crop_width * dst_crop_height;

	g2d_blt_h blit_para;
	unsigned long i;

	printf("hello g2d_test\n");
	ret = g2d_probe();
	printf("g2d probe done\n");
	ret = sunxi_g2d_open();
	if (ret) {
		printf("g2d open fail\n");
		return -1;
	}
	printf("g2d open done\n");
	//note:the test follow need an input image file 1.bin
	buf1 = hal_malloc(src_size);
	buf2 = hal_malloc(dst_size);
	if (buf1 == NULL || buf2 == NULL) {
		printf("g2d hal_malloc failed\n");
		return -1;
	}
	printf("g2d hal_malloc done\n");

	memset(&blit_para, 0, sizeof(g2d_blt_h));
	memset(buf1, 2, src_size);
	memset(buf2, 3, dst_size);

	src_addr = __va_to_pa((uintptr_t)buf1);//phy_addr1;
	dst_addr = __va_to_pa((uintptr_t)buf2);//phy_addr2;
	printf("src_paddr: 0x%x\n", src_addr);
	printf("dst_paddr: 0x%x\n", dst_addr);

	g2d_test_initialize_task_para(src_addr, dst_addr, src_image_width, src_image_height,
			     src_crop_x, src_crop_y, src_crop_width, src_crop_height,
			     dst_image_width, dst_image_height,
			     dst_crop_x, dst_crop_y, dst_crop_width, dst_crop_height);

	printf("start control\n");
	create_g2d_task();
	apply_g2d_task();
	destory_g2d_task();

	/* create, apply, destory task in one command */
	/* if (g2d_mixer_task() < 0) */
	/* 	printf("task failed\n"); */

	printf("------dump G2D_CMD_BITBLT_H input--------\n");
	temp = (char *)buf1;
	i = 64;
	while(i--) {
		printf("%x ", *temp);
		temp++;
		if(i%32==0)
			printf("\n");
	}

	hal_dcache_clean((unsigned long)buf1, src_size);
	hal_dcache_clean((unsigned long)buf2, dst_size);
	printf("------dump G2D_CMD_BITBLT_H output--------\n");

	temp = (char *)buf2;
	i = 64;
	while(i--) {//only dump the first 1280 byte
		printf("%x ",*temp );
		temp++;
		if(i%32==0)
			printf("\n");
	}

	printf("finished\n");
out:
	free(buf1);
	free(buf2);
	sunxi_g2d_close();
	return ret;

}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_g2d_test_crop, __cmd_g2d_test_crop, g2d_test_crop);
