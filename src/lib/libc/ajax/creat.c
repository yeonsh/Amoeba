/*	@(#)creat.c	1.2	94/04/07 10:25:03 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* creat(2) system call emulation */

#include "ajax.h"
#include "fdstuff.h"

#ifdef __STDC__
int creat(const char *file, mode_t mode)
#else
int
creat(file, mode)
char *file;
mode_t mode;
#endif
{
	return open(file, O_WRONLY | O_CREAT | O_TRUNC, mode);
}
