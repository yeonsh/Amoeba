/*	@(#)bootsvr.cls	1.3	94/04/06 11:38:18 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <class/boot.cls>

/*
 *	Specific class for the server:
 */
class boot_svr {
    inherit boot;

    /*
     *	Internal state kept for each config:
     */
    typedef struct {
	mutex		or_lock;	/* To serialise operations */
	struct {
	    capability	or_proccap;	/* The current process capability */
	    capability	or_objcap;	/* The unrestricted capability */
	    long	or_boottime;	/* Time when last booted */
	    boot_data	or_data;	/* Data visible in the outside */
	    long	or_nextpoll;	/* At which time to do the next poll */
	    long	or_nextboot;	/* When to do the next boot */
	    int		or_status;	/* Some flags; see below */
	    int		or_retries;	/* Poll retries done */
	    char	or_errstr[200];	/* Last error or info-string */
	} or;
    } obj_rep;
    typedef obj_rep *obj_ptr;

    /* Flags for status: */
    const ST_AMOWNER = 0x01;		/* ownercap is valid */
    const ST_GOTPROC = 0x02;		/* proccap is valid */
    const ST_POLLOK  = 0x04;		/* Last poll successfull */
    const ST_ISIDLE = 0x20;		/* Don't touch this one */
    const MAX_DBGTEXT = 5000;

    /* to #if on: */
    const INCLUDED_OBJREP_DEF = 1;

};
