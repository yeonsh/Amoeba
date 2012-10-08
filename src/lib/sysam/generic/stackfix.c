/*	@(#)stackfix.c	1.2	94/04/07 11:18:03 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** stackfix - Fix a stack that is possibly in wrong-endian
** order.
** The routine assumes that both stack and current machine are
** either big- or little-endian, not something else.
**
** Jack Jansen, CWI, May 1989.
*/

#include "amoeba.h"

struct stack {
    uint32 argc;
    uint32 argv;
    uint32 envp;
    uint32 capv;
};

#define FIX(l)	(*(l))=((((*(l))>>24)&0xFF)|(((*(l))>>8)&0xFF00)|(((*(l))<<8)&0xFF0000)|(((*(l))<<24)&0xFF000000))

_stackfix(stackp)
    register struct stack *stackp;
{
    register int i;
    register uint32 *ip;

    if (stackp->argc & 0xffff0000) {
	FIX(&stackp->argc);
	FIX(&stackp->argv);
	FIX(&stackp->envp);
	FIX(&stackp->capv);
	for (i = stackp->argc, ip = (uint32 *) stackp->argv; i > 0; i--, ip++)
	    FIX(ip);
	for (ip = (uint32 *) stackp->envp; *ip; ip++)
	    FIX(ip);
	for (ip = (uint32 *) stackp->capv; *ip; ip++)
	    FIX(ip);
    }
}
