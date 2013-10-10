/* ssi_usrman.h -- header of ssi_usrman.c
 * by bowen guo, 2008-11-24 */

#ifndef SSI_USRMAN_H
#define SSI_USRMAN_H

#include "shttpd.h"

#define LINE_LEN 256            /* fine line buff len */
#define TOKEN ':'
typedef struct html_ssi_struct{
	const char * const html_para;	/* html passed parameter */
	const char * const ssi_para;		/* ssi varible */
}html_ssi_t;

// get current CGI environment varible, such as
// REMOTE_USER, RMOTE_ADDRESS, ...
// if exist, shttpd_printf its value
// else shttpd_printf "NULL" or "Unknow Command"
void ssi_get_env(struct shttpd_arg *arg);

// get user info, such as usename, password ...
// if exist, shttpd_printf its valuse
// else shttpd_printf "NULL" or "Unknow Command"
void ssi_get_userinfo(struct shttpd_arg *arg);    
#endif
					
