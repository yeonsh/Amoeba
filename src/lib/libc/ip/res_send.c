/*	@(#)res_send.c	1.3	96/02/27 11:09:25 */
/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define AMOEBA	1	/* Rediculous but nessesary */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_send.c	6.25 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

/*
 * Send query to name server and wait for reply.
 */

#if AMOEBA
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <_ARGS.h>
#include <amoeba.h>
#include <ampolicy.h>
#include <amtools.h>
#include <cmdreg.h>
#include <stderr.h>

#include <module/stdcmd.h>

#include <server/ip/hton.h>
#include <server/ip/tcpip.h>
#include <server/ip/tcp_io.h>
#include <server/ip/types.h>

#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/nameser.h>
#include <server/ip/gen/resolv.h>
#include <server/ip/gen/tcp.h>
#include <server/ip/gen/tcp_io.h>

typedef u16_t u_short;

#define ETIMEDOUT STD_INTR
#define ECONNRESET TCPIP_CONNRESET

static capability *tcp_connect _ARGS(( ipaddr_t host, Tcpport_t prt, 
	int *terrno ));
static errstat tcpip_writeall _ARGS(( capability *chan_cap, char *buf,
	size_t buflen ));
#else
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/nameser.h>
#include <resolv.h>

extern int errno;
#endif /* AMOEBA */

#if AMOEBA
capability tcp_server_cap, chan_cap;
capability *t_serv_cap_ptr= NULL, *chan_cap_ptr= NULL;
#else /* AMOEBA */
static int s = -1;	/* socket used for communications */
static struct sockaddr no_addr;

#ifndef FD_SET
#define	NFDBITS		32
#define	FD_SETSIZE	32
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif /* FD_SET */
#endif /* AMOEBA */

res_send(buf, buflen, answer, anslen)
#ifdef __STDC__
	const char *buf;
#else
	char *buf;
#endif
	int buflen;
	char *answer;
	int anslen;
{
	register int n;
	int try, v_circuit, resplen, ns;
	int gotsomewhere = 0;
#if BSD >= 43
	int connected = 0;
#endif
	int connreset = 0;
#if !AMOEBA
	u_short id;
	dns_hdr_t *hp = (dns_hdr_t *) buf;
#endif
	u_short len;
	char *cp;
	dns_hdr_t *anhp = (dns_hdr_t *) answer;
	int terrno = ETIMEDOUT;
	char junk[512];

#ifdef DEBUG
	if (_res.options & RES_DEBUG) {
		printf("res_send()\n");
		p_query(buf);
	}
#endif /* DEBUG */
	if (!(_res.options & RES_INIT))
		if (res_init() == -1) {
			return(-1);
		}
	v_circuit = (_res.options & RES_USEVC) || buflen > PACKETSZ;
#if !AMOEBA
	id = hp->dh_id;
#endif
	/*
	 * Send request, RETRY times, or until successful
	 */
	for (try = 0; try < _res.retry; try++) {
	   for (ns = 0; ns < _res.nscount; ns++) {
#ifdef DEBUG
		if (_res.options & RES_DEBUG)
			printf("Querying server (# %d) address = %s\n", ns+1,
			      inet_ntoa(_res.nsaddr_list[ns]));
#endif /* DEBUG */
#if !AMOEBA
	usevc:
#endif
		if (v_circuit) {
#if AMOEBA
			int truncated = 0;
			errstat nbytes;

			/*
			 * Use virtual circuit;
			 * at most one attempt per server.
			 */
			try = _res.retry;
			if (!chan_cap_ptr) 
			{
				chan_cap_ptr= tcp_connect(_res.nsaddr_list[ns],
					_res.nsport_list[ns], &terrno);
				if (!chan_cap_ptr)
					continue;
			}
			/*
			 * Send length & message
			 */
			len = htons((u_short)buflen);
			nbytes= tcpip_writeall(chan_cap_ptr, (char *)&len, 
				sizeof(len));
			if (nbytes != sizeof(len))
			{
				assert(ERR_STATUS(nbytes));
				terrno= ERR_CONVERT(nbytes);
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "write failed: %s\n",
					err_why(terrno));
#endif /* DEBUG */
				std_destroy(chan_cap_ptr);
				chan_cap_ptr= NULL;
				continue;
			}
			nbytes= tcpip_writeall(chan_cap_ptr,
					       (char *) buf, buflen);
			if (nbytes != buflen)
			{
				assert(ERR_STATUS(nbytes));
				terrno= ERR_CONVERT(nbytes);
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "write failed: %s\n",
					err_why(terrno));
#endif /* DEBUG */
				std_destroy(chan_cap_ptr);
				chan_cap_ptr= NULL;
				continue;
			}
			/*
			 * Receive length & response
			 */
			cp = answer;
			len = sizeof(short);
			while (len != 0)
			{
				n = tcpip_read(chan_cap_ptr, (char *)cp, 
					(int)len);
				if (ERR_STATUS(n) || !n)
					break;
				cp += n;
				assert(len >= n);
				len -= n;
			}
			if (len) {
				terrno = ERR_CONVERT(n);
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "read failed: %s\n",
						err_why(terrno));
#endif /* DEBUG */
				std_destroy(chan_cap_ptr);
				chan_cap_ptr= NULL;
				/*
				 * A long running process might get its TCP
				 * connection reset if the remote server was
				 * restarted.  Requery the server instead of
				 * trying a new one.  When there is only one
				 * server, this means that a query might work
				 * instead of failing.  We only allow one reset
				 * per query to prevent looping.
				 */
				if (terrno == ECONNRESET && !connreset) {
					connreset = 1;
					ns--;
				}
				continue;
			}
			cp = answer;
			if ((resplen = ntohs(*(u_short *)cp)) > anslen) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "response truncated\n");
#endif /* DEBUG */
				len = anslen;
				truncated = 1;
			} else
				len = resplen;
			while (len != 0)
			{
				n= tcpip_read(chan_cap_ptr, (char *)cp,
					(int)len);
				if (ERR_STATUS(n) || !n)
					break;
				cp += n;
				assert(len >= n);
				len -= n;
			}
			if (len) {
				terrno = ERR_CONVERT(n);
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "read failed: %s\n",
						err_why(terrno));
#endif /* DEBUG */
				std_destroy(chan_cap_ptr);
				chan_cap_ptr= NULL;
				continue;
			}
			if (truncated) {
				/*
				 * Flush rest of answer
				 * so connection stays in synch.
				 */
				anhp->dh_flag1 |= DHF_TC;
				len = resplen - anslen;
				while (len != 0) {
					n = (len > sizeof(junk) ?
					    sizeof(junk) : len);
					n = tcpip_read(chan_cap_ptr, junk, n);
					if (!ERR_STATUS(n) && n)
					{
						assert(len >= n);
						len -= n;
					}
					else
						break;
				}
			}
#else /* AMOEBA */
			int truncated = 0;

			/*
			 * Use virtual circuit;
			 * at most one attempt per server.
			 */
			try = _res.retry;
			if (s < 0) {
				s = socket(AF_INET, SOCK_STREAM, 0);
				if (s < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("socket (vc) failed");
#endif /* DEBUG */
					continue;
				}
				if (connect(s, &(_res.nsaddr_list[ns]),
				   sizeof(struct sockaddr)) < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("connect failed");
#endif /* DEBUG */
					(void) close(s);
					s = -1;
					continue;
				}
			}
			/*
			 * Send length & message
			 */
			len = htons((u_short)buflen);
			iov[0].iov_base = (caddr_t)&len;
			iov[0].iov_len = sizeof(len);
			iov[1].iov_base = buf;
			iov[1].iov_len = buflen;
			if (writev(s, iov, 2) != sizeof(len) + buflen) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("write failed");
#endif /* DEBUG */
				(void) close(s);
				s = -1;
				continue;
			}
			/*
			 * Receive length & response
			 */
			cp = answer;
			len = sizeof(short);
			while (len != 0 &&
			    (n = read(s, (char *)cp, (int)len)) > 0) {
				cp += n;
				len -= n;
			}
			if (n <= 0) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("read failed");
#endif /* DEBUG */
				(void) close(s);
				s = -1;
				/*
				 * A long running process might get its TCP
				 * connection reset if the remote server was
				 * restarted.  Requery the server instead of
				 * trying a new one.  When there is only one
				 * server, this means that a query might work
				 * instead of failing.  We only allow one reset
				 * per query to prevent looping.
				 */
				if (terrno == ECONNRESET && !connreset) {
					connreset = 1;
					ns--;
				}
				continue;
			}
			cp = answer;
			if ((resplen = ntohs(*(u_short *)cp)) > anslen) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					fprintf(stderr, "response truncated\n");
#endif /* DEBUG */
				len = anslen;
				truncated = 1;
			} else
				len = resplen;
			while (len != 0 &&
			   (n = read(s, (char *)cp, (int)len)) > 0) {
				cp += n;
				len -= n;
			}
			if (n <= 0) {
				terrno = errno;
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("read failed");
#endif /* DEBUG */
				(void) close(s);
				s = -1;
				continue;
			}
			if (truncated) {
				/*
				 * Flush rest of answer
				 * so connection stays in synch.
				 */
				anhp->tc = 1;
				len = resplen - anslen;
				while (len != 0) {
					n = (len > sizeof(junk) ?
					    sizeof(junk) : len);
					if ((n = read(s, junk, n)) > 0)
						len -= n;
					else
						break;
				}
			}
#endif /* AMOEBA */
		} else {
#if AMOEBA
			printf("not implemented datagrams\n");
			exit(1);
#else /* AMOEBA */
			/*
			 * Use datagrams.
			 */
			if (s < 0) {
				s = socket(AF_INET, SOCK_DGRAM, 0);
				if (s < 0) {
					terrno = errno;
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
					    perror("socket (dg) failed");
#endif /* DEBUG */
					continue;
				}
			}
#if	BSD >= 43
			/*
			 * I'm tired of answering this question, so:
			 * On a 4.3BSD+ machine (client and server,
			 * actually), sending to a nameserver datagram
			 * port with no nameserver will cause an
			 * ICMP port unreachable message to be returned.
			 * If our datagram socket is "connected" to the
			 * server, we get an ECONNREFUSED error on the next
			 * socket operation, and select returns if the
			 * error message is received.  We can thus detect
			 * the absence of a nameserver without timing out.
			 * If we have sent queries to at least two servers,
			 * however, we don't want to remain connected,
			 * as we wish to receive answers from the first
			 * server to respond.
			 */
			if (_res.nscount == 1 || (try == 0 && ns == 0)) {
				/*
				 * Don't use connect if we might
				 * still receive a response
				 * from another server.
				 */
				if (connected == 0) {
					if (connect(s, &_res.nsaddr_list[ns],
					    sizeof(struct sockaddr)) < 0) {
#ifdef DEBUG
						if (_res.options & RES_DEBUG)
							perror("connect");
#endif /* DEBUG */
						continue;
					}
					connected = 1;
				}
				if (send(s, buf, buflen, 0) != buflen) {
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
						perror("send");
#endif /* DEBUG */
					continue;
				}
			} else {
				/*
				 * Disconnect if we want to listen
				 * for responses from more than one server.
				 */
				if (connected) {
					(void) connect(s, &no_addr,
					    sizeof(no_addr));
					connected = 0;
				}
#endif /* BSD */
				if (sendto(s, buf, buflen, 0,
				    &_res.nsaddr_list[ns],
				    sizeof(struct sockaddr)) != buflen) {
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
						perror("sendto");
#endif /* DEBUG */
					continue;
				}
#if	BSD >= 43
			}
#endif /* BSD */

			/*
			 * Wait for reply
			 */
			timeout.tv_sec = (_res.retrans << try);
			if (try > 0)
				timeout.tv_sec /= _res.nscount;
			if (timeout.tv_sec <= 0)
				timeout.tv_sec = 1;
			timeout.tv_usec = 0;
wait:
			FD_ZERO(&dsmask);
			FD_SET(s, &dsmask);
			n = select(s+1, &dsmask, (fd_set *)NULL,
				(fd_set *)NULL, &timeout);
			if (n < 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("select");
#endif /* DEBUG */
				continue;
			}
			if (n == 0) {
				/*
				 * timeout
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("timeout\n");
#endif /* DEBUG */
#if BSD >= 43
				gotsomewhere = 1;
#endif
				continue;
			}
			if ((resplen = recv(s, answer, anslen, 0)) <= 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					perror("recvfrom");
#endif /* DEBUG */
				continue;
			}
			gotsomewhere = 1;
			if (id != anhp->id) {
				/*
				 * response from old query, ignore it
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG) {
					printf("old answer:\n");
					p_query(answer);
				}
#endif /* DEBUG */
				goto wait;
			}
			if (!(_res.options & RES_IGNTC) && anhp->tc) {
				/*
				 * get rest of answer;
				 * use TCP with same server.
				 */
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("truncated answer\n");
#endif /* DEBUG */
				(void) close(s);
				s = -1;
				v_circuit = 1;
				goto usevc;
			}
#endif /* AMOEBA */
		}
#ifdef DEBUG
		if (_res.options & RES_DEBUG) {
			printf("got answer:\n");
			p_query(answer);
		}
#endif /* DEBUG */
		/*
		 * If using virtual circuits, we assume that the first server
		 * is preferred * over the rest (i.e. it is on the local
		 * machine) and only keep that one open.
		 * If we have temporarily opened a virtual circuit,
		 * or if we haven't been asked to keep a socket open,
		 * close the socket.
		 */
		if ((v_circuit &&
		    ((_res.options & RES_USEVC) == 0 || ns != 0)) ||
		    (_res.options & RES_STAYOPEN) == 0) {
#if AMOEBA
			std_destroy(chan_cap_ptr);
			chan_cap_ptr= NULL;
#else /* AMOEBA */
			(void) close(s);
			s = -1;
#endif /* AMOEBA */
		}
		return (resplen);
	   }
	}
#if AMOEBA
	if (chan_cap_ptr)
	{
		std_destroy(chan_cap_ptr);
		chan_cap_ptr= NULL;
	}
#else /* AMOEBA */
	if (s >= 0) {
		(void) close(s);
		s = -1;
	}
#endif /* AMOEBA */
	if (v_circuit == 0)
		if (gotsomewhere == 0)
			errno = ECONNREFUSED;	/* no nameservers found */
		else
			errno = ETIMEDOUT;	/* no answer obtained */
	else
		errno = terrno;
	return (-1);
}

/*
 * This routine is for closing the socket if a virtual circuit is used and
 * the program wants to close it.  This provides support for endhostent()
 * which expects to close the socket.
 *
 * This routine is not expected to be user visible.
 */
void
_res_close()
{
#if AMOEBA
	if (chan_cap_ptr)
	{
		std_destroy(chan_cap_ptr);
		chan_cap_ptr= NULL;
	}
#else /* AMOEBA */
	if (s != -1) {
		(void) close(s);
		s = -1;
	}
#endif /* AMOEBA */
}

static capability *tcp_connect(host, prt, terrno)
ipaddr_t host;
tcpport_t prt;
int *terrno;
{
	char *serv_name;
	errstat error;
	nwio_tcpconf_t tcpconf;
	nwio_tcpcl_t clopt;

	if (!t_serv_cap_ptr)
	{
		serv_name= getenv("TCP_SERVER");
		if (serv_name && !serv_name[0])
			serv_name= NULL;
		if (!serv_name)
			serv_name= TCP_SVR_NAME;
		error= name_lookup(serv_name, &tcp_server_cap);
		if (ERR_STATUS(error))
		{
#ifdef DEBUG
			if (_res.options & RES_DEBUG)
				printf("unable to lookup '%s': %s\n", serv_name,
					err_why(ERR_CONVERT(error)));
#endif
			*terrno= ERR_CONVERT(error);
			return NULL;
		}
		t_serv_cap_ptr= &tcp_server_cap;
	}
	error= tcpip_open(t_serv_cap_ptr, &chan_cap);
	if (ERR_STATUS(error))
	{
		*terrno= ERR_CONVERT(error);
		return NULL;
	}
	error= tcpip_keepalive_cap(&chan_cap);
	if (ERR_STATUS(error))
	{
		std_destroy(&chan_cap);
		*terrno= ERR_CONVERT(error);
		return NULL;
	}
	tcpconf.nwtc_flags= NWTC_EXCL | NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
	tcpconf.nwtc_remaddr= host;
	tcpconf.nwtc_remport= prt;
	error= tcp_ioc_setconf(&chan_cap, &tcpconf);
	if (ERR_STATUS(error))
	{
		std_destroy(&chan_cap);
		*terrno= ERR_CONVERT(error);
		return NULL;
	}
	clopt.nwtcl_flags= 0;
	error= tcp_ioc_connect(&chan_cap, &clopt);
	if (error == STD_NOTNOW)
	{
		/* Try again in 0.5 secs - it will probably succeed */
		millisleep(500);
		error= tcp_ioc_connect(&chan_cap, &clopt);
	}
	if (ERR_STATUS(error))
	{
		std_destroy(&chan_cap);
		*terrno= ERR_CONVERT(error);
		return NULL;
	}
	return &chan_cap;
}

static errstat tcpip_writeall(cap, buf, siz)
capability *cap;
char *buf;
size_t siz;
{
	size_t siz_org;
	errstat nbytes;

	siz_org= siz;

	while (siz)
	{
		nbytes= tcpip_write(cap, buf, siz);
		if (ERR_STATUS(nbytes) || !nbytes)
			return nbytes;
		assert(siz >= nbytes);
		buf += nbytes;
		siz -= nbytes;
	}
	return siz_org;
}
