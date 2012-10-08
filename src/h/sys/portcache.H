/*	@(#)portcache.H	1.2	94/04/06 17:18:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**  WARNING:
**	This file is included by kthread.h directly into a struct definition.
**	As a consequence you cannot define anything except macros and
**	additions inside the portcache struct.
*/

struct portcache {
	address PE_location;
	struct thread *PE_link;
} tk_portcache;

#ifdef PORTCACHE

#define pe_location	tk_portcache.PE_location
#define pe_link		tk_portcache.PE_link

#else 

#define tk_portcache	tk_dummy	/* other modules must not touch it */

#endif

#define NOWAIT		0
#define WAIT		1

#define LOOK		0
#define DELETE		1
