/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @ingroup adf_os_public
 * @file adf_os_module.h
 * This file abstracts "kernel module" semantics.
 */

#ifndef _ADF_OS_MODULE_H
#define _ADF_OS_MODULE_H

#include <adf_os_module_pvt.h>

typedef a_status_t (*module_init_func_t)(void);

/**
 * @brief Specify the module's entry point.
 */ 
#define adf_os_virt_module_init(_mod_init_func)  __adf_os_virt_module_init(_mod_init_func)

/**
 * @brief Specify the module's exit point.
 */ 
#define adf_os_virt_module_exit(_mod_exit_func)  __adf_os_virt_module_exit(_mod_exit_func)

/**
 * @brief Specify the module's name.
 */ 
#define adf_os_virt_module_name(_name)      __adf_os_virt_module_name(_name)

/**
 * @brief Specify the module's dependency on another module.
 */ 
#define adf_os_module_dep(_name,_dep)       __adf_os_module_dep(_name,_dep)

/**
 * @brief Export a symbol from a module.
 */ 
#define adf_os_export_symbol(_sym)         __adf_os_export_symbol(_sym)
     
#endif /*_ADF_OS_MODULE_H*/
