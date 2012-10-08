/*	@(#)am_conn.c	1.1	96/02/27 10:13:45 */
#include <stdio.h>
#include <amoeba.h>
#include <semaphore.h>
#include <circbuf.h>
#include <stdlib.h>
#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/tcp_io.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/inet.h>
#include <server/ip/gen/tcp.h>
#include <server/ip/gen/tcp_io.h>
#include <server/ip/gen/netdb.h>
#include <server/ip/gen/socket.h>
#include <server/ip/hton.h>
#include <stderr.h>
#include <module/ar.h>
#include <thread.h>
#include <module/stdcmd.h>
#ifdef __STDC__
#include <stdarg.h>
#define  va_dostart(ap, fmt)	va_start(ap, fmt)
#else
#include <varargs.h>
#define  va_dostart(ap, fmt)	va_start(ap)
#endif

#include "am_conn.h"

extern char *strdup();


#define TCPIP_CAPNAME	 "TCPIP_CAP"
#define TCPIPSVR_CAPNAME "TCPIPSVR_CAP"

#define MAXBUFSIZE	8192
#define CONNECTOR_STACK	8192

#define MAX_WAIT	30000 /* msec */

static semaphore EventSema;

#define dprintf(list)	/* printf list */
#define dbprintf(list)	printf list


ipaddr_t  data_dest_addr;
tcpport_t data_dest_port;
static tcpconn *Default_tcpconn;
static capability TcpSvrCap;

int
net_close(conn)
tcpconn *conn;
{
    errstat err;

    dprintf(("net_close(%d, 0x%x)\n", conn->tcp_nr, conn->tcp_status));
    if ((conn->tcp_status & CONN_CLOSED) == 0) {
	if ((err = tcp_ioc_shutdown(&conn->tcp_cap)) != STD_OK) {
	    fprintf(stderr, "ftpsvr: tcp_ioc_shutdown failed (%s)\n",
		    tcpip_why(err));
	}
	conn->tcp_status |= CONN_CLOSED;
	cb_close(conn->tcp_circbuf);
    }

    conn->tcp_status &= ~(CONN_OPEN | CONN_ALLOC);

    /* The circbuf and the connection structure will be freed when
     * the reader thread finds that it has been closed.
     */

    return 0;
}

#define CONN_ALL	(CONN_ALLOC|CONN_OPEN|CONN_CLOSED|CONN_EOF|CONN_ERR)

void
sanity_check(conn, where)
tcpconn *conn;
char    *where;
{
    if ((conn->tcp_status & ~CONN_ALL) == 0 &&
	 (conn->tcp_status & CONN_ALL) != 0)
    {
	dprintf(("%s: OK\n", where));
    }
    else
    {
	fprintf(stderr, "%s: sanity check failed (flags 0x%x)\n",
		where, conn->tcp_status);
    }
}

int
net_getc(conn)
tcpconn *conn;
{
    char smallbuf[1];
    int got;

    sanity_check(conn, "net_getc[1]");
    dprintf(("net_getc(%d, 0x%x)\n", conn->tcp_nr, conn->tcp_status));

    got = net_read(conn, smallbuf, 1);
    if (got < 1) {
	dprintf(("net_getc: no more data\n"));
	sanity_check(conn, "net_getc[2]");
	return EOF;
    } else {
	dprintf(("net_getc: read '%c'\n", smallbuf[0]));
	sanity_check(conn, "net_getc[3]");
	return (smallbuf[0] & 0xFF);
    }
}

int
net_std_getc()
{
    dprintf(("net_std_getc()\n"));
    return net_getc(Default_tcpconn);
}

#ifdef __STDC__
volatile
#endif
int GotTcpUrg = 0;

int
net_write(conn, buf, size)
tcpconn *conn;
char *buf;
int   size;
{
    int wrote;

    dprintf(("net_write(%d, 0x%x, 0x%x, %d)\n", conn->tcp_nr, conn->tcp_status,
	     buf, size));

#ifdef DEBUG
    {
	char mybuf[256];

	if (size < sizeof(mybuf)) {
	    memcpy(mybuf, buf, size);
	    mybuf[size] = '\0';
	    dprintf(("net_write: %d bytes; data %s\n", size, mybuf));
	}
    }
#endif

    wrote = tcpip_write(&conn->tcp_cap, buf, (size_t) size);
    if (ERR_STATUS(wrote)) {
	fprintf(stderr, "tcpip_write failed (%s)\n",
		tcpip_why((errstat) ERR_CONVERT(wrote)));
	return -1;
    }

    if (wrote != size) {
	fprintf(stderr, "tcpip_write only wrote %d bytes\n", wrote);
    }

    return wrote;
}

int
net_add_to_buf(conn, buf, size)
tcpconn *conn;
char    *buf;
int      size;
{
    int retval = size;

    dprintf(("net_add_to_buf(%d, 0x%x, 0x%x, %d)\n",
	     conn->tcp_nr, conn->tcp_status,
	     buf, size));

    if (conn->tcp_bufsiz > 0) {
	if (conn->tcp_inbuf + size > conn->tcp_bufsiz) {
	    int wrote = net_flush(conn);

	    if (wrote < 0) {
		return wrote;
	    }
	}
	if (conn->tcp_inbuf + size > conn->tcp_bufsiz) {
	    /* still doesn't fit; write directly */
	    retval = net_write(conn, buf, size);
	} else {
	    memcpy(&conn->tcp_buf[conn->tcp_inbuf], buf, size);
	    conn->tcp_inbuf += size;
	}
    } else {
	/* no buffering, write directly */
	retval = net_write(conn, buf, size);
    }

    return retval;
}

int
#ifdef __STDC__
net_fprintf(tcpconn *conn, char *fmt, ...)
#else
net_fprintf(conn, fmt, va_alist) tcpconn *conn; char *fmt; va_dcl
#endif
{
    char buf[256];
    int size;
    va_list ap;

    dprintf(("net_fprintf(%d, 0x%x)\n", conn->tcp_nr, conn->tcp_status));

    va_dostart(ap, fmt);
    size = vsprintf(buf, fmt, ap);
    va_end(ap);

    return net_add_to_buf(conn, buf, size);
}

int
#ifdef __STDC__
net_printf(char *fmt, ...)
#else
net_printf(fmt, va_alist) char *fmt; va_dcl
#endif
{
    char buf[256];
    int size;
    va_list ap;

    dprintf(("net_printf(%s)\n", fmt));

    va_dostart(ap, fmt);
    size = vsprintf(buf, fmt, ap);
    va_end(ap);

    return net_add_to_buf(Default_tcpconn, buf, size);
}

int
net_putc(conn, c)
tcpconn *conn;
int c;
{
    char smallbuf[1];

    dprintf(("net_putc(%d, 0x%x, '%c')\n",
	     conn->tcp_nr, conn->tcp_status, c));
    smallbuf[0] = c;
    return net_add_to_buf(conn, smallbuf, 1);
}

int
net_std_putc(c)
int c;
{
    return net_putc(Default_tcpconn, c);
}

int
net_flush(conn)
tcpconn *conn;
{
    int written = 0;

    dprintf(("net_flush(%d, 0x%x)\n", conn->tcp_nr, conn->tcp_status));
    if (conn->tcp_inbuf > 0) {
	written = net_write(conn, conn->tcp_buf, conn->tcp_inbuf);
	conn->tcp_inbuf = 0;
    }
    return written;
}

int
net_std_flush()
{
    return net_flush(Default_tcpconn);
}

int
net_read(conn, buf, size)
tcpconn *conn;
char *buf;
int size;
{
    int got;

    dprintf(("net_read(%d, 0x%x, 0x%x, %d)\n",
	     conn->tcp_nr, conn->tcp_status, buf, size));
    sanity_check(conn, "net_read[0]");

    dprintf(("reading from net queue; "));
    got = cb_gets(conn->tcp_circbuf, buf, /* at least */ 1, size);
    sanity_check(conn, "net_read[1]");
    if (got > 0) {
	char mybuf[256];

	if (got < sizeof(mybuf)) {
	    /* debug */
	    memcpy(mybuf, buf, (size_t) got);
	    mybuf[got] = '\0';
	    dprintf(("%d bytes; data: \"%s\"\n", got, mybuf));
	} else {
	    dprintf(("%d bytes\n", got));
	}
    } else {
	/* This may happen during the option processing in ttloop.
	 * In that case there may not be data available yet.
	 * The caller just sleeps a while and retries in that case.
	 */
	dprintf(("no data from net\n"));
	conn->tcp_status |= CONN_EOF;
    }

    sanity_check(conn, "net_read[2]");
    return got;
}

int
net_error(conn)
tcpconn *conn;
{
    dprintf(("net_error(%d, 0x%x)\n", conn->tcp_nr, conn->tcp_status));
    return (conn->tcp_status & CONN_ERR) != 0;
}

void
#ifdef __STDC__
FatalError(char * fmt, ...)
#else
FatalError(fmt, va_alist) char *fmt; va_dcl
#endif
{
    va_list ap;

    fprintf(stderr, "fatal error: ");
    va_dostart(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

static void SomethingHappened();

static void
handle_urgent_data(conn)
tcpconn *conn;
{
    nwio_tcpopt_t tcpopt;
    char buffer[8]; /* typically only a few control bytes are sent */
    errstat err;
    int size;
 
    /* Tell IP server we are willing to process the pending urgent data,
     * read the urgent data, queue it, and go back to normal mode.
     */
    tcpopt.nwto_flags = (NWTO_RCV_URG | NWTO_BSD_URG);
    err = tcp_ioc_setopt(&conn->tcp_cap, &tcpopt);
    if (err != STD_OK) {
	fprintf(stderr, "handle_urgent_data: cannot set tcp option (%s)\n",
		tcpip_why(err));
	return;
    }

    for (;;) {
	size = tcpip_read(&conn->tcp_cap, buffer, sizeof(buffer));
	if (ERR_STATUS(size) || size == 0) {
	    if (ERR_CONVERT(size) != TCPIP_NOURG) {
		fprintf(stderr, "error reading urgent data: %s\n",
			tcpip_why(ERR_CONVERT(size)));
	    }
	    break;
	}
	/* put it in the circbuf like usual data */
	if (cb_puts(conn->tcp_circbuf, buffer, size)) {
            FatalError("write failed to TCP/IP buffer");
        }
    }

    tcpopt.nwto_flags = (NWTO_RCV_NOTURG | NWTO_NOTBSD_URG);
    err = tcp_ioc_setopt(&conn->tcp_cap, &tcpopt);
    if (err != STD_OK) {
	fprintf(stderr, "handle_urgent_data: cannot set tcp option (%s)\n",
		tcpip_why(err));
    }
}

/*ARGSUSED*/
static void
tcpip_reader(argptr, argsize)
char *argptr;
int argsize;
{
    tcpconn *conn, **connp;

    connp = (tcpconn **) argptr;
    conn = *connp;

    sleep(3);

    for (;;) {
	char buffer[MAXBUFSIZE];
	bufsize size;

	dprintf(("tcpip_reader (%d, 0x%x)\n", conn->tcp_nr, conn->tcp_status));

	/* TODO: set error flag instead? */
	size = tcpip_read(&conn->tcp_cap, buffer, sizeof(buffer));
	if (size == 0 ||
	    (ERR_STATUS(size) && (ERR_CONVERT(size) == TCPIP_NOCONN)))
	{
	    dprintf(("tcpip_read: connection closed\n"));
	    if ((conn->tcp_status & CONN_CLOSED) == 0) {
		dprintf(("tcpip_reader, destroy: %s\n",
			 ar_cap(&conn->tcp_cap)));
		std_destroy(&conn->tcp_cap);
		conn->tcp_status |= CONN_CLOSED;
		cb_close(conn->tcp_circbuf);
	    }
	    while (conn->tcp_status & CONN_ALLOC) {
		dprintf((" wait until all data is processed\n"));
		sleep(5);
	    }
	    if (conn->tcp_circbuf != NULL) {
		cb_free(conn->tcp_circbuf);
		conn->tcp_circbuf = NULL;
	    }
	    if (conn->tcp_buf != NULL) {
		free(conn->tcp_buf);
		conn->tcp_buf = NULL;
	    }
	    free(conn);
	    thread_exit();
	} else if (ERR_STATUS(size)) {
	    errstat err = ERR_CONVERT(size);

	    if (err == TCPIP_URG) {
		/* Handle urgent data and set a flag so that the main
		 * thread knows it has to abort the current command.
		 */
		GotTcpUrg++;
		handle_urgent_data(conn);
		continue;
	    } else {
		FatalError("TCP/IP read failed (%s)", tcpip_why(err));
	    }
	}

	dprintf(("tcpip: read %d bytes\n", size));
	if (cb_puts(conn->tcp_circbuf, buffer, size)) {
	    FatalError("write failed to TCP/IP buffer");
	}
	SomethingHappened();
    }
}

#define BUFFERSIZE	512

static tcpconn *
net_new_conn(conncap)
capability *conncap;
{
    static int last_conn_nr;
    tcpconn *newconn, **connp;
    char *errstring;
#   define alloc_failure(msg) { errstring = msg; goto failure; }

    connp = NULL;
    newconn = (tcpconn *) calloc(sizeof(tcpconn), 1);
    if (newconn == NULL) {
	alloc_failure("cannot alloc connection struct");
    }

    newconn->tcp_nr = last_conn_nr++;
    newconn->tcp_status = CONN_ALLOC;
    newconn->tcp_cap = *conncap;
    newconn->tcp_buf = (char *) malloc(BUFFERSIZE);
    if (newconn->tcp_buf == NULL) {
	alloc_failure("cannot alloc output buffer");
    }
    newconn->tcp_bufsiz = BUFFERSIZE;
    newconn->tcp_inbuf = 0;
    newconn->tcp_circbuf = cb_alloc(MAXBUFSIZE);
    if (newconn->tcp_circbuf == NULL) {
	alloc_failure("cannot alloc circular buffer");
    }

    connp = (tcpconn **) malloc(sizeof(tcpconn *));
    if (connp == NULL) {
	alloc_failure("cannot alloc tcpconn ptr");
    }
    *connp = newconn;

    /* Start TCP/IP reader thread for this connection */
    dprintf(("tcpip: start reader ..."));
    if (thread_newthread(tcpip_reader, MAXBUFSIZE + CONNECTOR_STACK,
			 (char *) connp, sizeof(connp)) == 0)
    {
	alloc_failure("cannot start tcpip reader thread");
    }

    newconn->tcp_status |= CONN_OPEN;

    return newconn;

failure:
    fprintf(stderr, "failure creating tcp connection: %s\n", errstring);
    if (newconn != NULL) {
	if (newconn->tcp_circbuf != NULL) cb_free(newconn->tcp_circbuf);
	if (newconn->tcp_buf != NULL) free(newconn->tcp_buf);
	free(newconn);
    }
    if (connp != NULL) free(connp);

    return NULL;
}


tcpconn *
net_create_conn()
{
    capability     chancap;
    nwio_tcpconf_t tcpconf;
    nwio_tcpcl_t   tcpconnopt;
    tcpconn       *newconn;
    errstat        err;

    if ((err = tcpip_open(&TcpSvrCap, &chancap)) != STD_OK) {
	fprintf(stderr, "create_conn: TCP/IP open failed: %s", tcpip_why(err));
	return NULL;
    }
    dprintf(("net_create_conn, tcpip_open: %s\n", ar_cap(&chancap)));

    /* tcpconf.nwtc_locport = htons(data_dest_port); ? */
    tcpconf.nwtc_remport = htons(data_dest_port);
    tcpconf.nwtc_remaddr = htonl(data_dest_addr);
    tcpconf.nwtc_flags = NWTC_EXCL|NWTC_LP_SEL|NWTC_SET_RA|NWTC_SET_RP;
    if ((err = tcp_ioc_setconf(&chancap, &tcpconf)) != STD_OK) {
	fprintf(stderr, "create_conn: TCP/IP setconf failed: %s",
		tcpip_why(err));
	std_destroy(&chancap);
	return NULL;
    }

    tcpconnopt.nwtcl_flags = 0;
    if ((err = tcp_ioc_connect(&chancap, &tcpconnopt)) != STD_OK) {
	fprintf(stderr, "TCP/IP connect failed: %s", tcpip_why(err));
	std_destroy(&chancap);
	return NULL;
    }

    if ((err = tcpip_keepalive_cap(&chancap)) != STD_OK) {
	fprintf(stderr, "TCP/IP keep alive failed: %s", tcpip_why(err));
	std_destroy(&chancap);
	return NULL;
    }
    
    newconn = net_new_conn(&chancap);
    if (newconn == NULL) {
	std_destroy(&chancap);
	return NULL;
    }

    return newconn;
}
    

tcpconn *
net_open(tcp_conn_cap)
capability *tcp_conn_cap;
{
    nwio_tcpconf_t tcpconf;
    struct hostent *remote;
    tcpconn *newconn;
    char *hostname;
    errstat err;

    newconn = net_new_conn(tcp_conn_cap);
    if (newconn == NULL) {
	FatalError("net_open: cannot create new connection\n");
    }

    if ((err = tcp_ioc_getconf(tcp_conn_cap, &tcpconf)) != STD_OK) {
	FatalError("Cannot get tcp conf parameters (%s)", err_why(err));
    }

    /* get name of remote host */
    remote = gethostbyaddr((char *) &tcpconf.nwtc_remaddr,
			   sizeof(tcpconf.nwtc_remaddr), AF_INET);
    if ((remote == NULL) || (remote->h_name == NULL)) {
	hostname = inet_ntoa(tcpconf.nwtc_remaddr);
    } else {
	hostname = remote->h_name;
    }

    /* TODO: log connections? */
    fprintf(stderr, "ftpsvr: made connection with host %s\n", hostname);
    
    return newconn;
}

static void
get_cap_from_env(name, cap)
char *name;
capability *cap;
{
    char *capstr, *after_cap;

    if ((capstr = getenv(name)) == NULL) {
	FatalError("no %s in environment", name);
    }
    dprintf(("%s is %s", name, capstr));

    after_cap = ar_tocap(capstr, cap);
    if (after_cap == NULL || *after_cap != '\0') {
	FatalError("malformed %s capability", name);
    }
}

int
net_init()
{
    capability tcp_conn_cap;

    sema_init(&EventSema, 0);

    /* Get and check the Tcpip Channel cap passed via the environment */
    get_cap_from_env(TCPIP_CAPNAME, &tcp_conn_cap);

    /* Get and check the Tcpip Server cap passed via the environment */
    get_cap_from_env(TCPIPSVR_CAPNAME, &TcpSvrCap);

    Default_tcpconn = net_open(&tcp_conn_cap);

    return (Default_tcpconn != NULL);
}

static void
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
	dprintf(("*"));
    }
}
