/*	@(#)fillbuf.c	1.2	94/04/07 10:52:08 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fillbuf.c - fill a buffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "loc_incl.h"
#include "locking.h"

int
__fillbuf(stream)
register FILE *stream;
{
	static unsigned char ch[FOPEN_MAX];
	register int i;
        int result, local_count;

	/* Assertion: stream->_count < 0  (see getc() for proof) */

	/* The decrement of stream->_count (in getc) that took it negative may
	 * have been illegal, because some other thread has the mutex locked,
	 * so repair the damage before any other thread can see it:
	 */
	++stream->_count;
	LOCK(stream, local_count);
	--local_count;			/* get original, lowered value back */

	/* If we now find stream->_count > 0, it can only be because some other
	 * thread beat us to it and filled the buffer while we were blocked
	 * at the sig_mu_lock.  So act as if we never came into the function at
	 * all:
	 */
	if (local_count >= 0) {
		RETURN(*stream->_ptr++);
	}

	local_count = 0;

	if (fileno(stream) < 0) RETURN(EOF);
	if (io_testflag(stream, (_IOEOF | _IOERR ))) RETURN(EOF); 
	if (!io_testflag(stream, _IOREAD)) RETURN(EOF);
	if (io_testflag(stream, _IOWRITING)) RETURN(EOF);

	if (!io_testflag(stream, _IOREADING))
		stream->_flags |= _IOREADING;
	
	if (!io_testflag(stream, _IONBF) && !stream->_buf) {
		stream->_buf = (unsigned char *) malloc(BUFSIZ);
		if (!stream->_buf) {
			stream->_flags |= _IONBF;
		}
		else {
			stream->_flags |= _IOMYBUF;
			stream->_bufsiz = BUFSIZ;
		}
	}

	/* flush line-buffered output when filling an input buffer */
	for (i = 0; i < FOPEN_MAX; i++) {
		if (__iotab[i] && io_testflag(__iotab[i], _IOLBF) &&
		    __iotab[i] != stream) /* don't lock ourselves! */
			if (io_testflag(__iotab[i], _IOWRITING))
				(void) fflush(__iotab[i]);
	}

	if (!stream->_buf) {
		stream->_buf = &ch[fileno(stream)];
		stream->_bufsiz = 1;
	}
	stream->_ptr = stream->_buf;
	local_count = read(stream->_fd, (char *)stream->_buf, stream->_bufsiz);

	if (local_count <= 0){
		if (local_count == 0) {
			stream->_flags |= _IOEOF;
		}
		else 
			stream->_flags |= _IOERR;

		RETURN(EOF);
	}
	local_count--;
	RETURN(*stream->_ptr++);
done:
	UNLOCK(stream, local_count);	/* Unlocking actions */
        return result;
}
