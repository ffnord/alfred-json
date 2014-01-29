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
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include "alfred.h"

static void alfred_usage(void)
{
	printf("Usage: alfred-json -r <data type> [-f <format>]\n\n");
	printf("  -r, --request [data type]   retrieve data from the network\n");
	printf("  -f, --format <format>       output format (\"json\" (default), \"string\" or \"binary\")\n");
	printf("  -h, --help                  this help\n");
	printf("\n");
}

static struct globals *alfred_init(int argc, char *argv[])
{
	int opt, opt_ind, i;
	struct globals *globals;
	struct option long_options[] = {
		{"request",	required_argument,	NULL,	'r'},
		{"format",	required_argument,	NULL,	'f'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL,		0,			NULL,	0},
	};

	globals = malloc(sizeof(*globals));
	if (!globals)
		return NULL;

	memset(globals, 0, sizeof(*globals));

	globals->output_format = FORMAT_JSON;
	globals->clientmode_arg = -1;

	while ((opt = getopt_long(argc, argv, "r:f:h", long_options,
				  &opt_ind)) != -1) {
		switch (opt) {
		case 'r':
			i = atoi(optarg);
			if (i < ALFRED_MAX_RESERVED_TYPE || i > 255) {
				fprintf(stderr, "bad data type argument\n");
				return NULL;
			}
			globals->clientmode_arg = i;

			break;
		case 'f':
			if (strncmp(optarg, "json", 4) == 0)
				globals->output_format = FORMAT_JSON;
			else if (strncmp(optarg, "string", 6) == 0)
				globals->output_format = FORMAT_STRING;
			else if (strncmp(optarg, "binary", 6) == 0)
				globals->output_format = FORMAT_BINARY;
			break;
		case 'h':
		default:
			alfred_usage();
			return NULL;
		}
	}

	return globals;
}

int request_data(struct globals *globals)
{
	unsigned char buf[MAX_PAYLOAD], *pos;
	const struct output_formatter *formatter;
	struct alfred_request_v0 *request;
	struct alfred_push_data_v0 *push;
	struct alfred_status_v0 *status;
	struct alfred_tlv *tlv;
	struct alfred_data *data;
	void *formatter_ctx;
	int ret, len, data_len;

	switch (globals->output_format) {
		case FORMAT_JSON:
			formatter = &output_formatter_json;
			break;
		case FORMAT_STRING:
			formatter = &output_formatter_string;
			break;
		case FORMAT_BINARY:
			formatter = &output_formatter_binary;
			break;
		default:
			fprintf(stderr, "Invalid output format!\n");
			return 0;
	}

	if (unix_sock_open_client(globals, ALFRED_SOCK_PATH))
		return -1;

	request = (struct alfred_request_v0 *)buf;
	len = sizeof(*request);

	request->header.type = ALFRED_REQUEST;
	request->header.version = ALFRED_VERSION;
	request->header.length = sizeof(*request) - sizeof(request->header);
	request->header.length = htons(request->header.length);
	request->requested_type = globals->clientmode_arg;
	request->tx_id = random();

	ret = write(globals->unix_sock, buf, len);
	if (ret != len)
		fprintf(stderr, "%s: only wrote %d of %d bytes: %s\n",
			__func__, ret, len, strerror(errno));

	formatter_ctx = formatter->prepare();

	push = (struct alfred_push_data_v0 *)buf;
	tlv = (struct alfred_tlv *)buf;
	while ((ret = read(globals->unix_sock, buf, sizeof(*tlv))) > 0) {
		if (ret < (int)sizeof(*tlv))
			break;

		if (tlv->type == ALFRED_STATUS_ERROR)
			goto recv_err;

		if (tlv->type != ALFRED_PUSH_DATA)
			break;

		/* read the rest of the header */
		ret = read(globals->unix_sock, buf + sizeof(*tlv),
			   sizeof(*push) - sizeof(*tlv));

		/* too short */
		if (ret < (int)(sizeof(*push) - (int)sizeof(*tlv)))
			break;

		/* read the rest of the header */
		ret = read(globals->unix_sock, buf + sizeof(*push),
			   sizeof(*data));

		data = push->data;
		data_len = ntohs(data->header.length);

		/* would it fit? it should! */
		if (data_len > (int)(sizeof(buf) - sizeof(*push)))
			break;

		/* read the data */
		ret = read(globals->unix_sock,
			   buf + sizeof(*push) + sizeof(*data), data_len);

		/* again too short */
		if (ret < data_len)
			break;

		pos = data->data;

		formatter->push(formatter_ctx, data->source, ETH_ALEN, pos, data_len);
	}

	formatter->finalize(formatter_ctx);

	unix_sock_close(globals);

	return 0;

recv_err:
	formatter->cancel(formatter_ctx);

	/* read the rest of the status message */
	ret = read(globals->unix_sock, buf + sizeof(*tlv),
		   sizeof(*status) - sizeof(*tlv));

	/* too short */
	if (ret < (int)(sizeof(*status) - sizeof(*tlv)))
		return -1;

	status = (struct alfred_status_v0 *)buf;
	fprintf(stderr, "Request failed with %d\n", status->tx.seqno);

	return status->tx.seqno;;
}

int main(int argc, char *argv[])
{
	struct globals *globals;

	globals = alfred_init(argc, argv);

	if (!globals)
		return 1;

	if (globals->clientmode_arg > 0)
			return request_data(globals);
	else
			alfred_usage();

	return 1;
}
