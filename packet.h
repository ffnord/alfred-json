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

/* basic blocks */

/**
 * struct alfred_tlv - Type (Version) Length part of a TLV
 * @type: Type of the data
 * @version: Version of the data
 * @length: Length of the data without the alfred_tlv header
 */
struct alfred_tlv {
	uint8_t type;
	uint8_t version;
	uint16_t length;
} __packed;

/**
 * struct alfred_data - Data block header
 * @source: Mac address of the original source of the data
 * @header: TLV-header for the data
 * @data: "length" number of bytes followed by the header
 */
struct alfred_data {
	uint8_t source[ETH_ALEN];
	struct alfred_tlv header;
	/* flexible data block */
	__extension__ uint8_t data[0];
} __packed;

/**
 * struct alfred_transaction_mgmt - Transaction Mgmt block for multiple packets
 * @id: random identificator used for this transaction
 * @seqno: Number of packet inside a transaction
 */
struct alfred_transaction_mgmt {
	uint16_t id;
	uint16_t seqno;
} __packed;

/**
 * enum alfred_packet_type - Types of packet stored in the main alfred_tlv
 * @ALFRED_PUSH_DATA: Packet is an alfred_push_data_v*
 * @ALFRED_ANNOUNCE_MASTER: Packet is an alfred_announce_master_v*
 * @ALFRED_REQUEST: Packet is an alfred_request_v*
 * @ALFRED_STATUS_TXEND: Transaction was finished by sender
 * @ALFRED_STATUS_ERROR: Error was detected during the transaction
 */
enum alfred_packet_type {
	ALFRED_PUSH_DATA = 0,
	ALFRED_ANNOUNCE_MASTER = 1,
	ALFRED_REQUEST = 2,
	ALFRED_STATUS_TXEND = 3,
	ALFRED_STATUS_ERROR = 4,
};

/* packets */

/**
 * struct alfred_push_data_v0 - Packet to push data blocks to another
 * @header: TLV header describing the complete packet
 * @tx: Transaction identificator and sequence number of packet
 * @data: multiple "alfred_data" blocks of arbitrary size (accumulated size
 *  stored in "header.length")
 *
 * alfred_push_data_v0 packets are always sent using unicast
 */
struct alfred_push_data_v0 {
	struct alfred_tlv header;
	struct alfred_transaction_mgmt tx;
	/* flexible data block */
	__extension__  struct alfred_data data[0];
} __packed;

/**
 * struct alfred_announce_master_v0 - Hello packet sent by an alfred master
 * @header: TLV header describing the complete packet
 *
 * Each alfred daemon running in master mode sends it using multicast. The
 * receiver has to calculate the source using the network header
 */
struct alfred_announce_master_v0 {
	struct alfred_tlv header;
} __packed;

/**
 * struct alfred_request_v0 - Request for a specific type
 * @header: TLV header describing the complete packet
 * @requested_type: data type which is requested
 * @tx_id: random identificator used for this transaction
 *
 * Sent as unicast to the node storing it
 */
struct alfred_request_v0 {
	struct alfred_tlv header;
	uint8_t requested_type;
	uint16_t tx_id;
} __packed;

/**
 * struct alfred_status_v0 - Status info of a transaction
 * @header: TLV header describing the complete packet
 * @tx: Transaction identificator and sequence number of packet
 *
 * The sequence number has a special meaning. Failure status packets use
 * it to store the error code. Success status packets store the number of
 * transfered packets in it.
 *
 * Sent as unicast to the node requesting the data
 */
struct alfred_status_v0 {
	struct alfred_tlv header;
	struct alfred_transaction_mgmt tx;
} __packed;

#define ALFRED_VERSION			0
#define ALFRED_PORT			0x4242
#define ALFRED_MAX_RESERVED_TYPE	64

#endif
