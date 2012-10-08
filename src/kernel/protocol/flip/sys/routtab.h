/*	@(#)routtab.h	1.4	94/04/06 08:49:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ROUTTAB_H__
#define __ROUTTAB_H__

/* An address is remote or local */
#define REMOTE		0
#define LOCAL		1

/* A location is on a trusted network or not. */
#define UNSECURE	0
#define SECURE		1

struct loc_info {
	location 	li_location;	/* ditto */
	int   		li_local;	/* is this location local ? */
	f_hopcnt_t 	li_hopcnt;	/* how far is it away? */
	uint16 		li_count;	/* # locations */
	uint16 		li_trust_count;	/* # loc on trusted networks */
	long 		li_idle;	/* idle timer */
	short 		li_mcast;	/* does it listen to mcast addr ? */
	struct loc_info *li_next;
};


#define LI_MCAST	0x1
#define LI_NOMCAST	0x2

struct ntw_info {
	int 		ni_network;	/* on which network is it? */
	uint16 		ni_count;	/* # locations */
	uint16		ni_trust_count;	/* # loc on trusted networks */
	uint16 		ni_nmcast;	/* # listeners of mcast addr */
	location 	ni_multicast;	/* if multicast is applicable */
	struct loc_info *ni_loc;	/* locations on this network */
	struct ntw_info *ni_next;
};


struct adr_info {
	adr_t 		ai_address;	/* the address this entry is about */
	uint16 		ai_count;	/* # locations */
	uint16		ai_trust_count; /* # loc on trusted networks */
	f_hopcnt_t 	ai_hopcnt;	/* how far is it away ? */
	struct ntw_info *ai_ntw;	/* sites listening to address */
	struct adr_info *ai_next;
};


typedef struct route {			/*  last src and dst seen on ntw */
    struct adr_info	*r_sadr;
    struct loc_info	*r_sloc;
    struct adr_info	*r_dadr;
    struct loc_info	*r_dloc;
    struct ntw_info 	*r_dntw;
    short		r_dhcnt;
} route_t, *route_p;


#define NI_BROADCAST	((struct ntw_info *) 1)

#define NONTW		(-1)		/* no network specified */

void adr_install _ARGS((adr_p adr, int ntw, location *loc,
			f_hopcnt_t hopcnt, int local, int trusted));

void adr_add _ARGS((adr_p adr, int ntw, location *loc, f_hopcnt_t hopcnt,
			uint16 count, uint16 trust_count, int local)); 

int adr_route _ARGS((adr_p adr, f_hopcnt_t maxhops, int *dstntw,
		     location *dstloc, f_hopcnt_t *hopcnt, int notntw,
		     int secure));

/* return values of adr_route: */
#define		ADR_OK		1
#define		ADR_TOOFARAWAY	(-1)
#define 	ADR_UNSAFE	(-2)


void adr_remove _ARGS((adr_p adr, int ntw, location *loc, int all, int nontw));

uint16 adr_count _ARGS((adr_p adr, f_hopcnt_t *hopcnt, int notntw, int
			trusted));

struct ntw_info *adr_lookup _ARGS((adr_p adr, uint16 *count, f_hopcnt_t *hopcnt,
				   int safe));

void adr_purge _ARGS(( void ));

#endif /* __ROUTTAB_H__ */
