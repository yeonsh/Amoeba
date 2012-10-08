/*	@(#)tape_ctrl.c	1.3	96/02/27 13:17:26 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** tape_ctrl.c, a program module which holds the tape control routines.
** tape_unload, unload the tape
**
** peterb@cwi.nl, 120190
**
*/
#include <stdio.h>
#include <amtools.h>
#include <module/tape.h>
#include "commands.h"

/*
** global variables.
**
*/
extern errstat		err;
extern uint8		trans_buf[];
extern int		immediate;
extern int32		skip_count;
extern capability	tape_cap;
extern char *		cmd;

/*
** mtape_unload, unload the tape
**
*/
void
mtape_unload()
{
	if ((err = tape_unload(&tape_cap)) != STD_OK) {
		fprintf(stderr, "%s: tape_unload error: %s\n", 
			cmd, tape_why(err));
		exit(1);
	}
}


/*
** mtape_erase, erase a tape.
**
*/
void
mtape_erase()
{
	if ((err = tape_erase(&tape_cap)) != STD_OK) {
		fprintf(stderr, "%s: tape_erase error: %s\n", 
			cmd, tape_why(err));
		exit(1);
	}
}


/*
** mtape_fskip, skip a number of files.
**
*/
void
mtape_fskip()
{
	if ((err = tape_fskip(&tape_cap, skip_count)) != STD_OK) {
		fprintf(stderr, "%s: tape_fskip error: %s\n", 
			cmd, tape_why(err));
		exit(1);
	}
}


/*
** mtape_load, load the tape...
**
*/
mtape_load()
{
	if ((err = tape_load(&tape_cap)) != STD_OK) {
		fprintf(stderr, "%s: tape_load error: %s\n", 
			cmd, tape_why(err));
		exit(1);
	}
}


/*
** mtape_rewind, rewind the tape.
**
*/
void
mtape_rewind()
{
	if ((err = tape_rewind(&tape_cap, immediate)) != STD_OK) {
		fprintf(stderr, "%s: tape_rewind error: %s\n", 
			cmd, tape_why(err));
		exit(1);
	}
}


/*
** mtape_rskip, skip a number of records on tape.
**
*/
void
mtape_rskip()
{
	if ((err = tape_rskip(&tape_cap, skip_count)) != STD_OK) {
		fprintf(stderr, "%s: tape_rskip error: %s\n", 
			cmd, tape_why(err));
		exit(1);
	}
}


/*
** mtape_status, return a tape unit status.
**
*/
void
mtape_status()
{
	err = tape_status(&tape_cap, (bufptr) trans_buf, T_REQBUFSZ);
	if (err != STD_OK) {
		fprintf(stderr, "%s: tape_status error: %s\n", 
			cmd, tape_why(err));
		exit(1);
	}
	(void) fputs((char *)trans_buf, stdout);
}
