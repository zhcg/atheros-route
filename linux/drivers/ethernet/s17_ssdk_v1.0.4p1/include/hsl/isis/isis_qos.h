/*
 * Copyright (c) 2007-2009 Atheros Communications, Inc.
 * All rights reserved.
 *
 */ 
#ifndef _ISIS_QOS_H_
#define _ISIS_QOS_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "fal/fal_qos.h"

sw_error_t isis_qos_init(a_uint32_t dev_id);

#ifdef IN_QOS
    #define ISIS_QOS_INIT(rv, dev_id) \
    { \
        rv = isis_qos_init(dev_id); \
        SW_RTN_ON_ERROR(rv); \
    }
#else
    #define ISIS_QOS_INIT(rv, dev_id)
#endif

#ifdef HSL_STANDALONG


HSL_LOCAL sw_error_t
isis_qos_queue_tx_buf_status_set(a_uint32_t dev_id,
                                   fal_port_t port_id, a_bool_t enable);


HSL_LOCAL sw_error_t
isis_qos_queue_tx_buf_status_get(a_uint32_t dev_id,
                                   fal_port_t port_id,
                                   a_bool_t * enable);

HSL_LOCAL sw_error_t
isis_qos_port_tx_buf_status_set(a_uint32_t dev_id, fal_port_t port_id,
                                  a_bool_t enable);


HSL_LOCAL sw_error_t
isis_qos_port_tx_buf_status_get(a_uint32_t dev_id, fal_port_t port_id,
                                  a_bool_t * enable);


HSL_LOCAL sw_error_t
isis_qos_queue_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                               fal_queue_t queue_id,
                               a_uint32_t * number);


HSL_LOCAL sw_error_t
isis_qos_queue_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
                               fal_queue_t queue_id,
                               a_uint32_t * number);


HSL_LOCAL sw_error_t
isis_qos_port_tx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                              a_uint32_t * number);


HSL_LOCAL sw_error_t
isis_qos_port_tx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
                              a_uint32_t * number);


HSL_LOCAL sw_error_t
isis_qos_port_rx_buf_nr_set(a_uint32_t dev_id, fal_port_t port_id,
                              a_uint32_t * number);


HSL_LOCAL sw_error_t
isis_qos_port_rx_buf_nr_get(a_uint32_t dev_id, fal_port_t port_id,
                              a_uint32_t * number);


HSL_LOCAL sw_error_t
isis_qos_port_mode_set(a_uint32_t dev_id, fal_port_t port_id,
                         fal_qos_mode_t mode, a_bool_t enable);


HSL_LOCAL sw_error_t
isis_qos_port_mode_get(a_uint32_t dev_id, fal_port_t port_id,
                         fal_qos_mode_t mode, a_bool_t * enable);


HSL_LOCAL sw_error_t
isis_qos_port_mode_pri_set(a_uint32_t dev_id, fal_port_t port_id,
                             fal_qos_mode_t mode, a_uint32_t pri);


HSL_LOCAL sw_error_t
isis_qos_port_mode_pri_get(a_uint32_t dev_id, fal_port_t port_id,
                             fal_qos_mode_t mode, a_uint32_t * pri);


HSL_LOCAL sw_error_t
isis_qos_port_sch_mode_set(a_uint32_t dev_id, a_uint32_t port_id,
                        fal_sch_mode_t mode, const a_uint32_t weight[]);


HSL_LOCAL sw_error_t
isis_qos_port_sch_mode_get(a_uint32_t dev_id, a_uint32_t port_id,
                        fal_sch_mode_t * mode, a_uint32_t weight[]);

HSL_LOCAL sw_error_t
isis_qos_port_default_spri_set(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t spri);


HSL_LOCAL sw_error_t
isis_qos_port_default_spri_get(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t * spri);


HSL_LOCAL sw_error_t
isis_qos_port_default_cpri_set(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t cpri);


HSL_LOCAL sw_error_t
isis_qos_port_default_cpri_get(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t * cpri);


HSL_LOCAL sw_error_t
isis_qos_queue_remark_table_set(a_uint32_t dev_id, fal_port_t port_id,
                           fal_queue_t queue_id, a_uint32_t tbl_id, a_bool_t enable);


HSL_LOCAL sw_error_t
isis_qos_queue_remark_table_get(a_uint32_t dev_id, fal_port_t port_id,
                           fal_queue_t queue_id, a_uint32_t * tbl_id, a_bool_t * enable);


#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _ISIS_QOS_H_ */

