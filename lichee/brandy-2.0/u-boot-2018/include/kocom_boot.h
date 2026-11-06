/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef KOCOM_BOOT_H
#define KOCOM_BOOT_H
#ifdef CONFIG_MACH_SUN8IW21
void kocom_boot_init(void);
void kocom_boot_power_sample(void);
void kocom_boot_fdt(void *blob);
/* Save only a completed install. Does not change upgrade success/recovery. */
int kocom_boot_ota_complete(const char *identity);
int kocom_boot_ota_begin(void);
#else
static inline void kocom_boot_init(void) {}
static inline void kocom_boot_power_sample(void) {}
static inline void kocom_boot_fdt(void *blob) { (void)blob; }
static inline int kocom_boot_ota_begin(void) { return 0; }
static inline int kocom_boot_ota_complete(const char *id) { (void)id; return -1; }
#endif
#endif
