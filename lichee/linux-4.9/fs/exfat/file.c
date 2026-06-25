// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2012-2013 Samsung Electronics Co., Ltd.
 */

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/cred.h>
#endif
#include <linux/compat.h>
#include <linux/blkdev.h>
#include <linux/falloc.h>

#include "exfat_raw.h"
#include "exfat_fs.h"

static int exfat_cont_expand(struct inode *inode, loff_t size)
{
	struct address_space *mapping = inode->i_mapping;
	loff_t start = i_size_read(inode), count = size - i_size_read(inode);
	int err, err2;

	err = generic_cont_expand_simple(inode, size);
	if (err)
		return err;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	inode->i_ctime = inode->i_mtime = current_time(inode);
#else
	inode->i_ctime = inode->i_mtime = CURRENT_TIME_SEC;
#endif
	mark_inode_dirty(inode);

	if (!IS_SYNC(inode))
		return 0;

	err = filemap_fdatawrite_range(mapping, start, start + count - 1);
	err2 = sync_mapping_buffers(mapping);
	if (!err)
		err = err2;
	err2 = write_inode_now(inode, 1);
	if (!err)
		err = err2;
	if (err)
		return err;

	return filemap_fdatawait_range(mapping, start, start + count - 1);
}

static bool exfat_allow_set_time(struct exfat_sb_info *sbi, struct inode *inode)
{
	mode_t allow_utime = sbi->options.allow_utime;

	if (!uid_eq(current_fsuid(), inode->i_uid)) {
		if (in_group_p(inode->i_gid))
			allow_utime >>= 3;
		if (allow_utime & MAY_WRITE)
			return true;
	}

	/* use a default check */
	return false;
}

static int exfat_sanitize_mode(const struct exfat_sb_info *sbi,
		struct inode *inode, umode_t *mode_ptr)
{
	mode_t i_mode, mask, perm;

	i_mode = inode->i_mode;

	mask = (S_ISREG(i_mode) || S_ISLNK(i_mode)) ?
		sbi->options.fs_fmask : sbi->options.fs_dmask;
	perm = *mode_ptr & ~(S_IFMT | mask);

	/* Of the r and x bits, all (subject to umask) must be present.*/
	if ((perm & 0555) != (i_mode & 0555))
		return -EPERM;

	if (exfat_mode_can_hold_ro(inode)) {
		/*
		 * Of the w bits, either all (subject to umask) or none must
		 * be present.
		 */
		if ((perm & 0222) && ((perm & 0222) != (0222 & ~mask)))
			return -EPERM;
	} else {
		/*
		 * If exfat_mode_can_hold_ro(inode) is false, can't change
		 * w bits.
		 */
		if ((perm & 0222) != (0222 & ~mask))
			return -EPERM;
	}

	*mode_ptr &= S_IFMT | perm;

	return 0;
}

/* resize the file length */
int __exfat_truncate(struct inode *inode, loff_t new_size)
{
	unsigned int num_clusters_new, num_clusters_phys;
	unsigned int last_clu = EXFAT_FREE_CLUSTER;
	struct exfat_chain clu;
	struct super_block *sb = inode->i_sb;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct exfat_inode_info *ei = EXFAT_I(inode);

	/* check if the given file ID is opened */
	if (ei->type != TYPE_FILE && ei->type != TYPE_DIR)
		return -EPERM;

	exfat_set_volume_dirty(sb);

	num_clusters_new = EXFAT_B_TO_CLU_ROUND_UP(i_size_read(inode), sbi);
	num_clusters_phys = EXFAT_B_TO_CLU_ROUND_UP(ei->i_size_ondisk, sbi);

	exfat_chain_set(&clu, ei->start_clu, num_clusters_phys, ei->flags);

	if (new_size > 0) {
		/*
		 * Truncate FAT chain num_clusters after the first cluster
		 * num_clusters = min(new, phys);
		 */
		unsigned int num_clusters =
			min(num_clusters_new, num_clusters_phys);

		/*
		 * Follow FAT chain
		 * (defensive coding - works fine even with corrupted FAT table
		 */
		if (clu.flags == ALLOC_NO_FAT_CHAIN) {
			clu.dir += num_clusters;
			clu.size -= num_clusters;
		} else {
			while (num_clusters > 0) {
				last_clu = clu.dir;
				if (exfat_get_next_cluster(sb, &(clu.dir)))
					return -EIO;

				num_clusters--;
				clu.size--;
			}
		}
	} else {
		ei->flags = ALLOC_NO_FAT_CHAIN;
		ei->start_clu = EXFAT_EOF_CLUSTER;
	}

	i_size_write(inode, new_size);

	if (ei->type == TYPE_FILE)
		ei->attr |= ATTR_ARCHIVE;

	/*
	 * update the directory entry
	 *
	 * If the directory entry is updated by mark_inode_dirty(), the
	 * directory entry will be written after a writeback cycle of
	 * updating the bitmap/FAT, which may result in clusters being
	 * freed but referenced by the directory entry in the event of a
	 * sudden power failure.
	 * __exfat_write_inode() is called for directory entry, bitmap
	 * and FAT to be written in a same writeback.
	 */
	if (__exfat_write_inode(inode, inode_needs_sync(inode)))
		return -EIO;

	/* cut off from the FAT chain */
	if (ei->flags == ALLOC_FAT_CHAIN && last_clu != EXFAT_FREE_CLUSTER &&
			last_clu != EXFAT_EOF_CLUSTER) {
		if (exfat_ent_set(sb, last_clu, EXFAT_EOF_CLUSTER))
			return -EIO;
	}

	/* invalidate cache and free the clusters */
	/* clear exfat cache */
	exfat_cache_inval_inode(inode);

	/* hint information */
	ei->hint_bmap.off = EXFAT_EOF_CLUSTER;
	ei->hint_bmap.clu = EXFAT_EOF_CLUSTER;

	/* hint_stat will be used if this is directory. */
	ei->hint_stat.eidx = 0;
	ei->hint_stat.clu = ei->start_clu;
	ei->hint_femp.eidx = EXFAT_HINT_NONE;

	/* free the clusters */
	if (exfat_free_cluster(inode, &clu))
		return -EIO;

	return 0;
}

void exfat_truncate(struct inode *inode, loff_t size)
{
	struct super_block *sb = inode->i_sb;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	struct exfat_inode_info *ei = EXFAT_I(inode);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 72)
	unsigned int blocksize = i_blocksize(inode);
#else
	unsigned int blocksize = 1 << inode->i_blkbits;
#endif
	loff_t aligned_size;
	int err;

	mutex_lock(&sbi->s_lock);

	if (exfat_check_prealloc(inode)) {
		i_size_write(inode, size);
		if (__exfat_write_inode(inode, inode_needs_sync(inode))) {
			exfat_fs_error_ratelimit(sb, "%s failed to write turncate inode", __func__);
		}
		mutex_unlock(&sbi->s_lock);
		return ;
	}

	if (ei->start_clu == 0) {
		/*
		 * Empty start_clu != ~0 (not allocated)
		 */
		exfat_fs_error(sb, "tried to truncate zeroed cluster.");
		goto write_size;
	}

	err = __exfat_truncate(inode, i_size_read(inode));
	if (err)
		goto write_size;

	inode->i_blocks = round_up(i_size_read(inode), sbi->cluster_size) >>
				inode->i_blkbits;
write_size:
	aligned_size = i_size_read(inode);
	if (aligned_size & (blocksize - 1)) {
		aligned_size |= (blocksize - 1);
		aligned_size++;
	}

	if (ei->i_size_ondisk > i_size_read(inode))
		ei->i_size_ondisk = aligned_size;

	if (ei->i_size_aligned > i_size_read(inode))
		ei->i_size_aligned = aligned_size;

	mutex_unlock(&sbi->s_lock);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
int exfat_getattr(struct user_namespace *mnt_uerns, const struct path *path,
		  struct kstat *stat, unsigned int request_mask,
		  unsigned int query_flags)
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
int exfat_getattr(const struct path *path, struct kstat *stat,
		unsigned int request_mask, unsigned int query_flags)
#else
int exfat_getattr(struct vfsmount *mnt, struct dentry *dentry,
		struct kstat *stat)
#endif
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	struct inode *inode = d_backing_inode(path->dentry);
	struct exfat_inode_info *ei = EXFAT_I(inode);
#else
	struct inode *inode = d_inode(dentry);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	generic_fillattr(&init_user_ns, inode, stat);
#else
	generic_fillattr(inode, stat);
#endif
	exfat_truncate_atime(&stat->atime);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	stat->result_mask |= STATX_BTIME;
	stat->btime.tv_sec = ei->i_crtime.tv_sec;
	stat->btime.tv_nsec = ei->i_crtime.tv_nsec;
#endif
	stat->blksize = EXFAT_SB(inode->i_sb)->cluster_size;
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
int exfat_setattr(struct user_namespace *mnt_userns, struct dentry *dentry,
		struct iattr *attr)
#else
int exfat_setattr(struct dentry *dentry, struct iattr *attr)
#endif
{
	struct exfat_sb_info *sbi = EXFAT_SB(dentry->d_sb);
	struct inode *inode = dentry->d_inode;
	unsigned int ia_valid;
	int error;

	if ((attr->ia_valid & ATTR_SIZE) &&
	    attr->ia_size > i_size_read(inode)) {
		error = exfat_cont_expand(inode, attr->ia_size);
		if (error || attr->ia_valid == ATTR_SIZE)
			return error;
		attr->ia_valid &= ~ATTR_SIZE;
	}

	/* Check for setting the inode time. */
	ia_valid = attr->ia_valid;
	if ((ia_valid & (ATTR_MTIME_SET | ATTR_ATIME_SET | ATTR_TIMES_SET)) &&
	    exfat_allow_set_time(sbi, inode)) {
		attr->ia_valid &= ~(ATTR_MTIME_SET | ATTR_ATIME_SET |
				ATTR_TIMES_SET);
	}

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)) && \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 37))) || \
		(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	error = setattr_prepare(&init_user_ns, dentry, attr);
#else
	error = setattr_prepare(dentry, attr);
#endif
#else
	error = inode_change_ok(inode, attr);
#endif
	attr->ia_valid = ia_valid;
	if (error)
		goto out;

	if (((attr->ia_valid & ATTR_UID) &&
	     !uid_eq(attr->ia_uid, sbi->options.fs_uid)) ||
	    ((attr->ia_valid & ATTR_GID) &&
	     !gid_eq(attr->ia_gid, sbi->options.fs_gid)) ||
	    ((attr->ia_valid & ATTR_MODE) &&
	     (attr->ia_mode & ~(S_IFREG | S_IFLNK | S_IFDIR | 0777)))) {
		error = -EPERM;
		goto out;
	}

	/*
	 * We don't return -EPERM here. Yes, strange, but this is too
	 * old behavior.
	 */
	if (attr->ia_valid & ATTR_MODE) {
		if (exfat_sanitize_mode(sbi, inode, &attr->ia_mode) < 0)
			attr->ia_valid &= ~ATTR_MODE;
	}

	if (attr->ia_valid & ATTR_SIZE)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
		inode->i_mtime = inode->i_ctime = current_time(inode);
#else
		inode->i_mtime = inode->i_ctime = CURRENT_TIME_SEC;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	setattr_copy(&init_user_ns, inode, attr);
#else
	setattr_copy(inode, attr);
#endif
	exfat_truncate_atime(&inode->i_atime);

	if (attr->ia_valid & ATTR_SIZE) {
		error = exfat_block_truncate_page(inode, attr->ia_size);
		if (error)
			goto out;

		down_write(&EXFAT_I(inode)->truncate_lock);
		truncate_setsize(inode, attr->ia_size);
		/*
		 * __exfat_write_inode() is called from exfat_truncate(), inode
		 * is already written by it, so mark_inode_dirty() is unneeded.
		 */
		exfat_truncate(inode, attr->ia_size);
		up_write(&EXFAT_I(inode)->truncate_lock);
	} else
		mark_inode_dirty(inode);
out:
	return error;
}

static int exfat_ioctl_fitrim(struct inode *inode, unsigned long arg)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0)
	struct request_queue *q = bdev_get_queue(inode->i_sb->s_bdev);
#endif
	struct fstrim_range range;
	int ret = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
	if (!bdev_max_discard_sectors(inode->i_sb->s_bdev))
#else
	if (!blk_queue_discard(q))
#endif
		return -EOPNOTSUPP;

	if (copy_from_user(&range, (struct fstrim_range __user *)arg, sizeof(range)))
		return -EFAULT;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
	range.minlen = max_t(unsigned int, range.minlen,
				bdev_discard_granularity(inode->i_sb->s_bdev));
#else
	range.minlen = max_t(unsigned int, range.minlen,
				q->limits.discard_granularity);
#endif

	ret = exfat_trim_fs(inode, &range);
	if (ret < 0)
		return ret;

	if (copy_to_user((struct fstrim_range __user *)arg, &range, sizeof(range)))
		return -EFAULT;

	return 0;
}

long exfat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct inode *inode = file_inode(filp);

	switch (cmd) {
	case FITRIM:
		return exfat_ioctl_fitrim(inode, arg);
	default:
		return -ENOTTY;
	}
}

#ifdef CONFIG_COMPAT
long exfat_compat_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	return exfat_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

int exfat_file_fsync(struct file *filp, loff_t start, loff_t end, int datasync)
{
	struct inode *inode = filp->f_mapping->host;
	int err;

	err = __generic_file_fsync(filp, start, end, datasync);
	if (err)
		return err;

	err = sync_blockdev(inode->i_sb->s_bdev);
	if (err)
		return err;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	return blkdev_issue_flush(inode->i_sb->s_bdev);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
	return blkdev_issue_flush(inode->i_sb->s_bdev, GFP_KERNEL);
#else
	return blkdev_issue_flush(inode->i_sb->s_bdev, GFP_KERNEL, NULL);
#endif
#endif
}

static long exfat_fallocate(struct file *file, int mode,
				loff_t offset, loff_t len)
{
	struct inode *inode = file->f_mapping->host;
	struct exfat_inode_info *ei = EXFAT_I(inode);
	struct super_block *sb = inode->i_sb;
	struct exfat_sb_info *sbi = EXFAT_SB(sb);
	int err = 0, create = 1;
	unsigned int cluster;
	sector_t iblock;
	sector_t last_block;
	pgoff_t start, end, index;
	loff_t pos;
	const unsigned blkbits = inode->i_blkbits;
	unsigned int clu;

	/* No support for hole punch or other fallocate flags. */
	if (mode & ~FALLOC_FL_KEEP_SIZE)
		return -EOPNOTSUPP;

	/* No support for dir */
	if (!S_ISREG(inode->i_mode))
		return -EOPNOTSUPP;

	if (exfat_check_prealloc(inode)) {
		exfat_fs_error_ratelimit(sb, KERN_ERR, "Repeatedly called exfat_fallocate");
		return -EOPNOTSUPP;
	}

	start = offset >> PAGE_SHIFT;
	end = (offset + len + PAGE_SIZE - 1) >> PAGE_SHIFT;

	mutex_lock(&sbi->s_lock);
	last_block = EXFAT_B_TO_BLK_ROUND_UP(i_size_read(inode), sb);
	if (mode & FALLOC_FL_KEEP_SIZE) {
		for (index = start; index < end; index++) {
			iblock = index << (PAGE_SHIFT - blkbits);
			/* Is this block already allocated? */
			err = exfat_map_new_cluster(inode, iblock >> sbi->sect_per_clus_bits,
						&cluster);
			if (err) {
				if (err != -ENOSPC)
					exfat_fs_error_ratelimit(sb,
						"failed to bmap (inode : %p iblock : %llu, err : %d)",
						inode, (unsigned long long)iblock, err);
				goto unlock_ret;
			}

			//pr_err("start_clu:%u--clu:%u--flag:%d\n", ei->start_clu, cluster, ei->flags);
			/* Treat newly added block / cluster */
			if (iblock < last_block)
				create = 0;

			/*
			* Adjust i_size_aligned if i_size_ondisk is bigger than it.
			*/
			if (create) {
				pos = EXFAT_BLK_TO_B((iblock + 1), sb);
				if (ei->i_size_ondisk < pos)
					ei->i_size_ondisk = pos;
			}
			if (ei->i_size_ondisk > ei->i_size_aligned)
				ei->i_size_aligned = ei->i_size_ondisk;

		}

		clu = ei->start_clu;
		ei->end_clu = cluster;
		//ei->end_clu = ei->start_clu + EXFAT_B_TO_CLU_ROUND_UP(((end - start) << PAGE_SHIFT), sbi) - 1;
#if 0
		while (clu !=  EXFAT_EOF_CLUSTER)
		if (exfat_ent_get(inode->i_sb, clu, &clu)) {
			pr_err("fallocate_clu:%u\n", clu);
		}
#endif

		//pr_err("end_clu:%u--clu:%u--flag:%d\n", ei->start_clu, ei->end_clu, ei->flags);
	} else {
		exfat_fs_error_ratelimit(sb, "current fallocate only support FALLOC_FL_KEEP_SIZE:%d mode", FALLOC_FL_KEEP_SIZE);
	}

	exfat_mark_prealloc(inode);
	mark_inode_dirty(inode);

unlock_ret:
	mutex_unlock(&sbi->s_lock);

	return err;
}

const struct file_operations exfat_file_operations = {
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.unlocked_ioctl = exfat_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = exfat_compat_ioctl,
#endif
	.mmap		= generic_file_mmap,
	.fsync		= exfat_file_fsync,
	.splice_read	= generic_file_splice_read,
	.splice_write	= iter_file_splice_write,
       .fallocate      = exfat_fallocate,
};

const struct inode_operations exfat_file_inode_operations = {
	.setattr     = exfat_setattr,
	.getattr     = exfat_getattr,
};
