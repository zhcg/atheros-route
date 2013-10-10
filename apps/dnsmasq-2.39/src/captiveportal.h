/* captiveportal.h - captive portal for ctc
 * Author: bowen
 * Date:   2009-03-10 */
#ifndef CAPTIVEPORTAL_H
#define CAPTIVEPORTAL_H
#undef CAPTIVEPORTAL
#define CHAIN_NAME "CAPTIVEPORTAL_CHAIN"
#define MAC_MAX_LEN 32
#define CMD_BUF_LEN 256
#define LINE_LEN 512
#define REDIRECTPORT 61176

#define MAC_LEN 32
#define IP_LEN 32   
typedef struct if_status_struct{
    int actived;                /* 0 - down, 1 - up */
    char ipaddr[IP_LEN];
    char mac[MAC_LEN];
}if_status_t;

int get_ifinfo(if_status_t *if_status_p, const char * const ifname);

// ifs_name - server side interface name
// ifc_ip - client side ip address
void captiveportal(const char * const ifs_name,
                   const char * const ifc_ip);
    

#endif
