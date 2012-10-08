/*	@(#)switch.c	1.8	96/02/27 14:05:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* This module is the actual packet switch.  It has one entry point,
 * pktswitch.  This module tries to figure out where the packet has
 * to go.  It uses the Routing Table module to maintain a hint cache
 * of where Destination Addresses are located physically.  It also
 * uses the cache module to handle loops in the network topology.
 * Packet management is done through the packet module.
 */
#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "sys/flip/switch.h"
#include "routtab.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"

#define MON_EVENT(x)   DPRINTF(1, ("%s\n", x))

extern f_hopcnt_t max_hops;
static location nullloc;

extern struct netswitch *flip_netswitch;	/* network info */
extern struct netswitch **flip_indexswitch;
extern uint16 flip_nnetwork;
extern route_p route_cache;	/* cache for most recent used routes. */


/* Handle a Locate packet.
 */
static void f_locate(pkt, ntw, loc)
    pkt_p pkt;
    location *loc;
{
    flip_hdr *fh;
    uint16 count, trusted_count;
    f_hopcnt_t hopcnt;
    int remove;
    
    proto_cur_header(fh, pkt, flip_hdr);
    /* If the Destination Address is null, this is an error
     * in the packet;  Just discard it.
     */
    if (ADR_NULL(&fh->fh_srcaddr)) {
	MON_EVENT("Null Source in Locate packet");
	pkt_discard(pkt);
	return;
    }
    if (ADR_NULL(&fh->fh_dstaddr)) {
	MON_EVENT("Null Destination in Locate packet");
	pkt_discard(pkt);
	return;
    }
    /* If there is a loop in the internetwork, we can get
     * locate messages twice or more.  Get rid of all but
     * the first.
     */
    if (flip_toomuch(ntw) || flcache_present(fh)) {
	DPRINTF(2, ("Loop in Locate or too many broadcasts\n"));
	STINC(flip_indexswitch[ntw], ns_dropped);
	pkt_discard(pkt);
	return;
    }
    
    /* Install <Source Address, Network, Location> in the Routing Table, so
     * that here-is packets can find their way back.
     */

    DPRINTF(2, ("f_locate: "));
    adr_remove(&fh->fh_srcaddr, NONTW, &nullloc, 0, NONTW);
    adr_install(&fh->fh_srcaddr, ntw, loc, fh->fh_act_hop, REMOTE,
		!(fh->fh_flags & FL_F_UNSAFE));

    hopcnt = fh->fh_max_hop;
    count = adr_count(&fh->fh_dstaddr, &hopcnt, ntw, UNSECURE);
    hopcnt = fh->fh_max_hop;
    trusted_count = adr_count(&fh->fh_dstaddr, &hopcnt, ntw, SECURE);
    compare(count, >=, trusted_count);

    /* If the Actual Hopcnt == Maximum Hopcnt, and the address is in
     * our Routing Table, return a HereIs packet.  Adjust Max Hopcnt
     * to state the actual hop count.
     */
    
    hopcnt = fh->fh_max_hop;
    if (fh->fh_act_hop == fh->fh_max_hop &&
	    (count = adr_count(&fh->fh_dstaddr, &hopcnt, ntw,
				 (int)(fh->fh_flags & FL_F_SECURITY))) != 0) { 
	MON_EVENT("Successful Locate");
	fh->fh_max_hop = hopcnt;
	fh->fh_type = FL_T_HEREIS;
	fh->fh_length += FLIP_HEREIS_LENGTH;
	fh->fh_total += FLIP_HEREIS_LENGTH;
	fh->fh_max_hop += fh->fh_act_hop;
	proto_dir_append(pkt, (char *) &count, sizeof(count));
	proto_dir_append(pkt, (char *) &trusted_count, sizeof(trusted_count));
	
	STINC(flip_indexswitch[ntw], ns_shereis);
	pkt_reverse(pkt, ntw, loc);
    } else {
	/* We couldn't return a HereIs packet.  If the packet is broadcast
	 * ``through'' our switch (Actual Hopcnt < Max Hopcnt), remove the
	 * address.  In any case, broadcast the packet onto the other
	 * networks.
	 */
	if (fh->fh_act_hop < fh->fh_max_hop) {
	    /* TODO: in case fh_dstaddr is a multicast address, the next
	     * statement currently also has the side effect of unregistering
	     * the hardware multicast address, if we were already listening
	     * to it.  We work around this by reenabling it at the end of
	     * the locate phase, but that is really a hack.
	     */
	    DPRINTF(2, ("f_locate[2]: "));
	    adr_remove(&fh->fh_dstaddr, NONTW, &nullloc, 0, ntw);
	}
	remove = pkt_broadcast(pkt, ntw);
	assert(remove == 0);
    }
}


/* Handle a HereIs packet.
 */
static void f_hereis(pkt, ntw, loc)
    pkt_p pkt;
    location *loc;
{
    int src_ntw;
    location src_loc;
    flip_hdr *fh;
    uint16 incr, trusted_incr;
    uint16 count, trusted_count;
    
    proto_cur_header(fh, pkt, flip_hdr);
    
    /* If the Destination Address is null, this is an error
     * in the packet;  Just discard it.
     */
    if (ADR_NULL(&fh->fh_srcaddr)) {
	MON_EVENT("Null Source in Hereis packet");
	pkt_discard(pkt);
	return;
    }
    if (ADR_NULL(&fh->fh_dstaddr)) {
	MON_EVENT("Null address in HereIs packet");
	pkt_discard(pkt);
	return;
    }
    if (fh->fh_length != FLIP_HEREIS_LENGTH) {
	MON_EVENT("No Count in Hereis packet");
	pkt_discard(pkt);
	return;
    }
    
    /* Install <Destination Address, Network, Location> in the Routing Table, 
     * so that the Source can send packets to the Destination.  Use adr_add 
     * instead of adr_install to count the number of Destinations.
     */
    proto_dir_getoff(pkt, (char *) &trusted_incr, sizeof(trusted_incr));
    proto_dir_getoff(pkt, (char *) &incr, sizeof(incr));
    if(incr < trusted_incr || incr == 0) {
	DPRINTF(0, ("Strange Count in Hereis packet %u\n", incr));
	pkt_discard(pkt);
	return;
    }
    if(fh->fh_flags & FL_F_UNSAFE) {
	trusted_incr = 0;
    }
    adr_add(&fh->fh_dstaddr, ntw, loc, fh->fh_max_hop-fh->fh_act_hop,
	    incr, trusted_incr, 0);
    
    /* If the Source Address is unknown, discard the packet.
     * Apparently the Destination was so slow, that the path set up by
     * the Locate packet is forgotten.  The Source can relocate
     * later, and then we may still know where the Destination is.
     */
    /* The Source Address is in our Routing Table.  Pick a Source that
     * is not on the same network as where this packet came from.  If
     * this is not possible, just discard the packet.
     */
    if(adr_route(&fh->fh_srcaddr, fh->fh_act_hop, &src_ntw, &src_loc, 
		  (f_hopcnt_t *) 0, ntw,
		  (int)(fh->fh_flags & FL_F_SECURITY)) != ADR_OK) {
	MON_EVENT("Could not route srcaddr");
	DPRINTF(0, ("ntw: %d hcnt %d\n", ntw, fh->fh_act_hop));
	pkt_discard(pkt);
    } else {
	/* Send the packet on to the network on which (we think) the
	 * Source (of the locate packet) is located.
	 */
	MON_EVENT("Forward HereIs packet");
	count = adr_count(&fh->fh_dstaddr, (f_hopcnt_t *) 0, src_ntw,
			  UNSECURE);
	trusted_count = adr_count(&fh->fh_dstaddr, (f_hopcnt_t *) 0, src_ntw,
				  SECURE);
	compare(count, >=, incr);
	compare(trusted_count, >=, trusted_incr);
	compare(trusted_count, <=, count);
	compare(count, >, 0);
	proto_dir_append(pkt, (char *) &count, sizeof(count));
	proto_dir_append(pkt, (char *) &trusted_count, sizeof(trusted_count));
	STINC(flip_indexswitch[src_ntw], ns_shereis);
	pkt_reverse(pkt, src_ntw, &src_loc);
    }
}


static void f_unidata(pkt, ntw, loc)
    pkt_p pkt;
    location *loc;
{
    register struct netswitch *ns;
    register route_p r = route_cache + ntw;
    register struct flip_hdr *fh;
    int dst_ntw;
    location dst_loc;
    int f;

    proto_cur_header(fh, pkt, flip_hdr);

    /* Install <Source Address, Network, Location> in the Routing Table, so
     * that the Destination can send packets back to the source.  A Null Source
     * address is allowed, but not installed.
     */
    if(r->r_sadr != 0 && ADR_EQUAL(&(r->r_sadr->ai_address),  &fh->fh_srcaddr) 
       && LOC_EQUAL(loc, &r->r_sloc->li_location)) { 
	if(fh->fh_act_hop < r->r_sloc->li_hopcnt)
	    r->r_sloc->li_hopcnt = fh->fh_act_hop;
	r->r_sloc->li_idle = 0;
    } else adr_install(&fh->fh_srcaddr, ntw, loc, fh->fh_act_hop, REMOTE,
		       !(fh->fh_flags & FL_F_UNSAFE));
    
    if(r->r_dadr != 0 && ADR_EQUAL(&(r->r_dadr->ai_address), &fh->fh_dstaddr) 
       && r->r_dhcnt == fh->fh_max_hop - fh->fh_act_hop && 
       ((fh->fh_flags & FL_F_SECURITY) ? r->r_dloc->li_trust_count > 0 : 1)) {
	/* packet is destined for the same destination as last time */
	assert(r->r_dntw->ni_network < (int) flip_nnetwork);
	ns = flip_indexswitch[r->r_dntw->ni_network];
	fh->fh_act_hop += ns->ns_weight;
        if(!ns->ns_trusted) fh->fh_flags |= FL_F_UNSAFE;
	assert(fh->fh_act_hop <= fh->fh_max_hop);
	STINC(ns, ns_sunidata);
	(*ns->ns_send)(ns->ns_devno, pkt, &(r->r_dloc->li_location));
	return;
    }
    /* Look in the routing table for the destination address. */
    f = adr_route(&fh->fh_dstaddr, fh->fh_max_hop-fh->fh_act_hop,
		  &dst_ntw, &dst_loc, (f_hopcnt_t *) 0, ntw,
		  (int)(fh->fh_flags & FL_F_SECURITY));
    switch(f) {
    case ADR_OK:
	/* Finally we think we have everything straight.  Let's send it. */
	assert(dst_ntw < (int) flip_nnetwork);
	ns = flip_indexswitch[dst_ntw];
        if(!ns->ns_trusted) fh->fh_flags |= FL_F_UNSAFE;
	fh->fh_act_hop += ns->ns_weight;
	assert(fh->fh_act_hop <= fh->fh_max_hop);
	STINC(ns, ns_sunidata);
	(*ns->ns_send)(ns->ns_devno, pkt, &dst_loc);
	break;
    case ADR_TOOFARAWAY:
	/* A packet arrived here of which the Destination Address is unknown.
         * Send a NotHere packet back to the source, so that it can relocate the
	 * Destination.
	 */
	MON_EVENT("Unknown Destination in UniData packet"); 
	if (ADR_NULL(&fh->fh_srcaddr))
	    pkt_discard(pkt);
	else {
	    fh->fh_type = FL_T_NOTHERE;
	    STINC(flip_indexswitch[ntw], ns_snothere);
	    pkt_reverse(pkt, ntw, loc);
	}
	break;
    case ADR_UNSAFE:
	MON_EVENT("Unsafe address in unidata packet");
	fh->fh_type = FL_T_UNTRUSTED;
	fh->fh_flags |= FL_F_UNREACHABLE;
	STINC(flip_indexswitch[ntw], ns_suntrusted);
	pkt_reverse(pkt, ntw, loc);
	break;
    }
}


static void f_multidata(pkt, ntw, loc)
    pkt_p pkt;
    location *loc;
{
    register flip_hdr *fh;
    register route_p r = route_cache + ntw;
    struct ntw_info *ni, *tni;
    int last, remove = 1;
    f_hopcnt_t maxweight;

    proto_cur_header(fh, pkt, flip_hdr);

    if (ADR_NULL(&fh->fh_dstaddr) && flcache_present(fh)) {  
	/* We used to call flip_toomuch(ntw) here as well, but since flow
	 * control for the sender has not been implemented yet, this could 
	 * cause many unnecessary retransmissions in the group protocol.
	 */
	DPRINTF(2,("Loop in broadcast or too many\n"));
	STINC(flip_indexswitch[ntw], ns_dropped);
	pkt_discard(pkt);
	return;
    }
    
    /* Install <Source Address, Network, Location> in the Routing Table, so 
     * that the Destination can send packets back to the source.
     */
    
    /* Only install src address for first fragment. */
    if (fh->fh_offset == 0) {
	if(ADR_NULL(&fh->fh_dstaddr))  {
	    DPRINTF(2, ("f_multidata: "));
	    adr_remove(&fh->fh_srcaddr, NONTW, &nullloc, 0, NONTW);
	}
	if(r->r_sadr != 0 && ADR_EQUAL(&(r->r_sadr->ai_address),
				       &fh->fh_srcaddr) &&
	   LOC_EQUAL(&(r->r_sloc->li_location), loc)) {  
	    if(fh->fh_act_hop < r->r_sloc->li_hopcnt)
		r->r_sloc->li_hopcnt = fh->fh_act_hop;
	    r->r_sloc->li_idle = 0;
	} else adr_install(&fh->fh_srcaddr, ntw, loc, fh->fh_act_hop, REMOTE,
			   !(fh->fh_flags & FL_F_UNSAFE));
    }
    
    if ((ni = adr_lookup(&fh->fh_dstaddr, (uint16 *) 0, (f_hopcnt_t *) 0,
			 (int)(fh->fh_flags & FL_F_SECURITY))) == 0) {
	/* A multicast packet arrived here of which the Destination
	 * Address is unknown.  Throw away.
	 */
	MON_EVENT("Unknown Destination in MultiData packet");
	pkt_discard(pkt);
    } else {
	/* The Destination Address is known.  Forward the packet.
	 */
	if (ni == NI_BROADCAST) {
	    remove = pkt_broadcast(pkt, ntw);
#ifdef NOMULTICAST
	} else
	    remove = 1;
#else /* NOMULTICAST */
	} else {
	    maxweight = fh->fh_max_hop - fh->fh_act_hop;
	    /* tricky code for two reasons. First, avoid making copies of 
	     * messages. Second, the routing table might change.
	     */
	    do {
		last = ni->ni_next == 0 || (ni->ni_next->ni_network == ntw && 
					    ni->ni_next->ni_next == 0);
		tni = ni->ni_next;
		if (ni->ni_network != ntw) {
		    remove = ni_multicast(ni, pkt, maxweight, last,
					  (location *) 0);
		}
		ni = tni;
	    } while (ni != 0);
	}
#endif /* NOMULTICAST */
	if(remove) pkt_discard(pkt);
   }
}


/* Handle a NotHere packet. */
static void f_nothere(pkt, ntw, loc)
    pkt_p pkt;
    location *loc;
{
    int nxt_ntw;
    location nxt_loc;
    flip_hdr *fh;
    
    proto_cur_header(fh, pkt, flip_hdr);
    
    /* If the Source or Destination Address is null, this is an error
     * in the packet;  Just discard it.
     */
    if (ADR_NULL(&fh->fh_srcaddr)) {
	MON_EVENT("Null Source in NotHere packet");
	pkt_discard(pkt);
	return;
    }
    if (ADR_NULL(&fh->fh_dstaddr)) {
	MON_EVENT("Null Destination in NotHere packet");
	pkt_discard(pkt);
	return;
    }
    
    /* A packet that was forwarded by us didn't find it's
     * way through.  Apparently the Routing Table is not
     * completely up-to-date anymore.  Remove the entry.
     */
    DPRINTF(2, ("f_nothere: "));
    adr_remove(&fh->fh_dstaddr, ntw, loc, 0, NONTW);
    
    /* Now try to find the Destination Address again.  If this fails,
     * there is no alternative destination, and we have to forward
     * the NotHere message.
     */
    if (adr_lookup(&fh->fh_dstaddr, (uint16 *) 0, (f_hopcnt_t *) 0,
			(int)(fh->fh_flags & FL_F_SECURITY)) == 0) {
	/* If we can't find the Source anymore, just discard
	 * the packet.
	 */
	/* The Source Address is in our Routing Table.  Pick a
	 * Source that is not on the same network as where this
	 * packet came from.  If this is not possible, discard the
	 * packet.
	 */
	if(adr_route(&fh->fh_srcaddr, fh->fh_act_hop, &nxt_ntw,
		      &nxt_loc, (f_hopcnt_t *) 0, ntw,
		      (int)(fh->fh_flags & FL_F_SECURITY)) != ADR_OK) { 
	    MON_EVENT("Bad Source in NotHere packet or unkown");
	    pkt_discard(pkt);
	}
        /* Everythings OK.  Pass it on.
	 */
	else {
	    MON_EVENT("Forward NotHere packet");
	    STINC(flip_indexswitch[nxt_ntw], ns_snothere);
	    pkt_reverse(pkt, nxt_ntw, &nxt_loc);
	}
    }
    /* We have an alternative for the Destination.  However, if this
     * is on the same network as where we got this packet from, we
     * should not try to act as an intermediary.  In that case, just
     * get rid of the packet;
     */
    else if (adr_route(&fh->fh_dstaddr, fh->fh_max_hop - fh->fh_act_hop,
			&nxt_ntw, &nxt_loc, (f_hopcnt_t *) 0, ntw,
			(int)(fh->fh_flags & FL_F_SECURITY)) != ADR_OK) {
	MON_EVENT("Bad alternative Destination for NotHere packet");
	pkt_discard(pkt);
    }
    
    /* Now we have an OK alternative.  Transform it back into a Data
     * packet, and send it again.
     */
    else {
	MON_EVENT("Try new Destination for NotHere packet");
	fh->fh_type = FL_T_UNIDATA;
	STINC(flip_indexswitch[nxt_ntw], ns_sunidata);
	pkt_send(pkt, nxt_ntw, &nxt_loc);
    }
}


/* Handle an untrusted packet.
 */
static void f_untrusted(pkt, ntw, loc)
    pkt_p pkt;
    int ntw;
    location *loc;
{
    int nxt_ntw;
    location nxt_loc;
    flip_hdr *fh;
    
    proto_cur_header(fh, pkt, flip_hdr);

    /* If an address is null, this is an error in the packet;  Just discard it.
     */
    if (ADR_NULL(&fh->fh_srcaddr)) {
	MON_EVENT("Null Source in UnTrusted packet");
	pkt_discard(pkt);
	return;
    }
    if (ADR_NULL(&fh->fh_dstaddr)) {
	MON_EVENT("Null address in Untrusted packet");
	pkt_discard(pkt);
	return;
    }
    DPRINTF(2, ("f_untrusted: "));
    adr_remove(&fh->fh_dstaddr, ntw, loc, 0, NONTW);
    adr_install(&fh->fh_dstaddr, ntw, loc, fh->fh_act_hop, REMOTE, UNSECURE);
    if (adr_route(&fh->fh_dstaddr, fh->fh_max_hop - fh->fh_act_hop,
		  &nxt_ntw, &nxt_loc, (f_hopcnt_t *) 0, ntw, SECURE) == ADR_OK)
    { 
	/* There is another safe route to the destination. */
	MON_EVENT("Try new Destination for UnTrusted packet");
	fh->fh_type = FL_T_UNIDATA;
	STINC(flip_indexswitch[nxt_ntw], ns_sunidata);
	pkt_send(pkt, nxt_ntw, &nxt_loc);
    } else if(adr_route(&fh->fh_srcaddr, fh->fh_act_hop, &nxt_ntw,
			&nxt_loc, (f_hopcnt_t *) 0, ntw, SECURE) == ADR_OK) {
	/* Route back to the source is still safe. */
	MON_EVENT("Forward UnTrusted packet");
	STINC(flip_indexswitch[nxt_ntw], ns_snothere);
	pkt_reverse(pkt, nxt_ntw, &nxt_loc);
	
    } else {
	MON_EVENT("Bad Source in UnTrusted packet or unkown");
	pkt_discard(pkt);
    }
}


/* A packet has arrived.  Have it handled by the appropriate routine.
 */
void pktswitch(pkt, ntw, loc)
    register pkt_p pkt;
    int ntw;
    location *loc;		/* The location on that network */
{
    register flip_hdr *fh;
    uint16 rand();

#ifdef FLNETCONTROL
    if (flip_netswitch[ntw].ns_loss != 0 &&
	rand() % 100 < flip_netswitch[ntw].ns_loss) {
	pkt_discard(pkt);
	return;
    }
    if (!flip_netswitch[ntw].ns_on) {
	pkt_discard(pkt);
	return;
    }
#endif /* FLNETCONTROL */
    assert(flip_netswitch[ntw].ns_on);

    proto_cur_header(fh, pkt, flip_hdr);

    if(!flip_netswitch[ntw].ns_trusted) fh->fh_flags |= FL_F_UNSAFE;

    /* Throw packet away if the act hop count exceeds the max hop. If act hop
     * exceeds max_hops  and the packet is of type locate, do *not* discard
     * the packet; it is used to invalidate routes.
     */
    if ((fh->fh_act_hop > fh->fh_max_hop) || (fh->fh_type != FL_T_LOCATE &&
					      fh->fh_max_hop > max_hops)) {
	DPRINTF(0, ("Packet with too big hop counts ntw %d\n", ntw));
	pkt_discard(pkt);
	return;
    }
    
    /* Handle the different packet types.  Packets of unknown types
     * are discarded.
     */
    switch (fh->fh_type) {
    case FL_T_LOCATE:	
	STINC(flip_indexswitch[ntw], ns_rlocate);
	f_locate(pkt, ntw, loc);
	break;
    case FL_T_HEREIS:	
	STINC(flip_indexswitch[ntw], ns_rhereis);
	f_hereis(pkt, ntw, loc);
	break;
    case FL_T_UNIDATA:
	STINC(flip_indexswitch[ntw], ns_runidata);
	f_unidata(pkt, ntw, loc);
	break;
    case FL_T_MULTIDATA:	
	STINC(flip_indexswitch[ntw], ns_rmultidata);
	f_multidata(pkt, ntw, loc);
	break;
    case FL_T_NOTHERE:	
	STINC(flip_indexswitch[ntw], ns_rnothere);
	f_nothere(pkt, ntw, loc);	
	break;
    case FL_T_UNTRUSTED:
	STINC(flip_indexswitch[ntw], ns_runtrusted);
	f_untrusted(pkt, ntw, loc);
	break;
    default:
	MON_EVENT("Unknown packet type"); 
	pkt_discard(pkt);
    }
}
