#ifndef _SW_SHELL_H
#define _SW_SHELL_H

#ifdef __cplusplus
extern "C" {
#endif    

#include "sw.h"
#include "sw_api.h"
#include "ssdk_init.h"

extern a_uint32_t *ioctl_buf;
extern ssdk_init_cfg init_cfg;

#define IOCTL_BUF_SIZE 1024
#define CMDSTR_BUF_SIZE 1024
#define CMDSTR_ARGS_MAX 128
#define dprintf cmd_print
extern sw_error_t cmd_exec_api(a_uint32_t *arg_val);
extern void cmd_print(char *fmt, ...);
void cmd_print_error(sw_error_t rtn);


#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* _SW_SHELL_H */
