/*	@(#)tcp.am.c	1.2	96/02/27 09:58:40 */
/*
 *		T C P . A . M . C
 *
 *	Open a TCP/SMTP connection using Amoeba style networking.
 *
 *	Philip Homburg, derived from Berkeley version by Doug Kingston
 */
#include "util.h"		/* includes <sys/types.h>, and others */
#include "mmdf.h"
#if AMOEBA
#include <stdlib.h>
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <module/stdcmd.h>
#include <module/name.h>
#include <server/ip/hton.h>
#include <server/ip/tcpip.h>
#include <server/ip/types.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/tcp.h>
#include <server/ip/gen/tcp_io.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

extern LLog *logptr;
extern int errno;

/*ARGSUSED*/
tc_uicp (addr, sock, timeout, capptr)
long addr;
long sock;	/* IGNORED */	/* absolute socket number       */
int timeout;			/* time to wait for open        */
capability *capptr;
{
	register int skt;
#if AMOEBA
	char *tcp_svr_name;
	errstat am_err;
	capability tcp_server;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t tcpcl;
#else
	struct sockaddr_in haddr;
#endif
	short	smtpport = 0;
#ifdef notdef
	int	on = 1;
#endif

	if (smtpport == 0) {
		struct servent *sp;

		if ((sp = getservbyname("smtp", "tcp")) == NULL)
			return (RP_NO);
		smtpport = (short)sp->s_port;
	}

#if AMOEBA
#else
	haddr.sin_family = AF_INET;
	haddr.sin_port = smtpport;
	haddr.sin_addr.s_addr = htonl(addr);
#endif

#if AMOEBA
	tcp_svr_name= getenv("TCP_SVR_NAME");
	if (tcp_svr_name == NULL)
	{
#ifdef TCPIPSERV
		tcp_svr_name= TCPIPSERV;
#else
		tcp_svr_name= TCP_SVR_NAME;
#endif
	}
	am_err= name_lookup(tcp_svr_name, &tcp_server);
	if (am_err != STD_OK)
	{
		ll_log( logptr, LLOGFST, "Can't lookup server (%s)", 
							err_why(am_err));
		return( RP_LIO );
	}
	am_err= tcpip_open(&tcp_server, capptr);
	if (am_err != STD_OK)
	{
		ll_log( logptr, LLOGFST, "Can't tcpip_open server (%s)", 
							tcpip_why(am_err));
		return( RP_LIO );
	}
	tcpconf.nwtc_flags= NWTC_COPY|NWTC_LP_SEL|NWTC_SET_RA|NWTC_SET_RP;
	tcpconf.nwtc_remaddr= htonl(addr);
	tcpconf.nwtc_remport= smtpport;
	am_err= tcp_ioc_setconf(capptr, &tcpconf);
	if (am_err != STD_OK)
	{
		ll_log( logptr, LLOGFST, "Can't tcp_ioc_setconf (%s)", 
							tcpip_why(am_err));
		return( RP_LIO );
	}
#else
	skt = socket( AF_INET, SOCK_STREAM, 0 );
	if( skt < 0 ) {
		ll_log( logptr, LLOGFST, "Can't get socket (%d)", errno);
		return( RP_LIO );
	}
#endif

#ifdef notdef
	if (setsockopt (skt, SOL_SOCKET, SO_DONTLINGER, &on, sizeof on)) {
		ll_log( logptr, LLOGFST, "Can't set socket options (%d)", errno);
		close (skt);
		return( RP_LIO );
	}
#endif /* notdef */
	
        if (setjmp(timerest)) {
            close(skt);
            return ( RP_TIME );
        }
        
#if AMOEBA
	tcpcl.nwtcl_flags= 0;
        s_alarm( (unsigned) timeout );
	am_err= tcp_ioc_connect(capptr, &tcpcl);
	if (am_err != STD_OK)
	{
		std_destroy( capptr );
		s_alarm( 0 );
		ll_log( logptr, LLOGFST, "Can't get connection (%s)", 
							tcpip_why(am_err));
		return( RP_DHST );
	}
	s_alarm( 0 );
#else
        s_alarm( (unsigned) timeout );
	if( connect(skt, &haddr, sizeof haddr) < 0 ) {
		close( skt );
		s_alarm( 0 );
		ll_log( logptr, LLOGFST, "Can't get connection (%d)", errno);
		return( RP_DHST );
	}
	s_alarm( 0 );
	fds -> pip.prd = skt;
	fds -> pip.pwrt = dup(skt);
#endif
	return (RP_OK);
}
