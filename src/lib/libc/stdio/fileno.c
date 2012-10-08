/*	@(#)fileno.c	1.2	94/04/07 10:52:02 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * fileno.c - map a stream to a file descriptor
 */

#include	<stdio.h>

#undef fileno

int
fileno(stream)
FILE *stream;
{
	return stream->_fd;
}
