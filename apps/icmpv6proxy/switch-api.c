#include "defs.h"
#include <linux/sockios.h>
#include "switch-api.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <athrs_ctrl.h>

static void 
IpAddrToMacAddr(struct in6_addr *group, fal_mac_addr_t *mac)
{   
    mac->addr[0] = 0x33;
    mac->addr[1] = 0x33;
    mac->addr[2] = group->s6_addr[12];
    mac->addr[3] = group->s6_addr[13];
    mac->addr[4] = group->s6_addr[14];
    mac->addr[5] = group->s6_addr[15];
}

#if 0
static void
error_print(sw_error_t rtn)
{
	switch (rtn) {
    case SW_FAIL:{
        atlog(LOG_WARNING, 0, "Operation failed\n");
        break;
    }
    case SW_BAD_VALUE:{
        atlog(LOG_WARNING, 0, "Illegal value\n");
        break;
    }
    case SW_OUT_OF_RANGE:{
        atlog(LOG_WARNING, 0, "Value is out of range\n");
        break;
    }
    case SW_BAD_PARAM:{
        atlog(LOG_WARNING, 0, "Illegal parameter(s)\n");
        break;
    }
    case SW_BAD_PTR:{
        atlog(LOG_WARNING, 0, "Illegal pointer value\n");
        break;
    }
    case SW_BAD_LEN:{
        atlog(LOG_WARNING, 0, "Wrong length\n");
        break;
    }
    case SW_READ_ERROR:{
        atlog(LOG_WARNING, 0, "Read operation failed\n");
        break;
    }
    case SW_WRITE_ERROR:{
        atlog(LOG_WARNING, 0, "Write operation failed\n");
        break;
    }
    case SW_CREATE_ERROR:{
        atlog(LOG_WARNING, 0, "Fail in creating an entry\n");
        break;
    }
    case SW_DELETE_ERROR:{
        atlog(LOG_WARNING, 0, "Fail in deleteing an entry\n");
        break;
    }
    case SW_NOT_FOUND:{
        atlog(LOG_WARNING, 0, "Entry not found\n");
        break;
    }
    case SW_NO_CHANGE:{
        atlog(LOG_WARNING, 0, "The parameter(s) is the same\n");
        break;
    }
    case SW_NO_MORE:{
        atlog(LOG_WARNING, 0, "No more entry found \n");
        break;
    }
    case SW_NO_SUCH:{
        atlog(LOG_WARNING, 0, "No such entry \n");
        break;
    }
    case SW_ALREADY_EXIST:{
        atlog(LOG_WARNING, 0, "Tried to create existing entry \n");
        break;
    }
    case SW_FULL:{
        atlog(LOG_WARNING, 0, "Table is full \n");
        break;
    }
    case SW_EMPTY:{
        atlog(LOG_WARNING, 0, "Table is empty \n");
        break;
    }
    case SW_NOT_SUPPORTED:{
        atlog(LOG_WARNING, 0, "This request is not support   \n");
        break;
    }
    case SW_NOT_IMPLEMENTED:{
        atlog(LOG_WARNING, 0, "This request is not implemented \n");
        break;
    }
    case SW_NOT_INITIALIZED:{
        atlog(LOG_WARNING, 0, "The item is not initialized \n");
        break;
    }
    case SW_BUSY:{
        atlog(LOG_WARNING, 0, "Operation is still running n");
        break;
    }
    case SW_TIMEOUT:{
        atlog(LOG_WARNING, 0, "Operation Time Out \n");
        break;
    }
    case SW_DISABLE:{
        atlog(LOG_WARNING, 0, "Operation is disabled \n");
        break;
    }
    case SW_NO_RESOURCE:{
        atlog(LOG_WARNING, 0, "Resource not available (memory ...)\n");
        break;
    }
    case SW_INIT_ERROR:{
        atlog(LOG_WARNING, 0, "Error occured while INIT process\n");
        break;
    }
    case SW_NOT_READY:{
        atlog(LOG_WARNING, 0, "The other side is not ready yet \n");
        break;
    }
    case SW_OUT_OF_MEM:{
        atlog(LOG_WARNING, 0, "Cpu memory allocation failed\n");
        break;
    }
    case SW_ABORTED:{
        atlog(LOG_WARNING, 0, "Operation has been aborted.\n");
        break;
    }

    default:
        atlog(LOG_WARNING, 0, "ioctl error return code <%d>\n", rtn);
    }
}
#endif

int addMFdb(struct in6_addr *group, uint32 port)
{
    int s = -1;
    struct ifreq ifr = {};
    arl_struct_t arl;
    fal_mac_addr_t macAddr;
    struct eth_cfg_params ethcfg = {};

    if(port > 31) 
        return -1;
    
    if(0 > (s=socket(AF_INET, SOCK_DGRAM, 0))) 
        return -1;

    memcpy(ifr.ifr_name, IF_NAME, IFNAMSIZ);
    IpAddrToMacAddr(group, &macAddr);
    memcpy(&arl.hwaddr,&macAddr,6);

    arl.port_map = port;
    arl.sa_drop = 0;

    memcpy(&ethcfg.vlanid,&arl,sizeof(arl_struct_t));
    ethcfg.cmd = ATHR_ARL_ADD;
    ifr.ifr_ifru.ifru_data = (void *)&ethcfg;
    if (ioctl(s, ATHR_VLAN_IGMP_IOC, &ifr) < 0) {
        close(s);
        return -1;
    } 
    //atlog(LOG_DEBUG, 0, "addMFdb group %x port %d.\n",group,port);
    close(s);
    return 0;
}


int addMFdbByMac(unsigned char * macAddr, uint32 port)
{
    int s = -1;
    struct ifreq ifr = {};
    arl_struct_t arl;
    struct eth_cfg_params ethcfg = {};

    if(port > 31) return -1;
    
    if(0 > (s=socket(AF_INET, SOCK_DGRAM, 0))) {
        return -1;
    }

    memcpy(ifr.ifr_name, IF_NAME, IFNAMSIZ);
    memcpy(&arl.hwaddr,macAddr,6);

    arl.port_map = port;
    arl.sa_drop = 0;

    memcpy(&ethcfg.vlanid,&arl,sizeof(arl_struct_t));
    ethcfg.cmd = ATHR_ARL_ADD;
    ifr.ifr_ifru.ifru_data = (void *)&ethcfg;

    if (ioctl(s, ATHR_VLAN_IGMP_IOC, &ifr) < 0) {
        close(s);
        return -1;
    } 
    close(s);
    return 0;
}


int delMFdb(struct in6_addr *group)
{
    int s = -1;
    struct ifreq ifr = {};
    arl_struct_t arl;
    fal_mac_addr_t macAddr;
    struct eth_cfg_params ethcfg = {};

    if(0 >= (s=socket(AF_INET, SOCK_DGRAM, 0))) 
        return -1;

    memcpy(ifr.ifr_name, IF_NAME, IFNAMSIZ);
    IpAddrToMacAddr(group, &macAddr);
    memcpy(&arl.hwaddr,&macAddr,6);
   
    memcpy(&ethcfg.vlanid,&arl,sizeof(arl_struct_t));
    ethcfg.cmd = ATHR_ARL_DEL;
    ifr.ifr_ifru.ifru_data = (void *)&ethcfg;
    if (ioctl(s, ATHR_VLAN_IGMP_IOC, &ifr) < 0) {
        close(s);
        return -1;
    }

    //atlog(LOG_DEBUG, 0, "delMFdb group %x.OK.\n",group);
    close(s);
    return 0;
}

//0: switch off the unkown multicast packets over vlan. 1: allow the unknown multicaset packets over vlans.
int setMultiOnVlan(int enable)
{
    int s = -1;
    struct ifreq ifr;
    struct eth_cfg_params ethcfg = {};

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    memcpy(ifr.ifr_name, IF_NAME, IFNAMSIZ);

    ethcfg.val = enable;
    ethcfg.cmd = ATHR_MCAST_CLR;
    ifr.ifr_ifru.ifru_data = (void *)&ethcfg;
    if (ioctl(s, ATHR_VLAN_IGMP_IOC, &ifr) != 0) {
	   close(s);
	   return -1;
    }

    close(s);    atlog(LOG_DEBUG, 0,"setMultiOnVlan %d.\n",enable);
    return 0;
}


int enableHwIcmpv6()
{
    int s = -1;
    struct ifreq ifr;
    uint32 port_id;
    struct eth_cfg_params ethcfg = {};
      
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    memcpy(ifr.ifr_name, IF_NAME, IFNAMSIZ);
    for(port_id = 1; port_id <= SW_MAX_NR_PORTS; port_id++)
    {
        ethcfg.val = port_id & 0x1f;
        ethcfg.val |= 1 << 7;

        ethcfg.cmd = ATHR_IGMP_ON_OFF;
        ifr.ifr_ifru.ifru_data = (void *)&ethcfg;

        if (ioctl(s, ATHR_VLAN_IGMP_IOC, &ifr) != 0) {
	   close(s);
	   return -1;
        }
    } 
    close(s);    atlog(LOG_DEBUG, 0,"enableHwIcmpv6 OK.\n");
    return 0;
}

int disableHwIGMPS()
{
    int s = -1;
    struct ifreq ifr;
    uint32 port_id;
    struct eth_cfg_params ethcfg = {};
       
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    memcpy(ifr.ifr_name, IF_NAME, IFNAMSIZ);
    for(port_id = 1; port_id <= SW_MAX_NR_PORTS; port_id++){

        ethcfg.val = port_id & 0x1f;
        ethcfg.val &= ~(1 << 7);

        ethcfg.cmd = ATHR_IGMP_ON_OFF;
        ifr.ifr_ifru.ifru_data = (void *)&ethcfg;

        if (ioctl(s, ATHR_VLAN_IGMP_IOC, &ifr) != 0) {
	   close(s);
	   return -1;
        }
    } 
    close(s);
    atlog(LOG_DEBUG, 0,"disableHwIGMPS OK.\n");
    return 0;
}
