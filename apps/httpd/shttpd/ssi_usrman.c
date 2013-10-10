/* ssi_usrman.c -- use to extend shttpd functions.
 * deal with user management 
 * by bowen guo, 2008-11-25 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "ssi_usrman.h"
#include "config.h"
#include "shttpd.h"
#include "webuserman.h"

//#define _DEBUG
#include "debug.h"

#ifdef CTC_E8_PAGES
#define ADMIN "telecomadmin"           /* please keep same with userman.c in cgi dir.maybe we can consider make a symbol link with ln? */
#endif
#ifdef MULITYLANG_PAGES
#define ADMIN "Atheros_Router_Admin"           
#define USER "Atheros_Router_User"           
#endif
#define HTPASSWDPATH "/tmp/.htpasswd"
#define SESSIONFILE "/tmp/sessioninfo.ses"

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

#ifdef CTC_E8_PAGES
void ssi_get_userinfo(struct shttpd_arg *arg)
{
    FILE *fp = NULL;

    if(strstr(arg -> in.buf, "curUserName") != NULL)
    {
        int fd;
        char buffer[4096];
    
        if((fd = open(SESSIONFILE, O_RDONLY)) < 0)
            goto err;
        memset(buffer, 0, sizeof buffer);
        if(read(fd, buffer, sizeof buffer) < 0)
            goto err;

        struct loginInfo *LoginInfo = (struct loginInfo *)buffer;
        struct loginOneItem *FirstItem = (struct loginOneItem *)LoginInfo->data;
        struct loginOneItem *Item = NULL;

        if(LoginInfo->ItemCount <= 0)
            goto err;

        Item = FirstItem + LoginInfo->ItemCount - 1;
        shttpd_printf(arg, "%s", Item->UserName);
        close(fd);
        goto out;
err:
        close(fd);
        shttpd_printf(arg, "%s", NULL);
        goto out;

#if 0
#define CGI_VERIFY
#if defined(DIGEST_VERIFY)
        const char * const cur_user = 
            shttpd_get_env(arg, "REMOTE_USER");
        DBG((stderr, "===> cur_user_digest = %s\n", cur_user));
        shttpd_printf(arg, "%s", cur_user == NULL ? "NULL" : cur_user);

        goto out;
#elif defined(CGI_VERIFY)
        DBG((stderr, "===> Enter %s", __FUNCTION__));
        fp = fopen(SESSIONFILE, "r");
        if(fp == NULL)
        {
            DBG((stderr, "===> Open file %s error: %s", SESSIONFILE, strerror(errno)));
            shttpd_printf(arg, "%s", NULL);
            goto out;
        }

        char line[LINE_LEN];
        char value[LINE_LEN];
        char tmp[FILENAME_MAX];

        while(fgets(line, LINE_LEN, fp) != NULL)
        {
            if(strstr(line, "#"))/* remark line */
                continue;

            if(strstr(line,  shttpd_get_env(arg, "REMOTE_ADDR")) != NULL)
            {
                strsplit(line, value, LINE_LEN, ':', 1);
                DBG((stderr, "===> cur_user_cgi = %s\n", value));
                shttpd_printf(arg, "%s", value);
                goto out;
            }
        }
        shttpd_printf(arg, "%s", NULL);
        goto out;
#endif
#endif
    }

    if(strstr(arg -> in.buf, "sysUserName") != NULL)
    {
        shttpd_printf(arg, "%s", ADMIN);
        goto out;
    }

    if(strstr(arg -> in.buf, "usrUserName") != NULL)
    {
        char line[LINE_LEN];
        const char * prev;
        const char * next;
        
        if((fp = fopen(HTPASSWDPATH, "r")) == NULL)
        {
            DBG((stderr, "===> File open error: %s", strerror(errno)));
            shttpd_printf(arg, "get user name error");
            goto out;
        }

        while(fgets(line, LINE_LEN, fp) != NULL)
        {
	    char compare[strlen(ADMIN) + 2];
	    sprintf(compare, "%s:", ADMIN);
            if(strstr(line, compare) != NULL) /* admin user */
                continue;
            else
            {
                prev = line;
                next = strchr(line, TOKEN);
                
                if(next != prev)
                {
                    char tmp[next - prev + 1];
                    memset(tmp, '\0', next -prev + 1);
                    memcpy(tmp, prev, next - prev);
                    DBG((stderr, "===> User name = %s", tmp));
                    shttpd_printf(arg, "%s", tmp);
                    
                    goto out;
                    }
                shttpd_printf(arg, "Password file format error");
                goto out;
            }   
        }
        shttpd_printf(arg, "NULL"); /* Not enough user */
        goto out;
    }
    
    shttpd_printf(arg, "Unknow Command\n");
  out:
    arg->flags |= SHTTPD_END_OF_OUTPUT;
    if(fp != NULL)
        fclose(fp);
    return;
    
}
#endif

#ifdef MULITYLANG_PAGES
void ssi_get_userinfo(struct shttpd_arg *arg)
{
    FILE *fp = NULL;

    if(strstr(arg -> in.buf, "curUserName") != NULL)
    {
        int fd;
        char buffer[4096];
    
        if((fd = open(SESSIONFILE, O_RDONLY)) < 0)
            goto err;
        memset(buffer, 0, sizeof buffer);
        if(read(fd, buffer, sizeof buffer) < 0)
            goto err;

        struct loginInfo *LoginInfo = (struct loginInfo *)buffer;
        struct loginOneItem *FirstItem = (struct loginOneItem *)LoginInfo->data;
        struct loginOneItem *Item = NULL;

        if(LoginInfo->ItemCount <= 0)
            goto err;

        Item = FirstItem + LoginInfo->ItemCount - 1;
        shttpd_printf(arg, "%s", Item->UserName);
        close(fd);
        goto out;
err:
        close(fd);
        shttpd_printf(arg, "%s", NULL);
        goto out;

    }

    if(strstr(arg -> in.buf, "sysUserName") != NULL)
    {
        char line[LINE_LEN];
        const char * prev;
        const char * next;
        
        if((fp = fopen(HTPASSWDPATH, "r")) == NULL)
        {
            DBG((stderr, "===> File open error: %s", strerror(errno)));
            shttpd_printf(arg, "get user name error");
            goto out;
        }

        char compare[strlen(ADMIN) + 2];
        sprintf(compare, "%s:", ADMIN);
        
        while(fgets(line, LINE_LEN, fp) != NULL)
        {
            if(strstr(line, compare) == NULL) /* admin user */
                continue;
            else
            {
                prev = line;
                next = strchr(line, TOKEN);
                
                if(next != prev)
                {
                    char tmp[next - prev + 1];
                    memset(tmp, '\0', next -prev + 1);
                    memcpy(tmp, prev, next - prev);
                    DBG((stderr, "===> User name = %s", tmp));
                    shttpd_printf(arg, "%s", tmp);
                    
                    goto out;
                    }
                shttpd_printf(arg, "Password file format error");
                goto out;
            }   
        }
        shttpd_printf(arg, "NULL"); /* Not enough user */
        goto out;
    }

    if(strstr(arg -> in.buf, "usrUserName") != NULL)
    {
        char line[LINE_LEN];
        const char * prev;
        const char * next;
        
        if((fp = fopen(HTPASSWDPATH, "r")) == NULL)
        {
            DBG((stderr, "===> File open error: %s", strerror(errno)));
            shttpd_printf(arg, "get user name error");
            goto out;
        }

        char compare[strlen(ADMIN) + 2];
        sprintf(compare, "%s:", USER);
        
        while(fgets(line, LINE_LEN, fp) != NULL)
        {
            if(strstr(line, compare) == NULL) /* admin user */
                continue;
            else
            {
                prev = line;
                next = strchr(line, TOKEN);
                
                if(next != prev)
                {
                    char tmp[next - prev + 1];
                    memset(tmp, '\0', next -prev + 1);
                    memcpy(tmp, prev, next - prev);
                    DBG((stderr, "===> User name = %s", tmp));
                    shttpd_printf(arg, "%s", tmp);
                    
                    goto out;
                    }
                shttpd_printf(arg, "Password file format error");
                goto out;
            }   
        }
        shttpd_printf(arg, "NULL"); /* Not enough user */
        goto out;
    }

    shttpd_printf(arg, "Unknow Command\n");
  out:
    arg->flags |= SHTTPD_END_OF_OUTPUT;
    if(fp != NULL)
        fclose(fp);
    return;
    
}
#endif
