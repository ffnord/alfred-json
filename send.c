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

int announce_master(struct globals *globals)
{
	struct alfred_packet announcement;

	announcement.type = ALFRED_ANNOUNCE_MASTER;
	announcement.version = ALFRED_VERSION;
	announcement.length = htons(0);

	send_alfred_packet(globals, &in6addr_localmcast, &announcement,
			   sizeof(announcement));

	return 0;
}

int push_data(struct globals *globals, struct in6_addr *destination,
	      enum data_source max_source_level, int type_filter)
{
	struct hash_it_t *hashit = NULL;
	uint8_t buf[MAX_PAYLOAD];
	struct alfred_packet *packet;
	struct alfred_data *data;
	uint16_t total_length = 0;

	packet = (struct alfred_packet *)buf;
	packet->type = ALFRED_PUSH_DATA;
	packet->version = ALFRED_VERSION;

	while (NULL != (hashit = hash_iterate(globals->data_hash, hashit))) {
		struct dataset *dataset = hashit->bucket->data;

		if (dataset->data_source > max_source_level)
			continue;

		if (type_filter >= 0 && dataset->data.type != type_filter)
			continue;

		/* would the packet be too big? send so far aggregated data
		 * first */
		if (total_length + dataset->data.length + sizeof(*data) >
		    MAX_PAYLOAD - ALFRED_HEADLEN) {
			packet->length = htons(total_length);
			send_alfred_packet(globals, destination, packet,
					   sizeof(*packet) + total_length);
			total_length = 0;
		}

		data = (struct alfred_data *)
		       (buf + sizeof(*packet) + total_length);
		memcpy(data, &dataset->data, sizeof(*data));
		data->length = htons(data->length);
		memcpy((data + 1), dataset->buf, dataset->data.length);

		total_length += dataset->data.length + sizeof(*data);
	}
	/* send the final packet */
	if (total_length) {
		packet->length = htons(total_length);
		send_alfred_packet(globals, destination, packet,
				   sizeof(*packet) + total_length);
	}

	return 0;
}


int sync_data(struct globals *globals)
{
	struct hash_it_t *hashit = NULL;

	/* send local data and data from our clients to (all) other servers */
	while (NULL != (hashit = hash_iterate(globals->server_hash, hashit))) {
		struct server *server = hashit->bucket->data;

		push_data(globals, &server->address, SOURCE_FIRST_HAND,
			  NO_FILTER);
	}
	return 0;
}

int push_local_data(struct globals *globals)
{
	/* no server - yet */
	if (!globals->best_server)
		return -1;

	push_data(globals, &globals->best_server->address, SOURCE_LOCAL,
		  NO_FILTER);

	return 0;
}
