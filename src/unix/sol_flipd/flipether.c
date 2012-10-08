/*	@(#)flipether.c	1.1	96/02/27 11:49:47 */
/* Solaris 2.3 changed names in sys/ioccom.h */
#include <sys/ioccom.h>
#define _IOC_INOUT      IOC_INOUT
#define _IOCPARM_MASK   IOCPARM_MASK
 
/*#define timeout amoeba_timeout*/
#define ffs	string_ffs
#include "amoeba.h"
#include "sys/flip/flip.h"
#include "sys/flip/packet.h"
#include "sys/flip/fragment.h"
#include "sys/debug.h"
#undef ffs
#undef timeout
#include "ux_int.h"
#include "ux_rpc_int.h"
#include "assert.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/kmem.h>
#include <server/process/proc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/dlpi.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/modctl.h>
#include <sys/devops.h>
#include <sys/cmn_err.h>
#include <sys/ethernet.h>
#include <sys/le.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#undef curthread

INIT_ASSERT

#include "byteorder.h"
#include "sys/flip/fl_byteorder.h"

/* To get the prototypes right */
#include "machdep.h"

#include "fldefs.h"

static void flsend(int, pkt_p, location *, unsigned short);


/* Amoeba rpc protocol header. */
typedef struct amphdr {
    adr_t       ah_kid;         /* kernel identifier */
    port        ah_port;        /* port for RPC_REQUEST, etc. */
    uint8       ah_type;        /* see above */
    uint8       ah_flags;       /* see above */
    uint32      ah_tid;         /* transaction identifier */
    uint16      ah_dest;        /* destination state entry */
    uint16      ah_from;        /* source state entry */
} amphdr_t, *amphdr_p;
 



/* This file contains the flip ethernet dependent code. It interacts
 * with a fragmentation module to do fragmentation and flow control.
 * The flow control scheme is implemented using credits. We do
 * not do flow control for flip msgs that fit in one packet or 
 * multidata flip msgs (i don't know how to this in an efficient and
 * clean way). The latter is clearly a BUG.
 */

#define FLIP		0x8146
#define ETH_WEIGHT	3
#define NRELEASE	100

#define ETH_DATA 	(1500L - sizeof(fc_hdr_t) - sizeof(flip_hdr))
#define ETH_EQUAL(a1, a2) ((* (short *) a1) == (* (short *) a2) &&\
    (* (short *) (a1+2)) == (* (short *) (a2 +2)) &&\
    (* (short *) (a1+4)) == (* (short *) (a2+4)))

static volatile int busy;
static unsigned char flipbrdcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

#ifdef STATISTICS
typedef struct {
	int ff_sreq;
	int ff_scredit;
	int ff_spiggy;
	int ff_smcreq;
	int ff_smcack;
	int ff_smcnak;
	int ff_sacredit;
	int ff_rreq;
	int ff_rcredit;
	int ff_rpiggy;
	int ff_rmcreq;
	int ff_rmcack;
	int ff_rmcnak;
	int ff_racredit;
} ffstat_t, *ffstat_p;

static ffstat_t ffstat;

#define STINC(type)	ffstat.type++

int ffstatistics(begin, end)
    char *begin;
    char *end;
{
    char *p;
    
    p = bprintf(begin, end, "============== ff statistics =======\n");
    p = bprintf(p, end, "scredit   :%7ld ", ffstat.ff_scredit);
    p = bprintf(p, end, "spiggy    :%7ld ", ffstat.ff_spiggy);
    p = bprintf(p, end, "srequest  :%7ld ", ffstat.ff_sreq);
    p = bprintf(p, end, "smcreq    :%7ld\n", ffstat.ff_smcreq);
    p = bprintf(p, end, "smcack    :%7ld ", ffstat.ff_smcack);
    p = bprintf(p, end, "smcnak    :%7ld ", ffstat.ff_smcnak);
    p = bprintf(p, end, "sabscredit:%7ld\n", ffstat.ff_sacredit);

    p = bprintf(p, end, "rcredit   :%7ld ", ffstat.ff_rcredit);
    p = bprintf(p, end, "rpiggy    :%7ld ", ffstat.ff_rpiggy);
    p = bprintf(p, end, "rrequest  :%7ld ", ffstat.ff_rreq);
    p = bprintf(p, end, "rmcreq    :%7ld\n", ffstat.ff_rmcreq);
    p = bprintf(p, end, "rmcack    :%7ld ", ffstat.ff_rmcack);
    p = bprintf(p, end, "rmcnak    :%7ld ", ffstat.ff_rmcnak);
    p = bprintf(p, end, "rabscredit:%7ld\n", ffstat.ff_racredit);

    if (p == 0) {
	printf("ffstat fail\n");
	return 0;
    }
    return(p - begin);
}
#else
#define STINC(type)
#endif	/* STATISTICS */

/* When a packet coming from location r_location is released, we might have
 * to send a credits to the location. If we do it immediately when the packet
 * arrives, the packet might arrive to fast to process. So, we send credits
 * when a packet is completely processed. To be able to send credits back,
 * the location and device on which the packet arrived is stored in the
 * structure release when the packet comes in. When the packet is discarded,
 * this information is used to send credits back.
 */

typedef struct release {
    long	r_oldarg;
    void	(*r_oldrelease)();
    pkt_p 	r_pkt;
    location	r_location;
    int		r_dev;
    int		r_credit;
    struct release *r_next;
} release_t, *release_p;

static release_p free_release;	/* free list of release structures */


static void notify(int dev)
{
    location l;
    pkt_p p;
    fc_cnt_t credit;
    int r;
    fc_hdr_p fc;
    int last;
    flip_hdr *fh;

    while((r = ff_get(fleth[dev].fe_ntw, &l, &p, &credit)) > 0) { 
	proto_cur_header(fh, p, flip_hdr);
	last = fh->fh_total == fh->fh_offset + fh->fh_length;
	proto_fix_header(p);
	PROTO_ADD_HEADER(fc, p, fc_hdr_t);
	fc->fc_type = FC_DATA;
	fc->fc_cnt = credit;
	proto_fix_header(p);
	if (credit > 0)
	    STINC(ff_spiggy);
	flsend(dev, p, &l, FLIP);
	if (r == 1)
	    return;
    }
}

/* The fragmentation module tells us that we should ask for credits. */
void fl_req_credit(int dev, location *locp, pkt_p pkt)
{
    STINC(ff_sreq);
    flsend(dev, pkt, locp, FLIP);
}


/* The fragmentation layer thinks that locp is dead. */
/*ARGSUSED*/
void failure(location *locp, pkt_p pkt)
{
    DPRINTF(1, ("failure\n"));
    flux_failure(pkt);
}


/* When the packet is discarded, check if we have to send credits back
 * to the location where the packet came from.
 */
static void send_credit(long arg)
{
    release_p r = (release_p) arg;
    pkt_p p;
    void (*tmp)();
    long oldarg;
    pkt_p pkt;

    if(r->r_credit && ff_rcv_credit(fleth[r->r_dev].fe_ntw, &r->r_location, 0,
				    EV_RELEASE)) {
    	notify(r->r_dev);
    }
    if((p = ff_snd_credit(fleth[r->r_dev].fe_ntw, &r->r_location, 0))) {
	STINC(ff_scredit);
	flsend(r->r_dev, p, &r->r_location, FLIP);
    }
    pkt = r->r_pkt;
    tmp = r->r_oldrelease;
    oldarg = r->r_oldarg;
    r->r_oldrelease = 0;
    r->r_pkt= 0;
    r->r_next = free_release;
    free_release =  r;
    pkt->p_admin.pa_release = tmp;
    pkt->p_admin.pa_arg = oldarg;
    if(tmp != 0) (*tmp)(oldarg);
}


/* Store information in the release structure, so that we can send credits
 * back when the packet is discarded. If there are no release structure,
 * we do nothing; the source will gives us some time to process all the packets
 * that are apparently floating around in the kernel.
 */
static void setreleaseinfo(pkt_p pkt, int dev, location *loc, int credit)
{
    release_p r = free_release;

    if(r != 0) {
	free_release = r->r_next;
	r->r_next = 0;
	r->r_pkt = pkt;
	r->r_oldarg = pkt->p_admin.pa_arg;
	r->r_oldrelease = pkt->p_admin.pa_release;
	pkt->p_admin.pa_release = 0;
	r->r_dev = dev;
	r->r_location = *loc;
	r->r_credit = credit;
	pkt_set_release(pkt, send_credit, (long) r);
    } else {
	DPRINTF(0, ("setreleaseinfo: WARNING out of release structures\n"));
    }
}

static location loc;

/*ARGSUSED*/
static int flarrive(int dev, char *dstaddr, char *srcaddr, register pkt_p pkt)
{
    
    /* Problem with Ethernet which needs a minimum packet size.
     * pc_totsize and pc_dirsize may be too large.  Here we hack
     * in the values that they should have.
     */
    register flip_hdr *fh;
    register fc_hdr_p fc;
    register location *locp;
    pkt_p p;
    fc_cnt_t scredit = 0, rcredit;
    amphdr_p tmp;
    
    locp = &loc;
    bcopy(srcaddr, locp->l_bytes, 6);
    PROTO_LOOK_HEADER(fc, pkt, fc_hdr_t);
    if(fc == 0) {
	printf("flarrive: flip packet without control header received\n");
	return(0);
    }
    switch(fc->fc_type) {
    case FC_REQCREDIT:
	STINC(ff_rreq);
        proto_remove_header(pkt);
        if((p = ff_snd_credit(fleth[dev].fe_ntw, locp, 1))) {
	    STINC(ff_sacredit);
            flsend(dev, p, locp, FLIP);
        }
	return(0);
    case FC_CREDIT:
	STINC(ff_rcredit);
	proto_remove_header(pkt);
	if(ff_rcv_credit(fleth[dev].fe_ntw, locp, fc->fc_cnt,
			 EV_CREDIT)) {
	    notify(dev);
	}
	return(0);
    case FC_ABSCREDIT:	 /* packet with new credits to use */
	STINC(ff_racredit);
	proto_remove_header(pkt);
	if(ff_rcv_credit(fleth[dev].fe_ntw, locp, fc->fc_cnt,
			 EV_ABSCREDIT)) {
	    notify(dev);
	}
	return(0);
    case FC_MCREQ:		
	STINC(ff_rmcreq);
	return 0;
    case FC_MCACK:
	STINC(ff_rmcack);
	return 0;
    case FC_MCNAK:
	STINC(ff_rmcnak);
	return 0;
    case FC_DATA:
	rcredit = fc->fc_cnt;
	if(rcredit > 0) STINC(ff_rpiggy);
	proto_remove_header(pkt);
	PROTO_LOOK_HEADER(fh, pkt, flip_hdr);
	if(fh == 0) {
	    return(0);
	}
	fl_orderhdr(fh);
	if (fh->fh_length >= sizeof(amphdr_t)) {
	    tmp = (amphdr_p) ((char *) fh + sizeof(flip_hdr));
	}
	if(fh->fh_length + sizeof(flip_hdr) > pkt->p_contents.pc_totsize) { 
	    printf("flip_arrived: bad packet\n");
	    return 0;
	}
	pkt->p_contents.pc_dirsize = pkt->p_contents.pc_totsize =
	    sizeof(flip_hdr) + fh->fh_length;

	if(fh->fh_total > ETH_DATA && fh->fh_type == FL_T_UNIDATA &&
	   ff_rcv_credit(fleth[dev].fe_ntw, locp, rcredit, EV_ARRIVE)) {
	    notify(dev);
	}
	setreleaseinfo(pkt, dev, locp, fh->fh_total > ETH_DATA && fh->fh_type ==
		       FL_T_UNIDATA);
	pktswitch(pkt, fleth[dev].fe_ntw, locp);
	return 1;
    default:
	printf("flarrive: flip packet of unknown type received\n");
	return 0;
    }
}


/*
int fllwsrv(queue_t *q)
{
	printf("fllwsrv\n");
	return(1);
}
*/


int fllrput(queue_t *q, mblk_t *mp)
{
    struct fleth *fe;
    struct ether_header *eh;
    pkt_p pkt;

#if 0
    if (busy) {		/* catch bouncing broadcast messages */
	freemsg(mp);
	return 0;
    }

    if (eh->ether_type != FLIP) {
	printf("FLIP: fllrput: got packet of type %x\n", eh->ether_type);
	freemsg(mp);
	return 0;
    }
#endif

    putq(q, mp);
}



int fllrsrv(queue_t *q)
{
    struct fleth *fe;
    struct ether_header *eh;
    pkt_p pkt;
    mblk_t *mp;
    int i;

    for (fe = fleth; fe < &fleth[nifs]; fe++)
	if (fe->q == q)
	    break;
    if (fe == &fleth[nifs]) {
	printf("FLIP: fllrsrv: got packet from unknown interface\n");
	printf("fllrsrv: queue is %x\n", q);
	printf("fllrsrv: fleth[0]->q is %x\n", fleth[0].q);
	return 0;
    }
    while ((mp = getq(q)) != NULL) {
	    eh = (struct ether_header *) mp->b_rptr;
	    if (DB_TYPE(mp) != M_DATA) {
		printf("fllrsrv: type != M_DATA\n");
		freemsg(mp);
		mutex_exit(&flip_mutex);
		continue;
	    }
	    mutex_enter(&flip_mutex);
	    if ((pkt = flip_pkt_acquire()) == 0) {
		printf("fleth_arrived: out of packets\n");
		freemsg(mp);
		mutex_exit(&flip_mutex);
		continue;
	    }

	    /*
	     * Second argument is used for alignment. Its size is taken and
	     * used for offset. We would have liked to use a type which
	     * size is zero, but no such type exists (yet). So int is the
	     * second best.
	     */
	    proto_setup_input(pkt, int);

	    assert(pkt->p_contents.pc_totsize + (mp->b_wptr - mp->b_rptr) -
		sizeof(struct ether_header) <= pkt->p_contents.pc_dirsize);
	    bcopy((char *) eh + sizeof(struct ether_header),
		    pkt_offset(pkt) + pkt->p_contents.pc_totsize,
		    (mp->b_wptr - mp->b_rptr) - sizeof(struct ether_header));
	    pkt->p_contents.pc_totsize += (mp->b_wptr - mp->b_rptr) -
			sizeof(struct ether_header);

	    if(!flarrive(fe-fleth, (char *) eh->ether_dhost.ether_addr_octet, 
		(char *) eh->ether_shost.ether_addr_octet, pkt))
		    pkt_discard(pkt);
	    mutex_exit(&flip_mutex);
	    freemsg(mp);
    }
    return 0;
}


void testbuf(char *buf, int size)
{
	char *p;
	volatile char c;

	for (p = buf; p < buf + size; p++)
		c = *p;
}

int copyin_buf1(queue_t *q, caddr_t from, caddr_t to, int size)
{
	struct rpc_args *fa;
	int offset;

	MYASSERT((struct rpc_args *)((struct fl_q_priv *) q->q_ptr)->db != 0);
	MYASSERT((struct rpc_args *)((struct fl_q_priv *) q->q_ptr)->buf1 != 0);
	fa = (struct rpc_args *)((struct fl_q_priv *) q->q_ptr)->db->b_rptr;
	offset = from - fa->rpc_buf1;
	DPRINTF(1, ("offset=%d, size=%d, fa->rpc_cnt1=%d\n",
		offset, size, fa->rpc_cnt1));
	if ((offset < 0) || (offset + size > fa->rpc_cnt1)) {
		printf("assertion of (offset < 0) || (offset + size > fa->rpc_cnt1) failed\n");
		return(1);
	}
	testbuf((char *) (((struct fl_q_priv *) q->q_ptr)->buf1->b_rptr + offset), size);
	testbuf(to, size);
	bcopy((char *) (((struct fl_q_priv *) q->q_ptr)->buf1->b_rptr + offset),
						to, size);
	return(0);
}


/* Flsend puts the message on the network. If the pkt contains virtual data,
 * flsend should be called the process that owns the virtual data.
 */
static void flsend(int dev, register pkt_p pkt, location *loc,
					unsigned short proto)
{
    int size, csiz;
    int dirsize;
    int r;
    dl_unitdata_req_t *unitdata_req;
    mblk_t *m_proto;
    mblk_t *chunk;
    struct rpc_device *rpcfd;
    struct rpc_info_t *rpc_info_p;
    
    size = pkt->p_contents.pc_totsize;
    dirsize = pkt->p_contents.pc_dirsize;
    /*
    assert(size <= ifp->if_mtu);
    */

    busy = 1;
    /* allocate space for M_PROTO message block */
    if ((m_proto = allocb(sizeof(dl_unitdata_req_t) + LEADDRL,
				    BPRI_HI)) == NULL) {
	printf("FLIP: flsend: out of message blocks\n");
	pkt_discard(pkt);
	busy = 0;
	return;
    }
    DB_TYPE(m_proto) = M_PROTO;
    unitdata_req = (dl_unitdata_req_t *) m_proto->b_wptr;
    unitdata_req->dl_primitive = DL_UNITDATA_REQ;
    unitdata_req->dl_dest_addr_length = LEADDRL;
    unitdata_req->dl_dest_addr_offset = sizeof(dl_unitdata_req_t);
    m_proto->b_wptr += sizeof(dl_unitdata_req_t);
    assert(loc != 0);
    bcopy(loc->l_bytes, (char *) m_proto->b_wptr, ETHERADDRL);
    m_proto->b_wptr += ETHERADDRL;
    bcopy((char *) &proto, (char *) m_proto->b_wptr, LEADDRL - ETHERADDRL);
    m_proto->b_wptr += (LEADDRL - ETHERADDRL);

    if ((chunk = allocb(size, BPRI_HI)) == NULL) {
		printf("FLIP: flsend: out of message blocks\n");
		pkt_discard(pkt);
		busy = 0;
		return;
    }
    DB_TYPE(chunk) = M_DATA;
    m_proto->b_cont = chunk;
    if (dirsize > 0) {
		assert(pkt != 0);
		bcopy(pkt_offset(pkt), (char *) chunk->b_wptr, dirsize);
		chunk->b_wptr += dirsize;
		pkt->p_contents.pc_offset += dirsize;
		size -= dirsize;
		dirsize = 0;
    }
    if (size > 0) {
	if (pkt->p_contents.pc_dstype != FL_DS_USER) {
		extern dev_info_t saved_dip;

		assert(pkt != 0);
		assert(pkt->p_contents.pc_virtual != 0);
		rpcfd = (struct rpc_device *) pkt->p_contents.pc_dsident;
		if (!rpcfd->rpc_open) {
		    /* printf("flsend: rpcfd->rpc_open not open anymore\n"); */
		    busy = 0;
		    pkt_discard(pkt);
		    return;
		}
		if (!rpcfd->mp_data) {
		    /* printf("flsend: rpcfd->mp_data == 0\n"); */
		    busy = 0;
		    pkt_discard(pkt);
		    return;
		}
		if (ddi_peekc(&saved_dip, (char *) (pkt->p_contents.pc_virtual),
				0) == DDI_FAILURE) {
		    /* printf("flsend: start of bcopy: invalid address\n"); */
		    pkt_discard(pkt);
		    busy = 0;
		    return;
		}
		if (ddi_peekc(&saved_dip,
			    (char *) (pkt->p_contents.pc_virtual + size),
				0) == DDI_FAILURE) {
		    /* printf("flsend: end of bcopy: invalid address\n"); */
		    pkt_discard(pkt);
		    busy = 0;
		    return;
		}
		bcopy((char *) (pkt->p_contents.pc_virtual),
				(char *) chunk->b_wptr, size);
		chunk->b_wptr += size;
	} else {
		printf("flsend: pkt->p_contents.pc_dstype == FL_DS_USER\n");
		rpcfd = (struct rpc_device *) pkt->p_contents.pc_dsident;
		assert(((rpcfd - rpc_dev) >= FIRST_RPC) &&
			((rpcfd - rpc_dev) <= LAST_RPC));
		rpc_info_p = &rpc_info[rpcfd - rpc_dev];
		if ((r = copyin_buf1(rpc_info_p->q,
			(caddr_t) (pkt->p_contents.pc_virtual),
				     (caddr_t) (chunk->b_wptr), size)) != 0) {
			    printf("copyin failed: %d\n", r);
			    freemsg(chunk);
			    pkt_discard(pkt);
			    busy = 0;
			    return;
		}
		chunk->b_wptr += size;
	}
    }
    if (flip_unlinked) {
	printf("flsend: flsend: FLIP driver unlinked from Ethernet\n");
	freemsg(m_proto);
	busy = 0;
        pkt_discard(pkt);
	return;
    }
    if (canputnext(WR(fleth[dev].q))) {
	    putnext(WR(fleth[dev].q), m_proto);
    } else {
	    static int	one_time = 0;

	    if (one_time++ % 10000)
		printf("flsend: canputnext fails, drop msg\n");
	    freemsg(m_proto);
    }
    assert(pkt != 0);
    pkt_discard(pkt);
    busy = 0;
}


void fleth_send(int dev, register pkt_p pkt, location *loc)
{
    register fc_hdr_p fc;
    flip_hdr *fh;
    static location l;
    pkt_p p;
    fc_cnt_t credit = 0;
    int last;
    int userdata = 0;
    struct rpc_device *rpcfd;
    
    rpcfd = (struct rpc_device *) pkt->p_contents.pc_dsident;
    proto_cur_header(fh, pkt, flip_hdr);
    if(fh->fh_total > ETH_DATA) {
	userdata = pkt->p_contents.pc_dstype == FL_DS_USER;
        if(ff_store(fleth[dev].fe_ntw, pkt, loc, &credit)) {
	    /* Note that this will only happen if a gateway forwards an already
	     * fragmented message.
	     */
	    assert(!(pkt->p_contents.pc_virtual != 0 && 
		pkt->p_contents.pc_dstype == FL_DS_USER));
	    if(credit > 0) STINC(ff_spiggy);
	    proto_fix_header(pkt);
	    PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
	    fc->fc_type = FC_DATA;
	    fc->fc_cnt = credit;
	    proto_fix_header(pkt);
	    flsend(dev, pkt, loc, FLIP);
        }
	while(ff_get(fleth[dev].fe_ntw, &l, &p, &credit) > 0) {
	    proto_fix_header(p);
	    if(credit > 0) STINC(ff_spiggy);
	    PROTO_ADD_HEADER(fc, p, fc_hdr_t);
	    fc->fc_type = FC_DATA;
	    fc->fc_cnt = credit;
	    proto_fix_header(p);
	    flsend(dev, p, &l, FLIP);
	}
    } else {	/* message fits in one Ethernet packet */
	proto_fix_header(pkt);
	PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
	fc->fc_type = FC_DATA;
	fc->fc_cnt = 0;
	proto_fix_header(pkt);
	flsend(dev, pkt, loc, FLIP);
    }
}


/* The packet switch wants to broadcast a message on the specified network.
 */
void
fleth_broadcast(int dev, pkt_p pkt)
{
    register fc_hdr_p fc;
    static location loc;

    proto_fix_header(pkt);
    PROTO_ADD_HEADER(fc, pkt, fc_hdr_t);
    fc->fc_type = FC_DATA;
    fc->fc_cnt = 0;
    proto_fix_header(pkt);
    bcopy((caddr_t) flipbrdcast, loc.l_bytes, 6);
    flsend(dev, pkt, &loc, FLIP);
}


int fleth_stat(char *begin, char *end)
{
    char *p;

    p = bprintf(begin, end, "========= no statistics ========\n");
    if (p == 0) {
	printf("fl_eth_stat buffer overflow\n");
	return 0;
    }
    return(p - begin);
}


void fleth_init(void){
    release_p r;
    
    DPRINTF(1, ("fleth_init\n"));
    assert(sizeof(fc_hdr_t) == 2);
    free_release =
	(release_p) kmem_zalloc((size_t) (NRELEASE * sizeof(release_t)),
								KM_SLEEP);
    for (r = free_release; r < free_release + NRELEASE - 1; r++) {
	r->r_next = r + 1;
    }
    r->r_next = 0;
}
