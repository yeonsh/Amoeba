/*	@(#)proc.c	1.7	96/02/27 14:20:44 */
/*
 * Copyright 1995 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** proc - process management server.
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "direct/direct.h"
#include "global.h"
#include "exception.h"
#include "machdep.h"
#include "kthread.h"
#include "seg.h"
#include "process/proc.h"
#include "module/buffers.h"
#include "procdefs.h"
#include "bullet/bullet.h"
#include "module/prv.h"
#include "stdlib.h"
#include "string.h"
#include "module/rnd.h"
#include "capset.h"
#include "soap/soap.h"
#include "kerneldir.h"
#include "sys/proto.h"
#include "monitor.h"
#include "assert.h"
INIT_ASSERT

extern errstat dirreplace _ARGS(( char * name, capability * cap ));

/*
** kips is defined in sys/kips.c.  It is 1000's of instructions per second
** (ie. MIPS * 1000).
*/
extern uint16	kips;

/* The port for the server for clients is in segsvr_put_port */
port			segsvr_put_port;
static capability	segcap;
/* The server only allows process creation with the exec_caps */
static private		exec_cap[2];
static port		exec_chk[2];
/* Determines which of the two exec_caps is current */
static int		exec_cur;
static int		exec_switching;

#ifdef STATISTICS
static struct
{
    int	st_execs;
    int	st_badexecs;
} stats;
#endif

extern long		loadav;

#ifndef NR_SEGSVR_THREADS
#define NR_SEGSVR_THREADS	4
#endif

#ifndef SEGSVR_STACKSIZE
#define SEGSVR_STACKSIZE	0	/* default size */
#endif

/*
** segsvr - the segment/process server.
**	    Handles calls for both processes and segments.
**	    It should be two servers but there is no time to fix it now.
*/

static void
segsvr _ARGS((void))
{
    char *		buffer;
    char *		bufferp;
    header		hdr;
    bufsize		n;
    bufsize		n2;
    capability		ownercap;
    capability		orig_seg; /* if creating segment, can make clone */
    process_d *		pd;
    rights_bits		rights;
    errstat		err;

    if ((buffer = (char *) malloc(PSVR_BUFSZ)) == 0)
	panic("segsvr: aalloc of getreq buffer failed");

    while (1)
    {
	hdr.h_port = segcap.cap_port;
#ifdef USE_AM6_RPC
	n = rpc_getreq(&hdr, (bufptr) buffer, (bufsize) PSVR_BUFSZ);
#else
	n = getreq(&hdr, (bufptr) buffer, (bufsize) PSVR_BUFSZ);
#endif
	assert(n <= PSVR_BUFSZ);
	if (ERR_STATUS(n))
	    panic("segsvr: getreq abort");
	n2 = 0;
	bufferp = NILBUF;
	switch(hdr.h_command)
	{
	/*
	** Calls that refer to any object, be it a segment or a process.
	*/
	case STD_INFO:
	    MON_EVENT("std_info");
	    hdr.h_status = STD_OK;
	    *buffer = '\0';
	    if (prv_number(&hdr.h_priv) == 0) /* it is the server cap */
	    {
		if (prv_decode(&hdr.h_priv, &rights,
					    &segcap.cap_priv.prv_random) < 0 &&
		    prv_decode(&hdr.h_priv, &rights, &exec_chk[0]) < 0 &&
		    prv_decode(&hdr.h_priv, &rights, &exec_chk[1]) < 0)
		{
			hdr.h_status = STD_CAPBAD;
		}
		else
		    (void) strcpy(buffer, "process/segment server");
	    }
	    else
		if (seg_getprv((rights_bits) 0, &hdr.h_priv) != 0)
		    (void) strcpy(buffer, "in-core segment");
		else
		{
		    struct process *cp;
		    if ((err = ppro_getprv((rights_bits) 0, &hdr.h_priv, &cp))
			== STD_OK)
		    {
			if (PRVNUM(prv_number(&hdr.h_priv)) == 0)
			    (void) strcpy(buffer, "kernel process");
			else
			    if (cp->pr_flags & CL_STOPPED)
				(void) strcpy(buffer, "Stp");
			    else if (cp->pr_flags & CL_DYING)
				(void) strcpy(buffer, "Die");
			    else
				(void) strcpy(buffer, "Run");
			if (*cp->pr_comment) {
			    (void) strcat(buffer, ": ");
			    (void) strcat(buffer, cp->pr_comment);
			}
		    }
		    else
		    {
			hdr.h_status = err;
			break;
		    }
		}
	    hdr.h_size = n2 = strlen(buffer);
	    bufferp = buffer;
	    break;

	case STD_RESTRICT:
	    if (n != 0)
		hdr.h_status = STD_ARGBAD;
	    else
	    {
		MON_EVENT("std_restrict");
		switch (PRVTYPE(prv_number(&hdr.h_priv)))
		{
		case 0:  /* the capability for the server itself */
		    if (prv_decode(&hdr.h_priv, &rights,
					    &segcap.cap_priv.prv_random) < 0)
			hdr.h_status = STD_CAPBAD;
		    else
		    {
			rights &= (rights_bits) hdr.h_offset;
			if (prv_encode(&hdr.h_priv, (objnum) 0, rights,
					    &segcap.cap_priv.prv_random) < 0)
			    hdr.h_status = STD_CAPBAD;
			else
			    hdr.h_status = STD_OK;
		    }
		    break;

		case OBJ_P:
		    hdr.h_status = ppro_restrict(&hdr.h_priv,
						(rights_bits) hdr.h_offset);
		    break;

		case OBJ_S:
		    hdr.h_status = seg_restrict(&hdr.h_priv,
						(rights_bits) hdr.h_offset);
		    break;
		
		default:
		    MON_EVENT("restrict: bad command");
		    hdr.h_status = STD_COMBAD;
		    break;
		}
	    }
	    break;

#ifdef STATISTICS
	case STD_STATUS:
	    if (prv_number(&hdr.h_priv) != 0 ||
		prv_decode(&hdr.h_priv, &rights,
					&segcap.cap_priv.prv_random) < 0 ||
		rights != PRV_ALL_RIGHTS)
	    {
		hdr.h_status = STD_DENIED;
	    }
	    else
	    {
		char * p;
		char * end;

		end = buffer + PSVR_BUFSZ;
		p = bprintf(buffer, end, "Process Server Status\n\n");
		p = bprintf(p, end, "# PS_EXEC commands: %d\n", stats.st_execs);
		p = bprintf(p, end, "# PS_EXEC badcmds : %d\n", stats.st_badexecs);
		bufferp = buffer;
		n2 = p - bufferp;
		hdr.h_status = STD_OK;
	    }
	    break;
#endif /* STATISTICS */
	/*
	** Standard operations on segments.
	*/
	case STD_DESTROY:
	    MON_EVENT("std_destroy");
	    hdr.h_status = usr_segdelete(&hdr.h_priv);
	    break;

	case PS_SEG_CREATE:
	    if (prv_number(&hdr.h_priv) != 0 ||
		(prv_decode(&hdr.h_priv, &rights, &exec_chk[exec_cur]) < 0 &&
		prv_decode(&hdr.h_priv, &rights, &exec_chk[exec_cur ^ 1]) < 0))
	    {
		hdr.h_status = STD_CAPBAD;
	    }
	    else
	    {
		MON_EVENT("create segment");
		if (n == 0) /* not copying an extant segment */
		    hdr.h_status = usr_segcreate(&hdr, (capability *) 0);
		else
		    if (buf_get_cap(buffer, buffer + n, &orig_seg) == 0)
			hdr.h_status = STD_ARGBAD;
		    else
			hdr.h_status = usr_segcreate(&hdr, &orig_seg);
	    }
	    break;

	case PS_SEG_WRITE:
	    if (hdr.h_size < n)
		hdr.h_status = STD_ARGBAD;
	    else
	    {
		MON_EVENT("write segment");
		if (n < hdr.h_size)
		    hdr.h_size = n;
		hdr.h_status = usr_segwrite(&hdr.h_priv, hdr.h_offset, buffer,
						(long) hdr.h_size, &hdr.h_size);
	    }
	    break;

	case BS_READ:
	    if (PRVTYPE(prv_number(&hdr.h_priv)) != OBJ_S)
	    {
		MON_EVENT("read segment: bad command");
		hdr.h_status = STD_COMBAD;
	    }
	    else
	    {
		MON_EVENT("read segment");
		bufferp = usr_segread(&hdr, &n2);
	    }
	    break;

	case BS_SIZE:
	    if (PRVTYPE(prv_number(&hdr.h_priv)) != OBJ_S)
	    {
		MON_EVENT("size segment: bad command");
		hdr.h_status = STD_COMBAD;
	    }
	    else
	    {
		MON_EVENT("size segment");
		hdr.h_status = usr_segsize(&hdr);
	    }
	    break;

	/*
	** The informational calls.
	*/
	case PS_GETDEF:
	    if (prv_number(&hdr.h_priv) != 0 ||
		    prv_decode(&hdr.h_priv, &rights,
					    &segcap.cap_priv.prv_random) < 0)
		hdr.h_status = STD_CAPBAD;
	    else
	    {
		MON_EVENT("getdef");
		hdr.h_extra = VIR_LOW;
		hdr.h_offset = VIR_HIGH;
		hdr.h_size = CLICKSHIFT;
		hdr.h_status = STD_OK;
	    }
	    break;

	case PS_GETLOAD:
	    if (prv_number(&hdr.h_priv) != 0 ||
		    prv_decode(&hdr.h_priv, &rights,
					    &segcap.cap_priv.prv_random) < 0)
		hdr.h_status = STD_CAPBAD;
	    else
	    {
		MON_EVENT("getload");
		hdr.h_offset = mem_free() << CLICKSHIFT;
		hdr.h_size = loadav >> 6;
		hdr.h_extra = kips;
		hdr.h_status = STD_OK;
	    }
	    break;

	case PS_SGETLOAD:
	    if (prv_number(&hdr.h_priv) != 0 ||
		    prv_decode(&hdr.h_priv, &rights,
					&segcap.cap_priv.prv_random) < 0 ||
		    rights != PRV_ALL_RIGHTS)
		hdr.h_status = STD_CAPBAD;
	    else
	    {
		static int switch_cnt;

		MON_EVENT("sgetload");
		hdr.h_offset = mem_free() << CLICKSHIFT;
		hdr.h_size = loadav >> 6;
		hdr.h_extra = kips;
		hdr.h_port = segsvr_put_port;
#ifndef NO_KERNEL_SECURITY
		/*
		 * We update the exec_cap after every 2nd call to sgetload.
		 * The only client that should be doing this call is the run
		 * server.
		 */
		if ((switch_cnt ^= 1) != 0)
		{
		    exec_switching = 1; /* we are using switching exec caps */
		    exec_cur ^= 1;
		    uniqport(&exec_chk[exec_cur]);
		    (void) prv_encode(&exec_cap[exec_cur], (objnum) 0,
						PSR_EXEC, &exec_chk[exec_cur]);
		}
#endif /* NO_KERNEL_SECURITY */
		hdr.h_priv = exec_cap[exec_cur];
		hdr.h_status = STD_OK;
	    }
	    break;
	/*
	** Operations on processes.
	*/
	case PS_EXEC:
#ifdef STATISTICS
	    stats.st_execs++;
#endif
	    /* Make sure it is one of the exec caps */
	    if (prv_number(&hdr.h_priv) != 0 ||
		(prv_decode(&hdr.h_priv, &rights, &exec_chk[exec_cur]) < 0 &&
		prv_decode(&hdr.h_priv, &rights, &exec_chk[exec_cur ^ 1]) < 0
		&& (prv_decode(&hdr.h_priv, &rights,
			    &segcap.cap_priv.prv_random) < 0 ||
			    rights != PRV_ALL_RIGHTS)))
	    {
#ifdef STATISTICS
		stats.st_badexecs++;
#endif
		hdr.h_status = STD_CAPBAD;
	    }
	    else
		if ((rights & PSR_EXEC) == 0)
		    hdr.h_status = STD_DENIED;
		else
		{
		    MON_EVENT("exec process");
		    /* malloc a buffer for the process descriptor struct */
		    if ((pd = pdmalloc(buffer, buffer + n)) == (process_d *) 0)
			hdr.h_status = STD_NOMEM;
		    else
		    {
			if (buf_get_pd(buffer, buffer + n, pd) == (char *) 0)
			    hdr.h_status = STD_ARGBAD;
			else
			{
			    hdr.h_status = create_process(&hdr.h_priv, pd, n);
			    if (hdr.h_status == STD_OK)
				hdr.h_port = segsvr_put_port;
			}
			free((_VOIDSTAR) pd); /* release memory used for pd */
		    }
		}
	    break;

	case PS_STUN:
	    if (n != 0)
		hdr.h_status = STD_ARGBAD;
	    else
	    {
		MON_EVENT("stun process");
		hdr.h_status = ppro_signal(&hdr.h_priv, (signum) hdr.h_offset);
	    }
	    break;

	case PS_SETOWNER:
	    if (buf_get_cap(buffer, buffer + n, &ownercap) == (char *) 0)
		hdr.h_status = STD_ARGBAD;
	    else
	    {
		MON_EVENT("set owner");
		hdr.h_status = ppro_setowner(&hdr.h_priv, &ownercap);
	    }
	    break;

	case PS_GETOWNER:
	    if (n != 0)
		hdr.h_status = STD_ARGBAD;
	    else
	    {
		MON_EVENT("get owner");
		hdr.h_status = ppro_getowner(&hdr.h_priv, &ownercap);
		if (hdr.h_status == STD_OK)
		{
		    hdr.h_port = ownercap.cap_port;
		    hdr.h_priv = ownercap.cap_priv;
		}
	    }
	    break;

	case PS_SETCOMMENT:
	    MON_EVENT("set comment");
	    hdr.h_status = ppro_setcomment(&hdr.h_priv, buffer, n);
	    break;
	case PS_GETCOMMENT:
	    MON_EVENT("get comment");
	    n2 = 0;
	    hdr.h_status = ppro_getcomment(&hdr.h_priv, &bufferp, &n2);
	    break;
	default:
	    MON_EVENT("no matching command");
	    hdr.h_status = STD_COMBAD;
	    break;
	}
#ifdef USE_AM6_RPC
	rpc_putrep(&hdr, bufferp, n2);
#else
	putrep(&hdr, bufferp, n2);
#endif
    }
}

static void
init_exec_caps()
{
    exec_chk[0] = segcap.cap_priv.prv_random;
    exec_chk[1] = exec_chk[0];

    /* Per default we begin with these caps being identical */
    (void) prv_encode(&exec_cap[0], (objnum) 0, PSR_EXEC, &exec_chk[0]);
    exec_cap[1] = exec_cap[0];
}

void
initproc()
{
    long	cols[NROOTCOLUMNS];
    errstat	err;
    int		i;
    capability	pub_cap;

    cols[0] = 0xFF;
    cols[1] = 0x3F;
    cols[2] = 0x3F;
    
    /*
     * We now generate the capability for the process/segment server
     * It has object number 0.
     */
    ppro_dirinit();
    uniqport(&segcap.cap_port);
    uniqport(&segcap.cap_priv.prv_random);
    if (prv_encode(&segcap.cap_priv, (objnum) 0, PRV_ALL_RIGHTS,
					    &segcap.cap_priv.prv_random) < 0)
	panic("initproc: prv_encode failed\n");
    
    init_exec_caps();

    /*
     * Now make a public "put" capability and publish it
     */
    priv2pub(&segcap.cap_port, &segsvr_put_port);

    pub_cap.cap_port = segsvr_put_port;
    pub_cap.cap_priv = segcap.cap_priv;
    err = dirappend(PROCESS_SVR_NAME, &pub_cap);
    if (err != STD_OK)
	printf("initproc: sp_append of proc server cap failed: %s\n",
								err_why(err));
    err = dirchmod(PROCESS_SVR_NAME, NROOTCOLUMNS, cols);
    if (err != STD_OK)
	printf("initproc: sp_chmod of proc directory failed: %s\n",
								err_why(err));

    /* Start the segment server threads */
    for (i = 0; i < NR_SEGSVR_THREADS; i++)
	NewKernelThread(segsvr, (vir_bytes) SEGSVR_STACKSIZE);
}


void
proc_change_svr_cap(chkfield)
port *	chkfield;
{
    capability	pub_cap;
    errstat	err;

    segcap.cap_priv.prv_random = *chkfield;
    if (prv_encode(&segcap.cap_priv, (objnum) 0, PRV_ALL_RIGHTS,
					    &segcap.cap_priv.prv_random) < 0)
	panic("initproc: prv_encode failed\n");
    pub_cap.cap_port = segsvr_put_port;
    pub_cap.cap_priv = segcap.cap_priv;
    err = dirreplace(PROCESS_SVR_NAME, &pub_cap);
    if (err != STD_OK) {
	printf("pro_change_svr_cap: dirreplace failed (%s)\n", err_why(err));
	panic("");
	/*NOTREACHED*/
    }
    /* Just in case the run server isn't running ... */
    if (!exec_switching)
	init_exec_caps();
}
