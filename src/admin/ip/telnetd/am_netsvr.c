/*	@(#)am_netsvr.c	1.2	96/02/27 10:15:36 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <semaphore.h>
#include <circbuf.h>
#include <stdlib.h>
#include <stderr.h>
#include <thread.h>
#include <module/ar.h>
#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/tcp_io.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/tcp.h>
#include <server/ip/gen/tcp_io.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/socket.h>

#include "am_defs.h"

#ifdef DEBUG
#define dbprintf(list)	{ warning list ; }
#else
#define dbprintf(list)	{ /* nothing */ }
#endif

#define TCPIP_CAPNAME	"TCPIP_CAP"

#define MAXBUFSIZE	8192
#define CONNECTOR_STACK	8192

#define MAX_WAIT	30000 /* msec */

static capability TcpipConnCap;
static struct circbuf *TcpipCircbuf;
static semaphore EventSema;

/*ARGSUSED*/
static void
TcpIpReaderThread(argptr, argsize)
char *argptr;
int argsize;
{
    for (;;) {
	char buffer[MAXBUFSIZE];
	bufsize size;

	dbprintf(("tcpip: calling read"));
	size = tcpip_read(&TcpipConnCap, buffer, sizeof(buffer));
	if (ERR_STATUS(size)) {
	    FatalError("TCP/IP read failed (%s)",
		       tcpip_why((errstat) ERR_CONVERT(size)));
	}

	dbprintf(("tcpip: read %d bytes", size));
	if (size == 0 || cb_puts(TcpipCircbuf, buffer, size)) {
	    if (size != 0) {
		FatalError("write failed to TCP/IP buffer");
	    } else {
		FatalError("tcpip_read failed; connection closed");
	    }
	}
	SomethingHappened();
    }
}

char *
net_init()
{
    char *strdup();
    char *tcpip_cap;
    char *after_cap;
    struct hostent *remote;
    char *hostname;
    nwio_tcpconf_t tcpconf;
    errstat err;

    sema_init(&EventSema, 0);

    /*
     * Get and check the Tcpip Channel cap we got passed via the environment.
     */
    if ((tcpip_cap = getenv(TCPIP_CAPNAME)) == NULL) {
	FatalError("no %s in environment", TCPIP_CAPNAME);
    }
    dbprintf(("%s is %s", TCPIP_CAPNAME, tcpip_cap));

    after_cap = ar_tocap(tcpip_cap, &TcpipConnCap);
    if (after_cap == NULL || *after_cap != '\0') {
	FatalError("malformed %s capability", TCPIP_CAPNAME);
    }
    
    if ((err = tcp_ioc_getconf(&TcpipConnCap, &tcpconf)) != STD_OK) {
	FatalError("Cannot get tcp conf parameters (%s)", err_why(err));
    }

    /* get name of remote host */
    remote = gethostbyaddr((char *) &tcpconf.nwtc_remaddr,
			   sizeof(tcpconf.nwtc_remaddr), AF_INET);
    if ((remote == NULL) || (remote->h_name == NULL)) {
	hostname = strdup(inet_ntoa(tcpconf.nwtc_remaddr));
    } else {
	hostname = strdup(remote->h_name);
    }
    if (hostname == NULL) {
	FatalError("cannot alloc hostname");
    }

    if ((TcpipCircbuf = cb_alloc(MAXBUFSIZE)) == NULL) {
	FatalError("tcpip: cannot allocate circular buffer");
    }

    /*
     * Start TCP/IP reader thread
     */
    dbprintf(("tcpip: start reader ..."));
    if (thread_newthread(TcpIpReaderThread, MAXBUFSIZE + CONNECTOR_STACK,
			 (char *) 0, 0) == 0)
    {
	FatalError("Cannot start TcpIp reader thread");
    }

    /* TODO: log telnet connections? */
    warning("made connection with host %s", hostname);

    return hostname;
}

int
net_write(buf, size)
char *buf;
int   size;
{
    int wrote;

    buf[size] = '\0';
    dbprintf(("net_write: %d bytes; data %s", size, buf));
    wrote = tcpip_write(&TcpipConnCap, buf, (size_t) size);
    if (wrote != size) {
	warning("tcpip_write returned %d bytes", wrote);
    }

    return wrote;
}

int
net_data_available()
{
    int size;

    size = cb_full(TcpipCircbuf);

    /* dbprintf(("net: %d bytes available", size)); */

    return (size > 0);
}

int
net_read(buf, size)
char *buf;
int   size;
{
    int got;

    dbprintf(("reading from net queue"));
    got = cb_gets(TcpipCircbuf, buf, 0, size);
    if (got > 0) {
	buf[got] = '\0';
	dbprintf(("%d bytes; data: \"%s\"", got, buf));
    } else {
	/* This may happen during the option processing in ttloop.
	 * In that case there may not be data available yet.
	 * The caller just sleeps a while and retries in that case.
	 */
	dbprintf(("no data from net"));
    }

    return got;
}

int
net_can_flush()
{
    return 1;
}

void
SomethingHappened()
{
    sema_up(&EventSema);
}

void
WaitForSomething()
{
    int failed;

    failed = sema_trydown(&EventSema, (interval) MAX_WAIT);
    if (failed) {
	/* Nothing seems to have happened during the last MAX_WAIT milli secs,
	 * but we poll the net and tty modules anyway, just in case.
	 */
	dbprintf(("*"));
    }
}
