/*	@(#)setvbuf.c	1.2	94/04/07 10:55:27 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * setbuf.c - control buffering of a stream
 */

#include <stdio.h>
#include <stdlib.h>
#include "loc_incl.h"
#include "locking.h"

extern void (*_clean) _ARGS((void));

int
setvbuf(stream, buf, mode, size)
register FILE *stream;
char *buf;
int mode;
size_t size;
{
	int retval = 0, result;
	int local_count;

	LOCK(stream, local_count);	/* Lock this stream */

	_clean = __cleanup;
	if (mode != _IOFBF && mode != _IOLBF && mode != _IONBF)
		RETURN(EOF);

	if (stream->_buf && io_testflag(stream,_IOMYBUF) )
		free((_VOIDSTAR)stream->_buf);

	stream->_flags &= ~(_IOMYBUF | _IONBF | _IOLBF);

	if (!buf && (mode != _IONBF)) {
		if ((buf = (char *) malloc(size)) == NULL) {
			retval = EOF;
		} else {
			stream->_flags |= _IOMYBUF;
		}
	}

	if (io_testflag(stream, _IOREADING) || io_testflag(stream, _IOWRITING))
		retval = EOF;

	stream->_buf = (unsigned char *) buf;

	local_count = 0;
	stream->_flags |= mode;
	stream->_ptr = stream->_buf;

	if (!buf) {
		stream->_bufsiz = 1;
	} else {
		stream->_bufsiz = size;
	}

	RETURN(retval);
done:
	UNLOCK(stream, local_count);	/* Unlock the stream */
	return result;
}
