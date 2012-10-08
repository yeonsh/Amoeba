/*	@(#)misc.h	1.2	94/04/06 11:58:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef MISC_H
#define MISC_H

extern void destroy_file _ARGS((capability *filecap));

extern void clear_caps	 _ARGS((capability *caplist, int ncaps));
extern int  caps_zero	 _ARGS((capability *caplist, int ncaps));

#endif
