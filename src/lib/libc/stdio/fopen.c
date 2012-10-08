/*	@(#)fopen.c	1.2	94/04/07 10:52:31 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fopen.c - open a stream
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "loc_incl.h"
#include <amoeba.h>
#include <module/mutex.h>

#define	PMODE		0666

int open  _ARGS((const char *path, int flags, ...));
int close _ARGS((int d));

FILE *
fopen(name, mode)
const char *name;
const char *mode;
{
	register int i;
	int rwmode = 0, rwflags = 0;
	FILE *stream;
	int fd, flags = 0;

	for (i = 0; __iotab[i] != 0 ; i++) 
		if ( i >= FOPEN_MAX-1 )
			return (FILE *)NULL;

	switch(*mode++) {
	case 'r':
		flags |= _IOREAD | _IOREADING;	
		rwmode = O_RDONLY;
		break;
	case 'w':
		flags |= _IOWRITE | _IOWRITING;
		rwmode = O_WRONLY;
		rwflags = O_CREAT | O_TRUNC;
		break;
	case 'a': 
		flags |= _IOWRITE | _IOWRITING | _IOAPPEND;
		rwmode = O_WRONLY;
		rwflags |= O_APPEND | O_CREAT;
		break;         
	default:
		return (FILE *)NULL;
	}

	while (*mode) {
		switch(*mode++) {
		case 'b':
			continue;
		case '+':
			rwmode = O_RDWR;
			flags |= _IOREAD | _IOWRITE;
			continue;
		/* The sequence may be followed by additional characters */
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

        if (fd < 0) return (FILE *)NULL;

	if (( stream = (FILE *) malloc(sizeof(FILE))) == NULL ) {
		close(fd);
		return (FILE *)NULL;
	}

	if ((flags & (_IOREAD | _IOWRITE))  == (_IOREAD | _IOWRITE))
		flags &= ~(_IOREADING | _IOWRITING);

	stream->_count = 0;
	stream->_fd = fd;
	stream->_flags = flags;
	stream->_buf = NULL;
	mu_init(&stream->_mu);
	__iotab[i] = stream;
	return stream;
}
