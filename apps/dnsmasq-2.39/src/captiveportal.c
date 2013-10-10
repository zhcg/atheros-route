/* captiveportal.c - captive portal for ctc
 * Author: bowen
 * Date:   2009-03-12 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h> ////
#include <net/if.h>
#include <netinet/in.h> ////
#include <net/if_arp.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define _DEBUG
#include "debug.h"
#include "captiveportal.h"

/*
int main(){
    if_status_t if_status;
    get_ifinfo(&if_status, "br0");

    printf("===> actived = %d\n", if_status.actived);
    printf("===> actived = %s\n", if_status.ipaddr);
    printf("===> actived = %s\n", if_status.mac);

    return 0;
}
*/

int get_ifinfo(if_status_t *if_status_p, const char * const ifname)
{
#define MAX_INTERFACES 16
    int fd, ifnum;
    struct ifreq buf[MAX_INTERFACES];
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

void captiveportal(const char * const ifs_name,
                  const char * const ifc_ip)
{
#define CAPTIVEFILENAME "/tmp/redirect_iplist"
    FILE * fp = NULL;
    char line[LINE_LEN];
    char remote_ip[strlen(ifc_ip) + 1];

    strcpy(remote_ip, ifc_ip);
    
    if(((fp = fopen(CAPTIVEFILENAME, "r")) != NULL) /*&&*/ /* captive file not existed, no need redirect */
       /* (memcmp(emac_old, emac, emac_len) != 0)*/) /* empired and renew, no need redirect */
    {
        while(fgets(line, LINE_LEN, fp) != NULL)
        {
                /* rule exist, delete one by one */
            if(strstr(line, remote_ip) != NULL)
            {
                FILE * fp_p = NULL;
                char line_p[LINE_LEN];
                if_status_t if_status;
                if((fp_p = popen("iptables -t nat -L "CHAIN_NAME, "r")) != NULL)
                {
                    while(fgets(line_p, LINE_LEN, fp_p) != NULL)
                    {
                            /* rule exist, not add again */
                        if((strstr(line_p,  remote_ip) != NULL) ||
                           (strstr("0.0.0.0", remote_ip) != NULL))
                            goto end;
                    }

                    char cmdbuf[CMD_BUF_LEN];
                    get_ifinfo(&if_status, ifs_name);

                    sprintf(cmdbuf, "iptables -t nat -A %s -p tcp -s %s -d! %s --dport 80 -i %s -j REDIRECT --to-ports %d",
                            CHAIN_NAME, remote_ip, if_status.ipaddr, ifs_name, REDIRECTPORT);
                    system(cmdbuf);
                        // memset(emac_old, 0xff, MAC_MAX_LEN);
                    fprintf(stderr, "===> iptables command = %s\n", cmdbuf);
                  end:
                    pclose(fp_p);
                }

                break;
            }
        }

        fclose(fp);
    }
#undef CAPTIVEFILENAME
}
