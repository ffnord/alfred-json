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

#ifndef SOURCE_VERSION
#define SOURCE_VERSION			"2013.4.0"
#endif

#include <stdint.h>
#include <netinet/ether.h>
#include "packet.h"

#define ALFRED_SOCK_PATH		"/var/run/alfred.sock"

struct globals {
  const struct output_formatter *output_formatter;
  int clientmode_arg;
  int unix_sock;
};

struct output_formatter {
  void* (*prepare)(void);
  void (*push)(void *ctx, unsigned char *id, size_t id_len, unsigned char *data, size_t data_len);
  void (*finalize)(void *ctx);
  void (*cancel)(void *ctx);
};

#define MAX_PAYLOAD ((1 << 16) - 1)

extern const struct output_formatter output_formatter_json;
extern const struct output_formatter output_formatter_string;
extern const struct output_formatter output_formatter_binary;

/* client.c */
int alfred_client_request_data(struct globals *globals);
/* unix_sock.c */
int unix_sock_open_client(struct globals *globals, char *path);
int unix_sock_close(struct globals *globals);
