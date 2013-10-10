/* ssi_modcntrl.h -- header of ssi_modcntrl.c
 * by bowen guo, 2009-07-15 */

#ifndef SSI_MODCNTRL_H
#define SSI_MODCNTRL_H

#include "shttpd.h"

#define MODULENAMEVAR "[MODULENAME]"

#ifndef BUFFLEN
#define BUFFLEN 1024
#endif
#define SECTIONNAMELEN 32
#define MODCNTRLFILE "modcntrl.cfg"
#define MODCOUNT 10


// filename -- module contrl filename
// modidx_t -- start modidx array address
// modidxcnt -- modidx_t count
// return 0 -- successful
// return -1 -- failed.
int shttpdext_create_modidx(const char * const filename);
    
void ssi_get_module(struct shttpd_arg *arg);
#endif
					
