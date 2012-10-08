/*	@(#)buffer.h	1.3	94/04/07 14:46:46 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* use of this module:
 *
 * char *res;
 *
 * BufferStart();
 * BufferAdd(c); ... BufferAddString(s); ..
 * if (ok) {
 *     res = BufferResult();
 * } else {
 *     res = BufferFailure();
 * } 
 */

/*
 * Don't touch the following variables!
 */
EXTERN char *BufferP;
EXTERN int BufferLeft;

#define BufferAdd(ch) 		\
{				\
    if (--BufferLeft < 0) {	\
	BufferIncr();		\
    }				\
    *BufferP++ = ch;		\
}

void BufferStart	P((void));
void BufferAddString	P((char *s ));
void BufferIncr		P((void));
char *BufferResult	P((void));
char *BufferFailure	P((void));

