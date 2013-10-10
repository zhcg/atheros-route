/*
 * High-level sync()-related operations
 */

#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/syscalls.h>
#include <linux/linkage.h>
#include <linux/pagemap.h>
#include <linux/quotaops.h>
#include <linux/buffer_head.h>

/*
 * sync everything.  Start out by waking pdflush, because that writes back
 * all queues in parallel.
 */
static void do_sync(unsigned long wait)
{
	wakeup_pdflush(0);
	sync_inodes(0);		/* All mappings, inodes and their blockdevs */
	DQUOT_SYNC(NULL);
	sync_supers();		/* Write the superblocks */
	sync_filesystems(0);	/* Start syncing the filesystems */
	sync_filesystems(wait);	/* Waitingly sync the filesystems */
	sync_inodes(wait);	/* Mappings, inodes and blockdevs, again. */
	if (!wait)
		printk("Emergency Sync complete\n");
	if (unlikely(laptop_mode))
		laptop_sync_completion();
}

asmlinkage long sys_sync(void)
{
	do_sync(1);
	return 0;
}

void emergency_sync(void)
{
	pdflush_operation(do_sync, 0);
}

/*
 * Generic function to fsync a file.
 *
 * filp may be NULL if called via the msync of a vma.
 */
 
int file_fsync(struct file *filp, struct dentry *dentry, int datasync)
{
	struct inode * inode = dentry->d_inode;
	struct super_block * sb;
	int ret, err;

	/* sync the inode to buffers */
	ret = write_inode_now(inode, 0);

	/* sync the superblock to buffers */
	sb = inode->i_sb;
	lock_super(sb);
	if (sb->s_op->write_super)
		sb->s_op->write_super(sb);
	unlock_super(sb);

	/* .. finally sync the buffers to disk */
	err = sync_blockdev(sb->s_bdev);
	if (!ret)
		ret = err;
	return ret;
}

static long do_fsync(unsigned int fd, int datasync)
{
	struct file * file;
	struct address_space *mapping;
	int ret, err;

	ret = -EBADF;
	file = fget(fd);
	if (!file)
		goto out;

	ret = -EINVAL;
	if (!file->f_op || !file->f_op->fsync) {
		/* Why?  We can still call filemap_fdatawrite */
		goto out_putf;
	}

	mapping = file->f_mapping;

	current->flags |= PF_SYNCWRITE;
	ret = filemap_fdatawrite(mapping);

	/*
	 * We need to protect against concurrent writers,
	 * which could cause livelocks in fsync_buffers_list
	 */
	down(&mapping->host->i_sem);
	err = file->f_op->fsync(file, file->f_dentry, datasync);
	if (!ret)
		ret = err;
	up(&mapping->host->i_sem);
	err = filemap_fdatawait(mapping);
	if (!ret)
		ret = err;
	current->flags &= ~PF_SYNCWRITE;

out_putf:
	fput(file);
out:
	return ret;
}

asmlinkage long sys_fsync(unsigned int fd)
{
	return do_fsync(fd, 0);
}

asmlinkage long sys_fdatasync(unsigned int fd)
{
	return do_fsync(fd, 1);
}
