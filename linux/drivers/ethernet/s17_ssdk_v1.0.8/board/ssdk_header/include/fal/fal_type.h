/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_type FAL_TYPE
 * @{
 */  
#ifndef _FAL_TYPE_H_
#define _FAL_TYPE_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

typedef a_uint32_t fal_port_t;

#if (SW_MAX_NR_PORT <= 32)
    typedef a_uint32_t fal_pbmp_t;
#else
    typedef a_uint64_t fal_pbmp_t;
#endif

typedef struct {
    a_uint8_t uc[6];
} fal_mac_addr_t;

typedef a_uint32_t fal_ip4_addr_t;

typedef struct {
    a_uint32_t ul[4];
} fal_ip6_addr_t;

/**
@brief This enum defines several forwarding command type.
* Field description:
    FAL_MAC_FRWRD      - packets are normally forwarded
    FAL_MAC_DROP       - packets are dropped
    FAL_MAC_CPY_TO_CPU - packets are copyed to cpu
    FAL_MAC_RDT_TO_CPU - packets are redirected to cpu
*/
typedef enum {
    FAL_MAC_FRWRD = 0,		/**<   packets are normally forwarded */
    FAL_MAC_DROP,			/**<   packets are dropped */
    FAL_MAC_CPY_TO_CPU,		/**<   packets are copyed to cpu */
    FAL_MAC_RDT_TO_CPU		/**<   packets are redirected to cpu */
} fal_fwd_cmd_t;

typedef enum {
    FAL_BYTE_BASED = 0,
    FAL_FRAME_BASED, 
    FAL_RATE_MODE_BUTT
} fal_traffic_unit_t;

typedef a_uint32_t fal_queue_t;

#define FAL_SVL_FID   0xffff


/**
@brief This enum defines packets transmitted out vlan tagged mode.
*/
typedef enum {
    FAL_EG_UNMODIFIED = 0,  /**<  egress transmit packets unmodified */
    FAL_EG_UNTAGGED,        /**<   egress transmit packets without vlan tag*/
    FAL_EG_TAGGED,          /**<  egress transmit packets with vlan tag     */
    FAL_EG_HYBRID,          /**<  egress transmit packets in hybrid tag mode     */
    FAL_EG_UNTOUCHED,
    FAL_EG_MODE_BUTT
} fal_pt_1q_egmode_t;

#define FAL_NEXT_ENTRY_FIRST_ID 0xffffffff

typedef struct {
    a_uint32_t     counter_id;
    a_uint32_t     ingress_packet;
    a_uint32_t     ingress_byte;
    a_uint32_t     egress_packet;
    a_uint32_t     egress_byte;
}fal_counter_entry_t;

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_TYPE_H_ */
/**
 * @}
 */
