/*	@(#)mu_init.c	1.3	96/02/27 11:22:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/mutex.h"

void
mu_init(s)
mutex *	s;
{
    static mutex null_mutex;

    *s = null_mutex;
}
