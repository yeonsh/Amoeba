/*	@(#)portcache.c	1.3	94/04/06 08:34:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define PORTCACHE

/*
 * This module does the port management. It keeps track of the local servers
 * doing a ``getreq'' on a port, local clients waiting for a server on some
 * port, and interlocal servers addressed by some port. This last category of
 * ports may be forgotten, or may be incorrect.
 *
 * The following routines are defined:
 *	portinstall(port, where, wait);
 *	portlookup(port, wait, delete);
 *	portremove(port, location);
 *	portquit(port, thread);
 *
 * ``Portinstall'' is called when a port is assumed to be at location
 * ``where.'' If ``wait'' is set, the port is local.
 * ``Portlookup'' is called to find a port in the cache. If ``wait'' is
 * set, the routine will block until the port is found. If ``delete'' is
 * set, the port must be removed when it is found.
 * ``Portremove'' removes a port from the cache which is thought
 * of to be at the specified location.
 * When a port doesn't have to be located anymore for some thread, ``portquit''
 * takes care of that.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "exception.h"
#include "kthread.h"
#include "internet.h"
#include "assert.h"
INIT_ASSERT

#ifdef STATISTICS
#include "portstat.h"

struct portstat portstat;
#define STINC(x)	portstat.x++
#else
#define STINC(x)
#endif

#define LOGHASH		   5	/* log sizeof hash table of local ports */

struct porttab {
	port p_port;			/* the port this entry is about */
	uint16 p_idle;			/* idle timer */
	address p_location;		/* where is it? (0 = being located) */
	address p_asker;		/* address of someone interested */
	struct porttab *p_nextport;	/* port with same hash value */
	struct thread *p_threadlist;	/* list of threads */
};

#define NILPORTTAB	((struct porttab *) 0)
#define NILBACK		((struct porttab **) 0)

#define NHASH		(1 << LOGHASH)
#define HASHMASK	(NHASH - 1)

#define hash(p)		(* (uint16 *) (p) & HASHMASK)

extern uint16 nport;
static struct porttab *porttab, *lastport, *hashtab[NHASH], *portfree;

#ifndef NONET

#define NLOCATE            5    /* max. number of ports to locate */
static port loctab[NLOCATE];
static uint16 loccnt, loctim, locthissweep;
extern uint16 minloccnt, maxloccnt;

#endif

/* Allocate an entry in the hash table at location ``ht.'' */
static struct porttab *allocate(ht, p)
struct porttab **ht;
port *p;
{
  register struct porttab *pt;

  STINC(pts_alloc);
  if ((pt=portfree) == 0) {
	STINC(pts_full);
	portpurge();		/* total cleanup, not likely to happen */
	if ((pt=portfree) == 0)
		return 0;
  }
  portfree = pt->p_nextport;
  pt->p_nextport = *ht;
  *ht = pt;
  pt->p_port = *p;
  return pt;
}

/* Install a port in the hash table.  If ``wait'' is set, the location will
 * be this machine and is certain.  If not, the location is somewhere else
 * and uncertain.
 */
portinstall(p, where, wait)
register port *p;
address where;
{
  register struct porttab **ht, *pt;
  register struct thread *t;

  ht = &hashtab[hash(p)];
  for (pt = *ht; pt != NILPORTTAB; pt = pt->p_nextport)
	if (PORTCMP(&pt->p_port, p)) {
		switch (area(pt->p_location)) {
		case DONTKNOW:	/* found a client, wake him up */
			compare(pt->p_threadlist, !=, NILTHREAD);
			do {
				t = pt->p_threadlist;
				t->pe_location = where;
				STINC(pts_wakeup);
				wakeup((event) &t->pe_location);
			} while ((pt->p_threadlist = t->pe_link) != NILTHREAD);
			break;

		case LOCAL:	/* another server */
			if (!wait && pt->p_threadlist!=0)
				return;
			break;
		
		case GLOBAL:	/* somewhere else too, remove this info */
			compare(pt->p_threadlist, ==, NILTHREAD);
			break;
		
		default:
			assert(0);
		}
		break;
	}
  if (pt == NILPORTTAB && (pt = allocate(ht, p)) == NILPORTTAB)
	if (wait)
		panic("portcache full for servers");
	else		/* no room left, so forget it */
		return;
  pt->p_location = where;
  if (wait) {	/* thread is going to await a client, so leave it immortal */
	compare(area(where), ==, LOCAL);
	t = &thread[threadnum(where)];
	t->pe_location = where;
	t->pe_link = pt->p_threadlist;
	pt->p_threadlist = t;
#ifndef NONET
	if (pt->p_asker != NOWHERE) {
		STINC(pts_here);
		hereport(pt->p_asker, p, 1);
		pt->p_asker = NOWHERE;
	}
#endif
  }
  pt->p_idle = 0;
}

#ifndef NONET
/* Broadcast a locate message
 */
static sendloc(){
  register struct porttab *pt;
  register n = 0;

  if (locthissweep) {
	/* During this clocktick we already sent out a broadcast packet.
	 * To prevent buggy userprograms from creating a broadcast storm
	 * we do not send another one, we just prepare for it to be done 
	 */
	 STINC(pts_nolocate);
	 loccnt = maxloccnt;
	 return;
  }
  for (pt = porttab; pt < lastport; pt++)
	if (pt->p_location == SOMEWHERE) {
		loctab[n++] = pt->p_port;
		if (n == NLOCATE)
			break;
	}
  if (n) {
	locthissweep = 1;
	whereport(loctab, n);	/* send out the broadcast locate */
  } else
	loctim = 0;	/* No locates necessary anymore */
  loccnt = 0;
}

#endif NONET

/* Look whether port p is in the portcache.  You can specify whether you
 * want to wait for the information and whether you want to delete it.
 */
address portlookup(p, wait, del)
register port *p;
{
  register struct porttab **ht, *pt;
  register struct thread *c;
  register struct thread *t;
  register address location;

  STINC(pts_lookup);
  ht = &hashtab[hash(p)];
  for (pt = *ht; pt != NILPORTTAB; pt = pt->p_nextport)
	if (PORTCMP(&pt->p_port, p)) {	/* found it */
		location = pt->p_location;
		switch (area(location)) {
		case LOCAL:		/* local server */
			if (pt->p_threadlist == 0)
				break;
			if (del) {	/* remove it */
				pt->p_threadlist = pt->p_threadlist->pe_link;
				if ((t = pt->p_threadlist) != NILTHREAD)
					pt->p_location = t->pe_location;
			}
			pt->p_idle = 0;
			STINC(pts_flocal);
			return(location);

		case GLOBAL:		/* remote server */
			compare(pt->p_threadlist, ==, NILTHREAD);
			pt->p_idle = 0;
			STINC(pts_fglobal);
			return(location);
		
		case DONTKNOW:		/* somebody else wants to know too */
			compare(pt->p_threadlist, !=, NILTHREAD);
			break;
		
		default:
			assert(0);
		}
		break;
	}
  /* The port wasn't in the port cache */
  if (wait) {		/* wait for it */
	if (pt == NILPORTTAB && (pt = allocate(ht, p)) == NILPORTTAB)
		panic("portcache full for clients");
	pt->p_location = SOMEWHERE;
	c = curthread;
	c->pe_link = pt->p_threadlist;
	pt->p_threadlist = c;
#ifndef NONET
	STINC(pts_locate);
	sendloc();
	loctim = minloccnt;
#endif
	c->pe_location = SOMEWHERE;
	if (await((event) &c->pe_location, 0L))
		assert(pt->p_threadlist != c);
	pt->p_idle = 0;
	return(c->pe_location);
  }
  else
	return(NOWHERE);
}

/* Port p isn't at location ``location'' anymore */
portremove(p, location)
port *p;
address location;
{
  register struct porttab *pt, **ht;
  register struct thread *t;

  for (ht = &hashtab[hash(p)]; (pt= *ht) != NILPORTTAB; ht = &pt->p_nextport)
	if (PORTCMP(&pt->p_port, p)) {
		if (pt->p_location == location) {
			if ((t = pt->p_threadlist) != NILTHREAD) {
				compare(area(location), ==, LOCAL);
				compare(t->pe_location, ==, location);
				if ((pt->p_threadlist = t->pe_link) != NILTHREAD) {
					pt->p_location =
						pt->p_threadlist->pe_location;
					break;
				}
			} else {
				*ht = pt->p_nextport;
				pt->p_location = NOWHERE;	/* for dump */
				pt->p_nextport = portfree;
				portfree = pt;
			}
		}
		else if ((t = pt->p_threadlist) != NILTHREAD)
			while (t->pe_link != NILTHREAD)
				if (t->pe_link->pe_location == location) {
					t->pe_link = t->pe_link->pe_link;
					break;
				}
				else
					t = t->pe_link;
		return;
	}
}

/* Thread ``s'' isn't interested anymore */
portquit(p, s)
register port *p;
register struct thread *s;
{
  register struct porttab *pt, **ht;
  register struct thread *t;

  for (ht = &hashtab[hash(p)]; (pt= *ht) != NILPORTTAB; ht = &pt->p_nextport)
	if (PORTCMP(&pt->p_port, p)) {
		if (pt->p_location != SOMEWHERE)
			return;
		if ((t = pt->p_threadlist) == s) {
			if ((pt->p_threadlist = s->pe_link) == NILTHREAD) {
				*ht = pt->p_nextport;
				pt->p_location = NOWHERE;	/* for dump */
				pt->p_nextport = portfree;
				portfree = pt;
			}
			s->pe_location = NOWHERE;
			wakeup((event) &s->pe_location);
		}
		else {
			while (t != NILTHREAD)
				if (t->pe_link == s) {
					t->pe_link = s->pe_link;
					s->pe_location = NOWHERE;
					wakeup((event) &s->pe_location);
					break;
				}
				else
					t = t->pe_link;
		}
		return;
	}
}

#ifndef NONET

#define NHERE 8
static port heretab[NHERE];

portask(asker, ptab, nports)		/* handle locate request */
address asker;
register port *ptab;
unsigned nports;
{
  register port *p,*q;
  register struct porttab **ht, *pt;
  register nfound;

  STINC(pts_portask);
  nfound = 0; q = heretab;
  for(p=ptab; nports--; p++) {
	ht = &hashtab[hash(p)];
	for (pt = *ht; pt != NILPORTTAB; pt = pt->p_nextport)
		if (PORTCMP(&pt->p_port, p)) {	/* found it */
			if (area(pt->p_location) == LOCAL) {
				if (pt->p_threadlist == 0) {
					/* just record someone was interested */
					pt->p_asker = asker;
					break;
				}
				if (nfound < NHERE) {
					*q++ = *p;
					nfound++;
				}
				pt->p_idle = 0;
			}
		}
  }
  if (nfound) {
	STINC(pts_portyes);
	hereport(asker, heretab, nfound);
  }
}
#endif

#ifndef SMALL_KERNEL
int
portdump(begin, end)
char *	begin;	/* start of buffer in which to write dump */
char *	end;	/* end of buffer */
{
    char *	bprintf();

    register struct porttab *	pt;
    register struct thread *	t;
    register char *		p;

    p = bprintf(begin, end, "\n PORT  LOCATION IDLE THREADS\n");
    for (pt = porttab; pt < lastport; pt++)
	if (pt->p_location != NOWHERE)
	{
	    p = bprintf(p, end, "%p   %4x   %4d",
			&pt->p_port, pt->p_location, pt->p_idle);
	    for (t = pt->p_threadlist; t != NILTHREAD; t = t->pe_link)
		p = bprintf(p, end, " {%d, %x}", THREADSLOT(t), t->pe_location);
	    p = bprintf(p, end, "\n");
	}
#ifdef STATISTICS
    p = bprintf(p, end, "alloc:    %6d aged:     %6d full:     %6d wakeup:   %6d\n",
	portstat.pts_alloc, portstat.pts_aged, portstat.pts_full, portstat.pts_wakeup);
    p = bprintf(p, end, "here:     %6d lookup:   %6d flocal:   %6d fglobal:  %6d\n",
	portstat.pts_here, portstat.pts_lookup, portstat.pts_flocal, portstat.pts_fglobal);
    p = bprintf(p, end, "portask:  %6d portyes:  %6d locate:   %6d\n",
	portstat.pts_portask, portstat.pts_portyes, portstat.pts_locate);
    p = bprintf(p, end, "nolocate: %6d relocate: %6d\n",
	portstat.pts_nolocate, portstat.pts_relocate);
#endif /* STATISTICS */
    return p - begin;
}
#endif

/* called when freelist was empty, will throw away all mortal ports */
portpurge() {
  register struct porttab *pt,**ht,**htp;
 
  for (htp=hashtab; htp< &hashtab[NHASH]; htp++) {
	ht = htp;
	while ((pt = *ht) != 0) {
		if (pt->p_threadlist == 0){
			*ht = pt->p_nextport;
			pt->p_location = NOWHERE;	/* for dump */
			pt->p_nextport = portfree;
			portfree = pt;
		} else
			ht = &pt->p_nextport;
	}
  }
}

#define MAXSWEEP	 3000	/* dseconds maximum idle time for port */
#define SWEEPRESOLUTION	  100	/* accuracy */

/*ARGSUSED*/
void portsweep(arg)
long arg;
{
  register struct porttab *pt,**ht,**htp;
  static uint16 cnt;

#ifndef NONET
  locthissweep = 0;
  if (loctim && ++loccnt > loctim) {              /* send a locate message */
	STINC(pts_relocate);
        sendloc();
        loctim <<= 1;
        if (loctim > maxloccnt)
                loctim = maxloccnt;
	locthissweep = 0;
  }
#endif NONET
  if (++cnt<SWEEPRESOLUTION)
	return;
  for (htp=hashtab; htp< &hashtab[NHASH]; htp++) {
	ht = htp;
	while ((pt = *ht) != 0) {
		if (pt->p_threadlist == 0 && (pt->p_idle += cnt) > MAXSWEEP) {
			*ht = pt->p_nextport;
			pt->p_location = NOWHERE;	/* for dump */
			pt->p_nextport = portfree;
			portfree = pt;
			STINC(pts_aged);
		} else
			ht = &pt->p_nextport;
	}
  }
  cnt=0;
}

/* Initialize tables and free list */
void
portinit(){
  register struct porttab *pt;
  extern char *aalloc();

  porttab = (struct porttab *) aalloc((vir_bytes) nport * sizeof(struct porttab), 0);
  lastport = &porttab[nport];
  for (pt = porttab; pt < lastport; pt++) {
	pt->p_nextport = portfree;
	portfree = pt;
  }
  sweeper_set(portsweep, 0L, 100, 0);
}
