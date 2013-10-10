#ifndef _SHELL_IO_H
#define _SHELL_IO_H

#include "sw.h"
#include "sw_api.h"
#include "fal.h"

#define SW_TYPE_DEF(type, parser, show) {type, parser, show}
typedef struct {
    sw_data_type_e data_type;
    sw_error_t(*param_check) ();
    void (*show_func) ();
} sw_data_type_t;

void  set_talk_mode(int mode);
int get_talk_mode(void);
void set_full_cmdstrp(char **cmdstrp);
sw_data_type_t * cmd_data_type_find(sw_data_type_e type);
void  cmd_strtol(char *str, a_uint32_t * arg_val);


sw_error_t cmd_data_check_portmap(char *cmdstr, fal_pbmp_t * val, a_uint32_t size);
sw_error_t cmd_data_check_confirm(char *cmdstr, a_bool_t def, a_bool_t * val, a_uint32_t size);

sw_error_t cmd_data_check_uint32(char *cmd_str, a_uint32_t * arg_val,
                                 a_uint32_t size);
sw_error_t cmd_data_check_uint16(char *cmd_str, a_uint32_t * arg_val,
                                 a_uint32_t size);
sw_error_t cmd_data_check_enable(char *cmd_str, a_uint32_t * arg_val,
                                 a_uint32_t size);
sw_error_t cmd_data_check_pbmp(char *cmd_str, a_uint32_t * arg_val,
                               a_uint32_t size);
sw_error_t cmd_data_check_duplex(char *cmd_str, a_uint32_t * arg_val,
                                 a_uint32_t size);
sw_error_t cmd_data_check_speed(char *cmd_str, a_uint32_t * arg_val,
                                a_uint32_t size);
sw_error_t cmd_data_check_1qmode(char *cmd_str, a_uint32_t * arg_val,
                                 a_uint32_t size);
sw_error_t cmd_data_check_egmode(char *cmd_str, a_uint32_t * arg_val,
                                 a_uint32_t size);
sw_error_t cmd_data_check_capable(char *cmd_str, a_uint32_t * arg_val,
                                  a_uint32_t size);
sw_error_t cmd_data_check_fdbentry(char *cmdstr, void *val, a_uint32_t size);
sw_error_t cmd_data_check_macaddr(char *cmdstr, void *val, a_uint32_t size);

void cmd_data_print_uint32(a_uint8_t * param_name, a_uint32_t * buf,
                           a_uint32_t size);
void cmd_data_print_uint16(a_uint8_t * param_name, a_uint32_t * buf,
                           a_uint32_t size);
void cmd_data_print_enable(a_uint8_t * param_name, a_uint32_t * buf,
                           a_uint32_t size);
void cmd_data_print_pbmp(a_uint8_t * param_name, a_uint32_t * buf,
                         a_uint32_t size);
void cmd_data_print_duplex(a_uint8_t * param_name, a_uint32_t * buf,
                           a_uint32_t size);
void cmd_data_print_speed(a_uint8_t * param_name, a_uint32_t * buf,
                          a_uint32_t size);
sw_error_t cmd_data_check_vlan(char *cmdstr, fal_vlan_t * val, a_uint32_t size);
void cmd_data_print_vlan(a_uint8_t * param_name, a_uint32_t * buf,
                         a_uint32_t size);
void cmd_data_print_mib(a_uint8_t * param_name, a_uint32_t * buf,
                        a_uint32_t size);
void cmd_data_print_1qmode(a_uint8_t * param_name, a_uint32_t * buf,
                           a_uint32_t size);
void cmd_data_print_egmode(a_uint8_t * param_name, a_uint32_t * buf,
                           a_uint32_t size);
void cmd_data_print_capable(a_uint8_t * param_name, a_uint32_t * buf,
                            a_uint32_t size);
void cmd_data_print_fdbentry(a_uint8_t * param_name, a_uint32_t * buf,
                             a_uint32_t size);
void cmd_data_print_macaddr(a_uint8_t * param_name, a_uint32_t * buf,
                            a_uint32_t size);
sw_error_t cmd_data_check_qos_sch(char *cmdstr, fal_sch_mode_t * val,
                                  a_uint32_t size);
void cmd_data_print_qos_sch(a_uint8_t * param_name, a_uint32_t * buf,
                            a_uint32_t size);
sw_error_t cmd_data_check_qos_pt(char *cmdstr, fal_qos_mode_t * val,
                                 a_uint32_t size);
void cmd_data_print_qos_pt(a_uint8_t * param_name, a_uint32_t * buf,
                           a_uint32_t size);
sw_error_t cmd_data_check_storm(char *cmdstr, fal_storm_type_t * val,
                                a_uint32_t size);
void cmd_data_print_storm(a_uint8_t * param_name, a_uint32_t * buf,
                          a_uint32_t size);
sw_error_t cmd_data_check_stp_state(char *cmdstr, fal_stp_state_t * val,
                                    a_uint32_t size);
void cmd_data_print_stp_state(a_uint8_t * param_name, a_uint32_t * buf,
                              a_uint32_t size);
sw_error_t cmd_data_check_leaky(char *cmdstr, fal_leaky_ctrl_mode_t * val,
                                a_uint32_t size);
void cmd_data_print_leaky(a_uint8_t * param_name, a_uint32_t * buf,
                          a_uint32_t size);
sw_error_t cmd_data_check_uinta(char *cmdstr, a_uint32_t * val,
                                a_uint32_t size);
void cmd_data_print_uinta(a_uint8_t * param_name, a_uint32_t * buf,
                          a_uint32_t size);
sw_error_t cmd_data_check_maccmd(char *cmdstr, fal_fwd_cmd_t * val,
                                 a_uint32_t size);
void cmd_data_print_maccmd(a_uint8_t * param_name, a_uint32_t * buf,
                           a_uint32_t size);

sw_error_t cmd_data_check_aclrule(char *info, void *val, a_uint32_t size);

void cmd_data_print_aclrule(a_uint8_t * param_name, a_uint32_t * buf,
                            a_uint32_t size);

sw_error_t
cmd_data_check_ledpattern(char *info, void * val, a_uint32_t size);

void
cmd_data_print_ledpattern(a_uint8_t * param_name, a_uint32_t * buf,
                       a_uint32_t size);

sw_error_t
cmd_data_check_invlan_mode(char *cmd_str, a_uint32_t * arg_val, a_uint32_t size);
void
cmd_data_print_invlan_mode(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);
sw_error_t
cmd_data_check_vlan_propagation(char *cmd_str, a_uint32_t * arg_val, a_uint32_t size);
void
cmd_data_print_vlan_propagation(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);
sw_error_t
cmd_data_check_vlan_translation(char *info, fal_vlan_trans_entry_t *val, a_uint32_t size);
void
cmd_data_print_vlan_translation(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);
sw_error_t
cmd_data_check_qinq_mode(char *cmd_str, a_uint32_t * arg_val, a_uint32_t size);
void
cmd_data_print_qinq_mode(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);
sw_error_t
cmd_data_check_qinq_role(char *cmd_str, a_uint32_t * arg_val, a_uint32_t size);
void
cmd_data_print_qinq_role(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);
void
cmd_data_print_cable_status(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);
void
cmd_data_print_cable_len(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);
void
cmd_data_print_ssdk_cfg(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_hdrmode(char *cmd_str, a_uint32_t * arg_val, a_uint32_t size);

void
cmd_data_print_hdrmode(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_fdboperation(char *cmd_str, void * val, a_uint32_t size);

sw_error_t
cmd_data_check_pppoe(char *cmd_str, void * val, a_uint32_t size);

void
cmd_data_print_pppoe(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_udf_type(char *cmdstr, fal_acl_udf_type_t * arg_val, a_uint32_t size);

void
cmd_data_print_udf_type(a_uint8_t * param_name, a_uint32_t * buf,
                       a_uint32_t size);

sw_error_t
cmd_data_check_host_entry(char *cmd_str, void * val, a_uint32_t size);

void
cmd_data_print_host_entry(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_arp_learn_mode(char *cmd_str, fal_arp_learn_mode_t * arg_val,
                        a_uint32_t size);

void
cmd_data_print_arp_learn_mode(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_ip_guard_mode(char *cmd_str, fal_source_guard_mode_t * arg_val, a_uint32_t size);

void
cmd_data_print_ip_guard_mode(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_nat_entry(char *cmd_str, void * val, a_uint32_t size);

void
cmd_data_print_nat_entry(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_napt_entry(char *cmd_str, void * val, a_uint32_t size);

void
cmd_data_print_napt_entry(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_napt_mode(char *cmd_str, fal_napt_mode_t * arg_val, a_uint32_t size);

void
cmd_data_print_napt_mode(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_intf_mac_entry(char *cmd_str, void * val, a_uint32_t size);

void
cmd_data_print_intf_mac_entry(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_ip4addr(char *cmdstr, void * val, a_uint32_t size);

void
cmd_data_print_ip4addr(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_ip6addr(char *cmdstr, void * val, a_uint32_t size);

void
cmd_data_print_ip6addr(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);

sw_error_t
cmd_data_check_pub_addr_entry(char *cmd_str, void * val, a_uint32_t size);


void
cmd_data_print_pub_addr_entry(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);


sw_error_t
cmd_data_check_egress_shaper(char *cmd_str, void * val, a_uint32_t size);


void
cmd_data_print_egress_shaper(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);


sw_error_t
cmd_data_check_acl_policer(char *cmd_str, void * val, a_uint32_t size);


void
cmd_data_print_acl_policer(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);


sw_error_t
cmd_data_check_port_policer(char *cmd_str, void * val, a_uint32_t size);


void
cmd_data_print_port_policer(a_uint8_t * param_name, a_uint32_t * buf, a_uint32_t size);


#endif

