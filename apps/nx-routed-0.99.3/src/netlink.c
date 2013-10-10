#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>

#include "router.h"
#include "socket.h"
#include "util.h"
#include "link.h"

#ifdef LINUX
#include "sys_linux.h"
#endif

extern struct rproto system_proto;

struct socket *netlink_socket = NULL;

int netlink_init() {
	struct socket *socket;
	
	// Регистрируем протокол
	if(register_routing_protocol(&system_proto) < 0) {
		error("Unable to register System routing protocol");
		return -1;
	}
	
	if((socket = socket_create(&system_proto)) == NULL) {
		error("Unable to create Netlink socket");
		return -1;
	}
	
	if(socket_bind_to_netlink(socket) < 0) {
		error("Unable to bind socket to Netlink");
		return -1;
	}
	
	if(socketlist_add(socket) < 0) {
		error("Unable to register Netlink socket");
		return -1;
	}	
	
	return 0;
}

int netlink_learn_routes(struct rproto *netlink,struct rtable *rtable) {
#ifdef DEBUG
	log_msg("netlink_learn_routes called");
	dump_rtable(rtable);
#endif	
	return system_add_routes(socket_list.next,rtable);
}

int netlink_unlearn_routes(struct rproto *netlink,struct rtable *rtable) {
#ifdef DEBUG
	log_msg("netlink_unlearn_routes called");
	dump_rtable(rtable);
#endif	
	return system_remove_routes(socket_list.next,rtable);	
}

int netlink_input_packet(struct rproto *netlink,
                         void *buf,
			 unsigned int len,
			 struct sockaddr *from,
			 int fromlen) {
//	struct rtable *learn_table,*unlearn_table;
//	struct sockaddr_nl *origin = (struct sockaddr_nl *)from;

	struct nlmsghdr *nh;
	char content[128] = "";
	struct ifinfomsg *ifinfo;
/*	struct ifaddrmsg *addrinfo;
	struct rtmsg *rtinfo;*/

	nh = (struct nlmsghdr *)buf;
	switch(nh->nlmsg_type) {
		case RTM_NEWLINK:
			ifinfo = (struct ifinfomsg *)NLMSG_DATA(nh);
#ifdef DEBUG
			{
			int rtlen;
			struct rtattr *rta;
			
			rtlen=RTM_PAYLOAD(nh);
			rta=RTM_RTA(ifinfo);
			
			/* little hack.. UNSPEC has len==1. This doesn't go well with 
			 * RTA_OK macro..
			 */
			if (rta->rta_type == IFLA_UNSPEC)
				rta->rta_len=4;
			while (RTA_OK(rta, rtlen)) {
				if (rta->rta_type == IFLA_IFNAME)
					log_msg("Netlink interface: %s", RTA_DATA(rta));
				rta=RTA_NEXT(rta,rtlen);
			}
			}
#endif			
			if((ifinfo->ifi_flags & IFF_UP) && (ifinfo->ifi_flags & IFF_RUNNING)) {
				linklist_update();
				snprintf(content,sizeof(content),"Iface %d is going up",ifinfo->ifi_index);
			}else{
				linklist_update();
				snprintf(content,sizeof(content),"Iface %d is going down",ifinfo->ifi_index);
			}
			break;
		default:
			return 0;
	}
	log_msg("Netlink: %s",content);
	
/*	learn_table = new_rtable();
	unlearn_table = new_rtable();
	
	if(learn_table->num_routes != 0)
		learn_routes(netlink,learn_table);
	if(unlearn_table->num_routes != 0)
		unlearn_routes(netlink,unlearn_table);
	
	free_rtable(learn_table);
	free_rtable(unlearn_table);*/
	
	return 0;
}

int netlink_output_packet(struct rproto *netlink,struct rtable *rtable,struct link *link) {
	return 0;
}

void netlink_link_up(struct rproto *netlink, struct link *link) {
	struct rtable *tmp;
	struct route *route; 
    
	if(!(link->flags & LF_UP))
		return;

	if(!(link->flags & LF_ANNOUNCETHIS))
		return;

	if((route = malloc(sizeof(struct route))) == NULL) {
		error("Insufficient memory allocating route");
		return;
	}
	tmp = new_rtable();
	
	if(link->flags & LF_POINTOPOINT) {
		route->dst = link->broadcast & link->netmask;
	}else{
		route->dst = link->address & link->netmask;
	}
	route->dstmask = link->netmask;
	route->nexthop = 0; // Через нас
	route->metric = link->cost;
	route->type = RT_DYNAMIC;
	route->flags = 0;
	route->source = 0;
	route->source_link = link;
	route->domain = 0;
	route->expire = 0;
	route->garbage = 0;
	route->timerset = 0;
	strncpy(route->iface,link->iface,sizeof(route->iface));

	add_route(tmp, route);
	learn_routes(&system_proto, tmp);
	free_rtable(tmp);
}

void netlink_link_down(struct rproto *netlink, struct link *link) {
	/* Must find all routes, that depend on this link.. and call unlearn
	 */
	struct route *r, *q;
	struct rtable *tmp;
	
	tmp = new_rtable();
	for (r=main_rtable->routes; r ; r=r->next) {
		if (r->source_link != link)
			continue;
		if (r->flags & RF_UNREACH)
			continue;
		q = dup_route(r);
		q->flags |= RF_UNREACH;
		q->garbage = time(NULL) + 120;
		add_route(tmp, q);
	}
	if(tmp->num_routes != 0)
		unlearn_routes(&system_proto, tmp);
	free_rtable(tmp);
}

void netlink_shutdown(struct rproto *netlink) {
//	free_rtable(rip2_announce_table);
}

struct rproto_operations system_ops = {
	netlink_learn_routes,
	netlink_unlearn_routes,
	netlink_input_packet,
	netlink_output_packet,
	NULL,
	netlink_shutdown,
	netlink_link_up,
	netlink_link_down
};

struct rproto system_proto = {
	"system",
	PF_NETLINK,
	SOCK_DGRAM,
	4,
	NETLINK_ROUTE,
	0,
	&system_ops,
	NULL
};
