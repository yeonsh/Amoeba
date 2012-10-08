/*	@(#)vfork.c	1.2	94/04/07 09:55:49 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* vfork(2) system call emulation */

int
vfork()
{
	return fork();
}
