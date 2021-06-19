/*
 *	crc16.h - CRC-16 routine
 *
 * Implements the standard CRC-16:
 *   Width 16
 *   Poly  0x8005 (x^16 + x^15 + x^2 + 1)
 *   Init  0
 *
 * Copyright (c) 2005 Ben Gardner <bgardner@wabtec.com>
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2. See the file COPYING for more details.
 */

#ifndef LIBROSE_CRC16_H_INCLUDED
#define LIBROSE_CRC16_H_INCLUDED

#include "SDL_types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint16_t const crc16_table[256];

uint16_t crc16(uint16_t  crc, const uint8_t *buffer, size_t len);

static inline uint16_t crc16_byte(uint16_t crc, const uint8_t data)
{
	return (crc >> 8) ^ crc16_table[(crc ^ data) & 0xff];
}

uint16_t cal_crc16(const uint8_t *frame, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __CRC16_H */
