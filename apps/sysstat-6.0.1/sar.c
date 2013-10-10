/*
 * sar: report system activity
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
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/param.h>	/* for HZ */

#include "version.h"
#include "sa.h"
#include "common.h"


#ifdef USE_NLS
#include <locale.h>
#include <libintl.h>
#define _(string) gettext(string)
#else
#define _(string) (string)
#endif

long interval = -1, count = 0;
unsigned int sar_actflag = 0;
unsigned int flags = 0;
unsigned char irq_bitmap[(NR_IRQS / 8) + 1];
unsigned char cpu_bitmap[(NR_CPUS / 8) + 1];
int kb_shift = 0;

struct stats_sum asum;
struct file_hdr file_hdr;
struct file_stats file_stats[DIM];
struct stats_one_cpu *st_cpu[DIM] = {NULL, NULL, NULL};
struct stats_serial *st_serial[DIM] = {NULL, NULL, NULL};
struct stats_irq_cpu *st_irq_cpu[DIM] = {NULL, NULL, NULL};
struct stats_net_dev *st_net_dev[DIM] = {NULL, NULL, NULL};
struct disk_stats *st_disk[DIM] = {NULL, NULL, NULL};

/* Array members of common types are always packed */
unsigned int interrupts[DIM][NR_IRQS];
/* Structures are aligned but also padded. Thus array members are packed */
struct pid_stats *pid_stats[DIM][MAX_PID_NR];

struct tm loc_time;
/* Contain the date specified by -s and -e options */
struct tstamp tm_start, tm_end;
short dis_hdr = -1;
int pid_nr = 0;
char *args[MAX_ARGV_NR];


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
	           "[ -A ] [ -b ] [ -B ] [ -c ] [ -d ] [ -i <interval> ] [ -p ] [ -q ]\n"
		   "[ -r ] [ -R ] [ -t ] [ -u ] [ -v ] [ -V ] [ -w ] [ -W ] [ -y ]\n"
		   "[ -I { <irq> | SUM | ALL | XALL } ] [ -P { <cpu> | ALL } ]\n"
		   "[ -n { DEV | EDEV | NFS | NFSD | SOCK | FULL } ]\n"
		   "[ -x { <pid> | SELF | ALL } ] [ -X { <pid> | SELF | ALL } ]\n"
	           "[ -o [ <filename> ] | -f [ <filename> ] ]\n"
		   "[ -s [ <hh:mm:ss> ] ] [ -e [ <hh:mm:ss> ] ]\n"),
	   VERSION, progname);
   exit(1);
}


/*
 ***************************************************************************
 * Init stats structures
 ***************************************************************************
 */
void init_all_stats(void)
{
   int i;

   init_stats(file_stats, interrupts);
   memset(&asum, 0, STATS_SUM_SIZE);

   for (i = 0; i < DIM; i++)
      pid_stats[i][0] = NULL;
}


/*
 ***************************************************************************
 * Allocate memory for sadc args
 ***************************************************************************
 */
void salloc(int i, char *ltemp)
{
   if ((args[i] = (char *) malloc(strlen(ltemp) + 1)) == NULL) {
      perror("malloc");
      exit(4);
   }
   strcpy(args[i], ltemp);
}


/*
 ***************************************************************************
 * Allocate pid_stats structures
 ***************************************************************************
 */
void salloc_pid(int nr_pid)
{
   int i, pid;

   for (i = 0; i < DIM; i++) {
      if (pid_stats[i][0])
	 free(pid_stats[i][0]);
      if ((pid_stats[i][0] = (struct pid_stats *) malloc(PID_STATS_SIZE * nr_pid)) == NULL) {
	 perror("malloc");
	 exit(4);
      }

      memset(pid_stats[i][0], 0, PID_STATS_SIZE * nr_pid);

      for (pid = 1; pid < nr_pid; pid++)
	 /* Structures are aligned but also padded. Thus array members are packed */
	 pid_stats[i][pid] = pid_stats[i][0] + pid;	/* Assume nr_pids <= MAX_PID_NR */
   }
}


/*
 ***************************************************************************
 * Allocate structures
 ***************************************************************************
 */
void allocate_structures(int stype)
{
   if (file_hdr.sa_proc > 0)
      salloc_cpu_array(st_cpu, file_hdr.sa_proc + 1);
   if ((stype == USE_SADC) &&
       (GET_PID(sar_actflag) || GET_CPID(sar_actflag))) {
      pid_nr = file_hdr.sa_nr_pid;
      salloc_pid(pid_nr);
   }
   if (file_hdr.sa_serial)
      salloc_serial_array(st_serial, file_hdr.sa_serial);
   if (file_hdr.sa_irqcpu)
      salloc_irqcpu_array(st_irq_cpu, file_hdr.sa_proc + 1, file_hdr.sa_irqcpu);
   if (file_hdr.sa_iface)
      salloc_net_dev_array(st_net_dev, file_hdr.sa_iface);
   if (file_hdr.sa_nr_disk)
      salloc_disk_array(st_disk, file_hdr.sa_nr_disk);
}


/*
 ***************************************************************************
 * Check if the user has the right to use -P option.
 * Note that he may use this option when reading stats from a file,
 * even if his machine is not an SMP one...
 * This routine is called only if we are *not* reading stats from a file.
 ***************************************************************************
 */
void check_smp_option(unsigned int cpu_nr)
{
   unsigned int j = 0, i;

   if (cpu_nr == 0) {
      fprintf(stderr, _("Not an SMP machine...\n"));
      exit(1);
   }

   for (i = (cpu_nr + 1); i < NR_CPUS; i++)
      j |= cpu_bitmap[i >> 3] & (1 << (i & 0x07));

   if (j) {
      fprintf(stderr, _("Not that many processors!\n"));
      exit(1);
   }
}


/*
 ***************************************************************************
 * Check the use of option -P.
 * Called only if reading stats sent by the data collector.
 ***************************************************************************
 */
void prep_smp_option(unsigned int cpu_nr)
{
   unsigned int i;

   if (WANT_PER_PROC(flags)) {
      if (WANT_ALL_PROC(flags))
	 for (i = cpu_nr + 1; i < ((NR_CPUS >> 3) + 1) << 3; i++)
	    /*
	     * Reset every bit for proc > cpu_nr
	     * (only done when -P ALL entered on the command line)
	     */
	    cpu_bitmap[i >> 3] &= ~(1 << (i & 0x07));
      check_smp_option(cpu_nr);
   }
}


/*
 ***************************************************************************
 * Fill loc_time structure according to time data saved in current
 * structure.
 ***************************************************************************
*/
void set_loc_time(short curr)
{
   struct tm *ltm;

   /* NOTE: loc_time structure must have been init'ed before! */

   /* Check if option -t was specified on the command line */
   if (PRINT_TRUE_TIME(flags)) {
      /* -t */
      loc_time.tm_hour = file_stats[curr].hour;
      loc_time.tm_min  = file_stats[curr].minute;
      loc_time.tm_sec  = file_stats[curr].second;
   }
   else {
      ltm = localtime(&file_stats[curr].ust_time);
      loc_time = *ltm;
   }
}


/*
 ***************************************************************************
 * Set timestamp string
 ***************************************************************************
*/
void set_timestamp(short curr, char *cur_time, int len)
{
   set_loc_time(curr);

   /* Set cur_time date value */
   strftime(cur_time, len, "%X", &loc_time);
}


/*
 ***************************************************************************
 * Core function used to display statistics
 ***************************************************************************
 */
void write_stats_core(short prev, short curr, short dis, char *prev_string,
		      char *curr_string, unsigned int act,
		      unsigned long long itv, unsigned long long g_itv,
		      int disp_avg, int want_since_boot)
{
   int i, j = 0, k;
   struct file_stats
      *fsi = &file_stats[curr],
      *fsj = &file_stats[prev];

   /*
    * Under very special circumstances, STDOUT may become unavailable,
    * This is what we try to guess here
    */
   if (write(STDOUT_FILENO, "", 0) == -1) {
      perror("stdout");
      exit(6);
   }

   /* Print number of processes created per second */
   if (GET_PROC(act)) {
      if (dis)
	 printf("\n%-11s    proc/s\n", prev_string);

      printf("%-11s %9.2f\n", curr_string,
	     S_VALUE(fsj->processes, fsi->processes, itv));
   }

   /* Print number of context switches per second */
   if (GET_CTXSW(act)) {
      if (dis)
	 printf("\n%-11s   cswch/s\n", prev_string);

      printf("%-11s %9.2f\n", curr_string,
	     ll_s_value(fsj->context_swtch, fsi->context_swtch, itv));
   }

   /* Print CPU usage */
   if (GET_CPU(act)) {
      if (dis)
	 printf("\n%-11s       CPU     %%user     %%nice   %%system   "
		"%%iowait     %%idle\n",
		prev_string);

      if (!WANT_PER_PROC(flags) ||
	  (WANT_PER_PROC(flags) && WANT_ALL_PROC(flags))) {

	 printf("%-11s       all", curr_string);

	 printf("    %6.2f    %6.2f    %6.2f    %6.2f",
		ll_sp_value(fsj->cpu_user, fsi->cpu_user, g_itv),
		ll_sp_value(fsj->cpu_nice, fsi->cpu_nice, g_itv),
		ll_sp_value(fsj->cpu_system, fsi->cpu_system, g_itv),
		ll_sp_value(fsj->cpu_iowait, fsi->cpu_iowait, g_itv));

	 printf("    %6.2f\n",
		fsi->cpu_idle < fsj->cpu_idle ? /* Handle buggy kernels */
		0.0 : ll_sp_value(fsj->cpu_idle, fsi->cpu_idle, g_itv));
      }

      if (WANT_PER_PROC(flags) && file_hdr.sa_proc) {
	 unsigned long long pc_itv;
	 struct stats_one_cpu
	    *sci = st_cpu[curr],
	    *scj = st_cpu[prev];
	
	 for (i = 0; i <= file_hdr.sa_proc; i++, sci++, scj++) {
	    if (cpu_bitmap[i >> 3] & (1 << (i & 0x07))) {

	       printf("%-11s       %3d", curr_string, i);
	
	       /* Recalculate itv for current proc */
	       pc_itv = get_per_cpu_interval(sci, scj);
	
	       printf("    %6.2f    %6.2f    %6.2f    %6.2f",
		      ll_sp_value(scj->per_cpu_user, sci->per_cpu_user, pc_itv),
		      ll_sp_value(scj->per_cpu_nice, sci->per_cpu_nice, pc_itv),
		      ll_sp_value(scj->per_cpu_system, sci->per_cpu_system, pc_itv),
		      ll_sp_value(scj->per_cpu_iowait, sci->per_cpu_iowait, pc_itv));

	       printf("    %6.2f\n",
		      sci->per_cpu_idle < scj->per_cpu_idle ?
		      0.0 : ll_sp_value(scj->per_cpu_idle, sci->per_cpu_idle, pc_itv));
	    }
	 }
      }
   }

   if (GET_IRQ(act) &&
       (!WANT_PER_PROC(flags) ||
	(WANT_PER_PROC(flags) && WANT_ALL_PROC(flags)))) {

	 if (dis)
	    printf("\n%-11s      INTR    intr/s\n", prev_string);

	 /* Print number of interrupts per second */
	 printf("%-11s       sum", curr_string);

	 printf(" %9.2f\n",
		ll_s_value(fsj->irq_sum, fsi->irq_sum, itv));
   }

   if (GET_ONE_IRQ(act)) {
      if (dis)
	 printf("\n%-11s      INTR    intr/s\n", prev_string);

      /* Print number of interrupts per second */
      for (i = 0; i < NR_IRQS; i++) {
	 if (irq_bitmap[i >> 3] & (1 << (i & 0x07))) {

	    printf("%-11s       %3d", curr_string, i);

	    printf(" %9.2f\n",
		   S_VALUE(interrupts[prev][i], interrupts[curr][i], itv));
	 }
      }
   }

   /* Print paging statistics */
   if (GET_PAGE(act)) {
      if (dis)
	 printf("\n%-11s  pgpgin/s pgpgout/s   fault/s  majflt/s\n",
		prev_string);

      printf("%-11s %9.2f %9.2f %9.2f %9.2f\n", curr_string,
	     S_VALUE(fsj->pgpgin, fsi->pgpgin, itv),
	     S_VALUE(fsj->pgpgout, fsi->pgpgout, itv),
	     S_VALUE(fsj->pgfault, fsi->pgfault, itv),
	     S_VALUE(fsj->pgmajfault, fsi->pgmajfault, itv));
   }

   /* Print number of swap pages brought in and out */
   if (GET_SWAP(act)) {
      if (dis)
	 printf("\n%-11s  pswpin/s pswpout/s\n", prev_string);

      printf("%-11s %9.2f %9.2f\n", curr_string,
	     S_VALUE(fsj->pswpin, fsi->pswpin, itv),
	     S_VALUE(fsj->pswpout, fsi->pswpout, itv));
   }

   /* Print I/O stats (no distinction made between disks) */
   if (GET_IO(act)) {
      if (dis)
	 printf("\n%-11s       tps      rtps      wtps   bread/s   bwrtn/s\n",
		prev_string);

      printf("%-11s %9.2f %9.2f %9.2f %9.2f %9.2f\n", curr_string,
	     S_VALUE(fsj->dk_drive, fsi->dk_drive, itv),
	     S_VALUE(fsj->dk_drive_rio, fsi->dk_drive_rio, itv),
	     S_VALUE(fsj->dk_drive_wio, fsi->dk_drive_wio, itv),
	     S_VALUE(fsj->dk_drive_rblk, fsi->dk_drive_rblk, itv),
	     S_VALUE(fsj->dk_drive_wblk, fsi->dk_drive_wblk, itv));
   }

   /* Print memory stats */
   if (GET_MEMORY(act)) {
      if (dis)
	 printf("\n%-11s   frmpg/s   bufpg/s   campg/s\n", prev_string);

      printf("%-11s %9.2f %9.2f %9.2f\n", curr_string,
	     S_VALUE((double) PG(fsj->frmkb), (double) PG(fsi->frmkb), itv),
	     S_VALUE((double) PG(fsj->bufkb), (double) PG(fsi->bufkb), itv),
	     S_VALUE((double) PG(fsj->camkb), (double) PG(fsi->camkb), itv));
   }

   /* Print per-process statistics */
   if (GET_PID(act)) {
      if (dis) {
	 printf("\n%-11s       PID  minflt/s  majflt/s     %%user   %%system   nswap/s",
		prev_string);
	 if (!disp_avg)
	    printf("   CPU\n");
	 else
	    printf("\n");
      }

      for (i = 0; i < pid_nr; i++) {
	 if (!pid_stats[curr][i]->pid || !(pid_stats[curr][i]->flag & X_PID_SET))
	    continue;

 	 printf("%-11s %9ld", curr_string, pid_stats[curr][i]->pid);

	 printf(" %9.2f %9.2f    %6.2f    %6.2f %9.2f",
		S_VALUE(pid_stats[prev][i]->minflt, pid_stats[curr][i]->minflt, itv),
		S_VALUE(pid_stats[prev][i]->majflt, pid_stats[curr][i]->majflt, itv),
		SP_VALUE(pid_stats[prev][i]->utime, pid_stats[curr][i]->utime, itv),
		SP_VALUE(pid_stats[prev][i]->stime, pid_stats[curr][i]->stime, itv),
		S_VALUE((long) pid_stats[prev][i]->nswap,
			(long) pid_stats[curr][i]->nswap, itv));

	 if (!disp_avg)
	    printf("   %3d\n", pid_stats[curr][i]->processor);
	 else
	    printf("\n");
      }
   }

   /* Print statistics about children of a given process */
   if (GET_CPID(act)) {
      if (dis)
	 printf("\n%-11s      PPID cminflt/s cmajflt/s    %%cuser  %%csystem  cnswap/s\n",
		prev_string);

      for (i = 0; i < pid_nr; i++) {
	 if (!pid_stats[curr][i]->pid || !(pid_stats[curr][i]->flag & X_PPID_SET))
	    continue;
	 printf("%-11s %9ld", curr_string, pid_stats[curr][i]->pid);

	 printf(" %9.2f %9.2f    %6.2f    %6.2f %9.2f\n",
		S_VALUE(pid_stats[prev][i]->cminflt, pid_stats[curr][i]->cminflt, itv),
		S_VALUE(pid_stats[prev][i]->cmajflt, pid_stats[curr][i]->cmajflt, itv),
		SP_VALUE(pid_stats[prev][i]->cutime,  pid_stats[curr][i]->cutime,  itv),
		SP_VALUE(pid_stats[prev][i]->cstime,  pid_stats[curr][i]->cstime,  itv),
		S_VALUE((long) pid_stats[prev][i]->cnswap,
			(long) pid_stats[curr][i]->cnswap, itv));
      }
   }

   /* Print TTY statistics (serial lines) */
   if (GET_SERIAL(act)) {
      struct stats_serial
	 *ssi = st_serial[curr],
	 *ssj = st_serial[prev];

      if (dis)
	 printf("\n%-11s       TTY   rcvin/s   xmtin/s framerr/s prtyerr/s     "
		"brk/s   ovrun/s\n", prev_string);

      for (i = 0; i < file_hdr.sa_serial; i++, ssi++, ssj++) {

	 if (ssi->line == ~0)
	    continue;
	
	 printf("%-11s       %3d", curr_string, ssi->line);

	 if ((ssi->line == ssj->line) || want_since_boot) {
	    printf(" %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n",
		   S_VALUE(ssj->rx, ssi->rx, itv),
		   S_VALUE(ssj->tx, ssi->tx, itv),
		   S_VALUE(ssj->frame, ssi->frame, itv),
		   S_VALUE(ssj->parity, ssi->parity, itv),
		   S_VALUE(ssj->brk, ssi->brk, itv),
		   S_VALUE(ssj->overrun, ssi->overrun, itv));
	 }
	 else
	    printf("       N/A       N/A       N/A       N/A       N/A       N/A\n");
      }
   }

   if (GET_IRQ(act) && WANT_PER_PROC(flags) && file_hdr.sa_irqcpu) {
      int offset;
      struct stats_irq_cpu *p, *q, *p0, *q0;

      j = 0;
      /* Check if number of interrupts has changed */
      if (!dis && !want_since_boot && !disp_avg) {
	 do {
	    p0 = st_irq_cpu[curr] + j;
	    if (p0->irq != ~0) {
	       q0 = st_irq_cpu[prev] + j;
	       if (p0->irq != q0->irq)
		  j = -2;
	    }
	    j++;
	 }
	 while ((j > 0) && (j <= file_hdr.sa_irqcpu));
      }

      if (dis || (j < 0)) {
	 /* Print header */
	 printf("\n%-11s  CPU", prev_string);
	 for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	    p0 = st_irq_cpu[curr] + j;
	    if (p0->irq != ~0)	/* Nb of irq per proc may have varied... */
	       printf("  i%03d/s", p0->irq);
	 }
	 printf("\n");
      }

      for (k = 0; k <= file_hdr.sa_proc; k++) {
	 if (cpu_bitmap[k >> 3] & (1 << (k & 0x07))) {

	    printf("%-11s  %3d", curr_string, k);

	    for (j = 0; j < file_hdr.sa_irqcpu; j++) {
	       p0 = st_irq_cpu[curr] + j;	/* irq field set only for proc #0 */
	       /*
		* A value of ~0 means it is a remaining interrupt
		* which is no longer used, for example because the
		* number of interrupts has decreased in /proc/interrupts
		* or because we are appending data to an old sa file
		* with more interrupts than are actually available now.
		*/
	       if (p0->irq != ~0) {	
		  q0 = st_irq_cpu[prev] + j;
		  offset = j;

		  /*
		   * If we want stats for the time since system startup,
		   * we have p0->irq != q0->irq, since q0 structure is
		   * completely set to zero.
		   */
		  if ((p0->irq != q0->irq) && !want_since_boot) {
		     if (j)
			offset = j - 1;
		     q0 = st_irq_cpu[prev] + offset;
		     if ((p0->irq != q0->irq) && (j + 1 < file_hdr.sa_irqcpu))
			offset = j + 1;
		     q0 = st_irq_cpu[prev] + offset;
		  }
		  if ((p0->irq == q0->irq) || want_since_boot) {
		     p = st_irq_cpu[curr] + k * file_hdr.sa_irqcpu + j;
		     q = st_irq_cpu[prev] + k * file_hdr.sa_irqcpu + offset;
		     printf(" %7.2f",
			    S_VALUE(q->interrupt, p->interrupt, itv));
		  }
		  else
		     printf("     N/A");
	       }
	    }
	    printf("\n");
	 }
      }
   }

   /* Print network interface statistics */
   if (GET_NET_DEV(act)) {
      struct stats_net_dev
	 *sndi = st_net_dev[curr],
	 *sndj;

      if (dis)
	 printf("\n%-11s     IFACE   rxpck/s   txpck/s   rxbyt/s   txbyt/s   "
		"rxcmp/s   txcmp/s  rxmcst/s\n",
		prev_string);

      for (i = 0; i < file_hdr.sa_iface; i++, sndi++) {

	 if (!strcmp(sndi->interface, "?"))
	    continue;
	 j = check_iface_reg(&file_hdr, st_net_dev, curr, prev, i);
	 sndj = st_net_dev[prev] + j;
	
	 printf("%-11s %9s", curr_string, sndi->interface);
	
	 printf(" %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n",
		S_VALUE(sndj->rx_packets, sndi->rx_packets, itv),
		S_VALUE(sndj->tx_packets, sndi->tx_packets, itv),
		S_VALUE(sndj->rx_bytes, sndi->rx_bytes, itv),
		S_VALUE(sndj->tx_bytes, sndi->tx_bytes, itv),
		S_VALUE(sndj->rx_compressed, sndi->rx_compressed, itv),
		S_VALUE(sndj->tx_compressed, sndi->tx_compressed, itv),
		S_VALUE(sndj->multicast, sndi->multicast, itv));
      }
   }

   /* Print network interface statistics (errors) */
   if (GET_NET_EDEV(act)) {
      struct stats_net_dev
	 *sndi = st_net_dev[curr],
	 *sndj;

      if (dis)
	 printf("\n%-11s     IFACE   rxerr/s   txerr/s    coll/s  rxdrop/s  "
		"txdrop/s  txcarr/s  rxfram/s  rxfifo/s  txfifo/s\n",
		prev_string);

      for (i = 0; i < file_hdr.sa_iface; i++, sndi++) {

	 if (!strcmp(sndi->interface, "?"))
	    continue;
	 j = check_iface_reg(&file_hdr, st_net_dev, curr, prev, i);
	 sndj = st_net_dev[prev] + j;
	
	 printf("%-11s %9s", curr_string, sndi->interface);

	 printf(" %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n",
		S_VALUE(sndj->rx_errors, sndi->rx_errors, itv),
		S_VALUE(sndj->tx_errors, sndi->tx_errors, itv),
		S_VALUE(sndj->collisions, sndi->collisions, itv),
		S_VALUE(sndj->rx_dropped, sndi->rx_dropped, itv),
		S_VALUE(sndj->tx_dropped, sndi->tx_dropped, itv),
		S_VALUE(sndj->tx_carrier_errors, sndi->tx_carrier_errors, itv),
		S_VALUE(sndj->rx_frame_errors, sndi->rx_frame_errors, itv),
		S_VALUE(sndj->rx_fifo_errors, sndi->rx_fifo_errors, itv),
		S_VALUE(sndj->tx_fifo_errors, sndi->tx_fifo_errors, itv));
      }
   }

   /* Print disk statistics */
   if (GET_DISK(act)) {
      double tput, util, await, svctm, arqsz;
      struct disk_stats
	 *sdi = st_disk[curr],
	 *sdj;

      if (dis)
	 printf("\n%-11s       DEV       tps  rd_sec/s  wr_sec/s  avgrq-sz  "
		"avgqu-sz     await     svctm     %%util\n",
		prev_string);

      for (i = 0; i < file_hdr.sa_nr_disk; i++, ++sdi) {
	
	 if (!(sdi->major + sdi->minor))
	    continue;

	 j = check_disk_reg(&file_hdr, st_disk, curr, prev, i);
	 sdj = st_disk[prev] + j;

	 tput = ((double) (sdi->nr_ios - sdj->nr_ios)) * HZ / itv;
	 util = S_VALUE(sdj->tot_ticks, sdi->tot_ticks, itv);
	 svctm = tput ? util / tput : 0.0;
	 await = (sdi->nr_ios - sdj->nr_ios) ?
	    ((sdi->rd_ticks - sdj->rd_ticks) + (sdi->wr_ticks - sdj->wr_ticks)) /
	    ((double) (sdi->nr_ios - sdj->nr_ios)) : 0.0;
	 arqsz  = (sdi->nr_ios - sdj->nr_ios) ?
	    ((sdi->rd_sect - sdj->rd_sect) + (sdi->wr_sect - sdj->wr_sect)) /
	    ((double) (sdi->nr_ios - sdj->nr_ios)) : 0.0;

	 printf("%-11s %9s %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n",
		curr_string,
		/* Confusion possible here between index and minor numbers */
		get_devname(sdi->major, sdi->minor, USE_PRETTY_OPTION(flags)),
		S_VALUE(sdj->nr_ios, sdi->nr_ios,  itv),
		ll_s_value(sdj->rd_sect, sdi->rd_sect, itv),
		ll_s_value(sdj->wr_sect, sdi->wr_sect, itv),
		/* See iostat for explanations */
		arqsz,
		S_VALUE(sdj->rq_ticks, sdi->rq_ticks, itv) / 1000.0,
		await,
		svctm,
		util / 10.0);
      }
   }

   /* Print NFS client stats */
   if (GET_NET_NFS(act)) {
      if (dis)
	 printf("\n%-11s    call/s retrans/s    read/s   write/s  access/s  getatt/s\n",
		prev_string);

      printf("%-11s %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n", curr_string,
	     S_VALUE(fsj->nfs_rpccnt, fsi->nfs_rpccnt, itv),
	     S_VALUE(fsj->nfs_rpcretrans, fsi->nfs_rpcretrans, itv),
	     S_VALUE(fsj->nfs_readcnt, fsi->nfs_readcnt, itv),
	     S_VALUE(fsj->nfs_writecnt, fsi->nfs_writecnt, itv),
	     S_VALUE(fsj->nfs_accesscnt, fsi->nfs_accesscnt, itv),
	     S_VALUE(fsj->nfs_getattcnt, fsi->nfs_getattcnt, itv));
   }

   /* Print NFS server stats */
   if (GET_NET_NFSD(act)) {
      if (dis)
	 printf("\n%-11s   scall/s badcall/s  packet/s     udp/s     tcp/s     "
		"hit/s    miss/s   sread/s  swrite/s saccess/s sgetatt/s\n",
		prev_string);

      printf("%-11s %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f %9.2f\n",
	     curr_string,
	     S_VALUE(fsj->nfsd_rpccnt, fsi->nfsd_rpccnt, itv),
	     S_VALUE(fsj->nfsd_rpcbad, fsi->nfsd_rpcbad, itv),
	     S_VALUE(fsj->nfsd_netcnt, fsi->nfsd_netcnt, itv),
	     S_VALUE(fsj->nfsd_netudpcnt, fsi->nfsd_netudpcnt, itv),
	     S_VALUE(fsj->nfsd_nettcpcnt, fsi->nfsd_nettcpcnt, itv),
	     S_VALUE(fsj->nfsd_rchits, fsi->nfsd_rchits, itv),
	     S_VALUE(fsj->nfsd_rcmisses, fsi->nfsd_rcmisses, itv),
	     S_VALUE(fsj->nfsd_readcnt, fsi->nfsd_readcnt, itv),
	     S_VALUE(fsj->nfsd_writecnt, fsi->nfsd_writecnt, itv),
	     S_VALUE(fsj->nfsd_accesscnt, fsi->nfsd_accesscnt, itv),
	     S_VALUE(fsj->nfsd_getattcnt, fsi->nfsd_getattcnt, itv));
   }
}


/*
 ***************************************************************************
 * Print statistics average
 ***************************************************************************
 */
void write_stats_avg(int curr, short dis, unsigned int act, int read_from_file)
{
   unsigned long long itv, g_itv;
   char string[16];
   struct file_stats
     *fsi = &file_stats[curr];

   /* Interval value in jiffies */
   g_itv = (file_stats[curr].uptime - file_stats[2].uptime) & 0xffffffff;
   if (file_hdr.sa_proc)
      itv = (file_stats[curr].uptime0 - file_stats[2].uptime0) & 0xffffffff;
   else
      itv = g_itv;

   if (!itv || !g_itv) { /* Should no longer happen with recent versions */
      /*
       * Aiee: null interval... This should only happen when reading stats
       * from a system activity file with a far too big interval value...
       */
      fprintf(stderr, _("Please give a smaller interval value\n"));
      exit(1);
   }

   strcpy(string, _("Average:"));
   write_stats_core(2, curr, dis, string, string, act, itv, g_itv, TRUE, FALSE);

   if (GET_MEM_AMT(act)) {
      if (dis)
	 printf("\n%-11s kbmemfree kbmemused  %%memused kbbuffers  kbcached kbswpfree "
		"kbswpused  %%swpused  kbswpcad\n",
		string);

      printf("%-11s %9.0f %9.0f    %6.2f %9.0f %9.0f %9.0f %9.0f    %6.2f %9.0f\n",
	     string,
	     (double) asum.frmkb / asum.count,
	     (double) fsi->tlmkb - ((double) asum.frmkb / asum.count),
	     fsi->tlmkb ?
	     SP_VALUE(asum.frmkb / asum.count, fsi->tlmkb, fsi->tlmkb) : 0.0,
	     (double) asum.bufkb / asum.count,
	     (double) asum.camkb / asum.count,
	     (double) asum.frskb / asum.count,
	     ((double) asum.tlskb / asum.count) - ((double) asum.frskb / asum.count),
	     (asum.tlskb / asum.count) ?
	     SP_VALUE(asum.frskb / asum.count,
		      asum.tlskb / asum.count, asum.tlskb / asum.count)
	     : 0.0,
	     (double) asum.caskb / asum.count);
   }

   if (GET_KTABLES(act)) {
      if (dis)
	 printf("\n%-11s dentunusd   file-sz  inode-sz  super-sz %%super-sz  "
		"dquot-sz %%dquot-sz  rtsig-sz %%rtsig-sz\n",
		string);

      printf("%-11s %9.0f %9.0f %9.0f %9.0f    %6.2f %9.0f    %6.2f %9.0f    %6.2f\n",
	     string,
	     (double) asum.dentry_stat / asum.count,
	     (double) asum.file_used / asum.count,
	     (double) asum.inode_used / asum.count,
	     (double) asum.super_used / asum.count,
	     fsi->super_max ?
	     ((double) ((asum.super_used / asum.count) * 100)) / fsi->super_max
	     : 0.0,
	     (double) asum.dquot_used / asum.count,
	     fsi->dquot_max ?
	     ((double) ((asum.dquot_used / asum.count) * 100)) / fsi->dquot_max
	     : 0.0,
	     (double) asum.rtsig_queued / asum.count,
	     fsi->rtsig_max ?
	     ((double) ((asum.rtsig_queued / asum.count) * 100)) / fsi->rtsig_max
	     : 0.0);
   }

   if (GET_NET_SOCK(act)) {
      if (dis)
	 printf("\n%-11s    totsck    tcpsck    udpsck    rawsck   ip-frag\n",
		string);

      printf("%-11s %9.0f %9.0f %9.0f %9.0f %9.0f\n", string,
	     (double) asum.sock_inuse / asum.count,
	     (double) asum.tcp_inuse / asum.count,
	     (double) asum.udp_inuse / asum.count,
	     (double) asum.raw_inuse / asum.count,
	     (double) asum.frag_inuse / asum.count);
   }

   if (GET_QUEUE(act)) {
      if (dis)
	 printf("\n%-11s   runq-sz  plist-sz   ldavg-1   ldavg-5  ldavg-15\n",
		string);

      printf("%-11s %9.0f %9.0f %9.2f %9.2f %9.2f\n", string,
	     (double) asum.nr_running / asum.count,
	     (double) asum.nr_threads / asum.count,
	     (double) asum.load_avg_1 / (asum.count * 100),
	     (double) asum.load_avg_5 / (asum.count * 100),
	     (double) asum.load_avg_15 / (asum.count * 100));
   }

   if (read_from_file)
      /* Reset counters only if we read stats from a system activity file */
      memset(&asum, 0, STATS_SUM_SIZE);
}


/*
 ***************************************************************************
 * Print system statistics
 ***************************************************************************
 */
int write_stats(short curr, short dis, unsigned int act, int read_from_file,
		long *cnt, int use_tm_start, int use_tm_end, int reset,
		int want_since_boot)
{
   char cur_time[2][16];
   unsigned long long itv, g_itv;
   struct file_stats
      *fsi = &file_stats[curr];

   /* Check time (1) */
   if (read_from_file) {
      if (!next_slice(file_stats[2].uptime, file_stats[curr].uptime,
		      &file_hdr, reset, interval))
	 /* Not close enough to desired interval */
	 return 0;
   }

   /* Set previous timestamp */
   set_timestamp(!curr, cur_time[!curr], 16);
   /* Set current timestamp */
   set_timestamp(curr, cur_time[curr], 16);

   /* Check time (2) */
   if (use_tm_start && (datecmp(&loc_time, &tm_start) < 0))
     /* it's too soon... */
     return 0;

   /* Get interval values */
   get_itv_value(&file_stats[curr], &file_stats[!curr],
		 file_hdr.sa_proc, &itv, &g_itv);

   /* Check time (3) */
   if (use_tm_end && (datecmp(&loc_time, &tm_end) > 0)) {
      /* It's too late... */
      *cnt = 0;
      return 0;
   }

   (asum.count)++;	/* Nb of lines printed */

   write_stats_core(!curr, curr, dis, cur_time[!curr], cur_time[curr], act,
		    itv, g_itv, FALSE, want_since_boot);

   /* Print amount and usage of memory */
   if (GET_MEM_AMT(act)) {
      if (dis)
	 printf("\n%-11s kbmemfree kbmemused  %%memused kbbuffers  kbcached "
		"kbswpfree kbswpused  %%swpused  kbswpcad\n",
		cur_time[!curr]);

      printf("%-11s %9lu %9lu    %6.2f %9lu %9lu %9lu %9lu    %6.2f %9lu\n",
	     cur_time[curr],
	     fsi->frmkb,
	     fsi->tlmkb - fsi->frmkb,
	     fsi->tlmkb ?
	     SP_VALUE(fsi->frmkb, fsi->tlmkb, fsi->tlmkb) : 0.0,
	     fsi->bufkb,
	     fsi->camkb,
	     fsi->frskb,
	     fsi->tlskb - fsi->frskb,
	     fsi->tlskb ?
	     SP_VALUE(fsi->frskb, fsi->tlskb, fsi->tlskb) : 0.0,
	     fsi->caskb);

      /*
       * Will be used to compute the average.
       * Note: overflow unlikely to happen but not impossible...
       * We assume that the total amount of memory installed can not vary
       * during the interval given on the command line, whereas the total
       * amount of swap space may.
       */
      asum.frmkb += fsi->frmkb;
      asum.bufkb += fsi->bufkb;
      asum.camkb += fsi->camkb;
      asum.frskb += fsi->frskb;
      asum.tlskb += fsi->tlskb;
      asum.caskb += fsi->caskb;
   }

   /* Print values of some kernel tables */
   if (GET_KTABLES(act)) {
      if (dis)
	 printf("\n%-11s dentunusd   file-sz  inode-sz  super-sz %%super-sz  "
		"dquot-sz %%dquot-sz  rtsig-sz %%rtsig-sz\n",
		cur_time[!curr]);

      printf("%-11s %9u %9u %9u %9u    %6.2f %9u    %6.2f %9u    %6.2f\n",
	     cur_time[curr],
	     fsi->dentry_stat,
	     fsi->file_used,
	     fsi->inode_used,
	     fsi->super_used,
	     fsi->super_max ?
	     ((double) (fsi->super_used * 100)) / fsi->super_max : 0.0,
	     fsi->dquot_used,
	     fsi->dquot_max ?
	     ((double) (fsi->dquot_used * 100)) / fsi->dquot_max : 0.0,
	     fsi->rtsig_queued,
	     fsi->rtsig_max ?
	     ((double) (fsi->rtsig_queued * 100)) / fsi->rtsig_max : 0.0);

      /*
       * Will be used to compute the average.
       * Note: overflow unlikely to happen but not impossible...
       * We assume that *_max values can not vary during the interval.
       */
      asum.dentry_stat += fsi->dentry_stat;
      asum.file_used += fsi->file_used;
      asum.inode_used += fsi->inode_used;
      asum.super_used += fsi->super_used;
      asum.dquot_used += fsi->dquot_used;
      asum.rtsig_queued += fsi->rtsig_queued;
   }

   /* Print number of sockets in use */
   if (GET_NET_SOCK(act)) {
      if (dis)
	 printf("\n%-11s    totsck    tcpsck    udpsck    rawsck   ip-frag\n",
		cur_time[!curr]);

      printf("%-11s %9u %9u %9u %9u %9u\n", cur_time[curr],
	     fsi->sock_inuse, fsi->tcp_inuse, fsi->udp_inuse,
	     fsi->raw_inuse, fsi->frag_inuse);

      /* Will be used to compute the average */
      asum.sock_inuse += fsi->sock_inuse;
      asum.tcp_inuse += fsi->tcp_inuse;
      asum.udp_inuse += fsi->udp_inuse;
      asum.raw_inuse += fsi->raw_inuse;
      asum.frag_inuse += fsi->frag_inuse;
   }

   /* Print load averages and queue length */
   if (GET_QUEUE(act)) {
      if (dis)
	 printf("\n%-11s   runq-sz  plist-sz   ldavg-1   ldavg-5  ldavg-15\n",
		cur_time[!curr]);

      printf("%-11s %9lu %9u %9.2f %9.2f %9.2f\n", cur_time[curr],
	     fsi->nr_running, fsi->nr_threads,
	     (double) fsi->load_avg_1 / 100,
	     (double) fsi->load_avg_5 / 100,
	     (double) fsi->load_avg_15 / 100);

      /* Will be used to compute the average */
      asum.nr_running += fsi->nr_running;
      asum.nr_threads += fsi->nr_threads;
      asum.load_avg_1 += fsi->load_avg_1;
      asum.load_avg_5 += fsi->load_avg_5;
      asum.load_avg_15 += fsi->load_avg_15;
   }

   return 1;
}


/*
 ***************************************************************************
 * Display stats since system startup
 ***************************************************************************
 */
void write_stats_startup(short curr)
{
   /* Set to 0 previous structures corresponding to boot time */
   memset(&file_stats[!curr], 0, FILE_STATS_SIZE);
   file_stats[!curr].record_type = R_STATS;
   file_stats[!curr].hour        = file_stats[curr].hour;
   file_stats[!curr].minute      = file_stats[curr].minute;
   file_stats[!curr].second      = file_stats[curr].second;
   file_stats[!curr].ust_time    = file_stats[curr].ust_time;
   if (file_hdr.sa_proc > 0)
      memset(st_cpu[!curr], 0, STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1));
   memset(interrupts[!curr], 0, STATS_ONE_IRQ_SIZE);
   if (pid_nr)
      memset (pid_stats[!curr][0], 0, PID_STATS_SIZE * pid_nr);
   if (file_hdr.sa_serial)
      memset(st_serial[!curr], 0, STATS_SERIAL_SIZE * file_hdr.sa_serial);
   if (file_hdr.sa_irqcpu)
      memset(st_irq_cpu[!curr], 0, STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu);
   if (file_hdr.sa_iface)
      memset(st_net_dev[!curr], 0, STATS_NET_DEV_SIZE * file_hdr.sa_iface);
   if (file_hdr.sa_nr_disk)
      memset(st_disk[!curr], 0, DISK_STATS_SIZE * file_hdr.sa_nr_disk);
	
   /* Display stats since boot time */
   write_stats(curr, DISP_HDR, sar_actflag, USE_SADC, &count,
	       NO_TM_START, NO_TM_END, NO_RESET, ST_SINCE_BOOT);
   exit(0);
   }


/*
 ***************************************************************************
 * Read data sent by the data collector
 ***************************************************************************
 */
int sa_read(void *buffer, int size)
{
   int n;

   while (size) {

      if ((n = read(STDIN_FILENO, buffer, size)) < 0) {
	 perror("read");
	 exit(2);
      }

      if (!n)
	 return 1;	/* EOF */

      size -= n;
      buffer = (char *) buffer + n;
   }

   return 0;
}


/*
 ***************************************************************************
 * Print a Linux restart message (contents of a DUMMY record)
 ***************************************************************************
 */
void write_dummy(short curr, int use_tm_start, int use_tm_end)
{
   char cur_time[26];

   set_timestamp(curr, cur_time, 26);

   /* The RESTART message must be in the interval specified by -s/-e options */
   if ((use_tm_start && (datecmp(&loc_time, &tm_start) < 0)) ||
       (use_tm_end && (datecmp(&loc_time, &tm_end) > 0)))
      return;

   printf("\n%-11s       LINUX RESTART\n", cur_time);
}


/*
 ***************************************************************************
 * Move structures data
 ***************************************************************************
 */
void copy_structures(int dest, int src, int stype)
{
   memcpy(&file_stats[dest], &file_stats[src], FILE_STATS_SIZE);
   if (file_hdr.sa_proc > 0)
      memcpy(st_cpu[dest], st_cpu[src],
	     STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1));
   if (GET_ONE_IRQ(file_hdr.sa_actflag))
      memcpy(interrupts[dest], interrupts[src],
	     STATS_ONE_IRQ_SIZE);
   if ((stype == USE_SADC) && (pid_nr))
      memcpy(pid_stats[dest][0], pid_stats[src][0],
	     PID_STATS_SIZE * pid_nr);
   if (file_hdr.sa_serial)
      memcpy(st_serial[dest], st_serial[src],
	     STATS_SERIAL_SIZE * file_hdr.sa_serial);
   if (file_hdr.sa_irqcpu)
      memcpy(st_irq_cpu[dest], st_irq_cpu[src],
	     STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu);
   if (file_hdr.sa_iface)
      memcpy(st_net_dev[dest], st_net_dev[src],
	     STATS_NET_DEV_SIZE * file_hdr.sa_iface);
   if (file_hdr.sa_nr_disk)
      memcpy(st_disk[dest], st_disk[src],
	     DISK_STATS_SIZE * file_hdr.sa_nr_disk);
}


/*
 ***************************************************************************
 * Read varying part of the statistics from a daily data file
 ***************************************************************************
 */
void read_extra_stats(short curr, int ifd)
{
   if (file_hdr.sa_proc > 0)
      sa_fread(ifd, st_cpu[curr],
	       STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1), HARD_SIZE);
   if (GET_ONE_IRQ(file_hdr.sa_actflag))
      sa_fread(ifd, interrupts[curr],
	       STATS_ONE_IRQ_SIZE, HARD_SIZE);
   if (file_hdr.sa_serial)
      sa_fread(ifd, st_serial[curr],
	       STATS_SERIAL_SIZE * file_hdr.sa_serial, HARD_SIZE);
   if (file_hdr.sa_irqcpu)
      sa_fread(ifd, st_irq_cpu[curr],
	       STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu, HARD_SIZE);
   if (file_hdr.sa_iface)
      sa_fread(ifd, st_net_dev[curr],
	       STATS_NET_DEV_SIZE * file_hdr.sa_iface, HARD_SIZE);
   if (file_hdr.sa_nr_disk)
      sa_fread(ifd, st_disk[curr],
	       DISK_STATS_SIZE * file_hdr.sa_nr_disk, HARD_SIZE);
   /* PID stats cannot be saved in file. So we don't read them */
}


/*
 ***************************************************************************
 * Read a bunch of statistics sent by the data collector
 ***************************************************************************
 */
void read_stat_bunch(short curr)
{
   if (sa_read(&file_stats[curr], file_hdr.sa_st_size))
      exit(0);
   if ((file_hdr.sa_proc > 0) &&
       sa_read(st_cpu[curr], STATS_ONE_CPU_SIZE * (file_hdr.sa_proc + 1)))
      exit(0);
   if (GET_ONE_IRQ(file_hdr.sa_actflag) &&
       sa_read(interrupts[curr], STATS_ONE_IRQ_SIZE))
      exit(0);
   if (pid_nr &&
       sa_read(pid_stats[curr][0], PID_STATS_SIZE * pid_nr))
      exit(0);
   if (file_hdr.sa_serial &&
       sa_read(st_serial[curr], STATS_SERIAL_SIZE * file_hdr.sa_serial))
      exit(0);
   if (file_hdr.sa_irqcpu &&
       sa_read(st_irq_cpu[curr], STATS_IRQ_CPU_SIZE * (file_hdr.sa_proc + 1) * file_hdr.sa_irqcpu))
      exit(0);
   if (file_hdr.sa_iface &&
       sa_read(st_net_dev[curr], STATS_NET_DEV_SIZE * file_hdr.sa_iface))
      exit(0);
   if (file_hdr.sa_nr_disk &&
       sa_read(st_disk[curr], DISK_STATS_SIZE * file_hdr.sa_nr_disk))
      exit(0);
}


/*
 ***************************************************************************
 * Read stats for current activity from file and display them.
 ***************************************************************************
 */
void handle_curr_act_stats(int ifd, off_t fpos, short *curr, long *cnt, int *eosaf,
			 int rows, unsigned int act, int *reset)
{
   short dis = 1;
   unsigned long lines;
   unsigned char rtype;
   int davg, next;

   if (lseek(ifd, fpos, SEEK_SET) < fpos) {
      perror("lseek");
      exit(2);
   }

   /*
    * Restore the first stats collected.
    * Used to compute the rate displayed on the first line.
    */
   copy_structures(!(*curr), 2, USE_SA_FILE);
	
   lines = 0;
   davg  = 0;
   *cnt  = count;

   do {
      /* Display count lines of stats */
      *eosaf = sa_fread(ifd, &file_stats[*curr],
			file_hdr.sa_st_size, SOFT_SIZE);
      rtype = file_stats[*curr].record_type;
	
      if (!(*eosaf) && (rtype != R_DUMMY))
	 /* Read the extra fields since it's not a DUMMY record */
	 read_extra_stats(*curr, ifd);

      dis = !(lines++ % rows);

      if (!(*eosaf) && (rtype != R_DUMMY)) {

	 /* next is set to 1 when we were close enough to desired interval */
	 next = write_stats(*curr, dis, act, USE_SA_FILE, cnt,
			    tm_start.use, tm_end.use, *reset, ST_IMMEDIATE);
	 if (next && ((*cnt) > 0))
	    (*cnt)--;
	 if (next) {
	    davg++;
	    *curr ^=1;
	 }
	 else
	    lines--;
	 *reset = 0;
      }
   }
   while ((*cnt) && !(*eosaf) && (rtype != R_DUMMY));

   if (davg)
      write_stats_avg(!(*curr), dis, act, USE_SA_FILE);

   *reset = TRUE;
}


/*
 ***************************************************************************
 * Read header data sent by sadc
 ***************************************************************************
 */
void read_header_data(void)
{
   /* Read stats header */
   if (sa_read(&file_hdr, FILE_HDR_SIZE))
      exit(0);
   if (file_hdr.sa_magic != SA_MAGIC) {
      /* sar and sadc commands are not consistent */
      fprintf(stderr, _("Invalid data format\n"));
      exit(3);
   }

}


/*
 ***************************************************************************
 * Read statistics from a system activity data file
 ***************************************************************************
 */
void read_stats_from_file(char from_file[])
{
   short curr = 1;
   unsigned int act;
   int ifd;
   int rows = 23, eosaf = TRUE, reset = FALSE;
   long cnt = 1;
   off_t fpos;

   if (!dis_hdr)
      /* Get window size */
      rows = get_win_height();

   /* Prepare file for reading */
   prep_file_for_reading(&ifd, from_file, &file_hdr, &sar_actflag, flags);

   if ((GET_SERIAL(sar_actflag) && (file_hdr.sa_serial > 1)) ||
       ((GET_NET_DEV(sar_actflag) || GET_NET_EDEV(sar_actflag)) && (file_hdr.sa_iface  > 1)) ||
       (GET_DISK(sar_actflag) && (file_hdr.sa_nr_disk > 1)))
      dis_hdr = 9;

   /* Perform required allocations */
   allocate_structures(USE_SA_FILE);

   /* Print report header */
   print_report_hdr(S_O_NONE, flags, &loc_time, &file_hdr);

   /* Read system statistics from file */
   do {
      /*
       * If this record is a DUMMY one, print it and (try to) get another one.
       * We must be sure that we have real stats in file_stats[2].
       */
      do {
	 if (sa_fread(ifd, &file_stats[0], file_hdr.sa_st_size, SOFT_SIZE))
	    /* End of sa data file */
	    return;
	
	 if (file_stats[0].record_type == R_DUMMY)
	    write_dummy(0, tm_start.use, tm_end.use);
	 else {
	    /*
	     * Ok: previous record was not a DUMMY one.
	     * So read now the extra fields.
	     */
	    read_extra_stats(0, ifd);
	    set_loc_time(0);
	 }
      }
      while ((file_stats[0].record_type == R_DUMMY) ||
	     (tm_start.use && (datecmp(&loc_time, &tm_start) < 0)) ||
	     (tm_end.use && (datecmp(&loc_time, &tm_end) >=0)));

      /* Save the first stats collected. Will be used to compute the average */
      copy_structures(2, 0, USE_SA_FILE);

      reset = TRUE;	/* Set flag to reset last_uptime variable */

      /* Save current file position */
      if ((fpos = lseek(ifd, 0, SEEK_CUR)) < 0) {
	 perror("lseek");
	 exit(2);
      }

      /* Read and write stats located between two possible Linux restarts */

      /* For each requested activity... */
      for (act = 1; act <= A_LAST; act <<= 1) {

	 if (sar_actflag & act) {
	    if ((act == A_IRQ) && WANT_PER_PROC(flags) && WANT_ALL_PROC(flags)) {
	       /* Distinguish -I SUM activity from IRQs per processor activity */
	       flags &= ~S_F_PER_PROC;
	       handle_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf, rows, act, &reset);
	       flags |= S_F_PER_PROC;
	       flags &= ~S_F_ALL_PROC;
	       handle_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf, rows, act, &reset);
	       flags |= S_F_ALL_PROC;
	    }
	    else
	       handle_curr_act_stats(ifd, fpos, &curr, &cnt, &eosaf, rows, act, &reset);
	 }
      }

      if (!cnt) {
	 /* Go to next Linux restart, if possible */
	 do {
	    eosaf = sa_fread(ifd, &file_stats[curr],
			     file_hdr.sa_st_size, SOFT_SIZE);
	    if (!eosaf && (file_stats[curr].record_type != R_DUMMY))
	       read_extra_stats(curr, ifd);
	 }
	 while (!eosaf && (file_stats[curr].record_type != R_DUMMY));
      }

      /* The last record we read was a DUMMY one: print it */
      if (!eosaf && (file_stats[curr].record_type == R_DUMMY))
	 write_dummy(curr, tm_start.use, tm_end.use);
   }
   while (!eosaf);

   close(ifd);
}


/*
 ***************************************************************************
 * Read statistics sent by sadc, the data collector.
 ***************************************************************************
 */
void read_stats(void)
{
   short curr = 1, dis = 1;
   unsigned long lines = 0;
   unsigned int rows = 23, more = 1;

   /* Read stats header */
   read_header_data();

   /* Force '-P ALL' flag if -A option is used on SMP machines */
   if (USE_A_OPTION(flags) && file_hdr.sa_proc) {
      init_bitmap(cpu_bitmap, ~0, NR_CPUS);
      flags |= S_F_ALL_PROC + S_F_PER_PROC;
   }

   /*
    * Check that data corresponding to requested activities
    * are sent by the data collector.
    */
   if (GET_SERIAL(sar_actflag) && !file_hdr.sa_serial) {
      sar_actflag &= ~A_SERIAL;
      dis_hdr--;
   }
   if ((sar_actflag & (A_NET_DEV + A_NET_EDEV)) && !file_hdr.sa_iface) {
      if (GET_NET_DEV(sar_actflag) && GET_NET_EDEV(sar_actflag))
	 dis_hdr--;
      sar_actflag &= ~(A_NET_DEV + A_NET_EDEV);
      dis_hdr--;
   }
   if (GET_DISK(sar_actflag) && !file_hdr.sa_nr_disk) {
      sar_actflag &= ~A_DISK;
      dis_hdr--;
   }
   if (!sar_actflag) {
      fprintf(stderr, _("Requested activities not available\n"));
      exit(1);
   }

   if ((GET_SERIAL(sar_actflag) && (file_hdr.sa_serial > 1)) ||
       ((GET_NET_DEV(sar_actflag) || GET_NET_EDEV(sar_actflag)) && (file_hdr.sa_iface  > 1)) ||
       (GET_DISK(sar_actflag) && (file_hdr.sa_nr_disk > 1)))
      dis_hdr = 9;

   if (!dis_hdr) {
      /* Get window size */
      rows = get_win_height();
      lines = rows;
   }

   /* Check use of -P option */
   prep_smp_option(file_hdr.sa_proc);

   /*
    * No need to force sar_actflag to file_hdr.sa_actflag
    * since we are not reading stats from a file.
    */

   /* Perform required allocations */
   allocate_structures(USE_SADC);

   /* Print report header */
   print_report_hdr(S_O_NONE, flags, &loc_time, &file_hdr);

   /* Read system statistics sent by the data collector */
   read_stat_bunch(0);

   if (!dis_hdr) {
      if (file_hdr.sa_proc > 0)
	 more = 2 + file_hdr.sa_proc;
      if (pid_nr)
	 more = pid_nr;
   }

   if (!interval)
      /* Display stats since boot time and exit */
      write_stats_startup(0);

   /* Save the first stats collected. Will be used to compute the average */
   copy_structures(2, 0, USE_SADC);

   /* Main loop */
   do {

      /* Get stats */
      read_stat_bunch(curr);

      /* Print results */
      if (!dis_hdr) {
	 dis = lines / rows;
	 if (dis)
	    lines %= rows;
	 lines += more;
      }
      write_stats(curr, dis, sar_actflag, USE_SADC, &count, NO_TM_START,
		  tm_end.use, NO_RESET, ST_IMMEDIATE);
      fflush(stdout);	/* Don't buffer data if redirected to a pipe... */

      if (file_stats[curr].record_type == R_LAST_STATS) {
	 /* File rotation is happening: re-read header data sent by sadc */
	 read_header_data();
	 allocate_structures(USE_SADC);
      }

      if (count > 0)
	 count--;
      if (count)
	 curr ^= 1;
   }
   while (count);

   /* Print statistics average */
   write_stats_avg(curr, dis_hdr, sar_actflag, USE_SADC);
}


/*
 ***************************************************************************
 * Main entry to the sar program
 ***************************************************************************
 */
int main(int argc, char **argv)
{
   int opt = 1, args_idx = 2;
   int fd[2];
   unsigned long pid = 0;
   char from_file[MAX_FILE_LEN], to_file[MAX_FILE_LEN];
   char ltemp[20];

   /* Compute page shift in kB */
   kb_shift = get_kb_shift();

   from_file[0] = to_file[0] = '\0';

#ifdef USE_NLS
   /* Init National Language Support */
   init_nls();
#endif

   tm_start.use = tm_end.use = FALSE;
   init_bitmap(irq_bitmap, 0, NR_IRQS);
   init_bitmap(cpu_bitmap, 0, NR_CPUS);
   init_all_stats();

   /* Process options */
   while (opt < argc) {

      if (!strcmp(argv[opt], "-I")) {
	 if (argv[++opt]) {
	    dis_hdr++;
	    /* Parse -I option */
	    if (parse_sar_I_opt(argv, &opt, &sar_actflag, &dis_hdr,
				irq_bitmap))
	       usage(argv[0]);
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-P")) {
	 /* Parse -P option */
	 if (parse_sa_P_opt(argv, &opt, &flags, &dis_hdr, cpu_bitmap))
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-o")) {
	 /* Save stats to a file */
	 if ((argv[++opt]) && strncmp(argv[opt], "-", 1) &&
	     (strspn(argv[opt], DIGITS) != strlen(argv[opt]))) {
	    strncpy(to_file, argv[opt++], MAX_FILE_LEN);
	    to_file[MAX_FILE_LEN - 1] = '\0';
	 }
	 else
	    strcpy(to_file, "-");
      }

      else if (!strcmp(argv[opt], "-f")) {
	 /* Read stats from a file */
	 if ((argv[++opt]) && strncmp(argv[opt], "-", 1) &&
	     (strspn(argv[opt], DIGITS) != strlen(argv[opt]))) {
	    strncpy(from_file, argv[opt++], MAX_FILE_LEN);
	    from_file[MAX_FILE_LEN - 1] = '\0';
	 }
	 else {
	    get_localtime(&loc_time);
	    snprintf(from_file, MAX_FILE_LEN,
		     "%s/sa%02d", SA_DIR, loc_time.tm_mday);
	    from_file[MAX_FILE_LEN - 1] = '\0';
	 }
      }

      else if (!strcmp(argv[opt], "-s")) {
	 /* Get time start */
	 if (parse_timestamp(argv, &opt, &tm_start, DEF_TMSTART))
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-e")) {
	 /* Get time end */
	 if (parse_timestamp(argv, &opt, &tm_end, DEF_TMEND))
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-i")) {
	 if (!argv[++opt] || (strspn(argv[opt], DIGITS) != strlen(argv[opt])))
	    usage(argv[0]);
	 interval = atol(argv[opt++]);
	 if (interval < 1)
	   usage(argv[0]);
	 flags |= S_F_I_OPTION;
      }

      else if (!strcmp(argv[opt], "-x") || !strcmp(argv[opt], "-X")) {
	 if (argv[++opt]) {
	    dis_hdr++;
	    if (!strcmp(argv[opt], K_SADC)) {
	       strcpy(ltemp, K_SELF);
	       opt++;
	    }
	    else if (!strcmp(argv[opt], K_ALL)) {
	       if (args_idx < MAX_ARGV_NR - 7) {
		  dis_hdr = 9;
		  salloc(args_idx++, argv[opt - 1]);	/* "-x" or "-X" */
		  salloc(args_idx++, K_ALL);
		  if (!strcmp(argv[opt - 1], "-x"))
		     sar_actflag |= A_PID;
		  else
		     sar_actflag |= A_CPID;
	       }
	       opt++;
	       continue;	/* Next option */
	    }
	    else {
	       if (!strcmp(argv[opt], K_SELF)) {
		  pid = getpid();
		  opt++;
	       }
	       else if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
		  usage(argv[0]);
	       else {
		  pid = atol(argv[opt++]);
		  if (pid < 1)
		     usage(argv[0]);
	       }
	       sprintf(ltemp, "%ld", pid);
	    }

	    if (args_idx < MAX_ARGV_NR - 7) {
	       if (!strcmp(argv[opt - 2], "-x"))
		  sar_actflag |= A_PID;
	       else
		  sar_actflag |= A_CPID;
	       salloc(args_idx++, argv[opt - 2]);	/* "-x" or "-X" */
	       salloc(args_idx++, ltemp);
	    }
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strcmp(argv[opt], "-n")) {
	 if (argv[++opt]) {
	    dis_hdr++;
	    /* Parse option -n */
	    if (parse_sar_n_opt(argv, &opt, &sar_actflag, &dis_hdr))
	       usage(argv[0]);
	 }
	 else
	    usage(argv[0]);
      }

      else if (!strncmp(argv[opt], "-", 1)) {
	 /* Other options not previously tested */
	 if (parse_sar_opt(argv, opt, &sar_actflag, &flags, &dis_hdr, C_SAR,
			   irq_bitmap, cpu_bitmap))
	    usage(argv[0]);
	 opt++;
      }

      else if (interval < 0) { 				/* Get interval */
	 if (strspn(argv[opt], DIGITS) != strlen(argv[opt]))
	    usage(argv[0]);
	 interval = atol(argv[opt++]);
	 if (interval < 0)
	   usage(argv[0]);
      }

      else {					/* Get count value */
	 if ((strspn(argv[opt], DIGITS) != strlen(argv[opt])) ||
	     !interval)
	    usage(argv[0]);
	 if (count)
	    /* Count parameter already set */
	    usage(argv[0]);
	 count = atol(argv[opt++]);
	 if (count < 0)
	   usage(argv[0]);
	 else if (!count)
	    count = -1;	/* To generate a report continuously */
      }
   }

   /* 'sar' is equivalent to 'sar -f' */
   if ((argc == 1) ||
       ((interval < 0) && !from_file[0] && !to_file[0])) {
      get_localtime(&loc_time);
      snprintf(from_file, MAX_FILE_LEN,
	       "%s/sa%02d", SA_DIR, loc_time.tm_mday);
      from_file[MAX_FILE_LEN - 1] = '\0';
   }

   /*
    * Check option dependencies
    */
   /* You read from a file OR you write to it... */
   if (from_file[0] && to_file[0]) {
      fprintf(stderr, _("-f and -o options are mutually exclusive\n"));
      exit(1);
   }
   /* Use time start or option -i only when reading stats from a file */
   if ((tm_start.use || USE_I_OPTION(flags)) && !from_file[0]) {
      fprintf(stderr,
	      _("Not reading from a system activity file (use -f option)\n"));
      exit(1);
   }
   /* Don't print stats since boot time if -o or -f options are used */
   if (!interval && (from_file[0] || to_file[0]))
      usage(argv[0]);

   if (!count) {
      /* count parameter not set */
      if (from_file[0])
	 /* Display all the contents of the daily data file */
	 count = -1;
      else
	 /* Default value for the count parameter is 1 */
	 count = 1;
   }

   /* -x and -X options ignored when writing to a file */
   if (to_file[0]) {
      sar_actflag &= ~A_PID;
      sar_actflag &= ~A_CPID;
   }

   /* Default is CPU activity... */
   if (!sar_actflag) {
      /*
       * Still OK even when reading stats from a file
       * since A_CPU activity is always recorded.
       */
      sar_actflag |= A_CPU;
      dis_hdr++;
   }

   /* ---Reading stats from file */
   if (from_file[0]) {
      if (interval < 0)
	 interval = 1;

      /* Read stats from file */
      read_stats_from_file(from_file);

      return 0;
   }

   /* ---Reading stats from sadc */

   /* Create anonymous pipe */
   if (pipe(fd) == -1) {
      perror("pipe");
      exit(4);
   }

   switch (fork()) {

    case -1:
      perror("fork");
      exit(4);
      break;

    case 0: /* Child */
      dup2(fd[1], STDOUT_FILENO);
      CLOSE_ALL(fd);

      /*
       * Prepare options for sadc
       */
      /* Program name */
      salloc(0, SADC);

      /* Interval value */
      if (interval < 0)
	 usage(argv[0]);
      else if (!interval)
	 strcpy(ltemp, "1");
      else
	 sprintf(ltemp, "%ld", interval);
      salloc(1, ltemp);

      /* Count number */
      if (count >= 0) {
	 sprintf(ltemp, "%ld", count + 1);
	 salloc(args_idx++, ltemp);
      }

      /* Flags to be passed to sadc */
      salloc(args_idx++, "-z");
      if (GET_ONE_IRQ(sar_actflag))
	 salloc(args_idx++, "-I");
      if (GET_DISK(sar_actflag))
	 salloc(args_idx++, "-d");

      /* Outfile arg */
      if (to_file[0])
	 salloc(args_idx++, to_file);

      /* Last arg is NULL */
      args[args_idx] = NULL;

      /* Call now the data collector */
      execv(SADC_PATH, args);
      execvp(SADC, args);
      /*
       * Note: don't use execl/execlp since we don't have a fixed number of
       * args to give to sadc.
       */

      perror("exec");
      exit(4);
      break;

    default: /* Parent */
      dup2(fd[0], STDIN_FILENO);
      CLOSE_ALL(fd);

      /* Get now the statistics */
      read_stats();
      break;
   }
   return 0;
}
