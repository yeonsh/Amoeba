/*	@(#)cap_cmp.c	1.4	96/02/27 10:59:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/cap.h"

#if defined(__STDC__) || defined(AMOEBA) || defined(SYSV)
#include <string.h>
#else
extern int bcmp();
#define memcmp(p1, p2, size)	bcmp(p2, p1, size)
#endif

int
cap_cmp(cap1, cap2)
capability *cap1, *cap2;
{
    return memcmp(cap1, cap2, sizeof(capability)) == 0;
}
