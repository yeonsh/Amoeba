/*	@(#)am4glue.c	1.4	96/02/27 14:00:38 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Glue routines to hack Amoeba 3 onto flippish lower layer
 */

#include <string.h>
#include "assert.h"
INIT_ASSERT

#include "amoeba.h"
#include "internet.h"

#include "sys/flip/packet.h"
#include "../network/ethproto.h"
#include "../network/ethpreamble.h"

#include "global.h"
#include "machdep.h"
#include "byteorder.h"
#include "sys/proto.h"

pool_t amoeba3pool;

#ifdef STATISTICS
struct {
	int am3_arrived;
	int am3_send;
} am3stat;

#define STINC(type)	am3stat.type++

am4_statistics(begin, end)
char *begin;
char *end;
{
	char *p;

	p = bprintf(begin, end, "======= amoeba 3 statistics ============\n");
	p = bprintf(p, end, "sent    %7ld ", am3stat.am3_send);
	p = bprintf(p, end, "arrived %7ld\n", am3stat.am3_arrived);
	return p - begin;
}
#else
#define STINC(type)
#endif

typedef unsigned short  unshort;		/* HACK */

#define NPACKETS 	40
#define PKTSIZE 	400

#define ETHERBITS	0x80
#define FAKESITENO	0xFF
/*
 * Mapping routine from 48-bit Ethernet addresses to 7-bit pseudo PRONET 
 * addresses. The trick is that a node does not know its own number
 * but the destination fills it in for him.
 */

#define MAPENTRIES	127	/* Cannot use 0xFF */

char gwaddr[6];
char broadcastaddr[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
char eamap[MAPENTRIES+1][8];	/* 8 instead of 6 for speed */

#define eacmp(a1,a2) PORTCMP((port *) (a1), (port *) (a2))
#define eanull(a) NULLPORT((port *) (a))
#define eacopy(f,t) (* (port *) (t) = * (port *) (f))   /* 48-bit hack */

static
ealookup(eaddr)
register char *eaddr;
{
	register index;
	register i;
	register char *mep;

	index = eaddr[5]&0x7F;	/* hash */
	if (index >=MAPENTRIES)
		index = 0;
	i = index;
	do {
		mep = eamap[i];
		if (eacmp(eaddr,mep))
			return i|ETHERBITS;
		if (eanull(mep)) {
			eacopy(eaddr,mep);
			return i|ETHERBITS;
		}
		i++;
		if (i>=MAPENTRIES)
			i = 0;
	} while(i != index);
	return 0;
}

static
eam_init()
{

	eacopy(broadcastaddr, eamap[MAPENTRIES]);
}

/*
 * Transmit side
 */

static pkt_p curopacket;		/* Current output packet */
static char *curoaddr;			/* Pointer to Ethernet Address */


puthead(dest, source, ident, seq, type, size)
address dest, source;
char ident, seq, type;
unshort size;
{
	register pkt_p packet;
	register struct pktheader *aih;
	char dstnode;

#ifdef TCPIP_DEBUG
printf("puthead(%x, %x, %d, %d, %d, %d)\n",dest&0xFFFF, source&0xFFFF, ident&0xFF,
		seq&0xFF, type&0xFF, size);
#endif
#ifdef TCPIP_DEBUG
	printf("amoeba3pool.pp_nbuf= %d\n", amoeba3pool.pp_nbuf);
	{ pkt_p pkt; for(pkt= amoeba3pool.pp_freelist; pkt;
		pkt= pkt->p_admin.pa_next)
		printf("(0x%x, %d) ", pkt, pkt->p_admin.pa_allocated);
	printf("\n"); }
#endif
	PKT_GET(packet, &amoeba3pool);
	if (packet == 0) {
		printf("puthead dropped packet\n");
		return;
	}
	curopacket = packet;
	proto_init(packet);
	PROTO_ADD_HEADER(aih, packet, struct pktheader);
	aih->ph_dstnode = dstnode = lobyte(dest);

	if ((dstnode & ETHERBITS) == 0) {
		assert(!eanull(gwaddr));
		curoaddr = gwaddr;
	} else
		curoaddr = eamap[dstnode&0x7F];

	aih->ph_srcnode = 0;
	aih->ph_dstthread = hibyte(dest);
	aih->ph_srcthread = hibyte(source);
	aih->ph_ident = ident;
	aih->ph_seq = seq;
	aih->ph_type = type;
	aih->ph_flags = 0;
	aih->ph_size = size+24;			/* old protocol hack */
	enc_s_le(&aih->ph_size);
	proto_fix_header(packet);

	if (size == 0) {
		STINC(am3_send);
		eth_send(0, curopacket, curoaddr, EP_AMOEBA3);
	}
}

append(data, size, send)
phys_bytes data;
register unsigned size;
{
	register pkt_p packet;

	if ((packet = curopacket) == 0)
		return;
	if (send) {
		/*
		 * Last part, don't copy
		 */
		proto_set_virtual(packet, 0, 0, data, size);
		STINC(am3_send);
		eth_send(0, curopacket, curoaddr, EP_AMOEBA3);
	} else {
		proto_dir_append(packet, data, size);
	}
}

/*
 * Receive side
 */

static char *inptr;
static unshort npkts;

int
amoeba3_arrived(ifno, packet)
int ifno;
register pkt_p packet;
{
	register struct pktheader *aih;
	register eh_p eh;
	char srcaddr[6];

	proto_cur_header(eh, packet, eh_t);
	assert(eh);
	(void) memmove((_VOIDSTAR) srcaddr, (_VOIDSTAR) eh->eh_srcaddr, 6);
	proto_remove_header(packet);

	STINC(am3_arrived);
	if (ifno != 0) {
		/*printf("Discarding amoeba3 packet from interface %d\n", ifno);*/
		return 0;
	}
	npkts++;
	aih = proto_look_header(packet, struct pktheader);
	if (aih == 0) {
		printf("very short packet discarded\n");
		return 0;
	}
	dec_s_le(&aih->ph_size);
	if (aih->ph_size > packet->p_contents.pc_totsize+14) {
		printf("Discarding short packet, should be %d bytes, was %d\n",
			aih->ph_size, packet->p_contents.pc_totsize+14);
		return 0;
	}
	if (aih->ph_srcnode == 0) {
		/*
		 * Normal ethernet node, look it up in table
		 */
		if ((aih->ph_srcnode = ealookup(srcaddr))==0) {
			printf("Ethernet mapping table overflow\n");
			return 0;
		}
	} else {
		/*
		 * Coming from PRONET gateway
		 */
		if (aih->ph_srcnode&ETHERBITS)
			return 0;	/* This used to be a killer */
		if (eanull(gwaddr)) {
			eacopy(srcaddr,gwaddr);
			printf("Gateway to PRONET at ");
			eth_praddr(gwaddr);
			printf("\n");
		}
	}
	aih->ph_dstnode = FAKESITENO;
	aih->ph_size -= 24;
	inptr = pkt_offset(packet)+sizeof(*aih)+HEADERSIZE;
	pkthandle(aih, inptr - HEADERSIZE);
	/*
	 * Since this is the Amoeba kernel we are guaranteed to
	 * be done with the packet, so...
	 */
	return 0;
}

pickoff(data, size)
phys_bytes data;
unsigned size;
{

	phys_copy((phys_bytes) inptr, data, (phys_bytes) size);
	inptr += size;
}

/*
 * random leftovers
 */

netenable() {

	/* nop */
}

net_port(p)
char *p;
{

	eth_addr(0, p);
}

getall() {

	/* nop */
}

interinit() {

	return 0xFF;
}

char myaddr[6];

initnetdev() {
	static pkt_t ambufs[NPACKETS];
	static char amdata[NPACKETS*PKTSIZE];

	pkt_init(&amoeba3pool, PKTSIZE, ambufs, NPACKETS, amdata,
						(void (*)()) 0, (long) 0);
#ifdef TCPIP_DEBUG
	printf("amoeba3pool.pp_nbuf= %d\n", amoeba3pool.pp_nbuf);
	{ pkt_p pkt; for(pkt= amoeba3pool.pp_freelist; pkt;
		pkt= pkt->p_admin.pa_next)
		printf("(0x%x, %d) ", pkt, pkt->p_admin.pa_allocated);
	printf("\n"); }
	printf("doing amoeba4glue_nop()\n");
	amoeba4glue_nop();
	printf("amoeba4glue_nop() done\n");


#endif
	eam_init();
	eth_addr(0, myaddr);
	eth_register(EP_AMOEBA3, "amoeba3", amoeba3_arrived);
	eth_register(EP_AMOEBA3ALT, "amoeba3 new", amoeba3_arrived);
}

#ifdef TCPIP_DEBUG
amoeba4glue_nop()
{
	static pkt_p pkt[NPACKETS];
	int i;

	for (i= 0; i<NPACKETS; i++)
		PKT_GET(pkt[i], &amoeba3pool);
	for (i= 0; i<NPACKETS; i++)
		pkt_discard(pkt[i]);
}
#endif

