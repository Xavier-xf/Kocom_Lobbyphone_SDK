#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <wifi_log.h>
#include <wmg_common.h>
#include <linux_get_config.h>

#define XR8X9 1

static uint8_t linux_support_list[16] = {1,2,4,8,0,0,0,0,0,0,0,0,0,0,0,0};
static mode_support_list_t  linux_mode_support_list = {
	.item_num = 4,
	.item_table = &linux_support_list,
};

static wmg_status_t update_support_list(mode_support_list_t *support_list, char *support_list_config)
{
	uint8_t tmp_support_list[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int i;
	int tmp_value;
	char *pch;

	pch = strtok(support_list_config, ":");
	for(i=0;(pch != NULL) && (i < 16); i++) {
		tmp_value = atoi(pch);
		if(tmp_value < 16) {
			tmp_support_list[i] = (uint8_t)tmp_value;
			pch = strtok(NULL, ":");
		} else {
			WMG_ERROR("support list config illegal(%d)\n", tmp_value);
			return WMG_STATUS_FAIL;
		}
	}
	memcpy(support_list->item_table, tmp_support_list, 16 * sizeof(uint8_t));
	support_list->item_num = i + 1;
	return WMG_STATUS_SUCCESS;
}

mode_support_list_t* wmg_mode_support_list_register(void)
{
	char support_list_buf[64];
	if(!get_config("support_list", support_list_buf, NULL)) {
		WMG_DEBUG("get linux support list: %s, update support list now\n", support_list_buf);
		update_support_list(&linux_mode_support_list, support_list_buf);
	} else {
		WMG_DEBUG("get linux support list fail, use default support list now\n", support_list_buf);
	}
	return &linux_mode_support_list;
}

static char *del_left_trim(char *str)
{
	assert(str != NULL);
	for(;*str != '\0' && isblank(*str); ++str);
	return  str;
}

static char *del_both_trim(char * str)
{
	char *p;
	char * szOutput;
	szOutput = del_left_trim(str);
	for(p = szOutput + strlen(szOutput) - 1; p >= szOutput && isblank(*p);--p);
		*(++p) =  '\0';
	return  szOutput;
}

wmg_status_t get_config(char *config_name, char *config_buf, char *config_default)
{
	FILE * fp = NULL;
	char s[256] = {0};
	char *delim = "=";
	char *p;
	char ch;
	char config_file[256] = {0};

	/*open config file*/
	sprintf(config_file, "%s/wifimg.config", WMG_CONFIG_PATH);
	if((fp = fopen(config_file, "r")) == NULL) {
		WMG_WARNG("Fail to open config file(%s/wifimg.config)!, open default config file(%s/wifimg.def) now\n", WMG_CONFIG_PATH, WMG_CONFIG_PATH);
		memset(config_file, 0, 256);
		sprintf(config_file, "%s/wifimg.def", WMG_CONFIG_PATH);
		if((fp = fopen(config_file, "r")) == NULL) {
			WMG_WARNG("Fail to open default config file(%s/wifimg.def)!\n", WMG_CONFIG_PATH);
			goto get_config_fail;
		} else {
			WMG_DEBUG("open config file(%s/wifimg.def)!\n", WMG_CONFIG_PATH);
		}
	} else {
		WMG_DEBUG("open config file(%s/wifimg.config)!\n", WMG_CONFIG_PATH);
	}

	while (!feof(fp)) {
		if((p = fgets(s, sizeof(s), fp)) != NULL) {
			ch=del_left_trim(s)[0];

			if(ch == '#' || isblank(ch) || ch == '\n' )
				continue ;
			p=strtok(s, delim);
			if(p) {
				if(!strcmp(config_name, del_both_trim(p))) {
					while((p = strtok(NULL, delim))) {
						strcpy(config_buf, p);
						if(config_buf[strlen(config_buf) - 1] == '\n') {
							config_buf[strlen(config_buf) - 1] = '\0';
						}
						WMG_DEBUG("get config: %s=%s\n", config_name, config_buf);
						fclose(fp);
						return WMG_STATUS_SUCCESS;
					}
				} else {
					p = strtok(NULL, delim);
				}
			}
		}
	}
get_config_fail:
	if(config_default != NULL) {
		strcpy(config_buf, config_default);
		WMG_WARNG("Can't get config: %s, used default config: %s\n", config_name, config_default);
	} else {
		WMG_WARNG("Can't get config: %s\n", config_name);
	}
	fclose(fp);
	return WMG_STATUS_FAIL;
}
