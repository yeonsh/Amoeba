/*	@(#)glastat.h	1.4	96/02/27 13:48:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifdef STATISTICS
/* statistics */
typedef 
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
	long 	ls_reboot;	/* Reboots				     */
	long 	ls_toflo;	/* Transmit packets dropped (queue overflow) */
	long 	ls_tqtot;	/* Packets moved to overflow queue	     */
	long	ls_memcpy;	/* Transmit packets that required memcpy     */
} lastat;
#define STINC(x) lap->lad_stat.x++
#define STADD(x,y) lap->lad_stat.x += y
#else /* STATISTICS */
#define STINC(x)	/* nothing */
#define STADD(x,y)	/* nothing */
#endif /* STATISTICS */
