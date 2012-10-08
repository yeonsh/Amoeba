/*	@(#)f30obether.c	1.3	94/04/06 09:04:41 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "assert.h"
INIT_ASSERT
#include "map.h"

/************************************************************************/
/*      NETWORK                                                         */
/************************************************************************/

phys_bytes
force30_ether_addr_conv(a)
phys_bytes a;
{

        if (a <0xFEF00000)
                return 0x1000000;
        return (a - 0xFEF00000);
}

/*ARGSUSED*/
static
char *bmalloc(size,align)
int size;
int align;
{
        compare(size, <=, 0x10000);
        (void) memset((_VOIDSTAR) 0xFEF00000, 0, (size_t) size);
        return (char *) 0xFEF00000;
}

#include "interrupt.h"
#include "lainfo.h"
struct lainfo lainfo = {
        bmalloc,
        DVADR_LANCE,            /* device address */
        {246},                  /* interrupt info */
        4                       /* csr3 */
};
