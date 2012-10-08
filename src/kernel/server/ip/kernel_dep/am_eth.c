/*	@(#)am_eth.c	1.6	96/02/27 14:19:52 */
/*
flip_dep/am_eth.c
*/

#include <stdlib.h>

#include "inet.h"
#include "amoeba_incl.h"
#include "osdep_eth.h"
#include "generic/event.h"
#include "generic/type.h"

#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/eth.h"
#include "generic/eth_int.h"
#include "generic/io.h"
#include "generic/sr.h"

#undef eth_send 
#include <global.h>
#include <flip/eth_if.h>
#include <flip/packet.h>

INIT_PANIC();

#define USE_MALLOCS	1

FORWARD int eth_proto_arrived _ARGS(( int ifno, pkt_t *pkt ));
FORWARD void receive_thread _ARGS(( void ));
FORWARD void pkt_released _ARGS(( long arg ));

typedef struct eth_rec_info
{
	mutex eri_wait;
	pkt_t *eri_packet_head;
	pkt_t *eri_packet_tail;
} eth_rec_info_t;

eth_rec_info_t eth_rec_info[ETH_PORT_NR];

#ifndef AM_ETH_NPKT
#define AM_ETH_NPKT	50
#endif

#define PKTSIZE	1600
#define RECEIVE_STACK	8192

PRIVATE pkt_t packet[AM_ETH_NPKT];
#if USE_MALLOCS
PRIVATE char (*pool_data)[PKTSIZE];
#else
PRIVATE char pool_data[PKTSIZE][AM_ETH_NPKT];
#endif
PRIVATE pool_t eth_pool;
PRIVATE int thread_nr;

FORWARD void eth_init0 ARGS(( void ));

PRIVATE void eth_init0()
{
	int i;
	int result;
	eth_port_t *eth_port;

	eth_port= &eth_port_table[0];
	eth_port->etp_osdep.etp_flip_port= 0;

	eth_info (eth_port->etp_osdep.etp_flip_port,
		(char *)eth_port->etp_ethaddr.ea_addr, NULL, NULL);

	for (i= 0; i<ETH_PORT_NR; i++)
	{
		mu_init(&eth_rec_info[i].eri_wait);
		mu_lock(&eth_rec_info[i].eri_wait);
		eth_rec_info[i].eri_packet_head= 0;
		eth_rec_info[i].eri_packet_tail= 0;
	}

#if USE_MALLOCS
	printf("am_eth.c: malloc %d packets (%dK)\n", AM_ETH_NPKT, 
		AM_ETH_NPKT * PKTSIZE / 1024);
	pool_data= (char (*)[PKTSIZE])malloc(AM_ETH_NPKT*PKTSIZE);
	if (!pool_data)
		ip_panic(( "malloc failed" )); 
#endif
	pkt_init(&eth_pool, PKTSIZE, packet, AM_ETH_NPKT, pool_data[0], 0, 0);

	if (sr_add_minor (ETH_DEV0, eth_port- eth_port_table,
		eth_open, eth_close, eth_read, eth_write,
		eth_ioctl, eth_cancel)<0)
		ip_panic (( "eth.c: can't sr_init" ));

	eth_port->etp_flags |= EPF_ENABLED;
	eth_port->etp_wr_pack= 0;
	eth_port->etp_rd_pack= 0;
	thread_nr= 0;
	for (i= 0; i<ETH_PORT_NR; i++)
	{
		NewKernelThread(receive_thread, RECEIVE_STACK);
		mu_lock(&mu_generic);
	}
	eth_register(ETH_RARP_PROTO, "rarp for tcpip", eth_proto_arrived);
	eth_register(ETH_ARP_PROTO, "arp for tcpip", eth_proto_arrived);
	eth_register(ETH_IP_PROTO, "ip for tcpip", eth_proto_arrived);
}

PUBLIC void
osdep_eth_init()
{
	eth_init0();
}

PRIVATE void receive_thread()
{
	int port_nr;
	eth_rec_info_t *info_ptr;
	eth_port_t *eth_port;
	pkt_t *pkt;
	acc_t *packet;

	port_nr= thread_nr++;
	assert (port_nr < ETH_PORT_NR);
	info_ptr= &eth_rec_info[port_nr];
	eth_port= &eth_port_table[port_nr];

	for (;;)
	{
		mu_unlock(&mu_generic);
		mu_lock(&info_ptr->eri_wait);
		mu_lock(&mu_generic);
		assert (info_ptr->eri_packet_head);
		pkt= info_ptr->eri_packet_head;
		packet= bf_memreq(pkt->p_contents.pc_dirsize);
		assert(pkt->p_contents.pc_dirsize==pkt->p_contents.pc_totsize);
		assert(!packet->acc_next);
		memcpy(ptr2acc_data(packet), pkt_offset(pkt),
			pkt->p_contents.pc_dirsize);
		info_ptr->eri_packet_head= pkt->p_admin.pa_next;
		if (info_ptr->eri_packet_head)
			mu_unlock(&info_ptr->eri_wait);
		pkt_discard(pkt);
		eth_arrive(eth_port, packet, bf_bufsize(packet));
	}
}

PRIVATE int eth_proto_arrived(ifno, pkt)
int ifno;
pkt_t *pkt;
{
	eth_rec_info_t *info_ptr;

	assert (ifno >= 0);
	if (ifno >= ETH_PORT_NR)
	{
		DBLOCK(2, printf("discarding packet\n"));
		pkt_discard(pkt);
		return 1;
	}
	info_ptr= &eth_rec_info[ifno];
	pkt->p_admin.pa_next= NULL;
	if (info_ptr->eri_packet_head)
		info_ptr->eri_packet_tail->p_admin.pa_next= pkt;
	else
	{
		info_ptr->eri_packet_head= pkt;
		mu_unlock(&info_ptr->eri_wait);
	}
	info_ptr->eri_packet_tail= pkt;
	return 1;
}

PUBLIC int eth_get_stat(eth_port, eth_stat)
eth_port_t *eth_port;
eth_stat_t *eth_stat;
{
	DBLOCK(2, printf("eth_get_stat not implemented\n"));
	return NW_OK;
}

PUBLIC void eth_set_rec_conf (eth_port, flags)
eth_port_t *eth_port;
u32_t flags;
{
	DBLOCK(2, printf("not implemented\n"));
}

PUBLIC void eth_write_port(eth_port, pack)
eth_port_t *eth_port;
acc_t *pack;
{
	int i, pack_size, result;
	acc_t *pack_ptr;
	eth_hdr_t *eth_hdr;
	pkt_p pkt;

        assert(eth_port->etp_wr_pack == NULL);
        eth_port->etp_wr_pack= pack;

	if (pack->acc_next)
	{
		/* packet is too fragmented */
		pack= bf_pack(pack);
		eth_port->etp_wr_pack= pack;
	}
	assert (!pack->acc_next);

	pack_size= bf_bufsize(pack);

	assert (pack_size >= ETH_MIN_PACK_SIZE);

	PKT_GET(pkt, &eth_pool);
	if (pkt)
	{
		pkt_set_release(pkt, pkt_released, eth_port-eth_port_table);
		proto_init(pkt);
		proto_set_virtual(pkt, 0, 0, (long)ptr2acc_data(pack)+
			ETH_HDR_SIZE, pack_size-ETH_HDR_SIZE);
		eth_hdr= (eth_hdr_t *)ptr2acc_data(pack);
		eth_send(eth_port->etp_osdep.etp_flip_port, pkt, 
			(char *)eth_hdr->eh_dst.ea_addr,
			ntohs(eth_hdr->eh_proto));
	}
	else
	{
		DBLOCK(1, printf("out of packets\n"));
	}
			
	/* BUG - We should implement something to allow more than one packet
	 * in the transmit ring! The current scheme limits throughput.
	 */
}

PRIVATE void pkt_released(arg)
long arg;
{
	eth_port_t *eth_port;
	ev_arg_t ev_arg;

	eth_port= &eth_port_table[arg];
	ev_arg.ev_ptr= eth_port;
	ev_enqueue(&eth_port->etp_sendev, eth_loop_ev, ev_arg);
}
