/*	@(#)ping.c	1.1	96/02/27 10:14:34 */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ping.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

/*
 *			P I N G . C
 *
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */

#ifdef __minix_vmd
#define _MINIX_SOURCE
#define MINIX
#endif

#if !defined(MINIX) && !defined(AMOEBA)
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/signal.h>
#endif

#ifdef MINIX
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/ip_hdr.h>
#include <net/gen/ip_io.h>
#include <net/gen/icmp.h>
#include <net/gen/icmp_hdr.h>
#include <net/gen/netdb.h>
#include <net/gen/socket.h>
#else
#ifdef AMOEBA
#define interval	BAD_INTERVAL
#include <amoeba.h>
#include <ampolicy.h>
#include <stderr.h>
#undef interval
#include <stdlib.h>
#include <sys/time.h>
#include <module/name.h>
#include <module/stdcmd.h>
#include <server/ip/hton.h>
#include <server/ip/types.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/ip_hdr.h>
#include <server/ip/gen/ip_io.h>
#include <server/ip/gen/icmp.h>
#include <server/ip/gen/icmp_hdr.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/socket.h>
#else
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netdb.h>
#endif /* AMOEBA */
#endif /* MINIX */
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#if defined(MINIX) || defined(AMOEBA)
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>
#endif

#ifdef MINIX
typedef unsigned char	u_char;
typedef unsigned short	u_short;
typedef unsigned int	u_int;
typedef unsigned long	u_long;
typedef char *caddr_t;
#endif

#ifdef AMOEBA
char *strdup _ARGS(( char * ));
#define	bcopy(a,b,c)	memmove(b,a,c)
#define	bzero(a,b)	memset(a,0,b)
#define	bcmp(a,b,c)	memcmp(a,b,c)
#endif

#if defined(MINIX) || defined(AMOEBA)
struct sockaddr
{
	u_char	sa_len;
	u_char	sa_family;
	char	sa_data[14];
};
struct in_addr
{
	u_long	s_addr;
};
struct sockaddr_in
{
	u_char	sin_len;
	u_char	sin_family;
	u_short sin_port;
	struct	in_addr sin_addr;
	char	sin_zero[8];
};

#define ip_ttl		ih_ttl
#define ip_p		ih_proto
#define ip_sum		ih_hdr_chk
#define ip_src		ih_src
#define ip_dst		ih_dst

#define icmp_type	ih_type
#define icmp_code	ih_code
#define icmp_cksum	ih_chksum
#define icmp_seq	ih_hun.ihh_idseq.iis_seq
#define icmp_id		ih_hun.ihh_idseq.iis_id
#define icmp_data	ih_dun.uhd_data
#define icmp_gwaddr	ih_hun.ihh_gateway

#define ip_off		ih_flags_fragoff

#define ICMP_ECHO	ICMP_TYPE_ECHO_REQ
#define ICMP_ECHOREPLY	ICMP_TYPE_ECHO_REPL
#define ICMP_UNREACH	ICMP_TYPE_DST_UNRCH
#define ICMP_UNREACH_NET	ICMP_NET_UNRCH
#define ICMP_UNREACH_HOST	ICMP_HOST_UNRCH
#define ICMP_UNREACH_PROTOCOL	ICMP_PROTOCOL_UNRCH
#define ICMP_UNREACH_PORT	ICMP_PORT_UNRCH
#define ICMP_UNREACH_NEEDFRAG	ICMP_FRAGM_AND_DF
#define ICMP_UNREACH_SRCFAIL	ICMP_SOURCE_ROUTE_FAILED
#define ICMP_SOURCEQUENCH	ICMP_TYPE_SRC_QUENCH
#define ICMP_REDIRECT		ICMP_TYPE_REDIRECT
#define ICMP_REDIRECT_TOSNET	ICMP_REDIRECT_TOS_AND_NET
#define ICMP_REDIRECT_TOSHOST	ICMP_REDIRECT_TOS_AND_HOST
#define ICMP_TIMXCEED	ICMP_TYPE_TIME_EXCEEDED
#define ICMP_TIMXCEED_INTRANS	ICMP_TTL_EXC
#define ICMP_TIMXCEED_REASS	ICMP_FRAG_REASSEM
#define ICMP_PARAMPROB	ICMP_TYPE_PARAM_PROBLEM
#define ICMP_TSTAMP	ICMP_TYPE_TS_REQ
#define ICMP_TSTAMPREPLY	ICMP_TYPE_TS_REPL
#define ICMP_IREQ	ICMP_TYPE_INFO_REQ
#define ICMP_IREQREPLY	ICMP_TYPE_INFO_REPL

#define MAX_IPOPTLEN (IP_MAX_HDR_SIZE-IP_MIN_HDR_SIZE)
#define ICMP_MINLEN ICMP_MIN_HDR_LEN

#define IPOPT_EOL	IP_OPT_EOL
#define IPOPT_LSRR	IP_OPT_LSRR
#define IPOPT_MINOFF	IP_OPT_RR_MIN
#define IPOPT_RR	IP_OPT_RR
#define IPOPT_NOP	IP_OPT_NOP

#endif /* MINIX */

#define	DEFDATALEN	(64 - 8)	/* default data length */
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPACKET	(65536 - 60 - 8)/* max packet size */
#define	MAXWAIT		10		/* max seconds to wait for response */
#define	NROUTES		9		/* number of record route slots */

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/* various options */
int options;
#define	F_FLOOD		0x001
#define	F_INTERVAL	0x002
#define	F_NUMERIC	0x004
#define	F_PINGFILLED	0x008
#define	F_QUIET		0x010
#define	F_RROUTE	0x020
#define	F_SO_DEBUG	0x040
#define	F_SO_DONTROUTE	0x080
#define	F_VERBOSE	0x100

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 128)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

struct sockaddr whereto;	/* who to ping */
int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
#if defined(MINIX) || defined(AMOEBA)
u_char ip_outpack[IP_MIN_HDR_SIZE + MAXPACKET];
u_char *outpack= &ip_outpack[IP_MIN_HDR_SIZE];
#else
u_char outpack[MAXPACKET];
#endif
char BSPACE = '\b';		/* characters written for flood */
char DOT = '.';
char *hostname;
int ident;			/* process id to identify our packets */
#ifdef AMOEBA
capability chan_cap;
#endif

/* counters */
long npackets;			/* max packets to transmit */
long nreceived;			/* # of packets we got back */
long nrepeats;			/* number of duplicates */
long ntransmitted;		/* sequence # for outbound packets = #sent */
int interval = 1;		/* interval between packets */

/* timing */
int timing;			/* flag to do timing */
double tmin = 999999999.0;	/* minimum round trip time */
double tmax = 0.0;		/* maximum round trip time */
double tsum = 0.0;		/* sum of all times, for doing average */

char *pr_addr();
void catcher(), finish();
#ifdef MINIX
void dummy_catcher();
#endif

main(argc, argv)
	int argc;
	char **argv;
{
	extern int errno, optind;
	extern char *optarg;
	struct timeval timeout;
	struct hostent *hp;
	struct sockaddr_in *to;
	struct protoent *proto;
	register int i;
	int ch, fdmask, hold, packlen, preload;
	u_char *datap, *packet;
#ifdef MINIX
	char *target;
	char *ip_device;
	nwio_ipopt_t ipopt; 
	ip_hdr_t *ih;
#else
#ifdef AMOEBA
	char *target;
	char *ip_server;
	nwio_ipopt_t ipopt; 
	ip_hdr_t *ih;
	errstat error;
	capability ip_cap;
#else
	char *target, hnamebuf[MAXHOSTNAMELEN], *malloc();
#endif /* AMOEBA */
#endif /* MINIX */
#ifdef IP_OPTIONS
	char rspace[3 + 4 * NROUTES + 1];	/* record route space */
#endif

	preload = 0;
	datap = &outpack[8 + sizeof(struct timeval)];
	while ((ch = getopt(argc, argv, "Rc:dfh:i:l:np:qrs:v")) != EOF)
		switch(ch) {
		case 'c':
			npackets = atoi(optarg);
			if (npackets <= 0) {
				(void)fprintf(stderr,
				    "ping: bad number of packets to transmit.\n");
				exit(1);
			}
			break;
		case 'd':
			options |= F_SO_DEBUG;
			break;
		case 'f':
			if (getuid()) {
				(void)fprintf(stderr,
				    "ping: %s\n", strerror(EPERM));
				exit(1);
			}
			options |= F_FLOOD;
			setbuf(stdout, (char *)NULL);
			break;
		case 'i':		/* wait between sending packets */
			interval = atoi(optarg);
			if (interval <= 0) {
				(void)fprintf(stderr,
				    "ping: bad timing interval.\n");
				exit(1);
			}
			options |= F_INTERVAL;
			break;
		case 'l':
			preload = atoi(optarg);
			if (preload < 0) {
				(void)fprintf(stderr,
				    "ping: bad preload value.\n");
				exit(1);
			}
			break;
		case 'n':
			options |= F_NUMERIC;
			break;
		case 'p':		/* fill buffer with user pattern */
			options |= F_PINGFILLED;
			fill((char *)datap, optarg);
				break;
		case 'q':
			options |= F_QUIET;
			break;
		case 'R':
			options |= F_RROUTE;
			break;
		case 'r':
			options |= F_SO_DONTROUTE;
			break;
		case 's':		/* size of packet to send */
			datalen = atoi(optarg);
			if (datalen > MAXPACKET) {
				(void)fprintf(stderr,
				    "ping: packet size too large.\n");
				exit(1);
			}
			if (datalen <= 0) {
				(void)fprintf(stderr,
				    "ping: illegal packet size.\n");
				exit(1);
			}
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	target = *argv;

	bzero((char *)&whereto, sizeof(struct sockaddr));
	to = (struct sockaddr_in *)&whereto;
	to->sin_family = AF_INET;
#ifdef MINIX
	if (inet_aton(target, &to->sin_addr.s_addr) == 1)
		hostname = target;
#else
	to->sin_addr.s_addr = inet_addr(target);
	if (to->sin_addr.s_addr != (u_int)-1)
		hostname = target;
#endif
	else {
		hp = gethostbyname(target);
		if (!hp) {
			(void)fprintf(stderr,
			    "ping: unknown host %s\n", target);
			exit(1);
		}
		to->sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, (caddr_t)&to->sin_addr, hp->h_length);
#if defined(MINIX) || defined(AMOEBA)
		hostname= strdup(hp->h_name);
		if (hostname == NULL)
		{
			fprintf(stderr, "ping: out of memory\n");
			exit(1);
		}
#else
		(void)strncpy(hnamebuf, hp->h_name, sizeof(hnamebuf) - 1);
		hostname = hnamebuf;
#endif
	}

	if (options & F_FLOOD && options & F_INTERVAL) {
		(void)fprintf(stderr,
		    "ping: -f and -i incompatible options.\n");
		exit(1);
	}

	if (datalen >= sizeof(struct timeval))	/* can we time transfer */
		timing = 1;
	packlen = datalen + MAXIPLEN + MAXICMPLEN;
	if (!(packet = (u_char *)malloc((u_int)packlen))) {
		(void)fprintf(stderr, "ping: out of memory.\n");
		exit(1);
	}
	if (!(options & F_PINGFILLED))
		for (i = 8; i < datalen; ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;

	if (!(proto = getprotobyname("icmp"))) {
		(void)fprintf(stderr, "ping: unknown protocol icmp.\n");
		exit(1);
	}
#if defined(MINIX) || defined(AMOEBA)
#ifdef AMOEBA
	if ((ip_server= getenv("IP_SERVER")) == NULL) ip_server= IP_SVR_NAME;
	error= name_lookup(ip_server, &ip_cap);
	if (error != STD_OK)
	{
		fprintf(stderr, "ping: name_lookup %s: %s\n", 
			ip_server, tcpip_why(error));
		exit(1);
	}

	error= tcpip_open(&ip_cap, &chan_cap);
	if (error != STD_OK)
	{
		fprintf(stderr, "ping: open: %s\n", err_why(error));
		exit(1);
	}
#else
	if ((ip_device= getenv("IP_DEVICE")) == NULL) ip_device= IP_DEVICE;
	if ((s= open(ip_device, O_RDWR)) == -1)
	{
		fprintf(stderr, "ping: open '%s': %s\n", ip_device,
			strerror(errno));
		exit(1);
	}
#endif

	if (options & F_RROUTE) {
		ipopt.nwio_hdropt.iho_opt_siz= IP_MAX_HDR_SIZE-IP_MIN_HDR_SIZE;
		ipopt.nwio_hdropt.iho_data[0]= IPOPT_RR;
		ipopt.nwio_hdropt.iho_data[1]= ipopt.nwio_hdropt.iho_opt_siz-1;
		ipopt.nwio_hdropt.iho_data[2]= IPOPT_MINOFF;
	}
	else
		ipopt.nwio_hdropt.iho_opt_siz= 0;

	ipopt.nwio_flags= NWIO_COPY | NWIO_EN_LOC | NWIO_EN_BROAD | 
		NWIO_REMANY | NWIO_PROTOSPEC | NWIO_HDR_O_SPEC |
		NWIO_RWDATALL;
	ipopt.nwio_tos= 0;
	ipopt.nwio_ttl= 255;
	ipopt.nwio_df= 0;
	ipopt.nwio_proto= proto->p_proto;
#ifdef AMOEBA
	error= ip_ioc_setopt(&chan_cap, &ipopt);
	if (error != STD_OK)
	{
		fprintf(stderr, "ping: unable to set ip options: %s\n",
			tcpip_why(error));
		exit(1);
	}
#else
	if (ioctl(s, NWIOSIPOPT, &ipopt) == -1)
	{
		fprintf(stderr, "ping: unable to set ip options: %s\n",
			strerror(errno));
		exit(1);
	}
#endif

#else /* !MINIX */

	if ((s = socket(AF_INET, SOCK_RAW, proto->p_proto)) < 0) {
		perror("ping: socket");
		exit(1);
	}
	hold = 1;
	if (options & F_SO_DEBUG)
		(void)setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&hold,
		    sizeof(hold));
	if (options & F_SO_DONTROUTE)
		(void)setsockopt(s, SOL_SOCKET, SO_DONTROUTE, (char *)&hold,
		    sizeof(hold));

	/* record route option */
	if (options & F_RROUTE) {
#ifdef IP_OPTIONS
		rspace[IPOPT_OPTVAL] = IPOPT_RR;
		rspace[IPOPT_OLEN] = sizeof(rspace)-1;
		rspace[IPOPT_OFFSET] = IPOPT_MINOFF;
		if (setsockopt(s, IPPROTO_IP, IP_OPTIONS, rspace,
		    sizeof(rspace)) < 0) {
			perror("ping: record route");
			exit(1);
		}
#else
		(void)fprintf(stderr,
		  "ping: record route not available in this implementation.\n");
		exit(1);
#endif /* IP_OPTIONS */
	}
#endif /* MINIX */

	/*
	 * When pinging the broadcast address, you can get a lot of answers.
	 * Doing something so evil is useful if you are trying to stress the
	 * ethernet, or just want to fill the arp cache to get some stuff for
	 * /etc/ethers.
	 */
#if !defined(MINIX) && !defined(AMOEBA)
	hold = 48 * 1024;
	(void)setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&hold,
	    sizeof(hold));
#endif

	if (to->sin_family == AF_INET)
#ifdef AMOEBA
		(void)printf("PING %s (%s): %d data bytes\n", hostname,
		    inet_ntoa(to->sin_addr.s_addr),
		    datalen);
#else
		(void)printf("PING %s (%s): %d data bytes\n", hostname,
		    inet_ntoa(*(struct in_addr *)&to->sin_addr.s_addr),
		    datalen);
#endif
	else
		(void)printf("PING %s: %d data bytes\n", hostname, datalen);

	(void)signal(SIGINT, finish);
	(void)signal(SIGALRM, catcher);
#ifdef MINIX
	if (options & F_FLOOD)
		signal(SIGALRM, dummy_catcher);
#endif

	while (preload--)		/* fire off them quickies */
		pinger();

	if ((options & F_FLOOD) == 0)
		catcher();		/* start things going */

	for (;;) {
		struct sockaddr_in from;
		register int cc;
		int fromlen;

		if (options & F_FLOOD) {
			pinger();
#ifdef MINIX
			sysutime(UTIME_TIMEOFDAY, &timeout);
			if ((timeout.tv_usec += 10000) >= 1000000)
			{
				timeout.tv_sec++;
				timeout.tv_usec -= 1000000;
			}
			sysutime(UTIME_SETALARM, &timeout);
#else
#ifdef AMOEBA
			printf("abort %d\n", __LINE__);
			abort();
#else
			timeout.tv_sec = 0;
			timeout.tv_usec = 10000;
			fdmask = 1 << s;
			if (select(s + 1, (fd_set *)&fdmask, (fd_set *)NULL,
			    (fd_set *)NULL, &timeout) < 1)
				continue;
#endif /* AMOEBA */
#endif /* MINIX */
		}
#if defined(MINIX) || defined(AMOEBA)
#ifdef AMOEBA
		error= tcpip_read(&chan_cap, packet, packlen);
		cc= error;
		if (ERR_STATUS(error))
		{
			if (ERR_CONVERT(error) == STD_INTR)
				continue;
			fprintf(stderr, "ping: read: %s\n",
				err_why(ERR_CONVERT(error)));
			 continue;
		}
#else
		cc= read(s, packet, packlen);
		if (cc == -1)
		{	
			if (errno == EINTR)
				continue;
			perror("ping: read");
			continue;
		}
#endif
		ih= (ip_hdr_t *)packet;
		from.sin_len= sizeof(from);
		from.sin_family= AF_INET;
		from.sin_port= 0;
		from.sin_addr.s_addr= ih->ih_src;
#else
		fromlen = sizeof(from);
		if ((cc = recvfrom(s, (char *)packet, packlen, 0,
		    (struct sockaddr *)&from, &fromlen)) < 0) {
			if (errno == EINTR)
				continue;
			perror("ping: recvfrom");
			continue;
		}
#endif
		pr_pack((char *)packet, cc, &from);
		if (npackets && nreceived >= npackets)
			break;
	}
	finish();
	/* NOTREACHED */
}

#ifdef MINIX
void
dummy_catcher(sig)
int sig;
{
	signal(SIGALRM, dummy_catcher);
}
#endif

/*
 * catcher --
 *	This routine causes another PING to be transmitted, and then
 * schedules another SIGALRM for 1 second from now.
 *
 * bug --
 *	Our sense of time will slowly skew (i.e., packets will not be
 * launched exactly at 1-second intervals).  This does not affect the
 * quality of the delay and loss statistics.
 */
void
catcher(sig)
int sig;
{
	int waittime;

	pinger();
	(void)signal(SIGALRM, catcher);
	if (!npackets || ntransmitted < npackets)
		alarm((u_int)interval);
	else {
		if (nreceived) {
			waittime = 2 * tmax / 1000;
			if (!waittime)
				waittime = 1;
		} else
			waittime = MAXWAIT;
		(void)signal(SIGALRM, finish);
		(void)alarm((u_int)waittime);
	}
}

/*
 * pinger --
 *	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
pinger()
{
#if defined(MINIX) || defined(AMOEBA)
	struct ip_hdr *ih;
	register struct icmp_hdr *icp;
#else
	register struct icmp *icp;
#endif
	register int cc;
	int i;
#ifdef AMOEBA
	errstat error;
#endif

#if defined(MINIX) || defined(AMOEBA)
	icp = (struct icmp_hdr *)outpack;
#else
	icp = (struct icmp *)outpack;
#endif
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = ntransmitted++;
	icp->icmp_id = ident;			/* ID */

	CLR(icp->icmp_seq % mx_dup_ck);

	if (timing)
		(void)gettimeofday((struct timeval *)&outpack[8],
		    (struct timezone *)NULL);

	cc = datalen + 8;			/* skips ICMP portion */

	/* compute ICMP checksum here */
	icp->icmp_cksum = in_cksum((u_short *)icp, cc);

#if defined(MINIX) || defined(AMOEBA)
	ih= (ip_hdr_t *)ip_outpack;
	ih->ih_dst= ((struct sockaddr_in *)&whereto)->sin_addr.s_addr;

#ifdef AMOEBA
	error= tcpip_write(&chan_cap, ip_outpack, IP_MIN_HDR_SIZE + cc);
	i= error;
	if (ERR_STATUS(error) || i != IP_MIN_HDR_SIZE + cc)  {
		if (ERR_STATUS(error))
			fprintf(stderr, "ping: write: %s\n",
				tcpip_why(ERR_CONVERT(error)));
		(void)printf("ping: wrote %s %d chars, ret=%d\n",
		    hostname, cc, i);
	}
#else
	i = write(s, (char *)ip_outpack, IP_MIN_HDR_SIZE + cc);
	if (i < 0 || i != IP_MIN_HDR_SIZE + cc)  {
		if (i < 0)
			perror("ping: write");
		(void)printf("ping: wrote %s %d chars, ret=%d\n",
		    hostname, cc, i);
	}
#endif
#else
	i = sendto(s, (char *)outpack, cc, 0, &whereto,
	    sizeof(struct sockaddr));
	if (i < 0 || i != cc)  {
		if (i < 0)
			perror("ping: sendto");
		(void)printf("ping: wrote %s %d chars, ret=%d\n",
		    hostname, cc, i);
	}
#endif

	if (!(options & F_QUIET) && options & F_FLOOD)
		(void)write(STDOUT_FILENO, &DOT, 1);
}

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
pr_pack(buf, cc, from)
	char *buf;
	int cc;
	struct sockaddr_in *from;
{
#if defined(MINIX) || defined(AMOEBA)
	register struct icmp_hdr *icp;
#else
	register struct icmp *icp;
#endif
	register u_long l;
	register int i, j;
	register u_char *cp,*dp;
	static int old_rrlen;
	static char old_rr[MAX_IPOPTLEN];
#if defined(MINIX) || defined(AMOEBA)
	struct ip_hdr *ip;
#else
	struct ip *ip;
#endif
	struct timeval tv, *tp;
	double triptime;
	int hlen, dupflag;

	(void)gettimeofday(&tv, (struct timezone *)NULL);

	/* Check the IP header */
#if defined(MINIX) || defined(AMOEBA)
	ip = (struct ip_hdr *)buf;
	hlen= (ip->ih_vers_ihl & 0xf) << 2;
#else
	ip = (struct ip *)buf;
	hlen = ip->ip_hl << 2;
#endif

	if (cc < hlen + ICMP_MINLEN) {
		if (options & F_VERBOSE)
#ifdef AMOEBA
			(void)fprintf(stderr,
			  "ping: packet too short (%d bytes) from %s\n", cc,
			  inet_ntoa(from->sin_addr.s_addr));
#else
			(void)fprintf(stderr,
			  "ping: packet too short (%d bytes) from %s\n", cc,
			  inet_ntoa(*(struct in_addr *)&from->sin_addr.s_addr));
#endif
		return;
	}

	/* Now the ICMP part */
	cc -= hlen;
#if defined(MINIX) || defined(AMOEBA)
	icp = (struct icmp_hdr *)(buf + hlen);
#else
	icp = (struct icmp *)(buf + hlen);
#endif
	if (icp->icmp_type == ICMP_ECHOREPLY) {
		if (icp->icmp_id != ident)
			return;			/* 'Twas not our ECHO */
		++nreceived;
		if (timing) {
#ifndef icmp_data
			tp = (struct timeval *)&icp->icmp_ip;
#else
			tp = (struct timeval *)icp->icmp_data;
#endif
			tvsub(&tv, tp);
			triptime = ((double)tv.tv_sec) * 1000.0 +
			    ((double)tv.tv_usec) / 1000.0;
			tsum += triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(icp->icmp_seq % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(icp->icmp_seq % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_FLOOD)
			(void)write(STDOUT_FILENO, &BSPACE, 1);
		else {
#ifdef AMOEBA
			(void)printf("%d bytes from %s: icmp_seq=%u", cc,
			   inet_ntoa(from->sin_addr.s_addr),
			   icp->icmp_seq);
#else
			(void)printf("%d bytes from %s: icmp_seq=%u", cc,
			   inet_ntoa(*(struct in_addr *)&from->sin_addr.s_addr),
			   icp->icmp_seq);
#endif
			(void)printf(" ttl=%d", ip->ip_ttl);
			if (timing)
				(void)printf(" time=%g ms", triptime);
			if (dupflag)
				(void)printf(" (DUP!)");
			/* check the data */
			cp = (u_char*)&icp->icmp_data[8];
			dp = &outpack[8 + sizeof(struct timeval)];
			for (i = 8; i < datalen; ++i, ++cp, ++dp) {
				if (*cp != *dp) {
	(void)printf("\nwrong data byte #%d should be 0x%x but was 0x%x",
	    i, *dp, *cp);
					cp = (u_char*)&icp->icmp_data[0];
					for (i = 8; i < datalen; ++i, ++cp) {
						if ((i % 32) == 8)
							(void)printf("\n\t");
						(void)printf("%x ", *cp);
					}
					break;
				}
			}
		}
	} else {
		/* We've got something other than an ECHOREPLY */
		if (!(options & F_VERBOSE))
			return;
		(void)printf("%d bytes from %s: ", cc,
		    pr_addr(from->sin_addr.s_addr));
		pr_icmph(icp);
	}

	/* Display any IP options */
#if defined(MINIX) || defined(AMOEBA)
	cp = (u_char *)buf + sizeof(struct ip_hdr);

	for (; hlen > (int)sizeof(struct ip_hdr); --hlen, ++cp)
#else
	cp = (u_char *)buf + sizeof(struct ip);

	for (; hlen > (int)sizeof(struct ip); --hlen, ++cp)
#endif
		switch (*cp) {
		case IPOPT_EOL:
			hlen = 0;
			break;
		case IPOPT_LSRR:
			(void)printf("\nLSRR: ");
			hlen -= 2;
			j = *++cp;
			++cp;
			if (j > IPOPT_MINOFF)
				for (;;) {
					l = *++cp;
					l = (l<<8) + *++cp;
					l = (l<<8) + *++cp;
					l = (l<<8) + *++cp;
					if (l == 0)
						(void)printf("\t0.0.0.0");
				else
					(void)printf("\t%s", pr_addr(ntohl(l)));
				hlen -= 4;
				j -= 4;
				if (j <= IPOPT_MINOFF)
					break;
				(void)putchar('\n');
			}
			break;
		case IPOPT_RR:
			j = *++cp;		/* get length */
			i = *++cp;		/* and pointer */
			hlen -= 2;
			if (i > j)
				i = j;
			i -= IPOPT_MINOFF;
			if (i <= 0)
				continue;
			if (i == old_rrlen
#if defined(MINIX) || defined(AMOEBA)
			    && cp == (u_char *)buf + sizeof(struct ip_hdr) + 2
#else
			    && cp == (u_char *)buf + sizeof(struct ip) + 2
#endif
			    && !bcmp((char *)cp, old_rr, i)
			    && !(options & F_FLOOD)) {
				(void)printf("\t(same route)");
				i = ((i + 3) / 4) * 4;
				hlen -= i;
				cp += i;
				break;
			}
			old_rrlen = i;
			bcopy((char *)cp, old_rr, i);
			(void)printf("\nRR: ");
			for (;;) {
				l = *++cp;
				l = (l<<8) + *++cp;
				l = (l<<8) + *++cp;
				l = (l<<8) + *++cp;
				if (l == 0)
					(void)printf("\t0.0.0.0");
				else
					(void)printf("\t%s", pr_addr(ntohl(l)));
				hlen -= 4;
				i -= 4;
				if (i <= 0)
					break;
				(void)putchar('\n');
			}
			break;
		case IPOPT_NOP:
			(void)printf("\nNOP");
			break;
		default:
			(void)printf("\nunknown option %x", *cp);
			break;
		}
	if (!(options & F_FLOOD)) {
		(void)putchar('\n');
		(void)fflush(stdout);
	}
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */
in_cksum(addr, len)
	u_short *addr;
	int len;
{
	register int nleft = len;
	register u_short *w = addr;
	register int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}

/*
 * tvsub --
 *	Subtract 2 timeval structs:  out = out - in.  Out is assumed to
 * be >= in.
 */
tvsub(out, in)
	register struct timeval *out, *in;
{
	if ((out->tv_usec -= in->tv_usec) < 0) {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 * finish --
 *	Print out statistics, and give up.
 */
void
finish()
{
	register int i;

	(void)signal(SIGINT, SIG_IGN);
#ifdef AMOEBA
	std_destroy(&chan_cap);
#endif
	(void)putchar('\n');
	(void)fflush(stdout);
	(void)printf("--- %s ping statistics ---\n", hostname);
	(void)printf("%ld packets transmitted, ", ntransmitted);
	(void)printf("%ld packets received, ", nreceived);
	if (nrepeats)
		(void)printf("+%ld duplicates, ", nrepeats);
	if (ntransmitted)
		if (nreceived > ntransmitted)
			(void)printf("-- somebody's printing up packets!");
		else
			(void)printf("%d%% packet loss",
			    (int) (((ntransmitted - nreceived) * 100) /
			    ntransmitted));
	(void)putchar('\n');
	if (nreceived && timing) {
		/* Only display average to microseconds */
		i = 1000.0 * tsum / (nreceived + nrepeats);
		(void)printf("round-trip min/avg/max = %g/%g/%g ms\n",
		    tmin, ((double)i) / 1000.0, tmax);
	}
	exit(0);
}

#ifdef notdef
static char *ttab[] = {
	"Echo Reply",		/* ip + seq + udata */
	"Dest Unreachable",	/* net, host, proto, port, frag, sr + IP */
	"Source Quench",	/* IP */
	"Redirect",		/* redirect type, gateway, + IP  */
	"Echo",
	"Time Exceeded",	/* transit, frag reassem + IP */
	"Parameter Problem",	/* pointer + IP */
	"Timestamp",		/* id + seq + three timestamps */
	"Timestamp Reply",	/* " */
	"Info Request",		/* id + sq */
	"Info Reply"		/* " */
};
#endif

/*
 * pr_icmph --
 *	Print a descriptive string about an ICMP header.
 */
pr_icmph(icp)
#if defined(MINIX) || defined(AMOEBA)
	struct icmp_hdr *icp;
#else
	struct icmp *icp;
#endif
{
	switch(icp->icmp_type) {
	case ICMP_ECHOREPLY:
		(void)printf("Echo Reply\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_UNREACH:
		switch(icp->icmp_code) {
		case ICMP_UNREACH_NET:
			(void)printf("Destination Net Unreachable\n");
			break;
		case ICMP_UNREACH_HOST:
			(void)printf("Destination Host Unreachable\n");
			break;
		case ICMP_UNREACH_PROTOCOL:
			(void)printf("Destination Protocol Unreachable\n");
			break;
		case ICMP_UNREACH_PORT:
			(void)printf("Destination Port Unreachable\n");
			break;
		case ICMP_UNREACH_NEEDFRAG:
			(void)printf("frag needed and DF set\n");
			break;
		case ICMP_UNREACH_SRCFAIL:
			(void)printf("Source Route Failed\n");
			break;
		default:
			(void)printf("Dest Unreachable, Bad Code: %d\n",
			    icp->icmp_code);
			break;
		}
		/* Print returned IP header information */
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_SOURCEQUENCH:
		(void)printf("Source Quench\n");
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_REDIRECT:
		switch(icp->icmp_code) {
		case ICMP_REDIRECT_NET:
			(void)printf("Redirect Network");
			break;
		case ICMP_REDIRECT_HOST:
			(void)printf("Redirect Host");
			break;
		case ICMP_REDIRECT_TOSNET:
			(void)printf("Redirect Type of Service and Network");
			break;
		case ICMP_REDIRECT_TOSHOST:
			(void)printf("Redirect Type of Service and Host");
			break;
		default:
			(void)printf("Redirect, Bad Code: %d", icp->icmp_code);
			break;
		}
#if defined(MINIX) || defined(AMOEBA)
		(void)printf("(New addr: 0x%08lx)\n", icp->icmp_gwaddr);
#else
		(void)printf("(New addr: 0x%08lx)\n", icp->icmp_gwaddr.s_addr);
#endif
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_ECHO:
		(void)printf("Echo Request\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_TIMXCEED:
		switch(icp->icmp_code) {
		case ICMP_TIMXCEED_INTRANS:
			(void)printf("Time to live exceeded\n");
			break;
		case ICMP_TIMXCEED_REASS:
			(void)printf("Frag reassembly time exceeded\n");
			break;
		default:
			(void)printf("Time exceeded, Bad Code: %d\n",
			    icp->icmp_code);
			break;
		}
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_PARAMPROB:
#if defined(MINIX) || defined(AMOEBA)
		(void)printf("Parameter problem: pointer = 0x%02x\n",
		    icp->ih_hun.ihh_pp.ipp_ptr);
#else
		(void)printf("Parameter problem: pointer = 0x%02x\n",
		    icp->icmp_hun.ih_pptr);
#endif
#ifndef icmp_data
		pr_retip(&icp->icmp_ip);
#else
		pr_retip((struct ip *)icp->icmp_data);
#endif
		break;
	case ICMP_TSTAMP:
		(void)printf("Timestamp\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_TSTAMPREPLY:
		(void)printf("Timestamp Reply\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_IREQ:
		(void)printf("Information Request\n");
		/* XXX ID + Seq */
		break;
	case ICMP_IREQREPLY:
		(void)printf("Information Reply\n");
		/* XXX ID + Seq */
		break;
#ifdef ICMP_MASKREQ
	case ICMP_MASKREQ:
		(void)printf("Address Mask Request\n");
		break;
#endif
#ifdef ICMP_MASKREPLY
	case ICMP_MASKREPLY:
		(void)printf("Address Mask Reply\n");
		break;
#endif
	default:
		(void)printf("Bad ICMP type: %d\n", icp->icmp_type);
	}
}

/*
 * pr_iph --
 *	Print an IP header with options.
 */
pr_iph(ip)
#if defined(MINIX) || defined(AMOEBA)
	struct ip_hdr *ip;
#else
	struct ip *ip;
#endif
{
	int hlen;
	u_char *cp;

#if defined(MINIX) || defined(AMOEBA)
	hlen = (ip->ih_vers_ihl & 0xf) << 2;
#else
	hlen = ip->ip_hl << 2;
#endif
	cp = (u_char *)ip + 20;		/* point to options */

	(void)printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src      Dst Data\n");
#if defined(MINIX) || defined(AMOEBA)
	(void)printf(" %1x  %1x  %02x %04x %04x",
	    ip->ih_vers_ihl >> 4, ip->ih_vers_ihl & 0xf, ip->ih_tos,
	    ip->ih_length, ip->ih_id);
#else
	(void)printf(" %1x  %1x  %02x %04x %04x",
	    ip->ip_v, ip->ip_hl, ip->ip_tos, ip->ip_len, ip->ip_id);
#endif
	(void)printf("   %1x %04x", ((ip->ip_off) & 0xe000) >> 13,
	    (ip->ip_off) & 0x1fff);
	(void)printf("  %02x  %02x %04x", ip->ip_ttl, ip->ip_p, ip->ip_sum);
#ifdef MINIX
	(void)printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_src));
	(void)printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_dst));
#else
	(void)printf(" %s ", inet_ntoa(ip->ip_src));
	(void)printf(" %s ", inet_ntoa(ip->ip_dst));
#ifdef AMOEBA
#else
	(void)printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_src.s_addr));
	(void)printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_dst.s_addr));
#endif /* AMOEBA */
#endif /* MINIX */
	/* dump and option bytes */
	while (hlen-- > 20) {
		(void)printf("%02x", *cp++);
	}
	(void)putchar('\n');
}

/*
 * pr_addr --
 *	Return an ascii host address as a dotted quad and optionally with
 * a hostname.
 */
char *
pr_addr(l)
	u_long l;
{
	struct hostent *hp;
	static char buf[80];

#ifdef AMOEBA
	if ((options & F_NUMERIC) ||
	    !(hp = gethostbyaddr((char *)&l, 4, AF_INET)))
		(void)sprintf(buf, "%s", inet_ntoa(l));
	else
		(void)sprintf(buf, "%s (%s)", hp->h_name,
		    inet_ntoa(l));
#else
	if ((options & F_NUMERIC) ||
	    !(hp = gethostbyaddr((char *)&l, 4, AF_INET)))
		(void)sprintf(buf, "%s", inet_ntoa(*(struct in_addr *)&l));
	else
		(void)sprintf(buf, "%s (%s)", hp->h_name,
		    inet_ntoa(*(struct in_addr *)&l));
#endif
	return(buf);
}

/*
 * pr_retip --
 *	Dump some info on a returned (via ICMP) IP packet.
 */
pr_retip(ip)
#if defined(MINIX) || defined(AMOEBA)
	struct ip_hdr *ip;
#else
	struct ip *ip;
#endif
{
	int hlen;
	u_char *cp;

	pr_iph(ip);
#if defined(MINIX) || defined(AMOEBA)
	hlen = (ip->ih_vers_ihl & 0xf) << 2;
#else
	hlen = ip->ip_hl << 2;
#endif
	cp = (u_char *)ip + hlen;

	if (ip->ip_p == 6)
		(void)printf("TCP: from port %u, to port %u (decimal)\n",
		    (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
	else if (ip->ip_p == 17)
		(void)printf("UDP: from port %u, to port %u (decimal)\n",
			(*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
}

fill(bp, patp)
	char *bp, *patp;
{
	register int ii, jj, kk;
	int pat[16];
	char *cp;

	for (cp = patp; *cp; cp++)
		if (!isxdigit(*cp)) {
			(void)fprintf(stderr,
			    "ping: patterns must be specified as hex digits.\n");
			exit(1);
		}
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

	if (ii > 0)
		for (kk = 0;
		    kk <= MAXPACKET - (8 + sizeof(struct timeval) + ii);
		    kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	if (!(options & F_QUIET)) {
		(void)printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			(void)printf("%02x", bp[jj] & 0xFF);
		(void)printf("\n");
	}
}

usage()
{
	(void)fprintf(stderr,
	    "usage: ping [-Rdfnqrv] [-c count] [-i wait] [-l preload]\n\t[-p pattern] [-s packetsize] host\n");
	exit(1);
}
