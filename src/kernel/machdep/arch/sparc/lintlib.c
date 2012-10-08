/*	@(#)lintlib.c	1.3	96/02/27 13:46:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Lint library for assembly code routines
 *
 * Author: Gregory J. Sharp, 2 Sept 1991
 */

#include "amoeba.h"
#include "machdep.h"
#include "fault.h"
#include "exception.h"
#include "process/proc.h"
#include "seg.h"
#include "group.h"
#include "sys/proto.h"

/* From as.S */
/*************/
void flush_windows() {}
void fast_flush_windows() {}
void trap_exitthread() {}
void sparc_syscall() {}
void syscall_test() {}

int trap_mapping[1], trap_count[1];

/*ARGSUSED*/
void start(tu)
thread_ustate * tu;
{
    clearbss();
}

void
waitint() {}

void
enable()
{
}

void
disable()
{
}

int
sparc_disable()
{
    return 0;
}

/*ARGSUSED*/
int
setints(val)
int val;
{
}

struct stack_frame *
sparc_getframep()
{
    return (struct stack_frame *) 0;
}

char *
sparc_gettbr()
{
    return (char *) 0;
}

/*
 *The System Calls need to be used in C since they are otherwise
 * only used in assembler.  We let poor old sparc_settbr do this.
 */

/*ARGSUSED*/
void
sparc_settbr(tbr)
struct trap_vector *tbr;
{
    bufsize	sc_getreq();
    void	sc_putrep();
    bufsize	sc_trans();
    interval	sc_timeout();
    void	sc_threadswitch();
    void	sc_cleanup();
    int		sc_sys_newthread();
    long	sc_getinfo();
    void	sc_exitprocess();
    int		sc_sys_setvec();
    unsigned	sc_sys_sigret();
    void	sc_sys_null();
    void	sc_sig_raise();
    segid	sc_seg_map();
    errstat	sc_seg_unmap();
    errstat	sc_seg_grow();
    int		sc_mu_trylock();
    void	sc_mu_unlock();
    long	sc_sys_getlocal();
    long	sc_sys_milli();
#ifdef FLIPGRP
    g_id_t	sc_grp_create();
    errstat	sc_grp_forward();
    errstat	sc_grp_info();
    g_id_t	sc_grp_join();
    errstat	sc_grp_leave();
    int32	sc_grp_receive();
    g_indx_t	sc_grp_reset();
    errstat	sc_grp_send();
    errstat	sc_grp_set();
#endif /* FLIPGRP */
    void	trp_stkoverflow();

    header	hdr;
    port	p;
    mutex	mu;
    segid	sid;

    trp_stkoverflow((thread_ustate *) 0);
    (void) sc_sys_sigret((thread_ustate *) 0);
    sc_sys_null();
    (void) sc_getreq(&hdr, NILBUF, (bufsize) 0);
    sc_putrep(&hdr, NILBUF, (bufsize) 0);
    (void) sc_trans(&hdr, NILBUF, (bufsize) 0, &hdr, NILBUF, (bufsize) 0);
    (void) sc_timeout((interval) 0);
    sc_threadswitch();
    sc_cleanup();
    (void) sc_sys_newthread((vir_bytes) 0, (vir_bytes) 0, (long) 0);
    (void) sc_getinfo((capability *) 0, (process_d *) 0, 0);
    sc_exitprocess((long) 0);
    sc_sys_setvec((vir_bytes) 0, (vir_bytes) 0);
    sc_sig_raise((signum) 0);
    sid = sc_seg_map((capability *) 0, (vir_bytes) 0, (vir_bytes) 0, (long) 0);
    (void) sc_seg_unmap(sid, (capability *) 0);
    (void) sc_seg_grow(sid, (long) 0);
    (void) sc_mu_trylock(&mu, (interval) 0);
    sc_mu_unlock(&mu);
    (void) sc_sys_getlocal();
    (void) sc_sys_milli();
#ifdef FLIPGRP
    (void) sc_grp_create(&p, (g_indx_t) 0, (g_indx_t) 0, (uint32) 0,
			(uint32) 0);
    (void) sc_grp_forward((g_id_t) 0, &p, (g_indx_t) 0);
    (void) sc_grp_info((g_id_t) 0, &p, (grpstate_p) 0, (g_indx_t *) 0,
			(g_indx_t) 0);
    (void) sc_grp_join(&hdr);
    (void) sc_grp_leave((g_id_t) 0, &hdr);
    (void) sc_grp_receive((g_id_t) 0, &hdr, NILBUF, (uint32) 0, (int *) 0);
    (void) sc_grp_reset((g_id_t) 0, &hdr, (g_indx_t) 0);
    (void) sc_grp_send((g_id_t) 0, &hdr, NILBUF, (uint32) 0);
    (void) sc_grp_set((g_id_t) 0, &p, (interval) 0, (interval) 0,
			(interval) 0, (uint32) 0);
#endif /* FLIPGRP */
}

/* Some variables declared in assembler */
thread_ustate *kern_stack;
struct trap_vector trap_table[1];
struct trap_vector trap_template;
struct trap_vector trap_overflow_template;
struct trap_vector trap_underflow_template;
