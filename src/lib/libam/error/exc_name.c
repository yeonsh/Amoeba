/*	@(#)exc_name.c	1.4	94/04/07 10:01:29 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** exc_name - return name of an exception or thread signal.
*/

#include "amoeba.h"
#include "exception.h"
#include "module/strmisc.h"

#ifndef KERNEL
#include <stdio.h>
#endif

struct excname {
	signum	s_exc;
	char	*s_name;
} _excname[] = {
	SIG_TRANS,	"Client-to-server signal",
	EXC_ILL,	"Illegal instruction",
	EXC_ODD,	"Unaligned memory reference",
	EXC_MEM,	"Reference to non-existent memory",
	EXC_BPT,	"Breakpoint or trace trap",
	EXC_INS,	"Undefined instruction",
	EXC_DIV,	"Divide by zero",
	EXC_FPE,	"Floating exception",
	EXC_ACC,	"Memory access control violation",
	EXC_SYS,	"Bad system call or parameters",
	EXC_ARG,	"Illegal instruction operand",
	EXC_EMU,	"System call attempt",
	EXC_ABT,	"Function abort() called",
	EXC_NONE,	"Exception zero",
};

char *
exc_name(num)
    signum num;
{
    struct excname *p = _excname;
    static char buf[64];

    do {
	if (p->s_exc == num)
	    return p->s_name;
	p++;
    } while (p->s_exc != EXC_NONE);

#ifdef KERNEL
    /* We don't want sprintf in the kernel */
    if (num < 0)
	*bprintf(buf, buf+sizeof(buf)-1, "Exception %ld", (long) -num) = '\0';
    else
	*bprintf(buf, buf+sizeof(buf)-1, "Thread signal %ld", (long)num) = '\0';
#else
    if (num < 0)
	(void) sprintf(buf, "Exception %ld", (long) -num);
    else
	(void) sprintf(buf, "Thread signal %ld", (long) num);
#endif /* KERNEL */
    return buf;
}
