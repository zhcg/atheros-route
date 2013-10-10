/*
 * Export MIPS-specific functions needed for loadable modules.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 97, 98, 99, 2000, 01, 03, 04, 05 by Ralf Baechle
 * Copyright (C) 1999, 2000, 01 Silicon Graphics, Inc.
 */
#include <linux/config.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <asm/checksum.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>

extern void *__bzero(void *__s, size_t __count);
extern long __strncpy_from_user_nocheck_asm(char *__to,
                                            const char *__from, long __len);
extern long __strncpy_from_user_asm(char *__to, const char *__from,
                                    long __len);
extern long __strlen_user_nocheck_asm(const char *s);
extern long __strlen_user_asm(const char *s);
extern long __strnlen_user_nocheck_asm(const char *s);
extern long __strnlen_user_asm(const char *s);

/*
 * String functions
 */
EXPORT_SYMBOL(memchr);
EXPORT_SYMBOL(memcmp);
EXPORT_SYMBOL(memset);
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(memmove);
EXPORT_SYMBOL(strcat);
EXPORT_SYMBOL(strchr);
#ifdef CONFIG_64BIT
EXPORT_SYMBOL(strncmp);
#endif
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(strpbrk);
EXPORT_SYMBOL(strncat);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(strrchr);
EXPORT_SYMBOL(strstr);

EXPORT_SYMBOL(kernel_thread);

/*
 * Userspace access stuff.
 */
EXPORT_SYMBOL(__copy_user);
EXPORT_SYMBOL(__bzero);
EXPORT_SYMBOL(__strncpy_from_user_nocheck_asm);
EXPORT_SYMBOL(__strncpy_from_user_asm);
EXPORT_SYMBOL(__strlen_user_nocheck_asm);
EXPORT_SYMBOL(__strlen_user_asm);
EXPORT_SYMBOL(__strnlen_user_nocheck_asm);
EXPORT_SYMBOL(__strnlen_user_asm);

EXPORT_SYMBOL(csum_partial);

EXPORT_SYMBOL(invalid_pte_table);
#ifdef CONFIG_GENERIC_IRQ_PROBE
EXPORT_SYMBOL(probe_irq_mask);
#endif


/* ADDED BY ATHEROS (Ted Merrill, Feb. 2010)
*  The purpose of this is to support a VoIP application in kernel space.
*  The application is written using a lot of system calls.
*  Putting such application in kernel space is necessary to meet
*  realtime requirements.
*  Not all of the system calls below are actually needed;
*  i just put all system calls i could find into here to be on safe side.
*/

extern void sys_io_setup(void); EXPORT_SYMBOL(sys_io_setup);
extern void sys_io_destroy(void); EXPORT_SYMBOL(sys_io_destroy);
extern void sys_io_submit(void); EXPORT_SYMBOL(sys_io_submit);
extern void sys_io_cancel(void); EXPORT_SYMBOL(sys_io_cancel);
extern void sys_io_getevents(void); EXPORT_SYMBOL(sys_io_getevents);
extern void sys_sync(void); EXPORT_SYMBOL(sys_sync);
extern void sys_fsync(void); EXPORT_SYMBOL(sys_fsync);
extern void sys_fdatasync(void); EXPORT_SYMBOL(sys_fdatasync);
extern void sys_bdflush(void); EXPORT_SYMBOL(sys_bdflush);
extern void sys_getcwd(void); EXPORT_SYMBOL(sys_getcwd);
extern void sys_lookup_dcookie(void); EXPORT_SYMBOL(sys_lookup_dcookie);
extern void sys_epoll_create(void); EXPORT_SYMBOL(sys_epoll_create);
extern void sys_epoll_ctl(void); EXPORT_SYMBOL(sys_epoll_ctl);
extern void sys_epoll_wait(void); EXPORT_SYMBOL(sys_epoll_wait);
extern void sys_uselib(void); EXPORT_SYMBOL(sys_uselib);
extern void sys_dup2(void); EXPORT_SYMBOL(sys_dup2);
extern void sys_dup(void); EXPORT_SYMBOL(sys_dup);
extern void sys_fcntl(void); EXPORT_SYMBOL(sys_fcntl);
extern void sys_fcntl64(void); EXPORT_SYMBOL(sys_fcntl64);
extern void sys_sysfs(void); EXPORT_SYMBOL(sys_sysfs);
extern void sys_inotify_init(void); EXPORT_SYMBOL(sys_inotify_init);
extern void sys_inotify_add_watch(void); EXPORT_SYMBOL(sys_inotify_add_watch);
extern void sys_inotify_rm_watch(void); EXPORT_SYMBOL(sys_inotify_rm_watch);
extern void sys_ioctl(void); EXPORT_SYMBOL(sys_ioctl);
extern void sys_ioprio_set(void); EXPORT_SYMBOL(sys_ioprio_set);
extern void sys_ioprio_get(void); EXPORT_SYMBOL(sys_ioprio_get);
extern void sys_flock(void); EXPORT_SYMBOL(sys_flock);
extern void sys_mknod(void); EXPORT_SYMBOL(sys_mknod);
extern void sys_mkdir(void); EXPORT_SYMBOL(sys_mkdir);
extern void sys_rmdir(void); EXPORT_SYMBOL(sys_rmdir);
extern void sys_unlink(void); EXPORT_SYMBOL(sys_unlink);
extern void sys_symlink(void); EXPORT_SYMBOL(sys_symlink);
extern void sys_link(void); EXPORT_SYMBOL(sys_link);
extern void sys_rename(void); EXPORT_SYMBOL(sys_rename);
extern void sys_umount(void); EXPORT_SYMBOL(sys_umount);
extern void sys_oldumount(void); EXPORT_SYMBOL(sys_oldumount);
extern void sys_mount(void); EXPORT_SYMBOL(sys_mount);
extern void sys_pivot_root(void); EXPORT_SYMBOL(sys_pivot_root);
extern void sys_nfsservctl(void); EXPORT_SYMBOL(sys_nfsservctl);
extern void sys_statfs(void); EXPORT_SYMBOL(sys_statfs);
extern void sys_statfs64(void); EXPORT_SYMBOL(sys_statfs64);
extern void sys_fstatfs(void); EXPORT_SYMBOL(sys_fstatfs);
extern void sys_fstatfs64(void); EXPORT_SYMBOL(sys_fstatfs64);
extern void sys_truncate(void); EXPORT_SYMBOL(sys_truncate);
extern void sys_ftruncate(void); EXPORT_SYMBOL(sys_ftruncate);
extern void sys_truncate64(void); EXPORT_SYMBOL(sys_truncate64);
extern void sys_ftruncate64(void); EXPORT_SYMBOL(sys_ftruncate64);
extern void sys_utime(void); EXPORT_SYMBOL(sys_utime);
extern void sys_utimes(void); EXPORT_SYMBOL(sys_utimes);
extern void sys_access(void); EXPORT_SYMBOL(sys_access);
extern void sys_chdir(void); EXPORT_SYMBOL(sys_chdir);
extern void sys_fchdir(void); EXPORT_SYMBOL(sys_fchdir);
extern void sys_chroot(void); EXPORT_SYMBOL(sys_chroot);
extern void sys_fchmod(void); EXPORT_SYMBOL(sys_fchmod);
extern void sys_chmod(void); EXPORT_SYMBOL(sys_chmod);
extern void sys_chown(void); EXPORT_SYMBOL(sys_chown);
extern void sys_lchown(void); EXPORT_SYMBOL(sys_lchown);
extern void sys_fchown(void); EXPORT_SYMBOL(sys_fchown);
/* the following 3 lines result in duplicate symbol errors if uncommented */
//extern void sys_open(void); EXPORT_SYMBOL(sys_open);
//extern void sys_close(void); EXPORT_SYMBOL(sys_close);
//extern void sys_read(void); EXPORT_SYMBOL(sys_read);
extern void sys_creat(void); EXPORT_SYMBOL(sys_creat);
extern void sys_vhangup(void); EXPORT_SYMBOL(sys_vhangup);
extern void sys_quotactl(void); EXPORT_SYMBOL(sys_quotactl);
extern void sys_lseek(void); EXPORT_SYMBOL(sys_lseek);
extern void sys_llseek(void); EXPORT_SYMBOL(sys_llseek);
extern void sys_write(void); EXPORT_SYMBOL(sys_write);
extern void sys_pread64(void); EXPORT_SYMBOL(sys_pread64);
extern void sys_pwrite64(void); EXPORT_SYMBOL(sys_pwrite64);
extern void sys_sendfile(void); EXPORT_SYMBOL(sys_sendfile);
extern void sys_sendfile64(void); EXPORT_SYMBOL(sys_sendfile64);
extern void sys_getdents(void); EXPORT_SYMBOL(sys_getdents);
extern void sys_getdents64(void); EXPORT_SYMBOL(sys_getdents64);
extern void sys_poll(void); EXPORT_SYMBOL(sys_poll);
extern void sys_newstat(void); EXPORT_SYMBOL(sys_newstat);
extern void sys_newlstat(void); EXPORT_SYMBOL(sys_newlstat);
extern void sys_newfstat(void); EXPORT_SYMBOL(sys_newfstat);
extern void sys_readlink(void); EXPORT_SYMBOL(sys_readlink);
extern void sys_stat64(void); EXPORT_SYMBOL(sys_stat64);
extern void sys_lstat64(void); EXPORT_SYMBOL(sys_lstat64);
extern void sys_fstat64(void); EXPORT_SYMBOL(sys_fstat64);
extern void sys_ustat(void); EXPORT_SYMBOL(sys_ustat);
extern void sys_mq_open(void); EXPORT_SYMBOL(sys_mq_open);
extern void sys_mq_unlink(void); EXPORT_SYMBOL(sys_mq_unlink);
extern void sys_mq_timedsend(void); EXPORT_SYMBOL(sys_mq_timedsend);
extern void sys_mq_timedreceive(void); EXPORT_SYMBOL(sys_mq_timedreceive);
extern void sys_mq_notify(void); EXPORT_SYMBOL(sys_mq_notify);
extern void sys_mq_getsetattr(void); EXPORT_SYMBOL(sys_mq_getsetattr);
extern void sys_msgget (void); EXPORT_SYMBOL(sys_msgget );
extern void sys_msgctl (void); EXPORT_SYMBOL(sys_msgctl );
extern void sys_msgsnd (void); EXPORT_SYMBOL(sys_msgsnd );
extern void sys_msgrcv (void); EXPORT_SYMBOL(sys_msgrcv );
extern void sys_semget (void); EXPORT_SYMBOL(sys_semget );
extern void sys_semctl (void); EXPORT_SYMBOL(sys_semctl );
extern void sys_semtimedop(void); EXPORT_SYMBOL(sys_semtimedop);
extern void sys_semop (void); EXPORT_SYMBOL(sys_semop );
extern void sys_shmget (void); EXPORT_SYMBOL(sys_shmget );
extern void sys_shmctl (void); EXPORT_SYMBOL(sys_shmctl );
extern void sys_shmat(void); EXPORT_SYMBOL(sys_shmat);
extern void sys_shmdt(void); EXPORT_SYMBOL(sys_shmdt);
extern void sys_acct(void); EXPORT_SYMBOL(sys_acct);
extern void sys_capget(void); EXPORT_SYMBOL(sys_capget);
extern void sys_capset(void); EXPORT_SYMBOL(sys_capset);
extern void sys_exit(void); EXPORT_SYMBOL(sys_exit);
extern void sys_exit_group(void); EXPORT_SYMBOL(sys_exit_group);
extern void sys_waitid(void); EXPORT_SYMBOL(sys_waitid);
extern void sys_wait4(void); EXPORT_SYMBOL(sys_wait4);
extern void sys_waitpid(void); EXPORT_SYMBOL(sys_waitpid);
extern void sys_set_tid_address(void); EXPORT_SYMBOL(sys_set_tid_address);
extern void sys_futex(void); EXPORT_SYMBOL(sys_futex);
extern void sys_getitimer(void); EXPORT_SYMBOL(sys_getitimer);
extern void sys_setitimer(void); EXPORT_SYMBOL(sys_setitimer);
extern void sys_kexec_load(void); EXPORT_SYMBOL(sys_kexec_load);
extern void sys_syslog(void); EXPORT_SYMBOL(sys_syslog);
extern void sys_ptrace(void); EXPORT_SYMBOL(sys_ptrace);
extern void sys_nice(void); EXPORT_SYMBOL(sys_nice);
extern void sys_sched_setscheduler(void); EXPORT_SYMBOL(sys_sched_setscheduler);
extern void sys_sched_setparam(void); EXPORT_SYMBOL(sys_sched_setparam);
extern void sys_sched_getscheduler(void); EXPORT_SYMBOL(sys_sched_getscheduler);
extern void sys_sched_getparam(void); EXPORT_SYMBOL(sys_sched_getparam);
extern void sys_sched_setaffinity(void); EXPORT_SYMBOL(sys_sched_setaffinity);
extern void sys_sched_getaffinity(void); EXPORT_SYMBOL(sys_sched_getaffinity);
extern void sys_sched_yield(void); EXPORT_SYMBOL(sys_sched_yield);
extern void sys_sched_get_priority_max(void); EXPORT_SYMBOL(sys_sched_get_priority_max);
extern void sys_sched_get_priority_min(void); EXPORT_SYMBOL(sys_sched_get_priority_min);
extern void sys_restart_syscall(void); EXPORT_SYMBOL(sys_restart_syscall);
extern void sys_tgkill(void); EXPORT_SYMBOL(sys_tgkill);
extern void sys_setpriority(void); EXPORT_SYMBOL(sys_setpriority);
extern void sys_getpriority(void); EXPORT_SYMBOL(sys_getpriority);
extern void sys_reboot(void); EXPORT_SYMBOL(sys_reboot);
extern void sys_setregid(void); EXPORT_SYMBOL(sys_setregid);
extern void sys_setgid(void); EXPORT_SYMBOL(sys_setgid);
extern void sys_setreuid(void); EXPORT_SYMBOL(sys_setreuid);
extern void sys_setuid(void); EXPORT_SYMBOL(sys_setuid);
extern void sys_setresuid(void); EXPORT_SYMBOL(sys_setresuid);
extern void sys_getresuid(void); EXPORT_SYMBOL(sys_getresuid);
extern void sys_setresgid(void); EXPORT_SYMBOL(sys_setresgid);
extern void sys_getresgid(void); EXPORT_SYMBOL(sys_getresgid);
extern void sys_setfsuid(void); EXPORT_SYMBOL(sys_setfsuid);
extern void sys_setfsgid(void); EXPORT_SYMBOL(sys_setfsgid);
extern void sys_times(void); EXPORT_SYMBOL(sys_times);
extern void sys_setpgid(void); EXPORT_SYMBOL(sys_setpgid);
extern void sys_getpgid(void); EXPORT_SYMBOL(sys_getpgid);
extern void sys_getpgrp(void); EXPORT_SYMBOL(sys_getpgrp);
extern void sys_getsid(void); EXPORT_SYMBOL(sys_getsid);
extern void sys_setsid(void); EXPORT_SYMBOL(sys_setsid);
extern void sys_getgroups(void); EXPORT_SYMBOL(sys_getgroups);
extern void sys_setgroups(void); EXPORT_SYMBOL(sys_setgroups);
extern void sys_newuname(void); EXPORT_SYMBOL(sys_newuname);
extern void sys_sethostname(void); EXPORT_SYMBOL(sys_sethostname);
extern void sys_gethostname(void); EXPORT_SYMBOL(sys_gethostname);
extern void sys_setdomainname(void); EXPORT_SYMBOL(sys_setdomainname);
extern void sys_getrlimit(void); EXPORT_SYMBOL(sys_getrlimit);
extern void sys_old_getrlimit(void); EXPORT_SYMBOL(sys_old_getrlimit);
extern void sys_setrlimit(void); EXPORT_SYMBOL(sys_setrlimit);
extern void sys_getrusage(void); EXPORT_SYMBOL(sys_getrusage);
extern void sys_umask(void); EXPORT_SYMBOL(sys_umask);
extern void sys_prctl(void); EXPORT_SYMBOL(sys_prctl);
extern void sys_ni_syscall(void); EXPORT_SYMBOL(sys_ni_syscall);
extern void sys_sysctl(void); EXPORT_SYMBOL(sys_sysctl);
extern void sys_time(void); EXPORT_SYMBOL(sys_time);
extern void sys_stime(void); EXPORT_SYMBOL(sys_stime);
extern void sys_gettimeofday(void); EXPORT_SYMBOL(sys_gettimeofday);
extern void sys_settimeofday(void); EXPORT_SYMBOL(sys_settimeofday);
extern void sys_adjtimex(void); EXPORT_SYMBOL(sys_adjtimex);
extern void sys_alarm(void); EXPORT_SYMBOL(sys_alarm);
extern void sys_getpid(void); EXPORT_SYMBOL(sys_getpid);
extern void sys_getppid(void); EXPORT_SYMBOL(sys_getppid);
extern void sys_getuid(void); EXPORT_SYMBOL(sys_getuid);
extern void sys_geteuid(void); EXPORT_SYMBOL(sys_geteuid);
extern void sys_getgid(void); EXPORT_SYMBOL(sys_getgid);
extern void sys_getegid(void); EXPORT_SYMBOL(sys_getegid);
extern void sys_gettid(void); EXPORT_SYMBOL(sys_gettid);
extern void sys_nanosleep(void); EXPORT_SYMBOL(sys_nanosleep);
extern void sys_sysinfo(void); EXPORT_SYMBOL(sys_sysinfo);
extern void sys_fadvise64_64(void); EXPORT_SYMBOL(sys_fadvise64_64);
extern void sys_fadvise64(void); EXPORT_SYMBOL(sys_fadvise64);
extern void sys_readahead(void); EXPORT_SYMBOL(sys_readahead);
extern void sys_remap_file_pages(void); EXPORT_SYMBOL(sys_remap_file_pages);
extern void sys_madvise(void); EXPORT_SYMBOL(sys_madvise);
extern void sys_mbind(void); EXPORT_SYMBOL(sys_mbind);
extern void sys_set_mempolicy(void); EXPORT_SYMBOL(sys_set_mempolicy);
extern void sys_get_mempolicy(void); EXPORT_SYMBOL(sys_get_mempolicy);
extern void sys_mincore(void); EXPORT_SYMBOL(sys_mincore);
extern void sys_mlock(void); EXPORT_SYMBOL(sys_mlock);
extern void sys_munlock(void); EXPORT_SYMBOL(sys_munlock);
extern void sys_mlockall(void); EXPORT_SYMBOL(sys_mlockall);
extern void sys_munlockall(void); EXPORT_SYMBOL(sys_munlockall);
extern void sys_brk(void); EXPORT_SYMBOL(sys_brk);
extern void sys_munmap(void); EXPORT_SYMBOL(sys_munmap);
extern void sys_mremap(void); EXPORT_SYMBOL(sys_mremap);
extern void sys_msync(void); EXPORT_SYMBOL(sys_msync);
extern void sys_swapoff(void); EXPORT_SYMBOL(sys_swapoff);
extern void sys_swapon(void); EXPORT_SYMBOL(sys_swapon);
extern void sys_set_zone_reclaim(void); EXPORT_SYMBOL(sys_set_zone_reclaim);
extern void sys_socket(void); EXPORT_SYMBOL(sys_socket);
extern void sys_socketpair(void); EXPORT_SYMBOL(sys_socketpair);
extern void sys_bind(void); EXPORT_SYMBOL(sys_bind);
extern void sys_listen(void); EXPORT_SYMBOL(sys_listen);
extern void sys_accept(void); EXPORT_SYMBOL(sys_accept);
extern void sys_connect(void); EXPORT_SYMBOL(sys_connect);
extern void sys_getsockname(void); EXPORT_SYMBOL(sys_getsockname);
extern void sys_getpeername(void); EXPORT_SYMBOL(sys_getpeername);
extern void sys_sendto(void); EXPORT_SYMBOL(sys_sendto);
extern void sys_send(void); EXPORT_SYMBOL(sys_send);
extern void sys_recvfrom(void); EXPORT_SYMBOL(sys_recvfrom);
extern void sys_recv(void); EXPORT_SYMBOL(sys_recv);
extern void sys_setsockopt(void); EXPORT_SYMBOL(sys_setsockopt);
extern void sys_getsockopt(void); EXPORT_SYMBOL(sys_getsockopt);
extern void sys_shutdown(void); EXPORT_SYMBOL(sys_shutdown);
extern void sys_sendmsg(void); EXPORT_SYMBOL(sys_sendmsg);
extern void sys_recvmsg(void); EXPORT_SYMBOL(sys_recvmsg);
extern void sys_socketcall(void); EXPORT_SYMBOL(sys_socketcall);
extern void sys_select(void); EXPORT_SYMBOL(sys_select);
extern void sys_kill(void); EXPORT_SYMBOL(sys_kill);
// extern void set_task_comm(void); EXPORT_SYMBOL(set_task_comm);
/*already declared*/  EXPORT_SYMBOL(set_task_comm);

/* END ADDED BY ATHEROS */


