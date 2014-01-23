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

#include<time.h>
#include<pthread.h>

//#include<string.h>
//#include <stdlib.h>
//#include <stdio.h>




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
int lists=0;
static int flaglist;


/*
** Internal Prototypes
*/

char *CFG_get_by_name(char *name,char *buff);
char *extractParam(char *str,char *Name,char *Val);
char *Execute_cmd(char *cmd,char *buffer);
char *expandOutput(char *inBuff, char *outBuff);
void htmlEncode(char *src, char *dest);
int	isKeyHex(char *key,int flags);

int  CFG_set_by_name(char *name,char *val);

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

            CFG_set_by_name(buff,vPtr);
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
				if(flaglist!=1)
					break;
				/*int num;
				//val1-mac,val2-ssid,val3-security,val4-channel
				if(strcmp(val3[0],"on")==0)
					sprintf(val3[0],"WPA");
				else
					sprintf(val3[0],"None");
				outBuff +=sprintf(outBuff,"<option>%s(%s)(%s)(%s)</option>",val2[0],val1[0],val4[0],val3[0]);
				for(num=1;num<lists;num++)
				{
					if(strcmp(val3[num]+4,"on")==0)
						sprintf(val3[num]+4,"WPA");
					else
						sprintf(val3[num]+4,"None");
					//printf("[luodp] %s(%s)(%s)(%s) \n",val2[num]+4,val1[num]+4,val3[num]+4,val4[num]+4);
					outBuff +=sprintf(outBuff,"<option>%s(%s)(%s)(%s)</option>",val2[num]+4,val1[num]+4,val4[num]+4,val3[num]+4);
				}*/
				FILE *fp;
                int ii;
                char ch;
                char tmpc[65536]={0};
                if((fp=fopen("/tmp/wifilist","r"))==NULL)
                {
                    printf("\n----------cannot open file  line:%d\n",__LINE__);
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
                    system("rm /tmp/wifilist");
                }
				flaglist=0;
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
            /*
            ** We don't want to store the "update" or "commit" parameters, so
            ** remove them if we get here.  Also, if we have values that have
            ** no value, don't write them out.
            DHCP SIP PPP L2TP P2TP WIRRE WIRELESS DHCPW SIPW PPPW L2TPW P2TPW ADMINSET
            */

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

//    printf("%s: Index set to %d\n",__func__,parameterIndex);
    if(index > 1)
        sprintf(varname,"AP_RADIO_ID_%d",parameterIndex);
    else
        strcpy(varname,"AP_RADIO_ID");

    valBuff[0] = 0;
//    printf("%s: Getting %s\n",__func__,varname);

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



//added by yhl,2013.12.17
//get_now_seconds,from  00h/00m/00s  for any year
int get_now_seconds(void)
{
	 char hh_char[10],mm_char[10],ss_char[10];
	 int hh_int,mm_int,ss_int;
	 int final_sec;
	 
	 Execute_cmd("date +%H", hh_char);
	 hh_int=atoi(hh_char);

	  Execute_cmd("date +%M", mm_char);
	  mm_int=atoi(mm_char);

	  Execute_cmd("date +%S", ss_char);
	  ss_int=atoi(ss_char);

	  final_sec=(hh_int*3600+mm_int*60+ss_int);
	  return final_sec;
}


//three thread handler func,  added by yhl 2013.12.04
int * pppoe_demand_thread(void)
{	    
        printf("pppoe_demand_thread id is %d.\n",pthread_self());
        char pppoe_argu[10];
		int  argu_int;
		int  start=0;
		char mac_list[10];
		char first=1;
	
		memset(pppoe_argu,'\0',10);
		memset(mac_list,'\0',10);
				
	while(1)
	{
		if((!strncmp(Execute_cmd("grep 'PPPOE_MODE' /tmp/.apcfg | awk -F '=' '{ print $2 }'|cut -c1-6", rspBuff),"demand",6))&&
			(!strncmp(Execute_cmd("grep 'WAN_MODE' /tmp/.apcfg | awk -F '=' '{ print $2 }'|cut -c1-5", rspBuff),"pppoe",5)))
		    {//pppoe+demand
				Execute_cmd("grep 'PPPOE_DEMAND' /tmp/.apcfg | awk -F '=' '{ print $2 }'", pppoe_argu);
				argu_int=atoi(pppoe_argu);
				Execute_cmd("wlanconfig ath0 list | grep ADDR |awk '{print $1}'|cut -c1-4",mac_list);
					if(strncmp(mac_list,"ADDR",4))//no station
					 {
						   if(first)
						   	 {start = get_now_seconds();}
						   if(get_now_seconds()<(start+argu_int*60))
                                {system("pppoe-start > /dev/null 2>&1");}
						   else 
							    {
							     system("pppoe-stop > /dev/null 2>&1");
						         sleep(5);
								 }
						   first=0;
					 }
					else if(!strncmp(mac_list,"ADDR",4))//have station
					 {  
						  system("pppoe-start > /dev/null 2>&1");sleep(5);
						  start = get_now_seconds();
						  first=0;
                     }
			}
	usleep(10*1000);//must have
  }
             
}


int * pppoe_manual_thread(void *arg)
	{		
			printf("pppoe_manual_thread id is %d.\n",pthread_self());
			char pppoe_argu[10];
			int  argu_int;
			int  start=0;
			char mac_list[10];
			char first=1;
			char flag_manual=1;
		
			memset(pppoe_argu,'\0',10);
			memset(mac_list,'\0',10);
					
		while(1)
		{
			if((!strncmp(Execute_cmd("grep 'PPPOE_MODE' /tmp/.apcfg | awk -F '=' '{ print $2 }'|cut -c1-6", rspBuff),"manual",6))&&
				(!strncmp(Execute_cmd("grep 'WAN_MODE' /tmp/.apcfg | awk -F '=' '{ print $2 }'|cut -c1-5", rspBuff),"pppoe",5)))
				{//pppoe+manual
					Execute_cmd("grep 'PPPOE_MANUAL' /tmp/.apcfg | awk -F '=' '{ print $2 }'", pppoe_argu);
					argu_int=atoi(pppoe_argu);
					Execute_cmd("wlanconfig ath0 list | grep ADDR |awk '{print $1}'|cut -c1-4",mac_list);
						if(strncmp(mac_list,"ADDR",4))//no station
						 {
							   if(first)
								 {start = get_now_seconds();}
							   if(get_now_seconds()<(start+argu_int*60))
									{system("pppoe-start > /dev/null 2>&1");}
							   else 
									{
									 system("pppoe-stop > /dev/null 2>&1");
									 sleep(5);
									 }
							   first=0;
							   flag_manual=0;
							   continue;
						 }
						else if((!strncmp(mac_list,"ADDR",4))&&(flag_manual))
						 {	
							  system("pppoe-start > /dev/null 2>&1");sleep(5);
							  start = get_now_seconds();
							  first=0;
							  continue;
						 }
						else if((!strncmp(mac_list,"ADDR",4))&&(!flag_manual))
						 {	
							  system("pppoe-stop > /dev/null 2>&1");sleep(5);
							  first=0;
							  continue;
						 }
				}
		usleep(10*1000);//must have
	  }
				 
	}


int * pppoe_timing_thread(void *arg)
{
        printf("pppoe_timing_thread id is %d.\n",pthread_self());
         int seconds;
		 int b_time,e_time;
		 char bh[10],bm[10],eh[10],em[10],pppoe_argu[10];
		 int bh_int,bm_int,eh_int,em_int;

		memset(bh,'\0',10);
		memset(bm,'\0',10);
		memset(eh,'\0',10);
		memset(em,'\0',10);
		memset(pppoe_argu,'\0',10);
while(1)
 {			
	if((!strncmp(Execute_cmd("grep 'PPPOE_MODE' /tmp/.apcfg | awk -F '=' '{ print $2 }'|cut -c1-6", rspBuff),"timing",6))&&
		(!strncmp(Execute_cmd("grep 'WAN_MODE' /tmp/.apcfg | awk -F '=' '{ print $2 }'|cut -c1-5", rspBuff),"pppoe",5)))
	     {//pppoe+timing
	         Execute_cmd("grep 'PPPOE_TIMING_BH' /tmp/.apcfg | awk -F '=' '{ print $2 }'", bh);bh_int=atoi(bh);
			 Execute_cmd("grep 'PPPOE_TIMING_BM' /tmp/.apcfg | awk -F '=' '{ print $2 }'", bm);bm_int=atoi(bm);
			 Execute_cmd("grep 'PPPOE_TIMING_EH' /tmp/.apcfg | awk -F '=' '{ print $2 }'", eh);eh_int=atoi(eh);
			 Execute_cmd("grep 'PPPOE_TIMING_EM' /tmp/.apcfg | awk -F '=' '{ print $2 }'", em);em_int=atoi(em);
		
			  b_time=(bh_int*3600+bm_int*60);
			  e_time=(eh_int*3600+em_int*60);//b_time<   <e_time
			  
				seconds= get_now_seconds();
				if((seconds>=b_time)&&(seconds<=e_time))
				  {
					printf("timing pppoe<%d---%d>,and now->%d\n",b_time,e_time,seconds);
				    system("pppoe-start > /dev/null 2>&1");
					sleep(5);
				  }
				else
				  {system("pppoe-stop > /dev/null 2>&1");
				   sleep(5);
				  }
		}	
	usleep(10*1000);
  }
}


//boot by cgi exec_cmd func,if pppoe
int main()
{
 pthread_t pppoe_demand_tid;
 pthread_t pppoe_manual_tid;
 pthread_t pppoe_timing_tid;

 char manbuf[10];
 memset(manbuf,'\0',10);

 printf("main process\n");
  if(!strncmp(Execute_cmd("grep 'WAN_MODE' /tmp/.apcfg | awk -F '=' '{ print $2 }'|cut -c1-5", manbuf), "pppoe", 5))
   {//current is pppoe
      if(!pthread_create(&pppoe_demand_tid,NULL,(void *)pppoe_demand_thread,NULL))
          {printf("create pppoe_demand_thread\n");}
      else
          {printf("Fail to Create pppoe_demand_thread");}
	  
      if(!pthread_create(&pppoe_manual_tid,NULL,(void *)pppoe_manual_thread,NULL))
          {printf("create pppoe_manual_thread\n");}
      else
          {printf("Fail to Create pppoe_manual_thread");}
	  
      if(!pthread_create(&pppoe_timing_tid,NULL,(void *)pppoe_timing_thread,NULL))
          {printf("create pppoe_timing_thread\n");}
      else
          {printf("Fail to Create pppoe_timing_thread");}
   }
  pthread_join(pppoe_demand_tid,NULL);
  pthread_join(pppoe_manual_tid,NULL);
  pthread_join(pppoe_timing_tid,NULL);
  
  printf("printf main proc end\n");
  
}  
/********************************** End of Module ,added by yhl date 2013.12.13*****************************/





