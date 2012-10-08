/*	@(#)sbrk.c	1.4	96/02/27 10:59:25 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* sbrk(2) and brk(2) system call emulation */

#include "ajax.h"

/* We will allocate only one buffer to handle all sbrk requests.
 * Another, more complicated, solution would be to get our own segment here,
 * and try to grow this when necessary. See _getblk().
 */

#define BIG_CHUNCK	(256 * 1024)	/* 256 Kbytes */

static char
    *bottom = NULL,	/* the buffer used for sbrk() and brk() */
    *top = NULL;	/* the "break", i.e., first free byte available */

char *
sbrk(size)
int size;
{
    if (size < 0) {
	ERR2(EINVAL, "sbrk: negative size", (char *)(-1));
    }

    if (bottom == NULL) {
	/* First time we try to get a big chunck.
	 * From this chunck we allocate the current and all future sbrk()
	 * requests. (The Unix sbrk() semantics require an address space 
	 * without holes.)
	 */
	extern char *_getblk();

	if ((bottom = _getblk((unsigned int) BIG_CHUNCK)) == NULL) {
	    ERR2(ENOMEM, "sbrk: _getblk failure", (char *)(-1));
	} else {
	    top = bottom;	/* nothing allocated yet */
	}
    }
	
    /* See if there is room between top and bottom + BIG_CHUNCK */
    if (size < (bottom + BIG_CHUNCK) - top) {
	char *orig = top;

	top += size;
	return(orig);	/* return previous value of the "break" */
    } else {
	ERR2(ENOMEM, "sbrk: out of memory", (char *)(-1));
    }
}

int
brk(ptr)
char *ptr;
{
    /* Only allow brk() to set the "break" when we have already done
     * the _getblk(), and when `ptr' points in the buffer.
     */

    if (bottom != NULL && (bottom <= ptr) && (ptr < bottom + BIG_CHUNCK)) {
	top = ptr;
	return 0;
    } else {
	ERR(EINVAL, "brk: not within _getblk area");
    }
}
