/* webuserman.h -- header of webuserman.c
 * by bowen guo, 2009-02-20 */

#ifndef WEBUSERMAN_H
#define WEBUSERMAN_H
#include "defs.h"
/* find specified record, if timeout, delete it, else refresh time to now.                                                                                  
 * and if not find, not any change */
int shttpdext_timeoutverify(const char * const path,
                            const char * const remote_addr,
                            int timeval);

// verify user, if existed, return 0, else return -1
int shttpdext_userverify(const char * const path, const char * const remote_addr);

// redirect specified connect web page to other. redirect - redirect web                                                                                    
// page name
void shttpdext_redirect(struct conn *c, int status, const char * redirect);

struct loginOneItem {
time_t Tm               __attribute__ ((packed));
int Count               __attribute__ ((packed));
char UserName[32]       __attribute__ ((packed));
char Ip[32]             __attribute__ ((packed));
};

struct loginInfo {
int ItemCount           __attribute__ ((packed));
int LoginItem           __attribute__ ((packed));
char data[1]            __attribute__ ((packed));
};

int locateItem(struct loginInfo *Info, char Ip[32]);
#endif
