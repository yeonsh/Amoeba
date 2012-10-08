/*	@(#)flip.c	1.9	96/02/27 14:04:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* This file contains functions to send a msg. It also contains the
 * network switch and the function to register a network.
 */

#include "amoeba.h"
#include "stderr.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "sys/flip/switch.h"
#include "routtab.h"
#include "assert.h"
INIT_ASSERT
#include "debug.h"
#include "string.h"
#include "machdep.h"
#include "sys/proto.h"
#include "protocols/rawflip.h"

extern uint16 flip_maxntw;
extern uint16 flip_npkt;
extern uint16 flip_pktsize;

struct netswitch *flip_netswitch;
struct netswitch **flip_indexswitch;
uint16 flip_nnetwork;

pkt_send(pkt, ntw, loc)
    pkt_p pkt;
    location *loc;
{
    struct netswitch *ns;
    flip_hdr *fh;
    
    assert(ntw >= 0 && ntw < (int) flip_nnetwork);
    ns = &flip_netswitch[ntw];
    proto_cur_header(fh, pkt, flip_hdr);
    fh->fh_act_hop += ns->ns_weight;
    if(!ns->ns_trusted) fh->fh_flags |= FL_F_UNSAFE;
    if(!ns->ns_trusted && fh->fh_flags & FL_F_SECURITY) {
	pkt_discard(pkt);
    } else {
        assert(fh->fh_act_hop <= fh->fh_max_hop);
        assert(ns->ns_send);
        (*ns->ns_send)(ns->ns_devno, pkt, loc);
    }
}


/* Send a packet back to network ntw to location loc.
 */
pkt_reverse(pkt, ntw, loc)
    pkt_p pkt;
    location *loc;
{
    struct netswitch *ns;
    flip_hdr *fh;
    
    assert(ntw >= 0 && ntw < (int) flip_nnetwork);
    ns = &flip_netswitch[ntw];
    proto_cur_header(fh, pkt, flip_hdr);
    fh->fh_act_hop -= ns->ns_weight;
    if(!ns->ns_trusted) fh->fh_flags |= FL_F_UNSAFE;
    if(!ns->ns_trusted && fh->fh_flags & FL_F_SECURITY) {
	pkt_discard(pkt);
    } else {
	assert(ns->ns_send);
	(*ns->ns_send)(ns->ns_devno, pkt, loc);
    }
}


/* Broadcast a packet over all networks except ntw.
 */
int pkt_broadcast(pkt, ntw)
    pkt_p pkt;
    int ntw;
{
    pkt_p new;
    struct netswitch *ns, *not;
    flip_hdr *fh, *fn;
    f_hopcnt_t maxweight;
    int locate;
    int security;
    
    assert(ntw >= 0 && ntw < (int) flip_nnetwork);
    not = &flip_netswitch[ntw];
    proto_cur_header(fh, pkt, flip_hdr);
    assert(fh);
    locate = fh->fh_type == FL_T_LOCATE;
    security = fh->fh_flags & FL_F_SECURITY;
    assert(fh->fh_act_hop <= fh->fh_max_hop);
    maxweight = fh->fh_max_hop - fh->fh_act_hop;
    proto_fix_header(pkt);
    for (ns = flip_netswitch; ns < flip_netswitch + flip_nnetwork; ns++) {
#ifdef FLNETCONTROL
	if(!ns->ns_on) continue;
#endif /* FLNETCONTROL */
	assert(ns->ns_on);
	if(security && !ns->ns_trusted) continue; 
	if(ns->ns_weight <= maxweight) {
	    if (ns != not) {
		if((new = flip_pkt_new(pkt)) != 0) {
		    fn = proto_look_header(new, flip_hdr);
		    assert(fn);
		    fn->fh_act_hop += ns->ns_weight;
	            if(!ns->ns_trusted) fn->fh_flags |= FL_F_UNSAFE;
		    if(locate) STINC(ns, ns_slocate);
		    else STINC(ns, ns_smultidata);
		    assert(ns->ns_bcast);
		    (*ns->ns_bcast)(ns->ns_devno, new);
		} else {
		    DPRINTF(0, ("WARNING: pkt_broadcast: out of packets\n"));
		    break;
		}
	    }
	}
    }
    pkt_discard(pkt);
    return(0);
}

#ifndef NOMULTICAST
flip_delmcast(adr, ni, nontw)
    adr_p adr;
    struct ntw_info *ni;
    int nontw;
{
    struct netswitch *ns;
    int ntw;

    assert(ni->ni_network >= 0 && ni->ni_network < (int) flip_nnetwork);
    for(ns = flip_netswitch, ntw = 0; ns < flip_netswitch + flip_nnetwork;
	ns++, ntw++) {
	if(ntw != nontw && ns->ns_delmc != 0)  {
	    (*ns->ns_delmc)(ns->ns_devno, &ni->ni_multicast, adr);
	}
    }
}


/* Multicast pkt over all sites in ni, unless the weight is too high and
 * consequently the hop count runs out.
 */
int ni_multicast(ni, pkt, maxweight, last, skip)
    struct ntw_info *ni;
    pkt_p pkt;
    f_hopcnt_t maxweight;
    int last;
    location *skip; /* local interface location to skip? */
{
    struct netswitch *ns;
    struct loc_info *li, *tli;
    pkt_p new;
    flip_hdr *fn, *fh;
    int send = 0;
    adr_t adr;
    
    assert(ni->ni_network >= 0 && ni->ni_network < (int) flip_nnetwork);
    proto_cur_header(fh, pkt, flip_hdr);
    ns = flip_indexswitch[ni->ni_network];

    if(fh->fh_flags & FL_F_SECURITY && !ns->ns_trusted) {
	return(1);
    }
    if(!ns->ns_trusted) fh->fh_flags |= FL_F_UNSAFE;

    if (ns->ns_weight <= maxweight && ni->ni_nmcast > 0) {
	if(last && ni->ni_count == ni->ni_nmcast) {
	    fh->fh_act_hop += ns->ns_weight;
	    assert(ns->ns_send);
	    STINC(ns, ns_smultidata);
	    (*ns->ns_send)(ns->ns_devno, pkt, &ni->ni_multicast);
	    return(0);
	} else {
	    proto_fix_header(pkt);
	    if ((new = flip_pkt_new(pkt)) == 0) {
		DPRINTF(0, ("WARNING: ni_multicast(%d < %d): out of packets\n",
			    ni->ni_count, ni->ni_nmcast));
		PROTO_LOOK_HEADER(fh, pkt, flip_hdr);
	    } else {
		PROTO_LOOK_HEADER(fn, new, flip_hdr);
		PROTO_LOOK_HEADER(fh, pkt, flip_hdr);
		assert(fn);
		fn->fh_act_hop += ns->ns_weight;
		assert(ns->ns_send);
		STINC(ns, ns_smultidata);
		(*ns->ns_send)(ns->ns_devno, new,  &ni->ni_multicast);
	    }
	}
    }
    if (ni->ni_count == ni->ni_nmcast)  {
	return(last ? 0 : 1);
    }
    adr = fh->fh_dstaddr;
    if (ns->ns_weight <= maxweight) {
	for (li = ni->ni_loc; li != 0; li = tli) {
	    tli = li->li_next;	/* route table might change */
	    if(li->li_mcast == LI_MCAST) continue; /* we sent it using mcast */
	    if (skip && LOC_EQUAL(skip, &li->li_location)) {
		DPRINTF(0, ("ni_multicast: skip location\n"));
		continue;
	    }
	    if(last && tli == 0) {
		fh->fh_act_hop += ns->ns_weight;
		send = 1;
		assert(ns->ns_send);
		STINC(ns, ns_smultidata);
		(*ns->ns_send)(ns->ns_devno, pkt, &li->li_location);
	    }
	    else {
		DPRINTF(2, ("ni_multicast: copy packet\n"));
		proto_fix_header(pkt);
		if ((new = flip_pkt_new(pkt)) == 0) {
		    DPRINTF(0, ("WARNING: ni_multicast: out of packets\n"));
		    PROTO_LOOK_HEADER(fh, pkt, flip_hdr);
		} else {
		    PROTO_LOOK_HEADER(fn, new, flip_hdr);
		    PROTO_LOOK_HEADER(fh, pkt, flip_hdr);
		    assert(fn);
		    fn->fh_act_hop += ns->ns_weight;
		    assert(ns->ns_send);
		    STINC(ns, ns_smultidata);
		    (*ns->ns_send)(ns->ns_devno, new, &li->li_location);
		}
	    }
	}
    } else {
	DPRINTF(0, ("ni_multicast: pkt too heavy\n"));
    }
    if (ns->ns_setmc && ni->ni_count != ni->ni_nmcast &&
	ni->ni_count >= ns->ns_nlocation)
    {
	/* There is a location who could possible listen to 
	 ** a multicast address. Let's try it.
	 */
	DPRINTF(0, ("ni_multicast(%d != %d): set up multicast address\n",
		    ni->ni_count, ni->ni_nmcast));
	(*ns->ns_setmc)(ns->ns_devno, ni, &adr);
    }
    if(send) return(0);
    else return(1);
}
#endif /* NOMULTICAST */


int flip_toomuch(ntw)
    int ntw;
{
    struct netswitch *ns;

    assert(ntw >= 0 && ntw < (int) flip_nnetwork);
    ns = flip_indexswitch[ntw];
    ns->ns_bcastpersweep++;
    if(ns->ns_bcastpersweep >= MAXBCAST) {
	STINC(ns, ns_bcastdrop);
    }
    return(ns->ns_bcastpersweep >= MAXBCAST);
}


/*ARGSUSED*/
static void net_sweep(id)
    long id;
{
    struct netswitch *ns;

    for (ns = flip_netswitch; ns < flip_netswitch + flip_nnetwork; ns++) {
	ns->ns_bcastpersweep = 0;
    }
}


int flip_newnetwork(name, devno, weight, loss, send, bcast, setmc, delmc, nloc)
    char *name;
    int devno;
    uint16 weight;
    int16 loss;
    void (*send)(), (*bcast)(), (*setmc)(), (*delmc)();
    uint16 nloc;
{
    struct netswitch *ns;
    
    DPRINTF(0, ("flip_network: alloc %s network %d(%d, %d, %d)\n", name,
		flip_nnetwork, devno, weight, loss));
    assert (flip_nnetwork != flip_maxntw);
    ns = flip_netswitch + flip_nnetwork;
    flip_indexswitch[flip_nnetwork] = ns;
    compare(strlen(name), <, MAXNAME);
    (void) memmove((_VOIDSTAR) ns->ns_name, (_VOIDSTAR) name, strlen(name));
    ns->ns_devno = devno;
    ns->ns_weight = weight;
    ns->ns_trusted = 1;
    ns->ns_loss = loss;
    ns->ns_send = send;
    ns->ns_bcast = bcast;
    ns->ns_setmc = setmc;
    ns->ns_delmc = delmc;
    ns->ns_nlocation = nloc;
    ns->ns_on = 1;
    ns->ns_bcastpersweep = 0;
    return flip_nnetwork++;
}


#ifdef FLNETCONTROL
int flip_control(ntw, delay, loss, on, trusted)
    short ntw;
    short *delay;
    short *loss;
    short *on;
    short *trusted;
{
    if (ntw < 0 || ntw >= flip_nnetwork) {
	DPRINTF(0, ("illegal ntw number\n"));
	return(0);
    }
    if (delay && *delay >= 0)
    {
	printf("Setting delay of network #%d to %d\n", ntw, *delay);
	flip_netswitch[ntw].ns_delay= *delay;
    }
    
    if (loss && *loss >= 0)
    {
	printf("Setting loss of network #%d to %d\n", ntw, *loss);
	flip_netswitch[ntw].ns_loss= *loss;
    }
    
    if (on && *on >= 0)
    {
	printf("Switching network #%d to %d\n", ntw, *on);
	flip_netswitch[ntw].ns_on= *on;
	adr_purge();
    }
    
    if(trusted && *trusted >= 0)
    {
	printf("Switching trusted of network #%d to %d\n", ntw, *trusted);
	flip_netswitch[ntw].ns_trusted = *trusted;
	adr_purge();
    }
    
    if (delay) *delay= flip_netswitch[ntw].ns_delay;
    if (loss) *loss= flip_netswitch[ntw].ns_loss;
    if (on) *on= flip_netswitch[ntw].ns_on;
    if (trusted) *trusted= flip_netswitch[ntw].ns_trusted;
    return(1);
}
#endif

pool_t flip_pool;
static pkt_p flip_bufs;
static char *flip_bufdata;

/* Return a packet from FLIP's pool.
 */
pkt_p flip_pkt_acquire(){
    pkt_p pkt;
    
    PKT_GET(pkt, &flip_pool);
    return(pkt);
}

/* Return a duplicate of pkt by acquiring a new one and copying the
 * contents.
 */
pkt_p flip_pkt_new(pkt)
    pkt_p pkt;
{
    pkt_p new;
    
    if ((new = flip_pkt_acquire()) != 0) {
	pkt_copy(pkt, new);
    }
    return new;
}

void
netswitch_init() 
{
    uint16 i;

    DPRINTF(0, ("netswitch_init: aalloc %d networks\n", flip_maxntw));
    flip_netswitch = (struct netswitch *)
	       aalloc((vir_bytes)(sizeof(struct netswitch) * flip_maxntw), 0);
    flip_indexswitch = (struct netswitch **)
	       aalloc((vir_bytes)(sizeof(struct netswitch *) * flip_maxntw), 0);
    flip_nnetwork = 0;
    for(i=0; i < flip_maxntw; i++) {
	flip_indexswitch[i] = 0;
	flip_netswitch[i].ns_on = 0;
	flip_netswitch[i].ns_bcastpersweep = 0;
    }
    ff_init(flip_maxntw);
    flip_bufs = (pkt_p) aalloc((vir_bytes)(sizeof(pkt_t) * flip_npkt), 0);
    flip_bufdata = aalloc((vir_bytes)(flip_npkt*flip_pktsize), 0);
    pkt_init(&flip_pool, (int) flip_pktsize, flip_bufs, (int) flip_npkt,
					    flip_bufdata, (void (*)()) 0, 0L);
    compare(sizeof(flip_hdr), ==, FLIP_HDR_SIZE);
    compare(sizeof(adr_t), ==, FLIP_ADR_SIZE);
    compare(sizeof(location), ==, FLIP_LOC_SIZE);
    compare(2*sizeof(uint16), ==, FLIP_HEREIS_LENGTH);
    sweeper_set(net_sweep, BCASTSWEEP, BCASTSWEEP, 0);
}

/*
 * Dynamic interface to rawflip code
 *
 * This should be more general, with loadable system calls
 */

static int (*func_rawflip)();

register_rawflip(func)
int (*func)();
{

	func_rawflip = func;
}

do_rawflip(arg)
struct rawflip_parms *arg;
{

	if (func_rawflip != 0)
		return (*func_rawflip)(arg);
	return STD_NOTFOUND;
}

#ifndef SMALL_KERNEL
flip_netdump(begin, end)
    char *begin, *end;
{
    char *p;
    struct netswitch *ns;
    
    p = bprintf(begin, end, "======== network dump =======\n");
    for(ns = flip_netswitch; ns < flip_netswitch + flip_nnetwork; ns++) {
	p = bprintf(p, end, "%d: %s: #%d weight %d trusted %d loss-rate %d nloc %d on %d\
 nbcast %d\n",
		    ns - flip_netswitch, ns->ns_name, ns->ns_devno,
		    ns->ns_weight, ns->ns_trusted, ns->ns_loss,
		    ns->ns_nlocation, ns->ns_on, ns->ns_bcastpersweep); 
    }
    p = bprintf(p, end, "=============================\n");
    return(p - begin);
}


#ifdef STATISTICS
flip_netstat(begin, end)
    char *begin, *end;
{
    char *p;
    struct netswitch *ns;
    
    p = bprintf(begin, end, "======== network statistics =======\n");
    for(ns = flip_netswitch; ns < flip_netswitch + flip_nnetwork; ns++) {
	p = bprintf(p, end, "==== %s: #%d ====\n", ns->ns_name, ns->ns_devno);
	p = bprintf(p, end, "slocate    %7ld ",  ns->ns_slocate);
	p = bprintf(p, end, "shereis    %7ld ",  ns->ns_shereis);
	p = bprintf(p, end, "snothere   %7ld ",  ns->ns_snothere);
	p = bprintf(p, end, "sunidata   %7ld\n",  ns->ns_sunidata);
	p = bprintf(p, end, "smultidata %7ld ",  ns->ns_smultidata);
	p = bprintf(p, end, "suntrusted %7ld\n",  ns->ns_suntrusted);
	p = bprintf(p, end, "rlocate    %7ld ",  ns->ns_rlocate);
	p = bprintf(p, end, "rhereis    %7ld ",  ns->ns_rhereis);
	p = bprintf(p, end, "rnothere   %7ld ",  ns->ns_rnothere);
	p = bprintf(p, end, "runidata   %7ld\n",  ns->ns_runidata);
	p = bprintf(p, end, "rmultidata %7ld ",  ns->ns_rmultidata);
	p = bprintf(p, end, "runtrusted %7ld\n",  ns->ns_runtrusted);
	p = bprintf(p, end, "dropped    %7ld ",  ns->ns_dropped);
	p = bprintf(p, end, "bcast drop %7ld\n",  ns->ns_bcastdrop);
    }
    p = bprintf(p, end, "=============================\n");
    return(p - begin);
}
#endif
#endif
