/*	@(#)syssvr.c	1.11	96/02/27 14:21:07 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 *  SYSTHREAD
 *
 *	This is the thread in the kernel that responds to system requests
 *	to reboot, give kernel stats, print the console buffer, etc.
 *	If the thread is running in a kernel with no directory server
 *	(usually a ROM kernel) it does not attempt to publish its port but
 *	does go to work listening to the port normally listened to by the
 *	kernel.  Otherwise it registers as the "syssvr" and is an entry in
 *	the kernel directory.
 */

#include "amoeba.h"
#include "ampolicy.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "machdep.h"
#include "global.h"
#include "map.h"
#ifdef FLNETCONTROL
#include "assert.h"
INIT_ASSERT
#endif
#include "printbuf.h"
#include "table.h"
#include "capset.h"
#include "direct/direct.h"
#include "sys/types.h"
#include "module/buffers.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "systask/systask.h"
#include "bullet/bullet.h"
#include "module/strmisc.h"
#include "string.h"
#include "sys/proto.h"

#include "sys/debug.h"

#if !SECURITY
#ifndef NOPROC
#include "monitor.h"
#endif
#endif

#define BUFFERSIZE 8192
#if PB_SIZE > BUFFERSIZE
#define OUTBUFFERSIZE	PB_SIZE
#else
#define OUTBUFFERSIZE	BUFFERSIZE
#endif

#ifndef SMALL_KERNEL

/* The following are used to implement kstat requests */


#ifndef KSTAT_BUFSZ
#define	KSTAT_BUFSZ	30000
#endif /* KSTAT_BUFSZ */

static char		kstatbuf[KSTAT_BUFSZ];
extern struct dumptab	dumptab[];


#endif /* SMALL_KERNEL */

#ifndef SYSTHREAD_STKSIZ
#define	SYSTHREAD_STKSIZ	0 /* default size */
#endif /* SYSTHREAD_STKSIZ */


static b_fsize kernelsize;
char *kernelbase;
int kernelflags;

static port		sys_random;
static capability	syscap;

extern void proc_change_svr_cap _ARGS(( port * chkfield ));
extern errstat dirreplace _ARGS(( char * name, capability * cap ));


static errstat
setupboot(cap, flags)
capability *cap;
int flags;
{
    errstat result;
    b_fsize haveread;

    result = b_size(cap, &kernelsize);
    if (result != STD_OK)
	return result;
    /* Allocate something extra.  This is useful for the i80386, which still
     * needs to move the segments around before it can really jump to the
     * new kernel.  We don't have to allocate another kernel buffer then.
     */
    kernelsize += 0x2000;
    kernelbase = (char *) getblk((vir_bytes) kernelsize);
    if (kernelbase == 0)
	return STD_NOMEM;
    result = b_read(cap, (b_fsize) 0, kernelbase, kernelsize, &haveread);
    if (result != STD_OK) {
	relblk((vir_bytes) kernelbase);
	kernelbase=0;
	return result;
    }
    kernelflags = flags;
    return STD_OK;
}

static void
doboot(commandline, flags)
    char *commandline;
    int flags;
{
    extern void bootkernel();

    stop_kernel();
    disable();
    printf("Booting next kernel\n\n");
    bootkernel(VTOP(kernelbase), (long) kernelsize, commandline, flags);
    /* NOTREACHED */
}


#ifndef ROMKERNEL

static interval time_till_revert;


static void
revert_to_original()
{
    /* Set kernel capability back to its default from boot time */
    DPRINTF(0, ("reverting to original\n"));
    dir_restore_cap();
    time_till_revert = 0;
}


/*ARGSUSED*/
static void
revert_sweeper(arg)
long arg;
{
    if (time_till_revert <= 0)
	return;
    if (--time_till_revert > 0)
	sweeper_set(revert_sweeper, 0L, (interval) 1000, 1);
    else
	revert_to_original();
}


static errstat
change_caps(time_out, chkfield)
interval time_out; /* in seconds */
port *	 chkfield;
{
    DPRINTF(0, ("change_caps: t = %d seconds\n", time_out));
    DPRINTF(0, ("change_caps: port 0 = %P\n", chkfield[0]));
    DPRINTF(0, ("change_caps: port 1 = %P\n", chkfield[1]));
    DPRINTF(0, ("change_caps: port 2 = %P\n", chkfield[2]));
    /*
     * Set the kernel directory, the syssvr and the proc svr capability's
     * check fields to those specified in chkfield.
     */
    if (time_out <= 0)
	return STD_ARGBAD;

    if (NULLPORT(&chkfield[0]))
    {
	revert_to_original();
    }
    else
    {
	DPRINTF(0, ("change_caps: altering capabilities\n"));
	/* Firstly the kernel directory server checkfield */
	dir_chgcap(&chkfield[0]);

	/* Secondly the syssvr */
	sys_random = chkfield[1];
	if (prv_encode(&syscap.cap_priv, (objnum) 1,
			(rights_bits) SYS_RGT_ALL, &sys_random) < 0)
	{
	    panic("syssvr: prv_encode failed\n");
	}
	dirreplace(DEF_SYSSVRNAME, &syscap);

#ifndef NOPROC
	/* Thirdly the process server */
	proc_change_svr_cap(&chkfield[2]);
#endif

	/* Finally start the timer */
	if (time_till_revert == 0)
	{
	    time_till_revert = time_out;
	    sweeper_set(revert_sweeper, 0L, (interval) 1000, 1);
	}
	else
	    time_till_revert = time_out;
    }
    return STD_OK;
}

#endif /* ROMKERNEL */

static void
init_syscap(gport, random)
port *	gport;
port *	random;
{
#ifndef ROMKERNEL
    uniqport(gport);
    uniqport(random);
#else /* ROMKERNEL */
    kernel_port(gport);
    kernel_port(random);
#endif /* ROMKERNEL */
}


static void
systhread _ARGS((void))
{
    bufsize	n;
    header	hdr;
    rights_bits	rights;
    capability	kernelcap;
    port	sys_getport;
    char 	*commandline;
#ifdef ROMKERNEL
    static char	inbuf[BUFFERSIZE];
#define outbuf	inbuf
#else
    static char	inbuf[BUFFERSIZE];
    static char	outbuf[OUTBUFFERSIZE];
#endif /* ROMKERNEL */
    char *	obuf_ptr;	/* points to buffer returned by putrep */
    char *	p;

    init_syscap(&sys_getport, &sys_random);
    priv2pub(&sys_getport, &syscap.cap_port);
    if (prv_encode(&syscap.cap_priv, (objnum) 1, (rights_bits) SYS_RGT_ALL,
							    &sys_random) < 0)
    {
	panic("syssvr: prv_encode failed\n");
    }
#ifndef ROMKERNEL
    dirappend(DEF_SYSSVRNAME, &syscap);
#endif /* ROMKERNEL */

    for (;;)
    {
	hdr.h_port = sys_getport;
#ifdef USE_AM6_RPC
	n = rpc_getreq(&hdr, (bufptr) inbuf, BUFFERSIZE);
#else /* USE_AM6_RPC */
	n = getreq(&hdr, (bufptr) inbuf, BUFFERSIZE);
#endif /* USE_AM6_RPC */
	if (ERR_STATUS(n))
	{
	    printf("syssvr: getreq failed (%d)\n", ERR_CONVERT(n));
	    continue;
	}

	if (prv_number(&hdr.h_priv) != 1 ||
	    prv_decode(&hdr.h_priv, &rights, &sys_random) < 0)
	{
	    hdr.h_status = STD_CAPBAD;
	    n = 0;
	}
	else
	{
	    obuf_ptr = outbuf;
	    switch (hdr.h_command)
	    {
	    case STD_INFO:
#ifdef ROMKERNEL
		bprintf(outbuf, outbuf + OUTBUFFERSIZE, "bootstrap kernel");
#else /* ROMKERNEL */
		bprintf(outbuf, outbuf + OUTBUFFERSIZE, "system server");
#endif /* ROMKERNEL */
		hdr.h_size = n = strlen(outbuf);
		hdr.h_status = STD_OK;
		break;

#ifdef ROMKERNEL
	    case STD_RESTRICT:
		rights &= hdr.h_offset;
		if (prv_encode(&hdr.h_priv, (objnum) 1, rights, &sys_random) < 0)
		    hdr.h_status = STD_ARGBAD;
		else
		    hdr.h_status = STD_OK;
		n = 0;
		break;
	    
	    case STD_TOUCH:
		n = 0;
		hdr.h_status = STD_OK;
		break;

	    case STD_SETPARAMS:
		n = 0;
		hdr.h_status = STD_OK;
		break;

	    case STD_GETPARAMS:
		n = 0;
		hdr.h_status = STD_OK;
		break;
#endif /* ROMKERNEL */

	    case SYS_BOOT:	   /* Boot new kernel */
		if (rights & SYS_RGT_BOOT) {
		    p = buf_get_cap(inbuf, inbuf + BUFFERSIZE, &kernelcap);
		    p = buf_get_string(p, inbuf + BUFFERSIZE, &commandline);
		    if (p) {
			hdr.h_status = setupboot(&kernelcap, (int) hdr.h_extra);
		    } else
			hdr.h_status = STD_SYSERR;
		} else
		    hdr.h_status = STD_DENIED;
#ifdef USE_AM6_RPC
		rpc_putrep(&hdr, NILBUF, 0);
#else /* USE_AM6_RPC */
		putrep(&hdr, NILBUF, 0);
#endif /* USE_AM6_RPC */
		if (hdr.h_status == STD_OK)
		    doboot(commandline, (int) hdr.h_extra);
		continue;	/* On error or just in case doboot returns */

	    case SYS_PRINTBUF:  /* print console buffer */
		if (!(rights & SYS_RGT_READ))
		{
		    n = 0;
		    hdr.h_status = STD_DENIED;
		}
		else
		{
		    if (!pb_descr)
		    {
			n= 0;
			hdr.h_offset= 0;
			hdr.h_status = STD_OK;
			break;
		    }
		    /* To guarantee a consistent print buffer, copy it
		     * to outbuf first.  Extra data could be added to the
		     * print buffer while we're transferring it to the client!
		     */
		    obuf_ptr = outbuf;
		    n = pb_descr->pb_end - pb_descr->pb_buf;
		    (void) memcpy((_VOIDSTAR) outbuf,
				  (_VOIDSTAR) pb_descr->pb_buf, (size_t) n);
		    hdr.h_offset = pb_descr->pb_next - pb_descr->pb_buf;
		    hdr.h_status = STD_OK;
		}
		break;

#ifndef SMALL_KERNEL
	    case SYS_KSTAT:
	    {
		struct dumptab * dp;
		int	letter;
		char *	p;
		char *	end;

		n = 0;
		p = kstatbuf;
		end = kstatbuf + KSTAT_BUFSZ;
		switch (letter = (int) hdr.h_extra)
		{
		case '?': /* return a list of flags with info strings */
		    for (dp = dumptab; dp->d_func; dp++)
		        p = bprintf(p, end, "%c  %s\n", dp->d_char, dp->d_help);
		    obuf_ptr = kstatbuf;
		    n = p - kstatbuf;
		    hdr.h_status = STD_OK;
		    break;
		    
		default:
		    for (dp = dumptab; dp->d_func; dp++)
			if (dp->d_char == letter)
			{
			    if ((rights & dp->d_req_rgts) != dp->d_req_rgts)
			    {
				hdr.h_status = STD_DENIED;
			    }
			    else
			    {
				n = (*dp->d_func)(p, end);
				obuf_ptr = kstatbuf;
				hdr.h_status = STD_OK;
			    }
			    break;
			}
		    if (dp->d_func == 0)
			hdr.h_status = STD_ARGBAD;
		    break;
		}
		break;
	    }
#endif /* SMALL_KERNEL */

#ifdef FLNETCONTROL
	    case SYS_NETCONTROL:  /* net control */
	    {
		char *bp, *ep;
		int16 nw, delay, loss, on, trusted;

		if (!(rights & SYS_RGT_CONTROL))
		{
		    n = 0;
		    hdr.h_status = STD_DENIED;
		}
		else
		{
		    bp = inbuf;
		    ep = inbuf + n;
		    bp = buf_get_int16(bp, ep, &nw);
		    bp = buf_get_int16(bp, ep, &delay);
		    bp = buf_get_int16(bp, ep, &loss);
		    bp = buf_get_int16(bp, ep, &on);
		    bp = buf_get_int16(bp, ep, &trusted);
		    if (bp != ep || !flip_control(nw, &delay, &loss, &on,
						  &trusted))
		    {
			n = 0;
			hdr.h_status = STD_ARGBAD;
			break;
		    }
		    hdr.h_status = STD_OK;
		    bp = outbuf;
		    ep = outbuf + OUTBUFFERSIZE;
		    bp = buf_put_int16(bp, ep, delay);
		    bp = buf_put_int16(bp, ep, loss);
		    bp = buf_put_int16(bp, ep, on);
		    bp = buf_put_int16(bp, ep, trusted);
		    assert(bp);
		    n = bp - outbuf;
		}
		break;
	    }
#endif /* FLNETCONTROL */

#ifndef ROMKERNEL
	    case SYS_CHGCAP:
		if (rights != SYS_RGT_ALL)
		    hdr.h_status = STD_DENIED;
		else
		{
		    port chkfields[3];
		    char * b;
		    char * e;

		    e = inbuf + n;
		    b = buf_get_port(inbuf, e, &chkfields[0]);
		    b = buf_get_port(b, e, &chkfields[1]);
		    b = buf_get_port(b, e, &chkfields[2]);
		    if (b != e)
			hdr.h_status = STD_ARGBAD;
		    else
			hdr.h_status = change_caps(hdr.h_offset, chkfields);
		}
		n = 0;
		break;
#endif /* ROMKERNEL */

	    default:
		hdr.h_status = STD_COMBAD;
		n = 0;
		break;
	    }
	}

#ifdef USE_AM6_RPC
	rpc_putrep(&hdr, (bufptr) obuf_ptr, n);
#else /* USE_AM6_RPC */
	putrep(&hdr, (bufptr) obuf_ptr, n);
#endif /* USE_AM6_RPC */
    }
}

void
init_systask()
{
    NewKernelThread(systhread, (vir_bytes) SYSTHREAD_STKSIZ);
}
