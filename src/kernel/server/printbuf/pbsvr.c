/*	@(#)pbsvr.c	1.8	96/02/27 14:20:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
pbsvr.c

Created Oct 3, 1991 by Philip Homburg

This is the implementation of the printbuf server.
*/

#include <assert.h>
INIT_ASSERT
#include <string.h>
#include <stdlib.h>

#include <_ARGS.h>
#include <amoeba.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <module/prv.h>
#include <module/rnd.h>

#include <server/systask/systask.h>

#include <sys/printbuf.h>
#include <sys/proto.h>

#define PB_STACK	8192
#define PB_THREAD_NR	2
#define DEF_PBNAME	"printbuf"
#define BUFFER_SIZE	128

void pb_init _ARGS(( void ));
static void pb_thread _ARGS(( void ));
static void sig_trans_catcher _ARGS(( int thread_no, signum sig ));

static port priv_port, pub_port, priv_random;
static int threads;
extern int pb_busy;

void pb_init()
{
	int i;

	threads= 0;
	pb_busy= 0;

	for (i= 0; i< PB_THREAD_NR; i++)
		NewKernelThread(pb_thread, PB_STACK);
}

static void pb_thread()
{
	capability cap;
	header hdr;
	bufsize n;
	rights_bits rights;
	char *obuf_ptr;
	bufsize ret_size;
	char *buffer;
	int thread_nr;

	buffer= (char *) malloc(BUFFER_SIZE);
	if (!buffer)
		panic("pbsvr.c: unable to malloc");

	thread_nr= threads++;
	if (thread_nr == 0)
	{
		uniqport(&priv_port);

		priv2pub(&priv_port, &pub_port);
		cap.cap_port= pub_port;
		uniqport(&priv_random);
		if (prv_encode(&cap.cap_priv, (objnum) 0, (rights_bits) 
			SYS_RGT_READ, &priv_random) < 0)
		{
			panic("pb_thread: prv_encode failed");
		}
		dirappend(DEF_PBNAME, &cap);
	}

	getsig(sig_trans_catcher, 0);

	for (;;)
	{
		hdr.h_port= priv_port;
		n= rpc_getreq(&hdr, buffer, BUFFER_SIZE);

		compare((short)n, >=, 0);

		obuf_ptr = NULL;
		ret_size = 0;
		if (prv_number(&hdr.h_priv) != 0 ||
		    prv_decode(&hdr.h_priv, &rights, &priv_random) < 0)
		{
			hdr.h_status= STD_CAPBAD;
		}
		else
		{
			switch(hdr.h_command)
			{
			case STD_INFO:
				strncpy(buffer, "printbuf server", BUFFER_SIZE);
				obuf_ptr= buffer;
				hdr.h_size = ret_size = strlen(obuf_ptr);
				hdr.h_status= STD_OK;
				break;
			case SYS_PRINTBUF:
				if (!(rights & SYS_RGT_READ))
				{
					hdr.h_status= STD_DENIED;
					break;
				}
				if (!pb_descr)
				{
					hdr.h_offset= 0;
					hdr.h_status= STD_OK;
					break;
				}
				if (pb_busy >= PB_THREAD_NR-1)
				{
					hdr.h_status= STD_NOTNOW;
					break;
				}
				pb_busy++;

				if (pb_descr->pb_first == pb_descr->pb_next)
					await((char *)pb_descr, 0);
				
				obuf_ptr = pb_descr->pb_first;
				if (pb_descr->pb_next < obuf_ptr)
					ret_size = pb_descr->pb_end - obuf_ptr;
				else
					ret_size = pb_descr->pb_next - obuf_ptr;
				hdr.h_offset = ret_size;
				hdr.h_status = STD_OK;
				pb_descr->pb_first += ret_size;
				if (pb_descr->pb_first >= pb_descr->pb_end)
					pb_descr->pb_first= pb_descr->pb_buf;
				pb_busy--;
				break;
			default:
				hdr.h_status= STD_COMBAD;
				break;
			}
		}
		(void) rpc_putrep(&hdr, obuf_ptr, ret_size);
	}
}

/*ARGSUSED*/
static void sig_trans_catcher(thread_no, sig)
int thread_no;
signum sig;
{
	assert(thread_no == 0);

	wakeup((char *)pb_descr);
}
