/*	@(#)fast_copy.h	1.1	96/02/27 13:46:46 */
#include <string.h>

#ifdef USER_INTERRUPTS
/* We need a *really* fast copy here */
static __inline__ void
fast_copy(w1, w2, nshorts)
unsigned short *w1, *w2;
register unsigned nshorts;
{
    /* Only optimize the full 8 byte aligned non-overlapping case */
    if (((((unsigned) w1 | (unsigned) w2) & 0x7) == 0) &&
        ((nshorts & 0x3) == 0))
    {
        register double *src, *dst;
        register int ndoubles;
 
        src = (double *) w1;
        dst = (double *) w2;
        ndoubles = nshorts >> 2;
 
        /* With a smart compiler, this generates a tight loop on the sparc: */
        while (ndoubles > 1) {
            dst[ndoubles - 1] = src[ndoubles - 1];
            dst[ndoubles - 2] = src[ndoubles - 2];
            ndoubles -= 2;
        }
        if (ndoubles) {
            *dst = *src;
        }
    } else {
        /* Just let the default memcpy handle all other cases */
        memmove(w2, w1, nshorts * 2);
    }
}
#endif

