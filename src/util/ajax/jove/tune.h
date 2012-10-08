/*	@(#)tune.h	1.1	92/05/13 10:33:38 */
/***************************************************************************
 * This program is Copyright (C) 1986, 1987, 1988 by Jonathan Payne.  JOVE *
 * is provided to you without charge, and with no warranty.  You may give  *
 * away copies of JOVE, including sources, provided that this notice is    *
 * included in all the files.                                              *
 ***************************************************************************/

#define TUNED		/* don't touch this */

#include "sysdep.h"

#ifndef AMOEBA
#ifdef UNIX
# define SUBPROCS	/* only on UNIX systems (NOT INCORPORATED YET) */
# define IPROCS		/* interactive processes */
# define BSD4_3
# define JOB_CONTROL
#endif /* UNIX */
#endif

#define BACKUPFILES	/* enable the backup files code */
#define F_COMPLETION	/* filename completion */
#define ABBREV		/* word abbreviation mode */
#define CMT_FMT		/* format and indent C code */
#if !(defined(IBMPC) || defined(MAC))
# define ID_CHAR	/* include code to IDchar */
# define WIRED_TERMS	/* include code for wired terminals */
#endif
#define LISP		/* include the code for Lisp Mode */
#define CMT_FMT		/* include the comment formatting routines */
#ifndef AMOEBA
#define RESHAPING	/* catch SIGWINCH when window changes */
#endif

#ifdef AMOEBA
# define SPELL		/* spell words and buffer commands */
#else
#ifdef UNIX
# undef LOAD_AV	/* Use the load average for various commands.
#			   Do not define this if you lack a load average
#			   system call and kmem is read protected. */
# undef BIFF		/* if you have biff (or the equivalent) */
# define SPELL		/* spell words and buffer commands */
#endif
#endif

#define DFLT_MODE	0666	/* file will be created with this mode */

#ifdef MAC
# undef F_COMPLETION
# define F_COMPLETION 1
# define byte_zero(s,n) setmem((s),(n),0)
# define swritef sprintf
# define USE_PROTOTYPES	1
# define NBUF 64
# define JBUFSIZ 1024
# undef LISP
# define LISP 1
# undef ABBREV
# define ABBREV 1
# undef CMT_FMT
# define CMT_FMT 1
#endif

/* These are here since they define things in tune.c.  If you add things to
   tune.c, add them here too, if necessary. */

extern char
	*d_tempfile,
	*p_tempfile,
	*Recover,
	*Joverc,

#if defined(IPROCS) && defined(PIPEPROCS)
	*Portsrv,
	*Kbd_Proc,
#endif

#ifdef MSDOS
	CmdDb[],
#else
	*CmdDb,
#endif

	TmpFilePath[],
	Shell[],
	ShFlags[];
