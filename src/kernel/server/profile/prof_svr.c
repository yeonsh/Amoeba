/*	@(#)prof_svr.c	1.6	96/02/27 14:20:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
prof_svr.c

Created Nov 5, 1991 by Philip Homburg

This is the implementation of the profile server.
*/

#if RPC_PROFILE
#include <_ARGS.new.h>
#endif

#include <assert.h>
INIT_ASSERT
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <_ARGS.h>
#include <amoeba.h>
#include <byteorder.h>
#include <cmdreg.h>
#include <exception.h>
#include <stdcom.h>
#include <stderr.h>
#include <module/prv.h>
#include <module/rnd.h>
#include <sys/proto.h>
#include <server/profile/profile.h>
#if RPC_PROFILE
#include <module/buffers.h>
#include <sys/profile.h>
#endif

#include <arch_proto.h>

#define PROF_STACK	0 /* default size */
#define DEF_PROF_NAME	"profile"
#define BUFFER_SIZE	128
#define PROF_BUF_SIZE 30000

void prof_init _ARGS(( void ));
static void prof_thread _ARGS(( void ));
static void sig_trans_catcher _ARGS(( int thread_no, signum sig ));
static void prof_done _ARGS(( int /* bufsize */ res ));
#if RPC_PROFILE
struct rpc_prof_info;

char *buf_put _ARGS(( char *curr, char *end, struct rpc_prof_info *d ));
#endif /* RPC_PROFILE */

static port priv_port, pub_port, priv_random;
static int threads;
static int prof_busy;
static int prof_ready;
static bufsize prof_res;

#if RPC_PROFILE
typedef struct rpc_prof_info
{
	int rpi_type;
	union
	{
		struct
		{
			int rpi_prc;
			int rpi_thr;
			port rpi_reqport;
			objnum rpi_reqobj;
			command rpi_reqcmd;
			bufsize rpi_reqsiz;
			bufsize rpi_replsiz;
			unsigned long rpi_reqtim;
			unsigned long rpi_repltim;
		} rpi_rpc;
	} rpi_u;
} rpc_prof_info_t;

rpc_prof_info_t *rpc_double_buffer, *rpc_double_end, *rpc_double_read, 
	*rpc_double_write;
int rpc_double_size;
long rpcs_missed;
int rpc_prof_running;
int rpc_prof_started;
char *prof_current, *prof_end, *prof_hi;

#define RPC_DOUBLE_NR	1024
#endif /* RPC_PROFILE */

void prof_init()
{
	threads= 0;
	prof_busy= 0;

	NewKernelThread(prof_thread, (vir_bytes) PROF_STACK);
	NewKernelThread(prof_thread, (vir_bytes) PROF_STACK);
#if RPC_PROFILE
	printf("rpc_double_size= %d\n", rpc_double_size);
#endif
}

static void prof_thread()
{
	capability cap;
	header hdr;
	f_size_t n;
	rights_bits rights;
	char *obuf_ptr;
	char *buffer;
	char *prof_buffer;
	int thread_nr;
	size_t prof_size;
	errstat error;

	buffer= (char *) malloc(BUFFER_SIZE);
	if (buffer == NULL)
		panic("prof_svr.c: unable to malloc");

	thread_nr= threads++;
	if (thread_nr == 0)
	{
		uniqport(&priv_port);

		priv2pub(&priv_port, &pub_port);
		cap.cap_port= pub_port;
		uniqport(&priv_random);
		if (prv_encode(&cap.cap_priv, (objnum) 0, (rights_bits) 
			PROFILE_RGT_ALL, &priv_random) < 0)
		{
			panic("prof_thread: prv_encode failed");
		}
		dirappend(DEF_PROF_NAME, &cap);
	}

	getsig(sig_trans_catcher, 0);

	prof_buffer= NULL;
	for (;;)
	{
		prof_ready= 0;
		prof_res= STD_INTR;
		hdr.h_port= priv_port;
		n= rpc_getreq(&hdr, buffer, (f_size_t) BUFFER_SIZE);

		compare((short)n, >=, 0);

		if (prv_number(&hdr.h_priv) != 0 ||
			prv_decode(&hdr.h_priv, &rights, 
			&priv_random) < 0)
		{
			hdr.h_status= STD_CAPBAD;
			n= 0;
		}
		else
		{
			obuf_ptr= NULL;
			switch(hdr.h_command)
			{
			case STD_INFO:
				bprintf(buffer, &buffer[BUFFER_SIZE],
					"profile server");
				obuf_ptr= buffer;
				hdr.h_size= n= strlen(obuf_ptr);
				hdr.h_status= STD_OK;
				break;
			case PROFILE_CMD:
				if (!(rights & PROFILE_RGT))
				{
					n= 0;
					hdr.h_status= STD_DENIED;
					break;
				}
				if (prof_busy)
				{
					n= 0;
					hdr.h_status= STD_NOTNOW;
					break;
				}
				prof_busy++;

				prof_size= hdr.h_size;
				if (prof_size > PROF_BUF_SIZE)
					prof_size= PROF_BUF_SIZE;

				prof_buffer= (char *) malloc(prof_size);
				if (prof_buffer == NULL)
					panic("prof_svr.c: unable to malloc");
				error= profile_start(prof_buffer, prof_size, 
					hdr.h_offset, prof_done);
				if (error != STD_OK)
				{
					n= 0;
					hdr.h_status= error;
					prof_busy--;
					break;
				}

				await((event) &prof_ready, (interval) 0);
				
				profile_stop();
				obuf_ptr = prof_buffer;
				if (ERR_STATUS(prof_res))
					n= 0;
				else
					n= prof_res;

				hdr.h_size = n;
				hdr.h_status = prof_res;
				prof_busy--;
				break;
#if RPC_PROFILE
			case PROFILE_RPC:
				if (!(rights & PROFILE_RGT))
				{
					n= 0;
					hdr.h_status= STD_DENIED;
					break;
				}
				if (prof_busy)
				{
					n= 0;
					hdr.h_status= STD_NOTNOW;
					break;
				}
				if (!rpc_prof_started)
				{
					n= 0;
					hdr.h_status= STD_NOTFOUND;
					break;
				}
				prof_busy++;

				prof_size= hdr.h_size;
				if (prof_size > PROF_BUF_SIZE)
					prof_size= PROF_BUF_SIZE;

				prof_buffer= malloc(prof_size);
				if (!prof_buffer)
					panic("prof_svr.c: unable to malloc");
				prof_current= prof_buffer;
				prof_end= prof_buffer+prof_size;
				prof_hi= prof_current;

				/* The header contains the number of rpcs that
				 * didn't fit in the buffer. */
				prof_current= buf_put_long(prof_current, 
					prof_end, rpcs_missed);
				if (prof_current)
				{
					rpcs_missed= 0;
					prof_hi= prof_current;
				}

				/* Statistics that are buffered are inserted 
				 * first. */
				while (rpc_double_size)
				{
					prof_current= buf_put(prof_current, 
						prof_end, rpc_double_read);
					if (prof_current == NULL)
						break;
					prof_hi= prof_current;
					rpc_double_size--;
					rpc_double_read++;
					if (rpc_double_read == rpc_double_end)
						rpc_double_read= rpc_double_buffer;
				}

				/* Setup getting info. */
				if (prof_current != NULL)
				{
					rpc_prof_running= 1;
					await((char *)&prof_ready, 0);
					rpc_prof_running= 0;
					if (prof_hi > prof_buffer)
						prof_res= prof_hi - prof_buffer;
				}
				else
					prof_res= prof_hi-prof_buffer;
				
				obuf_ptr = prof_buffer;
				if (ERR_STATUS(prof_res))
					n= 0;
				else
					n= prof_res;

				hdr.h_size = n;
				hdr.h_status = prof_res;
				prof_busy--;
				break;
			case PROFILE_RPC_START:
				if (!(rights & PROFILE_RGT))
				{
					n= 0;
					hdr.h_status= STD_DENIED;
					break;
				}
				if (rpc_prof_started)
				{
					n= 0;
					hdr.h_status= STD_EXISTS;
					break;
				}
				rpc_double_buffer= malloc(RPC_DOUBLE_NR*
								sizeof(rpc_prof_info_t));
				if (rpc_double_buffer == NULL)
					panic("prof_svr.c: unable to malloc");
				rpc_double_end= rpc_double_buffer + RPC_DOUBLE_NR;
				rpc_double_write= rpc_double_read= rpc_double_buffer;
				rpc_double_size= 0;
				rpc_prof_started= 1;

				n= 0;
				hdr.h_status = STD_OK;
				break;
			case PROFILE_RPC_STOP:
				if (!(rights & PROFILE_RGT))
				{
					n= 0;
					hdr.h_status= STD_DENIED;
					break;
				}
				if (!rpc_prof_started)
				{
					n= 0;
					hdr.h_status= STD_NOTFOUND;
					break;
				}
				prof_done(STD_INTR);
				rpc_prof_started= 0;
				free((_VOIDSTAR) rpc_double_buffer);

				hdr.h_size = 0;
				hdr.h_status = STD_OK;
				break;
#endif /* RPC_PROFILE */
			default:
				hdr.h_status= STD_COMBAD;
				n= 0;
				
			}
		}
		rpc_putrep(&hdr, obuf_ptr, n);
		if (prof_buffer)
		{
			free((_VOIDSTAR) prof_buffer);
			prof_buffer= NULL;
		}
	}
}

/*ARGSUSED*/
static void sig_trans_catcher(thread_no, sig)
int thread_no;
signum sig;
{
	assert(thread_no == 0);

	prof_done((int) STD_INTR);
}

static void prof_done(res)
int res;
{
	prof_ready= 1;
	prof_res= res;
	wakeup((char *)&prof_ready);
}

#if RPC_PROFILE
_DEFUN
(void profile_rpc, (prc, thr, reqport, reqobj, reqcmd, reqsiz, replsiz, reqtim, repltim),
	int prc _AND
	int thr _AND
	port *reqport _AND
	objnum reqobj _AND
	command reqcmd _AND
	bufsize reqsiz _AND
	bufsize replsiz _AND
	unsigned long reqtim _AND
	unsigned long repltim
)
{
	if (!rpc_prof_started)
		return;

	/* Look for a active request, store the and store the data */
	if (rpc_prof_running)
	{
		prof_current= buf_put_char(prof_current, prof_end, 1);	/* Type */
		prof_current= buf_put_short(prof_current, prof_end, prc); /* process */
		prof_current= buf_put_short(prof_current, prof_end, thr); /* thread */
		prof_current= buf_put_port(prof_current, prof_end, reqport); /* port */
		prof_current= buf_put_long(prof_current, prof_end, reqobj); /* obj */
		prof_current= buf_put_short(prof_current, prof_end, reqcmd); /* command */
		prof_current= buf_put_short(prof_current, prof_end, reqsiz); /* size1 */
		prof_current= buf_put_short(prof_current, prof_end, replsiz); /* size2 */
		prof_current= buf_put_long(prof_current, prof_end, reqtim); /* time1 */
		prof_current= buf_put_long(prof_current, prof_end, repltim); /* time2 */
		if (prof_current)
		{
			prof_hi= prof_current;
			return;
		}
		prof_done(STD_OK);
	}

	/* Try to store the data in the buffer */
	if (rpc_double_size < RPC_DOUBLE_NR)
	{
		rpc_double_write->rpi_type= 1;
		rpc_double_write->rpi_u.rpi_rpc.rpi_prc= prc;
		rpc_double_write->rpi_u.rpi_rpc.rpi_thr= thr;
		rpc_double_write->rpi_u.rpi_rpc.rpi_reqport= *reqport;
		rpc_double_write->rpi_u.rpi_rpc.rpi_reqobj= reqobj;
		rpc_double_write->rpi_u.rpi_rpc.rpi_reqcmd= reqcmd;
		rpc_double_write->rpi_u.rpi_rpc.rpi_reqsiz= reqsiz;
		rpc_double_write->rpi_u.rpi_rpc.rpi_replsiz= replsiz;
		rpc_double_write->rpi_u.rpi_rpc.rpi_reqtim= reqtim;
		rpc_double_write->rpi_u.rpi_rpc.rpi_repltim= repltim;
		rpc_double_write++;
		if (rpc_double_write == rpc_double_end)
			rpc_double_write= rpc_double_buffer;
		rpc_double_size++;
		return;
	}
	/* Increment the error count */
	rpcs_missed++;
}


_DEFUN
(char *buf_put, (curr, end, d),
	char *curr _AND
	char *end _AND
	struct rpc_prof_info *d
)
{
	curr= buf_put_char(curr, end, 1);	/* Type */
	curr= buf_put_short(curr, end, d->rpi_u.rpi_rpc.rpi_prc); /* process */
	curr= buf_put_short(curr, end, d->rpi_u.rpi_rpc.rpi_thr); /* thread */
	curr= buf_put_port(curr, end, &d->rpi_u.rpi_rpc.rpi_reqport); /* port */
	curr= buf_put_long(curr, end, d->rpi_u.rpi_rpc.rpi_reqobj); /* obj */
	curr= buf_put_short(curr, end, d->rpi_u.rpi_rpc.rpi_reqcmd); /* command */
	curr= buf_put_short(curr, end, d->rpi_u.rpi_rpc.rpi_reqsiz); /* size1 */
	curr= buf_put_short(curr, end, d->rpi_u.rpi_rpc.rpi_replsiz); /* size2 */
	curr= buf_put_long(curr, end, d->rpi_u.rpi_rpc.rpi_reqtim); /* time1 */
	curr= buf_put_long(curr, end, d->rpi_u.rpi_rpc.rpi_repltim); /* time2 */

	return curr;
}
	
#endif /* RPC_PROFILE */
