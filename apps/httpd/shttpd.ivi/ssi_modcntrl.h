/* ssi_modcntrl.h -- header of ssi_modcntrl.c
 * by bowen guo, 2009-07-15 */

#ifndef SSI_MODCNTRL_H
#define SSI_MODCNTRL_H

#include "shttpd.h"

#define MODULENAMEVAR "[MODULENAME]"

#ifndef BUFFLEN
#define BUFFLEN 1024
#endif

#define MODCNTRLFILE "modcntrl.cfg"
#define MODCOUNT 10

typedef struct modidx_struct{
	char filename[BUFFLEN/16];	/* filename length: BUFFLEN/16 - 1 */
	unsigned int idxbegin;		/* index begin filename */
    unsigned int idxend;        /* index end file name */
}modidx_t;

// filename -- module contrl filename
// modidx_t -- start modidx array address
// modidxcnt -- modidx_t count
// return 0 -- successful
// return -1 -- failed.
int shttpdext_create_modidx(const char * const filename,
                           modidx_t * modidx, unsigned int modidxcnt);
    
void ssi_get_module(struct shttpd_arg *arg);
#endif
					
