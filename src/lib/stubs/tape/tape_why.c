/*	@(#)tape_why.c	1.4	96/02/27 11:20:40 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tape_why.c
**	This file contains a library routine which converts a tape error
**	to a ASCII string.
**
** Peter Bosch, 181289, peterb@cwi.nl
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "module/tape.h"

/*
** tape_why return an error code string...
**
*/
static char *tape_error_msg[] = {
	"Invalid error",
	"No tape inserted",
	"Write protected",
	"Command aborted",
	"Hit begin-of-tape",
	"Hit end-of-file",
	"Invalid record size",
	"Position lost",
	"Hit end-of-tape",
	"Media format error",
	};

char *
tape_why(err)
	errstat err;
{
	if (TAPE_FIRST_ERR >= err && err >= TAPE_LAST_ERR) {
		err = TAPE_FIRST_ERR - err;

		if (err > sizeof(tape_error_msg)/sizeof(tape_error_msg[0]))
			return(tape_error_msg[0]);

		return(tape_error_msg[err]);
	}
	else
		return(err_why(err));
}
