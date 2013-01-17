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


static int unix_sock_add_data(struct globals *globals,
			      struct alfred_push_data_v0 *push)
{
	struct alfred_data *data;
	struct dataset *dataset;
	int len, data_len;

	len = ntohs(push->header.length);

	if (len < (int)(sizeof(*push) + sizeof(push->header)))
		return -1;

	/* subtract rest of push header */
	len -= sizeof(*push) - sizeof(push->header);

	if (len < (int)(sizeof(*data)))
		return -1;

	data = push->data;
	data_len = ntohs(data->header.length);
	memcpy(data->source, &globals->hwaddr, sizeof(globals->hwaddr));

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
	clock_gettime(CLOCK_MONOTONIC, &dataset->last_seen);

	/* free old buffer */
	free(dataset->buf);

	dataset->buf = malloc(data_len);
	/* that's not good */
	if (!dataset->buf)
		return -1;

	dataset->data.header.length = data_len;
	memcpy(dataset->buf, data->data, data_len);

	return 0;
}


static int unix_sock_req_data(struct globals *globals,
			      struct alfred_request_v0 *request,
			      int client_sock)
{
	struct hash_it_t *hashit = NULL;
	struct timespec tv, last_check, now;
	fd_set fds;
	int ret, len;
	uint8_t buf[MAX_PAYLOAD];
	struct alfred_push_data_v0 *push;
	uint16_t seqno = 0;
	uint16_t id;
	struct transaction_head search, *head = NULL;
	struct alfred_status_v0 status;

	len = ntohs(request->header.length);

	if (len != (sizeof(*request) - sizeof(request->header)))
		return -1;

	/* no server to send the request to, only give back what we have now. */
	if (!globals->best_server)
		goto send_reply;

	/* a master already has data to respond with */
	if (globals->opmode == OPMODE_MASTER)
		goto send_reply;

	id = ntohs(request->tx_id);
	head = transaction_add(globals, globals->best_server->hwaddr, id);
	if (!head)
		return -1;

	search.server_addr = globals->best_server->hwaddr;
	search.id = id;

	send_alfred_packet(globals, &globals->best_server->address,
			   request, sizeof(*request));

	/* process incoming packets ... */
	FD_ZERO(&fds);
	clock_gettime(CLOCK_MONOTONIC, &last_check);

	while (1) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		now.tv_sec -= ALFRED_REQUEST_TIMEOUT;
		if (!time_diff(&last_check, &now, &tv))
			break;

		FD_SET(globals->netsock, &fds);

		ret = pselect(globals->netsock + 1, &fds, NULL, NULL, &tv,
			      NULL);

		if (ret == -1) {
			fprintf(stderr, "select failed ...: %s\n",
				strerror(errno));
			return -1;
		}

		if (FD_ISSET(globals->netsock, &fds))
			recv_alfred_packet(globals);
	}

	head = transaction_clean(globals, &search);
	if (!head || head->finished != 1) {
		free(head);
		goto reply_error;
	}

send_reply:

	if (globals->opmode != OPMODE_MASTER)
		free(head);

	/* send some data back through the unix socket */

	push = (struct alfred_push_data_v0 *)buf;
	push->header.type = ALFRED_PUSH_DATA;
	push->header.version = ALFRED_VERSION;
	push->tx.id = request->tx_id;

	while (NULL != (hashit = hash_iterate(globals->data_hash, hashit))) {
		struct dataset *dataset = hashit->bucket->data;
		struct alfred_data *data;

		if (dataset->data.header.type != request->requested_type)
			continue;

		data = push->data;
		memcpy(data, &dataset->data, sizeof(*data));
		data->header.length = htons(data->header.length);
		memcpy(data->data, dataset->buf, dataset->data.header.length);

		len = dataset->data.header.length + sizeof(*data);
		push->header.length = htons(len);
		push->tx.seqno = htons(seqno++);

		write(client_sock, buf, sizeof(*push) + len);
	}

	return 0;

reply_error:

	free(head);
	status.header.type = ALFRED_STATUS_ERROR;
	status.header.version = ALFRED_VERSION;
	status.header.length = htons(sizeof(status) - sizeof(status.header));
	status.tx.id = request->tx_id;
	status.tx.seqno = 1;
	write(client_sock, &status, sizeof(status));

	return 0;
}

int unix_sock_read(struct globals *globals)
{
	int client_sock;
	struct sockaddr_un sun_addr;
	socklen_t sun_size = sizeof(sun_addr);
	struct alfred_tlv *packet;
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

	packet = (struct alfred_tlv *)buf;

	if ((length - headsize) < ((int)ntohs(packet->length)))
		goto err;

	if (packet->version != ALFRED_VERSION)
		goto err;

	ret = 0;

	switch (packet->type) {
	case ALFRED_PUSH_DATA:
		ret = unix_sock_add_data(globals,
					 (struct alfred_push_data_v0 *)packet);
		break;
	case ALFRED_REQUEST:
		ret = unix_sock_req_data(globals,
					 (struct alfred_request_v0 *)packet,
					 client_sock);
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
