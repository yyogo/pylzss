#include <py3c.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/**************************************************************
	LZSS.C -- A Data Compression Program
	(tab = 4 spaces)
***************************************************************
	4/6/1989 Haruhiko Okumura
	Use, distribute, and modify this program freely.
	Please send me your improved versions.
		PC-VAN		SCIENCE
		NIFTY-Serve	PAF01022
		CompuServe	74050,1022
**************************************************************/

/* If match length <= P then output one character */
static const unsigned LZSS_P = 1;

#define N		 4096	/* size of ring buffer */
#define F		   18	/* upper limit for match_length */
#define THRESHOLD	2   /* encode string into position and length
						   if match_length is greater than this */
#define NIL			N	/* index for root of binary search trees */

struct lzss {
	unsigned long int textsize;	/* text size counter */
	unsigned long int codesize;	/* code size counter */
	unsigned long int printcount ;	/* counter for reporting progress every 1K bytes */
	unsigned char text_buf[N + F - 1];	/* ring buffer of size N,
				with extra F-1 bytes to facilitate string comparison
				of longest match.  These are
				set by the lzss_insert_node() procedure. */
	int match_position;
	int match_length;
	int lson[N + 1];
	int rson[N + 257];
	int dad[N + 1];  /* left & right children &
				parents -- These constitute binary search trees. */
};
/* ----------------------------------------------------------------------------
 * public functions
 */
 

typedef int (*lzss_read_func_t)(void *);
typedef int (*lzss_write_func_t)(int, void *);

/** Input/Output interface to perform encoding/decoding operations */
struct lzss_io {
	lzss_read_func_t rd;                /**< function to read a character */
	lzss_write_func_t wr;                /**< function to write a character */
	void *i;                     /**< input context passed to rd */
	void *o;                     /**< output context passed to wr */
};

void lzss_init(struct lzss *ctx)  /* initialize trees */
{
	int  i;

	/* For i = 0 to N - 1, ctx->rson[i] and ctx->lson[i] will be the right and
	   left children of node i.  These nodes need not be initialized.
	   Also, ctx->dad[i] is the parent of node i.  These are initialized to
	   NIL (= N), which stands for 'not used.'
	   For i = 0 to 255, ctx->rson[N + i + 1] is the root of the tree
	   for strings that begin with character i.  These are initialized
	   to NIL.  Note there are 256 trees. */
	ctx->textsize = 0;
	ctx->codesize = 0;
	ctx->printcount = 0;

	for (i = N + 1; i <= N + 256; i++) ctx->rson[i] = NIL;
	for (i = 0; i < N; i++) ctx->dad[i] = NIL;
}


void lzss_insert_node(struct lzss *ctx, int r)
	/* Inserts string of length F, text_buf[r..r+F-1], into one of the
	   trees (text_buf[r]'th tree) and returns the longest-match position
	   and length via the global variables match_position and match_length.
	   If match_length = F, then removes the old node in favor of the new
	   one, because the old one will be deleted sooner.
	   Note r plays double role, as tree node and position in buffer. */
{
	int  i, p, cmp;
	unsigned char  *key;

	cmp = 1;  key = &ctx->text_buf[r];  p = N + 1 + key[0];
	ctx->rson[r] = ctx->lson[r] = NIL;  ctx->match_length = 0;
	for ( ; ; ) {
		if (cmp >= 0) {
			if (ctx->rson[p] != NIL) p = ctx->rson[p];
			else {  ctx->rson[p] = r;  ctx->dad[r] = p;  return;  }
		} else {
			if (ctx->lson[p] != NIL) p = ctx->lson[p];
			else {  ctx->lson[p] = r;  ctx->dad[r] = p;  return;  }
		}
		for (i = 1; i < F; i++)
			if ((cmp = key[i] - ctx->text_buf[p + i]) != 0)  break;
		if (i > ctx->match_length) {
			ctx->match_position = p;
			if ((ctx->match_length = i) >= F)  break;
		}
	}
	ctx->dad[r] = ctx->dad[p];  ctx->lson[r] = ctx->lson[p];  ctx->rson[r] = ctx->rson[p];
	ctx->dad[ctx->lson[p]] = r;  ctx->dad[ctx->rson[p]] = r;
	if (ctx->rson[ctx->dad[p]] == p) ctx->rson[ctx->dad[p]] = r;
	else                   ctx->lson[ctx->dad[p]] = r;
	ctx->dad[p] = NIL;  /* remove p */
}

void lzss_delete_node(struct lzss *ctx, int p)  /* deletes node p from tree */
{
	int  q;
	
	if (ctx->dad[p] == NIL) return;  /* not in tree */
	if (ctx->rson[p] == NIL) q = ctx->lson[p];
	else if (ctx->lson[p] == NIL) q = ctx->rson[p];
	else {
		q = ctx->lson[p];
		if (ctx->rson[q] != NIL) {
			do {  q = ctx->rson[q];  } while (ctx->rson[q] != NIL);
			ctx->rson[ctx->dad[q]] = ctx->lson[q];  ctx->dad[ctx->lson[q]] = ctx->dad[q];
			ctx->lson[q] = ctx->lson[p];  ctx->dad[ctx->lson[p]] = q;
		}
		ctx->rson[q] = ctx->rson[p];  ctx->dad[ctx->rson[p]] = q;
	}
	ctx->dad[q] = ctx->dad[p];
	if (ctx->rson[ctx->dad[p]] == p) ctx->rson[ctx->dad[p]] = q;  else ctx->lson[ctx->dad[p]] = q;
	ctx->dad[p] = NIL;
}

int lzss_encode(struct lzss_io *io, unsigned char initial_buffer_byte_value)
{
	int  i, c, len, r, s, last_match_length, code_buf_ptr;
	unsigned char  code_buf[17], mask;

	struct lzss ctx;
	lzss_init(&ctx);  /* initialize trees */

	code_buf[0] = 0;  /* code_buf[1..16] saves eight units of code, and
		code_buf[0] works as eight flags, "1" representing that the unit
		is an unencoded letter (1 byte), "0" a position-and-length pair
		(2 bytes).  Thus, eight units require at most 16 bytes of code. */
	code_buf_ptr = mask = 1;
	s = 0;  r = N - F;
	for (i = s; i < r; i++) {
		/* Clear the buffer with
		any character that will appear often. */
		ctx.text_buf[i] = initial_buffer_byte_value;
        }

	for (len = 0; len < F && ((c = io->rd(io->i)) != EOF); len++)
		ctx.text_buf[r + len] = c;  /* Read F bytes into the last F bytes of
			the buffer */
	if ((ctx.textsize = len) == 0) return 0;  /* text of size zero */
	for (i = 1; i <= F; i++) lzss_insert_node(&ctx, r - i);  /* Insert the F strings,
		each of which begins with one or more 'space' characters.  Note
		the order in which these strings are inserted.  This way,
		degenerate trees will be less likely to occur. */
	lzss_insert_node(&ctx, r);  /* Finally, insert the whole string just read.  The
		global variables match_length and match_position are set. */
	do {
		if (ctx.match_length > len) ctx.match_length = len;  /* match_length
			may be spuriously long near the end of text. */
		if (ctx.match_length <= THRESHOLD) {
			ctx.match_length = 1;  /* Not long enough match.  Send one byte. */
			code_buf[0] |= mask;  /* 'send one byte' flag */
			code_buf[code_buf_ptr++] = ctx.text_buf[r];  /* Send uncoded. */
		} else {
			code_buf[code_buf_ptr++] = (unsigned char) ctx.match_position;
			code_buf[code_buf_ptr++] = (unsigned char)
				(((ctx.match_position >> 4) & 0xf0)
			  | (ctx.match_length - (THRESHOLD + 1)));  /* Send position and
					length pair. Note match_length > THRESHOLD. */
		}
		if ((mask <<= 1) == 0) {  /* Shift mask left one bit. */
			for (i = 0; i < code_buf_ptr; i++)  /* Send at most 8 units of */
				io->wr(code_buf[i], io->o);   /* code together */
			ctx.codesize += code_buf_ptr;
			code_buf[0] = 0;  code_buf_ptr = mask = 1;
		}
		last_match_length = ctx.match_length;
		for (i = 0; i < last_match_length &&
				((c = io->rd(io->i)) != EOF); i++) {
			lzss_delete_node(&ctx, s);		/* Delete old strings and */
			ctx.text_buf[s] = c;	/* read new bytes */
			if (s < F - 1) ctx.text_buf[s + N] = c;  /* If the position is
				near the end of buffer, extend the buffer to make
				string comparison easier. */
			s = (s + 1) & (N - 1);  r = (r + 1) & (N - 1);
				/* Since this is a ring buffer, increment the position
				   modulo N. */
			lzss_insert_node(&ctx, r);	/* Register the string in text_buf[r..r+F-1] */
		}
		
		while (i++ < last_match_length) {	/* After the end of text, */
			lzss_delete_node(&ctx, s);					/* no need to read, but */
			s = (s + 1) & (N - 1);  r = (r + 1) & (N - 1);
			if (--len) lzss_insert_node(&ctx, r);		/* buffer may not be empty. */
		}
	} while (len > 0);	/* until length of string to be processed is zero */
	if (code_buf_ptr > 1) {		/* Send remaining code. */
		for (i = 0; i < code_buf_ptr; i++) io->wr(code_buf[i], io->o);
		ctx.codesize += code_buf_ptr;
	}
    
    return 0;
	/*printf("In : %ld bytes\n", textsize);
	printf("Out: %ld bytes\n", codesize);
	printf("Out/In: %.3f\n", (double)codesize / textsize);*/
}

int lzss_decode(struct lzss_io *io, unsigned char initial_buffer_byte_value)
{
	int  i, j, k, r, c;
	unsigned int  flags;
	unsigned char text_buf[N + F - 1];
	
	for (i = 0; i < N - F; i++) text_buf[i] = initial_buffer_byte_value;
	r = N - F;  flags = 0;
	for ( ; ; ) {
		if (((flags >>= 1) & 256) == 0) {
			if ((c = io->rd(io->i)) == EOF) break;
			flags = c | 0xff00;		/* uses higher byte cleverly */
		}							/* to count eight */
		if (flags & 1) {
			if ((c = io->rd(io->i)) == EOF) break;
			io->wr(c, io->o);  text_buf[r++] = c;  r &= (N - 1);
		} else {
			if ((i = io->rd(io->i)) == EOF) break;
			if ((j = io->rd(io->i)) == EOF) break;
			i |= ((j & 0xf0) << 4);  j = (j & 0x0f) + THRESHOLD;
			for (k = 0; k <= j; k++) {
				c = text_buf[(i + k) & (N - 1)];
				io->wr(c, io->o);  text_buf[r++] = c;  r &= (N - 1);
			}
		}
	}
    return 0;
}



static PyObject *pylzss_error;

struct pylzss_buffer {
	uint8_t *data;
	size_t length;
	size_t cur;
};

/* -- private functions -- */

static PyObject *pylzss_process(PyObject *m, PyObject *args, PyObject *kw, int compress);
static int pylzss_buf_rd(struct pylzss_buffer *buffer);
static int pylzss_buf_wr(int c, struct pylzss_buffer *buffer);

/* ----------------------------------------------------------------------------
 * module methods
 */

static PyObject *pylzss_compress(PyObject *m, PyObject *args, PyObject *kw)
{
	return pylzss_process(m, args, kw, 1);
}

static PyObject *pylzss_decompress(PyObject *m, PyObject *args, PyObject *kw)
{
	return pylzss_process(m, args, kw, 0);
}

static PyMethodDef pylzss_methods[] = {
	{ "compress", (PyCFunction)pylzss_compress, METH_VARARGS | METH_KEYWORDS,
	  "compress(data)\n\n"
	  "Compress data using LZSS" },
	{ "decompress", (PyCFunction)pylzss_decompress, METH_VARARGS | METH_KEYWORDS,
	  "Decompress LZSS compressed data" },
	{ NULL, NULL, 0, NULL }
};

MODULE_INIT_FUNC(lzss)
{
	PyObject *module;
	static struct PyModuleDef moduledef = {
		PyModuleDef_HEAD_INIT,
		"lzss",
		NULL,
		-1,
		pylzss_methods,
		NULL,
		NULL,
		NULL,
		NULL
	};
	module = PyModule_Create(&moduledef);
	if (!module) return NULL;

	pylzss_error = PyErr_NewException("lzss.error", NULL, NULL);
	Py_INCREF(pylzss_error);
	PyModule_AddObject(module, "error", pylzss_error);
	return module;
}

/* -----------------------------------------------------------------------------
 * private functions
 */

static PyObject *pylzss_process(PyObject *m, PyObject *args,
                                PyObject *kw, int compress)
{
	static char *kwlist[] = { "data", "initial_buffer_value", NULL };
	char *data;
	unsigned char initial_buffer_value = 0x20; /* Use the space char as a default. */
	size_t data_length = 0;

	struct pylzss_buffer ibuf;
	struct pylzss_buffer obuf;
	struct lzss_io io;
	PyObject *output;
	int stat;

	if (!PyArg_ParseTupleAndKeywords(args, kw, "s#c", kwlist,
					 &data, &data_length, &initial_buffer_value))
		return NULL;

	ibuf.data = (uint8_t *)data;
	ibuf.length = data_length;
	ibuf.cur = 0;

	if (compress)
		obuf.length = data_length / 2;
	else
		obuf.length = data_length * 2;

	obuf.cur = 0;
	obuf.data = PyMem_Malloc(obuf.length);

	if (obuf.data == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	io.rd = (lzss_read_func_t)pylzss_buf_rd;
	io.wr = (lzss_write_func_t)pylzss_buf_wr;
	io.i = &ibuf;
	io.o = &obuf;

	if (compress)
		stat = lzss_encode(&io, initial_buffer_value);
	else
		stat = lzss_decode(&io, initial_buffer_value);

	if (stat) {
		PyErr_SetString(pylzss_error, "Failed to process buffer");
		return NULL;
	}

	output = PyBytes_FromStringAndSize((char *)obuf.data, obuf.cur);
	PyMem_Free(obuf.data);

	return output;
}

static int pylzss_buf_rd(struct pylzss_buffer *buffer)
{
	if (buffer->cur == buffer->length)
		return EOF;

	return buffer->data[buffer->cur++];
}

static int pylzss_buf_wr(int c, struct pylzss_buffer *buffer)
{
	if (buffer->cur == buffer->length) {
		buffer->length += buffer->length / 2;
		buffer->data = PyMem_Realloc(buffer->data, buffer->length);
	}

	buffer->data[buffer->cur++] = (uint8_t)c;

	return c;
}
