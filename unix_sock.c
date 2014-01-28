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

int unix_sock_close(struct globals *globals)
{
	close(globals->unix_sock);
	return 0;
}
