/*
* wastemem is a program that deliberately wastes the amount of memory specified
* as a command line argument.
* This is useful for development purposes, to reveal by a process of 
* experimentation how much memory is really available for use.
* 
* Usage:  wastemem <nbytes>... &
* For each <nbytes> arg, a malloc of that size is done and initialized.
* The program then spins in an infinite loop calling sleep(100);
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
    /* Get arg(s) */
    for (argv++; *argv; argv++) {
        unsigned NBytes = atol(*argv);
        char *Ptr = malloc(NBytes+1);
            fprintf(stderr, "wastemem: malloc %u bytes OK\n", NBytes);
            memset(Ptr, 0x1, NBytes+1); /* force the memory to be real */
        if (Ptr) {
        } else {
            fprintf(stderr, "wastemem: Failed to malloc %u bytes\n", NBytes);
            fprintf(stderr, "wastemem: ABORT!\n");
            exit(1);
        }
    }

    /* Wait forever, wasting the memory */
    for (;;) {
        sleep(100);
    }

    return 0;
}

