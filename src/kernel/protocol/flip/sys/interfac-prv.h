/*	@(#)interfac-prv.h	1.2	96/02/27 14:04:53 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __INTERFACE_PRV_H__
#define __INTERFACE_PRV_H__

#define INT_WEIGHT	0
#define NADDRESSES	(2 * int_maxint)	/* max. # addresses */
#ifndef MAXEP
#define MAXEP		100
#endif
#define MAXRETRIAL	1
#define LOCTIMEOUT	((interval) 100)		/* msec */

#define IF_FREE		1
#define IF_USED		2

    
/* List of addresses. */
typedef struct address_list {
    adr_t al_address;
    struct address_list *al_next;
} alist_t, *alist_p;


/* Interface struct. */
typedef struct intface {
    int 	if_state;		/* state of interface */
    long 	if_ident;		/* argument to wakeup routine */
    int		if_broadcast;		/* deliver broadcast msg's? */
    alist_p	if_addrs;		/* public addresses */
    adr_p	if_endpoint[MAXEP];	/* list of EP */
    uint32 	if_messid;		/* flip messid */
    location 	if_location;		/* interface location */
    void	(*if_receive)();	/* routine to receive a fragment */
    void	(*if_notdeliver)();	/* routine for undelivered fragment */
    adr_t	if_lastdst;		/* last destination */
    f_hopcnt_t	if_lasthcnt;		/* hops to last destination */
    int		if_lastntw;		/* ntw for last destination */
    location	if_lastloc;		/* location for last destination */
    int		if_lastsafe;		/* safe bit for last destination */
} intface_t, *intface_p;


/* Locate structure. */
typedef struct loc_list {
    interval	l_timer;	/* locate timer (ms) */
    interval	l_timeout;	/* time before giving up (ms) */
    interval	l_deltatime;	/* time before sending next locate */
    adr_t 	l_dst;		/* address to locate */
    uint16	l_cnt;		/* # destination locations */
    short	l_trusted;	/* security required? */
    f_hopcnt_t	l_hopcnt;	/* hopcount for locate */
    int		l_retrial;	/* # retrial for locate */
    pkt_p	l_pkt;		/* packet to send to dst */
    int		l_unicast;	/* packet is unicast */
    int		l_ep;		/* source EP */
    f_size_t	l_length;	/* length of user data */
    intface_p	l_intf;		/* pointer to interface */
    struct loc_list *l_next;	/* pointer to next entry in locate list */
    int		l_locevent;	/* synchronization var for FLIP_SYNC */
    int 	l_state;	/* state var */
    int		l_flags;	/* flip_xxcast flags */
#ifdef NEWLOCATECODE
    struct loc_list *l_dstlist; /* list structures for the same destination. */
    struct loc_list *l_lastdst; /* ptr to last entry on destination list. */
#endif
} locate_t, *locate_p;
    

/* Possible values of l_state: */
#define SLEEPING	1
#define FAILED		2
#define SUCCESS		3


#ifdef SPEED_HACK
#define int_fast(intf, fh, pkt) (*intf->if_receive)(intf->if_ident, pkt,\
                          &fh->fh_dstaddr, &fh->fh_srcaddr, fh->fh_messid,\
			  0, fh->fh_length,fh->fh_total);

#endif

#endif /* __INTERFACE_PRV_H__ */
