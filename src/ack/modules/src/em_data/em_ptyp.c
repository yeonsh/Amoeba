/*	@(#)em_ptyp.c	1.2	94/04/06 11:21:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include	<em_spec.h>
#include	<em_ptyp.h>

short	em_ptyp[] = {
	0,			/* PAR_NO */
	cst_ptyp,		/* PAR_C */
	cst_ptyp,		/* PAR_D */
	cst_ptyp,		/* PAR_N */
	cst_ptyp,		/* PAR_F */
	cst_ptyp,		/* PAR_L */
	arg_ptyp,		/* PAR_G */
	cst_ptyp|ptyp(sp_cend),	/* PAR_W */
	cst_ptyp,		/* PAR_S */
	cst_ptyp,		/* PAR_Z */
	cst_ptyp,		/* PAR_O */
	ptyp(sp_pnam),		/* PAR_P */
	ptyp(sp_cst2),		/* PAR_B */
	ptyp(sp_cst2)		/* PAR_R */
};
