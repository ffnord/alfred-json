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
#include <arpa/inet.h>
#include "alfred.h"
#include "batadv_query.h"

static int process_alfred_push_data(struct globals *globals,
				    struct in6_addr *source,
				    struct alfred_push_data_v0 *push)
{
	int len, data_len;
	struct alfred_data *data;
	struct dataset *dataset;
	uint8_t *pos;
	struct ether_addr mac;
	int ret;

	ret = ipv6_to_mac(source, &mac);
	if (ret < 0)
		goto err;

	len = ntohs(push->header.length);
	pos = (uint8_t *)push->data;

	while (len > (int)sizeof(*data)) {
		data = (struct alfred_data *)pos;
		data_len = ntohs(data->header.length);

		/* check if enough data is available */
		if ((int)(data_len + sizeof(*data)) > len)
			break;

		dataset = hash_find(globals->data_hash, data);
		if (!dataset) {
			dataset = malloc(sizeof(*dataset));
			if (!dataset)
				goto err;

			dataset->buf = NULL;
			dataset->data_source = SOURCE_SYNCED;

			memcpy(&dataset->data, data, sizeof(*data));
			if (hash_add(globals->data_hash, dataset)) {
				free(dataset);
				goto err;
			}
		}
		/* don't overwrite our own data */
		if (dataset->data_source == SOURCE_LOCAL)
			goto skip_data;

		dataset->last_seen = time(NULL);

		/* free old buffer */
		if (dataset->buf)
			free(dataset->buf);
		dataset->buf = malloc(data_len);

		/* that's not good */
		if (!dataset->buf)
			goto err;

		dataset->data.header.length = data_len;
		memcpy(dataset->buf, data->data, data_len);

		/* if the sender is also the the source of the dataset, we
		 * got a first hand dataset. */
		if (memcmp(&mac, data->source, ETH_ALEN) == 0)
			dataset->data_source = SOURCE_FIRST_HAND;
		else
			dataset->data_source = SOURCE_SYNCED;
skip_data:
		pos += (sizeof(*data) + data_len);
		len -= (sizeof(*data) + data_len);
	}
	return 0;
err:
	return -1;
}

static int
process_alfred_announce_master(struct globals *globals,
			       struct in6_addr *source,
			       struct alfred_announce_master_v0 *announce)
{
	struct server *server;
	struct ether_addr *macaddr;
	struct ether_addr mac;
	int ret;
	int len;

	len = ntohs(announce->header.length);

	ret = ipv6_to_mac(source, &mac);
	if (ret < 0)
		return -1;

	if (announce->header.version != ALFRED_VERSION)
		return -1;

	if (len != (sizeof(*announce) - sizeof(announce->header)))
		return -1;

	server = hash_find(globals->server_hash, &mac);
	if (!server) {
		server = malloc(sizeof(*server));
		if (!server)
			return -1;

		memcpy(&server->hwaddr, &mac, ETH_ALEN);
		memcpy(&server->address, source, sizeof(*source));

		if (hash_add(globals->server_hash, server)) {
			free(server);
			return -1;
		}
	}

	server->last_seen = time(NULL);
	if (strcmp(globals->mesh_iface, "none") != 0) {
		macaddr = translate_mac(globals->mesh_iface,
					(struct ether_addr *)&server->hwaddr);
		if (macaddr)
			server->tq = get_tq(globals->mesh_iface, macaddr);
		else
			server->tq = 0;
	} else {
		server->tq = 255;
	}

	if (globals->opmode == OPMODE_SLAVE)
		set_best_server(globals);

	return 0;
}


static int process_alfred_request(struct globals *globals,
				  struct in6_addr *source,
				  struct alfred_request_v0 *request)
{
	int len;

	len = ntohs(request->header.length);

	if (request->header.version != ALFRED_VERSION)
		return -1;

	if (len != (sizeof(*request) - sizeof(request->header)))
		return -1;

	push_data(globals, source, SOURCE_SYNCED, request->requested_type);

	return 0;
}


int recv_alfred_packet(struct globals *globals)
{
	uint8_t buf[MAX_PAYLOAD];
	ssize_t length;
	struct alfred_tlv *packet;
	struct sockaddr_in6 source;
	socklen_t sourcelen;

	sourcelen = sizeof(source);
	length = recvfrom(globals->netsock, buf, sizeof(buf), 0,
			  (struct sockaddr *)&source, &sourcelen);
	if (length <= 0) {
		fprintf(stderr, "read from network socket failed: %s\n",
			strerror(errno));
		return -1;
	}

	packet = (struct alfred_tlv *)buf;

	/* drop packets not sent over link-local ipv6 */
	if (!is_ipv6_eui64(&source.sin6_addr))
		return -1;

	/* drop packets from ourselves */
	if (0 == memcmp(&source.sin6_addr, &globals->address,
			sizeof(source.sin6_addr)))
		return -1;

	/* drop truncated packets */
	if (length < ((int)ntohs(packet->length)))
		return -1;

	/* drop incompatible packet */
	if (packet->version != ALFRED_VERSION)
		return -1;

	switch (packet->type) {
	case ALFRED_PUSH_DATA:
		process_alfred_push_data(globals, &source.sin6_addr,
					 (struct alfred_push_data_v0 *)packet);
		break;
	case ALFRED_ANNOUNCE_MASTER:
		process_alfred_announce_master(globals, &source.sin6_addr,
					       (struct alfred_announce_master_v0 *)packet);
		break;
	case ALFRED_REQUEST:
		process_alfred_request(globals, &source.sin6_addr,
				       (struct alfred_request_v0 *)packet);
		break;
	default:
		/* unknown packet type */
		return -1;
	}

	return 0;
}
