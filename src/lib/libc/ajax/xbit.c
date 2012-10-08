/*	@(#)xbit.c	1.3	96/02/27 11:07:00 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * _aj_xbit(cap)
 *
 *	This routine returns:
 *		2 if the capability is for a Bullet-like file which is an
 *		  executable binary
 *		1 if the capability is for a Bullet-like file which is a
 *		  beginning with "#!".  Shell, perl and awk scripts should
 *		  all be correctly executed if set up this way.
 *		0 otherwise.
 *
 *  It is mainly for use by "access()" and will be inlined if necessary.
 *
 * Author: Gregory J. Sharp, Dec 1993
 */

#include "amoeba.h"
#include "stderr.h"
#include "bullet/bullet.h"
#include "module/proc.h"

#define	MIN(a, b)	((a) < (b) ? (a) : (b))

typedef union {
    char	chr[10];
    process_d	pdbuf;
}	fhead;


static char am_arch[][ARCHSIZE] = {
    "i80386",
    "mc68000",
    "sparc",
    ""
};


_aj_xbit(cap)
capability *	cap;
{
    fhead	buf;
    b_fsize	cnt;
    b_fsize	numred;
    b_fsize	fsz;
    int		i;

    if (b_size(cap, &fsz) != STD_OK)
	return 0;

    /*
     * If the file is smaller than the bare minimum process descriptor
     * then it can at best be a shell script.  At any rate we don't
     * try to read more than the file size.
     */
    cnt = MIN(fsz, sizeof (process_d));
    if (b_read(cap, (b_fsize) 0, (char *) &buf, cnt, &numred) != STD_OK ||
		cnt != numred)
    {
	return 0;
    }

    if (fsz > sizeof (process_d))
    {
	/*
	 * See if it is an Amoeba binary
	 */
	for (i = 0; am_arch[i][0] != '\0'; i++)
	{
	    if (memcmp(am_arch[i], buf.pdbuf.pd_magic, ARCHSIZE) == 0)
		return 2;
	}
    }

    /*
     * If we get here then it didn't look like an Amoeba binary.
     * Let's see if it is a shell script
     */
    if (buf.chr[0] == '#' && buf.chr[1] == '!')
	return 1;

    /* It didn't look like anything Amoeba would want to execute */
    return 0;
}
