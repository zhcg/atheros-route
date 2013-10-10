#ifndef _SHELL_SW_H_
#define _SHELL_SW_H_

#ifdef __cplusplus
extern "C" {
#endif    

#include "sw.h"

int get_devid(void);
sw_error_t cmd_set_devid(a_uint32_t *arg_val);
sw_error_t cmd_show_fdb(a_uint32_t *arg_val);
sw_error_t cmd_show_vlan(a_uint32_t *arg_val);
sw_error_t cmd_show_resv_fdb(a_uint32_t *arg_val);
sw_error_t cmd_show_host(a_uint32_t *arg_val);
sw_error_t cmd_show_nat(a_uint32_t *arg_val);
sw_error_t cmd_show_napt(a_uint32_t *arg_val);
sw_error_t cmd_show_intfmac(a_uint32_t *arg_val);
sw_error_t cmd_show_pubaddr(a_uint32_t *arg_val);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* _SHELL_SW_H_ */
