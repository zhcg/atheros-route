/*
 * Copyright (c) 2004-2005 Sergey Lyubka <valenok@gmail.com>
 * All rights reserved
 *
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Sergey Lyubka wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.
 */
#include "captiveportal.h"

#include "defs.h"
#include "shttpdext.h"
#include "ssi_langout.h"
#include "ssi_modcntrl.h"

msgidx_t msgidx[RESCOUNT];
modidx_t modidx[MODCOUNT];

static int	exit_flag;	/* Program termination flag	*/

static void
signal_handler(int sig_num)
{
	switch (sig_num) {
#ifndef _WIN32
	case SIGCHLD:
		while (waitpid(-1, &sig_num, WNOHANG) > 0) ;
		break;
#endif /* !_WIN32 */
	default:
		exit_flag = sig_num;
		break;
	}
}

struct shttpd_ctx	*ctx;

/* successfully return 0, else -1, and strsplitted will be null string */
static int strsplit(const char * const strorgin, /* need to split string */
                    char * strsplitted,          /* splitted string */
                    unsigned int length,         /* strsplit buffer length */
                    int token,                   /* split token  */
                    unsigned int index)          /* get substring index */
{
    int i = 0;
    char * prev, * next;
    prev = (char *)strorgin;
    next = (char *)strorgin;

    memset(strsplitted, '\0', length);

    if(strorgin == NULL)
        return -1;

    for(; i < index - 1; i++)
    {
        prev = strchr(prev, token);
        if(prev == NULL)
        {
            if(index == 1)
                prev = (char *)strorgin;
            else
                return -1;
        }
        else
            while(*(++prev) == token);
    }

    if((next = strchr(prev, token)) != NULL)
    {
        if(length < next - prev + 1) /* buffer length not enough */
            return -1;
        memcpy(strsplitted, prev, next - prev);
        DBG((stderr, "===> strsplitted = %s", strsplitted));
        return 0;
    }
    else
    {
        if(length < strlen(prev) + 1) /* buffer length not enough */
            return -1;
        strcpy(strsplitted, prev);
        DBG((stderr, "===> strsplitted = %s", strsplitted));
        return 0;
    }
}

int
main(int argc, char *argv[])
{
    printf("This daemon go ....!");
#if !defined(NO_AUTH)
	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'A') {
		if (argc != 6)
			_shttpd_usage(argv[0]);
		exit(_shttpd_edit_passwords(argv[2],argv[3],argv[4],argv[5]));
	}
#endif /* NO_AUTH */

	if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")))
		_shttpd_usage(argv[0]);

#if defined(_WIN32)
	try_to_run_as_nt_service();
#endif /* _WIN32 */

#ifndef _WIN32
	(void) signal(SIGCHLD, signal_handler);
	(void) signal(SIGPIPE, SIG_IGN);
#endif /* _WIN32 */

	(void) signal(SIGTERM, signal_handler);
	(void) signal(SIGINT, signal_handler);

	if ((ctx = shttpd_init(argc, argv)) == NULL)
		_shttpd_elog(E_FATAL, NULL, "%s",
		    "Cannot initialize SHTTPD context");

	_shttpd_elog(E_LOG, NULL, "shttpd %s started on port(s) %s, serving %s",
	    VERSION, ctx->options[OPT_PORTS], ctx->options[OPT_ROOT]);

        /* add by bowen for extend cgi function */
    char pathres[BUFFLEN];
    char pathmod[BUFFLEN];
    sprintf(pathres, "%s/%s", ctx->options[OPT_ROOT], LANGRESFILE);
    shttpdext_create_langidx(pathres, msgidx, RESCOUNT);
    shttpd_register_ssi_func(ctx, "get_language", ssi_get_language, pathres);
    
    sprintf(pathmod, "%s/%s", ctx->options[OPT_ROOT], MODCNTRLFILE);
    shttpdext_create_modidx(pathmod, modidx, MODCOUNT);
    shttpd_register_ssi_func(ctx, "get_module", ssi_get_module, pathmod);

        /* Change index page to the first html in modcntr.cfg */
    if(strlen(modidx[0].filename) != 0)
    {
        FILE *fp = NULL;
        char line[BUFFLEN];
        char label[BUFFLEN];
        
        if((fp = fopen(pathmod, "r")) != NULL)
        {
            fseek(fp, modidx[0].idxend, SEEK_SET);
            while((fgets(line, BUFFLEN, fp) != NULL) && (ftell(fp) <= modidx[1].idxbegin))
            {
                if(strstr(line, "#") != NULL)
                    continue;
                
                strsplit(line, label, BUFFLEN, ' ', 1);
                break;
            }
            
            free(ctx->options[OPT_INDEX_FILES]);
            ctx->options[OPT_INDEX_FILES] = _shttpd_strdup(label);
            fprintf(stderr, "====> lable = %s\n", label);
            fclose(fp);
                
        }        
    }
    
    shttpdext_init(NULL);

	while (exit_flag == 0)
		shttpd_poll(ctx, 10 * 1000);

	_shttpd_elog(E_LOG, NULL, "Exit on signal %d", exit_flag);
	shttpd_fini(ctx);

	return (EXIT_SUCCESS);
}
