/*	@(#)ua68562info.h	1.2	94/04/06 16:46:35 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * parameters for Signetics 68562 duart
 */

extern
struct uart68562info {
	vir_bytes uai_devaddr;
	short     uai_vector;
} uart68562info;
