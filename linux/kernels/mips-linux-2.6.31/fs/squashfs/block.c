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
 * block.c
 */

/*
 * This file implements the low-level routines to read and decompress
 * datablocks and metadata blocks.
 */

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/buffer_head.h>

#include <crypto/compress.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs_fs_i.h"
#include "squashfs.h"


#ifdef CONFIG_SQUASHFS_MTD

#define SQUASHFS_MTD_OFFSET(index, msblk)	(index * msblk->devblksize)
#define SQUASHFS_BUF_FREE(buf)			kfree(buf)
#define SQUASHFS_GET_BLK(s, index)		squashfs_getblk_mtd(s, (index))

static int
squashfs_mtd_read(
	struct super_block	*s,
	int			index,
	size_t			*retlen,
	u_char			*buf)
{
	struct squashfs_sb_info	*msblk = s->s_fs_info;
	struct mtd_info		*mtd = msblk->mtd;
	int			ret;
	loff_t			off = SQUASHFS_MTD_OFFSET(index, msblk);

	TRACE(	"%s(): issuing mtd from offset: %lld on mtd->index:%d "
		"mtd->name:%s msblk=%p mtd=%p s=%p \n", __func__, off,
		mtd->index, mtd->name, msblk, mtd, s);

	ret = mtd->read(mtd, off, msblk->devblksize, retlen, buf);

	TRACE(	"%s(): mtd->read result:%d retlen:0x%x\n",
		__func__, ret, *retlen);

	if (ret != 0) {
		printk("%s(): mtd read failed err %d\n", __func__, ret);
	}

	return ret;
}

u_char *
squashfs_getblk_mtd(struct super_block *s, int index)
{
	struct squashfs_sb_info	*msblk = s->s_fs_info;
	u_char			*buf;
	size_t			retlen;
	int			ret;

	buf = kmalloc(msblk->devblksize, GFP_KERNEL);

	if (!buf) {
		return NULL;
	}

	ret = squashfs_mtd_read(s, index, &retlen, buf);
	if (ret != 0) {
		return NULL;
	}

	return buf;
}

static u_char *
get_block_length(
	struct super_block	*sb,
	u64			*cur_index,
	int			*offset,
	int			*length)
{
	struct squashfs_sb_info	*msblk = sb->s_fs_info;
	int			ret;
	size_t			retlen;
	u_char			*buf;

	buf = kmalloc(msblk->devblksize, GFP_KERNEL);
	if (buf == NULL) {
		return NULL;
	}

	ret = squashfs_mtd_read(sb, *cur_index, &retlen, buf);
	if (ret != 0) {
		goto out;
	}

	if (msblk->devblksize - *offset == 1) {
		*length = (unsigned char) buf[*offset];
		ret = squashfs_mtd_read(sb, ++(*cur_index), &retlen, buf);
		if (ret != 0) {
			goto out;
		}
		*length |= (unsigned char) buf[0] << 8;
		*offset = 1;
	} else {
		*length = (unsigned char) buf[*offset] |
			(unsigned char) buf[*offset + 1] << 8;
		*offset += 2;
	}

	return buf;

out:
	kfree(buf);
	return NULL;
}

#else

#define SQUASHFS_BUF_FREE(bh)		put_bh(bh)
#define SQUASHFS_GET_BLK(s, index)	sb_getblk(s, index)

/*
 * Read the metadata block length, this is stored in the first two
 * bytes of the metadata block.
 */
static struct buffer_head *get_block_length(struct super_block *sb,
			u64 *cur_index, int *offset, int *length)
{
	struct squashfs_sb_info *msblk = sb->s_fs_info;
	struct buffer_head *bh;

	bh = sb_bread(sb, *cur_index);
	if (bh == NULL)
		return NULL;

	if (msblk->devblksize - *offset == 1) {
		*length = (unsigned char) bh->b_data[*offset];
		put_bh(bh);
		bh = sb_bread(sb, ++(*cur_index));
		if (bh == NULL)
			return NULL;
		*length |= (unsigned char) bh->b_data[0] << 8;
		*offset = 1;
	} else {
		*length = (unsigned char) bh->b_data[*offset] |
			(unsigned char) bh->b_data[*offset + 1] << 8;
		*offset += 2;
	}

	return bh;
}

#endif	/* CONFIG_SQUASHFS_MTD */


/*
 * Read and decompress a metadata block or datablock.  Length is non-zero
 * if a datablock is being read (the size is stored elsewhere in the
 * filesystem), otherwise the length is obtained from the first two bytes of
 * the metadata block.  A bit in the length field indicates if the block
 * is stored uncompressed in the filesystem (usually because compression
 * generated a larger block - this does occasionally happen with zlib).
 */
int squashfs_read_data(struct super_block *sb, void **buffer, u64 index,
			int length, u64 *next_index, int srclength, int pages)
{
	struct squashfs_sb_info *msblk = sb->s_fs_info;
#ifdef CONFIG_SQUASHFS_MTD
	u_char **bh;
#else
	struct buffer_head **bh;
#endif	/* CONFIG_SQUASHFS_MTD */
	int offset = index & ((1 << msblk->devblksize_log2) - 1);
	u64 cur_index = index >> msblk->devblksize_log2;
	int bytes, compressed, b = 0, k = 0, page = 0, avail;


	bh = kcalloc((msblk->block_size >> msblk->devblksize_log2) + 1,
				sizeof(*bh), GFP_KERNEL);
	if (bh == NULL)
		return -ENOMEM;

	if (length) {
		/*
		 * Datablock.
		 */
		bytes = -offset;
		compressed = SQUASHFS_COMPRESSED_BLOCK(length);
		length = SQUASHFS_COMPRESSED_SIZE_BLOCK(length);
		if (next_index)
			*next_index = index + length;

		TRACE("Block @ 0x%llx, %scompressed size %d, src size %d\n",
			index, compressed ? "" : "un", length, srclength);

		if (length < 0 || length > srclength ||
				(index + length) > msblk->bytes_used)
			goto read_failure;

		for (b = 0; bytes < length; b++, cur_index++) {
			bh[b] = SQUASHFS_GET_BLK(sb, cur_index);
			if (bh[b] == NULL)
				goto block_release;
			bytes += msblk->devblksize;
		}
#if !defined(CONFIG_SQUASHFS_MTD)
		ll_rw_block(READ, b, bh);
#endif
	} else {
		/*
		 * Metadata block.
		 */
		if ((index + 2) > msblk->bytes_used)
			goto read_failure;

		bh[0] = get_block_length(sb, &cur_index, &offset, &length);
		if (bh[0] == NULL)
			goto read_failure;
		b = 1;

		bytes = msblk->devblksize - offset;
		compressed = SQUASHFS_COMPRESSED(length);
		length = SQUASHFS_COMPRESSED_SIZE(length);
		if (next_index)
			*next_index = index + length + 2;

		TRACE("Block @ 0x%llx, %scompressed size %d\n", index,
				compressed ? "" : "un", length);

		if (length < 0 || length > srclength ||
					(index + length) > msblk->bytes_used)
			goto block_release;

		for (; bytes < length; b++) {
			bh[b] = SQUASHFS_GET_BLK(sb, ++cur_index);
			if (bh[b] == NULL)
				goto block_release;
			bytes += msblk->devblksize;
		}
#if !defined(CONFIG_SQUASHFS_MTD)
		ll_rw_block(READ, b - 1, bh + 1);
#endif
	}

	if (compressed) {
		int res = 0, decomp_init = 0;
		struct comp_request req;

		/*
		 * Uncompress block.
		 */

		mutex_lock(&msblk->read_data_mutex);

		req.avail_out = 0;
		req.avail_in = 0;

		bytes = length;
		length = 0;
		do {
			if (req.avail_in == 0 && k < b) {
				avail = min(bytes, msblk->devblksize - offset);
				bytes -= avail;
#if !defined(CONFIG_SQUASHFS_MTD)
				wait_on_buffer(bh[k]);
				if (!buffer_uptodate(bh[k]))
					goto release_mutex;
#endif /* CONFIG_SQUASHFS_MTD */

				if (avail == 0) {
					offset = 0;
					SQUASHFS_BUF_FREE(bh[k++]);
					continue;
				}

#ifdef CONFIG_SQUASHFS_MTD
				req.next_in = bh[k] + offset;
#else
				req.next_in = bh[k]->b_data + offset;
#endif
				req.avail_in = avail;
				offset = 0;
			}

			if (req.avail_out == 0 && page < pages) {
				req.next_out = buffer[page++];
				req.avail_out = PAGE_CACHE_SIZE;
			}
			if (!decomp_init) {
				res = crypto_decompress_init(msblk->tfm);
				if (res) {
					ERROR("crypto_decompress_init "
						"returned %d, srclength %d\n",
						res, srclength);
					goto release_mutex;
				}
				decomp_init = 1;
			}

			res = crypto_decompress_update(msblk->tfm, &req);
			if (res < 0) {
				ERROR("crypto_decompress_update returned %d, "
					"data probably corrupt\n", res);
				goto release_mutex;
			}
			length += res;

			if (req.avail_in == 0 && k < b)
				SQUASHFS_BUF_FREE(bh[k++]);

		} while (bytes || res);

		res = crypto_decompress_final(msblk->tfm, &req);
		if (res < 0) {
			ERROR("crypto_decompress_final returned %d, data "
				"probably corrupt\n", res);
			goto release_mutex;
		}
		length += res;

		mutex_unlock(&msblk->read_data_mutex);
	} else {
		/*
		 * Block is uncompressed.
		 */
#ifdef CONFIG_SQUASHFS_MTD
		int in, pg_offset = 0;
#else
		int i, in, pg_offset = 0;
#endif

#if !defined(CONFIG_SQUASHFS_MTD)
		for (i = 0; i < b; i++) {
			wait_on_buffer(bh[i]);
			if (!buffer_uptodate(bh[i]))
				goto block_release;
		}
#endif /* CONFIG_SQUASHFS_MTD */
		for (bytes = length; k < b; k++) {
			in = min(bytes, msblk->devblksize - offset);
			bytes -= in;
			while (in) {
				if (pg_offset == PAGE_CACHE_SIZE) {
					page++;
					pg_offset = 0;
				}
				avail = min_t(int, in, PAGE_CACHE_SIZE -
						pg_offset);
#ifdef CONFIG_SQUASHFS_MTD
				memcpy(buffer[page] + pg_offset,
						bh[k] + offset, avail);
#else
				memcpy(buffer[page] + pg_offset,
						bh[k]->b_data + offset, avail);
#endif /* CONFIG_SQUASHFS_MTD */
				in -= avail;
				pg_offset += avail;
				offset += avail;
			}
			offset = 0;
			SQUASHFS_BUF_FREE(bh[k]);
		}
	}

	kfree(bh);
	return length;

release_mutex:
	mutex_unlock(&msblk->read_data_mutex);

block_release:
	for (; k < b; k++)
		SQUASHFS_BUF_FREE(bh[k]);

read_failure:
	ERROR("squashfs_read_data failed to read block 0x%llx\n",
					(unsigned long long) index);
	kfree(bh);
	return -EIO;
}
