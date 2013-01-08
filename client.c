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
	struct alfred_packet *packet;
	struct alfred_data *data;
	int ret, len, headlen, data_len, i;

	if (unix_sock_open_client(globals, ALFRED_SOCK_PATH))
		return -1;

	packet = (struct alfred_packet *)buf;
	len = sizeof(*packet) + 1;

	packet->type = ALFRED_REQUEST;
	packet->version = ALFRED_VERSION;
	packet->length = htons(1);
	*((uint8_t *)(packet + 1)) = globals->clientmode_arg;

	ret = write(globals->unix_sock, buf, len);
	if (ret != len)
		fprintf(stderr, "%s: only wrote %d of %d bytes: %s\n",
			__func__, ret, len, strerror(errno));

	headlen = sizeof(*packet) + sizeof(*data);
	while ((ret = read(globals->unix_sock, buf, headlen)) > 0) {
		/* too short */
		if (ret < headlen)
			break;

		data = (struct alfred_data *)(packet + 1);
		data_len = ntohs(data->length);

		/* would it fit? it should! */
		if (data_len > (int)(sizeof(buf) - headlen))
			break;

		/* read the data */
		ret = read(globals->unix_sock, buf + headlen, data_len);

		/* again too short */
		if (ret < data_len)
			break;

		pos = (uint8_t *)(data + 1);

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
	struct alfred_packet *packet;
	struct alfred_data *data;
	int ret, len;

	if (unix_sock_open_client(globals, ALFRED_SOCK_PATH))
		return -1;

	packet = (struct alfred_packet *)buf;
	data = (struct alfred_data *)(packet + 1);
	len = sizeof(*packet) + sizeof(*data);
	while (!feof(stdin)) {
		ret = fread(&buf[len], 1, sizeof(buf) - len, stdin);
		len += ret;

		if (sizeof(buf) == len)
			break;
	}

	packet->type = ALFRED_PUSH_DATA;
	packet->version = ALFRED_VERSION;
	packet->length = htons(len - sizeof(*packet));

	/* we leave data->source "empty" */
	memset(data->source, 0, sizeof(data->source));
	data->type = globals->clientmode_arg;
	data->version = globals->clientmode_version;
	data->length = htons(len - sizeof(*packet) - sizeof(*data));


	ret = write(globals->unix_sock, buf, len);
	if (ret != len)
		fprintf(stderr, "%s: only wrote %d of %d bytes: %s\n",
			__func__, ret, len, strerror(errno));

	unix_sock_close(globals);
	return 0;
}


