/*
  LZSS encoder/decoder

  Copyright (C) 2013 Plastic Logic Limited

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

#ifndef INCLUDE_LZSS_H
#define INCLUDE_LZSS_H 1

#include <stdio.h>

/**
   @file lzss.h

  This file defines the interface to an LZSS encoding/decoding library.

  It can perform in-memory operations using a user-defined I/O interface, let
  the user provide the working buffer to run without malloc and also provide
  convenience functions to directly process files.
*/

/** Version string */
#define LZSS_VERSION "pldata-008"

/** Size of the lzss dictionary buffer for a given ei value */
#define LZSS_BUFFER_SIZE(ei) ((size_t)((1 << ei) * 2))

/** Standard value for the "ei" parameter */
#define LZSS_STD_EI 10

/** Standard value for the "ej" parameter */
#define LZSS_STD_EJ 4

/** Function type to read a character from the input stream. */
typedef int (*lzss_rd_t)(void *);

/** Function type to write a character to the output stream. */
typedef int (*lzss_wr_t)(int, void *);

/** Input/Output interface to perform encoding/decoding operations */
struct lzss_io {
	lzss_rd_t rd;                /**< function to read a character */
	lzss_wr_t wr;                /**< function to write a character */
	void *i;                     /**< input context passed to rd */
	void *o;                     /**< output context passed to wr */
};

/** LZSS instance

    The dictionary buffer pointer can be set by the caller as it is not
    automatically allocated by lzss_init.
*/
struct lzss {
	char *buffer;                /**< dictionary buffer */
	unsigned ei;                 /**< dictionary buffer size parameter */
	unsigned ej;                 /**< input buffer size parameter */
	size_t n;                    /**< ei ^ 2 */
	size_t f;                    /**< (ej ^ 2) + 1 */
	size_t in_size;              /**< total input size read */
	size_t out_size;             /**< total output size written */
	int bit_buffer;              /**< buffer for output bit operations */
	int bit_mask;                /**< mask for output bit operations */
	int getbit_buffer;           /**< buffer for input bit operations */
	int getbit_mask;             /**< mask for output bit operations */
};

/** Initialise an lzss instance

    The dictionary buffer pointer is initialised to NULL.  The caller can then
    pass a pointer to an arbitrary buffer of the LZSS_BUFFER_SIZE(lzss->ei)
    size, or call lzss_alloc_buffer to allocate it.

    @param[in] lzss pointer to an lzss structure
    @param[in] ei dictionary buffer size parameter
    @param[in] ej input buffer size parameter
    @return -1 if error (i.e. invalid ei and ej values), 0 otherwise
 */
extern int lzss_init(struct lzss *lzss, unsigned ei, unsigned ej);

/** Allocate the dictionary buffer
    @param[in] lzss pointer to an lzss instance
    @return -1 if error (i.e. out of memory), 0 otherwise
 */
extern int lzss_alloc_buffer(struct lzss *lzss);

/** Free the dictionary buffer

    Each call of lzss_init must have a matching call to lzss_free_buffer to
    free the associated memory.

    @param[in] lzss pointer to an lzss instance
*/
extern void lzss_free_buffer(struct lzss *lzss);

/** Encode some data
    @param[in] lzss pointer to an lzss instance
    @param[in] io pointer to an I/O API instance
    @return -1 if error (i.e. I/O error), 0 otherwise
 */
extern int lzss_encode(struct lzss *lzss, struct lzss_io *io);

/** Decode some data
    @param[in] lzss pointer to an lzss instance
    @param[in] io pointer to an I/O API instance
    @return -1 if error (i.e. I/O error), 0 otherwise
 */
extern int lzss_decode(struct lzss *lzss, struct lzss_io *io);

/** Encode a file into an other
    @param[in] lzss pointer to an lzss instance
    @param[in] in_path path to the input file (in original format)
    @param[in] out_path path to the output file (in LZSS format)
    @return -1 if error (i.e. I/O error), 0 otherwise
 */
extern int lzss_encode_file(struct lzss *lzss, const char *in_path,
			    const char *out_path);

/** Decode a file into an other
    @param[in] lzss pointer to an lzss instance
    @param[in] in_path path to the input file (in LZSS format)
    @param[in] out_path path to the output file (in original format)
    @return -1 if error (i.e. I/O error), 0 otherwise
 */
extern int lzss_decode_file(struct lzss *lzss, const char *in_path,
			    const char *out_path);

#endif /* INCLUDE_LZSS_H */
