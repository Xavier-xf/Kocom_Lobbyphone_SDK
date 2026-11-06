/* SPDX-License-Identifier: GPL-2.0 */
#ifndef KOCOM_BOOT_HANDOFF_H
#define KOCOM_BOOT_HANDOFF_H
#ifdef CONFIG_ARCH_SUN8IW21
void kocom_boot_handoff_restart(const char *command);
#else
static inline void kocom_boot_handoff_restart(const char *command) { (void)command; }
#endif
#endif
