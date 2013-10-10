#ifndef __MDIO_API_H__
#define __MDIO_API_H__

#include "gmac_fwd.h"

struct mdio_api {
    void (*_mdio_init)(void);
    int  (*_mdio_download_fw)(mdio_bw_exec_t *);
    int  (*_mdio_wait_lock)(void);
    void (*_mdio_release_lock)(unsigned char );
    int  (*_mdio_read_block)(unsigned char *, int );
    int  (*_mdio_copy_bytes)(unsigned char*, int );
    unsigned int (*_mdio_comp_cksum)(unsigned int *, int);
};

void mdio_module_install(struct mdio_api *api);

#endif /* __MDIO_API_H__ */
