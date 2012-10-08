/*	@(#)dgwdef.h	1.1	96/02/27 11:01:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __DGWDEF_H__
#define	__DGWDEF_H__

/* Definition file for internal interface to dgwalk set of
   routines

   Author: E.G. Keizer
	   Faculteit Wiskunde & Informatika
	   Vrije Universiteit
	   Amsterdam

   The dgwalk routines create a set of directory graphs that enables
   an amoeba program(mer) to walk through a set of directories.

*/
/* #define static	/* Just to be able to profile */

#define PROBE_TIMEOUT	((interval) 500)	/* Time-out when probing */
#define	ALL_RIGHTS	PRV_ALL_RIGHTS


struct memlist {
	unsigned	size ;
	unsigned	used ;
	struct memlist	*next ;
	/* The rest is allocated right here */
} ;
struct memhead {
	struct memlist	*first ;
	struct memlist	*last ;
} ;

#define	MEMINCR		4096
#define MEMRND(x)	((x)+3)&(~3) /* 4-byte mutiple */
#define getsmem(type)	((type *)getmem(&myp->statmem,sizeof(type)))

/* The handle to it all */

struct dgw_info {
	dgw_params	params;
	struct obj_table *hasharray;
	int		hashsize;
	dgw_dirlist	*head;
	struct memhead	dofirst;
	struct memhead	doafter;
	struct memhead	statmem;
};

#endif /* __DGWDEF_H__ */
