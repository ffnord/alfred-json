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
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <ctype.h>
#include "alfred.h"

int alfred_client_request_data(struct globals *globals)
{
	unsigned char buf[MAX_PAYLOAD], *pos;
	struct alfred_request_v0 *request;
	struct alfred_push_data_v0 *push;
	struct alfred_data *data;
	int ret, len, headlen, data_len, i;

	if (unix_sock_open_client(globals, ALFRED_SOCK_PATH))
		return -1;

	request = (struct alfred_request_v0 *)buf;
	len = sizeof(*request);

	request->header.type = ALFRED_REQUEST;
	request->header.version = ALFRED_VERSION;
	request->header.length = htons(1);
	request->requested_type = globals->clientmode_arg;

	ret = write(globals->unix_sock, buf, len);
	if (ret != len)
		fprintf(stderr, "%s: only wrote %d of %d bytes: %s\n",
			__func__, ret, len, strerror(errno));

	headlen = sizeof(*push) + sizeof(*data);
	push = (struct alfred_push_data_v0 *)buf;
	while ((ret = read(globals->unix_sock, buf, headlen)) > 0) {
		/* too short */
		if (ret < headlen)
			break;

		data = push->data;
		data_len = ntohs(data->header.length);

		/* would it fit? it should! */
		if (data_len > (int)(sizeof(buf) - headlen))
			break;

		/* read the data */
		ret = read(globals->unix_sock, buf + headlen, data_len);

		/* again too short */
		if (ret < data_len)
			break;

		pos = data->data;

		printf("{ \"%02x:%02x:%02x:%02x:%02x:%02x\", \"",
		       data->source[0], data->source[1],
		       data->source[2], data->source[3],
		       data->source[4], data->source[5]);
		for (i = 0; i < data_len; i++) {
			if (pos[i] == '"')
				printf("\\\"");
			else if (pos[i] == '\\')
				printf("\\\\");
			else if (!isprint(pos[i]))
				printf("\\x%02x", pos[i]);
			else
				printf("%c", pos[i]);
		}

		printf("\" },\n");
	}

	unix_sock_close(globals);

	return 0;
}

int alfred_client_set_data(struct globals *globals)
{
	unsigned char buf[MAX_PAYLOAD];
	struct alfred_push_data_v0 *push;
	struct alfred_data *data;
	int ret, len;

	if (unix_sock_open_client(globals, ALFRED_SOCK_PATH))
		return -1;

	push = (struct alfred_push_data_v0 *)buf;
	data = push->data;
	len = sizeof(*push) + sizeof(*data);
	while (!feof(stdin)) {
		ret = fread(&buf[len], 1, sizeof(buf) - len, stdin);
		len += ret;

		if (sizeof(buf) == len)
			break;
	}

	push->header.type = ALFRED_PUSH_DATA;
	push->header.version = ALFRED_VERSION;
	push->header.length = htons(len - sizeof(push->header));
	push->tx.id = get_random_id();
	push->tx.seqno = htons(0);

	/* we leave data->source "empty" */
	memset(data->source, 0, sizeof(data->source));
	data->header.type = globals->clientmode_arg;
	data->header.version = globals->clientmode_version;
	data->header.length = htons(len - sizeof(*push) - sizeof(*data));

	ret = write(globals->unix_sock, buf, len);
	if (ret != len)
		fprintf(stderr, "%s: only wrote %d of %d bytes: %s\n",
			__func__, ret, len, strerror(errno));

	unix_sock_close(globals);
	return 0;
}
