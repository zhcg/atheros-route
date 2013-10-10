/* shttpdext.c -- use to extend shttpd functions.
 * by bowen guo, 2008-09-18 */

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <time.h>
#include <syslog.h>
#include "defs.h"
#include "shttpdext.h"
#include "ssi_langout.h"
#include "ssi_modcntrl.h"

msgidx_t msgidx[RESCOUNT];

// add by bowenguo, 2008-11-21
//#include "ssi_usrman.h"

//#include "ssi_langout.h"
//#include "ssi_modcntrl.h"

#define EXTHTPASSWD "/tmp/.htpasswd"
#define REALMADMIN "Atheros_Router_Admin" /* please keep same with config.h in shttpd dir
                                           * maybe we can consider make a symbol link with ln? */
#define REALMUSER "Atheros_Router_User" /* please keep same with config.h in shttpd dir
                                         * maybe we can consider make a symbol link with ln? */


void printfha(char * ha,int n)
{
    int i;
    for ( i = 0; i < n; i++)
    {
        fprintf(stderr,"%c",ha[i]);
    }
    fprintf(stderr,"\n");
}
static int _autherize(const char * UserName, const char * Password,const char *Domain)
{
    struct vec  user, domain, vha1;
    char        line[256];
    struct vec u, d, p;
    char  ha1[32]; 
    FILE * fp =  fopen(EXTHTPASSWD,"r");
    u.ptr = UserName;
    u.len = strlen(UserName);
    d.ptr = Domain;
    d.len = strlen(Domain);
    p.ptr = Password;
    p.len = strlen(Password);

    if(fp == NULL )
        return 1;
    md5(ha1, &u, &d, &p, NULL);

    while(fgets(line, sizeof(line), fp) != NULL)
    {
        if(!parse_htpasswd_line(line, &user, &domain, &vha1))
            continue;
        if(memcmp(user.ptr,UserName,user.len) == 0)
        {
    //        printfha(vha1.ptr,vha1.len);
    //        printfha(ha1,sizeof(ha1));
            
            if(memcmp((void*)vha1.ptr,ha1,sizeof(ha1)) == 0)
            {
                return 1;
            }
            else
            {
                return 0;
            }
        }
    }
    return 0;    
}
static void _Redirect(struct shttpd_arg * arg,const char * uri)
{
    shttpd_printf(arg, "HTTP/1.1 307 Temporary Redirect\r\n"
            "Location: %s\r\n\r\n",uri);
}
typedef struct _User{
#ifdef IPV6_ENABLE
    char RemoteIP[64];
#else
    char RemoteIP[20];
#endif //IPV6_ENABLE
    char UserName[64];
    int  UserType;
    time_t VisitTime;
    int TryTime;
}LoginUser;

LoginUser CurrentUser={};

void login_page(struct shttpd_arg * arg)
{
    char Name[32];
    char Passwd[32];
    const char * QueryString = shttpd_get_env(arg, "QUERY_STRING");
#ifdef IPV6_ENABLE    
    char * RemoteIP = NULL;
#else
    const char * RemoteIP;
#endif //IPV6_ENABLE

    time_t now = time(NULL);

    fprintf(stderr, "current time:%d\n",(int)now); 
#ifdef IPV6_ENABLE
    RemoteIP = (char *) shttpd_get_env(arg, "REMOTE_ADDR");
#else
    RemoteIP = shttpd_get_env(arg, "REMOTE_ADDR");
#endif
    if(strlen(CurrentUser.RemoteIP) != 0 && strcmp(RemoteIP, CurrentUser.RemoteIP) != 0
            && (now - CurrentUser.VisitTime) < 300)
    {
       _Redirect(arg, ALREADYERR);
    }else if(QueryString 
            && (shttpd_get_var("username", QueryString, strlen(QueryString), Name,sizeof(Name)) != -1)
            &&(shttpd_get_var("password", QueryString, strlen(QueryString),Passwd,sizeof(Passwd)) != -1))
    {
        if(_autherize(Name,Passwd,REALMADMIN) == 1)
        {
            CurrentUser.UserType = ADMIN_TYPE;
        }
        else if(_autherize(Name,Passwd,REALMUSER) == 1)
        {
            CurrentUser.UserType = USER_TYPE;
        }
        else
        {
            if(strcmp(RemoteIP,CurrentUser.RemoteIP) == 0)
            {
                CurrentUser.TryTime++;
            }
            else
            {
                CurrentUser.TryTime = 1;
            }
            if(CurrentUser.TryTime >2)
            {
                CurrentUser.TryTime = 0;
                _Redirect(arg,TRYMAXERR);
            }
            else
            {
                _Redirect(arg,ACCOUNTERR);
            }
            goto LoginEnd;
        }
        strncpy(CurrentUser.RemoteIP,RemoteIP,sizeof(CurrentUser.RemoteIP));
        strncpy(CurrentUser.UserName,Name,sizeof(CurrentUser.UserName));
        CurrentUser.VisitTime = now;
        char message[128];
        snprintf(message, sizeof message, "%s login as %s", RemoteIP, Name);
        openlog("web", 0, LOG_DAEMON );
        syslog(LOG_INFO, message);
        closelog();

        _Redirect(arg, "/");
    }
    else
    {
        _Redirect(arg,LOGINPAGE);
    }
LoginEnd:
    arg->flags |= SHTTPD_END_OF_OUTPUT;
#ifdef IPV6_ENABLE
    if(isIpv6)
    {
        if(RemoteIP != NULL)
            free(RemoteIP);
    }
#endif //IPV6_ENABLE
    return;
}

#ifdef IPV6_ENABLE
int check_authorize(struct conn *c)
{
    char RemoteIP[64];
    char * temp;
    
    if(strstr(c->uri, LOGINREQ) != NULL
            || (strstr(c->uri, LOGINPIC1) != NULL)
            || (strstr(c->uri, LOGINPIC2) != NULL)
            || (strstr(c->uri, LOGINPAGE) != NULL)
            || (c->query && strstr(c->query, "action=captiveportal") != NULL)
            ||(strstr(c->uri, LOGOUTREQ) != NULL))
    {
        return 1;
    }
    else
    {
        time_t now = time(NULL);
        if (isIpv6)
        {
            inet_ntop(AF_INET6, &c->sa.u.sin6.sin6_addr, RemoteIP, sizeof(RemoteIP));
            if(strcmp(RemoteIP, CurrentUser.RemoteIP) == 0
                && (now - CurrentUser.VisitTime) < 300)
            {
                CurrentUser.VisitTime = now;
                return 1;
            }            
        }
        else
        {
            temp = inet_ntoa(c->sa.u.sin.sin_addr);
            if(strcmp(temp, CurrentUser.RemoteIP) == 0
                && (now - CurrentUser.VisitTime) < 300)
            {
                CurrentUser.VisitTime = now;
                return 1;
            }            
        }
    }
    return 0;
}
#else
int check_authorize(struct conn *c)
{

    if(strstr(c->uri, LOGINREQ) != NULL
            || (strstr(c->uri, LOGINPIC1) != NULL)
            || (strstr(c->uri, LOGINPIC2) != NULL)
            || (strstr(c->uri, LOGINPAGE) != NULL)
            || (c->query && strstr(c->query, "action=captiveportal") != NULL)
            ||(strstr(c->uri, LOGOUTREQ) != NULL))
    {
        return 1;
    }
    else
    {
        time_t now = time(NULL);
        char *RemoteIP = inet_ntoa(c->sa.u.sin.sin_addr);
        if(strcmp(RemoteIP, CurrentUser.RemoteIP) == 0
            && (now - CurrentUser.VisitTime) < 300)
        {
            CurrentUser.VisitTime = now;
            return 1;
        }
    }
    return 0;
}
#endif //IPV6_ENABLE

int CurretUserType()
{
    return CurrentUser.UserType;
}
void logout_page(struct shttpd_arg * arg)
{
    char message[128];
    snprintf(message, sizeof message, "%s logout as %s", CurrentUser.RemoteIP, CurrentUser.UserName);
    openlog("web", 0, LOG_DAEMON );
    syslog(LOG_INFO, message);
    closelog();
    memset(&CurrentUser, 0,sizeof(CurrentUser));
    _Redirect(arg,LOGINPAGE);
    arg->flags |= SHTTPD_END_OF_OUTPUT;
    return;
}
void getUserInfo(struct shttpd_arg * arg)
{
    if(strstr(arg -> in.buf, "curUserName") != NULL)
    {
        shttpd_printf(arg, "%d",CurrentUser.UserType);
    }
    else if(strstr(arg -> in.buf, "sysUserName") != NULL)
    {
        shttpd_printf(arg, "%d",ADMIN_TYPE);
    }
    else if(strstr(arg -> in.buf, "usrUserName") != NULL)
    {
        shttpd_printf(arg, "%s",CurrentUser.UserName);
    }
    arg->flags |= SHTTPD_END_OF_OUTPUT;
    return;
}
void shttpdext_init(struct shttpd_ctx   *ctx)
{
    // Add by bowenguo, 2007-11-25
    //Modified by Ken.
    static char pathres[BUFFLEN];
    char pathmod[BUFFLEN];
    sprintf(pathres, "%s/%s", ctx->options[OPT_ROOT], LANGRESFILE);
    shttpdext_create_langidx(pathres, msgidx, RESCOUNT);
    shttpd_register_ssi_func(ctx, "get_language", ssi_get_language, pathres);
    
    sprintf(pathmod, "%s/%s", ctx->options[OPT_ROOT], MODCNTRLFILE);
    shttpdext_create_modidx(pathmod);
    shttpd_register_ssi_func(ctx, "get_module", ssi_get_module, pathmod);
    
    shttpd_register_ssi_func(ctx, "get_userinfo", getUserInfo, NULL);
    shttpd_register_uri(ctx,"/login", &login_page,NULL);
    shttpd_register_uri(ctx,"/logout", &logout_page,NULL);
    return;
}
  
