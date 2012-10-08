/*	@(#)flip_ip.c	1.7	96/02/27 14:13:42 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
flip_ip.c

Created July 23, 1991 by Philip Homburg

Flip over udp or tcp
*/

#include "nw_task.h"
#include "amoeba_incl.h"

#include <sys/flip/packet.h>
#include <sys/flip/flip.h>
#include <sys/flip/switch.h>
#include <sys/flip/fragment.h>
#include <sys/flip/fl_byteorder.h>

#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/io.h"
#include "generic/udp.h"
#include "generic/tcp.h"

#include "flip_ip.h"

INIT_PANIC();

#define NEW_FLOW_CODE	1

#define FIP_PRI_EXP_BROAD_BUFS	2
#define FIP_PRI_EXP_BUFS	3
#define SEND_THREAD_STACK	8192

#if INCLUDE_UDP
#define UDP_READ_SIZE		BUF_S
#define UDP_MTU			8096
#endif

#define TCP_READ_SIZE		8000
#define TCP_TIMOUT		(60 * HZ)		/* One minute */
#define TCP_MTU			1900

#define DIR_HDR_SIZE		128
/* #define DIR_HDR_SIZE		1024 */
#define NPKT	10
#define PKTSIZE	(DIR_HDR_SIZE+PKTBEGHDR+256+2)

static pkt_t packet[NPKT];
/* static acc_t *packet_acc[NPKT]; */
static char data[NPKT][PKTSIZE];
static pool_t fip_pool;

#if INCLUDE_UDP
typedef struct fip_udp_peer
{
	ipaddr_t fup_ipaddr;
	udpport_t fup_udpport;
	struct fip_udp_peer *fup_next;
} fip_udp_peer_t;
#endif

fip_port_t fip_port_table[FIP_PORT_NR];
fip_tcp_peer_t fip_tcp_peer_table[TCP_PEER_NR];
#if NEW_FLOW_CODE
int tcp_restart_process;
#endif /* NEW_FLOW_CODE */
mutex mu_flip_ip_inited;

static semaphore send_sema;
static send_thread_started;
static location null_location;

void flip_ip_init ARGS(( void ));
static void fip_buffree ARGS(( int prioriry, size_t reqsize ));
static void fip_main ARGS(( fip_port_t *fip_port ));
static void fip_send ARGS(( int dev, pkt_p pkt, location *loc ));
static void fip_bcast ARGS (( int dev, pkt_p pkt ));
static void fip_notify ARGS (( int dev ));
static void fip_request ARGS (( int dev, location *loc, pkt_t *pkt ));
static void pkt_released ARGS(( long arg ));
static void send_thread ARGS(( void ));
static void pp_notify ARGS(( long arg, pkt_p pkt ));
#if INCLUDE_UDP
/* For udp */
static acc_t *fip_get_udp_data ARGS(( int fd, size_t offset, size_t count, 
	int for_ioctl ));
static int fip_put_udp_data ARGS(( int fd, size_t offset, acc_t *data, 
	int for_ioctl ));
static void add_udp_peers ARGS(( fip_port_t *fip_port ));
static void read_udp_packets ARGS(( fip_port_t *fip_port ));
static void send_udp_pkt ARGS(( fip_port_t *fip_port, acc_t *acc,
	location *loc ));
static void udp_send ARGS(( fip_port_t *fip_port ));
static void process_udp_data ARGS(( fip_port_t *fip_port, acc_t *data ));
#endif /* INCLUDE_UDP */
#if INCLUDE_TCP
/* For tcp */
static void read_tcp_packets ARGS(( fip_tcp_peer_t *fip_tcp_peer ));
static void send_tcp_pkt ARGS(( fip_port_t *fip_port, acc_t *acc,
	location *loc ));
static void tcp_send ARGS(( fip_tcp_peer_t *fip_tcp_peer ));
static acc_t *fip_get_tcp_main_data ARGS(( int fd, size_t offset, size_t count, 
	int for_ioctl ));
static int fip_put_tcp_main_data ARGS(( int fd, size_t offset, acc_t *data, 
	int for_ioctl ));
static acc_t *fip_get_tcp_write_data ARGS(( int fd, size_t offset,
	size_t count, int for_ioctl ));
static int fip_put_tcp_write_data ARGS(( int fd, size_t offset, acc_t *data, 
	int for_ioctl ));
static acc_t *fip_get_tcp_read_data ARGS(( int fd, size_t offset, size_t count, 
	int for_ioctl ));
static int fip_put_tcp_read_data ARGS(( int fd, size_t offset, acc_t *data, 
	int for_ioctl ));
static void tcp_timeout ARGS(( int ref, timer_t *timer ));
static void tcp_set_write_timer ARGS(( fip_tcp_peer_t *fip_tcp_peer ));
static void tcp_set_read_timer ARGS(( fip_tcp_peer_t *fip_tcp_peer ));
static acc_t *process_tcp_data ARGS(( fip_tcp_peer_t *fip_tcp_peer,
	acc_t *packet ));
static void add_peer_tcp ARGS(( fip_port_t *fip_port, ipaddr_t host, 
	Tcpport_t listen_port, Tcpport_t connect_port ));
#endif /* INCLUDE_TCP */

void flip_ip_init()
{
	fip_port_t *fip_port;
	int i;


#if DEBUG & 256
 { where(); printf("flip_ip_init called\n"); }
#endif

	assert (BUF_S >= sizeof(struct nwio_udpopt));
	assert (BUF_S >= sizeof(struct udp_io_hdr));

	mu_lock(&mu_generic);

	mu_init(&mu_flip_ip_inited);
	mu_lock(&mu_flip_ip_inited);

	send_thread_started= 0;
	null_location.u_long[0]= 0;
	null_location.u_long[1]= 0;
#if 0
	/* For UDP */
	fip_port_table[0].fp_udpdev= UDP0;
	fip_port_table[0].fp_nettype= FPNT_UDP;
#else
	/* For TCP */
	fip_port_table[0].fp_tcpdev= TCP0;
	fip_port_table[0].fp_nettype= FPNT_TCP;
#endif
	for (i= 0; i<TCP_PEER_NR; i++)
	{
		fip_tcp_peer_table[i].ftp_flags= 0;
#if DEBUG
		fip_tcp_peer_table[i].ftp_n_req_credit= 0;
#endif
	}

	bf_logon(fip_buffree);
	pkt_init(&fip_pool, PKTSIZE, packet, NPKT, data[0], pp_notify, 0);

	for (i= 0, fip_port= fip_port_table; i<FIP_PORT_NR; i++, fip_port++)
	{
		switch(fip_port->fp_nettype)
		{
#if INCLUDE_UDP
		case FPNT_UDP:
			fip_port->fp_state= FPS_UDP_EMPTY;
			fip_port->fp_udp_send_head= NULL;
			fip_port->fp_udp_send_tail= NULL;
			break;
#endif
#if INCLUDE_TCP
		case FPNT_TCP:
			fip_port->fp_state= FPS_TCP_EMPTY;
			fip_port->fp_tcp_peerlist= NULL;
			break;
#endif
		default:
			ip_panic(( "unknown nettype" ));
		}
		fip_port->fp_flags= FPF_EMPTY;
		fip_port->fp_pkt_head= NULL;
		fip_port->fp_pkt_tail= NULL;

		fip_main(fip_port);
	}
	mu_unlock(&mu_generic);
}

static void fip_main(fip_port)
fip_port_t *fip_port;
{
	int result;
	short on, trusted;

#if DEBUG & 256
 { where(); printf("fip_main(&fip_port_table[%d]) called\n", 
	fip_port-fip_port_table); }
#endif
	switch(fip_port->fp_state)
	{
#if INCLUDE_UDP
	case FPS_UDP_EMPTY:
		fip_port->fp_state= FPS_UDP_SETOPT;

		fip_port->fp_udpfd= udp_open(fip_port->fp_udpdev, 
			fip_port-fip_port_table, fip_get_udp_data, 
			fip_put_udp_data);
		if (fip_port->fp_udpfd < 0)
		{
			fip_port->fp_state= FPS_UDP_ERROR;
			ip_warning(( "unable to open udp port" ));
			return;
		}

		result= udp_ioctl(fip_port->fp_udpfd, NWIOSUDPOPT);
		if (result == NW_SUSPEND)
		{
			fip_port->fp_flags |= FPF_SUSPEND;
			return;
		}
assert(result == NW_OK);
assert(fip_port->fp_state == FPS_UDP_GETOPT);
		/* drops_through */
	case FPS_UDP_GETOPT:
assert(!(fip_port->fp_flags & FPF_SUSPEND));

		result= udp_ioctl(fip_port->fp_udpfd, NWIOGUDPOPT);
		if (result == NW_SUSPEND)
			fip_port->fp_flags |= FPF_SUSPEND;
assert(result == NW_OK);
assert(fip_port->fp_state == FPS_UDP_MAIN);
		/* drops through */
	case FPS_UDP_MAIN:
assert(!(fip_port->fp_flags & FPF_SUSPEND));

		add_udp_peers(fip_port);
		if (!send_thread_started)
		{
			send_thread_started= 1;
			NewKernelThread(send_thread, SEND_THREAD_STACK);
			mu_lock(&mu_generic);
			mu_unlock(&mu_flip_inited);
		}
		fip_port->fp_flipdev= flip_network("flip-ip", fip_port-
			fip_port_table, 3, 0, UDP_MTU, fip_send, fip_bcast, 
			0, 0, 0, 1, fip_notify, 20);
		on= 0;
		flip_control(fip_port->fp_flipdev, NULL, NULL, &on);
#if DEBUG
 { where(); printf("fip_port_table[%d].fp_flipdev= %d\n", 
	fip_port-fip_port_table, fip_port->fp_flipdev); }
#endif
		read_udp_packets(fip_port);
		return;
#endif /* INCLUDE_UDP */
#if INCLUDE_TCP
	case FPS_TCP_EMPTY:
		fip_port->fp_state= FPS_TCP_GETCONF;

		fip_port->fp_tcpfd= tcp_open(fip_port->fp_tcpdev, 
			fip_port-fip_port_table, fip_get_tcp_main_data,
			fip_put_tcp_main_data);
		if (fip_port->fp_tcpfd < 0)
		{
			fip_port->fp_state == FPS_TCP_ERROR;
			ip_warning(( "unable to open udp port" ));
			return;
		}

#if DEBUG & 256
 { where(); printf("doing NWIOGTCPCONF\n"); }
#endif
		result= tcp_ioctl(fip_port->fp_tcpfd, NWIOGTCPCONF);
		if (result == NW_SUSPEND)
		{
			fip_port->fp_flags |= FPF_SUSPEND;
			return;
		}
assert(result == NW_OK);
assert(fip_port->fp_state == FPS_TCP_MAIN);
		/* drops through */
	case FPS_TCP_MAIN:
	{
		fip_tcp_peer_t *fip_tcp_peer;
		int i;

assert(!(fip_port->fp_flags & FPF_SUSPEND));
		tcp_close(fip_port->fp_tcpfd);

		for (i= 0; tcp_peers[i].tpr_lport; i++)
		{
			add_peer_tcp(fip_port, *(ipaddr_t *)tcp_peers[i].
				tpr_addr, htons(tcp_peers[i].tpr_lport), 
				htons(tcp_peers[i].tpr_cport));
		}
		if (!send_thread_started)
		{
			send_thread_started= 1;
			NewKernelThread(send_thread, SEND_THREAD_STACK);
			mu_lock(&mu_generic);
			mu_unlock(&mu_flip_ip_inited);
		}
#ifndef OLDFRAGMENTCODE
		fip_port->fp_flipdev= flip_newnetwork("flip-ip", 
			fip_port-fip_port_table, 3, 0, fip_send, fip_bcast,
				NULL, NULL, 0);
		ff_newntw(fip_port->fp_flipdev, fip_port-fip_port_table,
			TCP_MTU, 1, 100,  fip_request, NULL);
#else	/* OLDFRAGMENTCODE */
		fip_port->fp_flipdev= flip_network("flip-ip", fip_port-
			fip_port_table, 3, 0, TCP_MTU, fip_send, fip_bcast, 
			0, 0, 0, 1, fip_notify, fip_request, 100);
#endif	/* OLDFRAGMENTCODE */
		on= 0;
		trusted= 1;
		flip_control(fip_port->fp_flipdev, NULL, NULL, &on, &trusted);
#if DEBUG & 256
 { where(); printf("fip_port_table[%d].fp_flipdev= %d\n", 
	fip_port-fip_port_table, fip_port->fp_flipdev); }
#endif
#if 0
		for (fip_tcp_peer= fip_port->fp_tcp_peerlist; fip_tcp_peer;
			fip_tcp_peer= fip_tcp_peer->ftp_next)
			read_tcp_packets(fip_tcp_peer);
#endif
		return;
	}
#endif /* INCLUDE_TCP */
	default:
		ip_panic(( "unknown state" ));
		break;
	}
}

#if INCLUDE_UDP
/************************************************************************
*				UDP code				*
************************************************************************/

static acc_t *fip_get_udp_data (fd, offset, count, for_ioctl)
int fd;
size_t offset;
size_t count;
int for_ioctl;
{
	fip_port_t *fip_port;
	int result;

#if DEBUG & 2
 { where(); printf("fip_get_udp_data(%d, %d, %d, %d) called\n",
	fd, offset, count, for_ioctl); }
#endif

assert(fd >= 0 && fd < FIP_PORT_NR);
	
	fip_port= &fip_port_table[fd];

	switch(fip_port->fp_state)
	{
	case FPS_UDP_SETOPT:
assert (for_ioctl);
		if (!count)
		{
			result= (int)offset;
			if (result<0)
			{
				fip_port->fp_state= FPS_UDP_ERROR;
				break;
			}
			fip_port->fp_state= FPS_UDP_GETOPT;
			if (fip_port->fp_flags & FPF_SUSPEND)
			{
				fip_port->fp_flags &= ~FPF_SUSPEND;
				fip_main(fip_port);
			}
		}
		else
		{
			struct nwio_udpopt *udpopt;
			acc_t *acc;

assert (!offset);
assert (count == sizeof(*udpopt));

			acc= bf_memreq(sizeof(*udpopt));
			udpopt= (struct nwio_udpopt *)ptr2acc_data(acc);
			udpopt->nwuo_flags= NWUO_EXCL | NWUO_LP_SET |
				NWUO_EN_LOC | NWUO_DI_BROAD | NWUO_RP_ANY |
				NWUO_RA_ANY | NWUO_RWDATALL | NWUO_DI_IPOPT;
			udpopt->nwuo_locport= htons(UDPPORT_FLIP);
			return acc;
		}
		break;
	case FPS_UDP_MAIN:
assert (!for_ioctl);
assert (fip_port->fp_flags & FPF_SEND_IP);
		if (!count)
		{
			acc_t *tmp_acc;

			result= (int)offset;
			tmp_acc= fip_port->fp_udp_send_head;
			fip_port->fp_udp_send_head= tmp_acc->acc_ext_link;
			bf_afree(tmp_acc);
			if (result<0)
			{
#if DEBUG
 { where(); }
#endif
				printf("udp_write_gave error %d\n", result);
			}
			if (fip_port->fp_flags & FPF_SEND_SP)
			{
				fip_port->fp_flags &= ~(FPF_SEND_IP|
					FPF_SEND_SP);
				udp_send(fip_port);
				break;
			}
			fip_port->fp_flags &= ~FPF_SEND_IP;
		}
		else
			return bf_cut(fip_port->fp_udp_send_head, offset,
				count);
		break;
	default:
#if DEBUG
 { where(); printf("fip_get_udp_data called but state is %d\n",
	fip_port->fp_state); }
#endif
		ip_panic(( "illegal state in fip_get_udp_data" ));
	}
	return NULL;
}

static int fip_put_udp_data(fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	fip_port_t *fip_port;
	int result;

#if DEBUG & 2
 { where(); printf("fip_put_udp_data(%d, %d, 0x%x, %d) called\n", fd, offset,
	data, for_ioctl); }
#endif

assert (fd >= 0 && fd < FIP_PORT_NR);

	fip_port= &fip_port_table[fd];

	switch (fip_port->fp_state)
	{
	case FPS_UDP_GETOPT:
		if (!data)
		{
			result= (int)offset;
			if (result<0)
			{
#if DEBUG
 { where(); printf("got error %d\n", result); }
#endif
				fip_port->fp_state= FPS_UDP_ERROR;
				break;
			}
			fip_port->fp_state= FPS_UDP_MAIN;
			if (fip_port->fp_flags & FPF_SUSPEND)
				fip_main(fip_port);
		}
		else
		{
			struct nwio_udpopt *udpopt;

			data= bf_packIffLess(data, sizeof(*udpopt));
			udpopt= (struct nwio_udpopt *)ptr2acc_data(data);
			fip_port->fp_ipaddr= udpopt->nwuo_locaddr;
			bf_afree(data);
		}
		break;
	case FPS_UDP_MAIN:
assert (!for_ioctl);
assert (fip_port->fp_flags & FPF_READ_IP);
		if (!data)
		{
			result= (int)offset;
			if (result<0)
			{
				ip_panic(( "unable to udp_read" ));
			}
			if (fip_port->fp_flags & FPF_READ_SP)
			{
				fip_port->fp_flags &= ~(FPF_READ_IP|
					FPF_READ_SP);
				read_udp_packets(fip_port);
				break;
			}
			fip_port->fp_flags &= ~FPF_READ_IP;
		}
		else
		{
assert (!offset);	/* This not a valid assertion but udp passes up
			   complete packets only
			 */

			process_udp_data(fip_port, data);
		}
		break;
	default:
		ip_panic(( "illegal state in fip_put_udp_data (%d)",
			fip_port->fp_state ));
	}
	return NW_OK;
}

static void read_udp_packets(fip_port)
fip_port_t *fip_port;
{
	int result;

#if DEBUG & 2
 { where(); printf("read_udp_packets called\n"); }
#endif

	for(;;)
	{
assert (!(fip_port->fp_flags & (FPF_READ_IP|FPF_READ_SP)));
		fip_port->fp_flags |= FPF_READ_IP;
		result= udp_read(fip_port->fp_udpfd, UDP_READ_SIZE);
		if (result == NW_SUSPEND)
		{
			fip_port->fp_flags |= FPF_READ_SP;
			return;
		}
assert (result == NW_OK);
	}
}

static void add_udp_peers(fip_port)
fip_port_t *fip_port;
{
	fip_udp_peer_t *fip_udp_peer;
	int i;

	printf("add_udp_peers called\n");
	fip_port->fp_udp_peerlist= 0;
	i= 0;
	for (i= 0; udp_peers[i].upr_port; i++)
	{
		if (*(ipaddr_t *)udp_peers[i].upr_addr == fip_port->fp_ipaddr)
			continue;
		fip_udp_peer= malloc(sizeof(*fip_udp_peer));
		if (!fip_udp_peer)
			ip_panic(( "malloc failed" ));
		fip_udp_peer->fup_ipaddr= *(ipaddr_t *)udp_peers[i].upr_addr;
		fip_udp_peer->fup_udpport= htons(udp_peers[i].upr_port);
		fip_udp_peer->fup_next= fip_port->fp_udp_peerlist;
		fip_port->fp_udp_peerlist= fip_udp_peer;
	}
}

static void udp_send(fip_port)
fip_port_t *fip_port;
{
	int result;
	udp_io_hdr_t *udp_io_hdr;
	location loc;

	while (fip_port->fp_udp_send_head)
	{
#if DEBUG & 2
 { where(); printf("fip_port->fp_flags= 0x%x\n", fip_port->fp_flags); }
#endif
assert (!(fip_port->fp_flags & (FPF_SEND_IP|FPF_SEND_SP)));

		fip_port->fp_flags |= FPF_SEND_IP;
assert(fip_port->fp_udp_send_head->acc_length >= sizeof(udp_io_hdr_t));
		udp_io_hdr= (udp_io_hdr_t *)ptr2acc_data(fip_port->
			fp_udp_send_head);
		loc.u_long[0]= udp_io_hdr->uih_dst_addr;
		loc.u_long[1]= udp_io_hdr->uih_dst_port;
		if (ff_rcv_credit(fip_port->fp_flipdev, &loc, 1, EV_ABSCREDIT))
			fip_notify(fip_port-fip_port_table);

		result= udp_write(fip_port->fp_udpfd,
			bf_bufsize(fip_port->fp_udp_send_head));
		if (result == NW_SUSPEND)
		{
			fip_port->fp_flags |= FPF_SEND_SP;
			return;
		}
assert (result == NW_OK);
	}
#if DEBUG & 2
 { where(); printf("udp_send done\n"); }
#endif
}

static void process_udp_data(fip_port, data)
fip_port_t *fip_port;
acc_t *data;
{
	udp_io_hdr_t *udphdr;
	pkt_t *pkt;
	acc_t *udp_acc, *hdr_acc, *data_acc;
	size_t pack_size, hdr_size;
	location loc;
	struct flip_hdr	*fliphdr;

#if DEBUG & 2
 { where(); printf("fip_port_table[%d].fp_flipdev= %d\n", 
	fip_port-fip_port_table, fip_port->fp_flipdev); }
#endif
	pack_size= bf_bufsize(data);
	if (pack_size < sizeof(*udphdr)+sizeof(*fliphdr))
	{
#if DEBUG
 { where(); printf("Garbage packet\n"); }
#endif
		bf_afree(data);
		return;
	}
	PKT_GET(pkt, &fip_pool);
	if (!pkt)
	{
		where(); printf("out of packets !!!\n");
		bf_afree(data);
		return;
	}
#if DEBUG & 2
 { where(); printf("got packet %d\n", pkt-packet); }
#endif
	udp_acc= bf_cut(data, 0, sizeof(*udphdr));
	data_acc= data;
	pack_size -= sizeof(*udphdr);
#if DEBUG & 2
 { where(); printf(
		"sizeof(*udphdr)= %d, pack_size= %d, bf_bufsize(data)= %d\n",
			sizeof(*udphdr), pack_size, bf_bufsize(data)); }
#endif
	data= bf_cut(data_acc, sizeof(*udphdr), pack_size);
	bf_afree(data_acc);

	if (pack_size>DIR_HDR_SIZE)
	{
		hdr_size= DIR_HDR_SIZE;
		pack_size -= hdr_size;
#if DEBUG & 2
 { where(); printf("hdr_size= %d, pack_size= %d\n", hdr_size, pack_size); }
#endif
		hdr_acc= bf_cut(data, 0, hdr_size);
		data_acc= bf_cut(data, hdr_size, pack_size);
		bf_afree(data);
#if DEBUG & 2
 { where(); printf("bf_cut done\n"); }
#endif
	}
	else
	{
		hdr_size= pack_size;
		pack_size= 0;
		hdr_acc= data;
		data_acc= 0;
	}

#if DEBUG & 2
 { where(); printf("fip_port_table[%d].fp_flipdev= %d\n", 
	fip_port-fip_port_table, fip_port->fp_flipdev); }
#endif
	proto_setup_input(pkt, short);	/* Smallest alligned value is a
					 * pointer. But for now we need a
					 * misaligned object: short
					 */
	proto_set_dir_size(pkt, 0);
#if DEBUG & 2
 { where(); printf("pc_offset= %d, pc_dirsize= %d, size= %d, pa_size= %d\n",
	(pkt)->p_contents.pc_offset, (pkt)->p_contents.pc_dirsize, hdr_size,
	(pkt)->p_admin.pa_size); }
#endif
	if (data_acc)
	{
		data_acc= bf_pack(data_acc);
assert (!data_acc->acc_next);
		pkt_set_release(pkt, pkt_released, (long)data_acc);
		proto_set_virtual(pkt, 0, 0, (long)ptr2acc_data(data_acc),
			pack_size);
	}
	if (hdr_acc->acc_next)
		hdr_acc= bf_pack(hdr_acc);
#if DEBUG & 2
 { where(); printf("pc_offset= %d, pc_dirsize= %d, size= %d, pa_size= %d\n",
	(pkt)->p_contents.pc_offset, (pkt)->p_contents.pc_dirsize, hdr_size,
	(pkt)->p_admin.pa_size); }
#endif
	proto_dir_append(pkt, ptr2acc_data(hdr_acc), hdr_size);
	PROTO_LOOK_HEADER(fliphdr, pkt, struct flip_hdr);

#if DEBUG & 2
 { where(); printf("fip_port_table[%d].fp_flipdev= %d\n", 
	fip_port-fip_port_table, fip_port->fp_flipdev); }
#endif
	udp_acc= bf_packIffLess(udp_acc, sizeof(*udphdr));
	udphdr= (udp_io_hdr_t *)ptr2acc_data(udp_acc);
	loc.u_long[0]= udphdr->uih_src_addr;
	loc.u_long[1]= udphdr->uih_src_port;

#if DEBUG & 2
 { where(); printf("fip_port_table[%d].fp_flipdev= %d\n", 
	fip_port-fip_port_table, fip_port->fp_flipdev); }
#endif
	bf_afree(hdr_acc);
	bf_afree(udp_acc);


#if DEBUG & 2
 { where(); printf("fip_port_table[%d].fp_flipdev= %d\n", 
	fip_port-fip_port_table, fip_port->fp_flipdev); }
#endif
#if DEBUG & 2
 { where(); printf("calling pktswitch(0x%x, %d, &{0x%x, 0x%x})\n",
	pkt, fip_port->fp_flipdev, loc.u_long[0], loc.u_long[1]); }
#endif
#if DEBUG & 2
 { where(); printf("pc_offset= %d, pc_dirsize= %d, pc_totsize= %d\n",
	(pkt)->p_contents.pc_offset, (pkt)->p_contents.pc_dirsize, 
	(pkt)->p_contents.pc_totsize); }
#endif
#if DEBUG & 2
 { where(); pkt_print(pkt); }
#endif
	pktswitch(pkt, fip_port->fp_flipdev, &loc);
}

static void send_udp_pkt(fip_port, acc, loc)
fip_port_t *fip_port;
acc_t *acc;
location *loc;
{
	udp_io_hdr_t *udphdr;
	fip_udp_peer_t *fip_udp_peer;
	acc_t *header_acc;

	if (loc->u_long[0])
	{
		header_acc= bf_memreq(sizeof(*udphdr));
		udphdr= (udp_io_hdr_t *)ptr2acc_data
			(header_acc);
#if DEBUG & 2
 { where(); printf("sending to 0x%x, 0x%x\n", loc->u_long[0], loc->u_long[1]); }
#endif
		udphdr->uih_dst_addr= loc->u_long[0];
		udphdr->uih_dst_port= loc->u_long[1];
		udphdr->uih_ip_opt_len= 0;
		udphdr->uih_data_len= 0;
assert (!header_acc->acc_next);
		header_acc->acc_next= acc;
		header_acc->acc_ext_link= NULL;
		if (fip_port->fp_udp_send_head)
			fip_port->fp_udp_send_tail->acc_ext_link=
				header_acc;
		else
			fip_port->fp_udp_send_head= header_acc;
		fip_port->fp_udp_send_tail= header_acc;
	}
	else
	{
#if DEBUG & 2
 { where(); printf("doing broadcast\n"); }
#endif
		for (fip_udp_peer= fip_port->fp_udp_peerlist;
			fip_udp_peer; fip_udp_peer= 
			fip_udp_peer->fup_next)
		{
			header_acc= bf_memreq(sizeof(*udphdr));
			udphdr= (udp_io_hdr_t *)ptr2acc_data
				(header_acc);
#if DEBUG & 2
 { where(); printf("broadcast to: 0x%x, 0x%x\n", fip_udp_peer->fup_ipaddr,
fip_udp_peer->fup_udpport); }
#endif
			udphdr->uih_dst_addr=
				fip_udp_peer->fup_ipaddr;
			udphdr->uih_dst_port=
				fip_udp_peer->fup_udpport;
			udphdr->uih_ip_opt_len= 0;
			udphdr->uih_data_len= 0;
assert (!header_acc->acc_next);
			header_acc->acc_next= acc;
			header_acc->acc_ext_link= NULL;
			acc->acc_linkC++;
			if (fip_port->fp_udp_send_head)
				fip_port->fp_udp_send_tail->
					acc_ext_link=
					header_acc;
			else
				fip_port->fp_udp_send_head=
					header_acc;
			fip_port->fp_udp_send_tail= header_acc;
		}
		bf_afree(acc);
	}
	if (fip_port->fp_udp_send_head &&
		!(fip_port->fp_flags & FPF_SEND_IP))
	{
#if DEBUG & 2
 { where(); printf("calling udp_send\n"); }
#endif
		udp_send(fip_port);
	}
}

#endif /* INCLUDE_UDP */

#if INCLUDE_TCP
/************************************************************************
*				TCP code				*
************************************************************************/

static void tcp_timeout(ref, timer)
int ref;
timer_t *timer;
{
	fip_port_t *fip_port;
	fip_tcp_peer_t *fip_tcp_peer;

#if DEBUG & 256
 { where(); printf("tcp_timeout(%d, 0x%x) called\n", ref, timer); }
#endif
assert(ref >= 0 && ref < FIP_PORT_NR);
	fip_port= &fip_port_table[ref];
compare(fip_port->fp_nettype, ==, FPNT_TCP);
	for (fip_tcp_peer= fip_port->fp_tcp_peerlist; fip_tcp_peer; 
		fip_tcp_peer= fip_tcp_peer->ftp_next)
	{
		if (timer == &fip_tcp_peer->ftp_write_timer)
		{
			fip_tcp_peer->ftp_flags &= ~(FTPF_WRITE_IP|
				FTPF_WRITE_SP);
#if DEBUG
 { where(); printf("calling tcp_send\n"); }
#endif
			tcp_send(fip_tcp_peer);
			return;
		}
		if (timer == &fip_tcp_peer->ftp_read_timer)
		{
			fip_tcp_peer->ftp_flags &= ~(FTPF_READ_IP|
				FTPF_READ_SP);
			read_tcp_packets(fip_tcp_peer);
			return;
		}
	}
	ip_panic(( "not reached" ));
}

static void tcp_send(fip_tcp_peer)
fip_tcp_peer_t *fip_tcp_peer;
{
	int result;
	fip_port_t *fip_port;
	location loc;

#if DEBUG & 256
 { where(); printf("tcp_send(&fip_tcp_peer_table[%d]) called\n", fip_tcp_peer-
	fip_tcp_peer_table); stacktrace(); }
#endif

assert(!(fip_tcp_peer->ftp_flags & FTPF_WRITE_IP));
assert(!(fip_tcp_peer->ftp_flags & FTPF_WRITE_SP));
	fip_tcp_peer->ftp_flags |= FTPF_WRITE_IP;

	fip_port= fip_tcp_peer->ftp_fip_port;
assert(fip_port->fp_nettype == FPNT_TCP);
	for (;;)
	{
assert(fip_tcp_peer->ftp_flags & FTPF_WRITE_IP);
assert(!(fip_tcp_peer->ftp_flags & FTPF_WRITE_SP));
		switch(fip_tcp_peer->ftp_write_state)
		{
		case FTPW_EMPTY:
assert(fip_tcp_peer->ftp_write_fd == -1);
			fip_tcp_peer->ftp_write_state= FTPW_SETCONF;
			fip_tcp_peer->ftp_write_fd= tcp_open(fip_port->
				fp_tcpdev, fip_tcp_peer-fip_tcp_peer_table,
				fip_get_tcp_write_data, fip_put_tcp_write_data);
			if (fip_tcp_peer->ftp_write_fd < 0)
			{
				tcp_set_write_timer(fip_tcp_peer);
				return;
			}
#if DEBUG & 256
 { where(); printf("doing NWIOSTCPCONF\n"); }
#endif
			result= tcp_ioctl(fip_tcp_peer->ftp_write_fd, 
				NWIOSTCPCONF);
			if (result == NW_SUSPEND)
			{
				fip_tcp_peer->ftp_flags |= FTPF_WRITE_SP;
				return;
			}
assert(result == NW_OK);
assert(fip_tcp_peer->ftp_write_state= FTPW_CONNECT);
			/* drops through */
		case FTPW_CONNECT:
#if DEBUG & 256
 { where(); printf("doing NWIOTCPCONN\n"); }
#endif
			result= tcp_ioctl(fip_tcp_peer->ftp_write_fd,
				NWIOTCPCONN);
			if (result == NW_SUSPEND)
			{
				fip_tcp_peer->ftp_flags |= FTPF_WRITE_SP;
				return;
			}
			if (fip_tcp_peer->ftp_write_state == FTPW_TIMER)
				return;
assert (result == NW_OK);
assert(fip_tcp_peer->ftp_write_state == FTPW_WRITE);
			/* drops through */
		case FTPW_WRITE:
			if (!fip_tcp_peer->ftp_pack && 
						!fip_tcp_peer->ftp_broad_pack)
			{
				if (fip_tcp_peer->ftp_write_head)
				{
					fip_tcp_peer->ftp_pack= fip_tcp_peer->
						ftp_write_head;
					fip_tcp_peer->ftp_write_head= 
						fip_tcp_peer->ftp_write_head->
						acc_ext_link;
					if (!fip_tcp_peer->ftp_write_head)
					{
						loc.u_long[0]= fip_tcp_peer->
							ftp_ipaddr;
						loc.u_long[1]= fip_tcp_peer->
							ftp_lport;
assert(loc.u_long[0] && loc.u_long[1]);
#if DEBUG & 256
 { where(); printf("receiving credit for 0x%x, 0x%x\n", ntohl(loc.u_long[0]),
	ntohl(loc.u_long[1])); }
#endif
						if (ff_rcv_credit(fip_port->
							fp_flipdev, &loc, 1, 
							EV_ABSCREDIT))
							fip_notify(fip_port-
								fip_port_table);
					}
				}
				if (fip_tcp_peer->ftp_write_broad_head)
				{
					fip_tcp_peer->ftp_broad_pack=
						fip_tcp_peer->
						ftp_write_broad_head;
					fip_tcp_peer->ftp_write_broad_head=
						fip_tcp_peer->
						ftp_write_broad_head->
						acc_ext_link;
				}
				if (!fip_tcp_peer->ftp_pack && 
					!fip_tcp_peer->ftp_broad_pack)
				{
					fip_tcp_peer->ftp_flags &= 
						~FTPF_WRITE_IP;
					return;
				}
			}
			if (!fip_tcp_peer->ftp_pack)
			{
				fip_tcp_peer->ftp_pack= fip_tcp_peer->
					ftp_broad_pack;
				fip_tcp_peer->ftp_broad_pack= NULL;
			}
assert(fip_tcp_peer->ftp_pack);
assert(!(fip_tcp_peer->ftp_flags & FTPF_WRITE_SP));
#if DEBUG & 256
 { where(); printf("calling tcp_write\n"); }
#endif
			result= tcp_write(fip_tcp_peer->ftp_write_fd, 
				bf_bufsize(fip_tcp_peer->ftp_pack));
			if (result == NW_SUSPEND)
			{
				fip_tcp_peer->ftp_flags |= FTPF_WRITE_SP;
				return;
			}
			if (fip_tcp_peer->ftp_write_state == FTPW_TIMER)
				return;
assert(result == NW_OK);
assert(fip_tcp_peer->ftp_flags & FTPF_WRITE_IP);
assert(!(fip_tcp_peer->ftp_flags & FTPF_WRITE_SP));
assert(fip_tcp_peer->ftp_write_state == FTPW_WRITE);
			continue;
		case FTPW_TIMER:
			fip_tcp_peer->ftp_write_state= FTPW_EMPTY;
			continue;
		default:
			ip_panic(( "unknown state (%d)", 
				fip_tcp_peer->ftp_write_state ));
		}
	}
}

static void read_tcp_packets(fip_tcp_peer)
fip_tcp_peer_t *fip_tcp_peer;
{
	fip_port_t *fip_port;
	int result;
	
/*	where(); printf("not doing read_tcp_packets\n"); return; */
#if DEBUG & 256
 { where(); printf("read_tcp_packets(&fip_tcp_peer_table[%d]) called\n",
	fip_tcp_peer-fip_tcp_peer_table); }
#endif

assert (!(fip_tcp_peer->ftp_flags & FTPF_NOPACK));

	fip_port= fip_tcp_peer->ftp_fip_port;
assert(fip_port->fp_nettype == FPNT_TCP);
	
	for (;;)
	{
assert (!(fip_tcp_peer->ftp_flags & FTPF_READ_IP));
assert (!(fip_tcp_peer->ftp_flags & FTPF_READ_SP));
	fip_tcp_peer->ftp_flags |= FTPF_READ_IP;

assert (!(fip_tcp_peer->ftp_flags & FTPF_READ_SP));
		switch(fip_tcp_peer->ftp_read_state)
		{
		case FTPR_EMPTY:
assert(fip_tcp_peer->ftp_read_fd == -1);
			fip_tcp_peer->ftp_read_state= FTPR_SETCONF;
			fip_tcp_peer->ftp_read_fd= tcp_open(fip_port->fp_tcpdev,
				fip_tcp_peer-fip_tcp_peer_table,
				fip_get_tcp_read_data, fip_put_tcp_read_data);
			if (fip_tcp_peer->ftp_read_fd < 0)
			{
				tcp_set_read_timer(fip_tcp_peer);
				return;
			}
			result= tcp_ioctl(fip_tcp_peer->ftp_read_fd, 
				NWIOSTCPCONF);
			if (result == NW_SUSPEND)
			{
				fip_tcp_peer->ftp_flags |= FTPF_READ_SP;
				return;
			}
assert(result == NW_OK);
assert(fip_tcp_peer->ftp_read_state= FTPR_LISTEN);
			/* drops through */
		case FTPR_LISTEN:
#if DEBUG & 256
 { where(); printf("doing NWIOTCPLISTEN\n"); }
#endif
			result= tcp_ioctl(fip_tcp_peer->ftp_read_fd, 
				NWIOTCPLISTEN);
			if (result == NW_SUSPEND)
			{
				fip_tcp_peer->ftp_flags |= FTPF_READ_SP;
				return;
			}
			if (fip_tcp_peer->ftp_read_state == FTPR_TIMER)
				return;
assert(result == NW_OK);
assert(fip_tcp_peer->ftp_read_state == FTPR_READ);
			/* drops through */
		case FTPR_READ:
assert (!(fip_tcp_peer->ftp_flags & FTPF_NOPACK));
			result= tcp_read(fip_tcp_peer->ftp_read_fd,
				TCP_READ_SIZE);
			if (result == NW_SUSPEND)
			{
				fip_tcp_peer->ftp_flags |= FTPF_READ_SP;
				return;
			}
			if (fip_tcp_peer->ftp_flags & FTPF_NOPACK)
				return;
			if (fip_tcp_peer->ftp_read_state == FTPR_TIMER)
				return;
assert(result == NW_OK);
assert(fip_tcp_peer->ftp_read_state == FTPR_READ);
			continue;
		case FTPR_TIMER:
			fip_tcp_peer->ftp_read_state= FTPR_EMPTY;
assert (!(fip_tcp_peer->ftp_flags & FTPF_READ_SP));
			fip_tcp_peer->ftp_flags &= ~FTPF_READ_IP;
			continue;
		default:
			ip_panic(( "unknown state (%d)",
				fip_tcp_peer->ftp_read_state ));
		}
	}
}

/*
send_tcp_pkt
*/

static void send_tcp_pkt(fip_port, acc, loc)
fip_port_t *fip_port;
acc_t *acc;
location *loc;
{
	fip_tcp_peer_t *fip_tcp_peer;
	ipaddr_t ipaddr;
	tcpport_t lport;
	size_t acc_size;
	acc_t *tmp_acc;

#if DEBUG & 256
 { where(); printf("send_tcp_pkt called\n"); }
#endif
	if (loc->u_long[0])
	{
#if DEBUG & 256
 { where(); printf("sending to 0x%x, 0x%x\n", loc->u_long[0], loc->u_long[1]); }
#endif
		ipaddr= loc->u_long[0];
		lport= loc->u_long[1];

		for (fip_tcp_peer= fip_port->fp_tcp_peerlist; fip_tcp_peer;
			fip_tcp_peer= fip_tcp_peer->ftp_next)
		{
			if (fip_tcp_peer->ftp_ipaddr == ipaddr && 
				fip_tcp_peer->ftp_lport == lport)
				break;
		}
		if (!fip_tcp_peer)
		{
#if DEBUG
 { where(); printf("got invalid location: 0x%x, 0x%x\n", loc->u_long[0],
	loc->u_long[1]); }
#endif
			bf_afree(acc);
			return;
		}
		acc->acc_ext_link= NULL;
		if (fip_tcp_peer->ftp_write_head)
			fip_tcp_peer->ftp_write_tail->acc_ext_link= acc;
		else
			fip_tcp_peer->ftp_write_head= acc;
		fip_tcp_peer->ftp_write_tail= acc;
		while (!(fip_tcp_peer->ftp_flags & FTPF_WRITE_IP) &&
			fip_tcp_peer->ftp_write_head)
		{
#if DEBUG & 256
 { where(); printf("calling tcp_send\n"); }
#endif
			tcp_send(fip_tcp_peer);
		}
#if DEBUG & 256
 if (!fip_tcp_peer->ftp_write_head)
 { where(); printf("end of loop\n"); 
	assert(!fip_tcp_peer->ftp_write_broad_head); }
#endif
	}
	else
	{
#if DEBUG & 256
 { where(); printf("doing broadcast\n"); }
#endif
		acc_size= bf_bufsize(acc);
		for (fip_tcp_peer= fip_port->fp_tcp_peerlist; fip_tcp_peer;
			fip_tcp_peer= fip_tcp_peer->ftp_next)
		{
			tmp_acc= bf_cut(acc, 0, acc_size);
			tmp_acc->acc_ext_link= NULL;
			if (fip_tcp_peer->ftp_write_broad_head)
				fip_tcp_peer->ftp_write_broad_tail->
					acc_ext_link= tmp_acc;
			else
				fip_tcp_peer->ftp_write_broad_head= tmp_acc;
			fip_tcp_peer->ftp_write_broad_tail= tmp_acc;
			while (!(fip_tcp_peer->ftp_flags & FTPF_WRITE_IP) &&
				fip_tcp_peer->ftp_write_broad_head)
			{
#if DEBUG & 256
 { where(); printf("calling tcp_send\n"); }
#endif
				tcp_send(fip_tcp_peer);
			}
		}
		bf_afree(acc);
#if 0	/* no credit for broadcasts */
		if (ff_rcv_credit(fip_port->fp_flipdev, loc, 1, EV_ABSCREDIT))
			fip_notify(fip_port-fip_port_table);
#endif
	}
}

static acc_t *fip_get_tcp_main_data(fd, offset, count, for_ioctl)
int fd;
size_t offset;
size_t count;
int for_ioctl;
{
	ip_panic(( "fip_get_tcp_main_data called" ));
}

static int fip_put_tcp_main_data(fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	int result;
	fip_port_t *fip_port;

assert (fd >= 0 && fd < FIP_PORT_NR);
	fip_port= &fip_port_table[fd];
assert (fip_port->fp_nettype == FPNT_TCP);

	switch(fip_port->fp_state)
	{
	case FPS_TCP_GETCONF:
		if (!data)
		{
			result= (int)offset;
			if (result < 0)
			{
				ip_warning(( "error %d", result ));
				fip_port->fp_state= FPS_TCP_ERROR;
				break;
			}
			fip_port->fp_state= FPS_TCP_MAIN;
#if DEBUG & 256
 { where(); printf("calling fip_main\n"); }
#endif
			if (fip_port->fp_flags & FPF_SUSPEND)
			{
				fip_port->fp_flags &= ~FPF_SUSPEND;
				fip_main(fip_port);
			}
#if DEBUG & 256
 { where(); printf("not calling fip_main\n"); }
#endif
		}
		else
		{
			struct nwio_tcpconf *tcpconf;

			data= bf_packIffLess(data, sizeof(*tcpconf));
			tcpconf= (struct nwio_tcpconf *)ptr2acc_data(data);
			fip_port->fp_ipaddr= tcpconf->nwtc_locaddr;
#if DEBUG & 256
 { where(); printf("got ip address: "); writeIpAddr(fip_port->fp_ipaddr); 
	printf("\n"); }
#endif
			bf_afree(data);
		}
		break;
	default:
		ip_panic(( "illegal state in fip_put_tcp_main_data (%d)", 
			fip_port->fp_state ));
	}
	return NW_OK;
}

static acc_t *fip_get_tcp_write_data(fd, offset, count, for_ioctl)
int fd;
size_t offset;
size_t count;
int for_ioctl;
{
	int result;
	fip_tcp_peer_t *fip_tcp_peer;

#if DEBUG & 256
 { where(); printf("fip_get_tcp_write_data(%d, %d, %d, %d) called\n",
	fd, offset, count, for_ioctl); }
#endif

assert(fd >= 0 && fd < TCP_PEER_NR);
	fip_tcp_peer= &fip_tcp_peer_table[fd];

	switch(fip_tcp_peer->ftp_write_state)
	{
	case FTPW_SETCONF:
		if (!count)
		{
			result= (int)offset;
			if (result < 0)
			{
				ip_warning(( "NWIOSTCPCONF failed (%d)", 
								result ));
				tcp_set_write_timer(fip_tcp_peer);
				break;
			}
			fip_tcp_peer->ftp_write_state= FTPW_CONNECT;
			if(fip_tcp_peer->ftp_flags & FTPF_WRITE_SP)
			{
				fip_tcp_peer->ftp_flags &= ~FTPF_WRITE_SP;
#if DEBUG
 { where(); printf("calling tcp_send\n"); }
#endif
				tcp_send(fip_tcp_peer);
			}
			break;
		}
		else
		{
			nwio_tcpconf_t *tcpconf;
			acc_t *acc;

assert(!offset);
assert(count == sizeof(*tcpconf));

			acc= bf_memreq(sizeof(*tcpconf));
			tcpconf= (nwio_tcpconf_t *)ptr2acc_data(acc);
			tcpconf->nwtc_flags= NWTC_COPY | NWTC_LP_SET | 
				NWTC_SET_RA | NWTC_SET_RP;
			tcpconf->nwtc_remaddr= fip_tcp_peer->ftp_ipaddr;
			tcpconf->nwtc_locport= htons(TCPPORT_CONNECT_FLIP);
			tcpconf->nwtc_remport= fip_tcp_peer->ftp_lport;
			return acc;
		}
	case FTPW_CONNECT:
		if (!count)
		{
			result= (int)offset;
			if (result < 0)
			{
				ip_warning(( "NWIOTCPCONN failed (%d)", 
								result ));
				tcp_set_write_timer(fip_tcp_peer);
				break;
			}
			fip_tcp_peer->ftp_write_state= FTPW_WRITE;
			if(fip_tcp_peer->ftp_flags & FTPF_WRITE_SP)
			{
				fip_tcp_peer->ftp_flags &= ~(FTPF_WRITE_IP|
					FTPF_WRITE_SP);
#if DEBUG
 { where(); printf("calling tcp_send\n"); }
#endif
				tcp_send(fip_tcp_peer);
			}
			break;
		}
		else
		{
			nwio_tcpcl_t *tcpconnopt;
			acc_t *acc;

assert(!offset);
assert(count == sizeof(*tcpconnopt));

			acc= bf_memreq(sizeof(*tcpconnopt));
			tcpconnopt= (nwio_tcpcl_t *)ptr2acc_data(acc);
			tcpconnopt->nwtcl_flags= 0;
			return acc;
		}
	case FTPW_WRITE:
		if (!count)
		{
			acc_t *tmp_acc;

			tmp_acc= fip_tcp_peer->ftp_pack;
			result= (int)offset;
			if (result <= 0)
			{
				ip_warning(( "tcp_write failed (%d)", result ));
				fip_tcp_peer->ftp_pack= NULL;
				bf_afree(tmp_acc);
				tcp_set_write_timer(fip_tcp_peer);
				break;
			}
			if (result == bf_bufsize(tmp_acc))
			{
				fip_tcp_peer->ftp_pack= NULL;
				bf_afree(tmp_acc);
			}
			else
			{
				fip_tcp_peer->ftp_pack= bf_cut(tmp_acc, 0,
					result);
				bf_afree(tmp_acc);
			}
			if(fip_tcp_peer->ftp_flags & FTPF_WRITE_SP)
			{
				fip_tcp_peer->ftp_flags &= ~(FTPF_WRITE_IP|
					FTPF_WRITE_SP);
#if DEBUG & 256
 { where(); printf("calling tcp_send\n"); }
#endif
				tcp_send(fip_tcp_peer);
				break;
			}
#if DEBUG & 256
			else
 { where(); printf("not calling tcp_send\n"); }
#endif
assert(!(fip_tcp_peer->ftp_flags & FTPF_WRITE_SP));
			break;
		}
		else
			return bf_cut(fip_tcp_peer->ftp_pack, offset, count);
	default:
		ip_panic(( "illegal state in fip_get_tcp_write_data (%d)",
			fip_tcp_peer->ftp_write_state ));
	}
	return NULL;
}

static int fip_put_tcp_write_data(fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	ip_panic(( "fip_put_tcp_write_data called" ));
}

static acc_t *fip_get_tcp_read_data(fd, offset, count, for_ioctl)
int fd;
size_t offset;
size_t count;
int for_ioctl;
{
	int result;
	fip_tcp_peer_t *fip_tcp_peer;

#if DEBUG & 256
 { where(); printf("fip_get_tcp_read_data(%d, %d, %d, %d) called\n",
	fd, offset, count, for_ioctl); }
#endif

assert(fd >= 0 && fd < TCP_PEER_NR);
	fip_tcp_peer= &fip_tcp_peer_table[fd];

#if DEBUG & 256
 { where(); printf("ftp_read_state= %d\n", fip_tcp_peer->ftp_read_state); }
#endif
	switch(fip_tcp_peer->ftp_read_state)
	{
	case FTPR_SETCONF:
		if (!count)
		{
			result= (int)offset;
			if (result < 0)
			{
				ip_warning(( "NWIOSTCPCONF failed (%d)", 
								result ));
				tcp_set_read_timer(fip_tcp_peer);
				break;
			}
			fip_tcp_peer->ftp_read_state= FTPR_LISTEN;
			if(fip_tcp_peer->ftp_flags & FTPF_READ_SP)
			{
				fip_tcp_peer->ftp_flags &= ~FTPF_READ_SP;
				read_tcp_packets(fip_tcp_peer);
			}
			break;
		}
		else
		{
			nwio_tcpconf_t *tcpconf;
			acc_t *acc;

assert(!offset);
assert(count == sizeof(*tcpconf));

			acc= bf_memreq(sizeof(*tcpconf));
			tcpconf= (nwio_tcpconf_t *)ptr2acc_data(acc);
			tcpconf->nwtc_flags= NWTC_COPY | NWTC_LP_SET | 
				NWTC_SET_RA | NWTC_SET_RP;
			tcpconf->nwtc_remaddr= fip_tcp_peer->ftp_ipaddr;
			tcpconf->nwtc_locport= htons(TCPPORT_LISTEN_FLIP);
			tcpconf->nwtc_remport= fip_tcp_peer->ftp_cport;
			return acc;
		}
	case FTPR_LISTEN:
		if (!count)
		{
			result= (int)offset;
			if (result < 0)
			{
				ip_warning(( "NWIOTCPLISTEN failed (%d)", 
					result ));
				tcp_set_read_timer(fip_tcp_peer);
				break;
			}
			fip_tcp_peer->ftp_read_state= FTPR_READ;
			if(fip_tcp_peer->ftp_flags & FTPF_READ_SP)
			{
				fip_tcp_peer->ftp_flags &= ~(FTPF_READ_IP|
					FTPF_READ_SP);
				read_tcp_packets(fip_tcp_peer);
			}
			break;
		}
		else
		{
			nwio_tcpcl_t *tcplistenopt;
			acc_t *acc;

assert(!offset);
assert(count == sizeof(*tcplistenopt));

			acc= bf_memreq(sizeof(*tcplistenopt));
			tcplistenopt= (nwio_tcpcl_t *)ptr2acc_data(acc);
			tcplistenopt->nwtcl_flags= 0;
			return acc;
		}
	default:
		ip_panic(( "illegal state in fip_get_tcp_read_data (%d)",
			fip_tcp_peer->ftp_read_state ));
	}
	return NULL;
}

static int fip_put_tcp_read_data(fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	int result;
	fip_tcp_peer_t *fip_tcp_peer;
	int suspended;

assert(fd >= 0 && fd < TCP_PEER_NR);
	fip_tcp_peer= &fip_tcp_peer_table[fd];

	switch(fip_tcp_peer->ftp_read_state)
	{
	case FTPR_READ:
		if (!data)
		{
			acc_t *packet;

			result= (int)offset;
			if (result <= 0)	/* EOF is also an error */
			{
assert (!fip_tcp_peer->ftp_read_tail);
				if (fip_tcp_peer->ftp_read_head)
				{
					bf_afree(fip_tcp_peer->ftp_read_head);
					fip_tcp_peer->ftp_read_head= NULL;
				}
				ip_warning(( "tcp_read failed (%d)", result ));
				tcp_set_read_timer(fip_tcp_peer);
				break;
			}
assert(fip_tcp_peer->ftp_read_tail);
assert(offset == bf_bufsize(fip_tcp_peer->ftp_read_tail));
			assert(fip_tcp_peer->ftp_flags & FTPF_READ_IP);
			suspended= !!(fip_tcp_peer->ftp_flags & FTPF_READ_SP);
			fip_tcp_peer->ftp_flags &= ~(FTPF_READ_IP|FTPF_READ_SP);

			if (fip_tcp_peer->ftp_read_head)
				packet= bf_append(fip_tcp_peer->ftp_read_head,
					fip_tcp_peer->ftp_read_tail);
			else
				packet= fip_tcp_peer->ftp_read_tail;
			fip_tcp_peer->ftp_read_tail= NULL;
			fip_tcp_peer->ftp_read_head= process_tcp_data(
				fip_tcp_peer, packet);
			if (suspended)
			{
#if NEW_FLOW_CODE
				if (!(fip_tcp_peer->ftp_flags & FTPF_NOPACK))
					read_tcp_packets(fip_tcp_peer);
#else /* !NEW_FLOW_CODE */
				read_tcp_packets(fip_tcp_peer);
#endif /* NEW_FLOW_CODE */
			}
			break;
		}
		if (!fip_tcp_peer->ftp_read_tail)
		{
assert(!offset);
			fip_tcp_peer->ftp_read_tail= data;
			break;
		}
assert(offset= bf_bufsize(fip_tcp_peer->ftp_read_tail));
		fip_tcp_peer->ftp_read_tail= bf_append(fip_tcp_peer->
			ftp_read_tail, data);
		break;
	default:
		ip_panic(( "illegal state in fip_put_tcp_read_data (%d)",
			fip_tcp_peer->ftp_read_state ));
	}
	return NW_OK;
}

static void tcp_set_write_timer(fip_tcp_peer)
fip_tcp_peer_t *fip_tcp_peer;
{
	fip_port_t *fip_port;

#if DEBUG & 256
 { where(); printf("set_write_timer(&fip_tcp_peer_table[%d]) called\n",
	fip_tcp_peer-fip_tcp_peer_table); stacktrace(); }
#endif
	fip_port= fip_tcp_peer->ftp_fip_port;

	if (fip_tcp_peer->ftp_write_fd != -1)
		tcp_close(fip_tcp_peer->ftp_write_fd);
	fip_tcp_peer->ftp_write_fd= -1;
	clck_timer(&fip_tcp_peer->ftp_write_timer, get_time() + TCP_TIMOUT,
		tcp_timeout, fip_port-fip_port_table);
	fip_tcp_peer->ftp_write_state= FTPW_TIMER;
	fip_tcp_peer->ftp_flags |= FTPF_WRITE_SP;
}

static void tcp_set_read_timer(fip_tcp_peer)
fip_tcp_peer_t *fip_tcp_peer;
{
	fip_port_t *fip_port;

#if DEBUG & 2
 { where(); printf("set_read_timer(&fip_tcp_peer_table[%d]) called\n",
	fip_tcp_peer-fip_tcp_peer_table); stacktrace(); }
#endif
	fip_port= fip_tcp_peer->ftp_fip_port;

	if (fip_tcp_peer->ftp_read_fd != -1)
		tcp_close(fip_tcp_peer->ftp_read_fd);
	fip_tcp_peer->ftp_read_fd= -1;
	clck_timer(&fip_tcp_peer->ftp_read_timer, get_time() + TCP_TIMOUT,
		tcp_timeout, fip_port-fip_port_table);
	fip_tcp_peer->ftp_read_state= FTPR_TIMER;
	fip_tcp_peer->ftp_flags |= FTPF_READ_SP;
}

static acc_t *process_tcp_data(fip_tcp_peer, packet)
fip_tcp_peer_t *fip_tcp_peer;
acc_t *packet;
{
	flip_hdr *fh;
	size_t bufsize, pack_size, hdr_size;
	acc_t *flip_pack, *tmp_acc, *hdr_acc, *data_acc;
	pkt_t *pkt;
	location loc;

	while (packet)
	{
		bufsize= bf_bufsize(packet);
		if (bufsize < sizeof(*fh))
			return packet;

#if ALIGN_HEADERS
		packet= bf_align(packet, sizeof(flip_hdr), 4);
#endif

		packet= bf_packIffLess(packet, sizeof(*fh));
		fh= (flip_hdr *)ptr2acc_data(packet);
#if ALIGN_HEADERS
		assert(((unsigned)fh & 3) == 0);
#endif

		fl_orderhdr(fh);
		pack_size= fh->fh_length + sizeof(*fh);
#if DEBUG & 256
 { where(); printf("pack_size= %d\n", pack_size); }
#endif
assert(pack_size>= sizeof(*fh));
		if (bufsize < pack_size)
			return packet;

#if NEW_FLOW_CODE
		PKT_GET(pkt, &fip_pool);
		if (!pkt)
		{
#if DEBUG & 256
 { where(); printf("out of packets !!!\n"); }
#endif
			fip_tcp_peer->ftp_flags |= FTPF_NOPACK;
assert(!(fip_tcp_peer->ftp_flags & FTPF_READ_IP));
			tcp_restart_process= 1;
			pkt_incwanted(&fip_pool, 1);
			return packet;
		}
#endif /* NEW_FLOW_CODE */

		if (bufsize == pack_size)
		{
			flip_pack= packet;
			packet= NULL;
		}
		else
		{
			flip_pack= bf_cut(packet, 0, pack_size);
			tmp_acc= bf_cut(packet, pack_size, bufsize-pack_size);
			bf_afree(packet);
			packet= tmp_acc;
		}

#if !NEW_FLOW_CODE
		PKT_GET(pkt, &fip_pool);
		if (!pkt)
		{
			where(); printf("out of packets !!!\n");
			bf_afree(flip_pack);
			continue;
		}
#endif /* !NEW_FLOW_CODE */

		if (pack_size>DIR_HDR_SIZE)
		{
			hdr_size= DIR_HDR_SIZE;
			pack_size -= hdr_size;
			hdr_acc= bf_cut(flip_pack, 0, hdr_size);
			data_acc= bf_cut(flip_pack, hdr_size, pack_size);
			bf_afree(flip_pack);
		}
		else
		{
			hdr_size= pack_size;
			pack_size= 0;
			hdr_acc= flip_pack;
			data_acc= 0;
		}

		proto_setup_input(pkt, short);	/* Smallest alligned value is a
						 * pointer. But for now we 
						 * need a misaligned object:
						 * short
						 */
		proto_set_dir_size(pkt, 0);
		if (data_acc)
		{
/* ip_panic(__FILE__, __LINE__, "no virtual data expected"); */
			data_acc= bf_pack(data_acc);
assert (!data_acc->acc_next);
			pkt_set_release(pkt, pkt_released, (long)data_acc);
			proto_set_virtual(pkt, 0, 0, 
				(long)ptr2acc_data(data_acc), pack_size);
		}
		if (hdr_acc->acc_next)
			hdr_acc= bf_pack(hdr_acc);
		proto_dir_append(pkt, ptr2acc_data(hdr_acc), hdr_size);
		PROTO_LOOK_HEADER(fh, pkt, struct flip_hdr);
assert(fh);

		loc.u_long[0]= fip_tcp_peer->ftp_ipaddr;;
		loc.u_long[1]= fip_tcp_peer->ftp_lport;

		bf_afree(hdr_acc);

#if DEBUG & 256
 { where(); printf("hdr_size= %d, pack_size= %d (%d)\n", hdr_size, 
	pack_size, hdr_size+pack_size); }
#endif
		pktswitch(pkt, fip_tcp_peer->ftp_fip_port->fp_flipdev, &loc);
	}
	return packet;
}


void fip_add_tcp_peer(tcpdev, host, listen_port, connect_port)
int tcpdev;
ipaddr_t host;
tcpport_t listen_port, connect_port;
{
	fip_port_t *fip_port;
	int i;

#if DEBUG & 3
 { where(); printf("adding { "); writeIpAddr(host); printf(", 0x%x, 0x%x }\n",
	ntohs(listen_port), ntohs(connect_port)); }
#endif

	for (i= 0, fip_port= fip_port_table; i< FIP_PORT_NR; i++, fip_port++)
	{
		if (fip_port->fp_nettype != FPNT_TCP)
			continue;
		if (!tcpdev)
			break;
		tcpdev--;
	}
assert (!tcpdev);
	add_peer_tcp(fip_port, host, listen_port, connect_port);
}

static void add_peer_tcp(fip_port, host, listen_port, connect_port)
fip_port_t *fip_port;
ipaddr_t host;
tcpport_t listen_port, connect_port;
{
	int j;
	fip_tcp_peer_t *fip_tcp_peer;

	if (host == fip_port->fp_ipaddr)
		return;

	for (j= 0, fip_tcp_peer= fip_tcp_peer_table; j<TCP_PEER_NR &&
		fip_tcp_peer->ftp_flags; j++, fip_tcp_peer++)
	if (j == TCP_PEER_NR)
		ip_panic(( "out of peers" ));
	fip_tcp_peer->ftp_fip_port= fip_port;
	fip_tcp_peer->ftp_ipaddr= host;
	fip_tcp_peer->ftp_lport= listen_port;
	fip_tcp_peer->ftp_cport= connect_port;
	fip_tcp_peer->ftp_read_state= FTPR_EMPTY;
	fip_tcp_peer->ftp_write_state= FTPW_EMPTY;
	fip_tcp_peer->ftp_read_fd= -1;
	fip_tcp_peer->ftp_write_fd= -1;
	fip_tcp_peer->ftp_write_head= NULL;
	fip_tcp_peer->ftp_write_tail= NULL;
	fip_tcp_peer->ftp_write_broad_head= NULL;
	fip_tcp_peer->ftp_write_broad_tail= NULL;
	fip_tcp_peer->ftp_read_head= NULL;
	fip_tcp_peer->ftp_read_tail= NULL;
	fip_tcp_peer->ftp_pack= NULL;
	fip_tcp_peer->ftp_broad_pack= NULL;
	fip_tcp_peer->ftp_flags= FTPF_INUSE;
	fip_tcp_peer->ftp_next= fip_port->fp_tcp_peerlist;
	fip_port->fp_tcp_peerlist= fip_tcp_peer;
	read_tcp_packets(fip_tcp_peer);
}
	
#endif /* INCLUDE_TCP */

static void fip_notify(dev)
int dev;
{
	fip_port_t *fip_port;
	pkt_t *p;
	location l;
	location *loc_hdr;

assert (dev>=0 && dev<FIP_PORT_NR);
	fip_port= &fip_port_table[dev];

#if DEBUG & 256
 { where(); printf("fip_notify called\n"); }
#endif

	while(ff_get(fip_port->fp_flipdev, &l, &p, NULL))
	{
#if DEBUG & 256
 { flip_hdr *fh; where(); proto_cur_header(fh, p, flip_hdr);
printf("fleth_send: offset = %d, mid = %d\n",
fh->fh_offset, fh->fh_messid); }
#endif
		proto_fix_header(p);
		PROTO_ADD_HEADER(loc_hdr, p, location);
		*loc_hdr= l;
		proto_fix_header(p);

		p->p_admin.pa_next= NULL;

		if (fip_port->fp_pkt_head)
			fip_port->fp_pkt_tail->p_admin.pa_next= p;
		else
			fip_port->fp_pkt_head= p;
		fip_port->fp_pkt_tail= p;
	}
	sema_up(&send_sema);
}

static void fip_request(dev, loc, pkt)
int dev;
location *loc;
pkt_t *pkt;
{
	fip_port_t *fip_port;

#if DEBUG
 { where(); printf("fip_request(%d, { 0x%x, 0x%x }, pkt) called\n", dev,
	ntohl(loc->u_long[0]), ntohl(loc->u_long[1])); }
#endif

assert (dev >= 0 && dev < FIP_PORT_NR);

	fip_port= &fip_port_table[dev];

	pkt_discard(pkt);

	switch(fip_port->fp_nettype)
	{
	case FPNT_TCP:
	{
		fip_tcp_peer_t *fip_tcp_peer;
		ipaddr_t ipaddr;
		tcpport_t lport;

		ipaddr= loc->u_long[0];
		lport= loc->u_long[1];

                for (fip_tcp_peer= fip_port->fp_tcp_peerlist; fip_tcp_peer;
			fip_tcp_peer= fip_tcp_peer->ftp_next)
		{
			if (fip_tcp_peer->ftp_ipaddr == ipaddr &&
				fip_tcp_peer->ftp_lport == lport)
				break;
		}
		if (!fip_tcp_peer)
		{
#if DEBUG
 { where(); printf("got invalid location: 0x%x, 0x%x\n", loc->u_long[0],
   loc->u_long[1]); }
#endif
			assert(0);	/* Should not happen, without a
					 * delete address option implemented
					 */
			return;
		}
	 	if (fip_tcp_peer->ftp_write_fd == -1)
		{
#if DEBUG
 { where(); printf("replying not connection\n"); }
#endif
			return;
		}

#if DEBUG
if (fip_tcp_peer->ftp_flags & (FTPF_WRITE_IP | FTPF_WRITE_SP) == 
	(FTPF_WRITE_IP | FTPF_WRITE_SP))
	fip_tcp_peer->ftp_n_req_credit= 0;
else if (fip_tcp_peer->ftp_n_req_credit++ >= 3)
 { where(); printf("fip_tcp_peer->ftp_n_req_credit= %d\n", 
			fip_tcp_peer->ftp_n_req_credit);
compare (fip_tcp_peer->ftp_flags & (FTPF_WRITE_IP | FTPF_WRITE_SP), ==, 
	(FTPF_WRITE_IP | FTPF_WRITE_SP) ); }
#endif

		if (ff_rcv_credit(fip_port->fp_flipdev, loc, 0, EV_ABSCREDIT))
			fip_notify(fip_port-fip_port_table);
		break;
	}
	default:
		ip_panic(( "not reached" ));
	}
}

static void fip_send(dev, pkt, loc)
int dev;
pkt_t *pkt;
location *loc;
{
	fip_port_t *fip_port;
	location *loc_hdr;
	location l;
	pkt_t *p;

#if DEBUG & 256
 if (loc->u_long[0])
 { where(); printf("fip_send(%d, ..., &{0x%x, 0x%x}): \n", dev, loc->u_long[0],
	loc->u_long[1]); pkt_print(pkt); }
#endif

assert (dev>=0 && dev<FIP_PORT_NR);
	fip_port= &fip_port_table[dev];

	if (!loc->u_long[0] && !loc->u_long[1])	/* broadcast */
	{
		proto_fix_header(pkt);
		PROTO_ADD_HEADER(loc_hdr, pkt, location);
		*loc_hdr= *loc;
		proto_fix_header(pkt);

		pkt->p_admin.pa_next= NULL;

		if (fip_port->fp_pkt_head)
			fip_port->fp_pkt_tail->p_admin.pa_next= pkt;
		else
			fip_port->fp_pkt_head= pkt;
		fip_port->fp_pkt_tail= pkt;
		sema_up(&send_sema);
		return;
	}

	if(ff_store(fip_port->fp_flipdev, pkt, loc, NULL))
	{
#if DEBUG & 256
 { flip_hdr *fh; where(); proto_cur_header(fh, pkt, flip_hdr);
	printf("fleth_send: offset = %d, mid = %d, length %d, total %d\n",
		fh->fh_offset, fh->fh_messid, fh->fh_length, fh->fh_total); }
#endif
		proto_fix_header(pkt);
		PROTO_ADD_HEADER(loc_hdr, pkt, location);
		*loc_hdr= *loc;
		proto_fix_header(pkt);

		pkt->p_admin.pa_next= NULL;

		if (fip_port->fp_pkt_head)
			fip_port->fp_pkt_tail->p_admin.pa_next= pkt;
		else
			fip_port->fp_pkt_head= pkt;
		fip_port->fp_pkt_tail= pkt;
        }
	else
	{
		while(ff_get(fip_port->fp_flipdev, &l, &p, NULL))
		{
#if DEBUG
 { flip_hdr *fh; where(); proto_cur_header(fh, p, flip_hdr);
	printf("fleth_send: offset = %d, mid = %d\n",
	fh->fh_offset, fh->fh_messid); }
#endif
			proto_fix_header(p);
			PROTO_ADD_HEADER(loc_hdr, p, location);
			*loc_hdr= l;
			proto_fix_header(p);

			p->p_admin.pa_next= NULL;

			if (fip_port->fp_pkt_head)
				fip_port->fp_pkt_tail->p_admin.pa_next= p;
			else
				fip_port->fp_pkt_head= p;
			fip_port->fp_pkt_tail= p;
		}
        };
#if DEBUG & 256
 { where(); printf("sema_upi\n"); }
#endif
	sema_up(&send_sema);
}

static void fip_bcast(dev, pkt)
int dev;
pkt_t *pkt;
{
	fip_send(dev, pkt, &null_location);
}

static void fip_buffree(priority, reqsize)
int priority;
size_t reqsize;
{
	int again, i;
	acc_t *acc;
	fip_port_t *fip_port;

#if DEBUG & 256
 { where(); printf("fip_buffree called\n"); }
#endif

	switch(priority)
	{
	case FIP_PRI_EXP_BROAD_BUFS:
		do
		{
			again= FALSE;
			for (i= 0, fip_port= fip_port_table; i<FIP_PORT_NR; 
				i++, fip_port++)
			{
				switch (fip_port->fp_nettype)
				{
#if INCLUDE_UDP
				case FPNT_UFP:
					break;
#endif
#if INCLUDE_TCP
				case FPNT_TCP:
				{
					fip_tcp_peer_t *fip_tcp_peer;
					acc_t *tmp_acc;

					for (fip_tcp_peer= fip_port->
						fp_tcp_peerlist; fip_tcp_peer; 
						fip_tcp_peer= fip_tcp_peer->
						ftp_next)
					{
						tmp_acc= fip_tcp_peer->
							ftp_write_broad_head;
						if (!tmp_acc)
							continue;
						fip_tcp_peer->
							ftp_write_broad_head=
							tmp_acc->acc_ext_link;
						bf_afree(tmp_acc);
						again= TRUE;
					}
					break;
				}
#endif
				default:
					ip_panic(( "not reached" ));
				}
			}
			if (bf_free_buffsize >= reqsize)
				return;
		} while (again);
		break;
	case FIP_PRI_EXP_BUFS:
		do
		{
			again= FALSE;
			for (i= 0, fip_port= fip_port_table; i<FIP_PORT_NR; 
				i++, fip_port++)
			{
				switch (fip_port->fp_nettype)
				{
#if INCLUDE_UDP
				case FPNT_UDP:
					if (!fip_port->fp_udp_send_head || 
						!fip_port->fp_udp_send_head->
						acc_ext_link)
						continue;
					acc= fip_port->fp_udp_send_head->
						acc_ext_link;
					fip_port->fp_udp_send_head->
						acc_ext_link= acc->acc_ext_link;
					if (!fip_port->fp_udp_send_head->
						acc_ext_link)
						fip_port->fp_udp_send_tail=
							fip_port->
							fp_udp_send_head;
					bf_afree(acc);
					again= TRUE;
					break;
#endif
#if INCLUDE_TCP
				case FPNT_TCP:
				{
					fip_tcp_peer_t *fip_tcp_peer;
					acc_t *tmp_acc;

					for (fip_tcp_peer= fip_port->
						fp_tcp_peerlist; fip_tcp_peer; 
						fip_tcp_peer= fip_tcp_peer->
						ftp_next)
					{
						tmp_acc= fip_tcp_peer->
							ftp_write_head;
						if (!tmp_acc)
							continue;
						fip_tcp_peer->ftp_write_head=
							tmp_acc->acc_ext_link;
						bf_afree(tmp_acc);
						again= TRUE;
					}
					break;
				}
#endif
				default:
					ip_panic(( "not reached" ));
				}
			}
			if (bf_free_buffsize >= reqsize)
				return;
		} while (again);
		break;
	}
}

static void pkt_released(arg)
long arg;
{
	bf_afree((acc_t *)arg);
}

#if NEW_FLOW_CODE
static void pp_notify(arg, pkt)
long arg;
pkt_p pkt;
{
	fip_tcp_peer_t *ftp;
	int i;

	pkt_discard(pkt);
#if DEBUG & 256
 { where(); printf("pp_notify called\n"); }
#endif
	if (tcp_restart_process)
	{
		tcp_restart_process= 0;
		for (i= 0, ftp= fip_tcp_peer_table; i<TCP_PEER_NR; i++, ftp++)
		{
			if ((ftp->ftp_flags & (FTPF_INUSE | FTPF_NOPACK)) !=
				(FTPF_INUSE | FTPF_NOPACK))
			{
				continue;
			}
			assert(!(ftp->ftp_flags & FTPF_READ_IP));
			ftp->ftp_flags &= ~FTPF_NOPACK;
#if DEBUG & 256
 { where(); printf("calling process_tcp_data\n"); }
#endif
			ftp->ftp_read_head= process_tcp_data(
					ftp, ftp->ftp_read_head);
			if (ftp->ftp_flags & FTPF_NOPACK)
			{
#if DEBUG & 256
 { where(); printf("process_tcp_data failed\n"); }
#endif
				return;
			}
			assert(!(ftp->ftp_flags & FTPF_READ_IP));
#if DEBUG & 256
 { where(); printf("calling read_tcp_packets\n"); }
#endif
			read_tcp_packets(ftp);
		}
	}
}
#endif /* NEW_FLOW_CODE */

static void send_thread()
{
	fip_port_t *fip_port;
	int i;
	int more2do;
	pkt_t *pkt;
	location *loc;
	acc_t *flip_pack, *tmp_acc, *header_acc;
	char *data_dir, *data_dir_end, *data_vir, *data_vir_end;
	size_t len;

	sema_init(&send_sema, 0);
	for(;;)
	{
		mu_unlock(&mu_generic);
		i= sema_level(&send_sema);
		if (i)
		{
#if DEBUG & 256
 { where(); printf("doing sema_mdown(%d)\n", i); }
#endif
			sema_mdown(&send_sema, i);
		}
		else
		{
#if DEBUG & 256
 { where(); printf("doing sema_down\n"); }
#endif
			sema_down(&send_sema);
		}
#if DEBUG & 256
 { where(); printf("locking mu_generic\n"); }
#endif
		mu_lock(&mu_generic);
#if DEBUG & 256
 { where(); printf("locked mu_generic\n"); }
#endif

		more2do= FALSE;

		for(i=0, fip_port= fip_port_table; i<FIP_PORT_NR; 
			i++, fip_port++)
		{
			if (!fip_port->fp_pkt_head)
				continue;

			pkt= fip_port->fp_pkt_head;
			fip_port->fp_pkt_head= pkt->p_admin.pa_next;
			if (pkt->p_admin.pa_next)
				more2do= TRUE;

			PROTO_LOOK_HEADER(loc, pkt, location);
			proto_remove_header(pkt);

#if DEBUG & 256
 { where(); printf("calling bf_memreq\n"); }
#endif
			flip_pack= bf_memreq(pkt->p_contents.pc_totsize);
			data_dir= pkt_offset(pkt);
			data_dir_end= data_dir + pkt->p_contents.pc_dirsize;
			data_vir= (char *)pkt->p_contents.pc_virtual;
			data_vir_end= data_vir + pkt->p_contents.pc_totsize -
				pkt->p_contents.pc_dirsize;
			len= data_dir_end-data_dir;
			for (tmp_acc= flip_pack; tmp_acc; 
				tmp_acc= tmp_acc->acc_next)
			{
				if (len)
				{
					if (len>=tmp_acc->acc_length)
					{
						memcpy(ptr2acc_data(tmp_acc),
							data_dir, tmp_acc->
							acc_length);
						data_dir += tmp_acc->acc_length;
						len -= tmp_acc->acc_length;
						continue;
					}
					memcpy(ptr2acc_data(tmp_acc), data_dir, 
						len);
					memcpy(ptr2acc_data(tmp_acc)+len,
						data_vir, tmp_acc->acc_length-
						len);
					data_dir += len;
					data_vir += tmp_acc->acc_length-len;
					len= 0;
					continue;
				}
				memcpy(ptr2acc_data(tmp_acc), data_vir, 
					tmp_acc->acc_length);
				data_vir += tmp_acc->acc_length;
			}
assert (data_dir == data_dir_end && data_vir == data_vir_end);
#if DEBUG & 256
 { where(); printf("before switch\n"); }
#endif
			switch(fip_port->fp_nettype)
			{
#if INCLUDE_UDP
			case FPNT_UDP:
				send_udp_pkt(fip_port, flip_pack, loc);
				break;
#endif
#if INCLUDE_TCP
			case FPNT_TCP:
				send_tcp_pkt(fip_port, flip_pack, loc);
				break;
#endif
			default:
				ip_panic(( "not reached" ));
			}
			pkt_discard(pkt);
		}
		if (more2do)
			sema_up(&send_sema);
	}
}
