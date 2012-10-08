/*	@(#)ftpsvr.c	1.1	96/02/27 10:13:58 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <amoeba.h>
#include <exception.h>
#include <fault.h>
#include <semaphore.h>
#include <module/mutex.h>
#include <module/host.h>
#include <module/stdcmd.h>
#include <module/ar.h>
#include <vc.h>
#include <circbuf.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <ampolicy.h>
#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/tcp_io.h>
#include <server/ip/hton.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/tcp.h>
#include <server/ip/gen/tcp_io.h>
#include <thread.h>
#ifdef __STDC__
#include <stdarg.h>
#define  va_dostart(ap, format)	va_start(ap, format)
#else
#include <varargs.h>
#define  va_dostart(ap, format)	va_start(ap)
#endif

extern void putenv();
extern char *strdup();
extern char **environ;

#ifdef TELNETSVR
#define DAEMON_NAME	"/super/admin/bin/telnetd"
#define SERVICE_PORT	23
static char *Service   = "telnet";
static char *Prog_name = "TELNETSVR";
#else
/* FTP */
#define DAEMON_NAME	"/super/admin/bin/ftpd"
#define SERVICE_PORT	21
static char *Service   = "ftp";
static char *Prog_name = "FTPSVR";
#endif

#define CONNECTOR_STACK	10000

static mutex ConnectionLock;

static char    *TcpServerName;
static char    *DaemonName;

static void TCPConnectorThread();

static int verbose = 0;
#define do_verbose(list)	if (verbose) { warning list; }


static semaphore EventSema;

static void
SomethingHappened()
{
    sema_up(&EventSema);
}

static void
WaitForSomething()
{
    sema_down(&EventSema);
}

#ifdef __STDC__
void fatal(char *format, ...)
#else
/*VARARGS1*/
void fatal(format, va_alist) char *format; va_dcl
#endif
{
    va_list ap;

    va_dostart(ap, format);
    fprintf(stderr, "%s: fatal: ", Prog_name);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    /* TODO: Cleanup pending connections? */
    exit(1);
}

#ifdef __STDC__
void warning(char *format, ...)
#else
/*VARARGS1*/
void warning(format, va_alist) char *format; va_dcl
#endif
{
    va_list ap;

    va_dostart(ap, format);
    fprintf(stderr, "%s: ", Prog_name);
    vfprintf(stderr, format, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

static void
SetupConnection(tcp_name, daemon_name)
char *tcp_name;
char *daemon_name;
{
    mu_init(&ConnectionLock);
    sema_init(&EventSema, 0);

    /*
     * Start TCP/IP service threads
     */
    TcpServerName = tcp_name;
    DaemonName = daemon_name;
    if (thread_newthread(TCPConnectorThread,
			 CONNECTOR_STACK, (char*)0, 0) <= 0)
	fatal("Cannot start TCP connector thread");
}

static void
destroy_conn_cap(tcpcap)
capability *tcpcap;
{
    errstat err;

    err = std_destroy(tcpcap);
    if (err != STD_OK && err != STD_CAPBAD) {
	warning("cannot destroy tcp connection cap (%s)", err_why(err));
    }
}

#define MAX_CONNECTIONS	20	/* should be enough */

static struct conn_status {
    int        conn_pid;
    capability conn_cap;
} Connections[MAX_CONNECTIONS];

static int Nr_Connections;

static int
AllocateConnection()
{
    int i;

    mu_lock(&ConnectionLock);

    for (i = 0; i < MAX_CONNECTIONS; i++) {
	if (Connections[i].conn_pid == 0) {
	    /* reserve it */
	    Connections[i].conn_pid = -1;
	    break;
	}
    }

    mu_unlock(&ConnectionLock);
    if (i <= MAX_CONNECTIONS) {
	return i;
    } else {
	return -1;
    }
}

static void
RegisterConnection(slot, pid, tcpcap)
int	    slot;
int         pid;
capability *tcpcap;
{
    mu_lock(&ConnectionLock);

    if (Connections[slot].conn_pid != -1) {
	warning("reusing non-free connection ?! (old = %d, new = %d)",
		Connections[slot].conn_pid, pid);
    }

    Connections[slot].conn_pid = pid;
    Connections[slot].conn_cap = *tcpcap;
    Nr_Connections++;

    mu_unlock(&ConnectionLock);
}

static void
CleanupConnection(pid)
int	pid;
{
    int i;

    mu_lock(&ConnectionLock);

    for (i = 0; i < MAX_CONNECTIONS; i++) {
	if (Connections[i].conn_pid == pid) {
	    break;
	}
    }

    if (i <= MAX_CONNECTIONS) {
	destroy_conn_cap(&Connections[i].conn_cap);
	/* free this slot */
	Connections[i].conn_pid = 0;
	Nr_Connections--;
    } else {
	warning("cannot find connection for pid %d", pid);
    }

    mu_unlock(&ConnectionLock);
}


/*
 * To prevent the Telnet server from generating lots of error messages,
 * in case the server is gone or when it's full.
 */
#define	LOOP_OPEN	1
#define	LOOP_SETCONF	2
#define	LOOP_LISTEN	4

/*
 * The TCP/IP connector thread listens to the Telnet port for a connection
 * request.  When such a request comes in, a Telnet Daemon will be started
 * which will start an Amoeba login, and manage the communication between
 * the Amoeba client and the telnet client.
 */
static void
TCPConnectorThread()
{
    capability		svrcap, chancap;
    nwio_tcpconf_t	tcpconf;
    nwio_tcpcl_t	tcpconnopt;
    errstat 		err;
    int			looping = 0;
    char		envbuf[256];
    char	       *prev_env_string = NULL;
    char	       *prev_env_string2 = NULL;
    char	       *env_string;
    char	       *daemon_argv[4];
    static int          fdlist[3] = {0, 1, 2};
    int			conn_slot;
    int			pid;

    if ((err = ip_host_lookup(TcpServerName, "tcp", &svrcap)) != STD_OK) {
	fatal("Lookup %s failed: %s", TcpServerName, err_why(err));
    }

    do_verbose(("TCPConnectorThread() running ..."));

    conn_slot = -1;
    for (;;) {
	if (conn_slot < 0) {
	    conn_slot = AllocateConnection();
	    if (conn_slot < 0) {
		warning("cannot allocate new connection");
		/* wait for other connections to close down */
		sleep(60);
		continue;
	    }
	}
	
	/*
	 * Listen to SERVICE_PORT for connections.
	 * Some interesting actions have to be taken to keep this connection
	 * alive and kicking :-)
	 */
	do_verbose(("tcpip_open ..."));

	if ((err = tcpip_open(&svrcap, &chancap)) != STD_OK) {
	    /* the server probably disappeared, just wait for it to return */
	    if ((looping & LOOP_OPEN) == 0) {
		warning("TCP/IP open failed: %s", tcpip_why(err));
		looping |= LOOP_OPEN;
	    }
	    sleep(60);
	    (void) ip_host_lookup(TcpServerName, "tcp", &svrcap);
	    continue;
	}
	looping &= ~LOOP_OPEN;

	tcpconf.nwtc_locport = htons(SERVICE_PORT);
	tcpconf.nwtc_flags =
	    NWTC_SHARED | NWTC_LP_SET | NWTC_UNSET_RA | NWTC_UNSET_RP;
	do_verbose(("tcp_ioc_setconf ..."));
	if ((err = tcp_ioc_setconf(&chancap, &tcpconf)) != STD_OK) {
	    /* couldn't configure, probably server space problem */
	    if ((looping & LOOP_SETCONF) == 0) {
		looping |= LOOP_SETCONF;
		warning("TCP/IP setconf failed: %s", tcpip_why(err));
	    }
	    destroy_conn_cap(&chancap);
	    sleep(60);
	    continue;
	}
	looping &= ~LOOP_SETCONF;

	tcpconnopt.nwtcl_flags = 0;
	do_verbose(("tcp_ioc_listen ..."));
	if ((err = tcp_ioc_listen(&chancap, &tcpconnopt)) != STD_OK) {
	    /* couldn't listen, definitely a server memory problem */
	    if ((looping & LOOP_LISTEN) == 0) {
		warning("TCP/IP listen failed: %s", tcpip_why(err));
		looping |= LOOP_LISTEN;
	    }
	    destroy_conn_cap(&chancap);
	    sleep(60);
	    continue;
	}
	looping &= ~LOOP_LISTEN;

	do_verbose(("tcpip_keepalive ..."));
	if ((err = tcpip_keepalive_cap(&chancap)) != STD_OK) {
	    warning("TCP/IP keep alive failed: %s", tcpip_why(err));
	    destroy_conn_cap(&chancap);
	    continue;
	}

#ifdef TELNETSVR
	/*
	 * Now start the telnet daemon that will manage this session by
	 * implementing a pseudo tty and starting a login command on it.
	 * The TCP/IP channel cap is passed via the environment.
	 */
#endif
	sprintf(envbuf, "TCPIP_CAP=%s", ar_cap(&chancap));
	env_string = strdup(envbuf);
	if (env_string == NULL) {
	    fatal("cannot allocate TCPIP environment string");
	}
	do_verbose(("putting %s in environ ...", env_string));
	putenv(env_string);
	if (prev_env_string != NULL) {
	    free((_VOIDSTAR) prev_env_string);
	}
	prev_env_string = env_string;


	sprintf(envbuf, "TCPIPSVR_CAP=%s", ar_cap(&svrcap));
	env_string = strdup(envbuf);
	if (env_string == NULL) {
	    fatal("cannot allocate TCPIP environment string");
	}
	do_verbose(("putting %s in environ ...", env_string));
	putenv(env_string);
	if (prev_env_string2 != NULL) {
	    free((_VOIDSTAR) prev_env_string2);
	}
	prev_env_string2 = env_string;


	daemon_argv[0] = DaemonName;
	daemon_argv[1] = NULL;

	pid = newproc(DaemonName, daemon_argv, environ,
		      3, fdlist, -1L);
	if (pid < 0) {
	    warning("Cannot startup %s (%s)",
		    DaemonName, strerror(errno));
	    destroy_conn_cap(&chancap);
	    continue;
	}

	/*
	 * Administer this command, so that we know what to clean
	 * up when it exits.
	 */
	RegisterConnection(conn_slot, pid, &chancap);
	SomethingHappened(); /* we now can wait for this session to exit */
	conn_slot = -1;
    }
}

static void
AwaitSessions()
{
    /* Wait for telnetd sessions to terminate, and up clean
     * the corresponding TCP/IP connection (supposing the telnetd
     * wasn't able to do this properly).
     */

    for (;;) {
	if (Nr_Connections == 0) {
	    WaitForSomething();
	}
	if (Nr_Connections > 0) {
	    int status;
	    int pid;

	    pid = waitpid(-1, &status, 0);
	    if (pid < 0) {
		warning("waitpid failed (%s)", strerror(errno));
	    } else {
		if (status == 0) {
		    warning("%s session (pid %d) completed", Service, pid);
		} else {
		    warning("%s session (pid %d) exited with status %d",
			    Service, pid, status);
		}
		CleanupConnection(pid);
	    }
	} else {
	    /* This happens when the last of multiple telnet sessions has
	     * just exited.
	     */
	    warning("no more %s sessions", Service);
	    sleep(30);
	}
    }
}

void usage()
{
    fprintf(stderr, "usage: %s [-T <tcp-cap>] [-daemon <program>] [-v]\n",
	    Prog_name);
    exit(1);
}

int main(argc, argv)
int argc;
char *argv[];
{
    int argindex;
    char *tcp_svr_name; 
    char *daemon_name; 

    Prog_name = argv[0];
    tcp_svr_name = NULL;
    daemon_name = NULL;

    for (argindex = 1; argindex < argc; argindex++) {
	if (strcmp(argv[argindex], "-?") == 0)
	    usage();
	if (strcmp(argv[argindex], "-v") == 0) {
	    verbose = 1;
	    continue;
	}
	if (strcmp(argv[argindex], "-T") == 0) {
	    argindex++;
	    if (tcp_svr_name)
		usage();
	    if (argindex >= argc)
		usage();
	    tcp_svr_name = argv[argindex];
	    continue;
	}
	if (strcmp(argv[argindex], "-daemon") == 0) {
	    argindex++;
	    if (daemon_name)
		usage();
	    if (argindex >= argc)
		usage();
	    daemon_name = argv[argindex];
	    continue;
	}
	usage();
    }

    if (tcp_svr_name == NULL) {
	tcp_svr_name = getenv("TCP_SERVER");
	if (tcp_svr_name != NULL && tcp_svr_name[0] == '\0') {
	    tcp_svr_name = NULL;
	}
    }
    if (tcp_svr_name == NULL) {
	tcp_svr_name = TCP_SVR_NAME;
    }
    if (daemon_name == NULL) {
	daemon_name = DAEMON_NAME;
    }

    /* now start listening */
    SetupConnection(tcp_svr_name, daemon_name);
    AwaitSessions();
    return 0;
}
