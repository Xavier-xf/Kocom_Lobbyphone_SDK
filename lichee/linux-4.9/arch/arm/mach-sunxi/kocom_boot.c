// SPDX-License-Identifier: GPL-2.0
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/crc32.h>
#include <linux/kocom_boot.h>
#include <linux/init.h>

#define BOOT_SEQ_MAGIC 0xb1000000U
#define BOOT_SEQ_MASK 0x00ffffffU
#define BOOT_SEAL 0x4b424f54U
static void __iomem *gpr;
static u32 boot_sequence;
static bool normal_restart, panicked;

static int prepare(struct notifier_block *nb, unsigned long action, void *data)
{
    normal_restart = action == SYS_RESTART;
    return NOTIFY_DONE;
}
static int panic_seen(struct notifier_block *nb, unsigned long action, void *data)
{
    panicked = true;
    return NOTIFY_DONE;
}
static struct notifier_block prepare_nb = { .notifier_call = prepare };
static struct notifier_block panic_nb = { .notifier_call = panic_seen };

static int __init kocom_boot_handoff_init(void)
{
    struct device_node *rtc;
    u32 offset, length, version;
    if (!of_chosen || of_property_read_u32(of_chosen, "kocom,boot-contract", &version) ||
        version != 1 || of_property_read_u32(of_chosen, "kocom,boot-sequence", &boot_sequence) ||
        !boot_sequence || boot_sequence >= BOOT_SEQ_MASK) return 0;
    rtc = of_find_compatible_node(NULL, NULL, "allwinner,sunxi-rtc");
    if (!rtc) return 0;
    if (of_property_read_u32(rtc, "gpr_offset", &offset) || offset != 0x100 ||
        of_property_read_u32(rtc, "gpr_len", &length) || length < 8) {
        of_node_put(rtc);
        return 0;
    }
    gpr = of_iomap(rtc, 0);
    of_node_put(rtc);
    if (!gpr) return 0;
    if (register_reboot_notifier(&prepare_nb)) {
        iounmap(gpr);
        gpr = NULL;
        return 0;
    }
    if (atomic_notifier_chain_register(&panic_notifier_list, &panic_nb)) {
        unregister_reboot_notifier(&prepare_nb);
        iounmap(gpr);
        gpr = NULL;
    }
    return 0;
}

late_initcall(kocom_boot_handoff_init);

void kocom_boot_handoff_restart(const char *command)
{
    u32 header, ticket = 0, kind = 1, value;
    unsigned char bytes[12];
    unsigned int i;
    if (!gpr) return;
    writel(0, gpr + 0x110); /* commit slot 4, invalidate first */
    if (!normal_restart || panicked) return;
    header = BOOT_SEQ_MAGIC | boot_sequence;
    if (readl(gpr + 0x104) != header || readl(gpr + 0x114) != ~header) return;
    /* RESTART2 is an existing syscall, not a new userspace kernel ABI. Only
     * our bounded M30 ticket gets OTA context; all other commands stay REBOOT. */
    if (command && !strncmp(command, "kocom-m30:", 10) && strlen(command) == 18 &&
        !kstrtou32(command + 10, 16, &ticket) && ticket) kind = 2;
    for (i = 0; i < 4; ++i) {
        bytes[i] = header >> (i * 8);
        bytes[i + 4] = ticket >> (i * 8);
        bytes[i + 8] = kind >> (i * 8);
    }
    value = (crc32_le(~0U, bytes, sizeof(bytes)) ^ ~0U) ^ BOOT_SEAL;
    if (!value) return;
    writel(ticket, gpr + 0x10c);
    wmb();
    writel(value, gpr + 0x110); /* commit last, immediately before reset */
    readl(gpr + 0x110);
}
