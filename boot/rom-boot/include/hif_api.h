#ifndef __HIF_API_H__
#define __HIF_API_H__

#include <fwd.h>
//#include <wasp_api.h>
//#include <vbuf_api.h>

typedef struct _HIF_CONFIG {
    int dummy;
} HIF_CONFIG;

typedef VBUF * adf_nbuf_t;

typedef struct _HIF_CALLBACK {
    /* callback when a buffer has be sent to the host*/
    void (*send_buf_done)(adf_nbuf_t buf, void *context);
    /* callback when a receive message is received */
    void (*recv_buf)(adf_nbuf_t hdr_buf, adf_nbuf_t buf, void *context);
    /* context used for all callbacks */
    void *context;
} HIF_CALLBACK;

struct hif_api {
    A_HOSTIF (*_find_hif)(void);

    hif_handle_t (*_init)(HIF_CONFIG *pConfig);

    void (* _shutdown)(hif_handle_t);

    void (*_register_callback)(hif_handle_t, HIF_CALLBACK *);

    int  (*_get_total_credit_count)(hif_handle_t);

    void (*_start)(hif_handle_t);

    void (*_config_pipe)(hif_handle_t handle, int pipe, int creditCount);

    int  (*_send_buffer)(hif_handle_t handle, int pipe, adf_nbuf_t buf);

    void (*_return_recv_buf)(hif_handle_t handle, int pipe, adf_nbuf_t buf);
    //void (*_set_recv_bufsz)(int pipe, int bufsz);
    //void (*_pause_recv)(int pipe);
    //void (*_resume_recv)(int pipe);
    int  (*_is_pipe_supported)(hif_handle_t handle, int pipe);

    int  (*_get_max_msg_len)(hif_handle_t handle, int pipe);

    int  (*_get_reserved_headroom)(hif_handle_t handle);

    void (*_isr_handler)(hif_handle_t handle);

    void (*_get_default_pipe)(hif_handle_t handle, A_UINT8 *pipe_uplink, A_UINT8 *pipe_downlink);

        /* room to expand this table by another table */
    void *pReserved;

};


void hif_module_install(struct hif_api *api);

#endif /* __HIF_API_H__  */
