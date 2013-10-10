#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "errorcode.h"
#include "debug_if.h"
#include "errorcode.h"
#include "dyndns.h"

#define DDNS_PID_FILE "/etc/inadyn.pid"
#define DDNS_STATUS_FILE "/etc/inadyn.status"

int main(int argc, char* argv[])
{

	RC_TYPE rc_maincall = RC_OK, rc_general = RC_OK;
	DYN_DNS_CLIENT *p_dyndns = NULL;
        FILE *fp;

	/* initialize the status file to say OK */
	if ((fp = fopen(DDNS_STATUS_FILE, "w")))
	  {
            fprintf(fp,"%d %s\n",rc_maincall,errorcode_get_name(rc_maincall) );
	    fclose(fp);
	  }

	do
	{
		rc_general = dyn_dns_construct(&p_dyndns);
		if (rc_general != RC_OK)
		{
			break;
		}    
		rc_maincall = dyn_dns_main(p_dyndns, argc, argv);
	}
	while(0);

	/* status file should reflect the outcome of running the inadyn client */
	if ((fp = fopen(DDNS_STATUS_FILE, "w")))
	  {
            fprintf(fp,"%d %s\n",rc_maincall,errorcode_get_name(rc_maincall) );
	    fclose(fp);
	  }

	/* For console debug */
/* 	if (rc_maincall != 0) */
/* 	{ */
/*           printf("error executing dyn_dns %d-%s\n",rc_maincall,errorcode_get_name(rc_maincall)); */
/* 	} */
	
	rc_general = dyn_dns_destruct(p_dyndns);
	if (rc_general != RC_OK)
	{
                printf("error destructing dyn_dns %d\n", rc_general);
	}
	 

	os_close_dbg_output();
	return (int) rc_maincall;
}

int inadyn_main(int c, char *v[])
{
  /* satisfy a compilation error */
  return 0;
}
