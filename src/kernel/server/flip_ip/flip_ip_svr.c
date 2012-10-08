/*	@(#)flip_ip_svr.c	1.4	94/04/06 09:54:13 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
flip_ip_svr.c

Server interface for flip_ip.
*/

#include "nw_task.h"
#include "amoeba_incl.h"

#include <sys/flip/packet.h>
#include <server/ip/gen/inet.h>

#include "generic/buf.h"
#include "generic/clock.h"
#include "flip_ip.h"

INIT_PANIC();

void flip_ip_svr_init _ARGS(( void ));
static void flip_ip_svr _ARGS(( void ));
char *print_fip_port _ARGS(( char *bp, char *ep, fip_port_t *fip_port ));
char *print_tcp_peer _ARGS(( char *bp, char *ep, 
	fip_tcp_peer_t *fip_tcp_peer ));

am_port_t fis_pub_port, fis_priv_port;
am_port_t fis_random;

#define FIS_STACK	8192
#define FIS_RIGHTS	0x80
#define FIS_NAME	"flip_ip"

void flip_ip_svr_init()
{
	capability fis_cap;
	errstat result;

#if DEBUG & 2
 { where(); printf("flip_ip_svr_init called\n"); }
#endif

	uniqport(&fis_priv_port);
	priv2pub(&fis_priv_port, &fis_pub_port);
	uniqport(&fis_random);

	NewKernelThread(flip_ip_svr, FIS_STACK);

	fis_cap.cap_port= fis_pub_port;
	result= prv_encode(&fis_cap.cap_priv, (objnum)0, FIS_RIGHTS, 
		&fis_random);
	if (result)
		panic(( "unable to initialize capability" ));

	dirappend(FIS_NAME, &fis_cap);
}

static void flip_ip_svr()
{
	char reqbuf[1024], replbuf[1024];
	char *repl_ptr;
	char *bp, *ep;
	errstat req_size, repl_size;
	am_header_t req_header, repl_header;

	for (;;)
	{
		req_header.h_port= fis_priv_port;
		req_size= rpc_getreq(&req_header, reqbuf, sizeof(reqbuf));
		repl_ptr= replbuf;
		repl_header.h_status= STD_OK;
		if (ERR_STATUS(req_size))
			panic(( "getreq() failed: %s",
				err_why(ERR_CONVERT(req_size)) ));

		switch (req_header.h_command)
		{
		case STD_INFO:
			repl_ptr= bprintf(repl_ptr, replbuf + sizeof(replbuf),
				"flip_ip server");
			break;
		case STD_STATUS:
		{
			int i;

			mu_lock(&mu_flip_ip_inited);
			mu_lock(&mu_generic);

			bp= replbuf;
			ep= replbuf+sizeof(replbuf);
			for (i= 0; i<FIP_PORT_NR; i++)
				bp= print_fip_port(bp, ep, fip_port_table+i);
			repl_ptr= bp;

			mu_unlock(&mu_generic);
			mu_unlock(&mu_flip_ip_inited);

			break;
		}
		case UNREGISTERED_FIRST_COM:
		{
			int proto;
			ipaddr_t host;
			tcpport_t listen_port, connect_port;

			mu_lock(&mu_flip_ip_inited);
			mu_lock(&mu_generic);

			bp= reqbuf;
			ep= bp + req_size;
			bp= buf_get_uint32(bp, ep, &proto);
			bp= buf_get_bytes(bp, ep, &host, sizeof(host));
			bp= buf_get_bytes(bp, ep, &listen_port, 
				sizeof(listen_port));
			bp= buf_get_bytes(bp, ep, &connect_port, 
				sizeof(connect_port));
			if (bp != ep)
			{
				repl_header.h_status= STD_ARGBAD;
				break;
			}
			if (proto != FPNT_TCP)
			{
				repl_header.h_status= STD_ARGBAD;
				break;
			}
			fip_add_tcp_peer (0, host, listen_port, connect_port);

			mu_unlock(&mu_generic);
			mu_unlock(&mu_flip_ip_inited);

			break;
		}
		default:
			repl_header.h_status= STD_COMBAD;
			break;
		}
		if (!repl_ptr)
			repl_size= sizeof(replbuf);
		else
			repl_size= repl_ptr-replbuf;
		repl_header.h_size= repl_size;
		rpc_putrep(&repl_header, replbuf, repl_size);
	}
}

char *print_fip_port(bp, ep, fip_port)
char *bp, *ep;
fip_port_t *fip_port;
{
	bp= bprintf(bp, ep, "fip_port %d: type %d\n", fip_port-fip_port_table,
		fip_port->fp_nettype);
	switch(fip_port->fp_nettype)
	{
	case FPNT_TCP:
	{

		fip_tcp_peer_t *fip_tcp_peer;
		if (!fip_port->fp_tcp_peerlist)
		{
			bp= bprintf(bp, ep, "no tcp_peers\n");
			break;
		}
		for (fip_tcp_peer= fip_port->fp_tcp_peerlist; fip_tcp_peer;
			fip_tcp_peer= fip_tcp_peer->ftp_next)
		{
			bp= bprintf(bp, ep, "\t");
			bp= print_tcp_peer(bp, ep, fip_tcp_peer);
			bp= bprintf(bp, ep, "\n");
		}
		break;
	}
	}
	return bp;
}

char *print_tcp_peer(bp, ep, fip_tcp_peer)
char *bp, *ep;
fip_tcp_peer_t *fip_tcp_peer;
{
	bp= bprintf(bp, ep, "{ 0x%x, 0x%x, 0x%x }", 
		ntohl(fip_tcp_peer->ftp_ipaddr), ntohs(fip_tcp_peer->ftp_lport),
			ntohs(fip_tcp_peer->ftp_cport));
	return bp;
}
