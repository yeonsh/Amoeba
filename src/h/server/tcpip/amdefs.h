/*	@(#)amdefs.h	1.2	94/04/06 17:16:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __AMDEFS_H__
#define __AMDEFS_H__

/*
** Customizable defines.
*/

#define TCP_PRIVDIR	"/super/module/tcpip"

/*
*  timeout for response to ARP packet for Ethernet
*  (in seconds)
*/
#define DLAYTIMEOUT 5

/*
*  how often to poke a TCP connection to keep it alive and make
*  sure other side hasn't crashed.
*  And, timeout interval
*/
#define POKEINTERVAL	120000	/* 2 minutes */
#define MAXRTO  	5000	/* 5 seconds */
#define MINRTO		400	/* .4 second */
#define ARPTO		1000	/* 1 second */
#define CACHETO		350000	/* 6 minutes */
#define WAITTIME	2000	/* 2 seconds */
#define LASTTIME	200000	/* 200 seconds */
#define OPENTIME	2000	/* 2 seconds between sending SSYNS */
#define OPENTRIES	5	/* try 5 times */
#define CLEANDELAY	60000	/* Cleanup once per minute */
#define CLEANCOUNT	10	/* GC after 10 minutes idle */

#define NTASK_USER	3	/* # of user request handlers */
#define NTASK_ETHER	2	/* # of ethernet listeners */

#define SMALLSTACK	3072	/* A small stack size */
#define AILSTACK	4096	/* extra stacksize for ail thread */


#define CACHELEN 10		/* number of ARP cache entries */
#define MAXSEG 1024		/* Maximum segment size */
#define CREDIT 4096		/* Window size */

#endif /* __AMDEFS_H__ */
