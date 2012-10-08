/*	@(#)lintlib.c	1.3	94/04/06 08:59:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "fault.h"
#include "sys/proto.h"

/* Functions from as.S */

char	vectab[1];
char	nextvec[1];

#ifdef STACKFRAME_68000	/* for 68020 stackframe simulation on 68000 */
il_68000()
{
}

/*ARGSUSED*/
be_68000(a, b)
long	a;
long	b;
{
}

/*ARGSUSED*/
ae_68000(a, b)
long	a;
long	b;
{
}

#endif /* STACKFRAME_68000 */

/*ARGSUSED*/
void
exception(foo)
int foo;
{
    extern long intcount[];
    void trap();
    void swtrap();

    trap((uint16) 0, (thread_ustate **) 0);
    swtrap((long) 0, (int) 0, (thread_ustate **) 0);
#ifndef NDEBUG
    intcount[0] = 0;
    checkstack();
#endif
}

/*ARGSUSED*/
call_with_trap(rout, arg, result)
long (*rout)();
long arg;
long *result;
{

	return 1;
}

void
trap_to_here()
{
}

long
move_tt1()
{

	return 0;
}

long
fnop()
{

	return 0;
}

#ifndef NOPROC
void ugetreq()
{
    (void) getreq((header *) 0, NILBUF, 0);
}
void uputrep()
{
    putrep((header *) 0, NILBUF, 0);
}
void utrans()
{
    (void) trans((header *) 0, NILBUF, 0, (header *) 0, NILBUF, 0);
}
void uawait()
{
    threadswitch();
    (void) await((event) 0, (interval) 0);
}
void utimeout()
{
    (void) timeout((interval) 0);
}
void ucleanup()
{
    void cleanup();

    cleanup();
}
void usuicide()
{
}
void umuunlock(m)
vir_bytes m;
{
    (void) u_mu_unlock(m);
}
void umutrylock(m, tout)
vir_bytes m;
interval tout;
{
    (void) u_mu_trylock(m, tout);
}
void urpc_getreq()
{
    (void) rpc_getreq((header *) 0, NILBUF, (f_size_t) 0);
}
void urpc_putrep()
{
    (void) rpc_putrep((header *) 0, NILBUF, (f_size_t) 0);
}
void urpc_trans()
{
    (void) rpc_trans((header *) 0, NILBUF, (f_size_t) 0,
				(header *) 0, NILBUF, (f_size_t) 0);
}
void unewsys()
{
    long	newsys();
    long *	usp = 0;
    (void) newsys(usp);
}

/*ARGSUSED*/
start(pc, sp)	/* start a user process */
vir_bytes	pc;
vir_bytes	sp;
{
}

#endif /* NOPROC */

long *
geta6()	/* get contents of register a6 */
{
    return (long *) 0;
}

#ifdef notdef /* the next two aren't used now but maybe in a debugger? */

long
getsr()
{
}
/*ARGSUSED*/
setsr(a)
long a;
{
}

#endif

void
enable() {}

void
disable() {}

void
waitint() {}

/* Routines from as10.S */

/*ARGSUSED*/
tctlb(a, b)
long		a;
unsigned char	b;
{
}
/*ARGSUSED*/
tctll(a, b)
long	a;
long	b;
{
}
/*ARGSUSED*/
int
fctlb(a)
long	a;
{
    return (int) 0;
}
