#ifndef _IPT_NATTYPE_H_target
#define _IPT_NATTYPE_H_target

#define NATTYPE_TIMEOUT 300

enum nattype_mode
{
	MODE_DNAT,
	MODE_FORWARD_IN,
	MODE_FORWARD_OUT
};

enum nattype_type
{
	TYPE_PORT_ADDRESS_RESTRICT,
	TYPE_ENDPOINT_INDEPEND,
	TYPE_ADDRESS_RESTRICT
};


struct ipt_nattype_info {
	u_int16_t mode;
	u_int16_t type;
};

#endif /*_IPT_NATTYPE_H_target*/
