/*
 * Copyright (C) 2022  Allwinnertech
 * Author: yajianz <yajianz@allwinnertech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "pqdata.h"
#include "config.h"
#include "debug.h"
#include "list.h"
#include "pqal.h"
#include "utils.h"

#define PQDATA_ENTRY_HEADER 0x50514852 /* "PQHR" */

struct pqdata_entry {
    int header;
    int port; /* which DE0/1 this data will apply to */
    enum pq_package_type type;
    int index; /* index of this data in the same type */
    int size;  /* size of the data */
    char data[0];
};

struct pqdata_binary {
    int header;
    int magic;          /* soc magic id */
    int version;        /* DE hardware version */
    int size;           /* total pqdata_entry  */
    char data[0];
};

/* list node struct add to pqdata_entry_list_ */
struct pqdata_entry_node {
    list_t link;
    struct pqdata_entry* entry;
};

static LIST_HEAD(pqdata_entry_list_);
static struct pqdata_binary binary_head_;

int pqdata_init(void)
{
    const struct hardware_data *hwdat = pq_hardware_data();

    binary_head_.header = PQDATA_ENTRY_HEADER;
    binary_head_.magic = hwdat->code;
    binary_head_.version = hwdat->version;

    dlogi("PQ config data init: code[%d] version[%d]", binary_head_.magic, binary_head_.version);
    return 0;
}

static struct pqdata_entry_node* pqdata_entry_node_create(int size)
{
    int es = sizeof(struct pqdata_entry) + size;
    struct pqdata_entry* entry = malloc(es);
    memset(entry, 0, es);
    entry->size = size;
    entry->header = PQDATA_ENTRY_HEADER;

    struct pqdata_entry_node* node = malloc(sizeof(*node));
    INIT_LIST_HEAD(&node->link);
    node->entry = entry;

    return node;
}

static void pqdata_entry_node_free(struct pqdata_entry_node* node)
{
    free(node->entry);
    free(node);
}

static inline void pqdata_entry_list_add(struct pqdata_entry_node* node)
{
    list_add(&node->link, &pqdata_entry_list_);
}

static inline void pqdata_entry_list_del(struct pqdata_entry_node* node)
{
    list_del(&node->link);
}

static inline int pqdata_entry_list_size()
{
    int count = 0;
    list_t *link;
    list_for_each(link, &pqdata_entry_list_) {
        count++;
    }
    return count;
}

static void pqdata_entry_copy(struct pqdata_entry *to, const struct pqdata_entry *from)
{
    to->header = from->header;
    to->port   = from->port;
    to->type   = from->type;
    to->index  = from->index;
    to->size   = from->size;
    memcpy(to->data, from->data, from->size);
}

/*
 * save pqdata list to file
 */

static inline int write_file_and_check(int fd, const char* buf, int size)
{
    int written;
retry:
    written = write(fd, buf, size);
    if (written != size) {
        if (written == -1 && errno == EAGAIN)
            goto retry;
    }
    fsync(fd);
    return 0;
}

static int pqdata_to_file(const char *fname)
{
    int ret = 0;
    int fd = open(fname, O_TRUNC | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd == -1)
        return -EINVAL;

    binary_head_.size = pqdata_entry_list_size();
    if (write_file_and_check(fd, (const char*)&binary_head_, sizeof(struct pqdata_binary)) != 0) {
        dloge("write pqdata header error!");
        ret = -1;
        goto out;
    }

    list_t *link;
    list_for_each(link, &pqdata_entry_list_) {
        struct pqdata_entry_node *node = list_entry(link, struct pqdata_entry_node, link);
        struct pqdata_entry *entry = node->entry;
        int size = entry->size + sizeof(struct pqdata_entry);

        if (write_file_and_check(fd, (const char*)entry, size) != 0) {
            dloge("write pqdata error!!!");
            ret = -1;
            break;
        }
    }

out:
    close(fd);
    return ret;
}

/*
 * parse pqdata_entry from file, and add it to pqdata_entry_list_ .
 * return total entry count.
 */
static int pqdata_from_file(const char* fname)
{
    struct stat fstat;
    if (stat(fname, &fstat) < 0)
        return -EINVAL;
    int fd = open(fname, O_RDONLY, 0);
    if (fd == -1)
        return -EINVAL;

    const struct pqdata_binary* bin =
        mmap(0, fstat.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (bin == MAP_FAILED) {
        close(fd);
        return -1;
    }

    if (bin->header != PQDATA_ENTRY_HEADER) {
        munmap((void*)bin, fstat.st_size);
        close(fd);
        return -1;
    }

    if (bin->magic != binary_head_.magic) {
        munmap((void*)bin, fstat.st_size);
        close(fd);
        dloge("'%s' magic[%d] not match the current target[%d]",
                fname, binary_head_.magic, bin->magic);
        return -1;
    }

    if (bin->version != binary_head_.version) {
        munmap((void*)bin, fstat.st_size);
        close(fd);
        dloge("'%s' version[%d] not match the current target[%d]",
                fname, binary_head_.version, bin->version);
        return -1;
    }

    int entry_count = 0;
    const struct pqdata_entry* entry = (const struct pqdata_entry *)bin->data;

    while ((entry_count < bin->size) &&
            entry->header == PQDATA_ENTRY_HEADER) {
        struct pqdata_entry_node *node = pqdata_entry_node_create(entry->size);
        pqdata_entry_copy(node->entry, entry);
        pqdata_entry_list_add(node);

        dlogv("load config packet port[%d] type[%d] index[%d] size=%d",
                entry->port, entry->type, entry->index, entry->size);

        entry = (struct pqdata_entry*)(entry->data + entry->size);
        entry_count++;
    }

    munmap((void*)bin, fstat.st_size);
    close(fd);
    return entry_count;
}

int pqdata_load(struct pqdata **out)
{
    /* load pqdata from file on the first time */
    if (list_empty(&pqdata_entry_list_)) {
        pqdata_from_file(PQDATA_FILE_NAME);
    }

    int count = pqdata_entry_list_size();
    if (count > 0) {
        struct pqdata* result = malloc(sizeof(struct pqdata) * count);
        struct pqdata* pds = result;
        list_t *link;
        list_for_each(link, &pqdata_entry_list_) {
            struct pqdata_entry_node *node =  list_entry(link, struct pqdata_entry_node, link);
            pds->port  = node->entry->port;
            pds->type  = node->entry->type;
            pds->index = node->entry->index;
            pds->size  = node->entry->size;
            pds->data  = node->entry->data;
            pds++;
        }

        *out = result;
    }

    return count;
}

void pqdata_free(struct pqdata *p) { free(p); }

static struct pqdata_entry_node* pqdata_find_node(int port, enum pq_package_type type, int index)
{
    struct pqdata_entry_node *found = NULL;
    list_t *link;

    list_for_each(link, &pqdata_entry_list_){
        struct pqdata_entry_node *node = list_entry(link, struct pqdata_entry_node, link);
        struct pqdata_entry *entry = node->entry;
        if (entry->type == type && entry->index == index && entry->port == port) {
            found = node;
            break;
        }
    }

    return found;
}

static void pqdata_insert(struct pqdata_entry_node *node)
{
    list_t *link;
    struct pqdata_entry_node *found = NULL;
    enum pq_package_type type = node->entry->type;
    int port = node->entry->port;

    /* we want to save all the same port and type data one by one */
    list_for_each(link, &pqdata_entry_list_){
        struct pqdata_entry_node *lnode = list_entry(link, struct pqdata_entry_node, link);
        struct pqdata_entry *entry = lnode->entry;
        if (entry->type == type && entry->port == port) {
            found = lnode;
            break;
        }
    }

    list_add(&node->link, found ? &found->link : &pqdata_entry_list_);
}

int pqdata_save(int port, enum pq_package_type type, int index, const void* data, int size)
{
    struct pqdata_entry_node* node = pqdata_find_node(port, type, index);

    if (node) {
        /* delete the old data */
        pqdata_entry_list_del(node);
        pqdata_entry_node_free(node);
    }

    struct pqdata_entry_node* newdat = pqdata_entry_node_create(size);
    memcpy(newdat->entry->data, data, size);
    newdat->entry->port  = port;
    newdat->entry->type  = type;
    newdat->entry->index = index;

    pqdata_insert(newdat);

    return 0;
}

int pqdata_find(int port, enum pq_package_type type, int index, const void** out, int* len)
{
    struct pqdata_entry_node* node = pqdata_find_node(port, type, index);

    if (node) {
        *out = node->entry->data;
        *len = node->entry->size;
        return 0;
    }
    return -1;
}

int pqdata_store_to_file(void)
{
    dlogi("save pq config data [%s]", PQDATA_FILE_NAME);
    return pqdata_to_file(PQDATA_FILE_NAME);
}

