/*	@(#)flushbuf.c	1.3	94/04/07 10:52:23 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * flushbuf.c - flush a buffer
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include "loc_incl.h"
#include "locking.h"

extern int isatty _ARGS((int d));
extern void (*_clean) _ARGS((void));
extern off_t lseek _ARGS((int fildes, off_t offset, int whence));

int
__flushbuf(c, stream)
int c;
FILE *stream;
{
	/* Assertion: stream->_count < 0  (see putc() for proof) */

	int result, local_count;

	/* The decrement of stream->_count (in putc) that took it negative may
	 * have been illegal, because some other thread has the mutex locked,
	 * so repair the damage before any other thread can see it:
	 */
	++stream->_count;
	LOCK(stream, local_count);
	--local_count;			/* get original, lowered value back */

	/* If we now find stream->_count > 0, it can only be because some other
	 * thread beat us to it and flushed the buffer while we were blocked
	 * at the sig_mu_lock.  So act as if we never came into the function at
	 * all:
	 */
	if (local_count >= 0) {
		*stream->_ptr++ = c;
		RETURN(c);
	}

	_clean = __cleanup;
	if (fileno(stream) < 0) RETURN(EOF);
	if (!io_testflag(stream, _IOWRITE)) RETURN(EOF);
	if (io_testflag(stream, _IOREADING) && !feof(stream)) RETURN(EOF);

	stream->_flags &= ~_IOREADING;
	stream->_flags |= _IOWRITING;
	if (!io_testflag(stream, _IONBF)) {
		if (!stream->_buf) {
			if (stream == stdout && isatty(fileno(stdout))) {
				if (!(stream->_buf =
					    (unsigned char *)malloc(BUFSIZ))) {
					stream->_flags |= _IONBF;
				} else {
					stream->_flags |= _IOLBF|_IOMYBUF;
					stream->_bufsiz = BUFSIZ;
					local_count = -1;
				}
			} else {
				if (!(stream->_buf =
					    (unsigned char *)malloc(BUFSIZ))) {
					stream->_flags |= _IONBF;
				} else {
					stream->_flags |= _IOMYBUF;
					stream->_bufsiz = BUFSIZ;
					if (!io_testflag(stream, _IOLBF))
						local_count = BUFSIZ - 1;
					else	local_count = -1;
				}
			}
			stream->_ptr = stream->_buf;
		}
	}

	if (io_testflag(stream, _IONBF)) {
		char c1 = c;

		local_count = 0;
		if (io_testflag(stream, _IOAPPEND)) {
			if (lseek(fileno(stream), 0L, SEEK_END) == -1) {
				stream->_flags |= _IOERR;
				RETURN(EOF);
			}
		}
		if (write(fileno(stream), &c1, 1) != 1) {
			stream->_flags |= _IOERR;
			RETURN(EOF);
		}
		RETURN(c);
	} else if (io_testflag(stream, _IOLBF)) {
		*stream->_ptr++ = c;
		/* stream->_count has been updated in putc macro. */
		if (c == '\n' || local_count == -stream->_bufsiz) {
			int count = -local_count;

			stream->_ptr  = stream->_buf;
			local_count = 0;

			if (io_testflag(stream, _IOAPPEND)) {
				if (lseek(fileno(stream),0L, SEEK_END) == -1) {
					stream->_flags |= _IOERR;
					RETURN(EOF);
				}
			}
			if (write(fileno(stream), (char *)stream->_buf, count)
			    != count) {
				stream->_flags |= _IOERR;
				RETURN(EOF);
			}
		}
	} else {
		int count = stream->_ptr - stream->_buf;

		local_count = stream->_bufsiz - 1;
		stream->_ptr = stream->_buf + 1;

		if (count > 0) {
			if (io_testflag(stream, _IOAPPEND)) {
				if (lseek(fileno(stream),0L, SEEK_END) == -1) {
					stream->_flags |= _IOERR;
					RETURN(EOF);
				}
			}
			if (write(fileno(stream), (char *)stream->_buf, count)
			    != count) {
				*(stream->_buf) = c;
				stream->_flags |= _IOERR;
				RETURN(EOF);
			}
		}
		*(stream->_buf) = c;
	}
	RETURN(c);
done:
	UNLOCK(stream, local_count);	/* Unlocking actions */
	return result;
}
