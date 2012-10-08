/*	@(#)ftp.c	1.1	96/02/27 13:08:56 */
/*
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)ftp.c	5.38 (Berkeley) 4/22/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef AMOEBA
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif
#include <sys/time.h>
#include <sys/file.h>

#ifndef AMOEBA
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/ftp.h>
#include <arpa/telnet.h>
#else
#define  port am_port_t
#define  command am_command_t
#include <amoeba.h>
#include <ampolicy.h>
#include <amtools.h>
#include <stderr.h>
#include <thread.h>
#include <semaphore.h>
#undef   port
#undef   command

#include <module/name.h>
#include <stdlib.h>
#include <string.h>
#define rindex strrchr
#define index  strchr

#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/tcp_io.h>
#include <server/ip/hton.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/tcp.h>
#include <server/ip/gen/tcp_io.h>
#include <module/ar.h>

#include "ftp.h"

#define bufsize buf_size /* avoid SunOs cc complaint */

/* Next defines should come from a include file, really */
#define IAC     255             /* interpret as command: */
#define DONT    254             /* you are not to use option */
#define DO      253             /* please, you use option */
#define WONT    252             /* I won't use option */
#define WILL    251             /* I will use option */
#define IP	244		/* interrupt process */
#define DM	242		/* data mark for connection cleaning */
#endif /* AMOEBA */

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#ifndef AMOEBA
#include <netdb.h>
#endif
#include <fcntl.h>
#include <pwd.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ftp_var.h"

#ifndef AMOEBA
struct	sockaddr_in hisctladdr;
struct	sockaddr_in data_addr;
#else
capability hisctlcap, data_cap, cw_cap, tcp_cap;
nwio_tcpconf_t hisconf, dataconf, cw_conf;
nwio_tcpcl_t tcpconnopt;
char *host_name;
char *tcp_cap_name, *port_name;
extern char *prog_name;

typedef void (*sig_t)();
#endif

int	data = -1;
int	abrtflag = 0;
int	ptflag = 0;
#ifndef AMOEBA
struct	sockaddr_in myctladdr;
#endif
uid_t	getuid();
sig_t	lostpeer();
off_t	restart_point = 0;

extern char *strerror();
extern int connected, errno;

#ifndef AMOEBA
FILE	*cin, *cout;
FILE	*dataconn();
#else
int	dataconn();

static am_port_t null_port;

void
invalidate_cap(cap)
capability *cap;
{
    cap->cap_port = null_port;
}

int
valid_cap(cap)
capability *cap;
{
    return !NULLPORT(&cap->cap_port);
}

void
destroy_cap(cap)
capability *cap;
{
    errstat err;

    if (valid_cap(cap)) {
	/* printf("shutting down %s\n", ar_cap(cap)); */
	if ((err = tcp_ioc_shutdown(cap)) != STD_OK) {
	    fprintf(stderr, "ftpsvr: tcp_ioc_shutdown failed (%s)\n",
		    tcpip_why(err));
	}
	err = std_destroy(cap);
	if (err != STD_OK) {
	    fprintf(stderr, "cannot destroy connection cap (%s)\n",
		    err_why(err));
	}
	invalidate_cap(cap);
    }
}

#endif

char *
hookup(host, port)
	char *host;
	int port;
{
#ifndef AMOEBA
	register struct hostent *hp = 0;
	int s, len, tos;
	static char hostnamebuf[80];

	bzero((char *)&hisctladdr, sizeof (hisctladdr));
	hisctladdr.sin_addr.s_addr = inet_addr(host);
	if (hisctladdr.sin_addr.s_addr != -1) {
		hisctladdr.sin_family = AF_INET;
		(void) strncpy(hostnamebuf, host, sizeof(hostnamebuf));
	} else {
		hp = gethostbyname(host);
		if (hp == NULL) {
			fprintf(stderr, "ftp: %s: ", host);
			herror((char *)NULL);
			code = -1;
			return((char *) 0);
		}
		hisctladdr.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr_list[0],
		    (caddr_t)&hisctladdr.sin_addr, hp->h_length);
		(void) strncpy(hostnamebuf, hp->h_name, sizeof(hostnamebuf));
	}
	hostname = hostnamebuf;
	s = socket(hisctladdr.sin_family, SOCK_STREAM, 0);
	if (s < 0) {
		perror("ftp: socket");
		code = -1;
		return (0);
	}
	hisctladdr.sin_port = port;
	while (connect(s, (struct sockaddr *)&hisctladdr, sizeof (hisctladdr)) < 0) {
		if (hp && hp->h_addr_list[1]) {
			int oerrno = errno;
			extern char *inet_ntoa();

			fprintf(stderr, "ftp: connect to address %s: ",
				inet_ntoa(hisctladdr.sin_addr));
			errno = oerrno;
			perror((char *) 0);
			hp->h_addr_list++;
			bcopy(hp->h_addr_list[0],
			     (caddr_t)&hisctladdr.sin_addr, hp->h_length);
			fprintf(stdout, "Trying %s...\n",
				inet_ntoa(hisctladdr.sin_addr));
			(void) close(s);
			s = socket(hisctladdr.sin_family, SOCK_STREAM, 0);
			if (s < 0) {
				perror("ftp: socket");
				code = -1;
				return (0);
			}
			continue;
		}
		perror("ftp: connect");
		code = -1;
		goto bad;
	}
	len = sizeof (myctladdr);
	if (getsockname(s, (struct sockaddr *)&myctladdr, &len) < 0) {
		perror("ftp: getsockname");
		code = -1;
		goto bad;
	}
#ifdef IP_TOS
	tos = IPTOS_LOWDELAY;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		perror("ftp: setsockopt TOS (ignored)");
#endif
	cin = fdopen(s, "r");
	cout = fdopen(s, "w");
	if (cin == NULL || cout == NULL) {
		fprintf(stderr, "ftp: fdopen failed.\n");
		if (cin)
			(void) fclose(cin);
		if (cout)
			(void) fclose(cout);
		code = -1;
		goto bad;
	}
	if (verbose)
		printf("Connected to %s.\n", hostname);
	if (getreply(0) > 2) { 	/* read startup message from server */
		if (cin)
			(void) fclose(cin);
		if (cout)
			(void) fclose(cout);
		code = -1;
		goto bad;
	}
#ifdef SO_OOBINLINE
	{
	int on = 1;

	if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on))
		< 0 && debug) {
			perror("ftp: setsockopt");
		}
	}
#endif /* SO_OOBINLINE */

	return (hostname);
bad:
	(void) close(s);
	return ((char *)0);
#else  /* AMOEBA */
	register struct hostent *hp = 0;
	static char hostnamebuf[80];
	errstat err;
  
	host_name = host;
 
	if (!tcp_cap_name) {
	    tcp_cap_name = getenv("TCP_SERVER");
	    if (tcp_cap_name && !tcp_cap_name[0])
		tcp_cap_name = NULL;
	}
	if (!tcp_cap_name)
	    tcp_cap_name = TCP_SVR_NAME;
 
	hp = gethostbyname(host_name);
	if (!hp) {
	    fprintf(stderr, "%s: unknown host '%s'\n",
		    prog_name, host_name);
	    code = -1;
	    return NULL;
	}
	(void) strncpy(hostnamebuf, hp->h_name, sizeof(hostnamebuf));
	hostname = hostnamebuf;
  
	/* printf("connecting to %s %u\n", host_name, ntohs(port)); */

	err = name_lookup(tcp_cap_name, &tcp_cap);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: cannot lookup tcpip cap %s (%s)\n",
		    prog_name, tcp_cap_name, err_why(err));
	    return NULL;
	}
	err = tcpip_open (&tcp_cap, &hisctlcap);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: tcpip_open failed (%s)\n",
		    prog_name, err_why(err));
	    return NULL;
	}
 
	hisconf.nwtc_flags = NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
	hisconf.nwtc_remaddr = *(ipaddr_t *)(hp->h_addr);
	hisconf.nwtc_remport = port;
 
	err = tcp_ioc_setconf (&hisctlcap, &hisconf);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: tcp_ioc_setconf failed (%s)\n",
		    prog_name, err_why(err));
	    destroy_cap(&hisctlcap);
	    return NULL;
	}
	/* printf("tcp_ioc_setconf succeeded\n"); */
	err = tcpip_keepalive_cap(&hisctlcap);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: tcpip_keepalive_cap failed (%s)\n",
		    prog_name, err_why(err));
	    destroy_cap(&hisctlcap);
	    return NULL;
	}
 
	tcpconnopt.nwtcl_flags= 0;
	tcpconnopt.nwtcl_ttl= 0xffff; /* ? */
	do {
	    err = tcp_ioc_connect (&hisctlcap, &tcpconnopt);
	    if (err == EAGAIN) {
		fprintf(stderr, "tcp_ioc_connect: got EAGAIN, sleeping 1s\n");
		sleep(1);
	    }
	} while (err == EAGAIN);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: tcp_ioc_connect failed (%s)\n",
		    prog_name, err_why(err));
	    destroy_cap(&hisctlcap);
	    return NULL;
	}
	/* if (verbose) printf("tcp_ioc_connect succeeded\r\n"); */
	cw_cap = hisctlcap;
 
	if (verbose)
		printf("Connected to %s.\n", hostname);
	if (getreply(0) > 2) {  /* read startup message from server */
	    destroy_cap(&cw_cap);
	    code = -1;
	    return NULL;
	}
 
	return hostname;
#endif /* AMOEBA */
}

#ifdef __STDC__
int command(char *fmt, ...);
#endif

login(host)
	char *host;
{
	char tmp[80];
	char *user, *pass, *acct, *getlogin(), *getpass();
	int n, aflag = 0;

	user = pass = acct = 0;
	if (ruserpass(host, &user, &pass, &acct) < 0) {
		code = -1;
		return(0);
	}
	while (user == NULL) {
		char *myname = getlogin();

		if (myname == NULL) {
			struct passwd *pp = getpwuid(getuid());

			if (pp != NULL)
				myname = pp->pw_name;
		}
		if (myname)
			printf("Name (%s:%s): ", host, myname);
		else
			printf("Name (%s): ", host);
		(void) fgets(tmp, sizeof(tmp) - 1, stdin);
		tmp[strlen(tmp) - 1] = '\0';
		if (*tmp == '\0')
			user = myname;
		else
			user = tmp;
	}
	n = command("USER %s", user);
	if (n == CONTINUE) {
		if (pass == NULL)
			pass = getpass("Password:");
		n = command("PASS %s", pass);
	}
	if (n == CONTINUE) {
		aflag++;
		acct = getpass("Account:");
		n = command("ACCT %s", acct);
	}
	if (n != COMPLETE) {
		fprintf(stderr, "Login failed.\n");
		return (0);
	}
	if (!aflag && acct != NULL)
		(void) command("ACCT %s", acct);
	if (proxy)
		return(1);
	for (n = 0; n < macnum; ++n) {
		if (!strcmp("init", macros[n].mac_name)) {
			(void) strcpy(line, "$init");
			makeargv();
			domacro(margc, margv);
			break;
		}
	}
	return (1);
}

void
cmdabort()
{
	extern jmp_buf ptabort;

	printf("\n");
	(void) fflush(stdout);
	abrtflag++;
	if (ptflag)
		longjmp(ptabort,1);
}

/*VARARGS*/
#ifdef __STDC__
command(char *fmt, ...)
#else
command(fmt, va_alist) char *fmt; va_dcl
#endif
{
	va_list ap;
	int r;
	sig_t oldintr;
#ifdef AMOEBA
	char tbuffer[BUFSIZ];
#endif
	void cmdabort();

	abrtflag = 0;
	if (debug) {
		printf("---> ");
#ifdef __STDC__
		va_start(ap, fmt);
#else
		va_start(ap);
#endif
		if (strncmp("PASS ", fmt, 5) == 0)
			printf("PASS XXXX");
		else 
			vfprintf(stdout, fmt, ap);
		va_end(ap);
		printf("\n");
		(void) fflush(stdout);
	}
#ifndef AMOEBA
	if (cout == NULL) {
#else
	if (!valid_cap(&cw_cap)) {
#endif
		perror ("No control connection for command");
		code = -1;
		return (0);
	}
	oldintr = signal(SIGINT, cmdabort);
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
#ifndef AMOEBA
	vfprintf(cout, fmt, ap);
#else
	vsprintf(tbuffer, fmt, ap);
	tcpip_write(&cw_cap, tbuffer, strlen(tbuffer));
#endif
	va_end(ap);
#ifndef AMOEBA
	fprintf(cout, "\r\n");
	(void) fflush(cout);
#else
	tcpip_write(&cw_cap, "\r\n", 2);
#endif
	cpend = 1;
	r = getreply(!strcmp(fmt, "QUIT"));
	if (abrtflag && oldintr != SIG_IGN)
		(*oldintr)(SIGINT);
	(void) signal(SIGINT, oldintr);
	return(r);
}

#ifdef AMOEBA
int
tcpip_getc(cap)
capability *cap;
{
    char c;
  
    if (tcpip_read(cap, &c, 1) != 1) {
	return EOF;
    } else {
	return c & 0xFF;
    }
}
#endif

char reply_string[BUFSIZ];		/* last line of previous reply */

#include <ctype.h>

getreply(expecteof)
	int expecteof;
{
	register int c, n;
	register int dig;
	register char *cp;
	int originalcode = 0, continuation = 0;
	sig_t oldintr;
	int pflag = 0;
	char *pt = pasv;
	void cmdabort();

	oldintr = signal(SIGINT, cmdabort);
	for (;;) {
		dig = n = code = 0;
		cp = reply_string;
#ifndef AMOEBA
		while ((c = getc(cin)) != '\n') {
			if (c == IAC) {     /* handle telnet commands */
				switch (c = getc(cin)) {
				case WILL:
				case WONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, DONT, c);
					(void) fflush(cout);
					break;
				case DO:
				case DONT:
					c = getc(cin);
					fprintf(cout, "%c%c%c", IAC, WONT, c);
					(void) fflush(cout);
					break;
				default:
					break;
				}
				continue;
			}
#else
		while ((c = tcpip_getc(&cw_cap)) != '\n') {
			char tbuf[4];

			if (c == IAC) {     /* handle telnet commands */
				switch (c = tcpip_getc(&cw_cap)) {
				case WILL:
				case WONT:
					c = tcpip_getc(&cw_cap);
					sprintf(tbuf, "%c%c%c", IAC, DONT, c);
					(void) tcpip_write(&cw_cap, tbuf, 3);
					break;
				case DO:
				case DONT:
					c = tcpip_getc(&cw_cap);
					sprintf(tbuf, "%c%c%c", IAC, WONT, c);
					(void) tcpip_write(&cw_cap, tbuf, 3);
					break;
				default:
					break;
				}
				continue;
			}
#endif /* AMOEBA */
			dig++;
			if (c == EOF) {
				if (expecteof) {
					(void) signal(SIGINT,oldintr);
					code = 221;
					return (0);
				}
				lostpeer();
				if (verbose) {
					printf("421 Service not available, remote server has closed connection\n");
					(void) fflush(stdout);
				}
				code = 421;
				return(4);
			}
			if (c != '\r' && (verbose > 0 ||
			    (verbose > -1 && n == '5' && dig > 4))) {
				if (proxflag &&
				   (dig == 1 || dig == 5 && verbose == 0))
					printf("%s:",hostname);
				(void) putchar(c);
			}
			if (dig < 4 && isdigit(c))
				code = code * 10 + (c - '0');
			if (!pflag && code == 227)
				pflag = 1;
			if (dig > 4 && pflag == 1 && isdigit(c))
				pflag = 2;
			if (pflag == 2) {
				if (c != '\r' && c != ')')
					*pt++ = c;
				else {
					*pt = '\0';
					pflag = 3;
				}
			}
			if (dig == 4 && c == '-') {
				if (continuation)
					code = 0;
				continuation++;
			}
			if (n == 0)
				n = c;
			if (cp < &reply_string[sizeof(reply_string) - 1])
				*cp++ = c;
		}
		if (verbose > 0 || verbose > -1 && n == '5') {
			(void) putchar(c);
			(void) fflush (stdout);
		}
		if (continuation && code != originalcode) {
			if (originalcode == 0)
				originalcode = code;
			continue;
		}
		*cp = '\0';
		if (n != '1')
			cpend = 0;
		(void) signal(SIGINT,oldintr);
		if (code == 421 || originalcode == 421)
			lostpeer();
		if (abrtflag && oldintr != cmdabort && oldintr != SIG_IGN)
			(*oldintr)(SIGINT);
		return (n - '0');
	}
}

#ifndef AMOEBA
empty(mask, sec)
	struct fd_set *mask;
	int sec;
{
	struct timeval t;

	t.tv_sec = (long) sec;
	t.tv_usec = 0;
	return(select(32, mask, (struct fd_set *) 0, (struct fd_set *) 0, &t));
}
#endif

jmp_buf	sendabort;

void
abortsend()
{

	mflag = 0;
	abrtflag = 0;
	printf("\nsend aborted\nwaiting for remote to finish abort\n");
	(void) fflush(stdout);
	longjmp(sendabort, 1);
}

#define HASHBYTES 1024

sendrequest(cmd, local, remote, printnames)
	char *cmd, *local, *remote;
	int printnames;
{
	struct stat st;
	struct timeval start, stop;
	register int c, d;
#ifndef AMOEBA
	FILE * dout = 0;
#endif
	FILE *fin, *popen();
	int (*closefunc)(), pclose(), fclose();
	sig_t oldintr, oldintp;
	long bytes = 0, hashbytes = HASHBYTES;
	char *lmode, buf[BUFSIZ], *bufp;
	void abortsend();

	if (verbose && printnames) {
		if (local && *local != '-')
			printf("local: %s ", local);
		if (remote)
			printf("remote: %s\n", remote);
	}
	if (proxy) {
#ifndef AMOEBA
		proxtrans(cmd, local, remote);
#else
		fprintf(stderr, "sendrequest: proxtrans not implemented\n");
#endif
		return;
	}
	if (curtype != type)
		changetype(type, 0);
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	lmode = "w";
	if (setjmp(sendabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT,oldintr);
		if (oldintp)
			(void) signal(SIGPIPE,oldintp);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortsend);
	if (strcmp(local, "-") == 0)
		fin = stdin;
	else if (*local == '|') {
		oldintp = signal(SIGPIPE,SIG_IGN);
		fin = popen(local + 1, "r");
		if (fin == NULL) {
			perror(local + 1);
			(void) signal(SIGINT, oldintr);
			(void) signal(SIGPIPE, oldintp);
			code = -1;
			return;
		}
		closefunc = pclose;
	} else {
		fin = fopen(local, "r");
		if (fin == NULL) {
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
		closefunc = fclose;
		if (fstat(fileno(fin), &st) < 0 ||
		    (st.st_mode&S_IFMT) != S_IFREG) {
			fprintf(stdout, "%s: not a plain file.\n", local);
			(void) signal(SIGINT, oldintr);
			fclose(fin);
			code = -1;
			return;
		}
	}
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		if (oldintp)
			(void) signal(SIGPIPE, oldintp);
		code = -1;
		if (closefunc != NULL)
			(*closefunc)(fin);
		return;
	}
	if (setjmp(sendabort))
		goto abort;

	if (restart_point &&
	    (strcmp(cmd, "STOR") == 0 || strcmp(cmd, "APPE") == 0)) {
		if (fseek(fin, (long) restart_point, 0) < 0) {
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
			restart_point = 0;
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
		if (command("REST %ld", (long) restart_point)
			!= CONTINUE) {
			restart_point = 0;
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
		restart_point = 0;
		lmode = "r+w";
	}
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
	} else
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			if (oldintp)
				(void) signal(SIGPIPE, oldintp);
			if (closefunc != NULL)
				(*closefunc)(fin);
			return;
		}
#ifndef AMOEBA
	dout = dataconn(lmode);
	if (dout == NULL)
		goto abort;
#else
	if (!dataconn(lmode)) {
		goto abort;
	}
#endif
	(void) gettimeofday(&start, (struct timezone *)0);
	oldintp = signal(SIGPIPE, SIG_IGN);
	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		errno = d = 0;
		while ((c = read(fileno(fin), buf, sizeof (buf))) > 0) {
			bytes += c;
			for (bufp = buf; c > 0; c -= d, bufp += d)
#ifndef AMOEBA
				if ((d = write(fileno(dout), bufp, c)) <= 0)
					break;
#else
				if ((d = tcpip_write(&data_cap, bufp, c)) < 0)
					break;
#endif
			if (hash) {
				while (bytes >= hashbytes) {
					(void) putchar('#');
					hashbytes += HASHBYTES;
				}
				(void) fflush(stdout);
			}
		}
		if (hash && bytes > 0) {
			if (bytes < HASHBYTES)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (c < 0)
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
		if (d < 0) {
			if (errno != EPIPE) 
				perror("netout");
			bytes = -1;
		}
		break;

	case TYPE_A:
		while ((c = getc(fin)) != EOF) {
			if (c == '\n') {
				while (hash && (bytes >= hashbytes)) {
					(void) putchar('#');
					(void) fflush(stdout);
					hashbytes += HASHBYTES;
				}
#ifndef AMOEBA
				if (ferror(dout))
					break;
				(void) putc('\r', dout);
#else
				(void) tcpip_write(&data_cap, "\r", 1);
#endif
				bytes++;
			}
#ifndef AMOEBA
			(void) putc(c, dout);
#else
			{
			    char ch = c;
			    (void) tcpip_write(&data_cap, &ch, sizeof(ch));
			}
#endif
			bytes++;
	/*		if (c == '\r') {			  	*/
	/*		(void)	putc('\0', dout);  /* this violates rfc */
	/*			bytes++;				*/
	/*		}                          			*/	
		}
		if (hash) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (ferror(fin))
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
#ifndef AMOEBA
		if (ferror(dout)) {
			if (errno != EPIPE)
				perror("netout");
			bytes = -1;
		}
#endif
		break;
	}
	(void) gettimeofday(&stop, (struct timezone *)0);
	if (closefunc != NULL)
		(*closefunc)(fin);
#ifndef AMOEBA
	(void) fclose(dout);
#else
	destroy_cap(&data_cap);
#endif
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	if (bytes > 0)
		ptransfer("sent", bytes, &start, &stop);
	return;
abort:
	(void) gettimeofday(&stop, (struct timezone *)0);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	if (!cpend) {
		code = -1;
		return;
	}
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
#ifndef AMOEBA
	if (dout)
		(void) fclose(dout);
#endif
	(void) getreply(0);
	code = -1;
	if (closefunc != NULL && fin != NULL)
		(*closefunc)(fin);
	if (bytes > 0)
		ptransfer("sent", bytes, &start, &stop);
}

jmp_buf	recvabort;

void
abortrecv()
{

	mflag = 0;
	abrtflag = 0;
	printf("\nreceive aborted\nwaiting for remote to finish abort\n");
	(void) fflush(stdout);
	longjmp(recvabort, 1);
}

recvrequest(cmd, local, remote, lmode, printnames)
	char *cmd, *local, *remote, *lmode;
{
	FILE *fout, *din = 0, *popen();
	int (*closefunc)(), pclose(), fclose();
	sig_t oldintr, oldintp;
	int is_retr, tcrflag, bare_lfs = 0;
	char *gunique();
	static int bufsize;
	static char *buf;
	long bytes = 0, hashbytes = HASHBYTES;
	register int c, d;
	struct timeval start, stop;
	struct stat st;
	off_t lseek();
	void abortrecv();
#ifndef AMOEBA
	char *malloc();
#endif

	is_retr = strcmp(cmd, "RETR") == 0;
	if (is_retr && verbose && printnames) {
		if (local && *local != '-')
			printf("local: %s ", local);
		if (remote)
			printf("remote: %s\n", remote);
	}
	if (proxy && is_retr) {
#ifndef AMOEBA
		proxtrans(cmd, local, remote);
#else
		fprintf(stderr, "recrequest: proxtrans not implemented\n");
#endif
		return;
	}
	closefunc = NULL;
	oldintr = NULL;
	oldintp = NULL;
	tcrflag = !crflag && is_retr;
	if (setjmp(recvabort)) {
		while (cpend) {
			(void) getreply(0);
		}
		if (data >= 0) {
			(void) close(data);
			data = -1;
		}
		if (oldintr)
			(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	oldintr = signal(SIGINT, abortrecv);
	if (strcmp(local, "-") && *local != '|') {
		if (access(local, 2) < 0) {
			char *dir = rindex(local, '/');

			if (errno != ENOENT && errno != EACCES) {
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (dir != NULL)
				*dir = 0;
			d = access(dir ? local : ".", 2);
			if (dir != NULL)
				*dir = '/';
			if (d < 0) {
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (!runique && errno == EACCES &&
			    chmod(local, 0600) < 0) {
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
				(void) signal(SIGINT, oldintr);
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
			if (runique && errno == EACCES &&
			   (local = gunique(local)) == NULL) {
				(void) signal(SIGINT, oldintr);
				code = -1;
				return;
			}
		}
		else if (runique && (local = gunique(local)) == NULL) {
			(void) signal(SIGINT, oldintr);
			code = -1;
			return;
		}
	}
	if (!is_retr) {
		if (curtype != TYPE_A)
			changetype(TYPE_A, 0);
	} else if (curtype != type)
		changetype(type, 0);
	if (initconn()) {
		(void) signal(SIGINT, oldintr);
		code = -1;
		return;
	}
	if (setjmp(recvabort))
		goto abort;
	if (is_retr && restart_point &&
	    command("REST %ld", (long) restart_point) != CONTINUE)
		return;
	if (remote) {
		if (command("%s %s", cmd, remote) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			return;
		}
	} else {
		if (command("%s", cmd) != PRELIM) {
			(void) signal(SIGINT, oldintr);
			return;
		}
	}
#ifndef AMOEBA
	din = dataconn("r");
	if (din == NULL)
		goto abort;
#else
	if (!dataconn("r"))
		goto abort;
#endif
	if (strcmp(local, "-") == 0)
		fout = stdout;
	else if (*local == '|') {
		oldintp = signal(SIGPIPE, SIG_IGN);
		fout = popen(local + 1, "w");
		if (fout == NULL) {
			perror(local+1);
			goto abort;
		}
		closefunc = pclose;
	} else {
		fout = fopen(local, lmode);
		if (fout == NULL) {
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
			goto abort;
		}
		closefunc = fclose;
	}
	if (fstat(fileno(fout), &st) < 0 || st.st_blksize == 0)
		st.st_blksize = BUFSIZ;
	if (st.st_blksize > bufsize) {
		if (buf)
			(void) free(buf);
		buf = malloc((unsigned)st.st_blksize);
		if (buf == NULL) {
			perror("malloc");
			bufsize = 0;
			goto abort;
		}
		bufsize = st.st_blksize;
	}
	(void) gettimeofday(&start, (struct timezone *)0);
	switch (curtype) {

	case TYPE_I:
	case TYPE_L:
		if (restart_point &&
		    lseek(fileno(fout), (long) restart_point, L_SET) < 0) {
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
			if (closefunc != NULL)
				(*closefunc)(fout);
			return;
		}
		errno = d = 0;
#ifndef AMOEBA
		while ((c = read(fileno(din), buf, bufsize)) > 0) {
#else
		while ((c = tcpip_read(&data_cap, buf, bufsize)) > 0) {
#endif
			if ((d = write(fileno(fout), buf, c)) != c)
				break;
			bytes += c;
			if (hash) {
				while (bytes >= hashbytes) {
					(void) putchar('#');
					hashbytes += HASHBYTES;
				}
				(void) fflush(stdout);
			}
		}
		if (hash && bytes > 0) {
			if (bytes < HASHBYTES)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
		if (c < 0) {
			if (errno != EPIPE)
				perror("netin");
			bytes = -1;
		}
		if (d < c) {
			if (d < 0)
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
			else
				fprintf(stderr, "%s: short write\n", local);
		}
		break;

	case TYPE_A:
		if (restart_point) {
			register int i, n, ch;

			if (fseek(fout, 0L, L_SET) < 0)
				goto done;
			n = restart_point;
			for (i = 0; i++ < n;) {
				if ((ch = getc(fout)) == EOF)
					goto done;
				if (ch == '\n')
					i++;
			}
			if (fseek(fout, 0L, L_INCR) < 0) {
done:
				fprintf(stderr, "local: %s: %s\n", local,
					strerror(errno));
				if (closefunc != NULL)
					(*closefunc)(fout);
				return;
			}
		}
#ifndef AMOEBA
		while ((c = getc(din)) != EOF) {
#else
		while ((c = tcpip_getc(&data_cap)) != EOF) {
#endif
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				while (hash && (bytes >= hashbytes)) {
					(void) putchar('#');
					(void) fflush(stdout);
					hashbytes += HASHBYTES;
				}
				bytes++;
#ifndef AMOEBA
				if ((c = getc(din)) != '\n' || tcrflag) {
#else
				if ((c = tcpip_getc(&data_cap)) != '\n'
				    || tcrflag) {
#endif
					if (ferror(fout))
						goto break2;
					(void) putc('\r', fout);
					if (c == '\0') {
						bytes++;
						goto contin2;
					}
					if (c == EOF)
						goto contin2;
				}
			}
			(void) putc(c, fout);
			bytes++;
	contin2:	;
		}
break2:
		if (bare_lfs) {
			printf("WARNING! %d bare linefeeds received in ASCII mode\n", bare_lfs);
			printf("File may not have transferred correctly.\n");
		}
		if (hash) {
			if (bytes < hashbytes)
				(void) putchar('#');
			(void) putchar('\n');
			(void) fflush(stdout);
		}
#ifndef AMOEBA
		if (ferror(din)) {
			if (errno != EPIPE)
				perror("netin");
			bytes = -1;
		}
#endif
		if (ferror(fout))
			fprintf(stderr, "local: %s: %s\n", local,
				strerror(errno));
		break;
	}
	if (closefunc != NULL)
		(*closefunc)(fout);
	(void) signal(SIGINT, oldintr);
	if (oldintp)
		(void) signal(SIGPIPE, oldintp);
	(void) gettimeofday(&stop, (struct timezone *)0);
#ifndef AMOEBA
	(void) fclose(din);
#else
	destroy_cap(&data_cap);
#endif
	(void) getreply(0);
	if (bytes > 0 && is_retr)
		ptransfer("received", bytes, &start, &stop);
	return;
abort:

/* abort using RFC959 recommended IP,SYNC sequence  */

	(void) gettimeofday(&stop, (struct timezone *)0);
	if (oldintp)
		(void) signal(SIGPIPE, oldintr);
	(void) signal(SIGINT, SIG_IGN);
	if (!cpend) {
		code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}

	abort_remote(din);
	code = -1;
	if (data >= 0) {
		(void) close(data);
		data = -1;
	}
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	if (din)
		(void) fclose(din);
	if (bytes > 0)
		ptransfer("received", bytes, &start, &stop);
	(void) signal(SIGINT, oldintr);
}

/*
 * Need to start a listen on the data channel before we send the command,
 * otherwise the server's connect may fail.
 */
initconn()
{
#ifndef AMOEBA
	register char *p, *a;
	int result, len, tmpno = 0;
	int on = 1;

noport:
	data_addr = myctladdr;
	if (sendport)
		data_addr.sin_port = 0;	/* let system pick one */ 
	if (data != -1)
		(void) close(data);
	data = socket(AF_INET, SOCK_STREAM, 0);
	if (data < 0) {
		perror("ftp: socket");
		if (tmpno)
			sendport = 1;
		return (1);
	}
	if (!sendport)
		if (setsockopt(data, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0) {
			perror("ftp: setsockopt (reuse address)");
			goto bad;
		}
	if (bind(data, (struct sockaddr *)&data_addr, sizeof (data_addr)) < 0) {
		perror("ftp: bind");
		goto bad;
	}
	if (options & SO_DEBUG &&
	    setsockopt(data, SOL_SOCKET, SO_DEBUG, (char *)&on, sizeof (on)) < 0)
		perror("ftp: setsockopt (ignored)");
	len = sizeof (data_addr);
	if (getsockname(data, (struct sockaddr *)&data_addr, &len) < 0) {
		perror("ftp: getsockname");
		goto bad;
	}
	if (listen(data, 1) < 0)
		perror("ftp: listen");
	if (sendport) {
		a = (char *)&data_addr.sin_addr;
		p = (char *)&data_addr.sin_port;
#define	UC(b)	(((int)b)&0xff)
		result =
		    command("PORT %d,%d,%d,%d,%d,%d",
		      UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
		      UC(p[0]), UC(p[1]));
		if (result == ERROR && sendport == -1) {
			sendport = 0;
			tmpno = 1;
			goto noport;
		}
		return (result != COMPLETE);
	}
	if (tmpno)
		sendport = 1;
#ifdef IP_TOS
	on = IPTOS_THROUGHPUT;
	if (setsockopt(data, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int)) < 0)
		perror("ftp: setsockopt TOS (ignored)");
#endif
	return (0);
bad:
	(void) close(data), data = -1;
	if (tmpno)
		sendport = 1;
	return (1);
#else  /* AMOEBA */

	register char *p, *a;
	errstat err;
	int len, tmpno = 0;
	int on = 1;
noport:
	if (valid_cap(&data_cap)) {
	    destroy_cap(&data_cap);
	}
	err = tcpip_open(&tcp_cap, &data_cap);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: unable to tcpip_open: %s\n", prog_name,
		    err_why(err));
	    if (tmpno)
		sendport = 1;
	    return(1);
	}
 
	if (!sendport) {
	    err = tcp_ioc_setconf(&data_cap, &hisconf);
	    if (err != STD_OK) {
		fprintf(stderr, "%s: unable to  tcp_ioc_setconf: %s\n",
			prog_name, err_why(err));
		goto bad;
	    }
	} else {
	    nwio_tcpconf_t tcpconf;
	    static int lastport = 0x9000;
	    int try = 0;

	    do {
		/* Find a port that is not in use.  Cannot use NWTC_LP_SEL
		 * because that might return a port that has been used
		 * recently.  Using that again may result in EADDRNOTAVAIL
		 * when the ftp server is connecting back to us.
		 */
		tcpconf.nwtc_flags=
		    NWTC_EXCL|NWTC_LP_SET|NWTC_UNSET_RA|NWTC_UNSET_RP;
		tcpconf.nwtc_locport = lastport;
		if (lastport++ >= 0xF000) {
		    /* wrap */
		    lastport = 0x9000;
		}
		err = tcp_ioc_setconf(&data_cap, &tcpconf);
	    } while (err == TCPIP_ADDRINUSE && try++ < 100);

	    if (err != STD_OK) {
		fprintf(stderr, "%s: unable to  tcp_ioc_setconf: %s\n",
			prog_name, err_why(err));
		goto bad;
	    }
	}

	err = tcp_ioc_getconf(&data_cap, &dataconf);
	if (err != STD_OK) {
	    fprintf(stderr, "%s: unable to  tcp_ioc_getconf: %s\n",
		    prog_name, err_why(err));
	    goto bad;
	}
 
	if (sendport) {
	    void start_listener();
	    int iresult;

	    start_listener();
	    millisleep(500); /* should be known to the ip svr by now */
	    a = (char *)&dataconf.nwtc_locaddr;
	    p = (char *)&dataconf.nwtc_locport;
#define       UC(b)   (((int)b)&0xff)
	    iresult =
		command("PORT %d,%d,%d,%d,%d,%d",
		     UC(a[0]), UC(a[1]), UC(a[2]), UC(a[3]),
		     UC(p[0]), UC(p[1]));
	    if (iresult == ERROR && sendport == -1) {
		sendport = 0;
		tmpno = 1;
		goto noport;
	    }
	    return (iresult != COMPLETE);
	}
	if (tmpno)
	    sendport = 1;
	return (0);
bad:
	destroy_cap(&data_cap);
	if (tmpno)
	    sendport = 1;
	return (1);

#endif /* AMOEBA */
}

#ifdef AMOEBA

static semaphore connection_sema;

void
do_listen(param, size)
char *param;
int   size;
{
    /* Unfortunately we have to do the listen in a seperate thread,
     * because the server maybe trying to connect us before sending
     * a reply on a command.
     */
    errstat err;
    nwio_tcpcl_t tcpconnopt;
    capability copycap;

    tcpconnopt.nwtcl_flags= 0; /* no semantics yet? */
    tcpconnopt.nwtcl_ttl= 0xffff; /* ignored? */
    copycap = data_cap;
    err = tcp_ioc_listen(&copycap, &tcpconnopt);
    if (err != STD_OK) {
	fprintf(stderr, "dataconn: unable to tcp_ioc_listen: %s\r\n",
		err_why(err));
	if (cap_cmp(&copycap, &data_cap)) {
	    destroy_cap(&copycap);
	    invalidate_cap(&data_cap);
	}
    }
    if (cap_cmp(&copycap, &data_cap)) {
	/* listening to the current data_cap */
	sema_up(&connection_sema);
    }
}

void
start_listener()
{
    sema_init(&connection_sema, 0);
    if (!thread_newthread(do_listen, 10000, (char *) 0, 0)) {
	fprintf(stderr, "start_listener failed\n");
    }
}

#endif /* AMOEBA */

#ifndef AMOEBA
FILE *
#else
int
#endif
dataconn(lmode)
	char *lmode;
{
#ifndef AMOEBA
	struct sockaddr_in from;
	int s, fromlen = sizeof (from), tos;

	s = accept(data, (struct sockaddr *) &from, &fromlen);
	if (s < 0) {
		perror("ftp: accept");
		(void) close(data), data = -1;
		return (NULL);
	}
	(void) close(data);
	data = s;
#ifdef IP_TOS
	tos = IPTOS_THROUGHPUT;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int)) < 0)
		perror("ftp: setsockopt TOS (ignored)");
#endif
	return (fdopen(data, lmode));
#else
	sema_down(&connection_sema);
	return valid_cap(&data_cap);
#endif
}

ptransfer(direction, bytes, t0, t1)
	char *direction;
	long bytes;
	struct timeval *t0, *t1;
{
	struct timeval td;
	float s, bs;

	if (verbose) {
		tvsub(&td, t1, t0);
		s = td.tv_sec + (td.tv_usec / 1000000.);
#define	nz(x)	((x) == 0 ? 1 : (x))
		bs = bytes / nz(s);
		printf("%ld bytes %s in %.2g seconds (%.2g Kbytes/s)\n",
		    bytes, direction, s, bs / 1024.);
	}
}

/*tvadd(tsum, t0)
	struct timeval *tsum, *t0;
{

	tsum->tv_sec += t0->tv_sec;
	tsum->tv_usec += t0->tv_usec;
	if (tsum->tv_usec > 1000000)
		tsum->tv_sec++, tsum->tv_usec -= 1000000;
} */

tvsub(tdiff, t1, t0)
	struct timeval *tdiff, *t1, *t0;
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

void
psabort(sig)
int sig;
{
	extern int abrtflag;

	abrtflag++;
}

pswitch(flag)
	int flag;
{
	extern int proxy, abrtflag;
	sig_t oldintr;
	static struct comvars {
		int connect;
		char name[MAXHOSTNAMELEN];
#ifndef AMOEBA
		struct sockaddr_in mctl;
		struct sockaddr_in hctl;
		FILE *in;
		FILE *out;
#else
		nwio_tcpconf_t mctl;
		nwio_tcpconf_t hctl;
		capability hcap;
		capability lw_cap;
#endif
		int tpe;
		int curtpe;
		int cpnd;
		int sunqe;
		int runqe;
		int mcse;
		int ntflg;
		char nti[17];
		char nto[17];
		int mapflg;
		char mi[MAXPATHLEN];
		char mo[MAXPATHLEN];
	} proxstruct, tmpstruct;
	struct comvars *ip, *op;

	abrtflag = 0;
	oldintr = signal(SIGINT, psabort);
	if (flag) {
		if (proxy)
			return;
		ip = &tmpstruct;
		op = &proxstruct;
		proxy++;
	} else {
		if (!proxy)
			return;
		ip = &proxstruct;
		op = &tmpstruct;
		proxy = 0;
	}
	ip->connect = connected;
	connected = op->connect;
	if (hostname) {
		(void) strncpy(ip->name, hostname, sizeof(ip->name) - 1);
		ip->name[strlen(ip->name)] = '\0';
	} else
		ip->name[0] = 0;
	hostname = op->name;
#ifndef AMOEBA
	ip->hctl = hisctladdr;
	hisctladdr = op->hctl;
	ip->mctl = myctladdr;
	myctladdr = op->mctl;
	ip->in = cin;
	cin = op->in;
	ip->out = cout;
	cout = op->out;
#else
	ip->hctl = hisconf;
	ip->hcap = hisctlcap;
	hisctlcap = op->hcap;
	hisconf = op->hctl;
	ip->lw_cap = cw_cap;
	cw_cap = op->lw_cap;
#endif
	ip->tpe = type;
	type = op->tpe;
	ip->curtpe = curtype;
	curtype = op->curtpe;
	ip->cpnd = cpend;
	cpend = op->cpnd;
	ip->sunqe = sunique;
	sunique = op->sunqe;
	ip->runqe = runique;
	runique = op->runqe;
	ip->mcse = mcase;
	mcase = op->mcse;
	ip->ntflg = ntflag;
	ntflag = op->ntflg;
	(void) strncpy(ip->nti, ntin, 16);
	(ip->nti)[strlen(ip->nti)] = '\0';
	(void) strcpy(ntin, op->nti);
	(void) strncpy(ip->nto, ntout, 16);
	(ip->nto)[strlen(ip->nto)] = '\0';
	(void) strcpy(ntout, op->nto);
	ip->mapflg = mapflag;
	mapflag = op->mapflg;
	(void) strncpy(ip->mi, mapin, MAXPATHLEN - 1);
	(ip->mi)[strlen(ip->mi)] = '\0';
	(void) strcpy(mapin, op->mi);
	(void) strncpy(ip->mo, mapout, MAXPATHLEN - 1);
	(ip->mo)[strlen(ip->mo)] = '\0';
	(void) strcpy(mapout, op->mo);
	(void) signal(SIGINT, oldintr);
	if (abrtflag) {
		abrtflag = 0;
		(*oldintr)(SIGINT);
	}
}

jmp_buf ptabort;
int ptabflg;

void
abortpt()
{
	printf("\n");
	(void) fflush(stdout);
	ptabflg++;
	mflag = 0;
	abrtflag = 0;
	longjmp(ptabort, 1);
}

#ifndef AMOEBA
proxtrans(cmd, local, remote)
	char *cmd, *local, *remote;
{
	sig_t oldintr;
	int secndflag = 0, prox_type, nfnd;
	extern jmp_buf ptabort;
	char *cmd2;
	struct fd_set mask;
	void abortpt();

	if (strcmp(cmd, "RETR"))
		cmd2 = "RETR";
	else
		cmd2 = runique ? "STOU" : "STOR";
	if ((prox_type = type) == 0) {
		if (unix_server && unix_proxy)
			prox_type = TYPE_I;
		else
			prox_type = TYPE_A;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PASV") != COMPLETE) {
		printf("proxy server does not support third party transfers.\n");
		return;
	}
	pswitch(0);
	if (!connected) {
		printf("No primary connection\n");
		pswitch(1);
		code = -1;
		return;
	}
	if (curtype != prox_type)
		changetype(prox_type, 1);
	if (command("PORT %s", pasv) != COMPLETE) {
		pswitch(1);
		return;
	}
	if (setjmp(ptabort))
		goto abort;
	oldintr = signal(SIGINT, abortpt);
	if (command("%s %s", cmd, remote) != PRELIM) {
		(void) signal(SIGINT, oldintr);
		pswitch(1);
		return;
	}
	sleep(2);
	pswitch(1);
	secndflag++;
	if (command("%s %s", cmd2, local) != PRELIM)
		goto abort;
	ptflag++;
	(void) getreply(0);
	pswitch(0);
	(void) getreply(0);
	(void) signal(SIGINT, oldintr);
	pswitch(1);
	ptflag = 0;
	printf("local: %s remote: %s\n", local, remote);
	return;
abort:
	(void) signal(SIGINT, SIG_IGN);
	ptflag = 0;
	if (strcmp(cmd, "RETR") && !proxy)
		pswitch(1);
	else if (!strcmp(cmd, "RETR") && proxy)
		pswitch(0);
	if (!cpend && !secndflag) {  /* only here if cmd = "STOR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote((FILE *) NULL);
		}
		pswitch(1);
		if (ptabflg)
			code = -1;
		(void) signal(SIGINT, oldintr);
		return;
	}
	if (cpend)
		abort_remote((FILE *) NULL);
	pswitch(!proxy);
	if (!cpend && !secndflag) {  /* only if cmd = "RETR" (proxy=1) */
		if (command("%s %s", cmd2, local) != PRELIM) {
			pswitch(0);
			if (cpend)
				abort_remote((FILE *) NULL);
			pswitch(1);
			if (ptabflg)
				code = -1;
			(void) signal(SIGINT, oldintr);
			return;
		}
	}
	if (cpend)
		abort_remote((FILE *) NULL);
	pswitch(!proxy);
	if (cpend) {
		FD_ZERO(&mask);
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask, 10)) <= 0) {
			if (nfnd < 0) {
				perror("abort");
			}
			if (ptabflg)
				code = -1;
			lostpeer();
		}
		(void) getreply(0);
		(void) getreply(0);
	}
	if (proxy)
		pswitch(0);
	pswitch(1);
	if (ptabflg)
		code = -1;
	(void) signal(SIGINT, oldintr);
}
#endif /* AMOEBA */

reset()
{
#ifndef AMOEBA
	struct fd_set mask;
	int nfnd = 1;

	FD_ZERO(&mask);
	while (nfnd > 0) {
		FD_SET(fileno(cin), &mask);
		if ((nfnd = empty(&mask,0)) < 0) {
			perror("reset");
			code = -1;
			lostpeer();
		}
		else if (nfnd) {
			(void) getreply(0);
		}
	}
#else
	fprintf(stderr, "reset: not yet implemented\n");
#endif
}

char *
gunique(local)
	char *local;
{
	static char new[MAXPATHLEN];
	char *cp = rindex(local, '/');
	int d, count=0;
	char ext = '1';

	if (cp)
		*cp = '\0';
	d = access(cp ? local : ".", 2);
	if (cp)
		*cp = '/';
	if (d < 0) {
		fprintf(stderr, "local: %s: %s\n", local, strerror(errno));
		return((char *) 0);
	}
	(void) strcpy(new, local);
	cp = new + strlen(new);
	*cp++ = '.';
	while (!d) {
		if (++count == 100) {
			printf("runique: can't find unique file name.\n");
			return((char *) 0);
		}
		*cp++ = ext;
		*cp = '\0';
		if (ext == '9')
			ext = '0';
		else
			ext++;
		if ((d = access(new, 0)) < 0)
			break;
		if (ext != '0')
			cp--;
		else if (*(cp - 2) == '.')
			*(cp - 1) = '1';
		else {
			*(cp - 2) = *(cp - 2) + 1;
			cp--;
		}
	}
	return(new);
}

abort_remote(din)
FILE *din;
{
	char buf[BUFSIZ];
#ifndef AMOEBA
	int nfnd;
	struct fd_set mask;
#else
	nwio_tcpopt_t tcpopt;
	errstat err;
#endif

	/*
	 * send IAC in urgent mode instead of DM because 4.3BSD places oob mark
	 * after urgent byte rather than before as is protocol now
	 */
	sprintf(buf, "%c%c%c", IAC, IP, IAC);
#ifndef AMOEBA
	if (send(fileno(cout), buf, 3, MSG_OOB) != 3)
		perror("abort");
	fprintf(cout,"%cABOR\r\n", DM);
	(void) fflush(cout);
	FD_ZERO(&mask);
	FD_SET(fileno(cin), &mask);
	if (din) { 
		FD_SET(fileno(din), &mask);
	}
	if ((nfnd = empty(&mask, 10)) <= 0) {
		if (nfnd < 0) {
			perror("abort");
		}
		if (ptabflg)
			code = -1;
		lostpeer();
	}
	if (din && FD_ISSET(fileno(din), &mask)) {
		while (read(fileno(din), buf, BUFSIZ) > 0)
			/* LOOP */;
	}
#else
	/* switch to urgent mode, temporarily */
	tcpopt.nwto_flags = NWTO_SND_URG | NWTO_BSD_URG;
	err = tcp_ioc_setopt(&cw_cap, &tcpopt);
	if (err != STD_OK) {
		fprintf(stderr, "abort_remote: cannot set urgent mode (%s)\n",
			tcpip_why(err));
		return;
	}
	(void) tcpip_write(&cw_cap, buf, 3);

	/* switch back to normal mode */
	tcpopt.nwto_flags = NWTO_SND_NOTURG | NWTO_NOTBSD_URG;
	err = tcp_ioc_setopt(&cw_cap, &tcpopt);
	if (err != STD_OK) {
		fprintf(stderr, "abort_remote: cannot set normal mode (%s)\n",
			tcpip_why(err));
		/* This is fatal, since in that case we cannot talk
		 * properly to the server at the other side anymore.
		 */
		exit(1);
	}

	sprintf(buf,"%cABOR\r\n", DM);
	(void) tcpip_write(&cw_cap, buf, strlen(buf));
#endif
	if (getreply(0) == ERROR && code == 552) {
		/* 552 needed for nic style abort */
		(void) getreply(0);
	}
	(void) getreply(0);
}
