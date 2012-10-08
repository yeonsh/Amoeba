/*	@(#)trans.c	1.3	94/04/06 08:34:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define TRANS

/*
 * This module implements the transaction mechanism. The transaction
 * calls are:
 *	getreq(header, buffer, count);
 *	putrep(header, buffer, count);
 *	trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2);
 *
 * ``Getreq'' is called by servers that want to wait for a request.
 * ``Putrep'' is called by servers that what to send a reply to a client.
 * ``Trans'' is called by clients that want to send a request to a server.
 * Requests are addressed by the ``h_port'' field in the header structure.
 * Replies are automatically sent back to the corresponding client. Between
 * a ``getreq'' and a ``putrep'' the server may not call ``getreq''. All
 * the three calls are blocking.
 */
#ifndef NONET
/*
 * The following network interface routines must be defined externally:
 *
 *	puthead(dest, source, ident, seq, type, size);
 *	append(data, size, send);
 *	pickoff(data, size);
 *
 * ``Puthead'' is called first when a packet has to be sent. Among other
 * things the destination and the size are specified. If this size is zero,
 * the packet must be sent immediately.
 * ``Append'' is called when more data must be appended to a packet. The
 * ``send'' parameter is set when the packet can be sent.
 * ``Pickoff'' is called when a packet has arrived. The specified number
 * of bytes must copied to the specified data buffer.
 *
 * When a packet arrives, the local routine ``handle'' must be called:
 *	handle(dest, source, ident, seq, type, size, hdr);
 * ``Hdr'' contains the first HEADERSIZE data bytes. With a call to
 * ``getall'' this buffer is enlarged to contain the full packet.
 */
#endif /*NONET*/

#if !defined(__STDC__) && !defined(NEED_VOLATILE)
#define volatile
#endif
#define inline

#include "amoeba.h"
#include "machdep.h"
#include "byteorder.h"
#include "exception.h"
#include "global.h"
#include "kthread.h"
#include "internet.h"
#include "assert.h"
INIT_ASSERT
#include "map.h"
#include "cmdreg.h"
#include "stderr.h"

#define FIRSTSIZE	(PACKETSIZE - HEADERSIZE)

#define MAXCNT		30000		/* maximum buffer size */

#define threadptr(to)	(&thread[threadnum(to)])

static port nullport;

#ifndef NONET
extern uint16 maxcrash;
extern uint16 retranstime, crashtime, clientcrash;
extern uint16 maxretrans, mincrash;
#endif

static address local;

#ifdef USETICKER
#define STARTTICK(a) (a -= getmilli())
#define STOPTICK(a) (a += getmilli())
#else
#define STARTTICK(a)
#define STOPTICK(a)
#endif

extern address portlookup();
extern phys_bytes umap();

#ifdef STATISTICS
#define SIZEBINSIZE	16	/* Actually log2(MAXCNT+1)+1 */
struct rpcstat {
	long rpcs_clfail;
	long rpcs_svfail;
	long rpcs_clcrash;
	long rpcs_rxcl;
	long rpcs_rxsv;
	long rpcs_trans;
	long rpcs_loctrans;
	long rpcs_remtrans;
	long rpcs_getreq;
	long rpcs_putrep;
	long rpcs_naks;
	long rpcs_nloc;

	long rpcs_grqbin[SIZEBINSIZE];
	long rpcs_prpbin[SIZEBINSIZE];
	long rpcs_prqbin[SIZEBINSIZE];
	long rpcs_grpbin[SIZEBINSIZE];
} rpcstat;

static
twologenplusone(n)
bufsize n;
{
	register log;

	log = -1;
	n++;
	do {
		n >>= 1;
		log++;
	} while(n != 0);
	compare(log, >=, 0);
	compare(log, <, SIZEBINSIZE);
	return log;
}
#define INCSTAT(x)		rpcstat.x++
#define INCSIZEBIN(x, size) 	rpcstat.x[twologenplusone(size)]++
#else
#define INCSTAT(x)		/* nothing */
#define INCSIZEBIN(x, size)	/* nothing */
#endif

/* return where the ``location'' can be found:
 *	DONTKNOW:	the location is unknown;
 *	LOCAL:		it's on this machine;
 *	GLOBAL:		it's probably somewhere on the net.
 */
inline
area(location)
register address location;
{
  if (location == SOMEWHERE)
	return(DONTKNOW);
  if (siteaddr(location) == local)
	return(LOCAL);
#ifdef NONET
  assert(0);
  /*NOTREACHED*/
#else
  return(GLOBAL);
#endif
}

/* (re)start getreq
 */
static getreqstart(t)
register struct thread *t;
{
  t->ts_state = WAITBUF;
  t->ts_seq = 0;
  t->ts_offset = 0;
  portinstall(&t->ts_rhdr->h_port, t->ts_addr, WAIT);
}

#ifndef NONET	/*--------------------------------------------------*/

/*
** headerfromnet - convert byte sex to host format.
*/
/*ARGSUSED*/
inline
static headerfromnet(hdr)
header *hdr;
{
  dec_s_be(&hdr->h_command);
  dec_s_be(&hdr->h_size);
  dec_l_be(&hdr->h_offset);
  dec_s_be(&hdr->h_extra);
}

/*
** headertonet - convert byte sex to network format.
*/
/*ARGSUSED*/
inline
static headertonet(hdr)
header *hdr;
{
  enc_s_be(&hdr->h_command);
  enc_s_be(&hdr->h_size);
  enc_l_be(&hdr->h_offset);
  enc_s_be(&hdr->h_extra);
}

/* A locate message has arrived. This routine handles it by looking the
 * ports up in the cache, and if it finds some there that are local it
 * sends a reply back with those ports.
 */

static locreq(ph, ptab)         /* handle locate request */
register struct pktheader *ph;
register char *ptab;
{

  getall();
  portask(pktfrom(ph), (port *) ptab, ph->ph_size/PORTSIZE);
  netenable();
}

/* called from portcache.c */
/*
** hereport - Send answer to a locate message (from portcache.c)
*/
hereport(asker, ptab, nports)
address asker;
port *ptab;
{

  puthead(asker, local, 0, 0, HERE, (uint16) (nports*PORTSIZE));
  append(VTOP(ptab), (unsigned) nports*PORTSIZE, SEND);
}

/*
** whereport - send a locate broadcast.
*/
whereport(ptab, nports)
port *ptab;
{

  INCSTAT(rpcs_nloc);
  puthead(BROADCAST, local, 0, 0, LOCATE, (uint16) (nports*PORTSIZE));
  append(VTOP(ptab), (unsigned) nports*PORTSIZE, SEND);
}

/* start getrep
 */
inline
static getrepstart(t)
register struct thread *t;
{
  t->ts_flags &= ~PUTREQ;
  t->ts_state = WAITBUF;
  t->ts_seq = 0;
  t->ts_offset = 0;
  t->ts_flags |= GETREP;
  t->ts_timer = crashtime;
  t->ts_count = maxcrash;
}

/* A reply on a locate message has arrived. Store the ports in the cache.
 */
static here(ph, ptab)	/* handle locate reply */
register struct pktheader *ph;
register char *ptab;
{
  register port *p;
  register uint16 from = pktfrom(ph);

  getall();
  p = ABSPTR(port *, ptab + ph->ph_size);
  while ((char *) --p >= ptab)
	portinstall(p, from, NOWAIT);
  netenable();
}

/* After I've enquired about the health of a server, the processor answered
 * that it's fine. Thank goodness. But keep asking.
 */
static alive(ph)
register struct pktheader *ph;
{
  register struct thread *t = &thread[ph->ph_dstthread&0xFF];

  netenable();
  if (t->ts_server == pktfrom(ph) && t->ts_svident == ph->ph_ident &&
						(t->ts_flags & GETREP)) {
	t->ts_timer = crashtime;
	t->ts_count = maxcrash;
	t->ts_signal = 0;
  }
}

/* After I've enquired about the health of a server, the processor answered
 * that it's dead. Too bad. Notify client.
 */
static dead(ph)
register struct pktheader *ph;
{
  register struct thread *t = &thread[ph->ph_dstthread&0xFF];

  netenable();
  if (t->ts_server == pktfrom(ph) && t->ts_svident == ph->ph_ident &&
						(t->ts_flags & GETREP)) {
	t->ts_timer = 0;
	t->ts_state = FAILED;
	wakeup((event) &t->ts_state);
  }
}

/* Someone enquires how a server doing. Inform him. A signal may be sent
 * along with the enquiry. Handle that.
 */
static enquiry(ph)
register struct pktheader *ph;
{
  register uint16 from = pktfrom(ph), to = pktto(ph);
  register struct thread *t = &thread[ph->ph_dstthread&0xFF];

  netenable();
  if (t->ts_client == from && t->ts_clident == ph->ph_ident) {
  	if (t->ts_flags & SERVING) {
		t->ts_cltim = clientcrash;
		puthead(from, to, ph->ph_ident, 0, ALIVE, 0);
		if (ph->ph_signal != 0)
			putsig(t, (signum) SIG_TRANS);

	}
  }
  else
	puthead(from, to, ph->ph_ident, 0, DEAD, 0);
}

/* Send a fragment of a packet. If it's the first, insert the header.
 */
static void sendfragment(t, what)
struct thread *t;
uint16 what;
{
  register address to;
  register short size = t->ts_xcnt - t->ts_offset;

  if (t->ts_flags & PUTREQ) {
	to = t->ts_server;
	what |= REQUEST;
  } else {
	to = t->ts_client;
	what |= REPLY;
  }
  if (t->ts_seq == 0) {		/* first fragment--append header */
	if (size <= FIRSTSIZE)		/* first and last */
		what |= LAST;
	else				/* first but not last */
		size = FIRSTSIZE;
	puthead(to, t->ts_addr, t->ts_ident, t->ts_seq, (char) what,
						(uint16) size + HEADERSIZE);
	headertonet(t->ts_xhdr);
	append(VTOP(t->ts_xhdr), HEADERSIZE, size == 0 ? SEND : NOSEND);
	headerfromnet(t->ts_xhdr);
  } else {			/* not first */
	if (size <= PACKETSIZE)		/* last but not first */
		what |= LAST;
	else				/* not first and not last */
		size = PACKETSIZE;
	puthead(to, t->ts_addr, t->ts_ident, t->ts_seq, (char) what,
							(uint16) size);
  }
  if( size != 0 )
	append((phys_bytes) (t->ts_xbaddr + t->ts_offset), (uint16) size, SEND);
}

/* Send a message. Call sendfragment to send the first fragment. When an
 * acknowledgement arrives, the interrupt routine handling it will send
 * the next fragment, if necessary.
 */
static send(){
  register struct thread *c = curthread;
  register uint16 what = 0;

  c->ts_state = SENDING;
  c->ts_ident = c->ts_flags & PUTREQ ? c->ts_svident : c->ts_clident;
  c->ts_seq = 0;
  c->ts_count = maxretrans;
  do {
	sendfragment(c, what);
	if (c->ts_state == SENDING) {
		c->ts_timer = retranstime;
		if (await((event) &c->ts_state, 0L)) {
			c->ts_timer = 0;
			c->ts_state = ABORT;
			return;
		}
	}
	if (c->ts_state == ACKED) {
		c->ts_state = SENDING;
		what = 0;
	}
	else /* if (c->ts_state == SENDING) */
		what = RETRANS;
  } while (c->ts_state == SENDING);
}

/* An acknowledgement for a fragment arrived. If it wasn't the last fragment,
 * send the next. Else if it was a putrep, restart the thread. Else it was the
 * putreq part of a transaction, start up the getrep part.
 */
static gotack(ph)
register struct pktheader *ph;
{
  register uint16 to = pktto(ph);
  register uint16 from = pktfrom(ph);
  register struct thread *t = &thread[ph->ph_dstthread&0xFF];

  netenable();
  if (t->ts_state == SENDING && t->ts_ident == ph->ph_ident &&
						t->ts_seq == ph->ph_seq) {
	if (ph->ph_seq == 0) {		/* first fragment */
		if (t->ts_flags & PUTREQ)
			t->ts_server = from;
		else if (t->ts_client != from)
			return;
		t->ts_offset += FIRSTSIZE;
	}
	else
		t->ts_offset += PACKETSIZE;
	t->ts_timer = 0;
	if (t->ts_offset >= t->ts_xcnt)		/* ack for last fragment */
		if (t->ts_flags & PUTREQ) {	/* in a transaction */
			getrepstart(t);
			if (t->ts_signal != 0)
				puthead(from, to, ph->ph_ident, t->ts_signal,
								ENQUIRY, 0);
		} else {			/* putrep done */
			assert(t->ts_flags & PUTREP);
			t->ts_state = DONE;
			wakeup((event) &t->ts_state);
		}
	else {
		t->ts_seq++;
		sendfragment(t, 0);
		t->ts_timer = retranstime;
		t->ts_count = maxretrans;
	}
  }
}

/* A nak has arrived. Obviously the server was not at the assumed address.
 * Wake up the thread, to do a new locate.
 */
static gotnak(ph)
register struct pktheader *ph;
{
  register struct thread *t = &thread[ph->ph_dstthread&0xFF];

  netenable();
  if (t->ts_state == SENDING && t->ts_ident == ph->ph_ident && t->ts_seq == 0
	  && (t->ts_flags & PUTREQ) && t->ts_server == (ph->ph_srcnode&0xFF)) {
	t->ts_timer = 0;
	t->ts_state = NACKED;
	wakeup((event) &t->ts_state);
  }
}

/* A fragment has arrived. Get the data and see if more fragments are expected.
 */
inline
static received(t, what, size, hdr)
struct thread *t;
char what, *hdr;
uint16 size;
{
  register uint16 cnt = t->ts_rcnt - t->ts_offset, n;
  register phys_bytes rbuf;

  if (cnt > size)
	cnt = size;
  if (cnt != 0) {
	rbuf = t->ts_rbaddr+t->ts_offset;
	t->ts_offset += cnt;
	if (t->ts_seq != 0) {	/* copy the ``header'' */
		n = cnt < HEADERSIZE ? cnt : HEADERSIZE;
		phys_copy(VTOP(hdr), rbuf, (phys_bytes) n);
		rbuf += n;
		cnt -= n;
	}
	if (cnt != 0)
		pickoff(rbuf, cnt);
  }
  netenable();
  if (what & LAST) {
	t->ts_timer = 0;
	t->ts_state = DONE;
	wakeup((event) &t->ts_state);
  }
  else {
	t->ts_seq++;
	t->ts_timer = crashtime;
	t->ts_count = mincrash;
	t->ts_state = RECEIVING;
  }
}

/* See if someone is handling the request for `from.'
 */
static struct thread *find(from, ident)
address from;
char ident;
{
  register struct thread *t;

  for (t = thread; t < upperthread; t++)
	if ((t->ts_flags & (GETREQ | SERVING | PUTREP)) &&
				t->ts_client == from && t->ts_clident == ident)
		return(t);
  return(NILTHREAD);
}

/* A request packet has arrived. Find out which thread this packet should go to.
 * Send an acknowledgement if it can't do any harm.
 */
static gotrequest(ph, hdr)
register struct pktheader *ph;
header *hdr;
{
  register struct thread *t;
  register unsigned /* uint16 */ from = pktfrom(ph), to, size = ph->ph_size;

  if (ph->ph_seq == 0)	/* only the first fragment is really interesting */
	if ((ph->ph_type & RETRANS) &&
				(t = find(from, ph->ph_ident)) != NILTHREAD) {
		netenable();		/* ack got lost, send it again */
		puthead(from, t->ts_addr, ph->ph_ident, ph->ph_seq, ACK, 0);
	}
	else {
		register address location;

		location = portlookup(&hdr->h_port, NOWAIT, DELETE);
		if (location == NOWHERE || siteaddr(location) != local) {
			netenable();
			puthead(from, pktto(ph), ph->ph_ident, 0, NAK, 0);
			return;
		}
		size -= HEADERSIZE;
		t = threadptr(location);
		t->ts_client = from;
		t->ts_clident = ph->ph_ident;
		*t->ts_rhdr = *hdr;
		headerfromnet(t->ts_rhdr);
		if ((ph->ph_type & (LAST | RETRANS)) != LAST)
			puthead(from, location, ph->ph_ident, 0, ACK, 0);
		received(t, ph->ph_type, size, (char *) 0);
	}
  else {	/* seq != 0 */
	to = pktto(ph);
	t = &thread[ph->ph_dstthread&0xFF];
	if (t->ts_state != RECEIVING || ph->ph_seq != t->ts_seq) {
		netenable();
		puthead(from, to, ph->ph_ident, ph->ph_seq, ACK, 0);
	}
	else {
		if ((ph->ph_type & (LAST | RETRANS)) != LAST)
			puthead(from, to, ph->ph_ident, ph->ph_seq, ACK, 0);
		received(t, ph->ph_type, size, (char *) hdr);
	}
  }
}

/* A reply packet has arrived. Send an acknowledgement if it can't do any
 * harm.
 */
inline
static gotreply(ph, hdr)
register struct pktheader *ph;
header *hdr;
{
  register uint16 from = pktfrom(ph), to = pktto(ph), size = ph->ph_size;
  register struct thread *t = &thread[ph->ph_dstthread&0xFF];

  if (ph->ph_ident != t->ts_svident)
	t = NILTHREAD;
  else if ((t->ts_flags & GETREP) == 0) {
	if (t->ts_flags & PUTREQ) {	/* ack for request got lost */
		compare(t->ts_ident, ==, ph->ph_ident);
		getrepstart(t);		/* start the getrep */
		t->ts_signal = 0;
	}
	else
		t = NILTHREAD;
  } else
	if (ph->ph_seq != t->ts_seq)
		t = NILTHREAD;
  if (t != NILTHREAD) {
	if (ph->ph_seq == 0) {
		*t->ts_rhdr = *hdr;
		headerfromnet(t->ts_rhdr);
		size -= HEADERSIZE;
	}
	else if (t->ts_state != RECEIVING)
		t = NILTHREAD;
  }
  if (t == NILTHREAD) {
	netenable();
	puthead(from, to, ph->ph_ident, ph->ph_seq, ACK, 0);
  }
  else {
	puthead(from, to, ph->ph_ident, ph->ph_seq, ACK, 0);
	received(t, ph->ph_type, size, (char *) hdr);
  }
}

/* A packet has arrived. Call an appropiate routine, after checking some
 * things.
 */
pkthandle(ph, hdr)
register struct pktheader *ph;
register char *hdr;
{
  if (ph->ph_dstthread < totthread && ph->ph_size <= PACKETSIZE)
	switch (ph->ph_type & TYPE) {
  	case LOCATE:
		if (ph->ph_size == 0 || ph->ph_ident != 0 || ph->ph_seq != 0)
			break;
		if ((ph->ph_size % PORTSIZE) != 0)
			break;
		locreq(ph, hdr);
		return(1);
	case REQUEST:
		if (ph->ph_seq == 0 && ph->ph_size < HEADERSIZE)
			break;
		gotrequest(ph, ABSPTR(header *, hdr));
		return(1);
	case ACK:
		if (ph->ph_size != 0)
			break;
		gotack(ph);
		return(1);
	case ENQUIRY:
		if (ph->ph_size != 0)
			break;
		enquiry(ph);
		return(1);
	case HERE:
		if (ph->ph_size == 0 || ph->ph_ident != 0 || ph->ph_seq != 0)
			break;
		if ((ph->ph_size % PORTSIZE) != 0)
			break;
		here(ph, hdr);
		return(1);
	case REPLY:
		if (ph->ph_seq == 0 && ph->ph_size < HEADERSIZE)
			break;
		gotreply(ph, ABSPTR(header *, hdr));
		return(1);
	case NAK:
		if (ph->ph_size != 0 || ph->ph_seq != 0)
			break;
		gotnak(ph);
		return(1);
	case ALIVE:
		if (ph->ph_size != 0 || ph->ph_seq != 0)
			break;
		alive(ph);
		return(1);
	case DEAD:
		if (ph->ph_size != 0 || ph->ph_seq != 0)
			break;
		dead(ph);
		return(1);
	case 0:
		return(0);
  }
  return(0);
}

handle(to, from, ident, seq, what, size, hdr)	/* compatibility */
address to, from;
char ident, seq, what;
uint16 size;
char *hdr;
{
  struct pktheader ph;

  ph.ph_dstnode = siteaddr(to);
  ph.ph_srcnode = siteaddr(from);
  ph.ph_dstthread = threadnum(to);
  ph.ph_srcthread = threadnum(from);
  ph.ph_ident = ident;
  ph.ph_seq = seq;
  ph.ph_size = size;
  ph.ph_type = what;
  return pkthandle(&ph, hdr);
}

/* A timer has gone off too many times. See what went wrong. Restart the thread.
 */
static failed(t)
register struct thread *t;
{
  assert(t->ts_flags & (GETREQ | PUTREP | GETREP | PUTREQ));
#ifndef SMALL_KERNEL
  if (t->ts_flags & (GETREQ | PUTREP))
	printf("%x: client %x failed (%d)\n", THREADSLOT(t),
					t->ts_client, t->ts_state);
  if (t->ts_flags & (GETREP | PUTREQ))
	printf("%x: server %x failed (%d)\n", THREADSLOT(t),
					t->ts_server, t->ts_state);
#endif
#ifdef STATISTICS
  if (t->ts_flags & (GETREQ | PUTREP))
	INCSTAT(rpcs_clfail);
  if (t->ts_flags & (GETREP | PUTREQ))
	INCSTAT(rpcs_svfail);
#endif
  switch (t->ts_state) {
  case SENDING:		/* Message didn't arrive */
	t->ts_state = FAILED;
	assert(t->ts_flags & (PUTREQ | PUTREP));
	break;
  case WAITBUF:		/* server site has crashed */
	assert(t->ts_flags & GETREP);
  case RECEIVING:
	if (t->ts_flags & GETREP)
		t->ts_state = FAILED;
	else {
		getreqstart(t);	/* client failed, restart getreq */
		return;
	}
	break;
  default:
	assert(0);
  }
  wakeup((event) &t->ts_state);
}

/* A timer went off. See what is wrong.
 */
static again(t)
register struct thread *t;
{
  switch (t->ts_state) {
  case SENDING:			/* retransmit */
#ifdef STATISTICS
  if (t->ts_flags & (GETREQ | PUTREP))
	INCSTAT(rpcs_rxcl);
  if (t->ts_flags & (GETREP | PUTREQ))
	INCSTAT(rpcs_rxsv);
#endif
	sendfragment(t, RETRANS);
	t->ts_timer = retranstime;
	break;
  case WAITBUF:			/* Check if the server is still alive */
	assert(t->ts_flags & GETREP);
  case RECEIVING:
	if (t->ts_flags & GETREP)  /* See if the other side is still there */
		puthead(t->ts_server, t->ts_addr, t->ts_svident,
						t->ts_signal, ENQUIRY, 0);
	t->ts_timer = retranstime;
	break;
  default:
	assert(0);
  }
}

#endif /*NONET*/ /*--------------------------------------------------*/

/* First check all the timers. If any went off call the appropiate routine.
 * Then see if there are ports to locate.
 */
/*ARGSUSED*/
void netsweep(arg)
long arg;
{
  register struct thread *t;

  for (t = thread; t < upperthread; t++) {
	if (t->ts_timer != 0 && --t->ts_timer == 0) {	/* timer expired */
		if (t->ts_flags & LOCATING)
			portquit(&t->ts_xhdr->h_port, t);
		else {
#ifndef NONET
			compare(t->ts_count, !=, 0);
			if (--t->ts_count == 0)		/* serious */
				failed(t);
			else				/* try again */
				again(t);
			break;
#endif
		}
	}
#ifndef NONET
	if (t->ts_cltim != 0 && --t->ts_cltim == 0) {	/* client crashed */
		INCSTAT(rpcs_clcrash);
		putsig(t, (signum) SIG_TRANS);
	}
#endif /*NONET*/
  }
}

/* The transaction is local. This routine does the sending.
 */
static sendbuf(t)
register struct thread *t;
{
  register struct thread *c = curthread;
  register uint16 cnt = t->ts_rcnt < c->ts_xcnt ? t->ts_rcnt : c->ts_xcnt;

  compare(t->ts_state, ==, WAITBUF);
  if (cnt != 0)
	phys_copy(c->ts_xbaddr, t->ts_rbaddr, (phys_bytes) cnt);
  t->ts_offset = cnt;
  t->ts_state = DONE;
  wakeup((event) &t->ts_state);
  c->ts_state = DONE;
}

/* The transaction is local. Send the request to the server.
 */
static recvrequest(){
  register address to;
  register struct thread *c = curthread, *t;

  if ((to = portlookup(&c->ts_xhdr->h_port, NOWAIT, DELETE)) == NOWHERE)
	return(0);
#ifndef NONET
  if (siteaddr(to) != local)
	return(0);
#endif
  t = threadptr(to);
  c->ts_server = to;
  t->ts_client = c->ts_addr;
  t->ts_clident = c->ts_svident;
  *t->ts_rhdr = *c->ts_xhdr;
  sendbuf(t);
  return(c->ts_xcnt != (uint16) RPC_FAILURE);
}

/* A thread calls this routine when it wants to be blocked awaiting a request.
 * It specifies the header containing the port to wait for, a buffer where
 * the data must go and the size of this buffer. Getreq returns the size of
 * the request when one arrives.
 */
#ifdef __STDC__
bufsize getreq(header *hdr, bufptr buf, bufsize cnt)
#else
bufsize getreq(hdr, buf, cnt)
header *hdr;
bufptr buf;
bufsize cnt;
#endif
{
  void	progerror();

  register struct thread *c = curthread;

  if (c->ts_flags != 0 || cnt > MAXCNT) {
#ifndef ROMKERNEL
	if (!userthread())
		printf("getreq: flags = %d, cnt = %d\n", c->ts_flags, cnt);
#endif
	progerror();
	return((bufsize) RPC_FAILURE);
  }
  if (NULLPORT(&hdr->h_port)) {
	return((bufsize) RPC_BADPORT);
  }
  INCSTAT(rpcs_getreq);
  if (cnt == 0 || (c->ts_rbaddr = umap(c, (vir_bytes) buf, (vir_bytes) cnt, 1))!=0) {
	c->ts_rhdr = hdr;
	c->ts_rcnt = cnt;
	c->ts_flags |= GETREQ;
	getreqstart(c);
	if (await((event) &c->ts_state, 0L)) {
		portremove(&hdr->h_port, c->ts_addr);
		c->ts_state = ABORT;
	}
  } else
	c->ts_state = MEMFAULT;
  c->ts_flags &= ~GETREQ;
  switch (c->ts_state) {
  case DONE:
	compare(c->ts_state, ==, DONE);
	c->ts_flags |= SERVING;
#ifndef NONET
	if (area(c->ts_client) != LOCAL) {
		c->ts_cltim = clientcrash;
		INCSIZEBIN(rpcs_grqbin, c->ts_offset);
	}
#endif /*NONET*/
	cnt = c->ts_offset;
	break;
  case ABORT:
	cnt = RPC_ABORTED;
	break;
  case MEMFAULT:
	cnt = RPC_BADADDRESS;
	break;
  default:
	panic("getreq: illegal state 0x%x\n", c->ts_state);
  }
  c->ts_state = IDLE;
  if (cnt == (bufsize) RPC_BADADDRESS) {
#ifndef ROMKERNEL
	if (!userthread())
		printf("getreq: got a memory fault\n");
#endif
	progerror();
  }
  return(cnt);
}

/* A thread wants to send a reply to its client. Putrep returns the size of
 * the reply.
 */
#ifdef __STDC__
void putrep(header *hdr, bufptr buf, bufsize cnt)
#else
void putrep(hdr, buf, cnt)
header *hdr;
bufptr buf;
bufsize cnt;
#endif
{
  void	progerror();

  register struct thread *c = curthread;

  if (c->ts_flags & FASTKILLED) {
	/* Client long gone, forget it */
	c->ts_flags &= ~(FASTKILLED|SERVING);
	c->ts_state = IDLE;
	printf("%d: client dead\n", THREADSLOT(c));
	return;
  }
  if (c->ts_flags != SERVING) {
#ifndef ROMKERNEL
	if (!userthread())
		printf("putrep: tried to do putrep without a getreq\n");
#endif
	progerror();
	return;
  }
  c->ts_flags &= ~SERVING;
  if (cnt > MAXCNT) {
#ifndef ROMKERNEL
	if (!userthread())
		printf("putrep: byte count (%d) too big\n", cnt);
#endif
	progerror();
	return;
  }
  INCSTAT(rpcs_putrep);
  if (cnt==0 || (c->ts_xbaddr = umap(c, (vir_bytes) buf, (vir_bytes) cnt, 0)) != 0) {
	c->ts_cltim = 0;
	c->ts_xhdr = hdr;
	c->ts_xcnt = cnt;
	c->ts_offset = 0;
	c->ts_flags |= PUTREP;
#ifndef NONET
	if (siteaddr(c->ts_client) != local) {
		INCSIZEBIN(rpcs_prpbin, cnt);
		send();
	} else
#endif
	{		/* local transaction */
		register struct thread *t = threadptr(c->ts_client);

		*t->ts_rhdr = *hdr;
		sendbuf(t);
	}
  } else
	c->ts_state = MEMFAULT;
  c->ts_flags &= ~PUTREP;
  switch (c->ts_state) {
  case FAILED:
  case ABORT:
  case DONE:
	break;
  case MEMFAULT:
	cnt = RPC_BADADDRESS;
	break;
  default:
	assert(0);
  }
  c->ts_state = IDLE;
  if (cnt == (bufsize) RPC_BADADDRESS) {
#ifndef ROMKERNEL
	if (!userthread())
	    printf("putrep: got a memory fault\n");
#endif
	progerror();
  }
  return;
}

#ifdef FLIP
int inlocalportcache(p)
    port *p;
{
  register struct thread *c = curthread;

  return(PORTCMP(&c->ts_portcache, p));
}
#endif /* FLIP */

/* Somebody wants to contact a server, and wait for a reply. The port this
 * server should listen to is specified in the first header. The reply
 * comes in the second. Trans returns the size of the reply, or RPC_FAILURE if
 * a transaction fails after the server has been located.
 */
bufsize
#ifdef FLIP
am_trans
#else
trans
#endif
#ifdef __STDC__
(header *hdr1, bufptr buf1, bufsize cnt1,
	      header *hdr2, bufptr buf2, bufsize cnt2)
#else
(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
header *hdr1, *hdr2;
bufptr buf1, buf2;
bufsize cnt1, cnt2;
#endif
{
  void	progerror();

  register struct thread *c = curthread;

  /*
  ** Do some argument checking.
  */
  if ((c->ts_flags & ~(SERVING|FASTKILLED)) || cnt1 > MAXCNT || cnt2 > MAXCNT) {
#ifndef ROMKERNEL
	if (!userthread())
		printf("trans: flags = %d, cnt1 = %d, cnt2 = %d\n",
						c->ts_flags, cnt1, cnt2);
#endif
	progerror();
	return((bufsize) RPC_FAILURE);
  }
  if (NULLPORT(&hdr1->h_port)) {
	return((bufsize) RPC_BADPORT);
  }
  INCSTAT(rpcs_trans);
  if ((cnt1 == 0 || (c->ts_xbaddr = umap(c, (vir_bytes) buf1, (vir_bytes) cnt1, 0))!= 0) &&
      (cnt2 == 0 || (c->ts_rbaddr = umap(c, (vir_bytes) buf2, (vir_bytes) cnt2, 1))!= 0)) {
    for (;;) {
	c->ts_state = IDLE;
	c->ts_xhdr = hdr1;
	c->ts_xcnt = cnt1;
	c->ts_rhdr = hdr2;
	c->ts_rcnt = cnt2;
	c->ts_signal = 0;
	if (!PORTCMP(&c->ts_portcache, &hdr1->h_port)) {
		c->ts_flags |= LOCATING;
		c->ts_timer = c->ts_maxloc;
		STARTTICK(c->ts_totloc);
		c->ts_server = portlookup(&hdr1->h_port, WAIT, LOOK);
		STOPTICK(c->ts_totloc);
		c->ts_timer = 0;
		c->ts_flags &= ~LOCATING;
		switch (c->ts_server) {
		case NOWHERE:		/* server not found */
			c->ts_portcache = nullport;
			return(bufsize) (c->ts_signal == 0 ? RPC_NOTFOUND : RPC_ABORTED);
		case SOMEWHERE:
			c->ts_portcache = nullport;
			return((bufsize) RPC_FAILURE);
		}
		c->ts_portcache = hdr1->h_port;
	} else
		c->ts_server = siteaddr(c->ts_server);
	c->ts_svident++;
	c->ts_offset = 0;
	c->ts_flags |= PUTREQ;
#ifndef NONET
	if (siteaddr(c->ts_server) != local) {
		INCSTAT(rpcs_remtrans);
		INCSIZEBIN(rpcs_prqbin, cnt1);
		STARTTICK(c->ts_totsvr);
		send();
		c->ts_flags &= ~PUTREQ;
		STOPTICK(c->ts_totsvr);
		if (c->ts_state == NACKED || c->ts_state == FAILED) {
			portremove(&hdr1->h_port, siteaddr(c->ts_server));
			c->ts_portcache = nullport;
		}
		if (c->ts_state != NACKED) {
			INCSIZEBIN(rpcs_grpbin, c->ts_offset);
			break;
		}
		INCSTAT(rpcs_naks);
		c->ts_portcache = nullport;
	}
	else
#endif /*NONET*/
	if (recvrequest()) {	/* local transaction */
		INCSTAT(rpcs_loctrans);
		c->ts_flags &= ~PUTREQ;
		c->ts_flags |= GETREP;
		STARTTICK(c->ts_totsvr);
		c->ts_offset = 0;
		c->ts_state = WAITBUF;
		if (c->ts_signal != 0) {
			putsig(threadptr(c->ts_server), (signum) SIG_TRANS);
			c->ts_signal = 0;
		}
		if (await((event) &c->ts_state, 0L)) {
			c->ts_state = ABORT;
		}
		STOPTICK(c->ts_totsvr);
		break;
	}
	else {		/* too bad, try again */
		c->ts_flags &= ~PUTREQ;
	}
	c->ts_portcache = nullport;
	if (c->ts_signal != 0) {
		c->ts_state = ABORT;
		break;
	}
    }
  } else
	c->ts_state = MEMFAULT;
  c->ts_signal = 0;
  c->ts_flags &= ~(PUTREQ | GETREP);
#ifndef SMALL_KERNEL
  if (c->ts_state != DONE && c->ts_state != ABORT) {
	  printf("trans failed with %x (state = %d; command = %d; port %p)\n",
			c->ts_server, c->ts_state, c->ts_xhdr->h_command,
			&c->ts_xhdr->h_port);
  }
#endif
  switch (c->ts_state) {
  case DONE:
	cnt2 = c->ts_offset;
	break;
  case FAILED:
  case ABORT:
	cnt2 = RPC_FAILURE;
	break;
  case MEMFAULT:
	cnt2 = RPC_BADADDRESS;
	break;
  default:
#ifndef notdef
	cnt2 = RPC_FAILURE;
#else
	assert(0);
#endif
  }
  c->ts_state = IDLE;
  if (cnt2 == (bufsize) RPC_BADADDRESS) {
#ifndef ROMKERNEL
	if (!userthread())
	    printf("trans: got a memory fault\n");
#endif
	progerror();
  }
  return(cnt2);
}

/* If doing a transaction, send a signal to the server. For a remote server,
 * the signal is sent along with enquiries. If it's still locating a server,
 * abort that.  If it isn't doing a transaction, but blocked in a getreq,
 * abort that.
 */
sendsig(t, sig)
register struct thread *t;
char sig;
{
  if( t->ts_flags == 0 )
	return(0);
  sig = -1;
  if (t->ts_flags & (LOCATING | PUTREQ | GETREP))
	t->ts_signal = sig;
  if (t->ts_flags & LOCATING)
	portquit(&t->ts_xhdr->h_port, t);
  else if (t->ts_state == WAITBUF)
	if (t->ts_flags & GETREQ) {
		portremove(&t->ts_rhdr->h_port, t->ts_addr);
		t->ts_state = ABORT;
		wakeup((event) &t->ts_state);
	}
	else if ( sig != 0 && (t->ts_flags & GETREP)) {
#ifndef NONET
		if (area(t->ts_server) != LOCAL) {
			puthead(t->ts_server, t->ts_addr, t->ts_svident,
							sig, ENQUIRY, 0);
		} else 
#endif /*NONET*/
		{
			putsig(threadptr(t->ts_server), (signum) SIG_TRANS);
		}
		t->ts_signal = 0;
	}
  if( t->ts_state == IDLE && (t->ts_flags & SERVING) )
	return(0);
  return(1);
}

/* Abort anything thread s is doing. If the thread is serving somebody, notify
 * it that the server has failed.
 */
destroy(s)
register struct thread *s;
{
  register struct thread *t;

  (void) sendsig(s, (char) SIG_TRANS);
  if ( (s->ts_flags & SERVING) && !(s->ts_flags & FASTKILLED))
#ifndef NONET
	if (area(s->ts_client) != LOCAL) {
		puthead(s->ts_client, s->ts_addr, s->ts_clident, 0, DEAD, 0);
		s->ts_cltim = 0;
	}
	else
#endif
	{
#ifndef SMALL_KERNEL
		printf("%x destroyed, %x victim\n", s->ts_addr, s->ts_client);
#endif
		t = threadptr(s->ts_client);
		compare(t->ts_state, ==, WAITBUF);
		assert(t->ts_flags & GETREP);
		t->ts_timer = 0;
		t->ts_state = FAILED;
		wakeup((event) &t->ts_state);
	}
  s->ts_timer = 0;
  if (s->ts_flags & (LOCATING|GETREQ|GETREP|PUTREQ|PUTREP)) {
	if ((s->ts_flags & (PUTREQ|GETREP)) && siteaddr(s->ts_server) == local) {

	    /*
	     * Make sure the server doesn't try to putrep
	     * Do this by overloading the FASTKILLED flag
	     */
	    t = threadptr(s->ts_server);
#ifndef SMALL_KERNEL
	    printf("%d: fast-killing %d\n", THREADSLOT(s), THREADSLOT(t));
#endif
	    t->ts_flags |= FASTKILLED;
	}
	s->ts_state = ABORT;
	wakeup((event) &s->ts_state);
  }
  else {
	s->ts_state = IDLE;
	s->ts_flags = 0;
	s->ts_signal = 0;
  }
}

/* Clean up the mess.
 */
cleanup(){
  register struct thread *c = curthread;

#ifndef SMALL_KERNEL
  if( c->ts_state != IDLE ) {
      (void) mpxdump((char *) 0, (char *) 0);
      (void) transdump((char *) 0, (char *) 0);
  }
#endif
  compare(c->ts_state, ==, IDLE);
  destroy(c);
}

/* Limit the maximum locate time & service time. 0 is infinite.
 * Since internally we still use deci-seconds we have to convert between
 * deci and milliseconds because the interface is milliseconds.
 */
interval timeout(maxloc)
interval maxloc;
{
  uint16 oldloc = curthread->ts_maxloc * 100;

  curthread->ts_maxloc = (uint16) (maxloc + 99) / 100;
  return oldloc;
}

#ifndef NOPROC
/*
 * Needed to save and restore kernel state in process descriptors
 */
long tgtimeout(tp)
struct thread *tp;
{
    return (long) tp->ts_maxloc;  /*AMOEBA4*/
}

void tstimeout(tp, tout)
struct thread *	tp;
long		tout;
{
    tp->ts_maxloc = (uint16) tout;
}
#endif

#ifndef SMALL_KERNEL
int
transdump(begin, end)
char *	begin;
char *	end;
{
    char *	bprintf();

    char *	p;
    register struct thread *t;
    static char *states[] = {
	"IDLE", "SENDING", "DONE", "ACKED", "NACKED", "FAILED",
	"WAITBUF", "RECEIVING", "ABORT", "MEMFAULT"
    };
    static struct ftab {
	uint16 flag;
	char *name;
    } ftab[] = {
	{ GETREQ,	"GETREQ"  }, { SERVING,	 "SERVING" },
	{ PUTREP,	"PUTREP"  }, { LOCATING, "LOCATE"  },
	{ PUTREQ,	"PUTREQ"  }, { GETREP,	 "GETREP"  },
	{ FASTKILLED,	"FASTKILLED" },
    };
    register struct ftab *fp;

    p = bprintf(begin, end,
	    "\nTK STATE    CTM TIM CNT  CLT  SRV CLI SVI SEQ SIG FLAGS\n");

    for (t = thread; t < upperthread; t++) {
	if (t->ts_state == IDLE && t->ts_flags == 0) {
	    compare(t->ts_cltim, ==, 0);
	    compare(t->ts_timer, ==, 0);
	    compare(t->ts_signal, ==, 0);
	    continue;
	}
	p = bprintf(p, end, "%2d %9s%3d %3d %3d %4x %4x %3d %3d %3d %3u",
		THREADSLOT(t), states[t->ts_state], t->ts_cltim, t->ts_timer,
		t->ts_count, t->ts_client, t->ts_server, t->ts_clident & 0xFF,
		t->ts_svident & 0xFF, t->ts_seq & 0xFF, t->ts_signal & 0xFF);

	for (fp = ftab; fp < &ftab[SIZEOFTABLE(ftab)]; fp++)
	    if (t->ts_flags & fp->flag)
		p = bprintf(p, end, " %s", fp->name);

	if (t->ts_flags & (GETREQ | LOCATING | GETREP))
	    p = bprintf(p, end, "  '%p'", t->ts_flags & GETREQ ?
				&t->ts_rhdr->h_port : &t->ts_xhdr->h_port);
	p = bprintf(p, end, "\n");
    }
    return p - begin;
}
#endif /*SMALL_KERNEL*/

trinit(){

  compare(THREADSLOT(curthread), <, 256);
  curthread->ts_addr = (THREADSLOT(curthread) << 8 | local);
  (void) timeout((interval) 30000);		/* Default 30 sec */
}

/* Get the site address.
 * ( How is that for misleading comments ? )
 */
void
transinit(){
#ifdef NONET
  local = 1;
#else
  extern address interinit();

  local = siteaddr(interinit());
  netenable();
#endif /*NONET*/
  sweeper_set(netsweep, 0L, 100, 0);
}

#ifndef SMALL_KERNEL
/*ARGSUSED*/
int
amdump(beg, end)
char *	beg;
char *	end;
{
#ifdef STATISTICS
    char *	bprintf();
    register char *	p;
    register i;

    p = bprintf(beg, end, "\nAmoeba statistics:\n");
    p = bprintf(p, end, "clfail  %7ld ",  rpcstat.rpcs_clfail);
    p = bprintf(p, end, "svfail  %7ld ",  rpcstat.rpcs_svfail);
    p = bprintf(p, end, "clcrash %7ld ",  rpcstat.rpcs_clcrash);
    p = bprintf(p, end, "rxcl    %7ld ",  rpcstat.rpcs_rxcl);
    p = bprintf(p, end, "rxsv    %7ld\n", rpcstat.rpcs_rxsv);
    p = bprintf(p, end, "trans   %7ld ",  rpcstat.rpcs_trans);
    p = bprintf(p, end, "local   %7ld ",  rpcstat.rpcs_loctrans);
    p = bprintf(p, end, "remote  %7ld ",  rpcstat.rpcs_remtrans);
    p = bprintf(p, end, "getreq  %7ld ",  rpcstat.rpcs_getreq);
    p = bprintf(p, end, "putrep  %7ld\n", rpcstat.rpcs_putrep);
    p = bprintf(p, end, "naks    %7ld ",  rpcstat.rpcs_naks);
    p = bprintf(p, end, "nloc    %7ld\n", rpcstat.rpcs_nloc);

    p = bprintf(p, end, "\nbin#\t    grq     prp     prq     grp\n");
    for (i = 0; i < SIZEBINSIZE; i++)
	p = bprintf(p, end, "%3d\t%7ld %7ld %7ld %7ld\n", i,
			rpcstat.rpcs_grqbin[i],
			rpcstat.rpcs_prpbin[i],
			rpcstat.rpcs_prqbin[i],
			rpcstat.rpcs_grpbin[i]);

    return p - beg;
#else
    return 0;
#endif
}
#endif
