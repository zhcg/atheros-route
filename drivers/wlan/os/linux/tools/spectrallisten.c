
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>

#define ATH_PORT    "8001"
#define HOST        "192.168.2.1"
#define RECVSIZE    8192

void usage(char *prog)
{
    printf("Usage: %s [-h] [-tcp] [-host <ip>] [num_samples]\ndefault: num_samples=1, udp", prog);
}

int main(int argc, char **argv)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sd, s, dgram = 1, num_samples = 1;
    ssize_t nrecv, nsend;
    char recv_buf[RECVSIZE], host_ip[32];

    strcpy(host_ip, HOST);
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
        if (strcmp(argv[1], "-tcp") == 0) {
            argv += 1;
            argc--;
            dgram = 0;
        }
        if (strcmp(argv[1], "-host") == 0) {
            strcpy(host_ip, argv[2]);
            argv += 2;
            argc -= 2;
        }
        if (argc)
            num_samples = atoi(argv[1]);
    }

    /* Obtain address(es) matching host/port */

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;    /* Allow IPv4 or IPv6 */
    if (dgram)
        hints.ai_socktype = SOCK_DGRAM; /* udp socket */
    else
        hints.ai_socktype = SOCK_STREAM; /* tcp socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(host_ip, ATH_PORT, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
     * Try each address until we successfully connect(2).
     * If socket(2) (or connect(2)) fails, we (close the socket
     * and) try the next address. */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sd == -1)
            continue;

        if (dgram) 
            break;

        if (connect(sd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(sd);
    }
    if (rp == NULL) {
        fprintf(stderr, "could not connect\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result);

    if (dgram) {
        nsend = sendto(sd, recv_buf, 16, 0, rp->ai_addr, rp->ai_addrlen); /* just for sync purposes */
        if (nsend  == -1) {
            close(sd);
            perror("sendto");
            return 1;
        }
printf("sent\n");
    }

    /* wait for a message to come back from the server */
    while (num_samples--) {
        if (dgram)
            nrecv = recvfrom(sd, recv_buf, RECVSIZE, 0, rp->ai_addr, &rp->ai_addrlen);
        else
            nrecv = recv(sd, recv_buf, RECVSIZE, 0);
        if (nrecv  == -1) {
            close(sd);
            perror("recv");
            return 1;
        }
        printf(" R(%d) ", nrecv);
    }

    close(sd);
    return 0;
}

 
