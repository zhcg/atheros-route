/* copyright (c) 2009 Atheros Communications, Inc.
 * All rights reserved 
 * iptables module for matching the IPv4 priority field
 *
 * Tos Xu tosx@wicresoft.com 2008/12/18
 * 
 */


#ifndef _IPT_CLASSIFY_MATCH_H
#define _IPT_CLASSIFY_MATCH_H

/* match info */
struct ipt_classify_match_info {
	unsigned int priority;
	u_int8_t invert;
};

#endif /* _IPT_CLASSIFY_MATCH_H */
