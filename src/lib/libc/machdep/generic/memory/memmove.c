/*	@(#)memmove.c	1.3	94/04/07 10:37:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	This version of memmove does memory moves for when there is a an
**	overlap of src and dst and handles the RISC 4 byte alignment
**	restrictions.  We refer to 2 Byte units as "SHORT" and 4 bytes as
**	LONG. Pn in the macro names refers to Plus n bytes that have to be
**	moved initially.
**	If you have a machine like the mc680x0 which allows 2 byte aligned
**	long integer access then you can defined TWO_BYTE_ALIGN
*/
#include <string.h>

#if defined(vax) || defined(mc68000) || defined(i80386)
#define	TWO_BYTE_ALIGN 1
#endif

#if defined(TWO_BYTE_ALIGN)

#define	LONGP3	0
#define	LONGP2	1
#define	LONGP1	2
#define	LONG	3
#define	SHORTP1	LONGP1 
#define	SHORT	LONG
#define	BYTE	6

#else

#define	LONGP3	0
#define	LONGP2	1
#define	LONGP1	2
#define	LONG	3
#define	SHORTP1	4 
#define	SHORT	5
#define	BYTE	6

#endif

static char aligntype[4][4] = {
	{ LONG, BYTE, SHORT, BYTE },
	{ BYTE, LONGP3, BYTE, SHORTP1 },
	{ SHORT, BYTE, LONGP2, BYTE },
	{ BYTE, SHORTP1, BYTE, LONGP1 }
};


#ifndef __STDC__
#define const	/**/
#endif

_VOIDSTAR
memmove(dest, source, len)
_VOIDSTAR	dest;
const _VOIDSTAR	source;
size_t		len;
{
    register char *	src = (char *) source;
    register char *	dst = (char *) dest;

    if (src <= dst) /* then we have to copy backwards */
    {
	src += len;
	dst += len;
	if (len >= 3)
	{
	    switch (aligntype[((long) src) & 3][((long) dst) & 3])
	    {
	    case LONGP1:
		*--dst = *--src;
		len--;
	    case LONGP2:
		*--dst = *--src;
		len--;
	    case LONGP3:
		*--dst = *--src;
		len--;
	    case LONG:
		{
		    register int x = len >> 2;

		    while (--x >= 0)
		    {
			dst -= 4;
			src -= 4;
			*((long *) dst) = *((long *) src);
		    }
		    len &= 3;
		}
		break;

#if ! defined(TWO_BYTE_ALIGN)
	    case SHORTP1:
		*--dst = *--src;
		len--;
	    case SHORT:
		{
		    register int x = len >> 1;
		    while (--x >= 0)
		    {
			dst -= 2;
			src -= 2;
			*((short *) dst) = *((short *) src);
		    }
		    len &= 1;
		}
		break;
#endif
	    }
	}

    /* now we handle the BYTE case */
	if (len != 0)
	    do
		*--dst = *--src;
	    while (--len != 0);
    }
    else /* we can copy forwards */
    {
	if (len >= 3)
	{
	    switch (aligntype[((long) src) & 3][((long) dst) & 3])
	    {
	    case LONGP3:
		*dst++ = *src++;
		len--;
	    case LONGP2:
		*dst++ = *src++;
		len--;
	    case LONGP1:
		*dst++ = *src++;
		len--;
	    case LONG:
		{
		    register int x = len >> 2;

		    while (--x >= 0)
		    {
			*((long *) dst) = *((long *) src);
			dst += 4;
			src += 4;
		    }
		    len &= 3;
		}
		break;

#if ! defined(TWO_BYTE_ALIGN)
	    case SHORTP1:
		*dst++ = *src++;
		len--;
	    case SHORT:
		{
		    register int x = len >> 1;
		    while (--x >= 0)
		    {
			*((short *) dst) = *((short *) src);
			dst += 2;
			src += 2;
		    }
		    len &= 1;
		}
		break;
#endif
	    }
	}

    /* now we handle the BYTE case */
	if (len != 0)
	    do
		*dst++ = *src++;
	    while (--len != 0);
    }
    return dest;
}
