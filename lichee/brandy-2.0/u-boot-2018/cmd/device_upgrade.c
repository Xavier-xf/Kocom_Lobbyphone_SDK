// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <watchdog.h>
#include <command.h>
#include <environment.h>
#include <malloc.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/libfdt.h>
#include <sunxi_flash.h>
#include <sys_partition.h>
#include <sunxi_flashmap.h>
#include <u-boot/md5.h>
#include <u-boot/crc.h>
#include <u-boot/sha256.h>
#include <device_upgrade.h>
#include <upgrade_jffs2.h>

#define PACKAGE_LIMIT (12U * 1024U * 1024U)
#define HEADER_LIMIT 65536U
#define PART_LIMIT 10
#define IO_SIZE 65536U
#define REQUEST_ENV "DEVICE_UPGRADE_REQUEST"
#define HASH_ENV "DEVICE_UPGRADE_SHA256"
#define STATUS_ENV "DEVICE_UPGRADE_STATUS"
#define END_MARKER "# <- this is end of image parttion"

int sprite_cartoon_upgrade(int rate);
uint sprite_cartoon_create(int op);

struct upgrade_part {
    char name[16];
    unsigned int sector, sectors, offset, size;
    unsigned char md5[16];
};

static int hex_bytes(const char *text, unsigned char *bytes, unsigned int count)
{
    unsigned int i;
    if (strlen(text) != count * 2)
        return -EINVAL;
    for (i = 0; i < count * 2; ++i) {
        unsigned char c = text[i];
        unsigned int v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return -EINVAL;
        if (!(i & 1)) bytes[i / 2] = v << 4;
        else bytes[i / 2] |= v;
    }
    return 0;
}

static int next_line(const unsigned char *data, unsigned int size,
                      unsigned int *position, char *line, unsigned int capacity)
{
    unsigned int start = *position, length = 0;
    while (*position < size && *position < HEADER_LIMIT) {
        unsigned char c = data[(*position)++];
        if (c == '\n') {
            line[length] = 0;
            return 0;
        }
        if (c < 32 || c > 126 || length + 1 >= capacity)
            return -EINVAL;
        line[length++] = c;
    }
    *position = start;
    return -EINVAL;
}

static int parse_part(char *line, struct upgrade_part *part)
{
    char *fields[6], *cursor = line, *end;
    unsigned int i;
    const char *prefix = "# File partition: ";
    if (strncmp(cursor, prefix, strlen(prefix))) return -EINVAL;
    cursor += strlen(prefix);
    for (i = 0; i < 6; ++i) {
        fields[i] = strsep(&cursor, " ");
        if (!fields[i] || !*fields[i]) return -EINVAL;
    }
    if (cursor || strlen(fields[0]) >= sizeof(part->name)) return -EINVAL;
    strcpy(part->name, fields[0]);
    for (i = 1; i <= 4; ++i) {
        const char *p;
        unsigned int value;
        if (strlen(fields[i]) > 8) return -EINVAL;
        for (p = fields[i]; *p; ++p) if (!isxdigit(*p)) return -EINVAL;
        value = simple_strtoul(fields[i], &end, 16);
        if (*end) return -EINVAL;
        switch (i) {
        case 1: part->sector = value; break;
        case 2: part->sectors = value; break;
        case 3: part->offset = value; break;
        case 4: part->size = value; break;
        }
    }
    return hex_bytes(fields[5], part->md5, 16);
}

static int allowed_partition(const char *name)
{
    /* data holds the source; env holds the durable receipt. Bootloader and
     * partition-map maintenance remain on the independent SD recovery path. */
    return !strcmp(name, "app") || !strcmp(name, "resource") ||
           !strcmp(name, "rootfs") || !strcmp(name, "boot") || !strcmp(name, "logo");
}

static int package_check(const unsigned char *data, unsigned int size,
                          struct upgrade_part *parts, int *count, unsigned int *base)
{
    char line[256];
    unsigned int position = 0, payload = 0, i;
    unsigned int logical = sunxi_flashmap_logical_offset(FLASHMAP_SPI_NOR, LINUX_LOGIC_OFFSET);
    unsigned int flash_sectors = sunxi_flash_size();
    int n = 0, ended = 0;
    const unsigned int prefix_length = sizeof("#<upgrade_bin_version=") - 1;
    if (next_line(data, size, &position, line, sizeof(line)) ||
        strlen(line) != prefix_length + 14 || strncmp(line, "#<upgrade_bin_version=", prefix_length))
        return -EINVAL;
    for (i = prefix_length; i < prefix_length + 14; ++i)
        if (!isdigit(line[i])) return -EINVAL;
    while (!next_line(data, size, &position, line, sizeof(line))) {
        struct upgrade_part *p;
        uint sector, sectors;
        if (!strcmp(line, END_MARKER)) { ended = 1; break; }
        if (n >= PART_LIMIT) return -EINVAL;
        p = &parts[n];
        if (parse_part(line, p) || !allowed_partition(p->name) ||
            sunxi_partition_get_info_byname(p->name, &sector, &sectors) ||
            (u64)p->sector != (u64)sector + logical || p->sectors != sectors ||
            (u64)p->sector + p->sectors > flash_sectors ||
            !p->size || p->size > size - payload || p->offset != payload ||
            p->size > (u64)sectors * 512 || p->sector % (IO_SIZE / 512) ||
            p->sectors % (IO_SIZE / 512))
            return -EINVAL;
        for (i = 0; i < n; ++i)
            if (!strcmp(parts[i].name, p->name) ||
                ((u64)p->sector < (u64)parts[i].sector + parts[i].sectors &&
                 (u64)parts[i].sector < (u64)p->sector + p->sectors))
                return -EINVAL;
        payload += p->size;
        ++n;
    }
    if (!ended || !n || payload != size - position)
        return -EINVAL;
    for (i = 0; i < n; ++i) {
        unsigned char digest_bytes[16];
        md5_wd((unsigned char *)data + position + parts[i].offset, parts[i].size,
               digest_bytes, IO_SIZE);
        if (memcmp(digest_bytes, parts[i].md5, 16))
            return -EINVAL;
    }
    *count = n;
    *base = position;
    return 0;
}

static int environment_has(const env_t *environment, const char *key, const char *value)
{
    const char *entry = (const char *)environment->data;
    const char *end = entry + ENV_SIZE;
    size_t key_length = strlen(key);
    while (entry < end && *entry) {
        const char *nul = memchr(entry, 0, end - entry);
        if (!nul) return 0;
        if ((size_t)(nul - entry) == key_length + 1 + strlen(value) &&
            !memcmp(entry, key, key_length) && entry[key_length] == '=' &&
            !strcmp(entry + key_length + 1, value)) return 1;
        entry = nul + 1;
    }
    return 0;
}

static int save_result(const char *id, const char *hash, const char *status)
{
    env_t *saved;
    uint sector, sectors;
    int rc = -EIO;
    if (env_set(REQUEST_ENV, id) || env_set(HASH_ENV, hash) ||
        env_set(STATUS_ENV, status) || env_save())
        goto failed;
    if (sunxi_partition_get_info_byname("env", &sector, &sectors) ||
        (u64)sectors * 512 < CONFIG_ENV_SIZE) goto failed;
    saved = memalign(ARCH_DMA_MINALIGN, ALIGN(CONFIG_ENV_SIZE, 512));
    if (!saved) goto failed;
    if (sunxi_flash_read(sector, ALIGN(CONFIG_ENV_SIZE, 512) / 512, saved) == ALIGN(CONFIG_ENV_SIZE, 512) / 512 &&
        saved->crc == crc32(0, saved->data, ENV_SIZE) &&
        environment_has(saved, REQUEST_ENV, id) && environment_has(saved, HASH_ENV, hash) &&
        environment_has(saved, STATUS_ENV, status)) rc = 0;
    free(saved);
    if (!rc) return 0;
failed:
    /* Do not publish a success receipt from RAM when its durable write failed. */
    env_set(STATUS_ENV, "unknown");
    return -EIO;
}

static int write_parts(const unsigned char *data, struct upgrade_part *parts,
                        int count, unsigned int base)
{
    unsigned char *buffer = memalign(ARCH_DMA_MINALIGN, IO_SIZE);
    unsigned char *verify = memalign(ARCH_DMA_MINALIGN, IO_SIZE);
    unsigned int total = 0, done = 0;
    int i, rc = -ENOMEM;
    if (!buffer || !verify) goto out;
    for (i = 0; i < count; ++i) total += parts[i].sectors;
    for (i = 0; i < count; ++i) {
        struct upgrade_part *p = &parts[i];
        unsigned int sector;
        printf("internal upgrade: writing %s\n", p->name);
        for (sector = 0; sector < p->sectors; sector += IO_SIZE / 512) {
            unsigned int offset = sector * 512;
            memset(buffer, 0xff, IO_SIZE);
            if (offset < p->size)
                memcpy(buffer, data + base + p->offset + offset,
                        min(IO_SIZE, p->size - offset));
            if (sunxi_flash_phywrite(p->sector + sector, IO_SIZE / 512, buffer) != IO_SIZE / 512 ||
                sunxi_flash_phyread(p->sector + sector, IO_SIZE / 512, verify) != IO_SIZE / 512 ||
                memcmp(buffer, verify, IO_SIZE)) {
                rc = -EIO;
                goto out;
            }
            done += IO_SIZE / 512;
            sprite_cartoon_upgrade(10 + (u64)done * 85 / total);
            WATCHDOG_RESET();
        }
    }
    rc = sunxi_flash_flush();
out:
    free(verify);
    free(buffer);
    return rc;
}

static void recovery_required(void)
{
    printf("internal upgrade incomplete: do not boot partial partitions. Restart to retry, or use SD recovery.\n");
    for (;;) { WATCHDOG_RESET(); mdelay(500); }
}

int device_upgrade_internal(void)
{
    struct upgrade_jffs2 *fs = NULL;
    struct upgrade_part parts[PART_LIMIT];
    void *request = NULL, *package = NULL;
    unsigned int request_size = 0, package_size = 0, pos = 0, expected, base;
    char line[96], id[65], hash[65], *end;
    unsigned char expected_hash[32], actual_hash[32], id_bytes[32];
    const char *previous_id, *previous_hash, *previous_status;
    int count, rc, owned = 0;
    int interrupted;
    previous_status = env_get(STATUS_ENV);
    interrupted = previous_status && (!strcmp(previous_status, "writing") || !strcmp(previous_status, "failed"));
    rc = upgrade_jffs2_open(&fs);
    if (rc) {
        if (interrupted) recovery_required();
        return 0;
    }
    rc = upgrade_jffs2_load(fs, "pending", &request, &request_size, 256);
    if (rc == -ENOENT) {
        if (interrupted) recovery_required();
        goto out;
    }
    owned = 1;
    if (rc || next_line(request, request_size, &pos, line, sizeof(line)) ||
        strcmp(line, "DEVICE-UPGRADE-1") ||
        next_line(request, request_size, &pos, id, sizeof(id)) || hex_bytes(id, id_bytes, 32) ||
        next_line(request, request_size, &pos, hash, sizeof(hash)) || hex_bytes(hash, expected_hash, 32) ||
        next_line(request, request_size, &pos, line, sizeof(line)) || pos != request_size) {
        printf("internal upgrade: invalid request; no flash writes\n");
        if (interrupted) recovery_required();
        goto out;
    }
    if (!*line || strlen(line) > 8) {
        if (interrupted) recovery_required();
        goto out;
    }
    for (end = line; *end; ++end) {
        if (!isdigit(*end)) {
            if (interrupted) recovery_required();
            goto out;
        }
    }
    expected = simple_strtoul(line, &end, 10);
    if (*end || expected < 1024 || expected > PACKAGE_LIMIT) {
        if (interrupted) recovery_required();
        goto out;
    }
    previous_id = env_get(REQUEST_ENV);
    previous_hash = env_get(HASH_ENV);
    if (interrupted && (!previous_id || !previous_hash || strcmp(previous_id, id) || strcmp(previous_hash, hash)))
        recovery_required();
    previous_status = env_get(STATUS_ENV);
    if (previous_id && previous_hash && previous_status && !strcmp(previous_id, id) &&
        !strcmp(previous_hash, hash) && (!strcmp(previous_status, "complete") ||
                                       !strcmp(previous_status, "superseded") || !strcmp(previous_status, "invalid")))
        goto out;
    rc = upgrade_jffs2_load(fs, "package.img", &package, &package_size, expected);
    upgrade_jffs2_close(fs);
    fs = NULL;
    if (!rc && package_size == expected) {
        sha256_csum_wd(package, package_size, actual_hash, IO_SIZE);
        rc = memcmp(expected_hash, actual_hash, 32) ? -EINVAL :
             package_check(package, package_size, parts, &count, &base);
    } else rc = -EINVAL;
    if (rc) {
        if (interrupted) recovery_required();
        save_result(id, hash, "invalid");
        printf("internal upgrade: validation failed; no flash writes\n");
        goto out;
    }
    if (save_result(id, hash, "writing")) {
        printf("internal upgrade: cannot save intent; no partition writes\n");
        if (interrupted) recovery_required();
        goto out;
    }
    sprite_cartoon_create(2);
    sprite_cartoon_upgrade(10);
    rc = write_parts(package, parts, count, base);
    if (!rc) rc = save_result(id, hash, "complete");
    if (rc) {
        save_result(id, hash, "failed");
        printf("internal upgrade failed; package retained. Restart to retry, or use SD recovery.\n");
        recovery_required();
    }
    sprite_cartoon_upgrade(100);
    printf("internal upgrade: verified; restarting\n");
    run_command("reset", 0);
out:
    free(package);
    free(request);
    upgrade_jffs2_close(fs);
    return owned;
}

static unsigned int sd_checked_parts;
static int sd_recovery_incomplete;

void device_upgrade_sd_begin(void)
{
    const char *state = env_get(STATUS_ENV);
    sd_checked_parts = 0;
    /* Capture before an SD package can replace env itself. */
    sd_recovery_incomplete = state && (!strcmp(state, "writing") || !strcmp(state, "failed"));
}

int device_upgrade_sd_recovery_required(void)
{
    return sd_recovery_incomplete;
}

void device_upgrade_sd_checked(const char *partition)
{
    static const char *names[] = {"app", "resource", "rootfs", "boot", "logo"};
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(names); ++i)
        if (!strcmp(partition, names[i])) sd_checked_parts |= 1U << i;
}

int device_upgrade_sd_ready(void)
{
    if (sd_recovery_incomplete && sd_checked_parts != 31U) {
        printf("SD recovery must include app, resource, rootfs, boot and logo after an interrupted internal update\n");
        return -EINVAL;
    }
    return 0;
}

int device_upgrade_sd_complete(void)
{
    struct upgrade_jffs2 *fs = NULL;
    void *request = NULL;
    unsigned int size = 0, pos = 0;
    char line[96], id[65], hash[65], *end;
    unsigned char bytes[32];
    unsigned int expected;
    int rc;
    if (device_upgrade_sd_ready()) return -EINVAL;
    rc = upgrade_jffs2_open(&fs);
    if (rc) goto out;
    rc = upgrade_jffs2_load(fs, "pending", &request, &size, 256);
    if (rc == -ENOENT) {
        /* Deleting an absent variable fails in this U-Boot. */
        rc = env_get(REQUEST_ENV) ? env_set(REQUEST_ENV, NULL) : 0;
        if (!rc && env_get(HASH_ENV)) rc = env_set(HASH_ENV, NULL);
    } else {
        /* An unreadable request is not an absent request. Keep recovery active
         * rather than lose the identity needed to retire the old internal pkg. */
        if (rc) goto out;
        rc = -EINVAL;
        if (next_line(request, size, &pos, line, sizeof(line)) ||
            strcmp(line, "DEVICE-UPGRADE-1") ||
            next_line(request, size, &pos, id, sizeof(id)) || hex_bytes(id, bytes, 32) ||
            next_line(request, size, &pos, hash, sizeof(hash)) || hex_bytes(hash, bytes, 32) ||
            next_line(request, size, &pos, line, sizeof(line)) || pos != size ||
            !*line || strlen(line) > 8) goto out;
        for (end = line; *end; ++end) if (!isdigit(*end)) goto out;
        expected = simple_strtoul(line, &end, 10);
        if (*end || expected < 1024 || expected > PACKAGE_LIMIT) goto out;
        rc = env_set(REQUEST_ENV, id);
        if (!rc) rc = env_set(HASH_ENV, hash);
    }
    if (!rc) rc = env_set(STATUS_ENV, "superseded");
out:
    free(request);
    upgrade_jffs2_close(fs);
    return rc;
}

void device_upgrade_fdt(void *blob)
{
    int chosen = fdt_path_offset(blob, "/chosen");
    const char *id = env_get(REQUEST_ENV), *hash = env_get(HASH_ENV), *status = env_get(STATUS_ENV);
    if (chosen < 0) chosen = fdt_add_subnode(blob, 0, "chosen");
    if (chosen < 0) return;
    /* Capability comes from the bootloader actually running, not an APP guess. */
    fdt_setprop_string(blob, chosen, "device-upgrade,internal-upgrade", "1");
    if (id && strlen(id) == 64 && hash && strlen(hash) == 64 && status) {
        fdt_setprop_string(blob, chosen, "device-upgrade,upgrade-request", id);
        fdt_setprop_string(blob, chosen, "device-upgrade,upgrade-sha256", hash);
        fdt_setprop_string(blob, chosen, "device-upgrade,upgrade-status", status);
    }
}
