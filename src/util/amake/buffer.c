/*	@(#)buffer.c	1.4	94/04/07 14:46:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "global.h"
#include "standlib.h"
#include "alloc.h"
#include "buffer.h"

/*
 * This module may be used to avoid problems of overflowing static buffers.
 */

#define BUFFER_SIZE 1024

PRIVATE char	*BufferText; /* beginning of buffer */
PRIVATE int	 BufferSize; /* no. bytes currently allocated */
PUBLIC char	*BufferP;    /* next free byte */
PUBLIC int	 BufferLeft; /* no. bytes left */

PUBLIC void
BufferStart()
{
    BufferP = BufferText =
	Malloc((unsigned)(BufferSize = BufferLeft = BUFFER_SIZE));
}

PUBLIC void
BufferAddString(s)
char *s;
{
    register int len = strlen(s);

    while (BufferLeft < len + 1) {/* string including '\0' doesn't fit yet */
	BufferIncr();
    }
    (void) strcpy(BufferP, s);
    BufferP += len;
    BufferLeft -= len;
}

PUBLIC void
BufferIncr()
{
    int len = BufferP - BufferText;

    BufferText = Realloc(BufferText, (unsigned)(BufferSize += BUFFER_SIZE));
    BufferP = BufferText + len;
    BufferLeft += BUFFER_SIZE;
}

PUBLIC char *
BufferResult()
{
    BufferText = Realloc(BufferText, (unsigned)(BufferSize -= BufferLeft - 1));
    BufferText[BufferSize - 1] = '\0';
    return(BufferText);
}

PUBLIC char *
BufferFailure()
{
    free(BufferText); BufferText = NULL;
    return(Salloc("", 1));
}
