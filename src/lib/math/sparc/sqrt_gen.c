/*	@(#)sqrt_gen.c	1.1	96/02/27 11:13:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Just define the default C version of sqrt(), only call it sqrt_gen().
 * It is called by the fast assembly sqrt() stub when an error is detected.
 */
#define sqrt sqrt_gen
#include "../generic/sqrt.c"
