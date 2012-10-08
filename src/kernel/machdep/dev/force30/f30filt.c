/*	@(#)f30filt.c	1.3	94/04/06 09:04:24 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include "amoeba.h"
#include "sys/proto.h"
#include "machdep.h"
#include "global.h"

/************************************************************************/
/*      NETWORK                                                         */
/************************************************************************/

/*ARGSUSED*/
static
char *myaalloc(size, align)
int size;
int align;
{
        char *base;

        base = (char *) getblk((vir_bytes) size);
        disable_caching(base, size);
        (void) memset((_VOIDSTAR) base, 0, size);
        return base;
}

#include "interrupt.h"
#include "lainfo.h"
struct lainfo lainfo = {
        myaalloc,
        0xFCFF00E0,             /* device address */
        {179},                  /* interrupt info */
        4                       /* csr3 */
};
