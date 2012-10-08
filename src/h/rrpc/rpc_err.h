/*	@(#)rpc_err.h	1.1	93/10/07 17:04:02 */
/*
 *
 *
 * Replicated RPC Package
 * Copyright 1992 Mark D. Wood
 * Vrije Universiteit, The Netherlands
 *
 * Previous versions,
 * Copyright 1990, 1991 by Mark D. Wood and the Meta Project, Cornell University
 *
 * Rights to use this source in unmodified form granted for all
 * commercial and research uses.  Rights to develop derivative
 * versions reserved by the authors.
 *
 * Error routines
 */


/* msg_error exports */

void msg_fatal_error _ARGS((char *err));

int check_amoeba_error _ARGS((int code, char *err_msg));

char *program_name;		/* set if you would like this to be printed */
				/* in error messages */
