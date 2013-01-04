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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "alfred.h"

int unix_sock_open_daemon(struct globals *globals, char *path)
{
	struct sockaddr_un addr;

	unlink(path);

	globals->unix_sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (globals->unix_sock < 0) {
		fprintf(stderr, "can't create unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strcpy(addr.sun_path, path);

	if (bind(globals->unix_sock, (struct sockaddr *)&addr,
		 sizeof(addr)) < 0) {
		fprintf(stderr, "can't bind unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	if (listen(globals->unix_sock, 10) < 0) {
		fprintf(stderr, "can't listen on unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

int unix_sock_open_client(struct globals *globals, char *path)
{
	struct sockaddr_un addr;

	globals->unix_sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (globals->unix_sock < 0) {
		fprintf(stderr, "can't create unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_LOCAL;
	strcpy(addr.sun_path, path);

	if (connect(globals->unix_sock, (struct sockaddr *)&addr,
		    sizeof(addr)) < 0) {
		fprintf(stderr, "can't connect to unix socket: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}


int unix_sock_add_data(struct globals *globals, struct alfred_packet *packet)
{
	struct alfred_data *data;
	struct dataset *dataset;
	int len, data_len;

	len = ntohs(packet->length);

	if (len < (int)sizeof(*data))
		return -1;

	data = (struct alfred_data *)(packet + 1);
	data_len = ntohs(data->length);
	memcpy(data->source, globals->hwaddr, sizeof(globals->hwaddr));

	if ((int)(data_len + sizeof(*data)) > len)
		return -1;

	dataset = hash_find(globals->data_hash, data);
	if (!dataset) {
		dataset = malloc(sizeof(*dataset));
		if (!dataset)
			return -1;

		dataset->buf = NULL;

		memcpy(&dataset->data, data, sizeof(*data));
		if (hash_add(globals->data_hash, dataset)) {
			free(dataset);
			return -1;
		}
	}
	dataset->data_source = SOURCE_LOCAL;
	dataset->last_seen = time(NULL);

	/* free old buffer */
	if (dataset->buf)
		free(dataset->buf);

	dataset->buf = malloc(data_len);
	/* that's not good */
	if (!dataset->buf)
		return -1;

	dataset->data.length = data_len;
	memcpy(dataset->buf, (data + 1), data_len);

	return 0;
}


int unix_sock_req_data(struct globals *globals, struct alfred_packet *packet,
		       int client_sock)
{
	struct hash_it_t *hashit = NULL;
	struct timeval tv, last_check, now;
	fd_set fds;
	int ret, len, type;
	uint8_t buf[MAX_PAYLOAD];

	len = ntohs(packet->length);

	if (len != 1)
		return -1;

	/* no server to send the request to, only give back what we have now. */
	if (!globals->best_server)
		goto send_reply;

	/* a master already has data to respond with */
	if (globals->opmode == OPMODE_MASTER)
		goto send_reply;

	send_alfred_packet(globals, globals->best_server->address,
			   packet, sizeof(*packet) + len);

	/* process incoming packets ... */
	FD_ZERO(&fds);
	gettimeofday(&last_check, NULL);

	while (1) {
		gettimeofday(&now, NULL);
		now.tv_sec -= ALFRED_REQUEST_TIMEOUT;
		if (!time_diff(&last_check, &now, &tv))
			break;

		FD_SET(globals->netsock, &fds);

		ret = select(globals->netsock + 1, &fds, NULL, NULL, &tv);

		if (ret == -1) {
			fprintf(stderr, "select failed ...: %s\n",
				strerror(errno));
			return -1;
		}

		if (FD_ISSET(globals->netsock, &fds))
			recv_alfred_packet(globals);
	}

send_reply:

	/* send some data back through the unix socket */

	type = *((uint8_t *)(packet + 1));
	packet = (struct alfred_packet *)buf;
	packet->type = ALFRED_PUSH_DATA;
	packet->version = ALFRED_VERSION;

	while (NULL != (hashit = hash_iterate(globals->data_hash, hashit))) {
		struct dataset *dataset = hashit->bucket->data;
		struct alfred_data *data;

		if (dataset->data.type != type)
			continue;

		data = (struct alfred_data *)(packet + 1);
		memcpy(data, &dataset->data, sizeof(*data));
		data->length = htons(data->length);
		memcpy((data + 1), dataset->buf, dataset->data.length);

		packet->length = htons(dataset->data.length + sizeof(*packet));

		write(client_sock, buf,
		      sizeof(*packet) + sizeof(*data) + dataset->data.length);
	}

	return 0;
}

int unix_sock_read(struct globals *globals)
{
	int client_sock;
	struct sockaddr_un sun_addr;
	socklen_t sun_size = sizeof(sun_addr);
	struct alfred_packet *packet;
	uint8_t buf[MAX_PAYLOAD];
	int length, headsize, ret = -1;

	client_sock = accept(globals->unix_sock, (struct sockaddr *)&sun_addr,
			     &sun_size);
	if (client_sock < 0) {
		fprintf(stderr, "can't accept unix connection: %s\n",
			strerror(errno));
		return -1;
	}

	/* we assume that we can instantly read here. */
	length = read(client_sock, buf, sizeof(buf));
	if (length <= 0) {
		fprintf(stderr, "read from unix socket failed: %s\n",
			strerror(errno));
		goto err;
	}

	/* drop too small packets */
	headsize = sizeof(*packet);
	if (length < headsize)
		goto err;

	packet = (struct alfred_packet *)buf;

	if (length - headsize < ((int)ntohs(packet->length)))
		goto err;

	if (packet->version != ALFRED_VERSION)
		goto err;

	ret = 0;

	switch (packet->type) {
	case ALFRED_PUSH_DATA:
		ret = unix_sock_add_data(globals, packet);
		break;
	case ALFRED_REQUEST:
		ret = unix_sock_req_data(globals, packet, client_sock);
		break;
	default:
		/* unknown packet type */
		ret = -1;
	}
err:
	close(client_sock);
	return ret;
}

int unix_sock_close(struct globals *globals)
{
	close(globals->unix_sock);
	return 0;
}
