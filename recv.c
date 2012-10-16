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

int process_alfred_push_data(struct globals *globals, struct ethhdr *ethhdr,
			     struct alfred_packet *packet)
{
	int len, data_len;
	struct alfred_data *data;
	struct dataset *dataset;
	uint8_t *pos;
	
	len = ntohs(packet->length);
	pos = (uint8_t *)(packet + 1);

	while (len > (int)sizeof(*data)) {
		data = (struct alfred_data *)pos;
		data_len = ntohs(data->length);

		/* check if enough data is available */
		if (data_len + sizeof(*data) > len)
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

		dataset->data.length = data_len;
		memcpy(dataset->buf, (data + 1), data_len);

		/* if the sender is also the the source of the dataset, we
		 * got a first hand dataset. */
		if (memcmp(ethhdr->h_source, data->source, ETH_ALEN) == 0)
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

int process_alfred_announce_master(struct globals *globals,
				   struct ethhdr *ethhdr,
				   struct alfred_packet *packet)
{
	struct server *server;

	if (packet->version != ALFRED_VERSION)
		return -1;

	server = hash_find(globals->server_hash, ethhdr->h_source);
	if (!server) {
		server = malloc(sizeof(*server));
		if (!server)
			return -1;

		memcpy(server->address, ethhdr->h_source, ETH_ALEN);

		if (hash_add(globals->server_hash, server)) {
			free(server);
			return -1;
		}
	}

	server->last_seen = time(NULL);
	server->tq = 255;
	/* TODO: update TQ */

	if (globals->opmode == OPMODE_SLAVE)
		set_best_server(globals);

	return 0;
}


int process_alfred_request(struct globals *globals,
			   struct ethhdr *ethhdr,
			   struct alfred_packet *packet)
{
	uint8_t type;
	int len;

	len = ntohs(packet->length);

	if (packet->version != ALFRED_VERSION)
		return -1;

	if (len != 1)
		return -1;

	type = *((uint8_t *)(packet + 1));
	push_data(globals, ethhdr->h_source, SOURCE_SYNCED, type);

	return 0;
}


int recv_alfred_packet(struct globals *globals)
{
	uint8_t buf[9000];
	int length;
	struct ethhdr *ethhdr;
	struct alfred_packet *packet;
	int headsize;

	length = read(globals->netsock, buf, sizeof(buf));
	if (length <= 0) {
		fprintf(stderr, "read from network socket failed: %s\n",
			strerror(errno));
		return -1;
	}

	/* drop too small packets */
	headsize = sizeof(*ethhdr) + sizeof(*packet);
	if (length < headsize)
		return -1;

	ethhdr = (struct ethhdr *)buf;
	packet = (struct alfred_packet *)(ethhdr + 1);

	/* drop packets from ourselves */
	if (memcmp(ethhdr->h_source, globals->hwaddr, ETH_ALEN) == 0)
		return -1;

	/* drop truncated packets */
	if (length - headsize < ((int)ntohs(packet->length)))
		return -1;

	/* drop incompatible packet */
	if (packet->version != ALFRED_VERSION)
		return -1;

	switch (packet->type) {
	case ALFRED_PUSH_DATA:
		process_alfred_push_data(globals, ethhdr, packet);
		break;
	case ALFRED_ANNOUNCE_MASTER:
		process_alfred_announce_master(globals, ethhdr, packet);
		break;
	case ALFRED_REQUEST:
		process_alfred_request(globals, ethhdr, packet);
		break;
	default:
		/* unknown packet type */
		return -1;
	}

	return 0;
}

int send_alfred_packet(struct globals *globals, uint8_t *dest, void *buf,
		       int length)
{
	int ret;
	struct ethhdr *ethhdr;
	char *sendbuf;

	sendbuf = malloc(sizeof(*ethhdr) + length);
	if (!buf)
		return -1;

	ethhdr = (struct ethhdr *)sendbuf;

	memcpy(ethhdr->h_source, globals->hwaddr, ETH_ALEN);
	memcpy(ethhdr->h_dest, dest, ETH_ALEN);
	ethhdr->h_proto = htons(ETH_P_ALFRED);
	memcpy(sendbuf + sizeof(*ethhdr), buf, length);

	ret = write(globals->netsock, sendbuf, sizeof(*ethhdr) + length);

	free(sendbuf);

	return (ret == length);
}
