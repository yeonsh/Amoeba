/*	@(#)exec.cls	1.3	96/02/27 10:27:40 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Don't generate client stubs from this! */

#include "amoeba.h"
#include "server/process/proc.h"

class exec [PS_EXEC..PS_GETLOAD] {

	pro_1getload [ PS_GETLOAD ] (*,
		out:h_extra int kips, /* CPU speed, in KIPS */
		out:h_size int loadav, /* runnable threads * 1024 */
		out:h_offset long mfree); /* free memory in bytes */
	/* The handwritten stub returns ips, by multiplying the kips
	   with 1024, and has longs for all three output parameters */

};
