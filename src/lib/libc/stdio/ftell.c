/*	@(#)ftell.c	1.3	94/04/07 10:53:28 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * ftell.c - obtain the value of the file-position indicator of a stream
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include "loc_incl.h"
#include "locking.h"

extern off_t lseek _ARGS((int fildes, off_t offset, int whence));

long ftell(stream)
FILE *stream;
{
	long result;
	int adjust = 0;
	int local_count;

        LOCK(stream, local_count);	/* Lock the stream */

	if (io_testflag(stream,_IOREADING))
		adjust = -local_count;
	else if (io_testflag(stream,_IOWRITING)
		    && stream->_buf
		    && !io_testflag(stream,_IONBF))
		adjust = stream->_ptr - stream->_buf;
	else adjust = 0;

	result = lseek(fileno(stream), 0, SEEK_CUR);

	if ( result == -1 )
		RETURN(result);

	result += (long) adjust;
	RETURN(result);
done:
        UNLOCK(stream, local_count);	/* Unlock the stream */
        return result;
}
