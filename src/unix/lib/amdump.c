/*	@(#)amdump.c	1.2	94/04/07 14:05:56 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "host_os.h"
#include "amoeba.h"
#include "amparam.h"

amdump()
{
    return _amoeba(AM_DUMP);
}
