/*	@(#)fflush.c	1.2	94/04/07 10:51:35 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fflush.c - flush stream(s)
 */

#include <sys/types.h>
#include <stdio.h>
#include "loc_incl.h"
#include "locking.h"

int   write _ARGS((int d, const char *buf, int nbytes));
off_t lseek _ARGS((int fildes, off_t offset, int whence));

int
fflush(stream)
FILE *stream;
{
	int count, c1, i, result = 0;
	int local_count;

	if (!stream) {
	    for(i= 0; i < FOPEN_MAX; i++)
		if (__iotab[i] && fflush(__iotab[i]))
			result = EOF;
	    return result;
	}

	LOCK(stream, local_count);

	if (!stream->_buf
	    || (!io_testflag(stream, _IOREADING)
		&& !io_testflag(stream, _IOWRITING)))
		RETURN(0);
	if (io_testflag(stream, _IOREADING)) {
		/* (void) fseek(stream, 0L, SEEK_CUR); */
		int adjust = 0;
		if (stream->_buf && !io_testflag(stream,_IONBF))
			adjust = local_count;
		local_count = 0;
		lseek(fileno(stream), (off_t) adjust, SEEK_CUR);
		if (io_testflag(stream, _IOWRITE))
			stream->_flags &= ~(_IOREADING | _IOWRITING);
		stream->_ptr = stream->_buf;
		RETURN(0);
	} else if (io_testflag(stream, _IONBF)) RETURN(0);

	if (io_testflag(stream, _IOREAD))		/* "a" or "+" mode */
		stream->_flags &= ~_IOWRITING;

	count = stream->_ptr - stream->_buf;
	stream->_ptr = stream->_buf;

	if ( count <= 0 )
		RETURN(0);

	if (io_testflag(stream, _IOAPPEND)) {
		if (lseek(fileno(stream), 0L, SEEK_END) == -1) {
			stream->_flags |= _IOERR;
			RETURN(EOF);
		}
	}
	c1 = write(stream->_fd, (char *)stream->_buf, count);

	local_count = 0;

	if ( count == c1 )
		RETURN(0);

	stream->_flags |= _IOERR;
	RETURN(EOF); 
done:
	UNLOCK(stream, local_count);	/* Unlock the stream */
        return result;
}

void
__cleanup()
{
	register int i;

	for(i= 0; i < FOPEN_MAX; i++)
		if (__iotab[i] && io_testflag(__iotab[i], _IOWRITING))
			(void) fflush(__iotab[i]);
}
