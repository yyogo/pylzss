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

#include <lzss.h>

int lzss_encode_file(struct lzss *lzss, const char *in_path,
		     const char *out_path)
{
	struct lzss_io io;
	FILE *infile;
	FILE *outfile;
	int stat = -1;

	infile = fopen(in_path, "rb");

	if (infile == NULL)
		goto exit_now;

	outfile = fopen(out_path, "wb");

	if (outfile == NULL)
		goto exit_close_infile;

	io.rd = (lzss_rd_t)fgetc;
	io.wr = (lzss_wr_t)fputc;
	io.i = infile;
	io.o = outfile;
	stat = lzss_encode(lzss, &io);

	fclose(outfile);
exit_close_infile:
	fclose(infile);
exit_now:
	return stat;
}

int lzss_decode_file(struct lzss *lzss, const char *in_path,
		     const char *out_path)
{
	struct lzss_io io;
	FILE *infile;
	FILE *outfile;
	int stat = -1;

	infile = fopen(in_path, "rb");

	if (infile == NULL)
		goto exit_now;

	outfile = fopen(out_path, "wb");

	if (outfile == NULL)
		goto exit_close_infile;

	io.rd = (lzss_rd_t)fgetc;
	io.wr = (lzss_wr_t)fputc;
	io.i = infile;
	io.o = outfile;
	stat = lzss_decode(lzss, &io);

	fclose(outfile);
exit_close_infile:
	fclose(infile);
exit_now:
	return stat;
}
