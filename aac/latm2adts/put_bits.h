/*
 * copyright (c) 2004 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * bitstream writer API
 */

#ifndef AVCODEC_PUT_BITS_H
#define AVCODEC_PUT_BITS_H

#include <stdint.h>
#include <stddef.h>

//#include "config.h"
//#include "libavutil/intreadwrite.h"
//#include "libavutil/avassert.h"
//#include "libavutil/common.h"

typedef struct PutBitContext
{
	uint32_t bit_buf;
	int bit_left;
	uint8_t *buf, *buf_ptr, *buf_end;
	int size_in_bits;
} PutBitContext;

#ifndef AV_WB32
# define AV_WB32(p, darg) do {                \
	unsigned d = (darg);                    \
	((uint8_t*)(p))[3] = (d);               \
	((uint8_t*)(p))[2] = (d)>>8;            \
	((uint8_t*)(p))[1] = (d)>>16;           \
	((uint8_t*)(p))[0] = (d)>>24;           \
} while(0)
#endif
/**
 * Initialize the PutBitContext s.
 *
 * @param buffer the buffer where to put bits
 * @param buffer_size the size in bytes of buffer
 */
static inline void init_put_bits(PutBitContext *s, uint8_t *buffer,
                                 int buffer_size)
{
	if (buffer_size < 0)
	{
		buffer_size = 0;
		buffer      = NULL;
	}

	s->size_in_bits = 8 * buffer_size;
	s->buf          = buffer;
	s->buf_end      = s->buf + buffer_size;
	s->buf_ptr      = s->buf;
	s->bit_left     = 32;
	s->bit_buf      = 0;
}

static inline void put_bits(PutBitContext *s, int n, unsigned int value)
{
	unsigned int bit_buf;
	int bit_left;

	//assert(n <= 31 && value < (1U << n));

	bit_buf  = s->bit_buf;
	bit_left = s->bit_left;

	if (n < bit_left)
	{
		bit_buf     = (bit_buf << n) | value;
		bit_left   -= n;
	}
	else
	{
		bit_buf   <<= bit_left;
		bit_buf    |= value >> (n - bit_left);
		if (3 < s->buf_end - s->buf_ptr)
		{
			AV_WB32(s->buf_ptr, bit_buf);
			s->buf_ptr += 4;
		}
		else
		{
			printf("Internal error, put_bits buffer too small\n");
			//assert(0);
		}
		bit_left   += 32 - n;
		bit_buf     = value;
	}

	s->bit_buf  = bit_buf;
	s->bit_left = bit_left;
}

/**
 * Pad the end of the output stream with zeros.
 */
static inline void flush_put_bits(PutBitContext *s)
{
	if (s->bit_left < 32)
	{
		s->bit_buf <<= s->bit_left;
	}
	while (s->bit_left < 32)
	{
		//assert(s->buf_ptr < s->buf_end);
		*s->buf_ptr++ = s->bit_buf >> 24;
		s->bit_buf  <<= 8;
		s->bit_left  += 8;
	}
	s->bit_left = 32;
	s->bit_buf  = 0;
}


#endif /* AVCODEC_PUT_BITS_H */
