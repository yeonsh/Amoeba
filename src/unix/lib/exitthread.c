/*	@(#)exitthread.c	1.4	96/02/27 11:55:36 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "thread.h"

void
exitthread(ready)
long *ready;
{
    _exit(0);
}
