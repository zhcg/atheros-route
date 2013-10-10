#ifndef FLOCK_H
#define FLOCK_H
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

inline int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
    struct flock    lock;

    lock.l_type = type;     /* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start = offset;  /* byte offset, relative to l_whence */
    lock.l_whence = whence; /* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len = len;       /* #bytes (0 means to EOF) */

    return( fcntl(fd, cmd, &lock) );
}

#define read_lock(fd, offset, whence, len) \
                        lock_reg(fd, F_SETLK, F_RDLCK, offset, whence, len)
#define write_lock(fd, offset, whence, len) \
                        lock_reg(fd, F_SETLK, F_WRLCK, offset, whence, len)
#define write_unlock(fd, offset, whence, len) \
                        lock_reg(fd, F_SETLK, F_UNLCK, offset, whence, len)

inline int write_lock_wait(int fd, off_t offset, int whence, off_t len, int timeout_msec)
{
    while((write_lock(fd, offset, whence, len) < 0) && 
            (timeout_msec-- > 0))
        usleep(1000);
    
    return (timeout_msec > 0 ? 0 : -1);
}

#define err_sys(x) { perror(x); exit(1); }
#endif
