/*
* inittrampoline -- a substitute "init" program (installed as /init) that 
*       decompresses an external archive file into the root file system
*       (assumed to be "rootfs")
*       and then execs to the traditional /sbin/init.
* NOTE: there are some important installation and use notes in
*       initramfs.txt that you should read!
* Required arg: (see also initramfs.txt!):
*       -A 0x<hexaddress> -- where to find external archive file.
* Additional args are passed to /sbin/init.
*
*
*
* Implementation notes:
* inittrampoline (installed as "/init") runs briefly, after which it
* can be deleted from RAM... but it occupies space in flash, and so should
* be kept as small as possible.
* It also must be linked staticly.
* Therefore, it must use as few C library calls as possible.
*/

#include <unistd.h>
#include <sys/mman.h>

/* Don't call printf, too big. Instead we use this one function for
* debug messages.
*/
static void message(const char *msg)
{
    int len;
    for (len = 0; msg[len]; len++) {;}
    write(1/*STDOUT == console*/, msg, len);
}

#define DO_DEBUG 1

/* Called from unarchive for debugging purposes so we can trace
* the progress... this can be removed by setting DO_DEBUG to 0
* when we fell certain about it's operation.
*/
#if DO_DEBUG
static void do_debug_unarchive_filepath(const char *filepath)
{
    message("x ");
    message(filepath);
    message("\n");
}
#endif

/* Include subroutines, saves having to create include files
*       and alllows easy sharing of e.g. DO_DEBUG.
*/
/* cpio un-archiver: */
#include "unarchive.c"
/* lzma decoder: */
#include "lzma/LzmaDec.c"
#include "decode.c"

/* parsehex -- convert hex to integer, for argumeent parsing */
static unsigned parsehex(const char *s)
{
    unsigned sum = 0;
    int c;
    while ((c = *s++) != 0) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else 
        if (c >= 'A' && c <= 'F') 
            c = 10 + c - 'A';
        else 
        if (c >= 'a' && c <= 'f') 
            c = 10 + c - 'a';
        else
            return 0;
        sum = (sum << 4) | (unsigned)c;
    }
    return sum;
}

/* formathex -- convert integer to string, for debugging.
*       Returns address of static cell which is overwritten each time.
*/
static char *formathex(unsigned value)
{
    static char buf[9];
    int i;
    for (i = 0; i < 8; i++) {
        unsigned nibble = (value >> (28-4*i))&0xf;
        if (nibble < 10)
            buf[i] = '0' + nibble;
        else
            buf[i] = 'A' + nibble - 10;
    }
    buf[8] = 0;
    return buf;
}

/* mapping variables, held as global variables to eliminate the
*       need for passing context around.
*       To avoid possible problems with a large mapping, only
*       a piece of the external archive (usually in flash memory)
*       is mapped at a time.
*/
int mapfd = -1;
unsigned map_physaddr = 0;
unsigned char *map_vaddr = NULL;
int map_length = 0;

/* amount to map each time is arbitrary, 
*       but keep aligned (just in case) and don't make too big (just in case)
*/
#define default_map_length 0x40000

/* GetNextBuffer returns buffer with next bunch of bytes to process
*       for decode; no. of bytes is returned via *nbytes
*/
static unsigned char *GetNextBuffer(unsigned *nbytes)
{
    if (mapfd < 0) {
        mapfd = open("/dev/mem", O_RDONLY);
        if (mapfd < 0) {
            message("Open of /dev/mem failed!\n");
            return NULL;
        }
        message("Open of /dev/mem OK\n");
    }
    if (map_vaddr) {
        if (munmap(map_vaddr, map_length)) {
            message("munmap of /dev/mem failed!\n");
        }
        map_vaddr = NULL;
        map_physaddr += map_length;
    }
    map_length = default_map_length;
    *nbytes = map_length;
    map_vaddr = mmap(NULL/*don't care*/, map_length,
        PROT_READ, MAP_PRIVATE, mapfd, map_physaddr/*offset in /dev/mem*/);
    if (map_vaddr == (void *)-1) {
        message("mmap of /dev/mem failed!\n");
        return NULL;
    }
    message("phys addr="); message(formathex(map_physaddr));
        message(" map_vaddr="); message(formathex((unsigned)map_vaddr));
        message("\n");
    // message("first word of mapped memory is:\n");
    // message(formathex(*(unsigned *)map_vaddr)); message("\n");
    /* don't worry about closing fd */
    return map_vaddr;
}



int main(int argc, char **argv)
{
    message("\ninitrampoline -- Begin\n");
    chdir("/"); /* so we unarchive relative to root directory */
    argv++;
    /* Args for init should have been passed via kernel args */
    if (argv[0] && argv[0][0] == '-' && argv[0][1] == 'A' && argv[0][2] == 0 &&
            argv[1] && argv[1][0] == '0' && argv[1][1] == 'x') {
        unsigned addr;
        addr = parsehex(argv[1]+2);
        if (addr == 0) {
            message("inittrampoline -- Invalid archive file address!: ");
                message(argv[1]); message("\n");
        } else {
            message("initrampoline -- Archive file specified at ");
                message(argv[1]); message("\n");
            map_physaddr = addr;
            if (decode(GetNextBuffer)) {
                message("inittrampoline -- Decode failed!");
            } else {
                message("inittrampoline -- Decode OK.");
                if (unarchiveDoneOK()) {
                    message("inittrampoline -- Unarchive OK.");
                } else {
                    message("inittrampoline -- Unarchive failed!");
                }
            }
        }
        argv += 2;
    } else {
        message("initrampoline -- No archive file specified -- probably won't work!\n");
    }
    /* Even if the above failed, continue on... maybe we'll get lucky... */
    argv--;
    message("initrampoline -- Exec /sbin/init\n");
    argv[0] = "/sbin/init";
    execv("/sbin/init", argv);
    message("initrampoline -- FAILED to exec /sbin/init\n");
    return 1;
}
