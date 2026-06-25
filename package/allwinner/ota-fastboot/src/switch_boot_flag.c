#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/prctl.h>

typedef long long loff_t;

#define BUFFER_SIZE_64K	  64 * 1024
#define ISP1_PARAM_OFFSET 112  /*112 ~ 118 sector 3.5k*/
#define MEMWRITE_4K             _IOWR('M', 29, struct write4k_op_t)


// One camera accounts for 4k data
#pragma pack(1)
typedef struct tagsensor_isp_config_s {
	int sign; //id0: 0xAA66AA66, id1: 0xBB66BB66
	int crc; //checksum, crc32 check, if crc = 0, do check
	int ver; //version, 0x01
	int light_enable; //adc en, 1:use LIGHT_SENSOR_ATTR_S
	//ADC mode, 0:the brighter the u16LightValue more, 1:the brighter the u16LightValue smaller
	int adc_mode;
	//adc threshold: 1,adc_mode=0:smaller than it enter night mode, greater than it or equal enter day mode
	unsigned short light_def;
	//adc_mode=1:greater than it enter night mode, smaller than it or equal enter day mode;
	unsigned char ircut_state; //1:hold, 0:use ir_mode
	unsigned char resever[4072];
	 /* noteFastboot specific for switching filesystems and systems */
	unsigned char switch_system_flag;
} SENSOR_ISP_CONFIGS_S;
#pragma pack()

struct boot0_64k {
	unsigned int res0[14336]; /* resever 56k */
	SENSOR_ISP_CONFIGS_S isp1_param;
	unsigned int res1[1024];
};

struct write4k_op_t {
	loff_t start;
	unsigned char *buf;
	loff_t len;
};

int main(int argc, char *argv[])
{
	char *buffer;
	struct boot0_64k *boot0_flash;
	int ret;
	int fd = 0;
	struct write4k_op_t write_4k;
	fd = open("/dev/mtd0", O_RDWR);
	if (fd < 0) {
		printf("open mtd error");
		return -1;
	}

	/* printf("switch flag:0x%x\n", strtol(argv[1], NULL, 16)); */
        buffer = (char *)malloc(BUFFER_SIZE_64K);
	if (NULL == buffer) {
		printf("malloc buffer failed\n");
		return -1;
	}

	ret = read(fd, buffer, BUFFER_SIZE_64K);
	if (ret < 0) {
		printf("read mtd0 failed\n");
		free(buffer);
		return -1;
	}

        boot0_flash = (struct boot0_64k *)buffer;
	write_4k.start = ISP1_PARAM_OFFSET*512;
	write_4k.buf = malloc(sizeof(SENSOR_ISP_CONFIGS_S));
	memset(write_4k.buf, 0, sizeof(SENSOR_ISP_CONFIGS_S));

#if 0	/* for test */
	boot0_flash->isp1_param.sign = 0xBB66BB66;
	boot0_flash->isp1_param.adc_mode = 0x12345601;
	boot0_flash->isp1_param.ircut_state = 1;

	printf("isp1_param.sign:0x%x, adc_mode:0x%x, ircut_state:%d\n",
			boot0_flash->isp1_param.sign,
			boot0_flash->isp1_param.adc_mode,
	                boot0_flash->isp1_param.ircut_state);

#endif
	/* switch_system_flag
	 * bit3~7: reserve
	 * bit2  : extend(/usr) 1:extend 0:extend_back
	 * bit1  : rootfs  1:rootfs  0:rootfs_backup
	 * bit0  : kernel  1:kernel  0:kernel_back */

	boot0_flash->isp1_param.switch_system_flag &= 0xF8;
	boot0_flash->isp1_param.switch_system_flag |= (unsigned char)strtol(argv[1], NULL, 16);
	printf("switch_system_flag:0x%x\n", boot0_flash->isp1_param.switch_system_flag);
	memcpy((void *)write_4k.buf, (void *)&boot0_flash->isp1_param, sizeof(SENSOR_ISP_CONFIGS_S));

	write_4k.len = sizeof(SENSOR_ISP_CONFIGS_S);
	ioctl(fd, MEMWRITE_4K, &write_4k);

	free(write_4k.buf);
	free(buffer);
	close(fd);
	return 0;
}
