// SPDX-License-Identifier: GPL-2.0+
/* Read-only, bounded JFFS2 access for /upgrade on Sunxi SPI NOR.
 * Never writes filesystem nodes or assumes NOR is memory mapped. */
#include <common.h>
#include <watchdog.h>
#include <malloc.h>
#include <linux/errno.h>
#include <linux/stat.h>
#include <jffs2/jffs2.h>
#include <u-boot/crc.h>
#include <u-boot/zlib.h>
#include <sunxi_flash.h>
#include <sys_partition.h>
#include <sunxi_flashmap.h>
#include <upgrade_jffs2.h>

#define DATA_LIMIT (16U * 1024U * 1024U)
#define ERASE_SIZE 65536U
#define NODE_LIMIT 65536U
#define FRAGMENT_LIMIT 32768U

struct upgrade_jffs2 {
    unsigned char *image;
    unsigned int size;
};

struct fragment {
    const struct jffs2_raw_inode *node;
    u32 version;
};

static u32 jcrc(const void *data, unsigned int size)
{
    return crc32_no_comp(0, data, size);
}

/* Invalid/torn nodes are skipped just as after an interrupted JFFS2 write.
 * File length, fragment CRCs and the final package digest remain mandatory. */
static const struct jffs2_unknown_node *next_node(struct upgrade_jffs2 *fs,
                                                  unsigned int *position)
{
    while (*position <= fs->size - sizeof(struct jffs2_unknown_node)) {
        const struct jffs2_unknown_node *n = (const void *)(fs->image + *position);
        u32 length = le32_to_cpu(n->totlen);
        u16 type = le16_to_cpu(n->nodetype);
        struct jffs2_unknown_node header = *n;
        header.nodetype = cpu_to_le16(type | JFFS2_NODE_ACCURATE);
        if (le16_to_cpu(n->magic) != JFFS2_MAGIC_BITMASK ||
            length < sizeof(*n) || length > NODE_LIMIT || length > fs->size - *position ||
            length > ERASE_SIZE - (*position % ERASE_SIZE) ||
            le32_to_cpu(n->hdr_crc) != jcrc(&header, sizeof(header) - 4)) {
            *position += 4;
            continue;
        }
        *position += ALIGN(length, 4);
        if (type & JFFS2_NODE_ACCURATE)
            return n;
    }
    return NULL;
}

int upgrade_jffs2_open(struct upgrade_jffs2 **out)
{
    struct upgrade_jffs2 *fs;
    uint sector, sectors;
    unsigned int offset;
    int logical = sunxi_flashmap_logical_offset(FLASHMAP_SPI_NOR, LINUX_LOGIC_OFFSET);
    if (!out) return -EINVAL;
    *out = NULL;
    if (logical < 0 || sunxi_partition_get_info_byname("data", &sector, &sectors) ||
        !sectors || sectors > DATA_LIMIT / 512 || sectors % (ERASE_SIZE / 512) ||
        ((u64)sector + logical) % (ERASE_SIZE / 512) ||
        (u64)sector + logical + sectors > sunxi_flash_size())
        return -EINVAL;
    fs = calloc(1, sizeof(*fs));
    if (!fs)
        return -ENOMEM;
    fs->size = sectors * 512;
    fs->image = memalign(ARCH_DMA_MINALIGN, fs->size);
    if (!fs->image) {
        free(fs);
        return -ENOMEM;
    }
    for (offset = 0; offset < sectors; offset += ERASE_SIZE / 512) {
        if (sunxi_flash_read(sector + offset, ERASE_SIZE / 512,
                             fs->image + offset * 512) != ERASE_SIZE / 512) {
            upgrade_jffs2_close(fs);
            return -EIO;
        }
        WATCHDOG_RESET();
    }
    *out = fs;
    return 0;
}

void upgrade_jffs2_close(struct upgrade_jffs2 *fs)
{
    if (fs) {
        free(fs->image);
        free(fs);
    }
}

static int find_entry(struct upgrade_jffs2 *fs, u32 parent, const char *name,
                      unsigned int expected_type, u32 *inode)
{
    const struct jffs2_unknown_node *n;
    unsigned int position = 0;
    u32 version = 0;
    unsigned int type = 0;
    int found = 0, damaged = 0;
    size_t length = strlen(name);
    *inode = 0;
    while ((n = next_node(fs, &position))) {
        const struct jffs2_raw_dirent *d = (const void *)n;
        if (le16_to_cpu(n->nodetype) != JFFS2_NODETYPE_DIRENT ||
            le32_to_cpu(n->totlen) < sizeof(*d))
            continue;
        if (le32_to_cpu(d->pino) != parent || d->nsize != length ||
            sizeof(*d) + length != le32_to_cpu(n->totlen) ||
            memcmp(d->name, name, length))
            continue;
        if (le32_to_cpu(d->node_crc) != jcrc(d, sizeof(*d) - 8) ||
            le32_to_cpu(d->name_crc) != jcrc(d->name, length)) {
            damaged = 1;
            continue;
        }
        if (!found || le32_to_cpu(d->version) > version) {
            found = 1;
            version = le32_to_cpu(d->version);
            *inode = le32_to_cpu(d->ino);
            type = d->type;
        }
    }
    if (!found && damaged) return -EIO;
    if (*inode && type != expected_type) return -EINVAL;
    return *inode ? 0 : -ENOENT;
}

static int valid_inode(const struct jffs2_unknown_node *n, u32 inode)
{
    const struct jffs2_raw_inode *f = (const void *)n;
    if (le16_to_cpu(n->nodetype) != JFFS2_NODETYPE_INODE ||
        le32_to_cpu(n->totlen) < sizeof(*f) || le32_to_cpu(f->ino) != inode)
        return 0;
    /* Linux discards torn nodes instead of letting an obsolete bad fragment
     * mask a newer valid file. Both index passes must use the same predicate. */
    return le32_to_cpu(f->node_crc) == jcrc(f, sizeof(*f) - 8) &&
           le32_to_cpu(f->csize) == le32_to_cpu(n->totlen) - sizeof(*f) &&
           le32_to_cpu(f->data_crc) == jcrc(f + 1, le32_to_cpu(f->csize));
}

static int fragment_compare(const void *a, const void *b)
{
    const struct fragment *x = a, *y = b;
    return x->version < y->version ? -1 : x->version > y->version;
}

static int inflate_fragment(const unsigned char *src, unsigned int length,
                            unsigned char *dst, unsigned int size, unsigned int type)
{
    if (type == JFFS2_COMPR_NONE || type == JFFS2_COMPR_COPY) {
        if (length != size)
            return -EINVAL;
        memcpy(dst, src, size);
        return 0;
    }
    if (type == JFFS2_COMPR_ZERO) {
        if (length)
            return -EINVAL;
        memset(dst, 0, size);
        return 0;
    }
    if (type == JFFS2_COMPR_RTIME) {
        unsigned int positions[256] = {0};
        unsigned int in = 0, out = 0;
        while (out < size) {
            unsigned int value, repeat, back;
            if (length - in < 2)
                return -EINVAL;
            value = src[in++];
            dst[out++] = value;
            repeat = src[in++];
            back = positions[value];
            positions[value] = out;
            if (repeat > size - out || (repeat && back >= out))
                return -EINVAL;
            while (repeat--)
                dst[out++] = dst[back++];
        }
        return in == length ? 0 : -EINVAL;
    }
    if (type == JFFS2_COMPR_ZLIB) {
        z_stream stream;
        int rc;
        memset(&stream, 0, sizeof(stream));
        stream.next_in = (unsigned char *)src;
        stream.avail_in = length;
        stream.next_out = dst;
        stream.avail_out = size;
        rc = inflateInit(&stream);
        if (rc != Z_OK)
            return -ENOMEM;
        rc = inflate(&stream, Z_FINISH);
        if (rc != Z_STREAM_END || stream.total_out != size || stream.avail_in)
            rc = -EINVAL;
        else
            rc = 0;
        inflateEnd(&stream);
        return rc;
    }
    return -ENOTSUPP;
}

int upgrade_jffs2_load(struct upgrade_jffs2 *fs, const char *name,
                    void **data, unsigned int *size, unsigned int limit)
{
    const struct jffs2_unknown_node *n;
    struct fragment *fragments = NULL;
    unsigned char *output = NULL, *decoded = NULL;
    unsigned int position = 0, count = 0, i = 0;
    u32 directory, inode, newest = 0, length = 0, current_length = 0;
    int rc;
    if (!fs || !name || !data || !size || !limit || limit > DATA_LIMIT) return -EINVAL;
    *data = NULL;
    *size = 0;
    rc = find_entry(fs, 1, "upgrade", DT_DIR, &directory);
    if (rc)
        return rc;
    rc = find_entry(fs, directory, name, DT_REG, &inode);
    if (rc)
        return rc;
    while ((n = next_node(fs, &position))) {
        const struct jffs2_raw_inode *f = (const void *)n;
        if (!valid_inode(n, inode))
            continue;
        if (!S_ISREG(le32_to_cpu(f->mode)) ||
            le32_to_cpu(f->dsize) > ERASE_SIZE || le32_to_cpu(f->isize) > limit ||
            le32_to_cpu(f->offset) > limit ||
            le32_to_cpu(f->dsize) > limit - le32_to_cpu(f->offset))
            return -EINVAL;
        if (++count > FRAGMENT_LIMIT)
            return -E2BIG;
        if (count == 1 || le32_to_cpu(f->version) >= newest) {
            newest = le32_to_cpu(f->version);
            length = le32_to_cpu(f->isize);
        }
    }
    if (!count || !length)
        return -EINVAL;
    fragments = malloc(count * sizeof(*fragments));
    output = memalign(ARCH_DMA_MINALIGN, ALIGN(length, 512));
    decoded = malloc(ERASE_SIZE);
    if (!fragments || !output || !decoded) {
        rc = -ENOMEM;
        goto out;
    }
    memset(output, 0, ALIGN(length, 512));
    position = 0;
    while ((n = next_node(fs, &position))) {
        const struct jffs2_raw_inode *f = (const void *)n;
        if (!valid_inode(n, inode))
            continue;
        fragments[i].node = f;
        fragments[i++].version = le32_to_cpu(f->version);
    }
    qsort(fragments, count, sizeof(*fragments), fragment_compare);
    for (i = 0; i < count; ++i) {
        const struct jffs2_raw_inode *f = fragments[i].node;
        unsigned int compressed = le32_to_cpu(f->csize);
        unsigned int expanded = le32_to_cpu(f->dsize);
        unsigned int offset = le32_to_cpu(f->offset);
        unsigned int inode_size = le32_to_cpu(f->isize);
        if (inode_size < current_length && inode_size < length)
            memset(output + inode_size, 0, min(current_length, length) - inode_size);
        current_length = inode_size;
        if (!expanded)
            continue;
        if (le32_to_cpu(f->data_crc) != jcrc(f + 1, compressed)) {
            rc = -EIO;
            goto out;
        }
        rc = inflate_fragment((const void *)(f + 1), compressed, decoded, expanded, f->compr);
        if (rc)
            goto out;
        if (offset < length)
            memcpy(output + offset, decoded, min(expanded, length - offset));
        WATCHDOG_RESET();
    }
    *data = output;
    *size = length;
    output = NULL;
    rc = 0;
out:
    free(decoded);
    free(output);
    free(fragments);
    return rc;
}
