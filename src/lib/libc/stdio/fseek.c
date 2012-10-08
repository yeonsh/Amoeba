/*	@(#)fseek.c	1.2	94/04/07 10:53:16 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fseek.c - perform an fseek
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include "loc_incl.h"
#include "locking.h"

extern off_t lseek _ARGS((int fildes, off_t offset, int whence));

int
fseek(stream, offset, whence)
FILE *stream;
long offset;
int whence;
{
	int adjust = 0;
	long pos;
	int local_count;

	/* flush the stream before locking it, otherwise we lock ourselves! */
	if (io_testflag(stream,_IOWRITING)) {
		fflush(stream);
	}

	LOCK(stream, local_count);	/* Lock the stream */

	stream->_flags &= ~(_IOEOF | _IOERR);
	/* Clear both the end of file and error flags */

	if (io_testflag(stream, _IOREADING)) {
		if (whence == SEEK_CUR
		    && stream->_buf
		    && !io_testflag(stream,_IONBF))
			adjust = local_count;
		local_count = 0;
	} else {
		/* Not reading, and we have flushed for writing.
		 * The buffer must now be empty.
		 */
	}

	pos = lseek(fileno(stream), offset - adjust, whence);
	if (io_testflag(stream, _IOREAD) && io_testflag(stream, _IOWRITE))
		stream->_flags &= ~(_IOREADING | _IOWRITING);

	stream->_ptr = stream->_buf;

	UNLOCK(stream, local_count);	/* Unlock the stream */
	return ((pos == -1) ? -1 : 0);
}
