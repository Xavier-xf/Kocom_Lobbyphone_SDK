#include "doorlock_common.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "linux/videodev2.h"

int g_doorlock_log_level = DOORLOCK_DEFAULT_LOG_LEVEL;
char g_doorlock_log_path[256] = {0};
char g_doorlock_log_buf[1024 * 10] = {0};

int doorlock_set_dynamic_log_level(int level)
{
    int ret = 0;
    char tmp_buf[256];
    g_doorlock_log_level = level;

    memset(tmp_buf, 0, sizeof(tmp_buf));
    sprintf(tmp_buf, "/etc/doorlock_loglevel");

    int fd_value = open(tmp_buf, O_WRONLY | O_CREAT);
    if (fd_value <= 0) {
        DOORLOCK_DBG("failed\n");
        ret = -1;
        goto __exit;
    }
    memset(tmp_buf, 0, sizeof(tmp_buf));
    sprintf(tmp_buf, "%d", g_doorlock_log_level);
    write(fd_value, tmp_buf, strlen(tmp_buf));
    close(fd_value);
    DOORLOCK_DBG("loglevel = %d\n", g_doorlock_log_level);

__exit:
    return ret;
}

int doorlock_get_dynamic_log_level(int *level)
{
    int ret = 0;
    char tmp_buf[256];
    memset(tmp_buf, 0, sizeof(tmp_buf));
    sprintf(tmp_buf, "/etc/doorlock_loglevel");

    int fd_value = open(tmp_buf, O_RDONLY);
    if (fd_value <= 0) {
        DOORLOCK_ERR("failed\n");
        ret = -1;
        goto __exit;
    }

    memset(tmp_buf, 0, sizeof(tmp_buf));
    read(fd_value, tmp_buf, sizeof(tmp_buf));
    g_doorlock_log_level = atoi(tmp_buf);
    close(fd_value);
    DOORLOCK_INFO("loglevel = %d\n", g_doorlock_log_level);
    *level = g_doorlock_log_level;
__exit:
    return ret;
}

int doorlock_log_file_config(char *file_path)
{
    memset(g_doorlock_log_path, 0, sizeof(g_doorlock_log_path));
    if (file_path == NULL || strlen(file_path) >= sizeof(g_doorlock_log_path)) {
        return -1;
    }

    memcpy(g_doorlock_log_path, file_path, strlen(file_path));
    DOORLOCK_INFO("DOORLOCK LogPath = %s\n", g_doorlock_log_path);
    return 0;
}

int doorlock_get_timestr(char *buf, int buf_size, char *fmt)
{
    memset(buf, 0, buf_size);
    time_t now_time = time(0);
    if (fmt == NULL) {
        fmt = "%Y.%m.%d %H:%M:%S";
    }
    strftime(buf, buf_size, fmt, (struct tm *)localtime(&now_time));
    return 0;
}

void doorlock_set_str(char *file_name, char *str_buf)
{
    FILE *stream = fopen(file_name, "wb");
    if (stream == NULL || str_buf == NULL) {
        // DOORLOCK_ERR("fopen [%s] failed !!!\n", file_name);
        return;
    }
    fwrite(str_buf, sizeof(char), strlen(str_buf), stream);
    fclose(stream);
}

int doorlock_get_int(char *file_name, int default_int)
{
    char ret_buf[256] = {0};

    FILE *stream = fopen(file_name, "rb");
    if (stream == NULL) {
        // DOORLOCK_ERR("fopen [%s] failed !!!\n", file_name);
        return default_int;
    }
    memset(ret_buf, 0, sizeof(ret_buf));
    fread(ret_buf, sizeof(char), sizeof(ret_buf) - 1, stream);
    fclose(stream);

    char *tmp_env = ret_buf;  // getenv(pName);
    int tmp_flag = default_int;
    if (tmp_env == NULL || strlen(tmp_env) == 0) {
        tmp_flag = default_int;
    } else {
        tmp_flag = atoi(tmp_env);
    }
    return tmp_flag;
}

void doorlock_get_str(char *file_name, char *str_buf, uint32_t max_buf_len)
{
    memset(str_buf, 0, max_buf_len);
    FILE *stream = fopen(file_name, "rb");

    if (stream == NULL || str_buf == NULL) {
        // DOORLOCK_ERR("fopen [%s] failed !!!\n", file_name);
        return;
    }
    fread(str_buf, sizeof(char), max_buf_len - 1, stream);
    fclose(stream);
}

int doorlock_get_cpu_occupy_info(cpu_occupy_t *occupy)
{
    FILE *fd;
    int n;
    char buff[256];
    cpu_occupy_t *cpu_occupy = occupy;

    if (cpu_occupy == NULL) {
        return -1;
    }
    fd = fopen("/proc/stat", "r");
    if (fd == NULL) {
        DOORLOCK_ERR("fopen:/proc/stat failed\n");
        return -1;
    }
    memset(cpu_occupy, 0, sizeof(cpu_occupy_t));
    memset(buff, 0, sizeof(buff));
    fgets(buff, sizeof(buff), fd);
    sscanf(buff, "%s %u %u %u %u %u %u %u", cpu_occupy->name, &cpu_occupy->user, &cpu_occupy->nice,
           &cpu_occupy->system, &cpu_occupy->idle, &cpu_occupy->iowait, &cpu_occupy->irq,
           &cpu_occupy->softirq);

    //DOORLOCK_INFO("%s %u %u %u %u %u %u %u\n", cpu_occupy->name, cpu_occupy->user, cpu_occupy->nice, cpu_occupy->system, \
		cpu_occupy->idle, cpu_occupy->iowait, cpu_occupy->irq, cpu_occupy->softirq);

    fclose(fd);
    return 0;
}

double doorlock_calc_cpu_occupy_info(cpu_occupy_t *o, cpu_occupy_t *n)
{
    double od, nd;
    double id, sd;
    double cpu_use;

    od = (double)(o->user + o->nice + o->system + o->idle + o->softirq + o->iowait +
                  o->irq);  // 第一次(用户+优先级+系统+空闲)的时间再赋给od
    nd = (double)(n->user + n->nice + n->system + n->idle + n->softirq + n->iowait +
                  n->irq);  // 第二次(用户+优先级+系统+空闲)的时间再赋给od

    id = (double)(n->idle);  // 用户第一次和第二次的时间之差再赋给id
    sd = (double)(o->idle);  // 系统第一次和第二次的时间之差再赋给sd
    if ((nd - od) != 0)
        cpu_use =
            100.0 - ((id - sd)) / (nd - od) *
                        100.00;  //((用户+系统)乖100)除(第一次和第二次的时间差)再赋给g_cpu_used
    else
        cpu_use = 0;
    return cpu_use;
}

int doorlock_get_mem_occupy_info(mem_occupy_t *mem_info)
{
    FILE *fp = NULL;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        DOORLOCK_ERR("fopen:/proc/meminfo failed\n");
        return -1;
    }

    char data[1024] = {'\0'};
    char name[128] = {'\0'};
    char unit[128] = {'\0'};
    fgets(data, 1024, fp);
    sscanf(data, "%s %u %s", name, &mem_info->total, unit);

    fgets(data, 1024, fp);
    sscanf(data, "%s %u %s", name, &mem_info->free, unit);

    fgets(data, 1024, fp);
    sscanf(data, "%s %u %s", name, &mem_info->avaliable, unit);

    mem_info->usage = (1.0 - ((double)mem_info->avaliable) / ((double)mem_info->total)) * 100.00;

    fclose(fp);
    return 0;
}
