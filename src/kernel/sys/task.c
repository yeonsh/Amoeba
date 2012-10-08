/*	@(#)task.c	1.2	94/04/06 10:08:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "exception.h"

/* This file contains pointers to the thread table.  Interesting. */

#define extern

#include "kthread.h"

#undef extern
