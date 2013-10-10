/* ssi_langout.c -- use to extend shttpd functions.
 * deal with language output 
 * by bowen guo, 2009-07-08 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ssi_modcntrl.h"
#include "shttpd.h"

#define _DEBUG
#include "debug.h"

extern int errno;

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

int shttpdext_create_modidx(const char * const filename,
                           modidx_t * modidx, unsigned int modidxcnt)
{
    FILE *fp = NULL;
    char line[BUFFLEN];
    int count = 0;

    memset(modidx, 0, sizeof(modidx_t) * modidxcnt);
    
    if((fp = fopen(filename, "r")) == NULL)
    {
        DBG((stderr, "===> File open error: %s:%s", filename, strerror(errno)));
        return -1;
    }

    while((fgets(line, BUFFLEN, fp) != NULL) && (count < modidxcnt))
    {
        if(strstr(line, "#") != NULL)
            continue;
        
        if(strstr(line, MODULENAMEVAR) == NULL) 
            continue;
        
        modidx[count].idxend = ftell(fp);
        modidx[count].idxbegin = modidx[count].idxend - strlen(line);
        strsplit(line, modidx[count].filename, BUFFLEN/16, '\'', 2);
        fseek(fp, modidx[count].idxbegin, SEEK_SET);
        DBG((stderr, "===> File read before: %s", fgets(line, BUFFLEN, fp)));
        DBG((stderr, "===> File read postion: %d, %d, %s",  modidx[count].idxbegin,
             modidx[count].idxend, modidx[count].filename));
        fseek(fp, modidx[count].idxend, SEEK_SET);
        count++;
    }

    fseek(fp, 0L, SEEK_END);
    modidx[count].idxbegin = ftell(fp);
    modidx[count].filename[0] = '\0';
    
    fclose(fp);
    return ;
}

#if 1
void ssi_get_module(struct shttpd_arg *arg)
{
    extern modidx_t modidx[];

    FILE *fp = NULL;
    char line[BUFFLEN];
    char label[BUFFLEN];
    int count = 0;

    char section[BUFFLEN/16];
    char varible[BUFFLEN/16];

    strsplit(arg -> in.buf, section, BUFFLEN/16, ' ', 1);
    strsplit(arg -> in.buf, varible, BUFFLEN/16, ' ', 2);

    while(strlen(modidx[count].filename) != 0)
    {
        if(strstr(modidx[count].filename, section) != NULL)
            break;
        count++;
    }

    if(strlen(modidx[count].filename) == 0)
    {
        DBG((stderr, "===> Not found module, module not display!"));
        shttpd_printf(arg, "style='display:none'");
        arg->flags |= SHTTPD_END_OF_OUTPUT;
        return;
    }

    if((fp = fopen((char *)arg -> user_data, "r")) == NULL)
    {
        DBG((stderr, "===> File open error: %s", strerror(errno)));
        arg->flags |= SHTTPD_END_OF_OUTPUT;
        return ;
    }

    fseek(fp, modidx[count].idxend, SEEK_SET);
    if(strlen(varible) == 0)
    {
            /* module */
    }
    else if(strstr("ModIndex", varible) != NULL) /* Module index page */
    {
        while((fgets(line, BUFFLEN, fp) != NULL) && (ftell(fp) <= modidx[count + 1].idxbegin))
        {
            if(strstr(line, "#") != NULL)
                continue;

            strsplit(line, label, BUFFLEN, ' ', 1);
            shttpd_printf(arg, "%s", label);
            break;
        }
        
    }
    else if(strstr("", varible) != NULL) /* Module index page */
    {
        while((fgets(line, BUFFLEN, fp) != NULL) && (ftell(fp) <= modidx[count + 1].idxbegin))
        {
            if(strstr(line, "#") != NULL)
                continue;

            strsplit(line, label, BUFFLEN, ' ', 1);
            shttpd_printf(arg, "%s", label);
            break;
        }

    }
    else{                       
        while((fgets(line, BUFFLEN, fp) != NULL) && (ftell(fp) <= modidx[count + 1].idxbegin))
        {
            if(strstr(line, "#") != NULL)
                continue;
            
            if(strstr(line, varible) != NULL)
                goto end;
        }
        shttpd_printf(arg, "style='display:none'");
    }

end:
    arg->flags |= SHTTPD_END_OF_OUTPUT;
    fclose(fp);
    return 0;
}

#endif
