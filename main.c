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

int main(int argc, char *argv[])
{
	struct globals *globals;

	globals = alfred_init(argc, argv);

	if (!globals)
		return 1;

	if (globals->clientmode_arg > 0)
			return alfred_client_request_data(globals);
	else
			alfred_usage();

	return 1;
}
