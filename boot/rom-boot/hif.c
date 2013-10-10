#include <asm/addrspace.h>
#include <ar7240_soc.h>
#include <wasp_api.h>
#include <hif_api.h>

A_HOSTIF find_hif(void)
{
    /* Find the host interface by reading bootstrap register */
    A_PRINTF("%s: bootstrap = 0x%x\n", __func__, ar7240_reg_rd(WASP_BOOTSTRAP_REG));
    return WASP_BOOT_TYPE(ar7240_reg_rd(WASP_BOOTSTRAP_REG));
}


void hif_module_install(struct hif_api *api)
{
    api->_find_hif = find_hif;
}
