/*	@(#)lintlib.c	1.4	94/04/06 09:43:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Lintlibrary for routines in khead.S */

void
abort()		/* go into infinite loop on termination of kernel */
{
    /* these are called from startup but we can't make an entry for startup */
    clearbss();
    main();
}

void
initboard()	/* light all heartbeat leds at back of sun */
{
}

reboot()	/* jump back to monitor */
{
}

/*ARGSUSED*/
setled(a)
int	a;
{
}

#if 0
/*
** the next two are actually assembler labels but so we can use them from
** C we disguise them as characters and take their address where we need
** it.
*/
char	kst_beg;	/* beginning of kernel stack */
char	kst_end;	/* end of kernel stack */
#endif

/* another label marker - from the loader: at the end of bss */
char	end;

/* the int we put at the beginning of bss */
int	begin;
