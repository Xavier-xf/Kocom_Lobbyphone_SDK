#include <ata.h>
#include <command.h>
#include <common.h>
#include <fat.h>
#include <fs.h>
#include <net.h>
#include <part.h>
#include <s_record.h>
#include <u-boot/md5.h>
#include <environment.h>
#include <sunxi_flash.h>
#include <sprite_download.h>
#include <sys_partition.h>

int sprite_cartoon_upgrade(int rate);
uint sprite_cartoon_create(int op);

#define FULL_FIRMWARE "tina_v853s-perf1_uart0_nor.img"
#define ENV_FIRMWARE_NAME "FIRMWARE_NAME"
#define ENV_FIRMWARE_VERSION "FIRMWARE_VERSION"
#define FIRMWARE_END_STR "# <- this is end of image parttion"
#define FIRMWARE_PARRTION_STR "# File partition:"
static char* skip_space(const char* str) {
	while ((str) && ((*str) == ' '))str++;
	return (char*)str;
}
static char* find_end(const char* str) {
	while ((str) && ((*str) != ' ') && ((*str) != '\0')) str++;
	return (char*)str;
}
static int find_between_spaces(const char* str, char** next, char* val, int len) {
	char* s = skip_space(str);
	if (!s) {
		return -1;
	}
	char* e = find_end(s);
	if (!e) {
		return -1;
	}
	if ((e - s) > len) {
		printf("parttion val over fill (src:%d,len:%d)\n", e - s, len);
		return -1;
	}
	memset(val, 0, len);
	strncpy(val, s, e - s);
	*next = e + 1;
	return 0;
}
static int check_firmware_exist(const char* filename){
	loff_t size;
	if (!filename)
		return 0;

	if (run_command("sunxi_card0_probe", 0)) {
		return 0;
	}
	if (fs_set_blk_dev("mmc", "0", FS_TYPE_FAT)){
		return 0;
	}
	if(fs_size(filename,&size)){
		return 0;
	}
	return size>0?1:0;
}

static loff_t fat_fs_read(const char *filename, void *buf, int offset, int len)
{
	loff_t len_read;
	if (!buf || !filename){
		printf("[%s:%d]\n",__func__,__LINE__);
		return -1;
	}

	if (fs_set_blk_dev("mmc", "0", FS_TYPE_FAT)){
		printf("[%s:%d]\n",__func__,__LINE__);
		return -1;
	}
		

	if (fs_read(filename, (ulong)buf, offset, len, &len_read)){
		printf("[%s:%d]\n",__func__,__LINE__);
		return -1;
	}
	return len_read;
}


static unsigned long read_firmware_file(char* file, char* buffer, long offset, int size) {	
	return  fat_fs_read(file, buffer, offset,	size);
}

static unsigned long  read_firmware_line(char* file, char* buffer, long offset, int size) {
	unsigned long read_len = read_firmware_file(file, buffer, offset, size);
	if (read_len > 0) {
		char* space = strstr(buffer, "\n");
		if (!space) {
			return 0;
		}
		*space = '\0';
	//	printf("read line:%s[%d]\n", buffer, space - buffer + 1);
		return  space - buffer + 1;
	}

	return 0;
}

static int firmware_version_compare(const char* buffer, char* new_version, int size) {
	char* env_version = NULL;
	char* version = strstr(buffer, "#<upgrade_bin_version=");
	if (version == NULL) {
		printf("file error:%s\n", buffer);
		return -1;
	}
	version += strlen("#<upgrade_bin_version=");
	env_version = env_get(ENV_FIRMWARE_VERSION);
	memset(new_version, 0, size);
	strncpy(new_version, version, size);
	printf("upgarde version:%s cur version:%s\n", version, env_version);
	if (env_version == NULL) {
		return 1;
	}
	return strncmp(version, env_version, strlen(env_version)) ? 1 : 0;
}

static long firmware_base_find(char* file, char* buffer, int size) {
	long offset = 0;
	long read_len = 0;
	char* s;
	while ((read_len = read_firmware_line(file, buffer, offset, size)) > 0) {
		offset += read_len;
		s = strstr(buffer, FIRMWARE_END_STR);
		if (s) {
			return offset;
		}
	}
	return 0;
}

static int parttion_string_format_parse(char* string, int size,
	char* name, int name_len,
	char* parttion_offset, int parttion_offset_len,
	char* parttion_size, int parttion_size_len,
	char* data_offset, int data_offset_len,
	char* data_size, int data_size_len,
	char* md5, int md5_len) {

	char* next = NULL;
	char* str = strstr(string, FIRMWARE_PARRTION_STR);
	if (!str) {
		return -1;
	}

	if (find_between_spaces(str + strlen(FIRMWARE_PARRTION_STR), &next, name, name_len) != 0) {
		printf("find name failed\n");
		return -1;
	}

	if (find_between_spaces(next, &next, parttion_offset, parttion_offset_len) != 0) {
		printf("find parttion offset failed\n");
		return -1;
	}

	if (find_between_spaces(next, &next, parttion_size, parttion_size_len) != 0) {
		printf("find parttion size failed\n");
		return -1;
	}

	if (find_between_spaces(next, &next, data_offset, data_offset_len) != 0) {
		printf("find data offset failed\n");
		return -1;
	}

	if (find_between_spaces(next, &next, data_size, data_size_len) != 0) {
		printf("find data len failed\n");
		return -1;
	}

	if (find_between_spaces(next, &next, md5, md5_len) != 0) {
		printf("find md5 failed(%s)\n", next);
		return -1;
	}
	//printf("find:%s %s %s %s %s %s\n", name, parttion_offset, parttion_size, data_offset, data_size, md5);
	return 0;
}

static int firmware_parttion_check(char* file, long firmware_base, char* buffer, int size, long* upgrade_total) {
	char name[64];
	char str_parttion_offset[32];
	char str_parttion_size[32];
	char str_data_offset[32];
	char str_data_size[32];
	char str_srcmd5[33];
	int read_len = 0;
	long offset = 0;
	unsigned char digest[16];
	char data_digest[33] = { 0 };
	int parttion_size = 0;
	if (parttion_string_format_parse(buffer, size,
		name, sizeof(name),
		str_parttion_offset, sizeof(str_parttion_size),
		str_parttion_size, sizeof(str_parttion_size),
		str_data_offset, sizeof(str_data_offset),
		str_data_size, sizeof(str_data_size),
		str_srcmd5, sizeof(str_srcmd5)) == -1) {
		return -1;
	}
	offset = firmware_base + simple_strtoul(str_data_offset, NULL, 16);
	size = simple_strtoul(str_data_size, NULL, 16);
	parttion_size = simple_strtoul(str_parttion_size, NULL, 16);
	if (size > (parttion_size*512)) {
		printf("%s data size(%d) > pattion size(%d)\n", name, size, parttion_size*512);
		return -1;
	}
	//printf("read %s (base:%ld) data:%#lx,size:%#x ,write addres:0x%#X\n", name, firmware_base, offset, size,buffer);
	if ((read_len = read_firmware_file(file, buffer, offset, size)) != size) {
		printf("read filesize(%d) not %d\n", read_len, size);
		return -1;
	}
	md5((unsigned char*)buffer, size, digest);

	memset(data_digest, 0, sizeof(data_digest));
	sprintf(data_digest, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
		digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]);

	if (memcmp(data_digest, str_srcmd5, sizeof(str_srcmd5) - 1)) {
		printf("parttion %s check valid failed\n", name);
		return -1;
	}
	*upgrade_total += size;
	printf("parttion %s check valid ok\n", name);
	return 0;
}


static int firmware_data_valid_check(char* file, long firmware_base, char* buffer, long offset, int size, long* upgrade_total) {
	int read_len;
	while ((read_len = read_firmware_line(file, buffer, offset, size)) > 0) {
		offset += read_len;
		if (strstr(buffer, FIRMWARE_END_STR)) {
			//在原来的基础上增加20%
			*upgrade_total += (*upgrade_total)*20/100;
			return 0;
		}
		if (strstr(buffer, FIRMWARE_PARRTION_STR)) {
			if (firmware_parttion_check(file, firmware_base, buffer, read_len, upgrade_total)) {
				printf("check parttion:%s failed\n", buffer);
				return -1;
			}
		}
	}
	return -1;
}


static int frimware_partition_write(const char*name,long partition_sectors,char* data,long size,long total_progress,long* pcur_progress){
	long cur_progress = *pcur_progress;
	long remain_sector = (size + 511)>>9;
	const long write_one_sector = (64*1024)>>9;
	long write_sector = 0;
	long total_write_sectors = 0;
	while (remain_sector > 0) {
		if (remain_sector < write_one_sector) {
			write_sector = remain_sector;
		}
		else {
			write_sector = write_one_sector;
		}
		
		if(sunxi_sprite_phywrite(partition_sectors + total_write_sectors, write_sector,&data[total_write_sectors*512]) != write_sector){
			printf("write %s partition failed\n",name);
			break;
		}
		//printf("%s write secotrs:%#lx size:%#lx\n", name, partition_sectors + total_write_sectors,write_sector);

		total_write_sectors += write_sector;
		remain_sector -= write_sector;
		
		cur_progress += write_sector*512;
		printf("\r upgrade proress%3ld%%", cur_progress * 100 / total_progress);
		
		sprite_cartoon_upgrade(15+ cur_progress * 100 / total_progress);
	}
	* pcur_progress = cur_progress;
	return 0;
}

static int firmware_parttion_upgrade(char* file, long firmware_base, char* data, int size, long upgrade_total, long* current_upgrade) {
	char name[64];
	char str_parttion_offset[32];
	char str_parttion_size[32];
	char str_data_offset[32];
	char str_data_size[32];
	char str_srcmd5[33];

	char* gpt_buf = NULL;
	const long gpt_size = 8*1024; //固定

	long data_offset = 0;
	long data_size = 0;

	long partition_sectors;
	int ret = 0;
	char* buffer = data;

	//int parttion_size = 0;
	if (parttion_string_format_parse(buffer, size,
		name, sizeof(name),
		str_parttion_offset, sizeof(str_parttion_size),
		str_parttion_size, sizeof(str_parttion_size),
		str_data_offset, sizeof(str_data_offset),
		str_data_size, sizeof(str_data_size),
		str_srcmd5, sizeof(str_srcmd5)) == -1) {
		return -1;
	}
	data_offset = firmware_base + simple_strtoul(str_data_offset, NULL, 16);
	partition_sectors = simple_strtoul(str_parttion_offset, NULL, 16);
	data_size = simple_strtoul(str_data_size, NULL, 16);

	if ((read_firmware_file(file, buffer, data_offset, data_size)) != data_size) {
		printf("read filesize not %ld\n", data_size);
		return -1;
	}


	if(!strcmp(name,"mbr")){
		gpt_buf = memalign(CONFIG_SYS_CACHELINE_SIZE, ALIGN(gpt_size, CONFIG_SYS_CACHELINE_SIZE));
		if(gpt_buf == NULL) {
			debug("malloc for GPT  fail\n");
			return -1;
		}
		memset(gpt_buf, 0x0, gpt_size);

		data_size = sunxi_mbr_convert_to_gpt(buffer, gpt_buf, STORAGE_NOR);
		if(data_size == 0) {
			printf("mbr conver to gpt failed\n");
			free(gpt_buf);
			return -1;
		}
		buffer = gpt_buf;
	}

	ret = frimware_partition_write(name ,partition_sectors,buffer,data_size,upgrade_total,current_upgrade);
	
	if(!strcmp(name,"mbr")){
		sunxi_probe_partition_map();
		free(buffer);
	}
	return ret;
}
static int firmware_upgrade(char* file, long firmware_base, char* buffer, long offset, int size, long upgrade_total) {
	int read_len;
	long current_upgrade = 0;
	while ((read_len = read_firmware_line(file, buffer, offset, size)) > 0) {
		offset += read_len;
		if (strstr(buffer, FIRMWARE_END_STR)) {
			return 0;
		}
		if (strstr(buffer, FIRMWARE_PARRTION_STR)) {
			if (firmware_parttion_upgrade(file, firmware_base, buffer, size, upgrade_total, &current_upgrade)) {
				printf("upgrade parttion:%s failed\n", buffer);
				return -1;
			}
		}
	}
	return  0;
}

int do_sd_upgrade(cmd_tbl_t* cmdtp, int flag, int argc, char* const argv[]) {
	char* buffer = NULL;
	char* upgrade_filename = NULL;
	char new_version[20] = { 0 };
	long read_len = 0, firmware_base = 0, offset = 0, upgrade_total = 0;
	long progress = 0;
	upgrade_filename = env_get(ENV_FIRMWARE_NAME);
	if (upgrade_filename == NULL) {
		upgrade_filename = "KCMLobbyPhone.IMG";
		env_set(ENV_FIRMWARE_NAME, upgrade_filename);
		env_save();
	}

	buffer =(char *)0x45000000;// (char*)env_get_hex("loadaddr", 0x80008000);
	if(!check_firmware_exist(upgrade_filename)){
		if(check_firmware_exist(FULL_FIRMWARE)){
			return run_command("auto_update_check 1",0);
		}
		return 0;
	}

	//version
	printf("read firmware head data\n");
	read_len = read_firmware_line(upgrade_filename, buffer, offset, 1024);
	if (read_len <= 0) {
		return 0;
	}
	offset += read_len;
	printf("check firmware version\n");
	if (firmware_version_compare(buffer, new_version, sizeof(new_version)) == 0) {
		printf("firmware version same:%s\n", new_version);
		return 0;
	}

	
	printf("ready check parttion md5\n");
	sprite_cartoon_create(2);

	sprite_cartoon_upgrade(5);
	firmware_base = firmware_base_find(upgrade_filename, buffer, 1024);
	if (firmware_base == 0) {
		printf("not find firmware end str\n");
		return 0;
	}
	
	printf("start check parttion md5\n");
	if (firmware_data_valid_check(upgrade_filename, firmware_base, buffer, offset, 1024, &upgrade_total)) {
		printf("firame data check vaild failed\n");
		return 0;
	}
	sprite_cartoon_upgrade(10);
	printf("start upgrade firmware\n");
	if ((upgrade_total > 0) && firmware_upgrade(upgrade_filename, firmware_base, buffer, offset, 1024*64,upgrade_total)) {
		printf("firmware upgrade failed\n");
		return 0;
	}
	env_load();
	env_set(ENV_FIRMWARE_VERSION, new_version);
	env_save();
	progress = 100;
	sprite_cartoon_upgrade(progress);
	printf("finish ...\n");
	while (1) {
		sprite_cartoon_upgrade(100);
		mdelay(500);
		sprite_cartoon_upgrade(0);
		mdelay(500);
	}
	return 0;
}
U_BOOT_CMD(
	sd_upgrade, 2, 0, do_sd_upgrade,
	"upgrade file from a dos filesystem",
	"<interface> [<dev[:part]>]  <addr> <filename> [bytes [pos]]\n"
	"    - Load binary file 'filename' from 'dev' on 'interface'\n"
	"      to address 'addr' from dos filesystem.\n"
	"      'pos' gives the file position to start loading from.\n"
	"      If 'pos' is omitted, 0 is used. 'pos' requires 'bytes'.\n"
	"      'bytes' gives the size to load. If 'bytes' is 0 or omitted,\n"
	"      the load stops on end of file.\n"
	"      If either 'pos' or 'bytes' are not aligned to\n"
	"      ARCH_DMA_MINALIGN then a misaligned buffer warning will\n"
	"      be printed and performance will suffer for the load.");