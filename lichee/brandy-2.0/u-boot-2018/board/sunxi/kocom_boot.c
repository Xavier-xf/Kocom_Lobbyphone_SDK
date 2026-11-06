// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <environment.h>
#include <malloc.h>
#include <linux/libfdt.h>
#include <asm/arch/rtc.h>
#include <u-boot/crc.h>
#include <sunxi_flash.h>
#include <sys_partition.h>
#include <kocom_boot.h>
#ifdef CONFIG_AXP2101_POWER
#include <sunxi_power/pmu_axp2101.h>
#include <asm/arch/pmic_bus.h>
#endif

/* Four dedicated GPR slots. Leave 0 (vendor reason), 2 (FEL), 6 (mode),
 * 7 (vendor bootcount) alone. Wire contract is duplicated in the kernel;
 * neither independent project imports the other's source. */
#define BOOT_SEQ_SLOT 1
#define BOOT_TICKET_SLOT 3
#define BOOT_COMMIT_SLOT 4
#define BOOT_INVERSE_SLOT 5
#define BOOT_SEQ_MAGIC 0xb1000000U
#define BOOT_SEQ_MASK 0x00ffffffU
#define BOOT_SEAL 0x4b424f54U
#define OTA_ENV "KOCOM_BOOT_OTA"
#define POWER_ENV "KOCOM_POWER_TRACK"
#define POWER_RETAINED 1U
#define POWER_POR 2U

static u32 sequence, source_sequence, ticket, cause;
static int initialized, ota_retire_failed;
static char linux_ota[65];
static u32 power_state;
static int power_sampled, power_present, power_zero, power_tracked;
static int power_finalized;

static u32 seal(u32 header, u32 value, u32 kind)
{
    /* Explicit bytes avoid host-endianness dependent CRCs. */
    unsigned char bytes[12];
    unsigned int i;
    for (i = 0; i < 4; ++i) {
        bytes[i] = header >> (i * 8);
        bytes[i + 4] = value >> (i * 8);
        bytes[i + 8] = kind >> (i * 8);
    }
    return crc32(0, bytes, sizeof(bytes)) ^ BOOT_SEAL;
}

void kocom_boot_init(void)
{
    u32 header, value, commit, next;
    if (initialized) return;
    initialized = 1;
    header = rtc_read_data(BOOT_SEQ_SLOT);
    value = rtc_read_data(BOOT_TICKET_SLOT);
    commit = rtc_read_data(BOOT_COMMIT_SLOT);
    /* Consume before any upgrade/menu/early-return path can run. */
    rtc_write_data(BOOT_COMMIT_SLOT, 0);
    if (rtc_read_data(BOOT_COMMIT_SLOT)) return;
    if ((header & ~BOOT_SEQ_MASK) == BOOT_SEQ_MAGIC &&
        rtc_read_data(BOOT_INVERSE_SLOT) == ~header && (header & BOOT_SEQ_MASK)) {
        source_sequence = header & BOOT_SEQ_MASK;
        if (commit && commit == seal(header, value, 1U)) cause = 1U;
        else if (commit && value && commit == seal(header, value, 2U)) cause = 2U;
        ticket = value;
        if (source_sequence == BOOT_SEQ_MASK) { cause = 0; return; }
        next = source_sequence + 1U;
    } else {
        next = 1U;
    }
    header = BOOT_SEQ_MAGIC | next;
    rtc_write_data(BOOT_INVERSE_SLOT, 0);
    rtc_write_data(BOOT_SEQ_SLOT, header);
    rtc_write_data(BOOT_TICKET_SLOT, 0);
    rtc_write_data(BOOT_INVERSE_SLOT, ~header);
    if (rtc_read_data(BOOT_SEQ_SLOT) == header &&
        rtc_read_data(BOOT_INVERSE_SLOT) == ~header) sequence = next;
    else cause = 0;
}

/* AXP2101 datasheet v1.4 sections 6.13.2.3..5: DATA_BUFFER0..2 are
 * reset only by PMIC POR. BUFFER3 remains the vendor system-mode register.
 * Called after axp_probe, before the environment is available. No rail/control
 * register is written. The RTC backup supply is not our POR evidence. */
void kocom_boot_power_sample(void)
{
#ifdef CONFIG_AXP2101_POWER
    static const u8 marker[3] = {0x4b, 0x50, 0x31};
    u8 chip, value[3], verify;
    unsigned int i;
    if (power_sampled) return;
    power_sampled = 1;
    if (pmic_bus_read(AXP2101_RUNTIME_ADDR, AXP2101_VERSION, &chip) ||
        ((chip & 0xcf) != 0x47 && (chip & 0xcf) != 0x4a)) return;
    for (i = 0; i < 3; ++i)
        if (pmic_bus_read(AXP2101_RUNTIME_ADDR, AXP2101_DATA_BUFFER0 + i, &value[i])) return;
    power_present = 1;
    power_zero = !(value[0] | value[1] | value[2]);
    if (!memcmp(value, marker, sizeof(marker))) {
        power_tracked = 1;
        return;
    }
    if (!power_zero) return; /* Foreign/corrupt marker: do not overwrite or guess. */
    /* Zero means POR OR first enrollment. Never inherit an RTC reboot seal
     * across either; the persistent enrollment below distinguishes them. */
    cause = 0;
    for (i = 0; i < 3; ++i)
        if (pmic_bus_write(AXP2101_RUNTIME_ADDR, AXP2101_DATA_BUFFER0 + i, marker[i])) return;
    for (i = 0; i < 3; ++i)
        if (pmic_bus_read(AXP2101_RUNTIME_ADDR, AXP2101_DATA_BUFFER0 + i, &verify) ||
            verify != marker[i]) return;
    power_tracked = 1;
#endif
}

/* Read back the actual env partition, not merely env_get's RAM copy. */
static int saved_env_matches(const char *name, const char *expected)
{
    env_t *saved;
    uint sector, sectors;
    char *entry, *end;
    int found = 0, valid = 0;
    size_t name_length = strlen(name);
    if (sunxi_partition_get_info_byname("env", &sector, &sectors) ||
        (u64)sectors * 512 < CONFIG_ENV_SIZE) return -1;
    saved = memalign(ARCH_DMA_MINALIGN, ALIGN(CONFIG_ENV_SIZE, 512));
    if (!saved) return -1;
    if (sunxi_flash_read(sector, ALIGN(CONFIG_ENV_SIZE, 512) / 512, saved) !=
        ALIGN(CONFIG_ENV_SIZE, 512) / 512 || saved->crc != crc32(0, saved->data, ENV_SIZE))
        goto out;
    entry = (char *)saved->data;
    end = entry + ENV_SIZE;
    while (entry < end && *entry) {
        char *nul = memchr(entry, 0, end - entry);
        if (!nul) goto out;
        if ((size_t)(nul - entry) > name_length && !memcmp(entry, name, name_length) &&
            entry[name_length] == '=') {
            if (found || !expected || strcmp(entry + name_length + 1, expected)) goto out;
            found = 1;
        }
        entry = nul + 1;
    }
    valid = entry < end && (!!expected == found);
out:
    free(saved);
    return valid ? 0 : -1;
}

/* Enrollment is saved only after the PMIC marker was verified. A first
 * install with zero scratch bytes is unknown, not fabricated POWER_ON.
 * This one-time env bit avoids flash writes on every ordinary boot. */
static void power_finalize(void)
{
    const char *enrolled;
    if (power_finalized) return;
    power_finalized = 1;
    if (!power_present || !power_tracked) return;
    enrolled = env_get(POWER_ENV);
    if (enrolled) {
        if (strcmp(enrolled, "1") || saved_env_matches(POWER_ENV, "1"))
            goto unavailable;
        power_state = power_zero ? POWER_POR : POWER_RETAINED;
    } else {
        /* env_save writes the whole RAM environment. Never overwrite an
         * existing enrollment or turn an unverified OTA value into evidence. */
        if (saved_env_matches(POWER_ENV, NULL) ||
            saved_env_matches(OTA_ENV, env_get(OTA_ENV))) goto unavailable;
        if (env_set(POWER_ENV, "1") || env_save() || saved_env_matches(POWER_ENV, "1")) {
            env_set(POWER_ENV, NULL);
            goto unavailable;
        }
    }
    printf("boot reason: power=%u pmic_zero=%d tracked=%d\n",
           power_state, power_zero, power_tracked);
    return;
unavailable:
    printf("boot reason: PMIC enrollment unavailable\n");
}

/* Only an existing unconsumed completion requires a durable retirement.
 * A new install must not inherit a prior install's completion. */
int kocom_boot_ota_begin(void)
{
    linux_ota[0] = 0;
    if (ota_retire_failed) return -1;
    if (!env_get(OTA_ENV)) return 0;
    if (env_set(OTA_ENV, NULL) || env_save() || saved_env_matches(OTA_ENV, NULL)) {
        /* RAM may already be clear while flash still holds the old receipt.
         * Repeated attempts in this U-Boot must not bypass the failed retire. */
        ota_retire_failed = 1;
        return -1;
    }
    return 0;
}

int kocom_boot_ota_complete(const char *identity)
{
    char record[80];
    size_t i;
    /* Digest is opaque install identity, not an installed-version claim. */
    if (ota_retire_failed || !identity || strlen(identity) != 64) return -1;
    for (i = 0; i < 64; ++i)
        if (!((identity[i] >= '0' && identity[i] <= '9') ||
              (identity[i] >= 'a' && identity[i] <= 'f'))) return -1;
    snprintf(record, sizeof(record), "1:%s", identity);
    if (env_set(OTA_ENV, record) || env_save() || saved_env_matches(OTA_ENV, record)) {
        /* Never expose an unverified RAM-only completion. */
        env_set(OTA_ENV, NULL);
        return -1;
    }
    return 0;
}

void kocom_boot_fdt(void *blob)
{
    int chosen;
    const char *pending;
    char ota[80];
    size_t i;
    kocom_boot_init();
    power_finalize();
    chosen = fdt_path_offset(blob, "/chosen");
    if (chosen < 0) chosen = fdt_add_subnode(blob, 0, "chosen");
    if (chosen < 0) return;
    /* Clear inherited properties. A repeated fixup may target a fresh blob,
     * but belongs to this same U-Boot instance, not another Linux boot. */
    fdt_delprop(blob, chosen, "kocom,boot-contract");
    fdt_delprop(blob, chosen, "kocom,power-state");
    fdt_delprop(blob, chosen, "kocom,reboot-source");
    fdt_delprop(blob, chosen, "kocom,reboot-kind");
    fdt_delprop(blob, chosen, "kocom,reboot-ticket");
    fdt_delprop(blob, chosen, "kocom,ota-completed");
    if (fdt_setprop_u32(blob, chosen, "kocom,boot-sequence", sequence) ||
        fdt_setprop_u32(blob, chosen, "kocom,power-state", power_state)) return;
    if (cause && sequence &&
        (fdt_setprop_u32(blob, chosen, "kocom,reboot-source", source_sequence) ||
         fdt_setprop_u32(blob, chosen, "kocom,reboot-kind", cause) ||
         fdt_setprop_u32(blob, chosen, "kocom,reboot-ticket", ticket))) return;
    if (fdt_setprop_u32(blob, chosen, "kocom,boot-contract", 1)) return;
    if (ota_retire_failed) return;
    if (linux_ota[0]) {
        fdt_setprop_string(blob, chosen, "kocom,ota-completed", linux_ota);
        return;
    }
    pending = env_get(OTA_ENV);
    if (!pending || strlen(pending) != 66 || strncmp(pending, "1:", 2)) return;
    for (i = 2; i < 66; ++i)
        if (!((pending[i] >= '0' && pending[i] <= '9') ||
              (pending[i] >= 'a' && pending[i] <= 'f'))) return;
    strcpy(ota, pending);
    /* At-most-once Linux handoff, not a cloud-delivery receipt. A power cut
     * after consume may lose this reason; never replay it into a later boot. */
    if (saved_env_matches(OTA_ENV, ota) || env_set(OTA_ENV, NULL) || env_save() ||
        saved_env_matches(OTA_ENV, NULL)) return;
    strcpy(linux_ota, ota + 2);
    fdt_setprop_string(blob, chosen, "kocom,ota-completed", linux_ota);
}
