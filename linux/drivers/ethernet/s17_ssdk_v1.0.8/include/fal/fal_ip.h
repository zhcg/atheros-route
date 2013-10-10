/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_ip FAL_IP
 * @{
 */
#ifndef _FAL_IP_H_
#define _FAL_IP_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"
#include "fal/fal_type.h"

/* IP WCMP hash key flags */
#define FAL_WCMP_HASH_KEY_SIP             0x1
#define FAL_WCMP_HASH_KEY_DIP             0x2
#define FAL_WCMP_HASH_KEY_SPORT           0x4
#define FAL_WCMP_HASH_KEY_DPORT           0x8

/* IP entry operation flags */
#define FAL_IP_ENTRY_ID_EN               0x1
#define FAL_IP_ENTRY_INTF_EN             0x2
#define FAL_IP_ENTRY_PORT_EN             0x4
#define FAL_IP_ENTRY_STATUS_EN           0x8
#define FAL_IP_ENTRY_IPADDR_EN           0x10

/* IP host entry structure flags field */
#define FAL_IP_IP4_ADDR                         0x1
#define FAL_IP_IP6_ADDR                         0x2
#define FAL_IP_CPU_ADDR                         0x4

typedef struct {
    a_uint32_t entry_id;
    a_uint32_t flags;
    a_uint32_t status;
    fal_ip4_addr_t ip4_addr;
    fal_ip6_addr_t ip6_addr;
    fal_mac_addr_t mac_addr;
    a_uint32_t intf_id;
    fal_port_t port_id;
    a_bool_t mirror_en;
    a_bool_t counter_en;
    a_uint32_t counter_id;
    a_uint32_t packet;
    a_uint32_t byte;
    a_bool_t pppoe_en;
    a_uint32_t pppoe_id;
    fal_fwd_cmd_t action;
} fal_host_entry_t;

typedef enum {
    FAL_MAC_IP_GUARD = 0,
    FAL_MAC_IP_PORT_GUARD,
    FAL_MAC_IP_VLAN_GUARD,
    FAL_MAC_IP_PORT_VLAN_GUARD,
    FAL_NO_SOURCE_GUARD,
} fal_source_guard_mode_t;

typedef enum {
    FAL_ARP_LEARN_LOCAL = 0,
    FAL_ARP_LEARN_ALL,
} fal_arp_learn_mode_t;

/* IP host entry auto learn arp packets type */
#define FAL_ARP_LEARN_REQ                0x1
#define FAL_ARP_LEARN_ACK                0x2

typedef struct {
    a_uint32_t entry_id;
    a_uint16_t vid_low;
    a_uint16_t vid_high;
    fal_mac_addr_t mac_addr;
    a_bool_t ip4_route;
    a_bool_t ip6_route;
} fal_intf_mac_entry_t;

typedef struct {
    a_uint32_t nh_nr;
    a_uint32_t nh_id[16];
}fal_ip_wcmp_t;

sw_error_t
fal_ip_host_add(a_uint32_t dev_id, fal_host_entry_t * host_entry);

sw_error_t
fal_ip_host_del(a_uint32_t dev_id, a_uint32_t del_mode,
                fal_host_entry_t * host_entry);

sw_error_t
fal_ip_host_get(a_uint32_t dev_id, a_uint32_t get_mode,
                fal_host_entry_t * host_entry);

sw_error_t
fal_ip_host_next(a_uint32_t dev_id, a_uint32_t next_mode,
                 fal_host_entry_t * host_entry);

sw_error_t
fal_ip_host_counter_bind(a_uint32_t dev_id, a_uint32_t entry_id,
                         a_uint32_t cnt_id, a_bool_t enable);

sw_error_t
fal_ip_host_pppoe_bind(a_uint32_t dev_id, a_uint32_t entry_id,
                       a_uint32_t pppoe_id, a_bool_t enable);

sw_error_t
fal_ip_pt_arp_learn_set(a_uint32_t dev_id, fal_port_t port_id,
                        a_uint32_t flags);

sw_error_t
fal_ip_pt_arp_learn_get(a_uint32_t dev_id, fal_port_t port_id,
                        a_uint32_t * flags);

sw_error_t
fal_ip_arp_learn_set(a_uint32_t dev_id, fal_arp_learn_mode_t mode);

sw_error_t
fal_ip_arp_learn_get(a_uint32_t dev_id, fal_arp_learn_mode_t * mode);

sw_error_t
fal_ip_source_guard_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_source_guard_mode_t mode);

sw_error_t
fal_ip_source_guard_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_source_guard_mode_t * mode);

sw_error_t
fal_ip_arp_guard_set(a_uint32_t dev_id, fal_port_t port_id,
                     fal_source_guard_mode_t mode);

sw_error_t
fal_ip_arp_guard_get(a_uint32_t dev_id, fal_port_t port_id,
                     fal_source_guard_mode_t * mode);

sw_error_t
fal_ip_route_status_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
fal_ip_route_status_get(a_uint32_t dev_id, a_bool_t * enable);

sw_error_t
fal_ip_intf_entry_add(a_uint32_t dev_id, fal_intf_mac_entry_t * entry);

sw_error_t
fal_ip_intf_entry_del(a_uint32_t dev_id, a_uint32_t del_mode,
                      fal_intf_mac_entry_t * entry);

sw_error_t
fal_ip_intf_entry_next(a_uint32_t dev_id, a_uint32_t next_mode,
                       fal_intf_mac_entry_t * entry);

sw_error_t
fal_ip_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);

sw_error_t
fal_ip_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);

sw_error_t
fal_arp_unk_source_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);

sw_error_t
fal_arp_unk_source_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);

sw_error_t
fal_ip_age_time_set(a_uint32_t dev_id, a_uint32_t * time);

sw_error_t
fal_ip_age_time_get(a_uint32_t dev_id, a_uint32_t * time);

sw_error_t
fal_ip_wcmp_entry_set(a_uint32_t dev_id, a_uint32_t wcmp_id, fal_ip_wcmp_t * wcmp);

sw_error_t
fal_ip_wcmp_entry_get(a_uint32_t dev_id, a_uint32_t wcmp_id, fal_ip_wcmp_t * wcmp);

sw_error_t
fal_ip_wcmp_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode);

sw_error_t
fal_ip_wcmp_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_IP_H_ */
/**
 * @}
 */
