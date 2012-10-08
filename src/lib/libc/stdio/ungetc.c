/*	@(#)ungetc.c	1.2	94/04/07 10:56:17 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * ungetc.c - push a character back onto an input stream
 */

#include <stdio.h>
#include "loc_incl.h"

int
ungetc(ch, stream)
int ch;
FILE *stream;
{
	unsigned char *p;

	if (ch == EOF  || !io_testflag(stream,_IOREADING))
		return EOF;
	if (stream->_ptr == stream->_buf) {
		if (stream->_count != 0) return EOF;
		stream->_ptr++;
	}
	stream->_count++;
	p = --(stream->_ptr);		/* ??? Bloody vax assembler !!! */
	/* ungetc() in sscanf() shouldn't write in rom */
	if (*p != (unsigned char) ch)
		*p = (unsigned char) ch;
	return ch;
}
