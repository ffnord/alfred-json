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

#ifndef SOURCE_VERSION
#define SOURCE_VERSION			"2012.?.?"
#endif

#include <stdint.h>
#include <linux/if_ether.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <time.h>
#include "hash.h"
#define __packed __attribute__ ((packed))

struct alfred_data {
	uint8_t source[ETH_ALEN];
	uint8_t type;
	uint8_t version;
	uint16_t length;
} __packed;

struct alfred_packet {
	uint8_t type;
	uint8_t version;
	uint16_t length;
} __packed;

enum alfred_packet_type {
	ALFRED_PUSH_DATA = 0,
	ALFRED_ANNOUNCE_MASTER = 1,
	ALFRED_REQUEST = 2,
};

#define ALFRED_VERSION			0
#define ALFRED_PORT			0x4242
#define ALFRED_MAX_RESERVED_TYPE	64
#define ALFRED_INTERVAL			10
#define ALFRED_REQUEST_TIMEOUT		1
#define ALFRED_SERVER_TIMEOUT		60
#define ALFRED_DATA_TIMEOUT		600
#define ALFRED_SOCK_PATH		"/var/run/alfred.sock"
#define NO_FILTER			-1

enum data_source {
	SOURCE_LOCAL = 0,
	SOURCE_FIRST_HAND = 1,
	SOURCE_SYNCED = 2,
};

struct alfred_data;

struct dataset {
	struct alfred_data data;
	unsigned char *buf;

	time_t last_seen;
	enum data_source data_source;
	uint8_t local_data;
};

struct server {
	struct ether_addr hwaddr;
	struct in6_addr address;
	time_t last_seen;
	uint8_t tq;
};

enum opmode {
	OPMODE_SLAVE,
	OPMODE_MASTER,
};

enum clientmode {
	CLIENT_NONE,
	CLIENT_REQUEST_DATA,
	CLIENT_SET_DATA,
};

struct globals {
	struct ether_addr hwaddr;
	struct in6_addr address;
	uint32_t scope_id;
	struct server *best_server;	/* NULL if we are a server ourselves */
	char *interface;
	char *mesh_iface;
	enum opmode opmode;
	enum clientmode clientmode;
	int clientmode_arg;
	int clientmode_version;

	int netsock;
	int unix_sock;

	struct hashtable_t *server_hash;
	struct hashtable_t *data_hash;
};

#define debugMalloc(size, num)	malloc(size)
#define debugFree(ptr, num)	free(ptr)

#define ALFRED_HEADLEN		sizeof(struct alfred_packet)

#define MAX_PAYLOAD ((1 << 16) - 1)

extern const struct in6_addr in6addr_localmcast;

/* server.c */
int alfred_server(struct globals *globals);
int set_best_server(struct globals *globals);
/* client.c */
int alfred_client_request_data(struct globals *globals);
int alfred_client_set_data(struct globals *globals);
/* recv.c */
int recv_alfred_packet(struct globals *globals);
/* send.c */
int push_data(struct globals *globals, struct in6_addr *destination,
	      enum data_source max_source_level, int type_filter);
int announce_master(struct globals *globals);
int push_local_data(struct globals *globals);
int sync_data(struct globals *globals);
int send_alfred_packet(struct globals *globals, const struct in6_addr *dest,
		       void *buf, int length);
/* unix_sock.c */
int unix_sock_read(struct globals *globals);
int unix_sock_open_daemon(struct globals *globals, char *path);
int unix_sock_open_client(struct globals *globals, char *path);
int unix_sock_close(struct globals *globals);
/* vis.c */
int vis_update_data(struct globals *globals);
/* netsock.c */
int netsock_open(struct globals *globals);
int netsock_close(int sock);
/* util.c */
int time_diff(struct timeval *tv1, struct timeval *tv2,
	      struct timeval *tvdiff);
