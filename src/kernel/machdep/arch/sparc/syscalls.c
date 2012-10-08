/*	@(#)syscalls.c	1.5	96/02/27 13:47:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * FILE: syscalls.c -- function definitions for the system call interface.
 * These functions basically just mangled the arguments from the syscall
 * stub into something that the kernel routine can deal with. By and large,
 * this is a waste, as the system calls should be able to enter the kernel
 * directly, but I guess one extra procedure call would get lost in the
 * noise of a kernel trap anyway.
 * Most of what goes on here is just argument rearrangement and user address
 * mapping via umap().
 *
 * Author: Philip Shafer <phil@procyon.ics.Hawaii.Edu>
 *	   ( Univerity of Hawai'i, Manoa ) November 1990
 * Modified: Hans van Staveren, Greg Sharp Nov 1992
 *		- added group routines and null system call and sigret
 * Modified: Leendert van Doorn March 1994
 *		- added active message system calls
 */
#define MPX	/* Hack */

#include "amoeba.h"
#include "debug.h"
#include "exception.h"
#include "cmdreg.h"
#include "fault.h"
#include "machdep.h"
#include "mmuconst.h"
#include "psr.h"
#include "map.h"
#include "process/proc.h"
#include "seg.h"
#include "stderr.h"

#include "global.h"
#include "kthread.h"
#include "stdlib.h"
#include "string.h"
#include "module/mutex.h"
#include "sys/flip/measure.h"
#ifdef FLIPGRP
#include "group.h"
#endif
#ifdef ACTMSG
#include "actmsg.h"
#endif

#include "sys/proto.h"
#include "module/rawflip.h"
#ifdef PROFILE
#include "module/profiling.h"
#endif
#include "protocols/rawflip.h"

#ifndef NOPROC

void	exitprocess();
long	getinfo();
long	getlocal();
segid	map_segment();
void	sendall();
long	usr_unmap();
errstat	map_grow();
void	fast_flush_windows();

extern struct thread *curthread;

/* Indication from upper layers that a signal is pending for this thread */
extern long thread_sigpend;

#define UMAP( addr, modp ) \
			PTOV( umap( curthread, (vir_bytes) (addr), \
					(vir_bytes) sizeof( *addr ), modp ))
#define UMAP_BUF( addr, size, modp ) \
			PTOV( umap( curthread, (vir_bytes) (addr), \
					(vir_bytes) (size), modp ))

#define PROGERROR()	do { progerror(); } while ( 0 )

void sc_exitthread(ready)
long *ready;
{
    long *kready;

    if (ready == NULL) {
	kready = NULL;
    } else {
	kready = (long *) UMAP(ready, 1);
	/* note: the umap is allowed to fail (backward compatibility) */
    }
    exitthread(kready);
}

bufsize sc_getreq( hdr, buf, cnt )
header *hdr;
bufptr buf;
bufsize cnt;
{
    header *khdr;
    bufptr kbuf;

    DPRINTF(2, ( "sc_getreq( %x, %x, %x )\n", hdr, buf, cnt ));

    khdr = (header *) UMAP( hdr, 1 );
    if ( khdr == 0 ) {
	PROGERROR();
	return( (bufsize) RPC_BADADDRESS );
    }

    if ( cnt != 0 ) {
	kbuf = (bufptr) UMAP_BUF( buf, cnt, 1 );
    }
    /* kbuf is unused if cnt != 0  so we don't waste time setting it. */

#ifdef USE_AM6_RPC
    return( rpc_getreq( khdr, kbuf, cnt ));
#else
    return( getreq( khdr, kbuf, cnt ));
#endif
}

void sc_putrep( hdr, buf, cnt )
header *hdr;
bufptr buf;
bufsize cnt;
{
    header *khdr;
    bufptr kbuf;

    DPRINTF(2, ( "sc_putrep( %x, %x, %x )\n", hdr, buf, cnt ));

    khdr = (header *) UMAP( hdr, 0 );
    if ( khdr == 0 ) {
	PROGERROR();
	return;
    }
    if ( cnt != 0 ) {
	kbuf = (bufptr) UMAP_BUF( buf, cnt, 0 );
    }
    /* kbuf is unused if cnt != 0  so we don't waste time setting it. */

#ifdef USE_AM6_RPC
    rpc_putrep( khdr, kbuf, cnt );
#else
    putrep( khdr, kbuf, cnt );
#endif
}

bufsize sc_trans( hdrsend, bufsend, cntsend, hdrrecv, bufrecv, cntrecv )
header *hdrsend, *hdrrecv;
bufptr bufsend, bufrecv;
bufsize cntsend, cntrecv;
{
    header *khdrsend;
    header *khdrrecv;
    bufptr kbufs;
    bufptr kbufr;

    DPRINTF(2, ( "sc_trans( %x, %x, %x, %x, %x, %x )\n",
	   hdrsend, bufsend, cntsend, hdrrecv, bufrecv, cntrecv ));

    khdrsend = (header *) UMAP( hdrsend, 0 );
    khdrrecv = (header *) UMAP( hdrrecv, 1 );
    if ( khdrsend == 0 || khdrrecv == 0 ) {
	PROGERROR();
	return( (bufsize) RPC_BADADDRESS );
    }

    if ( cntsend != 0 ) {
	kbufs = (bufptr) UMAP_BUF( bufsend, cntsend, 0 );
    }
    /* kbufs is unused if cnt != 0  so we don't waste time setting it. */

    if ( cntrecv != 0 ) {
	kbufr = (bufptr) UMAP_BUF( bufrecv, cntrecv, 1 );
    }
    /* kbufr is unused if cnt != 0  so we don't waste time setting it. */
    return( trans( khdrsend, kbufs, cntsend, khdrrecv, kbufr, cntrecv ));
}

f_size_t sc_rpc_getreq( hdr, buf, cnt )
header  *hdr;
bufptr   buf;
f_size_t cnt;
{
    header *khdr;
    bufptr kbuf;

    DPRINTF(2, ( "sc_rpc_getreq( %x, %x, %lx )\n", hdr, buf, cnt ));

    khdr = (header *) UMAP( hdr, 1 );
    if ( khdr == 0 ) {
	PROGERROR();
	return( (bufsize) RPC_BADADDRESS );
    }

    if ( cnt != 0 ) {
	kbuf = (bufptr) UMAP_BUF( buf, cnt, 1 );
    }
    /* kbuf is unused if cnt != 0  so we don't waste time setting it. */

    return( rpc_getreq( khdr, kbuf, cnt ));
}

f_size_t sc_rpc_putrep( hdr, buf, cnt )
header  *hdr;
bufptr   buf;
f_size_t cnt;
{
    header *khdr;
    bufptr kbuf;

    DPRINTF(2, ( "sc_rpc_putrep( %x, %x, %lx )\n", hdr, buf, cnt ));

    khdr = (header *) UMAP( hdr, 0 );
    if ( khdr == 0 ) {
	PROGERROR();
	return (bufsize) RPC_BADADDRESS;
    }
    if ( cnt != 0 ) {
	kbuf = (bufptr) UMAP_BUF( buf, cnt, 0 );
    }
    /* kbuf is unused if cnt != 0  so we don't waste time setting it. */

    return rpc_putrep( khdr, kbuf, cnt );
}

f_size_t sc_rpc_trans( hdrsend, bufsend, cntsend, hdrrecv, bufrecv, cntrecv )
header  *hdrsend, *hdrrecv;
bufptr   bufsend, bufrecv;
f_size_t cntsend, cntrecv;
{
    header *khdrsend;
    header *khdrrecv;
    bufptr kbufs;
    bufptr kbufr;

    DPRINTF(2, ( "sc_rpc_trans( %x, %x, %x, %x, %x, %x )\n",
		hdrsend, bufsend, cntsend, hdrrecv, bufrecv, cntrecv ));

    khdrsend = (header *) UMAP( hdrsend, 0 );
    khdrrecv = (header *) UMAP( hdrrecv, 1 );
    if ( khdrsend == 0 || khdrrecv == 0 ) {
	PROGERROR();
	return( (bufsize) RPC_BADADDRESS );
    }

    if ( cntsend != 0 ) {
	kbufs = (bufptr) UMAP_BUF( bufsend, cntsend, 0 );
    }
    /* kbufs is unused if cnt != 0  so we don't waste time setting it. */

    if ( cntrecv != 0 ) {
	kbufr = (bufptr) UMAP_BUF( bufrecv, cntrecv, 1 );
    }
    /* kbufr is unused if cnt != 0  so we don't waste time setting it. */
    return( rpc_trans( khdrsend, kbufs, cntsend, khdrrecv, kbufr, cntrecv ));
}

interval
sc_timeout( maxloc )
interval maxloc;
{
    DPRINTF(2, ( "sc_timeout( %x )\n", maxloc ));

    return( timeout( maxloc ));
}

void
sc_threadswitch() {
    DPRINTF(2, ( "sc_threadswitch()\n" ));

    threadswitch();
}
     
#ifdef USE_AM6_RPC
extern void rpc_cleanup();
#else
extern void cleanup();
#endif

void
sc_cleanup() {
    DPRINTF(2, ( "sc_cleanup()\n" ));
#ifdef USE_AM6_RPC
    rpc_cleanup();
#else
    cleanup();
#endif
}

sc_sys_newthread( pc, sp, local )
vir_bytes	pc;
vir_bytes	sp;
long		local;
{
    DPRINTF(2, ( "sc_sys_newthread( %x, %x, %x )\n", pc, sp, local ));

    return( newuthread( pc, sp, local ));
}

long sc_getinfo( ccp, cdp, cdplen )
capability *ccp;
process_d *cdp;
int cdplen;
{
    capability *kccp;
    process_d *kcdp;

    DPRINTF(2, ( "sc_getinfo( %x, %x, %x )\n", ccp, cdp, cdplen ));

    if ( ccp == 0 ) kccp = 0;
    else {
	kccp = (capability *) UMAP( ccp, 1 );
	if ( kccp == 0 ) {
	    PROGERROR();
	    return( -1 );
	}
    }

    if ( cdp == 0 ) kcdp = 0;
    else {
	kcdp = (process_d *) PTOV( umap( curthread,
				    (vir_bytes) cdp, (vir_bytes) cdplen, 1 ));
	if ( kcdp == 0 ) {
	    PROGERROR();
	    return( -1 );
	}
    }

    return( getinfo( kccp, kcdp, cdplen ));
}

void sc_exitprocess( status )
long status;
{
    DPRINTF(2, ( "sc_exitprocess( %x )\n", status ));

    fast_flush_windows();
    exitprocess( status );
}

sc_sys_setvec( vec, len )
vir_bytes vec;
vir_bytes len;
{
    DPRINTF(2, ( "sc_sys_setvec( %x, %x )\n", vec, len ));

    sigcatch( vec, len );
}

void sc_sig_raise( signo )
signum signo;
{
    DPRINTF(2, ( "sc_sig_raise( %x )\n", signo ));

    fast_flush_windows();
    sendall((struct process *) 0, signo, 1 );
}

extern struct stack_frame *sparc_getfp _ARGS(( void ));

#ifdef USER_INTERRUPTS
#include "fast_copy.h"
#endif

/*
 * Put your analyst on danger money before you try figuring this out!
 * The idea is that we copy the saved fault frame from callcatcher (which
 * framep should point to) to the current fault-frame on the kernel stack
 * and then return through it to the user process.  Since sparc_syscall()
 * fiddles with return values we have to return the value in a way that
 * sparc_syscall's fiddling is a nop.
 */
unsigned int sc_sys_sigret( framep )
thread_ustate * framep;
{
    thread_ustate * kfp;
    thread_ustate * sigret_frame;

    fast_flush_windows();
    kfp = (thread_ustate *) sparc_getfp();

    DPRINTF(2, ( "sc_sigret( %x )\n", framep ));
    sigret_frame = (thread_ustate *) UMAP_BUF( framep, sizeof (thread_ustate), 1 );
    if (sigret_frame == 0) {
	DPRINTF(2, ("sc_sys_sigret: 1st umap failed\n"));
	progerror();
	return 0;
    }
#ifdef USER_INTERRUPTS
    fast_copy((short *) sigret_frame, (short *) kfp, sizeof(thread_ustate) / 2);
#else
    *kfp = *sigret_frame;
#endif
    /* Restore the correct return value of the interrupted routine */
    return kfp->tu_in[0];
}


segid sc_seg_map( newcap, addr, len, type )
capability *newcap;
vir_bytes addr;
vir_bytes len;
long type;
{
    capability *knewcap;
    segid result;

    DPRINTF(2, ( "sc_seg_map( %x, %x, %x, %x )", newcap, addr, len, type ));

    if ( newcap == 0 ) knewcap = 0;
    else {
	knewcap = (capability *) UMAP( newcap, 1 );
	if ( knewcap == 0 ) {
	    PROGERROR();
	    return( 0 );
	}
    }

    result = map_segment((struct process *) 0, addr,
					len, type, knewcap, (long) 0 );
    DPRINTF(2, ( " = %d\n", result ));
    return result;
}

errstat sc_seg_unmap( mapid, newcap )
segid mapid;
capability *newcap;
{
    errstat i;
    capability *knewcap;

    DPRINTF(2, ( "sc_seg_unmap( %x, %x )\n", mapid, newcap ));

    if ( newcap == 0 )
	knewcap = 0;
    else {
	knewcap = (capability *) UMAP( newcap, 1 );
	if ( knewcap == 0 ) {
	    PROGERROR();
	    return( STD_ARGBAD );
	}
    }

    i = usr_unmap((struct process *) 0, mapid, knewcap );
    if ( i == STD_COMBAD ) PROGERROR();

    return( i );
}

errstat sc_seg_grow( mapid, size )
segid mapid;
long size;
{
    errstat i;

    DPRINTF(2, ( "sc_seg_grow( %x, %x )\n", mapid, size ));

    i = map_grow( mapid, size );
    if ( i == STD_COMBAD ) PROGERROR();

    return( i );
}

int sc_mu_trylock( userlock, timeout )
mutex *userlock;
interval timeout;
{
    mutex *klock;

    BEGIN_MEASURE(mu_trylock);
    DPRINTF( 2, ( "sc_mu_trylock( %x, %x ) thread %d pid %d\n", userlock,
	    timeout, curthread->tk_slotno, PROCSLOT(curthread->mx_process) ));

    klock = (mutex *) UMAP( userlock, 1 );
    if ( klock == 0 ) {
	PROGERROR();
	return( STD_ARGBAD );
    }

#ifdef MEASURE
    {
	register int retval = mu_trylock( klock, timeout );

	END_MEASURE(mu_gotlock);
	return retval;
    }
#else
    return( mu_trylock( klock, timeout ));
#endif
}

void sc_mu_unlock( userlock )
mutex *userlock;
{
    mutex *klock;

    BEGIN_MEASURE(mu_unlock);
    DPRINTF( 2, ( "sc_mu_unlock( %x ) thread %d pid = %d\n",
	userlock, curthread->tk_slotno, PROCSLOT(curthread->mx_process) ));

    klock = (mutex *) UMAP( userlock, 1 );
    if ( klock == 0 )
	PROGERROR();
    else
	mu_unlock( klock );
}

long sc_sys_getlocal() {
    DPRINTF(2, ( "sc_getlocal() thread %d\n", curthread->tk_slotno ));
    return( getlocal() );
}

long sc_sys_milli() {
    DPRINTF(2, ( "sc_getmilli()\n" ));
    return( getmilli() );
}

void sc_sys_null() {
    DPRINTF(2, ( "sc_null()\n" ));
}

long sc_rawflip(userarg)
struct rawflip_parms *userarg;
{
    struct rawflip_parms *arg;

    DPRINTF(2, ( "sc_rawflip(%x)\n", userarg ));
    arg = (struct rawflip_parms *) UMAP( userarg, 1 );
    if ( arg == 0 ) {
	PROGERROR();
	return STD_ARGBAD;
    } else {
	return do_rawflip(arg);
    }
}

#ifdef PROFILE

errstat sc_profile(userarg)
struct profile_parms *userarg;
{
    extern errstat user_profile();
    struct profile_parms *arg;

    DPRINTF(2, ( "sc_profile(%x)\n", userarg ));
    arg = (struct profile_parms *) UMAP( userarg, 1 );
    if ( arg == 0 ) {
	PROGERROR();
	return STD_ARGBAD;
    } else {
	return user_profile(arg);
    }
}

#endif /* PROFILE */

#ifdef FLIPGRP

g_id_t
sc_grp_create(p, resilience, maxgroup, nbuf, maxmess)
port *p;
g_indx_t resilience;
g_indx_t maxgroup;
uint32 nbuf;
uint32 maxmess;
{
    port *kport = (port *) UMAP(p, 0);

    DPRINTF(2, ( "sc_grp_create( %x, %x, %x, %x, %x )\n",
			p, resilience, maxgroup, nbuf, maxmess ));

    if (kport == (port *) 0) {
	PROGERROR();
	return 0;
    }
    return grp_create(kport, resilience, maxgroup, nbuf, maxmess);
}

errstat
sc_grp_forward(gid, p, memid)
g_id_t gid;
port *p;
g_indx_t memid;
{
    port *kport = (port *) UMAP(p, 0);

    DPRINTF(2, ( "sc_grp_forward( %x, %x, %x )\n", gid, p, memid ));

    if (kport == (port *) 0) {
	PROGERROR();
	return 0;
    }
    return grp_forward(gid, kport, memid);
}

errstat
sc_grp_info(gid, p, state, memlist, size)
g_id_t gid;
port *p;
grpstate_p state;
g_indx_t *memlist;
g_indx_t size;
{
    port *kport = (port *) UMAP(p, 0);
    grpstate_p kstate = (grpstate_p) UMAP_BUF(state, sizeof (grpstate_t), 1);
    g_indx_t * kmemlist;

    DPRINTF(2, ( "sc_grp_info( %x, %x, %x, %x, %x )\n",
			gid, p, state, memlist, size ));

    if (kport == (port *) 0 || kstate == (grpstate_p) 0) {
	PROGERROR();
	return 0;
    }
    kmemlist = (g_indx_t *) UMAP_BUF(memlist, sizeof (g_indx_t) * size, 1);
    return grp_info(gid, kport, kstate, kmemlist, size);
}

g_id_t
sc_grp_join(hdr)
header_p hdr;
{
    header_p khdr = (header_p) UMAP(hdr, 0);

    DPRINTF(2, ( "sc_grp_join( %x )\n", hdr ));

    if (khdr == (header_p) 0) {
	PROGERROR();
	return 0;
    }
    return grp_join(khdr);
}

errstat
sc_grp_leave(gid, hdr)
g_id_t gid;
header_p hdr;
{
    header_p khdr = (header_p) UMAP(hdr, 0);

    DPRINTF(2, ( "sc_grp_leave( %x, %x )\n", gid, hdr ));

    if (khdr == (header_p) 0) {
	PROGERROR();
	return 0;
    }
    return grp_leave(gid, khdr);
}

int32
sc_grp_receive(gid, hdr, buf, cnt, more)
g_id_t gid;
header_p hdr;
bufptr buf;
uint32 cnt;
int *more;
{
    header_p khdr = (header_p) UMAP(hdr, 1);
    bufptr kbuf;
    int * kmore;

    DPRINTF(2, ( "sc_grp_receive( %x, %x, %x, %x, %x )\n",
			gid, hdr, buf, cnt, more ));

    if (khdr == (header_p) 0) {
	PROGERROR();
	return 0;
    }
    kmore = (int *) UMAP(more, 1);
    if (more != 0 && kmore == 0)
	return STD_ARGBAD;
    if (cnt == 0)
	kbuf = 0;
    else
        kbuf = (bufptr) UMAP_BUF(buf, cnt, 1);
    return grp_receive(gid, khdr, kbuf, cnt, kmore);
}

g_indx_t
sc_grp_reset(gid, hdr, n)
g_id_t gid;
header_p hdr;
g_indx_t n;
{
    header_p khdr = (header_p) UMAP(hdr, 0);

    DPRINTF(2, ( "sc_grp_reset( %x, %x, %x )\n", gid, hdr, n ));

    if (khdr == (header_p) 0) {
	PROGERROR();
	return 0;
    }
    return grp_reset(gid, khdr, n);
}

errstat
sc_grp_send(gid, hdr, buf, cnt)
g_id_t gid;
header_p hdr;
bufptr buf;
uint32 cnt;
{
    header_p khdr = (header_p) UMAP(hdr, 0);
    bufptr kbuf;

    DPRINTF(2, ( "sc_grp_send( %x, %x, %x, %x )\n",
			gid, hdr, buf, cnt ));
    if (khdr == (header_p) 0) {
	PROGERROR();
	return 0;
    }
    if (cnt == 0)
	kbuf = 0;
    else
        kbuf = (bufptr) UMAP_BUF(buf, cnt, 0);
    return grp_send(gid, khdr, kbuf, cnt);
}

errstat
sc_grp_set(gid, p, sync, reply, alive, large)
g_id_t gid;
port *p;
interval sync;
interval reply;
interval alive;
uint32 large;
{
    port *kport = (port *) UMAP(p, 0);

    DPRINTF(2, ( "sc_grp_set( %x, %x, %x, %x, %x, %x )\n",
			gid, p, sync, reply, alive, large ));
    if (kport == (port *) 0) {
	PROGERROR();
	return 0;
    }
    return grp_set(gid, kport, sync, reply, alive, large);
}

#endif /* FLIPGRP */

int
sc_thread_set_priority(new, old)
long    new;
long *  old;
{
    long *kold = (long *) UMAP(old, 1);

    DPRINTF(2, ( "sc_thread_set_priority( %x, %x )\n", new, old));
    if (kold == (long *) 0) {
        PROGERROR();
        return -1;
    }
    return thread_set_priority(new, kold);
}

int
sc_thread_get_max_prio(max)
long * max;
{
    long *kmax = (long *) UMAP(max, 1);

    DPRINTF(2, ( "sc_thread_get_max_priority( %x )\n", max));
    if (kmax == (long *) 0) {
        PROGERROR();
        return -1;
    }
    return thread_get_max_priority(kmax);
}

int
sc_thread_en_preempt(ptr)
long **ptr;
{
    DPRINTF(2, ( "sc_thread_enable_preemption()\n"));
    return thread_enable_preemption(ptr);
}

#ifdef ACTMSG
int
sc_am_register(flags, portp, hcnt, hvec, lock)
    int flags;
    port *portp;
    int hcnt;
    void (*hvec[])();
    mutex *lock;
{
    port *p = (port *) UMAP(portp, 0);
    mutex *l = NULL;
    
    if (lock && (l = (mutex *) UMAP(lock, 1)) == (mutex *)0) {
        PROGERROR();
        return -1;
    }
    if (p == (port *)0 || hvec == (void (**)())0) {
        PROGERROR();
        return -1;
    }
    return am_register(flags, p, hcnt, hvec, l);
}

int
sc_am_unregister(portp)
    port *portp;
{
    port *p = (port *) UMAP(portp, 0);

    if (p == (port *)0) {
        PROGERROR();
        return -1;
    }
    return am_unregister(p);
}

int
sc_am_send(portp, method, data, size)
    port *portp;
    int method;
    bufptr data;
    int size;
{
    port *p = (port *) UMAP(portp, 0);
    bufptr d = NULL;
    
    if (p == (port *)0) {
        PROGERROR();
        return -1;
    }
    if (size > 0 && !(d = (bufptr) UMAP_BUF(data, size, 0))) {
        PROGERROR();
        return -1;
    }
    return am_send(p, method, d, size);
}
#endif /* ACTMSG */

#ifdef USER_INTERRUPTS

#define MAX_INTR_DEVICES	2
#define DEV_INTR_MYRINET	0
#define DEV_INTR_FASTETHER	1

#ifdef AWAITING_USER_INTERRUPTS
static int dev_intr[MAX_INTR_DEVICES];

void
dev_interrupted(device)
unsigned int device;
{
    if (device >= MAX_INTR_DEVICES) {
	panic("interrupt from unexpected device %d\n", device);
    }
    if (dev_intr[device] == -1) {
	/* User thread is waiting for it */
	dev_intr[device] = 0;
	wakeup((event) &dev_intr[device]);
    } else {
	/* Just register the fact that we got an interrupt */
	dev_intr[device]++;
    }
}

int
sc_await_intr(device, maxwait)
unsigned int device;
interval maxwait;
{
    /* Block user thread until the device specified generates an interrupt.
     * This mechanism can be used to avoid most of the user-level polling
     * overhead for memory-mapped network devices.
     */
    if (device >= MAX_INTR_DEVICES) {
	return STD_ARGBAD;
    }

    if (dev_intr[device] > 0) {
	dev_intr[device] = 0;
	return STD_OK;
    }

    /* Put -1 in the counter to announce that we're waiting */
    dev_intr[device] = -1;

    if (await((event) &dev_intr[device], maxwait) >= 0) {
	/* Got interrupt */
	return STD_OK;
    } else {
	/* Process was signalled in another way */
	dev_intr[device] = 0;
	return STD_INTR;
    }
}
#else /* !AWAITING_USER_INTERRUPTS */


/*
 * Otherwise we have interface mapping device interrupts directly
 * to user signals.  We still have to figure out what is the most
 * useful and efficient interface.
 */

/* We only support one user thread per device right now. */
static struct {
	struct thread  *aw_thread;
	signum		aw_signal;
	sig_vector      aw_vec;
} awaiting_signal[MAX_INTR_DEVICES];

#define TDS_AWAIT_SIG	0x8000
#define SIG_FLAGS	(TDS_DEAD|TDS_EXC|TDS_INT|TDS_SRET|TDS_LSIG|TDS_STUN|TDS_SHAND|TDS_DIE|TDS_USIG)

int
fast_findcatcher(t, sig, vec)
struct thread *t;
signum sig;
sig_vector *vec;
{
    /* avoid findcatcher/umap overhead in the case of device interrupts */
    register int dev;

    for (dev = 0; dev < MAX_INTR_DEVICES; dev++) {
	if (awaiting_signal[dev].aw_thread == t &&
	    awaiting_signal[dev].aw_signal == sig)
	{
	    *vec = awaiting_signal[dev].aw_vec;
	    return 1;
	}
    }

    return 0;
}

void
dev_interrupted(device)
unsigned int device;
{
    struct thread *t;

    if ((t = awaiting_signal[device].aw_thread) != 0) {
	if (((t->tk_mpx.MX_flags & TDS_AWAIT_SIG) != 0) &&
	    ((t->tk_mpx.MX_flags & SIG_FLAGS) == 0))
	{
	    /* printf("DEVINTR pid %d\n", getpid(t)); */
	    END_MEASURE(sig_gotintr);
	    BEGIN_MEASURE(sig_put);
	    fast_putsig(t, awaiting_signal[device].aw_signal);
	    END_MEASURE(sig_put);
	    BEGIN_MEASURE(sig_call);
	}
    }
}

int
sc_await_intr(device, signo)
unsigned int device;
signum signo;
{
    sig_vector vec;

    if (device >= MAX_INTR_DEVICES) {
	return STD_ARGBAD;
    }

    if (!findcatcher(curthread, signo, &vec)) {
	return STD_ARGBAD;
    }

    awaiting_signal[device].aw_thread = curthread;
    awaiting_signal[device].aw_signal = signo;
    awaiting_signal[device].aw_vec    = vec;

    curthread->tk_mpx.MX_flags |= TDS_AWAIT_SIG;

    printf("Installed device interrupt signal handler for pid %d\n",
	   getpid(curthread));
    
    return STD_OK;
}

#endif /* !AWAITING_USER_INTERRUPTS */

#endif /* USER_INTERRUPTS */

#endif /* NOPROC */
