#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *errOut;
char *Execute_cmd(char *cmd,char *buffer)
{
    FILE            *f;
    char            *retBuff = buffer;
    char            cmdLine[1024];

    /*
    ** Provide he command string to popen
    */
 //   fprintf(errOut,"Execute_cmd start ^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

    f = popen(cmd, "r");

    if(f)
    {
  //      fprintf(errOut,"Execute_cmd start ^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

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
  //  fprintf(errOut,"Execute_cmd end^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

    return(retBuff);
}

int main()
{

    char *req_method;
    char rspBuff1[65536];
    req_method = getenv("REQUEST_METHOD");

    errOut = fopen("/dev/ttyS0","w");
    fprintf(errOut,"ping start ^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

    Execute_cmd("killall net_test > /dev/null 2>&1 ",rspBuff1);
    Execute_cmd("/bin/net_test > /dev/null 2>&1 &",rspBuff1);

 //   fprintf(errOut,"ping end ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
    
    return 1;
}


