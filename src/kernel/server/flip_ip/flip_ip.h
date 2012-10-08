/*	@(#)flip_ip.h	1.4	94/04/06 09:53:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
flip_ip.h

Created Sept 2, 1991 by Philip Homburg
*/

#define FIP_PORT_NR		1

#define INCLUDE_UDP	0
#define INCLUDE_TCP	1

#define TCPPORT_LISTEN_FLIP	0x8145
#define TCPPORT_CONNECT_FLIP	0x8146
#define TCP_PEER_NR		20

#define UDPPORT_FLIP		0x8146

typedef struct fip_port
{
	int fp_flags;
	int fp_state;
	int fp_flipdev;
	pkt_t *fp_pkt_head, *fp_pkt_tail;
	int fp_nettype;
	ipaddr_t fp_ipaddr;

#if INCLUDE_UDP
	/* UDP fields */
	int fp_udpdev;
	int fp_udpfd;
	struct fip_udp_peer *fp_udp_peerlist;
	acc_t *fp_udp_send_head, *fp_udp_send_tail;
#endif

#if INCLUDE_TCP
	/* TCP fields */
	int fp_tcpdev;
	int fp_tcpfd;
	struct fip_tcp_peer *fp_tcp_peerlist;
#endif
} fip_port_t;

#define FPF_EMPTY	0x0
#define FPF_SUSPEND	0x1
#if 0
#define FPF_SEND_IP	0x2
#define FPF_SEND_SP	0x4
#define FPF_READ_IP	0x8
#define FPF_READ_SP	0x10
#endif

#define FPS_UDP_EMPTY	0
#define FPS_UDP_SETOPT	1
#define FPS_UDP_GETOPT	2
#define FPS_UDP_MAIN	3
#define FPS_UDP_ERROR	0xF

#define FPS_TCP_EMPTY	0x10
#define FPS_TCP_GETCONF	0x11
#define FPS_TCP_MAIN	0x12
#define FPS_TCP_ERROR	0x1F

#define FPNT_UDP	1
#define FPNT_TCP	2

extern struct udp_peers
{
	u8_t upr_addr[4];
	udpport_t upr_port;
} udp_peers[];

extern struct tcp_peers
{
	u8_t tpr_addr[4];
	tcpport_t tpr_lport;
	tcpport_t tpr_cport;
} tcp_peers[];

typedef struct fip_tcp_peer
{
	struct fip_port *ftp_fip_port;
	ipaddr_t ftp_ipaddr;
	tcpport_t ftp_lport;
	tcpport_t ftp_cport;
	int ftp_read_state;
	int ftp_write_state;
	int ftp_read_fd;
	int ftp_write_fd;
	timer_t ftp_read_timer;
	timer_t ftp_write_timer;
	int ftp_flags;
	acc_t *ftp_write_head;
	acc_t *ftp_write_tail;
	acc_t *ftp_write_broad_head;
	acc_t *ftp_write_broad_tail;
	acc_t *ftp_read_head;
	acc_t *ftp_read_tail;
	acc_t *ftp_pack;
	acc_t *ftp_broad_pack;
#if DEBUG
	int ftp_n_req_credit;
#endif
	struct fip_tcp_peer *ftp_next;
} fip_tcp_peer_t;

#define FTPR_EMPTY	0
#define FTPR_SETCONF	1
#define FTPR_LISTEN	2
#define FTPR_READ	3
#define FTPR_TIMER	4

#define FTPW_EMPTY	0
#define FTPW_SETCONF	1
#define FTPW_CONNECT	2
#define FTPW_WRITE	3
#define FTPW_TIMER	4

#define FTPF_INUSE	0x1
#define FTPF_WRITE_IP	0x2
#define FTPF_WRITE_SP	0x4
#define FTPF_READ_IP	0x8
#define FTPF_READ_SP	0x10
#define FTPF_NOPACK	0x20

extern fip_tcp_peer_t fip_tcp_peer_table[TCP_PEER_NR];
extern fip_port_t fip_port_table[FIP_PORT_NR];
extern mutex mu_flip_ip_inited;

void fip_add_tcp_peer _ARGS(( int tcpdev, ipaddr_t host, Tcpport_t listen_port,
	Tcpport_t connect_port ));
