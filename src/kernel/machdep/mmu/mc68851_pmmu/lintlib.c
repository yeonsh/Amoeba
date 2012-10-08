/*	@(#)lintlib.c	1.4	94/04/06 09:41:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Lintlibrary for routines in khead.S */
#include "amoeba.h"
#include "mc68851.h"

void
abort()		/* go into infinite loop on termination of kernel */
{
    /* these are called from startup but we can't make an entry for startup */
    clearbss();
    main();
}

/*
** the next two are actually assembler labels but so we can use them from
** C we disguise them as characters and take there address where we need
** them.
*/
char	kst_beg;	/* beginning of kernel stack */
char	kst_end;	/* end of kernel stack */

/* another label marker - from the loader: at the end of bss */
char	end;

/* the int we put at the beginning of bss */
int	begin;

/* the int we put at the beginning of text */
int textbegin;

/* PMMU commands */

/*ARGSUSED*/
PsetTc(a)
long a;
{
}

#ifndef NOPROC

/*ARGSUSED*/
PsetSrp(a)
struct pm_rpr *	a;
{
}

/*ARGSUSED*/
PsetCrp(a)
struct pm_rpr *	a;
{
}

/*ARGSUSED*/
Pflushr(a)
struct pm_rpr *	a;
{
}

long
PgetTc()
{
    return (long) 0;
}

/*ARGSUSED*/
Ptest(addr, fc, dp)
char *	addr;
int	fc;
long **	dp;
{
    return (int) 0;
}

/*ARGSUSED*/
Ploadw(a)
char *	a;
{
}
#endif /* NOPROC */
