/*	@(#)sr.c	1.7	96/02/27 14:15:03 */
/*
 * This file implements the server interface with amoeba clients.
 */

#include <stdlib.h>

#include "inet.h"
#include "amoeba_incl.h"

#include "generic/assert.h"
#include "generic/buf.h"
#include "generic/clock.h"
#include "generic/type.h"
#include "generic/sr.h"

#include "am_sr.h"

INIT_PANIC();

#ifndef SR_FD_NR
#define SR_FD_NR		256
#endif
#define SR_REC_STACK		8192

#ifndef SR_THREAD_NR
#define SR_THREAD_NR		32
#endif

/* Self destruction after between 1 minute and 10 minutes. */
#define SR_MIN_DESTR_TO		(60L * HZ)
#define SR_MAX_DESTR_TO		(10 * 60L * HZ)

#ifndef SR_CONCURR_THREADS
#define SR_CONCURR_THREADS	4
#endif

#undef	minor

#define MAX_GETREQ		30000

#if DEBUG
#define whereThread() printf("%s, %d, thread %d: ", __FILE__, __LINE__, \
	sr_thread-sr_thread_table)
#endif
	
struct sr_fd;

typedef struct sr_thread
{
	semaphore srt_block;
	acc_t *srt_req;
	acc_t *srt_repl;
	struct sr_thread *srt_next_thread;
	int srt_released;
	int srt_intr;
	command srt_status;
	struct sr_fd *srt_sr_fd;
	int srt_cancel_op;
} sr_thread_t;

#ifdef STATISTICS
typedef struct sr_stat
{
	long stat_TCPIP_OPEN;
	long stat_TCPIP_MKCAP;
	long stat_TCPIP_ETH_SETOPT;
	long stat_TCPIP_ETH_GETOPT;
	long stat_TCPIP_ETH_GETSTAT;
	long stat_TCPIP_IP_SETCONF;
	long stat_TCPIP_IP_GETCONF;
	long stat_TCPIP_IP_SETOPT;
	long stat_TCPIP_IP_GETOPT;
	long stat_TCPIP_IP_GETROUTE;
	long stat_TCPIP_IP_SETROUTE;
	long stat_TCPIP_TCP_SETCONF;
	long stat_TCPIP_TCP_GETCONF;
	long stat_TCPIP_TCP_SHUTDOWN;
	long stat_TCPIP_TCP_LISTEN;
	long stat_TCPIP_TCP_CONNECT;
	long stat_TCPIP_TCP_SETOPT;
	long stat_TCPIP_TCP_GETOPT;
	long stat_TCPIP_UDP_SETOPT;
	long stat_TCPIP_UDP_GETOPT;
	long stat_TCPIP_IOCTL;
	long stat_TCPIP_READ;
	long stat_TCPIP_WRITE;
	long stat_TCPIP_KEEPALIVE;
	long stat_STD_INFO;
	long stat_STD_RESTRICT;
	long stat_STD_DESTROY;
	long stat_bad_command;
} sr_stat_t;

sr_stat_t sr_stat_issued;
sr_stat_t sr_stat_failed;
#endif

typedef struct sr_fd
{
	int srf_flags;
	int srf_fd;
	int srf_port;

	char *srf_cap_name;
	am_port_t srf_random;

	sr_thread_t *srf_ioctl_thread;
	sr_thread_t *srf_ioctl_last_thread;

	sr_thread_t *srf_write_thread;
	sr_thread_t *srf_write_last_thread;

	sr_thread_t *srf_read_thread;
	sr_thread_t *srf_read_last_thread;

	sr_open_t srf_open;
	sr_close_t srf_close;
	sr_write_t srf_write;
	sr_read_t srf_read;
	sr_ioctl_t srf_ioctl;
	sr_cancel_t srf_cancel;

	time_t srf_creat_time;
	time_t srf_touch_time;
	timer_t srf_garbage_dog;
#ifdef STATISTICS
	sr_stat_t srf_stat_issued;
	sr_stat_t srf_stat_failed;
#endif
} sr_fd_t;

#define SFF_FLAGS 	0x0F
#	define SFF_FREE		0x00
#	define SFF_MINOR	0x01
#	define SFF_INUSE	0x02
#	define SFF_BUSY		0x3C
#		define SFF_IOCTL_IP	0x04
#		define SFF_READ_IP	0x08
#		define SFF_WRITE_IP	0x10
#		define SFF_OPEN_IP	0x20
#	define SFF_PENDING_REQ	0x30
#	define SFF_SUSPENDED	0x1C0
#		define SFF_IOCTL_SP	0x40
#		define SFF_READ_SP	0x80
#		define SFF_WRITE_SP	0x100

static acc_t *sr_get_userdata ARGS(( int fd, size_t offset,
	size_t count, int for_ioctl ));
static int sr_put_userdata ARGS(( int fd, size_t offset,
	acc_t *data, int for_ioctl ));
static int sr_check_cap ARGS(( private *, rights_bits *, sr_fd_t ** ));
static command sr_open ARGS(( sr_fd_t *sr_fd,
	am_header_t *trans_header, int linger ));
static command sr_read_write_ioctl ARGS(( sr_thread_t *sr_thread, 
	sr_fd_t *sr_fd, am_header_t *trans_header ));
static void sr_enqueue ARGS(( sr_thread_t *sr_thread,
	sr_thread_t **thread_head, sr_thread_t **thread_tail ));
static void sr_dequeue ARGS(( sr_thread_t *sr_thread,
	sr_thread_t **thread_head, sr_thread_t **thread_tail ));
static void sr_cancel ARGS(( sr_thread_t *sr_thread ));
static errstat sr_destroy ARGS(( sr_fd_t *sr_fd ));
static int sr_dummy_ioctl ARGS(( int fd, unsigned long req ));
static int sr_dummy_read ARGS(( int fd, size_t size ));
static int sr_dummy_write ARGS(( int fd, size_t size ));
static void sr_garbage_dog ARGS(( int sr_fd_nr, struct timer *timer ));
static void do_status ARGS(( void ));
static errstat do_mkcap _ARGS(( int obj, am_header_t *hdr ));
static char *bprint_interval ARGS(( char *buf, char *buf_end, long iv ));
#if STATISTICS
static char *sr_bprintf_stats ARGS(( char *buf, char *buf_end, 
	struct sr_stat *stats_issued, struct sr_stat *stats_failed ));
static void clear_stats ARGS(( struct sr_stat *stats ));
#endif
static void sr_rec ARGS(( void ));
#if AM_KERNEL
static void sr_trans_catcher ARGS(( int thread_no, signum sig ));
#else
static void sr_trans_catcher ARGS(( signum sig, thread_ustate *us,
	char *extra ));
#endif
#ifdef BUF_CONSISTENCY_CHECK
static void sr_buffree ARGS(( int priority ));
static void sr_bufcheck ARGS(( void ));
#endif

static sr_fd_t *sr_fd_table;
static sr_thread_t sr_thread_table[SR_THREAD_NR];
static am_port_t tcpip_public_port, tcpip_private_port;
static int linger_right= FALSE;
static int last_thread;
static semaphore getreq_block;
static mutex newthread_mutex;
static char reply[10240];

PUBLIC int sr_add_minor(minor, port, openf, closef, readf, writef,
	ioctlf, cancelf)
int minor;
int port;
sr_open_t openf;
sr_close_t closef;
sr_read_t readf;
sr_write_t writef;
sr_ioctl_t ioctlf;
sr_cancel_t cancelf;
{
	capability public_cap;
	int result;
	errstat error;
	sr_fd_t *sr_fd;
	rights_bits open_rights;

	assert (minor>=0 && minor<SR_FD_NR);

	sr_fd= &sr_fd_table[minor];

	if (sr_fd->srf_flags & SFF_INUSE)
	{
		DBLOCK(2, printf("sr_add_minor: minor %d already in use\n",
			minor));
		return ERROR;
	}

	if (sr_fd->srf_ioctl_thread || sr_fd->srf_read_thread ||
		sr_fd->srf_write_thread)
	{
		DBLOCK(2, printf(
		"sr_add_minor: fd already in use use: %lx, %lx, %lx\n",
		   sr_fd->srf_ioctl_thread, sr_fd->srf_read_thread,
		   sr_fd->srf_write_thread));
		return ERROR;
	}

	if (!sr_fd->srf_cap_name)
	{
		printf("%s: Warning no name for minor device %d\n",
			prog_name, minor);
		return -1;
	}

	uniqport(&sr_fd->srf_random);
	public_cap.cap_port= tcpip_public_port;
	open_rights= IP_RIGHTS_OPEN |
		(linger_right ? IP_RIGHTS_LINGER : 0) | IP_RIGHTS_SUPER;
	result= prv_encode(&public_cap.cap_priv, (objnum)minor, open_rights,
		&sr_fd->srf_random);
	if (result)
		ip_panic(( "unable to initialize capability" ));

#if AM_KERNEL
	append_name(sr_fd->srf_cap_name, &public_cap);
#else
	(void) name_delete(sr_fd->srf_cap_name);
	error= name_append(sr_fd->srf_cap_name, &public_cap);
	if (error != STD_OK)
	{
		printf("unable to append '%s': %s\n", sr_fd->srf_cap_name,
			err_why(error));
		ip_panic(( "name_append failed" ));
	}
#endif

	sr_fd->srf_flags= SFF_INUSE | SFF_MINOR;
	sr_fd->srf_port= port;
	sr_fd->srf_open= openf;
	sr_fd->srf_close= closef;
	sr_fd->srf_write= writef;
	sr_fd->srf_read= readf;
	sr_fd->srf_ioctl= ioctlf;
	sr_fd->srf_cancel= cancelf;

	return NW_OK;
}

PUBLIC void sr_init()
{
	int i;
	int *thread_no_ptr;
	sr_fd_t *sr_fd;

	uniqport(&tcpip_private_port);
	priv2pub(&tcpip_private_port, &tcpip_public_port);
	DBLOCK(2, printf("pub port= %p, priv port= %p\n", &tcpip_public_port,
		&tcpip_private_port));
	for (i=0, sr_fd= sr_fd_table; i<SR_FD_NR; i++, sr_fd++)
	{
		sr_fd->srf_flags= SFF_FREE;
		sr_fd->srf_ioctl_thread= 0;
		sr_fd->srf_read_thread= 0;
		sr_fd->srf_write_thread= 0;
	}
	sema_init(&getreq_block, SR_CONCURR_THREADS);
	mu_init(&newthread_mutex);
	last_thread= 0;

	for (i= 0; i< SR_THREAD_NR;i++)
	{
		mu_lock(&newthread_mutex);
		new_thread(sr_rec, SR_REC_STACK);
	}
	mu_lock(&newthread_mutex);

#ifdef BUF_CONSISTENCY_CHECK
	bf_logon(sr_buffree, sr_bufcheck);
#endif
}

PUBLIC void sr_init_cap_names()
{
	int i;

	printf("sr.c: allocating %d sr_fds (%dK)\n", SR_FD_NR,
		SR_FD_NR * sizeof(*sr_fd_table) / 1024);
	sr_fd_table= malloc(SR_FD_NR * sizeof(*sr_fd_table));
	if (!sr_fd_table)
		ip_panic(( "unable to allocate sr_fd_table" ));
	for (i=0; i<SR_FD_NR; i++) {
		(void) memset(&sr_fd_table[i], '\0', sizeof(*sr_fd_table));
		sr_fd_table[i].srf_cap_name= NULL;
	}
}

PUBLIC void sr_set_cap_name(minor, cap_name)
int minor;
char *cap_name;
{
	if (minor<0 || minor>=SR_FD_NR || sr_fd_table[minor].srf_cap_name)
		ip_panic(( "no capability name for device %d", minor ));
	DBLOCK(2, printf("cap name of minor %d is %s\n", minor, cap_name));
	sr_fd_table[minor].srf_cap_name= cap_name;
}

static void sr_rec()
{
	acc_t *req_acc, *repl_acc;
	am_bufsize_t req_size, repl_size;
	am_header_t trans_header;
	char *repl_buf;
	int result;
	rights_bits rights;
	sr_fd_t *sr_fd;
	int thread_no;
	sr_thread_t *sr_thread;
	errstat error;
	char *mars_ptr, *mars_end;
#ifdef STATISTICS
	long *glob_stat_failed, *loc_stat_failed;
#endif

	thread_no= last_thread++;
	assert (thread_no >= 0 && thread_no < SR_THREAD_NR);
	sr_thread= &sr_thread_table[thread_no];

	req_acc= 0;

#if AM_KERNEL
	getsig(sr_trans_catcher, thread_no);
#else
	error= sig_catch(SIG_TRANS, sr_trans_catcher, (char *)sr_thread);
	if (error != STD_OK)
	{
		printf("%s: unable to set signal handler: %s\n",
			err_why(error));
		exit(1);
	}
#endif

	mu_unlock(&newthread_mutex);
	mu_lock(&mu_generic);

	for (;;)
	{
		assert (!req_acc);
		assert (!sr_thread->srt_req);
		assert (!sr_thread->srt_repl);

		sr_thread->srt_released= 0;
		sr_thread->srt_intr= 0;
#ifdef STATISTICS
		glob_stat_failed= NULL;
		loc_stat_failed= NULL;
#endif

		mu_unlock(&mu_generic);
		sema_down(&getreq_block);
		mu_lock(&mu_generic);

		req_acc= bf_memreq(BUF_S);
		assert (req_acc);
		assert (!req_acc->acc_next);

		/* Store the acc to be able to do the buf consistency check.
		 */
		sr_thread->srt_req= req_acc;

		/* Allow other threads to use the generic code after
		 * this point.
		 */
		mu_unlock(&mu_generic);

		/* static assertion */
		assert (BUF_S > MAX_GETREQ);

		trans_header.h_port= tcpip_private_port;
		req_size= rpc_getreq(&trans_header, ptr2acc_data(req_acc), 
			MAX_GETREQ);

		mu_lock(&mu_generic);
		sr_thread->srt_req= NULL;

		if (!req_size)
		{
			bf_afree(req_acc);
			req_acc= 0;
		}
		sema_up(&getreq_block);
		sema_init (&sr_thread->srt_block, 0);
		sr_thread->srt_req= req_acc;
		req_acc= 0;

		if (ERR_STATUS(req_size))
		{
			printf("getreq failed: %s\n",
				err_why(ERR_CONVERT(req_size)));
			ip_panic(( "getreq failed" ));
		}
		repl_buf= 0;
		repl_size= 0;
		switch(trans_header.h_command)
		{
		case TCPIP_OPEN:
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_OPEN++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_OPEN;
#endif
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_OPEN++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_OPEN;
#endif
			if (!(rights & IP_RIGHTS_OPEN))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			trans_header.h_status= sr_open(sr_fd, &trans_header,
						!!(rights & IP_RIGHTS_LINGER));
			assert (!sr_thread->srt_repl);
			break;
		case TCPIP_MKCAP:
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_MKCAP++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_MKCAP;
#endif
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_MKCAP++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_MKCAP;
#endif
			if (!(rights & IP_RIGHTS_SUPER))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			trans_header.h_status= do_mkcap(trans_header.h_extra,
				&trans_header);
			assert (!sr_thread->srt_repl);
			break;
		case TCPIP_ETH_GETOPT:
		{
			struct nwio_ethopt *ethopt;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_ETH_GETOPT++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_ETH_GETOPT;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_ETH_GETOPT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_ETH_GETOPT;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			if (sr_thread->srt_req)
			{
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOGETHOPT;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			if (ERR_STATUS(trans_header.h_status))
				break;
			assert (sr_thread->srt_repl != 0);
			repl_acc= sr_thread->srt_repl;
			assert (bf_bufsize(repl_acc) == sizeof(*ethopt));
			if (repl_acc->acc_next)
				repl_acc= bf_pack(repl_acc);
			assert (!repl_acc->acc_next);
			ethopt= (struct nwio_ethopt *)ptr2acc_data(repl_acc);
			sr_thread->srt_repl= bf_memreq(BUF_S);
			assert (sr_thread->srt_repl != 0);
			mars_ptr= ptr2acc_data(sr_thread->srt_repl);
			mars_end= mars_ptr+BUF_S;

			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
				ethopt->nweo_flags);
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&ethopt->nweo_multi,
				sizeof(ethopt->nweo_multi));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&ethopt->nweo_rem,
				sizeof(ethopt->nweo_rem));
			mars_ptr= buf_put_uint16(mars_ptr, mars_end,
				ethopt->nweo_type);

			assert (mars_ptr);
			bf_afree(repl_acc);
			repl_acc= bf_cut(sr_thread->srt_repl, 0,
				mars_ptr-ptr2acc_data(sr_thread->srt_repl));
			bf_afree(sr_thread->srt_repl);
			sr_thread->srt_repl= repl_acc;
			assert (sr_thread->srt_repl != 0);
			repl_acc= 0;
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_ETH_SETOPT:
		{
			struct nwio_ethopt *ethopt;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_ETH_SETOPT++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_ETH_SETOPT;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_ETH_SETOPT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_ETH_SETOPT;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			req_acc= bf_memreq(sizeof(struct nwio_ethopt));
			assert (!sr_thread->srt_req->acc_next);
			assert (!req_acc->acc_next);
			ethopt= (struct nwio_ethopt *)ptr2acc_data(req_acc);
			mars_ptr= ptr2acc_data(sr_thread->srt_req);
			mars_end= ptr2acc_data(sr_thread->srt_req)+req_size;
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&ethopt->nweo_flags);
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				&ethopt->nweo_multi,
				sizeof(ethopt->nweo_multi));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				&ethopt->nweo_rem,
				sizeof(ethopt->nweo_rem));
			mars_ptr= buf_get_uint16(mars_ptr, mars_end,
				&ethopt->nweo_type);
			if (mars_ptr != mars_end)
			{
				bf_afree(req_acc);
				req_acc= 0;
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= req_acc;
			req_acc= 0;
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOSETHOPT;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_ETH_GETSTAT:
		{
			struct nwio_ethstat *ethstat;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_ETH_GETSTAT++;
			glob_stat_failed= &sr_stat_failed.
				stat_TCPIP_ETH_GETSTAT;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_ETH_GETSTAT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_ETH_GETSTAT;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			if (sr_thread->srt_req)
			{
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOGETHSTAT;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			if (ERR_STATUS(trans_header.h_status))
				break;
			assert (sr_thread->srt_repl != 0);
			repl_acc= sr_thread->srt_repl;
			assert (bf_bufsize(repl_acc) == sizeof(*ethstat));
			if (repl_acc->acc_next)
				repl_acc= bf_pack(repl_acc);
			assert (!repl_acc->acc_next);
			ethstat= (struct nwio_ethstat *)ptr2acc_data(repl_acc);
			sr_thread->srt_repl= bf_memreq(BUF_S);
			assert (sr_thread->srt_repl != 0);
			mars_ptr= ptr2acc_data(sr_thread->srt_repl);
			mars_end= mars_ptr+BUF_S;
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&ethstat->nwes_addr,
				sizeof(ethstat->nwes_addr));
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_recvErr);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_sendErr);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_OVW);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_CRCerr);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_frameAll);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_missedP);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_packetR);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_packetT);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_transDef);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_collision);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_transAb);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_carrSense);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_fifoUnder);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_fifoOver);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_CDheartbeat);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
					ethstat->nwes_stat.ets_OWC);
			assert (mars_ptr);
			bf_afree(repl_acc);
			repl_acc= bf_cut(sr_thread->srt_repl, 0,
				mars_ptr-ptr2acc_data(sr_thread->srt_repl));
			bf_afree(sr_thread->srt_repl);
			sr_thread->srt_repl= repl_acc;
			assert (sr_thread->srt_repl != 0);
			repl_acc= 0;
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_IP_SETCONF:
		{
			struct nwio_ipconf *ipconf;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_IP_SETCONF++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_IP_SETCONF;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_IP_SETCONF++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_IP_SETCONF;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			req_acc= bf_memreq(sizeof(struct nwio_ipconf));
			assert (!sr_thread->srt_req->acc_next);
			assert (!req_acc->acc_next);
			ipconf= (struct nwio_ipconf *)ptr2acc_data(req_acc);
			mars_ptr= ptr2acc_data(sr_thread->srt_req);
			mars_end= ptr2acc_data(sr_thread->srt_req)+req_size;
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&ipconf->nwic_flags);
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				&ipconf->nwic_ipaddr,
				sizeof(ipconf->nwic_ipaddr));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				&ipconf->nwic_netmask,
				sizeof(ipconf->nwic_netmask));
			if (mars_ptr != mars_end)
			{
				bf_afree(req_acc);
				req_acc= NULL;
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= req_acc;
			req_acc= 0;
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOSIPCONF;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_IP_GETCONF:
		{
			struct nwio_ipconf *ipconf;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_IP_GETCONF++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_IP_GETCONF;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_IP_GETCONF++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_IP_GETCONF;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			if (sr_thread->srt_req)
			{
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOGIPCONF;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			if (ERR_STATUS(trans_header.h_status))
				break;
			repl_acc= sr_thread->srt_repl;
			assert (bf_bufsize(repl_acc) == sizeof(*ipconf));
			if (repl_acc->acc_next)
				repl_acc= bf_pack(repl_acc);
			assert (!repl_acc->acc_next);
			ipconf= (struct nwio_ipconf *)ptr2acc_data(repl_acc);
			sr_thread->srt_repl= bf_memreq(BUF_S);
			assert (sr_thread->srt_repl != 0);
			mars_ptr= ptr2acc_data(sr_thread->srt_repl);
			mars_end= mars_ptr+BUF_S;
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							ipconf->nwic_flags);
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&ipconf->nwic_ipaddr,
				sizeof(ipconf->nwic_ipaddr));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&ipconf->nwic_netmask,
				sizeof(ipconf->nwic_netmask));
			assert (mars_ptr);
			bf_afree(repl_acc);
			repl_acc= bf_cut(sr_thread->srt_repl, 0,
				mars_ptr-ptr2acc_data(sr_thread->srt_repl));
			bf_afree(sr_thread->srt_repl);
			sr_thread->srt_repl= repl_acc;
			repl_acc= 0;
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_IP_GETOPT:
		{
			struct nwio_ipopt *ipopt;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_IP_GETOPT++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_IP_GETOPT;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_IP_GETOPT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_IP_GETOPT;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			if (sr_thread->srt_req)
			{
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOGIPOPT;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			if (ERR_STATUS(trans_header.h_status))
				break;
			assert (sr_thread->srt_repl != 0);
			repl_acc= sr_thread->srt_repl;
			assert (bf_bufsize(repl_acc) == sizeof(*ipopt));
			if (repl_acc->acc_next)
				repl_acc= bf_pack(repl_acc);
			assert (!repl_acc->acc_next);
			ipopt= (struct nwio_ipopt *)ptr2acc_data(repl_acc);
			sr_thread->srt_repl= bf_memreq(BUF_S);
			assert (sr_thread->srt_repl != 0);
			mars_ptr= ptr2acc_data(sr_thread->srt_repl);
			mars_end= mars_ptr+BUF_S;

			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							ipopt->nwio_flags);
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&ipopt->nwio_rem, sizeof(ipopt->nwio_rem));
			mars_ptr= buf_put_uint8(mars_ptr, mars_end,
				ipopt->nwio_hdropt.iho_opt_siz);
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&ipopt->nwio_hdropt.iho_data[0],
				sizeof(ipopt->nwio_hdropt.iho_data));
			mars_ptr= buf_put_uint8(mars_ptr, mars_end,
				ipopt->nwio_tos);
			mars_ptr= buf_put_uint8(mars_ptr, mars_end,
				ipopt->nwio_ttl);
			mars_ptr= buf_put_uint8(mars_ptr, mars_end,
				ipopt->nwio_df);
			mars_ptr= buf_put_uint8(mars_ptr, mars_end,
				ipopt->nwio_proto);
			assert (mars_ptr);
			bf_afree(repl_acc);
			repl_acc= bf_cut(sr_thread->srt_repl, 0,
				mars_ptr-ptr2acc_data(sr_thread->srt_repl));
			bf_afree(sr_thread->srt_repl);
			sr_thread->srt_repl= repl_acc;
			assert (sr_thread->srt_repl != 0);
			repl_acc= 0;
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_IP_SETOPT:
		{
			struct nwio_ipopt *ipopt;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_IP_SETOPT++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_IP_SETOPT;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_IP_SETOPT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_IP_SETOPT;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			req_acc= bf_memreq(sizeof(struct nwio_ipopt));
			assert (!sr_thread->srt_req->acc_next);
			assert (!req_acc->acc_next);
			ipopt= (struct nwio_ipopt *)ptr2acc_data(req_acc);
			mars_ptr= ptr2acc_data(sr_thread->srt_req);
			mars_end= ptr2acc_data(sr_thread->srt_req)+req_size;
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&ipopt->nwio_flags);
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				&ipopt->nwio_rem, sizeof(ipopt->nwio_rem));
			mars_ptr= buf_get_uint8(mars_ptr, mars_end,
				&ipopt->nwio_hdropt.iho_opt_siz);
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				&ipopt->nwio_hdropt.iho_data[0],
				sizeof(ipopt->nwio_hdropt.iho_data));
			mars_ptr= buf_get_uint8(mars_ptr, mars_end,
				&ipopt->nwio_tos);
			mars_ptr= buf_get_uint8(mars_ptr, mars_end,
				&ipopt->nwio_ttl);
			mars_ptr= buf_get_uint8(mars_ptr, mars_end,
				&ipopt->nwio_df);
			mars_ptr= buf_get_uint8(mars_ptr, mars_end,
				&ipopt->nwio_proto);
			if (mars_ptr != mars_end)
			{
				bf_afree(req_acc);
				req_acc= NULL;
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= req_acc;
			req_acc= 0;
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOSIPOPT;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_IP_GETROUTE:
		{
			struct nwio_route *route;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_IP_GETROUTE++;
			glob_stat_failed= &sr_stat_failed.
				stat_TCPIP_IP_GETROUTE;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_IP_GETROUTE++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_IP_GETROUTE;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			req_acc= bf_memreq(sizeof(struct nwio_route));
			assert (!sr_thread->srt_req->acc_next);
			assert (!req_acc->acc_next);
			route= (struct nwio_route *)ptr2acc_data(req_acc);
			mars_ptr= ptr2acc_data(sr_thread->srt_req);
			mars_end= ptr2acc_data(sr_thread->srt_req)+req_size;
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_ent_no);
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_ent_count);
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				&route->nwr_dest, sizeof(route->nwr_dest));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end, &route->
				nwr_netmask, sizeof(route->nwr_netmask));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end, &route->
				nwr_gateway, sizeof(route->nwr_gateway));
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_dist);
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_flags);
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_pref);
			if (mars_ptr != mars_end)
			{
				bf_afree(req_acc);
				req_acc= NULL;
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= req_acc;
			req_acc= 0;
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOGIPOROUTE;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			if (ERR_STATUS(trans_header.h_status))
				break;
			assert (sr_thread->srt_repl != 0);
			repl_acc= sr_thread->srt_repl;
			compare (bf_bufsize(repl_acc), ==, sizeof(*route));
			if (repl_acc->acc_next)
				repl_acc= bf_pack(repl_acc);
			assert (!repl_acc->acc_next);
			route= (struct nwio_route *)ptr2acc_data(repl_acc);
			sr_thread->srt_repl= bf_memreq(BUF_S);
			assert (sr_thread->srt_repl != 0);
			mars_ptr= ptr2acc_data(sr_thread->srt_repl);
			mars_end= mars_ptr+BUF_S;
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							route->nwr_ent_no);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							route->nwr_ent_count);
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&route->nwr_dest, sizeof(route->nwr_dest));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end, &route->
				nwr_netmask, sizeof(route->nwr_netmask));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end, &route->
				nwr_gateway, sizeof(route->nwr_gateway));
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							route->nwr_dist);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							route->nwr_flags);
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							route->nwr_pref);
			assert (mars_ptr);
			bf_afree(repl_acc);
			repl_acc= bf_cut(sr_thread->srt_repl, 0,
				mars_ptr-ptr2acc_data(sr_thread->srt_repl));
			bf_afree(sr_thread->srt_repl);
			sr_thread->srt_repl= repl_acc;
			assert (sr_thread->srt_repl != 0);
			repl_acc= 0;
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_IP_SETROUTE:
		{
			struct nwio_route *route;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_IP_SETROUTE++;
			glob_stat_failed= &sr_stat_failed.
				stat_TCPIP_IP_SETROUTE;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_IP_SETROUTE++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_IP_SETROUTE;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			req_acc= bf_memreq(sizeof(struct nwio_route));
			assert (!sr_thread->srt_req->acc_next);
			assert (!req_acc->acc_next);
			route= (struct nwio_route *)ptr2acc_data(req_acc);
			mars_ptr= ptr2acc_data(sr_thread->srt_req);
			mars_end= ptr2acc_data(sr_thread->srt_req)+req_size;
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_ent_no);
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_ent_count);
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &route->nwr_dest,
				sizeof(route->nwr_dest));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &route->nwr_netmask,
				sizeof(route->nwr_netmask));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &route->nwr_gateway,
				sizeof(route->nwr_gateway));
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_dist);
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_flags);
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&route->nwr_pref);
			if (mars_ptr != mars_end)
			{
				bf_afree(req_acc);
				req_acc= NULL;
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= req_acc;
			req_acc= 0;
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOSIPOROUTE;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			break;
		}
		case TCPIP_TCP_SETCONF:
		{
			struct nwio_tcpconf *tcpconf;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_TCP_SETCONF++;
			glob_stat_failed= &sr_stat_failed.
				stat_TCPIP_TCP_SETCONF;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_TCP_SETCONF++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_TCP_SETCONF;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			req_acc= bf_memreq(sizeof(struct nwio_tcpconf));
			assert (!sr_thread->srt_req->acc_next);
			assert (!req_acc->acc_next);
			tcpconf= (struct nwio_tcpconf *)ptr2acc_data(req_acc);
			mars_ptr= ptr2acc_data(sr_thread->srt_req);
			mars_end= ptr2acc_data(sr_thread->srt_req)+req_size;
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&tcpconf->nwtc_flags);
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &tcpconf->nwtc_locaddr,
				sizeof(tcpconf->nwtc_locaddr));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &tcpconf->nwtc_remaddr,
				sizeof(tcpconf->nwtc_remaddr));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &tcpconf->nwtc_locport,
				sizeof(tcpconf->nwtc_locport));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &tcpconf->nwtc_remport,
				sizeof(tcpconf->nwtc_remport));
			if (mars_ptr != mars_end)
			{
				bf_afree(req_acc);
				req_acc= NULL;
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= req_acc;
			req_acc= 0;
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOSTCPCONF;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			break;
		}
		case TCPIP_TCP_GETCONF:
		{
			struct nwio_tcpconf *tcpconf;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_TCP_GETCONF++;
			glob_stat_failed= &sr_stat_failed.
							stat_TCPIP_TCP_GETCONF;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_TCP_GETCONF++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_TCP_GETCONF;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			if (sr_thread->srt_req)
			{
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOGTCPCONF;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			if (ERR_STATUS(trans_header.h_status))
				break;
			assert (sr_thread->srt_repl != 0);
			repl_acc= sr_thread->srt_repl;
			assert (bf_bufsize(repl_acc) == sizeof(*tcpconf));
			if (repl_acc->acc_next)
				repl_acc= bf_pack(repl_acc);
			assert (!repl_acc->acc_next);
			tcpconf= (struct nwio_tcpconf *)ptr2acc_data(repl_acc);
			sr_thread->srt_repl= bf_memreq(BUF_S);
			assert (sr_thread->srt_repl != 0);
			mars_ptr= ptr2acc_data(sr_thread->srt_repl);
			mars_end= mars_ptr+BUF_S;
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							tcpconf->nwtc_flags);
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&tcpconf->nwtc_locaddr,
				sizeof(tcpconf->nwtc_locaddr));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&tcpconf->nwtc_remaddr,
				sizeof(tcpconf->nwtc_remaddr));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&tcpconf->nwtc_locport,
				sizeof(tcpconf->nwtc_locport));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&tcpconf->nwtc_remport,
				sizeof(tcpconf->nwtc_remport));
			assert (mars_ptr);
			bf_afree(repl_acc);
			repl_acc= bf_cut(sr_thread->srt_repl, 0,
				mars_ptr-ptr2acc_data(sr_thread->srt_repl));
			bf_afree(sr_thread->srt_repl);
			sr_thread->srt_repl= repl_acc;
			assert (sr_thread->srt_repl != 0);
			repl_acc= 0;
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_TCP_SHUTDOWN:
#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_TCP_SHUTDOWN++;
			glob_stat_failed= &sr_stat_failed.
				stat_TCPIP_TCP_SHUTDOWN;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_TCP_SHUTDOWN++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_TCP_SHUTDOWN;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			trans_header.h_offset= NWIOTCPSHUTDOWN;
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			sr_fd->srf_touch_time= get_time();
			break;
		case TCPIP_TCP_LISTEN:
		case TCPIP_TCP_CONNECT:
		{
			struct nwio_tcpcl *clopt;

#ifdef STATISTICS
			switch (trans_header.h_command)
			{
			case TCPIP_TCP_LISTEN:
				sr_stat_issued.stat_TCPIP_TCP_LISTEN++;
				glob_stat_failed= &sr_stat_failed.
					stat_TCPIP_TCP_LISTEN;
				break;
			case TCPIP_TCP_CONNECT:
				sr_stat_issued.stat_TCPIP_TCP_CONNECT++;
				glob_stat_failed= &sr_stat_failed.
					stat_TCPIP_TCP_CONNECT;
				break;
			default:
				ip_panic(( "not reached" ));
			}
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			switch (trans_header.h_command)
			{
			case TCPIP_TCP_LISTEN:
				sr_fd->srf_stat_issued.stat_TCPIP_TCP_LISTEN++;
				loc_stat_failed= &sr_fd->srf_stat_failed.
					stat_TCPIP_TCP_LISTEN;
				break;
			case TCPIP_TCP_CONNECT:
				sr_fd->srf_stat_issued.stat_TCPIP_TCP_CONNECT++;
				loc_stat_failed= &sr_fd->srf_stat_failed.
					stat_TCPIP_TCP_CONNECT;
				break;
			default:
				ip_panic(( "not reached" ));
			}
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			req_acc= bf_memreq(sizeof(struct nwio_tcpcl));
			assert (!sr_thread->srt_req->acc_next);
			assert (!req_acc->acc_next);
			clopt= (struct nwio_tcpcl *)ptr2acc_data(req_acc);
			mars_ptr= ptr2acc_data(sr_thread->srt_req);
			mars_end= ptr2acc_data(sr_thread->srt_req)+req_size;
			mars_ptr= buf_get_int32(mars_ptr, mars_end,
				&clopt->nwtcl_flags);
			mars_ptr= buf_get_int32(mars_ptr, mars_end,
				&clopt->nwtcl_ttl);
			if (mars_ptr != mars_end)
			{
				bf_afree(req_acc);
				req_acc= NULL;
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= req_acc;
			req_acc= 0;
			switch (trans_header.h_command)
			{
			case TCPIP_TCP_LISTEN:
				trans_header.h_offset= NWIOTCPLISTEN;
				break;
			case TCPIP_TCP_CONNECT:
				trans_header.h_offset= NWIOTCPCONN;
				break;
			default:
				ip_panic(( "not reached" ));
			}
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_TCP_SETOPT:
		{
			struct nwio_tcpopt *tcpopt;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_TCP_SETOPT++;
			glob_stat_failed= &sr_stat_failed.
				stat_TCPIP_TCP_SETOPT;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_TCP_SETOPT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_TCP_SETOPT;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			req_acc= bf_memreq(sizeof(struct nwio_tcpopt));
			assert (!sr_thread->srt_req->acc_next);
			assert (!req_acc->acc_next);
			tcpopt= (struct nwio_tcpopt *)ptr2acc_data(req_acc);
			mars_ptr= ptr2acc_data(sr_thread->srt_req);
			mars_end= ptr2acc_data(sr_thread->srt_req)+req_size;
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&tcpopt->nwto_flags);
			if (mars_ptr != mars_end)
			{
				bf_afree(req_acc);
				req_acc= NULL;
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= req_acc;
			req_acc= 0;
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOSTCPOPT;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_TCP_GETOPT:
		{
			struct nwio_tcpopt *tcpopt;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_TCP_GETOPT++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_TCP_GETOPT;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_TCP_GETOPT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_TCP_GETOPT;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			if (sr_thread->srt_req)
			{
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOGTCPOPT;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			if (ERR_STATUS(trans_header.h_status))
				break;
			assert (sr_thread->srt_repl != 0);
			repl_acc= sr_thread->srt_repl;
			assert (bf_bufsize(repl_acc) == sizeof(*tcpopt));
			if (repl_acc->acc_next)
				repl_acc= bf_pack(repl_acc);
			assert (!repl_acc->acc_next);
			tcpopt= (struct nwio_tcpopt *)ptr2acc_data(repl_acc);
			sr_thread->srt_repl= bf_memreq(BUF_S);
			assert (sr_thread->srt_repl != 0);
			mars_ptr= ptr2acc_data(sr_thread->srt_repl);
			mars_end= mars_ptr+BUF_S;
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							tcpopt->nwto_flags);
			assert (mars_ptr);
			bf_afree(repl_acc);
			repl_acc= bf_cut(sr_thread->srt_repl, 0,
				mars_ptr-ptr2acc_data(sr_thread->srt_repl));
			bf_afree(sr_thread->srt_repl);
			sr_thread->srt_repl= repl_acc;
			assert (sr_thread->srt_repl != 0);
			repl_acc= 0;
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_UDP_SETOPT:
		{
			struct nwio_udpopt *udpopt;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_UDP_SETOPT++;
			glob_stat_failed= &sr_stat_failed.
				stat_TCPIP_UDP_SETOPT;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_UDP_SETOPT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_UDP_SETOPT;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			req_acc= bf_memreq(sizeof(struct nwio_udpopt));
			assert (!sr_thread->srt_req->acc_next);
			assert (!req_acc->acc_next);
			udpopt= (struct nwio_udpopt *)ptr2acc_data(req_acc);
			mars_ptr= ptr2acc_data(sr_thread->srt_req);
			mars_end= ptr2acc_data(sr_thread->srt_req)+req_size;
			mars_ptr= buf_get_uint32(mars_ptr, mars_end,
				&udpopt->nwuo_flags);
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &udpopt->nwuo_locport,
				sizeof(udpopt->nwuo_locport));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &udpopt->nwuo_remport,
				sizeof(udpopt->nwuo_remport));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &udpopt->nwuo_locaddr,
				sizeof(udpopt->nwuo_locaddr));
			mars_ptr= buf_get_bytes(mars_ptr, mars_end,
				(void *) &udpopt->nwuo_remaddr,
				sizeof(udpopt->nwuo_remaddr));
			if (mars_ptr != mars_end)
			{
				bf_afree(req_acc);
				req_acc= NULL;
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= req_acc;
			req_acc= 0;
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOSUDPOPT;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_UDP_GETOPT:
		{
			struct nwio_udpopt *udpopt;

#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_UDP_GETOPT++;
			glob_stat_failed= &sr_stat_failed.stat_TCPIP_UDP_GETOPT;
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_UDP_GETOPT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_UDP_GETOPT;
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			if (sr_thread->srt_req)
			{
				trans_header.h_status= STD_ARGBAD;
				break;
			}
			trans_header.h_command= TCPIP_IOCTL;
			trans_header.h_offset= NWIOGUDPOPT;
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			if (ERR_STATUS(trans_header.h_status))
				break;
			assert (sr_thread->srt_repl != 0);
			repl_acc= sr_thread->srt_repl;
			assert (bf_bufsize(repl_acc) == sizeof(*udpopt));
			if (repl_acc->acc_next)
				repl_acc= bf_pack(repl_acc);
			assert (!repl_acc->acc_next);
			udpopt= (struct nwio_udpopt *)ptr2acc_data(repl_acc);
			sr_thread->srt_repl= bf_memreq(BUF_S);
			assert (sr_thread->srt_repl != 0);
			mars_ptr= ptr2acc_data(sr_thread->srt_repl);
			mars_end= mars_ptr+BUF_S;
			mars_ptr= buf_put_uint32(mars_ptr, mars_end,
							udpopt->nwuo_flags);
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&udpopt->nwuo_locport,
				sizeof(udpopt->nwuo_locport));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&udpopt->nwuo_remport,
				sizeof(udpopt->nwuo_remport));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&udpopt->nwuo_locaddr,
				sizeof(udpopt->nwuo_locaddr));
			mars_ptr= buf_put_bytes(mars_ptr, mars_end,
				&udpopt->nwuo_remaddr,
				sizeof(udpopt->nwuo_remaddr));
			assert (mars_ptr);
			bf_afree(repl_acc);
			repl_acc= bf_cut(sr_thread->srt_repl, 0,
				mars_ptr-ptr2acc_data(sr_thread->srt_repl));
			bf_afree(sr_thread->srt_repl);
			sr_thread->srt_repl= repl_acc;
			assert (sr_thread->srt_repl != 0);
			repl_acc= 0;
			sr_fd->srf_touch_time= get_time();
			break;
		}
		case TCPIP_IOCTL:
		case TCPIP_READ:
		case TCPIP_WRITE:
#ifdef STATISTICS
			switch (trans_header.h_command)
			{
			case TCPIP_IOCTL:
				sr_stat_issued.stat_TCPIP_IOCTL++;
				glob_stat_failed= &sr_stat_failed.
					stat_TCPIP_IOCTL;
				break;
			case TCPIP_READ:
				sr_stat_issued.stat_TCPIP_READ++;
				glob_stat_failed= &sr_stat_failed.
					stat_TCPIP_READ;
				break;
			case TCPIP_WRITE:
				sr_stat_issued.stat_TCPIP_WRITE++;
				glob_stat_failed= &sr_stat_failed.
					stat_TCPIP_WRITE;
				break;
			default:
				ip_panic(( "not reached" ));
			}
#endif
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			switch (trans_header.h_command)
			{
			case TCPIP_IOCTL:
				sr_fd->srf_stat_issued.stat_TCPIP_IOCTL++;
				loc_stat_failed= &sr_fd->srf_stat_failed.
					stat_TCPIP_IOCTL;
				break;
			case TCPIP_READ:
				sr_fd->srf_stat_issued.stat_TCPIP_READ++;
				loc_stat_failed= &sr_fd->srf_stat_failed.
					stat_TCPIP_READ;
				break;
			case TCPIP_WRITE:
				sr_fd->srf_stat_issued.stat_TCPIP_WRITE++;
				loc_stat_failed= &sr_fd->srf_stat_failed.
					stat_TCPIP_WRITE;
				break;
			default:
				ip_panic(( "not reached" ));
			}
#endif
			if (!(rights & IP_RIGHTS_RWIO))
			{
				trans_header.h_status= STD_DENIED;
				break;
			}
			trans_header.h_status= sr_read_write_ioctl(sr_thread,
				sr_fd, &trans_header);
			sr_fd->srf_touch_time= get_time();
			break;
		case TCPIP_KEEPALIVE:
#ifdef STATISTICS
			sr_stat_issued.stat_TCPIP_KEEPALIVE++;
			glob_stat_failed= &sr_stat_failed.
				stat_TCPIP_KEEPALIVE;
#endif
			trans_header.h_status= STD_OK;
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_TCPIP_KEEPALIVE++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_TCPIP_KEEPALIVE;
#endif
			trans_header.h_offset= (sr_fd->srf_touch_time -
				sr_fd->srf_creat_time) >> 2;
			if (trans_header.h_offset < SR_MIN_DESTR_TO)
				trans_header.h_offset= SR_MIN_DESTR_TO;
			else if (trans_header.h_offset > SR_MAX_DESTR_TO)
				trans_header.h_offset= SR_MAX_DESTR_TO;

			/* Reply time in milliseconds! */
			trans_header.h_offset *= 1000;
			trans_header.h_offset /= HZ;
			sr_fd->srf_touch_time= get_time();
			break;
		case STD_INFO:
#ifdef STATISTICS
			sr_stat_issued.stat_STD_INFO++;
			glob_stat_failed= &sr_stat_failed.
				stat_STD_INFO;
#endif
			trans_header.h_status= STD_OK;
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_STD_INFO++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_STD_INFO;
#endif
			repl_buf= bprintf(reply, reply+sizeof(reply),
				"T%c%c%c%c%c--- %d",
				(rights & IP_RIGHTS_OPEN) ? 'o' : '-', 
				(rights & IP_RIGHTS_RWIO) ? 'i' : '-', 
				(rights & IP_RIGHTS_DESTROY) ? 'd' : '-', 
				(rights & IP_RIGHTS_LINGER) ? 'l' : '-', 
				(rights & IP_RIGHTS_SUPER) ? 's' : '-', 
				prv_number(&trans_header.h_priv));
			repl_buf[0]= '\0';
			repl_buf= reply;
			repl_size= strlen(repl_buf)+1;
			break;
		case STD_STATUS:
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
			do_status();
			trans_header.h_status= STD_OK;
			repl_buf= reply;
			repl_size= strlen(repl_buf)+1;
			break;
		case STD_RESTRICT:
#ifdef STATISTICS
			sr_stat_issued.stat_STD_RESTRICT++;
			glob_stat_failed= &sr_stat_failed.
				stat_STD_RESTRICT;
#endif
			trans_header.h_status= STD_OK;
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_STD_RESTRICT++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_STD_RESTRICT;
#endif
			result= prv_encode(&trans_header.h_priv,
					prv_number(&trans_header.h_priv),
					rights & trans_header.h_offset, 
					&sr_fd->srf_random);
			if (result != STD_OK)
				trans_header.h_status= STD_CAPBAD;
			break;
		case STD_DESTROY:
#ifdef STATISTICS
			sr_stat_issued.stat_STD_DESTROY++;
			glob_stat_failed= &sr_stat_failed.
				stat_STD_DESTROY;
#endif
			trans_header.h_status= STD_OK;
			result= sr_check_cap(&trans_header.h_priv, &rights,
				&sr_fd);
			if (result<0)
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
#ifdef STATISTICS
			sr_fd->srf_stat_issued.stat_STD_DESTROY++;
			loc_stat_failed= &sr_fd->srf_stat_failed.
				stat_STD_DESTROY;
#endif
			if (!(rights & IP_RIGHTS_DESTROY))
			{
				trans_header.h_status= STD_CAPBAD;
				break;
			}
			trans_header.h_status= sr_destroy (sr_fd);
			break;
		default:
#ifdef STATISTICS
			sr_stat_issued.stat_bad_command++;
			glob_stat_failed= &sr_stat_failed.
				stat_bad_command;
#endif
			printf("got an unknown request: %d\n",
				trans_header.h_command);
			trans_header.h_status= STD_COMBAD;
			break;
		}
		if (sr_thread->srt_req)
		{
			bf_afree(sr_thread->srt_req);
			sr_thread->srt_req= 0;
		}
		repl_acc= sr_thread->srt_repl;
		if (repl_acc)
		{
			if (repl_acc->acc_next)
			{
				repl_acc= bf_pack(repl_acc);
				sr_thread->srt_repl= repl_acc;
			}
			repl_buf= ptr2acc_data(repl_acc);
			assert (!repl_acc->acc_next);
			repl_size= bf_bufsize(repl_acc);
		}
		trans_header.h_size= repl_size;
#ifdef STATISTICS
		if (ERR_STATUS(trans_header.h_status))
		{
			if (glob_stat_failed)
				(*glob_stat_failed)++;
			if (loc_stat_failed)
				(*loc_stat_failed)++;
		}
#endif
		mu_unlock(&mu_generic);	/* XXX */
		rpc_putrep(&trans_header, repl_buf, repl_size);
		mu_lock(&mu_generic);	/* XXX */
		sr_thread->srt_repl= 0;
		if (repl_acc)
		{
			bf_afree(repl_acc);
			repl_acc= 0;
		}
#ifdef BUF_CONSISTENCY_CHECK
		{
			static int sometimes;

			if ((sometimes++ % 100) == 0)
			    assert(bf_consistency_check());
		}
#endif
		if (sema_level(&sr_thread->srt_block))
		{
			sema_down(&sr_thread->srt_block);
		}
		assert (!sema_level(&sr_thread->srt_block));
	}
}

static int sr_check_cap(priv, rights_ptr, sr_fd_ptr)
private *priv;
rights_bits *rights_ptr;
sr_fd_t **sr_fd_ptr;
{
	objnum object;
	sr_fd_t *sr_fd;

	object= prv_number(priv);
	if (object<0 || object >= SR_FD_NR)
	{
		DBLOCK(1, printf("strange object number\n"));
		return EINVAL;
	}
	sr_fd= &sr_fd_table[object];
	*sr_fd_ptr= sr_fd;
	if (!(sr_fd->srf_flags & SFF_INUSE))
	{
		return EINVAL;
	}
	if (prv_decode(priv, rights_ptr, &sr_fd->srf_random) != 0)
	{
		DBLOCK(1, printf("random not inuse\n"));
		return EINVAL;
	}
	return NW_OK;
}

static command sr_open(sr_fd, trans_header, linger)
sr_fd_t *sr_fd;
am_header_t *trans_header;
int linger;
{
	int i, result, fd;
	sr_fd_t *new_sr_fd;
	rights_bits iowr_rights;

	assert (sr_fd->srf_flags & SFF_MINOR);

	for (i=0, new_sr_fd= sr_fd_table; i<SR_FD_NR &&
		((new_sr_fd->srf_flags & SFF_INUSE) ||
		new_sr_fd->srf_ioctl_thread || new_sr_fd->srf_read_thread ||
		new_sr_fd->srf_write_thread); i++, new_sr_fd++);

	if (i>=SR_FD_NR)
	{
		DBLOCK(1, printf("replying STD_NOSPACE [1]\n"));
		return STD_NOSPACE;
	}

	sr_fd_table[i]= *sr_fd;
	sr_fd= &sr_fd_table[i];
	sr_fd->srf_flags= SFF_INUSE;
	uniqport(&sr_fd->srf_random);
	iowr_rights= IP_RIGHTS_RWIO|IP_RIGHTS_DESTROY;
	trans_header->h_port= tcpip_public_port;
	result= prv_encode(&trans_header->h_priv, (objnum)i, iowr_rights,
		&sr_fd->srf_random);
	if (result)
		ip_panic(( "unable to initialize capability" ));

	fd= (*sr_fd->srf_open)(sr_fd->srf_port, i, sr_get_userdata,
		sr_put_userdata, (put_pkt_t)0);
	if (fd<0)
	{
		sr_fd->srf_flags= SFF_FREE;
		DBLOCK(1, printf("replying STD_NOSPACE [2]\n", fd));
		return STD_NOSPACE;
	}
	sr_fd->srf_fd= fd;
	sr_fd->srf_ioctl_thread= 0;
	sr_fd->srf_write_thread= 0;
	sr_fd->srf_read_thread= 0;
	sr_fd->srf_ioctl_last_thread= 0;
	sr_fd->srf_write_last_thread= 0;
	sr_fd->srf_read_last_thread= 0;
	sr_fd->srf_creat_time= get_time();
	sr_fd->srf_touch_time= get_time();

	if (!linger)
	{
		sr_garbage_dog(i, &sr_fd->srf_garbage_dog);
	}
#ifdef STATISTICS
	clear_stats(&sr_fd->srf_stat_issued);
	clear_stats(&sr_fd->srf_stat_failed);
#endif

	return STD_OK;
}

static command sr_read_write_ioctl(sr_thread, sr_fd, trans_header)
sr_thread_t *sr_thread;
sr_fd_t *sr_fd;
am_header_t *trans_header;
{
	unsigned short size;
	unsigned long request;
	size_t reqsize;
	int result;
	command rwio;
	sr_thread_t **thread_ptr, **last_thread_ptr;
	int in_progres_flag, suspend_flag;

	rwio= trans_header->h_command;
	switch (rwio)
	{
	case TCPIP_IOCTL:
		thread_ptr= &sr_fd->srf_ioctl_thread;
		last_thread_ptr= &sr_fd->srf_ioctl_last_thread;
		in_progres_flag= SFF_IOCTL_IP;
		suspend_flag= SFF_IOCTL_SP;
		sr_thread->srt_cancel_op= SR_CANCEL_IOCTL;
		break;
	case TCPIP_READ:
		thread_ptr= &sr_fd->srf_read_thread;
		last_thread_ptr= &sr_fd->srf_read_last_thread;
		in_progres_flag= SFF_READ_IP;
		suspend_flag= SFF_READ_SP;
		sr_thread->srt_cancel_op= SR_CANCEL_READ;
		break;
	case TCPIP_WRITE:
		thread_ptr= &sr_fd->srf_write_thread;
		last_thread_ptr= &sr_fd->srf_write_last_thread;
		in_progres_flag= SFF_WRITE_IP;
		suspend_flag= SFF_WRITE_SP;
		sr_thread->srt_cancel_op= SR_CANCEL_WRITE;
		break;
	default:
		ip_panic(( "not reached" ));
	}
	sr_thread->srt_sr_fd= sr_fd;
	if (*thread_ptr)
	{
		sr_enqueue(sr_thread, thread_ptr, last_thread_ptr);
		mu_unlock(&mu_generic);
		sema_down(&sr_thread->srt_block);
		mu_lock(&mu_generic);
		if (sr_thread->srt_intr)
		{
			sr_dequeue(sr_thread, thread_ptr, last_thread_ptr);
			return STD_INTR;
		}
		sr_thread->srt_released= 0;
	}
	else
		sr_enqueue (sr_thread, thread_ptr, last_thread_ptr);
	assert (*thread_ptr == sr_thread);

	sr_fd->srf_flags |= in_progres_flag;

	switch (rwio)
	{
	case TCPIP_IOCTL:
		request= trans_header->h_offset;
		size= (request >> 16) & IOCPARM_MASK;
		if (size>MAX_IOCTL_S)
		{
			DBLOCK(1, printf("ioctl: bad request\n"));
			sr_fd->srf_flags &= ~in_progres_flag;
			sr_dequeue(sr_thread, thread_ptr, last_thread_ptr);
			return STD_ARGBAD;
		}

		result=(*sr_fd->srf_ioctl)(sr_fd->srf_fd, request);
		break;
	case TCPIP_READ:
		size= trans_header->h_size;
		result=(*sr_fd->srf_read)(sr_fd->srf_fd, size);
		break;
	case TCPIP_WRITE:
		size= trans_header->h_size;
		reqsize= sr_thread->srt_req ?
			bf_bufsize(sr_thread->srt_req): 0;
		if (size > reqsize)
		{
			sr_fd->srf_flags &= ~in_progres_flag;
			sr_dequeue(sr_thread, thread_ptr, last_thread_ptr);
			return STD_ARGBAD;
		}
		result=(*sr_fd->srf_write)(sr_fd->srf_fd, size);
		break;
	default:
		ip_panic (( "not reached" ));
	}

	assert (result == NW_SUSPEND || result == NW_OK);
	if (result == NW_SUSPEND)
	{
		sr_fd->srf_flags |= suspend_flag;
		mu_unlock(&mu_generic);
		sema_down(&sr_thread->srt_block);
		mu_lock(&mu_generic);
		if (!sr_thread->srt_released)	/* We got a signal */
		{
			assert (sr_thread->srt_intr);
			sr_cancel(sr_thread);
		}
		sr_fd->srf_flags &= ~suspend_flag;
		result= NW_OK;
	}
	else
	{	/* Should succeed imidiately */
		assert (sema_level(&sr_thread->srt_block));
		sema_down(&sr_thread->srt_block);
	}

	assert (sr_thread->srt_released);
	sr_fd->srf_flags &= ~in_progres_flag;

	assert (!(sr_fd->srf_flags & (in_progres_flag|suspend_flag)));
	sr_dequeue(sr_thread, thread_ptr, last_thread_ptr);

	if (result<0)
		sr_thread->srt_status= result;
	if ((short)sr_thread->srt_status == EINTR)
		return STD_INTR;
	return sr_thread->srt_status;
}

static acc_t *sr_get_userdata (fd, offset, count, for_ioctl)
int fd;
size_t offset;
size_t count;
int for_ioctl;
{
	sr_fd_t *sr_fd;
	sr_thread_t *sr_thread;
	sr_fd= &sr_fd_table[fd];

	assert (sr_fd->srf_flags & (for_ioctl ? SFF_IOCTL_IP : SFF_WRITE_IP));
	if (for_ioctl)
		sr_thread= sr_fd->srf_ioctl_thread;
	else
		sr_thread= sr_fd->srf_write_thread;
	assert (sr_thread);

	if (!count)
	{
		sr_thread->srt_status= offset;
		assert (!sr_thread->srt_released);
		sr_thread->srt_released= 1;
		sema_up(&sr_thread->srt_block);
		return 0;
	}	

	return bf_cut(sr_thread->srt_req, offset, count);
}

static int sr_put_userdata (fd, offset, data, for_ioctl)
int fd;
size_t offset;
acc_t *data;
int for_ioctl;
{
	sr_fd_t *sr_fd;
	sr_thread_t *sr_thread;
	sr_fd= &sr_fd_table[fd];

	assert (sr_fd->srf_flags & (for_ioctl ? SFF_IOCTL_IP : SFF_READ_IP));

	if (for_ioctl)
		sr_thread= sr_fd->srf_ioctl_thread;
	else
		sr_thread= sr_fd->srf_read_thread;
	assert (sr_thread);
	if (!data)
	{
		sr_thread->srt_status= offset;
		assert (!sr_thread->srt_released);
		sr_thread->srt_released= 1;
		sema_up(&sr_thread->srt_block);
		return 0;
	}	

	if (sr_thread->srt_repl != 0) {
		assert (bf_bufsize(sr_thread->srt_repl) == offset);
	}
	sr_thread->srt_repl= bf_append(sr_thread->srt_repl, data);
	assert (sr_thread->srt_repl != 0);
	return 0;
}

#if AM_KERNEL
static void sr_trans_catcher(thread_no, sig)
int thread_no;
signum sig;
#else
static void sr_trans_catcher(sig, us, extra )
signum sig;
thread_ustate *us;
char *extra;
#endif
{
	sr_thread_t *sr_thread;
#if !AM_KERNEL
	int thread_no;
#endif
	char line[100];
	int mode;

#if AM_KERNEL
	sr_thread= &sr_thread_table[thread_no];
#else
	sr_thread= (sr_thread_t *)extra;
	thread_no= sr_thread-sr_thread_table;
#endif

	assert (thread_no>=0 && thread_no< SR_THREAD_NR);
	assert (!sr_thread->srt_intr);

	sr_thread->srt_intr= 1;
	if (sr_thread->srt_released)
		return;

	sema_up(&sr_thread->srt_block);
}

static void sr_enqueue(sr_thread, thread_head, thread_tail)
sr_thread_t *sr_thread;
sr_thread_t **thread_head;
sr_thread_t **thread_tail;
{
	sr_thread->srt_next_thread= 0;
	if (!*thread_head)
		*thread_head= sr_thread;
	else
		(*thread_tail)->srt_next_thread= sr_thread;
	(*thread_tail)= sr_thread;
}

static void sr_dequeue(sr_thread, thread_head, thread_tail)
sr_thread_t *sr_thread;
sr_thread_t **thread_head;
sr_thread_t **thread_tail;
{
	sr_thread_t *prev_thread, *next_thread;

	assert (*thread_head);
	if (*thread_head == sr_thread)
	{
		next_thread= sr_thread->srt_next_thread;

		*thread_head= next_thread;
		if (next_thread)
		{
			assert (!next_thread->srt_released);
			next_thread->srt_released= 1;
			sema_up(&next_thread->srt_block);
		}
	}
	else
	{
		for (prev_thread= *thread_head;
			prev_thread->srt_next_thread  !=  sr_thread;
			prev_thread= prev_thread->srt_next_thread);
		prev_thread->srt_next_thread= sr_thread->srt_next_thread;
		if (*thread_tail == sr_thread)
			*thread_tail= prev_thread;
	}
}

static void sr_cancel(sr_thread)
sr_thread_t *sr_thread;
{
	int result;
	sr_fd_t *sr_fd;

	sr_fd= sr_thread->srt_sr_fd;
	result= (*sr_fd->srf_cancel)(sr_fd->srf_fd, sr_thread->srt_cancel_op);

	assert (result >= 0);
}

static errstat sr_destroy(sr_fd)
sr_fd_t *sr_fd;
{
	int flags;

	flags= sr_fd->srf_flags;
	assert (flags & SFF_INUSE);

	if (flags & SFF_MINOR)
	{
		sr_fd->srf_flags &= ~(SFF_INUSE|SFF_MINOR);
		assert (sr_fd->srf_flags == SFF_FREE);
		return STD_OK;
	}
	uniqport (&sr_fd->srf_random);	/* invalidate capabilities */
	if (flags & SFF_IOCTL_IP)
	{
		assert (sr_fd->srf_ioctl_thread);
		sr_cancel(sr_fd->srf_ioctl_thread);
	}

	if (flags & SFF_READ_IP)
	{
		assert (sr_fd->srf_read_thread);
		sr_cancel(sr_fd->srf_read_thread);
	}

	if (flags & SFF_WRITE_IP)
	{
		assert (sr_fd->srf_write_thread);
		sr_cancel(sr_fd->srf_write_thread);
	}

	(*sr_fd->srf_close)(sr_fd->srf_fd);

	sr_fd->srf_open= 0;
	sr_fd->srf_read= sr_dummy_read;
	sr_fd->srf_write= sr_dummy_write;
	sr_fd->srf_ioctl= sr_dummy_ioctl;
	sr_fd->srf_fd= sr_fd-sr_fd_table;
	sr_fd->srf_cancel= 0;
	sr_fd->srf_flags &= ~SFF_INUSE;
	clck_untimer(&sr_fd->srf_garbage_dog);

	return STD_OK;
}

static int sr_dummy_ioctl(fd, req)
int fd;
unsigned long req;
{
	int result;

	result= sr_put_userdata (fd, (size_t)STD_CAPBAD, NULL, 1);
	assert(result == NW_OK);

	return NW_OK;
}

static int sr_dummy_write(fd, size)
int fd;
size_t size;
{
	acc_t *acc;

	acc= sr_get_userdata (fd, (size_t)STD_CAPBAD, (size_t)0, 0);
	assert(!acc);

	return NW_OK;
}

static int sr_dummy_read(fd, size)
int fd;
size_t size;
{
	int result;

	result= sr_put_userdata (fd, (size_t)STD_CAPBAD, NULL, 0);
	assert(result == NW_OK);
	return NW_OK;
}

static void sr_garbage_dog(sr_fd_nr, timer)
int sr_fd_nr;
struct timer *timer;
{
	sr_fd_t *sr_fd;
	time_t destroy_time, destroy_timeout, curr_time;
	errstat error;

	assert (sr_fd_nr>=0 && sr_fd_nr<SR_FD_NR);

	sr_fd= &sr_fd_table[sr_fd_nr];

	assert (timer == &sr_fd->srf_garbage_dog);
	assert (sr_fd->srf_flags & SFF_INUSE);
	assert (!(sr_fd->srf_flags & SFF_MINOR));

	curr_time= get_time();
	if (sr_fd->srf_flags & SFF_BUSY)
		sr_fd->srf_touch_time= curr_time;

	destroy_timeout= (sr_fd->srf_touch_time - sr_fd->srf_creat_time) >> 2;
	if (destroy_timeout < SR_MIN_DESTR_TO)
		destroy_timeout= SR_MIN_DESTR_TO;
	else if (destroy_timeout > SR_MAX_DESTR_TO)
		destroy_timeout= SR_MAX_DESTR_TO;

	destroy_time= sr_fd->srf_touch_time + destroy_timeout;
	if (destroy_time <= curr_time)
	{
		error= sr_destroy(sr_fd);
		assert (error == STD_OK);
		return;
	}
	clck_timer (&sr_fd->srf_garbage_dog, destroy_time,
		sr_garbage_dog, sr_fd_nr);
}

PUBLIC void sr_enable_linger_right()
{
	linger_right= TRUE;
}

static void do_status()
{
	sr_fd_t *sr_fd;
	char *repl_ptr;
	int i, j;
	time_t curr_tim;

	repl_ptr= reply;
	curr_tim= get_time();
#ifdef STATISTICS
		repl_ptr= bprintf(repl_ptr, reply+sizeof(reply),
			"global statistics:\n");
		repl_ptr= sr_bprintf_stats(repl_ptr, reply+sizeof(reply),
			&sr_stat_issued, &sr_stat_failed);
#endif
	for (i= 0, sr_fd= sr_fd_table; i<SR_FD_NR; i++, sr_fd++)
	{
		if (sr_fd->srf_flags & SFF_MINOR)
		{
			repl_ptr= bprintf(repl_ptr, reply+sizeof(reply),
				"object %d: device '%s':\n", i,
				sr_fd->srf_cap_name);
#ifdef STATISTICS
			repl_ptr= sr_bprintf_stats(repl_ptr, 
				reply+sizeof(reply), &sr_fd->srf_stat_issued,
				&sr_fd->srf_stat_failed);
#endif
		}
		else if (sr_fd->srf_flags & SFF_INUSE)
		{
			for (j= 0; j<SR_FD_NR; j++)
			{
				if (!(sr_fd_table[j].srf_flags & SFF_MINOR))
					continue;
				if (sr_fd->srf_write ==
				    sr_fd_table[j].srf_write)
					break;
			}
			assert(j != SR_FD_NR);
			repl_ptr=bprintf(repl_ptr, reply+sizeof(reply),
				"object %d: chan. of dev. '%s'(%d): created ",
				i, sr_fd_table[j].srf_cap_name, j);
			repl_ptr=bprint_interval(repl_ptr, reply+sizeof(reply),
				sr_fd->srf_creat_time-curr_tim);
			repl_ptr=bprintf(repl_ptr, reply+sizeof(reply),
				", last touch ");
			repl_ptr=bprint_interval(repl_ptr, reply+sizeof(reply),
				sr_fd->srf_touch_time-curr_tim);
			repl_ptr=bprintf(repl_ptr, reply+sizeof(reply),
				"\n");
#ifdef STATISTICS
			repl_ptr= sr_bprintf_stats(repl_ptr, 
				reply+sizeof(reply), &sr_fd->srf_stat_issued,
				&sr_fd->srf_stat_failed);
#endif
		}
	}
	if (repl_ptr && repl_ptr < &reply[sizeof(reply)])
		*repl_ptr= '\0';
	else
		reply[sizeof(reply)-1]= '\0';
}

#if STATISTICS
static char *sr_bprintf_stats(buf, buf_end, stats_issued, stats_failed)
char *buf;
char *buf_end;
struct sr_stat *stats_issued;
struct sr_stat *stats_failed;
{
	int i;

	i= 0;

	if (stats_issued->stat_TCPIP_OPEN)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_OPEN: %ld (%ld) ", 
			stats_issued->stat_TCPIP_OPEN, 
			stats_failed->stat_TCPIP_OPEN);
		i++;
	}
	if (stats_issued->stat_TCPIP_MKCAP)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_MKCAP: %ld (%ld)", 
			stats_issued->stat_TCPIP_MKCAP, 
			stats_failed->stat_TCPIP_MKCAP);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_ETH_SETOPT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_ETH_SETOPT: %ld (%ld)", 
			stats_issued->stat_TCPIP_ETH_SETOPT, 
			stats_failed->stat_TCPIP_ETH_SETOPT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_ETH_GETOPT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_ETH_GETOPT: %ld (%ld)", 
			stats_issued->stat_TCPIP_ETH_GETOPT, 
			stats_failed->stat_TCPIP_ETH_GETOPT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_ETH_GETSTAT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_ETH_GETSTAT: %ld (%ld)", 
			stats_issued->stat_TCPIP_ETH_GETSTAT, 
			stats_failed->stat_TCPIP_ETH_GETSTAT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_IP_SETCONF)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_IP_SETCONF: %ld (%ld)", 
			stats_issued->stat_TCPIP_IP_SETCONF, 
			stats_failed->stat_TCPIP_IP_SETCONF);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_IP_GETCONF)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_IP_GETCONF: %ld (%ld)", 
			stats_issued->stat_TCPIP_IP_GETCONF, 
			stats_failed->stat_TCPIP_IP_GETCONF);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_IP_SETOPT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_IP_SETOPT: %ld (%ld)", 
			stats_issued->stat_TCPIP_IP_SETOPT, 
			stats_failed->stat_TCPIP_IP_SETOPT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_IP_GETOPT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_IP_GETOPT: %ld (%ld)", 
			stats_issued->stat_TCPIP_IP_GETOPT, 
			stats_failed->stat_TCPIP_IP_GETOPT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_IP_GETROUTE)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_IP_GETROUTE: %ld (%ld)", 
			stats_issued->stat_TCPIP_IP_GETROUTE, 
			stats_failed->stat_TCPIP_IP_GETROUTE);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_IP_SETROUTE)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_IP_SETROUTE: %ld (%ld)", 
			stats_issued->stat_TCPIP_IP_SETROUTE, 
			stats_failed->stat_TCPIP_IP_SETROUTE);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_TCP_SETCONF)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_TCP_SETCONF: %ld (%ld)", 
			stats_issued->stat_TCPIP_TCP_SETCONF, 
			stats_failed->stat_TCPIP_TCP_SETCONF);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_TCP_GETCONF)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_TCP_GETCONF: %ld (%ld)", 
			stats_issued->stat_TCPIP_TCP_GETCONF, 
			stats_failed->stat_TCPIP_TCP_GETCONF);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_TCP_SHUTDOWN)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_TCP_SHUTDOWN: %ld (%ld)", 
			stats_issued->stat_TCPIP_TCP_SHUTDOWN, 
			stats_failed->stat_TCPIP_TCP_SHUTDOWN);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_TCP_LISTEN)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_TCP_LISTEN: %ld (%ld)", 
			stats_issued->stat_TCPIP_TCP_LISTEN, 
			stats_failed->stat_TCPIP_TCP_LISTEN);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_TCP_CONNECT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_TCP_CONNECT: %ld (%ld)", 
			stats_issued->stat_TCPIP_TCP_CONNECT, 
			stats_failed->stat_TCPIP_TCP_CONNECT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_TCP_SETOPT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_TCP_SETOPT: %ld (%ld)", 
			stats_issued->stat_TCPIP_TCP_SETOPT, 
			stats_failed->stat_TCPIP_TCP_SETOPT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_TCP_GETOPT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_TCP_GETOPT: %ld (%ld)", 
			stats_issued->stat_TCPIP_TCP_GETOPT, 
			stats_failed->stat_TCPIP_TCP_GETOPT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_UDP_SETOPT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_UDP_SETOPT: %ld (%ld)", 
			stats_issued->stat_TCPIP_UDP_SETOPT, 
			stats_failed->stat_TCPIP_UDP_SETOPT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_UDP_GETOPT)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_UDP_GETOPT: %ld (%ld)", 
			stats_issued->stat_TCPIP_UDP_GETOPT, 
			stats_failed->stat_TCPIP_UDP_GETOPT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_IOCTL)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_IOCTL: %ld (%ld)", 
			stats_issued->stat_TCPIP_IOCTL, 
			stats_failed->stat_TCPIP_IOCTL);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_READ)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_READ: %ld (%ld)", 
			stats_issued->stat_TCPIP_READ, 
			stats_failed->stat_TCPIP_READ);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_WRITE)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_WRITE: %ld (%ld)", 
			stats_issued->stat_TCPIP_WRITE, 
			stats_failed->stat_TCPIP_WRITE);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_TCPIP_KEEPALIVE)
	{
		buf= bprintf(buf, buf_end, "\tTCPIP_KEEPALIVE: %ld (%ld)", 
			stats_issued->stat_TCPIP_KEEPALIVE, 
			stats_failed->stat_TCPIP_KEEPALIVE);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_STD_INFO)
	{
		buf= bprintf(buf, buf_end, "\tSTD_INFO: %ld (%ld)", 
			stats_issued->stat_STD_INFO, 
			stats_failed->stat_STD_INFO);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_STD_RESTRICT)
	{
		buf= bprintf(buf, buf_end, "\tSTD_RESTRICT: %ld (%ld)", 
			stats_issued->stat_STD_RESTRICT, 
			stats_failed->stat_STD_RESTRICT);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_STD_DESTROY)
	{
		buf= bprintf(buf, buf_end, "\tSTD_DESTROY: %ld (%ld)", 
			stats_issued->stat_STD_DESTROY, 
			stats_failed->stat_STD_DESTROY);
		i++;
	}
	if (i == 2) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	if (stats_issued->stat_bad_command)
	{
		buf= bprintf(buf, buf_end, "\tbad_command: %ld (%ld)", 
			stats_issued->stat_bad_command, 
			stats_failed->stat_bad_command);
		i++;
	}
	if (i) { i= 0; buf= bprintf(buf, buf_end, "\n"); }
	return buf;
}

static void clear_stats(stats)
struct sr_stat *stats;
{
	stats->stat_TCPIP_OPEN= 0;
	stats->stat_TCPIP_MKCAP= 0;
	stats->stat_TCPIP_ETH_SETOPT= 0;
	stats->stat_TCPIP_ETH_GETOPT= 0;
	stats->stat_TCPIP_ETH_GETSTAT= 0;
	stats->stat_TCPIP_IP_SETCONF= 0;
	stats->stat_TCPIP_IP_GETCONF= 0;
	stats->stat_TCPIP_IP_SETOPT= 0;
	stats->stat_TCPIP_IP_GETOPT= 0;
	stats->stat_TCPIP_IP_GETROUTE= 0;
	stats->stat_TCPIP_IP_SETROUTE= 0;
	stats->stat_TCPIP_TCP_SETCONF= 0;
	stats->stat_TCPIP_TCP_GETCONF= 0;
	stats->stat_TCPIP_TCP_SHUTDOWN= 0;
	stats->stat_TCPIP_TCP_LISTEN= 0;
	stats->stat_TCPIP_TCP_CONNECT= 0;
	stats->stat_TCPIP_TCP_SETOPT= 0;
	stats->stat_TCPIP_TCP_GETOPT= 0;
	stats->stat_TCPIP_UDP_SETOPT= 0;
	stats->stat_TCPIP_UDP_GETOPT= 0;
	stats->stat_TCPIP_IOCTL= 0;
	stats->stat_TCPIP_READ= 0;
	stats->stat_TCPIP_WRITE= 0;
	stats->stat_TCPIP_KEEPALIVE= 0;
	stats->stat_STD_INFO= 0;
	stats->stat_STD_RESTRICT= 0;
	stats->stat_STD_DESTROY= 0;
	stats->stat_bad_command= 0;
}
#endif /* STATISTICS */

static char *bprint_interval(buf, buf_end, iv)
char *buf;
char *buf_end;
long iv;
{
	long h, m, s, mil;

	if (iv < 0)
	{
		buf= bprintf(buf, buf_end, "-");
		iv= -iv;
	}
	s= iv/HZ;
	m= s/60;
	h= m/60;
	buf= bprintf(buf, buf_end, "%d:%02d %02d.%03d", h, m % 60, s % 60, 
			(iv % HZ) * 1000 / HZ);
	return buf;
}

static errstat do_mkcap(obj, hdr)
int obj;
am_header_t *hdr;
{
	sr_fd_t *sr_fd;
	rights_bits iowr_rights;
	int result;

	if (obj<0 || obj >= SR_FD_NR)
		return EINVAL;
	sr_fd= &sr_fd_table[obj];
	if (!(sr_fd->srf_flags & SFF_INUSE))
		return EINVAL;
	
	if (sr_fd->srf_flags & SFF_MINOR)
		iowr_rights= IP_RIGHTS_OPEN |
			(linger_right ? IP_RIGHTS_LINGER : 0) | IP_RIGHTS_SUPER;
	else
		iowr_rights= IP_RIGHTS_RWIO|IP_RIGHTS_DESTROY;
	hdr->h_port= tcpip_public_port;
	result= prv_encode(&hdr->h_priv, (objnum)obj, iowr_rights,
		&sr_fd->srf_random);
	if (result == -1)
		ip_panic(( "unable to initialize capability" ));

	return STD_OK;
}

#ifdef BUF_CONSISTENCY_CHECK
static void sr_buffree(priority)
int priority;
{
	return;		/* nothing to free */
}

static void sr_bufcheck()
{
	int i;

	for (i= 0; i<last_thread; i++)
	{
		bf_check_acc(sr_thread_table[i].srt_req);
		bf_check_acc(sr_thread_table[i].srt_repl);
	}
}
#endif
