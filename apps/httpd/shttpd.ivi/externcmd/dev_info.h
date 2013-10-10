/* dev_info -- header file
 * by bowen guo, 2008-9-18*/

#ifndef DEV_INFO_H
#define DEV_INFO_H

typedef struct eqp_hd_struct{
    unsigned int major;
    unsigned int minor;
        // other information ...
}eqp_hd_t;

typedef struct eqp_sw_struct{
    unsigned int major;
    unsigned int minor;
        // other information ...
}eqp_sw_t;

typedef struct eqp_struct{
    const char * const eqp_type; /* array maybe enough */
    unsigned int eqp_id;
    eqp_hd_t eqp_hd_info;
    eqp_sw_t eqp_sw_info;
}eqp_t;


eqp_t *get_eqp_info(void);

#endif
