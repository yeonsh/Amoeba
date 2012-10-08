/*	@(#)sessvr.cls	1.7	96/02/27 10:28:21 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Session server interface */

#include <amoeba.h>
#include <cmdreg.h>
#include <posix/limits.h>
#include "std.cls"
#include "checkpoint.cls"
#include "tti.cls"
#include "proswappro.cls"
#include "stdrights.cls"

/* We only go to SES_LAST_COM - 10 since the pipeserver needs some cmd codes */
class sessvr [SES_FIRST_COM .. SES_LAST_COM - 10] {
	
	/* The session server is used by the Posix emulation library to
	   implement everything that has to do with process ids: fork,
	   waitpid, getpid, getppid, kill.
	   Some requests are implemented as signals: R_FORK causes a fork
	   (after which the parent must call ses_getchpid); R_JUNK
	   says that a new Amoeba process was successfully execed in the
	   place of the caller, and that the caller must be disposed of
	   silently. */
	
	inherit checkpoint, stdinfo, stddestroy, tti,
		proswapproc, stdstatus, stdrestrict, process_rights;
	
	/* INITIALIZATION */
	
	/* Published capability of session server */
	
	ses_init(*,
		var in	capability self,
		out	capability handicap)	rights PSR_EXEC;
	/* New process wants to initialize; specifies its own process
	   cap, returns handler capability (containing the pid).
	   Must be followed by the following signal. */
	
	/* EXECVE SYSTEM CALL */
	
	/* Placeholder for binary backward compatibility; new version below. */
	ses_old_preexec(*,
		in	long sigignore)		rights PSR_EXEC;
	/* Process starts exec'ing a new program with given signal
	   ignore mask.
	   Must be followed by either of the following two. */
	
	ses_execbad(*)				rights PSR_EXEC;
	/* Exec failed; revert to previous state */
	
	ses_execok(*,
		var in	capability newprocess)	rights PSR_EXEC;
	/* Exec succeeded; newprocess specifies new process cap.
	   Hereafter the caller must send itself an R_JUNK signal,
	   upon receipt of which it is silently discarded. */
	
	/* FORK SYSTEM CALL */
	
	const R_FORK = 99;
	/* Process wants to fork a child.
	   Must be followed by the following two. */

	ses_parent(*,
		out	int cpid)		rights PSR_EXEC;
	/* Parent after fork; return the pid of the child just forked */
	
	ses_child(*,
		var in	capability sigsvrcap)	rights PSR_EXEC;
	/* Child after fork; specify new signal server thread capability */
	
	/* SIGNAL SYSTEM CALL */
	
	/* Placeholder for binary backward compatibility; new version below. */
	ses_old_sigaware(*,
		var in	capability sigsvrcap,
		out	long sigignore)		rights PSR_EXEC;
	/* Program becomes signal-aware; sigsvrcap specifies the signal
	   server thread cap, sigignore returns the signal ignore mask */
	
	/* OTHER SYSTEM CALLS */
	
	ses_waitpid(*,
		in out	int pid,
		out	int cause,
		out	long detail,
		var in	capability callback)	rights PSR_EXEC;
	/* Get status on a terminated child.
	   Input value of pid specifies which child status is requested,
	   in the same manner as for kill below.
	   If there are no children, pid is set to -1.
	   If no status is immediately available, pid is set to 0,
	   and if the callback capability has a non-null port,
	   a checkpoint (without process descriptor) will be sent to the
	   callback capability when a child dies.
	   There will be at most one callback per call to ses_waitpid. */
	
	ses_kill(*,
		in	int pid,
		in	int sig)		rights PSR_KILL;
	/* Emulate the kill signal call, with all the variants
	   from the 4.3 BSD manual(negative pid: kill pg; zero pid:
	   kill sender's pg; pid -1: kill everybody except caller) */
	
	ses_getppid(*,
		out	int ppid)		rights PSR_READ;
	/* Return the parent pid */
	
	ses_getpgrp(*,
		in	int pid,
		out	int pgrp)		rights PSR_READ;
	/* Return the caller's process group */
	
	ses_setpgrp(*,
		in	int pid,
		in	int pgrp)		rights PSR_EXEC;
	/* Set the caller's process group */
	
	/* NEWPROC SYSTEM CALL */
	
	const MAXNCAPS = 20;
	
	/* Placeholder for binary backward compatibility; new version below. */
	ses_old_newchild(*,
		in	long sigignore,
		out	capability newcap,
		in	capability sharedcaps[nsharedcaps:MAXNCAPS],
		in	int nsharedcaps)	rights PSR_EXEC;
	/* Start a new process without forking.  This adds only
	   specified capabilities to the new child's 'shared' list. */
	
	/* PIPE SYSTEM CALL */
	
	ses_pipe(*,
		out	capability read_end,
		out	capability write_end)	rights PSR_EXEC /*?*/;
	/* Create a pipe */
	
	/* PS PROGRAM SUPPORT */
	
	const MAXNPROC = 300;
	
	struct proctab {
		short pid, ppid, pgrp;
		char state[10];
		capability proc;
	};
	
	ses_getpt(*,
		out	struct proctab pt[nproc:MAXNPROC],
		in out	int nproc)		rights PSR_READ;
	/* Get a list of running processes */
	
	/* SHARED OBJECT SUPPORT (see also ses_newchild above) */
	
	ses_share(*,
		in	capability sharedcaps[nsharedcaps:MAXNCAPS],
		in	int nsharedcaps)	rights PSR_EXEC;
	/* Adds the specified pipes' capabilities to the process's
	   'shared' list. */
	
	ses_unshare(*,
		in	capability sharedcaps[nsharedcaps:MAXNCAPS],
		in	int nsharedcaps)	rights PSR_EXEC;
	/* Removes the specified pipes' capabilities to the process's
	   'shared' list. */
	
	/* (BULLET) FILE SHARING */
	
	ses_file(*,
		in	long flags,
		in	long pos,
		in	long size,
		in	int mode,
		in	long mtime,
		var in	capability cap,
		var in	capability dircap,
		in	char name[NAME_MAX+1],
		out	capability cap_ret)	rights PSR_EXEC;
	/* Turn a bullet file into a stream that can be treated like a
	   tty or a pipe. */

	/* Shared file operations, that are not in generic FS interface: */

	/* ses_seek: does an lseek() on a shared file */
	ses_seek(*,
		var in	capability filecap,
		in	long offset,
		in	int  mode,
		out	long new_off)		rights PSR_EXEC /*?*/;

	/* ses_size: gives the size of a shared file (for fstat emulation) */
	ses_size(*,
		var in	capability filecap,
		out	long size)		rights PSR_EXEC /*?*/;

	/* ses_runsvr_pool: specify run server capability and pool directory
	 * to be used by sessin server in subsequent fork() calls issued by
	 * the process.
	 */
	ses_runsvr(*,
		var in	capability runsvr_cap,
		var in  capability pooldir_cap)	rights PSR_EXEC;

	/*
	 * New versions of the session call ses_preexec, ses_sigaware
	 * and ses_newchild with more parameters.  The original ones
	 * are retained for binary backward compatibility.
	 * Eventually the session server should stop implementing them.
	 */
	ses_preexec(*,
		in	long sigignore,
		in	long sigblock,
		in	long sigpending)	rights PSR_EXEC;

	ses_sigaware(*,
		var in	capability sigsvrcap,
		out	long sigignore,
		out	long sigblock,
		out	long sigpending)	rights PSR_EXEC;
	/* Program becomes signal-aware; sigsvrcap specifies the signal
	 * server thread cap, sigignore sigblock sigpending return
	 * the signal ignore mask, the set of blocked signals and the 
	 * set of pending signals repectively.
	 */

	ses_newchild(*,
                in      long sigignore,
		in      long sigblock,
		in	long sigpending,
		out	capability newcap,
		in	capability sharedcaps[nsharedcaps:MAXNCAPS],
		in	int nsharedcaps)	rights PSR_EXEC;

	ses_alarm(*,
		in out	int sec)		rights PSR_EXEC;
	/* Implements the Posix alarm() call */

	ses_getid(*,
		out     int uid,
		out     int euid,
		out     int gid,
		out     int egid)		rights PSR_READ;
	/* Returns the (real and effective) user and group id */

	ses_setout(*,
		var in	capability cons_tty)
		rights PSR_READ, PSR_WRITE, PSR_EXEC, PSR_KILL, PSR_CREATE,
		       PSR_DELETE, PSR_DUMMY1, PSR_DUMMY2;
	/* Sets the stdout used by the session svr */
};
