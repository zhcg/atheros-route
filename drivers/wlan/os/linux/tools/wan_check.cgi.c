#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cgiMain.h"
static FILE *errOut;

char *Execute_cmd(char *cmd,char *buffer)
{
    FILE            *f;
    char            *retBuff = buffer;
    char            cmdLine[1024];

    /*
    ** Provide he command string to popen
    */

    f = popen(cmd, "r");

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
            buffer += strlen(buffer);
        }

        pclose(f);
    }

    return(retBuff);
}

int main()
{
    char rspBuff1[65536];
	
	write_systemLog("wan_check setting  begin"); 
	
    errOut = fopen("/dev/ttyS0","w");

    Execute_cmd("net_check", rspBuff1);
    printf("%s",rspBuff1);
    fprintf(errOut,"%s!!!!!  %d \n",rspBuff1,strlen(rspBuff1));
	
	write_systemLog("wan_check setting  end"); 

    return 1;
}


