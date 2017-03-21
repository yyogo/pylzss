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

#include <Python.h>
#include "lzss.h"

#ifndef uint8_t
typedef unsigned char uint8_t;
#endif

static PyObject *pylzss_error;

struct pylzss_buffer {
	uint8_t *data;
	size_t length;
	size_t cur;
};

/* -- private functions -- */

static PyObject *pylzss_encode_decode(PyObject *m, PyObject *args,
				      PyObject *kw, int encode);
static PyObject *pylzss_encode_decode_file(PyObject *m, PyObject *args,
					   PyObject *kw, int encode);
static int pylzss_init_lzss(struct lzss *lzss, int ei, int ej);
static void pylzss_free_lzss(struct lzss *lzss);
static int pylzss_rd(struct pylzss_buffer *buffer);
static int pylzss_wr(int c, struct pylzss_buffer *buffer);

/* ----------------------------------------------------------------------------
 * module methods
 */

static PyObject *pylzss_encode(PyObject *m, PyObject *args, PyObject *kw)
{
	return pylzss_encode_decode(m, args, kw, 1);
}

static PyObject *pylzss_decode(PyObject *m, PyObject *args, PyObject *kw)
{
	return pylzss_encode_decode(m, args, kw, 0);
}

static PyObject *pylzss_encode_file(PyObject *m, PyObject *args, PyObject *kw)
{
	return pylzss_encode_decode_file(m, args, kw, 1);
}

static PyObject *pylzss_decode_file(PyObject *m, PyObject *args, PyObject *kw)
{
	return pylzss_encode_decode_file(m, args, kw, 0);
}

static PyMethodDef pylzss_methods[] = {
	{ "encode", (PyCFunction)pylzss_encode, METH_VARARGS | METH_KEYWORDS,
	  "encode(data[, ei[, ej]])\n\n"
	  "Encode an original string into LZSS" },
	{ "decode", (PyCFunction)pylzss_decode, METH_VARARGS | METH_KEYWORDS,
	  "Decode an LZSS string into the original" },
	{ "encode_file", (PyCFunction)pylzss_encode_file,
	  METH_VARARGS | METH_KEYWORDS,
	  "Encode an original file using LZSS into another one." },
	{ "decode_file", (PyCFunction)pylzss_decode_file,
	  METH_VARARGS | METH_KEYWORDS,
	  "Decode a LZSS compressed file into another original one." },
	{ NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC initlzss(void)
{
	PyObject *m;

	m = Py_InitModule("lzss", pylzss_methods);

	if (m == NULL)
		return;

	pylzss_error = PyErr_NewException("lzss.error", NULL, NULL);
	Py_INCREF(pylzss_error);
	PyModule_AddObject(m, "error", pylzss_error);
}

/* -----------------------------------------------------------------------------
 * private functions
 */

static PyObject *pylzss_encode_decode(PyObject *m, PyObject *args,
				      PyObject *kw, int encode)
{
	static char *kwlist[] = { "data", "ei", "ej", NULL };
	char *data;
	size_t data_length = 0;
	int ei = LZSS_STD_EI;
	int ej = LZSS_STD_EJ;

	struct lzss lzss;
	struct pylzss_buffer ibuf;
	struct pylzss_buffer obuf;
	struct lzss_io io;
	PyObject *output;
	int stat;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "s#|ii", kwlist,
					 &data, &data_length, &ei, &ej))
		return NULL;

	if (pylzss_init_lzss(&lzss, ei, ej))
		return NULL;

	ibuf.data = (uint8_t *)data;
	ibuf.length = data_length;
	ibuf.cur = 0;

	if (encode)
		obuf.length = data_length / 2;
	else
		obuf.length = data_length * 2;

	obuf.cur = 0;
	obuf.data = PyMem_Malloc(obuf.length);

	if (obuf.data == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	io.rd = (lzss_rd_t)pylzss_rd;
	io.wr = (lzss_wr_t)pylzss_wr;
	io.i = &ibuf;
	io.o = &obuf;

	if (encode)
		stat = lzss_encode(&lzss, &io);
	else
		stat = lzss_decode(&lzss, &io);

	if (stat) {
		PyErr_SetString(pylzss_error, "Failed to process buffer");
		return NULL;
	}

	output = PyString_FromStringAndSize((char *)obuf.data, obuf.cur);
	PyMem_Free(obuf.data);
	pylzss_free_lzss(&lzss);

	return output;
}

static PyObject *pylzss_encode_decode_file(PyObject *m, PyObject *args,
					   PyObject *kw, int encode)
{
	static char *pylzss_kwlist[] = {
		"in_path", "out_path", "ei", "ej", NULL };
	char *in_path;
	char *out_path;
	int ei = LZSS_STD_EI;
	int ej = LZSS_STD_EJ;

	struct lzss lzss;
	int stat;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "ss|ii", pylzss_kwlist,
					 &in_path, &out_path, &ei, &ej))
		return NULL;

	if (pylzss_init_lzss(&lzss, ei, ej))
		return NULL;

	if (encode)
		stat = lzss_encode_file(&lzss, in_path, out_path);
	else
		stat = lzss_decode_file(&lzss, in_path, out_path);

	PyMem_Free(lzss.buffer);

	if (stat) {
		PyErr_SetString(pylzss_error, "Failed to process LZSS file");
		return NULL;
	}

	Py_RETURN_NONE;
}

static int pylzss_init_lzss(struct lzss *lzss, int ei, int ej)
{
	if (lzss_init(lzss, ei, ej)) {
		PyErr_SetString(pylzss_error, "Failed to initialise lzss");
		return -1;
	}

	lzss->buffer = PyMem_Malloc(LZSS_BUFFER_SIZE(lzss->ei));

	if (lzss->buffer == NULL) {
		PyErr_NoMemory();
		return -1;
	}

	return 0;
}

static void pylzss_free_lzss(struct lzss *lzss)
{
	PyMem_Free(lzss->buffer);
}

static int pylzss_rd(struct pylzss_buffer *buffer)
{
	if (buffer->cur == buffer->length)
		return EOF;

	return buffer->data[buffer->cur++];
}

static int pylzss_wr(int c, struct pylzss_buffer *buffer)
{
	if (buffer->cur == buffer->length) {
		buffer->length += buffer->length / 2;
		buffer->data = PyMem_Realloc(buffer->data, buffer->length);
	}

	buffer->data[buffer->cur++] = (uint8_t)c;

	return c;
}
