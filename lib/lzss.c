/*
  LZSS encoder/decoder

  Copyright (C) 2013 Plastic Logic Limited

  Based on original implementation by Haruhiko Okumura

      Guillaume Tucker <guillaume.tucker@plasticlogic.com>

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <lzss.h>
#include <stdlib.h>
#include <string.h>

/* If match length <= P then output one character */
static const unsigned LZSS_P = 1;

static int output1(struct lzss *lzss, struct lzss_io *io, int c);
static int output2(struct lzss *lzss, struct lzss_io *io, int x, int y);
static int output_word(struct lzss *lzss, struct lzss_io *io,
		       int mask, int byte);
static int putbit0(struct lzss *lzss, struct lzss_io *io);
static int putbit1(struct lzss *lzss, struct lzss_io *io);
static int flush_bit_buffer(struct lzss *lzss, struct lzss_io *io);
static int getbit(struct lzss *lzss, int n, struct lzss_io *io);

/* ----------------------------------------------------------------------------
 * public functions
 */

int lzss_init(struct lzss *lzss, unsigned ei, unsigned ej)
{
	lzss->ei = ei;
	lzss->ej = ej;
	lzss->n = 1 << ei;
	lzss->f = ((1 << ej) + LZSS_P);
	lzss->buffer = NULL;
	lzss->in_size = 0;
	lzss->out_size = 0;
	lzss->bit_buffer = 0;
	lzss->bit_mask = 0x80;
	lzss->getbit_buffer = 0;
	lzss->getbit_mask = 0;

	return 0;
}

int lzss_alloc_buffer(struct lzss *lzss)
{
	lzss->buffer = malloc(LZSS_BUFFER_SIZE(lzss->ei));

	if (lzss->buffer == NULL)
		return -1;

	return 0;
}

void lzss_free_buffer(struct lzss *lzss)
{
	free(lzss->buffer);
}

int lzss_encode(struct lzss *lzss, struct lzss_io *io)
{
	int i, j, f1, x, y, r, s, bufferend, c;

	memset(lzss->buffer, 0, (lzss->n - lzss->f));
	lzss->in_size = 0;
	lzss->out_size = 0;

	for (i = lzss->n - lzss->f; i < lzss->n * 2; i++) {
		if ((c = io->rd(io->i)) == EOF)
			break;

		lzss->buffer[i] = c;
		lzss->in_size++;
	}

	bufferend = i;
	r = lzss->n - lzss->f;
	s = 0;

	while (r < bufferend) {
		f1 = (lzss->f <= bufferend - r) ? lzss->f : bufferend - r;
		x = 0;
		y = 1;
		c = lzss->buffer[r];

		for (i = r - 1; i >= s; i--) {
			if (lzss->buffer[i] == c) {
				for (j = 1; j < f1; j++)
					if (lzss->buffer[i + j] !=
					    lzss->buffer[r + j])
						break;
				if (j > y) {
					x = i;
					y = j;
				}
			}
		}

		if (y <= LZSS_P) {
			if (output1(lzss, io, c))
				return -1;
		} else {
			if (output2(lzss, io, x & (lzss->n - 1), y - 2))
				return -1;
		}

		r += y;
		s += y;

		if (r >= (lzss->n * 2) - lzss->f) {
			for (i = 0; i < lzss->n; i++)
				lzss->buffer[i] = lzss->buffer[i + lzss->n];

			bufferend -= lzss->n;
			r -= lzss->n;
			s -= lzss->n;

			while (bufferend < lzss->n * 2) {
				if ((c = io->rd(io->i)) == EOF)
					break;

				lzss->buffer[bufferend++] = c;
				lzss->in_size++;
			}
		}
	}

	flush_bit_buffer(lzss, io);

	return 0;
}

int lzss_decode(struct lzss *lzss, struct lzss_io *io)
{
	int i, j, k, r, c;

	memset(lzss->buffer, 0, (lzss->n - lzss->f));
	r = lzss->n - lzss->f;

	while ((c = getbit(lzss, 1, io)) != EOF) {
		if (c) {
			if ((c = getbit(lzss, 8, io)) == EOF)
				break;

			io->wr(c, io->o);
			lzss->buffer[r++] = c;
			r &= (lzss->n - 1);
		} else {
			if ((i = getbit(lzss, lzss->ei, io)) == EOF)
				break;

			if ((j = getbit(lzss, lzss->ej, io)) == EOF)
				break;

			for (k = 0; k <= j + 1; k++) {
				c = lzss->buffer[(i + k) & (lzss->n - 1)];
				io->wr(c, io->o);
				lzss->buffer[r++] = c;
				r &= (lzss->n - 1);
			}
		}
	}

	return 0;
}

/* ----------------------------------------------------------------------------
 * static functions
 */

static int output1(struct lzss *lzss, struct lzss_io *io, int c)
{
	if (putbit1(lzss, io))
		return -1;

	if (output_word(lzss, io, 0x100, c))
		return -1;

	return 0;
}

static int output2(struct lzss *lzss, struct lzss_io *io, int x, int y)
{
	if (putbit0(lzss, io))
		return -1;

	if (output_word(lzss, io, lzss->n, x))
		return -1;

	if (output_word(lzss, io, (1 << lzss->ej), y))
		return -1;

	return 0;
}

static int output_word(struct lzss *lzss, struct lzss_io *io,
		       int mask, int byte)
{
	while (mask >>= 1) {
		if (byte & mask) {
			if (putbit1(lzss, io))
				return -1;
		} else { 
			if (putbit0(lzss, io))
				return -1;
		}
	}

	return 0;
}

static int putbit0(struct lzss *lzss, struct lzss_io *io)
{
	if (!(lzss->bit_mask >>= 1)) {
		if (io->wr(lzss->bit_buffer, io->o) == EOF)
			return -1;

		lzss->bit_buffer = 0;
		lzss->bit_mask = 0x80;
		lzss->out_size++;
	}

	return 0;
}

static int putbit1(struct lzss *lzss, struct lzss_io *io)
{
	lzss->bit_buffer |= lzss->bit_mask;

	return putbit0(lzss, io);
}

static int flush_bit_buffer(struct lzss *lzss, struct lzss_io *io)
{
	if (lzss->bit_mask != 0x80) {
		if (io->wr(lzss->bit_buffer, io->o) == EOF)
			return -1;

		lzss->out_size++;
	}

	return 0;
}

static int getbit(struct lzss *lzss, int n, struct lzss_io *io)
{
	int x;
	int i;

	x = 0;

	for (i = 0; i < n; i++) {
		if (!lzss->getbit_mask) {
			if ((lzss->getbit_buffer = io->rd(io->i)) == EOF)
				return EOF;

			lzss->getbit_mask = 0x80;
		}

		x <<= 1;

		if (lzss->getbit_buffer & lzss->getbit_mask)
			x++;

		lzss->getbit_mask >>= 1;
	}

	return x;
}
