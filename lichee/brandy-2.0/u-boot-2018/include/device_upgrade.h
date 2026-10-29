/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef DEVICE_UPGRADE_H
#define DEVICE_UPGRADE_H

/* Return 1 when an internal request owns this boot, 0 when there is none. */
int device_upgrade_internal(void);
/* SD recovery must replace all system partitions after an interrupted write. */
void device_upgrade_sd_begin(void);
int device_upgrade_sd_recovery_required(void);
void device_upgrade_sd_checked(const char *partition);
int device_upgrade_sd_ready(void);
int device_upgrade_sd_complete(void);
void device_upgrade_fdt(void *blob);

#endif
