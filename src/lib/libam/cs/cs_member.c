/*	@(#)cs_member.c	1.3	96/02/27 11:00:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "capset.h"
#include "module/cap.h"

/* See whether cap is a member of the capability set cs.
 */
cs_member(cs, cap)
capset *cs;
capability *cap;
{
    register i;
    suite *s;

    for (i = 0; i < cs->cs_final; i++)
    {
	s = &cs->cs_suite[i];
	if (s->s_current && cap_cmp(&s->s_object, cap))
	    return 1;
    }
    return 0;
}
