// -*- Mode: C; c-basic-offset: 8; -*-
//
// Copyright (c) 2012 Andrew Tridgell, All Rights Reserved
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//  o Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  o Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//

///
/// @file	mavlink.c
///
/// mavlink reporting code
///

#include <stdarg.h>
#include "radio.h"
#include "packet.h"
#include "timer.h"

extern __xdata uint8_t pbuf[MAX_PACKET_LENGTH];
static __pdata uint8_t seqnum;

/*
#define MAVLINK_MSG_ID_RADIO 166
#define MAVLINK_RADIO_CRC_EXTRA 21
*/

// new RADIO_STATUS common message
#define MAVLINK_MSG_ID_RADIO_STATUS 109
#define MAVLINK_RADIO_STATUS_CRC_EXTRA 185

// use '3D' for 3DRadio
#define RADIO_SOURCE_SYSTEM '3'
#define RADIO_SOURCE_COMPONENT 'D'

/*
 * Calculates the MAVLink checksum on a packet in pbuf[] 
 * and append it after the data
 */
static void mavlink_crc(register uint8_t crc_extra)
{
	register uint8_t length = pbuf[1];
	__pdata uint16_t sum = 0xFFFF;
	__pdata uint8_t i, stoplen;

	stoplen = length + 6;

	// MAVLink 1.0 has an extra CRC seed
	pbuf[length+6] = crc_extra;
	stoplen++;

	i = 1;
	while (i<stoplen) {
		register uint8_t tmp;
		tmp = pbuf[i] ^ (uint8_t)(sum&0xff);
		tmp ^= (tmp<<4);
		sum = (sum>>8) ^ (tmp<<8) ^ (tmp<<3) ^ (tmp>>4);
		i++;
	}

	pbuf[length+6] = sum&0xFF;
	pbuf[length+7] = sum>>8;
}

struct mavlink_RADIO_v10 {
	uint16_t	rxerrors;
	uint16_t	fixed;
	 uint8_t	rssi;
	 uint8_t	remrssi;
	 uint8_t	txbuf;
	 uint8_t	noise;
	 uint8_t	remnoise;
};

static void swap_bytes(__pdata uint8_t ofs, __pdata uint8_t len) __nonbanked
{
	register uint8_t i;
	for (i=ofs; i<ofs+len; i+=2) {
		register uint8_t tmp = pbuf[i];
		pbuf[i] = pbuf[i+1];
		pbuf[i+1] = tmp;
	}
}

/// send a MAVLink status report packet
void MAVLink_report(void)
{
	struct mavlink_RADIO_v10 *m = (struct mavlink_RADIO_v10 *)&pbuf[6];
	pbuf[0] = MAVLINK10_STX;
	pbuf[1] = sizeof(struct mavlink_RADIO_v10);
	pbuf[2] = seqnum++;
	pbuf[3] = RADIO_SOURCE_SYSTEM;
	pbuf[4] = RADIO_SOURCE_COMPONENT;

	m->rxerrors	= errors.rx_errors;
	m->fixed	= errors.corrected_packets;
	m->txbuf	= serial_read_space();
	if(nodeId == 0) {
		m->rssi     = statistics[1].average_rssi;
		m->remrssi  = remote_statistics[1].average_rssi;
        m->noise    = statistics[1].average_noise;
		m->remnoise = remote_statistics[1].average_noise;
	}
	else {
		m->rssi     = statistics[nodeId].average_rssi;
		m->remrssi  = remote_statistics[0].average_rssi;
        m->noise    = statistics[nodeId].average_noise;
		m->remnoise = remote_statistics[0].average_noise;
	}

	// the new RADIO_STATUS common message
	pbuf[5] = MAVLINK_MSG_ID_RADIO_STATUS;
	mavlink_crc(MAVLINK_RADIO_STATUS_CRC_EXTRA);

	if (serial_write_space() < sizeof(struct mavlink_RADIO_v10)+8) {
		// don't cause an overflow
		return;
	}

	serial_write_buf(pbuf, sizeof(struct mavlink_RADIO_v10)+8);
}
