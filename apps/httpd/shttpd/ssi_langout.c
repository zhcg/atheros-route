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

#include "ssi_langout.h"
#include "shttpd.h"

//#define _DEBUG
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

int shttpdext_create_langidx(const char * const filename,
                           msgidx_t * msgidx, unsigned int msgidxcnt)
{
    FILE *fp = NULL;
    char line[BUFFLEN];
    int count = 0;

    memset(msgidx, 0, sizeof(msgidx_t) * msgidxcnt);
    
    if((fp = fopen(filename, "r")) == NULL)
    {
        DBG((stderr, "===> File open error: %s", strerror(errno)));
        return -1;
    }

    while((fgets(line, BUFFLEN, fp) != NULL) && (count < msgidxcnt))
    {
        if(strstr(line, "#") != NULL)
            continue;
        
        if(strstr(line, FILENAMEVAR) == NULL) 
            continue;
        
        msgidx[count].idxend = ftell(fp);
        msgidx[count].idxbegin = msgidx[count].idxend - strlen(line);
        strsplit(line, msgidx[count].filename, BUFFLEN/16, '\'', 2);
        fseek(fp, msgidx[count].idxbegin, SEEK_SET);
        DBG((stderr, "===> File read before: %s", fgets(line, BUFFLEN, fp)));
        DBG((stderr, "===> File read postion: %d, %d, %s",  msgidx[count].idxbegin,
             msgidx[count].idxend, msgidx[count].filename));
        fseek(fp, msgidx[count].idxend, SEEK_SET);
        count++;
    }

    fseek(fp, 0L, SEEK_END);
    msgidx[count].idxbegin = ftell(fp);
    msgidx[count].filename[0] = '\0';
    
    fclose(fp);
    return 0;
}

#if 0                           /* just for test easy on pc */
int shttpdext_search_index(const char * const filename,
                           const msgidx_t * const msgidx, const unsigned int msgidxcnt,
                           const char * const section, const char * varible)
{
    FILE *fp = NULL;
    char line[BUFFLEN];
    char label[BUFFLEN];
    int count = 0;

    for(; count < msgidxcnt; count++)
    {
        if(strstr(msgidx[count].filename, section) != NULL)
            break;
        count++;
    }

    if(count >= msgidxcnt)
    {
        DBG((stderr, "===> Not found index, please check resource file!"));
        
        return -1;
    }
    
    if((fp = fopen(filename, "r")) == NULL)
    {
        DBG((stderr, "===> File open error: %s", strerror(errno)));
        return -1;
    }

    fseek(fp, msgidx[count].idxend, SEEK_SET);
    if(varible == NULL)
    {
        while((fgets(line, BUFFLEN, fp) != NULL) &&
              (ftell(fp) < msgidx[count + 1].idxbegin))
        {
            printf("var %s", line);
        }
    }
    else{
        while((fgets(line, BUFFLEN, fp) != NULL) && (ftell(fp) < msgidx[count + 1].idxbegin))
        {
            if(strstr(line, varible) != NULL)
            {
                strsplit(line, label, BUFFLEN, '\'', 2);
                printf("%s", label);
                break;
            }
            
        }
        
    }

    fclose(fp);
    return 0;
}
#endif

#if 1
void ssi_get_language(struct shttpd_arg *arg)
{
    extern msgidx_t msgidx[];

    FILE *fp = NULL;
    char line[BUFFLEN];
    char label[BUFFLEN];
    int count = 0;

    char section[BUFFLEN/16];
    char varible[BUFFLEN/16];

    strsplit(arg -> in.buf, section, BUFFLEN/16, ' ', 1);
    strsplit(arg -> in.buf, varible, BUFFLEN/16, ' ', 2);

    while(strlen(msgidx[count].filename) != 0)
    {
        if(strcmp(msgidx[count].filename, section) == 0)
            break;
        count++;
    }

    if(strlen(msgidx[count].filename) == 0)
    {
        DBG((stderr, "===> Not found index, please check resource file!"));
        arg->flags |= SHTTPD_END_OF_OUTPUT;
        return;
    }

    if((fp = fopen((char *)arg -> user_data, "r")) == NULL)
    {
        DBG((stderr, "===> File open error: %s", strerror(errno)));
        arg->flags |= SHTTPD_END_OF_OUTPUT;
        return ;
    }

    fseek(fp, msgidx[count].idxend, SEEK_SET);
    if(strlen(varible) == 0)
    {
        DBG((stderr, "===> fp cur position: %d", msgidx[count + 1].idxbegin));
        while((fgets(line, BUFFLEN, fp) != NULL) &&
              (ftell(fp) <= msgidx[count + 1].idxbegin))
        {
            if(strstr(line, "#") != NULL)
                continue;

            if(strstr(line, "=") != NULL)
            {
            
                DBG((stderr, "===> File cur position: %d", ftell(fp)));
                *(strrchr(line, '\'') + 1) = '\0'; /* unicode string get rid of '\n' */
                shttpd_printf(arg, "var %s;\n", line);
            }
            else
            {
                shttpd_printf(arg, "%s", line);
            }
        }
    }
    else{
        while((fgets(line, BUFFLEN, fp) != NULL) && (ftell(fp) <= msgidx[count + 1].idxbegin))
        {
            if(strstr(line, "#") != NULL)
                continue;
            
            if(strstr(line, varible) != NULL)
            {
                strsplit(line, label, BUFFLEN, '\'', 2);
                shttpd_printf(arg, "%s", label);
                break;
            }
        }
    }

    arg->flags |= SHTTPD_END_OF_OUTPUT;
    fclose(fp);
    return ;
}

#endif

#if 0                           /* just for test easy on pc */
int main()
{
    msgidx_t msgidx[100];
    shttpdext_create_index("language.res", msgidx, 100);
    shttpdext_search_index("language.res", msgidx, 100, "broadbund.shtml", "AlertInfo1");

    return 0;
}
#endif
