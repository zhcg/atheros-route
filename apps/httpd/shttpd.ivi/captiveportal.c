/* captiveportal.c - captive portal for ctc
 * Author: bowen
 * Date:   2009-03-12 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/if_arp.h>
#include <string.h>

#define _DEBUG
#include "debug.h"
#include "captiveportal.h"

/*
int main(){
    if_status_t if_status;
    get_ifinfo(&if_status, "eth0");

    printf("===> actived = %d\n", if_status.actived);
    printf("===> actived = %s\n", if_status.ipaddr);
    printf("===> actived = %s\n", if_status.mac);

    return 0;
}
*/

int get_ifinfo(if_status_t *if_status_p, char * ifname)
{
#define MAX_INTERFACES 16
    int fd, ifnum, ret = -1;
    struct ifreq buf[MAX_INTERFACES];
    struct arpreq arp;
    struct ifconf ifc;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        goto end;
    
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = (caddr_t) buf;
    if (ioctl(fd, SIOCGIFCONF, (char *) &ifc) != 0)
        goto end;
    
    ifnum = ifc.ifc_len / sizeof(struct ifreq);
    DBG((stderr, "===> ifnum = %d", ifnum));
    
    while (ifnum -- > 0)
    {
        DBG((stderr, "===> ifname = %s", buf[ifnum].ifr_name));

            /*Jugde whether the net card status is promisc */
        if (ioctl(fd, SIOCGIFFLAGS, (char *) &buf[ifnum]) != 0)
            goto end;

        if (buf[ifnum].ifr_flags & IFF_PROMISC)
            goto end;

        if(strcmp(buf[ifnum].ifr_name, ifname) == 0)
        {
                /*Jugde whether the net card status is up */
            if (buf[ifnum].ifr_flags & IFF_UP) 
                if_status_p -> actived = 1;
            else
                if_status_p -> actived = 0;
            
                /*Get IP of the net card */
            if (ioctl(fd, SIOCGIFADDR, (char *) &buf[ifnum]) != 0)
                goto end;

            sprintf(if_status_p -> ipaddr, "%s",
                    inet_ntoa(((struct sockaddr_in *)
                               (&buf[ifnum].ifr_addr)) -> sin_addr));
            
                /*Get HW ADDRESS of the net card */
            if (ioctl(fd, SIOCGIFHWADDR, (char *) &buf[ifnum]) != 0)
                goto end;

            sprintf(if_status_p -> mac, "%02x%02x%02x%02x%02x%02x",
                    (unsigned char) buf[ifnum].ifr_hwaddr.
                    sa_data[0],
                    (unsigned char) buf[ifnum].ifr_hwaddr.
                    sa_data[1],
                    (unsigned char) buf[ifnum].ifr_hwaddr.
                    sa_data[2],
                    (unsigned char) buf[ifnum].ifr_hwaddr.
                    sa_data[3],
                    (unsigned char) buf[ifnum].ifr_hwaddr.
                    sa_data[4],
                    (unsigned char) buf[ifnum].ifr_hwaddr.
                    sa_data[5]); 
            DBG((stderr, "===> ifmac = %s", if_status_p -> mac));

            close(fd);
            return 0;
        }
    }
  end:
    close(fd);
    return -1;   
#undef MAX_INTERFACES
}


