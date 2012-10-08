/*	@(#)dump.c	1.3	94/04/07 14:49:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "global.h"
#include "idf.h"
#include "Lpars.h"
#include "lexan.h"
#include "dbug.h"
#include "error.h"
#include "dump.h"

PRIVATE int	   IndentLevel = 0;

PUBLIC WriterFunc Write = NULL;

PUBLIC void Incr()	{ IndentLevel++; }
PUBLIC void Decr()	{ IndentLevel--; }
PUBLIC void NL()	{ Write("\n"); }

#ifdef DEBUG

PUBLIC void
DefaultWriter(s)
char *s;
{
    _db_write_("%s", s);
}

#endif /* DEBUG */

PUBLIC void
Indent()
{
    int i;

    for (i = 0; i < IndentLevel; i++) {
	Write("\t");
    }
}

#define NUL	'\0'
#define QUOTE	'\''
#define ID_MAX	256

PUBLIC char *
QuotedId(id)
char *id;
/* return quoted string if it's no identifier, argument otherwise */
{
    static char id_buf[ID_MAX];

    if (IsIdent(id)) {
	return(id);
    } else {
	register char *s, *p = id_buf;

	*p++ = QUOTE;
	for(s = id; *s != NUL && p <= &id_buf[ID_MAX-2]; s++) {
	    if (*s == QUOTE) { /* single quotes are doubled in a string */
		*p++ = QUOTE;
		*p++ = QUOTE;
	    } else {
		*p++ = *s;
	    }
	}
	if (*s != NUL) {
	    FatalError("static buffer overflow in `%s'", id);
	    /* NOTREACHED */
	} else {
	    *p++ = QUOTE;
	    *p = NUL;
	    return(id_buf);
	}
    }
}
	
