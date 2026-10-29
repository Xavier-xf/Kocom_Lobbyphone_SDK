/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef UPGRADE_JFFS2_H
#define UPGRADE_JFFS2_H

struct upgrade_jffs2;
int upgrade_jffs2_open(struct upgrade_jffs2 **out);
int upgrade_jffs2_load(struct upgrade_jffs2 *fs, const char *name,
                      void **data, unsigned int *size, unsigned int limit);
void upgrade_jffs2_close(struct upgrade_jffs2 *fs);

#endif
