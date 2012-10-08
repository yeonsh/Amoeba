/*	@(#)mpx_md.H	1.3	94/04/06 15:59:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * i80386 CPU dependent stuff
 *
 * For the moment this is located in the thread structure,
 * but it should be somewhere on the user stack (just as
 * the normal registers).
 */
struct {
    int MD_ndpvalid;			/* the 387 state is valid */
    int MD_ndpstate[27];		/* actual 387 state */
} tk_md;

#define	md_ndpvalid	tk_md.MD_ndpvalid
#define	md_ndpstate	tk_md.MD_ndpstate
