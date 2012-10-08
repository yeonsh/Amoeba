/*	@(#)memcpy.c	1.1	96/02/27 11:10:45 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * memcpy
 *	This memcpy is for the big-endian RISC machines and the SPARC in
 *	particular.  It uses a bitblt approach for unaligned copies and tries
 *	to optimize the aligned copy with an unrolled 64-bit copy on the SPARC.
 *
 *	Although the hack of subtracting the src from the destination to avoid
 *	incrementing both pointers isn't strictly legal, it does work.
 *	Perhaps a cleaner version will appear soon.
 *
 *	Author: Gregory J. Sharp, Sept 1995
 */

#include "string.h"

#ifndef __STDC__
#define const   /**/
#endif


#if defined(sparc) && (__GNUC__ > 1)
#define	QUADMOVE 1
#endif

typedef unsigned long		uint32;
typedef unsigned short		uint16;

#if defined(QUADMOVE)
typedef	long long		uint64;
#endif /* defined(QUADMOVE) */

#define	WS		4
#define	WSM1		3


_VOIDSTAR
memcpy(dest, sauce, len)
_VOIDSTAR	dest;
const _VOIDSTAR	sauce;
size_t		len;
{
    char *	dst = (char *) dest;
    uint32	src = (uint32) sauce;
    uint32	sl;
    uint32	dl;
    uint32	prevword;
    uint32	thisword;
    size_t	x;

    /*
     * Don't go to any trouble for small byte counts.
     * Do the blt only for larger copies.
     */
    if (len >= (WS*4 - 1))
    {
	/* Do a bitblt-like copy */

	sl = (uint32) src & WSM1;

	/* Now 4-byte-align src */
	switch (sl)
	{
	case 1:
	    *dst++ = *(char *)src++;
	    len--;
	    /* Fall through */
	case 2:
	    thisword = *(uint16 *) src;
	    src += 2;
	    *dst = (char) (thisword >> 8);
	    *(dst+1) = (char) thisword;
	    dst += 2;
	    len -= 2;
	    break;
	case 3:
	    *dst++ = *(char *)src++;
	    len--;
	    break;
	}

	/* Now to the copying */
	dl = (uint32) dst & WSM1;
	switch (dl)
	{
	case 1:
	    prevword = *(uint32 *) src;
	    src += 4;
	    *dst = (char) (prevword >> 24);
	    *(uint16 *) (dst+1) = (uint16) (prevword >> 8);
	    dst += 3;
	    len -= 3;
	    src -= (uint32) dst;
	    x = len & ~WSM1;
	    do
	    {
		thisword = prevword << 24;
		prevword = *(uint32 *) (dst + src);
		x -= 4;
		*(uint32 *) dst = (prevword >> 8) | thisword;
		dst += 4;
	    } while (x != 0);
	    src--;
	    len &= 3;
	    break;
	
	case 2:
	    prevword = *(uint32 *) src;
	    src += 4;
	    *(uint16 *) dst = (uint16) (prevword >> 16);
	    dst += 2;
	    len -= 2;
	    src -= (uint32) dst;
	    x = len & ~WSM1;
	    do
	    {
		thisword = prevword << 16;
		prevword = *(uint32 *) (dst + src);
		x -= 4;
		*(uint32 *) dst = (prevword >> 16) | thisword;
		dst += 4;
	    } while (x != 0);
	    src -= 2;
	    len &= 3;
	    break;

	case 3:
	    prevword = *(uint32 *) src;
	    src += 4;
	    *dst++ = (char) (prevword >> 24);
	    len--;
	    src -= (uint32) dst;
	    x = len & ~WSM1;
	    do
	    {
		thisword = prevword << 8;
		prevword = *(uint32 *) (dst + src);
		x -= 4;
		*(uint32 *) dst = (prevword >> 24) | thisword;
		dst += 4;
	    } while (x != 0);
	    src -= 3;
	    len &= 3;
	    break;

	case 0:
#if defined(QUADMOVE)
	    /* Can we do 8-byte moves?? */

	    sl = (uint32) src & 7;
	    dl = (uint32) dst & 7;

	    if ((dl ^ sl) == 0)
	    {
		/* At least at 4-byte boundary now */
		if (sl & 4)
		{
		    *(uint32 *) dst = *(uint32 *) src;
		    dst += 4;
		    src += 4;
		    len -= 4;
		}

		/*
		 * 8-bytes at a time copying.
		 * We use an unrolled loop to get around various register interlocks
		 * and memory barriers on the sparc.
		 */
		{
		    uint64 s0, s1;
		    int tmp_len;
 
                    s0 = *(uint64 *) src;
                    src += 8;
                    len -= 8;
                    while ((tmp_len = len - 32) >= 0)
                    {
                        s1 = *(uint64 *) src;
                        *(uint64 *) dst = s0;
                        s0 = *(uint64 *) (src+8);
                        *(uint64 *) (dst+8) = s1;
 
                        s1 = *(uint64 *) (src+16);
                        *(uint64 *) (dst+16) = s0;
                        s0 = *(uint64 *) (src+24);
                        *(uint64 *) (dst+24) = s1;
 
                        len = tmp_len;
                        dst += 32;
                        src += 32;
                    }
                    *(uint64 *) dst = s0;
                    dst += 8;
 
                    /* There might be 24 bytes left - we do them now */
                    while ((tmp_len = len - 8) >= 0)
                    {
                        *(uint64 *) dst = *(uint64 *) src;
                        dst += 8;
                        src += 8;
                        len = tmp_len;
                    }
		}
	    }
	    /* The may be up to 7 bytes to go ... */

#endif /* defined(QUADMOVE) */

	    /* Do 4-byte aligned copy */

	    x = len & ~WSM1;

	    src -= (uint32) dst;
	    while (x != 0)
	    {
		*(uint32 *) dst = *(uint32 *) (dst + src);
		x -= 4;
		dst += 4;
	    }
	    len &= WSM1;
	    break;
	}
    }
    else
    {
	src -= (uint32) dst;
    }

    /* Any cleaning up still? */
    while (len != 0)
    {
	*dst = *(dst + src);
	dst++;
	len--;
    }
    return dest;
}
