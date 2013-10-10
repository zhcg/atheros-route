/* shttpdext.c -- use to extend shttpd functions.
 * by bowen guo, 2008-09-18 */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "shttpdext.h"

// add by bowenguo, 2008-11-21
#include "ssi_usrman.h"

#include "ssi_langout.h"
#include "ssi_modcntrl.h"
#include "shttpd.h"

extern struct shttpd_ctx   *ctx;

void shttpdext_init(void * para)
{
        // Add by bowenguo, 2007-11-25
    shttpd_register_ssi_func(ctx, "get_userinfo", ssi_get_userinfo, NULL);
    
    return;
}
  
