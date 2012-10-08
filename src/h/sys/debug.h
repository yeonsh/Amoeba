/*	@(#)debug.h	1.2	94/04/06 17:17:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

/*
 * This is used mainly in the kernel for different levels of debug which
 * can be set at compile time.  Add a flag -DPRINTF_LEVEL=n (n >= 0) to the
 * compiler call to get the desired level of debugging.
 * Example call to DPRINTF is
 *	DPRINT(2, ("this is the %d error message\n", mesg_num));
 */

#ifdef PRINTF_LEVEL

#define	DPRINTF(x, y)	\
	if ((x) <= PRINTF_LEVEL)	\
	    printf y;			\
	else				\
	    /* do nothing */;

#else

#define DPRINTF(x, y)

#endif /* PRINTF_LEVEL */

#endif /* __DEBUG_H__ */
