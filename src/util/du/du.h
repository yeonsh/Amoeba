/*	@(#)du.h	1.2	94/04/07 15:04:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef DU_H
#define DU_H

#include "_ARGS.h"

void fatal   _ARGS((char *format, ...));
void verbose _ARGS((int doprint, char *format, ...));

extern int opt_verbose, opt_debug;

#define SMALL_TIMEOUT	((interval)500)	/* to avoid hanging on dead server */

#endif
