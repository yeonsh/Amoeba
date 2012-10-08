/*	@(#)input.h	1.2	94/04/07 14:51:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Instantiation of the generic input module. */

#define INP_NPUSHBACK	3
#define INP_TYPE	struct f_info
#define INP_VAR		file_info
#define INP_BUFSIZE	4096

#include <inp_pkg.spec>

