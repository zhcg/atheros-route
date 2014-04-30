/*****************************************************************************
**
** cgiMain.c
**
** This is a CGI function that is used to process the tagged HTML files that
** contain the environmental markers.  It also provides a "translation"
** function that will replace tagged parameters with their equivalent values.
**
** Copyright (c) 2009, Atheros Communications Inc.
**
** Permission to use, copy, modify, and/or distribute this software for any
** purpose with or without fee is hereby granted, provided that the above
** copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
** WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
** ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
** WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
** ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
** OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**
******************************************************************************/

/*
** include files
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/reboot.h>

#include <sys/file.h>

#ifdef CONFIG_NVRAM
#define NVRAM  "/dev/nvram"
#define NVRAM_OFFSET 0
#else
#ifdef CONFIG_LOCALPC
#ifdef CONFIG_DATA_AT_ROOT
#define NVRAM "/root/.configData"
#else
#define NVRAM "~/.configData"
#endif
#define NVRAM_OFFSET 0
#else
#define NVRAM  "/dev/caldata"
#define NVRAM_OFFSET (32 * 1024)
#endif
#endif

#define PARC_PATH "/etc/ath/iptables/parc"
/*
** local definitions
*****************
** Local Types
** Structure element for parameter display
*/

typedef struct {
char    *val;
char    *label;
} t_DispLable;

typedef struct {
char    Name[32];
char    Val[70];
} t_singleParam;

struct staList
{
	int id;
	char macAddr[20];
	char staDesc[80];
	char status[10];  /*on-enable-1; off-disable-0 */
	struct staList *next;
};

static struct staList *staHostList;
static int staId = 0;
#define OLD_STAFILE  "/etc/.OldStaList"
#define UDHCPD_FILE  "/var/run/udhcpd.leases"
#define STA_MAC "/etc/.staMac"
#define ARP_IP_MAC_ON_FILE "/etc/arp_ip_mac_on.conf"
#define ARP_IP_MAC_OFF_FILE "/etc/arp_ip_mac_off.conf"
#define MAX_WLAN_ELEMENT    1024

typedef struct {
int             numParams;
t_singleParam   Param[MAX_WLAN_ELEMENT];
} EnvParam_t;

/*
** Global Data
*/

extern char **environ;

/*
** Defined flags
*/

#define KEY_IS_WPA	0x00
#define KEY_IS_WEP	0x01
#define SILENT_ERROR	0x02

/*
** This bit in the sync word is used to indicate that parameters have been
** written to either cache or the flash, and that the factory default
** data has been changed
*/

#define NOT_FACTORY_DEFAULT		0x01

/*
**  Data
*/

#ifdef ATH_SINGLE_CFG
#define PRIVATE_C
#else
#define PRIVATE_C static
#endif

static  EnvParam_t      config;
static  EnvParam_t      config_sync;
static  char            rspBuff[65536];
PRIVATE_C  char         opBuff[65536];
unsigned int            parameterIndex = 0;
static  int		radioIndex = 0;
static  char            additionalParams[5][32];
static  unsigned int    numAdditionalParams = 0;
PRIVATE_C  unsigned     AbortFlag=0;
PRIVATE_C  unsigned     ModeFlag=0; // 0 = web page translation, 1 = cmdline
static  int		FactoryDefault = 0;  // indicates that parameters have been changed if zero

static FILE *errOut;

static char  *val1[100];
static char  *val2[100];
static char  *val3[100];
static char  *val4[100];
static char  *val5[100];
static char  buf[100];//wangyu add  for ath scan
int lists=0;
static int flaglist;
static int flaglist_2;

static int flaglist_route;
static char *httpreferer=NULL;

/*
** Internal Prototypes
*/

char *CFG_get_by_name(char *name,char *buff);
char *extractParam(char *str,char *Name,char *Val);
char *Execute_cmd(char *cmd,char *buffer);
char *expandOutput(char *inBuff, char *outBuff);
void htmlEncode(char *src, char *dest);
int	isKeyHex(char *key,int flags);

int CFG_set_by_name_sync(char *name,char *val);
int  CFG_set_by_name(char *name,char *val);
struct staList *getSta(struct staList *list, char *maddr);
struct staList *getSta1(struct staList *list, char *maddr);
void addSta(char *maddr, char *desc, char *status, int id);
void delSta(char *maddr);
struct staList *scan_staList(struct staList *list);
static void  Reboot_tiaozhuan(char* res,char * gopage);

void writeParametersWithSync(void);
/******************************************************************************/
/*!
**  \brief Print output to HTML or standard text
**
**  This macro will output a string based either on being in HTML mode
**  or in command line mode.  Used for Debug/Error messages that will show
**  up on the web page or on console output.
**
*/

#define modePrintf(x...) if(ModeFlag){printf(x);printf("\n");} \
                         else \
                         { printf("<p class='header2'>"); \
                           printf(x); printf("</p>"); }


/******************************************************************************/
/*!
**  This function will check if the user enter string has only numberic or nor
**
**  \param src Pointer to string
**  \return 0 if the string has only numberic character, return -1 if not
*/

static inline int isNumericOnly(char *pStr)
{
    while ( *pStr){
        if (!(*pStr >= '0' && *pStr <= '9'))
            return -1;/* non numeric */
        pStr++;
    }
    return 0;
}

char *get_webuicfg(char *name, char *value)
{
    FILE *f;
    char buff[255] = {0};
    char syncWord[5];
    char            *vPtr;

    f = fopen("/tmp/.webuicfg","a+");
    if ( !f )
    {
          modePrintf("open file error");
          exit(-1);
    }
    fread(&syncWord,4,1,f);

    while( !feof(f) )
    {
         fgets(buff,256,f);

         if( buff[0] == 0 )
            break;

         if(vPtr=strchr(buff,0x0a))
             *vPtr = 0;  // extract the line feed
         else
             break;      // No line feed, bad line.

         vPtr = strchr(buff,'=');

         if(!vPtr)
             break;
         else
             *vPtr++ = 0;

         if(!strcmp(buff,name))
         {
             strcpy(value,vPtr);
             break;
         }
    }

    fclose(f);
    return value;
}


/******************************************************************************/
/*!
**  \brief Fills parameter struct
**
**  This function will fill the parameter data structure with additional
**  parameters as read from either the temp file or the flash file system.
**
**  \param f File pointer to file containing name/value pairs
**  \return N/A
*/

void fillParamStruct(FILE *f)
{
    char            buff[256];
    u_int32_t  syncWord;
    char            *vPtr;

    /*
    ** Read the first word.  It should be 0xfaf30020
    */

    fread(&syncWord,4,1,f);

    /*
    ** For proper WPS operation, we need to know if ANY parameters have
    ** been changed.  This is done by using a bit in the sync word
    ** that indicates if any updates of the paramters from factory
    ** conditions have occurred.  This bit will be set whenever the
    ** cache or flash are updated, except for a  "cfg -x" which will
    ** set everything back to Factory Default
    */

    if(syncWord & NOT_FACTORY_DEFAULT)
            FactoryDefault = 0;
    else
            FactoryDefault = 1;

    if((syncWord & ~NOT_FACTORY_DEFAULT) == 0xfaf30020 )
    {
        /*
        ** File has been initialized.  Let's read until we find a NULL
        */

        while( !feof(f) )
        {
            /*
            ** Read one line at a time.  Seperated by line feed
            ** characters.
            */

            fgets(buff,256,f);

            if( buff[0] == 0 )
                break;

            /*
            ** We don't want to use extractParam here since the cache may contain
            ** "funny" characters for certain parameters.  Just assume the NAME=VAL
            ** format, terminated by 0x0a
            */

            if(vPtr=strchr(buff,0x0a))
                *vPtr = 0;  // extract the line feed
            else
                break;      // No line feed, bad line.

            vPtr = strchr(buff,'=');

            /*
            ** If this string doesn't have an "=" inserted, it's a malformed
            ** string and we should just terminate.  If it does, Replace the
            ** equal sign with a null to seperate the name and value strings,
            ** so they can be copied directly
            */

            if(!vPtr)
                break;
            else
                *vPtr++ = 0;

            /*
            ** Insert into the local structure
            */

            CFG_set_by_name_sync(buff,vPtr);
        }
    }
}


/******************************************************************************/
/*!
**  \brief Translate variable name
**
** This function translates the variable name provided by inserting the
** appropriate index values.  The # marker indicates the index value, and
** the @ marker indicates the radio ID value.  Output buffer is provided
** by the caller
**
**  \param  varName         Pointer to variable name string with embedded markers
**  \param  transVarName    Pointer to buffer to put the fully translated name into
**  \return pointer to buffer provided for variable name
*/

char *translateName(char *varName, char *transVarName)
{
    char    *p = transVarName;

    while(*varName)
    {
        if(*varName == '#')
        {
            /*
            ** If the parameter index is greater than 1, we will put in
            ** an actual value.  If it is 1, then we ignore the # charecter
            ** and not insert anything
            */

            if(parameterIndex > 1)
                p += sprintf(p,"_%d",parameterIndex);
        }
        else if(*varName == '@')
        {
            /*
            ** Here, we'll always insert the radio index value, with
            ** no preceeding underscore.  Usually the value is 0 or
            ** 1
            */

            p += sprintf(p,"%d",radioIndex);
        }
        else
        {
            *p++ = *varName;
        }
        varName++;
    }
    /*
    ** Null terminate, jut to be thorough
    */

    *p = 0;

    return(transVarName);
}
#include <netinet/in.h>

/*****************************************************************************
**
** processSpecial
**
** This function expands special processing tags.  These tags must exist on a
** single line.  These are used as substution tags in template files that
** are replaced with values in the enviornment or in the saved parameter
** cache.
**
** Formats Supported
**
** ~`executable string`~    Indicates run the command and insert it's output
** ~cParam:Value~           For Checkbox/Radio Button support (if Param = Value)
** ~sParam:Value~           For Select support (if Param=Value)
** ~~Param:Default~     `   Direct Parameter Substution, use default if not defined
** ~?Param:Value`executable`~   Conditional execution, if Param=value
**
**  PARAMETERS:
**
**  RETURNS: 
**
*/

char *processSpecial(char *paramStr, char *outBuff)
{

    char            arg;
    char            *secArg = NULL;
    char            *exeArg = NULL;
    char            *indexPtr;
    char            *exePtr;
    char            paramVal[128];
    char            paramRawVal[70];
    char            paramIndexName[48];
    unsigned int    extraParamIndex;
    unsigned int    negate = 0;
    unsigned int    cmpVal;
    char            wifi0_flag[5];
    char            wifi1_flag[5];


	CFG_get_by_name("WIFION_OFF",wifi0_flag);
	CFG_get_by_name("WIFION_OFF_3",wifi1_flag);

    /*
    ** Get the pointers to the unique components of the parameter string
    */

    arg = *paramStr++;

    secArg = strchr(paramStr,':');

    /*
    ** If a parameter is indicated with a ! instead of a :, then any comparison
    ** is negated (used for comparison implementations)
    */

    if(!secArg)
    {
        secArg = strchr(paramStr,'!');

        if(secArg)
            negate = 1;
    }

    /*
    ** If the second argument is specified, break it out
    */

    if(secArg)
    {
        *secArg++ = 0;

        exeArg = strchr(secArg,'`');
        if(exeArg)
        {
            *exeArg++ = 0;
            exePtr = strchr(exeArg,'`');
            if(exePtr)
                *exePtr = 0;
        }
    }

    /*
    ** Get the parameter in question
    **
    ** There are two "index" markers, the index marker (which vap) and the radio marker
    ** (which radio).  We need to search for both the # and @ characters, and replace them
    ** with the proper value.  Will create a function that iterates through the parameter
    ** name string, and replaces the tokens with the appropriate values.
    */

    CFG_get_by_name(translateName(paramStr,paramIndexName),paramRawVal);
    if(*paramRawVal == '\0')
        get_webuicfg(translateName(paramStr,paramIndexName),paramRawVal);

    if(ModeFlag)
    {
        /*
        ** Direct translation, don't HTMLify
        */

        strcpy(paramVal,paramRawVal);
    }
    else
    {
        htmlEncode(paramRawVal,paramVal);
    }

    /*
    ** Unique processing depends on the argument.  The supported formats will
    ** be processed individually
    */

    switch (arg)
    {
    case '~':
        /*
        ** Direct Insertion.  If no value, insert default
        */

        if( paramVal[0] == 0 && secArg != 0)
            outBuff += sprintf(outBuff,"%s",secArg);
        else
            outBuff += sprintf(outBuff,"%s",paramVal);
        break;

    case '!':
        /*
        ** Abort Line.  If the parameter has no specified OR default
        ** value, simply do not output the line.  Used for file substution
        ** for values that may or may not be there
        */

        if( paramVal[0] == 0 && secArg != 0)
            outBuff += sprintf(outBuff,"%s",secArg);
        else
            if( paramVal[0] == 0 )
                AbortFlag = 1;
            else
                outBuff += sprintf(outBuff,"%s",paramVal);
        break;

    case 'c':
        /*
        ** If the sec arg and the value are equal, then put "checked" in the output
        */

        if( secArg != NULL)
        {
            cmpVal = strcmp(paramVal,secArg);

            if( (negate && cmpVal) || (!negate && !cmpVal) )
                outBuff += sprintf(outBuff,"checked");
        }
        break;

    case 's':
        /*
        ** If the sec arg and the value are equal, then put "checked" in the output
        */

        if( secArg != NULL)
        {
            cmpVal = strcmp(paramVal,secArg);

            if( (negate && cmpVal) || (!negate && !cmpVal) )
                outBuff += sprintf(outBuff,"selected");
        }
        break;

    case '`':
        {

            /*
            ** Execute the command. Contained in paramStr for this case
            */

            exePtr = strchr(paramStr,'`');
            if( exePtr )
                *exePtr = 0;

            outBuff = expandOutput(Execute_cmd(paramStr,rspBuff),outBuff);
        }
        break;

    case '?':
        cmpVal = strcmp(paramVal,secArg);

        if( (negate && cmpVal) || (!negate && !cmpVal) )
        {
            outBuff = expandOutput(Execute_cmd(exeArg,rspBuff),outBuff);
        }

        break;

    case '$':
        /*
        ** Insert "extra" Parameter by index
        */

        extraParamIndex = atoi(paramStr) - 1;
        if(extraParamIndex < numAdditionalParams)
            outBuff += sprintf(outBuff,"%s",additionalParams[extraParamIndex]);

        break;

    case '#':
        /*
        ** Insert _Index to allow for indexed parameter names
        */

        if(parameterIndex > 1)
            outBuff += sprintf(outBuff,"_%d",parameterIndex);
        break;
    case 'i':
        /*
        ** Insert Index to insert the interface 
        */
	    if(parameterIndex)	
             outBuff += sprintf(outBuff,"%d",parameterIndex-1);
	    else
             outBuff += sprintf(outBuff,"%d",0);
        break;
    case '@':
        /*
        ** Insert the radio index number directly.  Default is 0
        */

        outBuff += sprintf(outBuff,"%d",radioIndex);
        break;

    case 'h':
        /*
        ** Enable the line if the value is a hex string.  This can be negated, like the 'e' tag.
		** If the parameter name has "PSK" inserted, it's the WPA PSK key, otherwise it's a WEP
		** key.
        */

    	if(!strncmp(paramIndexName,"PSK_KEY",7))
			cmpVal = isKeyHex(paramVal,(KEY_IS_WPA | SILENT_ERROR));
		else
			cmpVal = isKeyHex(paramVal,(KEY_IS_WEP | SILENT_ERROR));

        if( (!negate && !cmpVal) || (negate && cmpVal || cmpVal == -1) )
            AbortFlag = 1;

        break;

    case 'f':
        /*
        ** Enable the line if the the values are factory defaults.
        */

        if( (!negate && !FactoryDefault) || (negate && FactoryDefault) )
            AbortFlag = 1;

        break;

    case 'e':
        /*
        ** Enable the line.  This is used in cases where the parameter line in a file
        ** is dependant on another variable.  If it's enabled, then further processing
        ** can occur.  If not, then the line is abandoned.
        */

        cmpVal = strcmp(paramVal,secArg);
        if( (!negate && cmpVal) || (negate && !cmpVal) )
            AbortFlag = 1;

        break;

     case 'L'://update wifi list
		 {	
			fprintf(errOut,"\nupdate wifi list ----------secArg:%s\n",secArg);
			if(strcmp(secArg,"List")==0)
			{		
				if(flaglist!=1)
					break;
				
				FILE *fp;
				int ii;
				char ch;
				char tmpc[65536]={0};
				if((fp=fopen("/tmp/wifilist","r"))==NULL)
				{
					fprintf(errOut,"\n----------cannot open file  line:%d\n",__LINE__);
				}
			else
			{
				while ((ch=fgetc(fp))!=EOF)
				{
					sprintf(tmpc+ii,"%c",ch);
					ii++;
				}
				if(strlen(tmpc)>0)
					outBuff += sprintf(outBuff,"%s",tmpc);
					fclose(fp);
					//system("rm /tmp/wifilist");
				}
				//flaglist=0;
			}	
			 if(strcmp(secArg,"List2")==0)
 	           {		
				 if(flaglist_2!=1)
					 break;
			 
				 FILE *fp_5g;
				 int ii_5g;
				 char ch_5g;
				 char tmpc_5g[65536]={0};
				 if((fp_5g=fopen("/tmp/wifilist_2","r"))==NULL)
				 {
					 fprintf(errOut,"\n----------cannot open file  line:%d\n",__LINE__);
				 }
				 else
				 {
					 while ((ch_5g=fgetc(fp_5g))!=EOF)
					 {
						 sprintf(tmpc_5g+ii_5g,"%c",ch_5g);
						 ii_5g++;
					 }
					 if(strlen(tmpc_5g)>0)
						 outBuff += sprintf(outBuff,"%s",tmpc_5g);
					 fclose(fp_5g);
					 //system("rm /tmp/wifilist_2");
				 }
				 //flaglist_2=0;
            }
     	}
        break;       

             case 'Z':
            {
                fprintf(errOut,"\n----------secArg:%s\n",secArg);
                //接入管理(带配置)
                if(strcmp(secArg,"conmlist")==0)
                {
					FILE *fp;
					//const char *staFile = "/etc/.staMac";
					struct staList stalist;
					char buf[150];
					char buff[5];
					
					if( (fp = fopen(STA_MAC, "r")) != NULL)
					{
						while(fread(&stalist, sizeof stalist, 1, fp) == 1)
						{
							outBuff += sprintf(outBuff, "<tr>");
							outBuff += sprintf(outBuff, "<td>%d</td>",stalist.id);
							outBuff += sprintf(outBuff, "<td>%s</td>",stalist.macAddr);
							outBuff += sprintf(outBuff, "<td>%s</td>",stalist.staDesc);

                            if(strcmp(stalist.status, "1") == 0)
							{
	                            outBuff += sprintf(outBuff,"<td><input type=\"checkbox\" name=\"%d\" id=\"%d\" onclick=\"lismod(this.name)\" checked></td>",stalist.id,stalist.id);
                            }
							else
	                            outBuff += sprintf(outBuff,"<td><input type=\"checkbox\" name=\"%d\" id=\"%d\" onclick=\"lismod(this.name)\"></td>",stalist.id,stalist.id);

							
							//sprintf(buf, "<td><input type=\"button\" name=\"%s\"  style=\"color:red;font-size:20px;\" value=\"脳\" onClick=\"listdel(this.name)\"></td>", stalist.macAddr);
                                    outBuff += sprintf(outBuff,"<td><img border=\"0\" name=\"%s\"  style=\"cursor:hand;\" src=\"../images/del.gif\" width=\"20\" height=\"20\" onclick=\"listdel(this.name)\" /></td>",stalist.macAddr);

							//outBuff += sprintf(outBuff, buf);
	                    	outBuff += sprintf(outBuff, "</tr>");
						}
						fclose(fp);
					}

                }
                //家长控制(带配置)
                else if(strcmp(secArg,"parclist")==0)
                {
                    FILE *f_parc;
                    int id = 1;
                    char cmd_buffer[512];
                    char mac[240];
                    char url[128];
                    char *url_tmp;
                    char weekdays[30];
                    char time_start[30];
                    char time_stop[30];
                    char enable_flag[10];

                    int i=0;
                    int ret;

                    int url_num = 0;

                    f_parc = fopen(PARC_PATH,"r");

                    if(f_parc)
                    {
                        while(1)
                        {
                            ret = fscanf(f_parc,"%s -A FORWARD_ACCESSCTRL -i br0 -m mac --mac-source %s -m multiurl --url %s -m time --timestart %s --timestop %s --weekdays %s -j RETURN\n",enable_flag, mac, url, time_start, time_stop, weekdays);
                            if(ret < 3)
                                break;

                                outBuff += sprintf(outBuff,"<tr>");
                                outBuff += sprintf(outBuff,"<td>%d</td>",id);
                                outBuff += sprintf(outBuff,"<td>%s</td>",mac);
                                //outBuff += sprintf(outBuff,"<td>%s</td>","www.baidu.com<br>www.qq.com<br>");
                                outBuff += sprintf(outBuff,"<td>");
                           //     outBuff += sprintf(outBuff,"%s<br>",url);
                                url_tmp = strtok(url,",");
                                while(url_tmp != NULL)
                                {
                                    if(strncmp(url_tmp,"return",6)==0)
                                        break;

                                     outBuff += sprintf(outBuff,"%s<br>",url_tmp);
                                     url_tmp = strtok(NULL,",");
                                }
                                outBuff += sprintf(outBuff,"</td>");
                                outBuff += sprintf(outBuff,"<td>%s</td>",weekdays);
                                outBuff += sprintf(outBuff,"<td>%s-%s</td>", time_start, time_stop);

                                if(strncmp(enable_flag,"##",2)==0)
		                            outBuff += sprintf(outBuff,"<td>off</td>");
                                else
		                            outBuff += sprintf(outBuff,"<td>on</td>");
                        //        outBuff += sprintf(outBuff,"<td>%s</td>","ON");
                                outBuff += sprintf(outBuff,"<td><img border=\"0\" name=\"%d\" src=\"../images/mod.gif\" width=\"20\" height=\"20\" onclick=\"modify(this.name)\" /></td>",id);
	          
                                outBuff += sprintf(outBuff,"<td><img border=\"0\"  style=\"cursor:hand;\" name=\"%d\" src=\"../images/del.gif\" width=\"20\" height=\"20\" onclick=\"listdel(this.name)\" /></td>",id);
                              //outBuff += sprintf(outBuff,"<td><input type=\"button\" name=\"%d\"  style=\"color:red;font-size:20px;\" value=\"脳\" onClick=\"listdel(this.name)\"></td>", id);
                                outBuff += sprintf(outBuff,"</tr>");

                                //ret = fscanf(f_parc,"iptables -A FORWARD_ACCESSCTRL -i br0 -m mac --mac-source %s -j DROP\n",mac);
                                id++;
                        }

                    }
                    else
                    {
                        printf("fopen error\n");
                    }

                    fclose(f_parc);

              }
                //静态DHCP(带配置)
                else if(strcmp(secArg,"sdhcplist")==0)
                {
                	/*wangyu add for ipmac list*/
					unsigned int id ,num ;	
					char addr_buf[1024];
					FILE *f = fopen("/etc/ip_mac.conf","r");
					
					if ( !f )
					{
						 fprintf(errOut,"\n%s  %d open ip_mac error\n \n",__func__,__LINE__);
						 break;
					}
					char addr_mac[17],addr_ip[15],addr_status[10];		
					id = 1;

					memset(addr_buf,0,1024);
					
					while ( fgets(addr_buf,1024,f) != NULL )
					{
						if(strstr(addr_buf,"static_lease"))
					{
						sscanf(addr_buf,"static_lease %s %s %s",addr_mac,addr_ip,addr_status);
					#ifdef MESSAGE
						fprintf(errOut,"\n%s  %d addr_mac, %s addr_ip, %s addr_status %s \n \n",
								__func__,__LINE__,addr_mac,addr_ip,addr_status);
					#endif
						outBuff += sprintf(outBuff,"<tr>");
	                    outBuff += sprintf(outBuff,"<td>%d</td>",id);
	                    outBuff += sprintf(outBuff,"<td>%s</td>",addr_mac);   
	                    outBuff += sprintf(outBuff,"<td>%s</td>",addr_ip);

                          if ( memcmp ( addr_status , "enable" ,6) == 0)
                            outBuff += sprintf(outBuff,"<td><input type=\"checkbox\" name=\"%s\" id=\"%s\" onclick=\"lismod(this.name)\" checked></td>",addr_mac,addr_mac);
                          if ( memcmp ( addr_status , "disable" ,7) == 0)
                            outBuff += sprintf(outBuff,"<td><input type=\"checkbox\" name=\"%s\" id=\"%s\" onclick=\"lismod(this.name)\"></td>",addr_mac,addr_mac);
						//outBuff += sprintf(outBuff,"<td><input type=\"button\" name=\"%s\"  style=\"color:red;font-size:30px;cursor:hand;\" value=\"脳\" onClick=\"listdel(this.name)\"></td>",addr_mac);
                            outBuff += sprintf(outBuff,"<td><img border=\"0\" name=\"%s\" style=\"cursor:hand;\" src=\"../images/del.gif\" width=\"20\" height=\"20\" onclick=\"listdel(this.name)\" /></td>",addr_mac);

	                    outBuff += sprintf(outBuff,"</tr>");
						id ++;						
						memset(addr_buf,0,1024);
							}
					}
					fclose(f);
					/*wangyu add end*/
                }
                //静态路由表(带配置)
                else if(strcmp(secArg,"routelist")==0)
                {
                	/*luodp add for ipmac list*/
					unsigned int id=1;	
					char buf[1024];
					char des_ip[15],de_mask[15],gw_ip[15],rule_status[10];		
					
					FILE *f = fopen("/etc/route.conf","r");
					if ( !f )
					{
						 fprintf(errOut,"\n%s  %d open /etc/route.conf error\n \n",__func__,__LINE__);
						 break;
					}
					memset(buf,0,1024);
					
					while ( fgets(buf,1024,f) != NULL )
					{
						sscanf(buf,"%s %s %s %s",des_ip,de_mask,gw_ip,rule_status);
					//#ifdef MESSAGE
						fprintf(errOut,"\n%s  %d des_ip, %s de_mask, %s gw_ip %s rule_status %s\n \n",
								__func__,__LINE__,des_ip,de_mask,gw_ip,rule_status);
					//#endif
						outBuff += sprintf(outBuff,"<tr>");
	                    outBuff += sprintf(outBuff,"<td>%d</td>",id);
	                    outBuff += sprintf(outBuff,"<td>%s</td>",des_ip);   
	                    outBuff += sprintf(outBuff,"<td>%s</td>",de_mask);
						outBuff += sprintf(outBuff,"<td>%s</td>",gw_ip);
						if ( memcmp ( rule_status , "enable" ,6) == 0)
		                   outBuff += sprintf(outBuff,"<td><input type=\"checkbox\" name=\"%s\" id=\"%s\" onclick=\"lismod(this.name)\" checked></td>",des_ip,des_ip);
						if ( memcmp ( rule_status , "disable" ,7) == 0)
		                    outBuff += sprintf(outBuff,"<td><input type=\"checkbox\" name=\"%s\" id=\"%s\" onclick=\"lismod(this.name)\"></td>",des_ip,des_ip);
						//outBuff += sprintf(outBuff,"<td><input type=\"button\" name=\"%s\"  style=\"color:red;font-size:30px;cursor:hand;\" value=\"脳\" onClick=\"listdel(this.name)\"></td>",des_ip);
                        outBuff += sprintf(outBuff,"<td><img border=\"0\" name=\"%s\"  style=\"cursor:hand;\" src=\"../images/del.gif\" width=\"20\" height=\"20\" onclick=\"listdel(this.name)\" /></td>",des_ip);
		
	                    outBuff += sprintf(outBuff,"</tr>");
						id ++;						
						memset(buf,0,1024);
					}
					fclose(f);
					/*luodp add end*/
                }
                //IPMAC绑定(带配置)
                else if(strcmp(secArg,"IPMAClist")==0)
                {
					/*luodp add for ipmac list*/
					unsigned int id=1;	
					char buf[1024];
					char arp_ip[15],arp_mac[17],arp_status[10];		
					
					FILE *f = fopen("/etc/arp_ip_mac.conf","r");
					if ( !f )
					{
						 fprintf(errOut,"\n%s  %d open /etc/arp_ip_mac_on.conf error\n \n",__func__,__LINE__);
						 break;
					}
					memset(buf,0,1024);
					
					while ( fgets(buf,1024,f) != NULL )
					{
						sscanf(buf,"%s %s %s",arp_ip,arp_mac,arp_status);
						outBuff += sprintf(outBuff,"<tr>");
	                    outBuff += sprintf(outBuff,"<td>%d</td>",id);
	                    outBuff += sprintf(outBuff,"<td>%s</td>",arp_mac);   
	                    outBuff += sprintf(outBuff,"<td>%s</td>",arp_ip);
		                //outBuff += sprintf(outBuff,"<td>%s</td>","YES");
		                if (strstr(arp_status , "enable"))
		                	outBuff += sprintf(outBuff,"<td><input type=\"checkbox\" name=\"%s\" id=\"%s\" onclick=\"lismod(this.name)\" checked></td>",arp_ip,arp_ip);
						if (strstr(arp_status , "disable"))
							outBuff += sprintf(outBuff,"<td><input type=\"checkbox\" name=\"%s\" id=\"%s\" onclick=\"lismod(this.name)\"></td>",arp_ip,arp_ip);
						//outBuff += sprintf(outBuff,"<td><input type=\"button\" name=\"%s\"  style=\"color:red;font-size:30px;cursor:hand;\" value=\"脳\" onClick=\"listdel(this.name)\"></td>",arp_ip);
                        outBuff += sprintf(outBuff,"<td><img border=\"0\" name=\"%s\"  style=\"cursor:hand;\" src=\"../images/del.gif\" width=\"20\" height=\"20\" onclick=\"listdel(this.name)\" /></td>",arp_ip);
	                    outBuff += sprintf(outBuff,"</tr>");
						id ++;						
						memset(buf,0,1024);
					}
					fclose(f);
					#if 0
					f = fopen("/etc/arp_ip_mac_off.conf","r");
					if ( !f )
					{
						 fprintf(errOut,"\n%s  %d open /etc/arp_ip_mac_off.conf error\n \n",__func__,__LINE__);
						 break;
					}
					memset(buf,0,1024);
					
					while ( fgets(buf,1024,f) != NULL )
					{
						sscanf(buf,"%s %s",arp_ip,arp_mac);
						outBuff += sprintf(outBuff,"<tr>");
	                    outBuff += sprintf(outBuff,"<td>%d</td>",id);
	                    outBuff += sprintf(outBuff,"<td>%s</td>",arp_mac);   
	                    outBuff += sprintf(outBuff,"<td>%s</td>",arp_ip);
		                //outBuff += sprintf(outBuff,"<td>%s</td>","NO");
						outBuff += sprintf(outBuff,"<td><input type=\"checkbox\" name=\"%s\" id=\"%s\" onclick=\"lismod(this.name)\"></td>",arp_ip,arp_ip);
						outBuff += sprintf(outBuff,"<td><input type=\"button\" name=\"%s\"  style=\"color:red;font-size:30px;cursor:hand;\" value=\"脳\" onClick=\"listdel(this.name)\"></td>",arp_ip);
	                    outBuff += sprintf(outBuff,"</tr>");
						id ++;						
						memset(buf,0,1024);
					}
					fclose(f);
					/*luodp add end*/
					#endif

                }
                //系统ARP映射表(不带配置)
                else if(strcmp(secArg,"IPMAClistshow")==0)
                {
					system("arp -a > /dev/null 2>&1"); 
					FILE *fp;
					int ii;
					char ch;
					char tmpc[65536]={0};
					if((fp=fopen("/tmp/arplist","r"))==NULL)
					{
						fprintf(errOut,"\n----------cannot open file  line:%d\n",__LINE__);
					}
					else
					{
						while ((ch=fgetc(fp))!=EOF)
						{
							sprintf(tmpc+ii,"%c",ch);
							ii++;
						}
						if(strlen(tmpc)>0)
							outBuff += sprintf(outBuff,"%s",tmpc);
						fclose(fp);
						system("rm /tmp/arplist");
					}
                }

                //接入管理有线设备(不带配置)
                else if(strcmp(secArg,"wiredevlist")==0)
                {
                    FILE *fp, *fp1, *flist, *fWriteOk;
    				struct staList oldstalist, *p;
                    struct dhcpOfferedAddr 
                    {
                        unsigned char hostname[16];
                        unsigned char mac[16];
                        unsigned long ip;
                        unsigned long expires;
                    } lease;
                    char STAbuf[128];
                    char buf[20], mac_buf[20], rate_buf[10], rssi_buf[10];
                    int i, j;
                    struct in_addr addr;
                    unsigned long expires;
                    unsigned d, h, m;
                    int num=1;
                    int shi=0;
					int add = 0;
					int open = 0;

                    Execute_cmd("killall -q -USR1 udhcpd", rspBuff);
					if(strcmp(wifi0_flag,"on") == 0 ) //on
                    	Execute_cmd("wlanconfig ath0 list sta > /etc/.STAlist 2>&1", rspBuff);					
					if(strcmp(wifi1_flag,"on") == 0 ) //on
						Execute_cmd("wlanconfig ath2 list sta > /etc/.STAlist2 2>&1", rspBuff);  /*for 5G*/

					fprintf(errOut,"\n%s  %d  to open [dhcpclinetlist]\n",__func__,__LINE__);

					/*if the /etc/.OldStaList is not exit, creat it*/
					if((fp1 = fopen(OLD_STAFILE, "r")) == NULL)     /*  /etc/.OldStaList  */
					{
						open = 1;
						fprintf(errOut,"\n%s  %d  creat the file %s\n",__func__,__LINE__, OLD_STAFILE);
						fp1 = fopen(OLD_STAFILE, "at");
						flist = fopen("/etc/.STAlist", "r");    /*  /etc/.STAlist   */
						memset(STAbuf, 0, sizeof STAbuf);
						fgets(STAbuf, 128, flist);
						if(strlen(STAbuf) > 0)
						{
	                        while(fgets(STAbuf, 128, flist))
	                        {
	                        	memset(&oldstalist, 0, sizeof(oldstalist));
	                        	strncpy(oldstalist.macAddr, STAbuf, 17);
								fwrite(&oldstalist, sizeof(oldstalist), 1, fp1);
	                        }
						}
						fclose(flist);
						fclose(fp1);
					}
					
					if((flist = fopen("/etc/.STAlist", "r")) != NULL)    /*  /etc/.STAlist   */
					{
						add = 0;
						memset(STAbuf, 0, sizeof STAbuf);
						fgets(STAbuf, 128, flist);
						//fprintf(errOut,"\n%s  %d  -------------the STAbuf is %s \n",__func__,__LINE__, STAbuf);
						if(strlen(STAbuf) > 0)
						{
		                    while(fgets(STAbuf, 128, flist))
		                    {
		                    	int ret = 0;
		                    	memset(buf, 0, sizeof buf);
								strncpy(buf, STAbuf, 17);
								if(open == 1)
									fp1 = fopen(OLD_STAFILE, "r");			/*  /etc/.OldStaList  */
								//fprintf(errOut,"\n%s  %d  buf is %s\n",__func__,__LINE__, buf);
								while(fread(&oldstalist, sizeof oldstalist, 1, fp1) == 1)
								{
									//fprintf(errOut,"buf is [%s],oldstalist.macAddr is [%s]\n\n", buf, oldstalist.macAddr);
									if(strcmp(buf, oldstalist.macAddr) == 0)
									{
										ret = 1;
										break;
									}
								}
								
								if(ret == 0)
								{
									addSta(buf, oldstalist.staDesc, oldstalist.status, oldstalist.id);
									add = 1;
								}
								fclose(fp1);
								open =  1;
		                    }
							fclose(flist);
							
							if((add == 1) && (fp1 = fopen(OLD_STAFILE, "at")))			/*  /etc/.OldStaList  */
							{
								p = scan_staList(staHostList);
								while(p)
								{
									fwrite(p, sizeof(struct staList), 1, fp1);
									p = p->next;
								}
								fclose(fp1);
							}
						}
                	}
					
					if((flist = fopen("/etc/.STAlist2", "r")) != NULL)    /*  /etc/.STAlist2   */
					{
						add = 0;
						memset(STAbuf, 0, sizeof STAbuf);
						fgets(STAbuf, 128, flist);
						//fprintf(errOut,"\n%s  %d  -------------the STAbuf22 is %s \n",__func__,__LINE__, STAbuf);
						if(strlen(STAbuf) > 0)
						{
		                    while(fgets(STAbuf, 128, flist))
		                    {
		                    	int ret = 0;
		                    	memset(buf, 0, sizeof buf);
								strncpy(buf, STAbuf, 17);
								if(open == 1)
									fp1 = fopen(OLD_STAFILE, "r");			/*  /etc/.OldStaList  */
								//fprintf(errOut,"\n%s  %d  buf is %s\n",__func__,__LINE__, buf);
								while(fread(&oldstalist, sizeof oldstalist, 1, fp1) == 1)
								{
									//fprintf(errOut,"buf is [%s],oldstalist.macAddr is [%s]\n\n", buf, oldstalist.macAddr);
									if(strcmp(buf, oldstalist.macAddr) == 0)
									{
										ret = 1;
										break;
									}
								}
								
								if(ret == 0)
								{
									addSta(buf, oldstalist.staDesc, oldstalist.status, oldstalist.id);
									add = 1;
								}
								fclose(fp1);
								open =  1;
		                    }
							fclose(flist);
							
							if((add == 1) && (fp1 = fopen(OLD_STAFILE, "at")))			/*  /etc/.OldStaList  */
							{
								p = scan_staList(staHostList);
								while(p)
								{
									fwrite(p, sizeof(struct staList), 1, fp1);
									p = p->next;
								}
								fclose(fp1);
							}
						}
					}
					
					//fprintf(errOut,"\n%s  %d  zhaozhanwei22222\n",__func__,__LINE__);
                    if((fp = fopen(UDHCPD_FILE, "r")) != NULL)  /*  /var/run/udhcpd.leases   */
                    {
                    	while (fread(&lease, sizeof(lease), 1, fp) == 1)
	                    {
	                        shi=0;
							
							if(strlen(lease.mac) == 0)
								continue;
							//fprintf(errOut,"\n%s  %d lease.mac is [%x] len is %d\n",__func__,__LINE__, lease.mac, strlen(lease.mac));
							
							memset(mac_buf, 0, sizeof(mac_buf));
							for(i = 0, j = 0 ; i < 6; i++, j+=3)
	                        {
	                            sprintf(&mac_buf[j], "%02x:", lease.mac[i]);
	                        }
							//fprintf(errOut,"\n%s  %d mac_buf is [%s] len is %d\n",__func__,__LINE__, mac_buf);
							
	                        /*compare MAC*/
							if((fp1 = fopen(OLD_STAFILE, "r")) != NULL)		/*  /etc/.OldStaList  */
							{
								while(fread(&oldstalist, sizeof(struct staList), 1, fp1) == 1)
								{
									//fprintf(errOut,"\n%s  %d oldstalist.macAddr:%s mac_buf:%s\n",__func__,__LINE__,oldstalist.macAddr,mac_buf);
									if(!strncmp(oldstalist.macAddr, mac_buf, 17))
		                            {
		                                shi=1;  //wireless
		                                break;
		                            }
								}
								fclose(fp1);
								
								if(shi == 1)	continue;
							}
	                        //fprintf(errOut,"\n%s  %d mac_buf:%s buf:%s\n",__func__,__LINE__,mac_buf,buf);
							
	                        if(shi == 0)
	                        {
	                            outBuff += sprintf(outBuff,"<tr>");
	                            outBuff += sprintf(outBuff,"<td>%d</td>",num);
	                            num++; 
								
	                            //fprintf(errOut,"\n%s  %d  zhaozhanwei the hostname is %s \n",__func__,__LINE__, lease.hostname);
	                            if (strlen(lease.hostname) > 0)
	                                outBuff += sprintf(outBuff,"<td>%s</td>",lease.hostname);
	                            else
	                                outBuff += sprintf(outBuff,"<td>NULL</td>");
	                            
	                            if (lease.ip > 0)
								{
									addr.s_addr = lease.ip;
	                            	expires = ntohl(lease.expires);
	                            	outBuff += sprintf(outBuff,"<td>%s</td>", inet_ntoa(addr));
								}
								else
									outBuff += sprintf(outBuff,"<td>NULL</td>", inet_ntoa(addr));

								if(strlen(mac_buf) > 0)
								{
									strncpy(buf, mac_buf, 17);
									outBuff += sprintf(outBuff,"<td>%s</td>", buf);
								}
								else
									outBuff += sprintf(outBuff,"<td>NULL</td>");
	                            //outBuff += sprintf(outBuff,"<td>%02x", lease.mac[0]);
	                            //for (i = 1; i < 6; i++)
	                            //     outBuff += sprintf(outBuff,":%02x", lease.mac[i]);
	                            
	                             outBuff += sprintf(outBuff,"</td>");
	                             outBuff += sprintf(outBuff,"</tr>");
	                        }
	                    }
                    	fclose(fp);
                	}
                }
                //接入管理无线设备(不带配置)
                else if(strcmp(secArg,"wirelessdevlist")==0)
                {
                    FILE *fp, *flist;
					struct staList oldstalist;
                    struct dhcpOfferedAddr 
                    {
                        unsigned char hostname[16];
                        unsigned char mac[16];
                        unsigned long ip;
                        unsigned long expires;
                    } lease;
                    char STAbuf[128];
                    char buf[20], mac_buf[20], rate_buf[10], rssi_buf[10];
					char valBuff[10];
                    int i, j;
                    struct in_addr addr;
                    unsigned long expires;
                    unsigned d, h, m;
                    int num=1;
					int staticIp;

                    //Execute_cmd("killall -q -USR1 udhcpd", rspBuff);
                    //Execute_cmd("wlanconfig ath0 list sta > /var/run/.STAlist", rspBuff);
                    Execute_cmd("cfg -e | grep \"DHCPON_OFF=\" | awk -F \"=\" \'{print $2}\'",valBuff);
					//fprintf(errOut,"\n%s  %d valBuff is %s\n \n",__func__,__LINE__, valBuff);
					if(strstr(valBuff, "on"))
					{
					
						/*2.4G*/
	                    flist = fopen("/etc/.STAlist", "r");    /*  /etc/.STAlist   */
						fgets(STAbuf, 128, flist);
	                    while(fgets(STAbuf, 128, flist))
	                    {
	                    	memset(buf, 0, sizeof(buf));
	                    	strncpy(buf, STAbuf, 17);
							
	                    	fp = fopen(UDHCPD_FILE, "r");  /*  /var/run/udhcpd.leases   */	                   						
							while (fread(&lease, sizeof(lease), 1, fp) == 1)
							{	
								staticIp = 1;
	                            /*compare MAC*/
	                            memset(mac_buf, 0, sizeof(mac_buf));
                            	for(i = 0, j = 0 ; i < 6; i++, j+=3)
                                	sprintf(&mac_buf[j], "%02x:", lease.mac[i]);
	                            
	                            //fprintf(errOut,"\n%s  %d 2.4G mac_buf:%s buf:%s\n",__func__,__LINE__, mac_buf, buf);
	                            if(strncmp(buf, mac_buf, 17) == 0)
	                            {
	                                outBuff += sprintf(outBuff,"<tr>");
	                                outBuff += sprintf(outBuff,"<td>%d</td>",num);
	                                num++; 
									
	                            	if (strlen(lease.hostname) > 0)
	                                    outBuff += sprintf(outBuff,"<td>%s</td>",lease.hostname);
	                                else
	                                    outBuff += sprintf(outBuff,"<td>NULL</td>");

									
									if (lease.ip > 0)
									{
										addr.s_addr = lease.ip;
	                                	expires = ntohl(lease.expires);
	                                	outBuff += sprintf(outBuff,"<td>%s</td>", inet_ntoa(addr));
										
									}
									else
										outBuff += sprintf(outBuff,"<td>NULL</td>");
	                                
	                                //outBuff += sprintf(outBuff,"<td>%02x", lease.mac[0]);
	                                //for (i = 1; i < 6; i++)
	                                //     outBuff += sprintf(outBuff,":%02x", lease.mac[i]);
	                                //strncpy(buf, mac_buf, 17);
									outBuff += sprintf(outBuff,"<td>%s</td>", buf);
	                                outBuff += sprintf(outBuff,"</td>");
									
	                                memset(rate_buf, 0, sizeof(rate_buf));
	                                strncpy(rate_buf, &STAbuf[27], 5);
	                                outBuff += sprintf(outBuff,"<td>%sbit/s</td>",rate_buf);
	                                
	                                /*get the Signal strength */
	                                memset(rssi_buf, 0, sizeof(rssi_buf));
	                                strncpy(rssi_buf, &STAbuf[35], 5);
	                                outBuff += sprintf(outBuff,"<td>-%s dBm</td>",rssi_buf);//printf("the RSSI is %s\n", rssi_buf);    
	                                outBuff += sprintf(outBuff,"</tr>");
									
									staticIp = 0;
									break;
								}
								
	                        } /*end while (fread(&lease, sizeof(lease), 1, fp) == 1)*/
	                        fclose(fp);
							
							//fprintf(errOut,"\n%s  %d 2.4G the static is %d\n",__func__, __LINE__, staticIp);
							if(staticIp == 1) /*the sta is static ip*/
							{
								outBuff += sprintf(outBuff,"<tr>");
                                outBuff += sprintf(outBuff,"<td>%d</td>",num);
                                num++; 
								/*hostname*/
                            	outBuff += sprintf(outBuff,"<td>NULL</td>");
								/*ip*/
								outBuff += sprintf(outBuff,"<td>NULL</td>");
                                /*mac*/	
                                outBuff += sprintf(outBuff,"<td>%s</td>", buf);
                                
                                outBuff += sprintf(outBuff,"</td>");

                                memset(rate_buf, 0, sizeof(rate_buf));
                                strncpy(rate_buf, &STAbuf[27], 5);
                                outBuff += sprintf(outBuff,"<td>%sbit/s</td>",rate_buf);
                                //printf("the sta's rate is %s\n", rate_buf);

                                /*get the Signal strength */
                                memset(rssi_buf, 0, sizeof(rssi_buf));
                                strncpy(rssi_buf, &STAbuf[35], 5);
                                outBuff += sprintf(outBuff,"<td>-%s dBm</td>",rssi_buf);//printf("the RSSI is %s\n", rssi_buf);    
                                
                                outBuff += sprintf(outBuff,"</tr>");
							}
							
	                    }  /*end 2.4G while(fgets(STAbuf, 128, flist))*/
						fclose(flist);
						
						/*for 5G*/
						flist = fopen("/etc/.STAlist2", "r");    /*  /etc/.STAlist2   */
						fgets(STAbuf, 128, flist);
	                    while(fgets(STAbuf, 128, flist))
	                    {
	                    	memset(buf, 0, sizeof(buf));
	                    	strncpy(buf, STAbuf, 17);
							
	                    	fp = fopen(UDHCPD_FILE, "r");  /*  /var/run/udhcpd.leases   */	                   						
							while (fread(&lease, sizeof(lease), 1, fp) == 1)
							{	
								staticIp = 1;
	                            /*compare MAC*/	                           
	                            memset(mac_buf, 0, sizeof(mac_buf));	                            
                            	for(i = 0, j = 0 ; i < 6; i++, j+=3)
                                	sprintf(&mac_buf[j], "%02x:", lease.mac[i]);
	                            
	                            //fprintf(errOut,"\n%s  %d 5G mac_buf:%s buf:%s\n",__func__,__LINE__,mac_buf,buf);
	                            if(strncmp(buf, mac_buf, 17) == 0)
	                            {
	                                outBuff += sprintf(outBuff,"<tr>");
	                                outBuff += sprintf(outBuff,"<td>%d</td>",num);
	                                num++; 
									
	                            	if (strlen(lease.hostname) > 0)
	                                    outBuff += sprintf(outBuff,"<td>%s</td>",lease.hostname);
	                                else
	                                    outBuff += sprintf(outBuff,"<td>NULL</td>");

									
									if (lease.ip > 0)
									{
										addr.s_addr = lease.ip;
	                                	expires = ntohl(lease.expires);
	                                	outBuff += sprintf(outBuff,"<td>%s</td>", inet_ntoa(addr));
									}
									else
										outBuff += sprintf(outBuff,"<td>NULL</td>", inet_ntoa(addr));
	                                
	                                //outBuff += sprintf(outBuff,"<td>%02x", lease.mac[0]);
	                                //for (i = 1; i < 6; i++)
	                                //     outBuff += sprintf(outBuff,":%02x", lease.mac[i]);
	                                //strncpy(buf, mac_buf, 17);
									outBuff += sprintf(outBuff,"<td>%s</td>", buf);
	                                
	                                outBuff += sprintf(outBuff,"</td>");

	                                memset(rate_buf, 0, sizeof(rate_buf));
	                                strncpy(rate_buf, &STAbuf[27], 5);
	                                outBuff += sprintf(outBuff,"<td>%sbit/s</td>",rate_buf);
	                                //printf("the sta's rate is %s\n", rate_buf);

	                                /*get the Signal strength */
	                                memset(rssi_buf, 0, sizeof(rssi_buf));
	                                strncpy(rssi_buf, &STAbuf[35], 5);
	                                outBuff += sprintf(outBuff,"<td>-%s dBm</td>",rssi_buf);//printf("the RSSI is %s\n", rssi_buf);    
	                                
	                                outBuff += sprintf(outBuff,"</tr>");
									
									staticIp = 0;
									break;
								}
	                        } /*end while (fread(&lease, sizeof(lease), 1, fp) == 1)*/
	                        fclose(fp);
							//fprintf(errOut,"\n%s  %d 5G the static is %d\n",__func__,__LINE__, staticIp);
							if(staticIp == 1) /*the sta is static ip*/
							{
								outBuff += sprintf(outBuff,"<tr>");
                                outBuff += sprintf(outBuff,"<td>%d</td>",num);
                                num++; 
								/*hostname*/
                            	outBuff += sprintf(outBuff,"<td>NULL</td>");
								/*ip*/
								outBuff += sprintf(outBuff,"<td>NULL</td>");
                                /*mac*/	
                                outBuff += sprintf(outBuff,"<td>%s</td>", buf);
                                
                                outBuff += sprintf(outBuff,"</td>");

                                memset(rate_buf, 0, sizeof(rate_buf));
                                strncpy(rate_buf, &STAbuf[27], 5);
                                outBuff += sprintf(outBuff,"<td>%sbit/s</td>",rate_buf);
                                //printf("the sta's rate is %s\n", rate_buf);

                                /*get the Signal strength */
                                memset(rssi_buf, 0, sizeof(rssi_buf));
                                strncpy(rssi_buf, &STAbuf[35], 5);
                                outBuff += sprintf(outBuff,"<td>-%s dBm</td>",rssi_buf);//printf("the RSSI is %s\n", rssi_buf);    
                                
                                outBuff += sprintf(outBuff,"</tr>");
							}
	                    }  /*end 5G while(fgets(STAbuf, 128, flist))*/
						fclose(flist);
						
	                    
	                }/*end if(strstr(valBuff, "on")) */
					else
					{
						flist = fopen("/etc/.STAlist", "r");    /*  /etc/.STAlist   */
                        if (NULL == flist)
                        {
                            fprintf(errOut,"\n%s  %d open STAlist error\n \n",__func__,__LINE__);
                            break;
                        }
                        fgets(STAbuf, 128, flist);
                        while(fgets(STAbuf, 128, flist))
                        {
                            /*compare MAC*/
                            memset(buf, 0, sizeof(buf));
                            memset(mac_buf, 0, sizeof(mac_buf));
                            strncpy(buf, STAbuf, 17);
                            
                            //fprintf(errOut,"\n%s  %d 5G mac_buf:%s buf:%s\n",__func__,__LINE__,mac_buf,buf);
                            
                            //if(strncmp(buf, mac_buf, 17) == 0)
                            {//wireless
                                outBuff += sprintf(outBuff,"<tr>");
                                outBuff += sprintf(outBuff,"<td>%d</td>",num);
                                num++; 

								/*hostname*/
								outBuff += sprintf(outBuff,"<td>NULL</td>");
								/*ip*/
								outBuff += sprintf(outBuff,"<td>NULL</td>");
								/*mac*/
								outBuff += sprintf(outBuff,"<td>%s</td>", buf);
                                
                                outBuff += sprintf(outBuff,"</td>");

                                memset(rate_buf, 0, sizeof(rate_buf));
                                strncpy(rate_buf, &STAbuf[27], 5);
                                outBuff += sprintf(outBuff,"<td>%sbit/s</td>",rate_buf);
                                //printf("the sta's rate is %s\n", rate_buf);

                                /*get the Signal strength */
                                memset(rssi_buf, 0, sizeof(rssi_buf));
                                strncpy(rssi_buf, &STAbuf[35], 5);
                                outBuff += sprintf(outBuff,"<td>-%s dBm</td>",rssi_buf);//printf("the RSSI is %s\n", rssi_buf);    
                                
                                outBuff += sprintf(outBuff,"</tr>");
							}
                        }
                         fclose(flist);

						 /*for 5G sta list*/
						flist = fopen("/etc/.STAlist2", "r");    /*  /etc/.STAlist2   */
						fgets(STAbuf, 128, flist);
						while(fgets(STAbuf, 128, flist))
						{
							/*compare MAC*/
							memset(buf, 0, sizeof(buf));
							memset(mac_buf, 0, sizeof(mac_buf));
							strncpy(buf, STAbuf, 17);
							//fprintf(errOut,"\n%s  %d mac_buf:%s buf:%s\n",__func__,__LINE__,mac_buf,buf);
							
                            //if(strncmp(buf, mac_buf, 17) == 0)
                            {//wireless
								outBuff += sprintf(outBuff,"<tr>");
								outBuff += sprintf(outBuff,"<td>%d</td>",num);
								num++; 

								/*hostname*/
								outBuff += sprintf(outBuff,"<td>NULL</td>");
								/*ip*/
								outBuff += sprintf(outBuff,"<td>NULL</td>");
								/*mac*/
								outBuff += sprintf(outBuff,"<td>%s</td>", buf);

								outBuff += sprintf(outBuff,"</td>");

								memset(rate_buf, 0, sizeof(rate_buf));
								strncpy(rate_buf, &STAbuf[27], 5);
								outBuff += sprintf(outBuff,"<td>%sbit/s</td>",rate_buf);
								//printf("the sta's rate is %s\n", rate_buf);

								/*get the Signal strength */
								memset(rssi_buf, 0, sizeof(rssi_buf));
								strncpy(rssi_buf, &STAbuf[35], 5);
								outBuff += sprintf(outBuff,"<td>-%s dBm</td>",rssi_buf);//printf("the RSSI is %s\n", rssi_buf);    

								outBuff += sprintf(outBuff,"</tr>");
								break;
							}
						}
						fclose(flist);
					}/*end else*/
                }
                 //系统路由表(不带配置)
                else if(strcmp(secArg,"sysrulist")==0)
                {

					system("route -n > /dev/null 2>&1"); 
					FILE *fp;
					int ii;
					char ch;
					char tmpc[65536]={0};
					if((fp=fopen("/tmp/routelist","r"))==NULL)
					{
						fprintf(errOut,"\n----------cannot open file  line:%d\n",__LINE__);
					}
					else
					{
						while ((ch=fgetc(fp))!=EOF)
						{
							sprintf(tmpc+ii,"%c",ch);
							ii++;
						}
						if(strlen(tmpc)>0)
							outBuff += sprintf(outBuff,"%s",tmpc);
						fclose(fp);
						system("rm /tmp/routelist");
					}
                }

                //DHCP客户端(不带配置)
                else if(strcmp(secArg,"dhcpclinetlist")==0)
                {
                    FILE *fp, *flist, *fWriteOk;
                    struct dhcpOfferedAddr 
                    {
                        unsigned char hostname[16];
                        unsigned char mac[16];
                        unsigned long ip;
                        unsigned long expires;
                    } lease;
                    char STAbuf[128];
                    char buf[20], mac_buf[20], rate_buf[10], rssi_buf[10];
                    int i, j;
                    struct in_addr addr;
                    unsigned long expires;
                    unsigned d, h, m;
                    int num=1;
                    int shi=0;
					int ret = 0;

                    Execute_cmd("killall -q -USR1 udhcpd", rspBuff);					
					if(strcmp(wifi0_flag,"on") == 0 ) //on
						Execute_cmd("wlanconfig ath0 list sta > /etc/.STAlist 2>&1", rspBuff);

					fprintf(errOut,"\n%s  %d  to open [dhcpclinetlist] ret is %d\n",__func__,__LINE__, ret);

                    fp = fopen(UDHCPD_FILE, "r");  /*  /var/run/udhcpd.leases   */
                    if (NULL == fp)
                    {
                        fprintf(errOut,"\n%s  %d open udhcpd error\n \n",__func__,__LINE__);
                        break;
                    }

                    while (fread(&lease, sizeof(lease), 1, fp) == 1) 
                    {
                        outBuff += sprintf(outBuff,"<tr>");
                        outBuff += sprintf(outBuff,"<td>%d</td>",num);
                        num++; 

						/*hostname*/
                        if (strlen(lease.hostname) > 0)
                        	outBuff += sprintf(outBuff,"<td>%s</td>",lease.hostname);
                        else
                        	outBuff += sprintf(outBuff,"<td>NULL</td>");

						/*mac*/
						for(i = 0, j = 0 ; i < 6; i++, j+=3)
							sprintf(&mac_buf[j], "%02x:", lease.mac[i]);
						strncpy(buf, mac_buf, 17);
						if(strlen(buf) > 0)
						{
							outBuff += sprintf(outBuff,"<td>%s</td>", buf);
						}
						else
                        	outBuff += sprintf(outBuff,"<td>NULL</td>");
						//outBuff += sprintf(outBuff,"<td>%02x", lease.mac[0]);
                        //for (i = 1; i < 6; i++)
                        //	outBuff += sprintf(outBuff,":%02x", lease.mac[i]);

						/*ip*/
                        addr.s_addr = lease.ip;
                        expires = ntohl(lease.expires);
                        outBuff += sprintf(outBuff,"<td>%s</td>", inet_ntoa(addr));

						/*lease*/
                        outBuff += sprintf(outBuff,"</td>");
                        d = expires / (24*60*60); expires %= (24*60*60);
                        h = expires / (60*60); expires %= (60*60);
                        m = expires / 60; expires %= 60;
                        outBuff += sprintf(outBuff,"<td>");
                        if (d) 
                        {
                            //printf("lease time is %u days ", d);
                            outBuff += sprintf(outBuff,"%u days ", d);
                        }
                        outBuff += sprintf(outBuff,"%02u:%02u:%02u ",  h, m, (unsigned)expires);
                       	//printf("%02u:%02u:%02u\n", h, m, (unsigned)expires);
                       	outBuff += sprintf(outBuff,"</td>");
                      
                       	outBuff += sprintf(outBuff,"</tr>");
                    }
                    fclose(fp);
                }
              }
        break;     

        case 'B':
        /*
        ** Direct Insertion.  If no value, insert default
        */

        {
		
                     FILE *fp, *f;
                    struct dhcpOfferedAddr 
                    {
                        unsigned char hostname[16];
                        unsigned char mac[16];
                        unsigned long ip;
                        unsigned long expires;
                    } lease;
                    char STAbuf[128];
                    char buf[1024], mac_buf[20], rate_buf[10], rssi_buf[10];
                    int i, j;
                    struct in_addr addr;
                    unsigned long expires;
                    unsigned d, h, m;
                    int num=1;
                    int shi=0;
					int flag=0;
fprintf(errOut,"[luodp] get mac from arp89");
                    Execute_cmd("killall -q -USR1 udhcpd", rspBuff);
					if(strcmp(wifi0_flag,"on") == 0 ) //on
	                    Execute_cmd("wlanconfig ath0 list sta > /var/run/.STAlist 2>&1", rspBuff);

                    fp = fopen(UDHCPD_FILE, "r");  /*  /var/run/udhcpd.leases   */
                    if (NULL == fp)
                    {
                        fprintf(errOut,"\n%s  %d open udhcpd error\n \n",__func__,__LINE__);
                        flag=3;
                    }
					if(flag!=3)
					{fprintf(errOut,"[luodp] get mac from arp6");
						while ((fread(&lease, 1, sizeof(lease), fp) == sizeof(lease))) 
						{
								addr.s_addr = lease.ip;
								//fprintf(errOut,"\nhttpreferer:%s  inet_ntoa(addr) :\n",httpreferer);//,inet_ntoa(addr));
								
								fprintf(errOut,"[luodp] get mac from arp7");
								fprintf(errOut,"[luodp] get mac from arp8");
								if(httpreferer!=NULL)
								{
									fprintf(errOut,"[luodp] get mac from arp5");
									if(!strcmp(httpreferer,(char *)inet_ntoa(addr)))
									{
										if(lease.mac[0])
										{
											outBuff += sprintf(outBuff,"%02x", lease.mac[0]);
											for (i = 1; i < 6; i++)
												outBuff += sprintf(outBuff,":%02x", lease.mac[i]);
											flag=1;
										}
									}
									fprintf(errOut,"[luodp] get mac from arp4");
								}
								fprintf(errOut,"[luodp] get mac from arp3");
						}
						fprintf(errOut,"[luodp] get mac from arp2");
						fclose(fp);
					}
					fprintf(errOut,"[luodp] get mac from arp");
					if(flag==0) //get mac from arp if getmac from dhcp fail
					{
						Execute_cmd("arp -a > /tmp/getmac",rspBuff);
						system("rm /tmp/arplist");
							
						f = fopen("/tmp/getmac","r");
						char *ptr;
            		    char tmp[20]={0};
						if ( !f )
						{
							fprintf(errOut,"\n%s  %d open /etc/arp_ip_mac_on.conf error\n \n",__func__,__LINE__);
							break;
						}
						memset(buf,0,1024);
					    fprintf(errOut,"[luodp] is here");
						while ( fgets(buf,1024,f) != NULL )
						{
							ptr=strstr(buf,httpreferer);
							if(ptr)
							{
								while(*ptr!=')')
								{
									ptr++;
								}
								strncpy(tmp, ptr+5,17);
								fprintf(errOut,"[luodp] getmac %s",tmp);
								outBuff += sprintf(outBuff,"%s", tmp);
							}
							memset(buf,0,1024);
						}
						fclose(f);	
					}
                    
        }
        break;       
        case 'D':
        {
            char *result;
			char rspBuff1[128];
            Execute_cmd("date +'%Y-%m-%d %H:%M:%S'",rspBuff1);
			result=strtok(rspBuff1,"\n");
            outBuff += sprintf(outBuff,"%s",result);
        }
        break;
    }

    return outBuff;
}

/*****************************************************************************
**
** expandOutput
**
** This function checks the input buffer, and replaces all cr/lf or lf
** strings with <br> strings.  Used to "html-ify" output data from an
** embedded command.
**
**  PARAMETERS:
**
**  RETURNS:
**
*/

char *expandOutput(char *inBuff, char *outBuff)
{
    int wasSpace = 0;

    /*
    ** Go until the line has a NULL
    */

    while(*inBuff)
    {
        if ( *inBuff == 0x0a)
        {
            wasSpace = 0;
            inBuff++;
            strcpy(outBuff,"<br>");
            outBuff+=4;
        }
        else if ( *inBuff == 0x0d )
        {
            wasSpace = 0;
            inBuff++;
        }
        else if ( *inBuff == ' ' )
        {
            if(wasSpace)
            {
                strcpy(outBuff,"&nbsp;");
                outBuff+=5;
            }
            else
                wasSpace = 1;

            inBuff++;
        }
        else if ( *inBuff == 0x08 )
        {
            wasSpace = 0;
            strcpy(outBuff,"&nbsp;&nbsp;&nbsp;&nbsp;");
            outBuff+=20;
            inBuff++;
        }
        else
        {
            wasSpace = 0;
            *outBuff++ = *inBuff++;
        }
    }

    return outBuff;
}


/*****************************************************************************
**
** expandLine
**
** This function checks the input provided, and expands any section that is
** market with the special ~ sequences.  These sequences indicate a specific
** action to be taken regarding the parameter that
** is marked with the ~param~ marker.  It returns the a line that has been
** updated with the proper strings.
**
**  PARAMETERS:
**
**  RETURNS:
**
*/

char *expandLine(char *line,char *outBuff)
{
    int     respLen;
    char    paramStr[64];
    char    *p;
    char    *tl;
    int     exeFlag = 0;

    /*
    ** Go until the line has a LF or NULL
    */

    while(*line)
    {
        if ( *line == 0x0a || *line == 0x0d)
        {
            *outBuff++ = *line++;
            break;
        }

        if ( *line == '~')
        {
            /*
            ** This is a special parameter.  The parameter string
            ** will be copied into a temporary string, and passed to
            ** the special parameter processing function.
            */

            p = paramStr;
            line++;
            *p++ =*line++;    /* this is the qualifier character, "always there" */

            if(paramStr[0] == '`')
                exeFlag = 1;    /* This is an executable string */

            while(*line != '~')
            {
                /*
                ** Check for the start of an executable string.  We don't want to process
                ** any of the executable string (gets recursively called)
                */

                if(*line == '`')
                    exeFlag = 1;

                if(exeFlag)
                {
                    /*
                    ** If this is an executable string, then we need to find the termination
                    ** of the executable string.  There are two cases
                    **
                    ** 1: the ~`...`~ case, where exeFlag is determined above, and
                    ** 2: the ~?`...`~ case, where the ~ won't be found until here.
                    */

                    while(*line != '`')
                    {
                        /*
                        ** Look for an early null character.  Abort if so
                        */
                        if(*line == 0)
                        {
                            AbortFlag = 1;
                            return NULL;
                        }
                        *p++ = *line++;
                    }

                    /*
                    ** Terminate exe string processing, but we do want
                    ** to include the last `
                    */

                    exeFlag = 0;
                }

                *p++ = *line++;
            }

            line++; /* Increment past the last ~ */
            *p = 0; /* Null Terimnate, and line now points at "after" the parameter string */

            /*
            ** At this point paramStr contains the full parameter string, ready
            ** for expansion
            */

            outBuff = processSpecial(paramStr,outBuff);

            /*
            ** If an abort flag is raised, return now
            */

            if( AbortFlag)
                return (NULL);
        }
        else
        {
            *outBuff++ = *line++;
        }
    }

    *outBuff = 0;
    return NULL;
}

/*****************************************************************************
**
** Execute_cmd
**
** This function executes the given command, and returns the results in the
** buffer provided.  Usually good for one line response commands
**
**  RETURNS:
**      Output Buffer
**
*/

char *Execute_cmd(char *cmd,char *buffer)
{
    FILE            *f;
    char            *retBuff = buffer;
    char            cmdLine[1024];

    /*
    ** Code Begins
    ** Expand the command in case it contains variables within the command line.
    ** NOTE: THIS IS A RECURSIVE CALL.  DO NOT USE GLOBALS HERE.
    */

    expandLine(cmd,cmdLine);

    /*
    ** Provide he command string to popen
    */

    f = popen(cmdLine, "r");

    if(f)
    {
        /*
        ** Read each line.
        */

        while(1)
        {
            *buffer = 0;
            fgets(buffer,120,f);
            if(strlen(buffer) == 0)
            {
                break;
            }

            strcat(buffer,"<br>");
            buffer += strlen(buffer);
        }

        pclose(f);
    }

    return(retBuff);
}

/*****************************************************************************
**
** setParamValue
**
** This function puts a parameter value into the indicated location, processing
** for %i or %s markers that require the device ID or serial number to be
** inserted.
**
**  PARAMETERS:
**
**  RETURNS:
**
*/

void setParamValue(char *targ,char *val,int check)
{
    int     index = 0;

    /*
    ** Code begins.
    ** Assume the value is null terminated
    */

    while(*val)
    {
		/*
		** If the check flag is set, we need to see if ANY character
		** is changed when putting into the variable.
		*/

		if(check && *val != targ[index])
			FactoryDefault = 0;

        if(*val == 0x0a || *val == 0x0d)
        {
            /*
            ** line feed or carrage return.  This should be truncated, end of string
            */

            break;
        }
        else
            targ[index++] = *val;

        val++;
    }

    targ[index] = 0;    // Insert null terminator
}

/******************************************************************************/
/*!
**  \brief Determine if WEP key is hex
**
**  This routine will process the ASCII representation of the WEP key, and
**  determine if it is correctly formatted.  If it is hex, will return a 1
**  If it is properly formatted and not hex, will return a 0.  If improperly
**  formatted, returns -1
**
**  \param key Null terminated character string containing the WEP key ASCII
**  \return 1 on key is formatted as HEX
**  \return 0 on key is formatted as ASCII
**  \return -1 on incorrectly formatted key
*/

int isKeyHex(char *key, int flags)
{
	int		len;
	int		i;
	int		wep = flags & 0x01;
	int		silent = flags & 0x02;

	/*
	** Get the key length (in characters)
	*/

	len = strlen(key);

	/*
	** Return 0 if the string is blank, this is OK if the fields are not
	** filled out -- will be caught later
	*/

	if(len == 0)
		return(0);

	/*
	** This works for both WEP and WPA keys.  If the WEP flag is set, only
	** check for WEP lengths.  If not, only check for WPA
	*/

	if(wep)
	{
    	/*
    	** First Pass, if the length is 0, 5, or 13 it's a string.  Can be anything
    	** Also, automatically the corresponding "IS HEX" value
    	*/

    	if(len == 5 || len == 13 || len == 16)
			return (0);
	}
	else
	{
		if(len > 7 && len < 64)
			return (0);
	}


    /*
    ** If it's not a string, then we need to determine if it's the proper length
    */

    if(wep)
    {
    	if((len != 10) && (len != 26) && (len != 32))
    	{
    	    if(!silent)
    	    	modePrintf("'%s' invalid WEP Key length (length %d != 5,13,16 for ASCII or 10,26,32 for HEX)",
    	    		   key,len);
    	    return (-1);
    	}
	}
	else
	{
   		if(len != 64)
		{
    	    if(!silent)
                modePrintf("'%s' invalid lendth (length %d != 8 to 63 chars)",key,len);
    	    return (-1);
		}
	}

	/*
	** Make sure all characters are valid hex characters
	*/

    for(i=0;i<len;i++)
    {
        if(!isxdigit(key[i]))
        {
    	    if(!silent)
                modePrintf("'%s' has non hex digit, digit='%c' pos=%d",key,key[i],i);
            return (-1);
        }
    }
	return (1); // is Hex
}

/******************************************************************************/
/*!
**  \brief Determine if WPS PIN is valid
**
**  This routine will process the Integer representation of the WPS PIN, and
**  determine if its check sum is correctly formatted.  If it is vaild, will return a 1
**  otherwise, it will return a 0.  
**
**  \param Eight digit PIN (i.e., including the checksum digit)
**  \return 1 if check sum value is valid
**  \return 0 if not
*/
int isWPSPinValid(int digit_pin)
{
    int accum = 0;
    int pin_check_sum = 0;
    int pin = digit_pin / 10;
	while (pin) {
		accum += 3 * (pin % 10);
		pin /= 10;
		accum += pin % 10;
		pin /= 10;
	}
    
    pin_check_sum = (10 - accum % 10) % 10;
    
    return pin_check_sum == (digit_pin % 10);
}

/*****************************************************************************
**
** CheckValue
**
** Performs input validation on fields that must conform to certain values.
**
**  PARAMETERS:
**
**  RETURNS: 
**
*/


int CheckValue(char *name,char *val)
{
    int     len;
    int     i;
    char    ParamName[32];

    /*
    ** Code Begins
    ** Check for the fields of interest
    **
    ** SSID.  Must be 2 to 32 characters long, certain characters
    ** now allowed.
    */

    if(!strncmp(name,"AP_SSID",7))
    {
        len = strlen(val);

        if(len > 32)
        {
            modePrintf("%s=%s invalid lendth (length %d > 32 chars)",name,val,len);
            return (-1);
        }
    }

    /*
    ** PSK Key
    ** If Defined, it must conform to the 8 to 64 character rule
    */

    if(!strncmp(name,"PSK_KEY",7) && (strlen(name) < 10))
    {
		/*
		** Get the status value
		*/

		i = isKeyHex(val,KEY_IS_WPA);

		if(i < 0)
			return (-1);	// Invalid key format
    }

    /*
    ** WEP_KEY
    ** Perform automatic determination of Hex or String.  String values are 5,13, or 16
    ** characters where hex values are 10, 26, or 32 characters.  No other lengths allowed.
    */

    if(!strncmp(name,"WEPKEY",6))
    {
		/*
		** Get the status value
		*/

		i = isKeyHex(val,KEY_IS_WEP);

		if(i < 0)
			return (-1);	// Invalid key format
    }

    if(!strncmp(name,"AP_VLAN",7))
    {
        if(val)
        {
            if (strlen(val) == 0 )
                return 0;/* Emptry string, no error */
            if ( isNumericOnly(val) != 0 ){
                modePrintf("invalid vlan tag value, 1 <= vlan <= 4094");
                return -1;
            }
            int tagval = atoi(val);
            /* tag should be between 1-4094. Linux uses 4095 for its
             * internal use 
             */
            if((tagval >= 1 && tagval <= 4094))
            {
                return 0;
            }
            else
            {
                modePrintf("invalid vlan tag value, 1 <= vlan <= 4094");
                return -1;
            }
        }
    }

    if(!strncmp(name,"WSC_PIN",7))
    {
        if(val)
        {
            if(strlen(val) != 8)
            {
                return -1; 
            }

            int pinval = atoi(val);
            if (isWPSPinValid(pinval))
            {
                return 0;
            } 
            else
            {
                modePrintf("%s=%s WPS PIN check sum digit is invalid", name, val);            
                return -1;
            }
        }
    }
    return (0); // Parameter is OK.
}


/*****************************************************************************
**
** /brief writes parameters to file
**
** This function will write the save parameter list to the file pointer
** provided.  It is assumed that the file pointer is open and positioned to
** the correct location.  File opening and closing is the responsibility of
** the caller.
*/

void writeParameters(char *name,char *mode,unsigned long offset)
{
    int         i;
    FILE        *f;
    u_int32_t   Sync = 0xfaf30020;
    u_int32_t   Zero = 0;
#ifdef ATH_SINGLE_CFG
    int         nvram_write=0;
#endif

    /*
    ** Code Begins
    ** The name of the file, and the offset into the file are passed as parameters.
    ** This will return an error (-1) if the file does not exist
    */

    if(!FactoryDefault)
        Sync |= NOT_FACTORY_DEFAULT;

    f = fopen(name,mode);

    if(f)
    {
#ifdef ATH_SINGLE_CFG
        if (!strcmp(name,NVRAM)) {
            nvram_write = 1;

            /*
             * For factory reset (before erase) don't try to
             * save the wps cfg available in the nvram
             */

            if (athcfg_prepare_nvram(f, name,
                                     (config.numParams == 0 ? 0 : 1)) != 0) {
                fclose(f);
                return;
            }
        }
#endif
        /*
        ** If an offset is provided, seek to the offset
        */

        if(offset != 0)
            fseek( f, offset, SEEK_SET);

        /*
        ** Start writing the file.  Write sync word, then parameters
        */

        fwrite(&Sync,4,1,f);

        for(i=0;i<config.numParams;i++)
        {
			//case luodp
            /*
            ** We don't want to store the "update" or "commit" parameters, so
            ** remove them if we get here.  Also, if we have values that have
            ** no value, don't write them out.
            DHCP SIP PPP L2TP P2TP WIRRE WIRELESS DHCPW SIPW PPPW L2TPW P2TPW ADMINSET
            */
		    if( !strcmp(config.Param[i].Name,"MOD_SDHCP") )
                continue;
            if( !strcmp(config.Param[i].Name,"MOD_RU") )
                continue;
            if( !strcmp(config.Param[i].Name,"MOD_IPMAC") )
                continue;
            if( !strcmp(config.Param[i].Name,"MOD_PRC") )
                continue;
            if( !strcmp(config.Param[i].Name,"ON_OFF") )
                continue;
            //if( !strcmp(config.Param[i].Name,"MODXXX") )
            //    continue;
			if( !strcmp(config.Param[i].Name,"TIME_SYNC") )
                continue;
			if( !strcmp(config.Param[i].Name,"SAV") )
                continue;
            if( !strcmp(config.Param[i].Name,"ACT") )
                continue;
            if( !strcmp(config.Param[i].Name,"DEL_PRC") )
                continue;
            if( !strcmp(config.Param[i].Name,"DEL_CON") )
                continue;
            if( !strcmp(config.Param[i].Name,"DEL_RU") )
                continue;
            if( !strcmp(config.Param[i].Name,"DEL_IPMAC") )
                continue;
            if( !strcmp(config.Param[i].Name,"MAC_CLONE") )
                continue;
			if( !strcmp(config.Param[i].Name,"MAC_BACK") )
                continue;
            if( !strcmp(config.Param[i].Name,"DHCP_SET") )
                continue;
            if( !strcmp(config.Param[i].Name,"ADD_STATICDHCP") )
                continue;
            if( !strcmp(config.Param[i].Name,"GW_SET") )
                continue;
            if( !strcmp(config.Param[i].Name,"RULIST_SET") )
                continue;
            if( !strcmp(config.Param[i].Name,"ADD_STATICR") )
                continue;
            if( !strcmp(config.Param[i].Name,"CONM_WORK") )
                continue;
            if( !strcmp(config.Param[i].Name,"WEBCONM_WORK") )
                continue;
            if( !strcmp(config.Param[i].Name,"ADD_CONRULE") )
                continue;
            if( !strcmp(config.Param[i].Name,"WIRELESS_ADVSET") )
                continue;
            if( !strcmp(config.Param[i].Name,"IPMAC_SET") )
                continue;
            if( !strcmp(config.Param[i].Name,"ADD_IPMACBIND") )
                continue;
            if( !strcmp(config.Param[i].Name,"ADD_PARC") )
                continue;
            if( !strcmp(config.Param[i].Name,"TIMEZONE_SET") )
                continue;
            if( !strcmp(config.Param[i].Name,"LOCAL_SAVE") )
                continue;
            if( !strcmp(config.Param[i].Name,"CFG_MODIFY_USE") )
                continue;
            if( !strcmp(config.Param[i].Name,"CFG_MODIFY_DEL") )
                continue;
            if( !strcmp(config.Param[i].Name,"NETCHECK") )
                continue;
			if( !strcmp(config.Param[i].Name,"DEL_SDHCP") )
                continue;
            if( !strcmp(config.Param[i].Name,"REBOOT") )
                continue;
            if( !strcmp(config.Param[i].Name,"DHCP") )
                continue;
            if( !strcmp(config.Param[i].Name,"SIP") )
                continue;
            if( !strcmp(config.Param[i].Name,"PPP") )
                continue;
            if( !strcmp(config.Param[i].Name,"L2TP") )
                continue;
            if( !strcmp(config.Param[i].Name,"P2TP") )
                continue;
            if( !strcmp(config.Param[i].Name,"WIRRE") )
                continue;
            if( !strcmp(config.Param[i].Name,"WIRELESS") )
                continue;
            if( !strcmp(config.Param[i].Name,"DHCPW") )
                continue;
            if( !strcmp(config.Param[i].Name,"SIPW") )
                continue;
            if( !strcmp(config.Param[i].Name,"PPPW") )
                continue;
            if( !strcmp(config.Param[i].Name,"L2TPW") )
                continue;
            if( !strcmp(config.Param[i].Name,"P2TPW") )
                continue;
			if( !strcmp(config.Param[i].Name,"FACROTY") )
                continue;
            if( !strcmp(config.Param[i].Name,"ADMINSET") )
                continue;
			if( !strcmp(config.Param[i].Name,"WFILIST") )
                continue;
            if( !strcmp(config.Param[i].Name,"INDEX") )
                continue;
            if( config.Param[i].Val[0] == 0)
                continue;

            fprintf(f,"%s=%s\n",config.Param[i].Name,config.Param[i].Val);
        }

        fwrite(&Zero,4,1,f);

#ifdef ATH_SINGLE_CFG

        /*
         * For factory reset, there is no need to write back the
         * wps cfg to nvram
         */

        if (config.numParams != 0 && nvram_write == 1) {
            athcfg_save_wps_cfg_to_nvram(NULL, f, 1);
        }
#endif
        /* force the buffers to be flushed to the storage device */
        fsync(fileno(f));

        fclose(f);
    }
}


int CFG_set_by_name_sync(char *name,char *val)
{
    int     i;
	int		check = 1;

	char	*Dont_Check[] = {"WPS_ENABLE","AP_SSID",0};

    if( CheckValue(name, val) )
        return (-1);


	i = 0;
	while(Dont_Check[i])
	{
		if(!strncmp(name, Dont_Check[i], strlen(Dont_Check[i++])) )
		{
			check = 0;
			break;
		}
	}


    for( i=0; i < config.numParams; i++ )
    {
        if( !strcmp(config.Param[i].Name,name) )
        {

            setParamValue(config.Param[i].Val, val, check);
            return (0);     // Done
        }
    }

    if(config.numParams < MAX_WLAN_ELEMENT)
    {
        strcpy(config.Param[config.numParams].Name,name);
        setParamValue(config.Param[config.numParams++].Val,val,0);
    }

    return (0);
}


void writeParametersWithSync(void)
{
    FILE *f;
    int i;
    int fd;

    fd = open("/tmp/br0", O_RDWR);
    flock(fd, LOCK_EX);

    memset(&config,0,sizeof(config));
    
    //fprintf(errOut,"%s  %d\n",__func__,__LINE__);
    f = fopen("/tmp/.apcfg","r");

    if ( !f )
    {

        f = fopen( NVRAM, "r" );

        if (!f)
        {
            printf("ERROR:  %s not defined on this device\n", NVRAM);
            printf("ERROR:  Cannot store data in flash!!!!\n");
            exit(-1);
        }

        fseek(f, NVRAM_OFFSET, SEEK_SET);
    }
    if ( f )
    {
        fillParamStruct(f);
        fclose(f);
    }

    for(i=0;i<config_sync.numParams;i++)
    {
          CFG_set_by_name_sync(config_sync.Param[i].Name,config_sync.Param[i].Val);
  //        printf("config_sync %s:=%s\n",config_sync.Param[i].Name,config_sync.Param[i].Val);
    }


            writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            writeParameters("/tmp/.apcfg","w+",0);			


     flock(fd, LOCK_UN);
     close(fd);

}

/*****************************************************************************
**
** CFG_set_by_name
**
** This will set the parameter specified by name to the indicated value.  Values
** are always strings.
**
**  PARAMETERS:
**          name        Name of the parameter to modify
**          val         New value for the parameter
**
**  RETURNS:
**          0 on success, -1 on failure
**
*/

int CFG_set_by_name(char *name,char *val)
{
    int     i;
	int		check = 1;

	/*
	** List of parameters NOT to check for changes, since changing them
	** will not affect the state of WPS configured/not configured
	*/

	char	*Dont_Check[] = {"WPS_ENABLE","AP_SSID",0};

	/*
	** Code Begins
	** Check the value against the parameter name for special processing
	*/

    if( CheckValue(name, val) )
        return (-1);

	/*
	** Determine if this parameter should be checked to see if it is
	** changed from factory default
	*/

	i = 0;
	while(Dont_Check[i])
	{
		if(!strncmp(name, Dont_Check[i], strlen(Dont_Check[i++])) )
		{
			check = 0;
			break;
		}
	}

    /*
     * set new parameter to struct config_sync
     */
    if(config_sync.numParams < MAX_WLAN_ELEMENT)
    {
        strcpy(config_sync.Param[config_sync.numParams].Name,name);
        setParamValue(config_sync.Param[config_sync.numParams++].Val,val,0);
    }

	/*
	** Now, search the list and get the proper slot for this
	** parameter
	*/

    for( i=0; i < config.numParams; i++ )
    {
        if( !strcmp(config.Param[i].Name,name) )
        {
            /*
            ** This is der one.
            */

            setParamValue(config.Param[i].Val, val, check);
            return (0);     // Done
        }
    }

    /*
    ** If we get here, we did not find the item.  Insert as a new one
    */

    if(config.numParams < MAX_WLAN_ELEMENT)
    {
        strcpy(config.Param[config.numParams].Name,name);
        setParamValue(config.Param[config.numParams++].Val,val,0);
    }

    return (0);
}

/*****************************************************************************
**
** CFG_get_by_name
**
** This function gets the parameters from the config structure
**
**  PARAMETERS:
**
**  RETURNS: 
**
*/

char *CFG_get_by_name(char *name,char *buff)
{
    int     i;

    *buff = '\0';  // null terminate

    for(i=0;i<config.numParams;i++)
    {
        if(!strcmp(config.Param[i].Name,name))
        {
            /*
            ** This is der one.  Find out if there is a %s or %i
            ** in the stream.  If so, insert the proper value
            */

            strcpy(buff,config.Param[i].Val);
            break;
        }
    }

    return buff;     // Done
}

/*****************************************************************************
**
** CFG_remove_by_name
**
** This function removes a parameter from the config structure
**
**  PARAMETERS:
**
**  RETURNS: 
**
*/

void CFG_remove_by_name(char *name)
{
    int     i;


    for(i=0;i<config.numParams;i++)
    {
        if(!strcmp(config.Param[i].Name,name))
        {
            /*
            ** This is the one.  Move the rest of the items on the list
            ** "up" by one, and decrement the total number of
            ** parameters
            */

            for(i++;i<config.numParams;i++)
            {
                strcpy(config.Param[i-1].Name,config.Param[i].Name);
                strcpy(config.Param[i-1].Val,config.Param[i].Val);
            }
            config.numParams--;
            return;
        }
    }
}

/*****************************************************************************
**
** extractVal
**
** This function returns both the value name and value string for the next item in
** the list.  It returns a pointer to the next value, or NULL if the string has
** "run out".
**
**  PARAMETERS:
**
**  RETURNS:
**
*/

char *extractParam(char *str,char *Name,char *Val)
{
    int     param=0;
    int     val = 0;

    /*
    ** Code Begins
    ** Search for the ? or & to start the string
    */

    while(*str)
    {
        /*
        ** Check for the beginning ? or &.  This signifies the start or
        ** end of the parameter. start is null at the start
        */

        if(*str == '?' || *str=='&' || *str == 0x0a || *str == 0x0d )
        {
            if(!param)
            {
                param = 1;
            }
            else
            {
                /*
                ** All Done.  Return this pointer
                */

                *Val = 0;
                return (str);
            }
        }
        else if(*str == '=')
        {
            val = 1;
            *Name = 0;  // Null terminate
        }
        else if(!val)
        {
            param = 1;
            *Name++ = *str;
        }
        else
            *Val++ = *str;

        str++;
    }

    /*
    ** If we get here, we have run out before getting a complete
    ** parameter. End of the line
    */

    return (NULL);
}

/******************************************************************************/
/*!
**  \brief converts strings to HTML format
**
**  This function will translate HTML special characters to the equivalent
**  HTML form for display.  This is only required for display purposes,
**  not used for command line optiosn.
**
**  \param src Pointer to source string
**  \param dest Pointer to destination string
**  \return N/A
*/

void htmlEncode(char *src, char *dest)
{
    /*
    ** Code Begins
    ** Search for special characters to do encoding as required
    */

    while(*src)
    {
        switch (*src)
        {
        case 0x22:  // Quote Character
            dest += sprintf(dest,"&quot;");
            break;

        case '&':
            dest += sprintf(dest,"&amp;");
            break;

        case '>':
            dest += sprintf(dest,"&gt;");
            break;

        case '<':
            dest += sprintf(dest,"&lt;");
            break;

        default:
            *dest++ = *src;
        }
        src++;
    }
    /*
    ** Put in the terminal null for the destination
    */

    *dest = 0;
}

/******************************************************************************/
/*!
**  \brief converts strings to "shell safe" format
**
**  This function will translate strings into "shell safe" forms that can be
**  exported.  It will detect special characters and "escape" them as required.
**  Note that some characters cannot be escaped, so this will not always
**  work.  The return value will indicate whether the string should be
**  enclosed in quotes or not.
**
**  \param src Pointer to source string
**  \param dest Pointer to destination string
**  \return 0 for no quotes required
**  \return 1 for quotes required.
*/

int shellEncode(char *src, char *dest)
{
    int needQuot = 0;

    /*
    ** Code Begins
    ** Search for special characters to do encoding as required
    */

    while(*src)
    {
        switch (*src)
        {
        case 0x22:  //Quote Character
            dest += sprintf(dest,"\\\"");
            needQuot = 1;
            break;
        default:
            *dest++ = *src ;
        }

         if(((*src < '0') || (*src > '9')) &&
        ((*src < 'a') || (*src > 'z')) &&
            ((*src < 'A') || (*src > 'Z')))
          {
            needQuot = 1;
          }

         src++;
         }

      /*
      ** Put in the terminal null for the destination
      */

      *dest =0;
      return(needQuot);
}

/******************************************************************************/
/*!
**  \brief decodes a string from HTML format (%hex)
**
**  This function will translate HTML special characters from the HTML form
**  of %xx to the equivalent 8 bit character.  Used to process input from
**  GET/POST transactions.
**
**  \param src Pointer to source string
**  \param dest Pointer to destination string
**  \return N/A
*/

void unencode(char *src, char *dest)
{
    for(; *src != '\0'; src++)
    {
        if(*src == '+') 
        {
            *dest = ' ';
            dest++;
        }
        //decoding special symbols
        else if(*src == '%') 
        {
            int code;
            if(sscanf(src+1, "%2x", &code) != 1) code = '?';
            // ignoring all newline symbols that come from form - we put our own instead of "&varname1=" --> '\n'
            if(code != 10 && code != 12 && code != 13)
            {
                *dest = code;
                dest++;
            }
            src +=2;
        }
        else
        {
            *dest = *src;
            dest++;
        }
    } //outer for loop

    *dest = 0;
}

/*****************************************************************************
**
** /brief translateFile
**
** This function will read a provided file name, and output the file with
** any substutions included.  This is used to translate template files into
** specific files that have required parameters included.
**
** An optional "index" variable will be used to look for parameters that
** have a specific index (such as AP_SSID_2, etc).  If no index is specified,
** then the parameter is assumed to be not there.  If the index is specified,
** then parameters with a tailing "_#" will have # replaced with the parameter
** ID.
**
**  \param fname File pointer to input file to translate.
**  \return 0 for success
**  \return -1 on error
*/

int translateFile(char *fname)
{
    char            Name[32];
    char            Value[64];
    char            line[1024];
    FILE            *f;

    /*
    ** Code Begins.
    ** Input the parameter cache for processing
    */

    f = fopen(fname,"r");

    if ( !f )
    {
        return (-1);
    }

    /*
    ** Read the file, one line at a time.  If the line is aborted, then
    ** dump the line and continue
    */

    while(!feof(f))
    {
        line[0] = 0;
        fgets(line,1024,f);
        expandLine(line,opBuff);

        if( !AbortFlag )
            printf("%s",opBuff);
        else
            AbortFlag = 0;

        opBuff[0] = 0;  // clear the buffer for the next cycle

    }

    fclose ( f );

    return (0);
}

/*****************************************************************************
**
** /brief getRadioID
**
** This function determine if the radio ID is defined as specified by
** the index value.
**
**  \param index	index of the VAP to check.
**  \return radioID	On success
**  \return -1 on error
*/


int getRadioID(int index)
{
    char    varname[32];
    char    valBuff[32];
    int     len;

//    fprintf(errOut,"%s: Index set to %d\n",__func__,parameterIndex);
    if(index > 1)
        sprintf(varname,"AP_RADIO_ID_%d",parameterIndex);
    else
        strcpy(varname,"AP_RADIO_ID");

    valBuff[0] = 0;
//    fprintf(errOut,"%s: Getting %s\n",__func__,varname);

    CFG_get_by_name(varname,valBuff);

    /*
    ** Only process if a non-null string is returned.  This is to protect the
    ** single radio implementations that don't need the radio ID variable.
    */

    if(strlen(valBuff))
    {
        return(atoi(valBuff));
	}
	else
		return (-1);
}


/*begin :  wangyu add for dhcp server operation */
/*
**dhcp server setting function
*/
int set_dhcp(void)
{ 
		fprintf(errOut,"\n%s  %d WIRRE \n",__func__,__LINE__);		
		char valBuff1[128]; 
				
		CFG_get_by_name("DHCPON_OFF",valBuff1);
		//save new config to flash 
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
		//off
		if(strcmp(valBuff1,"off") == 0 )
		{
			//if( CFG_set_by_name(valBuff2,"off") )
			//exit(-1);
			Execute_cmd("killall udhcpd > /dev/null 2>&1",rspBuff);
		}
		else if(strcmp(valBuff1,"on") == 0 ) //on
		{
			Execute_cmd("killall udhcpd > /dev/null 2>&1",rspBuff);
			//Execute_cmd("/etc/ath/apcfg ; /etc/rc.d/rc.udhcpd",rspBuff);
			//Execute_cmd("/etc/ath/apcfg",rspBuff);
			//system("/etc/rc.d/rc.udhcpd");
			Execute_cmd("/etc/rc.d/rc.udhcpd",rspBuff);
			Execute_cmd("/usr/sbin/set_addr_conf > /dev/null 2>&1",rspBuff);
			Execute_cmd("/usr/sbin/udhcpd /etc/udhcpd.conf",rspBuff);
		}
		return 0;
}
/*
**dhcp server ip and mac address bonding  add function
*/


int set_addr_bind(void)
{ 
		#ifdef MESSAGE
			fprintf(errOut,"\n%s  %d set_addr_bind \n",__func__,__LINE__);
		#endif		
		char valBuff1[128]; 
		char valBuff2[128]; 
		char valBuff3[128];
		char valBuff4[128];		
		int result = 0;		
				
		CFG_get_by_name("ADD_MAC",valBuff1);
		CFG_get_by_name("ADD_IP",valBuff2);
		CFG_get_by_name("ADD_STATUS",valBuff3);
		//off
		
		#ifdef MESSAGE
		fprintf(errOut,"%s  %d  valBuff1 %s valBuff2 %s valBuff3 %s\n",__func__,__LINE__,
				valBuff1,valBuff2,valBuff3);
		#endif

        //viqjeee
        writeParametersWithSync();
		//save new config to flash 
        //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
        //writeParameters("/tmp/.apcfg","w+",0);
		//TODO if close do commit no ath1
		
		Execute_cmd("killall udhcpd > /dev/null 2>&1",rspBuff);
		//Execute_cmd("/etc/ath/apcfg ; /etc/rc.d/rc.udhcpd",rspBuff);
		//Execute_cmd("/etc/ath/apcfg",rspBuff);
		//system("/etc/rc.d/rc.udhcpd");
		Execute_cmd("/etc/rc.d/rc.udhcpd",rspBuff);
		
		Execute_cmd("/usr/sbin/set_addr > /tmp/set_addr.log 2>&1",rspBuff);
		
		FILE *fileBuf2=NULL;
		if ((fileBuf2= fopen("/tmp/set_addr.log", "r")) == NULL)
		{
			fprintf(errOut,"%s	%d File open error.Make sure you have the permission.\n",__func__,__LINE__);
		}else
		{
			fgets(valBuff4,sizeof(valBuff4),fileBuf2);
			if(strstr(valBuff4,"the address is exist")!=NULL)
			{
				fprintf(errOut,"%s	%d	the address is exist...\n",__func__,__LINE__);///sjin
                printf("HTTP/1.0 200 OK\r\n");
                printf("Content-type: text/html\r\n");
                printf("Connection: close\r\n");
                printf("\r\n");
                printf("\r\n");
                
                printf("<HTML><HEAD>\r\n");
                printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
                printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
                printf("</head><body>");
                printf("<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"lan\");window.parent.DialogHide();alert(_(\"err IPMAC exist\"));window.location.href=\"ad_local_addsdhcp?ad_local_addsdhcp=yes\";</script>");
                printf("</body></html>");
                result = 1;
			}
			fclose(fileBuf2);
		}
		
		Execute_cmd("/usr/sbin/udhcpd /etc/udhcpd.conf",rspBuff);
		return result;
}

int modify_addr_bind(void)
{					
		char tmp[128];
		char valBuff[128];
		CFG_get_by_name("ON_OFF",valBuff);
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
		
		Execute_cmd("killall udhcpd > /dev/null 2>&1",rspBuff);
		sprintf(tmp,"/usr/sbin/modify_addr %s > /dev/null 2>&1",valBuff);
		Execute_cmd(tmp,rspBuff);
		Execute_cmd("/usr/sbin/udhcpd /etc/udhcpd.conf",rspBuff);
		return 1;
}

int del_addr_bind(void)
{
	fprintf(errOut,"\n%s  %d del_addr_bind \n",__func__,__LINE__);

	char valBuff1[128]; 
	char valBuff2[128]; 
	char valBuff3[128]; 

	//CFG_get_by_name("DELXXX",valBuff1);
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	Execute_cmd("killall udhcpd > /dev/null 2>&1",rspBuff);

	Execute_cmd("/usr/sbin/del_addr > /dev/null 2>&1",rspBuff);
	
	Execute_cmd("/usr/sbin/udhcpd /etc/udhcpd.conf",rspBuff);
	return 0;
}
/*end :  wangyu add for dhcp server operation */

//luodp route rule
void add_route_rule(void)
{
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	Execute_cmd("/usr/sbin/set_route > /dev/null 2>&1",rspBuff);
} 
void del_route_rule(void)
{
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	Execute_cmd("/usr/sbin/del_route > /dev/null 2>&1",rspBuff);
}

void modify_route_rule(void)
{
	char tmp[128];
	char valBuff[128];
	CFG_get_by_name("ON_OFF",valBuff);
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	sprintf(tmp,"/usr/sbin/modify_route %s > /dev/null 2>&1",valBuff);
	Execute_cmd(tmp,rspBuff);
}

int add_arp(void)
{
	char valBuff[128];
	char valBuff2[128];
	char valBuff4[128];
	char buf[128];	
	char arp_mac[20];
	char arp_ip[20];
	int result = 0;
	FILE *fp;
	char *gateIp, *gateMask;
	int ret, ret2;
	struct in_addr addr_ip, addr_mask;
	struct in_addr addr_ip2, addr_mask2;
	
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);


	Execute_cmd("cfg -e | grep AP_IPADDR= | awk -F '=' '{print $2}'",valBuff);
	Execute_cmd("cfg -e | grep \"AP_NETMASK=\" | awk -F \"=\" \'{print $2}\'",valBuff2);
	CFG_get_by_name("ADD_MAC",arp_mac);
	CFG_get_by_name("ADD_IP",arp_ip);
	
	gateIp = strtok(valBuff, "\"");
	gateMask = strtok(valBuff2, "\"");
	//fprintf(errOut,"the gateIp is [%s] \n", gateIp);
	//fprintf(errOut,"the gateMask is [%s] \n", gateMask);
	if(inet_aton(gateIp, &addr_ip) && inet_aton(gateMask, &addr_mask))
	{
		ret = addr_ip.s_addr&addr_mask.s_addr;
		//fprintf(errOut,"the ret is [%x] \n", ret);
	}
	if(inet_aton(arp_ip, &addr_ip2) && inet_aton(gateMask, &addr_mask2))
	{
		ret2 = addr_ip2.s_addr&addr_mask2.s_addr;
		//fprintf(errOut,"the ret2 is [%x] \n", ret2);
	}

	if(ret == ret2)
	{
		result = 0;
	}
	else
	{
		result = 1;
		goto error;
	}
	
	
	system("arp -a > /tmp/arplist");
	fp = fopen("/tmp/arplist", "r");
	while(fgets(buf, 128, fp))
	{
		if(strstr(buf, arp_mac))
		{
			result = 0;
			if(strstr(buf, arp_ip))
			{
				break;
			}
			result = 1;
			break;
		}
		else
		{
			if(strstr(buf, arp_ip) && !strstr(buf, "incomplete"))
			{
				result = 1;
				break;
			}
			result = 0;
		}
	}
	fclose(fp);

error:
	if(result == 1)
	{
		/*bound error*/
		printf("HTTP/1.0 200 OK\r\n");
        printf("Content-type: text/html\r\n");
        printf("Connection: close\r\n");
        printf("\r\n");
        printf("\r\n");

        printf("<HTML><HEAD>\r\n");
        printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
        printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
        printf("</head><body>");
        printf("<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"lan\");window.parent.DialogHide();alert(_(\"err IPMAC exist\"));window.location.href=\"ad_safe_IPMAC?ad_safe_IPMAC=yes\";</script>");
        printf("</body></html>");
		return result;
	}
	
	Execute_cmd("/usr/sbin/set_arp > /tmp/set_arp.log 2>&1",rspBuff);
	
	FILE *fileBuf2=NULL;
	if ((fileBuf2= fopen("/tmp/set_arp.log", "r")) == NULL)
	{
		fprintf(errOut,"%s	%d File open error.Make sure you have the permission.\n",__func__,__LINE__);
	}else
	{
		fgets(valBuff4,sizeof(valBuff4),fileBuf2);
		if(strstr(valBuff4,"the address is exist")!=NULL)
		{
				fprintf(errOut,"%s	%d	the address is exist...\n",__func__,__LINE__);///sjin

		        printf("HTTP/1.0 200 OK\r\n");
		        printf("Content-type: text/html\r\n");
		        printf("Connection: close\r\n");
		        printf("\r\n");
		        printf("\r\n");
        
                printf("<HTML><HEAD>\r\n");
                printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
                printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
                printf("</head><body>");
                printf("<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"lan\");window.parent.DialogHide();alert(_(\"err IPMAC exist\"));window.location.href=\"ad_safe_IPMAC?ad_safe_IPMAC=yes\";</script>");
                printf("</body></html>");
                result = 1;
		}
		fclose(fileBuf2);
	}
	return result;
}
void del_arp(void)
{
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	Execute_cmd("/usr/sbin/del_arp > /dev/null 2>&1",rspBuff);
}

void modify_arp(void)
{
	char tmp[128];
	char valBuff[128];
	CFG_get_by_name("ON_OFF",valBuff);
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	sprintf(tmp,"/usr/sbin/modify_arp %s > /dev/null 2>&1",valBuff);
	Execute_cmd(tmp,rspBuff);
}

void add_backup(void)
{
	char pChar[128];
	char valBuff1[128];
	errOut = fopen("/dev/ttyS0","w");
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	CFG_get_by_name("SAV",valBuff1);
	fprintf(errOut,"\n%s  %d valBuff1 is %s \n",__func__,__LINE__, valBuff1);
	sprintf(pChar,"/usr/sbin/set_backup %s > /dev/null 2>&1",valBuff1);
	Execute_cmd(pChar,rspBuff);
}
void del_backup(void)
{
	char pChar[128];
	char valBuff1[128];
	CFG_get_by_name("ACT",valBuff1);
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	sprintf(pChar,"/usr/sbin/del_backup %s > /dev/null 2>&1",valBuff1);
	Execute_cmd(pChar,rspBuff);
	CFG_set_by_name(valBuff1,"");
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
}
void use_backup(void)
{
	char valBuff1[128];
	char cmdd[128];
	char buf[128];
	char *bakupName;
	int i;
	FILE *fp;
	errOut = fopen("/dev/ttyS0","w");
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	CFG_get_by_name("ACT",valBuff1);
	
    #if 0
	printf("Content-Type:text/html\n\n");
	printf("<HTML><HEAD>\r\n");
	printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
	printf("</head><body>");
	printf("<div style=\"font-size: 14pt; font-weight: bold; margin-left: 10px; font-family: 寰蒋闆呴粦, Arial, Helvetica, sans-serif; color: #848484;border-bottom:1px dotted #d0d0d0; padding-bottom:10px; margin-bottom:10px;height:30px; line-height:30px; padding:5px;\">閰嶇疆绠＄悊</div>\n");	
	printf("<p align=\"center\" style=\"font-size: 9pt; margin-left: 10px; font-family: 寰蒋闆呴粦, Arial, Helvetica, sans-serif; color: #848484\">鎭㈠閰嶇疆瀹屾垚,姝ｅ湪閲嶅惎BASE..........</p><br>\n");	
	printf("<p align=\"center\" style=\"font-size: 9pt; margin-left: 10px; font-family: 寰蒋闆呴粦, Arial, Helvetica, sans-serif; color: #848484\">Import the configuration file was completed, restartting BASE..........</p><br>\n");	
	printf("<script  language=javascript>setTimeout(function(){window.top.location.href=\"index.html\";},140000);</script>");
	printf("</body></html>");	
    #endif
	//reconfig
    Reboot_tiaozhuan("cfgback","index.html");

	if((fp = fopen("/etc/backup/backup_list.conf", "r")) != NULL)
	{
		while(fgets(buf, 128, fp))
		{
			if(!strncmp(buf, valBuff1, strlen(valBuff1) ))
			{
				for(i = strlen(valBuff1)+1; ; i++)
				{
					if(!strncmp(&buf[i], "20", 2))
						break;
				}
				//fprintf(errOut,"\n%s  %d strlen(&buf[i]) is %d \n",__func__,__LINE__, strlen(&buf[i]));
				bakupName = malloc(strlen(&buf[i]));
				strncpy(bakupName, &buf[i], strlen(&buf[i]) - 1);
				break;
			}
		}
		fclose(fp);
	}
	
	//fprintf(errOut,"\n%s  %d bakupName is %s the size is %d\n",__func__,__LINE__, bakupName, strlen(bakupName));
	sprintf(cmdd,"dd if=/etc/backup/%s.bin of=/dev/caldata   > /dev/null 2>&1", bakupName);
	i = 5;
	while(i--)
	{
		usleep(10);
		system(cmdd);
	}
	
	/*recover the /etc/backup/*.staAcl & *.staMac to /etc/.staAcl & .staMac*/
	memset(cmdd, 0, sizeof cmdd);
	sprintf(cmdd, "cp /etc/backup/%s.staMac /etc/.staMac > /dev/null 2>&1", bakupName);
	system(cmdd);
	memset(cmdd, 0, sizeof cmdd);
	sprintf(cmdd, "cp /etc/backup/%s.staAcl /etc/.staAcl > /dev/null 2>&1", bakupName);
	system(cmdd);

	free(bakupName);
	system("reboot"); 
}
//luodp end  route rule 
//zhao zhanwei

struct staList *getSta(struct staList *list, char *maddr)
{
	struct staList *p;

	if(list != NULL)
		p = list->next;
	while(p != NULL)
	{
		if(!strcmp(p->macAddr, maddr))
			return p;
		else
			p = p->next;
	}
	return NULL;
}

/*get the prev one*/
struct staList *getSta1(struct staList *list, char *maddr)
{
	struct staList *p, *s;

	if(!list)
		return NULL;
	
	p = list->next;
	s = list;

	while(p != NULL)
	{
		if(!strcmp(p->macAddr, maddr))
			return s;
		else
		{
			p = p->next;
			s = s->next;
		}
	}
	return NULL;
	
}

void addSta(char *maddr, char *desc, char *status, int id)
{          
    struct staList *list, *p;
          
    list = (struct staList *)malloc( sizeof(struct staList) );
    if(list == NULL)
    { 
		return; 
    }

    strcpy(list->macAddr, maddr);
    strcpy(list->staDesc, desc);
	strcpy(list->status, status);
    list->id = id;
    list->next = NULL;

	if(staHostList != NULL)
		p = staHostList;
    if(p != NULL) 
    {
        while(p->next != NULL)
                p = p->next;

        list->next = p->next;
        p->next = list;
    }
    else
    {
		p = (struct staList *)malloc( sizeof(struct staList) );
		memset(p->macAddr, 0, sizeof p->macAddr);
		memset(p->staDesc, 0, sizeof p->staDesc);
		p->id = 0;
		staHostList = p;
		p->next = list;
	}

}


void delSta(char *maddr)
{
	struct staList *p, *pre;
	p = getSta(staHostList, maddr);
	if(!p)
		return;
	pre = getSta1(staHostList, maddr);

	pre->next = p->next;
	free(p);
}

void restart_sta_access()
{
	#if 0
	int i, j, k;
	char pr_buf[50];
	char pChar[40];
	char valBuff[128];
	char valBuff2[128];

	Execute_cmd("cfg -e | grep 'AP_SECMODE=' |  awk -F '=' '{print $2}'", valBuff2);
	//fprintf(errOut," the ath0 AP_SECMODE is [%s] \n", valBuff2);
	if(strstr(valBuff2, "None")) /*wifi doesn't use WPA*/
		Execute_cmd("cfg -t0 /etc/ath/PSK.ap_bss_none ath0 > /tmp/secath0",rspBuff);
	else
		Execute_cmd("cfg -t0 /etc/ath/PSK.ap_bss ath0 > /tmp/secath0",rspBuff);

	Execute_cmd("cfg -e | grep 'AP_SECMODE_3=' |  awk -F '=' '{print $2}'", valBuff2);
	//fprintf(errOut," the ath0 AP_SECMODE_3 is [%s] \n", valBuff2);
	if(strstr(valBuff2, "None")) /*wifi doesn't use WPA*/
		Execute_cmd("cfg -t3 /etc/ath/PSK.ap_bss_none ath2 > /tmp/secath2",rspBuff);
	else
		Execute_cmd("cfg -t3 /etc/ath/PSK.ap_bss ath2 > /tmp/secath2",rspBuff);
	
	//Execute_cmd("killall hostapd > /dev/null 2>&1",rspBuff);
	/*get the hostapd for ath0's pid, to kill it, then restart it*/
	Execute_cmd("ps | grep 'hostapd -B /tmp/secath0 -e /etc/wpa2/entropy' | awk -F ' ' '{print $1}'", valBuff);
	//fprintf(errOut," the ath0 hostapd is [%s] \n", valBuff);
	i = j = k = 0;
	if((strlen(valBuff) > 0) && strstr(valBuff, "<br>"))
	{
		while(valBuff[i])
		{
			if(valBuff[i] == '\n')
			{
				k = 1;
				break;
			}
			//fprintf(errOut," is [%c] \n", valBuff[i]);
			i++;
		}
		if(k == 1)
		{
			memcpy(pr_buf, valBuff, i);
			//fprintf(errOut," is [%s] \n", pr_buf);
			sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
			Execute_cmd(pChar, rspBuff);

			j = i+5;
			while(valBuff[i+5])
			{
				if(valBuff[i+5] == '\n')
				{
					k = 2;
					break;
				}
				//fprintf(errOut," is [%c] \n", valBuff[i+5]);
				i++;
			}
			if(k == 2)
			{
				memcpy(pr_buf, &valBuff[j], i+5-j);
				//fprintf(errOut," is [%s] \n", pr_buf);
				sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
				Execute_cmd(pChar, rspBuff);
			}
		}
	}
	
	Execute_cmd("ps | grep 'hostapd -B /tmp/secath2 -e /etc/wpa2/entropy' | awk -F ' ' '{print $1}'", valBuff);
	//fprintf(errOut," the ath2 hostapd is [%s] \n", valBuff);
	i = j = k = 0;
	if((strlen(valBuff) > 0) && strstr(valBuff, "<br>"))
	{
		while(valBuff[i])
		{
			if(valBuff[i] == '\n')
			{
				k = 1;
				break;
			}
			//fprintf(errOut," is [%c] \n", valBuff[i]);
			i++;
		}
		if(k == 1)
		{
			memcpy(pr_buf, valBuff, i);
			//fprintf(errOut," is [%s] \n", pr_buf);
			sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
			Execute_cmd(pChar, rspBuff);

			j = i+5;
			while(valBuff[i+5])
			{
				if(valBuff[i+5] == '\n')
				{
					k = 2;
					break;
				}
				//fprintf(errOut," is [%c] \n", valBuff[i+5]);
				i++;
			}
			if(k == 2)
			{
				memcpy(pr_buf, &valBuff[j], i+5-j);
				//fprintf(errOut," is [%s] \n", pr_buf);
				sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
				Execute_cmd(pChar, rspBuff);
			}
		}
	}
	//fprintf(errOut,"the end \n");

	Execute_cmd("hostapd -B /tmp/secath0 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
	Execute_cmd("hostapd -B /tmp/secath2 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
	#endif
	Execute_cmd("ifconfig ath0 down;ifconfig ath0 up > /dev/null 2>&1",rspBuff);
	Execute_cmd("ifconfig ath2 down;ifconfig ath2 up > /dev/null 2>&1",rspBuff);
	
}

int  add_sta_access()
{
	FILE *fp, *fp1;
	struct staList stalist;
	int same = 0, id = 0;
	char staMac[20];
	char staDesc[80];
	char status[50];
	char con_buf[10];
	char buf[50];
	char valBuf[20];
	char valBuf1[50];
    char wifi0_flag[5];
    char wifi1_flag[5];


	CFG_get_by_name("WIFION_OFF",wifi0_flag);
	CFG_get_by_name("WIFION_OFF_3",wifi1_flag);

	memset(staMac, 0, sizeof staMac);
	memset(staDesc, 0, sizeof staDesc);
	Execute_cmd("cfg -e | grep \"WCONON_OFF=\" | awk -F \"=\" \'{print $2}\'", valBuf);
	Execute_cmd("grep -c 168c /proc/bus/pci/devices", valBuf1);
	//fprintf(errOut,"valBuf1 is [%s] \n", valBuf1);
	
	if ((fp = fopen(STA_MAC, "r")) == NULL)
	{
		if ((fp = fopen(STA_MAC, "w+")) == NULL) 
		{
			fprintf(errOut,"\nUnable to open /etc/.staMac for writing\n");
		}
		else
		{
			CFG_get_by_name("MAC",staMac);
			CFG_get_by_name("DES",staDesc);
			CFG_get_by_name("CON_STATUS",status);
			strcpy(stalist.macAddr, staMac);
			strcpy(stalist.staDesc, staDesc);
			strcpy(stalist.status, status);
			stalist.id = id + 1;
			fwrite(&stalist, sizeof(struct staList), 1, fp);
			fclose(fp);

			
			if ((fp1 = fopen("/etc/.staAcl", "r")) != NULL)
			{
				memset(con_buf, 0, 20);
				fgets(con_buf, 8, fp1);
				if((!strncmp(con_buf, "enable", 6)) && (!strcmp(status, "1")))
				{
					restart_sta_access();
					//sprintf(buf, "iptables -A control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
					//Execute_cmd(buf, rspBuff);
				}
				fclose(fp1);
			}
			else if(strstr(valBuf, "on"))
			{
				restart_sta_access();
				//sprintf(buf, "iptables -A control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
				//Execute_cmd(buf, rspBuff);
			}
			if(strcmp(wifi0_flag,"on") == 0 ) //on			
			{
				sprintf(buf, "iwpriv ath0 addmac %s", stalist.macAddr);
				Execute_cmd(buf, rspBuff);
			}
			if(strstr(valBuf1, "1"))
			{
				sprintf(buf, "iwpriv ath2 addmac %s", stalist.macAddr);
				Execute_cmd(buf, rspBuff);
			}
			//fprintf(errOut,"add_sta_access 1111 the add mac is %s\n", stalist.macAddr);
		}
	}
	else
	{
		
		CFG_get_by_name("MAC",staMac);
		CFG_get_by_name("DES",staDesc);
		CFG_get_by_name("CON_STATUS",status);
		while(fread(&stalist, sizeof stalist, 1, fp) == 1)
		{
			id = stalist.id;
			if( strcmp(stalist.macAddr, staMac) == 0)
			{
				fprintf(errOut,"\n the %s is exit\n", staMac);
				fclose(fp);
				same = 1;
				break;
			}
			else
				same = 0;
		}
		if(same == 0)
		{
			fp = fopen(STA_MAC, "at");
			strcpy(stalist.macAddr, staMac);
			strcpy(stalist.staDesc, staDesc);
			strcpy(stalist.status, status);
			stalist.id = id + 1;
			fwrite(&stalist, sizeof(struct staList), 1, fp);
			fclose(fp);

			if ((fp1 = fopen("/etc/.staAcl", "r")) != NULL)
			{
				memset(con_buf, 0, 20);
				fgets(con_buf, 8, fp1);
				//fprintf(errOut,"\n the con_buf is %s\n", con_buf);
				if((!strncmp(con_buf, "enable", 6)) && (!strcmp(status, "1")))
				{
					restart_sta_access();
					//memset(buf, 0, sizeof buf);
					//sprintf(buf, "iptables -A control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
					//Execute_cmd(buf, rspBuff);
				}
				fclose(fp1);
			}
			else if(strstr(valBuf, "on"))
			{
				restart_sta_access();
				//sprintf(buf, "iptables -A control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
				//Execute_cmd(buf, rspBuff);
			}

			memset(buf, 0, sizeof buf);
			
			if(strcmp(wifi0_flag,"on") == 0 ) //on
			{
				sprintf(buf, "iwpriv ath0 addmac %s", stalist.macAddr);
				Execute_cmd(buf, rspBuff);
			}

			if(strstr(valBuf1, "1"))
			{
				sprintf(buf, "iwpriv ath2 addmac %s", stalist.macAddr);
				Execute_cmd(buf, rspBuff);
			}
			//fprintf(errOut,"add_sta_access 222 the add mac is %s\n", stalist.macAddr);
		}
		
	} 
    return same;
}

void del_sta_access()
{
	FILE *fp, *fp1;
	struct staList stalist, *p;
	char staMac[20];
	char con_buf[10];
	char buf[50];
	char valBuf[50];

    char wifi0_flag[5];
    char wifi1_flag[5];


	CFG_get_by_name("WIFION_OFF",wifi0_flag);
	CFG_get_by_name("WIFION_OFF_3",wifi1_flag);

	if(CFG_get_by_name("DELXXX",staMac))
	{
		if( (fp = fopen(STA_MAC, "r")) == NULL)
			fprintf(errOut,"\nopen %s error\n", STA_MAC);
		else
		{
			while(fread(&stalist, sizeof stalist, 1, fp) == 1)
			{
				if(!strcmp(stalist.macAddr, staMac))
				{
					#if 0
					if ((fp1 = fopen("/etc/.staAcl", "r")) != NULL)
					{
						memset(con_buf, 0, 20);
						fgets(con_buf, 8, fp1);
						if((!strncmp(con_buf, "enable", 6)) && (!strcmp(stalist.status, "1")))
						{
							sprintf(buf, "iptables -D control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
							Execute_cmd(buf, rspBuff);
						}
						fclose(fp1);
					}
					#endif
					//fprintf(errOut,"the read mac is [%s] \n", stalist.macAddr);
					//fprintf(errOut,"the del mac is [%s] \n", staMac);
			
					if(strcmp(wifi0_flag,"on") == 0 ) //on
					{
						sprintf(buf, "iwpriv ath0 delmac %s", staMac);
						Execute_cmd(buf, rspBuff);
					}
					Execute_cmd("grep -c 168c /proc/bus/pci/devices", valBuf);
					if(strstr(valBuf, "1"))
					{
						sprintf(buf, "iwpriv ath2 delmac %s", staMac);
						Execute_cmd(buf, rspBuff);
					}
					continue;
				}
				addSta(stalist.macAddr, stalist.staDesc, stalist.status, stalist.id);
			}
			fclose(fp);
		}

		if( (fp = fopen(STA_MAC, "w")) == NULL)
			fprintf(errOut,"\nopen %s error\n", STA_MAC);
		else
		{
			int num = 1;
			p = scan_staList(staHostList);
			while(p)
			{
				p->id = num++;
				fwrite(p, sizeof(struct staList), 1, fp);
				p = p->next;
			}
			fclose(fp);
		}
	}
}

void control_sta_access()
{
	FILE *fp, *fpp;
	struct staList stalist;
	char buf[80];
	char valBuff[128];
	char valBuf[50];
	const char *staAcl = "/etc/.staAcl";

    char wifi0_flag[5];
    char wifi1_flag[5];


	CFG_get_by_name("WIFION_OFF",wifi0_flag);
	CFG_get_by_name("WIFION_OFF_3",wifi1_flag);

	
	Execute_cmd("grep -c 168c /proc/bus/pci/devices", valBuf);
	if(strcmp(CFG_get_by_name("WCONON_OFF",valBuff),"on") == 0 )
	{
		if ((fp = fopen(staAcl, "w")) != NULL)
		{
			fwrite("enable", sizeof("enable"), 1, fp);
			fclose(fp);
		}

		//Execute_cmd("iptables -t filter -F control_sta", rspBuff);
		if ((fpp = fopen(STA_MAC, "r")) != NULL)
		{
			while(fread(&stalist, sizeof stalist, 1, fpp) == 1)
			{
				memset(buf, 0, sizeof buf);
				if(!strcmp(stalist.status, "1"))
				{
					if(strcmp(wifi0_flag,"on") == 0 ) //on
					{
						sprintf(buf, "iwpriv ath0 addmac %s", stalist.macAddr);
						Execute_cmd(buf, rspBuff);
					}
					if(strstr(valBuf, "1"))
					{
						sprintf(buf, "iwpriv ath2 addmac %s", stalist.macAddr);
						Execute_cmd(buf, rspBuff);
					}
					//fprintf(errOut,"\n%s  %d the add mac is %s \n",__func__,__LINE__, stalist.macAddr);
					//memset(buf, 0, sizeof buf);
					//sprintf(buf, "iptables -A control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
					//Execute_cmd(buf, rspBuff);
				}
			}
			fclose(fpp);
			restart_sta_access();
		}
		if(strcmp(wifi0_flag,"on") == 0 ) //on
		{
			Execute_cmd("iwpriv ath0 maccmd 2",rspBuff);
		}
		if(strstr(valBuf, "1"))
		{
			Execute_cmd("iwpriv ath2 maccmd 2",rspBuff);
		}

		fprintf(errOut,"\n%s  %d --------CONM_WORK on--------- \n",__func__,__LINE__);
	}
	else if(strcmp(CFG_get_by_name("WCONON_OFF",valBuff),"off") == 0 )
	{
		if ((fp = fopen(staAcl, "w")) != NULL)
		{
			fwrite("disable", sizeof("disable"), 1, fp);
			fclose(fp);
		}
		
		Execute_cmd("iptables -t filter -F control_sta", rspBuff);
		if ((fpp = fopen(STA_MAC, "r")) != NULL)
		{
			while(fread(&stalist, sizeof stalist, 1, fpp) == 1)
			{
				memset(buf, 0, sizeof buf);
				if(strcmp(wifi0_flag,"on") == 0 ) //on
				{
					sprintf(buf, "iwpriv ath0 delmac %s", stalist.macAddr);
					Execute_cmd(buf, rspBuff);
				}
				if(strstr(valBuf, "1"))
				{
					sprintf(buf, "iwpriv ath2 delmac %s", stalist.macAddr);
					Execute_cmd(buf, rspBuff);
				}
			}
			fclose(fpp);
		}
		if(strcmp(wifi0_flag,"on") == 0 ) //on
		{
			Execute_cmd("iwpriv ath0 maccmd 0",rspBuff);
		}
		if(strstr(valBuf, "1"))
		{
			Execute_cmd("iwpriv ath2 maccmd 0",rspBuff);
		}
		//Execute_cmd("killall -q -USR1 udhcpd", rspBuff);
		fprintf(errOut,"\n%s  %d --------CONM_WORK off--------- \n",__func__,__LINE__);
	}
}

void modify_sta_access()
{
	char staId[10];
	char staStatus[10];
	char buf[80];
	char valBuf[50];
	struct staList stalist, *p;
	FILE *fp;
	int on_off;

    char wifi0_flag[5];
    char wifi1_flag[5];


	CFG_get_by_name("WIFION_OFF",wifi0_flag);
	CFG_get_by_name("WIFION_OFF_3",wifi1_flag);


	CFG_get_by_name("MODXXX", staId);
	CFG_get_by_name("ON_OFF", staStatus);
	Execute_cmd("grep -c 168c /proc/bus/pci/devices", valBuf);
	
	fp = fopen(STA_MAC, "r");
	while(fread(&stalist, sizeof stalist, 1, fp) == 1)
	{
		if( stalist.id == atoi(staId))
		{
			if(!strcmp(staStatus, "OFF"))
			{
			
				if(strcmp(wifi0_flag,"on") == 0 ) //on
				{
					sprintf(buf, "iwpriv ath0 delmac %s", stalist.macAddr);
					Execute_cmd(buf, rspBuff);
				}
				if(strstr(valBuf, "1"))
				{
					sprintf(buf, "iwpriv ath2 delmac %s", stalist.macAddr);
					Execute_cmd(buf, rspBuff);
				}

				//restart_sta_access();
				//sprintf(buf, "iptables -D control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
				//Execute_cmd(buf, rspBuff);
			
				strcpy(stalist.status, "0");
			}
			else
			{
			
				if(strcmp(wifi0_flag,"on") == 0 ) //on
				{
					sprintf(buf, "iwpriv ath0 addmac %s", stalist.macAddr);
					Execute_cmd(buf, rspBuff);
				}
				if(strstr(valBuf, "1"))
				{
					sprintf(buf, "iwpriv ath2 addmac %s", stalist.macAddr);
					Execute_cmd(buf, rspBuff);
				}

				restart_sta_access();
				//sprintf(buf, "iptables -A control_sta -m mac --mac-source %s -j DROP", stalist.macAddr);
				//Execute_cmd(buf, rspBuff);

				strcpy(stalist.status, "1");
			}
		}
		addSta(stalist.macAddr, stalist.staDesc, stalist.status, stalist.id);
	}
	fclose(fp);
	
	fp = fopen(STA_MAC, "w");
	p = scan_staList(staHostList);
	while(p)
	{
		fwrite(p, sizeof(struct staList), 1, fp);
		p = p->next;
	}
	fclose(fp);
}

struct staList *scan_staList(struct staList *list)
{
	struct staList *stalist;
	if(!list)
		return NULL;
	stalist = list->next;

	return stalist;
	
}

//end zhaozhanwei
//added by yhl below for ip process
//pChar is net_seg,ip is output,max_end_ip
unsigned char * get_max_dhcp_end_ip(unsigned char* pChar,unsigned char ip[20])
{		   
      unsigned char pstr[30]; 
	  unsigned char* pip[4];
	  unsigned char tmp;
	  int num=0,i,j;

	  memset(ip,'\0',20);
	  strcpy(ip,pChar);
	  num=strlen(ip);
	  
	  pip[0]=ip;
	  for(i=0,j=1;i<num,j<4;i++)
         { 
           if(ip[i]=='.')   
              {
                ip[i]='\0';
				pip[j]=&ip[i+1];
				j++;
			  }
    	 }
             for(i=3;i>=0;i--)
			  {
			   	if(atoi(pip[i])!=0)
			   	{
			    	if((atoi(pip[i])%128)==0)
                            { tmp=(atoi(pip[i])+127-1);break;}
				    if((atoi(pip[i])%64)==0)
					        {tmp=(atoi(pip[i])+63-1);break;}
				    if((atoi(pip[i])%32)==0)
					        {tmp=(atoi(pip[i])+31-1);break;}
				    if((atoi(pip[i])%16)==0)
					        {tmp=(atoi(pip[i])+15-1);break;}
				    if((atoi(pip[i])%8)==0)
					       {tmp=(atoi(pip[i])+7-1);break;}
				    if((atoi(pip[i])%4)==0)
					       {tmp=(atoi(pip[i])+3-1);break;}
				    if((atoi(pip[i])%2)==0)
					       { tmp=(atoi(pip[i])+1-1);break;	}
				    break;
			   	}
              }
   sprintf(pstr,"%d.%d.%d.%d",atoi(pip[0]),atoi(pip[1]),atoi(pip[2]),tmp);
   strcpy(ip,pstr);
}

//ensure pip psub to be :x.x.x.x\0.... ret_buf to be:\0 \0 ......
void get_net_seg_or(unsigned char* pip,unsigned char* psub,unsigned char* ret_buf)
{
unsigned char* ip_seg_pos[4];
unsigned char* sub_seg_pos[4];
unsigned char ip_seg_int[4],sub_seg_int[4],net_seg_int[4];
unsigned char net_seg_str[20];
unsigned char num,i,j=0;

unsigned char tmp[20];

memset(ret_buf,'\0',20);
memset(net_seg_str,'\0',20);
memset(tmp,'\0',20);


ip_seg_pos[0]=pip;
sub_seg_pos[0]=psub;

//for--ip
for(i=0;i<strlen(pip);i++)
	{
	   if(pip[i]=='.')
		{
		 pip[i]=='\0';
         j++;
	    ip_seg_pos[j]=&pip[i+1];
        }
    }

for(i=0;i<4;i++)
{
ip_seg_int[i]=atoi(ip_seg_pos[i]);
}

//for sub
   j=0;
for(i=0;i<strlen(psub);i++)
	{
	   if(psub[i]=='.')
		{
		 psub[i]=='\0';
         j++;
	     sub_seg_pos[j]=&psub[i+1];
        }
    }

for(i=0;i<4;i++)
{
sub_seg_int[i]=atoi(sub_seg_pos[i]);
}

//or process
for(i=0;i<4;i++)
	{net_seg_int[i]=(ip_seg_int[i]|sub_seg_int[i]);}

sprintf(net_seg_str,"%d",(net_seg_int[0]));
strcat(net_seg_str,".");

sprintf(tmp,"%d",(net_seg_int[1]));
strcat(net_seg_str,tmp);
strcat(net_seg_str,".");

sprintf(tmp,"%d",(net_seg_int[2]));
strcat(net_seg_str,tmp);
strcat(net_seg_str,".");

sprintf(tmp,"%d",(net_seg_int[3]));
strcat(net_seg_str,tmp);

strcpy(ret_buf,net_seg_str);
}


//ensure pip psub to be :x.x.x.x\0.... ret_buf to be:\0 \0 ......
void get_net_seg_and(unsigned char* pip,unsigned char* psub,unsigned char* ret_buf)
{
unsigned char* ip_seg_pos[4];
unsigned char* sub_seg_pos[4];
unsigned char ip_seg_int[4],sub_seg_int[4],net_seg_int[4];
unsigned char net_seg_str[20];
unsigned char num,i,j=0;

unsigned char tmp[20];

memset(ret_buf,'\0',20);
memset(net_seg_str,'\0',20);
memset(tmp,'\0',20);

ip_seg_pos[0]=pip;
sub_seg_pos[0]=psub;

//for--ip
for(i=0;i<strlen(pip);i++)
	{
	   if(pip[i]=='.')
		{
		pip[i]=='\0';
         j++;
	    ip_seg_pos[j]=&pip[i+1];
        }
    }
for(i=0;i<4;i++)
 { ip_seg_int[i]=atoi(ip_seg_pos[i]);}

//for sub
   j=0;
for(i=0;i<strlen(psub);i++)
	{
	   if(psub[i]=='.')
		{
		 psub[i]=='.';
         j++;
	     sub_seg_pos[j]=&psub[i+1];
        }
    }

for(i=0;i<4;i++)
  { sub_seg_int[i]=atoi(sub_seg_pos[i]);}

//and process
for(i=0;i<4;i++)
	{net_seg_int[i]=(ip_seg_int[i]&sub_seg_int[i]);}

		sprintf(net_seg_str,"%d",(net_seg_int[0]));
		strcat(net_seg_str,".");

		sprintf(tmp,"%d",(net_seg_int[1]));
		strcat(net_seg_str,tmp);
		strcat(net_seg_str,".");

		sprintf(tmp,"%d",(net_seg_int[2]));
		strcat(net_seg_str,tmp);
		strcat(net_seg_str,".");

		sprintf(tmp,"%d",(net_seg_int[3]));
		strcat(net_seg_str,tmp);

		strcpy(ret_buf,net_seg_str);
}

//added by yhl above,for ip process

//add by wangyu
int set_ntp_server( void)
{
	
#ifdef MESSAGE
	fprintf(errOut,"\n%s  %d TIMEZONE_SET \n",__func__,__LINE__);
#endif		
	char valBuff1[128]; 
	char valBuff2[128]; 
	char valBuff3[128]; 
	char valBuff4[128]; 
	int  set_ntp_flag;


	CFG_get_by_name("NTPON_OFF",valBuff1);
	CFG_get_by_name("TIME_ZONE",valBuff2);
	CFG_get_by_name("NTPServerIP1",valBuff3);
	CFG_get_by_name("NTPServerIP2",valBuff4);
	//off
	
#ifdef MESSAGE
	fprintf(errOut,"%s	%d	valBuff1 %s valBuff2 %s valBuff3 %s\n",__func__,__LINE__,
			valBuff1,valBuff2,valBuff3);
#endif
	//save new config to flash 
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
	Execute_cmd("killall ntpclient > /dev/null 2>&1",rspBuff);
	Execute_cmd("/usr/sbin/set_ntpserver > /tmp/ntpserver.log 2>&1",rspBuff);
#if 1
	FILE *fileBuf2=NULL;
	if ((fileBuf2= fopen("/tmp/ntpserver.log", "r")) == NULL)
	{
		fprintf(errOut,"%s	%d File open error.Make sure you have the permission.\n",__func__,__LINE__);
		set_ntp_flag=1;
	}else
	{
		while(fgets(valBuff4,sizeof(valBuff4),fileBuf2) != NULL){
			if(strstr(valBuff4,"Network is unreachable")!=NULL)
			{
				set_ntp_flag=2;
			}			
			if(strstr(valBuff4,"Unknown host")!=NULL)
			{
				set_ntp_flag=3;
			}
			if(strstr(valBuff4,"Connection refused")!=NULL)
			{
				set_ntp_flag=4;
			}
			else
			{
				set_ntp_flag=5;
			}
		}
		fclose(fileBuf2);
	}
	return set_ntp_flag;
#endif
}
int set_pctime(void)
{
	
#ifdef MESSAGE
	fprintf(errOut,"\n%s  %d TIMEZONE_SET \n",__func__,__LINE__);
#endif		
	
	char valBuff1[128]; 
	char valBuff2[128]; 
	char valBuff3[128]; 
	char valBuff4[128]; 
	
	CFG_get_by_name("PCTIME",valBuff1);

	
#ifdef MESSAGE
	fprintf(errOut,"%s	%d	valBuff1 %s \n",__func__,__LINE__,valBuff1);
#endif

	//save new config to flash 
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);

	Execute_cmd("/usr/sbin/set_pctime > /dev/null 2>&1",rspBuff);

	return 0;
}

/**************************************************************************/
static void  Result_tiaozhuan(char* res,char * gopage)
{
    char temp[256]={0};
	
	system("dd if=/dev/caldata of=/etc/cal.bin > /dev/null 2>&1");
	
    printf("HTTP/1.0 200 OK\r\n");
    printf("Content-type: text/html\r\n");
    printf("Connection: close\r\n");
    printf("\r\n");
    printf("\r\n");
    printf("<HTML><HEAD>\r\n");
    printf("</head><body>");

    sprintf(temp,"<script language=javascript>window.location.href=\"tiaozhuan?RESULT=%s?PAGE=%s\";</script>",res,gopage);
    printf(temp);
    printf("</body></html>");
}

static void  Normal_tiaozhuan(char * gopage)
{
    char temp[256]={0};
	
   // system("dd if=/dev/caldata of=/etc/cal.bin > /dev/null 2>&1");
	
    printf("HTTP/1.0 200 OK\r\n");
    printf("Content-type: text/html\r\n");
    printf("Connection: close\r\n");
    printf("\r\n");
    printf("\r\n");
    printf("<HTML><HEAD>\r\n");
    printf("</head><body>");

    sprintf(temp,"<script language=javascript>window.location.href=\"%s?%s=yes?INDEX=33\";</script>",gopage,gopage);
    printf(temp);
    printf("</body></html>");
}

/**************************************************************************/
static void  Reboot_tiaozhuan(char* res,char * gopage)
{
    char temp[256]={0};
    printf("HTTP/1.0 200 OK\r\n");
    printf("Content-type: text/html\r\n");
    printf("Connection: close\r\n");
    printf("\r\n");
    printf("\r\n");
    printf("<HTML><HEAD>\r\n");
    printf("</head><body>");

    sprintf(temp,"<script language=javascript>window.location.href=\"reboot?RESULT=%s?PAGE=%s\";</script>",res,gopage);
    printf(temp);
    printf("</body></html>");
}
/*****************************************************************************
**
** /brief Main
**
** This program will read parameterized HTML files and insert the proper
** strings from the parameter store -- either in flash or temp storage.
**
** The specific page to process will be determined by the actual command
** name returned in argv[0].  Each page name will be linked to the executable
** in much the same manner as busybox does it's commands.  This will require
** that each page link be located in the cgi-bin directory since they are ALL
** being processed as cgi scripts by the httpd daemon.  Actual pages will be
** located in the /usr/www directory.
**
** Other functions are provided to support the command line processing.
**
** Options: -a  Add a parameter/value pair to the cache
**              # cgiMain -a SSID=MySSID
**
**          -r  Remove a parameter from the parameter cache
**              # cgiMain -r AP_THIS_PARAM
**
**          -c  Commit parameter cache to flash
**              # cgiMain -c
**
**          -e  Print the export list for use in scripts
**              `cgiMain -e`
**
**          -i  Invalidate the parameter cache by re-reading the flash
**              values and overriding the parameter cache.  NOTE: this
**              will loose any changes made to the parameter cache.
**              # cgiMain -i
**
**          -s  Print the contents of the database without translation
**
**          -t  Translate File.  This will take the indicated file and
**              insert parameter data as marked by the ~ markers in the
**              file.  Uses the same rules as the HTML files.  Normal
**              output is to stdout, can be redirected to another file.
**              if the third parameter is specified, it is assumed to be
**              the interface name.
**
**              # cgiMain -t wpapsk.conf ath0 > /tmp/secvap.conf
**              # cgiMain -t2 wpapsk.conf > /tmp/secvap2.conf
*/

int main(int argc,char **argv)
{
    char            Page[64];
    char            Name[32];
    char            Value[70];
    char            valBuff[128];
    char            *outPtr;
    int             i;
    int             j;
    int             ret=0;
    int             holdOutput;

    char            *nextField;
    char            *update;
    FILE            *f;

    int             lock11=0;
    int             lock12=0;
	int             lock13=0;
    int             lock112=0;
    int             gohome = 3;

	
    char wifi0_flag[5];
    char wifi1_flag[5];


	CFG_get_by_name("WIFION_OFF",wifi0_flag);
	CFG_get_by_name("WIFION_OFF_3",wifi1_flag);

    /*
    ** Code Begins.
    ** Zero out the config structure, and read the parameter cache
    ** (or flash, depending on command)
    */
    errOut = fopen("/dev/ttyS0","w");

    memset(&config,0,sizeof(config));
    memset(&config_sync,0,sizeof(config_sync));
    
    //fprintf(errOut,"%s  %d\n",__func__,__LINE__);
    f = fopen("/tmp/.apcfg","r");

    if ( !f )
    {
        /*
        ** We need to read the parameter data starting at 32K into the calibration
        ** sector (or) the NVRAM sector.  This is mapped to /dev/nvram or /dev/caldata, so we simply open 
        ** that device and read until we hit the EOL
        */

        f = fopen( NVRAM, "r" );

        if (!f)
        {
            printf("ERROR:  %s not defined on this device\n", NVRAM);
            printf("ERROR:  Cannot store data in flash!!!!\n");
            exit(-1);
        }

        fseek(f, NVRAM_OFFSET, SEEK_SET);
    }
    /*
    ** At this point the file is either open or not.  If it is, read the 
    ** parameters as require
    */

    if ( f )
    {
        fillParamStruct(f);
        fclose(f);
    }

    /*
    ** Now we look for options.
    ** -t means translate the indicated file
    */
    //fprintf(errOut,"%s  %d\n",__func__,__LINE__);

    if(argc > 1)
    {	
        if(!strncmp(argv[1],"-t",2))
        {
            /*
            ** Translate file
            ** read the file, then output the translated version
            */
            parameterIndex = 0;

            if(argv[1][2] != 0) {
                parameterIndex = argv[1][2] - 0x30;
            }
            if(isdigit(argv[1][3])) {
                parameterIndex = parameterIndex * 10 + (argv[1][3] - 0x30);
            }

            if(parameterIndex > 16)
                parameterIndex = 0;

			/*
			** Get the radio ID, if it exists
			*/

			radioIndex = getRadioID(parameterIndex);
			if(radioIndex == -1)
				radioIndex = 0;	/* set back to default */

            /*
            ** Input the "extra" parameters that may be included
            */

            for(i=3,numAdditionalParams=0;i<argc;i++)
            {
                strcpy(additionalParams[numAdditionalParams++],argv[i]);
            }

            /*
            ** Now, perform the translation
            */

            ModeFlag = 1;
            translateFile(argv[2]);
            exit(0);
        }
        else if(!strncmp(argv[1],"-b",2))
        {
        //    CFG_set_by_name("WIRELESS_ADVSET","WIRELESS_ADVSET");
        //viqjeee
            char    *vval;
            int i=0, j=0;
            int err_flag =0;
            char vname[10][10][64] =
            {
                {"WAN_MODE"  },  
                {"WAN_MODE","IPGW","WAN_IPADDR","WAN_NETMASK","PRIDNS" },  
                {"WAN_MODE","PPPOE_USER","PPPOE_PWD"  },  
                {"AP_SSID_2","AP_SSID_4","PSK_KEY_2","PSK_KEY_4" },  
                {"ADD_MAC","ADD_IP","ADD_STATUS"  }
            };


            if(argc < 3)
            {
                modePrintf("Invalid argument\n");
                exit(-1);
            }
            switch(argv[2][0])
            {
            case '0':
                 printf("DHCP\n");
                 if(argc !=4)
                 {
                    err_flag=1;
                    break;
                 }

                 for (i = 0; i < 1; i++) 
                 {
                     printf(" %s=%s ", vname[0][i], argv[i+3]);
                     CFG_set_by_name(vname[0][i],argv[i+3]);
                 }
                 CFG_set_by_name("DHCP","DHCP");
                 break;

            case '1':
                 printf("SIP\n");
                 if(argc !=8)
                 {
                    err_flag=1;
                    break;
                 }

                 for (i = 0; i < 5; i++) 
                 {
                     printf(" %s=%s ", vname[1][i], argv[i+3]);
                     CFG_set_by_name(vname[1][i],argv[i+3]);
                 }
                 CFG_set_by_name("SIP","SIP");
                 break;
                
            case '2':
                 printf("PPP\n");
                 if(argc !=6)
                 {
                    err_flag=1;
                    break;
                 }

                 for (i = 0; i < 3; i++) 
                 {
                     printf(" %s=%s ", vname[2][i], argv[i+3]);
                     CFG_set_by_name(vname[2][i],argv[i+3]);
                 }
                 CFG_set_by_name("PPP","PPP");
                 CFG_set_by_name("PPPOE_MODE","auto");
                 break;

            case '3':
                 printf("WIRELESS\n");
				 int flag1, flag2;
				 char pr_buf[50], pChar[128];;
				 char valBuff2[128], valBuff3[128], valBuff4[128], valBuff5[128], valBuff6[128], valBuff7[128], valBuff8[128], valBuff9[128];	
                 if(argc !=7)
                 {
                    err_flag=1;
                    break;
                 }

                 for (i = 0; i < 4; i++) 
                 {
                     printf(" %s=%s ", vname[3][i], argv[i+3]);
                     CFG_set_by_name(vname[3][i],argv[i+3]);
                 }
                 CFG_set_by_name("WIRELESS","WIRELESS");

				 /*set hide ap "ath1 ath3"   by zzw*/
				flag1 = 0;
				Execute_cmd("cfg -e | grep \"AP_SSID_2=\" | awk -F \"AP_SSID_2=\" '{print $2}'",valBuff2);
				Execute_cmd("cfg -e | grep \"AP_SSID_4=\" | awk -F \"AP_SSID_4=\" '{print $2}'",valBuff3);
				Execute_cmd("cfg -s | grep \"PSK_KEY_2:\" | awk -F \"=\" \'{print $2}\'",valBuff4);
				Execute_cmd("cfg -s | grep \"PSK_KEY_4:\" | awk -F \"=\" \'{print $2}\'",valBuff5);
				
				//2G
				CFG_get_by_name("AP_SSID_2",valBuff6);
				CFG_get_by_name("PSK_KEY_2",valBuff7);
				sprintf(valBuff8, "\"%s\"\n<br>", valBuff6);
				sprintf(valBuff9, "%s\n<br>", valBuff6);
				fprintf(errOut,"the ath1 AP_SSID_2--------- [%s][%s]\n", valBuff2, valBuff8);
				if(strcmp(valBuff2, valBuff8) && strcmp(valBuff2, valBuff9))
				{
					sprintf(pChar,"iwconfig ath1 essid %s  > /dev/null 2>&1",valBuff6);
					Execute_cmd(pChar, rspBuff);
					flag1 = 5;
				}

				sprintf(valBuff8, "%s\n<br>", valBuff7);
				fprintf(errOut,"the ath1 PSK_KEY_2--------- [%s][%s]\n", valBuff4, valBuff8);
				if(strcmp(valBuff4, valBuff8))
				{
					CFG_set_by_name("PSK_KEY_2",valBuff7);
					Execute_cmd(pChar, rspBuff);
					flag1 = 5;
				}

				//5G
				flag2 = 0;
				CFG_get_by_name("AP_SSID_4",valBuff6);
				CFG_get_by_name("PSK_KEY_4",valBuff7);
				sprintf(valBuff8, "\"%s\"\n<br>", valBuff6);
				sprintf(valBuff9, "%s\n<br>", valBuff6);
				fprintf(errOut,"the ath3 AP_SSID_4--------- [%s][%s]\n", valBuff3, valBuff8);
				if(strcmp(valBuff3, valBuff8) && strcmp(valBuff3, valBuff9))
				{
					sprintf(pChar,"iwconfig ath3 essid %s  > /dev/null 2>&1",valBuff6);
					Execute_cmd(pChar, rspBuff);
					flag2 = 5;
				}

				sprintf(valBuff8, "%s\n<br>", valBuff7);
				fprintf(errOut,"the ath3 PSK_KEY_4--------- [%s][%s]\n", valBuff5, valBuff8);
				if(strcmp(valBuff5, valBuff8))
				{
					CFG_set_by_name("PSK_KEY_4",valBuff7);
					Execute_cmd(pChar, rspBuff);
					flag2 = 5;
				}
				
            
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
		
				if(flag1 == 5)
				{
					int i,j,k;
					Execute_cmd("cfg -t2 /etc/ath/PSK.ap_bss ath1 > /tmp/secath1",rspBuff);
					/*get the hostapd for ath0's pid, to kill it, then restart it*/
					Execute_cmd("ps | grep 'hostapd -B /tmp/secath1 -e /etc/wpa2/entropy' | awk -F ' ' '{print $1}'", valBuff);
					fprintf(errOut,"the kill process is [%s] %d\n",valBuff, strlen(valBuff));
					if(strstr(valBuff, "<br>"))
					{
						while(valBuff[i])
						{
							if(valBuff[i] == '\n')
							{
								k = 1;
								break;
							}
							i++;
						}
						if(k == 1)
						{
							memcpy(pr_buf, valBuff, i);
							sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
							Execute_cmd(pChar, rspBuff);

							j = i+5;
							while(valBuff[i+5])
							{
								if(valBuff[i+5] == '\n')
								{
									k = 2;
									break;
								}
								i++;
							}
							if(k == 2)
							{
								memcpy(pr_buf, &valBuff[j], i+5-j);
								sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
								Execute_cmd(pChar, rspBuff);
							}
						}
					}
					Execute_cmd("hostapd -B /tmp/secath1 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);

					Execute_cmd("ifconfig ath1 down > /dev/null 2>&1", rspBuff);
					Execute_cmd("ifconfig ath1 up > /dev/null 2>&1", rspBuff);
				}

				if(flag2 == 5)
				{
		            int i,j,k;
					i=0;j=0;k=0;
		            memset(pr_buf,0,50);
					Execute_cmd("cfg -t4 /etc/ath/PSK.ap_bss ath3 > /tmp/secath3",rspBuff);
					/*get the hostapd for ath0's pid, to kill it, then restart it*/
					Execute_cmd("ps | grep 'hostapd -B /tmp/secath3 -e /etc/wpa2/entropy' | awk -F ' ' '{print $1}'", valBuff);
					fprintf(errOut,"the kill process is [%s] %d\n",valBuff, strlen(valBuff));
					if(strstr(valBuff, "<br>"))
					{
						while(valBuff[i])
						{
							if(valBuff[i] == '\n')
							{
								k = 1;
								break;
							}
							i++;
						}
						if(k == 1)
						{
							memcpy(pr_buf, valBuff, i);
							sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
							Execute_cmd(pChar, rspBuff);

							j = i+5;
							while(valBuff[i+5])
							{
								if(valBuff[i+5] == '\n')
								{
									k = 2;
									break;
								}
								i++;
							}
							if(k == 2)
							{
								memcpy(pr_buf, &valBuff[j], i+5-j);
								sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
								Execute_cmd(pChar, rspBuff);
							}
						}
					}
					Execute_cmd("hostapd -B /tmp/secath3 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);

					Execute_cmd("ifconfig ath3 down > /dev/null 2>&1", rspBuff);
					Execute_cmd("ifconfig ath3 up > /dev/null 2>&1", rspBuff);
				}

				 /*end zzw*/
                 break;

            case '4':
                 printf("ADD_STATICDHCP\n");
                 if(argc !=6)
                 {
                    err_flag=1;
                    break;
                 }

                 for (i = 0; i < 3; i++) 
                 {
                     printf(" %s=%s ", vname[4][i], argv[i+3]);
                     CFG_set_by_name(vname[4][i],argv[i+3]);
                 }
                 CFG_set_by_name("ADD_STATICDHCP","ADD_STATICDHCP");
                 break;

            case '5':
            //viqjeee
                 printf("terminal_dev_register factory reset\n");
                 if(argc !=3)
                 {
                    err_flag=1;
                    break;
                 }

                //factory reset
                 system("cfg -x");
                 //sync
                 writeParametersWithSync();
                 CFG_remove_by_name(argv[2]);
                CFG_remove_by_name("AP_RADIO_ID_2");
                CFG_remove_by_name("AP_SSID_2");
                CFG_remove_by_name("PSK_KEY_2");
                CFG_remove_by_name("AP_SECMODE_2");
                CFG_remove_by_name("AP_CYPHER_2");
                CFG_remove_by_name("AP_SECFILE_2");
    //            CFG_remove_by_name("AP_CHMODE");
    //            CFG_remove_by_name("AP_PRIMARY_CH");                

//				CFG_remove_by_name("AP_CHMODE_2");
 //               CFG_remove_by_name("AP_PRIMARY_CH_2");
				
                CFG_remove_by_name("AP_MODE_2");
                CFG_remove_by_name("AP_WPA_2");

                CFG_remove_by_name("AP_RADIO_ID_4");
                CFG_remove_by_name("AP_SSID_4");
                CFG_remove_by_name("PSK_KEY_4");
                CFG_remove_by_name("AP_SECMODE_4");
                CFG_remove_by_name("AP_CYPHER_4");
                CFG_remove_by_name("AP_SECFILE_4");
     //           CFG_remove_by_name("AP_CHMODE_3");
    //            CFG_remove_by_name("AP_PRIMARY_CH_3");
				
   //             CFG_remove_by_name("AP_CHMODE_4");
   //             CFG_remove_by_name("AP_PRIMARY_CH_4");                

				CFG_remove_by_name("AP_MODE_4");
                CFG_remove_by_name("AP_WPA_4");

                writeParameters(NVRAM,"w+", NVRAM_OFFSET);
                writeParameters("/tmp/.apcfg","w+",0);
                //remove ip_mac  bing
                 system("rm /etc/ip_mac.conf > /dev/null 2>&1");

                exit(0);
                 break;

            default:
                 break;
            }

            if(err_flag)
            {
                printf("\n Usage :\n");

                for (i = 0; i < 4; i++) 
                {
                    printf("\n cfg -b %d ", i);
                    while(vname[i][j][0] != '\0')
                    {
                        printf(" <%s>", vname[i][j]);
                        j++;
                    }
                    j=0;
                }
                exit(0);
            }

            //exit(0);
        }
        else if(!strncmp(argv[1],"-w",2))
        {
            char    *vname;
            char    *vval;
            FILE    *f;

            memset(&config,0,sizeof(config));

            f = fopen("/tmp/.webuicfg","a+");
            if ( !f )
            {
                modePrintf("open file error");
                exit(-1);
            }

            fillParamStruct(f);
            fclose(f);


            if(argc == 2)
            {
                for(i=0;i<config.numParams;i++)
                {
                    if(shellEncode(config.Param[i].Val,valBuff))
                        printf("export %s=\"%s\"\n",config.Param[i].Name,valBuff);
                    else
                        printf("export %s=%s\n",config.Param[i].Name,valBuff);
                }
                exit(0);
            }

            ModeFlag = 1;
            vname = argv[2];

            if(vval=strchr(argv[2],'='))
                *vval++ = 0;
            else
            {
                modePrintf("Mal formed string %s",argv[2]);
                exit(-1);
            }

            /*
            ** If setting fails, return a -1 (for scripts)
            */

            if( CFG_set_by_name(vname,vval) )
                exit(-1);

            writeParameters("/tmp/.webuicfg","w+",0);
            exit(0);
        }
        else if(!strncmp(argv[1],"-a",2))
        {
            char    *vname;
            char    *vval;

            /*
            ** Add a parameter.  Argv[2] should contain the parameter=value string.
            ** Do NOT use extractParam in this case, we need to directly enter
            ** the value.
            */

            if(argc < 3)
            {
                modePrintf("Invalid argument");
                exit(-1);
            }

            ModeFlag = 1;
            vname = argv[2];

            if(vval=strchr(argv[2],'='))
                *vval++ = 0;
            else
            {
                modePrintf("Mal formed string %s",argv[2]);
                exit(-1);
            }

            /*
            ** If setting fails, return a -1 (for scripts)
            */

            if( CFG_set_by_name(vname,vval) )
                exit(-1);

            writeParameters("/tmp/.apcfg","w+",0);
            exit(0);
        }
        else if(!strncmp(argv[1],"-c",2))
        {
            /*
            ** Write the parameter structure to the flash.  This is
            ** the "commit" function
            */
#ifdef ATH_SINGLE_CFG
            athcfg_set_default_config_values();
#endif
            writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            writeParameters("/tmp/.apcfg","w+",0);
            exit(0);
        }
        else if(!strncmp(argv[1],"-r",2))
        {
            /*
            ** Remove the parameter from the cache.  This will write the
            ** cache, but not write to the flash.  Explicit commit required
            ** to write to flash.
            */

            CFG_remove_by_name(argv[2]);
            FactoryDefault = 0;
            writeParameters("/tmp/.apcfg","w+",0);
            exit(0);
        }
        else if(!strncmp(argv[1],"-h",2))
        {
            /*
            ** We want to check argv[2] to determine if it's a properly formattedRemove the parameter from the cache.  This will write the
            ** cache, but not write to the flash.  Explicit commit required
            ** to write to flash.
            */

            exit(isKeyHex(argv[2],atoi(argv[3])));
        }
        else if(!strncmp(argv[1],"-e",2))
        {
            /*
            ** Export the variables
            ** This is used as part of a shell script to "export" the variables
            ** to the environment
            */
#ifdef ATH_SINGLE_CFG
            athcfg_set_default_config_values();
#endif /* ATH_SINGLE_CFG */

            for(i=0;i<config.numParams;i++)
            {
                /*
                ** Check for certain variables -- mostly key values -- that
                ** we don't want to export due to the "funnies" in their string
                ** values.  They will still be included in the database, but the
                ** will not be exported in the script files.
                **
                ** Unfortunately, SSID is a parameter that cannot be ignored, but
                ** can have various issues with shell special characters.  This will
                ** be a limitation on the SSID string that cannot be avoided
                */

                if(!strncmp(config.Param[i].Name,"PSK_KEY",7))
                    continue;

                /*
                ** We can export this variable.
                */

                if(shellEncode(config.Param[i].Val,valBuff))
                    printf("export %s=\"%s\"\n",config.Param[i].Name,valBuff);
                else
                    printf("export %s=%s\n",config.Param[i].Name,valBuff);

            }
            exit(0);
        }
        else if(!strncmp(argv[1],"-s",2))
        {
            /*
            ** Show the variables
            ** This is used as a debug method to dump the variables to the output.
            ** This dumps ALL variables
            */
            for(i=0;i<config.numParams;i++)
            {
                printf("%s:=%s\n",config.Param[i].Name,config.Param[i].Val);
            }
            exit(0);
        }
        else if(!strncmp(argv[1],"-x",2))
        {
            /*
            ** Erase all parameters in flash and cache.
            ** This is the equivalent of a reset command.
            */
			char valBuff[128];	

//wangyu add for ath1 and ath3 envionment param save
			char ap_radioid_2g[128];			
			char ap_ssid_2g[128];
            char ap_psk_2g[128];
			char ap_secmode_2g[128];
			char ap_cypher_2g[128];
			char ap_secfile_2g[128];
			char ap_chmode_2g[128];
			char ap_primarych_2g[128];
			char ap_mode_2g[128];
			char ap_wpa_2g[128];

			char ap_radioid_5g[128];			
			char ap_ssid_5g[128];
            char ap_psk_5g[128];
			char ap_secmode_5g[128];
			char ap_cypher_5g[128];
			char ap_secfile_5g[128];
			char ap_chmode_5g[128];
			char ap_primarych_5g[128];
			char ap_mode_5g[128];
			char ap_wpa_5g[128];

			int ii;
			char buf[128];
			char bakeup[128][128];
			
			CFG_get_by_name("SOFT_VERSION",valBuff);

			CFG_get_by_name("AP_RADIO_ID_2",ap_radioid_2g);
			CFG_get_by_name("AP_SSID_2",ap_ssid_2g);
            CFG_get_by_name("PSK_KEY_2",ap_psk_2g);
			CFG_get_by_name("AP_SECMODE_2",ap_secmode_2g);
			CFG_get_by_name("AP_CYPHER_2",ap_cypher_2g);
			CFG_get_by_name("AP_SECFILE_2",ap_secfile_2g);
//			CFG_get_by_name("AP_CHMODE",ap_chmode_2g);
//			CFG_get_by_name("AP_PRIMARY_CH",ap_primarych_2g);

//			CFG_get_by_name("AP_CHMODE_2",ap_chmode_2g);
//			CFG_get_by_name("AP_PRIMARY_CH_2",ap_primarych_2g);

			CFG_get_by_name("AP_MODE_2",ap_mode_2g);
			CFG_get_by_name("AP_WPA_2",ap_wpa_2g);

			CFG_get_by_name("AP_RADIO_ID_4",ap_radioid_5g);
			CFG_get_by_name("AP_SSID_4",ap_ssid_5g);
            CFG_get_by_name("PSK_KEY_4",ap_psk_5g);
			CFG_get_by_name("AP_SECMODE_4",ap_secmode_5g);
			CFG_get_by_name("AP_CYPHER_4",ap_cypher_5g);
			CFG_get_by_name("AP_SECFILE_4",ap_secfile_5g);
//			CFG_get_by_name("AP_CHMODE_3",ap_chmode_5g);
//			CFG_get_by_name("AP_PRIMARY_CH_3",ap_primarych_5g);

//			CFG_get_by_name("AP_CHMODE_4",ap_chmode_5g);
//			CFG_get_by_name("AP_PRIMARY_CH_4",ap_primarych_5g);

			CFG_get_by_name("AP_MODE_4",ap_mode_5g);
			CFG_get_by_name("AP_WPA_4",ap_wpa_5g);

			for(i = 0; i < 128; i++)
			{
				sprintf(buf, "BACKUP%d", i+1);
				CFG_get_by_name(buf, bakeup[i]);
				if(strlen(bakeup[i]) == 0)
					break;
			}
			
            memset(&config,0,sizeof(config));
            FactoryDefault = 1;
			CFG_set_by_name("FACTORY_RESET","0");
            writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            writeParameters("/tmp/.apcfg","w+",0);
#ifndef ATH_SINGLE_CFG
            /*
            ** Now, remove WPS files and execute the apcfg script to set the
            ** values.  This is required for determination of "factory default" state
            */
			//fprintf(errOut,"[luodp] do factory");
            Execute_cmd("rm -rf /etc/wpa2/*.conf;/etc/ath/apcfg", rspBuff);	
            //backup FACTORY flag
            CFG_set_by_name("FACTORY","1");

			//backup version
			CFG_set_by_name("SOFT_VERSION",valBuff);


			CFG_set_by_name("AP_RADIO_ID_2",ap_radioid_2g);
			CFG_set_by_name("AP_SSID_2",ap_ssid_2g);
            CFG_set_by_name("PSK_KEY_2",ap_psk_2g);
			CFG_set_by_name("AP_SECMODE_2",ap_secmode_2g);
			CFG_set_by_name("AP_CYPHER_2",ap_cypher_2g);
			CFG_set_by_name("AP_SECFILE_2",ap_secfile_2g);
//			CFG_set_by_name("AP_CHMODE",ap_chmode_2g);
//			CFG_set_by_name("AP_PRIMARY_CH",ap_primarych_2g);

//			CFG_set_by_name("AP_CHMODE_2",ap_chmode_2g);
//			CFG_set_by_name("AP_PRIMARY_CH_2",ap_primarych_2g);

			CFG_set_by_name("AP_MODE_2",ap_mode_2g);
			CFG_set_by_name("AP_WPA_2",ap_wpa_2g);

			CFG_set_by_name("AP_RADIO_ID_4",ap_radioid_5g);
			CFG_set_by_name("AP_SSID_4",ap_ssid_5g);
            CFG_set_by_name("PSK_KEY_4",ap_psk_5g);
			CFG_set_by_name("AP_SECMODE_4",ap_secmode_5g);
			CFG_set_by_name("AP_CYPHER_4",ap_cypher_5g);
			CFG_set_by_name("AP_SECFILE_4",ap_secfile_5g);
//			CFG_set_by_name("AP_CHMODE_3",ap_chmode_5g);
//			CFG_set_by_name("AP_PRIMARY_CH_3",ap_primarych_5g);

//			CFG_set_by_name("AP_CHMODE_4",ap_chmode_5g);
//			CFG_set_by_name("AP_PRIMARY_CH_4",ap_primarych_5g);

			CFG_set_by_name("AP_MODE_4",ap_mode_5g);
			CFG_set_by_name("AP_WPA_4",ap_wpa_5g);
			
			for(i = 0; i < 128; i++)
			{
				if(strlen(bakeup[i]) == 0)
					break;
				
				sprintf(buf, "BACKUP%d", i+1);
				CFG_set_by_name(buf, bakeup[i]);
			}

			
			writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            writeParameters("/tmp/.apcfg","w+",0);
			//[TODO] reset any args
			system("echo \"/index.html:admin:admin\" > /etc/httpd.conf");
			system("echo \"/index2.html:admin:admin\" >> /etc/httpd.conf");
			//MAC and BACKUP
			system("/usr/sbin/var_backup > /dev/null 2>&1");
			system("cat /dev/null > /etc/ath/iptables/parc");
//			system("rm -f /etc/ip_mac.conf");//wangyu add for ip and mac address bond operation
			system("echo `grep 10.10.10.100 /etc/ip_mac.conf` > /etc/ip_mac.conf");//wangyu add for ip and mac address bond operation

			system("rm -f /etc/.staAcl /etc/.staMac");//wangyu add for wireless client manage
			system("rm -f  /etc/arp_ip_mac_on.conf /etc/arp_ip_mac_off.conf /etc/arp_ip_mac.conf");//wangyu add for arp  ip and mac address bond operation
			system("rm -f /etc/route.conf");//wangyu add for static route list

			CFG_set_by_name("FACTORY_RESET","1");
			writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            writeParameters("/tmp/.apcfg","w+",0);
#else
            system("rm -rf /tmp/WSC*.conf");
#endif /* #ifndef ATH_SINGLE_CFG */
            exit(0);
        }
        else if(!strncmp(argv[1],"-v",2))
        {
            /*
            ** Get the VALUE of the parameter without any processing or other
            ** stuff.  The form of this command is
            ** cfg -v NAME index1 index2
            **
            ** where NAME is the UNINDEXED name (such as AP_SSID), and index1 (index2)
            ** are indexes added to the name to get the fully qualified value.  It is
            ** intended that this can be used in the form
            &&
            ** `cfg -v AP_SSID $INDEX`
            **
            ** to allow for looping through parameters.  This is similiar to the -t
            ** function, but is for individual parameters.
            */

            if(argv[1][2] == '0')
                ModeFlag = 0;
            else
                ModeFlag = 1;

            if(argc > 4)
                sprintf(Name,"%s_%s_%s",argv[2],argv[3],argv[4]);
            else if(argc > 3)
                sprintf(Name,"%s_%s",argv[2],argv[3]);
            else
                sprintf(Name,"%s",argv[2]);

            CFG_get_by_name(Name,Value);
            printf("%s",Value);
            exit(0);
        }
#ifdef ATH_SINGLE_CFG
        athcfg_process_commandline(argc, argv);
#endif /* #ifdef ATH_SINGLE_CFG */
    }
#ifdef ATH_SINGLE_CFG
    usage(argv[0]);
#else
    /*
    ** Otherwise, this is processing for an HTML page
    ** Zero out the config structure, and get the page name
    */

    strcpy(Page,"../");
    strcat(Page,argv[0]);
    strcat(Page,".html");
	/*
	if(strcmp(Page,"../map.html")==0)
	{
		//fprintf(errOut,"[luodp] test net\n");
		Execute_cmd("net_test > /dev/null 2>&1", rspBuff);
	}*/
	//fprintf(errOut,"%s  %d Page:%s\n",__func__,__LINE__,Page);
    {

        httpreferer=getenv("REMOTE_ADDR");
        if(httpreferer)
        {
           // fprintf(errOut,"\n----------REMOTE_ADDR:%s success\n",httpreferer);
        }
        else
        {
           // fprintf(errOut,"\n----------REMOTE_ADDR:fail\n",httpreferer);
        }

    }
    /*
    ** Now to get the environment data.
    ** We parse the input until all parameters are inserted.  If we see a reset, commit,
    ** or accept/submit label, we do the appropriate action
    */

    /* if every thing fine, make sure we add 200 OK with proper content type.
     * At this point we do not know Content-Length, but http 1.0 restricts to
     * add content length so just tell the browser read until connection close
     */

    /*
    ** This method allows processing of either "get" or "post" methods.  Post
    ** overrides Get.
    */
    nextField = getenv("CONTENT_LENGTH");

    if (nextField == NULL)
    {
        sprintf(valBuff,"?%s",getenv("QUERY_STRING"));
        nextField = valBuff;
    }
   // fprintf(errOut,"%s  %d nextField:%s\n",__func__,__LINE__ ,nextField);

    if(nextField != NULL)
    {
        if(*nextField != '?')
        {
            j = atoi(nextField);

            memset(opBuff,0,1024);
            fgets(opBuff,j+3,stdin);
            nextField = opBuff;
        }

        fprintf(errOut,"%s  %d nextField:%s\n",__func__,__LINE__ ,nextField);
       {
            char *s="&";
            strcat(nextField,s);
        }
        fprintf(errOut,"%s  %d nextField:%s\n",__func__,__LINE__ ,nextField);
        /*
        ** Check for the reboot button
        ** If hit, we die gloriously
        */

        update = strstr(nextField,"RebootButton");
        if(update)
        {
            reboot(RB_AUTOBOOT);
        }

        /*
        ** We want to read all parameters regardless, and update
        ** what was read from tmp/flash.  If the commit parameter
        ** is set, we will write to flash
        */
        fprintf(errOut,"\n----------Page:%s parameter :\n",Page);
        while(nextField)
        {
        	FILE *fp;
			int ret = 0;
			char id[10];
			struct staList stalist;
            //memset(Value,0x0,70);
            //Value[0]='\0';
            nextField = extractParam(nextField,Name,valBuff);
            unencode(valBuff,Value);

            if(!strcmp("INDEX",Name))
            {
				int num=0;
				static  char rspBuff1[65536];
				static  char rspBuff2[65536];
				static  char rspBuff3[65536];
				static  char rspBuff4[65536];
				static  char rspBuff5[65536];
                parameterIndex = atoi(Value);
                if((parameterIndex==11)&&(lock11==0))//update wifilist
                {
					fprintf(errOut,"[luodp] do update wifilist\n");
					num=0;
					//do check ath0
					//Execute_cmd("iwlist ath0 scanning > /tmp/scanlist", rspBuff);
					if(strcmp(wifi0_flag,"on") == 0 ) //on
					{
						system("iwlist ath0 scanning > /tmp/scanlist 2>&1");
					}
					//mac
					Execute_cmd("cat /tmp/scanlist | grep Cell | awk '{print $5}'", rspBuff1);
					//fprintf(errOut,"\n-[luodp] %s\n",rspBuff1);
					val1[num]=strtok(rspBuff1,"\n");
					while(val1[num]) {
						num++;
						val1[num]=strtok(NULL,"\n"); 
					}
					//fprintf(errOut,"[luodp] new20\n");
					//int i=0;
					/*for(i=1;i<num-1;i++)
					{
						fprintf(errOut,"[luodp] %s\n",val1[i]+4);
					}*/
					num=0;
					//Signal level
					Execute_cmd("cat /tmp/scanlist | grep Signal | awk '{print $3}' | cut -d \"-\" -f2", rspBuff5);
					//fprintf(errOut,"\n[luodp] %s\n",rspBuff5);
					val5[num]=strtok(rspBuff5,"\n");
					while(val5[num]) {
						num++;
						val5[num]=strtok(NULL,"\n"); 
					}
					num=0;
					//ssid
					Execute_cmd("cat /tmp/scanlist | grep ESSID | awk '{print $1}' | cut -d \":\" -f2", rspBuff2);
					//fprintf(errOut,"\n[luodp] %s\n",rspBuff2);
					val2[num]=strtok(rspBuff2,"\n");
					while(val2[num]) {
						num++;
						val2[num]=strtok(NULL,"\n"); 
					}
					/*for(i=1;i<num-1;i++)
					{
						fprintf(errOut,"[luodp] %s\n",val2[i]+4);
					}*/
					num=0;
					//security
					Execute_cmd("cat /tmp/scanlist | grep Encryption | awk '{print $2}' | cut -d \":\" -f2", rspBuff3);
					//fprintf(errOut,"\n[luodp] %s\n",rspBuff);
					val3[num]=strtok(rspBuff3,"\n");
					while(val3[num]) {
						num++;
						val3[num]=strtok(NULL,"\n"); 
					}
					/*for(i=1;i<num-1;i++)
					{
						fprintf(errOut,"[luodp] %s\n",val3[i]+4);
					}*/
					num=0;
					//channel
					Execute_cmd("cat /tmp/scanlist | grep Frequency | awk '{print $4}' | cut -d \")\" -f1", rspBuff4);
					//fprintf(errOut,"\n[luodp] %s\n",rspBuff);
					val4[num]=strtok(rspBuff4,"\n");
					while(val4[num]) {
						num++;
						val4[num]=strtok(NULL,"\n"); 
					}
					/*for(i=1;i<num-1;i++)
					{
						fprintf(errOut,"[luodp] %s\n",val4[i]+4);
					}*/
					lists=num-1;
					/*for(i=1;i<lists;i++)
					{
						fprintf(errOut,"[luodp] %s(%s)(%s)(%s)\n",val2[i]+4,val1[i]+4,val3[i]+4,val4[i]+4);
					}*/
					char cmdd[128]={0};
                    FILE *fp;
                    if((fp=fopen("/tmp/wifilist","w+"))==NULL)
                    {
                        fprintf(errOut,"\n----------cannot open file  line:%d\n",__LINE__);
                        return;
                    }
					if(val3[0]!=NULL)
					{
						//val1-mac,val2-ssid,val3-security,val4-channel
						if(strcmp(val2[0],"\"\""))
						{
							if(strcmp(val3[0],"on")==0)
								sprintf(val3[0],"WPA");
							else  
								sprintf(val3[0],"None");
							memset(cmdd,0x00,128);	
							memset(buf,0x00,100);	
							memcpy(buf,val2[0]+1,(strlen(val2[0])-2));
							sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(-%sdbm)</option>",buf,val1[0],val4[0],val3[0],buf,val5[0]);
							fwrite(cmdd,strlen(cmdd),1,fp);
						}
						for(i=1;i<lists;i++)
						{
							memset(cmdd,0x00,128);
							if(strcmp(val2[i]+4,"\"\""))
							{
								if(strcmp(val3[i]+4,"on")==0)
									sprintf(val3[i]+4,"WPA");
								else
									sprintf(val3[i]+4,"None");
								//fprintf(errOut,"[luodp] %s(%s)(%s)(%s) \n",val2[i]+4,val1[i]+4,val3[i]+4,val4[i]+4);
								memset(buf,0x00,100);	
								memcpy(buf,val2[i]+5,(strlen(val2[i]+4)-2));
								sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(-%sdbm)</option>",buf,val1[i]+4,val4[i]+4,val3[i]+4,buf,val5[i]+4);
								fwrite(cmdd,strlen(cmdd),1,fp);
							}
						}
					}
                    fclose(fp);
					flaglist=1;
                    lock11 = 1;
                }
				else if((parameterIndex==112)&&(lock112==0))//update wifilist_2
                {
					fprintf(errOut,"[luodp] do update wifilist_2\n");
					num=0;
					//do check ath2
					//Execute_cmd("iwlist ath2 scanning > /tmp/scanlist", rspBuff);
					
					if(strcmp(wifi1_flag,"on") == 0 ) //on
					{
						system("iwlist ath2 scanning > /tmp/scanlist_2 2>&1");
					}
					//mac
					Execute_cmd("cat /tmp/scanlist_2 | grep Cell | awk '{print $5}'", rspBuff1);
					//fprintf(errOut,"\n-[luodp] %s\n",rspBuff1);
					val1[num]=strtok(rspBuff1,"\n");
					while(val1[num]) {
						num++;
						val1[num]=strtok(NULL,"\n"); 
					}
					//fprintf(errOut,"[luodp] new20\n");
					//int i=0;
					/*for(i=1;i<num-1;i++)
					{
						fprintf(errOut,"[luodp] %s\n",val1[i]+4);
					}*/
					num=0;
					//Signal level
					Execute_cmd("cat /tmp/scanlist_2 | grep Signal | awk '{print $3}' | cut -d \"-\" -f2", rspBuff5);
					//fprintf(errOut,"\n[luodp] %s\n",rspBuff5);
					val5[num]=strtok(rspBuff5,"\n");
					while(val5[num]) {
						num++;
						val5[num]=strtok(NULL,"\n"); 
					}
					num=0;
					//ssid
					Execute_cmd("cat /tmp/scanlist_2 | grep ESSID | awk '{print $1}' | cut -d \":\" -f2", rspBuff2);
					//fprintf(errOut,"\n[luodp] %s\n",rspBuff2);
					val2[num]=strtok(rspBuff2,"\n");
					while(val2[num]) {
						num++;
						val2[num]=strtok(NULL,"\n"); 
					}
					/*for(i=1;i<num-1;i++)
					{
						fprintf(errOut,"[luodp] %s\n",val2[i]+4);
					}*/
					num=0;
					//security
					Execute_cmd("cat /tmp/scanlist_2 | grep Encryption | awk '{print $2}' | cut -d \":\" -f2", rspBuff3);
					//fprintf(errOut,"\n[luodp] %s\n",rspBuff);
					val3[num]=strtok(rspBuff3,"\n");
					while(val3[num]) {
						num++;
						val3[num]=strtok(NULL,"\n"); 
					}
					/*for(i=1;i<num-1;i++)
					{
						fprintf(errOut,"[luodp] %s\n",val3[i]+4);
					}*/
					num=0;
					//channel
					Execute_cmd("cat /tmp/scanlist_2 | grep Frequency | awk '{print $4}' | cut -d \")\" -f1", rspBuff4);
					//fprintf(errOut,"\n[luodp] %s\n",rspBuff);
					val4[num]=strtok(rspBuff4,"\n");
					while(val4[num]) {
						num++;
						val4[num]=strtok(NULL,"\n"); 
					}
					/*for(i=1;i<num-1;i++)
					{
						fprintf(errOut,"[luodp] %s\n",val4[i]+4);
					}*/
					lists=num-1;
					/*for(i=1;i<lists;i++)
					{
						fprintf(errOut,"[luodp] %s(%s)(%s)(%s)\n",val2[i]+4,val1[i]+4,val3[i]+4,val4[i]+4);
					}*/
					char cmdd[128]={0};
                    FILE *fp;
                    if((fp=fopen("/tmp/wifilist_2","w+"))==NULL)
                    {
                        fprintf(errOut,"\n----------cannot open file  line:%d\n",__LINE__);
                        return;
                    }
					if(val3[0]!=NULL)
					{
						//val1-mac,val2-ssid,val3-security,val4-channel
						if(strcmp(val2[0],"\"\""))
						{
							if(strcmp(val3[0],"on")==0)
								sprintf(val3[0],"WPA");
							else  
								sprintf(val3[0],"None");
							memset(cmdd,0x00,128);
							memset(buf,0x00,100);	
							memcpy(buf,val2[0]+1,(strlen(val2[0])-2));
		//					fprintf(errOut,"[luodp] buff----------- %s\n",buf);
							sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(-%sdbm)</option>",buf,val1[0],val4[0],val3[0],buf,val5[0]);
							fwrite(cmdd,strlen(cmdd),1,fp);
						}
						for(i=1;i<lists;i++)
						{
							memset(cmdd,0x00,128);
							if(strcmp(val2[i]+4,"\"\""))
							{
								if(strcmp(val3[i]+4,"on")==0)
									sprintf(val3[i]+4,"WPA");
								else
									sprintf(val3[i]+4,"None");
								//fprintf(errOut,"[luodp] %s(%s)(%s)(%s) \n",val2[i]+4,val1[i]+4,val3[i]+4,val4[i]+4);
								memset(buf,0x00,100);	
								memcpy(buf,val2[i]+5,(strlen(val2[i]+4)-2));
	//							fprintf(errOut,"[luodp] buff----------- %s\n",buf);
								sprintf(cmdd,"<option id=\"%s(%s)(%s)(%s)\">%s(-%sdbm)</option>",buf,val1[i]+4,val4[i]+4,val3[i]+4,buf,val5[i]+4);
								fwrite(cmdd,strlen(cmdd),1,fp);
							}
						}
					}
                    fclose(fp);
					flaglist_2=1;
                    lock112 = 1;
                }
                else if((parameterIndex==12)&&(lock12==0))//do wan mode detect
                {
					Execute_cmd("net_check", rspBuff);
				    memset(&config,0,sizeof(config));
                    f = fopen("/tmp/.apcfg","r");
                    if ( !f )
                    {
                        f = fopen( NVRAM, "r" );
                        if (!f)
                        {
                        fprintf(errOut,"ERROR:  %s not defined on this device\n", NVRAM);
                        fprintf(errOut,"ERROR:  Cannot store data in flash!!!!\n");
                        exit(-1);
                        }

                        fseek(f, NVRAM_OFFSET, SEEK_SET);
                    }
                    if ( f )
                    {
                        fillParamStruct(f);
                        fclose(f);
                    }
                    lock12 = 1;
                }
				else if((parameterIndex==13)&&(lock13==0))//do wan mode detect
                {
					Execute_cmd("wan_check", rspBuff);
				    memset(&config,0,sizeof(config));
                    f = fopen("/tmp/.apcfg","r");
                    if ( !f )
                    {
                        f = fopen( NVRAM, "r" );
                        if (!f)
                        {
                        fprintf(errOut,"ERROR:  %s not defined on this device\n", NVRAM);
                        fprintf(errOut,"ERROR:  Cannot store data in flash!!!!\n");
                        exit(-1);
                        }

                        fseek(f, NVRAM_OFFSET, SEEK_SET);
                    }
                    if ( f )
                    {
                        fillParamStruct(f);
                        fclose(f);
                    }
					lock13 = 1;
				}
               sprintf(Value,"%d",parameterIndex);
            }
			else
            {
                if(strcmp(argv[0],"ad_wireless_wds")==0)
                {
					flaglist_2=0;
					flaglist=0;

                }
            }
             fprintf(errOut,"Name:%s Value:%s\n",Name,Value);
			 /*don't write something to flash*/
			 if ((fp = fopen(STA_MAC, "r")) != NULL)
			 {
			 	while(fread(&stalist, sizeof stalist, 1, fp) == 1)
				{
					sprintf(id,"%d", stalist.id);
					if(!strcmp(id, Name))
					{
						ret = 1;
						continue;
					}
				}
				fclose(fp);
			 }
			 //fprintf(errOut,"ret :%d\n", ret);
			 if(ret == 1)
				continue;
			 
             CFG_set_by_name(Name,Value);
        }
    if(parameterIndex)
		radioIndex = getRadioID(parameterIndex);

    }
    fprintf(errOut,"\n#######-Page:%s parameter .\n",Page);
    fprintf(errOut,"\n%s  %d valBuff:%s\n \n",__func__,__LINE__,valBuff);
    /*
    ** use the "translate file" function to translate the file, inserting the
    ** special strings as required.
    */
   // fprintf(errOut,"%s  %d Page:%s\n",__func__,__LINE__,Page);
   sync();
   // fprintf(errOut,"%s  %d\n",__func__,__LINE__);


    /*
    ** Now, look for the update and/or commit strings to send to either
    ** the temp file or the flash file
    */
	//wan mode dhcp
     if((strcmp(CFG_get_by_name("DHCP",valBuff),"DHCP") == 0 )|| (strcmp(CFG_get_by_name("DHCPW",valBuff),"DHCPW")== 0 ))
     {
        fprintf(errOut,"\n%s  %d DHCP \n",__func__,__LINE__);
		int flag=0;
		//if dhcp fail ,WAN_MODE rollback to last value
		char tmp[128],*tmp2;
		//static  char     rspBuff2[65536];
		
		if(strcmp(CFG_get_by_name("DHCPW",valBuff),"DHCPW")== 0 )
		{
			flag=1;
		}
		
		system("ifconfig eth0 0.0.0.0 up");   //wangyu add for the wan mode change from static to dhcp           

		Execute_cmd("wan_check  > /dev/null 2>&1", rspBuff);
		Execute_cmd("cfg -e | grep \"LINEIN_OUT=\"",valBuff);
		if(strstr(valBuff,"out") != 0)
		{
			char tempu[128]={0};
			printf("HTTP/1.0 200 OK\r\n");
			printf("Content-type: text/html\r\n");
			printf("Connection: Close\r\n");
			printf("\r\n");
			printf("\r\n");

			printf("<HTML><HEAD>\r\n");
			printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
			printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
			printf("</head><body>");	
                 
			if((strcmp(argv[0],"w1")==0)||(strcmp(argv[0],"w2")==0)||(strcmp(argv[0],"w3")==0)||(strcmp(argv[0],"w4")==0)||(strcmp(argv[0],"wwai")==0))
			{
			    sprintf(tempu,"<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();alert(_(\"wan not connected\"));window.parent.showpwd1();window.location.href=\"map?map=yes\";</script>");
			}
			else
			{
			    sprintf(tempu,"<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();alert(_(\"wan not connected\"));window.location.href=\"%s?%s=yes\";</script>",argv[0],argv[0]);
			}
			printf(tempu);
			printf("</body></html>");
			exit(1);
		}
		//CFG_set_by_name("AP_STARTMODE","standard");
		//1.destory old mode pid 
		Execute_cmd("cfg -e | grep \"WAN_MODE=\" | awk -F \"=\" '{print $2}'",tmp);
		//Execute_cmd("ps | grep udhcpc",rspBuff2);
		if(strstr(tmp,"pppoe") != 0)
		{
			//kill pppoe
			Execute_cmd("pppoe-stop > /dev/null 2>&1", rspBuff);
		}
		if(strstr(tmp,"dhcp") != 0)
		{
			//[TODO]kill udhcpc that may slow and httpd: bind: Address already in use
			//Execute_cmd("ps | grep udhcpc | awk \'{print $1}\' | xargs kill -9 > /dev/null 2>&1", rspBuff);	
			Execute_cmd("killall udhcpc > /dev/null 2>&1", rspBuff);
			//flag=1;
		}
		if(flag!=1)
		{
			//2.save new config to flash 
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
		}
		//3.do new config pid
		//if(flag!=1)
		Execute_cmd("udhcpc -b -i eth0 -h HBD-Router -s /etc/udhcpc.script > /dev/null 2>&1", rspBuff);
		//4.check the ip address 
		//unnormal 、none、normal
		Execute_cmd("cfg -e | grep \"WAN_IPFLAG=\"",valBuff);
		//get ip fail
		if(strstr(valBuff,"none") != 0)
		{
				char tempu[128]={0};
				 //WAN_MODE rollback to old value
				 tmp2=strtok(tmp,"\n");
				 CFG_set_by_name("WAN_MODE",tmp2);
				 if(flag!=1)
				 {
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
		         }
                 printf("HTTP/1.0 200 OK\r\n");
                 printf("Content-type: text/html\r\n");
                 printf("Connection: Close\r\n");
                 printf("\r\n");
                 printf("\r\n");
				 printf("<HTML><HEAD>\r\n");
				 printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
				 printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
				 printf("</head><body>");	
                 
                            if((strcmp(argv[0],"w1")==0)||(strcmp(argv[0],"w2")==0)||(strcmp(argv[0],"w3")==0)||(strcmp(argv[0],"w4")==0)||(strcmp(argv[0],"wwai")==0))
                            {
                                sprintf(tempu,"<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();alert(_(\"err dhcp null\"));window.parent.showpwd1();window.location.href=\"map?map=yes\";</script>");
                            }
                            else
                            {
                                sprintf(tempu,"<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();alert(_(\"err dhcp null\"));window.location.href=\"%s?%s=yes\";</script>",argv[0],argv[0]);
                            }
				 printf(tempu);
				 printf("</body></html>");
				 exit(1);
		}
		//wan ip and gateway in same subnet
		if(strstr(valBuff,"unnormal") != 0)
		{
                 char tempu2[128]={0};
				 //WAN_MODE rollback to old value
				 tmp2=strtok(tmp,"\n");
				 CFG_set_by_name("WAN_MODE",tmp2);
				 if(flag!=1)
				 {
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
		         }
				 
                 printf("HTTP/1.0 200 OK\r\n");
                 printf("Content-type: text/html\r\n");
                 printf("Connection: Close\r\n");
                 printf("\r\n");
                 printf("\r\n");      
				 printf("<HTML><HEAD>\r\n");
				 printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
				 printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
				 printf("</head><body>");
                            if((strcmp(argv[0],"w1")==0)||(strcmp(argv[0],"w2")==0)||(strcmp(argv[0],"w3")==0)||(strcmp(argv[0],"w4")==0)||(strcmp(argv[0],"wwai")==0))
                            {
                                sprintf(tempu2,"<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();alert(_(\"err dhcp ip\"));window.parent.showpwd1();window.location.href=\"map?map=yes\";</script>");
                            }
                            else
                            {
                                sprintf(tempu2,"<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();alert(_(\"err dhcp ip\"));window.location.href=\"%s?%s=yes\";</script>",argv[0],argv[0]);
                            }

				 printf(tempu2);
				 printf("</body></html>");
				 exit(1);
		}
		gohome =1;
    }
	//wan mode static ip
    if((strcmp(CFG_get_by_name("SIP",valBuff),"SIP") == 0 ) || (strcmp(CFG_get_by_name("SIPW",valBuff),"SIPW") == 0 ))
    {
		fprintf(errOut,"\n%s  %d SIP \n",__func__,__LINE__);
	
		char pChar[128];
		char valBuff2[128];	
		int flag=0;
		if(strcmp(CFG_get_by_name("SIPW",valBuff),"SIPW") == 0 )
		{
			flag=1;
		}
		
		//CFG_set_by_name("AP_STARTMODE","standard");
		//1.destory old mode pid
		Execute_cmd("cfg -e | grep \"WAN_MODE=\"",valBuff);
		//fprintf(errOut,"[luodp] WAN_MODE: %s\n",valBuff);
		if(strstr(valBuff,"pppoe") != 0)
		{
			//kill pppoe
			Execute_cmd("pppoe-stop > /dev/null 2>&1", rspBuff);
		}
		if(strstr(valBuff,"dhcp") != 0)
		{
			//kill udhcpc
			//Execute_cmd("ps aux | grep udhcpc | awk \'{print $1}\' | xargs kill -9  > /dev/null 2>&1", rspBuff);
			Execute_cmd("killall udhcpc > /dev/null 2>&1", rspBuff);
		}
		if(flag!=1)
		{
			//2.save new config to flash 
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
            
            writeParametersWithSync();
		}
		//3.do new config pid
		system("ifconfig eth0 down > /dev/null 2>&1");
		system("ifconfig eth0 up > /dev/null 2>&1");
		
		CFG_get_by_name("WAN_IPADDR",valBuff);
		CFG_get_by_name("WAN_NETMASK",valBuff2);
		sprintf(pChar,"ifconfig eth0 %s netmask %s up > /dev/null 2>&1",valBuff,valBuff2);
		Execute_cmd(pChar, rspBuff);
		
		//[TODO] DNS ,use system default dns 8.8.8.8
		//[TODO] may be this is a bug
		
		CFG_get_by_name("IPGW",valBuff);
		sprintf(pChar,"route add default gw %s dev eth0 > /dev/null 2>&1",valBuff);
		Execute_cmd(pChar, rspBuff);
		
		/*
		Execute_cmd("route -n", rspBuff);
		if(strstr(rspBuff,valBuff) == 0)
		{
			//fprintf(errOut,"[luodp] ishere34\n");
			sprintf(pChar,"route add default gw %s dev eth0 > /dev/null 2>&1",valBuff);
			Execute_cmd(pChar, rspBuff);
		}
		*/
		gohome =1;
    }


	//wan mode l2tp
     if((strcmp(CFG_get_by_name("L2TP",valBuff),"L2TP") == 0 ) ||(strcmp(CFG_get_by_name("L2TPW",valBuff),"L2TPW") == 0 ))
    {
        fprintf(errOut,"\n%s  %d L2TP \n",__func__,__LINE__);
        gohome =1;
    }
	//wan mode p2tp
     if((strcmp(CFG_get_by_name("P2TP",valBuff),"P2TP") == 0 ) || (strcmp(CFG_get_by_name("P2TPW",valBuff),"P2TPW") == 0 ))
    {
        fprintf(errOut,"\n%s  %d P2TP \n",__func__,__LINE__);
        gohome =1;
    }
	//wds 
     if(strcmp(CFG_get_by_name("WIRRE",valBuff),"WIRRE") == 0 ) 
     { 
		fprintf(errOut,"\n%s  %d WIRRE \n",__func__,__LINE__);
		
		char pChar[128];
		char mac[128];
		char channel[128];
		char channel_buf[128];
		char valBuff2[128];	
		char valBuff3[128];	
		char valBuff4[128];
		char valBuff5[128];
		char channel_5g[128];
		char channel_buf_5g[128];
		char valBuff2_5g[128];	
		char valBuff3_5g[128];	
		char valBuff4_5g[128];
		char valBuff5_5g[128];
		int flag=0;
		
		//2.get old config from flash 
		Execute_cmd("cfg -e | grep 'AP_SECMODE_2=' |  awk -F '\"' '{print $2}'",valBuff2);
		Execute_cmd("cfg -e | grep 'WDSON_OFF=' | awk -F '=' '{print $2}'",valBuff5);

		Execute_cmd("cfg -e | grep 'WDSON_OFF_3=' | awk -F '=' '{print $2}'",valBuff2_5g);
		
		//fprintf(errOut,"[luodp] wds %s,%s\n",valBuff2,valBuff5);
		CFG_get_by_name("WDSON_OFF",valBuff3);
		CFG_get_by_name("WDSON_OFF_3",valBuff3_5g);
//		fprintf(errOut,"[luodp] -----------  wds %s,%s----------%s,%s--------\n",valBuff3,valBuff5,
//			valBuff2_5g,valBuff3_5g);

		//on
		if((strcmp(valBuff3,"on") == 0)||(strcmp(valBuff3_5g,"on") == 0))
		{
			CFG_set_by_name("AP_STARTMODE","repeater");
			CFG_set_by_name("DHCPON_OFF","off");
			flag=1;
		}
		//off
		if((strcmp(valBuff3,"off") == 0)&&(strcmp(valBuff3_5g,"off") == 0))
		{
//			CFG_set_by_name("AP_STARTMODE","standard");
			if((strcmp(valBuff5,"on") == 0)||(strcmp(valBuff2_5g,"on") == 0))
				CFG_set_by_name("AP_STARTMODE","dual");
			CFG_set_by_name("DHCPON_OFF","on");
			flag=2;
		}

		//2.save new config to flash 
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
		//3.do new settings
		if((flag==3)||(flag==1))
		{
		if(strcmp(valBuff3,"on") == 0){
			CFG_get_by_name("STA_SECMODE",valBuff3);
			if(strcmp(valBuff3,"None") != 0)
			{
				//AP_SECMODE=WPA
				//AP_WPA=3 WPA/WPA2
				//AP_CYPHER="TKIP CCMP"
				//CFG_set_by_name("AP_RADIO_ID_2","0");
//				CFG_set_by_name("AP_SECMODE_2","WPA");
//				CFG_set_by_name("AP_WPA_2","3");
//				CFG_set_by_name("AP_CYPHER_2","TKIP CCMP");
//				CFG_set_by_name("AP_SECFILE_2","PSK"); 
				CFG_set_by_name("STA_SECMODE","WPA");
				CFG_set_by_name("STA_WPA","3");
//				CFG_set_by_name("STA_CYPHER","TKIP CCMP");
				CFG_set_by_name("STA_CYPHER","CCMP");
				CFG_set_by_name("STA_SECFILE","PSK"); 
			}
			if(strcmp(valBuff3,"WPA") != 0)
			{
				//AP_SECMODE=WPA
				//AP_WPA=3 WPA/WPA2
				//AP_CYPHER="TKIP CCMP"
				//CFG_set_by_name("AP_RADIO_ID_2","0");
//				CFG_set_by_name("AP_SECMODE_2","None");
				CFG_set_by_name("STA_SECMODE","None");
			}
			CFG_get_by_name("WDS_CHAN",channel);
			CFG_get_by_name("AP_PRIMARY_CH",channel_buf);
//			fprintf(errOut,"[luodp] -----------  wds %s\n",channel_buf);
			CFG_set_by_name("AP_PRIMARY_CH_BACK",channel_buf);
			
			CFG_set_by_name("AP_PRIMARY_CH",channel);
			}

		
		if(strcmp(valBuff3_5g,"on") == 0){
			CFG_get_by_name("STA_SECMODE_2",valBuff4_5g);
			if(strcmp(valBuff4_5g,"None") != 0)
			{
				//AP_SECMODE=WPA
				//AP_WPA=3 WPA/WPA2
				//AP_CYPHER="TKIP CCMP"
				//CFG_set_by_name("AP_RADIO_ID_2","0");

				CFG_set_by_name("STA_SECMODE_2","WPA");
				CFG_set_by_name("STA_WPA_2","3");
//				CFG_set_by_name("STA_CYPHER_2","TKIP CCMP");
				CFG_set_by_name("STA_CYPHER_2","CCMP");
				CFG_set_by_name("STA_SECFILE_2","PSK"); 
			}
			if(strcmp(valBuff4_5g,"WPA") != 0)
			{
				//AP_SECMODE=WPA
				//AP_WPA=3 WPA/WPA2
				//AP_CYPHER="TKIP CCMP"
				//CFG_set_by_name("AP_RADIO_ID_2","0");
//				CFG_set_by_name("AP_SECMODE_2","None");
				CFG_set_by_name("STA_SECMODE_2","None");
			}
			CFG_get_by_name("WDS_CHAN_2",channel_5g);
			CFG_get_by_name("AP_PRIMARY_CH_3",channel_buf_5g);
//			fprintf(errOut,"[luodp] -----------  wds channel_buf_5g %s\n",channel_buf_5g);
			CFG_set_by_name("AP_PRIMARY_CH_BACK_3",channel_buf_5g);
			CFG_set_by_name("AP_PRIMARY_CH_3",channel_5g);
			}
			//4.save new config to flash 
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
		}
		//5.do settings
		
		if(strcmp(valBuff3,"off") == 0)
		{
			CFG_get_by_name("AP_PRIMARY_CH_BACK",channel_buf);
//			fprintf(errOut,"[luodp] -----------  wds %s\n",channel_buf);
			CFG_set_by_name("AP_PRIMARY_CH",channel_buf);
		}

		
		if(strcmp(valBuff3_5g,"off") == 0)
		{
			CFG_get_by_name("AP_PRIMARY_CH_BACK_3",channel_buf_5g);
//			fprintf(errOut,"[luodp] -----------  wds channel_buf_5g %s\n",channel_buf_5g);
			CFG_set_by_name("AP_PRIMARY_CH_3",channel_buf_5g);
		}
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);

		//TODO if close do commit no ath1
		if(flag==2) //on->off
		{
			//Execute_cmd("wlanconfig ath1 destroy > /dev/null 2>&1", rspBuff);
			Execute_cmd("cfg -e | grep 'WIFION_OFF=' | awk -F \"=\" \'{print $2}\'",valBuff);
			//wifi on ,then wds on/off
			if(strstr(valBuff,"off") == 0)
			{
				//[TODO]sta mode
//				Execute_cmd("apdown > /dev/null 2>&1", rspBuff);
//				Execute_cmd("apup > /dev/null 2>&1", rspBuff);
				Execute_cmd("repeatVAP > /dev/null 2>&1", rspBuff);

				//kill udhcpd
				//[TODO]if dhcp no exist ,then up
				//Execute_cmd("ps | grep udhcpd",rspBuff);
				//if(strstr(rspBuff,"udhcpd")==0)
				//{
					Execute_cmd("killall udhcpd > /dev/null 2>&1",rspBuff);
					Execute_cmd("/usr/sbin/udhcpd /etc/udhcpd.conf > /dev/null 2>&1",rspBuff);
					//[TODO] wired device re-assign
				//}
				//fprintf(errOut,"[luodp] wds open\n");

				//reboot hostapd
				Execute_cmd("killall hostapd > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath0 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath1 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath2 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath3 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
			}
		}		
		if((flag==1)||(flag==3)) //off->on and on->on 
		{
			Execute_cmd("cfg -e | grep 'WIFION_OFF=' | awk -F \"=\" \'{print $2}\'",valBuff);
			//wifi on ,then wds on/off
			if(strstr(valBuff,"off") == 0)
			{
				//if dhcp exist ,then kill
				Execute_cmd("killall udhcpd > /dev/null 2>&1",rspBuff);
				Execute_cmd("rm -rf /var/run/udhcpd.leases > /dev/null 2>&1",rspBuff);
				//fprintf(errOut,"[luodp] wds open\n");
				//[TODO]sta mode
	//			Execute_cmd("apdown > /dev/null 2>&1", rspBuff);
	//			Execute_cmd("apup > /dev/null 2>&1", rspBuff);
				Execute_cmd("repeatVAP > /dev/null 2>&1", rspBuff);
				
				//reboot hostapd
				Execute_cmd("killall hostapd > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath0 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath1 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath2 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath3 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
			}
		}
		/*CFG_get_by_name("AP_RADIO_ID_2",valBuff);
		CFG_get_by_name("AP_SECMODE_2",valBuff2);
		CFG_get_by_name("AP_SECFILE_2",valBuff3);
		CFG_get_by_name("WPS_ENABLE_2",valBuff4);
		sprintf(pChar,"activateVAP ath1:%s br0 %s %s %s",valBuff,valBuff2,valBuff3,valBuff4);
		
		Execute_cmd(pChar, rspBuff);*/
		//maybe can replace by makeVAP
		/*Execute_cmd("wlanconfig ath1 create wlandev wifi0 wlanmode sta nosbeacon", rspBuff);
		CFG_get_by_name("AP_SSID_2",valBuff);
		sprintf(pChar,"iwconfig ath1 essid %s",valBuff);
		Execute_cmd(pChar, rspBuff);
		
		CFG_get_by_name("WDS_MAC",mac);
		sprintf(pChar,"iwconfig ath1 ap  %s",mac);
		Execute_cmd(pChar, rspBuff);*/
		#if 0
		if((flag==1)||(flag==3))
		{
			if(strcmp(valBuff3,"on") == 0){
				sprintf(pChar,"iwconfig ath0 channel %s  > /dev/null 2>&1",channel);	
				Execute_cmd(pChar, rspBuff);
				sprintf(pChar,"iwconfig ath4 channel %s  > /dev/null 2>&1",channel);	
				Execute_cmd(pChar, rspBuff);
				
				if(strcmp(valBuff3_5g,"on") == 0){
					sprintf(pChar,"iwconfig ath2 channel %s  > /dev/null 2>&1",channel_5g);	
					Execute_cmd(pChar, rspBuff);
					sprintf(pChar,"iwconfig ath5 channel %s  > /dev/null 2>&1",channel_5g);	
					Execute_cmd(pChar, rspBuff);
				}
			}
			else {
				sprintf(pChar,"iwconfig ath2 channel %s  > /dev/null 2>&1",channel_5g);	
				Execute_cmd(pChar, rspBuff);
				sprintf(pChar,"iwconfig ath4 channel %s  > /dev/null 2>&1",channel_5g);	
				Execute_cmd(pChar, rspBuff);
			}
		}
		#endif
		/*Execute_cmd("iwpriv ath1 wds 1", rspBuff);
		Execute_cmd("brctl addif br0 ath1", rspBuff);
		Execute_cmd("ifconfig ath1 up", rspBuff);*/
		gohome =1;
    }
	//wifi settings
     if((strcmp(CFG_get_by_name("WIRELESS",valBuff),"WIRELESS") == 0 ) || (strcmp(CFG_get_by_name("DHCPW",valBuff),"DHCPW") == 0 ) || (strcmp(CFG_get_by_name("SIPW",valBuff),"SIPW") == 0 ) || (strcmp(CFG_get_by_name("PPPW",valBuff),"PPPW") == 0 ) || (strcmp(CFG_get_by_name("L2TPW",valBuff),"L2TPW") == 0 ) || (strcmp(CFG_get_by_name("P2TPW",valBuff),"P2TPW") == 0 ) )
    {
		fprintf(errOut,"\n%s  %d WIRELESS \n",__func__,__LINE__);
	
		char pChar[128];
		char valBuff2[128];	
		char valBuff3[128];	
		char valBuff4[128];	
		char valBuff5[128];
		char valBuff6[128];	
		char valBuff7[128];	
		char valBuff8[128];	
		char valBuff9[128];
		char valBuff10[128];
		
		char valBuff_3[128];	
		char valBuff2_3[128];	
		char valBuff4_3[128];	
		char valBuff5_3[128];
		char valBuff7_3[128];	
		char valBuff8_3[128];	
		char valBuff10_3[128];
		int flag=0;
		int ii;
        int led_flag=0;

		//1.get old value from flash
		Execute_cmd("cfg -e | grep \"WIFION_OFF=\" | awk -F \"=\" \'{print $2}\'",valBuff);
		//TODO bug
		Execute_cmd("cfg -e | grep \"AP_SSID=\" | awk -F \"AP_SSID=\" '{print $2}'",valBuff2);
		Execute_cmd("cfg -s | grep \"PSK_KEY:\" | awk -F \"=\" \'{print $2}\'",valBuff4);
		//Execute_cmd("cfg -e | grep \"AP_SECMODE=\" |  awk -F \"=\" \'{print $2}\'",valBuff5);;
		//Execute_cmd("cfg -e | grep \"WIFION_OFF=\"",valBuff);
		//Execute_cmd("cfg -e | grep \"AP_SSID=\"",valBuff2);
		//Execute_cmd("iwconfig ath0 | grep \"ESSID\" | awk -F \"\"\" \'{print $2}\'",valBuff2);
		Execute_cmd("cfg -e | grep \"AP_SECMODE=\"",valBuff5);
		Execute_cmd("cfg -e | grep \"AP_PRIMARY_CH=\" | awk -F \"=\" \'{print $2}\'",valBuff7);
		Execute_cmd("cfg -e | grep \"AP_HIDESSID=\" | awk -F \"=\" \'{print $2}\'",valBuff8);
		Execute_cmd("cfg -e | grep \"AP_CHMODE=\" | awk -F \"=\" \'{print $2}\'",valBuff10);

		//get 5G's old value from flash
		Execute_cmd("cfg -e | grep \"WIFION_OFF_3=\" | awk -F \"=\" \'{print $2}\'",valBuff_3);
		Execute_cmd("cfg -e | grep \"AP_SSID_3=\" | awk -F \"AP_SSID_3=\" '{print $2}'",valBuff2_3);
		Execute_cmd("cfg -s | grep \"PSK_KEY_3:\" | awk -F \"=\" \'{print $2}\'",valBuff4_3);
		Execute_cmd("cfg -e | grep \"AP_SECMODE_3=\"",valBuff5_3);
		Execute_cmd("cfg -e | grep \"AP_PRIMARY_CH_3=\" | awk -F \"=\" \'{print $2}\'",valBuff7_3);
		Execute_cmd("cfg -e | grep \"AP_HIDESSID_3=\" | awk -F \"=\" \'{print $2}\'",valBuff8_3);
		Execute_cmd("cfg -e | grep \"AP_CHMODE_3=\" | awk -F \"=\" \'{print $2}\'",valBuff10_3);
		
		//fprintf(errOut,"[luodp] WIFI: %s\n%s%s\n%s\n",valBuff,valBuff2,valBuff5,valBuff4);
		//2.save new config to flash
           
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);			
		//fprintf(errOut,"[luodp]here\n");
		//3.do new config pid
		//TODO key check
		CFG_get_by_name("PSK_KEY",valBuff3);
		//fprintf(errOut,"[luodp] %shere\n",valBuff3);
		//sprintf(valBuff6,"%s\n<br>",valBuff3);
		sprintf(valBuff6,"%s\n",valBuff3);
		/*deal PSK_KEY*/
		ii = 0;
		if(strstr(valBuff4, "<br>"))
		{
			while(valBuff4[ii])
			{
				if(valBuff4[ii] == '\n')
				{
					ii++;
					memcpy(valBuff9, valBuff4, ii);
					break;
				}
				ii++;
				//fprintf(errOut," is [%c] \n", valBuff4[ii]);
			}
		}
		//fprintf(errOut,"valBuff6 is [%s]\n", valBuff6);
		//fprintf(errOut,"valBuff9 is [%s]\n", valBuff9);
		if((strcmp(valBuff6,valBuff9))&&(strcmp(valBuff6,"\n") != 0))
		{
			//fprintf(errOut,"[luodp] KEY here");
			CFG_set_by_name("PSK_KEY",valBuff3);
			//flag=2;
			flag = 5;
			//fprintf(errOut,"11111 2.4G flag is %d\n", flag);
		}
		//TODO SECMODE
		CFG_get_by_name("AP_SECMODE",valBuff3);
		//fprintf(errOut,"[luodp] %s here2\n",valBuff3);
		if((strstr(valBuff5,valBuff3) == 0)&&(strcmp(valBuff3,"None") != 0))
		{
			//AP_SECMODE=WPA
			//AP_WPA=3 WPA/WPA2
			//AP_CYPHER="TKIP CCMP"
			//fprintf(errOut,"[luodp] SECMODE here");
			CFG_set_by_name("AP_SECMODE","WPA");
			CFG_set_by_name("AP_WPA","3");
//			CFG_set_by_name("AP_CYPHER","TKIP CCMP");
			CFG_set_by_name("AP_CYPHER","CCMP");
			CFG_set_by_name("AP_SECFILE","PSK"); 
			//flag=2;
			flag = 5;
			//fprintf(errOut,"22222 2.4G flag is %d\n", flag);
		}
		if((strstr(valBuff5,valBuff3) == 0)&&(strcmp(valBuff3,"WPA") != 0))
		{
			CFG_set_by_name("AP_SECMODE","None");
			Execute_cmd("cfg -r PSK_KEY > /dev/null 2>&1", rspBuff);
			flag=5;
			//Execute_cmd("iwpriv ath0 authmode 1 > /dev/null 2>&1", rspBuff);
			//fprintf(errOut,"333333 2.4G flag is %d\n", flag);
		}
		//4.save new add  to flash 
            
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);					
		CFG_get_by_name("WIFION_OFF",valBuff3);
		//fprintf(errOut,"[luodp] %s here4\n",valBuff3);
		if((strstr(valBuff,valBuff3) == 0) && (strcmp(valBuff3,"on") == 0) )
		{
			//fprintf(errOut,"[luodp] SECMODE here3");
			//Execute_cmd("apup > /dev/null 2>&1", rspBuff);
			Execute_cmd("ifconfig ath0 up > /dev/null 2>&1", rspBuff);
			Execute_cmd("echo enable > /dev/wifiled", rspBuff);
			flag=3;
		}
		if(strcmp(valBuff3,"off") == 0) 
		{
			//fprintf(errOut,"[luodp] SECMODE here4");
			//Execute_cmd("apdown > /dev/null 2>&1", rspBuff);
			Execute_cmd("ifconfig ath0 down > /dev/null 2>&1", rspBuff);
			//flag=5;
            led_flag++;
		}
		//ssid [TODO]
		CFG_get_by_name("AP_SSID",valBuff4);
        if(valBuff2[0]=='"')
            sprintf(valBuff5,"\"%s\"\n<br>",valBuff4);
        else
            sprintf(valBuff5,"%s\n<br>",valBuff4);
		//fprintf(errOut,"[%s][%s]\n", valBuff2, valBuff5);
		//if((strcmp(valBuff2,valBuff5) != 0)&&(flag==0)&& (strcmp(valBuff3,"on") == 0))
		if(strcmp(valBuff2, valBuff5) && (strcmp(valBuff3,"on") == 0))
		{
			//fprintf(errOut,"[luodp] SECMODE here6");
			sprintf(pChar,"iwconfig ath0 essid %s  > /dev/null 2>&1",valBuff4);
			Execute_cmd(pChar, rspBuff);
			flag = 5;
			#if 0
			Execute_cmd("cfg -e | grep \"AP_SECMODE=\"",valBuff9);
			if(strstr(valBuff9,"WPA") != 0)
			{
				//TODO
				Execute_cmd("cfg -t0 /etc/ath/PSK.ap_bss ath0 > /tmp/secath0",rspBuff);
				Execute_cmd("killall hostapd > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath0 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
			}
			#endif
		}
		//channel
		CFG_get_by_name("AP_PRIMARY_CH",valBuff4);
		//fprintf(errOut,"[luodp] %s here5 %s\n",valBuff4,valBuff7);
		sprintf(valBuff5,"%s\n<br>",valBuff4);
		//fprintf(errOut,"%d\n%d\n%d\n",strcmp(valBuff2,valBuff5),flag,strcmp(valBuff3,"on"));
		//if((strcmp(valBuff7,valBuff5) != 0)&&(flag==0)&& (strcmp(valBuff3,"on") == 0))
		if((strcmp(valBuff7,valBuff5) != 0) && (strcmp(valBuff3,"on") == 0))
		{
			//fprintf(errOut,"[luodp] %d",atoi(valBuff4));
			//11NGHT40PLUS/11NGHT40MINUS
			Execute_cmd("ifconfig ath0 down > /dev/null 2>&1", rspBuff);
			Execute_cmd("ifconfig ath1 down > /dev/null 2>&1", rspBuff);

			#if 0
			if(strstr(valBuff10,"11NGHT40PLUS")!=0)
			{
				if(atoi(valBuff4)>=10)
				{
					Execute_cmd("iwpriv ath0 mode 11NGHT40MINUS", rspBuff);
					Execute_cmd("iwpriv ath1 mode 11NGHT40MINUS", rspBuff);
					CFG_set_by_name("AP_CHMODE","11NGHT40MINUS");
				}
			}else if(strstr(valBuff10,"11NGHT40MINUS")!=0)
			{
				if(atoi(valBuff4)<=4)
				{
					Execute_cmd("iwpriv ath0 mode 11NGHT40PLUS", rspBuff);
					Execute_cmd("iwpriv ath1 mode 11NGHT40PLUS", rspBuff);
					CFG_set_by_name("AP_CHMODE","11NGHT40PLUS");
				}
			}
			#endif
			
			if(atoi(valBuff4)>=10)
			{
				if(strstr(valBuff10, "11NGHT40PLUS"))
				{
					Execute_cmd("iwpriv ath0 mode 11NGHT40MINUS", rspBuff);
					Execute_cmd("iwpriv ath1 mode 11NGHT40MINUS", rspBuff);
					CFG_set_by_name("AP_CHMODE","11NGHT40MINUS");
				}
			}
			else
			{
				if(strstr(valBuff10, "11NGHT40MINUS"))
				{
					Execute_cmd("iwpriv ath0 mode 11NGHT40PLUS", rspBuff);
					Execute_cmd("iwpriv ath1 mode 11NGHT40PLUS", rspBuff);
					CFG_set_by_name("AP_CHMODE","11NGHT40PLUS");
				}
			}
            
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);	
			//fprintf(errOut,"[luodp] SECMODE here7\n");
			sprintf(pChar,"iwconfig ath0 channel %s  > /dev/null 2>&1",valBuff4);
			Execute_cmd(pChar, rspBuff);
			sprintf(pChar,"iwconfig ath1 channel %s  > /dev/null 2>&1",valBuff4);
			Execute_cmd(pChar, rspBuff);
			Execute_cmd("ifconfig ath0 up > /dev/null 2>&1", rspBuff);
			Execute_cmd("ifconfig ath1 up > /dev/null 2>&1", rspBuff);

			Execute_cmd("ifconfig ath0 down > /dev/null 2>&1", rspBuff);
			Execute_cmd("ifconfig ath0 up > /dev/null 2>&1", rspBuff);
			flag = 5;
		}
		//hide ssid
		CFG_get_by_name("AP_HIDESSID",valBuff4);
		sprintf(valBuff5,"%s\n<br>",valBuff4);
		if((strcmp(valBuff8,valBuff5) != 0) && (strcmp(valBuff4,"1") == 0))
		{
			//fprintf(errOut,"set hide 1 \n");
			Execute_cmd("iwpriv ath0 hide_ssid 1  > /dev/null 2>&1", rspBuff);
			
			Execute_cmd("iwpriv ath0 bintval 1000  > /dev/null 2>&1", rspBuff);
			CFG_set_by_name("BEACON_INT", "1000");
			//save new config to flash 
           
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
			
			flag = 5;
		}
		if((strcmp(valBuff8,valBuff5) != 0) && (strcmp(valBuff4,"0") == 0))
		{
			//fprintf(errOut,"set hide 0 \n");
			Execute_cmd("iwpriv ath0 hide_ssid 0  > /dev/null 2>&1", rspBuff);
			
			Execute_cmd("iwpriv ath0 bintval 100  > /dev/null 2>&1", rspBuff);
			CFG_set_by_name("BEACON_INT", "100");
			//save new config to flash 
            
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
			
			flag = 5;
		}

		//fprintf(errOut,"2.4G flag is %d\n", flag);
		if(flag==2)
		{
			//fprintf(errOut,"[luodp] SECMODE here5");
			//CFG_get_by_name("AP_RADIO_ID",valBuff);
			//CFG_get_by_name("AP_SECMODE",valBuff2);
			//CFG_get_by_name("AP_SECFILE",valBuff3);
			//CFG_get_by_name("WPS_ENABLE",valBuff4);
			//CFG_get_by_name("WPS_VAP_TIE",valBuff5);
			//sprintf(pChar,"activateVAP ath0:%s br0 %s %s %s %s",valBuff,valBuff2,valBuff3,valBuff4,valBuff5);
			//Execute_cmd(pChar, rspBuff);
			Execute_cmd("apdown > /dev/null 2>&1", rspBuff);
			Execute_cmd("apup > /dev/null 2>&1", rspBuff);
		}/*else if(flag==2)
		{
			//fprintf(errOut,"[luodp] SECMODE here7");
			//CFG_get_by_name("AP_RADIO_ID",valBuff);
			//CFG_get_by_name("AP_SECMODE",valBuff2);
			//sprintf(pChar,"activateVAP ath0:%s br0 %s",valBuff,valBuff2);
			//Execute_cmd(pChar, rspBuff);
			Execute_cmd("apdown > /dev/null 2>&1", rspBuff);
			Execute_cmd("apup > /dev/null 2>&1", rspBuff);
		}*/
		else if(flag == 5)  /*deal password is changed, restart hostapd   by zzw*/
		{
			int i, j, k;
			char pr_buf[50];
			CFG_get_by_name("AP_SECMODE",valBuff3);
			if(!strcmp(valBuff3,"None")) /*wifi doesn't use WPA*/
				Execute_cmd("cfg -t0 /etc/ath/PSK.ap_bss_none ath0 > /tmp/secath0",rspBuff);
			else
				Execute_cmd("cfg -t0 /etc/ath/PSK.ap_bss ath0 > /tmp/secath0",rspBuff);
			//Execute_cmd("killall hostapd > /dev/null 2>&1",rspBuff);
			/*get the hostapd for ath0's pid, to kill it, then restart it*/
			Execute_cmd("ps | grep 'hostapd -B /tmp/secath0 -e /etc/wpa2/entropy' | awk -F ' ' '{print $1}'", valBuff);
			//fprintf(errOut,"2.4G the kill process is [%s] %d\n",valBuff, strlen(valBuff));
			if(strstr(valBuff, "<br>"))
			{
				while(valBuff[i])
				{
					if(valBuff[i] == '\n')
					{
						k = 1;
						break;
					}
					//fprintf(errOut," is [%c] \n", valBuff[i]);
					i++;
				}
				if(k == 1)
				{
					memcpy(pr_buf, valBuff, i);
					//fprintf(errOut," is [%s] \n", pr_buf);
					sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
					Execute_cmd(pChar, rspBuff);

					j = i+5;
					while(valBuff[i+5])
					{
						if(valBuff[i+5] == '\n')
						{
							k = 2;
							break;
						}
						//fprintf(errOut," is [%c] \n", valBuff[i+5]);
						i++;
					}
					if(k == 2)
					{
						memcpy(pr_buf, &valBuff[j], i+5-j);
						//fprintf(errOut," is [%s] \n", pr_buf);
						sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
						Execute_cmd(pChar, rspBuff);
					}
				}
			}
			Execute_cmd("hostapd -B /tmp/secath0 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);

			Execute_cmd("ifconfig ath0 down > /dev/null 2>&1", rspBuff);
			CFG_get_by_name("WIFION_OFF",valBuff3);
			if(strcmp(valBuff3,"off")) 
			{
				Execute_cmd("ifconfig ath0 up > /dev/null 2>&1", rspBuff);
			}
		}
#if 1
		/*deal with wifi 5G setting*/
		flag = 0;
		//do new config pid
		//TODO key check
		CFG_get_by_name("PSK_KEY_3",valBuff3);
		//fprintf(errOut,"web PSK_KEY_3 is [%s] \n",valBuff3);
		//fprintf(errOut,"flash PSK_KEY_3 is [%s] \n",valBuff4_3);
		sprintf(valBuff6,"%s\n<br>",valBuff3);
		//fprintf(errOut,"valBuff6 is [%s]\n", valBuff6);
		//fprintf(errOut,"valBuff4_3 is [%s]\n", valBuff4_3);
		if((strcmp(valBuff6,valBuff4_3) != 0)&&(strcmp(valBuff6,"\n<br>") != 0))
		{
			CFG_set_by_name("PSK_KEY_3",valBuff3);
			flag = 5;
			//fprintf(errOut,"1111 5G flag is %d\n", flag);
		}
		
		//TODO SECMODE
		CFG_get_by_name("AP_SECMODE_3",valBuff3);;
		if((strstr(valBuff5_3,valBuff3) == 0)&&(strcmp(valBuff3,"None") != 0))
		{
			CFG_set_by_name("AP_SECMODE_3","WPA");
			CFG_set_by_name("AP_WPA_3","3");
//			CFG_set_by_name("AP_CYPHER_3","TKIP CCMP");
			CFG_set_by_name("AP_CYPHER_3","CCMP");
			CFG_set_by_name("AP_SECFILE_3","PSK"); 
			flag = 5;
			//fprintf(errOut,"22222 5G flag is %d\n", flag);
		}
		if((strstr(valBuff5_3,valBuff3) == 0)&&(strcmp(valBuff3,"WPA") != 0))
		{
			CFG_set_by_name("AP_SECMODE_3","None");
			flag=5;
			//Execute_cmd("iwpriv ath2 authmode 1 > /dev/null 2>&1", rspBuff);
			//fprintf(errOut,"33333 5G flag is %d\n", flag);
		}
		
		//save new add  to flash 
           
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
		
		CFG_get_by_name("WIFION_OFF_3",valBuff3);
		//fprintf(errOut,"web WIFION_OFF_3 is [%s] \n",valBuff3);
		//fprintf(errOut,"flash WIFION_OFF_3 is [%s] \n",valBuff_3);
		if((strstr(valBuff_3,valBuff3) == 0) && (strcmp(valBuff3,"on") == 0) )
		{
			Execute_cmd("ifconfig ath2 up > /dev/null 2>&1", rspBuff);
			Execute_cmd("echo enable > /dev/wifiled", rspBuff);
		}
		if(strcmp(valBuff3,"off") == 0)
		{
			//Execute_cmd("apdown > /dev/null 2>&1", rspBuff);
			Execute_cmd("ifconfig ath2 down > /dev/null 2>&1", rspBuff);
			//flag = 5;
            led_flag++;
		}
        if(led_flag==2)
			Execute_cmd("echo disable > /dev/wifiled", rspBuff);
            
		
		//ssid [TODO]
		CFG_get_by_name("AP_SSID_3",valBuff4);
		//fprintf(errOut,"web AP_SSID_3 is [%s] \n",valBuff4);
		//fprintf(errOut,"flash AP_SSID_3 is [%s] \n",valBuff2_3);
        if(valBuff2_3[0]=='"')
            sprintf(valBuff5,"\"%s\"\n<br>",valBuff4);
        else
            sprintf(valBuff5,"%s\n<br>",valBuff4);
		//if((strcmp(valBuff2_3,valBuff5) != 0)&&(flag==0)&&(strcmp(valBuff3,"on") == 0))
		if(strcmp(valBuff2_3,valBuff5) &&(strcmp(valBuff3,"on") == 0))
		{
			sprintf(pChar,"iwconfig ath2 essid %s  > /dev/null 2>&1",valBuff4);
			Execute_cmd(pChar, rspBuff);
			flag = 5;
			/*Execute_cmd("cfg -e | grep \"AP_SECMODE_3=\"",valBuff9);
			if(strstr(valBuff9,"WPA") != 0)
			{
				//TODO
				fprintf(errOut,"restart hostapd \n");
				Execute_cmd("cfg -t0 /etc/ath/PSK.ap_bss ath2 > /tmp/secath2",rspBuff);
				Execute_cmd("killall hostapd > /dev/null 2>&1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath2 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
			}*/
		}
		
		//channel
		CFG_get_by_name("AP_PRIMARY_CH_3",valBuff4);
		//fprintf(errOut," web valBuff4 is [%s] \n",valBuff4);
		//fprintf(errOut," flash valBuff7_3 is [%s] \n",valBuff7_3);
		sprintf(valBuff5,"%s\n<br>",valBuff4);
		//if((strcmp(valBuff7_3,valBuff5) != 0)&&(flag==0)&& (strcmp(valBuff3,"on") == 0))
		if((strcmp(valBuff7_3,valBuff5) != 0) && (strcmp(valBuff3,"on") == 0))
		{
			//fprintf(errOut,"[luodp] 11NA %d",atoi(valBuff4));
			#if 0
			memcpy(valBuff4, "40", 2);
			Execute_cmd("iwpriv ath2 mode 11NAHT40MINUS", rspBuff);
			CFG_set_by_name("AP_CHMODE_3","11NAHT40MINUS");
			writeParameters(NVRAM,"w+", NVRAM_OFFSET);
			writeParameters("/tmp/.apcfg","w+",0);
			sprintf(pChar,"iwconfig ath2 channel %s  > /dev/null 2>&1",valBuff4);
			Execute_cmd(pChar, rspBuff);
			#endif
			//11NAHT40PLUS/11NAHT40MINUS
			Execute_cmd("ifconfig ath2 down > /dev/null 2>&1", rspBuff);
			Execute_cmd("ifconfig ath3 down > /dev/null 2>&1", rspBuff);
			if((atoi(valBuff4) == 165))
			{
				Execute_cmd("iwpriv ath2 mode 11A > /dev/null 2>&1", rspBuff);
				Execute_cmd("iwpriv ath3 mode 11A > /dev/null 2>&1", rspBuff);
				CFG_set_by_name("AP_CHMODE_3","11A");
			}
			else if( (atoi(valBuff4) == 149) || (atoi(valBuff4) == 157) )
			{
				Execute_cmd("iwpriv ath2 mode 11NAHT40PLUS > /dev/null 2>&1", rspBuff);
				Execute_cmd("iwpriv ath3 mode 11NAHT40PLUS > /dev/null 2>&1", rspBuff);
				CFG_set_by_name("AP_CHMODE_3","11NAHT40PLUS");
			}
			else if((atoi(valBuff4) == 161) || (atoi(valBuff4) == 153))
			{
				Execute_cmd("iwpriv ath2 mode 11NAHT40MINUS > /dev/null 2>&1", rspBuff);
				Execute_cmd("iwpriv ath3 mode 11NAHT40MINUS > /dev/null 2>&1", rspBuff);
				CFG_set_by_name("AP_CHMODE_3","11NAHT40MINUS");
			}
			#if 0
			if(strstr(valBuff10_3,"11NAHT40PLUS")!=0)
			{
				if(atoi(valBuff4)>=165)
				{
					Execute_cmd("iwpriv ath2 mode 11NAHT40MINUS", rspBuff);
					Execute_cmd("iwpriv ath3 mode 11NAHT40MINUS", rspBuff);
					CFG_set_by_name("AP_CHMODE_3","11NAHT40MINUS");
				}
			}else if(strstr(valBuff10_3,"11NAHT40MINUS")!=0)
			{
				if(atoi(valBuff4)<=149)
				{
					Execute_cmd("iwpriv ath2 mode 11NAHT40PLUS", rspBuff);
					Execute_cmd("iwpriv ath3 mode 11NAHT40PLUS", rspBuff);
					CFG_set_by_name("AP_CHMODE_3","11NAHT40PLUS");
				}
			}
			#endif
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
			sprintf(pChar,"iwconfig ath2 channel %s  > /dev/null 2>&1",valBuff4);
			Execute_cmd(pChar, rspBuff);
			sprintf(pChar,"iwconfig ath3 channel %s  > /dev/null 2>&1",valBuff4);
			Execute_cmd(pChar, rspBuff);
			Execute_cmd("ifconfig ath2 up > /dev/null 2>&1", rspBuff);
			Execute_cmd("ifconfig ath3 up > /dev/null 2>&1", rspBuff);
			flag = 5;
		}
		
		//hide ssid
		CFG_get_by_name("AP_HIDESSID_3",valBuff4);
		//fprintf(errOut,"web valBuff4 is [%s] \n",valBuff4);
		//fprintf(errOut,"flash valBuff8_3 is [%s] \n",valBuff8_3);
		sprintf(valBuff5,"%s\n<br>",valBuff4);
		//if((strcmp(valBuff8_3,valBuff5) != 0)&&(flag==0)&& (strcmp(valBuff3,"on") == 0)&&(strcmp(valBuff4,"1") == 0))
		if((strcmp(valBuff8_3,valBuff5) != 0) &&(strcmp(valBuff4,"1") == 0))
		{
			Execute_cmd("iwpriv ath2 hide_ssid 1  > /dev/null 2>&1", rspBuff);

			Execute_cmd("iwpriv ath2 bintval 1000  > /dev/null 2>&1", rspBuff);
			CFG_set_by_name("BEACON_INT_3", "1000");
			//save new config to flash 
          
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
			
			flag = 5;
		}
		if((strcmp(valBuff8_3,valBuff5) != 0)&&(strcmp(valBuff4,"0") == 0))
		{
			Execute_cmd("iwpriv ath2 hide_ssid 0  > /dev/null 2>&1", rspBuff);
			
			Execute_cmd("iwpriv ath2 bintval 100  > /dev/null 2>&1", rspBuff);
			CFG_set_by_name("BEACON_INT_3", "100");
			//save new config to flash 
            
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
			
			flag = 5;
		}

		//fprintf(errOut,"5G flag is %d\n", flag);
		if(flag==2)
		{
			Execute_cmd("apdown > /dev/null 2>&1", rspBuff);
			Execute_cmd("apup > /dev/null 2>&1", rspBuff);
		}
		else if(flag == 5)  /*deal password is changed, restart hostapd   by zzw*/
		{fprintf(errOut,"5G zhaozhanwei2222222  \n");
			int i, j, k;
			char pr_buf[50];
			CFG_get_by_name("AP_SECMODE_3",valBuff3);
			if(!strcmp(valBuff3,"None")) /*wifi doesn't use WPA*/
				Execute_cmd("cfg -t3 /etc/ath/PSK.ap_bss_none ath2 > /tmp/secath2",rspBuff);
			else
				Execute_cmd("cfg -t3 /etc/ath/PSK.ap_bss ath2 > /tmp/secath2",rspBuff);
			
			//Execute_cmd("killall hostapd > /dev/null 2>&1",rspBuff);
			/*get the hostapd for ath2's pid, to kill it, then restart it*/
			Execute_cmd("ps | grep 'hostapd -B /tmp/secath2 -e /etc/wpa2/entropy' | awk -F ' ' '{print $1}'", valBuff);
			//fprintf(errOut,"5G the kill process is contain [%s] %d\n",valBuff, strlen(valBuff));
			if(strstr(valBuff, "<br>"))
			{
				while(valBuff[i])
				{
					if(valBuff[i] == '\n')
					{
						k = 1;
						break;
					}
					//fprintf(errOut," is [%c] \n", valBuff[i]);
					i++;
				}
				if(k == 1)
				{
					memcpy(pr_buf, valBuff, i);
					//fprintf(errOut," is [%s] \n", pr_buf);
					sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
					Execute_cmd(pChar, rspBuff);
					fprintf(errOut,"kill 1 %s\n", pr_buf);

					j = i+5;
					while(valBuff[i+5])
					{
						if(valBuff[i+5] == '\n')
						{
							k = 2;
							break;
						}
						//fprintf(errOut," is [%c] \n", valBuff[i+5]);
						i++;
					}
					if(k == 2)
					{
						memcpy(pr_buf, &valBuff[j], i+5-j);
						//fprintf(errOut," is [%s] \n", pr_buf);
						sprintf(pChar, "kill %s > /dev/null 2>&1", pr_buf);
						Execute_cmd(pChar, rspBuff);
						fprintf(errOut,"kill 2 %s\n", pr_buf);
					}
				}
			}
			Execute_cmd("hostapd -B /tmp/secath2 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);

			Execute_cmd("ifconfig ath2 down > /dev/null 2>&1", rspBuff);
			CFG_get_by_name("WIFION_OFF_3",valBuff3);
			if(strcmp(valBuff3,"off"))
			{
				Execute_cmd("ifconfig ath2 up > /dev/null 2>&1", rspBuff);
			}
		}
#endif
	
		gohome =1;
    }

///pppyhl
	//wan mode pppoe
	//pppoe and advanced options below ,added by yhl 
	if((strcmp(CFG_get_by_name("PPP",valBuff),"PPP") == 0 ) || (strcmp(CFG_get_by_name("PPPW",valBuff),"PPPW") == 0 ))
	{	
			char pppoe_mode[10];						
			char three_thread_buf[128]; 					
			char tmp_Buff[10];	
			char cmdstr[128];
			char route_gw[20];
			
			memset(pppoe_mode,'\0',10);
			memset(three_thread_buf,'\0',128);
			memset(tmp_Buff,'\0',10);
			memset(cmdstr,'\0',128);
			memset(route_gw,'\0',20);
			int flag=0; 

			system("ifconfig eth0 0.0.0.0 up"); //wangyu add for the wan mode change from static to pppoe			
			
			Execute_cmd("cfg -e | grep 'WAN_MODE='",valBuff);			   
			if(strstr(valBuff,"dhcp") != 0)
			{											
				Execute_cmd("killall udhcpc > /dev/null 2>&1", rspBuff);			   
			}
			if (strcmp(CFG_get_by_name("PPPW",valBuff),"PPPW") == 0 ) 
			{ 
						flag=1; 
			}			 
			CFG_get_by_name("PPPOE_MODE",pppoe_mode);
			fprintf(errOut,"user select pppoe_mode:%s\n",pppoe_mode);
			if(flag!=1)
			{
           
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);//save new config to flash 
            //writeParameters("/tmp/.apcfg","w+",0);
			}
			if(!strncmp(pppoe_mode,"auto",4))
			{  
				char	usernameBuff[128];
				char	passBuff[128];
							  
				CFG_get_by_name("PPPOE_USER",usernameBuff);
				CFG_get_by_name("PPPOE_PWD",passBuff);
	
				sprintf(cmdstr,"pppoe-setup %s %s > /dev/null 2>&1",usernameBuff,passBuff);
				//fprintf(errOut,"auto pppoe-setup cmdstr-----%s\n",cmdstr);
				system(cmdstr);
	
				system("pppoe-stop > /dev/null 2>&1");sleep(2);
				system("pppoe-start > /dev/null 2>&1");sleep(5);
			}
			else if(!strncmp(pppoe_mode,"demand",6))
			{  
				char	usernameBuff[128];
				char	passBuff[128];
							  
				CFG_get_by_name("PPPOE_USER",usernameBuff);
				CFG_get_by_name("PPPOE_PWD",passBuff);
				
				sprintf(cmdstr,"pppoe-setup %s %s > /dev/null 2>&1",usernameBuff,passBuff);
				//fprintf(errOut,"demand pppoe-setup cmdstr-----%s\n",cmdstr);
				system(cmdstr);
	
				//system("pppoe-stop > /dev/null 2>&1");sleep(2);
				//system("pppoe-start > /dev/null 2>&1");sleep(5);
			}//demand mode
			else if(!strncmp(pppoe_mode,"manual",6))
			{
				char	usernameBuff[128];
				char	passBuff[128];
							  
				CFG_get_by_name("PPPOE_USER",usernameBuff);
				CFG_get_by_name("PPPOE_PWD",passBuff);
				
				sprintf(cmdstr,"pppoe-setup %s %s > /dev/null 2>&1",usernameBuff,passBuff);
				//fprintf(errOut,"manual pppoe-setup cmdstr-----%s\n",cmdstr);
				system(cmdstr);   
				
				//system("pppoe-stop > /dev/null 2>&1");sleep(2);
				//system("pppoe-start > /dev/null 2>&1");sleep(5);
			}//manual mode		
			else if(!strncmp(pppoe_mode,"timing",6))	
			{ 
				char	usernameBuff[128];
				char	passBuff[128];
							  
				CFG_get_by_name("PPPOE_USER",usernameBuff);
				CFG_get_by_name("PPPOE_PWD",passBuff);
				
				sprintf(cmdstr,"pppoe-setup %s %s > /dev/null 2>&1",usernameBuff,passBuff);
				//fprintf(errOut,"timing pppoe-setup cmdstr-----%s\n",cmdstr);
				system(cmdstr);
	
				//system("pppoe-stop > /dev/null 2>&1");sleep(2);
				//system("pppoe-start > /dev/null 2>&1");sleep(5);
			}
	
			//for pppoe show yhlnew
			char pppoe_ip[20];
			char pppoe_gw[20];
			char pppoe_mask[20];
			
			Execute_cmd("ifconfig | grep P-t-P | awk -F ' ' '{print$2}'| awk -F ':' '{print$2}'", pppoe_ip);
			Execute_cmd("ifconfig | grep P-t-P | awk -F ' ' '{print$3}'| awk -F ':' '{print$2}'", pppoe_gw);
			Execute_cmd("ifconfig | grep P-t-P | awk -F ' ' '{print$4}'| awk -F ':' '{print$2}'", pppoe_mask);
	
			CFG_set_by_name("WAN_IPADDR3",pppoe_ip);
			CFG_set_by_name("WAN_NETMASK3",pppoe_mask);
			CFG_set_by_name("IPGW3",pppoe_gw);
	
			//add ppp0 route default gw if losing
			if(!strstr(Execute_cmd("echo `route -n|grep ppp0|awk -F ' ' '{print$8}'`",route_gw),"ppp0 ppp0"))
				 {
				  Execute_cmd("route add default gw `route -n | grep ppp0 |awk -F ' ' '{print$1}'|awk 'NR==1'` > /dev/null 2>&1",route_gw);
				  fprintf(errOut,"\nPPP0 LOST,ADD ONE\n");
				 }
			//save new config to flash 
			if(flag!=1)
			{
         
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);//save new config to flash 
            //writeParameters("/tmp/.apcfg","w+",0);
			}
			gohome =1;
			//kill old,run new ppy
		    ////////system("killall ppy > /dev/null 2>&1");sleep(1);
			//system("/usr/sbin/ppy & > /dev/null 2>&1");sleep(1);
			////////system("/usr/sbin/ppy & > /dev/null 2>&1");
			system("killall pppoe-ppy > /dev/null 2>&1");sleep(3);//ppy is in pppoe-ppy script
			system("killall ppy > /dev/null 2>&1");sleep(3);
			system("ppy > /dev/null 2>&1 &");sleep(2);//bash  avoid let syntax error
			//system("sleep 1 && reboot &");

		}

///pppoe above
	//login settings
     if(strcmp(CFG_get_by_name("ADMINSET",valBuff),"ADMINSET") == 0 )
    {
         fprintf(errOut,"\n%s  %d ADMINSET \n",__func__,__LINE__);
            
         char cmdd[128]={0};
         int qq=0;
         FILE *fp;
         if((fp=fopen("/etc/httpd.conf","w+"))==NULL)
         {
            fprintf(errOut,"\n----------cannot open file  line:%d\n",__LINE__);
            exit(1);
         }
         memset(cmdd,0x00,128);
         sprintf(cmdd,"/index.html:admin:%s\n",CFG_get_by_name("ADMPASS" ,valBuff));
         fwrite(cmdd,strlen(cmdd),1,fp);
	 memset(cmdd,0x00,128);
  	 sprintf(cmdd,"/index2.html:admin:%s",valBuff);
	 fwrite(cmdd,strlen(cmdd),1,fp);

         fclose(fp);
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
     
		 printf("HTTP/1.0 200 OK\r\n");
         printf("Content-type: text/html\r\n");
         printf("Connection: Close\r\n");
         printf("\r\n");
         printf("\r\n");

         printf("<HTML><HEAD>\r\n");
         printf("<meta http-equiv=\"Content-Type\" content=\"text/html;charset=UTF-8\">");
         printf("</head><body>");
         srand(time(NULL));
         //printf("<script  language=javascript>setTimeout(function(){window.opener=null;window.open('/home.html','_top');},1000);</script>");
         printf("<script  language=javascript>setTimeout(function(){window.opener=null;window.open('/index.html?hhh=%d','_top');},1000);</script>",rand());
         //printf("<script  language=javascript>setTimeout(function(){window.opener=null;window.open('http://10.10.10.254','_top');},1000);</script>");
         printf("</body></html>");
         gohome =2;
    }
	if(strcmp(CFG_get_by_name("FACTORY",valBuff),"FACTORY") == 0 )
	{
        fprintf(errOut,"\n%s  %d FACTORY \n",__func__,__LINE__);
		
		//char valBuff[128];
		
		//CFG_get_by_name("SOFT_VERSION",valBuff);		

        
        system("cfg -x");

        
		sleep(3);
		//backup version
		//CFG_set_by_name("SOFT_VERSION",valBuff);
	    //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
        //writeParameters("/tmp/.apcfg","w+",0);	
		//[TODO] reset any args
		//system("echo \"/:admin:admin\" > /etc/httpd.conf");
		//MAC and BACKUP
		//system("/usr/sbin/var_backup > /dev/null 2>&1");
#if 0		
        printf("HTTP/1.0 200 OK\r\n");
        printf("Content-type: text/html\r\n");
        printf("Connection: close\r\n");
        printf("\r\n");
        printf("\r\n");

        printf("<HTML><HEAD>\r\n");
        printf("<LINK REL=\"stylesheet\" HREF=\"../style/handaer.css\"  TYPE=\"text/css\" media=\"all\">");
        printf("<link rel=\"stylesheet\" href=\"../style/normal_ws.css\" type=\"text/css\">");
		printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
        printf("</head><body>");
        
        printf("<div class=\"handaer_main\">");
        printf("<h1>鎭㈠鍑哄巶璁剧疆</h1>\n");	
        printf("<div align=\"center\"> <img align=\"center\" src=\"../images/loading.gif\"></img></div>");
        printf("<p  align=\"center\">鎭㈠鍑哄巶璁剧疆瀹屾垚,姝ｅ湪閲嶅惎BASE..........</p><br>\n");	
        printf("<p  align=\"center\">Restore factory settings was completed, restartting BASE..........</p><br>\n");	
        printf("<script  language=javascript>setTimeout(function(){window.location.href=\"crfact\";},60000);</script>");
        printf("</div>");
		printf("</body></html>");
	#endif
        Reboot_tiaozhuan("factory","index.html");
        system("sleep 1 && reboot &");
         gohome =2;
    }
    /*************************************
    外网设置     恢复初始mac克隆
   *************************************/
    if(strcmp(CFG_get_by_name("MAC_CLONE",valBuff),"MAC_CLONE") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d MAC_CLONE \n",__func__,__LINE__);
		char mymac[128];
		char str[128];
		char mymac2[128];	
		char tmp[128];
		char tmp2[128];
		char ipaddr[128],*ipaddr2;		
		char linenum[128];
		char valBuff_eth0[128];
		
//		Execute_cmd("cat /etc/mac.bin",mymac);
//		mymac2=strtok(mymac,"\n");
		Execute_cmd("/usr/sbin/get_mac eth0",mymac);
		strncpy(mymac2,mymac,17);
//		fprintf(errOut,"\n.........%s.............\n",mymac);

		CFG_get_by_name("MACTYPE",tmp);
		//0 -factory mac ,1-pc mac,2 -user input
		
		//wangyu add for route list backup
		Execute_cmd("ifconfig eth0", valBuff_eth0);
		if(strstr(valBuff_eth0,"inet addr:"))
		{
		Execute_cmd("route | grep default | awk -F' ' '{print $2}'",ipaddr);
		ipaddr2=strtok(ipaddr,"\n");
 //       fprintf(errOut,"\n.........%s.............\n",ipaddr);
		}
		
		if(atoi(tmp)==0)
		{
			//fprintf(errOut,"[luodp] %s",mymac2);
			CFG_set_by_name("ETH0_MAC",mymac2);
			sprintf(str,"ifconfig eth0 hw ether %s > /dev/null 2>&1",mymac2);
			system("ifconfig eth0 down");
			system(str);
			system("ifconfig eth0 up");
		}else if((atoi(tmp)==1)||(atoi(tmp)==2))
		{
			CFG_get_by_name("ETH0_MAC",tmp2);
			sprintf(str,"ifconfig eth0 hw ether %s > /dev/null 2>&1",tmp2);
			system("ifconfig eth0 down");
			system(str);
			system("ifconfig eth0 up");
		}

		//wangyu add for route list backup
		if(strstr(valBuff_eth0,"inet addr:"))
		{
		if(strncmp(Execute_cmd("route | grep default | awk -F' ' '{print $8}'",linenum),"eth0",4))
			sprintf(str,"route add default gw %s",ipaddr2);
		
   //     fprintf(errOut,"\n.........%s .............\n",str);
		system(str);
		}
		
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
        Result_tiaozhuan("yes",argv[0]);   
        gohome =2;
    }
    /*************************************
    内网设置    DHCP服务器
   *************************************/
    if(strcmp(CFG_get_by_name("DHCP_SET",valBuff),"DHCP_SET") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d DHCP_SET \n",__func__,__LINE__);
 		set_dhcp();   
        Result_tiaozhuan("yes",argv[0]);   
		gohome =2;
    }
    /*************************************
    内网设置    DHCP服务器    添加
   *************************************/
    if(strcmp(CFG_get_by_name("ADD_STATICDHCP",valBuff),"ADD_STATICDHCP") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d ADD_STATICDHCP \n",__func__,__LINE__);
		if(set_addr_bind()==1)
        {
            //Result_tiaozhuan("no",argv[0]); 			
            gohome =2;
        }
        else
        {
			memset(Page,0,64);
            //sprintf(Page,"%s","../ad_local_dhcp.html");
            //gohome =0;
            Normal_tiaozhuan("ad_local_dhcp");
            gohome =2;
        }
    }
   /*************************************
    内网设置    DHCP服务器    删除
   *************************************/
    if(strcmp(CFG_get_by_name("DEL_SDHCP",valBuff),"DEL_SDHCP") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d DEL_SDHCP \n",__func__,__LINE__);
		del_addr_bind();
        //sprintf(Page,"%s","../ad_local_dhcp.html");
        //gohome =0;
        Normal_tiaozhuan("ad_local_dhcp");
        gohome =2;

    }
	   /*************************************
    内网设置    DHCP服务器    修改
   *************************************/
    if(strcmp(CFG_get_by_name("MOD_SDHCP",valBuff),"MOD_SDHCP") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d MOD_SDHCP \n",__func__,__LINE__);
		modify_addr_bind();
        //sprintf(Page,"%s","../ad_local_dhcp.html");
        //gohome =0;
            Normal_tiaozhuan("ad_local_dhcp");
            gohome =2;
    }


    /*************************************
    内网设置    网关设置
   *************************************/
    if(strcmp(CFG_get_by_name("GW_SET",valBuff),"GW_SET") == 0 ) 
    {
		unsigned char br0_ip[20],br0_sub[20], eth0_ip[20];
		unsigned char*peth0_ip;
		unsigned char* ptmp;
		unsigned char dhcp_b[20],dhcp_e[20],net_seg[20],net_seg2[20],tmp[20],tmp2[20];
		unsigned char pChar[128];
		char valBuff[50];
		char valBuff2[50];
		char valBuff3[1000];
		
		int num,i=0;

		CFG_get_by_name("AP_IPADDR",br0_ip);
		CFG_get_by_name("AP_NETMASK",br0_sub);
		Execute_cmd("cfg -e | grep AP_IPADDR= | awk -F '=' '{print $2}'",valBuff);
		Execute_cmd("cfg -e | grep \"AP_NETMASK=\" | awk -F \"=\" \'{print $2}\'",valBuff2);

		
		Execute_cmd("ifconfig eth0", valBuff3);
		if(strstr(valBuff3,"inet addr:"))
		{
			Execute_cmd("ifconfig eth0|grep 'inet addr:'|awk -F ' ' '{print$2}'|awk -F ':' '{print$2}'", eth0_ip);
			peth0_ip=strtok(eth0_ip,"\n");
		}
		
		if((!strstr(valBuff3,"inet addr:"))||(strstr(valBuff3,"inet addr:")&&(strcmp(peth0_ip, br0_ip) )))
		{
			if(!strstr(valBuff, br0_ip) || !strstr(valBuff2, br0_sub))
			{
				sprintf(pChar,"ifconfig br0 %s netmask %s up > /dev/null 2>&1",br0_ip,br0_sub);
				system(pChar);

				//update nat_vlan.sh
				Execute_cmd("cat /etc/nat_vlan.sh | grep 'ppp0'|grep 'DNAT --to'|awk -F ' ' '{print$15}'", tmp);
				ptmp=strtok(tmp,"\n");
				sprintf(pChar,"sed -i 's/%s/%s/g' /etc/nat_vlan.sh",ptmp,br0_ip);
				system(pChar);

				memset(net_seg2,'\0',20);
				memset(net_seg,'\0',20);

				get_net_seg_and(br0_ip,br0_sub,net_seg);	

				strcpy(net_seg2,net_seg);//backup net_seg
				fprintf(errOut,"net seg:%s\n",net_seg);

				memset(tmp,'\0',20);
				memset(tmp2,'\0',20);	

				strcpy(tmp,"0.0.0.255");
				get_net_seg_and(net_seg,tmp,tmp2);	   
				fprintf(errOut,"\nAnd 0.0.0.255 is:%s to check\n",tmp2);

				if(!strcmp(tmp2,"0.0.0.0"))
				{  
				  memset(tmp,'\0',20);
				  memset(tmp2,'\0',20);
				  strcpy(tmp,"255.255.255.0");
				  
				  get_net_seg_and(net_seg,tmp,tmp2);
				  fprintf(errOut,"\nAnd 255.255.255.0---%s\n",tmp2);
				  memset(tmp,'\0',20);
				  strcpy(tmp,"0.0.0.101");
				  
				  memset(dhcp_b,'\0',20);
				  get_net_seg_or(tmp2,tmp,dhcp_b);

				  strcpy(tmp,"255.255.255.0");
				  memset(tmp2,'\0',20);
				  memset(dhcp_e,'\0',20);
				  get_net_seg_and(net_seg,tmp,tmp2);
				  strcpy(tmp,"0.0.0.199");
				  get_net_seg_or(tmp2,tmp,dhcp_e);
				}
				else
					{	
				   get_max_dhcp_end_ip(net_seg2,dhcp_e);
				   strcpy(tmp,"0.0.0.1");
				   get_net_seg_or(net_seg2,tmp,dhcp_b);
				}
				fprintf(errOut,"\ndhcp_b:%s\ndhcp_e:%s\n",dhcp_b,dhcp_e);

				CFG_set_by_name("DHCP_BIP",dhcp_b);
				CFG_set_by_name("DHCP_EIP",dhcp_e);
				//write to flash
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);

				CFG_get_by_name("DHCPON_OFF",tmp);
				if(strcmp(tmp,"on")==0)
				{
					system("killall udhcpd > /dev/null 2>&1");			 
					system("/etc/rc.d/rc.udhcpd > /dev/null 2>&1");
					system("/usr/sbin/set_addr_conf > /dev/null 2>&1");	
					system("/usr/sbin/udhcpd /etc/udhcpd.conf > /dev/null 2>&1");
				}

				/*make the eth1(br0/lan)'s pc retake ip address*/
				system("ifconfig eth1 down > /dev/null 2>&1");
				system("ifconfig eth1 up > /dev/null 2>&1");
				
				//reboot hostapd
				Execute_cmd("killall hostapd > /dev/null 2>&1",rspBuff);

				Execute_cmd("cfg -e | grep \"AP_SECMODE=\" | awk -F \"=\" \'{print $2}\'",valBuff3);
				if(strstr(valBuff3,"None")) /*wifi doesn't use WPA*/
				Execute_cmd("cfg -t0 /etc/ath/PSK.ap_bss_none ath0 > /tmp/secath0",rspBuff);
				else
				Execute_cmd("cfg -t0 /etc/ath/PSK.ap_bss ath0 > /tmp/secath0",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath0 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);


				Execute_cmd("cfg -e | grep \"AP_SECMODE_2=\" | awk -F \"=\" \'{print $2}\'",valBuff3);
				if(strstr(valBuff3,"None")) /*wifi doesn't use WPA*/
				Execute_cmd("cfg -t2 /etc/ath/PSK.ap_bss_none ath1 > /tmp/secath1",rspBuff);
				else
				Execute_cmd("cfg -t2 /etc/ath/PSK.ap_bss ath1 > /tmp/secath1",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath1 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);

				Execute_cmd("cfg -e | grep \"AP_SECMODE_3=\" | awk -F \"=\" \'{print $2}\'",valBuff3);
				if(strstr(valBuff3,"None")) /*wifi doesn't use WPA*/
				Execute_cmd("cfg -t3 /etc/ath/PSK.ap_bss_none ath2 > /tmp/secath2",rspBuff);
				else
				Execute_cmd("cfg -t3 /etc/ath/PSK.ap_bss ath2 > /tmp/secath2",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath2 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);

				Execute_cmd("cfg -e | grep \"AP_SECMODE_4=\" | awk -F \"=\" \'{print $2}\'",valBuff3);
				if(strstr(valBuff3,"None")) /*wifi doesn't use WPA*/
				Execute_cmd("cfg -t4 /etc/ath/PSK.ap_bss_none ath3 > /tmp/secath3",rspBuff);
				else
				Execute_cmd("cfg -t4 /etc/ath/PSK.ap_bss ath3 > /tmp/secath3",rspBuff);
				Execute_cmd("hostapd -B /tmp/secath3 -e /etc/wpa2/entropy > /dev/null 2>&1",rspBuff);
			}
	   }//end eth0_ip != br0_ip
	   else
	   {
				 fprintf(errOut,"\nbr0_ip shall not be equal to eth0_ip,try others,please\n");
				 char tempu2[128]={0};
                 printf("HTTP/1.0 200 OK\r\n");
                 printf("Content-type: text/html\r\n");
                 printf("Connection: Close\r\n");
                 printf("\r\n");
                 printf("\r\n");
                 

				 printf("<HTML><HEAD>\r\n");
				 printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
				 printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
				 printf("</head><body>");
                 sprintf(tempu2,"<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();alert(_(\"err WANsame\"));window.location.href=\"%s?%s=yes\";</script>",argv[0],argv[0]);
				 printf(tempu2);
				 printf("</body></html>");
				 exit(1);
		
       } 
		gohome =2;
        sleep(10);
    }
    
    /*************************************
    内网设置    路由表设置       添加
   *************************************/
    if(strcmp(CFG_get_by_name("ADD_STATICR",valBuff),"ADD_STATICR") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d ADD_STATICR \n",__func__,__LINE__);
		add_route_rule();
        memset(Page,0,64);
        //sprintf(Page,"%s","../ad_local_rulist.html");
        //gohome =0;

        Normal_tiaozhuan("ad_local_rulist");
         gohome =2;

    }
    /*************************************
    内网设置    路由表设置       删除
   *************************************/
    if(strcmp(CFG_get_by_name("DEL_RU",valBuff),"DEL_RU") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d DEL_RU \n",__func__,__LINE__);
        del_route_rule();
        memset(Page,0,64);
        //sprintf(Page,"%s","../ad_local_rulist.html");
       // gohome =0;
               Normal_tiaozhuan("ad_local_rulist");
         gohome =2;


    }
        /*************************************
    内网设置    路由表设置      修改
   *************************************/
    if(strcmp(CFG_get_by_name("MOD_RU",valBuff),"MOD_RU") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d MOD_RU \n",__func__,__LINE__);
		modify_route_rule();
        memset(Page,0,64);
        //sprintf(Page,"%s","../ad_local_rulist.html");
        //gohome =0;
                Normal_tiaozhuan("ad_local_rulist");
         gohome =2;

    }
    /*************************************
    接入管理    无线接入控制
   
    Access mangement   STA Control
   *************************************/
	/*ADD STA's MAC*/
    if(strcmp(CFG_get_by_name("ADD_CONRULE",valBuff),"ADD_CONRULE") == 0 ) 
    {
    	int ret_add_conrule = add_sta_access();
		
        fprintf(errOut,"\n%s  %d ADD_CONRULE \n",__func__,__LINE__);
        memset(Page,0,64);
        //sprintf(Page,"%s","../ad_con_manage.html");
       // gohome =0;
       if(ret_add_conrule==0)
        {
        Normal_tiaozhuan("ad_con_manage");
         gohome =2;
        }
        else
        {
                fprintf(errOut,"\nret_add_conrule fail\n");
                char tempu2[128]={0};
                 printf("HTTP/1.0 200 OK\r\n");
                 printf("Content-type: text/html\r\n");
                 printf("Connection: Close\r\n");
                 printf("\r\n");
                 printf("\r\n");
                 

printf("<HTML><HEAD>\r\n");
printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
printf("</head><body>");
sprintf(tempu2,"<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"conn\");window.parent.DialogHide();alert(_(\"err MAC exist\"));window.location.href=\"ad_con_manage?ad_con_manage=yes\";</script>");
printf(tempu2);
printf("</body></html>");
exit(1);
        }
    }
	/*DEL STA's MAC*/
	if(strcmp(CFG_get_by_name("DEL_CON",valBuff),"DEL_CON") == 0 ) 
    {	
    	
		del_sta_access();
		
        fprintf(errOut,"\n%s  %d DEL_CON \n",__func__,__LINE__);
        memset(Page,0,64);
        //sprintf(Page,"%s","../ad_con_manage.html?ad_con_manage=yes");
        //gohome =0;
                Normal_tiaozhuan("ad_con_manage");
         gohome =2;

    }
    /*ON-OFF/MODIFY*/
	/*************************************
    接入管理    无线接入控制

    Control STA's MAC
    *************************************/
    if(strcmp(CFG_get_by_name("CONM_WORK",valBuff),"CONM_WORK") == 0 ) 
    {
    	control_sta_access();

		fprintf(errOut,"\n%s  %d --------CONM_WORK--------- \n",__func__,__LINE__);
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
        Result_tiaozhuan("yes",argv[0]);   
		gohome =2;
	}
	/*modify the status*/
	if(strcmp(CFG_get_by_name("MOD_CON",valBuff), "MOD_CON") == 0 ) 
	{
		modify_sta_access();
		
		fprintf(errOut,"\n%s  %d --------MOD_CON--------- \n",__func__,__LINE__);
	}
	
	
	/*************************************
    接入管理   远程web管理
   *************************************/
    if(strcmp(CFG_get_by_name("WEBCONM_WORK",valBuff),"WEBCONM_WORK") == 0 ) 
    {
    	char valBuf[20];
    	Execute_cmd("cfg -e | grep \"WEBCONON_OFF=\" | awk -F \"=\" \'{print $2}\'",valBuf);
		CFG_get_by_name("WEBCONON_OFF",valBuff);
		fprintf(errOut,"\n%s  %d valBuf is [%s] [%s] \n",__func__,__LINE__, valBuf, valBuff);
		
        if(!strstr(valBuf, valBuff) && !strcmp(valBuff, "on"))
        {
            
            Execute_cmd("iptables  -D INPUT -i eth0 -p tcp --dport 80 -j DROP",rspBuff);
            fprintf(errOut,"\n%s  %d --------WEBCONM_WORK on--------- \n",__func__,__LINE__);
        }
        else if(!strstr(valBuf, valBuff) && !strcmp(valBuff, "off"))
        {
            
            Execute_cmd("iptables  -A INPUT -i eth0 -p tcp --dport 80 -j DROP",rspBuff);
            fprintf(errOut,"\n%s  %d --------WEBCONM_WORK off--------- \n",__func__,__LINE__);
        }

            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);
        Result_tiaozhuan("yes",argv[0]);   
        gohome =2;
    }
   /*************************************
    无线设置    高级
   *************************************/
    if(strcmp(CFG_get_by_name("WIRELESS_ADVSET",valBuff),"WIRELESS_ADVSET") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d WIRELESS_ADVSET \n",__func__,__LINE__);
            
		char pChar[128];
		char valTxPwrCfg[128];	
		char valBintCfg[128];	
		char valRtsCfg[128];	
		//char valFragCfg[128];
		char valRateCfg[128];
		char valShorGiCfg[128];
		char valBintCfg_3[128];	
		char valRtsCfg_3[128];	
		char valRateCfg_3[128];
		char valShorGiCfg_3[128];
		char valBuffTemp1[128];
		char valBuffTemp2[128];

    //2G
		Execute_cmd("cfg -e | grep \"BEACON_INT=\" | awk -F \"BEACON_INT=\" '{print $2}'",valBintCfg);
		Execute_cmd("cfg -e | grep \"WIFI_ADV_RSTCTS=\" | awk -F \"WIFI_ADV_RSTCTS=\" \'{print $2}\'",valRtsCfg);
		Execute_cmd("cfg -e | grep \"TXRATE=\" | awk -F \"TXRATE=\" \'{print $2}\'",valRateCfg);
		Execute_cmd("cfg -e | grep \"SHORTGI=\" | awk -F \"SHORTGI=\" \'{print $2}\'",valShorGiCfg);

    //5G
		Execute_cmd("cfg -e | grep \"BEACON_INT_3\" | awk -F \"=\" '{print $2}'",valBintCfg_3);
		Execute_cmd("cfg -e | grep \"WIFI_ADV_RSTCTS_3\" | awk -F \"=\" \'{print $2}\'",valRtsCfg_3);
		Execute_cmd("cfg -e | grep \"TXRATE_3\" | awk -F \"=\" \'{print $2}\'",valRateCfg_3);
		Execute_cmd("cfg -e | grep \"SHORTGI_3\" | awk -F \"=\" \'{print $2}\'",valShorGiCfg_3);
    //set Beacon Intval (2G)
		CFG_get_by_name("BEACON_INT",valBuffTemp1);
		sprintf(valBuffTemp2,"%s\n<br>",valBuffTemp1);
		if((strcmp(valBuffTemp2,valBintCfg) != 0)&&(strcmp(valBuffTemp2,"\n<br>") != 0))
		{
			sprintf(pChar,"iwpriv ath0 bintval %s  > /dev/null 2>&1",valBuffTemp2);
			fprintf(errOut,"\n%s  %d BEACON_INT:%s \n",__func__,__LINE__, pChar);
			Execute_cmd(pChar, rspBuff);
		}
    //set Beacon Intval (5G)
		CFG_get_by_name("BEACON_INT_3",valBuffTemp1);
		sprintf(valBuffTemp2,"%s\n<br>",valBuffTemp1);
		if((strcmp(valBuffTemp2,valBintCfg_3) != 0)&&(strcmp(valBuffTemp2,"\n<br>") != 0))
		{
			sprintf(pChar,"iwpriv ath2 bintval %s  > /dev/null 2>&1",valBuffTemp2);
			fprintf(errOut,"\n%s  %d BEACON_INT_3:%s \n",__func__,__LINE__, pChar);
			Execute_cmd(pChar, rspBuff);
		}

    //set RTS (2G)
		CFG_get_by_name("WIFI_ADV_RSTCTS",valBuffTemp1);
		sprintf(valBuffTemp2,"%s\n<br>",valBuffTemp1);
		if((strcmp(valBuffTemp2,valRtsCfg) != 0)&&(strcmp(valBuffTemp2,"\n<br>") != 0))
		{
			sprintf(pChar,"iwconfig ath0 rts %s  > /dev/null 2>&1",valBuffTemp2);
        fprintf(errOut,"\n%s  %d WIFI_ADV_RSTCTS:%s \n",__func__,__LINE__, pChar);
			Execute_cmd(pChar, rspBuff);

		}
    //set RTS (5G)
		CFG_get_by_name("WIFI_ADV_RSTCTS_3",valBuffTemp1);
		sprintf(valBuffTemp2,"%s\n<br>",valBuffTemp1);
		if((strcmp(valBuffTemp2,valRtsCfg_3) != 0)&&(strcmp(valBuffTemp2,"\n<br>") != 0))
		{
			sprintf(pChar,"iwconfig ath2 rts %s  > /dev/null 2>&1",valBuffTemp2);
        fprintf(errOut,"\n%s  %d WIFI_ADV_RSTCTS_3:%s \n",__func__,__LINE__, pChar);
			Execute_cmd(pChar, rspBuff);

		}

    // set Rate (2G)
		CFG_get_by_name("TXRATE",valBuffTemp1);
		sprintf(valBuffTemp2,"%s\n<br>",valBuffTemp1);
		if((strcmp(valBuffTemp2,valRateCfg) != 0)&&(strcmp(valBuffTemp2,"\n<br>") != 0))
		{

            if(strncmp(valBuffTemp2,"0x",2))
            {
		           CFG_set_by_name("RATECTL","auto");
            }
            else
            {
		           CFG_set_by_name("RATECTL","manual");
            }

		    CFG_set_by_name("MANRATE",valBuffTemp2);
			sprintf(pChar,"iwpriv ath0 set11NRates %s  > /dev/null 2>&1",valBuffTemp2);
        fprintf(errOut,"\n%s  %d TXRATE:%s \n",__func__,__LINE__, pChar);
			Execute_cmd(pChar, rspBuff);

		}
    // set Rate (5G)
		CFG_get_by_name("TXRATE_3",valBuffTemp1);
		sprintf(valBuffTemp2,"%s\n<br>",valBuffTemp1);
		if((strcmp(valBuffTemp2,valRateCfg_3) != 0)&&(strcmp(valBuffTemp2,"\n<br>") != 0))
		{

            if(strncmp(valBuffTemp2,"0x",2))
            {
		           CFG_set_by_name("RATECTL_3","auto");
            }
            else
            {
		           CFG_set_by_name("RATECTL_3","manual");
            }

		    CFG_set_by_name("MANRATE_3",valBuffTemp2);
			sprintf(pChar,"iwpriv ath2 set11NRates %s  > /dev/null 2>&1",valBuffTemp2);
        fprintf(errOut,"\n%s  %d TXRATE_3:%s \n",__func__,__LINE__, pChar);
			Execute_cmd(pChar, rspBuff);

		}

        //set Short GI (2G)
		CFG_get_by_name("SHORTGI",valBuffTemp1);
		sprintf(valBuffTemp2,"%s\n<br>",valBuffTemp1);
		if((strcmp(valBuffTemp2,valShorGiCfg) != 0)&&(strcmp(valBuffTemp2,"\n<br>") != 0))
		{
			sprintf(pChar,"iwpriv ath0 shortgi %s  > /dev/null 2>&1",valBuffTemp2);
			fprintf(errOut,"\n%s  %d SHORTGI:%s \n",__func__,__LINE__, pChar);
			Execute_cmd(pChar, rspBuff);

		}
        //set Short GI (5G)
		CFG_get_by_name("SHORTGI_3",valBuffTemp1);
		sprintf(valBuffTemp2,"%s\n<br>",valBuffTemp1);
		if((strcmp(valBuffTemp2,valShorGiCfg_3) != 0)&&(strcmp(valBuffTemp2,"\n<br>") != 0))
		{
			sprintf(pChar,"iwpriv ath2 shortgi %s  > /dev/null 2>&1",valBuffTemp2);
			fprintf(errOut,"\n%s  %d SHORTGI_3:%s \n",__func__,__LINE__, pChar);
			Execute_cmd(pChar, rspBuff);

		}
		
		//save new config to flash
            writeParametersWithSync();
            //writeParameters(NVRAM,"w+", NVRAM_OFFSET);
            //writeParameters("/tmp/.apcfg","w+",0);			
        Result_tiaozhuan("yes",argv[0]);   
        gohome =2;
    }


    /*************************************
    安全管理    IP/MAC绑定    添加
   *************************************/
    if(strcmp(CFG_get_by_name("ADD_IPMACBIND",valBuff),"ADD_IPMACBIND") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d ADD_IPMACBIND \n",__func__,__LINE__);
		if(add_arp()==1)
        {
           // Result_tiaozhuan("no",argv[0]); 
            gohome =2;
        }
        else
        {
			memset(Page,0,64);
			//sprintf(Page,"%s","../ad_safe_IPMAC.html");
			//gohome =0;
			        Normal_tiaozhuan("ad_safe_IPMAC");
         gohome =2;

        }
    }
  
    /*************************************
    安全管理    IP/MAC绑定    删除
   *************************************/
    if(strcmp(CFG_get_by_name("DEL_IPMAC",valBuff),"DEL_IPMAC") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d DEL_IPMAC \n",__func__,__LINE__);
		del_arp();
        memset(Page,0,64);
			//sprintf(Page,"%s","../ad_safe_IPMAC.html?ad_safe_IPMAC=yes");
			//gohome =0;
						        Normal_tiaozhuan("ad_safe_IPMAC");
         gohome =2;

    }
    /*************************************
    内网设置    IP/MAC绑定    修改
   *************************************/
    if(strcmp(CFG_get_by_name("MOD_IPMAC",valBuff),"MOD_IPMAC") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d MOD_IPMAC \n",__func__,__LINE__);
		modify_arp();
        memset(Page,0,64);
		//sprintf(Page,"%s","../ad_safe_IPMAC.html");
		//gohome =0;

        			        Normal_tiaozhuan("ad_safe_IPMAC");
         gohome =2;

    }
    
    /*************************************
   访问控制    访问管理      添加
   *************************************/
    if(strcmp(CFG_get_by_name("ADD_PARC",valBuff),"ADD_PARC") == 0 ) 
    {		 
        FILE *f_parc;
        int parc_id = 1;
        char cmd_buffer_w[512];
        char cmd_buffer_r[512];
        char cmd_sed_buff[512];
        char *cmd_buffer_p;

        char parc_mode[4] = {0};
        char parc_mac[64] = {0};
        char parc_url[128] ={0};
        int url_num = 0;
        char url_num_str[10] = {0};
        char url_arg[10]={0};
        //char url_tmp[10][100]={0};
        char url_tmp[100]={0};
        char weekdays[32] ={0};
        char time_start_h[32] = {0};
        char time_start_m[32] = {0};
        char time_stop_h[32] = {0};
        char time_stop_m[32] = {0};
        char add_cmd_err[100] = {0};
        char enable_status[10] = {0};
        char enable_cmd[10] = {0};


        int i=0;
        int mode_id=0;
        int parc_line=0;
        int repeat_line=0;
        char *parc_ret;
        int parc_flag = 0;
        int parc_mode_flag = 0;


        CFG_get_by_name("IFMOD",parc_mode);
        CFG_get_by_name("ADD_MAC",parc_mac);
        CFG_get_by_name("WEEK",weekdays);
        CFG_get_by_name("TIMING_BH",time_start_h);
        CFG_get_by_name("TIMING_BM",time_start_m);
        CFG_get_by_name("TIMING_EH",time_stop_h);
        CFG_get_by_name("TIMING_EM",time_stop_m);
        CFG_get_by_name("PARC_STATUS",enable_status);

        mode_id = atoi(parc_mode);

        if(strncmp(enable_status,"1",1)==0)
            strcpy(enable_cmd,"iptables");
        else
            strcpy(enable_cmd,"##");

        for (i = 1; i <= 5; i++) 
        {
            sprintf(url_arg, "ADD_NET%d", i);
            CFG_get_by_name(url_arg,url_tmp);
            if(strlen(url_tmp) != 0)
            {
                strcat(parc_url,url_tmp);
                strcat(parc_url,",");
            }
        }
        strcat(parc_url,"return1");

        f_parc = fopen(PARC_PATH,"a+");

        if(f_parc)
        {
            memset(cmd_buffer_w,0,512);
            sprintf(cmd_buffer_w ,"-A FORWARD_ACCESSCTRL -i br0 -m mac --mac-source %s -m multiurl --url %s -m time --timestart %s:%s --timestop %s:%s --weekdays %s -j RETURN\n", parc_mac, parc_url, time_start_h, time_start_m, time_stop_h, time_stop_m, weekdays);
            while(1)
            {
                parc_ret = fgets(cmd_buffer_r,500,f_parc);
                if(parc_ret == NULL)
                    break;
                cmd_buffer_p = strstr(cmd_buffer_r, "-A");
                if(strcmp(cmd_buffer_w, cmd_buffer_p) == 0)
                {
                    fprintf(errOut,"\n%s  %d there is a same rule exist\n",__func__,__LINE__);
                   // printf("there is a same rule exist\n");				   
                    parc_flag=1;
                    repeat_line=parc_line+1;
                    if(mode_id==repeat_line)
                    {
                        if(strncmp(enable_cmd, cmd_buffer_r,2) != 0)
                            parc_flag=0;
                    }
                }
                parc_line++;
            }
            if(!parc_flag)
            {
                if(!mode_id)
                {
                    sprintf(cmd_sed_buff ,"sed -i '%da\\%s %s' %s",parc_line/2, enable_cmd, cmd_buffer_w, PARC_PATH);
                    if(!parc_line)
                        fprintf(f_parc,"%s %s",enable_cmd, cmd_buffer_w);
                        
                    memset(cmd_buffer_w,0,512);
                    sprintf(cmd_buffer_w ,"%s -A FORWARD_ACCESSCTRL -i br0 -m mac --mac-source %s -j DROP\n", enable_cmd, parc_mac);
                    fprintf(f_parc,"%s",cmd_buffer_w);
                }
            }
        }
        else
        {
            fprintf(errOut,"\n%s  %d open %s error \n",__func__,__LINE__,PARC_PATH);
        }

        fprintf(errOut,"\n%s  %d ADD_PARC \n",__func__,__LINE__);
        fclose(f_parc);

		if(!parc_flag)
		{
			if(parc_line)
                    {
                        if(!mode_id)
        				    system(cmd_sed_buff);
                            else
                            {
                                memset(cmd_sed_buff,0,512);
                                sprintf(cmd_sed_buff ,"sed -i '%dc\\%s %s' %s",mode_id, enable_cmd, cmd_buffer_w, PARC_PATH);
            				    system(cmd_sed_buff);

                                memset(cmd_sed_buff,0,512);
                                sprintf(cmd_sed_buff ,"sed -i '%dc\\%s -A FORWARD_ACCESSCTRL -i br0 -m mac --mac-source %s -j DROP' %s",mode_id+parc_line/2, enable_cmd, parc_mac, PARC_PATH);
            				    system(cmd_sed_buff);
                            }
                    }
			Execute_cmd("iptables -F FORWARD_ACCESSCTRL" , add_cmd_err);
			Execute_cmd("sh /etc/ath/iptables/parc" , add_cmd_err);

			memset(Page,0,64);
			//sprintf(Page,"%s","../ad_parentc_accept.html");
			//gohome =0;

                     Normal_tiaozhuan("ad_parentc_accept");
                    gohome =2;

		}
                else
		{
                    if(mode_id!=repeat_line)
                    {
                    char tempu2[128]={0};
                    printf("HTTP/1.0 200 OK\r\n");
                    printf("Content-type: text/html\r\n");
                    printf("Connection: Close\r\n");
                    printf("\r\n");
                    printf("\r\n");

                    printf("<HTML><HEAD>\r\n");
                    printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">");
                    printf("<script type=\"text/javascript\" src=\"/lang/b28n.js\"></script>");
                    printf("</head><body>");
                    sprintf(tempu2,"<script type='text/javascript' language='javascript'>Butterlate.setTextDomain(\"admin\");window.parent.DialogHide();alert(_(\"err Ruleexist\"));window.location.href=\"ad_parentc_accept?ad_parentc_accept=yes\";</script>");
                    printf(tempu2);
                    printf("</body></html>");
                    exit(1);
                    }
                    else
                    {
                         Normal_tiaozhuan("ad_parentc_accept");
                        gohome =2;
                    }
		}
    }
    /*************************************
    家长控制    访问管理      删除
   *************************************/
    if(strcmp(CFG_get_by_name("DEL_PRC",valBuff),"DEL_PRC") == 0 ) 
    {		 
        int del_id = 0;
        int del_line = 0;
        char del_id_str[10] = {0};
        char del_sed_cmd[100] = {0};
        char del_sed_err[100] = {0};

        CFG_get_by_name("DELXXX",del_id_str);
        del_id = atoi(del_id_str);


        Execute_cmd("awk 'END{print NR}' /etc/ath/iptables/parc" , del_sed_err);
        del_line=atoi(del_sed_err);

        sprintf(del_sed_cmd,"sed -i '%dd' %s", del_id, PARC_PATH);
        Execute_cmd(del_sed_cmd , del_sed_err);

        sprintf(del_sed_cmd,"sed -i '%dd' %s", del_line/2+del_id-1, PARC_PATH);
        fprintf(errOut,"\n%s  %d DEL_PRC sed_cmd :%s \n",__func__,__LINE__,del_sed_cmd);

        Execute_cmd(del_sed_cmd , del_sed_err);
        Execute_cmd("iptables -F FORWARD_ACCESSCTRL" , del_sed_err);
        Execute_cmd("sh /etc/ath/iptables/parc" , del_sed_err);

        memset(Page,0,64);
        //sprintf(Page,"%s","../ad_parentc_accept.html");
       // gohome =0;
        //gohome =2;
        Normal_tiaozhuan("ad_parentc_accept");
         gohome =2;

    }
    /*************************************
    家长控制    访问管理      修改
   *************************************/
    if(strcmp(CFG_get_by_name("MOD_PRC",valBuff),"MOD_PRC") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d MOD_PRC \n",__func__,__LINE__);

        int mod_id = 0;
        int mod_line = 0;
        char mod_id_str[10] = {0};
        char mod_sed_cmd[100] = {0};
        char mod_sed_err[100] = {0};
        char enable_flag[10];

        CFG_get_by_name("MODXXX",mod_id_str);
        CFG_get_by_name("ON_OFF",enable_flag);
        mod_id = atoi(mod_id_str);


        Execute_cmd("awk 'END{print NR}' /etc/ath/iptables/parc" , mod_sed_err);
        mod_line=atoi(mod_sed_err);

        if(strncmp(enable_flag,"ON",2)==0)
        {
            sprintf(mod_sed_cmd,"sed -i '%ds/##/iptables/' %s", mod_id, PARC_PATH);
            Execute_cmd(mod_sed_cmd , mod_sed_err);
            sprintf(mod_sed_cmd,"sed -i '%ds/##/iptables/' %s", mod_line/2+mod_id, PARC_PATH);
            Execute_cmd(mod_sed_cmd , mod_sed_err);

        }
        else
        {
            sprintf(mod_sed_cmd,"sed -i '%ds/iptables/##/' %s", mod_id, PARC_PATH);
            Execute_cmd(mod_sed_cmd , mod_sed_err);
            sprintf(mod_sed_cmd,"sed -i '%ds/iptables/##/' %s", mod_line/2+mod_id, PARC_PATH);
            Execute_cmd(mod_sed_cmd , mod_sed_err);

        }
        fprintf(errOut,"\n%s  %d MOD_PRC sed_cmd :%s \n",__func__,__LINE__,mod_sed_cmd);

        Execute_cmd("iptables -F FORWARD_ACCESSCTRL" , mod_sed_err);
        Execute_cmd("sh /etc/ath/iptables/parc" , mod_sed_err);

        memset(Page,0,64);
        //sprintf(Page,"%s","../ad_parentc_accept.html");
        //gohome =0;

        Normal_tiaozhuan("ad_parentc_accept");
         gohome =2;
    }
    
	/*************************************
    系统设置    时间设置
   *************************************/
    if(strcmp(CFG_get_by_name("TIME_SYNC",valBuff),"TIME_SYNC") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d TIME_SYNC \n",__func__,__LINE__);
		set_pctime();
        gohome =0;
    }
	
    /*************************************
    系统设置    时间设置
   *************************************/
    if(strcmp(CFG_get_by_name("TIMEZONE_SET",valBuff),"TIMEZONE_SET") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d TIMEZONE_SET \n",__func__,__LINE__);
		set_ntp_server();
        Result_tiaozhuan("yes",argv[0]); 
        gohome =2;
    }

	/*set the system time > 1970*/
	if(strcmp(CFG_get_by_name("ad_man_timezone", valBuff),"yes") == 0 ) 
    {
    	Execute_cmd("date |awk -F ' ' '{print $6}'", valBuff);
		//fprintf(errOut,"\n%s  %d the year is %s \n",__func__,__LINE__, valBuff);
		if(atoi(valBuff) < 2014)
		{
			Execute_cmd("date 040100002014", valBuff);
		
        	fprintf(errOut,"\n%s  %d set the system time 2014/04/01 \n",__func__,__LINE__);
		}
    }
    
    /*************************************
   系统设置    配置管理     备份
   *************************************/
    if(strcmp(CFG_get_by_name("LOCAL_SAVE",valBuff),"LOCAL_SAVE") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d ADD_IPMACBIND \n",__func__,__LINE__);	
		add_backup();
        gohome =0;
    }
    /*************************************
   系统设置    配置管理     使用
   *************************************/
    if(strcmp(CFG_get_by_name("CFG_MODIFY_USE",valBuff),"CFG_MODIFY_USE") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d CFG_MODIFY_USE \n",__func__,__LINE__);
		use_backup();
        gohome =2;
    }
    /*************************************
   系统设置    配置管理     删除
   *************************************/
    if(strcmp(CFG_get_by_name("CFG_MODIFY_DEL",valBuff),"CFG_MODIFY_DEL") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d CFG_MODIFY_DEL \n",__func__,__LINE__);
		del_backup();
        gohome =0;
    }

    
    /*************************************
   网络诊断
   *************************************/
    if(strcmp(CFG_get_by_name("NETCHECK",valBuff),"NETCHECK") == 0 ) 
    {		
		Execute_cmd("network_diagnostics > /dev/null 2>&1", rspBuff); 		
        fprintf(errOut,"\n%s  %d NETCHECK \n",__func__,__LINE__);
		
        printf("HTTP/1.0 200 OK\r\n");
        printf("Content-type: text/html\r\n");
        printf("Connection: close\r\n");
        printf("\r\n");
        printf("\r\n");

        printf("<HTML><HEAD>\r\n");
        printf("</head><body>");
 
        printf("<script language=javascript>window.location.href=\"ad_netcheck?INDEX=14\";</script>");
        printf("</body></html>");
        gohome =2;
    }
	
	/*************************************
   重启
   *************************************/
    if(strcmp(CFG_get_by_name("REBOOT",valBuff),"REBOOT") == 0 ) 
    {		 
        fprintf(errOut,"\n%s  %d REBOOT \n",__func__,__LINE__);
#if 0
        printf("HTTP/1.0 200 OK\r\n");
        printf("Content-type: text/html\r\n");
        printf("Connection: close\r\n");
        printf("\r\n");
        printf("\r\n");

        printf("<HTML><HEAD>\r\n");
        printf("<LINK REL=\"stylesheet\" HREF=\"../style/handaer.css\"  TYPE=\"text/css\" media=\"all\">");
        printf("<link rel=\"stylesheet\" href=\"../style/normal_ws.css\" type=\"text/css\">");
		printf("<meta http-equiv=\"Content-Type\" content=\"text/html;charset=UTF-8\">");
        printf("</head><body>");
        
        printf("<div class=\"handaer_main\">");
        printf("<h1>绯荤粺閲嶅惎</h1>\n");	
        printf("<div align=\"center\"> <img align=\"center\" src=\"../images/loading.gif\"></img></div>");
        printf("<p  align=\"center\">姝ｅ湪閲嶅惎BASE..........璇风◢绛?...........</p><br>\n");	
        printf("<p  align=\"center\">Restartting BASE............Please wait............</p><br>\n");	
        printf("<script language=javascript>setTimeout(function(){window.location.href=\"map\";},60000);</script>");
        printf("</div>");
        printf("</body></html>");
#endif	
        Reboot_tiaozhuan("reboot","index.html");
        system("sleep 1 && reboot &");
        gohome =2;
		//luodp
		//reboot(RB_AUTOBOOT);
    }
	
    fprintf(errOut,"\n----------argv[0]:%s gohome:%d\n",argv[0],gohome);

    if( gohome == 0)//高级设置不显示设置结果
    {
   		printf("HTTP/1.0 200 OK\r\n");
   		printf("Content-type: text/html\r\n");
    	printf("Connection: close\r\n");
    	printf("\r\n");
    	printf("\r\n");
        system("dd if=/dev/caldata of=/etc/cal.bin   > /dev/null 2>&1");
        if(translateFile(Page) < 0)
        {
			fprintf(errOut,"\n%s  %d \n",__func__,__LINE__);
			printf("Content-Type:text/html\n\n");
			printf("<HTML><HEAD>\r\n");
			printf("<LINK REL=\"stylesheet\" href=\"../styleSheet.css\" type=\"text/css\">");
			printf("</head><body>");
			printf("Page %s Not Found",Page);
			printf("</body></html>");
			exit(1);
        }	
    }
    else if( gohome == 1)//w1 w2 w3 w4 wwai
    {
        if((strcmp(argv[0],"w1")==0)||(strcmp(argv[0],"w2")==0)||(strcmp(argv[0],"w3")==0)||(strcmp(argv[0],"w4")==0)||(strcmp(argv[0],"wwai")==0))
        {
            Result_tiaozhuan("yes","map");            
        }
        else
        {
            Result_tiaozhuan("yes",argv[0]);            
        }
    }
    else if( gohome == 2)//高级设置显示结果
    {
        exit(0);
    }
	else
	{
        printf("HTTP/1.0 200 OK\r\n");
        printf("Content-type: text/html\r\n");
        printf("Connection: close\r\n");
        printf("\r\n");
        printf("\r\n");
    
		fprintf(errOut,"\n%s  %d \n",__func__,__LINE__);
        if(translateFile(Page) < 0)
        {
			fprintf(errOut,"\n%s  %d \n",__func__,__LINE__);
			printf("Content-Type:text/html\n\n");
			printf("<HTML><HEAD>\r\n");
			printf("<LINK REL=\"stylesheet\" href=\"../styleSheet.css\" type=\"text/css\">");
			printf("</head><body>");
			printf("Page %s Not Found",Page);
			printf("</body></html>");
			exit(1);
        }
	}

	
	#endif /* #ifndef ATH_SINGLE_CFG */
     //fprintf(errOut,"%s  %d\n",__func__,__LINE__);
   	exit(0);
}

/********************************** End of Module *****************************/

