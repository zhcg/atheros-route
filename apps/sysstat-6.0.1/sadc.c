/*
 * sadc: system activity data collector
 * (C) 1999-2005 by Sebastien GODARD (sysstat <at> wanadoo.fr)
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
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "version.h"
#include "sa.h"
#include "common.h"
#include "ioconf.h"


#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

/* Nb of processors on the machine. A value of 1 means two processors */
int cpu_nr = -1;
unsigned int serial_nr = 0, iface_nr = 0, irqcpu_nr = 0, disk_nr = 0;
int pid_nr = 0;
unsigned int sadc_actflag;
long interval = 0;
int kb_shift = 0;

struct file_hdr file_hdr;
struct file_stats file_stats;
struct stats_one_cpu *st_cpu = NULL;
struct stats_serial *st_serial = NULL;
struct stats_net_dev *st_net_dev = NULL;
struct stats_irq_cpu *st_irq_cpu = NULL;
struct disk_stats *st_disk = NULL;
struct pid_stats *pid_stats[MAX_PID_NR];

unsigned long all_pids[MAX_PID_NR];
unsigned char f_pids[MAX_PID_NR];
unsigned int interrupts[NR_IRQS];
unsigned int u_tmp[NR_DISKS - 1];


/*
 ***************************************************************************
 * Print usage and exit
 ***************************************************************************
 */
void usage(char *progname)
{
   /*
    * Don't show options like -x ALL or -X SELF.
    * They should only be used with sar.
    */
   fprintf(stderr, _("sysstat version %s\n"
		   "(C) Sebastien Godard\n"
	           "Usage: %s [ options... ] [ <interval> [ <count> ] ] [ <outfile> ]\n"
		   "Options are:\n"
		   "[ -d ] [ -F ] [ -I ] [ -V ]\n"),
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
 * Allocate pid_stats structures
 ***************************************************************************
 */
void salloc_pid(int nb_pid)
{
   int pid;

   if ((pid_stats[0] = (struct pid_stats *) malloc(PID_STATS_SIZE * nb_pid)) == NULL) {
      perror("malloc");
      exit(4);
   }

   memset(pid_stats[0], 0, PID_STATS_SIZE * nb_pid);

   for (pid = 1; pid < nb_pid; pid++)
      /* Structures are aligned but also padded. Thus array members are packed */
      pid_stats[pid] = pid_stats[0] + pid;	/* Assume nb_pid <= MAX_PID_NR */
}


/*
 ***************************************************************************
 * Display an error message
 ***************************************************************************
 */
void p_write_error(void)
{
    fprintf(stderr, _("Cannot write data to system activity file: %s\n"),
	    strerror(errno));
    exit(2);
}


/*
 ***************************************************************************
 * Init dk_drive* counters (used for sar -b)
 ***************************************************************************
 */
void init_dk_drive_stat(void)
{
   file_stats.dk_drive = 0;
   file_stats.dk_drive_rio  = file_stats.dk_drive_wio  = 0;
   file_stats.dk_drive_rblk = file_stats.dk_drive_wblk = 0;
}



/*
 ***************************************************************************
 * Insert PID number into the list if not already there and if possible.
 * Return the index in the list, or MAX_PID_NR if PID could not be inserted.
 ***************************************************************************
 */
int save_pid(unsigned long pid)
{
   int i = 0;

   while ((i < pid_nr) && (all_pids[i] != pid))
      i++;
   /* PID not found: insert it if possible */
   if ((i == pid_nr) && (pid_nr < MAX_PID_NR))
      all_pids[pid_nr++] = pid;

   return i;
}


/*
 ***************************************************************************
 * Set PID flag value (bit 0 set: -x, bit 1 set: -X)
 ***************************************************************************
 */
void set_pflag(int child, unsigned long pid)
{
   int i = 0, flag;

   if (child)
      flag = X_PPID_SET;
   else
      flag = X_PID_SET;

   if (!pid) {
      for (i = 0; i < MAX_PID_NR; i++)
	 f_pids[i] |= flag;
   }
   else {
      if ((i = save_pid(pid)) < MAX_PID_NR)
	 f_pids[i] |= flag;
   }
}


/*
 ***************************************************************************
 * Look for all the PIDs
 ***************************************************************************
 */
void get_pid_list(void)
{
   DIR *dir;
   struct dirent *drp;

   /* Open /proc directory */
   if ((dir = opendir(PROC)) == NULL) {
      perror("opendir");
      exit(4);
   }

   /* Get directory entries */
   while ((drp = readdir(dir)) != NULL) {
      if (isdigit(drp->d_name[0])) {
	 /* Save PID in th list */
	 if (save_pid(atol(drp->d_name)) == MAX_PID_NR)
	    break;
      }
   }

   /* Close /proc directory */
   closedir(dir);
}


/*
 ***************************************************************************
 * Find number of serial lines that support tx/rx accounting
 * in /proc/tty/driver/serial file.
 ***************************************************************************
 */
unsigned int get_serial_lines(void)
#ifdef SMP_RACE
{
   /*
    * Ignore serial lines if SMP_RACE flag is defined.
    * This is because there is an SMP race in some 2.2.x kernels that
    * may be triggered when reading the /proc/tty/driver/serial file.
    */
   return 0;
}
#else
{
   FILE *fp;
   char line[256];
   unsigned int sl = 0;

   if ((fp = fopen(SERIAL, "r")) == NULL)
      return 0;	/* No SERIAL file */

   while (fgets(line, 256, fp) != NULL) {
      /*
       * tx/rx statistics are always present,
       * except when serial line is unknown.
       */
      if (strstr(line, "tx:") != NULL)
	 sl++;
   }

   fclose(fp);

   return (sl + NR_SERIAL_PREALLOC);
}
#endif


/*
 ***************************************************************************
 * Find number of interfaces (network devices) that are in /proc/net/dev
 * file
 ***************************************************************************
 */
unsigned int get_net_dev(void)
{
   FILE *fp;
   char line[128];
   unsigned int dev = 0;

   if ((fp = fopen(NET_DEV, "r")) == NULL)
      return 0;	/* No network device file */

   while (fgets(line, 128, fp) != NULL) {
      if (strchr(line, ':'))
	 dev++;
   }

   fclose(fp);

   return (dev + NR_IFACE_PREALLOC);
}


/*
 ***************************************************************************
 * Find number of interrupts available per processor (use
 * /proc/interrupts file). Called on SMP machines only.
 ***************************************************************************
 */
unsigned int get_irqcpu_nb(unsigned int max_nr_irqcpu)
{
   FILE *fp;
   char line[256];
   unsigned int irq = 0;

   if ((fp = fopen(INTERRUPTS, "r")) == NULL)
      return 0;	/* No interrupts file */

   while ((fgets(line, 256, fp) != NULL) && (irq < max_nr_irqcpu)) {
      if (isdigit(line[2]))
	 irq++;
   }

   fclose(fp);

   return (irq + NR_IRQPROC_PREALLOC);
}


/*
 ***************************************************************************
 * Allocate and init structures, according to system state
 ***************************************************************************
 */
void sa_sys_init(unsigned int *flags)
{
   /* How many processors on this machine ? */
   if ((cpu_nr = get_cpu_nr(NR_CPUS)) > 0)
      SREALLOC(st_cpu, struct stats_one_cpu, STATS_ONE_CPU_SIZE * (cpu_nr + 1));

   /* Get serial lines that support accounting */
   if ((serial_nr = get_serial_lines())) {
      sadc_actflag |= A_SERIAL;
      SREALLOC(st_serial, struct stats_serial, STATS_SERIAL_SIZE * serial_nr);
   }
   /* Get number of interrupts available per processor */
   if (cpu_nr > 0) {
      if ((irqcpu_nr = get_irqcpu_nb(NR_IRQS)))
	 SREALLOC(st_irq_cpu, struct stats_irq_cpu,
		  STATS_IRQ_CPU_SIZE * (cpu_nr + 1) * irqcpu_nr);
   }
   else
      /* IRQ per processor are not provided by sadc on UP machines */
      irqcpu_nr = 0;

   /* Get number of network devices (interfaces) */
   if ((iface_nr = get_net_dev())) {
      sadc_actflag |= A_NET_DEV + A_NET_EDEV;
      SREALLOC(st_net_dev, struct stats_net_dev, STATS_NET_DEV_SIZE * iface_nr);
   }
   /*
    * Get number of devices in /proc/{diskstats,partitions}
    * or number of disk_io entries in /proc/stat.
    * Alwyays done, since disk stats must be read at least for sar -b
    * if not for sar -d.
    */
   if ((disk_nr = get_diskstats_dev_nr(CNT_DEV, CNT_USED_DEV)) > 0) {
      *flags |= S_F_HAS_DISKSTATS;
      disk_nr += NR_DISK_PREALLOC;
      SREALLOC(st_disk, struct disk_stats, DISK_STATS_SIZE * disk_nr);
   }
   else if ((disk_nr = get_ppartitions_dev_nr(CNT_DEV)) > 0) {
      *flags |= S_F_HAS_PPARTITIONS;
      disk_nr += NR_DISK_PREALLOC;
      SREALLOC(st_disk, struct disk_stats, DISK_STATS_SIZE * disk_nr);
   }
   else if ((disk_nr = get_disk_io_nr()) > 0) {
      disk_nr += NR_DISK_PREALLOC;
      SREALLOC(st_disk, struct disk_stats, DISK_STATS_SIZE * disk_nr);
   }
}


/*
 ***************************************************************************
 * If -L option used, request a non-blocking, exclusive lock on the file.
 * If lock would block, then another process (possibly sadc) has already
 * opened that file => exit.
 ***************************************************************************
 */
int ask_for_flock(int fd, unsigned int *flags, int fatal)
{
   /* Option -L may be used only if an outfile was specified on the command line */
   if (USE_L_OPTION(*flags)) {
      /*
       * Yes: try to lock file. To make code portable, check for both EWOULDBLOCK
       * and EAGAIN return codes, and treat them the same (glibc documentation).
       * Indeed, some Linux ports (e.g. hppa-linux) do not equate EWOULDBLOCK and
       * EAGAIN like every other Linux port.
       */
      if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
	 if ((((errno == EWOULDBLOCK) || (errno == EAGAIN)) && (fatal == FATAL)) ||
	      ((errno != EWOULDBLOCK) && (errno != EAGAIN))) {
	    perror("flock");
	    exit(1);
	 }
	 /* Was unable to lock file: lock would have blocked... */
	 return 1;
      }
      else
	 /* File successfully locked */
	 *flags |= S_F_FILE_LCK;
   }
   return 0;
}


/*
 ***************************************************************************
 * Fill system activity file header, then print it
 ***************************************************************************
 */
void setup_file_hdr(int fd, size_t *file_stats_size)
{
   int nb;
   struct tm loc_time;
   struct utsname header;

   /* First reset the structure */
   memset(&file_hdr, 0, FILE_HDR_SIZE);

   /* Then get current date */
   file_hdr.sa_ust_time = get_localtime(&loc_time);

   /* Ok, now fill the header */
   file_hdr.sa_actflag = sadc_actflag;
   file_hdr.sa_magic   = SA_MAGIC;
   file_hdr.sa_st_size = FILE_STATS_SIZE;
   file_hdr.sa_day     = loc_time.tm_mday;
   file_hdr.sa_month   = loc_time.tm_mon;
   file_hdr.sa_year    = loc_time.tm_year;
   file_hdr.sa_proc    = cpu_nr;
   file_hdr.sa_nr_pid  = pid_nr;
   file_hdr.sa_serial  = serial_nr;
   file_hdr.sa_irqcpu  = irqcpu_nr;
   file_hdr.sa_iface   = iface_nr;
   if (GET_DISK(sadc_actflag))
      file_hdr.sa_nr_disk = disk_nr;
   else
      file_hdr.sa_nr_disk = 0;

   *file_stats_size = FILE_STATS_SIZE;

   /* Get system name, release number and hostname */
   uname(&header);
   strncpy(file_hdr.sa_sysname, header.sysname, UTSNAME_LEN);
   file_hdr.sa_sysname[UTSNAME_LEN - 1] = '\0';
   strncpy(file_hdr.sa_nodename, header.nodename, UTSNAME_LEN);
   file_hdr.sa_nodename[UTSNAME_LEN - 1] = '\0';
   strncpy(file_hdr.sa_release, header.release, UTSNAME_LEN);
   file_hdr.sa_release[UTSNAME_LEN - 1] = '\0';

   /* Write file header */
   if ((nb = write(fd, &file_hdr, FILE_HDR_SIZE)) != FILE_HDR_SIZE) {
      fprintf(stderr, _("Cannot write system activity file header: %s\n"),
	      strerror(errno));
      exit(2);
   }
}


/*
 ***************************************************************************
 * sadc called with interval and count parameters not set:
 * write a dummy record notifying a system restart.
 * This should typically be called this way at boot time,
 * before the cron daemon is started to avoid conflict with sa1/sa2 scripts.
 ***************************************************************************
 */
void write_dummy_record(int ofd, size_t file_stats_size, unsigned int *flags)
{
   int nb;
   struct tm loc_time;

   /* Check if file is locked */
   if (!FILE_LOCKED(*flags))
      ask_for_flock(ofd, flags, FATAL);

   /* Reset the structure (not compulsory, but a bit cleaner */
   memset(&file_stats, 0, FILE_STATS_SIZE);

   file_stats.record_type = R_DUMMY;

   /* Save time */
   file_stats.ust_time = get_localtime(&loc_time);

   file_stats.hour   = loc_time.tm_hour;
   file_stats.minute = loc_time.tm_min;
   file_stats.second = loc_time.tm_sec;

   /* Write record now */
   if ((nb = write(ofd, &file_stats, file_stats_size)) != file_stats_size)
      p_write_error();
}


/*
 ***************************************************************************
 * Write stats.
 * NB: sadc provides all the stats, including:
 * -> CPU utilization per processor (on SMP machines only)
 * -> IRQ per processor (on SMP machines only)
 * -> number of each IRQ (if -I option passed to sadc), including APIC
 *    interrupts sources
 * -> device stats for sar -d (kernels 2.4 and newer only, and only if
 *    -d option passed to sadc)
 ***************************************************************************
 */
void write_stats(int ofd, size_t file_stats_size, unsigned int *flags)
{
   int nb;

   /* Try to lock file */
   if (!FILE_LOCKED(*flags)) {
      if (ask_for_flock(ofd, flags, NON_FATAL))
	 /* Unable to lock file: wait for next iteration to try again to save data */
	 return;
   }
   if ((nb = write(ofd, &file_stats, file_stats_size)) != file_stats_size)
      p_write_error();
   if (cpu_nr > 0) {
      if ((nb = write(ofd, st_cpu, STATS_ONE_CPU_SIZE * (cpu_nr + 1))) != (STATS_ONE_CPU_SIZE * (cpu_nr + 1)))
	 p_write_error();
   }
   if (GET_ONE_IRQ(sadc_actflag)) {
      if ((nb = write(ofd, interrupts, STATS_ONE_IRQ_SIZE)) != STATS_ONE_IRQ_SIZE)
	 p_write_error();
   }
   if (pid_nr) {
      /* Structures are packed together! */
      if ((nb = write(ofd, pid_stats[0], PID_STATS_SIZE * pid_nr)) != (PID_STATS_SIZE * pid_nr))
	 p_write_error();
   }
   if (serial_nr) {
      if ((nb = write(ofd, st_serial, STATS_SERIAL_SIZE * serial_nr)) != (STATS_SERIAL_SIZE * serial_nr))
	 p_write_error();
   }
   if (irqcpu_nr) {
      if ((nb = write(ofd, st_irq_cpu, STATS_IRQ_CPU_SIZE * (cpu_nr + 1) * irqcpu_nr))
	  != (STATS_IRQ_CPU_SIZE * (cpu_nr + 1) * irqcpu_nr))
	 p_write_error();
   }
   if (iface_nr) {
      if ((nb = write(ofd, st_net_dev, STATS_NET_DEV_SIZE * iface_nr)) != (STATS_NET_DEV_SIZE * iface_nr))
	 p_write_error();
   }
   if (disk_nr && GET_DISK(sadc_actflag)) {
      /* Disk stats written only if -d option used */
      if ((nb = write(ofd, st_disk, DISK_STATS_SIZE * disk_nr)) != (DISK_STATS_SIZE * disk_nr))
	 p_write_error();
   }
}


/*
 ***************************************************************************
 * Create a system activity daily data file
 ***************************************************************************
 */
void create_sa_file(int *ofd, char *ofile, size_t *file_stats_size,
		    unsigned int *flags)
{
   if ((*ofd = open(ofile, O_CREAT | O_WRONLY,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
      fprintf(stderr, _("Cannot open %s: %s\n"), ofile, strerror(errno));
      exit(2);
   }

   /* Try to lock file */
   ask_for_flock(*ofd, flags, FATAL);

   /* Truncate file */
   if (ftruncate(*ofd, 0) < 0) {
      fprintf(stderr, _("Cannot open %s: %s\n"), ofile, strerror(errno));
      exit(2);
   }

   /* Write file header */
   setup_file_hdr(*ofd, file_stats_size);
}


/*
 ***************************************************************************
 * Get descriptor for stdout.
 ***************************************************************************
 */
void open_stdout(int *stdfd, size_t *file_stats_size)
{
   if (*stdfd >= 0) {
      if ((*stdfd = dup(STDOUT_FILENO)) < 0) {
	 perror("dup");
	 exit(4);
      }
      /* Write file header on STDOUT */
      setup_file_hdr(*stdfd, file_stats_size);
   }
}


/*
 ***************************************************************************
 * Get descriptor for output file and write its header.
 * We may enter this function several times (when we rotate a file).
 ***************************************************************************
 */
void open_ofile(int *ofd, char ofile[], size_t *file_stats_size, unsigned int *flags)
{
   ssize_t size;

   if (ofile[0]) {
      /* Does file exist? */
      if (access(ofile, F_OK) < 0)
	 /* NO: create it */
	 create_sa_file(ofd, ofile, file_stats_size, flags);
      else {
	 /* YES: append data to it if possible */
	 if ((*ofd = open(ofile, O_APPEND | O_RDWR)) < 0) {
	    fprintf(stderr, _("Cannot open %s: %s\n"), ofile, strerror(errno));
	    exit(2);
	 }
	
	 /* Read file header */
	 size = read(*ofd, &file_hdr, FILE_HDR_SIZE);
	 if (!size) {
	    close(*ofd);
	    /* This is an empty file: create it again */
	    create_sa_file(ofd, ofile, file_stats_size, flags);
	    return;
	 }
	 if ((size != FILE_HDR_SIZE) || (file_hdr.sa_magic != SA_MAGIC)) {
	    close(*ofd);
	    if (USE_F_OPTION(*flags)) {
	       /* -F option used: Truncate file */
	       create_sa_file(ofd, ofile, file_stats_size, flags);
	       return;
	    }
	    fprintf(stderr, _("Invalid system activity file: %s (%#x)\n"),
		    ofile, file_hdr.sa_magic);
	    exit(3);
	 }
	 /*
	  * Ok: it's a true system activity file.
	  * File activity flag prevails over that of the user
	  * e.g. for the A_ONE_IRQ activity...
	  * Same thing with file file_stats size.
	  */
	 sadc_actflag     = file_hdr.sa_actflag;
	 *file_stats_size = file_hdr.sa_st_size;

	 if (file_hdr.sa_proc != cpu_nr) {
	    close(*ofd);
	    if (USE_F_OPTION(*flags)) {
	       create_sa_file(ofd, ofile, file_stats_size, flags);
	       return;
	    }
	    fprintf(stderr, _("Cannot append data to that file\n"));
	    exit(1);
	 }
	
	 /*
	  * Force characteristics (nb of serial lines, network interfaces...)
	  * to that of the file.
	  */
	 if (file_hdr.sa_serial != serial_nr) {
	    serial_nr = file_hdr.sa_serial;
	    SREALLOC(st_serial, struct stats_serial, STATS_SERIAL_SIZE * serial_nr);
	 }
	 if (file_hdr.sa_iface != iface_nr) {
	    iface_nr = file_hdr.sa_iface;
	    SREALLOC(st_net_dev, struct stats_net_dev, STATS_NET_DEV_SIZE * iface_nr);
	 }
	 if (file_hdr.sa_irqcpu != irqcpu_nr) {
	    irqcpu_nr = file_hdr.sa_irqcpu;
	    SREALLOC(st_irq_cpu, struct stats_irq_cpu,
		     STATS_IRQ_CPU_SIZE * (cpu_nr + 1) * irqcpu_nr);
	 }
	 if (file_hdr.sa_nr_disk != disk_nr) {
	    if (!file_hdr.sa_nr_disk)
	       /* Remove use of option -d */
	       sadc_actflag &= ~A_DISK;
	    else {
	       disk_nr = file_hdr.sa_nr_disk;
	       SREALLOC(st_disk, struct disk_stats, DISK_STATS_SIZE * disk_nr);
	    }
	 }
      }
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/stat
 ***************************************************************************
 */
void read_proc_stat(unsigned int flags)
{
   FILE *fp;
   struct stats_one_cpu *st_cpu_i;
   struct disk_stats *st_disk_i;
   static char line[8192];
   unsigned long long cc_user, cc_nice, cc_system, cc_hardirq, cc_softirq;
   unsigned long long cc_idle, cc_iowait;
   unsigned int v_tmp[5], v_major, v_index;
   int proc_nb, i, pos;

   if ((fp = fopen(STAT, "r")) == NULL) {
      fprintf(stderr, _("Cannot open %s: %s\n"), STAT, strerror(errno));
      exit(2);
   }

   while (fgets(line, 8192, fp) != NULL) {

      if (!strncmp(line, "cpu ", 4)) {
	 /*
	  * Read the number of jiffies spent in the different modes
	  * (user, nice, etc.) among all proc. CPU usage is not reduced
	  * to one processor to avoid rounding problems.
	  */
	 file_stats.cpu_iowait = 0;	/* For pre 2.5 kernels */
	 cc_hardirq = cc_softirq = 0;
	 sscanf(line + 5, "%llu %llu %llu %llu %llu %llu %llu",
		&(file_stats.cpu_user),   &(file_stats.cpu_nice),
		&(file_stats.cpu_system), &(file_stats.cpu_idle),
		&(file_stats.cpu_iowait), &cc_hardirq, &cc_softirq);

	 /*
	  * Time spent in system mode also includes time spent
	  * servicing interrrupts and softirqs
	  */
	 file_stats.cpu_system += cc_hardirq + cc_softirq;
	
	 /*
	  * Compute the uptime of the system in jiffies (1/100ths of a second
	  * if HZ=100).
	  * Machine uptime is multiplied by the number of processors here.
	  * Note that overflow is not so far away: ULONG_MAX is 4294967295 on
	  * 32 bit systems. Overflow happens when machine uptime is:
	  * 497 days on a monoprocessor machine,
	  * 248 days on a bi processor,
	  * 124 days on a quad processor...
	  */
	 file_stats.uptime = file_stats.cpu_user   + file_stats.cpu_nice +
	                     file_stats.cpu_system + file_stats.cpu_idle +
	                     file_stats.cpu_iowait;
      }

      else if (!strncmp(line, "cpu", 3)) {
	 if (cpu_nr > 0) {
	    /*
	     * Read the number of jiffies spent in the different modes
	     * (user, nice, etc) for current proc.
	     * This is done only on SMP machines.
	     * Warning: st_cpu_i struct is _not_ allocated even if the kernel
	     * has SMP support enabled.
	     */
	    cc_iowait = 0;	/* For pre 2.5 kernels */
	    cc_hardirq = cc_softirq = 0;
	    sscanf(line + 3, "%d %llu %llu %llu %llu %llu %llu %llu",
		   &proc_nb,
		   &cc_user, &cc_nice, &cc_system, &cc_idle, &cc_iowait,
		   &cc_hardirq, &cc_softirq);
	    cc_system += cc_hardirq + cc_softirq;
	
	    if (proc_nb <= cpu_nr) {
	       st_cpu_i = st_cpu + proc_nb;
	       st_cpu_i->per_cpu_user   = cc_user;
	       st_cpu_i->per_cpu_nice   = cc_nice;
	       st_cpu_i->per_cpu_system = cc_system;
	       st_cpu_i->per_cpu_idle   = cc_idle;
	       st_cpu_i->per_cpu_iowait = cc_iowait;
	    }
	    /* else:
	     * Additional CPUs have been dynamically registered in /proc/stat.
	     * sar won't crash, but the CPU stats might be false...
	     */
	
	    if (!proc_nb)
	       /* Compute uptime reduced to one proc using proc#0 */
	       file_stats.uptime0 = cc_user + cc_nice + cc_system +
	       			    cc_idle + cc_iowait;
	 }
      }

      else if (!strncmp(line, "page ", 5))
	 /* Read number of pages the system paged in and out */
	 sscanf(line + 5, "%lu %lu",
		&(file_stats.pgpgin), &(file_stats.pgpgout));

      else if (!strncmp(line, "swap ", 5))
	 /* Read number of swap pages brought in and out */
	 sscanf(line + 5, "%lu %lu",
		&(file_stats.pswpin), &(file_stats.pswpout));

      else if (!strncmp(line, "intr ", 5)) {
	 /* Read total number of interrupts received since system boot */
	 sscanf(line + 5, "%llu", &(file_stats.irq_sum));
	 pos = strcspn(line + 5, " ") + 5;

	 if (GET_ONE_IRQ(sadc_actflag)) {
	    /*
	     * If -I option set on the command line,
	     * read number of each interrupts received since system boot.
	     */
	    for (i = 0; i < NR_IRQS; i++) {
	       sscanf(line + pos, " %u", &interrupts[i]);
	       pos += strcspn(line + pos + 1, " ") + 1;
	    }
	 }
      }

      else if (!strncmp(line, "ctxt ", 5))
	 /* Read number of context switches */
	 sscanf(line + 5, "%llu", &(file_stats.context_swtch));

      else if (!strncmp(line, "processes ", 10))
	 /* Read number of processes created since system boot */
	 sscanf(line + 10, "%lu", &(file_stats.processes));

      else if (!HAS_DISKSTATS(flags) && !HAS_PPARTITIONS(flags)) {
	 /*
	  * Read possible disk stats from /proc/stat only if we are
	  * sure they won't be read later from /proc/diskstats or
	  * /proc/partitions.
	  */
	 if (!strncmp(line, "disk ", 5)) {
	    /* Read number of I/O done since the last reboot */
	    sscanf(line + 5, "%u %u %u %u",
		   &(file_stats.dk_drive), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	    file_stats.dk_drive += u_tmp[0] + u_tmp[1] + u_tmp[2];
	 }

	 else if (!strncmp(line, "disk_rio ", 9)) {
	    /* Read number of read I/O */
	    sscanf(line + 9, "%u %u %u %u",
		   &(file_stats.dk_drive_rio), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	    file_stats.dk_drive_rio += u_tmp[0] + u_tmp[1] + u_tmp[2];
	 }

	 else if (!strncmp(line, "disk_wio ", 9)) {
	    /* Read number of write I/O */
	    sscanf(line + 9, "%u %u %u %u",
		   &(file_stats.dk_drive_wio), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	    file_stats.dk_drive_wio += u_tmp[0] + u_tmp[1] + u_tmp[2];
	 }

	 else if (!strncmp(line, "disk_rblk ", 10)) {
	    /* Read number of blocks read from disk */
	    sscanf(line + 10, "%u %u %u %u",
		   &(file_stats.dk_drive_rblk), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	    file_stats.dk_drive_rblk += u_tmp[0] + u_tmp[1] + u_tmp[2];
	 }

	 else if (!strncmp(line, "disk_wblk ", 10)) {
	    /* Read number of blocks written to disk */
	    sscanf(line + 10, "%u %u %u %u",
		   &(file_stats.dk_drive_wblk), &u_tmp[0], &u_tmp[1], &u_tmp[2]);
	    file_stats.dk_drive_wblk += u_tmp[0] + u_tmp[1] + u_tmp[2];
	 }

	 else if (!strncmp(line, "disk_io: ", 9)) {
	    unsigned int dsk = 0;

	    init_dk_drive_stat();
	    pos = 9;
	
	    /* Read disks I/O statistics (for 2.4 kernels) */
	    while (pos < strlen(line) - 1) {	/* Beware: a CR is already included in the line */
	       sscanf(line + pos, "(%u,%u):(%u,%u,%u,%u,%u) ",
		      &v_major, &v_index,
		      &v_tmp[0], &v_tmp[1], &v_tmp[2], &v_tmp[3], &v_tmp[4]);
	       file_stats.dk_drive += v_tmp[0];
	       file_stats.dk_drive_rio  += v_tmp[1];
	       file_stats.dk_drive_rblk += v_tmp[2];
	       file_stats.dk_drive_wio  += v_tmp[3];
	       file_stats.dk_drive_wblk += v_tmp[4];

	       if (dsk < disk_nr) {
		  st_disk_i = st_disk + dsk++;
		  st_disk_i->major = v_major;
		  st_disk_i->minor = v_index;
		  st_disk_i->nr_ios = v_tmp[0];
		  st_disk_i->rd_sect = v_tmp[2];
		  st_disk_i->wr_sect = v_tmp[4];
	       }
	       pos += strcspn(line + pos, " ") + 1;
	    }

	    while (dsk < disk_nr) {
	       /*
		* Nb of disks has changed, or appending data to an old file
		* with more disks than are actually available now.
		*/
	       st_disk_i = st_disk + dsk++;
	       st_disk_i->major = st_disk_i->minor = 0;
	    }
	 }
      }
   }

   fclose(fp);
}


/*
 ***************************************************************************
 * Read stats from /proc/loadavg
 ***************************************************************************
 */
void read_proc_loadavg(void)
{
   FILE *fp;
   int load_tmp[3];

   if ((fp = fopen(LOADAVG, "r")) != NULL) {

      /* Read load averages and queue length */
      fscanf(fp, "%d.%d %d.%d %d.%d %ld/%d %*d\n",
	     &(load_tmp[0]), &(file_stats.load_avg_1),
	     &(load_tmp[1]), &(file_stats.load_avg_5),
	     &(load_tmp[2]), &(file_stats.load_avg_15),
	     &(file_stats.nr_running),
	     &(file_stats.nr_threads));
      fclose(fp);

      file_stats.load_avg_1  += load_tmp[0] * 100;
      file_stats.load_avg_5  += load_tmp[1] * 100;
      file_stats.load_avg_15 += load_tmp[2] * 100;
      if (file_stats.nr_running)
	 /* Do not take current process into account */
	 file_stats.nr_running--;
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/meminfo
 ***************************************************************************
 */
void read_proc_meminfo(void)
{
   FILE *fp;
   static char line[128];

   if ((fp = fopen(MEMINFO, "r")) == NULL)
      return;

   while (fgets(line, 128, fp) != NULL) {

      if (!strncmp(line, "MemTotal:", 9))
	 /* Read the total amount of memory in kB */
	 sscanf(line + 9, "%lu", &(file_stats.tlmkb));
      else if (!strncmp(line, "MemFree:", 8))
	 /* Read the amount of free memory in kB */
	 sscanf(line + 8, "%lu", &(file_stats.frmkb));

      else if (!strncmp(line, "Buffers:", 8))
	 /* Read the amount of buffered memory in kB */
	 sscanf(line + 8, "%lu", &(file_stats.bufkb));

      else if (!strncmp(line, "Cached:", 7))
	 /* Read the amount of cached memory in kB */
	 sscanf(line + 7, "%lu", &(file_stats.camkb));

      else if (!strncmp(line, "SwapCached:", 11))
	 /* Read the amount of cached swap in kB */
	 sscanf(line + 11, "%lu", &(file_stats.caskb));

      else if (!strncmp(line, "SwapTotal:", 10))
	 /* Read the total amount of swap memory in kB */
	 sscanf(line + 10, "%lu", &(file_stats.tlskb));

      else if (!strncmp(line, "SwapFree:", 9))
	 /* Read the amount of free swap memory in kB */
	 sscanf(line + 9, "%lu", &(file_stats.frskb));
   }

   fclose(fp);
}


/*
 ***************************************************************************
 * Read stats from /proc/vmstat (post 2.5 kernels)
 ***************************************************************************
 */
void read_proc_vmstat(void)
{
   FILE *fp;
   static char line[128];

   if ((fp = fopen(VMSTAT, "r")) == NULL)
      return;

   while (fgets(line, 128, fp) != NULL) {
      /*
       * Some of these stats may have already been read
       * in /proc/stat file (pre 2.5 kernels).
       */

      if (!strncmp(line, "pgpgin", 6))
	 /* Read number of pages the system paged in */
	 sscanf(line + 6, "%lu", &(file_stats.pgpgin));

      else if (!strncmp(line, "pgpgout", 7))
	 /* Read number of pages the system paged out */
	 sscanf(line + 7, "%lu", &(file_stats.pgpgout));

      else if (!strncmp(line, "pswpin", 6))
	 /* Read number of swap pages brought in */
	 sscanf(line + 6, "%lu", &(file_stats.pswpin));

      else if (!strncmp(line, "pswpout", 7))
	 /* Read number of swap pages brought out */
	 sscanf(line + 7, "%lu", &(file_stats.pswpout));

      else if (!strncmp(line, "pgfault", 7))
	 /* Read number of faults (major+minor) made by the system */
	 sscanf(line + 7, "%lu", &(file_stats.pgfault));

      else if (!strncmp(line, "pgmajfault", 10))
	 /* Read number of faults (major only) made by the system */
	 sscanf(line + 10, "%lu", &(file_stats.pgmajfault));
   }

   fclose(fp);
}


/*
 ***************************************************************************
 * Read stats from /proc/<pid>/stat
 ***************************************************************************
 */
void read_pid_stat(void)
{
   FILE *fp;
   int pid;
   static char filename[24];

   for (pid = 0; pid < pid_nr; pid++) {

      if (!all_pids[pid])
	 continue;

      sprintf(filename, PID_STAT, all_pids[pid]);
      if ((fp = fopen(filename, "r")) == NULL) {
	 /* No such process */
	 all_pids[pid] = 0;
	 pid_stats[pid]->pid = 0;
	 continue;
      }

      fscanf(fp, "%*d %*s %*s %*d %*d %*d %*d %*d %*u %lu %lu %lu %lu %lu %lu %lu "
	         "%lu %*d %*d %*u %*u %*d %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u "
	         "%*u %*u %*u %lu %lu %*u %u\n",
	     &(pid_stats[pid]->minflt), &(pid_stats[pid]->cminflt),
	     &(pid_stats[pid]->majflt), &(pid_stats[pid]->cmajflt),
	     &(pid_stats[pid]->utime),  &(pid_stats[pid]->stime),
	     &(pid_stats[pid]->cutime), &(pid_stats[pid]->cstime),
	     &(pid_stats[pid]->nswap),  &(pid_stats[pid]->cnswap),
	     &(pid_stats[pid]->processor));

      pid_stats[pid]->pid  = all_pids[pid];
      pid_stats[pid]->flag = f_pids[pid];

      fclose(fp);
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/tty/driver/serial
 ***************************************************************************
 */
void read_serial_stat(void)
{
   struct stats_serial *st_serial_i;
   unsigned int sl = 0;

#ifndef SMP_RACE
   FILE *fp;
   static char line[256];
   char *p;
	
      if ((fp = fopen(SERIAL, "r")) != NULL) {

	 while ((fgets(line, 256, fp) != NULL) && (sl < serial_nr)) {

	    if ((p = strstr(line, "tx:")) != NULL) {
	       st_serial_i = st_serial + sl;
	       sscanf(line, "%u", &(st_serial_i->line));
	       /*
		* Read the number of chars transmitted and received by
		* current serial line.
		*/
	       sscanf(p + 3, "%u", &(st_serial_i->tx));
	       if ((p = strstr(line, "rx:")) != NULL)
		  sscanf(p + 3, "%u", &(st_serial_i->rx));
	       if ((p = strstr(line, "fe:")) != NULL)
		  sscanf(p + 3, "%u", &(st_serial_i->frame));
	       if ((p = strstr(line, "pe:")) != NULL)
		  sscanf(p + 3, "%u", &(st_serial_i->parity));
	       if ((p = strstr(line, "brk:")) != NULL)
		  sscanf(p + 4, "%u", &(st_serial_i->brk));
	       if ((p = strstr(line, "oe:")) != NULL)
		  sscanf(p + 3, "%u", &(st_serial_i->overrun));

	       sl++;
	    }
	 }

	 fclose(fp);
      }
#endif

   while (sl < serial_nr) {
      /*
       * Nb of serial lines has changed, or appending data to an old file
       * with more serial lines than are actually available now.
       */
      st_serial_i = st_serial + sl++;
      st_serial_i->line = ~0;
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/interrupts
 ***************************************************************************
 */
void read_interrupts_stat(void)
{
   FILE *fp;
   static char line[INTERRUPTS_LINE];
   unsigned int irq = 0, cpu;
   struct stats_irq_cpu *p;

   if ((fp = fopen(INTERRUPTS, "r")) != NULL) {

      while ((fgets(line, INTERRUPTS_LINE, fp) != NULL) && (irq < irqcpu_nr)) {

	 if (isdigit(line[2])) {
	
	    p = st_irq_cpu + irq;
	    sscanf(line, "%3u", &(p->irq));
	
	    for (cpu = 0; cpu <= cpu_nr; cpu++) {
	       p = st_irq_cpu + cpu * irqcpu_nr + irq;
	       /*
		* No need to set (st_irq_cpu + cpu * irqcpu_nr)->irq:
		* same as st_irq_cpu->irq.
		*/
	       sscanf(line + 4 + 11 * cpu, " %10u", &(p->interrupt));
	    }
	    irq++;
	 }
      }

      fclose(fp);
   }

   while (irq < irqcpu_nr) {
      /*
       * Nb of interrupts per processor has changed, or appending data to an
       * old file with more interrupts than are actually available now.
       */
      p = st_irq_cpu + irq;
      p->irq = ~0;	/* This value means this is a dummy interrupt */
      irq++;
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/sys/fs/...
 * Some files may not exist, depending on the kernel configuration.
 ***************************************************************************
 */
void read_ktables_stat(void)
{
   FILE *fp;
   unsigned int parm;

   /* Open /proc/sys/fs/dentry-state file */
   if ((fp = fopen(FDENTRY_STATE, "r")) != NULL) {
      fscanf(fp, "%*d %u",
	     &(file_stats.dentry_stat));
      fclose(fp);
   }

   /* Open /proc/sys/fs/file-nr file */
   if ((fp = fopen(FFILE_NR, "r")) != NULL) {
      fscanf(fp, "%u %u",
	     &(file_stats.file_used), &parm);
      fclose(fp);
      /*
       * The number of used handles is the number of allocated ones
       * minus the number of free ones.
       */
      file_stats.file_used -= parm;
   }

   /* Open /proc/sys/fs/inode-state file */
   if ((fp = fopen(FINODE_STATE, "r")) != NULL) {
      fscanf(fp, "%u %u",
	     &(file_stats.inode_used), &parm);
      fclose(fp);
      /*
       * The number of inuse inodes is the number of allocated ones
       * minus the number of free ones.
       */
      file_stats.inode_used -= parm;
   }

   /* Open /proc/sys/fs/super-max file */
   if ((fp = fopen(FSUPER_MAX, "r")) != NULL) {
      fscanf(fp, "%u\n",
	     &(file_stats.super_max));
      fclose(fp);

      /* Open /proc/sys/fs/super-nr file */
      if ((fp = fopen(FSUPER_NR, "r")) != NULL) {
	 fscanf(fp, "%u\n",
		&(file_stats.super_used));
	 fclose(fp);
      }
   }

   /* Open /proc/sys/fs/dquot-max file */
   if ((fp = fopen(FDQUOT_MAX, "r")) != NULL) {
      fscanf(fp, "%u\n",
	     &(file_stats.dquot_max));
      fclose(fp);

      /* Open /proc/sys/fs/dquot-nr file */
      if ((fp = fopen(FDQUOT_NR, "r")) != NULL) {
	 fscanf(fp, "%u",
		&(file_stats.dquot_used));
	 fclose(fp);
      }
   }

   /* Open /proc/sys/kernel/rtsig-max file */
   if ((fp = fopen(FRTSIG_MAX, "r")) != NULL) {
      fscanf(fp, "%u\n",
	     &(file_stats.rtsig_max));
      fclose(fp);

      /* Open /proc/sys/kernel/rtsig-nr file */
      if ((fp = fopen(FRTSIG_NR, "r")) != NULL) {
	 fscanf(fp, "%u\n",
		&(file_stats.rtsig_queued));
	 fclose(fp);
      }
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/net/dev
 ***************************************************************************
 */
void read_net_dev_stat(void)
{
   FILE *fp;
   struct stats_net_dev *st_net_dev_i;
   static char line[256];
   char iface[MAX_IFACE_LEN];
   unsigned int dev = 0;
   int pos;

   if ((fp = fopen(NET_DEV, "r")) != NULL) {

      while ((fgets(line, 256, fp) != NULL) && (dev < iface_nr)) {
	
	 pos = strcspn(line, ":");
	 if (pos < strlen(line)) {
	    st_net_dev_i = st_net_dev + dev;
  	    strncpy(iface, line, MINIMUM(pos, MAX_IFACE_LEN - 1));
	    iface[MINIMUM(pos, MAX_IFACE_LEN - 1)] = '\0';
	    sscanf(iface, "%s", st_net_dev_i->interface); /* Skip heading spaces */
	    sscanf(line + pos + 1, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
		                   "%lu %lu %lu %lu %lu %lu",
		   &(st_net_dev_i->rx_bytes),
		   &(st_net_dev_i->rx_packets),
		   &(st_net_dev_i->rx_errors),
		   &(st_net_dev_i->rx_dropped),
		   &(st_net_dev_i->rx_fifo_errors),
		   &(st_net_dev_i->rx_frame_errors),
		   &(st_net_dev_i->rx_compressed),
		   &(st_net_dev_i->multicast),
		   &(st_net_dev_i->tx_bytes),
		   &(st_net_dev_i->tx_packets),
		   &(st_net_dev_i->tx_errors),
		   &(st_net_dev_i->tx_dropped),
		   &(st_net_dev_i->tx_fifo_errors),
		   &(st_net_dev_i->collisions),
		   &(st_net_dev_i->tx_carrier_errors),
		   &(st_net_dev_i->tx_compressed));
	    dev++;
	 }
      }

      fclose(fp);
   }

   if (dev < iface_nr) {
      /* Reset unused structures */
      memset(st_net_dev + dev, 0, STATS_NET_DEV_SIZE * (iface_nr - dev));

      while (dev < iface_nr) {
	 /*
	  * Nb of network interfaces has changed, or appending data to an
	  * old file with more interfaces than are actually available now.
	  */
	 st_net_dev_i = st_net_dev + dev++;
	 strcpy(st_net_dev_i->interface, "?");
      }
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/net/sockstat
 ***************************************************************************
 */
void read_net_sock_stat(void)
{
   FILE *fp;
   static char line[96];

   if ((fp = fopen(NET_SOCKSTAT, "r")) == NULL)
      return;

   while (fgets(line, 96, fp) != NULL) {
	
      if (!strncmp(line, "sockets:", 8))
	 /* Sockets */
	 sscanf(line + 14, "%u", &(file_stats.sock_inuse));
      else if (!strncmp(line, "TCP:", 4))
	 /* TCP sockets */
	 sscanf(line + 11, "%u", &(file_stats.tcp_inuse));
      else if (!strncmp(line, "UDP:", 4))
	 /* UDP sockets */
	 sscanf(line + 11, "%u", &(file_stats.udp_inuse));
      else if (!strncmp(line, "RAW:", 4))
	 /* RAW sockets */
	 sscanf(line + 11, "%u", &(file_stats.raw_inuse));
      else if (!strncmp(line, "FRAG:", 5))
	 /* FRAGments */
	 sscanf(line + 12, "%u", &(file_stats.frag_inuse));
   }

   fclose(fp);
}


/*
 ***************************************************************************
 * Read stats from /proc/net/rpc/nfs
 ***************************************************************************
 */
void read_net_nfs_stat(void)
{
   FILE *fp;
   static char line[256];

   if ((fp = fopen(NET_RPC_NFS, "r")) == NULL)
      return;

   while (fgets(line, 256, fp) != NULL) {
	
      if (!strncmp(line, "rpc", 3))
	 sscanf(line + 3, "%u %u",
		&(file_stats.nfs_rpccnt), &(file_stats.nfs_rpcretrans));
	
      else if (!strncmp(line, "proc3", 5))
	 sscanf(line + 5, "%*u %u %*u %*u %u %*u %u %u",
		&(file_stats.nfs_getattcnt), &(file_stats.nfs_accesscnt),
		&(file_stats.nfs_writecnt), &(file_stats.nfs_readcnt));
   }

   fclose(fp);
}


/*
 ***************************************************************************
 * Read stats from /proc/net/rpc/nfsd
 ***************************************************************************
 */
void read_net_nfsd_stat(void)
{
   FILE *fp;
   static char line[256];

   if ((fp = fopen(NET_RPC_NFSD, "r")) == NULL)
      return;

   while (fgets(line, 256, fp) != NULL) {
	
      if (!strncmp(line, "rc", 2))
	 sscanf(line + 2, "%u %u",
		&(file_stats.nfsd_rchits), &(file_stats.nfsd_rcmisses));
	
      else if (!strncmp(line, "net", 3))
	 sscanf(line + 3, "%u %u %u",
		&(file_stats.nfsd_netcnt), &(file_stats.nfsd_netudpcnt),
		&(file_stats.nfsd_nettcpcnt));
	
      else if (!strncmp(line, "rpc", 3))
	 sscanf(line + 3, "%u %u",
		&(file_stats.nfsd_rpccnt), &(file_stats.nfsd_rpcbad));
	
      else if (!strncmp(line, "proc3", 5))
	 sscanf(line + 5, "%*u %u %*u %*u %u %*u %u %u",
		&(file_stats.nfsd_getattcnt), &(file_stats.nfsd_accesscnt),
		&(file_stats.nfsd_writecnt), &(file_stats.nfsd_readcnt));
   }

   fclose(fp);
}



/*
 ***************************************************************************
 * Read stats from /proc/diskstats
 ***************************************************************************
 */
void read_diskstats_stat(void)
{
   FILE *fp;
   static char line[256];
   int dsk = 0;
   struct disk_stats *st_disk_i;
   unsigned int major, minor;
   unsigned long rd_ios, wr_ios, rd_ticks, wr_ticks;
   unsigned long tot_ticks, rq_ticks;
   unsigned long long rd_sec, wr_sec;

   if ((fp = fopen(DISKSTATS, "r")) != NULL) {

      init_dk_drive_stat();

      while ((fgets(line, 256, fp) != NULL) && (dsk < disk_nr)) {
	
	 if (sscanf(line, "%u %u %*s %lu %*u %llu %lu %lu %*u %llu"
		          " %lu %*u %lu %lu",
		    &major, &minor,
		    &rd_ios, &rd_sec, &rd_ticks, &wr_ios, &wr_sec, &wr_ticks,
		    &tot_ticks, &rq_ticks) == 10) {
	    /* It's a device and not a partition */
	    if (!rd_ios && !wr_ios)
	       /* Unused device: ignore it */
	       continue;
	    st_disk_i = st_disk + dsk++;
	    st_disk_i->major = major;
	    st_disk_i->minor = minor;
	    st_disk_i->nr_ios = rd_ios + wr_ios;
	    st_disk_i->rd_sect = rd_sec;
	    st_disk_i->wr_sect = wr_sec;
	    st_disk_i->rd_ticks = rd_ticks;
	    st_disk_i->wr_ticks = wr_ticks;
	    st_disk_i->tot_ticks = tot_ticks;
	    st_disk_i->rq_ticks = rq_ticks;
	
	    file_stats.dk_drive += rd_ios + wr_ios;
	    file_stats.dk_drive_rio += rd_ios;
	    file_stats.dk_drive_rblk += (unsigned int) rd_sec;
	    file_stats.dk_drive_wio += wr_ios;
	    file_stats.dk_drive_wblk += (unsigned int) wr_sec;
	 }
      }

      fclose(fp);
   }

   while (dsk < disk_nr) {
      /*
       * Nb of disks has changed, or appending data to an old file
       * with more disks than are actually available now.
       */
      st_disk_i = st_disk + dsk++;
      st_disk_i->major = st_disk_i->minor = 0;
   }
}


/*
 ***************************************************************************
 * Read stats from /proc/partitions
 ***************************************************************************
 */
void read_ppartitions_stat(void)
{
   FILE *fp;
   static char line[256];
   int dsk = 0;
   struct disk_stats *st_disk_i;
   unsigned int major, minor;
   unsigned long rd_ios, wr_ios, rd_ticks, wr_ticks, tot_ticks, rq_ticks;
   unsigned long long rd_sec, wr_sec;

   if ((fp = fopen(PPARTITIONS, "r")) != NULL) {

      init_dk_drive_stat();

      while ((fgets(line, 256, fp) != NULL) && (dsk < disk_nr)) {
	
	 if (sscanf(line, "%u %u %*u %*s %lu %*u %llu %lu %lu %*u %llu"
		          " %lu %*u %lu %lu",
		    &major, &minor, &rd_ios, &rd_sec, &rd_ticks, &wr_ios,
		    &wr_sec, &wr_ticks, &tot_ticks, &rq_ticks) == 10) {

	    if (ioc_iswhole(major, minor)) {
	       /* OK: it's a device and not a partition */
	       st_disk_i = st_disk + dsk++;
	       st_disk_i->major = major;
	       st_disk_i->minor = minor;
	       st_disk_i->nr_ios = rd_ios + wr_ios;
	       st_disk_i->rd_sect = rd_sec;
	       st_disk_i->wr_sect = wr_sec;
	       st_disk_i->rd_ticks = rd_ticks;
	       st_disk_i->wr_ticks = wr_ticks;
	       st_disk_i->tot_ticks = tot_ticks;
	       st_disk_i->rq_ticks = rq_ticks;
	
	       file_stats.dk_drive += rd_ios + wr_ios;
	       file_stats.dk_drive_rio += rd_ios;
	       file_stats.dk_drive_rblk += (unsigned int) rd_sec;
	       file_stats.dk_drive_wio += wr_ios;
	       file_stats.dk_drive_wblk += (unsigned int) wr_sec;
	    }
	 }
      }

      fclose(fp);
   }

   while (dsk < disk_nr) {
      /*
       * Nb of disks has changed, or appending data to an old file
       * with more disks than are actually available now.
       */
      st_disk_i = st_disk + dsk++;
      st_disk_i->major = st_disk_i->minor = 0;
   }
}


/*
 ***************************************************************************
 * Read system statistics from various files
 ***************************************************************************
 */
void read_stats(unsigned int *flags)
{
   read_proc_stat(*flags);
   read_proc_meminfo();
   read_proc_loadavg();
   read_proc_vmstat();
   read_ktables_stat();
   read_net_sock_stat();
   read_net_nfs_stat();
   read_net_nfsd_stat();

   /* Read disk stats, at least for sar -b */
   if (HAS_DISKSTATS(*flags))
      read_diskstats_stat();
   else if (HAS_PPARTITIONS(*flags))
      read_ppartitions_stat();
   /* else disks are in /proc/stat */

   if (pid_nr)
       read_pid_stat();
    if (serial_nr)
      read_serial_stat();
   if (irqcpu_nr)
      read_interrupts_stat();
   if (iface_nr)
      read_net_dev_stat();
}


/*
 ***************************************************************************
 * Main loop: read stats from the relevant sources,
 * and display them.
 ***************************************************************************
 */
void rw_sa_stat_loop(unsigned int *flags, long count, struct tm *loc_time,
		     int stdfd, int ofd, size_t file_stats_size, char ofile[],
		     char new_ofile[])
{
   int do_sa_rotat = 0;
   unsigned int save_flags;

   /* Main loop */
   do {

      /*
       * Init file_stats structure. Every record of other structures
       * is set when reading corresponding stat file (records are set
       * to 0 if there are not enough data to fill the structure).
       */
      memset(&file_stats, 0, FILE_STATS_SIZE);

      /* Set record type */
      if (do_sa_rotat)
	 file_stats.record_type = R_LAST_STATS;
      else
	 file_stats.record_type = R_STATS;

      /* Save time */
      file_stats.ust_time = get_localtime(loc_time);
      file_stats.hour   = loc_time->tm_hour;
      file_stats.minute = loc_time->tm_min;
      file_stats.second = loc_time->tm_sec;

      /* Read then write stats */
      read_stats(flags);

      if (stdfd >= 0) {
	 save_flags = *flags;
	 *flags &= ~S_F_L_OPTION;
	 write_stats(stdfd, file_stats_size, flags);
	 *flags = save_flags;
      }

      file_stats.record_type = R_STATS;
      if (ofile[0])
	 write_stats(ofd, file_stats_size, flags);

      if (do_sa_rotat) {
	 /*
	  * Stats are written at the end of previous file *and* at the
	  * beginning of the new one (outfile must have been specified
	  * as '-' on the command line).
	  */
	 do_sa_rotat = FALSE;

	 if (fdatasync(ofd) < 0) {	/* Flush previous file */
	    perror("fdatasync");
	    exit(4);
	 }
	 close(ofd);
	 strcpy(ofile, new_ofile);
	 /* Recalculate nb of system items and reallocate structures */
	 sa_sys_init(flags);
	 /* Rewrite header to stdout */
	 if (stdfd >= 0)
	    setup_file_hdr(stdfd, &file_stats_size);
	 /* Open and init new file */
	 open_ofile(&ofd, ofile, &file_stats_size, flags);
	 /* Write stats again */
	 write_stats(ofd, file_stats_size, flags);
      }

      /* Flush data */
      fflush(stdout);
      if (ofile[0] && (fdatasync(ofd) < 0)) {
	 perror("fdatasync");
	 exit(4);
      }

      if (count > 0)
	 count--;

      if (count)
	 pause();

      /* Rotate activity file if necessary */
      if (WANT_SA_ROTAT(*flags)) {
	 /* The user specified '-' as the filename to use */
	 get_localtime(loc_time);
	 snprintf(new_ofile, MAX_FILE_LEN,
		  "%s/sa%02d", SA_DIR, loc_time->tm_mday);
	 new_ofile[MAX_FILE_LEN - 1] = '\0';

	 if (strcmp(ofile, new_ofile))
	    do_sa_rotat = TRUE;
      }
   }
   while (count);

   CLOSE(stdfd);
   CLOSE(ofd);
}


/*
 ***************************************************************************
 * Main entry to the program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
   int opt = 0, optz = 0, i;
   unsigned long pid;
   char ofile[MAX_FILE_LEN];
   char new_ofile[MAX_FILE_LEN];
   unsigned int flags = 0;
   struct tm loc_time;
   int stdfd = 0, ofd = -1;
   long count = 0;
   /*
    * This variable contains:
    * - FILE_STATS_SIZE defined in sa.h if creating a new daily data file or
    *   using STDOUT,
    * - the size of the file_stats structure defined in the header of the
    *   file if appending data to an existing daily data file.
    */
   size_t file_stats_size = FILE_STATS_SIZE;

   /* Compute page shift in kB */
   kb_shift = get_kb_shift();

   ofile[0] = new_ofile[0] = '\0';

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   /* Init activity flag */
   sadc_actflag = A_PROC + A_PAGE + A_IRQ + A_IO + A_CPU + A_CTXSW + A_SWAP +
                  A_MEMORY + A_MEM_AMT + A_KTABLES + A_NET_SOCK + A_QUEUE +
      		  A_NET_NFS + A_NET_NFSD;

   while (++opt < argc) {

      if (!strcmp(argv[opt], "-I"))
	 sadc_actflag |= A_ONE_IRQ;

      else if (!strcmp(argv[opt], "-d"))
	 sadc_actflag |= A_DISK;

      else if (!strcmp(argv[opt], "-F"))
	 flags |= S_F_F_OPTION;

      else if (!strcmp(argv[opt], "-L"))
	 flags |= S_F_L_OPTION;

      else if (!strcmp(argv[opt], "-V"))
	 usage(argv[0]);

      else if (!strcmp(argv[opt], "-z"))	/* Set by sar command */
	 optz = 1;

      else if (!strcmp(argv[opt], "-x") || !strcmp(argv[opt], "-X")) {
	 if (!GET_PID(sadc_actflag)) {
	    /* Init PID flag the first time */
	    for (i = 0; i < MAX_PID_NR; i++)
	       f_pids[i] = 0;
	 }
	 sadc_actflag |= A_PID;
	 if (argv[++opt]) {
	    if (!strcmp(argv[opt], K_ALL)) {
	       /* Get PID list */
	       get_pid_list();
	       set_pflag(strcmp(argv[opt - 1], "-x"), 0);
	       continue;	/* Next option */
	    }
	    else if (!strcmp(argv[opt], K_SELF))
	       pid = getpid();
	    else {
	       if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
		  usage(argv[0]);
	       pid = atol(argv[opt]);
	       if (pid < 1)
		  usage(argv[0]);
	    }
	    set_pflag(strcmp(argv[opt - 1], "-x"), pid);
	 }
      }

      else if (strspn(argv[opt], DIGITS) != strlen(argv[opt])) {
	 if (!ofile[0]) {
	    stdfd = -1;	/* Don't write to STDOUT */
	    if (!strcmp(argv[opt], "-")) {
	       /* File name set to '-' */
	       get_localtime(&loc_time);
	       snprintf(ofile, MAX_FILE_LEN,
			"%s/sa%02d", SA_DIR, loc_time.tm_mday);
	       ofile[MAX_FILE_LEN - 1] = '\0';
	       flags |= S_F_SA_ROTAT;
	    }
	    else if (!strncmp(argv[opt], "-", 1))
	       /* Bad option */
	       usage(argv[0]);
	    else {
	       /* Write data to file */
	       strncpy(ofile, argv[opt], MAX_FILE_LEN);
	       ofile[MAX_FILE_LEN - 1] = '\0';
	    }
	 }
	 else
	    /* Outfile already specified */
	    usage(argv[0]);
      }

      else if (!interval) {		/* Get interval */
	 interval = atol(argv[opt]);
	 if (interval < 1)
	   usage(argv[0]);
	 count = -1;
      }

      else if (count <= 0) {		/* Get count value */
	 count = atol(argv[opt]);
	 if (count < 1)
	   usage(argv[0]);
      }

      else
	 usage(argv[0]);
   }

   /*
    * If option -z used, write to STDOUT even if a filename
    * has been entered on the command line.
    */
   if (optz)
      stdfd = 0;

   /* -x and -X options ignored when writing to a file */
   if (ofile[0]) {
      pid_nr = 0;
      sadc_actflag &= ~A_PID;
   }
   else
      /* -L option ignored when writing to STDOUT */
      flags &= ~S_F_L_OPTION;

   /* Init structures according to machine architecture */
   sa_sys_init(&flags);

   if (GET_PID(sadc_actflag))
      salloc_pid(pid_nr);

   /*
    * Open output file then STDOUT. Write header for each of them.
    * NB: Output file must be opened first because we may change
    * the activity flag to that of the file, and the activity flag
    * written on STDOUT must be consistent.
    */
   open_ofile(&ofd, ofile, &file_stats_size, &flags);
   open_stdout(&stdfd, &file_stats_size);

   if (!interval) {
      /* Interval (and count) not set: write a dummy record and exit */
      write_dummy_record(ofd, file_stats_size, &flags);
      CLOSE(ofd);
      CLOSE(stdfd);
      exit(0);
   }

   /* Set a handler for SIGALRM */
   alarm_handler(0);

   /* Main loop */
   rw_sa_stat_loop(&flags, count, &loc_time, stdfd, ofd,
		   file_stats_size, ofile, new_ofile);

   return 0;
}
