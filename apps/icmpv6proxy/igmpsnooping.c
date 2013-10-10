#include "defs.h"

static int igmpSState = 0;
    
// Local function Prototypes
int icmpv6ProxyInit();
void igmpProxyCleanUp();

void 
setIgmpSState(int state)
{
    igmpSState = state;
}

int
getIgmpSState()
{
    return igmpSState;
}

/**
*   Handles the initial startup of IGMP SNOOPING
*/

int igmpSnoopInit()
{
    unsigned char mac[6];
    atlog(LOG_DEBUG, 0, "igmpSnoopInit...");
    initFdbTable();
    enableHwIcmpv6();
 
    //Disable unknown multicast packets over vlans.
    setMultiOnVlan(0);

    //Insert 224.0.0.1 into ARL table
    addMFdb(&allhosts_group,30);  

    mac[0] = 0x01;
    mac[1] = 0x00;
    mac[2] = 0x5E;
    mac[3] = 0x00;
    mac[4] = 0x00;
    mac[5] = 0x01;
    addMFdbByMac(mac,30);

    initSnoopBr();
    return 1;
}

/**
*   Clean up all on exit...
*/
void
igmpSnoopCleanUp()
{
    atlog(LOG_DEBUG, 0, "igmpSnoopCleanUp...");
    //clearSnoopBr();

    //Enable unknow multicaset packets over vlans.
    setMultiOnVlan(1);

    // delete 224.0.0.1 from ARL table.
    delMFdb(&allhosts_group);  
  
    clearAllFdbs();
    disableHwIGMPS();

}
