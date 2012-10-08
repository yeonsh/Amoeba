/*	@(#)data.c	1.2	94/04/07 10:50:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * data.c - this is the initialization for the standard streams
 */

#include	<stdio.h>

struct __iobuf __stdin = {
	0, 0, _IOREAD, 0,
	(unsigned char *)NULL, (unsigned char *)NULL, 
};

struct __iobuf __stdout = {
	0, 1, _IOWRITE, 0,
	(unsigned char *)NULL, (unsigned char *)NULL, 
};

struct __iobuf __stderr = {
	0, 2, _IOWRITE | _IOLBF, 0,
	(unsigned char *)NULL, (unsigned char *)NULL, 
};

FILE *__iotab[FOPEN_MAX] = {
	&__stdin,
	&__stdout,
	&__stderr,
	0
};
