/*	@(#)proto.h	1.9	96/02/27 10:39:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * proto.h
 *
 * Created July 3, 1991 by Philip Homburg
 *
 * prototypes for kernel functions.
*/

#ifndef __SYS__PROTO_H__
#define __SYS__PROTO_H__

#include <amoeba.h>
#include <exception.h>
#include <machdep.h>
#include <global.h>
#include <fault.h>
#include <kthread.h>
#include <group.h>
#include <_ARGS.h>
#include <sys/flip/packet.h>
#include <sys/flip/flip.h>
#include <module/strmisc.h>  /* for bprintf */

struct thread;
struct stframe;	/* should be removed */

void abort _ARGS(( void ));

/* from the runtime support code */
void disable _ARGS(( void ));
void enable _ARGS(( void ));
void waitint _ARGS(( void ));

/* kernel/sys/assert.c */
extern /* void */ panic _ARGS(( char *fmt, ... ));

/* kernel/sys/main.c */
void helloworld _ARGS(( void ));
void exitkernel _ARGS(( void ));

/* kernel/sys/mpx.c */
void checkints _ARGS(( void ));
void getsig _ARGS(( void (*vec)(int thread_no, signum sig), int id ));
void putsig _ARGS(( struct thread *t, signum sig ));
void NewKernelThread _ARGS(( void (*fp)(void), vir_bytes stksiz ));
interval await _ARGS(( event ev, interval tout ));
void wakeup _ARGS(( event ev ));
void enqueue _ARGS(( void (*rout) (long param), long param ));
void exitthread _ARGS(( long *ready ));
phys_bytes umap _ARGS(( struct thread *t, vir_bytes vir, vir_bytes len, 
							int might_modify ));
void progerror _ARGS(( void ));
void scheduler _ARGS(( void ));

/* kernel/sys/seg.c */
char * aalloc _ARGS((vir_bytes size, int align));
vir_bytes getblk _ARGS(( vir_bytes size ));
void relblk _ARGS(( vir_bytes addr ));
vir_bytes mgetblk _ARGS(( vir_bytes size ));
int HardwareSeg _ARGS((capability *, phys_bytes, phys_bytes, int));

/* kernel/sys/sweeper.c */
unsigned long getmilli _ARGS(( void ));
void sweeper_run _ARGS(( long ms_passed ));
void sweeper_set _ARGS(( void (*sweep)(long), long arg, interval ival, int once ));

/* kernel/server/direct.c */
errstat dirappend _ARGS(( char *name, capability *cap ));
errstat dirnappend _ARGS(( char *name, int n, capability *cap ));
errstat dirchmod _ARGS(( char *name, int ncols, long cols[] ));

/* kernel/machdep/arch/$ARCH/trap.c */
void stacktrace _ARGS(( void ));
int probe _ARGS(( vir_bytes addr, int width ));
int wprobe _ARGS(( vir_bytes addr, int width ));
int callcatcher _ARGS(( long trapno, sig_vector *sv, thread_ustate *us ));

/* kernel/machdep/dev/$MACH/<timer> */
#if PROFILE
void start_prof_tim _ARGS(( long interv ));
void stop_prof_tim _ARGS(( void ));
#endif

/* kernel/machdep/mmu/$MMU/<mm> */
int mmrerun _ARGS(( char *addr, int fc ));

/* kernel/protocol/flip/rpc/flrpc.c */
long rpc_getreq _ARGS(( header *hdr, bufptr buf, f_size_t cnt ));
long rpc_putrep _ARGS(( header *hdr, bufptr buf, f_size_t cnt ));
long rpc_trans _ARGS(( header *hdr1, bufptr buf1, f_size_t cnt1,
			header *hdr2, bufptr buf2, f_size_t cnt2 ));
void rpc_cleanup _ARGS(( void ));
void rpc_stoprpc _ARGS(( thread_t * ));
errstat grp_forward _ARGS(( g_id_t gd, port *p, g_indx_t memid ));
void rpc_initstate _ARGS(( thread_t * ));
void rpc_cleanstate _ARGS(( thread_t * ));
int rpc_sendsig _ARGS(( thread_t * ));

/* kernel/server/process/exception.c */
int usertrap _ARGS(( long type, thread_ustate *addr ));

#endif /* __SYS__PROTO_H__ */
