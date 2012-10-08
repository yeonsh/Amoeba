/*	@(#)syscall.c	1.13	96/02/27 13:46:18 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef NOPROC

/*
** New System call interface.
**
** Upon entry to this routine, 'usp' is the user sp. The user stack looks
** as follows:
**	argn
**	...
**	arg2
**	arg1
**	return address from syscall stub
**	local ( thread's glocal pointer )
** sp-> system call number
*/

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "syscall.h"
#include "global.h"
#include "machdep.h"
#include "exception.h"
#include "kthread.h"
#include "process/proc.h"
#include "map.h"
#include "module/mutex.h"
#ifdef FLIPGRP
#include "group.h"
#endif
#include "sys/proto.h"
#include "module/rawflip.h"
#include "memaccess.h"
#ifdef PROFILE
#include <module/profiling.h>
#endif

#define mm_umap(x, t) (umap(curthread, (vir_bytes) (x), (vir_bytes) sizeof(t), 1))
#define mm_umapbuf(x, s, m) (umap(curthread, (vir_bytes) (x), (vir_bytes) (s), m))

extern long thread_sigpend;

long
newsys(usp)
volatile long *usp;	/* usp is value result user stackpointer */
{
    void	exitprocess();
    long	getinfo();
    long	getlocal();
    segid	map_segment();
    void	progerror();
    void	sendall();
    errstat	usr_unmap();
    errstat	map_grow();

    extern struct thread *curthread;
    long va1, va2;
#ifdef FLIPGRP
    long va3;
#endif /* FLIPGRP */
    long i;
    register long code;
    register long *ap;

    ap = (long *) umap(curthread, (vir_bytes) usp, (vir_bytes) 32, 1);
    if (ap == 0)
    {
#ifndef NDEBUG
	printf("newsys: invalid usp %x. Process killed.\n", usp);
#endif
	progerror();
	return -1;
    }
    code = *ap;
    ap++; /* point at one past the trap code */
    ap++; /* skip over 'local' which was saved on the stack */
    i = -1; /*BUG-invalid error code?!*/
    switch(code) {
    /*
    ** Standard system calls.
    */
    case SC_sys_newthread:
	i = newuthread((vir_bytes) ap[1], (vir_bytes) ap[2], ap[3]);
	break;
    case SC_getinfo:
    /* we musn't apply PTOV until after checking the result of mm_umap
    ** and we want to be able to pass NULL pointers to getinfo so we
    ** don't unmap the NULL pointer.
    */
	va1 = va2 = 0;
	if (ap[1])
	{
	    if ((va1 = mm_umap(ap[1], capability)) == 0)
		break;
	    va1 = PTOV(va1);
	}
	if (ap[2])
	{
	    if ((va2 = mm_umap(ap[2], process_d)) == 0)
		break;
	    va2 = PTOV(va2);
	}
	i = getinfo((capability *) va1, (process_d *) va2, (int) ap[3]);
	break;
    case SC_exitprocess:
	exitprocess(ap[1]);
	break;
    case SC_sys_setvec:
	i = 0;
	sigcatch((vir_bytes) ap[1], (vir_bytes) ap[2]);
	break;
    case SC_sig_raise:
	sendall((struct process *) 0, ap[1], 1);
	break;
    case SC_sys_sigret:
	{
	    /*
	     * HACK: put address of new fault frame in usp.
	     * Look in as.S to understand this (optimistic)
	     * as.S -> syscall.c -> as.S -> trap.c (swtrap, sigreturn) -> as.S
	     */
	    volatile long **usp_addr = &usp;

	    *usp_addr = (long *) ap[1];
	}
	thread_sigpend |= TDS_SRET;
	break;
    case SC_sysseg_map:
	if (ap[1]) {
	    if ((va1 = mm_umap(ap[1], capability)) == 0)
		break;
	    va1 = PTOV(va1);
	}
	else
	    va1 = 0;
	i = (long) map_segment((struct process *) 0, (vir_bytes) ap[2],
			(vir_bytes) ap[3], ap[4], (capability *) va1, 0L);
	break;
    case SC_sysseg_unmap:
	if (ap[2]) {
	    if ((va2 = mm_umap(ap[2], capability)) == 0)
		break;
	    va2 = PTOV(va2);
	}
	else
	    va2 = 0;
	i = usr_unmap((struct process *) 0, (segid) ap[1], (capability *) va2);
	if (i == STD_COMBAD)
	    progerror();
	break;
    case SC_sysseg_grow:
	if ((i = (long) map_grow((segid) ap[1], ap[2])) == STD_COMBAD)
	    progerror();
	break;
    case SC_mu_trylock:
	if ((va1 = mm_umap(ap[1], mutex)) == 0)
	    break;
	i = mu_trylock((mutex *) PTOV(va1), ap[2]);
	break;
    case SC_mu_unlock:
	if ((va1 = mm_umap(ap[1], mutex)) == 0)
	    break;
	mu_unlock((mutex *) PTOV(va1));
	break;
    case SC_sys_null:
	i = 0;
	break;
    case SC_sys_getlocal:
	i = getlocal();
	break;
    case SC_sys_milli:
	i = getmilli();
	break;
#ifdef FLIPGRP
    case SC_grp_create:
	if ((va1 = mm_umap(ap[1], port)) == 0)
	    break;
	i = grp_create((port *) PTOV(va1), (g_indx_t) ap[2],
		(g_indx_t) ap[3], (uint32) ap[4], (uint32) ap[5]);
	break;
    case SC_grp_join:
	if ((va1 = mm_umap(ap[1], header)) == 0)
	    break;
	i = grp_join((header_p) PTOV(va1));
	break;
    case SC_grp_leave:
	if ((va2 = mm_umap(ap[2], header)) == 0)
	    break;
	i = grp_leave((g_id_t) ap[1], (header_p) PTOV(va2));
	break;
    case SC_grp_send:
	if ((va2 = mm_umap(ap[2], header)) == 0)
	    break;
	/* Map in the buffer (cnt bytes) */
	if (ap[4] == 0)
	    va3 = 0;
	else
	    va3 = mm_umapbuf(ap[3], ap[4], 0);
	i = grp_send((g_id_t) ap[1], (header *) PTOV(va2), (bufptr) PTOV(va3),
								(uint32) ap[4]);
	break;
    case SC_grp_receive:
	if ((va2 = mm_umap(ap[2], header)) == 0)
	    break;
	/* Map in the buffer (cnt bytes) */
	if (ap[4] == 0)
	    va3 = 0;
	else
	    va3 = mm_umapbuf(ap[3], ap[4], 1);
	/* We reuse va1 - it should really be called va5 */
	va1 = mm_umap(ap[5], int);
	if (ap[5] != 0 && va1 == 0) {
	    i = STD_ARGBAD;
	    break;
	}
	i = grp_receive((g_id_t) ap[1], (header_p) PTOV(va2),
		(bufptr) PTOV(va3), (uint32) ap[4], (int *) PTOV(va1));
	break;
    case SC_grp_reset:
	if ((va2 = mm_umap(ap[2], header)) == 0)
	    break;
	i = grp_reset((g_id_t) ap[1], (header_p) PTOV(va2), (g_indx_t) ap[3]);
	break;
    case SC_grp_info:
	if ((va2 = mm_umap(ap[2], port)) == 0)
	    break;
	if ((va3 = mm_umap(ap[3], grpstate_t)) == 0)
	    break;
	/* We reuse va1 - it should really be called va4 */
	va1 = mm_umapbuf(ap[4], sizeof(g_indx_t) * ap[5], 1);
	i = grp_info((g_id_t) ap[1], (port *) PTOV(va2), 
		     (grpstate_p) PTOV(va3), (g_indx_t *) PTOV(va1),
		     (g_indx_t) ap[5]);
	break;
    case SC_grp_set:
	if ((va2 = mm_umap(ap[2], port)) == 0)
	    break;
	i = grp_set((g_id_t) ap[1], (port *) PTOV(va2), (interval) ap[3],
		    (interval) ap[4], (interval) ap[5], (uint32) ap[6]);
	break;
    case SC_grp_forward:
	if ((va2 = mm_umap(ap[2], port)) == 0)
	    break;
	i = grp_forward((g_id_t) ap[1], (port *) PTOV(va2), (g_indx_t) ap[3]);
	break;
#endif /* FLIPGRP */
    case SC_thread_get_max_prio:
	if ((va1 = mm_umap(ap[1], long)) == 0)
	    break;
	i = thread_get_max_priority((long *)PTOV(va1));
	break;
    case SC_thread_set_priority:
	if ((va2 = mm_umap(ap[2], long)) == 0)
	    break;
	i = thread_set_priority(ap[1], (long *)PTOV(va2));
	break;
    case SC_thread_en_preempt:
	/* The argument is address of the "_thread_local" pointer (or NULL).
	 * 
	 * We pass the (un-umapped) user pointer to thread_enable_preemption(),
	 * which will check that it is valid.  Each time we use it to update
	 * _thread_local we will umap it, because in the meantime the process
	 * might have changed its memory map.
	 */
	i = thread_enable_preemption((long **) ap[1]);
	break;
    case SC_rawflip:
	if ((va1 = mm_umap(ap[1], struct rawflip_parms)) == 0)
	    break;
	i = do_rawflip((struct rawflip_parms *) va1);
	break;
#ifdef PROFILE
    case SC_sys_profil:
	if ((va1 = mm_umap(ap[1], struct profile_parms)) == 0)
	    break;
        i = user_profile(va1);
        break;
#endif
    default:
#ifndef NDEBUG
	{
	    int j;

	    printf("Illegal syscall %d, usp=%x, ap=%x\n", code, usp, ap);
	    for (j = -1; j < 8; j++)
		printf("%x ", ap[j]);
	    printf("\n");
	}
#endif
	progerror();
	break;
    }
    return i;
}

u_mu_trylock(m, tout)
vir_bytes m;
interval tout;
{
	phys_bytes va1;

	if ((va1 = mm_umap(m, mutex)) == 0)
	    return -1;
	return  mu_trylock((mutex *) PTOV(va1), tout);
}

u_mu_unlock(m)
vir_bytes m;
{
	phys_bytes va1;

	if ((va1 = mm_umap(m, mutex)) == 0)
	    return;
	mu_unlock((mutex *) PTOV(va1));
}

void
sc_exitthread(ready)
vir_bytes ready;
{
    long *kready;

    if (ready == 0) {
	kready = NULL;
    } else {
	kready = (long *) mm_umap(ready, long);
	/* note: the mm_umap is allowed to fail (backward compatibility) */
    }

    exitthread(kready);
}

/* The header is already mapped in so now we umap the buffer */
sc_rpc_getreq(hdr, buf, cnt)
header *  hdr;
bufptr	  buf;
f_size_t  cnt;
{
    bufptr	kbuf;

    if (cnt != 0)
	kbuf = (bufptr) mm_umapbuf(buf, cnt, 1);

#ifdef SECURITY
    return secure_getreq(hdr, kbuf, cnt);
#else
    return rpc_getreq(hdr, kbuf, cnt);
#endif
}

/* The header is already mapped in so now we umap the buffer */
sc_rpc_putrep(hdr, buf, cnt)
header *  hdr;
bufptr	  buf;
f_size_t  cnt;
{
    bufptr	kbuf;

    if (cnt != 0)
	kbuf = (bufptr) mm_umapbuf(buf, cnt, 0);

#ifdef SECURITY
    return secure_putrep(hdr, kbuf, cnt);
#else
    return rpc_putrep(hdr, kbuf, cnt);
#endif
}

/*
 * This routine is called after hdr1 & hdr2 are "mapped in".  It still has
 * to umap the buffers
 */
sc_rpc_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
header *  hdr1;
bufptr	  buf1;
f_size_t   cnt1;
header *  hdr2;
bufptr	  buf2;
f_size_t   cnt2;
{
    bufptr	kbuf1;
    bufptr	kbuf2;

    if (cnt1 != 0)
	kbuf1 = (bufptr) mm_umapbuf(buf1, cnt1, 0);

    if (cnt2 != 0)
	kbuf2 = (bufptr) mm_umapbuf(buf2, cnt2, 1);

#ifdef SECURITY
    return secure_trans(hdr1, kbuf1, cnt1, hdr2, kbuf2, cnt2);
#else
    return rpc_trans(hdr1, kbuf1, cnt1, hdr2, kbuf2, cnt2);
#endif
}

#ifdef __STDC__
#define buf_size	_bufsize
#else
#define buf_size	bufsize
#endif

/* The header is already mapped in so now we umap the buffer */
sc_getreq(hdr, buf, cnt)
header *  hdr;
bufptr	  buf;
_bufsize  cnt;
{
    bufptr	kbuf;

    if (cnt != 0)
	kbuf = (bufptr) mm_umapbuf(buf, cnt, 1);

#ifdef USE_AM6_RPC
    return rpc_getreq(hdr, kbuf, cnt);
#else
    return getreq(hdr, kbuf, (buf_size) cnt);
#endif
}

/* The header is already mapped in so now we umap the buffer */
sc_putrep(hdr, buf, cnt)
header *  hdr;
bufptr	  buf;
_bufsize  cnt;
{
    bufptr	kbuf;

    if (cnt != 0)
	kbuf = (bufptr) mm_umapbuf(buf, cnt, 0);

#ifdef USE_AM6_RPC
    return rpc_putrep(hdr, kbuf, cnt);
#else
    putrep(hdr, kbuf, (buf_size) cnt);
    return STD_OK;
#endif
}

/*
 * This routine is called after hdr1 & hdr2 are "mapped in".  It still has
 * to umap the buffers
 */
sc_trans(hdr1, buf1, cnt1, hdr2, buf2, cnt2)
header *  hdr1;
bufptr	  buf1;
_bufsize  cnt1;
header *  hdr2;
bufptr	  buf2;
_bufsize  cnt2;
{
    bufptr	kbuf1;
    bufptr	kbuf2;

    if (cnt1 != 0)
	kbuf1 = (bufptr) mm_umapbuf(buf1, cnt1, 0);

    if (cnt2 != 0)
	kbuf2 = (bufptr) mm_umapbuf(buf2, cnt2, 1);

    return trans(hdr1, kbuf1, (buf_size) cnt1, hdr2, kbuf2, (buf_size) cnt2);
}
#endif /* NOPROC */
