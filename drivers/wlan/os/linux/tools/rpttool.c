#include "rpttool.h"


struct rptgput_msg *rptgput_data_buf;

RPTGPUT_MSG *rptgput_data_buf;


/********* Functions added for iwpriv support. Taken as is from wlancfg.c **********/
static iwprivargs *wlan_hw_if_privcmd_list = NULL;
static iwprivargs *wlan_vap_if_privcmd_list = NULL;
static int iwpriv_hw_if_cmd_numbers=0;
static int iwpriv_vap_if_cmd_numbers=0;

int athcfg_sock_init(const char *caller)
{
    int sockfd;
    char buf[50];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        sprintf(buf,"%s: socket err caller: %s", __func__, caller);
        perror(buf);
    }
    return sockfd;
}

/* Execute a private command on the interface */
static iwpriv_init(int skfd,  const char *ifname)
{
    int iwpriv_cmds = 0;
    iwprivargs **iwpriv_list;

    if (!strcmp(ifname,"wifi0"))
      iwpriv_list = &wlan_hw_if_privcmd_list;
    else
      iwpriv_list = &wlan_vap_if_privcmd_list;

    /* Read the private ioctls */
    iwpriv_cmds = iw_get_priv_info(skfd, ifname, iwpriv_list);

    /* Is there any ? */
    if(iwpriv_cmds <= 0) {
      /* Should I skip this message ? */
      fprintf(stderr, "%-8.16s  no private ioctls.\n\n", ifname);
      free(*iwpriv_list);
      return(-1);
    }

    if (!strcmp(ifname,"wifi0"))
      iwpriv_hw_if_cmd_numbers = iwpriv_cmds;
    else
      iwpriv_vap_if_cmd_numbers = iwpriv_cmds;

    return 0;
}

static void iwpriv_deinit()
{
    if (wlan_hw_if_privcmd_list != NULL)
        free(wlan_hw_if_privcmd_list);

    if (wlan_vap_if_privcmd_list != NULL)
        free(wlan_vap_if_privcmd_list);
}

/* Execute a private command on the interface */
static int iwpriv_set_private_cmd(int skfd, char args[][30], int count, 
        char * ifname, char * cmdname, iwprivargs * priv, int priv_num)
{
    struct iwreq    wrq;
    u_char  buffer[4096];   /* Only that big in v25 and later */
    int     i = 0;          /* Start with first command arg */
    int     k;              /* Index in private description table */
    int     temp;
    int     subcmd = 0;     /* sub-ioctl index */
    int     offset = 0;     /* Space for sub-ioctl index */

    /* Check if we have a token index.
     * Do it now so that sub-ioctl takes precedence, and so that we
     * don't have to bother with it later on... */
    if((count >= 1) && (sscanf(args[0], "[%i]", &temp) == 1)) {
        subcmd = temp;
        args++;
        count--;
    }

    /* Search the correct ioctl */
    k = -1;
    while((++k < priv_num) && strcmp(priv[k].name, cmdname));

    /* If not found... */
    if(k == priv_num) {
      fprintf(stderr, "Invalid command : %s\n", cmdname);
      return(-1);
    }

    /* Watch out for sub-ioctls ! */
    if(priv[k].cmd < SIOCDEVPRIVATE) {
        int j = -1;

        /* Find the matching *real* ioctl */
        while((++j < priv_num) && ((priv[j].name[0] != '\0') || (priv[j].set_args != priv[k].set_args) ||
              (priv[j].get_args != priv[k].get_args)));

        /* If not found... */
        if (j == priv_num) {
            fprintf(stderr, "Invalid private ioctl definition for : %s\n", cmdname);
            return(-1);
        }
        /* Save sub-ioctl number */
        subcmd = priv[k].cmd;
        /* Reserve one int (simplify alignment issues) */
        offset = sizeof(__u32);
        /* Use real ioctl definition from now on */
        k = j;
    }

    /* If we have to set some data */
    if((priv[k].set_args & IW_PRIV_TYPE_MASK) && (priv[k].set_args & IW_PRIV_SIZE_MASK)) {
        switch(priv[k].set_args & IW_PRIV_TYPE_MASK) {
            case IW_PRIV_TYPE_BYTE:
                /* Number of args to fetch */
                wrq.u.data.length = count;
                if(wrq.u.data.length > (priv[k].set_args & IW_PRIV_SIZE_MASK))
                    wrq.u.data.length = priv[k].set_args & IW_PRIV_SIZE_MASK;

                /* Fetch args */
                for(; i < wrq.u.data.length; i++) {
                    sscanf(args[i], "%i", &temp);
                    buffer[i] = (char) temp;
                }
                break;

            case IW_PRIV_TYPE_INT:
                /* Number of args to fetch */
                wrq.u.data.length = count;
                if(wrq.u.data.length > (priv[k].set_args & IW_PRIV_SIZE_MASK))
                    wrq.u.data.length = priv[k].set_args & IW_PRIV_SIZE_MASK;

                /* Fetch args */
                for(; i < wrq.u.data.length; i++) {
                    sscanf(args[i], "%i", &temp);
                    ((__s32 *) buffer)[i] = (__s32) temp;
                }
                break;

            case IW_PRIV_TYPE_CHAR:
                if(i < count) {
                    /* Size of the string to fetch */
                    wrq.u.data.length = strlen(args[i]) + 1;
                    if(wrq.u.data.length > (priv[k].set_args & IW_PRIV_SIZE_MASK))
                        wrq.u.data.length = priv[k].set_args & IW_PRIV_SIZE_MASK;

                    /* Fetch string */
                    memcpy(buffer, args[i], wrq.u.data.length);
                    buffer[sizeof(buffer) - 1] = '\0';
                    i++;
                } else {
                    wrq.u.data.length = 1;
                    buffer[0] = '\0';
                }
                break;

            case IW_PRIV_TYPE_FLOAT:
                /* Number of args to fetch */
                wrq.u.data.length = count;
                if (wrq.u.data.length > (priv[k].set_args & IW_PRIV_SIZE_MASK))
                    wrq.u.data.length = priv[k].set_args & IW_PRIV_SIZE_MASK;

                /* Fetch args */
                for(; i < wrq.u.data.length; i++) {
                    double      freq;
                    if(sscanf(args[i], "%lg", &(freq)) != 1) {
                        printf("Invalid float [%s]...\n", args[i]);
                        return(-1);
                }
                if(index(args[i], 'G')) freq *= GIGA;
                if(index(args[i], 'M')) freq *= MEGA;
                if(index(args[i], 'k')) freq *= KILO;
                sscanf(args[i], "%i", &temp);
                iw_float2freq(freq, ((struct iw_freq *) buffer) + i);
            }
            break;

            case IW_PRIV_TYPE_ADDR:
                /* Number of args to fetch */
                wrq.u.data.length = count;
                if(wrq.u.data.length > (priv[k].set_args & IW_PRIV_SIZE_MASK))
                    wrq.u.data.length = priv[k].set_args & IW_PRIV_SIZE_MASK;

                 /* Fetch args */
                for(; i < wrq.u.data.length; i++) {
                    if(iw_in_addr(skfd, ifname, args[i], ((struct sockaddr *) buffer) + i) < 0) {
                        printf("Invalid address [%s]...\n", args[i]);
                        return(-1);
                    }
                }
                break;

            default:
                fprintf(stderr, "Not implemented...\n");
                return(-1);
        }

        if ((priv[k].set_args & IW_PRIV_SIZE_FIXED) &&
            (wrq.u.data.length != (priv[k].set_args & IW_PRIV_SIZE_MASK))) {
            printf("The command %s needs exactly %d argument(s)...\n",
                 cmdname, priv[k].set_args & IW_PRIV_SIZE_MASK);
            return(-1);
        }
    } else {
        wrq.u.data.length = 0L;
    }

    strncpy(wrq.ifr_name, ifname, IFNAMSIZ);

    /* Those two tests are important. They define how the driver
     * will have to handle the data */
    if((priv[k].set_args & IW_PRIV_SIZE_FIXED) &&
        ((iw_get_priv_size(priv[k].set_args) + offset) <= IFNAMSIZ)) {
        /* First case : all SET args fit within wrq */
        if(offset)
            wrq.u.mode = subcmd;
        memcpy(wrq.u.name + offset, buffer, IFNAMSIZ - offset);
    } else {
        if((priv[k].set_args == 0) && (priv[k].get_args & IW_PRIV_SIZE_FIXED) &&
           (iw_get_priv_size(priv[k].get_args) <= IFNAMSIZ)) {
          /* Second case : no SET args, GET args fit within wrq */
        if(offset)
            wrq.u.mode = subcmd;
        } else {
            /* Third case : args won't fit in wrq, or variable number of args */
            wrq.u.data.pointer = (caddr_t) buffer;
            wrq.u.data.flags = subcmd;
        }
    }
    /* Perform the private ioctl */
    if(ioctl(skfd, priv[k].cmd, &wrq) < 0) {
      fprintf(stderr, "Interface doesn't accept private ioctl...\n");
      fprintf(stderr, "%s (%X): %s\n", cmdname, priv[k].cmd, strerror(errno));
      return(-1);
    }
    return(0);
}

static inline int iwpriv_set_private(int skfd, char args[][30], int count, char * ifname)
{
    int     ret, iwpriv_cmd_numbers = 0;
    iwprivargs    *iwpriv_cmd_list;

    if (!strcmp(ifname,"wifi0")) {
      iwpriv_cmd_list = wlan_hw_if_privcmd_list;
      iwpriv_cmd_numbers = iwpriv_hw_if_cmd_numbers;
    }
    else {
      iwpriv_cmd_list = wlan_vap_if_privcmd_list;
      iwpriv_cmd_numbers = iwpriv_vap_if_cmd_numbers;
    }
    /* Do it */
    ret = iwpriv_set_private_cmd(skfd, args+1, count-1, ifname, args[0], iwpriv_cmd_list, iwpriv_cmd_numbers);
    return(ret);
}

static int athcfg_wlan_set_iwpriv_acl(const char *cmd, const char *value, int *fd)
{
    int ret, skfd;
    char iwpriv_args[2][30];

    if ((skfd = athcfg_sock_init(__func__)) < 0) {
        fprintf(stderr, "%s: socket error: %d\n", __func__, errno);
        return -1;
    }
    *fd = skfd;
    if (iwpriv_init(skfd, "ath0") != 0)
        return -1;

    strcpy(iwpriv_args[0], cmd);
    strcpy(iwpriv_args[1], value);
    ret = iwpriv_set_private(skfd, iwpriv_args, 2, "ath0");
    iwpriv_deinit();
    if (ret != 0) {
        fprintf(stderr,"%s: Error setting iwpriv var : %s value : %s ap : %s\n",\
                            __func__, iwpriv_args[0], iwpriv_args[1], "ath0");
        return -1;
    }
    return 0;
}

char *ether_sprintf(const u_int8_t *mac)
{
    static char buf[32];
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

/* Is used to set iwprivs to pass MAC address information to driver */
void set_iwpriv_mac(u_int8_t *mac_addr, int sta_count)
{
    char  buffer[50];

    sprintf(buffer, "iwpriv ath0 rptmacaddr1 0x%02x%02x%02x%02x", 
            mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3]);
    system(buffer);
    sprintf(buffer, "iwpriv ath0 rptmacaddr2 0x0000%02x%02x", 
            mac_addr[4], mac_addr[5]);
    system(buffer);
    sprintf(buffer, "iwpriv ath0 rptmacdev %d", sta_count);
    system(buffer);
}

/* Check validity of string MAC address input, strips colons & converts ascii 
   char to integer */
int char2addr(char* addr)
{
    int i, j=2;

    for(i=2; i<17; i+=3) {
        addr[j++] = addr[i+1];
        addr[j++] = addr[i+2];
    }

    for(i=0; i<12; i++) {
        /* check 0~9, A~F */
        addr[i] = ((addr[i]-48) < 10) ? (addr[i] - 48) : (addr[i] - 55);
        /* check a~f */
        if (addr[i] >= 42)
            addr[i] -= 32;

        if (addr[i] > 0xf)
            return -1;
    }
    for(i=0; i<6; i++){
        addr[i] = (addr[(i<<1)] << 4) + addr[(i<<1)+1];
    }
    return 0;
}

/* Used to covert a MAC address in string format to u_int8_t array */
void mac_addr_from_string(char *mac_addr_char, u_int8_t *mac_addr) 
{
    char  tmp_mac_addr[18];
    int   i;

    for (i = 0; i < strlen(mac_addr_char); i++) {
        if (isupper(mac_addr_char[i])) {
            mac_addr_char[i] = tolower(mac_addr_char[i]);
        }
    }

    if (char2addr(mac_addr_char) != 0) {
        printf("The parameter is invalid\n");
        exit(0);
    }
    memcpy(mac_addr, mac_addr_char, IEEE80211_ADDR_LEN);
}

/* Used to check if a custom protocol message has been received & return
   the message number */
int rx_proto_msg()
{
    FILE *fp;
    char  devstr1[4], devstr2[19];
    int   protmsg, err;
    system("iwpriv ath0 getrptrxprotomsg >> RxProtoMsg.txt");
    fp = fopen("RxProtoMsg.txt", "r");
    fscanf(fp,"%s %s",devstr1, devstr2);
    protmsg = (int) devstr2[17];
    protmsg = protmsg - '0'; /* 48 is ascii of 0 */
    fclose(fp);
    err = unlink("RxProtoMsg.txt");
    if (err != 0 && err != ENOENT) {
        printf("%d Error in deleting RxProtoMsg.txt\n", err);
        return err;
    } 
    return protmsg;
}

/* Used to record the received goodput tables into files */
void record_gput(int mode)
{
    u_int32_t sta_count,count;
    struct rptgput_msg *rptgput_data;
    FILE *outFile;
    char file_name_ext[16];
    char  buffer[50];

    if (mode == 0) {
        strcpy(file_name_ext, "RptGputCalc.txt");
    } else {
        strcpy(file_name_ext, "RxRptGput.txt");
    }
    outFile = fopen(file_name_ext, "wt");
    if (outFile == NULL) {
        printf("%s could not be created. Exiting...\n", file_name_ext);
        exit(0);
    }
    rptgput_data = &rptgput_data_buf[0];
    sta_count    = rptgput_data->sta_count;
    fprintf(outFile,"%d\n", sta_count);
    for (count = 0; count < sta_count; count++ ) {
        fprintf(outFile, "%s %d\n", ether_sprintf(rptgput_data->sta_top_map[count].mac_address), rptgput_data->sta_top_map[count].goodput);
    }
    fclose(outFile);
}

/* Used to record the received status & association information into files and
   set the corresponding iwpriv params at the rx */
void record_status_assoc()
{
    u_int32_t sta_count,count;
    struct rptgput_msg *rptgput_data;
    FILE *outFile;
    char file_name_ext[21];
    char  buffer[50];

    strcpy(file_name_ext, "SlaveStatusAssoc.txt");
    outFile = fopen(file_name_ext, "wt");
    if (outFile == NULL) {
        printf("%s could not be created. Exiting...\n", file_name_ext);
        exit(0);
    }

    rptgput_data = &rptgput_data_buf[0];
    sta_count    = rptgput_data->sta_count;
    fprintf(outFile,"%d\n", sta_count);
    fprintf(outFile,"%d\n",((rptgput_data->sta_top_map[0].goodput & 0xff000000) >> 24));
    fprintf(outFile,"%d\n", rptgput_data->sta_top_map[0].goodput & 0x000000ff); 
    fclose(outFile);

    sprintf(buffer, "iwpriv ath0 rptstatus %d", 
            ((rptgput_data->sta_top_map[0].goodput & 0xff000000) >> 24));
    system(buffer);

    sprintf(buffer, "iwpriv ath0 rptassoc %d", 
            rptgput_data->sta_top_map[0].goodput & 0x000000ff);
    system(buffer);

    fclose(outFile);
}

/* Used to record the routing information into files and
   set the corresponding iwpriv params at the rx */
void record_routing_table()
{
    u_int32_t sta_count, count, routing_table[4];
    struct rptgput_msg *rptgput_data;
    FILE *outFile;
    char file_name_ext[16];
    char  buffer[50];
    int err;
 
    strcpy(file_name_ext, "RoutingTable.txt");
    outFile = fopen(file_name_ext, "wt");
    if (outFile == NULL) {
        printf("%s could not be created. Exiting...\n", file_name_ext);
        exit(0);
    }

    rptgput_data = &rptgput_data_buf[0];

    sta_count    = rptgput_data->sta_count;
    for (count = 0; count < sta_count; count++) {
        fprintf(outFile, "%s %d\n", ether_sprintf(rptgput_data->sta_top_map[count].mac_address), 
                      rptgput_data->sta_top_map[count].goodput);
        routing_table[count] = rptgput_data->sta_top_map[count].goodput;
        set_iwpriv_mac(rptgput_data->sta_top_map[count].mac_address, count+1);
    }

    sprintf(buffer, "iwpriv ath0 rptsta1route %d", routing_table[0]);
    system(buffer);
    sprintf(buffer, "iwpriv ath0 rptsta2route %d", routing_table[1]);
    system(buffer);
    sprintf(buffer, "iwpriv ath0 rptsta3route %d", routing_table[2]);
    system(buffer);
    sprintf(buffer, "iwpriv ath0 rptsta4route %d", routing_table[3]);
    system(buffer);

    fclose(outFile);
}

/* Is used to trigger/monitor events that will lead to transmission of 
   driver data over netlink socket */
void rpttool_trigger(int func_num, int ap_rpt)
{
    int i;

    switch (func_num) {

        case 0: /* Record locally generated goodput table */
            if (ap_rpt) { 
                system("iwpriv ath0 rptgputcalc 1");
            } else {
                system("iwpriv ath0 rptgputcalc 0");
            }
            break;
        case 1: /* Receive goodput table */
            while (1) {
                if (rx_proto_msg() == 3) {
                    printf("Received protocol message 3\n");
                    break;
                }
                sleep(1);
            }
            break;
        case 2: /* Receive status & association */
            while (1) {
                i++; 
                if (rx_proto_msg() == 6) {
                    printf("Received protocol message 6\n");
                    break;
                }
                sleep(1);
                if (i == 500) {
                    printf("Reception of status & association timed out\n"); 
                    exit(0);
                } 
            }
            break;
        case 3: /* Receive routing table */
            while (1) {
                i++; 
                if (rx_proto_msg() == 7) {
                    printf("Received protocol message 7\n");
                    break;
                }
                sleep(1);
                if (i == 500) {
                    printf("Receiption of routing table timed out\n"); 
                    exit(0);
                } 
            }
            break;
        default:
            printf("Invalid function number\n");
            exit(0); 
    }
}

/* Is used to call the appropriate record function to receive driver data overdriver
   over netlink socket */ 
void rpttool_get_data(int func_num)
{
    switch(func_num) { 
        case 0:  /* Record locally generated goodput table */
            record_gput(0);
            break; 
        case 1:  /* Record received goodput table */
            record_gput(1);
            break; 
        case 2:  /* Record received status & association */
            record_status_assoc();
            break; 
        case 3:  /* Record received routing table */
            record_routing_table();
            break;
        default:
            break;
    }
}

/* Listen on netlink socket - triggered by main(), in order to receive information
   from driver. Is used to receive custom protocol messages #3, #6 & #9 (goodput 
   table, status/assoc table & routing table from kernel space at the receiver.
   Is also used to receive goodput table from kernel space after training.
   Consists of a trigger function, which is used to trigger/detect activity that will 
   precede data reception from kernel and a record function that is used to record
   the incoming data from the kernel into a file */
int rpttoolListen(int func_num, int ap_rpt, int interface)
{
    struct sockaddr_nl src_addr, dst_addr;
    socklen_t fromlen;
    struct nlmsghdr *nlh = NULL;
    struct iovec iov;
    int sock_fd;
    struct msghdr msg;
    int i = 0, ret_val;
    RPTGPUT_MSG *rptgput_msg;
    fd_set read_set;

    FD_ZERO(&read_set);

    if (interface == 0) {
        sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_RPTGPUT);
    } else {
        sock_fd=socket(PF_NETLINK, SOCK_RAW, NETLINK_RPTGPUT+1);
    }

    if (sock_fd < 0) { 
        fprintf(stderr, "socket errno=%d\n", sock_fd);
        return sock_fd;
    }
    printf("Opened netlink socket from rpttool\n");

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = PF_NETLINK;
    src_addr.nl_pid    = getpid();  
    src_addr.nl_groups = 1;
    ret_val = bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

    if(ret_val < 0) {
        perror("bind(netlink)");
        fprintf(stderr, "BIND errno=%d\n", ret_val);
        return ret_val;
    }
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(sizeof(RPTGPUT_MSG)));
    if (nlh == NULL) {
        close(sock_fd);
        printf("Memory allock for nlh in rpttoolListen failed\n");
        return -1;
    } 

    nlh->nlmsg_len   = NLMSG_SPACE(sizeof(RPTGPUT_MSG));
    nlh->nlmsg_pid   = getpid();
    nlh->nlmsg_flags = 0;
    
    iov.iov_base = (void *)nlh;
    iov.iov_len  = nlh->nlmsg_len;
    
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.nl_family = PF_NETLINK;
    dst_addr.nl_pid    = 0;  
    dst_addr.nl_groups = 1;
    
    memset(&msg, 0, sizeof(msg));
    msg.msg_name    = (void *)&dst_addr;
    msg.msg_namelen = sizeof(dst_addr);
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;
    rptgput_data_buf = (struct rptgput_msg*)malloc(sizeof(struct rptgput_msg)); 
 
    if(rptgput_data_buf == NULL) {
        close(sock_fd);
        free(nlh);
        nlh = NULL;
        printf("Could not allocate buffer for repeater placement data.\n");
        return -1;
    }
    rpttool_trigger(func_num, ap_rpt);
    FD_SET(sock_fd, &read_set);
    select((sock_fd + 1), &read_set, NULL, NULL, NULL);
    if (FD_ISSET(sock_fd, &read_set)) {
        fromlen = sizeof(src_addr);
        memset(nlh, 0, NLMSG_SPACE(sizeof(RPTGPUT_MSG)));
        memset(&msg, 0, sizeof(msg));
        msg.msg_name = (void *)&dst_addr;
        msg.msg_namelen = sizeof(dst_addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        ret_val=recvmsg(sock_fd, &msg, MSG_WAITALL);
        rptgput_msg = (RPTGPUT_MSG*)NLMSG_DATA(nlh);
        memcpy(rptgput_data_buf, rptgput_msg, sizeof(RPTGPUT_MSG));
    } else {
        printf("Stuck after fdset in rpttool!!\n");
    }
    close(sock_fd);
    rpttool_get_data(func_num);
    free(rptgput_data_buf);
    free(nlh); 
    rptgput_data_buf = NULL;
    nlh = NULL;
    return 0;
}

/* Used to set the MAC addresses of Root AP, repeater & STAs. Takes
   in text input from files and uses iwprivs to feed this info to
   the driver */
void get_set_addr(int val)
{
    FILE *fp;
    char file_name_ext[16]; 
    char macaddrstr[18];
    u_int8_t mac_addr[6]; 
    int  count, sta_count; 

    switch(val) {
        case 1:
            strcpy(file_name_ext, "RootAPAddr");
            break;  
        case 2:
            strcpy(file_name_ext, "RptAddr");
            break;  
        case 3:
            strcpy(file_name_ext, "StaAddr");
            break;  
        default:
            printf("Invalid option in get_set_addr\n");
            return;
    } 

    fp = fopen(file_name_ext, "r");
    if (fp == NULL){
       printf("File %s does not exist. Exiting....\n", file_name_ext);
       exit(0); 
    } else {
        switch(val) {
            case 1:
                fscanf(fp,"%s",macaddrstr);
                mac_addr_from_string(macaddrstr, mac_addr);
                set_iwpriv_mac(mac_addr, 5); 
                break;  
            case 2:
                fscanf(fp,"%s",macaddrstr);
                mac_addr_from_string(macaddrstr, mac_addr);
                set_iwpriv_mac(mac_addr, 6); 
                break;  
            case 3:
                fscanf(fp,"%d", &sta_count);
                if (sta_count > 4) {
                   printf("STA count cannot exceed 4. Exiting..\n");
                   exit(0);
                }
                for (count = 0; count <  sta_count; count++) {
                    fscanf(fp,"%s", macaddrstr);
                    mac_addr_from_string(macaddrstr, mac_addr);
                    set_iwpriv_mac(mac_addr, count+1);  
                } 
                break;  
            default:
                printf("Invalid option in get_set_addr\n");
                break;
        }
    }
}

void block_nodes(int mode)
{
    FILE *fp;
    int numstas, status, assoc, sta_count, i, retval;
    char macaddr[17]; 
    int fd = -1;

    if (mode == 0) {
        fp = fopen("MasterStatusAssoc.txt", "rt"); 
    } else {
        fp = fopen("SlaveStatusAssoc.txt", "rt"); 
    }
    if (fp == NULL) {
       printf("Training Status & Association information not available. Exiting ....\n");
       exit(0); 
    }
    fscanf(fp,"%d", &numstas);
    fscanf(fp,"%d", &status);
    fscanf(fp,"%d", &assoc);
    fclose(fp);
    fp = fopen("StaAddr", "rt");
    if (fp == NULL) {
       printf("STA information not available. Exiting ....\n");
       exit(0); 
    }
    fscanf(fp,"%d", &sta_count);
    if (sta_count != numstas) { 
        printf("STA count mismatch. Exiting...\n");
    }
    printf("Status = %d, Assoc = %d\n", status, assoc);
    for (i = 0; i < sta_count; i++) {
        fscanf(fp,"%s", macaddr); 
        if ((((assoc & (1 << i)) == 0) && ((assoc & (1 << (i+4))) != 0) && (mode == 0)) ||
            (((assoc & (1 << i)) != 0) && ((assoc & (1 << (i+4))) == 0) && (mode == 1))) {
            retval = athcfg_wlan_set_iwpriv_acl("addmac", macaddr, &fd);
            if (fd != -1) {
                close(fd);
            }
            if (retval != 0) {
                printf("Node %s could not be added to ACL block list!! Moving to next node ...\n");
                continue;
            } 
            printf("Node %s is added to ACL block list\n", macaddr);
            system("iwpriv ath0 maccmd 2"); // Set deny policy
            retval = athcfg_wlan_set_iwpriv_acl("kickmac", macaddr, &fd);
            if (fd != -1) {
                close(fd);
            }
            printf("Node %s is kicked out\n", macaddr);
        } else {
            if (status & (1 << i)) {
                printf("Node %s is not added to ACL block list\n", macaddr);
            } else {
                printf("Connection to Node %s failed or does not meet requirements\n", macaddr); 
            }
        }    
    }
    fclose(fp);
    system("iwpriv ath0 maccmd 2"); // Set deny policy
}

/* Displays usage of rpttool - lists supported parameters and actions */
static void usage(void)
{
    fprintf(stderr, "Usage: rpttool [cmd]\n"
            "    rpttool rootap_addr          -- Set root AP's MAC address\n"
            "    rpttool rpt_addr             -- Set RPT's MAC address\n"
            "    rpttool sta_addr             -- Set STA MAC addresses\n"
            "    rpttool rec_gput <mode> <if> -- Record goodput : 0-AP 1-RPT\n"
            "    rpttool rec_msg  <msg> <if>  -- Store received information\n"
            "    rpttool node_blk <mode>      -- Block STA MAC address : 0-at AP, 1-at Rpt\n"
            "    rpttool master   <mode> <if> -- 0 - Slave Rpt Placement \n"
            "                                    1 - Master Rpt Placement\n"
            "                                    2 - Slave Pkt Forwarding\n"
            "                                    3 - Master Pkt Forwarding\n"
            "    rpttool -h                   -- This message\n"
           );
}

extern int mode, master, interface;

/* Main function with command line parameters - Validates parameters & calls
   necessary functions to support the parameters*/
int main(int argc, char *argv[])
{
    int func_num;

    if (argc >= 2) {
        if (streq(argv[1], "rootap_addr")) {
            get_set_addr(1);
        } else if (streq(argv[1], "rpt_addr")) {
            get_set_addr(2); 
        } else if (streq(argv[1], "sta_addr")) {
            get_set_addr(3);
        } else if(streq(argv[1], "rec_msg")) {
            if (argc != 4) {
                printf("Missing argument : msg \n");
                usage();
                exit(0); 
            }
            if (atoi(argv[2]) == 3)
                func_num = 1;  
            else if (atoi(argv[2]) == 6)
                func_num = 2;
            else if (atoi(argv[2]) == 7) 
                func_num = 3;
            else {       
                printf("Invalid func: msg \n");
                exit(0);             
            } 
            rpttoolListen(func_num, 0, atoi(argv[3])); 
        } else if (streq(argv[1], "rec_gput")) {
            if (argc != 4) {
                printf("Missing argument : mode \n");
                usage();
                exit(0); 
            }
            rpttoolListen(0,atoi(argv[2]), atoi(argv[3])); 
        } else if (streq(argv[1], "node_blk")) {
            if (argc != 3) {
                printf("Missing argument : mode \n");
                usage();
                exit(0); 
            }
            block_nodes(atoi(argv[2]));
        } else if (streq(argv[1], "master")) {
            master = atoi(argv[2]);
            if (master < 0 || master > 4 ) {
                printf("Invalid mode. Exiting...\n");
                usage();
            }
            mode = master >> 1;
            interface = atoi(argv[3]); 
            init_training(); 
            if (master%2 == 1) {
                master_sm();
            } else {
                slave_sm();
            } 
        } else if (streq(argv[1],"-h")) {
            usage();
        } else { 
            usage();
        }
    } else if ((argc == 1) || (argc > 3)) {
        usage ();
    }
    return 0;
}
