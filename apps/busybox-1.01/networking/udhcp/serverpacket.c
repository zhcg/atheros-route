/* serverpacket.c
 *
 * Construct and send DHCP server packets
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#include "serverpacket.h"
#include "dhcpd.h"
#include "options.h"
#include "common.h"
#include "static_leases.h"

#define OLD_STAFILE  "/etc/.OldStaList"
struct staList
{
	int id;
	char macAddr[20];
	char staDesc[80];
	struct staList *next;
};

/* send a packet to giaddr using the kernel ip stack */
static int send_packet_to_relay(struct dhcpMessage *payload)
{
	DEBUG(LOG_INFO, "Forwarding packet to relay");

	return kernel_packet(payload, server_config.server, SERVER_PORT,
			payload->giaddr, SERVER_PORT);
}


/* send a packet to a specific arp address and ip address by creating our own ip packet */
static int send_packet_to_client(struct dhcpMessage *payload, int force_broadcast)
{
	uint8_t *chaddr;
	uint32_t ciaddr;

	if (force_broadcast) {
		DEBUG(LOG_INFO, "broadcasting packet to client (NAK)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else if (payload->ciaddr) {
		DEBUG(LOG_INFO, "unicasting packet to client ciaddr");
		ciaddr = payload->ciaddr;
		chaddr = payload->chaddr;
	} else if (ntohs(payload->flags) & BROADCAST_FLAG) {
		DEBUG(LOG_INFO, "broadcasting packet to client (requested)");
		ciaddr = INADDR_BROADCAST;
		chaddr = MAC_BCAST_ADDR;
	} else {
		DEBUG(LOG_INFO, "unicasting packet to client yiaddr");
		ciaddr = payload->yiaddr;
		chaddr = payload->chaddr;
	}
	return raw_packet(payload, server_config.server, SERVER_PORT,
			ciaddr, CLIENT_PORT, chaddr, server_config.ifindex);
}


/* send a dhcp packet, if force broadcast is set, the packet will be broadcast to the client */
static int send_packet(struct dhcpMessage *payload, int force_broadcast)
{
	int ret;

	if (payload->giaddr)
		ret = send_packet_to_relay(payload);
	else ret = send_packet_to_client(payload, force_broadcast);
	return ret;
}


static void init_packet(struct dhcpMessage *packet, struct dhcpMessage *oldpacket, char type)
{
	init_header(packet, type);
	packet->xid = oldpacket->xid;
	memcpy(packet->chaddr, oldpacket->chaddr, 16);
	packet->flags = oldpacket->flags;
	packet->giaddr = oldpacket->giaddr;
	packet->ciaddr = oldpacket->ciaddr;
	add_simple_option(packet->options, DHCP_SERVER_ID, server_config.server);
}


/* add in the bootp options */
static void add_bootp_options(struct dhcpMessage *packet)
{
	packet->siaddr = server_config.siaddr;
	if (server_config.sname)
		strncpy(packet->sname, server_config.sname, sizeof(packet->sname) - 1);
	if (server_config.boot_file)
		strncpy(packet->file, server_config.boot_file, sizeof(packet->file) - 1);
}


/* send a DHCP OFFER to a DHCP DISCOVER */
int sendOffer(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;
	struct dhcpOfferedAddr *lease = NULL;
	uint32_t req_align, lease_time_align = server_config.lease;
	uint8_t *req, *lease_time;
	struct option_set *curr;
	struct in_addr addr;
	uint8_t *hostname;

	uint32_t static_lease_ip;

	init_packet(&packet, oldpacket, DHCPOFFER);

	static_lease_ip = getIpByMac(server_config.static_leases, oldpacket->chaddr);

	/* ADDME: if static, short circuit */
	if(!static_lease_ip)
	{
	/* the client is in our lease/offered table */
	if ((lease = find_lease_by_chaddr(oldpacket->chaddr))) {
		if (!lease_expired(lease))
			lease_time_align = lease->expires - time(0);
		packet.yiaddr = lease->yiaddr;

	/* Or the client has a requested ip */
	} else if ((req = get_option(oldpacket, DHCP_REQUESTED_IP)) &&

		   /* Don't look here (ugly hackish thing to do) */
		   memcpy(&req_align, req, 4) &&

		   /* and the ip is in the lease range */
		   ntohl(req_align) >= ntohl(server_config.start) &&
		   ntohl(req_align) <= ntohl(server_config.end) &&
		
			!static_lease_ip &&  /* Check that its not a static lease */
			/* and is not already taken/offered */
		   ((!(lease = find_lease_by_yiaddr(req_align)) ||
		
		   /* or its taken, but expired */ /* ADDME: or maybe in here */
		   lease_expired(lease)))) {
				packet.yiaddr = req_align; /* FIXME: oh my, is there a host using this IP? */

			/* otherwise, find a free IP */
	} else {
			/* Is it a static lease? (No, because find_address skips static lease) */
		packet.yiaddr = find_address(0);

		/* try for an expired lease */
		if (!packet.yiaddr) packet.yiaddr = find_address(1);
	}

	if(!packet.yiaddr) {
		LOG(LOG_WARNING, "no IP addresses to give -- OFFER abandoned");
		return -1;
	}

	hostname = (uint8_t *)get_option(oldpacket, DHCP_HOST_NAME);
	if (!add_lease(hostname, packet.chaddr, packet.yiaddr, server_config.offer_time)) {
		LOG(LOG_WARNING, "lease pool is full -- OFFER abandoned");
		return -1;
	}

	if ((lease_time = get_option(oldpacket, DHCP_LEASE_TIME))) {
		memcpy(&lease_time_align, lease_time, 4);
		lease_time_align = ntohl(lease_time_align);
		if (lease_time_align > server_config.lease)
			lease_time_align = server_config.lease;
	}

	/* Make sure we aren't just using the lease time from the previous offer */
	if (lease_time_align < server_config.min_lease)
		lease_time_align = server_config.lease;
	}
	/* ADDME: end of short circuit */
	else
	{
		/* It is a static lease... use it */
		packet.yiaddr = static_lease_ip;
	}

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_align));

	curr = server_config.options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

	addr.s_addr = packet.yiaddr;
	LOG(LOG_INFO, "sending OFFER of %s", inet_ntoa(addr));
	return send_packet(&packet, 0);
}


int sendNAK(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;

	init_packet(&packet, oldpacket, DHCPNAK);

	DEBUG(LOG_INFO, "sending NAK");
	return send_packet(&packet, 1);
}


void add_staMac()
{
	FILE *fp, *flist;
	char STAbuf[128];
	char buf[20];
	int open = 0;
	struct staList oldstalist;
	
	system("wlanconfig ath0 list sta > /etc/.STAlist");
	
	/*if the /etc/.OldStaList is not exit, creat it*/
	if((fp = fopen(OLD_STAFILE, "r")) == NULL)     /*  /etc/.OldStaList  */
	{
		open = 1;
		fp = fopen(OLD_STAFILE, "at");
		flist = fopen("/etc/.STAlist", "r");    /*  /etc/.STAlist   */
        fgets(STAbuf, 128, flist);
        while(fgets(STAbuf, 128, flist))
        {
        	memset(&oldstalist, 0, sizeof(oldstalist));
        	strncpy(oldstalist.macAddr, STAbuf, 17);
			fwrite(&oldstalist, sizeof(oldstalist), 1, fp);
			LOG(LOG_INFO, "write old open is %d", open);
        }
		fclose(flist);
		fclose(fp);
	}

	flist = fopen("/etc/.STAlist", "r");    /*  /etc/.STAlist   */
	fgets(STAbuf, 128, flist);
    while(fgets(STAbuf, 128, flist))
    {
    	int ret = 0;
    	memset(buf, 0, sizeof buf);
		strncpy(buf, STAbuf, 17);
		LOG(LOG_INFO, "open is %d", open);

		if(open == 1)
		{
			fp = fopen(OLD_STAFILE, "r");			/*  /etc/.OldStaList  */
		}
		while(fread(&oldstalist, sizeof oldstalist, 1, fp) == 1)
		{
			LOG(LOG_INFO, "buf is %s, oldmac is %s", buf, oldstalist.macAddr);
			if(strcmp(buf, oldstalist.macAddr) == 0)
			{
				ret = 1;
				break;
			}
		}
		LOG(LOG_INFO, "ret is %d", ret);
		if(ret == 0)
		{
			fclose(fp);
			fp = fopen(OLD_STAFILE, "at");
			memset(&oldstalist, 0, sizeof(oldstalist));
        	strncpy(oldstalist.macAddr, buf, 17);
			fwrite(&oldstalist, sizeof(oldstalist), 1, fp);
		}
		fclose(fp);
		open =  1;
    }
	fclose(flist);
}

int sendACK(struct dhcpMessage *oldpacket, uint32_t yiaddr)
{
	struct dhcpMessage packet;
	struct option_set *curr;
	uint8_t *lease_time;
	uint32_t lease_time_align = server_config.lease;
	struct in_addr addr;
	uint8_t *hostname;

	init_packet(&packet, oldpacket, DHCPACK);
	packet.yiaddr = yiaddr;

	if ((lease_time = get_option(oldpacket, DHCP_LEASE_TIME))) {
		memcpy(&lease_time_align, lease_time, 4);
		lease_time_align = ntohl(lease_time_align);
		if (lease_time_align > server_config.lease)
			lease_time_align = server_config.lease;
		else if (lease_time_align < server_config.min_lease)
			lease_time_align = server_config.lease;
	}

	add_simple_option(packet.options, DHCP_LEASE_TIME, htonl(lease_time_align));

	curr = server_config.options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

	addr.s_addr = packet.yiaddr;
	LOG(LOG_INFO, "sending ACK to %s", inet_ntoa(addr));

	if (send_packet(&packet, 0) < 0)
		return -1;

	hostname = (uint8_t *)get_option(oldpacket, DHCP_HOST_NAME);
	add_lease(hostname, packet.chaddr, packet.yiaddr, lease_time_align);
	add_staMac();
	return 0;
}

void deal_addIp(struct dhcpMessage *oldpacket)
{
	int i, same = 0;
	struct in_addr addr, addr2;
	char mac_buf[20];
	int j, k;
	uint8_t *lease_time;
	uint8_t *requested;
	uint32_t requested_align;
	uint32_t lease_time_align = server_config.lease;

	requested = get_option(oldpacket, DHCP_REQUESTED_IP);
	if (requested) 
		memcpy(&requested_align, requested, 4);
	addr.s_addr = requested_align;

	for(j = 0, k = 0 ; j < 6; j++, k+=3)
	{
        sprintf(&mac_buf[k], "%02x:", oldpacket->chaddr[j]);
	}
	LOG(LOG_INFO, "the hostname is %s, the mac is %s, the ip is %s", get_option(oldpacket, DHCP_HOST_NAME), mac_buf, inet_ntoa(addr));

	//add_lease(get_option(oldpacket, DHCP_HOST_NAME), oldpacket->chaddr, oldpacket->yiaddr, lease_time_align);
	#if 0
	if ((lease_time = get_option(oldpacket, DHCP_LEASE_TIME))) 
	{
		LOG(LOG_ERR, "******the lease time is > 0");
		memcpy(&lease_time_align, lease_time, 4);
		lease_time_align = ntohl(lease_time_align);
		
		if (lease_time_align > server_config.lease)
		{
			lease_time_align = server_config.lease;
		}
		else if (lease_time_align < server_config.min_lease)
		{
			lease_time_align = server_config.lease;
		}
	}

	for (i = 0; i < server_config.max_leases; i++)
	{
		if(strlen(leases[i].hostname) > 0)
		{
			LOG(LOG_ERR, "******the hostname is %s", leases[i].hostname );

			addr2.s_addr = leases[i].yiaddr;
			LOG(LOG_ERR, "******the packet ip is %s, the exit ip is %s", inet_ntoa(addr), inet_ntoa(addr2));
			
			if (oldpacket->yiaddr == leases[i].yiaddr)
			{
				same = 1;
			}
		}
	}
	if( (same == 0 ) && (oldpacket->yiaddr > 0))
	{
		LOG(LOG_ERR, "******add the ip %s to lease", inet_ntoa(addr));
		add_lease(get_option(oldpacket, DHCP_HOST_NAME), oldpacket->chaddr, oldpacket->yiaddr, lease_time_align);
	}
	#endif
	
}


int send_inform(struct dhcpMessage *oldpacket)
{
	struct dhcpMessage packet;
	struct option_set *curr;

	init_packet(&packet, oldpacket, DHCPACK);

	curr = server_config.options;
	while (curr) {
		if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
			add_option_string(packet.options, curr->data);
		curr = curr->next;
	}

	add_bootp_options(&packet);

	return send_packet(&packet, 0);
}

void clear_dhcpIp(struct dhcpMessage *oldpacket)
{
	int i;
	
	for (i = 0; i < server_config.max_leases; i++)
	{
		if(strlen(leases[i].hostname) > 0)
		{
			if (strcmp(oldpacket->chaddr, leases[i].chaddr) == 0)
			{
				LOG(LOG_INFO, "clear the lease, hostname is %s", leases[i].hostname);
				memset(&(leases[i]), 0, sizeof(struct dhcpOfferedAddr));
			}
		}
	}
}

