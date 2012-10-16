/*
 * Copyright (C) 2012 B.A.T.M.A.N. contributors:
 *
 * Simon Wunderlich
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/filter.h>
#include "alfred.h"

int netsock_close(int sock)
{
	return close(sock);
}

int netsock_open(struct globals *globals)
{
	int sock;
	struct sock_fprog filter;
	struct sockaddr_ll sll;
	struct ifreq ifr;
	struct sock_filter BPF[] = {
	/* tcpdump -dd "ether proto 0x4242" */
		{ 0x28, 0, 0, 0x0000000c },
		{ 0x15, 0, 1, 0x00004242 },
		{ 0x6, 0, 0, 0x0000ffff },
		{ 0x6, 0, 0, 0x00000000 },
	};

	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALFRED));
	if (sock  < 0) {
		fprintf(stderr, "can't open socket: %s\n", strerror(errno));
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, globals->interface, IFNAMSIZ);
	if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
		fprintf(stderr, "can't get interface: %s\n", strerror(errno));
		return -1;
	}

	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_protocol = htons(ETH_P_ALL);
	sll.sll_ifindex = ifr.ifr_ifindex;

	if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
		fprintf(stderr, "can't get MAC address: %s\n", strerror(errno));
		return -1;
	}

	memcpy(globals->hwaddr, &ifr.ifr_hwaddr.sa_data, 6);

	if (ioctl(sock, SIOCGIFMTU, &ifr) == -1) {
		fprintf(stderr, "can't get MTU: %s\n", strerror(errno));
		return -1;
	}

	globals->mtu = ifr.ifr_mtu;

	if (bind(sock, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
		fprintf(stderr, "can't bind\n");
		return -1;
	}

	filter.len = sizeof(BPF) / sizeof(struct sock_filter);
	filter.filter = BPF;

	if (setsockopt(sock, SOL_SOCKET, SO_ATTACH_FILTER,
		       &filter, sizeof(filter)) < 0) {
		fprintf(stderr, "can't attach filter on socket");
		return -1;
	}

	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
	globals->netsock = sock;

	return 0;
}
