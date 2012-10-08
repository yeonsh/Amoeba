/*	@(#)syscall.c	1.13	96/02/27 13:45:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * syscall.c
 *
 * New system call interface.
 *
 * Upon entry to this routine the kernel stack has a copy of all
 * possible arguments to routines (4). Unused arguments are zeroed
 * out in the library stub. The stack layout:
 *
 *	arg4		ap[3]
 *	arg3		ap[2]
 *	arg2		ap[1]
 *	arg1 <----|	ap[0]
 *	ap   -----|
 *	code
 *
 * Some system calls require more than four arguments; in that case the
 * first three arguments are pushed onto the stack, just as described
 * above, and the ap[3] contains a pointer to a block containing the
 * remaining arguments. This block is mapped in by the individual cases.
 */
#ifndef NOPROC
#include <amoeba.h>
#include <cmdreg.h>
#include <stderr.h>
#include <syscall.h>
#include <global.h>
#include <machdep.h>
#include <exception.h>
#include <kthread.h>
#include <process/proc.h>
#include <map.h>
#include <module/mutex.h>
#ifdef FLIPGRP
#include <group.h>
#endif
#include <sys/proto.h>
#include <module/rawflip.h>
#ifdef PROFILE
#include <module/profiling.h>
#endif

#define	umap_args(x)	(umap(curthread, (vir_bytes)(x), \
			      (vir_bytes)(3 * sizeof(void *)), 1))
#define umap_ptr(x)	(umap(curthread, (vir_bytes)(x), \
			      (vir_bytes)(sizeof(void *)), 1))
#define umap_buf(x,s,m)	(umap(curthread, (vir_bytes)(x), (vir_bytes)(s), m))

extern long	  thread_sigpend;

extern void       exitprocess();
extern long       getinfo();
extern long       getlocal();
extern segid      map_segment();
extern void       sendall();
extern errstat    usr_unmap();
extern errstat    map_grow();

long
newsys(code, ap)
    long code;
    long ap[];
{
    long va1, va2;
    int i = -1;
#ifdef FLIPGRP
    long va3;
    long *nap;
#endif

    switch (code) {
    case SC_sys_null:
	i = 0;
	break;
    case SC_sys_newthread:
	i = newuthread((vir_bytes) ap[0], (vir_bytes) ap[1], ap[2]);
	break;
    case SC_getinfo:
	va1 = va2 = 0;
	if (ap[0]) {
	    if ((va1 = umap_ptr(ap[0])) == 0)
		break;
	    va1 = PTOV(va1);
	}
	if (ap[1]) {
	    if ((va2 = umap_ptr(ap[1])) == 0)
		break;
	    va2 = PTOV(va2);
	}
	i = getinfo((capability *)va1, (process_d *)va2, (int) ap[2]);
	break;
    case SC_exitprocess:
	exitprocess(ap[0]);
	break;
    case SC_sys_setvec:
	i = 0;
	sigcatch((vir_bytes) ap[0], (vir_bytes) ap[1]);
	break;
    case SC_sig_raise:
	sendall((struct process *)0, ap[0], 1);
	break;
    case SC_sys_sigret:
	/* ap[0] is fault frame to return to, as.S will take care of it */
	thread_sigpend |= TDS_SRET;
	break;
    case SC_sysseg_map:
	if (ap[0]) {
	    if ((va1 = umap_ptr(ap[0])) == 0)
		break;
	    va1 = PTOV(va1);
	} else 
	    va1 = 0;
	i = (long) map_segment((struct process *)0, (vir_bytes) ap[1],
			    (vir_bytes) ap[2], ap[3], (capability *) va1, 0L);
	break;
    case SC_sysseg_unmap:
	if (ap[1]) {
	    if ((va2 = umap_ptr(ap[1])) == 0)
		break;
	    va2 = PTOV(va2);
	} else
	    va2 = 0;
	i  = usr_unmap((struct process *) 0,
	    (segid) ap[0], (capability *)va2);
	if (i == STD_COMBAD) progerror();
	break;
    case SC_sysseg_grow:
	if ((i = (long) map_grow((segid) ap[0], ap[1])) == STD_COMBAD)
	    progerror();	
	break;
    case SC_mu_trylock:
	if ((va1 = umap_ptr(ap[0])) == 0)
	    break;
	i = mu_trylock((mutex *)PTOV(va1), ap[1]);
	break;
    case SC_mu_unlock:
	if ((va1 = umap_ptr(ap[0])) == 0)
	    break;
	mu_unlock((mutex *) PTOV(va1));
	break;
    case SC_sys_getlocal:
	i = getlocal();
	break;
    case SC_sys_milli:
	i = getmilli();
	break;
#ifdef FLIPGRP
    case SC_grp_create:
	/* ap[3] contains pointer to parameter block */
	if ((nap = (long *) umap_args(ap[3])) == 0)
	    break;
	if ((va1 = umap_ptr(ap[0])) == 0)
	    break;
	i = grp_create((port *) PTOV(va1), (g_indx_t) ap[1], (g_indx_t) ap[2],
			(uint32) nap[0], (uint32) nap[1]);
	break;
    case SC_grp_join:
	if ((va1 = umap_ptr(ap[0])) == 0)
	    break;
	i = grp_join((header *) PTOV(va1));
	break;
    case SC_grp_leave:
	if ((va2 = umap_ptr(ap[1])) == 0)
	    break;
	i = grp_leave((g_id_t) ap[0], (header *) PTOV(va2));
	break;
    case SC_grp_send:
	if ((va2 = umap_ptr(ap[1])) == 0)
	    break;
	if (ap[3] == 0)
	    va3 = 0;
	else
	    va3 = umap_buf(ap[2], ap[3], 0);
	i = grp_send((g_id_t) ap[0], (header_p) PTOV(va2),
				(bufptr) PTOV(va3), (uint32) ap[3]);
	break;
    case SC_grp_receive:
	/* ap[3] contains pointer to parameter block */
	if ((nap = (long *) umap_args(ap[3])) == 0)
	    break;
	if ((va2 = umap_ptr(ap[1])) == 0)
	    break;
	if (nap[0] == 0)
	    va3 = 0;
	else
	    va3 = umap_buf(ap[2], nap[0], 1);
	/* We abuse the name va1 and put arg 5 in it */
	va1 = umap_buf(nap[1], sizeof (int), 1);
	if (nap[1] != 0 && va1 == 0) {
	    i = STD_ARGBAD;
	    break;
	}
	i = grp_receive((g_id_t) ap[0], (header_p) PTOV(va2),
		(bufptr) PTOV(va3), (uint32) nap[0], (int *) PTOV(va1));
	break;
    case SC_grp_reset:
	if ((va2 = umap_ptr(ap[1])) == 0)
	    break;
	i = grp_reset((g_id_t) ap[0], (header_p) PTOV(va2), (g_indx_t) ap[2]);
	break;
    case SC_grp_info:
	/* ap[3] contains pointer to parameter block */
	if ((nap = (long *) umap_args(ap[3])) == 0)
	    break;
	if ((va2 = umap_ptr(ap[1])) == 0)	/* port */
	    break;
	if ((va3 = umap_buf(ap[2], sizeof (grpstate_t), 1)) == 0)	/* grpstate */
	    break;
	/* We abuse va1 and put arg 4 (memlist) in it */
	va1 = umap_buf(nap[0], nap[1] * sizeof (g_indx_t), 1);
	i = grp_info((g_id_t) ap[0], (port *) PTOV(va2), (grpstate_p) PTOV(va3),
	    (g_indx_t *) PTOV(va1), (g_indx_t) nap[1]);
	break;
    case SC_grp_set:
	/* ap[3] contains pointer to parameter block */
	if ((nap = (long *) umap_args(ap[3])) == 0)
	    break;
	if ((va2 = umap_ptr(ap[1])) == 0)
	    break;
	i = grp_set((g_id_t) ap[0], (port *) PTOV(va2), (interval) ap[2],
		    (interval) nap[0], (interval) nap[1], (uint32) nap[2]);
	break;
    case SC_grp_forward:
	if ((va2 = umap_ptr(ap[1])) == 0)
	    break;
	i = grp_forward(ap[0], (port *) PTOV(va2), (g_indx_t) ap[2]);
	break;
#endif /* FLIPGRP */
    case SC_thread_get_max_prio:
	if ((va1 = umap_ptr(ap[0])) == 0)
	    break;
	i = thread_get_max_priority((long *)PTOV(va1));
	break;
    case SC_thread_set_priority:
	if ((va2 = umap_ptr(ap[1])) == 0)
	    break;
	i = thread_set_priority(ap[0], (long *)PTOV(va2));
	break;
    case SC_thread_en_preempt:
	/* Note: we pass the (un-umapped) _thread_local pointer to
	 * thread_enable_preemption() which will check that it is valid.
	 * Each time we use it to update _thread_local we will umap it,
	 * because in the meantime the process might have changed its
	 * memory map.
	 */
	i = thread_enable_preemption((long **) ap[0]);
	break;
    case SC_rawflip:
	if ((va1 = umap_buf(ap[0], sizeof(struct rawflip_parms), 1)) == 0)
	    break;
	i = do_rawflip((struct rawflip_parms *) va1);
	break;
#ifdef PROFILE
    case SC_sys_profil:
	if ((va1 = umap_buf(ap[0], sizeof(struct profile_parms), 1)) == 0)
	    break;
	i = user_profile((struct profile_parms *) va1);
	break;
#endif
    default:
	printf("Illegal syscall %d\n", code);
	progerror();	
    }
    return i;
}

sc_exitthread(ready)
long *ready;
{
    long *kready;

    if (ready == NULL) {
	kready = NULL;
    } else {
        kready = (long *) umap_ptr(ready);
        /* Note: kready == 0 is allowed for backward compatibility */
    }
    exitthread(kready);
    /*NOTREACHED*/
}

/* The header is already mapped in so now we umap the buffer */
sc_rpc_getreq(hdr, buf, cnt)
header *  hdr;
bufptr	  buf;
f_size_t  cnt;
{
    bufptr	kbuf;

    if (cnt != 0)
	kbuf = (bufptr) umap_buf(buf, cnt, 1);

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
	kbuf = (bufptr) umap_buf(buf, cnt, 0);

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
	kbuf1 = (bufptr) umap_buf(buf1, cnt1, 0);

    if (cnt2 != 0)
	kbuf2 = (bufptr) umap_buf(buf2, cnt2, 1);

#ifdef SECURITY
    return secure_trans(hdr1, kbuf1, cnt1, hdr2, kbuf2, cnt2);
#else
    return rpc_trans(hdr1, kbuf1, cnt1, hdr2, kbuf2, cnt2);
#endif
}


/* The header is already mapped in so now we umap the buffer */
sc_getreq(hdr, buf, cnt)
header *  hdr;
bufptr	  buf;
_bufsize  cnt;
{
    bufptr	kbuf;

    if (cnt != 0)
	kbuf = (bufptr) umap_buf(buf, cnt, 1);

#ifdef USE_AM6_RPC
    return rpc_getreq(hdr, kbuf, (f_size_t) cnt);
#else
    return getreq(hdr, kbuf, (bufsize) cnt);
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
	kbuf = (bufptr) umap_buf(buf, cnt, 0);

#ifdef USE_AM6_RPC
    return rpc_putrep(hdr, kbuf, (f_size_t) cnt);
#else
    putrep(hdr, kbuf, (bufsize) cnt);
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
	kbuf1 = (bufptr) umap_buf(buf1, cnt1, 0);

    if (cnt2 != 0)
	kbuf2 = (bufptr) umap_buf(buf2, cnt2, 1);

#ifdef USE_AM6_RPC
    return rpc_trans(hdr1, kbuf1, (f_size_t) cnt1, hdr2, kbuf2, (f_size_t) cnt2);
#else
    return trans(hdr1, kbuf1, (bufsize) cnt1, hdr2, kbuf2, (bufsize) cnt2);
#endif
}
#endif /* NOPROC */
