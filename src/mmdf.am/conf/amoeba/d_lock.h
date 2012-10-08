/*	@(#)d_lock.h	1.1	91/11/20 16:49:44 */
/*
 * COMPILE-TIME CONFIGURATION OF THE PHONE CHANNEL LOCKING METHOD
 *
 * These definitions determine where lockfiles are placed, the format
 * of those files, and how a lock is verified.
 */

#define	UUCPLOCK	"/usr/spool/locks"	/* directory for locking
						compatible with tip/cu/
						uugetty/uucp */

#define	ASCIILOCKS		/* if you want your lockfiles to contain
				ascii, rather than binary, representations
				of pids. MUST match value in
				/usr/src/cmd/uucp/parms.h */

#define	SIZEOFPID	10	/* maximun number of digits in a pid.
				MUST match value in /usr/src/cmd/uucp/uucp.h */

#define	ATTSVKILL		/* if your system has kill(pid, 0) to
				check the existence of a process */
