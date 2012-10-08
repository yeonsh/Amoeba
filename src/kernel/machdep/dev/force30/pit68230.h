/*	@(#)pit68230.h	1.3	96/02/27 13:48:02 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Defines for Motorola 68230 parallel IO and Timer chip
 *
 *
 */

struct pit68230 {
	volatile char	pit_pgcr;
	volatile char	pit_psrr;
	volatile char	pit_paddr;
	volatile char	pit_pbddr;
	volatile char	pit_pcddr;
	volatile char	pit_pivr;
	volatile char	pit_pacr;
	volatile char	pit_pbcr;
	volatile char	pit_padr;
	volatile char	pit_pbdr;
	volatile char	pit_paar;
	volatile char	pit_pbar;
	volatile char	pit_pcdr;
	volatile char	pit_psr;
	volatile char	notused0;
	volatile char	notused1;
	volatile char	pit_tcr;
	volatile char	pit_tivr;
	volatile char	notused2;
	volatile char	pit_cprh;
	volatile char	pit_cprm;
	volatile char	pit_cprl;
	volatile char	notused3;
	volatile char	pit_cnrth;
	volatile char	pit_cnrtm;
	volatile char	pit_cnrtl;
	volatile char	pit_tsr;
};
