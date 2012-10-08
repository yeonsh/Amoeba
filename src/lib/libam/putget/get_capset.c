/*	@(#)get_capset.c	1.4	96/02/27 11:03:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <stdlib.h>
#include "amoeba.h"
#include "capset.h"
#include "module/buffers.h"
#include "soap/soap.h"

/* sp_alloc() normally calls calloc(), but if that fails it
 * the Soap server version retries after having uncached a directory.
 */

char *
buf_get_capset(p, e, cs)
char *		p;
char *		e;
capset *	cs;
{
    int16 short_val;

    if ((p = buf_get_int16(p, e, &short_val)) != 0) {
	cs->cs_initial = short_val;
    }
    if ((p = buf_get_int16(p, e, &short_val)) != 0) {
	cs->cs_final = short_val;
    }
    if (p == 0 || cs->cs_final < 0 || cs->cs_final > MAXCAPSET) {
        cs->cs_suite = 0; /* in case caller cs_free()s it */
	return 0;
    }

    if (cs->cs_final == 0) { /* no suites to allocate */
	cs->cs_suite = 0;
    } else {
        register int i;

        cs->cs_suite = (suite *) sp_alloc(sizeof (suite),
    					  (unsigned int) cs->cs_final);
        if (cs->cs_suite == 0) {
	    return 0;
	}

	for (i = 0; i < cs->cs_final; i++) {
	    p = buf_get_cap(p, e, &cs->cs_suite[i].s_object);
	    p = buf_get_int16(p, e, &short_val);
	    cs->cs_suite[i].s_current = short_val;
        }
    }

    return p;
}
