/*	@(#)pit68230info.h	1.3	94/04/06 16:45:09 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * parameters for Motorola MC68230 IO Timer
 */

extern
struct pit68230info {
	vir_bytes pit_devaddr;
	short     pit_vector;
} pit68230info, pit2_68230info;
