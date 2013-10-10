#include <stdio.h>
#include <stdlib.h>

#include "dev_info.h"

#define DEV_TYPE "dev_type"
#define DEV_ID "dev_id"
#define DEV_HD_VER "dev_hd_ver"
#define DEV_SW_VER "dev_sw_ver"


int main(int argc, char **argv)
{
	if(argc != 2)
	{
		printf("Need correct paramter\n");
		return -1;
	}
	 
    eqp_t * eqp_p = get_eqp_info();

    if(strcmp(argv[1], DEV_TYPE) == 0)
    {
        printf("%s\n", eqp_p -> eqp_type);
        return 0;
    }

    if(strcmp(argv[1], DEV_ID) == 0)
    {
        printf("%d\n", eqp_p -> eqp_id);
        return 0;
    }

    if(strcmp(argv[1], DEV_HD_VER) == 0)
    {
        printf("V%d.%d\n", eqp_p -> eqp_hd_info.major,
               eqp_p -> eqp_hd_info.minor);
        return 0;
    }

    if(strcmp(argv[1], DEV_SW_VER) == 0)
    {
        printf("V%d.%d\n", eqp_p -> eqp_sw_info.major,
               eqp_p -> eqp_sw_info.minor);
        return 0;
    }

    printf("Unknow Command");
    return -1;
}

eqp_t *get_eqp_info(void)
{
    static eqp_t eqp_info = {.eqp_type = "ctc_e8_router",
                             .eqp_id = 1001,
                             .eqp_hd_info.major = 1,
                             .eqp_hd_info.minor = 0,
                             .eqp_sw_info.major = 1,
                             .eqp_sw_info.minor = 0
    };

        // TODO: call system api. currently, just return inital value

    return &eqp_info;
}
