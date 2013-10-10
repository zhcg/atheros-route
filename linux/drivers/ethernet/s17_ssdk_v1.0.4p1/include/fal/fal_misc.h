/*
 * Copyright (c) 2007-2008 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/**
 * @defgroup fal_gen FAL_MISC
 * @{
 */ 
#ifndef _FAL_MISC_H_
#define _FAL_MISC_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "common/sw.h"
#include "fal/fal_type.h"


typedef struct {
    a_uint32_t entry_id;
    a_uint32_t session_id;
    a_bool_t   multi_session;
    a_bool_t   uni_session;
}fal_pppoe_session_t;

typedef enum {
    FAL_LOOP_CHECK_1MS = 0,
    FAL_LOOP_CHECK_10MS,
    FAL_LOOP_CHECK_100MS,
    FAL_LOOP_CHECK_500MS,
}fal_loop_check_time_t;

sw_error_t fal_arp_status_set(a_uint32_t dev_id, a_bool_t enable);



sw_error_t fal_arp_status_get(a_uint32_t dev_id, a_bool_t * enable);



sw_error_t fal_frame_max_size_set(a_uint32_t dev_id, a_uint32_t size);



sw_error_t fal_frame_max_size_get(a_uint32_t dev_id, a_uint32_t * size);



sw_error_t
fal_port_unk_sa_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
                        fal_fwd_cmd_t cmd);



sw_error_t
fal_port_unk_sa_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
                        fal_fwd_cmd_t * cmd);



sw_error_t
fal_port_unk_uc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable);



sw_error_t
fal_port_unk_uc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable);



sw_error_t
fal_port_unk_mc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable);



sw_error_t
fal_port_unk_mc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable);


sw_error_t
fal_port_bc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable);


sw_error_t
fal_port_bc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable);


sw_error_t
fal_cpu_port_status_set(a_uint32_t dev_id, a_bool_t enable);



sw_error_t
fal_cpu_port_status_get(a_uint32_t dev_id, a_bool_t * enable);



sw_error_t
fal_bc_to_cpu_port_set(a_uint32_t dev_id, a_bool_t enable);



sw_error_t
fal_bc_to_cpu_port_get(a_uint32_t dev_id, a_bool_t * enable);



sw_error_t
fal_pppoe_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);



sw_error_t
fal_pppoe_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);



sw_error_t
fal_pppoe_status_set(a_uint32_t dev_id, a_bool_t enable);



sw_error_t
fal_pppoe_status_get(a_uint32_t dev_id, a_bool_t * enable);



sw_error_t
fal_port_dhcp_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);



sw_error_t
fal_port_dhcp_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);


sw_error_t
fal_arp_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);


sw_error_t
fal_arp_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);


sw_error_t
fal_eapol_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);


sw_error_t
fal_eapol_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);


sw_error_t
fal_pppoe_session_add(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t strip_hdr);


sw_error_t
fal_pppoe_session_del(a_uint32_t dev_id, a_uint32_t session_id);


sw_error_t
fal_pppoe_session_get(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t * strip_hdr);

sw_error_t
fal_eapol_status_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable);

sw_error_t
fal_eapol_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t * enable);

sw_error_t
fal_ripv1_status_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
fal_ripv1_status_get(a_uint32_t dev_id, a_bool_t * enable);


sw_error_t
fal_port_arp_req_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


sw_error_t
fal_port_arp_req_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable);


sw_error_t
fal_port_arp_ack_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


sw_error_t
fal_port_arp_ack_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable);


sw_error_t
fal_pppoe_session_table_add(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl);


sw_error_t
fal_pppoe_session_table_del(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl);


sw_error_t
fal_pppoe_session_table_get(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_GEN_H_ */
/**
 * @}
 */
