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
#include "defs.h"
#include "shttpd.h"
#include "shttpdext.h"
#include "ssi_modcntrl.h"
//#define _DEBUG
#include "debug.h"

extern int errno;
#define MaxUrlLen 64
typedef struct _UrlAcl {
    char * Url;
    char IsProtected;
}UrlAcl;
UrlAcl UrlAclList[MaxUrlLen] = {/*NULL*/};
typedef struct Mod_Struct{
    char RefUrl[MaxUrlLen];
    char SectionName[MaxUrlLen];
    int UrlNumber;
    int UrlIndexBegin;
}Mod_t;

Mod_t ModList[MODCOUNT];
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
        return 0;
    }
    else
    {
        if(length < strlen(prev) + 1) /* buffer length not enough */
            return -1;
        strcpy(strsplitted, prev);
        return 0;
    }
}

int shttpdext_create_modidx(const char * const filename)
{
    FILE *fp = NULL;
    char line[BUFFLEN];
    int Mods = 0;
    int Items = 0;
    int SecItems = 0;
    char SecBuf[MaxUrlLen];
    char *sRef;
    char ItemBuf[MaxUrlLen];
    char *iRef;
    int len;

    memset(ModList, 0, sizeof(Mod_t) * MODCOUNT);
    if((fp = fopen(filename, "r")) == NULL)
    {
        DBG((stderr, "===> File open error: %s:%s", filename, strerror(errno)));
        return -1;
    }

    while((fgets(line, BUFFLEN, fp) != NULL) && (Mods < MODCOUNT))
    {
        if(strstr(line, "#") != NULL)
            continue;

        strsplit(line, ItemBuf, sizeof(ItemBuf), '=', 1);
        iRef = ItemBuf;
        while(iRef[0] == ' ')
        {
            iRef++;
        }
        len = strlen(iRef);
        while(len > 0 
                && ((iRef[len-1] == ' ')
                    ||(iRef[len-1] == '\r')
                    ||(iRef[len-1] == '\n')))
        {
            iRef[len-1] = '\0';
            len--;
        }
        if(strlen(iRef) == 0)
            continue;

        strsplit(line, SecBuf, sizeof(SecBuf), '\'', 2);
        sRef = SecBuf;
        while(sRef[0] == ' ')
        {
            sRef++;
        }
        len = strlen(sRef);
        while(len > 0 && sRef[len-1] == ' ')
        {
            sRef[len-1] = '\0';
            len--;
        }
        /*This means that this is a ModName when it has ref url.*/
        if(strlen(sRef) > 6 && strstr(sRef,".shtml") != NULL)
        {
            if(Mods > 0)
            {
                DBG((stderr, "MODS[%d]: UrlNumer:%d", Mods-1,SecItems));
                ModList[Mods-1].UrlNumber = SecItems;
            }
            strcpy(ModList[Mods].SectionName,iRef);
            strcpy(ModList[Mods].RefUrl, sRef);
            ModList[Mods].UrlIndexBegin = Items;
            DBG((stderr, "MODS[%d]:SectionName:%s", Mods,ModList[Mods].SectionName));
            DBG((stderr, "MODS[%d]:RefUrl:%s", Mods,ModList[Mods].RefUrl));
            DBG((stderr, "MODS[%d]:BeginItem:%d", Mods,ModList[Mods].UrlIndexBegin));
            SecItems= 0;
            Mods++;
        }
        else
        {
            UrlAclList[Items].Url = strdup(iRef);
            if(strcmp(sRef,"protected") == 0)
            {
                UrlAclList[Items].IsProtected = 1;
            }
            DBG((stderr, "UrlAclList[%d]:%s Protected:%s##", 
                        Items,UrlAclList[Items].Url,
                    UrlAclList[Items].IsProtected?"Yes":"No"));
            SecItems++;
            Items++;
        }
    }

    if(Mods > 0)
    {
        DBG((stderr, "MODS[%d]: UrlNumer:%d", Mods-1,SecItems));
        ModList[Mods-1].UrlNumber = SecItems;
    }
    fclose(fp);
    return 0;
}

#if 1
void ssi_get_module(struct shttpd_arg *arg)
{

    int Mods = 0;
    int len,i;

    char ModName[BUFFLEN/16];
    char * ModRef;
    char ItemName[BUFFLEN/16];
    char * ItemRef;

    strsplit(arg -> in.buf, ModName, BUFFLEN/16, ' ', 1);
    strsplit(arg -> in.buf, ItemName, BUFFLEN/16, ' ', 2);

    ModRef = ModName;
    while(ModRef[0] == ' ')
    {
        ModRef++;
    }
    len = strlen(ModRef);
    while(len > 0  && ModRef[len-1] == ' ')
    {
        ModRef[len-1] = '\0';
        len--;
    }

    ItemRef = ItemName;
    while(ItemRef[0] == ' ')
    {
        ItemRef++;
    }
    len = strlen(ItemRef);
    while(len > 0  && ItemRef[len-1] == ' ')
    {
        ItemRef[len-1] = '\0';
        len--;
    }
    DBG((stderr,"ModName:%s ItemName:%s",ModRef,ItemRef));
    while(strlen(ModList[Mods].SectionName) != 0)
    {
        if(strcasecmp(ModList[Mods].SectionName, ModRef) ==0)
            break;
        Mods++;
    }

    if(strlen(ModList[Mods].SectionName) == 0)
    {
        DBG((stderr, "===> Not found module, module not display!"));
        shttpd_printf(arg, "style='display:none'");
    }else if(strlen(ItemName) == 0)
    {
        if(CurretUserType() == USER_TYPE)
        {
            DBG((stderr, "It is a Common User!"));
            for(i= 0; i < ModList[Mods].UrlNumber; i++)
            {
                if(UrlAclList[ModList[Mods].UrlIndexBegin + i].IsProtected == 0)
                    break;
            }
            if(i >= ModList[Mods].UrlNumber)
            {
                shttpd_printf(arg, "style='display:none'");
            }
        }
            /* module */
    }
    else if(strcasecmp("ModIndex", ItemRef) == 0) /* Module index page */
    {
        if(CurretUserType() == USER_TYPE)
        {
            for(i= 0; i < ModList[Mods].UrlNumber; i++)
            {
                if(UrlAclList[ModList[Mods].UrlIndexBegin + i].IsProtected == 0)
                {
                    shttpd_printf(arg, "%s", UrlAclList[ModList[Mods].UrlIndexBegin + i].Url);
                    break;
                }
            }
        }
        else
        {
            shttpd_printf(arg, "%s", ModList[Mods].RefUrl);
        }
    }
    else{
        for(i=0;i < ModList[Mods].UrlNumber; i++)
        {
            if(strcasecmp(UrlAclList[ModList[Mods].UrlIndexBegin + i].Url,ItemRef) ==0)
                break;
        }        
        if( i >= ModList[Mods].UrlNumber)
        {
            shttpd_printf(arg, "style='display:none'");
        }
        else
        {
            if(CurretUserType() == USER_TYPE && UrlAclList[ModList[Mods].UrlIndexBegin + i].IsProtected)
            {
                shttpd_printf(arg, "style='display:none'");
            }
        }
    }

    arg->flags |= SHTTPD_END_OF_OUTPUT;
    return ;
}

#endif
