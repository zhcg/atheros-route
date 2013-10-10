/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */

#ifndef _SW_API_H
#define _SW_API_H

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "sw_ioctl.h"

#define SW_MAX_API_BUF         1024
#define SW_MAX_API_PARAM       12 /* cmd type + return value + ten parameters */
#define SW_MAX_PAYLOAD         (SW_MAX_API_PARAM << 2)  /* maximum payload size for netlink msg*/
#define SW_PARAM_IN            0x1
#define SW_PARAM_OUT           0x2
#define SW_PARAM_PTR           0x4

#define SW_API_DEF(ioctl, name) {ioctl, name}
#if (!defined(KERNEL_MODULE))
#define SW_PARAM_DEF(ioctl, data, size, type, name) \
    {ioctl, size, data, type, name}
#else
#define SW_PARAM_DEF(ioctl, data, size, type, name) {ioctl, size, data, type}
#endif

typedef enum {
    SW_UINT8 = 1,
    SW_INT8,
    SW_UINT16,
    SW_INT16,
    SW_UINT32,
    SW_INT32,
    SW_UINT64,
    SW_INT64,
    SW_ENABLE,
    SW_SPEED,
    SW_DUPLEX,
    SW_1QMODE,
    SW_EGMODE,
    SW_CAP,
    SW_VLAN,
    SW_PBMP,
    SW_MIB,
    SW_MACADDR,
    SW_FDBENTRY,
    SW_SCH,
    SW_QOS,
    SW_STORM,
    SW_STP,
    SW_LEAKY,
    SW_MACCMD,
    SW_UINT_A,
    SW_ACLRULE,
    SW_LEDPATTERN,
    SW_INVLAN,
    SW_VLANPROPAGATION,
    SW_VLANTRANSLATION,
    SW_QINQMODE,
    SW_QINQROLE,
    SW_CABLESTATUS,
    SW_CABLELEN,
    SW_SSDK_CFG,
    SW_HDRMODE,
    SW_FDBOPRATION,
    SW_PPPOE,
    SW_ACL_UDF_TYPE,
    SW_IP_HOSTENTRY,
    SW_ARP_LEARNMODE,
    SW_IP_GUARDMODE,
    SW_NATENTRY,
    SW_NAPTENTRY,
    SW_NAPTMODE,
    SW_IP4ADDR,
    SW_IP6ADDR,
    SW_INTFMACENTRY,
    SW_PUBADDRENTRY,
    SW_INGPOLICER,
    SW_EGSHAPER,
    SW_ACLPOLICER,
    SW_MACCONFIG,
    SW_PHYCONFIG,
    SW_DATA_MAX,
} sw_data_type_e;

typedef struct {
    a_uint32_t api_id;
    void *func;
} sw_api_func_t;

typedef struct {
    a_uint32_t api_id;
    a_uint16_t data_size;
    a_uint8_t  data_type;
    a_uint8_t  param_type;
#if (!defined(KERNEL_MODULE))
    a_uint8_t param_name[20];
#endif
} sw_api_param_t;

typedef struct {
    a_uint32_t api_id;
    sw_api_func_t *api_fp;
    sw_api_param_t *api_pp;
    a_uint32_t api_nr;
} sw_api_t;

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* _SW_API_H */
