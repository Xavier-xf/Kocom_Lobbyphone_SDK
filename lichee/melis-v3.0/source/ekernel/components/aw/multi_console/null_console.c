#include <cli_console.h>
#include <hal_time.h>
#include <stdint.h>
#include <kconfig.h>

int null_console_write(const void *buf, size_t len, void *privata_data)
{
    (void)buf;
    (void)len;
    (void)privata_data;
    return len;
}

int null_console_read(void *buf, size_t len, void *privata_data)
{
    (void)buf;
    (void)len;
    (void)privata_data;

    hal_sleep(1);
    return 0;
}

static int null_console_dummy_cb(void *private_data)
{
    return 1;
}

static device_console null_console =
{
    .name       = "null-console",
    .write      = null_console_write,
    .read       = null_console_read,
    .init       = null_console_dummy_cb,
    .deinit     = null_console_dummy_cb,
};

cli_console cli_null_console =
{
    .i_list         = {0},
    .name           = "cli-null",
    .dev_console    = &null_console,
    .init_flag      = 0,
    .exit_flag      = 0,
    .alive          = 1,
    .private_data   = NULL,
    .task_list      = {0},
    .finsh_callback = NULL,
    .start_callback = NULL,
};
