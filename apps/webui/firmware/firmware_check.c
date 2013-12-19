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
#include <fcntl.h>

#define FIRST_OFFSET 16

#define FLAG_OFFSET 1441776
#define MAX_SIZE  1024
#define FILE_READ  "/tmp/9344version.img"

char flagbuf[]="xxxxxxx9344base";
int main(int argc,char **argv)
{
    FILE  *f;
	int len;
	char buf[MAX_SIZE];
	int i=0;
	
	memset (buf , 0, sizeof(buf));
	
	if(  argc !=  2){
		printf("usage:fireware_check  ota.bin\n");
		return 1;
	}
	f = fopen( argv[1] , "r" );
	if (!f)
	{
		printf("ERROR:  the file is not found\n");
		exit(-1);
	}

	
	fseek(f, FIRST_OFFSET, SEEK_SET);
	len = fread (buf ,1,4,f );
	if( !len)
	{
		printf("ERROR:  the read operation is fail\n");
	}

	memset (buf , 0, sizeof(buf));

	
	fseek(f, FLAG_OFFSET, SEEK_SET);
	len = fread (buf ,1,16,f );
	if( !len)
	{
		printf("ERROR:  the read operation is fail\n");
	}


	if (strncmp(buf,flagbuf,15) != 0)
	{
		
		printf("ERROR:  the flag of fireware is wrong\n");
		
		return 1;
	}
	
	printf("It's OK\n");
		
	fclose(f);

	return 0;
}

