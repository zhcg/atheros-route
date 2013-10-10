/* test2 -- test lzma decompress from block + unarchive.
*       Build: gcc -Wall -g test2.c -o test2
*       Usage: test2 file.cpio.lzma
*               (archive file must use relative paths!)
*       Creates junk.test2 based on archive.
*/

#include <stdio.h>

static void do_debug_unarchive_filepath(const char *filepath)
{
    printf("x %s\n", filepath);
}

#define DO_DEBUG 1
#include "unarchive.c"

#include "decode.c"

#include "lzma/LzmaDec.c"

#include <stdlib.h>

unsigned char *buf;
unsigned bufsize;
unsigned bufpos;

static unsigned char *testGetBuf(unsigned *nbytes)
{
    int available = bufsize - bufpos;
    unsigned char *p = buf + bufpos;
    if (available > 0x10000) available = 0x10000;
    *nbytes = available;
    bufpos += available;
    return p;
}

int main(int argc, char **argv)
{
    char *filepath;
    struct stat Stat;
    int fd;

    filepath = argv[1];
    if (!filepath || *filepath == '-') {
        printf("Missing arg\n");
        return 1;
    }
    if (stat(filepath, &Stat)) {
        printf("Missing file: %s\n", filepath);
        return 1;
    }
    bufsize = Stat.st_size;
    buf = malloc(bufsize);
    if (buf == NULL) {
        printf("Malloc failure, %u bytes\n", (unsigned)Stat.st_size);
        return 1;
    }
    fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        printf("Could not open %s\n", filepath);
        return 1;
    }
    if (read(fd, buf, bufsize) != bufsize) {
        printf("Could not read %s\n", filepath);
        return 1;
    }
    close(fd);

    printf("Making temp directory junk.test2...\n");
    system("rm -rf junk.test2");
    mkdir("junk.test2", 0777);
    if (chdir("junk.test2")) {
        printf("Failed to create junk.test2\n");
        return 1;
    }
    if (decode(testGetBuf) ) {
        printf("Decode failed!\n");
    } else  {
        printf("Decode done OK\n");
    }
    if (unarchiveDoneOK()) {
        printf("Unarchive done OK\n");
    } else {
        printf("Unarchive failure(s)!\n");
    }
    return 0;
}
