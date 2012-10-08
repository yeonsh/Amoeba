/*	@(#)assert.c	1.2	96/03/19 13:02:46 */
/* Copyright (c) 1985 Ceriel J.H. Jacobs */

# define _ASSERT_

# include "in_all.h"
# include "assert.h"
# include "output.h"
# include "term.h"

/*
 * Assertion fails. Tell me about it.
 */

VOID
badassertion(ass,f,l) char *ass, *f; {

	clrbline();
	putline("Assertion \"");
	putline(ass);
	putline("\" failed ");
	putline(f);
	putline(", line ");
	prnum((long) l);
	putline(".\r\n");
	flush();
	resettty();
	abort();
}
