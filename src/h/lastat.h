/*	@(#)lastat.h	1.2	94/04/06 15:41:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __LASTAT_H__
#define __LASTAT_H__

#ifdef LASTAT
/* lance statistics */
struct lastat {
	long	ls_babl;	/* Babbling errors                           */
	long	ls_cerr;	/* Collision errors                          */
	long	ls_miss;	/* Missed packet errors                      */
	long	ls_dropped;	/* Dropped packets                           */
	long	ls_read;	/* Packets valid according to Lance          */
	long	ls_fram;	/* Input framing errors                      */
	long	ls_buff;	/* Input buffer errors                       */
	long	ls_crc;		/* Input CRC errors                          */
	long	ls_oflo;	/* Input silo overflow                       */
	long	ls_coll;	/* Collisions, not accurate                  */
	long	ls_written;	/* Packets stored in transmit ring           */
	long	ls_oerrors;	/* Output errors                             */
	long	ls_one;		/* One collision                             */
	long	ls_more;	/* More collisions                           */
	long	ls_def;		/* Deferred                                  */
	long	ls_uflo;	/* Transmission silo underflow               */
	long	ls_lcar;	/* Lost carrier                              */
	long	ls_lcol;	/* Late collisions                           */
	long	ls_rtry;	/* 16 tries failed                           */
	long	ls_altprot;	/* other Amoeba proto field                  */
	long	ls_unkprot;	/* other proto field                         */
	long	ls_stopped;	/* Lance stopped!!                           */
	long	ls_reboots;	/* Lance rebooted                            */
};
#define STINC(x) lastat.x++
#define STADD(x,y) lastat.x += y
#else
#define STINC(x)	/* nothing */
#define STADD(x,y)	/* nothing */
#endif /*LASTAT*/

#endif /* __LASTAT_H__ */
