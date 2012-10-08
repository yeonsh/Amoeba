/*	@(#)flstat.h	1.3	94/04/06 17:20:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Statistics for the interface module. */

typedef struct flstat {
	int fl_shereis;
	int fl_slocate;
	int fl_snothere;
	int fl_suni;
	int fl_smulti;
	int fl_rhereis;
	int fl_rlocate;
	int fl_rnothere;
	int fl_runi;
	int fl_rmulti;
	int fl_runtrusted;
} flstat_t, *flstat_p;
