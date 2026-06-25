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

static int cmd_g2d_test_rot(int argc, const char **argv)
{
	int ret;
	void *buf1 = NULL,*buf2 = NULL;
	char *temp;
	int src_width=64, src_height=32;
	int size = src_width * src_height * 4;

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
	buf1 = hal_malloc(size);
	buf2 = hal_malloc(size);
	if (buf1 == NULL || buf2 == NULL) {
		printf("g2d hal_malloc failed\n");
		return -1;
	}
	printf("g2d hal_malloc done\n");

	memset(&blit_para, 0, sizeof(g2d_blt_h));
	memset(buf1, 2, size);
	memset(buf2, 3, size);

	blit_para.src_image_h.laddr[0] = __va_to_pa((uintptr_t)buf1);//phy_addr1;
	blit_para.dst_image_h.laddr[0] = __va_to_pa((uintptr_t)buf2);//phy_addr2;
	printf("src_paddr: 0x%x\n", blit_para.src_image_h.laddr[0]);
	printf("dst_paddr: 0x%x\n", blit_para.dst_image_h.laddr[0]);

	blit_para.flag_h = G2D_ROT_0;
	/* configure src image */
	blit_para.src_image_h.bbuff            = 1;
	blit_para.src_image_h.format       = G2D_FORMAT_ARGB8888;
	blit_para.src_image_h.laddr[0] = (int)(blit_para.src_image_h.laddr[0]);
	blit_para.src_image_h.use_phy_addr = 1;
	blit_para.src_image_h.width        = src_width;
	blit_para.src_image_h.height       = src_height;
	blit_para.src_image_h.align[0]     = 0;
	blit_para.src_image_h.align[1]     = 0;
	blit_para.src_image_h.align[2]     = 0;
	blit_para.src_image_h.clip_rect.x  = 0;
	blit_para.src_image_h.clip_rect.y  = 0;
	blit_para.src_image_h.clip_rect.w  = src_width;
	blit_para.src_image_h.clip_rect.h  = src_height;
	blit_para.src_image_h.coor.x       = 0;
	blit_para.src_image_h.coor.y       = 0;
	blit_para.src_image_h.gamut        = G2D_BT601;

	/* configure dst image */
	blit_para.dst_image_h.bbuff            = 1;
	blit_para.dst_image_h.format       = G2D_FORMAT_ARGB8888;
	blit_para.dst_image_h.laddr[0] = (int)(blit_para.dst_image_h.laddr[0]);
	blit_para.dst_image_h.use_phy_addr = 1;
	blit_para.dst_image_h.width        = src_width;
	blit_para.dst_image_h.height       = src_height;
	blit_para.dst_image_h.align[0]     = 0;
	blit_para.dst_image_h.align[1]     = 0;
	blit_para.dst_image_h.align[2]     = 0;
	blit_para.dst_image_h.clip_rect.x  = 0;
	blit_para.dst_image_h.clip_rect.y  = 0;
	blit_para.dst_image_h.clip_rect.w  = src_width;
	blit_para.dst_image_h.clip_rect.h  = src_height;
	blit_para.src_image_h.coor.x       = 0;
	blit_para.src_image_h.coor.y       = 0;
	blit_para.dst_image_h.gamut        = G2D_BT601;

	printf("------dump G2D_CMD_BITBLT_H input--------\n");
	temp = (char *)buf1;
	i = 64;
	while(i--) {
		printf("%x ", *temp);
		temp++;
		if(i%32==0)
			printf("\n");
	}

	hal_dcache_clean((unsigned long)buf1, size);
	hal_dcache_clean((unsigned long)buf2, size);

	printf("start control\n");

	ret = sunxi_g2d_control(G2D_CMD_BITBLT_H, &blit_para);
	if (ret) {
		printf("g2d G2D_CMD_BITBLT_H fail\n");
		ret = -1;
		goto out;
	}
	else {
		printf("G2D_CMD_BITBLT_H ok\n");
	}

	hal_dcache_clean((unsigned long)buf1, size);
	hal_dcache_clean((unsigned long)buf2, size);
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

FINSH_FUNCTION_EXPORT_ALIAS(cmd_g2d_test_rot, __cmd_g2d_test_rot, g2d_test_rot);
