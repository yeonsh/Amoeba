/*	@(#)saksvrdebug.c	1.2	94/04/06 11:53:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	<stdio.h>
#include	"amtools.h"
#include	"sak.h"

#define	SAK_DIR			"/home/sak"

main (argc, argv)
int argc;
char *argv[];
{
	header hh;
	errstat err;
	bufsize result;
	capability scap;

	if (argc != 2) {
		fprintf (stderr, "Usage : %s <joblist|runlist|age>\n", argv[0]);
		exit (-1);
	}

	err = name_lookup (DEF_SAKSVR, &scap);
	if (err != STD_OK) {
		fprintf (stderr, "No cap from %s : %s\n", DEF_SAKSVR,
								err_why (err));
		exit (-1);
	}

	hh.h_port = scap.cap_port;
	hh.h_priv = scap.cap_priv;
	hh.h_command = *argv[1] == 'j' ? SAK_LISTJOBLIST : 
		       *argv[1] == 'a' ? STD_AGE : SAK_LISTRUNLIST;

	result = trans (&hh, NILBUF, (bufsize) 0, &hh, NILBUF, 0);
	if (ERR_STATUS (result)) {
		fprintf (stderr, "trans failed : %s\n",
						err_why (ERR_CONVERT (err)));
		exit (-1);
	}
}
