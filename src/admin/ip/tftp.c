/*	@(#)tftp.c	1.3	96/02/27 10:15:05 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* #define DEBUG */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <amtools.h>
#include <ampolicy.h>
#include <thread.h>

#include <module/direct.h>
#include <module/name.h>
#include <module/host.h>
#include <module/mutex.h>
#include <module/cap.h>

#include <server/ip/hton.h>
#include <server/ip/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/udp_io.h>
#include <server/ip/gen/in.h>
#include <server/ip/gen/udp.h>
#include <server/ip/gen/udp_hdr.h>
#include <server/ip/gen/udp_io.h>
#include <server/ip/udp.h>

#include <server/bullet/bullet.h>

#include <protocols/tftp.h>

#ifdef DEBUG
#define dprintf(arglist)	printf arglist
#else
#define dprintf(arglist)	{}
#endif

#define Tftp_fnammod	tftp_operand.tftp_req.tftp_fnammod
#define Tftp_blockno	tftp_operand.tftp_dat.tftp_blockno
#define Tftp_data	tftp_operand.tftp_dat.tftp_data

capability dircap;		/* Cap of tftpbootdir */
capability udp_cap;		/* Cap of udp server */
char *prog_name;		/* Name of this program */

#define CACHE_FIRSTTRY	(450 * 1024)	/* big enough for most kernels */
#define CACHE_KEEP	30000		/* keep binary in cache for a while */
#define NTFTPTHREAD	10
#define NTFTPCONN 	100
#define USE_TIMEOUT 	(5L * 1000)	/* 5 seconds timeout */

typedef struct pkt
{
	udp_io_hdr_t udp_hdr;
	struct tftp_pkt	tftp_pkt;
} pkt_t;

static struct tftpconn {
	capability	tc_cap;
	int		tc_blkno;
	ipaddr_t	tc_ipaddr;
	udpport_t	tc_udpport;
	int		tc_flags;
	unsigned long 	tc_lastused;
} tftpconn[NTFTPCONN];

#define TCF_INUSE	1
#define TCF_DROPPED	2

static int nextconn = 0;
static int verbose = 0;
static int docache = 0;

static mutex conn_mutex;  /* Guard around messing in nextconn and tftpconn */
static mutex cache_mutex; /* Avoid simultaneous update to cached file */
static mutex print_mutex; /* Avoid garbled output */

size_t handle _ARGS(( pkt_t *tp ));
int main _ARGS(( int argc, char *argv[] ));
void usage _ARGS(( void ));

static capability cached_cap;
static char *cached_buf;
static b_fsize cached_buf_size;
static b_fsize cached_size;

static errstat
cache_in(cap)
capability *cap;
{
    b_fsize didread;
    b_fsize filesize;
    errstat err;

    if (verbose) {
	mu_lock(&print_mutex);
	printf("%s: cache in new binary\n", prog_name);
	mu_unlock(&print_mutex);
    }

    err = b_size(cap, &filesize);
    if (err != STD_OK) {
	fprintf(stderr, "%s: cache_in: b_size failed\n", prog_name);
	return err;
    }

    /* Make sure we have a big enough buffer for the file */
    if (cached_buf == NULL) {
	if (filesize < CACHE_FIRSTTRY) {
	    /* To reduce memory fragmentation problems,
	     * first try to allocate more than we really need now.
	     * CACHE_FIRSTTRY should be enough for most kernels.
	     */
	    cached_buf = (char *) malloc((size_t) CACHE_FIRSTTRY);
	}

	if (cached_buf != NULL) {
	    cached_buf_size = CACHE_FIRSTTRY;
	} else {
	    cached_buf = (char *) malloc((size_t) filesize);
	    if (cached_buf != NULL) {
		cached_buf_size = filesize;
	    } else {
		fprintf(stderr, "%s: malloc(%ld) failed\n",
			prog_name, filesize);
		return STD_NOSPACE;
	    }
	}
    } else if (cached_buf_size < filesize) {
	char *newbuf;

	newbuf = (char *) realloc((_VOIDSTAR) cached_buf, (size_t) filesize);
	if (newbuf != NULL) {
	    cached_buf = newbuf;
	    cached_buf_size = filesize;
	} else {
	    fprintf(stderr, "%s: realloc(buf, %ld) failed\n",
		    prog_name, filesize);
	    return STD_NOSPACE;
	}
    }

    /* Now try to cache in the file */
    err = b_read(cap, (b_fsize) 0, cached_buf, filesize, &didread);
    if (err != STD_OK) {
	fprintf(stderr, "%s: cache_in: b_read failed (%s)\n",
		prog_name, err_why(err));
    } else if (didread != filesize) {
	fprintf(stderr, "%s: cache_in: read %ld instead of %ld bytes\n",
		prog_name, didread, filesize);
	err = STD_SYSERR;
    }
    if (err != STD_OK) {
	/* The cache was modified, so invalidate cached_cap */
	static capability nullcap;

	cached_size = 0;
	cached_cap = nullcap;
	return err;
    }

    /* OK, done.  Now update the administration. */
    cached_size = filesize;
    cached_cap = *cap;
    return STD_OK;
}

static errstat
do_read(cap, off, buf, size, bytesread)
capability *cap;
b_fsize     off;
char       *buf;
b_fsize	    size;
b_fsize	   *bytesread;
{
    /*
     * Use simple file cache to avoid lots of RPCs with the file server
     * if many kernels are booting the same kernel (almost) at the same
     * time.  Currently caches only one file.
     */
    static unsigned long cache_time;
    errstat err;
    
    if (!docache) {
	/* No file caching at all */
	return b_read(cap, off, buf, size, bytesread);
    }

    mu_lock(&cache_mutex);
    if (!cap_cmp(&cached_cap, cap)) {
	err = STD_NOTNOW;
	if (off == 0) {
	    /* First request for an uncached binary.  If we used the currently
	     * cached binary a long time ago, cache in the new one.
	     * Otherwise just read the data needed from the file server.
	     */
	    unsigned long now = sys_milli();

	    if (! ((now > cache_time) && (now < cache_time + CACHE_KEEP))) {
		err = cache_in(cap);
		if (err == STD_NOSPACE) {
		    /* If allocation failed, no sense in trying again soon */
		    cache_time = sys_milli();
		}
	    }
	}

	if (err != STD_OK) {
	    dprintf(("%s: b_read block at off 0x%lx\n", prog_name, off));
	    mu_unlock(&cache_mutex);
	    return b_read(cap, off, buf, size, bytesread);
	}
    }

    /* Use data from cached copy: */
    
    if (off > cached_size) {
	err = STD_ARGBAD;
    } else {
	if (off + size > cached_size) {
	    *bytesread = cached_size - off;
	} else {
	    *bytesread = size;
	}

	(void) memmove((_VOIDSTAR) buf, (_VOIDSTAR) (cached_buf + off),
		       (size_t) *bytesread);
	err = STD_OK;
    }

    /* update timer */
    cache_time = sys_milli();
    mu_unlock(&cache_mutex);
    return err;
}

size_t handle(tp)
register struct pkt *tp;
{
	register int i;
	char *fname;
	register struct tftpconn *conn;
	b_fsize bytesread;
	errstat err;

	switch (ntohs(tp->tftp_pkt.tftp_opcode)) {
	default:
	    printf("%s: unexpected opcode %d\n",
		   prog_name, ntohs(tp->tftp_pkt.tftp_opcode));
	    return 0;
	case TFTP_OPC_RRQ:
	    if (verbose) {
		mu_lock(&print_mutex);
		printf("%s: got tftp request from 0x%x.%d for 0x%x.%d\n",
		       prog_name,
		       tp->udp_hdr.uih_src_addr, tp->udp_hdr.uih_src_port,
		       tp->udp_hdr.uih_dst_addr, tp->udp_hdr.uih_dst_port);
		printf("Requested file: %s, requested service: %s\n",
		       tp->tftp_pkt.Tftp_fnammod,
		       tp->tftp_pkt.Tftp_fnammod +
		           strlen(tp->tftp_pkt.Tftp_fnammod) + 1);
		mu_unlock(&print_mutex);
	    }

	    mu_lock(&conn_mutex);
	    /* Look for a duplicate request. */
	    for (i = 0; i < NTFTPCONN; i++)
	    {
		if (tftpconn[i].tc_ipaddr == tp->udp_hdr.uih_src_addr &&
		    tftpconn[i].tc_udpport == tp->udp_hdr.uih_src_port)
			break;
	    }
	    if (i < NTFTPCONN)
	    {
		/* We already received a tftp request from this machine.
		 * If the last request happened more than a few seconds ago,
		 * clear the old connection and create a new one.
		 */
		if (tftpconn[i].tc_lastused + USE_TIMEOUT < sys_milli())
		{
		    tftpconn[i].tc_ipaddr = 0;
		    tftpconn[i].tc_flags = 0;
		    i = NTFTPCONN;
		}
		else
		{
		    /* Otherwise we assume it is a retransmission, so we
		     * reuse the connection, and resend the first data block.
		     */
		    mu_lock(&print_mutex);
		    printf("%s: duplicate request from 0x%x\n",
			   prog_name, tftpconn[i].tc_ipaddr);
		    mu_unlock(&print_mutex);
		}
	    }

	    if (i >= NTFTPCONN) {
		/* Connections are recycled using round-robin.  As long as
		 * NTFTPCONN is a good upperbound for the number of machines
		 * booting in parallel, this should be OK.
		 */
		i = nextconn++;
		if (nextconn >= NTFTPCONN)
		    nextconn = 0;
	    }

	    fname = tp->tftp_pkt.Tftp_fnammod;
	    while (*fname == '/')
		fname++;
	    err = dir_lookup(&dircap, fname, &tftpconn[i].tc_cap);
	    if (err != STD_OK) {
		fprintf(stderr, "%s: dir_lookup \"%s\" failed (%s)\n",
			prog_name, fname, err_why(err));
		mu_unlock(&conn_mutex);
		return 0;
	    } else {
		dprintf(("%s: looked up file \"%s\"\n", prog_name, fname));
	    }
	    tftpconn[i].tc_flags = TCF_INUSE;
	    tftpconn[i].tc_blkno = 0;
	    tftpconn[i].tc_ipaddr = tp->udp_hdr.uih_src_addr;
	    tftpconn[i].tc_udpport = tp->udp_hdr.uih_src_port;
	    tp->tftp_pkt.Tftp_blockno = 0;
	    mu_unlock(&conn_mutex);
	    /* fall through to send first data block! */

	case TFTP_OPC_ACK:
	    /* Look for the connection. */
	    for (i = 0; i < NTFTPCONN; i++)
	    {
		if (tftpconn[i].tc_ipaddr == tp->udp_hdr.uih_src_addr &&
		    tftpconn[i].tc_udpport == tp->udp_hdr.uih_src_port)
			break;
	    }
	    if (i == NTFTPCONN)
	    {
		if (verbose) {
		    mu_lock(&print_mutex);
		    printf("%s: no connection for %X; request ignored\n",
			   prog_name, tp->udp_hdr.uih_src_addr);
		    mu_unlock(&print_mutex);
		}
		return 0;
	    }
	    conn = &tftpconn[i];
	    tp->tftp_pkt.Tftp_blockno = ntohs(tp->tftp_pkt.Tftp_blockno);
	    if (conn->tc_blkno != tp->tftp_pkt.Tftp_blockno)
	    {
		if (!(conn->tc_flags & TCF_DROPPED))
		{
		    if (verbose) {
			mu_lock(&print_mutex);
			printf("%s: ignoring ack of %d, current is %d\n",
			       prog_name, tp->tftp_pkt.Tftp_blockno,
			       conn->tc_blkno);
			mu_unlock(&print_mutex);
		    }
		    conn->tc_flags |= TCF_DROPPED;
		    return 0;
		}
		else
		{
		    conn->tc_blkno = tp->tftp_pkt.Tftp_blockno;
		}
	    }
	    conn->tc_flags &= ~TCF_DROPPED;

	    /*
	     * Ok, read the next block
	     */
	    err = do_read(&conn->tc_cap, 512L*conn->tc_blkno,
			  tp->tftp_pkt.Tftp_data, 512L, &bytesread);
	    if (err != STD_OK)
	    {
		if (err != STD_ARGBAD)	/* ARGBAD signals eof */
		    fprintf(stderr, "%s: unable to b_read: %s\n",
			    prog_name, err_why(err));
		return 0;
	    }

	    /*
	     * It is read. Make up packet.
	     */
	    tp->udp_hdr.uih_dst_addr = tp->udp_hdr.uih_src_addr;
	    tp->udp_hdr.uih_dst_port = tp->udp_hdr.uih_src_port;

	    conn->tc_blkno++;
	    tp->tftp_pkt.tftp_opcode = htons(TFTP_OPC_DATA);
	    tp->tftp_pkt.Tftp_blockno = htons(conn->tc_blkno);
	    if (bytesread < 512)
		conn->tc_flags = 0;
	    conn->tc_lastused = sys_milli();
	    return offsetof(pkt_t, tftp_pkt.Tftp_data[0])+bytesread;
	}
	/*NOTREACHED*/
}

char *chan_cap_name;
mutex chan_mutex;

/*ARGSUSED*/
void svr_thread(param, size)
char *param;
{
	static int threadnogen;
	int threadno;
	pkt_t pkt;
	nwio_udpopt_t udpopt;
	capability old_chan_cap;
	capability chan_cap;		/* Cap of udp channel */
	errstat result;
	char chan_cap_name_number[100];

	threadno = ++threadnogen;
	if (chan_cap_name)
	{
		sprintf(chan_cap_name_number, "%s:%03d",
			chan_cap_name, threadno);
		result= name_lookup(chan_cap_name_number, &old_chan_cap);
		if (result == STD_OK)
		{
			std_destroy(&old_chan_cap);
			name_delete(chan_cap_name_number);
		}
	}

	mu_lock(&chan_mutex);

	result= udp_connect(&udp_cap, &chan_cap, htons(UDP_TFTP), 0,
		(ipaddr_t)0, (int) NWUO_SHARED);
	if (result != STD_OK)
	{
		fprintf(stderr, "%s: fatal: unable to udp_connect: %s\n",
			prog_name, err_why(result));
		exit(1);
	}
	if (chan_cap_name)
	{
		result= name_append(chan_cap_name_number, &chan_cap);
		if (result != STD_OK)
		{
			fprintf(stderr, "%s: fatal: cannot append %s (%s)\n",
				chan_cap_name_number, err_why(result));
			(void) std_destroy(&chan_cap);
			exit(1);
		}
	}
	udpopt.nwuo_flags= NWUO_EN_BROAD;
	result= udp_ioc_setopt(&chan_cap, &udpopt);
	if (result != STD_OK)
	{
		fprintf(stderr, "%s: fatal: unable to udp_ioc_setopt: %s\n",
			prog_name, err_why(result));
		exit(1);
	}

	mu_unlock(&chan_mutex);

	for (;;) {	/* Just continue handling requests */
		size_t res_size;

		result= udp_read_msg(&chan_cap, (char *)&pkt, sizeof(pkt),
			(int *) NULL, 0);
		if (result<0)
		{
			fprintf(stderr,"%s: fatal: udp_read_msg failed (%s)\n",
				prog_name, err_why(result));
			exit(1);
		}
		res_size = handle(&pkt);
		if (res_size > 0)
		{
			dprintf(("doing udp_write_msg\n"));
			result= udp_write_msg(&chan_cap, (char *)&pkt,
					      (int) res_size, 0);
			if (result < 0)
			{
				fprintf(stderr,
					"%s: udp_write_msg failed (%s)\n",
					prog_name, err_why(result));
			}
		}
	}
}

int main(argc, argv)
int argc;
char *argv[];
{
	int argindex;
	char *udp_svr_name; 
	errstat result;
	int i;

	prog_name= argv[0];

	udp_svr_name = (char *) 0;
	chan_cap_name = (char *) 0;

	mu_init(&conn_mutex);
	mu_init(&chan_mutex);
	mu_init(&cache_mutex);
	mu_init(&print_mutex);

	for (argindex= 1; argindex < argc; argindex++)
	{
		if (!strcmp(argv[argindex], "-?"))
			usage();
		if (!strcmp(argv[argindex], "-v")) {
			verbose = 1;
			continue;
		}
		if (!strcmp(argv[argindex], "-cache")) {
			docache = 1;
			continue;
		}
		if (!strcmp(argv[argindex], "-U"))
		{
			argindex++;
			if (udp_svr_name)
				usage();
			if (argindex >= argc)
				usage();
			udp_svr_name= argv[argindex];
			continue;
		}
		if (!strcmp(argv[argindex], "-chan"))
		{
			argindex++;
			if (chan_cap_name)
				usage();
			if (argindex >= argc)
				usage();
			chan_cap_name= argv[argindex];
			continue;
		}
		usage();
	}
	if (!udp_svr_name)
	{
		udp_svr_name= getenv("UDP_SERVER");
		if (udp_svr_name && !udp_svr_name[0])
			udp_svr_name= 0;
	}
	if (!udp_svr_name)
		udp_svr_name= UDP_SVR_NAME;
	result= ip_host_lookup(udp_svr_name, "udp", &udp_cap);
	if (result != STD_OK)
	{
		fprintf(stderr, "%s: fatal: unable to lookup '%s': %s\n",
			prog_name, udp_svr_name, err_why(result));
		exit(1);
	}
	if (name_lookup(TFTPBOOT_DIR, &dircap)!=STD_OK) {
		fprintf(stderr, "%s: fatal: cannot lookup %s\n",
			prog_name, TFTPBOOT_DIR);
		exit(-1);
	}
	for (i=1; i < NTFTPTHREAD; i++)
		thread_newthread(svr_thread, 8192, (char *) 0, 0);
	svr_thread( (char *) 0, 0);
	/*NOTREACHED*/
}

void usage()
{
	fprintf(stderr,
	       "usage: %s [-U <udp-cap>] [-chan <chan-cap>] [-cache] [-v]\n",
		prog_name);
	exit(1);
}
