/*	@(#)mmuconst.h	1.2	94/04/06 16:48:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define PAGESHIFT	12
#define PAGESIZE	(1<<PAGESHIFT)

#define BTOPG( nb )	( (nb) >> PAGESHIFT )

#define MAXALIGN	8
