/*	@(#)switch.h	1.5	94/04/06 17:21:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
prototypes for switch.c
*/

void pktswitch _ARGS(( pkt_t *pkt, int ntw, location *loc ));

#define MAXNAME		40
#define BCASTSWEEP	1000L	/* no more bcasts than MAXBCAST PER SWEEP */
#ifndef MAXBCAST
#define MAXBCAST	100
#endif

struct netswitch {		/* description of network interface */
	char 	ns_name[MAXNAME];	/* name */
	int 	ns_devno;		/* device number */
	f_hopcnt_t ns_weight;		/* #hops */
	short	ns_trusted;		/* trusted? */
	short	ns_delay;		/* milli seconds artificial delay */
	short	ns_loss;		/* percentage of message loss */
	short	ns_on;			/* switched on ? */
#ifdef OLDFRAGMENTCODE
	f_size_t ns_mtu;		/* maxium pkt size */
	short 	ns_fragmentation;	/* fragmentation needed */
#endif
	/* send procedure */
	void 	(*ns_send)_ARGS((int dev, pkt_p pkt, location *loc));
	/* broadcast procedure */
	void 	(*ns_bcast)_ARGS((int dev, pkt_p pkt));
	/* set mcast procedure */
	void 	(*ns_setmc)_ARGS((int dev, struct ntw_info *ni, adr_p adr));
	/* delete mcast procedure */
	void 	(*ns_delmc)_ARGS((int dev, location *loc, adr_p adr));
	uint16 	ns_nlocation;		/* # locations needed for mcast */
	int	ns_bcastpersweep;	/* #bcasts received since last sweep */
#ifdef STATISTICS
	int 	ns_rlocate;			/* receive statitics */
	int 	ns_rhereis;   
	int 	ns_rnothere;
	int 	ns_rmultidata;
	int 	ns_runidata;
	int	ns_runtrusted;
	int 	ns_slocate;			/* send statitics */
	int 	ns_shereis;   
	int	ns_snothere;
	int 	ns_smultidata;
	int	ns_sunidata;
	int 	ns_suntrusted;
	int	ns_dropped;
	int 	ns_bcastdrop;
#endif
};

#ifdef STATISTICS
#define STINC(ns, type)	((ns)->type++)
#else
#define STINC(ns, type)
#endif
