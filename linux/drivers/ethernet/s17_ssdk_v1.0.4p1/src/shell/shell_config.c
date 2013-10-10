#include "shell_config.h"
#include "shell_sw.h"


/*cmdline tree descript*/
struct cmd_des_t gcmd_des[] = {
    /*port ctrl*/
    #ifdef IN_PORTCONTROL
    {"port", "config port control",
            {
             {"duplex", "get", "get duplex mode of a port", "<port_id>" , SW_API_PT_DUPLEX_GET, NULL}, 
             {"duplex", "set", "set duplex mode of a port", "<port_id> <half|full>", SW_API_PT_DUPLEX_SET, NULL},
             {"speed", "get", "get speed mode of a port", "<port_id>",  SW_API_PT_SPEED_GET, NULL},
             {"speed", "set", "set speed mode of a port", "<port_id> <10|100|1000>", SW_API_PT_SPEED_SET, NULL},
             {"autoAdv", "get", "get auto-negotiation advertisement of a port", "<port_id>", SW_API_PT_AN_ADV_GET, NULL},
             {"autoAdv", "set", "set auto-negotiation advertisement of a port", "<port_id> <cap_bitmap>", SW_API_PT_AN_ADV_SET, NULL},             
             {"autoNeg", "get", "get auto-negotiation status of a port", "<port_id>", SW_API_PT_AN_GET, NULL},
             {"autoNeg", "enable", "enable auto-negotiation of a port", "<port_id>", SW_API_PT_AN_ENABLE, NULL},
             {"autoNeg", "restart", "restart auto-negotiation process of a port", "<port_id>", SW_API_PT_AN_RESTART, NULL},
             {"header", "set", "set atheros header/tag status of a port", "<port_id> <enable|disable>", SW_API_PT_HDR_SET, NULL},
             {"header", "get", "get atheros header/tag status of a port", "<port_id>", SW_API_PT_HDR_GET, NULL},
             {"txhdr", "set", "set tx frame atheros header/tag status of a port", "<port_id> <noheader|onlymanagement|allframe>", SW_API_PT_TXHDR_SET, NULL},
             {"txhdr", "get", "get tx frame atheros header/tag status of a port", "<port_id>", SW_API_PT_TXHDR_GET, NULL},
             {"rxhdr", "set", "set rx frame atheros header/tag status of a port", "<port_id> <noheader|onlymanagement|allframe>", SW_API_PT_RXHDR_SET, NULL},
             {"rxhdr", "get", "get rx frame atheros header/tag status of a port", "<port_id>", SW_API_PT_RXHDR_GET, NULL},
             {"hdrtype", "set", "set atheros header/tag type", "<enable|disable> <type 0x-0xffff>", SW_API_HEADER_TYPE_SET, NULL},
             {"hdrtype", "get", "get atheros header/tag type", "", SW_API_HEADER_TYPE_GET, NULL},
             {"flowCtrl", "set", "set flow control status of a port", "<port_id> <enable|disable>", SW_API_PT_FLOWCTRL_SET, NULL},
             {"flowCtrl", "get", "get flow control status of a port", "<port_id>", SW_API_PT_FLOWCTRL_GET, NULL},
             {"flowCtrlmode", "set", "set flow control force mode of a port", "<port_id> <enable|disable>", SW_API_PT_FLOWCTRL_MODE_SET, NULL},
             {"flowCtrlmode", "get", "get flow control force mode of a port", "<port_id>", SW_API_PT_FLOWCTRL_MODE_GET, NULL},
             {"powersave", "set", "set powersave status of a port", "<port_id> <enable|disable>", SW_API_PT_POWERSAVE_SET, NULL},
             {"powersave", "get", "get powersave status of a port", "<port_id>", SW_API_PT_POWERSAVE_GET, NULL},
             {"hibernate", "set", "set hibernate status of a port", "<port_id> <enable|disable>", SW_API_PT_HIBERNATE_SET, NULL},
             {"hibernate", "get", "get hibernate status of a port", "<port_id>", SW_API_PT_HIBERNATE_GET, NULL},
             {"cdt", "run", "run cable diagnostic test of a port", "<port_id> <mdi_pair>", SW_API_PT_CDT, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL},/*end of desc*/
            },
    },
    #endif

    /*vlan*/
    #ifdef IN_VLAN
    {"vlan", "config VLAN table",
            { 
             {"entry", "create", "create a VLAN entry", "<vlan_id>", SW_API_VLAN_ADD, NULL}, 
             {"entry", "del", "delete a VLAN entryn", "<vlan_id>", SW_API_VLAN_DEL, NULL},
             {"entry", "update", "update port member of a VLAN entry", "<vlan_id> <member_bitmap> <0>", SW_API_VLAN_MEM_UPDATE, NULL}, 
             {"entry", "find", "find a VLAN entry by VLAN id", "<vlan_id>", SW_API_VLAN_FIND, NULL},
             {"entry", "next", "find next VLAN entry by VLAN id", "<vlan_id>",SW_API_VLAN_NEXT, NULL}, 
             {"entry", "append", "append a VLAN entry", "", SW_API_VLAN_APPEND, NULL},
             {"entry", "flush", "flush all VLAN entries", "",SW_API_VLAN_FLUSH, NULL},
             {"entry", "show", "show whole VLAN entries", "", SW_CMD_VLAN_SHOW, cmd_show_vlan},
             {"fid", "set", "set VLAN entry fid", "<vlan_id> <fid>",SW_API_VLAN_FID_SET, NULL},
             {"fid", "get", "get VLAN entry fid", "<vlan_id>",SW_API_VLAN_FID_GET, NULL},
             {"member", "add", "add VLAN entry member", "<vlan_id> <port_id> <unmodified|untagged|tagged>",SW_API_VLAN_MEMBER_ADD, NULL},
             {"member", "del", "del VLAN entry member", "<vlan_id> <port_id>",SW_API_VLAN_MEMBER_DEL, NULL},
             {"learnsts", "set", "set VLAN entry learn status", "<vlan_id> <enable|disable>",SW_API_VLAN_LEARN_STATE_SET, NULL},
             {"learnsts", "get", "get VLAN entry learn status", "<vlan_id>",SW_API_VLAN_LEARN_STATE_GET, NULL},             
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            },
    },
    #endif

    /*portvlan*/
    #ifdef IN_PORTVLAN
    {"portVlan", "config port base VLAN", 
            { 
             {"ingress", "get", "get ingress VLAN mode of a port", "<port_id>", SW_API_PT_ING_MODE_GET, NULL}, 
             {"ingress", "set", "set ingress VLAN mode of a port", "<port_id> <disable|secure|check|fallback>", SW_API_PT_ING_MODE_SET, NULL},
             {"egress", "get", "get egress VLAN mode of a port", "<port_id>", SW_API_PT_EG_MODE_GET, NULL}, 
             {"egress", "set", "set egress VLAN mode of a port", "<port_id> <unmodified|untagged|tagged|hybrid|untouched>", SW_API_PT_EG_MODE_SET, NULL},
             {"member", "add", "add a member to the port based VLAN of a port", "<port_id> <memport_id>", SW_API_PT_VLAN_MEM_ADD, NULL}, 
             {"member", "del", "delete a member from the port based VLAN of a port", "<port_id> <memport_id>", SW_API_PT_VLAN_MEM_DEL, NULL},
             {"member", "update", "update members of the port based VLAN of a port", "<port_id> <port_bitmap>", SW_API_PT_VLAN_MEM_UPDATE, NULL},
             {"member", "get", "get members of the port based VLAN of a port", "<port_id>", SW_API_PT_VLAN_MEM_GET, NULL},
             {"defaultVid", "get", "get default VLAN id of a port", "<port_id>", SW_API_PT_DEF_VID_GET, NULL},
             {"defaultVid", "set", "set default VLAN id of a port", "<port_id> <vid>", SW_API_PT_DEF_VID_SET, NULL},
             {"forceVid", "set", "set VLAN id enforcement status of a port", "<port_id> <enable|disable>", SW_API_PT_FORCE_DEF_VID_SET, NULL},
             {"forceVid", "get", "get VLAN id enforcement status of a port", "<port_id>", SW_API_PT_FORCE_DEF_VID_GET, NULL},        
             {"forceMode", "set", "set port based VLAN enforcement status of a port", "<port_id> <enable|disable>", SW_API_PT_FORCE_PORTVLAN_SET, NULL},
             {"forceMode", "get", "get port based VLAN enforcement status of a port", "<port_id>", SW_API_PT_FORCE_PORTVLAN_GET, NULL}, 
             {"nestVlan", "set", "set nest VLAN status of a port", "<port_id> <enable|disable>", SW_API_PT_NESTVLAN_SET, NULL},
             {"nestVlan", "get", "get nest VLAN status of a port", "<port_id>", SW_API_PT_NESTVLAN_GET, NULL}, 
             {"sVlanTPID", "set", "set service VLAN tpid", "<tpid>", SW_API_NESTVLAN_TPID_SET, NULL},
             {"sVlanTPID", "get", "get service VLAN tpid", "", SW_API_NESTVLAN_TPID_GET, NULL}, 
             /*shiva*/
             {"invlan", "set", "set port invlan mode", "<port_id> <admit_all|admit_tagged|admit_untagged>", SW_API_PT_IN_VLAN_MODE_SET, NULL}, 
             {"invlan", "get", "get port invlan mode", "<port_id>", SW_API_PT_IN_VLAN_MODE_GET, NULL}, 
             {"tlsMode", "set", "set TLS mode", "<port_id> <enable|disable>", SW_API_PT_TLS_SET, NULL}, 
             {"tlsMode", "get", "get TLS mode", "<port_id>", SW_API_PT_TLS_GET, NULL}, 
             {"priPropagation", "set", "set priority propagation", "<port_id> <enable|disable>", SW_API_PT_PRI_PROPAGATION_SET, NULL}, 
             {"priPropagation", "get", "get priority propagation", "<port_id>", SW_API_PT_PRI_PROPAGATION_GET, NULL},              
             {"defaultSVid", "set", "set default SVID", "<port_id> <vlan_id>", SW_API_PT_DEF_SVID_SET, NULL}, 
             {"defaultSVid", "get", "get default SVID", "<port_id>", SW_API_PT_DEF_SVID_GET, NULL}, 
             {"defaultCVid", "set", "set default CVID", "<port_id> <vlan_id>", SW_API_PT_DEF_CVID_SET, NULL}, 
             {"defaultCVid", "get", "get default CVID", "<port_id>", SW_API_PT_DEF_CVID_GET, NULL}, 
             {"vlanPropagation", "set", "set vlan propagation", "<port_id> <disable|clone|replace>", SW_API_PT_VLAN_PROPAGATION_SET, NULL}, 
             {"vlanPropagation", "get", "get vlan propagation", "<port_id>", SW_API_PT_VLAN_PROPAGATION_GET, NULL}, 
             {"translation", "add", "add vlan translation", "<port_id>", SW_API_PT_VLAN_TRANS_ADD, NULL}, 
             {"translation", "del", "del vlan translation", "<port_id>", SW_API_PT_VLAN_TRANS_DEL, NULL}, 
             {"translation", "get", "get vlan translation", "<port_id>", SW_API_PT_VLAN_TRANS_GET, NULL},
             {"translation", "iterate", "iterate vlan translation tables", "<port_id> <iterator>", SW_API_PT_VLAN_TRANS_ITERATE, NULL}, 
             {"qinqMode", "set", "set qinq mode", "<ctag|stag>", SW_API_QINQ_MODE_SET, NULL},              
             {"qinqMode", "get", "get qinq mode", "", SW_API_QINQ_MODE_GET, NULL},  
             {"qinqRole", "set", "set qinq role", "<port_id> <edge|core>", SW_API_PT_QINQ_ROLE_SET, NULL},              
             {"qinqRole", "get", "get qinq role", "<port_id>", SW_API_PT_QINQ_ROLE_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
             },
    },
    #endif

    /*fdb*/
    #ifdef IN_FDB
    {"fdb", "config FDB table", 
            { 
             {"entry", "add", "add a FDB entry", "", SW_API_FDB_ADD, NULL}, 
             {"entry", "del", "delete a FDB entry", "", SW_API_FDB_DELMAC, NULL},
             {"entry", "flush", "flush all FDB entries", "<0:dynamic only|1:dynamic and static>", SW_API_FDB_DELALL, NULL},
             {"entry", "show", "show whole FDB entries", "", SW_CMD_FDB_SHOW, cmd_show_fdb},             
             {"entry", "find", "find a FDB entry", "", SW_API_FDB_FIND, NULL},
             {"entry", "iterate", "iterate all FDB entries", "<iterator>", SW_API_FDB_ITERATE, NULL},
             {"entry", "extendnext", "find next FDB entry in extend mode", "", SW_API_FDB_EXTEND_NEXT, NULL},
             {"entry", "extendfirst", "find first FDB entry in extend mode", "", SW_API_FDB_EXTEND_FIRST, NULL},
             {"entry", "transfer", "transfer port info in FDB entry", "<old port_id> <new port_id> <fid>", SW_API_FDB_TRANSFER, NULL},
             {"portEntry", "flush", "flush all FDB entries by a port", "<port_id> <0:dynamic only|1:dynamic and static>", SW_API_FDB_DELPORT, NULL},
             {"firstEntry", "find", "find the first FDB entry", "", SW_API_FDB_FIRST, NULL},
             {"nextEntry", "find", "find next FDB entry", "", SW_API_FDB_NEXT, NULL},  
             {"portLearn", "set", "set FDB entry learning status of a port", "<port_id> <enable|disable>", SW_API_FDB_PT_LEARN_SET, NULL},
             {"portLearn", "get", "get FDB entry learning status of a port", "<port_id>", SW_API_FDB_PT_LEARN_GET, NULL},
             {"ageCtrl", "set", "set FDB entry aging status", "<enable|disable>", SW_API_FDB_AGE_CTRL_SET, NULL},
             {"ageCtrl", "get", "get FDB entry aging status", "", SW_API_FDB_AGE_CTRL_GET, NULL},             
             {"ageTime", "set", "set FDB entry aging time", "<time:s>", SW_API_FDB_AGE_TIME_SET, NULL},
             {"ageTime", "get", "get FDB entry aging time", "", SW_API_FDB_AGE_TIME_GET, NULL},
             {"ptlearnlimit", "set", "set port FDB entry learn limit", "<port_id> <enable|disable> <limitcounter>", SW_API_PT_FDB_LEARN_LIMIT_SET, NULL},
             {"ptlearnlimit", "get", "get port FDB entry learn limit", "<port_id>", SW_API_PT_FDB_LEARN_LIMIT_GET, NULL},
             {"ptlearnexceedcmd", "set", "set port forwarding cmd when exceed learn limit", "<port_id> <forward|drop|cpycpu|rdtcpu>", SW_API_PT_FDB_LEARN_EXCEED_CMD_SET, NULL},
             {"ptlearnexceedcmd", "get", "get port forwarding cmd when exceed learn limit", "<port_id>", SW_API_PT_FDB_LEARN_EXCEED_CMD_GET, NULL},
             {"learnlimit", "set", "set FDB entry learn limit", "<enable|disable> <limitcounter>", SW_API_FDB_LEARN_LIMIT_SET, NULL},
             {"learnlimit", "get", "get FDB entry learn limit", "", SW_API_FDB_LEARN_LIMIT_GET, NULL},
             {"learnexceedcmd", "set", "set forwarding cmd when exceed learn limit", "<forward|drop|cpycpu|rdtcpu>", SW_API_FDB_LEARN_EXCEED_CMD_SET, NULL},
             {"learnexceedcmd", "get", "get forwarding cmd when exceed learn limit", "", SW_API_FDB_LEARN_EXCEED_CMD_GET, NULL},
             {"resventry", "add", "add a reserve FDB entry", "", SW_API_FDB_RESV_ADD, NULL}, 
             {"resventry", "del", "delete reserve a FDB entry", "", SW_API_FDB_RESV_DEL, NULL},
             {"resventry", "find", "find a reserve FDB entry", "", SW_API_FDB_RESV_FIND, NULL},
             {"resventry", "iterate", "iterate all reserve FDB entries", "<iterator>", SW_API_FDB_RESV_ITERATE, NULL},
             {"resventry", "show", "show whole resv FDB entries", "", SW_CMD_RESV_FDB_SHOW, cmd_show_resv_fdb},
             {"ptLearnstatic", "set", "set FDB entry learning static status of a port", "<port_id> <enable|disable>", SW_API_FDB_PT_LEARN_STATIC_SET, NULL},
             {"ptLearnStatic", "get", "get FDB entry learning static status of a port", "<port_id>", SW_API_FDB_PT_LEARN_STATIC_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            }, 
    },
    #endif

    /*acl*/
    #ifdef IN_ACL
    {"acl", "config ACL", 
            { 
             {"list", "create", "create an ACL list", "<list_id> <priority>", SW_API_ACL_LIST_CREAT, NULL}, 
             {"list", "destroy", "destroy an ACL list", "<list_id>", SW_API_ACL_LIST_DESTROY, NULL},
             {"list", "bind", "bind an ACL list to a port", "<list_id> <0-0:direction> <0-0:objtype> <objindex>", SW_API_ACL_LIST_BIND, NULL},
             {"list", "unbind", "unbind an ACL list from a port", "<list_id> <0-0:direction> <0-0:objtype> <objindex>", SW_API_ACL_LIST_UNBIND, NULL},
             {"rule", "add", "add ACL rules to an ACL list", "<list_id> <rule_id> <rule_nr>", SW_API_ACL_RULE_ADD, NULL}, 
             {"rule", "del", "delete ACL rules from an ACL list", "<list_id> <rule_id> <rule_nr>", SW_API_ACL_RULE_DELETE, NULL},
             {"rule", "query", "query a ACL rule", "<list_id> <rule_id>", SW_API_ACL_RULE_QUERY, NULL}, 
             {"status", "set", "set status of ACL engine", "<enable|disable>", SW_API_ACL_STATUS_SET, NULL}, 
             {"status", "get", "get status of ACL engine", "", SW_API_ACL_STATUS_GET, NULL},
             {"udfprofile", "set", "set port udf profile", "<port_id> <l2/l2snap/l3/l3plus/l4> <offset> <length>", SW_API_ACL_PT_UDF_PROFILE_SET, NULL}, 
             {"udfprofile", "get", "get port udf profile", "<port_id> <l2/l2snap/l3/l3plus/l4>", SW_API_ACL_PT_UDF_PROFILE_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            },
    },
    #endif

    /*qos*/
    #ifdef IN_QOS
    {"qos", "config Qos", 
            { 
             {"schMode", "set", "set traffic scheduling mode", "<sp|wrr|mix|mix_plus> <q0,q1,q3,q4>", SW_API_QOS_SCH_MODE_SET, NULL}, 
             {"schMode", "get", "get traffic scheduling mode", "", SW_API_QOS_SCH_MODE_GET, NULL},
             {"qTxBufSts", "set", "set queue tx buffer counting status of a port", "<port_id> <enable|disable>", SW_API_QOS_QU_TX_BUF_ST_SET, NULL}, 
             {"qTxBufSts", "get", "get queue tx buffer counting status of a port", "<port_id>", SW_API_QOS_QU_TX_BUF_ST_GET, NULL},
             {"qTxBufNr", "set", "set queue tx buffer number", "<port_id> <queueid:0-3> <number:0-60>", SW_API_QOS_QU_TX_BUF_NR_SET, NULL}, 
             {"qTxBufNr", "get", "get queue tx buffer number", "<port_id> <queueid:0-3>", SW_API_QOS_QU_TX_BUF_NR_GET, NULL},             
             {"ptTxBufSts", "set", "set port tx buffer counting status of a port", "<port_id> <enable|disable>", SW_API_QOS_PT_TX_BUF_ST_SET, NULL}, 
             {"ptTxBufSts", "get", "get port tx buffer counting status of a port", "<port_id>", SW_API_QOS_PT_TX_BUF_ST_GET, NULL},
             {"ptTxBufNr", "set", "set port tx buffer number", "<port_id> <number:0-252>", SW_API_QOS_PT_TX_BUF_NR_SET, NULL},
             {"ptTxBufNr", "get", "get port tx buffer number", "<port_id>", SW_API_QOS_PT_TX_BUF_NR_GET, NULL},
             {"ptRxBufNr", "set", "set port rx buffer number", "<port_id> <number:0-60>", SW_API_QOS_PT_RX_BUF_NR_SET, NULL}, 
             {"ptRxBufNr", "get", "get port rx buffer number", "<port_id>", SW_API_QOS_PT_RX_BUF_NR_GET, NULL},
             {"up2q", "set", "set user priority to queue mapping", "<up:0-7> <queueid:0-3>", SW_API_COSMAP_UP_QU_SET, NULL}, 
             {"up2q", "get", "get user priority to queue mapping", "<up:0-7>", SW_API_COSMAP_UP_QU_GET, NULL}, 
             {"dscp2q", "set", "set dscp to queue mapping", "<dscp:0-63> <queueid:0-3>", SW_API_COSMAP_DSCP_QU_SET, NULL}, 
             {"dscp2q", "get", "get dscp to queue mapping", "<dscp:0-63>", SW_API_COSMAP_DSCP_QU_GET, NULL},
             {"ptMode", "set", "set Qos mode of a port", "<port_id> <da|up|dscp|port> <enable|disable>", SW_API_QOS_PT_MODE_SET, NULL}, 
             {"ptMode", "get", "get Qos mode of a port", "<port_id> <da|up|dscp|port>", SW_API_QOS_PT_MODE_GET, NULL},             
             {"ptModePri", "set", "set the priority of Qos modes of a port", "<port_id> <da|up|dscp|port> <priority:0-3>", SW_API_QOS_PT_MODE_PRI_SET, NULL}, 
             {"ptModePri", "get", "get the priority of Qos modes of a port", "<port_id> <da|up|dscp|port>", SW_API_QOS_PT_MODE_PRI_GET, NULL},  
             {"ptDefaultUp", "set", "set default user priority for received frames of a port", "<port_id> <up:0-7>", SW_API_QOS_PORT_DEF_UP_SET, NULL}, 
             {"ptDefaultUp", "get", "get default user priority for received frames of a port", "<port_id>", SW_API_QOS_PORT_DEF_UP_GET, NULL},
             {"ptschMode", "set", "set port traffic scheduling mode", "<port_id> <sp|wrr|mix|mixplus> <q0,q1,q2,q3,q4,q5>", SW_API_QOS_PORT_SCH_MODE_SET, NULL}, 
             {"ptschMode", "get", "get port traffic scheduling mode", "<port_id>", SW_API_QOS_PORT_SCH_MODE_GET, NULL},
             {"ptDefaultSpri", "set", "set default stag priority for received frames of a port", "<port_id> <spri:0-7>", SW_API_QOS_PT_DEF_SPRI_SET, NULL}, 
             {"ptDefaultSpri", "get", "get default stag priority for received frames of a port", "<port_id>", SW_API_QOS_PT_DEF_SPRI_GET, NULL},
             {"ptDefaultCpri", "set", "set default ctag priority for received frames of a port", "<port_id> <cpri:0-7>", SW_API_QOS_PT_DEF_CPRI_SET, NULL}, 
             {"ptDefaultCpri", "get", "get default ctag priority for received frames of a port", "<port_id>", SW_API_QOS_PT_DEF_CPRI_GET, NULL},
             {"ptQuRemark", "set", "set egress queue based remark", "<port_id> <queue_id> <table_id> <enable|disable>", SW_API_QOS_QUEUE_REMARK_SET, NULL}, 
             {"ptQuRemark", "get", "get egress queue based remark", "<port_id> <queue_id>", SW_API_QOS_QUEUE_REMARK_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            }, 
    },
    #endif

    /*igmp*/
    #ifdef IN_IGMP
    {"igmp", "config IGMP/MLD", 
            { 
             {"mode", "set", "set IGMP/MLD snooping status of a port", "<port_id> <enable|disable>", SW_API_PT_IGMPS_MODE_SET, NULL},
             {"mode", "get", "get port IGMP/MLD snooping status", "<port_id>", SW_API_PT_IGMPS_MODE_GET, NULL},     
             {"cmd", "set", "set IGMP/MLD frames forwarding command", "<forward|drop|cpycpu|rdtcpu>", SW_API_IGMP_MLD_CMD_SET, NULL}, 
             {"cmd", "get", "get IGMP/MLD frames forwarding command", "", SW_API_IGMP_MLD_CMD_GET, NULL},  
             {"portJoin", "set", "set IGMP/MLD hardware joining status", "<port_id> <enable|disable>", SW_API_IGMP_PT_JOIN_SET, NULL}, 
             {"portJoin", "get", "get IGMP/MLD hardware joining status", "<port_id>", SW_API_IGMP_PT_JOIN_GET, NULL},
             {"portLeave", "set", "set IGMP/MLD hardware leaving status", "<port_id> <enable|disable>", SW_API_IGMP_PT_LEAVE_SET, NULL}, 
             {"portLeave", "get", "get IGMP/MLD hardware leaving status", "<port_id>", SW_API_IGMP_PT_LEAVE_GET, NULL},
             {"rp", "set", "set IGMP/MLD router ports", "<port_bit_map>", SW_API_IGMP_RP_SET, NULL}, 
             {"rp", "get", "get IGMP/MLD router ports", "", SW_API_IGMP_RP_GET, NULL}, 
             {"createStatus", "set", "set IGMP/MLD ability for creating entry", "<enable|disable>", SW_API_IGMP_ENTRY_CREAT_SET, NULL}, 
             {"createStatus", "get", "get IGMP/MLD ability for creating entry", "", SW_API_IGMP_ENTRY_CREAT_GET, NULL},
             {"static", "set", "set IGMP/MLD static status for creating entry", "<enable|disable>", SW_API_IGMP_ENTRY_STATIC_SET, NULL}, 
             {"static", "get", "get IGMP/MLD static status for creating entry", "", SW_API_IGMP_ENTRY_STATIC_GET, NULL},
             {"leaky", "set", "set IGMP/MLD leaky status for creating entry", "<enable|disable>", SW_API_IGMP_ENTRY_LEAKY_SET, NULL}, 
             {"leaky", "get", "get IGMP/MLD leaky status for creating entry", "", SW_API_IGMP_ENTRY_LEAKY_GET, NULL},
             {"version3", "set", "set IGMP v3/MLD v2 status for creating entry", "<enable|disable>", SW_API_IGMP_ENTRY_V3_SET, NULL}, 
             {"version3", "get", "get IGMP v3/MLD v2 status for creating entry", "", SW_API_IGMP_ENTRY_V3_GET, NULL},
             {"queue", "set", "set IGMP/MLD queue status for creating entry", "<enable|disable> <queue_id>", SW_API_IGMP_ENTRY_QUEUE_SET, NULL}, 
             {"queue", "get", "get IGMP/MLD queue status for creating entry", "", SW_API_IGMP_ENTRY_QUEUE_GET, NULL},
             {"ptlearnlimit", "set", "set port Multicast entry learn limit", "<port_id> <enable|disable> <limitcounter>", SW_API_PT_IGMP_LEARN_LIMIT_SET, NULL},
             {"ptlearnlimit", "get", "get port Multicast entry learn limit", "<port_id>", SW_API_PT_IGMP_LEARN_LIMIT_GET, NULL},
             {"ptlearnexceedcmd", "set", "set port forwarding cmd when exceed multicast learn limit", "<port_id> <forward|drop|cpycpu|rdtcpu>", SW_API_PT_IGMP_LEARN_EXCEED_CMD_SET, NULL},
             {"ptlearnexceedcmd", "get", "get port forwarding cmd when exceed multicast learn limit", "<port_id>", SW_API_PT_IGMP_LEARN_EXCEED_CMD_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            },
    },
    #endif

    /*leaky*/
    #ifdef IN_LEAKY
    {"leaky", "config leaky", 
            { 
             {"ucMode", "set", "set unicast packets leaky mode", "<port|fdb>", SW_API_UC_LEAKY_MODE_SET, NULL}, 
             {"ucMode", "get", "get unicast packets leaky mode", "", SW_API_UC_LEAKY_MODE_GET, NULL},
             {"mcMode", "set", "set multicast packets leaky mode", "<port|fdb>", SW_API_MC_LEAKY_MODE_SET, NULL}, 
             {"mcMode", "get", "get multicast packets leaky mode", "", SW_API_MC_LEAKY_MODE_GET, NULL},             
             {"arpMode", "set", "set arp packets leaky mode", "<port_id> <enable|disable>", SW_API_ARP_LEAKY_MODE_SET, NULL}, 
             {"arpMode", "get", "get arp packets leaky mode", "<port_id>", SW_API_ARP_LEAKY_MODE_GET, NULL},
             {"ptUcMode", "set", "set unicast packets leaky status of a port", "<port_id> <enable|disable>", SW_API_PT_UC_LEAKY_MODE_SET, NULL}, 
             {"ptUcMode", "get", "get unicast packets leaky status of a port", "<port_id>", SW_API_PT_UC_LEAKY_MODE_GET, NULL},
             {"ptMcMode", "set", "set multicast packets leaky status of a port", "<port_id> <enable|disable>", SW_API_PT_MC_LEAKY_MODE_SET, NULL}, 
             {"ptMcMode", "get", "get multicast packets leaky status of a port", "<port_id>", SW_API_PT_MC_LEAKY_MODE_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            }, 
    },
    #endif

    /*mirror*/
    #ifdef IN_MIRROR
    {"mirror", "config mirror", 
            { 
             {"analyPt", "set", "set mirror analysis port", "<port_id>", SW_API_MIRROR_ANALY_PT_SET, NULL}, 
             {"analyPt", "get", "get mirror analysis port", "", SW_API_MIRROR_ANALY_PT_GET, NULL},
             {"ptIngress", "set", "set ingress mirror status of a port", "<port_id> <enable|disable>", SW_API_MIRROR_IN_PT_SET, NULL}, 
             {"ptIngress", "get", "get ingress mirror status of a port", "<port_id>", SW_API_MIRROR_IN_PT_GET, NULL},      
             {"ptEgress", "set", "set egress mirror status of a port", "<port_id> <enable|disable>", SW_API_MIRROR_EG_PT_SET, NULL}, 
             {"ptEgress", "get", "get egress mirror status of a port", "<port_id>", SW_API_MIRROR_EG_PT_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            }, 
    },
    #endif

    /*rate*/
    #ifdef IN_RATE
    {"rate", "config rate limit", 
            { 
             {"qEgress", "set", "set egress rate limit of a queue", "<port_id> <queueid:0-3> <speed:(kbps)> <enable|disable>", SW_API_RATE_QU_EGRL_SET, NULL}, 
             {"qEgress", "get", "get egress rate limit of a queue", "<port_id> <queueid:0-3>", SW_API_RATE_QU_EGRL_GET, NULL},
             {"ptEgress", "set", "set egress rate limit of a port", "<port_id> <speed:(kbps)> <enable|disable>", SW_API_RATE_PT_EGRL_SET, NULL}, 
             {"ptEgress", "get", "get egress rate limit of a port", "<port_id>", SW_API_RATE_PT_EGRL_GET, NULL},       
             {"ptIngress", "set", "set ingress rate limit of a port", "<port_id> <speed:(kbps)> <enable|disable>", SW_API_RATE_PT_INRL_SET, NULL}, 
             {"ptIngress", "get", "get ingress rate limit of a port", "<port_id>", SW_API_RATE_PT_INRL_GET, NULL},
             {"stormCtrl", "set", "set storm control status of a particular frame type", "<port_id> <unicast|multicast|broadcast> <enable|disable>", SW_API_STORM_CTRL_FRAME_SET, NULL}, 
             {"stormCtrl", "get", "get storm control status of a particular frame type", "<port_id> <unicast|multicast|broadcast>", SW_API_STORM_CTRL_FRAME_GET, NULL}, 
             {"stormCtrlRate", "set", "set storm ctrl rate", "<port_id> <rate:(packets/s)>", SW_API_STORM_CTRL_RATE_SET, NULL}, 
             {"stormCtrlRate", "get", "get storm ctrl rate", "<port_id>", SW_API_STORM_CTRL_RATE_GET, NULL},
             {"portpolicer", "set", "set port policer", "<port_id>", SW_API_RATE_PORT_POLICER_SET, NULL}, 
             {"portpolicer", "get", "get port policer", "<port_id>", SW_API_RATE_PORT_POLICER_GET, NULL},
             {"portshaper", "set", "set port egress shaper", "<port_id> <enable|disable>", SW_API_RATE_PORT_SHAPER_SET, NULL}, 
             {"portshaper", "get", "get port egress shaper", "<port_id>", SW_API_RATE_PORT_SHAPER_GET, NULL},
             {"queueshaper", "set", "set queue egress shaper", "<port_id> <queue_id> <enable|disable>", SW_API_RATE_QUEUE_SHAPER_SET, NULL}, 
             {"queueshaper", "get", "get queue egress shaper", "<port_id> <queue_id>", SW_API_RATE_QUEUE_SHAPER_GET, NULL},
             {"aclpolicer", "set", "set acl policer", "<policer_id>", SW_API_RATE_ACL_POLICER_SET, NULL}, 
             {"aclpolicer", "get", "get acl policer", "<policer_id>", SW_API_RATE_ACL_POLICER_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            }, 
    },
    #endif
    
    /*stp*/
    #ifdef IN_STP
    {"stp", "config STP", 
            { 
             {"portState", "set", "set STP state of a port", "st_id <port_id> <disable|block|listen|learn|forward>", SW_API_STP_PT_STATE_SET, NULL}, 
             {"portState", "get", "get STP state of a port", "st_id <port_id>", SW_API_STP_PT_STATE_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            }, 
    },
    #endif

    /*mib*/
    #ifdef IN_MIB
    {"mib", "show MIB statistics information", 
            {
             {"statistics", "get", "get statistics information of a port", "<port_id>",  SW_API_PT_MIB_GET, NULL},
             {"status", "set", "set mib status", "<enable|disable>",  SW_API_MIB_STATUS_SET, NULL},
             {"status", "get", "get mib status", "",  SW_API_MIB_STATUS_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            },
    },
    #endif

    /* led */
    #ifdef IN_LED
    {"led", "set/get led control pattern",
        {
            {"ctrlpattern", "set", "set led control pattern", "<group_id> <led_id>", SW_API_LED_PATTERN_SET, NULL},
            {"ctrlpattern", "get", "get led control pattern", "<group_id> <led_id>", SW_API_LED_PATTERN_GET, NULL},
            {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
        },
    },
    #endif

    /* cosmap */
    #ifdef IN_COSMAP
    {"cosmap", "set/get cosmap table",
        {
            {"dscp2pri", "set", "set dscp to priority map table", "<dscp> <priority>", SW_API_COSMAP_DSCP_TO_PRI_SET, NULL},
            {"dscp2pri", "get", "get dscp to priority map table", "<dscp>", SW_API_COSMAP_DSCP_TO_PRI_GET, NULL},
            {"dscp2dp", "set", "set dscp to dp map table", "<dscp> <dp>", SW_API_COSMAP_DSCP_TO_DP_SET, NULL},
            {"dscp2dp", "get", "get dscp to dp map table", "<dscp>", SW_API_COSMAP_DSCP_TO_DP_GET, NULL},
            {"up2pri", "set", "set dot1p to priority map table", "<up> <priority>", SW_API_COSMAP_UP_TO_PRI_SET, NULL},
            {"up2pri", "get", "get dot1p to priority map table", "<up>", SW_API_COSMAP_UP_TO_PRI_GET, NULL},
            {"up2dp", "set", "set dot1p to dp map table", "<up> <dp>", SW_API_COSMAP_UP_TO_DP_SET, NULL},
            {"up2dp", "get", "get dot1p to dp map table", "<up>", SW_API_COSMAP_UP_TO_DP_GET, NULL},
            {"pri2q", "set", "set priority to queue mapping", "<priority> <queueid>", SW_API_COSMAP_PRI_TO_QU_SET, NULL}, 
            {"pri2q", "get", "get priority to queue mapping", "<priority>", SW_API_COSMAP_PRI_TO_QU_GET, NULL}, 
            {"pri2ehq", "set", "set priority to enhanced queue mapping", "<priority> <queueid>", SW_API_COSMAP_PRI_TO_EHQU_SET, NULL}, 
            {"pri2ehq", "get", "get priority to enhanced queue mapping", "<priority>", SW_API_COSMAP_PRI_TO_EHQU_GET, NULL}, 
            {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
        },
    },
    #endif

    /*misc*/
    #ifdef IN_MISC
    {"misc", "config miscellaneous", 
        { 
             {"arp", "set", "set arp packets hardware identification status", "<enable|disable>", SW_API_ARP_STATUS_SET, NULL}, 
             {"arp", "get", "get arp packets hardware identification status", "", SW_API_ARP_STATUS_GET, NULL},
             {"frameMaxSize", "set", "set the maximal received frame size of the device", "<size:byte>", SW_API_FRAME_MAX_SIZE_SET, NULL}, 
             {"frameMaxSize", "get", "get the maximal received frame size of the device", "", SW_API_FRAME_MAX_SIZE_GET, NULL},             
             {"ptUnkSaCmd", "set", "set forwarding command for frames with unknown source address", "<port_id> <forward|drop|cpycpu|rdtcpu>", SW_API_PT_UNK_SA_CMD_SET, NULL}, 
             {"ptUnkSaCmd", "get", "get forwarding command for frames with unknown source address", "<port_id>", SW_API_PT_UNK_SA_CMD_GET, NULL},               
             {"ptUnkUcFilter", "set", "set flooding status of unknown unicast frames", "<port_id> <enable|disable>", SW_API_PT_UNK_UC_FILTER_SET, NULL}, 
             {"ptUnkUcFilter", "get", "get flooding status of unknown unicast frames", "<port_id>", SW_API_PT_UNK_UC_FILTER_GET, NULL},  
             {"ptUnkMcFilter", "set", "set flooding status of unknown multicast frames", "<port_id> <enable|disable>", SW_API_PT_UNK_MC_FILTER_SET, NULL}, 
             {"ptUnkMcFilter", "get", "get flooding status of unknown multicast frames", "<port_id>", SW_API_PT_UNK_MC_FILTER_GET, NULL},
             {"ptBcFilter", "set", "set flooding status of broadcast frames", "<port_id> <enable|disable>", SW_API_PT_BC_FILTER_SET, NULL},
             {"ptBcFilter", "get", "get flooding status of broadcast frames", "<port_id>", SW_API_PT_BC_FILTER_GET, NULL}, 
             {"cpuPort", "set", "set cpu port status", "<enable|disable>", SW_API_CPU_PORT_STATUS_SET, NULL}, 
             {"cpuPort", "get", "get cpu port status", "", SW_API_CPU_PORT_STATUS_GET, NULL},  
             {"bctoCpu", "set", "set broadcast frames to Cpu port status", "<enable|disable>", SW_API_BC_TO_CPU_PORT_SET, NULL}, 
             {"bctoCpu", "get", "get broadcast frames to Cpu port status", "", SW_API_BC_TO_CPU_PORT_GET, NULL},  
             {"PppoeCmd", "set", "set pppoe frames forwarding command", "<forward|drop|cpycpu|rdtcpu>", SW_API_PPPOE_CMD_SET, NULL}, 
             {"PppoeCmd", "get", "get pppoe frames forwarding command", "", SW_API_PPPOE_CMD_GET, NULL},  
             {"Pppoe", "set", "set pppoe frames hardware identification status", "<enable|disable>", SW_API_PPPOE_STATUS_SET, NULL}, 
             {"Pppoe", "get", "get pppoe frames hardware identification status", "", SW_API_PPPOE_STATUS_GET, NULL},  
             {"ptDhcp", "set", "set dhcp frames hardware identification status", "<port_id> <enable|disable>", SW_API_PT_DHCP_SET, NULL}, 
             {"ptDhcp", "get", "get dhcp frames hardware identification status", "<port_id>", SW_API_PT_DHCP_GET, NULL},
             {"arpcmd", "set", "set arp packets forwarding command", "<forward|drop|cpycpu|rdtcpu>", SW_API_ARP_CMD_SET, NULL}, 
             {"arpcmd", "get", "get arp packets forwarding command", "", SW_API_ARP_CMD_GET, NULL},
             {"eapolcmd", "set", "set eapol packets forwarding command", "<forward|drop|cpycpu|rdtcpu>", SW_API_EAPOL_CMD_SET, NULL}, 
             {"eapolcmd", "get", "get eapol packets forwarding command", "", SW_API_EAPOL_CMD_GET, NULL},
             {"pppoesession", "add", "add a pppoe session entry", "<session_id> <enable|disable>", SW_API_PPPOE_SESSION_ADD, NULL},
             {"pppoesession", "del", "del a pppoe session entry", "<session_id>", SW_API_PPPOE_SESSION_DEL, NULL},
             {"pppoesession", "get", "get a pppoe session entry", "<session_id>", SW_API_PPPOE_SESSION_GET, NULL},
             {"eapolstatus", "set", "set eapol frames hardware identification status", "<port_id> <enable|disable>", SW_API_EAPOL_STATUS_SET, NULL}, 
             {"eapolstatus", "get", "get eapol frames hardware identification status", "<port_id>", SW_API_EAPOL_STATUS_GET, NULL},
             {"rip", "set", "set rip packets hardware identification status", "<enable|disable>", SW_API_RIPV1_STATUS_SET, NULL}, 
             {"rip", "get", "get rip packets hardware identification status", "", SW_API_RIPV1_STATUS_GET, NULL},
             {"ptarpreq", "set", "set arp request packets hardware identification status", "<port_id> <enable|disable>", SW_API_PT_ARP_REQ_STATUS_SET, NULL}, 
             {"ptarpreq", "get", "get arp request packets hardware identification status", "<port_id>", SW_API_PT_ARP_REQ_STATUS_GET, NULL},    
             {"ptarpack", "set", "set arp ack packets hardware identification status", "<port_id> <enable|disable>", SW_API_PT_ARP_ACK_STATUS_SET, NULL}, 
             {"ptarpack", "get", "get arp ack packets hardware identification status", "<port_id>", SW_API_PT_ARP_ACK_STATUS_GET, NULL},   
             {"extendpppoe", "add", "add a pppoe session entry", "", SW_API_PPPOE_SESSION_TABLE_ADD, NULL},
             {"extendpppoe", "del", "del a pppoe session entry", "", SW_API_PPPOE_SESSION_TABLE_DEL, NULL},
             {"extendpppoe", "get", "get a pppoe session entry", "", SW_API_PPPOE_SESSION_TABLE_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
        },
    },
    #endif

    /* IP */
    #ifdef IN_IP
    {"ip", "config ip",
        {
            {"hostentry", "add", "add host entry", "", SW_API_IP_HOST_ADD, NULL},
            {"hostentry", "del", "del host entry", "<del_mode>", SW_API_IP_HOST_DEL, NULL},
            {"hostentry", "get", "get host entry", "<get_mode>", SW_API_IP_HOST_GET, NULL},
            {"hostentry", "next", "next host entry", "<next_mode>", SW_API_IP_HOST_NEXT, NULL},
            {"hostentry", "show", "show whole host entries", "", SW_CMD_HOST_SHOW, cmd_show_host},
            {"hostentry", "bindcnt", "bind counter to host entry", "<host entry id> <cnt id> <enable|disable>", SW_API_IP_HOST_COUNTER_BIND, NULL},
            {"hostentry", "bindpppoe", "bind pppoe to host entry", "<host entry id> <pppoe id> <enable|disable>", SW_API_IP_HOST_PPPOE_BIND, NULL},
            {"ptarplearn", "set", "set port arp learn flag, bit0 req bit1 ack", "<port_id> <flag>", SW_API_IP_PT_ARP_LEARN_SET, NULL},
            {"ptarplearn", "get", "get port arp learn flag, bit0 req bit1 ack", "<port_id>", SW_API_IP_PT_ARP_LEARN_GET, NULL},
            {"arplearn",   "set", "set arp learn mode", "<learnlocal|learnall>", SW_API_IP_ARP_LEARN_SET, NULL},
            {"arplearn",   "get", "get arp learn mode", "", SW_API_IP_ARP_LEARN_GET, NULL},
            {"ptipsrcguard",   "set", "set ip source guard mode", "<port_id> <mac_ip|mac_ip_port|mac_ip_vlan|mac_ip_port_vlan|no_guard>", SW_API_IP_SOURCE_GUARD_SET, NULL},
            {"ptipsrcguard",   "get", "get ip source guard mode", "", SW_API_IP_SOURCE_GUARD_GET, NULL},
            {"ptarpsrcguard",  "set", "set arp source guard mode", "<port_id> <mac_ip|mac_ip_port|mac_ip_vlan|mac_ip_port_vlan|no_guard>", SW_API_IP_ARP_GUARD_SET, NULL},
            {"ptarpsrcguard",  "get", "get arp source guard mode", "", SW_API_IP_ARP_GUARD_GET, NULL},
            {"routestatus", "set", "set ip route status", "<enable|disable>", SW_API_IP_ROUTE_STATUS_SET, NULL}, 
            {"routestatus", "get", "get ip route status", "", SW_API_IP_ROUTE_STATUS_GET, NULL},
            {"intfentry", "add", "add interface mac address", "", SW_API_IP_INTF_ENTRY_ADD, NULL}, 
            {"intfentry", "del", "del interface mac address", "", SW_API_IP_INTF_ENTRY_DEL, NULL},
            {"intfentry", "show", "show whole interface mac entries", "", SW_CMD_INTFMAC_SHOW, cmd_show_intfmac},
            {"ipunksrc", "set", "set ip unkown source command", "<forward|drop|cpycpu|rdtcpu>", SW_API_IP_UNK_SOURCE_CMD_SET, NULL}, 
            {"ipunksrc", "get", "get ip unkown source command", "", SW_API_IP_UNK_SOURCE_CMD_GET, NULL},
            {"arpunksrc", "set", "set arp unkown source command", "<forward|drop|cpycpu|rdtcpu>", SW_API_ARP_UNK_SOURCE_CMD_SET, NULL}, 
            {"arpunksrc", "get", "get arp unkown source command", "", SW_API_ARP_UNK_SOURCE_CMD_GET, NULL},
            {"ip6baseaddr", "set", "set ip6 base address", "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx", SW_API_IP6_BASE_ADDR_SET, NULL}, 
            {"ip6baseaddr", "get", "get ip6 base address", "", SW_API_IP6_BASE_ADDR_GET, NULL},
            {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
        },
    },
    #endif

    /* NAT */
    #ifdef IN_NAT
    {"nat", "config nat",
        {
            {"natentry", "add", "add nat entry", "", SW_API_NAT_ADD, NULL},
            {"natentry", "del", "del nat entry", "<del_mode>", SW_API_NAT_DEL, NULL},
            {"natentry", "get", "get nat entry", "<get_mode>", SW_API_NAT_GET, NULL},
            {"natentry", "show", "show whole nat entries", "", SW_CMD_NAT_SHOW, cmd_show_nat},
            {"natentry", "bindcnt", "bind counter to nat entry", "<nat entry id> <cnt id> <enable|disable>", SW_API_NAT_COUNTER_BIND, NULL},
            {"naptentry", "add", "add napt entry", "", SW_API_NAPT_ADD, NULL},
            {"naptentry", "del", "del napt entry", "<del_mode>", SW_API_NAPT_DEL, NULL},
            {"naptentry", "get", "get napt entry", "<get_mode>", SW_API_NAPT_GET, NULL},
            {"naptentry", "show", "show whole napt entries", "", SW_CMD_NAPT_SHOW, cmd_show_napt},
            {"naptentry", "bindcnt", "bind counter to napt entry", "<napt entry id> <cnt id> <enable|disable>", SW_API_NAPT_COUNTER_BIND, NULL},
            {"natstatus", "set", "set nat status", "<enable|disable>", SW_API_NAT_STATUS_SET, NULL}, 
            {"natstatus", "get", "get nat status", "", SW_API_NAT_STATUS_GET, NULL},
            {"naptstatus", "set", "set napt status", "<enable|disable>", SW_API_NAPT_STATUS_SET, NULL},
            {"naptstatus", "get", "get napt status", "", SW_API_NAPT_STATUS_GET, NULL},
            {"nathash", "set", "set nat hash mode", "<flag>", SW_API_NAT_HASH_MODE_SET, NULL}, 
            {"nathash", "get", "get nat hash mode", "", SW_API_NAT_HASH_MODE_GET, NULL},
            {"naptmode", "set", "set napt mode", "<fullcone|strictcone|portstrict|synmatric>", SW_API_NAPT_MODE_SET, NULL}, 
            {"naptmode", "get", "get napt mode", "", SW_API_NAPT_MODE_GET, NULL},
            {"prvbaseaddr", "set", "set nat prv base address", "<ip4 addr>", SW_API_PRV_BASE_ADDR_SET, NULL}, 
            {"prvbaseaddr", "get", "get nat prv base address", "", SW_API_PRV_BASE_ADDR_GET, NULL},
            {"prvaddrmode", "set", "set nat prv address map mode", "<enable|disable>", SW_API_PRV_ADDR_MODE_SET, NULL}, 
            {"prvaddrmode", "get", "get nat prv address map mode", "", SW_API_PRV_ADDR_MODE_GET, NULL},
            {"pubaddr", "add", "add pub address", "", SW_API_PUB_ADDR_ENTRY_ADD, NULL},
            {"pubaddr", "del", "del pub address", "<del_mode>", SW_API_PUB_ADDR_ENTRY_DEL, NULL},
            {"pubaddr", "show", "show whole pub address entries", "", SW_CMD_PUBADDR_SHOW, cmd_show_pubaddr},
            {"natunksess", "set", "set nat unkown session command", "<forward|drop|cpycpu|rdtcpu>", SW_API_NAT_UNK_SESSION_CMD_SET, NULL}, 
            {"natunksess", "get", "get nat unkown session command", "", SW_API_NAT_UNK_SESSION_CMD_GET, NULL},
            {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
        },
    },
    #endif

    /*Trunk*/
    #ifdef IN_TRUNK
    {"trunk", "config trunk", 
        {
            {"group", "set", "set trunk group member info", "<trunk_id> <disable|enable> <port_bitmap>", SW_API_TRUNK_GROUP_SET, NULL}, 
            {"group", "get", "get trunk group member info", "<trunk_id>", SW_API_TRUNK_GROUP_GET, NULL},
            {"hashmode", "set", "set trunk hash mode", "<hash_mode>", SW_API_TRUNK_HASH_SET, NULL}, 
            {"hashmode", "get", "get trunk hash mode", "", SW_API_TRUNK_HASH_GET, NULL},
             {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
        },
    },
    #endif

    /* debug */
    {"debug", "read/write register",
        {
              {"phy", "get", "read phy register", "<ph_id> <reg_addr>", SW_API_PHY_GET, NULL}, 
              {"phy", "set", "write phy register", "<ph_id> <reg_addr> <value>", SW_API_PHY_SET, NULL}, 
              {"reg", "get", "read switch register", "<reg_addr> <4>", SW_API_REG_GET, NULL}, 
              {"reg", "set", "write switch register", "<reg_addr> <value> <4>", SW_API_REG_SET, NULL},  
              {"field", "get", "read switch register field", "<reg_addr> <offset> <len> <4>", SW_API_REG_FIELD_GET, NULL}, 
              {"field", "set", "write switch register field", "<reg_addr> <offset> <len> <value> <4>", SW_API_REG_FIELD_SET, NULL}, 
              {"aclList", "dump", "dump all acl list", "", SW_API_ACL_LIST_DUMP, NULL},
              {"aclRule", "dump", "dump all acl rule", "", SW_API_ACL_RULE_DUMP, NULL},
              {"device",  "reset", "reset device",     "", SW_API_SWITCH_RESET, NULL},
              {"ssdk",  "config", "show ssdk configuration",     "", SW_API_SSDK_CFG, NULL},
              {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
        },
    },

    /*debug*/             
    {"device", "set device id",
            { 
              {"id", "set", "set device id", "<dev_id>", SW_CMD_SET_DEVID, cmd_set_devid},
              {NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/
            },                                 
    },
    
    {"help", "type ? get help", {{NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/}},
    
    {"quit", "type quit/q quit shell", {{NULL, NULL, NULL, NULL, (int)NULL, NULL}/*end of desc*/}},
    
    {NULL, NULL, {{NULL, NULL, NULL, NULL, (int)NULL, NULL}}} /*end of desc*/
};

