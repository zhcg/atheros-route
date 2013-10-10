/*
 * Squashfs - a compressed read only filesystem for Linux
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008
 * Phillip Lougher <phillip@lougher.demon.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * super.c
 */

/*
 * This file implements code to read the superblock, read and initialise
 * in-memory structures at mount time, and all the VFS glue code to register
 * the filesystem.
 */

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/mutex.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/zlib.h>
#include <linux/magic.h>

#include <crypto/compress.h>

#include <net/netlink.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs_fs_i.h"
#include "squashfs.h"


static int squashfs_setup_zlib(struct squashfs_sb_info *msblk)
{
	int err = -EOPNOTSUPP;

#ifdef CONFIG_SQUASHFS_SUPPORT_ZLIB
	struct {
		struct nlattr nla;
		int val;
	} params = {
		.nla = {
			.nla_len	= nla_attr_size(sizeof(int)),
			.nla_type	= ZLIB_DECOMP_WINDOWBITS,
		},
		.val			= DEF_WBITS,
	};

	msblk->tfm = crypto_alloc_pcomp("zlib", 0,
					CRYPTO_ALG_ASYNC);
	if (IS_ERR(msblk->tfm)) {
		ERROR("Failed to load zlib crypto module\n");
		return PTR_ERR(msblk->tfm);
	}

	err = crypto_decompress_setup(msblk->tfm, &params, sizeof(params));
	if (err) {
		ERROR("Failed to set up decompression parameters\n");
		crypto_free_pcomp(msblk->tfm);
	}
#endif

	return err;
}

static int squashfs_setup_lzma(struct squashfs_sb_info *msblk)
{
	int err = -EOPNOTSUPP;

#ifdef CONFIG_SQUASHFS_SUPPORT_LZMA
	struct {
		struct nlattr nla;
		int val;
	} params = {
		.nla = {
			.nla_len	= nla_attr_size(sizeof(int)),
			.nla_type	= UNLZMA_DECOMP_OUT_BUFFERS,
		},
		.val = (msblk->block_size / PAGE_CACHE_SIZE) + 1
	};

	msblk->tfm = crypto_alloc_pcomp("lzma", 0,
					CRYPTO_ALG_ASYNC);
	if (IS_ERR(msblk->tfm)) {
		ERROR("Failed to load lzma crypto module\n");
		return PTR_ERR(msblk->tfm);
	}

	err = crypto_decompress_setup(msblk->tfm, &params, sizeof(params));
	if (err) {
		ERROR("Failed to set up decompression parameters\n");
		crypto_free_pcomp(msblk->tfm);
	}
#endif

	return err;
}

static struct file_system_type squashfs_fs_type;
static struct super_operations squashfs_super_ops;

static int supported_squashfs_filesystem(short major, short minor)
{
	if (major < SQUASHFS_MAJOR) {
		ERROR("Major/Minor mismatch, older Squashfs %d.%d "
			"filesystems are unsupported\n", major, minor);
		return -EINVAL;
	} else if (major > SQUASHFS_MAJOR || minor > SQUASHFS_MINOR) {
		ERROR("Major/Minor mismatch, trying to mount newer "
			"%d.%d filesystem\n", major, minor);
		ERROR("Please update your kernel\n");
		return -EINVAL;
	}

	return 0;
}


static int squashfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct squashfs_sb_info *msblk;
	struct squashfs_super_block *sblk = NULL;
#ifdef CONFIG_BLOCK
	char b[BDEVNAME_SIZE];
#endif
	struct inode *root;
	long long root_inode;
	unsigned short flags;
	unsigned int fragments;
	u64 lookup_table_start;
	int err;

	TRACE("Entered squashfs_fill_superblock\n");

#if !defined(CONFIG_SQUASHFS_MTD)
	sb->s_fs_info = kzalloc(sizeof(*msblk), GFP_KERNEL);
	if (sb->s_fs_info == NULL) {
		ERROR("Failed to allocate squashfs_sb_info\n");
		return -ENOMEM;
	}
#endif
	msblk = sb->s_fs_info;

	sblk = kzalloc(sizeof(*sblk), GFP_KERNEL);
	if (sblk == NULL) {
		ERROR("Failed to allocate squashfs_super_block\n");
		err = -ENOMEM;
		goto failure;
	}

#ifdef CONFIG_SQUASHFS_MTD
	msblk->devblksize = 1024;
#else
	msblk->devblksize = sb_min_blocksize(sb, BLOCK_SIZE);
#endif
	msblk->devblksize_log2 = ffz(~msblk->devblksize);

	mutex_init(&msblk->read_data_mutex);
	mutex_init(&msblk->meta_index_mutex);

	/*
	 * msblk->bytes_used is checked in squashfs_read_table to ensure reads
	 * are not beyond filesystem end.  But as we're using
	 * squashfs_read_table here to read the superblock (including the value
	 * of bytes_used) we need to set it to an initial sensible dummy value
	 */
	msblk->bytes_used = sizeof(*sblk);
	err = squashfs_read_table(sb, sblk, SQUASHFS_START, sizeof(*sblk));

	if (err < 0) {
		ERROR("unable to read squashfs_super_block\n");
		goto failed_mount;
	}

	/* Check it is a SQUASHFS superblock */
	sb->s_magic = le32_to_cpu(sblk->s_magic);
	if (sb->s_magic != SQUASHFS_MAGIC) {
		if (!silent) {
#ifdef CONFIG_BLOCK
			ERROR("Can't find a SQUASHFS superblock on %s\n",
						bdevname(sb->s_bdev, b));
#elif defined CONFIG_SQUASHFS_MTD
			ERROR("Can't find a SQUASHFS superblock on %s\n",
						sb->s_mtd->name);
#endif
		}
		err = -EINVAL;
		goto failed_mount;
	}

	/* Check block size for sanity */
	msblk->block_size = le32_to_cpu(sblk->block_size);
	if (msblk->block_size > SQUASHFS_FILE_MAX_SIZE)
		goto failed_mount;

	/* Check the MAJOR & MINOR versions and compression type */
	err = supported_squashfs_filesystem(le16_to_cpu(sblk->s_major),
			le16_to_cpu(sblk->s_minor));
	if (err < 0)
		goto failed_mount;

	switch(le16_to_cpu(sblk->compression)) {
	case ZLIB_COMPRESSION:
		err = squashfs_setup_zlib(msblk);
		break;
	case LZMA_COMPRESSION:
		err = squashfs_setup_lzma(msblk);
		break;
	default:
		err = -EINVAL;
		break;
	}
	if (err < 0)
		goto failed_mount;

	err = -EINVAL;

	/*
	 * Check if there's xattrs in the filesystem.  These are not
	 * supported in this version, so warn that they will be ignored.
	 */
	if (le64_to_cpu(sblk->xattr_table_start) != SQUASHFS_INVALID_BLK)
		ERROR("Xattrs in filesystem, these will be ignored\n");

	/* Check the filesystem does not extend beyond the end of the
	   block device */
	msblk->bytes_used = le64_to_cpu(sblk->bytes_used);

#ifdef CONFIG_SQUASHFS_MTD
        if(msblk->bytes_used < 0  || msblk->bytes_used > msblk->mtd->size)
#else
	if (msblk->bytes_used < 0 || msblk->bytes_used >
			i_size_read(sb->s_bdev->bd_inode))
#endif
		goto failed_mount;

	/*
	 * Check the system page size is not larger than the filesystem
	 * block size (by default 128K).  This is currently not supported.
	 */
	if (PAGE_CACHE_SIZE > msblk->block_size) {
		ERROR("Page size > filesystem block size (%d).  This is "
			"currently not supported!\n", msblk->block_size);
		goto failed_mount;
	}

	msblk->block_log = le16_to_cpu(sblk->block_log);
	if (msblk->block_log > SQUASHFS_FILE_MAX_LOG)
		goto failed_mount;

	/* Check the root inode for sanity */
	root_inode = le64_to_cpu(sblk->root_inode);
	if (SQUASHFS_INODE_OFFSET(root_inode) > SQUASHFS_METADATA_SIZE)
		goto failed_mount;

	msblk->inode_table = le64_to_cpu(sblk->inode_table_start);
	msblk->directory_table = le64_to_cpu(sblk->directory_table_start);
	msblk->inodes = le32_to_cpu(sblk->inodes);
	flags = le16_to_cpu(sblk->flags);

#ifdef CONFIG_BLOCK
	TRACE("Found valid superblock on %s\n", bdevname(sb->s_bdev, b));
#elif defined CONFIG_SQUASHSFS_MTD
	TRACE("Found valid superblock on %s\n", sb->s_mtd->name);
#endif
	TRACE("Inodes are %scompressed\n", SQUASHFS_UNCOMPRESSED_INODES(flags)
				? "un" : "");
	TRACE("Data is %scompressed\n", SQUASHFS_UNCOMPRESSED_DATA(flags)
				? "un" : "");
	TRACE("Filesystem size %lld bytes\n", msblk->bytes_used);
	TRACE("Block size %d\n", msblk->block_size);
	TRACE("Number of inodes %d\n", msblk->inodes);
	TRACE("Number of fragments %d\n", le32_to_cpu(sblk->fragments));
	TRACE("Number of ids %d\n", le16_to_cpu(sblk->no_ids));
	TRACE("sblk->inode_table_start %llx\n", msblk->inode_table);
	TRACE("sblk->directory_table_start %llx\n", msblk->directory_table);
	TRACE("sblk->fragment_table_start %llx\n",
		(u64) le64_to_cpu(sblk->fragment_table_start));
	TRACE("sblk->id_table_start %llx\n",
		(u64) le64_to_cpu(sblk->id_table_start));

	sb->s_maxbytes = MAX_LFS_FILESIZE;
	sb->s_flags |= MS_RDONLY;
	sb->s_op = &squashfs_super_ops;

	err = -ENOMEM;

	msblk->block_cache = squashfs_cache_init("metadata",
			SQUASHFS_CACHED_BLKS, SQUASHFS_METADATA_SIZE);
	if (msblk->block_cache == NULL)
		goto failed_mount;

	/* Allocate read_page block */
	msblk->read_page = squashfs_cache_init("data", 1, msblk->block_size);
	if (msblk->read_page == NULL) {
		ERROR("Failed to allocate read_page block\n");
		goto failed_mount;
	}

	/* Allocate and read id index table */
	msblk->id_table = squashfs_read_id_index_table(sb,
		le64_to_cpu(sblk->id_table_start), le16_to_cpu(sblk->no_ids));
	if (IS_ERR(msblk->id_table)) {
		err = PTR_ERR(msblk->id_table);
		msblk->id_table = NULL;
		goto failed_mount;
	}

	fragments = le32_to_cpu(sblk->fragments);
	if (fragments == 0)
		goto allocate_lookup_table;

	msblk->fragment_cache = squashfs_cache_init("fragment",
		SQUASHFS_CACHED_FRAGMENTS, msblk->block_size);
	if (msblk->fragment_cache == NULL) {
		err = -ENOMEM;
		goto failed_mount;
	}

	/* Allocate and read fragment index table */
	msblk->fragment_index = squashfs_read_fragment_index_table(sb,
		le64_to_cpu(sblk->fragment_table_start), fragments);
	if (IS_ERR(msblk->fragment_index)) {
		err = PTR_ERR(msblk->fragment_index);
		msblk->fragment_index = NULL;
		goto failed_mount;
	}

allocate_lookup_table:
	lookup_table_start = le64_to_cpu(sblk->lookup_table_start);
	if (lookup_table_start == SQUASHFS_INVALID_BLK)
		goto allocate_root;

	/* Allocate and read inode lookup table */
	msblk->inode_lookup_table = squashfs_read_inode_lookup_table(sb,
		lookup_table_start, msblk->inodes);
	if (IS_ERR(msblk->inode_lookup_table)) {
		err = PTR_ERR(msblk->inode_lookup_table);
		msblk->inode_lookup_table = NULL;
		goto failed_mount;
	}

	sb->s_export_op = &squashfs_export_ops;

allocate_root:
	root = new_inode(sb);
	if (!root) {
		err = -ENOMEM;
		goto failed_mount;
	}

	err = squashfs_read_inode(root, root_inode);
	if (err) {
		iget_failed(root);
		goto failed_mount;
	}
	insert_inode_hash(root);

	sb->s_root = d_alloc_root(root);
	if (sb->s_root == NULL) {
		ERROR("Root inode create failed\n");
		err = -ENOMEM;
		iput(root);
		goto failed_mount;
	}

	TRACE("Leaving squashfs_fill_super\n");
	kfree(sblk);
	return 0;

failed_mount:
	if (msblk->tfm)
		crypto_free_pcomp(msblk->tfm);
	squashfs_cache_delete(msblk->block_cache);
	squashfs_cache_delete(msblk->fragment_cache);
	squashfs_cache_delete(msblk->read_page);
	kfree(msblk->inode_lookup_table);
	kfree(msblk->fragment_index);
	kfree(msblk->id_table);
	kfree(sblk);
failure:
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
	return err;
}


static int squashfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct squashfs_sb_info *msblk = dentry->d_sb->s_fs_info;
	u64 id = huge_encode_dev(dentry->d_sb->s_bdev->bd_dev);

	TRACE("Entered squashfs_statfs\n");

	buf->f_type = SQUASHFS_MAGIC;
	buf->f_bsize = msblk->block_size;
	buf->f_blocks = ((msblk->bytes_used - 1) >> msblk->block_log) + 1;
	buf->f_bfree = buf->f_bavail = 0;
	buf->f_files = msblk->inodes;
	buf->f_ffree = 0;
	buf->f_namelen = SQUASHFS_NAME_LEN;
	buf->f_fsid.val[0] = (u32)id;
	buf->f_fsid.val[1] = (u32)(id >> 32);

	return 0;
}


static int squashfs_remount(struct super_block *sb, int *flags, char *data)
{
	*flags |= MS_RDONLY;
	return 0;
}


static void squashfs_put_super(struct super_block *sb)
{
	lock_kernel();

	if (sb->s_fs_info) {
		struct squashfs_sb_info *sbi = sb->s_fs_info;
		squashfs_cache_delete(sbi->block_cache);
		squashfs_cache_delete(sbi->fragment_cache);
		squashfs_cache_delete(sbi->read_page);
		kfree(sbi->id_table);
		kfree(sbi->fragment_index);
		kfree(sbi->meta_index);
		crypto_free_pcomp(sbi->tfm);
		kfree(sb->s_fs_info);
		sb->s_fs_info = NULL;
	}

	unlock_kernel();
}

#ifdef CONFIG_SQUASHFS_MTD
static void
squashfs_kill_sb(struct super_block *sb)
{
	struct squashfs_sb_info *c = sb->s_fs_info;
	generic_shutdown_super(sb);
	put_mtd_device(c->mtd);
	kfree(c);
}

static int
squashfs_sb_compare(struct super_block *sb, void *data)
{
	struct squashfs_sb_info *p = (struct squashfs_sb_info *)data;
	struct squashfs_sb_info *c = sb->s_fs_info;

	/*
	 * The superblocks are considered to be equivalent if the
	 * underlying MTD device is the same one
	 */
	if (c->mtd == p->mtd) {
		printk("%s: match on device %d (\"%s\")\n",
			__func__, p->mtd->index, p->mtd->name);
		return 1;
	} else {
		printk("%s: No match, device %d (\"%s\"), device %d (\"%s\")\n",
			__func__, c->mtd->index, c->mtd->name,
			p->mtd->index, p->mtd->name);
		return 0;
	}
}

static int
squashfs_sb_set(struct super_block *sb, void *data)
{
	struct squashfs_sb_info *p = data;

	/*
	 * For persistence of NFS exports etc. we use the same s_dev
	 * each time we mount the device, don't just use an anonymous
	 * device
	 */
	TRACE(	"%s(): assigning p to sb->s_fs_info p = 0x%p p->mtd = 0x%p "
		"p->mtd->name:%s %p\n", __func__, p, p->mtd, p->mtd->name, sb);
	sb->s_fs_info = p;
	sb->s_dev = MKDEV(MTD_BLOCK_MAJOR, p->mtd->index);

	return 0;
}

static struct super_block *
squashfs_get_sb_mtd(
	struct file_system_type	*fs_type,
	int			flags,
	const char		*dev_name,
	void			*data,
	struct mtd_info		*mtd)
{
	struct super_block *sb;
	struct squashfs_sb_info *c;
	int ret;

	c = kmalloc(sizeof(struct squashfs_sb_info), GFP_KERNEL);

	if (!c)
		return ERR_PTR(-ENOMEM);
	memset(c, 0, sizeof(*c));

	c->mtd = mtd;

	TRACE("%s(): c = 0x%p c->mtd = 0x%p\n", __func__, c, c->mtd);

	sb = sget(fs_type, squashfs_sb_compare, squashfs_sb_set, c);

	if (IS_ERR(sb))
		goto out_put;

	if (sb->s_root) {
		/* New mountpoint for JFFS2 which is already mounted */
		printk(	"%s(): Device %d (\"%s\") is already mounted\n",
			__func__, c->mtd->index, c->mtd->name);
		goto out_put;
	}

	printk("%s(): New superblock for device %d (\"%s\")\n",
		__func__, mtd->index, mtd->name);

	/*
	 * Initialize squashfs superblock locks, the further initialization
	 * will be done later
	 */

	ret = squashfs_fill_super(sb, data, (flags & MS_VERBOSE) ? 1 : 0);

	if (ret) {
		/* Failure case... */
		up_write(&sb->s_umount);
		deactivate_super(sb);
		return ERR_PTR(ret);
	}

	sb->s_flags |= MS_ACTIVE;
	return sb;

out_put:
	kfree(c);
	put_mtd_device(mtd);
	return sb;
}

static struct super_block *
squashfs_get_sb_mtdnr(
	struct file_system_type	*fs_type,
	int			flags,
	const char		*dev_name,
	void			*data,
	int			mtdnr)
{
	struct mtd_info *mtd;

	//printk("%s(): dev_name = %s mtdnr=%d\n", __func__, dev_name, mtdnr);
	mtd = get_mtd_device(NULL, mtdnr);
	if (!mtd) {
		printk(	"%s(): MTD device #%u doesn't appear to exist\n",
			__func__,  mtdnr);
		return ERR_PTR(-EINVAL);
	}

	//printk(	"%s(): calling squashfs_get_sb_mtd, mtd->name: %s "
	//	"mtd->index: %d\n", __func__, mtd->name, mtd->index);
	return squashfs_get_sb_mtd(fs_type, flags, dev_name, data, mtd);
}
#endif /* CONFIG_SQUASHFS_MTD */

static int squashfs_get_sb(struct file_system_type *fs_type, int flags,
				const char *dev_name, void *data,
				struct vfsmount *mnt)
{
#ifdef CONFIG_SQUASHFS_MTD
	struct super_block	*sb;
	int			err, mtdnr;
	struct nameidata	nd;
	struct mtd_info		*mtd;


	if (dev_name[0] == 'm' && dev_name[1] == 't' && dev_name[2] == 'd') {
		/* Probably mounting without the blkdev crap */
		if (dev_name[3] == ':') {
			for (mtdnr = 0; mtdnr < MAX_MTD_DEVICES; mtdnr++) {
				mtd = get_mtd_device(NULL, mtdnr);
				if (mtd) {
					if (!strcmp(mtd->name, dev_name+4)) {
						sb = squashfs_get_sb_mtd(
							fs_type, flags,
							dev_name, data, mtd);
						if (!IS_ERR(sb)) {
							simple_set_mnt(mnt, sb);
							err = 0;
						} else {
							err = -EIO;
						}
						put_mtd_device(mtd);
						return err;
					}
					put_mtd_device(mtd);
				}
			}
			printk("%s(): MTD device with name \"%s\" not found.\n",
				__func__, dev_name+4);
		} else if (isdigit(dev_name[3])) {
			/* Mount by MTD device number name */
			char *endptr;

			mtdnr = simple_strtoul(dev_name+3, &endptr, 0);
			if (!*endptr) {
				/* It was a valid number */
				printk("%s(): mtd%%d, mtdnr %d\n",
					__func__, mtdnr);
				sb = squashfs_get_sb_mtdnr(fs_type, flags,
							dev_name, data, mtdnr);
				if (!IS_ERR(sb)) {
					simple_set_mnt(mnt, sb);
					return 0;
				} else {
					return -EIO;
				}
			}
		}
	}

	/*
	 * Try the old way - the hack where we allowed users to mount
         * /dev/mtdblock$(n) but didn't actually _use_ the blkdev
	 */

	err = path_lookup(dev_name, LOOKUP_FOLLOW, &nd);

	/* printk(KERN_DEBUG "%s(): path_lookup() returned %d, inode %p\n",
		__func__, err, nd.dentry->d_inode); */

	if (err)
		return err;

	err = -EINVAL;

	if (!S_ISBLK(nd.path.dentry->d_inode->i_mode))
		goto out;

	if (nd.path.mnt->mnt_flags & MNT_NODEV) {
		err = -EACCES;
		goto out;
	}

	if (imajor(nd.path.dentry->d_inode) != MTD_BLOCK_MAJOR) {
		if (!(flags & MS_VERBOSE)) /* Yes I mean this. Strangely */
			printk(	"Attempt to mount non-MTD device \"%s\" "
				"as squashfs\n", dev_name);
		goto out;
	}

	mtdnr = iminor(nd.path.dentry->d_inode);
	// not needed for 2.6.31?? path_release(&nd);

	sb = squashfs_get_sb_mtdnr(fs_type, flags, dev_name, data, mtdnr);

	if (!IS_ERR(sb)) {
		simple_set_mnt(mnt, sb);
		return 0;
	} else {
		return -EIO;
	}
out:
	// not needed for 2.6.31?? path_release(&nd);
	return err;
#else
	return get_sb_bdev(fs_type, flags, dev_name, data, squashfs_fill_super,
				mnt);
#endif
}


static struct kmem_cache *squashfs_inode_cachep;


static void init_once(void *foo)
{
	struct squashfs_inode_info *ei = foo;

	inode_init_once(&ei->vfs_inode);
}


static int __init init_inodecache(void)
{
	squashfs_inode_cachep = kmem_cache_create("squashfs_inode_cache",
		sizeof(struct squashfs_inode_info), 0,
		SLAB_HWCACHE_ALIGN|SLAB_RECLAIM_ACCOUNT, init_once);

	return squashfs_inode_cachep ? 0 : -ENOMEM;
}


static void destroy_inodecache(void)
{
	kmem_cache_destroy(squashfs_inode_cachep);
}


static int __init init_squashfs_fs(void)
{
	int err = init_inodecache();

	if (err)
		return err;

	err = register_filesystem(&squashfs_fs_type);
	if (err) {
		destroy_inodecache();
		return err;
	}

	printk(KERN_INFO "squashfs: version 4.0 (2009/01/31) "
		"Phillip Lougher\n");

	return 0;
}


static void __exit exit_squashfs_fs(void)
{
	unregister_filesystem(&squashfs_fs_type);
	destroy_inodecache();
}


static struct inode *squashfs_alloc_inode(struct super_block *sb)
{
	struct squashfs_inode_info *ei =
		kmem_cache_alloc(squashfs_inode_cachep, GFP_KERNEL);

	return ei ? &ei->vfs_inode : NULL;
}


static void squashfs_destroy_inode(struct inode *inode)
{
	kmem_cache_free(squashfs_inode_cachep, squashfs_i(inode));
}

#ifdef CONFIG_SQUASHFS_MTD
static void squashfs_kill_sb(struct super_block *sb);
#endif

static struct file_system_type squashfs_fs_type = {
	.owner = THIS_MODULE,
	.name = "squashfs",
	.get_sb = squashfs_get_sb,
#ifdef CONFIG_SQUASHFS_MTD
	.kill_sb = squashfs_kill_sb,
#else
	.kill_sb = kill_block_super,
#endif
	.fs_flags = FS_REQUIRES_DEV
};

static struct super_operations squashfs_super_ops = {
	.alloc_inode = squashfs_alloc_inode,
	.destroy_inode = squashfs_destroy_inode,
	.statfs = squashfs_statfs,
	.put_super = squashfs_put_super,
	.remount_fs = squashfs_remount
};

module_init(init_squashfs_fs);
module_exit(exit_squashfs_fs);
MODULE_DESCRIPTION("squashfs 4.0, a compressed read-only filesystem");
MODULE_AUTHOR("Phillip Lougher <phillip@lougher.demon.co.uk>");
MODULE_LICENSE("GPL");
