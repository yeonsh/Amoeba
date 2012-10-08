/*	@(#)sessvr.h	1.4	96/02/27 10:28:28 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __SESSVR_H__
#define __SESSVR_H__

#include <_ARGS.h>

/* Inherited classes: */
#include "checkpoint.h"
#include "stdinfo.h"
#include "stddestroy.h"
#include "tti.h"
#include "proswapproc.h"
#include "stdstatus.h"

/* Constants: */
#define R_FORK 99
#define MAXNCAPS 20
#define MAXNPROC 300

struct proctab {
    short int pid, ppid, pgrp;
    char state[10];
    capability proc;
};

/* Operators: */
errstat ses_init _ARGS((capability *_ses, 
    /* var */	capability *_self, 
    /* out */	capability *_handicap));

errstat ses_execbad _ARGS((capability *_ses));

errstat ses_execok _ARGS((capability *_ses, 
    /* var */	capability *_newprocess));

errstat ses_parent _ARGS((capability *_ses, 
    /* out */	int *_cpid));

errstat ses_child _ARGS((capability *_ses, 
    /* var */	capability *_sigsvrcap));

errstat ses_waitpid _ARGS((capability *_ses, 
    /* inout */	int *_pid, 
    /* out */	int *_cause, 
    /* out */	long int *_detail, 
    /* var */	capability *_callback));

errstat ses_kill _ARGS((capability *_ses, 
    /* in */	int _pid, 
    /* in */	int _sig));

errstat ses_getppid _ARGS((capability *_ses, 
    /* out */	int *_ppid));

errstat ses_getpgrp _ARGS((capability *_ses, 
    /* in */	int _pid, 
    /* out */	int *_pgrp));

errstat ses_setpgrp _ARGS((capability *_ses, 
    /* in */	int _pid, 
    /* in */	int _pgrp));

errstat ses_pipe _ARGS((capability *_ses, 
    /* out */	capability *_read_end, 
    /* out */	capability *_write_end));

errstat ses_getpt _ARGS((capability *_ses, 
    /* out */	struct proctab _pt[300], 
    /* inout */	int *_nproc));

errstat ses_share _ARGS((capability *_ses, 
    /* in */	capability _sharedcaps[20], 
    /* in */	int _nsharedcaps));

errstat ses_unshare _ARGS((capability *_ses, 
    /* in */	capability _sharedcaps[20], 
    /* in */	int _nsharedcaps));

errstat ses_file _ARGS((capability *_ses, 
    /* in */	long int _flags, 
    /* in */	long int _pos, 
    /* in */	long int _size, 
    /* in */	int _mode, 
    /* in */	long int _mtime, 
    /* var */	capability *_cap, 
    /* var */	capability *_dircap, 
    /* in */	char _name[256], 
    /* out */	capability *_cap_ret));

errstat ses_seek _ARGS((capability *_ses, 
    /* var */	capability *_filecap, 
    /* in */	long int _offset, 
    /* in */	int _mode, 
    /* out */	long int *_new_off));

errstat ses_size _ARGS((capability *_ses, 
    /* var */	capability *_filecap, 
    /* out */	long int *_size));

errstat ses_runsvr _ARGS((capability *_ses, 
    /* var */	capability *_runsvr_cap, 
    /* var */	capability *_pooldir_cap));

errstat ses_preexec _ARGS((capability *_ses, 
    /* in */	long int _sigignore, 
    /* in */	long int _sigblock, 
    /* in */	long int _sigpending));

errstat ses_sigaware _ARGS((capability *_ses, 
    /* var */	capability *_sigsvrcap, 
    /* out */	long int *_sigignore, 
    /* out */	long int *_sigblock, 
    /* out */	long int *_sigpending));

errstat ses_newchild _ARGS((capability *_ses, 
    /* in */	long int _sigignore, 
    /* in */	long int _sigblock, 
    /* in */	long int _sigpending, 
    /* out */	capability *_newcap, 
    /* in */	capability _sharedcaps[20], 
    /* in */	int _nsharedcaps));

errstat ses_alarm _ARGS((capability *_ses, 
    /* inout */	int *_sec));

errstat ses_getid _ARGS((capability *_ses, 
    /* out */	int *_uid, 
    /* out */	int *_euid, 
    /* out */	int *_gid, 
    /* out */	int *_egid));

/*
 * Old interfaces:
 */
errstat ses_old_preexec _ARGS((capability *_ses, 
    /* in */	long int _sigignore));

errstat ses_old_sigaware _ARGS((capability *_ses, 
    /* var */	capability *_sigsvrcap, 
    /* out */	long int *_sigignore));

errstat ses_old_newchild _ARGS((capability *_ses, 
    /* in */	long int _sigignore, 
    /* out */	capability *_newcap, 
    /* in */	capability _sharedcaps[20], 
    /* in */	int _nsharedcaps));

#endif /* __SESSVR_H__ */
