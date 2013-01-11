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

#ifndef _ALFRED_PACKET_H
#define _ALFRED_PACKET_H

#define __packed __attribute__ ((packed))

struct alfred_data {
	uint8_t source[ETH_ALEN];
	uint8_t type;
	uint8_t version;
	uint16_t length;
} __packed;

struct alfred_packet {
	uint8_t type;
	uint8_t version;
	uint16_t length;
} __packed;

enum alfred_packet_type {
	ALFRED_PUSH_DATA = 0,
	ALFRED_ANNOUNCE_MASTER = 1,
	ALFRED_REQUEST = 2,
};

#define ALFRED_VERSION			0
#define ALFRED_PORT			0x4242
#define ALFRED_MAX_RESERVED_TYPE	64

#endif
