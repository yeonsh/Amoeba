/*	@(#)syscall.h	1.7	96/02/27 10:26:38 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __SYSCALL_H__
#define __SYSCALL_H__

/*
** Definition of Amoeba system calls.
** Used in trap.c and to generate user level stubs.
*/

#define SC_getreq		0
#define SC_putrep		1
#define SC_trans		2
#define SC_timeout		3
#define SC_exitthread		4
#define SC_sys_cleanup		14
#define SC_getinfo		15
#define SC_exitprocess		16
#define SC_threadswitch		17
#define SC_sysseg_map		24
#define SC_sysseg_unmap		25
#define SC_sysseg_grow		26
#define SC_mu_trylock		27
#define SC_sys_mu_trylock	27	/* An alias for sparc & 386 */
#define SC_mu_unlock		28
#define SC_sys_mu_unlock	28	/* An alias for sparc & 386 */
#define SC_sys_setvec		29
#define SC_sig_raise		30
#define SC_sys_sigret		31
#define SC_sys_newthread	32
#define SC_sys_getlocal		33
#define SC_sys_milli		34

#define SC_grp_create		35
#define SC_grp_join		36
#define SC_grp_leave		37
#define SC_grp_send		38
#define SC_grp_receive		39
#define SC_grp_reset		40
#define SC_grp_info		41
#define SC_grp_set		42
#define SC_grp_forward		43

#define	SC_sys_null		44

#define	SC_thread_get_max_prio	45
#define	SC_thread_set_priority	46
#define	SC_thread_en_preempt	47

#define	SC_rpc_getreq		48
#define	SC_rpc_putrep		49
#define	SC_rpc_trans		50

#define SC_rawflip		51
#define SC_sys_profil		52

#define SC_await_intr		56

#endif /* __SYSCALL_H__ */
