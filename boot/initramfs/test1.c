/* test1 -- test unarchive
*
*       Build: gcc -Wall -g test1.c -o test1
*       Usage: test1 <file.cpio
*               (archive file must use relative paths!)
*       Creates junk.test1 based on archive.
*/
#include <stdio.h>

static void do_debug_unarchive_filepath(const char *filepath)
{
    printf("x %s\n", filepath);
}

#define DO_DEBUG 1
#include "unarchive.c"

#include <stdlib.h>

#define BUFSIZE 4096

int main(int argc, char **argv)
{
    unsigned char buf[BUFSIZE];

    printf("Making temp directory junk.test1...\n");
    system("rm -rf junk.test1");
    mkdir("junk.test1", 0777);
    if (chdir("junk.test1")) {
        printf("Failed to create junk.test1\n");
        return 1;
    }

    printf("Unarchiving from stdin to junk.test1...\n");
    for (;;) {
        int nread = read(0, buf, sizeof(buf));
        if (nread <= 0) 
            break;
        unarchiveBuf(buf, nread);
        if (unarchiveDoneOK()) {
            printf("Unarchive done OK\n");
        } else {
            printf("Unarchive failure(s)!\n");
        }
    }
    return 0;
}


