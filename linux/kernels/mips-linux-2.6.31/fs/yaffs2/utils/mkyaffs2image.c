/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 * Nick Bane modifications flagged NCB
 * Endian handling patches by James Ng.
 * mkyaffs2image hacks by NCB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * makeyaffs2image.c
 *
 * Makes a YAFFS2 file system image that can be used to load up a file system.
 * Uses default Linux MTD layout - change if you need something different.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include "yaffs_ecc.h"
#include "yaffs_guts.h"
#include <linux/autoconf.h>

#include "yaffs_tagsvalidity.h"
#include "yaffs_packedtags2.h"

unsigned yaffs_traceMask=0;

#define MAX_OBJECTS 10000

#define chunkSize 2048
#define spareSize 64

const char * mkyaffsimage_c_version = "$Id: //depot/sw/releases/Aquila_9.2.0_U11/linux/kernels/mips-linux-2.6.31/fs/yaffs2/utils/mkyaffs2image.c#1 $";


typedef struct
{
	dev_t dev;
	ino_t ino;
	int   obj;
} objItem;


static objItem obj_list[MAX_OBJECTS];
static int n_obj = 0;
static int obj_id = YAFFS_NOBJECT_BUCKETS + 1;

static int nObjects, nDirectories, nPages;

static int outFile;

static int error;

static int convert_endian = 0;

static unsigned pad = 0;

static char *devfile;

static void little_to_big_endian(yaffs_ExtendedTags *);
static void padup(int);

static int obj_compare(const void *a, const void * b)
{
  objItem *oa, *ob;

  oa = (objItem *)a;
  ob = (objItem *)b;

  if(oa->dev < ob->dev) return -1;
  if(oa->dev > ob->dev) return 1;
  if(oa->ino < ob->ino) return -1;
  if(oa->ino > ob->ino) return 1;

  return 0;
}


static void add_obj_to_list(dev_t dev, ino_t ino, int obj)
{
	if(n_obj < MAX_OBJECTS)
	{
		obj_list[n_obj].dev = dev;
		obj_list[n_obj].ino = ino;
		obj_list[n_obj].obj = obj;
		n_obj++;
		qsort(obj_list,n_obj,sizeof(objItem),obj_compare);

	}
	else
	{
		// oops! not enough space in the object array
		fprintf(stderr,"Not enough space in object array\n");
		exit(2);
	}
}


static int find_obj_in_list(dev_t dev, ino_t ino)
{
	objItem *i = NULL;
	objItem test;

	test.dev = dev;
	test.ino = ino;

	if(n_obj > 0)
	{
		i = bsearch(&test,obj_list,n_obj,sizeof(objItem),obj_compare);
	}

	if(i)
	{
		return i->obj;
	}
	return -1;
}

/* This little function converts a little endian tag to a big endian tag.
 * NOTE: The tag is not usable after this other than calculating the CRC
 * with.
 */
#if 0 // FIXME NCB
static void little_to_big_endian(yaffs_Tags *tagsPtr)
{
    yaffs_TagsUnion * tags = (yaffs_TagsUnion* )tagsPtr; // Work in bytes.
    yaffs_TagsUnion   temp;

    memset(&temp, 0, sizeof(temp));
    // Ick, I hate magic numbers.
    temp.asBytes[0] = ((tags->asBytes[2] & 0x0F) << 4) | ((tags->asBytes[1] & 0xF0) >> 4);
    temp.asBytes[1] = ((tags->asBytes[1] & 0x0F) << 4) | ((tags->asBytes[0] & 0xF0) >> 4);
    temp.asBytes[2] = ((tags->asBytes[0] & 0x0F) << 4) | ((tags->asBytes[2] & 0x30) >> 2) | ((tags->asBytes[3] & 0xC0) >> 6);
    temp.asBytes[3] = ((tags->asBytes[3] & 0x3F) << 2) | ((tags->asBytes[2] & 0xC0) >> 6);
    temp.asBytes[4] = ((tags->asBytes[6] & 0x03) << 6) | ((tags->asBytes[5] & 0xFC) >> 2);
    temp.asBytes[5] = ((tags->asBytes[5] & 0x03) << 6) | ((tags->asBytes[4] & 0xFC) >> 2);
    temp.asBytes[6] = ((tags->asBytes[4] & 0x03) << 6) | (tags->asBytes[7] & 0x3F);
    temp.asBytes[7] = (tags->asBytes[6] & 0xFC) | ((tags->asBytes[7] & 0xC0) >> 6);

    // Now copy it back.
    tags->asBytes[0] = temp.asBytes[0];
    tags->asBytes[1] = temp.asBytes[1];
    tags->asBytes[2] = temp.asBytes[2];
    tags->asBytes[3] = temp.asBytes[3];
    tags->asBytes[4] = temp.asBytes[4];
    tags->asBytes[5] = temp.asBytes[5];
    tags->asBytes[6] = temp.asBytes[6];
    tags->asBytes[7] = temp.asBytes[7];
}
#endif

#define SWAP32(x)   ((((x) & 0x000000FF) << 24) | \
                     (((x) & 0x0000FF00) << 8 ) | \
                     (((x) & 0x00FF0000) >> 8 ) | \
                     (((x) & 0xFF000000) >> 24))

#define SWAP16(x)   ((((x) & 0x00FF) << 8) | \
                     (((x) & 0xFF00) >> 8))

static int write_chunk(__u8 *data, __u32 objId, __u32 chunkId, __u32 nBytes)
{
#define	proper_spare 0
	yaffs_ExtendedTags t;
#if proper_spare
	__u8 spare[spareSize];
	yaffs_PackedTags2 *pt;
#	define	pt_fld(x)	(pt->x)
#	define	pt_addr()	pt
#else
	yaffs_PackedTags2 pt;
#	define	pt_fld(x)	(pt.x)
#	define	pt_addr()	(&pt)
#endif

	error = write(outFile,data,chunkSize);
	if(error < 0) return error;

	yaffs_InitialiseTags(&t);

#if proper_spare
	memset(spare, 0xff, sizeof(spare));
	pt = (yaffs_PackedTags2 *)spare;
#endif
	t.chunkId = chunkId;
//	t.serialNumber = 0;
	t.serialNumber = 1;	// **CHECK**
	t.byteCount = nBytes;
	t.objectId = objId;

	t.sequenceNumber = YAFFS_LOWEST_SEQUENCE_NUMBER;

// added NCB **CHECK**
	t.chunkUsed = 1;

	if (convert_endian) {
		little_to_big_endian(&t);
	}

	nPages++;

	//yaffs_PackTags2(pt_addr(), &t);
	yaffs_PackTags2TagsPart(&pt_fld(t), &t);

#ifndef CONFIG_YAFFS_DISABLE_TAGS_ECC
	yaffs_ECCCalculateOther((unsigned char *)&pt_fld(t),
				sizeof(yaffs_PackedTags2TagsPart),
				&pt_fld(ecc));
#if 0
	printf("Object %u chunk %u ecc 0x%x:0x%x:0x%x\n", SWAP32(t.objectId),
			chunkId,
			pt_fld(ecc).colParity,
			pt_fld(ecc).lineParity,
			pt_fld(ecc).lineParityPrime);
#endif
#endif
	if (convert_endian) {
		pt_fld(ecc).lineParity = SWAP32(pt_fld(ecc).lineParity);
		pt_fld(ecc).lineParityPrime = SWAP32(pt_fld(ecc).lineParityPrime);
	}

#if 0
	if (convert_endian) {
		printf("Writing spare for Object %u %u at 0x%lx\n", SWAP32(t.objectId),
			SWAP32(pt_fld(t).objectId), lseek(outFile, 0, SEEK_CUR));
	} else {
		printf("Writing spare for Object %u %u at 0x%lx\n", t.objectId,
			pt_fld(t).objectId, lseek(outFile, 0, SEEK_CUR));
	}
#endif

//	return write(outFile, pt_addr(), sizeof(yaffs_PackedTags2));
	return write(outFile, pt_addr(), spareSize);
}

// This one is easier, since the types are more standard. No funky shifts here.
static void object_header_little_to_big_endian(yaffs_ObjectHeader* oh)
{
    oh->type = SWAP32(oh->type); // GCC makes enums 32 bits.
    oh->parentObjectId = SWAP32(oh->parentObjectId); // int
    oh->sum__NoLongerUsed = SWAP16(oh->sum__NoLongerUsed); // __u16 - Not used, but done for completeness.
    // name = skip. Char array. Not swapped.
    oh->yst_mode = SWAP32(oh->yst_mode);
#ifdef CONFIG_YAFFS_WINCE // WinCE doesn't implement this, but we need to just in case.
    // In fact, WinCE would be *THE* place where this would be an issue!
    oh->notForWinCE[0] = SWAP32(oh->notForWinCE[0]);
    oh->notForWinCE[1] = SWAP32(oh->notForWinCE[1]);
    oh->notForWinCE[2] = SWAP32(oh->notForWinCE[2]);
    oh->notForWinCE[3] = SWAP32(oh->notForWinCE[3]);
    oh->notForWinCE[4] = SWAP32(oh->notForWinCE[4]);
#else
    // Regular POSIX.
    oh->yst_uid = SWAP32(oh->yst_uid);
    oh->yst_gid = SWAP32(oh->yst_gid);
    oh->yst_atime = SWAP32(oh->yst_atime);
    oh->yst_mtime = SWAP32(oh->yst_mtime);
    oh->yst_ctime = SWAP32(oh->yst_ctime);
#endif

    oh->fileSize = SWAP32(oh->fileSize); // Aiee. An int... signed, at that!
    oh->equivalentObjectId = SWAP32(oh->equivalentObjectId);
    // alias  - char array.
    oh->yst_rdev = SWAP32(oh->yst_rdev);

#ifdef CONFIG_YAFFS_WINCE
    oh->win_ctime[0] = SWAP32(oh->win_ctime[0]);
    oh->win_ctime[1] = SWAP32(oh->win_ctime[1]);
    oh->win_atime[0] = SWAP32(oh->win_atime[0]);
    oh->win_atime[1] = SWAP32(oh->win_atime[1]);
    oh->win_mtime[0] = SWAP32(oh->win_mtime[0]);
    oh->win_mtime[1] = SWAP32(oh->win_mtime[1]);
    oh->roomToGrow[0] = SWAP32(oh->roomToGrow[0]);
    oh->roomToGrow[1] = SWAP32(oh->roomToGrow[1]);
    oh->roomToGrow[2] = SWAP32(oh->roomToGrow[2]);
    oh->roomToGrow[3] = SWAP32(oh->roomToGrow[3]);
    oh->roomToGrow[4] = SWAP32(oh->roomToGrow[4]);
    oh->roomToGrow[5] = SWAP32(oh->roomToGrow[5]);
#else
    oh->roomToGrow[0] = SWAP32(oh->roomToGrow[0]);
    oh->roomToGrow[1] = SWAP32(oh->roomToGrow[1]);
    oh->roomToGrow[2] = SWAP32(oh->roomToGrow[2]);
    oh->roomToGrow[3] = SWAP32(oh->roomToGrow[3]);
    oh->roomToGrow[4] = SWAP32(oh->roomToGrow[4]);
    oh->roomToGrow[5] = SWAP32(oh->roomToGrow[5]);
    oh->roomToGrow[6] = SWAP32(oh->roomToGrow[6]);
    oh->roomToGrow[7] = SWAP32(oh->roomToGrow[7]);
    oh->roomToGrow[8] = SWAP32(oh->roomToGrow[8]);
    oh->roomToGrow[9] = SWAP32(oh->roomToGrow[9]);
    oh->roomToGrow[10] = SWAP32(oh->roomToGrow[10]);
    oh->roomToGrow[11] = SWAP32(oh->roomToGrow[11]);
#endif
}

static void little_to_big_endian(yaffs_ExtendedTags *t)
{
	t->chunkUsed			= SWAP32(t->chunkUsed);
	t->objectId			= SWAP32(t->objectId);
	t->chunkId			= SWAP32(t->chunkId);
	t->byteCount			= SWAP32(t->byteCount);
	t->eccResult			= SWAP32(t->eccResult);
	t->blockBad			= SWAP32(t->blockBad);
	t->chunkDeleted			= SWAP32(t->chunkDeleted);
	t->serialNumber			= SWAP32(t->serialNumber);
	t->sequenceNumber		= SWAP32(t->sequenceNumber);
	t->extraHeaderInfoAvailable	= SWAP32(t->extraHeaderInfoAvailable);
	t->extraParentObjectId		= SWAP32(t->extraParentObjectId);
	t->extraIsShrinkHeader		= SWAP32(t->extraIsShrinkHeader);
	t->extraShadows			= SWAP32(t->extraShadows);
	t->extraObjectType		= SWAP32(t->extraObjectType);
	t->extraFileLength		= SWAP32(t->extraFileLength);
	t->extraEquivalentObjectId	= SWAP32(t->extraEquivalentObjectId);
}

static int write_object_header(int objId, yaffs_ObjectType t, struct stat *s, int parent, const char *name, int equivalentObj, const char * alias)
{
	__u8 bytes[chunkSize];


	yaffs_ObjectHeader *oh = (yaffs_ObjectHeader *)bytes;

	memset(bytes,0xff,sizeof(bytes));

	oh->type = t;

	oh->parentObjectId = parent;

	strncpy(oh->name,name,YAFFS_MAX_NAME_LENGTH);


	if(t != YAFFS_OBJECT_TYPE_HARDLINK)
	{
		oh->yst_mode = s->st_mode;
		oh->yst_uid = s->st_uid;
// NCB 12/9/02		oh->yst_gid = s->yst_uid;
		oh->yst_gid = s->st_gid;
		oh->yst_atime = s->st_atime;
		oh->yst_mtime = s->st_mtime;
		oh->yst_ctime = s->st_ctime;
		oh->yst_rdev  = s->st_rdev;
	}

	if(t == YAFFS_OBJECT_TYPE_FILE)
	{
		oh->fileSize = s->st_size;
	}

	if(t == YAFFS_OBJECT_TYPE_HARDLINK)
	{
		oh->equivalentObjectId = equivalentObj;
	}

	if(t == YAFFS_OBJECT_TYPE_SYMLINK)
	{
		strncpy(oh->alias,alias,YAFFS_MAX_ALIAS_LENGTH);
	}

	if (convert_endian)
	{
    		object_header_little_to_big_endian(oh);
	}

	return write_chunk(bytes,objId,0,0xffff);

}

struct dirent *readdev(struct stat *stats)
{
	static struct dirent	dir;
	static FILE		*dev;
	static int		dev_done;
	char			*line, *p, *pn;;
	size_t			n;
	char			*name, type;
	unsigned long		mode = 0755, uid = 0, gid = 0,
				major = 0, minor = 0,
				start = 0, increment = 1, count = 0;

	if (dev_done) {
		return NULL;
	}

	if (!dev) {
		if (!devfile) {
			return NULL;
		}
		dev = fopen(devfile, "r");
		if (!dev) {
			perror(devfile);
			return NULL;
		}
	}

redo:
	line = NULL;
	if (getline(&line, &n, dev) < 0) {
		fclose(dev);
		dev_done = 1;
		return NULL;
	}

	p = line;

	while (isspace(*p++));

	if (*p == '#') {
		// Skip comments
		free(line);
		goto redo;
	}

	name = NULL;
	if (sscanf (line, "%as %c %lo %lu %lu %lu %lu %lu %lu %lu",
				&name, &type, &mode, &uid, &gid, &major, &minor,
				&start, &increment, &count) < 0) {
		free(line);
		if (name) {
			free(name);
		}
		goto redo;
	}

	for (pn = name; *pn; pn++) {
		if (*pn == '/') {
			p = pn;
		}
	}

	if (*(p + 1)) {
		memset(&dir, 0, sizeof(dir));
		strcpy(dir.d_name, p+1);
	}

	//printf("Procesing: <%s><%s>\n", p+1, line);

	memset(stats, 0, sizeof(*stats));

	switch (type) {
		case 'p':
			mode |= S_IFIFO;
			break;
		case 'c':
			mode |= S_IFCHR;
			break;
		case 'b':
			mode |= S_IFBLK;
			break;
		case 'l':
			mode |= S_IFLNK;
			break;
		default:
			//printf("Ignoring entry <%s>\n", line);
			free(name);
			free(line);
			goto redo;
	}

	stats->st_mode	= mode;
	stats->st_uid	= uid;
	stats->st_gid	= gid;
	stats->st_rdev	= makedev(major, minor);

	free(name);
	free(line);

	return &dir;

}

static int process_directory(int parent, const char *path, int devices)
{

	DIR		*dir;
	struct dirent	*entry, *deventry = NULL;
	char		full_name[500];
	struct stat	stats;
	int		equivalentObj;
	int		newObj;


	nDirectories++;

	dir = opendir(path);

	if(dir)
	{
		while((entry = readdir(dir)) || (devices && (deventry = readdev(&stats))))
		{

			/* Ignore . and .. */
			if (entry && ((strcmp(entry->d_name, ".") == 0) ||
			    (strcmp(entry->d_name, "..") == 0))) {
				continue;
			}

			if (!entry) {
				entry = deventry;
			}
			sprintf(full_name,"%s/%s",path,entry->d_name);
			if (!deventry) {
				lstat(full_name,&stats);
			}

			if(S_ISLNK(stats.st_mode) ||
			    S_ISREG(stats.st_mode) ||
			    S_ISDIR(stats.st_mode) ||
			    S_ISFIFO(stats.st_mode) ||
			    S_ISBLK(stats.st_mode) ||
			    S_ISCHR(stats.st_mode) ||
			    S_ISSOCK(stats.st_mode))
			{

				newObj = obj_id++;
				nObjects++;

				printf("Object %d, %s is a ",newObj,full_name);

				/* We're going to create an object for it */
				if(!deventry && (equivalentObj = find_obj_in_list(stats.st_dev, stats.st_ino)) > 0)
				{
				 	/* we need to make a hard link */
				 	printf("hard link to object %d\n",equivalentObj);
					error =  write_object_header(newObj, YAFFS_OBJECT_TYPE_HARDLINK, &stats, parent, entry->d_name, equivalentObj, NULL);
				}
				else
				{

					add_obj_to_list(stats.st_dev,stats.st_ino,newObj);

					if(S_ISLNK(stats.st_mode))
					{

						char symname[500];

						memset(symname,0, sizeof(symname));

						readlink(full_name,symname,sizeof(symname) -1);

						printf("symlink to \"%s\"\n",symname);
						error =  write_object_header(newObj, YAFFS_OBJECT_TYPE_SYMLINK, &stats, parent, entry->d_name, -1, symname);

					}
					else if(S_ISREG(stats.st_mode))
					{
						printf("file, ");
						error =  write_object_header(newObj, YAFFS_OBJECT_TYPE_FILE, &stats, parent, entry->d_name, -1, NULL);

						if(error >= 0)
						{
							int h;
							__u8 bytes[chunkSize];
							int nBytes;
							int chunk = 0;

							h = open(full_name,O_RDONLY);
							if(h >= 0)
							{
								memset(bytes,0xff,sizeof(bytes));
								while((nBytes = read(h,bytes,sizeof(bytes))) > 0)
								{
									chunk++;
									write_chunk(bytes,newObj,chunk,nBytes);
									memset(bytes,0xff,sizeof(bytes));
								}
								if(nBytes < 0)
								   error = nBytes;

								printf("%d data chunks written\n",chunk);
							}
							else
							{
								perror("Error opening file");
							}
							close(h);

						}

					}
					else if(S_ISSOCK(stats.st_mode))
					{
						printf("socket\n");
						error =  write_object_header(newObj, YAFFS_OBJECT_TYPE_SPECIAL, &stats, parent, entry->d_name, -1, NULL);
					}
					else if(S_ISFIFO(stats.st_mode))
					{
						printf("fifo\n");
						error =  write_object_header(newObj, YAFFS_OBJECT_TYPE_SPECIAL, &stats, parent, entry->d_name, -1, NULL);
					}
					else if(S_ISCHR(stats.st_mode))
					{
						printf("character device\n");
						error =  write_object_header(newObj, YAFFS_OBJECT_TYPE_SPECIAL, &stats, parent, entry->d_name, -1, NULL);
					}
					else if(S_ISBLK(stats.st_mode))
					{
						printf("block device\n");
						error =  write_object_header(newObj, YAFFS_OBJECT_TYPE_SPECIAL, &stats, parent, entry->d_name, -1, NULL);
					}
					else if(S_ISDIR(stats.st_mode))
					{
						printf("directory\n");
						error =  write_object_header(newObj, YAFFS_OBJECT_TYPE_DIRECTORY, &stats, parent, entry->d_name, -1, NULL);
// NCB modified 10/9/2001			process_directory(1,full_name);
						process_directory(newObj, full_name,
							strncmp("dev", entry->d_name, sizeof("dev")) == 0);
					}
				}
			}
			else
			{
				printf("%s: we don't handle this type\n", full_name);
			}
		}
	}

	return 0;

}

/*
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 * This works as of now. Not sure if spareSize
 * should be factored in.
 * FIXME FIXME FIXME FIXME FIXME FIXME FIXME
 */
static void padup(int fd)
{
	loff_t		l;
	unsigned	i;
	char		c = 0xff;

	if (!pad) {
		pad = chunkSize + spareSize;
	}

	l = lseek(fd, 0, SEEK_END);

	i = pad - (l % pad);

	if (i == pad) {
		return;
	}

	while (i--) {
		write(fd, &c, sizeof(c));
	}
}


int main(int argc, char *argv[])
{
	struct stat	stats;
	int		i;

	printf("mkyaffs2image: image building tool for YAFFS2 built "__DATE__"\n");

	if(argc < 3)
	{
usage:
		printf("usage: mkyaffs2image dir image_file [convert]\n");
		printf("	dir		the directory tree to be converted\n");
		printf("	image_file	the output file to hold the image\n");
		printf("	convert		produce a big-endian image from a little-endian machine\n");
		printf("	bs		block size (to pad till end of block)\n");
		printf("	dev		path/to/dev.txt\n");
		exit(1);
	}

	for (i = 3; i < argc; i ++) {
		if (strncmp(argv[i], "convert", sizeof("convert")) == 0) {
			convert_endian = 1;
		}

		if (strncmp(argv[i], "bs", sizeof("bs")) == 0) {
			char *p;

			if ((i + 1) > argc) {
				goto usage;
			}

			pad = strtoul(argv[i + 1], &p, 0);

			printf("padding to %u%c\n", pad, *p);

			if (*p == 'k' || *p == 'K') {
				pad = (pad << 10);
			} else if (*p == 'm' || *p == 'M') {
				pad = (pad << 20);
			} else if (*p == 'g' || *p == 'G') {
				pad = (pad << 30);
			}

			i ++;
		}

		if (strncmp(argv[i], "dev", sizeof("dev")) == 0) {
			if ((i + 1) > argc) {
				goto usage;
			}
			i ++;
			devfile = argv[i];
		}

	}

	if(stat(argv[1],&stats) < 0)
	{
		printf("Could not stat %s\n",argv[1]);
		exit(1);
	}

	if(!S_ISDIR(stats.st_mode))
	{
		printf(" %s is not a directory\n",argv[1]);
		exit(1);
	}

	outFile = open(argv[2],O_CREAT | O_TRUNC | O_WRONLY, S_IREAD | S_IWRITE);


	if(outFile < 0)
	{
		printf("Could not open output file %s\n",argv[2]);
		exit(1);
	}

	printf("Processing directory %s into image file %s\n",argv[1],argv[2]);
	error =  write_object_header(1, YAFFS_OBJECT_TYPE_DIRECTORY, &stats, 1,"", -1, NULL);
	if(error)
	error = process_directory(YAFFS_OBJECTID_ROOT, argv[1], 0);

	if (error >= 0) {
		padup(outFile);
	}

	close(outFile);

	if(error < 0)
	{
		perror("operation incomplete");
		exit(1);
	}
	else
	{
		printf("Operation complete.\n"
		       "%d objects in %d directories\n"
		       "%d NAND pages\n",nObjects, nDirectories, nPages);
	}

	exit(0);
}

