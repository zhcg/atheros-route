/* ssi_langout.h -- header of ssi_langout.c
 * by bowen guo, 2009-07-08 */

#ifndef SSI_LANGOUT_H
#define SSI_LANGOUT_H

#include "shttpd.h"

#ifndef BUFFLEN
#define BUFFLEN 1024
#endif

#define FILENAMEVAR "[FILENAME]"
#define LANGRESFILE "language.res"
#define RESCOUNT 200

typedef struct msgidx_struct{
	char filename[BUFFLEN/16];	/* filename length: LINE_LEN/16 - 1 */
	unsigned int idxbegin;		/* index begin filename */
    unsigned int idxend;        /* index end file name */
}msgidx_t;

// filename -- resource filename
// msgidx_t -- start msgidx array address
// msgidxcnt -- msgidx_t count
// return 0 -- successful
// return -1 -- failed.
int shttpdext_create_langidx(const char * const filename,
                           msgidx_t * msgidx, unsigned int msgidxcnt);
    
void ssi_get_language(struct shttpd_arg *arg);
#endif
					
