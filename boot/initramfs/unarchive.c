/* unarchive -- stream output of decompression.
*       The binary cpio format is assumed; endianess automatically handled
*       so long as it is pure big endian or pure little endian.
*       The input should list directores before the nodes that need
*       the directory.
*
* The format is repetitions of {header,filepath,filecontent}...
* ending when filepath == "TRAILER!!!".
* The information in the header is closely related to unix/linux "stat"
* information, see "man 2 stat".
* A terminating null is always included in the namesize; an extra null
* is added in the stream (but not in namesize) to round out to 2 byte
* alignment.
* Likewise, an additional null is present in the stream if the filesize
* is not a multiple of 2.
*
* Important: chdir to root directory if you expect the files to be
* restored beneath root directory.
*
* NOTE: errors are NOT reported... beware.
*/

// #include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#if 0   /* might be smaller */
void *memset(void *s, int c, size_t n)
{
    unsigned char *res = s;
    unsigned char *b = s;
    while (n-- > 0) *b++ = c;
    return res;
}
#endif

#if 0   /* might be smaller */
void *memcpy(void *dest, const void *src, size_t n)
{
    void *res = dest;
    unsigned char *t = dest;
    const unsigned char *f = src;
    while (n > 0)
        *t++ = *f++, n--;
    return res;
}
#endif

#if 0   /* might be smaller */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2 && *s1 == *s2) s1++, s2++;
    return *s1 - *s2;
}
#endif


struct cpioHeader {
    unsigned short magic;
    unsigned short dev;
    unsigned short ino;
    unsigned short mode;
    unsigned short uid;
    unsigned short gid;
    unsigned short nlink;
    unsigned short rdev;
    unsigned short mtime[2];    /* big endian w.r.t. shortwords */
    unsigned short namesize;
    unsigned short filesize[2]; /* big endian w.r.t. shortwords */
};

enum unarchiveState {
    unarchiveState_eStart,
    unarchiveState_eInHeader,
    unarchiveState_eInName,
    unarchiveState_eInFile,
    unarchiveState_eInSymlink,
    unarchiveState_eDone,
    unarchiveState_ePoison      /* if we've lost sync */
};

struct {
    enum unarchiveState State;
    int NErrors;
    int NPartial;               /* how much we have if only partly read */
    int NBytes;                 /* how big the entire portion is */
    int Round;                  /* 0, or 1 for round up of regular file only */
    unsigned FileSize;
    int FileType;
    int Fd;                     /* of file being written to */
    struct cpioHeader Header;   /* current header */
    char FilePath[256];
    char SymLink[256];
} unarchiveS = {
    .State = unarchiveState_eStart,
    .Fd = -1,
};


/*-F- unarchiveInit -- initialize unarchiver.
*       NOTE that only one unarchive operation may occur at a time
*       because the state is held in a static variable.
*/
void unarchiveInit(void)
{
    memset(&unarchiveS, 0, sizeof(unarchiveS));
    unarchiveS.Fd = -1;
    unarchiveS.NErrors = 0;
}

/*-F- unarchiveBuf -- unarchive from a stream.
*       buf contains the next portion of the stream.
*/
void unarchiveBuf(const unsigned char *buf, unsigned nbytes)
{
    for (;;) {
        if (nbytes <= 0) {
            return;
        }
        switch (unarchiveS.State) {
            case unarchiveState_eStart:
                unarchiveS.State = unarchiveState_eInHeader;
                unarchiveS.NPartial = 0;
                unarchiveS.NBytes = sizeof(unarchiveS.Header);
            continue;
            case unarchiveState_eInHeader: {
                unsigned char *copyto = (unsigned char *)&unarchiveS.Header
                    + unarchiveS.NPartial;
                unsigned need = unarchiveS.NBytes - unarchiveS.NPartial;
                if (nbytes < need) {
                    memcpy(copyto, buf, nbytes);
                    unarchiveS.NPartial += nbytes;
                    return;
                }
                memcpy(copyto, buf, need);
                buf += need;
                nbytes -= need;

                if (unarchiveS.Header.magic == 0xc771) {
                    /* reverse endian -- swap bytes! */
                    unsigned char *h = (void *)&unarchiveS.Header;
                    int ns = sizeof(unarchiveS.Header);
                    int i;
                    unsigned temp;
                    for (i = 0; i < ns; i += 2) {
                        temp = h[i];
                        h[i] = h[i+1];
                        h[i+1] = temp;
                    }
                }
                if (unarchiveS.Header.magic == 0x71c7) {
                    /* same endian */
                    int Mode = unarchiveS.Header.mode;
                    unarchiveS.State = unarchiveState_eInName;
                    memset(unarchiveS.FilePath, 0, sizeof(unarchiveS.FilePath));
                    unarchiveS.FileType = (Mode >> 12) & 0xf;
                    unarchiveS.NPartial = 0;
                    unarchiveS.FileSize = 
                        unarchiveS.Header.filesize[0] << 16 |
                        unarchiveS.Header.filesize[1];
                    unarchiveS.NBytes = unarchiveS.Header.namesize;
                    /* round up. This may read in two nuls intead of one
                    *   which should make no difference.
                    */
                    unarchiveS.NBytes = (unarchiveS.NBytes+1) & ~1;
                } else {
                    goto Fail;
                }
            } break;
            case unarchiveState_eInName: {
                char *copyto = unarchiveS.FilePath + unarchiveS.NPartial;
                unsigned need = unarchiveS.NBytes - unarchiveS.NPartial;
                if (nbytes < need) {
                    memcpy(copyto, buf, nbytes);
                    unarchiveS.NPartial += nbytes;
                    return;
                }
                memcpy(copyto, buf, need);
                buf += need;
                nbytes -= need;

                #if DO_DEBUG
                do_debug_unarchive_filepath(unarchiveS.FilePath);
                #endif
                if (!strcmp(unarchiveS.FilePath, "TRAILER!!!")) {
                    unarchiveS.State = unarchiveState_eDone;
                    break;
                }

                switch (unarchiveS.FileType) {
                    case 012:   /* symbolic link */
                        unarchiveS.State = unarchiveState_eInSymlink;
                        unarchiveS.NPartial = 0;
                        unarchiveS.NBytes = unarchiveS.FileSize;
                        memset(unarchiveS.SymLink, 0,
                            sizeof(unarchiveS.SymLink));
                        /* round up. This reads in extra nul
                        *   which should make no difference.
                        */
                        unarchiveS.NBytes = (unarchiveS.NBytes+1) & ~1;
                    break;
                    case 010:   /* regular file */
                        unarchiveS.State = unarchiveState_eInFile;
                        unarchiveS.NPartial = 0;
                        unarchiveS.NBytes = unarchiveS.FileSize;
                        /* round up. But we must make sure we don't 
                        *  write any round up byte!
                        */
                        unarchiveS.Round = unarchiveS.NBytes & 1;
                        unarchiveS.Fd = open(unarchiveS.FilePath,
                                O_WRONLY|O_CREAT, unarchiveS.Header.mode);
                        if (unarchiveS.Fd < 0) {
                            unarchiveS.NErrors++;
                            goto Fail;
                        }
                    break;
                    case 014:   /* socket file */
                    case 001:   /* fifo device */
                    case 006:   /* block device */
                    case 002:   /* character device */
                        if (mknod(unarchiveS.FilePath, 
                            unarchiveS.Header.mode,
                            unarchiveS.Header.rdev))
                                unarchiveS.NErrors++;
                        unarchiveS.State = unarchiveState_eStart;
                    break;
                    case 004:   /* directory */
                        if (mkdir(unarchiveS.FilePath, 
                                unarchiveS.Header.mode)) {
                            if (errno == EEXIST) {;}
                            else goto Fail;
                        }
                        unarchiveS.State = unarchiveState_eStart;
                    break;
                    default:
                        unarchiveS.State = unarchiveState_eStart;
                    break;
                }
            } break;
            case unarchiveState_eInFile: {
                unsigned need = unarchiveS.NBytes - unarchiveS.NPartial;
                if (need == 0) {
                    if (unarchiveS.Round) {
                        buf++;
                        nbytes--;
                    }
                    if (close(unarchiveS.Fd)) {
                        unarchiveS.NErrors++;
                    }
                    unarchiveS.Fd = -1;
                    unarchiveS.State = unarchiveState_eStart;
                    break;
                }
                if (need > nbytes)
                    need = nbytes;
                write(unarchiveS.Fd, buf, need);
                buf += need;
                nbytes -= need;
                unarchiveS.NPartial += need;
            } break;
            case unarchiveState_eInSymlink: {
                char *copyto = unarchiveS.SymLink + unarchiveS.NPartial;
                unsigned need = unarchiveS.NBytes - unarchiveS.NPartial;
                if (nbytes < need) {
                    memcpy(copyto, buf, nbytes);
                    unarchiveS.NPartial += nbytes;
                    return;
                }
                memcpy(copyto, buf, need);
                buf += need;
                nbytes -= need;
                if (symlink(unarchiveS.SymLink, unarchiveS.FilePath)) 
                    unarchiveS.NErrors++;
                unarchiveS.State = unarchiveState_eStart;
            } break;
            case unarchiveState_eDone:
                /* Shouldn't get here, but we do... just ignore it */
            return;
            case unarchiveState_ePoison:
            default:
            goto Fail;
        }
    }   /* forever loop */
    return;

    Fail:
    unarchiveS.NErrors++;
    if (unarchiveS.Fd > 0) {
        close(unarchiveS.Fd);
        unarchiveS.Fd = -1;
    }
    unarchiveS.State = unarchiveState_ePoison;
    return;
}

/*-F- unarchiveDoneOK -- returns nonzero if called at end of unarchive
*       and was ok.
*/
int unarchiveDoneOK(void) {
    return unarchiveS.State == unarchiveState_eDone &&
        unarchiveS.NErrors == 0;
}

