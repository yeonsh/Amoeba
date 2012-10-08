/*	@(#)freopen.c	1.2	94/04/07 10:53:04 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * freopen.c - open a file and associate a stream with it
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "loc_incl.h"
#include "locking.h"

#define	PMODE		0666

FILE *
freopen(name, mode, stream)
const char *name;
const char *mode;
FILE *stream;
{
	register int i;
	int rwmode = 0, rwflags = 0;
	int fd, flags = stream->_flags & (_IONBF | _IOFBF | _IOLBF | _IOMYBUF);
	FILE *result;
	int local_count;

	(void) fflush(stream);			/* ignore errors */

	LOCK(stream, local_count);		/* Lock the stream */

	(void) close(fileno(stream));

	switch(*mode++) {
	case 'r':
		flags |= _IOREAD;	
		rwmode = O_RDONLY;
		break;
	case 'w':
		flags |= _IOWRITE;
		rwmode = O_WRONLY;
		rwflags = O_CREAT | O_TRUNC;
		break;
	case 'a': 
		flags |= _IOWRITE | _IOAPPEND;
		rwmode = O_WRONLY;
		rwflags |= O_APPEND | O_CREAT;
		break;         
	default:
		RETURN(NULL);
	}

	while (*mode) {
		switch(*mode++) {
		case 'b':
			continue;
		case '+':
			rwmode = O_RDWR;
			flags |= _IOREAD | _IOWRITE;
			continue;
		/* The sequence may be followed by aditional characters */
		default:
			break;
		}
		break;
	}

	if (rwflags & O_CREAT) {
		fd = open(name, rwmode | rwflags, PMODE);
	} else {
		fd = open(name, rwmode | rwflags);
	}

	if (fd < 0) {
		for( i = 0; i < FOPEN_MAX; i++) {
			if (stream == __iotab[i]) {
				__iotab[i] = 0;
				break;
			}
		}
		if (stream != stdin && stream != stdout && stream != stderr)
			free((_VOIDSTAR)stream);
		RETURN(NULL);
	}
	local_count = 0;
	stream->_fd = fd;
	stream->_flags = flags;
	RETURN(stream);
done:
	UNLOCK(stream, local_count);	/* Unlock the stream */
	return result;
}
