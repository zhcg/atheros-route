/*
 * sar/sadc: report system activity
 * (C) 1999-2005 by Sebastien Godard (sysstat <at> wanadoo.fr)
 */

#ifndef _SA_H
#define _SA_H

/* Get IFNAMSIZ */
#include <net/if.h>
#ifndef IFNAMSIZ
#define IFNAMSIZ	16
#endif

#include "common.h"

/*
 * System activity daily file magic number
 * (will vary when file format changes)
 */
#define SA_MAGIC	0x2167


/* Define activities */
#define A_PROC		0x000001
#define A_CTXSW		0x000002
#define A_CPU		0x000004
#define A_IRQ		0x000008
#define A_ONE_IRQ	0x000010
#define A_SWAP		0x000020
#define A_IO		0x000040
#define A_MEMORY	0x000080
#define A_SERIAL	0x000100
#define A_NET_DEV	0x000200
#define A_NET_EDEV	0x000400
#define A_DISK		0x000800
#define A_PID		0x001000
#define A_CPID		0x002000
#define A_NET_NFS	0x004000
#define A_NET_NFSD	0x008000
#define A_PAGE		0x010000
#define A_MEM_AMT	0x020000
#define A_KTABLES	0x040000
#define A_NET_SOCK	0x080000
#define A_QUEUE		0x100000

#define A_LAST		0x100000

#define GET_PROC(m)	(((m) & A_PROC) == A_PROC)
#define GET_CTXSW(m)	(((m) & A_CTXSW) == A_CTXSW)
#define GET_CPU(m)	(((m) & A_CPU) == A_CPU)
#define GET_IRQ(m)	(((m) & A_IRQ) == A_IRQ)
#define GET_PAGE(m)	(((m) & A_PAGE) == A_PAGE)
#define GET_SWAP(m)	(((m) & A_SWAP) == A_SWAP)
#define GET_IO(m)	(((m) & A_IO) == A_IO)
#define GET_ONE_IRQ(m)	(((m) & A_ONE_IRQ) == A_ONE_IRQ)
#define GET_MEMORY(m)	(((m) & A_MEMORY) == A_MEMORY)
#define GET_PID(m)	(((m) & A_PID) == A_PID)
#define GET_CPID(m)	(((m) & A_CPID) == A_CPID)
#define GET_CPID(m)	(((m) & A_CPID) == A_CPID)
#define GET_ALL_PID(m)	(((m) & A_ALL_PID) == A_ALL_PID)
#define GET_SERIAL(m)	(((m) & A_SERIAL) == A_SERIAL)
#define GET_MEM_AMT(m)	(((m) & A_MEM_AMT) == A_MEM_AMT)
#define GET_KTABLES(m)	(((m) & A_KTABLES) == A_KTABLES)
#define GET_NET_DEV(m)	(((m) & A_NET_DEV) == A_NET_DEV)
#define GET_NET_EDEV(m)	(((m) & A_NET_EDEV) == A_NET_EDEV)
#define GET_NET_SOCK(m)	(((m) & A_NET_SOCK) == A_NET_SOCK)
#define GET_NET_NFS(m)	(((m) & A_NET_NFS) == A_NET_NFS)
#define GET_NET_NFSD(m)	(((m) & A_NET_NFSD) == A_NET_NFSD)
#define GET_QUEUE(m)	(((m) & A_QUEUE) == A_QUEUE)
#define GET_DISK(m)	(((m) & A_DISK) == A_DISK)


/*
 * kB -> number of pages.
 * Page size depends on machine architecture (4 kB, 8 kB, 16 kB, 64 kB...)
 */
#define PG(k)	((k) >> kb_shift)

/* Keywords */
#define K_XALL	"XALL"
#define K_SUM	"SUM"
#define K_SELF	"SELF"
#define K_SADC	"SADC"
#define K_DEV	"DEV"
#define K_EDEV	"EDEV"
#define K_NFS	"NFS"
#define K_NFSD	"NFSD"
#define K_SOCK	"SOCK"
#define K_FULL	"FULL"

/* S_= sar/sadc/sadf - F_= Flag */
#define S_F_ALL_PROC      	0x0001
#define S_F_SA_ROTAT      	0x0002
#define S_F_DEV_PRETTY		0x0004
#define S_F_A_OPTION		0x0008
#define S_F_F_OPTION		0x0010
#define S_F_I_OPTION		0x0020
#define S_F_TRUE_TIME		0x0040
#define S_F_L_OPTION		0x0080
#define S_F_HAS_PPARTITIONS	0x0100
#define S_F_HAS_DISKSTATS	0x0200
#define S_F_FILE_LCK		0X0400
#define S_F_PER_PROC		0x0800

#define WANT_ALL_PROC(m)	(((m) & S_F_ALL_PROC) == S_F_ALL_PROC)
#define WANT_SA_ROTAT(m)	(((m) & S_F_SA_ROTAT) == S_F_SA_ROTAT)
#define USE_PRETTY_OPTION(m)	(((m) & S_F_DEV_PRETTY) == S_F_DEV_PRETTY)
#define USE_A_OPTION(m)		(((m) & S_F_A_OPTION) == S_F_A_OPTION)
#define USE_F_OPTION(m)		(((m) & S_F_F_OPTION) == S_F_F_OPTION)
#define USE_I_OPTION(m)		(((m) & S_F_I_OPTION) == S_F_I_OPTION)
#define PRINT_TRUE_TIME(m)	(((m) & S_F_TRUE_TIME) == S_F_TRUE_TIME)
#define USE_L_OPTION(m)		(((m) & S_F_L_OPTION) == S_F_L_OPTION)
#define HAS_PPARTITIONS(m)	(((m) & S_F_HAS_PPARTITIONS) == S_F_HAS_PPARTITIONS)
#define HAS_DISKSTATS(m)	(((m) & S_F_HAS_DISKSTATS) == S_F_HAS_DISKSTATS)
#define FILE_LOCKED(m)		(((m) & S_F_FILE_LCK) == S_F_FILE_LCK)
#define WANT_PER_PROC(m)	(((m) & S_F_PER_PROC) == S_F_PER_PROC)

/* Output formats (O_= Output)  */
#define S_O_NONE		0
#define S_O_HDR_OPTION		1
#define S_O_PPC_OPTION		2
#define S_O_DB_OPTION		3
#define S_O_XML_OPTION		4

/* Files */
#define PROC		"/proc"
#define PSTAT		"stat"
#define MEMINFO		"/proc/meminfo"
#define PID_STAT	"/proc/%ld/stat"
#define SERIAL		"/proc/tty/driver/serial"
#define FDENTRY_STATE	"/proc/sys/fs/dentry-state"
#define FFILE_NR	"/proc/sys/fs/file-nr"
#define FINODE_STATE	"/proc/sys/fs/inode-state"
#define FDQUOT_NR	"/proc/sys/fs/dquot-nr"
#define FDQUOT_MAX	"/proc/sys/fs/dquot-max"
#define FSUPER_NR	"/proc/sys/fs/super-nr"
#define FSUPER_MAX	"/proc/sys/fs/super-max"
#define FRTSIG_NR	"/proc/sys/kernel/rtsig-nr"
#define FRTSIG_MAX	"/proc/sys/kernel/rtsig-max"
#define NET_DEV		"/proc/net/dev"
#define NET_SOCKSTAT	"/proc/net/sockstat"
#define NET_RPC_NFS	"/proc/net/rpc/nfs"
#define NET_RPC_NFSD	"/proc/net/rpc/nfsd"
#define SADC		"sadc"
#define LOADAVG		"/proc/loadavg"
#define VMSTAT		"/proc/vmstat"

#define NR_IFACE_PREALLOC	2
#define NR_SERIAL_PREALLOC	2
#define NR_IRQPROC_PREALLOC	3

#define NR_IRQS			256

/* Maximum number of processes that can be monitored simultaneously */
#define MAX_PID_NR	256
/* Maximum length of network interface name */
#define MAX_IFACE_LEN	IFNAMSIZ
/*
 * Maximum number of args that can be passed to sadc:
 * sadc -x <pid> [-x <pid> ...] -X <pid> [-X <pid> ...]
 *	-I <interval> <count> <outfile> NULL
 */
#define MAX_ARGV_NR	(64 * 2) + 6

#define USE_SADC	0
#define USE_SA_FILE	1
#define ST_IMMEDIATE	0
#define ST_SINCE_BOOT	1
#define NO_TM_START	0
#define NO_TM_END	0
#define NO_RESET	0
#define NON_FATAL	0
#define FATAL		1
#define C_SAR		0
#define C_SADF		1

#define X_PID_SET	0x01
#define X_PPID_SET	0x02

/* Record type */
#define R_STATS		1
#define R_DUMMY		2
#define R_LAST_STATS	3

#define SOFT_SIZE	0
#define HARD_SIZE	1


/*
 * IMPORTANT NOTE:
 * Attributes such as 'aligned' and 'packed' have been defined for every
 * members of the following structures, so that:
 * 1) structures have a fixed size whether on 32 or 64-bit systems,
 * 2) we don't have variable gap between members.
 * Indeed, we want to be able to read daily data files recorded on
 * 32 and 64-bit systems, even if we are not on the same architecture.
 * Moreover, we are sure that sizeof(struct) is a constant for every
 * struct of same type, so that expressions like
 * struct *foo2 = struct *foo1 + i;
 * can be safely used.
 *
 * Structures are padded so that their length be a multiple of 8 bytes.
 * It is better (although not compulsory) for structures written
 * contiguously into daily data files and accessed the following way once
 * they are read into memory:
 * struct *foo2 = struct *foo1 + i;  (since i <=> sizeof(struct foo))
 */

/* System activity data file header */
struct file_hdr {
   /* --- LONG --- */
   /* Time stamp in seconds since the epoch */
   unsigned long  sa_ust_time			__attribute__ ((aligned (8)));
   /* --- INT --- */
   /* Activity flag */
   unsigned int	  sa_actflag			__attribute__ ((aligned (8)));
   /* Number of processes to monitor ( {-x | -X } ALL) */
   unsigned int   sa_nr_pid			__attribute__ ((packed));
   /* Number of interrupts per processor: 2 means two interrupts */
   unsigned int   sa_irqcpu			__attribute__ ((packed));
   /* Number of disks */
   unsigned int   sa_nr_disk			__attribute__ ((packed));
   /* Number of processors: 1 means two proc */
   unsigned int   sa_proc 			__attribute__ ((packed));
   /* Number of serial lines: 2 means two lines (ttyS00 and ttyS01) */
   unsigned int   sa_serial 			__attribute__ ((packed));
   /* Number of network devices (interfaces): 2 means two lines */
   unsigned int   sa_iface 			__attribute__ ((packed));
   /* --- SHORT --- */
   /* System activity data file magic number */
   unsigned short sa_magic			__attribute__ ((packed));
   /* file_stats structure size */
   unsigned short sa_st_size			__attribute__ ((packed));
   /* --- CHAR --- */
   /*
    * Current day, month and year.
    * No need to save DST (daylight saving time) flag, since it is not taken
    * into account by the strftime() function used to print the timestamp.
    */
   unsigned char  sa_day			__attribute__ ((packed));
   unsigned char  sa_month			__attribute__ ((packed));
   unsigned char  sa_year			__attribute__ ((packed));
   /* Operating system name */
   char           sa_sysname[UTSNAME_LEN]	__attribute__ ((packed));
   /* Machine hostname */
   char           sa_nodename[UTSNAME_LEN]	__attribute__ ((packed));
   /* Operating system release number */
   char           sa_release[UTSNAME_LEN]	__attribute__ ((packed));
};

#define FILE_HDR_SIZE	(sizeof(struct file_hdr))

struct file_stats {
   /* --- LONG LONG --- */
   /* Machine uptime (multiplied by the # of proc) */
   unsigned long long uptime			__attribute__ ((aligned (16)));
   /* Uptime reduced to one processor. Set *only* on SMP machines */
   unsigned long long uptime0			__attribute__ ((aligned (16)));
   unsigned long long context_swtch		__attribute__ ((aligned (16)));
   unsigned long long cpu_user			__attribute__ ((aligned (16)));
   unsigned long long cpu_nice			__attribute__ ((aligned (16)));
   unsigned long long cpu_system		__attribute__ ((aligned (16)));
   unsigned long long cpu_idle			__attribute__ ((aligned (16)));
   unsigned long long cpu_iowait		__attribute__ ((aligned (16)));
   unsigned long long irq_sum			__attribute__ ((aligned (16)));
   /* --- LONG --- */
   /* Time stamp (number of seconds since the epoch) */
   unsigned long ust_time			__attribute__ ((aligned (16)));
   unsigned long processes			__attribute__ ((aligned (8)));
   unsigned long pgpgin				__attribute__ ((aligned (8)));
   unsigned long pgpgout			__attribute__ ((aligned (8)));
   unsigned long pswpin				__attribute__ ((aligned (8)));
   unsigned long pswpout			__attribute__ ((aligned (8)));
   /* Memory stats in kB */
   unsigned long frmkb				__attribute__ ((aligned (8)));
   unsigned long bufkb				__attribute__ ((aligned (8)));
   unsigned long camkb				__attribute__ ((aligned (8)));
   unsigned long tlmkb				__attribute__ ((aligned (8)));
   unsigned long frskb				__attribute__ ((aligned (8)));
   unsigned long tlskb				__attribute__ ((aligned (8)));
   unsigned long caskb				__attribute__ ((aligned (8)));
   unsigned long nr_running			__attribute__ ((aligned (8)));
   unsigned long pgfault			__attribute__ ((aligned (8)));
   unsigned long pgmajfault			__attribute__ ((aligned (8)));
   /* --- INT --- */
   unsigned int  dk_drive			__attribute__ ((aligned (8)));
   unsigned int  dk_drive_rio			__attribute__ ((packed));
   unsigned int  dk_drive_wio			__attribute__ ((packed));
   unsigned int  dk_drive_rblk			__attribute__ ((packed));
   unsigned int  dk_drive_wblk			__attribute__ ((packed));
   unsigned int  file_used			__attribute__ ((packed));
   unsigned int  inode_used			__attribute__ ((packed));
   unsigned int  super_used			__attribute__ ((packed));
   unsigned int  super_max			__attribute__ ((packed));
   unsigned int  dquot_used			__attribute__ ((packed));
   unsigned int  dquot_max			__attribute__ ((packed));
   unsigned int  rtsig_queued			__attribute__ ((packed));
   unsigned int  rtsig_max			__attribute__ ((packed));
   unsigned int  sock_inuse			__attribute__ ((packed));
   unsigned int  tcp_inuse			__attribute__ ((packed));
   unsigned int  udp_inuse			__attribute__ ((packed));
   unsigned int  raw_inuse			__attribute__ ((packed));
   unsigned int  frag_inuse			__attribute__ ((packed));
   unsigned int  dentry_stat			__attribute__ ((packed));
   unsigned int  load_avg_1			__attribute__ ((packed));
   unsigned int  load_avg_5			__attribute__ ((packed));
   unsigned int  load_avg_15			__attribute__ ((packed));
   unsigned int  nr_threads			__attribute__ ((packed));
   unsigned int  nfs_rpccnt			__attribute__ ((packed));
   unsigned int  nfs_rpcretrans			__attribute__ ((packed));
   unsigned int  nfs_readcnt			__attribute__ ((packed));
   unsigned int  nfs_writecnt			__attribute__ ((packed));
   unsigned int  nfs_accesscnt			__attribute__ ((packed));
   unsigned int  nfs_getattcnt			__attribute__ ((packed));
   unsigned int  nfsd_rpccnt			__attribute__ ((packed));
   unsigned int  nfsd_rpcbad			__attribute__ ((packed));
   unsigned int  nfsd_netcnt			__attribute__ ((packed));
   unsigned int  nfsd_netudpcnt			__attribute__ ((packed));
   unsigned int  nfsd_nettcpcnt			__attribute__ ((packed));
   unsigned int  nfsd_rchits			__attribute__ ((packed));
   unsigned int  nfsd_rcmisses			__attribute__ ((packed));
   unsigned int  nfsd_readcnt			__attribute__ ((packed));
   unsigned int  nfsd_writecnt			__attribute__ ((packed));
   unsigned int  nfsd_accesscnt			__attribute__ ((packed));
   unsigned int  nfsd_getattcnt			__attribute__ ((packed));
   /* --- CHAR --- */
   /* Record type: R_STATS or R_DUMMY */
   unsigned char record_type			__attribute__ ((packed));
   /*
    * Time stamp: hour, minute and second.
    * Used to determine TRUE time (immutable, non locale dependent time).
    */
   unsigned char hour		/* (0-23) */	__attribute__ ((packed));
   unsigned char minute		/* (0-59) */	__attribute__ ((packed));
   unsigned char second		/* (0-59) */	__attribute__ ((packed));
};

#define FILE_STATS_SIZE	(sizeof(struct file_stats))

struct stats_one_cpu {
   unsigned long long per_cpu_idle		__attribute__ ((aligned (16)));
   unsigned long long per_cpu_iowait		__attribute__ ((aligned (16)));
   unsigned long long per_cpu_user		__attribute__ ((aligned (16)));
   unsigned long long per_cpu_nice		__attribute__ ((aligned (16)));
   unsigned long long per_cpu_system		__attribute__ ((aligned (16)));
   unsigned long long pad			__attribute__ ((aligned (16)));
};

#define STATS_ONE_CPU_SIZE	(sizeof(struct stats_one_cpu))

/*
 * Members do not need to be aligned since these stats are not written
 * to daily data files.
 */
struct pid_stats {
   /* If pid is null, the process has been killed */
   unsigned long pid				__attribute__ ((aligned (8)));
   unsigned long minflt				__attribute__ ((packed));
   unsigned long majflt				__attribute__ ((packed));
   unsigned long utime				__attribute__ ((packed));
   unsigned long stime				__attribute__ ((packed));
   unsigned long nswap				__attribute__ ((packed));
   unsigned long cminflt			__attribute__ ((packed));
   unsigned long cmajflt			__attribute__ ((packed));
   unsigned long cutime				__attribute__ ((packed));
   unsigned long cstime				__attribute__ ((packed));
   unsigned long cnswap				__attribute__ ((packed));
   unsigned int  processor			__attribute__ ((packed));
   unsigned char flag				__attribute__ ((packed));
   unsigned char pad[3]				__attribute__ ((packed));
};

#define PID_STATS_SIZE	(sizeof(struct pid_stats))

struct stats_serial {
   unsigned int  rx				__attribute__ ((aligned (8)));
   unsigned int  tx				__attribute__ ((packed));
   unsigned int  frame				__attribute__ ((packed));
   unsigned int  parity				__attribute__ ((packed));
   unsigned int  brk				__attribute__ ((packed));
   unsigned int  overrun			__attribute__ ((packed));
   unsigned int  line				__attribute__ ((packed));
   unsigned char pad[4]				__attribute__ ((packed));
};

#define STATS_SERIAL_SIZE	(sizeof(struct stats_serial))

/* See linux source file linux/include/linux/netdevice.h */
struct stats_net_dev {
   unsigned long rx_packets			__attribute__ ((aligned (8)));
   unsigned long tx_packets			__attribute__ ((aligned (8)));
   unsigned long rx_bytes			__attribute__ ((aligned (8)));
   unsigned long tx_bytes			__attribute__ ((aligned (8)));
   unsigned long rx_compressed			__attribute__ ((aligned (8)));
   unsigned long tx_compressed			__attribute__ ((aligned (8)));
   unsigned long multicast			__attribute__ ((aligned (8)));
   unsigned long collisions			__attribute__ ((aligned (8)));
   unsigned long rx_errors			__attribute__ ((aligned (8)));
   unsigned long tx_errors			__attribute__ ((aligned (8)));
   unsigned long rx_dropped			__attribute__ ((aligned (8)));
   unsigned long tx_dropped			__attribute__ ((aligned (8)));
   unsigned long rx_fifo_errors			__attribute__ ((aligned (8)));
   unsigned long tx_fifo_errors			__attribute__ ((aligned (8)));
   unsigned long rx_frame_errors		__attribute__ ((aligned (8)));
   unsigned long tx_carrier_errors		__attribute__ ((aligned (8)));
   char		 interface[MAX_IFACE_LEN]	__attribute__ ((aligned (8)));
};

#define STATS_NET_DEV_SIZE	(sizeof(struct stats_net_dev))


/*
 * stats_irq_cpu->irq:       IRQ#-A
 * stats_irq_cpu->interrupt: number of IRQ#-A for proc 0
 * stats_irq_cpu->irq:       IRQ#-B
 * stats_irq_cpu->interrupt: number of IRQ#-B for proc 0
 * ...
 * stats_irq_cpu->irq:       (undef'd)
 * stats_irq_cpu->interrupt: number of IRQ#-A for proc 1
 * stats_irq_cpu->irq:       (undef'd)
 * stats_irq_cpu->interrupt: number of IRQ#-B for proc 1
 * ...
 */
struct stats_irq_cpu {
   unsigned int interrupt			__attribute__ ((aligned (8)));
   unsigned int irq				__attribute__ ((packed));
};

#define STATS_IRQ_CPU_SIZE	(sizeof(struct stats_irq_cpu))
#define STATS_ONE_IRQ_SIZE	(sizeof(int) * NR_IRQS)

struct disk_stats {
   unsigned long long rd_sect			__attribute__ ((aligned (16)));
   unsigned long long wr_sect			__attribute__ ((aligned (16)));
   unsigned long rd_ticks			__attribute__ ((aligned (16)));
   unsigned long wr_ticks			__attribute__ ((aligned (8)));
   unsigned long tot_ticks			__attribute__ ((aligned (8)));
   unsigned long rq_ticks			__attribute__ ((aligned (8)));
   unsigned long nr_ios				__attribute__ ((aligned (8)));
   unsigned int  major				__attribute__ ((aligned (8)));
   unsigned int  minor				__attribute__ ((packed));
};

#define DISK_STATS_SIZE		(sizeof(struct disk_stats))

struct stats_sum {
   unsigned long count				__attribute__ ((aligned (8)));
   unsigned long frmkb				__attribute__ ((packed));
   unsigned long bufkb				__attribute__ ((packed));
   unsigned long camkb				__attribute__ ((packed));
   unsigned long frskb				__attribute__ ((packed));
   unsigned long tlskb				__attribute__ ((packed));
   unsigned long caskb				__attribute__ ((packed));
   unsigned long dentry_stat			__attribute__ ((packed));
   unsigned long file_used			__attribute__ ((packed));
   unsigned long inode_used			__attribute__ ((packed));
   unsigned long super_used			__attribute__ ((packed));
   unsigned long dquot_used			__attribute__ ((packed));
   unsigned long rtsig_queued			__attribute__ ((packed));
   unsigned long sock_inuse			__attribute__ ((packed));
   unsigned long tcp_inuse			__attribute__ ((packed));
   unsigned long udp_inuse			__attribute__ ((packed));
   unsigned long raw_inuse			__attribute__ ((packed));
   unsigned long frag_inuse			__attribute__ ((packed));
   unsigned long nr_running			__attribute__ ((packed));
   unsigned long nr_threads			__attribute__ ((packed));
   unsigned long load_avg_1			__attribute__ ((packed));
   unsigned long load_avg_5			__attribute__ ((packed));
   unsigned long load_avg_15			__attribute__ ((packed));
};

#define STATS_SUM_SIZE	(sizeof(struct stats_sum))

struct tstamp {
   int tm_sec;
   int tm_min;
   int tm_hour;
   int use;
};

/* Time must have the format HH:MM:SS with HH in 24-hour format */
#define DEF_TMSTART	"08:00:00"
#define DEF_TMEND	"18:00:00"


/* Using 'do ... while' makes this macro safer to use (trailing semicolon) */
#define CLOSE_ALL(_fd_)		do {		\
				close(_fd_[0]); \
				close(_fd_[1]); \
				} while (0)

#define CLOSE(_fd_)		if (_fd_ >= 0)	\
				close(_fd_)

#define SREALLOC(S, TYPE, SIZE)	do {							\
   				   TYPE *p;						\
				   p = S;						\
   				   if ((S = (TYPE *) realloc(S, (SIZE))) == NULL) {	\
				      perror("realloc");				\
				      exit(4);						\
				   }							\
   				   if (!p)						\
      				      memset(S, 0, (SIZE));				\
				} while (0)

/* Functions */
extern int	    check_disk_reg(struct file_hdr *, struct disk_stats * [],
				   short, short, int);
extern unsigned int check_iface_reg(struct file_hdr *,
				    struct stats_net_dev * [],
				    short, short, unsigned int);
extern int	    datecmp(struct tm *, struct tstamp *);
unsigned long long  get_per_cpu_interval(struct stats_one_cpu *,
					 struct stats_one_cpu *);
extern char	   *get_devname(unsigned int, unsigned int, int);
extern void	    init_bitmap(unsigned char [], unsigned char, unsigned int);
extern void	    init_stats(struct file_stats [], unsigned int [][NR_IRQS]);
extern int	    next_slice(unsigned long long, unsigned long long,
			       struct file_hdr *, int, long);
extern int	    parse_sar_opt(char * [], int, unsigned int *,
				  unsigned int *, short *, int,
				  unsigned char [], unsigned char []);
extern int	    parse_sar_I_opt(char * [], int *, unsigned int *, short *,
				    unsigned char []);
extern int	    parse_sa_P_opt(char * [], int *, unsigned int *, short *,
				   unsigned char []);
extern int	    parse_sar_n_opt(char * [], int *, unsigned int *, short *);
extern int	    parse_timestamp(char * [], int *, struct tstamp *,
				    const char *);
extern void	    prep_file_for_reading(int *, char *, struct file_hdr *,
					  unsigned int *, unsigned int);
extern void	    get_itv_value(struct file_stats *, struct file_stats *,
				  unsigned int, unsigned long long *,
				  unsigned long long *);
extern void	    print_report_hdr(unsigned short, unsigned int,
				     struct tm *, struct file_hdr *);
extern int	    sa_fread(int, void *, int, int);
extern void	    salloc_cpu_array(struct stats_one_cpu * [], unsigned int);
extern void	    salloc_disk_array(struct disk_stats * [], int);
extern void	    salloc_irqcpu_array(struct stats_irq_cpu * [],
					unsigned int, unsigned int);
extern void	    salloc_net_dev_array(struct stats_net_dev * [],
					 unsigned int);
extern void	    salloc_serial_array(struct stats_serial * [], int);

#endif  /* _SA_H */
