/*
 * Copyright (C) 2012 Simon Wunderlich, 2014 Nils Schneider
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

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include "output.h"
#include "packet.h"
#include "zcat.h"

#ifndef SOURCE_VERSION
#define SOURCE_VERSION	"version unknown"
#endif

#define MAX_PAYLOAD ((1 << 16) - 1)

static void alfred_usage(void)
{
	printf("alfred-json %s\n\n", SOURCE_VERSION);
	printf("Usage: alfred-json -r <data type> [-f <format>] [-z] [-s <socket>]\n\n");
	printf("  -r, --request [data type]   retrieve data from the network\n");
	printf("  -f, --format <format>       output format (\"json\" (default), \"string\" or \"binary\")\n");
	printf("  -s, --socket <path>         path to alfred unix socket\n");
	printf("  -z, --gzip                  enable transparent decompression (GZip)\n");
	printf("  -h, --help                  this help\n");
	printf("\n");
}

int unix_sock_open(const char* path)
{
	int sock;
	struct sockaddr_un addr = {0};

	sock = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sock < 0)
		return perror("Can't create unix socket"), -1;

	addr.sun_family = AF_LOCAL;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		return perror("Can't connect to unix socket"), -1;

	return sock;
}

int request_data(int sock, int request_type, bool gzip,
  const struct output_formatter *output_formatter)
{
	unsigned char buf[MAX_PAYLOAD], *pos;
	struct alfred_request_v0 *request;
	struct alfred_push_data_v0 *push;
	struct alfred_status_v0 *status;
	struct alfred_tlv *tlv;
	struct alfred_data *data;
	void *formatter_ctx;
	int ret, len, data_len;

	request = (struct alfred_request_v0 *)buf;
	len = sizeof(*request);

	request->header.type = ALFRED_REQUEST;
	request->header.version = ALFRED_VERSION;
	request->header.length = sizeof(*request) - sizeof(request->header);
	request->header.length = htons(request->header.length);
	request->requested_type = request_type;
	request->tx_id = random();

	ret = write(sock, buf, len);
	if (ret != len)
		fprintf(stderr, "%s: only wrote %d of %d bytes: %s\n",
			__func__, ret, len, strerror(errno));

	formatter_ctx = output_formatter->prepare();

	push = (struct alfred_push_data_v0 *)buf;
	tlv = (struct alfred_tlv *)buf;
	while ((ret = read(sock, buf, sizeof(*tlv))) > 0) {
		if (ret < (int)sizeof(*tlv))
			break;

		if (tlv->type == ALFRED_STATUS_ERROR)
			goto recv_err;

		if (tlv->type != ALFRED_PUSH_DATA)
			break;

		/* read the rest of the header */
		ret = read(sock, buf + sizeof(*tlv),
			   sizeof(*push) - sizeof(*tlv));

		/* too short */
		if (ret < (int)(sizeof(*push) - (int)sizeof(*tlv)))
			break;

		/* read the rest of the header */
		ret = read(sock, buf + sizeof(*push),
			   sizeof(*data));

		data = push->data;
		data_len = ntohs(data->header.length);

		/* would it fit? it should! */
		if (data_len > (int)(sizeof(buf) - sizeof(*push)))
			break;

		/* read the data */
		ret = read(sock,
			   buf + sizeof(*push) + sizeof(*data), data_len);

		/* again too short */
		if (ret < data_len)
			break;

		pos = data->data;

		unsigned char *buffer = NULL;
		size_t buffer_len = 0;

		if (gzip) {
			/* try decompressing data using GZIP */

			buffer_len = zcat(&buffer, pos, data_len);

			if (buffer_len > 0) {
				pos = buffer;
				data_len = buffer_len;
			}
		}

		output_formatter->push(formatter_ctx, data->source, ETH_ALEN, pos, data_len);

		if (buffer_len > 0)
			free(buffer);
	}

	output_formatter->finalize(formatter_ctx);

	return 0;

recv_err:
	output_formatter->cancel(formatter_ctx);

	/* read the rest of the status message */
	ret = read(sock, buf + sizeof(*tlv), sizeof(*status) - sizeof(*tlv));

	/* too short */
	if (ret < (int)(sizeof(*status) - sizeof(*tlv)))
		return -1;

	status = (struct alfred_status_v0 *)buf;
	fprintf(stderr, "Request failed with %d\n", status->tx.seqno);

	return status->tx.seqno;;
}

int main(int argc, char *argv[])
{
	int request = -1;
	bool gzip = false;
	char *socket_path = "/var/run/alfred.sock";
  struct output_formatter output_formatter = output_formatter_json;

	int opt, opt_ind, i;
	struct option long_options[] = {
		{"request",	required_argument,	NULL,	'r'},
		{"format",	required_argument,	NULL,	'f'},
		{"socket",	required_argument,	NULL,	's'},
		{"gzip",	no_argument,	NULL,	'z'},
		{"help",	no_argument,	NULL,	'h'},
		{NULL,	0,	NULL,	0},
	};

	while ((opt = getopt_long(argc, argv, "r:f:s:hz", long_options, &opt_ind)) != -1) {
		switch (opt) {
		case 'r':
			i = atoi(optarg);
			if (i < 0 || i > 255) {
				fprintf(stderr, "bad data type argument\n");
				exit(1);
			}
			request = i;

			break;
		case 'f':
			if (strncmp(optarg, "json", 4) == 0)
				output_formatter = output_formatter_json;
			else if (strncmp(optarg, "string", 6) == 0)
				output_formatter = output_formatter_string;
			else if (strncmp(optarg, "binary", 6) == 0)
				output_formatter = output_formatter_binary;
			else {
				fprintf(stderr, "Invalid output format!\n");
				exit(1);
			}
			break;
		case 's':
			socket_path = optarg;
			break;
		case 'z':
			gzip = true;
			break;
		case 'h':
		default:
			alfred_usage();
			return 1;
		}
	}

	if (request < 0)
		alfred_usage();
	else {
		int ret, sock;

		sock = unix_sock_open(socket_path);
		if (sock < 0)
			return 1;

		ret = request_data(sock, request, gzip, &output_formatter);
		close(sock);

		return ret;
	}

	return 1;
}
