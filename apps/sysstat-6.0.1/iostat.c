/*
 * iostat: report CPU and I/O statistics
 * (C) 1998-2005 by Sebastien GODARD (sysstat <at> wanadoo.fr)
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published  by  the *
 * Free Software Foundation; either version 2 of the License, or (at  your *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it  will  be  useful,  but *
 * WITHOUT ANY WARRANTY; without the implied warranty  of  MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License *
 * for more details.                                                       *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA                   *
 ***************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/param.h>	/* for HZ */

#include "version.h"
#include "iostat.h"
#include "common.h"
#include "ioconf.h"


#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif


struct comm_stats  comm_stats[2];
struct io_stats *st_iodev[2];
struct io_hdr_stats *st_hdr_iodev;
struct io_dlist *st_dev_list;

/* Nb of devices and partitions found */
int iodev_nr = 0;

/* Nb of devices entered on the command line */
int dlist_idx = 0;

long interval = 0;
unsigned char timestamp[64];

/*
 * Nb of processors on the machine.
 * A value of 1 means two procs...
 */
int cpu_nr = -1;


/*
 ***************************************************************************
 * Print usage and exit
 ***************************************************************************
 */
void usage(char *progname)
{
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) Sebastien Godard\n"
	           "Usage: %s [ options... ] [ <interval> [ <count> ] ]\n"
		   "Options are:\n"
		   "[ -c | -d ] [ -k | -m ] [ -t ] [ -V ] [ -x ]\n"
		   "[ <device> [ ... ] | ALL ] [ -p [ <device> | ALL ] ]\n"),
	   VERSION, progname);
   exit(1);
}


/*
 ***************************************************************************
 * SIGALRM signal handler
 ***************************************************************************
 */
void alarm_handler(int sig)
{
   signal(SIGALRM, alarm_handler);
   alarm(interval);
}


/*
 ***************************************************************************
 * Initialize stats common structures
 ***************************************************************************
 */
void init_stats(void)
{
   memset(&comm_stats[0], 0, COMM_STATS_SIZE);
   memset(&comm_stats[1], 0, COMM_STATS_SIZE);
}


/*
 ***************************************************************************
 * Set every disk_io entry to inactive state
 ***************************************************************************
 */
void set_entries_inactive(int iodev_nr)
{
   int i;
   struct io_hdr_stats *shi = st_hdr_iodev;

   for (i = 0; i < iodev_nr; i++, shi++)
      shi->active = FALSE;
}


/*
 ***************************************************************************
 * Set structures's state to free for inactive entries
 ***************************************************************************
 */
void free_inactive_entries(int iodev_nr)
{
   int i;
   struct io_hdr_stats *shi = st_hdr_iodev;

   for (i = 0; i < iodev_nr; i++, shi++) {
      if (!shi->active)
	 shi->used = FALSE;
   }
}


/*
 ***************************************************************************
 * Allocate and init I/O devices structures
 ***************************************************************************
 */
void salloc_device(int iodev_nr)
{
   int i;

   for (i = 0; i < 2; i++) {
      if ((st_iodev[i] = (struct io_stats *) malloc(IO_STATS_SIZE * iodev_nr)) == NULL) {
	 perror("malloc");
	 exit(4);
      }
      memset(st_iodev[i], 0, IO_STATS_SIZE * iodev_nr);
   }

   if ((st_hdr_iodev = (struct io_hdr_stats *) malloc(IO_HDR_STATS_SIZE * iodev_nr)) == NULL) {
      perror("malloc");
      exit(4);
   }
   memset(st_hdr_iodev, 0, IO_HDR_STATS_SIZE * iodev_nr);
}


/*
 ***************************************************************************
 * Allocate structures for devices entered on the command line
 ***************************************************************************
 */
void salloc_dev_list(int list_len)
{
   if ((st_dev_list = (struct io_dlist *) malloc(IO_DLIST_SIZE * list_len)) == NULL) {
      perror("malloc");
      exit(4);
   }
   memset(st_dev_list, 0, IO_DLIST_SIZE * list_len);
}


/*
 ***************************************************************************
 * Look for the device in the device list and store it if necessary.
 * Returns the position of the device in the list.
 ***************************************************************************
 */
int update_dev_list(int *dlist_idx, char *device_name)
{
   int i;
   struct io_dlist *sdli = st_dev_list;

   for (i = 0; i < *dlist_idx; i++, sdli++) {
      if (!strcmp(sdli->dev_name, device_name))
	 break;
   }

   if (i == *dlist_idx) {
      /* Device not found: store it */
      (*dlist_idx)++;
      strncpy(sdli->dev_name, device_name, MAX_NAME_LEN - 1);
   }

   return i;
}


/*
 ***************************************************************************
 * Allocate and init structures, according to system state
 ***************************************************************************
 */
void io_sys_init(int *flags)
{
   int i;

   /* Init stat common counters */
   init_stats();

   /* How many processors on this machine ? */
   cpu_nr = get_cpu_nr(~0);

   /* Get number of block devices and partitions in /proc/diskstats */
   if ((iodev_nr = get_diskstats_dev_nr(CNT_PART, CNT_ALL_DEV)) > 0) {
      *flags |= I_F_HAS_DISKSTATS;
      iodev_nr += NR_DEV_PREALLOC;
   }

   if (!HAS_DISKSTATS(*flags) ||
       (DISPLAY_PARTITIONS(*flags) && !DISPLAY_PART_ALL(*flags))) {
      /*
       * If /proc/diskstats exists but we also want stats for the partitions
       * of a particular device, stats will have to be found in /sys. So we
       * need to know if /sys is mounted or not, and set *flags accordingly.
       */

      /* Get number of block devices (and partitions) in sysfs */
      if ((iodev_nr = get_sysfs_dev_nr(DISPLAY_PARTITIONS(*flags))) > 0) {
	 *flags |= I_F_HAS_SYSFS;
	 iodev_nr += NR_DEV_PREALLOC;
      }
      /*
       * Get number of block devices and partitions in /proc/partitions,
       * those with statistics...
       */
      else if ((iodev_nr = get_ppartitions_dev_nr(CNT_PART)) > 0) {
	 *flags |= I_F_HAS_PPARTITIONS;
	 iodev_nr += NR_DEV_PREALLOC;
      }
      /* Get number of "disk_io:" entries in /proc/stat */
      else if ((iodev_nr = get_disk_io_nr()) > 0) {
	 *flags |= I_F_PLAIN_KERNEL24;
	 iodev_nr += NR_DISK_PREALLOC;
      }
      else {
	 /* Assume we have an old kernel: stats for 4 disks are in /proc/stat */
	 iodev_nr = 4;
	 *flags |= I_F_OLD_KERNEL;
      }
   }
   /* Allocate structures for number of disks found */
   salloc_device(iodev_nr);

   if (HAS_OLD_KERNEL(*flags)) {
      struct io_hdr_stats *shi = st_hdr_iodev;
      /*
       * If we have an old kernel with the stats for the first four disks
       * in /proc/stat, then set the devices names to hdisk[0..3].
       */
      for (i = 0; i < 4; i++, shi++) {
	 shi->used = TRUE;
	 sprintf(shi->name, "%s%d", K_HDISK, i);
      }
   }
}


/*
 ***************************************************************************
 * Save stats for current device or partition
 ***************************************************************************
 */
void save_dev_stats(char *dev_name, int curr, struct io_stats *sdev)
{
   int i;
   struct io_hdr_stats *st_hdr_iodev_i;
   struct io_stats *st_iodev_i;

   /* Look for device in data table */
   for (i = 0; i < iodev_nr; i++) {
      st_hdr_iodev_i = st_hdr_iodev + i;
      if (!strcmp(st_hdr_iodev_i->name, dev_name)) {
	 break;
      }
   }
	
   if (i == iodev_nr) {
      /*
       * This is a new device: look for an unused entry to store it.
       * Thus we are able to handle dynamically registered devices.
       */
      for (i = 0; i < iodev_nr; i++) {
	 st_hdr_iodev_i = st_hdr_iodev + i;
	 if (!st_hdr_iodev_i->used) {
	    /* Unused entry found... */
	    st_hdr_iodev_i->used = TRUE;	/* Indicate it is now used */
	    strcpy(st_hdr_iodev_i->name, dev_name);
	    st_iodev_i = st_iodev[!curr] + i;
	    memset(st_iodev_i, 0, IO_STATS_SIZE);
	    break;
	 }
      }
   }
   if (i < iodev_nr) {
      st_hdr_iodev_i = st_hdr_iodev + i;
      st_hdr_iodev_i->active = TRUE;
      st_iodev_i = st_iodev[curr] + i;
      *st_iodev_i = *sdev;
   }
   /* else it was a new device but there was no free structure to store it */
}


/*
 ***************************************************************************
 * Read stats from /proc/stat file...
 * Useful at least for CPU utilization.
 * May be useful to get disk stats if /sys not available.
 ***************************************************************************
 */
void read_proc_stat(int curr, int flags)
{
   FILE *fp;
   char line[8192];
   int pos, i;
   unsigned long v_tmp[4];
   unsigned int v_major, v_index;
   struct io_stats *st_iodev_tmp[4];
   unsigned long long cc_idle, cc_iowait;
   unsigned long long cc_user, cc_nice, cc_system, cc_hardirq, cc_softirq;


   /*
    * Prepare pointers on the 4 disk structures in case we have a
    * /proc/stat file with "disk_rblk", etc. entries.
    */
   for (i = 0; i < 4; i++)
      st_iodev_tmp[i] = st_iodev[curr] + i;

   if ((fp = fopen(STAT, "r")) == NULL) {
      perror("fopen");
      exit(2);
   }

   while (fgets(line, 8192, fp) != NULL) {

      if (!strncmp(line, "cpu ", 4)) {
	 /*
	  * Read the number of jiffies spent in the different modes,
	  * and compute system uptime in jiffies (1/100ths of a second
	  * if HZ=100).
	  * Some fields are only presnt in 2.6 kernels.
	  */
	 comm_stats[curr].cpu_iowait = 0;	/* For pre 2.6 kernels */
	 cc_hardirq = cc_softirq = 0;
	 /* CPU counters became unsigned long long with kernel 2.6.5 */
	 sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu",
	        &(comm_stats[curr].cpu_user), &(comm_stats[curr].cpu_nice),
		&(comm_stats[curr].cpu_system), &(comm_stats[curr].cpu_idle),
		&(comm_stats[curr].cpu_iowait), &cc_hardirq, &cc_softirq);

	 /*
	  * Time spent in system mode also includes time spent servicing
	  * interrupts and softirqs.
	  */
	 comm_stats[curr].cpu_system += cc_hardirq + cc_softirq;
	
	 /*
	  * Compute system uptime in jiffies.
	  * Uptime is multiplied by the number of processors.
	  */
	 comm_stats[curr].uptime = comm_stats[curr].cpu_user   +
	                           comm_stats[curr].cpu_nice +
	                           comm_stats[curr].cpu_system +
	                           comm_stats[curr].cpu_idle +
	                           comm_stats[curr].cpu_iowait;
      }

      else if ((!strncmp(line, "cpu0", 4)) && cpu_nr) {
	 /*
	  * Read CPU line for proc#0 (if available).
	  * Useful to compute uptime reduced to one processor on SMP machines,
	  * with fewer risks to get an overflow...
	  */
	 cc_iowait = cc_hardirq = cc_softirq = 0;
	 sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu",
		&cc_user, &cc_nice, &cc_system, &cc_idle, &cc_iowait,
		&cc_hardirq, &cc_softirq);
	 comm_stats[curr].uptime0 = cc_user + cc_nice + cc_system +
	    			    cc_idle + cc_iowait +
	    			    cc_hardirq + cc_softirq;
      }

      else if (DISPLAY_EXTENDED(flags) || HAS_DISKSTATS(flags) ||
	       HAS_PPARTITIONS(flags) || HAS_SYSFS(flags))
	 /*
	  * When displaying extended statistics, or if /proc/diskstats or
	  * /proc/partitions exists, or /sys is mounted,
	  * we just need to get CPU info from /proc/stat.
	  */
	 continue;

      else if (!strncmp(line, "disk_rblk ", 10)) {
	 /*
	  * Read the number of blocks read from disk.
	  * A block is of indeterminate size.
	  * The size may vary depending on the device type.
	  */
	 sscanf(line + 10, "%lu %lu %lu %lu",
		&v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3]);

	 st_iodev_tmp[0]->dk_drive_rblk = v_tmp[0];
	 st_iodev_tmp[1]->dk_drive_rblk = v_tmp[1];
	 st_iodev_tmp[2]->dk_drive_rblk = v_tmp[2];
	 st_iodev_tmp[3]->dk_drive_rblk = v_tmp[3];
      }

      else if (!strncmp(line, "disk_wblk ", 10)) {
	 /* Read the number of blocks written to disk */
	 sscanf(line + 10, "%lu %lu %lu %lu",
		&v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3]);
	
	 st_iodev_tmp[0]->dk_drive_wblk = v_tmp[0];
	 st_iodev_tmp[1]->dk_drive_wblk = v_tmp[1];
	 st_iodev_tmp[2]->dk_drive_wblk = v_tmp[2];
	 st_iodev_tmp[3]->dk_drive_wblk = v_tmp[3];
      }

      else if (!strncmp(line, "disk ", 5)) {
	 /* Read the number of I/O done since the last reboot */
	 sscanf(line + 5, "%lu %lu %lu %lu",
		&v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3]);
	
	 st_iodev_tmp[0]->dk_drive = v_tmp[0];
	 st_iodev_tmp[1]->dk_drive = v_tmp[1];
	 st_iodev_tmp[2]->dk_drive = v_tmp[2];
	 st_iodev_tmp[3]->dk_drive = v_tmp[3];
      }

      else if (!strncmp(line, "disk_io: ", 9)) {
	 struct io_stats sdev;
	 char dev_name[MAX_NAME_LEN];
	
	 pos = 9;

	 /* Every disk_io entry is potentially unregistered */
	 set_entries_inactive(iodev_nr);
	
	 /* Read disks I/O statistics (for 2.4 kernels) */
	 while (pos < strlen(line) - 1) {
	    /* Beware: a CR is already included in the line */
	    sscanf(line + pos, "(%u,%u):(%lu,%*u,%lu,%*u,%lu) ",
		   &v_major, &v_index, &v_tmp[0], &v_tmp[1], &v_tmp[2]);

	    sprintf(dev_name, "dev%d-%d", v_major, v_index);
	    sdev.dk_drive      = v_tmp[0];
	    sdev.dk_drive_rblk = v_tmp[1];
	    sdev.dk_drive_wblk = v_tmp[2];
	    save_dev_stats(dev_name, curr, &sdev);

	    pos += strcspn(line + pos, " ") + 1;
	 }

	 /* Free structures corresponding to unregistered disks */
	 free_inactive_entries(iodev_nr);
      }
   }

   fclose(fp);
}


/*
 ***************************************************************************
 * Read sysfs stat for current block device or partition
 ***************************************************************************
 */
int read_sysfs_file_stat(int curr, char *filename, char *dev_name,
			  int dev_type)
{
   FILE *fp;
   struct io_stats sdev;
   int i;

   /* Try to read given stat file */
   if ((fp = fopen(filename, "r")) == NULL)
      return 0;
	
   if (dev_type == DT_DEVICE)
      i = (fscanf(fp, "%lu %lu %llu %lu %lu %lu %llu %lu %lu %lu %lu",
		  &sdev.rd_ios, &sdev.rd_merges,
		  &sdev.rd_sectors, &sdev.rd_ticks,
		  &sdev.wr_ios, &sdev.wr_merges,
		  &sdev.wr_sectors, &sdev.wr_ticks,
		  &sdev.ios_pgr, &sdev.tot_ticks, &sdev.rq_ticks) == 11);
   else
      i = (fscanf(fp, "%lu %llu %lu %llu",
		  &sdev.rd_ios, &sdev.rd_sectors,
		  &sdev.wr_ios, &sdev.wr_sectors) == 4);

   if (i)
      save_dev_stats(dev_name, curr, &sdev);

   fclose(fp);

   return 1;
}


/*
 ***************************************************************************
 * Read sysfs stats for all the partitions of a device
 ***************************************************************************
 */
void read_sysfs_dlist_part_stat(int curr, char *dev_name)
{
   DIR *dir;
   struct dirent *drd;
   char dfile[MAX_PF_NAME], filename[MAX_PF_NAME];

   sprintf(dfile, "%s/%s", SYSFS_BLOCK, dev_name);

   /* Open current device directory in /sys/block */
   if ((dir = opendir(dfile)) == NULL)
      return;

   /* Get current entry */
   while ((drd = readdir(dir)) != NULL) {
      if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
	 continue;
      sprintf(filename, "%s/%s/%s", dfile, drd->d_name, S_STAT);

      /* Read current partition stats */
      read_sysfs_file_stat(curr, filename, drd->d_name, DT_PARTITION);
   }

   /* Close device directory */
   closedir(dir);
}


/*
 ***************************************************************************
 * Read stats from the sysfs filesystem
 * for the devices entered on the command line
 ***************************************************************************
 */
void read_sysfs_dlist_stat(int curr, int flags)
{
   int dev, ok;
   char filename[MAX_PF_NAME];
   struct io_dlist *st_dev_list_i;

   /* Every I/O device (or partition) is potentially unregistered */
   set_entries_inactive(iodev_nr);

   for (dev = 0; dev < dlist_idx; dev++) {
      st_dev_list_i = st_dev_list + dev;
      sprintf(filename, "%s/%s/%s",
	      SYSFS_BLOCK, st_dev_list_i->dev_name, S_STAT);

      /* Read device stats */
      ok = read_sysfs_file_stat(curr, filename, st_dev_list_i->dev_name, DT_DEVICE);

      if (ok && st_dev_list_i->disp_part)
	 /* Also read stats for its partitions */
	 read_sysfs_dlist_part_stat(curr, st_dev_list_i->dev_name);
   }

   /* Free structures corresponding to unregistered devices */
   free_inactive_entries(iodev_nr);
}


/*
 ***************************************************************************
 * Read stats from the sysfs filesystem
 * for every block devices found
 ***************************************************************************
 */
void read_sysfs_stat(int curr, int flags)
{
   DIR *dir;
   struct dirent *drd;
   char filename[MAX_PF_NAME];
   int ok;

   /* Every I/O device entry is potentially unregistered */
   set_entries_inactive(iodev_nr);

   /* Open /sys/block directory */
   if ((dir = opendir(SYSFS_BLOCK)) != NULL) {

      /* Get current entry */
      while ((drd = readdir(dir)) != NULL) {
	 if (!strcmp(drd->d_name, ".") || !strcmp(drd->d_name, ".."))
	    continue;
	 sprintf(filename, "%s/%s/%s", SYSFS_BLOCK, drd->d_name, S_STAT);
	
	 /* If current entry is a directory, try to read its stat file */
	 ok = read_sysfs_file_stat(curr, filename, drd->d_name, DT_DEVICE);
	
	 /*
	  * If '-p ALL' was entered on the command line,
	  * also try to read stats for its partitions
	  */
	 if (ok && DISPLAY_PART_ALL(flags))
	    read_sysfs_dlist_part_stat(curr, drd->d_name);
      }

      /* Close /sys/block directory */
      closedir(dir);
   }

   /* Free structures corresponding to unregistered devices */
   free_inactive_entries(iodev_nr);
}


/*
 ***************************************************************************
 * Read stats from /proc/diskstats
 ***************************************************************************
 */
void read_diskstats_stat(int curr, int flags)
{
   FILE *fp;
   char line[256], dev_name[MAX_NAME_LEN];
   struct io_stats sdev;
   int i;
   unsigned long rd_ios, rd_merges_or_rd_sec, rd_ticks_or_wr_sec, wr_ios;
   unsigned long ios_pgr, tot_ticks, rq_ticks, wr_merges, wr_ticks;
   unsigned long long rd_sec_or_wr_ios, wr_sec;
   char *ioc_dname;
   unsigned int major, minor;

   /* Every I/O device entry is potentially unregistered */
   set_entries_inactive(iodev_nr);

   if ((fp = fopen(DISKSTATS, "r")) == NULL)
      return;

   while (fgets(line, 256, fp) != NULL) {

      /* major minor name rio rmerge rsect ruse wio wmerge wsect wuse running use aveq */
      i = sscanf(line, "%u %u %s %lu %lu %llu %lu %lu %lu %llu %lu %lu %lu %lu",
		 &major, &minor, dev_name,
		 &rd_ios, &rd_merges_or_rd_sec, &rd_sec_or_wr_ios, &rd_ticks_or_wr_sec,
		 &wr_ios, &wr_merges, &wr_sec, &wr_ticks, &ios_pgr, &tot_ticks, &rq_ticks);

      if (i == 14) {
	 /* Device */
	 sdev.rd_ios     = rd_ios;
	 sdev.rd_merges  = rd_merges_or_rd_sec;
	 sdev.rd_sectors = rd_sec_or_wr_ios;
	 sdev.rd_ticks   = rd_ticks_or_wr_sec;
	 sdev.wr_ios     = wr_ios;
	 sdev.wr_merges  = wr_merges;
	 sdev.wr_sectors = wr_sec;
	 sdev.wr_ticks   = wr_ticks;
	 sdev.ios_pgr    = ios_pgr;
	 sdev.tot_ticks  = tot_ticks;
	 sdev.rq_ticks   = rq_ticks;
      }
      else if (i == 7) {
	 /* Partition */
	 if (DISPLAY_EXTENDED(flags) || (!dlist_idx && !DISPLAY_PARTITIONS(flags)))
	    continue;

	 sdev.rd_ios     = rd_ios;
	 sdev.rd_sectors = rd_merges_or_rd_sec;
	 sdev.wr_ios     = rd_sec_or_wr_ios;
	 sdev.wr_sectors = rd_ticks_or_wr_sec;
      }
      else
	 /* Unknown entry: ignore it */
	 continue;

      if ((ioc_dname = ioc_name(major, minor)) != NULL) {
	 if (strcmp(dev_name, ioc_dname) && strcmp(ioc_dname, K_NODEV))
	    /*
	     * No match: Use name generated from sysstat.ioconf data (if different
	     * from "nodev") works around known issues with EMC PowerPath.
	     */
	    strcpy(dev_name, ioc_dname);
      }

      save_dev_stats(dev_name, curr, &sdev);
   }

   fclose(fp);

   /* Free structures corresponding to unregistered devices */
   free_inactive_entries(iodev_nr);
}


/*
 ***************************************************************************
 * Read stats from /proc/partitions
 ***************************************************************************
 */
void read_ppartitions_stat(int curr, int flags)
{
   FILE *fp;
   char line[256], dev_name[MAX_NAME_LEN];
   struct io_stats sdev;
   unsigned long rd_ios, rd_merges, rd_ticks, wr_ios, wr_merges, wr_ticks;
   unsigned long ios_pgr, tot_ticks, rq_ticks;
   unsigned long long rd_sec, wr_sec;
   char *ioc_dname;
   unsigned int major, minor;

   /* Every I/O device entry is potentially unregistered */
   set_entries_inactive(iodev_nr);

   if ((fp = fopen(PPARTITIONS, "r")) == NULL)
      return;

   while (fgets(line, 256, fp) != NULL) {
      /* major minor #blocks name rio rmerge rsect ruse wio wmerge wsect wuse running use aveq */
      if (sscanf(line, "%u %u %*u %s %lu %lu %llu %lu %lu %lu %llu"
		       " %lu %lu %lu %lu",
		 &major, &minor, dev_name,
		 &rd_ios, &rd_merges, &rd_sec, &rd_ticks, &wr_ios, &wr_merges,
		 &wr_sec, &wr_ticks, &ios_pgr, &tot_ticks, &rq_ticks) == 14) {
	 /* Device or partition */
	 sdev.rd_ios     = rd_ios;  sdev.rd_merges = rd_merges;
	 sdev.rd_sectors = rd_sec;  sdev.rd_ticks  = rd_ticks;
	 sdev.wr_ios     = wr_ios;  sdev.wr_merges = wr_merges;
	 sdev.wr_sectors = wr_sec;  sdev.wr_ticks  = wr_ticks;
	 sdev.ios_pgr    = ios_pgr; sdev.tot_ticks = tot_ticks;
	 sdev.rq_ticks   = rq_ticks;
      }
      else
	 /* Unknown entry: ignore it */
	 continue;

      if ((ioc_dname = ioc_name(major, minor)) != NULL) {
	 if (strcmp(dev_name, ioc_dname) && strcmp(ioc_dname, K_NODEV))
	    /* Compensate for EMC PowerPath driver bug */
	    strcpy(dev_name, ioc_dname);
      }

      save_dev_stats(dev_name, curr, &sdev);
   }

   fclose(fp);

   /* Free structures corresponding to unregistered devices */
   free_inactive_entries(iodev_nr);
}


/*
 ***************************************************************************
 * Display CPU utilization
 ***************************************************************************
 */
void write_cpu_stat(int curr, unsigned long long itv)
{
   printf("avg-cpu:  %%user   %%nice %%system %%iowait   %%idle\n");

   printf("         %6.2f  %6.2f  %6.2f  %6.2f  %6.2f\n\n",
	  ll_sp_value(comm_stats[!curr].cpu_user,   comm_stats[curr].cpu_user,   itv),
	  ll_sp_value(comm_stats[!curr].cpu_nice,   comm_stats[curr].cpu_nice,   itv),
	  ll_sp_value(comm_stats[!curr].cpu_system, comm_stats[curr].cpu_system, itv),
	  ll_sp_value(comm_stats[!curr].cpu_iowait, comm_stats[curr].cpu_iowait, itv),
	  (comm_stats[curr].cpu_idle < comm_stats[!curr].cpu_idle) ?
	  0.0 :
	  ll_sp_value(comm_stats[!curr].cpu_idle, comm_stats[curr].cpu_idle, itv));
}


/*
 ***************************************************************************
 * Display stats header
 ***************************************************************************
 */
void write_stat_header(int flags, int *fctr)
{
   if (DISPLAY_EXTENDED(flags)) {
      /* Extended stats */
      printf("Device:    rrqm/s wrqm/s   r/s   w/s  rsec/s  wsec/s");
      if (DISPLAY_MEGABYTES(flags)) {
	 printf("    rMB/s    wMB/s");
	 *fctr = 2048;
      }
      else {
	 printf("    rkB/s    wkB/s");
	 *fctr = 2;
      }
      printf(" avgrq-sz avgqu-sz   await  svctm  %%util\n");
   }
   else {
      /* Basic stats */
      printf("Device:            tps");
      if (DISPLAY_KILOBYTES(flags)) {
	 printf("    kB_read/s    kB_wrtn/s    kB_read    kB_wrtn\n");
	 *fctr = 2;
      }
      else if (DISPLAY_MEGABYTES(flags)) {
	 printf("    MB_read/s    MB_wrtn/s    MB_read    MB_wrtn\n");
	 *fctr = 2048;
      }
      else
	 printf("   Blk_read/s   Blk_wrtn/s   Blk_read   Blk_wrtn\n");
   }
}


/*
 ***************************************************************************
 * Display extended stats, read from /proc/{diskstats,partitions} or /sys
 ***************************************************************************
 */
void write_ext_stat(int curr, unsigned long long itv, int flags, int fctr,
		    struct io_hdr_stats *shi, struct io_stats *ioi,
		    struct io_stats *ioj)
{
   unsigned long long rd_sec, wr_sec;
   double tput, util, await, svctm, arqsz, nr_ios;
	
   /*
    * Counters overflows are possible, but don't need to be handled in
    * a special way: the difference is still properly calculated if the
    * result is of the same type as the two values.
    * Exception is field rq_ticks which is incremented by the number of
    * I/O in progress times the number of milliseconds spent doing I/O.
    * But the number of I/O in progress (field ios_pgr) happens to be
    * sometimes negative...
    */
   nr_ios = (ioi->rd_ios - ioj->rd_ios) + (ioi->wr_ios - ioj->wr_ios);
   tput   = ((double) nr_ios) * HZ / itv;
   util = S_VALUE(ioj->tot_ticks, ioi->tot_ticks, itv);
   svctm  = tput ? util / tput : 0.0;
   /*
    * kernel gives ticks already in milliseconds for all platforms
    * => no need for further scaling.
    */
   await  = nr_ios ?
      ((ioi->rd_ticks - ioj->rd_ticks) + (ioi->wr_ticks - ioj->wr_ticks)) /
      nr_ios : 0.0;

   rd_sec = ioi->rd_sectors - ioj->rd_sectors;
   if ((ioi->rd_sectors < ioj->rd_sectors) && (ioj->rd_sectors <= 0xffffffff))
      rd_sec &= 0xffffffff;
   wr_sec = ioi->wr_sectors - ioj->wr_sectors;
   if ((ioi->wr_sectors < ioj->wr_sectors) && (ioj->wr_sectors <= 0xffffffff))
      wr_sec &= 0xffffffff;

   arqsz  = nr_ios ? (rd_sec + wr_sec) / nr_ios : 0.0;

   printf("%-10s", shi->name);
   if (strlen(shi->name) > 10)
      printf("\n          ");
   /*       rrq/s wrq/s   r/s   w/s  rsec  wsec   r?B   w?B  rqsz  qusz await svctm %util */
   printf(" %6.2f %6.2f %5.2f %5.2f %7.2f %7.2f %8.2f %8.2f %8.2f %8.2f %7.2f %6.2f %6.2f\n",
	  S_VALUE(ioj->rd_merges, ioi->rd_merges, itv),
	  S_VALUE(ioj->wr_merges, ioi->wr_merges, itv),
	  S_VALUE(ioj->rd_ios, ioi->rd_ios, itv),
	  S_VALUE(ioj->wr_ios, ioi->wr_ios, itv),
	  ll_s_value(ioj->rd_sectors, ioi->rd_sectors, itv),
	  ll_s_value(ioj->wr_sectors, ioi->wr_sectors, itv),
	  ll_s_value(ioj->rd_sectors, ioi->rd_sectors, itv) / fctr,
	  ll_s_value(ioj->wr_sectors, ioi->wr_sectors, itv) / fctr,
	  arqsz,
	  S_VALUE(ioj->rq_ticks, ioi->rq_ticks, itv) / 1000.0,
	  await,
	  /* The ticks output is biased to output 1000 ticks per second */
	  svctm,
	  /* Again: ticks in milliseconds */
	  util / 10.0);
}


/*
 ***************************************************************************
 * Write basic stats, read from /proc/stat, /proc/{diskstats,partitions}
 * or from sysfs
 ***************************************************************************
 */
void write_basic_stat(int curr, unsigned long long itv, int flags, int fctr,
		      struct io_hdr_stats *shi, struct io_stats *ioi,
		      struct io_stats *ioj)
{
   unsigned long long rd_sec, wr_sec;

   printf("%-13s", shi->name);
   if (strlen(shi->name) > 13)
      printf("\n             ");

   if (HAS_SYSFS(flags) ||
       HAS_DISKSTATS(flags) || HAS_PPARTITIONS(flags)) {
      /* Print stats coming from /sys or /proc/{diskstats,partitions} */
      rd_sec = ioi->rd_sectors - ioj->rd_sectors;
      if ((ioi->rd_sectors < ioj->rd_sectors) && (ioj->rd_sectors <= 0xffffffff))
	 rd_sec &= 0xffffffff;
      wr_sec = ioi->wr_sectors - ioj->wr_sectors;
      if ((ioi->wr_sectors < ioj->wr_sectors) && (ioj->wr_sectors <= 0xffffffff))
	 wr_sec &= 0xffffffff;

      printf(" %8.2f %12.2f %12.2f %10llu %10llu\n",
	     S_VALUE(ioj->rd_ios + ioj->wr_ios, ioi->rd_ios + ioi->wr_ios, itv),
	     ll_s_value(ioj->rd_sectors, ioi->rd_sectors, itv) / fctr,
	     ll_s_value(ioj->wr_sectors, ioi->wr_sectors, itv) / fctr,
	     (unsigned long long) rd_sec / fctr,
	     (unsigned long long) wr_sec / fctr);
   }
   else {
      /* Print stats coming from /proc/stat */
      printf(" %8.2f %12.2f %12.2f %10lu %10lu\n",
	     S_VALUE(ioj->dk_drive, ioi->dk_drive, itv),
	     S_VALUE(ioj->dk_drive_rblk, ioi->dk_drive_rblk, itv) / fctr,
	     S_VALUE(ioj->dk_drive_wblk, ioi->dk_drive_wblk, itv) / fctr,
	     (ioi->dk_drive_rblk - ioj->dk_drive_rblk) / fctr,
	     (ioi->dk_drive_wblk - ioj->dk_drive_wblk) / fctr);
   }
}


/*
 ***************************************************************************
 * Print everything now (stats and uptime)
 ***************************************************************************
 */
int write_stat(int curr, int flags, struct tm *loc_time)
{
   int dev, i, fctr = 1;
   unsigned long long itv;
   struct io_hdr_stats *shi = st_hdr_iodev;
   struct io_stats *ioi, *ioj;
   struct io_dlist *st_dev_list_i;

   /*
    * Under very special circumstances, STDOUT may become unavailable,
    * This is what we try to guess here
    */
   if (write(STDOUT_FILENO, "", 0) == -1) {
      perror("stdout");
      exit(6);
   }

   /* Print time stamp */
   if (DISPLAY_TIMESTAMP(flags)) {
      strftime(timestamp, sizeof(timestamp), "%X", loc_time);
      printf(_("Time: %s\n"), timestamp);
   }

   /*
    * itv is multiplied by the number of processors.
    * This is OK to compute CPU usage since the number of jiffies spent in the
    * different modes (user, nice, etc.) is the sum of all the processors.
    * But itv should be reduced to one processor before displaying disk
    * utilization.
    */
   if (!comm_stats[!curr].uptime)
      /*
       * This is the first report displaying stats since system startup.
       * Only in this case we admit that the interval may be greater
       * than 0xffffffff, else it was an overflow.
       */
      itv = comm_stats[curr].uptime;
   else
      /* uptime in jiffies */
      itv = (comm_stats[curr].uptime - comm_stats[!curr].uptime)
	 & 0xffffffff;
   if (!itv)
      itv = 1;

   if (!DISPLAY_DISK_ONLY(flags))
      /* Display CPU utilization */
      write_cpu_stat(curr, itv);

   if (cpu_nr) {
      /* On SMP machines, reduce itv to one processor (see note above) */
      if (!comm_stats[!curr].uptime0)
	 itv = comm_stats[curr].uptime0;
      else
	 itv = (comm_stats[curr].uptime0 - comm_stats[!curr].uptime0)
	    & 0xffffffff;
      if (!itv)
	 itv = 1;
   }

   if (!DISPLAY_CPU_ONLY(flags)) {

      /* Display stats header */
      write_stat_header(flags, &fctr);

      if (DISPLAY_EXTENDED(flags) &&
	  (HAS_OLD_KERNEL(flags) || HAS_PLAIN_KERNEL24(flags))) {
	 /* No extended stats with old 2.2-2.4 kernels */
	 printf("\n");
	 return 1;
      }

      for (i = 0; i < iodev_nr; i++, shi++) {
	 if (shi->used) {
	
	    if (dlist_idx && !HAS_SYSFS(flags)) {
	       /*
		* With sysfs, only stats for the requested devices are read.
		* With /proc/{diskstats,partitions}, stats for every devices
		* are read. Thus we need to check if stats for current device
		* are to be displayed.
		*/
	       for (dev = 0; dev < dlist_idx; dev++) {
		  st_dev_list_i = st_dev_list + dev;
		  if (!strcmp(shi->name, st_dev_list_i->dev_name))
		     break;
	       }
	       if (dev == dlist_idx)
		  /* Device not found in list: don't display it */
		  continue;
	    }
	
	    ioi = st_iodev[curr] + i;
	    ioj = st_iodev[!curr] + i;

	    if (!DISPLAY_UNFILTERED(flags)) {
	       if (HAS_OLD_KERNEL(flags) ||
		   HAS_PLAIN_KERNEL24(flags)) {
		  if (!ioi->dk_drive)
		     continue;
	       }
	       else {
		  if (!ioi->rd_ios && !ioi->wr_ios)
		     continue;
	       }
	    }

	    if (DISPLAY_EXTENDED(flags))
	       write_ext_stat(curr, itv, flags, fctr, shi, ioi, ioj);
	    else
	       write_basic_stat(curr, itv, flags, fctr, shi, ioi, ioj);
	 }
      }
      printf("\n");
   }
   return 1;
}


/*
 ***************************************************************************
 * Main loop: read I/O stats from the relevant sources,
 * and display them.
 ***************************************************************************
 */
void rw_io_stat_loop(int flags, long int count, struct tm *loc_time)
{
   int curr = 1;
   int next;

   do {
      /* Read kernel statistics (CPU, and possibly disks for old kernels) */
      read_proc_stat(curr, flags);

      if (dlist_idx) {
	 /*
	  * A device or partition name was entered on the command line,
	  * with or without -p option (but not -p ALL).
	  */
	 if (HAS_DISKSTATS(flags) && !DISPLAY_PARTITIONS(flags))
	    read_diskstats_stat(curr, flags);
	 else if (HAS_SYSFS(flags))
	    read_sysfs_dlist_stat(curr, flags);
	 else if (HAS_PPARTITIONS(flags) && !DISPLAY_PARTITIONS(flags))
	    read_ppartitions_stat(curr, flags);
      }
      else {
	 /*
	  * No devices nor partitions entered on the command line
	  * (for example if -p ALL was used).
	  */
	 if (HAS_DISKSTATS(flags))
	    read_diskstats_stat(curr, flags);
	 else if (HAS_SYSFS(flags))
	    read_sysfs_stat(curr,flags);
	 else if (HAS_PPARTITIONS(flags))
	    read_ppartitions_stat(curr, flags);
      }

      /* Save time */
      get_localtime(loc_time);

      /* Print results */
      if ((next = write_stat(curr, flags, loc_time))
	  && (count > 0))
	 count--;
      fflush(stdout);

      if (count) {
	 pause();

	 if (next)
	    curr ^= 1;
      }
   }
   while (count);
}


/*
 ***************************************************************************
 * Main entry to the iostat program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
   int it = 0, flags = 0;
   int opt = 1;
   int i;
   long count = 1;
   struct utsname header;
   struct io_dlist *st_dev_list_i;
   struct tm loc_time;

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   /* Allocate structures for device list */
   if (argc > 1)
      salloc_dev_list(argc - 1);

   /* Process args... */
   while (opt < argc) {

      if (!strcmp(argv[opt], "-p")) {
	 flags |= I_D_PARTITIONS;
	 if (argv[++opt] && (strspn(argv[opt], DIGITS) != strlen(argv[opt])) &&
	     (strncmp(argv[opt], "-", 1))) {
	    flags |= I_D_UNFILTERED;
	    if (!strcmp(argv[opt], K_ALL)) {
	       flags |= I_D_PART_ALL;
	       opt++;
	    }
	    else {
	       /* Store device name */
	       i = update_dev_list(&dlist_idx, device_name(argv[opt++]));
	       st_dev_list_i = st_dev_list + i;
	       st_dev_list_i->disp_part = TRUE;
	    }
	 }
	 else
	    flags |= I_D_PART_ALL;
      }

      else if (!strncmp(argv[opt], "-", 1)) {
	 for (i = 1; *(argv[opt] + i); i++) {

	    switch (*(argv[opt] + i)) {

	     case 'c':
	       flags |= I_D_CPU_ONLY;	/* Display cpu usage only */
	       flags &= ~I_D_DISK_ONLY;
	       break;

	     case 'd':
	       flags |= I_D_DISK_ONLY;	/* Display disk utilization only */
	       flags &= ~I_D_CPU_ONLY;
	       break;
	
	     case 'k':
	       if (DISPLAY_MEGABYTES(flags))
		  usage(argv[0]);
	       flags |= I_D_KILOBYTES;	/* Display stats in kB/s */
	       break;

	     case 'm':
	       if (DISPLAY_KILOBYTES(flags))
		  usage(argv[0]);
	       flags |= I_D_MEGABYTES;	/* Display stats in MB/s */
	       break;

	     case 't':
	       flags |= I_D_TIMESTAMP;	/* Display timestamp */
	       break;
	
	     case 'x':
	       flags |= I_D_EXTENDED;	/* Display extended stats */
	       break;

	     case 'V':			/* Print usage and exit	*/
	     default:
	       usage(argv[0]);
	    }
	 }
	 opt++;
      }

      else if (!isdigit(argv[opt][0])) {
	 flags |= I_D_UNFILTERED;
	 if (strcmp(argv[opt], K_ALL))
	    /* Store device name */
	    update_dev_list(&dlist_idx, device_name(argv[opt++]));
	 else
	    opt++;
      }

      else if (!it) {
	 interval = atol(argv[opt++]);
	 if (interval < 1)
 	   usage(argv[0]);
	 count = -1;
	 it = 1;
      }

      else {
	 count = atol(argv[opt++]);
	 if (count < 1)
	   usage(argv[0]);
      }
   }

   /* Linux does not provide extended stats for partitions */
   if (DISPLAY_PARTITIONS(flags) && DISPLAY_EXTENDED(flags)) {
      fprintf(stderr, _("-x and -p options are mutually exclusive\n"));
      exit(1);
   }

   /* Ignore device list if '-p ALL' entered on the command line */
   if (DISPLAY_PART_ALL(flags))
      dlist_idx = 0;

   /* Init structures according to machine architecture */
   io_sys_init(&flags);

   get_localtime(&loc_time);

   /* Get system name, release number and hostname */
   uname(&header);
   print_gal_header(&loc_time,
		    header.sysname, header.release, header.nodename);
   printf("\n");

   /* Set a handler for SIGALRM */
   alarm_handler(0);

   /* Main loop */
   rw_io_stat_loop(flags, count, &loc_time);

   return 0;
}
