/*	@(#)proswappro.cls	1.3	96/02/27 10:28:01 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 *	This should once be moved to a reasonable place,
 *	but I need it right now.	- Siebren
 */

#include "server/process/proc.h"

class proswapproc [PS_SWAPPROC .. PS_SWAPPROC] {
    pro_swapproc(*, var capability oldproc, var capability newproc);
};
