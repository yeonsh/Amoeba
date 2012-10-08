/*	@(#)pnstat.h	1.2	94/04/06 15:42:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __PNSTAT_H__
#define __PNSTAT_H__

struct pnstat {
	long pns_read;
	long pns_written;
	long pns_rxerr;
	long pns_txerr;
	long pns_wrong;
	long pns_old;
	long pns_refused;
	long pns_phwait;
	long pns_short;
	long pns_vshort;
};

#endif /* __PNSTAT_H__ */
