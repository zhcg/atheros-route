#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include "busybox.h"

static volatile int fd = 0;

static void siginthnd(int signo)
{
    printf("signal caught. closing fr device\n");
    if (fd != 0)
        close(fd);
    exit(0);
}

extern int factoryreset_main(int argc, char **argv)
{
	if (daemon(0, 1) < 0)
		bb_perror_msg_and_die("Failed forking factory reset daemon");

	signal(SIGHUP, siginthnd);
	signal(SIGINT, siginthnd);

	fd = bb_xopen(argv[argc - 1], O_WRONLY);

        if (fd < 0)
		bb_perror_msg_and_die("Failed to open factory reset device");

        ioctl(fd, 0x89ABCDEF, 0);

	close(fd);

        printf("\nRestoring the factory default configuration ....\n");
        fflush(stdout);

        /* Restore the factory default settings */
        system("cfg -x");
        sleep(1);

        reboot(RB_AUTOBOOT);
	return EXIT_SUCCESS;
}
