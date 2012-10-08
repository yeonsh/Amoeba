/*	@(#)pipelink.h	1.3	94/04/07 16:03:34 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <cmdreg.h>

/* We steal the last ten commands from the session server's allocation! */

#define PIPE_LINK	(SES_LAST_COM - 10)
#define PIPE_UNLINK	(PIPE_LINK + 1)
